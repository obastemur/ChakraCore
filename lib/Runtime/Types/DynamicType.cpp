//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(DynamicType);
    DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(DynamicType);

    DynamicType::DynamicType(DynamicType * type, DynamicTypeHandler *typeHandler, bool isLocked, bool isShared)
        : Type(type), typeHandler(typeHandler), isLocked(isLocked), isShared(isShared)
#if DBG
        , isCachedForChangePrototype(false)
#endif
    {
        Assert(!this->isLocked || this->typeHandler->GetIsLocked());
        Assert(!this->isShared || this->typeHandler->GetIsShared());
    }


    DynamicType::DynamicType(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint, DynamicTypeHandler * typeHandler, bool isLocked, bool isShared)
        : Type(scriptContext, typeId, prototype, entryPoint) , typeHandler(typeHandler), isLocked(isLocked), isShared(isShared), hasNoEnumerableProperties(false)
#if DBG
        , isCachedForChangePrototype(false)
#endif
    {
        Assert(typeHandler != nullptr);
        Assert(!this->isLocked || this->typeHandler->GetIsLocked());
        Assert(!this->isShared || this->typeHandler->GetIsShared());
    }

    DynamicType *
    DynamicType::New(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint, DynamicTypeHandler * typeHandler, bool isLocked, bool isShared)
    {TRACE_IT(66029);
        return RecyclerNew(scriptContext->GetRecycler(), DynamicType, scriptContext, typeId, prototype, entryPoint, typeHandler, isLocked, isShared);
    }

    bool
    DynamicType::Is(TypeId typeId)
    {TRACE_IT(66030);
        return !StaticType::Is(typeId);
    }

    bool
    DynamicType::SetHasNoEnumerableProperties(bool value)
    {TRACE_IT(66031);
        if (!value)
        {TRACE_IT(66032);
            this->hasNoEnumerableProperties = value;
            return false;
        }

#if DEBUG
        PropertyIndex propertyIndex = (PropertyIndex)-1;
        JavascriptString* propertyString = nullptr;
        PropertyId propertyId = Constants::NoProperty;
        Assert(!this->GetTypeHandler()->FindNextProperty(this->GetScriptContext(), propertyIndex, &propertyString, &propertyId, nullptr, this, this, EnumeratorFlags::None));
#endif

        this->hasNoEnumerableProperties = true;
        return true;
    }

    bool DynamicType::PrepareForTypeSnapshotEnumeration()
    {TRACE_IT(66033);
        if (CONFIG_FLAG(TypeSnapshotEnumeration))
        {TRACE_IT(66034);
            // Lock the type and handler, enabling us to enumerate properties of the type snapshotted
            // at the beginning of enumeration, despite property changes made by script during enumeration.
            return LockType(); // Note: this only works for type handlers that support locking.
        }
        return false;
    }

    void DynamicObject::InitSlots(DynamicObject* instance)
    {
        InitSlots(instance, GetScriptContext());
    }

    void DynamicObject::InitSlots(DynamicObject * instance, ScriptContext * scriptContext)
    {TRACE_IT(66035);
        Recycler * recycler = scriptContext->GetRecycler();
        int slotCapacity = GetTypeHandler()->GetSlotCapacity();
        int inlineSlotCapacity = GetTypeHandler()->GetInlineSlotCapacity();
        if (slotCapacity > inlineSlotCapacity)
        {TRACE_IT(66036);
            instance->auxSlots = RecyclerNewArrayZ(recycler, Field(Var), slotCapacity - inlineSlotCapacity);
        }
    }

    int DynamicObject::GetPropertyCount()
    {TRACE_IT(66037);
        if (!this->GetTypeHandler()->EnsureObjectReady(this))
        {TRACE_IT(66038);
            return 0;
        }
        return GetTypeHandler()->GetPropertyCount();
    }

    PropertyId DynamicObject::GetPropertyId(PropertyIndex index)
    {TRACE_IT(66039);
        return GetTypeHandler()->GetPropertyId(this->GetScriptContext(), index);
    }

    PropertyId DynamicObject::GetPropertyId(BigPropertyIndex index)
    {TRACE_IT(66040);
        return GetTypeHandler()->GetPropertyId(this->GetScriptContext(), index);
    }

    PropertyIndex DynamicObject::GetPropertyIndex(PropertyId propertyId)
    {TRACE_IT(66041);
        Assert(!Js::IsInternalPropertyId(propertyId));
        Assert(propertyId != Constants::NoProperty);
        return GetTypeHandler()->GetPropertyIndex(this->GetScriptContext()->GetPropertyName(propertyId));
    }

    BOOL DynamicObject::HasProperty(PropertyId propertyId)
    {TRACE_IT(66042);
        // HasProperty can be invoked with propertyId = NoProperty in some cases, namely cross-thread and DOM
        // This is done to force creation of a type handler in case the type handler is deferred
        Assert(!Js::IsInternalPropertyId(propertyId) || propertyId == Js::Constants::NoProperty);
        return GetTypeHandler()->HasProperty(this, propertyId);
    }

    // HasOwnProperty and HasProperty is the same for most objects except globalobject (moduleroot as well in legacy)
    // Note that in GlobalObject, HasProperty and HasRootProperty is not quite the same as it's handling let/const global etc.
    BOOL DynamicObject::HasOwnProperty(PropertyId propertyId)
    {TRACE_IT(66043);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return HasProperty(propertyId);
    }

    BOOL DynamicObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66044);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL DynamicObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66045);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetProperty");

        return GetTypeHandler()->GetProperty(this, originalInstance, propertyNameString, value, info, requestContext);
    }

    BOOL DynamicObject::GetInternalProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66046);
        Assert(Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetProperty(this, originalInstance, propertyId, value, nullptr, requestContext);
    }

    BOOL DynamicObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66047);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL DynamicObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66048);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->SetProperty(this, propertyId, value, flags, info);
    }

    BOOL DynamicObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66049);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling SetProperty");

        return GetTypeHandler()->SetProperty(this, propertyNameString, value, flags, info);
    }

    BOOL DynamicObject::SetInternalProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66050);
        Assert(Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->SetProperty(this, propertyId, value, flags, nullptr);
    }

    DescriptorFlags DynamicObject::GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66051);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetSetter(this, propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags DynamicObject::GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66052);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord* before calling GetSetter");

        return GetTypeHandler()->GetSetter(this, propertyNameString, setterValue, info, requestContext);
    }

    BOOL DynamicObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66053);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->InitProperty(this, propertyId, value, flags, info);
    }

    BOOL DynamicObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(66054);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->DeleteProperty(this, propertyId, flags);
    }

    BOOL DynamicObject::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(66055);
        return GetTypeHandler()->DeleteProperty(this, propertyNameString, flags);
    }

    BOOL DynamicObject::IsFixedProperty(PropertyId propertyId)
    {TRACE_IT(66056);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->IsFixedProperty(this, propertyId);
    }

    BOOL DynamicObject::HasItem(uint32 index)
    {TRACE_IT(66057);
        return GetTypeHandler()->HasItem(this, index);
    }

    BOOL DynamicObject::HasOwnItem(uint32 index)
    {TRACE_IT(66058);
        return HasItem(index);
    }

    BOOL DynamicObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(66059);
        return GetTypeHandler()->GetItem(this, originalInstance, index, value, requestContext);
    }

    BOOL DynamicObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(66060);
        return GetTypeHandler()->GetItem(this, originalInstance, index, value, requestContext);
    }

    DescriptorFlags DynamicObject::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {TRACE_IT(66061);
        return GetTypeHandler()->GetItemSetter(this, index, setterValue, requestContext);
    }

    BOOL DynamicObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(66062);
        return GetTypeHandler()->SetItem(this, index, value, flags);
    }

    BOOL DynamicObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(66063);
        return GetTypeHandler()->DeleteItem(this, index, flags);
    }

    BOOL DynamicObject::ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext)
    {TRACE_IT(66064);
        if(hint == JavascriptHint::HintString)
        {TRACE_IT(66065);
            return ToPrimitiveImpl<PropertyIds::toString>(result, requestContext)
                || ToPrimitiveImpl<PropertyIds::valueOf>(result, requestContext);
        }
        else
        {TRACE_IT(66066);
            Assert(hint == JavascriptHint::None || hint == JavascriptHint::HintNumber);
            return ToPrimitiveImpl<PropertyIds::valueOf>(result, requestContext)
                || ToPrimitiveImpl<PropertyIds::toString>(result, requestContext);

        }
    }

    template <PropertyId propertyId>
    BOOL DynamicObject::ToPrimitiveImpl(Var* result, ScriptContext * requestContext)
    {TRACE_IT(66067);
        CompileAssert(propertyId == PropertyIds::valueOf || propertyId == PropertyIds::toString);
        InlineCache * inlineCache = propertyId == PropertyIds::valueOf ? requestContext->GetValueOfInlineCache() : requestContext->GetToStringInlineCache();
        // Use per script context inline cache for valueOf and toString
        Var aValue = JavascriptOperators::PatchGetValueUsingSpecifiedInlineCache(inlineCache, this, this, propertyId, requestContext);

        // Fast path to the default valueOf/toString implementation
        if (propertyId == PropertyIds::valueOf)
        {TRACE_IT(66068);
            if (aValue == requestContext->GetLibrary()->GetObjectValueOfFunction())
            {TRACE_IT(66069);
                Assert(JavascriptConversion::IsCallable(aValue));
                // The default Object.prototype.valueOf will in turn just call ToObject().
                // The result is always an object if it is not undefined or null (which "this" is not)
                return false;
            }
        }
        else
        {TRACE_IT(66070);
            if (aValue == requestContext->GetLibrary()->GetObjectToStringFunction())
            {TRACE_IT(66071);
                Assert(JavascriptConversion::IsCallable(aValue));
                // These typeIds should never be here (they override ToPrimitive or they don't derive to DynamicObject::ToPrimitive)
                // Otherwise, they may case implicit call in ToStringHelper
                Assert(this->GetTypeId() != TypeIds_HostDispatch
                    && this->GetTypeId() != TypeIds_HostObject);
                *result = JavascriptObject::ToStringHelper(this, requestContext);
                return true;
            }
        }

        return CallToPrimitiveFunction(aValue, propertyId, result, requestContext);
    }
    BOOL DynamicObject::CallToPrimitiveFunction(Var toPrimitiveFunction, PropertyId propertyId, Var* result, ScriptContext * requestContext)
    {TRACE_IT(66072);
        if (JavascriptConversion::IsCallable(toPrimitiveFunction))
        {TRACE_IT(66073);
            RecyclableObject* toStringFunction = RecyclableObject::FromVar(toPrimitiveFunction);

            ThreadContext * threadContext = requestContext->GetThreadContext();
            Var aResult = threadContext->ExecuteImplicitCall(toStringFunction, ImplicitCall_ToPrimitive, [=]() -> Js::Var
            {
                // Stack object should have a pre-op bail on implicit call.  We shouldn't see them here.
                Assert(!ThreadContext::IsOnStack(this) || threadContext->HasNoSideEffect(toStringFunction));
                return CALL_FUNCTION(toStringFunction, CallInfo(CallFlags_Value, 1), this);
            });

            if (!aResult)
            {TRACE_IT(66074);
                // There was an implicit call and implicit calls are disabled. This would typically cause a bailout.
                Assert(threadContext->IsDisableImplicitCall());
                *result = requestContext->GetLibrary()->GetNull();
                return true;
            }

            if (JavascriptOperators::GetTypeId(aResult) <= TypeIds_LastToPrimitiveType)
            {TRACE_IT(66075);
                *result = aResult;
                return true;
            }
        }
        return false;
    }

    BOOL DynamicObject::GetEnumeratorWithPrefix(JavascriptEnumerator * prefixEnumerator, JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache)
    {TRACE_IT(66076);
        Js::ArrayObject * arrayObject = nullptr;
        if (this->HasObjectArray())
        {TRACE_IT(66077);
            arrayObject = this->GetObjectArrayOrFlagsAsArray();
            Assert(arrayObject->GetPropertyCount() == 0);
        }
        return enumerator->Initialize(prefixEnumerator, arrayObject, this, flags, requestContext, forInCache);
    }

    BOOL DynamicObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache)
    {TRACE_IT(66078);
        return GetEnumeratorWithPrefix(nullptr, enumerator, flags, requestContext, forInCache);
    }

    BOOL DynamicObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(66079);
        return GetTypeHandler()->SetAccessors(this, propertyId, getter, setter, flags);
    }

    BOOL DynamicObject::GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext)
    {TRACE_IT(66080);
        return GetTypeHandler()->GetAccessors(this, propertyId, getter, setter);
    }

    BOOL DynamicObject::PreventExtensions()
    {TRACE_IT(66081);
        return GetTypeHandler()->PreventExtensions(this);
    }

    BOOL DynamicObject::Seal()
    {TRACE_IT(66082);
        return GetTypeHandler()->Seal(this);
    }

    BOOL DynamicObject::Freeze()
    {TRACE_IT(66083);
        Type* oldType = this->GetType();
        BOOL ret = GetTypeHandler()->Freeze(this);

        // We just made all properties on this object non-writable.
        // Make sure the type is evolved so that the property string caches
        // are no longer hit.
        if (this->GetType() == oldType)
        {TRACE_IT(66084);
            this->ChangeType();
        }

        return ret;
    }

    BOOL DynamicObject::IsSealed()
    {TRACE_IT(66085);
        return GetTypeHandler()->IsSealed(this);
    }

    BOOL DynamicObject::IsFrozen()
    {TRACE_IT(66086);
        return GetTypeHandler()->IsFrozen(this);
    }

    BOOL DynamicObject::IsWritable(PropertyId propertyId)
    {TRACE_IT(66087);
        return GetTypeHandler()->IsWritable(this, propertyId);
    }

    BOOL DynamicObject::IsConfigurable(PropertyId propertyId)
    {TRACE_IT(66088);
        return GetTypeHandler()->IsConfigurable(this, propertyId);
    }

    BOOL DynamicObject::IsEnumerable(PropertyId propertyId)
    {TRACE_IT(66089);
        return GetTypeHandler()->IsEnumerable(this, propertyId);
    }

    BOOL DynamicObject::SetEnumerable(PropertyId propertyId, BOOL value)
    {TRACE_IT(66090);
        return GetTypeHandler()->SetEnumerable(this, propertyId, value);
    }

    BOOL DynamicObject::SetWritable(PropertyId propertyId, BOOL value)
    {TRACE_IT(66091);
        return GetTypeHandler()->SetWritable(this, propertyId, value);
    }

    BOOL DynamicObject::SetConfigurable(PropertyId propertyId, BOOL value)
    {TRACE_IT(66092);
        return GetTypeHandler()->SetConfigurable(this, propertyId, value);
    }

    BOOL DynamicObject::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
    {TRACE_IT(66093);
        return GetTypeHandler()->SetAttributes(this, propertyId, attributes);
    }

    BOOL DynamicObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(66094);
        stringBuilder->AppendCppLiteral(_u("{...}"));
        return TRUE;
    }

    BOOL DynamicObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(66095);
        stringBuilder->AppendCppLiteral(_u("Object"));
        return TRUE;
    }

    Var DynamicObject::GetTypeOfString(ScriptContext * requestContext)
    {TRACE_IT(66096);
        return requestContext->GetLibrary()->GetObjectTypeDisplayString();
    }

    // If this object is not extensible and the property being set does not already exist,
    // if throwIfNotExtensible is
    // * true, a type error will be thrown
    // * false, FALSE will be returned (unless strict mode is enabled, in which case a type error will be thrown).
    // Either way, the property will not be set.
    //
    // throwIfNotExtensible should always be false for non-numeric properties.
    BOOL DynamicObject::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(66097);
        return GetTypeHandler()->SetPropertyWithAttributes(this, propertyId, value, attributes, info, flags, possibleSideEffects);
    }

#if DBG
    bool DynamicObject::CanStorePropertyValueDirectly(PropertyId propertyId, bool allowLetConst)
    {TRACE_IT(66098);
        return GetTypeHandler()->CanStorePropertyValueDirectly(this, propertyId, allowLetConst);
    }
#endif

    void DynamicObject::RemoveFromPrototype(ScriptContext * requestContext)
    {
        GetTypeHandler()->RemoveFromPrototype(this, requestContext);
    }

    void DynamicObject::AddToPrototype(ScriptContext * requestContext)
    {
        GetTypeHandler()->AddToPrototype(this, requestContext);
    }

    void DynamicObject::SetPrototype(RecyclableObject* newPrototype)
    {TRACE_IT(66099);
        // Mark newPrototype it is being set as prototype
        newPrototype->SetIsPrototype();

        GetTypeHandler()->SetPrototype(this, newPrototype);
    }
}
