//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    template <typename T>
    DictionaryTypeHandlerBase<T>* DictionaryTypeHandlerBase<T>::New(Recycler * recycler, int initialCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {TRACE_IT(65423);
        return NewTypeHandler<DictionaryTypeHandlerBase>(recycler, initialCapacity, inlineSlotCapacity, offsetOfInlineSlots);
    }

    template <typename T>
    DictionaryTypeHandlerBase<T>* DictionaryTypeHandlerBase<T>::CreateTypeHandlerForArgumentsInStrictMode(Recycler * recycler, ScriptContext * scriptContext)
    {TRACE_IT(65424);
        DictionaryTypeHandlerBase<T> * dictTypeHandler = New(recycler, 8, 0, 0);
        
        dictTypeHandler->Add(scriptContext->GetPropertyName(Js::PropertyIds::caller), PropertyWritable, scriptContext);
        dictTypeHandler->Add(scriptContext->GetPropertyName(Js::PropertyIds::callee), PropertyWritable, scriptContext);
        dictTypeHandler->Add(scriptContext->GetPropertyName(Js::PropertyIds::length), PropertyBuiltInMethodDefaults, scriptContext);
        dictTypeHandler->Add(scriptContext->GetPropertyName(Js::PropertyIds::_symbolIterator), PropertyBuiltInMethodDefaults, scriptContext);

        return dictTypeHandler;
    }

    template <typename T>
    DictionaryTypeHandlerBase<T>::DictionaryTypeHandlerBase(Recycler* recycler) :
        DynamicTypeHandler(1),
        nextPropertyIndex(0),
        singletonInstance(nullptr)
    {TRACE_IT(65425);
        SetIsInlineSlotCapacityLocked();
        propertyMap = RecyclerNew(recycler, PropertyDescriptorMap, recycler, this->GetSlotCapacity());
    }

    template <typename T>
    DictionaryTypeHandlerBase<T>::DictionaryTypeHandlerBase(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots) :
        // Do not RoundUp passed in slotCapacity. This may be called by ConvertTypeHandler for an existing DynamicObject and should use the real existing slotCapacity.
        DynamicTypeHandler(slotCapacity, inlineSlotCapacity, offsetOfInlineSlots),
        nextPropertyIndex(0),
        singletonInstance(nullptr)
    {TRACE_IT(65426);
        SetIsInlineSlotCapacityLocked();
        Assert(GetSlotCapacity() <= MaxPropertyIndexSize);
        propertyMap = RecyclerNew(recycler, PropertyDescriptorMap, recycler, slotCapacity);
    }

    //
    // Takes over a given dictionary typeHandler. Used only by subclass.
    //
    template <typename T>
    DictionaryTypeHandlerBase<T>::DictionaryTypeHandlerBase(DictionaryTypeHandlerBase* typeHandler) :
        DynamicTypeHandler(typeHandler->GetSlotCapacity(), typeHandler->GetInlineSlotCapacity(), typeHandler->GetOffsetOfInlineSlots()),
        propertyMap(typeHandler->propertyMap), nextPropertyIndex(typeHandler->nextPropertyIndex),
        singletonInstance(typeHandler->singletonInstance)
    {TRACE_IT(65427);
        Assert(typeHandler->GetIsInlineSlotCapacityLocked());
        CopyPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection | PropertyTypesInlineSlotCapacityLocked, typeHandler->GetPropertyTypes());
    }

    template <typename T>
    int DictionaryTypeHandlerBase<T>::GetPropertyCount()
    {TRACE_IT(65428);
        return propertyMap->Count();
    }

    template <typename T>
    PropertyId DictionaryTypeHandlerBase<T>::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {TRACE_IT(65429);
        if (index < propertyMap->Count())
        {TRACE_IT(65430);
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            if (!(descriptor.Attributes & PropertyDeleted) && descriptor.HasNonLetConstGlobal())
            {TRACE_IT(65431);
                return propertyMap->GetKeyAt(index)->GetPropertyId();
            }
        }
        return Constants::NoProperty;
    }

    template <typename T>
    PropertyId DictionaryTypeHandlerBase<T>::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {TRACE_IT(65432);
        if (index < propertyMap->Count())
        {TRACE_IT(65433);
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            if (!(descriptor.Attributes & PropertyDeleted) && descriptor.HasNonLetConstGlobal())
            {TRACE_IT(65434);
                return propertyMap->GetKeyAt(index)->GetPropertyId();
            }
        }
        return Constants::NoProperty;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {TRACE_IT(65435);
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for(; index < propertyMap->Count(); ++index )
        {TRACE_IT(65436);
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            PropertyAttributes attribs = descriptor.Attributes;

            if (!(attribs & PropertyDeleted) && (!!(flags & EnumeratorFlags::EnumNonEnumerable) || (attribs & PropertyEnumerable)) &&
                (!(attribs & PropertyLetConstGlobal) || descriptor.HasNonLetConstGlobal()))
            {TRACE_IT(65437);
                const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);

                // Skip this property if it is a symbol and we are not including symbol properties
                if (!(flags & EnumeratorFlags::EnumSymbols) && propertyRecord->IsSymbol())
                {TRACE_IT(65438);
                    continue;
                }

                // Pass back attributes of this property so caller can use them if it needs
                if (attributes != nullptr)
                {TRACE_IT(65439);
                    *attributes = attribs;
                }

                *propertyId = propertyRecord->GetPropertyId();
                PropertyString* propertyString = scriptContext->GetPropertyString(*propertyId);
                *propertyStringName = propertyString;
                T dataSlot = descriptor.template GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots && (attribs & PropertyWritable))
                {TRACE_IT(65440);
                    uint16 inlineOrAuxSlotIndex;
                    bool isInlineSlot;
                    PropertyIndexToInlineOrAuxSlotIndex(dataSlot, &inlineOrAuxSlotIndex, &isInlineSlot);

                    propertyString->UpdateCache(type, inlineOrAuxSlotIndex, isInlineSlot, descriptor.IsInitialized && !descriptor.IsFixed);
                }
                else
                {TRACE_IT(65441);
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

    template <>
    BOOL DictionaryTypeHandlerBase<BigPropertyIndex>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {TRACE_IT(65442);
        Assert(false);
        Throw::InternalError();
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {TRACE_IT(65443);
        PropertyIndex local = (PropertyIndex)index;
        Assert(index <= Constants::UShortMaxValue || index == Constants::NoBigSlot);
        BOOL result = this->FindNextProperty(scriptContext, local, propertyString, propertyId, attributes, type, typeToEnumerate, flags);
        index = local;
        return result;
    }

    template <>
    BOOL DictionaryTypeHandlerBase<BigPropertyIndex>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {TRACE_IT(65444);
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for(; index < propertyMap->Count(); ++index )
        {TRACE_IT(65445);
            DictionaryPropertyDescriptor<BigPropertyIndex> descriptor = propertyMap->GetValueAt(index);
            PropertyAttributes attribs = descriptor.Attributes;
            if (!(attribs & PropertyDeleted) && (!!(flags & EnumeratorFlags::EnumNonEnumerable) || (attribs & PropertyEnumerable)) &&
                (!(attribs & PropertyLetConstGlobal) || descriptor.HasNonLetConstGlobal()))
            {TRACE_IT(65446);
                const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);

                // Skip this property if it is a symbol and we are not including symbol properties
                if (!(flags & EnumeratorFlags::EnumSymbols) && propertyRecord->IsSymbol())
                {TRACE_IT(65447);
                    continue;
                }

                if (attributes != nullptr)
                {TRACE_IT(65448);
                    *attributes = attribs;
                }

                *propertyId = propertyRecord->GetPropertyId();
                *propertyStringName = scriptContext->GetPropertyString(*propertyId);

                return TRUE;
            }
        }

        return FALSE;
    }

    template <typename T>
    PropertyIndex DictionaryTypeHandlerBase<T>::GetPropertyIndex(PropertyRecord const* propertyRecord)
    {TRACE_IT(65449);
        return GetPropertyIndex_Internal<false>(propertyRecord);
    }

    template <typename T>
    PropertyIndex DictionaryTypeHandlerBase<T>::GetRootPropertyIndex(PropertyRecord const* propertyRecord)
    {TRACE_IT(65450);
        return GetPropertyIndex_Internal<true>(propertyRecord);
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {TRACE_IT(65451);
        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {TRACE_IT(65452);
            AssertMsg(descriptor->template GetDataPropertyIndex<false>() != Constants::NoSlot, "We don't support equivalent object type spec on accessors.");
            AssertMsg(descriptor->template GetDataPropertyIndex<false>() <= Constants::PropertyIndexMax, "We don't support equivalent object type spec on big property indexes.");
            T propertyIndex = descriptor->template GetDataPropertyIndex<false>();
            info.slotIndex = propertyIndex <= Constants::PropertyIndexMax ?
                AdjustValidSlotIndexForInlineSlots(static_cast<PropertyIndex>(propertyIndex)) : Constants::NoSlot;
            info.isAuxSlot = propertyIndex >= GetInlineSlotCapacity();
            info.isWritable = !!(descriptor->Attributes & PropertyWritable);
        }
        else
        {TRACE_IT(65453);
            info.slotIndex = Constants::NoSlot;
            info.isAuxSlot = false;
            info.isWritable = false;
        }
        return info.slotIndex != Constants::NoSlot;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {TRACE_IT(65454);
        uint propertyCount = record.propertyCount;
        EquivalentPropertyEntry* properties = record.properties;
        for (uint pi = 0; pi < propertyCount; pi++)
        {TRACE_IT(65455);
            const EquivalentPropertyEntry* refInfo = &properties[pi];
            if (!this->IsObjTypeSpecEquivalentImpl<false>(type, refInfo))
            {TRACE_IT(65456);
                failedPropertyIndex = pi;
                return false;
            }
        }

        return true;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {TRACE_IT(65457);
        return this->IsObjTypeSpecEquivalentImpl<true>(type, entry);
    }

    template <typename T>
    template <bool doLock>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalentImpl(const Type* type, const EquivalentPropertyEntry *entry)
    {TRACE_IT(65458);
        ScriptContext* scriptContext = type->GetScriptContext();

        T absSlotIndex = Constants::NoSlot;
        PropertyIndex relSlotIndex = Constants::NoSlot;

        const PropertyRecord* propertyRecord =
            doLock ? scriptContext->GetPropertyNameLocked(entry->propertyId) : scriptContext->GetPropertyName(entry->propertyId);
        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {TRACE_IT(65459);
            // We don't object type specialize accessors at this point, so if we see an accessor on an object we must have a mismatch.
            // When we add support for accessors we will need another bit on EquivalentPropertyEntry indicating whether we expect
            // a data or accessor property.
            if (descriptor->IsAccessor)
            {TRACE_IT(65460);
                return false;
            }

            absSlotIndex = descriptor->template GetDataPropertyIndex<false>();
            if (absSlotIndex <= Constants::PropertyIndexMax)
            {TRACE_IT(65461);
                relSlotIndex = AdjustValidSlotIndexForInlineSlots(static_cast<PropertyIndex>(absSlotIndex));
            }
        }

        if (relSlotIndex != Constants::NoSlot)
        {TRACE_IT(65462);
            if (relSlotIndex != entry->slotIndex || ((absSlotIndex >= GetInlineSlotCapacity()) != entry->isAuxSlot))
            {TRACE_IT(65463);
                return false;
            }

            if (entry->mustBeWritable && (!(descriptor->Attributes & PropertyWritable) || !descriptor->IsInitialized || descriptor->IsFixed))
            {TRACE_IT(65464);
                return false;
            }
        }
        else
        {TRACE_IT(65465);
            if (entry->slotIndex != Constants::NoSlot || entry->mustBeWritable)
            {TRACE_IT(65466);
                return false;
            }
        }

        return true;
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    PropertyIndex DictionaryTypeHandlerBase<T>::GetPropertyIndex_Internal(PropertyRecord const* propertyRecord)
    {TRACE_IT(65467);
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {TRACE_IT(65468);
            return descriptor->template GetDataPropertyIndex<allowLetConstGlobal>();
        }
        else
        {TRACE_IT(65469);
            return NoSlots;
        }
    }

    template <>
    template <bool allowLetConstGlobal>
    PropertyIndex DictionaryTypeHandlerBase<BigPropertyIndex>::GetPropertyIndex_Internal(PropertyRecord const* propertyRecord)
    {TRACE_IT(65470);
        DictionaryPropertyDescriptor<BigPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {TRACE_IT(65471);
            BigPropertyIndex dataPropertyIndex = descriptor->GetDataPropertyIndex<allowLetConstGlobal>();
            if(dataPropertyIndex < Constants::NoSlot)
            {TRACE_IT(65472);
                return (PropertyIndex)dataPropertyIndex;
            }
        }
        return Constants::NoSlot;
    }

    template <>
    PropertyIndex DictionaryTypeHandlerBase<BigPropertyIndex>::GetRootPropertyIndex(PropertyRecord const* propertyRecord)
    {TRACE_IT(65473);
        return Constants::NoSlot;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::Add(
        const PropertyRecord* propertyId,
        PropertyAttributes attributes,
        ScriptContext *const scriptContext)
    {TRACE_IT(65474);
        return Add(propertyId, attributes, true, false, false, scriptContext);
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::Add(
        const PropertyRecord* propertyId,
        PropertyAttributes attributes,
        bool isInitialized, bool isFixed, bool usedAsFixed,
        ScriptContext *const scriptContext)
    {TRACE_IT(65475);
        Assert(this->GetSlotCapacity() <= MaxPropertyIndexSize);   // slotCapacity should never exceed MaxPropertyIndexSize
        Assert(nextPropertyIndex < this->GetSlotCapacity());       // nextPropertyIndex must be ready
        T index = nextPropertyIndex++;

        DictionaryPropertyDescriptor<T> descriptor(index, attributes);
        Assert((!isFixed && !usedAsFixed) || (!IsInternalPropertyId(propertyId->GetPropertyId()) && this->singletonInstance != nullptr));
        descriptor.IsInitialized = isInitialized;
        descriptor.IsFixed = isFixed;
        descriptor.UsedAsFixed = usedAsFixed;

        propertyMap->Add(propertyId, descriptor);

        if (!(attributes & PropertyWritable))
        {TRACE_IT(65476);
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {TRACE_IT(65477);
                scriptContext->InvalidateStoreFieldCaches(propertyId->GetPropertyId());
                scriptContext->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl)
    {TRACE_IT(65478);
        return HasProperty_Internal<false>(instance, propertyId, noRedecl, nullptr, nullptr);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasRootProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty, bool *pNonconfigurableProperty)
    {TRACE_IT(65479);
        return HasProperty_Internal<true>(instance, propertyId, noRedecl, pDeclaredProperty, pNonconfigurableProperty);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty_Internal(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty, bool *pNonconfigurableProperty)
    {TRACE_IT(65480);
        // HasProperty is called with NoProperty in JavascriptDispatch.cpp to for undeferral of the
        // deferred type system that DOM objects use.  Allow NoProperty for this reason, but only
        // here in HasProperty.
        if (propertyId == Constants::NoProperty)
        {TRACE_IT(65481);
            return false;
        }

        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65482);
            if ((descriptor->Attributes & PropertyDeleted) || (!allowLetConstGlobal && !descriptor->HasNonLetConstGlobal()))
            {TRACE_IT(65483);
                return false;
            }
            if (noRedecl && descriptor->Attributes & PropertyNoRedecl)
            {TRACE_IT(65484);
                *noRedecl = true;
            }
            if (pDeclaredProperty && descriptor->Attributes & (PropertyNoRedecl | PropertyDeclaredGlobal))
            {TRACE_IT(65485);
                *pDeclaredProperty = true;
            }
            if (pNonconfigurableProperty && !(descriptor->Attributes & PropertyConfigurable))
            {TRACE_IT(65486);
                *pNonconfigurableProperty = true;
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {TRACE_IT(65487);
            return DictionaryTypeHandlerBase<T>::HasItem(instance, propertyRecord->GetNumericValue());
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {TRACE_IT(65488);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {TRACE_IT(65489);
            if ((descriptor->Attributes & PropertyDeleted) || !descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65490);
                return false;
            }
            return true;
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetRootProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(65491);
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return GetProperty_Internal<true>(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(65492);
        return GetProperty_Internal<false>(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <typename T>
    template <bool allowLetConstGlobal, typename PropertyType>
    BOOL DictionaryTypeHandlerBase<T>::GetPropertyFromDescriptor(DynamicObject* instance, Var originalInstance,
        DictionaryPropertyDescriptor<T>* descriptor, Var* value, PropertyValueInfo* info, PropertyType propertyT, ScriptContext* requestContext)
    {TRACE_IT(65493);
        bool const isLetConstGlobal = (descriptor->Attributes & PropertyLetConstGlobal) != 0;
        AssertMsg(!isLetConstGlobal || RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
        if (allowLetConstGlobal)
        {TRACE_IT(65494);
            // GetRootProperty: false if not global
            if (!(descriptor->Attributes & PropertyLetConstGlobal) && (descriptor->Attributes & PropertyDeleted))
            {TRACE_IT(65495);
                return false;
            }
        }
        else
        {TRACE_IT(65496);
            // GetProperty: don't count deleted or global.
            if (descriptor->Attributes & (PropertyDeleted | (descriptor->IsShadowed ? 0 : PropertyLetConstGlobal)))
            {TRACE_IT(65497);
                return false;
            }
        }

        T dataSlot = descriptor->template GetDataPropertyIndex<allowLetConstGlobal>();
        if (dataSlot != NoSlots)
        {TRACE_IT(65498);
            *value = instance->GetSlot(dataSlot);
            SetPropertyValueInfo(info, instance, dataSlot, descriptor->Attributes);
            if (!descriptor->IsInitialized || descriptor->IsFixed)
            {TRACE_IT(65499);
                PropertyValueInfo::DisableStoreFieldCache(info);
            }
            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65500);
                // letconst shadowing a deleted property. don't bother to cache
                PropertyValueInfo::SetNoCache(info, instance);
            }
        }
        else if (descriptor->GetGetterPropertyIndex() != NoSlots)
        {
            // We must update cache before calling a getter, because it can invalidate something. Bug# 593815
            SetPropertyValueInfo(info, instance, descriptor->GetGetterPropertyIndex(), descriptor->Attributes);
            CacheOperators::CachePropertyReadForGetter(info, originalInstance, propertyT, requestContext);
            PropertyValueInfo::SetNoCache(info, instance); // we already cached getter, so we don't have to do it once more

            RecyclableObject* func = RecyclableObject::FromVar(instance->GetSlot(descriptor->GetGetterPropertyIndex()));
            *value = JavascriptOperators::CallGetter(func, originalInstance, requestContext);
            return true;
        }
        else
        {TRACE_IT(65501);
            *value = instance->GetLibrary()->GetUndefined();
            return true;
        }
        return true;
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty_Internal(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(65502);
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65503);
            return GetPropertyFromDescriptor<allowLetConstGlobal>(instance, originalInstance, descriptor, value, info, propertyId, requestContext);
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {TRACE_IT(65504);
            return DictionaryTypeHandlerBase<T>::GetItem(instance, originalInstance, propertyRecord->GetNumericValue(), value, requestContext);
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(65505);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {TRACE_IT(65506);
            return GetPropertyFromDescriptor<false>(instance, originalInstance, descriptor, value, info, propertyName, requestContext);
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, T propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {TRACE_IT(65507);
        PropertyValueInfo::Set(info, instance, propIndex, attributes, flags);
    }


    template <>
    void DictionaryTypeHandlerBase<BigPropertyIndex>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, BigPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {TRACE_IT(65508);
        PropertyValueInfo::SetNoCache(info, instance);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(65509);
        return GetSetter_Internal<false>(instance, propertyId, setterValue, info, requestContext);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetRootSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(65510);
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return GetSetter_Internal<true>(instance, propertyId, setterValue, info, requestContext);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter_Internal(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(65511);
        DictionaryPropertyDescriptor<T>* descriptor;

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65512);
            return GetSetterFromDescriptor<allowLetConstGlobal>(instance, descriptor, setterValue, info);
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {TRACE_IT(65513);
            return DictionaryTypeHandlerBase<T>::GetItemSetter(instance, propertyRecord->GetNumericValue(), setterValue, requestContext);
        }

        return None;
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetterFromDescriptor(DynamicObject* instance, DictionaryPropertyDescriptor<T> * descriptor, Var* setterValue, PropertyValueInfo* info)
    {TRACE_IT(65514);
        if (descriptor->Attributes & PropertyDeleted)
        {TRACE_IT(65515);
            return None;
        }
        if (descriptor->template GetDataPropertyIndex<allowLetConstGlobal>() != NoSlots)
        {TRACE_IT(65516);
            // not a setter but shadows
            if (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal))
            {TRACE_IT(65517);
                return (descriptor->Attributes & PropertyConst) ? (DescriptorFlags)(Const | Data) : WritableData;
            }
            if (descriptor->Attributes & PropertyWritable)
            {TRACE_IT(65518);
                return WritableData;
            }
            if (descriptor->Attributes & PropertyConst)
            {TRACE_IT(65519);
                return (DescriptorFlags)(Const|Data);
            }
            return Data;
        }
        else if (descriptor->GetSetterPropertyIndex() != NoSlots)
        {TRACE_IT(65520);
            *setterValue=((DynamicObject*)instance)->GetSlot(descriptor->GetSetterPropertyIndex());
            SetPropertyValueInfo(info, instance, descriptor->GetSetterPropertyIndex(), descriptor->Attributes, InlineCacheSetterFlag);
            return Accessor;
        }
        return None;
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(65521);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;

        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {TRACE_IT(65522);
            return GetSetterFromDescriptor<false>(instance, descriptor, setterValue, info);
        }

        return None;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetRootProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65523);
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return SetProperty_Internal<true>(instance, propertyId, value, flags, info);
    }
    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::InitProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65524);
        return SetProperty_Internal<false>(instance, propertyId, value, flags, info, true /* IsInit */);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65525);
        return SetProperty_Internal<false>(instance, propertyId, value, flags, info);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    void DictionaryTypeHandlerBase<T>::SetPropertyWithDescriptor(DynamicObject* instance, PropertyId propertyId, DictionaryPropertyDescriptor<T> * descriptor,
        Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65526);
        Assert(instance);
        Assert((descriptor->Attributes & PropertyDeleted) == 0 || (allowLetConstGlobal && descriptor->IsShadowed));

        // DictionaryTypeHandlers are not supposed to be shared.
        Assert(!GetIsOrMayBecomeShared());
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);

        T dataSlotAllowLetConstGlobal = descriptor->template GetDataPropertyIndex<allowLetConstGlobal>();
        if (dataSlotAllowLetConstGlobal != NoSlots)
        {TRACE_IT(65527);
            if (allowLetConstGlobal
                && (descriptor->Attributes & PropertyNoRedecl)
                && !(flags & PropertyOperation_AllowUndecl))
            {TRACE_IT(65528);
                ScriptContext* scriptContext = instance->GetScriptContext();
                if (scriptContext->IsUndeclBlockVar(instance->GetSlot(dataSlotAllowLetConstGlobal)))
                {TRACE_IT(65529);
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
                }
            }

            if (!descriptor->IsInitialized)
            {TRACE_IT(65530);
                if ((flags & PropertyOperation_PreInit) == 0)
                {TRACE_IT(65531);
                    descriptor->IsInitialized = true;
                    if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId) &&
                        (flags & (PropertyOperation_NonFixedValue | PropertyOperation_SpecialValue)) == 0)
                    {TRACE_IT(65532);
                        Assert(value != nullptr);
                        // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                        Assert(!instance->IsExternal());
                        descriptor->IsFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyId, value)));
                    }
                }
            }
            else
            {
                InvalidateFixedField(instance, propertyId, descriptor);
            }

            SetSlotUnchecked(instance, dataSlotAllowLetConstGlobal, value);

            // If we just added a fixed method, don't populate the inline cache so that we always take the slow path
            // when overwriting this property and correctly invalidate any JIT-ed code that hard-coded this method.
            if (descriptor->IsInitialized && !descriptor->IsFixed)
            {
                SetPropertyValueInfo(info, instance, dataSlotAllowLetConstGlobal, GetLetConstGlobalPropertyAttributes<allowLetConstGlobal>(descriptor->Attributes));
            }
            else
            {TRACE_IT(65533);
                PropertyValueInfo::SetNoCache(info, instance);
            }
        }
        else if (descriptor->GetSetterPropertyIndex() != NoSlots)
        {TRACE_IT(65534);
            RecyclableObject* func = RecyclableObject::FromVar(instance->GetSlot(descriptor->GetSetterPropertyIndex()));
            JavascriptOperators::CallSetter(func, instance, value, NULL);

            // Wait for the setter to return before setting up the inline cache info, as the setter may change
            // the attributes
            T dataSlot = descriptor->template GetDataPropertyIndex<false>();
            if (dataSlot != NoSlots)
            {
                SetPropertyValueInfo(info, instance, dataSlot, descriptor->Attributes);
            }
            else if (descriptor->GetSetterPropertyIndex() != NoSlots)
            {
                SetPropertyValueInfo(info, instance, descriptor->GetSetterPropertyIndex(), descriptor->Attributes, InlineCacheSetterFlag);
            }
        }
        SetPropertyUpdateSideEffect(instance, propertyId, value, SideEffects_Any);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::SetProperty_Internal(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, bool isInit)
    {TRACE_IT(65535);
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T>* descriptor;
        bool throwIfNotExtensible = (flags & (PropertyOperation_ThrowIfNotExtensible | PropertyOperation_StrictMode)) != 0;
        bool isForce = (flags & PropertyOperation_Force) != 0;

        JavascriptLibrary::CheckAndInvalidateIsConcatSpreadableCache(propertyId, scriptContext);

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65536);
            Assert(descriptor->SanityCheckFixedBits());
            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65537);
                if (!isForce)
                {TRACE_IT(65538);
                    if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                    {TRACE_IT(65539);
                        return false;
                    }
                }
                scriptContext->InvalidateProtoCaches(propertyId);
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {TRACE_IT(65540);
                    descriptor->Attributes = PropertyDynamicTypeDefaults | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {TRACE_IT(65541);
                    descriptor->Attributes = PropertyDynamicTypeDefaults;
                }
                instance->SetHasNoEnumerableProperties(false);
                descriptor->ConvertToData();
            }
            else if (!allowLetConstGlobal && descriptor->HasNonLetConstGlobal() && !(descriptor->Attributes & PropertyWritable))
            {TRACE_IT(65542);
                if (!isForce)
                {TRACE_IT(65543);
                    JavascriptError::ThrowCantAssignIfStrictMode(flags, scriptContext);
                }

                // Since we separate LdFld and StFld caches there is no point in caching for StFld with non-writable properties, except perhaps
                // to prepopulate the type property cache (which we do share between LdFld and StFld), for potential future field loads.  This
                // would require additional handling in CacheOperators::CachePropertyWrite, such that for !info-IsWritable() we don't populate
                // the local cache (that would be illegal), but still populate the type's property cache.
                PropertyValueInfo::SetNoCache(info, instance);
                return false;
            }
            else if (isInit && descriptor->IsAccessor)
            {TRACE_IT(65544);
                descriptor->ConvertToData();
            }
            SetPropertyWithDescriptor<allowLetConstGlobal>(instance, propertyId, descriptor, value, flags, info);
            return true;
        }

        // Always check numeric propertyRecord. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65545);
            // Calls this or subclass implementation
            return SetItem(instance, propertyRecord->GetNumericValue(), value, flags);
        }
        return this->AddProperty(instance, propertyRecord, value, PropertyDynamicTypeDefaults, info, flags, throwIfNotExtensible, SideEffects_Any);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65546);
        // Either the property exists in the dictionary, in which case a PropertyRecord for it exists,
        // or we have to add it to the dictionary, in which case we need to get or create a PropertyRecord.
        // Thus, just get or create one and call the PropertyId overload of SetProperty.
        PropertyRecord const * propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return DictionaryTypeHandlerBase<T>::SetProperty(instance, propertyRecord->GetPropertyId(), value, flags, info);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(65547);
        return DeleteProperty_Internal<false>(instance, propertyId, propertyOperationFlags);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::DeleteProperty(DynamicObject *instance, JavascriptString *propertyNameString, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(65548);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* ");

        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T>* descriptor;
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());

        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {TRACE_IT(65549);
            Assert(descriptor->SanityCheckFixedBits());

            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65550);
                return true;
            }
            else if (!(descriptor->Attributes & PropertyConfigurable))
            {TRACE_IT(65551);
                // Let/const properties do not have attributes and they cannot be deleted
                JavascriptError::ThrowCantDelete(propertyOperationFlags, scriptContext, propertyNameString->GetString());

                return false;
            }

            Var undefined = scriptContext->GetLibrary()->GetUndefined();

            if (descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65552);
                T dataSlot = descriptor->template GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots)
                {
                    SetSlotUnchecked(instance, dataSlot, undefined);
                }
                else
                {TRACE_IT(65553);
                    Assert(descriptor->IsAccessor);
                    SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), undefined);
                    SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), undefined);
                }

                if (this->GetFlags() & IsPrototypeFlag)
                {TRACE_IT(65554);
                    scriptContext->InvalidateProtoCaches(scriptContext->GetOrAddPropertyIdTracked(propertyNameString->GetString(), propertyNameString->GetLength()));
                }

                if ((descriptor->Attributes & PropertyLetConstGlobal) == 0)
                {TRACE_IT(65555);
                    Assert(!descriptor->IsShadowed);
                    descriptor->Attributes = PropertyDeletedDefaults;
                }
                else
                {TRACE_IT(65556);
                    descriptor->Attributes &= ~PropertyDynamicTypeDefaults;
                    descriptor->Attributes |= PropertyDeletedDefaults;
                }
                InvalidateFixedField(instance, propertyNameString, descriptor);

                // Change the type so as we can invalidate the cache in fast path jit
                if (instance->GetType()->HasBeenCached())
                {TRACE_IT(65557);
                    instance->ChangeType();
                }
                SetPropertyUpdateSideEffect(instance, propertyName, nullptr, SideEffects_Any);
                return true;
            }

            Assert(descriptor->Attributes & PropertyLetConstGlobal);
            return false;
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::DeleteRootProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(65558);
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return DeleteProperty_Internal<true>(instance, propertyId, propertyOperationFlags);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::DeleteProperty_Internal(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(65559);
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65560);
            Assert(descriptor->SanityCheckFixedBits());

            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65561);
                // If PropertyDeleted and PropertyLetConstGlobal are set then we have both
                // a deleted global property and let/const variable in this descriptor.
                // If allowLetConstGlobal is true then the let/const shadows the property
                // and we should return false for a failed delete by going into the else
                // if branch below.
                if (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal))
                {TRACE_IT(65562);
                    JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, scriptContext, propertyRecord->GetBuffer());

                    return false;
                }
                return true;
            }
            else if (!(descriptor->Attributes & PropertyConfigurable) ||
                (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal)))
            {TRACE_IT(65563);
                // Let/const properties do not have attributes and they cannot be deleted
                JavascriptError::ThrowCantDelete(propertyOperationFlags, scriptContext, scriptContext->GetPropertyName(propertyId)->GetBuffer());

                return false;
            }

            Var undefined = scriptContext->GetLibrary()->GetUndefined();

            if (descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65564);
                T dataSlot = descriptor->template GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots)
                {
                    SetSlotUnchecked(instance, dataSlot, undefined);
                }
                else
                {TRACE_IT(65565);
                    Assert(descriptor->IsAccessor);
                    SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), undefined);
                    SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), undefined);
                }

                if (this->GetFlags() & IsPrototypeFlag)
                {TRACE_IT(65566);
                    scriptContext->InvalidateProtoCaches(propertyId);
                }

                if ((descriptor->Attributes & PropertyLetConstGlobal) == 0)
                {TRACE_IT(65567);
                    Assert(!descriptor->IsShadowed);
                    descriptor->Attributes = PropertyDeletedDefaults;
                }
                else
                {TRACE_IT(65568);
                    descriptor->Attributes &= ~PropertyDynamicTypeDefaults;
                    descriptor->Attributes |= PropertyDeletedDefaults;
                }
                InvalidateFixedField(instance, propertyId, descriptor);

                // Change the type so as we can invalidate the cache in fast path jit
                if (instance->GetType()->HasBeenCached())
                {TRACE_IT(65569);
                    instance->ChangeType();
                }
                SetPropertyUpdateSideEffect(instance, propertyId, nullptr, SideEffects_Any);
                return true;
            }

            Assert(descriptor->Attributes & PropertyLetConstGlobal);
            return false;
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {TRACE_IT(65570);
            return DictionaryTypeHandlerBase<T>::DeleteItem(instance, propertyRecord->GetNumericValue(), propertyOperationFlags);
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsFixedProperty(const DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(65571);
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T> descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetValue(propertyRecord, &descriptor))
        {TRACE_IT(65572);
            return descriptor.IsFixed;
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }

        template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(65573);
        if (!(this->GetFlags() & IsExtensibleFlag) && !instance->HasObjectArray())
        {TRACE_IT(65574);
            ScriptContext* scriptContext = instance->GetScriptContext();
            JavascriptError::ThrowCantExtendIfStrictMode(flags, scriptContext);
            return false;
        }
        return __super::SetItem(instance, index, value, flags);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {TRACE_IT(65575);
        return instance->SetObjectArrayItemWithAttributes(index, value, attributes);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {TRACE_IT(65576);
        return instance->SetObjectArrayItemAttributes(index, attributes);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter)
    {TRACE_IT(65577);
        return instance->SetObjectArrayItemAccessors(index, getter, setter);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {TRACE_IT(65578);
        if (instance->HasObjectArray())
        {TRACE_IT(65579);
            return instance->GetObjectArrayItemSetter(index, setterValue, requestContext);
        }
        return __super::GetItemSetter(instance, index, setterValue, requestContext);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(65580);
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65581);
            if (!descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65582);
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");

                return true;
            }
            return descriptor->Attributes & PropertyEnumerable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65583);
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {TRACE_IT(65584);
                return objectArray->IsEnumerable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(65585);
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65586);
            if (!descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65587);
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return !(descriptor->Attributes & PropertyConst);
            }
            return descriptor->Attributes & PropertyWritable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65588);
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {TRACE_IT(65589);
                return objectArray->IsWritable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(65590);
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65591);
            if (!descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65592);
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return true;
            }
            return descriptor->Attributes & PropertyConfigurable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65593);
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {TRACE_IT(65594);
                return objectArray->IsConfigurable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(65595);
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65596);
            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65597);
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65598);
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {TRACE_IT(65599);
                descriptor->Attributes |= PropertyEnumerable;
                instance->SetHasNoEnumerableProperties(false);
            }
            else
            {TRACE_IT(65600);
                descriptor->Attributes &= (~PropertyEnumerable);
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65601);
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {TRACE_IT(65602);
                return objectArray->SetEnumerable(propertyId, value);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(65603);
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        ScriptContext* scriptContext = instance->GetScriptContext();
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65604);
            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65605);
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65606);
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {TRACE_IT(65607);
                descriptor->Attributes |= PropertyWritable;
            }
            else
            {TRACE_IT(65608);
                descriptor->Attributes &= (~PropertyWritable);
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {TRACE_IT(65609);
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
            instance->ChangeType();
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {TRACE_IT(65610);
            return instance->SetObjectArrayItemWritable(propertyId, value);
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(65611);
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65612);
            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65613);
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65614);
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {TRACE_IT(65615);
                descriptor->Attributes |= PropertyConfigurable;
            }
            else
            {TRACE_IT(65616);
                descriptor->Attributes &= (~PropertyConfigurable);
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65617);
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {TRACE_IT(65618);
                return objectArray->SetConfigurable(propertyId, value);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::PreventExtensions(DynamicObject* instance)
    {TRACE_IT(65619);
        this->ClearFlags(IsExtensibleFlag);

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {TRACE_IT(65620);
            objectArray->PreventExtensions();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::Seal(DynamicObject* instance)
    {TRACE_IT(65621);
        this->ClearFlags(IsExtensibleFlag);

        // Set [[Configurable]] flag of each property to false
        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {TRACE_IT(65622);
            descriptor = propertyMap->GetReferenceAt(index);
            if (descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65623);
                descriptor->Attributes &= (~PropertyConfigurable);
            }
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {TRACE_IT(65624);
            objectArray->Seal();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {TRACE_IT(65625);
        this->ClearFlags(IsExtensibleFlag);

        // Set [[Writable]] flag of each property to false except for setter\getters
        // Set [[Configurable]] flag of each property to false
        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {TRACE_IT(65626);
            descriptor = propertyMap->GetReferenceAt(index);
            if (descriptor->HasNonLetConstGlobal())
            {TRACE_IT(65627);
                if (descriptor->template GetDataPropertyIndex<false>() != NoSlots)
                {TRACE_IT(65628);
                    // Only data descriptor has Writable property
                    descriptor->Attributes &= ~(PropertyWritable | PropertyConfigurable);
                }
                else
                {TRACE_IT(65629);
                    descriptor->Attributes &= ~(PropertyConfigurable);
                }
            }
#if DBG
            else
            {TRACE_IT(65630);
                AssertMsg(RootObjectBase::Is(instance), "instance needs to be global object when letconst global is set");
            }
#endif
        }
        if (!isConvertedType)
        {TRACE_IT(65631);
            // Change of [[Writable]] property requires cache invalidation, hence ChangeType
            instance->ChangeType();
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {TRACE_IT(65632);
            objectArray->Freeze();
        }

        this->ClearHasOnlyWritableDataProperties();
        if(GetFlags() & IsPrototypeFlag)
        {TRACE_IT(65633);
            InvalidateStoreFieldCachesForAllProperties(instance->GetScriptContext());
            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsSealed(DynamicObject* instance)
    {TRACE_IT(65634);
        if (this->GetFlags() & IsExtensibleFlag)
        {TRACE_IT(65635);
            return false;
        }

        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {TRACE_IT(65636);
            descriptor = propertyMap->GetReferenceAt(index);
            if ((!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal)))
            {TRACE_IT(65637);
                if (descriptor->Attributes & PropertyConfigurable)
                {TRACE_IT(65638);
                    // [[Configurable]] must be false for all (existing) properties.
                    // IE9 compatibility: keep IE9 behavior (also check deleted properties)
                    return false;
                }
            }
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray && !objectArray->IsSealed())
        {TRACE_IT(65639);
            return false;
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsFrozen(DynamicObject* instance)
    {TRACE_IT(65640);
        if (this->GetFlags() & IsExtensibleFlag)
        {TRACE_IT(65641);
            return false;
        }

        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {TRACE_IT(65642);
            descriptor = propertyMap->GetReferenceAt(index);
            if ((!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal)))
            {TRACE_IT(65643);
                if (descriptor->Attributes & PropertyConfigurable)
                {TRACE_IT(65644);
                    return false;
                }

                if (descriptor->template GetDataPropertyIndex<false>() != NoSlots && (descriptor->Attributes & PropertyWritable))
                {TRACE_IT(65645);
                    // Only data descriptor has [[Writable]] property
                    return false;
                }
            }
        }

        // Use IsObjectArrayFrozen() to skip "length" [[Writable]] check
        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray && !objectArray->IsObjectArrayFrozen())
        {TRACE_IT(65646);
            return false;
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetAccessors(DynamicObject* instance, PropertyId propertyId, Var* getter, Var* setter)
    {TRACE_IT(65647);
        DictionaryPropertyDescriptor<T>* descriptor;
        ScriptContext* scriptContext = instance->GetScriptContext();
        AssertMsg(nullptr != getter && nullptr != setter, "Getter/Setter must be a valid pointer" );

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65648);
            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65649);
                return false;
            }

            if (descriptor->template GetDataPropertyIndex<false>() == NoSlots)
            {TRACE_IT(65650);
                bool getset = false;
                if (descriptor->GetGetterPropertyIndex() != NoSlots)
                {TRACE_IT(65651);
                    *getter = instance->GetSlot(descriptor->GetGetterPropertyIndex());
                    getset = true;
                }
                if (descriptor->GetSetterPropertyIndex() != NoSlots)
                {TRACE_IT(65652);
                    *setter = instance->GetSlot(descriptor->GetSetterPropertyIndex());
                    getset = true;
                }
                return getset;
            }
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65653);
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {TRACE_IT(65654);
                return objectArray->GetAccessors(propertyId, getter, setter, scriptContext);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(65655);
        Assert(instance);
        JavascriptLibrary* library = instance->GetLibrary();
        ScriptContext* scriptContext = instance->GetScriptContext();

        Assert(this->VerifyIsExtensible(scriptContext, false) || this->HasProperty(instance, propertyId)
            || JavascriptFunction::IsBuiltinProperty(instance, propertyId));

        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->GetFlags() & IsPrototypeFlag)
        {TRACE_IT(65656);
            scriptContext->InvalidateProtoCaches(propertyId);
        }

        bool isGetterSet = true;
        bool isSetterSet = true;
        if (!getter || getter == library->GetDefaultAccessorFunction())
        {TRACE_IT(65657);
            isGetterSet = false;
        }
        if (!setter || setter == library->GetDefaultAccessorFunction())
        {TRACE_IT(65658);
            isSetterSet = false;
        }

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65659);
            Assert(descriptor->SanityCheckFixedBits());

            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65660);
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {TRACE_IT(65661);
                    descriptor->Attributes = PropertyDynamicTypeDefaults | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {TRACE_IT(65662);
                    descriptor->Attributes = PropertyDynamicTypeDefaults;
                }
            }

            if (!descriptor->IsAccessor)
            {TRACE_IT(65663);
                // New getter/setter, make sure both values are not null and set to the slots
                getter = CanonicalizeAccessor(getter, library);
                setter = CanonicalizeAccessor(setter, library);
            }

            // conversion from data-property to accessor property
            if (descriptor->ConvertToGetterSetter(nextPropertyIndex))
            {TRACE_IT(65664);
                if (this->GetSlotCapacity() <= nextPropertyIndex)
                {TRACE_IT(65665);
                    if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
                    {TRACE_IT(65666);
                        Throw::OutOfMemory();
                    }

                    this->EnsureSlotCapacity(instance);
                }
            }

            // DictionaryTypeHandlers are not supposed to be shared.
            Assert(!GetIsOrMayBecomeShared());
            DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
            Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);

            // Although we don't actually have CreateTypeForNewScObject on DictionaryTypeHandler, we could potentially
            // transition to a DictionaryTypeHandler with some properties uninitialized.
            if (!descriptor->IsInitialized)
            {TRACE_IT(65667);
                descriptor->IsInitialized = true;
                if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId))
                {TRACE_IT(65668);
                    // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                    Assert(!instance->IsExternal() || (flags & PropertyOperation_NonFixedValue) != 0);
                    descriptor->IsFixed = (flags & PropertyOperation_NonFixedValue) == 0 && ShouldFixAccessorProperties();
                }
                if (!isGetterSet || !isSetterSet)
                {TRACE_IT(65669);
                    descriptor->IsOnlyOneAccessorInitialized = true;
                }
            }
            else if (descriptor->IsOnlyOneAccessorInitialized)
            {TRACE_IT(65670);
                // Only one of getter/setter was initialized, allow the isFixed to stay if we are defining the other one.
                Var oldGetter = GetSlot(instance, descriptor->GetGetterPropertyIndex());
                Var oldSetter = GetSlot(instance, descriptor->GetSetterPropertyIndex());

                if (((getter == oldGetter || !isGetterSet) && oldSetter == library->GetDefaultAccessorFunction()) ||
                    ((setter == oldSetter || !isSetterSet) && oldGetter == library->GetDefaultAccessorFunction()))
                {TRACE_IT(65671);
                    descriptor->IsOnlyOneAccessorInitialized = false;
                }
                else
                {
                    InvalidateFixedField(instance, propertyId, descriptor);
                }
            }
            else
            {
                InvalidateFixedField(instance, propertyId, descriptor);
            }

            // don't overwrite an existing accessor with null
            if (getter != nullptr)
            {TRACE_IT(65672);
                getter = CanonicalizeAccessor(getter, library);
                SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), getter);
            }
            if (setter != nullptr)
            {TRACE_IT(65673);
                setter = CanonicalizeAccessor(setter, library);
                SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), setter);
            }
            instance->ChangeType();
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {TRACE_IT(65674);
                scriptContext->InvalidateStoreFieldCaches(propertyId);
                library->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
            SetPropertyUpdateSideEffect(instance, propertyId, nullptr, SideEffects_Any);

            // Let's make sure we always have a getter and a setter
            Assert(instance->GetSlot(descriptor->GetGetterPropertyIndex()) != nullptr && instance->GetSlot(descriptor->GetSetterPropertyIndex()) != nullptr);

            return true;
        }

        // Always check numeric propertyRecord. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65675);
            // Calls this or subclass implementation
            return SetItemAccessors(instance, propertyRecord->GetNumericValue(), getter, setter);
        }

        getter = CanonicalizeAccessor(getter, library);
        setter = CanonicalizeAccessor(setter, library);
        T getterIndex = nextPropertyIndex++;
        T setterIndex = nextPropertyIndex++;
        DictionaryPropertyDescriptor<T> newDescriptor(getterIndex, setterIndex);
        if (this->GetSlotCapacity() <= nextPropertyIndex)
        {TRACE_IT(65676);
            if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
            {TRACE_IT(65677);
                Throw::OutOfMemory();
            }

            this->EnsureSlotCapacity(instance);
        }

        // DictionaryTypeHandlers are not supposed to be shared.
        Assert(!GetIsOrMayBecomeShared());
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);
        newDescriptor.IsInitialized = true;
        if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId))
        {TRACE_IT(65678);
            // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
            Assert(!instance->IsExternal() || (flags & PropertyOperation_NonFixedValue) != 0);

            // Even if one (or both?) accessors are the default functions obtained through canonicalization,
            // they are still legitimate functions, so it's ok to mark the whole property as fixed.
            newDescriptor.IsFixed = (flags & PropertyOperation_NonFixedValue) == 0 && ShouldFixAccessorProperties();
            if (!isGetterSet || !isSetterSet)
            {TRACE_IT(65679);
                newDescriptor.IsOnlyOneAccessorInitialized = true;
            }
        }

        propertyMap->Add(propertyRecord, newDescriptor);

        SetSlotUnchecked(instance, newDescriptor.GetGetterPropertyIndex(), getter);
        SetSlotUnchecked(instance, newDescriptor.GetSetterPropertyIndex(), setter);
        this->ClearHasOnlyWritableDataProperties();
        if(GetFlags() & IsPrototypeFlag)
        {TRACE_IT(65680);
            scriptContext->InvalidateStoreFieldCaches(propertyId);
            library->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
        SetPropertyUpdateSideEffect(instance, propertyId, nullptr, SideEffects_Any);

        // Let's make sure we always have a getter and a setter
        Assert(instance->GetSlot(newDescriptor.GetGetterPropertyIndex()) != nullptr && instance->GetSlot(newDescriptor.GetSetterPropertyIndex()) != nullptr);

        return true;
    }

    // If this type is not extensible and the property being set does not already exist,
    // if throwIfNotExtensible is
    // * true, a type error will be thrown
    // * false, FALSE will be returned (unless strict mode is enabled, in which case a type error will be thrown).
    // Either way, the property will not be set.
    //
    // This is used to ensure that we throw in the following scenario, in accordance with
    // section 10.2.1.2.2 of the Errata to the ES5 spec:
    //    Object.preventExtension(this);  // make the global object non-extensible
    //    var x = 4;
    //
    // throwIfNotExtensible should always be false for non-numeric properties.
    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value,
        PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(65681);
        DictionaryPropertyDescriptor<T>* descriptor;
        ScriptContext* scriptContext = instance->GetScriptContext();
        bool isForce = (flags & PropertyOperation_Force) != 0;
        bool throwIfNotExtensible = (flags & PropertyOperation_ThrowIfNotExtensible) != 0;

