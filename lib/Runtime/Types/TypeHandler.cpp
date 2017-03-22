//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    BigPropertyIndex
    DynamicTypeHandler::GetPropertyIndexFromInlineSlotIndex(uint inlineSlot)
    {LOGMEIN("TypeHandler.cpp] 10\n");
        return inlineSlot - (offsetOfInlineSlots / sizeof(Var *));
    }

    BigPropertyIndex
    DynamicTypeHandler::GetPropertyIndexFromAuxSlotIndex(uint auxIndex)
    {LOGMEIN("TypeHandler.cpp] 16\n");
        return auxIndex + this->GetInlineSlotCapacity();
    }

    PropertyIndex DynamicTypeHandler::RoundUpObjectHeaderInlinedInlineSlotCapacity(const PropertyIndex slotCapacity)
    {LOGMEIN("TypeHandler.cpp] 21\n");
        const PropertyIndex objectHeaderInlinableSlotCapacity = GetObjectHeaderInlinableSlotCapacity();
        if(slotCapacity <= objectHeaderInlinableSlotCapacity)
        {LOGMEIN("TypeHandler.cpp] 24\n");
            return objectHeaderInlinableSlotCapacity;
        }

        // Align the slot capacity for slots that are outside the object header, and add to that the slot capacity for slots
        // that are inside the object header
        return RoundUpInlineSlotCapacity(slotCapacity - objectHeaderInlinableSlotCapacity) + objectHeaderInlinableSlotCapacity;
    }

    PropertyIndex DynamicTypeHandler::RoundUpInlineSlotCapacity(const PropertyIndex slotCapacity)
    {LOGMEIN("TypeHandler.cpp] 34\n");
        return ::Math::Align<PropertyIndex>(slotCapacity, HeapConstants::ObjectGranularity / sizeof(Var));
    }

    int DynamicTypeHandler::RoundUpAuxSlotCapacity(const int slotCapacity)
    {LOGMEIN("TypeHandler.cpp] 39\n");
        CompileAssert(4 * sizeof(Var) % HeapConstants::ObjectGranularity == 0);
        return ::Math::Align<int>(slotCapacity, 4);
    }

    int DynamicTypeHandler::RoundUpSlotCapacity(const int slotCapacity, const PropertyIndex inlineSlotCapacity)
    {LOGMEIN("TypeHandler.cpp] 45\n");
        Assert(slotCapacity >= 0);

        if(slotCapacity <= inlineSlotCapacity)
        {LOGMEIN("TypeHandler.cpp] 49\n");
            return inlineSlotCapacity;
        }

        const int auxSlotCapacity = RoundUpAuxSlotCapacity(slotCapacity - inlineSlotCapacity);
        Assert(auxSlotCapacity + inlineSlotCapacity >= auxSlotCapacity);
        const int maxSlotCapacity =
            slotCapacity <= PropertyIndexRanges<PropertyIndex>::MaxValue
                ? PropertyIndexRanges<PropertyIndex>::MaxValue
                : PropertyIndexRanges<BigPropertyIndex>::MaxValue;
        return min(maxSlotCapacity, inlineSlotCapacity + auxSlotCapacity);
    }

    DynamicTypeHandler::DynamicTypeHandler(int slotCapacity, uint16 inlineSlotCapacity, uint16 offsetOfInlineSlots, BYTE flags) :
        flags(flags),
        propertyTypes(PropertyTypesWritableDataOnly | PropertyTypesReserved),
        offsetOfInlineSlots(offsetOfInlineSlots),
        unusedBytes(Js::AtomTag)
    {LOGMEIN("TypeHandler.cpp] 67\n");
        Assert(!GetIsOrMayBecomeShared() || GetIsLocked());
        Assert(offsetOfInlineSlots != 0 || inlineSlotCapacity == 0);
        Assert(!IsObjectHeaderInlined(offsetOfInlineSlots) || inlineSlotCapacity != 0);

        // Align the slot capacities and set the total slot capacity
        this->inlineSlotCapacity = inlineSlotCapacity =
            IsObjectHeaderInlined(offsetOfInlineSlots)
                ? RoundUpObjectHeaderInlinedInlineSlotCapacity(inlineSlotCapacity)
                : RoundUpInlineSlotCapacity(inlineSlotCapacity);
        this->slotCapacity = RoundUpSlotCapacity(slotCapacity, inlineSlotCapacity);
        this->isNotPathTypeHandlerOrHasUserDefinedCtor = true;

        Assert(IsObjectHeaderInlinedTypeHandler() == IsObjectHeaderInlined(offsetOfInlineSlots));
    }

    Var DynamicTypeHandler::GetSlot(DynamicObject * instance, int index)
    {LOGMEIN("TypeHandler.cpp] 84\n");
        if (index < inlineSlotCapacity)
        {LOGMEIN("TypeHandler.cpp] 86\n");
            Var * slots = reinterpret_cast<Var*>(reinterpret_cast<size_t>(instance) + offsetOfInlineSlots);
            Var value = slots[index];
            Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
            return value;
        }
        else
        {
            Var value = instance->auxSlots[index - inlineSlotCapacity];
            Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
            return value;
        }
    }

    Var DynamicTypeHandler::GetInlineSlot(DynamicObject * instance, int index)
    {LOGMEIN("TypeHandler.cpp] 101\n");
        AssertMsg(index >= (int)(offsetOfInlineSlots / sizeof(Var)), "index should be relative to the address of the object");
        Assert(index - (int)(offsetOfInlineSlots / sizeof(Var)) < this->GetInlineSlotCapacity());
        Var * slots = reinterpret_cast<Var*>(instance);
        Var value = slots[index];
        Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
        return value;
    }

    Var DynamicTypeHandler::GetAuxSlot(DynamicObject * instance, int index)
    {LOGMEIN("TypeHandler.cpp] 111\n");
        // We should only assign a stack value only to a stack object (current mark temp number in mark temp object)

        Assert(index < GetSlotCapacity() - GetInlineSlotCapacity());
        Var value = instance->auxSlots[index];
        Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
        return value;
    }

