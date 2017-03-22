//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    template <typename T>
    DictionaryTypeHandlerBase<T>* DictionaryTypeHandlerBase<T>::New(Recycler * recycler, int initialCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots)
    {LOGMEIN("DictionaryTypeHandler.cpp] 10\n");
        return NewTypeHandler<DictionaryTypeHandlerBase>(recycler, initialCapacity, inlineSlotCapacity, offsetOfInlineSlots);
    }

    template <typename T>
    DictionaryTypeHandlerBase<T>* DictionaryTypeHandlerBase<T>::CreateTypeHandlerForArgumentsInStrictMode(Recycler * recycler, ScriptContext * scriptContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 16\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 32\n");
        SetIsInlineSlotCapacityLocked();
        propertyMap = RecyclerNew(recycler, PropertyDescriptorMap, recycler, this->GetSlotCapacity());
    }

    template <typename T>
    DictionaryTypeHandlerBase<T>::DictionaryTypeHandlerBase(Recycler* recycler, int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots) :
        // Do not RoundUp passed in slotCapacity. This may be called by ConvertTypeHandler for an existing DynamicObject and should use the real existing slotCapacity.
        DynamicTypeHandler(slotCapacity, inlineSlotCapacity, offsetOfInlineSlots),
        nextPropertyIndex(0),
        singletonInstance(nullptr)
    {LOGMEIN("DictionaryTypeHandler.cpp] 43\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 57\n");
        Assert(typeHandler->GetIsInlineSlotCapacityLocked());
        CopyPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection | PropertyTypesInlineSlotCapacityLocked, typeHandler->GetPropertyTypes());
    }

    template <typename T>
    int DictionaryTypeHandlerBase<T>::GetPropertyCount()
    {LOGMEIN("DictionaryTypeHandler.cpp] 64\n");
        return propertyMap->Count();
    }

    template <typename T>
    PropertyId DictionaryTypeHandlerBase<T>::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {LOGMEIN("DictionaryTypeHandler.cpp] 70\n");
        if (index < propertyMap->Count())
        {LOGMEIN("DictionaryTypeHandler.cpp] 72\n");
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            if (!(descriptor.Attributes & PropertyDeleted) && descriptor.HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 75\n");
                return propertyMap->GetKeyAt(index)->GetPropertyId();
            }
        }
        return Constants::NoProperty;
    }

    template <typename T>
    PropertyId DictionaryTypeHandlerBase<T>::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {LOGMEIN("DictionaryTypeHandler.cpp] 84\n");
        if (index < propertyMap->Count())
        {LOGMEIN("DictionaryTypeHandler.cpp] 86\n");
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            if (!(descriptor.Attributes & PropertyDeleted) && descriptor.HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 89\n");
                return propertyMap->GetKeyAt(index)->GetPropertyId();
            }
        }
        return Constants::NoProperty;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 99\n");
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for(; index < propertyMap->Count(); ++index )
        {LOGMEIN("DictionaryTypeHandler.cpp] 105\n");
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);
            PropertyAttributes attribs = descriptor.Attributes;

            if (!(attribs & PropertyDeleted) && (!!(flags & EnumeratorFlags::EnumNonEnumerable) || (attribs & PropertyEnumerable)) &&
                (!(attribs & PropertyLetConstGlobal) || descriptor.HasNonLetConstGlobal()))
            {LOGMEIN("DictionaryTypeHandler.cpp] 111\n");
                const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);

                // Skip this property if it is a symbol and we are not including symbol properties
                if (!(flags & EnumeratorFlags::EnumSymbols) && propertyRecord->IsSymbol())
                {LOGMEIN("DictionaryTypeHandler.cpp] 116\n");
                    continue;
                }

                // Pass back attributes of this property so caller can use them if it needs
                if (attributes != nullptr)
                {LOGMEIN("DictionaryTypeHandler.cpp] 122\n");
                    *attributes = attribs;
                }

                *propertyId = propertyRecord->GetPropertyId();
                PropertyString* propertyString = scriptContext->GetPropertyString(*propertyId);
                *propertyStringName = propertyString;
                T dataSlot = descriptor.template GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots && (attribs & PropertyWritable))
                {LOGMEIN("DictionaryTypeHandler.cpp] 131\n");
                    uint16 inlineOrAuxSlotIndex;
                    bool isInlineSlot;
                    PropertyIndexToInlineOrAuxSlotIndex(dataSlot, &inlineOrAuxSlotIndex, &isInlineSlot);

                    propertyString->UpdateCache(type, inlineOrAuxSlotIndex, isInlineSlot, descriptor.IsInitialized && !descriptor.IsFixed);
                }
                else
                {
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 156\n");
        Assert(false);
        Throw::InternalError();
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 164\n");
        PropertyIndex local = (PropertyIndex)index;
        Assert(index <= Constants::UShortMaxValue || index == Constants::NoBigSlot);
        BOOL result = this->FindNextProperty(scriptContext, local, propertyString, propertyId, attributes, type, typeToEnumerate, flags);
        index = local;
        return result;
    }

    template <>
    BOOL DictionaryTypeHandlerBase<BigPropertyIndex>::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 175\n");
        Assert(propertyStringName);
        Assert(propertyId);
        Assert(type);

        for(; index < propertyMap->Count(); ++index )
        {LOGMEIN("DictionaryTypeHandler.cpp] 181\n");
            DictionaryPropertyDescriptor<BigPropertyIndex> descriptor = propertyMap->GetValueAt(index);
            PropertyAttributes attribs = descriptor.Attributes;
            if (!(attribs & PropertyDeleted) && (!!(flags & EnumeratorFlags::EnumNonEnumerable) || (attribs & PropertyEnumerable)) &&
                (!(attribs & PropertyLetConstGlobal) || descriptor.HasNonLetConstGlobal()))
            {LOGMEIN("DictionaryTypeHandler.cpp] 186\n");
                const PropertyRecord* propertyRecord = propertyMap->GetKeyAt(index);

                // Skip this property if it is a symbol and we are not including symbol properties
                if (!(flags & EnumeratorFlags::EnumSymbols) && propertyRecord->IsSymbol())
                {LOGMEIN("DictionaryTypeHandler.cpp] 191\n");
                    continue;
                }

                if (attributes != nullptr)
                {LOGMEIN("DictionaryTypeHandler.cpp] 196\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 212\n");
        return GetPropertyIndex_Internal<false>(propertyRecord);
    }

    template <typename T>
    PropertyIndex DictionaryTypeHandlerBase<T>::GetRootPropertyIndex(PropertyRecord const* propertyRecord)
    {LOGMEIN("DictionaryTypeHandler.cpp] 218\n");
        return GetPropertyIndex_Internal<true>(propertyRecord);
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {LOGMEIN("DictionaryTypeHandler.cpp] 224\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {LOGMEIN("DictionaryTypeHandler.cpp] 227\n");
            AssertMsg(descriptor->template GetDataPropertyIndex<false>() != Constants::NoSlot, "We don't support equivalent object type spec on accessors.");
            AssertMsg(descriptor->template GetDataPropertyIndex<false>() <= Constants::PropertyIndexMax, "We don't support equivalent object type spec on big property indexes.");
            T propertyIndex = descriptor->template GetDataPropertyIndex<false>();
            info.slotIndex = propertyIndex <= Constants::PropertyIndexMax ?
                AdjustValidSlotIndexForInlineSlots(static_cast<PropertyIndex>(propertyIndex)) : Constants::NoSlot;
            info.isAuxSlot = propertyIndex >= GetInlineSlotCapacity();
            info.isWritable = !!(descriptor->Attributes & PropertyWritable);
        }
        else
        {
            info.slotIndex = Constants::NoSlot;
            info.isAuxSlot = false;
            info.isWritable = false;
        }
        return info.slotIndex != Constants::NoSlot;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {LOGMEIN("DictionaryTypeHandler.cpp] 247\n");
        uint propertyCount = record.propertyCount;
        EquivalentPropertyEntry* properties = record.properties;
        for (uint pi = 0; pi < propertyCount; pi++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 251\n");
            const EquivalentPropertyEntry* refInfo = &properties[pi];
            if (!this->IsObjTypeSpecEquivalentImpl<false>(type, refInfo))
            {LOGMEIN("DictionaryTypeHandler.cpp] 254\n");
                failedPropertyIndex = pi;
                return false;
            }
        }

        return true;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {LOGMEIN("DictionaryTypeHandler.cpp] 265\n");
        return this->IsObjTypeSpecEquivalentImpl<true>(type, entry);
    }

    template <typename T>
    template <bool doLock>
    bool DictionaryTypeHandlerBase<T>::IsObjTypeSpecEquivalentImpl(const Type* type, const EquivalentPropertyEntry *entry)
    {LOGMEIN("DictionaryTypeHandler.cpp] 272\n");
        ScriptContext* scriptContext = type->GetScriptContext();

        T absSlotIndex = Constants::NoSlot;
        PropertyIndex relSlotIndex = Constants::NoSlot;

        const PropertyRecord* propertyRecord =
            doLock ? scriptContext->GetPropertyNameLocked(entry->propertyId) : scriptContext->GetPropertyName(entry->propertyId);
        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {LOGMEIN("DictionaryTypeHandler.cpp] 282\n");
            // We don't object type specialize accessors at this point, so if we see an accessor on an object we must have a mismatch.
            // When we add support for accessors we will need another bit on EquivalentPropertyEntry indicating whether we expect
            // a data or accessor property.
            if (descriptor->IsAccessor)
            {LOGMEIN("DictionaryTypeHandler.cpp] 287\n");
                return false;
            }

            absSlotIndex = descriptor->template GetDataPropertyIndex<false>();
            if (absSlotIndex <= Constants::PropertyIndexMax)
            {LOGMEIN("DictionaryTypeHandler.cpp] 293\n");
                relSlotIndex = AdjustValidSlotIndexForInlineSlots(static_cast<PropertyIndex>(absSlotIndex));
            }
        }

        if (relSlotIndex != Constants::NoSlot)
        {LOGMEIN("DictionaryTypeHandler.cpp] 299\n");
            if (relSlotIndex != entry->slotIndex || ((absSlotIndex >= GetInlineSlotCapacity()) != entry->isAuxSlot))
            {LOGMEIN("DictionaryTypeHandler.cpp] 301\n");
                return false;
            }

            if (entry->mustBeWritable && (!(descriptor->Attributes & PropertyWritable) || !descriptor->IsInitialized || descriptor->IsFixed))
            {LOGMEIN("DictionaryTypeHandler.cpp] 306\n");
                return false;
            }
        }
        else
        {
            if (entry->slotIndex != Constants::NoSlot || entry->mustBeWritable)
            {LOGMEIN("DictionaryTypeHandler.cpp] 313\n");
                return false;
            }
        }

        return true;
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    PropertyIndex DictionaryTypeHandlerBase<T>::GetPropertyIndex_Internal(PropertyRecord const* propertyRecord)
    {LOGMEIN("DictionaryTypeHandler.cpp] 324\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {LOGMEIN("DictionaryTypeHandler.cpp] 327\n");
            return descriptor->template GetDataPropertyIndex<allowLetConstGlobal>();
        }
        else
        {
            return NoSlots;
        }
    }

    template <>
    template <bool allowLetConstGlobal>
    PropertyIndex DictionaryTypeHandlerBase<BigPropertyIndex>::GetPropertyIndex_Internal(PropertyRecord const* propertyRecord)
    {LOGMEIN("DictionaryTypeHandler.cpp] 339\n");
        DictionaryPropertyDescriptor<BigPropertyIndex>* descriptor;
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && !(descriptor->Attributes & PropertyDeleted))
        {LOGMEIN("DictionaryTypeHandler.cpp] 342\n");
            BigPropertyIndex dataPropertyIndex = descriptor->GetDataPropertyIndex<allowLetConstGlobal>();
            if(dataPropertyIndex < Constants::NoSlot)
            {LOGMEIN("DictionaryTypeHandler.cpp] 345\n");
                return (PropertyIndex)dataPropertyIndex;
            }
        }
        return Constants::NoSlot;
    }

    template <>
    PropertyIndex DictionaryTypeHandlerBase<BigPropertyIndex>::GetRootPropertyIndex(PropertyRecord const* propertyRecord)
    {LOGMEIN("DictionaryTypeHandler.cpp] 354\n");
        return Constants::NoSlot;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::Add(
        const PropertyRecord* propertyId,
        PropertyAttributes attributes,
        ScriptContext *const scriptContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 363\n");
        return Add(propertyId, attributes, true, false, false, scriptContext);
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::Add(
        const PropertyRecord* propertyId,
        PropertyAttributes attributes,
        bool isInitialized, bool isFixed, bool usedAsFixed,
        ScriptContext *const scriptContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 373\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 387\n");
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {LOGMEIN("DictionaryTypeHandler.cpp] 390\n");
                scriptContext->InvalidateStoreFieldCaches(propertyId->GetPropertyId());
                scriptContext->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl)
    {LOGMEIN("DictionaryTypeHandler.cpp] 399\n");
        return HasProperty_Internal<false>(instance, propertyId, noRedecl, nullptr, nullptr);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasRootProperty(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty, bool *pNonconfigurableProperty)
    {LOGMEIN("DictionaryTypeHandler.cpp] 405\n");
        return HasProperty_Internal<true>(instance, propertyId, noRedecl, pDeclaredProperty, pNonconfigurableProperty);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty_Internal(DynamicObject* instance, PropertyId propertyId, bool *noRedecl, bool *pDeclaredProperty, bool *pNonconfigurableProperty)
    {LOGMEIN("DictionaryTypeHandler.cpp] 412\n");
        // HasProperty is called with NoProperty in JavascriptDispatch.cpp to for undeferral of the
        // deferred type system that DOM objects use.  Allow NoProperty for this reason, but only
        // here in HasProperty.
        if (propertyId == Constants::NoProperty)
        {LOGMEIN("DictionaryTypeHandler.cpp] 417\n");
            return false;
        }

        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 425\n");
            if ((descriptor->Attributes & PropertyDeleted) || (!allowLetConstGlobal && !descriptor->HasNonLetConstGlobal()))
            {LOGMEIN("DictionaryTypeHandler.cpp] 427\n");
                return false;
            }
            if (noRedecl && descriptor->Attributes & PropertyNoRedecl)
            {LOGMEIN("DictionaryTypeHandler.cpp] 431\n");
                *noRedecl = true;
            }
            if (pDeclaredProperty && descriptor->Attributes & (PropertyNoRedecl | PropertyDeclaredGlobal))
            {LOGMEIN("DictionaryTypeHandler.cpp] 435\n");
                *pDeclaredProperty = true;
            }
            if (pNonconfigurableProperty && !(descriptor->Attributes & PropertyConfigurable))
            {LOGMEIN("DictionaryTypeHandler.cpp] 439\n");
                *pNonconfigurableProperty = true;
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 447\n");
            return DictionaryTypeHandlerBase<T>::HasItem(instance, propertyRecord->GetNumericValue());
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {LOGMEIN("DictionaryTypeHandler.cpp] 456\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 463\n");
            if ((descriptor->Attributes & PropertyDeleted) || !descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 465\n");
                return false;
            }
            return true;
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetRootProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 477\n");
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return GetProperty_Internal<true>(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 485\n");
        return GetProperty_Internal<false>(instance, originalInstance, propertyId, value, info, requestContext);
    }

    template <typename T>
    template <bool allowLetConstGlobal, typename PropertyType>
    BOOL DictionaryTypeHandlerBase<T>::GetPropertyFromDescriptor(DynamicObject* instance, Var originalInstance,
        DictionaryPropertyDescriptor<T>* descriptor, Var* value, PropertyValueInfo* info, PropertyType propertyT, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 493\n");
        bool const isLetConstGlobal = (descriptor->Attributes & PropertyLetConstGlobal) != 0;
        AssertMsg(!isLetConstGlobal || RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
        if (allowLetConstGlobal)
        {LOGMEIN("DictionaryTypeHandler.cpp] 497\n");
            // GetRootProperty: false if not global
            if (!(descriptor->Attributes & PropertyLetConstGlobal) && (descriptor->Attributes & PropertyDeleted))
            {LOGMEIN("DictionaryTypeHandler.cpp] 500\n");
                return false;
            }
        }
        else
        {
            // GetProperty: don't count deleted or global.
            if (descriptor->Attributes & (PropertyDeleted | (descriptor->IsShadowed ? 0 : PropertyLetConstGlobal)))
            {LOGMEIN("DictionaryTypeHandler.cpp] 508\n");
                return false;
            }
        }

        T dataSlot = descriptor->template GetDataPropertyIndex<allowLetConstGlobal>();
        if (dataSlot != NoSlots)
        {LOGMEIN("DictionaryTypeHandler.cpp] 515\n");
            *value = instance->GetSlot(dataSlot);
            SetPropertyValueInfo(info, instance, dataSlot, descriptor->Attributes);
            if (!descriptor->IsInitialized || descriptor->IsFixed)
            {LOGMEIN("DictionaryTypeHandler.cpp] 519\n");
                PropertyValueInfo::DisableStoreFieldCache(info);
            }
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 523\n");
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
        {
            *value = instance->GetLibrary()->GetUndefined();
            return true;
        }
        return true;
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty_Internal(DynamicObject* instance, Var originalInstance, PropertyId propertyId,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 551\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 556\n");
            return GetPropertyFromDescriptor<allowLetConstGlobal>(instance, originalInstance, descriptor, value, info, propertyId, requestContext);
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 562\n");
            return DictionaryTypeHandlerBase<T>::GetItem(instance, originalInstance, propertyRecord->GetNumericValue(), value, requestContext);
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString,
        Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 573\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;
        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 580\n");
            return GetPropertyFromDescriptor<false>(instance, originalInstance, descriptor, value, info, propertyName, requestContext);
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, T propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 590\n");
        PropertyValueInfo::Set(info, instance, propIndex, attributes, flags);
    }


    template <>
    void DictionaryTypeHandlerBase<BigPropertyIndex>::SetPropertyValueInfo(PropertyValueInfo* info, RecyclableObject* instance, BigPropertyIndex propIndex, PropertyAttributes attributes, InlineCacheFlags flags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 597\n");
        PropertyValueInfo::SetNoCache(info, instance);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 603\n");
        return GetSetter_Internal<false>(instance, propertyId, setterValue, info, requestContext);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetRootSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 609\n");
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return GetSetter_Internal<true>(instance, propertyId, setterValue, info, requestContext);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter_Internal(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 617\n");
        DictionaryPropertyDescriptor<T>* descriptor;

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 623\n");
            return GetSetterFromDescriptor<allowLetConstGlobal>(instance, descriptor, setterValue, info);
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 629\n");
            return DictionaryTypeHandlerBase<T>::GetItemSetter(instance, propertyRecord->GetNumericValue(), setterValue, requestContext);
        }

        return None;
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetterFromDescriptor(DynamicObject* instance, DictionaryPropertyDescriptor<T> * descriptor, Var* setterValue, PropertyValueInfo* info)
    {LOGMEIN("DictionaryTypeHandler.cpp] 639\n");
        if (descriptor->Attributes & PropertyDeleted)
        {LOGMEIN("DictionaryTypeHandler.cpp] 641\n");
            return None;
        }
        if (descriptor->template GetDataPropertyIndex<allowLetConstGlobal>() != NoSlots)
        {LOGMEIN("DictionaryTypeHandler.cpp] 645\n");
            // not a setter but shadows
            if (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal))
            {LOGMEIN("DictionaryTypeHandler.cpp] 648\n");
                return (descriptor->Attributes & PropertyConst) ? (DescriptorFlags)(Const | Data) : WritableData;
            }
            if (descriptor->Attributes & PropertyWritable)
            {LOGMEIN("DictionaryTypeHandler.cpp] 652\n");
                return WritableData;
            }
            if (descriptor->Attributes & PropertyConst)
            {LOGMEIN("DictionaryTypeHandler.cpp] 656\n");
                return (DescriptorFlags)(Const|Data);
            }
            return Data;
        }
        else if (descriptor->GetSetterPropertyIndex() != NoSlots)
        {LOGMEIN("DictionaryTypeHandler.cpp] 662\n");
            *setterValue=((DynamicObject*)instance)->GetSlot(descriptor->GetSetterPropertyIndex());
            SetPropertyValueInfo(info, instance, descriptor->GetSetterPropertyIndex(), descriptor->Attributes, InlineCacheSetterFlag);
            return Accessor;
        }
        return None;
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 672\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        DictionaryPropertyDescriptor<T>* descriptor;

        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 680\n");
            return GetSetterFromDescriptor<false>(instance, descriptor, setterValue, info);
        }

        return None;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetRootProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("DictionaryTypeHandler.cpp] 689\n");
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return SetProperty_Internal<true>(instance, propertyId, value, flags, info);
    }
    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::InitProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("DictionaryTypeHandler.cpp] 695\n");
        return SetProperty_Internal<false>(instance, propertyId, value, flags, info, true /* IsInit */);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("DictionaryTypeHandler.cpp] 701\n");
        return SetProperty_Internal<false>(instance, propertyId, value, flags, info);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    void DictionaryTypeHandlerBase<T>::SetPropertyWithDescriptor(DynamicObject* instance, PropertyId propertyId, DictionaryPropertyDescriptor<T> * descriptor,
        Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("DictionaryTypeHandler.cpp] 709\n");
        Assert(instance);
        Assert((descriptor->Attributes & PropertyDeleted) == 0 || (allowLetConstGlobal && descriptor->IsShadowed));

        // DictionaryTypeHandlers are not supposed to be shared.
        Assert(!GetIsOrMayBecomeShared());
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        Assert(this->singletonInstance == nullptr || localSingletonInstance == instance);

        T dataSlotAllowLetConstGlobal = descriptor->template GetDataPropertyIndex<allowLetConstGlobal>();
        if (dataSlotAllowLetConstGlobal != NoSlots)
        {LOGMEIN("DictionaryTypeHandler.cpp] 720\n");
            if (allowLetConstGlobal
                && (descriptor->Attributes & PropertyNoRedecl)
                && !(flags & PropertyOperation_AllowUndecl))
            {LOGMEIN("DictionaryTypeHandler.cpp] 724\n");
                ScriptContext* scriptContext = instance->GetScriptContext();
                if (scriptContext->IsUndeclBlockVar(instance->GetSlot(dataSlotAllowLetConstGlobal)))
                {LOGMEIN("DictionaryTypeHandler.cpp] 727\n");
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
                }
            }

            if (!descriptor->IsInitialized)
            {LOGMEIN("DictionaryTypeHandler.cpp] 733\n");
                if ((flags & PropertyOperation_PreInit) == 0)
                {LOGMEIN("DictionaryTypeHandler.cpp] 735\n");
                    descriptor->IsInitialized = true;
                    if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId) &&
                        (flags & (PropertyOperation_NonFixedValue | PropertyOperation_SpecialValue)) == 0)
                    {LOGMEIN("DictionaryTypeHandler.cpp] 739\n");
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
            {
                PropertyValueInfo::SetNoCache(info, instance);
            }
        }
        else if (descriptor->GetSetterPropertyIndex() != NoSlots)
        {LOGMEIN("DictionaryTypeHandler.cpp] 766\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 788\n");
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T>* descriptor;
        bool throwIfNotExtensible = (flags & (PropertyOperation_ThrowIfNotExtensible | PropertyOperation_StrictMode)) != 0;
        bool isForce = (flags & PropertyOperation_Force) != 0;

        JavascriptLibrary::CheckAndInvalidateIsConcatSpreadableCache(propertyId, scriptContext);

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 799\n");
            Assert(descriptor->SanityCheckFixedBits());
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 802\n");
                if (!isForce)
                {LOGMEIN("DictionaryTypeHandler.cpp] 804\n");
                    if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                    {LOGMEIN("DictionaryTypeHandler.cpp] 806\n");
                        return false;
                    }
                }
                scriptContext->InvalidateProtoCaches(propertyId);
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {LOGMEIN("DictionaryTypeHandler.cpp] 812\n");
                    descriptor->Attributes = PropertyDynamicTypeDefaults | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {
                    descriptor->Attributes = PropertyDynamicTypeDefaults;
                }
                instance->SetHasNoEnumerableProperties(false);
                descriptor->ConvertToData();
            }
            else if (!allowLetConstGlobal && descriptor->HasNonLetConstGlobal() && !(descriptor->Attributes & PropertyWritable))
            {LOGMEIN("DictionaryTypeHandler.cpp] 823\n");
                if (!isForce)
                {LOGMEIN("DictionaryTypeHandler.cpp] 825\n");
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
            {LOGMEIN("DictionaryTypeHandler.cpp] 837\n");
                descriptor->ConvertToData();
            }
            SetPropertyWithDescriptor<allowLetConstGlobal>(instance, propertyId, descriptor, value, flags, info);
            return true;
        }

        // Always check numeric propertyRecord. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 846\n");
            // Calls this or subclass implementation
            return SetItem(instance, propertyRecord->GetNumericValue(), value, flags);
        }
        return this->AddProperty(instance, propertyRecord, value, PropertyDynamicTypeDefaults, info, flags, throwIfNotExtensible, SideEffects_Any);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("DictionaryTypeHandler.cpp] 855\n");
        // Either the property exists in the dictionary, in which case a PropertyRecord for it exists,
        // or we have to add it to the dictionary, in which case we need to get or create a PropertyRecord.
        // Thus, just get or create one and call the PropertyId overload of SetProperty.
        PropertyRecord const * propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return DictionaryTypeHandlerBase<T>::SetProperty(instance, propertyRecord->GetPropertyId(), value, flags, info);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 866\n");
        return DeleteProperty_Internal<false>(instance, propertyId, propertyOperationFlags);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::DeleteProperty(DynamicObject *instance, JavascriptString *propertyNameString, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 872\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* ");

        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T>* descriptor;
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());

        if (propertyMap->TryGetReference(propertyName, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 881\n");
            Assert(descriptor->SanityCheckFixedBits());

            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 885\n");
                return true;
            }
            else if (!(descriptor->Attributes & PropertyConfigurable))
            {LOGMEIN("DictionaryTypeHandler.cpp] 889\n");
                // Let/const properties do not have attributes and they cannot be deleted
                JavascriptError::ThrowCantDelete(propertyOperationFlags, scriptContext, propertyNameString->GetString());

                return false;
            }

            Var undefined = scriptContext->GetLibrary()->GetUndefined();

            if (descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 899\n");
                T dataSlot = descriptor->template GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots)
                {
                    SetSlotUnchecked(instance, dataSlot, undefined);
                }
                else
                {
                    Assert(descriptor->IsAccessor);
                    SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), undefined);
                    SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), undefined);
                }

                if (this->GetFlags() & IsPrototypeFlag)
                {LOGMEIN("DictionaryTypeHandler.cpp] 913\n");
                    scriptContext->InvalidateProtoCaches(scriptContext->GetOrAddPropertyIdTracked(propertyNameString->GetString(), propertyNameString->GetLength()));
                }

                if ((descriptor->Attributes & PropertyLetConstGlobal) == 0)
                {LOGMEIN("DictionaryTypeHandler.cpp] 918\n");
                    Assert(!descriptor->IsShadowed);
                    descriptor->Attributes = PropertyDeletedDefaults;
                }
                else
                {
                    descriptor->Attributes &= ~PropertyDynamicTypeDefaults;
                    descriptor->Attributes |= PropertyDeletedDefaults;
                }
                InvalidateFixedField(instance, propertyNameString, descriptor);

                // Change the type so as we can invalidate the cache in fast path jit
                if (instance->GetType()->HasBeenCached())
                {LOGMEIN("DictionaryTypeHandler.cpp] 931\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 947\n");
        AssertMsg(RootObjectBase::Is(instance), "Instance must be a root object!");
        return DeleteProperty_Internal<true>(instance, propertyId, propertyOperationFlags);
    }

    template <typename T>
    template <bool allowLetConstGlobal>
    BOOL DictionaryTypeHandlerBase<T>::DeleteProperty_Internal(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 955\n");
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 961\n");
            Assert(descriptor->SanityCheckFixedBits());

            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 965\n");
                // If PropertyDeleted and PropertyLetConstGlobal are set then we have both
                // a deleted global property and let/const variable in this descriptor.
                // If allowLetConstGlobal is true then the let/const shadows the property
                // and we should return false for a failed delete by going into the else
                // if branch below.
                if (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal))
                {LOGMEIN("DictionaryTypeHandler.cpp] 972\n");
                    JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, scriptContext, propertyRecord->GetBuffer());

                    return false;
                }
                return true;
            }
            else if (!(descriptor->Attributes & PropertyConfigurable) ||
                (allowLetConstGlobal && (descriptor->Attributes & PropertyLetConstGlobal)))
            {LOGMEIN("DictionaryTypeHandler.cpp] 981\n");
                // Let/const properties do not have attributes and they cannot be deleted
                JavascriptError::ThrowCantDelete(propertyOperationFlags, scriptContext, scriptContext->GetPropertyName(propertyId)->GetBuffer());

                return false;
            }

            Var undefined = scriptContext->GetLibrary()->GetUndefined();

            if (descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 991\n");
                T dataSlot = descriptor->template GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots)
                {
                    SetSlotUnchecked(instance, dataSlot, undefined);
                }
                else
                {
                    Assert(descriptor->IsAccessor);
                    SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), undefined);
                    SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), undefined);
                }

                if (this->GetFlags() & IsPrototypeFlag)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1005\n");
                    scriptContext->InvalidateProtoCaches(propertyId);
                }

                if ((descriptor->Attributes & PropertyLetConstGlobal) == 0)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1010\n");
                    Assert(!descriptor->IsShadowed);
                    descriptor->Attributes = PropertyDeletedDefaults;
                }
                else
                {
                    descriptor->Attributes &= ~PropertyDynamicTypeDefaults;
                    descriptor->Attributes |= PropertyDeletedDefaults;
                }
                InvalidateFixedField(instance, propertyId, descriptor);

                // Change the type so as we can invalidate the cache in fast path jit
                if (instance->GetType()->HasBeenCached())
                {LOGMEIN("DictionaryTypeHandler.cpp] 1023\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 1036\n");
            return DictionaryTypeHandlerBase<T>::DeleteItem(instance, propertyRecord->GetNumericValue(), propertyOperationFlags);
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsFixedProperty(const DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1045\n");
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T> descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetValue(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1051\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 1063\n");
        if (!(this->GetFlags() & IsExtensibleFlag) && !instance->HasObjectArray())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1065\n");
            ScriptContext* scriptContext = instance->GetScriptContext();
            JavascriptError::ThrowCantExtendIfStrictMode(flags, scriptContext);
            return false;
        }
        return __super::SetItem(instance, index, value, flags);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1075\n");
        return instance->SetObjectArrayItemWithAttributes(index, value, attributes);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1081\n");
        return instance->SetObjectArrayItemAttributes(index, attributes);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1087\n");
        return instance->SetObjectArrayItemAccessors(index, getter, setter);
    }

    template <typename T>
    DescriptorFlags DictionaryTypeHandlerBase<T>::GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1093\n");
        if (instance->HasObjectArray())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1095\n");
            return instance->GetObjectArrayItemSetter(index, setterValue, requestContext);
        }
        return __super::GetItemSetter(instance, index, setterValue, requestContext);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1103\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1108\n");
            if (!descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 1110\n");
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");

                return true;
            }
            return descriptor->Attributes & PropertyEnumerable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1120\n");
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1123\n");
                return objectArray->IsEnumerable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1133\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1138\n");
            if (!descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 1140\n");
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return !(descriptor->Attributes & PropertyConst);
            }
            return descriptor->Attributes & PropertyWritable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1149\n");
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1152\n");
                return objectArray->IsWritable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1162\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1167\n");
            if (!descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 1169\n");
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return true;
            }
            return descriptor->Attributes & PropertyConfigurable;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1178\n");
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1181\n");
                return objectArray->IsConfigurable(propertyId);
            }
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1191\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1196\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1198\n");
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 1203\n");
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1209\n");
                descriptor->Attributes |= PropertyEnumerable;
                instance->SetHasNoEnumerableProperties(false);
            }
            else
            {
                descriptor->Attributes &= (~PropertyEnumerable);
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1222\n");
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1225\n");
                return objectArray->SetEnumerable(propertyId, value);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1235\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        ScriptContext* scriptContext = instance->GetScriptContext();
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1241\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1243\n");
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 1248\n");
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1254\n");
                descriptor->Attributes |= PropertyWritable;
            }
            else
            {
                descriptor->Attributes &= (~PropertyWritable);
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1262\n");
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }
            instance->ChangeType();
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1273\n");
            return instance->SetObjectArrayItemWritable(propertyId, value);
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1282\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1287\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1289\n");
                return false;
            }

            if (!descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 1294\n");
                AssertMsg(RootObjectBase::Is(instance), "object must be a global object if letconstglobal is set");
                return false;
            }

            if (value)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1300\n");
                descriptor->Attributes |= PropertyConfigurable;
            }
            else
            {
                descriptor->Attributes &= (~PropertyConfigurable);
            }
            return true;
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1312\n");
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1315\n");
                return objectArray->SetConfigurable(propertyId, value);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::PreventExtensions(DynamicObject* instance)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1325\n");
        this->ClearFlags(IsExtensibleFlag);

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1330\n");
            objectArray->PreventExtensions();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::Seal(DynamicObject* instance)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1339\n");
        this->ClearFlags(IsExtensibleFlag);

        // Set [[Configurable]] flag of each property to false
        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1345\n");
            descriptor = propertyMap->GetReferenceAt(index);
            if (descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 1348\n");
                descriptor->Attributes &= (~PropertyConfigurable);
            }
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1355\n");
            objectArray->Seal();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1364\n");
        this->ClearFlags(IsExtensibleFlag);

        // Set [[Writable]] flag of each property to false except for setter\getters
        // Set [[Configurable]] flag of each property to false
        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1371\n");
            descriptor = propertyMap->GetReferenceAt(index);
            if (descriptor->HasNonLetConstGlobal())
            {LOGMEIN("DictionaryTypeHandler.cpp] 1374\n");
                if (descriptor->template GetDataPropertyIndex<false>() != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1376\n");
                    // Only data descriptor has Writable property
                    descriptor->Attributes &= ~(PropertyWritable | PropertyConfigurable);
                }
                else
                {
                    descriptor->Attributes &= ~(PropertyConfigurable);
                }
            }
#if DBG
            else
            {
                AssertMsg(RootObjectBase::Is(instance), "instance needs to be global object when letconst global is set");
            }
#endif
        }
        if (!isConvertedType)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1393\n");
            // Change of [[Writable]] property requires cache invalidation, hence ChangeType
            instance->ChangeType();
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1400\n");
            objectArray->Freeze();
        }

        this->ClearHasOnlyWritableDataProperties();
        if(GetFlags() & IsPrototypeFlag)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1406\n");
            InvalidateStoreFieldCachesForAllProperties(instance->GetScriptContext());
            instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsSealed(DynamicObject* instance)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1416\n");
        if (this->GetFlags() & IsExtensibleFlag)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1418\n");
            return false;
        }

        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1424\n");
            descriptor = propertyMap->GetReferenceAt(index);
            if ((!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal)))
            {LOGMEIN("DictionaryTypeHandler.cpp] 1427\n");
                if (descriptor->Attributes & PropertyConfigurable)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1429\n");
                    // [[Configurable]] must be false for all (existing) properties.
                    // IE9 compatibility: keep IE9 behavior (also check deleted properties)
                    return false;
                }
            }
        }

        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray && !objectArray->IsSealed())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1439\n");
            return false;
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::IsFrozen(DynamicObject* instance)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1448\n");
        if (this->GetFlags() & IsExtensibleFlag)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1450\n");
            return false;
        }

        DictionaryPropertyDescriptor<T> *descriptor = nullptr;
        for (T index = 0; index < propertyMap->Count(); index++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1456\n");
            descriptor = propertyMap->GetReferenceAt(index);
            if ((!(descriptor->Attributes & PropertyDeleted) && !(descriptor->Attributes & PropertyLetConstGlobal)))
            {LOGMEIN("DictionaryTypeHandler.cpp] 1459\n");
                if (descriptor->Attributes & PropertyConfigurable)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1461\n");
                    return false;
                }

                if (descriptor->template GetDataPropertyIndex<false>() != NoSlots && (descriptor->Attributes & PropertyWritable))
                {LOGMEIN("DictionaryTypeHandler.cpp] 1466\n");
                    // Only data descriptor has [[Writable]] property
                    return false;
                }
            }
        }

        // Use IsObjectArrayFrozen() to skip "length" [[Writable]] check
        ArrayObject * objectArray = instance->GetObjectArray();
        if (objectArray && !objectArray->IsObjectArrayFrozen())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1476\n");
            return false;
        }

        return true;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetAccessors(DynamicObject* instance, PropertyId propertyId, Var* getter, Var* setter)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1485\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        ScriptContext* scriptContext = instance->GetScriptContext();
        AssertMsg(nullptr != getter && nullptr != setter, "Getter/Setter must be a valid pointer" );

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1493\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1495\n");
                return false;
            }

            if (descriptor->template GetDataPropertyIndex<false>() == NoSlots)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1500\n");
                bool getset = false;
                if (descriptor->GetGetterPropertyIndex() != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1503\n");
                    *getter = instance->GetSlot(descriptor->GetGetterPropertyIndex());
                    getset = true;
                }
                if (descriptor->GetSetterPropertyIndex() != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1508\n");
                    *setter = instance->GetSlot(descriptor->GetSetterPropertyIndex());
                    getset = true;
                }
                return getset;
            }
        }

        // Check numeric propertyRecord only if objectArray available
        if (propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1518\n");
            ArrayObject * objectArray = instance->GetObjectArray();
            if (objectArray != nullptr)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1521\n");
                return objectArray->GetAccessors(propertyId, getter, setter, scriptContext);
            }
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1531\n");
        Assert(instance);
        JavascriptLibrary* library = instance->GetLibrary();
        ScriptContext* scriptContext = instance->GetScriptContext();

        Assert(this->VerifyIsExtensible(scriptContext, false) || this->HasProperty(instance, propertyId)
            || JavascriptFunction::IsBuiltinProperty(instance, propertyId));

        DictionaryPropertyDescriptor<T>* descriptor;
        if (this->GetFlags() & IsPrototypeFlag)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1541\n");
            scriptContext->InvalidateProtoCaches(propertyId);
        }

        bool isGetterSet = true;
        bool isSetterSet = true;
        if (!getter || getter == library->GetDefaultAccessorFunction())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1548\n");
            isGetterSet = false;
        }
        if (!setter || setter == library->GetDefaultAccessorFunction())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1552\n");
            isSetterSet = false;
        }

        Assert(propertyId != Constants::NoProperty);
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1559\n");
            Assert(descriptor->SanityCheckFixedBits());

            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1563\n");
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1565\n");
                    descriptor->Attributes = PropertyDynamicTypeDefaults | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {
                    descriptor->Attributes = PropertyDynamicTypeDefaults;
                }
            }

            if (!descriptor->IsAccessor)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1575\n");
                // New getter/setter, make sure both values are not null and set to the slots
                getter = CanonicalizeAccessor(getter, library);
                setter = CanonicalizeAccessor(setter, library);
            }

            // conversion from data-property to accessor property
            if (descriptor->ConvertToGetterSetter(nextPropertyIndex))
            {LOGMEIN("DictionaryTypeHandler.cpp] 1583\n");
                if (this->GetSlotCapacity() <= nextPropertyIndex)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1585\n");
                    if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
                    {LOGMEIN("DictionaryTypeHandler.cpp] 1587\n");
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
            {LOGMEIN("DictionaryTypeHandler.cpp] 1603\n");
                descriptor->IsInitialized = true;
                if (localSingletonInstance == instance && !IsInternalPropertyId(propertyId))
                {LOGMEIN("DictionaryTypeHandler.cpp] 1606\n");
                    // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                    Assert(!instance->IsExternal() || (flags & PropertyOperation_NonFixedValue) != 0);
                    descriptor->IsFixed = (flags & PropertyOperation_NonFixedValue) == 0 && ShouldFixAccessorProperties();
                }
                if (!isGetterSet || !isSetterSet)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1612\n");
                    descriptor->IsOnlyOneAccessorInitialized = true;
                }
            }
            else if (descriptor->IsOnlyOneAccessorInitialized)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1617\n");
                // Only one of getter/setter was initialized, allow the isFixed to stay if we are defining the other one.
                Var oldGetter = GetSlot(instance, descriptor->GetGetterPropertyIndex());
                Var oldSetter = GetSlot(instance, descriptor->GetSetterPropertyIndex());

                if (((getter == oldGetter || !isGetterSet) && oldSetter == library->GetDefaultAccessorFunction()) ||
                    ((setter == oldSetter || !isSetterSet) && oldGetter == library->GetDefaultAccessorFunction()))
                {LOGMEIN("DictionaryTypeHandler.cpp] 1624\n");
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
            {LOGMEIN("DictionaryTypeHandler.cpp] 1639\n");
                getter = CanonicalizeAccessor(getter, library);
                SetSlotUnchecked(instance, descriptor->GetGetterPropertyIndex(), getter);
            }
            if (setter != nullptr)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1644\n");
                setter = CanonicalizeAccessor(setter, library);
                SetSlotUnchecked(instance, descriptor->GetSetterPropertyIndex(), setter);
            }
            instance->ChangeType();
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1651\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 1665\n");
            // Calls this or subclass implementation
            return SetItemAccessors(instance, propertyRecord->GetNumericValue(), getter, setter);
        }

        getter = CanonicalizeAccessor(getter, library);
        setter = CanonicalizeAccessor(setter, library);
        T getterIndex = nextPropertyIndex++;
        T setterIndex = nextPropertyIndex++;
        DictionaryPropertyDescriptor<T> newDescriptor(getterIndex, setterIndex);
        if (this->GetSlotCapacity() <= nextPropertyIndex)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1676\n");
            if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1678\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 1691\n");
            // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
            Assert(!instance->IsExternal() || (flags & PropertyOperation_NonFixedValue) != 0);

            // Even if one (or both?) accessors are the default functions obtained through canonicalization,
            // they are still legitimate functions, so it's ok to mark the whole property as fixed.
            newDescriptor.IsFixed = (flags & PropertyOperation_NonFixedValue) == 0 && ShouldFixAccessorProperties();
            if (!isGetterSet || !isSetterSet)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1699\n");
                newDescriptor.IsOnlyOneAccessorInitialized = true;
            }
        }

        propertyMap->Add(propertyRecord, newDescriptor);

        SetSlotUnchecked(instance, newDescriptor.GetGetterPropertyIndex(), getter);
        SetSlotUnchecked(instance, newDescriptor.GetSetterPropertyIndex(), setter);
        this->ClearHasOnlyWritableDataProperties();
        if(GetFlags() & IsPrototypeFlag)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1710\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 1737\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 1750\n");
            Assert(descriptor->SanityCheckFixedBits());

            if (attributes & descriptor->Attributes & PropertyLetConstGlobal)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1754\n");
                // Do not need to change the descriptor or its attributes if setting the initial value of a LetConstGlobal
            }
            else if (descriptor->Attributes & PropertyDeleted && !(attributes & PropertyLetConstGlobal))
            {LOGMEIN("DictionaryTypeHandler.cpp] 1758\n");
                if (!isForce)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1760\n");
                    if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
                    {LOGMEIN("DictionaryTypeHandler.cpp] 1762\n");
                        return FALSE;
                    }
                }

                scriptContext->InvalidateProtoCaches(propertyId);
                if (descriptor->Attributes & PropertyLetConstGlobal)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1769\n");
                    descriptor->Attributes = attributes | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
                }
                else
                {
                    descriptor->Attributes = attributes;
                }
                descriptor->ConvertToData();
            }
            else if (descriptor->IsShadowed)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1779\n");
                descriptor->Attributes = attributes | (descriptor->Attributes & (PropertyLetConstGlobal | PropertyNoRedecl));
            }
            else if ((descriptor->Attributes & PropertyLetConstGlobal) != (attributes & PropertyLetConstGlobal))
            {LOGMEIN("DictionaryTypeHandler.cpp] 1783\n");
                bool addingLetConstGlobal = (attributes & PropertyLetConstGlobal) != 0;

                if (addingLetConstGlobal)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1787\n");
                    descriptor->Attributes = descriptor->Attributes | (attributes & PropertyNoRedecl);
                }
                else
                {
                    descriptor->Attributes = attributes | (descriptor->Attributes & PropertyNoRedecl);
                }

                descriptor->AddShadowedData(nextPropertyIndex, addingLetConstGlobal);

                if (this->GetSlotCapacity() <= nextPropertyIndex)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1798\n");
                    if (this->GetSlotCapacity() >= MaxPropertyIndexSize)
                    {LOGMEIN("DictionaryTypeHandler.cpp] 1800\n");
                        Throw::OutOfMemory();
                    }

                    this->EnsureSlotCapacity(instance);
                }

                if (addingLetConstGlobal)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1808\n");
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
            {
                if (descriptor->IsAccessor && !(attributes & PropertyLetConstGlobal))
                {LOGMEIN("DictionaryTypeHandler.cpp] 1821\n");
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
                    {LOGMEIN("DictionaryTypeHandler.cpp] 1839\n");
                        if (scriptContext && scriptContext->GetThreadContext()->RecordImplicitException())
                        {LOGMEIN("DictionaryTypeHandler.cpp] 1841\n");
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
            {LOGMEIN("DictionaryTypeHandler.cpp] 1864\n");
                instance->SetHasNoEnumerableProperties(false);
            }

            if (!(descriptor->Attributes & PropertyWritable))
            {LOGMEIN("DictionaryTypeHandler.cpp] 1869\n");
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1872\n");
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }

            SetPropertyUpdateSideEffect(instance, propertyId, value, possibleSideEffects);
            return true;
        }

        // Always check numeric propertyRecord. This may create objectArray.
        if (propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1884\n");
            // Calls this or subclass implementation
            return SetItemWithAttributes(instance, propertyRecord->GetNumericValue(), value, attributes);
        }

        return this->AddProperty(instance, propertyRecord, value, attributes, info, flags, throwIfNotExtensible, possibleSideEffects);
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::EnsureSlotCapacity(DynamicObject * instance)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1894\n");
        Assert(this->GetSlotCapacity() < MaxPropertyIndexSize); // Otherwise we can't grow this handler's capacity. We should've evolved to Bigger handler or OOM.

        // A Dictionary type is expected to have more properties
        // grow exponentially rather linearly to avoid the realloc and moves,
        // however use a small exponent to avoid waste
        int newSlotCapacity = (nextPropertyIndex + 1);
        newSlotCapacity += (newSlotCapacity>>2);
        if (newSlotCapacity > MaxPropertyIndexSize)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1903\n");
            newSlotCapacity = MaxPropertyIndexSize;
        }
        newSlotCapacity = RoundUpSlotCapacity(newSlotCapacity, GetInlineSlotCapacity());
        Assert(newSlotCapacity <= MaxPropertyIndexSize);

        instance->EnsureSlots(this->GetSlotCapacity(), newSlotCapacity, instance->GetScriptContext(), this);
        this->SetSlotCapacity(newSlotCapacity);
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1915\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(propertyId != Constants::NoProperty);
        ScriptContext* scriptContext = instance->GetScriptContext();
        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1921\n");
            if (descriptor->Attributes & PropertyDeleted)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1923\n");
                return false;
            }

            descriptor->Attributes = (descriptor->Attributes & ~PropertyDynamicTypeDefaults) | (attributes & PropertyDynamicTypeDefaults);

            if (descriptor->Attributes & PropertyEnumerable)
            {LOGMEIN("DictionaryTypeHandler.cpp] 1930\n");
                instance->SetHasNoEnumerableProperties(false);
            }

            if (!(descriptor->Attributes & PropertyWritable))
            {LOGMEIN("DictionaryTypeHandler.cpp] 1935\n");
                this->ClearHasOnlyWritableDataProperties();
                if(GetFlags() & IsPrototypeFlag)
                {LOGMEIN("DictionaryTypeHandler.cpp] 1938\n");
                    scriptContext->InvalidateStoreFieldCaches(propertyId);
                    instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
                }
            }

            return true;
        }

        // Check numeric propertyId only if objectArray available
        if (instance->HasObjectArray() && propertyRecord->IsNumeric())
        {LOGMEIN("DictionaryTypeHandler.cpp] 1949\n");
            return DictionaryTypeHandlerBase<T>::SetItemAttributes(instance, propertyRecord->GetNumericValue(), attributes);
        }

        return false;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1958\n");
        // this might get value that are deleted from the dictionary, but that should be nulled out
        DictionaryPropertyDescriptor<T> * descriptor;
        // We can't look it up using the slot index, as one propertyId might have multiple slots,  do the propertyId map lookup
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (!propertyMap->TryGetReference(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1964\n");
            return false;
        }
        // This function is only used by LdRootFld, so the index will allow let const globals
        Assert(descriptor->template GetDataPropertyIndex<true>() == index);
        if (descriptor->Attributes & PropertyDeleted)
        {LOGMEIN("DictionaryTypeHandler.cpp] 1970\n");
            return false;
        }
        *attributes = descriptor->Attributes & PropertyDynamicTypeDefaults;
        return true;
    }

    template <typename T>
    Var DictionaryTypeHandlerBase<T>::CanonicalizeAccessor(Var accessor, /*const*/ JavascriptLibrary* library)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1979\n");
        if (accessor == nullptr || JavascriptOperators::IsUndefinedObject(accessor, library))
        {LOGMEIN("DictionaryTypeHandler.cpp] 1981\n");
            accessor = library->GetDefaultAccessorFunction();
        }
        return accessor;
    }

    template <typename T>
    BigDictionaryTypeHandler* DictionaryTypeHandlerBase<T>::ConvertToBigDictionaryTypeHandler(DynamicObject* instance)
    {LOGMEIN("DictionaryTypeHandler.cpp] 1989\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 2010\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2045\n");
        return RecyclerNew(recycler, BigDictionaryTypeHandler, recycler, slotCapacity, inlineSlotCapacity, offsetOfInlineSlots);
    }

    template <>
    BigDictionaryTypeHandler* DictionaryTypeHandlerBase<BigPropertyIndex>::ConvertToBigDictionaryTypeHandler(DynamicObject* instance)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2051\n");
        Throw::OutOfMemory();
    }

    template<>
    BOOL DictionaryTypeHandlerBase<PropertyIndex>::IsBigDictionaryTypeHandler()
    {LOGMEIN("DictionaryTypeHandler.cpp] 2057\n");
        return FALSE;
    }

    template<>
    BOOL DictionaryTypeHandlerBase<BigPropertyIndex>::IsBigDictionaryTypeHandler()
    {LOGMEIN("DictionaryTypeHandler.cpp] 2063\n");
        return TRUE;
    }

    template <typename T>
    BOOL DictionaryTypeHandlerBase<T>::AddProperty(DynamicObject* instance, const PropertyRecord* propertyRecord, Var value,
        PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, bool throwIfNotExtensible, SideEffects possibleSideEffects)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2070\n");
        AnalysisAssert(instance);
        ScriptContext* scriptContext = instance->GetScriptContext();
        bool isForce = (flags & PropertyOperation_Force) != 0;

