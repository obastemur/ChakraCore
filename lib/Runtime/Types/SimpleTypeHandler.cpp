//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

#include "Types/NullTypeHandler.h"
#include "Types/SimpleTypeHandler.h"

namespace Js
{
    template<size_t size>
    SimpleTypeHandler<size>::SimpleTypeHandler(SimpleTypeHandler<size> * typeHandler)
        : DynamicTypeHandler(sizeof(descriptors) / sizeof(SimplePropertyDescriptor),
            typeHandler->GetInlineSlotCapacity(), typeHandler->GetOffsetOfInlineSlots()), propertyCount(typeHandler->propertyCount)
    {TRACE_IT(67490);
        Assert(typeHandler->GetIsInlineSlotCapacityLocked());
        SetIsInlineSlotCapacityLocked();
        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67491);
            descriptors[i] = typeHandler->descriptors[i];
        }
    }

    template<size_t size>
    SimpleTypeHandler<size>::SimpleTypeHandler(Recycler*) :
         DynamicTypeHandler(sizeof(descriptors) / sizeof(SimplePropertyDescriptor)),
         propertyCount(0)
    {TRACE_IT(67492);
        SetIsInlineSlotCapacityLocked();
    }

    template<size_t size>
    SimpleTypeHandler<size>::SimpleTypeHandler(NO_WRITE_BARRIER_TAG_TYPE(const PropertyRecord* id), PropertyAttributes attributes, PropertyTypes propertyTypes, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots) :
        DynamicTypeHandler(sizeof(descriptors) / sizeof(SimplePropertyDescriptor),
        inlineSlotCapacity, offsetOfInlineSlots, DefaultFlags | IsLockedFlag | MayBecomeSharedFlag | IsSharedFlag), propertyCount(1)
    {TRACE_IT(67493);
        Assert((attributes & PropertyDeleted) == 0);
        NoWriteBarrierSet(descriptors[0].Id, id); // Used to init from global static BuiltInPropertyId
        descriptors[0].Attributes = attributes;

        Assert((propertyTypes & (PropertyTypesAll & ~PropertyTypesWritableDataOnly)) == 0);
        SetPropertyTypes(PropertyTypesWritableDataOnly, propertyTypes);
        SetIsInlineSlotCapacityLocked();
    }

    template<size_t size>
    SimpleTypeHandler<size>::SimpleTypeHandler(NO_WRITE_BARRIER_TAG_TYPE(SimplePropertyDescriptor const (&SharedFunctionPropertyDescriptors)[size]), PropertyTypes propertyTypes, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots) :
         DynamicTypeHandler(sizeof(descriptors) / sizeof(SimplePropertyDescriptor),
         inlineSlotCapacity, offsetOfInlineSlots, DefaultFlags | IsLockedFlag | MayBecomeSharedFlag | IsSharedFlag), propertyCount(size)
    {TRACE_IT(67494);
        for (size_t i = 0; i < size; i++)
        {TRACE_IT(67495);
            Assert((SharedFunctionPropertyDescriptors[i].Attributes & PropertyDeleted) == 0);
             // Used to init from global static BuiltInPropertyId
            NoWriteBarrierSet(descriptors[i].Id, SharedFunctionPropertyDescriptors[i].Id);
            descriptors[i].Attributes = SharedFunctionPropertyDescriptors[i].Attributes;
        }
        Assert((propertyTypes & (PropertyTypesAll & ~PropertyTypesWritableDataOnly)) == 0);
        SetPropertyTypes(PropertyTypesWritableDataOnly, propertyTypes);
        SetIsInlineSlotCapacityLocked();
    }

    template<size_t size>
    SimpleTypeHandler<size> * SimpleTypeHandler<size>::ConvertToNonSharedSimpleType(DynamicObject* instance)
    {TRACE_IT(67496);
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();


        CompileAssert(_countof(descriptors) == size);

        SimpleTypeHandler * newTypeHandler = RecyclerNew(recycler, SimpleTypeHandler, this);

        // Consider: Add support for fixed fields to SimpleTypeHandler when
        // non-shared.  Here we could set the instance as the singleton instance on the newly
        // created handler.

        newTypeHandler->SetFlags(IsPrototypeFlag | HasKnownSlot0Flag, this->GetFlags());
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);

        return newTypeHandler;
    }

    template<size_t size>
    template <typename T>
    T* SimpleTypeHandler<size>::ConvertToTypeHandler(DynamicObject* instance)
    {TRACE_IT(67497);
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

#if DBG
        DynamicType* oldType = instance->GetDynamicType();
#endif

        T* newTypeHandler = RecyclerNew(recycler, T, recycler, SimpleTypeHandler<size>::GetSlotCapacity(), GetInlineSlotCapacity(), GetOffsetOfInlineSlots());
        Assert(HasSingletonInstanceOnlyIfNeeded());

        bool const hasSingletonInstance = newTypeHandler->SetSingletonInstanceIfNeeded(instance);
        // If instance has a shared type, the type handler change below will induce a type transition, which
        // guarantees that any existing fast path field stores (which could quietly overwrite a fixed field
        // on this instance) will be invalidated.  It is safe to mark all fields as fixed.
        bool const allowFixedFields = hasSingletonInstance && instance->HasLockedType();

        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67498);
            Var value = instance->GetSlot(i);
            Assert(value != nullptr || IsInternalPropertyId(descriptors[i].Id->GetPropertyId()));
            bool markAsFixed = allowFixedFields && !IsInternalPropertyId(descriptors[i].Id->GetPropertyId()) &&
                (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : false);
            newTypeHandler->Add(PointerValue(descriptors[i].Id), descriptors[i].Attributes, true, markAsFixed, false, scriptContext);
        }

        newTypeHandler->SetFlags(IsPrototypeFlag | HasKnownSlot0Flag, this->GetFlags());
        // We don't expect to convert to a PathTypeHandler.  If we change this later we need to review if we want the new type handler to have locked
        // inline slot capacity, or if we want to allow shrinking of the SimpleTypeHandler's inline slot capacity.
        Assert(!newTypeHandler->IsPathTypeHandler());
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);