#ifdef DEBUG
        uint32 debugIndex;
        Assert(!(throwIfNotExtensible && scriptContext->IsNumericPropertyId(propertyId, &debugIndex)));
#endif
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65682);
            Assert(descriptor->SanityCheckFixedBits());

            if (attributes & descriptor->Attributes & PropertyLetConstGlobal)
            {TRACE_IT(65683);
                // Do not need to change the descriptor or its attributes if setting the initial value of a LetConstGlobal
            }
            else if (descriptor->Attributes & PropertyDeleted && !(attributes & PropertyLetConstGlobal))
            {TRACE_IT(65684);
                if (!isForce)
                {TRACE_IT(65685);
                    if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                    {TRACE_IT(65686);
                        return FALSE;
                    }
                }

                scriptContext->InvalidateProtoCaches(propertyId);
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {TRACE_IT(65687);
                    descriptor->Attributes = attributes | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {TRACE_IT(65688);
                    descriptor->Attributes = attributes;
                }
                descriptor->ConvertToData();
            }
            else if (descriptor->IsShadowed)
            {TRACE_IT(65689);
                descriptor->Attributes = attributes | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
            }
            else if ((descriptor->Attributes & PropertyLetConstGlobal) != (attributes & PropertyLetConstGlobal))
            {TRACE_IT(65690);
                bool addingLetConstGlobal = (attributes & PropertyLetConstGlobal) != 0;

                if (addingLetConstGlobal)
                {TRACE_IT(65691);
                    descriptor->Attributes = descriptor->Attributes | (attributes & PropertyNoRedecl);
                }
                else
                {TRACE_IT(65692);
                    descriptor->Attributes = attributes | (descriptor->Attributes & PropertyNoRedecl);
                }

                descriptor->AddShadowedData(nextPropertyIndex, addingLetConstGlobal);

                if (this->GetSlotCapacity() <= nextPropertyIndex)
                {TRACE_IT(65693);
                    if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
                    {TRACE_IT(65694);
                        Throw::OutOfMemory();
                    }

                    this->EnsureSlotCapacity(instance);
                }

                if (addingLetConstGlobal)
                {TRACE_IT(65695);
                    // If shadowing a global property with a let/const, need to invalidate
                    // JIT fast path cache since look up could now go to the let/const instead
                    // of the global property.
                    //
                    // Do not need to invalidate when adding a global property that gets shadowed
                    // by an existing let/const, since all caches will still be correct.
                    instance->ChangeType();
                }
            }
            else
            {TRACE_IT(65696);
                if (descriptor->IsAccessor && !(attributes & PropertyLetConstGlobal))
                {TRACE_IT(65697);
                    AssertMsg(RootObjectBase::Is(instance) || JavascriptFunction::IsBuiltinProperty(instance, propertyId) ||
                        // ValidateAndApplyPropertyDescriptor says to preserve Configurable and Enumerable flags

                        // For InitRootFld, which is equivalent to
                        // CreateGlobalFunctionBinding called from GlobalDeclarationInstantiation in the spec,
                        // we can assume that the attributes specified include enumerable and writable.  Thus
                        // we don't need to preserve the original values of these two attributes and therefore
                        // do not need to change InitRootFld from being a SetPropertyWithAttributes API call to
                        // something else.  All we need to do is convert the descriptor to a data descriptor.
                        // Built-in Function.prototype properties 'length', 'arguments', and 'caller' are special cases.

                        (JavascriptOperators::IsClassConstructor(JavascriptOperators::GetProperty(instance, PropertyIds::constructor, scriptContext)) &&
                            (attributes & PropertyClassMemberDefaults) == PropertyClassMemberDefaults),
                        // 14.3.9: InitClassMember sets property descriptor to {writable:true, enumerable:false, configurable:true}

                        "Expect to only come down this path for InitClassMember or InitRootFld (on the global object) overwriting existing accessor property");
                    if (!(descriptor->Attributes & PropertyConfigurable))
                    {TRACE_IT(65698);
                        if (scriptContext && scriptContext->GetThreadContext()->RecordImplicitException())
                        {TRACE_IT(65699);
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable, scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
                        }
                        return FALSE;
                    }
                    descriptor->ConvertToData();
                    instance->ChangeType();
                }

                // Make sure to keep the PropertyLetConstGlobal bit as is while taking the new attributes.
                descriptor->Attributes = attributes | (descriptor->Attributes & PropertyLetConstGlobal);
            }

            if (attributes & PropertyLetConstGlobal)
            {
                SetPropertyWithDescriptor<true>(instance, propertyId, descriptor, value, flags, info);
            }
            else
            {
                SetPropertyWithDescriptor<false>(instance, propertyId, descriptor, value, flags, info);
            }

            if (descriptor->Attributes & PropertyEnumerable)
            {TRACE_IT(65700);
                instance->SetHasNoEnumerableProperties(false);
            }

            if (!(descriptor->Attributes & PropertyWritable))
            {TRACE_IT(65701);
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {TRACE_IT(65702);
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }

            SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
            return true;
        }

        // Always check numeric propertyRecord. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {TRACE_IT(65703);
            // Calls this or subclass implementation
            return SetItemWithAttributes(instance, propertyRecord->GetNumericValue(), value, attributes);
        }

        return this->AddProperty(instance, propertyRecord, value, attributes, info, flags, throwIfNotExtensible, possibleSideEffects);
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::EnsureSlotCapacity(DynamicObject * instance)
    {TRACE_IT(65704);
        Assert(this->GetSlotCapacity() < MaxPropertyIndexSize); // Otherwise we can't grow this handler's capacity. We should've evolved to Bigger handler or OOM.

        // A Dictionary type is expected to have more properties
        // grow exponentially rather linearly to avoid the realloc and moves,
        // however use a small exponent to avoid waste
        int newSlotCapacity = (nextPropertyIndex + 1);
        newSlotCapacity += (newSlotCapacity>>2);
        if (newSlotCapacity > MaxPropertyIndexSize)
        {TRACE_IT(65705);
            newSlotCapacity = MaxPropertyIndexSize;
        }
        newSlotCapacity = RoundUpSlotCapacity(newSlotCapacity, GetInlineSlotCapacity());
        Assert(newSlotCapacity <= MaxPropertyIndexSize);

        instance->EnsureSlots(this->GetSlotCapacity(), newSlotCapacity, instance->GetScriptContext(), this);
        this->SetSlotCapacity(newSlotCapacity);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {TRACE_IT(65706);
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        ScriptContext* scriptContext = instance->GetScriptContext();
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65707);
            if (descriptor->Attributes & PropertyDeleted)
            {TRACE_IT(65708);
                return false;
            }

            descriptor->Attributes = (descriptor->Attributes & ~PropertyDynamicTypeDefaults) | (attributes & PropertyDynamicTypeDefaults);

            if (descriptor->Attributes & PropertyEnumerable)
            {TRACE_IT(65709);
                instance->SetHasNoEnumerableProperties(false);
            }

            if (!(descriptor->Attributes & PropertyWritable))
            {TRACE_IT(65710);
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {TRACE_IT(65711);
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }

            return true;
        }

        // Check numeric propertyId only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {TRACE_IT(65712);
            return DictionaryTypeHandlerBase<T>::SetItemAttributes(instance, propertyRecord->GetNumericValue(), attributes);
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {TRACE_IT(65713);
        // this might get value that are deleted from the dictionary, but that should be nulled out
        DictionaryPropertyDescriptor<T> * descriptor;
        // We can't look it up using the slot index, as one propertyId might have multiple slots,  do the propertyId map lookup
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (!propertyMap->TryGetReference(propertyRecord, &descriptor))
        {TRACE_IT(65714);
            return false;
        }
        // This function is only used by LdRootFld, so the index will allow let const globals
        Assert(descriptor->template GetDataPropertyIndex<true>() == index);
        if (descriptor->Attributes & PropertyDeleted)
        {TRACE_IT(65715);
            return false;
        }
        *attributes = descriptor->Attributes & PropertyDynamicTypeDefaults;
        return true;
    }

    template <typename T>
    Var DictionaryTypeHandlerBase<T>::CanonicalizeAccessor(Var accessor, /*const*/ JavascriptLibrary* library)
    {TRACE_IT(65716);
        if (accessor == nullptr || JavascriptOperators::IsUndefinedObject(accessor, library))
        {TRACE_IT(65717);
            accessor = library->GetDefaultAccessorFunction();
        }
        return accessor;
    }

    template <typename T>
    BigDictionaryTypeHandler* DictionaryTypeHandlerBase<T>::ConvertToBigDictionaryTypeHandler(DynamicObject* instance)
    {TRACE_IT(65718);
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        BigDictionaryTypeHandler* newTypeHandler = NewBigDictionaryTypeHandler(recycler, GetSlotCapacity(), GetInlineSlotCapacity(), GetOffsetOfInlineSlots());
        // We expect the new type handler to start off marked as having only writable data properties.
        Assert(newTypeHandler->GetHasOnlyWritableDataProperties());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        DynamicType* oldType = instance->GetDynamicType();
        RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
        TraceFixedFieldsBeforeTypeHandlerChange(_u("DictionaryTypeHandler"), _u("BigDictionaryTypeHandler"), instance, this, oldType, oldSingletonInstance);
#endif

        CopySingletonInstance(instance, newTypeHandler);

        DictionaryPropertyDescriptor<T> descriptor;
        DictionaryPropertyDescriptor<BigPropertyIndex> bigDescriptor;

        const PropertyRecord* propertyId;
        for (int i = 0; i < propertyMap->Count(); i++)
        {TRACE_IT(65719);
            descriptor = propertyMap->GetValueAt(i);
            propertyId = propertyMap->GetKeyAt(i);

            bigDescriptor.CopyFrom(descriptor);
            newTypeHandler->propertyMap->Add(propertyId, bigDescriptor);
        }

        newTypeHandler->nextPropertyIndex = nextPropertyIndex;

        ClearSingletonInstance();

        AssertMsg((newTypeHandler->GetFlags() & IsPrototypeFlag) == 0, "Why did we create a brand new type handler with a prototype flag set?");
        newTypeHandler->SetFlags(IsPrototypeFlag, this->GetFlags());
        newTypeHandler->ChangeFlags(IsExtensibleFlag, this->GetFlags());
        // Any new type handler we expect to see here should have inline slot capacity locked.  If this were to change, we would need
        // to update our shrinking logic (see PathTypeHandlerBase::ShrinkSlotAndInlineSlotCapacity).
        Assert(newTypeHandler->GetIsInlineSlotCapacityLocked());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection,  this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);
        // Unlike for SimpleDictionaryTypeHandler or PathTypeHandler, the DictionaryTypeHandler copies usedAsFixed indiscriminately above.
        // Therefore, we don't care if we changed the type or not, and don't need the assert below.
        // We assumed that we don't need to transfer used as fixed bits unless we are a prototype, which is only valid if we also changed the type.
        // Assert(instance->GetType() != oldType);
        Assert(!newTypeHandler->HasSingletonInstance() || !instance->HasSharedType());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        TraceFixedFieldsAfterTypeHandlerChange(instance, this, newTypeHandler, oldType, oldSingletonInstance);
#endif

        return newTypeHandler;
    }

    template <typename T>
    BigDictionaryTypeHandler* DictionaryTypeHandlerBase<T>::NewBigDictionaryTypeHandler(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {TRACE_IT(65720);
        return RecyclerNew(recycler, BigDictionaryTypeHandler, recycler, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots);
    }

    template <>
    BigDictionaryTypeHandler* DictionaryTypeHandlerBase<BigPropertyIndex>::ConvertToBigDictionaryTypeHandler(DynamicObject* instance)
    {TRACE_IT(65721);
        Throw::OutOfMemory();
    }

    template<>
    BOOL DictionaryTypeHandlerBase<PropertyIndex>::IsBigDictionaryTypeHandler()
    {TRACE_IT(65722);
        return FALSE;
    }

    template<>
    BOOL DictionaryTypeHandlerBase<BigPropertyIndex>::IsBigDictionaryTypeHandler()
    {TRACE_IT(65723);
        return TRUE;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::AddProperty(DynamicObject* instance, const PropertyRecord* propertyRecord, Var value,
        PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, bool throwIfNotExtensible, SideEffects possibleSideEffects)
    {TRACE_IT(65724);
        AnalysisAssert(instance);
        ScriptContext* scriptContext = instance->GetScriptContext();
        bool isForce = (flags & PropertyOperation_Force) != 0;

#if DBG
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(!propertyMap->TryGetReference(propertyRecord, &descriptor));
        Assert(!propertyRecord->IsNumeric());
#endif

        if (!isForce)
        {TRACE_IT(65725);
            if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
            {TRACE_IT(65726);
                return FALSE;
            }
        }

        if (this->GetSlotCapacity() <= nextPropertyIndex)
        {TRACE_IT(65727);
            if (this->GetSlotCapacity() >= MaxPropertyIndexSize ||
                (this->GetSlotCapacity() >= CONFIG_FLAG(BigDictionaryTypeHandlerThreshold) && !this->IsBigDictionaryTypeHandler()))
            {TRACE_IT(65728);
                BigDictionaryTypeHandler* newTypeHandler = ConvertToBigDictionaryTypeHandler(instance);

                return newTypeHandler->AddProperty(instance, propertyRecord, value, attributes, info, flags, false, possibleSideEffects);
            }
            this->EnsureSlotCapacity(instance);
        }

        T index = nextPropertyIndex++;
        DictionaryPropertyDescriptor<T> newDescriptor(index, attributes);

        // DictionaryTypeHandlers are not supposed to be shared.
        Assert(!GetIsOrMayBecomeShared());
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);

        if ((flags & PropertyOperation_PreInit) == 0)
        {TRACE_IT(65729);
            newDescriptor.IsInitialized = true;
            if (localSingletonInstance == instance && !IsInternalPropertyId(propertyRecord->GetPropertyId()) &&
                (flags & (PropertyOperation_NonFixedValue | PropertyOperation_SpecialValue)) == 0)
            {TRACE_IT(65730);
                Assert(value != nullptr);
                // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                Assert(!instance->IsExternal());
                newDescriptor.IsFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() & CheckHeuristicsForFixedDataProps(instance, propertyRecord, value)));
            }
        }

        propertyMap->Add(propertyRecord, newDescriptor);

        if (attributes & PropertyEnumerable)
        {TRACE_IT(65731);
            instance->SetHasNoEnumerableProperties(false);
        }

        if (!(attributes & PropertyWritable))
        {TRACE_IT(65732);
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {TRACE_IT(65733);
                instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyRecord->GetPropertyId());
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }

        SetSlotUnchecked(instance, index, value);

        // If we just added a fixed method, don't populate the inline cache so that we always take the
        // slow path when overwriting this property and correctly invalidate any JIT-ed code that hard-coded
        // this method.
        if (newDescriptor.IsFixed)
        {TRACE_IT(65734);
            PropertyValueInfo::SetNoCache(info, instance);
        }
        else
        {
            SetPropertyValueInfo(info, instance, index, attributes);
        }

        if (!IsInternalPropertyId(propertyRecord->GetPropertyId()) && ((this->GetFlags() & IsPrototypeFlag)
            || JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(instance, propertyRecord->GetPropertyId())))
        {TRACE_IT(65735);
            // We don't evolve dictionary types when adding a field, so we need to invalidate prototype caches.
            // We only have to do this though if the current type is used as a prototype, or the current property
            // is found on the prototype chain.
            scriptContext->InvalidateProtoCaches(propertyRecord->GetPropertyId());
        }
        SetPropertyUpdateSideEffect(instance, propertyRecord->GetPropertyId(), value, possibleSideEffects);
        return true;
    }

    //
    // Converts (upgrades) this dictionary type handler to an ES5 array type handler. The new handler takes
    // over all members of this handler including the property map.
    //
    template <typename T>
    ES5ArrayTypeHandlerBase<T>* DictionaryTypeHandlerBase<T>::ConvertToES5ArrayType(DynamicObject *instance)
    {TRACE_IT(65736);
        Recycler* recycler = instance->GetRecycler();

        ES5ArrayTypeHandlerBase<T>* newTypeHandler = RecyclerNew(recycler, ES5ArrayTypeHandlerBase<T>, recycler, this);
        // Don't need to transfer the singleton instance, because the new handler takes over this handler.
        AssertMsg((newTypeHandler->GetFlags() & IsPrototypeFlag) == 0, "Why did we create a brand new type handler with a prototype flag set?");
        newTypeHandler->SetFlags(IsPrototypeFlag, this->GetFlags());
        // Property types were copied in the constructor.
        //newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection | PropertyTypesInlineSlotCapacityLocked, this->GetPropertyTypes());
        newTypeHandler->SetInstanceTypeHandler(instance);
        return newTypeHandler;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields)
    {TRACE_IT(65737);
        // The Var for window is reused across navigation. we shouldn't preserve the IsExtensibleFlag when we don't keep
        // the expandos. Reset the IsExtensibleFlag in cleanup scenario should be good enough
        // to cover all the preventExtension/Freeze/Seal scenarios.
        // Note that we don't change the flag for keepProperties scenario: the flags should be preserved and that's consistent
        // with other browsers.
        ChangeFlags(IsExtensibleFlag | IsSealedOnceFlag | IsFrozenOnceFlag, IsExtensibleFlag);

        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        int propertyCount = this->propertyMap->Count();

        if (invalidateFixedFields)
        {TRACE_IT(65738);
            for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {TRACE_IT(65739);
                const PropertyRecord* propertyRecord = this->propertyMap->GetKeyAt(propertyIndex);
                DictionaryPropertyDescriptor<T>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);
                InvalidateFixedField(instance, propertyRecord->GetPropertyId(), descriptor);
            }
        }

        Js::RecyclableObject* undefined = instance->GetLibrary()->GetUndefined();
        Js::JavascriptFunction* defaultAccessor = instance->GetLibrary()->GetDefaultAccessorFunction();
        for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
        {TRACE_IT(65740);
            DictionaryPropertyDescriptor<T>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);

            T dataPropertyIndex = descriptor->template GetDataPropertyIndex<false>();
            if (dataPropertyIndex != NoSlots)
            {
                SetSlotUnchecked(instance, dataPropertyIndex, undefined);
            }
            else
            {
                SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), defaultAccessor);
                SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), defaultAccessor);
            }
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields)
    {TRACE_IT(65741);
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        if (invalidateFixedFields)
        {TRACE_IT(65742);
            int propertyCount = this->propertyMap->Count();
            for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {TRACE_IT(65743);
                const PropertyRecord* propertyRecord = this->propertyMap->GetKeyAt(propertyIndex);
                DictionaryPropertyDescriptor<T>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);
                InvalidateFixedField(instance, propertyRecord->GetPropertyId(), descriptor);
            }
        }

        int slotCount = this->nextPropertyIndex;
        for (int slotIndex = 0; slotIndex < slotCount; slotIndex++)
        {
            SetSlotUnchecked(instance, slotIndex, CrossSite::MarshalVar(targetScriptContext, GetSlot(instance, slotIndex)));
        }
    }

    template <typename T>
    DynamicTypeHandler* DictionaryTypeHandlerBase<T>::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {TRACE_IT(65744);
        return JavascriptArray::Is(instance) ? ConvertToES5ArrayType(instance) : this;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::SetIsPrototype(DynamicObject* instance)
    {TRACE_IT(65745);
        // Don't return if IsPrototypeFlag is set, because we may still need to do a type transition and
        // set fixed bits.  If this handler were to be shared, this instance may not be a prototype yet.
        // We might need to convert to a non-shared type handler and/or change type.
        if (!ChangeTypeOnProto() && !(GetIsOrMayBecomeShared() && IsolatePrototypes()))
        {TRACE_IT(65746);
            SetFlags(IsPrototypeFlag);
            return;
        }

        Assert(!GetIsShared() || this->singletonInstance == nullptr);
        Assert(this->singletonInstance == nullptr || this->singletonInstance->Get() == instance);

        // Review (jedmiad): Why isn't this getting inlined?
        const auto setFixedFlags = [instance](const PropertyRecord* propertyRecord, DictionaryPropertyDescriptor<T>* const descriptor, bool hasNewType)
        {TRACE_IT(65747);
            if (IsInternalPropertyId(propertyRecord->GetPropertyId()))
            {TRACE_IT(65748);
                return;
            }
            if (!(descriptor->Attributes & PropertyDeleted))
            {TRACE_IT(65749);
                // See PathTypeHandlerBase::ConvertToSimpleDictionaryType for rules governing fixed field bits during type
                // handler transitions.  In addition, we know that the current instance is not yet a prototype.

                Assert(descriptor->SanityCheckFixedBits());
                if (descriptor->IsInitialized)
                {TRACE_IT(65750);
                    // Since DictionaryTypeHandlers are never shared, we can set fixed fields and clear used as fixed as long
                    // as we have changed the type.  Otherwise populated load field caches would still be valid and would need
                    // to be explicitly invalidated if the property value changes.
                    if (hasNewType)
                    {TRACE_IT(65751);
                        T dataSlot = descriptor->template GetDataPropertyIndex<false>();
                        if (dataSlot != NoSlots)
                        {TRACE_IT(65752);
                            Var value = instance->GetSlot(dataSlot);
                            // Because DictionaryTypeHandlers are never shared we should always have a property value if the handler
                            // says it's initialized.
                            Assert(value != nullptr);
                            descriptor->IsFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyRecord, value)));
                        }
                        else if (descriptor->IsAccessor)
                        {TRACE_IT(65753);
                            Assert(descriptor->GetGetterPropertyIndex() != NoSlots && descriptor->GetSetterPropertyIndex() != NoSlots);
                            descriptor->IsFixed = ShouldFixAccessorProperties();
                        }

                        // Since we have a new type we can clear all used as fixed bits.  That's because any instance field loads
                        // will have been invalidated by the type transition, and there are no proto fields loads from this object
                        // because it is just now becoming a proto.
                        descriptor->UsedAsFixed = false;
                    }
                }
                else
                {TRACE_IT(65754);
                    Assert(!descriptor->IsFixed && !descriptor->UsedAsFixed);
                }
                Assert(descriptor->SanityCheckFixedBits());
            }
        };

        // DictionaryTypeHandlers are never shared. If we allow sharing, we will have to handle this case
        // just like SimpleDictionaryTypeHandler.
        Assert(!GetIsOrMayBecomeShared());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        DynamicType* oldType = instance->GetDynamicType();
        RecyclerWeakReference<DynamicObject>* oldSingletonInstance = GetSingletonInstance();
        TraceFixedFieldsBeforeSetIsProto(instance, this, oldType, oldSingletonInstance);
