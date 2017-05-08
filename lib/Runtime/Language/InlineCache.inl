//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    template<
        bool CheckLocal,
        bool CheckProto,
        bool CheckAccessor,
        bool CheckMissing,
        bool ReturnOperationInfo>
    bool InlineCache::TryGetProperty(
        Var const instance,
        RecyclableObject *const propertyObject,
        const PropertyId propertyId,
        Var *const propertyValue,
        ScriptContext *const requestContext,
        PropertyCacheOperationInfo *const operationInfo)
    {TRACE_IT(48495);
        CompileAssert(CheckLocal || CheckProto || CheckAccessor);
        Assert(!ReturnOperationInfo || operationInfo);
        CompileAssert(!ReturnOperationInfo || (CheckLocal && CheckProto && CheckAccessor));
        Assert(instance);
        Assert(propertyObject);
        Assert(propertyId != Constants::NoProperty);
        Assert(propertyValue);
        Assert(requestContext);
        DebugOnly(VerifyRegistrationForInvalidation(this, requestContext, propertyId));

        Type *const type = propertyObject->GetType();

        if (CheckLocal && type == u.local.type)
        {TRACE_IT(48496);
            Assert(propertyObject->GetScriptContext() == requestContext); // we never cache a type from another script context
            *propertyValue = DynamicObject::FromVar(propertyObject)->GetInlineSlot(u.local.slotIndex);
            Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext) ||
                (RootObjectBase::Is(propertyObject) && *propertyValue == JavascriptOperators::GetRootProperty(propertyObject, propertyId, requestContext)));
            if (ReturnOperationInfo)
            {TRACE_IT(48497);
                operationInfo->cacheType = CacheType_Local;
                operationInfo->slotType = SlotType_Inline;
            }
            return true;
        }

        if (CheckLocal && TypeWithAuxSlotTag(type) == u.local.type)
        {TRACE_IT(48498);
            Assert(propertyObject->GetScriptContext() == requestContext); // we never cache a type from another script context
            *propertyValue = DynamicObject::FromVar(propertyObject)->GetAuxSlot(u.local.slotIndex);
            Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext) ||
                (RootObjectBase::Is(propertyObject) && *propertyValue == JavascriptOperators::GetRootProperty(propertyObject, propertyId, requestContext)));
            if (ReturnOperationInfo)
            {TRACE_IT(48499);
                operationInfo->cacheType = CacheType_Local;
                operationInfo->slotType = SlotType_Aux;
            }
            return true;
        }

        if (CheckProto && type == u.proto.type && !this->u.proto.isMissing)
        {TRACE_IT(48500);
            Assert(u.proto.prototypeObject->GetScriptContext() == requestContext); // we never cache a type from another script context
            *propertyValue = u.proto.prototypeObject->GetInlineSlot(u.proto.slotIndex);
            Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext) ||
                (RootObjectBase::Is(propertyObject) && *propertyValue == JavascriptOperators::GetRootProperty(propertyObject, propertyId, requestContext)));
            if (ReturnOperationInfo)
            {TRACE_IT(48501);
                operationInfo->cacheType = CacheType_Proto;
                operationInfo->slotType = SlotType_Inline;
            }
            return true;
        }

        if (CheckProto && TypeWithAuxSlotTag(type) == u.proto.type && !this->u.proto.isMissing)
        {TRACE_IT(48502);
            Assert(u.proto.prototypeObject->GetScriptContext() == requestContext); // we never cache a type from another script context
            *propertyValue = u.proto.prototypeObject->GetAuxSlot(u.proto.slotIndex);
            Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext) ||
                (RootObjectBase::Is(propertyObject) && *propertyValue == JavascriptOperators::GetRootProperty(propertyObject, propertyId, requestContext)));
            if (ReturnOperationInfo)
            {TRACE_IT(48503);
                operationInfo->cacheType = CacheType_Proto;
                operationInfo->slotType = SlotType_Aux;
            }
            return true;
        }

        if (CheckAccessor && type == u.accessor.type)
        {TRACE_IT(48504);
            Assert(propertyObject->GetScriptContext() == requestContext); // we never cache a type from another script context
            Assert(u.accessor.flags & InlineCacheGetterFlag);

            RecyclableObject *const function = RecyclableObject::FromVar(u.accessor.object->GetInlineSlot(u.accessor.slotIndex));

            *propertyValue = JavascriptOperators::CallGetter(function, instance, requestContext);

            // Can't assert because the getter could have a side effect
#ifdef CHKGETTER
            Assert(JavascriptOperators::Equal(*propertyValue, JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext), requestContext));
