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
    {TRACE_IT(66840);
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
    {TRACE_IT(66841);
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
    {TRACE_IT(66842);
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
    {TRACE_IT(66843);
        if (info != NULL)
        {TRACE_IT(66844);
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
    {TRACE_IT(66845);
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
    {TRACE_IT(66846);
#ifdef PROFILE_TYPES
        TypeId typeId = this->GetType()->GetTypeId();
        if (typeId < sizeof(scriptContext->instanceCount)/sizeof(int))
        {TRACE_IT(66847);
            scriptContext->instanceCount[typeId]++;
        }
#endif
    }
#endif

    RecyclableObject::RecyclableObject(Type * type) : type(type)
    {TRACE_IT(66848);
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
        {TRACE_IT(66849);
            RecordAllocation(type->GetScriptContext());
        }
#endif
    }

    RecyclableObject* RecyclableObject::GetPrototype() const
    {TRACE_IT(66850);
        Type* type = GetType();
        if (!type->HasSpecialPrototype())
        {TRACE_IT(66851);
            return type->GetPrototype();
        }
        return const_cast<RecyclableObject*>(this)->GetPrototypeSpecial();
    }

    RecyclableObject* RecyclableObject::GetPrototypeSpecial()
    {TRACE_IT(66852);
        AssertMsg(GetType()->GetTypeId() == TypeIds_Null, "Do not use this function.");
        return nullptr;
    }

    JavascriptMethod RecyclableObject::GetEntryPoint() const
    {TRACE_IT(66853);
        return this->GetType()->GetEntryPoint();
    }

    Recycler* RecyclableObject::GetRecycler() const
    {TRACE_IT(66854);
        return this->GetLibrary()->GetRecycler();
    }

    void RecyclableObject::SetIsPrototype()
    {TRACE_IT(66855);
        if (DynamicType::Is(this->GetTypeId()))
        {TRACE_IT(66856);
            DynamicObject* dynamicThis = DynamicObject::FromVar(this);
            dynamicThis->SetIsPrototype();      // Call the DynamicObject::SetIsPrototype
        }
    }

    bool RecyclableObject::HasOnlyWritableDataProperties()
    {TRACE_IT(66857);
        if (DynamicType::Is(this->GetTypeId()))
        {TRACE_IT(66858);
            DynamicObject* obj = DynamicObject::FromVar(this);
            return obj->GetTypeHandler()->GetHasOnlyWritableDataProperties() &&
                (!obj->HasObjectArray() || obj->GetObjectArrayOrFlagsAsArray()->HasOnlyWritableDataProperties());
        }

        return true;
    }

    void RecyclableObject::ClearWritableDataOnlyDetectionBit()
    {TRACE_IT(66859);
        if (DynamicType::Is(this->GetTypeId()))
        {TRACE_IT(66860);
            DynamicObject* obj = DynamicObject::FromVar(this);
            obj->GetTypeHandler()->ClearWritableDataOnlyDetectionBit();
            if (obj->HasObjectArray())
            {TRACE_IT(66861);
                obj->GetObjectArrayOrFlagsAsArray()->ClearWritableDataOnlyDetectionBit();
            }
        }
    }

    bool RecyclableObject::IsWritableDataOnlyDetectionBitSet()
    {TRACE_IT(66862);
        if (DynamicType::Is(this->GetTypeId()))
        {TRACE_IT(66863);
            DynamicObject* obj = DynamicObject::FromVar(this);
            return obj->GetTypeHandler()->IsWritableDataOnlyDetectionBitSet() ||
                (obj->HasObjectArray() && obj->GetObjectArrayOrFlagsAsArray()->IsWritableDataOnlyDetectionBitSet());
        }

        return false;
    }

    RecyclableObject* RecyclableObject::GetProxiedObjectForHeapEnum()
    {TRACE_IT(66864);
        Assert(this->GetScriptContext()->IsHeapEnumInProgress());
        return NULL;
    }

    BOOL RecyclableObject::IsExternal() const
    {TRACE_IT(66865);
        Assert(this->IsExternalVirtual() == this->GetType()->IsExternal());
        return this->GetType()->IsExternal();
    }

    BOOL RecyclableObject::SkipsPrototype() const
    {TRACE_IT(66866);
        Assert(this->DbgSkipsPrototype() == this->GetType()->SkipsPrototype());
        return this->GetType()->SkipsPrototype();
    }

    RecyclableObject * RecyclableObject::CloneToScriptContext(ScriptContext* requestContext)
    {TRACE_IT(66867);
        switch (JavascriptOperators::GetTypeId(this))
        {
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
    {TRACE_IT(66868);
        if (isArray)
        {TRACE_IT(66869);
            // Don't deal with array
            return false;
        }

        Output::Print(_u("%S{%x} %p"), typeinfo->name(), ((RecyclableObject *)objectAddress)->GetTypeId(), objectAddress);
        return true;
    }
#endif

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType RecyclableObject::GetSnapTag_TTD() const
    {TRACE_IT(66870);
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void RecyclableObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Missing subtype implementation.");
    }
#endif

    BOOL RecyclableObject::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(66871);
        // TODO: It appears as though this is never called. Some types (such as JavascriptNumber) don't override this, but they
        // also don't expect properties to be set on them. Need to review this and see if we can make this pure virtual or
        // Assert(false) here. In any case, this should be SetProperty, not InitProperty.
        Assert(false);

        bool isForce = (flags & PropertyOperation_Force) != 0;
        bool throwIfNotExtensible = (flags & PropertyOperation_ThrowIfNotExtensible) != 0;
        if (!isForce)
        {TRACE_IT(66872);
            // throwIfNotExtensible is only relevant to DynamicObjects
            Assert(!throwIfNotExtensible);
        }

        return
            this->InitProperty(propertyId, value, flags) &&
            this->SetAttributes(propertyId, attributes);
    }

    void RecyclableObject::ThrowIfCannotDefineProperty(PropertyId propId, const PropertyDescriptor& descriptor)
    {TRACE_IT(66873);
        // Do nothing
    }

    BOOL RecyclableObject::GetDefaultPropertyDescriptor(PropertyDescriptor& descriptor)
    {TRACE_IT(66874);
        // By default, when GetOwnPropertyDescriptor is called for a nonexistent property,
        // return undefined.
        return false;
    }

    HRESULT RecyclableObject::QueryObjectInterface(REFIID riid, void **ppvObj)
    {TRACE_IT(66875);
        Assert(!this->GetScriptContext()->GetThreadContext()->IsScriptActive());
        return E_NOINTERFACE;
    }
    RecyclableObject* RecyclableObject::GetThisObjectOrUnWrap()
    {TRACE_IT(66876);
        if (WithScopeObject::Is(this))
        {TRACE_IT(66877);
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
    {TRACE_IT(66878);
        return false;
    }

    BOOL RecyclableObject::HasOwnProperty(PropertyId propertyId)
    {TRACE_IT(66879);
        return false;
    }

    BOOL RecyclableObject::HasOwnPropertyNoHostObject(PropertyId propertyId)
    {TRACE_IT(66880);
        return HasOwnProperty(propertyId);
    }

    BOOL RecyclableObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66881);
        return false;
    }

    BOOL RecyclableObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66882);
        return false;
    }

    BOOL RecyclableObject::GetInternalProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66883);
        return false;
    }

    BOOL RecyclableObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66884);
        return false;
    }

    BOOL RecyclableObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66885);
        return false;
    }

    BOOL RecyclableObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66886);
        return false;
    }

    BOOL RecyclableObject::SetInternalProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66887);
        return false;
    }

    BOOL RecyclableObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66888);
        return false;
    }

    BOOL RecyclableObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {TRACE_IT(66889);
        return false;
    }

    BOOL RecyclableObject::InitFuncScoped(PropertyId propertyId, Var value)
    {TRACE_IT(66890);
        return false;
    }

    BOOL RecyclableObject::EnsureProperty(PropertyId propertyId)
    {TRACE_IT(66891);
        return false;
    }

    BOOL RecyclableObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {TRACE_IT(66892);
        return false;
    }

    BOOL RecyclableObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(66893);
        return true;
    }

    BOOL RecyclableObject::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(66894);
        return true;
    }

    BOOL RecyclableObject::IsFixedProperty(PropertyId propertyId)
    {TRACE_IT(66895);
        return false;
    }

    BOOL RecyclableObject::HasItem(uint32 index)
    {TRACE_IT(66896);
        return false;
    }

    BOOL RecyclableObject::HasOwnItem(uint32 index)
    {TRACE_IT(66897);
        return false;
    }

    BOOL RecyclableObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(66898);
        return false;
    }

    BOOL RecyclableObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(66899);
        return false;
    }

    BOOL RecyclableObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(66900);
        return false;
    }

    BOOL RecyclableObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(66901);
        return true;
    }

    BOOL RecyclableObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(66902);
        return false;
    }

    BOOL RecyclableObject::ToPrimitive(JavascriptHint hint, Var* value, ScriptContext * scriptContext)
    {TRACE_IT(66903);
        *value = NULL;
        return false;
    }

    BOOL RecyclableObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(66904);
        return false;
    }

    BOOL RecyclableObject::GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext)
    {TRACE_IT(66905);
        return false;
    }

    BOOL RecyclableObject::StrictEquals(__in Var aRight, __out BOOL* value, ScriptContext * requestContext)
    {TRACE_IT(66906);
        *value = false;
        //StrictEquals is handled in JavascriptOperators::StrictEqual
        Throw::InternalError();
    }

