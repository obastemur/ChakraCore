//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if ENABLE_NATIVE_CODEGEN
#include "JITType.h"
#endif

namespace Js
{
    void InlineCache::CacheLocal(
        Type *const type,
        const PropertyId propertyId,
        const PropertyIndex propertyIndex,
        const bool isInlineSlot,
        Type *const typeWithoutProperty,
        int requiredAuxSlotCapacity,
        ScriptContext *const requestContext)
    {TRACE_IT(48236);
        Assert(type);
        Assert(propertyId != Constants::NoProperty);
        Assert(propertyIndex != Constants::NoSlot);
        Assert(requestContext);
        Assert(type->GetScriptContext() == requestContext);
        DebugOnly(VerifyRegistrationForInvalidation(this, requestContext, propertyId));
        Assert(requiredAuxSlotCapacity >= 0 && requiredAuxSlotCapacity < 0x01 << RequiredAuxSlotCapacityBitCount);
        // Store field and load field caches are never shared so we should never have a prototype cache morphing into an add property cache.
        // We may, however, have a flags cache (setter) change to add property cache.
        Assert(typeWithoutProperty == nullptr || !IsProto());

        requestContext->SetHasUsedInlineCache(true);

        // Add cache into a store field cache list if required, but not there yet.
        if (typeWithoutProperty != nullptr && invalidationListSlotPtr == nullptr)
        {TRACE_IT(48237);
            // Note, this can throw due to OOM, so we need to do it before the inline cache is set below.
            requestContext->RegisterStoreFieldInlineCache(this, propertyId);
        }

        if (isInlineSlot)
        {TRACE_IT(48238);
            u.local.type = type;
            u.local.typeWithoutProperty = typeWithoutProperty;
        }
        else
        {TRACE_IT(48239);
            u.local.type = TypeWithAuxSlotTag(type);
            u.local.typeWithoutProperty = typeWithoutProperty ? TypeWithAuxSlotTag(typeWithoutProperty) : nullptr;
        }

        u.local.isLocal = true;
        u.local.slotIndex = propertyIndex;
        u.local.requiredAuxSlotCapacity = requiredAuxSlotCapacity;

        type->SetHasBeenCached();
        if (typeWithoutProperty)
        {TRACE_IT(48240);
            typeWithoutProperty->SetHasBeenCached();
        }

        DebugOnly(VerifyRegistrationForInvalidation(this, requestContext, propertyId));

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(48241);
            Output::Print(_u("IC::CacheLocal, %s: "), requestContext->GetPropertyName(propertyId)->GetBuffer());
            Dump();
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
    }

    void InlineCache::CacheProto(
        DynamicObject *const prototypeObjectWithProperty,
        const PropertyId propertyId,
        const PropertyIndex propertyIndex,
        const bool isInlineSlot,
        const bool isMissing,
        Type *const type,
        ScriptContext *const requestContext)
    {TRACE_IT(48242);
        Assert(prototypeObjectWithProperty);
        Assert(propertyId != Constants::NoProperty);
        Assert(propertyIndex != Constants::NoSlot);
        Assert(type);
        Assert(requestContext);
        Assert(prototypeObjectWithProperty->GetScriptContext() == requestContext);
        DebugOnly(VerifyRegistrationForInvalidation(this, requestContext, propertyId));

        // This is an interesting quirk.  In the browser Chakra's global object cannot be used directly as a prototype, because
        // the global object (referenced either as window or as this) always points to the host object.  Thus, when we retrieve
        // a property from Chakra's global object the prototypeObjectWithProperty != info->GetInstance() and we will never cache
        // such property loads (see CacheOperators::CachePropertyRead).  However, in jc.exe or jshost.exe the only global object
        // is Chakra's global object, and so prototypeObjectWithProperty == info->GetInstance() and we can cache.  Hence, the
        // assert below is only correct when running in the browser.
        // Assert(prototypeObjectWithProperty != prototypeObjectWithProperty->type->GetLibrary()->GetGlobalObject());

        // Store field and load field caches are never shared so we should never have an add property cache morphing into a prototype cache.
        Assert(!IsLocal() || u.local.typeWithoutProperty == nullptr);

        requestContext->SetHasUsedInlineCache(true);

        // Add cache into a proto cache list if not there yet.
        if (invalidationListSlotPtr == nullptr)
        {TRACE_IT(48243);
            // Note, this can throw due to OOM, so we need to do it before the inline cache is set below.
            requestContext->RegisterProtoInlineCache(this, propertyId);
        }

        u.proto.prototypeObject = prototypeObjectWithProperty;
        u.proto.isProto = true;
        u.proto.isMissing = isMissing;
        u.proto.slotIndex = propertyIndex;
        if (isInlineSlot)
        {TRACE_IT(48244);
            u.proto.type = type;
        }
        else
        {TRACE_IT(48245);
            u.proto.type = TypeWithAuxSlotTag(type);
        }

        prototypeObjectWithProperty->GetType()->SetHasBeenCached();

        DebugOnly(VerifyRegistrationForInvalidation(this, requestContext, propertyId));
        Assert(u.proto.isMissing == (uint16)(u.proto.prototypeObject == requestContext->GetLibrary()->GetMissingPropertyHolder()));

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(48246);
            Output::Print(_u("IC::CacheProto, %s: "), requestContext->GetPropertyName(propertyId)->GetBuffer());
            Dump();
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
    }