#if DBG
        DictionaryPropertyDescriptor<T>* descriptor;
        Assert(!propertyMap->TryGetReference(propertyRecord, &descriptor));
        Assert(!propertyRecord->IsNumeric());
#endif

        if (!isForce)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2082\n");
            if (!this->VerifyIsExtensible(scriptContext, throwIfNotExtensible))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2084\n");
                return FALSE;
            }
        }

        if (this->GetSlotCapacity() <= nextPropertyIndex)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2090\n");
            if (this->GetSlotCapacity() >= MaxPropertyIndexSize ||
                (this->GetSlotCapacity() >= CONFIG_FLAG(BigDictionaryTypeHandlerThreshold) && !this->IsBigDictionaryTypeHandler()))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2093\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 2110\n");
            newDescriptor.IsInitialized = true;
            if (localSingletonInstance == instance && !IsInternalPropertyId(propertyRecord->GetPropertyId()) &&
                (flags & (PropertyOperation_NonFixedValue | PropertyOperation_SpecialValue)) == 0)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2114\n");
                Assert(value != nullptr);
                // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
                Assert(!instance->IsExternal());
                newDescriptor.IsFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() & CheckHeuristicsForFixedDataProps(instance, propertyRecord, value)));
            }
        }

        propertyMap->Add(propertyRecord, newDescriptor);

        if (attributes & PropertyEnumerable)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2125\n");
            instance->SetHasNoEnumerableProperties(false);
        }

        if (!(attributes & PropertyWritable))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2130\n");
            this->ClearHasOnlyWritableDataProperties();
            if(GetFlags() & IsPrototypeFlag)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2133\n");
                instance->GetScriptContext()->InvalidateStoreFieldCaches(propertyRecord->GetPropertyId());
                instance->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
            }
        }

        SetSlotUnchecked(instance, index, value);

        // If we just added a fixed method, don't populate the inline cache so that we always take the
        // slow path when overwriting this property and correctly invalidate any JIT-ed code that hard-coded
        // this method.
        if (newDescriptor.IsFixed)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2145\n");
            PropertyValueInfo::SetNoCache(info, instance);
        }
        else
        {
            SetPropertyValueInfo(info, instance, index, attributes);
        }

        if (!IsInternalPropertyId(propertyRecord->GetPropertyId()) && ((this->GetFlags() & IsPrototypeFlag)
            || JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(instance, propertyRecord->GetPropertyId())))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2155\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2171\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2186\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 2201\n");
            for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2203\n");
                const PropertyRecord* propertyRecord = this->propertyMap->GetKeyAt(propertyIndex);
                DictionaryPropertyDescriptor<T>* descriptor = this->propertyMap->GetReferenceAt(propertyIndex);
                InvalidateFixedField(instance, propertyRecord->GetPropertyId(), descriptor);
            }
        }

        Js::RecyclableObject* undefined = instance->GetLibrary()->GetUndefined();
        Js::JavascriptFunction* defaultAccessor = instance->GetLibrary()->GetDefaultAccessorFunction();
        for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2213\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2231\n");
        // Note: This method is currently only called from ResetObject, which in turn only applies to external objects.
        // Before using for other purposes, make sure the assumptions made here make sense in the new context.  In particular,
        // the invalidateFixedFields == false is only correct if a) the object is known not to have any, or b) the type of the
        // object has changed and/or property guards have already been invalidated through some other means.
        if (invalidateFixedFields)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2237\n");
            int propertyCount = this->propertyMap->Count();
            for (int propertyIndex = 0; propertyIndex < propertyCount; propertyIndex++)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2240\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2256\n");
        return JavascriptArray::Is(instance) ? ConvertToES5ArrayType(instance) : this;
    }

    template <typename T>
    void DictionaryTypeHandlerBase<T>::SetIsPrototype(DynamicObject* instance)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2262\n");
        // Don't return if IsPrototypeFlag is set, because we may still need to do a type transition and
        // set fixed bits.  If this handler were to be shared, this instance may not be a prototype yet.
        // We might need to convert to a non-shared type handler and/or change type.
        if (!ChangeTypeOnProto() && !(GetIsOrMayBecomeShared() && IsolatePrototypes()))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2267\n");
            SetFlags(IsPrototypeFlag);
            return;
        }

        Assert(!GetIsShared() || this->singletonInstance == nullptr);
        Assert(this->singletonInstance == nullptr || this->singletonInstance->Get() == instance);

        // Review (jedmiad): Why isn't this getting inlined?
        const auto setFixedFlags = [instance](const PropertyRecord* propertyRecord, DictionaryPropertyDescriptor<T>* const descriptor, bool hasNewType)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2277\n");
            if (IsInternalPropertyId(propertyRecord->GetPropertyId()))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2279\n");
                return;
            }
            if (!(descriptor->Attributes & PropertyDeleted))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2283\n");
                // See PathTypeHandlerBase::ConvertToSimpleDictionaryType for rules governing fixed field bits during type
                // handler transitions.  In addition, we know that the current instance is not yet a prototype.

                Assert(descriptor->SanityCheckFixedBits());
                if (descriptor->IsInitialized)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2289\n");
                    // Since DictionaryTypeHandlers are never shared, we can set fixed fields and clear used as fixed as long
                    // as we have changed the type.  Otherwise populated load field caches would still be valid and would need
                    // to be explicitly invalidated if the property value changes.
                    if (hasNewType)
                    {LOGMEIN("DictionaryTypeHandler.cpp] 2294\n");
                        T dataSlot = descriptor->template GetDataPropertyIndex<false>();
                        if (dataSlot != NoSlots)
                        {LOGMEIN("DictionaryTypeHandler.cpp] 2297\n");
                            Var value = instance->GetSlot(dataSlot);
                            // Because DictionaryTypeHandlers are never shared we should always have a property value if the handler
                            // says it's initialized.
                            Assert(value != nullptr);
                            descriptor->IsFixed = (JavascriptFunction::Is(value) ? ShouldFixMethodProperties() : (ShouldFixDataProperties() && CheckHeuristicsForFixedDataProps(instance, propertyRecord, value)));
                        }
                        else if (descriptor->IsAccessor)
                        {LOGMEIN("DictionaryTypeHandler.cpp] 2305\n");
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
                {
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 2336\n");
            // Forcing a type transition allows us to fix all fields (even those that were previously marked as non-fixed).
            instance->ChangeType();
            Assert(!instance->HasSharedType());
            hasNewType = true;
        }

        // Currently there is no way to become the prototype if you are a stack instance
        Assert(!ThreadContext::IsOnStack(instance));
        if (AreSingletonInstancesNeeded() && this->singletonInstance == nullptr)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2346\n");
            this->singletonInstance = instance->CreateWeakReferenceToSelf();
        }

        // We don't want fixed properties on external objects.  See DynamicObject::ResetObject for more information.
        if (!instance->IsExternal())
        {LOGMEIN("DictionaryTypeHandler.cpp] 2352\n");
            // The propertyMap dictionary is guaranteed to have contiguous entries because we never remove entries from it.
            for (int i = 0; i < propertyMap->Count(); i++)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2355\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2372\n");
        return this->singletonInstance != nullptr;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::TryUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2378\n");
        bool result = TryGetFixedProperty<false, true>(propertyRecord, pProperty, propertyType, requestContext);
        TraceUseFixedProperty(propertyRecord, pProperty, result, _u("DictionaryTypeHandler"), requestContext);
        return result;
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::TryUseFixedAccessor(PropertyRecord const * propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2386\n");
        bool result = TryGetFixedAccessor<false, true>(propertyRecord, pAccessor, propertyType, getter, requestContext);
        TraceUseFixedProperty(propertyRecord, pAccessor, result, _u("DictionaryTypeHandler"), requestContext);
        return result;
    }

#if DBG
    template <typename T>
    bool DictionaryTypeHandlerBase<T>::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2395\n");
        ScriptContext* scriptContext = instance->GetScriptContext();
        DictionaryPropertyDescriptor<T> descriptor;

        // We pass Constants::NoProperty for ActivationObjects for functions with same named formals.
        if (propertyId == Constants::NoProperty)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2401\n");
            return true;
        }

        PropertyRecord const* propertyRecord = scriptContext->GetPropertyName(propertyId);
        if (propertyMap->TryGetValue(propertyRecord, &descriptor))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2407\n");
            if (allowLetConst && (descriptor.Attributes & PropertyLetConstGlobal))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2409\n");
                return true;
            }
            else
            {
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2426\n");
        return TryGetFixedProperty<true, false>(propertyRecord, pProperty, (Js::FixedPropertyKind)(Js::FixedPropertyKind::FixedMethodProperty | Js::FixedPropertyKind::FixedDataProperty), requestContext);
    }

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::HasAnyFixedProperties() const
    {LOGMEIN("DictionaryTypeHandler.cpp] 2432\n");
        for (int i = 0; i < propertyMap->Count(); i++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2434\n");
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(i);
            if (descriptor.IsFixed)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2437\n");
                return true;
            }
        }
        return false;
    }