#endif
            if (ReturnOperationInfo)
            {TRACE_IT(48505);
                operationInfo->cacheType = CacheType_Getter;
                operationInfo->slotType = SlotType_Inline;
            }
            return true;
        }

        if (CheckAccessor && TypeWithAuxSlotTag(type) == u.accessor.type)
        {TRACE_IT(48506);
            Assert(propertyObject->GetScriptContext() == requestContext); // we never cache a type from another script context
            Assert(u.accessor.flags & InlineCacheGetterFlag);

            RecyclableObject *const function = RecyclableObject::FromVar(u.accessor.object->GetAuxSlot(u.accessor.slotIndex));

            *propertyValue = JavascriptOperators::CallGetter(function, instance, requestContext);

            // Can't assert because the getter could have a side effect
#ifdef CHKGETTER
            Assert(JavascriptOperators::Equal(*propertyValue, JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext), requestContext));
#endif
            if (ReturnOperationInfo)
            {TRACE_IT(48507);
                operationInfo->cacheType = CacheType_Getter;
                operationInfo->slotType = SlotType_Aux;
            }
            return true;
        }

        if (CheckMissing && type == u.proto.type && this->u.proto.isMissing)
        {TRACE_IT(48508);
            Assert(u.proto.prototypeObject->GetScriptContext() == requestContext); // we never cache a type from another script context
            *propertyValue = u.proto.prototypeObject->GetInlineSlot(u.proto.slotIndex);
            Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext) ||
                (RootObjectBase::Is(propertyObject) && *propertyValue == JavascriptOperators::GetRootProperty(propertyObject, propertyId, requestContext)));

#ifdef MISSING_PROPERTY_STATS
            if (PHASE_STATS1(MissingPropertyCachePhase))
            {TRACE_IT(48509);
                requestContext->RecordMissingPropertyHit();
            }
#endif

            if (ReturnOperationInfo)
            {TRACE_IT(48510);
                operationInfo->cacheType = CacheType_Proto;
                operationInfo->slotType = SlotType_Inline;
            }
            return true;
        }

        if (CheckMissing && TypeWithAuxSlotTag(type) == u.proto.type && this->u.proto.isMissing)
        {TRACE_IT(48511);
            Assert(u.proto.prototypeObject->GetScriptContext() == requestContext); // we never cache a type from another script context
            *propertyValue = u.proto.prototypeObject->GetAuxSlot(u.proto.slotIndex);
            Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext) ||
                (RootObjectBase::Is(propertyObject) && *propertyValue == JavascriptOperators::GetRootProperty(propertyObject, propertyId, requestContext)));

#ifdef MISSING_PROPERTY_STATS
            if (PHASE_STATS1(MissingPropertyCachePhase))
            {TRACE_IT(48512);
                requestContext->RecordMissingPropertyHit();
            }
