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
    {LOGMEIN("DynamicObject.cpp] 15\n");
        Assert(!UsesObjectArrayOrFlagsAsFlags());
        if(initSlots)
        {LOGMEIN("DynamicObject.cpp] 18\n");
            InitSlots(this);
        }
        else
        {
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
    {LOGMEIN("DynamicObject.cpp] 52\n");
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
        {LOGMEIN("DynamicObject.cpp] 66\n");
#if !FLOATVAR
            // Currently we only support temp numbers assigned to stack objects
            dstSlots[i] = JavascriptNumber::BoxStackNumber(srcSlots[i], scriptContext);
#else
            dstSlots[i] = srcSlots[i];
#endif

        }

        if (propertyCount > inlineSlotCapacity)
        {LOGMEIN("DynamicObject.cpp] 77\n");
            uint auxSlotCount = propertyCount - inlineSlotCapacity;

            for (uint i = 0; i < auxSlotCount; i++)
            {LOGMEIN("DynamicObject.cpp] 81\n");
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
    {LOGMEIN("DynamicObject.cpp] 97\n");
        return NewObject<DynamicObject>(recycler, type);
    }

    bool DynamicObject::Is(Var aValue)
    {LOGMEIN("DynamicObject.cpp] 102\n");
        return RecyclableObject::Is(aValue) && (RecyclableObject::FromVar(aValue)->GetTypeId() == TypeIds_Object);
    }

    DynamicObject* DynamicObject::FromVar(Var aValue)
    {LOGMEIN("DynamicObject.cpp] 107\n");
        RecyclableObject* obj = RecyclableObject::FromVar(aValue);
        AssertMsg(obj->DbgIsDynamicObject(), "Ensure instance is actually a DynamicObject");
        Assert(DynamicType::Is(obj->GetTypeId()));
        return static_cast<DynamicObject*>(obj);
    }

    ArrayObject* DynamicObject::EnsureObjectArray()
    {LOGMEIN("DynamicObject.cpp] 115\n");
        if (!HasObjectArray())
        {LOGMEIN("DynamicObject.cpp] 117\n");
            ScriptContext* scriptContext = GetScriptContext();
            ArrayObject* objArray = scriptContext->GetLibrary()->CreateArray(0, SparseArraySegmentBase::SMALL_CHUNK_SIZE);
            SetObjectArray(objArray);
        }
        Assert(HasObjectArray());
        return GetObjectArrayOrFlagsAsArray();
    }

    void DynamicObject::SetObjectArray(ArrayObject* objArray)
    {LOGMEIN("DynamicObject.cpp] 127\n");
        Assert(!IsAnyArray(this));

        DeoptimizeObjectHeaderInlining();

        this->objectArray = objArray;
        if (objArray)
        {LOGMEIN("DynamicObject.cpp] 134\n");
            if (!this->IsExtensible()) // sync objectArray isExtensible
            {LOGMEIN("DynamicObject.cpp] 136\n");
                objArray->PreventExtensions();
            }

            // sync objectArray is prototype
            if ((this->GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag) != 0)
            {LOGMEIN("DynamicObject.cpp] 142\n");
                objArray->SetIsPrototype();
            }
        }
    }

    bool DynamicObject::HasNonEmptyObjectArray() const
    {LOGMEIN("DynamicObject.cpp] 149\n");
        return HasObjectArray() && GetObjectArrayOrFlagsAsArray()->GetLength() > 0;
    }

    // Check if a typeId is of any array type (JavascriptArray or ES5Array).
    bool DynamicObject::IsAnyArrayTypeId(TypeId typeId)
    {LOGMEIN("DynamicObject.cpp] 155\n");
        return JavascriptArray::Is(typeId) || typeId == TypeIds_ES5Array;
    }

    // Check if a Var is either a JavascriptArray* or ES5Array*.
    bool DynamicObject::IsAnyArray(const Var aValue)
    {LOGMEIN("DynamicObject.cpp] 161\n");
        return IsAnyArrayTypeId(JavascriptOperators::GetTypeId(aValue));
    }

    BOOL DynamicObject::HasObjectArrayItem(uint32 index)
    {LOGMEIN("DynamicObject.cpp] 166\n");
        return HasObjectArray() && GetObjectArrayOrFlagsAsArray()->HasItem(index);
    }

    BOOL DynamicObject::DeleteObjectArrayItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("DynamicObject.cpp] 171\n");
        if (HasObjectArray())
        {LOGMEIN("DynamicObject.cpp] 173\n");
            return GetObjectArrayOrFlagsAsArray()->DeleteItem(index, flags);
        }
        return true;
    }

    BOOL DynamicObject::GetObjectArrayItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("DynamicObject.cpp] 180\n");
        *value = requestContext->GetMissingItemResult();
        return HasObjectArray() && GetObjectArrayOrFlagsAsArray()->GetItem(originalInstance, index, value, requestContext);
    }

    DescriptorFlags DynamicObject::GetObjectArrayItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("DynamicObject.cpp] 186\n");
        return HasObjectArray() ? GetObjectArrayOrFlagsAsArray()->GetItemSetter(index, setterValue, requestContext) : None;
    }

    BOOL DynamicObject::SetObjectArrayItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("DynamicObject.cpp] 191\n");
        const auto result = EnsureObjectArray()->SetItem(index, value, flags);

        // We don't track non-enumerable items in object arrays.  Any object with an object array reports having
        // enumerable properties.  See comment in DynamicObject::GetHasNoEnumerableProperties.
        //SetHasNoEnumerableProperties(false);

        return result;
    }

    BOOL DynamicObject::SetObjectArrayItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {LOGMEIN("DynamicObject.cpp] 202\n");
        const auto result = EnsureObjectArray()->SetItemWithAttributes(index, value, attributes);

        // We don't track non-enumerable items in object arrays.  Any object with an object array reports having
        // enumerable properties.  See comment in DynamicObject::GetHasNoEnumerableProperties.
        //if (attributes & PropertyEnumerable)
        //{
        //    SetHasNoEnumerableProperties(false);
        //}

        if (!(attributes & PropertyWritable) && result)
        {LOGMEIN("DynamicObject.cpp] 213\n");
            InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        }
        return result;
    }

    BOOL DynamicObject::SetObjectArrayItemAttributes(uint32 index, PropertyAttributes attributes)
    {LOGMEIN("DynamicObject.cpp] 220\n");
        const auto result = HasObjectArray() && GetObjectArrayOrFlagsAsArray()->SetItemAttributes(index, attributes);

        // We don't track non-enumerable items in object arrays.  Any object with an object array reports having
        // enumerable properties.  See comment in DynamicObject::GetHasNoEnumerableProperties.
        //if (attributes & PropertyEnumerable)
        //{
        //    SetHasNoEnumerableProperties(false);
        //}

        if (!(attributes & PropertyWritable) && result)
        {LOGMEIN("DynamicObject.cpp] 231\n");
            InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        }
        return result;
    }

    BOOL DynamicObject::SetObjectArrayItemWritable(PropertyId propertyId, BOOL writable)
    {LOGMEIN("DynamicObject.cpp] 238\n");
        const auto result = HasObjectArray() && GetObjectArrayOrFlagsAsArray()->SetWritable(propertyId, writable);
        if (!writable && result)
        {LOGMEIN("DynamicObject.cpp] 241\n");
            InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        }
        return result;
    }

    BOOL DynamicObject::SetObjectArrayItemAccessors(uint32 index, Var getter, Var setter)
    {LOGMEIN("DynamicObject.cpp] 248\n");
        const auto result = EnsureObjectArray()->SetItemAccessors(index, getter, setter);
        if (result)
        {LOGMEIN("DynamicObject.cpp] 251\n");
            InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype();
        }
        return result;
    }

    void DynamicObject::InvalidateHasOnlyWritableDataPropertiesInPrototypeChainCacheIfPrototype()
    {LOGMEIN("DynamicObject.cpp] 258\n");
        if (GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {LOGMEIN("DynamicObject.cpp] 260\n");
            // No need to invalidate store field caches for non-writable properties here.  We're dealing
            // with numeric properties only, and we never cache these in add property inline caches.

            // If this object is used as a prototype, the has-only-writable-data-properties-in-prototype-chain cache needs to be
            // invalidated here since the type handler of 'objectArray' is not marked as being used as a prototype
            GetType()->GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
    }

    bool DynamicObject::HasLockedType() const
    {LOGMEIN("DynamicObject.cpp] 271\n");
        return this->GetDynamicType()->GetIsLocked();
    }

    bool DynamicObject::HasSharedType() const
    {LOGMEIN("DynamicObject.cpp] 276\n");
        return this->GetDynamicType()->GetIsShared();
    }

    bool DynamicObject::HasSharedTypeHandler() const
    {LOGMEIN("DynamicObject.cpp] 281\n");
        return this->GetTypeHandler()->GetIsShared();
    }

    void DynamicObject::ReplaceType(DynamicType * type)
    {LOGMEIN("DynamicObject.cpp] 286\n");
        Assert(!type->isLocked || type->GetTypeHandler()->GetIsLocked());
        Assert(!type->isShared || type->GetTypeHandler()->GetIsShared());

        // For now, i have added only Aux Slot -> so new inlineSlotCapacity should be 2.
        AssertMsg(DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining(this->GetTypeHandler(), type->GetTypeHandler()),
            "Object is ObjectHeaderInlined and should have compatible TypeHandlers for proper transition");

        this->type = type;
    }

    void DynamicObject::ReplaceTypeWithPredecessorType(DynamicType * predecessorType)
    {LOGMEIN("DynamicObject.cpp] 298\n");
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
    {LOGMEIN("DynamicObject.cpp] 318\n");
        return offsetof(DynamicObject, auxSlots);
    }

    DWORD DynamicObject::GetOffsetOfObjectArray()
    {LOGMEIN("DynamicObject.cpp] 323\n");
        return offsetof(DynamicObject, objectArray);
    }

    DWORD DynamicObject::GetOffsetOfType()
    {LOGMEIN("DynamicObject.cpp] 328\n");
        return offsetof(DynamicObject, type);
    }

    void DynamicObject::EnsureSlots(int oldCount, int newCount, ScriptContext * scriptContext, DynamicTypeHandler * newTypeHandler)
    {LOGMEIN("DynamicObject.cpp] 333\n");
        this->GetTypeHandler()->EnsureSlots(this, oldCount, newCount, scriptContext, newTypeHandler);
    }

    void DynamicObject::EnsureSlots(int newCount, ScriptContext * scriptContext)
    {LOGMEIN("DynamicObject.cpp] 338\n");
        EnsureSlots(GetTypeHandler()->GetSlotCapacity(), newCount, scriptContext);
    }

    Var DynamicObject::GetSlot(int index)
    {LOGMEIN("DynamicObject.cpp] 343\n");
        return this->GetTypeHandler()->GetSlot(this, index);
    }

    Var DynamicObject::GetInlineSlot(int index)
    {LOGMEIN("DynamicObject.cpp] 348\n");
        return this->GetTypeHandler()->GetInlineSlot(this, index);
    }

    Var DynamicObject::GetAuxSlot(int index)
    {LOGMEIN("DynamicObject.cpp] 353\n");
        return this->GetTypeHandler()->GetAuxSlot(this, index);
    }

#if DBG
    void DynamicObject::SetSlot(PropertyId propertyId, bool allowLetConst, int index, Var value)
    {LOGMEIN("DynamicObject.cpp] 359\n");
        this->GetTypeHandler()->SetSlot(this, propertyId, allowLetConst, index, value);
    }

    void DynamicObject::SetInlineSlot(PropertyId propertyId, bool allowLetConst, int index, Var value)
    {LOGMEIN("DynamicObject.cpp] 364\n");
        this->GetTypeHandler()->SetInlineSlot(this, propertyId, allowLetConst, index, value);
    }

    void DynamicObject::SetAuxSlot(PropertyId propertyId, bool allowLetConst, int index, Var value)
    {LOGMEIN("DynamicObject.cpp] 369\n");
        this->GetTypeHandler()->SetAuxSlot(this, propertyId, allowLetConst, index, value);
    }
#else
    void DynamicObject::SetSlot(int index, Var value)
    {LOGMEIN("DynamicObject.cpp] 374\n");
        this->GetTypeHandler()->SetSlot(this, index, value);
    }

    void DynamicObject::SetInlineSlot(int index, Var value)
    {LOGMEIN("DynamicObject.cpp] 379\n");
        this->GetTypeHandler()->SetInlineSlot(this, index, value);
    }

    void DynamicObject::SetAuxSlot(int index, Var value)
    {LOGMEIN("DynamicObject.cpp] 384\n");
        this->GetTypeHandler()->SetAuxSlot(this, index, value);
    }
#endif

    bool
    DynamicObject::GetIsExtensible() const
    {LOGMEIN("DynamicObject.cpp] 391\n");
        return this->GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsExtensibleFlag;
    }

    BOOL
    DynamicObject::FindNextProperty(BigPropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId, PropertyAttributes* attributes,
        DynamicType *typeToEnumerate, EnumeratorFlags flags, ScriptContext * requestContext) const
    {LOGMEIN("DynamicObject.cpp] 398\n");
        if(index == Constants::NoBigSlot)
        {LOGMEIN("DynamicObject.cpp] 400\n");
            return FALSE;
        }

#if ENABLE_TTD
        if(this->GetScriptContext()->ShouldPerformReplayAction())
        {LOGMEIN("DynamicObject.cpp] 406\n");
            BOOL res = FALSE;
            PropertyAttributes tmpAttributes = PropertyNone;
            this->GetScriptContext()->GetThreadContext()->TTDLog->ReplayPropertyEnumEvent(requestContext, &res, &index, this, propertyId, &tmpAttributes, propertyString);

            if(attributes != nullptr)
            {LOGMEIN("DynamicObject.cpp] 412\n");
                *attributes = tmpAttributes;
            }

            return res;
        }
        else if(this->GetScriptContext()->ShouldPerformRecordAction())
        {LOGMEIN("DynamicObject.cpp] 419\n");
            BOOL res = this->GetTypeHandler()->FindNextProperty(requestContext, index, propertyString, propertyId, attributes, this->GetType(), typeToEnumerate, flags);

            PropertyAttributes tmpAttributes = (attributes != nullptr) ? *attributes : PropertyNone;
            this->GetScriptContext()->GetThreadContext()->TTDLog->RecordPropertyEnumEvent(res, *propertyId, tmpAttributes, *propertyString);
            return res;
        }
        else
        {
            return this->GetTypeHandler()->FindNextProperty(requestContext, index, propertyString, propertyId, attributes, this->GetType(), typeToEnumerate, flags);
        }
#else
        return this->GetTypeHandler()->FindNextProperty(requestContext, index, propertyString, propertyId, attributes, this->GetType(), typeToEnumerate, flags);
#endif
    }

    BOOL
    DynamicObject::HasDeferredTypeHandler() const
    {LOGMEIN("DynamicObject.cpp] 437\n");
        return this->GetTypeHandler()->IsDeferredTypeHandler();
    }

    DynamicTypeHandler *
    DynamicObject::GetTypeHandler() const
    {LOGMEIN("DynamicObject.cpp] 443\n");
        return this->GetDynamicType()->GetTypeHandler();
    }

    uint16 DynamicObject::GetOffsetOfInlineSlots() const
    {LOGMEIN("DynamicObject.cpp] 448\n");
        return this->GetDynamicType()->GetTypeHandler()->GetOffsetOfInlineSlots();
    }

    void
    DynamicObject::SetTypeHandler(DynamicTypeHandler * typeHandler, bool hasChanged)
    {LOGMEIN("DynamicObject.cpp] 454\n");
        if (hasChanged && this->HasLockedType())
        {LOGMEIN("DynamicObject.cpp] 456\n");
            this->ChangeType();
        }
        this->GetDynamicType()->typeHandler = typeHandler;
    }

    DynamicType* DynamicObject::DuplicateType()
    {LOGMEIN("DynamicObject.cpp] 463\n");
        return RecyclerNew(GetRecycler(), DynamicType, this->GetDynamicType());
    }

    /*
    *   DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining
    *   -   Checks if the TypeHandlers are compatible for transition from oldTypeHandler to newTypeHandler
    */
    bool DynamicObject::IsTypeHandlerCompatibleForObjectHeaderInlining(DynamicTypeHandler * oldTypeHandler, DynamicTypeHandler * newTypeHandler)
    {LOGMEIN("DynamicObject.cpp] 472\n");
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
    {LOGMEIN("DynamicObject.cpp] 486\n");
        return this->GetTypeHandler()->IsObjectHeaderInlinedTypeHandlerUnchecked();
    }

    bool DynamicObject::IsObjectHeaderInlinedTypeHandler() const
    {LOGMEIN("DynamicObject.cpp] 491\n");
        return this->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler();
    }

    bool DynamicObject::DeoptimizeObjectHeaderInlining()
    {LOGMEIN("DynamicObject.cpp] 496\n");
        if(!IsObjectHeaderInlinedTypeHandler())
        {LOGMEIN("DynamicObject.cpp] 498\n");
            return false;
        }

        if (PHASE_TRACE1(Js::ObjectHeaderInliningPhase))
        {LOGMEIN("DynamicObject.cpp] 503\n");
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
    {LOGMEIN("DynamicObject.cpp] 525\n");
        Assert(!GetDynamicType()->GetIsShared() || GetTypeHandler()->GetIsShared());
        this->type = this->DuplicateType();
    }

    void DynamicObject::ChangeTypeIf(const Type* oldType)
    {LOGMEIN("DynamicObject.cpp] 531\n");
        if (this->type == oldType)
        {LOGMEIN("DynamicObject.cpp] 533\n");
            ChangeType();
        }
    }

    DynamicObjectFlags DynamicObject::GetArrayFlags() const
    {LOGMEIN("DynamicObject.cpp] 539\n");
        Assert(IsAnyArray(const_cast<DynamicObject *>(this)));
        Assert(UsesObjectArrayOrFlagsAsFlags()); // an array object never has another internal array
        return arrayFlags & DynamicObjectFlags::AllArrayFlags;
    }

    DynamicObjectFlags DynamicObject::GetArrayFlags_Unchecked() const // do not use except in extreme circumstances
    {LOGMEIN("DynamicObject.cpp] 546\n");
        return arrayFlags & DynamicObjectFlags::AllArrayFlags;
    }

    void DynamicObject::InitArrayFlags(const DynamicObjectFlags flags)
    {LOGMEIN("DynamicObject.cpp] 551\n");
        Assert(IsAnyArray(this));
        Assert(this->objectArray == nullptr);
        Assert((flags & DynamicObjectFlags::ObjectArrayFlagsTag) == DynamicObjectFlags::ObjectArrayFlagsTag);
        Assert((flags & ~DynamicObjectFlags::AllFlags) == DynamicObjectFlags::None);
        this->arrayFlags = flags;
    }

    void DynamicObject::SetArrayFlags(const DynamicObjectFlags flags)
    {LOGMEIN("DynamicObject.cpp] 560\n");
        Assert(IsAnyArray(this));
        Assert(UsesObjectArrayOrFlagsAsFlags()); // an array object never has another internal array
        // Make sure we don't attempt to set any flags outside of the range of array flags.
        Assert((arrayFlags & ~DynamicObjectFlags::AllArrayFlags) == DynamicObjectFlags::ObjectArrayFlagsTag);
        Assert((flags & ~DynamicObjectFlags::AllArrayFlags) == DynamicObjectFlags::None);
        arrayFlags = flags | DynamicObjectFlags::ObjectArrayFlagsTag;
    }

    ProfileId DynamicObject::GetArrayCallSiteIndex() const
    {LOGMEIN("DynamicObject.cpp] 570\n");
        Assert(IsAnyArray(const_cast<DynamicObject *>(this)));
        return arrayCallSiteIndex;
    }

    void DynamicObject::SetArrayCallSiteIndex(ProfileId profileId)
    {LOGMEIN("DynamicObject.cpp] 576\n");
        Assert(IsAnyArray(this));
        arrayCallSiteIndex = profileId;
    }

    void DynamicObject::SetIsPrototype()
    {LOGMEIN("DynamicObject.cpp] 582\n");
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
        {LOGMEIN("DynamicObject.cpp] 595\n");
            Assert(this->GetObjectArray() == nullptr || (this->GetObjectArray()->GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag) != 0);
            return;
        }

        currentTypeHandler->SetIsPrototype(this);
        // Get type handler again, in case it got changed by SetIsPrototype.
        currentTypeHandler = this->GetTypeHandler();

        // Set the object array as a prototype as well, so if it is an ES5 array, we will disable the array set element fast path
        ArrayObject * objectArray = this->GetObjectArray();
        if (objectArray)
        {LOGMEIN("DynamicObject.cpp] 607\n");
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
        {LOGMEIN("DynamicObject.cpp] 620\n");
            currentTypeHandler->InvalidateStoreFieldCachesForAllProperties(this->GetScriptContext());
        }
    }

    bool
    DynamicObject::LockType()
    {LOGMEIN("DynamicObject.cpp] 627\n");
        return this->GetDynamicType()->LockType();
    }

    bool
    DynamicObject::ShareType()
    {LOGMEIN("DynamicObject.cpp] 633\n");
        return this->GetDynamicType()->ShareType();
    }

    void
    DynamicObject::ResetObject(DynamicType* newType, BOOL keepProperties)
    {LOGMEIN("DynamicObject.cpp] 639\n");
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
        {LOGMEIN("DynamicObject.cpp] 661\n");
            this->objectArray = nullptr;
        }
        oldTypeHandler->ResetTypeHandler(this);
        Assert(this->GetScriptContext() == newType->GetScriptContext());

        if (this->GetTypeHandler()->IsDeferredTypeHandler())
        {LOGMEIN("DynamicObject.cpp] 668\n");
            return;
        }

        if (!keepProperties)
        {LOGMEIN("DynamicObject.cpp] 673\n");
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
        {LOGMEIN("DynamicObject.cpp] 688\n");
            this->SetInternalProperty(InternalPropertyIds::StackTrace, nullptr, PropertyOperation_None, nullptr);
        }
        else
        {
            // Above GetInternalProperty fails - which means the stackTraceValue is filed with Missing result. Reset to null so that we will not restore it back below.
            stackTraceValue = nullptr;
        }

        Var weakMapKeyMapValue = nullptr;
        if (this->GetInternalProperty(this, InternalPropertyIds::WeakMapKeyMap, &weakMapKeyMapValue, nullptr, this->GetScriptContext()))
        {LOGMEIN("DynamicObject.cpp] 699\n");
            this->SetInternalProperty(InternalPropertyIds::WeakMapKeyMap, nullptr, PropertyOperation_Force, nullptr);
        }
        else
        {
            weakMapKeyMapValue = nullptr;
        }

        Var mutationBpValue = nullptr;
        if (this->GetInternalProperty(this, InternalPropertyIds::MutationBp, &mutationBpValue, nullptr, this->GetScriptContext()))
        {LOGMEIN("DynamicObject.cpp] 709\n");
            this->SetInternalProperty(InternalPropertyIds::MutationBp, nullptr, PropertyOperation_Force, nullptr);
        }
        else
        {
            mutationBpValue = nullptr;
        }

        if (keepProperties)
        {LOGMEIN("DynamicObject.cpp] 718\n");
            this->GetTypeHandler()->MarshalAllPropertiesToScriptContext(this, this->GetScriptContext(), false);

            if (stackTraceValue)
            {LOGMEIN("DynamicObject.cpp] 722\n");
                this->SetInternalProperty(InternalPropertyIds::StackTrace, stackTraceValue, PropertyOperation_None, nullptr);
            }
            if (weakMapKeyMapValue)
            {LOGMEIN("DynamicObject.cpp] 726\n");
                this->SetInternalProperty(InternalPropertyIds::WeakMapKeyMap, weakMapKeyMapValue, PropertyOperation_Force, nullptr);
            }
            if (mutationBpValue)
            {LOGMEIN("DynamicObject.cpp] 730\n");
                this->SetInternalProperty(InternalPropertyIds::MutationBp, mutationBpValue, PropertyOperation_Force, nullptr);
            }
        }
    }

    bool
    DynamicObject::GetHasNoEnumerableProperties()
    {LOGMEIN("DynamicObject.cpp] 738\n");
        if (!this->GetTypeHandler()->EnsureObjectReady(this))
        {LOGMEIN("DynamicObject.cpp] 740\n");
            return false;
        }

        if (!this->GetDynamicType()->GetHasNoEnumerableProperties())
        {LOGMEIN("DynamicObject.cpp] 745\n");
            return false;
        }
        if (HasObjectArray() || (JavascriptArray::Is(this) && JavascriptArray::FromVar(this)->GetLength() != 0))
        {LOGMEIN("DynamicObject.cpp] 749\n");
            return false;
        }
        return true;
    }

    bool
    DynamicObject::SetHasNoEnumerableProperties(bool value)
    {LOGMEIN("DynamicObject.cpp] 757\n");
        return this->GetDynamicType()->SetHasNoEnumerableProperties(value);
    }

    BigPropertyIndex
    DynamicObject::GetPropertyIndexFromInlineSlotIndex(uint inlineSlotIndex)
    {LOGMEIN("DynamicObject.cpp] 763\n");
        return this->GetTypeHandler()->GetPropertyIndexFromInlineSlotIndex(inlineSlotIndex);
    }

    BigPropertyIndex
    DynamicObject::GetPropertyIndexFromAuxSlotIndex(uint auxIndex)
    {LOGMEIN("DynamicObject.cpp] 769\n");
        return this->GetTypeHandler()->GetPropertyIndexFromAuxSlotIndex(auxIndex);
    }

    BOOL
    DynamicObject::GetAttributesWithPropertyIndex(PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {LOGMEIN("DynamicObject.cpp] 775\n");
        return this->GetTypeHandler()->GetAttributesWithPropertyIndex(this, propertyId, index, attributes);
    }

    RecyclerWeakReference<DynamicObject>* DynamicObject::CreateWeakReferenceToSelf()
    {LOGMEIN("DynamicObject.cpp] 780\n");
        Assert(!ThreadContext::IsOnStack(this));
        return GetRecycler()->CreateWeakReferenceHandle(this);
    }

    DynamicObject *
    DynamicObject::BoxStackInstance(DynamicObject * instance)
    {LOGMEIN("DynamicObject.cpp] 787\n");
        Assert(ThreadContext::IsOnStack(instance));
        // On the stack, the we reserved a pointer before the object as to store the boxed value
        DynamicObject ** boxedInstanceRef = ((DynamicObject **)instance) - 1;
        DynamicObject * boxedInstance = *boxedInstanceRef;
        if (boxedInstance)
        {LOGMEIN("DynamicObject.cpp] 793\n");
            return boxedInstance;
        }

        size_t inlineSlotsSize = instance->GetTypeHandler()->GetInlineSlotsSize();
        if (inlineSlotsSize)
        {LOGMEIN("DynamicObject.cpp] 799\n");
            boxedInstance = RecyclerNewPlusZ(instance->GetRecycler(), inlineSlotsSize, DynamicObject, instance);
        }
        else
        {
            boxedInstance = RecyclerNew(instance->GetRecycler(), DynamicObject, instance);
        }

        *boxedInstanceRef = boxedInstance;
        return boxedInstance;
    }

#ifdef RECYCLER_STRESS
    void DynamicObject::Finalize(bool isShutdown)
    {LOGMEIN("DynamicObject.cpp] 813\n");
        // If -RecyclerTrackStress is enabled, DynamicObject will be allocated as Track (and thus Finalize too).
        // Just ignore this.
        if (Js::Configuration::Global.flags.RecyclerTrackStress)
        {LOGMEIN("DynamicObject.cpp] 817\n");
            return;
        }

        RecyclableObject::Finalize(isShutdown);
    }

    void DynamicObject::Dispose(bool isShutdown)
    {LOGMEIN("DynamicObject.cpp] 825\n");
        // If -RecyclerTrackStress is enabled, DynamicObject will be allocated as Track (and thus Finalize too).
        // Just ignore this.
        if (Js::Configuration::Global.flags.RecyclerTrackStress)
        {LOGMEIN("DynamicObject.cpp] 829\n");
            return;
        }

        RecyclableObject::Dispose(isShutdown);
    }

    void DynamicObject::Mark(Recycler *recycler)
    {LOGMEIN("DynamicObject.cpp] 837\n");
        // If -RecyclerTrackStress is enabled, DynamicObject will be allocated as Track (and thus Finalize too).
        // Process the mark now.

        if (Js::Configuration::Global.flags.RecyclerTrackStress)
        {LOGMEIN("DynamicObject.cpp] 842\n");
            size_t inlineSlotsSize = this->GetDynamicType()->GetTypeHandler()->GetInlineSlotsSize();
            size_t objectSize = sizeof(DynamicObject) + inlineSlotsSize;
            void ** obj = (void **)this;
            void ** objEnd = obj + (objectSize / sizeof(void *));

            do
            {LOGMEIN("DynamicObject.cpp] 849\n");
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
    {LOGMEIN("DynamicObject.cpp] 864\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapDynamicObject;
    }

    void DynamicObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("DynamicObject.cpp] 869\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapDynamicObject>(objData, nullptr);
    }

    Js::Var const* DynamicObject::GetInlineSlots_TTD() const
    {LOGMEIN("DynamicObject.cpp] 874\n");
        return reinterpret_cast<Var const*>(
            reinterpret_cast<size_t>(this) + this->GetTypeHandler()->GetOffsetOfInlineSlots());
    }

    Js::Var const* DynamicObject::GetAuxSlots_TTD() const
    {LOGMEIN("DynamicObject.cpp] 880\n");
        return AddressOf(this->auxSlots[0]);
    }

#if ENABLE_OBJECT_SOURCE_TRACKING
    void DynamicObject::SetDiagOriginInfoAsNeeded()
    {LOGMEIN("DynamicObject.cpp] 886\n");
        if(!TTD::IsDiagnosticOriginInformationValid(this->TTDDiagOriginInfo))
        {LOGMEIN("DynamicObject.cpp] 888\n");
            if(this->GetScriptContext()->ShouldPerformRecordOrReplayAction())
            {LOGMEIN("DynamicObject.cpp] 890\n");
                this->GetScriptContext()->GetThreadContext()->TTDLog->GetTimeAndPositionForDiagnosticObjectTracking(this->TTDDiagOriginInfo);
            }
        }
    }
#endif

#endif

} // namespace Js
