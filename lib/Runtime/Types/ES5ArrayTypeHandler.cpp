//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

#include "Library/ES5Array.h"

namespace Js
{
    IndexPropertyDescriptorMap::IndexPropertyDescriptorMap(Recycler* recycler)
        : recycler(recycler), indexList(nullptr), lastIndexAt(-1)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 12\n");
        indexPropertyMap = RecyclerNew(recycler, InnerMap, recycler);
    }

    void IndexPropertyDescriptorMap::Add(uint32 key, const IndexPropertyDescriptor& value)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 17\n");
        if (indexPropertyMap->Count() >= (INT_MAX / 2))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 19\n");
            Js::Throw::OutOfMemory(); // Would possibly overflow our dictionary
        }

        indexList = nullptr; // Discard indexList on change
        indexPropertyMap->Add(key, value);
    }

    //
    // Build sorted index list if not found.
    //
    void IndexPropertyDescriptorMap::EnsureIndexList()
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 31\n");
        if (!indexList)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 33\n");
            int length = Count();
            indexList = RecyclerNewArrayLeaf(recycler, uint32, length);
            lastIndexAt = -1; // Reset lastAccessorIndexAt

            for (int i = 0; i < length; i++)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 39\n");
                indexList[i] = GetKeyAt(i);
            }

            ::qsort(indexList, length, sizeof(uint32), &CompareIndex);
        }
    }

    //
    // Try get the last index in this map if it contains any valid index.
    //
    bool IndexPropertyDescriptorMap::TryGetLastIndex(uint32* lastIndex)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 51\n");
        if (Count() == 0)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 53\n");
            return false;
        }

        EnsureIndexList();

        // Search the index list backwards for the last index
        for (int i = Count() - 1; i >= 0; i--)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 61\n");
            uint32 key = indexList[i];

            IndexPropertyDescriptor* descriptor;
            bool b = TryGetReference(key, &descriptor);
            Assert(b && descriptor);

            if (!(descriptor->Attributes & PropertyDeleted))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 69\n");
                *lastIndex = key;
                return true;
            }
        }

        return false;
    }

    //
    // Get the next index in the map, similar to JavascriptArray::GetNextIndex().
    //
    BOOL IndexPropertyDescriptorMap::IsValidDescriptorToken(void * descriptorValidationToken) const
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 82\n");
        return indexList != nullptr && descriptorValidationToken == indexList;
    }

    uint32 IndexPropertyDescriptorMap::GetNextDescriptor(uint32 key, IndexPropertyDescriptor** ppDescriptor, void ** pDescriptorValidationToken)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 87\n");
        *pDescriptorValidationToken = nullptr;
        *ppDescriptor = nullptr;
        if (Count() == 0)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 91\n");
            return JavascriptArray::InvalidIndex;
        }

        EnsureIndexList();

        // Find the first item index > key
        int low = 0;
        if (key != JavascriptArray::InvalidIndex)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 100\n");
            Assert(lastIndexAt < Count()); // lastIndexAt must be either -1 or in range [0, Count)
            if (lastIndexAt >= 0 && indexList[lastIndexAt] == key)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 103\n");
                low = lastIndexAt + 1;
            }
            else
            {
                int high = Count() - 1;
                while (low < high)
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 110\n");
                    int mid = (low + high) / 2;
                    if (indexList[mid] <= key)
                    {LOGMEIN("ES5ArrayTypeHandler.cpp] 113\n");
                        low = mid + 1;
                    }
                    else
                    {
                        high = mid;
                    }
                }
                if (low < Count() && indexList[low] <= key)
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 122\n");
                    ++low;
                }
            }
        }

        // Search for the next valid index
        for (; low < Count(); low++)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 130\n");
            uint32 index = indexList[low];
            IndexPropertyDescriptor* descriptor;
            bool b = TryGetReference(index, &descriptor);
            Assert(b && descriptor);

            if (!(descriptor->Attributes & PropertyDeleted))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 137\n");
                lastIndexAt = low; // Save last index location
                *pDescriptorValidationToken = indexList; // use the index list to keep track of where the descriptor has been changed.
                *ppDescriptor = descriptor;
                return index;
            }
        }

        return JavascriptArray::InvalidIndex;
    }

    //
    // Try to delete the range [firstKey, length) from right to left, stop if running into an element whose
    // [[CanDelete]] is false. Return the index where [index, ...) are all deleted.
    //
    uint32 IndexPropertyDescriptorMap::DeleteDownTo(uint32 firstKey)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 153\n");
        EnsureIndexList();

        // Iterate the index list backwards to delete from right to left
        for (int i = Count() - 1; i >= 0; i--)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 158\n");
            uint32 key = indexList[i];
            if (key < firstKey)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 161\n");
                break; // We are done, [firstKey, ...) have already been deleted
            }

            IndexPropertyDescriptor* descriptor;
            bool b = TryGetReference(key, &descriptor);
            Assert(b && descriptor);

            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 170\n");
                continue; // Skip empty entry
            }

            if (descriptor->Attributes & PropertyConfigurable)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 175\n");
                descriptor->Getter = nullptr;
                descriptor->Setter = nullptr;
                descriptor->Attributes = PropertyDeleted | PropertyWritable | PropertyConfigurable;
            }
            else
            {
                // Cannot delete key, and [key + 1, ...) are all deleted
                return key + 1;
            }
        }

        return firstKey;
    }

    template <class T>
    ES5ArrayTypeHandlerBase<T>* ES5ArrayTypeHandlerBase<T>::New(Recycler * recycler, int initialCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 192\n");
        return DictionaryTypeHandlerBase<T>::template NewTypeHandler<ES5ArrayTypeHandlerBase<T>>(recycler, initialCapacity, inlineSlotCapacity, offsetOfInlineSlots);
    }

    template <class T>
    ES5ArrayTypeHandlerBase<T>::ES5ArrayTypeHandlerBase(Recycler* recycler)
        : DictionaryTypeHandlerBase<T>(recycler), dataItemAttributes(PropertyDynamicTypeDefaults), lengthWritable(true)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 199\n");
        indexPropertyMap = RecyclerNew(recycler, IndexPropertyDescriptorMap, recycler);
    }

    template <class T>
    ES5ArrayTypeHandlerBase<T>::ES5ArrayTypeHandlerBase(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
        : DictionaryTypeHandlerBase<T>(recycler, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots), dataItemAttributes(PropertyDynamicTypeDefaults), lengthWritable(true)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 206\n");
        indexPropertyMap = RecyclerNew(recycler, IndexPropertyDescriptorMap, recycler);
    }

    template <class T>
    ES5ArrayTypeHandlerBase<T>::ES5ArrayTypeHandlerBase(Recycler* recycler, DictionaryTypeHandlerBase<T>* typeHandler)
        : DictionaryTypeHandlerBase<T>(typeHandler), dataItemAttributes(PropertyDynamicTypeDefaults), lengthWritable(true)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 213\n");
        indexPropertyMap = RecyclerNew(recycler, IndexPropertyDescriptorMap, recycler);
    }

    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetIsPrototype(DynamicObject * instance)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 219\n");
        __super::SetIsPrototype(instance);

        // We have ES5 array has array/object prototype, we can't use array fast path for set
        // as index could be readonly or be getter/setter in the prototype
        // TODO: we may be able to separate out the array fast path and the object array fast path
        // here.
        instance->GetScriptContext()->optimizationOverrides.DisableArraySetElementFastPath();
    }

    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetInstanceTypeHandler(DynamicObject* instance, bool hasChanged)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 231\n");
        Assert(JavascriptArray::Is(instance));
        if (this->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 234\n");
            // We have ES5 array has array/object prototype, we can't use array fast path for set
            // as index could be readonly or be getter/setter in the prototype
            // TODO: we may be able to separate out the array fast path and the object array fast path
            // here.
            instance->GetScriptContext()->optimizationOverrides.DisableArraySetElementFastPath();
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        JavascriptArray * arrayInstance = JavascriptArray::EnsureNonNativeArray(JavascriptArray::FromVar(instance));
#if DBG
        bool doneConversion = false;
        Js::Type* oldType = arrayInstance->GetType();
#endif
        bool isCrossSiteObject = false;

        try
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 253\n");
            if (!CrossSite::IsCrossSiteObjectTyped(arrayInstance))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 255\n");
                // Convert instance to an ES5Array
                Assert(VirtualTableInfo<JavascriptArray>::HasVirtualTable(arrayInstance));
                VirtualTableInfo<ES5Array>::SetVirtualTable(arrayInstance);
            }
            else
            {
                // If instance was a cross-site JavascriptArray, convert to a cross-site ES5Array
                Assert(VirtualTableInfo<CrossSiteObject<JavascriptArray>>::HasVirtualTable(arrayInstance));
                VirtualTableInfo<CrossSiteObject<ES5Array>>::SetVirtualTable(arrayInstance);
                isCrossSiteObject = true;
            }

            arrayInstance->ChangeType(); // force change TypeId
            __super::SetInstanceTypeHandler(arrayInstance, false); // after forcing the type change, we don't need to changeType again.