#endif

            if (ReturnOperationInfo)
            {TRACE_IT(48513);
                operationInfo->cacheType = CacheType_Proto;
                operationInfo->slotType = SlotType_Aux;
            }
            return true;
        }

        return false;
    }

    template<
        bool CheckLocal,
        bool CheckLocalTypeWithoutProperty,
        bool CheckAccessor,
        bool ReturnOperationInfo>
    bool InlineCache::TrySetProperty(
        RecyclableObject *const object,
        const PropertyId propertyId,
        Var propertyValue,
        ScriptContext *const requestContext,
        PropertyCacheOperationInfo *const operationInfo,
        const PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(48514);
        CompileAssert(CheckLocal || CheckLocalTypeWithoutProperty || CheckAccessor);
        Assert(!ReturnOperationInfo || operationInfo);
        CompileAssert(!ReturnOperationInfo || (CheckLocal && CheckLocalTypeWithoutProperty && CheckAccessor));
        Assert(object);
        Assert(propertyId != Constants::NoProperty);
        Assert(requestContext);
        DebugOnly(VerifyRegistrationForInvalidation(this, requestContext, propertyId));

#if DBG
        const bool isRoot = (propertyOperationFlags & PropertyOperation_Root) != 0;

        bool canSetField; // To verify if we can set a field on the object
        Var setterValue = nullptr;
        { 
            // We need to disable implicit call to ensure the check doesn't cause unwanted side effects in debug code
            // Save old disableImplicitFlags and implicitCallFlags and disable implicit call and exception
            ThreadContext * threadContext = requestContext->GetThreadContext();
            DisableImplicitFlags disableImplicitFlags = *threadContext->GetAddressOfDisableImplicitFlags();
            Js::ImplicitCallFlags implicitCallFlags = threadContext->GetImplicitCallFlags();
            threadContext->ClearImplicitCallFlags();
            *threadContext->GetAddressOfDisableImplicitFlags() = DisableImplicitCallAndExceptionFlag;

            DescriptorFlags flags = DescriptorFlags::None;
            canSetField = !JavascriptOperators::CheckPrototypesForAccessorOrNonWritablePropertySlow(object, propertyId, &setterValue, &flags, isRoot, requestContext);
            if (threadContext->GetImplicitCallFlags() != Js::ImplicitCall_None)
            {TRACE_IT(48515);
                canSetField = true; // If there was an implicit call, inconclusive. Disable debug check.
                setterValue = nullptr;
            }
            else 
                if ((flags & Accessor) == Accessor)
            {TRACE_IT(48516);
                Assert(setterValue != nullptr);
            }

            // Restore old disableImplicitFlags and implicitCallFlags
            *threadContext->GetAddressOfDisableImplicitFlags() = disableImplicitFlags;
            threadContext->SetImplicitCallFlags(implicitCallFlags);
        }
#endif

        Type *const type = object->GetType();

        if (CheckLocal && type == u.local.type)
        {TRACE_IT(48517);
            Assert(object->GetScriptContext() == requestContext); // we never cache a type from another script context
            Assert(isRoot || object->GetPropertyIndex(propertyId) == DynamicObject::FromVar(object)->GetTypeHandler()->InlineOrAuxSlotIndexToPropertyIndex(u.local.slotIndex, true));
            Assert(!isRoot || RootObjectBase::FromVar(object)->GetRootPropertyIndex(propertyId) == DynamicObject::FromVar(object)->GetTypeHandler()->InlineOrAuxSlotIndexToPropertyIndex(u.local.slotIndex, true));
            Assert(object->CanStorePropertyValueDirectly(propertyId, isRoot));
            DynamicObject::FromVar(object)->SetInlineSlot(SetSlotArgumentsRoot(propertyId, isRoot, u.local.slotIndex, propertyValue));
            if (ReturnOperationInfo)
            {TRACE_IT(48518);
                operationInfo->cacheType = CacheType_Local;
                operationInfo->slotType = SlotType_Inline;
            }
            Assert(canSetField);
            return true;
        }

        if (CheckLocal && TypeWithAuxSlotTag(type) == u.local.type)
        {TRACE_IT(48519);
            Assert(object->GetScriptContext() == requestContext); // we never cache a type from another script context
            Assert(isRoot || object->GetPropertyIndex(propertyId) == DynamicObject::FromVar(object)->GetTypeHandler()->InlineOrAuxSlotIndexToPropertyIndex(u.local.slotIndex, false));
            Assert(!isRoot || RootObjectBase::FromVar(object)->GetRootPropertyIndex(propertyId) == DynamicObject::FromVar(object)->GetTypeHandler()->InlineOrAuxSlotIndexToPropertyIndex(u.local.slotIndex, false));
            Assert(object->CanStorePropertyValueDirectly(propertyId, isRoot));
            DynamicObject::FromVar(object)->SetAuxSlot(SetSlotArgumentsRoot(propertyId, isRoot, u.local.slotIndex, propertyValue));
            if (ReturnOperationInfo)
            {TRACE_IT(48520);
                operationInfo->cacheType = CacheType_Local;
                operationInfo->slotType = SlotType_Aux;
            }
            Assert(canSetField);
            return true;
        }

        if (CheckLocalTypeWithoutProperty && type == u.local.typeWithoutProperty)
        {TRACE_IT(48521);
            // CAREFUL! CheckIfPrototypeChainHasOnlyWritableDataProperties may do allocation that triggers GC and
            // clears this cache, so save any info that is needed from the cache before calling those functions.
            Type *const typeWithProperty = u.local.type;
            const PropertyIndex propertyIndex = u.local.slotIndex;

#if DBG
            uint16 newAuxSlotCapacity = u.local.requiredAuxSlotCapacity;
#endif
            Assert(object->GetScriptContext() == requestContext); // we never cache a type from another script context
            Assert(typeWithProperty);
            Assert(DynamicType::Is(typeWithProperty->GetTypeId()));
            Assert(((DynamicType*)typeWithProperty)->GetIsShared());
            Assert(((DynamicType*)typeWithProperty)->GetTypeHandler()->IsPathTypeHandler());
            AssertMsg(!((DynamicType*)u.local.typeWithoutProperty)->GetTypeHandler()->GetIsPrototype(), "Why did we cache a property add for a prototype?");
            Assert(((DynamicType*)typeWithProperty)->GetTypeHandler()->CanStorePropertyValueDirectly((const DynamicObject*)object, propertyId, isRoot));

            DynamicObject *const dynamicObject = DynamicObject::FromVar(object);

            // If we're adding a property to an inlined slot, we should never need to adjust auxiliary slot array size.
            Assert(newAuxSlotCapacity == 0);

            dynamicObject->type = typeWithProperty;

            Assert(isRoot || object->GetPropertyIndex(propertyId) == DynamicObject::FromVar(object)->GetTypeHandler()->InlineOrAuxSlotIndexToPropertyIndex(propertyIndex, true));
            Assert(!isRoot || RootObjectBase::FromVar(object)->GetRootPropertyIndex(propertyId) == DynamicObject::FromVar(object)->GetTypeHandler()->InlineOrAuxSlotIndexToPropertyIndex(propertyIndex, true));

            dynamicObject->SetInlineSlot(SetSlotArgumentsRoot(propertyId, isRoot, propertyIndex, propertyValue));

            if (ReturnOperationInfo)
            {TRACE_IT(48522);
                operationInfo->cacheType = CacheType_LocalWithoutProperty;
                operationInfo->slotType = SlotType_Inline;
            }
            Assert(canSetField);
            return true;
        }

        if (CheckLocalTypeWithoutProperty && TypeWithAuxSlotTag(type) == u.local.typeWithoutProperty)
        {TRACE_IT(48523);
            // CAREFUL! CheckIfPrototypeChainHasOnlyWritableDataProperties or AdjustSlots may do allocation that triggers GC and
            // clears this cache, so save any info that is needed from the cache before calling those functions.
            Type *const typeWithProperty = TypeWithoutAuxSlotTag(u.local.type);
            const PropertyIndex propertyIndex = u.local.slotIndex;
            uint16 newAuxSlotCapacity = u.local.requiredAuxSlotCapacity;

            Assert(object->GetScriptContext() == requestContext); // we never cache a type from another script context
            Assert(typeWithProperty);
            Assert(DynamicType::Is(typeWithProperty->GetTypeId()));
            Assert(((DynamicType*)typeWithProperty)->GetIsShared());
            Assert(((DynamicType*)typeWithProperty)->GetTypeHandler()->IsPathTypeHandler());
            AssertMsg(!((DynamicType*)TypeWithoutAuxSlotTag(u.local.typeWithoutProperty))->GetTypeHandler()->GetIsPrototype(), "Why did we cache a property add for a prototype?");
            Assert(((DynamicType*)typeWithProperty)->GetTypeHandler()->CanStorePropertyValueDirectly((const DynamicObject*)object, propertyId, isRoot));

            DynamicObject *const dynamicObject = DynamicObject::FromVar(object);

            if (newAuxSlotCapacity > 0)
            {TRACE_IT(48524);
                DynamicTypeHandler::AdjustSlots(
                    dynamicObject,
                    static_cast<DynamicType *>(typeWithProperty)->GetTypeHandler()->GetInlineSlotCapacity(),
                    newAuxSlotCapacity);
            }

            dynamicObject->type = typeWithProperty;

            Assert(isRoot || object->GetPropertyIndex(propertyId) == DynamicObject::FromVar(object)->GetTypeHandler()->InlineOrAuxSlotIndexToPropertyIndex(propertyIndex, false));
            Assert(!isRoot || RootObjectBase::FromVar(object)->GetRootPropertyIndex(propertyId) == DynamicObject::FromVar(object)->GetTypeHandler()->InlineOrAuxSlotIndexToPropertyIndex(propertyIndex, false));

            dynamicObject->SetAuxSlot(SetSlotArgumentsRoot(propertyId, isRoot, propertyIndex, propertyValue));

            if (ReturnOperationInfo)
            {TRACE_IT(48525);
                operationInfo->cacheType = CacheType_LocalWithoutProperty;
                operationInfo->slotType = SlotType_Aux;
            }
            Assert(canSetField);
            return true;
        }

        if (CheckAccessor && type == u.accessor.type)
        {TRACE_IT(48526);
            Assert(object->GetScriptContext() == requestContext); // we never cache a type from another script context
            Assert(u.accessor.flags & InlineCacheSetterFlag);

            RecyclableObject *const function = RecyclableObject::FromVar(u.accessor.object->GetInlineSlot(u.accessor.slotIndex));

            Assert(setterValue == nullptr || setterValue == function);
            Js::JavascriptOperators::CallSetter(function, object, propertyValue, requestContext);

            if (ReturnOperationInfo)
            {TRACE_IT(48527);
                operationInfo->cacheType = CacheType_Setter;
                operationInfo->slotType = SlotType_Inline;
            }
            return true;
        }

        if (CheckAccessor && TypeWithAuxSlotTag(type) == u.accessor.type)
        {TRACE_IT(48528);
            Assert(object->GetScriptContext() == requestContext); // we never cache a type from another script context
            Assert(u.accessor.flags & InlineCacheSetterFlag);

            RecyclableObject *const function = RecyclableObject::FromVar(u.accessor.object->GetAuxSlot(u.accessor.slotIndex));

            Assert(setterValue == nullptr || setterValue == function);
            Js::JavascriptOperators::CallSetter(function, object, propertyValue, requestContext);

            if (ReturnOperationInfo)
            {TRACE_IT(48529);
                operationInfo->cacheType = CacheType_Setter;
                operationInfo->slotType = SlotType_Aux;
            }
            return true;
        }

        return false;
    }

    template<
        bool CheckLocal,
        bool CheckProto,
        bool CheckAccessor>
    void PolymorphicInlineCache::CloneInlineCacheToEmptySlotInCollision(Type * const type, uint inlineCacheIndex)
    {TRACE_IT(48530);
        if (CheckLocal && (inlineCaches[inlineCacheIndex].u.local.type == type || inlineCaches[inlineCacheIndex].u.local.type == TypeWithAuxSlotTag(type)))
        {TRACE_IT(48531);
            return;
        }
        if (CheckProto && (inlineCaches[inlineCacheIndex].u.proto.type == type || inlineCaches[inlineCacheIndex].u.proto.type == TypeWithAuxSlotTag(type)))
        {TRACE_IT(48532);
            return;
        }
        if (CheckAccessor && (inlineCaches[inlineCacheIndex].u.accessor.type == type || inlineCaches[inlineCacheIndex].u.accessor.type == TypeWithAuxSlotTag(type)))
        {TRACE_IT(48533);
            return;
        }

        if (this->IsFull())
        {TRACE_IT(48534);
            // If the cache is full, we won't find an empty slot to move the contents of the colliding inline cache to.
            return;
        }

        // Collision is with a cache having a different type.

        uint tryInlineCacheIndex = GetNextInlineCacheIndex(inlineCacheIndex);

        // Iterate over the inline caches in the polymorphic cache, stop when:
        //   1. an empty inline cache is found, or
        //   2. a cache already populated with the incoming type is found, or
        //   3. all the inline caches have been looked at.
        while (!inlineCaches[tryInlineCacheIndex].IsEmpty() && tryInlineCacheIndex != inlineCacheIndex)
        {TRACE_IT(48535);
            if (CheckLocal && (inlineCaches[tryInlineCacheIndex].u.local.type == type || inlineCaches[tryInlineCacheIndex].u.local.type == TypeWithAuxSlotTag(type)))
            {TRACE_IT(48536);
                break;
            }
            if (CheckProto && (inlineCaches[tryInlineCacheIndex].u.proto.type == type || inlineCaches[tryInlineCacheIndex].u.proto.type == TypeWithAuxSlotTag(type)))
            {TRACE_IT(48537);
                Assert(GetInlineCacheIndexForType(inlineCaches[tryInlineCacheIndex].u.proto.type) == inlineCacheIndex);
                break;
            }
            if (CheckAccessor && (inlineCaches[tryInlineCacheIndex].u.accessor.type == type || inlineCaches[tryInlineCacheIndex].u.accessor.type == TypeWithAuxSlotTag(type)))
            {TRACE_IT(48538);
                Assert(GetInlineCacheIndexForType(inlineCaches[tryInlineCacheIndex].u.accessor.type) == inlineCacheIndex);
                break;
            }
            tryInlineCacheIndex = GetNextInlineCacheIndex(tryInlineCacheIndex);
        }
        if (tryInlineCacheIndex != inlineCacheIndex)
        {TRACE_IT(48539);
            if (inlineCaches[inlineCacheIndex].invalidationListSlotPtr != nullptr)
            {TRACE_IT(48540);
                Assert(*(inlineCaches[inlineCacheIndex].invalidationListSlotPtr) == &inlineCaches[inlineCacheIndex]);
                if (inlineCaches[tryInlineCacheIndex].invalidationListSlotPtr != nullptr)
                {TRACE_IT(48541);
                    Assert(*(inlineCaches[tryInlineCacheIndex].invalidationListSlotPtr) == &inlineCaches[tryInlineCacheIndex]);
                }
                else
                {TRACE_IT(48542);
                    inlineCaches[tryInlineCacheIndex].invalidationListSlotPtr = inlineCaches[inlineCacheIndex].invalidationListSlotPtr;
                    *(inlineCaches[tryInlineCacheIndex].invalidationListSlotPtr) = &inlineCaches[tryInlineCacheIndex];
                    inlineCaches[inlineCacheIndex].invalidationListSlotPtr = nullptr;
                }
            }
            inlineCaches[tryInlineCacheIndex].u = inlineCaches[inlineCacheIndex].u;
            UpdateInlineCachesFillInfo(tryInlineCacheIndex, true /*set*/);
            // Let's clear the cache slot on which we had the collision. We might have stolen the invalidationListSlotPtr,
            // so it may not pass VerifyRegistrationForInvalidation. Besides, it will be repopulated with the incoming data,
            // and registered for invalidation, if necessary.
            inlineCaches[inlineCacheIndex].Clear();
            Assert((this->inlineCachesFillInfo & (1 << inlineCacheIndex)) != 0);
            UpdateInlineCachesFillInfo(inlineCacheIndex, false /*set*/);
        }
    }