#endif

        bool hasNewType = false;
        if (ChangeTypeOnProto())
        {TRACE_IT(65755);
            // Forcing a type transition allows us to fix all fields (even those that were previously marked as non-fixed).
            instance->ChangeType();
            Assert(!instance->HasSharedType());
            hasNewType = true;
        }

        // Currently there is no way to become the prototype if you are a stack instance
        Assert(!ThreadContext::IsOnStack(instance));
        if (AreSingletonInstancesNeeded() && this->singletonInstance == nullptr)
        {TRACE_IT(65756);
            this->singletonInstance = instance->CreateWeakReferenceToSelf();
        }

        // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
        if (!instance->IsExternal())
        {TRACE_IT(65757);
            // The propertyMap dictionary is guaranteed to have contiguous entries because we never remove entries from it.
            for (int i = 0; i < propertyMap->Count(); i++)
            {TRACE_IT(65758);
                const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(i);
                DictionaryPropertyDescriptor<T>* const descriptor = propertyMap->GetReferenceAt(i);
                setFixedFlags(propertyRecord, descriptor, hasNewType);
            }
        }

        SetFlags(IsPrototypeFlag);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        TraceFixedFieldsAfterSetIsProto(instance, this, this, oldType, oldSingletonInstance);
#endif

    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::HasSingletonInstance() const
    {TRACE_IT(65759);
        return this->singletonInstance != nullptr;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::TryUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {TRACE_IT(65760);
        bool result = TryGetFixedProperty<false, true>(propertyRecord, pProperty, propertyType, requestContext);
        TraceUseFixedProperty(propertyRecord, pProperty, result, _u("DictionaryTypeHandler"), requestContext);
        return result;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::TryUseFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {TRACE_IT(65761);
        bool result = TryGetFixedAccessor<false, true>(propertyRecord, pAccessor, propertyType, getter, requestContext);
        TraceUseFixedProperty(propertyRecord, pAccessor, result, _u("DictionaryTypeHandler"), requestContext);
        return result;
    }

#if DBG
    template <typename T>
    bool DictionaryTypeHandlerBase<T>::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {TRACE_IT(65762);
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T> descriptor;

        // We pass Constants::NoProperty for ActivationObjects for functions with same named formals.
        if (propertyId == Constants::NoProperty)
        {TRACE_IT(65763);
            return true;
        }

        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetValue(propertyRecord, &descriptor))
        {TRACE_IT(65764);
            if (allowLetConst && (descriptor.Attributes & PropertyLetConstGlobal))
            {TRACE_IT(65765);
                return true;
            }
            else
            {TRACE_IT(65766);
                return descriptor.IsInitialized && !descriptor.IsFixed;
            }
        }
        else
        {
            AssertMsg(false, "Asking about a property this type handler doesn't know about?");
            return false;
        }
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::CheckFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, ScriptContext * requestContext)
    {TRACE_IT(65767);
        return TryGetFixedProperty<true, false>(propertyRecord, pProperty, (Js::FixedPropertyKind)(Js::FixedPropertyKind::FixedMethodProperty | Js::FixedPropertyKind::FixedDataProperty), requestContext);
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::HasAnyFixedProperties() const
    {TRACE_IT(65768);
        for (int i = 0; i < propertyMap->Count(); i++)
        {TRACE_IT(65769);
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(i);
            if (descriptor.IsFixed)
            {TRACE_IT(65770);
                return true;
            }
        }
        return false;
    }
#endif

    template <typename T>
    template <bool allowNonExistent, bool markAsUsed>
    bool DictionaryTypeHandlerBase<T>::TryGetFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {TRACE_IT(65771);
        // Note: This function is not thread-safe and cannot be called from the JIT thread.  That's why we collect and
        // cache any fixed function instances during work item creation on the main thread.
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        if (localSingletonInstance != nullptr && localSingletonInstance->GetScriptContext() == requestContext)
        {TRACE_IT(65772);
            DictionaryPropertyDescriptor<T>* descriptor;
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {TRACE_IT(65773);
                if (descriptor->Attributes & PropertyDeleted || !descriptor->IsFixed)
                {TRACE_IT(65774);
                    return false;
                }
                T dataSlot = descriptor->template GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots)
                {TRACE_IT(65775);
                    Assert(!IsInternalPropertyId(propertyRecord->GetPropertyId()));
                    Var value = localSingletonInstance->GetSlot(dataSlot);
                    if (value && ((IsFixedMethodProperty(propertyType) && JavascriptFunction::Is(value)) || IsFixedDataProperty(propertyType)))
                    {TRACE_IT(65776);
                        *pProperty = value;
                        if (markAsUsed)
                        {TRACE_IT(65777);
                            descriptor->UsedAsFixed = true;
                        }
                        return true;
                    }
                }
            }
            else
            {
                AssertMsg(allowNonExistent, "Trying to get a fixed function instance for a non-existent property?");
            }
        }

        return false;
    }

    template <typename T>
    template <bool allowNonExistent, bool markAsUsed>
    bool DictionaryTypeHandlerBase<T>::TryGetFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {TRACE_IT(65778);
        // Note: This function is not thread-safe and cannot be called from the JIT thread.  That's why we collect and
        // cache any fixed function instances during work item creation on the main thread.
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        if (localSingletonInstance != nullptr && localSingletonInstance->GetScriptContext() == requestContext)
        {TRACE_IT(65779);
            DictionaryPropertyDescriptor<T>* descriptor;
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {TRACE_IT(65780);
                if (descriptor->Attributes & PropertyDeleted || !descriptor->IsAccessor || !descriptor->IsFixed)
                {TRACE_IT(65781);
                    return false;
                }

                T accessorSlot = getter ? descriptor->GetGetterPropertyIndex() : descriptor->GetSetterPropertyIndex();
                if (accessorSlot != NoSlots)
                {TRACE_IT(65782);
                    Assert(!IsInternalPropertyId(propertyRecord->GetPropertyId()));
                    Var value = localSingletonInstance->GetSlot(accessorSlot);
                    if (value && IsFixedAccessorProperty(propertyType) && JavascriptFunction::Is(value))
                    {TRACE_IT(65783);
                        *pAccessor = value;
                        if (markAsUsed)
                        {TRACE_IT(65784);
                            descriptor->UsedAsFixed = true;
                        }
                        return true;
                    }
                }
            }
            else
            {
                AssertMsg(allowNonExistent, "Trying to get a fixed function instance for a non-existent property?");
            }
        }

        return false;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::CopySingletonInstance(DynamicObject* instance, DynamicTypeHandler* typeHandler)
    {TRACE_IT(65785);
        if (this->singletonInstance != nullptr)
        {TRACE_IT(65786);
            Assert(AreSingletonInstancesNeeded());
            Assert(this->singletonInstance->Get() == instance);
            typeHandler->SetSingletonInstanceUnchecked(this->singletonInstance);
        }
    }

    template <typename T>
    template <typename TPropertyKey>
    void DictionaryTypeHandlerBase<T>::InvalidateFixedField(DynamicObject* instance, TPropertyKey propertyKey, DictionaryPropertyDescriptor<T>* descriptor)
    {TRACE_IT(65787);
        // DictionaryTypeHandlers are never shared, but if they were we would need to invalidate even if
        // there wasn't a singleton instance.  See SimpleDictionaryTypeHandler::InvalidateFixedFields.
        Assert(!GetIsOrMayBecomeShared());
        if (this->singletonInstance != nullptr)
        {TRACE_IT(65788);
            Assert(this->singletonInstance->Get() == instance);

            // Even if we wrote a new value into this property (overwriting a previously fixed one), we don't
            // consider the new one fixed. This also means that it's ok to populate the inline caches for
            // this property from now on.
            descriptor->IsFixed = false;

            if (descriptor->UsedAsFixed)
            {TRACE_IT(65789);
                // Invalidate any JIT-ed code that hard coded this method. No need to invalidate
                // any store field inline caches, because they have never been populated.
#if ENABLE_NATIVE_CODEGEN
                PropertyId propertyId = TMapKey_GetPropertyId(instance->GetScriptContext(), propertyKey);
                instance->GetScriptContext()->GetThreadContext()->InvalidatePropertyGuards(propertyId);
#endif
                descriptor->UsedAsFixed = false;
            }
        }
    }

#if DBG
    template <typename T>
    bool DictionaryTypeHandlerBase<T>::IsLetConstGlobal(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(65790);
        DictionaryPropertyDescriptor<T>* descriptor;
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && (descriptor->Attributes & PropertyLetConstGlobal))
        {TRACE_IT(65791);
            return true;
        }
        return false;
    }
#endif

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::NextLetConstGlobal(int& index, RootObjectBase* instance, const PropertyRecord** propertyRecord, Var* value, bool* isConst)
    {TRACE_IT(65792);
        for (; index < propertyMap->Count(); index++)
        {TRACE_IT(65793);
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);

            if (descriptor.Attributes & PropertyLetConstGlobal)
            {TRACE_IT(65794);
                *propertyRecord = propertyMap->GetKeyAt(index);
                *value = instance->GetSlot(descriptor.template GetDataPropertyIndex<true>());
                *isConst = (descriptor.Attributes & PropertyConst) != 0;

                index += 1;

                return true;
            }
        }

        return false;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    template <typename T>
    void DictionaryTypeHandlerBase<T>::DumpFixedFields() const {TRACE_IT(65795);
        for (int i = 0; i < propertyMap->Count(); i++)
        {TRACE_IT(65796);
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(i);

            const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(i);
            Output::Print(_u(" %s %d%d%d,"), propertyRecord->GetBuffer(),
                descriptor.IsInitialized ? 1 : 0, descriptor.IsFixed ? 1 : 0, descriptor.UsedAsFixed ? 1 : 0);
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::TraceFixedFieldsBeforeTypeHandlerChange(
        const char16* oldTypeHandlerName, const char16* newTypeHandlerName,
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler,
        DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {TRACE_IT(65797);
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {TRACE_IT(65798);
            Output::Print(_u("FixedFields: converting 0x%p from %s to %s:\n"), instance, oldTypeHandlerName, newTypeHandlerName);
            Output::Print(_u("   before: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p)\n"),
                oldType, oldTypeHandler, oldSingletonInstanceBefore, oldSingletonInstanceBefore != nullptr ? oldSingletonInstanceBefore->Get() : nullptr);
            Output::Print(_u("   fixed fields:"));
            oldTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {TRACE_IT(65799);
            Output::Print(_u("FixedFields: converting instance from %s to %s:\n"), oldTypeHandlerName, newTypeHandlerName);
            Output::Print(_u("   old singleton before %s null \n"), oldSingletonInstanceBefore == nullptr ? _u("==") : _u("!="));
            Output::Print(_u("   fixed fields before:"));
            oldTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::TraceFixedFieldsAfterTypeHandlerChange(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
        DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {TRACE_IT(65800);
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {TRACE_IT(65801);
            RecyclerWeakReference<DynamicObject>* oldSingletonInstanceAfter = oldTypeHandler->GetSingletonInstance();
            RecyclerWeakReference<DynamicObject>* newSingletonInstanceAfter = newTypeHandler->GetSingletonInstance();
            Output::Print(_u("   after: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p), new singleton = 0x%p(0x%p)\n"),
                instance->GetType(), newTypeHandler,
                oldSingletonInstanceAfter, oldSingletonInstanceAfter != nullptr ? oldSingletonInstanceAfter->Get() : nullptr,
                newSingletonInstanceAfter, newSingletonInstanceAfter != nullptr ? newSingletonInstanceAfter->Get() : nullptr);
            Output::Print(_u("   fixed fields after:"));
            newTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
            Output::Flush();
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {TRACE_IT(65802);
            Output::Print(_u("   type %s, typeHandler %s, old singleton after %s null (%s), new singleton after %s null\n"),
                oldTypeHandler != newTypeHandler ? _u("changed") : _u("unchanged"),
                oldType != instance->GetType() ? _u("changed") : _u("unchanged"),
                oldSingletonInstanceBefore == nullptr ? _u("==") : _u("!="),
                oldSingletonInstanceBefore != oldTypeHandler->GetSingletonInstance() ? _u("changed") : _u("unchanged"),
                newTypeHandler->GetSingletonInstance() == nullptr ? _u("==") : _u("!="));
            Output::Print(_u("   fixed fields after:"));
            newTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
            Output::Flush();
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::TraceFixedFieldsBeforeSetIsProto(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {TRACE_IT(65803);
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {TRACE_IT(65804);
            Output::Print(_u("FixedFields: PathTypeHandler::SetIsPrototype(0x%p):\n"), instance);
            Output::Print(_u("   before: type = 0x%p, old singleton = 0x%p(0x%p)\n"),
                oldType, oldSingletonInstanceBefore, oldSingletonInstanceBefore != nullptr ? oldSingletonInstanceBefore->Get() : nullptr);
            Output::Print(_u("   fixed fields:"));
            oldTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {TRACE_IT(65805);
            Output::Print(_u("FixedFields: PathTypeHandler::SetIsPrototype():\n"));
            Output::Print(_u("   old singleton before %s null \n"), oldSingletonInstanceBefore == nullptr ? _u("==") : _u("!="));
            Output::Print(_u("   fixed fields before:"));
            oldTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
        }
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::TraceFixedFieldsAfterSetIsProto(
        DynamicObject* instance, DynamicTypeHandler* oldTypeHandler, DynamicTypeHandler* newTypeHandler,
        DynamicType* oldType, RecyclerWeakReference<DynamicObject>* oldSingletonInstanceBefore)
    {TRACE_IT(65806);
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {TRACE_IT(65807);
            RecyclerWeakReference<DynamicObject>* oldSingletonInstanceAfter = oldTypeHandler->GetSingletonInstance();
            RecyclerWeakReference<DynamicObject>* newSingletonInstanceAfter = newTypeHandler->GetSingletonInstance();
            Output::Print(_u("   after: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p), new singleton = 0x%p(0x%p)\n"),
                instance->GetType(), newTypeHandler,
                oldSingletonInstanceAfter, oldSingletonInstanceAfter != nullptr ? oldSingletonInstanceAfter->Get() : nullptr,
                newSingletonInstanceAfter, newSingletonInstanceAfter != nullptr ? newSingletonInstanceAfter->Get() : nullptr);
            Output::Print(_u("   fixed fields:"));
            newTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
            Output::Flush();
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {TRACE_IT(65808);
            Output::Print(_u("   type %s, old singleton after %s null (%s)\n"),
                oldType != instance->GetType() ? _u("changed") : _u("unchanged"),
                oldSingletonInstanceBefore == nullptr ? _u("==") : _u("!="),
                oldSingletonInstanceBefore != oldTypeHandler->GetSingletonInstance() ? _u("changed") : _u("unchanged"));
            Output::Print(_u("   fixed fields after:"));
            newTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
            Output::Flush();
        }
    }
#endif

#if ENABLE_TTD
    template <typename T>
    void DictionaryTypeHandlerBase<T>::MarkObjectSlots_TTD(TTD::SnapshotExtractor* extractor, DynamicObject* obj) const
    {TRACE_IT(65809);
        for(auto iter = this->propertyMap->GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(65810);
            DictionaryPropertyDescriptor<T> descriptor = iter.CurrentValue();

            //
            //TODO: not sure about relationship with PropertyLetConstGlobal here need to -- check how GetProperty works
            //      maybe we need to template this with allowLetGlobalConst as well
            //

            Js::PropertyId pid = iter.CurrentKey()->GetPropertyId();
            if((!DynamicTypeHandler::ShouldMarkPropertyId_TTD(pid)) | (!descriptor.IsInitialized) | (descriptor.Attributes & PropertyDeleted))
            {TRACE_IT(65811);
                continue;
            }

            T dIndex = descriptor.template GetDataPropertyIndex<false>();
            if(dIndex != NoSlots)
            {TRACE_IT(65812);
                Js::Var dValue = obj->GetSlot(dIndex);
                extractor->MarkVisitVar(dValue);
            }
            else
            {TRACE_IT(65813);
                T gIndex = descriptor.GetGetterPropertyIndex();
                if(gIndex != NoSlots)
                {TRACE_IT(65814);
                    Js::Var gValue = obj->GetSlot(gIndex);
                    extractor->MarkVisitVar(gValue);
                }

                T sIndex = descriptor.GetSetterPropertyIndex();
                if(sIndex != NoSlots)
                {TRACE_IT(65815);
                    Js::Var sValue = obj->GetSlot(sIndex);
                    extractor->MarkVisitVar(sValue);
                }
            }
        }
    }

    template <typename T>
    uint32 DictionaryTypeHandlerBase<T>::ExtractSlotInfo_TTD(TTD::NSSnapType::SnapHandlerPropertyEntry* entryInfo, ThreadContext* threadContext, TTD::SlabAllocator& alloc) const
    {TRACE_IT(65816);
        T maxSlot = 0;

        for(auto iter = this->propertyMap->GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(65817);
            DictionaryPropertyDescriptor<T> descriptor = iter.CurrentValue();
            Js::PropertyId pid = iter.CurrentKey()->GetPropertyId();

            T dIndex = descriptor.template GetDataPropertyIndex<false>();
            if(dIndex != NoSlots)
            {TRACE_IT(65818);
                maxSlot = max(maxSlot, dIndex);

                TTD::NSSnapType::SnapEntryDataKindTag tag = descriptor.IsInitialized ? TTD::NSSnapType::SnapEntryDataKindTag::Data : TTD::NSSnapType::SnapEntryDataKindTag::Uninitialized;
                TTD::NSSnapType::ExtractSnapPropertyEntryInfo(entryInfo + dIndex, pid, descriptor.Attributes, tag);
            }
            else
            {TRACE_IT(65819);
                TTDAssert(descriptor.IsInitialized, "How can this not be initialized?");

                T gIndex = descriptor.GetGetterPropertyIndex();
                if(gIndex != NoSlots)
                {TRACE_IT(65820);
                    maxSlot = max(maxSlot, gIndex);

                    TTD::NSSnapType::SnapEntryDataKindTag tag = TTD::NSSnapType::SnapEntryDataKindTag::Getter;
                    TTD::NSSnapType::ExtractSnapPropertyEntryInfo(entryInfo + gIndex, pid, descriptor.Attributes, tag);
                }

                T sIndex = descriptor.GetSetterPropertyIndex();
                if(sIndex != NoSlots)
                {TRACE_IT(65821);
                    maxSlot = max(maxSlot, sIndex);

                    TTD::NSSnapType::SnapEntryDataKindTag tag = TTD::NSSnapType::SnapEntryDataKindTag::Setter;
                    TTD::NSSnapType::ExtractSnapPropertyEntryInfo(entryInfo + sIndex, pid, descriptor.Attributes, tag);
                }
            }
        }

        if(this->propertyMap->Count() == 0)
        {TRACE_IT(65822);
            return 0;
        }
        else
        {TRACE_IT(65823);
            return (uint32)(maxSlot + 1);
        }
    }

    template <typename T>
    Js::BigPropertyIndex DictionaryTypeHandlerBase<T>::GetPropertyIndex_EnumerateTTD(const Js::PropertyRecord* pRecord)
    {TRACE_IT(65824);
        for(Js::BigPropertyIndex index = 0; index < this->propertyMap->Count(); index++)
        {TRACE_IT(65825);
            Js::PropertyId pid = this->propertyMap->GetKeyAt(index)->GetPropertyId();
            const DictionaryPropertyDescriptor<T>& idescriptor = propertyMap->GetValueAt(index);

            if(pid == pRecord->GetPropertyId() && !(idescriptor.Attributes & PropertyDeleted))
            {TRACE_IT(65826);
                return index;
            }
        }

        TTDAssert(false, "We found this and not accessor but NoBigSlot for index?");
        return Js::Constants::NoBigSlot;
    }
#endif

    template class DictionaryTypeHandlerBase<PropertyIndex>;
    template class DictionaryTypeHandlerBase<BigPropertyIndex>;

    template <bool allowLetConstGlobal>
    PropertyAttributes GetLetConstGlobalPropertyAttributes(PropertyAttributes attributes)
    {TRACE_IT(65827);
        return (allowLetConstGlobal && (attributes & PropertyLetConstGlobal) != 0) ? (attributes | PropertyWritable) : attributes;
    }
}
