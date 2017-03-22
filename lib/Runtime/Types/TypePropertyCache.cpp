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
    {LOGMEIN("TypePropertyCache.cpp] 14\n");
    }

    PropertyId TypePropertyCacheElement::Id() const
    {LOGMEIN("TypePropertyCache.cpp] 18\n");
        return id;
    }

    PropertyIndex TypePropertyCacheElement::Index() const
    {LOGMEIN("TypePropertyCache.cpp] 23\n");
        return index;
    }

    bool TypePropertyCacheElement::IsInlineSlot() const
    {LOGMEIN("TypePropertyCache.cpp] 28\n");
        return isInlineSlot;
    }

    bool TypePropertyCacheElement::IsSetPropertyAllowed() const
    {LOGMEIN("TypePropertyCache.cpp] 33\n");
        return isSetPropertyAllowed;
    }

    bool TypePropertyCacheElement::IsMissing() const
    {LOGMEIN("TypePropertyCache.cpp] 38\n");
        return isMissing;
    }

    DynamicObject *TypePropertyCacheElement::PrototypeObjectWithProperty() const
    {LOGMEIN("TypePropertyCache.cpp] 43\n");
        return prototypeObjectWithProperty;
    }

    void TypePropertyCacheElement::Cache(
        const PropertyId id,
        const PropertyIndex index,
        const bool isInlineSlot,
        const bool isSetPropertyAllowed)
    {LOGMEIN("TypePropertyCache.cpp] 52\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 72\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 91\n");
        id = Constants::NoProperty;
    }

    // -------------------------------------------------------------------------------------------------------------------------
    // TypePropertyCache
    // -------------------------------------------------------------------------------------------------------------------------

    size_t TypePropertyCache::ElementIndex(const PropertyId id)
    {LOGMEIN("TypePropertyCache.cpp] 100\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 114\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 135\n");
        Assert(index);
        Assert(isInlineSlot);

        const TypePropertyCacheElement &element = elements[ElementIndex(id)];
        if(element.Id() != id ||
            !element.IsSetPropertyAllowed() ||
            element.PrototypeObjectWithProperty())
        {LOGMEIN("TypePropertyCache.cpp] 143\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 161\n");
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
        {LOGMEIN("TypePropertyCache.cpp] 175\n");
        #if DBG_DUMP
            if(PHASE_TRACE1(TypePropertyCachePhase))
            {LOGMEIN("TypePropertyCache.cpp] 178\n");
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
        {LOGMEIN("TypePropertyCache.cpp] 191\n");
        #if DBG_DUMP
            if(PHASE_TRACE1(TypePropertyCachePhase))
            {LOGMEIN("TypePropertyCache.cpp] 194\n");
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
            {LOGMEIN("TypePropertyCache.cpp] 219\n");
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
            {LOGMEIN("TypePropertyCache.cpp] 242\n");
                operationInfo->cacheType = CacheType_TypeProperty;
                operationInfo->slotType = isInlineSlot ? SlotType_Inline : SlotType_Aux;
            }
            return true;
        }

    #if DBG_DUMP
        if(PHASE_TRACE1(TypePropertyCachePhase))
        {LOGMEIN("TypePropertyCache.cpp] 251\n");
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
        {LOGMEIN("TypePropertyCache.cpp] 275\n");
            Assert(*propertyValue == JavascriptOperators::GetProperty(propertyObject, propertyId, requestContext));

            if(propertyObject->GetScriptContext() != requestContext)
            {LOGMEIN("TypePropertyCache.cpp] 279\n");
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
        {LOGMEIN("TypePropertyCache.cpp] 303\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 317\n");
        Assert(propertyValueInfo);
        Assert(propertyValueInfo->GetInlineCache() || propertyValueInfo->GetPolymorphicInlineCache());

        PropertyIndex propertyIndex;
        bool isInlineSlot;
        if(!TryGetIndexForStore(propertyId, &propertyIndex, &isInlineSlot))
        {LOGMEIN("TypePropertyCache.cpp] 324\n");
        #if DBG_DUMP
            if(PHASE_TRACE1(TypePropertyCachePhase))
            {LOGMEIN("TypePropertyCache.cpp] 327\n");
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
        {LOGMEIN("TypePropertyCache.cpp] 341\n");
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
        {LOGMEIN("TypePropertyCache.cpp] 365\n");
            propertyValue = CrossSite::MarshalVar(objectScriptContext, propertyValue);
        }

        if(isInlineSlot)
        {LOGMEIN("TypePropertyCache.cpp] 370\n");
            DynamicObject::FromVar(object)->SetInlineSlot(SetSlotArguments(propertyId, propertyIndex, propertyValue));
        }
        else
        {
            DynamicObject::FromVar(object)->SetAuxSlot(SetSlotArguments(propertyId, propertyIndex, propertyValue));
        }

        if(objectScriptContext == requestContext)
        {LOGMEIN("TypePropertyCache.cpp] 379\n");
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
        {LOGMEIN("TypePropertyCache.cpp] 397\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 409\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 421\n");
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
    {LOGMEIN("TypePropertyCache.cpp] 436\n");
        TypePropertyCacheElement &element = elements[ElementIndex(id)];
        if(element.Id() == id && element.PrototypeObjectWithProperty())
            element.Clear();
    }

    void TypePropertyCache::Clear(const PropertyId id)
    {LOGMEIN("TypePropertyCache.cpp] 443\n");
        TypePropertyCacheElement &element = elements[ElementIndex(id)];
        if(element.Id() == id)
            element.Clear();
    }
}