#ifdef CLONE_INLINECACHE_TO_EMPTYSLOT
    template <typename TDelegate>
    bool PolymorphicInlineCache::CheckClonedInlineCache(uint inlineCacheIndex, TDelegate mapper)
    {TRACE_IT(48543);
        bool success = false;
        uint tryInlineCacheIndex = GetNextInlineCacheIndex(inlineCacheIndex);
        do
        {TRACE_IT(48544);
            if (inlineCaches[tryInlineCacheIndex].IsEmpty())
            {TRACE_IT(48545);
                break;
            }
            success = mapper(tryInlineCacheIndex);
            if (success)
            {TRACE_IT(48546);
                Assert(inlineCaches[inlineCacheIndex].invalidationListSlotPtr == nullptr || *inlineCaches[inlineCacheIndex].invalidationListSlotPtr == &inlineCaches[inlineCacheIndex]);
                Assert(inlineCaches[tryInlineCacheIndex].invalidationListSlotPtr == nullptr || *inlineCaches[tryInlineCacheIndex].invalidationListSlotPtr == &inlineCaches[tryInlineCacheIndex]);

                // Swap inline caches, including their invalidationListSlotPtrs.
                InlineCache temp = inlineCaches[tryInlineCacheIndex];
                inlineCaches[tryInlineCacheIndex] = inlineCaches[inlineCacheIndex];
                inlineCaches[inlineCacheIndex] = temp;

                // Fix up invalidationListSlotPtrs to point to their owners.
                if (inlineCaches[inlineCacheIndex].invalidationListSlotPtr != nullptr)
                {TRACE_IT(48547);
                    *inlineCaches[inlineCacheIndex].invalidationListSlotPtr = &inlineCaches[inlineCacheIndex];
                }
                if (inlineCaches[tryInlineCacheIndex].invalidationListSlotPtr != nullptr)
                {TRACE_IT(48548);
                    *inlineCaches[tryInlineCacheIndex].invalidationListSlotPtr = &inlineCaches[tryInlineCacheIndex];
                }

                break;
            }
            tryInlineCacheIndex = GetNextInlineCacheIndex(tryInlineCacheIndex);

        } while (tryInlineCacheIndex != inlineCacheIndex);

        return success;
    }