#if DBG
        // If we marked fields as fixed we had better forced a type transition.
        Assert(!allowFixedFields || instance->GetDynamicType() != oldType);
#endif

        return newTypeHandler;
    }

    template<size_t size>
    DictionaryTypeHandler* SimpleTypeHandler<size>::ConvertToDictionaryType(DynamicObject* instance)
    {TRACE_IT(67499);
        DictionaryTypeHandler* newTypeHandler = ConvertToTypeHandler<DictionaryTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleToDictionaryCount++;
#endif
        return newTypeHandler;
    }

    template<size_t size>
    SimpleDictionaryTypeHandler* SimpleTypeHandler<size>::ConvertToSimpleDictionaryType(DynamicObject* instance)
    {TRACE_IT(67500);
        SimpleDictionaryTypeHandler* newTypeHandler = ConvertToTypeHandler<SimpleDictionaryTypeHandler >(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleToSimpleDictionaryCount++;
#endif
        return newTypeHandler;
    }

    template<size_t size>
    ES5ArrayTypeHandler* SimpleTypeHandler<size>::ConvertToES5ArrayType(DynamicObject* instance)
    {TRACE_IT(67501);
        ES5ArrayTypeHandler* newTypeHandler = ConvertToTypeHandler<ES5ArrayTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertSimpleToDictionaryCount++;
#endif
        return newTypeHandler;
    }

    template<size_t size>
    int SimpleTypeHandler<size>::GetPropertyCount()
    {TRACE_IT(67502);
        return propertyCount;
    }

    template<size_t size>
    PropertyId SimpleTypeHandler<size>::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {TRACE_IT(67503);
        if (index < propertyCount && !(descriptors[index].Attributes & PropertyDeleted))
        {TRACE_IT(67504);
            return descriptors[index].Id->GetPropertyId();
        }
        else
        {TRACE_IT(67505);
            return Constants::NoProperty;
        }
    }

    template<size_t size>
    PropertyId SimpleTypeHandler<size>::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {TRACE_IT(67506);
        if (index < propertyCount && !(descriptors[index].Attributes & PropertyDeleted))
        {TRACE_IT(67507);
            return descriptors[index].Id->GetPropertyId();
        }
        else
        {TRACE_IT(67508);
            return Constants::NoProperty;
        }
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {TRACE_IT(67509);
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for( ; index < propertyCount; ++index )
        {TRACE_IT(67510);
            PropertyAttributes attribs = descriptors[index].Attributes;
            if( !(attribs & PropertyDeleted) && (!!(flags & EnumeratorFlags::EnumNonEnumerable) || (attribs & PropertyEnumerable)))
            {TRACE_IT(67511);
                const PropertyRecord* propertyRecord = descriptors[index].Id;

                // Skip this property if it is a symbol and we are not including symbol properties
                if (!(flags & EnumeratorFlags::EnumSymbols) && propertyRecord->IsSymbol())
                {TRACE_IT(67512);
                    continue;
                }

                if (attributes != nullptr)
                {TRACE_IT(67513);
                    *attributes = attribs;
                }

                *propertyId = propertyRecord->GetPropertyId();
                PropertyString* propertyString = scriptContext->GetPropertyString(*propertyId);
                *propertyStringName = propertyString;
                if (attribs & PropertyWritable)
                {TRACE_IT(67514);
                    uint16 inlineOrAuxSlotIndex;
                    bool isInlineSlot;
                    PropertyIndexToInlineOrAuxSlotIndex(index, &inlineOrAuxSlotIndex, &isInlineSlot);

                    propertyString->UpdateCache(type, inlineOrAuxSlotIndex, isInlineSlot, true);
                }
                else
                {TRACE_IT(67515);
#ifdef DEBUG
                    PropertyCache const* cache = propertyString->GetPropertyCache();
                    Assert(!cache || cache->type != type);
#endif
                }

                return TRUE;
            }
        }

        return FALSE;
    }

    template<size_t size>
    PropertyIndex SimpleTypeHandler<size>::GetPropertyIndex(PropertyRecord const* propertyRecord)
    {TRACE_IT(67516);
        int index;
        if (GetDescriptor(propertyRecord->GetPropertyId(), &index) && !(descriptors[index].Attributes & PropertyDeleted))
        {TRACE_IT(67517);
            return (PropertyIndex)index;
        }
        return Constants::NoSlot;
    }

    template<size_t size>
    bool SimpleTypeHandler<size>::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {TRACE_IT(67518);
        int index;
        if (GetDescriptor(propertyRecord->GetPropertyId(), &index) && !(descriptors[index].Attributes & PropertyDeleted))
        {TRACE_IT(67519);
            info.slotIndex = AdjustSlotIndexForInlineSlots((PropertyIndex)index);
            info.isWritable = !!(descriptors[index].Attributes & PropertyWritable);
            return true;
        }
        else
        {TRACE_IT(67520);
            info.slotIndex = Constants::NoSlot;
            info.isWritable = true;
            return false;
        }
    }

    template<size_t size>
    bool SimpleTypeHandler<size>::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {TRACE_IT(67521);
        Js::EquivalentPropertyEntry* properties = record.properties;
        for (uint pi = 0; pi < record.propertyCount; pi++)
        {TRACE_IT(67522);
            const EquivalentPropertyEntry* refInfo = &properties[pi];
            if (!this->SimpleTypeHandler<size>::IsObjTypeSpecEquivalent(type, refInfo))
            {TRACE_IT(67523);
                failedPropertyIndex = pi;
                return false;
            }
        }

        return true;
    }

    template<size_t size>
    bool SimpleTypeHandler<size>::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {TRACE_IT(67524);
        if (this->propertyCount > 0)
        {TRACE_IT(67525);
            for (int i = 0; i < this->propertyCount; i++)
            {TRACE_IT(67526);
                SimplePropertyDescriptor* descriptor = &this->descriptors[i];
                Js::PropertyId propertyId = descriptor->Id->GetPropertyId();

                if (entry->propertyId == propertyId && !(descriptor->Attributes & PropertyDeleted))
                {TRACE_IT(67527);
                    Js::PropertyIndex relSlotIndex = AdjustValidSlotIndexForInlineSlots(static_cast<PropertyIndex>(0));
                    if (relSlotIndex != entry->slotIndex ||
                        entry->isAuxSlot != (GetInlineSlotCapacity() == 0) ||
                        (entry->mustBeWritable && !(descriptor->Attributes & PropertyWritable)))
                    {TRACE_IT(67528);
                        return false;
                    }
                }
                else
                {TRACE_IT(67529);
                    if (entry->slotIndex != Constants::NoSlot || entry->mustBeWritable)
                    {TRACE_IT(67530);
                        return false;
                    }
                }
            }
        }
        else
        {TRACE_IT(67531);
            if (entry->slotIndex != Constants::NoSlot || entry->mustBeWritable)
            {TRACE_IT(67532);
                return false;
            }
        }

        return true;
    }


    template<size_t size>
    BOOL SimpleTypeHandler<size>::HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *noRedecl)
    {TRACE_IT(67533);
        if (noRedecl != nullptr)
        {TRACE_IT(67534);
            *noRedecl = false;
        }

        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67535);
            if (descriptors[i].Id->GetPropertyId() == propertyId)
            {TRACE_IT(67536);
                if (descriptors[i].Attributes & PropertyDeleted)
                {TRACE_IT(67537);
                    return false;
                }
                if (noRedecl && descriptors[i].Attributes & PropertyNoRedecl)
                {TRACE_IT(67538);
                    *noRedecl = true;
                }
                return true;
            }
        }

        // Check numeric propertyId only if objectArray available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(67539);
            return SimpleTypeHandler<size>::HasItem(instance, indexVal);
        }

        return false;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {TRACE_IT(67540);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67541);
            if (descriptors[i].Id->Equals(propertyName))
            {TRACE_IT(67542);
                return !(descriptors[i].Attributes & PropertyDeleted);
            }
        }

        return false;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(67543);
        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67544);
            if (descriptors[i].Id->GetPropertyId() == propertyId)
            {TRACE_IT(67545);
                if (descriptors[i].Attributes & PropertyDeleted)
                {TRACE_IT(67546);
                    *value = requestContext->GetMissingPropertyResult();
                    return false;
                }
                *value = instance->GetSlot(i);
                PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(i), descriptors[i].Attributes);
                return true;
            }
        }

        // Check numeric propertyId only if objectArray available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(67547);
            return SimpleTypeHandler<size>::GetItem(instance, originalInstance, indexVal, value, scriptContext);
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(67548);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67549);
            if (descriptors[i].Id->Equals(propertyName))
            {TRACE_IT(67550);
                if (descriptors[i].Attributes & PropertyDeleted)
                {TRACE_IT(67551);
                    *value = requestContext->GetMissingPropertyResult();
                    return false;
                }
                *value = instance->GetSlot(i);
                PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(i), descriptors[i].Attributes);
                return true;
            }
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(67552);
        ScriptContext* scriptContext = instance->GetScriptContext();
        int index;

        JavascriptLibrary::CheckAndInvalidateIsConcatSpreadableCache(propertyId, scriptContext);

        if (GetDescriptor(propertyId, &index))
        {TRACE_IT(67553);
            if (descriptors[index].Attributes & PropertyDeleted)
            {TRACE_IT(67554);
                // A locked type should not have deleted properties
                Assert(!GetIsLocked());
                descriptors[index].Attributes = PropertyDynamicTypeDefaults;
                instance->SetHasNoEnumerableProperties(false);
            }
            else if (!(descriptors[index].Attributes & PropertyWritable))
            {TRACE_IT(67555);
                JavascriptError::ThrowCantAssignIfStrictMode(flags, scriptContext);

                PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(index), descriptors[index].Attributes); // Try to cache property info even if not writable
                return false;
            }

            SetSlotUnchecked(instance, index, value);
            PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(index), descriptors[index].Attributes);
            SetPropertyUpdateSideEffect(instance, propertyId, value, SideEffects_Any);
            return true;
        }

        // Always check numeric propertyId. This may create objectArray.
        uint32 indexVal;
        if (scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(67556);
            return SimpleTypeHandler<size>::SetItem(instance, indexVal, value, flags);
        }

        return this->AddProperty(instance, propertyId, value, PropertyDynamicTypeDefaults, info, flags, SideEffects_Any);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(67557);
        // Either the property exists in the dictionary, in which case a PropertyRecord for it exists,
        // or we have to add it to the dictionary, in which case we need to get or create a PropertyRecord.
        // Thus, just get or create one and call the PropertyId overload of SetProperty.
        PropertyRecord const* propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return SimpleTypeHandler<size>::SetProperty(instance, propertyRecord->GetPropertyId(), value, flags, info);
    }

    template<size_t size>
    DescriptorFlags SimpleTypeHandler<size>::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(67558);
        int index;
        PropertyValueInfo::SetNoCache(info, instance);
        if (GetDescriptor(propertyId, &index))
        {TRACE_IT(67559);
            if (descriptors[index].Attributes & PropertyDeleted)
            {TRACE_IT(67560);
                return None;
            }
            return (descriptors[index].Attributes & PropertyWritable) ? WritableData : Data;
        }

        uint32 indexVal;
        if (instance->GetScriptContext()->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(67561);
            return SimpleTypeHandler<size>::GetItemSetter(instance, indexVal, setterValue, requestContext);
        }

        return None;
    }

    template<size_t size>
    DescriptorFlags SimpleTypeHandler<size>::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(67562);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67563);
            if (descriptors[i].Id->Equals(propertyName))
            {TRACE_IT(67564);
                if (descriptors[i].Attributes & PropertyDeleted)
                {TRACE_IT(67565);
                    return None;
                }
                return (descriptors[i].Attributes & PropertyWritable) ? WritableData : Data;
            }
        }

        return None;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(67566);
        ScriptContext* scriptContext = instance->GetScriptContext();
        int index;
        if (GetDescriptor(propertyId, &index))
        {TRACE_IT(67567);
            if (descriptors[index].Attributes & PropertyDeleted)
            {TRACE_IT(67568);
                // A locked type should not have deleted properties
                Assert(!GetIsLocked());
                return true;
            }
            if (!(descriptors[index].Attributes & PropertyConfigurable))
            {TRACE_IT(67569);
                JavascriptError::ThrowCantDelete(propertyOperationFlags, scriptContext, scriptContext->GetPropertyName(propertyId)->GetBuffer());

                return false;
            }

            if ((this->GetFlags() & IsPrototypeFlag)
                || JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(instance, propertyId))
            {TRACE_IT(67570);
                // We don't evolve dictionary types when deleting a field, so we need to invalidate prototype caches.
                // We only have to do this though if the current type is used as a prototype, or the current property
                // is found on the prototype chain.)
                scriptContext->InvalidateProtoCaches(propertyId);
            }

            instance->ChangeType();


            CompileAssert(_countof(descriptors) == size);
            if (size > 1)
            {
                SetAttribute(instance, index, PropertyDeleted);
            }
            else
            {
                SetSlotUnchecked(instance, index, nullptr);

                NullTypeHandlerBase* nullTypeHandler = ((this->GetFlags() & IsPrototypeFlag) != 0) ?
                    (NullTypeHandlerBase*)NullTypeHandler<true>::GetDefaultInstance() : (NullTypeHandlerBase*)NullTypeHandler<false>::GetDefaultInstance();
                if (instance->HasReadOnlyPropertiesInvisibleToTypeHandler())
                {TRACE_IT(67571);
                    nullTypeHandler->ClearHasOnlyWritableDataProperties();
                }
                SetInstanceTypeHandler(instance, nullTypeHandler, false);
            }

            return true;
        }

        // Check numeric propertyId only if objectArray available
        uint32 indexVal;
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(67572);
            return SimpleTypeHandler<size>::DeleteItem(instance, indexVal, propertyOperationFlags);
        }

        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(67573);
        int index;
        if (!GetDescriptor(propertyId, &index))
        {TRACE_IT(67574);
            return true;
        }
        return descriptors[index].Attributes & PropertyEnumerable;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(67575);
        int index;
        if (!GetDescriptor(propertyId, &index))
        {TRACE_IT(67576);
            return true;
        }
        return descriptors[index].Attributes & PropertyWritable;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(67577);
        int index;
        if (!GetDescriptor(propertyId, &index))
        {TRACE_IT(67578);
            return true;
        }
        return descriptors[index].Attributes & PropertyConfigurable;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(67579);
        int index;
        if (!GetDescriptor(propertyId, &index))
        {TRACE_IT(67580);
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            ScriptContext* scriptContext = instance->GetScriptContext();
            uint32 indexVal;
            if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
            {TRACE_IT(67581);
                return SimpleTypeHandler<size>::ConvertToTypeWithItemAttributes(instance)
                    ->SetEnumerable(instance, propertyId, value);
            }
            return true;
        }

        if (value)
        {
            if (SetAttribute(instance, index, PropertyEnumerable))
            {TRACE_IT(67582);
                instance->SetHasNoEnumerableProperties(false);
            }
        }
        else
        {
            ClearAttribute(instance, index, PropertyEnumerable);
        }
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(67583);
        int index;
        if (!GetDescriptor(propertyId, &index))
        {TRACE_IT(67584);
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            ScriptContext* scriptContext = instance->GetScriptContext();
            uint32 indexVal;
            if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
            {TRACE_IT(67585);
                return SimpleTypeHandler<size>::ConvertToTypeWithItemAttributes(instance)
                    ->SetWritable(instance, propertyId, value);
            }
            return true;
        }

        const Type* oldType = instance->GetType();
        if (value)
        {
            if (SetAttribute(instance, index, PropertyWritable))
            {TRACE_IT(67586);
                instance->ChangeTypeIf(oldType); // Ensure type change to invalidate caches
            }
        }
        else
        {
            if (ClearAttribute(instance, index, PropertyWritable))
            {TRACE_IT(67587);
                instance->ChangeTypeIf(oldType); // Ensure type change to invalidate caches

                // Clearing the attribute may have changed the type handler, so make sure
                // we access the current one.
                DynamicTypeHandler* const typeHandler = GetCurrentTypeHandler(instance);
                typeHandler->ClearHasOnlyWritableDataProperties();
                if (typeHandler->GetFlags() & IsPrototypeFlag)
                {TRACE_IT(67588);
                    instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
        }
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(67589);
        int index;
        if (!GetDescriptor(propertyId, &index))
        {TRACE_IT(67590);
            // Upgrade type handler if set objectArray item attribute.
            // Only check numeric propertyId if objectArray available.
            ScriptContext* scriptContext = instance->GetScriptContext();
            uint32 indexVal;
            if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
            {TRACE_IT(67591);
                return SimpleTypeHandler<size>::ConvertToTypeWithItemAttributes(instance)
                    ->SetConfigurable(instance, propertyId, value);
            }
            return true;
        }

        if (value)
        {
            SetAttribute(instance, index, PropertyConfigurable);
        }
        else
        {
            ClearAttribute(instance, index, PropertyConfigurable);
        }
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::GetDescriptor(PropertyId propertyId, int * index)
    {TRACE_IT(67592);
        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67593);
            if (descriptors[i].Id->GetPropertyId() == propertyId)
            {TRACE_IT(67594);
                *index = i;
                return true;
            }
        }
        return false;
    }

    //
    // Set an attribute bit. Return true if change is made.
    //
    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetAttribute(DynamicObject* instance, int index, PropertyAttributes attribute)
    {TRACE_IT(67595);
        if (descriptors[index].Attributes & PropertyDeleted)
        {TRACE_IT(67596);
            // A locked type should not have deleted properties
            Assert(!GetIsLocked());
            return false;
        }

        PropertyAttributes attributes = descriptors[index].Attributes;
        attributes |= attribute;
        if (attributes == descriptors[index].Attributes)
        {TRACE_IT(67597);
            return false;
        }

        // If the type is locked we must force type transition to invalidate any potential property string
        // caches used in snapshot enumeration.
        if (GetIsLocked())
        {TRACE_IT(67598);
#if DBG
            DynamicType* oldType = instance->GetDynamicType();
#endif
            // This changes TypeHandler, but non-necessarily Type.
            this->ConvertToNonSharedSimpleType(instance)->descriptors[index].Attributes = attributes;
#if DBG
            Assert(!oldType->GetIsLocked() || instance->GetDynamicType() != oldType);
#endif
        }
        else
        {TRACE_IT(67599);
            descriptors[index].Attributes = attributes;
        }
        return true;
    }

    //
    // Clear an attribute bit. Return true if change is made.
    //
    template<size_t size>
    BOOL SimpleTypeHandler<size>::ClearAttribute(DynamicObject* instance, int index, PropertyAttributes attribute)
    {TRACE_IT(67600);
        if (descriptors[index].Attributes & PropertyDeleted)
        {TRACE_IT(67601);
            // A locked type should not have deleted properties
            Assert(!GetIsLocked());
            return false;
        }

        PropertyAttributes attributes = descriptors[index].Attributes;
        attributes &= ~attribute;
        if (attributes == descriptors[index].Attributes)
        {TRACE_IT(67602);
            return false;
        }

        // If the type is locked we must force type transition to invalidate any potential property string
        // caches used in snapshot enumeration.
        if (GetIsLocked())
        {TRACE_IT(67603);
#if DBG
            DynamicType* oldType = instance->GetDynamicType();
#endif
            // This changes TypeHandler, but non-necessarily Type.
            this->ConvertToNonSharedSimpleType(instance)->descriptors[index].Attributes = attributes;
#if DBG
            Assert(!oldType->GetIsLocked() || instance->GetDynamicType() != oldType);
#endif
        }
        else
        {TRACE_IT(67604);
            descriptors[index].Attributes = attributes;
        }
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(67605);
        return ConvertToDictionaryType(instance)->SetAccessors(instance, propertyId, getter, setter, flags);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::PreventExtensions(DynamicObject* instance)
    {TRACE_IT(67606);
        return ConvertToDictionaryType(instance)->PreventExtensions(instance);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::Seal(DynamicObject* instance)
    {TRACE_IT(67607);
        return ConvertToDictionaryType(instance)->Seal(instance);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {TRACE_IT(67608);
        return ConvertToDictionaryType(instance)->Freeze(instance, true);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(67609);
        int index;
        if (GetDescriptor(propertyId, &index))
        {TRACE_IT(67610);
            if (descriptors[index].Attributes != attributes)
            {TRACE_IT(67611);
                SimpleTypeHandler * typeHandler = this;
                if (GetIsLocked())
                {TRACE_IT(67612);
#if DBG
                    DynamicType* oldType = instance->GetDynamicType();
#endif
                    typeHandler = this->ConvertToNonSharedSimpleType(instance);
#if DBG
                    Assert(!oldType->GetIsLocked() || instance->GetDynamicType() != oldType);
#endif
                }
                typeHandler->descriptors[index].Attributes = attributes;
                if (attributes & PropertyEnumerable)
                {TRACE_IT(67613);
                    instance->SetHasNoEnumerableProperties(false);
                }
                if (!(attributes & PropertyWritable))
                {TRACE_IT(67614);
                    typeHandler->ClearHasOnlyWritableDataProperties();
                    if (typeHandler->GetFlags() & IsPrototypeFlag)
                    {TRACE_IT(67615);
                        instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                        instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                    }
                }
            }
            SetSlotUnchecked(instance, index, value);
            PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(index), descriptors[index].Attributes);
            SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
            return true;
        }

        // Always check numeric propertyId. May create objectArray.
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 indexVal;
        if (scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(67616);
            return SimpleTypeHandler<size>::SetItemWithAttributes(instance, indexVal, value, attributes);
        }

        return this->AddProperty(instance, propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {TRACE_IT(67617);
        for (int i = 0; i < propertyCount; i++)
        {TRACE_IT(67618);
            if (descriptors[i].Id->GetPropertyId() == propertyId)
            {TRACE_IT(67619);
                if (descriptors[i].Attributes & PropertyDeleted)
                {TRACE_IT(67620);
                    return true;
                }

                descriptors[i].Attributes = (descriptors[i].Attributes & ~PropertyDynamicTypeDefaults) | (attributes & PropertyDynamicTypeDefaults);
                if (descriptors[i].Attributes & PropertyEnumerable)
                {TRACE_IT(67621);
                    instance->SetHasNoEnumerableProperties(false);
                }
                if (!(descriptors[i].Attributes & PropertyWritable))
                {TRACE_IT(67622);
                    this->ClearHasOnlyWritableDataProperties();
                    if (GetFlags() & IsPrototypeFlag)
                    {TRACE_IT(67623);
                        instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                        instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                    }
                }
                return true;
            }
        }

        // Check numeric propertyId only if objectArray available
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 indexVal;
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(67624);
            return SimpleTypeHandler<size>::SetItemAttributes(instance, indexVal, attributes);
        }

        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {TRACE_IT(67625);
        if (index >= propertyCount) {TRACE_IT(67626); return false; }
        Assert(descriptors[index].Id->GetPropertyId() == propertyId);
        if (descriptors[index].Attributes & PropertyDeleted)
        {TRACE_IT(67627);
            return false;
        }
        *attributes = descriptors[index].Attributes & PropertyDynamicTypeDefaults;
        return true;
    }

    template<size_t size>
    BOOL SimpleTypeHandler<size>::AddProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(67628);
        ScriptContext* scriptContext = instance->GetScriptContext();

#if DBG
        int index;
        uint32 indexVal;
        Assert(!GetDescriptor(propertyId, &index));
        Assert(!scriptContext->IsNumericPropertyId(propertyId, &indexVal));
#endif

        if (propertyCount >= sizeof(descriptors)/sizeof(SimplePropertyDescriptor))
        {TRACE_IT(67629);
            Assert(propertyId != Constants::NoProperty);
            PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
            return ConvertToSimpleDictionaryType(instance)->AddProperty(instance, propertyRecord, value, attributes, info, flags, possibleSideEffects);
        }

        descriptors[propertyCount].Id = scriptContext->GetPropertyName(propertyId);
        descriptors[propertyCount].Attributes = attributes;
        if (attributes & PropertyEnumerable)
        {TRACE_IT(67630);
            instance->SetHasNoEnumerableProperties(false);
        }
        if (!(attributes & PropertyWritable))
        {TRACE_IT(67631);
            this->ClearHasOnlyWritableDataProperties();
            if (GetFlags() & IsPrototypeFlag)
            {TRACE_IT(67632);
                instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyId);
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }
        SetSlotUnchecked(instance, propertyCount, value);
        PropertyValueInfo::Set(info, instance, static_cast<PropertyIndex>(propertyCount), attributes);
        propertyCount++;

        if ((this->GetFlags() && IsPrototypeFlag)
            || JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(instance, propertyId))
        {TRACE_IT(67633);
            scriptContext->InvalidateProtoCaches(propertyId);
        }
        SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
        return true;
    }

    template<size_t size>
    void SimpleTypeHandler<size>::SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields)
    {TRACE_IT(67634);
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.

        // We can ignore invalidateFixedFields, because SimpleTypeHandler doesn't support fixed fields at this point.
        Js::RecyclableObject* undefined = instance->GetLibrary()->GetUndefined();
        for (int propertyIndex = 0; propertyIndex < this->propertyCount; propertyIndex++)
        {
            SetSlotUnchecked(instance, propertyIndex, undefined);
        }
    }

    template<size_t size>
    void SimpleTypeHandler<size>::MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields)
    {TRACE_IT(67635);
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.

        // We can ignore invalidateFixedFields, because SimpleTypeHandler doesn't support fixed fields at this point.
        for (int propertyIndex = 0; propertyIndex < this->propertyCount; propertyIndex++)
        {
            SetSlotUnchecked(instance, propertyIndex, CrossSite::MarshalVar(targetScriptContext, GetSlot(instance, propertyIndex)));
        }
    }

    template<size_t size>
    DynamicTypeHandler* SimpleTypeHandler<size>::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {TRACE_IT(67636);
        return JavascriptArray::Is(instance) ?
            ConvertToES5ArrayType(instance) : ConvertToDictionaryType(instance);
    }

    template<size_t size>
    void SimpleTypeHandler<size>::SetIsPrototype(DynamicObject* instance)
    {TRACE_IT(67637);
        // Don't return if IsPrototypeFlag is set, because we may still need to do a type transition and
        // set fixed bits.  If this handler is shared, this instance may not even be a prototype yet.
        // In this case we may need to convert to a non-shared type handler.
        if (!ChangeTypeOnProto() && !(GetIsOrMayBecomeShared() && IsolatePrototypes()))
        {TRACE_IT(67638);
            return;
        }

        ConvertToSimpleDictionaryType(instance)->SetIsPrototype(instance);
    }

#if DBG
    template<size_t size>
    bool SimpleTypeHandler<size>::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {TRACE_IT(67639);
        Assert(!allowLetConst);
        int index;
        if (GetDescriptor(propertyId, &index))
        {TRACE_IT(67640);
            return true;
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }
#endif

#if ENABLE_TTD
    template<size_t size>
    void SimpleTypeHandler<size>::MarkObjectSlots_TTD(TTD::SnapshotExtractor* extractor, DynamicObject* obj) const
    {TRACE_IT(67641);
        uint32 plength = this->propertyCount;

        for(uint32 index = 0; index < plength; ++index)
        {TRACE_IT(67642);
            Js::PropertyId pid = this->descriptors[index].Id->GetPropertyId();

            if(DynamicTypeHandler::ShouldMarkPropertyId_TTD(pid) & !(this->descriptors[index].Attributes & PropertyDeleted))
            {TRACE_IT(67643);
                Js::Var value = obj->GetSlot(index);
                extractor->MarkVisitVar(value);
            }
        }
    }

    template<size_t size>
    uint32 SimpleTypeHandler<size>::ExtractSlotInfo_TTD(TTD::NSSnapType::SnapHandlerPropertyEntry* entryInfo, ThreadContext* threadContext, TTD::SlabAllocator& alloc) const
    {TRACE_IT(67644);
        uint32 plength = this->propertyCount;

        for(uint32 index = 0; index < plength; ++index)
        {TRACE_IT(67645);
            TTD::NSSnapType::ExtractSnapPropertyEntryInfo(entryInfo + index, this->descriptors[index].Id->GetPropertyId(), this->descriptors[index].Attributes, TTD::NSSnapType::SnapEntryDataKindTag::Data);
        }

        return plength;
    }

    template<size_t size>
    Js::BigPropertyIndex SimpleTypeHandler<size>::GetPropertyIndex_EnumerateTTD(const Js::PropertyRecord* pRecord)
    {TRACE_IT(67646);
        int index;
        if(this->GetDescriptor(pRecord->GetPropertyId(), &index))
        {TRACE_IT(67647);
            TTDAssert(!(this->descriptors[index].Attributes & PropertyDeleted), "How is this deleted but we enumerated it anyway???");

            return (Js::BigPropertyIndex)index;
        }

        TTDAssert(false, "We found this during enum so what is going on here?");
        return Js::Constants::NoBigSlot;
    }

#endif

    template class SimpleTypeHandler<1>;
    template class SimpleTypeHandler<2>;

}
