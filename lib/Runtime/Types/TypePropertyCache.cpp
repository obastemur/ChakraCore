//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    // -------------------------------------------------------------------------------------------------------------------------
    // TypePropertyCacheElement
    // -------------------------------------------------------------------------------------------------------------------------

    TypePropertyCacheElement::TypePropertyCacheElement()
        : id(Constants::NoProperty), tag(1), index(0), prototypeObjectWithProperty(nullptr)
    {TRACE_IT(68025);
    }

    PropertyId TypePropertyCacheElement::Id() const
    {TRACE_IT(68026);
        return id;
    }

    PropertyIndex TypePropertyCacheElement::Index() const
    {TRACE_IT(68027);
        return index;
    }

    bool TypePropertyCacheElement::IsInlineSlot() const
    {TRACE_IT(68028);
        return isInlineSlot;
    }

    bool TypePropertyCacheElement::IsSetPropertyAllowed() const
    {TRACE_IT(68029);
        return isSetPropertyAllowed;
    }

    bool TypePropertyCacheElement::IsMissing() const
    {TRACE_IT(68030);
        return isMissing;
    }

    DynamicObject *TypePropertyCacheElement::PrototypeObjectWithProperty() const
    {TRACE_IT(68031);
        return prototypeObjectWithProperty;
    }

    void TypePropertyCacheElement::Cache(
        const PropertyId id,
        const PropertyIndex index,
        const bool isInlineSlot,
        const bool isSetPropertyAllowed)
    {TRACE_IT(68032);
        Assert(id != Constants::NoProperty);
        Assert(index != Constants::NoSlot);

        this->id = id;
        this->index = index;
        this->isInlineSlot = isInlineSlot;
        this->isSetPropertyAllowed = isSetPropertyAllowed;
        this->isMissing = false;
        this->prototypeObjectWithProperty = nullptr;
    }

    void TypePropertyCacheElement::Cache(
        const PropertyId id,
        const PropertyIndex index,
        const bool isInlineSlot,
        const bool isSetPropertyAllowed,
        const bool isMissing,
        DynamicObject *const prototypeObjectWithProperty,
        Type *const myParentType)
    {TRACE_IT(68033);
        Assert(id != Constants::NoProperty);
        Assert(index != Constants::NoSlot);
        Assert(prototypeObjectWithProperty);
        Assert(myParentType);

        if(this->id != id || !this->prototypeObjectWithProperty)
            myParentType->GetScriptContext()->GetThreadContext()->RegisterTypeWithProtoPropertyCache(id, myParentType);

        this->id = id;
        this->index = index;
        this->isInlineSlot = isInlineSlot;
        this->isSetPropertyAllowed = isSetPropertyAllowed;
        this->isMissing = isMissing;
        this->prototypeObjectWithProperty = prototypeObjectWithProperty;
        Assert(this->isMissing == (uint16)(this->prototypeObjectWithProperty == this->prototypeObjectWithProperty->GetLibrary()->GetMissingPropertyHolder()));
    }

    void TypePropertyCacheElement::Clear()
    {TRACE_IT(68034);
        id = Constants::NoProperty;
    }

    // -------------------------------------------------------------------------------------------------------------------------
    // TypePropertyCache
    // -------------------------------------------------------------------------------------------------------------------------

    size_t TypePropertyCache::ElementIndex(const PropertyId id)
    {TRACE_IT(68035);
        Assert(id != Constants::NoProperty);
        Assert((TypePropertyCache_NumElements & TypePropertyCache_NumElements - 1) == 0);

        return id & TypePropertyCache_NumElements - 1;
    }

    inline bool TypePropertyCache::TryGetIndexForLoad(
        const bool checkMissing,
        const PropertyId id,
        PropertyIndex *const index,
        bool *const isInlineSlot,
        bool *const isMissing,
        DynamicObject * *const prototypeObjectWithProperty) const
    {TRACE_IT(68036);
        Assert(index);
        Assert(isInlineSlot);
        Assert(isMissing);
        Assert(prototypeObjectWithProperty);

        const TypePropertyCacheElement &element = elements[ElementIndex(id)];
        if(element.Id() != id || (!checkMissing && element.IsMissing()))
            return false;

        *index = element.Index();
        *isInlineSlot = element.IsInlineSlot();
        *isMissing = checkMissing ? element.IsMissing() : false;
        *prototypeObjectWithProperty = element.PrototypeObjectWithProperty();
        return true;
    }

    inline bool TypePropertyCache::TryGetIndexForStore(
        const PropertyId id,
        PropertyIndex *const index,
        bool *const isInlineSlot) const
    {TRACE_IT(68037);
        Assert(index);
        Assert(isInlineSlot);

        const TypePropertyCacheElement &element = elements[ElementIndex(id)];
        if(element.Id() != id ||
            !element.IsSetPropertyAllowed() ||
            element.PrototypeObjectWithProperty())
        {TRACE_IT(68038);
            return false;
        }

        Assert(!element.IsMissing());
        *index = element.Index();
        *isInlineSlot = element.IsInlineSlot();
        return true;
    }

    bool TypePropertyCache::TryGetProperty(
        const bool checkMissing,
        RecyclableObject *const propertyObject,
        const PropertyId propertyId,
        Var *const propertyValue,
        ScriptContext *const requestContext,
        PropertyCacheOperationInfo *const operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {TRACE_IT(68039);
        Assert(propertyValueInfo);
        Assert(propertyValueInfo->GetInlineCache() || propertyValueInfo->GetPolymorphicInlineCache());

        PropertyIndex propertyIndex;
        DynamicObject *prototypeObjectWithProperty;
        bool isInlineSlot, isMissing;
        if(!TryGetIndexForLoad(
                checkMissing,
                propertyId,
                &propertyIndex,
                &isInlineSlot,
                &isMissing,
                &prototypeObjectWithProperty))
        {TRACE_IT(68040);
        #if DBG_DUMP
            if(PHASE_TRACE1(TypePropertyCachePhase))
            {TRACE_IT(68041);
                CacheOperators::TraceCache(
                    static_cast<InlineCache *>(nullptr),
                    _u("TypePropertyCache get miss"),
                    propertyId,
                    requestContext,
                    propertyObject);
            }
        #endif
            return false;
        }

        if(!prototypeObjectWithProperty)
        {TRACE_IT(68042);
        #if DBG_DUMP
            if(PHASE_TRACE1(TypePropertyCachePhase))
            {TRACE_IT(68043);
                CacheOperators::TraceCache(
                    static_cast<InlineCache *>(nullptr),
                    _u("TypePropertyCache get hit"),
                    propertyId,
                    requestContext,
                    propertyObject);
            }
        #endif

        #if DBG
            const PropertyIndex typeHandlerPropertyIndex =
                DynamicObject
                    ::FromVar(propertyObject)
                    ->GetDynamicType()
                    ->GetTypeHandler()
                    ->InlineOrAuxSlotIndexToPropertyIndex(propertyIndex, isInlineSlot);
            Assert(typeHandlerPropertyIndex == propertyObject->GetPropertyIndex(propertyId));
        #endif

            *propertyValue =
                isInlineSlot
                    ? DynamicObject::FromVar(propertyObject)->GetInlineSlot(propertyIndex)
                    : DynamicObject::FromVar(propertyObject)->GetAuxSlot(propertyIndex);
            if(propertyObject->GetScriptContext() == requestContext)
            {TRACE_IT(68044);
                Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext));

                CacheOperators::Cache<false, true, false>(
                    false,
                    DynamicObject::FromVar(propertyObject),
                    false,
                    propertyObject->GetType(),
                    nullptr,
                    propertyId,
                    propertyIndex,
                    isInlineSlot,
                    false,
                    0,
                    propertyValueInfo,
                    requestContext);
                return true;
            }

            *propertyValue = CrossSite::MarshalVar(requestContext, *propertyValue);
            // Cannot use GetProperty and compare results since they may not compare equal when they're marshaled

            if(operationInfo)
            {TRACE_IT(68045);
                operationInfo->cacheType = CacheType_TypeProperty;
                operationInfo->slotType = isInlineSlot ? SlotType_Inline : SlotType_Aux;
            }
            return true;
        }

    #if DBG_DUMP
        if(PHASE_TRACE1(TypePropertyCachePhase))
        {TRACE_IT(68046);
            CacheOperators::TraceCache(
                static_cast<InlineCache *>(nullptr),
                _u("TypePropertyCache get hit prototype"),
                propertyId,
                requestContext,
                propertyObject);
        }
    #endif

    #if DBG
        const PropertyIndex typeHandlerPropertyIndex =
            prototypeObjectWithProperty
                ->GetDynamicType()
                ->GetTypeHandler()
                ->InlineOrAuxSlotIndexToPropertyIndex(propertyIndex, isInlineSlot);
        Assert(typeHandlerPropertyIndex == prototypeObjectWithProperty->GetPropertyIndex(propertyId));
    #endif

        *propertyValue =
            isInlineSlot
                ? prototypeObjectWithProperty->GetInlineSlot(propertyIndex)
                : prototypeObjectWithProperty->GetAuxSlot(propertyIndex);
        if(prototypeObjectWithProperty->GetScriptContext() == requestContext)
        {TRACE_IT(68047);
            Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext));

            if(propertyObject->GetScriptContext() != requestContext)
            {TRACE_IT(68048);
                return true;
            }

            CacheOperators::Cache<false, true, false>(
                true,
                prototypeObjectWithProperty,
                false,
                propertyObject->GetType(),
                nullptr,
                propertyId,
                propertyIndex,
                isInlineSlot,
                isMissing,
                0,
                propertyValueInfo,
                requestContext);
            return true;
        }

        *propertyValue = CrossSite::MarshalVar(requestContext, *propertyValue);
        // Cannot use GetProperty and compare results since they may not compare equal when they're marshaled

        if(operationInfo)
        {TRACE_IT(68049);
            operationInfo->cacheType = CacheType_TypeProperty;
            operationInfo->slotType = isInlineSlot ? SlotType_Inline : SlotType_Aux;
        }
        return true;
    }

    bool TypePropertyCache::TrySetProperty(
        RecyclableObject *const object,
        const PropertyId propertyId,
        Var propertyValue,
        ScriptContext *const requestContext,
        PropertyCacheOperationInfo *const operationInfo,
        PropertyValueInfo *const propertyValueInfo)
    {TRACE_IT(68050);
        Assert(propertyValueInfo);
        Assert(propertyValueInfo->GetInlineCache() || propertyValueInfo->GetPolymorphicInlineCache());

        PropertyIndex propertyIndex;
        bool isInlineSlot;
        if(!TryGetIndexForStore(propertyId, &propertyIndex, &isInlineSlot))
        {TRACE_IT(68051);
        #if DBG_DUMP
            if(PHASE_TRACE1(TypePropertyCachePhase))
            {TRACE_IT(68052);
                CacheOperators::TraceCache(
                    static_cast<InlineCache *>(nullptr),
                    _u("TypePropertyCache set miss"),
                    propertyId,
                    requestContext,
                    object);
            }
        #endif
            return false;
        }

    #if DBG_DUMP
        if(PHASE_TRACE1(TypePropertyCachePhase))
        {TRACE_IT(68053);
            CacheOperators::TraceCache(
                static_cast<InlineCache *>(nullptr),
                _u("TypePropertyCache set hit"),
                propertyId,
                requestContext,
                object);
        }
    #endif

        Assert(!object->IsFixedProperty(propertyId));
        Assert(
            (
                DynamicObject
                    ::FromVar(object)
                    ->GetDynamicType()
                    ->GetTypeHandler()
                    ->InlineOrAuxSlotIndexToPropertyIndex(propertyIndex, isInlineSlot)
            ) ==
            object->GetPropertyIndex(propertyId));
        Assert(object->CanStorePropertyValueDirectly(propertyId, false));

        ScriptContext *const objectScriptContext = object->GetScriptContext();
        if(objectScriptContext != requestContext)
        {TRACE_IT(68054);
            propertyValue = CrossSite::MarshalVar(objectScriptContext, propertyValue);
        }

        if(isInlineSlot)
        {TRACE_IT(68055);
            DynamicObject::FromVar(object)->SetInlineSlot(SetSlotArguments(propertyId, propertyIndex, propertyValue));
        }
        else
        {TRACE_IT(68056);
            DynamicObject::FromVar(object)->SetAuxSlot(SetSlotArguments(propertyId, propertyIndex, propertyValue));
        }

        if(objectScriptContext == requestContext)
        {TRACE_IT(68057);
            CacheOperators::Cache<false, false, false>(
                false,
                DynamicObject::FromVar(object),
                false,
                object->GetType(),
                nullptr,
                propertyId,
                propertyIndex,
                isInlineSlot,
                false,
                0,
                propertyValueInfo,
                requestContext);
            return true;
        }

        if(operationInfo)
        {TRACE_IT(68058);
            operationInfo->cacheType = CacheType_TypeProperty;
            operationInfo->slotType = isInlineSlot ? SlotType_Inline : SlotType_Aux;
        }
        return true;
    }

    void TypePropertyCache::Cache(
        const PropertyId id,
        const PropertyIndex index,
        const bool isInlineSlot,
        const bool isSetPropertyAllowed)
    {TRACE_IT(68059);
        elements[ElementIndex(id)].Cache(id, index, isInlineSlot, isSetPropertyAllowed);
    }

    void TypePropertyCache::Cache(
        const PropertyId id,
        const PropertyIndex index,
        const bool isInlineSlot,
        const bool isSetPropertyAllowed,
        const bool isMissing,
        DynamicObject *const prototypeObjectWithProperty,
        Type *const myParentType)
    {TRACE_IT(68060);
        Assert(myParentType);
        Assert(myParentType->GetPropertyCache() == this);

        elements[ElementIndex(id)].Cache(
            id,
            index,
            isInlineSlot,
            isSetPropertyAllowed,
            isMissing,
            prototypeObjectWithProperty,
            myParentType);
    }

    void TypePropertyCache::ClearIfPropertyIsOnAPrototype(const PropertyId id)
    {TRACE_IT(68061);
        TypePropertyCacheElement &element = elements[ElementIndex(id)];
        if(element.Id() == id && element.PrototypeObjectWithProperty())
            element.Clear();
    }

    void TypePropertyCache::Clear(const PropertyId id)
    {TRACE_IT(68062);
        TypePropertyCacheElement &element = elements[ElementIndex(id)];
        if(element.Id() == id)
            element.Clear();
    }
}
