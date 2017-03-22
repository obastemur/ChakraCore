//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"
#include "Library/JavascriptSymbol.h"
#include "Library/JavascriptSymbolObject.h"

DEFINE_VALIDATE_HAS_VTABLE_CTOR(Js::RecyclableObject);

namespace Js
{
    void PropertyValueInfo::SetCacheInfo(PropertyValueInfo* info, InlineCache *const inlineCache)
    {LOGMEIN("RecyclableObject.cpp] 13\n");
        Assert(info);
        Assert(inlineCache);

        info->functionBody = nullptr;
        info->inlineCache = inlineCache;
        info->polymorphicInlineCache = nullptr;
        info->inlineCacheIndex = Js::Constants::NoInlineCacheIndex;
        info->allowResizingPolymorphicInlineCache = false;
    }

    void PropertyValueInfo::SetCacheInfo(
        PropertyValueInfo* info,
        FunctionBody *const functionBody,
        InlineCache *const inlineCache,
        const InlineCacheIndex inlineCacheIndex,
        const bool allowResizingPolymorphicInlineCache)
    {LOGMEIN("RecyclableObject.cpp] 30\n");
        Assert(info);
        Assert(functionBody);
        Assert(inlineCache);
        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());

        info->functionBody = functionBody;
        info->inlineCache = inlineCache;
        info->polymorphicInlineCache = nullptr;
        info->inlineCacheIndex = inlineCacheIndex;
        info->allowResizingPolymorphicInlineCache = allowResizingPolymorphicInlineCache;
    }

    void PropertyValueInfo::SetCacheInfo(
        PropertyValueInfo* info,
        FunctionBody *const functionBody,
        PolymorphicInlineCache *const polymorphicInlineCache,
        const InlineCacheIndex inlineCacheIndex,
        const bool allowResizingPolymorphicInlineCache)
    {LOGMEIN("RecyclableObject.cpp] 49\n");
        Assert(info);
        Assert(functionBody);
        Assert(polymorphicInlineCache);
        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());

        info->functionBody = functionBody;
        info->inlineCache = nullptr;
        info->polymorphicInlineCache = polymorphicInlineCache;
        info->inlineCacheIndex = inlineCacheIndex;
        info->allowResizingPolymorphicInlineCache = allowResizingPolymorphicInlineCache;
    }

    void PropertyValueInfo::ClearCacheInfo(PropertyValueInfo* info)
    {LOGMEIN("RecyclableObject.cpp] 63\n");
        if (info != NULL)
        {LOGMEIN("RecyclableObject.cpp] 65\n");
            info->functionBody = nullptr;
            info->inlineCache = nullptr;
            info->polymorphicInlineCache = nullptr;
            info->inlineCacheIndex = Constants::NoInlineCacheIndex;
            info->allowResizingPolymorphicInlineCache = true;
        }
    }

#if DBG || defined(PROFILE_TYPES)
    // Used only by the GlobalObject, because it's typeHandler can't be fully initialized
    // with the globalobject which is currently being created.
    RecyclableObject::RecyclableObject(DynamicType * type, ScriptContext * scriptContext) : type(type)
    {LOGMEIN("RecyclableObject.cpp] 78\n");
#if DBG_EXTRAFIELD
        dtorCalled = false;
#ifdef HEAP_ENUMERATION_VALIDATION
        m_heapEnumValidationCookie = 0;
#endif
#endif
        Assert(type->GetTypeId() == TypeIds_GlobalObject);
        RecordAllocation(scriptContext);
    }

    void RecyclableObject::RecordAllocation(ScriptContext * scriptContext)
    {LOGMEIN("RecyclableObject.cpp] 90\n");
#ifdef PROFILE_TYPES
        TypeId typeId = this->GetType()->GetTypeId();
        if (typeId < sizeof(scriptContext->instanceCount)/sizeof(int))
        {LOGMEIN("RecyclableObject.cpp] 94\n");
            scriptContext->instanceCount[typeId]++;
        }
#endif
    }