#if DBG
    void DynamicTypeHandler::SetSlot(DynamicObject* instance, PropertyId propertyId, bool allowLetConst, int index, Var value)
#else
    void DynamicTypeHandler::SetSlot(DynamicObject* instance, int index, Var value)
#endif
    {
        Assert(index < GetSlotCapacity());
        Assert(propertyId == Constants::NoProperty || CanStorePropertyValueDirectly(instance, propertyId, allowLetConst));
        SetSlotUnchecked(instance, index, value);
    }

    void DynamicTypeHandler::SetSlotUnchecked(DynamicObject * instance, int index, Var value)
    {LOGMEIN("TypeHandler.cpp] 132\n");
        // We should only assign a stack value only to a stack object (current mark temp number in mark temp object)
        Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
        uint16 inlineSlotCapacity = instance->GetTypeHandler()->GetInlineSlotCapacity();
        uint16 offsetOfInlineSlots = instance->GetTypeHandler()->GetOffsetOfInlineSlots();
        int slotCapacity = instance->GetTypeHandler()->GetSlotCapacity();

        if (index < inlineSlotCapacity)
        {LOGMEIN("TypeHandler.cpp] 140\n");
            Field(Var) * slots = reinterpret_cast<Field(Var)*>(reinterpret_cast<size_t>(instance) + offsetOfInlineSlots);
            slots[index] = value;
        }
        else
        {
            Assert((index - inlineSlotCapacity) < (slotCapacity - inlineSlotCapacity));
            instance->auxSlots[index - inlineSlotCapacity] = value;
        }
    }

#if DBG
    void DynamicTypeHandler::SetInlineSlot(DynamicObject* instance, PropertyId propertyId, bool allowLetConst, int index, Var value)
#else
    void DynamicTypeHandler::SetInlineSlot(DynamicObject* instance, int index, Var value)
#endif
    {
        // We should only assign a stack value only to a stack object (current mark temp number in mark temp object)
        Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
        AssertMsg(index >= (int)(offsetOfInlineSlots / sizeof(Var)), "index should be relative to the address of the object");
        Assert(index - (int)(offsetOfInlineSlots / sizeof(Var)) < this->GetInlineSlotCapacity());
        Assert(propertyId == Constants::NoProperty || CanStorePropertyValueDirectly(instance, propertyId, allowLetConst));

        Field(Var) * slots = reinterpret_cast<Field(Var)*>(instance);
        slots[index] = value;
    }

#if DBG
    void DynamicTypeHandler::SetAuxSlot(DynamicObject* instance, PropertyId propertyId, bool allowLetConst, int index, Var value)
#else
    void DynamicTypeHandler::SetAuxSlot(DynamicObject* instance, int index, Var value)
