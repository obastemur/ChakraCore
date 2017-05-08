//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    BigPropertyIndex
    DynamicTypeHandler::GetPropertyIndexFromInlineSlotIndex(uint inlineSlot)
    {TRACE_IT(67710);
        return inlineSlot - (offsetOfInlineSlots / sizeof(Var *));
    }

    BigPropertyIndex
    DynamicTypeHandler::GetPropertyIndexFromAuxSlotIndex(uint auxIndex)
    {TRACE_IT(67711);
        return auxIndex + this->GetInlineSlotCapacity();
    }

    PropertyIndex DynamicTypeHandler::RoundUpObjectHeaderInlinedInlineSlotCapacity(const PropertyIndex slotCapacity)
    {TRACE_IT(67712);
        const PropertyIndex objectHeaderInlinableSlotCapacity = GetObjectHeaderInlinableSlotCapacity();
        if(slotCapacity <= objectHeaderInlinableSlotCapacity)
        {TRACE_IT(67713);
            return objectHeaderInlinableSlotCapacity;
        }

        // Align the slot capacity for slots that are outside the object header, and add to that the slot capacity for slots
        // that are inside the object header
        return RoundUpInlineSlotCapacity(slotCapacity - objectHeaderInlinableSlotCapacity) + objectHeaderInlinableSlotCapacity;
    }

    PropertyIndex DynamicTypeHandler::RoundUpInlineSlotCapacity(const PropertyIndex slotCapacity)
    {TRACE_IT(67714);
        return ::Math::Align<PropertyIndex>(slotCapacity, HeapConstants::ObjectGranularity / sizeof(Var));
    }

    int DynamicTypeHandler::RoundUpAuxSlotCapacity(const int slotCapacity)
    {TRACE_IT(67715);
        CompileAssert(4 * sizeof(Var) % HeapConstants::ObjectGranularity == 0);
        return ::Math::Align<int>(slotCapacity, 4);
    }

    int DynamicTypeHandler::RoundUpSlotCapacity(const int slotCapacity, const PropertyIndex inlineSlotCapacity)
    {TRACE_IT(67716);
        Assert(slotCapacity >= 0);

        if(slotCapacity <= inlineSlotCapacity)
        {TRACE_IT(67717);
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
    {TRACE_IT(67718);
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
    {TRACE_IT(67719);
        if (index < inlineSlotCapacity)
        {TRACE_IT(67720);
            Var * slots = reinterpret_cast<Var*>(reinterpret_cast<size_t>(instance) + offsetOfInlineSlots);
            Var value = slots[index];
            Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
            return value;
        }
        else
        {TRACE_IT(67721);
            Var value = instance->auxSlots[index - inlineSlotCapacity];
            Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
            return value;
        }
    }

    Var DynamicTypeHandler::GetInlineSlot(DynamicObject * instance, int index)
    {TRACE_IT(67722);
        AssertMsg(index >= (int)(offsetOfInlineSlots / sizeof(Var)), "index should be relative to the address of the object");
        Assert(index - (int)(offsetOfInlineSlots / sizeof(Var)) < this->GetInlineSlotCapacity());
        Var * slots = reinterpret_cast<Var*>(instance);
        Var value = slots[index];
        Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
        return value;
    }

    Var DynamicTypeHandler::GetAuxSlot(DynamicObject * instance, int index)
    {TRACE_IT(67723);
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
    {TRACE_IT(67724);
        // We should only assign a stack value only to a stack object (current mark temp number in mark temp object)
        Assert(ThreadContext::IsOnStack(instance) || !ThreadContext::IsOnStack(value) || TaggedNumber::Is(value));
        uint16 inlineSlotCapacity = instance->GetTypeHandler()->GetInlineSlotCapacity();
        uint16 offsetOfInlineSlots = instance->GetTypeHandler()->GetOffsetOfInlineSlots();
        int slotCapacity = instance->GetTypeHandler()->GetSlotCapacity();

        if (index < inlineSlotCapacity)
        {TRACE_IT(67725);
            Field(Var) * slots = reinterpret_cast<Field(Var)*>(reinterpret_cast<size_t>(instance) + offsetOfInlineSlots);
            slots[index] = value;
        }
        else
        {TRACE_IT(67726);
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
    {TRACE_IT(67727);
        return offsetOfInlineSlots == GetOffsetOfObjectHeaderInlineSlots();
    }

    bool DynamicTypeHandler::IsObjectHeaderInlinedTypeHandlerUnchecked() const
    {TRACE_IT(67728);
        return IsObjectHeaderInlined(GetOffsetOfInlineSlots());
    }

    bool DynamicTypeHandler::IsObjectHeaderInlinedTypeHandler() const
    {TRACE_IT(67729);
        const bool isObjectHeaderInlined = IsObjectHeaderInlinedTypeHandlerUnchecked();
        if(isObjectHeaderInlined)
        {TRACE_IT(67730);
            VerifyObjectHeaderInlinedTypeHandler();
        }
        return isObjectHeaderInlined;
    }

    void DynamicTypeHandler::VerifyObjectHeaderInlinedTypeHandler() const
    {TRACE_IT(67731);
        Assert(IsObjectHeaderInlined(GetOffsetOfInlineSlots()));
        Assert(GetInlineSlotCapacity() >= GetObjectHeaderInlinableSlotCapacity());
        Assert(GetInlineSlotCapacity() == GetSlotCapacity());
    }

    uint16 DynamicTypeHandler::GetOffsetOfObjectHeaderInlineSlots()
    {TRACE_IT(67732);
        return offsetof(DynamicObject, auxSlots);
    }

    PropertyIndex DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity()
    {TRACE_IT(67733);
        const PropertyIndex maxAllowedSlotCapacity = (sizeof(DynamicObject) - DynamicTypeHandler::GetOffsetOfObjectHeaderInlineSlots()) / sizeof(Var);
        AssertMsg(maxAllowedSlotCapacity == 2, "Today we should be getting 2 with the math here. Change this Assert, if we are changing this logic in the future");
        return maxAllowedSlotCapacity;
    }

    void
        DynamicTypeHandler::SetInstanceTypeHandler(DynamicObject * instance, DynamicTypeHandler * typeHandler, bool hasChanged)
    {TRACE_IT(67734);
        instance->SetTypeHandler(typeHandler, hasChanged);
    }

    DynamicTypeHandler *
    DynamicTypeHandler::GetCurrentTypeHandler(DynamicObject * instance)
    {TRACE_IT(67735);
        return instance->GetTypeHandler();
    }

    void
    DynamicTypeHandler::ReplaceInstanceType(DynamicObject * instance, DynamicType * type)
    {TRACE_IT(67736);
        instance->ReplaceType(type);
    }

    void
    DynamicTypeHandler::ResetTypeHandler(DynamicObject * instance)
    {TRACE_IT(67737);
        // just reuse the current type handler.
        this->SetInstanceTypeHandler(instance);
    }

    BOOL
    DynamicTypeHandler::FindNextProperty(ScriptContext* scriptContext, BigPropertyIndex& index, JavascriptString** propertyString,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {TRACE_IT(67738);
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
    {TRACE_IT(67739);
        int count = GetPropertyCount();
        if (count < 128) // Invalidate a propertyId involves dictionary lookups. Only do this when the number is relatively small.
        {TRACE_IT(67740);
            for (int i = 0; i < count; i++)
            {TRACE_IT(67741);
                PropertyId propertyId = GetPropertyId(requestContext, static_cast<PropertyIndex>(i));
                if (propertyId != Constants::NoProperty)
                {TRACE_IT(67742);
                    isStoreField ? requestContext->InvalidateStoreFieldCaches(propertyId) : requestContext->InvalidateProtoCaches(propertyId);
                }
            }
        }
        else
        {TRACE_IT(67743);
            isStoreField ? requestContext->InvalidateAllStoreFieldCaches() : requestContext->InvalidateAllProtoCaches();
        }
    }

    void DynamicTypeHandler::InvalidateProtoCachesForAllProperties(ScriptContext* requestContext)
    {TRACE_IT(67744);
        InvalidateInlineCachesForAllProperties<false>(requestContext);
    }

    void DynamicTypeHandler::InvalidateStoreFieldCachesForAllProperties(ScriptContext* requestContext)
    {TRACE_IT(67745);
        InvalidateInlineCachesForAllProperties<true>(requestContext);
    }

    void DynamicTypeHandler::RemoveFromPrototype(DynamicObject* instance, ScriptContext * requestContext)
    {TRACE_IT(67746);
        InvalidateProtoCachesForAllProperties(requestContext);
    }

    void DynamicTypeHandler::AddToPrototype(DynamicObject* instance, ScriptContext * requestContext)
    {TRACE_IT(67747);
        InvalidateStoreFieldCachesForAllProperties(requestContext);
    }

    void DynamicTypeHandler::SetPrototype(DynamicObject* instance, RecyclableObject* newPrototype)
    {TRACE_IT(67748);
        // Force a type transition on the instance to invalidate its inline caches
        DynamicTypeHandler::ResetTypeHandler(instance);

        // Put new prototype in place
        instance->GetDynamicType()->SetPrototype(newPrototype);
    }

    bool DynamicTypeHandler::TryUseFixedProperty(PropertyRecord const* propertyRecord, Var * pProperty, FixedPropertyKind propertyType, ScriptContext * requestContext)
    {TRACE_IT(67749);
        if (PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase) ||
            PHASE_VERBOSE_TRACE1(Js::UseFixedDataPropsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::UseFixedDataPropsPhase))
        {TRACE_IT(67750);
            Output::Print(_u("FixedFields: attempt to use fixed property %s from DynamicTypeHandler returned false.\n"), propertyRecord->GetBuffer());
            if (this->HasSingletonInstance() && this->GetSingletonInstance()->Get()->GetScriptContext() != requestContext)
            {TRACE_IT(67751);
                Output::Print(_u("FixedFields: Cross Site Script Context is used for property %s. \n"), propertyRecord->GetBuffer());
            }
            Output::Flush();
        }
        return false;
    }

    bool DynamicTypeHandler::TryUseFixedAccessor(PropertyRecord const* propertyRecord, Var * pAccessor, FixedPropertyKind propertyType, bool getter, ScriptContext * requestContext)
    {TRACE_IT(67752);
        if (PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase) ||
            PHASE_VERBOSE_TRACE1(Js::UseFixedDataPropsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::UseFixedDataPropsPhase))
        {TRACE_IT(67753);
            Output::Print(_u("FixedFields: attempt to use fixed accessor %s from DynamicTypeHandler returned false.\n"), propertyRecord->GetBuffer());
            if (this->HasSingletonInstance() && this->GetSingletonInstance()->Get()->GetScriptContext() != requestContext)
            {TRACE_IT(67754);
                Output::Print(_u("FixedFields: Cross Site Script Context is used for property %s. \n"), propertyRecord->GetBuffer());
            }
            Output::Flush();
        }
        return false;
    }

    bool DynamicTypeHandler::IsFixedMethodProperty(FixedPropertyKind fixedPropKind)
    {TRACE_IT(67755);
        return (fixedPropKind & Js::FixedPropertyKind::FixedMethodProperty) == Js::FixedPropertyKind::FixedMethodProperty;
    }

    bool DynamicTypeHandler::IsFixedDataProperty(FixedPropertyKind fixedPropKind)
    {TRACE_IT(67756);
        return ((fixedPropKind & Js::FixedPropertyKind::FixedDataProperty) == Js::FixedPropertyKind::FixedDataProperty) &&
            !PHASE_OFF1(UseFixedDataPropsPhase);
    }

    bool DynamicTypeHandler::IsFixedAccessorProperty(FixedPropertyKind fixedPropKind)
    {TRACE_IT(67757);
        return (fixedPropKind & Js::FixedPropertyKind::FixedAccessorProperty) == Js::FixedPropertyKind::FixedAccessorProperty;
    }

    bool DynamicTypeHandler::CheckHeuristicsForFixedDataProps(DynamicObject* instance, const PropertyRecord * propertyRecord, Var value)
    {TRACE_IT(67758);
        if (PHASE_FORCE1(Js::FixDataPropsPhase))
        {TRACE_IT(67759);
            return true;
        }

        if (Js::TaggedInt::Is(value) &&
            ((instance->GetTypeId() == TypeIds_GlobalObject && instance->GetScriptContext()->IsIntConstPropertyOnGlobalObject(propertyRecord->GetPropertyId())) ||
            (instance->GetTypeId() == TypeIds_Object && instance->GetScriptContext()->IsIntConstPropertyOnGlobalUserObject(propertyRecord->GetPropertyId()))))
        {TRACE_IT(67760);
            return true;
        }

        // Disabled by default
        if (PHASE_ON1(Js::FixDataVarPropsPhase))
        {TRACE_IT(67761);
            if (instance->GetTypeHandler()->GetFlags() & IsPrototypeFlag)
            {TRACE_IT(67762);
                return true;
            }
            if (instance->GetType()->GetTypeId() == TypeIds_GlobalObject)
            {TRACE_IT(67763);
                // if we have statically seen multiple stores - we should not do this optimization
                RootObjectInlineCache* cache = (static_cast<Js::RootObjectBase*>(instance))->GetRootInlineCache(propertyRecord, /*isLoadMethod*/ false, /*isStore*/ true);
                uint refCount = cache->Release();
                return refCount <= 1;
            }
        }
        return false;
    }

    bool DynamicTypeHandler::CheckHeuristicsForFixedDataProps(DynamicObject* instance, PropertyId propertyId, Var value)
    {TRACE_IT(67764);
        return CheckHeuristicsForFixedDataProps(instance, instance->GetScriptContext()->GetPropertyName(propertyId), value);
    }

    bool DynamicTypeHandler::CheckHeuristicsForFixedDataProps(DynamicObject* instance, JavascriptString * propertyKey, Var value)
    {TRACE_IT(67765);
        return false;
    }

    bool DynamicTypeHandler::CheckHeuristicsForFixedDataProps(DynamicObject* instance, const PropertyRecord * propertyRecord, PropertyId propertyId, Var value)
    {TRACE_IT(67766);
        if(propertyRecord)
        {TRACE_IT(67767);
            return CheckHeuristicsForFixedDataProps(instance, propertyRecord, value);
        }
        else
        {TRACE_IT(67768);
            return CheckHeuristicsForFixedDataProps(instance,propertyId,value);
        }
    }

    void DynamicTypeHandler::TraceUseFixedProperty(PropertyRecord const * propertyRecord, Var * pProperty, bool result, LPCWSTR typeHandlerName, ScriptContext * requestContext)
    {TRACE_IT(67769);
        LPCWSTR fixedPropertyResultType = nullptr;
        bool log = false;

        if (pProperty && *pProperty && ((Js::JavascriptFunction::Is(*pProperty) && (PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))) ||
            ((PHASE_VERBOSE_TRACE1(Js::UseFixedDataPropsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::UseFixedDataPropsPhase))) ))
        {TRACE_IT(67770);
            if(*pProperty == nullptr)
            {TRACE_IT(67771);
                fixedPropertyResultType = _u("null");
            }
            else if (Js::JavascriptFunction::Is(*pProperty))
            {TRACE_IT(67772);
                fixedPropertyResultType = _u("function");
            }
            else if (TaggedInt::Is(*pProperty))
            {TRACE_IT(67773);
                fixedPropertyResultType = _u("int constant");
            }
            else
            {TRACE_IT(67774);
                fixedPropertyResultType = _u("Var");
            }
            log = true;
        }

        if(log)
        {TRACE_IT(67775);
            Output::Print(_u("FixedFields: attempt to use fixed property %s, which is a %s, from %s returned %s.\n"),
                propertyRecord->GetBuffer(), fixedPropertyResultType, typeHandlerName, IsTrueOrFalse(result));

            if (this->HasSingletonInstance() && this->GetSingletonInstance()->Get()->GetScriptContext() != requestContext)
            {TRACE_IT(67776);
                Output::Print(_u("FixedFields: Cross Site Script Context is used for property %s. \n"), propertyRecord->GetBuffer());
            }

            Output::Flush();
        }
    }

    BOOL DynamicTypeHandler::GetInternalProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value)
    {TRACE_IT(67777);
        // Type handlers that store internal properties differently from normal properties
        // override this method to provide access to them.  Otherwise, by default, simply
        // defer to GetProperty()
        return this->GetProperty(instance, originalInstance, propertyId, value, nullptr, nullptr);
    }

    BOOL DynamicTypeHandler::InitProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(67778);
        // By default just call the SetProperty method
        return this->SetProperty(instance, propertyId, value, flags, info);
    }

    BOOL DynamicTypeHandler::SetInternalProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags)
    {TRACE_IT(67779);
        // Type handlers that store internal properties differently from normal properties
        // override this method to provide access to them.  Otherwise, by default, simply
        // defer to SetProperty()
        return this->SetProperty(instance, propertyId, value, flags, nullptr);
    }

    //
    // Default implementations delegate to instance objectArray
    //
    BOOL DynamicTypeHandler::HasItem(DynamicObject* instance, uint32 index)
    {TRACE_IT(67780);
        return instance->HasObjectArrayItem(index);
    }
    BOOL DynamicTypeHandler::SetItem(DynamicObject* instance, uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(67781);
        return instance->SetObjectArrayItem(index, value, flags);
    }
    BOOL DynamicTypeHandler::DeleteItem(DynamicObject* instance, uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(67782);
        return instance->DeleteObjectArrayItem(index, flags);
    }
    BOOL DynamicTypeHandler::GetItem(DynamicObject* instance, Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(67783);
        return instance->GetObjectArrayItem(originalInstance, index, value, requestContext);
    }

    DescriptorFlags DynamicTypeHandler::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(67784);
        PropertyValueInfo::SetNoCache(info, instance);
        return this->HasProperty(instance, propertyId) ? WritableData : None;
    }

    DescriptorFlags DynamicTypeHandler::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(67785);
        PropertyValueInfo::SetNoCache(info, instance);
        return this->HasProperty(instance, propertyNameString) ? WritableData : None;
    }

    DescriptorFlags DynamicTypeHandler::GetItemSetter(DynamicObject* instance, uint32 index, Var* setterValue, ScriptContext* requestContext)
    {TRACE_IT(67786);
        return this->HasItem(instance, index) ? WritableData : None;
    }

    //
    // Default implementations upgrades type handler with item attribute/getter/setter support
    //
    BOOL DynamicTypeHandler::SetItemWithAttributes(DynamicObject* instance, uint32 index, Var value, PropertyAttributes attributes)
    {TRACE_IT(67787);
        return ConvertToTypeWithItemAttributes(instance)->SetItemWithAttributes(instance, index, value, attributes);
    }
    BOOL DynamicTypeHandler::SetItemAttributes(DynamicObject* instance, uint32 index, PropertyAttributes attributes)
    {TRACE_IT(67788);
        return ConvertToTypeWithItemAttributes(instance)->SetItemAttributes(instance, index, attributes);
    }
    BOOL DynamicTypeHandler::SetItemAccessors(DynamicObject* instance, uint32 index, Var getter, Var setter)
    {TRACE_IT(67789);
        return ConvertToTypeWithItemAttributes(instance)->SetItemAccessors(instance, index, getter, setter);
    }

    void DynamicTypeHandler::SetPropertyUpdateSideEffect(DynamicObject* instance, PropertyId propertyId, Var value, SideEffects possibleSideEffects)
    {TRACE_IT(67790);
        if (possibleSideEffects && propertyId < PropertyIds::_countJSOnlyProperty)
        {TRACE_IT(67791);
            ScriptContext* scriptContext = instance->GetScriptContext();

            if (scriptContext->GetConfig()->IsES6ToPrimitiveEnabled() && propertyId == PropertyIds::_symbolToPrimitive)
            {TRACE_IT(67792);
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ValueOf & possibleSideEffects));
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ToString & possibleSideEffects));
            }

            else if (propertyId == PropertyIds::valueOf)
            {TRACE_IT(67793);
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ValueOf & possibleSideEffects));
            }
            else if (propertyId == PropertyIds::toString)
            {TRACE_IT(67794);
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ToString & possibleSideEffects));
            }
            else if (propertyId == PropertyIds::Math)
            {TRACE_IT(67795);
                if (instance == scriptContext->GetLibrary()->GetGlobalObject())
                {TRACE_IT(67796);
                    scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_MathFunc & possibleSideEffects));
                }
            }
            else if (IsMathLibraryId(propertyId))
            {TRACE_IT(67797);
                if (instance == scriptContext->GetLibrary()->GetMathObject())
                {TRACE_IT(67798);
                    scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_MathFunc & possibleSideEffects));
                }
            }
        }
    }

    void DynamicTypeHandler::SetPropertyUpdateSideEffect(DynamicObject* instance, JsUtil::CharacterBuffer<WCHAR> const& propertyName, Var value, SideEffects possibleSideEffects)
    {TRACE_IT(67799);
        if (possibleSideEffects)
        {TRACE_IT(67800);
            ScriptContext* scriptContext = instance->GetScriptContext();
            if (BuiltInPropertyRecords::valueOf.Equals(propertyName))
            {TRACE_IT(67801);
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ValueOf & possibleSideEffects));
            }
            else if (BuiltInPropertyRecords::toString.Equals(propertyName))
            {TRACE_IT(67802);
                scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_ToString & possibleSideEffects));
            }
            else if (BuiltInPropertyRecords::Math.Equals(propertyName))
            {TRACE_IT(67803);
                if (instance == scriptContext->GetLibrary()->GetGlobalObject())
                {TRACE_IT(67804);
                    scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_MathFunc & possibleSideEffects));
                }
            }
            else if (instance == scriptContext->GetLibrary()->GetMathObject())
            {TRACE_IT(67805);
                PropertyRecord const* propertyRecord;
                scriptContext->FindPropertyRecord(propertyName.GetBuffer(), propertyName.GetLength(), &propertyRecord);

                if (propertyRecord && IsMathLibraryId(propertyRecord->GetPropertyId()))
                {TRACE_IT(67806);
                    scriptContext->optimizationOverrides.SetSideEffects((SideEffects)(SideEffects_MathFunc & possibleSideEffects));
                }
            }

        }
    }

    bool DynamicTypeHandler::VerifyIsExtensible(ScriptContext* scriptContext, bool alwaysThrow)
    {TRACE_IT(67807);
        if (!(this->GetFlags() & IsExtensibleFlag))
        {TRACE_IT(67808);
            if (alwaysThrow)
            {TRACE_IT(67809);
                if (scriptContext && scriptContext->GetThreadContext()->RecordImplicitException())
                {TRACE_IT(67810);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NonExtensibleObject);
                }
            }
            return false;
        }

        return true;
    }

    void DynamicTypeHandler::EnsureSlots(DynamicObject* instance, int oldCount, int newCount, ScriptContext * scriptContext, DynamicTypeHandler * newTypeHandler)
    {TRACE_IT(67811);
        Assert(oldCount == instance->GetTypeHandler()->GetSlotCapacity());
        AssertMsg(oldCount <= newCount, "Old count should be less than or equal to new count");

        if (oldCount < newCount && newCount > GetInlineSlotCapacity())
        {TRACE_IT(67812);
            const PropertyIndex newInlineSlotCapacity = newTypeHandler->GetInlineSlotCapacity();
            Assert(newCount > newInlineSlotCapacity);
            AdjustSlots(instance, newInlineSlotCapacity, newCount - newInlineSlotCapacity);
        }
    }

    void DynamicTypeHandler::AdjustSlots_Jit(
        DynamicObject *const object,
        const PropertyIndex newInlineSlotCapacity,
        const int newAuxSlotCapacity)
    {TRACE_IT(67813);
        Assert(object);

        // The JIT may call AdjustSlots multiple times on the same object, even after changing its type to the new type. Check
        // if anything needs to be done.
        DynamicTypeHandler *const oldTypeHandler = object->GetTypeHandler();
        const PropertyIndex oldInlineSlotCapacity = oldTypeHandler->GetInlineSlotCapacity();
        if(oldInlineSlotCapacity == newInlineSlotCapacity &&
            oldTypeHandler->GetSlotCapacity() - oldInlineSlotCapacity == newAuxSlotCapacity)
        {TRACE_IT(67814);
            return;
        }

        AdjustSlots(object, newInlineSlotCapacity, newAuxSlotCapacity);
    }

    void DynamicTypeHandler::AdjustSlots(
        DynamicObject *const object,
        const PropertyIndex newInlineSlotCapacity,
        const int newAuxSlotCapacity)
    {TRACE_IT(67815);
        Assert(object);

        // Allocate new aux slot array
        Recycler *const recycler = object->GetRecycler();
        TRACK_ALLOC_INFO(recycler, Var, Recycler, 0, newAuxSlotCapacity);
        Field(Var) *const newAuxSlots = reinterpret_cast<Field(Var) *>(
            recycler->AllocZero(newAuxSlotCapacity * sizeof(Field(Var))));

        DynamicTypeHandler *const oldTypeHandler = object->GetTypeHandler();
        const PropertyIndex oldInlineSlotCapacity = oldTypeHandler->GetInlineSlotCapacity();
        if(oldInlineSlotCapacity == newInlineSlotCapacity)
        {TRACE_IT(67816);
            const int oldAuxSlotCapacity = oldTypeHandler->GetSlotCapacity() - oldInlineSlotCapacity;
            Assert(oldAuxSlotCapacity < newAuxSlotCapacity);
            if(oldAuxSlotCapacity > 0)
            {TRACE_IT(67817);
                // Copy aux slots to the new array
                Field(Var) *const oldAuxSlots = object->auxSlots;
                Assert(oldAuxSlots);
                int i = 0;
                do
                {TRACE_IT(67818);
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
        {TRACE_IT(67819);
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
        {TRACE_IT(67820);
            // Move the remaining inline slots such that none are object header-inlined. Copy backwards, as the two buffers may
            // overlap, with the new inline slot array starting beyond the start of the old inline slot array.
            if(PHASE_TRACE1(Js::ObjectHeaderInliningPhase))
            {TRACE_IT(67821);
                Output::Print(_u("ObjectHeaderInlining: Moving inlined properties out of the object header.\n"));
                Output::Flush();
            }
            Field(Var) *const newInlineSlots = reinterpret_cast<Field(Var) *>(object + 1);
            PropertyIndex i = newInlineSlotCapacity;
            do
            {TRACE_IT(67822);
                --i;
                newInlineSlots[i] = oldInlineSlots[i];
            } while(i > 0);
        }

        object->auxSlots = newAuxSlots;
        object->objectArray = nullptr;
    }

    bool DynamicTypeHandler::CanBeSingletonInstance(DynamicObject * instance)
    {TRACE_IT(67823);
        return !ThreadContext::IsOnStack(instance);
    }

    BOOL DynamicTypeHandler::DeleteProperty(DynamicObject* instance, JavascriptString* propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(67824);
        PropertyRecord const *propertyRecord = nullptr;
        if (!JavascriptOperators::CanShortcutOnUnknownPropertyName(instance))
        {TRACE_IT(67825);
            instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        }
        else
        {TRACE_IT(67826);
            instance->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);
        }

        if (propertyRecord == nullptr)
        {TRACE_IT(67827);
            return TRUE;
        }

        return DeleteProperty(instance, propertyRecord->GetPropertyId(), flags);
    }

    PropertyId DynamicTypeHandler::TMapKey_GetPropertyId(ScriptContext* scriptContext, const PropertyId key)
    {TRACE_IT(67828);
        return key;
    }

    PropertyId DynamicTypeHandler::TMapKey_GetPropertyId(ScriptContext* scriptContext, const PropertyRecord* key)
    {TRACE_IT(67829);
        return key->GetPropertyId();
    }

    PropertyId DynamicTypeHandler::TMapKey_GetPropertyId(ScriptContext* scriptContext, JavascriptString* key)
    {TRACE_IT(67830);
        return scriptContext->GetOrAddPropertyIdTracked(key->GetSz(), key->GetLength());
    }

#if ENABLE_TTD
    Js::BigPropertyIndex DynamicTypeHandler::GetPropertyIndex_EnumerateTTD(const Js::PropertyRecord* pRecord)
    {
        TTDAssert(false, "Should never be called.");

        return Js::Constants::NoBigSlot;
    }

    void DynamicTypeHandler::ExtractSnapHandler(TTD::NSSnapType::SnapHandler* handler, ThreadContext* threadContext, TTD::SlabAllocator& alloc) const
    {TRACE_IT(67831);
        handler->HandlerId = TTD_CONVERT_TYPEINFO_TO_PTR_ID(this);

        handler->InlineSlotCapacity = this->inlineSlotCapacity;
        handler->TotalSlotCapacity = this->slotCapacity;

        handler->MaxPropertyIndex = 0;
        handler->PropertyInfoArray = nullptr;

        if(handler->TotalSlotCapacity != 0)
        {TRACE_IT(67832);
            handler->PropertyInfoArray = alloc.SlabReserveArraySpace<TTD::NSSnapType::SnapHandlerPropertyEntry>(handler->TotalSlotCapacity);
            memset(handler->PropertyInfoArray, 0, handler->TotalSlotCapacity * sizeof(TTD::NSSnapType::SnapHandlerPropertyEntry));

            handler->MaxPropertyIndex = this->ExtractSlotInfo_TTD(handler->PropertyInfoArray, threadContext, alloc);
            TTDAssert(handler->MaxPropertyIndex <= handler->TotalSlotCapacity, "Huh we have more property entries than slots to put them in.");

            if(handler->MaxPropertyIndex != 0)
            {TRACE_IT(67833);
                alloc.SlabCommitArraySpace<TTD::NSSnapType::SnapHandlerPropertyEntry>(handler->MaxPropertyIndex, handler->TotalSlotCapacity);
            }
            else
            {TRACE_IT(67834);
                alloc.SlabAbortArraySpace<TTD::NSSnapType::SnapHandlerPropertyEntry>(handler->TotalSlotCapacity);
                handler->PropertyInfoArray = nullptr;
            }
        }

        //The kind of type this snaptype record is associated with and the extensible flag
        handler->IsExtensibleFlag = this->GetFlags() & Js::DynamicTypeHandler::IsExtensibleFlag;
    }

    void DynamicTypeHandler::SetExtensible_TTD()
    {TRACE_IT(67835);
        this->flags |= Js::DynamicTypeHandler::IsExtensibleFlag;
    }

    bool DynamicTypeHandler::IsResetableForTTD(uint32 snapMaxIndex) const
    {TRACE_IT(67836);
        return false;
    }
#endif
}