#endif

    RecyclableObject::RecyclableObject(Type * type) : type(type)
    {LOGMEIN("RecyclableObject.cpp] 102\n");
#if DBG_EXTRAFIELD
        dtorCalled = false;
#ifdef HEAP_ENUMERATION_VALIDATION
        m_heapEnumValidationCookie = 0;
#endif
#endif
#if DBG || defined(PROFILE_TYPES)
#if ENABLE_NATIVE_CODEGEN
        if (!JITManager::GetJITManager()->IsOOPJITEnabled())
#endif
        {LOGMEIN("RecyclableObject.cpp] 113\n");
            RecordAllocation(type->GetScriptContext());
        }
#endif
    }

    RecyclableObject* RecyclableObject::GetPrototype() const
    {LOGMEIN("RecyclableObject.cpp] 120\n");
        Type* type = GetType();
        if (!type->HasSpecialPrototype())
        {LOGMEIN("RecyclableObject.cpp] 123\n");
            return type->GetPrototype();
        }
        return const_cast<RecyclableObject*>(this)->GetPrototypeSpecial();
    }

    RecyclableObject* RecyclableObject::GetPrototypeSpecial()
    {LOGMEIN("RecyclableObject.cpp] 130\n");
        AssertMsg(GetType()->GetTypeId() == TypeIds_Null, "Do not use this function.");
        return nullptr;
    }

    JavascriptMethod RecyclableObject::GetEntryPoint() const
    {LOGMEIN("RecyclableObject.cpp] 136\n");
        return this->GetType()->GetEntryPoint();
    }

    Recycler* RecyclableObject::GetRecycler() const
    {LOGMEIN("RecyclableObject.cpp] 141\n");
        return this->GetLibrary()->GetRecycler();
    }

    void RecyclableObject::SetIsPrototype()
    {LOGMEIN("RecyclableObject.cpp] 146\n");
        if (DynamicType::Is(this->GetTypeId()))
        {LOGMEIN("RecyclableObject.cpp] 148\n");
            DynamicObject* dynamicThis = DynamicObject::FromVar(this);
            dynamicThis->SetIsPrototype();      // Call the DynamicObject::SetIsPrototype
        }
    }

    bool RecyclableObject::HasOnlyWritableDataProperties()
    {LOGMEIN("RecyclableObject.cpp] 155\n");
        if (DynamicType::Is(this->GetTypeId()))
        {LOGMEIN("RecyclableObject.cpp] 157\n");
            DynamicObject* obj = DynamicObject::FromVar(this);
            return obj->GetTypeHandler()->GetHasOnlyWritableDataProperties() &&
                (!obj->HasObjectArray() || obj->GetObjectArrayOrFlagsAsArray()->HasOnlyWritableDataProperties());
        }

        return true;
    }

    void RecyclableObject::ClearWritableDataOnlyDetectionBit()
    {LOGMEIN("RecyclableObject.cpp] 167\n");
        if (DynamicType::Is(this->GetTypeId()))
        {LOGMEIN("RecyclableObject.cpp] 169\n");
            DynamicObject* obj = DynamicObject::FromVar(this);
            obj->GetTypeHandler()->ClearWritableDataOnlyDetectionBit();
            if (obj->HasObjectArray())
            {LOGMEIN("RecyclableObject.cpp] 173\n");
                obj->GetObjectArrayOrFlagsAsArray()->ClearWritableDataOnlyDetectionBit();
            }
        }
    }

    bool RecyclableObject::IsWritableDataOnlyDetectionBitSet()
    {LOGMEIN("RecyclableObject.cpp] 180\n");
        if (DynamicType::Is(this->GetTypeId()))
        {LOGMEIN("RecyclableObject.cpp] 182\n");
            DynamicObject* obj = DynamicObject::FromVar(this);
            return obj->GetTypeHandler()->IsWritableDataOnlyDetectionBitSet() ||
                (obj->HasObjectArray() && obj->GetObjectArrayOrFlagsAsArray()->IsWritableDataOnlyDetectionBitSet());
        }

        return false;
    }

    RecyclableObject* RecyclableObject::GetProxiedObjectForHeapEnum()
    {LOGMEIN("RecyclableObject.cpp] 192\n");
        Assert(this->GetScriptContext()->IsHeapEnumInProgress());
        return NULL;
    }

    BOOL RecyclableObject::IsExternal() const
    {LOGMEIN("RecyclableObject.cpp] 198\n");
        Assert(this->IsExternalVirtual() == this->GetType()->IsExternal());
        return this->GetType()->IsExternal();
    }

    BOOL RecyclableObject::SkipsPrototype() const
    {LOGMEIN("RecyclableObject.cpp] 204\n");
        Assert(this->DbgSkipsPrototype() == this->GetType()->SkipsPrototype());
        return this->GetType()->SkipsPrototype();
    }

    RecyclableObject * RecyclableObject::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("RecyclableObject.cpp] 210\n");
        switch (JavascriptOperators::GetTypeId(this))
        {LOGMEIN("RecyclableObject.cpp] 212\n");
        case TypeIds_Undefined:
            return requestContext->GetLibrary()->GetUndefined();
        case TypeIds_Null:
            return requestContext->GetLibrary()->GetNull();
        case TypeIds_Number:
            return RecyclableObject::FromVar(JavascriptNumber::CloneToScriptContext(this, requestContext));
        default:
            AssertMsg(FALSE, "shouldn't clone for other types");
            Js::JavascriptError::ThrowError(requestContext, VBSERR_InternalError);
        }
    }