#endif
    {
        // We should only assign a stack value only to a stack object (current mark temp number in mark temp object)
        Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
        Assert(index < GetSlotCapacity() - GetInlineSlotCapacity());
        Assert(propertyId == Constants::NoProperty || CanStorePropertyValueDirectly(instance, propertyId, allowLetConst));
        instance->auxSlots[index] = value;
    }

    void
    DynamicTypeHandler::SetInstanceTypeHandler(DynamicObject * instance, bool hasChanged)
    {
        SetInstanceTypeHandler(instance, this, hasChanged);
    }

    bool DynamicTypeHandler::IsObjectHeaderInlined(const uint16 offsetOfInlineSlots)
    {LOGMEIN("TypeHandler.cpp] 187\n");
        return offsetOfInlineSlots == GetOffsetOfObjectHeaderInlineSlots();
    }

    bool DynamicTypeHandler::IsObjectHeaderInlinedTypeHandlerUnchecked() const
    {LOGMEIN("TypeHandler.cpp] 192\n");
        return IsObjectHeaderInlined(GetOffsetOfInlineSlots());
    }

    bool DynamicTypeHandler::IsObjectHeaderInlinedTypeHandler() const
    {LOGMEIN("TypeHandler.cpp] 197\n");
        const bool isObjectHeaderInlined = IsObjectHeaderInlinedTypeHandlerUnchecked();
        if(isObjectHeaderInlined)
        {LOGMEIN("TypeHandler.cpp] 200\n");
            VerifyObjectHeaderInlinedTypeHandler();
        }
        return isObjectHeaderInlined;
    }

    void DynamicTypeHandler::VerifyObjectHeaderInlinedTypeHandler() const
    {LOGMEIN("TypeHandler.cpp] 207\n");
        Assert(IsObjectHeaderInlined(GetOffsetOfInlineSlots()));
        Assert(GetInlineSlotCapacity() >= GetObjectHeaderInlinableSlotCapacity());
        Assert(GetInlineSlotCapacity() == GetSlotCapacity());
    }

    uint16 DynamicTypeHandler::GetOffsetOfObjectHeaderInlineSlots()
    {LOGMEIN("TypeHandler.cpp] 214\n");
        return offsetof(DynamicObject, auxSlots);
    }

    PropertyIndex DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity()
    {LOGMEIN("TypeHandler.cpp] 219\n");
        const PropertyIndex maxAllowedSlotCapacity = (sizeof(DynamicObject) - DynamicTypeHandler::GetOffsetOfObjectHeaderInlineSlots()) / sizeof(Var);
        AssertMsg(maxAllowedSlotCapacity == 2, "Today we should be getting 2 with the math here. Change this Assert, if we are changing this logic in the future");
        return maxAllowedSlotCapacity;
    }

    void
        DynamicTypeHandler::SetInstanceTypeHandler(DynamicObject * instance, DynamicTypeHandler * typeHandler, bool hasChanged)
    {LOGMEIN("TypeHandler.cpp] 227\n");
        instance->SetTypeHandler(typeHandler, hasChanged);
    }

    DynamicTypeHandler *
    DynamicTypeHandler::GetCurrentTypeHandler(DynamicObject * instance)
    {LOGMEIN("TypeHandler.cpp] 233\n");
        return instance->GetTypeHandler();
    }

    void
    DynamicTypeHandler::ReplaceInstanceType(DynamicObject * instance, DynamicType * type)
    {LOGMEIN("TypeHandler.cpp] 239\n");
        instance->ReplaceType(type);
    }

    void
    DynamicTypeHandler::ResetTypeHandler(DynamicObject * instance)
    {LOGMEIN("TypeHandler.cpp] 245\n");
        // just reuse the current type handler.
        this->SetInstanceTypeHandler(instance);
    }

    BOOL
    DynamicTypeHandler::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {LOGMEIN("TypeHandler.cpp] 253\n");
        // Type handlers that support big property indexes override this function, so if we're here then this type handler does
        // not support big property indexes. Forward the call to the small property index version.
        Assert(GetSlotCapacity() <= PropertyIndexRanges<PropertyIndex>::MaxValue);
        PropertyIndex smallIndex = static_cast<PropertyIndex>(index);
        Assert(static_cast<BigPropertyIndex>(smallIndex) == index);
        const BOOL found = FindNextProperty(scriptContext, smallIndex, propertyString, propertyId, attributes, type, typeToEnumerate, flags);
        index = smallIndex;
        return found;
    }

    template<bool isStoreField>
    void DynamicTypeHandler::InvalidateInlineCachesForAllProperties(ScriptContext* requestContext)
    {LOGMEIN("TypeHandler.cpp] 266\n");
        int count = GetPropertyCount();
        if (count < 128) // Invalidate a propertyId involves dictionary lookups. Only do this when the number is relatively small.
        {LOGMEIN("TypeHandler.cpp] 269\n");
            for (int i = 0; i < count; i++)
            {LOGMEIN("TypeHandler.cpp] 271\n");
                PropertyId propertyId = GetPropertyId(requestContext, static_cast<PropertyIndex>(i));
                if (propertyId != Constants::NoProperty)
                {LOGMEIN("TypeHandler.cpp] 274\n");
                    isStoreField ? requestContext->InvalidateStoreFieldCaches(propertyId) : requestContext->InvalidateProtoCaches(propertyId);
                }
            }
        }
        else
        {
            isStoreField ? requestContext->InvalidateAllStoreFieldCaches() : requestContext->InvalidateAllProtoCaches();
        }
    }

    void DynamicTypeHandler::InvalidateProtoCachesForAllProperties(ScriptContext* requestContext)
    {LOGMEIN("TypeHandler.cpp] 286\n");
        InvalidateInlineCachesForAllProperties<false>(requestContext);
    }

    void DynamicTypeHandler::InvalidateStoreFieldCachesForAllProperties(ScriptContext* requestContext)
    {LOGMEIN("TypeHandler.cpp] 291\n");
        InvalidateInlineCachesForAllProperties<true>(requestContext);
    }

    void DynamicTypeHandler::RemoveFromPrototype(DynamicObject* instance, ScriptContext * requestContext)
    {LOGMEIN("TypeHandler.cpp] 296\n");
        InvalidateProtoCachesForAllProperties(requestContext);
    }

    void DynamicTypeHandler::AddToPrototype(DynamicObject* instance, ScriptContext * requestContext)
    {LOGMEIN("TypeHandler.cpp] 301\n");
        InvalidateStoreFieldCachesForAllProperties(requestContext);
    }

    void DynamicTypeHandler::SetPrototype(DynamicObject* instance, RecyclableObject* newPrototype)
    {LOGMEIN("TypeHandler.cpp] 306\n");
        // Force a type transition on the instance to invalidate its inline caches
        DynamicTypeHandler::ResetTypeHandler(instance);

        // Put new prototype in place
        instance->GetDynamicType()->SetPrototype(newPrototype);
    }

    bool DynamicTypeHandler::TryUseFixedProperty(PropertyRecord const* propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {LOGMEIN("TypeHandler.cpp] 315\n");
        if (PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase) ||
            PHASE_VERBOSE_TRACE1(Js::UseFixedDataPropsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::UseFixedDataPropsPhase))
        {LOGMEIN("TypeHandler.cpp] 318\n");
            Output::Print(_u("FixedFields: attempt to use fixed property %s from DynamicTypeHandler returned false.\n"), propertyRecord->GetBuffer());
            if (this->HasSingletonInstance() && this->GetSingletonInstance()->Get()->GetScriptContext() != requestContext)
            {LOGMEIN("TypeHandler.cpp] 321\n");
                Output::Print(_u("FixedFields: Cross Site Script Context is used for property %s. \n"), propertyRecord->GetBuffer());
            }
            Output::Flush();
        }
        return false;
    }

    bool DynamicTypeHandler::TryUseFixedAccessor(PropertyRecord const* propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {LOGMEIN("TypeHandler.cpp] 330\n");
        if (PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase) ||
            PHASE_VERBOSE_TRACE1(Js::UseFixedDataPropsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::UseFixedDataPropsPhase))
        {LOGMEIN("TypeHandler.cpp] 333\n");
            Output::Print(_u("FixedFields: attempt to use fixed accessor %s from DynamicTypeHandler returned false.\n"), propertyRecord->GetBuffer());
            if (this->HasSingletonInstance() && this->GetSingletonInstance()->Get()->GetScriptContext() != requestContext)
            {LOGMEIN("TypeHandler.cpp] 336\n");
                Output::Print(_u("FixedFields: Cross Site Script Context is used for property %s. \n"), propertyRecord->GetBuffer());
            }
            Output::Flush();
        }
        return false;
    }

    bool DynamicTypeHandler::IsFixedMethodProperty(FixedPropertyKind fixedPropKind)
    {LOGMEIN("TypeHandler.cpp] 345\n");
        return (fixedPropKind & Js::FixedPropertyKind::FixedMethodProperty) == Js::FixedPropertyKind::FixedMethodProperty;
    }

    bool DynamicTypeHandler::IsFixedDataProperty(FixedPropertyKind fixedPropKind)
    {LOGMEIN("TypeHandler.cpp] 350\n");
        return ((fixedPropKind & Js::FixedPropertyKind::FixedDataProperty) == Js::FixedPropertyKind::FixedDataProperty) &&
            !PHASE_OFF1(UseFixedDataPropsPhase);
    }

    bool DynamicTypeHandler::IsFixedAccessorProperty(FixedPropertyKind fixedPropKind)
    {LOGMEIN("TypeHandler.cpp] 356\n");
        return (fixedPropKind & Js::FixedPropertyKind::FixedAccessorProperty) == Js::FixedPropertyKind::FixedAccessorProperty;
    }

    bool DynamicTypeHandler::CheckHeuristicsForFixedDataProps(DynamicObject* instance, const PropertyRecord * propertyRecord, Var value)
    {LOGMEIN("TypeHandler.cpp] 361\n");
        if (PHASE_FORCE1(Js::FixDataPropsPhase))
        {LOGMEIN("TypeHandler.cpp] 363\n");
            return true;
        }

        if (Js::TaggedInt::Is(value) &&
            ((instance->GetTypeId() == TypeIds_GlobalObject && instance->GetScriptContext()->IsIntConstPropertyOnGlobalObject(propertyRecord->GetPropertyId())) ||
            (instance->GetTypeId() == TypeIds_Object && instance->GetScriptContext()->IsIntConstPropertyOnGlobalUserObject(propertyRecord->GetPropertyId()))))
        {LOGMEIN("TypeHandler.cpp] 370\n");
            return true;
        }

        // Disabled by default
        if (PHASE_ON1(Js::FixDataVarPropsPhase))
        {LOGMEIN("TypeHandler.cpp] 376\n");
            if (instance->GetTypeHandler()->GetFlags() & IsPrototypeFlag)
            {LOGMEIN("TypeHandler.cpp] 378\n");
                return true;
            }
            if (instance->GetType()->GetTypeId() == TypeIds_GlobalObject)
            {LOGMEIN("TypeHandler.cpp] 382\n");
                // if we have statically seen multiple stores - we should not do this optimization
                RootObjectInlineCache* cache = (static_cast<Js::RootObjectBase*>(instance))->GetRootInlineCache(propertyRecord, /*isLoadMethod*/ false, /*isStore*/ true);
                uint refCount = cache->Release();
                return refCount <= 1;
            }
        }
        return false;
    }

    bool DynamicTypeHandler::CheckHeuristicsForFixedDataProps(DynamicObject* instance, PropertyId propertyId, Var value)
    {LOGMEIN("TypeHandler.cpp] 393\n");
        return CheckHeuristicsForFixedDataProps(instance, instance->GetScriptContext()->GetPropertyName(propertyId), value);
    }

    bool DynamicTypeHandler::CheckHeuristicsForFixedDataProps(DynamicObject* instance, JavascriptString * propertyKey, Var value)
    {LOGMEIN("TypeHandler.cpp] 398\n");
        return false;
    }

    bool DynamicTypeHandler::CheckHeuristicsForFixedDataProps(DynamicObject* instance, const PropertyRecord * propertyRecord, PropertyId propertyId, Var value)
    {LOGMEIN("TypeHandler.cpp] 403\n");
        if(propertyRecord)
        {LOGMEIN("TypeHandler.cpp] 405\n");
            return CheckHeuristicsForFixedDataProps(instance, propertyRecord, value);
        }
        else
        {
            return CheckHeuristicsForFixedDataProps(instance,propertyId,value);
        }
    }

    void DynamicTypeHandler::TraceUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, bool result, LPCWSTR typeHandlerName, ScriptContext * requestContext)
    {LOGMEIN("TypeHandler.cpp] 415\n");
        LPCWSTR fixedPropertyResultType = nullptr;
        bool log = false;

        if (pProperty && *pProperty && ((Js::JavascriptFunction::Is(*pProperty) && (PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))) ||
            ((PHASE_VERBOSE_TRACE1(Js::UseFixedDataPropsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::UseFixedDataPropsPhase))) ))
        {LOGMEIN("TypeHandler.cpp] 421\n");
            if(*pProperty == nullptr)
            {LOGMEIN("TypeHandler.cpp] 423\n");
                fixedPropertyResultType = _u("null");
            }
            else if (Js::JavascriptFunction::Is(*pProperty))
            {LOGMEIN("TypeHandler.cpp] 427\n");
                fixedPropertyResultType = _u("function");
            }
            else if (TaggedInt::Is(*pProperty))
            {LOGMEIN("TypeHandler.cpp] 431\n");
                fixedPropertyResultType = _u("int constant");
            }
            else
            {
                fixedPropertyResultType = _u("Var");
            }
            log = true;
        }

        if(log)
        {LOGMEIN("TypeHandler.cpp] 442\n");
            Output::Print(_u("FixedFields: attempt to use fixed property %s, which is a %s, from %s returned %s.\n"),
                propertyRecord->GetBuffer(), fixedPropertyResultType, typeHandlerName, IsTrueOrFalse(result));

            if (this->HasSingletonInstance() && this->GetSingletonInstance()->Get()->GetScriptContext() != requestContext)
            {LOGMEIN("TypeHandler.cpp] 447\n");
                Output::Print(_u("FixedFields: Cross Site Script Context is used for property %s. \n"), propertyRecord->GetBuffer());
            }

            Output::Flush();
        }
    }

    BOOL DynamicTypeHandler::GetInternalProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value)
    {LOGMEIN("TypeHandler.cpp] 456\n");
        // Type handlers that store internal properties differently from normal properties
        // override this method to provide access to them.  Otherwise, by default, simply
        // defer to GetProperty()
        return this->GetProperty(instance, originalInstance, propertyId, value, nullptr, nullptr);
    }

    BOOL DynamicTypeHandler::InitProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("TypeHandler.cpp] 464\n");
        // By default just call the SetProperty method
        return this->SetProperty(instance, propertyId, value, flags, info);
    }

    BOOL DynamicTypeHandler::SetInternalProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags)
    {LOGMEIN("TypeHandler.cpp] 470\n");
        // Type handlers that store internal properties differently from normal properties
        // override this method to provide access to them.  Otherwise, by default, simply
        // defer to SetProperty()
        return this->SetProperty(instance, propertyId, value, flags, nullptr);
    }

    //
    // Default implementations delegate to instance objectArray
    //
    BOOL DynamicTypeHandler::HasItem(DynamicObject* instance, uint32 index)
    {LOGMEIN("TypeHandler.cpp] 481\n");
        return instance->HasObjectArrayItem(index);
    }
    BOOL DynamicTypeHandler::SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("TypeHandler.cpp] 485\n");
        return instance->SetObjectArrayItem(index, value, flags);
    }
    BOOL DynamicTypeHandler::DeleteItem(DynamicObject* instance, uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("TypeHandler.cpp] 489\n");
        return instance->DeleteObjectArrayItem(index, flags);
    }
    BOOL DynamicTypeHandler::GetItem(DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {LOGMEIN("TypeHandler.cpp] 493\n");
        return instance->GetObjectArrayItem(originalInstance, index, value, requestContext);
    }

    DescriptorFlags DynamicTypeHandler::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("TypeHandler.cpp] 498\n");
        PropertyValueInfo::SetNoCache(info, instance);
        return this->HasProperty(instance, propertyId) ? WritableData : None;
    }

    DescriptorFlags DynamicTypeHandler::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("TypeHandler.cpp] 504\n");
        PropertyValueInfo::SetNoCache(info, instance);
        return this->HasProperty(instance, propertyNameString) ? WritableData : None;
    }

    DescriptorFlags DynamicTypeHandler::GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("TypeHandler.cpp] 510\n");
        return this->HasItem(instance, index) ? WritableData : None;
    }

    //
    // Default implementations upgrades type handler with item attribute/getter/setter support
    //
    BOOL DynamicTypeHandler::SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {LOGMEIN("TypeHandler.cpp] 518\n");
        return ConvertToTypeWithItemAttributes(instance)->SetItemWithAttributes(instance, index, value, attributes);
    }
    BOOL DynamicTypeHandler::SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {LOGMEIN("TypeHandler.cpp] 522\n");
        return ConvertToTypeWithItemAttributes(instance)->SetItemAttributes(instance, index, attributes);
    }
    BOOL DynamicTypeHandler::SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter)
    {LOGMEIN("TypeHandler.cpp] 526\n");
        return ConvertToTypeWithItemAttributes(instance)->SetItemAccessors(instance, index, getter, setter);
    }

    void DynamicTypeHandler::SetPropertyUpdateSideEffect(DynamicObject* instance, PropertyId propertyId, Var value, SideEffects possibleSideEffects)
    {LOGMEIN("TypeHandler.cpp] 531\n");
        if (possibleSideEffects && propertyId < PropertyIds::_countJSOnlyProperty)
        {LOGMEIN("TypeHandler.cpp] 533\n");
            ScriptContext* scriptContext = instance->GetScriptContext();

            if (scriptContext->GetConfig()->IsES6ToPrimitiveEnabled() && propertyId == PropertyIds::_symbolToPrimitive)
            {LOGMEIN("TypeHandler.cpp] 537\n");
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ValueOf & possibleSideEffects));
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ToString & possibleSideEffects));
            }

            else if (propertyId == PropertyIds::valueOf)
            {LOGMEIN("TypeHandler.cpp] 543\n");
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ValueOf & possibleSideEffects));
            }
            else if (propertyId == PropertyIds::toString)
            {LOGMEIN("TypeHandler.cpp] 547\n");
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ToString & possibleSideEffects));
            }
            else if (propertyId == PropertyIds::Math)
            {LOGMEIN("TypeHandler.cpp] 551\n");
                if (instance == scriptContext->GetLibrary()->GetGlobalObject())
                {LOGMEIN("TypeHandler.cpp] 553\n");
                    scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_MathFunc & possibleSideEffects));
                }
            }
            else if (IsMathLibraryId(propertyId))
            {LOGMEIN("TypeHandler.cpp] 558\n");
                if (instance == scriptContext->GetLibrary()->GetMathObject())
                {LOGMEIN("TypeHandler.cpp] 560\n");
                    scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_MathFunc & possibleSideEffects));
                }
            }
        }
    }

    void DynamicTypeHandler::SetPropertyUpdateSideEffect(DynamicObject* instance, JsUtil::CharacterBuffer<WCHAR> const& propertyName, Var value, SideEffects possibleSideEffects)
    {LOGMEIN("TypeHandler.cpp] 568\n");
        if (possibleSideEffects)
        {LOGMEIN("TypeHandler.cpp] 570\n");
            ScriptContext* scriptContext = instance->GetScriptContext();
            if (BuiltInPropertyRecords::valueOf.Equals(propertyName))
            {LOGMEIN("TypeHandler.cpp] 573\n");
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ValueOf & possibleSideEffects));
            }
            else if (BuiltInPropertyRecords::toString.Equals(propertyName))
            {LOGMEIN("TypeHandler.cpp] 577\n");
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ToString & possibleSideEffects));
            }
            else if (BuiltInPropertyRecords::Math.Equals(propertyName))
            {LOGMEIN("TypeHandler.cpp] 581\n");
                if (instance == scriptContext->GetLibrary()->GetGlobalObject())
                {LOGMEIN("TypeHandler.cpp] 583\n");
                    scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_MathFunc & possibleSideEffects));
                }
            }
            else if (instance == scriptContext->GetLibrary()->GetMathObject())
            {LOGMEIN("TypeHandler.cpp] 588\n");
                PropertyRecord const* propertyRecord;
                scriptContext->FindPropertyRecord(propertyName.GetBuffer(), propertyName.GetLength(), &propertyRecord);

                if (propertyRecord && IsMathLibraryId(propertyRecord->GetPropertyId()))
                {LOGMEIN("TypeHandler.cpp] 593\n");
                    scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_MathFunc & possibleSideEffects));
                }
            }

        }
    }

    bool DynamicTypeHandler::VerifyIsExtensible(ScriptContext* scriptContext, bool alwaysThrow)
    {LOGMEIN("TypeHandler.cpp] 602\n");
        if (!(this->GetFlags() & IsExtensibleFlag))
        {LOGMEIN("TypeHandler.cpp] 604\n");
            if (alwaysThrow)
            {LOGMEIN("TypeHandler.cpp] 606\n");
                if (scriptContext && scriptContext->GetThreadContext()->RecordImplicitException())
                {LOGMEIN("TypeHandler.cpp] 608\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NonExtensibleObject);
                }
            }
            return false;
        }

        return true;
    }

    void DynamicTypeHandler::EnsureSlots(DynamicObject* instance, int oldCount, int newCount, ScriptContext * scriptContext, DynamicTypeHandler * newTypeHandler)
    {LOGMEIN("TypeHandler.cpp] 619\n");
        Assert(oldCount == instance->GetTypeHandler()->GetSlotCapacity());
        AssertMsg(oldCount <= newCount, "Old count should be less than or equal to new count");

        if (oldCount < newCount && newCount > GetInlineSlotCapacity())
        {LOGMEIN("TypeHandler.cpp] 624\n");
            const PropertyIndex newInlineSlotCapacity = newTypeHandler->GetInlineSlotCapacity();
            Assert(newCount > newInlineSlotCapacity);
            AdjustSlots(instance, newInlineSlotCapacity, newCount - newInlineSlotCapacity);
        }
    }

    void DynamicTypeHandler::AdjustSlots_Jit(
        DynamicObject *const object,
        const PropertyIndex newInlineSlotCapacity,
        const int newAuxSlotCapacity)
    {LOGMEIN("TypeHandler.cpp] 635\n");
        Assert(object);

        // The JIT may call AdjustSlots multiple times on the same object, even after changing its type to the new type. Check
        // if anything needs to be done.
        DynamicTypeHandler *const oldTypeHandler = object->GetTypeHandler();
        const PropertyIndex oldInlineSlotCapacity = oldTypeHandler->GetInlineSlotCapacity();
        if(oldInlineSlotCapacity == newInlineSlotCapacity &&
            oldTypeHandler->GetSlotCapacity() - oldInlineSlotCapacity == newAuxSlotCapacity)
        {LOGMEIN("TypeHandler.cpp] 644\n");
            return;
        }

        AdjustSlots(object, newInlineSlotCapacity, newAuxSlotCapacity);
    }

    void DynamicTypeHandler::AdjustSlots(
        DynamicObject *const object,
        const PropertyIndex newInlineSlotCapacity,
        const int newAuxSlotCapacity)
    {LOGMEIN("TypeHandler.cpp] 655\n");
        Assert(object);

        // Allocate new aux slot array
        Recycler *const recycler = object->GetRecycler();
        TRACK_ALLOC_INFO(recycler, Var, Recycler, 0, newAuxSlotCapacity);
        Field(Var) *const newAuxSlots = reinterpret_cast<Field(Var) *>(
            recycler->AllocZero(newAuxSlotCapacity * sizeof(Field(Var))));

        DynamicTypeHandler *const oldTypeHandler = object->GetTypeHandler();
        const PropertyIndex oldInlineSlotCapacity = oldTypeHandler->GetInlineSlotCapacity();
        if(oldInlineSlotCapacity == newInlineSlotCapacity)
        {LOGMEIN("TypeHandler.cpp] 667\n");
            const int oldAuxSlotCapacity = oldTypeHandler->GetSlotCapacity() - oldInlineSlotCapacity;
            Assert(oldAuxSlotCapacity < newAuxSlotCapacity);
            if(oldAuxSlotCapacity > 0)
            {LOGMEIN("TypeHandler.cpp] 671\n");
                // Copy aux slots to the new array
                Field(Var) *const oldAuxSlots = object->auxSlots;
                Assert(oldAuxSlots);
                int i = 0;
                do
                {LOGMEIN("TypeHandler.cpp] 677\n");
                    newAuxSlots[i] = oldAuxSlots[i];
                } while(++i < oldAuxSlotCapacity);

            #ifdef EXPLICIT_FREE_SLOTS
                recycler->ExplicitFreeNonLeaf(oldAuxSlots, oldAuxSlotCapacity * sizeof(Var));
            #endif
            }

            object->auxSlots = newAuxSlots;
            return;
        }

        // An object header-inlined type handler is transitioning into one that is not. Some inline slots need to move, and
        // there are no old aux slots that need to be copied.
        Assert(oldTypeHandler->IsObjectHeaderInlinedTypeHandler());
        Assert(oldInlineSlotCapacity > newInlineSlotCapacity);
        Assert(oldInlineSlotCapacity - newInlineSlotCapacity == DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity());
        Assert(newAuxSlotCapacity >= DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity());

        // Move the last few inline slots into the aux slots
        if(PHASE_TRACE1(Js::ObjectHeaderInliningPhase))
        {LOGMEIN("TypeHandler.cpp] 699\n");
            Output::Print(_u("ObjectHeaderInlining: Moving inlined properties to aux slots.\n"));
            Output::Flush();
        }
        Var *const oldInlineSlots =
            reinterpret_cast<Var *>(
                reinterpret_cast<uintptr_t>(object) + DynamicTypeHandler::GetOffsetOfObjectHeaderInlineSlots());
        Assert(DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity() == 2);
        newAuxSlots[0] = oldInlineSlots[oldInlineSlotCapacity - 2];
        newAuxSlots[1] = oldInlineSlots[oldInlineSlotCapacity - 1];

        if(newInlineSlotCapacity > 0)
        {LOGMEIN("TypeHandler.cpp] 711\n");
            // Move the remaining inline slots such that none are object header-inlined. Copy backwards, as the two buffers may
            // overlap, with the new inline slot array starting beyond the start of the old inline slot array.
            if(PHASE_TRACE1(Js::ObjectHeaderInliningPhase))
            {LOGMEIN("TypeHandler.cpp] 715\n");
                Output::Print(_u("ObjectHeaderInlining: Moving inlined properties out of the object header.\n"));
                Output::Flush();
            }
            Field(Var) *const newInlineSlots = reinterpret_cast<Field(Var) *>(object + 1);
            PropertyIndex i = newInlineSlotCapacity;
            do
            {LOGMEIN("TypeHandler.cpp] 722\n");
                --i;
                newInlineSlots[i] = oldInlineSlots[i];
            } while(i > 0);
        }

        object->auxSlots = newAuxSlots;
        object->objectArray = nullptr;
    }

    bool DynamicTypeHandler::CanBeSingletonInstance(DynamicObject * instance)
    {LOGMEIN("TypeHandler.cpp] 733\n");
        return !ThreadContext::IsOnStack(instance);
    }

    BOOL DynamicTypeHandler::DeleteProperty(DynamicObject* instance, JavascriptString* propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("TypeHandler.cpp] 738\n");
        PropertyRecord const *propertyRecord = nullptr;
        if (!JavascriptOperators::CanShortcutOnUnknownPropertyName(instance))
        {LOGMEIN("TypeHandler.cpp] 741\n");
            instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        }
        else
        {
            instance->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);
        }

        if (propertyRecord == nullptr)
        {LOGMEIN("TypeHandler.cpp] 750\n");
            return TRUE;
        }

        return DeleteProperty(instance, propertyRecord->GetPropertyId(), flags);
    }

    PropertyId DynamicTypeHandler::TMapKey_GetPropertyId(ScriptContext* scriptContext, const PropertyId key)
    {LOGMEIN("TypeHandler.cpp] 758\n");
        return key;
    }

    PropertyId DynamicTypeHandler::TMapKey_GetPropertyId(ScriptContext* scriptContext, const PropertyRecord* key)
    {LOGMEIN("TypeHandler.cpp] 763\n");
        return key->GetPropertyId();
    }

    PropertyId DynamicTypeHandler::TMapKey_GetPropertyId(ScriptContext* scriptContext, JavascriptString* key)
    {LOGMEIN("TypeHandler.cpp] 768\n");
        return scriptContext->GetOrAddPropertyIdTracked(key->GetSz(), key->GetLength());
    }

