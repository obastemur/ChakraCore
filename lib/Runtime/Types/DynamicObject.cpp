//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(DynamicObject);
    DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(DynamicObject);

    DynamicObject::DynamicObject(DynamicType * type, const bool initSlots) :
        RecyclableObject(type),
        auxSlots(nullptr),
        objectArray(nullptr)
    {TRACE_IT(65833);
        Assert(!UsesObjectArrayOrFlagsAsFlags());
        if(initSlots)
        {TRACE_IT(65834);
            InitSlots(this);
        }
        else
        {TRACE_IT(65835);
            Assert(type->GetTypeHandler()->GetInlineSlotCapacity() == type->GetTypeHandler()->GetSlotCapacity());
        }

#if ENABLE_OBJECT_SOURCE_TRACKING
        TTD::InitializeDiagnosticOriginInformation(this->TTDDiagOriginInfo);
#endif
    }

    DynamicObject::DynamicObject(DynamicType * type, ScriptContext * scriptContext) :
#if DBG || defined(PROFILE_TYPES)
        RecyclableObject(type, scriptContext),
#else
        RecyclableObject(type),
#endif
        auxSlots(nullptr),
        objectArray(nullptr)
    {
        Assert(!UsesObjectArrayOrFlagsAsFlags());
        InitSlots(this, scriptContext);

#if ENABLE_OBJECT_SOURCE_TRACKING
        TTD::InitializeDiagnosticOriginInformation(this->TTDDiagOriginInfo);
#endif
    }

    DynamicObject::DynamicObject(DynamicObject * instance) :
        RecyclableObject(instance->type),
        auxSlots(instance->auxSlots),
        objectArray(instance->objectArray)  // copying the array should copy the array flags and array call site index as well
    {TRACE_IT(65836);
        DynamicTypeHandler * typeHandler = this->GetTypeHandler();

        // TODO: stack allocate aux Slots
        Assert(typeHandler->IsObjectHeaderInlinedTypeHandler() || !ThreadContext::IsOnStack(this->auxSlots));
        int propertyCount = typeHandler->GetPropertyCount();
        int inlineSlotCapacity = GetTypeHandler()->GetInlineSlotCapacity();
        int inlineSlotCount = min(inlineSlotCapacity, propertyCount);
        Var * srcSlots = reinterpret_cast<Var*>(reinterpret_cast<size_t>(instance) + typeHandler->GetOffsetOfInlineSlots());
        Field(Var) * dstSlots = reinterpret_cast<Field(Var)*>(reinterpret_cast<size_t>(this) + typeHandler->GetOffsetOfInlineSlots());
#if !FLOATVAR
        ScriptContext * scriptContext = this->GetScriptContext();
#endif
        for (int i = 0; i < inlineSlotCount; i++)
        {TRACE_IT(65837);
#if !FLOATVAR
            // Currently we only support temp numbers assigned to stack objects
            dstSlots[i] = JavascriptNumber::BoxStackNumber(srcSlots[i], scriptContext);
#else
            dstSlots[i] = srcSlots[i];
#endif

        }

        if (propertyCount > inlineSlotCapacity)
        {TRACE_IT(65838);
            uint auxSlotCount = propertyCount - inlineSlotCapacity;

            for (uint i = 0; i < auxSlotCount; i++)
            {TRACE_IT(65839);
#if !FLOATVAR
                // Currently we only support temp numbers assigned to stack objects
                auxSlots[i] = JavascriptNumber::BoxStackNumber(instance->auxSlots[i], scriptContext);
#else
                auxSlots[i] = instance->auxSlots[i];
#endif
            }
        }

#if ENABLE_OBJECT_SOURCE_TRACKING
        TTD::InitializeDiagnosticOriginInformation(this->TTDDiagOriginInfo);
#endif
    }

    DynamicObject * DynamicObject::New(Recycler * recycler, DynamicType * type)
    {TRACE_IT(65840);
        return NewObject<DynamicObject>(recycler, type);
    }

    bool DynamicObject::Is(Var aValue)
    {TRACE_IT(65841);
        return RecyclableObject::Is(aValue) && (RecyclableObject::FromVar(aValue)->GetTypeId() == TypeIds_Object);
    }

    DynamicObject* DynamicObject::FromVar(Var aValue)
    {TRACE_IT(65842);
        RecyclableObject* obj = RecyclableObject::FromVar(aValue);
        AssertMsg(obj->DbgIsDynamicObject(), "Ensure instance is actually a DynamicObject");
        Assert(DynamicType::Is(obj->GetTypeId()));
        return static_cast<DynamicObject*>(obj);
    }

    ArrayObject* DynamicObject::EnsureObjectArray()
    {TRACE_IT(65843);
        if (!HasObjectArray())
        {TRACE_IT(65844);
            ScriptContext* scriptContext = GetScriptContext();
            ArrayObject* objArray = scriptContext->GetLibrary()->CreateArray(0, SparseArraySegmentBase::SMALL_CHUNK_SIZE);
            SetObjectArray(objArray);
        }
        Assert(HasObjectArray());
        return GetObjectArrayOrFlagsAsArray();
    }

    void DynamicObject::SetObjectArray(ArrayObject* objArray)
    {TRACE_IT(65845);
        Assert(!IsAnyArray(this));

        DeoptimizeObjectHeaderInlining();

        this->objectArray = objArray;
        if (objArray)
        {TRACE_IT(65846);
            if (!this->IsExtensible()) // sync objectArray isExtensible
            {TRACE_IT(65847);
                objArray->PreventExtensions();
            }

            // sync objectArray is prototype
            if ((this->GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag) != 0)
            {TRACE_IT(65848);
                objArray->SetIsPrototype();
            }
        }
    }

    bool DynamicObject::HasNonEmptyObjectArray() const
    {TRACE_IT(65849);
        return HasObjectArray() && GetObjectArrayOrFlagsAsArray()->GetLength() > 0;
    }

    // Check if a typeId is of any array type (JavascriptArray or ES5Array).
    bool DynamicObject::IsAnyArrayTypeId(TypeId typeId)
    {TRACE_IT(65850);
        return JavascriptArray::Is(typeId) || typeId == TypeIds_ES5Array;
    }

    // Check if a Var is either a JavascriptArray* or ES5Array*.
    bool DynamicObject::IsAnyArray(const Var aValue)
    {TRACE_IT(65851);
        return IsAnyArrayTypeId(JavascriptOperators::GetTypeId(aValue));
    }

    BOOL DynamicObject::HasObjectArrayItem(uint32 index)
    {TRACE_IT(65852);
        return HasObjectArray() && GetObjectArrayOrFlagsAsArray()->HasItem(index);
    }

    BOOL DynamicObject::DeleteObjectArrayItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(65853);
        if (HasObjectArray())
        {TRACE_IT(65854);
            return GetObjectArrayOrFlagsAsArray()->DeleteItem(index, flags);
        }
        return true;
    }

    BOOL DynamicObject::GetObjectArrayItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(65855);
        *value = requestContext->GetMissingItemResult();
        return HasObjectArray() && GetObjectArrayOrFlagsAsArray()->GetItem(originalInstance, index, value, requestContext);
    }

    DescriptorFlags DynamicObject::GetObjectArrayItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {TRACE_IT(65856);
        return HasObjectArray() ? GetObjectArrayOrFlagsAsArray()->GetItemSetter(index, setterValue, requestContext) : None;
    }

    BOOL DynamicObject::SetObjectArrayItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(65857);
        const auto result = EnsureObjectArray()->SetItem(index, value, flags);

        // We don't track non-enumerable items in object arrays.  Any object with an object array reports having
        // enumerable properties.  See comment in DynamicObject::GetHasNoEnumerableProperties.
        //SetHasNoEnumerableProperties(false);

        return result;
    }

    BOOL DynamicObject::SetObjectArrayItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {TRACE_IT(65858);
        const auto result = EnsureObjectArray()->SetItemWithAttributes(index, value, attributes);

        // We don't track non-enumerable items in object arrays.  Any object with an object array reports having
        // enumerable properties.  See comment in DynamicObject::GetHasNoEnumerableProperties.
        //if (attributes & PropertyEnumerable)
        //{
        //    SetHasNoEnumerableProperties(false);
        //}

        if (!(attributes & PropertyWritable) && result)
        {TRACE_IT(65859);
            InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        }
        return result;
    }

    BOOL DynamicObject::SetObjectArrayItemAttributes(uint32 index, PropertyAttributes attributes)
    {TRACE_IT(65860);
        const auto result = HasObjectArray() && GetObjectArrayOrFlagsAsArray()->SetItemAttributes(index, attributes);

        // We don't track non-enumerable items in object arrays.  Any object with an object array reports having
        // enumerable properties.  See comment in DynamicObject::GetHasNoEnumerableProperties.
        //if (attributes & PropertyEnumerable)
        //{
        //    SetHasNoEnumerableProperties(false);
        //}

        if (!(attributes & PropertyWritable) && result)
        {TRACE_IT(65861);
            InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        }
        return result;
    }

    BOOL DynamicObject::SetObjectArrayItemWritable(PropertyId propertyId, BOOL writable)
    {TRACE_IT(65862);
        const auto result = HasObjectArray() && GetObjectArrayOrFlagsAsArray()->SetWritable(propertyId, writable);
        if (!writable && result)
        {TRACE_IT(65863);
            InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        }
        return result;
    }

    BOOL DynamicObject::SetObjectArrayItemAccessors(uint32 index, Var getter, Var setter)
    {TRACE_IT(65864);
        const auto result = EnsureObjectArray()->SetItemAccessors(index, getter, setter);
        if (result)
        {TRACE_IT(65865);
            InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        }
        return result;
    }

    void DynamicObject::InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype()
    {TRACE_IT(65866);
        if (GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {TRACE_IT(65867);
            // No need to invalidate store field caches for non-writable properties here.  We're dealing
            // with numeric properties only, and we never cache these in add property inline caches.

            // If this object is used as a prototype, the has-only-writable-data-properties-in-prototype-chain cache needs to be
            // invalidated here since the type handler of 'objectArray' is not marked as being used as a prototype
            GetType()->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
    }

    bool DynamicObject::HasLockedType() const
    {TRACE_IT(65868);
        return this->GetDynamicType()->GetIsLocked();
    }

    bool DynamicObject::HasSharedType() const
    {TRACE_IT(65869);
        return this->GetDynamicType()->GetIsShared();
    }

    bool DynamicObject::HasSharedTypeHandler() const
    {TRACE_IT(65870);
        return this->GetTypeHandler()->GetIsShared();
    }

    void DynamicObject::ReplaceType(DynamicType * type)
    {TRACE_IT(65871);
        Assert(!type->isLocked || type->GetTypeHandler()->GetIsLocked());
        Assert(!type->isShared || type->GetTypeHandler()->GetIsShared());

        // For now, i have added only Aux Slot -> so new inlineSlotCapacity should be 2.
        AssertMsg(DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining(this->GetTypeHandler(), type->GetTypeHandler()),
            "Object is ObjectHeaderInlined and should have compatible TypeHandlers for proper transition");

        this->type = type;
    }

    void DynamicObject::ReplaceTypeWithPredecessorType(DynamicType * predecessorType)
    {TRACE_IT(65872);
        Assert(this->GetTypeHandler()->IsPathTypeHandler());
        Assert(((PathTypeHandlerBase*)this->GetTypeHandler())->GetPredecessorType()->GetTypeHandler()->IsPathTypeHandler());

        Assert(((PathTypeHandlerBase*)this->GetTypeHandler())->GetPredecessorType() == predecessorType);

        Assert(!predecessorType->GetIsLocked() || predecessorType->GetTypeHandler()->GetIsLocked());
        Assert(!predecessorType->GetIsShared() || predecessorType->GetTypeHandler()->GetIsShared());

        Assert(this->GetType()->GetPrototype() == predecessorType->GetPrototype());

        PathTypeHandlerBase* currentPathTypeHandler = (PathTypeHandlerBase*)this->GetTypeHandler();
        PathTypeHandlerBase* predecessorPathTypeHandler = (PathTypeHandlerBase*)predecessorType->GetTypeHandler();

        Assert(predecessorPathTypeHandler->GetInlineSlotCapacity() >= currentPathTypeHandler->GetInlineSlotCapacity());

        this->type = predecessorType;
    }

    DWORD DynamicObject::GetOffsetOfAuxSlots()
    {TRACE_IT(65873);
        return offsetof(DynamicObject, auxSlots);
    }

    DWORD DynamicObject::GetOffsetOfObjectArray()
    {TRACE_IT(65874);
        return offsetof(DynamicObject, objectArray);
    }

    DWORD DynamicObject::GetOffsetOfType()
    {TRACE_IT(65875);
        return offsetof(DynamicObject, type);
    }

    void DynamicObject::EnsureSlots(int oldCount, int newCount, ScriptContext * scriptContext, DynamicTypeHandler * newTypeHandler)
    {TRACE_IT(65876);
        this->GetTypeHandler()->EnsureSlots(this, oldCount, newCount, scriptContext, newTypeHandler);
    }

    void DynamicObject::EnsureSlots(int newCount, ScriptContext * scriptContext)
    {TRACE_IT(65877);
        EnsureSlots(GetTypeHandler()->GetSlotCapacity(), newCount, scriptContext);
    }

    Var DynamicObject::GetSlot(int index)
    {TRACE_IT(65878);
        return this->GetTypeHandler()->GetSlot(this, index);
    }

    Var DynamicObject::GetInlineSlot(int index)
    {TRACE_IT(65879);
        return this->GetTypeHandler()->GetInlineSlot(this, index);
    }

    Var DynamicObject::GetAuxSlot(int index)
    {TRACE_IT(65880);
        return this->GetTypeHandler()->GetAuxSlot(this, index);
    }

#if DBG
    void DynamicObject::SetSlot(PropertyId propertyId, bool allowLetConst, int index, Var value)
    {TRACE_IT(65881);
        this->GetTypeHandler()->SetSlot(this, propertyId, allowLetConst, index, value);
    }

    void DynamicObject::SetInlineSlot(PropertyId propertyId, bool allowLetConst, int index, Var value)
    {TRACE_IT(65882);
        this->GetTypeHandler()->SetInlineSlot(this, propertyId, allowLetConst, index, value);
    }

    void DynamicObject::SetAuxSlot(PropertyId propertyId, bool allowLetConst, int index, Var value)
    {TRACE_IT(65883);
        this->GetTypeHandler()->SetAuxSlot(this, propertyId, allowLetConst, index, value);
    }
#else
    void DynamicObject::SetSlot(int index, Var value)
    {TRACE_IT(65884);
        this->GetTypeHandler()->SetSlot(this, index, value);
    }

    void DynamicObject::SetInlineSlot(int index, Var value)
    {TRACE_IT(65885);
        this->GetTypeHandler()->SetInlineSlot(this, index, value);
    }

    void DynamicObject::SetAuxSlot(int index, Var value)
    {TRACE_IT(65886);
        this->GetTypeHandler()->SetAuxSlot(this, index, value);
    }
#endif

    bool
    DynamicObject::GetIsExtensible() const
    {TRACE_IT(65887);
        return this->GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsExtensibleFlag;
    }

    BOOL
    DynamicObject::FindNextProperty(BigPropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId, PropertyAttributes* attributes,
        DynamicType *typeToEnumerate, EnumeratorFlags flags, ScriptContext * requestContext) const
    {TRACE_IT(65888);
        if(index == Constants::NoBigSlot)
        {TRACE_IT(65889);
            return FALSE;
        }

#if ENABLE_TTD
        if(this->GetScriptContext()->ShouldPerformReplayAction())
        {TRACE_IT(65890);
            BOOL res = FALSE;
            PropertyAttributes tmpAttributes = PropertyNone;
            this->GetScriptContext()->GetThreadContext()->TTDLog->ReplayPropertyEnumEvent(requestContext, &res, &index, this, propertyId, &tmpAttributes, propertyString);

            if(attributes != nullptr)
            {TRACE_IT(65891);
                *attributes = tmpAttributes;
            }

            return res;
        }
        else if(this->GetScriptContext()->ShouldPerformRecordAction())
        {TRACE_IT(65892);
            BOOL res = this->GetTypeHandler()->FindNextProperty(requestContext, index, propertyString, propertyId, attributes, this->GetType(), typeToEnumerate, flags);

            PropertyAttributes tmpAttributes = (attributes != nullptr) ? *attributes : PropertyNone;
            this->GetScriptContext()->GetThreadContext()->TTDLog->RecordPropertyEnumEvent(res, *propertyId, tmpAttributes, *propertyString);
            return res;
        }
        else
        {TRACE_IT(65893);
            return this->GetTypeHandler()->FindNextProperty(requestContext, index, propertyString, propertyId, attributes, this->GetType(), typeToEnumerate, flags);
        }
#else
        return this->GetTypeHandler()->FindNextProperty(requestContext, index, propertyString, propertyId, attributes, this->GetType(), typeToEnumerate, flags);
#endif
    }

    BOOL
    DynamicObject::HasDeferredTypeHandler() const
    {TRACE_IT(65894);
        return this->GetTypeHandler()->IsDeferredTypeHandler();
    }

    DynamicTypeHandler *
    DynamicObject::GetTypeHandler() const
    {
        return this->GetDynamicType()->GetTypeHandler();
    }

    uint16 DynamicObject::GetOffsetOfInlineSlots() const
    {TRACE_IT(65896);
        return this->GetDynamicType()->GetTypeHandler()->GetOffsetOfInlineSlots();
    }

    void
    DynamicObject::SetTypeHandler(DynamicTypeHandler * typeHandler, bool hasChanged)
    {TRACE_IT(65897);
        if (hasChanged && this->HasLockedType())
        {TRACE_IT(65898);
            this->ChangeType();
        }
        this->GetDynamicType()->typeHandler = typeHandler;
    }

    DynamicType* DynamicObject::DuplicateType()
    {TRACE_IT(65899);
        return RecyclerNew(GetRecycler(), DynamicType, this->GetDynamicType());
    }

    /*
    *   DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining
    *   -   Checks if the TypeHandlers are compatible for transition from oldTypeHandler to newTypeHandler
    */
    bool DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining(DynamicTypeHandler * oldTypeHandler, DynamicTypeHandler * newTypeHandler)
    {TRACE_IT(65900);
        Assert(oldTypeHandler);
        Assert(newTypeHandler);

        return
            oldTypeHandler->GetInlineSlotCapacity() == newTypeHandler->GetInlineSlotCapacity() ||
            (
                oldTypeHandler->IsObjectHeaderInlinedTypeHandler() &&
                newTypeHandler->GetInlineSlotCapacity() ==
                    oldTypeHandler->GetInlineSlotCapacity() - DynamicTypeHandler::GetObjectHeaderInlinableSlotCapacity()
            );
    }

    bool DynamicObject::IsObjectHeaderInlinedTypeHandlerUnchecked() const
    {TRACE_IT(65901);
        return this->GetTypeHandler()->IsObjectHeaderInlinedTypeHandlerUnchecked();
    }

    bool DynamicObject::IsObjectHeaderInlinedTypeHandler() const
    {TRACE_IT(65902);
        return this->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler();
    }

    bool DynamicObject::DeoptimizeObjectHeaderInlining()
    {TRACE_IT(65903);
        if(!IsObjectHeaderInlinedTypeHandler())
        {TRACE_IT(65904);
            return false;
        }

        if (PHASE_TRACE1(Js::ObjectHeaderInliningPhase))
        {TRACE_IT(65905);
            Output::Print(_u("ObjectHeaderInlining: De-optimizing the object.\n"));
            Output::Flush();
        }

        PathTypeHandlerBase *const oldTypeHandler = PathTypeHandlerBase::FromTypeHandler(GetTypeHandler());
        SimplePathTypeHandler *const newTypeHandler = oldTypeHandler->DeoptimizeObjectHeaderInlining(GetLibrary());

        const PropertyIndex newInlineSlotCapacity = newTypeHandler->GetInlineSlotCapacity();
        DynamicTypeHandler::AdjustSlots(
            this,
            newInlineSlotCapacity,
            newTypeHandler->GetSlotCapacity() - newInlineSlotCapacity);

        DynamicType *const newType = DuplicateType();
        newType->typeHandler = newTypeHandler;
        newType->ShareType();
        type = newType;
        return true;
    }

    void DynamicObject::ChangeType()
    {TRACE_IT(65906);
        Assert(!GetDynamicType()->GetIsShared() || GetTypeHandler()->GetIsShared());
        this->type = this->DuplicateType();
    }

    void DynamicObject::ChangeTypeIf(const Type* oldType)
    {TRACE_IT(65907);
        if (this->type == oldType)
        {TRACE_IT(65908);
            ChangeType();
        }
    }

    DynamicObjectFlags DynamicObject::GetArrayFlags() const
    {
        Assert(IsAnyArray(const_cast<DynamicObject *>(this)));
        Assert(UsesObjectArrayOrFlagsAsFlags()); // an array object never has another internal array
        return arrayFlags & DynamicObjectFlags::AllArrayFlags;
    }

    DynamicObjectFlags DynamicObject::GetArrayFlags_Unchecked() const // do not use except in extreme circumstances
    {
        return arrayFlags & DynamicObjectFlags::AllArrayFlags;
    }

    void DynamicObject::InitArrayFlags(const DynamicObjectFlags flags)
    {TRACE_IT(65911);
        Assert(IsAnyArray(this));
        Assert(this->objectArray == nullptr);
        Assert((flags & DynamicObjectFlags::ObjectArrayFlagsTag) == DynamicObjectFlags::ObjectArrayFlagsTag);
        Assert((flags & ~DynamicObjectFlags::AllFlags) == DynamicObjectFlags::None);
        this->arrayFlags = flags;
    }

    void DynamicObject::SetArrayFlags(const DynamicObjectFlags flags)
    {TRACE_IT(65912);
        Assert(IsAnyArray(this));
        Assert(UsesObjectArrayOrFlagsAsFlags()); // an array object never has another internal array
        // Make sure we don't attempt to set any flags outside of the range of array flags.
        Assert((arrayFlags & ~DynamicObjectFlags::AllArrayFlags) == DynamicObjectFlags::ObjectArrayFlagsTag);
        Assert((flags & ~DynamicObjectFlags::AllArrayFlags) == DynamicObjectFlags::None);
        arrayFlags = flags | DynamicObjectFlags::ObjectArrayFlagsTag;
    }

    ProfileId DynamicObject::GetArrayCallSiteIndex() const
    {TRACE_IT(65913);
        Assert(IsAnyArray(const_cast<DynamicObject *>(this)));
        return arrayCallSiteIndex;
    }

    void DynamicObject::SetArrayCallSiteIndex(ProfileId profileId)
    {TRACE_IT(65914);
        Assert(IsAnyArray(this));
        arrayCallSiteIndex = profileId;
    }

    void DynamicObject::SetIsPrototype()
    {TRACE_IT(65915);
        DynamicTypeHandler* currentTypeHandler = this->GetTypeHandler();
        Js::DynamicType* oldType = this->GetDynamicType();

#if DBG
        bool wasShared = currentTypeHandler->GetIsShared();
        bool wasPrototype = (currentTypeHandler->GetFlags() & DynamicTypeHandler::IsPrototypeFlag) != 0;
        Assert(!DynamicTypeHandler::IsolatePrototypes() || !currentTypeHandler->RespectsIsolatePrototypes() || !currentTypeHandler->GetIsOrMayBecomeShared() || !wasPrototype);
#endif

        // If this handler is not shared and it already has a prototype flag then we must have taken the required
        // type transition (if any) earlier when the singleton object first became a prototype.
        if ((currentTypeHandler->GetFlags() & (DynamicTypeHandler::IsSharedFlag | DynamicTypeHandler::IsPrototypeFlag)) == DynamicTypeHandler::IsPrototypeFlag)
        {TRACE_IT(65916);
            Assert(this->GetObjectArray() == nullptr || (this->GetObjectArray()->GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag) != 0);
            return;
        }

        currentTypeHandler->SetIsPrototype(this);
        // Get type handler again, in case it got changed by SetIsPrototype.
        currentTypeHandler = this->GetTypeHandler();

        // Set the object array as a prototype as well, so if it is an ES5 array, we will disable the array set element fast path
        ArrayObject * objectArray = this->GetObjectArray();
        if (objectArray)
        {TRACE_IT(65917);
            objectArray->SetIsPrototype();
        }

#if DBG
        Assert(currentTypeHandler->SupportsPrototypeInstances());
        Assert(!DynamicTypeHandler::IsolatePrototypes() || !currentTypeHandler->RespectsIsolatePrototypes() || !currentTypeHandler->GetIsOrMayBecomeShared());
        Assert((wasPrototype && !wasShared) || !DynamicTypeHandler::ChangeTypeOnProto() || !currentTypeHandler->RespectsChangeTypeOnProto() || this->GetDynamicType() != oldType);
#endif

        // If we haven't changed type we must explicitly invalidate store field inline caches to avoid properties
        // getting added to this prototype object on the fast path without proper invalidation.
        if (this->GetDynamicType() == oldType)
        {TRACE_IT(65918);
            currentTypeHandler->InvalidateStoreFieldCachesForAllProperties(this->GetScriptContext());
        }
    }

    bool
    DynamicObject::LockType()
    {TRACE_IT(65919);
        return this->GetDynamicType()->LockType();
    }

    bool
    DynamicObject::ShareType()
    {TRACE_IT(65920);
        return this->GetDynamicType()->ShareType();
    }

    void
    DynamicObject::ResetObject(DynamicType* newType, BOOL keepProperties)
    {TRACE_IT(65921);
        Assert(newType != NULL);
        Assert(!keepProperties || (!newType->GetTypeHandler()->IsDeferredTypeHandler() && newType->GetTypeHandler()->GetPropertyCount() == 0));

        // This is what's going on here.  The newType comes from the (potentially) new script context, but the object is
        // described by the old type handler, so we want to keep that type handler.  We set the new type on the object, but
        // then re-set the type handler of that type back to the old type handler.  In the process, we may actually change
        // the type of the object again (if the new type was locked) via DuplicateType; the newer type will then also be
        // from the new script context.
        DynamicType * oldType = this->GetDynamicType();
        DynamicTypeHandler* oldTypeHandler = oldType->GetTypeHandler();

        // Consider: Because we've disabled fixed properties on DOM objects, we don't need to rely on a type change here to
        // invalidate fixed properties.  Under some circumstances (with F12 tools enabled) an object which
        // is already in the new context can be reset and newType == oldType. If we re-enable fixed properties on DOM objects
        // we'll have to investigate and address this issue.
        // Assert(newType != oldType);
        // We only expect DOM objects to ever be reset and we explicitly disable fixed properties on DOM objects.
        Assert(!oldTypeHandler->HasAnyFixedProperties());

        this->type = newType;
        if (!IsAnyArray(this))
        {TRACE_IT(65922);
            this->objectArray = nullptr;
        }
        oldTypeHandler->ResetTypeHandler(this);
        Assert(this->GetScriptContext() == newType->GetScriptContext());

        if (this->GetTypeHandler()->IsDeferredTypeHandler())
        {TRACE_IT(65923);
            return;
        }

        if (!keepProperties)
        {TRACE_IT(65924);
            this->GetTypeHandler()->SetAllPropertiesToUndefined(this, false);
        }

        // Marshalling cannot handle non-Var values, so extract
        // the two internal property values that could appear on a CEO, clear them to null which
        // marshalling does handle, and then restore them after marshalling.  Neither property's
        // data needs marshalling because:
        //  1. StackTrace's data does not contain references to JavaScript objects that would need marshalling.
        //  2. Values in the WeakMapKeyMap can only be accessed by the WeakMap object that put them there.  If
        //     that WeakMap is marshalled it will take care of any necessary marshalling of the value by virtue
        //     of being wrapped in CrossSite<>.

        Var stackTraceValue = nullptr;
        if (this->GetInternalProperty(this, InternalPropertyIds::StackTrace, &stackTraceValue, nullptr, this->GetScriptContext()))
        {TRACE_IT(65925);
            this->SetInternalProperty(InternalPropertyIds::StackTrace, nullptr, PropertyOperation_None, nullptr);
        }
        else
        {TRACE_IT(65926);
            // Above GetInternalProperty fails - which means the stackTraceValue is filed with Missing result. Reset to null so that we will not restore it back below.
            stackTraceValue = nullptr;
        }

        Var weakMapKeyMapValue = nullptr;
        if (this->GetInternalProperty(this, InternalPropertyIds::WeakMapKeyMap, &weakMapKeyMapValue, nullptr, this->GetScriptContext()))
        {TRACE_IT(65927);
            this->SetInternalProperty(InternalPropertyIds::WeakMapKeyMap, nullptr, PropertyOperation_Force, nullptr);
        }
        else
        {TRACE_IT(65928);
            weakMapKeyMapValue = nullptr;
        }

        Var mutationBpValue = nullptr;
        if (this->GetInternalProperty(this, InternalPropertyIds::MutationBp, &mutationBpValue, nullptr, this->GetScriptContext()))
        {TRACE_IT(65929);
            this->SetInternalProperty(InternalPropertyIds::MutationBp, nullptr, PropertyOperation_Force, nullptr);
        }
        else
        {TRACE_IT(65930);
            mutationBpValue = nullptr;
        }

        if (keepProperties)
        {TRACE_IT(65931);
            this->GetTypeHandler()->MarshalAllPropertiesToScriptContext(this, this->GetScriptContext(), false);

            if (stackTraceValue)
            {TRACE_IT(65932);
                this->SetInternalProperty(InternalPropertyIds::StackTrace, stackTraceValue, PropertyOperation_None, nullptr);
            }
            if (weakMapKeyMapValue)
            {TRACE_IT(65933);
                this->SetInternalProperty(InternalPropertyIds::WeakMapKeyMap, weakMapKeyMapValue, PropertyOperation_Force, nullptr);
            }
            if (mutationBpValue)
            {TRACE_IT(65934);
                this->SetInternalProperty(InternalPropertyIds::MutationBp, mutationBpValue, PropertyOperation_Force, nullptr);
            }
        }
    }

    bool
    DynamicObject::GetHasNoEnumerableProperties()
    {TRACE_IT(65935);
        if (!this->GetTypeHandler()->EnsureObjectReady(this))
        {TRACE_IT(65936);
            return false;
        }

        if (!this->GetDynamicType()->GetHasNoEnumerableProperties())
        {TRACE_IT(65937);
            return false;
        }
        if (HasObjectArray() || (JavascriptArray::Is(this) && JavascriptArray::FromVar(this)->GetLength() != 0))
        {TRACE_IT(65938);
            return false;
        }
        return true;
    }

    bool
    DynamicObject::SetHasNoEnumerableProperties(bool value)
    {TRACE_IT(65939);
        return this->GetDynamicType()->SetHasNoEnumerableProperties(value);
    }

    BigPropertyIndex
    DynamicObject::GetPropertyIndexFromInlineSlotIndex(uint inlineSlotIndex)
    {TRACE_IT(65940);
        return this->GetTypeHandler()->GetPropertyIndexFromInlineSlotIndex(inlineSlotIndex);
    }

    BigPropertyIndex
    DynamicObject::GetPropertyIndexFromAuxSlotIndex(uint auxIndex)
    {TRACE_IT(65941);
        return this->GetTypeHandler()->GetPropertyIndexFromAuxSlotIndex(auxIndex);
    }

    BOOL
    DynamicObject::GetAttributesWithPropertyIndex(PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {TRACE_IT(65942);
        return this->GetTypeHandler()->GetAttributesWithPropertyIndex(this, propertyId, index, attributes);
    }

    RecyclerWeakReference<DynamicObject>* DynamicObject::CreateWeakReferenceToSelf()
    {TRACE_IT(65943);
        Assert(!ThreadContext::IsOnStack(this));
        return GetRecycler()->CreateWeakReferenceHandle(this);
    }

    DynamicObject *
    DynamicObject::BoxStackInstance(DynamicObject * instance)
    {TRACE_IT(65944);
        Assert(ThreadContext::IsOnStack(instance));
        // On the stack, the we reserved a pointer before the object as to store the boxed value
        DynamicObject ** boxedInstanceRef = ((DynamicObject **)instance) - 1;
        DynamicObject * boxedInstance = *boxedInstanceRef;
        if (boxedInstance)
        {TRACE_IT(65945);
            return boxedInstance;
        }

        size_t inlineSlotsSize = instance->GetTypeHandler()->GetInlineSlotsSize();
        if (inlineSlotsSize)
        {TRACE_IT(65946);
            boxedInstance = RecyclerNewPlusZ(instance->GetRecycler(), inlineSlotsSize, DynamicObject, instance);
        }
        else
        {TRACE_IT(65947);
            boxedInstance = RecyclerNew(instance->GetRecycler(), DynamicObject, instance);
        }

        *boxedInstanceRef = boxedInstance;
        return boxedInstance;
    }

#ifdef RECYCLER_STRESS
    void DynamicObject::Finalize(bool isShutdown)
    {TRACE_IT(65948);
        // If -RecyclerTrackStress is enabled, DynamicObject will be allocated as Track (and thus Finalize too).
        // Just ignore this.
        if (Js::Configuration::Global.flags.RecyclerTrackStress)
        {TRACE_IT(65949);
            return;
        }

        RecyclableObject::Finalize(isShutdown);
    }

    void DynamicObject::Dispose(bool isShutdown)
    {TRACE_IT(65950);
        // If -RecyclerTrackStress is enabled, DynamicObject will be allocated as Track (and thus Finalize too).
        // Just ignore this.
        if (Js::Configuration::Global.flags.RecyclerTrackStress)
        {TRACE_IT(65951);
            return;
        }

        RecyclableObject::Dispose(isShutdown);
    }

    void DynamicObject::Mark(Recycler *recycler)
    {TRACE_IT(65952);
        // If -RecyclerTrackStress is enabled, DynamicObject will be allocated as Track (and thus Finalize too).
        // Process the mark now.

        if (Js::Configuration::Global.flags.RecyclerTrackStress)
        {TRACE_IT(65953);
            size_t inlineSlotsSize = this->GetDynamicType()->GetTypeHandler()->GetInlineSlotsSize();
            size_t objectSize = sizeof(DynamicObject) + inlineSlotsSize;
            void ** obj = (void **)this;
            void ** objEnd = obj + (objectSize / sizeof(void *));

            do
            {TRACE_IT(65954);
                recycler->TryMarkNonInterior(*obj, nullptr);
                obj++;
            } while (obj != objEnd);

            return;
        }

        RecyclableObject::Mark(recycler);
    }
#endif

#if ENABLE_TTD

    TTD::NSSnapObjects::SnapObjectType DynamicObject::GetSnapTag_TTD() const
    {TRACE_IT(65955);
        return TTD::NSSnapObjects::SnapObjectType::SnapDynamicObject;
    }

    void DynamicObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(65956);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapDynamicObject>(objData, nullptr);
    }

    Js::Var const* DynamicObject::GetInlineSlots_TTD() const
    {TRACE_IT(65957);
        return reinterpret_cast<Var const*>(
            reinterpret_cast<size_t>(this) + this->GetTypeHandler()->GetOffsetOfInlineSlots());
    }

    Js::Var const* DynamicObject::GetAuxSlots_TTD() const
    {TRACE_IT(65958);
        return AddressOf(this->auxSlots[0]);
    }

#if ENABLE_OBJECT_SOURCE_TRACKING
    void DynamicObject::SetDiagOriginInfoAsNeeded()
    {TRACE_IT(65959);
        if(!TTD::IsDiagnosticOriginInformationValid(this->TTDDiagOriginInfo))
        {TRACE_IT(65960);
            if(this->GetScriptContext()->ShouldPerformRecordOrReplayAction())
            {TRACE_IT(65961);
                this->GetScriptContext()->GetThreadContext()->TTDLog->GetTimeAndPositionForDiagnosticObjectTracking(this->TTDDiagOriginInfo);
            }
        }
    }
#endif

#endif

} // namespace Js