#if defined(PROFILE_RECYCLER_ALLOC) && defined(RECYCLER_DUMP_OBJECT_GRAPH)
    bool RecyclableObject::DumpObjectFunction(type_info const * typeinfo, bool isArray, void * objectAddress)
    {LOGMEIN("RecyclableObject.cpp] 227\n");
        if (isArray)
        {LOGMEIN("RecyclableObject.cpp] 229\n");
            // Don't deal with array
            return false;
        }

        Output::Print(_u("%S{%x} %p"), typeinfo->name(), ((RecyclableObject *)objectAddress)->GetTypeId(), objectAddress);
        return true;
    }
#endif

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType RecyclableObject::GetSnapTag_TTD() const
    {LOGMEIN("RecyclableObject.cpp] 241\n");
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void RecyclableObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Missing subtype implementation.");
    }
#endif

    BOOL RecyclableObject::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {LOGMEIN("RecyclableObject.cpp] 252\n");
        // TODO: It appears as though this is never called. Some types (such as JavascriptNumber) don't override this, but they
        // also don't expect properties to be set on them. Need to review this and see if we can make this pure virtual or
        // Assert(false) here. In any case, this should be SetProperty, not InitProperty.
        Assert(false);

        bool isForce = (flags & PropertyOperation_Force) != 0;
        bool throwIfNotExtensible = (flags & PropertyOperation_ThrowIfNotExtensible) != 0;
        if (!isForce)
        {LOGMEIN("RecyclableObject.cpp] 261\n");
            // throwIfNotExtensible is only relevant to DynamicObjects
            Assert(!throwIfNotExtensible);
        }

        return
            this->InitProperty(propertyId, value, flags) &&
            this->SetAttributes(propertyId, attributes);
    }

    void RecyclableObject::ThrowIfCannotDefineProperty(PropertyId propId, const PropertyDescriptor& descriptor)
    {LOGMEIN("RecyclableObject.cpp] 272\n");
        // Do nothing
    }

    BOOL RecyclableObject::GetDefaultPropertyDescriptor(PropertyDescriptor& descriptor)
    {LOGMEIN("RecyclableObject.cpp] 277\n");
        // By default, when GetOwnPropertyDescriptor is called for a nonexistent property,
        // return undefined.
        return false;
    }

    HRESULT RecyclableObject::QueryObjectInterface(REFIID riid, void **ppvObj)
    {LOGMEIN("RecyclableObject.cpp] 284\n");
        Assert(!this->GetScriptContext()->GetThreadContext()->IsScriptActive());
        return E_NOINTERFACE;
    }
    RecyclableObject* RecyclableObject::GetThisObjectOrUnWrap()
    {LOGMEIN("RecyclableObject.cpp] 289\n");
        if (WithScopeObject::Is(this))
        {LOGMEIN("RecyclableObject.cpp] 291\n");
            return WithScopeObject::FromVar(this)->GetWrappedObject();
        }
        return this;
    }

    // In order to avoid a branch, every object has an entry point if it gets called like a
    // function - however, if it can't be called like a function, it's set to DefaultEntryPoint
    // which will emit an error.
    Var RecyclableObject::DefaultEntryPoint(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        TypeId typeId = function->GetTypeId();
        rtErrors err = typeId == TypeIds_Undefined || typeId == TypeIds_Null ? JSERR_NeedObject : JSERR_NeedFunction;
        JavascriptError::ThrowTypeError(function->GetScriptContext(), err
            /* TODO-ERROR: args.Info.Count > 0? args[0] : nullptr); */);
    }

    BOOL RecyclableObject::HasProperty(PropertyId propertyId)
    {LOGMEIN("RecyclableObject.cpp] 310\n");
        return false;
    }

    BOOL RecyclableObject::HasOwnProperty(PropertyId propertyId)
    {LOGMEIN("RecyclableObject.cpp] 315\n");
        return false;
    }

    BOOL RecyclableObject::HasOwnPropertyNoHostObject(PropertyId propertyId)
    {LOGMEIN("RecyclableObject.cpp] 320\n");
        return HasOwnProperty(propertyId);
    }

    BOOL RecyclableObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("RecyclableObject.cpp] 325\n");
        return false;
    }

    BOOL RecyclableObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("RecyclableObject.cpp] 330\n");
        return false;
    }

    BOOL RecyclableObject::GetInternalProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("RecyclableObject.cpp] 335\n");
        return false;
    }

    BOOL RecyclableObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("RecyclableObject.cpp] 340\n");
        return false;
    }

    BOOL RecyclableObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("RecyclableObject.cpp] 345\n");
        return false;
    }

    BOOL RecyclableObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("RecyclableObject.cpp] 350\n");
        return false;
    }

    BOOL RecyclableObject::SetInternalProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("RecyclableObject.cpp] 355\n");
        return false;
    }

    BOOL RecyclableObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("RecyclableObject.cpp] 360\n");
        return false;
    }

    BOOL RecyclableObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {LOGMEIN("RecyclableObject.cpp] 365\n");
        return false;
    }

    BOOL RecyclableObject::InitFuncScoped(PropertyId propertyId, Var value)
    {LOGMEIN("RecyclableObject.cpp] 370\n");
        return false;
    }

    BOOL RecyclableObject::EnsureProperty(PropertyId propertyId)
    {LOGMEIN("RecyclableObject.cpp] 375\n");
        return false;
    }

    BOOL RecyclableObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {LOGMEIN("RecyclableObject.cpp] 380\n");
        return false;
    }

    BOOL RecyclableObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("RecyclableObject.cpp] 385\n");
        return true;
    }

    BOOL RecyclableObject::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("RecyclableObject.cpp] 390\n");
        return true;
    }

    BOOL RecyclableObject::IsFixedProperty(PropertyId propertyId)
    {LOGMEIN("RecyclableObject.cpp] 395\n");
        return false;
    }

    BOOL RecyclableObject::HasItem(uint32 index)
    {LOGMEIN("RecyclableObject.cpp] 400\n");
        return false;
    }

    BOOL RecyclableObject::HasOwnItem(uint32 index)
    {LOGMEIN("RecyclableObject.cpp] 405\n");
        return false;
    }

    BOOL RecyclableObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {LOGMEIN("RecyclableObject.cpp] 410\n");
        return false;
    }

    BOOL RecyclableObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {LOGMEIN("RecyclableObject.cpp] 415\n");
        return false;
    }

    BOOL RecyclableObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("RecyclableObject.cpp] 420\n");
        return false;
    }

    BOOL RecyclableObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("RecyclableObject.cpp] 425\n");
        return true;
    }

    BOOL RecyclableObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("RecyclableObject.cpp] 430\n");
        return false;
    }

    BOOL RecyclableObject::ToPrimitive(JavascriptHint hint, Var* value, ScriptContext * scriptContext)
    {LOGMEIN("RecyclableObject.cpp] 435\n");
        *value = NULL;
        return false;
    }

    BOOL RecyclableObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("RecyclableObject.cpp] 441\n");
        return false;
    }

    BOOL RecyclableObject::GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext)
    {LOGMEIN("RecyclableObject.cpp] 446\n");
        return false;
    }

    BOOL RecyclableObject::StrictEquals(__in Var aRight, __out BOOL* value, ScriptContext * requestContext)
    {LOGMEIN("RecyclableObject.cpp] 451\n");
        *value = false;
        //StrictEquals is handled in JavascriptOperators::StrictEqual
        Throw::InternalError();
    }