    // TODO (InlineCacheCleanup): When simplifying inline caches due to not sharing between loads and stores, create two
    // separate methods CacheSetter and CacheGetter.
    void InlineCache::CacheAccessor(
        const bool isGetter,
        const PropertyId propertyId,
        const PropertyIndex propertyIndex,
        const bool isInlineSlot,
        Type *const type,
        DynamicObject *const object,
        const bool isOnProto,
        ScriptContext *const requestContext)
    {TRACE_IT(48247);
        Assert(propertyId != Constants::NoProperty);
        Assert(propertyIndex != Constants::NoSlot);
        Assert(type);
        Assert(object);
        Assert(requestContext);
        DebugOnly(VerifyRegistrationForInvalidation(this, requestContext, propertyId));
        // It is possible that prototype is from a different scriptContext than the original instance. We don't want to cache
        // in this case.
        Assert(type->GetScriptContext() == requestContext);

        requestContext->SetHasUsedInlineCache(true);

        if (isOnProto && invalidationListSlotPtr == nullptr)
        {TRACE_IT(48248);
            // Note, this can throw due to OOM, so we need to do it before the inline cache is set below.
            if (!isGetter)
            {TRACE_IT(48249);
                // If the setter is on a prototype, this cache must be invalidated whenever proto
                // caches are invalidated, so we must register it here.  Note that store field inline
                // caches are invalidated any time proto caches are invalidated.
                requestContext->RegisterStoreFieldInlineCache(this, propertyId);
            }
            else
            {TRACE_IT(48250);
                requestContext->RegisterProtoInlineCache(this, propertyId);
            }
        }

        u.accessor.isAccessor = true;
        // TODO (PersistentInlineCaches): Consider removing the flag altogether and just have a bit indicating
        // whether the cache itself is a store field cache (isStore?).
        u.accessor.flags = isGetter ? InlineCacheGetterFlag : InlineCacheSetterFlag;
        u.accessor.isOnProto = isOnProto;
        u.accessor.type = isInlineSlot ? type : TypeWithAuxSlotTag(type);
        u.accessor.slotIndex = propertyIndex;
        u.accessor.object = object;

        type->SetHasBeenCached();

        DebugOnly(VerifyRegistrationForInvalidation(this, requestContext, propertyId));

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(48251);
            Output::Print(_u("IC::CacheAccessor, %s: "), requestContext->GetPropertyName(propertyId)->GetBuffer());
            Dump();
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
    }

    bool InlineCache::PretendTryGetProperty(Type *const type, PropertyCacheOperationInfo * operationInfo)
    {TRACE_IT(48252);
        if (type == u.local.type)
        {TRACE_IT(48253);
            operationInfo->cacheType = CacheType_Local;
            operationInfo->slotType = SlotType_Inline;
            return true;
        }

        if (TypeWithAuxSlotTag(type) == u.local.type)
        {TRACE_IT(48254);
            operationInfo->cacheType = CacheType_Local;
            operationInfo->slotType = SlotType_Aux;
            return true;
        }

        if (type == u.proto.type)
        {TRACE_IT(48255);
            operationInfo->cacheType = CacheType_Proto;
            operationInfo->slotType = SlotType_Inline;
            return true;
        }

        if (TypeWithAuxSlotTag(type) == u.proto.type)
        {TRACE_IT(48256);
            operationInfo->cacheType = CacheType_Proto;
            operationInfo->slotType = SlotType_Aux;
            return true;
        }

        if (type == u.accessor.type)
        {TRACE_IT(48257);
            Assert(u.accessor.flags & InlineCacheGetterFlag);

            operationInfo->cacheType = CacheType_Getter;
            operationInfo->slotType = SlotType_Inline;
            return true;
        }

        if (TypeWithAuxSlotTag(type) == u.accessor.type)
        {TRACE_IT(48258);
            Assert(u.accessor.flags & InlineCacheGetterFlag);

            operationInfo->cacheType = CacheType_Getter;
            operationInfo->slotType = SlotType_Aux;
            return true;
        }

        return false;
    }

    bool InlineCache::PretendTrySetProperty(Type *const type, Type *const oldType, PropertyCacheOperationInfo * operationInfo)
    {TRACE_IT(48259);
        if (oldType == u.local.typeWithoutProperty)
        {TRACE_IT(48260);
            operationInfo->cacheType = CacheType_LocalWithoutProperty;
            operationInfo->slotType = SlotType_Inline;
            return true;
        }

        if (TypeWithAuxSlotTag(oldType) == u.local.typeWithoutProperty)
        {TRACE_IT(48261);
            operationInfo->cacheType = CacheType_LocalWithoutProperty;
            operationInfo->slotType = SlotType_Aux;
            return true;
        }

        if (type == u.local.type)
        {TRACE_IT(48262);
            operationInfo->cacheType = CacheType_Local;
            operationInfo->slotType = SlotType_Inline;
            return true;
        }

        if (TypeWithAuxSlotTag(type) == u.local.type)
        {TRACE_IT(48263);
            operationInfo->cacheType = CacheType_Local;
            operationInfo->slotType = SlotType_Aux;
            return true;
        }

        if (type == u.accessor.type)
        {TRACE_IT(48264);
            if (u.accessor.flags & InlineCacheSetterFlag)
            {TRACE_IT(48265);
                operationInfo->cacheType = CacheType_Setter;
                operationInfo->slotType = SlotType_Inline;
                return true;
            }
        }

        if (TypeWithAuxSlotTag(type) == u.accessor.type)
        {TRACE_IT(48266);
            if (u.accessor.flags & InlineCacheSetterFlag)
            {TRACE_IT(48267);
                operationInfo->cacheType = CacheType_Setter;
                operationInfo->slotType = SlotType_Aux;
                return true;
            }
        }

        return false;
    }

    bool InlineCache::GetGetterSetter(Type *const type, RecyclableObject **callee)
    {TRACE_IT(48268);
        Type *const taggedType = TypeWithAuxSlotTag(type);
        *callee = nullptr;

        if (u.accessor.flags & (InlineCacheGetterFlag | InlineCacheSetterFlag))
        {TRACE_IT(48269);
            if (type == u.accessor.type)
            {TRACE_IT(48270);
                *callee = RecyclableObject::FromVar(u.accessor.object->GetInlineSlot(u.accessor.slotIndex));
                return true;
            }
            else if (taggedType == u.accessor.type)
            {TRACE_IT(48271);
                *callee = RecyclableObject::FromVar(u.accessor.object->GetAuxSlot(u.accessor.slotIndex));
                return true;
            }
        }
        return false;
    }