#if DBG
            doneConversion = true;
#endif
        }
        catch(Js::ExceptionBase)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 275\n");
            Assert(!doneConversion);
            // change vtbl shouldn't OOM. revert back the vtable.
            if (isCrossSiteObject)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 279\n");
                Assert(VirtualTableInfo<CrossSiteObject<ES5Array>>::HasVirtualTable(arrayInstance));
                VirtualTableInfo<CrossSiteObject<JavascriptArray>>::SetVirtualTable(arrayInstance);
            }
            else
            {
                Assert(VirtualTableInfo<ES5Array>::HasVirtualTable(arrayInstance));
                VirtualTableInfo<JavascriptArray>::SetVirtualTable(arrayInstance);
            }
            // The only allocation is in ChangeType, which won't have changed the type yet.
            Assert(arrayInstance->GetType() == oldType);
            throw;
        }
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasDataItem(ES5Array* arr, uint32 index)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 296\n");
        Var value;
        return arr->DirectGetItemAt(index, &value);
    }

    //
    // Check if the array contains any data item not in attribute map (so that we know there are items
    // using shared data item attributes)
    //
    template <class T>
    bool ES5ArrayTypeHandlerBase<T>::HasAnyDataItemNotInMap(ES5Array* arr)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 307\n");
        JavascriptArray::ArrayElementEnumerator e(arr);
        while (e.MoveNext<Var>())
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 310\n");
            if (!indexPropertyMap->ContainsKey(e.GetIndex()))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 312\n");
                return true;
            }
        }

        return false;
    }

    template <class T>
    PropertyAttributes ES5ArrayTypeHandlerBase<T>::GetDataItemAttributes() const
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 322\n");
        return dataItemAttributes;
    }
    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetDataItemSealed()
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 327\n");
        dataItemAttributes &= ~(PropertyConfigurable);
    }
    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetDataItemFrozen()
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 332\n");
        dataItemAttributes &= ~(PropertyWritable | PropertyConfigurable);
        this->ClearHasOnlyWritableDataProperties();
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::CantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 339\n");
        JavascriptError::ThrowCantAssignIfStrictMode(flags, scriptContext);
        return false;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::CantExtend(PropertyOperationFlags flags, ScriptContext* scriptContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 346\n");
        JavascriptError::ThrowCantExtendIfStrictMode(flags, scriptContext);
        return false;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasItem(ES5Array* arr, uint32 index)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 353\n");
        // We have the item if we have its descriptor.
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 357\n");
            return !(descriptor->Attributes & PropertyDeleted);
        }

        // Otherwise check if we have such a data item.
        return HasDataItem(arr, index);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItem(ES5Array* arr, DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 367\n");
        ScriptContext* scriptContext = instance->GetScriptContext();

        // Reject if we need to grow non-writable length
        if (!CanSetItemAt(arr, index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 372\n");
            return CantExtend(flags, scriptContext);
        }

        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 378\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 380\n");
                if (!(this->GetFlags() & this->IsExtensibleFlag))
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 382\n");
                    return CantExtend(flags, scriptContext);
                }

                // No need to change hasNoEnumerableProperties. See comment in ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes.
                Assert(!arr->GetHasNoEnumerableProperties());

                Assert(!descriptor->Getter && !descriptor->Setter);
                descriptor->Attributes = PropertyDynamicTypeDefaults;
                arr->DirectSetItemAt(index, value);
                return true;
            }

            if (!(descriptor->Attributes & PropertyWritable))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 396\n");
                return CantAssign(flags, scriptContext);
            }

            if (HasDataItem(arr, index))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 401\n");
                arr->DirectSetItemAt(index, value);
            }
            else if (descriptor->Setter)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 405\n");
                RecyclableObject* func = RecyclableObject::FromVar(descriptor->Setter);
                // TODO : request context
                JavascriptOperators::CallSetter(func, instance, value, NULL);
            }

            return true;
        }

        //
        // Not found in attribute map. Extend or update data item.
        //
        if (!(this->GetFlags() & this->IsExtensibleFlag))
        {
            if (!HasDataItem(arr, index))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 420\n");
                return CantExtend(flags, scriptContext);
            }
            else if (!(GetDataItemAttributes() & PropertyWritable))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 424\n");
                return CantAssign(flags, scriptContext);
            }
        }

        // No need to change hasNoEnumerableProperties. See comment in ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes.
        Assert(!arr->GetHasNoEnumerableProperties());

        arr->DirectSetItemAt(index, value); // sharing data item attributes
        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes(ES5Array* arr, DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {
        // Reject if we need to grow non-writable length
        if (!CanSetItemAt(arr, index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 441\n");
            return false;
        }

        // We don't track non-enumerable items in object array.  Objects with an object array
        // report having enumerable properties.  See DynamicObject::GetHasNoEnumerableProperties.
        // Array objects (which don't have an object array, and could report their hasNoEnumerableProperties
        // directly) take an explicit type transition before switching to ES5ArrayTypeHandler, so their
        // hasNoEnumerableProperties flag gets cleared.
        Assert(!arr->GetHasNoEnumerableProperties());

        if (!(attributes & PropertyWritable))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 453\n");
            this->ClearHasOnlyWritableDataProperties();
            if(this->GetFlags() & this->IsPrototypeFlag)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 456\n");
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }

        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 463\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 465\n");
                Assert(!descriptor->Getter && !descriptor->Setter);
                descriptor->Attributes = attributes;
                arr->DirectSetItemAt(index, value);
                return true;
            }

            descriptor->Attributes = attributes;

            if (HasDataItem(arr, index))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 475\n");
                arr->DirectSetItemAt(index, value);
            }
            else if (descriptor->Setter)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 479\n");
                RecyclableObject* func = RecyclableObject::FromVar(descriptor->Setter);
                // TODO : request context
                JavascriptOperators::CallSetter(func, instance, value, NULL);
            }
        }
        else
        {
            // See comment for the same assert above.
            Assert(!arr->GetHasNoEnumerableProperties());

            // Not found in attribute map
            arr->DirectSetItemAt(index, value);
            indexPropertyMap->Add(index, IndexPropertyDescriptor(attributes));
        }

        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemAttributes(ES5Array* arr, DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 500\n");
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 503\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 505\n");
                return false;
            }

            // No need to change hasNoEnumerableProperties. See comment in ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes.
            Assert(!arr->GetHasNoEnumerableProperties());

            descriptor->Attributes = (descriptor->Attributes & ~PropertyDynamicTypeDefaults) | (attributes & PropertyDynamicTypeDefaults);
            if (!(descriptor->Attributes & PropertyWritable))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 514\n");
                this->ClearHasOnlyWritableDataProperties();
                if(this->GetFlags() & this->IsPrototypeFlag)
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 517\n");
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
            return true;
        }
        else if (HasDataItem(arr, index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 524\n");
            // No need to change hasNoEnumerableProperties. See comment in ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes.
            Assert(!arr->GetHasNoEnumerableProperties());

            indexPropertyMap->Add(index, IndexPropertyDescriptor(attributes & PropertyDynamicTypeDefaults));
            if (!(attributes & PropertyWritable))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 530\n");
                this->ClearHasOnlyWritableDataProperties();
                if(this->GetFlags() & this->IsPrototypeFlag)
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 533\n");
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
            return true;
        }

        return false;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemAccessors(ES5Array* arr, DynamicObject* instance, uint32 index, Var getter, Var setter)
    {
        // Reject if we need to grow non-writable length
        if (!CanSetItemAt(arr, index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 548\n");
            return false;
        }

        JavascriptLibrary* lib = instance->GetLibrary();
        if (getter)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 554\n");
            getter = this->CanonicalizeAccessor(getter, lib);
        }
        if (setter)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 558\n");
            setter = this->CanonicalizeAccessor(setter, lib);
        }

        // conversion from data-property to accessor property
        arr->DirectDeleteItemAt<Var>(index);

        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 567\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 569\n");
                descriptor->Attributes = PropertyDynamicTypeDefaults;
            }
            if (getter)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 573\n");
                descriptor->Getter = getter;
            }
            if (setter)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 577\n");
                descriptor->Setter = setter;
            }
        }
        else
        {
            indexPropertyMap->Add(index, IndexPropertyDescriptor(getter, setter));
        }

        if (arr->GetLength() <= index)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 587\n");
            uint32 newLength = index;
            UInt32Math::Inc(newLength);
            arr->SetLength(newLength);
        }

        this->ClearHasOnlyWritableDataProperties();
        if(this->GetFlags() & this->IsPrototypeFlag)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 595\n");
            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetItemAccessors(ES5Array* arr, DynamicObject* instance, uint32 index, Var* getter, Var* setter)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 603\n");
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 606\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 608\n");
                return false;
            }

            if (!HasDataItem(arr, index)) // if not shadowed by data item
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 613\n");
                *getter = descriptor->Getter;
                *setter = descriptor->Setter;
                return descriptor->Getter || descriptor->Setter;
            }
        }

        return false;
    }

    // Check if this array can set item at the given index.
    template <class T>
    bool ES5ArrayTypeHandlerBase<T>::CanSetItemAt(ES5Array* arr, uint32 index) const
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 626\n");
        return IsLengthWritable() || index < arr->GetLength();
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::DeleteItem(ES5Array* arr, DynamicObject* instance, uint32 index, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 632\n");
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 635\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 637\n");
                return true;
            }
            else if (!(descriptor->Attributes & PropertyConfigurable))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 641\n");
                JavascriptError::ThrowCantDelete(propertyOperationFlags, instance->GetScriptContext(), TaggedInt::ToString(index, instance->GetScriptContext())->GetString());

                return false;
            }

            arr->DirectDeleteItemAt<Var>(index);
            descriptor->Getter = nullptr;
            descriptor->Setter = nullptr;
            descriptor->Attributes = PropertyDeleted | PropertyWritable | PropertyConfigurable;
            return true;
        }

        // Not in attribute map
        if (!(GetDataItemAttributes() & PropertyConfigurable))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 656\n");
            return !HasDataItem(arr, index); // CantDelete
        }
        return arr->DirectDeleteItemAt<Var>(index);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetItem(ES5Array* arr, DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 664\n");
        if (arr->DirectGetItemAt<Var>(index, value))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 666\n");
            return true;
        }

        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 672\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 674\n");
                *value = requestContext->GetMissingItemResult();
                return false;
            }

            if (descriptor->Getter)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 680\n");
                RecyclableObject* func = RecyclableObject::FromVar(descriptor->Getter);
                *value = Js::JavascriptOperators::CallGetter(func, originalInstance, requestContext);
            }
            else
            {
                *value = requestContext->GetMissingItemResult();
            }
            return true;
        }

        *value = requestContext->GetMissingItemResult();
        return false;
    }

    template <class T>
    DescriptorFlags ES5ArrayTypeHandlerBase<T>::GetItemSetter(ES5Array* arr, DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 697\n");
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 700\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 702\n");
                return None;
            }

            if (HasDataItem(ES5Array::FromVar(instance), index))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 707\n");
                // not a setter but shadows
                return (descriptor->Attributes & PropertyWritable) ? WritableData : Data;
            }
            else if (descriptor->Setter)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 712\n");
                *setterValue = descriptor->Setter;
                return Accessor;
            }
        }
        else if (HasDataItem(ES5Array::FromVar(instance), index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 718\n");
            return (GetDataItemAttributes() & PropertyWritable) ? WritableData : Data;
        }

        return None;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 727\n");
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 index;

        if (noRedecl != nullptr)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 732\n");
            *noRedecl = false;
        }

        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 737\n");
            // Call my version of HasItem
            return ES5ArrayTypeHandlerBase<T>::HasItem(instance, index);
        }

        return __super::HasProperty(instance, propertyId, noRedecl);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 747\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        return __super::HasProperty(instance, propertyNameString);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 756\n");
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 760\n");
            return GetItem(ES5Array::FromVar(instance), instance, index, value, requestContext);
        }

        return __super::GetProperty(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 769\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        return __super::GetProperty(instance, originalInstance, propertyNameString, value, info, requestContext);
    }

    template <class T>
    DescriptorFlags ES5ArrayTypeHandlerBase<T>::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 778\n");
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 783\n");
            PropertyValueInfo::SetNoCache(info, instance);
            return ES5ArrayTypeHandlerBase<T>::GetItemSetter(instance, index, setterValue, requestContext);
        }

        return __super::GetSetter(instance, propertyId, setterValue, info, requestContext);
    }

    template <class T>
    DescriptorFlags ES5ArrayTypeHandlerBase<T>::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 793\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        return __super::GetSetter(instance, propertyNameString, setterValue, info, requestContext);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 802\n");
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 806\n");
            return DeleteItem(ES5Array::FromVar(instance), instance, index, flags);
        }

        return __super::DeleteProperty(instance, propertyId, flags);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::HasItem(DynamicObject* instance, uint32 index)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 815\n");
        return HasItem(ES5Array::FromVar(instance), index);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 821\n");
        return SetItem(ES5Array::FromVar(instance), instance, index, value, flags);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 827\n");
        return SetItemWithAttributes(ES5Array::FromVar(instance), instance, index, value, attributes);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 833\n");
        return SetItemAttributes(ES5Array::FromVar(instance), instance, index, attributes);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 839\n");
        return SetItemAccessors(ES5Array::FromVar(instance), instance, index, getter, setter);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::DeleteItem(DynamicObject* instance, uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 845\n");
        return DeleteItem(ES5Array::FromVar(instance), instance, index, flags);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetItem(DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 851\n");
        return GetItem(ES5Array::FromVar(instance), instance, originalInstance, index, value, requestContext);
    }

    template <class T>
    DescriptorFlags ES5ArrayTypeHandlerBase<T>::GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 857\n");
        return GetItemSetter(ES5Array::FromVar(instance), instance, index, setterValue, requestContext);
    }

    template <class T>
    bool ES5ArrayTypeHandlerBase<T>::IsLengthWritable() const
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 863\n");
        return lengthWritable;
    }

    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetLengthWritable(bool writable)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 869\n");
        lengthWritable = writable;

        if (!writable)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 873\n");
            this->ClearHasOnlyWritableDataProperties();
        }
    }

    //
    // Try to delete the range [firstKey, length) from right to left, stop if running into an element whose
    // [[CanDelete]] is false. Return the index where [index, ...) can all be deleted.
    //
    // Note that this helper method finds the max range to delete but may or may not delete the data items.
    // The caller needs to call JavascriptArray::SetLength to trim the data items.
    //
    template <class T>
    uint32 ES5ArrayTypeHandlerBase<T>::DeleteDownTo(ES5Array* arr, uint32 first, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 887\n");
        Assert(first < arr->GetLength()); // Only called when newLen < oldLen

        // If the number of elements to be deleted is small, iterate on it.
        uint32 count = arr->GetLength() - first;
        if (count < 5)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 893\n");
            uint32 oldLen = arr->GetLength();
            while (first < oldLen)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 896\n");
                if (!arr->DeleteItem(oldLen - 1, propertyOperationFlags))
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 898\n");
                    break;
                }
                --oldLen;
            }

            return oldLen;
        }

        // If data items are [[CanDelete]], check attribute map only.
        if (GetDataItemAttributes() & PropertyConfigurable)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 909\n");
            return indexPropertyMap->DeleteDownTo(first);
        }
        else
        {
            // The array isSealed. No existing item can be deleted. Look for the max index.
            uint32 lastIndex;
            if (indexPropertyMap->TryGetLastIndex(&lastIndex) && lastIndex >= first)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 917\n");
                first = lastIndex + 1;
            }
            if (TryGetLastDataItemIndex(arr, first, &lastIndex))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 921\n");
                first = lastIndex + 1;
            }
            return first;
        }
    }

    //
    // Try get the last data item index in the range of [first, length).
    //
    template <class T>
    bool ES5ArrayTypeHandlerBase<T>::TryGetLastDataItemIndex(ES5Array* arr, uint32 first, uint32* lastIndex)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 933\n");
        uint32 index = JavascriptArray::InvalidIndex;

        JavascriptArray::ArrayElementEnumerator e(arr, first);
        while (e.MoveNext<Var>())
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 938\n");
            index = e.GetIndex();
        }

        if (index != JavascriptArray::InvalidIndex)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 943\n");
            *lastIndex = index;
            return true;
        }

        return false;
    }

    template <class T>
    void ES5ArrayTypeHandlerBase<T>::SetLength(ES5Array* arr, uint32 newLen, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 953\n");
        Assert(IsLengthWritable()); // Should have already checked

        if (newLen < arr->GetLength())
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 957\n");
            newLen = DeleteDownTo(arr, newLen, propertyOperationFlags); // Result newLen might be different
        }

        // Trim data items and set length
        arr->SetLength(newLen);

        //
        // Strict mode TODO: In strict mode we may need to throw if we cannot delete to
        // requested newLen (ES5 15.4.5.1 3.l.III.4).
        //
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsAttributeSet(uint32 index, PropertyAttributes attr)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 972\n");
        IndexPropertyDescriptor* descriptor;
        if (indexPropertyMap->TryGetReference(index, &descriptor))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 975\n");
            if (!(descriptor->Attributes & PropertyDeleted))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 977\n");
                return descriptor->Attributes & attr;
            }
        }
        else
        {
            return GetDataItemAttributes() & attr;
        }

        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsAttributeSet(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attr, BOOL& isNumericPropertyId)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 991\n");
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        isNumericPropertyId = scriptContext->IsNumericPropertyId(propertyId, &index);
        if (isNumericPropertyId)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 997\n");
            return IsAttributeSet(index, attr);
        }

        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::UpdateAttribute(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attr, BOOL value, BOOL& isNumericPropertyId)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1006\n");
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        isNumericPropertyId = scriptContext->IsNumericPropertyId(propertyId, &index);
        if (isNumericPropertyId)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1012\n");
            IndexPropertyDescriptor* descriptor;
            if (indexPropertyMap->TryGetReference(index, &descriptor))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1015\n");
                if (descriptor->Attributes & PropertyDeleted)
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 1017\n");
                    return false;
                }

                if (value)
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 1022\n");
                    descriptor->Attributes |= attr;
                }
                else
                {
                    descriptor->Attributes &= (~attr);
                    if (!(descriptor->Attributes & PropertyWritable))
                    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1029\n");
                        this->ClearHasOnlyWritableDataProperties();
                        if(this->GetFlags() & this->IsPrototypeFlag)
                        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1032\n");
                            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                        }
                    }
                }
            }
            else
            {
                if (!HasDataItem(ES5Array::FromVar(instance), index))
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 1041\n");
                    return false;
                }

                PropertyAttributes newAttr = GetDataItemAttributes();
                if (value)
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 1047\n");
                    newAttr |= attr;
                }
                else
                {
                    newAttr &= (~attr);
                }

                if (newAttr != GetDataItemAttributes())
                {LOGMEIN("ES5ArrayTypeHandler.cpp] 1056\n");
                    indexPropertyMap->Add(index, IndexPropertyDescriptor(newAttr));
                    if (!(newAttr & PropertyWritable))
                    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1059\n");
                        this->ClearHasOnlyWritableDataProperties();
                        if(this->GetFlags() & this->IsPrototypeFlag)
                        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1062\n");
                            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                        }
                    }
                }
            }

            return true;
        }

        return false;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsItemEnumerable(ES5Array* arr, uint32 index)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1077\n");
        return IsAttributeSet(index, PropertyEnumerable);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1083\n");
        BOOL isNumericPropertyId;
        return IsAttributeSet(instance, propertyId, PropertyEnumerable, isNumericPropertyId)
            && (isNumericPropertyId || __super::IsEnumerable(instance, propertyId));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1091\n");
        BOOL isNumericPropertyId;
        return IsAttributeSet(instance, propertyId, PropertyWritable, isNumericPropertyId)
            && (isNumericPropertyId || __super::IsWritable(instance, propertyId));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1099\n");
        BOOL isNumericPropertyId;
        return IsAttributeSet(instance, propertyId, PropertyConfigurable, isNumericPropertyId)
            && (isNumericPropertyId || __super::IsConfigurable(instance, propertyId));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1107\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1109\n");
            Assert(!value); // Can only set enumerable to false
            return true;
        }

        BOOL isNumericPropertyId;
        return UpdateAttribute(instance, propertyId, PropertyEnumerable, value, isNumericPropertyId)
            || (!isNumericPropertyId && __super::SetEnumerable(instance, propertyId, value));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1121\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1123\n");
            SetLengthWritable(value ? true : false);
            if(!value && this->GetFlags() & this->IsPrototypeFlag)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1126\n");
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
            return true;
        }

        BOOL isNumericPropertyId;
        return UpdateAttribute(instance, propertyId, PropertyWritable, value, isNumericPropertyId)
            || (!isNumericPropertyId && __super::SetWritable(instance, propertyId, value));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1139\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1141\n");
            Assert(!value); // Can only set configurable to false
            return true;
        }

        BOOL isNumericPropertyId;
        return UpdateAttribute(instance, propertyId, PropertyConfigurable, value, isNumericPropertyId)
            || (!isNumericPropertyId && __super::SetConfigurable(instance, propertyId, value));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetAccessors(DynamicObject* instance, PropertyId propertyId, Var* getter, Var* setter)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1153\n");
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1158\n");
            return GetItemAccessors(ES5Array::FromVar(instance), instance, index, getter, setter);
        }

        return __super::GetAccessors(instance, propertyId, getter, setter);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::Seal(DynamicObject* instance)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1167\n");
        IndexPropertyDescriptor* descriptor = NULL;
        for (int i = 0; i < indexPropertyMap->Count(); i++)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1170\n");
            descriptor = indexPropertyMap->GetReferenceAt(i);
            descriptor->Attributes &= (~PropertyConfigurable);
        }

        this->SetDataItemSealed(); // set shared data item attributes sealed

        return __super::Seal(instance);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1182\n");
        ES5Array* arr = ES5Array::FromVar(instance);

        for (int i = 0; i < indexPropertyMap->Count(); i++)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1186\n");
            uint32 index = indexPropertyMap->GetKeyAt(i);
            IndexPropertyDescriptor* descriptor = indexPropertyMap->GetReferenceAt(i);

            if (HasDataItem(arr, index))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1191\n");
                //Only data descriptor has Writable property
                descriptor->Attributes &= ~(PropertyWritable | PropertyConfigurable);
            }
            else
            {
                descriptor->Attributes &= ~(PropertyConfigurable);
            }
        }

        this->SetDataItemFrozen(); // set shared data item attributes frozen
        SetLengthWritable(false); // Freeze "length" as well

        return __super::FreezeImpl(instance, isConvertedType);
    }

    template <class T>
    BigDictionaryTypeHandler* ES5ArrayTypeHandlerBase<T>::NewBigDictionaryTypeHandler(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1209\n");
        return RecyclerNew(recycler, BigES5ArrayTypeHandler, recycler, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots, this);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsSealed(DynamicObject* instance)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1215\n");
        if (!__super::IsSealed(instance))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1217\n");
            return false;
        }

        for (int i = 0; i < indexPropertyMap->Count(); i++)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1222\n");
            IndexPropertyDescriptor* descriptor = indexPropertyMap->GetReferenceAt(i);
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1225\n");
                continue; // Skip deleted
            }
            if (descriptor->Attributes & PropertyConfigurable)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1229\n");
                //[[Configurable]] must be false for all properties.
                return false;
            }
        }

        // Check data item not in map
        if (this->GetDataItemAttributes() & PropertyConfigurable)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1237\n");
            if (HasAnyDataItemNotInMap(ES5Array::FromVar(instance)))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1239\n");
                return false;
            }
        }

        return true;
    }

    //
    // When arr is objectArray of an object, we should skip "length" while testing isFrozen. "length" is an
    // own property of arr, but not of the containing object.
    //
    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsObjectArrayFrozen(ES5Array* arr)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1253\n");
        if (!__super::IsFrozen(arr))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1255\n");
            return false;
        }

        for (int i = 0; i < indexPropertyMap->Count(); i++)
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1260\n");
            uint32 index = indexPropertyMap->GetKeyAt(i);
            IndexPropertyDescriptor* descriptor = indexPropertyMap->GetReferenceAt(i);

            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1265\n");
                continue; // Skip deleted
            }
            if (descriptor->Attributes & PropertyConfigurable)
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1269\n");
                return false;
            }

            if ((descriptor->Attributes & PropertyWritable) && HasDataItem(arr, index))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1274\n");
                //Only data descriptor has Writable property
                return false;
            }
        }

        // Check data item not in map
        if (this->GetDataItemAttributes() & (PropertyWritable | PropertyConfigurable))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1282\n");
            if (HasAnyDataItemNotInMap(arr))
            {LOGMEIN("ES5ArrayTypeHandler.cpp] 1284\n");
                return false;
            }
        }

        return true;
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsFrozen(DynamicObject* instance)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1294\n");
        // We need to check "length" frozen for standalone ES5Array
        if (IsLengthWritable())
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1297\n");
            return false;
        }

        return IsObjectArrayFrozen(ES5Array::FromVar(instance));
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1306\n");
        ScriptContext* scriptContext = instance->GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("ES5ArrayTypeHandler.cpp] 1311\n");
            return SetItemAttributes(ES5Array::FromVar(instance), instance, index, attributes);
        }

        return __super::SetAttributes(instance, propertyId, attributes);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::IsValidDescriptorToken(void * descriptorValidationToken) const
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1320\n");
        return indexPropertyMap->IsValidDescriptorToken(descriptorValidationToken);
    }

    template <class T>
    uint32 ES5ArrayTypeHandlerBase<T>::GetNextDescriptor(uint32 key, IndexPropertyDescriptor** descriptor, void ** descriptorValidationToken)
    {LOGMEIN("ES5ArrayTypeHandler.cpp] 1326\n");
        return indexPropertyMap->GetNextDescriptor(key, descriptor, descriptorValidationToken);
    }

    template <class T>
    BOOL ES5ArrayTypeHandlerBase<T>::GetDescriptor(uint32 index, Js::IndexPropertyDescriptor **ppDescriptor) {LOGMEIN("ES5ArrayTypeHandler.cpp] 1331\n");
        return indexPropertyMap->TryGetReference(index, ppDescriptor);
    }

    template class ES5ArrayTypeHandlerBase<PropertyIndex>;
    template class ES5ArrayTypeHandlerBase<BigPropertyIndex>;
}
