//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLibraryPch.h"
#include "Types/PathTypeHandler.h"
#include "Types/SpreadArgument.h"

namespace Js
{
    // Make sure EmptySegment points to read-only memory.
    // Can't do this the easy way because SparseArraySegment has a constructor...
    static const char EmptySegmentData[sizeof(SparseArraySegmentBase)] = {0};
    const SparseArraySegmentBase *JavascriptArray::EmptySegment = (SparseArraySegmentBase *)&EmptySegmentData;

    // col0 : allocation bucket
    // col1 : No. of missing items to set during initialization depending on bucket.
    // col2 : allocation size for elements in given bucket.
    // col1 and col2 is calculated at runtime
    uint JavascriptNativeFloatArray::allocationBuckets[][AllocationBucketsInfoSize] =
    {
        { 3, 0, 0 },    // allocate space for 3 elements for array of length 0,1,2,3
        { 5, 0, 0 },    // allocate space for 5 elements for array of length 4,5
        { 8, 0, 0 },    // allocate space for 8 elements for array of length 6,7,8
    };
#if defined(_M_X64_OR_ARM64)
    const Var JavascriptArray::MissingItem = (Var)0x8000000280000002;
    uint JavascriptNativeIntArray::allocationBuckets[][AllocationBucketsInfoSize] =
    {
        // See comments above on how to read this
        {2, 0, 0},
        {6, 0, 0},
        {8, 0, 0},
    };
    uint JavascriptArray::allocationBuckets[][AllocationBucketsInfoSize] =
    {
        // See comments above on how to read this
        {4, 0, 0},
        {6, 0, 0},
        {8, 0, 0},
    };
#else
    const Var JavascriptArray::MissingItem = (Var)0x80000002;
    uint JavascriptNativeIntArray::allocationBuckets[][AllocationBucketsInfoSize] =
    {
        // See comments above on how to read this
        { 3, 0, 0 },
        { 7, 0, 0 },
        { 8, 0, 0 },
    };
    uint JavascriptArray::allocationBuckets[][AllocationBucketsInfoSize] =
    {
        // See comments above on how to read this
        { 4, 0, 0 },
        { 8, 0, 0 },
    };
#endif

    const int32 JavascriptNativeIntArray::MissingItem = 0x80000002;
    static const uint64 FloatMissingItemPattern = 0x8000000280000002ull;
    const double JavascriptNativeFloatArray::MissingItem = *(double*)&FloatMissingItemPattern;

    // Allocate enough space for 4 inline property slots and 16 inline element slots
    const size_t JavascriptArray::StackAllocationSize = DetermineAllocationSize<JavascriptArray, 4>(16);
    const size_t JavascriptNativeIntArray::StackAllocationSize = DetermineAllocationSize<JavascriptNativeIntArray, 4>(16);
    const size_t JavascriptNativeFloatArray::StackAllocationSize = DetermineAllocationSize<JavascriptNativeFloatArray, 4>(16);

    SegmentBTree::SegmentBTree()
        : segmentCount(0),
          segments(nullptr),
          keys(nullptr),
          children(nullptr)
    {LOGMEIN("JavascriptArray.cpp] 73\n");
    }

    uint32 SegmentBTree::GetLazyCrossOverLimit()
    {LOGMEIN("JavascriptArray.cpp] 77\n");
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.DisableArrayBTree)
        {LOGMEIN("JavascriptArray.cpp] 80\n");
            return Js::JavascriptArray::InvalidIndex;
        }
        else if (Js::Configuration::Global.flags.ForceArrayBTree)
        {LOGMEIN("JavascriptArray.cpp] 84\n");
            return ARRAY_CROSSOVER_FOR_VALIDATE;
        }
#endif
#ifdef VALIDATE_ARRAY
        if (Js::Configuration::Global.flags.ArrayValidate)
        {LOGMEIN("JavascriptArray.cpp] 90\n");
            return ARRAY_CROSSOVER_FOR_VALIDATE;
        }
#endif
        return SegmentBTree::MinDegree * 3;
    }

    BOOL SegmentBTree::IsLeaf() const
    {LOGMEIN("JavascriptArray.cpp] 98\n");
        return children == NULL;
    }
    BOOL SegmentBTree::IsFullNode() const
    {LOGMEIN("JavascriptArray.cpp] 102\n");
        return segmentCount == MaxKeys;
    }

    void SegmentBTree::InternalFind(SegmentBTree* node, uint32 itemIndex, SparseArraySegmentBase*& prev, SparseArraySegmentBase*& matchOrNext)
    {LOGMEIN("JavascriptArray.cpp] 107\n");
        uint32 i = 0;

        for(; i < node->segmentCount; i++)
        {LOGMEIN("JavascriptArray.cpp] 111\n");
            Assert(node->keys[i] == node->segments[i]->left);
            if (itemIndex <  node->keys[i])
            {LOGMEIN("JavascriptArray.cpp] 114\n");
                break;
            }
        }

        // i indicates the 1st segment in the node past any matching segment.
        // the i'th child is the children to the 'left' of the i'th segment.

        // If itemIndex matches segment i-1 (note that left is always a match even when length == 0)
        bool matches = i > 0 && (itemIndex == node->keys[i-1] || itemIndex < node->keys[i-1] + node->segments[i-1]->length);

        if (matches)
        {LOGMEIN("JavascriptArray.cpp] 126\n");
            // Find prev segment
            if (node->IsLeaf())
            {LOGMEIN("JavascriptArray.cpp] 129\n");
                if (i > 1)
                {LOGMEIN("JavascriptArray.cpp] 131\n");
                    // Previous is either sibling or set in a parent
                    prev = node->segments[i-2];
                }
            }
            else
            {
                // prev is the right most leaf in children[i-1] tree
                SegmentBTree* child = &node->children[i - 1];
                while (!child->IsLeaf())
                {LOGMEIN("JavascriptArray.cpp] 141\n");
                    child = &child->children[child->segmentCount];
                }
                prev = child->segments[child->segmentCount - 1];
            }

            // Return the matching segment
            matchOrNext = node->segments[i-1];
        }
        else // itemIndex in between segment i-1 and i
        {
            if (i > 0)
            {LOGMEIN("JavascriptArray.cpp] 153\n");
                // Store in previous in case a match or next is the first segment in a child.
                prev = node->segments[i-1];
            }

            if (node->IsLeaf())
            {LOGMEIN("JavascriptArray.cpp] 159\n");
                matchOrNext = (i == 0 ? node->segments[0] : PointerValue(prev->next));
            }
            else
            {
                InternalFind(node->children + i, itemIndex, prev, matchOrNext);
            }
        }
    }

    void SegmentBTreeRoot::Find(uint32 itemIndex, SparseArraySegmentBase*& prev, SparseArraySegmentBase*& matchOrNext)
    {LOGMEIN("JavascriptArray.cpp] 170\n");
        prev = matchOrNext = NULL;
        InternalFind(this, itemIndex, prev, matchOrNext);
        Assert(prev == NULL || (prev->next == matchOrNext));// If prev exists it is immediately before matchOrNext in the list of arraysegments
        Assert(prev == NULL || (prev->left < itemIndex && prev->left + prev->length <= itemIndex)); // prev should never be a match (left is a match if length == 0)
        Assert(matchOrNext == NULL || (matchOrNext->left >= itemIndex || matchOrNext->left + matchOrNext->length > itemIndex));
    }

    void SegmentBTreeRoot::Add(Recycler* recycler, SparseArraySegmentBase* newSeg)
    {LOGMEIN("JavascriptArray.cpp] 179\n");

        if (IsFullNode())
        {LOGMEIN("JavascriptArray.cpp] 182\n");
            SegmentBTree * children = AllocatorNewArrayZ(Recycler, recycler, SegmentBTree, MaxDegree);
            children[0] = *this;

            // Even though the segments point to a GC pointer, the main array should keep a references
            // as well.  So just make it a leaf allocation
            this->segmentCount = 0;
            this->segments = AllocatorNewArrayLeafZ(Recycler, recycler, SparseArraySegmentBase*, MaxKeys);
            this->keys = AllocatorNewArrayLeafZ(Recycler,recycler,uint32,MaxKeys);
            this->children = children;

            // This split is the only way the tree gets deeper
            SplitChild(recycler, this, 0, &children[0]);
        }
        InsertNonFullNode(recycler, this, newSeg);
    }

    void SegmentBTree::SwapSegment(uint32 originalKey, SparseArraySegmentBase* oldSeg, SparseArraySegmentBase* newSeg)
    {LOGMEIN("JavascriptArray.cpp] 200\n");
        // Find old segment
        uint32 itemIndex = originalKey;
        uint32 i = 0;

        for(; i < segmentCount; i++)
        {LOGMEIN("JavascriptArray.cpp] 206\n");
            Assert(keys[i] == segments[i]->left || (oldSeg == newSeg && newSeg == segments[i]));
            if (itemIndex <  keys[i])
            {LOGMEIN("JavascriptArray.cpp] 209\n");
                break;
            }
        }

        // i is 1 past any match

        if (i > 0)
        {LOGMEIN("JavascriptArray.cpp] 217\n");
            if (oldSeg == segments[i-1])
            {LOGMEIN("JavascriptArray.cpp] 219\n");
                segments[i-1] = newSeg;
                keys[i-1] = newSeg->left;
                return;
            }
        }

        Assert(!IsLeaf());
        children[i].SwapSegment(originalKey, oldSeg, newSeg);
    }


    void SegmentBTree::SplitChild(Recycler* recycler, SegmentBTree* parent, uint32 iChild, SegmentBTree* child)
    {LOGMEIN("JavascriptArray.cpp] 232\n");
        // Split child in two, move it's median key up to parent, and put the result of the split
        // on either side of the key moved up into parent

        Assert(child != NULL);
        Assert(parent != NULL);
        Assert(!parent->IsFullNode());
        Assert(child->IsFullNode());

        SegmentBTree newNode;
        newNode.segmentCount = MinKeys;

        // Even though the segments point to a GC pointer, the main array should keep a references
        // as well.  So just make it a leaf allocation
        newNode.segments = AllocatorNewArrayLeafZ(Recycler, recycler, SparseArraySegmentBase*, MaxKeys);
        newNode.keys = AllocatorNewArrayLeafZ(Recycler,recycler,uint32,MaxKeys);

        // Move the keys above the median into the new node
        for(uint32 i = 0; i < MinKeys; i++)
        {LOGMEIN("JavascriptArray.cpp] 251\n");
            newNode.segments[i] = child->segments[i+MinDegree];
            newNode.keys[i] = child->keys[i+MinDegree];

            // Do not leave false positive references around in the b-tree
            child->segments[i+MinDegree] = nullptr;
        }

        // If children exist move those as well.
        if (!child->IsLeaf())
        {LOGMEIN("JavascriptArray.cpp] 261\n");
            newNode.children = AllocatorNewArrayZ(Recycler, recycler, SegmentBTree, MaxDegree);
            for(uint32 j = 0; j < MinDegree; j++)
            {LOGMEIN("JavascriptArray.cpp] 264\n");
                newNode.children[j] = child->children[j+MinDegree];

                // Do not leave false positive references around in the b-tree
                child->children[j+MinDegree].segments = nullptr;
                child->children[j+MinDegree].children = nullptr;
            }
        }
        child->segmentCount = MinKeys;

        // Make room for the new child in parent
        for(uint32 j = parent->segmentCount; j > iChild; j--)
        {LOGMEIN("JavascriptArray.cpp] 276\n");
            parent->children[j+1] = parent->children[j];
        }
        // Copy the contents of the new node into the correct place in the parent's child array
        parent->children[iChild+1] = newNode;

        // Move the keys to make room for the median key
        for(uint32 k = parent->segmentCount; k > iChild; k--)
        {LOGMEIN("JavascriptArray.cpp] 284\n");
            parent->segments[k] = parent->segments[k-1];
            parent->keys[k] = parent->keys[k-1];
        }

        // Move the median key into the proper place in the parent node
        parent->segments[iChild] = child->segments[MinKeys];
        parent->keys[iChild] = child->keys[MinKeys];

        // Do not leave false positive references around in the b-tree
        child->segments[MinKeys] = nullptr;

        parent->segmentCount++;
    }

    void SegmentBTree::InsertNonFullNode(Recycler* recycler, SegmentBTree* node, SparseArraySegmentBase* newSeg)
    {LOGMEIN("JavascriptArray.cpp] 300\n");
        Assert(!node->IsFullNode());
        AnalysisAssert(node->segmentCount < MaxKeys);       // Same as !node->IsFullNode()
        Assert(newSeg != NULL);

        if (node->IsLeaf())
        {LOGMEIN("JavascriptArray.cpp] 306\n");
            // Move the keys
            uint32 i = node->segmentCount - 1;
            while( (i != -1) && (newSeg->left < node->keys[i]))
            {LOGMEIN("JavascriptArray.cpp] 310\n");
                node->segments[i+1] = node->segments[i];
                node->keys[i+1] = node->keys[i];
                i--;
            }
            if (!node->segments)
            {LOGMEIN("JavascriptArray.cpp] 316\n");
                // Even though the segments point to a GC pointer, the main array should keep a references
                // as well.  So just make it a leaf allocation
                node->segments = AllocatorNewArrayLeafZ(Recycler, recycler, SparseArraySegmentBase*, MaxKeys);
                node->keys = AllocatorNewArrayLeafZ(Recycler, recycler, uint32, MaxKeys);
            }
            node->segments[i + 1] = newSeg;
            node->keys[i + 1] = newSeg->left;
            node->segmentCount++;
        }
        else
        {
            // find the correct child node
            uint32 i = node->segmentCount-1;

            while((i != -1) && (newSeg->left < node->keys[i]))
            {LOGMEIN("JavascriptArray.cpp] 332\n");
                i--;
            }
            i++;

            // Make room if full
            if(node->children[i].IsFullNode())
            {
                // This split doesn't make the tree any deeper as node already has children.
                SplitChild(recycler, node, i, node->children+i);
                Assert(node->keys[i] == node->segments[i]->left);
                if (newSeg->left > node->keys[i])
                {LOGMEIN("JavascriptArray.cpp] 344\n");
                    i++;
                }
            }
            InsertNonFullNode(recycler, node->children+i, newSeg);
        }
    }

    inline void ThrowTypeErrorOnFailureHelper::ThrowTypeErrorOnFailure(BOOL operationSucceeded)
    {LOGMEIN("JavascriptArray.cpp] 353\n");
        if (IsThrowTypeError(operationSucceeded))
        {LOGMEIN("JavascriptArray.cpp] 355\n");
            ThrowTypeErrorOnFailure();
        }
    }

    inline void ThrowTypeErrorOnFailureHelper::ThrowTypeErrorOnFailure()
    {LOGMEIN("JavascriptArray.cpp] 361\n");
        JavascriptError::ThrowTypeError(m_scriptContext, VBSERR_ActionNotSupported, m_functionName);
    }

    inline BOOL ThrowTypeErrorOnFailureHelper::IsThrowTypeError(BOOL operationSucceeded)
    {LOGMEIN("JavascriptArray.cpp] 366\n");
        return !operationSucceeded;
    }

    // Make sure EmptySegment points to read-only memory.
    // Can't do this the easy way because SparseArraySegment has a constructor...
    JavascriptArray::JavascriptArray(DynamicType * type)
        : ArrayObject(type, false, 0)
    {LOGMEIN("JavascriptArray.cpp] 374\n");
        Assert(type->GetTypeId() == TypeIds_Array || type->GetTypeId() == TypeIds_NativeIntArray || type->GetTypeId() == TypeIds_NativeFloatArray || ((type->GetTypeId() == TypeIds_ES5Array || type->GetTypeId() == TypeIds_Object) && type->GetPrototype() == GetScriptContext()->GetLibrary()->GetArrayPrototype()));
        Assert(EmptySegment->length == 0 && EmptySegment->size == 0 && EmptySegment->next == NULL);
        InitArrayFlags(DynamicObjectFlags::InitialArrayValue);
        SetHeadAndLastUsedSegment(const_cast<SparseArraySegmentBase *>(EmptySegment));

    }

    JavascriptArray::JavascriptArray(uint32 length, DynamicType * type)
        : ArrayObject(type, false, length)
    {LOGMEIN("JavascriptArray.cpp] 384\n");
        Assert(JavascriptArray::Is(type->GetTypeId()));
        Assert(EmptySegment->length == 0 && EmptySegment->size == 0 && EmptySegment->next == NULL);
        InitArrayFlags(DynamicObjectFlags::InitialArrayValue);
        SetHeadAndLastUsedSegment(const_cast<SparseArraySegmentBase *>(EmptySegment));
    }

    JavascriptArray::JavascriptArray(uint32 length, uint32 size, DynamicType * type)
        : ArrayObject(type, false, length)
    {LOGMEIN("JavascriptArray.cpp] 393\n");
        Assert(type->GetTypeId() == TypeIds_Array);
        InitArrayFlags(DynamicObjectFlags::InitialArrayValue);
        Recycler* recycler = GetRecycler();
        SetHeadAndLastUsedSegment(SparseArraySegment<Var>::AllocateSegment(recycler, 0, 0, size, nullptr));
    }

    JavascriptArray::JavascriptArray(DynamicType * type, uint32 size)
        : ArrayObject(type, false)
    {LOGMEIN("JavascriptArray.cpp] 402\n");
        InitArrayFlags(DynamicObjectFlags::InitialArrayValue);
        SetHeadAndLastUsedSegment(DetermineInlineHeadSegmentPointer<JavascriptArray, 0, false>(this));
        head->size = size;
        Var fill = Js::JavascriptArray::MissingItem;
        for (uint i = 0; i < size; i++)
        {LOGMEIN("JavascriptArray.cpp] 408\n");
            SparseArraySegment<Var>::From(head)->elements[i] = fill;
        }
    }

    JavascriptNativeIntArray::JavascriptNativeIntArray(uint32 length, uint32 size, DynamicType * type)
        : JavascriptNativeArray(type)
    {LOGMEIN("JavascriptArray.cpp] 415\n");
        Assert(type->GetTypeId() == TypeIds_NativeIntArray);
        this->length = length;
        Recycler* recycler = GetRecycler();
        SetHeadAndLastUsedSegment(SparseArraySegment<int32>::AllocateSegment(recycler, 0, 0, size, nullptr));
    }

    JavascriptNativeIntArray::JavascriptNativeIntArray(DynamicType * type, uint32 size)
        : JavascriptNativeArray(type)
    {
        SetHeadAndLastUsedSegment(DetermineInlineHeadSegmentPointer<JavascriptNativeIntArray, 0, false>(this));
        head->size = size;
        SparseArraySegment<int32>::From(head)->FillSegmentBuffer(0, size);
    }

    JavascriptNativeFloatArray::JavascriptNativeFloatArray(uint32 length, uint32 size, DynamicType * type)
        : JavascriptNativeArray(type)
    {LOGMEIN("JavascriptArray.cpp] 432\n");
        Assert(type->GetTypeId() == TypeIds_NativeFloatArray);
        this->length = length;
        Recycler* recycler = GetRecycler();
        SetHeadAndLastUsedSegment(SparseArraySegment<double>::AllocateSegment(recycler, 0, 0, size, nullptr));
    }

    JavascriptNativeFloatArray::JavascriptNativeFloatArray(DynamicType * type, uint32 size)
        : JavascriptNativeArray(type)
    {
        SetHeadAndLastUsedSegment(DetermineInlineHeadSegmentPointer<JavascriptNativeFloatArray, 0, false>(this));
        head->size = size;
        SparseArraySegment<double>::From(head)->FillSegmentBuffer(0, size);
    }

    bool JavascriptArray::Is(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 448\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptArray::Is(typeId);
    }

    bool JavascriptArray::Is(TypeId typeId)
    {LOGMEIN("JavascriptArray.cpp] 454\n");
        return typeId >= TypeIds_ArrayFirst && typeId <= TypeIds_ArrayLast;
    }

    bool JavascriptArray::IsVarArray(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 459\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptArray::IsVarArray(typeId);
    }

    bool JavascriptArray::IsVarArray(TypeId typeId)
    {LOGMEIN("JavascriptArray.cpp] 465\n");
        return typeId == TypeIds_Array;
    }

    template<typename T>
    bool JavascriptArray::IsMissingItemAt(uint32 index) const
    {LOGMEIN("JavascriptArray.cpp] 471\n");
        SparseArraySegment<T>* headSeg = SparseArraySegment<T>::From(this->head);

        return SparseArraySegment<T>::IsMissingItem(&headSeg->elements[index]);
    }

    bool JavascriptArray::IsMissingItem(uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 478\n");
        bool isIntArray = false, isFloatArray = false;
        this->GetArrayTypeAndConvert(&isIntArray, &isFloatArray);

        if (isIntArray)
        {LOGMEIN("JavascriptArray.cpp] 483\n");
            return IsMissingItemAt<int32>(index);
        }
        else if (isFloatArray)
        {LOGMEIN("JavascriptArray.cpp] 487\n");
            return IsMissingItemAt<double>(index);
        }
        else
        {
            return IsMissingItemAt<Var>(index);
        }
    }

    JavascriptArray* JavascriptArray::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptArray'");

        return static_cast<JavascriptArray *>(RecyclableObject::FromVar(aValue));
    }

    // Get JavascriptArray* from a Var, which is either a JavascriptArray* or ESArray*.
    JavascriptArray* JavascriptArray::FromAnyArray(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 505\n");
        AssertMsg(Is(aValue) || ES5Array::Is(aValue), "Ensure var is actually a 'JavascriptArray' or 'ES5Array'");

        return static_cast<JavascriptArray *>(RecyclableObject::FromVar(aValue));
    }

    // Check if a Var is a direct-accessible (fast path) JavascriptArray.
    bool JavascriptArray::IsDirectAccessArray(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 513\n");
        return RecyclableObject::Is(aValue) &&
            (VirtualTableInfo<JavascriptArray>::HasVirtualTable(aValue) ||
                VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(aValue) ||
                VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(aValue));
    }

    DynamicObjectFlags JavascriptArray::GetFlags() const
    {LOGMEIN("JavascriptArray.cpp] 521\n");
        return GetArrayFlags();
    }

    DynamicObjectFlags JavascriptArray::GetFlags_Unchecked() const // do not use except in extreme circumstances
    {LOGMEIN("JavascriptArray.cpp] 526\n");
        return GetArrayFlags_Unchecked();
    }

    void JavascriptArray::SetFlags(const DynamicObjectFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 531\n");
        SetArrayFlags(flags);
    }

    DynamicType * JavascriptArray::GetInitialType(ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 536\n");
        return scriptContext->GetLibrary()->GetArrayType();
    }

    JavascriptArray *JavascriptArray::GetArrayForArrayOrObjectWithArray(const Var var)
    {LOGMEIN("JavascriptArray.cpp] 541\n");
        bool isObjectWithArray;
        TypeId arrayTypeId;
        return GetArrayForArrayOrObjectWithArray(var, &isObjectWithArray, &arrayTypeId);
    }

    JavascriptArray *JavascriptArray::GetArrayForArrayOrObjectWithArray(
        const Var var,
        bool *const isObjectWithArrayRef,
        TypeId *const arrayTypeIdRef)
    {LOGMEIN("JavascriptArray.cpp] 551\n");
        // This is a helper function used by jitted code. The array checks done here match the array checks done by jitted code
        // (see Lowerer::GenerateArrayTest) to minimize bailouts.

        Assert(var);
        Assert(isObjectWithArrayRef);
        Assert(arrayTypeIdRef);

        *isObjectWithArrayRef = false;
        *arrayTypeIdRef = TypeIds_Undefined;

        if(!RecyclableObject::Is(var))
        {LOGMEIN("JavascriptArray.cpp] 563\n");
            return nullptr;
        }

        JavascriptArray *array = nullptr;
        INT_PTR vtable = VirtualTableInfoBase::GetVirtualTable(var);
        if(vtable == VirtualTableInfo<DynamicObject>::Address)
        {LOGMEIN("JavascriptArray.cpp] 570\n");
            ArrayObject* objectArray = DynamicObject::FromVar(var)->GetObjectArray();
            array = (objectArray && Is(objectArray)) ? FromVar(objectArray) : nullptr;
            if(!array)
            {LOGMEIN("JavascriptArray.cpp] 574\n");
                return nullptr;
            }
            *isObjectWithArrayRef = true;
            vtable = VirtualTableInfoBase::GetVirtualTable(array);
        }

        if(vtable == VirtualTableInfo<JavascriptArray>::Address)
        {LOGMEIN("JavascriptArray.cpp] 582\n");
            *arrayTypeIdRef = TypeIds_Array;
        }
        else if(vtable == VirtualTableInfo<JavascriptNativeIntArray>::Address)
        {LOGMEIN("JavascriptArray.cpp] 586\n");
            *arrayTypeIdRef = TypeIds_NativeIntArray;
        }
        else if(vtable == VirtualTableInfo<JavascriptNativeFloatArray>::Address)
        {LOGMEIN("JavascriptArray.cpp] 590\n");
            *arrayTypeIdRef = TypeIds_NativeFloatArray;
        }
        else
        {
            return nullptr;
        }

        if(!array)
        {LOGMEIN("JavascriptArray.cpp] 599\n");
            array = FromVar(var);
        }
        return array;
    }

    const SparseArraySegmentBase *JavascriptArray::Jit_GetArrayHeadSegmentForArrayOrObjectWithArray(const Var var)
    {LOGMEIN("JavascriptArray.cpp] 606\n");
        // This is a helper function used by jitted code

        JavascriptArray *const array = GetArrayForArrayOrObjectWithArray(var);
        return array ? array->head : nullptr;
    }

    uint32 JavascriptArray::Jit_GetArrayHeadSegmentLength(const SparseArraySegmentBase *const headSegment)
    {LOGMEIN("JavascriptArray.cpp] 614\n");
        // This is a helper function used by jitted code

        return headSegment ? headSegment->length : 0;
    }

    bool JavascriptArray::Jit_OperationInvalidatedArrayHeadSegment(
        const SparseArraySegmentBase *const headSegmentBeforeOperation,
        const uint32 headSegmentLengthBeforeOperation,
        const Var varAfterOperation)
    {LOGMEIN("JavascriptArray.cpp] 624\n");
        // This is a helper function used by jitted code

        Assert(varAfterOperation);

        if(!headSegmentBeforeOperation)
        {LOGMEIN("JavascriptArray.cpp] 630\n");
            return false;
        }

        const SparseArraySegmentBase *const headSegmentAfterOperation =
            Jit_GetArrayHeadSegmentForArrayOrObjectWithArray(varAfterOperation);
        return
            headSegmentAfterOperation != headSegmentBeforeOperation ||
            headSegmentAfterOperation->length != headSegmentLengthBeforeOperation;
    }

    uint32 JavascriptArray::Jit_GetArrayLength(const Var var)
    {LOGMEIN("JavascriptArray.cpp] 642\n");
        // This is a helper function used by jitted code

        bool isObjectWithArray;
        TypeId arrayTypeId;
        JavascriptArray *const array = GetArrayForArrayOrObjectWithArray(var, &isObjectWithArray, &arrayTypeId);
        return array && !isObjectWithArray ? array->GetLength() : 0;
    }

    bool JavascriptArray::Jit_OperationInvalidatedArrayLength(const uint32 lengthBeforeOperation, const Var varAfterOperation)
    {LOGMEIN("JavascriptArray.cpp] 652\n");
        // This is a helper function used by jitted code

        return Jit_GetArrayLength(varAfterOperation) != lengthBeforeOperation;
    }

    DynamicObjectFlags JavascriptArray::Jit_GetArrayFlagsForArrayOrObjectWithArray(const Var var)
    {LOGMEIN("JavascriptArray.cpp] 659\n");
        // This is a helper function used by jitted code

        JavascriptArray *const array = GetArrayForArrayOrObjectWithArray(var);
        return array && array->UsesObjectArrayOrFlagsAsFlags() ? array->GetFlags() : DynamicObjectFlags::None;
    }

    bool JavascriptArray::Jit_OperationCreatedFirstMissingValue(
        const DynamicObjectFlags flagsBeforeOperation,
        const Var varAfterOperation)
    {LOGMEIN("JavascriptArray.cpp] 669\n");
        // This is a helper function used by jitted code

        Assert(varAfterOperation);

        return
            !!(flagsBeforeOperation & DynamicObjectFlags::HasNoMissingValues) &&
            !(Jit_GetArrayFlagsForArrayOrObjectWithArray(varAfterOperation) & DynamicObjectFlags::HasNoMissingValues);
    }

    bool JavascriptArray::HasNoMissingValues() const
    {LOGMEIN("JavascriptArray.cpp] 680\n");
        return !!(GetFlags() & DynamicObjectFlags::HasNoMissingValues);
    }

    bool JavascriptArray::HasNoMissingValues_Unchecked() const // do not use except in extreme circumstances
    {LOGMEIN("JavascriptArray.cpp] 685\n");
        return !!(GetFlags_Unchecked() & DynamicObjectFlags::HasNoMissingValues);
    }

    void JavascriptArray::SetHasNoMissingValues(const bool hasNoMissingValues)
    {LOGMEIN("JavascriptArray.cpp] 690\n");
        SetFlags(
            hasNoMissingValues
                ? GetFlags() | DynamicObjectFlags::HasNoMissingValues
                : GetFlags() & ~DynamicObjectFlags::HasNoMissingValues);
    }

    template<class T>
    bool JavascriptArray::IsMissingHeadSegmentItemImpl(const uint32 index) const
    {LOGMEIN("JavascriptArray.cpp] 699\n");
        Assert(index < head->length);

        return SparseArraySegment<T>::IsMissingItem(&SparseArraySegment<T>::From(head)->elements[index]);
    }

    bool JavascriptArray::IsMissingHeadSegmentItem(const uint32 index) const
    {LOGMEIN("JavascriptArray.cpp] 706\n");
        return IsMissingHeadSegmentItemImpl<Var>(index);
    }

#if ENABLE_COPYONACCESS_ARRAY
    void JavascriptCopyOnAccessNativeIntArray::ConvertCopyOnAccessSegment()
    {LOGMEIN("JavascriptArray.cpp] 712\n");
        Assert(this->GetScriptContext()->GetLibrary()->cacheForCopyOnAccessArraySegments->IsValidIndex(::Math::PointerCastToIntegral<uint32>(this->GetHead())));
        SparseArraySegment<int32> *seg = this->GetScriptContext()->GetLibrary()->cacheForCopyOnAccessArraySegments->GetSegmentByIndex(::Math::PointerCastToIntegral<byte>(this->GetHead()));
        SparseArraySegment<int32> *newSeg = SparseArraySegment<int32>::AllocateLiteralHeadSegment(this->GetRecycler(), seg->length);

#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::CopyOnAccessArrayPhase))
        {LOGMEIN("JavascriptArray.cpp] 719\n");
            Output::Print(_u("Convert copy-on-access array: index(%d) length(%d)\n"), this->GetHead(), seg->length);
            Output::Flush();
        }
#endif

        newSeg->CopySegment(this->GetRecycler(), newSeg, 0, seg, 0, seg->length);
        this->SetHeadAndLastUsedSegment(newSeg);

        VirtualTableInfo<JavascriptNativeIntArray>::SetVirtualTable(this);
        this->type = JavascriptNativeIntArray::GetInitialType(this->GetScriptContext());

        ArrayCallSiteInfo *arrayInfo = this->GetArrayCallSiteInfo();
        if (arrayInfo && !arrayInfo->isNotCopyOnAccessArray)
        {LOGMEIN("JavascriptArray.cpp] 733\n");
            arrayInfo->isNotCopyOnAccessArray = 1;
        }
    }

    uint32 JavascriptCopyOnAccessNativeIntArray::GetNextIndex(uint32 index) const
    {LOGMEIN("JavascriptArray.cpp] 739\n");
        if (this->length == 0 || (index != Js::JavascriptArray::InvalidIndex && index >= this->length))
        {LOGMEIN("JavascriptArray.cpp] 741\n");
            return Js::JavascriptArray::InvalidIndex;
        }
        else if (index == Js::JavascriptArray::InvalidIndex)
        {LOGMEIN("JavascriptArray.cpp] 745\n");
            return 0;
        }
        else
        {
            return index + 1;
        }
    }

    BOOL JavascriptCopyOnAccessNativeIntArray::DirectGetItemAt(uint32 index, int* outVal)
    {LOGMEIN("JavascriptArray.cpp] 755\n");
        Assert(this->GetScriptContext()->GetLibrary()->cacheForCopyOnAccessArraySegments->IsValidIndex(::Math::PointerCastToIntegral<uint32>(this->GetHead())));
        SparseArraySegment<int32> *seg = this->GetScriptContext()->GetLibrary()->cacheForCopyOnAccessArraySegments->GetSegmentByIndex(::Math::PointerCastToIntegral<byte>(this->GetHead()));

        if (this->length == 0 || index == Js::JavascriptArray::InvalidIndex || index >= this->length)
        {LOGMEIN("JavascriptArray.cpp] 760\n");
            return FALSE;
        }
        else
        {
            *outVal = seg->elements[index];
            return TRUE;
        }
    }
#endif

    bool JavascriptNativeIntArray::IsMissingHeadSegmentItem(const uint32 index) const
    {LOGMEIN("JavascriptArray.cpp] 772\n");
        return IsMissingHeadSegmentItemImpl<int32>(index);
    }

    bool JavascriptNativeFloatArray::IsMissingHeadSegmentItem(const uint32 index) const
    {LOGMEIN("JavascriptArray.cpp] 777\n");
        return IsMissingHeadSegmentItemImpl<double>(index);
    }

    template<typename T>
    void JavascriptArray::InternalFillFromPrototype(JavascriptArray *dstArray, const T& dstIndex, JavascriptArray *srcArray, uint32 start, uint32 end, uint32 count)
    {LOGMEIN("JavascriptArray.cpp] 783\n");
        RecyclableObject* prototype = srcArray->GetPrototype();
        while (start + count != end && JavascriptOperators::GetTypeId(prototype) != TypeIds_Null)
        {
            ForEachOwnMissingArrayIndexOfObject(srcArray, dstArray, prototype, start, end, dstIndex, [&](uint32 index, Var value) {
                T n = dstIndex + (index - start);
                dstArray->DirectSetItemAt(n, value);

                count++;
            });

            prototype = prototype->GetPrototype();
        }
    }

    template<>
    void JavascriptArray::InternalFillFromPrototype<uint32>(JavascriptArray *dstArray, const uint32& dstIndex, JavascriptArray *srcArray, uint32 start, uint32 end, uint32 count)
    {LOGMEIN("JavascriptArray.cpp] 800\n");
        RecyclableObject* prototype = srcArray->GetPrototype();
        while (start + count != end && JavascriptOperators::GetTypeId(prototype) != TypeIds_Null)
        {
            ForEachOwnMissingArrayIndexOfObject(srcArray, dstArray, prototype, start, end, dstIndex, [&](uint32 index, Var value) {
                uint32 n = dstIndex + (index - start);
                dstArray->SetItem(n, value, PropertyOperation_None);

                count++;
            });

            prototype = prototype->GetPrototype();
        }
    }

    /* static */
    bool JavascriptArray::HasInlineHeadSegment(uint32 length)
    {LOGMEIN("JavascriptArray.cpp] 817\n");
        return length <= SparseArraySegmentBase::INLINE_CHUNK_SIZE;
    }

    Var JavascriptArray::OP_NewScArray(uint32 elementCount, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 822\n");
        // Called only to create array literals: size is known.
        return scriptContext->GetLibrary()->CreateArrayLiteral(elementCount);
    }

    Var JavascriptArray::OP_NewScArrayWithElements(uint32 elementCount, Var *elements, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 828\n");
        // Called only to create array literals: size is known.
        JavascriptArray *arr = scriptContext->GetLibrary()->CreateArrayLiteral(elementCount);

        SparseArraySegment<Var> *head = SparseArraySegment<Var>::From(arr->head);
        Assert(elementCount <= head->length);
        CopyArray(head->elements, head->length, elements, elementCount);

#ifdef VALIDATE_ARRAY
        arr->ValidateArray();
#endif

        return arr;
    }

    Var JavascriptArray::OP_NewScArrayWithMissingValues(uint32 elementCount, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 844\n");
        // Called only to create array literals: size is known.
        JavascriptArray *const array = static_cast<JavascriptArray *>(OP_NewScArray(elementCount, scriptContext));
        array->SetHasNoMissingValues(false);
        SparseArraySegment<Var> *head = SparseArraySegment<Var>::From(array->head);
        head->FillSegmentBuffer(0, elementCount);

        return array;
    }

#if ENABLE_PROFILE_INFO
    Var JavascriptArray::ProfiledNewScArray(uint32 elementCount, ScriptContext *scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef)
    {LOGMEIN("JavascriptArray.cpp] 856\n");
        if (arrayInfo->IsNativeIntArray())
        {LOGMEIN("JavascriptArray.cpp] 858\n");
            JavascriptNativeIntArray *arr = scriptContext->GetLibrary()->CreateNativeIntArrayLiteral(elementCount);
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);
            return arr;
        }

        if (arrayInfo->IsNativeFloatArray())
        {LOGMEIN("JavascriptArray.cpp] 865\n");
            JavascriptNativeFloatArray *arr = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(elementCount);
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);
            return arr;
        }

        JavascriptArray *arr = scriptContext->GetLibrary()->CreateArrayLiteral(elementCount);
        return arr;
    }
#endif
    Var JavascriptArray::OP_NewScIntArray(AuxArray<int32> *ints, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 876\n");
        uint32 count = ints->count;
        JavascriptArray *arr = scriptContext->GetLibrary()->CreateArrayLiteral(count);
        SparseArraySegment<Var> *head = SparseArraySegment<Var>::From(arr->head);
        Assert(count > 0 && count == head->length);
        for (uint i = 0; i < count; i++)
        {LOGMEIN("JavascriptArray.cpp] 882\n");
            head->elements[i] = JavascriptNumber::ToVar(ints->elements[i], scriptContext);
        }
        return arr;
    }

#if ENABLE_PROFILE_INFO
    Var JavascriptArray::ProfiledNewScIntArray(AuxArray<int32> *ints, ScriptContext* scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef)
    {LOGMEIN("JavascriptArray.cpp] 890\n");
        // Called only to create array literals: size is known.
        uint32 count = ints->count;

        if (arrayInfo->IsNativeIntArray())
        {LOGMEIN("JavascriptArray.cpp] 895\n");
            JavascriptNativeIntArray *arr;

#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary *lib = scriptContext->GetLibrary();
            FunctionBody *functionBody = weakFuncRef->Get();

            if (JavascriptLibrary::IsCopyOnAccessArrayCallSite(lib, arrayInfo, count))
            {LOGMEIN("JavascriptArray.cpp] 903\n");
                Assert(lib->cacheForCopyOnAccessArraySegments);
                arr = scriptContext->GetLibrary()->CreateCopyOnAccessNativeIntArrayLiteral(arrayInfo, functionBody, ints);
            }
            else
#endif
            {
                arr = scriptContext->GetLibrary()->CreateNativeIntArrayLiteral(count);
                SparseArraySegment<int32> *head = SparseArraySegment<int32>::From(arr->head);
                Assert(count > 0 && count == head->length);
                CopyArray(head->elements, head->length, ints->elements, count);
            }

            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);

            return arr;
        }

        if (arrayInfo->IsNativeFloatArray())
        {LOGMEIN("JavascriptArray.cpp] 922\n");
            JavascriptNativeFloatArray *arr = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(count);
            SparseArraySegment<double> *head = SparseArraySegment<double>::From(arr->head);
            Assert(count > 0 && count == head->length);
            for (uint i = 0; i < count; i++)
            {LOGMEIN("JavascriptArray.cpp] 927\n");
                head->elements[i] = (double)ints->elements[i];
            }
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);

            return arr;
        }

        return OP_NewScIntArray(ints, scriptContext);
    }
#endif

    Var JavascriptArray::OP_NewScFltArray(AuxArray<double> *doubles, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 940\n");
        uint32 count = doubles->count;
        JavascriptArray *arr = scriptContext->GetLibrary()->CreateArrayLiteral(count);
        SparseArraySegment<Var> *head = SparseArraySegment<Var>::From(arr->head);
        Assert(count > 0 && count == head->length);
        for (uint i = 0; i < count; i++)
        {LOGMEIN("JavascriptArray.cpp] 946\n");
            double dval = doubles->elements[i];
            int32 ival;
            if (JavascriptNumber::TryGetInt32Value(dval, &ival) && !TaggedInt::IsOverflow(ival))
            {LOGMEIN("JavascriptArray.cpp] 950\n");
                head->elements[i] = TaggedInt::ToVarUnchecked(ival);
            }
            else
            {
                head->elements[i] = JavascriptNumber::ToVarNoCheck(dval, scriptContext);
            }
        }
        return arr;
    }

#if ENABLE_PROFILE_INFO
    Var JavascriptArray::ProfiledNewScFltArray(AuxArray<double> *doubles, ScriptContext* scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef)
    {LOGMEIN("JavascriptArray.cpp] 963\n");
        // Called only to create array literals: size is known.
        if (arrayInfo->IsNativeFloatArray())
        {LOGMEIN("JavascriptArray.cpp] 966\n");
            arrayInfo->SetIsNotNativeIntArray();
            uint32 count = doubles->count;
            JavascriptNativeFloatArray *arr = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(count);
            SparseArraySegment<double> *head = SparseArraySegment<double>::From(arr->head);
            Assert(count > 0 && count == head->length);
            CopyArray(head->elements, head->length, doubles->elements, count);
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);

            return arr;
        }

        return OP_NewScFltArray(doubles, scriptContext);
    }

    Var JavascriptArray::ProfiledNewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(JavascriptFunction::Is(function) &&
               JavascriptFunction::FromVar(function)->GetFunctionInfo() == &JavascriptArray::EntryInfo::NewInstance);
        Assert(callInfo.Count >= 2);

        ArrayCallSiteInfo *arrayInfo = (ArrayCallSiteInfo*)args[0];
        JavascriptArray* pNew = nullptr;

        if (callInfo.Count == 2)
        {LOGMEIN("JavascriptArray.cpp] 993\n");
            // Exactly one argument, which is the array length if it's a uint32.
            Var firstArgument = args[1];
            int elementCount;
            if (TaggedInt::Is(firstArgument))
            {LOGMEIN("JavascriptArray.cpp] 998\n");
                elementCount = TaggedInt::ToInt32(firstArgument);
                if (elementCount < 0)
                {LOGMEIN("JavascriptArray.cpp] 1001\n");
                    JavascriptError::ThrowRangeError(function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                if (arrayInfo && arrayInfo->IsNativeArray())
                {LOGMEIN("JavascriptArray.cpp] 1005\n");
                    if (arrayInfo->IsNativeIntArray())
                    {LOGMEIN("JavascriptArray.cpp] 1007\n");
                        pNew = function->GetLibrary()->CreateNativeIntArray(elementCount);
                    }
                    else
                    {
                        pNew = function->GetLibrary()->CreateNativeFloatArray(elementCount);
                    }
                }
                else
                {
                    pNew = function->GetLibrary()->CreateArray(elementCount);
                }
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(firstArgument))
            {LOGMEIN("JavascriptArray.cpp] 1021\n");
                // Non-tagged-int number: make sure the double value is really a uint32.
                double value = JavascriptNumber::GetValue(firstArgument);
                uint32 uvalue = JavascriptConversion::ToUInt32(value);
                if (value != uvalue)
                {LOGMEIN("JavascriptArray.cpp] 1026\n");
                    JavascriptError::ThrowRangeError(function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                if (arrayInfo && arrayInfo->IsNativeArray())
                {LOGMEIN("JavascriptArray.cpp] 1030\n");
                    if (arrayInfo->IsNativeIntArray())
                    {LOGMEIN("JavascriptArray.cpp] 1032\n");
                        pNew = function->GetLibrary()->CreateNativeIntArray(uvalue);
                    }
                    else
                    {
                        pNew = function->GetLibrary()->CreateNativeFloatArray(uvalue);
                    }
                }
                else
                {
                    pNew = function->GetLibrary()->CreateArray(uvalue);
                }
            }
            else
            {
                //
                // First element is not int/double
                // create an array of length 1.
                // Set first element as the passed Var
                //

                pNew = function->GetLibrary()->CreateArray(1);
                pNew->DirectSetItemAt<Var>(0, firstArgument);
            }
        }
        else
        {
            // Called with a list of initial element values.
            // Create an array of the appropriate length and walk the list.

            if (arrayInfo && arrayInfo->IsNativeArray())
            {LOGMEIN("JavascriptArray.cpp] 1063\n");
                if (arrayInfo->IsNativeIntArray())
                {LOGMEIN("JavascriptArray.cpp] 1065\n");
                    pNew = function->GetLibrary()->CreateNativeIntArray(callInfo.Count - 1);
                }
                else
                {
                    pNew = function->GetLibrary()->CreateNativeFloatArray(callInfo.Count - 1);
                }
            }
            else
            {
                pNew = function->GetLibrary()->CreateArray(callInfo.Count - 1);
            }
            pNew->FillFromArgs(callInfo.Count - 1, 0, args.Values, arrayInfo);
        }

#ifdef VALIDATE_ARRAY
        pNew->ValidateArray();
#endif
        return pNew;
    }
#endif

    Var JavascriptArray::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        return NewInstance(function, args);
    }

    Var JavascriptArray::NewInstance(RecyclableObject* function, Arguments args)
    {LOGMEIN("JavascriptArray.cpp] 1094\n");
        // Call to new Array(), possibly under another name.
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        // SkipDefaultNewObject function flag should have prevented the default object
        // being created, except when call true a host dispatch.
        const CallInfo &callInfo = args.Info;
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert( isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptArray* pNew = nullptr;

        if (callInfo.Count < 2)
        {LOGMEIN("JavascriptArray.cpp] 1110\n");
            // No arguments passed to Array(), so create with the default size (0).
            pNew = CreateArrayFromConstructorNoArg(function, scriptContext);

            return isCtorSuperCall ?
                JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pNew, nullptr, scriptContext) :
                pNew;
        }

        if (callInfo.Count == 2)
        {LOGMEIN("JavascriptArray.cpp] 1120\n");
            // Exactly one argument, which is the array length if it's a uint32.
            Var firstArgument = args[1];
            int elementCount;

            if (TaggedInt::Is(firstArgument))
            {LOGMEIN("JavascriptArray.cpp] 1126\n");
                elementCount = TaggedInt::ToInt32(firstArgument);
                if (elementCount < 0)
                {LOGMEIN("JavascriptArray.cpp] 1129\n");
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
                }

                pNew = CreateArrayFromConstructor(function, elementCount, scriptContext);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(firstArgument))
            {LOGMEIN("JavascriptArray.cpp] 1136\n");
                // Non-tagged-int number: make sure the double value is really a uint32.
                double value = JavascriptNumber::GetValue(firstArgument);
                uint32 uvalue = JavascriptConversion::ToUInt32(value);
                if (value != uvalue)
                {LOGMEIN("JavascriptArray.cpp] 1141\n");
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
                }

                pNew = CreateArrayFromConstructor(function, uvalue, scriptContext);
            }
            else
            {
                //
                // First element is not int/double
                // create an array of length 1.
                // Set first element as the passed Var
                //

                pNew = CreateArrayFromConstructor(function, 1, scriptContext);

                JavascriptOperators::SetItem(pNew, pNew, 0u, firstArgument, scriptContext, PropertyOperation_ThrowIfNotExtensible);

                // If we were passed an uninitialized JavascriptArray as the this argument,
                // we need to set the length. We must do this _after_ setting the first
                // element as the array may have side effects such as a setter for property
                // named '0' which would make the previous length of the array observable.
                // If we weren't passed a JavascriptArray as the this argument, this is no-op.
                pNew->SetLength(1);
            }
        }
        else
        {
            // Called with a list of initial element values.
            // Create an array of the appropriate length and walk the list.
            pNew = CreateArrayFromConstructor(function, callInfo.Count - 1, scriptContext);
            pNew->JavascriptArray::FillFromArgs(callInfo.Count - 1, 0, args.Values);
        }

#ifdef VALIDATE_ARRAY
        pNew->ValidateArray();
#endif
        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pNew, nullptr, scriptContext) :
            pNew;
    }

    JavascriptArray* JavascriptArray::CreateArrayFromConstructor(RecyclableObject* constructor, uint32 length, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 1184\n");
        JavascriptLibrary* library = constructor->GetLibrary();

        // Create the Array object we'll return - this is the only way to create an object which is an exotic Array object.
        // Note: We need to use the library from the ScriptContext of the constructor, not the currently executing function.
        //       This is for the case where a built-in @@create method from a different JavascriptLibrary is installed on
        //       constructor.
        return library->CreateArray(length);
    }

    JavascriptArray* JavascriptArray::CreateArrayFromConstructorNoArg(RecyclableObject* constructor, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 1195\n");
        JavascriptLibrary* library = constructor->GetLibrary();
        return library->CreateArray();
    }

#if ENABLE_PROFILE_INFO
    Var JavascriptArray::ProfiledNewInstanceNoArg(RecyclableObject *function, ScriptContext *scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef)
    {LOGMEIN("JavascriptArray.cpp] 1202\n");
        Assert(JavascriptFunction::Is(function) &&
               JavascriptFunction::FromVar(function)->GetFunctionInfo() == &JavascriptArray::EntryInfo::NewInstance);

        if (arrayInfo->IsNativeIntArray())
        {LOGMEIN("JavascriptArray.cpp] 1207\n");
            JavascriptNativeIntArray *arr = scriptContext->GetLibrary()->CreateNativeIntArray();
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);
            return arr;
        }

        if (arrayInfo->IsNativeFloatArray())
        {LOGMEIN("JavascriptArray.cpp] 1214\n");
            JavascriptNativeFloatArray *arr = scriptContext->GetLibrary()->CreateNativeFloatArray();
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);
            return arr;
        }

        return scriptContext->GetLibrary()->CreateArray();
    }
#endif

    Var JavascriptNativeIntArray::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        return NewInstance(function, args);
    }

    Var JavascriptNativeIntArray::NewInstance(RecyclableObject* function, Arguments args)
    {LOGMEIN("JavascriptArray.cpp] 1231\n");
        Assert(!PHASE_OFF1(NativeArrayPhase));

        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        const CallInfo &callInfo = args.Info;
        if (callInfo.Count < 2)
        {LOGMEIN("JavascriptArray.cpp] 1238\n");
            // No arguments passed to Array(), so create with the default size (0).
            return function->GetLibrary()->CreateNativeIntArray();
        }

        JavascriptArray* pNew = nullptr;
        if (callInfo.Count == 2)
        {LOGMEIN("JavascriptArray.cpp] 1245\n");
            // Exactly one argument, which is the array length if it's a uint32.
            Var firstArgument = args[1];
            int elementCount;
            if (TaggedInt::Is(firstArgument))
            {LOGMEIN("JavascriptArray.cpp] 1250\n");
                elementCount = TaggedInt::ToInt32(firstArgument);
                if (elementCount < 0)
                {LOGMEIN("JavascriptArray.cpp] 1253\n");
                    JavascriptError::ThrowRangeError(
                        function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                pNew = function->GetLibrary()->CreateNativeIntArray(elementCount);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(firstArgument))
            {LOGMEIN("JavascriptArray.cpp] 1260\n");
                // Non-tagged-int number: make sure the double value is really a uint32.
                double value = JavascriptNumber::GetValue(firstArgument);
                uint32 uvalue = JavascriptConversion::ToUInt32(value);
                if (value != uvalue)
                {LOGMEIN("JavascriptArray.cpp] 1265\n");
                    JavascriptError::ThrowRangeError(
                        function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                pNew = function->GetLibrary()->CreateNativeIntArray(uvalue);
            }
            else
            {
                //
                // First element is not int/double
                // create an array of length 1.
                // Set first element as the passed Var
                //

                pNew = function->GetLibrary()->CreateArray(1);
                pNew->DirectSetItemAt<Var>(0, firstArgument);
            }
        }
        else
        {
            // Called with a list of initial element values.
            // Create an array of the appropriate length and walk the list.

            JavascriptNativeIntArray *arr = function->GetLibrary()->CreateNativeIntArray(callInfo.Count - 1);
            pNew = arr->FillFromArgs(callInfo.Count - 1, 0, args.Values);
        }

#ifdef VALIDATE_ARRAY
        pNew->ValidateArray();
#endif

        return pNew;
    }

    Var JavascriptNativeFloatArray::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        return NewInstance(function, args);
    }

    Var JavascriptNativeFloatArray::NewInstance(RecyclableObject* function, Arguments args)
    {LOGMEIN("JavascriptArray.cpp] 1306\n");
        Assert(!PHASE_OFF1(NativeArrayPhase));

        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        const CallInfo &callInfo = args.Info;
        if (callInfo.Count < 2)
        {LOGMEIN("JavascriptArray.cpp] 1313\n");
            // No arguments passed to Array(), so create with the default size (0).
            return function->GetLibrary()->CreateNativeFloatArray();
        }

        JavascriptArray* pNew = nullptr;
        if (callInfo.Count == 2)
        {LOGMEIN("JavascriptArray.cpp] 1320\n");
            // Exactly one argument, which is the array length if it's a uint32.
            Var firstArgument = args[1];
            int elementCount;
            if (TaggedInt::Is(firstArgument))
            {LOGMEIN("JavascriptArray.cpp] 1325\n");
                elementCount = TaggedInt::ToInt32(firstArgument);
                if (elementCount < 0)
                {LOGMEIN("JavascriptArray.cpp] 1328\n");
                    JavascriptError::ThrowRangeError(
                        function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                pNew = function->GetLibrary()->CreateNativeFloatArray(elementCount);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(firstArgument))
            {LOGMEIN("JavascriptArray.cpp] 1335\n");
                // Non-tagged-int number: make sure the double value is really a uint32.
                double value = JavascriptNumber::GetValue(firstArgument);
                uint32 uvalue = JavascriptConversion::ToUInt32(value);
                if (value != uvalue)
                {LOGMEIN("JavascriptArray.cpp] 1340\n");
                    JavascriptError::ThrowRangeError(
                        function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                pNew = function->GetLibrary()->CreateNativeFloatArray(uvalue);
            }
            else
            {
                //
                // First element is not int/double
                // create an array of length 1.
                // Set first element as the passed Var
                //

                pNew = function->GetLibrary()->CreateArray(1);
                pNew->DirectSetItemAt<Var>(0, firstArgument);
            }
        }
        else
        {
            // Called with a list of initial element values.
            // Create an array of the appropriate length and walk the list.

            JavascriptNativeFloatArray *arr = function->GetLibrary()->CreateNativeFloatArray(callInfo.Count - 1);
            pNew = arr->FillFromArgs(callInfo.Count - 1, 0, args.Values);
        }

#ifdef VALIDATE_ARRAY
        pNew->ValidateArray();
#endif

        return pNew;
    }


#if ENABLE_PROFILE_INFO
    JavascriptArray * JavascriptNativeIntArray::FillFromArgs(uint length, uint start, Var *args, ArrayCallSiteInfo *arrayInfo, bool dontCreateNewArray)
#else
    JavascriptArray * JavascriptNativeIntArray::FillFromArgs(uint length, uint start, Var *args, bool dontCreateNewArray)
#endif
    {
        uint i;
        for (i = start; i < length; i++)
        {LOGMEIN("JavascriptArray.cpp] 1383\n");
            Var item = args[i + 1];

            bool isTaggedInt = TaggedInt::Is(item);
            bool isTaggedIntMissingValue = false;
#ifdef _M_AMD64
            if (isTaggedInt)
            {LOGMEIN("JavascriptArray.cpp] 1390\n");
                int32 iValue = TaggedInt::ToInt32(item);
                isTaggedIntMissingValue = Js::SparseArraySegment<int32>::IsMissingItem(&iValue);
            }
#endif
            if (isTaggedInt && !isTaggedIntMissingValue)
            {LOGMEIN("JavascriptArray.cpp] 1396\n");
                // This is taggedInt case and we verified that item is not missing value in AMD64.
                this->DirectSetItemAt(i, TaggedInt::ToInt32(item));
            }
            else if (!isTaggedIntMissingValue && JavascriptNumber::Is_NoTaggedIntCheck(item))
            {LOGMEIN("JavascriptArray.cpp] 1401\n");
                double dvalue = JavascriptNumber::GetValue(item);
                int32 ivalue;
                if (JavascriptNumber::TryGetInt32Value(dvalue, &ivalue) && !Js::SparseArraySegment<int32>::IsMissingItem(&ivalue))
                {LOGMEIN("JavascriptArray.cpp] 1405\n");
                    this->DirectSetItemAt(i, ivalue);
                }
                else
                {
#if ENABLE_PROFILE_INFO
                    if (arrayInfo)
                    {LOGMEIN("JavascriptArray.cpp] 1412\n");
                        arrayInfo->SetIsNotNativeIntArray();
                    }
#endif

                    if (HasInlineHeadSegment(length) && i < this->head->length && !dontCreateNewArray)
                    {LOGMEIN("JavascriptArray.cpp] 1418\n");
                        // Avoid shrinking the number of elements in the head segment. We can still create a new
                        // array here, so go ahead.
                        JavascriptNativeFloatArray *fArr =
                            this->GetScriptContext()->GetLibrary()->CreateNativeFloatArrayLiteral(length);
                        return fArr->JavascriptNativeFloatArray::FillFromArgs(length, 0, args);
                    }

                    JavascriptNativeFloatArray *fArr = JavascriptNativeIntArray::ToNativeFloatArray(this);
                    fArr->DirectSetItemAt(i, dvalue);
#if ENABLE_PROFILE_INFO
                    return fArr->JavascriptNativeFloatArray::FillFromArgs(length, i + 1, args, arrayInfo, dontCreateNewArray);
#else
                    return fArr->JavascriptNativeFloatArray::FillFromArgs(length, i + 1, args, dontCreateNewArray);
#endif
                }
            }
            else
            {
#if ENABLE_PROFILE_INFO
                if (arrayInfo)
                {LOGMEIN("JavascriptArray.cpp] 1439\n");
                    arrayInfo->SetIsNotNativeArray();
                }
#endif

                #pragma prefast(suppress:6237, "The right hand side condition does not have any side effects.")
                if (sizeof(int32) < sizeof(Var) && HasInlineHeadSegment(length) && i < this->head->length && !dontCreateNewArray)
                {LOGMEIN("JavascriptArray.cpp] 1446\n");
                    // Avoid shrinking the number of elements in the head segment. We can still create a new
                    // array here, so go ahead.
                    JavascriptArray *arr = this->GetScriptContext()->GetLibrary()->CreateArrayLiteral(length);
                    return arr->JavascriptArray::FillFromArgs(length, 0, args);
                }

                JavascriptArray *arr = JavascriptNativeIntArray::ToVarArray(this);
#if ENABLE_PROFILE_INFO
                return arr->JavascriptArray::FillFromArgs(length, i, args, nullptr, dontCreateNewArray);
#else
                return arr->JavascriptArray::FillFromArgs(length, i, args, dontCreateNewArray);
#endif
            }
        }

        return this;
    }

#if ENABLE_PROFILE_INFO
    JavascriptArray * JavascriptNativeFloatArray::FillFromArgs(uint length, uint start, Var *args, ArrayCallSiteInfo *arrayInfo, bool dontCreateNewArray)
#else
    JavascriptArray * JavascriptNativeFloatArray::FillFromArgs(uint length, uint start, Var *args, bool dontCreateNewArray)
#endif
    {
        uint i;
        for (i = start; i < length; i++)
        {LOGMEIN("JavascriptArray.cpp] 1473\n");
            Var item = args[i + 1];
            if (TaggedInt::Is(item))
            {LOGMEIN("JavascriptArray.cpp] 1476\n");
                this->DirectSetItemAt(i, TaggedInt::ToDouble(item));
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(item))
            {LOGMEIN("JavascriptArray.cpp] 1480\n");
                this->DirectSetItemAt(i, JavascriptNumber::GetValue(item));
            }
            else
            {
                JavascriptArray *arr = JavascriptNativeFloatArray::ToVarArray(this);
#if ENABLE_PROFILE_INFO
                if (arrayInfo)
                {LOGMEIN("JavascriptArray.cpp] 1488\n");
                    arrayInfo->SetIsNotNativeArray();
                }
                return arr->JavascriptArray::FillFromArgs(length, i, args, nullptr, dontCreateNewArray);
#else
                return arr->JavascriptArray::FillFromArgs(length, i, args, dontCreateNewArray);
#endif
            }
        }

        return this;
    }

#if ENABLE_PROFILE_INFO
    JavascriptArray * JavascriptArray::FillFromArgs(uint length, uint start, Var *args, ArrayCallSiteInfo *arrayInfo, bool dontCreateNewArray)
#else
    JavascriptArray * JavascriptArray::FillFromArgs(uint length, uint start, Var *args, bool dontCreateNewArray)
#endif
    {
        uint32 i;
        for (i = start; i < length; i++)
        {LOGMEIN("JavascriptArray.cpp] 1509\n");
            Var item = args[i + 1];
            this->DirectSetItemAt(i, item);
        }

        return this;
    }

    DynamicType * JavascriptNativeIntArray::GetInitialType(ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 1518\n");
        return scriptContext->GetLibrary()->GetNativeIntArrayType();
    }

#if ENABLE_COPYONACCESS_ARRAY
    DynamicType * JavascriptCopyOnAccessNativeIntArray::GetInitialType(ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 1524\n");
        return scriptContext->GetLibrary()->GetCopyOnAccessNativeIntArrayType();
    }
#endif

    JavascriptNativeFloatArray *JavascriptNativeIntArray::ToNativeFloatArray(JavascriptNativeIntArray *intArray)
    {LOGMEIN("JavascriptArray.cpp] 1530\n");
#if ENABLE_PROFILE_INFO
        ArrayCallSiteInfo *arrayInfo = intArray->GetArrayCallSiteInfo();
        if (arrayInfo)
        {LOGMEIN("JavascriptArray.cpp] 1534\n");
#if DBG
            Js::JavascriptStackWalker walker(intArray->GetScriptContext());
            Js::JavascriptFunction* caller = NULL;
            bool foundScriptCaller = false;
            while(walker.GetCaller(&caller))
            {LOGMEIN("JavascriptArray.cpp] 1540\n");
                if(caller != NULL && Js::ScriptFunction::Is(caller))
                {LOGMEIN("JavascriptArray.cpp] 1542\n");
                    foundScriptCaller = true;
                    break;
                }
            }

            if(foundScriptCaller)
            {LOGMEIN("JavascriptArray.cpp] 1549\n");
                Assert(caller);
                Assert(caller->GetFunctionBody());
                if(PHASE_TRACE(Js::NativeArrayConversionPhase, caller->GetFunctionBody()))
                {LOGMEIN("JavascriptArray.cpp] 1553\n");
                    Output::Print(_u("Conversion: Int array to Float array    ArrayCreationFunctionNumber:%2d    CallSiteNumber:%2d \n"), arrayInfo->functionNumber, arrayInfo->callSiteNumber);
                    Output::Flush();
                }
            }
            else
            {
                if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
                {LOGMEIN("JavascriptArray.cpp] 1561\n");
                    Output::Print(_u("Conversion: Int array to Float array across ScriptContexts"));
                    Output::Flush();
                }
            }
#else
            if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
            {LOGMEIN("JavascriptArray.cpp] 1568\n");
                Output::Print(_u("Conversion: Int array to Float array"));
                Output::Flush();
            }
#endif

            arrayInfo->SetIsNotNativeIntArray();
        }
#endif

        // Grow the segments

        ScriptContext *scriptContext = intArray->GetScriptContext();
        Recycler *recycler = scriptContext->GetRecycler();
        SparseArraySegmentBase *seg, *nextSeg, *prevSeg = nullptr;
        for (seg = intArray->head; seg; seg = nextSeg)
        {LOGMEIN("JavascriptArray.cpp] 1584\n");
            nextSeg = seg->next;
            uint32 size = seg->size;
            if (size == 0)
            {LOGMEIN("JavascriptArray.cpp] 1588\n");
                continue;
            }

            uint32 left = seg->left;
            uint32 length = seg->length;
            int i;
            int32 ival;

            // The old segment will have size/2 and length capped by the new size.
            seg->size >>= 1;
            if (seg == intArray->head || seg->length > (seg->size >>= 1))
            {LOGMEIN("JavascriptArray.cpp] 1600\n");
                // Some live elements are being pushed out of this segment, so allocate a new one.
                SparseArraySegment<double> *newSeg =
                    SparseArraySegment<double>::AllocateSegment(recycler, left, length, nextSeg);
                Assert(newSeg != nullptr);
                Assert((prevSeg == nullptr) == (seg == intArray->head));
                newSeg->next = nextSeg;
                intArray->LinkSegments((SparseArraySegment<double>*)prevSeg, newSeg);
                if (intArray->GetLastUsedSegment() == seg)
                {LOGMEIN("JavascriptArray.cpp] 1609\n");
                    intArray->SetLastUsedSegment(newSeg);
                }
                prevSeg = newSeg;
                SegmentBTree * segmentMap = intArray->GetSegmentMap();
                if (segmentMap)
                {LOGMEIN("JavascriptArray.cpp] 1615\n");
                    segmentMap->SwapSegment(left, seg, newSeg);
                }

                // Fill the new segment with the overflow.
                for (i = 0; (uint)i < newSeg->length; i++)
                {LOGMEIN("JavascriptArray.cpp] 1621\n");
                    ival = ((SparseArraySegment<int32>*)seg)->elements[i /*+ seg->length*/];
                    if (ival == JavascriptNativeIntArray::MissingItem)
                    {LOGMEIN("JavascriptArray.cpp] 1624\n");
                        continue;
                    }
                    newSeg->elements[i] = (double)ival;
                }
            }
            else
            {
                // Now convert the contents that will remain in the old segment.
                for (i = seg->length - 1; i >= 0; i--)
                {LOGMEIN("JavascriptArray.cpp] 1634\n");
                    ival = ((SparseArraySegment<int32>*)seg)->elements[i];
                    if (ival == JavascriptNativeIntArray::MissingItem)
                    {LOGMEIN("JavascriptArray.cpp] 1637\n");
                        ((SparseArraySegment<double>*)seg)->elements[i] = (double)JavascriptNativeFloatArray::MissingItem;
                    }
                    else
                    {
                        ((SparseArraySegment<double>*)seg)->elements[i] = (double)ival;
                    }
                }
                prevSeg = seg;
            }
        }

        if (intArray->GetType() == scriptContext->GetLibrary()->GetNativeIntArrayType())
        {LOGMEIN("JavascriptArray.cpp] 1650\n");
            intArray->type = scriptContext->GetLibrary()->GetNativeFloatArrayType();
        }
        else
        {
            if (intArray->GetDynamicType()->GetIsLocked())
            {LOGMEIN("JavascriptArray.cpp] 1656\n");
                DynamicTypeHandler *typeHandler = intArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {LOGMEIN("JavascriptArray.cpp] 1659\n");
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(intArray);
                }
                else
                {
                    intArray->ChangeType();
                }
            }
            intArray->GetType()->SetTypeId(TypeIds_NativeFloatArray);
        }

        if (CrossSite::IsCrossSiteObjectTyped(intArray))
        {LOGMEIN("JavascriptArray.cpp] 1675\n");
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptNativeIntArray>>::HasVirtualTable(intArray));
            VirtualTableInfo<CrossSiteObject<JavascriptNativeFloatArray>>::SetVirtualTable(intArray);
        }
        else
        {
            Assert(VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(intArray));
            VirtualTableInfo<JavascriptNativeFloatArray>::SetVirtualTable(intArray);
        }

        return (JavascriptNativeFloatArray*)intArray;
    }

    /*
    *   JavascriptArray::ChangeArrayTypeToNativeArray<double>
    *   -   Converts the Var Array's type to NativeFloat.
    *   -   Sets the VirtualTable to "JavascriptNativeFloatArray"
    */
    template<>
    void JavascriptArray::ChangeArrayTypeToNativeArray<double>(JavascriptArray * varArray, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 1695\n");
        AssertMsg(!JavascriptNativeArray::Is(varArray), "Ensure that the incoming Array is a Var array");

        if (varArray->GetType() == scriptContext->GetLibrary()->GetArrayType())
        {LOGMEIN("JavascriptArray.cpp] 1699\n");
            varArray->type = scriptContext->GetLibrary()->GetNativeFloatArrayType();
        }
        else
        {
            if (varArray->GetDynamicType()->GetIsLocked())
            {LOGMEIN("JavascriptArray.cpp] 1705\n");
                DynamicTypeHandler *typeHandler = varArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {LOGMEIN("JavascriptArray.cpp] 1708\n");
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(varArray);
                }
                else
                {
                    varArray->ChangeType();
                }
            }
            varArray->GetType()->SetTypeId(TypeIds_NativeFloatArray);
        }

        if (CrossSite::IsCrossSiteObjectTyped(varArray))
        {LOGMEIN("JavascriptArray.cpp] 1724\n");
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptArray>>::HasVirtualTable(varArray));
            VirtualTableInfo<CrossSiteObject<JavascriptNativeFloatArray>>::SetVirtualTable(varArray);
        }
        else
        {
            Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(varArray));
            VirtualTableInfo<JavascriptNativeFloatArray>::SetVirtualTable(varArray);
        }
    }

    /*
    *   JavascriptArray::ChangeArrayTypeToNativeArray<int32>
    *   -   Converts the Var Array's type to NativeInt.
    *   -   Sets the VirtualTable to "JavascriptNativeIntArray"
    */
    template<>
    void JavascriptArray::ChangeArrayTypeToNativeArray<int32>(JavascriptArray * varArray, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 1742\n");
        AssertMsg(!JavascriptNativeArray::Is(varArray), "Ensure that the incoming Array is a Var array");

        if (varArray->GetType() == scriptContext->GetLibrary()->GetArrayType())
        {LOGMEIN("JavascriptArray.cpp] 1746\n");
            varArray->type = scriptContext->GetLibrary()->GetNativeIntArrayType();
        }
        else
        {
            if (varArray->GetDynamicType()->GetIsLocked())
            {LOGMEIN("JavascriptArray.cpp] 1752\n");
                DynamicTypeHandler *typeHandler = varArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {LOGMEIN("JavascriptArray.cpp] 1755\n");
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(varArray);
                }
                else
                {
                    varArray->ChangeType();
                }
            }
            varArray->GetType()->SetTypeId(TypeIds_NativeIntArray);
        }

        if (CrossSite::IsCrossSiteObjectTyped(varArray))
        {LOGMEIN("JavascriptArray.cpp] 1771\n");
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptArray>>::HasVirtualTable(varArray));
            VirtualTableInfo<CrossSiteObject<JavascriptNativeIntArray>>::SetVirtualTable(varArray);
        }
        else
        {
            Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(varArray));
            VirtualTableInfo<JavascriptNativeIntArray>::SetVirtualTable(varArray);
        }
    }

    template<>
    int32 JavascriptArray::GetNativeValue<int32>(Js::Var ival, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 1784\n");
        return JavascriptConversion::ToInt32(ival, scriptContext);
    }

    template <>
    double JavascriptArray::GetNativeValue<double>(Var ival, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 1790\n");
        return JavascriptConversion::ToNumber(ival, scriptContext);
    }


    /*
    *   JavascriptArray::ConvertToNativeArrayInPlace
    *   In place conversion of all Var elements to Native Int/Double elements in an array.
    *   We do not update the DynamicProfileInfo of the array here.
    */
    template<typename NativeArrayType, typename T>
    NativeArrayType *JavascriptArray::ConvertToNativeArrayInPlace(JavascriptArray *varArray)
    {LOGMEIN("JavascriptArray.cpp] 1802\n");
        AssertMsg(!JavascriptNativeArray::Is(varArray), "Ensure that the incoming Array is a Var array");

        ScriptContext *scriptContext = varArray->GetScriptContext();
        SparseArraySegmentBase *seg, *nextSeg, *prevSeg = nullptr;
        for (seg = varArray->head; seg; seg = nextSeg)
        {LOGMEIN("JavascriptArray.cpp] 1808\n");
            nextSeg = seg->next;
            uint32 size = seg->size;
            if (size == 0)
            {LOGMEIN("JavascriptArray.cpp] 1812\n");
                continue;
            }

            int i;
            Var ival;

            uint32 growFactor = sizeof(Var) / sizeof(T);
            AssertMsg(growFactor == 1, "We support only in place conversion of Var array to Native Array");

            // Now convert the contents that will remain in the old segment.
            for (i = seg->length - 1; i >= 0; i--)
            {LOGMEIN("JavascriptArray.cpp] 1824\n");
                ival = ((SparseArraySegment<Var>*)seg)->elements[i];
                if (ival == JavascriptArray::MissingItem)
                {LOGMEIN("JavascriptArray.cpp] 1827\n");
                    ((SparseArraySegment<T>*)seg)->elements[i] = NativeArrayType::MissingItem;
                }
                else
                {
                    ((SparseArraySegment<T>*)seg)->elements[i] = GetNativeValue<T>(ival, scriptContext);
                }
            }
            prevSeg = seg;
        }

        // Update the type of the Array
        ChangeArrayTypeToNativeArray<T>(varArray, scriptContext);

        return (NativeArrayType*)varArray;
    }

    JavascriptArray *JavascriptNativeIntArray::ConvertToVarArray(JavascriptNativeIntArray *intArray)
    {LOGMEIN("JavascriptArray.cpp] 1845\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(intArray);
#endif
        ScriptContext *scriptContext = intArray->GetScriptContext();
        Recycler *recycler = scriptContext->GetRecycler();
        SparseArraySegmentBase *seg, *nextSeg, *prevSeg = nullptr;
        for (seg = intArray->head; seg; seg = nextSeg)
        {LOGMEIN("JavascriptArray.cpp] 1853\n");
            nextSeg = seg->next;
            uint32 size = seg->size;
            if (size == 0)
            {LOGMEIN("JavascriptArray.cpp] 1857\n");
                continue;
            }

            uint32 left = seg->left;
            uint32 length = seg->length;
            int i;
            int32 ival;

            // Shrink?
            uint32 growFactor = sizeof(Var) / sizeof(int32);
            if ((growFactor != 1 && (seg == intArray->head || seg->length > (seg->size /= growFactor))) ||
                (seg->next == nullptr && SparseArraySegmentBase::IsLeafSegment(seg, recycler)))
            {LOGMEIN("JavascriptArray.cpp] 1870\n");
                // Some live elements are being pushed out of this segment, so allocate a new one.
                // And/or the old segment is not scanned by the recycler, so we need a new one to hold vars.
                SparseArraySegment<Var> *newSeg =
                    SparseArraySegment<Var>::AllocateSegment(recycler, left, length, nextSeg);

                AnalysisAssert(newSeg);
                Assert((prevSeg == nullptr) == (seg == intArray->head));
                newSeg->next = nextSeg;
                intArray->LinkSegments((SparseArraySegment<Var>*)prevSeg, newSeg);
                if (intArray->GetLastUsedSegment() == seg)
                {LOGMEIN("JavascriptArray.cpp] 1881\n");
                    intArray->SetLastUsedSegment(newSeg);
                }
                prevSeg = newSeg;

                SegmentBTree * segmentMap = intArray->GetSegmentMap();
                if (segmentMap)
                {LOGMEIN("JavascriptArray.cpp] 1888\n");
                    segmentMap->SwapSegment(left, seg, newSeg);
                }

                // Fill the new segment with the overflow.
                for (i = 0; (uint)i < newSeg->length; i++)
                {LOGMEIN("JavascriptArray.cpp] 1894\n");
                    ival = ((SparseArraySegment<int32>*)seg)->elements[i];
                    if (ival == JavascriptNativeIntArray::MissingItem)
                    {LOGMEIN("JavascriptArray.cpp] 1897\n");
                        continue;
                    }
                    newSeg->elements[i] = JavascriptNumber::ToVar(ival, scriptContext);
                }
            }
            else
            {
                // Now convert the contents that will remain in the old segment.
                // Walk backward in case we're growing the element size.
                for (i = seg->length - 1; i >= 0; i--)
                {LOGMEIN("JavascriptArray.cpp] 1908\n");
                    ival = ((SparseArraySegment<int32>*)seg)->elements[i];
                    if (ival == JavascriptNativeIntArray::MissingItem)
                    {LOGMEIN("JavascriptArray.cpp] 1911\n");
                        ((SparseArraySegment<Var>*)seg)->elements[i] = (Var)JavascriptArray::MissingItem;
                    }
                    else
                    {
                        ((SparseArraySegment<Var>*)seg)->elements[i] = JavascriptNumber::ToVar(ival, scriptContext);
                    }
                }
                prevSeg = seg;
            }
        }

        if (intArray->GetType() == scriptContext->GetLibrary()->GetNativeIntArrayType())
        {LOGMEIN("JavascriptArray.cpp] 1924\n");
            intArray->type = scriptContext->GetLibrary()->GetArrayType();
        }
        else
        {
            if (intArray->GetDynamicType()->GetIsLocked())
            {LOGMEIN("JavascriptArray.cpp] 1930\n");
                DynamicTypeHandler *typeHandler = intArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {LOGMEIN("JavascriptArray.cpp] 1933\n");
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(intArray);
                }
                else
                {
                    intArray->ChangeType();
                }
            }
            intArray->GetType()->SetTypeId(TypeIds_Array);
        }

        if (CrossSite::IsCrossSiteObjectTyped(intArray))
        {LOGMEIN("JavascriptArray.cpp] 1949\n");
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptNativeIntArray>>::HasVirtualTable(intArray));
            VirtualTableInfo<CrossSiteObject<JavascriptArray>>::SetVirtualTable(intArray);
        }
        else
        {
            Assert(VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(intArray));
            VirtualTableInfo<JavascriptArray>::SetVirtualTable(intArray);
        }

        return intArray;
    }
    JavascriptArray *JavascriptNativeIntArray::ToVarArray(JavascriptNativeIntArray *intArray)
    {LOGMEIN("JavascriptArray.cpp] 1962\n");
#if ENABLE_PROFILE_INFO
        ArrayCallSiteInfo *arrayInfo = intArray->GetArrayCallSiteInfo();
        if (arrayInfo)
        {LOGMEIN("JavascriptArray.cpp] 1966\n");
#if DBG
            Js::JavascriptStackWalker walker(intArray->GetScriptContext());
            Js::JavascriptFunction* caller = NULL;
            bool foundScriptCaller = false;
            while(walker.GetCaller(&caller))
            {LOGMEIN("JavascriptArray.cpp] 1972\n");
                if(caller != NULL && Js::ScriptFunction::Is(caller))
                {LOGMEIN("JavascriptArray.cpp] 1974\n");
                    foundScriptCaller = true;
                    break;
                }
            }

            if(foundScriptCaller)
            {LOGMEIN("JavascriptArray.cpp] 1981\n");
                Assert(caller);
                Assert(caller->GetFunctionBody());
                if(PHASE_TRACE(Js::NativeArrayConversionPhase, caller->GetFunctionBody()))
                {LOGMEIN("JavascriptArray.cpp] 1985\n");
                    Output::Print(_u("Conversion: Int array to Var array    ArrayCreationFunctionNumber:%2d    CallSiteNumber:%2d \n"), arrayInfo->functionNumber, arrayInfo->callSiteNumber);
                    Output::Flush();
                }
            }
            else
            {
                if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
                {LOGMEIN("JavascriptArray.cpp] 1993\n");
                    Output::Print(_u("Conversion: Int array to Var array across ScriptContexts"));
                    Output::Flush();
                }
            }
#else
            if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
            {LOGMEIN("JavascriptArray.cpp] 2000\n");
                Output::Print(_u("Conversion: Int array to Var array"));
                Output::Flush();
            }
#endif

            arrayInfo->SetIsNotNativeArray();
        }
#endif

        intArray->ClearArrayCallSiteIndex();

        return ConvertToVarArray(intArray);
    }

    DynamicType * JavascriptNativeFloatArray::GetInitialType(ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 2016\n");
        return scriptContext->GetLibrary()->GetNativeFloatArrayType();
    }

    /*
    *   JavascriptNativeFloatArray::ConvertToVarArray
    *   This function only converts all Float elements to Var elements in an array.
    *   DynamicProfileInfo of the array is not updated in this function.
    */
    JavascriptArray *JavascriptNativeFloatArray::ConvertToVarArray(JavascriptNativeFloatArray *fArray)
    {LOGMEIN("JavascriptArray.cpp] 2026\n");
        // We can't be growing the size of the element.
        Assert(sizeof(double) >= sizeof(Var));

        uint32 shrinkFactor = sizeof(double) / sizeof(Var);
        ScriptContext *scriptContext = fArray->GetScriptContext();
        Recycler *recycler = scriptContext->GetRecycler();
        SparseArraySegmentBase *seg, *nextSeg, *prevSeg = nullptr;
        for (seg = fArray->head; seg; seg = nextSeg)
        {LOGMEIN("JavascriptArray.cpp] 2035\n");
            nextSeg = seg->next;
            if (seg->size == 0)
            {LOGMEIN("JavascriptArray.cpp] 2038\n");
                continue;
            }
            uint32 left = seg->left;
            uint32 length = seg->length;
            SparseArraySegment<Var> *newSeg;
            if (seg->next == nullptr && SparseArraySegmentBase::IsLeafSegment(seg, recycler))
            {LOGMEIN("JavascriptArray.cpp] 2045\n");
                // The old segment is not scanned by the recycler, so we need a new one to hold vars.
                newSeg =
                    SparseArraySegment<Var>::AllocateSegment(recycler, left, length, nextSeg);
                Assert((prevSeg == nullptr) == (seg == fArray->head));
                newSeg->next = nextSeg;
                fArray->LinkSegments((SparseArraySegment<Var>*)prevSeg, newSeg);
                if (fArray->GetLastUsedSegment() == seg)
                {LOGMEIN("JavascriptArray.cpp] 2053\n");
                    fArray->SetLastUsedSegment(newSeg);
                }
                prevSeg = newSeg;

                SegmentBTree * segmentMap = fArray->GetSegmentMap();
                if (segmentMap)
                {LOGMEIN("JavascriptArray.cpp] 2060\n");
                    segmentMap->SwapSegment(left, seg, newSeg);
                }
            }
            else
            {
                newSeg = (SparseArraySegment<Var>*)seg;
                prevSeg = seg;
                if (shrinkFactor != 1)
                {LOGMEIN("JavascriptArray.cpp] 2069\n");
                    uint32 newSize = seg->size * shrinkFactor;
                    uint32 limit;
                    if (seg->next)
                    {LOGMEIN("JavascriptArray.cpp] 2073\n");
                        limit = seg->next->left;
                    }
                    else
                    {
                        limit = JavascriptArray::MaxArrayLength;
                    }
                    seg->size = min(newSize, limit - seg->left);
                }
            }
            uint32 i;
            for (i = 0; i < seg->length; i++)
            {LOGMEIN("JavascriptArray.cpp] 2085\n");
                if (SparseArraySegment<double>::IsMissingItem(&((SparseArraySegment<double>*)seg)->elements[i]))
                {LOGMEIN("JavascriptArray.cpp] 2087\n");
                    if (seg == newSeg)
                    {LOGMEIN("JavascriptArray.cpp] 2089\n");
                        newSeg->elements[i] = (Var)JavascriptArray::MissingItem;
                    }
                    Assert(newSeg->elements[i] == (Var)JavascriptArray::MissingItem);
                }
                else if (*(uint64*)&(((SparseArraySegment<double>*)seg)->elements[i]) == 0ull)
                {LOGMEIN("JavascriptArray.cpp] 2095\n");
                    newSeg->elements[i] = TaggedInt::ToVarUnchecked(0);
                }
                else
                {
                    int32 ival;
                    double dval = ((SparseArraySegment<double>*)seg)->elements[i];
                    if (JavascriptNumber::TryGetInt32Value(dval, &ival) && !TaggedInt::IsOverflow(ival))
                    {LOGMEIN("JavascriptArray.cpp] 2103\n");
                        newSeg->elements[i] = TaggedInt::ToVarUnchecked(ival);
                    }
                    else
                    {
                        newSeg->elements[i] = JavascriptNumber::ToVarWithCheck(dval, scriptContext);
                    }
                }
            }
            if (seg == newSeg && shrinkFactor != 1)
            {LOGMEIN("JavascriptArray.cpp] 2113\n");
                // Fill the remaining slots.
                newSeg->FillSegmentBuffer(i, seg->size);
            }
        }

        if (fArray->GetType() == scriptContext->GetLibrary()->GetNativeFloatArrayType())
        {LOGMEIN("JavascriptArray.cpp] 2120\n");
            fArray->type = scriptContext->GetLibrary()->GetArrayType();
        }
        else
        {
            if (fArray->GetDynamicType()->GetIsLocked())
            {LOGMEIN("JavascriptArray.cpp] 2126\n");
                DynamicTypeHandler *typeHandler = fArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {LOGMEIN("JavascriptArray.cpp] 2129\n");
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(fArray);
                }
                else
                {
                    fArray->ChangeType();
                }
            }
            fArray->GetType()->SetTypeId(TypeIds_Array);
        }

        if (CrossSite::IsCrossSiteObjectTyped(fArray))
        {LOGMEIN("JavascriptArray.cpp] 2145\n");
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptNativeFloatArray>>::HasVirtualTable(fArray));
            VirtualTableInfo<CrossSiteObject<JavascriptArray>>::SetVirtualTable(fArray);
        }
        else
        {
            Assert(VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(fArray));
            VirtualTableInfo<JavascriptArray>::SetVirtualTable(fArray);
        }

        return fArray;
    }

    JavascriptArray *JavascriptNativeFloatArray::ToVarArray(JavascriptNativeFloatArray *fArray)
    {LOGMEIN("JavascriptArray.cpp] 2159\n");
#if ENABLE_PROFILE_INFO
        ArrayCallSiteInfo *arrayInfo = fArray->GetArrayCallSiteInfo();
        if (arrayInfo)
        {LOGMEIN("JavascriptArray.cpp] 2163\n");
#if DBG
            Js::JavascriptStackWalker walker(fArray->GetScriptContext());
            Js::JavascriptFunction* caller = NULL;
            bool foundScriptCaller = false;
            while(walker.GetCaller(&caller))
            {LOGMEIN("JavascriptArray.cpp] 2169\n");
                if(caller != NULL && Js::ScriptFunction::Is(caller))
                {LOGMEIN("JavascriptArray.cpp] 2171\n");
                    foundScriptCaller = true;
                    break;
                }
            }

            if(foundScriptCaller)
            {LOGMEIN("JavascriptArray.cpp] 2178\n");
                Assert(caller);
                Assert(caller->GetFunctionBody());
                if(PHASE_TRACE(Js::NativeArrayConversionPhase, caller->GetFunctionBody()))
                {LOGMEIN("JavascriptArray.cpp] 2182\n");
                    Output::Print(_u("Conversion: Float array to Var array    ArrayCreationFunctionNumber:%2d    CallSiteNumber:%2d \n"), arrayInfo->functionNumber, arrayInfo->callSiteNumber);
                    Output::Flush();
                }
            }
            else
            {
                if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
                {LOGMEIN("JavascriptArray.cpp] 2190\n");
                    Output::Print(_u("Conversion: Float array to Var array across ScriptContexts"));
                    Output::Flush();
                }
            }
#else
            if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
            {LOGMEIN("JavascriptArray.cpp] 2197\n");
                Output::Print(_u("Conversion: Float array to Var array"));
                Output::Flush();
            }
#endif

            if(fArray->GetScriptContext()->IsScriptContextInNonDebugMode())
            {LOGMEIN("JavascriptArray.cpp] 2204\n");
                Assert(!arrayInfo->IsNativeIntArray());
            }

            arrayInfo->SetIsNotNativeArray();
        }
#endif

        fArray->ClearArrayCallSiteIndex();

        return ConvertToVarArray(fArray);

    }

    // Convert Var to index in the Array.
    // Note: Spec calls out a few rules for these parameters:
    // 1. if (arg > length) { return length; }
    // clamp to length, not length-1
    // 2. if (arg < 0) { return max(0, length + arg); }
    // treat negative arg as index from the end of the array (with -1 mapping to length-1)
    // Effectively, this function will return a value between 0 and length, inclusive.
    int64 JavascriptArray::GetIndexFromVar(Js::Var arg, int64 length, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 2226\n");
        int64 index;

        if (TaggedInt::Is(arg))
        {LOGMEIN("JavascriptArray.cpp] 2230\n");
            int intValue = TaggedInt::ToInt32(arg);

            if (intValue < 0)
            {LOGMEIN("JavascriptArray.cpp] 2234\n");
                index = max<int64>(0, length + intValue);
            }
            else
            {
                index = intValue;
            }

            if (index > length)
            {LOGMEIN("JavascriptArray.cpp] 2243\n");
                index = length;
            }
        }
        else
        {
            double doubleValue = JavascriptConversion::ToInteger(arg, scriptContext);

            // Handle the Number.POSITIVE_INFINITY case
            if (doubleValue > length)
            {LOGMEIN("JavascriptArray.cpp] 2253\n");
                return length;
            }

            index = NumberUtilities::TryToInt64(doubleValue);

            if (index < 0)
            {LOGMEIN("JavascriptArray.cpp] 2260\n");
                index = max<int64>(0, index + length);
            }
        }

        return index;
    }

    TypeId JavascriptArray::OP_SetNativeIntElementC(JavascriptNativeIntArray *arr, uint32 index, Var value, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 2269\n");
        int32 iValue;
        double dValue;

        TypeId typeId = arr->TrySetNativeIntArrayItem(value, &iValue, &dValue);
        if (typeId == TypeIds_NativeIntArray)
        {LOGMEIN("JavascriptArray.cpp] 2275\n");
            arr->SetArrayLiteralItem(index, iValue);
        }
        else if (typeId == TypeIds_NativeFloatArray)
        {LOGMEIN("JavascriptArray.cpp] 2279\n");
            arr->SetArrayLiteralItem(index, dValue);
        }
        else
        {
            arr->SetArrayLiteralItem(index, value);
        }
        return typeId;
    }

    TypeId JavascriptArray::OP_SetNativeFloatElementC(JavascriptNativeFloatArray *arr, uint32 index, Var value, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 2290\n");
        double dValue;
        TypeId typeId = arr->TrySetNativeFloatArrayItem(value, &dValue);
        if (typeId == TypeIds_NativeFloatArray)
        {LOGMEIN("JavascriptArray.cpp] 2294\n");
            arr->SetArrayLiteralItem(index, dValue);
        }
        else
        {
            arr->SetArrayLiteralItem(index, value);
        }
        return typeId;
    }

    template<typename T>
    void JavascriptArray::SetArrayLiteralItem(uint32 index, T value)
    {LOGMEIN("JavascriptArray.cpp] 2306\n");
        SparseArraySegment<T> * segment = SparseArraySegment<T>::From(this->head);

        Assert(segment->left == 0);
        Assert(index < segment->length);

        segment->elements[index] = value;
    }

    void JavascriptNativeIntArray::SetIsPrototype()
    {LOGMEIN("JavascriptArray.cpp] 2316\n");
        // Force the array to be non-native to simplify inspection, filling from proto, etc.
        ToVarArray(this);
        __super::SetIsPrototype();
    }

    void JavascriptNativeFloatArray::SetIsPrototype()
    {LOGMEIN("JavascriptArray.cpp] 2323\n");
        // Force the array to be non-native to simplify inspection, filling from proto, etc.
        ToVarArray(this);
        __super::SetIsPrototype();
    }

#if ENABLE_PROFILE_INFO
    ArrayCallSiteInfo *JavascriptNativeArray::GetArrayCallSiteInfo()
    {LOGMEIN("JavascriptArray.cpp] 2331\n");
        RecyclerWeakReference<FunctionBody> *weakRef = this->weakRefToFuncBody;
        if (weakRef)
        {LOGMEIN("JavascriptArray.cpp] 2334\n");
            FunctionBody *functionBody = weakRef->Get();
            if (functionBody)
            {LOGMEIN("JavascriptArray.cpp] 2337\n");
                if (functionBody->HasDynamicProfileInfo())
                {LOGMEIN("JavascriptArray.cpp] 2339\n");
                    Js::ProfileId profileId = this->GetArrayCallSiteIndex();
                    if (profileId < functionBody->GetProfiledArrayCallSiteCount())
                    {LOGMEIN("JavascriptArray.cpp] 2342\n");
                        return functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);
                    }
                }
            }
            else
            {
                this->ClearArrayCallSiteIndex();
            }
        }
        return nullptr;
    }

    void JavascriptNativeArray::SetArrayProfileInfo(RecyclerWeakReference<FunctionBody> *weakRef, ArrayCallSiteInfo *arrayInfo)
    {LOGMEIN("JavascriptArray.cpp] 2356\n");
        Assert(weakRef);
        FunctionBody *functionBody = weakRef->Get();
        if (functionBody && functionBody->HasDynamicProfileInfo())
        {LOGMEIN("JavascriptArray.cpp] 2360\n");
            ArrayCallSiteInfo *baseInfo = functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, 0);
            Js::ProfileId index = (Js::ProfileId)(arrayInfo - baseInfo);
            Assert(index < functionBody->GetProfiledArrayCallSiteCount());
            SetArrayCallSite(index, weakRef);
        }
    }

    void JavascriptNativeArray::CopyArrayProfileInfo(Js::JavascriptNativeArray* baseArray)
    {LOGMEIN("JavascriptArray.cpp] 2369\n");
        if (baseArray->weakRefToFuncBody)
        {LOGMEIN("JavascriptArray.cpp] 2371\n");
            if (baseArray->weakRefToFuncBody->Get())
            {LOGMEIN("JavascriptArray.cpp] 2373\n");
                SetArrayCallSite(baseArray->GetArrayCallSiteIndex(), baseArray->weakRefToFuncBody);
            }
            else
            {
                baseArray->ClearArrayCallSiteIndex();
            }
        }
    }
#endif

    Var JavascriptNativeArray::FindMinOrMax(Js::ScriptContext * scriptContext, bool findMax)
    {LOGMEIN("JavascriptArray.cpp] 2385\n");
        if (JavascriptNativeIntArray::Is(this))
        {LOGMEIN("JavascriptArray.cpp] 2387\n");
            return this->FindMinOrMax<int32, false>(scriptContext, findMax);
        }
        else
        {
            return this->FindMinOrMax<double, true>(scriptContext, findMax);
        }
    }

    template <typename T, bool checkNaNAndNegZero>
    Var JavascriptNativeArray::FindMinOrMax(Js::ScriptContext * scriptContext, bool findMax)
    {LOGMEIN("JavascriptArray.cpp] 2398\n");
        AssertMsg(this->HasNoMissingValues(), "Fastpath is only for arrays with one segment and no missing values");
        uint len = this->GetLength();

        Js::SparseArraySegment<T>* headSegment = ((Js::SparseArraySegment<T>*)this->GetHead());
        uint headSegLen = headSegment->length;
        Assert(headSegLen == len);

        if (headSegment->next == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 2407\n");
            T currentRes = headSegment->elements[0];
            for (uint i = 0; i < headSegLen; i++)
            {LOGMEIN("JavascriptArray.cpp] 2410\n");
                T compare = headSegment->elements[i];
                if (checkNaNAndNegZero && JavascriptNumber::IsNan(double(compare)))
                {LOGMEIN("JavascriptArray.cpp] 2413\n");
                    return scriptContext->GetLibrary()->GetNaN();
                }
                if (findMax ? currentRes < compare : currentRes > compare ||
                    (checkNaNAndNegZero && compare == 0 && Js::JavascriptNumber::IsNegZero(double(currentRes))))
                {LOGMEIN("JavascriptArray.cpp] 2418\n");
                    currentRes = compare;
                }
            }
            return Js::JavascriptNumber::ToVarNoCheck(currentRes, scriptContext);
        }
        else
        {
            AssertMsg(false, "FindMinOrMax currently supports native arrays with only one segment");
            Throw::FatalInternalError();
        }
    }

    SparseArraySegmentBase * JavascriptArray::GetLastUsedSegment() const
    {LOGMEIN("JavascriptArray.cpp] 2432\n");
        return HasSegmentMap() ?
            PointerValue(segmentUnion.segmentBTreeRoot->lastUsedSegment) :
            PointerValue(segmentUnion.lastUsedSegment);
    }

    void JavascriptArray::SetHeadAndLastUsedSegment(SparseArraySegmentBase * segment)
    {LOGMEIN("JavascriptArray.cpp] 2439\n");

        Assert(!HasSegmentMap());
        this->head = this->segmentUnion.lastUsedSegment = segment;
    }

    void JavascriptArray::SetLastUsedSegment(SparseArraySegmentBase * segment)
    {LOGMEIN("JavascriptArray.cpp] 2446\n");
        if (HasSegmentMap())
        {LOGMEIN("JavascriptArray.cpp] 2448\n");
            this->segmentUnion.segmentBTreeRoot->lastUsedSegment = segment;
        }
        else
        {
            this->segmentUnion.lastUsedSegment = segment;
        }
    }
    bool JavascriptArray::HasSegmentMap() const
    {LOGMEIN("JavascriptArray.cpp] 2457\n");
        return !!(GetFlags() & DynamicObjectFlags::HasSegmentMap);
    }

    SegmentBTreeRoot * JavascriptArray::GetSegmentMap() const
    {LOGMEIN("JavascriptArray.cpp] 2462\n");
        return (HasSegmentMap() ? segmentUnion.segmentBTreeRoot : nullptr);
    }

    void JavascriptArray::SetSegmentMap(SegmentBTreeRoot * segmentMap)
    {LOGMEIN("JavascriptArray.cpp] 2467\n");
        Assert(!HasSegmentMap());
        SparseArraySegmentBase * lastUsedSeg = this->segmentUnion.lastUsedSegment;
        SetFlags(GetFlags() | DynamicObjectFlags::HasSegmentMap);
        segmentUnion.segmentBTreeRoot = segmentMap;
        segmentMap->lastUsedSegment = lastUsedSeg;
    }

    void JavascriptArray::ClearSegmentMap()
    {LOGMEIN("JavascriptArray.cpp] 2476\n");
        if (HasSegmentMap())
        {LOGMEIN("JavascriptArray.cpp] 2478\n");
            SetFlags(GetFlags() & ~DynamicObjectFlags::HasSegmentMap);
            SparseArraySegmentBase * lastUsedSeg = segmentUnion.segmentBTreeRoot->lastUsedSegment;
            segmentUnion.segmentBTreeRoot = nullptr;
            segmentUnion.lastUsedSegment = lastUsedSeg;
        }
    }

    SegmentBTreeRoot * JavascriptArray::BuildSegmentMap()
    {LOGMEIN("JavascriptArray.cpp] 2487\n");
        Recycler* recycler = GetRecycler();
        SegmentBTreeRoot* tmpSegmentMap = AllocatorNewStruct(Recycler, recycler, SegmentBTreeRoot);
        ForEachSegment([recycler, tmpSegmentMap](SparseArraySegmentBase * current)
        {
            tmpSegmentMap->Add(recycler, current);
            return false;
        });

        // There could be OOM during building segment map. Save to array only after its successful completion.
        SetSegmentMap(tmpSegmentMap);
        return tmpSegmentMap;
    }

    void JavascriptArray::TryAddToSegmentMap(Recycler* recycler, SparseArraySegmentBase* seg)
    {LOGMEIN("JavascriptArray.cpp] 2502\n");
        SegmentBTreeRoot * savedSegmentMap = GetSegmentMap();
        if (savedSegmentMap)
        {LOGMEIN("JavascriptArray.cpp] 2505\n");
            //
            // We could OOM and throw when adding to segmentMap, resulting in a corrupted segmentMap on this
            // array. Set segmentMap to null temporarily to protect from this. It will be restored correctly
            // if adding segment succeeds.
            //
            ClearSegmentMap();
            savedSegmentMap->Add(recycler, seg);
            SetSegmentMap(savedSegmentMap);
        }
    }

    void JavascriptArray::InvalidateLastUsedSegment()
    {LOGMEIN("JavascriptArray.cpp] 2518\n");
        this->SetLastUsedSegment(this->head);
    }

    DescriptorFlags JavascriptArray::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 2523\n");
        DescriptorFlags flags;
        if (GetSetterBuiltIns(propertyId, info, &flags))
        {LOGMEIN("JavascriptArray.cpp] 2526\n");
            return flags;
        }
        return __super::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags JavascriptArray::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 2533\n");
        DescriptorFlags flags;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetSetterBuiltIns(propertyRecord->GetPropertyId(), info, &flags))
        {LOGMEIN("JavascriptArray.cpp] 2539\n");
            return flags;
        }

        return __super::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool JavascriptArray::GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* descriptorFlags)
    {LOGMEIN("JavascriptArray.cpp] 2547\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 2549\n");
            PropertyValueInfo::SetNoCache(info, this);
            *descriptorFlags = WritableData;
            return true;
        }
        return false;
    }

    SparseArraySegmentBase * JavascriptArray::GetBeginLookupSegment(uint32 index, const bool useSegmentMap) const
    {LOGMEIN("JavascriptArray.cpp] 2558\n");
        SparseArraySegmentBase *seg = nullptr;
        SparseArraySegmentBase * lastUsedSeg = this->GetLastUsedSegment();
        if (lastUsedSeg != nullptr && lastUsedSeg->left <= index)
        {LOGMEIN("JavascriptArray.cpp] 2562\n");
            seg = lastUsedSeg;
            if(index - lastUsedSeg->left < lastUsedSeg->size)
            {LOGMEIN("JavascriptArray.cpp] 2565\n");
                return seg;
            }
        }

        SegmentBTreeRoot * segmentMap = GetSegmentMap();
        if(!useSegmentMap || !segmentMap)
        {LOGMEIN("JavascriptArray.cpp] 2572\n");
            return seg ? seg : PointerValue(this->head);
        }

        if(seg)
        {LOGMEIN("JavascriptArray.cpp] 2577\n");
            // If indexes are being accessed sequentially, check the segment after the last-used segment before checking the
            // segment map, as it is likely to hit
            SparseArraySegmentBase *const nextSeg = seg->next;
            if(nextSeg)
            {LOGMEIN("JavascriptArray.cpp] 2582\n");
                if(index < nextSeg->left)
                {LOGMEIN("JavascriptArray.cpp] 2584\n");
                    return seg;
                }
                else if(index - nextSeg->left < nextSeg->size)
                {LOGMEIN("JavascriptArray.cpp] 2588\n");
                    return nextSeg;
                }
            }
        }

        SparseArraySegmentBase *matchOrNextSeg;
        segmentMap->Find(index, seg, matchOrNextSeg);
        return seg ? seg : matchOrNextSeg;
    }

    uint32 JavascriptArray::GetNextIndex(uint32 index) const
    {LOGMEIN("JavascriptArray.cpp] 2600\n");
        if (JavascriptNativeIntArray::Is((Var)this))
        {LOGMEIN("JavascriptArray.cpp] 2602\n");
            return this->GetNextIndexHelper<int32>(index);
        }
        else if (JavascriptNativeFloatArray::Is((Var)this))
        {LOGMEIN("JavascriptArray.cpp] 2606\n");
            return this->GetNextIndexHelper<double>(index);
        }
        return this->GetNextIndexHelper<Var>(index);
    }

    template<typename T>
    uint32 JavascriptArray::GetNextIndexHelper(uint32 index) const
    {LOGMEIN("JavascriptArray.cpp] 2614\n");
        AssertMsg(this->head, "array head should never be null");
        uint candidateIndex;

        if (index == JavascriptArray::InvalidIndex)
        {LOGMEIN("JavascriptArray.cpp] 2619\n");
            candidateIndex = head->left;
        }
        else
        {
            candidateIndex = index + 1;
        }

        SparseArraySegment<T>* current = (SparseArraySegment<T>*)this->GetBeginLookupSegment(candidateIndex);

        while (current != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 2630\n");
            if ((current->left <= candidateIndex) && ((candidateIndex - current->left) < current->length))
            {LOGMEIN("JavascriptArray.cpp] 2632\n");
                for (uint i = candidateIndex - current->left; i < current->length; i++)
                {LOGMEIN("JavascriptArray.cpp] 2634\n");
                    if (!SparseArraySegment<T>::IsMissingItem(&current->elements[i]))
                    {LOGMEIN("JavascriptArray.cpp] 2636\n");
                        return i + current->left;
                    }
                }
            }
            current = SparseArraySegment<T>::From(current->next);
            if (current != NULL)
            {LOGMEIN("JavascriptArray.cpp] 2643\n");
                if (candidateIndex < current->left)
                {LOGMEIN("JavascriptArray.cpp] 2645\n");
                    candidateIndex = current->left;
                }
            }
        }
        return JavascriptArray::InvalidIndex;
    }

    // If new length > length, we just reset the length
    // If new length < length, we need to remove the rest of the elements and segment
    void JavascriptArray::SetLength(uint32 newLength)
    {LOGMEIN("JavascriptArray.cpp] 2656\n");
        if (newLength == length)
            return;

        if (head == EmptySegment)
        {LOGMEIN("JavascriptArray.cpp] 2661\n");
            // Do nothing to the segment.
        }
        else if (newLength == 0)
        {LOGMEIN("JavascriptArray.cpp] 2665\n");
            this->ClearElements(head, 0);
            head->length = 0;
            head->next = nullptr;
            SetHasNoMissingValues();

            ClearSegmentMap();
            this->InvalidateLastUsedSegment();
        }
        else if (newLength < length)
        {LOGMEIN("JavascriptArray.cpp] 2675\n");
            // _ _ 2 3 _ _ 6 7 _ _

            // SetLength(0)
            // 0 <= left -> set *prev = null
            // SetLength(2)
            // 2 <= left -> set *prev = null
            // SetLength(3)
            // 3 !<= left; 3 <= right -> truncate to length - 1
            // SetLength(5)
            // 5 <=

            SparseArraySegmentBase* next = GetBeginLookupSegment(newLength - 1); // head, or next.left < newLength
            Field(SparseArraySegmentBase*)* prev = &head;

            while(next != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 2691\n");
                if (newLength <= next->left)
                {LOGMEIN("JavascriptArray.cpp] 2693\n");
                    ClearSegmentMap(); // truncate segments, null out segmentMap
                    *prev = nullptr;
                    break;
                }
                else if (newLength <= (next->left + next->length))
                {LOGMEIN("JavascriptArray.cpp] 2699\n");
                    if (next->next)
                    {LOGMEIN("JavascriptArray.cpp] 2701\n");
                        ClearSegmentMap(); // Will truncate segments, null out segmentMap
                    }

                    uint32 newSegmentLength = newLength - next->left;
                    this->ClearElements(next, newSegmentLength);
                    next->next = nullptr;
                    next->length = newSegmentLength;
                    break;
                }
                else
                {
                    prev = &next->next;
                    next = next->next;
                }
            }
            this->InvalidateLastUsedSegment();
        }
        this->length = newLength;

#ifdef VALIDATE_ARRAY
        ValidateArray();
#endif
    }

    BOOL JavascriptArray::SetLength(Var newLength)
    {LOGMEIN("JavascriptArray.cpp] 2727\n");
        ScriptContext *scriptContext;
        if(TaggedInt::Is(newLength))
        {LOGMEIN("JavascriptArray.cpp] 2730\n");
            int32 lenValue = TaggedInt::ToInt32(newLength);
            if (lenValue < 0)
            {LOGMEIN("JavascriptArray.cpp] 2733\n");
                scriptContext = GetScriptContext();
                if (scriptContext->GetThreadContext()->RecordImplicitException())
                {LOGMEIN("JavascriptArray.cpp] 2736\n");
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
                }
            }
            else
            {
                this->SetLength(lenValue);
            }
            return TRUE;
        }

        scriptContext = GetScriptContext();
        uint32 uintValue = JavascriptConversion::ToUInt32(newLength, scriptContext);
        double dblValue = JavascriptConversion::ToNumber(newLength, scriptContext);
        if (dblValue == uintValue)
        {LOGMEIN("JavascriptArray.cpp] 2751\n");
            this->SetLength(uintValue);
        }
        else
        {
            ThreadContext* threadContext = scriptContext->GetThreadContext();
            ImplicitCallFlags flags = threadContext->GetImplicitCallFlags();
            if (flags != ImplicitCall_None && threadContext->IsDisableImplicitCall())
            {LOGMEIN("JavascriptArray.cpp] 2759\n");
                // We couldn't execute the implicit call(s) needed to convert the newLength to an integer.
                // Do nothing and let the jitted code bail out.
                return TRUE;
            }

            if (threadContext->RecordImplicitException())
            {LOGMEIN("JavascriptArray.cpp] 2766\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
            }
        }

        return TRUE;
    }

    void JavascriptArray::ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength)
    {LOGMEIN("JavascriptArray.cpp] 2775\n");
        SparseArraySegment<Var>::ClearElements(((SparseArraySegment<Var>*)seg)->elements + newSegmentLength, seg->length - newSegmentLength);
    }

    void JavascriptNativeIntArray::ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength)
    {LOGMEIN("JavascriptArray.cpp] 2780\n");
        SparseArraySegment<int32>::ClearElements(((SparseArraySegment<int32>*)seg)->elements + newSegmentLength, seg->length - newSegmentLength);
    }

    void JavascriptNativeFloatArray::ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength)
    {LOGMEIN("JavascriptArray.cpp] 2785\n");
        SparseArraySegment<double>::ClearElements(((SparseArraySegment<double>*)seg)->elements + newSegmentLength, seg->length - newSegmentLength);
    }

    Var JavascriptArray::DirectGetItem(uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 2790\n");
        SparseArraySegment<Var> *seg = (SparseArraySegment<Var>*)this->GetLastUsedSegment();
        uint32 offset = index - seg->left;
        if (index >= seg->left && offset < seg->length)
        {LOGMEIN("JavascriptArray.cpp] 2794\n");
            if (!SparseArraySegment<Var>::IsMissingItem(&seg->elements[offset]))
            {LOGMEIN("JavascriptArray.cpp] 2796\n");
                return seg->elements[offset];
            }
        }
        Var element;
        if (DirectGetItemAtFull(index, &element))
        {LOGMEIN("JavascriptArray.cpp] 2802\n");
            return element;
        }
        return GetType()->GetLibrary()->GetUndefined();
    }

    Var JavascriptNativeIntArray::DirectGetItem(uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 2809\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        SparseArraySegment<int32> *seg = (SparseArraySegment<int32>*)this->GetLastUsedSegment();
        uint32 offset = index - seg->left;
        if (index >= seg->left && offset < seg->length)
        {LOGMEIN("JavascriptArray.cpp] 2816\n");
            if (!SparseArraySegment<int32>::IsMissingItem(&seg->elements[offset]))
            {LOGMEIN("JavascriptArray.cpp] 2818\n");
                return JavascriptNumber::ToVar(seg->elements[offset], GetScriptContext());
            }
        }
        Var element;
        if (DirectGetItemAtFull(index, &element))
        {LOGMEIN("JavascriptArray.cpp] 2824\n");
            return element;
        }
        return GetType()->GetLibrary()->GetUndefined();
    }

    DescriptorFlags JavascriptNativeIntArray::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 2831\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif 
        int32 value = 0;
        return this->DirectGetItemAt(index, &value) ? WritableData : None;
    }


    Var JavascriptNativeFloatArray::DirectGetItem(uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 2841\n");
        SparseArraySegment<double> *seg = (SparseArraySegment<double>*)this->GetLastUsedSegment();
        uint32 offset = index - seg->left;
        if (index >= seg->left && offset < seg->length)
        {LOGMEIN("JavascriptArray.cpp] 2845\n");
            if (!SparseArraySegment<double>::IsMissingItem(&seg->elements[offset]))
            {LOGMEIN("JavascriptArray.cpp] 2847\n");
                return JavascriptNumber::ToVarWithCheck(seg->elements[offset], GetScriptContext());
            }
        }
        Var element;
        if (DirectGetItemAtFull(index, &element))
        {LOGMEIN("JavascriptArray.cpp] 2853\n");
            return element;
        }
        return GetType()->GetLibrary()->GetUndefined();
    }

    Var JavascriptArray::DirectGetItem(JavascriptString *propName, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 2860\n");
        PropertyRecord const * propertyRecord;
        scriptContext->GetOrAddPropertyRecord(propName->GetString(), propName->GetLength(), &propertyRecord);
        return JavascriptOperators::GetProperty(this, propertyRecord->GetPropertyId(), scriptContext, NULL);
    }

    BOOL JavascriptArray::DirectGetItemAtFull(uint32 index, Var* outVal)
    {LOGMEIN("JavascriptArray.cpp] 2867\n");
        if (this->DirectGetItemAt(index, outVal))
        {LOGMEIN("JavascriptArray.cpp] 2869\n");
            return TRUE;
        }

        ScriptContext* requestContext = type->GetScriptContext();
        return JavascriptOperators::GetItem(this, this->GetPrototype(), index, outVal, requestContext);
    }

    //
    // Link prev and current. If prev is NULL, make current the head segment.
    //
    void JavascriptArray::LinkSegmentsCommon(SparseArraySegmentBase* prev, SparseArraySegmentBase* current)
    {LOGMEIN("JavascriptArray.cpp] 2881\n");
        if (prev)
        {LOGMEIN("JavascriptArray.cpp] 2883\n");
            prev->next = current;
        }
        else
        {
            Assert(current);
            head = current;
        }
    }

    template<typename T>
    BOOL JavascriptArray::DirectDeleteItemAt(uint32 itemIndex)
    {LOGMEIN("JavascriptArray.cpp] 2895\n");
        if (itemIndex >= length)
        {LOGMEIN("JavascriptArray.cpp] 2897\n");
            return true;
        }
        SparseArraySegment<T>* next = (SparseArraySegment<T>*)GetBeginLookupSegment(itemIndex);
        while(next != nullptr && next->left <= itemIndex)
        {LOGMEIN("JavascriptArray.cpp] 2902\n");
            uint32 limit = next->left + next->length;
            if (itemIndex < limit)
            {LOGMEIN("JavascriptArray.cpp] 2905\n");
                next->SetElement(GetRecycler(), itemIndex, SparseArraySegment<T>::GetMissingItem());
                if(itemIndex - next->left == next->length - 1)
                {LOGMEIN("JavascriptArray.cpp] 2908\n");
                    --next->length;
                }
                else if(next == head)
                {LOGMEIN("JavascriptArray.cpp] 2912\n");
                    SetHasNoMissingValues(false);
                }
                break;
            }
            next = SparseArraySegment<T>::From(next->next);
        }
#ifdef VALIDATE_ARRAY
        ValidateArray();
#endif
        return true;
    }

    template <> Var JavascriptArray::ConvertToIndex(BigIndex idxDest, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 2926\n");
        return idxDest.ToNumber(scriptContext);
    }

    template <> uint32 JavascriptArray::ConvertToIndex(BigIndex idxDest, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 2931\n");
        // Note this is only for setting Array length which is a uint32
        return idxDest.IsSmallIndex() ? idxDest.GetSmallIndex() : UINT_MAX;
    }

    template <> Var JavascriptArray::ConvertToIndex(uint32 idxDest, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 2937\n");
        return  JavascriptNumber::ToVar(idxDest, scriptContext);
    }

    void JavascriptArray::ThrowErrorOnFailure(BOOL succeeded, ScriptContext* scriptContext, uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 2942\n");
        if (!succeeded)
        {LOGMEIN("JavascriptArray.cpp] 2944\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_CantRedefineProp, JavascriptConversion::ToString(JavascriptNumber::ToVar(index, scriptContext), scriptContext)->GetSz());
        }
    }

    void JavascriptArray::ThrowErrorOnFailure(BOOL succeeded, ScriptContext* scriptContext, BigIndex index)
    {LOGMEIN("JavascriptArray.cpp] 2950\n");
        if (!succeeded)
        {LOGMEIN("JavascriptArray.cpp] 2952\n");
            uint64 i = (uint64)(index.IsSmallIndex() ? index.GetSmallIndex() : index.GetBigIndex());
            JavascriptError::ThrowTypeError(scriptContext, JSERR_CantRedefineProp, JavascriptConversion::ToString(JavascriptNumber::ToVar(i, scriptContext), scriptContext)->GetSz());
        }
    }

    BOOL JavascriptArray::SetArrayLikeObjects(RecyclableObject* pDestObj, uint32 idxDest, Var aItem)
    {LOGMEIN("JavascriptArray.cpp] 2959\n");
        return pDestObj->SetItem(idxDest, aItem, Js::PropertyOperation_ThrowIfNotExtensible);
    }
    BOOL JavascriptArray::SetArrayLikeObjects(RecyclableObject* pDestObj, BigIndex idxDest, Var aItem)
    {LOGMEIN("JavascriptArray.cpp] 2963\n");
        ScriptContext* scriptContext = pDestObj->GetScriptContext();

        if (idxDest.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 2967\n");
            return pDestObj->SetItem(idxDest.GetSmallIndex(), aItem, Js::PropertyOperation_ThrowIfNotExtensible);
        }
        PropertyRecord const * propertyRecord;
        JavascriptOperators::GetPropertyIdForInt(idxDest.GetBigIndex(), scriptContext, &propertyRecord);
        return pDestObj->SetProperty(propertyRecord->GetPropertyId(), aItem, PropertyOperation_ThrowIfNotExtensible, nullptr);
    }

    template<typename T>
    void JavascriptArray::ConcatArgs(RecyclableObject* pDestObj, TypeId* remoteTypeIds,
        Js::Arguments& args, ScriptContext* scriptContext, uint start, BigIndex startIdxDest,
        BOOL FirstPromotedItemIsSpreadable, BigIndex FirstPromotedItemLength, bool spreadableCheckedAndTrue)
    {LOGMEIN("JavascriptArray.cpp] 2979\n");
        // This never gets called.
        Throw::InternalError();
    }
    //
    // Helper for EntryConcat. Concat args or elements of arg arrays into dest array.
    //
    template<typename T>
    void JavascriptArray::ConcatArgs(RecyclableObject* pDestObj, TypeId* remoteTypeIds,
        Js::Arguments& args, ScriptContext* scriptContext, uint start, uint startIdxDest,
        BOOL firstPromotedItemIsSpreadable, BigIndex firstPromotedItemLength, bool spreadableCheckedAndTrue)
    {LOGMEIN("JavascriptArray.cpp] 2990\n");
        JavascriptArray* pDestArray = nullptr;

        if (JavascriptArray::Is(pDestObj))
        {LOGMEIN("JavascriptArray.cpp] 2994\n");
            pDestArray = JavascriptArray::FromVar(pDestObj);
        }

        T idxDest = startIdxDest;
        for (uint idxArg = start; idxArg < args.Info.Count; idxArg++)
        {LOGMEIN("JavascriptArray.cpp] 3000\n");
            Var aItem = args[idxArg];
            bool spreadable = spreadableCheckedAndTrue;
            if (!spreadable && scriptContext->GetConfig()->IsES6IsConcatSpreadableEnabled())
            {LOGMEIN("JavascriptArray.cpp] 3004\n");
                // firstPromotedItemIsSpreadable is ONLY used to resume after a type promotion from uint32 to uint64
                // we do this because calls to IsConcatSpreadable are observable (a big deal for proxies) and we don't
                // want to do the work a second time as soon as we record the length we clear the flag.
                spreadable = firstPromotedItemIsSpreadable || JavascriptOperators::IsConcatSpreadable(aItem);

                if (!spreadable)
                {LOGMEIN("JavascriptArray.cpp] 3011\n");
                    JavascriptArray::SetConcatItem<T>(aItem, idxArg, pDestArray, pDestObj, idxDest, scriptContext);
                    ++idxDest;
                    continue;
                }
            }
            else
            {
                spreadableCheckedAndTrue = false; // if it was `true`, reset after the first use
            }

            if (pDestArray && JavascriptArray::IsDirectAccessArray(aItem) && JavascriptArray::IsDirectAccessArray(pDestArray)
                && BigIndex(idxDest + JavascriptArray::FromVar(aItem)->length).IsSmallIndex()) // Fast path
            {LOGMEIN("JavascriptArray.cpp] 3024\n");
                if (JavascriptNativeIntArray::Is(aItem))
                {LOGMEIN("JavascriptArray.cpp] 3026\n");
                    JavascriptNativeIntArray *pItemArray = JavascriptNativeIntArray::FromVar(aItem);
                    CopyNativeIntArrayElementsToVar(pDestArray, idxDest, pItemArray);
                    idxDest = idxDest + pItemArray->length;
                }
                else if (JavascriptNativeFloatArray::Is(aItem))
                {LOGMEIN("JavascriptArray.cpp] 3032\n");
                    JavascriptNativeFloatArray *pItemArray = JavascriptNativeFloatArray::FromVar(aItem);
                    CopyNativeFloatArrayElementsToVar(pDestArray, idxDest, pItemArray);
                    idxDest = idxDest + pItemArray->length;
                }
                else
                {
                    JavascriptArray* pItemArray = JavascriptArray::FromVar(aItem);
                    CopyArrayElements(pDestArray, idxDest, pItemArray);
                    idxDest = idxDest + pItemArray->length;
                }
            }
            else
            {
                // Flatten if other array or remote array (marked with TypeIds_Array)
                if (DynamicObject::IsAnyArray(aItem) || remoteTypeIds[idxArg] == TypeIds_Array || spreadable)
                {LOGMEIN("JavascriptArray.cpp] 3048\n");
                    //CONSIDER: enumerating remote array instead of walking all indices
                    BigIndex length;
                    if (firstPromotedItemIsSpreadable)
                    {LOGMEIN("JavascriptArray.cpp] 3052\n");
                        firstPromotedItemIsSpreadable = false;
                        length = firstPromotedItemLength;
                    }
                    else if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
                    {LOGMEIN("JavascriptArray.cpp] 3057\n");
                        // we can cast to uin64 without fear of converting negative numbers to large positive ones
                        // from int64 because ToLength makes negative lengths 0
                        length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(aItem, scriptContext), scriptContext);
                    }
                    else
                    {
                        length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(aItem, scriptContext), scriptContext);
                    }

                    if (PromoteToBigIndex(length,idxDest))
                    {
                        // This is a special case for spreadable objects. We do not pre-calculate the length
                        // in EntryConcat like we do with Arrays because a getProperty on an object Length
                        // is observable. The result is we have to check for overflows separately for
                        // spreadable objects and promote to a bigger index type when we find them.
                        ConcatArgs<BigIndex>(pDestArray, remoteTypeIds, args, scriptContext, idxArg, idxDest, /*firstPromotedItemIsSpreadable*/true, length);
                        return;
                    }

                    if (length + idxDest > FiftyThirdPowerOfTwoMinusOne) // 2^53-1: from ECMA 22.1.3.1 Array.prototype.concat(...arguments)
                    {LOGMEIN("JavascriptArray.cpp] 3078\n");
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_IllegalArraySizeAndLength);
                    }

                    RecyclableObject* itemObject = RecyclableObject::FromVar(aItem);
                    Var subItem;
                    uint32 lengthToUin32Max = length.IsSmallIndex() ? length.GetSmallIndex() : MaxArrayLength;
                    for (uint32 idxSubItem = 0u; idxSubItem < lengthToUin32Max; ++idxSubItem)
                    {LOGMEIN("JavascriptArray.cpp] 3086\n");
                        if (JavascriptOperators::HasItem(itemObject, idxSubItem))
                        {LOGMEIN("JavascriptArray.cpp] 3088\n");
                            subItem = JavascriptOperators::GetItem(itemObject, idxSubItem, scriptContext);

                            if (pDestArray)
                            {LOGMEIN("JavascriptArray.cpp] 3092\n");
                                pDestArray->DirectSetItemAt(idxDest, subItem);
                            }
                            else
                            {
                                ThrowErrorOnFailure(SetArrayLikeObjects(pDestObj, idxDest, subItem), scriptContext, idxDest);
                            }
                        }
                        ++idxDest;
                    }

                    for (BigIndex idxSubItem = MaxArrayLength; idxSubItem < length; ++idxSubItem)
                    {LOGMEIN("JavascriptArray.cpp] 3104\n");
                        PropertyRecord const * propertyRecord;
                        JavascriptOperators::GetPropertyIdForInt(idxSubItem.GetBigIndex(), scriptContext, &propertyRecord);
                        if (JavascriptOperators::HasProperty(itemObject,propertyRecord->GetPropertyId()))
                        {LOGMEIN("JavascriptArray.cpp] 3108\n");
                            subItem = JavascriptOperators::GetProperty(itemObject, propertyRecord->GetPropertyId(), scriptContext);
                            if (pDestArray)
                            {LOGMEIN("JavascriptArray.cpp] 3111\n");
                                pDestArray->DirectSetItemAt(idxDest, subItem);
                            }
                            else
                            {
                                ThrowErrorOnFailure(SetArrayLikeObjects(pDestObj, idxDest, subItem), scriptContext, idxSubItem);
                            }
                        }
                        ++idxDest;
                    }
                }
                else // concat 1 item
                {
                    JavascriptArray::SetConcatItem<T>(aItem, idxArg, pDestArray, pDestObj, idxDest, scriptContext);
                    ++idxDest;
                }
            }
        }
        if (!pDestArray)
        {LOGMEIN("JavascriptArray.cpp] 3130\n");
            pDestObj->SetProperty(PropertyIds::length, ConvertToIndex<T, Var>(idxDest, scriptContext), Js::PropertyOperation_None, nullptr);
        }
        else if (pDestArray->GetLength() != ConvertToIndex<T, uint32>(idxDest, scriptContext))
        {LOGMEIN("JavascriptArray.cpp] 3134\n");
            pDestArray->SetLength(ConvertToIndex<T, uint32>(idxDest, scriptContext));
        }
    }

    bool JavascriptArray::PromoteToBigIndex(BigIndex lhs, BigIndex rhs)
    {LOGMEIN("JavascriptArray.cpp] 3140\n");
        return false; // already a big index
    }

    bool JavascriptArray::PromoteToBigIndex(BigIndex lhs, uint32 rhs)
    {LOGMEIN("JavascriptArray.cpp] 3145\n");
        ::Math::RecordOverflowPolicy destLengthOverflow;
        if (lhs.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 3148\n");
            UInt32Math::Add(lhs.GetSmallIndex(), rhs, destLengthOverflow);
            return destLengthOverflow.HasOverflowed();
        }
        return true;
    }

    JavascriptArray* JavascriptArray::ConcatIntArgs(JavascriptNativeIntArray* pDestArray, TypeId *remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 3156\n");
        uint idxDest = 0u;
        for (uint idxArg = 0; idxArg < args.Info.Count; idxArg++)
        {LOGMEIN("JavascriptArray.cpp] 3159\n");
            Var aItem = args[idxArg];
            bool spreadableCheckedAndTrue = false;

            if (scriptContext->GetConfig()->IsES6IsConcatSpreadableEnabled())
            {LOGMEIN("JavascriptArray.cpp] 3164\n");
                if (JavascriptOperators::IsConcatSpreadable(aItem))
                {LOGMEIN("JavascriptArray.cpp] 3166\n");
                    spreadableCheckedAndTrue = true;
                }
                else
                {
                    pDestArray->SetItem(idxDest, aItem, PropertyOperation_ThrowIfNotExtensible);
                    idxDest = idxDest + 1;
                    if (!JavascriptNativeIntArray::Is(pDestArray)) // SetItem could convert pDestArray to a var array if aItem is not an integer if so fall back
                    {
                        ConcatArgs<uint>(pDestArray, remoteTypeIds, args, scriptContext, idxArg + 1, idxDest);
                        return pDestArray;
                    }
                    continue;
                }
            }

            if (JavascriptNativeIntArray::Is(aItem)) // Fast path
            {LOGMEIN("JavascriptArray.cpp] 3183\n");
                JavascriptNativeIntArray* pItemArray = JavascriptNativeIntArray::FromVar(aItem);
                bool converted = CopyNativeIntArrayElements(pDestArray, idxDest, pItemArray);
                idxDest = idxDest + pItemArray->length;
                if (converted)
                {
                    // Copying the last array forced a conversion, so switch over to the var version
                    // to finish.
                    ConcatArgs<uint>(pDestArray, remoteTypeIds, args, scriptContext, idxArg + 1, idxDest);
                    return pDestArray;
                }
            }
            else if (!JavascriptArray::IsAnyArray(aItem) && remoteTypeIds[idxArg] != TypeIds_Array)
            {LOGMEIN("JavascriptArray.cpp] 3196\n");
                if (TaggedInt::Is(aItem))
                {LOGMEIN("JavascriptArray.cpp] 3198\n");
                    pDestArray->DirectSetItemAt(idxDest, TaggedInt::ToInt32(aItem));
                }
                else
                {
#if DBG
                    int32 int32Value;
                    Assert(
                        JavascriptNumber::TryGetInt32Value(JavascriptNumber::GetValue(aItem), &int32Value) &&
                        !SparseArraySegment<int32>::IsMissingItem(&int32Value));
#endif
                    pDestArray->DirectSetItemAt(idxDest, static_cast<int32>(JavascriptNumber::GetValue(aItem)));
                }
                ++idxDest;
            }
            else
            {
                JavascriptArray *pVarDestArray = JavascriptNativeIntArray::ConvertToVarArray(pDestArray);
                ConcatArgs<uint>(pVarDestArray, remoteTypeIds, args, scriptContext, idxArg, idxDest, spreadableCheckedAndTrue);
                return pVarDestArray;
            }
        }
        if (pDestArray->GetLength() != idxDest)
        {LOGMEIN("JavascriptArray.cpp] 3221\n");
            pDestArray->SetLength(idxDest);
        }
        return pDestArray;
    }

    JavascriptArray* JavascriptArray::ConcatFloatArgs(JavascriptNativeFloatArray* pDestArray, TypeId *remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 3228\n");
        uint idxDest = 0u;
        for (uint idxArg = 0; idxArg < args.Info.Count; idxArg++)
        {LOGMEIN("JavascriptArray.cpp] 3231\n");
            Var aItem = args[idxArg];

            bool spreadableCheckedAndTrue = false;

            if (scriptContext->GetConfig()->IsES6IsConcatSpreadableEnabled())
            {LOGMEIN("JavascriptArray.cpp] 3237\n");
                if (JavascriptOperators::IsConcatSpreadable(aItem))
                {LOGMEIN("JavascriptArray.cpp] 3239\n");
                    spreadableCheckedAndTrue = true;
                }
                else
                {
                    pDestArray->SetItem(idxDest, aItem, PropertyOperation_ThrowIfNotExtensible);

                    idxDest = idxDest + 1;
                    if (!JavascriptNativeFloatArray::Is(pDestArray)) // SetItem could convert pDestArray to a var array if aItem is not an integer if so fall back
                    {
                        ConcatArgs<uint>(pDestArray, remoteTypeIds, args, scriptContext, idxArg + 1, idxDest);
                        return pDestArray;
                    }
                    continue;
                }
            }

            bool converted;
            if (JavascriptArray::IsAnyArray(aItem) || remoteTypeIds[idxArg] == TypeIds_Array)
            {LOGMEIN("JavascriptArray.cpp] 3258\n");
                if (JavascriptNativeIntArray::Is(aItem)) // Fast path
                {LOGMEIN("JavascriptArray.cpp] 3260\n");
                    JavascriptNativeIntArray *pIntArray = JavascriptNativeIntArray::FromVar(aItem);
                    converted = CopyNativeIntArrayElementsToFloat(pDestArray, idxDest, pIntArray);
                    idxDest = idxDest + pIntArray->length;
                }
                else if (JavascriptNativeFloatArray::Is(aItem))
                {LOGMEIN("JavascriptArray.cpp] 3266\n");
                    JavascriptNativeFloatArray* pItemArray = JavascriptNativeFloatArray::FromVar(aItem);
                    converted = CopyNativeFloatArrayElements(pDestArray, idxDest, pItemArray);
                    idxDest = idxDest + pItemArray->length;
                }
                else
                {
                    JavascriptArray *pVarDestArray = JavascriptNativeFloatArray::ConvertToVarArray(pDestArray);
                    ConcatArgs<uint>(pVarDestArray, remoteTypeIds, args, scriptContext, idxArg, idxDest, spreadableCheckedAndTrue);
                    return pVarDestArray;
                }

                if (converted)
                {
                    // Copying the last array forced a conversion, so switch over to the var version
                    // to finish.
                    ConcatArgs<uint>(pDestArray, remoteTypeIds, args, scriptContext, idxArg + 1, idxDest);
                    return pDestArray;
                }
            }
            else
            {
                if (TaggedInt::Is(aItem))
                {LOGMEIN("JavascriptArray.cpp] 3289\n");
                    pDestArray->DirectSetItemAt(idxDest, (double)TaggedInt::ToInt32(aItem));
                }
                else
                {
                    Assert(JavascriptNumber::Is(aItem));
                    pDestArray->DirectSetItemAt(idxDest, JavascriptNumber::GetValue(aItem));
                }
                ++idxDest;
            }
        }
        if (pDestArray->GetLength() != idxDest)
        {LOGMEIN("JavascriptArray.cpp] 3301\n");
            pDestArray->SetLength(idxDest);
        }

        return pDestArray;
    }

    bool JavascriptArray::BoxConcatItem(Var aItem, uint idxArg, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 3309\n");
        return idxArg == 0 && !JavascriptOperators::IsObject(aItem);
    }

    Var JavascriptArray::EntryConcat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 3323\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.concat"));
        }

        //
        // Compute the destination ScriptArray size:
        // - Each item, flattening only one level if a ScriptArray.
        //

        uint32 cDestLength = 0;
        JavascriptArray * pDestArray = NULL;

        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault + (args.Info.Count * sizeof(TypeId*)));
        TypeId* remoteTypeIds = (TypeId*)_alloca(args.Info.Count * sizeof(TypeId*));

        bool isInt = true;
        bool isFloat = true;
        ::Math::RecordOverflowPolicy destLengthOverflow;
        for (uint idxArg = 0; idxArg < args.Info.Count; idxArg++)
        {LOGMEIN("JavascriptArray.cpp] 3342\n");
            Var aItem = args[idxArg];
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(aItem);
#endif
            if (DynamicObject::IsAnyArray(aItem)) // Get JavascriptArray or ES5Array length
            {LOGMEIN("JavascriptArray.cpp] 3348\n");
                JavascriptArray * pItemArray = JavascriptArray::FromAnyArray(aItem);
                if (isFloat)
                {LOGMEIN("JavascriptArray.cpp] 3351\n");
                    if (!JavascriptNativeIntArray::Is(pItemArray))
                    {LOGMEIN("JavascriptArray.cpp] 3353\n");
                        isInt = false;
                        if (!JavascriptNativeFloatArray::Is(pItemArray))
                        {LOGMEIN("JavascriptArray.cpp] 3356\n");
                            isFloat = false;
                        }
                    }
                }
                cDestLength = UInt32Math::Add(cDestLength, pItemArray->GetLength(), destLengthOverflow);
            }
            else // Get remote array or object length
            {
                // We already checked for types derived from JavascriptArray. These are types that should behave like array
                // i.e. proxy to array and remote array.
                if (JavascriptOperators::IsArray(aItem))
                {LOGMEIN("JavascriptArray.cpp] 3368\n");
                    // Don't try to preserve nativeness of remote arrays. The extra complexity is probably not
                    // worth it.
                    isInt = false;
                    isFloat = false;
                    if (!JavascriptProxy::Is(aItem))
                    {LOGMEIN("JavascriptArray.cpp] 3374\n");
                        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
                        {LOGMEIN("JavascriptArray.cpp] 3376\n");
                            int64 len = JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(aItem, scriptContext), scriptContext);
                            // clipping to MaxArrayLength will overflow when added to cDestLength which we catch below
                            cDestLength = UInt32Math::Add(cDestLength, len < MaxArrayLength ? (uint32)len : MaxArrayLength, destLengthOverflow);
                        }
                        else
                        {
                            uint len = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(aItem, scriptContext), scriptContext);
                            cDestLength = UInt32Math::Add(cDestLength, len, destLengthOverflow);
                        }
                    }
                    remoteTypeIds[idxArg] = TypeIds_Array; // Mark remote array, no matter remote JavascriptArray or ES5Array.
                }
                else
                {
                    if (isFloat)
                    {
                        if (BoxConcatItem(aItem, idxArg, scriptContext))
                        {LOGMEIN("JavascriptArray.cpp] 3394\n");
                            // A primitive will be boxed, so we have to create a var array for the result.
                            isInt = false;
                            isFloat = false;
                        }
                        else if (!TaggedInt::Is(aItem))
                        {LOGMEIN("JavascriptArray.cpp] 3400\n");
                            if (!JavascriptNumber::Is(aItem))
                            {LOGMEIN("JavascriptArray.cpp] 3402\n");
                                isInt = false;
                                isFloat = false;
                            }
                            else if (isInt)
                            {LOGMEIN("JavascriptArray.cpp] 3407\n");
                                int32 int32Value;
                                if(!JavascriptNumber::TryGetInt32Value(JavascriptNumber::GetValue(aItem), &int32Value) ||
                                    SparseArraySegment<int32>::IsMissingItem(&int32Value))
                                {LOGMEIN("JavascriptArray.cpp] 3411\n");
                                    isInt = false;
                                }
                            }
                        }
                        else if(isInt)
                        {LOGMEIN("JavascriptArray.cpp] 3417\n");
                            int32 int32Value = TaggedInt::ToInt32(aItem);
                            if(SparseArraySegment<int32>::IsMissingItem(&int32Value))
                            {LOGMEIN("JavascriptArray.cpp] 3420\n");
                                isInt = false;
                            }
                        }
                    }

                    remoteTypeIds[idxArg] = TypeIds_Limit;
                    cDestLength = UInt32Math::Add(cDestLength, 1, destLengthOverflow);
                }
            }
        }
        if (destLengthOverflow.HasOverflowed())
        {LOGMEIN("JavascriptArray.cpp] 3432\n");
            cDestLength = MaxArrayLength;
            isInt = false;
            isFloat = false;
        }

        //
        // Create the destination array
        //
        RecyclableObject* pDestObj = nullptr;
        bool isArray = false;
        pDestObj = ArraySpeciesCreate(args[0], 0, scriptContext);
        if (pDestObj)
        {LOGMEIN("JavascriptArray.cpp] 3445\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pDestObj);
#endif
            // Check the thing that species create made. If it's a native array that can't handle the source
            // data, convert it. If it's a more conservative kind of array than the source data, indicate that
            // so that the data will be converted on copy.
            if (isInt)
            {LOGMEIN("JavascriptArray.cpp] 3453\n");
                if (JavascriptNativeIntArray::Is(pDestObj))
                {LOGMEIN("JavascriptArray.cpp] 3455\n");
                    isArray = true;
                }
                else
                {
                    isInt = false;
                    isFloat = JavascriptNativeFloatArray::Is(pDestObj);
                    isArray = JavascriptArray::Is(pDestObj);
                }
            }
            else if (isFloat)
            {LOGMEIN("JavascriptArray.cpp] 3466\n");
                if (JavascriptNativeIntArray::Is(pDestObj))
                {LOGMEIN("JavascriptArray.cpp] 3468\n");
                    JavascriptNativeIntArray::ToNativeFloatArray(JavascriptNativeIntArray::FromVar(pDestObj));
                    isArray = true;
                }
                else
                {
                    isFloat = JavascriptNativeFloatArray::Is(pDestObj);
                    isArray = JavascriptArray::Is(pDestObj);
                }
            }
            else
            {
                if (JavascriptNativeIntArray::Is(pDestObj))
                {LOGMEIN("JavascriptArray.cpp] 3481\n");
                    JavascriptNativeIntArray::ToVarArray(JavascriptNativeIntArray::FromVar(pDestObj));
                    isArray = true;
                }
                else if (JavascriptNativeFloatArray::Is(pDestObj))
                {LOGMEIN("JavascriptArray.cpp] 3486\n");
                    JavascriptNativeFloatArray::ToVarArray(JavascriptNativeFloatArray::FromVar(pDestObj));
                    isArray = true;
                }
                else
                {
                    isArray = JavascriptArray::Is(pDestObj);
                }
            }
        }

        if (pDestObj == nullptr || isArray)
        {LOGMEIN("JavascriptArray.cpp] 3498\n");
            if (isInt)
            {LOGMEIN("JavascriptArray.cpp] 3500\n");
                JavascriptNativeIntArray *pIntArray = isArray ? JavascriptNativeIntArray::FromVar(pDestObj) : scriptContext->GetLibrary()->CreateNativeIntArray(cDestLength);
                pIntArray->EnsureHead<int32>();
                pDestArray = ConcatIntArgs(pIntArray, remoteTypeIds, args, scriptContext);
            }
            else if (isFloat)
            {LOGMEIN("JavascriptArray.cpp] 3506\n");
                JavascriptNativeFloatArray *pFArray = isArray ? JavascriptNativeFloatArray::FromVar(pDestObj) : scriptContext->GetLibrary()->CreateNativeFloatArray(cDestLength);
                pFArray->EnsureHead<double>();
                pDestArray = ConcatFloatArgs(pFArray, remoteTypeIds, args, scriptContext);
            }
            else
            {

                pDestArray = isArray ?  JavascriptArray::FromVar(pDestObj) : scriptContext->GetLibrary()->CreateArray(cDestLength);
                // if the constructor has changed then we no longer specialize for ints and floats
                pDestArray->EnsureHead<Var>();
                ConcatArgsCallingHelper(pDestArray, remoteTypeIds, args, scriptContext, destLengthOverflow);
            }

            //
            // Return the new array instance.
            //

#ifdef VALIDATE_ARRAY
            pDestArray->ValidateArray();
#endif

            return pDestArray;
        }
        Assert(pDestObj);
        ConcatArgsCallingHelper(pDestObj, remoteTypeIds, args, scriptContext, destLengthOverflow);

        return pDestObj;
    }

    void JavascriptArray::ConcatArgsCallingHelper(RecyclableObject* pDestObj, TypeId* remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext, ::Math::RecordOverflowPolicy &destLengthOverflow)
    {LOGMEIN("JavascriptArray.cpp] 3537\n");
        if (destLengthOverflow.HasOverflowed())
        {
            ConcatArgs<BigIndex>(pDestObj, remoteTypeIds, args, scriptContext);
        }
        else
        {
            // Use faster uint32 version if no overflow
            ConcatArgs<uint32>(pDestObj, remoteTypeIds, args, scriptContext);
        }
    }

    template<typename T>
    /* static */ void JavascriptArray::SetConcatItem(Var aItem, uint idxArg, JavascriptArray* pDestArray, RecyclableObject* pDestObj, T idxDest, ScriptContext *scriptContext)
    {
        if (BoxConcatItem(aItem, idxArg, scriptContext))
        {LOGMEIN("JavascriptArray.cpp] 3553\n");
            // bug# 725784: ES5: not calling ToObject in Step 1 of 15.4.4.4
            RecyclableObject* pObj = nullptr;
            if (FALSE == JavascriptConversion::ToObject(aItem, scriptContext, &pObj))
            {LOGMEIN("JavascriptArray.cpp] 3557\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.concat"));
            }
            if (pDestArray)
            {LOGMEIN("JavascriptArray.cpp] 3561\n");
                pDestArray->DirectSetItemAt(idxDest, pObj);
            }
            else
            {
                SetArrayLikeObjects(pDestObj, idxDest, pObj);
            }
        }
        else
        {
            if (pDestArray)
            {LOGMEIN("JavascriptArray.cpp] 3572\n");
                pDestArray->DirectSetItemAt(idxDest, aItem);
            }
            else
            {
                SetArrayLikeObjects(pDestObj, idxDest, aItem);
            }
        }
    }

    uint32 JavascriptArray::GetFromIndex(Var arg, uint32 length, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 3583\n");
        uint32 fromIndex;

        if (TaggedInt::Is(arg))
        {LOGMEIN("JavascriptArray.cpp] 3587\n");
            int intValue = TaggedInt::ToInt32(arg);

            if (intValue >= 0)
            {LOGMEIN("JavascriptArray.cpp] 3591\n");
                fromIndex = intValue;
            }
            else
            {
                // (intValue + length) may exceed 2^31 or may be < 0, so promote to int64
                fromIndex = (uint32)max(0i64, (int64)(length) + intValue);
            }
        }
        else
        {
            double value = JavascriptConversion::ToInteger(arg, scriptContext);
            if (value > length)
            {LOGMEIN("JavascriptArray.cpp] 3604\n");
                return (uint32)-1;
            }
            else if (value >= 0)
            {LOGMEIN("JavascriptArray.cpp] 3608\n");
                fromIndex = (uint32)value;
            }
            else
            {
                fromIndex = (uint32)max((double)0, value + length);
            }
        }

        return fromIndex;
    }

    uint64 JavascriptArray::GetFromIndex(Var arg, uint64 length, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 3621\n");
        uint64 fromIndex;

        if (TaggedInt::Is(arg))
        {LOGMEIN("JavascriptArray.cpp] 3625\n");
            int64 intValue = TaggedInt::ToInt64(arg);

            if (intValue >= 0)
            {LOGMEIN("JavascriptArray.cpp] 3629\n");
                fromIndex = intValue;
            }
            else
            {
                fromIndex = max((int64)0, (int64)(intValue + length));
            }
        }
        else
        {
            double value = JavascriptConversion::ToInteger(arg, scriptContext);
            if (value > length)
            {LOGMEIN("JavascriptArray.cpp] 3641\n");
                return (uint64)-1;
            }
            else if (value >= 0)
            {LOGMEIN("JavascriptArray.cpp] 3645\n");
                fromIndex = (uint64)value;
            }
            else
            {
                fromIndex = (uint64)max((double)0, value + length);
            }
        }

        return fromIndex;
    }

    int64 JavascriptArray::GetFromLastIndex(Var arg, int64 length, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 3658\n");
        int64 fromIndex;

        if (TaggedInt::Is(arg))
        {LOGMEIN("JavascriptArray.cpp] 3662\n");
            int intValue = TaggedInt::ToInt32(arg);

            if (intValue >= 0)
            {LOGMEIN("JavascriptArray.cpp] 3666\n");
                fromIndex = min<int64>(intValue, length - 1);
            }
            else if ((uint32)-intValue > length)
            {LOGMEIN("JavascriptArray.cpp] 3670\n");
                return length;
            }
            else
            {
                fromIndex = intValue + length;
            }
        }
        else
        {
            double value = JavascriptConversion::ToInteger(arg, scriptContext);

            if (value >= 0)
            {LOGMEIN("JavascriptArray.cpp] 3683\n");
                fromIndex = (int64)min(value, (double)(length - 1));
            }
            else if (value + length < 0)
            {LOGMEIN("JavascriptArray.cpp] 3687\n");
                return length;
            }
            else
            {
                fromIndex = (int64)(value + length);
            }
        }

        return fromIndex;
    }

    // includesAlgorithm specifies to follow ES7 Array.prototype.includes semantics instead of Array.prototype.indexOf
    // Differences
    //    1. Returns boolean true or false value instead of the search hit index
    //    2. Follows SameValueZero algorithm instead of StrictEquals
    //    3. Missing values are scanned if the search value is undefined

    template <bool includesAlgorithm>
    Var JavascriptArray::IndexOfHelper(Arguments const & args, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 3707\n");
        RecyclableObject* obj = nullptr;
        JavascriptArray* pArr = nullptr;
        BigIndex length;
        Var trueValue = scriptContext->GetLibrary()->GetTrue();
        Var falseValue = scriptContext->GetLibrary()->GetFalse();

        if (JavascriptArray::Is(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 3715\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 3725\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.indexOf"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 3731\n");
            length = (uint64)JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }
        else
        {
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        Var search;
        uint32 fromIndex = 0;
        uint64 fromIndex64 = 0;

        // The evaluation of method arguments may change the type of the array. Hence, we do that prior to the actual helper method calls.
        // The if clause of the conditional statement below applies to an JavascriptArray or TypedArray instances. The rest of the conditional 
        // clauses apply to an ES5Array or other valid Javascript objects.
        if ((pArr || TypedArrayBase::Is(obj)) && (length.IsSmallIndex() || length.IsUint32Max()))
        {LOGMEIN("JavascriptArray.cpp] 3747\n");
            uint32 len = length.IsUint32Max() ? MaxArrayLength : length.GetSmallIndex();
            if (!GetParamForIndexOf(len, args, search, fromIndex, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 3750\n");
                return includesAlgorithm ? falseValue : TaggedInt::ToVarUnchecked(-1);
            }
        }
        else if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 3755\n");
            if (!GetParamForIndexOf(length.GetSmallIndex(), args, search, fromIndex, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 3757\n");
                return includesAlgorithm ? falseValue : TaggedInt::ToVarUnchecked(-1);
            }
        }
        else
        {
            if (!GetParamForIndexOf(length.GetBigIndex(), args, search, fromIndex64, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 3764\n");
                return includesAlgorithm ? falseValue : TaggedInt::ToVarUnchecked(-1);
            }
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of fromIndex argument may convert the array to an ES5 array.
        if (pArr && !JavascriptArray::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 3771\n");
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 3777\n");
            if (length.IsSmallIndex() || length.IsUint32Max())
            {LOGMEIN("JavascriptArray.cpp] 3779\n");
                uint32 len = length.IsUint32Max() ? MaxArrayLength : length.GetSmallIndex();
                int32 index = pArr->HeadSegmentIndexOfHelper(search, fromIndex, len, includesAlgorithm, scriptContext);

                // If we found the search value in the head segment, or if we determined there is no need to search other segments,
                // we stop right here.
                if (index != -1 || fromIndex == -1)
                {LOGMEIN("JavascriptArray.cpp] 3786\n");
                    if (includesAlgorithm)
                    {LOGMEIN("JavascriptArray.cpp] 3788\n");
                        //Array.prototype.includes
                        return (index == -1) ? falseValue : trueValue;
                    }
                    else
                    {
                        //Array.prototype.indexOf
                        return JavascriptNumber::ToVar(index, scriptContext);
                    }
                }

                //  If we really must search other segments, let's do it now. We'll have to search the slow way (dealing with holes, etc.).
                switch (pArr->GetTypeId())
                {LOGMEIN("JavascriptArray.cpp] 3801\n");
                case Js::TypeIds_Array:
                    return TemplatedIndexOfHelper<includesAlgorithm>(pArr, search, fromIndex, len, scriptContext);
                case Js::TypeIds_NativeIntArray:
                    return TemplatedIndexOfHelper<includesAlgorithm>(JavascriptNativeIntArray::FromVar(pArr), search, fromIndex, len, scriptContext);
                case Js::TypeIds_NativeFloatArray:
                    return TemplatedIndexOfHelper<includesAlgorithm>(JavascriptNativeFloatArray::FromVar(pArr), search, fromIndex, len, scriptContext);
                default:
                    AssertMsg(FALSE, "invalid array typeid");
                    return TemplatedIndexOfHelper<includesAlgorithm>(pArr, search, fromIndex, len, scriptContext);
                }
            }
        }

        // source object is not a JavascriptArray but source could be a TypedArray
        if (TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 3817\n");
            if (length.IsSmallIndex() || length.IsUint32Max())
            {LOGMEIN("JavascriptArray.cpp] 3819\n");
                return TemplatedIndexOfHelper<includesAlgorithm>(TypedArrayBase::FromVar(obj), search, fromIndex, length.GetSmallIndex(), scriptContext);
            }
        }
        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 3824\n");
            return TemplatedIndexOfHelper<includesAlgorithm>(obj, search, fromIndex, length.GetSmallIndex(), scriptContext);
        }
        else
        {
            return TemplatedIndexOfHelper<includesAlgorithm>(obj, search, fromIndex64, length.GetBigIndex(), scriptContext);
        }
    }

    // Array.prototype.indexOf as defined in ES6.0 (final) Section 22.1.3.11
    Var JavascriptArray::EntryIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_indexOf);

        Var returnValue =  IndexOfHelper<false>(args, scriptContext);

        //IndexOfHelper code is reused for array.prototype.includes as well. Let us assert here we didn't get a true or false instead of index
        Assert(returnValue != scriptContext->GetLibrary()->GetTrue() && returnValue != scriptContext->GetLibrary()->GetFalse());

        return returnValue;
    }

    Var JavascriptArray::EntryIncludes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_includes);

        Var returnValue = IndexOfHelper<true>(args, scriptContext);
        Assert(returnValue == scriptContext->GetLibrary()->GetTrue() || returnValue == scriptContext->GetLibrary()->GetFalse());

        return returnValue;
    }


    template<typename T>
    BOOL JavascriptArray::GetParamForIndexOf(T length, Arguments const& args, Var& search, T& fromIndex, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 3873\n");
        if (length == 0)
        {LOGMEIN("JavascriptArray.cpp] 3875\n");
            return false;
        }

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 3880\n");
            fromIndex = GetFromIndex(args[2], length, scriptContext);
            if (fromIndex >= length)
            {LOGMEIN("JavascriptArray.cpp] 3883\n");
                return false;
            }
            search = args[1];
        }
        else
        {
            fromIndex = 0;
            search = args.Info.Count > 1 ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        }
        return true;
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(RecyclableObject * obj, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3898\n");
        // Note: Sometime cross site array go down this path to get the marshalling
        Assert(!VirtualTableInfo<JavascriptArray>::HasVirtualTable(obj)
            && !VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(obj)
            && !VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(obj));
        if (checkHasItem && !JavascriptOperators::HasItem(obj, index))
        {LOGMEIN("JavascriptArray.cpp] 3904\n");
            return FALSE;
        }
        return JavascriptOperators::GetItem(obj, index, element, scriptContext);
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(RecyclableObject * obj, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3912\n");
        // Note: Sometime cross site array go down this path to get the marshalling
        Assert(!VirtualTableInfo<JavascriptArray>::HasVirtualTable(obj)
            && !VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(obj)
            && !VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(obj));
        PropertyRecord const * propertyRecord;
        JavascriptOperators::GetPropertyIdForInt(index, scriptContext, &propertyRecord);
        if (checkHasItem && !JavascriptOperators::HasProperty(obj, propertyRecord->GetPropertyId()))
        {LOGMEIN("JavascriptArray.cpp] 3920\n");
            return FALSE;
        }
        *element = JavascriptOperators::GetProperty(obj, propertyRecord->GetPropertyId(), scriptContext);
        return *element != scriptContext->GetLibrary()->GetUndefined();

    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptArray *pArr, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3930\n");
        Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(pArr)
            || VirtualTableInfo<CrossSiteObject<JavascriptArray>>::HasVirtualTable(pArr));
        return pArr->JavascriptArray::DirectGetItemAtFull(index, element);
    }
    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptArray *pArr, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3937\n");
        // This should never get called.
        Assert(false);
        Throw::InternalError();
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptNativeIntArray *pArr, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3945\n");
        Assert(VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(pArr)
            || VirtualTableInfo<CrossSiteObject<JavascriptNativeIntArray>>::HasVirtualTable(pArr));
        return pArr->JavascriptNativeIntArray::DirectGetItemAtFull(index, element);
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptNativeIntArray *pArr, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3953\n");
        // This should never get called.
        Assert(false);
        Throw::InternalError();
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptNativeFloatArray *pArr, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3961\n");
        Assert(VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(pArr)
            || VirtualTableInfo<CrossSiteObject<JavascriptNativeFloatArray>>::HasVirtualTable(pArr));
        return pArr->JavascriptNativeFloatArray::DirectGetItemAtFull(index, element);
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptNativeFloatArray *pArr, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3969\n");
        // This should never get called.
        Assert(false);
        Throw::InternalError();
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(TypedArrayBase * typedArrayBase, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3977\n");
        // We need to do explicit check for items since length value may not actually match the actual TypedArray length.
        // User could add a length property to a TypedArray instance which lies and returns a different value from the underlying length.
        // Since this method can be called via Array.prototype.indexOf with .apply or .call passing a TypedArray as this parameter
        // we don't know whether or not length == typedArrayBase->GetLength().
        if (checkHasItem && !typedArrayBase->HasItem(index))
        {LOGMEIN("JavascriptArray.cpp] 3983\n");
            return false;
        }

        *element = typedArrayBase->DirectGetItem(index);
        return true;
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(TypedArrayBase * typedArrayBase, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {LOGMEIN("JavascriptArray.cpp] 3993\n");
        // This should never get called.
        Assert(false);
        Throw::InternalError();
    }

    template <bool includesAlgorithm, typename T, typename P>
    Var JavascriptArray::TemplatedIndexOfHelper(T * pArr, Var search, P fromIndex, P toIndex, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4001\n");
        Var element = nullptr;
        bool isSearchTaggedInt = TaggedInt::Is(search);
        bool doUndefinedSearch = includesAlgorithm && JavascriptOperators::GetTypeId(search) == TypeIds_Undefined;

        Var trueValue = scriptContext->GetLibrary()->GetTrue();
        Var falseValue = scriptContext->GetLibrary()->GetFalse();

        //Consider: enumerating instead of walking all indices
        for (P i = fromIndex; i < toIndex; i++)
        {
            if (!TryTemplatedGetItem(pArr, i, &element, scriptContext, !includesAlgorithm))
            {LOGMEIN("JavascriptArray.cpp] 4013\n");
                if (doUndefinedSearch)
                {LOGMEIN("JavascriptArray.cpp] 4015\n");
                    return trueValue;
                }
                continue;
            }

            if (isSearchTaggedInt && TaggedInt::Is(element))
            {LOGMEIN("JavascriptArray.cpp] 4022\n");
                if (element == search)
                {LOGMEIN("JavascriptArray.cpp] 4024\n");
                    return includesAlgorithm? trueValue : JavascriptNumber::ToVar(i, scriptContext);
                }
                continue;
            }

            if (includesAlgorithm)
            {LOGMEIN("JavascriptArray.cpp] 4031\n");
                //Array.prototype.includes
                if (JavascriptConversion::SameValueZero(element, search))
                {LOGMEIN("JavascriptArray.cpp] 4034\n");
                    return trueValue;
                }
            }
            else
            {
                //Array.prototype.indexOf
                if (JavascriptOperators::StrictEqual(element, search, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4042\n");
                    return JavascriptNumber::ToVar(i, scriptContext);
                }
            }
        }

        return includesAlgorithm ? falseValue :  TaggedInt::ToVarUnchecked(-1);
    }

    int32 JavascriptArray::HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4052\n");
        Assert(Is(GetTypeId()) && !JavascriptNativeArray::Is(GetTypeId()));

        if (!HasNoMissingValues() || fromIndex >= GetHead()->length)
        {LOGMEIN("JavascriptArray.cpp] 4056\n");
            return -1;
        }

        bool isSearchTaggedInt = TaggedInt::Is(search);
        // We need to cast head segment to SparseArraySegment<Var> to have access to GetElement (onSparseArraySegment<T>). Because there are separate overloads of this
        // virtual method on JavascriptNativeIntArray and JavascriptNativeFloatArray, we know this version of this method will only be called for true JavascriptArray, and not for
        // either of the derived native arrays, so the elements of each segment used here must be Vars. Hence, the cast is safe.
        SparseArraySegment<Var>* head = static_cast<SparseArraySegment<Var>*>(GetHead());
        uint32 toIndexTrimmed = toIndex <= head->length ? toIndex : head->length;
        for (uint32 i = fromIndex; i < toIndexTrimmed; i++)
        {LOGMEIN("JavascriptArray.cpp] 4067\n");
            Var element = head->GetElement(i);
            if (isSearchTaggedInt && TaggedInt::Is(element))
            {LOGMEIN("JavascriptArray.cpp] 4070\n");
                if (search == element)
                {LOGMEIN("JavascriptArray.cpp] 4072\n");
                    return i;
                }
            }
            else if (includesAlgorithm && JavascriptConversion::SameValueZero(element, search))
            {LOGMEIN("JavascriptArray.cpp] 4077\n");
                //Array.prototype.includes
                return i;
            }
            else if (JavascriptOperators::StrictEqual(element, search, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 4082\n");
                //Array.prototype.indexOf
                return i;
            }
        }

        // Element not found in the head segment. Keep looking only if the range of indices extends past
        // the head segment.
        fromIndex = toIndex > GetHead()->length ? GetHead()->length : -1;
        return -1;
    }

    template<typename T>
    bool AreAllBytesEqual(T value)
    {LOGMEIN("JavascriptArray.cpp] 4096\n");
        byte* bValue = (byte*)&value;
        byte firstByte = *bValue++;
        for (int i = 1; i < sizeof(T); ++i)
        {LOGMEIN("JavascriptArray.cpp] 4100\n");
            if (*bValue++ != firstByte)
            {LOGMEIN("JavascriptArray.cpp] 4102\n");
                return false;
            }
        }
        return true;
    }

    template<>
    void JavascriptArray::CopyValueToSegmentBuferNoCheck(Field(double)* buffer, uint32 length, double value)
    {LOGMEIN("JavascriptArray.cpp] 4111\n");
        if (JavascriptNumber::IsZero(value) && !JavascriptNumber::IsNegZero(value))
        {
            memset(buffer, 0, sizeof(double) * length);
        }
        else
        {
            for (uint32 i = 0; i < length; i++)
            {LOGMEIN("JavascriptArray.cpp] 4119\n");
                buffer[i] = value;
            }
        }
    }

    template<>
    void JavascriptArray::CopyValueToSegmentBuferNoCheck(Field(int32)* buffer, uint32 length, int32 value)
    {LOGMEIN("JavascriptArray.cpp] 4127\n");
        if (value == 0 || AreAllBytesEqual(value))
        {
            memset(buffer, *(byte*)&value, sizeof(int32)* length);
        }
        else
        {
            for (uint32 i = 0; i < length; i++)
            {LOGMEIN("JavascriptArray.cpp] 4135\n");
                buffer[i] = value;
            }
        }
    }

    template<>
    void JavascriptArray::CopyValueToSegmentBuferNoCheck(Field(Js::Var)* buffer, uint32 length, Js::Var value)
    {LOGMEIN("JavascriptArray.cpp] 4143\n");
        for (uint32 i = 0; i < length; i++)
        {LOGMEIN("JavascriptArray.cpp] 4145\n");
            buffer[i] = value;
        }
    }

    int32 JavascriptNativeIntArray::HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm,  ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4151\n");
        // We proceed largely in the same manner as in JavascriptArray's version of this method (see comments there for more information),
        // except when we can further optimize thanks to the knowledge that all elements in the array are int32's. This allows for two additional optimizations:
        // 1. Only tagged ints or JavascriptNumbers that can be represented as int32 can be strict equal to some element in the array (all int32). Thus, if
        // the search value is some other kind of Var, we can return -1 without ever iterating over the elements.
        // 2. If the search value is a number that can be represented as int32, then we inspect the elements, but we don't need to perform the full strict equality algorithm.
        // Instead we can use simple C++ equality (which in case of such values is equivalent to strict equality in JavaScript).

        if (!HasNoMissingValues() || fromIndex >= GetHead()->length)
        {LOGMEIN("JavascriptArray.cpp] 4160\n");
            return -1;
        }

        bool isSearchTaggedInt = TaggedInt::Is(search);
        if (!isSearchTaggedInt && !JavascriptNumber::Is_NoTaggedIntCheck(search))
        {LOGMEIN("JavascriptArray.cpp] 4166\n");
            // The value can't be in the array, but it could be in a prototype, and we can only guarantee that
            // the head segment has no gaps.
            fromIndex = toIndex > GetHead()->length ? GetHead()->length : -1;
            return -1;
        }
        int32 searchAsInt32;
        if (isSearchTaggedInt)
        {LOGMEIN("JavascriptArray.cpp] 4174\n");
            searchAsInt32 = TaggedInt::ToInt32(search);
        }
        else if (!JavascriptNumber::TryGetInt32Value<true>(JavascriptNumber::GetValue(search), &searchAsInt32))
        {LOGMEIN("JavascriptArray.cpp] 4178\n");
            // The value can't be in the array, but it could be in a prototype, and we can only guarantee that
            // the head segment has no gaps.
            fromIndex = toIndex > GetHead()->length ? GetHead()->length : -1;
            return -1;
        }

        // We need to cast head segment to SparseArraySegment<int32> to have access to GetElement (onSparseArraySegment<T>). Because there are separate overloads of this
        // virtual method on JavascriptNativeIntArray and JavascriptNativeFloatArray, we know this version of this method will only be called for true JavascriptNativeIntArray, and not for
        // the other two, so the elements of each segment used here must be int32's. Hence, the cast is safe.

        SparseArraySegment<int32> * head = static_cast<SparseArraySegment<int32>*>(GetHead());
        uint32 toIndexTrimmed = toIndex <= head->length ? toIndex : head->length;
        for (uint32 i = fromIndex; i < toIndexTrimmed; i++)
        {LOGMEIN("JavascriptArray.cpp] 4192\n");
            int32 element = head->GetElement(i);
            if (searchAsInt32 == element)
            {LOGMEIN("JavascriptArray.cpp] 4195\n");
                return i;
            }
        }

        // Element not found in the head segment. Keep looking only if the range of indices extends past
        // the head segment.
        fromIndex = toIndex > GetHead()->length ? GetHead()->length : -1;
        return -1;
    }

    int32 JavascriptNativeFloatArray::HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4207\n");
        // We proceed largely in the same manner as in JavascriptArray's version of this method (see comments there for more information),
        // except when we can further optimize thanks to the knowledge that all elements in the array are doubles. This allows for two additional optimizations:
        // 1. Only tagged ints or JavascriptNumbers can be strict equal to some element in the array (all doubles). Thus, if
        // the search value is some other kind of Var, we can return -1 without ever iterating over the elements.
        // 2. If the search value is a number, then we inspect the elements, but we don't need to perform the full strict equality algorithm.
        // Instead we can use simple C++ equality (which in case of such values is equivalent to strict equality in JavaScript).

        if (!HasNoMissingValues() || fromIndex >= GetHead()->length)
        {LOGMEIN("JavascriptArray.cpp] 4216\n");
            return -1;
        }

        bool isSearchTaggedInt = TaggedInt::Is(search);
        if (!isSearchTaggedInt && !JavascriptNumber::Is_NoTaggedIntCheck(search))
        {LOGMEIN("JavascriptArray.cpp] 4222\n");
            // The value can't be in the array, but it could be in a prototype, and we can only guarantee that
            // the head segment has no gaps.
            fromIndex = toIndex > GetHead()->length ? GetHead()->length : -1;
            return -1;
        }

        double searchAsDouble = isSearchTaggedInt ? TaggedInt::ToDouble(search) : JavascriptNumber::GetValue(search);

        // We need to cast head segment to SparseArraySegment<double> to have access to GetElement (SparseArraySegment). We know the
        // segment's elements are all Vars so the cast is safe. It would have been more convenient here if JavascriptArray
        // used SparseArraySegment<Var>, instead of SparseArraySegmentBase.


        SparseArraySegment<double> * head = static_cast<SparseArraySegment<double>*>(GetHead());
        uint32 toIndexTrimmed = toIndex <= head->length ? toIndex : head->length;

        bool matchNaN = includesAlgorithm && JavascriptNumber::IsNan(searchAsDouble);

        for (uint32 i = fromIndex; i < toIndexTrimmed; i++)
        {LOGMEIN("JavascriptArray.cpp] 4242\n");
            double element = head->GetElement(i);

            if (element == searchAsDouble)
            {LOGMEIN("JavascriptArray.cpp] 4246\n");
                return i;
            }

            //NaN != NaN we expect to match for NaN in Array.prototype.includes algorithm
            if (matchNaN && JavascriptNumber::IsNan(element))
            {LOGMEIN("JavascriptArray.cpp] 4252\n");
                return i;
            }

        }

        fromIndex = toIndex > GetHead()->length ? GetHead()->length : -1;
        return -1;
    }

    Var JavascriptArray::EntryJoin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 4272\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.join"));
        }

        JavascriptString* separator;
        if (args.Info.Count >= 2)
        {LOGMEIN("JavascriptArray.cpp] 4278\n");
            TypeId typeId = JavascriptOperators::GetTypeId(args[1]);
            //ES5 15.4.4.5 If separator is undefined, let separator be the single-character String ",".
            if (TypeIds_Undefined != typeId)
            {LOGMEIN("JavascriptArray.cpp] 4282\n");
                separator = JavascriptConversion::ToString(args[1], scriptContext);
            }
            else
            {
                separator = scriptContext->GetLibrary()->GetCommaDisplayString();
            }
        }
        else
        {
            separator = scriptContext->GetLibrary()->GetCommaDisplayString();
        }

        return JoinHelper(args[0], separator, scriptContext);
    }

    JavascriptString* JavascriptArray::JoinToString(Var value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4299\n");
        TypeId typeId = JavascriptOperators::GetTypeId(value);
        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {LOGMEIN("JavascriptArray.cpp] 4302\n");
            return scriptContext->GetLibrary()->GetEmptyString();
        }
        else
        {
            return JavascriptConversion::ToString(value, scriptContext);
        }
    }

    JavascriptString* JavascriptArray::JoinHelper(Var thisArg, JavascriptString* separator, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4312\n");
        bool isArray = JavascriptArray::Is(thisArg) && (scriptContext == JavascriptArray::FromVar(thisArg)->GetScriptContext());
        bool isProxy = JavascriptProxy::Is(thisArg) && (scriptContext == JavascriptProxy::FromVar(thisArg)->GetScriptContext());
        Var target = NULL;
        bool isTargetObjectPushed = false;
        // if we are visiting a proxy object, track that we have visited the target object as well so the next time w
        // call the join helper for the target of this proxy, we will return above.
        if (isProxy)
        {LOGMEIN("JavascriptArray.cpp] 4320\n");
            JavascriptProxy* proxy = JavascriptProxy::FromVar(thisArg);
            Assert(proxy);
            target = proxy->GetTarget();
            if (target != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 4325\n");
                // If we end up joining same array, instead of going in infinite loop, return the empty string
                if (scriptContext->CheckObject(target))
                {LOGMEIN("JavascriptArray.cpp] 4328\n");
                    return scriptContext->GetLibrary()->GetEmptyString();
                }
                else
                {
                    scriptContext->PushObject(target);
                    isTargetObjectPushed = true;
                }
            }
        }
        // If we end up joining same array, instead of going in infinite loop, return the empty string
        else if (scriptContext->CheckObject(thisArg))
        {LOGMEIN("JavascriptArray.cpp] 4340\n");
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        if (!isTargetObjectPushed)
        {LOGMEIN("JavascriptArray.cpp] 4345\n");
            scriptContext->PushObject(thisArg);
        }

        JavascriptString* res = nullptr;

        TryFinally([&]()
        {
            if (isArray)
            {LOGMEIN("JavascriptArray.cpp] 4354\n");
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray(thisArg);
#endif
                JavascriptArray * arr = JavascriptArray::FromVar(thisArg);
                switch (arr->GetTypeId())
                {LOGMEIN("JavascriptArray.cpp] 4360\n");
                case Js::TypeIds_Array:
                    res = JoinArrayHelper(arr, separator, scriptContext);
                    break;
                case Js::TypeIds_NativeIntArray:
                    res = JoinArrayHelper(JavascriptNativeIntArray::FromVar(arr), separator, scriptContext);
                    break;
                case Js::TypeIds_NativeFloatArray:
                    res = JoinArrayHelper(JavascriptNativeFloatArray::FromVar(arr), separator, scriptContext);
                    break;
                }

            }
            else if (RecyclableObject::Is(thisArg))
            {LOGMEIN("JavascriptArray.cpp] 4374\n");
                res = JoinOtherHelper(RecyclableObject::FromVar(thisArg), separator, scriptContext);
            }
            else
            {
                res = JoinOtherHelper(scriptContext->GetLibrary()->CreateNumberObject(thisArg), separator, scriptContext);
            }
        },
        [&](bool/*hasException*/)
        {LOGMEIN("JavascriptArray.cpp] 4383\n");
            Var top = scriptContext->PopObject();
            if (isProxy)
            {LOGMEIN("JavascriptArray.cpp] 4386\n");
                AssertMsg(top == target, "Unmatched operation stack");
            }
            else
            {
                AssertMsg(top == thisArg, "Unmatched operation stack");
            }
        });

        if (res == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 4396\n");
            res = scriptContext->GetLibrary()->GetEmptyString();
        }

        return res;
    }

    static const charcount_t Join_MaxEstimatedAppendCount = static_cast<charcount_t>((64 << 20) / sizeof(void *)); // 64 MB worth of pointers

    template <typename T>
    JavascriptString* JavascriptArray::JoinArrayHelper(T * arr, JavascriptString* separator, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4407\n");
        Assert(VirtualTableInfo<T>::HasVirtualTable(arr) || VirtualTableInfo<CrossSiteObject<T>>::HasVirtualTable(arr));
        const uint32 arrLength = arr->length;
        switch(arrLength)
        {LOGMEIN("JavascriptArray.cpp] 4411\n");
            default:
            {
CaseDefault:
                bool hasSeparator = (separator->GetLength() != 0);
                const charcount_t estimatedAppendCount =
                    min(
                        Join_MaxEstimatedAppendCount,
                        static_cast<charcount_t>(arrLength + (hasSeparator ? arrLength - 1 : 0)));
                CompoundString *const cs =
                    CompoundString::NewWithPointerCapacity(estimatedAppendCount, scriptContext->GetLibrary());
                Var item;
                if (TemplatedGetItem(arr, 0u, &item, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4424\n");
                    cs->Append(JavascriptArray::JoinToString(item, scriptContext));
                }
                for (uint32 i = 1; i < arrLength; i++)
                {LOGMEIN("JavascriptArray.cpp] 4428\n");
                    if (hasSeparator)
                    {LOGMEIN("JavascriptArray.cpp] 4430\n");
                        cs->Append(separator);
                    }

                    if (TryTemplatedGetItem(arr, i, &item, scriptContext))
                    {LOGMEIN("JavascriptArray.cpp] 4435\n");
                        cs->Append(JavascriptArray::JoinToString(item, scriptContext));
                    }
                }
                return cs;
            }

            case 2:
            {LOGMEIN("JavascriptArray.cpp] 4443\n");
                bool hasSeparator = (separator->GetLength() != 0);
                if(hasSeparator)
                {LOGMEIN("JavascriptArray.cpp] 4446\n");
                    goto CaseDefault;
                }

                JavascriptString *res = nullptr;
                Var item;

                if (TemplatedGetItem(arr, 0u, &item, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4454\n");
                    res = JavascriptArray::JoinToString(item, scriptContext);
                }

                if (TryTemplatedGetItem(arr, 1u, &item, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4459\n");
                    JavascriptString *const itemString = JavascriptArray::JoinToString(item, scriptContext);
                    return res ? ConcatString::New(res, itemString) : itemString;
                }

                if(res)
                {LOGMEIN("JavascriptArray.cpp] 4465\n");
                    return res;
                }

                goto Case0;
            }

            case 1:
            {LOGMEIN("JavascriptArray.cpp] 4473\n");
                Var item;
                if (TemplatedGetItem(arr, 0u, &item, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4476\n");
                    return JavascriptArray::JoinToString(item, scriptContext);
                }
                // fall through
            }

            case 0:
Case0:
                return scriptContext->GetLibrary()->GetEmptyString();
        }
    }

    JavascriptString* JavascriptArray::JoinOtherHelper(RecyclableObject* object, JavascriptString* separator, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4489\n");
        // In ES6-mode, we always load the length property from the object instead of using the internal slot.
        // Even for arrays, this is now observable via proxies.
        // If source object is not an array, we fall back to this behavior anyway.
        Var lenValue = JavascriptOperators::OP_GetLength(object, scriptContext);
        int64 cSrcLength = JavascriptConversion::ToLength(lenValue, scriptContext);

        switch (cSrcLength)
        {LOGMEIN("JavascriptArray.cpp] 4497\n");
            default:
            {
CaseDefault:
                bool hasSeparator = (separator->GetLength() != 0);
                const charcount_t estimatedAppendCount =
                    min(
                        Join_MaxEstimatedAppendCount,
                        static_cast<charcount_t>(cSrcLength + (hasSeparator ? cSrcLength - 1 : 0)));
                CompoundString *const cs =
                    CompoundString::NewWithPointerCapacity(estimatedAppendCount, scriptContext->GetLibrary());
                Var value;
                if (JavascriptOperators::GetItem(object, 0u, &value, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4510\n");
                    cs->Append(JavascriptArray::JoinToString(value, scriptContext));
                }
                for (uint32 i = 1; i < cSrcLength; i++)
                {LOGMEIN("JavascriptArray.cpp] 4514\n");
                    if (hasSeparator)
                    {LOGMEIN("JavascriptArray.cpp] 4516\n");
                        cs->Append(separator);
                    }
                    if (JavascriptOperators::GetItem(object, i, &value, scriptContext))
                    {LOGMEIN("JavascriptArray.cpp] 4520\n");
                        cs->Append(JavascriptArray::JoinToString(value, scriptContext));
                    }
                }
                return cs;
            }

            case 2:
            {LOGMEIN("JavascriptArray.cpp] 4528\n");
                bool hasSeparator = (separator->GetLength() != 0);
                if(hasSeparator)
                {LOGMEIN("JavascriptArray.cpp] 4531\n");
                    goto CaseDefault;
                }

                JavascriptString *res = nullptr;
                Var value;
                if (JavascriptOperators::GetItem(object, 0u, &value, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4538\n");
                    res = JavascriptArray::JoinToString(value, scriptContext);
                }
                if (JavascriptOperators::GetItem(object, 1u, &value, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4542\n");
                    JavascriptString *const valueString = JavascriptArray::JoinToString(value, scriptContext);
                    return res ? ConcatString::New(res, valueString) : valueString;
                }
                if(res)
                {LOGMEIN("JavascriptArray.cpp] 4547\n");
                    return res;
                }
                goto Case0;
            }

            case 1:
            {LOGMEIN("JavascriptArray.cpp] 4554\n");
                Var value;
                if (JavascriptOperators::GetItem(object, 0u, &value, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4557\n");
                    return JavascriptArray::JoinToString(value, scriptContext);
                }
                // fall through
            }

            case 0:
Case0:
                return scriptContext->GetLibrary()->GetEmptyString();
        }
    }

    Var JavascriptArray::EntryLastIndexOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_lastIndexOf);

        Assert(!(callInfo.Flags & CallFlags_New));

        int64 length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 4585\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
            length = pArr->length;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 4596\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.lastIndexOf"));
            }
            Var lenValue = JavascriptOperators::OP_GetLength(obj, scriptContext);
            length = JavascriptConversion::ToLength(lenValue, scriptContext);
        }

        Var search;
        int64 fromIndex;
        if (!GetParamForLastIndexOf(length, args, search, fromIndex, scriptContext))
        {LOGMEIN("JavascriptArray.cpp] 4606\n");
            return TaggedInt::ToVarUnchecked(-1);
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of fromIndex argument may convert the array to an ES5 array.
        if (pArr && !JavascriptArray::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 4612\n");
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 4618\n");
            switch (pArr->GetTypeId())
            {LOGMEIN("JavascriptArray.cpp] 4620\n");
            case Js::TypeIds_Array:
                return LastIndexOfHelper(pArr, search, fromIndex, scriptContext);
            case Js::TypeIds_NativeIntArray:
                return LastIndexOfHelper(JavascriptNativeIntArray::FromVar(pArr), search, fromIndex, scriptContext);
            case Js::TypeIds_NativeFloatArray:
                return LastIndexOfHelper(JavascriptNativeFloatArray::FromVar(pArr), search, fromIndex, scriptContext);
            default:
                AssertMsg(FALSE, "invalid array typeid");
                return LastIndexOfHelper(pArr, search, fromIndex, scriptContext);
            }
        }

        // source object is not a JavascriptArray but source could be a TypedArray
        if (TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 4635\n");
            return LastIndexOfHelper(TypedArrayBase::FromVar(obj), search, fromIndex, scriptContext);
        }

        return LastIndexOfHelper(obj, search, fromIndex, scriptContext);
    }

    // Array.prototype.lastIndexOf as described in ES6.0 (draft 22) Section 22.1.3.14
    BOOL JavascriptArray::GetParamForLastIndexOf(int64 length, Arguments const & args, Var& search, int64& fromIndex, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4644\n");
        if (length == 0)
        {LOGMEIN("JavascriptArray.cpp] 4646\n");
            return false;
        }

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 4651\n");
            fromIndex = GetFromLastIndex(args[2], length, scriptContext);

            if (fromIndex >= length)
            {LOGMEIN("JavascriptArray.cpp] 4655\n");
                return false;
            }
            search = args[1];
        }
        else
        {
            search = args.Info.Count > 1 ? args[1] : scriptContext->GetLibrary()->GetUndefined();
            fromIndex = length - 1;
        }
        return true;
    }

    template <typename T>
    Var JavascriptArray::LastIndexOfHelper(T* pArr, Var search, int64 fromIndex, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 4670\n");
        Var element = nullptr;
        bool isSearchTaggedInt = TaggedInt::Is(search);

        // First handle the indices > 2^32
        while (fromIndex >= MaxArrayLength)
        {LOGMEIN("JavascriptArray.cpp] 4676\n");
            Var index = JavascriptNumber::ToVar(fromIndex, scriptContext);

            if (JavascriptOperators::OP_HasItem(pArr, index, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 4680\n");
                element = JavascriptOperators::OP_GetElementI(pArr, index, scriptContext);

                if (isSearchTaggedInt && TaggedInt::Is(element))
                {LOGMEIN("JavascriptArray.cpp] 4684\n");
                    if (element == search)
                    {LOGMEIN("JavascriptArray.cpp] 4686\n");
                        return index;
                    }
                    fromIndex--;
                    continue;
                }

                if (JavascriptOperators::StrictEqual(element, search, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 4694\n");
                    return index;
                }
            }

            fromIndex--;
        }

        Assert(fromIndex < MaxArrayLength);

        // fromIndex now has to be < MaxArrayLength so casting to uint32 is safe
        uint32 end = static_cast<uint32>(fromIndex);

        for (uint32 i = 0; i <= end; i++)
        {LOGMEIN("JavascriptArray.cpp] 4708\n");
            uint32 index = end - i;

            if (!TryTemplatedGetItem(pArr, index, &element, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 4712\n");
                continue;
            }

            if (isSearchTaggedInt && TaggedInt::Is(element))
            {LOGMEIN("JavascriptArray.cpp] 4717\n");
                if (element == search)
                {LOGMEIN("JavascriptArray.cpp] 4719\n");
                    return JavascriptNumber::ToVar(index, scriptContext);
                }
                continue;
            }

            if (JavascriptOperators::StrictEqual(element, search, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 4726\n");
                return JavascriptNumber::ToVar(index, scriptContext);
            }
        }

        return TaggedInt::ToVarUnchecked(-1);
    }

    /*
    *   PopWithNoDst
    *   - For pop calls that do not return a value, we only need to decrement the length of the array.
    */
    void JavascriptNativeArray::PopWithNoDst(Var nativeArray)
    {LOGMEIN("JavascriptArray.cpp] 4739\n");
        Assert(JavascriptNativeArray::Is(nativeArray));
        JavascriptArray * arr = JavascriptArray::FromVar(nativeArray);

        // we will bailout on length 0
        Assert(arr->GetLength() != 0);

        uint32 index = arr->GetLength() - 1;
        arr->SetLength(index);
    }

    /*
    *   JavascriptNativeIntArray::Pop
    *   -   Returns int32 value from the array.
    *   -   Returns missing item when the element is not available in the array object.
    *   -   It doesn't walk up the prototype chain.
    *   -   Length is decremented only if it pops an int32 element, in all other cases - we bail out from the jitted code.
    *   -   This api cannot cause any implicit call and hence do not need implicit call bailout test around this api
    */
    int32 JavascriptNativeIntArray::Pop(ScriptContext * scriptContext, Var object)
    {LOGMEIN("JavascriptArray.cpp] 4759\n");
        Assert(JavascriptNativeIntArray::Is(object));
        JavascriptNativeIntArray * arr = JavascriptNativeIntArray::FromVar(object);

        Assert(arr->GetLength() != 0);

        uint32 index = arr->length - 1;

        int32 element = Js::JavascriptOperators::OP_GetNativeIntElementI_UInt32(object, index, scriptContext);

        //If it is a missing item, then don't update the length - Pre-op Bail out will happen.
        if(!SparseArraySegment<int32>::IsMissingItem(&element))
        {LOGMEIN("JavascriptArray.cpp] 4771\n");
            arr->SetLength(index);
        }
        return element;
    }

    /*
    *   JavascriptNativeFloatArray::Pop
    *   -   Returns double value from the array.
    *   -   Returns missing item when the element is not available in the array object.
    *   -   It doesn't walk up the prototype chain.
    *   -   Length is decremented only if it pops a double element, in all other cases - we bail out from the jitted code.
    *   -   This api cannot cause any implicit call and hence do not need implicit call bailout test around this api
    */
    double JavascriptNativeFloatArray::Pop(ScriptContext * scriptContext, Var object)
    {LOGMEIN("JavascriptArray.cpp] 4786\n");
        Assert(JavascriptNativeFloatArray::Is(object));
        JavascriptNativeFloatArray * arr = JavascriptNativeFloatArray::FromVar(object);

        Assert(arr->GetLength() != 0);

        uint32 index = arr->length - 1;

        double element = Js::JavascriptOperators::OP_GetNativeFloatElementI_UInt32(object, index, scriptContext);

        // If it is a missing item then don't update the length - Pre-op Bail out will happen.
        if(!SparseArraySegment<double>::IsMissingItem(&element))
        {LOGMEIN("JavascriptArray.cpp] 4798\n");
            arr->SetLength(index);
        }
        return element;
    }

    /*
    *   JavascriptArray::Pop
    *   -   Calls the generic Pop API, which can find elements from the prototype chain, when it is not available in the array object.
    *   -   This API may cause implicit calls. Handles Array and non-array objects
    */
    Var JavascriptArray::Pop(ScriptContext * scriptContext, Var object)
    {LOGMEIN("JavascriptArray.cpp] 4810\n");
        if (JavascriptArray::Is(object))
        {LOGMEIN("JavascriptArray.cpp] 4812\n");
            return EntryPopJavascriptArray(scriptContext, object);
        }
        else
        {
            return EntryPopNonJavascriptArray(scriptContext, object);
        }
    }

    Var JavascriptArray::EntryPopJavascriptArray(ScriptContext * scriptContext, Var object)
    {LOGMEIN("JavascriptArray.cpp] 4822\n");
        JavascriptArray * arr = JavascriptArray::FromVar(object);
        uint32 length = arr->length;

        if (length == 0)
        {LOGMEIN("JavascriptArray.cpp] 4827\n");
            // If length is 0, return 'undefined'
            return scriptContext->GetLibrary()->GetUndefined();
        }

        uint32 index = length - 1;
        Var element;

        if (!arr->DirectGetItemAtFull(index, &element))
        {LOGMEIN("JavascriptArray.cpp] 4836\n");
            element = scriptContext->GetLibrary()->GetUndefined();
        }
        else
        {
            element = CrossSite::MarshalVar(scriptContext, element);
        }
        arr->SetLength(index); // SetLength will clear element at index

#ifdef VALIDATE_ARRAY
        arr->ValidateArray();
#endif
        return element;
    }

    Var JavascriptArray::EntryPopNonJavascriptArray(ScriptContext * scriptContext, Var object)
    {LOGMEIN("JavascriptArray.cpp] 4852\n");
        RecyclableObject* dynamicObject = nullptr;
        if (FALSE == JavascriptConversion::ToObject(object, scriptContext, &dynamicObject))
        {LOGMEIN("JavascriptArray.cpp] 4855\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.pop"));
        }
        BigIndex length;
        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 4860\n");
            length = (uint64)JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }
        else
        {
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }


        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.pop"));
        if (length == 0u)
        {LOGMEIN("JavascriptArray.cpp] 4871\n");
            // Set length = 0
            h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, TaggedInt::ToVarUnchecked(0), scriptContext, PropertyOperation_ThrowIfNotExtensible));
            return scriptContext->GetLibrary()->GetUndefined();
        }
        BigIndex index = length;
        --index;
        Var element;
        if (index.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 4880\n");
            if (!JavascriptOperators::GetItem(dynamicObject, index.GetSmallIndex(), &element, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 4882\n");
                element = scriptContext->GetLibrary()->GetUndefined();
            }
            h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, index.GetSmallIndex(), PropertyOperation_ThrowOnDeleteIfNotConfig));

            // Set the new length
            h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, JavascriptNumber::ToVar(index.GetSmallIndex(), scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
        }
        else
        {
            if (!JavascriptOperators::GetItem(dynamicObject, index.GetBigIndex(), &element, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 4893\n");
                element = scriptContext->GetLibrary()->GetUndefined();
            }
            h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, index.GetBigIndex(), PropertyOperation_ThrowOnDeleteIfNotConfig));

            // Set the new length
            h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, JavascriptNumber::ToVar(index.GetBigIndex(), scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
        }
        return element;
    }

    Var JavascriptArray::EntryPop(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 4914\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.pop"));
        }

        if (JavascriptArray::Is(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 4919\n");
            return EntryPopJavascriptArray(scriptContext, args.Values[0]);
        }
        else
        {
            return EntryPopNonJavascriptArray(scriptContext, args.Values[0]);
        }
    }

    /*
    *   JavascriptNativeIntArray::Push
    *   Pushes Int element in a native Int Array.
    *   We call the generic Push, if the array is not native Int or we have a really big array.
    */
    Var JavascriptNativeIntArray::Push(ScriptContext * scriptContext, Var array, int value)
    {LOGMEIN("JavascriptArray.cpp] 4934\n");
        // Handle non crossSite native int arrays here length within MaxArrayLength.
        // JavascriptArray::Push will handle other cases.
        if (JavascriptNativeIntArray::IsNonCrossSite(array))
        {LOGMEIN("JavascriptArray.cpp] 4938\n");
            JavascriptNativeIntArray * nativeIntArray = JavascriptNativeIntArray::FromVar(array);
            Assert(!nativeIntArray->IsCrossSiteObject());
            uint32 n = nativeIntArray->length;

            if(n < JavascriptArray::MaxArrayLength)
            {LOGMEIN("JavascriptArray.cpp] 4944\n");
                nativeIntArray->SetItem(n, value);

                n++;

                AssertMsg(n == nativeIntArray->length, "Wrong update to the length of the native Int array");

                return JavascriptNumber::ToVar(n, scriptContext);
            }
        }
        return JavascriptArray::Push(scriptContext, array, JavascriptNumber::ToVar(value, scriptContext));
    }

    /*
    *   JavascriptNativeFloatArray::Push
    *   Pushes Float element in a native Int Array.
    *   We call the generic Push, if the array is not native Float or we have a really big array.
    */
    Var JavascriptNativeFloatArray::Push(ScriptContext * scriptContext, Var * array, double value)
    {LOGMEIN("JavascriptArray.cpp] 4963\n");
        // Handle non crossSite native int arrays here length within MaxArrayLength.
        // JavascriptArray::Push will handle other cases.
        if(JavascriptNativeFloatArray::IsNonCrossSite(array))
        {LOGMEIN("JavascriptArray.cpp] 4967\n");
            JavascriptNativeFloatArray * nativeFloatArray = JavascriptNativeFloatArray::FromVar(array);
            Assert(!nativeFloatArray->IsCrossSiteObject());
            uint32 n = nativeFloatArray->length;

            if(n < JavascriptArray::MaxArrayLength)
            {LOGMEIN("JavascriptArray.cpp] 4973\n");
                nativeFloatArray->SetItem(n, value);

                n++;

                AssertMsg(n == nativeFloatArray->length, "Wrong update to the length of the native Float array");
                return JavascriptNumber::ToVar(n, scriptContext);
            }
        }

        return JavascriptArray::Push(scriptContext, array, JavascriptNumber::ToVarNoCheck(value, scriptContext));
    }

    /*
    *   JavascriptArray::Push
    *   Pushes Var element in a Var Array.
    */
    Var JavascriptArray::Push(ScriptContext * scriptContext, Var object, Var value)
    {LOGMEIN("JavascriptArray.cpp] 4991\n");
        Var args[2];
        args[0] = object;
        args[1] = value;

        if (JavascriptArray::Is(object))
        {LOGMEIN("JavascriptArray.cpp] 4997\n");
            return EntryPushJavascriptArray(scriptContext, args, 2);
        }
        else
        {
            return EntryPushNonJavascriptArray(scriptContext, args, 2);
        }

    }

    /*
    *   EntryPushNonJavascriptArray
    *   - Handles Entry push calls, when Objects are not javascript arrays
    */
    Var JavascriptArray::EntryPushNonJavascriptArray(ScriptContext * scriptContext, Var * args, uint argCount)
    {LOGMEIN("JavascriptArray.cpp] 5012\n");
            RecyclableObject* obj = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 5015\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.push"));
            }

            Var length = JavascriptOperators::OP_GetLength(obj, scriptContext);
            if(JavascriptOperators::GetTypeId(length) == TypeIds_Undefined && scriptContext->GetThreadContext()->IsDisableImplicitCall() &&
                scriptContext->GetThreadContext()->GetImplicitCallFlags() != Js::ImplicitCall_None)
            {LOGMEIN("JavascriptArray.cpp] 5022\n");
                return length;
            }

            ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.push"));
            BigIndex n;
            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {LOGMEIN("JavascriptArray.cpp] 5029\n");
                n = (uint64) JavascriptConversion::ToLength(length, scriptContext);
            }
            else
            {
                n = JavascriptConversion::ToUInt32(length, scriptContext);
            }
            // First handle "small" indices.
            uint index;
            for (index=1; index < argCount && n < JavascriptArray::MaxArrayLength; ++index, ++n)
            {LOGMEIN("JavascriptArray.cpp] 5039\n");
                if (h.IsThrowTypeError(JavascriptOperators::SetItem(obj, obj, n.GetSmallIndex(), args[index], scriptContext, PropertyOperation_ThrowIfNotExtensible)))
                {LOGMEIN("JavascriptArray.cpp] 5041\n");
                    if (scriptContext->GetThreadContext()->RecordImplicitException())
                    {LOGMEIN("JavascriptArray.cpp] 5043\n");
                        h.ThrowTypeErrorOnFailure();
                    }
                    else
                    {
                        return nullptr;
                    }
                }
            }

            // Use BigIndex if we need to push indices >= MaxArrayLength
            if (index < argCount)
            {LOGMEIN("JavascriptArray.cpp] 5055\n");
                BigIndex big = n;

                for (; index < argCount; ++index, ++big)
                {LOGMEIN("JavascriptArray.cpp] 5059\n");
                    if (h.IsThrowTypeError(big.SetItem(obj, args[index], PropertyOperation_ThrowIfNotExtensible)))
                    {LOGMEIN("JavascriptArray.cpp] 5061\n");
                        if(scriptContext->GetThreadContext()->RecordImplicitException())
                        {LOGMEIN("JavascriptArray.cpp] 5063\n");
                            h.ThrowTypeErrorOnFailure();
                        }
                        else
                        {
                            return nullptr;
                        }
                    }

                }

                // Set the new length; for objects it is all right for this to be >= MaxArrayLength
                if (h.IsThrowTypeError(JavascriptOperators::SetProperty(obj, obj, PropertyIds::length, big.ToNumber(scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible)))
                {LOGMEIN("JavascriptArray.cpp] 5076\n");
                    if(scriptContext->GetThreadContext()->RecordImplicitException())
                    {LOGMEIN("JavascriptArray.cpp] 5078\n");
                        h.ThrowTypeErrorOnFailure();
                    }
                    else
                    {
                        return nullptr;
                    }
                }

                return big.ToNumber(scriptContext);
            }
            else
            {
                // Set the new length
                Var lengthAsNUmberVar = JavascriptNumber::ToVar(n.IsSmallIndex() ? n.GetSmallIndex() : n.GetBigIndex(), scriptContext);
                if (h.IsThrowTypeError(JavascriptOperators::SetProperty(obj, obj, PropertyIds::length, lengthAsNUmberVar, scriptContext, PropertyOperation_ThrowIfNotExtensible)))
                {LOGMEIN("JavascriptArray.cpp] 5094\n");
                    if(scriptContext->GetThreadContext()->RecordImplicitException())
                    {LOGMEIN("JavascriptArray.cpp] 5096\n");
                        h.ThrowTypeErrorOnFailure();
                    }
                    else
                    {
                        return nullptr;
                    }
                }

                return lengthAsNUmberVar;
            }
    }

    /*
    *   JavascriptArray::EntryPushJavascriptArray
    *   Pushes Var element in a Var Array.
    *   Returns the length of the array.
    */
    Var JavascriptArray::EntryPushJavascriptArray(ScriptContext * scriptContext, Var * args, uint argCount)
    {LOGMEIN("JavascriptArray.cpp] 5115\n");
        JavascriptArray * arr = JavascriptArray::FromAnyArray(args[0]);
        uint n = arr->length;
        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.push"));

        // Fast Path for one push for small indexes
        if (argCount == 2 && n < JavascriptArray::MaxArrayLength)
        {LOGMEIN("JavascriptArray.cpp] 5122\n");
            // Set Item is overridden by CrossSiteObject, so no need to check for IsCrossSiteObject()
            h.ThrowTypeErrorOnFailure(arr->SetItem(n, args[1], PropertyOperation_None));
            return JavascriptNumber::ToVar(n + 1, scriptContext);
        }

        // Fast Path for multiple push for small indexes
        if (JavascriptArray::MaxArrayLength - argCount + 1 > n && JavascriptArray::IsVarArray(arr) && scriptContext == arr->GetScriptContext())
        {LOGMEIN("JavascriptArray.cpp] 5130\n");
            uint index;
            for (index = 1; index < argCount; ++index, ++n)
            {LOGMEIN("JavascriptArray.cpp] 5133\n");
                Assert(n != JavascriptArray::MaxArrayLength);
                // Set Item is overridden by CrossSiteObject, so no need to check for IsCrossSiteObject()
                arr->JavascriptArray::DirectSetItemAt(n, args[index]);
            }
            return JavascriptNumber::ToVar(n, scriptContext);
        }

        return EntryPushJavascriptArrayNoFastPath(scriptContext, args, argCount);
    }

    Var JavascriptArray::EntryPushJavascriptArrayNoFastPath(ScriptContext * scriptContext, Var * args, uint argCount)
    {LOGMEIN("JavascriptArray.cpp] 5145\n");
        JavascriptArray * arr = JavascriptArray::FromAnyArray(args[0]);
        uint n = arr->length;
        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.push"));

        // First handle "small" indices.
        uint index;
        for (index = 1; index < argCount && n < JavascriptArray::MaxArrayLength; ++index, ++n)
        {LOGMEIN("JavascriptArray.cpp] 5153\n");
            // Set Item is overridden by CrossSiteObject, so no need to check for IsCrossSiteObject()
            h.ThrowTypeErrorOnFailure(arr->SetItem(n, args[index], PropertyOperation_None));
        }

        // Use BigIndex if we need to push indices >= MaxArrayLength
        if (index < argCount)
        {LOGMEIN("JavascriptArray.cpp] 5160\n");
            // Not supporting native array with BigIndex.
            arr = EnsureNonNativeArray(arr);
            Assert(n == JavascriptArray::MaxArrayLength);
            for (BigIndex big = n; index < argCount; ++index, ++big)
            {LOGMEIN("JavascriptArray.cpp] 5165\n");
                h.ThrowTypeErrorOnFailure(big.SetItem(arr, args[index]));
            }

#ifdef VALIDATE_ARRAY
            arr->ValidateArray();
#endif
            // This is where we should set the length, but for arrays it cannot be >= MaxArrayLength
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
        }

#ifdef VALIDATE_ARRAY
        arr->ValidateArray();
#endif
        return JavascriptNumber::ToVar(n, scriptContext);
    }

    /*
    *   JavascriptArray::EntryPush
    *   Handles Push calls(Script Function)
    */
    Var JavascriptArray::EntryPush(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 5196\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.push"));
        }

        if (JavascriptArray::Is(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 5201\n");
            return EntryPushJavascriptArray(scriptContext, args.Values, args.Info.Count);
        }
        else
        {
            return EntryPushNonJavascriptArray(scriptContext, args.Values, args.Info.Count);
        }
    }


    Var JavascriptArray::EntryReverse(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();


        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 5222\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reverse"));
        }

        BigIndex length = 0u;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 5231\n");
            pArr = JavascriptArray::FromVar(args[0]);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pArr);
#endif
            obj = pArr;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 5241\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reverse"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 5247\n");
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);

        }
        else
        {
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 5257\n");
            return JavascriptArray::ReverseHelper(pArr, nullptr, obj, length.GetSmallIndex(), scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max()); // if pArr is not null lets make sure length is safe to cast, which will only happen if length is a uint32max
        return JavascriptArray::ReverseHelper(pArr, nullptr, obj, length.GetBigIndex(), scriptContext);
    }

    // Array.prototype.reverse as described in ES6.0 (draft 22) Section 22.1.3.20
    template <typename T>
    Var JavascriptArray::ReverseHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 5267\n");
        T middle = length / 2;
        Var lowerValue = nullptr, upperValue = nullptr;
        T lowerExists, upperExists;
        const char16* methodName;
        bool isTypedArrayEntryPoint = typedArrayBase != nullptr;

        if (isTypedArrayEntryPoint)
        {LOGMEIN("JavascriptArray.cpp] 5275\n");
            methodName = _u("[TypedArray].prototype.reverse");
        }
        else
        {
            methodName = _u("Array.prototype.reverse");
        }

        // If we came from Array.prototype.map and source object is not a JavascriptArray, source could be a TypedArray
        if (!isTypedArrayEntryPoint && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 5285\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        ThrowTypeErrorOnFailureHelper h(scriptContext, methodName);

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 5292\n");
            Recycler * recycler = scriptContext->GetRecycler();

            if (length <= 1)
            {LOGMEIN("JavascriptArray.cpp] 5296\n");
                return pArr;
            }

            if (pArr->IsFillFromPrototypes())
            {LOGMEIN("JavascriptArray.cpp] 5301\n");
                // For odd-length arrays, the middle element is unchanged,
                // so we cannot fill it from the prototypes.
                if (length % 2 == 0)
                {LOGMEIN("JavascriptArray.cpp] 5305\n");
                    pArr->FillFromPrototypes(0, (uint32)length);
                }
                else
                {
                    middle = length / 2;
                    pArr->FillFromPrototypes(0, (uint32)middle);
                    pArr->FillFromPrototypes(1 + (uint32)middle, (uint32)length);
                }
            }

            if (pArr->HasNoMissingValues() && pArr->head && pArr->head->next)
            {LOGMEIN("JavascriptArray.cpp] 5317\n");
                // This function currently does not track missing values in the head segment if there are multiple segments
                pArr->SetHasNoMissingValues(false);
            }

            SparseArraySegmentBase* seg = pArr->head;
            SparseArraySegmentBase *prevSeg = nullptr;
            SparseArraySegmentBase *nextSeg = nullptr;
            SparseArraySegmentBase *pinPrevSeg = nullptr;

            bool isIntArray = false;
            bool isFloatArray = false;

            if (JavascriptNativeIntArray::Is(pArr))
            {LOGMEIN("JavascriptArray.cpp] 5331\n");
                isIntArray = true;
            }
            else if (JavascriptNativeFloatArray::Is(pArr))
            {LOGMEIN("JavascriptArray.cpp] 5335\n");
                isFloatArray = true;
            }

            while (seg)
            {LOGMEIN("JavascriptArray.cpp] 5340\n");
                nextSeg = seg->next;

                // If seg.length == 0, it is possible that (seg.left + seg.length == prev.left + prev.length),
                // resulting in 2 segments sharing the same "left".
                if (seg->length > 0)
                {LOGMEIN("JavascriptArray.cpp] 5346\n");
                    if (isIntArray)
                    {LOGMEIN("JavascriptArray.cpp] 5348\n");
                        ((SparseArraySegment<int32>*)seg)->ReverseSegment(recycler);
                    }
                    else if (isFloatArray)
                    {LOGMEIN("JavascriptArray.cpp] 5352\n");
                        ((SparseArraySegment<double>*)seg)->ReverseSegment(recycler);
                    }
                    else
                    {
                        ((SparseArraySegment<Var>*)seg)->ReverseSegment(recycler);
                    }

                    seg->left = ((uint32)length) > (seg->left + seg->length) ? ((uint32)length) - (seg->left + seg->length) : 0;

                    seg->next = prevSeg;
                    // Make sure size doesn't overlap with next segment.
                    // An easy fix is to just truncate the size...
                    seg->EnsureSizeInBound();

                    // If the last segment is a leaf, then we may be losing our last scanned pointer to its previous
                    // segment. Hold onto it with pinPrevSeg until we reallocate below.
                    pinPrevSeg = prevSeg;
                    prevSeg = seg;
                }

                seg = nextSeg;
            }

            pArr->head = prevSeg;

            // Just dump the segment map on reverse
            pArr->ClearSegmentMap();

            if (isIntArray)
            {LOGMEIN("JavascriptArray.cpp] 5382\n");
                if (pArr->head && pArr->head->next && SparseArraySegmentBase::IsLeafSegment(pArr->head, recycler))
                {LOGMEIN("JavascriptArray.cpp] 5384\n");
                    pArr->ReallocNonLeafSegment(SparseArraySegment<int32>::From(pArr->head), pArr->head->next);
                }
                pArr->EnsureHeadStartsFromZero<int32>(recycler);
            }
            else if (isFloatArray)
            {LOGMEIN("JavascriptArray.cpp] 5390\n");
                if (pArr->head && pArr->head->next && SparseArraySegmentBase::IsLeafSegment(pArr->head, recycler))
                {LOGMEIN("JavascriptArray.cpp] 5392\n");
                    pArr->ReallocNonLeafSegment(SparseArraySegment<double>::From(pArr->head), pArr->head->next);
                }
                pArr->EnsureHeadStartsFromZero<double>(recycler);
            }
            else
            {
                pArr->EnsureHeadStartsFromZero<Var>(recycler);
            }

            pArr->InvalidateLastUsedSegment(); // lastUsedSegment might be 0-length and discarded above

#ifdef VALIDATE_ARRAY
            pArr->ValidateArray();
#endif
        }
        else if (typedArrayBase)
        {LOGMEIN("JavascriptArray.cpp] 5409\n");
            Assert(length <= JavascriptArray::MaxArrayLength);
            if (typedArrayBase->GetLength() == length)
            {LOGMEIN("JavascriptArray.cpp] 5412\n");
                // If typedArrayBase->length == length then we know that the TypedArray will have all items < length
                // and we won't have to check that the elements exist or not.
                for (uint32 lower = 0; lower < (uint32)middle; lower++)
                {LOGMEIN("JavascriptArray.cpp] 5416\n");
                    uint32 upper = (uint32)length - lower - 1;

                    lowerValue = typedArrayBase->DirectGetItem(lower);
                    upperValue = typedArrayBase->DirectGetItem(upper);

                    // We still have to call HasItem even though we know the TypedArray has both lower and upper because
                    // there may be a proxy handler trapping HasProperty.
                    lowerExists = typedArrayBase->HasItem(lower);
                    upperExists = typedArrayBase->HasItem(upper);

                    h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(lower, upperValue));
                    h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(upper, lowerValue));
                }
            }
            else
            {
                Assert(middle <= UINT_MAX);
                for (uint32 lower = 0; lower < (uint32)middle; lower++)
                {LOGMEIN("JavascriptArray.cpp] 5435\n");
                    uint32 upper = (uint32)length - lower - 1;

                    lowerValue = typedArrayBase->DirectGetItem(lower);
                    upperValue = typedArrayBase->DirectGetItem(upper);

                    lowerExists = typedArrayBase->HasItem(lower);
                    upperExists = typedArrayBase->HasItem(upper);

                    if (lowerExists)
                    {LOGMEIN("JavascriptArray.cpp] 5445\n");
                        if (upperExists)
                        {LOGMEIN("JavascriptArray.cpp] 5447\n");
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(lower, upperValue));
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(upper, lowerValue));
                        }
                        else
                        {
                            // This will always fail for a TypedArray if lower < length
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DeleteItem(lower, PropertyOperation_ThrowOnDeleteIfNotConfig));
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(upper, lowerValue));
                        }
                    }
                    else
                    {
                        if (upperExists)
                        {LOGMEIN("JavascriptArray.cpp] 5461\n");
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(lower, upperValue));
                            // This will always fail for a TypedArray if upper < length
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DeleteItem(upper, PropertyOperation_ThrowOnDeleteIfNotConfig));
                        }
                    }
                }
            }
        }
        else
        {
            for (T lower = 0; lower < middle; lower++)
            {LOGMEIN("JavascriptArray.cpp] 5473\n");
                T upper = length - lower - 1;

                lowerExists = JavascriptOperators::HasItem(obj, lower) &&
                              JavascriptOperators::GetItem(obj, lower, &lowerValue, scriptContext);

                upperExists = JavascriptOperators::HasItem(obj, upper) &&
                              JavascriptOperators::GetItem(obj, upper, &upperValue, scriptContext);

                if (lowerExists)
                {LOGMEIN("JavascriptArray.cpp] 5483\n");
                    if (upperExists)
                    {LOGMEIN("JavascriptArray.cpp] 5485\n");
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(obj, obj, lower, upperValue, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(obj, obj, upper, lowerValue, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                    }
                    else
                    {
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(obj, lower, PropertyOperation_ThrowOnDeleteIfNotConfig));
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(obj, obj, upper, lowerValue, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                    }
                }
                else
                {
                    if (upperExists)
                    {LOGMEIN("JavascriptArray.cpp] 5498\n");
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(obj, obj, lower, upperValue, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(obj, upper, PropertyOperation_ThrowOnDeleteIfNotConfig));
                    }
                }
            }
        }

        return obj;
    }

    template<typename T>
    void JavascriptArray::ShiftHelper(JavascriptArray* pArr, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 5511\n");
        Recycler * recycler = scriptContext->GetRecycler();

        SparseArraySegment<T>* next = SparseArraySegment<T>::From(pArr->head->next);
        while (next)
        {LOGMEIN("JavascriptArray.cpp] 5516\n");
            next->left--;
            next = SparseArraySegment<T>::From(next->next);
        }

        // head and next might overlap as the next segment left is decremented
        next = SparseArraySegment<T>::From(pArr->head->next);
        if (next && (pArr->head->size > next->left))
        {LOGMEIN("JavascriptArray.cpp] 5524\n");
            AssertMsg(pArr->head->left == 0, "Array always points to a head starting at index 0");
            AssertMsg(pArr->head->size == next->left + 1, "Shift next->left overlaps current segment by more than 1 element");

            SparseArraySegment<T> *head = SparseArraySegment<T>::From(pArr->head);
            // Merge the two adjacent segments
            if (next->length != 0)
            {LOGMEIN("JavascriptArray.cpp] 5531\n");
                uint32 offset = head->size - 1;
                // There is room for one unshifted element in head segment.
                // Hence it's enough if we grow the head segment by next->length - 1

                if (next->next)
                {LOGMEIN("JavascriptArray.cpp] 5537\n");
                    // If we have a next->next, we can't grow pass the left of that

                    // If the array had a segment map before, the next->next might just be right after next as well.
                    // So we just need to grow to the end of the next segment
                    // TODO: merge that segment too?
                    Assert(next->next->left >= head->size);
                    uint32 maxGrowSize = next->next->left - head->size;
                    if (maxGrowSize != 0)
                    {LOGMEIN("JavascriptArray.cpp] 5546\n");
                        head = head->GrowByMinMax(recycler, next->length - 1, maxGrowSize); //-1 is to account for unshift
                    }
                    else
                    {
                        // The next segment is only of length one, so we already have space in the header to copy that
                        Assert(next->length == 1);
                    }
                }
                else
                {
                    head = head->GrowByMin(recycler, next->length - 1); //-1 is to account for unshift
                }
                MoveArray(head->elements + offset, next->elements, next->length);
                head->length = offset + next->length;
                pArr->head = head;
            }
            head->next = next->next;
            pArr->InvalidateLastUsedSegment();
        }

#ifdef VALIDATE_ARRAY
            pArr->ValidateArray();
#endif
    }

    Var JavascriptArray::EntryShift(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var res = scriptContext->GetLibrary()->GetUndefined();

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 5584\n");
            return res;
        }
        if (JavascriptArray::Is(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 5588\n");
            JavascriptArray * pArr = JavascriptArray::FromVar(args[0]);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pArr);
#endif

            if (pArr->length == 0)
            {LOGMEIN("JavascriptArray.cpp] 5595\n");
                return res;
            }

            if(pArr->IsFillFromPrototypes())
            {LOGMEIN("JavascriptArray.cpp] 5600\n");
                pArr->FillFromPrototypes(0, pArr->length); // We need find all missing value from [[proto]] object
            }

            if(pArr->HasNoMissingValues() && pArr->head && pArr->head->next)
            {LOGMEIN("JavascriptArray.cpp] 5605\n");
                // This function currently does not track missing values in the head segment if there are multiple segments
                pArr->SetHasNoMissingValues(false);
            }

            pArr->length--;

            pArr->ClearSegmentMap(); // Dump segmentMap on shift (before any allocation)

            Recycler * recycler = scriptContext->GetRecycler();

            bool isIntArray = false;
            bool isFloatArray = false;

            if(JavascriptNativeIntArray::Is(pArr))
            {LOGMEIN("JavascriptArray.cpp] 5620\n");
                isIntArray = true;
            }
            else if(JavascriptNativeFloatArray::Is(pArr))
            {LOGMEIN("JavascriptArray.cpp] 5624\n");
                isFloatArray = true;
            }

            if (pArr->head->length != 0)
            {LOGMEIN("JavascriptArray.cpp] 5629\n");
                if(isIntArray)
                {LOGMEIN("JavascriptArray.cpp] 5631\n");
                    int32 nativeResult = SparseArraySegment<int32>::From(pArr->head)->GetElement(0);

                    if(SparseArraySegment<int32>::IsMissingItem(&nativeResult))
                    {LOGMEIN("JavascriptArray.cpp] 5635\n");
                        res = scriptContext->GetLibrary()->GetUndefined();
                    }
                    else
                    {
                        res = Js::JavascriptNumber::ToVar(nativeResult, scriptContext);
                    }
                    SparseArraySegment<int32>::From(pArr->head)->RemoveElement(recycler, 0);
                }
                else if (isFloatArray)
                {LOGMEIN("JavascriptArray.cpp] 5645\n");
                    double nativeResult = SparseArraySegment<double>::From(pArr->head)->GetElement(0);

                    if(SparseArraySegment<double>::IsMissingItem(&nativeResult))
                    {LOGMEIN("JavascriptArray.cpp] 5649\n");
                        res = scriptContext->GetLibrary()->GetUndefined();
                    }
                    else
                    {
                        res = Js::JavascriptNumber::ToVarNoCheck(nativeResult, scriptContext);
                    }
                    SparseArraySegment<double>::From(pArr->head)->RemoveElement(recycler, 0);
                }
                else
                {
                    res = SparseArraySegment<Var>::From(pArr->head)->GetElement(0);

                    if(SparseArraySegment<Var>::IsMissingItem(&res))
                    {LOGMEIN("JavascriptArray.cpp] 5663\n");
                        res = scriptContext->GetLibrary()->GetUndefined();
                    }
                    else
                    {
                        res = CrossSite::MarshalVar(scriptContext, res);
                    }
                    SparseArraySegment<Var>::From(pArr->head)->RemoveElement(recycler, 0);
                }
            }

            if(isIntArray)
            {
                ShiftHelper<int32>(pArr, scriptContext);
            }
            else if (isFloatArray)
            {
                ShiftHelper<double>(pArr, scriptContext);
            }
            else
            {
                ShiftHelper<Var>(pArr, scriptContext);
            }
        }
        else
        {
            RecyclableObject* dynamicObject = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
            {LOGMEIN("JavascriptArray.cpp] 5691\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.shift"));
            }

            ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.shift"));

            BigIndex length = 0u;
            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {LOGMEIN("JavascriptArray.cpp] 5699\n");
                length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
            }
            else
            {
                length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
            }

            if (length == 0u)
            {LOGMEIN("JavascriptArray.cpp] 5708\n");
                // If length is 0, return 'undefined'
                h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, TaggedInt::ToVarUnchecked(0), scriptContext, PropertyOperation_ThrowIfNotExtensible));
                return scriptContext->GetLibrary()->GetUndefined();
            }
            if (!JavascriptOperators::GetItem(dynamicObject, 0u, &res, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 5714\n");
                res = scriptContext->GetLibrary()->GetUndefined();
            }
            --length;
            uint32 lengthToUin32Max = length.IsSmallIndex() ? length.GetSmallIndex() : MaxArrayLength;
            for (uint32 i = 0u; i < lengthToUin32Max; i++)
            {LOGMEIN("JavascriptArray.cpp] 5720\n");
                if (JavascriptOperators::HasItem(dynamicObject, i + 1))
                {LOGMEIN("JavascriptArray.cpp] 5722\n");
                    Var element = JavascriptOperators::GetItem(dynamicObject, i + 1, scriptContext);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(dynamicObject, dynamicObject, i, element, scriptContext, PropertyOperation_ThrowIfNotExtensible, /*skipPrototypeCheck*/ true));
                }
                else
                {
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, i, PropertyOperation_ThrowOnDeleteIfNotConfig));
                }
            }

            for (uint64 i = MaxArrayLength; length > i; i++)
            {LOGMEIN("JavascriptArray.cpp] 5733\n");
                if (JavascriptOperators::HasItem(dynamicObject, i + 1))
                {LOGMEIN("JavascriptArray.cpp] 5735\n");
                    Var element = JavascriptOperators::GetItem(dynamicObject, i + 1, scriptContext);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(dynamicObject, dynamicObject, i, element, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                }
                else
                {
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, i, PropertyOperation_ThrowOnDeleteIfNotConfig));
                }
            }

            if (length.IsSmallIndex())
            {LOGMEIN("JavascriptArray.cpp] 5746\n");
                h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, length.GetSmallIndex(), PropertyOperation_ThrowOnDeleteIfNotConfig));
                h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, JavascriptNumber::ToVar(length.GetSmallIndex(), scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
            }
            else
            {
                h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, length.GetBigIndex(), PropertyOperation_ThrowOnDeleteIfNotConfig));
                h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, JavascriptNumber::ToVar(length.GetBigIndex(), scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
            }
        }
        return res;
    }

    Js::JavascriptArray* JavascriptArray::CreateNewArrayHelper(uint32 len, bool isIntArray, bool isFloatArray,  Js::JavascriptArray* baseArray, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 5760\n");
        if (isIntArray)
        {LOGMEIN("JavascriptArray.cpp] 5762\n");
            Js::JavascriptNativeIntArray *pnewArr = scriptContext->GetLibrary()->CreateNativeIntArray(len);
            pnewArr->EnsureHead<int32>();
#if ENABLE_PROFILE_INFO
            pnewArr->CopyArrayProfileInfo(Js::JavascriptNativeIntArray::FromVar(baseArray));
#endif

            return pnewArr;
        }
        else if (isFloatArray)
        {LOGMEIN("JavascriptArray.cpp] 5772\n");
            Js::JavascriptNativeFloatArray *pnewArr  = scriptContext->GetLibrary()->CreateNativeFloatArray(len);
            pnewArr->EnsureHead<double>();
#if ENABLE_PROFILE_INFO
            pnewArr->CopyArrayProfileInfo(Js::JavascriptNativeFloatArray::FromVar(baseArray));
#endif

            return pnewArr;
        }
        else
        {
            JavascriptArray *pnewArr = pnewArr = scriptContext->GetLibrary()->CreateArray(len);
            pnewArr->EnsureHead<Var>();
            return pnewArr;
        }
   }

    template<typename T>
    void JavascriptArray::SliceHelper(JavascriptArray* pArr,  JavascriptArray* pnewArr, uint32 start, uint32 newLen)
    {LOGMEIN("JavascriptArray.cpp] 5791\n");
        SparseArraySegment<T>* headSeg = SparseArraySegment<T>::From(pArr->head);
        SparseArraySegment<T>* pnewHeadSeg = SparseArraySegment<T>::From(pnewArr->head);

        // Fill the newly created sliced array
        CopyArray(pnewHeadSeg->elements, newLen, headSeg->elements + start, newLen);
        pnewHeadSeg->length = newLen;

        Assert(pnewHeadSeg->length <= pnewHeadSeg->size);
        // Prototype lookup for missing elements
        if (!pArr->HasNoMissingValues())
        {LOGMEIN("JavascriptArray.cpp] 5802\n");
            for (uint32 i = 0; i < newLen; i++)
            {LOGMEIN("JavascriptArray.cpp] 5804\n");
                // array type might be changed in the below call to DirectGetItemAtFull
                // need recheck array type before checking array item [i + start]
                if (pArr->IsMissingItem(i + start))
                {LOGMEIN("JavascriptArray.cpp] 5808\n");
                    Var element;
                    pnewArr->SetHasNoMissingValues(false);
                    if (pArr->DirectGetItemAtFull(i + start, &element))
                    {LOGMEIN("JavascriptArray.cpp] 5812\n");
                        pnewArr->SetItem(i, element, PropertyOperation_None);
                    }
                }
            }
        }
#ifdef DBG
        else
        {
            for (uint32 i = 0; i < newLen; i++)
            {LOGMEIN("JavascriptArray.cpp] 5822\n");
                AssertMsg(!SparseArraySegment<T>::IsMissingItem(&headSeg->elements[i+start]), "Array marked incorrectly as having missing value");
            }
        }
#endif
    }

    // If the creating profile data has changed, convert it to the type of array indicated
    // in the profile
    void JavascriptArray::GetArrayTypeAndConvert(bool* isIntArray, bool* isFloatArray)
    {LOGMEIN("JavascriptArray.cpp] 5832\n");
        if (JavascriptNativeIntArray::Is(this))
        {LOGMEIN("JavascriptArray.cpp] 5834\n");
#if ENABLE_PROFILE_INFO
            JavascriptNativeIntArray* nativeIntArray = JavascriptNativeIntArray::FromVar(this);
            ArrayCallSiteInfo* info = nativeIntArray->GetArrayCallSiteInfo();
            if(!info || info->IsNativeIntArray())
            {LOGMEIN("JavascriptArray.cpp] 5839\n");
                *isIntArray = true;
            }
            else if(info->IsNativeFloatArray())
            {LOGMEIN("JavascriptArray.cpp] 5843\n");
                JavascriptNativeIntArray::ToNativeFloatArray(nativeIntArray);
                *isFloatArray = true;
            }
            else
            {
                JavascriptNativeIntArray::ToVarArray(nativeIntArray);
            }
#else
            *isIntArray = true;
#endif
        }
        else if (JavascriptNativeFloatArray::Is(this))
        {LOGMEIN("JavascriptArray.cpp] 5856\n");
#if ENABLE_PROFILE_INFO
            JavascriptNativeFloatArray* nativeFloatArray = JavascriptNativeFloatArray::FromVar(this);
            ArrayCallSiteInfo* info = nativeFloatArray->GetArrayCallSiteInfo();

            if(info && !info->IsNativeArray())
            {LOGMEIN("JavascriptArray.cpp] 5862\n");
                JavascriptNativeFloatArray::ToVarArray(nativeFloatArray);
            }
            else
            {
                *isFloatArray = true;
            }
#else
            *isFloatArray = true;
#endif
        }
    }

    Var JavascriptArray::EntrySlice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var res = scriptContext->GetLibrary()->GetUndefined();

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 5887\n");
            return res;
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && scriptContext == JavascriptArray::FromVar(args[0])->GetScriptContext())
        {LOGMEIN("JavascriptArray.cpp] 5896\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 5903\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.slice"));
            }
        }

        Var lenValue = JavascriptOperators::OP_GetLength(obj, scriptContext);
        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 5910\n");
            length = (uint64) JavascriptConversion::ToLength(lenValue, scriptContext);
        }
        else
        {
            length = JavascriptConversion::ToUInt32(lenValue, scriptContext);
        }

        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 5919\n");
            return JavascriptArray::SliceHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max());
        return JavascriptArray::SliceHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.slice as described in ES6.0 (draft 22) Section 22.1.3.22
    template <typename T>
    Var JavascriptArray::SliceHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 5929\n");
        JavascriptLibrary* library = scriptContext->GetLibrary();
        JavascriptArray* newArr = nullptr;
        RecyclableObject* newObj = nullptr;
        bool isIntArray = false;
        bool isFloatArray = false;
        bool isTypedArrayEntryPoint = typedArrayBase != nullptr;
        bool isBuiltinArrayCtor = true;
        T startT = 0;
        T newLenT = length;
        T endT = length;

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pArr);
#endif
        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptArray.cpp] 5945\n");
            startT = GetFromIndex(args[1], length, scriptContext);

            if (startT > length)
            {LOGMEIN("JavascriptArray.cpp] 5949\n");
                startT = length;
            }

            if (args.Info.Count > 2)
            {LOGMEIN("JavascriptArray.cpp] 5954\n");
                if (JavascriptOperators::GetTypeId(args[2]) == TypeIds_Undefined)
                {LOGMEIN("JavascriptArray.cpp] 5956\n");
                    endT = length;
                }
                else
                {
                    endT = GetFromIndex(args[2], length, scriptContext);

                    if (endT > length)
                    {LOGMEIN("JavascriptArray.cpp] 5964\n");
                        endT = length;
                    }
                }
            }

            newLenT = endT > startT ? endT - startT : 0;
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of arguments start or end may convert the array to an ES5 array.
        if (pArr && !JavascriptArray::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 5975\n");
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (TypedArrayBase::IsDetachedTypedArray(obj))
        {LOGMEIN("JavascriptArray.cpp] 5981\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("Array.prototype.slice"));
        }

        // If we came from Array.prototype.slice and source object is not a JavascriptArray, source could be a TypedArray
        if (!isTypedArrayEntryPoint && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 5987\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        // If the entry point is %TypedArray%.prototype.slice or the source object is an Array exotic object we should try to load the constructor property
        // and use it to construct the return object.
        if (isTypedArrayEntryPoint)
        {LOGMEIN("JavascriptArray.cpp] 5994\n");
            Var constructor = JavascriptOperators::SpeciesConstructor(typedArrayBase, TypedArrayBase::GetDefaultConstructor(args[0], scriptContext), scriptContext);
            isBuiltinArrayCtor = (constructor == library->GetArrayConstructor());

            // If we have an array source object, we need to make sure to do the right thing if it's a native array.
            // The helpers below which do the element copying require the source and destination arrays to have the same native type.
            if (pArr && isBuiltinArrayCtor)
            {LOGMEIN("JavascriptArray.cpp] 6001\n");
                if (newLenT > JavascriptArray::MaxArrayLength)
                {LOGMEIN("JavascriptArray.cpp] 6003\n");
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
                }

                // If the constructor function is the built-in Array constructor, we can be smart and create the right type of native array.
                pArr->GetArrayTypeAndConvert(&isIntArray, &isFloatArray);
                newArr = CreateNewArrayHelper(static_cast<uint32>(newLenT), isIntArray, isFloatArray, pArr, scriptContext);
                newObj = newArr;
            }
            else if (JavascriptOperators::IsConstructor(constructor))
            {LOGMEIN("JavascriptArray.cpp] 6013\n");
                if (pArr)
                {LOGMEIN("JavascriptArray.cpp] 6015\n");
                    // If the constructor function is any other function, it can return anything so we have to call it.
                    // Roll the source array into a non-native array if it was one.
                    pArr = EnsureNonNativeArray(pArr);
                }

                Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(newLenT, scriptContext) };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = RecyclableObject::FromVar(TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), (uint32)newLenT, scriptContext));
            }
            else
            {
                // We only need to throw a TypeError when the constructor property is not an actual constructor if %TypedArray%.prototype.slice was called
                JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArray_Constructor, _u("[TypedArray].prototype.slice"));
            }
        }

        else if (pArr != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 6033\n");
            newObj = ArraySpeciesCreate(pArr, newLenT, scriptContext, &isIntArray, &isFloatArray, &isBuiltinArrayCtor);
        }

        // skip the typed array and "pure" array case, we still need to handle special arrays like es5array, remote array, and proxy of array.
        else
        {
            newObj = ArraySpeciesCreate(obj, newLenT, scriptContext, nullptr, nullptr, &isBuiltinArrayCtor);
        }

        // If we didn't create a new object above we will create a new array here.
        // This is the pre-ES6 behavior or the case of calling Array.prototype.slice with a constructor argument that is not a constructor function.
        if (newObj == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 6046\n");
            if (pArr)
            {LOGMEIN("JavascriptArray.cpp] 6048\n");
                pArr->GetArrayTypeAndConvert(&isIntArray, &isFloatArray);
            }

            if (newLenT > JavascriptArray::MaxArrayLength)
            {LOGMEIN("JavascriptArray.cpp] 6053\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
            }

            newArr = CreateNewArrayHelper(static_cast<uint32>(newLenT), isIntArray, isFloatArray, pArr, scriptContext);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newArr);
#endif
            newObj = newArr;
        }
        else
        {
            // If the new object we created is an array, remember that as it will save us time setting properties in the object below
            if (JavascriptArray::Is(newObj))
            {LOGMEIN("JavascriptArray.cpp] 6067\n");
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                newArr = JavascriptArray::FromVar(newObj);
            }
        }

        uint32 start  = (uint32) startT;
        uint32 newLen = (uint32) newLenT;

        // We at least have to have newObj as a valid object
        Assert(newObj);

        // Bail out early if the new object will have zero length.
        if (newLen == 0)
        {LOGMEIN("JavascriptArray.cpp] 6083\n");
            return newObj;
        }

        // The ArraySpeciesCreate call above could have converted the source array into an ES5Array. If this happens
        // we will process the array elements like an ES5Array.
        if (pArr && !JavascriptArray::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 6090\n");
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 6096\n");
            // If we constructed a new Array object, we have some nice helpers here
            if (newArr && isBuiltinArrayCtor)
            {LOGMEIN("JavascriptArray.cpp] 6099\n");
                if (JavascriptArray::IsDirectAccessArray(newArr))
                {LOGMEIN("JavascriptArray.cpp] 6101\n");
                    if (((start + newLen) <= pArr->head->length) && newLen <= newArr->head->size) //Fast Path
                    {LOGMEIN("JavascriptArray.cpp] 6103\n");
                        if (isIntArray)
                        {
                            SliceHelper<int32>(pArr, newArr, start, newLen);
                        }
                        else if (isFloatArray)
                        {
                            SliceHelper<double>(pArr, newArr, start, newLen);
                        }
                        else
                        {
                            SliceHelper<Var>(pArr, newArr, start, newLen);
                        }
                    }
                    else
                    {
                        if (isIntArray)
                        {LOGMEIN("JavascriptArray.cpp] 6120\n");
                            CopyNativeIntArrayElements(JavascriptNativeIntArray::FromVar(newArr), 0, JavascriptNativeIntArray::FromVar(pArr), start, start + newLen);
                        }
                        else if (isFloatArray)
                        {LOGMEIN("JavascriptArray.cpp] 6124\n");
                            CopyNativeFloatArrayElements(JavascriptNativeFloatArray::FromVar(newArr), 0, JavascriptNativeFloatArray::FromVar(pArr), start, start + newLen);
                        }
                        else
                        {
                            CopyArrayElements(newArr, 0u, pArr, start, start + newLen);
                        }
                    }
                }
                else
                {
                    AssertMsg(CONFIG_FLAG(ForceES5Array), "newArr can only be ES5Array when it is forced");
                    Var element;
                    for (uint32 i = 0; i < newLen; i++)
                    {LOGMEIN("JavascriptArray.cpp] 6138\n");
                        if (!pArr->DirectGetItemAtFull(i + start, &element))
                        {LOGMEIN("JavascriptArray.cpp] 6140\n");
                            continue;
                        }

                        newArr->SetItem(i, element, PropertyOperation_None);

                        // Side-effects in the prototype lookup may have changed the source array into an ES5Array. If this happens
                        // we will process the rest of the array elements like an ES5Array.
                        if (!JavascriptArray::Is(obj))
                        {LOGMEIN("JavascriptArray.cpp] 6149\n");
                            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                            return JavascriptArray::SliceObjectHelper(obj, start, i + 1, newArr, newObj, newLen, scriptContext);
                        }
                    }
                }
            }
            else
            {
                // The constructed object isn't an array, we'll need to use normal object manipulation
                Var element;

                for (uint32 i = 0; i < newLen; i++)
                {LOGMEIN("JavascriptArray.cpp] 6162\n");
                    if (!pArr->DirectGetItemAtFull(i + start, &element))
                    {LOGMEIN("JavascriptArray.cpp] 6164\n");
                        continue;
                    }

                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, i, element), scriptContext, i);

                    // Side-effects in the prototype lookup may have changed the source array into an ES5Array. If this happens
                    // we will process the rest of the array elements like an ES5Array.
                    if (!JavascriptArray::Is(obj))
                    {LOGMEIN("JavascriptArray.cpp] 6173\n");
                        AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                        return JavascriptArray::SliceObjectHelper(obj, start, i + 1, newArr, newObj, newLen, scriptContext);
                    }
                }
            }
        }
        else if (typedArrayBase)
        {LOGMEIN("JavascriptArray.cpp] 6181\n");
            // Source is a TypedArray, we must have created the return object via a call to constructor, but newObj may not be a TypedArray (or an array either)
            TypedArrayBase* newTypedArray = nullptr;

            if (TypedArrayBase::Is(newObj))
            {LOGMEIN("JavascriptArray.cpp] 6186\n");
                newTypedArray = TypedArrayBase::FromVar(newObj);
            }

            Var element;

            for (uint32 i = 0; i < newLen; i++)
            {LOGMEIN("JavascriptArray.cpp] 6193\n");
                // We only need to call HasItem in the case that we are called from Array.prototype.slice
                if (!isTypedArrayEntryPoint && !typedArrayBase->HasItem(i + start))
                {LOGMEIN("JavascriptArray.cpp] 6196\n");
                    continue;
                }

                element = typedArrayBase->DirectGetItem(i + start);

                // The object we got back from the constructor might not be a TypedArray. In fact, it could be any object.
                if (newTypedArray)
                {LOGMEIN("JavascriptArray.cpp] 6204\n");
                    newTypedArray->DirectSetItem(i, element);
                }
                else if (newArr)
                {LOGMEIN("JavascriptArray.cpp] 6208\n");
                    newArr->DirectSetItemAt(i, element);
                }
                else
                {
                    JavascriptOperators::OP_SetElementI_UInt32(newObj, i, element, scriptContext, PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }
        else
        {
            return JavascriptArray::SliceObjectHelper(obj, start, 0u, newArr, newObj, newLen, scriptContext);
        }

        if (!isTypedArrayEntryPoint)
        {LOGMEIN("JavascriptArray.cpp] 6223\n");
            JavascriptOperators::SetProperty(newObj, newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(newLen, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible);
        }

#ifdef VALIDATE_ARRAY
        if (JavascriptArray::Is(newObj))
        {LOGMEIN("JavascriptArray.cpp] 6229\n");
            JavascriptArray::FromVar(newObj)->ValidateArray();
        }
#endif

        return newObj;
    }

    Var JavascriptArray::SliceObjectHelper(RecyclableObject* obj, uint32 sliceStart, uint32 start, JavascriptArray* newArr, RecyclableObject* newObj, uint32 newLen, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 6238\n");
        for (uint32 i = start; i < newLen; i++)
        {LOGMEIN("JavascriptArray.cpp] 6240\n");
            if (JavascriptOperators::HasItem(obj, i + sliceStart))
            {LOGMEIN("JavascriptArray.cpp] 6242\n");
                Var element = JavascriptOperators::GetItem(obj, i + sliceStart, scriptContext);
                if (newArr != nullptr)
                {LOGMEIN("JavascriptArray.cpp] 6245\n");
                    newArr->SetItem(i, element, PropertyOperation_None);
                }
                else
                {
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, i, element), scriptContext, i);
                }
            }
        }

        JavascriptOperators::SetProperty(newObj, newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(newLen, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible);

#ifdef VALIDATE_ARRAY
        if (JavascriptArray::Is(newObj))
        {LOGMEIN("JavascriptArray.cpp] 6259\n");
            JavascriptArray::FromVar(newObj)->ValidateArray();
        }
#endif

        return newObj;
    }

    struct CompareVarsInfo
    {
        ScriptContext* scriptContext;
        RecyclableObject* compFn;
    };

    int __cdecl compareVars(void* cvInfoV, const void* aRef, const void* bRef)
    {LOGMEIN("JavascriptArray.cpp] 6274\n");
        CompareVarsInfo* cvInfo=(CompareVarsInfo*)cvInfoV;
        ScriptContext* requestContext=cvInfo->scriptContext;
        RecyclableObject* compFn=cvInfo->compFn;

        AssertMsg(*(Var*)aRef, "No null expected in sort");
        AssertMsg(*(Var*)bRef, "No null expected in sort");

        if (compFn != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 6283\n");
            ScriptContext* scriptContext = compFn->GetScriptContext();
            // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
            CallFlags flags = CallFlags_Value;
            Var undefined = scriptContext->GetLibrary()->GetUndefined();
            Var retVal;
            if (requestContext != scriptContext)
            {LOGMEIN("JavascriptArray.cpp] 6290\n");
                Var leftVar = CrossSite::MarshalVar(scriptContext, *(Var*)aRef);
                Var rightVar = CrossSite::MarshalVar(scriptContext, *(Var*)bRef);
                retVal = CALL_FUNCTION(compFn, CallInfo(flags, 3), undefined, leftVar, rightVar);
            }
            else
            {
                retVal = CALL_FUNCTION(compFn, CallInfo(flags, 3), undefined, *(Var*)aRef, *(Var*)bRef);
            }

            if (TaggedInt::Is(retVal))
            {LOGMEIN("JavascriptArray.cpp] 6301\n");
                return TaggedInt::ToInt32(retVal);
            }
            double dblResult;
            if (JavascriptNumber::Is_NoTaggedIntCheck(retVal))
            {LOGMEIN("JavascriptArray.cpp] 6306\n");
                dblResult = JavascriptNumber::GetValue(retVal);
            }
            else
            {
                dblResult = JavascriptConversion::ToNumber_Full(retVal, scriptContext);
            }
            if (dblResult < 0)
            {LOGMEIN("JavascriptArray.cpp] 6314\n");
                return -1;
            }
            return (dblResult > 0) ? 1 : 0;
        }
        else
        {
            JavascriptString* pStr1 = JavascriptConversion::ToString(*(Var*)aRef, requestContext);
            JavascriptString* pStr2 = JavascriptConversion::ToString(*(Var*)bRef, requestContext);

            return JavascriptString::strcmp(pStr1, pStr2);
        }
    }

    static void hybridSort(__inout_ecount(length) Field(Var) *elements, uint32 length, CompareVarsInfo* compareInfo)
    {LOGMEIN("JavascriptArray.cpp] 6329\n");
        // The cost of memory moves starts to be more expensive than additional comparer calls (given a simple comparer)
        // for arrays of more than 512 elements.
        if (length > 512)
        {
            qsort_s(elements, length, compareVars, compareInfo);
            return;
        }

        for (int i = 1; i < (int)length; i++)
        {
            if (compareVars(compareInfo, elements + i, elements + i - 1) < 0) {LOGMEIN("JavascriptArray.cpp] 6340\n");
                // binary search for the left-most element greater than value:
                int first = 0;
                int last = i - 1;
                while (first <= last)
                {LOGMEIN("JavascriptArray.cpp] 6345\n");
                    int middle = (first + last) / 2;
                    if (compareVars(compareInfo, elements + i, elements + middle) < 0)
                    {LOGMEIN("JavascriptArray.cpp] 6348\n");
                        last = middle - 1;
                    }
                    else
                    {
                        first = middle + 1;
                    }
                }

                // insert value right before first:
                Var value = elements[i];
                MoveArray(elements + first + 1, elements + first, (i - first));
                elements[first] = value;
            }
        }
    }

    void JavascriptArray::Sort(RecyclableObject* compFn)
    {LOGMEIN("JavascriptArray.cpp] 6366\n");
        if (length <= 1)
        {LOGMEIN("JavascriptArray.cpp] 6368\n");
            return;
        }

        this->EnsureHead<Var>();
        ScriptContext* scriptContext = this->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        CompareVarsInfo cvInfo;
        cvInfo.scriptContext = scriptContext;
        cvInfo.compFn = compFn;

        Assert(head != nullptr);

        // Just dump the segment map on sort
        ClearSegmentMap();

        uint32 countUndefined = 0;
        SparseArraySegment<Var>* startSeg = SparseArraySegment<Var>::From(head);

        // Sort may have side effects on the array. Setting a dummy head so that original array is not affected
        uint32 saveLength = length;
        // that if compare function tries to modify the array it won't AV.
        head = const_cast<SparseArraySegmentBase*>(EmptySegment);
        SetFlags(DynamicObjectFlags::None);
        this->InvalidateLastUsedSegment();
        length = 0;

        TryFinally([&]()
        {
            //The array is a continuous array if there is only one segment
            if (startSeg->next == nullptr) // Single segment fast path
            {LOGMEIN("JavascriptArray.cpp] 6400\n");
                if (compFn != nullptr)
                {LOGMEIN("JavascriptArray.cpp] 6402\n");
                    countUndefined = startSeg->RemoveUndefined(scriptContext);

#ifdef VALIDATE_ARRAY
                    ValidateSegment(startSeg);
#endif
                    hybridSort(startSeg->elements, startSeg->length, &cvInfo);
                }
                else
                {
                    countUndefined = sort(startSeg->elements, &startSeg->length, scriptContext);
                }
                head = startSeg;
            }
            else
            {
                SparseArraySegment<Var>* allElements = SparseArraySegment<Var>::AllocateSegment(recycler, 0, 0, nullptr);
                SparseArraySegment<Var>* next = startSeg;

                uint32 nextIndex = 0;
                // copy all the elements to single segment
                while (next)
                {LOGMEIN("JavascriptArray.cpp] 6424\n");
                    countUndefined += next->RemoveUndefined(scriptContext);
                    if (next->length != 0)
                    {LOGMEIN("JavascriptArray.cpp] 6427\n");
                        allElements = SparseArraySegment<Var>::CopySegment(recycler, allElements, nextIndex, next, next->left, next->length);
                    }
                    next = SparseArraySegment<Var>::From(next->next);
                    nextIndex = allElements->length;

#ifdef VALIDATE_ARRAY
                    ValidateSegment(allElements);
#endif
                }

                if (compFn != nullptr)
                {LOGMEIN("JavascriptArray.cpp] 6439\n");
                    hybridSort(allElements->elements, allElements->length, &cvInfo);
                }
                else
                {
                    sort(allElements->elements, &allElements->length, scriptContext);
                }

                head = allElements;
                head->next = nullptr;
            }
        },
        [&](bool hasException)
        {LOGMEIN("JavascriptArray.cpp] 6452\n");
            length = saveLength;
            ClearSegmentMap(); // Dump the segmentMap again in case user compare function rebuilds it
            if (hasException)
            {LOGMEIN("JavascriptArray.cpp] 6456\n");
                head = startSeg;
                this->InvalidateLastUsedSegment();
            }
        });

#if DEBUG
        {
            uint32 countNull = 0;
            uint32 index = head->length - 1;
            while (countNull < head->length)
            {LOGMEIN("JavascriptArray.cpp] 6467\n");
                if (SparseArraySegment<Var>::From(head)->elements[index] != NULL)
                {LOGMEIN("JavascriptArray.cpp] 6469\n");
                    break;
                }
                index--;
                countNull++;
            }
            AssertMsg(countNull == 0, "No null expected at the end");
        }
#endif

        if (countUndefined != 0)
        {LOGMEIN("JavascriptArray.cpp] 6480\n");
            // fill undefined at the end
            uint32 newLength = head->length + countUndefined;
            if (newLength > head->size)
            {LOGMEIN("JavascriptArray.cpp] 6484\n");
                head = SparseArraySegment<Var>::From(head)->GrowByMin(recycler, newLength - head->size);
            }

            Var undefined = scriptContext->GetLibrary()->GetUndefined();
            for (uint32 i = head->length; i < newLength; i++)
            {LOGMEIN("JavascriptArray.cpp] 6490\n");
                SparseArraySegment<Var>::From(head)->elements[i] = undefined;
            }
            head->length = newLength;
        }
        SetHasNoMissingValues();
        this->InvalidateLastUsedSegment();

#ifdef VALIDATE_ARRAY
        ValidateArray();
#endif
        return;
    }

    uint32 JavascriptArray::sort(__inout_ecount(*len) Field(Var) *orig, uint32 *len, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 6505\n");
        uint32 count = 0, countUndefined = 0;
        Element *elements = RecyclerNewArrayZ(scriptContext->GetRecycler(), Element, *len);
        RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();

        //
        // Create the Elements array
        //

        for (uint32 i = 0; i < *len; ++i)
        {LOGMEIN("JavascriptArray.cpp] 6515\n");
            if (!SparseArraySegment<Var>::IsMissingItem(&orig[i]))
            {LOGMEIN("JavascriptArray.cpp] 6517\n");
                if (!JavascriptOperators::IsUndefinedObject(orig[i], undefined))
                {LOGMEIN("JavascriptArray.cpp] 6519\n");
                    elements[count].Value = orig[i];
                    elements[count].StringValue =  JavascriptConversion::ToString(orig[i], scriptContext);

                    count++;
                }
                else
                {
                    countUndefined++;
                }
            }
        }

        if (count > 0)
        {
            SortElements(elements, 0, count - 1);

            for (uint32 i = 0; i < count; ++i)
            {LOGMEIN("JavascriptArray.cpp] 6537\n");
                orig[i] = elements[i].Value;
            }
        }

        for (uint32 i = count + countUndefined; i < *len; ++i)
        {LOGMEIN("JavascriptArray.cpp] 6543\n");
            orig[i] = SparseArraySegment<Var>::GetMissingItem();
        }

        *len = count; // set the correct length
        return countUndefined;
    }

    int __cdecl JavascriptArray::CompareElements(void* context, const void* elem1, const void* elem2)
    {LOGMEIN("JavascriptArray.cpp] 6552\n");
        const Element* element1 = static_cast<const Element*>(elem1);
        const Element* element2 = static_cast<const Element*>(elem2);

        Assert(element1 != NULL);
        Assert(element2 != NULL);

        return JavascriptString::strcmp(element1->StringValue, element2->StringValue);
    }

    void JavascriptArray::SortElements(Element* elements, uint32 left, uint32 right)
    {
        // Note: use write barrier policy of Field(Var)
        qsort_s<Element, Field(Var)>(elements, right - left + 1, CompareElements, this);
    }

    Var JavascriptArray::EntrySort(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.prototype.sort"));

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count >= 1, "Should have at least one argument");

        RecyclableObject* compFn = NULL;
        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptArray.cpp] 6582\n");
            if (JavascriptConversion::IsCallable(args[1]))
            {LOGMEIN("JavascriptArray.cpp] 6584\n");
                compFn = RecyclableObject::FromVar(args[1]);
            }
            else
            {
                TypeId typeId = JavascriptOperators::GetTypeId(args[1]);

                // Use default comparer:
                // - In ES5 mode if the argument is undefined.
                bool useDefaultComparer = typeId == TypeIds_Undefined;
                if (!useDefaultComparer)
                {LOGMEIN("JavascriptArray.cpp] 6595\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedInternalObject, _u("Array.prototype.sort"));
                }
            }
        }

        if (JavascriptArray::Is(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 6602\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif

            JavascriptArray *arr = JavascriptArray::FromVar(args[0]);

            if (arr->length <= 1)
            {LOGMEIN("JavascriptArray.cpp] 6610\n");
                return args[0];
            }

            if(arr->IsFillFromPrototypes())
            {LOGMEIN("JavascriptArray.cpp] 6615\n");
                arr->FillFromPrototypes(0, arr->length); // We need find all missing value from [[proto]] object
            }

            // Maintain nativity of the array only for the following cases (To favor inplace conversions - keeps the conversion cost less):
            // -    int cases for X86 and
            // -    FloatArray for AMD64
            // We convert the entire array back and forth once here O(n), rather than doing the costly conversion down the call stack which is O(nlogn)

#if defined(_M_X64_OR_ARM64)
            if(compFn && JavascriptNativeFloatArray::Is(arr))
            {LOGMEIN("JavascriptArray.cpp] 6626\n");
                arr = JavascriptNativeFloatArray::ConvertToVarArray((JavascriptNativeFloatArray*)arr);
                arr->Sort(compFn);
                arr = arr->ConvertToNativeArrayInPlace<JavascriptNativeFloatArray, double>(arr);
            }
            else
            {
                EnsureNonNativeArray(arr);
                arr->Sort(compFn);
            }
#else
            if(compFn && JavascriptNativeIntArray::Is(arr))
            {LOGMEIN("JavascriptArray.cpp] 6638\n");
                //EnsureNonNativeArray(arr);
                arr = JavascriptNativeIntArray::ConvertToVarArray((JavascriptNativeIntArray*)arr);
                arr->Sort(compFn);
                arr = arr->ConvertToNativeArrayInPlace<JavascriptNativeIntArray, int32>(arr);
            }
            else
            {
                EnsureNonNativeArray(arr);
                arr->Sort(compFn);
            }
#endif

        }
        else
        {
            RecyclableObject* pObj = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &pObj))
            {LOGMEIN("JavascriptArray.cpp] 6656\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.sort"));
            }
            uint32 len = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(pObj, scriptContext), scriptContext);
            JavascriptArray* sortArray = scriptContext->GetLibrary()->CreateArray(len);
            sortArray->EnsureHead<Var>();
            ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.sort"));

            BEGIN_TEMP_ALLOCATOR(tempAlloc, scriptContext, _u("Runtime"))
            {LOGMEIN("JavascriptArray.cpp] 6665\n");
                JsUtil::List<uint32, ArenaAllocator>* indexList = JsUtil::List<uint32, ArenaAllocator>::New(tempAlloc);

                for (uint32 i = 0; i < len; i++)
                {LOGMEIN("JavascriptArray.cpp] 6669\n");
                    Var item;
                    if (JavascriptOperators::GetItem(pObj, i, &item, scriptContext))
                    {LOGMEIN("JavascriptArray.cpp] 6672\n");
                        indexList->Add(i);
                        sortArray->DirectSetItemAt(i, item);
                    }
                }
                if (indexList->Count() > 0)
                {LOGMEIN("JavascriptArray.cpp] 6678\n");
                    if (sortArray->length > 1)
                    {LOGMEIN("JavascriptArray.cpp] 6680\n");
                        sortArray->FillFromPrototypes(0, sortArray->length); // We need find all missing value from [[proto]] object
                    }
                    sortArray->Sort(compFn);

                    uint32 removeIndex = sortArray->head->length;
                    for (uint32 i = 0; i < removeIndex; i++)
                    {LOGMEIN("JavascriptArray.cpp] 6687\n");
                        AssertMsg(!SparseArraySegment<Var>::IsMissingItem(&SparseArraySegment<Var>::From(sortArray->head)->elements[i]), "No gaps expected in sorted array");
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(pObj, pObj, i, SparseArraySegment<Var>::From(sortArray->head)->elements[i], scriptContext));
                    }
                    for (int i = 0; i < indexList->Count(); i++)
                    {LOGMEIN("JavascriptArray.cpp] 6692\n");
                        uint32 value = indexList->Item(i);
                        if (value >= removeIndex)
                        {LOGMEIN("JavascriptArray.cpp] 6695\n");
                            h.ThrowTypeErrorOnFailure((JavascriptOperators::DeleteItem(pObj, value)));
                        }
                    }
                }

            }
            END_TEMP_ALLOCATOR(tempAlloc, scriptContext);
        }
        return args[0];
    }

    Var JavascriptArray::EntrySplice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Recycler *recycler = scriptContext->GetRecycler();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count >= 1, "Should have at least one argument");

        bool isArr = false;
        JavascriptArray* pArr = 0;
        RecyclableObject* pObj = 0;
        RecyclableObject* newObj = nullptr;
        uint32 start = 0;
        uint32 deleteLen = 0;
        uint32 len = 0;

        if (JavascriptArray::Is(args[0]) && scriptContext == JavascriptArray::FromVar(args[0])->GetScriptContext())
        {LOGMEIN("JavascriptArray.cpp] 6728\n");
            isArr = true;
            pArr = JavascriptArray::FromVar(args[0]);
            pObj = pArr;
            len = pArr->length;

#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
        }
        else
        {
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &pObj))
            {LOGMEIN("JavascriptArray.cpp] 6741\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.splice"));
            }

            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {LOGMEIN("JavascriptArray.cpp] 6746\n");
                int64 len64 = JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(pObj, scriptContext), scriptContext);
                len = len64 > UINT_MAX ? UINT_MAX : (uint)len64;
            }
            else
            {
                len = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(pObj, scriptContext), scriptContext);
            }
        }

        switch (args.Info.Count)
        {LOGMEIN("JavascriptArray.cpp] 6757\n");
        case 1:
            start = len;
            deleteLen = 0;
            break;

        case 2:
            start = min(GetFromIndex(args[1], len, scriptContext), len);
            deleteLen = len - start;
            break;

        default:
            start = GetFromIndex(args[1], len, scriptContext);

            if (start > len)
            {LOGMEIN("JavascriptArray.cpp] 6772\n");
                start = len;
            }

            // When start >= len, we know we won't be deleting any items and don't really need to evaluate the second argument.
            // However, ECMA 262 15.4.4.12 requires that it be evaluated, anyway.  If the argument is an object with a valueOf
            // with a side effect, this evaluation is observable.  Hence, we must evaluate.
            if (TaggedInt::Is(args[2]))
            {LOGMEIN("JavascriptArray.cpp] 6780\n");
                int intDeleteLen = TaggedInt::ToInt32(args[2]);
                if (intDeleteLen < 0)
                {LOGMEIN("JavascriptArray.cpp] 6783\n");
                    deleteLen = 0;
                }
                else
                {
                    deleteLen = intDeleteLen;
                }
            }
            else
            {
                double dblDeleteLen = JavascriptConversion::ToInteger(args[2], scriptContext);

                if (dblDeleteLen > len)
                {LOGMEIN("JavascriptArray.cpp] 6796\n");
                    deleteLen = (uint32)-1;
                }
                else if (dblDeleteLen <= 0)
                {LOGMEIN("JavascriptArray.cpp] 6800\n");
                    deleteLen = 0;
                }
                else
                {
                    deleteLen = (uint32)dblDeleteLen;
                }
            }
            deleteLen = min(len - start, deleteLen);
            break;
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of arguments start or deleteCount may convert the array to an ES5 array.
        if (isArr && !JavascriptArray::Is(pObj))
        {LOGMEIN("JavascriptArray.cpp] 6814\n");
            AssertOrFailFastMsg(ES5Array::Is(pObj), "The array should have been converted to an ES5Array");
            isArr = false;
        }

        Var* insertArgs = args.Info.Count > 3 ? &args.Values[3] : nullptr;
        uint32 insertLen = args.Info.Count > 3 ? args.Info.Count - 3 : 0;

        ::Math::RecordOverflowPolicy newLenOverflow;
        uint32 newLen = UInt32Math::Add(len - deleteLen, insertLen, newLenOverflow); // new length of the array after splice

        if (isArr)
        {LOGMEIN("JavascriptArray.cpp] 6826\n");
            // If we have missing values then convert to not native array for now
            // In future, we could support this scenario.
            if (deleteLen == insertLen)
            {LOGMEIN("JavascriptArray.cpp] 6830\n");
                pArr->FillFromPrototypes(start, start + deleteLen);
            }
            else if (len)
            {LOGMEIN("JavascriptArray.cpp] 6834\n");
                pArr->FillFromPrototypes(start, len);
            }

            //
            // If newLen overflowed, pre-process to prevent pushing sparse array segments or elements out of
            // max array length, which would result in tons of index overflow and difficult to fix.
            //
            if (newLenOverflow.HasOverflowed())
            {LOGMEIN("JavascriptArray.cpp] 6843\n");
                pArr = EnsureNonNativeArray(pArr);
                BigIndex dstIndex = MaxArrayLength;

                uint32 maxInsertLen = MaxArrayLength - start;
                if (insertLen > maxInsertLen)
                {LOGMEIN("JavascriptArray.cpp] 6849\n");
                    // Copy overflowing insertArgs to properties
                    for (uint32 i = maxInsertLen; i < insertLen; i++)
                    {LOGMEIN("JavascriptArray.cpp] 6852\n");
                        pArr->DirectSetItemAt(dstIndex, insertArgs[i]);
                        ++dstIndex;
                    }

                    insertLen = maxInsertLen; // update

                    // Truncate elements on the right to properties
                    if (start + deleteLen < len)
                    {LOGMEIN("JavascriptArray.cpp] 6861\n");
                        pArr->TruncateToProperties(dstIndex, start + deleteLen);
                    }
                }
                else
                {
                    // Truncate would-overflow elements to properties
                    pArr->TruncateToProperties(dstIndex, MaxArrayLength - insertLen + deleteLen);
                }

                len = pArr->length; // update
                newLen = len - deleteLen + insertLen;
                Assert(newLen == MaxArrayLength);
            }

            if (insertArgs)
            {LOGMEIN("JavascriptArray.cpp] 6877\n");
                pArr = EnsureNonNativeArray(pArr);
            }

            bool isIntArray = false;
            bool isFloatArray = false;
            bool isBuiltinArrayCtor = true;
            JavascriptArray *newArr = nullptr;

            // Just dump the segment map on splice (before any possible allocation and throw)
            pArr->ClearSegmentMap();

            // If the source object is an Array exotic object (Array.isArray) we should try to load the constructor property
            // and use it to construct the return object.
            newObj = ArraySpeciesCreate(pArr, deleteLen, scriptContext, nullptr, nullptr, &isBuiltinArrayCtor);
            if (newObj != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 6893\n");
                pArr = EnsureNonNativeArray(pArr);
                // If the new object we created is an array, remember that as it will save us time setting properties in the object below
                if (JavascriptArray::Is(newObj))
                {LOGMEIN("JavascriptArray.cpp] 6897\n");
#if ENABLE_COPYONACCESS_ARRAY
                    JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                    newArr = JavascriptArray::FromVar(newObj);
                }
            }
            else
            // This is the ES5 case, pArr['constructor'] doesn't exist, or pArr['constructor'] is the builtin Array constructor
            {
                pArr->GetArrayTypeAndConvert(&isIntArray, &isFloatArray);
                newArr = CreateNewArrayHelper(deleteLen, isIntArray, isFloatArray, pArr, scriptContext);
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newArr);
#endif
            }

            // If return object is a JavascriptArray, we can use all the array splice helpers
            if (newArr && isBuiltinArrayCtor && len == pArr->length)
            {LOGMEIN("JavascriptArray.cpp] 6916\n");

                // Array has a single segment (need not start at 0) and splice start lies in the range
                // of that segment we optimize splice - Fast path.
                if (pArr->IsSingleSegmentArray() && pArr->head->HasIndex(start))
                {LOGMEIN("JavascriptArray.cpp] 6921\n");
                    if (isIntArray)
                    {
                        ArraySegmentSpliceHelper<int32>(newArr, SparseArraySegment<int32>::From(pArr->head), (SparseArraySegment<int32>**)&pArr->head, start, deleteLen, insertArgs, insertLen, recycler);
                    }
                    else if (isFloatArray)
                    {
                        ArraySegmentSpliceHelper<double>(newArr, SparseArraySegment<double>::From(pArr->head), (SparseArraySegment<double>**)&pArr->head, start, deleteLen, insertArgs, insertLen, recycler);
                    }
                    else
                    {
                        ArraySegmentSpliceHelper<Var>(newArr, SparseArraySegment<Var>::From(pArr->head), (SparseArraySegment<Var>**)&pArr->head, start, deleteLen, insertArgs, insertLen, recycler);
                    }

                    // Since the start index is within the bounds of the original array's head segment, it will not acquire any new
                    // missing values. If the original array had missing values in the head segment, some of them may have been
                    // copied into the array that will be returned; otherwise, the array that is returned will also not have any
                    // missing values.
                    newArr->SetHasNoMissingValues(pArr->HasNoMissingValues());
                }
                else
                {
                    if (isIntArray)
                    {
                        ArraySpliceHelper<int32>(newArr, pArr, start, deleteLen, insertArgs, insertLen, scriptContext);
                    }
                    else if (isFloatArray)
                    {
                        ArraySpliceHelper<double>(newArr, pArr, start, deleteLen, insertArgs, insertLen, scriptContext);
                    }
                    else
                    {
                        ArraySpliceHelper<Var>(newArr, pArr, start, deleteLen, insertArgs, insertLen, scriptContext);
                    }

                    // This function currently does not track missing values in the head segment if there are multiple segments
                    pArr->SetHasNoMissingValues(false);
                    newArr->SetHasNoMissingValues(false);
                }

                if (isIntArray)
                {LOGMEIN("JavascriptArray.cpp] 6962\n");
                    pArr->EnsureHeadStartsFromZero<int32>(recycler);
                    newArr->EnsureHeadStartsFromZero<int32>(recycler);
                }
                else if (isFloatArray)
                {LOGMEIN("JavascriptArray.cpp] 6967\n");
                    pArr->EnsureHeadStartsFromZero<double>(recycler);
                    newArr->EnsureHeadStartsFromZero<double>(recycler);
                }
                else
                {
                    pArr->EnsureHeadStartsFromZero<Var>(recycler);
                    newArr->EnsureHeadStartsFromZero<Var>(recycler);
                }

                pArr->InvalidateLastUsedSegment();

                // it is possible for valueOf accessors for the start or deleteLen
                // arguments to modify the size of the array. Since the resulting size of the array
                // is based on the cached value of length, this might lead to us having to trim
                // excess array segments at the end of the splice operation, which SetLength() will do.
                // However, this is also slower than performing the simple length assignment, so we only
                // do it if we can detect the array length changing.
                if(pArr->length != len)
                {LOGMEIN("JavascriptArray.cpp] 6986\n");
                    pArr->SetLength(newLen);
                }
                else
                {
                    pArr->length = newLen;
                }

                if (newArr->length != deleteLen)
                {LOGMEIN("JavascriptArray.cpp] 6995\n");
                    newArr->SetLength(deleteLen);
                }
                else
                {
                    newArr->length = deleteLen;
                }

                newArr->InvalidateLastUsedSegment();

#ifdef VALIDATE_ARRAY
                newArr->ValidateArray();
                pArr->ValidateArray();
#endif
                if (newLenOverflow.HasOverflowed())
                {LOGMEIN("JavascriptArray.cpp] 7010\n");
                    // ES5 15.4.4.12 16: If new len overflowed, SetLength throws
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
                }

                return newArr;
            }
        }

        if (newLenOverflow.HasOverflowed())
        {LOGMEIN("JavascriptArray.cpp] 7020\n");
            return ObjectSpliceHelper<BigIndex>(pObj, len, start, deleteLen, (Var*)insertArgs, insertLen, scriptContext, newObj);
        }
        else // Use uint32 version if no overflow
        {
            return ObjectSpliceHelper<uint32>(pObj, len, start, deleteLen, (Var*)insertArgs, insertLen, scriptContext, newObj);
        }
    }

    inline BOOL JavascriptArray::IsSingleSegmentArray() const
    {LOGMEIN("JavascriptArray.cpp] 7030\n");
        return nullptr == head->next;
    }

    template<typename T>
    void JavascriptArray::ArraySegmentSpliceHelper(JavascriptArray *pnewArr, SparseArraySegment<T> *seg, SparseArraySegment<T> **prev,
                                                    uint32 start, uint32 deleteLen, Var* insertArgs, uint32 insertLen, Recycler *recycler)
    {LOGMEIN("JavascriptArray.cpp] 7037\n");
        // book keeping variables
        uint32 relativeStart    = start - seg->left;  // This will be different from start when head->left is non zero -
                                                      //(Missing elements at the beginning)

        uint32 headDeleteLen    = min(start + deleteLen , seg->left + seg->length) - start;   // actual number of elements to delete in
                                                                                              // head if deleteLen overflows the length of head

        uint32 newHeadLen       = seg->length - headDeleteLen + insertLen;     // new length of the head after splice

        // Save the deleted elements
        if (headDeleteLen != 0)
        {LOGMEIN("JavascriptArray.cpp] 7049\n");
            pnewArr->InvalidateLastUsedSegment();
            pnewArr->head = SparseArraySegment<T>::CopySegment(recycler, SparseArraySegment<T>::From(pnewArr->head), 0, seg, start, headDeleteLen);
        }

        if (newHeadLen != 0)
        {LOGMEIN("JavascriptArray.cpp] 7055\n");
            if (seg->size < newHeadLen)
            {LOGMEIN("JavascriptArray.cpp] 7057\n");
                if (seg->next)
                {LOGMEIN("JavascriptArray.cpp] 7059\n");
                    // If we have "next", require that we haven't adjusted next segments left yet.
                    seg = seg->GrowByMinMax(recycler, newHeadLen - seg->size, seg->next->left - deleteLen + insertLen - seg->left - seg->size);
                }
                else
                {
                    seg = seg->GrowByMin(recycler, newHeadLen - seg->size);
                }
#ifdef VALIDATE_ARRAY
                ValidateSegment(seg);
#endif
            }

            // Move the elements if necessary
            if (headDeleteLen != insertLen)
            {LOGMEIN("JavascriptArray.cpp] 7074\n");
                uint32 noElementsToMove = seg->length - (relativeStart + headDeleteLen);
                MoveArray(seg->elements + relativeStart + insertLen,
                            seg->elements + relativeStart + headDeleteLen,
                            noElementsToMove);
                if (newHeadLen < seg->length) // truncate if necessary
                {LOGMEIN("JavascriptArray.cpp] 7080\n");
                    seg->Truncate(seg->left + newHeadLen); // set end elements to null so that when we introduce null elements we are safe
                }
                seg->length = newHeadLen;
            }
            // Copy the new elements
            if (insertLen > 0)
            {LOGMEIN("JavascriptArray.cpp] 7087\n");
                Assert(!VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(pnewArr) &&
                   !VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(pnewArr));

                // inserted elements starts at argument 3 of splice(start, deleteNumber, insertelem1, insertelem2, insertelem3, ...);
                CopyArray(seg->elements + relativeStart, insertLen,
                          reinterpret_cast<const T*>(insertArgs), insertLen);
            }
            *prev = seg;
        }
        else
        {
            *prev = SparseArraySegment<T>::From(seg->next);
        }
    }

    template<typename T>
    void JavascriptArray::ArraySpliceHelper(JavascriptArray* pnewArr, JavascriptArray* pArr, uint32 start, uint32 deleteLen, Var* insertArgs, uint32 insertLen, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 7105\n");
        // Skip pnewArr->EnsureHead(): we don't use existing segment at all.
        Recycler *recycler  = scriptContext->GetRecycler();

        Field(SparseArraySegmentBase*)* prevSeg  = &pArr->head;        // holds the next pointer of previous
        Field(SparseArraySegmentBase*)* prevPrevSeg  = &pArr->head;    // this holds the previous pointer to prevSeg dirty trick.
        SparseArraySegmentBase* savePrev = nullptr;

        Assert(pArr->head); // We should never have a null head.
        pArr->EnsureHead<T>();
        SparseArraySegment<T>* startSeg = SparseArraySegment<T>::From(pArr->head);

        const uint32 limit = start + deleteLen;
        uint32 rightLimit;
        if (UInt32Math::Add(startSeg->left, startSeg->size, &rightLimit))
        {LOGMEIN("JavascriptArray.cpp] 7120\n");
            rightLimit = JavascriptArray::MaxArrayLength;
        }

        // Find out the segment to start delete
        while (startSeg && (rightLimit <= start))
        {LOGMEIN("JavascriptArray.cpp] 7126\n");
            savePrev = startSeg;
            prevPrevSeg = prevSeg;
            prevSeg = &startSeg->next;
            startSeg = SparseArraySegment<T>::From(startSeg->next);

            if (startSeg)
            {LOGMEIN("JavascriptArray.cpp] 7133\n");
                if (UInt32Math::Add(startSeg->left, startSeg->size, &rightLimit))
                {LOGMEIN("JavascriptArray.cpp] 7135\n");
                    rightLimit = JavascriptArray::MaxArrayLength;
                }
            }
        }

        // handle inlined segment
        SparseArraySegmentBase* inlineHeadSegment = nullptr;
        bool hasInlineSegment = false;
        // The following if else set is used to determine whether a shallow or hard copy is needed
        if (JavascriptNativeArray::Is(pArr))
        {LOGMEIN("JavascriptArray.cpp] 7146\n");
            if (JavascriptNativeFloatArray::Is(pArr))
            {LOGMEIN("JavascriptArray.cpp] 7148\n");
                inlineHeadSegment = DetermineInlineHeadSegmentPointer<JavascriptNativeFloatArray, 0, true>((JavascriptNativeFloatArray*)pArr);
            }
            else if (JavascriptNativeIntArray::Is(pArr))
            {LOGMEIN("JavascriptArray.cpp] 7152\n");
                inlineHeadSegment = DetermineInlineHeadSegmentPointer<JavascriptNativeIntArray, 0, true>((JavascriptNativeIntArray*)pArr);
            }
            Assert(inlineHeadSegment);
            hasInlineSegment = (startSeg == (SparseArraySegment<T>*)inlineHeadSegment);
        }
        else
        {
            // This will result in false positives. It is used because DetermineInlineHeadSegmentPointer
            // does not handle Arrays that change type e.g. from JavascriptNativeIntArray to JavascriptArray
            // This conversion in particular is problematic because JavascriptNativeIntArray is larger than JavascriptArray
            // so the returned head segment ptr never equals pArr->head. So we will default to using this and deal with
            // false positives. It is better than always doing a hard copy.
            hasInlineSegment = HasInlineHeadSegment(pArr->head->length);
        }

        if (startSeg)
        {LOGMEIN("JavascriptArray.cpp] 7169\n");
            // Delete Phase
            if (startSeg->left <= start && (startSeg->left + startSeg->length) >= limit)
            {LOGMEIN("JavascriptArray.cpp] 7172\n");
                // All splice happens in one segment.
                SparseArraySegmentBase *nextSeg = startSeg->next;
                // Splice the segment first, which might OOM throw but the array would be intact.
                JavascriptArray::ArraySegmentSpliceHelper(pnewArr, (SparseArraySegment<T>*)startSeg, (SparseArraySegment<T>**)prevSeg, start, deleteLen, insertArgs, insertLen, recycler);
                while (nextSeg)
                {LOGMEIN("JavascriptArray.cpp] 7178\n");
                    // adjust next segments left
                    nextSeg->left = nextSeg->left - deleteLen + insertLen;
                    if (nextSeg->next == nullptr)
                    {LOGMEIN("JavascriptArray.cpp] 7182\n");
                        nextSeg->EnsureSizeInBound();
                    }
                    nextSeg = nextSeg->next;
                }
                if (*prevSeg)
                {LOGMEIN("JavascriptArray.cpp] 7188\n");
                    (*prevSeg)->EnsureSizeInBound();
                }
                return;
            }
            else
            {
                SparseArraySegment<T>* newHeadSeg = nullptr; // pnewArr->head is null
                Field(SparseArraySegmentBase*)* prevNewHeadSeg = &pnewArr->head;

                // delete till deleteLen and reuse segments for new array if it is possible.
                // 3 steps -
                //1. delete 1st segment (which may be partial delete)
                // 2. delete next n complete segments
                // 3. delete last segment (which again may be partial delete)

                // Step (1)  -- WOOB 1116297: When left >= start, step (1) is skipped, resulting in pNewArr->head->left != 0. We need to touch up pNewArr.
                if (startSeg->left < start)
                {LOGMEIN("JavascriptArray.cpp] 7206\n");
                    if (start < startSeg->left + startSeg->length)
                    {LOGMEIN("JavascriptArray.cpp] 7208\n");
                        uint32 headDeleteLen = startSeg->left + startSeg->length - start;

                        if (startSeg->next)
                        {LOGMEIN("JavascriptArray.cpp] 7212\n");
                            // We know the new segment will have a next segment, so allocate it as non-leaf.
                            newHeadSeg = SparseArraySegment<T>::template AllocateSegmentImpl<false>(recycler, 0, headDeleteLen, headDeleteLen, nullptr);
                        }
                        else
                        {
                            newHeadSeg = SparseArraySegment<T>::AllocateSegment(recycler, 0, headDeleteLen, headDeleteLen, nullptr);
                        }
                        newHeadSeg = SparseArraySegment<T>::CopySegment(recycler, newHeadSeg, 0, startSeg, start, headDeleteLen);
                        newHeadSeg->next = nullptr;
                        *prevNewHeadSeg = newHeadSeg;
                        prevNewHeadSeg = &newHeadSeg->next;
                        startSeg->Truncate(start);
                    }
                    savePrev = startSeg;
                    prevPrevSeg = prevSeg;
                    prevSeg = &startSeg->next;
                    startSeg = SparseArraySegment<T>::From(startSeg->next);
                }

                // Step (2) first we should do a hard copy if we have an inline head Segment
                else if (hasInlineSegment && nullptr != startSeg)
                {LOGMEIN("JavascriptArray.cpp] 7234\n");
                    // start should be in between left and left + length
                    if (startSeg->left  <= start && start < startSeg->left + startSeg->length)
                    {LOGMEIN("JavascriptArray.cpp] 7237\n");
                        uint32 headDeleteLen = startSeg->left + startSeg->length - start;
                        if (startSeg->next)
                        {LOGMEIN("JavascriptArray.cpp] 7240\n");
                            // We know the new segment will have a next segment, so allocate it as non-leaf.
                            newHeadSeg = SparseArraySegment<T>::template AllocateSegmentImpl<false>(recycler, 0, headDeleteLen, headDeleteLen, nullptr);
                        }
                        else
                        {
                            newHeadSeg = SparseArraySegment<T>::AllocateSegment(recycler, 0, headDeleteLen, headDeleteLen, nullptr);
                        }
                        newHeadSeg = SparseArraySegment<T>::CopySegment(recycler, newHeadSeg, 0, startSeg, start, headDeleteLen);
                        *prevNewHeadSeg = newHeadSeg;
                        prevNewHeadSeg = &newHeadSeg->next;

                        // Remove the entire segment from the original array
                        *prevSeg = startSeg->next;
                        startSeg = SparseArraySegment<T>::From(startSeg->next);
                    }
                    // if we have an inline head segment with 0 elements, remove it
                    else if (startSeg->left == 0 && startSeg->length == 0)
                    {LOGMEIN("JavascriptArray.cpp] 7258\n");
                        Assert(startSeg->size != 0);
                        *prevSeg = startSeg->next;
                        startSeg = SparseArraySegment<T>::From(startSeg->next);
                    }
                }
                // Step (2) proper
                SparseArraySegmentBase *temp = nullptr;
                while (startSeg && (startSeg->left + startSeg->length) <= limit)
                {LOGMEIN("JavascriptArray.cpp] 7267\n");
                    temp = startSeg->next;

                    // move that entire segment to new array
                    startSeg->left = startSeg->left - start;
                    startSeg->next = nullptr;
                    *prevNewHeadSeg = startSeg;
                    prevNewHeadSeg = &startSeg->next;

                    // Remove the entire segment from the original array
                    *prevSeg = temp;
                    startSeg = (SparseArraySegment<T>*)temp;
                }

                // Step(2) above could delete the original head segment entirely, causing current head not
                // starting from 0. Then if any of the following throw, we have a corrupted array. Need
                // protection here.
                bool dummyHeadNodeInserted = false;
                if (!savePrev && (!startSeg || startSeg->left != 0))
                {LOGMEIN("JavascriptArray.cpp] 7286\n");
                    Assert(pArr->head == startSeg);
                    pArr->EnsureHeadStartsFromZero<T>(recycler);
                    Assert(pArr->head && pArr->head->next == startSeg);

                    savePrev = pArr->head;
                    prevPrevSeg = prevSeg;
                    prevSeg = &pArr->head->next;
                    dummyHeadNodeInserted = true;
                }

                // Step (3)
                if (startSeg && (startSeg->left < limit))
                {LOGMEIN("JavascriptArray.cpp] 7299\n");
                    // copy the first part of the last segment to be deleted to new array
                    uint32 headDeleteLen = start + deleteLen - startSeg->left ;

                    newHeadSeg = SparseArraySegment<T>::AllocateSegment(recycler, startSeg->left -  start, headDeleteLen, (SparseArraySegmentBase *)nullptr);
                    newHeadSeg = SparseArraySegment<T>::CopySegment(recycler, newHeadSeg, startSeg->left -  start, startSeg, startSeg->left, headDeleteLen);
                    newHeadSeg->next = nullptr;
                    *prevNewHeadSeg = newHeadSeg;
                    prevNewHeadSeg = &newHeadSeg->next;

                    // move the last segment
                    MoveArray(startSeg->elements, startSeg->elements + headDeleteLen, startSeg->length - headDeleteLen);
                    startSeg->left = startSeg->left + headDeleteLen; // We are moving the left ahead to point to the right index
                    startSeg->length = startSeg->length - headDeleteLen;
                    startSeg->Truncate(startSeg->left + startSeg->length);
                    startSeg->EnsureSizeInBound(); // Just truncated, size might exceed next.left
                }

                if (startSeg && ((startSeg->left - deleteLen + insertLen) == 0) && dummyHeadNodeInserted)
                {LOGMEIN("JavascriptArray.cpp] 7318\n");
                    Assert(start + insertLen == 0);
                    // Remove the dummy head node to preserve array consistency.
                    pArr->head = startSeg;
                    savePrev = nullptr;
                    prevSeg = &pArr->head;
                }

                while (startSeg)
                {LOGMEIN("JavascriptArray.cpp] 7327\n");
                    startSeg->left = startSeg->left - deleteLen + insertLen ;
                    if (startSeg->next == nullptr)
                    {LOGMEIN("JavascriptArray.cpp] 7330\n");
                        startSeg->EnsureSizeInBound();
                    }
                    startSeg = SparseArraySegment<T>::From(startSeg->next);
                }
            }
        }

        // The size of pnewArr head allocated in above step 1 might exceed next.left concatenated in step 2/3.
        pnewArr->head->EnsureSizeInBound();
        if (savePrev)
        {LOGMEIN("JavascriptArray.cpp] 7341\n");
            savePrev->EnsureSizeInBound();
        }

        // insert elements
        if (insertLen > 0)
        {LOGMEIN("JavascriptArray.cpp] 7347\n");
            Assert(!JavascriptNativeIntArray::Is(pArr) && !JavascriptNativeFloatArray::Is(pArr));

            // InsertPhase
            SparseArraySegment<T> *segInsert = nullptr;

            // see if we are just about the right of the previous segment
            Assert(!savePrev || savePrev->left <= start);
            if (savePrev && (start - savePrev->left < savePrev->size))
            {LOGMEIN("JavascriptArray.cpp] 7356\n");
                segInsert = (SparseArraySegment<T>*)savePrev;
                uint32 spaceLeft = segInsert->size - (start - segInsert->left);
                if(spaceLeft < insertLen)
                {LOGMEIN("JavascriptArray.cpp] 7360\n");
                    if (!segInsert->next)
                    {LOGMEIN("JavascriptArray.cpp] 7362\n");
                        segInsert = segInsert->GrowByMin(recycler, insertLen - spaceLeft);
                    }
                    else
                    {
                        segInsert = segInsert->GrowByMinMax(recycler, insertLen - spaceLeft, segInsert->next->left - segInsert->left - segInsert->size);
                    }
                }
                *prevPrevSeg = segInsert;
                segInsert->length = start + insertLen - segInsert->left;
            }
            else
            {
                segInsert = SparseArraySegment<T>::AllocateSegment(recycler, start, insertLen, *prevSeg);
                segInsert->next = *prevSeg;
                *prevSeg = segInsert;
                savePrev = segInsert;
            }

            uint32 relativeStart = start - segInsert->left;
            // inserted elements starts at argument 3 of splice(start, deleteNumber, insertelem1, insertelem2, insertelem3, ...);
            CopyArray(segInsert->elements + relativeStart, insertLen,
                      reinterpret_cast<const T*>(insertArgs), insertLen);
        }
    }

    template<typename indexT>
    RecyclableObject* JavascriptArray::ObjectSpliceHelper(RecyclableObject* pObj, uint32 len, uint32 start,
        uint32 deleteLen, Var* insertArgs, uint32 insertLen, ScriptContext *scriptContext, RecyclableObject* pNewObj)
    {LOGMEIN("JavascriptArray.cpp] 7391\n");
        JavascriptArray *pnewArr = nullptr;

        if (pNewObj == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 7395\n");
            pNewObj = ArraySpeciesCreate(pObj, deleteLen, scriptContext);
            if (pNewObj == nullptr || !JavascriptArray::Is(pNewObj))
            {LOGMEIN("JavascriptArray.cpp] 7398\n");
                pnewArr = scriptContext->GetLibrary()->CreateArray(deleteLen);
                pnewArr->EnsureHead<Var>();

                pNewObj = pnewArr;
            }
        }

        if (JavascriptArray::Is(pNewObj))
        {LOGMEIN("JavascriptArray.cpp] 7407\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pNewObj);
#endif
            pnewArr = JavascriptArray::FromVar(pNewObj);
        }

        // copy elements to delete to new array
        if (deleteLen > 0)
        {LOGMEIN("JavascriptArray.cpp] 7416\n");
            for (uint32 i = 0; i < deleteLen; i++)
            {LOGMEIN("JavascriptArray.cpp] 7418\n");
               if (JavascriptOperators::HasItem(pObj, start+i))
               {LOGMEIN("JavascriptArray.cpp] 7420\n");
                   Var element = JavascriptOperators::GetItem(pObj, start + i, scriptContext);
                   if (pnewArr)
                   {LOGMEIN("JavascriptArray.cpp] 7423\n");
                       pnewArr->SetItem(i, element, PropertyOperation_None);
                   }
                   else
                   {
                       ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(pNewObj, i, element), scriptContext, i);
                   }
               }
            }
        }

        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.splice"));

        // If the return object is not an array, we'll need to set the 'length' property
        if (pnewArr == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 7438\n");
            h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(pNewObj, pNewObj, PropertyIds::length, JavascriptNumber::ToVar(deleteLen, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
        }

        // Now we need reserve room if it is necessary
        if (insertLen > deleteLen) // Might overflow max array length
        {
            // Unshift [start + deleteLen, len) to start + insertLen
            Unshift<indexT>(pObj, start + insertLen, start + deleteLen, len, scriptContext);
        }
        else if (insertLen < deleteLen) // Won't overflow max array length
        {LOGMEIN("JavascriptArray.cpp] 7449\n");
            uint32 j = 0;
            for (uint32 i = start + deleteLen; i < len; i++)
            {LOGMEIN("JavascriptArray.cpp] 7452\n");
                if (JavascriptOperators::HasItem(pObj, i))
                {LOGMEIN("JavascriptArray.cpp] 7454\n");
                    Var element = JavascriptOperators::GetItem(pObj, i, scriptContext);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(pObj, pObj, start + insertLen + j, element, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                }
                else
                {
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(pObj, start + insertLen + j, PropertyOperation_ThrowOnDeleteIfNotConfig));
                }
                j++;
            }

            // Clean up the rest
            for (uint32 i = len; i > len - deleteLen + insertLen; i--)
            {LOGMEIN("JavascriptArray.cpp] 7467\n");
                h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(pObj, i - 1, PropertyOperation_ThrowOnDeleteIfNotConfig));
            }
        }

        if (insertLen > 0)
        {LOGMEIN("JavascriptArray.cpp] 7473\n");
            indexT dstIndex = start; // insert index might overflow max array length
            for (uint i = 0; i < insertLen; i++)
            {LOGMEIN("JavascriptArray.cpp] 7476\n");
                h.ThrowTypeErrorOnFailure(IndexTrace<indexT>::SetItem(pObj, dstIndex, insertArgs[i], PropertyOperation_ThrowIfNotExtensible));
                ++dstIndex;
            }
        }

        // Set up new length
        indexT newLen = indexT(len - deleteLen) + insertLen;
        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(pObj, pObj, PropertyIds::length, IndexTrace<indexT>::ToNumber(newLen, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(pNewObj, pNewObj, PropertyIds::length, IndexTrace<indexT>::ToNumber(deleteLen, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
#ifdef VALIDATE_ARRAY
        if (pnewArr)
        {LOGMEIN("JavascriptArray.cpp] 7488\n");
            pnewArr->ValidateArray();
        }
#endif
        return pNewObj;
    }

    Var JavascriptArray::EntryToLocaleString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 7505\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Array.prototype.toLocaleString"));
        }

        if (JavascriptArray::IsDirectAccessArray(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 7510\n");
            JavascriptArray* arr = JavascriptArray::FromVar(args[0]);
            return ToLocaleString(arr, scriptContext);
        }
        else
        {
            if (TypedArrayBase::IsDetachedTypedArray(args[0]))
            {LOGMEIN("JavascriptArray.cpp] 7517\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("Array.prototype.toLocalString"));
            }

            RecyclableObject* obj = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 7523\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.toLocaleString"));
            }
            return ToLocaleString(obj, scriptContext);
        }
    }

    //
    // Unshift object elements [start, end) to toIndex, asserting toIndex > start.
    //
    template<typename T, typename P>
    void JavascriptArray::Unshift(RecyclableObject* obj, const T& toIndex, uint32 start, P end, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 7535\n");
        typedef IndexTrace<T> index_trace;

        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.unshift"));
        if (start < end)
        {LOGMEIN("JavascriptArray.cpp] 7540\n");
            T newEnd = (end - start - 1);// newEnd - 1
            T dst = toIndex + newEnd;
            uint32 i = 0;
            if (end > UINT32_MAX)
            {LOGMEIN("JavascriptArray.cpp] 7545\n");
                uint64 i64 = end;
                for (; i64 > UINT32_MAX; i64--)
                {LOGMEIN("JavascriptArray.cpp] 7548\n");
                    if (JavascriptOperators::HasItem(obj, i64 - 1))
                    {LOGMEIN("JavascriptArray.cpp] 7550\n");
                        Var element = JavascriptOperators::GetItem(obj, i64 - 1, scriptContext);
                        h.ThrowTypeErrorOnFailure(index_trace::SetItem(obj, dst, element, PropertyOperation_ThrowIfNotExtensible));
                    }
                    else
                    {
                        h.ThrowTypeErrorOnFailure(index_trace::DeleteItem(obj, dst, PropertyOperation_ThrowOnDeleteIfNotConfig));
                    }

                    --dst;
                }
                i = UINT32_MAX;
            }
            else
            {
                i = (uint32) end;
            }
            for (; i > start; i--)
            {LOGMEIN("JavascriptArray.cpp] 7568\n");
                if (JavascriptOperators::HasItem(obj, i-1))
                {LOGMEIN("JavascriptArray.cpp] 7570\n");
                    Var element = JavascriptOperators::GetItem(obj, i - 1, scriptContext);
                    h.ThrowTypeErrorOnFailure(index_trace::SetItem(obj, dst, element, PropertyOperation_ThrowIfNotExtensible));
                }
                else
                {
                    h.ThrowTypeErrorOnFailure(index_trace::DeleteItem(obj, dst, PropertyOperation_ThrowOnDeleteIfNotConfig));
                }

                --dst;
            }
        }
    }

    template<typename T>
    void JavascriptArray::GrowArrayHeadHelperForUnshift(JavascriptArray* pArr, uint32 unshiftElements, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 7586\n");
        SparseArraySegmentBase* nextToHeadSeg = pArr->head->next;
        Recycler* recycler = scriptContext->GetRecycler();

        if (nextToHeadSeg == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 7591\n");
            pArr->EnsureHead<T>();
            pArr->head = SparseArraySegment<T>::From(pArr->head)->GrowByMin(recycler, unshiftElements);
        }
        else
        {
            pArr->head = SparseArraySegment<T>::From(pArr->head)->GrowByMinMax(recycler, unshiftElements, ((nextToHeadSeg->left + unshiftElements) - pArr->head->left - pArr->head->size));
        }

    }

    template<typename T>
    void JavascriptArray::UnshiftHelper(JavascriptArray* pArr, uint32 unshiftElements, Js::Var * elements)
    {LOGMEIN("JavascriptArray.cpp] 7604\n");
        SparseArraySegment<T>* head = SparseArraySegment<T>::From(pArr->head);
        // Make enough room in the head segment to insert new elements at the front
        MoveArray(head->elements + unshiftElements, head->elements, pArr->head->length);
        uint32 oldHeadLength = head->length;
        head->length += unshiftElements;

        /* Set head segment as the last used segment */
        pArr->InvalidateLastUsedSegment();

        bool hasNoMissingValues = pArr->HasNoMissingValues();

        /* Set HasNoMissingValues to false -> Since we shifted elements right, we might have missing values after the memmove */
        if(unshiftElements > oldHeadLength)
        {LOGMEIN("JavascriptArray.cpp] 7618\n");
            pArr->SetHasNoMissingValues(false);
        }

#if ENABLE_PROFILE_INFO
        pArr->FillFromArgs(unshiftElements, 0, elements, nullptr, true/*dontCreateNewArray*/);
#else
        pArr->FillFromArgs(unshiftElements, 0, elements, true/*dontCreateNewArray*/);
#endif

        // Setting back to the old value
        pArr->SetHasNoMissingValues(hasNoMissingValues);
    }

    Var JavascriptArray::EntryUnshift(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        Var res = scriptContext->GetLibrary()->GetUndefined();

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 7644\n");
           return res;
        }
        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 7648\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
            JavascriptArray * pArr = JavascriptArray::FromVar(args[0]);

            uint32 unshiftElements = args.Info.Count - 1;

            if (unshiftElements > 0)
            {LOGMEIN("JavascriptArray.cpp] 7657\n");
                if (pArr->IsFillFromPrototypes())
                {LOGMEIN("JavascriptArray.cpp] 7659\n");
                    pArr->FillFromPrototypes(0, pArr->length); // We need find all missing value from [[proto]] object
                }

                // Pre-process: truncate overflowing elements to properties
                bool newLenOverflowed = false;
                uint32 maxLen = MaxArrayLength - unshiftElements;
                if (pArr->length > maxLen)
                {LOGMEIN("JavascriptArray.cpp] 7667\n");
                    newLenOverflowed = true;
                    // Ensure the array is non-native when overflow happens
                    EnsureNonNativeArray(pArr);
                    pArr->TruncateToProperties(MaxArrayLength, maxLen);
                    Assert(pArr->length + unshiftElements == MaxArrayLength);
                }

                pArr->ClearSegmentMap(); // Dump segmentMap on unshift (before any possible allocation and throw)

                Assert(pArr->length <= MaxArrayLength - unshiftElements);

                SparseArraySegmentBase* renumberSeg = pArr->head->next;

                bool isIntArray = false;
                bool isFloatArray = false;

                if (JavascriptNativeIntArray::Is(pArr))
                {LOGMEIN("JavascriptArray.cpp] 7685\n");
                    isIntArray = true;
                }
                else if (JavascriptNativeFloatArray::Is(pArr))
                {LOGMEIN("JavascriptArray.cpp] 7689\n");
                    isFloatArray = true;
                }

                // If we need to grow head segment and there is already a next segment, then allocate the new head segment upfront
                // If there is OOM in array allocation, then array consistency is maintained.
                if (pArr->head->size < pArr->head->length + unshiftElements)
                {LOGMEIN("JavascriptArray.cpp] 7696\n");
                    if (isIntArray)
                    {
                        GrowArrayHeadHelperForUnshift<int32>(pArr, unshiftElements, scriptContext);
                    }
                    else if (isFloatArray)
                    {
                        GrowArrayHeadHelperForUnshift<double>(pArr, unshiftElements, scriptContext);
                    }
                    else
                    {
                        GrowArrayHeadHelperForUnshift<Var>(pArr, unshiftElements, scriptContext);
                    }
                }

                while (renumberSeg)
                {LOGMEIN("JavascriptArray.cpp] 7712\n");
                    renumberSeg->left += unshiftElements;
                    if (renumberSeg->next == nullptr)
                    {LOGMEIN("JavascriptArray.cpp] 7715\n");
                        // last segment can shift its left + size beyond MaxArrayLength, so truncate if so
                        renumberSeg->EnsureSizeInBound();
                    }
                    renumberSeg = renumberSeg->next;
                }

                if (isIntArray)
                {
                    UnshiftHelper<int32>(pArr, unshiftElements, args.Values);
                }
                else if (isFloatArray)
                {
                    UnshiftHelper<double>(pArr, unshiftElements, args.Values);
                }
                else
                {
                    UnshiftHelper<Var>(pArr, unshiftElements, args.Values);
                }

                pArr->InvalidateLastUsedSegment();
                pArr->length += unshiftElements;

#ifdef VALIDATE_ARRAY
                pArr->ValidateArray();
#endif

                if (newLenOverflowed) // ES5: throw if new "length" exceeds max array length
                {LOGMEIN("JavascriptArray.cpp] 7743\n");
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
                }
            }
            res = JavascriptNumber::ToVar(pArr->length, scriptContext);
        }
        else
        {
            RecyclableObject* dynamicObject = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
            {LOGMEIN("JavascriptArray.cpp] 7753\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.unshift"));
            }

            BigIndex length;
            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {LOGMEIN("JavascriptArray.cpp] 7759\n");
                length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
            }
            else
            {
                length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
            }
            uint32 unshiftElements = args.Info.Count - 1;
            if (unshiftElements > 0)
            {LOGMEIN("JavascriptArray.cpp] 7768\n");
                uint32 MaxSpaceUint32 = MaxArrayLength - unshiftElements;
                // Note: end will always be a smallIndex either it is less than length in which case it is MaxSpaceUint32
                // or MaxSpaceUint32 is greater than length meaning length is a uint32 number
                BigIndex end = length > MaxSpaceUint32 ? MaxSpaceUint32 : length;
                if (end < length)
                {LOGMEIN("JavascriptArray.cpp] 7774\n");
                    // Unshift [end, length) to MaxArrayLength
                    // MaxArrayLength + (length - MaxSpaceUint32 - 1) = length + unshiftElements -1
                    if (length.IsSmallIndex())
                    {
                        Unshift<BigIndex>(dynamicObject, MaxArrayLength, end.GetSmallIndex(), length.GetSmallIndex(), scriptContext);
                    }
                    else
                    {
                        Unshift<BigIndex, uint64>(dynamicObject, MaxArrayLength, end.GetSmallIndex(), length.GetBigIndex(), scriptContext);
                    }
                }

                // Unshift [0, end) to unshiftElements
                // unshiftElements + (MaxSpaceUint32 - 0 - 1) = MaxArrayLength -1 therefore this unshift covers up to MaxArrayLength - 1
                Unshift<uint32>(dynamicObject, unshiftElements, 0, end.GetSmallIndex(), scriptContext);

                for (uint32 i = 0; i < unshiftElements; i++)
                {LOGMEIN("JavascriptArray.cpp] 7792\n");
                    JavascriptOperators::SetItem(dynamicObject, dynamicObject, i, args[i + 1], scriptContext, PropertyOperation_ThrowIfNotExtensible, true);
                }
            }

            ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.unshift"));

            //ES6 - update 'length' even if unshiftElements == 0;
            BigIndex newLen = length + unshiftElements;
            res = JavascriptNumber::ToVar(newLen.IsSmallIndex() ? newLen.GetSmallIndex() : newLen.GetBigIndex(), scriptContext);
            h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, res, scriptContext, PropertyOperation_ThrowIfNotExtensible));
        }
        return res;

    }

    Var JavascriptArray::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 7818\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
        }

         // ES5 15.4.4.2: call join, or built-in Object.prototype.toString

        RecyclableObject* obj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &obj))
        {LOGMEIN("JavascriptArray.cpp] 7826\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.toString"));
        }

        // In ES5 we could be calling a user defined join, even on array. We must [[Get]] join at runtime.
        Var join = JavascriptOperators::GetProperty(obj, PropertyIds::join, scriptContext);
        if (JavascriptConversion::IsCallable(join))
        {LOGMEIN("JavascriptArray.cpp] 7833\n");
            RecyclableObject* func = RecyclableObject::FromVar(join);
            // We need to record implicit call here, because marked the Array.toString as no side effect,
            // but if we call user code here which may have side effect
            ThreadContext * threadContext = scriptContext->GetThreadContext();
            Var result = threadContext->ExecuteImplicitCall(func, ImplicitCall_ToPrimitive, [=]() -> Js::Var
            {
                // Stack object should have a pre-op bail on implicit call. We shouldn't see them here.
                Assert(!ThreadContext::IsOnStack(obj));

                // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
                CallFlags flags = CallFlags_Value;
                return CALL_FUNCTION(func, CallInfo(flags, 1), obj);
            });

            if(!result)
            {LOGMEIN("JavascriptArray.cpp] 7849\n");
                // There was an implicit call and implicit calls are disabled. This would typically cause a bailout.
                Assert(threadContext->IsDisableImplicitCall());
                result = scriptContext->GetLibrary()->GetNull();
            }

            return result;
        }
        else
        {
            // call built-in Object.prototype.toString
            return CALL_ENTRYPOINT(JavascriptObject::EntryToString, function, CallInfo(1), obj);
        }
    }

#if DEBUG
    BOOL JavascriptArray::GetIndex(const char16* propName, uint32 *pIndex)
    {LOGMEIN("JavascriptArray.cpp] 7866\n");
        uint32 lu, luDig;

        int32 cch = (int32)wcslen(propName);
        char16* pch = const_cast<char16 *>(propName);

        lu = *pch - '0';
        if (lu > 9)
            return FALSE;

        if (0 == lu)
        {LOGMEIN("JavascriptArray.cpp] 7877\n");
            *pIndex = 0;
            return 1 == cch;
        }

        while ((luDig = *++pch - '0') < 10)
        {LOGMEIN("JavascriptArray.cpp] 7883\n");
            // If we overflow 32 bits, ignore the item
            if (lu > 0x19999999)
                return FALSE;
            lu *= 10;
            if(lu > (ULONG_MAX - luDig))
                return FALSE;
            lu += luDig;
        }

        if (pch - propName != cch)
            return FALSE;

        if (lu == JavascriptArray::InvalidIndex)
        {LOGMEIN("JavascriptArray.cpp] 7897\n");
            // 0xFFFFFFFF is not treated as an array index so that the length can be
            // capped at 32 bits.
            return FALSE;
        }

        *pIndex = lu;
        return TRUE;
    }
#endif

    JavascriptString* JavascriptArray::GetLocaleSeparator(ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 7909\n");
#ifdef ENABLE_GLOBALIZATION
        LCID lcid = GetUserDefaultLCID();
        int count = 0;
        char16 szSeparator[6];

        // According to the document for GetLocaleInfo this is a sufficient buffer size.
        count = GetLocaleInfoW(lcid, LOCALE_SLIST, szSeparator, 5);
        if( !count)
        {
            AssertMsg(FALSE, "GetLocaleInfo failed");
            return scriptContext->GetLibrary()->GetCommaSpaceDisplayString();
        }
        else
        {
            // Append ' '  if necessary
            if( count < 2 || szSeparator[count-2] != ' ')
            {
                szSeparator[count-1] = ' ';
                szSeparator[count] = '\0';
            }

            return JavascriptString::NewCopyBuffer(szSeparator, count, scriptContext);
        }
#else
        // xplat-todo: Support locale-specific seperator
        return scriptContext->GetLibrary()->GetCommaSpaceDisplayString();
#endif
    }

    template <typename T>
    JavascriptString* JavascriptArray::ToLocaleString(T* arr, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 7941\n");
        uint32 length = 0;
        if (TypedArrayBase::Is(arr))
        {LOGMEIN("JavascriptArray.cpp] 7944\n");
            // For a TypedArray use the actual length of the array.
            length = TypedArrayBase::FromVar(arr)->GetLength();
        }
        else
        {
            //For anything else, use the "length" property if present.
            length = ItemTrace<T>::GetLength(arr, scriptContext);
        }

        if (length == 0 || scriptContext->CheckObject(arr))
        {LOGMEIN("JavascriptArray.cpp] 7955\n");
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        JavascriptString* res = scriptContext->GetLibrary()->GetEmptyString();
        bool pushedObject = false;

        TryFinally([&]()
        {
            scriptContext->PushObject(arr);
            pushedObject = true;

            Var element;
            if (ItemTrace<T>::GetItem(arr, 0, &element, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 7969\n");
                res = JavascriptArray::ToLocaleStringHelper(element, scriptContext);
            }

            if (length > 1)
            {LOGMEIN("JavascriptArray.cpp] 7974\n");
                JavascriptString* separator = GetLocaleSeparator(scriptContext);

                for (uint32 i = 1; i < length; i++)
                {LOGMEIN("JavascriptArray.cpp] 7978\n");
                    res = JavascriptString::Concat(res, separator);
                    if (ItemTrace<T>::GetItem(arr, i, &element, scriptContext))
                    {LOGMEIN("JavascriptArray.cpp] 7981\n");
                        res = JavascriptString::Concat(res, JavascriptArray::ToLocaleStringHelper(element, scriptContext));
                    }
                }
            }
        },
        [&](bool/*hasException*/)
        {LOGMEIN("JavascriptArray.cpp] 7988\n");
            if (pushedObject)
            {LOGMEIN("JavascriptArray.cpp] 7990\n");
                Var top = scriptContext->PopObject();
                AssertMsg(top == arr, "Unmatched operation stack");
            }
        });

        if (res == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 7997\n");
            res = scriptContext->GetLibrary()->GetEmptyString();
        }

        return res;
    }

    Var JavascriptArray::EntryIsArray(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Constructor_isArray);

        if (args.Info.Count < 2)
        {LOGMEIN("JavascriptArray.cpp] 8016\n");
            return scriptContext->GetLibrary()->GetFalse();
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[1]);
#endif
        if (JavascriptOperators::IsArray(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 8024\n");
            return scriptContext->GetLibrary()->GetTrue();
        }
        return scriptContext->GetLibrary()->GetFalse();
    }

    ///----------------------------------------------------------------------------
    /// Find() calls the given predicate callback on each element of the array, in
    /// order, and returns the first element that makes the predicate return true,
    /// as described in (ES6.0: S22.1.3.8).
    ///----------------------------------------------------------------------------
    Var JavascriptArray::EntryFind(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 8045\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.find"));
        }

        int64 length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 8054\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
            length = pArr->length;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 8062\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.find"));
            }
            // In ES6-mode, we always load the length property from the object instead of using the internal slot.
            // Even for arrays, this is now observable via proxies.
            // If source object is not an array, we fall back to this behavior anyway.
            Var lenValue = JavascriptOperators::OP_GetLength(obj, scriptContext);
            length = JavascriptConversion::ToLength(lenValue, scriptContext);
        }

        return JavascriptArray::FindHelper<false>(pArr, nullptr, obj, length, args, scriptContext);
    }

    template <bool findIndex>
    Var JavascriptArray::FindHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, int64 length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 8077\n");
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 8079\n");
            // typedArrayBase is only non-null if and only if we came here via the TypedArray entrypoint
            if (typedArrayBase != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 8082\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, findIndex ? _u("[TypedArray].prototype.findIndex") : _u("[TypedArray].prototype.find"));
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, findIndex ? _u("Array.prototype.findIndex") : _u("Array.prototype.find"));
            }
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 8095\n");
            thisArg = args[2];
        }
        else
        {
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If we came from Array.prototype.find/findIndex and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 8105\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 8115\n");
            Var undefined = scriptContext->GetLibrary()->GetUndefined();

            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 8120\n");
                element = undefined;
                pArr->DirectGetItemAtFull(k, &element);

                Var index = JavascriptNumber::ToVar(k, scriptContext);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    index,
                    pArr);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8132\n");
                    return findIndex ? index : element;
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 8139\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::FindObjectHelper<findIndex>(obj, length, k + 1, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {LOGMEIN("JavascriptArray.cpp] 8146\n");
            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 8149\n");
                element = typedArrayBase->DirectGetItem(k);

                Var index = JavascriptNumber::ToVar(k, scriptContext);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    index,
                    typedArrayBase);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8160\n");
                    return findIndex ? index : element;
                }
            }
        }
        else
        {
            return JavascriptArray::FindObjectHelper<findIndex>(obj, length, 0u, callBackFn, thisArg, scriptContext);
        }

        return findIndex ? JavascriptNumber::ToVar(-1, scriptContext) : scriptContext->GetLibrary()->GetUndefined();
    }

    template <bool findIndex>
    Var JavascriptArray::FindObjectHelper(RecyclableObject* obj, int64 length, int64 start, RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 8175\n");
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        for (int64 k = start; k < length; k++)
        {LOGMEIN("JavascriptArray.cpp] 8182\n");
            element = JavascriptOperators::GetItem(obj, (uint64)k, scriptContext);
            Var index = JavascriptNumber::ToVar(k, scriptContext);

            testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                element,
                index,
                obj);

            if (JavascriptConversion::ToBoolean(testResult, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 8192\n");
                return findIndex ? index : element;
            }
        }

        return findIndex ? JavascriptNumber::ToVar(-1, scriptContext) : scriptContext->GetLibrary()->GetUndefined();
    }

    ///----------------------------------------------------------------------------
    /// FindIndex() calls the given predicate callback on each element of the
    /// array, in order, and returns the index of the first element that makes the
    /// predicate return true, as described in (ES6.0: S22.1.3.9).
    ///----------------------------------------------------------------------------
    Var JavascriptArray::EntryFindIndex(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 8215\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.findIndex"));
        }

        int64 length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 8224\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
            length = pArr->length;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 8232\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.findIndex"));
            }
            // In ES6-mode, we always load the length property from the object instead of using the internal slot.
            // Even for arrays, this is now observable via proxies.
            // If source object is not an array, we fall back to this behavior anyway.
            Var lenValue = JavascriptOperators::OP_GetLength(obj, scriptContext);
            length = JavascriptConversion::ToLength(lenValue, scriptContext);
        }

        return JavascriptArray::FindHelper<true>(pArr, nullptr, obj, length, args, scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// Entries() returns a new ArrayIterator object configured to return key-
    /// value pairs matching the elements of the this array/array-like object,
    /// as described in (ES6.0: S22.1.3.4).
    ///----------------------------------------------------------------------------
    Var JavascriptArray::EntryEntries(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 8260\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.entries"));
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {LOGMEIN("JavascriptArray.cpp] 8266\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.entries"));
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(thisObj);
#endif
        return scriptContext->GetLibrary()->CreateArrayIterator(thisObj, JavascriptArrayIteratorKind::KeyAndValue);
    }

    ///----------------------------------------------------------------------------
    /// Keys() returns a new ArrayIterator object configured to return the keys
    /// of the this array/array-like object, as described in (ES6.0: S22.1.3.13).
    ///----------------------------------------------------------------------------
    Var JavascriptArray::EntryKeys(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 8290\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.keys"));
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {LOGMEIN("JavascriptArray.cpp] 8296\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.keys"));
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(thisObj);
#endif
        return scriptContext->GetLibrary()->CreateArrayIterator(thisObj, JavascriptArrayIteratorKind::Key);
    }

    ///----------------------------------------------------------------------------
    /// Values() returns a new ArrayIterator object configured to return the values
    /// of the this array/array-like object, as described in (ES6.0: S22.1.3.29).
    ///----------------------------------------------------------------------------
    Var JavascriptArray::EntryValues(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 8320\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.values"));
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {LOGMEIN("JavascriptArray.cpp] 8326\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.values"));
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(thisObj);
#endif
        return scriptContext->GetLibrary()->CreateArrayIterator(thisObj, JavascriptArrayIteratorKind::Value);
    }

    Var JavascriptArray::EntryEvery(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.prototype.every"));

        Assert(!(callInfo.Flags & CallFlags_New));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_every);

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 8349\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.every"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 8358\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 8365\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.every"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 8371\n");
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }
        else
        {
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 8380\n");
            return JavascriptArray::EveryHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max()); // if pArr is not null lets make sure length is safe to cast, which will only happen if length is a uint32max
        return JavascriptArray::EveryHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.every as described by ES6.0 (draft 22) Section 22.1.3.5
    template <typename T>
    Var JavascriptArray::EveryHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 8390\n");
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 8392\n");
            // typedArrayBase is only non-null if and only if we came here via the TypedArray entrypoint
            if (typedArrayBase != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 8395\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.every"));
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.every"));
            }
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg = nullptr;


        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 8409\n");
            thisArg = args[2];
        }
        else
        {
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If we came from Array.prototype.map and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 8419\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        Var element = nullptr;
        Var testResult = nullptr;
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 8429\n");
            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 8432\n");
                if (!pArr->DirectGetItemAtFull(k, &element))
                {LOGMEIN("JavascriptArray.cpp] 8434\n");
                    continue;
                }

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                if (!JavascriptConversion::ToBoolean(testResult, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8444\n");
                    return scriptContext->GetLibrary()->GetFalse();
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 8451\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::EveryObjectHelper<T>(obj, length, k + 1, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {LOGMEIN("JavascriptArray.cpp] 8458\n");
            Assert(length <= UINT_MAX);

            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 8462\n");
                if (!typedArrayBase->HasItem(k))
                {LOGMEIN("JavascriptArray.cpp] 8464\n");
                    continue;
                }

                element = typedArrayBase->DirectGetItem(k);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    typedArrayBase);

                if (!JavascriptConversion::ToBoolean(testResult, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8476\n");
                    return scriptContext->GetLibrary()->GetFalse();
                }
            }
        }
        else
        {
            return JavascriptArray::EveryObjectHelper<T>(obj, length, 0u, callBackFn, thisArg, scriptContext);
        }

        return scriptContext->GetLibrary()->GetTrue();
    }

    template <typename T>
    Var JavascriptArray::EveryObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 8491\n");
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        for (T k = start; k < length; k++)
        {LOGMEIN("JavascriptArray.cpp] 8498\n");
            // According to es6 spec, we need to call Has first before calling Get
            if (JavascriptOperators::HasItem(obj, k))
            {LOGMEIN("JavascriptArray.cpp] 8501\n");
                element = JavascriptOperators::GetItem(obj, k, scriptContext);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                if (!JavascriptConversion::ToBoolean(testResult, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8510\n");
                    return scriptContext->GetLibrary()->GetFalse();
                }
            }
        }

        return scriptContext->GetLibrary()->GetTrue();
    }

    Var JavascriptArray::EntrySome(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.prototype.some"));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_some);

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 8532\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.some"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 8541\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 8548\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.some"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 8554\n");
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);

        }
        else
        {
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 8564\n");
            return JavascriptArray::SomeHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max()); // if pArr is not null lets make sure length is safe to cast, which will only happen if length is a uint32max
        return JavascriptArray::SomeHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.some as described in ES6.0 (draft 22) Section 22.1.3.23
    template <typename T>
    Var JavascriptArray::SomeHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 8574\n");
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 8576\n");
            // We are in the TypedArray version of this API if and only if typedArrayBase != nullptr
            if (typedArrayBase != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 8579\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.some"));
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.some"));
            }
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg = nullptr;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 8592\n");
            thisArg = args[2];
        }
        else
        {
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If we came from Array.prototype.some and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 8602\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 8612\n");
            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 8615\n");
                if (!pArr->DirectGetItemAtFull(k, &element))
                {LOGMEIN("JavascriptArray.cpp] 8617\n");
                    continue;
                }

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8627\n");
                    return scriptContext->GetLibrary()->GetTrue();
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 8634\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::SomeObjectHelper<T>(obj, length, k + 1, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {LOGMEIN("JavascriptArray.cpp] 8641\n");
            Assert(length <= UINT_MAX);

            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 8645\n");
                // If k < typedArrayBase->length, we know that HasItem will return true.
                // But we still have to call it in case there's a proxy trap or in the case that we are calling
                // Array.prototype.some with a TypedArray that has a different length instance property.
                if (!typedArrayBase->HasItem(k))
                {LOGMEIN("JavascriptArray.cpp] 8650\n");
                    continue;
                }

                element = typedArrayBase->DirectGetItem(k);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    typedArrayBase);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8662\n");
                    return scriptContext->GetLibrary()->GetTrue();
                }
            }
        }
        else
        {
            return JavascriptArray::SomeObjectHelper<T>(obj, length, 0u, callBackFn, thisArg, scriptContext);
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    template <typename T>
    Var JavascriptArray::SomeObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 8677\n");
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        for (T k = start; k < length; k++)
        {LOGMEIN("JavascriptArray.cpp] 8684\n");
            if (JavascriptOperators::HasItem(obj, k))
            {LOGMEIN("JavascriptArray.cpp] 8686\n");
                element = JavascriptOperators::GetItem(obj, k, scriptContext);
                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8694\n");
                    return scriptContext->GetLibrary()->GetTrue();
                }
            }
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    Var JavascriptArray::EntryForEach(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.prototype.forEach"));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_forEach)

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 8716\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.forEach"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* dynamicObject = nullptr;
        RecyclableObject* callBackFn = nullptr;
        Var thisArg = nullptr;

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
        if (JavascriptArray::Is(args[0]) && scriptContext == JavascriptArray::FromVar(args[0])->GetScriptContext())
        {LOGMEIN("JavascriptArray.cpp] 8730\n");
            pArr = JavascriptArray::FromVar(args[0]);
            dynamicObject = pArr;
        }
        else
        {
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
            {LOGMEIN("JavascriptArray.cpp] 8737\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.forEach"));
            }

            if (JavascriptArray::Is(dynamicObject) && scriptContext == JavascriptArray::FromVar(dynamicObject)->GetScriptContext())
            {LOGMEIN("JavascriptArray.cpp] 8742\n");
                pArr = JavascriptArray::FromVar(dynamicObject);
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 8748\n");
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }
        else
        {
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 8757\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.forEach"));
        }
        callBackFn = RecyclableObject::FromVar(args[1]);

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 8763\n");
            thisArg = args[2];
        }
        else
        {
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;

        auto fn32 = [dynamicObject, callBackFn, flags, thisArg, scriptContext](uint32 k, Var element)
        {
            CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                element,
                JavascriptNumber::ToVar(k, scriptContext),
                dynamicObject);
        };

        auto fn64 = [dynamicObject, callBackFn, flags, thisArg, scriptContext](uint64 k, Var element)
        {
            CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                element,
                JavascriptNumber::ToVar(k, scriptContext),
                dynamicObject);
        };

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 8791\n");
            Assert(pArr == dynamicObject);
            pArr->ForEachItemInRange<true>(0, length.IsUint32Max() ? MaxArrayLength : length.GetSmallIndex(), scriptContext, fn32);
        }
        else
        {
            if (length.IsSmallIndex())
            {
                TemplatedForEachItemInRange<true>(dynamicObject, 0u, length.GetSmallIndex(), scriptContext, fn32);
            }
            else
            {
                TemplatedForEachItemInRange<true>(dynamicObject, 0ui64, length.GetBigIndex(), scriptContext, fn64);
            }
        }
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptArray::EntryCopyWithin(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        RecyclableObject* obj = nullptr;
        JavascriptArray* pArr = nullptr;
        int64 length;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 8823\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;

            length = pArr->length;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 8835\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.copyWithin"));
            }

            // In ES6-mode, we always load the length property from the object instead of using the internal slot.
            // Even for arrays, this is now observable via proxies.
            // If source object is not an array, we fall back to this behavior anyway.
            Var lenValue = JavascriptOperators::OP_GetLength(obj, scriptContext);
            length = JavascriptConversion::ToLength(lenValue, scriptContext);
        }

        return JavascriptArray::CopyWithinHelper(pArr, nullptr, obj, length, args, scriptContext);
    }

    // Array.prototype.copyWithin as defined in ES6.0 (draft 22) Section 22.1.3.3
    Var JavascriptArray::CopyWithinHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, int64 length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 8851\n");
        Assert(args.Info.Count > 0);

        JavascriptLibrary* library = scriptContext->GetLibrary();
        int64 fromVal = 0;
        int64 toVal = 0;
        int64 finalVal = length;

        // If we came from Array.prototype.copyWithin and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 8861\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptArray.cpp] 8866\n");
            toVal = JavascriptArray::GetIndexFromVar(args[1], length, scriptContext);

            if (args.Info.Count > 2)
            {LOGMEIN("JavascriptArray.cpp] 8870\n");
                fromVal = JavascriptArray::GetIndexFromVar(args[2], length, scriptContext);

                if (args.Info.Count > 3 && args[3] != library->GetUndefined())
                {LOGMEIN("JavascriptArray.cpp] 8874\n");
                    finalVal = JavascriptArray::GetIndexFromVar(args[3], length, scriptContext);
                }
            }
        }

        // If count would be negative or zero, we won't do anything so go ahead and return early.
        if (finalVal <= fromVal || length <= toVal)
        {LOGMEIN("JavascriptArray.cpp] 8882\n");
            return obj;
        }

        // Make sure we won't underflow during the count calculation
        Assert(finalVal > fromVal && length > toVal);

        int64 count = min(finalVal - fromVal, length - toVal);

        // We shouldn't have made it here if the count was going to be zero
        Assert(count > 0);

        int direction;

        if (fromVal < toVal && toVal < (fromVal + count))
        {LOGMEIN("JavascriptArray.cpp] 8897\n");
            direction = -1;
            fromVal += count - 1;
            toVal += count - 1;
        }
        else
        {
            direction = 1;
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of arguments may convert the array to an ES5 array.
        if (pArr && !JavascriptArray::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 8909\n");
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        // If we are going to copy elements from or to indices > 2^32-1 we'll execute this (slightly slower path)
        // It's possible to optimize here so that we use the normal code below except for the > 2^32-1 indices
        if ((direction == -1 && (fromVal >= MaxArrayLength || toVal >= MaxArrayLength))
            || (((fromVal + count) > MaxArrayLength) || ((toVal + count) > MaxArrayLength)))
        {LOGMEIN("JavascriptArray.cpp] 8918\n");
            while (count > 0)
            {LOGMEIN("JavascriptArray.cpp] 8920\n");
                Var index = JavascriptNumber::ToVar(fromVal, scriptContext);

                if (JavascriptOperators::OP_HasItem(obj, index, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 8924\n");
                    Var val = JavascriptOperators::OP_GetElementI(obj, index, scriptContext);

                    JavascriptOperators::OP_SetElementI(obj, JavascriptNumber::ToVar(toVal, scriptContext), val, scriptContext, PropertyOperation_ThrowIfNotExtensible);
                }
                else
                {
                    JavascriptOperators::OP_DeleteElementI(obj, JavascriptNumber::ToVar(toVal, scriptContext), scriptContext, PropertyOperation_ThrowOnDeleteIfNotConfig);
                }

                fromVal += direction;
                toVal += direction;
                count--;
            }
        }
        else
        {
            Assert(fromVal < MaxArrayLength);
            Assert(toVal < MaxArrayLength);
            Assert(direction == -1 || (fromVal + count < MaxArrayLength && toVal + count < MaxArrayLength));

            uint32 fromIndex = static_cast<uint32>(fromVal);
            uint32 toIndex = static_cast<uint32>(toVal);

            while (count > 0)
            {LOGMEIN("JavascriptArray.cpp] 8949\n");
                if (obj->HasItem(fromIndex))
                {LOGMEIN("JavascriptArray.cpp] 8951\n");
                    if (typedArrayBase)
                    {LOGMEIN("JavascriptArray.cpp] 8953\n");
                        Var val = typedArrayBase->DirectGetItem(fromIndex);

                        typedArrayBase->DirectSetItem(toIndex, val);
                    }
                    else if (pArr)
                    {LOGMEIN("JavascriptArray.cpp] 8959\n");
                        Var val = pArr->DirectGetItem(fromIndex);

                        pArr->SetItem(toIndex, val, Js::PropertyOperation_ThrowIfNotExtensible);

                        if (!JavascriptArray::Is(obj))
                        {LOGMEIN("JavascriptArray.cpp] 8965\n");
                            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                            pArr = nullptr;
                        }
                    }
                    else
                    {
                        Var val = JavascriptOperators::OP_GetElementI_UInt32(obj, fromIndex, scriptContext);

                        JavascriptOperators::OP_SetElementI_UInt32(obj, toIndex, val, scriptContext, PropertyOperation_ThrowIfNotExtensible);
                    }
                }
                else
                {
                    obj->DeleteItem(toIndex, PropertyOperation_ThrowOnDeleteIfNotConfig);
                }

                fromIndex += direction;
                toIndex += direction;
                count--;
            }
        }

        return obj;
    }

    Var JavascriptArray::EntryFill(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        RecyclableObject* obj = nullptr;
        JavascriptArray* pArr = nullptr;
        int64 length;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 9005\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;

            length = pArr->length;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 9014\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.fill"));
            }

            // In ES6-mode, we always load the length property from the object instead of using the internal slot.
            // Even for arrays, this is now observable via proxies.
            // If source object is not an array, we fall back to this behavior anyway.
            Var lenValue = JavascriptOperators::OP_GetLength(obj, scriptContext);
            length = JavascriptConversion::ToLength(lenValue, scriptContext);
        }

        return JavascriptArray::FillHelper(pArr, nullptr, obj, length, args, scriptContext);
    }

    // Array.prototype.fill as defined in ES6.0 (draft 22) Section 22.1.3.6
    Var JavascriptArray::FillHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, int64 length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 9030\n");
        Assert(args.Info.Count > 0);

        JavascriptLibrary* library = scriptContext->GetLibrary();

        // If we came from Array.prototype.fill and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 9037\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        Var fillValue;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptArray.cpp] 9044\n");
            fillValue = args[1];
        }
        else
        {
            fillValue = library->GetUndefined();
        }

        int64 k = 0;
        int64 finalVal = length;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 9056\n");
            k = JavascriptArray::GetIndexFromVar(args[2], length, scriptContext);

            if (args.Info.Count > 3 && !JavascriptOperators::IsUndefinedObject(args[3]))
            {LOGMEIN("JavascriptArray.cpp] 9060\n");
                finalVal = JavascriptArray::GetIndexFromVar(args[3], length, scriptContext);
            }

            // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
            // we will process the array elements like an ES5Array.
            if (pArr && !JavascriptArray::Is(obj))
            {LOGMEIN("JavascriptArray.cpp] 9067\n");
                AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                pArr = nullptr;
            }
        }

        if (k < MaxArrayLength)
        {LOGMEIN("JavascriptArray.cpp] 9074\n");
            int64 end = min<int64>(finalVal, MaxArrayLength);
            uint32 u32k = static_cast<uint32>(k);

            while (u32k < end)
            {LOGMEIN("JavascriptArray.cpp] 9079\n");
                if (typedArrayBase)
                {LOGMEIN("JavascriptArray.cpp] 9081\n");
                    typedArrayBase->DirectSetItem(u32k, fillValue);
                }
                else if (pArr)
                {LOGMEIN("JavascriptArray.cpp] 9085\n");
                    pArr->SetItem(u32k, fillValue, PropertyOperation_ThrowIfNotExtensible);
                }
                else
                {
                    JavascriptOperators::OP_SetElementI_UInt32(obj, u32k, fillValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                }

                u32k++;
            }

            BigIndex dstIndex = MaxArrayLength;

            for (int64 i = end; i < finalVal; ++i)
            {LOGMEIN("JavascriptArray.cpp] 9099\n");
                if (pArr)
                {LOGMEIN("JavascriptArray.cpp] 9101\n");
                    pArr->DirectSetItemAt(dstIndex, fillValue);
                    ++dstIndex;
                }
                else
                {
                    JavascriptOperators::OP_SetElementI(obj, JavascriptNumber::ToVar(i, scriptContext), fillValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }
        else
        {
            BigIndex dstIndex = static_cast<uint64>(k);

            for (int64 i = k; i < finalVal; i++)
            {LOGMEIN("JavascriptArray.cpp] 9116\n");
                if (pArr)
                {LOGMEIN("JavascriptArray.cpp] 9118\n");
                    pArr->DirectSetItemAt(dstIndex, fillValue);
                    ++dstIndex;
                }
                else
                {
                    JavascriptOperators::OP_SetElementI(obj, JavascriptNumber::ToVar(i, scriptContext), fillValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }

        return obj;
    }

    // Array.prototype.map as defined by ES6.0 (Final) 22.1.3.15
    Var JavascriptArray::EntryMap(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.prototype.map"));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_map);

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 9146\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.map"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 9155\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 9162\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.map"));
            }
        }

        length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);

        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 9170\n");
            return JavascriptArray::MapHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max()); // if pArr is not null lets make sure length is safe to cast, which will only happen if length is a uint32max
        return JavascriptArray::MapHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }


    template<typename T>
    Var JavascriptArray::MapHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 9180\n");
        RecyclableObject* newObj = nullptr;
        JavascriptArray* newArr = nullptr;
        bool isTypedArrayEntryPoint = typedArrayBase != nullptr;
        bool isBuiltinArrayCtor = true;

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 9187\n");
            if (isTypedArrayEntryPoint)
            {LOGMEIN("JavascriptArray.cpp] 9189\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.map"));
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.map"));
            }
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 9202\n");
            thisArg = args[2];
        }
        else
        {
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If we came from Array.prototype.map and source object is not a JavascriptArray, source could be a TypedArray
        if (!isTypedArrayEntryPoint && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 9212\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        // If the entry point is %TypedArray%.prototype.map or the source object is an Array exotic object we should try to load the constructor property
        // and use it to construct the return object.
        if (isTypedArrayEntryPoint)
        {LOGMEIN("JavascriptArray.cpp] 9219\n");
            Var constructor = JavascriptOperators::SpeciesConstructor(
                typedArrayBase, TypedArrayBase::GetDefaultConstructor(args[0], scriptContext), scriptContext);
            isBuiltinArrayCtor = (constructor == scriptContext->GetLibrary()->GetArrayConstructor());

            if (JavascriptOperators::IsConstructor(constructor))
            {LOGMEIN("JavascriptArray.cpp] 9225\n");
                Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(length, scriptContext) };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = RecyclableObject::FromVar(TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), (uint32)length, scriptContext));
            }
            else if (isTypedArrayEntryPoint)
            {LOGMEIN("JavascriptArray.cpp] 9231\n");
                // We only need to throw a TypeError when the constructor property is not an actual constructor if %TypedArray%.prototype.map was called
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NotAConstructor, _u("[TypedArray].prototype.map"));
            }
        }
        // skip the typed array and "pure" array case, we still need to handle special arrays like es5array, remote array, and proxy of array.
        else if (pArr == nullptr || scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {LOGMEIN("JavascriptArray.cpp] 9238\n");
            newObj = ArraySpeciesCreate(obj, length, scriptContext, nullptr, nullptr, &isBuiltinArrayCtor);
        }

        if (newObj == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 9243\n");
            if (length > UINT_MAX)
            {LOGMEIN("JavascriptArray.cpp] 9245\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
            }
            newArr = scriptContext->GetLibrary()->CreateArray(static_cast<uint32>(length));
            newArr->EnsureHead<Var>();
            newObj = newArr;
        }
        else
        {
            // If the new object we created is an array, remember that as it will save us time setting properties in the object below
            if (JavascriptArray::Is(newObj))
            {LOGMEIN("JavascriptArray.cpp] 9256\n");
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                newArr = JavascriptArray::FromVar(newObj);
            }
        }

        Var element = nullptr;
        Var mappedValue = nullptr;
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags callBackFnflags = CallFlags_Value;
        CallInfo callBackFnInfo = CallInfo(callBackFnflags, 4);

        // We at least have to have newObj as a valid object
        Assert(newObj);

        // The ArraySpeciesCreate call above could have converted the source array into an ES5Array. If this happens
        // we will process the array elements like an ES5Array.
        if (pArr && !JavascriptArray::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 9276\n");
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (pArr != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 9282\n");
            // If source is a JavascriptArray, newObj may or may not be an array based on what was in source's constructor property
            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 9286\n");
                if (!pArr->DirectGetItemAtFull(k, &element))
                {LOGMEIN("JavascriptArray.cpp] 9288\n");
                    continue;
                }

                mappedValue = CALL_FUNCTION(callBackFn, callBackFnInfo, thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                // If newArr is a valid pointer, then we constructed an array to return. Otherwise we need to do generic object operations
                if (newArr && isBuiltinArrayCtor)
                {LOGMEIN("JavascriptArray.cpp] 9299\n");
                    newArr->DirectSetItemAt(k, mappedValue);
                }
                else
                {
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, k, mappedValue), scriptContext, k);
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 9310\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::MapObjectHelper<T>(obj, length, k + 1, newObj, newArr, isBuiltinArrayCtor, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else if (typedArrayBase != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 9317\n");
            // Source is a TypedArray, we may have tried to call a constructor, but newObj may not be a TypedArray (or an array either)
            TypedArrayBase* newTypedArray = nullptr;

            if (TypedArrayBase::Is(newObj))
            {LOGMEIN("JavascriptArray.cpp] 9322\n");
                newTypedArray = TypedArrayBase::FromVar(newObj);
            }

            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 9328\n");
                // We can't rely on the length value being equal to typedArrayBase->GetLength() because user code may lie and
                // attach any length property to a TypedArray instance and pass it as this parameter when .calling
                // Array.prototype.map.
                if (!typedArrayBase->HasItem(k))
                {LOGMEIN("JavascriptArray.cpp] 9333\n");
                    // We know that if HasItem returns false, all the future calls to HasItem will return false as well since
                    // we visit the items in order. We could return early here except that we have to continue calling HasItem
                    // on all the subsequent items according to the spec.
                    continue;
                }

                element = typedArrayBase->DirectGetItem(k);
                mappedValue = CALL_FUNCTION(callBackFn, callBackFnInfo, thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                // If newObj is a TypedArray, set the mappedValue directly, otherwise see if it's an array and finally fall back to
                // the normal Set path.
                if (newTypedArray)
                {LOGMEIN("JavascriptArray.cpp] 9349\n");
                    newTypedArray->DirectSetItem(k, mappedValue);
                }
                else if (newArr)
                {LOGMEIN("JavascriptArray.cpp] 9353\n");
                    newArr->DirectSetItemAt(k, mappedValue);
                }
                else
                {
                    JavascriptArray::SetArrayLikeObjects(newObj, k, mappedValue);
                }
            }
        }
        else
        {
            return JavascriptArray::MapObjectHelper<T>(obj, length, 0u, newObj, newArr, isBuiltinArrayCtor, callBackFn, thisArg, scriptContext);
        }

#ifdef VALIDATE_ARRAY
        if (JavascriptArray::Is(newObj))
        {LOGMEIN("JavascriptArray.cpp] 9369\n");
            newArr->ValidateArray();
        }
#endif

        return newObj;
    }

    template<typename T>
    Var JavascriptArray::MapObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* newObj, JavascriptArray* newArr,
        bool isBuiltinArrayCtor, RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 9380\n");
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags callBackFnflags = CallFlags_Value;
        CallInfo callBackFnInfo = CallInfo(callBackFnflags, 4);
        Var element = nullptr;
        Var mappedValue = nullptr;

        for (T k = start; k < length; k++)
        {LOGMEIN("JavascriptArray.cpp] 9388\n");
            if (JavascriptOperators::HasItem(obj, k))
            {LOGMEIN("JavascriptArray.cpp] 9390\n");
                element = JavascriptOperators::GetItem(obj, k, scriptContext);
                mappedValue = CALL_FUNCTION(callBackFn, callBackFnInfo, thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                if (newArr && isBuiltinArrayCtor)
                {LOGMEIN("JavascriptArray.cpp] 9398\n");
                    newArr->SetItem((uint32)k, mappedValue, PropertyOperation_None);
                }
                else
                {
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, BigIndex(k), mappedValue), scriptContext, BigIndex(k));
                }
            }
        }

#ifdef VALIDATE_ARRAY
        if (JavascriptArray::Is(newObj))
        {LOGMEIN("JavascriptArray.cpp] 9410\n");
            newArr->ValidateArray();
        }
#endif

        return newObj;
    }

    Var JavascriptArray::EntryFilter(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.prototype.filter"));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_filter);

        Assert(!(callInfo.Flags & CallFlags_New));
        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 9430\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.filter"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* dynamicObject = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 9439\n");
            pArr = JavascriptArray::FromVar(args[0]);
            dynamicObject = pArr;
        }
        else
        {
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
            {LOGMEIN("JavascriptArray.cpp] 9446\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.filter"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 9452\n");
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }
        else
        {
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 9461\n");
            return JavascriptArray::FilterHelper(pArr, dynamicObject, length.GetSmallIndex(), args, scriptContext);
        }
        return JavascriptArray::FilterHelper(pArr, dynamicObject, length.GetBigIndex(), args, scriptContext);
    }

    template <typename T>
    Var JavascriptArray::FilterHelper(JavascriptArray* pArr, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 9469\n");
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 9471\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.filter"));
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg = nullptr;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 9479\n");
            thisArg = args[2];
        }
        else
        {
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If the source object is an Array exotic object we should try to load the constructor property and use it to construct the return object.
        bool isBuiltinArrayCtor = true;
        RecyclableObject* newObj = ArraySpeciesCreate(obj, 0, scriptContext, nullptr, nullptr, &isBuiltinArrayCtor);
        JavascriptArray* newArr = nullptr;

        if (newObj == nullptr)
        {LOGMEIN("JavascriptArray.cpp] 9493\n");
            newArr = scriptContext->GetLibrary()->CreateArray(0);
            newArr->EnsureHead<Var>();
            newObj = newArr;
        }
        else
        {
            // If the new object we created is an array, remember that as it will save us time setting properties in the object below
            if (JavascriptArray::Is(newObj))
            {LOGMEIN("JavascriptArray.cpp] 9502\n");
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                newArr = JavascriptArray::FromVar(newObj);
            }
        }

        // The ArraySpeciesCreate call above could have converted the source array into an ES5Array. If this happens
        // we will process the array elements like an ES5Array.
        if (pArr && !JavascriptArray::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 9513\n");
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        Var element = nullptr;
        Var selected = nullptr;

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 9522\n");
            Assert(length <= MaxArrayLength);
            uint32 i = 0;

            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {LOGMEIN("JavascriptArray.cpp] 9528\n");
                if (!pArr->DirectGetItemAtFull(k, &element))
                {LOGMEIN("JavascriptArray.cpp] 9530\n");
                    continue;
                }

                selected = CALL_ENTRYPOINT(callBackFn->GetEntryPoint(), callBackFn, CallInfo(CallFlags_Value, 4),
                    thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                if (JavascriptConversion::ToBoolean(selected, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 9541\n");
                    // Try to fast path if the return object is an array
                    if (newArr && isBuiltinArrayCtor)
                    {LOGMEIN("JavascriptArray.cpp] 9544\n");
                        newArr->DirectSetItemAt(i, element);
                    }
                    else
                    {
                        ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, i, element), scriptContext, i);
                    }
                    ++i;
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 9557\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::FilterObjectHelper<T>(obj, length, k + 1, newArr, newObj, i, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else
        {
            return JavascriptArray::FilterObjectHelper<T>(obj, length, 0u, newArr, newObj, 0u, callBackFn, thisArg, scriptContext);
        }

#ifdef VALIDATE_ARRAY
        if (newArr)
        {LOGMEIN("JavascriptArray.cpp] 9570\n");
            newArr->ValidateArray();
        }
#endif

        return newObj;
    }

    template <typename T>
    Var JavascriptArray::FilterObjectHelper(RecyclableObject* obj, T length, T start, JavascriptArray* newArr, RecyclableObject* newObj, T newStart,
        RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 9581\n");
        Var element = nullptr;
        Var selected = nullptr;
        BigIndex i = BigIndex(newStart);

        for (T k = start; k < length; k++)
        {LOGMEIN("JavascriptArray.cpp] 9587\n");
            if (JavascriptOperators::HasItem(obj, k))
            {LOGMEIN("JavascriptArray.cpp] 9589\n");
                element = JavascriptOperators::GetItem(obj, k, scriptContext);
                selected = CALL_ENTRYPOINT(callBackFn->GetEntryPoint(), callBackFn, CallInfo(CallFlags_Value, 4),
                    thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                if (JavascriptConversion::ToBoolean(selected, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 9598\n");
                    if (newArr)
                    {LOGMEIN("JavascriptArray.cpp] 9600\n");
                        newArr->DirectSetItemAt(i, element);
                    }
                    else
                    {
                        ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, i, element), scriptContext, i);
                    }

                    ++i;
                }
            }
        }

#ifdef VALIDATE_ARRAY
        if (newArr)
        {LOGMEIN("JavascriptArray.cpp] 9615\n");
            newArr->ValidateArray();
        }
#endif

        return newObj;
    }

    Var JavascriptArray::EntryReduce(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.prototype.reduce"));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_reduce);

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 9636\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reduce"));
        }

        BigIndex length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 9645\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;

            length = pArr->length;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 9654\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reduce"));
            }

            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {LOGMEIN("JavascriptArray.cpp] 9659\n");
                length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
            }
            else
            {
                length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
            }
        }
        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 9668\n");
            return JavascriptArray::ReduceHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        return JavascriptArray::ReduceHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.reduce as described in ES6.0 (draft 22) Section 22.1.3.18
    template <typename T>
    Var JavascriptArray::ReduceHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 9677\n");
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 9679\n");
            if (typedArrayBase != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 9681\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.reduce"));
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.reduce"));
            }
        }

        // If we came from Array.prototype.reduce and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 9692\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        T k = 0;
        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var accumulator = nullptr;
        Var element = nullptr;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 9702\n");
            accumulator = args[2];
        }
        else
        {
            if (length == 0)
            {LOGMEIN("JavascriptArray.cpp] 9708\n");
                JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
            }

            bool bPresent = false;

            if (pArr)
            {LOGMEIN("JavascriptArray.cpp] 9715\n");
                for (; k < length && bPresent == false; k++)
                {LOGMEIN("JavascriptArray.cpp] 9717\n");
                    if (!pArr->DirectGetItemAtFull((uint32)k, &element))
                    {LOGMEIN("JavascriptArray.cpp] 9719\n");
                        continue;
                    }

                    bPresent = true;
                    accumulator = element;
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 9730\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    pArr = nullptr;
                }
            }
            else if (typedArrayBase)
            {LOGMEIN("JavascriptArray.cpp] 9736\n");
                Assert(length <= UINT_MAX);

                for (; k < length && bPresent == false; k++)
                {LOGMEIN("JavascriptArray.cpp] 9740\n");
                    if (!typedArrayBase->HasItem((uint32)k))
                    {LOGMEIN("JavascriptArray.cpp] 9742\n");
                        continue;
                    }

                    element = typedArrayBase->DirectGetItem((uint32)k);

                    bPresent = true;
                    accumulator = element;
                }
            }
            else
            {
                for (; k < length && bPresent == false; k++)
                {LOGMEIN("JavascriptArray.cpp] 9755\n");
                    if (JavascriptOperators::HasItem(obj, k))
                    {LOGMEIN("JavascriptArray.cpp] 9757\n");
                        accumulator = JavascriptOperators::GetItem(obj, k, scriptContext);
                        bPresent = true;
                    }
                }
            }

            if (bPresent == false)
            {LOGMEIN("JavascriptArray.cpp] 9765\n");
                JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
            }
        }

        Assert(accumulator);

        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 9777\n");
            for (; k < length; k++)
            {LOGMEIN("JavascriptArray.cpp] 9779\n");
                if (!pArr->DirectGetItemAtFull((uint32)k, &element))
                {LOGMEIN("JavascriptArray.cpp] 9781\n");
                    continue;
                }

                accumulator = CALL_FUNCTION(callBackFn, CallInfo(flags, 5), undefinedValue,
                    accumulator,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 9794\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::ReduceObjectHelper<T>(obj, length, k + 1, callBackFn, accumulator, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {LOGMEIN("JavascriptArray.cpp] 9801\n");
            Assert(length <= UINT_MAX);
            for (; k < length; k++)
            {LOGMEIN("JavascriptArray.cpp] 9804\n");
                if (!typedArrayBase->HasItem((uint32)k))
                {LOGMEIN("JavascriptArray.cpp] 9806\n");
                    continue;
                }

                element = typedArrayBase->DirectGetItem((uint32)k);

                accumulator = CALL_FUNCTION(callBackFn, CallInfo(flags, 5), undefinedValue,
                    accumulator,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    typedArrayBase);
            }
        }
        else
        {
            return JavascriptArray::ReduceObjectHelper<T>(obj, length, k, callBackFn, accumulator, scriptContext);
        }

        return accumulator;
    }

    template <typename T>
    Var JavascriptArray::ReduceObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* callBackFn, Var accumulator, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 9829\n");
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;

        for (T k = start; k < length; k++)
        {LOGMEIN("JavascriptArray.cpp] 9835\n");
            if (JavascriptOperators::HasItem(obj, k))
            {LOGMEIN("JavascriptArray.cpp] 9837\n");
                element = JavascriptOperators::GetItem(obj, k, scriptContext);

                accumulator = CALL_FUNCTION(callBackFn, CallInfo(flags, 5), scriptContext->GetLibrary()->GetUndefined(),
                    accumulator,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);
            }
        }

        return accumulator;
    }

    Var JavascriptArray::EntryReduceRight(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.prototype.reduceRight"));

        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Array_Prototype_reduceRight);

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 9864\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reduceRight"));
        }

        BigIndex length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {LOGMEIN("JavascriptArray.cpp] 9873\n");
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {LOGMEIN("JavascriptArray.cpp] 9880\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reduceRight"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {LOGMEIN("JavascriptArray.cpp] 9886\n");
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }
        else
        {
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 9895\n");
            return JavascriptArray::ReduceRightHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        return JavascriptArray::ReduceRightHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.reduceRight as described in ES6.0 (draft 22) Section 22.1.3.19
    template <typename T>
    Var JavascriptArray::ReduceRightHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 9904\n");
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {LOGMEIN("JavascriptArray.cpp] 9906\n");
            if (typedArrayBase != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 9908\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.reduceRight"));
            }
            else
            {
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.reduceRight"));
            }
        }

        // If we came from Array.prototype.reduceRight and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {LOGMEIN("JavascriptArray.cpp] 9919\n");
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var accumulator = nullptr;
        Var element = nullptr;
        T k = 0;
        T index = 0;

        if (args.Info.Count > 2)
        {LOGMEIN("JavascriptArray.cpp] 9930\n");
            accumulator = args[2];
        }
        else
        {
            if (length == 0)
            {LOGMEIN("JavascriptArray.cpp] 9936\n");
                JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
            }

            bool bPresent = false;
            if (pArr)
            {LOGMEIN("JavascriptArray.cpp] 9942\n");
                for (; k < length && bPresent == false; k++)
                {LOGMEIN("JavascriptArray.cpp] 9944\n");
                    index = length - k - 1;
                    if (!pArr->DirectGetItemAtFull((uint32)index, &element))
                    {LOGMEIN("JavascriptArray.cpp] 9947\n");
                        continue;
                    }
                    bPresent = true;
                    accumulator = element;
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 9957\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    pArr = nullptr;
                }
            }
            else if (typedArrayBase)
            {LOGMEIN("JavascriptArray.cpp] 9963\n");
                Assert(length <= UINT_MAX);
                for (; k < length && bPresent == false; k++)
                {LOGMEIN("JavascriptArray.cpp] 9966\n");
                    index = length - k - 1;
                    if (!typedArrayBase->HasItem((uint32)index))
                    {LOGMEIN("JavascriptArray.cpp] 9969\n");
                        continue;
                    }
                    element = typedArrayBase->DirectGetItem((uint32)index);
                    bPresent = true;
                    accumulator = element;
                }
            }
            else
            {
                for (; k < length && bPresent == false; k++)
                {LOGMEIN("JavascriptArray.cpp] 9980\n");
                    index = length - k - 1;
                    if (JavascriptOperators::HasItem(obj, index))
                    {LOGMEIN("JavascriptArray.cpp] 9983\n");
                        accumulator = JavascriptOperators::GetItem(obj, index, scriptContext);
                        bPresent = true;
                    }
                }
            }
            if (bPresent == false)
            {LOGMEIN("JavascriptArray.cpp] 9990\n");
                JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
            }
        }

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();

        if (pArr)
        {LOGMEIN("JavascriptArray.cpp] 10000\n");
            for (; k < length; k++)
            {LOGMEIN("JavascriptArray.cpp] 10002\n");
                index = length - k - 1;
                if (!pArr->DirectGetItemAtFull((uint32)index, &element))
                {LOGMEIN("JavascriptArray.cpp] 10005\n");
                    continue;
                }

                accumulator = CALL_FUNCTION(callBackFn, CallInfo(flags, 5), undefinedValue,
                    accumulator,
                    element,
                    JavascriptNumber::ToVar(index, scriptContext),
                    pArr);

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {LOGMEIN("JavascriptArray.cpp] 10018\n");
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::ReduceRightObjectHelper<T>(obj, length, k + 1, callBackFn, accumulator, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {LOGMEIN("JavascriptArray.cpp] 10025\n");
            Assert(length <= UINT_MAX);
            for (; k < length; k++)
            {LOGMEIN("JavascriptArray.cpp] 10028\n");
                index = length - k - 1;
                if (!typedArrayBase->HasItem((uint32) index))
                {LOGMEIN("JavascriptArray.cpp] 10031\n");
                    continue;
                }

                element = typedArrayBase->DirectGetItem((uint32)index);

                accumulator = CALL_FUNCTION(callBackFn, CallInfo(flags, 5), undefinedValue,
                    accumulator,
                    element,
                    JavascriptNumber::ToVar(index, scriptContext),
                    typedArrayBase);
            }
        }
        else
        {
            return JavascriptArray::ReduceRightObjectHelper<T>(obj, length, k, callBackFn, accumulator, scriptContext);
        }

        return accumulator;
    }

    template <typename T>
    Var JavascriptArray::ReduceRightObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* callBackFn, Var accumulator, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 10054\n");
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        T index = 0;

        for (T k = start; k < length; k++)
        {LOGMEIN("JavascriptArray.cpp] 10061\n");
            index = length - k - 1;
            if (JavascriptOperators::HasItem(obj, index))
            {LOGMEIN("JavascriptArray.cpp] 10064\n");
                element = JavascriptOperators::GetItem(obj, index, scriptContext);
                accumulator = CALL_FUNCTION(callBackFn, CallInfo(flags, 5), scriptContext->GetLibrary()->GetUndefined(),
                    accumulator,
                    element,
                    JavascriptNumber::ToVar(index, scriptContext),
                    obj);
            }
        }

        return accumulator;
    }

    Var JavascriptArray::EntryFrom(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        AUTO_TAG_NATIVE_LIBRARY_ENTRY(function, callInfo, _u("Array.from"));

        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptLibrary* library = scriptContext->GetLibrary();
        RecyclableObject* constructor = nullptr;

        if (JavascriptOperators::IsConstructor(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 10091\n");
            constructor = RecyclableObject::FromVar(args[0]);
        }

        RecyclableObject* items = nullptr;

        if (args.Info.Count < 2 || !JavascriptConversion::ToObject(args[1], scriptContext, &items))
        {LOGMEIN("JavascriptArray.cpp] 10098\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, _u("Array.from"));
        }

        JavascriptArray* itemsArr = nullptr;

        if (JavascriptArray::Is(items))
        {LOGMEIN("JavascriptArray.cpp] 10105\n");
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(items);
#endif
            itemsArr = JavascriptArray::FromVar(items);
        }

        bool mapping = false;
        JavascriptFunction* mapFn = nullptr;
        Var mapFnThisArg = nullptr;

        if (args.Info.Count >= 3 && !JavascriptOperators::IsUndefinedObject(args[2]))
        {LOGMEIN("JavascriptArray.cpp] 10117\n");
            if (!JavascriptFunction::Is(args[2]))
            {LOGMEIN("JavascriptArray.cpp] 10119\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.from"));
            }

            mapFn = JavascriptFunction::FromVar(args[2]);

            if (args.Info.Count >= 4)
            {LOGMEIN("JavascriptArray.cpp] 10126\n");
                mapFnThisArg = args[3];
            }
            else
            {
                mapFnThisArg = library->GetUndefined();
            }

            mapping = true;
        }

        RecyclableObject* newObj = nullptr;
        JavascriptArray* newArr = nullptr;

        RecyclableObject* iterator = JavascriptOperators::GetIterator(items, scriptContext, true /* optional */);

        if (iterator != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 10143\n");
            if (constructor)
            {LOGMEIN("JavascriptArray.cpp] 10145\n");
                Js::Var constructorArgs[] = { constructor };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = RecyclableObject::FromVar(JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext));

                if (JavascriptArray::Is(newObj))
                {LOGMEIN("JavascriptArray.cpp] 10151\n");
                    newArr = JavascriptArray::FromVar(newObj);
                }
            }
            else
            {
                newArr = scriptContext->GetLibrary()->CreateArray(0);
                newArr->EnsureHead<Var>();
                newObj = newArr;
            }

            uint32 k = 0;

            JavascriptOperators::DoIteratorStepAndValue(iterator, scriptContext, [&](Var nextValue) {
                if (mapping)
                {LOGMEIN("JavascriptArray.cpp] 10166\n");
                    Assert(mapFn != nullptr);
                    Assert(mapFnThisArg != nullptr);

                    Js::Var mapFnArgs[] = { mapFnThisArg, nextValue, JavascriptNumber::ToVar(k, scriptContext) };
                    Js::CallInfo mapFnCallInfo(Js::CallFlags_Value, _countof(mapFnArgs));
                    nextValue = mapFn->CallFunction(Js::Arguments(mapFnCallInfo, mapFnArgs));
                }

                if (newArr)
                {LOGMEIN("JavascriptArray.cpp] 10176\n");
                    newArr->SetItem(k, nextValue, PropertyOperation_None);
                }
                else
                {
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, k, nextValue), scriptContext, k);
                }

                k++;
            });

            JavascriptOperators::SetProperty(newObj, newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(k, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible);
        }
        else
        {
            Var lenValue = JavascriptOperators::OP_GetLength(items, scriptContext);
            int64 len = JavascriptConversion::ToLength(lenValue, scriptContext);

            if (constructor)
            {LOGMEIN("JavascriptArray.cpp] 10195\n");
                Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(len, scriptContext) };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = RecyclableObject::FromVar(JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext));

                if (JavascriptArray::Is(newObj))
                {LOGMEIN("JavascriptArray.cpp] 10201\n");
                    newArr = JavascriptArray::FromVar(newObj);
                }
            }
            else
            {
                // Abstract operation ArrayCreate throws RangeError if length argument is > 2^32 -1
                if (len > MaxArrayLength)
                {LOGMEIN("JavascriptArray.cpp] 10209\n");
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect, _u("Array.from"));
                }

                // Static cast len should be valid (len < 2^32) or we would throw above
                newArr = scriptContext->GetLibrary()->CreateArray(static_cast<uint32>(len));
                newArr->EnsureHead<Var>();
                newObj = newArr;
            }

            uint32 k = 0;

            for ( ; k < len; k++)
            {LOGMEIN("JavascriptArray.cpp] 10222\n");
                Var kValue;

                if (itemsArr)
                {LOGMEIN("JavascriptArray.cpp] 10226\n");
                    kValue = itemsArr->DirectGetItem(k);
                }
                else
                {
                    kValue = JavascriptOperators::OP_GetElementI_UInt32(items, k, scriptContext);
                }

                if (mapping)
                {LOGMEIN("JavascriptArray.cpp] 10235\n");
                    Assert(mapFn != nullptr);
                    Assert(mapFnThisArg != nullptr);

                    Js::Var mapFnArgs[] = { mapFnThisArg, kValue, JavascriptNumber::ToVar(k, scriptContext) };
                    Js::CallInfo mapFnCallInfo(Js::CallFlags_Value, _countof(mapFnArgs));
                    kValue = mapFn->CallFunction(Js::Arguments(mapFnCallInfo, mapFnArgs));
                }

                if (newArr)
                {LOGMEIN("JavascriptArray.cpp] 10245\n");
                    newArr->SetItem(k, kValue, PropertyOperation_None);
                }
                else
                {
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, k, kValue), scriptContext, k);
                }
            }

            JavascriptOperators::SetProperty(newObj, newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(len, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible);
        }

        return newObj;
    }

    Var JavascriptArray::EntryOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptArray.cpp] 10270\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.of"));
        }

        return JavascriptArray::OfHelper(false, args, scriptContext);
    }

    Var JavascriptArray::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    // Array.of and %TypedArray%.of as described in ES6.0 (draft 22) Section 22.1.2.2 and 22.2.2.2
    Var JavascriptArray::OfHelper(bool isTypedArrayEntryPoint, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 10288\n");
        Assert(args.Info.Count > 0);

        // args.Info.Count cannot equal zero or we would have thrown above so no chance of underflowing
        uint32 len = args.Info.Count - 1;
        Var newObj = nullptr;
        JavascriptArray* newArr = nullptr;
        TypedArrayBase* newTypedArray = nullptr;
        bool isBuiltinArrayCtor = true;

        if (JavascriptOperators::IsConstructor(args[0]))
        {LOGMEIN("JavascriptArray.cpp] 10299\n");
            RecyclableObject* constructor = RecyclableObject::FromVar(args[0]);
            isBuiltinArrayCtor = (constructor == scriptContext->GetLibrary()->GetArrayConstructor());

            Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(len, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            newObj = isTypedArrayEntryPoint ?
                TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), len, scriptContext) :
                JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);

            // If the new object we created is an array, remember that as it will save us time setting properties in the object below
            if (JavascriptArray::Is(newObj))
            {LOGMEIN("JavascriptArray.cpp] 10311\n");
                newArr = JavascriptArray::FromVar(newObj);
            }
            else if (TypedArrayBase::Is(newObj))
            {LOGMEIN("JavascriptArray.cpp] 10315\n");
                newTypedArray = TypedArrayBase::FromVar(newObj);
            }
        }
        else
        {
            // We only throw when the constructor property is not a constructor function in the TypedArray version
            if (isTypedArrayEntryPoint)
            {LOGMEIN("JavascriptArray.cpp] 10323\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, _u("[TypedArray].of"));
            }

            newArr = scriptContext->GetLibrary()->CreateArray(len);
            newArr->EnsureHead<Var>();
            newObj = newArr;
        }

        // At least we have a new object of some kind
        Assert(newObj);

        if (isBuiltinArrayCtor)
        {LOGMEIN("JavascriptArray.cpp] 10336\n");
            for (uint32 k = 0; k < len; k++)
            {LOGMEIN("JavascriptArray.cpp] 10338\n");
                Var kValue = args[k + 1];

                newArr->DirectSetItemAt(k, kValue);
            }
        }
        else if (newTypedArray)
        {LOGMEIN("JavascriptArray.cpp] 10345\n");
            for (uint32 k = 0; k < len; k++)
            {LOGMEIN("JavascriptArray.cpp] 10347\n");
                Var kValue = args[k + 1];

                newTypedArray->DirectSetItem(k, kValue);
            }
        }
        else
        {
            for (uint32 k = 0; k < len; k++)
            {LOGMEIN("JavascriptArray.cpp] 10356\n");
                Var kValue = args[k + 1];
                ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(RecyclableObject::FromVar(newObj), k, kValue), scriptContext, k);
            }
        }

        if (!isTypedArrayEntryPoint)
        {LOGMEIN("JavascriptArray.cpp] 10363\n");
            // Set length if we are in the Array version of the function
            JavascriptOperators::OP_SetProperty(newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(len, scriptContext), scriptContext, nullptr, PropertyOperation_ThrowIfNotExtensible);
        }

        return newObj;
    }

    JavascriptString* JavascriptArray::ToLocaleStringHelper(Var value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 10372\n");
        TypeId typeId = JavascriptOperators::GetTypeId(value);
        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {LOGMEIN("JavascriptArray.cpp] 10375\n");
            return scriptContext->GetLibrary()->GetEmptyString();
        }
        else
        {
            return JavascriptConversion::ToLocaleString(value, scriptContext);
        }
    }

    inline BOOL JavascriptArray::IsFullArray() const
    {LOGMEIN("JavascriptArray.cpp] 10385\n");
        if (head && head->length == length)
        {LOGMEIN("JavascriptArray.cpp] 10387\n");
            AssertMsg(head->next == 0 && head->left == 0, "Invalid Array");
            return true;
        }
        return (0 == length);
    }

    /*
    *   IsFillFromPrototypes
    *   -   Check the array has no missing values and only head segment.
    *   -   Also ensure if the lengths match.
    */
    bool JavascriptArray::IsFillFromPrototypes()
    {LOGMEIN("JavascriptArray.cpp] 10400\n");
        return !(this->head->next == nullptr && this->HasNoMissingValues() && this->length == this->head->length);
    }

    // Fill all missing value in the array and fill it from prototype between startIndex and limitIndex
    // typically startIndex = 0 and limitIndex = length. From start of the array till end of the array.
    void JavascriptArray::FillFromPrototypes(uint32 startIndex, uint32 limitIndex)
    {LOGMEIN("JavascriptArray.cpp] 10407\n");
        if (startIndex >= limitIndex)
        {LOGMEIN("JavascriptArray.cpp] 10409\n");
            return;
        }

        RecyclableObject* prototype = this->GetPrototype();

        // Fill all missing values by walking through prototype
        while (JavascriptOperators::GetTypeId(prototype) != TypeIds_Null)
        {
            ForEachOwnMissingArrayIndexOfObject(this, nullptr, prototype, startIndex, limitIndex,0, [this](uint32 index, Var value) {
                this->SetItem(index, value, PropertyOperation_None);
            });

            prototype = prototype->GetPrototype();
        }
#ifdef VALIDATE_ARRAY
        ValidateArray();
#endif
    }

    //
    // JavascriptArray requires head->left == 0 for fast path Get.
    //
    template<typename T>
    void JavascriptArray::EnsureHeadStartsFromZero(Recycler * recycler)
    {LOGMEIN("JavascriptArray.cpp] 10434\n");
        if (head == nullptr || head->left != 0)
        {LOGMEIN("JavascriptArray.cpp] 10436\n");
            // This is used to fix up altered arrays.
            // any SegmentMap would be invalid at this point.
            ClearSegmentMap();

            //
            // We could OOM and throw when allocating new empty head, resulting in a corrupted array. Need
            // some protection here. Save the head and switch this array to EmptySegment. Will be restored
            // correctly if allocating new segment succeeds.
            //
            SparseArraySegment<T>* savedHead = SparseArraySegment<T>::From(this->head);
            SparseArraySegment<T>* savedLastUsedSegment = (SparseArraySegment<T>*)this->GetLastUsedSegment();
            SetHeadAndLastUsedSegment(const_cast<SparseArraySegmentBase*>(EmptySegment));

            SparseArraySegment<T> *newSeg = SparseArraySegment<T>::AllocateSegment(recycler, 0, 0, savedHead);
            newSeg->next = savedHead;
            this->head = newSeg;
            SetHasNoMissingValues();
            this->SetLastUsedSegment(savedLastUsedSegment);
        }
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void JavascriptArray::CheckForceES5Array()
    {LOGMEIN("JavascriptArray.cpp] 10460\n");
        if (Configuration::Global.flags.ForceES5Array)
        {LOGMEIN("JavascriptArray.cpp] 10462\n");
            // There's a bad interaction with the jitted code for native array creation here.
            // ForceES5Array doesn't interact well with native arrays
            if (PHASE_OFF1(NativeArrayPhase))
            {LOGMEIN("JavascriptArray.cpp] 10466\n");
                GetTypeHandler()->ConvertToTypeWithItemAttributes(this);
            }
        }
    }
#endif

    template <typename T, typename Fn>
    void JavascriptArray::ForEachOwnMissingArrayIndexOfObject(JavascriptArray *baseArray, JavascriptArray *destArray, RecyclableObject* obj, uint32 startIndex, uint32 limitIndex, T destIndex, Fn fn)
    {LOGMEIN("JavascriptArray.cpp] 10475\n");
        Assert(DynamicObject::IsAnyArray(obj) || JavascriptOperators::IsObject(obj));

        Var oldValue;
        JavascriptArray* arr = nullptr;
        if (DynamicObject::IsAnyArray(obj))
        {LOGMEIN("JavascriptArray.cpp] 10481\n");
            arr = JavascriptArray::FromAnyArray(obj);
        }
        else if (DynamicType::Is(obj->GetTypeId()))
        {LOGMEIN("JavascriptArray.cpp] 10485\n");
            DynamicObject* dynobj = DynamicObject::FromVar(obj);
            ArrayObject* objectArray = dynobj->GetObjectArray();
            arr = (objectArray && JavascriptArray::IsAnyArray(objectArray)) ? JavascriptArray::FromAnyArray(objectArray) : nullptr;
        }

        if (arr != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 10492\n");
            if (JavascriptArray::Is(arr))
            {LOGMEIN("JavascriptArray.cpp] 10494\n");
                arr = EnsureNonNativeArray(arr);
                ArrayElementEnumerator e(arr, startIndex, limitIndex);

                while(e.MoveNext<Var>())
                {LOGMEIN("JavascriptArray.cpp] 10499\n");
                    uint32 index = e.GetIndex();
                    if (!baseArray->DirectGetVarItemAt(index, &oldValue, baseArray->GetScriptContext()))
                    {LOGMEIN("JavascriptArray.cpp] 10502\n");
                        T n = destIndex + (index - startIndex);
                        if (destArray == nullptr || !destArray->DirectGetItemAt(n, &oldValue))
                        {
                            fn(index, e.GetItem<Var>());
                        }
                    }
                }
            }
            else
            {
                ScriptContext* scriptContext = obj->GetScriptContext();

                Assert(ES5Array::Is(arr));

                ES5Array* es5Array = ES5Array::FromVar(arr);
                ES5ArrayIndexStaticEnumerator<true> e(es5Array);

                while (e.MoveNext())
                {LOGMEIN("JavascriptArray.cpp] 10521\n");
                    uint32 index = e.GetIndex();
                    if (index < startIndex) continue;
                    else if (index >= limitIndex) break;

                    if (!baseArray->DirectGetVarItemAt(index, &oldValue, baseArray->GetScriptContext()))
                    {LOGMEIN("JavascriptArray.cpp] 10527\n");
                        T n = destIndex + (index - startIndex);
                        if (destArray == nullptr || !destArray->DirectGetItemAt(n, &oldValue))
                        {LOGMEIN("JavascriptArray.cpp] 10530\n");
                            Var value = nullptr;
                            if (JavascriptOperators::GetOwnItem(obj, index, &value, scriptContext))
                            {
                                fn(index, value);
                            }
                        }
                    }
                }
            }
        }
    }

    //
    // ArrayElementEnumerator to enumerate array elements (not including elements from prototypes).
    //
    JavascriptArray::ArrayElementEnumerator::ArrayElementEnumerator(JavascriptArray* arr, uint32 start, uint32 end)
        : start(start), end(min(end, arr->length))
    {LOGMEIN("JavascriptArray.cpp] 10548\n");
        Init(arr);
    }

    //
    // Initialize this enumerator and prepare for the first MoveNext.
    //
    void JavascriptArray::ArrayElementEnumerator::Init(JavascriptArray* arr)
    {LOGMEIN("JavascriptArray.cpp] 10556\n");
        // Find start segment
        seg = (arr ? arr->GetBeginLookupSegment(start) : nullptr);
        while (seg && (seg->left + seg->length <= start))
        {LOGMEIN("JavascriptArray.cpp] 10560\n");
            seg = seg->next;
        }

        // Set start index and endIndex
        if (seg)
        {LOGMEIN("JavascriptArray.cpp] 10566\n");
            if (seg->left >= end)
            {LOGMEIN("JavascriptArray.cpp] 10568\n");
                seg = nullptr;
            }
            else
            {
                // set index to be at target index - 1, so MoveNext will move to target
                index = max(seg->left, start) - seg->left - 1;
                endIndex = min(end - seg->left, seg->length);
            }
        }
    }

    //
    // Move to the next element if available.
    //
    template<typename T>
    inline bool JavascriptArray::ArrayElementEnumerator::MoveNext()
    {LOGMEIN("JavascriptArray.cpp] 10585\n");
        while (seg)
        {LOGMEIN("JavascriptArray.cpp] 10587\n");
            // Look for next non-null item in current segment
            while (++index < endIndex)
            {LOGMEIN("JavascriptArray.cpp] 10590\n");
                if (!SparseArraySegment<T>::IsMissingItem(&((SparseArraySegment<T>*)seg)->elements[index]))
                {LOGMEIN("JavascriptArray.cpp] 10592\n");
                    return true;
                }
            }

            // Move to next segment
            seg = seg->next;
            if (seg)
            {LOGMEIN("JavascriptArray.cpp] 10600\n");
                if (seg->left >= end)
                {LOGMEIN("JavascriptArray.cpp] 10602\n");
                    seg = nullptr;
                    break;
                }
                else
                {
                    index = static_cast<uint32>(-1);
                    endIndex = min(end - seg->left, seg->length);
                }
            }
        }

        return false;
    }

    //
    // Get current array element index.
    //
    uint32 JavascriptArray::ArrayElementEnumerator::GetIndex() const
    {LOGMEIN("JavascriptArray.cpp] 10621\n");
        Assert(seg && index < seg->length && index < endIndex);
        return seg->left + index;
    }

    //
    // Get current array element value.
    //
    template<typename T>
    T JavascriptArray::ArrayElementEnumerator::GetItem() const
    {LOGMEIN("JavascriptArray.cpp] 10631\n");
        Assert(seg && index < seg->length && index < endIndex &&
               !SparseArraySegment<T>::IsMissingItem(&((SparseArraySegment<T>*)seg)->elements[index]));
        return ((SparseArraySegment<T>*)seg)->elements[index];
    }

    //
    // Construct a BigIndex initialized to a given uint32 (small index).
    //
    JavascriptArray::BigIndex::BigIndex(uint32 initIndex)
        : index(initIndex), bigIndex(InvalidIndex)
    {LOGMEIN("JavascriptArray.cpp] 10642\n");
        //ok if initIndex == InvalidIndex
    }

    //
    // Construct a BigIndex initialized to a given uint64 (large or small index).
    //
    JavascriptArray::BigIndex::BigIndex(uint64 initIndex)
        : index(InvalidIndex), bigIndex(initIndex)
    {LOGMEIN("JavascriptArray.cpp] 10651\n");
        if (bigIndex < InvalidIndex) // if it's actually small index
        {LOGMEIN("JavascriptArray.cpp] 10653\n");
            index = static_cast<uint32>(bigIndex);
            bigIndex = InvalidIndex;
        }
    }

    bool JavascriptArray::BigIndex::IsUint32Max() const
    {LOGMEIN("JavascriptArray.cpp] 10660\n");
        return index == InvalidIndex && bigIndex == InvalidIndex;
    }
    bool JavascriptArray::BigIndex::IsSmallIndex() const
    {LOGMEIN("JavascriptArray.cpp] 10664\n");
        return index < InvalidIndex;
    }

    uint32 JavascriptArray::BigIndex::GetSmallIndex() const
    {LOGMEIN("JavascriptArray.cpp] 10669\n");
        Assert(IsSmallIndex());
        return index;
    }

    uint64 JavascriptArray::BigIndex::GetBigIndex() const
    {LOGMEIN("JavascriptArray.cpp] 10675\n");
        Assert(!IsSmallIndex());
        return bigIndex;
    }
    //
    // Convert this index value to a JS number
    //
    Var JavascriptArray::BigIndex::ToNumber(ScriptContext* scriptContext) const
    {LOGMEIN("JavascriptArray.cpp] 10683\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10685\n");
            return small_index::ToNumber(index, scriptContext);
        }
        else
        {
            return JavascriptNumber::ToVar(bigIndex, scriptContext);
        }
    }

    //
    // Increment this index by 1.
    //
    const JavascriptArray::BigIndex& JavascriptArray::BigIndex::operator++()
    {LOGMEIN("JavascriptArray.cpp] 10698\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10700\n");
            ++index;
            // If index reaches InvalidIndex, we will start to use bigIndex which is initially InvalidIndex.
        }
        else
        {
            bigIndex = bigIndex + 1;
        }

        return *this;
    }

    //
    // Decrement this index by 1.
    //
    const JavascriptArray::BigIndex& JavascriptArray::BigIndex::operator--()
    {LOGMEIN("JavascriptArray.cpp] 10716\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10718\n");
            --index;
        }
        else
        {
            Assert(index == InvalidIndex && bigIndex >= InvalidIndex);

            --bigIndex;
            if (bigIndex < InvalidIndex)
            {LOGMEIN("JavascriptArray.cpp] 10727\n");
                index = InvalidIndex - 1;
                bigIndex = InvalidIndex;
            }
        }

        return *this;
    }

    JavascriptArray::BigIndex JavascriptArray::BigIndex::operator+(const BigIndex& delta) const
    {LOGMEIN("JavascriptArray.cpp] 10737\n");
        if (delta.IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10739\n");
            return operator+(delta.GetSmallIndex());
        }
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10743\n");
            return index + delta.GetBigIndex();
        }

        return bigIndex + delta.GetBigIndex();
    }

    //
    // Get a new BigIndex representing this + delta.
    //
    JavascriptArray::BigIndex JavascriptArray::BigIndex::operator+(uint32 delta) const
    {LOGMEIN("JavascriptArray.cpp] 10754\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10756\n");
            uint32 newIndex;
            if (UInt32Math::Add(index, delta, &newIndex))
            {LOGMEIN("JavascriptArray.cpp] 10759\n");
                return static_cast<uint64>(index) + static_cast<uint64>(delta);
            }
            else
            {
                return newIndex; // ok if newIndex == InvalidIndex
            }
        }
        else
        {
            return bigIndex + static_cast<uint64>(delta);
        }
    }

    bool JavascriptArray::BigIndex::operator==(const BigIndex& rhs) const
    {LOGMEIN("JavascriptArray.cpp] 10774\n");
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10776\n");
            return this->GetSmallIndex() == rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10780\n");
            // if lhs is big promote rhs
            return this->GetBigIndex() == (uint64) rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10785\n");
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) == rhs.GetBigIndex();
        }
        return this->GetBigIndex() == rhs.GetBigIndex();
    }

    bool JavascriptArray::BigIndex::operator> (const BigIndex& rhs) const
    {LOGMEIN("JavascriptArray.cpp] 10793\n");
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10795\n");
            return this->GetSmallIndex() > rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10799\n");
            // if lhs is big promote rhs
            return this->GetBigIndex() > (uint64)rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10804\n");
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) > rhs.GetBigIndex();
        }
        return this->GetBigIndex() > rhs.GetBigIndex();
    }

    bool JavascriptArray::BigIndex::operator< (const BigIndex& rhs) const
    {LOGMEIN("JavascriptArray.cpp] 10812\n");
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10814\n");
            return this->GetSmallIndex() < rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10818\n");
            // if lhs is big promote rhs
            return this->GetBigIndex() < (uint64)rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10823\n");
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) < rhs.GetBigIndex();
        }
        return this->GetBigIndex() < rhs.GetBigIndex();
    }

    bool JavascriptArray::BigIndex::operator<=(const BigIndex& rhs) const
    {LOGMEIN("JavascriptArray.cpp] 10831\n");
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10833\n");
            return this->GetSmallIndex() <= rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10837\n");
            // if lhs is big promote rhs
            return this->GetBigIndex() <= (uint64)rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && !this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10842\n");
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) <= rhs.GetBigIndex();
        }
        return this->GetBigIndex() <= rhs.GetBigIndex();
    }

    bool JavascriptArray::BigIndex::operator>=(const BigIndex& rhs) const
    {LOGMEIN("JavascriptArray.cpp] 10850\n");
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10852\n");
            return this->GetSmallIndex() >= rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10856\n");
            // if lhs is big promote rhs
            return this->GetBigIndex() >= (uint64)rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && this->IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10861\n");
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) >= rhs.GetBigIndex();
        }
        return this->GetBigIndex() >= rhs.GetBigIndex();
    }

    BOOL JavascriptArray::BigIndex::GetItem(JavascriptArray* arr, Var* outVal) const
    {LOGMEIN("JavascriptArray.cpp] 10869\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10871\n");
            return small_index::GetItem(arr, index, outVal);
        }
        else
        {
            ScriptContext* scriptContext = arr->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            return arr->GetProperty(arr, propertyRecord->GetPropertyId(), outVal, NULL, scriptContext);
        }
    }

    BOOL JavascriptArray::BigIndex::SetItem(JavascriptArray* arr, Var newValue) const
    {LOGMEIN("JavascriptArray.cpp] 10884\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10886\n");
            return small_index::SetItem(arr, index, newValue);
        }
        else
        {
            ScriptContext* scriptContext = arr->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            return arr->SetProperty(propertyRecord->GetPropertyId(), newValue, PropertyOperation_None, NULL);
        }
    }

    void JavascriptArray::BigIndex::SetItemIfNotExist(JavascriptArray* arr, Var newValue) const
    {LOGMEIN("JavascriptArray.cpp] 10899\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10901\n");
            small_index::SetItemIfNotExist(arr, index, newValue);
        }
        else
        {
            ScriptContext* scriptContext = arr->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            Var oldValue;
            PropertyId propertyId = propertyRecord->GetPropertyId();
            if (!arr->GetProperty(arr, propertyId, &oldValue, NULL, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 10912\n");
                arr->SetProperty(propertyId, newValue, PropertyOperation_None, NULL);
            }
        }
    }

    BOOL JavascriptArray::BigIndex::DeleteItem(JavascriptArray* arr) const
    {LOGMEIN("JavascriptArray.cpp] 10919\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10921\n");
            return small_index::DeleteItem(arr, index);
        }
        else
        {
            ScriptContext* scriptContext = arr->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            return arr->DeleteProperty(propertyRecord->GetPropertyId(), PropertyOperation_None);
        }
    }

    BOOL JavascriptArray::BigIndex::SetItem(RecyclableObject* obj, Var newValue, PropertyOperationFlags flags) const
    {LOGMEIN("JavascriptArray.cpp] 10934\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10936\n");
            return small_index::SetItem(obj, index, newValue, flags);
        }
        else
        {
            ScriptContext* scriptContext = obj->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            return JavascriptOperators::SetProperty(obj, obj, propertyRecord->GetPropertyId(), newValue, scriptContext, flags);
        }
    }

    BOOL JavascriptArray::BigIndex::DeleteItem(RecyclableObject* obj, PropertyOperationFlags flags) const
    {LOGMEIN("JavascriptArray.cpp] 10949\n");
        if (IsSmallIndex())
        {LOGMEIN("JavascriptArray.cpp] 10951\n");
            return small_index::DeleteItem(obj, index, flags);
        }
        else
        {
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, obj->GetScriptContext(), &propertyRecord);
            return JavascriptOperators::DeleteProperty(obj, propertyRecord->GetPropertyId(), flags);
        }
    }

    //
    // Truncate the array at start and clone the truncated span as properties starting at dstIndex (asserting dstIndex >= MaxArrayLength).
    //
    void JavascriptArray::TruncateToProperties(const BigIndex& dstIndex, uint32 start)
    {LOGMEIN("JavascriptArray.cpp] 10966\n");
        Assert(!dstIndex.IsSmallIndex());
        typedef IndexTrace<BigIndex> index_trace;

        BigIndex dst = dstIndex;
        uint32 i = start;

        ArrayElementEnumerator e(this, start);
        while(e.MoveNext<Var>())
        {LOGMEIN("JavascriptArray.cpp] 10975\n");
            // delete all items not enumerated
            while (i < e.GetIndex())
            {LOGMEIN("JavascriptArray.cpp] 10978\n");
                index_trace::DeleteItem(this, dst);
                ++i;
                ++dst;
            }

            // Copy over the item
            index_trace::SetItem(this, dst, e.GetItem<Var>());
            ++i;
            ++dst;
        }

        // Delete the rest till length
        while (i < this->length)
        {LOGMEIN("JavascriptArray.cpp] 10992\n");
            index_trace::DeleteItem(this, dst);
            ++i;
            ++dst;
        }

        // Elements moved, truncate the array at start
        SetLength(start);
    }

    //
    // Copy a srcArray elements (including elements from prototypes) to a dstArray starting from an index.
    //
    template<typename T>
    void JavascriptArray::InternalCopyArrayElements(JavascriptArray* dstArray, const T& dstIndex, JavascriptArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11007\n");
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<Var>())
        {LOGMEIN("JavascriptArray.cpp] 11015\n");
            T n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, e.GetItem<Var>());
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {
            InternalFillFromPrototype(dstArray, dstIndex, srcArray, start, end, count);
        }
    }

    //
    // Copy a srcArray elements (including elements from prototypes) to a dstArray starting from an index. If the index grows larger than
    // "array index", it will automatically turn to SetProperty using the index as property name.
    //
    void JavascriptArray::CopyArrayElements(JavascriptArray* dstArray, const BigIndex& dstIndex, JavascriptArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11033\n");
        end = min(end, srcArray->length);
        if (start < end)
        {LOGMEIN("JavascriptArray.cpp] 11036\n");
            uint32 len = end - start;
            if (dstIndex.IsSmallIndex() && (len < MaxArrayLength - dstIndex.GetSmallIndex()))
            {
                // Won't overflow, use faster small_index version
                InternalCopyArrayElements(dstArray, dstIndex.GetSmallIndex(), srcArray, start, end);
            }
            else
            {
                InternalCopyArrayElements(dstArray, dstIndex, srcArray, start, end);
            }
        }
    }

    //
    // Faster small_index overload of CopyArrayElements, asserting the uint32 dstIndex won't overflow.
    //
    void JavascriptArray::CopyArrayElements(JavascriptArray* dstArray, uint32 dstIndex, JavascriptArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11054\n");
        end = min(end, srcArray->length);
        if (start < end)
        {LOGMEIN("JavascriptArray.cpp] 11057\n");
            Assert(end - start <= MaxArrayLength - dstIndex);
            InternalCopyArrayElements(dstArray, dstIndex, srcArray, start, end);
        }
    }

    template <typename T>
    void JavascriptArray::CopyAnyArrayElementsToVar(JavascriptArray* dstArray, T dstIndex, JavascriptArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11065\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(srcArray);
#endif
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(dstArray);
#endif
        if (JavascriptNativeIntArray::Is(srcArray))
        {
            CopyNativeIntArrayElementsToVar(dstArray, dstIndex, JavascriptNativeIntArray::FromVar(srcArray), start, end);
        }
        else if (JavascriptNativeFloatArray::Is(srcArray))
        {
            CopyNativeFloatArrayElementsToVar(dstArray, dstIndex, JavascriptNativeFloatArray::FromVar(srcArray), start, end);
        }
        else
        {
            CopyArrayElements(dstArray, dstIndex, srcArray, start, end);
        }
    }

    void JavascriptArray::CopyNativeIntArrayElementsToVar(JavascriptArray* dstArray, const BigIndex& dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11087\n");
        end = min(end, srcArray->length);
        if (start < end)
        {LOGMEIN("JavascriptArray.cpp] 11090\n");
            uint32 len = end - start;
            if (dstIndex.IsSmallIndex() && (len < MaxArrayLength - dstIndex.GetSmallIndex()))
            {
                // Won't overflow, use faster small_index version
                InternalCopyNativeIntArrayElements(dstArray, dstIndex.GetSmallIndex(), srcArray, start, end);
            }
            else
            {
                InternalCopyNativeIntArrayElements(dstArray, dstIndex, srcArray, start, end);
            }
        }
    }

    //
    // Faster small_index overload of CopyArrayElements, asserting the uint32 dstIndex won't overflow.
    //
    void JavascriptArray::CopyNativeIntArrayElementsToVar(JavascriptArray* dstArray, uint32 dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11108\n");
        end = min(end, srcArray->length);
        if (start < end)
        {LOGMEIN("JavascriptArray.cpp] 11111\n");
            Assert(end - start <= MaxArrayLength - dstIndex);
            InternalCopyNativeIntArrayElements(dstArray, dstIndex, srcArray, start, end);
        }
    }

    bool JavascriptArray::CopyNativeIntArrayElements(JavascriptNativeIntArray* dstArray, uint32 dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11118\n");
        end = min(end, srcArray->length);
        if (start >= end)
        {LOGMEIN("JavascriptArray.cpp] 11121\n");
            return false;
        }

        Assert(end - start <= MaxArrayLength - dstIndex);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<int32>())
        {LOGMEIN("JavascriptArray.cpp] 11133\n");
            uint n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, e.GetItem<int32>());
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {LOGMEIN("JavascriptArray.cpp] 11141\n");
            JavascriptArray *varArray = JavascriptNativeIntArray::ToVarArray(dstArray);
            InternalFillFromPrototype(varArray, dstIndex, srcArray, start, end, count);
            return true;
        }

        return false;
    }

    bool JavascriptArray::CopyNativeIntArrayElementsToFloat(JavascriptNativeFloatArray* dstArray, uint32 dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11151\n");
        end = min(end, srcArray->length);
        if (start >= end)
        {LOGMEIN("JavascriptArray.cpp] 11154\n");
            return false;
        }

        Assert(end - start <= MaxArrayLength - dstIndex);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<int32>())
        {LOGMEIN("JavascriptArray.cpp] 11166\n");
            uint n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, (double)e.GetItem<int32>());
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {LOGMEIN("JavascriptArray.cpp] 11174\n");
            JavascriptArray *varArray = JavascriptNativeFloatArray::ToVarArray(dstArray);
            InternalFillFromPrototype(varArray, dstIndex, srcArray, start, end, count);
            return true;
        }

        return false;
    }

    void JavascriptArray::CopyNativeFloatArrayElementsToVar(JavascriptArray* dstArray, const BigIndex& dstIndex, JavascriptNativeFloatArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11184\n");
        end = min(end, srcArray->length);
        if (start < end)
        {LOGMEIN("JavascriptArray.cpp] 11187\n");
            uint32 len = end - start;
            if (dstIndex.IsSmallIndex() && (len < MaxArrayLength - dstIndex.GetSmallIndex()))
            {
                // Won't overflow, use faster small_index version
                InternalCopyNativeFloatArrayElements(dstArray, dstIndex.GetSmallIndex(), srcArray, start, end);
            }
            else
            {
                InternalCopyNativeFloatArrayElements(dstArray, dstIndex, srcArray, start, end);
            }
        }
    }

    //
    // Faster small_index overload of CopyArrayElements, asserting the uint32 dstIndex won't overflow.
    //
    void JavascriptArray::CopyNativeFloatArrayElementsToVar(JavascriptArray* dstArray, uint32 dstIndex, JavascriptNativeFloatArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11205\n");
        end = min(end, srcArray->length);
        if (start < end)
        {LOGMEIN("JavascriptArray.cpp] 11208\n");
            Assert(end - start <= MaxArrayLength - dstIndex);
            InternalCopyNativeFloatArrayElements(dstArray, dstIndex, srcArray, start, end);
        }
    }

    bool JavascriptArray::CopyNativeFloatArrayElements(JavascriptNativeFloatArray* dstArray, uint32 dstIndex, JavascriptNativeFloatArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11215\n");
        end = min(end, srcArray->length);
        if (start >= end)
        {LOGMEIN("JavascriptArray.cpp] 11218\n");
            return false;
        }

        Assert(end - start <= MaxArrayLength - dstIndex);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<double>())
        {LOGMEIN("JavascriptArray.cpp] 11230\n");
            uint n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, e.GetItem<double>());
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {LOGMEIN("JavascriptArray.cpp] 11238\n");
            JavascriptArray *varArray = JavascriptNativeFloatArray::ToVarArray(dstArray);
            InternalFillFromPrototype(varArray, dstIndex, srcArray, start, end, count);
            return true;
        }

        return false;
    }

    JavascriptArray *JavascriptArray::EnsureNonNativeArray(JavascriptArray *arr)
    {LOGMEIN("JavascriptArray.cpp] 11248\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arr);
#endif
        if (JavascriptNativeIntArray::Is(arr))
        {LOGMEIN("JavascriptArray.cpp] 11253\n");
            arr = JavascriptNativeIntArray::ToVarArray((JavascriptNativeIntArray*)arr);
        }
        else if (JavascriptNativeFloatArray::Is(arr))
        {LOGMEIN("JavascriptArray.cpp] 11257\n");
            arr = JavascriptNativeFloatArray::ToVarArray((JavascriptNativeFloatArray*)arr);
        }

        return arr;
    }

    BOOL JavascriptNativeIntArray::DirectGetItemAtFull(uint32 index, Var* outVal)
    {LOGMEIN("JavascriptArray.cpp] 11265\n");
        ScriptContext* requestContext = type->GetScriptContext();
        if (JavascriptNativeIntArray::GetItem(this, index, outVal, requestContext))
        {LOGMEIN("JavascriptArray.cpp] 11268\n");
            return TRUE;
        }

        return JavascriptOperators::GetItem(this, this->GetPrototype(), index, outVal, requestContext);
    }

    BOOL JavascriptNativeFloatArray::DirectGetItemAtFull(uint32 index, Var* outVal)
    {LOGMEIN("JavascriptArray.cpp] 11276\n");
        ScriptContext* requestContext = type->GetScriptContext();
        if (JavascriptNativeFloatArray::GetItem(this, index, outVal, requestContext))
        {LOGMEIN("JavascriptArray.cpp] 11279\n");
            return TRUE;
        }

        return JavascriptOperators::GetItem(this, this->GetPrototype(), index, outVal, requestContext);
    }

    template<typename T>
    void JavascriptArray::InternalCopyNativeIntArrayElements(JavascriptArray* dstArray, const T& dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11288\n");
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ScriptContext *scriptContext = dstArray->GetScriptContext();
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<int32>())
        {LOGMEIN("JavascriptArray.cpp] 11297\n");
            T n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, JavascriptNumber::ToVar(e.GetItem<int32>(), scriptContext));
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {
            InternalFillFromPrototype(dstArray, dstIndex, srcArray, start, end, count);
        }
    }

    template<typename T>
    void JavascriptArray::InternalCopyNativeFloatArrayElements(JavascriptArray* dstArray, const T& dstIndex, JavascriptNativeFloatArray* srcArray, uint32 start, uint32 end)
    {LOGMEIN("JavascriptArray.cpp] 11312\n");
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ScriptContext *scriptContext = dstArray->GetScriptContext();
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<double>())
        {LOGMEIN("JavascriptArray.cpp] 11321\n");
            T n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, JavascriptNumber::ToVarWithCheck(e.GetItem<double>(), scriptContext));
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {
            InternalFillFromPrototype(dstArray, dstIndex, srcArray, start, end, count);
        }
    }

    Var JavascriptArray::SpreadArrayArgs(Var arrayToSpread, const Js::AuxArray<uint32> *spreadIndices, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 11335\n");
        // At this stage we have an array literal with some arguments to be spread.
        // First we need to calculate the real size of the final literal.
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayToSpread);
#endif
        JavascriptArray *array = FromVar(arrayToSpread);
        uint32 actualLength = array->GetLength();

        for (unsigned i = 0; i < spreadIndices->count; ++i)
        {LOGMEIN("JavascriptArray.cpp] 11345\n");
            actualLength = UInt32Math::Add(actualLength - 1, GetSpreadArgLen(array->DirectGetItem(spreadIndices->elements[i]), scriptContext));
        }

        JavascriptArray *result = FromVar(OP_NewScArrayWithMissingValues(actualLength, scriptContext));

        // Now we copy each element and expand the spread parameters inline.
        for (unsigned i = 0, spreadArrIndex = 0, resultIndex = 0; i < array->GetLength() && resultIndex < actualLength; ++i)
        {LOGMEIN("JavascriptArray.cpp] 11353\n");
            uint32 spreadIndex = spreadIndices->elements[spreadArrIndex]; // The index of the next element to be spread.

            // An array needs a slow copy if it is a cross-site object or we have missing values that need to be set to undefined.
            auto needArraySlowCopy = [&](Var instance) {LOGMEIN("JavascriptArray.cpp] 11357\n");
                if (JavascriptArray::Is(instance))
                {LOGMEIN("JavascriptArray.cpp] 11359\n");
                    JavascriptArray *arr = JavascriptArray::FromVar(instance);
                    return arr->IsCrossSiteObject() || arr->IsFillFromPrototypes();
                }
                return false;
            };

            // Designed to have interchangeable arguments with CopyAnyArrayElementsToVar.
            auto slowCopy = [&scriptContext, &needArraySlowCopy](JavascriptArray *dstArray, unsigned dstIndex, Var srcArray, uint32 start, uint32 end) {LOGMEIN("JavascriptArray.cpp] 11367\n");
                Assert(needArraySlowCopy(srcArray) || ArgumentsObject::Is(srcArray) || TypedArrayBase::Is(srcArray) || JavascriptString::Is(srcArray));

                RecyclableObject *propertyObject;
                if (!JavascriptOperators::GetPropertyObject(srcArray, scriptContext, &propertyObject))
                {LOGMEIN("JavascriptArray.cpp] 11372\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidSpreadArgument);
                }

                for (uint32 j = start; j < end; j++)
                {LOGMEIN("JavascriptArray.cpp] 11377\n");
                    Var element;
                    if (!JavascriptOperators::GetItem(srcArray, propertyObject, j, &element, scriptContext))
                    {LOGMEIN("JavascriptArray.cpp] 11380\n");
                        // Copy across missing values as undefined as per 12.2.5.2 SpreadElement : ... AssignmentExpression 5f.
                        element = scriptContext->GetLibrary()->GetUndefined();
                    }
                    dstArray->DirectSetItemAt(dstIndex++, element);
                }
            };

            if (i < spreadIndex)
            {LOGMEIN("JavascriptArray.cpp] 11389\n");
                // Any non-spread elements can be copied in bulk.

                if (needArraySlowCopy(array))
                {
                    slowCopy(result, resultIndex, (Var)array, i, spreadIndex);
                }
                else
                {
                    CopyAnyArrayElementsToVar(result, resultIndex, array, i, spreadIndex);
                }
                resultIndex += spreadIndex - i;
                i = spreadIndex - 1;
                continue;
            }
            else if (i > spreadIndex)
            {LOGMEIN("JavascriptArray.cpp] 11405\n");
                // Any non-spread elements terminating the array can also be copied in bulk.
                Assert(spreadArrIndex == spreadIndices->count - 1);
                if (needArraySlowCopy(array))
                {
                    slowCopy(result, resultIndex, array, i, array->GetLength());
                }
                else
                {
                    CopyAnyArrayElementsToVar(result, resultIndex, array, i, array->GetLength());
                }
                break;
            }
            else
            {
                Var instance = array->DirectGetItem(i);

                if (SpreadArgument::Is(instance))
                {LOGMEIN("JavascriptArray.cpp] 11423\n");
                    SpreadArgument* spreadArgument = SpreadArgument::FromVar(instance);
                    uint32 len = spreadArgument->GetArgumentSpreadCount();
                    const Var*  spreadItems = spreadArgument->GetArgumentSpread();
                    for (uint32 j = 0; j < len; j++)
                    {LOGMEIN("JavascriptArray.cpp] 11428\n");
                        result->DirectSetItemAt(resultIndex++, spreadItems[j]);
                    }

                }
                else
                {
                    AssertMsg(JavascriptArray::Is(instance) || TypedArrayBase::Is(instance), "Only SpreadArgument, TypedArray, and JavascriptArray should be listed as spread arguments");

                    // We first try to interpret the spread parameter as a JavascriptArray.
                    JavascriptArray *arr = nullptr;
                    if (JavascriptArray::Is(instance))
                    {LOGMEIN("JavascriptArray.cpp] 11440\n");
                        arr = JavascriptArray::FromVar(instance);
                    }

                    if (arr != nullptr)
                    {LOGMEIN("JavascriptArray.cpp] 11445\n");
                        if (arr->GetLength() > 0)
                        {LOGMEIN("JavascriptArray.cpp] 11447\n");
                            if (needArraySlowCopy(arr))
                            {
                                slowCopy(result, resultIndex, arr, 0, arr->GetLength());
                            }
                            else
                            {
                                CopyAnyArrayElementsToVar(result, resultIndex, arr, 0, arr->GetLength());
                            }
                            resultIndex += arr->GetLength();
                        }
                    }
                    else
                    {
                        uint32 len = GetSpreadArgLen(instance, scriptContext);
                        slowCopy(result, resultIndex, instance, 0, len);
                        resultIndex += len;
                    }
                }

                if (spreadArrIndex < spreadIndices->count - 1)
                {LOGMEIN("JavascriptArray.cpp] 11468\n");
                    spreadArrIndex++;
                }
            }
        }
        return result;
    }

    uint32 JavascriptArray::GetSpreadArgLen(Var spreadArg, ScriptContext *scriptContext)
    {LOGMEIN("JavascriptArray.cpp] 11477\n");
        // A spread argument can be anything that returns a 'length' property, even if that
        // property is null or undefined.
        spreadArg = CrossSite::MarshalVar(scriptContext, spreadArg);
        if (JavascriptArray::Is(spreadArg))
        {LOGMEIN("JavascriptArray.cpp] 11482\n");
            JavascriptArray *arr = JavascriptArray::FromVar(spreadArg);
            return arr->GetLength();
        }

        if (TypedArrayBase::Is(spreadArg))
        {LOGMEIN("JavascriptArray.cpp] 11488\n");
            TypedArrayBase *tarr = TypedArrayBase::FromVar(spreadArg);
            return tarr->GetLength();
        }

        if (SpreadArgument::Is(spreadArg))
        {LOGMEIN("JavascriptArray.cpp] 11494\n");
            SpreadArgument *spreadFunctionArgs = SpreadArgument::FromVar(spreadArg);
            return spreadFunctionArgs->GetArgumentSpreadCount();
        }

        AssertMsg(false, "LdCustomSpreadIteratorList should have converted the arg to one of the above types");
        Throw::FatalInternalError();
    }

#ifdef VALIDATE_ARRAY
    class ArraySegmentsVisitor
    {
    private:
        SparseArraySegmentBase* seg;

    public:
        ArraySegmentsVisitor(SparseArraySegmentBase* head)
            : seg(head)
        {LOGMEIN("JavascriptArray.cpp] 11512\n");
        }

        void operator()(SparseArraySegmentBase* s)
        {LOGMEIN("JavascriptArray.cpp] 11516\n");
            Assert(seg == s);
            if (seg)
            {LOGMEIN("JavascriptArray.cpp] 11519\n");
                seg = seg->next;
            }
        }
    };

    void JavascriptArray::ValidateArrayCommon()
    {LOGMEIN("JavascriptArray.cpp] 11526\n");
        SparseArraySegmentBase * lastUsedSegment = this->GetLastUsedSegment();
        AssertMsg(this != nullptr && head && lastUsedSegment, "Array should not be null");
        AssertMsg(head->left == 0, "Array always should have a segment starting at zero");

        // Simple segments validation
        bool foundLastUsedSegment = false;
        SparseArraySegmentBase *seg = head;
        while(seg != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 11535\n");
            if (seg == lastUsedSegment)
            {LOGMEIN("JavascriptArray.cpp] 11537\n");
                foundLastUsedSegment = true;
            }

            AssertMsg(seg->length <= seg->size , "Length greater than size not possible");

            SparseArraySegmentBase* next = seg->next;
            if (next != nullptr)
            {LOGMEIN("JavascriptArray.cpp] 11545\n");
                AssertMsg(seg->left < next->left, "Segment is adjacent to or overlaps with next segment");
                AssertMsg(seg->size <= (next->left - seg->left), "Segment is adjacent to or overlaps with next segment");
                AssertMsg(!SparseArraySegmentBase::IsLeafSegment(seg, this->GetScriptContext()->GetRecycler()), "Leaf segment with a next pointer");
            }
            else
            {
                AssertMsg(seg->length <= MaxArrayLength - seg->left, "Segment index range overflow");
                AssertMsg(seg->left + seg->length <= this->length, "Segment index range exceeds array length");
            }

            seg = next;
        }
        AssertMsg(foundLastUsedSegment || HasSegmentMap(), "Corrupt lastUsedSegment in array header");

        // Validate segmentMap if present
        if (HasSegmentMap())
        {LOGMEIN("JavascriptArray.cpp] 11562\n");
            ArraySegmentsVisitor visitor(head);
            GetSegmentMap()->Walk(visitor);
        }
    }

    void JavascriptArray::ValidateArray()
    {LOGMEIN("JavascriptArray.cpp] 11569\n");
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {LOGMEIN("JavascriptArray.cpp] 11571\n");
            return;
        }
        ValidateArrayCommon();
        // Detailed segments validation
        JavascriptArray::ValidateVarSegment(SparseArraySegment<Var>::From(head));
    }

    void JavascriptNativeIntArray::ValidateArray()
    {LOGMEIN("JavascriptArray.cpp] 11580\n");
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {LOGMEIN("JavascriptArray.cpp] 11582\n");
#if DBG
            SparseArraySegmentBase *seg = head;
            while (seg)
            {LOGMEIN("JavascriptArray.cpp] 11586\n");
                if (seg->next != nullptr)
                {LOGMEIN("JavascriptArray.cpp] 11588\n");
                    AssertMsg(!SparseArraySegmentBase::IsLeafSegment(seg, this->GetScriptContext()->GetRecycler()), "Leaf segment with a next pointer");
                }
                seg = seg->next;
            }
#endif
            return;
        }
        ValidateArrayCommon();
        // Detailed segments validation
        JavascriptArray::ValidateSegment<int32>(SparseArraySegment<int32>::From(head));
    }

    void JavascriptNativeFloatArray::ValidateArray()
    {LOGMEIN("JavascriptArray.cpp] 11602\n");
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {LOGMEIN("JavascriptArray.cpp] 11604\n");
#if DBG
            SparseArraySegmentBase *seg = head;
            while (seg)
            {LOGMEIN("JavascriptArray.cpp] 11608\n");
                if (seg->next != nullptr)
                {LOGMEIN("JavascriptArray.cpp] 11610\n");
                    AssertMsg(!SparseArraySegmentBase::IsLeafSegment(seg, this->GetScriptContext()->GetRecycler()), "Leaf segment with a next pointer");
                }
                seg = seg->next;
            }
#endif
            return;
        }
        ValidateArrayCommon();
        // Detailed segments validation
        JavascriptArray::ValidateSegment<double>(SparseArraySegment<double>::From(head));
    }


    void JavascriptArray::ValidateVarSegment(SparseArraySegment<Var>* seg)
    {LOGMEIN("JavascriptArray.cpp] 11625\n");
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {LOGMEIN("JavascriptArray.cpp] 11627\n");
            return;
        }
        int32 inspect;
        double inspectDouble;
        while (seg)
        {LOGMEIN("JavascriptArray.cpp] 11633\n");
            uint32 i = 0;
            for (i = 0; i < seg->length; i++)
            {LOGMEIN("JavascriptArray.cpp] 11636\n");
                if (SparseArraySegment<Var>::IsMissingItem(&seg->elements[i]))
                {LOGMEIN("JavascriptArray.cpp] 11638\n");
                    continue;
                }
                if (TaggedInt::Is(seg->elements[i]))
                {LOGMEIN("JavascriptArray.cpp] 11642\n");
                    inspect = TaggedInt::ToInt32(seg->elements[i]);

                }
                else if (JavascriptNumber::Is_NoTaggedIntCheck(seg->elements[i]))
                {LOGMEIN("JavascriptArray.cpp] 11647\n");
                    inspectDouble = JavascriptNumber::GetValue(seg->elements[i]);
                }
                else
                {
                    AssertMsg(RecyclableObject::Is(seg->elements[i]), "Invalid entry in segment");
                }
            }
            ValidateSegment(seg);

            seg = SparseArraySegment<Var>::From(seg->next);
        }
    }

    template<typename T>
    void JavascriptArray::ValidateSegment(SparseArraySegment<T>* seg)
    {LOGMEIN("JavascriptArray.cpp] 11663\n");
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {LOGMEIN("JavascriptArray.cpp] 11665\n");
            return;
        }

        while (seg)
        {LOGMEIN("JavascriptArray.cpp] 11670\n");
            uint32 i = seg->length;
            while (i < seg->size)
            {LOGMEIN("JavascriptArray.cpp] 11673\n");
                AssertMsg(SparseArraySegment<T>::IsMissingItem(&seg->elements[i]), "Non missing value the end of the segment");
                i++;
            }

            seg = SparseArraySegment<T>::From(seg->next);
        }
    }
#endif

    template <typename T>
    void JavascriptArray::InitBoxedInlineHeadSegment(SparseArraySegment<T> * dst, SparseArraySegment<T> * src)
    {LOGMEIN("JavascriptArray.cpp] 11685\n");
        // Don't copy the segment map, we will build it again
        SetFlags(GetFlags() & ~DynamicObjectFlags::HasSegmentMap);

        SetHeadAndLastUsedSegment(dst);

        dst->left = src->left;
        dst->length = src->length;
        dst->size = src->size;
        dst->next = src->next;

        CopyArray(dst->elements, dst->size, src->elements, src->size);
    }

    JavascriptArray::JavascriptArray(JavascriptArray * instance, bool boxHead)
        : ArrayObject(instance)
    {LOGMEIN("JavascriptArray.cpp] 11701\n");
        if (boxHead)
        {
            InitBoxedInlineHeadSegment(DetermineInlineHeadSegmentPointer<JavascriptArray, 0, true>(this), SparseArraySegment<Var>::From(instance->head));
        }
        else
        {

            SetFlags(GetFlags() & ~DynamicObjectFlags::HasSegmentMap);
            head = instance->head;
            SetLastUsedSegment(instance->GetLastUsedSegment());
        }
    }

    template <typename T>
    T * JavascriptArray::BoxStackInstance(T * instance)
    {LOGMEIN("JavascriptArray.cpp] 11717\n");
        Assert(ThreadContext::IsOnStack(instance));
        // On the stack, the we reserved a pointer before the object as to store the boxed value
        T ** boxedInstanceRef = ((T **)instance) - 1;
        T * boxedInstance = *boxedInstanceRef;
        if (boxedInstance)
        {LOGMEIN("JavascriptArray.cpp] 11723\n");
            return boxedInstance;
        }

        const size_t inlineSlotsSize = instance->GetTypeHandler()->GetInlineSlotsSize();
        if (ThreadContext::IsOnStack(instance->head))
        {LOGMEIN("JavascriptArray.cpp] 11729\n");
            boxedInstance = RecyclerNewPlusZ(instance->GetRecycler(),
                inlineSlotsSize + sizeof(Js::SparseArraySegmentBase) + instance->head->size * sizeof(typename T::TElement),
                T, instance, true);
        }
        else if(inlineSlotsSize)
        {LOGMEIN("JavascriptArray.cpp] 11735\n");
            boxedInstance = RecyclerNewPlusZ(instance->GetRecycler(), inlineSlotsSize, T, instance, false);
        }
        else
        {
            boxedInstance = RecyclerNew(instance->GetRecycler(), T, instance, false);
        }

        *boxedInstanceRef = boxedInstance;
        return boxedInstance;
    }

    JavascriptArray *
    JavascriptArray::BoxStackInstance(JavascriptArray * instance)
    {LOGMEIN("JavascriptArray.cpp] 11749\n");
        return BoxStackInstance<JavascriptArray>(instance);
    }

#if ENABLE_TTD
    void JavascriptArray::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptArray.cpp] 11755\n");
        TTDAssert(this->GetTypeId() == Js::TypeIds_Array || this->GetTypeId() == Js::TypeIds_ES5Array, "Should only be used on basic arrays (or called as super from ES5Array.");

        ScriptContext* ctx = this->GetScriptContext();

        uint32 index = Js::JavascriptArray::InvalidIndex;
        while(true)
        {LOGMEIN("JavascriptArray.cpp] 11762\n");
            index = this->GetNextIndex(index);
            if(index == Js::JavascriptArray::InvalidIndex) // End of array
            {LOGMEIN("JavascriptArray.cpp] 11765\n");
                break;
            }

            Js::Var aval = nullptr;
            if(this->DirectGetVarItemAt(index, &aval, ctx))
            {LOGMEIN("JavascriptArray.cpp] 11771\n");
                extractor->MarkVisitVar(aval);
            }
        }
    }

    void JavascriptArray::ProcessCorePaths()
    {LOGMEIN("JavascriptArray.cpp] 11778\n");
        TTDAssert(this->GetTypeId() == Js::TypeIds_Array, "Should only be used on basic arrays.");

        ScriptContext* ctx = this->GetScriptContext();

        uint32 index = Js::JavascriptArray::InvalidIndex;
        while(true)
        {LOGMEIN("JavascriptArray.cpp] 11785\n");
            index = this->GetNextIndex(index);
            if(index == Js::JavascriptArray::InvalidIndex) // End of array
            {LOGMEIN("JavascriptArray.cpp] 11788\n");
                break;
            }

            Js::Var aval = nullptr;
            if(this->DirectGetVarItemAt(index, &aval, ctx))
            {LOGMEIN("JavascriptArray.cpp] 11794\n");
                TTD::UtilSupport::TTAutoString pathExt;
                ctx->TTDWellKnownInfo->BuildArrayIndexBuffer(index, pathExt);

                ctx->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, aval, pathExt.GetStrValue());
            }
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptArray::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptArray.cpp] 11804\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapArrayObject;
    }

    void JavascriptArray::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptArray.cpp] 11809\n");
        TTDAssert(this->GetTypeId() == Js::TypeIds_Array, "Should only be used on basic Js arrays.");

        TTD::NSSnapObjects::SnapArrayInfo<TTD::TTDVar>* sai = TTD::NSSnapObjects::ExtractArrayValues<TTD::TTDVar>(this, alloc);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayInfo<TTD::TTDVar>*, TTD::NSSnapObjects::SnapObjectType::SnapArrayObject>(objData, sai);
    }
#endif

    JavascriptNativeArray::JavascriptNativeArray(JavascriptNativeArray * instance) :
        JavascriptArray(instance, false),
        weakRefToFuncBody(instance->weakRefToFuncBody)
    {LOGMEIN("JavascriptArray.cpp] 11820\n");
    }

    JavascriptNativeIntArray::JavascriptNativeIntArray(JavascriptNativeIntArray * instance, bool boxHead) :
        JavascriptNativeArray(instance)
    {LOGMEIN("JavascriptArray.cpp] 11825\n");
        if (boxHead)
        {
            InitBoxedInlineHeadSegment(DetermineInlineHeadSegmentPointer<JavascriptNativeIntArray, 0, true>(this), SparseArraySegment<int>::From(instance->head));
        }
        else
        {
            // Base class ctor should have copied these
            Assert(head == instance->head);
            Assert(segmentUnion.lastUsedSegment == instance->GetLastUsedSegment());
        }
    }

    JavascriptNativeIntArray *
    JavascriptNativeIntArray::BoxStackInstance(JavascriptNativeIntArray * instance)
    {LOGMEIN("JavascriptArray.cpp] 11840\n");
        return JavascriptArray::BoxStackInstance<JavascriptNativeIntArray>(instance);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptNativeIntArray::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptArray.cpp] 11846\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapNativeIntArrayObject;
    }

    void JavascriptNativeIntArray::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptArray.cpp] 11851\n");
        TTD::NSSnapObjects::SnapArrayInfo<int32>* sai = TTD::NSSnapObjects::ExtractArrayValues<int32>(this, alloc);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayInfo<int32>*, TTD::NSSnapObjects::SnapObjectType::SnapNativeIntArrayObject>(objData, sai);
    }

#if ENABLE_COPYONACCESS_ARRAY
    TTD::NSSnapObjects::SnapObjectType JavascriptCopyOnAccessNativeIntArray::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptArray.cpp] 11858\n");
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void JavascriptCopyOnAccessNativeIntArray::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Not implemented yet!!!");
    }
#endif
#endif

    JavascriptNativeFloatArray::JavascriptNativeFloatArray(JavascriptNativeFloatArray * instance, bool boxHead) :
        JavascriptNativeArray(instance)
    {LOGMEIN("JavascriptArray.cpp] 11871\n");
        if (boxHead)
        {
            InitBoxedInlineHeadSegment(DetermineInlineHeadSegmentPointer<JavascriptNativeFloatArray, 0, true>(this), SparseArraySegment<double>::From(instance->head));
        }
        else
        {
            // Base class ctor should have copied these
            Assert(head == instance->head);
            Assert(segmentUnion.lastUsedSegment == instance->GetLastUsedSegment());
        }
    }

    JavascriptNativeFloatArray *
    JavascriptNativeFloatArray::BoxStackInstance(JavascriptNativeFloatArray * instance)
    {LOGMEIN("JavascriptArray.cpp] 11886\n");
        return JavascriptArray::BoxStackInstance<JavascriptNativeFloatArray>(instance);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptNativeFloatArray::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptArray.cpp] 11892\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapNativeFloatArrayObject;
    }

    void JavascriptNativeFloatArray::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptArray.cpp] 11897\n");
        TTDAssert(this->GetTypeId() == Js::TypeIds_NativeFloatArray, "Should only be used on native float arrays.");

        TTD::NSSnapObjects::SnapArrayInfo<double>* sai = TTD::NSSnapObjects::ExtractArrayValues<double>(this, alloc);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayInfo<double>*, TTD::NSSnapObjects::SnapObjectType::SnapNativeFloatArrayObject>(objData, sai);
    }
#endif

    template<typename T>
    RecyclableObject*
    JavascriptArray::ArraySpeciesCreate(Var originalArray, T length, ScriptContext* scriptContext, bool *pIsIntArray, bool *pIsFloatArray, bool *pIsBuiltinArrayCtor)
    {LOGMEIN("JavascriptArray.cpp] 11908\n");
        if (originalArray == nullptr || !scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {LOGMEIN("JavascriptArray.cpp] 11910\n");
            return nullptr;
        }

        if (JavascriptArray::Is(originalArray)
            && !DynamicObject::FromVar(originalArray)->GetDynamicType()->GetTypeHandler()->GetIsNotPathTypeHandlerOrHasUserDefinedCtor()
            && DynamicObject::FromVar(originalArray)->GetPrototype() == scriptContext->GetLibrary()->GetArrayPrototype()
            && !scriptContext->GetLibrary()->GetArrayObjectHasUserDefinedSpecies())
        {LOGMEIN("JavascriptArray.cpp] 11918\n");
            return nullptr;
        }

        Var constructor = scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptOperators::IsArray(originalArray))
        {LOGMEIN("JavascriptArray.cpp] 11925\n");
            if (!JavascriptOperators::GetProperty(RecyclableObject::FromVar(originalArray), PropertyIds::constructor, &constructor, scriptContext))
            {LOGMEIN("JavascriptArray.cpp] 11927\n");
                return nullptr;
            }

            if (JavascriptOperators::IsConstructor(constructor))
            {LOGMEIN("JavascriptArray.cpp] 11932\n");
                ScriptContext* constructorScriptContext = RecyclableObject::FromVar(constructor)->GetScriptContext();
                if (constructorScriptContext != scriptContext)
                {LOGMEIN("JavascriptArray.cpp] 11935\n");
                    if (constructorScriptContext->GetLibrary()->GetArrayConstructor() == constructor)
                    {LOGMEIN("JavascriptArray.cpp] 11937\n");
                        constructor = scriptContext->GetLibrary()->GetUndefined();
                    }
                }
            }

            if (JavascriptOperators::IsObject(constructor))
            {LOGMEIN("JavascriptArray.cpp] 11944\n");
                if (!JavascriptOperators::GetProperty((RecyclableObject*)constructor, PropertyIds::_symbolSpecies, &constructor, scriptContext))
                {LOGMEIN("JavascriptArray.cpp] 11946\n");
                    if (pIsBuiltinArrayCtor != nullptr)
                    {LOGMEIN("JavascriptArray.cpp] 11948\n");
                        *pIsBuiltinArrayCtor = false;
                    }

                    return nullptr;
                }
                if (constructor == scriptContext->GetLibrary()->GetNull())
                {LOGMEIN("JavascriptArray.cpp] 11955\n");
                    constructor = scriptContext->GetLibrary()->GetUndefined();
                }
            }
        }

        if (constructor == scriptContext->GetLibrary()->GetUndefined() || constructor == scriptContext->GetLibrary()->GetArrayConstructor())
        {LOGMEIN("JavascriptArray.cpp] 11962\n");
            if (length > UINT_MAX)
            {LOGMEIN("JavascriptArray.cpp] 11964\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
            }

            if (nullptr == pIsIntArray)
            {LOGMEIN("JavascriptArray.cpp] 11969\n");
                return scriptContext->GetLibrary()->CreateArray(static_cast<uint32>(length));
            }
            else
            {
                // If the constructor function is the built-in Array constructor, we can be smart and create the right type of native array.
                JavascriptArray* pArr = JavascriptArray::FromVar(originalArray);
                pArr->GetArrayTypeAndConvert(pIsIntArray, pIsFloatArray);
                return CreateNewArrayHelper(static_cast<uint32>(length), *pIsIntArray, *pIsFloatArray, pArr, scriptContext);
            }
        }

        if (!JavascriptOperators::IsConstructor(constructor))
        {LOGMEIN("JavascriptArray.cpp] 11982\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NotAConstructor, _u("constructor[Symbol.species]"));
        }

        if (pIsBuiltinArrayCtor != nullptr)
        {LOGMEIN("JavascriptArray.cpp] 11987\n");
            *pIsBuiltinArrayCtor = false;
        }

        Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(length, scriptContext) };
        Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));

        return RecyclableObject::FromVar(JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext));
    }
    /*static*/
    PropertyId const JavascriptArray::specialPropertyIds[] =
    {
        PropertyIds::length
    };

    BOOL JavascriptArray::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12003\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12005\n");
            return false;
        }
        return DynamicObject::DeleteProperty(propertyId, flags);
    }

    BOOL JavascriptArray::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12012\n");
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {LOGMEIN("JavascriptArray.cpp] 12015\n");
            return false;
        }
        return DynamicObject::DeleteProperty(propertyNameString, flags);
    }

    BOOL JavascriptArray::HasProperty(PropertyId propertyId)
    {LOGMEIN("JavascriptArray.cpp] 12022\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12024\n");
            return true;
        }

        ScriptContext* scriptContext = GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptArray.cpp] 12031\n");
            return this->HasItem(index);
        }

        return DynamicObject::HasProperty(propertyId);
    }

    BOOL JavascriptArray::IsEnumerable(PropertyId propertyId)
    {LOGMEIN("JavascriptArray.cpp] 12039\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12041\n");
            return false;
        }
        return DynamicObject::IsEnumerable(propertyId);
    }

    BOOL JavascriptArray::IsConfigurable(PropertyId propertyId)
    {LOGMEIN("JavascriptArray.cpp] 12048\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12050\n");
            return false;
        }
        return DynamicObject::IsConfigurable(propertyId);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetEnumerable(PropertyId propertyId, BOOL value)
    {LOGMEIN("JavascriptArray.cpp] 12061\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12063\n");
            Assert(!value); // Can't change array length enumerable
            return true;
        }

        ScriptContext* scriptContext = this->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptArray.cpp] 12072\n");
            return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
                ->SetEnumerable(this, propertyId, value);
        }

        return __super::SetEnumerable(propertyId, value);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetWritable(PropertyId propertyId, BOOL value)
    {LOGMEIN("JavascriptArray.cpp] 12085\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        uint32 index;

        bool setLengthNonWritable = (propertyId == PropertyIds::length && !value);
        if (setLengthNonWritable || scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptArray.cpp] 12091\n");
            return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
                ->SetWritable(this, propertyId, value);
        }

        return __super::SetWritable(propertyId, value);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetConfigurable(PropertyId propertyId, BOOL value)
    {LOGMEIN("JavascriptArray.cpp] 12104\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12106\n");
            Assert(!value); // Can't change array length configurable
            return true;
        }

        ScriptContext* scriptContext = this->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptArray.cpp] 12115\n");
            return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
                ->SetConfigurable(this, propertyId, value);
        }

        return __super::SetConfigurable(propertyId, value);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
    {LOGMEIN("JavascriptArray.cpp] 12128\n");
        ScriptContext* scriptContext = this->GetScriptContext();

        // SetAttributes on "length" is not expected. DefineOwnProperty uses SetWritable. If this is
        // changed, we need to handle it here.
        Assert(propertyId != PropertyIds::length);

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptArray.cpp] 12137\n");
            return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
                ->SetItemAttributes(this, index, attributes);
        }

        return __super::SetAttributes(propertyId, attributes);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12150\n");
        ScriptContext* scriptContext = this->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptArray.cpp] 12155\n");
            return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
                ->SetItemAccessors(this, index, getter, setter);
        }

        return __super::SetAccessors(propertyId, getter, setter, flags);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {LOGMEIN("JavascriptArray.cpp] 12168\n");
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
            ->SetItemWithAttributes(this, index, value, attributes);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetItemAttributes(uint32 index, PropertyAttributes attributes)
    {LOGMEIN("JavascriptArray.cpp] 12178\n");
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
            ->SetItemAttributes(this, index, attributes);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetItemAccessors(uint32 index, Var getter, Var setter)
    {LOGMEIN("JavascriptArray.cpp] 12188\n");
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
            ->SetItemAccessors(this, index, getter, setter);
    }

    // Check if this objectArray isFrozen.
    BOOL JavascriptArray::IsObjectArrayFrozen()
    {LOGMEIN("JavascriptArray.cpp] 12195\n");
        // If this is still a JavascriptArray, it's not frozen.
        return false;
    }

    JavascriptEnumerator * JavascriptArray::GetIndexEnumerator(EnumeratorFlags flags, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12201\n");
        if (!!(flags & EnumeratorFlags::SnapShotSemantics))
        {LOGMEIN("JavascriptArray.cpp] 12203\n");
            return RecyclerNew(GetRecycler(), JavascriptArrayIndexSnapshotEnumerator, this, flags, requestContext);
        }
        return RecyclerNew(GetRecycler(), JavascriptArrayIndexEnumerator, this, flags, requestContext);
    }

    BOOL JavascriptArray::GetNonIndexEnumerator(JavascriptStaticEnumerator * enumerator, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12210\n");
        return enumerator->Initialize(nullptr, nullptr, this, EnumeratorFlags::SnapShotSemantics, requestContext, nullptr);
    }

    BOOL JavascriptArray::IsItemEnumerable(uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 12215\n");
        return true;
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::PreventExtensions()
    {LOGMEIN("JavascriptArray.cpp] 12224\n");
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)->PreventExtensions(this);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::Seal()
    {LOGMEIN("JavascriptArray.cpp] 12233\n");
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)->Seal(this);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::Freeze()
    {LOGMEIN("JavascriptArray.cpp] 12242\n");
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)->Freeze(this);
    }

    BOOL JavascriptArray::GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12247\n");
        if (index == 0)
        {LOGMEIN("JavascriptArray.cpp] 12249\n");
            *propertyName = requestContext->GetPropertyString(PropertyIds::length);
            return true;
        }
        return false;
    }

    // Returns the number of special non-enumerable properties this type has.
    uint JavascriptArray::GetSpecialPropertyCount() const
    {LOGMEIN("JavascriptArray.cpp] 12258\n");
        return _countof(specialPropertyIds);
    }

    // Returns the list of special non-enumerable properties for the type.
    PropertyId const * JavascriptArray::GetSpecialPropertyIds() const
    {LOGMEIN("JavascriptArray.cpp] 12264\n");
        return specialPropertyIds;
    }

    BOOL JavascriptArray::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12269\n");
        return JavascriptArray::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptArray::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        if (GetPropertyBuiltIns(propertyId, value))
        {LOGMEIN("JavascriptArray.cpp] 12276\n");
            return true;
        }

        ScriptContext* scriptContext = GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptArray.cpp] 12283\n");
            return this->GetItem(this, index, value, scriptContext);
        }

        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptArray::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12291\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value))
        {LOGMEIN("JavascriptArray.cpp] 12299\n");
            return true;
        }

        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    BOOL JavascriptArray::GetPropertyBuiltIns(PropertyId propertyId, Var* value)
    {LOGMEIN("JavascriptArray.cpp] 12307\n");
        //
        // length being accessed. Return array length
        //
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12312\n");
            *value = JavascriptNumber::ToVar(this->GetLength(), GetScriptContext());
            return true;
        }

        return false;
    }

    BOOL JavascriptArray::HasItem(uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 12321\n");
        Var value;
        return this->DirectGetItemAt<Var>(index, &value);
    }

    BOOL JavascriptArray::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12327\n");
        return this->DirectGetItemAt<Var>(index, value);
    }

    BOOL JavascriptArray::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12332\n");
        return this->DirectGetItemAt<Var>(index, value);
    }

    BOOL JavascriptArray::DirectGetVarItemAt(uint32 index, Var *value, ScriptContext *requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12337\n");
        return this->DirectGetItemAt<Var>(index, value);
    }

    BOOL JavascriptNativeIntArray::HasItem(uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 12342\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        int32 value;
        return this->DirectGetItemAt<int32>(index, &value);
    }

    BOOL JavascriptNativeFloatArray::HasItem(uint32 index)
    {LOGMEIN("JavascriptArray.cpp] 12351\n");
        double dvalue;
        return this->DirectGetItemAt<double>(index, &dvalue);
    }

    BOOL JavascriptNativeIntArray::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12357\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        return JavascriptNativeIntArray::DirectGetVarItemAt(index, value, requestContext);
    }

    BOOL JavascriptNativeIntArray::DirectGetVarItemAt(uint32 index, Var *value, ScriptContext *requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12365\n");
        int32 intvalue;
        if (!this->DirectGetItemAt<int32>(index, &intvalue))
        {LOGMEIN("JavascriptArray.cpp] 12368\n");
            return FALSE;
        }
        *value = JavascriptNumber::ToVar(intvalue, requestContext);
        return TRUE;
    }

    BOOL JavascriptNativeIntArray::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12376\n");
        return JavascriptNativeIntArray::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL JavascriptNativeFloatArray::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12381\n");
        return JavascriptNativeFloatArray::DirectGetVarItemAt(index, value, requestContext);
    }

    BOOL JavascriptNativeFloatArray::DirectGetVarItemAt(uint32 index, Var *value, ScriptContext *requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12386\n");
        double dvalue;
        int32 ivalue;
        if (!this->DirectGetItemAt<double>(index, &dvalue))
        {LOGMEIN("JavascriptArray.cpp] 12390\n");
            return FALSE;
        }
        if (*(uint64*)&dvalue == 0ull)
        {LOGMEIN("JavascriptArray.cpp] 12394\n");
            *value = TaggedInt::ToVarUnchecked(0);
        }
        else if (JavascriptNumber::TryGetInt32Value(dvalue, &ivalue) && !TaggedInt::IsOverflow(ivalue))
        {LOGMEIN("JavascriptArray.cpp] 12398\n");
            *value = TaggedInt::ToVarUnchecked(ivalue);
        }
        else
        {
            *value = JavascriptNumber::ToVarWithCheck(dvalue, requestContext);
        }
        return TRUE;
    }

    BOOL JavascriptNativeFloatArray::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12409\n");
        return JavascriptNativeFloatArray::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL JavascriptArray::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptArray.cpp] 12414\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        uint32 indexValue;
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12420\n");
            return this->SetLength(value);
        }
        else if (GetScriptContext()->IsNumericPropertyId(propertyId, &indexValue))
        {LOGMEIN("JavascriptArray.cpp] 12424\n");
            // Call this or subclass method
            return SetItem(indexValue, value, flags);
        }
        else
        {
            return DynamicObject::SetProperty(propertyId, value, flags, info);
        }
    }

    BOOL JavascriptArray::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptArray.cpp] 12435\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && propertyRecord->GetPropertyId() == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12446\n");
            return this->SetLength(value);
        }

        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    BOOL JavascriptArray::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {LOGMEIN("JavascriptArray.cpp] 12454\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        ScriptContext* scriptContext = GetScriptContext();

        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptArray.cpp] 12461\n");
            Assert(attributes == PropertyWritable);
            Assert(IsWritable(propertyId) && !IsConfigurable(propertyId) && !IsEnumerable(propertyId));
            return this->SetLength(value);
        }

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptArray.cpp] 12469\n");
            // Call this or subclass method
            return SetItemWithAttributes(index, value, attributes);
        }

        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL JavascriptArray::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12478\n");
        this->DirectSetItemAt(index, value);
        return true;
    }

    BOOL JavascriptNativeIntArray::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12484\n");
        int32 iValue;
        double dValue;
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        TypeId typeId = this->TrySetNativeIntArrayItem(value, &iValue, &dValue);
        if (typeId == TypeIds_NativeIntArray)
        {LOGMEIN("JavascriptArray.cpp] 12492\n");
            this->SetItem(index, iValue);
        }
        else if (typeId == TypeIds_NativeFloatArray)
        {LOGMEIN("JavascriptArray.cpp] 12496\n");
            reinterpret_cast<JavascriptNativeFloatArray*>(this)->DirectSetItemAt<double>(index, dValue);
        }
        else
        {
            this->DirectSetItemAt<Var>(index, value);
        }

        return TRUE;
    }

    TypeId JavascriptNativeIntArray::TrySetNativeIntArrayItem(Var value, int32 *iValue, double *dValue)
    {LOGMEIN("JavascriptArray.cpp] 12508\n");
        if (TaggedInt::Is(value))
        {LOGMEIN("JavascriptArray.cpp] 12510\n");
            int32 i = TaggedInt::ToInt32(value);
            if (i != JavascriptNativeIntArray::MissingItem)
            {LOGMEIN("JavascriptArray.cpp] 12513\n");
                *iValue = i;
                return TypeIds_NativeIntArray;
            }
        }
        if (JavascriptNumber::Is_NoTaggedIntCheck(value))
        {LOGMEIN("JavascriptArray.cpp] 12519\n");
            bool isInt32;
            int32 i;
            double d = JavascriptNumber::GetValue(value);
            if (JavascriptNumber::TryGetInt32OrUInt32Value(d, &i, &isInt32))
            {LOGMEIN("JavascriptArray.cpp] 12524\n");
                if (isInt32 && i != JavascriptNativeIntArray::MissingItem)
                {LOGMEIN("JavascriptArray.cpp] 12526\n");
                    *iValue = i;
                    return TypeIds_NativeIntArray;
                }
            }
            else
            {
                *dValue = d;
                JavascriptNativeIntArray::ToNativeFloatArray(this);
                return TypeIds_NativeFloatArray;
            }
        }

        JavascriptNativeIntArray::ToVarArray(this);
        return TypeIds_Array;
    }

    BOOL JavascriptNativeIntArray::SetItem(uint32 index, int32 iValue)
    {LOGMEIN("JavascriptArray.cpp] 12544\n");
        if (iValue == JavascriptNativeIntArray::MissingItem)
        {LOGMEIN("JavascriptArray.cpp] 12546\n");
            JavascriptArray *varArr = JavascriptNativeIntArray::ToVarArray(this);
            varArr->DirectSetItemAt(index, JavascriptNumber::ToVar(iValue, GetScriptContext()));
            return TRUE;
        }

        this->DirectSetItemAt(index, iValue);
        return TRUE;
    }

    BOOL JavascriptNativeFloatArray::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12557\n");
        double dValue;
        TypeId typeId = this->TrySetNativeFloatArrayItem(value, &dValue);
        if (typeId == TypeIds_NativeFloatArray)
        {LOGMEIN("JavascriptArray.cpp] 12561\n");
            this->SetItem(index, dValue);
        }
        else
        {
            this->DirectSetItemAt(index, value);
        }
        return TRUE;
    }

    TypeId JavascriptNativeFloatArray::TrySetNativeFloatArrayItem(Var value, double *dValue)
    {LOGMEIN("JavascriptArray.cpp] 12572\n");
        if (TaggedInt::Is(value))
        {LOGMEIN("JavascriptArray.cpp] 12574\n");
            *dValue = (double)TaggedInt::ToInt32(value);
            return TypeIds_NativeFloatArray;
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(value))
        {LOGMEIN("JavascriptArray.cpp] 12579\n");
            *dValue = JavascriptNumber::GetValue(value);
            return TypeIds_NativeFloatArray;
        }

        JavascriptNativeFloatArray::ToVarArray(this);
        return TypeIds_Array;
    }

    BOOL JavascriptNativeFloatArray::SetItem(uint32 index, double dValue)
    {LOGMEIN("JavascriptArray.cpp] 12589\n");
        if (*(uint64*)&dValue == *(uint64*)&JavascriptNativeFloatArray::MissingItem)
        {LOGMEIN("JavascriptArray.cpp] 12591\n");
            JavascriptArray *varArr = JavascriptNativeFloatArray::ToVarArray(this);
            varArr->DirectSetItemAt(index, JavascriptNumber::ToVarNoCheck(dValue, GetScriptContext()));
            return TRUE;
        }

        this->DirectSetItemAt<double>(index, dValue);
        return TRUE;
    }

    BOOL JavascriptArray::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12602\n");
        return this->DirectDeleteItemAt<Var>(index);
    }

    BOOL JavascriptNativeIntArray::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12607\n");
        return this->DirectDeleteItemAt<int32>(index);
    }

    BOOL JavascriptNativeFloatArray::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptArray.cpp] 12612\n");
        return this->DirectDeleteItemAt<double>(index);
    }

    BOOL JavascriptArray::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("JavascriptArray.cpp] 12617\n");
        return enumerator->Initialize(nullptr, this, this, flags, requestContext, forInCache);
    }

    BOOL JavascriptArray::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12622\n");
        stringBuilder->Append(_u('['));

        if (this->length < 10)
        {LOGMEIN("JavascriptArray.cpp] 12626\n");
            auto funcPtr = [&]()
            {
                ENTER_PINNED_SCOPE(JavascriptString, valueStr);
                valueStr = JavascriptArray::JoinHelper(this, GetLibrary()->GetCommaDisplayString(), requestContext);
                stringBuilder->Append(valueStr->GetString(), valueStr->GetLength());
                LEAVE_PINNED_SCOPE();
            };

            if (!requestContext->GetThreadContext()->IsScriptActive())
            {LOGMEIN("JavascriptArray.cpp] 12636\n");
                BEGIN_JS_RUNTIME_CALL(requestContext);
                {LOGMEIN("JavascriptArray.cpp] 12638\n");
                    funcPtr();
                }
                END_JS_RUNTIME_CALL(requestContext);
            }
            else
            {
                funcPtr();
            }
        }
        else
        {
            stringBuilder->AppendCppLiteral(_u("..."));
        }

        stringBuilder->Append(_u(']'));

        return TRUE;
    }

    BOOL JavascriptArray::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptArray.cpp] 12659\n");
        stringBuilder->AppendCppLiteral(_u("Object, (Array)"));
        return TRUE;
    }

    bool JavascriptNativeArray::Is(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 12665\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptNativeArray::Is(typeId);
    }

    bool JavascriptNativeArray::Is(TypeId typeId)
    {LOGMEIN("JavascriptArray.cpp] 12671\n");
        return JavascriptNativeIntArray::Is(typeId) || JavascriptNativeFloatArray::Is(typeId);
    }

    JavascriptNativeArray* JavascriptNativeArray::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNativeArray'");

        return static_cast<JavascriptNativeArray *>(RecyclableObject::FromVar(aValue));
    }

    bool JavascriptNativeIntArray::Is(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 12683\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptNativeIntArray::Is(typeId);
    }

#if ENABLE_COPYONACCESS_ARRAY
    bool JavascriptCopyOnAccessNativeIntArray::Is(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 12690\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptCopyOnAccessNativeIntArray::Is(typeId);
    }
#endif

    bool JavascriptNativeIntArray::Is(TypeId typeId)
    {LOGMEIN("JavascriptArray.cpp] 12697\n");
        return typeId == TypeIds_NativeIntArray;
    }

#if ENABLE_COPYONACCESS_ARRAY
    bool JavascriptCopyOnAccessNativeIntArray::Is(TypeId typeId)
    {LOGMEIN("JavascriptArray.cpp] 12703\n");
        return typeId == TypeIds_CopyOnAccessNativeIntArray;
    }
#endif

    bool JavascriptNativeIntArray::IsNonCrossSite(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 12709\n");
        bool ret = !TaggedInt::Is(aValue) && VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(aValue);
        Assert(ret == (JavascriptNativeIntArray::Is(aValue) && !JavascriptNativeIntArray::FromVar(aValue)->IsCrossSiteObject()));
        return ret;
    }

    JavascriptNativeIntArray* JavascriptNativeIntArray::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNativeIntArray'");

        return static_cast<JavascriptNativeIntArray *>(RecyclableObject::FromVar(aValue));
    }

#if ENABLE_COPYONACCESS_ARRAY
    JavascriptCopyOnAccessNativeIntArray* JavascriptCopyOnAccessNativeIntArray::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptCopyOnAccessNativeIntArray'");

        return static_cast<JavascriptCopyOnAccessNativeIntArray *>(RecyclableObject::FromVar(aValue));
    }
#endif

    bool JavascriptNativeFloatArray::Is(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 12732\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptNativeFloatArray::Is(typeId);
    }

    bool JavascriptNativeFloatArray::Is(TypeId typeId)
    {LOGMEIN("JavascriptArray.cpp] 12738\n");
        return typeId == TypeIds_NativeFloatArray;
    }

    bool JavascriptNativeFloatArray::IsNonCrossSite(Var aValue)
    {LOGMEIN("JavascriptArray.cpp] 12743\n");
        bool ret = !TaggedInt::Is(aValue) && VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(aValue);
        Assert(ret == (JavascriptNativeFloatArray::Is(aValue) && !JavascriptNativeFloatArray::FromVar(aValue)->IsCrossSiteObject()));
        return ret;
    }

    JavascriptNativeFloatArray* JavascriptNativeFloatArray::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNativeFloatArray'");

        return static_cast<JavascriptNativeFloatArray *>(RecyclableObject::FromVar(aValue));
    }

    template int   Js::JavascriptArray::GetParamForIndexOf<unsigned int>(unsigned int, Js::Arguments const&, void*&, unsigned int&, Js::ScriptContext*);
    template bool  Js::JavascriptArray::ArrayElementEnumerator::MoveNext<void*>();
    template void  Js::JavascriptArray::SetArrayLiteralItem<void*>(unsigned int, void*);
    template void* Js::JavascriptArray::TemplatedIndexOfHelper<false, Js::TypedArrayBase, unsigned int>(Js::TypedArrayBase*, void*, unsigned int, unsigned int, Js::ScriptContext*);
    template void* Js::JavascriptArray::TemplatedIndexOfHelper<true, Js::TypedArrayBase, unsigned int>(Js::TypedArrayBase*, void*, unsigned int, unsigned int, Js::ScriptContext*);
} //namespace Js