#pragma fenv_access (on)
    BOOL RecyclableObject::Equals(__in Var aRight, __out BOOL* value, ScriptContext * requestContext)
    {TRACE_IT(66907);
        Var aLeft = this;
        if (aLeft == aRight)
        {TRACE_IT(66908);
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
        {TRACE_IT(66909);
            goto ReturnFalse;
        }

        switch (leftType)
        {
        case TypeIds_Undefined:
        case TypeIds_Null:
            switch (rightType)
            {
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
            {
            case TypeIds_Undefined:
            case TypeIds_Null:
            case TypeIds_Symbol:
                goto ReturnFalse;
            case TypeIds_Integer:
                // We already did a check to see if aLeft == aRight above, but we need to check again in case there was a redo.
                *value = aLeft == aRight;
                return TRUE;
            case TypeIds_Int64Number:
            {TRACE_IT(66910);
                int leftValue = TaggedInt::ToInt32(aLeft);
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                *value = leftValue == rightValue;
                Assert(!(*value));  // currently it cannot be true. more for future extension if we allow arithmetic calculation
                return TRUE;
            }
            case TypeIds_UInt64Number:
            {TRACE_IT(66911);
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
            {
            case TypeIds_Integer:
            {TRACE_IT(66912);
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
            {TRACE_IT(66913);
                __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                *value = leftValue == rightValue;
                return TRUE;
            }
            case TypeIds_UInt64Number:
            {TRACE_IT(66914);
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
            {
            case TypeIds_Integer:
            {TRACE_IT(66915);
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
            {TRACE_IT(66916);
                unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                // TODO: yongqu to review whether we need to check for neg value
                *value = (/* rightValue >= 0 && */leftValue == (unsigned __int64)rightValue);
                return TRUE;
            }
            case TypeIds_UInt64Number:
            {TRACE_IT(66917);
                unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                *value = leftValue == rightValue;
                return TRUE;
            }
            }
            break;
        case TypeIds_Number:
            switch (rightType)
            {
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
            {
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
            {
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
            {
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
            {TRACE_IT(66918);
                goto ReturnFalse;
            }
            // Fall through to do normal object comparison on function object.
        default:
            switch (rightType)
            {
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
    {TRACE_IT(66919);
        AssertMsg(JavascriptOperators::IsObject(this), "bad type object in conversion ToObject");
        Assert(!CrossSite::NeedMarshalVar(this, requestContext));
        return this;
    }

    Var RecyclableObject::GetTypeOfString(ScriptContext * requestContext)
    {TRACE_IT(66920);
        return requestContext->GetLibrary()->GetUnknownDisplayString();
    }

    Var RecyclableObject::InvokePut(Arguments args)
    {TRACE_IT(66921);
        // Handle x(y) = z.
        // Native jscript object behavior: throw an error in all such cases.
        JavascriptError::ThrowReferenceError(GetScriptContext(), JSERR_CantAsgCall);
    }

    BOOL RecyclableObject::GetRemoteTypeId(TypeId * typeId)
    {TRACE_IT(66922);
        return FALSE;
    }

    DynamicObject* RecyclableObject::GetRemoteObject()
    {TRACE_IT(66923);
        return NULL;
    }

    Var RecyclableObject::GetHostDispatchVar()
    {TRACE_IT(66924);
        Assert(FALSE);
        return this->GetLibrary()->GetUndefined();
    }

    JavascriptString* RecyclableObject::GetClassName(ScriptContext * requestContext)
    {TRACE_IT(66925);
        // we don't need this when not handling fastDOM.
        Assert(0);
        return NULL;
    }

    BOOL RecyclableObject::HasInstance(Var instance, ScriptContext* scriptContext, IsInstInlineCache* inlineCache)
    {TRACE_IT(66926);
        JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedFunction, _u("instanceof") /* TODO-ERROR: get arg name - aClass */);
    }
} // namespace Js