    bool InlineCache::GetCallApplyTarget(RecyclableObject* obj, RecyclableObject **callee)
    {TRACE_IT(48272);
        Type *const type = obj->GetType();
        Type *const taggedType = TypeWithAuxSlotTag(type);
        *callee = nullptr;

        if (IsLocal())
        {TRACE_IT(48273);
            if (type == u.local.type)
            {TRACE_IT(48274);
                const Var objectAtInlineSlot = DynamicObject::FromVar(obj)->GetInlineSlot(u.local.slotIndex);
                if (!Js::TaggedNumber::Is(objectAtInlineSlot))
                {TRACE_IT(48275);
                    *callee = RecyclableObject::FromVar(objectAtInlineSlot);
                    return true;
                }
            }
            else if (taggedType == u.local.type)
            {TRACE_IT(48276);
                const Var objectAtAuxSlot = DynamicObject::FromVar(obj)->GetAuxSlot(u.local.slotIndex);
                if (!Js::TaggedNumber::Is(objectAtAuxSlot))
                {TRACE_IT(48277);
                    *callee = RecyclableObject::FromVar(DynamicObject::FromVar(obj)->GetAuxSlot(u.local.slotIndex));
                    return true;
                }
            }
            return false;
        }
        else if (IsProto())
        {TRACE_IT(48278);
            if (type == u.proto.type)
            {TRACE_IT(48279);
                const Var objectAtInlineSlot = u.proto.prototypeObject->GetInlineSlot(u.proto.slotIndex);
                if (!Js::TaggedNumber::Is(objectAtInlineSlot))
                {TRACE_IT(48280);
                    *callee = RecyclableObject::FromVar(objectAtInlineSlot);
                    return true;
                }
            }
            else if (taggedType == u.proto.type)
            {TRACE_IT(48281);
                const Var objectAtAuxSlot = u.proto.prototypeObject->GetAuxSlot(u.proto.slotIndex);
                if (!Js::TaggedNumber::Is(objectAtAuxSlot))
                {TRACE_IT(48282);
                    *callee = RecyclableObject::FromVar(objectAtAuxSlot);
                    return true;
                }
            }
            return false;
        }
        return false;
    }

    void InlineCache::Clear()
    {TRACE_IT(48283);
        // IsEmpty() is a quick check to see that the cache is not populated, it only checks u.local.type, which does not
        // guarantee that the proto or flags cache would not hit. So Clear() must still clear everything.

        u.local.type = nullptr;
        u.local.isLocal = true;
        u.local.typeWithoutProperty = nullptr;
    }

    InlineCache *InlineCache::Clone(Js::PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(48284);
        Assert(scriptContext);

        InlineCacheAllocator* allocator = scriptContext->GetInlineCacheAllocator();
        // Important to zero the allocated cache to be sure CopyTo doesn't see garbage
        // when it uses the next pointer.
        InlineCache* clone = AllocatorNewZ(InlineCacheAllocator, allocator, InlineCache);
        CopyTo(propertyId, scriptContext, clone);
        return clone;
    }

    bool InlineCache::TryGetFixedMethodFromCache(Js::FunctionBody* functionBody, uint cacheId, Js::JavascriptFunction** pFixedMethod)
    {TRACE_IT(48285);
        Assert(pFixedMethod);

        if (IsEmpty())
        {TRACE_IT(48286);
            return false;
        }
        Js::Type * propertyOwnerType = nullptr;
        bool isLocal = IsLocal();
        bool isProto = IsProto();
        if (isLocal)
        {TRACE_IT(48287);
            propertyOwnerType = TypeWithoutAuxSlotTag(this->u.local.type);
        }
        else if (isProto)
        {TRACE_IT(48288);
            // TODO (InlineCacheCleanup): For loads from proto, we could at least grab the value from protoObject's slot
            // (given by the cache) and see if its a function. Only then, does it make sense to check with the type handler.
            propertyOwnerType = this->u.proto.prototypeObject->GetType();
        }
        else
        {TRACE_IT(48289);
            propertyOwnerType = this->u.accessor.object->GetType();
        }

        Assert(propertyOwnerType != nullptr);

        if (Js::DynamicType::Is(propertyOwnerType->GetTypeId()))
        {TRACE_IT(48290);
            Js::DynamicTypeHandler* propertyOwnerTypeHandler = ((Js::DynamicType*)propertyOwnerType)->GetTypeHandler();
            Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(cacheId);
            Js::PropertyRecord const * const methodPropertyRecord = functionBody->GetScriptContext()->GetPropertyName(propertyId);

            Var fixedMethod = nullptr;
            bool isUseFixedProperty;
            if (isLocal || isProto)
            {TRACE_IT(48291);
                isUseFixedProperty = propertyOwnerTypeHandler->TryUseFixedProperty(methodPropertyRecord, &fixedMethod, Js::FixedPropertyKind::FixedMethodProperty, functionBody->GetScriptContext());
            }
            else
            {TRACE_IT(48292);
                isUseFixedProperty = propertyOwnerTypeHandler->TryUseFixedAccessor(methodPropertyRecord, &fixedMethod, Js::FixedPropertyKind::FixedAccessorProperty, this->IsGetterAccessor(), functionBody->GetScriptContext());
            }
            AssertMsg(fixedMethod == nullptr || Js::JavascriptFunction::Is(fixedMethod), "The fixed value should have been a Method !!!");
            *pFixedMethod = reinterpret_cast<JavascriptFunction*>(fixedMethod);
            return isUseFixedProperty;
        }

        return false;
    }

    void InlineCache::CopyTo(PropertyId propertyId, ScriptContext * scriptContext, InlineCache * const clone)
    {
        DebugOnly(VerifyRegistrationForInvalidation(this, scriptContext, propertyId));
        DebugOnly(VerifyRegistrationForInvalidation(clone, scriptContext, propertyId));
        Assert(clone != nullptr);

        // Note, the Register methods can throw due to OOM, so we need to do it before the inline cache is copied below.
        if (this->invalidationListSlotPtr != nullptr && clone->invalidationListSlotPtr == nullptr)
        {TRACE_IT(48293);
            if (this->NeedsToBeRegisteredForProtoInvalidation())
            {TRACE_IT(48294);
                scriptContext->RegisterProtoInlineCache(clone, propertyId);
            }
            else if (this->NeedsToBeRegisteredForStoreFieldInvalidation())
            {TRACE_IT(48295);
                scriptContext->RegisterStoreFieldInlineCache(clone, propertyId);
            }
        }

        clone->u = this->u;

        DebugOnly(VerifyRegistrationForInvalidation(clone, scriptContext, propertyId));
    }