#pragma fenv_access (on)
    BOOL RecyclableObject::Equals(__in Var aRight, __out BOOL* value, ScriptContext * requestContext)
    {LOGMEIN("RecyclableObject.cpp] 459\n");
        Var aLeft = this;
        if (aLeft == aRight)
        {LOGMEIN("RecyclableObject.cpp] 462\n");
            //In ES5 mode strict equals (===) on same instance of object type VariantDate succeeds.
            //Hence equals needs to succeed.
            //goto ReturnTrue;
            *value = TRUE;
            return TRUE;
        }

        double dblLeft, dblRight;
        TypeId leftType = this->GetTypeId();
        TypeId rightType = JavascriptOperators::GetTypeId(aRight);
        int redoCount = 0;

    Redo:
        if (redoCount == 2)
        {LOGMEIN("RecyclableObject.cpp] 477\n");
            goto ReturnFalse;
        }

        switch (leftType)
        {LOGMEIN("RecyclableObject.cpp] 482\n");
        case TypeIds_Undefined:
        case TypeIds_Null:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 486\n");
            case TypeIds_Integer:
            case TypeIds_Number:
            case TypeIds_Symbol:
                goto ReturnFalse;
            case TypeIds_Undefined:
            case TypeIds_Null:
                goto ReturnTrue;
            default:
                // Falsy objects are == null and == undefined.
                *value = RecyclableObject::FromVar(aRight)->GetType()->IsFalsy();
                return TRUE;
            }
        case TypeIds_Integer:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 501\n");
            case TypeIds_Undefined:
            case TypeIds_Null:
            case TypeIds_Symbol:
                goto ReturnFalse;
            case TypeIds_Integer:
                // We already did a check to see if aLeft == aRight above, but we need to check again in case there was a redo.
                *value = aLeft == aRight;
                return TRUE;
            case TypeIds_Int64Number:
            {LOGMEIN("RecyclableObject.cpp] 511\n");
                int leftValue = TaggedInt::ToInt32(aLeft);
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                *value = leftValue == rightValue;
                Assert(!(*value));  // currently it cannot be true. more for future extension if we allow arithmetic calculation
                return TRUE;
            }
            case TypeIds_UInt64Number:
            {LOGMEIN("RecyclableObject.cpp] 519\n");
                __int64 leftValue = TaggedInt::ToInt32(aLeft);
                unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                // TODO: yongqu to review whether we need to check for neg value
                *value = (/*leftValue >= 0 && */(unsigned __int64)leftValue == rightValue);
                Assert(!(*value));  // currently it cannot be true. more for future extension if we allow arithmetic calculation
                return TRUE;
            }
            case TypeIds_Number:
                dblLeft = TaggedInt::ToDouble(aLeft);
                dblRight = JavascriptNumber::GetValue(aRight);
                goto CompareDoubles;
            case TypeIds_Boolean:
            case TypeIds_String:
                dblLeft = TaggedInt::ToDouble(aLeft);
                dblRight = JavascriptConversion::ToNumber(aRight, requestContext);
                goto CompareDoubles;
            default:
                goto RedoRight;
            }
            break;
        case TypeIds_Int64Number:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 542\n");
            case TypeIds_Integer:
            {LOGMEIN("RecyclableObject.cpp] 544\n");
                __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                int rightValue = TaggedInt::ToInt32(aRight);
                *value = leftValue == rightValue;
                Assert(!(*value));  // currently it cannot be true. more for future extension if we allow arithmetic calculation
                return TRUE;
            }
            case TypeIds_Number:
                dblLeft = (double)JavascriptInt64Number::FromVar(aLeft)->GetValue();
                dblRight = JavascriptNumber::GetValue(aRight);
                goto CompareDoubles;
            case TypeIds_Int64Number:
            {LOGMEIN("RecyclableObject.cpp] 556\n");
                __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                *value = leftValue == rightValue;
                return TRUE;
            }
            case TypeIds_UInt64Number:
            {LOGMEIN("RecyclableObject.cpp] 563\n");
                __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                // TODO: yongqu to review whether we need to check for neg value
                *value = (/* leftValue >= 0 && */(unsigned __int64)leftValue == rightValue);
                return TRUE;
            }
            }
            break;
        case TypeIds_UInt64Number:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 574\n");
            case TypeIds_Integer:
            {LOGMEIN("RecyclableObject.cpp] 576\n");
                unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                __int64 rightValue = TaggedInt::ToInt32(aRight);
                // TODO: yongqu to review whether we need to check for neg value
                *value = rightValue >= 0 && leftValue == (unsigned __int64)rightValue;
                Assert(!(*value));  // currently it cannot be true. more for future extension if we allow arithmetic calculation
                return TRUE;
            }
            case TypeIds_Number:
                dblLeft = (double)JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                dblRight = JavascriptNumber::GetValue(aRight);
                goto CompareDoubles;
            case TypeIds_Int64Number:
            {LOGMEIN("RecyclableObject.cpp] 589\n");
                unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                // TODO: yongqu to review whether we need to check for neg value
                *value = (/* rightValue >= 0 && */leftValue == (unsigned __int64)rightValue);
                return TRUE;
            }
            case TypeIds_UInt64Number:
            {LOGMEIN("RecyclableObject.cpp] 597\n");
                unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                *value = leftValue == rightValue;
                return TRUE;
            }
            }
            break;
        case TypeIds_Number:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 607\n");
            case TypeIds_Undefined:
            case TypeIds_Null:
            case TypeIds_Symbol:
                goto ReturnFalse;
            case TypeIds_Integer:
                dblLeft = JavascriptNumber::GetValue(aLeft);
                dblRight = TaggedInt::ToDouble(aRight);
                goto CompareDoubles;
            case TypeIds_Number:
                dblLeft = JavascriptNumber::GetValue(aLeft);
                dblRight = JavascriptNumber::GetValue(aRight);
                goto CompareDoubles;
            case TypeIds_Boolean:
            case TypeIds_String:
                dblLeft = JavascriptNumber::GetValue(aLeft);
                dblRight = JavascriptConversion::ToNumber(aRight, requestContext);
                goto CompareDoubles;
            default:
                goto RedoRight;
            }
            break;
        case TypeIds_String:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 631\n");
            case TypeIds_Undefined:
            case TypeIds_Null:
            case TypeIds_Symbol:
                goto ReturnFalse;
            case TypeIds_String:
                goto CompareStrings;
            case TypeIds_Number:
            case TypeIds_Integer:
            case TypeIds_Boolean:
                dblLeft = JavascriptConversion::ToNumber(aLeft, requestContext);
                dblRight = JavascriptConversion::ToNumber(aRight, requestContext);
                goto CompareDoubles;
            default:
                goto RedoRight;
            }
        case TypeIds_Boolean:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 649\n");
            case TypeIds_Undefined:
            case TypeIds_Null:
            case TypeIds_Symbol:
                goto ReturnFalse;
            case TypeIds_Boolean:
                *value = JavascriptBoolean::FromVar(aLeft)->GetValue() == JavascriptBoolean::FromVar(aRight)->GetValue();
                return TRUE;
            case TypeIds_Number:
            case TypeIds_Integer:
            case TypeIds_String:
                dblLeft = JavascriptConversion::ToNumber(aLeft, requestContext);
                dblRight = JavascriptConversion::ToNumber(aRight, requestContext);
                goto CompareDoubles;
            default:
                goto RedoRight;
            }
            break;

        case TypeIds_Symbol:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 670\n");
            case TypeIds_Undefined:
            case TypeIds_Null:
            case TypeIds_Number:
            case TypeIds_Integer:
            case TypeIds_String:
            case TypeIds_Boolean:
                goto ReturnFalse;
            case TypeIds_Symbol:
                *value = JavascriptSymbol::FromVar(aLeft)->GetValue() == JavascriptSymbol::FromVar(aRight)->GetValue();
                return TRUE;
            case TypeIds_SymbolObject:
                *value = JavascriptSymbol::FromVar(aLeft)->GetValue() == JavascriptSymbolObject::FromVar(aRight)->GetValue();
                return TRUE;
            default:
                goto RedoRight;
            }
            break;

        case TypeIds_Function:
            if (rightType == TypeIds_Function)
            {LOGMEIN("RecyclableObject.cpp] 691\n");
                goto ReturnFalse;
            }
            // Fall through to do normal object comparison on function object.
        default:
            switch (rightType)
            {LOGMEIN("RecyclableObject.cpp] 697\n");
            case TypeIds_Undefined:
            case TypeIds_Null:
                // Falsy objects are == null and == undefined.
                *value = this->type->IsFalsy();
                return TRUE;
            case TypeIds_Boolean:
            case TypeIds_Integer:
            case TypeIds_Number:
            case TypeIds_String:
            case TypeIds_Symbol:
                goto RedoLeft;
            default:
                goto ReturnFalse;
            }
        }

    RedoLeft:
        aLeft = JavascriptConversion::ToPrimitive(aLeft, JavascriptHint::None, requestContext);
        leftType = JavascriptOperators::GetTypeId(aLeft);
        redoCount++;
        goto Redo;
    RedoRight:
        aRight = JavascriptConversion::ToPrimitive(aRight, JavascriptHint::None, requestContext);
        rightType = JavascriptOperators::GetTypeId(aRight);
        redoCount++;
        goto Redo;
    CompareStrings:
        *value = JavascriptString::Equals(aLeft, aRight);
        return TRUE;
    CompareDoubles:
        *value = dblLeft == dblRight;
        return TRUE;
    ReturnFalse:
        *value = FALSE;
        return TRUE;
    ReturnTrue:
        *value = TRUE;
        return TRUE;
    }

    RecyclableObject* RecyclableObject::ToObject(ScriptContext * requestContext)
    {LOGMEIN("RecyclableObject.cpp] 739\n");
        AssertMsg(JavascriptOperators::IsObject(this), "bad type object in conversion ToObject");
        Assert(!CrossSite::NeedMarshalVar(this, requestContext));
        return this;
    }

    Var RecyclableObject::GetTypeOfString(ScriptContext * requestContext)
    {LOGMEIN("RecyclableObject.cpp] 746\n");
        return requestContext->GetLibrary()->GetUnknownDisplayString();
    }

    Var RecyclableObject::InvokePut(Arguments args)
    {LOGMEIN("RecyclableObject.cpp] 751\n");
        // Handle x(y) = z.
        // Native jscript object behavior: throw an error in all such cases.
        JavascriptError::ThrowReferenceError(GetScriptContext(), JSERR_CantAsgCall);
    }

    BOOL RecyclableObject::GetRemoteTypeId(TypeId * typeId)
    {LOGMEIN("RecyclableObject.cpp] 758\n");
        return FALSE;
    }

    DynamicObject* RecyclableObject::GetRemoteObject()
    {LOGMEIN("RecyclableObject.cpp] 763\n");
        return NULL;
    }

    Var RecyclableObject::GetHostDispatchVar()
    {LOGMEIN("RecyclableObject.cpp] 768\n");
        Assert(FALSE);
        return this->GetLibrary()->GetUndefined();
    }

    JavascriptString* RecyclableObject::GetClassName(ScriptContext * requestContext)
    {LOGMEIN("RecyclableObject.cpp] 774\n");
        // we don't need this when not handling fastDOM.
        Assert(0);
        return NULL;
    }

    BOOL RecyclableObject::HasInstance(Var instance, ScriptContext* scriptContext, IsInstInlineCache* inlineCache)
    {LOGMEIN("RecyclableObject.cpp] 781\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedFunction, _u("instanceof") /* TODO-ERROR: get arg name - aClass */);
    }
} // namespace Js