#if ENABLE_TTD
    Js::BigPropertyIndex DynamicTypeHandler::GetPropertyIndex_EnumerateTTD(const Js::PropertyRecord* pRecord)
    {
        TTDAssert(false, "Should never be called.");

        return Js::Constants::NoBigSlot;
    }

    void DynamicTypeHandler::ExtractSnapHandler(TTD::NSSnapType::SnapHandler* handler, ThreadContext* threadContext, TTD::SlabAllocator& alloc) const
    {LOGMEIN("TypeHandler.cpp] 781\n");
        handler->HandlerId = TTD_CONVERT_TYPEINFO_TO_PTR_ID(this);

        handler->InlineSlotCapacity = this->inlineSlotCapacity;
        handler->TotalSlotCapacity = this->slotCapacity;

        handler->MaxPropertyIndex = 0;
        handler->PropertyInfoArray = nullptr;

        if(handler->TotalSlotCapacity != 0)
        {LOGMEIN("TypeHandler.cpp] 791\n");
            handler->PropertyInfoArray = alloc.SlabReserveArraySpace<TTD::NSSnapType::SnapHandlerPropertyEntry>(handler->TotalSlotCapacity);
            memset(handler->PropertyInfoArray, 0, handler->TotalSlotCapacity * sizeof(TTD::NSSnapType::SnapHandlerPropertyEntry));

            handler->MaxPropertyIndex = this->ExtractSlotInfo_TTD(handler->PropertyInfoArray, threadContext, alloc);
            TTDAssert(handler->MaxPropertyIndex <= handler->TotalSlotCapacity, "Huh we have more property entries than slots to put them in.");

            if(handler->MaxPropertyIndex != 0)
            {LOGMEIN("TypeHandler.cpp] 799\n");
                alloc.SlabCommitArraySpace<TTD::NSSnapType::SnapHandlerPropertyEntry>(handler->MaxPropertyIndex, handler->TotalSlotCapacity);
            }
            else
            {
                alloc.SlabAbortArraySpace<TTD::NSSnapType::SnapHandlerPropertyEntry>(handler->TotalSlotCapacity);
                handler->PropertyInfoArray = nullptr;
            }
        }

        //The kind of type this snaptype record is associated with and the extensible flag
        handler->IsExtensibleFlag = this->GetFlags() & Js::DynamicTypeHandler::IsExtensibleFlag;
    }

    void DynamicTypeHandler::SetExtensible_TTD()
    {LOGMEIN("TypeHandler.cpp] 814\n");
        this->flags |= Js::DynamicTypeHandler::IsExtensibleFlag;
    }

    bool DynamicTypeHandler::IsResetableForTTD(uint32 snapMaxIndex) const
    {LOGMEIN("TypeHandler.cpp] 819\n");
        return false;
    }
#endif
}