    template <bool isAccessor>
    bool InlineCache::HasDifferentType(const bool isProto, const Type * type, const Type * typeWithoutProperty) const
    {TRACE_IT(48296);
        Assert(!isAccessor && !isProto || !typeWithoutProperty);

        if (isAccessor)
        {TRACE_IT(48297);
            return !IsEmpty() && u.accessor.type != type && u.accessor.type != TypeWithAuxSlotTag(type);
        }
        if (isProto)
        {TRACE_IT(48298);
            return !IsEmpty() && u.proto.type != type && u.proto.type != TypeWithAuxSlotTag(type);
        }

        // If the new type matches the cached type, the types without property must also match (unless one of them is null).
        Assert((u.local.typeWithoutProperty == nullptr || typeWithoutProperty == nullptr) ||
            ((u.local.type != type || u.local.typeWithoutProperty == typeWithoutProperty) &&
                (u.local.type != TypeWithAuxSlotTag(type) || u.local.typeWithoutProperty == TypeWithAuxSlotTag(typeWithoutProperty))));

        // Don't consider a cache polymorphic, if it differs only by the typeWithoutProperty.  We can handle this case with
        // the monomorphic cache.
        return !IsEmpty() && (u.local.type != type && u.local.type != TypeWithAuxSlotTag(type));
    }

    // explicit instantiation
    template bool InlineCache::HasDifferentType<true>(const bool isProto, const Type * type, const Type * typeWithoutProperty) const;
    template bool InlineCache::HasDifferentType<false>(const bool isProto, const Type * type, const Type * typeWithoutProperty) const;

    bool InlineCache::NeedsToBeRegisteredForProtoInvalidation() const
    {TRACE_IT(48299);
        return (IsProto() || IsGetterAccessorOnProto());
    }

    bool InlineCache::NeedsToBeRegisteredForStoreFieldInvalidation() const
    {TRACE_IT(48300);
        return (IsLocal() && this->u.local.typeWithoutProperty != nullptr) || IsSetterAccessorOnProto();
    }

#if DEBUG
    bool InlineCache::NeedsToBeRegisteredForInvalidation() const
    {TRACE_IT(48301);
        int howManyInvalidationsNeeded =
            (int)NeedsToBeRegisteredForProtoInvalidation() +
            (int)NeedsToBeRegisteredForStoreFieldInvalidation();
        Assert(howManyInvalidationsNeeded <= 1);
        return howManyInvalidationsNeeded > 0;
    }

    void InlineCache::VerifyRegistrationForInvalidation(const InlineCache* cache, ScriptContext* scriptContext, Js::PropertyId propertyId)
    {TRACE_IT(48302);
        bool needsProtoInvalidation = cache->NeedsToBeRegisteredForProtoInvalidation();
        bool needsStoreFieldInvalidation = cache->NeedsToBeRegisteredForStoreFieldInvalidation();
        int howManyInvalidationsNeeded = (int)needsProtoInvalidation + (int)needsStoreFieldInvalidation;
        bool hasListSlotPtr = cache->invalidationListSlotPtr != nullptr;
        bool isProtoRegistered = hasListSlotPtr ? scriptContext->GetThreadContext()->IsProtoInlineCacheRegistered(cache, propertyId) : false;
        bool isStoreFieldRegistered = hasListSlotPtr ? scriptContext->GetThreadContext()->IsStoreFieldInlineCacheRegistered(cache, propertyId) : false;
        int howManyRegistrations = (int)isProtoRegistered + (int)isStoreFieldRegistered;

        Assert(howManyInvalidationsNeeded <= 1);
        Assert((howManyInvalidationsNeeded == 0) || hasListSlotPtr);
        Assert(!needsProtoInvalidation || isProtoRegistered);
        Assert(!needsStoreFieldInvalidation || isStoreFieldRegistered);
        Assert(!hasListSlotPtr || howManyRegistrations > 0);
        Assert(!hasListSlotPtr || (*cache->invalidationListSlotPtr) == cache);
    }

    // Confirm inline cache miss against instance property lookup info.
    bool InlineCache::ConfirmCacheMiss(const Type * oldType, const PropertyValueInfo* info) const
    {TRACE_IT(48303);
        return u.local.type != oldType
            && u.proto.type != oldType
            && (u.accessor.type != oldType || info == NULL || u.accessor.flags != info->GetFlags());
    }
#endif

#if DBG_DUMP
    void InlineCache::Dump()
    {TRACE_IT(48304);
        if (this->u.local.isLocal)
        {TRACE_IT(48305);
            Output::Print(_u("LOCAL { types: 0x%X -> 0x%X, slot = %d, list slot ptr = 0x%X }"),
                this->u.local.typeWithoutProperty,
                this->u.local.type,
                this->u.local.slotIndex,
                this->invalidationListSlotPtr
                );
        }
        else if (this->u.proto.isProto)
        {TRACE_IT(48306);
            Output::Print(_u("PROTO { type = 0x%X, prototype = 0x%X, slot = %d, list slot ptr = 0x%X }"),
                this->u.proto.type,
                this->u.proto.prototypeObject,
                this->u.proto.slotIndex,
                this->invalidationListSlotPtr
                );
        }
        else if (this->u.accessor.isAccessor)
        {TRACE_IT(48307);
            Output::Print(_u("FLAGS { type = 0x%X, object = 0x%X, flag = 0x%X, slot = %d, list slot ptr = 0x%X }"),
                this->u.accessor.type,
                this->u.accessor.object,
                this->u.accessor.slotIndex,
                this->u.accessor.flags,
                this->invalidationListSlotPtr
                );
        }
        else
        {TRACE_IT(48308);
            Assert(this->u.accessor.type == 0);
            Assert(this->u.accessor.slotIndex == 0);
            Output::Print(_u("uninitialized"));
        }
    }

#endif
    PolymorphicInlineCache * PolymorphicInlineCache::New(uint16 size, FunctionBody * functionBody)
    {TRACE_IT(48309);
        ScriptContext * scriptContext = functionBody->GetScriptContext();
        InlineCache * inlineCaches = AllocatorNewArrayZ(InlineCacheAllocator, scriptContext->GetInlineCacheAllocator(), InlineCache, size);
#ifdef POLY_INLINE_CACHE_SIZE_STATS
        scriptContext->GetInlineCacheAllocator()->LogPolyCacheAlloc(size * sizeof(InlineCache));
#endif
        PolymorphicInlineCache * polymorphicInlineCache = RecyclerNewFinalizedLeaf(scriptContext->GetRecycler(), PolymorphicInlineCache, inlineCaches, size, functionBody);

        // Insert the cache into finalization list.  We maintain this linked list of polymorphic caches because when we allocate
        // a larger cache, the old one might still be used by some code on the stack.  Consequently, we can't release
        // the inline cache array back to the arena allocator.  The list is leaf-allocated and so does not keep the
        // old caches alive.  As soon as they are collectible, their finalizer releases the inline cache array to the arena.
        polymorphicInlineCache->prev = nullptr;
        polymorphicInlineCache->next = functionBody->GetPolymorphicInlineCachesHead();
        if (polymorphicInlineCache->next)
        {TRACE_IT(48310);
            polymorphicInlineCache->next->prev = polymorphicInlineCache;
        }
        functionBody->SetPolymorphicInlineCachesHead(polymorphicInlineCache);

        return polymorphicInlineCache;
    }