#endif

    template<
        bool CheckLocal,
        bool CheckProto,
        bool CheckAccessor,
        bool CheckMissing,
        bool IsInlineCacheAvailable,
        bool ReturnOperationInfo>
    bool PolymorphicInlineCache::TryGetProperty(
        Var const instance,
        RecyclableObject *const propertyObject,
        const PropertyId propertyId,
        Var *const propertyValue,
        ScriptContext *const requestContext,
        PropertyCacheOperationInfo *const operationInfo,
        InlineCache *const inlineCacheToPopulate)
    {TRACE_IT(48549);
        Assert(!IsInlineCacheAvailable || inlineCacheToPopulate);
        Assert(!ReturnOperationInfo || operationInfo);

        Type * const type = propertyObject->GetType();
        uint inlineCacheIndex = GetInlineCacheIndexForType(type);
        InlineCache *cache = &inlineCaches[inlineCacheIndex];

#ifdef INLINE_CACHE_STATS
        bool isEmpty = false;
        if (PHASE_STATS1(Js::PolymorphicInlineCachePhase))
        {TRACE_IT(48550);
            isEmpty = cache->IsEmpty();
        }
#endif
        bool result = cache->TryGetProperty<CheckLocal, CheckProto, CheckAccessor, CheckMissing, ReturnOperationInfo>(
            instance, propertyObject, propertyId, propertyValue, requestContext, operationInfo);

#ifdef CLONE_INLINECACHE_TO_EMPTYSLOT
        if (!result && !cache->IsEmpty())
        {TRACE_IT(48551);
            result = CheckClonedInlineCache(inlineCacheIndex, [&](uint tryInlineCacheIndex) -> bool
            {
                cache = &inlineCaches[tryInlineCacheIndex];
                return cache->TryGetProperty<CheckLocal, CheckProto, CheckAccessor, CheckMissing, ReturnOperationInfo>(
                    instance, propertyObject, propertyId, propertyValue, requestContext, operationInfo);
            });
        }
#endif

        if (IsInlineCacheAvailable && result)
        {TRACE_IT(48552);
            cache->CopyTo(propertyId, requestContext, inlineCacheToPopulate);
        }

#ifdef INLINE_CACHE_STATS
        if (PHASE_STATS1(Js::PolymorphicInlineCachePhase))
        {TRACE_IT(48553);
            bool collision = !result && !isEmpty;
            this->functionBody->GetScriptContext()->LogCacheUsage(this, /*isGet*/ true, propertyId, result, collision);
        }
#endif

        return result;
    }

    template<
        bool CheckLocal,
        bool CheckLocalTypeWithoutProperty,
        bool CheckAccessor,
        bool IsInlineCacheAvailable,
        bool ReturnOperationInfo>
    bool PolymorphicInlineCache::TrySetProperty(
        RecyclableObject *const object,
        const PropertyId propertyId,
        Var propertyValue,
        ScriptContext *const requestContext,
        PropertyCacheOperationInfo *const operationInfo,
        InlineCache *const inlineCacheToPopulate,
        const PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(48554);
        Assert(!IsInlineCacheAvailable || inlineCacheToPopulate);
        Assert(!ReturnOperationInfo || operationInfo);

        Type * const type = object->GetType();
        uint inlineCacheIndex = GetInlineCacheIndexForType(type);
        InlineCache *cache = &inlineCaches[inlineCacheIndex];

#ifdef INLINE_CACHE_STATS
        bool isEmpty = false;
        if (PHASE_STATS1(Js::PolymorphicInlineCachePhase))
        {TRACE_IT(48555);
            isEmpty = cache->IsEmpty();
        }
#endif
        bool result = cache->TrySetProperty<CheckLocal, CheckLocalTypeWithoutProperty, CheckAccessor, ReturnOperationInfo>(
            object, propertyId, propertyValue, requestContext, operationInfo, propertyOperationFlags);

#ifdef CLONE_INLINECACHE_TO_EMPTYSLOT
        if (!result && !cache->IsEmpty())
        {TRACE_IT(48556);
            result = CheckClonedInlineCache(inlineCacheIndex, [&](uint tryInlineCacheIndex) -> bool
            {
                cache = &inlineCaches[tryInlineCacheIndex];
                return cache->TrySetProperty<CheckLocal, CheckLocalTypeWithoutProperty, CheckAccessor, ReturnOperationInfo>(
                    object, propertyId, propertyValue, requestContext, operationInfo, propertyOperationFlags);
            });
        }
#endif

        if (IsInlineCacheAvailable && result)
        {TRACE_IT(48557);
            cache->CopyTo(propertyId, requestContext, inlineCacheToPopulate);
        }

#ifdef INLINE_CACHE_STATS
        if (PHASE_STATS1(Js::PolymorphicInlineCachePhase))
        {TRACE_IT(48558);
            bool collision = !result && !isEmpty;
            this->functionBody->GetScriptContext()->LogCacheUsage(this, /*isGet*/ false, propertyId, result, collision);
        }
#endif

        return result;
    }
}