#endif

    template <typename T>
    template <bool allowNonExistent, bool markAsUsed>
    bool DictionaryTypeHandlerBase<T>::TryGetFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2448\n");
        // Note: This function is not thread-safe and cannot be called from the JIT thread.  That's why we collect and
        // cache any fixed function instances during work item creation on the main thread.
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        if (localSingletonInstance != nullptr && localSingletonInstance->GetScriptContext() == requestContext)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2453\n");
            DictionaryPropertyDescriptor<T>* descriptor;
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2456\n");
                if (descriptor->Attributes & PropertyDeleted || !descriptor->IsFixed)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2458\n");
                    return false;
                }
                T dataSlot = descriptor->template GetDataPropertyIndex<false>();
                if (dataSlot != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2463\n");
                    Assert(!IsInternalPropertyId(propertyRecord->GetPropertyId()));
                    Var value = localSingletonInstance->GetSlot(dataSlot);
                    if (value && ((IsFixedMethodProperty(propertyType) && JavascriptFunction::Is(value)) || IsFixedDataProperty(propertyType)))
                    {LOGMEIN("DictionaryTypeHandler.cpp] 2467\n");
                        *pProperty = value;
                        if (markAsUsed)
                        {LOGMEIN("DictionaryTypeHandler.cpp] 2470\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2489\n");
        // Note: This function is not thread-safe and cannot be called from the JIT thread.  That's why we collect and
        // cache any fixed function instances during work item creation on the main thread.
        DynamicObject* localSingletonInstance = this->singletonInstance != nullptr ? this->singletonInstance->Get() : nullptr;
        if (localSingletonInstance != nullptr && localSingletonInstance->GetScriptContext() == requestContext)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2494\n");
            DictionaryPropertyDescriptor<T>* descriptor;
            if (propertyMap->TryGetReference(propertyRecord, &descriptor))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2497\n");
                if (descriptor->Attributes & PropertyDeleted || !descriptor->IsAccessor || !descriptor->IsFixed)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2499\n");
                    return false;
                }

                T accessorSlot = getter ? descriptor->GetGetterPropertyIndex() : descriptor->GetSetterPropertyIndex();
                if (accessorSlot != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2505\n");
                    Assert(!IsInternalPropertyId(propertyRecord->GetPropertyId()));
                    Var value = localSingletonInstance->GetSlot(accessorSlot);
                    if (value && IsFixedAccessorProperty(propertyType) && JavascriptFunction::Is(value))
                    {LOGMEIN("DictionaryTypeHandler.cpp] 2509\n");
                        *pAccessor = value;
                        if (markAsUsed)
                        {LOGMEIN("DictionaryTypeHandler.cpp] 2512\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2530\n");
        if (this->singletonInstance != nullptr)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2532\n");
            Assert(AreSingletonInstancesNeeded());
            Assert(this->singletonInstance->Get() == instance);
            typeHandler->SetSingletonInstanceUnchecked(this->singletonInstance);
        }
    }

    template <typename T>
    template <typename TPropertyKey>
    void DictionaryTypeHandlerBase<T>::InvalidateFixedField(DynamicObject* instance, TPropertyKey propertyKey, DictionaryPropertyDescriptor<T>* descriptor)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2542\n");
        // DictionaryTypeHandlers are never shared, but if they were we would need to invalidate even if
        // there wasn't a singleton instance.  See SimpleDictionaryTypeHandler::InvalidateFixedFields.
        Assert(!GetIsOrMayBecomeShared());
        if (this->singletonInstance != nullptr)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2547\n");
            Assert(this->singletonInstance->Get() == instance);

            // Even if we wrote a new value into this property (overwriting a previously fixed one), we don't
            // consider the new one fixed. This also means that it's ok to populate the inline caches for
            // this property from now on.
            descriptor->IsFixed = false;

            if (descriptor->UsedAsFixed)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2556\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2571\n");
        DictionaryPropertyDescriptor<T>* descriptor;
        PropertyRecord const* propertyRecord = instance->GetScriptContext()->GetPropertyName(propertyId);
        if (propertyMap->TryGetReference(propertyRecord, &descriptor) && (descriptor->Attributes & PropertyLetConstGlobal))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2575\n");
            return true;
        }
        return false;
    }
#endif

    template <typename T>
    bool DictionaryTypeHandlerBase<T>::NextLetConstGlobal(int& index, RootObjectBase* instance, const PropertyRecord** propertyRecord, Var* value, bool* isConst)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2584\n");
        for (; index < propertyMap->Count(); index++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2586\n");
            DictionaryPropertyDescriptor<T> descriptor = propertyMap->GetValueAt(index);

            if (descriptor.Attributes & PropertyLetConstGlobal)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2590\n");
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
    void DictionaryTypeHandlerBase<T>::DumpFixedFields() const {LOGMEIN("DictionaryTypeHandler.cpp] 2606\n");
        for (int i = 0; i < propertyMap->Count(); i++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2608\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2622\n");
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2624\n");
            Output::Print(_u("FixedFields: converting 0x%p from %s to %s:\n"), instance, oldTypeHandlerName, newTypeHandlerName);
            Output::Print(_u("   before: type = 0x%p, type handler = 0x%p, old singleton = 0x%p(0x%p)\n"),
                oldType, oldTypeHandler, oldSingletonInstanceBefore, oldSingletonInstanceBefore != nullptr ? oldSingletonInstanceBefore->Get() : nullptr);
            Output::Print(_u("   fixed fields:"));
            oldTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2633\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2646\n");
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2648\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 2661\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2678\n");
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2680\n");
            Output::Print(_u("FixedFields: PathTypeHandler::SetIsPrototype(0x%p):\n"), instance);
            Output::Print(_u("   before: type = 0x%p, old singleton = 0x%p(0x%p)\n"),
                oldType, oldSingletonInstanceBefore, oldSingletonInstanceBefore != nullptr ? oldSingletonInstanceBefore->Get() : nullptr);
            Output::Print(_u("   fixed fields:"));
            oldTypeHandler->DumpFixedFields();
            Output::Print(_u("\n"));
        }
        if (PHASE_VERBOSE_TESTTRACE1(FixMethodPropsPhase))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2689\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2702\n");
        if (PHASE_VERBOSE_TRACE1(FixMethodPropsPhase))
        {LOGMEIN("DictionaryTypeHandler.cpp] 2704\n");
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
        {LOGMEIN("DictionaryTypeHandler.cpp] 2717\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2733\n");
        for(auto iter = this->propertyMap->GetIterator(); iter.IsValid(); iter.MoveNext())
        {LOGMEIN("DictionaryTypeHandler.cpp] 2735\n");
            DictionaryPropertyDescriptor<T> descriptor = iter.CurrentValue();

            //
            //TODO: not sure about relationship with PropertyLetConstGlobal here need to -- check how GetProperty works
            //      maybe we need to template this with allowLetGlobalConst as well
            //

            Js::PropertyId pid = iter.CurrentKey()->GetPropertyId();
            if((!DynamicTypeHandler::ShouldMarkPropertyId_TTD(pid)) | (!descriptor.IsInitialized) | (descriptor.Attributes & PropertyDeleted))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2745\n");
                continue;
            }

            T dIndex = descriptor.template GetDataPropertyIndex<false>();
            if(dIndex != NoSlots)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2751\n");
                Js::Var dValue = obj->GetSlot(dIndex);
                extractor->MarkVisitVar(dValue);
            }
            else
            {
                T gIndex = descriptor.GetGetterPropertyIndex();
                if(gIndex != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2759\n");
                    Js::Var gValue = obj->GetSlot(gIndex);
                    extractor->MarkVisitVar(gValue);
                }

                T sIndex = descriptor.GetSetterPropertyIndex();
                if(sIndex != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2766\n");
                    Js::Var sValue = obj->GetSlot(sIndex);
                    extractor->MarkVisitVar(sValue);
                }
            }
        }
    }

    template <typename T>
    uint32 DictionaryTypeHandlerBase<T>::ExtractSlotInfo_TTD(TTD::NSSnapType::SnapHandlerPropertyEntry* entryInfo, ThreadContext* threadContext, TTD::SlabAllocator& alloc) const
    {LOGMEIN("DictionaryTypeHandler.cpp] 2776\n");
        T maxSlot = 0;

        for(auto iter = this->propertyMap->GetIterator(); iter.IsValid(); iter.MoveNext())
        {LOGMEIN("DictionaryTypeHandler.cpp] 2780\n");
            DictionaryPropertyDescriptor<T> descriptor = iter.CurrentValue();
            Js::PropertyId pid = iter.CurrentKey()->GetPropertyId();

            T dIndex = descriptor.template GetDataPropertyIndex<false>();
            if(dIndex != NoSlots)
            {LOGMEIN("DictionaryTypeHandler.cpp] 2786\n");
                maxSlot = max(maxSlot, dIndex);

                TTD::NSSnapType::SnapEntryDataKindTag tag = descriptor.IsInitialized ? TTD::NSSnapType::SnapEntryDataKindTag::Data : TTD::NSSnapType::SnapEntryDataKindTag::Uninitialized;
                TTD::NSSnapType::ExtractSnapPropertyEntryInfo(entryInfo + dIndex, pid, descriptor.Attributes, tag);
            }
            else
            {
                TTDAssert(descriptor.IsInitialized, "How can this not be initialized?");

                T gIndex = descriptor.GetGetterPropertyIndex();
                if(gIndex != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2798\n");
                    maxSlot = max(maxSlot, gIndex);

                    TTD::NSSnapType::SnapEntryDataKindTag tag = TTD::NSSnapType::SnapEntryDataKindTag::Getter;
                    TTD::NSSnapType::ExtractSnapPropertyEntryInfo(entryInfo + gIndex, pid, descriptor.Attributes, tag);
                }

                T sIndex = descriptor.GetSetterPropertyIndex();
                if(sIndex != NoSlots)
                {LOGMEIN("DictionaryTypeHandler.cpp] 2807\n");
                    maxSlot = max(maxSlot, sIndex);

                    TTD::NSSnapType::SnapEntryDataKindTag tag = TTD::NSSnapType::SnapEntryDataKindTag::Setter;
                    TTD::NSSnapType::ExtractSnapPropertyEntryInfo(entryInfo + sIndex, pid, descriptor.Attributes, tag);
                }
            }
        }

        if(this->propertyMap->Count() == 0)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2817\n");
            return 0;
        }
        else
        {
            return (uint32)(maxSlot + 1);
        }
    }

    template <typename T>
    Js::BigPropertyIndex DictionaryTypeHandlerBase<T>::GetPropertyIndex_EnumerateTTD(const Js::PropertyRecord* pRecord)
    {LOGMEIN("DictionaryTypeHandler.cpp] 2828\n");
        for(Js::BigPropertyIndex index = 0; index < this->propertyMap->Count(); index++)
        {LOGMEIN("DictionaryTypeHandler.cpp] 2830\n");
            Js::PropertyId pid = this->propertyMap->GetKeyAt(index)->GetPropertyId();
            const DictionaryPropertyDescriptor<T>& idescriptor = propertyMap->GetValueAt(index);

            if(pid == pRecord->GetPropertyId() && !(idescriptor.Attributes & PropertyDeleted))
            {LOGMEIN("DictionaryTypeHandler.cpp] 2835\n");
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
    {LOGMEIN("DictionaryTypeHandler.cpp] 2850\n");
        return (allowLetConstGlobal && (attributes & PropertyLetConstGlobal) != 0) ? (attributes | PropertyWritable) : attributes;
    }
}