    template<bool isAccessor>
    bool PolymorphicInlineCache::HasDifferentType(
        const bool isProto,
        const Type * type,
        const Type * typeWithoutProperty) const
    {TRACE_IT(48311);
        Assert(!isAccessor && !isProto || !typeWithoutProperty);

        uint inlineCacheIndex = GetInlineCacheIndexForType(type);
        if (inlineCaches[inlineCacheIndex].HasDifferentType<isAccessor>(isProto, type, typeWithoutProperty))
        {TRACE_IT(48312);
            return true;
        }

        if (!isAccessor && !isProto && typeWithoutProperty)
        {TRACE_IT(48313);
            inlineCacheIndex = GetInlineCacheIndexForType(typeWithoutProperty);
            return inlineCaches[inlineCacheIndex].HasDifferentType<isAccessor>(isProto, type, typeWithoutProperty);
        }

        return false;
    }

    // explicit instantiation
    template bool PolymorphicInlineCache::HasDifferentType<true>(const bool isProto, const Type * type, const Type * typeWithoutProperty) const;
    template bool PolymorphicInlineCache::HasDifferentType<false>(const bool isProto, const Type * type, const Type * typeWithoutProperty) const;

    bool PolymorphicInlineCache::HasType_Flags(const Type * type) const
    {TRACE_IT(48314);
        uint inlineCacheIndex = GetInlineCacheIndexForType(type);
        return inlineCaches[inlineCacheIndex].HasType_Flags(type);
    }

    void PolymorphicInlineCache::UpdateInlineCachesFillInfo(uint index, bool set)
    {TRACE_IT(48315);
        Assert(index < 0x20);
        if (set)
        {TRACE_IT(48316);
            this->inlineCachesFillInfo |= 1 << index;
        }
        else
        {TRACE_IT(48317);
            this->inlineCachesFillInfo &= ~(1 << index);
        }
    }

    bool PolymorphicInlineCache::IsFull()
    {TRACE_IT(48318);
        Assert(this->size <= 0x20);
        return this->inlineCachesFillInfo == ((1 << (this->size - 1)) << 1) - 1;
    }

