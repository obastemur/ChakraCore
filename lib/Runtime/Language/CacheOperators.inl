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
        bool CheckPolymorphicInlineCache,
        bool CheckTypePropertyCache,
        bool IsInlineCacheAvailable,
        bool IsPolymorphicInlineCacheAvailable,
        bool ReturnOperationInfo>
    inline bool CacheOperators::TryGetProperty(
        Var const instance,
        const bool isRoot,
        RecyclableObject *const object,
        const PropertyId propertyId,
        Var *const propertyValue,
        ScriptContext *const requestContext,
        PropertyCacheOperationInfo * operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {TRACE_IT(47419);
        CompileAssert(IsInlineCacheAvailable || IsPolymorphicInlineCacheAvailable);
        Assert(!CheckTypePropertyCache || !isRoot);
        Assert(propertyValueInfo);
        Assert(IsInlineCacheAvailable == !!propertyValueInfo->GetInlineCache());
        Assert(IsPolymorphicInlineCacheAvailable == !!propertyValueInfo->GetPolymorphicInlineCache());
        Assert(!ReturnOperationInfo || operationInfo);

        if(CheckLocal || CheckProto || CheckAccessor)
        {TRACE_IT(47420);
            InlineCache *const inlineCache = IsInlineCacheAvailable ? propertyValueInfo->GetInlineCache() : nullptr;
            if(IsInlineCacheAvailable)
            {TRACE_IT(47421);
                if (inlineCache->TryGetProperty<CheckLocal, CheckProto, CheckAccessor, CheckMissing, ReturnOperationInfo>(
                        instance,
                        object,
                        propertyId,
                        propertyValue,
                        requestContext,
                        operationInfo))
                {TRACE_IT(47422);
                    return true;
                }
                if(ReturnOperationInfo)
                {TRACE_IT(47423);
                    operationInfo->isPolymorphic = inlineCache->HasDifferentType(object->GetType());
                }
            }
            else if(ReturnOperationInfo)
            {TRACE_IT(47424);
                operationInfo->isPolymorphic = true;
            }

            if(CheckPolymorphicInlineCache)
            {TRACE_IT(47425);
                Assert(IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetFunctionBody());
                PolymorphicInlineCache *const polymorphicInlineCache =
                    IsPolymorphicInlineCacheAvailable
                        ?   propertyValueInfo->GetPolymorphicInlineCache()
                        :   propertyValueInfo->GetFunctionBody()->GetPolymorphicInlineCache(
                                propertyValueInfo->GetInlineCacheIndex());
                if ((IsPolymorphicInlineCacheAvailable || polymorphicInlineCache) &&
                    polymorphicInlineCache->TryGetProperty<
                            CheckLocal,
                            CheckProto,
                            CheckAccessor,
                            CheckMissing,
                            IsInlineCacheAvailable,
                            ReturnOperationInfo
                        >(
                            instance,
                            object,
                            propertyId,
                            propertyValue,
                            requestContext,
                            operationInfo,
                            inlineCache
                        ))
                {TRACE_IT(47426);
                    return true;
                }
            }
        }

        if(!CheckTypePropertyCache)
        {TRACE_IT(47427);
            return false;
        }

        TypePropertyCache *const typePropertyCache = object->GetType()->GetPropertyCache();
        if(!typePropertyCache ||
            !typePropertyCache->TryGetProperty(
                    CheckMissing,
                    object,
                    propertyId,
                    propertyValue,
                    requestContext,
                    ReturnOperationInfo ? operationInfo : nullptr,
                    propertyValueInfo))
        {TRACE_IT(47428);
            return false;
        }

        if(!ReturnOperationInfo || operationInfo->cacheType == CacheType_TypeProperty)
        {TRACE_IT(47429);
            return true;
        }

        // The property access was cached in an inline cache. Get the proper property operation info.
        PretendTryGetProperty<IsInlineCacheAvailable, IsPolymorphicInlineCacheAvailable>(
            object->GetType(),
            operationInfo,
            propertyValueInfo);
        return true;
    }

    template<
        bool CheckLocal,
        bool CheckLocalTypeWithoutProperty,
        bool CheckAccessor,
        bool CheckPolymorphicInlineCache,
        bool CheckTypePropertyCache,
        bool IsInlineCacheAvailable,
        bool IsPolymorphicInlineCacheAvailable,
        bool ReturnOperationInfo>
    inline bool CacheOperators::TrySetProperty(
        RecyclableObject *const object,
        const bool isRoot,
        const PropertyId propertyId,
        Var propertyValue,
        ScriptContext *const requestContext,
        const PropertyOperationFlags propertyOperationFlags,
        PropertyCacheOperationInfo * operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {TRACE_IT(47430);
        CompileAssert(IsInlineCacheAvailable || IsPolymorphicInlineCacheAvailable);
        Assert(!CheckTypePropertyCache || !isRoot);
        Assert(propertyValueInfo);
        Assert(IsInlineCacheAvailable == !!propertyValueInfo->GetInlineCache());
        Assert(IsPolymorphicInlineCacheAvailable == !!propertyValueInfo->GetPolymorphicInlineCache());
        Assert(!ReturnOperationInfo || operationInfo);

        if(CheckLocal || CheckLocalTypeWithoutProperty || CheckAccessor)
        {TRACE_IT(47431);
            InlineCache *const inlineCache = IsInlineCacheAvailable ? propertyValueInfo->GetInlineCache() : nullptr;
            if(IsInlineCacheAvailable)
            {TRACE_IT(47432);
                if (inlineCache->TrySetProperty<CheckLocal, CheckLocalTypeWithoutProperty, CheckAccessor, ReturnOperationInfo>(
                        object,
                        propertyId,
                        propertyValue,
                        requestContext,
                        operationInfo,
                        propertyOperationFlags))
                {TRACE_IT(47433);
                    return true;
                }
                if(ReturnOperationInfo)
                {TRACE_IT(47434);
                    operationInfo->isPolymorphic = inlineCache->HasDifferentType(object->GetType());
                }
            }
            else if(ReturnOperationInfo)
            {TRACE_IT(47435);
                operationInfo->isPolymorphic = true;
            }

            if(CheckPolymorphicInlineCache)
            {TRACE_IT(47436);
                Assert(IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetFunctionBody());
                PolymorphicInlineCache *const polymorphicInlineCache =
                    IsPolymorphicInlineCacheAvailable
                        ?   propertyValueInfo->GetPolymorphicInlineCache()
                        :   propertyValueInfo->GetFunctionBody()->GetPolymorphicInlineCache(
                                propertyValueInfo->GetInlineCacheIndex());
                if ((IsPolymorphicInlineCacheAvailable || polymorphicInlineCache) &&
                    polymorphicInlineCache->TrySetProperty<
                            CheckLocal,
                            CheckLocalTypeWithoutProperty,
                            CheckAccessor,
                            IsInlineCacheAvailable,
                            ReturnOperationInfo
                        >(
                            object,
                            propertyId,
                            propertyValue,
                            requestContext,
                            operationInfo,
                            inlineCache,
                            propertyOperationFlags
                        ))
                {TRACE_IT(47437);
                    return true;
                }
            }
        }

        if(!CheckTypePropertyCache)
        {TRACE_IT(47438);
            return false;
        }

        TypePropertyCache *const typePropertyCache = object->GetType()->GetPropertyCache();
        if(!typePropertyCache ||
            !typePropertyCache->TrySetProperty(
                object,
                propertyId,
                propertyValue,
                requestContext,
                ReturnOperationInfo ? operationInfo : nullptr,
                propertyValueInfo))
        {TRACE_IT(47439);
            return false;
        }

        if(!ReturnOperationInfo || operationInfo->cacheType == CacheType_TypeProperty)
        {TRACE_IT(47440);
            return true;
        }

        // The property access was cached in an inline cache. Get the proper property operation info.
        PretendTrySetProperty<IsInlineCacheAvailable, IsPolymorphicInlineCacheAvailable>(
            object->GetType(),
            object->GetType(),
            operationInfo,
            propertyValueInfo);
        return true;
    }

    template<
        bool IsInlineCacheAvailable,
        bool IsPolymorphicInlineCacheAvailable>
    inline void CacheOperators::PretendTryGetProperty(
        Type *const type,
        PropertyCacheOperationInfo *operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {TRACE_IT(47441);
        CompileAssert(IsInlineCacheAvailable || IsPolymorphicInlineCacheAvailable);
        Assert(propertyValueInfo);
        Assert(IsInlineCacheAvailable == !!propertyValueInfo->GetInlineCache());
        Assert(!IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetPolymorphicInlineCache());
        Assert(operationInfo);

        if (IsInlineCacheAvailable && propertyValueInfo->GetInlineCache()->PretendTryGetProperty(type, operationInfo))
        {TRACE_IT(47442);
            return;
        }

        Assert(IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetFunctionBody());
        PolymorphicInlineCache *const polymorphicInlineCache =
            IsPolymorphicInlineCacheAvailable
                ? propertyValueInfo->GetPolymorphicInlineCache()
                : propertyValueInfo->GetFunctionBody()->GetPolymorphicInlineCache(propertyValueInfo->GetInlineCacheIndex());
        if (IsPolymorphicInlineCacheAvailable || polymorphicInlineCache)
        {TRACE_IT(47443);
            polymorphicInlineCache->PretendTryGetProperty(type, operationInfo);
        }
    }

    template<
        bool IsInlineCacheAvailable,
        bool IsPolymorphicInlineCacheAvailable>
    inline void CacheOperators::PretendTrySetProperty(
        Type *const type,
        Type *const oldType,
        PropertyCacheOperationInfo * operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {TRACE_IT(47444);
        CompileAssert(IsInlineCacheAvailable || IsPolymorphicInlineCacheAvailable);
        Assert(propertyValueInfo);
        Assert(IsInlineCacheAvailable == !!propertyValueInfo->GetInlineCache());
        Assert(!IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetPolymorphicInlineCache());
        Assert(operationInfo);

        if (IsInlineCacheAvailable && propertyValueInfo->GetInlineCache()->PretendTrySetProperty(type, oldType, operationInfo))
        {TRACE_IT(47445);
            return;
        }

        Assert(IsPolymorphicInlineCacheAvailable || propertyValueInfo->GetFunctionBody());
        PolymorphicInlineCache *const polymorphicInlineCache =
            IsPolymorphicInlineCacheAvailable
                ? propertyValueInfo->GetPolymorphicInlineCache()
                : propertyValueInfo->GetFunctionBody()->GetPolymorphicInlineCache(propertyValueInfo->GetInlineCacheIndex());
        if (IsPolymorphicInlineCacheAvailable || polymorphicInlineCache)
        {TRACE_IT(47446);
            polymorphicInlineCache->PretendTrySetProperty(type, oldType, operationInfo);
        }
    }

    template<
        bool IsAccessor,
        bool IsRead,
        bool IncludeTypePropertyCache>
    inline void CacheOperators::Cache(
        const bool isProto,
        DynamicObject *const objectWithProperty,
        const bool isRoot,
        Type *const type,
        Type *const typeWithoutProperty,
        const PropertyId propertyId,
        const PropertyIndex propertyIndex,
        const bool isInlineSlot,
        const bool isMissing,
        const int requiredAuxSlotCapacity,
        const PropertyValueInfo *const info,
        ScriptContext *const requestContext)
    {TRACE_IT(47447);
        CompileAssert(!IsAccessor || !IncludeTypePropertyCache);
        Assert(info);
        Assert(objectWithProperty);

        if(!IsAccessor)
        {TRACE_IT(47448);
            if(!isProto)
            {TRACE_IT(47449);
                Assert(type == objectWithProperty->GetType());
            }
            else
            {TRACE_IT(47450);
                Assert(IsRead);
                Assert(type != objectWithProperty->GetType());
            }
        }
        else
        {TRACE_IT(47451);
            Assert(!isRoot); // could still be root object, but the parameter will be false and shouldn't be used for accessors
            Assert(!typeWithoutProperty);
            Assert(requiredAuxSlotCapacity == 0);
        }

        if(IsRead)
        {TRACE_IT(47452);
            Assert(!typeWithoutProperty);
            Assert(requiredAuxSlotCapacity == 0);
            Assert(CanCachePropertyRead(objectWithProperty, requestContext));

            if(!IsAccessor && isProto && PropertyValueInfo::PrototypeCacheDisabled(info))
            {TRACE_IT(47453);
                return;
            }
        }
        else
        {
            Assert(CanCachePropertyWrite(objectWithProperty, requestContext));

            // TODO(ianhall): the following assert would let global const properties slip through when they shadow
            // a global property. Reason being DictionaryTypeHandler::IsWritable cannot tell if it should check
            // the global property or the global let/const.  Fix this by updating IsWritable to recognize isRoot.

            // Built-in Function.prototype properties 'length', 'arguments', and 'caller' are special cases.
            Assert(
                objectWithProperty->IsWritable(propertyId) ||
                (isRoot && RootObjectBase::FromVar(objectWithProperty)->IsLetConstGlobal(propertyId)) ||
                JavascriptFunction::IsBuiltinProperty(objectWithProperty, propertyId));
        }

        const bool includeTypePropertyCache = IncludeTypePropertyCache && !isRoot;
        bool createTypePropertyCache = false;
        PolymorphicInlineCache *polymorphicInlineCache = info->GetPolymorphicInlineCache();
        if(!polymorphicInlineCache && info->GetFunctionBody())
        {TRACE_IT(47454);
            polymorphicInlineCache = info->GetFunctionBody()->GetPolymorphicInlineCache(info->GetInlineCacheIndex());
        }
        InlineCache *const inlineCache = info->GetInlineCache();
        if(inlineCache)
        {TRACE_IT(47455);
            const bool tryCreatePolymorphicInlineCache = !polymorphicInlineCache && info->GetFunctionBody();
            if((includeTypePropertyCache || tryCreatePolymorphicInlineCache) &&
                inlineCache->HasDifferentType<IsAccessor>(isProto, type, typeWithoutProperty))
            {TRACE_IT(47456);
                if(tryCreatePolymorphicInlineCache)
                {TRACE_IT(47457);
                    polymorphicInlineCache =
                        info->GetFunctionBody()->CreateNewPolymorphicInlineCache(
                            info->GetInlineCacheIndex(),
                            propertyId,
                            inlineCache);
                }
                if(includeTypePropertyCache)
                {TRACE_IT(47458);
                    createTypePropertyCache = true;
                }
            }

            if(!IsAccessor)
            {TRACE_IT(47459);
                if(!isProto)
                {TRACE_IT(47460);
                    inlineCache->CacheLocal(
                        type,
                        propertyId,
                        propertyIndex,
                        isInlineSlot,
                        typeWithoutProperty,
                        requiredAuxSlotCapacity,
                        requestContext);
                }
                else
                {TRACE_IT(47461);
                    inlineCache->CacheProto(
                        objectWithProperty,
                        propertyId,
                        propertyIndex,
                        isInlineSlot,
                        isMissing,
                        type,
                        requestContext);
                }
            }
            else
            {TRACE_IT(47462);
                inlineCache->CacheAccessor(
                    IsRead,
                    propertyId,
                    propertyIndex,
                    isInlineSlot,
                    type,
                    objectWithProperty,
                    isProto,
                    requestContext);
            }
        }

        if(polymorphicInlineCache)
        {TRACE_IT(47463);
            // Don't resize a polymorphic inline cache from full JIT because it currently doesn't rejit to use the new
            // polymorphic inline cache. Once resized, bailouts would populate only the new set of caches and full JIT would
            // continue to use to old set of caches.
            Assert(!info->AllowResizingPolymorphicInlineCache() || info->GetFunctionBody());
            if(((includeTypePropertyCache && !createTypePropertyCache) || info->AllowResizingPolymorphicInlineCache()) &&
                polymorphicInlineCache->HasDifferentType<IsAccessor>(isProto, type, typeWithoutProperty))
            {TRACE_IT(47464);
                if(info->AllowResizingPolymorphicInlineCache() && polymorphicInlineCache->CanAllocateBigger())
                {TRACE_IT(47465);
                    polymorphicInlineCache =
                        info->GetFunctionBody()->CreateBiggerPolymorphicInlineCache(
                            info->GetInlineCacheIndex(),
                            propertyId);
                }
                if(includeTypePropertyCache)
                {TRACE_IT(47466);
                    createTypePropertyCache = true;
                }
            }

            if(!IsAccessor)
            {TRACE_IT(47467);
                if(!isProto)
                {TRACE_IT(47468);
                    polymorphicInlineCache->CacheLocal(
                        type,
                        propertyId,
                        propertyIndex,
                        isInlineSlot,
                        typeWithoutProperty,
                        requiredAuxSlotCapacity,
                        requestContext);
                }
                else
                {TRACE_IT(47469);
                    polymorphicInlineCache->CacheProto(
                        objectWithProperty,
                        propertyId,
                        propertyIndex,
                        isInlineSlot,
                        isMissing,
                        type,
                        requestContext);
                }
            }
            else
            {TRACE_IT(47470);
                polymorphicInlineCache->CacheAccessor(
                    IsRead,
                    propertyId,
                    propertyIndex,
                    isInlineSlot,
                    type,
                    objectWithProperty,
                    isProto,
                    requestContext);
            }
        }

        if(!includeTypePropertyCache)
        {TRACE_IT(47471);
            return;
        }
        Assert(!IsAccessor);

        TypePropertyCache *typePropertyCache = type->GetPropertyCache();
        if(!typePropertyCache)
        {TRACE_IT(47472);
            if(!createTypePropertyCache)
            {TRACE_IT(47473);
                return;
            }
            typePropertyCache = type->CreatePropertyCache();
        }

        if(isProto)
        {TRACE_IT(47474);
            typePropertyCache->Cache(
                propertyId,
                propertyIndex,
                isInlineSlot,
                info->IsWritable() && info->IsStoreFieldCacheEnabled(),
                isMissing,
                objectWithProperty,
                type);

            typePropertyCache = objectWithProperty->GetType()->GetPropertyCache();
            if(!typePropertyCache)
            {TRACE_IT(47475);
                return;
            }
        }

        typePropertyCache->Cache(
            propertyId,
            propertyIndex,
            isInlineSlot,
            info->IsWritable() && info->IsStoreFieldCacheEnabled());
    }
}
