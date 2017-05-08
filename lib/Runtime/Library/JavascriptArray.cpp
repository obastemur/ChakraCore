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
    {TRACE_IT(56008);
    }

    uint32 SegmentBTree::GetLazyCrossOverLimit()
    {TRACE_IT(56009);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.DisableArrayBTree)
        {TRACE_IT(56010);
            return Js::JavascriptArray::InvalidIndex;
        }
        else if (Js::Configuration::Global.flags.ForceArrayBTree)
        {TRACE_IT(56011);
            return ARRAY_CROSSOVER_FOR_VALIDATE;
        }
#endif
#ifdef VALIDATE_ARRAY
        if (Js::Configuration::Global.flags.ArrayValidate)
        {TRACE_IT(56012);
            return ARRAY_CROSSOVER_FOR_VALIDATE;
        }
#endif
        return SegmentBTree::MinDegree * 3;
    }

    BOOL SegmentBTree::IsLeaf() const
    {TRACE_IT(56013);
        return children == NULL;
    }
    BOOL SegmentBTree::IsFullNode() const
    {TRACE_IT(56014);
        return segmentCount == MaxKeys;
    }

    void SegmentBTree::InternalFind(SegmentBTree* node, uint32 itemIndex, SparseArraySegmentBase*& prev, SparseArraySegmentBase*& matchOrNext)
    {TRACE_IT(56015);
        uint32 i = 0;

        for(; i < node->segmentCount; i++)
        {TRACE_IT(56016);
            Assert(node->keys[i] == node->segments[i]->left);
            if (itemIndex <  node->keys[i])
            {TRACE_IT(56017);
                break;
            }
        }

        // i indicates the 1st segment in the node past any matching segment.
        // the i'th child is the children to the 'left' of the i'th segment.

        // If itemIndex matches segment i-1 (note that left is always a match even when length == 0)
        bool matches = i > 0 && (itemIndex == node->keys[i-1] || itemIndex < node->keys[i-1] + node->segments[i-1]->length);

        if (matches)
        {TRACE_IT(56018);
            // Find prev segment
            if (node->IsLeaf())
            {TRACE_IT(56019);
                if (i > 1)
                {TRACE_IT(56020);
                    // Previous is either sibling or set in a parent
                    prev = node->segments[i-2];
                }
            }
            else
            {TRACE_IT(56021);
                // prev is the right most leaf in children[i-1] tree
                SegmentBTree* child = &node->children[i - 1];
                while (!child->IsLeaf())
                {TRACE_IT(56022);
                    child = &child->children[child->segmentCount];
                }
                prev = child->segments[child->segmentCount - 1];
            }

            // Return the matching segment
            matchOrNext = node->segments[i-1];
        }
        else // itemIndex in between segment i-1 and i
        {TRACE_IT(56023);
            if (i > 0)
            {TRACE_IT(56024);
                // Store in previous in case a match or next is the first segment in a child.
                prev = node->segments[i-1];
            }

            if (node->IsLeaf())
            {TRACE_IT(56025);
                matchOrNext = (i == 0 ? node->segments[0] : PointerValue(prev->next));
            }
            else
            {TRACE_IT(56026);
                InternalFind(node->children + i, itemIndex, prev, matchOrNext);
            }
        }
    }

    void SegmentBTreeRoot::Find(uint32 itemIndex, SparseArraySegmentBase*& prev, SparseArraySegmentBase*& matchOrNext)
    {TRACE_IT(56027);
        prev = matchOrNext = NULL;
        InternalFind(this, itemIndex, prev, matchOrNext);
        Assert(prev == NULL || (prev->next == matchOrNext));// If prev exists it is immediately before matchOrNext in the list of arraysegments
        Assert(prev == NULL || (prev->left < itemIndex && prev->left + prev->length <= itemIndex)); // prev should never be a match (left is a match if length == 0)
        Assert(matchOrNext == NULL || (matchOrNext->left >= itemIndex || matchOrNext->left + matchOrNext->length > itemIndex));
    }

    void SegmentBTreeRoot::Add(Recycler* recycler, SparseArraySegmentBase* newSeg)
    {TRACE_IT(56028);

        if (IsFullNode())
        {TRACE_IT(56029);
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
    {TRACE_IT(56030);
        // Find old segment
        uint32 itemIndex = originalKey;
        uint32 i = 0;

        for(; i < segmentCount; i++)
        {TRACE_IT(56031);
            Assert(keys[i] == segments[i]->left || (oldSeg == newSeg && newSeg == segments[i]));
            if (itemIndex <  keys[i])
            {TRACE_IT(56032);
                break;
            }
        }

        // i is 1 past any match

        if (i > 0)
        {TRACE_IT(56033);
            if (oldSeg == segments[i-1])
            {TRACE_IT(56034);
                segments[i-1] = newSeg;
                keys[i-1] = newSeg->left;
                return;
            }
        }

        Assert(!IsLeaf());
        children[i].SwapSegment(originalKey, oldSeg, newSeg);
    }


    void SegmentBTree::SplitChild(Recycler* recycler, SegmentBTree* parent, uint32 iChild, SegmentBTree* child)
    {TRACE_IT(56035);
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
        {TRACE_IT(56036);
            newNode.segments[i] = child->segments[i+MinDegree];
            newNode.keys[i] = child->keys[i+MinDegree];

            // Do not leave false positive references around in the b-tree
            child->segments[i+MinDegree] = nullptr;
        }

        // If children exist move those as well.
        if (!child->IsLeaf())
        {TRACE_IT(56037);
            newNode.children = AllocatorNewArrayZ(Recycler, recycler, SegmentBTree, MaxDegree);
            for(uint32 j = 0; j < MinDegree; j++)
            {TRACE_IT(56038);
                newNode.children[j] = child->children[j+MinDegree];

                // Do not leave false positive references around in the b-tree
                child->children[j+MinDegree].segments = nullptr;
                child->children[j+MinDegree].children = nullptr;
            }
        }
        child->segmentCount = MinKeys;

        // Make room for the new child in parent
        for(uint32 j = parent->segmentCount; j > iChild; j--)
        {TRACE_IT(56039);
            parent->children[j+1] = parent->children[j];
        }
        // Copy the contents of the new node into the correct place in the parent's child array
        parent->children[iChild+1] = newNode;

        // Move the keys to make room for the median key
        for(uint32 k = parent->segmentCount; k > iChild; k--)
        {TRACE_IT(56040);
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
    {TRACE_IT(56041);
        Assert(!node->IsFullNode());
        AnalysisAssert(node->segmentCount < MaxKeys);       // Same as !node->IsFullNode()
        Assert(newSeg != NULL);

        if (node->IsLeaf())
        {TRACE_IT(56042);
            // Move the keys
            uint32 i = node->segmentCount - 1;
            while( (i != -1) && (newSeg->left < node->keys[i]))
            {TRACE_IT(56043);
                node->segments[i+1] = node->segments[i];
                node->keys[i+1] = node->keys[i];
                i--;
            }
            if (!node->segments)
            {TRACE_IT(56044);
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
        {TRACE_IT(56045);
            // find the correct child node
            uint32 i = node->segmentCount-1;

            while((i != -1) && (newSeg->left < node->keys[i]))
            {TRACE_IT(56046);
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
                {TRACE_IT(56047);
                    i++;
                }
            }
            InsertNonFullNode(recycler, node->children+i, newSeg);
        }
    }

    inline void ThrowTypeErrorOnFailureHelper::ThrowTypeErrorOnFailure(BOOL operationSucceeded)
    {TRACE_IT(56048);
        if (IsThrowTypeError(operationSucceeded))
        {TRACE_IT(56049);
            ThrowTypeErrorOnFailure();
        }
    }

    inline void ThrowTypeErrorOnFailureHelper::ThrowTypeErrorOnFailure()
    {TRACE_IT(56050);
        JavascriptError::ThrowTypeError(m_scriptContext, VBSERR_ActionNotSupported, m_functionName);
    }

    inline BOOL ThrowTypeErrorOnFailureHelper::IsThrowTypeError(BOOL operationSucceeded)
    {TRACE_IT(56051);
        return !operationSucceeded;
    }

    // Make sure EmptySegment points to read-only memory.
    // Can't do this the easy way because SparseArraySegment has a constructor...
    JavascriptArray::JavascriptArray(DynamicType * type)
        : ArrayObject(type, false, 0)
    {TRACE_IT(56052);
        Assert(type->GetTypeId() == TypeIds_Array || type->GetTypeId() == TypeIds_NativeIntArray || type->GetTypeId() == TypeIds_NativeFloatArray || ((type->GetTypeId() == TypeIds_ES5Array || type->GetTypeId() == TypeIds_Object) && type->GetPrototype() == GetScriptContext()->GetLibrary()->GetArrayPrototype()));
        Assert(EmptySegment->length == 0 && EmptySegment->size == 0 && EmptySegment->next == NULL);
        InitArrayFlags(DynamicObjectFlags::InitialArrayValue);
        SetHeadAndLastUsedSegment(const_cast<SparseArraySegmentBase *>(EmptySegment));

    }

    JavascriptArray::JavascriptArray(uint32 length, DynamicType * type)
        : ArrayObject(type, false, length)
    {TRACE_IT(56053);
        Assert(JavascriptArray::Is(type->GetTypeId()));
        Assert(EmptySegment->length == 0 && EmptySegment->size == 0 && EmptySegment->next == NULL);
        InitArrayFlags(DynamicObjectFlags::InitialArrayValue);
        SetHeadAndLastUsedSegment(const_cast<SparseArraySegmentBase *>(EmptySegment));
    }

    JavascriptArray::JavascriptArray(uint32 length, uint32 size, DynamicType * type)
        : ArrayObject(type, false, length)
    {TRACE_IT(56054);
        Assert(type->GetTypeId() == TypeIds_Array);
        InitArrayFlags(DynamicObjectFlags::InitialArrayValue);
        Recycler* recycler = GetRecycler();
        SetHeadAndLastUsedSegment(SparseArraySegment<Var>::AllocateSegment(recycler, 0, 0, size, nullptr));
    }

    JavascriptArray::JavascriptArray(DynamicType * type, uint32 size)
        : ArrayObject(type, false)
    {TRACE_IT(56055);
        InitArrayFlags(DynamicObjectFlags::InitialArrayValue);
        SetHeadAndLastUsedSegment(DetermineInlineHeadSegmentPointer<JavascriptArray, 0, false>(this));
        head->size = size;
        Var fill = Js::JavascriptArray::MissingItem;
        for (uint i = 0; i < size; i++)
        {TRACE_IT(56056);
            SparseArraySegment<Var>::From(head)->elements[i] = fill;
        }
    }

    JavascriptNativeIntArray::JavascriptNativeIntArray(uint32 length, uint32 size, DynamicType * type)
        : JavascriptNativeArray(type)
    {TRACE_IT(56057);
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
    {TRACE_IT(56058);
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
    {TRACE_IT(56059);
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptArray::Is(typeId);
    }

    bool JavascriptArray::Is(TypeId typeId)
    {TRACE_IT(56060);
        return typeId >= TypeIds_ArrayFirst && typeId <= TypeIds_ArrayLast;
    }

    bool JavascriptArray::IsVarArray(Var aValue)
    {TRACE_IT(56061);
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptArray::IsVarArray(typeId);
    }

    bool JavascriptArray::IsVarArray(TypeId typeId)
    {TRACE_IT(56062);
        return typeId == TypeIds_Array;
    }

    template<typename T>
    bool JavascriptArray::IsMissingItemAt(uint32 index) const
    {TRACE_IT(56063);
        SparseArraySegment<T>* headSeg = SparseArraySegment<T>::From(this->head);

        return SparseArraySegment<T>::IsMissingItem(&headSeg->elements[index]);
    }

    bool JavascriptArray::IsMissingItem(uint32 index)
    {TRACE_IT(56064);
        if (this->length <= index)
        {TRACE_IT(56065);
            return false;
        }

        bool isIntArray = false, isFloatArray = false;
        this->GetArrayTypeAndConvert(&isIntArray, &isFloatArray);

        if (isIntArray)
        {TRACE_IT(56066);
            return IsMissingItemAt<int32>(index);
        }
        else if (isFloatArray)
        {TRACE_IT(56067);
            return IsMissingItemAt<double>(index);
        }
        else
        {TRACE_IT(56068);
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
    {TRACE_IT(56069);
        AssertMsg(Is(aValue) || ES5Array::Is(aValue), "Ensure var is actually a 'JavascriptArray' or 'ES5Array'");

        return static_cast<JavascriptArray *>(RecyclableObject::FromVar(aValue));
    }

    // Check if a Var is a direct-accessible (fast path) JavascriptArray.
    bool JavascriptArray::IsDirectAccessArray(Var aValue)
    {TRACE_IT(56070);
        return RecyclableObject::Is(aValue) &&
            (VirtualTableInfo<JavascriptArray>::HasVirtualTable(aValue) ||
                VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(aValue) ||
                VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(aValue));
    }

    DynamicObjectFlags JavascriptArray::GetFlags() const
    {
        return GetArrayFlags();
    }

    DynamicObjectFlags JavascriptArray::GetFlags_Unchecked() const // do not use except in extreme circumstances
    {
        return GetArrayFlags_Unchecked();
    }

    void JavascriptArray::SetFlags(const DynamicObjectFlags flags)
    {TRACE_IT(56073);
        SetArrayFlags(flags);
    }

    DynamicType * JavascriptArray::GetInitialType(ScriptContext * scriptContext)
    {TRACE_IT(56074);
        return scriptContext->GetLibrary()->GetArrayType();
    }

    JavascriptArray *JavascriptArray::GetArrayForArrayOrObjectWithArray(const Var var)
    {TRACE_IT(56075);
        bool isObjectWithArray;
        TypeId arrayTypeId;
        return GetArrayForArrayOrObjectWithArray(var, &isObjectWithArray, &arrayTypeId);
    }

    JavascriptArray *JavascriptArray::GetArrayForArrayOrObjectWithArray(
        const Var var,
        bool *const isObjectWithArrayRef,
        TypeId *const arrayTypeIdRef)
    {TRACE_IT(56076);
        // This is a helper function used by jitted code. The array checks done here match the array checks done by jitted code
        // (see Lowerer::GenerateArrayTest) to minimize bailouts.

        Assert(var);
        Assert(isObjectWithArrayRef);
        Assert(arrayTypeIdRef);

        *isObjectWithArrayRef = false;
        *arrayTypeIdRef = TypeIds_Undefined;

        if(!RecyclableObject::Is(var))
        {TRACE_IT(56077);
            return nullptr;
        }

        JavascriptArray *array = nullptr;
        INT_PTR vtable = VirtualTableInfoBase::GetVirtualTable(var);
        if(vtable == VirtualTableInfo<DynamicObject>::Address)
        {TRACE_IT(56078);
            ArrayObject* objectArray = DynamicObject::FromVar(var)->GetObjectArray();
            array = (objectArray && Is(objectArray)) ? FromVar(objectArray) : nullptr;
            if(!array)
            {TRACE_IT(56079);
                return nullptr;
            }
            *isObjectWithArrayRef = true;
            vtable = VirtualTableInfoBase::GetVirtualTable(array);
        }

        if(vtable == VirtualTableInfo<JavascriptArray>::Address)
        {TRACE_IT(56080);
            *arrayTypeIdRef = TypeIds_Array;
        }
        else if(vtable == VirtualTableInfo<JavascriptNativeIntArray>::Address)
        {TRACE_IT(56081);
            *arrayTypeIdRef = TypeIds_NativeIntArray;
        }
        else if(vtable == VirtualTableInfo<JavascriptNativeFloatArray>::Address)
        {TRACE_IT(56082);
            *arrayTypeIdRef = TypeIds_NativeFloatArray;
        }
        else
        {TRACE_IT(56083);
            return nullptr;
        }

        if(!array)
        {TRACE_IT(56084);
            array = FromVar(var);
        }
        return array;
    }

    const SparseArraySegmentBase *JavascriptArray::Jit_GetArrayHeadSegmentForArrayOrObjectWithArray(const Var var)
    {TRACE_IT(56085);
        // This is a helper function used by jitted code

        JavascriptArray *const array = GetArrayForArrayOrObjectWithArray(var);
        return array ? array->head : nullptr;
    }

    uint32 JavascriptArray::Jit_GetArrayHeadSegmentLength(const SparseArraySegmentBase *const headSegment)
    {TRACE_IT(56086);
        // This is a helper function used by jitted code

        return headSegment ? headSegment->length : 0;
    }

    bool JavascriptArray::Jit_OperationInvalidatedArrayHeadSegment(
        const SparseArraySegmentBase *const headSegmentBeforeOperation,
        const uint32 headSegmentLengthBeforeOperation,
        const Var varAfterOperation)
    {TRACE_IT(56087);
        // This is a helper function used by jitted code

        Assert(varAfterOperation);

        if(!headSegmentBeforeOperation)
        {TRACE_IT(56088);
            return false;
        }

        const SparseArraySegmentBase *const headSegmentAfterOperation =
            Jit_GetArrayHeadSegmentForArrayOrObjectWithArray(varAfterOperation);
        return
            headSegmentAfterOperation != headSegmentBeforeOperation ||
            headSegmentAfterOperation->length != headSegmentLengthBeforeOperation;
    }

    uint32 JavascriptArray::Jit_GetArrayLength(const Var var)
    {TRACE_IT(56089);
        // This is a helper function used by jitted code

        bool isObjectWithArray;
        TypeId arrayTypeId;
        JavascriptArray *const array = GetArrayForArrayOrObjectWithArray(var, &isObjectWithArray, &arrayTypeId);
        return array && !isObjectWithArray ? array->GetLength() : 0;
    }

    bool JavascriptArray::Jit_OperationInvalidatedArrayLength(const uint32 lengthBeforeOperation, const Var varAfterOperation)
    {TRACE_IT(56090);
        // This is a helper function used by jitted code

        return Jit_GetArrayLength(varAfterOperation) != lengthBeforeOperation;
    }

    DynamicObjectFlags JavascriptArray::Jit_GetArrayFlagsForArrayOrObjectWithArray(const Var var)
    {TRACE_IT(56091);
        // This is a helper function used by jitted code

        JavascriptArray *const array = GetArrayForArrayOrObjectWithArray(var);
        return array && array->UsesObjectArrayOrFlagsAsFlags() ? array->GetFlags() : DynamicObjectFlags::None;
    }

    bool JavascriptArray::Jit_OperationCreatedFirstMissingValue(
        const DynamicObjectFlags flagsBeforeOperation,
        const Var varAfterOperation)
    {TRACE_IT(56092);
        // This is a helper function used by jitted code

        Assert(varAfterOperation);

        return
            !!(flagsBeforeOperation & DynamicObjectFlags::HasNoMissingValues) &&
            !(Jit_GetArrayFlagsForArrayOrObjectWithArray(varAfterOperation) & DynamicObjectFlags::HasNoMissingValues);
    }

    bool JavascriptArray::HasNoMissingValues() const
    {TRACE_IT(56093);
        return !!(GetFlags() & DynamicObjectFlags::HasNoMissingValues);
    }

    bool JavascriptArray::HasNoMissingValues_Unchecked() const // do not use except in extreme circumstances
    {TRACE_IT(56094);
        return !!(GetFlags_Unchecked() & DynamicObjectFlags::HasNoMissingValues);
    }

    void JavascriptArray::SetHasNoMissingValues(const bool hasNoMissingValues)
    {TRACE_IT(56095);
        SetFlags(
            hasNoMissingValues
                ? GetFlags() | DynamicObjectFlags::HasNoMissingValues
                : GetFlags() & ~DynamicObjectFlags::HasNoMissingValues);
    }

    template<class T>
    bool JavascriptArray::IsMissingHeadSegmentItemImpl(const uint32 index) const
    {TRACE_IT(56096);
        Assert(index < head->length);

        return SparseArraySegment<T>::IsMissingItem(&SparseArraySegment<T>::From(head)->elements[index]);
    }

    bool JavascriptArray::IsMissingHeadSegmentItem(const uint32 index) const
    {TRACE_IT(56097);
        return IsMissingHeadSegmentItemImpl<Var>(index);
    }

#if ENABLE_COPYONACCESS_ARRAY
    void JavascriptCopyOnAccessNativeIntArray::ConvertCopyOnAccessSegment()
    {TRACE_IT(56098);
        Assert(this->GetScriptContext()->GetLibrary()->cacheForCopyOnAccessArraySegments->IsValidIndex(::Math::PointerCastToIntegral<uint32>(this->GetHead())));
        SparseArraySegment<int32> *seg = this->GetScriptContext()->GetLibrary()->cacheForCopyOnAccessArraySegments->GetSegmentByIndex(::Math::PointerCastToIntegral<byte>(this->GetHead()));
        SparseArraySegment<int32> *newSeg = SparseArraySegment<int32>::AllocateLiteralHeadSegment(this->GetRecycler(), seg->length);

#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::CopyOnAccessArrayPhase))
        {TRACE_IT(56099);
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
        {TRACE_IT(56100);
            arrayInfo->isNotCopyOnAccessArray = 1;
        }
    }

    uint32 JavascriptCopyOnAccessNativeIntArray::GetNextIndex(uint32 index) const
    {TRACE_IT(56101);
        if (this->length == 0 || (index != Js::JavascriptArray::InvalidIndex && index >= this->length))
        {TRACE_IT(56102);
            return Js::JavascriptArray::InvalidIndex;
        }
        else if (index == Js::JavascriptArray::InvalidIndex)
        {TRACE_IT(56103);
            return 0;
        }
        else
        {TRACE_IT(56104);
            return index + 1;
        }
    }

    BOOL JavascriptCopyOnAccessNativeIntArray::DirectGetItemAt(uint32 index, int* outVal)
    {TRACE_IT(56105);
        Assert(this->GetScriptContext()->GetLibrary()->cacheForCopyOnAccessArraySegments->IsValidIndex(::Math::PointerCastToIntegral<uint32>(this->GetHead())));
        SparseArraySegment<int32> *seg = this->GetScriptContext()->GetLibrary()->cacheForCopyOnAccessArraySegments->GetSegmentByIndex(::Math::PointerCastToIntegral<byte>(this->GetHead()));

        if (this->length == 0 || index == Js::JavascriptArray::InvalidIndex || index >= this->length)
        {TRACE_IT(56106);
            return FALSE;
        }
        else
        {TRACE_IT(56107);
            *outVal = seg->elements[index];
            return TRUE;
        }
    }
#endif

    bool JavascriptNativeIntArray::IsMissingHeadSegmentItem(const uint32 index) const
    {TRACE_IT(56108);
        return IsMissingHeadSegmentItemImpl<int32>(index);
    }

    bool JavascriptNativeFloatArray::IsMissingHeadSegmentItem(const uint32 index) const
    {TRACE_IT(56109);
        return IsMissingHeadSegmentItemImpl<double>(index);
    }

    template<typename T>
    void JavascriptArray::InternalFillFromPrototype(JavascriptArray *dstArray, const T& dstIndex, JavascriptArray *srcArray, uint32 start, uint32 end, uint32 count)
    {TRACE_IT(56110);
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
    {TRACE_IT(56111);
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
    {TRACE_IT(56112);
        return length <= SparseArraySegmentBase::INLINE_CHUNK_SIZE;
    }

    Var JavascriptArray::OP_NewScArray(uint32 elementCount, ScriptContext* scriptContext)
    {TRACE_IT(56113);
        // Called only to create array literals: size is known.
        return scriptContext->GetLibrary()->CreateArrayLiteral(elementCount);
    }

    Var JavascriptArray::OP_NewScArrayWithElements(uint32 elementCount, Var *elements, ScriptContext* scriptContext)
    {TRACE_IT(56114);
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
    {TRACE_IT(56115);
        // Called only to create array literals: size is known.
        JavascriptArray *const array = static_cast<JavascriptArray *>(OP_NewScArray(elementCount, scriptContext));
        array->SetHasNoMissingValues(false);
        SparseArraySegment<Var> *head = SparseArraySegment<Var>::From(array->head);
        head->FillSegmentBuffer(0, elementCount);

        return array;
    }

#if ENABLE_PROFILE_INFO
    Var JavascriptArray::ProfiledNewScArray(uint32 elementCount, ScriptContext *scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef)
    {TRACE_IT(56116);
        if (arrayInfo->IsNativeIntArray())
        {TRACE_IT(56117);
            JavascriptNativeIntArray *arr = scriptContext->GetLibrary()->CreateNativeIntArrayLiteral(elementCount);
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);
            return arr;
        }

        if (arrayInfo->IsNativeFloatArray())
        {TRACE_IT(56118);
            JavascriptNativeFloatArray *arr = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(elementCount);
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);
            return arr;
        }

        JavascriptArray *arr = scriptContext->GetLibrary()->CreateArrayLiteral(elementCount);
        return arr;
    }
#endif
    Var JavascriptArray::OP_NewScIntArray(AuxArray<int32> *ints, ScriptContext* scriptContext)
    {TRACE_IT(56119);
        uint32 count = ints->count;
        JavascriptArray *arr = scriptContext->GetLibrary()->CreateArrayLiteral(count);
        SparseArraySegment<Var> *head = SparseArraySegment<Var>::From(arr->head);
        Assert(count > 0 && count == head->length);
        for (uint i = 0; i < count; i++)
        {TRACE_IT(56120);
            head->elements[i] = JavascriptNumber::ToVar(ints->elements[i], scriptContext);
        }
        return arr;
    }

#if ENABLE_PROFILE_INFO
    Var JavascriptArray::ProfiledNewScIntArray(AuxArray<int32> *ints, ScriptContext* scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef)
    {TRACE_IT(56121);
        // Called only to create array literals: size is known.
        uint32 count = ints->count;

        if (arrayInfo->IsNativeIntArray())
        {TRACE_IT(56122);
            JavascriptNativeIntArray *arr;

#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary *lib = scriptContext->GetLibrary();
            FunctionBody *functionBody = weakFuncRef->Get();

            if (JavascriptLibrary::IsCopyOnAccessArrayCallSite(lib, arrayInfo, count))
            {TRACE_IT(56123);
                Assert(lib->cacheForCopyOnAccessArraySegments);
                arr = scriptContext->GetLibrary()->CreateCopyOnAccessNativeIntArrayLiteral(arrayInfo, functionBody, ints);
            }
            else
#endif
            {TRACE_IT(56124);
                arr = scriptContext->GetLibrary()->CreateNativeIntArrayLiteral(count);
                SparseArraySegment<int32> *head = SparseArraySegment<int32>::From(arr->head);
                Assert(count > 0 && count == head->length);
                CopyArray(head->elements, head->length, ints->elements, count);
            }

            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);

            return arr;
        }

        if (arrayInfo->IsNativeFloatArray())
        {TRACE_IT(56125);
            JavascriptNativeFloatArray *arr = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(count);
            SparseArraySegment<double> *head = SparseArraySegment<double>::From(arr->head);
            Assert(count > 0 && count == head->length);
            for (uint i = 0; i < count; i++)
            {TRACE_IT(56126);
                head->elements[i] = (double)ints->elements[i];
            }
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);

            return arr;
        }

        return OP_NewScIntArray(ints, scriptContext);
    }
#endif

    Var JavascriptArray::OP_NewScFltArray(AuxArray<double> *doubles, ScriptContext* scriptContext)
    {TRACE_IT(56127);
        uint32 count = doubles->count;
        JavascriptArray *arr = scriptContext->GetLibrary()->CreateArrayLiteral(count);
        SparseArraySegment<Var> *head = SparseArraySegment<Var>::From(arr->head);
        Assert(count > 0 && count == head->length);
        for (uint i = 0; i < count; i++)
        {TRACE_IT(56128);
            double dval = doubles->elements[i];
            int32 ival;
            if (JavascriptNumber::TryGetInt32Value(dval, &ival) && !TaggedInt::IsOverflow(ival))
            {TRACE_IT(56129);
                head->elements[i] = TaggedInt::ToVarUnchecked(ival);
            }
            else
            {TRACE_IT(56130);
                head->elements[i] = JavascriptNumber::ToVarNoCheck(dval, scriptContext);
            }
        }
        return arr;
    }

#if ENABLE_PROFILE_INFO
    Var JavascriptArray::ProfiledNewScFltArray(AuxArray<double> *doubles, ScriptContext* scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef)
    {TRACE_IT(56131);
        // Called only to create array literals: size is known.
        if (arrayInfo->IsNativeFloatArray())
        {TRACE_IT(56132);
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
        {TRACE_IT(56133);
            // Exactly one argument, which is the array length if it's a uint32.
            Var firstArgument = args[1];
            int elementCount;
            if (TaggedInt::Is(firstArgument))
            {TRACE_IT(56134);
                elementCount = TaggedInt::ToInt32(firstArgument);
                if (elementCount < 0)
                {TRACE_IT(56135);
                    JavascriptError::ThrowRangeError(function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                if (arrayInfo && arrayInfo->IsNativeArray())
                {TRACE_IT(56136);
                    if (arrayInfo->IsNativeIntArray())
                    {TRACE_IT(56137);
                        pNew = function->GetLibrary()->CreateNativeIntArray(elementCount);
                    }
                    else
                    {TRACE_IT(56138);
                        pNew = function->GetLibrary()->CreateNativeFloatArray(elementCount);
                    }
                }
                else
                {TRACE_IT(56139);
                    pNew = function->GetLibrary()->CreateArray(elementCount);
                }
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(firstArgument))
            {TRACE_IT(56140);
                // Non-tagged-int number: make sure the double value is really a uint32.
                double value = JavascriptNumber::GetValue(firstArgument);
                uint32 uvalue = JavascriptConversion::ToUInt32(value);
                if (value != uvalue)
                {TRACE_IT(56141);
                    JavascriptError::ThrowRangeError(function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                if (arrayInfo && arrayInfo->IsNativeArray())
                {TRACE_IT(56142);
                    if (arrayInfo->IsNativeIntArray())
                    {TRACE_IT(56143);
                        pNew = function->GetLibrary()->CreateNativeIntArray(uvalue);
                    }
                    else
                    {TRACE_IT(56144);
                        pNew = function->GetLibrary()->CreateNativeFloatArray(uvalue);
                    }
                }
                else
                {TRACE_IT(56145);
                    pNew = function->GetLibrary()->CreateArray(uvalue);
                }
            }
            else
            {TRACE_IT(56146);
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
        {TRACE_IT(56147);
            // Called with a list of initial element values.
            // Create an array of the appropriate length and walk the list.

            if (arrayInfo && arrayInfo->IsNativeArray())
            {TRACE_IT(56148);
                if (arrayInfo->IsNativeIntArray())
                {TRACE_IT(56149);
                    pNew = function->GetLibrary()->CreateNativeIntArray(callInfo.Count - 1);
                }
                else
                {TRACE_IT(56150);
                    pNew = function->GetLibrary()->CreateNativeFloatArray(callInfo.Count - 1);
                }
            }
            else
            {TRACE_IT(56151);
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
    {TRACE_IT(56152);
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
        {TRACE_IT(56153);
            // No arguments passed to Array(), so create with the default size (0).
            pNew = CreateArrayFromConstructorNoArg(function, scriptContext);

            return isCtorSuperCall ?
                JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pNew, nullptr, scriptContext) :
                pNew;
        }

        if (callInfo.Count == 2)
        {TRACE_IT(56154);
            // Exactly one argument, which is the array length if it's a uint32.
            Var firstArgument = args[1];
            int elementCount;

            if (TaggedInt::Is(firstArgument))
            {TRACE_IT(56155);
                elementCount = TaggedInt::ToInt32(firstArgument);
                if (elementCount < 0)
                {TRACE_IT(56156);
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
                }

                pNew = CreateArrayFromConstructor(function, elementCount, scriptContext);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(firstArgument))
            {TRACE_IT(56157);
                // Non-tagged-int number: make sure the double value is really a uint32.
                double value = JavascriptNumber::GetValue(firstArgument);
                uint32 uvalue = JavascriptConversion::ToUInt32(value);
                if (value != uvalue)
                {TRACE_IT(56158);
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
                }

                pNew = CreateArrayFromConstructor(function, uvalue, scriptContext);
            }
            else
            {TRACE_IT(56159);
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
        {TRACE_IT(56160);
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
    {TRACE_IT(56161);
        JavascriptLibrary* library = constructor->GetLibrary();

        // Create the Array object we'll return - this is the only way to create an object which is an exotic Array object.
        // Note: We need to use the library from the ScriptContext of the constructor, not the currently executing function.
        //       This is for the case where a built-in @@create method from a different JavascriptLibrary is installed on
        //       constructor.
        return library->CreateArray(length);
    }

    JavascriptArray* JavascriptArray::CreateArrayFromConstructorNoArg(RecyclableObject* constructor, ScriptContext* scriptContext)
    {TRACE_IT(56162);
        JavascriptLibrary* library = constructor->GetLibrary();
        return library->CreateArray();
    }

#if ENABLE_PROFILE_INFO
    Var JavascriptArray::ProfiledNewInstanceNoArg(RecyclableObject *function, ScriptContext *scriptContext, ArrayCallSiteInfo *arrayInfo, RecyclerWeakReference<FunctionBody> *weakFuncRef)
    {TRACE_IT(56163);
        Assert(JavascriptFunction::Is(function) &&
               JavascriptFunction::FromVar(function)->GetFunctionInfo() == &JavascriptArray::EntryInfo::NewInstance);

        if (arrayInfo->IsNativeIntArray())
        {TRACE_IT(56164);
            JavascriptNativeIntArray *arr = scriptContext->GetLibrary()->CreateNativeIntArray();
            arr->SetArrayProfileInfo(weakFuncRef, arrayInfo);
            return arr;
        }

        if (arrayInfo->IsNativeFloatArray())
        {TRACE_IT(56165);
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
    {TRACE_IT(56166);
        Assert(!PHASE_OFF1(NativeArrayPhase));

        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        const CallInfo &callInfo = args.Info;
        if (callInfo.Count < 2)
        {TRACE_IT(56167);
            // No arguments passed to Array(), so create with the default size (0).
            return function->GetLibrary()->CreateNativeIntArray();
        }

        JavascriptArray* pNew = nullptr;
        if (callInfo.Count == 2)
        {TRACE_IT(56168);
            // Exactly one argument, which is the array length if it's a uint32.
            Var firstArgument = args[1];
            int elementCount;
            if (TaggedInt::Is(firstArgument))
            {TRACE_IT(56169);
                elementCount = TaggedInt::ToInt32(firstArgument);
                if (elementCount < 0)
                {TRACE_IT(56170);
                    JavascriptError::ThrowRangeError(
                        function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                pNew = function->GetLibrary()->CreateNativeIntArray(elementCount);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(firstArgument))
            {TRACE_IT(56171);
                // Non-tagged-int number: make sure the double value is really a uint32.
                double value = JavascriptNumber::GetValue(firstArgument);
                uint32 uvalue = JavascriptConversion::ToUInt32(value);
                if (value != uvalue)
                {TRACE_IT(56172);
                    JavascriptError::ThrowRangeError(
                        function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                pNew = function->GetLibrary()->CreateNativeIntArray(uvalue);
            }
            else
            {TRACE_IT(56173);
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
        {TRACE_IT(56174);
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
    {TRACE_IT(56175);
        Assert(!PHASE_OFF1(NativeArrayPhase));

        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        const CallInfo &callInfo = args.Info;
        if (callInfo.Count < 2)
        {TRACE_IT(56176);
            // No arguments passed to Array(), so create with the default size (0).
            return function->GetLibrary()->CreateNativeFloatArray();
        }

        JavascriptArray* pNew = nullptr;
        if (callInfo.Count == 2)
        {TRACE_IT(56177);
            // Exactly one argument, which is the array length if it's a uint32.
            Var firstArgument = args[1];
            int elementCount;
            if (TaggedInt::Is(firstArgument))
            {TRACE_IT(56178);
                elementCount = TaggedInt::ToInt32(firstArgument);
                if (elementCount < 0)
                {TRACE_IT(56179);
                    JavascriptError::ThrowRangeError(
                        function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                pNew = function->GetLibrary()->CreateNativeFloatArray(elementCount);
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(firstArgument))
            {TRACE_IT(56180);
                // Non-tagged-int number: make sure the double value is really a uint32.
                double value = JavascriptNumber::GetValue(firstArgument);
                uint32 uvalue = JavascriptConversion::ToUInt32(value);
                if (value != uvalue)
                {TRACE_IT(56181);
                    JavascriptError::ThrowRangeError(
                        function->GetScriptContext(), JSERR_ArrayLengthConstructIncorrect);
                }
                pNew = function->GetLibrary()->CreateNativeFloatArray(uvalue);
            }
            else
            {TRACE_IT(56182);
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
        {TRACE_IT(56183);
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
        {TRACE_IT(56184);
            Var item = args[i + 1];

            bool isTaggedInt = TaggedInt::Is(item);
            bool isTaggedIntMissingValue = false;
#ifdef _M_AMD64
            if (isTaggedInt)
            {TRACE_IT(56185);
                int32 iValue = TaggedInt::ToInt32(item);
                isTaggedIntMissingValue = Js::SparseArraySegment<int32>::IsMissingItem(&iValue);
            }
#endif
            if (isTaggedInt && !isTaggedIntMissingValue)
            {TRACE_IT(56186);
                // This is taggedInt case and we verified that item is not missing value in AMD64.
                this->DirectSetItemAt(i, TaggedInt::ToInt32(item));
            }
            else if (!isTaggedIntMissingValue && JavascriptNumber::Is_NoTaggedIntCheck(item))
            {TRACE_IT(56187);
                double dvalue = JavascriptNumber::GetValue(item);
                int32 ivalue;
                if (JavascriptNumber::TryGetInt32Value(dvalue, &ivalue) && !Js::SparseArraySegment<int32>::IsMissingItem(&ivalue))
                {TRACE_IT(56188);
                    this->DirectSetItemAt(i, ivalue);
                }
                else
                {TRACE_IT(56189);
#if ENABLE_PROFILE_INFO
                    if (arrayInfo)
                    {TRACE_IT(56190);
                        arrayInfo->SetIsNotNativeIntArray();
                    }
#endif

                    if (HasInlineHeadSegment(length) && i < this->head->length && !dontCreateNewArray)
                    {TRACE_IT(56191);
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
            {TRACE_IT(56192);
#if ENABLE_PROFILE_INFO
                if (arrayInfo)
                {TRACE_IT(56193);
                    arrayInfo->SetIsNotNativeArray();
                }
#endif

                #pragma prefast(suppress:6237, "The right hand side condition does not have any side effects.")
                if (sizeof(int32) < sizeof(Var) && HasInlineHeadSegment(length) && i < this->head->length && !dontCreateNewArray)
                {TRACE_IT(56194);
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
        {TRACE_IT(56195);
            Var item = args[i + 1];
            if (TaggedInt::Is(item))
            {TRACE_IT(56196);
                this->DirectSetItemAt(i, TaggedInt::ToDouble(item));
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(item))
            {TRACE_IT(56197);
                this->DirectSetItemAt(i, JavascriptNumber::GetValue(item));
            }
            else
            {TRACE_IT(56198);
                JavascriptArray *arr = JavascriptNativeFloatArray::ToVarArray(this);
#if ENABLE_PROFILE_INFO
                if (arrayInfo)
                {TRACE_IT(56199);
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
        {TRACE_IT(56200);
            Var item = args[i + 1];
            this->DirectSetItemAt(i, item);
        }

        return this;
    }

    DynamicType * JavascriptNativeIntArray::GetInitialType(ScriptContext * scriptContext)
    {TRACE_IT(56201);
        return scriptContext->GetLibrary()->GetNativeIntArrayType();
    }

#if ENABLE_COPYONACCESS_ARRAY
    DynamicType * JavascriptCopyOnAccessNativeIntArray::GetInitialType(ScriptContext * scriptContext)
    {TRACE_IT(56202);
        return scriptContext->GetLibrary()->GetCopyOnAccessNativeIntArrayType();
    }
#endif

    JavascriptNativeFloatArray *JavascriptNativeIntArray::ToNativeFloatArray(JavascriptNativeIntArray *intArray)
    {TRACE_IT(56203);
#if ENABLE_PROFILE_INFO
        ArrayCallSiteInfo *arrayInfo = intArray->GetArrayCallSiteInfo();
        if (arrayInfo)
        {TRACE_IT(56204);
#if DBG
            Js::JavascriptStackWalker walker(intArray->GetScriptContext());
            Js::JavascriptFunction* caller = nullptr;
            bool foundScriptCaller = false;
            while(walker.GetCaller(&caller))
            {TRACE_IT(56205);
                if(caller != nullptr && Js::ScriptFunction::Test(caller))
                {TRACE_IT(56206);
                    foundScriptCaller = true;
                    break;
                }
            }

            if(foundScriptCaller)
            {TRACE_IT(56207);
                Assert(caller);
                Assert(caller->GetFunctionBody());
                if(PHASE_TRACE(Js::NativeArrayConversionPhase, caller->GetFunctionBody()))
                {TRACE_IT(56208);
                    Output::Print(_u("Conversion: Int array to Float array    ArrayCreationFunctionNumber:%2d    CallSiteNumber:%2d \n"), arrayInfo->functionNumber, arrayInfo->callSiteNumber);
                    Output::Flush();
                }
            }
            else
            {TRACE_IT(56209);
                if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
                {TRACE_IT(56210);
                    Output::Print(_u("Conversion: Int array to Float array across ScriptContexts"));
                    Output::Flush();
                }
            }
#else
            if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
            {TRACE_IT(56211);
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
        {TRACE_IT(56212);
            nextSeg = seg->next;
            uint32 size = seg->size;
            if (size == 0)
            {TRACE_IT(56213);
                continue;
            }

            uint32 left = seg->left;
            uint32 length = seg->length;
            int i;
            int32 ival;

            // The old segment will have size/2 and length capped by the new size.
            seg->size >>= 1;
            if (seg == intArray->head || seg->length > (seg->size >>= 1))
            {TRACE_IT(56214);
                // Some live elements are being pushed out of this segment, so allocate a new one.
                SparseArraySegment<double> *newSeg =
                    SparseArraySegment<double>::AllocateSegment(recycler, left, length, nextSeg);
                Assert(newSeg != nullptr);
                Assert((prevSeg == nullptr) == (seg == intArray->head));
                newSeg->next = nextSeg;
                intArray->LinkSegments((SparseArraySegment<double>*)prevSeg, newSeg);
                if (intArray->GetLastUsedSegment() == seg)
                {TRACE_IT(56215);
                    intArray->SetLastUsedSegment(newSeg);
                }
                prevSeg = newSeg;
                SegmentBTree * segmentMap = intArray->GetSegmentMap();
                if (segmentMap)
                {TRACE_IT(56216);
                    segmentMap->SwapSegment(left, seg, newSeg);
                }

                // Fill the new segment with the overflow.
                for (i = 0; (uint)i < newSeg->length; i++)
                {TRACE_IT(56217);
                    ival = ((SparseArraySegment<int32>*)seg)->elements[i /*+ seg->length*/];
                    if (ival == JavascriptNativeIntArray::MissingItem)
                    {TRACE_IT(56218);
                        continue;
                    }
                    newSeg->elements[i] = (double)ival;
                }
            }
            else
            {TRACE_IT(56219);
                // Now convert the contents that will remain in the old segment.
                for (i = seg->length - 1; i >= 0; i--)
                {TRACE_IT(56220);
                    ival = ((SparseArraySegment<int32>*)seg)->elements[i];
                    if (ival == JavascriptNativeIntArray::MissingItem)
                    {TRACE_IT(56221);
                        ((SparseArraySegment<double>*)seg)->elements[i] = (double)JavascriptNativeFloatArray::MissingItem;
                    }
                    else
                    {TRACE_IT(56222);
                        ((SparseArraySegment<double>*)seg)->elements[i] = (double)ival;
                    }
                }
                prevSeg = seg;
            }
        }

        if (intArray->GetType() == scriptContext->GetLibrary()->GetNativeIntArrayType())
        {TRACE_IT(56223);
            intArray->type = scriptContext->GetLibrary()->GetNativeFloatArrayType();
        }
        else
        {TRACE_IT(56224);
            if (intArray->GetDynamicType()->GetIsLocked())
            {TRACE_IT(56225);
                DynamicTypeHandler *typeHandler = intArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {TRACE_IT(56226);
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(intArray);
                }
                else
                {TRACE_IT(56227);
                    intArray->ChangeType();
                }
            }
            intArray->GetType()->SetTypeId(TypeIds_NativeFloatArray);
        }

        if (CrossSite::IsCrossSiteObjectTyped(intArray))
        {TRACE_IT(56228);
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptNativeIntArray>>::HasVirtualTable(intArray));
            VirtualTableInfo<CrossSiteObject<JavascriptNativeFloatArray>>::SetVirtualTable(intArray);
        }
        else
        {TRACE_IT(56229);
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
    {TRACE_IT(56230);
        AssertMsg(!JavascriptNativeArray::Is(varArray), "Ensure that the incoming Array is a Var array");

        if (varArray->GetType() == scriptContext->GetLibrary()->GetArrayType())
        {TRACE_IT(56231);
            varArray->type = scriptContext->GetLibrary()->GetNativeFloatArrayType();
        }
        else
        {TRACE_IT(56232);
            if (varArray->GetDynamicType()->GetIsLocked())
            {TRACE_IT(56233);
                DynamicTypeHandler *typeHandler = varArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {TRACE_IT(56234);
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(varArray);
                }
                else
                {TRACE_IT(56235);
                    varArray->ChangeType();
                }
            }
            varArray->GetType()->SetTypeId(TypeIds_NativeFloatArray);
        }

        if (CrossSite::IsCrossSiteObjectTyped(varArray))
        {TRACE_IT(56236);
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptArray>>::HasVirtualTable(varArray));
            VirtualTableInfo<CrossSiteObject<JavascriptNativeFloatArray>>::SetVirtualTable(varArray);
        }
        else
        {TRACE_IT(56237);
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
    {TRACE_IT(56238);
        AssertMsg(!JavascriptNativeArray::Is(varArray), "Ensure that the incoming Array is a Var array");

        if (varArray->GetType() == scriptContext->GetLibrary()->GetArrayType())
        {TRACE_IT(56239);
            varArray->type = scriptContext->GetLibrary()->GetNativeIntArrayType();
        }
        else
        {TRACE_IT(56240);
            if (varArray->GetDynamicType()->GetIsLocked())
            {TRACE_IT(56241);
                DynamicTypeHandler *typeHandler = varArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {TRACE_IT(56242);
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(varArray);
                }
                else
                {TRACE_IT(56243);
                    varArray->ChangeType();
                }
            }
            varArray->GetType()->SetTypeId(TypeIds_NativeIntArray);
        }

        if (CrossSite::IsCrossSiteObjectTyped(varArray))
        {TRACE_IT(56244);
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptArray>>::HasVirtualTable(varArray));
            VirtualTableInfo<CrossSiteObject<JavascriptNativeIntArray>>::SetVirtualTable(varArray);
        }
        else
        {TRACE_IT(56245);
            Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(varArray));
            VirtualTableInfo<JavascriptNativeIntArray>::SetVirtualTable(varArray);
        }
    }

    template<>
    int32 JavascriptArray::GetNativeValue<int32>(Js::Var ival, ScriptContext * scriptContext)
    {TRACE_IT(56246);
        return JavascriptConversion::ToInt32(ival, scriptContext);
    }

    template <>
    double JavascriptArray::GetNativeValue<double>(Var ival, ScriptContext * scriptContext)
    {TRACE_IT(56247);
        return JavascriptConversion::ToNumber(ival, scriptContext);
    }


    /*
    *   JavascriptArray::ConvertToNativeArrayInPlace
    *   In place conversion of all Var elements to Native Int/Double elements in an array.
    *   We do not update the DynamicProfileInfo of the array here.
    */
    template<typename NativeArrayType, typename T>
    NativeArrayType *JavascriptArray::ConvertToNativeArrayInPlace(JavascriptArray *varArray)
    {TRACE_IT(56248);
        AssertMsg(!JavascriptNativeArray::Is(varArray), "Ensure that the incoming Array is a Var array");

        ScriptContext *scriptContext = varArray->GetScriptContext();
        SparseArraySegmentBase *seg, *nextSeg, *prevSeg = nullptr;
        for (seg = varArray->head; seg; seg = nextSeg)
        {TRACE_IT(56249);
            nextSeg = seg->next;
            uint32 size = seg->size;
            if (size == 0)
            {TRACE_IT(56250);
                continue;
            }

            int i;
            Var ival;

            uint32 growFactor = sizeof(Var) / sizeof(T);
            AssertMsg(growFactor == 1, "We support only in place conversion of Var array to Native Array");

            // Now convert the contents that will remain in the old segment.
            for (i = seg->length - 1; i >= 0; i--)
            {TRACE_IT(56251);
                ival = ((SparseArraySegment<Var>*)seg)->elements[i];
                if (ival == JavascriptArray::MissingItem)
                {TRACE_IT(56252);
                    ((SparseArraySegment<T>*)seg)->elements[i] = NativeArrayType::MissingItem;
                }
                else
                {TRACE_IT(56253);
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
    {TRACE_IT(56254);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(intArray);
#endif
        ScriptContext *scriptContext = intArray->GetScriptContext();
        Recycler *recycler = scriptContext->GetRecycler();
        SparseArraySegmentBase *seg, *nextSeg, *prevSeg = nullptr;
        for (seg = intArray->head; seg; seg = nextSeg)
        {TRACE_IT(56255);
            nextSeg = seg->next;
            uint32 size = seg->size;
            if (size == 0)
            {TRACE_IT(56256);
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
            {TRACE_IT(56257);
                // Some live elements are being pushed out of this segment, so allocate a new one.
                // And/or the old segment is not scanned by the recycler, so we need a new one to hold vars.
                SparseArraySegment<Var> *newSeg =
                    SparseArraySegment<Var>::AllocateSegment(recycler, left, length, nextSeg);

                AnalysisAssert(newSeg);
                Assert((prevSeg == nullptr) == (seg == intArray->head));
                newSeg->next = nextSeg;
                intArray->LinkSegments((SparseArraySegment<Var>*)prevSeg, newSeg);
                if (intArray->GetLastUsedSegment() == seg)
                {TRACE_IT(56258);
                    intArray->SetLastUsedSegment(newSeg);
                }
                prevSeg = newSeg;

                SegmentBTree * segmentMap = intArray->GetSegmentMap();
                if (segmentMap)
                {TRACE_IT(56259);
                    segmentMap->SwapSegment(left, seg, newSeg);
                }

                // Fill the new segment with the overflow.
                for (i = 0; (uint)i < newSeg->length; i++)
                {TRACE_IT(56260);
                    ival = ((SparseArraySegment<int32>*)seg)->elements[i];
                    if (ival == JavascriptNativeIntArray::MissingItem)
                    {TRACE_IT(56261);
                        continue;
                    }
                    newSeg->elements[i] = JavascriptNumber::ToVar(ival, scriptContext);
                }
            }
            else
            {TRACE_IT(56262);
                // Now convert the contents that will remain in the old segment.
                // Walk backward in case we're growing the element size.
                for (i = seg->length - 1; i >= 0; i--)
                {TRACE_IT(56263);
                    ival = ((SparseArraySegment<int32>*)seg)->elements[i];
                    if (ival == JavascriptNativeIntArray::MissingItem)
                    {TRACE_IT(56264);
                        ((SparseArraySegment<Var>*)seg)->elements[i] = (Var)JavascriptArray::MissingItem;
                    }
                    else
                    {TRACE_IT(56265);
                        ((SparseArraySegment<Var>*)seg)->elements[i] = JavascriptNumber::ToVar(ival, scriptContext);
                    }
                }
                prevSeg = seg;
            }
        }

        if (intArray->GetType() == scriptContext->GetLibrary()->GetNativeIntArrayType())
        {TRACE_IT(56266);
            intArray->type = scriptContext->GetLibrary()->GetArrayType();
        }
        else
        {TRACE_IT(56267);
            if (intArray->GetDynamicType()->GetIsLocked())
            {TRACE_IT(56268);
                DynamicTypeHandler *typeHandler = intArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {TRACE_IT(56269);
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(intArray);
                }
                else
                {TRACE_IT(56270);
                    intArray->ChangeType();
                }
            }
            intArray->GetType()->SetTypeId(TypeIds_Array);
        }

        if (CrossSite::IsCrossSiteObjectTyped(intArray))
        {TRACE_IT(56271);
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptNativeIntArray>>::HasVirtualTable(intArray));
            VirtualTableInfo<CrossSiteObject<JavascriptArray>>::SetVirtualTable(intArray);
        }
        else
        {TRACE_IT(56272);
            Assert(VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(intArray));
            VirtualTableInfo<JavascriptArray>::SetVirtualTable(intArray);
        }

        return intArray;
    }
    JavascriptArray *JavascriptNativeIntArray::ToVarArray(JavascriptNativeIntArray *intArray)
    {TRACE_IT(56273);
#if ENABLE_PROFILE_INFO
        ArrayCallSiteInfo *arrayInfo = intArray->GetArrayCallSiteInfo();
        if (arrayInfo)
        {TRACE_IT(56274);
#if DBG
            Js::JavascriptStackWalker walker(intArray->GetScriptContext());
            Js::JavascriptFunction* caller = nullptr;
            bool foundScriptCaller = false;
            while(walker.GetCaller(&caller))
            {TRACE_IT(56275);
                if(caller != nullptr && Js::ScriptFunction::Test(caller))
                {TRACE_IT(56276);
                    foundScriptCaller = true;
                    break;
                }
            }

            if(foundScriptCaller)
            {TRACE_IT(56277);
                Assert(caller);
                Assert(caller->GetFunctionBody());
                if(PHASE_TRACE(Js::NativeArrayConversionPhase, caller->GetFunctionBody()))
                {TRACE_IT(56278);
                    Output::Print(_u("Conversion: Int array to Var array    ArrayCreationFunctionNumber:%2d    CallSiteNumber:%2d \n"), arrayInfo->functionNumber, arrayInfo->callSiteNumber);
                    Output::Flush();
                }
            }
            else
            {TRACE_IT(56279);
                if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
                {TRACE_IT(56280);
                    Output::Print(_u("Conversion: Int array to Var array across ScriptContexts"));
                    Output::Flush();
                }
            }
#else
            if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
            {TRACE_IT(56281);
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
    {TRACE_IT(56282);
        return scriptContext->GetLibrary()->GetNativeFloatArrayType();
    }

    /*
    *   JavascriptNativeFloatArray::ConvertToVarArray
    *   This function only converts all Float elements to Var elements in an array.
    *   DynamicProfileInfo of the array is not updated in this function.
    */
    JavascriptArray *JavascriptNativeFloatArray::ConvertToVarArray(JavascriptNativeFloatArray *fArray)
    {TRACE_IT(56283);
        // We can't be growing the size of the element.
        Assert(sizeof(double) >= sizeof(Var));

        uint32 shrinkFactor = sizeof(double) / sizeof(Var);
        ScriptContext *scriptContext = fArray->GetScriptContext();
        Recycler *recycler = scriptContext->GetRecycler();
        SparseArraySegmentBase *seg, *nextSeg, *prevSeg = nullptr;
        for (seg = fArray->head; seg; seg = nextSeg)
        {TRACE_IT(56284);
            nextSeg = seg->next;
            if (seg->size == 0)
            {TRACE_IT(56285);
                continue;
            }
            uint32 left = seg->left;
            uint32 length = seg->length;
            SparseArraySegment<Var> *newSeg;
            if (seg->next == nullptr && SparseArraySegmentBase::IsLeafSegment(seg, recycler))
            {TRACE_IT(56286);
                // The old segment is not scanned by the recycler, so we need a new one to hold vars.
                newSeg =
                    SparseArraySegment<Var>::AllocateSegment(recycler, left, length, nextSeg);
                Assert((prevSeg == nullptr) == (seg == fArray->head));
                newSeg->next = nextSeg;
                fArray->LinkSegments((SparseArraySegment<Var>*)prevSeg, newSeg);
                if (fArray->GetLastUsedSegment() == seg)
                {TRACE_IT(56287);
                    fArray->SetLastUsedSegment(newSeg);
                }
                prevSeg = newSeg;

                SegmentBTree * segmentMap = fArray->GetSegmentMap();
                if (segmentMap)
                {TRACE_IT(56288);
                    segmentMap->SwapSegment(left, seg, newSeg);
                }
            }
            else
            {TRACE_IT(56289);
                newSeg = (SparseArraySegment<Var>*)seg;
                prevSeg = seg;
                if (shrinkFactor != 1)
                {TRACE_IT(56290);
                    uint32 newSize = seg->size * shrinkFactor;
                    uint32 limit;
                    if (seg->next)
                    {TRACE_IT(56291);
                        limit = seg->next->left;
                    }
                    else
                    {TRACE_IT(56292);
                        limit = JavascriptArray::MaxArrayLength;
                    }
                    seg->size = min(newSize, limit - seg->left);
                }
            }
            uint32 i;
            for (i = 0; i < seg->length; i++)
            {TRACE_IT(56293);
                if (SparseArraySegment<double>::IsMissingItem(&((SparseArraySegment<double>*)seg)->elements[i]))
                {TRACE_IT(56294);
                    if (seg == newSeg)
                    {TRACE_IT(56295);
                        newSeg->elements[i] = (Var)JavascriptArray::MissingItem;
                    }
                    Assert(newSeg->elements[i] == (Var)JavascriptArray::MissingItem);
                }
                else if (*(uint64*)&(((SparseArraySegment<double>*)seg)->elements[i]) == 0ull)
                {TRACE_IT(56296);
                    newSeg->elements[i] = TaggedInt::ToVarUnchecked(0);
                }
                else
                {TRACE_IT(56297);
                    int32 ival;
                    double dval = ((SparseArraySegment<double>*)seg)->elements[i];
                    if (JavascriptNumber::TryGetInt32Value(dval, &ival) && !TaggedInt::IsOverflow(ival))
                    {TRACE_IT(56298);
                        newSeg->elements[i] = TaggedInt::ToVarUnchecked(ival);
                    }
                    else
                    {TRACE_IT(56299);
                        newSeg->elements[i] = JavascriptNumber::ToVarWithCheck(dval, scriptContext);
                    }
                }
            }
            if (seg == newSeg && shrinkFactor != 1)
            {TRACE_IT(56300);
                // Fill the remaining slots.
                newSeg->FillSegmentBuffer(i, seg->size);
            }
        }

        if (fArray->GetType() == scriptContext->GetLibrary()->GetNativeFloatArrayType())
        {TRACE_IT(56301);
            fArray->type = scriptContext->GetLibrary()->GetArrayType();
        }
        else
        {TRACE_IT(56302);
            if (fArray->GetDynamicType()->GetIsLocked())
            {TRACE_IT(56303);
                DynamicTypeHandler *typeHandler = fArray->GetDynamicType()->GetTypeHandler();
                if (typeHandler->IsPathTypeHandler())
                {TRACE_IT(56304);
                    // We can't allow a type with the new type ID to be promoted to the old type.
                    // So go to a dictionary type handler, which will orphan the new type.
                    // This should be a corner case, so the inability to share the new type is unlikely to matter.
                    // If it does matter, try building a path from the new type's built-in root.
                    static_cast<PathTypeHandlerBase*>(typeHandler)->ResetTypeHandler(fArray);
                }
                else
                {TRACE_IT(56305);
                    fArray->ChangeType();
                }
            }
            fArray->GetType()->SetTypeId(TypeIds_Array);
        }

        if (CrossSite::IsCrossSiteObjectTyped(fArray))
        {TRACE_IT(56306);
            Assert(VirtualTableInfo<CrossSiteObject<JavascriptNativeFloatArray>>::HasVirtualTable(fArray));
            VirtualTableInfo<CrossSiteObject<JavascriptArray>>::SetVirtualTable(fArray);
        }
        else
        {TRACE_IT(56307);
            Assert(VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(fArray));
            VirtualTableInfo<JavascriptArray>::SetVirtualTable(fArray);
        }

        return fArray;
    }

    JavascriptArray *JavascriptNativeFloatArray::ToVarArray(JavascriptNativeFloatArray *fArray)
    {TRACE_IT(56308);
#if ENABLE_PROFILE_INFO
        ArrayCallSiteInfo *arrayInfo = fArray->GetArrayCallSiteInfo();
        if (arrayInfo)
        {TRACE_IT(56309);
#if DBG
            Js::JavascriptStackWalker walker(fArray->GetScriptContext());
            Js::JavascriptFunction* caller = nullptr;
            bool foundScriptCaller = false;
            while(walker.GetCaller(&caller))
            {TRACE_IT(56310);
                if(caller != nullptr && Js::ScriptFunction::Test(caller))
                {TRACE_IT(56311);
                    foundScriptCaller = true;
                    break;
                }
            }

            if(foundScriptCaller)
            {TRACE_IT(56312);
                Assert(caller);
                Assert(caller->GetFunctionBody());
                if(PHASE_TRACE(Js::NativeArrayConversionPhase, caller->GetFunctionBody()))
                {TRACE_IT(56313);
                    Output::Print(_u("Conversion: Float array to Var array    ArrayCreationFunctionNumber:%2d    CallSiteNumber:%2d \n"), arrayInfo->functionNumber, arrayInfo->callSiteNumber);
                    Output::Flush();
                }
            }
            else
            {TRACE_IT(56314);
                if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
                {TRACE_IT(56315);
                    Output::Print(_u("Conversion: Float array to Var array across ScriptContexts"));
                    Output::Flush();
                }
            }
#else
            if(PHASE_TRACE1(Js::NativeArrayConversionPhase))
            {TRACE_IT(56316);
                Output::Print(_u("Conversion: Float array to Var array"));
                Output::Flush();
            }
#endif

            if(fArray->GetScriptContext()->IsScriptContextInNonDebugMode())
            {TRACE_IT(56317);
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
    {TRACE_IT(56318);
        int64 index;

        if (TaggedInt::Is(arg))
        {TRACE_IT(56319);
            int intValue = TaggedInt::ToInt32(arg);

            if (intValue < 0)
            {TRACE_IT(56320);
                index = max<int64>(0, length + intValue);
            }
            else
            {TRACE_IT(56321);
                index = intValue;
            }

            if (index > length)
            {TRACE_IT(56322);
                index = length;
            }
        }
        else
        {TRACE_IT(56323);
            double doubleValue = JavascriptConversion::ToInteger(arg, scriptContext);

            // Handle the Number.POSITIVE_INFINITY case
            if (doubleValue > length)
            {TRACE_IT(56324);
                return length;
            }

            index = NumberUtilities::TryToInt64(doubleValue);

            if (index < 0)
            {TRACE_IT(56325);
                index = max<int64>(0, index + length);
            }
        }

        return index;
    }

    TypeId JavascriptArray::OP_SetNativeIntElementC(JavascriptNativeIntArray *arr, uint32 index, Var value, ScriptContext *scriptContext)
    {TRACE_IT(56326);
        int32 iValue;
        double dValue;

        TypeId typeId = arr->TrySetNativeIntArrayItem(value, &iValue, &dValue);
        if (typeId == TypeIds_NativeIntArray)
        {TRACE_IT(56327);
            arr->SetArrayLiteralItem(index, iValue);
        }
        else if (typeId == TypeIds_NativeFloatArray)
        {TRACE_IT(56328);
            arr->SetArrayLiteralItem(index, dValue);
        }
        else
        {TRACE_IT(56329);
            arr->SetArrayLiteralItem(index, value);
        }
        return typeId;
    }

    TypeId JavascriptArray::OP_SetNativeFloatElementC(JavascriptNativeFloatArray *arr, uint32 index, Var value, ScriptContext *scriptContext)
    {TRACE_IT(56330);
        double dValue;
        TypeId typeId = arr->TrySetNativeFloatArrayItem(value, &dValue);
        if (typeId == TypeIds_NativeFloatArray)
        {TRACE_IT(56331);
            arr->SetArrayLiteralItem(index, dValue);
        }
        else
        {TRACE_IT(56332);
            arr->SetArrayLiteralItem(index, value);
        }
        return typeId;
    }

    template<typename T>
    void JavascriptArray::SetArrayLiteralItem(uint32 index, T value)
    {TRACE_IT(56333);
        SparseArraySegment<T> * segment = SparseArraySegment<T>::From(this->head);

        Assert(segment->left == 0);
        Assert(index < segment->length);

        segment->elements[index] = value;
    }

    void JavascriptNativeIntArray::SetIsPrototype()
    {TRACE_IT(56334);
        // Force the array to be non-native to simplify inspection, filling from proto, etc.
        ToVarArray(this);
        __super::SetIsPrototype();
    }

    void JavascriptNativeFloatArray::SetIsPrototype()
    {TRACE_IT(56335);
        // Force the array to be non-native to simplify inspection, filling from proto, etc.
        ToVarArray(this);
        __super::SetIsPrototype();
    }

#if ENABLE_PROFILE_INFO
    ArrayCallSiteInfo *JavascriptNativeArray::GetArrayCallSiteInfo()
    {TRACE_IT(56336);
        RecyclerWeakReference<FunctionBody> *weakRef = this->weakRefToFuncBody;
        if (weakRef)
        {TRACE_IT(56337);
            FunctionBody *functionBody = weakRef->Get();
            if (functionBody)
            {TRACE_IT(56338);
                if (functionBody->HasDynamicProfileInfo())
                {TRACE_IT(56339);
                    Js::ProfileId profileId = this->GetArrayCallSiteIndex();
                    if (profileId < functionBody->GetProfiledArrayCallSiteCount())
                    {TRACE_IT(56340);
                        return functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);
                    }
                }
            }
            else
            {TRACE_IT(56341);
                this->ClearArrayCallSiteIndex();
            }
        }
        return nullptr;
    }

    void JavascriptNativeArray::SetArrayProfileInfo(RecyclerWeakReference<FunctionBody> *weakRef, ArrayCallSiteInfo *arrayInfo)
    {TRACE_IT(56342);
        Assert(weakRef);
        FunctionBody *functionBody = weakRef->Get();
        if (functionBody && functionBody->HasDynamicProfileInfo())
        {TRACE_IT(56343);
            ArrayCallSiteInfo *baseInfo = functionBody->GetAnyDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, 0);
            Js::ProfileId index = (Js::ProfileId)(arrayInfo - baseInfo);
            Assert(index < functionBody->GetProfiledArrayCallSiteCount());
            SetArrayCallSite(index, weakRef);
        }
    }

    void JavascriptNativeArray::CopyArrayProfileInfo(Js::JavascriptNativeArray* baseArray)
    {TRACE_IT(56344);
        if (baseArray->weakRefToFuncBody)
        {TRACE_IT(56345);
            if (baseArray->weakRefToFuncBody->Get())
            {TRACE_IT(56346);
                SetArrayCallSite(baseArray->GetArrayCallSiteIndex(), baseArray->weakRefToFuncBody);
            }
            else
            {TRACE_IT(56347);
                baseArray->ClearArrayCallSiteIndex();
            }
        }
    }
#endif

    Var JavascriptNativeArray::FindMinOrMax(Js::ScriptContext * scriptContext, bool findMax)
    {TRACE_IT(56348);
        if (JavascriptNativeIntArray::Is(this))
        {TRACE_IT(56349);
            return this->FindMinOrMax<int32, false>(scriptContext, findMax);
        }
        else
        {TRACE_IT(56350);
            return this->FindMinOrMax<double, true>(scriptContext, findMax);
        }
    }

    template <typename T, bool checkNaNAndNegZero>
    Var JavascriptNativeArray::FindMinOrMax(Js::ScriptContext * scriptContext, bool findMax)
    {TRACE_IT(56351);
        AssertMsg(this->HasNoMissingValues(), "Fastpath is only for arrays with one segment and no missing values");
        uint len = this->GetLength();

        Js::SparseArraySegment<T>* headSegment = ((Js::SparseArraySegment<T>*)this->GetHead());
        uint headSegLen = headSegment->length;
        Assert(headSegLen == len);

        if (headSegment->next == nullptr)
        {TRACE_IT(56352);
            T currentRes = headSegment->elements[0];
            for (uint i = 0; i < headSegLen; i++)
            {TRACE_IT(56353);
                T compare = headSegment->elements[i];
                if (checkNaNAndNegZero && JavascriptNumber::IsNan(double(compare)))
                {TRACE_IT(56354);
                    return scriptContext->GetLibrary()->GetNaN();
                }
                if (findMax ? currentRes < compare : currentRes > compare ||
                    (checkNaNAndNegZero && compare == 0 && Js::JavascriptNumber::IsNegZero(double(currentRes))))
                {TRACE_IT(56355);
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
    {TRACE_IT(56356);
        return HasSegmentMap() ?
            PointerValue(segmentUnion.segmentBTreeRoot->lastUsedSegment) :
            PointerValue(segmentUnion.lastUsedSegment);
    }

    void JavascriptArray::SetHeadAndLastUsedSegment(SparseArraySegmentBase * segment)
    {TRACE_IT(56357);

        Assert(!HasSegmentMap());
        this->head = this->segmentUnion.lastUsedSegment = segment;
    }

    void JavascriptArray::SetLastUsedSegment(SparseArraySegmentBase * segment)
    {TRACE_IT(56358);
        if (HasSegmentMap())
        {TRACE_IT(56359);
            this->segmentUnion.segmentBTreeRoot->lastUsedSegment = segment;
        }
        else
        {TRACE_IT(56360);
            this->segmentUnion.lastUsedSegment = segment;
        }
    }
    bool JavascriptArray::HasSegmentMap() const
    {
        return !!(GetFlags() & DynamicObjectFlags::HasSegmentMap);
    }

    SegmentBTreeRoot * JavascriptArray::GetSegmentMap() const
    {TRACE_IT(56362);
        return (HasSegmentMap() ? segmentUnion.segmentBTreeRoot : nullptr);
    }

    void JavascriptArray::SetSegmentMap(SegmentBTreeRoot * segmentMap)
    {TRACE_IT(56363);
        Assert(!HasSegmentMap());
        SparseArraySegmentBase * lastUsedSeg = this->segmentUnion.lastUsedSegment;
        SetFlags(GetFlags() | DynamicObjectFlags::HasSegmentMap);
        segmentUnion.segmentBTreeRoot = segmentMap;
        segmentMap->lastUsedSegment = lastUsedSeg;
    }

    void JavascriptArray::ClearSegmentMap()
    {TRACE_IT(56364);
        if (HasSegmentMap())
        {TRACE_IT(56365);
            SetFlags(GetFlags() & ~DynamicObjectFlags::HasSegmentMap);
            SparseArraySegmentBase * lastUsedSeg = segmentUnion.segmentBTreeRoot->lastUsedSegment;
            segmentUnion.segmentBTreeRoot = nullptr;
            segmentUnion.lastUsedSegment = lastUsedSeg;
        }
    }

    SegmentBTreeRoot * JavascriptArray::BuildSegmentMap()
    {TRACE_IT(56366);
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
    {TRACE_IT(56367);
        SegmentBTreeRoot * savedSegmentMap = GetSegmentMap();
        if (savedSegmentMap)
        {TRACE_IT(56368);
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
    {TRACE_IT(56369);
        this->SetLastUsedSegment(this->head);
    }

    DescriptorFlags JavascriptArray::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(56370);
        DescriptorFlags flags;
        if (GetSetterBuiltIns(propertyId, info, &flags))
        {TRACE_IT(56371);
            return flags;
        }
        return __super::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags JavascriptArray::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(56372);
        DescriptorFlags flags;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetSetterBuiltIns(propertyRecord->GetPropertyId(), info, &flags))
        {TRACE_IT(56373);
            return flags;
        }

        return __super::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool JavascriptArray::GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* descriptorFlags)
    {TRACE_IT(56374);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(56375);
            PropertyValueInfo::SetNoCache(info, this);
            *descriptorFlags = WritableData;
            return true;
        }
        return false;
    }

    SparseArraySegmentBase * JavascriptArray::GetBeginLookupSegment(uint32 index, const bool useSegmentMap) const
    {TRACE_IT(56376);
        SparseArraySegmentBase *seg = nullptr;
        SparseArraySegmentBase * lastUsedSeg = this->GetLastUsedSegment();
        if (lastUsedSeg != nullptr && lastUsedSeg->left <= index)
        {TRACE_IT(56377);
            seg = lastUsedSeg;
            if(index - lastUsedSeg->left < lastUsedSeg->size)
            {TRACE_IT(56378);
                return seg;
            }
        }

        SegmentBTreeRoot * segmentMap = GetSegmentMap();
        if(!useSegmentMap || !segmentMap)
        {TRACE_IT(56379);
            return seg ? seg : PointerValue(this->head);
        }

        if(seg)
        {TRACE_IT(56380);
            // If indexes are being accessed sequentially, check the segment after the last-used segment before checking the
            // segment map, as it is likely to hit
            SparseArraySegmentBase *const nextSeg = seg->next;
            if(nextSeg)
            {TRACE_IT(56381);
                if(index < nextSeg->left)
                {TRACE_IT(56382);
                    return seg;
                }
                else if(index - nextSeg->left < nextSeg->size)
                {TRACE_IT(56383);
                    return nextSeg;
                }
            }
        }

        SparseArraySegmentBase *matchOrNextSeg;
        segmentMap->Find(index, seg, matchOrNextSeg);
        return seg ? seg : matchOrNextSeg;
    }

    uint32 JavascriptArray::GetNextIndex(uint32 index) const
    {TRACE_IT(56384);
        if (JavascriptNativeIntArray::Is((Var)this))
        {TRACE_IT(56385);
            return this->GetNextIndexHelper<int32>(index);
        }
        else if (JavascriptNativeFloatArray::Is((Var)this))
        {TRACE_IT(56386);
            return this->GetNextIndexHelper<double>(index);
        }
        return this->GetNextIndexHelper<Var>(index);
    }

    template<typename T>
    uint32 JavascriptArray::GetNextIndexHelper(uint32 index) const
    {TRACE_IT(56387);
        AssertMsg(this->head, "array head should never be null");
        uint candidateIndex;

        if (index == JavascriptArray::InvalidIndex)
        {TRACE_IT(56388);
            candidateIndex = head->left;
        }
        else
        {TRACE_IT(56389);
            candidateIndex = index + 1;
        }

        SparseArraySegment<T>* current = (SparseArraySegment<T>*)this->GetBeginLookupSegment(candidateIndex);

        while (current != nullptr)
        {TRACE_IT(56390);
            if ((current->left <= candidateIndex) && ((candidateIndex - current->left) < current->length))
            {TRACE_IT(56391);
                for (uint i = candidateIndex - current->left; i < current->length; i++)
                {TRACE_IT(56392);
                    if (!SparseArraySegment<T>::IsMissingItem(&current->elements[i]))
                    {TRACE_IT(56393);
                        return i + current->left;
                    }
                }
            }
            current = SparseArraySegment<T>::From(current->next);
            if (current != NULL)
            {TRACE_IT(56394);
                if (candidateIndex < current->left)
                {TRACE_IT(56395);
                    candidateIndex = current->left;
                }
            }
        }
        return JavascriptArray::InvalidIndex;
    }

    // If new length > length, we just reset the length
    // If new length < length, we need to remove the rest of the elements and segment
    void JavascriptArray::SetLength(uint32 newLength)
    {TRACE_IT(56396);
        if (newLength == length)
            return;

        if (head == EmptySegment)
        {TRACE_IT(56397);
            // Do nothing to the segment.
        }
        else if (newLength == 0)
        {TRACE_IT(56398);
            this->ClearElements(head, 0);
            head->length = 0;
            head->next = nullptr;
            SetHasNoMissingValues();

            ClearSegmentMap();
            this->InvalidateLastUsedSegment();
        }
        else if (newLength < length)
        {TRACE_IT(56399);
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
            {TRACE_IT(56400);
                if (newLength <= next->left)
                {TRACE_IT(56401);
                    ClearSegmentMap(); // truncate segments, null out segmentMap
                    *prev = nullptr;
                    break;
                }
                else if (newLength <= (next->left + next->length))
                {TRACE_IT(56402);
                    if (next->next)
                    {TRACE_IT(56403);
                        ClearSegmentMap(); // Will truncate segments, null out segmentMap
                    }

                    uint32 newSegmentLength = newLength - next->left;
                    this->ClearElements(next, newSegmentLength);
                    next->next = nullptr;
                    next->length = newSegmentLength;
                    break;
                }
                else
                {TRACE_IT(56404);
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
    {TRACE_IT(56405);
        ScriptContext *scriptContext;
        if(TaggedInt::Is(newLength))
        {TRACE_IT(56406);
            int32 lenValue = TaggedInt::ToInt32(newLength);
            if (lenValue < 0)
            {TRACE_IT(56407);
                scriptContext = GetScriptContext();
                if (scriptContext->GetThreadContext()->RecordImplicitException())
                {TRACE_IT(56408);
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
                }
            }
            else
            {TRACE_IT(56409);
                this->SetLength(lenValue);
            }
            return TRUE;
        }

        scriptContext = GetScriptContext();
        uint32 uintValue = JavascriptConversion::ToUInt32(newLength, scriptContext);
        double dblValue = JavascriptConversion::ToNumber(newLength, scriptContext);
        if (dblValue == uintValue)
        {TRACE_IT(56410);
            this->SetLength(uintValue);
        }
        else
        {TRACE_IT(56411);
            ThreadContext* threadContext = scriptContext->GetThreadContext();
            ImplicitCallFlags flags = threadContext->GetImplicitCallFlags();
            if (flags != ImplicitCall_None && threadContext->IsDisableImplicitCall())
            {TRACE_IT(56412);
                // We couldn't execute the implicit call(s) needed to convert the newLength to an integer.
                // Do nothing and let the jitted code bail out.
                return TRUE;
            }

            if (threadContext->RecordImplicitException())
            {TRACE_IT(56413);
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
            }
        }

        return TRUE;
    }

    void JavascriptArray::ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength)
    {TRACE_IT(56414);
        SparseArraySegment<Var>::ClearElements(((SparseArraySegment<Var>*)seg)->elements + newSegmentLength, seg->length - newSegmentLength);
    }

    void JavascriptNativeIntArray::ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength)
    {TRACE_IT(56415);
        SparseArraySegment<int32>::ClearElements(((SparseArraySegment<int32>*)seg)->elements + newSegmentLength, seg->length - newSegmentLength);
    }

    void JavascriptNativeFloatArray::ClearElements(SparseArraySegmentBase *seg, uint32 newSegmentLength)
    {TRACE_IT(56416);
        SparseArraySegment<double>::ClearElements(((SparseArraySegment<double>*)seg)->elements + newSegmentLength, seg->length - newSegmentLength);
    }

    Var JavascriptArray::DirectGetItem(uint32 index)
    {TRACE_IT(56417);
        SparseArraySegment<Var> *seg = (SparseArraySegment<Var>*)this->GetLastUsedSegment();
        uint32 offset = index - seg->left;
        if (index >= seg->left && offset < seg->length)
        {TRACE_IT(56418);
            if (!SparseArraySegment<Var>::IsMissingItem(&seg->elements[offset]))
            {TRACE_IT(56419);
                return seg->elements[offset];
            }
        }
        Var element;
        if (DirectGetItemAtFull(index, &element))
        {TRACE_IT(56420);
            return element;
        }
        return GetType()->GetLibrary()->GetUndefined();
    }

    Var JavascriptNativeIntArray::DirectGetItem(uint32 index)
    {TRACE_IT(56421);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        SparseArraySegment<int32> *seg = (SparseArraySegment<int32>*)this->GetLastUsedSegment();
        uint32 offset = index - seg->left;
        if (index >= seg->left && offset < seg->length)
        {TRACE_IT(56422);
            if (!SparseArraySegment<int32>::IsMissingItem(&seg->elements[offset]))
            {TRACE_IT(56423);
                return JavascriptNumber::ToVar(seg->elements[offset], GetScriptContext());
            }
        }
        Var element;
        if (DirectGetItemAtFull(index, &element))
        {TRACE_IT(56424);
            return element;
        }
        return GetType()->GetLibrary()->GetUndefined();
    }

    DescriptorFlags JavascriptNativeIntArray::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {TRACE_IT(56425);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        int32 value = 0;
        return this->DirectGetItemAt(index, &value) ? WritableData : None;
    }


    Var JavascriptNativeFloatArray::DirectGetItem(uint32 index)
    {TRACE_IT(56426);
        SparseArraySegment<double> *seg = (SparseArraySegment<double>*)this->GetLastUsedSegment();
        uint32 offset = index - seg->left;
        if (index >= seg->left && offset < seg->length)
        {TRACE_IT(56427);
            if (!SparseArraySegment<double>::IsMissingItem(&seg->elements[offset]))
            {TRACE_IT(56428);
                return JavascriptNumber::ToVarWithCheck(seg->elements[offset], GetScriptContext());
            }
        }
        Var element;
        if (DirectGetItemAtFull(index, &element))
        {TRACE_IT(56429);
            return element;
        }
        return GetType()->GetLibrary()->GetUndefined();
    }

    Var JavascriptArray::DirectGetItem(JavascriptString *propName, ScriptContext* scriptContext)
    {TRACE_IT(56430);
        PropertyRecord const * propertyRecord;
        scriptContext->GetOrAddPropertyRecord(propName->GetString(), propName->GetLength(), &propertyRecord);
        return JavascriptOperators::GetProperty(this, propertyRecord->GetPropertyId(), scriptContext, NULL);
    }

    BOOL JavascriptArray::DirectGetItemAtFull(uint32 index, Var* outVal)
    {TRACE_IT(56431);
        if (this->DirectGetItemAt(index, outVal))
        {TRACE_IT(56432);
            return TRUE;
        }

        ScriptContext* requestContext = type->GetScriptContext();
        return JavascriptOperators::GetItem(this, this->GetPrototype(), index, outVal, requestContext);
    }

    //
    // Link prev and current. If prev is NULL, make current the head segment.
    //
    void JavascriptArray::LinkSegmentsCommon(SparseArraySegmentBase* prev, SparseArraySegmentBase* current)
    {TRACE_IT(56433);
        if (prev)
        {TRACE_IT(56434);
            prev->next = current;
        }
        else
        {TRACE_IT(56435);
            Assert(current);
            head = current;
        }
    }

    template<typename T>
    BOOL JavascriptArray::DirectDeleteItemAt(uint32 itemIndex)
    {TRACE_IT(56436);
        if (itemIndex >= length)
        {TRACE_IT(56437);
            return true;
        }
        SparseArraySegment<T>* next = (SparseArraySegment<T>*)GetBeginLookupSegment(itemIndex);
        while(next != nullptr && next->left <= itemIndex)
        {TRACE_IT(56438);
            uint32 limit = next->left + next->length;
            if (itemIndex < limit)
            {TRACE_IT(56439);
                next->SetElement(GetRecycler(), itemIndex, SparseArraySegment<T>::GetMissingItem());
                if(itemIndex - next->left == next->length - 1)
                {TRACE_IT(56440);
                    --next->length;
                }
                else if(next == head)
                {TRACE_IT(56441);
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
    {TRACE_IT(56442);
        return idxDest.ToNumber(scriptContext);
    }

    template <> uint32 JavascriptArray::ConvertToIndex(BigIndex idxDest, ScriptContext* scriptContext)
    {TRACE_IT(56443);
        // Note this is only for setting Array length which is a uint32
        return idxDest.IsSmallIndex() ? idxDest.GetSmallIndex() : UINT_MAX;
    }

    template <> Var JavascriptArray::ConvertToIndex(uint32 idxDest, ScriptContext* scriptContext)
    {TRACE_IT(56444);
        return  JavascriptNumber::ToVar(idxDest, scriptContext);
    }

    void JavascriptArray::ThrowErrorOnFailure(BOOL succeeded, ScriptContext* scriptContext, uint32 index)
    {TRACE_IT(56445);
        if (!succeeded)
        {TRACE_IT(56446);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_CantRedefineProp, JavascriptConversion::ToString(JavascriptNumber::ToVar(index, scriptContext), scriptContext)->GetSz());
        }
    }

    void JavascriptArray::ThrowErrorOnFailure(BOOL succeeded, ScriptContext* scriptContext, BigIndex index)
    {TRACE_IT(56447);
        if (!succeeded)
        {TRACE_IT(56448);
            uint64 i = (uint64)(index.IsSmallIndex() ? index.GetSmallIndex() : index.GetBigIndex());
            JavascriptError::ThrowTypeError(scriptContext, JSERR_CantRedefineProp, JavascriptConversion::ToString(JavascriptNumber::ToVar(i, scriptContext), scriptContext)->GetSz());
        }
    }

    BOOL JavascriptArray::SetArrayLikeObjects(RecyclableObject* pDestObj, uint32 idxDest, Var aItem)
    {TRACE_IT(56449);
        return pDestObj->SetItem(idxDest, aItem, Js::PropertyOperation_ThrowIfNotExtensible);
    }
    BOOL JavascriptArray::SetArrayLikeObjects(RecyclableObject* pDestObj, BigIndex idxDest, Var aItem)
    {TRACE_IT(56450);
        ScriptContext* scriptContext = pDestObj->GetScriptContext();

        if (idxDest.IsSmallIndex())
        {TRACE_IT(56451);
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
    {TRACE_IT(56452);
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
    {TRACE_IT(56453);
        JavascriptArray* pDestArray = nullptr;

        if (JavascriptArray::Is(pDestObj))
        {TRACE_IT(56454);
            pDestArray = JavascriptArray::FromVar(pDestObj);
        }

        T idxDest = startIdxDest;
        for (uint idxArg = start; idxArg < args.Info.Count; idxArg++)
        {TRACE_IT(56455);
            Var aItem = args[idxArg];
            bool spreadable = spreadableCheckedAndTrue;
            if (!spreadable && scriptContext->GetConfig()->IsES6IsConcatSpreadableEnabled())
            {TRACE_IT(56456);
                // firstPromotedItemIsSpreadable is ONLY used to resume after a type promotion from uint32 to uint64
                // we do this because calls to IsConcatSpreadable are observable (a big deal for proxies) and we don't
                // want to do the work a second time as soon as we record the length we clear the flag.
                spreadable = firstPromotedItemIsSpreadable || JavascriptOperators::IsConcatSpreadable(aItem);

                if (!spreadable)
                {TRACE_IT(56457);
                    JavascriptArray::SetConcatItem<T>(aItem, idxArg, pDestArray, pDestObj, idxDest, scriptContext);
                    ++idxDest;
                    continue;
                }
            }
            else
            {TRACE_IT(56458);
                spreadableCheckedAndTrue = false; // if it was `true`, reset after the first use
            }

            if (pDestArray && JavascriptArray::IsDirectAccessArray(aItem) && JavascriptArray::IsDirectAccessArray(pDestArray)
                && BigIndex(idxDest + JavascriptArray::FromVar(aItem)->length).IsSmallIndex()) // Fast path
            {TRACE_IT(56459);
                if (JavascriptNativeIntArray::Is(aItem))
                {TRACE_IT(56460);
                    JavascriptNativeIntArray *pItemArray = JavascriptNativeIntArray::FromVar(aItem);
                    CopyNativeIntArrayElementsToVar(pDestArray, idxDest, pItemArray);
                    idxDest = idxDest + pItemArray->length;
                }
                else if (JavascriptNativeFloatArray::Is(aItem))
                {TRACE_IT(56461);
                    JavascriptNativeFloatArray *pItemArray = JavascriptNativeFloatArray::FromVar(aItem);
                    CopyNativeFloatArrayElementsToVar(pDestArray, idxDest, pItemArray);
                    idxDest = idxDest + pItemArray->length;
                }
                else
                {TRACE_IT(56462);
                    JavascriptArray* pItemArray = JavascriptArray::FromVar(aItem);
                    CopyArrayElements(pDestArray, idxDest, pItemArray);
                    idxDest = idxDest + pItemArray->length;
                }
            }
            else
            {TRACE_IT(56463);
                // Flatten if other array or remote array (marked with TypeIds_Array)
                if (DynamicObject::IsAnyArray(aItem) || remoteTypeIds[idxArg] == TypeIds_Array || spreadable)
                {TRACE_IT(56464);
                    //CONSIDER: enumerating remote array instead of walking all indices
                    BigIndex length;
                    if (firstPromotedItemIsSpreadable)
                    {TRACE_IT(56465);
                        firstPromotedItemIsSpreadable = false;
                        length = firstPromotedItemLength;
                    }
                    else if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
                    {TRACE_IT(56466);
                        // we can cast to uin64 without fear of converting negative numbers to large positive ones
                        // from int64 because ToLength makes negative lengths 0
                        length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(aItem, scriptContext), scriptContext);
                    }
                    else
                    {TRACE_IT(56467);
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
                    {TRACE_IT(56468);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_IllegalArraySizeAndLength);
                    }

                    RecyclableObject* itemObject = RecyclableObject::FromVar(aItem);
                    Var subItem;
                    uint32 lengthToUin32Max = length.IsSmallIndex() ? length.GetSmallIndex() : MaxArrayLength;
                    for (uint32 idxSubItem = 0u; idxSubItem < lengthToUin32Max; ++idxSubItem)
                    {TRACE_IT(56469);
                        if (JavascriptOperators::HasItem(itemObject, idxSubItem))
                        {TRACE_IT(56470);
                            subItem = JavascriptOperators::GetItem(itemObject, idxSubItem, scriptContext);

                            if (pDestArray)
                            {TRACE_IT(56471);
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
                    {TRACE_IT(56472);
                        PropertyRecord const * propertyRecord;
                        JavascriptOperators::GetPropertyIdForInt(idxSubItem.GetBigIndex(), scriptContext, &propertyRecord);
                        if (JavascriptOperators::HasProperty(itemObject,propertyRecord->GetPropertyId()))
                        {TRACE_IT(56473);
                            subItem = JavascriptOperators::GetProperty(itemObject, propertyRecord->GetPropertyId(), scriptContext);
                            if (pDestArray)
                            {TRACE_IT(56474);
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
                {TRACE_IT(56475);
                    JavascriptArray::SetConcatItem<T>(aItem, idxArg, pDestArray, pDestObj, idxDest, scriptContext);
                    ++idxDest;
                }
            }
        }
        if (!pDestArray)
        {TRACE_IT(56476);
            pDestObj->SetProperty(PropertyIds::length, ConvertToIndex<T, Var>(idxDest, scriptContext), Js::PropertyOperation_None, nullptr);
        }
        else if (pDestArray->GetLength() != ConvertToIndex<T, uint32>(idxDest, scriptContext))
        {TRACE_IT(56477);
            pDestArray->SetLength(ConvertToIndex<T, uint32>(idxDest, scriptContext));
        }
    }

    bool JavascriptArray::PromoteToBigIndex(BigIndex lhs, BigIndex rhs)
    {TRACE_IT(56478);
        return false; // already a big index
    }

    bool JavascriptArray::PromoteToBigIndex(BigIndex lhs, uint32 rhs)
    {TRACE_IT(56479);
        ::Math::RecordOverflowPolicy destLengthOverflow;
        if (lhs.IsSmallIndex())
        {TRACE_IT(56480);
            UInt32Math::Add(lhs.GetSmallIndex(), rhs, destLengthOverflow);
            return destLengthOverflow.HasOverflowed();
        }
        return true;
    }

    JavascriptArray* JavascriptArray::ConcatIntArgs(JavascriptNativeIntArray* pDestArray, TypeId *remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(56481);
        uint idxDest = 0u;
        for (uint idxArg = 0; idxArg < args.Info.Count; idxArg++)
        {TRACE_IT(56482);
            Var aItem = args[idxArg];
            bool spreadableCheckedAndTrue = false;

            if (scriptContext->GetConfig()->IsES6IsConcatSpreadableEnabled())
            {TRACE_IT(56483);
                spreadableCheckedAndTrue = JavascriptOperators::IsConcatSpreadable(aItem) != FALSE;
                if (!JavascriptNativeIntArray::Is(pDestArray))
                {
                    ConcatArgs<uint>(pDestArray, remoteTypeIds, args, scriptContext, idxArg, idxDest, spreadableCheckedAndTrue);
                    return pDestArray;
                }

                if(!spreadableCheckedAndTrue)
                {TRACE_IT(56484);
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
            {TRACE_IT(56485);
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
            {TRACE_IT(56486);
                if (TaggedInt::Is(aItem))
                {TRACE_IT(56487);
                    pDestArray->DirectSetItemAt(idxDest, TaggedInt::ToInt32(aItem));
                }
                else
                {TRACE_IT(56488);
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
            {TRACE_IT(56489);
                JavascriptArray *pVarDestArray = JavascriptNativeIntArray::ConvertToVarArray(pDestArray);
                ConcatArgs<uint>(pVarDestArray, remoteTypeIds, args, scriptContext, idxArg, idxDest, spreadableCheckedAndTrue);
                return pVarDestArray;
            }
        }
        if (pDestArray->GetLength() != idxDest)
        {TRACE_IT(56490);
            pDestArray->SetLength(idxDest);
        }
        return pDestArray;
    }

    JavascriptArray* JavascriptArray::ConcatFloatArgs(JavascriptNativeFloatArray* pDestArray, TypeId *remoteTypeIds, Js::Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(56491);
        uint idxDest = 0u;
        for (uint idxArg = 0; idxArg < args.Info.Count; idxArg++)
        {TRACE_IT(56492);
            Var aItem = args[idxArg];

            bool spreadableCheckedAndTrue = false;

            if (scriptContext->GetConfig()->IsES6IsConcatSpreadableEnabled())
            {TRACE_IT(56493);
                spreadableCheckedAndTrue = JavascriptOperators::IsConcatSpreadable(aItem) != FALSE;
                if (!JavascriptNativeFloatArray::Is(pDestArray))
                {
                    ConcatArgs<uint>(pDestArray, remoteTypeIds, args, scriptContext, idxArg, idxDest, spreadableCheckedAndTrue);
                    return pDestArray;
                }

                if(!spreadableCheckedAndTrue)
                {TRACE_IT(56494);
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
            {TRACE_IT(56495);
                if (JavascriptNativeIntArray::Is(aItem)) // Fast path
                {TRACE_IT(56496);
                    JavascriptNativeIntArray *pIntArray = JavascriptNativeIntArray::FromVar(aItem);
                    converted = CopyNativeIntArrayElementsToFloat(pDestArray, idxDest, pIntArray);
                    idxDest = idxDest + pIntArray->length;
                }
                else if (JavascriptNativeFloatArray::Is(aItem))
                {TRACE_IT(56497);
                    JavascriptNativeFloatArray* pItemArray = JavascriptNativeFloatArray::FromVar(aItem);
                    converted = CopyNativeFloatArrayElements(pDestArray, idxDest, pItemArray);
                    idxDest = idxDest + pItemArray->length;
                }
                else
                {TRACE_IT(56498);
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
            {TRACE_IT(56499);
                if (TaggedInt::Is(aItem))
                {TRACE_IT(56500);
                    pDestArray->DirectSetItemAt(idxDest, (double)TaggedInt::ToInt32(aItem));
                }
                else
                {TRACE_IT(56501);
                    Assert(JavascriptNumber::Is(aItem));
                    pDestArray->DirectSetItemAt(idxDest, JavascriptNumber::GetValue(aItem));
                }
                ++idxDest;
            }
        }
        if (pDestArray->GetLength() != idxDest)
        {TRACE_IT(56502);
            pDestArray->SetLength(idxDest);
        }

        return pDestArray;
    }

    bool JavascriptArray::BoxConcatItem(Var aItem, uint idxArg, ScriptContext *scriptContext)
    {TRACE_IT(56503);
        return idxArg == 0 && !JavascriptOperators::IsObject(aItem);
    }

    Var JavascriptArray::EntryConcat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {TRACE_IT(56504);
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
        {TRACE_IT(56505);
            Var aItem = args[idxArg];
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(aItem);
#endif
            if (DynamicObject::IsAnyArray(aItem)) // Get JavascriptArray or ES5Array length
            {TRACE_IT(56506);
                JavascriptArray * pItemArray = JavascriptArray::FromAnyArray(aItem);
                if (isFloat)
                {TRACE_IT(56507);
                    if (!JavascriptNativeIntArray::Is(pItemArray))
                    {TRACE_IT(56508);
                        isInt = false;
                        if (!JavascriptNativeFloatArray::Is(pItemArray))
                        {TRACE_IT(56509);
                            isFloat = false;
                        }
                    }
                }
                cDestLength = UInt32Math::Add(cDestLength, pItemArray->GetLength(), destLengthOverflow);
            }
            else // Get remote array or object length
            {TRACE_IT(56510);
                // We already checked for types derived from JavascriptArray. These are types that should behave like array
                // i.e. proxy to array and remote array.
                if (JavascriptOperators::IsArray(aItem))
                {TRACE_IT(56511);
                    // Don't try to preserve nativeness of remote arrays. The extra complexity is probably not
                    // worth it.
                    isInt = false;
                    isFloat = false;
                    if (!JavascriptProxy::Is(aItem))
                    {TRACE_IT(56512);
                        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
                        {TRACE_IT(56513);
                            int64 len = JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(aItem, scriptContext), scriptContext);
                            // clipping to MaxArrayLength will overflow when added to cDestLength which we catch below
                            cDestLength = UInt32Math::Add(cDestLength, len < MaxArrayLength ? (uint32)len : MaxArrayLength, destLengthOverflow);
                        }
                        else
                        {TRACE_IT(56514);
                            uint len = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(aItem, scriptContext), scriptContext);
                            cDestLength = UInt32Math::Add(cDestLength, len, destLengthOverflow);
                        }
                    }
                    remoteTypeIds[idxArg] = TypeIds_Array; // Mark remote array, no matter remote JavascriptArray or ES5Array.
                }
                else
                {TRACE_IT(56515);
                    if (isFloat)
                    {
                        if (BoxConcatItem(aItem, idxArg, scriptContext))
                        {TRACE_IT(56516);
                            // A primitive will be boxed, so we have to create a var array for the result.
                            isInt = false;
                            isFloat = false;
                        }
                        else if (!TaggedInt::Is(aItem))
                        {TRACE_IT(56517);
                            if (!JavascriptNumber::Is(aItem))
                            {TRACE_IT(56518);
                                isInt = false;
                                isFloat = false;
                            }
                            else if (isInt)
                            {TRACE_IT(56519);
                                int32 int32Value;
                                if(!JavascriptNumber::TryGetInt32Value(JavascriptNumber::GetValue(aItem), &int32Value) ||
                                    SparseArraySegment<int32>::IsMissingItem(&int32Value))
                                {TRACE_IT(56520);
                                    isInt = false;
                                }
                            }
                        }
                        else if(isInt)
                        {TRACE_IT(56521);
                            int32 int32Value = TaggedInt::ToInt32(aItem);
                            if(SparseArraySegment<int32>::IsMissingItem(&int32Value))
                            {TRACE_IT(56522);
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
        {TRACE_IT(56523);
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
        {TRACE_IT(56524);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pDestObj);
#endif
            // Check the thing that species create made. If it's a native array that can't handle the source
            // data, convert it. If it's a more conservative kind of array than the source data, indicate that
            // so that the data will be converted on copy.
            if (isInt)
            {TRACE_IT(56525);
                if (JavascriptNativeIntArray::Is(pDestObj))
                {TRACE_IT(56526);
                    isArray = true;
                }
                else
                {TRACE_IT(56527);
                    isInt = false;
                    isFloat = JavascriptNativeFloatArray::Is(pDestObj);
                    isArray = JavascriptArray::Is(pDestObj);
                }
            }
            else if (isFloat)
            {TRACE_IT(56528);
                if (JavascriptNativeIntArray::Is(pDestObj))
                {TRACE_IT(56529);
                    JavascriptNativeIntArray::ToNativeFloatArray(JavascriptNativeIntArray::FromVar(pDestObj));
                    isArray = true;
                }
                else
                {TRACE_IT(56530);
                    isFloat = JavascriptNativeFloatArray::Is(pDestObj);
                    isArray = JavascriptArray::Is(pDestObj);
                }
            }
            else
            {TRACE_IT(56531);
                if (JavascriptNativeIntArray::Is(pDestObj))
                {TRACE_IT(56532);
                    JavascriptNativeIntArray::ToVarArray(JavascriptNativeIntArray::FromVar(pDestObj));
                    isArray = true;
                }
                else if (JavascriptNativeFloatArray::Is(pDestObj))
                {TRACE_IT(56533);
                    JavascriptNativeFloatArray::ToVarArray(JavascriptNativeFloatArray::FromVar(pDestObj));
                    isArray = true;
                }
                else
                {TRACE_IT(56534);
                    isArray = JavascriptArray::Is(pDestObj);
                }
            }
        }

        if (pDestObj == nullptr || isArray)
        {TRACE_IT(56535);
            if (isInt)
            {TRACE_IT(56536);
                JavascriptNativeIntArray *pIntArray = isArray ? JavascriptNativeIntArray::FromVar(pDestObj) : scriptContext->GetLibrary()->CreateNativeIntArray(cDestLength);
                pIntArray->EnsureHead<int32>();
                pDestArray = ConcatIntArgs(pIntArray, remoteTypeIds, args, scriptContext);
            }
            else if (isFloat)
            {TRACE_IT(56537);
                JavascriptNativeFloatArray *pFArray = isArray ? JavascriptNativeFloatArray::FromVar(pDestObj) : scriptContext->GetLibrary()->CreateNativeFloatArray(cDestLength);
                pFArray->EnsureHead<double>();
                pDestArray = ConcatFloatArgs(pFArray, remoteTypeIds, args, scriptContext);
            }
            else
            {TRACE_IT(56538);

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
    {TRACE_IT(56539);
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
        {TRACE_IT(56540);
            // bug# 725784: ES5: not calling ToObject in Step 1 of 15.4.4.4
            RecyclableObject* pObj = nullptr;
            if (FALSE == JavascriptConversion::ToObject(aItem, scriptContext, &pObj))
            {TRACE_IT(56541);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.concat"));
            }
            if (pDestArray)
            {TRACE_IT(56542);
                pDestArray->DirectSetItemAt(idxDest, pObj);
            }
            else
            {
                SetArrayLikeObjects(pDestObj, idxDest, pObj);
            }
        }
        else
        {TRACE_IT(56543);
            if (pDestArray)
            {TRACE_IT(56544);
                pDestArray->DirectSetItemAt(idxDest, aItem);
            }
            else
            {
                SetArrayLikeObjects(pDestObj, idxDest, aItem);
            }
        }
    }

    uint32 JavascriptArray::GetFromIndex(Var arg, uint32 length, ScriptContext *scriptContext)
    {TRACE_IT(56545);
        uint32 fromIndex;

        if (TaggedInt::Is(arg))
        {TRACE_IT(56546);
            int intValue = TaggedInt::ToInt32(arg);

            if (intValue >= 0)
            {TRACE_IT(56547);
                fromIndex = intValue;
            }
            else
            {TRACE_IT(56548);
                // (intValue + length) may exceed 2^31 or may be < 0, so promote to int64
                fromIndex = (uint32)max(0i64, (int64)(length) + intValue);
            }
        }
        else
        {TRACE_IT(56549);
            double value = JavascriptConversion::ToInteger(arg, scriptContext);
            if (value > length)
            {TRACE_IT(56550);
                return (uint32)-1;
            }
            else if (value >= 0)
            {TRACE_IT(56551);
                fromIndex = (uint32)value;
            }
            else
            {TRACE_IT(56552);
                fromIndex = (uint32)max((double)0, value + length);
            }
        }

        return fromIndex;
    }

    uint64 JavascriptArray::GetFromIndex(Var arg, uint64 length, ScriptContext *scriptContext)
    {TRACE_IT(56553);
        uint64 fromIndex;

        if (TaggedInt::Is(arg))
        {TRACE_IT(56554);
            int64 intValue = TaggedInt::ToInt64(arg);

            if (intValue >= 0)
            {TRACE_IT(56555);
                fromIndex = intValue;
            }
            else
            {TRACE_IT(56556);
                fromIndex = max((int64)0, (int64)(intValue + length));
            }
        }
        else
        {TRACE_IT(56557);
            double value = JavascriptConversion::ToInteger(arg, scriptContext);
            if (value > length)
            {TRACE_IT(56558);
                return (uint64)-1;
            }
            else if (value >= 0)
            {TRACE_IT(56559);
                fromIndex = (uint64)value;
            }
            else
            {TRACE_IT(56560);
                fromIndex = (uint64)max((double)0, value + length);
            }
        }

        return fromIndex;
    }

    int64 JavascriptArray::GetFromLastIndex(Var arg, int64 length, ScriptContext *scriptContext)
    {TRACE_IT(56561);
        int64 fromIndex;

        if (TaggedInt::Is(arg))
        {TRACE_IT(56562);
            int intValue = TaggedInt::ToInt32(arg);

            if (intValue >= 0)
            {TRACE_IT(56563);
                fromIndex = min<int64>(intValue, length - 1);
            }
            else if ((uint32)-intValue > length)
            {TRACE_IT(56564);
                return length;
            }
            else
            {TRACE_IT(56565);
                fromIndex = intValue + length;
            }
        }
        else
        {TRACE_IT(56566);
            double value = JavascriptConversion::ToInteger(arg, scriptContext);

            if (value >= 0)
            {TRACE_IT(56567);
                fromIndex = (int64)min(value, (double)(length - 1));
            }
            else if (value + length < 0)
            {TRACE_IT(56568);
                return length;
            }
            else
            {TRACE_IT(56569);
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
    {TRACE_IT(56570);
        RecyclableObject* obj = nullptr;
        JavascriptArray* pArr = nullptr;
        BigIndex length;
        Var trueValue = scriptContext->GetLibrary()->GetTrue();
        Var falseValue = scriptContext->GetLibrary()->GetFalse();

        if (JavascriptArray::Is(args[0]))
        {TRACE_IT(56571);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {TRACE_IT(56572);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(56573);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.indexOf"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(56574);
            length = (uint64)JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }
        else
        {TRACE_IT(56575);
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        Var search;
        uint32 fromIndex = 0;
        uint64 fromIndex64 = 0;

        // The evaluation of method arguments may change the type of the array. Hence, we do that prior to the actual helper method calls.
        // The if clause of the conditional statement below applies to an JavascriptArray or TypedArray instances. The rest of the conditional
        // clauses apply to an ES5Array or other valid Javascript objects.
        if ((pArr || TypedArrayBase::Is(obj)) && (length.IsSmallIndex() || length.IsUint32Max()))
        {TRACE_IT(56576);
            uint32 len = length.IsUint32Max() ? MaxArrayLength : length.GetSmallIndex();
            if (!GetParamForIndexOf(len, args, search, fromIndex, scriptContext))
            {TRACE_IT(56577);
                return includesAlgorithm ? falseValue : TaggedInt::ToVarUnchecked(-1);
            }
        }
        else if (length.IsSmallIndex())
        {TRACE_IT(56578);
            if (!GetParamForIndexOf(length.GetSmallIndex(), args, search, fromIndex, scriptContext))
            {TRACE_IT(56579);
                return includesAlgorithm ? falseValue : TaggedInt::ToVarUnchecked(-1);
            }
        }
        else
        {TRACE_IT(56580);
            if (!GetParamForIndexOf(length.GetBigIndex(), args, search, fromIndex64, scriptContext))
            {TRACE_IT(56581);
                return includesAlgorithm ? falseValue : TaggedInt::ToVarUnchecked(-1);
            }
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of fromIndex argument may convert the array to an ES5 array.
        if (pArr && !JavascriptArray::Is(obj))
        {TRACE_IT(56582);
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (pArr)
        {TRACE_IT(56583);
            if (length.IsSmallIndex() || length.IsUint32Max())
            {TRACE_IT(56584);
                uint32 len = length.IsUint32Max() ? MaxArrayLength : length.GetSmallIndex();
                int32 index = pArr->HeadSegmentIndexOfHelper(search, fromIndex, len, includesAlgorithm, scriptContext);

                // If we found the search value in the head segment, or if we determined there is no need to search other segments,
                // we stop right here.
                if (index != -1 || fromIndex == -1)
                {TRACE_IT(56585);
                    if (includesAlgorithm)
                    {TRACE_IT(56586);
                        //Array.prototype.includes
                        return (index == -1) ? falseValue : trueValue;
                    }
                    else
                    {TRACE_IT(56587);
                        //Array.prototype.indexOf
                        return JavascriptNumber::ToVar(index, scriptContext);
                    }
                }

                //  If we really must search other segments, let's do it now. We'll have to search the slow way (dealing with holes, etc.).
                switch (pArr->GetTypeId())
                {
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
        {TRACE_IT(56588);
            if (length.IsSmallIndex() || length.IsUint32Max())
            {TRACE_IT(56589);
                return TemplatedIndexOfHelper<includesAlgorithm>(TypedArrayBase::FromVar(obj), search, fromIndex, length.GetSmallIndex(), scriptContext);
            }
        }
        if (length.IsSmallIndex())
        {TRACE_IT(56590);
            return TemplatedIndexOfHelper<includesAlgorithm>(obj, search, fromIndex, length.GetSmallIndex(), scriptContext);
        }
        else
        {TRACE_IT(56591);
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
    {TRACE_IT(56592);
        if (length == 0)
        {TRACE_IT(56593);
            return false;
        }

        if (args.Info.Count > 2)
        {TRACE_IT(56594);
            fromIndex = GetFromIndex(args[2], length, scriptContext);
            if (fromIndex >= length)
            {TRACE_IT(56595);
                return false;
            }
            search = args[1];
        }
        else
        {TRACE_IT(56596);
            fromIndex = 0;
            search = args.Info.Count > 1 ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        }
        return true;
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(RecyclableObject * obj, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56597);
        // Note: Sometime cross site array go down this path to get the marshalling
        Assert(!VirtualTableInfo<JavascriptArray>::HasVirtualTable(obj)
            && !VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(obj)
            && !VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(obj));
        if (checkHasItem && !JavascriptOperators::HasItem(obj, index))
        {TRACE_IT(56598);
            return FALSE;
        }
        return JavascriptOperators::GetItem(obj, index, element, scriptContext);
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(RecyclableObject * obj, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56599);
        // Note: Sometime cross site array go down this path to get the marshalling
        Assert(!VirtualTableInfo<JavascriptArray>::HasVirtualTable(obj)
            && !VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(obj)
            && !VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(obj));
        PropertyRecord const * propertyRecord;
        JavascriptOperators::GetPropertyIdForInt(index, scriptContext, &propertyRecord);
        if (checkHasItem && !JavascriptOperators::HasProperty(obj, propertyRecord->GetPropertyId()))
        {TRACE_IT(56600);
            return FALSE;
        }
        *element = JavascriptOperators::GetProperty(obj, propertyRecord->GetPropertyId(), scriptContext);
        return *element != scriptContext->GetLibrary()->GetUndefined();

    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptArray *pArr, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56601);
        Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(pArr)
            || VirtualTableInfo<CrossSiteObject<JavascriptArray>>::HasVirtualTable(pArr));
        return pArr->JavascriptArray::DirectGetItemAtFull(index, element);
    }
    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptArray *pArr, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56602);
        // This should never get called.
        Assert(false);
        Throw::InternalError();
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptNativeIntArray *pArr, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56603);
        Assert(VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(pArr)
            || VirtualTableInfo<CrossSiteObject<JavascriptNativeIntArray>>::HasVirtualTable(pArr));
        return pArr->JavascriptNativeIntArray::DirectGetItemAtFull(index, element);
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptNativeIntArray *pArr, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56604);
        // This should never get called.
        Assert(false);
        Throw::InternalError();
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptNativeFloatArray *pArr, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56605);
        Assert(VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(pArr)
            || VirtualTableInfo<CrossSiteObject<JavascriptNativeFloatArray>>::HasVirtualTable(pArr));
        return pArr->JavascriptNativeFloatArray::DirectGetItemAtFull(index, element);
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(JavascriptNativeFloatArray *pArr, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56606);
        // This should never get called.
        Assert(false);
        Throw::InternalError();
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(TypedArrayBase * typedArrayBase, uint32 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56607);
        // We need to do explicit check for items since length value may not actually match the actual TypedArray length.
        // User could add a length property to a TypedArray instance which lies and returns a different value from the underlying length.
        // Since this method can be called via Array.prototype.indexOf with .apply or .call passing a TypedArray as this parameter
        // we don't know whether or not length == typedArrayBase->GetLength().
        if (checkHasItem && !typedArrayBase->HasItem(index))
        {TRACE_IT(56608);
            return false;
        }

        *element = typedArrayBase->DirectGetItem(index);
        return true;
    }

    template <>
    BOOL JavascriptArray::TemplatedGetItem(TypedArrayBase * typedArrayBase, uint64 index, Var * element, ScriptContext * scriptContext, bool checkHasItem)
    {TRACE_IT(56609);
        // This should never get called.
        Assert(false);
        Throw::InternalError();
    }

    template <bool includesAlgorithm, typename T, typename P>
    Var JavascriptArray::TemplatedIndexOfHelper(T * pArr, Var search, P fromIndex, P toIndex, ScriptContext * scriptContext)
    {TRACE_IT(56610);
        Var element = nullptr;
        bool isSearchTaggedInt = TaggedInt::Is(search);
        bool doUndefinedSearch = includesAlgorithm && JavascriptOperators::GetTypeId(search) == TypeIds_Undefined;

        Var trueValue = scriptContext->GetLibrary()->GetTrue();
        Var falseValue = scriptContext->GetLibrary()->GetFalse();

        //Consider: enumerating instead of walking all indices
        for (P i = fromIndex; i < toIndex; i++)
        {
            if (!TryTemplatedGetItem(pArr, i, &element, scriptContext, !includesAlgorithm))
            {TRACE_IT(56611);
                if (doUndefinedSearch)
                {TRACE_IT(56612);
                    return trueValue;
                }
                continue;
            }

            if (isSearchTaggedInt && TaggedInt::Is(element))
            {TRACE_IT(56613);
                if (element == search)
                {TRACE_IT(56614);
                    return includesAlgorithm? trueValue : JavascriptNumber::ToVar(i, scriptContext);
                }
                continue;
            }

            if (includesAlgorithm)
            {TRACE_IT(56615);
                //Array.prototype.includes
                if (JavascriptConversion::SameValueZero(element, search))
                {TRACE_IT(56616);
                    return trueValue;
                }
            }
            else
            {TRACE_IT(56617);
                //Array.prototype.indexOf
                if (JavascriptOperators::StrictEqual(element, search, scriptContext))
                {TRACE_IT(56618);
                    return JavascriptNumber::ToVar(i, scriptContext);
                }
            }
        }

        return includesAlgorithm ? falseValue :  TaggedInt::ToVarUnchecked(-1);
    }

    int32 JavascriptArray::HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm, ScriptContext * scriptContext)
    {TRACE_IT(56619);
        Assert(Is(GetTypeId()) && !JavascriptNativeArray::Is(GetTypeId()));

        if (!HasNoMissingValues() || fromIndex >= GetHead()->length)
        {TRACE_IT(56620);
            return -1;
        }

        bool isSearchTaggedInt = TaggedInt::Is(search);
        // We need to cast head segment to SparseArraySegment<Var> to have access to GetElement (onSparseArraySegment<T>). Because there are separate overloads of this
        // virtual method on JavascriptNativeIntArray and JavascriptNativeFloatArray, we know this version of this method will only be called for true JavascriptArray, and not for
        // either of the derived native arrays, so the elements of each segment used here must be Vars. Hence, the cast is safe.
        SparseArraySegment<Var>* head = static_cast<SparseArraySegment<Var>*>(GetHead());
        uint32 toIndexTrimmed = toIndex <= head->length ? toIndex : head->length;
        for (uint32 i = fromIndex; i < toIndexTrimmed; i++)
        {TRACE_IT(56621);
            Var element = head->GetElement(i);
            if (isSearchTaggedInt && TaggedInt::Is(element))
            {TRACE_IT(56622);
                if (search == element)
                {TRACE_IT(56623);
                    return i;
                }
            }
            else if (includesAlgorithm && JavascriptConversion::SameValueZero(element, search))
            {TRACE_IT(56624);
                //Array.prototype.includes
                return i;
            }
            else if (JavascriptOperators::StrictEqual(element, search, scriptContext))
            {TRACE_IT(56625);
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
    {TRACE_IT(56626);
        byte* bValue = (byte*)&value;
        byte firstByte = *bValue++;
        for (int i = 1; i < sizeof(T); ++i)
        {TRACE_IT(56627);
            if (*bValue++ != firstByte)
            {TRACE_IT(56628);
                return false;
            }
        }
        return true;
    }

    template<>
    void JavascriptArray::CopyValueToSegmentBuferNoCheck(Field(double)* buffer, uint32 length, double value)
    {TRACE_IT(56629);
        if (JavascriptNumber::IsZero(value) && !JavascriptNumber::IsNegZero(value))
        {
            memset(buffer, 0, sizeof(double) * length);
        }
        else
        {TRACE_IT(56630);
            for (uint32 i = 0; i < length; i++)
            {TRACE_IT(56631);
                buffer[i] = value;
            }
        }
    }

    template<>
    void JavascriptArray::CopyValueToSegmentBuferNoCheck(Field(int32)* buffer, uint32 length, int32 value)
    {TRACE_IT(56632);
        if (value == 0 || AreAllBytesEqual(value))
        {
            memset(buffer, *(byte*)&value, sizeof(int32)* length);
        }
        else
        {TRACE_IT(56633);
            for (uint32 i = 0; i < length; i++)
            {TRACE_IT(56634);
                buffer[i] = value;
            }
        }
    }

    template<>
    void JavascriptArray::CopyValueToSegmentBuferNoCheck(Field(Js::Var)* buffer, uint32 length, Js::Var value)
    {TRACE_IT(56635);
        for (uint32 i = 0; i < length; i++)
        {TRACE_IT(56636);
            buffer[i] = value;
        }
    }

    int32 JavascriptNativeIntArray::HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm,  ScriptContext * scriptContext)
    {TRACE_IT(56637);
        // We proceed largely in the same manner as in JavascriptArray's version of this method (see comments there for more information),
        // except when we can further optimize thanks to the knowledge that all elements in the array are int32's. This allows for two additional optimizations:
        // 1. Only tagged ints or JavascriptNumbers that can be represented as int32 can be strict equal to some element in the array (all int32). Thus, if
        // the search value is some other kind of Var, we can return -1 without ever iterating over the elements.
        // 2. If the search value is a number that can be represented as int32, then we inspect the elements, but we don't need to perform the full strict equality algorithm.
        // Instead we can use simple C++ equality (which in case of such values is equivalent to strict equality in JavaScript).

        if (!HasNoMissingValues() || fromIndex >= GetHead()->length)
        {TRACE_IT(56638);
            return -1;
        }

        bool isSearchTaggedInt = TaggedInt::Is(search);
        if (!isSearchTaggedInt && !JavascriptNumber::Is_NoTaggedIntCheck(search))
        {TRACE_IT(56639);
            // The value can't be in the array, but it could be in a prototype, and we can only guarantee that
            // the head segment has no gaps.
            fromIndex = toIndex > GetHead()->length ? GetHead()->length : -1;
            return -1;
        }
        int32 searchAsInt32;
        if (isSearchTaggedInt)
        {TRACE_IT(56640);
            searchAsInt32 = TaggedInt::ToInt32(search);
        }
        else if (!JavascriptNumber::TryGetInt32Value<true>(JavascriptNumber::GetValue(search), &searchAsInt32))
        {TRACE_IT(56641);
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
        {TRACE_IT(56642);
            int32 element = head->GetElement(i);
            if (searchAsInt32 == element)
            {TRACE_IT(56643);
                return i;
            }
        }

        // Element not found in the head segment. Keep looking only if the range of indices extends past
        // the head segment.
        fromIndex = toIndex > GetHead()->length ? GetHead()->length : -1;
        return -1;
    }

    int32 JavascriptNativeFloatArray::HeadSegmentIndexOfHelper(Var search, uint32 &fromIndex, uint32 toIndex, bool includesAlgorithm, ScriptContext * scriptContext)
    {TRACE_IT(56644);
        // We proceed largely in the same manner as in JavascriptArray's version of this method (see comments there for more information),
        // except when we can further optimize thanks to the knowledge that all elements in the array are doubles. This allows for two additional optimizations:
        // 1. Only tagged ints or JavascriptNumbers can be strict equal to some element in the array (all doubles). Thus, if
        // the search value is some other kind of Var, we can return -1 without ever iterating over the elements.
        // 2. If the search value is a number, then we inspect the elements, but we don't need to perform the full strict equality algorithm.
        // Instead we can use simple C++ equality (which in case of such values is equivalent to strict equality in JavaScript).

        if (!HasNoMissingValues() || fromIndex >= GetHead()->length)
        {TRACE_IT(56645);
            return -1;
        }

        bool isSearchTaggedInt = TaggedInt::Is(search);
        if (!isSearchTaggedInt && !JavascriptNumber::Is_NoTaggedIntCheck(search))
        {TRACE_IT(56646);
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
        {TRACE_IT(56647);
            double element = head->GetElement(i);

            if (element == searchAsDouble)
            {TRACE_IT(56648);
                return i;
            }

            //NaN != NaN we expect to match for NaN in Array.prototype.includes algorithm
            if (matchNaN && JavascriptNumber::IsNan(element))
            {TRACE_IT(56649);
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
        {TRACE_IT(56650);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.join"));
        }

        JavascriptString* separator;
        if (args.Info.Count >= 2)
        {TRACE_IT(56651);
            TypeId typeId = JavascriptOperators::GetTypeId(args[1]);
            //ES5 15.4.4.5 If separator is undefined, let separator be the single-character String ",".
            if (TypeIds_Undefined != typeId)
            {TRACE_IT(56652);
                separator = JavascriptConversion::ToString(args[1], scriptContext);
            }
            else
            {TRACE_IT(56653);
                separator = scriptContext->GetLibrary()->GetCommaDisplayString();
            }
        }
        else
        {TRACE_IT(56654);
            separator = scriptContext->GetLibrary()->GetCommaDisplayString();
        }

        return JoinHelper(args[0], separator, scriptContext);
    }

    JavascriptString* JavascriptArray::JoinToString(Var value, ScriptContext* scriptContext)
    {TRACE_IT(56655);
        TypeId typeId = JavascriptOperators::GetTypeId(value);
        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {TRACE_IT(56656);
            return scriptContext->GetLibrary()->GetEmptyString();
        }
        else
        {TRACE_IT(56657);
            return JavascriptConversion::ToString(value, scriptContext);
        }
    }

    JavascriptString* JavascriptArray::JoinHelper(Var thisArg, JavascriptString* separator, ScriptContext* scriptContext)
    {TRACE_IT(56658);
        bool isArray = JavascriptArray::Is(thisArg) && (scriptContext == JavascriptArray::FromVar(thisArg)->GetScriptContext());
        bool isProxy = JavascriptProxy::Is(thisArg) && (scriptContext == JavascriptProxy::FromVar(thisArg)->GetScriptContext());
        Var target = NULL;
        bool isTargetObjectPushed = false;
        // if we are visiting a proxy object, track that we have visited the target object as well so the next time w
        // call the join helper for the target of this proxy, we will return above.
        if (isProxy)
        {TRACE_IT(56659);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(thisArg);
            Assert(proxy);
            target = proxy->GetTarget();
            if (target != nullptr)
            {TRACE_IT(56660);
                // If we end up joining same array, instead of going in infinite loop, return the empty string
                if (scriptContext->CheckObject(target))
                {TRACE_IT(56661);
                    return scriptContext->GetLibrary()->GetEmptyString();
                }
                else
                {TRACE_IT(56662);
                    scriptContext->PushObject(target);
                    isTargetObjectPushed = true;
                }
            }
        }
        // If we end up joining same array, instead of going in infinite loop, return the empty string
        else if (scriptContext->CheckObject(thisArg))
        {TRACE_IT(56663);
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        if (!isTargetObjectPushed)
        {TRACE_IT(56664);
            scriptContext->PushObject(thisArg);
        }

        JavascriptString* res = nullptr;

        TryFinally([&]()
        {
            if (isArray)
            {TRACE_IT(56665);
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray(thisArg);
#endif
                JavascriptArray * arr = JavascriptArray::FromVar(thisArg);
                switch (arr->GetTypeId())
                {
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
            {TRACE_IT(56666);
                res = JoinOtherHelper(RecyclableObject::FromVar(thisArg), separator, scriptContext);
            }
            else
            {TRACE_IT(56667);
                res = JoinOtherHelper(scriptContext->GetLibrary()->CreateNumberObject(thisArg), separator, scriptContext);
            }
        },
        [&](bool/*hasException*/)
        {TRACE_IT(56668);
            Var top = scriptContext->PopObject();
            if (isProxy)
            {TRACE_IT(56669);
                AssertMsg(top == target, "Unmatched operation stack");
            }
            else
            {TRACE_IT(56670);
                AssertMsg(top == thisArg, "Unmatched operation stack");
            }
        });

        if (res == nullptr)
        {TRACE_IT(56671);
            res = scriptContext->GetLibrary()->GetEmptyString();
        }

        return res;
    }

    static const charcount_t Join_MaxEstimatedAppendCount = static_cast<charcount_t>((64 << 20) / sizeof(void *)); // 64 MB worth of pointers

    template <typename T>
    JavascriptString* JavascriptArray::JoinArrayHelper(T * arr, JavascriptString* separator, ScriptContext* scriptContext)
    {TRACE_IT(56672);
        Assert(VirtualTableInfo<T>::HasVirtualTable(arr) || VirtualTableInfo<CrossSiteObject<T>>::HasVirtualTable(arr));
        const uint32 arrLength = arr->length;
        switch(arrLength)
        {
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
                {TRACE_IT(56673);
                    cs->Append(JavascriptArray::JoinToString(item, scriptContext));
                }
                for (uint32 i = 1; i < arrLength; i++)
                {TRACE_IT(56674);
                    if (hasSeparator)
                    {TRACE_IT(56675);
                        cs->Append(separator);
                    }

                    if (TryTemplatedGetItem(arr, i, &item, scriptContext))
                    {TRACE_IT(56676);
                        cs->Append(JavascriptArray::JoinToString(item, scriptContext));
                    }
                }
                return cs;
            }

            case 2:
            {TRACE_IT(56677);
                bool hasSeparator = (separator->GetLength() != 0);
                if(hasSeparator)
                {TRACE_IT(56678);
                    goto CaseDefault;
                }

                JavascriptString *res = nullptr;
                Var item;

                if (TemplatedGetItem(arr, 0u, &item, scriptContext))
                {TRACE_IT(56679);
                    res = JavascriptArray::JoinToString(item, scriptContext);
                }

                if (TryTemplatedGetItem(arr, 1u, &item, scriptContext))
                {TRACE_IT(56680);
                    JavascriptString *const itemString = JavascriptArray::JoinToString(item, scriptContext);
                    return res ? ConcatString::New(res, itemString) : itemString;
                }

                if(res)
                {TRACE_IT(56681);
                    return res;
                }

                goto Case0;
            }

            case 1:
            {TRACE_IT(56682);
                Var item;
                if (TemplatedGetItem(arr, 0u, &item, scriptContext))
                {TRACE_IT(56683);
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
    {TRACE_IT(56684);
        // In ES6-mode, we always load the length property from the object instead of using the internal slot.
        // Even for arrays, this is now observable via proxies.
        // If source object is not an array, we fall back to this behavior anyway.
        Var lenValue = JavascriptOperators::OP_GetLength(object, scriptContext);
        int64 cSrcLength = JavascriptConversion::ToLength(lenValue, scriptContext);

        switch (cSrcLength)
        {
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
                {TRACE_IT(56685);
                    cs->Append(JavascriptArray::JoinToString(value, scriptContext));
                }
                for (uint32 i = 1; i < cSrcLength; i++)
                {TRACE_IT(56686);
                    if (hasSeparator)
                    {TRACE_IT(56687);
                        cs->Append(separator);
                    }
                    if (JavascriptOperators::GetItem(object, i, &value, scriptContext))
                    {TRACE_IT(56688);
                        cs->Append(JavascriptArray::JoinToString(value, scriptContext));
                    }
                }
                return cs;
            }

            case 2:
            {TRACE_IT(56689);
                bool hasSeparator = (separator->GetLength() != 0);
                if(hasSeparator)
                {TRACE_IT(56690);
                    goto CaseDefault;
                }

                JavascriptString *res = nullptr;
                Var value;
                if (JavascriptOperators::GetItem(object, 0u, &value, scriptContext))
                {TRACE_IT(56691);
                    res = JavascriptArray::JoinToString(value, scriptContext);
                }
                if (JavascriptOperators::GetItem(object, 1u, &value, scriptContext))
                {TRACE_IT(56692);
                    JavascriptString *const valueString = JavascriptArray::JoinToString(value, scriptContext);
                    return res ? ConcatString::New(res, valueString) : valueString;
                }
                if(res)
                {TRACE_IT(56693);
                    return res;
                }
                goto Case0;
            }

            case 1:
            {TRACE_IT(56694);
                Var value;
                if (JavascriptOperators::GetItem(object, 0u, &value, scriptContext))
                {TRACE_IT(56695);
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
        {TRACE_IT(56696);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
            length = pArr->length;
        }
        else
        {TRACE_IT(56697);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(56698);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.lastIndexOf"));
            }
            Var lenValue = JavascriptOperators::OP_GetLength(obj, scriptContext);
            length = JavascriptConversion::ToLength(lenValue, scriptContext);
        }

        Var search;
        int64 fromIndex;
        if (!GetParamForLastIndexOf(length, args, search, fromIndex, scriptContext))
        {TRACE_IT(56699);
            return TaggedInt::ToVarUnchecked(-1);
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of fromIndex argument may convert the array to an ES5 array.
        if (pArr && !JavascriptArray::Is(obj))
        {TRACE_IT(56700);
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (pArr)
        {TRACE_IT(56701);
            switch (pArr->GetTypeId())
            {
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
        {TRACE_IT(56702);
            return LastIndexOfHelper(TypedArrayBase::FromVar(obj), search, fromIndex, scriptContext);
        }

        return LastIndexOfHelper(obj, search, fromIndex, scriptContext);
    }

    // Array.prototype.lastIndexOf as described in ES6.0 (draft 22) Section 22.1.3.14
    BOOL JavascriptArray::GetParamForLastIndexOf(int64 length, Arguments const & args, Var& search, int64& fromIndex, ScriptContext * scriptContext)
    {TRACE_IT(56703);
        if (length == 0)
        {TRACE_IT(56704);
            return false;
        }

        if (args.Info.Count > 2)
        {TRACE_IT(56705);
            fromIndex = GetFromLastIndex(args[2], length, scriptContext);

            if (fromIndex >= length)
            {TRACE_IT(56706);
                return false;
            }
            search = args[1];
        }
        else
        {TRACE_IT(56707);
            search = args.Info.Count > 1 ? args[1] : scriptContext->GetLibrary()->GetUndefined();
            fromIndex = length - 1;
        }
        return true;
    }

    template <typename T>
    Var JavascriptArray::LastIndexOfHelper(T* pArr, Var search, int64 fromIndex, ScriptContext * scriptContext)
    {TRACE_IT(56708);
        Var element = nullptr;
        bool isSearchTaggedInt = TaggedInt::Is(search);

        // First handle the indices > 2^32
        while (fromIndex >= MaxArrayLength)
        {TRACE_IT(56709);
            Var index = JavascriptNumber::ToVar(fromIndex, scriptContext);

            if (JavascriptOperators::OP_HasItem(pArr, index, scriptContext))
            {TRACE_IT(56710);
                element = JavascriptOperators::OP_GetElementI(pArr, index, scriptContext);

                if (isSearchTaggedInt && TaggedInt::Is(element))
                {TRACE_IT(56711);
                    if (element == search)
                    {TRACE_IT(56712);
                        return index;
                    }
                    fromIndex--;
                    continue;
                }

                if (JavascriptOperators::StrictEqual(element, search, scriptContext))
                {TRACE_IT(56713);
                    return index;
                }
            }

            fromIndex--;
        }

        Assert(fromIndex < MaxArrayLength);

        // fromIndex now has to be < MaxArrayLength so casting to uint32 is safe
        uint32 end = static_cast<uint32>(fromIndex);

        for (uint32 i = 0; i <= end; i++)
        {TRACE_IT(56714);
            uint32 index = end - i;

            if (!TryTemplatedGetItem(pArr, index, &element, scriptContext))
            {TRACE_IT(56715);
                continue;
            }

            if (isSearchTaggedInt && TaggedInt::Is(element))
            {TRACE_IT(56716);
                if (element == search)
                {TRACE_IT(56717);
                    return JavascriptNumber::ToVar(index, scriptContext);
                }
                continue;
            }

            if (JavascriptOperators::StrictEqual(element, search, scriptContext))
            {TRACE_IT(56718);
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
    {TRACE_IT(56719);
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
    {TRACE_IT(56720);
        Assert(JavascriptNativeIntArray::Is(object));
        JavascriptNativeIntArray * arr = JavascriptNativeIntArray::FromVar(object);

        Assert(arr->GetLength() != 0);

        uint32 index = arr->length - 1;

        int32 element = Js::JavascriptOperators::OP_GetNativeIntElementI_UInt32(object, index, scriptContext);

        //If it is a missing item, then don't update the length - Pre-op Bail out will happen.
        if(!SparseArraySegment<int32>::IsMissingItem(&element))
        {TRACE_IT(56721);
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
    {TRACE_IT(56722);
        Assert(JavascriptNativeFloatArray::Is(object));
        JavascriptNativeFloatArray * arr = JavascriptNativeFloatArray::FromVar(object);

        Assert(arr->GetLength() != 0);

        uint32 index = arr->length - 1;

        double element = Js::JavascriptOperators::OP_GetNativeFloatElementI_UInt32(object, index, scriptContext);

        // If it is a missing item then don't update the length - Pre-op Bail out will happen.
        if(!SparseArraySegment<double>::IsMissingItem(&element))
        {TRACE_IT(56723);
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
    {TRACE_IT(56724);
        if (JavascriptArray::Is(object))
        {TRACE_IT(56725);
            return EntryPopJavascriptArray(scriptContext, object);
        }
        else
        {TRACE_IT(56726);
            return EntryPopNonJavascriptArray(scriptContext, object);
        }
    }

    Var JavascriptArray::EntryPopJavascriptArray(ScriptContext * scriptContext, Var object)
    {TRACE_IT(56727);
        JavascriptArray * arr = JavascriptArray::FromVar(object);
        uint32 length = arr->length;

        if (length == 0)
        {TRACE_IT(56728);
            // If length is 0, return 'undefined'
            return scriptContext->GetLibrary()->GetUndefined();
        }

        uint32 index = length - 1;
        Var element;

        if (!arr->DirectGetItemAtFull(index, &element))
        {TRACE_IT(56729);
            element = scriptContext->GetLibrary()->GetUndefined();
        }
        else
        {TRACE_IT(56730);
            element = CrossSite::MarshalVar(scriptContext, element);
        }
        arr->SetLength(index); // SetLength will clear element at index

#ifdef VALIDATE_ARRAY
        arr->ValidateArray();
#endif
        return element;
    }

    Var JavascriptArray::EntryPopNonJavascriptArray(ScriptContext * scriptContext, Var object)
    {TRACE_IT(56731);
        RecyclableObject* dynamicObject = nullptr;
        if (FALSE == JavascriptConversion::ToObject(object, scriptContext, &dynamicObject))
        {TRACE_IT(56732);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.pop"));
        }
        BigIndex length;
        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(56733);
            length = (uint64)JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }
        else
        {TRACE_IT(56734);
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }


        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.pop"));
        if (length == 0u)
        {TRACE_IT(56735);
            // Set length = 0
            h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, TaggedInt::ToVarUnchecked(0), scriptContext, PropertyOperation_ThrowIfNotExtensible));
            return scriptContext->GetLibrary()->GetUndefined();
        }
        BigIndex index = length;
        --index;
        Var element;
        if (index.IsSmallIndex())
        {TRACE_IT(56736);
            if (!JavascriptOperators::GetItem(dynamicObject, index.GetSmallIndex(), &element, scriptContext))
            {TRACE_IT(56737);
                element = scriptContext->GetLibrary()->GetUndefined();
            }
            h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, index.GetSmallIndex(), PropertyOperation_ThrowOnDeleteIfNotConfig));

            // Set the new length
            h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, JavascriptNumber::ToVar(index.GetSmallIndex(), scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
        }
        else
        {TRACE_IT(56738);
            if (!JavascriptOperators::GetItem(dynamicObject, index.GetBigIndex(), &element, scriptContext))
            {TRACE_IT(56739);
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
        {TRACE_IT(56740);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.pop"));
        }

        if (JavascriptArray::Is(args[0]))
        {TRACE_IT(56741);
            return EntryPopJavascriptArray(scriptContext, args.Values[0]);
        }
        else
        {TRACE_IT(56742);
            return EntryPopNonJavascriptArray(scriptContext, args.Values[0]);
        }
    }

    /*
    *   JavascriptNativeIntArray::Push
    *   Pushes Int element in a native Int Array.
    *   We call the generic Push, if the array is not native Int or we have a really big array.
    */
    Var JavascriptNativeIntArray::Push(ScriptContext * scriptContext, Var array, int value)
    {TRACE_IT(56743);
        // Handle non crossSite native int arrays here length within MaxArrayLength.
        // JavascriptArray::Push will handle other cases.
        if (JavascriptNativeIntArray::IsNonCrossSite(array))
        {TRACE_IT(56744);
            JavascriptNativeIntArray * nativeIntArray = JavascriptNativeIntArray::FromVar(array);
            Assert(!nativeIntArray->IsCrossSiteObject());
            uint32 n = nativeIntArray->length;

            if(n < JavascriptArray::MaxArrayLength)
            {TRACE_IT(56745);
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
    {TRACE_IT(56746);
        // Handle non crossSite native int arrays here length within MaxArrayLength.
        // JavascriptArray::Push will handle other cases.
        if(JavascriptNativeFloatArray::IsNonCrossSite(array))
        {TRACE_IT(56747);
            JavascriptNativeFloatArray * nativeFloatArray = JavascriptNativeFloatArray::FromVar(array);
            Assert(!nativeFloatArray->IsCrossSiteObject());
            uint32 n = nativeFloatArray->length;

            if(n < JavascriptArray::MaxArrayLength)
            {TRACE_IT(56748);
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
    {TRACE_IT(56749);
        Var args[2];
        args[0] = object;
        args[1] = value;

        if (JavascriptArray::Is(object))
        {TRACE_IT(56750);
            return EntryPushJavascriptArray(scriptContext, args, 2);
        }
        else
        {TRACE_IT(56751);
            return EntryPushNonJavascriptArray(scriptContext, args, 2);
        }

    }

    /*
    *   EntryPushNonJavascriptArray
    *   - Handles Entry push calls, when Objects are not javascript arrays
    */
    Var JavascriptArray::EntryPushNonJavascriptArray(ScriptContext * scriptContext, Var * args, uint argCount)
    {TRACE_IT(56752);
            RecyclableObject* obj = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(56753);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.push"));
            }

            Var length = JavascriptOperators::OP_GetLength(obj, scriptContext);
            if(JavascriptOperators::GetTypeId(length) == TypeIds_Undefined && scriptContext->GetThreadContext()->IsDisableImplicitCall() &&
                scriptContext->GetThreadContext()->GetImplicitCallFlags() != Js::ImplicitCall_None)
            {TRACE_IT(56754);
                return length;
            }

            ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.push"));
            BigIndex n;
            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {TRACE_IT(56755);
                n = (uint64) JavascriptConversion::ToLength(length, scriptContext);
            }
            else
            {TRACE_IT(56756);
                n = JavascriptConversion::ToUInt32(length, scriptContext);
            }
            // First handle "small" indices.
            uint index;
            for (index=1; index < argCount && n < JavascriptArray::MaxArrayLength; ++index, ++n)
            {TRACE_IT(56757);
                if (h.IsThrowTypeError(JavascriptOperators::SetItem(obj, obj, n.GetSmallIndex(), args[index], scriptContext, PropertyOperation_ThrowIfNotExtensible)))
                {TRACE_IT(56758);
                    if (scriptContext->GetThreadContext()->RecordImplicitException())
                    {TRACE_IT(56759);
                        h.ThrowTypeErrorOnFailure();
                    }
                    else
                    {TRACE_IT(56760);
                        return nullptr;
                    }
                }
            }

            // Use BigIndex if we need to push indices >= MaxArrayLength
            if (index < argCount)
            {TRACE_IT(56761);
                BigIndex big = n;

                for (; index < argCount; ++index, ++big)
                {TRACE_IT(56762);
                    if (h.IsThrowTypeError(big.SetItem(obj, args[index], PropertyOperation_ThrowIfNotExtensible)))
                    {TRACE_IT(56763);
                        if(scriptContext->GetThreadContext()->RecordImplicitException())
                        {TRACE_IT(56764);
                            h.ThrowTypeErrorOnFailure();
                        }
                        else
                        {TRACE_IT(56765);
                            return nullptr;
                        }
                    }

                }

                // Set the new length; for objects it is all right for this to be >= MaxArrayLength
                if (h.IsThrowTypeError(JavascriptOperators::SetProperty(obj, obj, PropertyIds::length, big.ToNumber(scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible)))
                {TRACE_IT(56766);
                    if(scriptContext->GetThreadContext()->RecordImplicitException())
                    {TRACE_IT(56767);
                        h.ThrowTypeErrorOnFailure();
                    }
                    else
                    {TRACE_IT(56768);
                        return nullptr;
                    }
                }

                return big.ToNumber(scriptContext);
            }
            else
            {TRACE_IT(56769);
                // Set the new length
                Var lengthAsNUmberVar = JavascriptNumber::ToVar(n.IsSmallIndex() ? n.GetSmallIndex() : n.GetBigIndex(), scriptContext);
                if (h.IsThrowTypeError(JavascriptOperators::SetProperty(obj, obj, PropertyIds::length, lengthAsNUmberVar, scriptContext, PropertyOperation_ThrowIfNotExtensible)))
                {TRACE_IT(56770);
                    if(scriptContext->GetThreadContext()->RecordImplicitException())
                    {TRACE_IT(56771);
                        h.ThrowTypeErrorOnFailure();
                    }
                    else
                    {TRACE_IT(56772);
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
    {TRACE_IT(56773);
        JavascriptArray * arr = JavascriptArray::FromAnyArray(args[0]);
        uint n = arr->length;
        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.push"));

        // Fast Path for one push for small indexes
        if (argCount == 2 && n < JavascriptArray::MaxArrayLength)
        {TRACE_IT(56774);
            // Set Item is overridden by CrossSiteObject, so no need to check for IsCrossSiteObject()
            h.ThrowTypeErrorOnFailure(arr->SetItem(n, args[1], PropertyOperation_None));
            return JavascriptNumber::ToVar(n + 1, scriptContext);
        }

        // Fast Path for multiple push for small indexes
        if (JavascriptArray::MaxArrayLength - argCount + 1 > n && JavascriptArray::IsVarArray(arr) && scriptContext == arr->GetScriptContext())
        {TRACE_IT(56775);
            uint index;
            for (index = 1; index < argCount; ++index, ++n)
            {TRACE_IT(56776);
                Assert(n != JavascriptArray::MaxArrayLength);
                // Set Item is overridden by CrossSiteObject, so no need to check for IsCrossSiteObject()
                arr->JavascriptArray::DirectSetItemAt(n, args[index]);
            }
            return JavascriptNumber::ToVar(n, scriptContext);
        }

        return EntryPushJavascriptArrayNoFastPath(scriptContext, args, argCount);
    }

    Var JavascriptArray::EntryPushJavascriptArrayNoFastPath(ScriptContext * scriptContext, Var * args, uint argCount)
    {TRACE_IT(56777);
        JavascriptArray * arr = JavascriptArray::FromAnyArray(args[0]);
        uint n = arr->length;
        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.push"));

        // First handle "small" indices.
        uint index;
        for (index = 1; index < argCount && n < JavascriptArray::MaxArrayLength; ++index, ++n)
        {TRACE_IT(56778);
            // Set Item is overridden by CrossSiteObject, so no need to check for IsCrossSiteObject()
            h.ThrowTypeErrorOnFailure(arr->SetItem(n, args[index], PropertyOperation_None));
        }

        // Use BigIndex if we need to push indices >= MaxArrayLength
        if (index < argCount)
        {TRACE_IT(56779);
            // Not supporting native array with BigIndex.
            arr = EnsureNonNativeArray(arr);
            Assert(n == JavascriptArray::MaxArrayLength);
            for (BigIndex big = n; index < argCount; ++index, ++big)
            {TRACE_IT(56780);
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
        {TRACE_IT(56781);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.push"));
        }

        if (JavascriptArray::Is(args[0]))
        {TRACE_IT(56782);
            return EntryPushJavascriptArray(scriptContext, args.Values, args.Info.Count);
        }
        else
        {TRACE_IT(56783);
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
        {TRACE_IT(56784);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reverse"));
        }

        BigIndex length = 0u;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]))
        {TRACE_IT(56785);
            pArr = JavascriptArray::FromVar(args[0]);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pArr);
#endif
            obj = pArr;
        }
        else
        {TRACE_IT(56786);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(56787);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reverse"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(56788);
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);

        }
        else
        {TRACE_IT(56789);
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {TRACE_IT(56790);
            return JavascriptArray::ReverseHelper(pArr, nullptr, obj, length.GetSmallIndex(), scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max()); // if pArr is not null lets make sure length is safe to cast, which will only happen if length is a uint32max
        return JavascriptArray::ReverseHelper(pArr, nullptr, obj, length.GetBigIndex(), scriptContext);
    }

    // Array.prototype.reverse as described in ES6.0 (draft 22) Section 22.1.3.20
    template <typename T>
    Var JavascriptArray::ReverseHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, ScriptContext* scriptContext)
    {TRACE_IT(56791);
        T middle = length / 2;
        Var lowerValue = nullptr, upperValue = nullptr;
        T lowerExists, upperExists;
        const char16* methodName;
        bool isTypedArrayEntryPoint = typedArrayBase != nullptr;

        if (isTypedArrayEntryPoint)
        {TRACE_IT(56792);
            methodName = _u("[TypedArray].prototype.reverse");
        }
        else
        {TRACE_IT(56793);
            methodName = _u("Array.prototype.reverse");
        }

        // If we came from Array.prototype.map and source object is not a JavascriptArray, source could be a TypedArray
        if (!isTypedArrayEntryPoint && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(56794);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        ThrowTypeErrorOnFailureHelper h(scriptContext, methodName);

        if (pArr)
        {TRACE_IT(56795);
            Recycler * recycler = scriptContext->GetRecycler();

            if (length <= 1)
            {TRACE_IT(56796);
                return pArr;
            }

            if (pArr->IsFillFromPrototypes())
            {TRACE_IT(56797);
                // For odd-length arrays, the middle element is unchanged,
                // so we cannot fill it from the prototypes.
                if (length % 2 == 0)
                {TRACE_IT(56798);
                    pArr->FillFromPrototypes(0, (uint32)length);
                }
                else
                {TRACE_IT(56799);
                    middle = length / 2;
                    pArr->FillFromPrototypes(0, (uint32)middle);
                    pArr->FillFromPrototypes(1 + (uint32)middle, (uint32)length);
                }
            }

            if (pArr->HasNoMissingValues() && pArr->head && pArr->head->next)
            {TRACE_IT(56800);
                // This function currently does not track missing values in the head segment if there are multiple segments
                pArr->SetHasNoMissingValues(false);
            }

            // Above FillFromPrototypes call can change the length of the array. Our segment calculation below will
            // not work with the stale length. Update the length.
            // Note : since we are reversing the whole segment below - the functionality is not spec compliant already.
            length = pArr->length;

            SparseArraySegmentBase* seg = pArr->head;
            SparseArraySegmentBase *prevSeg = nullptr;
            SparseArraySegmentBase *nextSeg = nullptr;
            SparseArraySegmentBase *pinPrevSeg = nullptr;

            bool isIntArray = false;
            bool isFloatArray = false;

            if (JavascriptNativeIntArray::Is(pArr))
            {TRACE_IT(56801);
                isIntArray = true;
            }
            else if (JavascriptNativeFloatArray::Is(pArr))
            {TRACE_IT(56802);
                isFloatArray = true;
            }

            while (seg)
            {TRACE_IT(56803);
                nextSeg = seg->next;

                // If seg.length == 0, it is possible that (seg.left + seg.length == prev.left + prev.length),
                // resulting in 2 segments sharing the same "left".
                if (seg->length > 0)
                {TRACE_IT(56804);
                    if (isIntArray)
                    {TRACE_IT(56805);
                        ((SparseArraySegment<int32>*)seg)->ReverseSegment(recycler);
                    }
                    else if (isFloatArray)
                    {TRACE_IT(56806);
                        ((SparseArraySegment<double>*)seg)->ReverseSegment(recycler);
                    }
                    else
                    {TRACE_IT(56807);
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
            {TRACE_IT(56808);
                if (pArr->head && pArr->head->next && SparseArraySegmentBase::IsLeafSegment(pArr->head, recycler))
                {TRACE_IT(56809);
                    pArr->ReallocNonLeafSegment(SparseArraySegment<int32>::From(pArr->head), pArr->head->next);
                }
                pArr->EnsureHeadStartsFromZero<int32>(recycler);
            }
            else if (isFloatArray)
            {TRACE_IT(56810);
                if (pArr->head && pArr->head->next && SparseArraySegmentBase::IsLeafSegment(pArr->head, recycler))
                {TRACE_IT(56811);
                    pArr->ReallocNonLeafSegment(SparseArraySegment<double>::From(pArr->head), pArr->head->next);
                }
                pArr->EnsureHeadStartsFromZero<double>(recycler);
            }
            else
            {TRACE_IT(56812);
                pArr->EnsureHeadStartsFromZero<Var>(recycler);
            }

            pArr->InvalidateLastUsedSegment(); // lastUsedSegment might be 0-length and discarded above

#ifdef VALIDATE_ARRAY
            pArr->ValidateArray();
#endif
        }
        else if (typedArrayBase)
        {TRACE_IT(56813);
            Assert(length <= JavascriptArray::MaxArrayLength);
            if (typedArrayBase->GetLength() == length)
            {TRACE_IT(56814);
                // If typedArrayBase->length == length then we know that the TypedArray will have all items < length
                // and we won't have to check that the elements exist or not.
                for (uint32 lower = 0; lower < (uint32)middle; lower++)
                {TRACE_IT(56815);
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
            {TRACE_IT(56816);
                Assert(middle <= UINT_MAX);
                for (uint32 lower = 0; lower < (uint32)middle; lower++)
                {TRACE_IT(56817);
                    uint32 upper = (uint32)length - lower - 1;

                    lowerValue = typedArrayBase->DirectGetItem(lower);
                    upperValue = typedArrayBase->DirectGetItem(upper);

                    lowerExists = typedArrayBase->HasItem(lower);
                    upperExists = typedArrayBase->HasItem(upper);

                    if (lowerExists)
                    {TRACE_IT(56818);
                        if (upperExists)
                        {TRACE_IT(56819);
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(lower, upperValue));
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(upper, lowerValue));
                        }
                        else
                        {TRACE_IT(56820);
                            // This will always fail for a TypedArray if lower < length
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DeleteItem(lower, PropertyOperation_ThrowOnDeleteIfNotConfig));
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(upper, lowerValue));
                        }
                    }
                    else
                    {TRACE_IT(56821);
                        if (upperExists)
                        {TRACE_IT(56822);
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DirectSetItem(lower, upperValue));
                            // This will always fail for a TypedArray if upper < length
                            h.ThrowTypeErrorOnFailure(typedArrayBase->DeleteItem(upper, PropertyOperation_ThrowOnDeleteIfNotConfig));
                        }
                    }
                }
            }
        }
        else
        {TRACE_IT(56823);
            for (T lower = 0; lower < middle; lower++)
            {TRACE_IT(56824);
                T upper = length - lower - 1;

                lowerExists = JavascriptOperators::HasItem(obj, lower) &&
                              JavascriptOperators::GetItem(obj, lower, &lowerValue, scriptContext);

                upperExists = JavascriptOperators::HasItem(obj, upper) &&
                              JavascriptOperators::GetItem(obj, upper, &upperValue, scriptContext);

                if (lowerExists)
                {TRACE_IT(56825);
                    if (upperExists)
                    {TRACE_IT(56826);
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(obj, obj, lower, upperValue, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(obj, obj, upper, lowerValue, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                    }
                    else
                    {TRACE_IT(56827);
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(obj, lower, PropertyOperation_ThrowOnDeleteIfNotConfig));
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(obj, obj, upper, lowerValue, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                    }
                }
                else
                {TRACE_IT(56828);
                    if (upperExists)
                    {TRACE_IT(56829);
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
    {TRACE_IT(56830);
        Recycler * recycler = scriptContext->GetRecycler();

        SparseArraySegment<T>* next = SparseArraySegment<T>::From(pArr->head->next);
        while (next)
        {TRACE_IT(56831);
            next->left--;
            next = SparseArraySegment<T>::From(next->next);
        }

        // head and next might overlap as the next segment left is decremented
        next = SparseArraySegment<T>::From(pArr->head->next);
        if (next && (pArr->head->size > next->left))
        {TRACE_IT(56832);
            AssertMsg(pArr->head->left == 0, "Array always points to a head starting at index 0");
            AssertMsg(pArr->head->size == next->left + 1, "Shift next->left overlaps current segment by more than 1 element");

            SparseArraySegment<T> *head = SparseArraySegment<T>::From(pArr->head);
            // Merge the two adjacent segments
            if (next->length != 0)
            {TRACE_IT(56833);
                uint32 offset = head->size - 1;
                // There is room for one unshifted element in head segment.
                // Hence it's enough if we grow the head segment by next->length - 1

                if (next->next)
                {TRACE_IT(56834);
                    // If we have a next->next, we can't grow pass the left of that

                    // If the array had a segment map before, the next->next might just be right after next as well.
                    // So we just need to grow to the end of the next segment
                    // TODO: merge that segment too?
                    Assert(next->next->left >= head->size);
                    uint32 maxGrowSize = next->next->left - head->size;
                    if (maxGrowSize != 0)
                    {TRACE_IT(56835);
                        head = head->GrowByMinMax(recycler, next->length - 1, maxGrowSize); //-1 is to account for unshift
                    }
                    else
                    {TRACE_IT(56836);
                        // The next segment is only of length one, so we already have space in the header to copy that
                        Assert(next->length == 1);
                    }
                }
                else
                {TRACE_IT(56837);
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
        {TRACE_IT(56838);
            return res;
        }
        if (JavascriptArray::Is(args[0]))
        {TRACE_IT(56839);
            JavascriptArray * pArr = JavascriptArray::FromVar(args[0]);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pArr);
#endif

            if (pArr->length == 0)
            {TRACE_IT(56840);
                return res;
            }

            if(pArr->IsFillFromPrototypes())
            {TRACE_IT(56841);
                pArr->FillFromPrototypes(0, pArr->length); // We need find all missing value from [[proto]] object
            }

            if(pArr->HasNoMissingValues() && pArr->head && pArr->head->next)
            {TRACE_IT(56842);
                // This function currently does not track missing values in the head segment if there are multiple segments
                pArr->SetHasNoMissingValues(false);
            }

            pArr->length--;

            pArr->ClearSegmentMap(); // Dump segmentMap on shift (before any allocation)

            Recycler * recycler = scriptContext->GetRecycler();

            bool isIntArray = false;
            bool isFloatArray = false;

            if(JavascriptNativeIntArray::Is(pArr))
            {TRACE_IT(56843);
                isIntArray = true;
            }
            else if(JavascriptNativeFloatArray::Is(pArr))
            {TRACE_IT(56844);
                isFloatArray = true;
            }

            if (pArr->head->length != 0)
            {TRACE_IT(56845);
                if(isIntArray)
                {TRACE_IT(56846);
                    int32 nativeResult = SparseArraySegment<int32>::From(pArr->head)->GetElement(0);

                    if(SparseArraySegment<int32>::IsMissingItem(&nativeResult))
                    {TRACE_IT(56847);
                        res = scriptContext->GetLibrary()->GetUndefined();
                    }
                    else
                    {TRACE_IT(56848);
                        res = Js::JavascriptNumber::ToVar(nativeResult, scriptContext);
                    }
                    SparseArraySegment<int32>::From(pArr->head)->RemoveElement(recycler, 0);
                }
                else if (isFloatArray)
                {TRACE_IT(56849);
                    double nativeResult = SparseArraySegment<double>::From(pArr->head)->GetElement(0);

                    if(SparseArraySegment<double>::IsMissingItem(&nativeResult))
                    {TRACE_IT(56850);
                        res = scriptContext->GetLibrary()->GetUndefined();
                    }
                    else
                    {TRACE_IT(56851);
                        res = Js::JavascriptNumber::ToVarNoCheck(nativeResult, scriptContext);
                    }
                    SparseArraySegment<double>::From(pArr->head)->RemoveElement(recycler, 0);
                }
                else
                {TRACE_IT(56852);
                    res = SparseArraySegment<Var>::From(pArr->head)->GetElement(0);

                    if(SparseArraySegment<Var>::IsMissingItem(&res))
                    {TRACE_IT(56853);
                        res = scriptContext->GetLibrary()->GetUndefined();
                    }
                    else
                    {TRACE_IT(56854);
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
        {TRACE_IT(56855);
            RecyclableObject* dynamicObject = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
            {TRACE_IT(56856);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.shift"));
            }

            ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.shift"));

            BigIndex length = 0u;
            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {TRACE_IT(56857);
                length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
            }
            else
            {TRACE_IT(56858);
                length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
            }

            if (length == 0u)
            {TRACE_IT(56859);
                // If length is 0, return 'undefined'
                h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, TaggedInt::ToVarUnchecked(0), scriptContext, PropertyOperation_ThrowIfNotExtensible));
                return scriptContext->GetLibrary()->GetUndefined();
            }
            if (!JavascriptOperators::GetItem(dynamicObject, 0u, &res, scriptContext))
            {TRACE_IT(56860);
                res = scriptContext->GetLibrary()->GetUndefined();
            }
            --length;
            uint32 lengthToUin32Max = length.IsSmallIndex() ? length.GetSmallIndex() : MaxArrayLength;
            for (uint32 i = 0u; i < lengthToUin32Max; i++)
            {TRACE_IT(56861);
                if (JavascriptOperators::HasItem(dynamicObject, i + 1))
                {TRACE_IT(56862);
                    Var element = JavascriptOperators::GetItem(dynamicObject, i + 1, scriptContext);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(dynamicObject, dynamicObject, i, element, scriptContext, PropertyOperation_ThrowIfNotExtensible, /*skipPrototypeCheck*/ true));
                }
                else
                {TRACE_IT(56863);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, i, PropertyOperation_ThrowOnDeleteIfNotConfig));
                }
            }

            for (uint64 i = MaxArrayLength; length > i; i++)
            {TRACE_IT(56864);
                if (JavascriptOperators::HasItem(dynamicObject, i + 1))
                {TRACE_IT(56865);
                    Var element = JavascriptOperators::GetItem(dynamicObject, i + 1, scriptContext);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(dynamicObject, dynamicObject, i, element, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                }
                else
                {TRACE_IT(56866);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, i, PropertyOperation_ThrowOnDeleteIfNotConfig));
                }
            }

            if (length.IsSmallIndex())
            {TRACE_IT(56867);
                h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, length.GetSmallIndex(), PropertyOperation_ThrowOnDeleteIfNotConfig));
                h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, JavascriptNumber::ToVar(length.GetSmallIndex(), scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
            }
            else
            {TRACE_IT(56868);
                h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(dynamicObject, length.GetBigIndex(), PropertyOperation_ThrowOnDeleteIfNotConfig));
                h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(dynamicObject, dynamicObject, PropertyIds::length, JavascriptNumber::ToVar(length.GetBigIndex(), scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
            }
        }
        return res;
    }

    Js::JavascriptArray* JavascriptArray::CreateNewArrayHelper(uint32 len, bool isIntArray, bool isFloatArray,  Js::JavascriptArray* baseArray, ScriptContext* scriptContext)
    {TRACE_IT(56869);
        if (isIntArray)
        {TRACE_IT(56870);
            Js::JavascriptNativeIntArray *pnewArr = scriptContext->GetLibrary()->CreateNativeIntArray(len);
            pnewArr->EnsureHead<int32>();
#if ENABLE_PROFILE_INFO
            pnewArr->CopyArrayProfileInfo(Js::JavascriptNativeIntArray::FromVar(baseArray));
#endif

            return pnewArr;
        }
        else if (isFloatArray)
        {TRACE_IT(56871);
            Js::JavascriptNativeFloatArray *pnewArr  = scriptContext->GetLibrary()->CreateNativeFloatArray(len);
            pnewArr->EnsureHead<double>();
#if ENABLE_PROFILE_INFO
            pnewArr->CopyArrayProfileInfo(Js::JavascriptNativeFloatArray::FromVar(baseArray));
#endif

            return pnewArr;
        }
        else
        {TRACE_IT(56872);
            JavascriptArray *pnewArr = pnewArr = scriptContext->GetLibrary()->CreateArray(len);
            pnewArr->EnsureHead<Var>();
            return pnewArr;
        }
   }

    template<typename T>
    void JavascriptArray::SliceHelper(JavascriptArray* pArr,  JavascriptArray* pnewArr, uint32 start, uint32 newLen)
    {TRACE_IT(56873);
        SparseArraySegment<T>* headSeg = SparseArraySegment<T>::From(pArr->head);
        SparseArraySegment<T>* pnewHeadSeg = SparseArraySegment<T>::From(pnewArr->head);

        // Fill the newly created sliced array
        CopyArray(pnewHeadSeg->elements, newLen, headSeg->elements + start, newLen);
        pnewHeadSeg->length = newLen;

        Assert(pnewHeadSeg->length <= pnewHeadSeg->size);
        // Prototype lookup for missing elements
        if (!pArr->HasNoMissingValues())
        {TRACE_IT(56874);
            for (uint32 i = 0; i < newLen && (i + start) < pArr->length; i++)
            {TRACE_IT(56875);
                // array type might be changed in the below call to DirectGetItemAtFull
                // need recheck array type before checking array item [i + start]
                if (pArr->IsMissingItem(i + start))
                {TRACE_IT(56876);
                    Var element;
                    pnewArr->SetHasNoMissingValues(false);
                    if (pArr->DirectGetItemAtFull(i + start, &element))
                    {TRACE_IT(56877);
                        pnewArr->SetItem(i, element, PropertyOperation_None);
                    }
                }
            }
        }
#ifdef DBG
        else
        {TRACE_IT(56878);
            for (uint32 i = 0; i < newLen; i++)
            {TRACE_IT(56879);
                AssertMsg(!SparseArraySegment<T>::IsMissingItem(&headSeg->elements[i+start]), "Array marked incorrectly as having missing value");
            }
        }
#endif
    }

    // If the creating profile data has changed, convert it to the type of array indicated
    // in the profile
    void JavascriptArray::GetArrayTypeAndConvert(bool* isIntArray, bool* isFloatArray)
    {TRACE_IT(56880);
        if (JavascriptNativeIntArray::Is(this))
        {TRACE_IT(56881);
#if ENABLE_PROFILE_INFO
            JavascriptNativeIntArray* nativeIntArray = JavascriptNativeIntArray::FromVar(this);
            ArrayCallSiteInfo* info = nativeIntArray->GetArrayCallSiteInfo();
            if(!info || info->IsNativeIntArray())
            {TRACE_IT(56882);
                *isIntArray = true;
            }
            else if(info->IsNativeFloatArray())
            {TRACE_IT(56883);
                JavascriptNativeIntArray::ToNativeFloatArray(nativeIntArray);
                *isFloatArray = true;
            }
            else
            {TRACE_IT(56884);
                JavascriptNativeIntArray::ToVarArray(nativeIntArray);
            }
#else
            *isIntArray = true;
#endif
        }
        else if (JavascriptNativeFloatArray::Is(this))
        {TRACE_IT(56885);
#if ENABLE_PROFILE_INFO
            JavascriptNativeFloatArray* nativeFloatArray = JavascriptNativeFloatArray::FromVar(this);
            ArrayCallSiteInfo* info = nativeFloatArray->GetArrayCallSiteInfo();

            if(info && !info->IsNativeArray())
            {TRACE_IT(56886);
                JavascriptNativeFloatArray::ToVarArray(nativeFloatArray);
            }
            else
            {TRACE_IT(56887);
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
        {TRACE_IT(56888);
            return res;
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && scriptContext == JavascriptArray::FromVar(args[0])->GetScriptContext())
        {TRACE_IT(56889);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {TRACE_IT(56890);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(56891);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.slice"));
            }
        }

        Var lenValue = JavascriptOperators::OP_GetLength(obj, scriptContext);
        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(56892);
            length = (uint64) JavascriptConversion::ToLength(lenValue, scriptContext);
        }
        else
        {TRACE_IT(56893);
            length = JavascriptConversion::ToUInt32(lenValue, scriptContext);
        }

        if (length.IsSmallIndex())
        {TRACE_IT(56894);
            return JavascriptArray::SliceHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max());
        return JavascriptArray::SliceHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.slice as described in ES6.0 (draft 22) Section 22.1.3.22
    template <typename T>
    Var JavascriptArray::SliceHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(56895);
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
        {TRACE_IT(56896);
            startT = GetFromIndex(args[1], length, scriptContext);

            if (startT > length)
            {TRACE_IT(56897);
                startT = length;
            }

            if (args.Info.Count > 2)
            {TRACE_IT(56898);
                if (JavascriptOperators::GetTypeId(args[2]) == TypeIds_Undefined)
                {TRACE_IT(56899);
                    endT = length;
                }
                else
                {TRACE_IT(56900);
                    endT = GetFromIndex(args[2], length, scriptContext);

                    if (endT > length)
                    {TRACE_IT(56901);
                        endT = length;
                    }
                }
            }

            newLenT = endT > startT ? endT - startT : 0;
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of arguments start or end may convert the array to an ES5 array.
        if (pArr && !JavascriptArray::Is(obj))
        {TRACE_IT(56902);
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (TypedArrayBase::IsDetachedTypedArray(obj))
        {TRACE_IT(56903);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("Array.prototype.slice"));
        }

        // If we came from Array.prototype.slice and source object is not a JavascriptArray, source could be a TypedArray
        if (!isTypedArrayEntryPoint && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(56904);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        // If the entry point is %TypedArray%.prototype.slice or the source object is an Array exotic object we should try to load the constructor property
        // and use it to construct the return object.
        if (isTypedArrayEntryPoint)
        {TRACE_IT(56905);
            Var constructor = JavascriptOperators::SpeciesConstructor(typedArrayBase, TypedArrayBase::GetDefaultConstructor(args[0], scriptContext), scriptContext);
            isBuiltinArrayCtor = (constructor == library->GetArrayConstructor());

            // If we have an array source object, we need to make sure to do the right thing if it's a native array.
            // The helpers below which do the element copying require the source and destination arrays to have the same native type.
            if (pArr && isBuiltinArrayCtor)
            {TRACE_IT(56906);
                if (newLenT > JavascriptArray::MaxArrayLength)
                {TRACE_IT(56907);
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
                }

                // If the constructor function is the built-in Array constructor, we can be smart and create the right type of native array.
                pArr->GetArrayTypeAndConvert(&isIntArray, &isFloatArray);
                newArr = CreateNewArrayHelper(static_cast<uint32>(newLenT), isIntArray, isFloatArray, pArr, scriptContext);
                newObj = newArr;
            }
            else if (JavascriptOperators::IsConstructor(constructor))
            {TRACE_IT(56908);
                if (pArr)
                {TRACE_IT(56909);
                    // If the constructor function is any other function, it can return anything so we have to call it.
                    // Roll the source array into a non-native array if it was one.
                    pArr = EnsureNonNativeArray(pArr);
                }

                Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(newLenT, scriptContext) };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = RecyclableObject::FromVar(TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), (uint32)newLenT, scriptContext));
            }
            else
            {TRACE_IT(56910);
                // We only need to throw a TypeError when the constructor property is not an actual constructor if %TypedArray%.prototype.slice was called
                JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidTypedArray_Constructor, _u("[TypedArray].prototype.slice"));
            }
        }

        else if (pArr != nullptr)
        {TRACE_IT(56911);
            newObj = ArraySpeciesCreate(pArr, newLenT, scriptContext, &isIntArray, &isFloatArray, &isBuiltinArrayCtor);
        }

        // skip the typed array and "pure" array case, we still need to handle special arrays like es5array, remote array, and proxy of array.
        else
        {TRACE_IT(56912);
            newObj = ArraySpeciesCreate(obj, newLenT, scriptContext, nullptr, nullptr, &isBuiltinArrayCtor);
        }

        // If we didn't create a new object above we will create a new array here.
        // This is the pre-ES6 behavior or the case of calling Array.prototype.slice with a constructor argument that is not a constructor function.
        if (newObj == nullptr)
        {TRACE_IT(56913);
            if (pArr)
            {TRACE_IT(56914);
                pArr->GetArrayTypeAndConvert(&isIntArray, &isFloatArray);
            }

            if (newLenT > JavascriptArray::MaxArrayLength)
            {TRACE_IT(56915);
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
            }

            newArr = CreateNewArrayHelper(static_cast<uint32>(newLenT), isIntArray, isFloatArray, pArr, scriptContext);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newArr);
#endif
            newObj = newArr;
        }
        else
        {TRACE_IT(56916);
            // If the new object we created is an array, remember that as it will save us time setting properties in the object below
            if (JavascriptArray::Is(newObj))
            {TRACE_IT(56917);
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
        {TRACE_IT(56918);
            return newObj;
        }

        // The ArraySpeciesCreate call above could have converted the source array into an ES5Array. If this happens
        // we will process the array elements like an ES5Array.
        if (pArr && !JavascriptArray::Is(obj))
        {TRACE_IT(56919);
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (pArr)
        {TRACE_IT(56920);
            // If we constructed a new Array object, we have some nice helpers here
            if (newArr && isBuiltinArrayCtor)
            {TRACE_IT(56921);
                if (JavascriptArray::IsDirectAccessArray(newArr))
                {TRACE_IT(56922);
                    if (((start + newLen) <= pArr->head->length) && newLen <= newArr->head->size) //Fast Path
                    {TRACE_IT(56923);
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
                    {TRACE_IT(56924);
                        if (isIntArray)
                        {TRACE_IT(56925);
                            CopyNativeIntArrayElements(JavascriptNativeIntArray::FromVar(newArr), 0, JavascriptNativeIntArray::FromVar(pArr), start, start + newLen);
                        }
                        else if (isFloatArray)
                        {TRACE_IT(56926);
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
                    {TRACE_IT(56927);
                        if (!pArr->DirectGetItemAtFull(i + start, &element))
                        {TRACE_IT(56928);
                            continue;
                        }

                        newArr->SetItem(i, element, PropertyOperation_None);

                        // Side-effects in the prototype lookup may have changed the source array into an ES5Array. If this happens
                        // we will process the rest of the array elements like an ES5Array.
                        if (!JavascriptArray::Is(obj))
                        {TRACE_IT(56929);
                            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                            return JavascriptArray::SliceObjectHelper(obj, start, i + 1, newArr, newObj, newLen, scriptContext);
                        }
                    }
                }
            }
            else
            {TRACE_IT(56930);
                // The constructed object isn't an array, we'll need to use normal object manipulation
                Var element;

                for (uint32 i = 0; i < newLen; i++)
                {TRACE_IT(56931);
                    if (!pArr->DirectGetItemAtFull(i + start, &element))
                    {TRACE_IT(56932);
                        continue;
                    }

                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, i, element), scriptContext, i);

                    // Side-effects in the prototype lookup may have changed the source array into an ES5Array. If this happens
                    // we will process the rest of the array elements like an ES5Array.
                    if (!JavascriptArray::Is(obj))
                    {TRACE_IT(56933);
                        AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                        return JavascriptArray::SliceObjectHelper(obj, start, i + 1, newArr, newObj, newLen, scriptContext);
                    }
                }
            }
        }
        else if (typedArrayBase)
        {TRACE_IT(56934);
            // Source is a TypedArray, we must have created the return object via a call to constructor, but newObj may not be a TypedArray (or an array either)
            TypedArrayBase* newTypedArray = nullptr;

            if (TypedArrayBase::Is(newObj))
            {TRACE_IT(56935);
                newTypedArray = TypedArrayBase::FromVar(newObj);
            }

            Var element;

            for (uint32 i = 0; i < newLen; i++)
            {TRACE_IT(56936);
                // We only need to call HasItem in the case that we are called from Array.prototype.slice
                if (!isTypedArrayEntryPoint && !typedArrayBase->HasItem(i + start))
                {TRACE_IT(56937);
                    continue;
                }

                element = typedArrayBase->DirectGetItem(i + start);

                // The object we got back from the constructor might not be a TypedArray. In fact, it could be any object.
                if (newTypedArray)
                {TRACE_IT(56938);
                    newTypedArray->DirectSetItem(i, element);
                }
                else if (newArr)
                {TRACE_IT(56939);
                    newArr->DirectSetItemAt(i, element);
                }
                else
                {TRACE_IT(56940);
                    JavascriptOperators::OP_SetElementI_UInt32(newObj, i, element, scriptContext, PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }
        else
        {TRACE_IT(56941);
            return JavascriptArray::SliceObjectHelper(obj, start, 0u, newArr, newObj, newLen, scriptContext);
        }

        if (!isTypedArrayEntryPoint)
        {TRACE_IT(56942);
            JavascriptOperators::SetProperty(newObj, newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(newLen, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible);
        }

#ifdef VALIDATE_ARRAY
        if (JavascriptArray::Is(newObj))
        {TRACE_IT(56943);
            JavascriptArray::FromVar(newObj)->ValidateArray();
        }
#endif

        return newObj;
    }

    Var JavascriptArray::SliceObjectHelper(RecyclableObject* obj, uint32 sliceStart, uint32 start, JavascriptArray* newArr, RecyclableObject* newObj, uint32 newLen, ScriptContext* scriptContext)
    {TRACE_IT(56944);
        for (uint32 i = start; i < newLen; i++)
        {TRACE_IT(56945);
            if (JavascriptOperators::HasItem(obj, i + sliceStart))
            {TRACE_IT(56946);
                Var element = JavascriptOperators::GetItem(obj, i + sliceStart, scriptContext);
                if (newArr != nullptr)
                {TRACE_IT(56947);
                    newArr->SetItem(i, element, PropertyOperation_None);
                }
                else
                {TRACE_IT(56948);
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, i, element), scriptContext, i);
                }
            }
        }

        JavascriptOperators::SetProperty(newObj, newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(newLen, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible);

#ifdef VALIDATE_ARRAY
        if (JavascriptArray::Is(newObj))
        {TRACE_IT(56949);
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
    {TRACE_IT(56950);
        CompareVarsInfo* cvInfo=(CompareVarsInfo*)cvInfoV;
        ScriptContext* requestContext=cvInfo->scriptContext;
        RecyclableObject* compFn=cvInfo->compFn;

        AssertMsg(*(Var*)aRef, "No null expected in sort");
        AssertMsg(*(Var*)bRef, "No null expected in sort");

        if (compFn != nullptr)
        {TRACE_IT(56951);
            ScriptContext* scriptContext = compFn->GetScriptContext();
            // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
            CallFlags flags = CallFlags_Value;
            Var undefined = scriptContext->GetLibrary()->GetUndefined();
            Var retVal;
            if (requestContext != scriptContext)
            {TRACE_IT(56952);
                Var leftVar = CrossSite::MarshalVar(scriptContext, *(Var*)aRef);
                Var rightVar = CrossSite::MarshalVar(scriptContext, *(Var*)bRef);
                retVal = CALL_FUNCTION(compFn, CallInfo(flags, 3), undefined, leftVar, rightVar);
            }
            else
            {TRACE_IT(56953);
                retVal = CALL_FUNCTION(compFn, CallInfo(flags, 3), undefined, *(Var*)aRef, *(Var*)bRef);
            }

            if (TaggedInt::Is(retVal))
            {TRACE_IT(56954);
                return TaggedInt::ToInt32(retVal);
            }
            double dblResult;
            if (JavascriptNumber::Is_NoTaggedIntCheck(retVal))
            {TRACE_IT(56955);
                dblResult = JavascriptNumber::GetValue(retVal);
            }
            else
            {TRACE_IT(56956);
                dblResult = JavascriptConversion::ToNumber_Full(retVal, scriptContext);
            }
            if (dblResult < 0)
            {TRACE_IT(56957);
                return -1;
            }
            return (dblResult > 0) ? 1 : 0;
        }
        else
        {TRACE_IT(56958);
            JavascriptString* pStr1 = JavascriptConversion::ToString(*(Var*)aRef, requestContext);
            JavascriptString* pStr2 = JavascriptConversion::ToString(*(Var*)bRef, requestContext);

            return JavascriptString::strcmp(pStr1, pStr2);
        }
    }

    static void hybridSort(__inout_ecount(length) Field(Var) *elements, uint32 length, CompareVarsInfo* compareInfo)
    {TRACE_IT(56959);
        // The cost of memory moves starts to be more expensive than additional comparer calls (given a simple comparer)
        // for arrays of more than 512 elements.
        if (length > 512)
        {
            qsort_s(elements, length, compareVars, compareInfo);
            return;
        }

        for (int i = 1; i < (int)length; i++)
        {
            if (compareVars(compareInfo, elements + i, elements + i - 1) < 0) {TRACE_IT(56960);
                // binary search for the left-most element greater than value:
                int first = 0;
                int last = i - 1;
                while (first <= last)
                {TRACE_IT(56961);
                    int middle = (first + last) / 2;
                    if (compareVars(compareInfo, elements + i, elements + middle) < 0)
                    {TRACE_IT(56962);
                        last = middle - 1;
                    }
                    else
                    {TRACE_IT(56963);
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
    {TRACE_IT(56964);
        if (length <= 1)
        {TRACE_IT(56965);
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
            {TRACE_IT(56966);
                if (compFn != nullptr)
                {TRACE_IT(56967);
                    countUndefined = startSeg->RemoveUndefined(scriptContext);

#ifdef VALIDATE_ARRAY
                    ValidateSegment(startSeg);
#endif
                    hybridSort(startSeg->elements, startSeg->length, &cvInfo);
                }
                else
                {TRACE_IT(56968);
                    countUndefined = sort(startSeg->elements, &startSeg->length, scriptContext);
                }
                head = startSeg;
            }
            else
            {TRACE_IT(56969);
                SparseArraySegment<Var>* allElements = SparseArraySegment<Var>::AllocateSegment(recycler, 0, 0, nullptr);
                SparseArraySegment<Var>* next = startSeg;

                uint32 nextIndex = 0;
                // copy all the elements to single segment
                while (next)
                {TRACE_IT(56970);
                    countUndefined += next->RemoveUndefined(scriptContext);
                    if (next->length != 0)
                    {TRACE_IT(56971);
                        allElements = SparseArraySegment<Var>::CopySegment(recycler, allElements, nextIndex, next, next->left, next->length);
                    }
                    next = SparseArraySegment<Var>::From(next->next);
                    nextIndex = allElements->length;

#ifdef VALIDATE_ARRAY
                    ValidateSegment(allElements);
#endif
                }

                if (compFn != nullptr)
                {TRACE_IT(56972);
                    hybridSort(allElements->elements, allElements->length, &cvInfo);
                }
                else
                {TRACE_IT(56973);
                    sort(allElements->elements, &allElements->length, scriptContext);
                }

                head = allElements;
                head->next = nullptr;
            }
        },
        [&](bool hasException)
        {TRACE_IT(56974);
            length = saveLength;
            ClearSegmentMap(); // Dump the segmentMap again in case user compare function rebuilds it
            if (hasException)
            {TRACE_IT(56975);
                head = startSeg;
                this->InvalidateLastUsedSegment();
            }
        });

#if DEBUG
        {
            uint32 countNull = 0;
            uint32 index = head->length - 1;
            while (countNull < head->length)
            {TRACE_IT(56976);
                if (SparseArraySegment<Var>::From(head)->elements[index] != NULL)
                {TRACE_IT(56977);
                    break;
                }
                index--;
                countNull++;
            }
            AssertMsg(countNull == 0, "No null expected at the end");
        }
#endif

        if (countUndefined != 0)
        {TRACE_IT(56978);
            // fill undefined at the end
            uint32 newLength = head->length + countUndefined;
            if (newLength > head->size)
            {TRACE_IT(56979);
                head = SparseArraySegment<Var>::From(head)->GrowByMin(recycler, newLength - head->size);
            }

            Var undefined = scriptContext->GetLibrary()->GetUndefined();
            for (uint32 i = head->length; i < newLength; i++)
            {TRACE_IT(56980);
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
    {TRACE_IT(56981);
        uint32 count = 0, countUndefined = 0;
        Element *elements = RecyclerNewArrayZ(scriptContext->GetRecycler(), Element, *len);
        RecyclableObject *undefined = scriptContext->GetLibrary()->GetUndefined();

        //
        // Create the Elements array
        //

        for (uint32 i = 0; i < *len; ++i)
        {TRACE_IT(56982);
            if (!SparseArraySegment<Var>::IsMissingItem(&orig[i]))
            {TRACE_IT(56983);
                if (!JavascriptOperators::IsUndefinedObject(orig[i], undefined))
                {TRACE_IT(56984);
                    elements[count].Value = orig[i];
                    elements[count].StringValue =  JavascriptConversion::ToString(orig[i], scriptContext);

                    count++;
                }
                else
                {TRACE_IT(56985);
                    countUndefined++;
                }
            }
        }

        if (count > 0)
        {
            SortElements(elements, 0, count - 1);

            for (uint32 i = 0; i < count; ++i)
            {TRACE_IT(56986);
                orig[i] = elements[i].Value;
            }
        }

        for (uint32 i = count + countUndefined; i < *len; ++i)
        {TRACE_IT(56987);
            orig[i] = SparseArraySegment<Var>::GetMissingItem();
        }

        *len = count; // set the correct length
        return countUndefined;
    }

    int __cdecl JavascriptArray::CompareElements(void* context, const void* elem1, const void* elem2)
    {TRACE_IT(56988);
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
        {TRACE_IT(56989);
            if (JavascriptConversion::IsCallable(args[1]))
            {TRACE_IT(56990);
                compFn = RecyclableObject::FromVar(args[1]);
            }
            else
            {TRACE_IT(56991);
                TypeId typeId = JavascriptOperators::GetTypeId(args[1]);

                // Use default comparer:
                // - In ES5 mode if the argument is undefined.
                bool useDefaultComparer = typeId == TypeIds_Undefined;
                if (!useDefaultComparer)
                {TRACE_IT(56992);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedInternalObject, _u("Array.prototype.sort"));
                }
            }
        }

        if (JavascriptArray::Is(args[0]))
        {TRACE_IT(56993);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif

            JavascriptArray *arr = JavascriptArray::FromVar(args[0]);

            if (arr->length <= 1)
            {TRACE_IT(56994);
                return args[0];
            }

            if(arr->IsFillFromPrototypes())
            {TRACE_IT(56995);
                arr->FillFromPrototypes(0, arr->length); // We need find all missing value from [[proto]] object
            }

            // Maintain nativity of the array only for the following cases (To favor inplace conversions - keeps the conversion cost less):
            // -    int cases for X86 and
            // -    FloatArray for AMD64
            // We convert the entire array back and forth once here O(n), rather than doing the costly conversion down the call stack which is O(nlogn)

#if defined(_M_X64_OR_ARM64)
            if(compFn && JavascriptNativeFloatArray::Is(arr))
            {TRACE_IT(56996);
                arr = JavascriptNativeFloatArray::ConvertToVarArray((JavascriptNativeFloatArray*)arr);
                arr->Sort(compFn);
                arr = arr->ConvertToNativeArrayInPlace<JavascriptNativeFloatArray, double>(arr);
            }
            else
            {TRACE_IT(56997);
                EnsureNonNativeArray(arr);
                arr->Sort(compFn);
            }
#else
            if(compFn && JavascriptNativeIntArray::Is(arr))
            {TRACE_IT(56998);
                //EnsureNonNativeArray(arr);
                arr = JavascriptNativeIntArray::ConvertToVarArray((JavascriptNativeIntArray*)arr);
                arr->Sort(compFn);
                arr = arr->ConvertToNativeArrayInPlace<JavascriptNativeIntArray, int32>(arr);
            }
            else
            {TRACE_IT(56999);
                EnsureNonNativeArray(arr);
                arr->Sort(compFn);
            }
#endif

        }
        else
        {TRACE_IT(57000);
            RecyclableObject* pObj = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &pObj))
            {TRACE_IT(57001);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.sort"));
            }
            uint32 len = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(pObj, scriptContext), scriptContext);
            JavascriptArray* sortArray = scriptContext->GetLibrary()->CreateArray(len);
            sortArray->EnsureHead<Var>();
            ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.sort"));

            BEGIN_TEMP_ALLOCATOR(tempAlloc, scriptContext, _u("Runtime"))
            {TRACE_IT(57002);
                JsUtil::List<uint32, ArenaAllocator>* indexList = JsUtil::List<uint32, ArenaAllocator>::New(tempAlloc);

                for (uint32 i = 0; i < len; i++)
                {TRACE_IT(57003);
                    Var item;
                    if (JavascriptOperators::GetItem(pObj, i, &item, scriptContext))
                    {TRACE_IT(57004);
                        indexList->Add(i);
                        sortArray->DirectSetItemAt(i, item);
                    }
                }
                if (indexList->Count() > 0)
                {TRACE_IT(57005);
                    if (sortArray->length > 1)
                    {TRACE_IT(57006);
                        sortArray->FillFromPrototypes(0, sortArray->length); // We need find all missing value from [[proto]] object
                    }
                    sortArray->Sort(compFn);

                    uint32 removeIndex = sortArray->head->length;
                    for (uint32 i = 0; i < removeIndex; i++)
                    {TRACE_IT(57007);
                        AssertMsg(!SparseArraySegment<Var>::IsMissingItem(&SparseArraySegment<Var>::From(sortArray->head)->elements[i]), "No gaps expected in sorted array");
                        h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(pObj, pObj, i, SparseArraySegment<Var>::From(sortArray->head)->elements[i], scriptContext));
                    }
                    for (int i = 0; i < indexList->Count(); i++)
                    {TRACE_IT(57008);
                        uint32 value = indexList->Item(i);
                        if (value >= removeIndex)
                        {TRACE_IT(57009);
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
        {TRACE_IT(57010);
            isArr = true;
            pArr = JavascriptArray::FromVar(args[0]);
            pObj = pArr;
            len = pArr->length;

#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
        }
        else
        {TRACE_IT(57011);
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &pObj))
            {TRACE_IT(57012);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.splice"));
            }

            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {TRACE_IT(57013);
                int64 len64 = JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(pObj, scriptContext), scriptContext);
                len = len64 > UINT_MAX ? UINT_MAX : (uint)len64;
            }
            else
            {TRACE_IT(57014);
                len = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(pObj, scriptContext), scriptContext);
            }
        }

        switch (args.Info.Count)
        {
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
            {TRACE_IT(57015);
                start = len;
            }

            // When start >= len, we know we won't be deleting any items and don't really need to evaluate the second argument.
            // However, ECMA 262 15.4.4.12 requires that it be evaluated, anyway.  If the argument is an object with a valueOf
            // with a side effect, this evaluation is observable.  Hence, we must evaluate.
            if (TaggedInt::Is(args[2]))
            {TRACE_IT(57016);
                int intDeleteLen = TaggedInt::ToInt32(args[2]);
                if (intDeleteLen < 0)
                {TRACE_IT(57017);
                    deleteLen = 0;
                }
                else
                {TRACE_IT(57018);
                    deleteLen = intDeleteLen;
                }
            }
            else
            {TRACE_IT(57019);
                double dblDeleteLen = JavascriptConversion::ToInteger(args[2], scriptContext);

                if (dblDeleteLen > len)
                {TRACE_IT(57020);
                    deleteLen = (uint32)-1;
                }
                else if (dblDeleteLen <= 0)
                {TRACE_IT(57021);
                    deleteLen = 0;
                }
                else
                {TRACE_IT(57022);
                    deleteLen = (uint32)dblDeleteLen;
                }
            }
            deleteLen = min(len - start, deleteLen);
            break;
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of arguments start or deleteCount may convert the array to an ES5 array.
        if (isArr && !JavascriptArray::Is(pObj))
        {TRACE_IT(57023);
            AssertOrFailFastMsg(ES5Array::Is(pObj), "The array should have been converted to an ES5Array");
            isArr = false;
        }

        Var* insertArgs = args.Info.Count > 3 ? &args.Values[3] : nullptr;
        uint32 insertLen = args.Info.Count > 3 ? args.Info.Count - 3 : 0;

        ::Math::RecordOverflowPolicy newLenOverflow;
        uint32 newLen = UInt32Math::Add(len - deleteLen, insertLen, newLenOverflow); // new length of the array after splice

        if (isArr)
        {TRACE_IT(57024);
            // If we have missing values then convert to not native array for now
            // In future, we could support this scenario.
            if (deleteLen == insertLen)
            {TRACE_IT(57025);
                pArr->FillFromPrototypes(start, start + deleteLen);
            }
            else if (len)
            {TRACE_IT(57026);
                pArr->FillFromPrototypes(start, len);
            }

            //
            // If newLen overflowed, pre-process to prevent pushing sparse array segments or elements out of
            // max array length, which would result in tons of index overflow and difficult to fix.
            //
            if (newLenOverflow.HasOverflowed())
            {TRACE_IT(57027);
                pArr = EnsureNonNativeArray(pArr);
                BigIndex dstIndex = MaxArrayLength;

                uint32 maxInsertLen = MaxArrayLength - start;
                if (insertLen > maxInsertLen)
                {TRACE_IT(57028);
                    // Copy overflowing insertArgs to properties
                    for (uint32 i = maxInsertLen; i < insertLen; i++)
                    {TRACE_IT(57029);
                        pArr->DirectSetItemAt(dstIndex, insertArgs[i]);
                        ++dstIndex;
                    }

                    insertLen = maxInsertLen; // update

                    // Truncate elements on the right to properties
                    if (start + deleteLen < len)
                    {TRACE_IT(57030);
                        pArr->TruncateToProperties(dstIndex, start + deleteLen);
                    }
                }
                else
                {TRACE_IT(57031);
                    // Truncate would-overflow elements to properties
                    pArr->TruncateToProperties(dstIndex, MaxArrayLength - insertLen + deleteLen);
                }

                len = pArr->length; // update
                newLen = len - deleteLen + insertLen;
                Assert(newLen == MaxArrayLength);
            }

            if (insertArgs)
            {TRACE_IT(57032);
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
            {TRACE_IT(57033);
                pArr = EnsureNonNativeArray(pArr);
                // If the new object we created is an array, remember that as it will save us time setting properties in the object below
                if (JavascriptArray::Is(newObj))
                {TRACE_IT(57034);
#if ENABLE_COPYONACCESS_ARRAY
                    JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                    newArr = JavascriptArray::FromVar(newObj);
                }
            }
            else
            // This is the ES5 case, pArr['constructor'] doesn't exist, or pArr['constructor'] is the builtin Array constructor
            {TRACE_IT(57035);
                pArr->GetArrayTypeAndConvert(&isIntArray, &isFloatArray);
                newArr = CreateNewArrayHelper(deleteLen, isIntArray, isFloatArray, pArr, scriptContext);
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newArr);
#endif
            }

            // If return object is a JavascriptArray, we can use all the array splice helpers
            if (newArr && isBuiltinArrayCtor && len == pArr->length)
            {TRACE_IT(57036);

                // Array has a single segment (need not start at 0) and splice start lies in the range
                // of that segment we optimize splice - Fast path.
                if (pArr->IsSingleSegmentArray() && pArr->head->HasIndex(start))
                {TRACE_IT(57037);
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
                {TRACE_IT(57038);
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
                {TRACE_IT(57039);
                    pArr->EnsureHeadStartsFromZero<int32>(recycler);
                    newArr->EnsureHeadStartsFromZero<int32>(recycler);
                }
                else if (isFloatArray)
                {TRACE_IT(57040);
                    pArr->EnsureHeadStartsFromZero<double>(recycler);
                    newArr->EnsureHeadStartsFromZero<double>(recycler);
                }
                else
                {TRACE_IT(57041);
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
                {TRACE_IT(57042);
                    pArr->SetLength(newLen);
                }
                else
                {TRACE_IT(57043);
                    pArr->length = newLen;
                }

                if (newArr->length != deleteLen)
                {TRACE_IT(57044);
                    newArr->SetLength(deleteLen);
                }
                else
                {TRACE_IT(57045);
                    newArr->length = deleteLen;
                }

                newArr->InvalidateLastUsedSegment();

#ifdef VALIDATE_ARRAY
                newArr->ValidateArray();
                pArr->ValidateArray();
#endif
                if (newLenOverflow.HasOverflowed())
                {TRACE_IT(57046);
                    // ES5 15.4.4.12 16: If new len overflowed, SetLength throws
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
                }

                return newArr;
            }
        }

        if (newLenOverflow.HasOverflowed())
        {TRACE_IT(57047);
            return ObjectSpliceHelper<BigIndex>(pObj, len, start, deleteLen, (Var*)insertArgs, insertLen, scriptContext, newObj);
        }
        else // Use uint32 version if no overflow
        {TRACE_IT(57048);
            return ObjectSpliceHelper<uint32>(pObj, len, start, deleteLen, (Var*)insertArgs, insertLen, scriptContext, newObj);
        }
    }

    inline BOOL JavascriptArray::IsSingleSegmentArray() const
    {TRACE_IT(57049);
        return nullptr == head->next;
    }

    template<typename T>
    void JavascriptArray::ArraySegmentSpliceHelper(JavascriptArray *pnewArr, SparseArraySegment<T> *seg, SparseArraySegment<T> **prev,
                                                    uint32 start, uint32 deleteLen, Var* insertArgs, uint32 insertLen, Recycler *recycler)
    {TRACE_IT(57050);
        // book keeping variables
        uint32 relativeStart    = start - seg->left;  // This will be different from start when head->left is non zero -
                                                      //(Missing elements at the beginning)

        uint32 headDeleteLen    = min(start + deleteLen , seg->left + seg->length) - start;   // actual number of elements to delete in
                                                                                              // head if deleteLen overflows the length of head

        uint32 newHeadLen       = seg->length - headDeleteLen + insertLen;     // new length of the head after splice

        // Save the deleted elements
        if (headDeleteLen != 0)
        {TRACE_IT(57051);
            pnewArr->InvalidateLastUsedSegment();
            pnewArr->head = SparseArraySegment<T>::CopySegment(recycler, SparseArraySegment<T>::From(pnewArr->head), 0, seg, start, headDeleteLen);
        }

        if (newHeadLen != 0)
        {TRACE_IT(57052);
            if (seg->size < newHeadLen)
            {TRACE_IT(57053);
                if (seg->next)
                {TRACE_IT(57054);
                    // If we have "next", require that we haven't adjusted next segments left yet.
                    seg = seg->GrowByMinMax(recycler, newHeadLen - seg->size, seg->next->left - deleteLen + insertLen - seg->left - seg->size);
                }
                else
                {TRACE_IT(57055);
                    seg = seg->GrowByMin(recycler, newHeadLen - seg->size);
                }
#ifdef VALIDATE_ARRAY
                ValidateSegment(seg);
#endif
            }

            // Move the elements if necessary
            if (headDeleteLen != insertLen)
            {TRACE_IT(57056);
                uint32 noElementsToMove = seg->length - (relativeStart + headDeleteLen);
                MoveArray(seg->elements + relativeStart + insertLen,
                            seg->elements + relativeStart + headDeleteLen,
                            noElementsToMove);
                if (newHeadLen < seg->length) // truncate if necessary
                {TRACE_IT(57057);
                    seg->Truncate(seg->left + newHeadLen); // set end elements to null so that when we introduce null elements we are safe
                }
                seg->length = newHeadLen;
            }
            // Copy the new elements
            if (insertLen > 0)
            {TRACE_IT(57058);
                Assert(!VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(pnewArr) &&
                   !VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(pnewArr));

                // inserted elements starts at argument 3 of splice(start, deleteNumber, insertelem1, insertelem2, insertelem3, ...);
                CopyArray(seg->elements + relativeStart, insertLen,
                          reinterpret_cast<const T*>(insertArgs), insertLen);
            }
            *prev = seg;
        }
        else
        {TRACE_IT(57059);
            *prev = SparseArraySegment<T>::From(seg->next);
        }
    }

    template<typename T>
    void JavascriptArray::ArraySpliceHelper(JavascriptArray* pnewArr, JavascriptArray* pArr, uint32 start, uint32 deleteLen, Var* insertArgs, uint32 insertLen, ScriptContext *scriptContext)
    {TRACE_IT(57060);
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
        {TRACE_IT(57061);
            rightLimit = JavascriptArray::MaxArrayLength;
        }

        // Find out the segment to start delete
        while (startSeg && (rightLimit <= start))
        {TRACE_IT(57062);
            savePrev = startSeg;
            prevPrevSeg = prevSeg;
            prevSeg = &startSeg->next;
            startSeg = SparseArraySegment<T>::From(startSeg->next);

            if (startSeg)
            {TRACE_IT(57063);
                if (UInt32Math::Add(startSeg->left, startSeg->size, &rightLimit))
                {TRACE_IT(57064);
                    rightLimit = JavascriptArray::MaxArrayLength;
                }
            }
        }

        // handle inlined segment
        SparseArraySegmentBase* inlineHeadSegment = nullptr;
        bool hasInlineSegment = false;
        // The following if else set is used to determine whether a shallow or hard copy is needed
        if (JavascriptNativeArray::Is(pArr))
        {TRACE_IT(57065);
            if (JavascriptNativeFloatArray::Is(pArr))
            {TRACE_IT(57066);
                inlineHeadSegment = DetermineInlineHeadSegmentPointer<JavascriptNativeFloatArray, 0, true>((JavascriptNativeFloatArray*)pArr);
            }
            else if (JavascriptNativeIntArray::Is(pArr))
            {TRACE_IT(57067);
                inlineHeadSegment = DetermineInlineHeadSegmentPointer<JavascriptNativeIntArray, 0, true>((JavascriptNativeIntArray*)pArr);
            }
            Assert(inlineHeadSegment);
            hasInlineSegment = (startSeg == (SparseArraySegment<T>*)inlineHeadSegment);
        }
        else
        {TRACE_IT(57068);
            // This will result in false positives. It is used because DetermineInlineHeadSegmentPointer
            // does not handle Arrays that change type e.g. from JavascriptNativeIntArray to JavascriptArray
            // This conversion in particular is problematic because JavascriptNativeIntArray is larger than JavascriptArray
            // so the returned head segment ptr never equals pArr->head. So we will default to using this and deal with
            // false positives. It is better than always doing a hard copy.
            hasInlineSegment = HasInlineHeadSegment(pArr->head->length);
        }

        if (startSeg)
        {TRACE_IT(57069);
            // Delete Phase
            if (startSeg->left <= start && (startSeg->left + startSeg->length) >= limit)
            {TRACE_IT(57070);
                // All splice happens in one segment.
                SparseArraySegmentBase *nextSeg = startSeg->next;
                // Splice the segment first, which might OOM throw but the array would be intact.
                JavascriptArray::ArraySegmentSpliceHelper(pnewArr, (SparseArraySegment<T>*)startSeg, (SparseArraySegment<T>**)prevSeg, start, deleteLen, insertArgs, insertLen, recycler);
                while (nextSeg)
                {TRACE_IT(57071);
                    // adjust next segments left
                    nextSeg->left = nextSeg->left - deleteLen + insertLen;
                    if (nextSeg->next == nullptr)
                    {TRACE_IT(57072);
                        nextSeg->EnsureSizeInBound();
                    }
                    nextSeg = nextSeg->next;
                }
                if (*prevSeg)
                {TRACE_IT(57073);
                    (*prevSeg)->EnsureSizeInBound();
                }
                return;
            }
            else
            {TRACE_IT(57074);
                SparseArraySegment<T>* newHeadSeg = nullptr; // pnewArr->head is null
                Field(SparseArraySegmentBase*)* prevNewHeadSeg = &pnewArr->head;

                // delete till deleteLen and reuse segments for new array if it is possible.
                // 3 steps -
                //1. delete 1st segment (which may be partial delete)
                // 2. delete next n complete segments
                // 3. delete last segment (which again may be partial delete)

                // Step (1)  -- WOOB 1116297: When left >= start, step (1) is skipped, resulting in pNewArr->head->left != 0. We need to touch up pNewArr.
                if (startSeg->left < start)
                {TRACE_IT(57075);
                    if (start < startSeg->left + startSeg->length)
                    {TRACE_IT(57076);
                        uint32 headDeleteLen = startSeg->left + startSeg->length - start;

                        if (startSeg->next)
                        {TRACE_IT(57077);
                            // We know the new segment will have a next segment, so allocate it as non-leaf.
                            newHeadSeg = SparseArraySegment<T>::template AllocateSegmentImpl<false>(recycler, 0, headDeleteLen, headDeleteLen, nullptr);
                        }
                        else
                        {TRACE_IT(57078);
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
                {TRACE_IT(57079);
                    // start should be in between left and left + length
                    if (startSeg->left  <= start && start < startSeg->left + startSeg->length)
                    {TRACE_IT(57080);
                        uint32 headDeleteLen = startSeg->left + startSeg->length - start;
                        if (startSeg->next)
                        {TRACE_IT(57081);
                            // We know the new segment will have a next segment, so allocate it as non-leaf.
                            newHeadSeg = SparseArraySegment<T>::template AllocateSegmentImpl<false>(recycler, 0, headDeleteLen, headDeleteLen, nullptr);
                        }
                        else
                        {TRACE_IT(57082);
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
                    {TRACE_IT(57083);
                        Assert(startSeg->size != 0);
                        *prevSeg = startSeg->next;
                        startSeg = SparseArraySegment<T>::From(startSeg->next);
                    }
                }
                // Step (2) proper
                SparseArraySegmentBase *temp = nullptr;
                while (startSeg && (startSeg->left + startSeg->length) <= limit)
                {TRACE_IT(57084);
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
                {TRACE_IT(57085);
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
                {TRACE_IT(57086);
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
                {TRACE_IT(57087);
                    Assert(start + insertLen == 0);
                    // Remove the dummy head node to preserve array consistency.
                    pArr->head = startSeg;
                    savePrev = nullptr;
                    prevSeg = &pArr->head;
                }

                while (startSeg)
                {TRACE_IT(57088);
                    startSeg->left = startSeg->left - deleteLen + insertLen ;
                    if (startSeg->next == nullptr)
                    {TRACE_IT(57089);
                        startSeg->EnsureSizeInBound();
                    }
                    startSeg = SparseArraySegment<T>::From(startSeg->next);
                }
            }
        }

        // The size of pnewArr head allocated in above step 1 might exceed next.left concatenated in step 2/3.
        pnewArr->head->EnsureSizeInBound();
        if (savePrev)
        {TRACE_IT(57090);
            savePrev->EnsureSizeInBound();
        }

        // insert elements
        if (insertLen > 0)
        {TRACE_IT(57091);
            Assert(!JavascriptNativeIntArray::Is(pArr) && !JavascriptNativeFloatArray::Is(pArr));

            // InsertPhase
            SparseArraySegment<T> *segInsert = nullptr;

            // see if we are just about the right of the previous segment
            Assert(!savePrev || savePrev->left <= start);
            if (savePrev && (start - savePrev->left < savePrev->size))
            {TRACE_IT(57092);
                segInsert = (SparseArraySegment<T>*)savePrev;
                uint32 spaceLeft = segInsert->size - (start - segInsert->left);
                if(spaceLeft < insertLen)
                {TRACE_IT(57093);
                    if (!segInsert->next)
                    {TRACE_IT(57094);
                        segInsert = segInsert->GrowByMin(recycler, insertLen - spaceLeft);
                    }
                    else
                    {TRACE_IT(57095);
                        segInsert = segInsert->GrowByMinMax(recycler, insertLen - spaceLeft, segInsert->next->left - segInsert->left - segInsert->size);
                    }
                }
                *prevPrevSeg = segInsert;
                segInsert->length = start + insertLen - segInsert->left;
            }
            else
            {TRACE_IT(57096);
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
    {TRACE_IT(57097);
        JavascriptArray *pnewArr = nullptr;

        if (pNewObj == nullptr)
        {TRACE_IT(57098);
            pNewObj = ArraySpeciesCreate(pObj, deleteLen, scriptContext);
            if (pNewObj == nullptr || !JavascriptArray::Is(pNewObj))
            {TRACE_IT(57099);
                pnewArr = scriptContext->GetLibrary()->CreateArray(deleteLen);
                pnewArr->EnsureHead<Var>();

                pNewObj = pnewArr;
            }
        }

        if (JavascriptArray::Is(pNewObj))
        {TRACE_IT(57100);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(pNewObj);
#endif
            pnewArr = JavascriptArray::FromVar(pNewObj);
        }

        // copy elements to delete to new array
        if (deleteLen > 0)
        {TRACE_IT(57101);
            for (uint32 i = 0; i < deleteLen; i++)
            {TRACE_IT(57102);
               if (JavascriptOperators::HasItem(pObj, start+i))
               {TRACE_IT(57103);
                   Var element = JavascriptOperators::GetItem(pObj, start + i, scriptContext);
                   if (pnewArr)
                   {TRACE_IT(57104);
                       pnewArr->SetItem(i, element, PropertyOperation_None);
                   }
                   else
                   {TRACE_IT(57105);
                       ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(pNewObj, i, element), scriptContext, i);
                   }
               }
            }
        }

        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.splice"));

        // If the return object is not an array, we'll need to set the 'length' property
        if (pnewArr == nullptr)
        {TRACE_IT(57106);
            h.ThrowTypeErrorOnFailure(JavascriptOperators::SetProperty(pNewObj, pNewObj, PropertyIds::length, JavascriptNumber::ToVar(deleteLen, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible));
        }

        // Now we need reserve room if it is necessary
        if (insertLen > deleteLen) // Might overflow max array length
        {
            // Unshift [start + deleteLen, len) to start + insertLen
            Unshift<indexT>(pObj, start + insertLen, start + deleteLen, len, scriptContext);
        }
        else if (insertLen < deleteLen) // Won't overflow max array length
        {TRACE_IT(57107);
            uint32 j = 0;
            for (uint32 i = start + deleteLen; i < len; i++)
            {TRACE_IT(57108);
                if (JavascriptOperators::HasItem(pObj, i))
                {TRACE_IT(57109);
                    Var element = JavascriptOperators::GetItem(pObj, i, scriptContext);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::SetItem(pObj, pObj, start + insertLen + j, element, scriptContext, PropertyOperation_ThrowIfNotExtensible));
                }
                else
                {TRACE_IT(57110);
                    h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(pObj, start + insertLen + j, PropertyOperation_ThrowOnDeleteIfNotConfig));
                }
                j++;
            }

            // Clean up the rest
            for (uint32 i = len; i > len - deleteLen + insertLen; i--)
            {TRACE_IT(57111);
                h.ThrowTypeErrorOnFailure(JavascriptOperators::DeleteItem(pObj, i - 1, PropertyOperation_ThrowOnDeleteIfNotConfig));
            }
        }

        if (insertLen > 0)
        {TRACE_IT(57112);
            indexT dstIndex = start; // insert index might overflow max array length
            for (uint i = 0; i < insertLen; i++)
            {TRACE_IT(57113);
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
        {TRACE_IT(57114);
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
        {TRACE_IT(57115);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Array.prototype.toLocaleString"));
        }

        if (JavascriptArray::IsDirectAccessArray(args[0]))
        {TRACE_IT(57116);
            JavascriptArray* arr = JavascriptArray::FromVar(args[0]);
            return ToLocaleString(arr, scriptContext);
        }
        else
        {TRACE_IT(57117);
            if (TypedArrayBase::IsDetachedTypedArray(args[0]))
            {TRACE_IT(57118);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray, _u("Array.prototype.toLocalString"));
            }

            RecyclableObject* obj = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57119);
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
    {TRACE_IT(57120);
        typedef IndexTrace<T> index_trace;

        ThrowTypeErrorOnFailureHelper h(scriptContext, _u("Array.prototype.unshift"));
        if (start < end)
        {TRACE_IT(57121);
            T newEnd = (end - start - 1);// newEnd - 1
            T dst = toIndex + newEnd;
            uint32 i = 0;
            if (end > UINT32_MAX)
            {TRACE_IT(57122);
                uint64 i64 = end;
                for (; i64 > UINT32_MAX; i64--)
                {TRACE_IT(57123);
                    if (JavascriptOperators::HasItem(obj, i64 - 1))
                    {TRACE_IT(57124);
                        Var element = JavascriptOperators::GetItem(obj, i64 - 1, scriptContext);
                        h.ThrowTypeErrorOnFailure(index_trace::SetItem(obj, dst, element, PropertyOperation_ThrowIfNotExtensible));
                    }
                    else
                    {TRACE_IT(57125);
                        h.ThrowTypeErrorOnFailure(index_trace::DeleteItem(obj, dst, PropertyOperation_ThrowOnDeleteIfNotConfig));
                    }

                    --dst;
                }
                i = UINT32_MAX;
            }
            else
            {TRACE_IT(57126);
                i = (uint32) end;
            }
            for (; i > start; i--)
            {TRACE_IT(57127);
                if (JavascriptOperators::HasItem(obj, i-1))
                {TRACE_IT(57128);
                    Var element = JavascriptOperators::GetItem(obj, i - 1, scriptContext);
                    h.ThrowTypeErrorOnFailure(index_trace::SetItem(obj, dst, element, PropertyOperation_ThrowIfNotExtensible));
                }
                else
                {TRACE_IT(57129);
                    h.ThrowTypeErrorOnFailure(index_trace::DeleteItem(obj, dst, PropertyOperation_ThrowOnDeleteIfNotConfig));
                }

                --dst;
            }
        }
    }

    template<typename T>
    void JavascriptArray::GrowArrayHeadHelperForUnshift(JavascriptArray* pArr, uint32 unshiftElements, ScriptContext * scriptContext)
    {TRACE_IT(57130);
        SparseArraySegmentBase* nextToHeadSeg = pArr->head->next;
        Recycler* recycler = scriptContext->GetRecycler();

        if (nextToHeadSeg == nullptr)
        {TRACE_IT(57131);
            pArr->EnsureHead<T>();
            pArr->head = SparseArraySegment<T>::From(pArr->head)->GrowByMin(recycler, unshiftElements);
        }
        else
        {TRACE_IT(57132);
            pArr->head = SparseArraySegment<T>::From(pArr->head)->GrowByMinMax(recycler, unshiftElements, ((nextToHeadSeg->left + unshiftElements) - pArr->head->left - pArr->head->size));
        }

    }

    template<typename T>
    void JavascriptArray::UnshiftHelper(JavascriptArray* pArr, uint32 unshiftElements, Js::Var * elements)
    {TRACE_IT(57133);
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
        {TRACE_IT(57134);
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
        {TRACE_IT(57135);
           return res;
        }
        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57136);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
            JavascriptArray * pArr = JavascriptArray::FromVar(args[0]);

            uint32 unshiftElements = args.Info.Count - 1;

            if (unshiftElements > 0)
            {TRACE_IT(57137);
                if (pArr->IsFillFromPrototypes())
                {TRACE_IT(57138);
                    pArr->FillFromPrototypes(0, pArr->length); // We need find all missing value from [[proto]] object
                }

                // Pre-process: truncate overflowing elements to properties
                bool newLenOverflowed = false;
                uint32 maxLen = MaxArrayLength - unshiftElements;
                if (pArr->length > maxLen)
                {TRACE_IT(57139);
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
                {TRACE_IT(57140);
                    isIntArray = true;
                }
                else if (JavascriptNativeFloatArray::Is(pArr))
                {TRACE_IT(57141);
                    isFloatArray = true;
                }

                // If we need to grow head segment and there is already a next segment, then allocate the new head segment upfront
                // If there is OOM in array allocation, then array consistency is maintained.
                if (pArr->head->size < pArr->head->length + unshiftElements)
                {TRACE_IT(57142);
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
                {TRACE_IT(57143);
                    renumberSeg->left += unshiftElements;
                    if (renumberSeg->next == nullptr)
                    {TRACE_IT(57144);
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
                {TRACE_IT(57145);
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
                }
            }
            res = JavascriptNumber::ToVar(pArr->length, scriptContext);
        }
        else
        {TRACE_IT(57146);
            RecyclableObject* dynamicObject = nullptr;
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
            {TRACE_IT(57147);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.unshift"));
            }

            BigIndex length;
            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {TRACE_IT(57148);
                length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
            }
            else
            {TRACE_IT(57149);
                length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
            }
            uint32 unshiftElements = args.Info.Count - 1;
            if (unshiftElements > 0)
            {TRACE_IT(57150);
                uint32 MaxSpaceUint32 = MaxArrayLength - unshiftElements;
                // Note: end will always be a smallIndex either it is less than length in which case it is MaxSpaceUint32
                // or MaxSpaceUint32 is greater than length meaning length is a uint32 number
                BigIndex end = length > MaxSpaceUint32 ? MaxSpaceUint32 : length;
                if (end < length)
                {TRACE_IT(57151);
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
                {TRACE_IT(57152);
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
        {TRACE_IT(57153);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
        }

         // ES5 15.4.4.2: call join, or built-in Object.prototype.toString

        RecyclableObject* obj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &obj))
        {TRACE_IT(57154);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.toString"));
        }

        // In ES5 we could be calling a user defined join, even on array. We must [[Get]] join at runtime.
        Var join = JavascriptOperators::GetProperty(obj, PropertyIds::join, scriptContext);
        if (JavascriptConversion::IsCallable(join))
        {TRACE_IT(57155);
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
            {TRACE_IT(57156);
                // There was an implicit call and implicit calls are disabled. This would typically cause a bailout.
                Assert(threadContext->IsDisableImplicitCall());
                result = scriptContext->GetLibrary()->GetNull();
            }

            return result;
        }
        else
        {TRACE_IT(57157);
            // call built-in Object.prototype.toString
            return CALL_ENTRYPOINT(JavascriptObject::EntryToString, function, CallInfo(1), obj);
        }
    }

#if DEBUG
    BOOL JavascriptArray::GetIndex(const char16* propName, uint32 *pIndex)
    {TRACE_IT(57158);
        uint32 lu, luDig;

        int32 cch = (int32)wcslen(propName);
        char16* pch = const_cast<char16 *>(propName);

        lu = *pch - '0';
        if (lu > 9)
            return FALSE;

        if (0 == lu)
        {TRACE_IT(57159);
            *pIndex = 0;
            return 1 == cch;
        }

        while ((luDig = *++pch - '0') < 10)
        {TRACE_IT(57160);
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
        {TRACE_IT(57161);
            // 0xFFFFFFFF is not treated as an array index so that the length can be
            // capped at 32 bits.
            return FALSE;
        }

        *pIndex = lu;
        return TRUE;
    }
#endif

    JavascriptString* JavascriptArray::GetLocaleSeparator(ScriptContext* scriptContext)
    {TRACE_IT(57162);
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
        {TRACE_IT(57163);
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
    {TRACE_IT(57164);
        uint32 length = 0;
        if (TypedArrayBase::Is(arr))
        {TRACE_IT(57165);
            // For a TypedArray use the actual length of the array.
            length = TypedArrayBase::FromVar(arr)->GetLength();
        }
        else
        {TRACE_IT(57166);
            //For anything else, use the "length" property if present.
            length = ItemTrace<T>::GetLength(arr, scriptContext);
        }

        if (length == 0 || scriptContext->CheckObject(arr))
        {TRACE_IT(57167);
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
            {TRACE_IT(57168);
                res = JavascriptArray::ToLocaleStringHelper(element, scriptContext);
            }

            if (length > 1)
            {TRACE_IT(57169);
                JavascriptString* separator = GetLocaleSeparator(scriptContext);

                for (uint32 i = 1; i < length; i++)
                {TRACE_IT(57170);
                    res = JavascriptString::Concat(res, separator);
                    if (ItemTrace<T>::GetItem(arr, i, &element, scriptContext))
                    {TRACE_IT(57171);
                        res = JavascriptString::Concat(res, JavascriptArray::ToLocaleStringHelper(element, scriptContext));
                    }
                }
            }
        },
        [&](bool/*hasException*/)
        {TRACE_IT(57172);
            if (pushedObject)
            {TRACE_IT(57173);
                Var top = scriptContext->PopObject();
                AssertMsg(top == arr, "Unmatched operation stack");
            }
        });

        if (res == nullptr)
        {TRACE_IT(57174);
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
        {TRACE_IT(57175);
            return scriptContext->GetLibrary()->GetFalse();
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[1]);
#endif
        if (JavascriptOperators::IsArray(args[1]))
        {TRACE_IT(57176);
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
        {TRACE_IT(57177);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.find"));
        }

        int64 length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57178);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
            length = pArr->length;
        }
        else
        {TRACE_IT(57179);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57180);
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
    {TRACE_IT(57181);
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(57182);
            // typedArrayBase is only non-null if and only if we came here via the TypedArray entrypoint
            if (typedArrayBase != nullptr)
            {TRACE_IT(57183);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, findIndex ? _u("[TypedArray].prototype.findIndex") : _u("[TypedArray].prototype.find"));
            }
            else
            {TRACE_IT(57184);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, findIndex ? _u("Array.prototype.findIndex") : _u("Array.prototype.find"));
            }
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg;

        if (args.Info.Count > 2)
        {TRACE_IT(57185);
            thisArg = args[2];
        }
        else
        {TRACE_IT(57186);
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If we came from Array.prototype.find/findIndex and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(57187);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        if (pArr)
        {TRACE_IT(57188);
            Var undefined = scriptContext->GetLibrary()->GetUndefined();

            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57189);
                element = undefined;
                pArr->DirectGetItemAtFull(k, &element);

                Var index = JavascriptNumber::ToVar(k, scriptContext);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    index,
                    pArr);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {TRACE_IT(57190);
                    return findIndex ? index : element;
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {TRACE_IT(57191);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::FindObjectHelper<findIndex>(obj, length, k + 1, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {TRACE_IT(57192);
            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57193);
                element = typedArrayBase->DirectGetItem(k);

                Var index = JavascriptNumber::ToVar(k, scriptContext);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    index,
                    typedArrayBase);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {TRACE_IT(57194);
                    return findIndex ? index : element;
                }
            }
        }
        else
        {TRACE_IT(57195);
            return JavascriptArray::FindObjectHelper<findIndex>(obj, length, 0u, callBackFn, thisArg, scriptContext);
        }

        return findIndex ? JavascriptNumber::ToVar(-1, scriptContext) : scriptContext->GetLibrary()->GetUndefined();
    }

    template <bool findIndex>
    Var JavascriptArray::FindObjectHelper(RecyclableObject* obj, int64 length, int64 start, RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {TRACE_IT(57196);
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        for (int64 k = start; k < length; k++)
        {TRACE_IT(57197);
            element = JavascriptOperators::GetItem(obj, (uint64)k, scriptContext);
            Var index = JavascriptNumber::ToVar(k, scriptContext);

            testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                element,
                index,
                obj);

            if (JavascriptConversion::ToBoolean(testResult, scriptContext))
            {TRACE_IT(57198);
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
        {TRACE_IT(57199);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.findIndex"));
        }

        int64 length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57200);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
            length = pArr->length;
        }
        else
        {TRACE_IT(57201);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57202);
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
        {TRACE_IT(57203);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.entries"));
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {TRACE_IT(57204);
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
        {TRACE_IT(57205);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.keys"));
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {TRACE_IT(57206);
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
        {TRACE_IT(57207);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.values"));
        }

        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {TRACE_IT(57208);
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
        {TRACE_IT(57209);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.every"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57210);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {TRACE_IT(57211);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57212);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.every"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(57213);
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }
        else
        {TRACE_IT(57214);
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {TRACE_IT(57215);
            return JavascriptArray::EveryHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max()); // if pArr is not null lets make sure length is safe to cast, which will only happen if length is a uint32max
        return JavascriptArray::EveryHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.every as described by ES6.0 (draft 22) Section 22.1.3.5
    template <typename T>
    Var JavascriptArray::EveryHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(57216);
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(57217);
            // typedArrayBase is only non-null if and only if we came here via the TypedArray entrypoint
            if (typedArrayBase != nullptr)
            {TRACE_IT(57218);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.every"));
            }
            else
            {TRACE_IT(57219);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.every"));
            }
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg = nullptr;


        if (args.Info.Count > 2)
        {TRACE_IT(57220);
            thisArg = args[2];
        }
        else
        {TRACE_IT(57221);
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If we came from Array.prototype.map and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(57222);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        Var element = nullptr;
        Var testResult = nullptr;
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;

        if (pArr)
        {TRACE_IT(57223);
            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57224);
                if (!pArr->DirectGetItemAtFull(k, &element))
                {TRACE_IT(57225);
                    continue;
                }

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                if (!JavascriptConversion::ToBoolean(testResult, scriptContext))
                {TRACE_IT(57226);
                    return scriptContext->GetLibrary()->GetFalse();
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {TRACE_IT(57227);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::EveryObjectHelper<T>(obj, length, k + 1, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {TRACE_IT(57228);
            Assert(length <= UINT_MAX);

            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57229);
                if (!typedArrayBase->HasItem(k))
                {TRACE_IT(57230);
                    continue;
                }

                element = typedArrayBase->DirectGetItem(k);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    typedArrayBase);

                if (!JavascriptConversion::ToBoolean(testResult, scriptContext))
                {TRACE_IT(57231);
                    return scriptContext->GetLibrary()->GetFalse();
                }
            }
        }
        else
        {TRACE_IT(57232);
            return JavascriptArray::EveryObjectHelper<T>(obj, length, 0u, callBackFn, thisArg, scriptContext);
        }

        return scriptContext->GetLibrary()->GetTrue();
    }

    template <typename T>
    Var JavascriptArray::EveryObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {TRACE_IT(57233);
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        for (T k = start; k < length; k++)
        {TRACE_IT(57234);
            // According to es6 spec, we need to call Has first before calling Get
            if (JavascriptOperators::HasItem(obj, k))
            {TRACE_IT(57235);
                element = JavascriptOperators::GetItem(obj, k, scriptContext);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                if (!JavascriptConversion::ToBoolean(testResult, scriptContext))
                {TRACE_IT(57236);
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
        {TRACE_IT(57237);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.some"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57238);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {TRACE_IT(57239);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57240);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.some"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(57241);
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);

        }
        else
        {TRACE_IT(57242);
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {TRACE_IT(57243);
            return JavascriptArray::SomeHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max()); // if pArr is not null lets make sure length is safe to cast, which will only happen if length is a uint32max
        return JavascriptArray::SomeHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.some as described in ES6.0 (draft 22) Section 22.1.3.23
    template <typename T>
    Var JavascriptArray::SomeHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(57244);
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(57245);
            // We are in the TypedArray version of this API if and only if typedArrayBase != nullptr
            if (typedArrayBase != nullptr)
            {TRACE_IT(57246);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.some"));
            }
            else
            {TRACE_IT(57247);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.some"));
            }
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg = nullptr;

        if (args.Info.Count > 2)
        {TRACE_IT(57248);
            thisArg = args[2];
        }
        else
        {TRACE_IT(57249);
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If we came from Array.prototype.some and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(57250);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        if (pArr)
        {TRACE_IT(57251);
            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57252);
                if (!pArr->DirectGetItemAtFull(k, &element))
                {TRACE_IT(57253);
                    continue;
                }

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {TRACE_IT(57254);
                    return scriptContext->GetLibrary()->GetTrue();
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {TRACE_IT(57255);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::SomeObjectHelper<T>(obj, length, k + 1, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {TRACE_IT(57256);
            Assert(length <= UINT_MAX);

            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57257);
                // If k < typedArrayBase->length, we know that HasItem will return true.
                // But we still have to call it in case there's a proxy trap or in the case that we are calling
                // Array.prototype.some with a TypedArray that has a different length instance property.
                if (!typedArrayBase->HasItem(k))
                {TRACE_IT(57258);
                    continue;
                }

                element = typedArrayBase->DirectGetItem(k);

                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    typedArrayBase);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {TRACE_IT(57259);
                    return scriptContext->GetLibrary()->GetTrue();
                }
            }
        }
        else
        {TRACE_IT(57260);
            return JavascriptArray::SomeObjectHelper<T>(obj, length, 0u, callBackFn, thisArg, scriptContext);
        }

        return scriptContext->GetLibrary()->GetFalse();
    }

    template <typename T>
    Var JavascriptArray::SomeObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {TRACE_IT(57261);
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        Var testResult = nullptr;

        for (T k = start; k < length; k++)
        {TRACE_IT(57262);
            if (JavascriptOperators::HasItem(obj, k))
            {TRACE_IT(57263);
                element = JavascriptOperators::GetItem(obj, k, scriptContext);
                testResult = CALL_FUNCTION(callBackFn, CallInfo(flags, 4), thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                if (JavascriptConversion::ToBoolean(testResult, scriptContext))
                {TRACE_IT(57264);
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
        {TRACE_IT(57265);
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
        {TRACE_IT(57266);
            pArr = JavascriptArray::FromVar(args[0]);
            dynamicObject = pArr;
        }
        else
        {TRACE_IT(57267);
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
            {TRACE_IT(57268);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.forEach"));
            }

            if (JavascriptArray::Is(dynamicObject) && scriptContext == JavascriptArray::FromVar(dynamicObject)->GetScriptContext())
            {TRACE_IT(57269);
                pArr = JavascriptArray::FromVar(dynamicObject);
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(57270);
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }
        else
        {TRACE_IT(57271);
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(57272);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.forEach"));
        }
        callBackFn = RecyclableObject::FromVar(args[1]);

        if (args.Info.Count > 2)
        {TRACE_IT(57273);
            thisArg = args[2];
        }
        else
        {TRACE_IT(57274);
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
        {TRACE_IT(57275);
            Assert(pArr == dynamicObject);
            pArr->ForEachItemInRange<true>(0, length.IsUint32Max() ? MaxArrayLength : length.GetSmallIndex(), scriptContext, fn32);
        }
        else
        {TRACE_IT(57276);
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
        {TRACE_IT(57277);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(args[0]);
#endif
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;

            length = pArr->length;
        }
        else
        {TRACE_IT(57278);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57279);
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
    {TRACE_IT(57280);
        Assert(args.Info.Count > 0);

        JavascriptLibrary* library = scriptContext->GetLibrary();
        int64 fromVal = 0;
        int64 toVal = 0;
        int64 finalVal = length;

        // If we came from Array.prototype.copyWithin and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(57281);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        if (args.Info.Count > 1)
        {TRACE_IT(57282);
            toVal = JavascriptArray::GetIndexFromVar(args[1], length, scriptContext);

            if (args.Info.Count > 2)
            {TRACE_IT(57283);
                fromVal = JavascriptArray::GetIndexFromVar(args[2], length, scriptContext);

                if (args.Info.Count > 3 && args[3] != library->GetUndefined())
                {TRACE_IT(57284);
                    finalVal = JavascriptArray::GetIndexFromVar(args[3], length, scriptContext);
                }
            }
        }

        // If count would be negative or zero, we won't do anything so go ahead and return early.
        if (finalVal <= fromVal || length <= toVal)
        {TRACE_IT(57285);
            return obj;
        }

        // Make sure we won't underflow during the count calculation
        Assert(finalVal > fromVal && length > toVal);

        int64 count = min(finalVal - fromVal, length - toVal);

        // We shouldn't have made it here if the count was going to be zero
        Assert(count > 0);

        int direction;

        if (fromVal < toVal && toVal < (fromVal + count))
        {TRACE_IT(57286);
            direction = -1;
            fromVal += count - 1;
            toVal += count - 1;
        }
        else
        {TRACE_IT(57287);
            direction = 1;
        }

        // Side effects (such as defining a property in a ToPrimitive call) during evaluation of arguments may convert the array to an ES5 array.
        if (pArr && !JavascriptArray::Is(obj))
        {TRACE_IT(57288);
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        // If we are going to copy elements from or to indices > 2^32-1 we'll execute this (slightly slower path)
        // It's possible to optimize here so that we use the normal code below except for the > 2^32-1 indices
        if ((direction == -1 && (fromVal >= MaxArrayLength || toVal >= MaxArrayLength))
            || (((fromVal + count) > MaxArrayLength) || ((toVal + count) > MaxArrayLength)))
        {TRACE_IT(57289);
            while (count > 0)
            {TRACE_IT(57290);
                Var index = JavascriptNumber::ToVar(fromVal, scriptContext);

                if (JavascriptOperators::OP_HasItem(obj, index, scriptContext))
                {TRACE_IT(57291);
                    Var val = JavascriptOperators::OP_GetElementI(obj, index, scriptContext);

                    JavascriptOperators::OP_SetElementI(obj, JavascriptNumber::ToVar(toVal, scriptContext), val, scriptContext, PropertyOperation_ThrowIfNotExtensible);
                }
                else
                {TRACE_IT(57292);
                    JavascriptOperators::OP_DeleteElementI(obj, JavascriptNumber::ToVar(toVal, scriptContext), scriptContext, PropertyOperation_ThrowOnDeleteIfNotConfig);
                }

                fromVal += direction;
                toVal += direction;
                count--;
            }
        }
        else
        {TRACE_IT(57293);
            Assert(fromVal < MaxArrayLength);
            Assert(toVal < MaxArrayLength);
            Assert(direction == -1 || (fromVal + count < MaxArrayLength && toVal + count < MaxArrayLength));

            uint32 fromIndex = static_cast<uint32>(fromVal);
            uint32 toIndex = static_cast<uint32>(toVal);

            while (count > 0)
            {TRACE_IT(57294);
                if (obj->HasItem(fromIndex))
                {TRACE_IT(57295);
                    if (typedArrayBase)
                    {TRACE_IT(57296);
                        Var val = typedArrayBase->DirectGetItem(fromIndex);

                        typedArrayBase->DirectSetItem(toIndex, val);
                    }
                    else if (pArr)
                    {TRACE_IT(57297);
                        Var val = pArr->DirectGetItem(fromIndex);

                        pArr->SetItem(toIndex, val, Js::PropertyOperation_ThrowIfNotExtensible);

                        if (!JavascriptArray::Is(obj))
                        {TRACE_IT(57298);
                            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                            pArr = nullptr;
                        }
                    }
                    else
                    {TRACE_IT(57299);
                        Var val = JavascriptOperators::OP_GetElementI_UInt32(obj, fromIndex, scriptContext);

                        JavascriptOperators::OP_SetElementI_UInt32(obj, toIndex, val, scriptContext, PropertyOperation_ThrowIfNotExtensible);
                    }
                }
                else
                {TRACE_IT(57300);
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
        {TRACE_IT(57301);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;

            length = pArr->length;
        }
        else
        {TRACE_IT(57302);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57303);
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
    {TRACE_IT(57304);
        Assert(args.Info.Count > 0);

        JavascriptLibrary* library = scriptContext->GetLibrary();

        // If we came from Array.prototype.fill and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(57305);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        Var fillValue;

        if (args.Info.Count > 1)
        {TRACE_IT(57306);
            fillValue = args[1];
        }
        else
        {TRACE_IT(57307);
            fillValue = library->GetUndefined();
        }

        int64 k = 0;
        int64 finalVal = length;

        if (args.Info.Count > 2)
        {TRACE_IT(57308);
            k = JavascriptArray::GetIndexFromVar(args[2], length, scriptContext);

            if (args.Info.Count > 3 && !JavascriptOperators::IsUndefinedObject(args[3]))
            {TRACE_IT(57309);
                finalVal = JavascriptArray::GetIndexFromVar(args[3], length, scriptContext);
            }

            // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
            // we will process the array elements like an ES5Array.
            if (pArr && !JavascriptArray::Is(obj))
            {TRACE_IT(57310);
                AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                pArr = nullptr;
            }
        }

        if (k < MaxArrayLength)
        {TRACE_IT(57311);
            int64 end = min<int64>(finalVal, MaxArrayLength);
            uint32 u32k = static_cast<uint32>(k);

            while (u32k < end)
            {TRACE_IT(57312);
                if (typedArrayBase)
                {TRACE_IT(57313);
                    typedArrayBase->DirectSetItem(u32k, fillValue);
                }
                else if (pArr)
                {TRACE_IT(57314);
                    pArr->SetItem(u32k, fillValue, PropertyOperation_ThrowIfNotExtensible);
                }
                else
                {TRACE_IT(57315);
                    JavascriptOperators::OP_SetElementI_UInt32(obj, u32k, fillValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                }

                u32k++;
            }

            BigIndex dstIndex = MaxArrayLength;

            for (int64 i = end; i < finalVal; ++i)
            {TRACE_IT(57316);
                if (pArr)
                {TRACE_IT(57317);
                    pArr->DirectSetItemAt(dstIndex, fillValue);
                    ++dstIndex;
                }
                else
                {TRACE_IT(57318);
                    JavascriptOperators::OP_SetElementI(obj, JavascriptNumber::ToVar(i, scriptContext), fillValue, scriptContext, Js::PropertyOperation_ThrowIfNotExtensible);
                }
            }
        }
        else
        {TRACE_IT(57319);
            BigIndex dstIndex = static_cast<uint64>(k);

            for (int64 i = k; i < finalVal; i++)
            {TRACE_IT(57320);
                if (pArr)
                {TRACE_IT(57321);
                    pArr->DirectSetItemAt(dstIndex, fillValue);
                    ++dstIndex;
                }
                else
                {TRACE_IT(57322);
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
        {TRACE_IT(57323);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.map"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57324);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {TRACE_IT(57325);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57326);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.map"));
            }
        }

        length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);

        if (length.IsSmallIndex())
        {TRACE_IT(57327);
            return JavascriptArray::MapHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        Assert(pArr == nullptr || length.IsUint32Max()); // if pArr is not null lets make sure length is safe to cast, which will only happen if length is a uint32max
        return JavascriptArray::MapHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }


    template<typename T>
    Var JavascriptArray::MapHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(57328);
        RecyclableObject* newObj = nullptr;
        JavascriptArray* newArr = nullptr;
        bool isTypedArrayEntryPoint = typedArrayBase != nullptr;
        bool isBuiltinArrayCtor = true;

        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(57329);
            if (isTypedArrayEntryPoint)
            {TRACE_IT(57330);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.map"));
            }
            else
            {TRACE_IT(57331);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.map"));
            }
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg;

        if (args.Info.Count > 2)
        {TRACE_IT(57332);
            thisArg = args[2];
        }
        else
        {TRACE_IT(57333);
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If we came from Array.prototype.map and source object is not a JavascriptArray, source could be a TypedArray
        if (!isTypedArrayEntryPoint && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(57334);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        // If the entry point is %TypedArray%.prototype.map or the source object is an Array exotic object we should try to load the constructor property
        // and use it to construct the return object.
        if (isTypedArrayEntryPoint)
        {TRACE_IT(57335);
            Var constructor = JavascriptOperators::SpeciesConstructor(
                typedArrayBase, TypedArrayBase::GetDefaultConstructor(args[0], scriptContext), scriptContext);
            isBuiltinArrayCtor = (constructor == scriptContext->GetLibrary()->GetArrayConstructor());

            if (JavascriptOperators::IsConstructor(constructor))
            {TRACE_IT(57336);
                Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(length, scriptContext) };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = RecyclableObject::FromVar(TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), (uint32)length, scriptContext));
            }
            else if (isTypedArrayEntryPoint)
            {TRACE_IT(57337);
                // We only need to throw a TypeError when the constructor property is not an actual constructor if %TypedArray%.prototype.map was called
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NotAConstructor, _u("[TypedArray].prototype.map"));
            }
        }
        // skip the typed array and "pure" array case, we still need to handle special arrays like es5array, remote array, and proxy of array.
        else if (pArr == nullptr || scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {TRACE_IT(57338);
            newObj = ArraySpeciesCreate(obj, length, scriptContext, nullptr, nullptr, &isBuiltinArrayCtor);
        }

        if (newObj == nullptr)
        {TRACE_IT(57339);
            if (length > UINT_MAX)
            {TRACE_IT(57340);
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
            }
            newArr = scriptContext->GetLibrary()->CreateArray(static_cast<uint32>(length));
            newArr->EnsureHead<Var>();
            newObj = newArr;
        }
        else
        {TRACE_IT(57341);
            // If the new object we created is an array, remember that as it will save us time setting properties in the object below
            if (JavascriptArray::Is(newObj))
            {TRACE_IT(57342);
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
        {TRACE_IT(57343);
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        if (pArr != nullptr)
        {TRACE_IT(57344);
            // If source is a JavascriptArray, newObj may or may not be an array based on what was in source's constructor property
            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57345);
                if (!pArr->DirectGetItemAtFull(k, &element))
                {TRACE_IT(57346);
                    continue;
                }

                mappedValue = CALL_FUNCTION(callBackFn, callBackFnInfo, thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                // If newArr is a valid pointer, then we constructed an array to return. Otherwise we need to do generic object operations
                if (newArr && isBuiltinArrayCtor)
                {TRACE_IT(57347);
                    newArr->DirectSetItemAt(k, mappedValue);
                }
                else
                {TRACE_IT(57348);
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, k, mappedValue), scriptContext, k);
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {TRACE_IT(57349);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::MapObjectHelper<T>(obj, length, k + 1, newObj, newArr, isBuiltinArrayCtor, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else if (typedArrayBase != nullptr)
        {TRACE_IT(57350);
            // Source is a TypedArray, we may have tried to call a constructor, but newObj may not be a TypedArray (or an array either)
            TypedArrayBase* newTypedArray = nullptr;

            if (TypedArrayBase::Is(newObj))
            {TRACE_IT(57351);
                newTypedArray = TypedArrayBase::FromVar(newObj);
            }

            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57352);
                // We can't rely on the length value being equal to typedArrayBase->GetLength() because user code may lie and
                // attach any length property to a TypedArray instance and pass it as this parameter when .calling
                // Array.prototype.map.
                if (!typedArrayBase->HasItem(k))
                {TRACE_IT(57353);
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
                {TRACE_IT(57354);
                    newTypedArray->DirectSetItem(k, mappedValue);
                }
                else if (newArr)
                {TRACE_IT(57355);
                    newArr->DirectSetItemAt(k, mappedValue);
                }
                else
                {TRACE_IT(57356);
                    JavascriptArray::SetArrayLikeObjects(newObj, k, mappedValue);
                }
            }
        }
        else
        {TRACE_IT(57357);
            return JavascriptArray::MapObjectHelper<T>(obj, length, 0u, newObj, newArr, isBuiltinArrayCtor, callBackFn, thisArg, scriptContext);
        }

#ifdef VALIDATE_ARRAY
        if (JavascriptArray::Is(newObj))
        {TRACE_IT(57358);
            newArr->ValidateArray();
        }
#endif

        return newObj;
    }

    template<typename T>
    Var JavascriptArray::MapObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* newObj, JavascriptArray* newArr,
        bool isBuiltinArrayCtor, RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {TRACE_IT(57359);
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags callBackFnflags = CallFlags_Value;
        CallInfo callBackFnInfo = CallInfo(callBackFnflags, 4);
        Var element = nullptr;
        Var mappedValue = nullptr;

        for (T k = start; k < length; k++)
        {TRACE_IT(57360);
            if (JavascriptOperators::HasItem(obj, k))
            {TRACE_IT(57361);
                element = JavascriptOperators::GetItem(obj, k, scriptContext);
                mappedValue = CALL_FUNCTION(callBackFn, callBackFnInfo, thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                if (newArr && isBuiltinArrayCtor)
                {TRACE_IT(57362);
                    newArr->SetItem((uint32)k, mappedValue, PropertyOperation_None);
                }
                else
                {TRACE_IT(57363);
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, BigIndex(k), mappedValue), scriptContext, BigIndex(k));
                }
            }
        }

#ifdef VALIDATE_ARRAY
        if (JavascriptArray::Is(newObj))
        {TRACE_IT(57364);
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
        {TRACE_IT(57365);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.filter"));
        }

        BigIndex length;
        JavascriptArray* pArr = nullptr;
        RecyclableObject* dynamicObject = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57366);
            pArr = JavascriptArray::FromVar(args[0]);
            dynamicObject = pArr;
        }
        else
        {TRACE_IT(57367);
            if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &dynamicObject))
            {TRACE_IT(57368);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.filter"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(57369);
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }
        else
        {TRACE_IT(57370);
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(dynamicObject, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {TRACE_IT(57371);
            return JavascriptArray::FilterHelper(pArr, dynamicObject, length.GetSmallIndex(), args, scriptContext);
        }
        return JavascriptArray::FilterHelper(pArr, dynamicObject, length.GetBigIndex(), args, scriptContext);
    }

    template <typename T>
    Var JavascriptArray::FilterHelper(JavascriptArray* pArr, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(57372);
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(57373);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.filter"));
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var thisArg = nullptr;

        if (args.Info.Count > 2)
        {TRACE_IT(57374);
            thisArg = args[2];
        }
        else
        {TRACE_IT(57375);
            thisArg = scriptContext->GetLibrary()->GetUndefined();
        }

        // If the source object is an Array exotic object we should try to load the constructor property and use it to construct the return object.
        bool isBuiltinArrayCtor = true;
        RecyclableObject* newObj = ArraySpeciesCreate(obj, 0, scriptContext, nullptr, nullptr, &isBuiltinArrayCtor);
        JavascriptArray* newArr = nullptr;

        if (newObj == nullptr)
        {TRACE_IT(57376);
            newArr = scriptContext->GetLibrary()->CreateArray(0);
            newArr->EnsureHead<Var>();
            newObj = newArr;
        }
        else
        {TRACE_IT(57377);
            // If the new object we created is an array, remember that as it will save us time setting properties in the object below
            if (JavascriptArray::Is(newObj))
            {TRACE_IT(57378);
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                newArr = JavascriptArray::FromVar(newObj);
            }
        }

        // The ArraySpeciesCreate call above could have converted the source array into an ES5Array. If this happens
        // we will process the array elements like an ES5Array.
        if (pArr && !JavascriptArray::Is(obj))
        {TRACE_IT(57379);
            AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
            pArr = nullptr;
        }

        Var element = nullptr;
        Var selected = nullptr;

        if (pArr)
        {TRACE_IT(57380);
            Assert(length <= MaxArrayLength);
            uint32 i = 0;

            Assert(length <= UINT_MAX);
            for (uint32 k = 0; k < (uint32)length; k++)
            {TRACE_IT(57381);
                if (!pArr->DirectGetItemAtFull(k, &element))
                {TRACE_IT(57382);
                    continue;
                }

                selected = CALL_ENTRYPOINT(callBackFn->GetEntryPoint(), callBackFn, CallInfo(CallFlags_Value, 4),
                    thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    pArr);

                if (JavascriptConversion::ToBoolean(selected, scriptContext))
                {TRACE_IT(57383);
                    // Try to fast path if the return object is an array
                    if (newArr && isBuiltinArrayCtor)
                    {TRACE_IT(57384);
                        newArr->DirectSetItemAt(i, element);
                    }
                    else
                    {TRACE_IT(57385);
                        ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, i, element), scriptContext, i);
                    }
                    ++i;
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the rest of the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {TRACE_IT(57386);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::FilterObjectHelper<T>(obj, length, k + 1, newArr, newObj, i, callBackFn, thisArg, scriptContext);
                }
            }
        }
        else
        {TRACE_IT(57387);
            return JavascriptArray::FilterObjectHelper<T>(obj, length, 0u, newArr, newObj, 0u, callBackFn, thisArg, scriptContext);
        }

#ifdef VALIDATE_ARRAY
        if (newArr)
        {TRACE_IT(57388);
            newArr->ValidateArray();
        }
#endif

        return newObj;
    }

    template <typename T>
    Var JavascriptArray::FilterObjectHelper(RecyclableObject* obj, T length, T start, JavascriptArray* newArr, RecyclableObject* newObj, T newStart,
        RecyclableObject* callBackFn, Var thisArg, ScriptContext* scriptContext)
    {TRACE_IT(57389);
        Var element = nullptr;
        Var selected = nullptr;
        BigIndex i = BigIndex(newStart);

        for (T k = start; k < length; k++)
        {TRACE_IT(57390);
            if (JavascriptOperators::HasItem(obj, k))
            {TRACE_IT(57391);
                element = JavascriptOperators::GetItem(obj, k, scriptContext);
                selected = CALL_ENTRYPOINT(callBackFn->GetEntryPoint(), callBackFn, CallInfo(CallFlags_Value, 4),
                    thisArg,
                    element,
                    JavascriptNumber::ToVar(k, scriptContext),
                    obj);

                if (JavascriptConversion::ToBoolean(selected, scriptContext))
                {TRACE_IT(57392);
                    if (newArr)
                    {TRACE_IT(57393);
                        newArr->DirectSetItemAt(i, element);
                    }
                    else
                    {TRACE_IT(57394);
                        ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, i, element), scriptContext, i);
                    }

                    ++i;
                }
            }
        }

#ifdef VALIDATE_ARRAY
        if (newArr)
        {TRACE_IT(57395);
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
        {TRACE_IT(57396);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reduce"));
        }

        BigIndex length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57397);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;

            length = pArr->length;
        }
        else
        {TRACE_IT(57398);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57399);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reduce"));
            }

            if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
            {TRACE_IT(57400);
                length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
            }
            else
            {TRACE_IT(57401);
                length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
            }
        }
        if (length.IsSmallIndex())
        {TRACE_IT(57402);
            return JavascriptArray::ReduceHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        return JavascriptArray::ReduceHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.reduce as described in ES6.0 (draft 22) Section 22.1.3.18
    template <typename T>
    Var JavascriptArray::ReduceHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(57403);
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(57404);
            if (typedArrayBase != nullptr)
            {TRACE_IT(57405);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.reduce"));
            }
            else
            {TRACE_IT(57406);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.reduce"));
            }
        }

        // If we came from Array.prototype.reduce and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(57407);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        T k = 0;
        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var accumulator = nullptr;
        Var element = nullptr;

        if (args.Info.Count > 2)
        {TRACE_IT(57408);
            accumulator = args[2];
        }
        else
        {TRACE_IT(57409);
            if (length == 0)
            {TRACE_IT(57410);
                JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
            }

            bool bPresent = false;

            if (pArr)
            {TRACE_IT(57411);
                for (; k < length && bPresent == false; k++)
                {TRACE_IT(57412);
                    if (!pArr->DirectGetItemAtFull((uint32)k, &element))
                    {TRACE_IT(57413);
                        continue;
                    }

                    bPresent = true;
                    accumulator = element;
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {TRACE_IT(57414);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    pArr = nullptr;
                }
            }
            else if (typedArrayBase)
            {TRACE_IT(57415);
                Assert(length <= UINT_MAX);

                for (; k < length && bPresent == false; k++)
                {TRACE_IT(57416);
                    if (!typedArrayBase->HasItem((uint32)k))
                    {TRACE_IT(57417);
                        continue;
                    }

                    element = typedArrayBase->DirectGetItem((uint32)k);

                    bPresent = true;
                    accumulator = element;
                }
            }
            else
            {TRACE_IT(57418);
                for (; k < length && bPresent == false; k++)
                {TRACE_IT(57419);
                    if (JavascriptOperators::HasItem(obj, k))
                    {TRACE_IT(57420);
                        accumulator = JavascriptOperators::GetItem(obj, k, scriptContext);
                        bPresent = true;
                    }
                }
            }

            if (bPresent == false)
            {TRACE_IT(57421);
                JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
            }
        }

        Assert(accumulator);

        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;

        if (pArr)
        {TRACE_IT(57422);
            for (; k < length; k++)
            {TRACE_IT(57423);
                if (!pArr->DirectGetItemAtFull((uint32)k, &element))
                {TRACE_IT(57424);
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
                {TRACE_IT(57425);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::ReduceObjectHelper<T>(obj, length, k + 1, callBackFn, accumulator, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {TRACE_IT(57426);
            Assert(length <= UINT_MAX);
            for (; k < length; k++)
            {TRACE_IT(57427);
                if (!typedArrayBase->HasItem((uint32)k))
                {TRACE_IT(57428);
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
        {TRACE_IT(57429);
            return JavascriptArray::ReduceObjectHelper<T>(obj, length, k, callBackFn, accumulator, scriptContext);
        }

        return accumulator;
    }

    template <typename T>
    Var JavascriptArray::ReduceObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* callBackFn, Var accumulator, ScriptContext* scriptContext)
    {TRACE_IT(57430);
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;

        for (T k = start; k < length; k++)
        {TRACE_IT(57431);
            if (JavascriptOperators::HasItem(obj, k))
            {TRACE_IT(57432);
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
        {TRACE_IT(57433);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reduceRight"));
        }

        BigIndex length;
        JavascriptArray * pArr = nullptr;
        RecyclableObject* obj = nullptr;

        if (JavascriptArray::Is(args[0]) && !JavascriptArray::FromVar(args[0])->IsCrossSiteObject())
        {TRACE_IT(57434);
            pArr = JavascriptArray::FromVar(args[0]);
            obj = pArr;
        }
        else
        {TRACE_IT(57435);
            if (!JavascriptConversion::ToObject(args[0], scriptContext, &obj))
            {TRACE_IT(57436);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Array.prototype.reduceRight"));
            }
        }

        if (scriptContext->GetConfig()->IsES6ToLengthEnabled())
        {TRACE_IT(57437);
            length = (uint64) JavascriptConversion::ToLength(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }
        else
        {TRACE_IT(57438);
            length = JavascriptConversion::ToUInt32(JavascriptOperators::OP_GetLength(obj, scriptContext), scriptContext);
        }

        if (length.IsSmallIndex())
        {TRACE_IT(57439);
            return JavascriptArray::ReduceRightHelper(pArr, nullptr, obj, length.GetSmallIndex(), args, scriptContext);
        }
        return JavascriptArray::ReduceRightHelper(pArr, nullptr, obj, length.GetBigIndex(), args, scriptContext);
    }

    // Array.prototype.reduceRight as described in ES6.0 (draft 22) Section 22.1.3.19
    template <typename T>
    Var JavascriptArray::ReduceRightHelper(JavascriptArray* pArr, Js::TypedArrayBase* typedArrayBase, RecyclableObject* obj, T length, Arguments& args, ScriptContext* scriptContext)
    {TRACE_IT(57440);
        if (args.Info.Count < 2 || !JavascriptConversion::IsCallable(args[1]))
        {TRACE_IT(57441);
            if (typedArrayBase != nullptr)
            {TRACE_IT(57442);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("[TypedArray].prototype.reduceRight"));
            }
            else
            {TRACE_IT(57443);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.prototype.reduceRight"));
            }
        }

        // If we came from Array.prototype.reduceRight and source object is not a JavascriptArray, source could be a TypedArray
        if (typedArrayBase == nullptr && pArr == nullptr && TypedArrayBase::Is(obj))
        {TRACE_IT(57444);
            typedArrayBase = TypedArrayBase::FromVar(obj);
        }

        RecyclableObject* callBackFn = RecyclableObject::FromVar(args[1]);
        Var accumulator = nullptr;
        Var element = nullptr;
        T k = 0;
        T index = 0;

        if (args.Info.Count > 2)
        {TRACE_IT(57445);
            accumulator = args[2];
        }
        else
        {TRACE_IT(57446);
            if (length == 0)
            {TRACE_IT(57447);
                JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
            }

            bool bPresent = false;
            if (pArr)
            {TRACE_IT(57448);
                for (; k < length && bPresent == false; k++)
                {TRACE_IT(57449);
                    index = length - k - 1;
                    if (!pArr->DirectGetItemAtFull((uint32)index, &element))
                    {TRACE_IT(57450);
                        continue;
                    }
                    bPresent = true;
                    accumulator = element;
                }

                // Side-effects in the callback function may have changed the source array into an ES5Array. If this happens
                // we will process the array elements like an ES5Array.
                if (!JavascriptArray::Is(obj))
                {TRACE_IT(57451);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    pArr = nullptr;
                }
            }
            else if (typedArrayBase)
            {TRACE_IT(57452);
                Assert(length <= UINT_MAX);
                for (; k < length && bPresent == false; k++)
                {TRACE_IT(57453);
                    index = length - k - 1;
                    if (!typedArrayBase->HasItem((uint32)index))
                    {TRACE_IT(57454);
                        continue;
                    }
                    element = typedArrayBase->DirectGetItem((uint32)index);
                    bPresent = true;
                    accumulator = element;
                }
            }
            else
            {TRACE_IT(57455);
                for (; k < length && bPresent == false; k++)
                {TRACE_IT(57456);
                    index = length - k - 1;
                    if (JavascriptOperators::HasItem(obj, index))
                    {TRACE_IT(57457);
                        accumulator = JavascriptOperators::GetItem(obj, index, scriptContext);
                        bPresent = true;
                    }
                }
            }
            if (bPresent == false)
            {TRACE_IT(57458);
                JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
            }
        }

        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var undefinedValue = scriptContext->GetLibrary()->GetUndefined();

        if (pArr)
        {TRACE_IT(57459);
            for (; k < length; k++)
            {TRACE_IT(57460);
                index = length - k - 1;
                if (!pArr->DirectGetItemAtFull((uint32)index, &element))
                {TRACE_IT(57461);
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
                {TRACE_IT(57462);
                    AssertOrFailFastMsg(ES5Array::Is(obj), "The array should have been converted to an ES5Array");
                    return JavascriptArray::ReduceRightObjectHelper<T>(obj, length, k + 1, callBackFn, accumulator, scriptContext);
                }
            }
        }
        else if (typedArrayBase)
        {TRACE_IT(57463);
            Assert(length <= UINT_MAX);
            for (; k < length; k++)
            {TRACE_IT(57464);
                index = length - k - 1;
                if (!typedArrayBase->HasItem((uint32) index))
                {TRACE_IT(57465);
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
        {TRACE_IT(57466);
            return JavascriptArray::ReduceRightObjectHelper<T>(obj, length, k, callBackFn, accumulator, scriptContext);
        }

        return accumulator;
    }

    template <typename T>
    Var JavascriptArray::ReduceRightObjectHelper(RecyclableObject* obj, T length, T start, RecyclableObject* callBackFn, Var accumulator, ScriptContext* scriptContext)
    {TRACE_IT(57467);
        // The correct flag value is CallFlags_Value but we pass CallFlags_None in compat modes
        CallFlags flags = CallFlags_Value;
        Var element = nullptr;
        T index = 0;

        for (T k = start; k < length; k++)
        {TRACE_IT(57468);
            index = length - k - 1;
            if (JavascriptOperators::HasItem(obj, index))
            {TRACE_IT(57469);
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
        {TRACE_IT(57470);
            constructor = RecyclableObject::FromVar(args[0]);
        }

        RecyclableObject* items = nullptr;

        if (args.Info.Count < 2 || !JavascriptConversion::ToObject(args[1], scriptContext, &items))
        {TRACE_IT(57471);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedObject, _u("Array.from"));
        }

        JavascriptArray* itemsArr = nullptr;

        if (JavascriptArray::Is(items))
        {TRACE_IT(57472);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(items);
#endif
            itemsArr = JavascriptArray::FromVar(items);
        }

        bool mapping = false;
        JavascriptFunction* mapFn = nullptr;
        Var mapFnThisArg = nullptr;

        if (args.Info.Count >= 3 && !JavascriptOperators::IsUndefinedObject(args[2]))
        {TRACE_IT(57473);
            if (!JavascriptFunction::Is(args[2]))
            {TRACE_IT(57474);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_FunctionArgument_NeedFunction, _u("Array.from"));
            }

            mapFn = JavascriptFunction::FromVar(args[2]);

            if (args.Info.Count >= 4)
            {TRACE_IT(57475);
                mapFnThisArg = args[3];
            }
            else
            {TRACE_IT(57476);
                mapFnThisArg = library->GetUndefined();
            }

            mapping = true;
        }

        RecyclableObject* newObj = nullptr;
        JavascriptArray* newArr = nullptr;

        RecyclableObject* iterator = JavascriptOperators::GetIterator(items, scriptContext, true /* optional */);

        if (iterator != nullptr)
        {TRACE_IT(57477);
            if (constructor)
            {TRACE_IT(57478);
                Js::Var constructorArgs[] = { constructor };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = RecyclableObject::FromVar(JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext));

                if (JavascriptArray::Is(newObj))
                {TRACE_IT(57479);
#if ENABLE_COPYONACCESS_ARRAY
                    JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                    newArr = JavascriptArray::FromVar(newObj);
                }
            }
            else
            {TRACE_IT(57480);
                newArr = scriptContext->GetLibrary()->CreateArray(0);
                newArr->EnsureHead<Var>();
                newObj = newArr;
            }

            uint32 k = 0;

            JavascriptOperators::DoIteratorStepAndValue(iterator, scriptContext, [&](Var nextValue) {
                if (mapping)
                {TRACE_IT(57481);
                    Assert(mapFn != nullptr);
                    Assert(mapFnThisArg != nullptr);

                    Js::Var mapFnArgs[] = { mapFnThisArg, nextValue, JavascriptNumber::ToVar(k, scriptContext) };
                    Js::CallInfo mapFnCallInfo(Js::CallFlags_Value, _countof(mapFnArgs));
                    nextValue = mapFn->CallFunction(Js::Arguments(mapFnCallInfo, mapFnArgs));
                }

                if (newArr)
                {TRACE_IT(57482);
                    newArr->SetItem(k, nextValue, PropertyOperation_None);
                }
                else
                {TRACE_IT(57483);
                    ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(newObj, k, nextValue), scriptContext, k);
                }

                k++;
            });

            JavascriptOperators::SetProperty(newObj, newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(k, scriptContext), scriptContext, PropertyOperation_ThrowIfNotExtensible);
        }
        else
        {TRACE_IT(57484);
            Var lenValue = JavascriptOperators::OP_GetLength(items, scriptContext);
            int64 len = JavascriptConversion::ToLength(lenValue, scriptContext);

            if (constructor)
            {TRACE_IT(57485);
                Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(len, scriptContext) };
                Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
                newObj = RecyclableObject::FromVar(JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext));

                if (JavascriptArray::Is(newObj))
                {TRACE_IT(57486);
#if ENABLE_COPYONACCESS_ARRAY
                    JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                    newArr = JavascriptArray::FromVar(newObj);
                }
            }
            else
            {TRACE_IT(57487);
                // Abstract operation ArrayCreate throws RangeError if length argument is > 2^32 -1
                if (len > MaxArrayLength)
                {TRACE_IT(57488);
                    JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect, _u("Array.from"));
                }

                // Static cast len should be valid (len < 2^32) or we would throw above
                newArr = scriptContext->GetLibrary()->CreateArray(static_cast<uint32>(len));
                newArr->EnsureHead<Var>();
                newObj = newArr;
            }

            uint32 k = 0;

            for ( ; k < len; k++)
            {TRACE_IT(57489);
                Var kValue;

                if (itemsArr)
                {TRACE_IT(57490);
                    kValue = itemsArr->DirectGetItem(k);
                }
                else
                {TRACE_IT(57491);
                    kValue = JavascriptOperators::OP_GetElementI_UInt32(items, k, scriptContext);
                }

                if (mapping)
                {TRACE_IT(57492);
                    Assert(mapFn != nullptr);
                    Assert(mapFnThisArg != nullptr);

                    Js::Var mapFnArgs[] = { mapFnThisArg, kValue, JavascriptNumber::ToVar(k, scriptContext) };
                    Js::CallInfo mapFnCallInfo(Js::CallFlags_Value, _countof(mapFnArgs));
                    kValue = mapFn->CallFunction(Js::Arguments(mapFnCallInfo, mapFnArgs));
                }

                if (newArr)
                {TRACE_IT(57493);
                    newArr->SetItem(k, kValue, PropertyOperation_None);
                }
                else
                {TRACE_IT(57494);
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
        {TRACE_IT(57495);
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
    {TRACE_IT(57496);
        Assert(args.Info.Count > 0);

        // args.Info.Count cannot equal zero or we would have thrown above so no chance of underflowing
        uint32 len = args.Info.Count - 1;
        Var newObj = nullptr;
        JavascriptArray* newArr = nullptr;
        TypedArrayBase* newTypedArray = nullptr;
        bool isBuiltinArrayCtor = true;

        if (JavascriptOperators::IsConstructor(args[0]))
        {TRACE_IT(57497);
            RecyclableObject* constructor = RecyclableObject::FromVar(args[0]);
            isBuiltinArrayCtor = (constructor == scriptContext->GetLibrary()->GetArrayConstructor());

            Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(len, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            newObj = isTypedArrayEntryPoint ?
                TypedArrayBase::TypedArrayCreate(constructor, &Js::Arguments(constructorCallInfo, constructorArgs), len, scriptContext) :
                JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);

            // If the new object we created is an array, remember that as it will save us time setting properties in the object below
            if (JavascriptArray::Is(newObj))
            {TRACE_IT(57498);
#if ENABLE_COPYONACCESS_ARRAY
                JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(newObj);
#endif
                newArr = JavascriptArray::FromVar(newObj);
            }
            else if (TypedArrayBase::Is(newObj))
            {TRACE_IT(57499);
                newTypedArray = TypedArrayBase::FromVar(newObj);
            }
        }
        else
        {TRACE_IT(57500);
            // We only throw when the constructor property is not a constructor function in the TypedArray version
            if (isTypedArrayEntryPoint)
            {TRACE_IT(57501);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedFunction, _u("[TypedArray].of"));
            }

            newArr = scriptContext->GetLibrary()->CreateArray(len);
            newArr->EnsureHead<Var>();
            newObj = newArr;
        }

        // At least we have a new object of some kind
        Assert(newObj);

        if (isBuiltinArrayCtor)
        {TRACE_IT(57502);
            for (uint32 k = 0; k < len; k++)
            {TRACE_IT(57503);
                Var kValue = args[k + 1];

                newArr->DirectSetItemAt(k, kValue);
            }
        }
        else if (newTypedArray)
        {TRACE_IT(57504);
            for (uint32 k = 0; k < len; k++)
            {TRACE_IT(57505);
                Var kValue = args[k + 1];

                newTypedArray->DirectSetItem(k, kValue);
            }
        }
        else
        {TRACE_IT(57506);
            for (uint32 k = 0; k < len; k++)
            {TRACE_IT(57507);
                Var kValue = args[k + 1];
                ThrowErrorOnFailure(JavascriptArray::SetArrayLikeObjects(RecyclableObject::FromVar(newObj), k, kValue), scriptContext, k);
            }
        }

        if (!isTypedArrayEntryPoint)
        {TRACE_IT(57508);
            // Set length if we are in the Array version of the function
            JavascriptOperators::OP_SetProperty(newObj, Js::PropertyIds::length, JavascriptNumber::ToVar(len, scriptContext), scriptContext, nullptr, PropertyOperation_ThrowIfNotExtensible);
        }

        return newObj;
    }

    JavascriptString* JavascriptArray::ToLocaleStringHelper(Var value, ScriptContext* scriptContext)
    {TRACE_IT(57509);
        TypeId typeId = JavascriptOperators::GetTypeId(value);
        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {TRACE_IT(57510);
            return scriptContext->GetLibrary()->GetEmptyString();
        }
        else
        {TRACE_IT(57511);
            return JavascriptConversion::ToLocaleString(value, scriptContext);
        }
    }

    inline BOOL JavascriptArray::IsFullArray() const
    {TRACE_IT(57512);
        if (head && head->length == length)
        {TRACE_IT(57513);
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
    {TRACE_IT(57514);
        return !(this->head->next == nullptr && this->HasNoMissingValues() && this->length == this->head->length);
    }

    // Fill all missing value in the array and fill it from prototype between startIndex and limitIndex
    // typically startIndex = 0 and limitIndex = length. From start of the array till end of the array.
    void JavascriptArray::FillFromPrototypes(uint32 startIndex, uint32 limitIndex)
    {TRACE_IT(57515);
        if (startIndex >= limitIndex)
        {TRACE_IT(57516);
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
    {TRACE_IT(57517);
        if (head == nullptr || head->left != 0)
        {TRACE_IT(57518);
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
    {TRACE_IT(57519);
        if (Configuration::Global.flags.ForceES5Array)
        {TRACE_IT(57520);
            // There's a bad interaction with the jitted code for native array creation here.
            // ForceES5Array doesn't interact well with native arrays
            if (PHASE_OFF1(NativeArrayPhase))
            {TRACE_IT(57521);
                GetTypeHandler()->ConvertToTypeWithItemAttributes(this);
            }
        }
    }
#endif

    template <typename T, typename Fn>
    void JavascriptArray::ForEachOwnMissingArrayIndexOfObject(JavascriptArray *baseArray, JavascriptArray *destArray, RecyclableObject* obj, uint32 startIndex, uint32 limitIndex, T destIndex, Fn fn)
    {TRACE_IT(57522);
        Assert(DynamicObject::IsAnyArray(obj) || JavascriptOperators::IsObject(obj));

        Var oldValue;
        JavascriptArray* arr = nullptr;
        if (DynamicObject::IsAnyArray(obj))
        {TRACE_IT(57523);
            arr = JavascriptArray::FromAnyArray(obj);
        }
        else if (DynamicType::Is(obj->GetTypeId()))
        {TRACE_IT(57524);
            DynamicObject* dynobj = DynamicObject::FromVar(obj);
            ArrayObject* objectArray = dynobj->GetObjectArray();
            arr = (objectArray && JavascriptArray::IsAnyArray(objectArray)) ? JavascriptArray::FromAnyArray(objectArray) : nullptr;
        }

        if (arr != nullptr)
        {TRACE_IT(57525);
            if (JavascriptArray::Is(arr))
            {TRACE_IT(57526);
                arr = EnsureNonNativeArray(arr);
                ArrayElementEnumerator e(arr, startIndex, limitIndex);

                while(e.MoveNext<Var>())
                {TRACE_IT(57527);
                    uint32 index = e.GetIndex();
                    if (!baseArray->DirectGetVarItemAt(index, &oldValue, baseArray->GetScriptContext()))
                    {TRACE_IT(57528);
                        T n = destIndex + (index - startIndex);
                        if (destArray == nullptr || !destArray->DirectGetItemAt(n, &oldValue))
                        {
                            fn(index, e.GetItem<Var>());
                        }
                    }
                }
            }
            else
            {TRACE_IT(57529);
                ScriptContext* scriptContext = obj->GetScriptContext();

                Assert(ES5Array::Is(arr));

                ES5Array* es5Array = ES5Array::FromVar(arr);
                ES5ArrayIndexStaticEnumerator<true> e(es5Array);

                while (e.MoveNext())
                {TRACE_IT(57530);
                    uint32 index = e.GetIndex();
                    if (index < startIndex) continue;
                    else if (index >= limitIndex) break;

                    if (!baseArray->DirectGetVarItemAt(index, &oldValue, baseArray->GetScriptContext()))
                    {TRACE_IT(57531);
                        T n = destIndex + (index - startIndex);
                        if (destArray == nullptr || !destArray->DirectGetItemAt(n, &oldValue))
                        {TRACE_IT(57532);
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
    {TRACE_IT(57533);
        Init(arr);
    }

    //
    // Initialize this enumerator and prepare for the first MoveNext.
    //
    void JavascriptArray::ArrayElementEnumerator::Init(JavascriptArray* arr)
    {TRACE_IT(57534);
        // Find start segment
        seg = (arr ? arr->GetBeginLookupSegment(start) : nullptr);
        while (seg && (seg->left + seg->length <= start))
        {TRACE_IT(57535);
            seg = seg->next;
        }

        // Set start index and endIndex
        if (seg)
        {TRACE_IT(57536);
            if (seg->left >= end)
            {TRACE_IT(57537);
                seg = nullptr;
            }
            else
            {TRACE_IT(57538);
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
    {TRACE_IT(57539);
        while (seg)
        {TRACE_IT(57540);
            // Look for next non-null item in current segment
            while (++index < endIndex)
            {TRACE_IT(57541);
                if (!SparseArraySegment<T>::IsMissingItem(&((SparseArraySegment<T>*)seg)->elements[index]))
                {TRACE_IT(57542);
                    return true;
                }
            }

            // Move to next segment
            seg = seg->next;
            if (seg)
            {TRACE_IT(57543);
                if (seg->left >= end)
                {TRACE_IT(57544);
                    seg = nullptr;
                    break;
                }
                else
                {TRACE_IT(57545);
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
    {TRACE_IT(57546);
        Assert(seg && index < seg->length && index < endIndex);
        return seg->left + index;
    }

    //
    // Get current array element value.
    //
    template<typename T>
    T JavascriptArray::ArrayElementEnumerator::GetItem() const
    {TRACE_IT(57547);
        Assert(seg && index < seg->length && index < endIndex &&
               !SparseArraySegment<T>::IsMissingItem(&((SparseArraySegment<T>*)seg)->elements[index]));
        return ((SparseArraySegment<T>*)seg)->elements[index];
    }

    //
    // Construct a BigIndex initialized to a given uint32 (small index).
    //
    JavascriptArray::BigIndex::BigIndex(uint32 initIndex)
        : index(initIndex), bigIndex(InvalidIndex)
    {TRACE_IT(57548);
        //ok if initIndex == InvalidIndex
    }

    //
    // Construct a BigIndex initialized to a given uint64 (large or small index).
    //
    JavascriptArray::BigIndex::BigIndex(uint64 initIndex)
        : index(InvalidIndex), bigIndex(initIndex)
    {TRACE_IT(57549);
        if (bigIndex < InvalidIndex) // if it's actually small index
        {TRACE_IT(57550);
            index = static_cast<uint32>(bigIndex);
            bigIndex = InvalidIndex;
        }
    }

    bool JavascriptArray::BigIndex::IsUint32Max() const
    {TRACE_IT(57551);
        return index == InvalidIndex && bigIndex == InvalidIndex;
    }
    bool JavascriptArray::BigIndex::IsSmallIndex() const
    {TRACE_IT(57552);
        return index < InvalidIndex;
    }

    uint32 JavascriptArray::BigIndex::GetSmallIndex() const
    {TRACE_IT(57553);
        Assert(IsSmallIndex());
        return index;
    }

    uint64 JavascriptArray::BigIndex::GetBigIndex() const
    {TRACE_IT(57554);
        Assert(!IsSmallIndex());
        return bigIndex;
    }
    //
    // Convert this index value to a JS number
    //
    Var JavascriptArray::BigIndex::ToNumber(ScriptContext* scriptContext) const
    {TRACE_IT(57555);
        if (IsSmallIndex())
        {TRACE_IT(57556);
            return small_index::ToNumber(index, scriptContext);
        }
        else
        {TRACE_IT(57557);
            return JavascriptNumber::ToVar(bigIndex, scriptContext);
        }
    }

    //
    // Increment this index by 1.
    //
    const JavascriptArray::BigIndex& JavascriptArray::BigIndex::operator++()
    {TRACE_IT(57558);
        if (IsSmallIndex())
        {TRACE_IT(57559);
            ++index;
            // If index reaches InvalidIndex, we will start to use bigIndex which is initially InvalidIndex.
        }
        else
        {TRACE_IT(57560);
            bigIndex = bigIndex + 1;
        }

        return *this;
    }

    //
    // Decrement this index by 1.
    //
    const JavascriptArray::BigIndex& JavascriptArray::BigIndex::operator--()
    {TRACE_IT(57561);
        if (IsSmallIndex())
        {TRACE_IT(57562);
            --index;
        }
        else
        {TRACE_IT(57563);
            Assert(index == InvalidIndex && bigIndex >= InvalidIndex);

            --bigIndex;
            if (bigIndex < InvalidIndex)
            {TRACE_IT(57564);
                index = InvalidIndex - 1;
                bigIndex = InvalidIndex;
            }
        }

        return *this;
    }

    JavascriptArray::BigIndex JavascriptArray::BigIndex::operator+(const BigIndex& delta) const
    {TRACE_IT(57565);
        if (delta.IsSmallIndex())
        {TRACE_IT(57566);
            return operator+(delta.GetSmallIndex());
        }
        if (IsSmallIndex())
        {TRACE_IT(57567);
            return index + delta.GetBigIndex();
        }

        return bigIndex + delta.GetBigIndex();
    }

    //
    // Get a new BigIndex representing this + delta.
    //
    JavascriptArray::BigIndex JavascriptArray::BigIndex::operator+(uint32 delta) const
    {TRACE_IT(57568);
        if (IsSmallIndex())
        {TRACE_IT(57569);
            uint32 newIndex;
            if (UInt32Math::Add(index, delta, &newIndex))
            {TRACE_IT(57570);
                return static_cast<uint64>(index) + static_cast<uint64>(delta);
            }
            else
            {TRACE_IT(57571);
                return newIndex; // ok if newIndex == InvalidIndex
            }
        }
        else
        {TRACE_IT(57572);
            return bigIndex + static_cast<uint64>(delta);
        }
    }

    bool JavascriptArray::BigIndex::operator==(const BigIndex& rhs) const
    {TRACE_IT(57573);
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57574);
            return this->GetSmallIndex() == rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {TRACE_IT(57575);
            // if lhs is big promote rhs
            return this->GetBigIndex() == (uint64) rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57576);
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) == rhs.GetBigIndex();
        }
        return this->GetBigIndex() == rhs.GetBigIndex();
    }

    bool JavascriptArray::BigIndex::operator> (const BigIndex& rhs) const
    {TRACE_IT(57577);
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57578);
            return this->GetSmallIndex() > rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {TRACE_IT(57579);
            // if lhs is big promote rhs
            return this->GetBigIndex() > (uint64)rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57580);
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) > rhs.GetBigIndex();
        }
        return this->GetBigIndex() > rhs.GetBigIndex();
    }

    bool JavascriptArray::BigIndex::operator< (const BigIndex& rhs) const
    {TRACE_IT(57581);
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57582);
            return this->GetSmallIndex() < rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {TRACE_IT(57583);
            // if lhs is big promote rhs
            return this->GetBigIndex() < (uint64)rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57584);
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) < rhs.GetBigIndex();
        }
        return this->GetBigIndex() < rhs.GetBigIndex();
    }

    bool JavascriptArray::BigIndex::operator<=(const BigIndex& rhs) const
    {TRACE_IT(57585);
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57586);
            return this->GetSmallIndex() <= rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {TRACE_IT(57587);
            // if lhs is big promote rhs
            return this->GetBigIndex() <= (uint64)rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && !this->IsSmallIndex())
        {TRACE_IT(57588);
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) <= rhs.GetBigIndex();
        }
        return this->GetBigIndex() <= rhs.GetBigIndex();
    }

    bool JavascriptArray::BigIndex::operator>=(const BigIndex& rhs) const
    {TRACE_IT(57589);
        if (rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57590);
            return this->GetSmallIndex() >= rhs.GetSmallIndex();
        }
        else if (rhs.IsSmallIndex() && !this->IsSmallIndex())
        {TRACE_IT(57591);
            // if lhs is big promote rhs
            return this->GetBigIndex() >= (uint64)rhs.GetSmallIndex();
        }
        else if (!rhs.IsSmallIndex() && this->IsSmallIndex())
        {TRACE_IT(57592);
            // if rhs is big promote lhs
            return ((uint64)this->GetSmallIndex()) >= rhs.GetBigIndex();
        }
        return this->GetBigIndex() >= rhs.GetBigIndex();
    }

    BOOL JavascriptArray::BigIndex::GetItem(JavascriptArray* arr, Var* outVal) const
    {TRACE_IT(57593);
        if (IsSmallIndex())
        {TRACE_IT(57594);
            return small_index::GetItem(arr, index, outVal);
        }
        else
        {TRACE_IT(57595);
            ScriptContext* scriptContext = arr->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            return arr->GetProperty(arr, propertyRecord->GetPropertyId(), outVal, NULL, scriptContext);
        }
    }

    BOOL JavascriptArray::BigIndex::SetItem(JavascriptArray* arr, Var newValue) const
    {TRACE_IT(57596);
        if (IsSmallIndex())
        {TRACE_IT(57597);
            return small_index::SetItem(arr, index, newValue);
        }
        else
        {TRACE_IT(57598);
            ScriptContext* scriptContext = arr->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            return arr->SetProperty(propertyRecord->GetPropertyId(), newValue, PropertyOperation_None, NULL);
        }
    }

    void JavascriptArray::BigIndex::SetItemIfNotExist(JavascriptArray* arr, Var newValue) const
    {TRACE_IT(57599);
        if (IsSmallIndex())
        {TRACE_IT(57600);
            small_index::SetItemIfNotExist(arr, index, newValue);
        }
        else
        {TRACE_IT(57601);
            ScriptContext* scriptContext = arr->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            Var oldValue;
            PropertyId propertyId = propertyRecord->GetPropertyId();
            if (!arr->GetProperty(arr, propertyId, &oldValue, NULL, scriptContext))
            {TRACE_IT(57602);
                arr->SetProperty(propertyId, newValue, PropertyOperation_None, NULL);
            }
        }
    }

    BOOL JavascriptArray::BigIndex::DeleteItem(JavascriptArray* arr) const
    {TRACE_IT(57603);
        if (IsSmallIndex())
        {TRACE_IT(57604);
            return small_index::DeleteItem(arr, index);
        }
        else
        {TRACE_IT(57605);
            ScriptContext* scriptContext = arr->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            return arr->DeleteProperty(propertyRecord->GetPropertyId(), PropertyOperation_None);
        }
    }

    BOOL JavascriptArray::BigIndex::SetItem(RecyclableObject* obj, Var newValue, PropertyOperationFlags flags) const
    {TRACE_IT(57606);
        if (IsSmallIndex())
        {TRACE_IT(57607);
            return small_index::SetItem(obj, index, newValue, flags);
        }
        else
        {TRACE_IT(57608);
            ScriptContext* scriptContext = obj->GetScriptContext();
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, scriptContext, &propertyRecord);
            return JavascriptOperators::SetProperty(obj, obj, propertyRecord->GetPropertyId(), newValue, scriptContext, flags);
        }
    }

    BOOL JavascriptArray::BigIndex::DeleteItem(RecyclableObject* obj, PropertyOperationFlags flags) const
    {TRACE_IT(57609);
        if (IsSmallIndex())
        {TRACE_IT(57610);
            return small_index::DeleteItem(obj, index, flags);
        }
        else
        {TRACE_IT(57611);
            PropertyRecord const * propertyRecord;
            JavascriptOperators::GetPropertyIdForInt(bigIndex, obj->GetScriptContext(), &propertyRecord);
            return JavascriptOperators::DeleteProperty(obj, propertyRecord->GetPropertyId(), flags);
        }
    }

    //
    // Truncate the array at start and clone the truncated span as properties starting at dstIndex (asserting dstIndex >= MaxArrayLength).
    //
    void JavascriptArray::TruncateToProperties(const BigIndex& dstIndex, uint32 start)
    {TRACE_IT(57612);
        Assert(!dstIndex.IsSmallIndex());
        typedef IndexTrace<BigIndex> index_trace;

        BigIndex dst = dstIndex;
        uint32 i = start;

        ArrayElementEnumerator e(this, start);
        while(e.MoveNext<Var>())
        {TRACE_IT(57613);
            // delete all items not enumerated
            while (i < e.GetIndex())
            {TRACE_IT(57614);
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
        {TRACE_IT(57615);
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
    {TRACE_IT(57616);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<Var>())
        {TRACE_IT(57617);
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
    {TRACE_IT(57618);
        end = min(end, srcArray->length);
        if (start < end)
        {TRACE_IT(57619);
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
    {TRACE_IT(57620);
        end = min(end, srcArray->length);
        if (start < end)
        {TRACE_IT(57621);
            Assert(end - start <= MaxArrayLength - dstIndex);
            InternalCopyArrayElements(dstArray, dstIndex, srcArray, start, end);
        }
    }

    template <typename T>
    void JavascriptArray::CopyAnyArrayElementsToVar(JavascriptArray* dstArray, T dstIndex, JavascriptArray* srcArray, uint32 start, uint32 end)
    {TRACE_IT(57622);
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
    {TRACE_IT(57623);
        end = min(end, srcArray->length);
        if (start < end)
        {TRACE_IT(57624);
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
    {TRACE_IT(57625);
        end = min(end, srcArray->length);
        if (start < end)
        {TRACE_IT(57626);
            Assert(end - start <= MaxArrayLength - dstIndex);
            InternalCopyNativeIntArrayElements(dstArray, dstIndex, srcArray, start, end);
        }
    }

    bool JavascriptArray::CopyNativeIntArrayElements(JavascriptNativeIntArray* dstArray, uint32 dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end)
    {TRACE_IT(57627);
        end = min(end, srcArray->length);
        if (start >= end)
        {TRACE_IT(57628);
            return false;
        }

        Assert(end - start <= MaxArrayLength - dstIndex);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<int32>())
        {TRACE_IT(57629);
            uint n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, e.GetItem<int32>());
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {TRACE_IT(57630);
            JavascriptArray *varArray = JavascriptNativeIntArray::ToVarArray(dstArray);
            InternalFillFromPrototype(varArray, dstIndex, srcArray, start, end, count);
            return true;
        }

        return false;
    }

    bool JavascriptArray::CopyNativeIntArrayElementsToFloat(JavascriptNativeFloatArray* dstArray, uint32 dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end)
    {TRACE_IT(57631);
        end = min(end, srcArray->length);
        if (start >= end)
        {TRACE_IT(57632);
            return false;
        }

        Assert(end - start <= MaxArrayLength - dstIndex);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<int32>())
        {TRACE_IT(57633);
            uint n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, (double)e.GetItem<int32>());
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {TRACE_IT(57634);
            JavascriptArray *varArray = JavascriptNativeFloatArray::ToVarArray(dstArray);
            InternalFillFromPrototype(varArray, dstIndex, srcArray, start, end, count);
            return true;
        }

        return false;
    }

    void JavascriptArray::CopyNativeFloatArrayElementsToVar(JavascriptArray* dstArray, const BigIndex& dstIndex, JavascriptNativeFloatArray* srcArray, uint32 start, uint32 end)
    {TRACE_IT(57635);
        end = min(end, srcArray->length);
        if (start < end)
        {TRACE_IT(57636);
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
    {TRACE_IT(57637);
        end = min(end, srcArray->length);
        if (start < end)
        {TRACE_IT(57638);
            Assert(end - start <= MaxArrayLength - dstIndex);
            InternalCopyNativeFloatArrayElements(dstArray, dstIndex, srcArray, start, end);
        }
    }

    bool JavascriptArray::CopyNativeFloatArrayElements(JavascriptNativeFloatArray* dstArray, uint32 dstIndex, JavascriptNativeFloatArray* srcArray, uint32 start, uint32 end)
    {TRACE_IT(57639);
        end = min(end, srcArray->length);
        if (start >= end)
        {TRACE_IT(57640);
            return false;
        }

        Assert(end - start <= MaxArrayLength - dstIndex);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<double>())
        {TRACE_IT(57641);
            uint n = dstIndex + (e.GetIndex() - start);
            dstArray->DirectSetItemAt(n, e.GetItem<double>());
            count++;
        }

        // iterate on the array's prototypes only if not all elements found
        if (start + count != end)
        {TRACE_IT(57642);
            JavascriptArray *varArray = JavascriptNativeFloatArray::ToVarArray(dstArray);
            InternalFillFromPrototype(varArray, dstIndex, srcArray, start, end, count);
            return true;
        }

        return false;
    }

    JavascriptArray *JavascriptArray::EnsureNonNativeArray(JavascriptArray *arr)
    {TRACE_IT(57643);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arr);
#endif
        if (JavascriptNativeIntArray::Is(arr))
        {TRACE_IT(57644);
            arr = JavascriptNativeIntArray::ToVarArray((JavascriptNativeIntArray*)arr);
        }
        else if (JavascriptNativeFloatArray::Is(arr))
        {TRACE_IT(57645);
            arr = JavascriptNativeFloatArray::ToVarArray((JavascriptNativeFloatArray*)arr);
        }

        return arr;
    }

    BOOL JavascriptNativeIntArray::DirectGetItemAtFull(uint32 index, Var* outVal)
    {TRACE_IT(57646);
        ScriptContext* requestContext = type->GetScriptContext();
        if (JavascriptNativeIntArray::GetItem(this, index, outVal, requestContext))
        {TRACE_IT(57647);
            return TRUE;
        }

        return JavascriptOperators::GetItem(this, this->GetPrototype(), index, outVal, requestContext);
    }

    BOOL JavascriptNativeFloatArray::DirectGetItemAtFull(uint32 index, Var* outVal)
    {TRACE_IT(57648);
        ScriptContext* requestContext = type->GetScriptContext();
        if (JavascriptNativeFloatArray::GetItem(this, index, outVal, requestContext))
        {TRACE_IT(57649);
            return TRUE;
        }

        return JavascriptOperators::GetItem(this, this->GetPrototype(), index, outVal, requestContext);
    }

    template<typename T>
    void JavascriptArray::InternalCopyNativeIntArrayElements(JavascriptArray* dstArray, const T& dstIndex, JavascriptNativeIntArray* srcArray, uint32 start, uint32 end)
    {TRACE_IT(57650);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ScriptContext *scriptContext = dstArray->GetScriptContext();
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<int32>())
        {TRACE_IT(57651);
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
    {TRACE_IT(57652);
        Assert(start < end && end <= srcArray->length);

        uint32 count = 0;

        // iterate on the array itself
        ScriptContext *scriptContext = dstArray->GetScriptContext();
        ArrayElementEnumerator e(srcArray, start, end);
        while(e.MoveNext<double>())
        {TRACE_IT(57653);
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
    {TRACE_IT(57654);
        // At this stage we have an array literal with some arguments to be spread.
        // First we need to calculate the real size of the final literal.
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayToSpread);
#endif
        JavascriptArray *array = FromVar(arrayToSpread);
        uint32 actualLength = array->GetLength();

        for (unsigned i = 0; i < spreadIndices->count; ++i)
        {TRACE_IT(57655);
            actualLength = UInt32Math::Add(actualLength - 1, GetSpreadArgLen(array->DirectGetItem(spreadIndices->elements[i]), scriptContext));
        }

        JavascriptArray *result = FromVar(OP_NewScArrayWithMissingValues(actualLength, scriptContext));

        // Now we copy each element and expand the spread parameters inline.
        for (unsigned i = 0, spreadArrIndex = 0, resultIndex = 0; i < array->GetLength() && resultIndex < actualLength; ++i)
        {TRACE_IT(57656);
            uint32 spreadIndex = spreadIndices->elements[spreadArrIndex]; // The index of the next element to be spread.

            // An array needs a slow copy if it is a cross-site object or we have missing values that need to be set to undefined.
            auto needArraySlowCopy = [&](Var instance) {TRACE_IT(57657);
                if (JavascriptArray::Is(instance))
                {TRACE_IT(57658);
                    JavascriptArray *arr = JavascriptArray::FromVar(instance);
                    return arr->IsCrossSiteObject() || arr->IsFillFromPrototypes();
                }
                return false;
            };

            // Designed to have interchangeable arguments with CopyAnyArrayElementsToVar.
            auto slowCopy = [&scriptContext, &needArraySlowCopy](JavascriptArray *dstArray, unsigned dstIndex, Var srcArray, uint32 start, uint32 end) {TRACE_IT(57659);
                Assert(needArraySlowCopy(srcArray) || ArgumentsObject::Is(srcArray) || TypedArrayBase::Is(srcArray) || JavascriptString::Is(srcArray));

                RecyclableObject *propertyObject;
                if (!JavascriptOperators::GetPropertyObject(srcArray, scriptContext, &propertyObject))
                {TRACE_IT(57660);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidSpreadArgument);
                }

                for (uint32 j = start; j < end; j++)
                {TRACE_IT(57661);
                    Var element;
                    if (!JavascriptOperators::GetItem(srcArray, propertyObject, j, &element, scriptContext))
                    {TRACE_IT(57662);
                        // Copy across missing values as undefined as per 12.2.5.2 SpreadElement : ... AssignmentExpression 5f.
                        element = scriptContext->GetLibrary()->GetUndefined();
                    }
                    dstArray->DirectSetItemAt(dstIndex++, element);
                }
            };

            if (i < spreadIndex)
            {TRACE_IT(57663);
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
            {TRACE_IT(57664);
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
            {TRACE_IT(57665);
                Var instance = array->DirectGetItem(i);

                if (SpreadArgument::Is(instance))
                {TRACE_IT(57666);
                    SpreadArgument* spreadArgument = SpreadArgument::FromVar(instance);
                    uint32 len = spreadArgument->GetArgumentSpreadCount();
                    const Var*  spreadItems = spreadArgument->GetArgumentSpread();
                    for (uint32 j = 0; j < len; j++)
                    {TRACE_IT(57667);
                        result->DirectSetItemAt(resultIndex++, spreadItems[j]);
                    }

                }
                else
                {TRACE_IT(57668);
                    AssertMsg(JavascriptArray::Is(instance) || TypedArrayBase::Is(instance), "Only SpreadArgument, TypedArray, and JavascriptArray should be listed as spread arguments");

                    // We first try to interpret the spread parameter as a JavascriptArray.
                    JavascriptArray *arr = nullptr;
                    if (JavascriptArray::Is(instance))
                    {TRACE_IT(57669);
                        arr = JavascriptArray::FromVar(instance);
                    }

                    if (arr != nullptr)
                    {TRACE_IT(57670);
                        if (arr->GetLength() > 0)
                        {TRACE_IT(57671);
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
                    {TRACE_IT(57672);
                        uint32 len = GetSpreadArgLen(instance, scriptContext);
                        slowCopy(result, resultIndex, instance, 0, len);
                        resultIndex += len;
                    }
                }

                if (spreadArrIndex < spreadIndices->count - 1)
                {TRACE_IT(57673);
                    spreadArrIndex++;
                }
            }
        }
        return result;
    }

    uint32 JavascriptArray::GetSpreadArgLen(Var spreadArg, ScriptContext *scriptContext)
    {TRACE_IT(57674);
        // A spread argument can be anything that returns a 'length' property, even if that
        // property is null or undefined.
        spreadArg = CrossSite::MarshalVar(scriptContext, spreadArg);
        if (JavascriptArray::Is(spreadArg))
        {TRACE_IT(57675);
            JavascriptArray *arr = JavascriptArray::FromVar(spreadArg);
            return arr->GetLength();
        }

        if (TypedArrayBase::Is(spreadArg))
        {TRACE_IT(57676);
            TypedArrayBase *tarr = TypedArrayBase::FromVar(spreadArg);
            return tarr->GetLength();
        }

        if (SpreadArgument::Is(spreadArg))
        {TRACE_IT(57677);
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
        {TRACE_IT(57678);
        }

        void operator()(SparseArraySegmentBase* s)
        {TRACE_IT(57679);
            Assert(seg == s);
            if (seg)
            {TRACE_IT(57680);
                seg = seg->next;
            }
        }
    };

    void JavascriptArray::ValidateArrayCommon()
    {TRACE_IT(57681);
        SparseArraySegmentBase * lastUsedSegment = this->GetLastUsedSegment();
        AssertMsg(this != nullptr && head && lastUsedSegment, "Array should not be null");
        AssertMsg(head->left == 0, "Array always should have a segment starting at zero");

        // Simple segments validation
        bool foundLastUsedSegment = false;
        SparseArraySegmentBase *seg = head;
        while(seg != nullptr)
        {TRACE_IT(57682);
            if (seg == lastUsedSegment)
            {TRACE_IT(57683);
                foundLastUsedSegment = true;
            }

            AssertMsg(seg->length <= seg->size , "Length greater than size not possible");

            SparseArraySegmentBase* next = seg->next;
            if (next != nullptr)
            {TRACE_IT(57684);
                AssertMsg(seg->left < next->left, "Segment is adjacent to or overlaps with next segment");
                AssertMsg(seg->size <= (next->left - seg->left), "Segment is adjacent to or overlaps with next segment");
                AssertMsg(!SparseArraySegmentBase::IsLeafSegment(seg, this->GetScriptContext()->GetRecycler()), "Leaf segment with a next pointer");
            }
            else
            {TRACE_IT(57685);
                AssertMsg(seg->length <= MaxArrayLength - seg->left, "Segment index range overflow");
                AssertMsg(seg->left + seg->length <= this->length, "Segment index range exceeds array length");
            }

            seg = next;
        }
        AssertMsg(foundLastUsedSegment || HasSegmentMap(), "Corrupt lastUsedSegment in array header");

        // Validate segmentMap if present
        if (HasSegmentMap())
        {TRACE_IT(57686);
            ArraySegmentsVisitor visitor(head);
            GetSegmentMap()->Walk(visitor);
        }
    }

    void JavascriptArray::ValidateArray()
    {TRACE_IT(57687);
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {TRACE_IT(57688);
            return;
        }
        ValidateArrayCommon();
        // Detailed segments validation
        JavascriptArray::ValidateVarSegment(SparseArraySegment<Var>::From(head));
    }

    void JavascriptNativeIntArray::ValidateArray()
    {TRACE_IT(57689);
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {TRACE_IT(57690);
#if DBG
            SparseArraySegmentBase *seg = head;
            while (seg)
            {TRACE_IT(57691);
                if (seg->next != nullptr)
                {TRACE_IT(57692);
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
    {TRACE_IT(57693);
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {TRACE_IT(57694);
#if DBG
            SparseArraySegmentBase *seg = head;
            while (seg)
            {TRACE_IT(57695);
                if (seg->next != nullptr)
                {TRACE_IT(57696);
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
    {TRACE_IT(57697);
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {TRACE_IT(57698);
            return;
        }
        int32 inspect;
        double inspectDouble;
        while (seg)
        {TRACE_IT(57699);
            uint32 i = 0;
            for (i = 0; i < seg->length; i++)
            {TRACE_IT(57700);
                if (SparseArraySegment<Var>::IsMissingItem(&seg->elements[i]))
                {TRACE_IT(57701);
                    continue;
                }
                if (TaggedInt::Is(seg->elements[i]))
                {TRACE_IT(57702);
                    inspect = TaggedInt::ToInt32(seg->elements[i]);

                }
                else if (JavascriptNumber::Is_NoTaggedIntCheck(seg->elements[i]))
                {TRACE_IT(57703);
                    inspectDouble = JavascriptNumber::GetValue(seg->elements[i]);
                }
                else
                {TRACE_IT(57704);
                    AssertMsg(RecyclableObject::Is(seg->elements[i]), "Invalid entry in segment");
                }
            }
            ValidateSegment(seg);

            seg = SparseArraySegment<Var>::From(seg->next);
        }
    }

    template<typename T>
    void JavascriptArray::ValidateSegment(SparseArraySegment<T>* seg)
    {TRACE_IT(57705);
        if (!Js::Configuration::Global.flags.ArrayValidate)
        {TRACE_IT(57706);
            return;
        }

        while (seg)
        {TRACE_IT(57707);
            uint32 i = seg->length;
            while (i < seg->size)
            {TRACE_IT(57708);
                AssertMsg(SparseArraySegment<T>::IsMissingItem(&seg->elements[i]), "Non missing value the end of the segment");
                i++;
            }

            seg = SparseArraySegment<T>::From(seg->next);
        }
    }
#endif

    template <typename T>
    void JavascriptArray::InitBoxedInlineHeadSegment(SparseArraySegment<T> * dst, SparseArraySegment<T> * src)
    {TRACE_IT(57709);
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
    {TRACE_IT(57710);
        if (boxHead)
        {
            InitBoxedInlineHeadSegment(DetermineInlineHeadSegmentPointer<JavascriptArray, 0, true>(this), SparseArraySegment<Var>::From(instance->head));
        }
        else
        {TRACE_IT(57711);

            SetFlags(GetFlags() & ~DynamicObjectFlags::HasSegmentMap);
            head = instance->head;
            SetLastUsedSegment(instance->GetLastUsedSegment());
        }
    }

    template <typename T>
    T * JavascriptArray::BoxStackInstance(T * instance)
    {TRACE_IT(57712);
        Assert(ThreadContext::IsOnStack(instance));
        // On the stack, the we reserved a pointer before the object as to store the boxed value
        T ** boxedInstanceRef = ((T **)instance) - 1;
        T * boxedInstance = *boxedInstanceRef;
        if (boxedInstance)
        {TRACE_IT(57713);
            return boxedInstance;
        }

        const size_t inlineSlotsSize = instance->GetTypeHandler()->GetInlineSlotsSize();
        if (ThreadContext::IsOnStack(instance->head))
        {TRACE_IT(57714);
            boxedInstance = RecyclerNewPlusZ(instance->GetRecycler(),
                inlineSlotsSize + sizeof(Js::SparseArraySegmentBase) + instance->head->size * sizeof(typename T::TElement),
                T, instance, true);
        }
        else if(inlineSlotsSize)
        {TRACE_IT(57715);
            boxedInstance = RecyclerNewPlusZ(instance->GetRecycler(), inlineSlotsSize, T, instance, false);
        }
        else
        {TRACE_IT(57716);
            boxedInstance = RecyclerNew(instance->GetRecycler(), T, instance, false);
        }

        *boxedInstanceRef = boxedInstance;
        return boxedInstance;
    }

    JavascriptArray *
    JavascriptArray::BoxStackInstance(JavascriptArray * instance)
    {TRACE_IT(57717);
        return BoxStackInstance<JavascriptArray>(instance);
    }

#if ENABLE_TTD
    void JavascriptArray::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(57718);
        TTDAssert(this->GetTypeId() == Js::TypeIds_Array || this->GetTypeId() == Js::TypeIds_ES5Array, "Should only be used on basic arrays (or called as super from ES5Array.");

        ScriptContext* ctx = this->GetScriptContext();

        uint32 index = Js::JavascriptArray::InvalidIndex;
        while(true)
        {TRACE_IT(57719);
            index = this->GetNextIndex(index);
            if(index == Js::JavascriptArray::InvalidIndex) // End of array
            {TRACE_IT(57720);
                break;
            }

            Js::Var aval = nullptr;
            if(this->DirectGetVarItemAt(index, &aval, ctx))
            {TRACE_IT(57721);
                extractor->MarkVisitVar(aval);
            }
        }
    }

    void JavascriptArray::ProcessCorePaths()
    {TRACE_IT(57722);
        TTDAssert(this->GetTypeId() == Js::TypeIds_Array, "Should only be used on basic arrays.");

        ScriptContext* ctx = this->GetScriptContext();

        uint32 index = Js::JavascriptArray::InvalidIndex;
        while(true)
        {TRACE_IT(57723);
            index = this->GetNextIndex(index);
            if(index == Js::JavascriptArray::InvalidIndex) // End of array
            {TRACE_IT(57724);
                break;
            }

            Js::Var aval = nullptr;
            if(this->DirectGetVarItemAt(index, &aval, ctx))
            {TRACE_IT(57725);
                TTD::UtilSupport::TTAutoString pathExt;
                ctx->TTDWellKnownInfo->BuildArrayIndexBuffer(index, pathExt);

                ctx->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, aval, pathExt.GetStrValue());
            }
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptArray::GetSnapTag_TTD() const
    {TRACE_IT(57726);
        return TTD::NSSnapObjects::SnapObjectType::SnapArrayObject;
    }

    void JavascriptArray::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(57727);
        TTDAssert(this->GetTypeId() == Js::TypeIds_Array, "Should only be used on basic Js arrays.");

        TTD::NSSnapObjects::SnapArrayInfo<TTD::TTDVar>* sai = TTD::NSSnapObjects::ExtractArrayValues<TTD::TTDVar>(this, alloc);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayInfo<TTD::TTDVar>*, TTD::NSSnapObjects::SnapObjectType::SnapArrayObject>(objData, sai);
    }
#endif

    JavascriptNativeArray::JavascriptNativeArray(JavascriptNativeArray * instance) :
        JavascriptArray(instance, false),
        weakRefToFuncBody(instance->weakRefToFuncBody)
    {TRACE_IT(57728);
    }

    JavascriptNativeIntArray::JavascriptNativeIntArray(JavascriptNativeIntArray * instance, bool boxHead) :
        JavascriptNativeArray(instance)
    {TRACE_IT(57729);
        if (boxHead)
        {
            InitBoxedInlineHeadSegment(DetermineInlineHeadSegmentPointer<JavascriptNativeIntArray, 0, true>(this), SparseArraySegment<int>::From(instance->head));
        }
        else
        {TRACE_IT(57730);
            // Base class ctor should have copied these
            Assert(head == instance->head);
            Assert(segmentUnion.lastUsedSegment == instance->GetLastUsedSegment());
        }
    }

    JavascriptNativeIntArray *
    JavascriptNativeIntArray::BoxStackInstance(JavascriptNativeIntArray * instance)
    {TRACE_IT(57731);
        return JavascriptArray::BoxStackInstance<JavascriptNativeIntArray>(instance);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptNativeIntArray::GetSnapTag_TTD() const
    {TRACE_IT(57732);
        return TTD::NSSnapObjects::SnapObjectType::SnapNativeIntArrayObject;
    }

    void JavascriptNativeIntArray::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(57733);
        TTD::NSSnapObjects::SnapArrayInfo<int32>* sai = TTD::NSSnapObjects::ExtractArrayValues<int32>(this, alloc);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayInfo<int32>*, TTD::NSSnapObjects::SnapObjectType::SnapNativeIntArrayObject>(objData, sai);
    }

#if ENABLE_COPYONACCESS_ARRAY
    TTD::NSSnapObjects::SnapObjectType JavascriptCopyOnAccessNativeIntArray::GetSnapTag_TTD() const
    {TRACE_IT(57734);
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
    {TRACE_IT(57735);
        if (boxHead)
        {
            InitBoxedInlineHeadSegment(DetermineInlineHeadSegmentPointer<JavascriptNativeFloatArray, 0, true>(this), SparseArraySegment<double>::From(instance->head));
        }
        else
        {TRACE_IT(57736);
            // Base class ctor should have copied these
            Assert(head == instance->head);
            Assert(segmentUnion.lastUsedSegment == instance->GetLastUsedSegment());
        }
    }

    JavascriptNativeFloatArray *
    JavascriptNativeFloatArray::BoxStackInstance(JavascriptNativeFloatArray * instance)
    {TRACE_IT(57737);
        return JavascriptArray::BoxStackInstance<JavascriptNativeFloatArray>(instance);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptNativeFloatArray::GetSnapTag_TTD() const
    {TRACE_IT(57738);
        return TTD::NSSnapObjects::SnapObjectType::SnapNativeFloatArrayObject;
    }

    void JavascriptNativeFloatArray::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(57739);
        TTDAssert(this->GetTypeId() == Js::TypeIds_NativeFloatArray, "Should only be used on native float arrays.");

        TTD::NSSnapObjects::SnapArrayInfo<double>* sai = TTD::NSSnapObjects::ExtractArrayValues<double>(this, alloc);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapArrayInfo<double>*, TTD::NSSnapObjects::SnapObjectType::SnapNativeFloatArrayObject>(objData, sai);
    }
#endif

    template<typename T>
    RecyclableObject*
    JavascriptArray::ArraySpeciesCreate(Var originalArray, T length, ScriptContext* scriptContext, bool *pIsIntArray, bool *pIsFloatArray, bool *pIsBuiltinArrayCtor)
    {TRACE_IT(57740);
        if (originalArray == nullptr || !scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {TRACE_IT(57741);
            return nullptr;
        }

        if (JavascriptArray::Is(originalArray)
            && !DynamicObject::FromVar(originalArray)->GetDynamicType()->GetTypeHandler()->GetIsNotPathTypeHandlerOrHasUserDefinedCtor()
            && DynamicObject::FromVar(originalArray)->GetPrototype() == scriptContext->GetLibrary()->GetArrayPrototype()
            && !scriptContext->GetLibrary()->GetArrayObjectHasUserDefinedSpecies())
        {TRACE_IT(57742);
            return nullptr;
        }

        Var constructor = scriptContext->GetLibrary()->GetUndefined();

        if (JavascriptOperators::IsArray(originalArray))
        {TRACE_IT(57743);
            if (!JavascriptOperators::GetProperty(RecyclableObject::FromVar(originalArray), PropertyIds::constructor, &constructor, scriptContext))
            {TRACE_IT(57744);
                return nullptr;
            }

            if (JavascriptOperators::IsConstructor(constructor))
            {TRACE_IT(57745);
                ScriptContext* constructorScriptContext = RecyclableObject::FromVar(constructor)->GetScriptContext();
                if (constructorScriptContext != scriptContext)
                {TRACE_IT(57746);
                    if (constructorScriptContext->GetLibrary()->GetArrayConstructor() == constructor)
                    {TRACE_IT(57747);
                        constructor = scriptContext->GetLibrary()->GetUndefined();
                    }
                }
            }

            if (JavascriptOperators::IsObject(constructor))
            {TRACE_IT(57748);
                if (!JavascriptOperators::GetProperty((RecyclableObject*)constructor, PropertyIds::_symbolSpecies, &constructor, scriptContext))
                {TRACE_IT(57749);
                    if (pIsBuiltinArrayCtor != nullptr)
                    {TRACE_IT(57750);
                        *pIsBuiltinArrayCtor = false;
                    }

                    return nullptr;
                }
                if (constructor == scriptContext->GetLibrary()->GetNull())
                {TRACE_IT(57751);
                    constructor = scriptContext->GetLibrary()->GetUndefined();
                }
            }
        }

        if (constructor == scriptContext->GetLibrary()->GetUndefined() || constructor == scriptContext->GetLibrary()->GetArrayConstructor())
        {TRACE_IT(57752);
            if (length > UINT_MAX)
            {TRACE_IT(57753);
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthConstructIncorrect);
            }

            if (nullptr == pIsIntArray)
            {TRACE_IT(57754);
                return scriptContext->GetLibrary()->CreateArray(static_cast<uint32>(length));
            }
            else
            {TRACE_IT(57755);
                // If the constructor function is the built-in Array constructor, we can be smart and create the right type of native array.
                JavascriptArray* pArr = JavascriptArray::FromVar(originalArray);
                pArr->GetArrayTypeAndConvert(pIsIntArray, pIsFloatArray);
                return CreateNewArrayHelper(static_cast<uint32>(length), *pIsIntArray, *pIsFloatArray, pArr, scriptContext);
            }
        }

        if (!JavascriptOperators::IsConstructor(constructor))
        {TRACE_IT(57756);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NotAConstructor, _u("constructor[Symbol.species]"));
        }

        if (pIsBuiltinArrayCtor != nullptr)
        {TRACE_IT(57757);
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
    {TRACE_IT(57758);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(57759);
            return false;
        }
        return DynamicObject::DeleteProperty(propertyId, flags);
    }

    BOOL JavascriptArray::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(57760);
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {TRACE_IT(57761);
            return false;
        }
        return DynamicObject::DeleteProperty(propertyNameString, flags);
    }

    BOOL JavascriptArray::HasProperty(PropertyId propertyId)
    {TRACE_IT(57762);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(57763);
            return true;
        }

        ScriptContext* scriptContext = GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(57764);
            return this->HasItem(index);
        }

        return DynamicObject::HasProperty(propertyId);
    }

    BOOL JavascriptArray::IsEnumerable(PropertyId propertyId)
    {TRACE_IT(57765);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(57766);
            return false;
        }
        return DynamicObject::IsEnumerable(propertyId);
    }

    BOOL JavascriptArray::IsConfigurable(PropertyId propertyId)
    {TRACE_IT(57767);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(57768);
            return false;
        }
        return DynamicObject::IsConfigurable(propertyId);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetEnumerable(PropertyId propertyId, BOOL value)
    {TRACE_IT(57769);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(57770);
            Assert(!value); // Can't change array length enumerable
            return true;
        }

        ScriptContext* scriptContext = this->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(57771);
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
    {TRACE_IT(57772);
        ScriptContext* scriptContext = this->GetScriptContext();
        uint32 index;

        bool setLengthNonWritable = (propertyId == PropertyIds::length && !value);
        if (setLengthNonWritable || scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(57773);
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
    {TRACE_IT(57774);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(57775);
            Assert(!value); // Can't change array length configurable
            return true;
        }

        ScriptContext* scriptContext = this->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(57776);
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
    {TRACE_IT(57777);
        ScriptContext* scriptContext = this->GetScriptContext();

        // SetAttributes on "length" is not expected. DefineOwnProperty uses SetWritable. If this is
        // changed, we need to handle it here.
        Assert(propertyId != PropertyIds::length);

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(57778);
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
    {TRACE_IT(57779);
        ScriptContext* scriptContext = this->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(57780);
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
    {TRACE_IT(57781);
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
            ->SetItemWithAttributes(this, index, value, attributes);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetItemAttributes(uint32 index, PropertyAttributes attributes)
    {TRACE_IT(57782);
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
            ->SetItemAttributes(this, index, attributes);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::SetItemAccessors(uint32 index, Var getter, Var setter)
    {TRACE_IT(57783);
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)
            ->SetItemAccessors(this, index, getter, setter);
    }

    // Check if this objectArray isFrozen.
    BOOL JavascriptArray::IsObjectArrayFrozen()
    {TRACE_IT(57784);
        // If this is still a JavascriptArray, it's not frozen.
        return false;
    }

    JavascriptEnumerator * JavascriptArray::GetIndexEnumerator(EnumeratorFlags flags, ScriptContext* requestContext)
    {TRACE_IT(57785);
        if (!!(flags & EnumeratorFlags::SnapShotSemantics))
        {TRACE_IT(57786);
            return RecyclerNew(GetRecycler(), JavascriptArrayIndexSnapshotEnumerator, this, flags, requestContext);
        }
        return RecyclerNew(GetRecycler(), JavascriptArrayIndexEnumerator, this, flags, requestContext);
    }

    BOOL JavascriptArray::GetNonIndexEnumerator(JavascriptStaticEnumerator * enumerator, ScriptContext* requestContext)
    {TRACE_IT(57787);
        return enumerator->Initialize(nullptr, nullptr, this, EnumeratorFlags::SnapShotSemantics, requestContext, nullptr);
    }

    BOOL JavascriptArray::IsItemEnumerable(uint32 index)
    {TRACE_IT(57788);
        return true;
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::PreventExtensions()
    {TRACE_IT(57789);
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)->PreventExtensions(this);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::Seal()
    {TRACE_IT(57790);
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)->Seal(this);
    }

    //
    // Evolve typeHandlers explicitly so that simple typeHandlers can skip array
    // handling and only check instance objectArray for numeric propertyIds.
    //
    BOOL JavascriptArray::Freeze()
    {TRACE_IT(57791);
        return GetTypeHandler()->ConvertToTypeWithItemAttributes(this)->Freeze(this);
    }

    BOOL JavascriptArray::GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext)
    {TRACE_IT(57792);
        if (index == 0)
        {TRACE_IT(57793);
            *propertyName = requestContext->GetPropertyString(PropertyIds::length);
            return true;
        }
        return false;
    }

    // Returns the number of special non-enumerable properties this type has.
    uint JavascriptArray::GetSpecialPropertyCount() const
    {TRACE_IT(57794);
        return _countof(specialPropertyIds);
    }

    // Returns the list of special non-enumerable properties for the type.
    PropertyId const * JavascriptArray::GetSpecialPropertyIds() const
    {TRACE_IT(57795);
        return specialPropertyIds;
    }

    BOOL JavascriptArray::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(57796);
        return JavascriptArray::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptArray::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {
        if (GetPropertyBuiltIns(propertyId, value))
        {TRACE_IT(57797);
            return true;
        }

        ScriptContext* scriptContext = GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(57798);
            return this->GetItem(this, index, value, scriptContext);
        }

        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL JavascriptArray::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(57799);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value))
        {TRACE_IT(57800);
            return true;
        }

        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    BOOL JavascriptArray::GetPropertyBuiltIns(PropertyId propertyId, Var* value)
    {TRACE_IT(57801);
        //
        // length being accessed. Return array length
        //
        if (propertyId == PropertyIds::length)
        {TRACE_IT(57802);
            *value = JavascriptNumber::ToVar(this->GetLength(), GetScriptContext());
            return true;
        }

        return false;
    }

    BOOL JavascriptArray::HasItem(uint32 index)
    {TRACE_IT(57803);
        Var value;
        return this->DirectGetItemAt<Var>(index, &value);
    }

    BOOL JavascriptArray::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(57804);
        return this->DirectGetItemAt<Var>(index, value);
    }

    BOOL JavascriptArray::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(57805);
        return this->DirectGetItemAt<Var>(index, value);
    }

    BOOL JavascriptArray::DirectGetVarItemAt(uint32 index, Var *value, ScriptContext *requestContext)
    {TRACE_IT(57806);
        return this->DirectGetItemAt<Var>(index, value);
    }

    BOOL JavascriptNativeIntArray::HasItem(uint32 index)
    {TRACE_IT(57807);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        int32 value;
        return this->DirectGetItemAt<int32>(index, &value);
    }

    BOOL JavascriptNativeFloatArray::HasItem(uint32 index)
    {TRACE_IT(57808);
        double dvalue;
        return this->DirectGetItemAt<double>(index, &dvalue);
    }

    BOOL JavascriptNativeIntArray::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(57809);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        return JavascriptNativeIntArray::DirectGetVarItemAt(index, value, requestContext);
    }

    BOOL JavascriptNativeIntArray::DirectGetVarItemAt(uint32 index, Var *value, ScriptContext *requestContext)
    {TRACE_IT(57810);
        int32 intvalue;
        if (!this->DirectGetItemAt<int32>(index, &intvalue))
        {TRACE_IT(57811);
            return FALSE;
        }
        *value = JavascriptNumber::ToVar(intvalue, requestContext);
        return TRUE;
    }

    BOOL JavascriptNativeIntArray::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(57812);
        return JavascriptNativeIntArray::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL JavascriptNativeFloatArray::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(57813);
        return JavascriptNativeFloatArray::DirectGetVarItemAt(index, value, requestContext);
    }

    BOOL JavascriptNativeFloatArray::DirectGetVarItemAt(uint32 index, Var *value, ScriptContext *requestContext)
    {TRACE_IT(57814);
        double dvalue;
        int32 ivalue;
        if (!this->DirectGetItemAt<double>(index, &dvalue))
        {TRACE_IT(57815);
            return FALSE;
        }
        if (*(uint64*)&dvalue == 0ull)
        {TRACE_IT(57816);
            *value = TaggedInt::ToVarUnchecked(0);
        }
        else if (JavascriptNumber::TryGetInt32Value(dvalue, &ivalue) && !TaggedInt::IsOverflow(ivalue))
        {TRACE_IT(57817);
            *value = TaggedInt::ToVarUnchecked(ivalue);
        }
        else
        {TRACE_IT(57818);
            *value = JavascriptNumber::ToVarWithCheck(dvalue, requestContext);
        }
        return TRUE;
    }

    BOOL JavascriptNativeFloatArray::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(57819);
        return JavascriptNativeFloatArray::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL JavascriptArray::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(57820);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        uint32 indexValue;
        if (propertyId == PropertyIds::length)
        {TRACE_IT(57821);
            return this->SetLength(value);
        }
        else if (GetScriptContext()->IsNumericPropertyId(propertyId, &indexValue))
        {TRACE_IT(57822);
            // Call this or subclass method
            return SetItem(indexValue, value, flags);
        }
        else
        {TRACE_IT(57823);
            return DynamicObject::SetProperty(propertyId, value, flags, info);
        }
    }

    BOOL JavascriptArray::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(57824);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && propertyRecord->GetPropertyId() == PropertyIds::length)
        {TRACE_IT(57825);
            return this->SetLength(value);
        }

        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    BOOL JavascriptArray::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(57826);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        ScriptContext* scriptContext = GetScriptContext();

        if (propertyId == PropertyIds::length)
        {TRACE_IT(57827);
            Assert(attributes == PropertyWritable);
            Assert(IsWritable(propertyId) && !IsConfigurable(propertyId) && !IsEnumerable(propertyId));
            return this->SetLength(value);
        }

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(57828);
            // Call this or subclass method
            return SetItemWithAttributes(index, value, attributes);
        }

        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL JavascriptArray::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(57829);
        this->DirectSetItemAt(index, value);
        return true;
    }

    BOOL JavascriptNativeIntArray::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(57830);
        int32 iValue;
        double dValue;
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(this);
#endif
        TypeId typeId = this->TrySetNativeIntArrayItem(value, &iValue, &dValue);
        if (typeId == TypeIds_NativeIntArray)
        {TRACE_IT(57831);
            this->SetItem(index, iValue);
        }
        else if (typeId == TypeIds_NativeFloatArray)
        {TRACE_IT(57832);
            reinterpret_cast<JavascriptNativeFloatArray*>(this)->DirectSetItemAt<double>(index, dValue);
        }
        else
        {TRACE_IT(57833);
            this->DirectSetItemAt<Var>(index, value);
        }

        return TRUE;
    }

    TypeId JavascriptNativeIntArray::TrySetNativeIntArrayItem(Var value, int32 *iValue, double *dValue)
    {TRACE_IT(57834);
        if (TaggedInt::Is(value))
        {TRACE_IT(57835);
            int32 i = TaggedInt::ToInt32(value);
            if (i != JavascriptNativeIntArray::MissingItem)
            {TRACE_IT(57836);
                *iValue = i;
                return TypeIds_NativeIntArray;
            }
        }
        if (JavascriptNumber::Is_NoTaggedIntCheck(value))
        {TRACE_IT(57837);
            bool isInt32;
            int32 i;
            double d = JavascriptNumber::GetValue(value);
            if (JavascriptNumber::TryGetInt32OrUInt32Value(d, &i, &isInt32))
            {TRACE_IT(57838);
                if (isInt32 && i != JavascriptNativeIntArray::MissingItem)
                {TRACE_IT(57839);
                    *iValue = i;
                    return TypeIds_NativeIntArray;
                }
            }
            else
            {TRACE_IT(57840);
                *dValue = d;
                JavascriptNativeIntArray::ToNativeFloatArray(this);
                return TypeIds_NativeFloatArray;
            }
        }

        JavascriptNativeIntArray::ToVarArray(this);
        return TypeIds_Array;
    }

    BOOL JavascriptNativeIntArray::SetItem(uint32 index, int32 iValue)
    {TRACE_IT(57841);
        if (iValue == JavascriptNativeIntArray::MissingItem)
        {TRACE_IT(57842);
            JavascriptArray *varArr = JavascriptNativeIntArray::ToVarArray(this);
            varArr->DirectSetItemAt(index, JavascriptNumber::ToVar(iValue, GetScriptContext()));
            return TRUE;
        }

        this->DirectSetItemAt(index, iValue);
        return TRUE;
    }

    BOOL JavascriptNativeFloatArray::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(57843);
        double dValue;
        TypeId typeId = this->TrySetNativeFloatArrayItem(value, &dValue);
        if (typeId == TypeIds_NativeFloatArray)
        {TRACE_IT(57844);
            this->SetItem(index, dValue);
        }
        else
        {TRACE_IT(57845);
            this->DirectSetItemAt(index, value);
        }
        return TRUE;
    }

    TypeId JavascriptNativeFloatArray::TrySetNativeFloatArrayItem(Var value, double *dValue)
    {TRACE_IT(57846);
        if (TaggedInt::Is(value))
        {TRACE_IT(57847);
            *dValue = (double)TaggedInt::ToInt32(value);
            return TypeIds_NativeFloatArray;
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(value))
        {TRACE_IT(57848);
            *dValue = JavascriptNumber::GetValue(value);
            return TypeIds_NativeFloatArray;
        }

        JavascriptNativeFloatArray::ToVarArray(this);
        return TypeIds_Array;
    }

    BOOL JavascriptNativeFloatArray::SetItem(uint32 index, double dValue)
    {TRACE_IT(57849);
        if (*(uint64*)&dValue == *(uint64*)&JavascriptNativeFloatArray::MissingItem)
        {TRACE_IT(57850);
            JavascriptArray *varArr = JavascriptNativeFloatArray::ToVarArray(this);
            varArr->DirectSetItemAt(index, JavascriptNumber::ToVarNoCheck(dValue, GetScriptContext()));
            return TRUE;
        }

        this->DirectSetItemAt<double>(index, dValue);
        return TRUE;
    }

    BOOL JavascriptArray::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(57851);
        return this->DirectDeleteItemAt<Var>(index);
    }

    BOOL JavascriptNativeIntArray::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(57852);
        return this->DirectDeleteItemAt<int32>(index);
    }

    BOOL JavascriptNativeFloatArray::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(57853);
        return this->DirectDeleteItemAt<double>(index);
    }

    BOOL JavascriptArray::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(57854);
        return enumerator->Initialize(nullptr, this, this, flags, requestContext, forInCache);
    }

    BOOL JavascriptArray::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(57855);
        stringBuilder->Append(_u('['));

        if (this->length < 10)
        {TRACE_IT(57856);
            auto funcPtr = [&]()
            {
                ENTER_PINNED_SCOPE(JavascriptString, valueStr);
                valueStr = JavascriptArray::JoinHelper(this, GetLibrary()->GetCommaDisplayString(), requestContext);
                stringBuilder->Append(valueStr->GetString(), valueStr->GetLength());
                LEAVE_PINNED_SCOPE();
            };

            if (!requestContext->GetThreadContext()->IsScriptActive())
            {TRACE_IT(57857);
                BEGIN_JS_RUNTIME_CALL(requestContext);
                {TRACE_IT(57858);
                    funcPtr();
                }
                END_JS_RUNTIME_CALL(requestContext);
            }
            else
            {TRACE_IT(57859);
                funcPtr();
            }
        }
        else
        {TRACE_IT(57860);
            stringBuilder->AppendCppLiteral(_u("..."));
        }

        stringBuilder->Append(_u(']'));

        return TRUE;
    }

    BOOL JavascriptArray::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(57861);
        stringBuilder->AppendCppLiteral(_u("Object, (Array)"));
        return TRUE;
    }

    bool JavascriptNativeArray::Is(Var aValue)
    {TRACE_IT(57862);
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptNativeArray::Is(typeId);
    }

    bool JavascriptNativeArray::Is(TypeId typeId)
    {TRACE_IT(57863);
        return JavascriptNativeIntArray::Is(typeId) || JavascriptNativeFloatArray::Is(typeId);
    }

    JavascriptNativeArray* JavascriptNativeArray::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNativeArray'");

        return static_cast<JavascriptNativeArray *>(RecyclableObject::FromVar(aValue));
    }

    bool JavascriptNativeIntArray::Is(Var aValue)
    {
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptNativeIntArray::Is(typeId);
    }

#if ENABLE_COPYONACCESS_ARRAY
    bool JavascriptCopyOnAccessNativeIntArray::Is(Var aValue)
    {
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptCopyOnAccessNativeIntArray::Is(typeId);
    }
#endif

    bool JavascriptNativeIntArray::Is(TypeId typeId)
    {
        return typeId == TypeIds_NativeIntArray;
    }

#if ENABLE_COPYONACCESS_ARRAY
    bool JavascriptCopyOnAccessNativeIntArray::Is(TypeId typeId)
    {
        return typeId == TypeIds_CopyOnAccessNativeIntArray;
    }
#endif

    bool JavascriptNativeIntArray::IsNonCrossSite(Var aValue)
    {
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
    {TRACE_IT(57869);
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        return JavascriptNativeFloatArray::Is(typeId);
    }

    bool JavascriptNativeFloatArray::Is(TypeId typeId)
    {TRACE_IT(57870);
        return typeId == TypeIds_NativeFloatArray;
    }

    bool JavascriptNativeFloatArray::IsNonCrossSite(Var aValue)
    {TRACE_IT(57871);
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