    void PolymorphicInlineCache::CacheLocal(
        Type *const type,
        const PropertyId propertyId,
        const PropertyIndex propertyIndex,
        const bool isInlineSlot,
        Type *const typeWithoutProperty,
        int requiredAuxSlotCapacity,
        ScriptContext *const requestContext)
    {TRACE_IT(48319);
        // Let's not waste polymorphic cache slots by caching both the type without property and type with property. If the
        // cache is used for both adding a property and setting the existing property, then those instances will cause both
        // types to be cached. Until then, caching both types proactively here can unnecessarily trash useful cached info
        // because the types use different slots, unlike a monomorphic inline cache.
        if (!typeWithoutProperty)
        {TRACE_IT(48320);
            uint inlineCacheIndex = GetInlineCacheIndexForType(type);
#if INTRUSIVE_TESTTRACE_PolymorphicInlineCache
            bool collision = !inlineCaches[inlineCacheIndex].IsEmpty();
#endif
            if (!PHASE_OFF1(Js::CloneCacheInCollisionPhase))
            {TRACE_IT(48321);
                if (!inlineCaches[inlineCacheIndex].IsEmpty() && !inlineCaches[inlineCacheIndex].NeedsToBeRegisteredForStoreFieldInvalidation())
                {TRACE_IT(48322);
                    if (inlineCaches[inlineCacheIndex].IsLocal())
                    {
                        CloneInlineCacheToEmptySlotInCollision<true, false, false>(type, inlineCacheIndex);
                    }
                    else if (inlineCaches[inlineCacheIndex].IsProto())
                    {
                        CloneInlineCacheToEmptySlotInCollision<false, true, false>(type, inlineCacheIndex);
                    }
                    else
                    {
                        CloneInlineCacheToEmptySlotInCollision<false, false, true>(type, inlineCacheIndex);
                    }
                }
            }

            inlineCaches[inlineCacheIndex].CacheLocal(
                type, propertyId, propertyIndex, isInlineSlot, nullptr, requiredAuxSlotCapacity, requestContext);
            UpdateInlineCachesFillInfo(inlineCacheIndex, true /*set*/);

#if DBG_DUMP
            if (PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
            {TRACE_IT(48323);
                Output::Print(_u("PIC::CacheLocal, %s, %d: "), requestContext->GetPropertyName(propertyId)->GetBuffer(), inlineCacheIndex);
                inlineCaches[inlineCacheIndex].Dump();
                Output::Print(_u("\n"));
                Output::Flush();
            }
#endif
            PHASE_PRINT_INTRUSIVE_TESTTRACE1(
                Js::PolymorphicInlineCachePhase,
                _u("TestTrace PIC: CacheLocal, 0x%x, entryIndex = %d, collision = %s, entries = %d\n"), this, inlineCacheIndex, collision ? _u("true") : _u("false"), GetEntryCount());
        }
        else
        {TRACE_IT(48324);
            uint inlineCacheIndex = GetInlineCacheIndexForType(typeWithoutProperty);
#if INTRUSIVE_TESTTRACE_PolymorphicInlineCache
            bool collision = !inlineCaches[inlineCacheIndex].IsEmpty();
#endif
            inlineCaches[inlineCacheIndex].CacheLocal(
                type, propertyId, propertyIndex, isInlineSlot, typeWithoutProperty, requiredAuxSlotCapacity, requestContext);
            UpdateInlineCachesFillInfo(inlineCacheIndex, true /*set*/);

#if DBG_DUMP
            if (PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
            {TRACE_IT(48325);
                Output::Print(_u("PIC::CacheLocal, %s, %d: "), requestContext->GetPropertyName(propertyId)->GetBuffer(), inlineCacheIndex);
                inlineCaches[inlineCacheIndex].Dump();
                Output::Print(_u("\n"));
                Output::Flush();
            }
#endif
            PHASE_PRINT_INTRUSIVE_TESTTRACE1(
                Js::PolymorphicInlineCachePhase,
                _u("TestTrace PIC: CacheLocal, 0x%x, entryIndex = %d, collision = %s, entries = %d\n"), this, inlineCacheIndex, collision ? _u("true") : _u("false"), GetEntryCount());
        }
    }

    void PolymorphicInlineCache::CacheProto(
        DynamicObject *const prototypeObjectWithProperty,
        const PropertyId propertyId,
        const PropertyIndex propertyIndex,
        const bool isInlineSlot,
        const bool isMissing,
        Type *const type,
        ScriptContext *const requestContext)
    {TRACE_IT(48326);
        uint inlineCacheIndex = GetInlineCacheIndexForType(type);
#if INTRUSIVE_TESTTRACE_PolymorphicInlineCache
        bool collision = !inlineCaches[inlineCacheIndex].IsEmpty();
#endif
        if (!PHASE_OFF1(Js::CloneCacheInCollisionPhase))
        {TRACE_IT(48327);
            if (!inlineCaches[inlineCacheIndex].IsEmpty() && !inlineCaches[inlineCacheIndex].NeedsToBeRegisteredForStoreFieldInvalidation())
            {TRACE_IT(48328);
                if (inlineCaches[inlineCacheIndex].IsLocal())
                {
                    CloneInlineCacheToEmptySlotInCollision<true, false, false>(type, inlineCacheIndex);
                }
                else if (inlineCaches[inlineCacheIndex].IsProto())
                {
                    CloneInlineCacheToEmptySlotInCollision<false, true, false>(type, inlineCacheIndex);
                }
                else
                {
                    CloneInlineCacheToEmptySlotInCollision<false, false, true>(type, inlineCacheIndex);
                }
            }
        }

        inlineCaches[inlineCacheIndex].CacheProto(
            prototypeObjectWithProperty, propertyId, propertyIndex, isInlineSlot, isMissing, type, requestContext);
        UpdateInlineCachesFillInfo(inlineCacheIndex, true /*set*/);

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
        {TRACE_IT(48329);
            Output::Print(_u("PIC::CacheProto, %s, %d: "), requestContext->GetPropertyName(propertyId)->GetBuffer(), inlineCacheIndex);
            inlineCaches[inlineCacheIndex].Dump();
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
        PHASE_PRINT_INTRUSIVE_TESTTRACE1(
            Js::PolymorphicInlineCachePhase,
            _u("TestTrace PIC: CacheProto, 0x%x, entryIndex = %d, collision = %s, entries = %d\n"), this, inlineCacheIndex, collision ? _u("true") : _u("false"), GetEntryCount());
    }

    void PolymorphicInlineCache::CacheAccessor(
        const bool isGetter,
        const PropertyId propertyId,
        const PropertyIndex propertyIndex,
        const bool isInlineSlot,
        Type *const type,
        DynamicObject *const object,
        const bool isOnProto,
        ScriptContext *const requestContext)
    {TRACE_IT(48330);
        uint inlineCacheIndex = GetInlineCacheIndexForType(type);
#if INTRUSIVE_TESTTRACE_PolymorphicInlineCache
        bool collision = !inlineCaches[inlineCacheIndex].IsEmpty();
#endif
        if (!PHASE_OFF1(Js::CloneCacheInCollisionPhase))
        {TRACE_IT(48331);
            if (!inlineCaches[inlineCacheIndex].IsEmpty() && !inlineCaches[inlineCacheIndex].NeedsToBeRegisteredForStoreFieldInvalidation())
            {TRACE_IT(48332);
                if (inlineCaches[inlineCacheIndex].IsLocal())
                {
                    CloneInlineCacheToEmptySlotInCollision<true, false, false>(type, inlineCacheIndex);
                }
                else if (inlineCaches[inlineCacheIndex].IsProto())
                {
                    CloneInlineCacheToEmptySlotInCollision<false, true, false>(type, inlineCacheIndex);
                }
                else
                {
                    CloneInlineCacheToEmptySlotInCollision<false, false, true>(type, inlineCacheIndex);
                }
            }
        }

        inlineCaches[inlineCacheIndex].CacheAccessor(isGetter, propertyId, propertyIndex, isInlineSlot, type, object, isOnProto, requestContext);
        UpdateInlineCachesFillInfo(inlineCacheIndex, true /*set*/);

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
        {TRACE_IT(48333);
            Output::Print(_u("PIC::CacheAccessor, %s, %d: "), requestContext->GetPropertyName(propertyId)->GetBuffer(), inlineCacheIndex);
            inlineCaches[inlineCacheIndex].Dump();
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
        PHASE_PRINT_INTRUSIVE_TESTTRACE1(
            Js::PolymorphicInlineCachePhase,
            _u("TestTrace PIC: CacheAccessor, 0x%x, entryIndex = %d, collision = %s, entries = %d\n"), this, inlineCacheIndex, collision ? _u("true") : _u("false"), GetEntryCount());
    }

    bool PolymorphicInlineCache::PretendTryGetProperty(
        Type *const type,
        PropertyCacheOperationInfo * operationInfo)
    {TRACE_IT(48334);
        uint inlineCacheIndex = GetInlineCacheIndexForType(type);
        return inlineCaches[inlineCacheIndex].PretendTryGetProperty(type, operationInfo);
    }

    bool PolymorphicInlineCache::PretendTrySetProperty(
        Type *const type,
        Type *const oldType,
        PropertyCacheOperationInfo * operationInfo)
    {TRACE_IT(48335);
        uint inlineCacheIndex = GetInlineCacheIndexForType(type);
        return inlineCaches[inlineCacheIndex].PretendTrySetProperty(type, oldType, operationInfo);
    }

    void PolymorphicInlineCache::CopyTo(PropertyId propertyId, ScriptContext* scriptContext, PolymorphicInlineCache *const clone)
    {TRACE_IT(48336);
        Assert(clone);

        clone->ignoreForEquivalentObjTypeSpec = this->ignoreForEquivalentObjTypeSpec;
        clone->cloneForJitTimeUse = this->cloneForJitTimeUse;

        for (uint i = 0; i < GetSize(); ++i)
        {TRACE_IT(48337);
            Type * type = inlineCaches[i].GetType();
            if (type)
            {TRACE_IT(48338);
                uint inlineCacheIndex = clone->GetInlineCacheIndexForType(type);

                // When copying inline caches from one polymorphic cache to another, types are again hashed to get the corresponding indices in the new polymorphic cache.
                // This might lead to collision in the new cache. We need to try to resolve that collision.
                if (!PHASE_OFF1(Js::CloneCacheInCollisionPhase))
                {TRACE_IT(48339);
                    if (!clone->inlineCaches[inlineCacheIndex].IsEmpty() && !clone->inlineCaches[inlineCacheIndex].NeedsToBeRegisteredForStoreFieldInvalidation())
                    {TRACE_IT(48340);
                        if (clone->inlineCaches[inlineCacheIndex].IsLocal())
                        {TRACE_IT(48341);
                            clone->CloneInlineCacheToEmptySlotInCollision<true, false, false>(type, inlineCacheIndex);
                        }
                        else if (clone->inlineCaches[inlineCacheIndex].IsProto())
                        {TRACE_IT(48342);
                            clone->CloneInlineCacheToEmptySlotInCollision<false, true, false>(type, inlineCacheIndex);
                        }
                        else
                        {TRACE_IT(48343);
                            clone->CloneInlineCacheToEmptySlotInCollision<false, false, true>(type, inlineCacheIndex);
                        }

                    }
                }
                inlineCaches[i].CopyTo(propertyId, scriptContext, &clone->inlineCaches[inlineCacheIndex]);
                clone->UpdateInlineCachesFillInfo(inlineCacheIndex, true /*set*/);
            }
        }
    }

#if DBG_DUMP
    void PolymorphicInlineCache::Dump()
    {TRACE_IT(48344);
        for (uint i = 0; i < size; ++i)
        {TRACE_IT(48345);
            if (!inlineCaches[i].IsEmpty())
            {TRACE_IT(48346);
                Output::Print(_u("  %d: "), i);
                inlineCaches[i].Dump();
                Output::Print(_u("\n"));
            }
        }
    }
#endif
#if ENABLE_NATIVE_CODEGEN

    EquivalentTypeSet::EquivalentTypeSet(RecyclerJITTypeHolder * types, uint16 count)
        : types(types), count(count), sortedAndDuplicatesRemoved(false)
    {TRACE_IT(48347);
    }

    JITTypeHolder EquivalentTypeSet::GetType(uint16 index) const
    {TRACE_IT(48348);
        Assert(this->types != nullptr && this->count > 0 && index < this->count);
        return this->types[index];
    }

    JITTypeHolder EquivalentTypeSet::GetFirstType() const
    {TRACE_IT(48349);
        return GetType(0);
    }

    bool EquivalentTypeSet::Contains(const JITTypeHolder type, uint16* pIndex)
    {TRACE_IT(48350);
        if (!this->GetSortedAndDuplicatesRemoved())
        {TRACE_IT(48351);
            this->SortAndRemoveDuplicates();
        }
        for (uint16 ti = 0; ti < this->count; ti++)
        {TRACE_IT(48352);
            if (this->GetType(ti) == type)
            {TRACE_IT(48353);
                if (pIndex)
                {TRACE_IT(48354);
                    *pIndex = ti;
                }
                return true;
            }
        }
        return false;
    }

    bool EquivalentTypeSet::AreIdentical(EquivalentTypeSet * left, EquivalentTypeSet * right)
    {TRACE_IT(48355);
        if (!left->GetSortedAndDuplicatesRemoved())
        {TRACE_IT(48356);
            left->SortAndRemoveDuplicates();
        }
        if (!right->GetSortedAndDuplicatesRemoved())
        {TRACE_IT(48357);
            right->SortAndRemoveDuplicates();
        }

        Assert(left->GetSortedAndDuplicatesRemoved() && right->GetSortedAndDuplicatesRemoved());

        if (left->count != right->count)
        {TRACE_IT(48358);
            return false;
        }

        // TODO: OOP JIT, optimize this (previously we had memcmp)
        for (uint i = 0; i < left->count; ++i)
        {TRACE_IT(48359);
            if (left->types[i] != right->types[i])
            {TRACE_IT(48360);
                return false;
            }
        }
        return true;
    }

    bool EquivalentTypeSet::IsSubsetOf(EquivalentTypeSet * left, EquivalentTypeSet * right)
    {TRACE_IT(48361);
        if (!left->GetSortedAndDuplicatesRemoved())
        {TRACE_IT(48362);
            left->SortAndRemoveDuplicates();
        }
        if (!right->GetSortedAndDuplicatesRemoved())
        {TRACE_IT(48363);
            right->SortAndRemoveDuplicates();
        }

        if (left->count > right->count)
        {TRACE_IT(48364);
            return false;
        }

        // Try to find each left type in the right set.
        int j = 0;
        for (int i = 0; i < left->count; i++)
        {TRACE_IT(48365);
            bool found = false;
            for (; j < right->count; j++)
            {TRACE_IT(48366);
                if (left->types[i] < right->types[j])
                {TRACE_IT(48367);
                    // Didn't find the left type. Fail.
                    return false;
                }
                if (left->types[i] == right->types[j])
                {TRACE_IT(48368);
                    // Found the left type. Continue to the next left/right pair.
                    found = true;
                    j++;
                    break;
                }
            }
            Assert(j <= right->count);
            if (j == right->count && !found)
            {TRACE_IT(48369);
                // Exhausted the right set without finding the current left type.
                return false;
            }
        }
        return true;
    }

    void EquivalentTypeSet::SortAndRemoveDuplicates()
    {TRACE_IT(48370);
        uint16 oldCount = this->count;
        uint16 i;

        // sorting
        for (i = 1; i < oldCount; i++)
        {TRACE_IT(48371);
            uint16 j = i;
            while (j > 0 && (this->types[j - 1] > this->types[j]))
            {TRACE_IT(48372);
                JITTypeHolder tmp = this->types[j];
                this->types[j] = this->types[j - 1];
                this->types[j - 1] = tmp;
                j--;
            }
        }

        // removing duplicate types from the sorted set
        i = 0;
        for (uint16 j = 1; j < oldCount; j++)
        {TRACE_IT(48373);
            if (this->types[i] != this->types[j])
            {TRACE_IT(48374);
                this->types[++i] = this->types[j];
            }
        }
        this->count = ++i;
        for (i; i < oldCount; i++)
        {TRACE_IT(48375);
            this->types[i] = JITTypeHolder(nullptr);
        }

        this->sortedAndDuplicatesRemoved = true;
    }
#endif

    ConstructorCache ConstructorCache::DefaultInstance;

    ConstructorCache* ConstructorCache::EnsureValidInstance(ConstructorCache* currentCache, ScriptContext* scriptContext)
    {TRACE_IT(48376);
        Assert(currentCache != nullptr);

        ConstructorCache* newCache = currentCache;

        // If the old cache has been invalidated, we need to create a new one to avoid incorrectly re-validating
        // caches that may have been hard-coded in the JIT-ed code with different prototype and type.  However, if
        // the cache is already polymorphic, it will not be hard-coded, and hence we don't need to allocate a new
        // one - in case the prototype property changes frequently.
        if (ConstructorCache::IsDefault(currentCache) || (currentCache->IsInvalidated() && !currentCache->IsPolymorphic()))
        {TRACE_IT(48377);
            // Review (jedmiad): I don't think we need to zero the struct, since we initialize each field.
            newCache = RecyclerNew(scriptContext->GetRecycler(), ConstructorCache);
            // TODO: Consider marking the cache as polymorphic only if the prototype and type actually changed.  In fact,
            // if they didn't change we could reuse the same cache and simply mark it as valid.  Not really true.  The cache
            // might have been invalidated due to a property becoming read-only.  In that case we can't re-validate an old
            // monomorphic cache.  We must allocate a new one.
            newCache->content.isPolymorphic = currentCache->content.isPopulated && currentCache->content.hasPrototypeChanged;
        }

        // If we kept the old invalidated cache, it better be marked as polymorphic.
        Assert(!newCache->IsInvalidated() || newCache->IsPolymorphic());

        // If the cache was polymorphic, we shouldn't have allocated a new one.
        Assert(!currentCache->IsPolymorphic() || newCache == currentCache);

        return newCache;
    }

    void ConstructorCache::InvalidateOnPrototypeChange()
    {TRACE_IT(48378);
        if (IsDefault(this))
        {TRACE_IT(48379);
            Assert(this->guard.value == CtorCacheGuardValues::Invalid);
            Assert(!this->content.isPopulated);
        }
        else if (this->guard.value == CtorCacheGuardValues::Special && this->content.skipDefaultNewObject)
        {TRACE_IT(48380);
            // Do nothing.  If we skip the default object, changes to the prototype property don't affect
            // what we'll do during object allocation.

            // Can't assert the following because we set the prototype property during library initialization.
            // AssertMsg(false, "Overriding a prototype on a built-in constructor should be illegal.");
        }
        else
        {TRACE_IT(48381);
            this->guard.value = CtorCacheGuardValues::Invalid;
            this->content.hasPrototypeChanged = true;
            // Make sure we don't leak the old type.
            Assert(this->content.type == nullptr);
            this->content.pendingType = nullptr;
            Assert(this->content.pendingType == nullptr);
            Assert(IsInvalidated());
        }
        Assert(IsConsistent());
    }

#if DBG_DUMP
    void ConstructorCache::Dump() const
    {TRACE_IT(48382);
        Output::Print(_u("guard value or type = 0x%p, script context = 0x%p, pending type = 0x%p, slots = %d, inline slots = %d, populated = %d, polymorphic = %d, update cache = %d, update type = %d, skip default = %d, no return = %d"),
            this->GetRawGuardValue(), this->GetScriptContext(), this->GetPendingType(), this->GetSlotCount(), this->GetInlineSlotCount(),
            this->IsPopulated(), this->IsPolymorphic(), this->GetUpdateCacheAfterCtor(), this->GetTypeUpdatePending(),
            this->GetSkipDefaultNewObject(), this->GetCtorHasNoExplicitReturnValue());
    }
#endif

    void IsInstInlineCache::Set(Type * instanceType, JavascriptFunction * function, JavascriptBoolean * result)
    {TRACE_IT(48383);
        this->type = instanceType;
        this->function = function;
        this->result = result;
    }

    void IsInstInlineCache::Clear()
    {TRACE_IT(48384);
        this->type = NULL;
        this->function = NULL;
        this->result = NULL;
    }

    void IsInstInlineCache::Unregister(ScriptContext * scriptContext)
    {TRACE_IT(48385);
        scriptContext->GetThreadContext()->UnregisterIsInstInlineCache(this, this->function);
    }

    bool IsInstInlineCache::TryGetResult(Var instance, JavascriptFunction * function, JavascriptBoolean ** result)
    {TRACE_IT(48386);
        // In order to get the result from the cache we must have a function instance.
        Assert(function != NULL);

        if (this->function == function &&
            this->type == RecyclableObject::FromVar(instance)->GetType())
        {TRACE_IT(48387);
            if (result != nullptr)
            {TRACE_IT(48388);
                (*result = this->result);
            }
            return true;
        }
        else
        {TRACE_IT(48389);
            return false;
        }
    }

    void IsInstInlineCache::Cache(Type * instanceType, JavascriptFunction * function, JavascriptBoolean *  result, ScriptContext * scriptContext)
    {TRACE_IT(48390);
        // In order to populate the cache we must have a function instance.
        Assert(function != nullptr);

        // We assume the following invariant: (cache->function != nullptr) => script context is registered as having some populated instance-of inline caches and
        // this cache is registered with thread context for invalidation.
        if (this->function == function)
        {TRACE_IT(48391);
            Assert(scriptContext->IsIsInstInlineCacheRegistered(this, function));
            this->Set(instanceType, function, result);
        }
        else
        {TRACE_IT(48392);
            if (this->function != nullptr)
            {TRACE_IT(48393);
                Unregister(scriptContext);
                Clear();
            }

            // If the cache's function is not null, the cache must have been registered already.  No need to register again.
            // In fact, ThreadContext::RegisterIsInstInlineCache, would assert if we tried to re-register the same cache (to enforce the invariant above).
            // Review (jedmiad): What happens if we run out of memory inside RegisterIsInstInlieCache?
            scriptContext->RegisterIsInstInlineCache(this, function);
            this->Set(instanceType, function, result);
        }
    }

    /* static */
    uint32 IsInstInlineCache::OffsetOfFunction()
    {TRACE_IT(48394);
        return offsetof(IsInstInlineCache, function);
    }

    /* static */
    uint32 IsInstInlineCache::OffsetOfType()
    {TRACE_IT(48395);
        return offsetof(IsInstInlineCache, type);
    }

    /* static */
    uint32 IsInstInlineCache::OffsetOfResult()
    {TRACE_IT(48396);
        return offsetof(IsInstInlineCache, result);
    }
}
