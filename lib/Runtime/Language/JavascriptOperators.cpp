//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#include "Types/PathTypeHandler.h"
#include "Types/PropertyIndexRanges.h"
#include "Types/WithScopeObject.h"
#include "Types/SpreadArgument.h"
#include "Library/JavascriptPromise.h"
#include "Library/JavascriptRegularExpression.h"
#include "Library/ThrowErrorObject.h"
#include "Library/JavascriptGeneratorFunction.h"

#include "Library/ForInObjectEnumerator.h"
#include "Library/ES5Array.h"

#ifndef SCRIPT_DIRECT_TYPE
typedef enum JsNativeValueType: int
{
    JsInt8Type,
    JsUint8Type,
    JsInt16Type,
    JsUint16Type,
    JsInt32Type,
    JsUint32Type,
    JsInt64Type,
    JsUint64Type,
    JsFloatType,
    JsDoubleType,
    JsNativeStringType
} JsNativeValueType;

typedef struct JsNativeString
{
    unsigned int length;
    LPCWSTR str;
} JsNativeString;

#endif

namespace Js
{
    DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER(Var);
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(FrameDisplay);

    enum IndexType
    {
        IndexType_Number,
        IndexType_PropertyId,
        IndexType_JavascriptString
    };

    IndexType GetIndexTypeFromString(char16 const * propertyName, charcount_t propertyLength, ScriptContext* scriptContext, uint32* index, PropertyRecord const** propertyRecord, bool createIfNotFound)
    {TRACE_IT(49945);
        if (JavascriptOperators::TryConvertToUInt32(propertyName, propertyLength, index) &&
            (*index != JavascriptArray::InvalidIndex))
        {TRACE_IT(49946);
            return IndexType_Number;
        }
        else
        {TRACE_IT(49947);
            if (createIfNotFound)
            {TRACE_IT(49948);
                scriptContext->GetOrAddPropertyRecord(propertyName, propertyLength, propertyRecord);
            }
            else
            {TRACE_IT(49949);
                scriptContext->FindPropertyRecord(propertyName, propertyLength, propertyRecord);
            }
            return IndexType_PropertyId;
        }
    }

    IndexType GetIndexTypeFromPrimitive(Var indexVar, ScriptContext* scriptContext, uint32* index, PropertyRecord const ** propertyRecord, JavascriptString ** propertyNameString, bool createIfNotFound, bool preferJavascriptStringOverPropertyRecord)
    {TRACE_IT(49950);
        // CONSIDER: Only OP_SetElementI and OP_GetElementI use and take advantage of the
        // IndexType_JavascriptString result. Consider modifying other callers of GetIndexType to take
        // advantage of non-interned property strings where appropriate.
        if (TaggedInt::Is(indexVar))
        {TRACE_IT(49951);
            int indexInt = TaggedInt::ToInt32(indexVar);
            if (indexInt >= 0)
            {TRACE_IT(49952);
                *index = (uint)indexInt;
                return IndexType_Number;
            }
            else
            {TRACE_IT(49953);
                char16 buffer[20];
                ::_itow_s(indexInt, buffer, sizeof(buffer) / sizeof(char16), 10);
                charcount_t length = JavascriptString::GetBufferLength(buffer);
                if (createIfNotFound || preferJavascriptStringOverPropertyRecord)
                {TRACE_IT(49954);
                    // When preferring JavascriptString objects, just return a PropertyRecord instead
                    // of creating temporary JavascriptString objects for every negative integer that
                    // comes through here.
                    scriptContext->GetOrAddPropertyRecord(buffer, length, propertyRecord);
                }
                else
                {TRACE_IT(49955);
                    scriptContext->FindPropertyRecord(buffer, length, propertyRecord);
                }
                return IndexType_PropertyId;
            }
        }
        else if (JavascriptSymbol::Is(indexVar))
        {TRACE_IT(49956);
            JavascriptSymbol* symbol = JavascriptSymbol::FromVar(indexVar);

            // JavascriptSymbols cannot add a new PropertyRecord - they correspond to one and only one existing PropertyRecord.
            // We already know what the PropertyRecord is since it is stored in the JavascriptSymbol itself so just return it.

            *propertyRecord = symbol->GetValue();

            return IndexType_PropertyId;
        }
        else
        {TRACE_IT(49957);
            JavascriptString* indexStr = JavascriptConversion::ToString(indexVar, scriptContext);
            char16 const * propertyName = indexStr->GetString();
            charcount_t const propertyLength = indexStr->GetLength();

            if (!createIfNotFound && preferJavascriptStringOverPropertyRecord)
            {TRACE_IT(49958);
                if (JavascriptOperators::TryConvertToUInt32(propertyName, propertyLength, index) &&
                    (*index != JavascriptArray::InvalidIndex))
                {TRACE_IT(49959);
                    return IndexType_Number;
                }

                *propertyNameString = indexStr;
                return IndexType_JavascriptString;
            }
            return GetIndexTypeFromString(propertyName, propertyLength, scriptContext, index, propertyRecord, createIfNotFound);
        }
    }

    IndexType GetIndexTypeFromPrimitive(Var indexVar, ScriptContext* scriptContext, uint32* index, PropertyRecord const ** propertyRecord, bool createIfNotFound)
    {TRACE_IT(49960);
        return GetIndexTypeFromPrimitive(indexVar, scriptContext, index, propertyRecord, nullptr, createIfNotFound, false);
    }

    IndexType GetIndexType(Var& indexVar, ScriptContext* scriptContext, uint32* index, PropertyRecord const ** propertyRecord, JavascriptString ** propertyNameString, bool createIfNotFound, bool preferJavascriptStringOverPropertyRecord)
    {TRACE_IT(49961);
        indexVar = JavascriptConversion::ToPrimitive(indexVar, JavascriptHint::HintString, scriptContext);
        return GetIndexTypeFromPrimitive(indexVar, scriptContext, index, propertyRecord, propertyNameString, createIfNotFound, preferJavascriptStringOverPropertyRecord);
    }

    IndexType GetIndexType(Var& indexVar, ScriptContext* scriptContext, uint32* index, PropertyRecord const ** propertyRecord, bool createIfNotFound)
    {TRACE_IT(49962);
        return GetIndexType(indexVar, scriptContext, index, propertyRecord, nullptr, createIfNotFound, false);
    }

    BOOL FEqualDbl(double dbl1, double dbl2)
    {TRACE_IT(49963);
        // If the low ulongs don't match, they can't be equal.
        if (Js::NumberUtilities::LuLoDbl(dbl1) != Js::NumberUtilities::LuLoDbl(dbl2))
            return FALSE;

        // If the high ulongs don't match, they can be equal iff one is -0 and
        // the other is +0.
        if (Js::NumberUtilities::LuHiDbl(dbl1) != Js::NumberUtilities::LuHiDbl(dbl2))
        {TRACE_IT(49964);
            return 0x80000000 == (Js::NumberUtilities::LuHiDbl(dbl1) | Js::NumberUtilities::LuHiDbl(dbl2)) &&
                0 == Js::NumberUtilities::LuLoDbl(dbl1);
        }

        // The bit patterns match. They are equal iff they are not Nan.
        return !Js::NumberUtilities::IsNan(dbl1);
    }

    Var JavascriptOperators::OP_ApplyArgs(Var func, Var instance, __in_xcount(8) void** stackPtr, CallInfo callInfo, ScriptContext* scriptContext)
    {TRACE_IT(49965);
        int argCount = callInfo.Count;
        ///
        /// Check func has internal [[Call]] property
        /// If not, throw TypeError
        ///
        if (!JavascriptConversion::IsCallable(func)) {TRACE_IT(49966);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
        }

        // Fix callInfo: expect result/value, and none of other flags are currently applicable.
        //   OP_ApplyArgs expects a result. Neither of {jit, interpreted} mode sends correct callFlags:
        //   LdArgCnt -- jit sends whatever was passed to current function, interpreter always sends 0.
        //   See Win8 bug 490489.
        callInfo.Flags = CallFlags_Value;

        RecyclableObject *funcPtr = RecyclableObject::FromVar(func);
        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault + argCount * 4);

        JavascriptMethod entryPoint = funcPtr->GetEntryPoint();
        Var ret;

        switch (argCount) {
        case 0:
            Assert(false);
            ret = CALL_ENTRYPOINT(entryPoint, funcPtr, callInfo);
            break;
        case 1:
            ret = CALL_ENTRYPOINT(entryPoint, funcPtr, callInfo, instance);
            break;
        case 2:
            ret = CALL_ENTRYPOINT(entryPoint, funcPtr, callInfo, instance, stackPtr[0]);
            break;
        case 3:
            ret = CALL_ENTRYPOINT(entryPoint, funcPtr, callInfo, instance, stackPtr[0], stackPtr[1]);
            break;
        case 4:
            ret = CALL_ENTRYPOINT(entryPoint, funcPtr, callInfo, instance, stackPtr[0], stackPtr[1], stackPtr[2]);
            break;
        case 5:
            ret = CALL_ENTRYPOINT(entryPoint, funcPtr, callInfo, instance, stackPtr[0], stackPtr[1], stackPtr[2], stackPtr[3]);
            break;
        case 6:
            ret = CALL_ENTRYPOINT(entryPoint, funcPtr, callInfo, instance, stackPtr[0], stackPtr[1], stackPtr[2], stackPtr[3], stackPtr[4]);
            break;
        case 7:
            ret = CALL_ENTRYPOINT(entryPoint, funcPtr, callInfo, instance, stackPtr[0], stackPtr[1], stackPtr[2], stackPtr[3], stackPtr[4], stackPtr[5]);
            break;
        default: {TRACE_IT(49967);
            // Don't need stack probe here- we just did so above
            Arguments args(callInfo, stackPtr - 1);
            ret = JavascriptFunction::CallFunction<false>(funcPtr, entryPoint, args);
        }
                 break;
        }
        return ret;
    }

#ifdef _M_IX86
    // Alias for overloaded JavascriptNumber::ToVar so it can be called unambiguously from native code
    Var JavascriptOperators::Int32ToVar(int32 value, ScriptContext* scriptContext)
    {TRACE_IT(49968);
        return JavascriptNumber::ToVar(value, scriptContext);
    }

    // Alias for overloaded JavascriptNumber::ToVar so it can be called unambiguously from native code
    Var JavascriptOperators::Int32ToVarInPlace(int32 value, ScriptContext* scriptContext, JavascriptNumber* result)
    {TRACE_IT(49969);
        return JavascriptNumber::ToVarInPlace(value, scriptContext, result);
    }

    // Alias for overloaded JavascriptNumber::ToVar so it can be called unambiguously from native code
    Var JavascriptOperators::UInt32ToVar(uint32 value, ScriptContext* scriptContext)
    {TRACE_IT(49970);
        return JavascriptNumber::ToVar(value, scriptContext);
    }

    // Alias for overloaded JavascriptNumber::ToVar so it can be called unambiguously from native code
    Var JavascriptOperators::UInt32ToVarInPlace(uint32 value, ScriptContext* scriptContext, JavascriptNumber* result)
    {TRACE_IT(49971);
        return JavascriptNumber::ToVarInPlace(value, scriptContext, result);
    }
#endif

    Var JavascriptOperators::OP_FinishOddDivBy2(uint32 value, ScriptContext *scriptContext)
    {TRACE_IT(49972);
        return JavascriptNumber::New((double)(value + 0.5), scriptContext);
    }

    Var JavascriptOperators::ToNumberInPlace(Var aRight, ScriptContext* scriptContext, JavascriptNumber* result)
    {TRACE_IT(49973);
        if (TaggedInt::Is(aRight) || JavascriptNumber::Is_NoTaggedIntCheck(aRight))
        {TRACE_IT(49974);
            return aRight;
        }

        return JavascriptNumber::ToVarInPlace(JavascriptConversion::ToNumber(aRight, scriptContext), scriptContext, result);
    }

    Var JavascriptOperators::Typeof(Var var, ScriptContext* scriptContext)
    {TRACE_IT(49975);
#ifdef ENABLE_SIMDJS
        if (SIMDUtils::IsSimdType(var) && scriptContext->GetConfig()->IsSimdjsEnabled())
        {TRACE_IT(49976);
            switch ((JavascriptOperators::GetTypeId(var)))
            {
            case TypeIds_SIMDFloat32x4:
                return scriptContext->GetLibrary()->GetSIMDFloat32x4DisplayString();
            //case TypeIds_SIMDFloat64x2:  //Type under review by the spec.
            //    return scriptContext->GetLibrary()->GetSIMDFloat64x2DisplayString();
            case TypeIds_SIMDInt32x4:
                return scriptContext->GetLibrary()->GetSIMDInt32x4DisplayString();
            case TypeIds_SIMDInt16x8:
                return scriptContext->GetLibrary()->GetSIMDInt16x8DisplayString();
            case TypeIds_SIMDInt8x16:
                return scriptContext->GetLibrary()->GetSIMDInt8x16DisplayString();
            case TypeIds_SIMDUint32x4:
                return scriptContext->GetLibrary()->GetSIMDUint32x4DisplayString();
            case TypeIds_SIMDUint16x8:
                return scriptContext->GetLibrary()->GetSIMDUint16x8DisplayString();
            case TypeIds_SIMDUint8x16:
                return scriptContext->GetLibrary()->GetSIMDUint8x16DisplayString();
            case TypeIds_SIMDBool32x4:
                return scriptContext->GetLibrary()->GetSIMDBool32x4DisplayString();
            case TypeIds_SIMDBool16x8:
                return scriptContext->GetLibrary()->GetSIMDBool16x8DisplayString();
            case TypeIds_SIMDBool8x16:
                return scriptContext->GetLibrary()->GetSIMDBool8x16DisplayString();
            default:
                Assert(UNREACHED);
            }
        }
#endif
        //All remaining types.
        switch (JavascriptOperators::GetTypeId(var))
        {
        case TypeIds_Undefined:
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();
        case TypeIds_Null:
            //null
            return scriptContext->GetLibrary()->GetObjectTypeDisplayString();
        case TypeIds_Integer:
        case TypeIds_Number:
        case TypeIds_Int64Number:
        case TypeIds_UInt64Number:
            return scriptContext->GetLibrary()->GetNumberTypeDisplayString();
        default:
            // Falsy objects are typeof 'undefined'.
            if (RecyclableObject::FromVar(var)->GetType()->IsFalsy())
            {TRACE_IT(49977);
                return scriptContext->GetLibrary()->GetUndefinedDisplayString();
            }
            else
            {TRACE_IT(49978);
                return RecyclableObject::FromVar(var)->GetTypeOfString(scriptContext);
            }
        }
    }


    Var JavascriptOperators::TypeofFld(Var instance, PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(49979);
        return TypeofFld_Internal(instance, false, propertyId, scriptContext);
    }

    Var JavascriptOperators::TypeofRootFld(Var instance, PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(49980);
        return TypeofFld_Internal(instance, true, propertyId, scriptContext);
    }

    Var JavascriptOperators::TypeofFld_Internal(Var instance, const bool isRoot, PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(49981);
        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(49982);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined , scriptContext->GetPropertyName(propertyId)->GetBuffer());
        }

        Var value;
        try
        {TRACE_IT(49983);
            Js::JavascriptExceptionOperators::AutoCatchHandlerExists autoCatchHandlerExists(scriptContext);

            // In edge mode, spec compat is more important than backward compat. Use spec/web behavior here
            if (isRoot
                    ? !JavascriptOperators::GetRootProperty(instance, propertyId, &value, scriptContext)
                    : !JavascriptOperators::GetProperty(instance, object, propertyId, &value, scriptContext))
            {TRACE_IT(49984);
                return scriptContext->GetLibrary()->GetUndefinedDisplayString();
            }
            if (!scriptContext->IsUndeclBlockVar(value))
            {TRACE_IT(49985);
                return JavascriptOperators::Typeof(value, scriptContext);
            }
        }
        catch(const JavascriptException& err)
        {TRACE_IT(49986);
            err.GetAndClear();  // discard exception object
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }

        Assert(scriptContext->IsUndeclBlockVar(value));
        JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
    }


    Var JavascriptOperators::TypeofElem_UInt32(Var instance, uint32 index, ScriptContext* scriptContext)
    {TRACE_IT(49987);
        if (JavascriptOperators::IsNumberFromNativeArray(instance, index, scriptContext))
            return scriptContext->GetLibrary()->GetNumberTypeDisplayString();

#if FLOATVAR
        return TypeofElem(instance, Js::JavascriptNumber::ToVar(index, scriptContext), scriptContext);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return TypeofElem(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext);
#endif
    }

    Var JavascriptOperators::TypeofElem_Int32(Var instance, int32 index, ScriptContext* scriptContext)
    {TRACE_IT(49988);
        if (JavascriptOperators::IsNumberFromNativeArray(instance, index, scriptContext))
            return scriptContext->GetLibrary()->GetNumberTypeDisplayString();

#if FLOATVAR
        return TypeofElem(instance, Js::JavascriptNumber::ToVar(index, scriptContext), scriptContext);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return TypeofElem(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext);
#endif

    }

    Js::JavascriptString* GetPropertyDisplayNameForError(Var prop, ScriptContext* scriptContext)
    {TRACE_IT(49989);
        JavascriptString* str;

        if (JavascriptSymbol::Is(prop))
        {TRACE_IT(49990);
            str = JavascriptSymbol::ToString(JavascriptSymbol::FromVar(prop)->GetValue(), scriptContext);
        }
        else
        {TRACE_IT(49991);
            str = JavascriptConversion::ToString(prop, scriptContext);
        }

        return str;
    }

    Var JavascriptOperators::TypeofElem(Var instance, Var index, ScriptContext* scriptContext)
    {TRACE_IT(49992);
        RecyclableObject* object = nullptr;

        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(49993);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined, GetPropertyDisplayNameForError(index, scriptContext));
        }

        Var member;
        uint32 indexVal;
        PropertyRecord const * propertyRecord = nullptr;

        ThreadContext* threadContext = scriptContext->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        try
        {TRACE_IT(49994);
            Js::JavascriptExceptionOperators::AutoCatchHandlerExists autoCatchHandlerExists(scriptContext);
            IndexType indexType = GetIndexType(index, scriptContext, &indexVal, &propertyRecord, false);

            // For JS Objects, don't create the propertyId if not already added
            if (indexType == IndexType_Number)
            {TRACE_IT(49995);
                // In edge mode, we don't need to worry about the special "unknown" behavior. If the item is not available from Get,
                // just return undefined.
                if (!JavascriptOperators::GetItem(instance, object, indexVal, &member, scriptContext))
                {TRACE_IT(49996);
                    // If the instance doesn't have the item, typeof result is "undefined".
                    threadContext->CheckAndResetImplicitCallAccessorFlag();
                    threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
                    return scriptContext->GetLibrary()->GetUndefinedDisplayString();
                }
            }
            else
            {TRACE_IT(49997);
                Assert(indexType == IndexType_PropertyId);
                if (propertyRecord == nullptr && !JavascriptOperators::CanShortcutOnUnknownPropertyName(object))
                {TRACE_IT(49998);
                    indexType = GetIndexTypeFromPrimitive(index, scriptContext, &indexVal, &propertyRecord, true);
                    Assert(indexType == IndexType_PropertyId);
                    Assert(propertyRecord != nullptr);
                }

                if (propertyRecord != nullptr)
                {TRACE_IT(49999);
                    if (!JavascriptOperators::GetProperty(instance, object, propertyRecord->GetPropertyId(), &member, scriptContext))
                    {TRACE_IT(50000);
                        // If the instance doesn't have the property, typeof result is "undefined".
                        threadContext->CheckAndResetImplicitCallAccessorFlag();
                        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
                        return scriptContext->GetLibrary()->GetUndefinedDisplayString();
                    }
                }
                else
                {TRACE_IT(50001);
#if DBG
                    JavascriptString* indexStr = JavascriptConversion::ToString(index, scriptContext);
                    PropertyRecord const * debugPropertyRecord;
                    scriptContext->GetOrAddPropertyRecord(indexStr->GetString(), indexStr->GetLength(), &debugPropertyRecord);
                    AssertMsg(!JavascriptOperators::GetProperty(instance, object, debugPropertyRecord->GetPropertyId(), &member, scriptContext), "how did this property come? See OS Bug 2727708 if you see this come from the web");
#endif

                    // If the instance doesn't have the property, typeof result is "undefined".
                    threadContext->CheckAndResetImplicitCallAccessorFlag();
                    threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
                    return scriptContext->GetLibrary()->GetUndefinedDisplayString();
                }
            }
            threadContext->CheckAndResetImplicitCallAccessorFlag();
            threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
            return JavascriptOperators::Typeof(member, scriptContext);
        }
        catch(const JavascriptException& err)
        {TRACE_IT(50002);
            err.GetAndClear();  // discard exception object
            threadContext->CheckAndResetImplicitCallAccessorFlag();
            threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }
    }

    //
    // Delete the given Var
    //
    Var JavascriptOperators::Delete(Var var, ScriptContext* scriptContext)
    {TRACE_IT(50003);
        return scriptContext->GetLibrary()->GetTrue();
    }

    BOOL JavascriptOperators::Equal_Full(Var aLeft, Var aRight, ScriptContext* requestContext)
    {TRACE_IT(50004);
        //
        // Fast-path SmInts and paired Number combinations.
        //

        if (aLeft == aRight)
        {TRACE_IT(50005);
            if (JavascriptNumber::Is(aLeft) && JavascriptNumber::IsNan(JavascriptNumber::GetValue(aLeft)))
            {TRACE_IT(50006);
                return false;
            }
            else if (JavascriptVariantDate::Is(aLeft) == false) // only need to check on aLeft - since they are the same var, aRight would do the same
            {TRACE_IT(50007);
                return true;
            }
            else
            {TRACE_IT(50008);
                //In ES5 mode strict equals (===) on same instance of object type VariantDate succeeds.
                //Hence equals needs to succeed.
                return true;
            }
        }

        BOOL result = false;

        if (TaggedInt::Is(aLeft))
        {TRACE_IT(50009);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(50010);
                // If aLeft == aRight, we would already have returned true above.
                return false;
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(50011);
                return TaggedInt::ToDouble(aLeft) == JavascriptNumber::GetValue(aRight);
            }
            else
            {TRACE_IT(50012);
                BOOL res = RecyclableObject::FromVar(aRight)->Equals(aLeft, &result, requestContext);
                AssertMsg(res, "Should have handled this");
                return result;
            }
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft))
        {TRACE_IT(50013);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(50014);
                return TaggedInt::ToDouble(aRight) == JavascriptNumber::GetValue(aLeft);
            }
            else if(JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(50015);
                return JavascriptNumber::GetValue(aLeft) == JavascriptNumber::GetValue(aRight);
            }
            else
            {TRACE_IT(50016);
                BOOL res = RecyclableObject::FromVar(aRight)->Equals(aLeft, &result, requestContext);
                AssertMsg(res, "Should have handled this");
                return result;
            }
        }
#ifdef ENABLE_SIMDJS
        else if (SIMDUtils::IsSimdType(aLeft) && SIMDUtils::IsSimdType(aRight))
        {TRACE_IT(50017);
            return StrictEqualSIMD(aLeft, aRight, requestContext);
        }
#endif

        if (RecyclableObject::FromVar(aLeft)->Equals(aRight, &result, requestContext))
        {TRACE_IT(50018);
            return result;
        }
        else
        {TRACE_IT(50019);
            return false;
        }
    }

    BOOL JavascriptOperators::Greater_Full(Var aLeft,Var aRight,ScriptContext* scriptContext)
    {TRACE_IT(50020);
        return RelationalComparisonHelper(aRight, aLeft, scriptContext, false, false);
    }

    BOOL JavascriptOperators::Less_Full(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(50021);
        return RelationalComparisonHelper(aLeft, aRight, scriptContext, true, false);
    }

    BOOL JavascriptOperators::RelationalComparisonHelper(Var aLeft, Var aRight, ScriptContext* scriptContext, bool leftFirst, bool undefinedAs)
    {TRACE_IT(50022);
        TypeId typeId = JavascriptOperators::GetTypeId(aLeft);

        if (typeId == TypeIds_Null)
        {TRACE_IT(50023);
            aLeft=TaggedInt::ToVarUnchecked(0);
        }
        else if (typeId == TypeIds_Undefined)
        {TRACE_IT(50024);
            aLeft=scriptContext->GetLibrary()->GetNaN();
        }

        typeId = JavascriptOperators::GetTypeId(aRight);

        if (typeId == TypeIds_Null)
        {TRACE_IT(50025);
            aRight=TaggedInt::ToVarUnchecked(0);
        }
        else if (typeId == TypeIds_Undefined)
        {TRACE_IT(50026);
            aRight=scriptContext->GetLibrary()->GetNaN();
        }

        double dblLeft, dblRight;

#ifdef ENABLE_SIMDJS
        if (SIMDUtils::IsSimdType(aLeft) || SIMDUtils::IsSimdType(aRight))
        {TRACE_IT(50027);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_SIMDConversion, _u("SIMD type"));
        }
#endif
        TypeId leftType = JavascriptOperators::GetTypeId(aLeft);
        TypeId rightType = JavascriptOperators::GetTypeId(aRight);

        switch (leftType)
        {
        case TypeIds_Integer:
            dblLeft = TaggedInt::ToDouble(aLeft);
            switch (rightType)
            {
            case TypeIds_Integer:
                dblRight = TaggedInt::ToDouble(aRight);
                break;
            case TypeIds_Number:
                dblRight = JavascriptNumber::GetValue(aRight);
                break;
            default:
                dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
                break;
            }
            break;
        case TypeIds_Number:
            dblLeft = JavascriptNumber::GetValue(aLeft);
            switch (rightType)
            {
            case TypeIds_Integer:
                dblRight = TaggedInt::ToDouble(aRight);
                break;
            case TypeIds_Number:
                dblRight = JavascriptNumber::GetValue(aRight);
                break;
            default:
                dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
                break;
            }
            break;
        case TypeIds_Int64Number:
            {TRACE_IT(50028);
                switch (rightType)
                {
                case TypeIds_Int64Number:
                    {TRACE_IT(50029);
                        __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                        __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                        return leftValue < rightValue;
                    }
                    break;
                case TypeIds_UInt64Number:
                    {TRACE_IT(50030);
                        __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                        unsigned __int64 rightValue = JavascriptUInt64Number::FromVar(aRight)->GetValue();
                        if (rightValue <= INT_MAX && leftValue >= 0)
                        {TRACE_IT(50031);
                            return leftValue < (__int64)rightValue;
                        }
                    }
                    break;
                }
                dblLeft = (double)JavascriptInt64Number::FromVar(aLeft)->GetValue();
                dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
            }
            break;

        // we cannot do double conversion between 2 int64 numbers as we can get wrong result after conversion
        // i.e., two different numbers become the same after losing precision. We'll continue dbl comparison
        // if either number is not an int64 number.
        case TypeIds_UInt64Number:
            {TRACE_IT(50032);
                switch (rightType)
                {
                case TypeIds_Int64Number:
                    {TRACE_IT(50033);
                        unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                        __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                        if (leftValue < INT_MAX && rightValue >= 0)
                        {TRACE_IT(50034);
                            return (__int64)leftValue < rightValue;
                        }
                    }
                    break;
                case TypeIds_UInt64Number:
                    {TRACE_IT(50035);
                        unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                        unsigned __int64 rightValue = JavascriptUInt64Number::FromVar(aRight)->GetValue();
                        return leftValue < rightValue;
                    }
                    break;
                }
                dblLeft = (double)JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
            }
            break;
        case TypeIds_String:
            switch (rightType)
            {
            case TypeIds_Integer:
            case TypeIds_Number:
            case TypeIds_Boolean:
                break;
            default:
                aRight = JavascriptConversion::ToPrimitive(aRight, JavascriptHint::HintNumber, scriptContext);
                rightType = JavascriptOperators::GetTypeId(aRight);
                if (rightType != TypeIds_String)
                {TRACE_IT(50036);
                    dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
                    break;
                }
            case TypeIds_String:
                return JavascriptString::LessThan(aLeft, aRight);
            }
            dblLeft = JavascriptConversion::ToNumber(aLeft, scriptContext);
            dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
            break;
        case TypeIds_Boolean:
        case TypeIds_Null:
        case TypeIds_Undefined:
        case TypeIds_Symbol:
            dblLeft = JavascriptConversion::ToNumber(aLeft, scriptContext);
            dblRight = JavascriptConversion::ToNumber(aRight, scriptContext);
            break;
        default:
            if (leftFirst)
            {TRACE_IT(50037);
                aLeft = JavascriptConversion::ToPrimitive(aLeft, JavascriptHint::HintNumber, scriptContext);
                aRight = JavascriptConversion::ToPrimitive(aRight, JavascriptHint::HintNumber, scriptContext);
            }
            else
            {TRACE_IT(50038);
                aRight = JavascriptConversion::ToPrimitive(aRight, JavascriptHint::HintNumber, scriptContext);
                aLeft = JavascriptConversion::ToPrimitive(aLeft, JavascriptHint::HintNumber, scriptContext);
            }
            //BugFix: When @@ToPrimitive of an object is overridden with a function that returns null/undefined
            //this helper will fall into a inescapable goto loop as the checks for null/undefined were outside of the path
            return RelationalComparisonHelper(aLeft, aRight, scriptContext, leftFirst, undefinedAs);
        }

        //
        // And +0,-0 that is not implemented fully
        //

        if (JavascriptNumber::IsNan(dblLeft) || JavascriptNumber::IsNan(dblRight))
        {TRACE_IT(50039);
            return undefinedAs;
        }

        // this will succeed for -0.0 == 0.0 case as well
        if (dblLeft == dblRight)
        {TRACE_IT(50040);
            return false;
        }

        return dblLeft < dblRight;
    }

#ifdef ENABLE_SIMDJS
    BOOL JavascriptOperators::StrictEqualSIMD(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(50041);
        TypeId leftTid  = JavascriptOperators::GetTypeId(aLeft);
        TypeId rightTid = JavascriptOperators::GetTypeId(aRight);
        bool result = false;


        if (leftTid != rightTid)
        {TRACE_IT(50042);
            return result;
        }
        SIMDValue leftSimd;
        SIMDValue rightSimd;
        switch (leftTid)
        {
        case TypeIds_SIMDBool8x16:
            leftSimd = JavascriptSIMDBool8x16::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDBool8x16::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDBool16x8:
            leftSimd = JavascriptSIMDBool16x8::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDBool16x8::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDBool32x4:
            leftSimd = JavascriptSIMDBool32x4::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDBool32x4::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDInt8x16:
            leftSimd = JavascriptSIMDInt8x16::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDInt8x16::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDInt16x8:
            leftSimd = JavascriptSIMDInt16x8::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDInt16x8::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDInt32x4:
            leftSimd = JavascriptSIMDInt32x4::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDInt32x4::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDUint8x16:
            leftSimd = JavascriptSIMDUint8x16::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDUint8x16::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDUint16x8:
            leftSimd = JavascriptSIMDUint16x8::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDUint16x8::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDUint32x4:
            leftSimd = JavascriptSIMDUint32x4::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDUint32x4::FromVar(aRight)->GetValue();
            return (leftSimd == rightSimd);
        case TypeIds_SIMDFloat32x4:
            leftSimd = JavascriptSIMDFloat32x4::FromVar(aLeft)->GetValue();
            rightSimd = JavascriptSIMDFloat32x4::FromVar(aRight)->GetValue();
            result = true;
            for (int i = 0; i < 4; ++i)
            {TRACE_IT(50043);
                Var laneVarLeft  = JavascriptNumber::ToVarWithCheck(leftSimd.f32[i], scriptContext);
                Var laneVarRight = JavascriptNumber::ToVarWithCheck(rightSimd.f32[i], scriptContext);
                result = result && JavascriptOperators::Equal(laneVarLeft, laneVarRight, scriptContext);
            }
            return result;
        default:
            Assert(UNREACHED);
        }
        return result;
    }
#endif

    BOOL JavascriptOperators::StrictEqualString(Var aLeft, Var aRight)
    {TRACE_IT(50044);
        Assert(JavascriptOperators::GetTypeId(aRight) == TypeIds_String);

        if (JavascriptOperators::GetTypeId(aLeft) != TypeIds_String)
            return false;

        return JavascriptString::Equals(aLeft, aRight);
    }

    BOOL JavascriptOperators::StrictEqualEmptyString(Var aLeft)
    {TRACE_IT(50045);
        TypeId leftType = JavascriptOperators::GetTypeId(aLeft);
        if (leftType != TypeIds_String)
            return false;

        return JavascriptString::FromVar(aLeft)->GetLength() == 0;
    }

    BOOL JavascriptOperators::StrictEqual(Var aLeft, Var aRight, ScriptContext* requestContext)
    {TRACE_IT(50046);
        double dblLeft, dblRight;
        TypeId rightType, leftType;
        leftType = JavascriptOperators::GetTypeId(aLeft);

        // Because NaN !== NaN, we may not return TRUE when typeId is Number
        if (aLeft == aRight && leftType != TypeIds_Number) return TRUE;

        rightType = JavascriptOperators::GetTypeId(aRight);

        switch (leftType)
        {
        case TypeIds_String:
            switch (rightType)
            {
            case TypeIds_String:
                return JavascriptString::Equals(aLeft, aRight);
            }
            return FALSE;
        case TypeIds_Integer:
            switch (rightType)
            {
            case TypeIds_Integer:
                return aLeft == aRight;
            // we don't need to worry about int64: it cannot equal as we create
            // JavascriptInt64Number only in overflow scenarios.
            case TypeIds_Number:
                dblLeft     = TaggedInt::ToDouble(aLeft);
                dblRight    = JavascriptNumber::GetValue(aRight);
                goto CommonNumber;
            }
            return FALSE;
        case TypeIds_Int64Number:
            switch (rightType)
            {
            case TypeIds_Int64Number:
                {TRACE_IT(50047);
                    __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                    __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                    return leftValue == rightValue;
                }
            case TypeIds_UInt64Number:
                {TRACE_IT(50048);
                    __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                    unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                    return ((unsigned __int64)leftValue == rightValue);
                }
            case TypeIds_Number:
                dblLeft     = (double)JavascriptInt64Number::FromVar(aLeft)->GetValue();
                dblRight    = JavascriptNumber::GetValue(aRight);
                goto CommonNumber;
            }
            return FALSE;
        case TypeIds_UInt64Number:
            switch (rightType)
            {
            case TypeIds_Int64Number:
                {TRACE_IT(50049);
                    unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                    __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                    return (leftValue == (unsigned __int64)rightValue);
                }
            case TypeIds_UInt64Number:
                {TRACE_IT(50050);
                    unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                    unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                    return leftValue == rightValue;
                }
            case TypeIds_Number:
                dblLeft     = (double)JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                dblRight    = JavascriptNumber::GetValue(aRight);
                goto CommonNumber;
            }
            return FALSE;

        case TypeIds_Number:
            switch (rightType)
            {
            case TypeIds_Integer:
                dblLeft     = JavascriptNumber::GetValue(aLeft);
                dblRight    = TaggedInt::ToDouble(aRight);
                goto CommonNumber;
            case TypeIds_Int64Number:
                dblLeft     = JavascriptNumber::GetValue(aLeft);
                dblRight = (double)JavascriptInt64Number::FromVar(aRight)->GetValue();
                goto CommonNumber;
            case TypeIds_UInt64Number:
                dblLeft     = JavascriptNumber::GetValue(aLeft);
                dblRight = (double)JavascriptUInt64Number::FromVar(aRight)->GetValue();
                goto CommonNumber;
            case TypeIds_Number:
                dblLeft     = JavascriptNumber::GetValue(aLeft);
                dblRight    = JavascriptNumber::GetValue(aRight);
CommonNumber:
                return FEqualDbl(dblLeft, dblRight);
            }
            return FALSE;
        case TypeIds_Boolean:
            switch (rightType)
            {
            case TypeIds_Boolean:
                return aLeft == aRight;
            }
            return FALSE;

        case TypeIds_Undefined:
            return rightType == TypeIds_Undefined;

        case TypeIds_Null:
            return rightType == TypeIds_Null;

        case TypeIds_Array:
            return (rightType == TypeIds_Array && aLeft == aRight);

        case TypeIds_Symbol:
            switch (rightType)
            {
            case TypeIds_Symbol:
                {TRACE_IT(50051);
                    const PropertyRecord* leftValue = JavascriptSymbol::FromVar(aLeft)->GetValue();
                    const PropertyRecord* rightValue = JavascriptSymbol::FromVar(aRight)->GetValue();
                    return leftValue == rightValue;
                }
            }
            return false;

        case TypeIds_GlobalObject:
        case TypeIds_HostDispatch:
            switch (rightType)
            {
                case TypeIds_HostDispatch:
                case TypeIds_GlobalObject:
                {TRACE_IT(50052);
                    BOOL result;
                    if(RecyclableObject::FromVar(aLeft)->StrictEquals(aRight, &result, requestContext))
                    {TRACE_IT(50053);
                        return result;
                    }
                    return false;
                }
            }
            break;

#ifdef ENABLE_SIMDJS
        case TypeIds_SIMDBool8x16:
        case TypeIds_SIMDInt8x16:
        case TypeIds_SIMDUint8x16:
        case TypeIds_SIMDBool16x8:
        case TypeIds_SIMDInt16x8:
        case TypeIds_SIMDUint16x8:
        case TypeIds_SIMDBool32x4:
        case TypeIds_SIMDInt32x4:
        case TypeIds_SIMDUint32x4:
        case TypeIds_SIMDFloat32x4:
        case TypeIds_SIMDFloat64x2:
            return StrictEqualSIMD(aLeft, aRight, requestContext);
            break;
#endif
        }

        if (RecyclableObject::FromVar(aLeft)->CanHaveInterceptors())
        {TRACE_IT(50054);
            BOOL result;
            if (RecyclableObject::FromVar(aLeft)->StrictEquals(aRight, &result, requestContext))
            {TRACE_IT(50055);
                if (result)
                {TRACE_IT(50056);
                    return TRUE;
                }
            }
        }

        if (!TaggedNumber::Is(aRight) && RecyclableObject::FromVar(aRight)->CanHaveInterceptors())
        {TRACE_IT(50057);
            BOOL result;
            if (RecyclableObject::FromVar(aRight)->StrictEquals(aLeft, &result, requestContext))
            {TRACE_IT(50058);
                if (result)
                {TRACE_IT(50059);
                    return TRUE;
                }
            }
        }

        return aLeft == aRight;
    }

    BOOL JavascriptOperators::HasOwnProperty(Var instance, PropertyId propertyId, ScriptContext *requestContext)
    {TRACE_IT(50060);
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50061);
            return FALSE;
        }
        else
        {TRACE_IT(50062);
            RecyclableObject* object = RecyclableObject::FromVar(instance);

            if (JavascriptProxy::Is(instance))
            {TRACE_IT(50063);
                PropertyDescriptor desc;
                return GetOwnPropertyDescriptor(object, propertyId, requestContext, &desc);
            }
            else
            {TRACE_IT(50064);
                PropertyString *propString = requestContext->TryGetPropertyString(propertyId);
                if (propString != nullptr)
                {TRACE_IT(50065);
                    const PropertyCache *propCache = propString->GetPropertyCache();
                    if (object->GetType() == propCache->type)
                    {TRACE_IT(50066);
                        // The type cached for the property was the same as the type of this object
                        // (i.e. obj in obj.hasOwnProperty), so we know the answer is "true".
                        Assert(TRUE == (object && object->HasOwnProperty(propertyId))); // sanity check on the fastpath result
                        return TRUE;
                    }
                }

                return object && object->HasOwnProperty(propertyId);
            }
        }
    }

    BOOL JavascriptOperators::GetOwnAccessors(Var instance, PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext)
    {TRACE_IT(50067);
        BOOL result;
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50068);
            result = false;
        }
        else
        {TRACE_IT(50069);
            RecyclableObject* object = RecyclableObject::FromVar(instance);
            result = object && object->GetAccessors(propertyId, getter, setter, requestContext);
        }
        return result;
    }

    JavascriptArray* JavascriptOperators::GetOwnPropertyNames(Var instance, ScriptContext *scriptContext)
    {TRACE_IT(50070);
        RecyclableObject *object = RecyclableObject::FromVar(ToObject(instance, scriptContext));

        if (JavascriptProxy::Is(instance))
        {TRACE_IT(50071);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(instance);
            return proxy->PropertyKeysTrap(JavascriptProxy::KeysTrapKind::GetOwnPropertyNamesKind, scriptContext);
        }

        return JavascriptObject::CreateOwnStringPropertiesHelper(object, scriptContext);
    }

    JavascriptArray* JavascriptOperators::GetOwnPropertySymbols(Var instance, ScriptContext *scriptContext)
    {TRACE_IT(50072);
        RecyclableObject *object = RecyclableObject::FromVar(ToObject(instance, scriptContext));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Object_Constructor_getOwnPropertySymbols);

        if (JavascriptProxy::Is(instance))
        {TRACE_IT(50073);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(instance);
            return proxy->PropertyKeysTrap(JavascriptProxy::KeysTrapKind::GetOwnPropertySymbolKind, scriptContext);
        }

        return JavascriptObject::CreateOwnSymbolPropertiesHelper(object, scriptContext);
    }

    JavascriptArray* JavascriptOperators::GetOwnPropertyKeys(Var instance, ScriptContext* scriptContext)
    {TRACE_IT(50074);
        RecyclableObject *object = RecyclableObject::FromVar(ToObject(instance, scriptContext));

        if (JavascriptProxy::Is(instance))
        {TRACE_IT(50075);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(instance);
            return proxy->PropertyKeysTrap(JavascriptProxy::KeysTrapKind::KeysKind, scriptContext);
        }

        return JavascriptObject::CreateOwnStringSymbolPropertiesHelper(object, scriptContext);
    }

    JavascriptArray* JavascriptOperators::GetOwnEnumerablePropertyNames(Var instance, ScriptContext* scriptContext)
    {TRACE_IT(50076);
        RecyclableObject *object = RecyclableObject::FromVar(ToObject(instance, scriptContext));

        if (JavascriptProxy::Is(instance))
        {TRACE_IT(50077);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(instance);
            JavascriptArray* proxyResult = proxy->PropertyKeysTrap(JavascriptProxy::KeysTrapKind::GetOwnPropertyNamesKind, scriptContext);
            JavascriptArray* proxyResultToReturn = scriptContext->GetLibrary()->CreateArray(0);

            // filter enumerable keys
            uint32 resultLength = proxyResult->GetLength();
            Var element;
            const Js::PropertyRecord *propertyRecord;
            uint32 index = 0;
            for (uint32 i = 0; i < resultLength; i++)
            {TRACE_IT(50078);
                element = proxyResult->DirectGetItem(i);

                Assert(!JavascriptSymbol::Is(element));

                PropertyDescriptor propertyDescriptor;
                JavascriptConversion::ToPropertyKey(element, scriptContext, &propertyRecord);
                if (JavascriptOperators::GetOwnPropertyDescriptor(RecyclableObject::FromVar(instance), propertyRecord->GetPropertyId(), scriptContext, &propertyDescriptor))
                {TRACE_IT(50079);
                    if (propertyDescriptor.IsEnumerable())
                    {TRACE_IT(50080);
                        proxyResultToReturn->DirectSetItemAt(index++, CrossSite::MarshalVar(scriptContext, element));
                    }
                }
            }
            return proxyResultToReturn;
        }
        return JavascriptObject::CreateOwnEnumerableStringPropertiesHelper(object, scriptContext);
    }

    JavascriptArray* JavascriptOperators::GetOwnEnumerablePropertyNamesSymbols(Var instance, ScriptContext* scriptContext)
    {TRACE_IT(50081);
        RecyclableObject *object = RecyclableObject::FromVar(ToObject(instance, scriptContext));

        if (JavascriptProxy::Is(instance))
        {TRACE_IT(50082);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(instance);
            return proxy->PropertyKeysTrap(JavascriptProxy::KeysTrapKind::KeysKind, scriptContext);
        }
        return JavascriptObject::CreateOwnEnumerableStringSymbolPropertiesHelper(object, scriptContext);
    }

    BOOL JavascriptOperators::GetOwnProperty(Var instance, PropertyId propertyId, Var* value, ScriptContext* requestContext)
    {TRACE_IT(50083);
        BOOL result;
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50084);
            result = false;
        }
        else
        {TRACE_IT(50085);
            RecyclableObject* object = RecyclableObject::FromVar(instance);
            result = object && object->GetProperty(object, propertyId, value, NULL, requestContext);
        }
        return result;
    }

    BOOL JavascriptOperators::GetOwnPropertyDescriptor(RecyclableObject* obj, JavascriptString* propertyKey, ScriptContext* scriptContext, PropertyDescriptor* propertyDescriptor)
    {TRACE_IT(50086);
        return JavascriptOperators::GetOwnPropertyDescriptor(obj, JavascriptOperators::GetPropertyId(propertyKey, scriptContext), scriptContext, propertyDescriptor);
    }

    // ES5's [[GetOwnProperty]].
    // Return value:
    //   FALSE means "undefined" PD.
    //   TRUE means success. The propertyDescriptor parameter gets the descriptor.
    //
    BOOL JavascriptOperators::GetOwnPropertyDescriptor(RecyclableObject* obj, PropertyId propertyId, ScriptContext* scriptContext, PropertyDescriptor* propertyDescriptor)
    {TRACE_IT(50087);
        Assert(obj);
        Assert(scriptContext);
        Assert(propertyDescriptor);

        if (JavascriptProxy::Is(obj))
        {TRACE_IT(50088);
            return JavascriptProxy::GetOwnPropertyDescriptor(obj, propertyId, scriptContext, propertyDescriptor);
        }
        Var getter, setter;
        if (false == JavascriptOperators::GetOwnAccessors(obj, propertyId, &getter, &setter, scriptContext))
        {TRACE_IT(50089);
            Var value;
            if (false == JavascriptOperators::GetOwnProperty(obj, propertyId, &value, scriptContext))
            {TRACE_IT(50090);
                return FALSE;
            }
            if (nullptr != value)
            {TRACE_IT(50091);
                propertyDescriptor->SetValue(value);
            }

            //CONSIDER : Its expensive to query for each flag from type system. Combine this with the GetOwnProperty to get all the flags
            //at once. This will require a new API from type system and override in all the types which overrides IsEnumerable etc.
            //Currently there is no performance tuning for ES5. This should be ok.
            propertyDescriptor->SetWritable(FALSE != obj->IsWritable(propertyId));
        }
        else
        {TRACE_IT(50092);
            if (nullptr == getter)
            {TRACE_IT(50093);
                getter = scriptContext->GetLibrary()->GetUndefined();
            }
            propertyDescriptor->SetGetter(getter);

            if (nullptr == setter)
            {TRACE_IT(50094);
                setter = scriptContext->GetLibrary()->GetUndefined();
            }
            propertyDescriptor->SetSetter(setter);
        }

        propertyDescriptor->SetConfigurable(FALSE != obj->IsConfigurable(propertyId));
        propertyDescriptor->SetEnumerable(FALSE != obj->IsEnumerable(propertyId));
        return TRUE;
    }

    inline RecyclableObject* JavascriptOperators::GetPrototypeNoTrap(RecyclableObject* instance)
    {TRACE_IT(50095);
        Type* type = instance->GetType();
        if (type->HasSpecialPrototype())
        {TRACE_IT(50096);
            if (type->GetTypeId() == TypeIds_Proxy)
            {TRACE_IT(50097);
                // get back null
                Assert(type->GetPrototype() == instance->GetScriptContext()->GetLibrary()->GetNull());
                return type->GetPrototype();
            }
            else
            {TRACE_IT(50098);
                return instance->GetPrototypeSpecial();
            }
        }
        return type->GetPrototype();
    }

    BOOL JavascriptOperators::IsArray(Var instanceVar)
    {TRACE_IT(50099);
        if (!RecyclableObject::Is(instanceVar))
        {TRACE_IT(50100);
            return FALSE;
        }
        RecyclableObject* instance = RecyclableObject::FromVar(instanceVar);
        if (DynamicObject::IsAnyArray(instance))
        {TRACE_IT(50101);
            return TRUE;
        }
        if (JavascriptProxy::Is(instanceVar))
        {TRACE_IT(50102);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(instanceVar);
            return IsArray(proxy->GetTarget());
        }
        TypeId remoteTypeId = TypeIds_Limit;
        if (JavascriptOperators::GetRemoteTypeId(instanceVar, &remoteTypeId) &&
            DynamicObject::IsAnyArrayTypeId(remoteTypeId))
        {TRACE_IT(50103);
            return TRUE;
        }
        return FALSE;
    }

    BOOL JavascriptOperators::IsConstructor(Var instanceVar)
    {TRACE_IT(50104);
        if (!RecyclableObject::Is(instanceVar))
        {TRACE_IT(50105);
            return FALSE;
        }
        if (JavascriptProxy::Is(instanceVar))
        {TRACE_IT(50106);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(instanceVar);
            return IsConstructor(proxy->GetTarget());
        }
        if (!JavascriptFunction::Is(instanceVar))
        {TRACE_IT(50107);
            return FALSE;
        }
        return JavascriptFunction::FromVar(instanceVar)->IsConstructor();
    }

    BOOL JavascriptOperators::IsConcatSpreadable(Var instanceVar)
    {TRACE_IT(50108);
        // an object is spreadable under two condition, either it is a JsArray
        // or you define an isconcatSpreadable flag on it.
        if (!JavascriptOperators::IsObject(instanceVar))
        {TRACE_IT(50109);
            return false;
        }

        RecyclableObject* instance = RecyclableObject::FromVar(instanceVar);
        ScriptContext* scriptContext = instance->GetScriptContext();

        if (!PHASE_OFF1(IsConcatSpreadableCachePhase))
        {TRACE_IT(50110);
            BOOL retVal = FALSE;
            Type *instanceType = instance->GetType();
            IsConcatSpreadableCache *isConcatSpreadableCache = scriptContext->GetThreadContext()->GetIsConcatSpreadableCache();

            if (isConcatSpreadableCache->TryGetIsConcatSpreadable(instanceType, &retVal))
            {TRACE_IT(50111);
                OUTPUT_TRACE(Phase::IsConcatSpreadableCachePhase, _u("IsConcatSpreadableCache hit: %p\n"), instanceType);
                return retVal;
            }

            Var spreadable = nullptr;
            BOOL hasUserDefinedSpreadable = JavascriptOperators::GetProperty(instance, instance, PropertyIds::_symbolIsConcatSpreadable, &spreadable, scriptContext);

            if (hasUserDefinedSpreadable && spreadable != scriptContext->GetLibrary()->GetUndefined())
            {TRACE_IT(50112);
                return JavascriptConversion::ToBoolean(spreadable, scriptContext);
            }

            retVal = JavascriptOperators::IsArray(instance);

            if (!hasUserDefinedSpreadable)
            {TRACE_IT(50113);
                OUTPUT_TRACE(Phase::IsConcatSpreadableCachePhase, _u("IsConcatSpreadableCache saved: %p\n"), instanceType);
                isConcatSpreadableCache->CacheIsConcatSpreadable(instanceType, retVal);
            }

            return retVal;
        }

        Var spreadable = JavascriptOperators::GetProperty(instance, PropertyIds::_symbolIsConcatSpreadable, scriptContext);
        if (spreadable != scriptContext->GetLibrary()->GetUndefined())
        {TRACE_IT(50114);
            return JavascriptConversion::ToBoolean(spreadable, scriptContext);
        }

        return JavascriptOperators::IsArray(instance);
    }

    Var JavascriptOperators::OP_LdCustomSpreadIteratorList(Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(50115);
#if ENABLE_COPYONACCESS_ARRAY
        // We know we're going to read from this array. Do the conversion before we try to perform checks on the head segment.
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray(aRight);
#endif
        RecyclableObject* function = GetIteratorFunction(aRight, scriptContext);
        JavascriptMethod method = function->GetEntryPoint();
        if (((JavascriptArray::Is(aRight) &&
              (
                  method == JavascriptArray::EntryInfo::Values.GetOriginalEntryPoint()
                  // Verify that the head segment of the array covers all elements with no gaps.
                  // Accessing an element on the prototype could have side-effects that would invalidate the optimization.
                  && JavascriptArray::FromVar(aRight)->GetHead()->next == nullptr
                  && JavascriptArray::FromVar(aRight)->GetHead()->left == 0
                  && JavascriptArray::FromVar(aRight)->GetHead()->length == JavascriptArray::FromVar(aRight)->GetLength()
                  && JavascriptArray::FromVar(aRight)->HasNoMissingValues()
              )) ||
             (TypedArrayBase::Is(aRight) && method == TypedArrayBase::EntryInfo::Values.GetOriginalEntryPoint()))
            // We can't optimize away the iterator if the array iterator prototype is user defined.
            && !JavascriptLibrary::ArrayIteratorPrototypeHasUserDefinedNext(scriptContext))
        {TRACE_IT(50116);
            return aRight;
        }

        ThreadContext *threadContext = scriptContext->GetThreadContext();

        Var iteratorVar =
            threadContext->ExecuteImplicitCall(function, ImplicitCall_Accessor, [=]() -> Var
                {
                    return CALL_FUNCTION(function, CallInfo(Js::CallFlags_Value, 1), aRight);
                });

        if (!JavascriptOperators::IsObject(iteratorVar))
        {TRACE_IT(50117);
            if (!threadContext->RecordImplicitException())
            {TRACE_IT(50118);
                return scriptContext->GetLibrary()->GetUndefined();
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
        }

        RecyclableObject* iterator = RecyclableObject::FromVar(iteratorVar);
        return RecyclerNew(scriptContext->GetRecycler(), SpreadArgument, aRight, iterator, scriptContext->GetLibrary()->GetSpreadArgumentType());
    }

    BOOL JavascriptOperators::IsPropertyUnscopable(Var instanceVar, JavascriptString *propertyString)
    {TRACE_IT(50119);
        // This never gets called.
        Throw::InternalError();
    }

    BOOL JavascriptOperators::IsPropertyUnscopable(Var instanceVar, PropertyId propertyId)
    {TRACE_IT(50120);
        RecyclableObject* instance = RecyclableObject::FromVar(instanceVar);
        ScriptContext * scriptContext = instance->GetScriptContext();

        Var unscopables = JavascriptOperators::GetProperty(instance, PropertyIds::_symbolUnscopables, scriptContext);
        if (JavascriptOperators::IsObject(unscopables))
        {TRACE_IT(50121);
            DynamicObject *unscopablesList = DynamicObject::FromVar(unscopables);
            Var value;
            //8.1.1.2.1.9.c If blocked is not undefined
            if (JavascriptOperators::GetProperty(unscopablesList, propertyId, &value, scriptContext))
            {TRACE_IT(50122);
                return JavascriptConversion::ToBoolean(value, scriptContext);
            }
        }

        return false;
    }

    BOOL JavascriptOperators::HasProperty(RecyclableObject* instance, PropertyId propertyId)
    {TRACE_IT(50123);
        while (JavascriptOperators::GetTypeId(instance) != TypeIds_Null)
        {TRACE_IT(50124);
            if (instance->HasProperty(propertyId))
            {TRACE_IT(50125);
                return true;
            }
            instance = JavascriptOperators::GetPrototypeNoTrap(instance);
        }
        return false;
    }

    BOOL JavascriptOperators::HasPropertyUnscopables(RecyclableObject* instance, PropertyId propertyId)
    {TRACE_IT(50126);
        return JavascriptOperators::HasProperty(instance, propertyId)
            && !IsPropertyUnscopable(instance, propertyId);
    }

    BOOL JavascriptOperators::HasRootProperty(RecyclableObject* instance, PropertyId propertyId)
    {TRACE_IT(50127);
        Assert(RootObjectBase::Is(instance));

        RootObjectBase* rootObject = static_cast<RootObjectBase*>(instance);
        if (rootObject->HasRootProperty(propertyId))
        {TRACE_IT(50128);
            return true;
        }
        instance = instance->GetPrototype();

        return HasProperty(instance, propertyId);
    }

    BOOL JavascriptOperators::HasProxyOrPrototypeInlineCacheProperty(RecyclableObject* instance, PropertyId propertyId)
    {TRACE_IT(50129);
        TypeId typeId;
        typeId = JavascriptOperators::GetTypeId(instance);
        if (typeId == Js::TypeIds_Proxy)
        {TRACE_IT(50130);
            // let's be more aggressive to disable inline prototype cache when proxy is presented in the prototypechain
            return true;
        }
        do
        {TRACE_IT(50131);
            instance = instance->GetPrototype();
            typeId = JavascriptOperators::GetTypeId(instance);
            if (typeId == Js::TypeIds_Proxy)
            {TRACE_IT(50132);
                // let's be more aggressive to disable inline prototype cache when proxy is presented in the prototypechain
                return true;
            }
            if (typeId == TypeIds_Null)
            {TRACE_IT(50133);
                break;
            }
            /* We can rule out object with deferred type handler, because they would have expanded if they are in the cache */
            if (!instance->HasDeferredTypeHandler() && instance->HasProperty(propertyId)) {TRACE_IT(50134); return true; }
        } while (typeId != TypeIds_Null);
        return false;
    }

    BOOL JavascriptOperators::OP_HasProperty(Var instance, PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(50135);
        RecyclableObject* object = TaggedNumber::Is(instance) ?
            scriptContext->GetLibrary()->GetNumberPrototype() :
            RecyclableObject::FromVar(instance);
        BOOL result = HasProperty(object, propertyId);
        return result;
    }

    BOOL JavascriptOperators::OP_HasOwnProperty(Var instance, PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(50136);
        RecyclableObject* object = TaggedNumber::Is(instance) ?
            scriptContext->GetLibrary()->GetNumberPrototype() :
            RecyclableObject::FromVar(instance);
        BOOL result = HasOwnProperty(object, propertyId, scriptContext);
        return result;
    }

    // CONSIDER: Have logic similar to HasOwnPropertyNoHostObjectForHeapEnum
    BOOL JavascriptOperators::HasOwnPropertyNoHostObject(Var instance, PropertyId propertyId)
    {TRACE_IT(50137);
        AssertMsg(!TaggedNumber::Is(instance), "HasOwnPropertyNoHostObject int passed");

        RecyclableObject* object = RecyclableObject::FromVar(instance);
        return object && object->HasOwnPropertyNoHostObject(propertyId);
    }

    // CONSIDER: Remove HasOwnPropertyNoHostObjectForHeapEnum and use GetOwnPropertyNoHostObjectForHeapEnum in its place by changing it
    // to return BOOL, true or false with whether the property exists or not, and return the value if not getter/setter as an out param.
    BOOL JavascriptOperators::HasOwnPropertyNoHostObjectForHeapEnum(Var instance, PropertyId propertyId, ScriptContext* requestContext, Var& getter, Var& setter)
    {TRACE_IT(50138);
        AssertMsg(!TaggedNumber::Is(instance), "HasOwnPropertyNoHostObjectForHeapEnum int passed");

        RecyclableObject * object = RecyclableObject::FromVar(instance);
        if (StaticType::Is(object->GetTypeId()))
        {TRACE_IT(50139);
            return FALSE;
        }
        getter = setter = NULL;
        DynamicObject* dynamicObject = DynamicObject::FromVar(instance);
        Assert(dynamicObject->GetScriptContext()->IsHeapEnumInProgress());
        if (dynamicObject->UseDynamicObjectForNoHostObjectAccess())
        {TRACE_IT(50140);
            if (!dynamicObject->DynamicObject::GetAccessors(propertyId, &getter, &setter, requestContext))
            {TRACE_IT(50141);
                Var value;
                if (!dynamicObject->DynamicObject::GetProperty(instance, propertyId, &value, NULL, requestContext) ||
                    (requestContext->IsUndeclBlockVar(value) && (ActivationObject::Is(instance) || RootObjectBase::Is(instance))))
                {TRACE_IT(50142);
                    return FALSE;
                }
            }
        }
        else
        {TRACE_IT(50143);
            if (!object->GetAccessors(propertyId, &getter, &setter, requestContext))
            {TRACE_IT(50144);
                Var value;
                if (!object->GetProperty(instance, propertyId, &value, NULL, requestContext) ||
                    (requestContext->IsUndeclBlockVar(value) && (ActivationObject::Is(instance) || RootObjectBase::Is(instance))))
                {TRACE_IT(50145);
                    return FALSE;
                }
            }
        }
        return TRUE;
    }

    Var JavascriptOperators::GetOwnPropertyNoHostObjectForHeapEnum(Var instance, PropertyId propertyId, ScriptContext* requestContext, Var& getter, Var& setter)
    {TRACE_IT(50146);
        AssertMsg(!TaggedNumber::Is(instance), "GetDataPropertyNoHostObject int passed");
        Assert(HasOwnPropertyNoHostObjectForHeapEnum(instance, propertyId, requestContext, getter, setter) || getter || setter);
        DynamicObject* dynamicObject = DynamicObject::FromVar(instance);
        getter = setter = NULL;
        if (NULL == dynamicObject)
        {TRACE_IT(50147);
            return requestContext->GetLibrary()->GetUndefined();
        }
        Var returnVar = requestContext->GetLibrary()->GetUndefined();
        BOOL result = FALSE;
        if (dynamicObject->UseDynamicObjectForNoHostObjectAccess())
        {TRACE_IT(50148);
            if (! dynamicObject->DynamicObject::GetAccessors(propertyId, &getter, &setter, requestContext))
            {TRACE_IT(50149);
                result = dynamicObject->DynamicObject::GetProperty(instance, propertyId, &returnVar, NULL, requestContext);
            }
        }
        else
        {TRACE_IT(50150);
            if (! dynamicObject->GetAccessors(propertyId, &getter, &setter, requestContext))
            {TRACE_IT(50151);
                result = dynamicObject->GetProperty(instance, propertyId, &returnVar, NULL, requestContext);
            }
        }

        if (result)
        {TRACE_IT(50152);
            return returnVar;
        }
        return requestContext->GetLibrary()->GetUndefined();
    }


    BOOL JavascriptOperators::OP_HasOwnPropScoped(Var scope, PropertyId propertyId, Var defaultInstance, ScriptContext* scriptContext)
    {TRACE_IT(50153);
        AssertMsg(scope == scriptContext->GetLibrary()->GetNull() || JavascriptArray::Is(scope),
                  "Invalid scope chain pointer passed - should be null or an array");
        if (JavascriptArray::Is(scope))
        {TRACE_IT(50154);
            JavascriptArray* arrScope = JavascriptArray::FromVar(scope);
            Var instance = arrScope->DirectGetItem(0);
            return JavascriptOperators::OP_HasOwnProperty(instance, propertyId, scriptContext);
        }
        return JavascriptOperators::OP_HasOwnProperty(defaultInstance, propertyId, scriptContext);
    }

    BOOL JavascriptOperators::GetPropertyUnscopable(Var instance, RecyclableObject* propertyObject, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(50155);
        return GetProperty_Internal<true>(instance, propertyObject, false, propertyId, value, requestContext, info);
    }

    BOOL JavascriptOperators::GetProperty(Var instance, RecyclableObject* propertyObject, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(50156);
        return GetProperty_Internal<false>(instance, propertyObject, false, propertyId, value, requestContext, info);
    }

    BOOL JavascriptOperators::GetRootProperty(Var instance, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(50157);
        return GetProperty_Internal<false>(instance, RecyclableObject::FromVar(instance), true, propertyId, value, requestContext, info);
    }

    template <bool unscopables>
    BOOL JavascriptOperators::GetProperty_Internal(Var instance, RecyclableObject* propertyObject, const bool isRoot, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(50158);
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50159);
            PropertyValueInfo::ClearCacheInfo(info);
        }
        RecyclableObject* object = propertyObject;
        BOOL foundProperty = FALSE;
        if (isRoot)
        {TRACE_IT(50160);
            Assert(RootObjectBase::Is(object));

            RootObjectBase* rootObject = static_cast<RootObjectBase*>(object);
            foundProperty = rootObject->GetRootProperty(instance, propertyId, value, info, requestContext);
        }

        while (!foundProperty && JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50161);
            if (unscopables && IsPropertyUnscopable(object, propertyId))
            {TRACE_IT(50162);
                break;
            }
            else
            {TRACE_IT(50163);
                if (object->GetProperty(instance, propertyId, value, info, requestContext))
                {TRACE_IT(50164);
                    foundProperty = true;
                    break;
                }
            }

            if (object->SkipsPrototype())
            {TRACE_IT(50165);
                break;
            }

            object = JavascriptOperators::GetPrototypeNoTrap(object);
        }

        if (foundProperty)
        {TRACE_IT(50166);
#if DBG
            if (DynamicObject::Is(object))
            {TRACE_IT(50167);
                DynamicObject* dynamicObject = (DynamicObject*)object;
                DynamicTypeHandler* dynamicTypeHandler = dynamicObject->GetDynamicType()->GetTypeHandler();
                Var property;
                if (dynamicTypeHandler->CheckFixedProperty(requestContext->GetPropertyName(propertyId), &property, requestContext))
                {TRACE_IT(50168);
                    Assert(value == nullptr || *value == property);
                }
            }
#endif
            // Don't cache the information if the value is undecl block var
            // REVIEW: We might want to only check this if we need to (For LdRootFld or ScopedLdFld)
            //         Also we might want to throw here instead of checking it again in the caller
            if (value && !requestContext->IsUndeclBlockVar(*value) && !WithScopeObject::Is(object))
            {TRACE_IT(50169);
                CacheOperators::CachePropertyRead(instance, object, isRoot, propertyId, false, info, requestContext);
            }
#ifdef TELEMETRY_JSO
            if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
            {TRACE_IT(50170);
                requestContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(instance, propertyId, value, /*successful: */true);
            }
#endif

            return TRUE;
        }
        else
        {TRACE_IT(50171);
#ifdef MISSING_PROPERTY_STATS
            if (PHASE_STATS1(MissingPropertyCachePhase))
            {TRACE_IT(50172);
                requestContext->RecordMissingPropertyMiss();
            }
#endif
            if (PHASE_TRACE1(MissingPropertyCachePhase))
            {TRACE_IT(50173);
                Output::Print(_u("MissingPropertyCaching: Missing property %d on slow path.\n"), propertyId);
            }

            // Only cache missing property lookups for non-root field loads on objects that have PathTypeHandlers, because only these objects guarantee a type change when the property is added,
            // which obviates the need to explicitly invalidate missing property inline caches.
            if (!PHASE_OFF1(MissingPropertyCachePhase) && !isRoot && DynamicObject::Is(instance) && ((DynamicObject*)instance)->GetDynamicType()->GetTypeHandler()->IsPathTypeHandler())
            {TRACE_IT(50174);
#ifdef MISSING_PROPERTY_STATS
                if (PHASE_STATS1(MissingPropertyCachePhase))
                {TRACE_IT(50175);
                    requestContext->RecordMissingPropertyCacheAttempt();
                }
#endif
                if (PHASE_TRACE1(MissingPropertyCachePhase))
                {TRACE_IT(50176);
                    Output::Print(_u("MissingPropertyCache: Caching missing property for property %d.\n"), propertyId);
                }

                PropertyValueInfo::Set(info, requestContext->GetLibrary()->GetMissingPropertyHolder(), 0);
                CacheOperators::CachePropertyRead(instance, requestContext->GetLibrary()->GetMissingPropertyHolder(), isRoot, propertyId, true, info, requestContext);
            }
#if defined(TELEMETRY_JSO) || defined(TELEMETRY_AddToCache) // enabled for `TELEMETRY_AddToCache`, because this is the property-not-found codepath where the normal TELEMETRY_AddToCache code wouldn't be executed.
            if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
            {TRACE_IT(50177);
                if (info && info->AllowResizingPolymorphicInlineCache()) // If in interpreted mode, not JIT.
                {TRACE_IT(50178);
                    requestContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(instance, propertyId, nullptr, /*successful: */false);
                }
            }
#endif
            *value = requestContext->GetMissingPropertyResult();
            return FALSE;
        }
    }

    template<typename PropertyKeyType>
    BOOL JavascriptOperators::GetPropertyWPCache(Var instance, RecyclableObject* propertyObject, PropertyKeyType propertyKey, Var* value, ScriptContext* requestContext, PropertyString * propertyString)
    {TRACE_IT(50179);
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50180);
            propertyString = NULL;
        }
        PropertyValueInfo info;
        RecyclableObject* object = propertyObject;
        while (JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50181);
            if (object->GetProperty(instance, propertyKey, value, &info, requestContext))
            {TRACE_IT(50182);
                if (propertyString != NULL)
                {TRACE_IT(50183);
                    uint16 slotIndex = info.GetPropertyIndex();
                    if (slotIndex != Constants::NoSlot &&
                        info.GetInstance() == object &&
                        info.IsWritable() && !object->CanHaveInterceptors() &&
                        requestContext == object->GetScriptContext() &&
                        ((info.GetFlags() & (InlineCacheGetterFlag | InlineCacheSetterFlag)) == 0))
                    {TRACE_IT(50184);
                        uint16 inlineOrAuxSlotIndex;
                        bool isInlineSlot;
                        DynamicObject::FromVar(info.GetInstance())->GetTypeHandler()->PropertyIndexToInlineOrAuxSlotIndex(slotIndex, &inlineOrAuxSlotIndex, &isInlineSlot);
                        propertyString->UpdateCache(info.GetInstance()->GetType(), inlineOrAuxSlotIndex, isInlineSlot, info.IsStoreFieldCacheEnabled());
                    }
                }
                return TRUE;
            }
            if (object->SkipsPrototype())
            {TRACE_IT(50185);
                break;
            }
            object = JavascriptOperators::GetPrototypeNoTrap(object);
        }
        *value = requestContext->GetMissingPropertyResult();
        return FALSE;
    }

    BOOL JavascriptOperators::GetPropertyObject(Var instance, ScriptContext * scriptContext, RecyclableObject** propertyObject)
    {TRACE_IT(50186);
        Assert(propertyObject);
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50187);
            *propertyObject = scriptContext->GetLibrary()->GetNumberPrototype();
            return TRUE;
        }
        RecyclableObject* object = RecyclableObject::FromVar(instance);
        TypeId typeId = object->GetTypeId();
        *propertyObject = object;
        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {TRACE_IT(50188);
            return FALSE;
        }
        return TRUE;
    }

#if DBG
    BOOL JavascriptOperators::IsPropertyObject(RecyclableObject * instance)
    {TRACE_IT(50189);
        TypeId typeId = JavascriptOperators::GetTypeId(instance);
        return (typeId != TypeIds_Integer && typeId != TypeIds_Null && typeId != TypeIds_Undefined);
    }
#endif

    Var JavascriptOperators::OP_GetProperty(Var instance, PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(50190);
        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(50191);
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(50192);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined, scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            else
            {TRACE_IT(50193);
                return scriptContext->GetLibrary()->GetUndefined();
            }
        }

        Var result = JavascriptOperators::GetProperty(instance, object, propertyId, scriptContext);
        AssertMsg(result != nullptr, "result null in OP_GetProperty");
        return result;
    }

    Var JavascriptOperators::OP_GetRootProperty(Var instance, PropertyId propertyId, PropertyValueInfo * info, ScriptContext* scriptContext)
    {TRACE_IT(50194);
        AssertMsg(RootObjectBase::Is(instance), "Root must be an object!");

        Var value;
        if (JavascriptOperators::GetRootProperty(RecyclableObject::FromVar(instance), propertyId, &value, scriptContext, info))
        {TRACE_IT(50195);
            if (scriptContext->IsUndeclBlockVar(value))
            {TRACE_IT(50196);
                JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
            }
            return value;
        }

        const char16* propertyName = scriptContext->GetPropertyName(propertyId)->GetBuffer();

        JavascriptFunction * caller = nullptr;
        if (JavascriptStackWalker::GetCaller(&caller, scriptContext))
        {TRACE_IT(50197);
            FunctionBody * callerBody = caller->GetFunctionBody();
            if (callerBody && callerBody->GetUtf8SourceInfo()->GetIsXDomain())
            {TRACE_IT(50198);
                propertyName = nullptr;
            }
        }

        // Don't error if we disabled implicit calls
        if (scriptContext->GetThreadContext()->RecordImplicitException())
        {TRACE_IT(50199);
            JavascriptError::ThrowReferenceError(scriptContext, JSERR_UndefVariable, propertyName);
        }

        return scriptContext->GetMissingPropertyResult();
    }

    Var JavascriptOperators::OP_GetThisScoped(FrameDisplay *pScope, Var defaultInstance, ScriptContext* scriptContext)
    {TRACE_IT(50200);
        // NOTE: If changes are made to this logic be sure to update the debuggers as well
        int length = pScope->GetLength();

        for (int i = 0; i < length; i += 1)
        {TRACE_IT(50201);
            Var value;
            DynamicObject *obj = DynamicObject::FromVar(pScope->GetItem(i));
            if (JavascriptOperators::GetProperty(obj, Js::PropertyIds::_lexicalThisSlotSymbol, &value, scriptContext))
            {TRACE_IT(50202);
                return value;
            }
        }

        return defaultInstance;
    }

    Var JavascriptOperators::OP_UnwrapWithObj(Var aValue)
    {TRACE_IT(50203);
        return RecyclableObject::FromVar(aValue)->GetThisObjectOrUnWrap();
    }
    Var JavascriptOperators::OP_GetInstanceScoped(FrameDisplay *pScope, PropertyId propertyId, Var rootObject, Var* thisVar, ScriptContext* scriptContext)
    {TRACE_IT(50204);
        // Similar to GetPropertyScoped, but instead of returning the property value, we return the instance that
        // owns it, or the global object if no instance is found.

        int i;
        int length = pScope->GetLength();

        for (i = 0; i < length; i++)
        {TRACE_IT(50205);
            RecyclableObject *obj = (RecyclableObject*)pScope->GetItem(i);


            if (JavascriptOperators::HasProperty(obj, propertyId))
            {TRACE_IT(50206);
                // HasProperty will call WithObjects HasProperty which will do the filtering
                // All we have to do here is unwrap the object hence the api call

                *thisVar = obj->GetThisObjectOrUnWrap();
                return *thisVar;
            }
        }

        *thisVar = scriptContext->GetLibrary()->GetUndefined();
        if (rootObject != scriptContext->GetGlobalObject())
        {TRACE_IT(50207);
            if (JavascriptOperators::OP_HasProperty(rootObject, propertyId, scriptContext))
            {TRACE_IT(50208);
                return rootObject;
            }
        }

        return scriptContext->GetGlobalObject();
    }

    Var JavascriptOperators::GetPropertyReference(RecyclableObject *instance, PropertyId propertyId, ScriptContext* requestContext)
    {TRACE_IT(50209);
        Var value = nullptr;
        PropertyValueInfo info;
        if (JavascriptOperators::GetPropertyReference(instance, propertyId, &value, requestContext, &info))
        {TRACE_IT(50210);
            Assert(value != nullptr);
            return value;
        }
        return requestContext->GetMissingPropertyResult();
    }

    BOOL JavascriptOperators::GetPropertyReference(Var instance, RecyclableObject* propertyObject, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(50211);
        return GetPropertyReference_Internal(instance, propertyObject, false, propertyId, value, requestContext, info);
    }

    BOOL JavascriptOperators::GetRootPropertyReference(RecyclableObject* instance, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(50212);
        return GetPropertyReference_Internal(instance, instance, true, propertyId, value, requestContext, info);
    }

    BOOL JavascriptOperators::PropertyReferenceWalkUnscopable(Var instance, RecyclableObject** propertyObject, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(50213);
        return PropertyReferenceWalk_Impl<true>(instance, propertyObject, propertyId, value, info, requestContext);
    }

    BOOL JavascriptOperators::PropertyReferenceWalk(Var instance, RecyclableObject** propertyObject, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(50214);
        return PropertyReferenceWalk_Impl<false>(instance, propertyObject, propertyId, value, info, requestContext);
    }

    template <bool unscopables>
    BOOL JavascriptOperators::PropertyReferenceWalk_Impl(Var instance, RecyclableObject** propertyObject, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(50215);
        BOOL foundProperty = false;
        RecyclableObject* object = *propertyObject;
        while (!foundProperty && JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50216);
            if (unscopables && JavascriptOperators::IsPropertyUnscopable(object, propertyId))
            {TRACE_IT(50217);
                break;
            }
            else
            {TRACE_IT(50218);
                if (object->GetPropertyReference(instance, propertyId, value, info, requestContext))
                {TRACE_IT(50219);
                    foundProperty = true;
                    break;
                }
            }

            if (object->SkipsPrototype())
            {TRACE_IT(50220);
                break; // will return false
            }

            object = JavascriptOperators::GetPrototypeNoTrap(object);

        }
        *propertyObject = object;
        return foundProperty;
    }

    BOOL JavascriptOperators::GetPropertyReference_Internal(Var instance, RecyclableObject* propertyObject, const bool isRoot, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(50221);
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50222);
            PropertyValueInfo::ClearCacheInfo(info);
        }
        BOOL foundProperty = FALSE;
        RecyclableObject* object = propertyObject;

        if (isRoot)
        {TRACE_IT(50223);
            foundProperty = RootObjectBase::FromVar(object)->GetRootPropertyReference(instance, propertyId, value, info, requestContext);
        }
        if (!foundProperty)
        {TRACE_IT(50224);
            foundProperty = PropertyReferenceWalk(instance, &object, propertyId, value, info, requestContext);
        }

        if (!foundProperty)
        {TRACE_IT(50225);
#if defined(TELEMETRY_JSO) || defined(TELEMETRY_AddToCache) // enabled for `TELEMETRY_AddToCache`, because this is the property-not-found codepath where the normal TELEMETRY_AddToCache code wouldn't be executed.
            if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
            {TRACE_IT(50226);
                if (info && info->AllowResizingPolymorphicInlineCache()) // If in interpreted mode, not JIT.
                {TRACE_IT(50227);
                    requestContext->GetTelemetry().GetOpcodeTelemetry().GetProperty(instance, propertyId, nullptr, /*successful: */false);
                }
            }
#endif
            *value = requestContext->GetMissingPropertyResult();
            return foundProperty;
        }

        if (requestContext->IsUndeclBlockVar(*value))
        {TRACE_IT(50228);
            JavascriptError::ThrowReferenceError(requestContext, JSERR_UseBeforeDeclaration);
        }
#if DBG
        if (DynamicObject::Is(object))
        {TRACE_IT(50229);
            DynamicObject* dynamicObject = (DynamicObject*)object;
            DynamicTypeHandler* dynamicTypeHandler = dynamicObject->GetDynamicType()->GetTypeHandler();
            Var property;
            if (dynamicTypeHandler->CheckFixedProperty(requestContext->GetPropertyName(propertyId), &property, requestContext))
            {TRACE_IT(50230);
                Assert(value == nullptr || *value == property);
            }
        }
#endif

        CacheOperators::CachePropertyRead(instance, object, isRoot, propertyId, false, info, requestContext);
        return TRUE;
    }

    template <typename PropertyKeyType, bool unscopable>
    DescriptorFlags JavascriptOperators::GetterSetter_Impl(RecyclableObject* instance, PropertyKeyType propertyKey, Var* setterValue, PropertyValueInfo* info, ScriptContext* scriptContext)
    {TRACE_IT(50231);
        DescriptorFlags flags = None;
        RecyclableObject* object = instance;
        while (flags == None && JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50232);

            if (unscopable && IsPropertyUnscopable(object, propertyKey))
            {TRACE_IT(50233);
                break;
            }
            else
            {TRACE_IT(50234);

                flags = object->GetSetter(propertyKey, setterValue, info, scriptContext);
                if (flags != None)
                {TRACE_IT(50235);
                    break;
                }
            }
            // CONSIDER: we should add SkipsPrototype support. DOM has no ES 5 concepts built in that aren't
            // already part of our prototype objects which are chakra objects.
            object = object->GetPrototype();
        }
        return flags;
    }

    DescriptorFlags JavascriptOperators::GetterSetterUnscopable(RecyclableObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* scriptContext)
    {TRACE_IT(50236);
        return GetterSetter_Impl<PropertyId, true>(instance, propertyId, setterValue, info, scriptContext);
    }

    DescriptorFlags JavascriptOperators::GetterSetter(RecyclableObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* scriptContext)
    {TRACE_IT(50237);
        return GetterSetter_Impl<PropertyId, false>(instance, propertyId, setterValue, info, scriptContext);
    }

    DescriptorFlags JavascriptOperators::GetterSetter(RecyclableObject* instance, JavascriptString * propertyName, Var* setterValue, PropertyValueInfo* info, ScriptContext* scriptContext)
    {TRACE_IT(50238);
        return GetterSetter_Impl<JavascriptString*, false>(instance, propertyName, setterValue, info, scriptContext);
    }

    void JavascriptOperators::OP_InvalidateProtoCaches(PropertyId propertyId, ScriptContext *scriptContext)
    {TRACE_IT(50239);
        scriptContext->InvalidateProtoCaches(propertyId);
    }

    // Checks to see if any object in the prototype chain has a property descriptor for the given index
    // that specifies either an accessor or a non-writable attribute.
    // If TRUE, check flags for details.
    BOOL JavascriptOperators::CheckPrototypesForAccessorOrNonWritableItem(RecyclableObject* instance, uint32 index,
        Var* setterValue, DescriptorFlags *flags, ScriptContext* scriptContext, BOOL skipPrototypeCheck /* = FALSE */)
    {TRACE_IT(50240);
        Assert(setterValue);
        Assert(flags);

        // Do a quick walk up the prototype chain to see if any of the prototypes has ever had ANY setter or non-writable property.
        if (CheckIfObjectAndPrototypeChainHasOnlyWritableDataProperties(instance))
        {TRACE_IT(50241);
            return FALSE;
        }

        RecyclableObject* object = instance;
        while (JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50242);
            *flags = object->GetItemSetter(index, setterValue, scriptContext);
            if (*flags != None || skipPrototypeCheck)
            {TRACE_IT(50243);
                break;
            }
            object = object->GetPrototype();
        }

        return ((*flags & Accessor) == Accessor) || ((*flags & Proxy) == Proxy) || ((*flags & Data) == Data && (*flags & Writable) == None);
    }

    BOOL JavascriptOperators::SetGlobalPropertyNoHost(char16 const * propertyName, charcount_t propertyLength, Var value, ScriptContext * scriptContext)
    {TRACE_IT(50244);
        GlobalObject * globalObject = scriptContext->GetGlobalObject();
        uint32 index;
        PropertyRecord const * propertyRecord;
        IndexType indexType = GetIndexTypeFromString(propertyName, propertyLength, scriptContext, &index, &propertyRecord, true);

        if (indexType == IndexType_Number)
        {TRACE_IT(50245);
            return globalObject->DynamicObject::SetItem(index, value, PropertyOperation_None);
        }
        return globalObject->DynamicObject::SetProperty(propertyRecord->GetPropertyId(), value, PropertyOperation_None, NULL);
    }

    template<typename PropertyKeyType>
    BOOL JavascriptOperators::SetPropertyWPCache(Var receiver, RecyclableObject* object, PropertyKeyType propertyKey, Var newValue, ScriptContext* requestContext, PropertyString * propertyString, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50246);
        if (receiver)
        {TRACE_IT(50247);
            AnalysisAssert(object);
            Assert(!TaggedNumber::Is(receiver));
            Var setterValueOrProxy = nullptr;
            DescriptorFlags flags = None;
            if (JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(object, propertyKey, &setterValueOrProxy, &flags, NULL, requestContext))
            {TRACE_IT(50248);
                if ((flags & Accessor) == Accessor)
                {TRACE_IT(50249);
                    if (JavascriptError::ThrowIfStrictModeUndefinedSetter(propertyOperationFlags, setterValueOrProxy, requestContext))
                    {TRACE_IT(50250);
                        return TRUE;
                    }
                    if (setterValueOrProxy)
                    {TRACE_IT(50251);
                        receiver = (RecyclableObject::FromVar(receiver))->GetThisObjectOrUnWrap();
                        RecyclableObject* func = RecyclableObject::FromVar(setterValueOrProxy);
                        JavascriptOperators::CallSetter(func, receiver, newValue, requestContext);
                    }
                    return TRUE;
                }
                else if ((flags & Proxy) == Proxy)
                {TRACE_IT(50252);
                    Assert(JavascriptProxy::Is(setterValueOrProxy));
                    JavascriptProxy* proxy = JavascriptProxy::FromVar(setterValueOrProxy);
                    auto fn = [&](RecyclableObject* target) -> BOOL {TRACE_IT(50253);
                        return JavascriptOperators::SetPropertyWPCache(receiver, target, propertyKey, newValue, requestContext, propertyString, propertyOperationFlags);
                    };
                    return proxy->SetPropertyTrap(receiver, JavascriptProxy::SetPropertyTrapKind::SetPropertyWPCacheKind, propertyKey, newValue, requestContext);
                }
                else
                {TRACE_IT(50254);
                    Assert((flags & Data) == Data && (flags & Writable) == None);

                    requestContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
                    JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, requestContext);
                    return FALSE;
                }
            }
            else if (!JavascriptOperators::IsObject(receiver))
            {TRACE_IT(50255);
                JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, requestContext);
                return FALSE;
            }

            RecyclableObject* receiverObject = RecyclableObject::FromVar(receiver);
            if (receiver != object)
            {TRACE_IT(50256);
                // If the receiver object has the property and it is an accessor then return false
                PropertyDescriptor existingDesc;
                if (JavascriptOperators::GetOwnPropertyDescriptor(receiverObject, propertyKey, requestContext, &existingDesc)
                    && existingDesc.IsAccessorDescriptor())
                {TRACE_IT(50257);
                    return FALSE;
                }
            }

            // in 9.1.9, step 5, we should return false if receiver is not object, and that will happen in default RecyclableObject operation anyhow.
            PropertyValueInfo info;
            if (receiverObject->SetProperty(propertyKey, newValue, propertyOperationFlags, &info))
            {TRACE_IT(50258);
                if (propertyString != NULL)
                {TRACE_IT(50259);
                    uint16 slotIndex = info.GetPropertyIndex();
                    if (slotIndex != Constants::NoSlot &&
                        info.GetInstance() == receiverObject &&
                        !object->CanHaveInterceptors() &&
                        requestContext == receiverObject->GetScriptContext() &&
                        (info.GetFlags() != InlineCacheSetterFlag))
                    {TRACE_IT(50260);
                        uint16 inlineOrAuxSlotIndex;
                        bool isInlineSlot;
                        DynamicObject::FromVar(info.GetInstance())->GetTypeHandler()->PropertyIndexToInlineOrAuxSlotIndex(info.GetPropertyIndex(), &inlineOrAuxSlotIndex, &isInlineSlot);
                        propertyString->UpdateCache(info.GetInstance()->GetType(), inlineOrAuxSlotIndex, isInlineSlot, info.IsStoreFieldCacheEnabled());
                    }
                }
                return TRUE;
            }
        }

        return FALSE;
    }

    BOOL JavascriptOperators::SetItemOnTaggedNumber(Var receiver, RecyclableObject* object, uint32 index, Var newValue, ScriptContext* requestContext,
        PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50261);
        Assert(TaggedNumber::Is(receiver));

        if (requestContext->optimizationOverrides.GetSideEffects() & SideEffects_Accessor)
        {TRACE_IT(50262);
            Var setterValueOrProxy = nullptr;
            DescriptorFlags flags = None;
            if (object == nullptr)
            {
                GetPropertyObject(receiver, requestContext, &object);
            }
            if (JavascriptOperators::CheckPrototypesForAccessorOrNonWritableItem(object, index, &setterValueOrProxy, &flags, requestContext))
            {TRACE_IT(50263);
                if ((flags & Accessor) == Accessor)
                {TRACE_IT(50264);
                    if (JavascriptError::ThrowIfStrictModeUndefinedSetter(propertyOperationFlags, setterValueOrProxy, requestContext))
                    {TRACE_IT(50265);
                        return TRUE;
                    }
                    if (setterValueOrProxy)
                    {TRACE_IT(50266);
                        RecyclableObject* func = RecyclableObject::FromVar(setterValueOrProxy);
                        JavascriptOperators::CallSetter(func, receiver, newValue, requestContext);
                        return TRUE;
                    }
                }
                else if ((flags & Proxy) == Proxy)
                {TRACE_IT(50267);
                    Assert(JavascriptProxy::Is(setterValueOrProxy));
                    JavascriptProxy* proxy = JavascriptProxy::FromVar(setterValueOrProxy);
                    const PropertyRecord* propertyRecord;
                    proxy->PropertyIdFromInt(index, &propertyRecord);
                    return proxy->SetPropertyTrap(receiver, JavascriptProxy::SetPropertyTrapKind::SetItemOnTaggedNumberKind, propertyRecord->GetPropertyId(), newValue, requestContext);
                }
                else
                {TRACE_IT(50268);
                    Assert((flags & Data) == Data && (flags & Writable) == None);
                    JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, requestContext);
                }
            }
        }

        JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, requestContext);
        return FALSE;
    }

    BOOL JavascriptOperators::SetPropertyOnTaggedNumber(Var receiver, RecyclableObject* object, PropertyId propertyId, Var newValue, ScriptContext* requestContext,
        PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50269);
        Assert (TaggedNumber::Is(receiver));

        if (requestContext->optimizationOverrides.GetSideEffects() & SideEffects_Accessor)
        {TRACE_IT(50270);
            Var setterValueOrProxy = nullptr;
            PropertyValueInfo info;
            DescriptorFlags flags = None;
            if (object == nullptr)
            {
                GetPropertyObject(receiver, requestContext, &object);
            }
            if (JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(object, propertyId, &setterValueOrProxy, &flags, &info, requestContext))
            {TRACE_IT(50271);
                if ((flags & Accessor) == Accessor)
                {TRACE_IT(50272);
                    if (JavascriptError::ThrowIfStrictModeUndefinedSetter(propertyOperationFlags, setterValueOrProxy, requestContext))
                    {TRACE_IT(50273);
                        return TRUE;
                    }
                    if (setterValueOrProxy)
                    {TRACE_IT(50274);
                        RecyclableObject* func = RecyclableObject::FromVar(setterValueOrProxy);
                        Assert(info.GetFlags() == InlineCacheSetterFlag || info.GetPropertyIndex() == Constants::NoSlot);
                        JavascriptOperators::CallSetter(func, receiver, newValue, requestContext);
                        return TRUE;
                    }
                }
                else if ((flags & Proxy) == Proxy)
                {TRACE_IT(50275);
                    Assert(JavascriptProxy::Is(setterValueOrProxy));
                    JavascriptProxy* proxy = JavascriptProxy::FromVar(setterValueOrProxy);
                    return proxy->SetPropertyTrap(receiver, JavascriptProxy::SetPropertyTrapKind::SetPropertyOnTaggedNumberKind, propertyId, newValue, requestContext);
                }
                else
                {TRACE_IT(50276);
                    Assert((flags & Data) == Data && (flags & Writable) == None);
                    JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, requestContext);
                }
            }
        }

        // Add implicit call flags, to bail out if field copy prop may propagate the wrong value.
        requestContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
        JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, requestContext);
        return FALSE;
    }

    BOOL JavascriptOperators::SetPropertyUnscopable(Var instance, RecyclableObject* receiver, PropertyId propertyId, Var newValue, PropertyValueInfo * info, ScriptContext* requestContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50277);
        return SetProperty_Internal<true>(instance, receiver, false, propertyId, newValue, info, requestContext, propertyOperationFlags);
    }

    BOOL JavascriptOperators::SetProperty(Var receiver, RecyclableObject* object, PropertyId propertyId, Var newValue, PropertyValueInfo * info, ScriptContext* requestContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50278);
        return SetProperty_Internal<false>(receiver, object, false, propertyId, newValue, info, requestContext, propertyOperationFlags);
    }

    BOOL JavascriptOperators::SetRootProperty(RecyclableObject* instance, PropertyId propertyId, Var newValue, PropertyValueInfo * info, ScriptContext* requestContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50279);
        return SetProperty_Internal<false>(instance, instance, true, propertyId, newValue, info, requestContext, propertyOperationFlags);
    }

    template <bool unscopables>
    BOOL JavascriptOperators::SetProperty_Internal(Var receiver, RecyclableObject* object, const bool isRoot, PropertyId propertyId, Var newValue, PropertyValueInfo * info, ScriptContext* requestContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50280);
        if (receiver)
        {TRACE_IT(50281);
            Assert(!TaggedNumber::Is(receiver));
            Var setterValueOrProxy = nullptr;
            DescriptorFlags flags = None;
            if ((isRoot && JavascriptOperators::CheckPrototypesForAccessorOrNonWritableRootProperty(object, propertyId, &setterValueOrProxy, &flags, info, requestContext)) ||
                (!isRoot && JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(object, propertyId, &setterValueOrProxy, &flags, info, requestContext)))
            {TRACE_IT(50282);
                if ((flags & Accessor) == Accessor)
                {TRACE_IT(50283);
                    if (JavascriptError::ThrowIfStrictModeUndefinedSetter(propertyOperationFlags, setterValueOrProxy, requestContext) ||
                        JavascriptError::ThrowIfNotExtensibleUndefinedSetter(propertyOperationFlags, setterValueOrProxy, requestContext))
                    {TRACE_IT(50284);
                        return TRUE;
                    }
                    if (setterValueOrProxy)
                    {TRACE_IT(50285);
                        RecyclableObject* func = RecyclableObject::FromVar(setterValueOrProxy);
                        Assert(!info || info->GetFlags() == InlineCacheSetterFlag || info->GetPropertyIndex() == Constants::NoSlot);

                        if (WithScopeObject::Is(receiver))
                        {TRACE_IT(50286);
                            receiver = (RecyclableObject::FromVar(receiver))->GetThisObjectOrUnWrap();
                        }
                        else
                        {TRACE_IT(50287);
                            CacheOperators::CachePropertyWrite(RecyclableObject::FromVar(receiver), isRoot, object->GetType(), propertyId, info, requestContext);
                        }
#ifdef ENABLE_MUTATION_BREAKPOINT
                        if (MutationBreakpoint::IsFeatureEnabled(requestContext))
                        {TRACE_IT(50288);
                            MutationBreakpoint::HandleSetProperty(requestContext, object, propertyId, newValue);
                        }
#endif
                        JavascriptOperators::CallSetter(func, receiver, newValue, requestContext);
                    }
                    return TRUE;
                }
                else if ((flags & Proxy) == Proxy)
                {TRACE_IT(50289);
                    Assert(JavascriptProxy::Is(setterValueOrProxy));
                    JavascriptProxy* proxy = JavascriptProxy::FromVar(setterValueOrProxy);
                    // We can't cache the property at this time. both target and handler can be changed outside of the proxy, so the inline cache needs to be
                    // invalidate when target, handler, or handler prototype has changed. We don't have a way to achieve this yet.
                    PropertyValueInfo::SetNoCache(info, proxy);
                    PropertyValueInfo::DisablePrototypeCache(info, proxy); // We can't cache prototype property either

                    return proxy->SetPropertyTrap(receiver, JavascriptProxy::SetPropertyTrapKind::SetPropertyKind, propertyId, newValue, requestContext);
                }
                else
                {TRACE_IT(50290);
                    Assert((flags & Data) == Data && (flags & Writable) == None);
                    if (flags & Const)
                    {TRACE_IT(50291);
                        JavascriptError::ThrowTypeError(requestContext, ERRAssignmentToConst);
                    }

                    JavascriptError::ThrowCantAssign(propertyOperationFlags, requestContext, propertyId);
                    JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, requestContext);
                    return FALSE;
                }
            }
            else if (!JavascriptOperators::IsObject(receiver))
            {TRACE_IT(50292);
                JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, requestContext);
                return FALSE;
            }

#ifdef ENABLE_MUTATION_BREAKPOINT
            // Break on mutation if needed
            bool doNotUpdateCacheForMbp = MutationBreakpoint::IsFeatureEnabled(requestContext) ?
                MutationBreakpoint::HandleSetProperty(requestContext, object, propertyId, newValue) : false;
#endif

            // Get the original type before setting the property
            Type *typeWithoutProperty = object->GetType();
            BOOL didSetProperty = false;
            if (isRoot)
            {TRACE_IT(50293);
                AssertMsg(JavascriptOperators::GetTypeId(receiver) == TypeIds_GlobalObject
                    || JavascriptOperators::GetTypeId(receiver) == TypeIds_ModuleRoot,
                    "Root must be a global object!");

                RootObjectBase* rootObject = static_cast<RootObjectBase*>(receiver);
                didSetProperty = rootObject->SetRootProperty(propertyId, newValue, propertyOperationFlags, info);
            }
            else
            {TRACE_IT(50294);
                RecyclableObject* instanceObject = RecyclableObject::FromVar(receiver);
                while (JavascriptOperators::GetTypeId(instanceObject) != TypeIds_Null)
                {TRACE_IT(50295);
                    if (unscopables && JavascriptOperators::IsPropertyUnscopable(instanceObject, propertyId))
                    {TRACE_IT(50296);
                        break;
                    }
                    else
                    {TRACE_IT(50297);
                        didSetProperty = instanceObject->SetProperty(propertyId, newValue, propertyOperationFlags, info);
                        if (didSetProperty || !unscopables)
                        {TRACE_IT(50298);
                            break;
                        }
                    }
                    instanceObject = JavascriptOperators::GetPrototypeNoTrap(instanceObject);
                }
            }

            if (didSetProperty)
            {TRACE_IT(50299);
                bool updateCache = true;
#ifdef ENABLE_MUTATION_BREAKPOINT
                updateCache = updateCache && !doNotUpdateCacheForMbp;
#endif

                if (updateCache)
                {TRACE_IT(50300);
                    if (!JavascriptProxy::Is(receiver))
                    {TRACE_IT(50301);
                        CacheOperators::CachePropertyWrite(RecyclableObject::FromVar(receiver), isRoot, typeWithoutProperty, propertyId, info, requestContext);
                    }
                }
                return TRUE;
            }
        }

        return FALSE;
    }

    BOOL JavascriptOperators::IsNumberFromNativeArray(Var instance, uint32 index, ScriptContext* scriptContext)
    {TRACE_IT(50302);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        Js::TypeId instanceType = JavascriptOperators::GetTypeId(instance);
        // Fast path for native and typed arrays.
        if ( (instanceType == TypeIds_NativeIntArray || instanceType == TypeIds_NativeFloatArray) || (instanceType >= TypeIds_Int8Array && instanceType <= TypeIds_Uint64Array) )
        {TRACE_IT(50303);
            RecyclableObject* object = RecyclableObject::FromVar(instance);
            Var member;

            // If the item is found in the array own body, then it is a number
            if (JavascriptOperators::GetOwnItem(object, index, &member, scriptContext)
                && !JavascriptOperators::IsUndefined(member))
            {TRACE_IT(50304);
                return TRUE;
            }
        }
        return FALSE;
    }

    BOOL JavascriptOperators::GetAccessors(RecyclableObject* instance, PropertyId propertyId, ScriptContext* requestContext, Var* getter, Var* setter)
    {TRACE_IT(50305);
        RecyclableObject* object = instance;

        while (JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50306);
            if (object->GetAccessors(propertyId, getter, setter, requestContext))
            {TRACE_IT(50307);
                *getter = JavascriptOperators::CanonicalizeAccessor(*getter, requestContext);
                *setter = JavascriptOperators::CanonicalizeAccessor(*setter, requestContext);
                return TRUE;
            }

            if (object->SkipsPrototype())
            {TRACE_IT(50308);
                break;
            }
            object = JavascriptOperators::GetPrototype(object);
        }
        return FALSE;
    }

    BOOL JavascriptOperators::SetAccessors(RecyclableObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(50309);
        BOOL result = instance && instance->SetAccessors(propertyId, getter, setter, flags);
        return result;
    }

    BOOL JavascriptOperators::OP_SetProperty(Var instance, PropertyId propertyId, Var newValue, ScriptContext* scriptContext, PropertyValueInfo * info, PropertyOperationFlags flags, Var thisInstance)
    {TRACE_IT(50310);
        // The call into ToObject(dynamicObject) is avoided here by checking for null and undefined and doing nothing when dynamicObject is a primitive value.
        if (thisInstance == nullptr)
        {TRACE_IT(50311);
            thisInstance = instance;
        }
        TypeId typeId = JavascriptOperators::GetTypeId(thisInstance);

        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {TRACE_IT(50312);
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(50313);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotSet_NullOrUndefined, scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            return TRUE;
        }
        else if (typeId == TypeIds_VariantDate)
        {TRACE_IT(50314);
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(50315);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_VarDate, scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            return TRUE;
        }

        if (!TaggedNumber::Is(thisInstance))
        {TRACE_IT(50316);
            return JavascriptOperators::SetProperty(RecyclableObject::FromVar(thisInstance), RecyclableObject::FromVar(instance), propertyId, newValue, info, scriptContext, flags);
        }

        JavascriptError::ThrowCantAssignIfStrictMode(flags, scriptContext);
        return false;
    }

    BOOL JavascriptOperators::OP_StFunctionExpression(Var obj, PropertyId propertyId, Var newValue)
    {TRACE_IT(50317);
        RecyclableObject* instance = RecyclableObject::FromVar(obj);

        instance->SetProperty(propertyId, newValue, PropertyOperation_None, NULL);
        instance->SetWritable(propertyId, FALSE);
        instance->SetConfigurable(propertyId, FALSE);

        return TRUE;
    }

    BOOL JavascriptOperators::OP_InitClassMember(Var obj, PropertyId propertyId, Var newValue)
    {TRACE_IT(50318);
        RecyclableObject* instance = RecyclableObject::FromVar(obj);

        PropertyOperationFlags flags = PropertyOperation_None;
        PropertyAttributes attributes = PropertyClassMemberDefaults;

        instance->SetPropertyWithAttributes(propertyId, newValue, attributes, NULL, flags);

        return TRUE;
    }

    BOOL JavascriptOperators::OP_InitLetProperty(Var obj, PropertyId propertyId, Var newValue)
    {TRACE_IT(50319);
        RecyclableObject* instance = RecyclableObject::FromVar(obj);

        PropertyOperationFlags flags = instance->GetScriptContext()->IsUndeclBlockVar(newValue) ? PropertyOperation_SpecialValue : PropertyOperation_None;
        PropertyAttributes attributes = PropertyLetDefaults;

        if (RootObjectBase::Is(instance))
        {TRACE_IT(50320);
            attributes |= PropertyLetConstGlobal;
        }

        instance->SetPropertyWithAttributes(propertyId, newValue, attributes, NULL, (PropertyOperationFlags)(flags | PropertyOperation_AllowUndecl));

        return TRUE;
    }

    BOOL JavascriptOperators::OP_InitConstProperty(Var obj, PropertyId propertyId, Var newValue)
    {TRACE_IT(50321);
        RecyclableObject* instance = RecyclableObject::FromVar(obj);

        PropertyOperationFlags flags = instance->GetScriptContext()->IsUndeclBlockVar(newValue) ? PropertyOperation_SpecialValue : PropertyOperation_None;
        PropertyAttributes attributes = PropertyConstDefaults;

        if (RootObjectBase::Is(instance))
        {TRACE_IT(50322);
            attributes |= PropertyLetConstGlobal;
        }

        instance->SetPropertyWithAttributes(propertyId, newValue, attributes, NULL, (PropertyOperationFlags)(flags | PropertyOperation_AllowUndecl));

        return TRUE;
    }

    BOOL JavascriptOperators::OP_InitUndeclRootLetProperty(Var obj, PropertyId propertyId)
    {TRACE_IT(50323);
        RecyclableObject* instance = RecyclableObject::FromVar(obj);

        PropertyOperationFlags flags = static_cast<PropertyOperationFlags>(PropertyOperation_SpecialValue | PropertyOperation_AllowUndecl);
        PropertyAttributes attributes = PropertyLetDefaults | PropertyLetConstGlobal;

        instance->SetPropertyWithAttributes(propertyId, instance->GetLibrary()->GetUndeclBlockVar(), attributes, NULL, flags);

        return TRUE;
    }

    BOOL JavascriptOperators::OP_InitUndeclRootConstProperty(Var obj, PropertyId propertyId)
    {TRACE_IT(50324);
        RecyclableObject* instance = RecyclableObject::FromVar(obj);

        PropertyOperationFlags flags = static_cast<PropertyOperationFlags>(PropertyOperation_SpecialValue | PropertyOperation_AllowUndecl);
        PropertyAttributes attributes = PropertyConstDefaults | PropertyLetConstGlobal;

        instance->SetPropertyWithAttributes(propertyId, instance->GetLibrary()->GetUndeclBlockVar(), attributes, NULL, flags);

        return TRUE;
    }

    BOOL JavascriptOperators::OP_InitUndeclConsoleLetProperty(Var obj, PropertyId propertyId)
    {TRACE_IT(50325);
        FrameDisplay *pScope = (FrameDisplay*)obj;
        AssertMsg(ConsoleScopeActivationObject::Is((DynamicObject*)pScope->GetItem(pScope->GetLength() - 1)), "How come we got this opcode without ConsoleScopeActivationObject?");
        RecyclableObject* instance = RecyclableObject::FromVar(pScope->GetItem(0));
        PropertyOperationFlags flags = static_cast<PropertyOperationFlags>(PropertyOperation_SpecialValue | PropertyOperation_AllowUndecl);
        PropertyAttributes attributes = PropertyLetDefaults;
        instance->SetPropertyWithAttributes(propertyId, instance->GetLibrary()->GetUndeclBlockVar(), attributes, NULL, flags);
        return TRUE;
    }

    BOOL JavascriptOperators::OP_InitUndeclConsoleConstProperty(Var obj, PropertyId propertyId)
    {TRACE_IT(50326);
        FrameDisplay *pScope = (FrameDisplay*)obj;
        AssertMsg(ConsoleScopeActivationObject::Is((DynamicObject*)pScope->GetItem(pScope->GetLength() - 1)), "How come we got this opcode without ConsoleScopeActivationObject?");
        RecyclableObject* instance = RecyclableObject::FromVar(pScope->GetItem(0));
        PropertyOperationFlags flags = static_cast<PropertyOperationFlags>(PropertyOperation_SpecialValue | PropertyOperation_AllowUndecl);
        PropertyAttributes attributes = PropertyConstDefaults;
        instance->SetPropertyWithAttributes(propertyId, instance->GetLibrary()->GetUndeclBlockVar(), attributes, NULL, flags);
        return TRUE;
    }

    BOOL JavascriptOperators::InitProperty(RecyclableObject* instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags)
    {TRACE_IT(50327);
        return instance && instance->InitProperty(propertyId, newValue, flags);
    }

    BOOL JavascriptOperators::OP_InitProperty(Var instance, PropertyId propertyId, Var newValue)
    {TRACE_IT(50328);
        if(TaggedNumber::Is(instance)) {TRACE_IT(50329); return false; }
        return JavascriptOperators::InitProperty(RecyclableObject::FromVar(instance), propertyId, newValue);
    }

    BOOL JavascriptOperators::DeleteProperty(RecyclableObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50330);
        return DeleteProperty_Impl<false>(instance, propertyId, propertyOperationFlags);
    }

    bool JavascriptOperators::ShouldTryDeleteProperty(RecyclableObject* instance, JavascriptString *propertyNameString, PropertyRecord const **pPropertyRecord)
    {TRACE_IT(50331);
        PropertyRecord const *propertyRecord = nullptr;
        if (!JavascriptOperators::CanShortcutOnUnknownPropertyName(instance))
        {TRACE_IT(50332);
            instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        }
        else
        {TRACE_IT(50333);
            instance->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);
        }

        if (propertyRecord == nullptr)
        {TRACE_IT(50334);
            return false;
        }
        *pPropertyRecord = propertyRecord;
        return true;
    }

    BOOL JavascriptOperators::DeleteProperty(RecyclableObject* instance, JavascriptString *propertyNameString, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50335);
#ifdef ENABLE_MUTATION_BREAKPOINT
        ScriptContext *scriptContext = instance->GetScriptContext();
        if (MutationBreakpoint::IsFeatureEnabled(scriptContext)
            && scriptContext->HasMutationBreakpoints())
        {TRACE_IT(50336);
            MutationBreakpoint::HandleDeleteProperty(scriptContext, instance, propertyNameString);
        }
#endif
        return instance->DeleteProperty(propertyNameString, propertyOperationFlags);
    }

    BOOL JavascriptOperators::DeletePropertyUnscopables(RecyclableObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50337);
        return DeleteProperty_Impl<true>(instance, propertyId, propertyOperationFlags);
    }
    template<bool unscopables>
    BOOL JavascriptOperators::DeleteProperty_Impl(RecyclableObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50338);
        if (unscopables && JavascriptOperators::IsPropertyUnscopable(instance, propertyId))
        {TRACE_IT(50339);
            return false;
        }
#ifdef ENABLE_MUTATION_BREAKPOINT
        ScriptContext *scriptContext = instance->GetScriptContext();
        if (MutationBreakpoint::IsFeatureEnabled(scriptContext)
            && scriptContext->HasMutationBreakpoints())
        {TRACE_IT(50340);
            MutationBreakpoint::HandleDeleteProperty(scriptContext, instance, propertyId);
        }
#endif
         // !unscopables will hit the return statement on the first iteration
         return instance->DeleteProperty(propertyId, propertyOperationFlags);
    }

    Var JavascriptOperators::OP_DeleteProperty(Var instance, PropertyId propertyId, ScriptContext* scriptContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50341);
        if(TaggedNumber::Is(instance))
        {TRACE_IT(50342);
            return scriptContext->GetLibrary()->GetTrue();
        }

        TypeId typeId = JavascriptOperators::GetTypeId(instance);
        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {TRACE_IT(50343);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotDelete_NullOrUndefined,
                scriptContext->GetPropertyName(propertyId)->GetBuffer());
        }

        RecyclableObject *recyclableObject = RecyclableObject::FromVar(instance);

        return scriptContext->GetLibrary()->CreateBoolean(
            JavascriptOperators::DeleteProperty(recyclableObject, propertyId, propertyOperationFlags));
    }

    Var JavascriptOperators::OP_DeleteRootProperty(Var instance, PropertyId propertyId, ScriptContext* scriptContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50344);
        AssertMsg(RootObjectBase::Is(instance), "Root must be a global object!");
        RootObjectBase* rootObject = static_cast<RootObjectBase*>(instance);

        return scriptContext->GetLibrary()->CreateBoolean(
            rootObject->DeleteRootProperty(propertyId, propertyOperationFlags));
    }

    template <bool IsFromFullJit, class TInlineCache>
    inline void JavascriptOperators::PatchSetPropertyScoped(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var newValue, Var defaultInstance, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50345);
        // Set the property using a scope stack rather than an individual instance.
        // Walk the stack until we find an instance that has the property and store
        // the new value there.
        //
        // To propagate 'this' pointer, walk up the stack and update scopes
        // where field '_lexicalThisSlotSymbol' exists and stop at the
        // scope where field '_lexicalNewTargetSymbol' also exists, which
        // indicates class constructor.

        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        uint16 length = pDisplay->GetLength();
        DynamicObject *object;

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);

        bool allowUndecInConsoleScope = (propertyOperationFlags & PropertyOperation_AllowUndeclInConsoleScope) == PropertyOperation_AllowUndeclInConsoleScope;
        bool isLexicalThisSlotSymbol = (propertyId == PropertyIds::_lexicalThisSlotSymbol);

        for (uint16 i = 0; i < length; i++)
        {TRACE_IT(50346);
            object = (DynamicObject*)pDisplay->GetItem(i);

            AssertMsg(!ConsoleScopeActivationObject::Is(object) || (i == length - 1), "Invalid location for ConsoleScopeActivationObject");

            Type* type = object->GetType();
            if (CacheOperators::TrySetProperty<true, true, true, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                    object, false, propertyId, newValue, scriptContext, propertyOperationFlags, nullptr, &info))
            {TRACE_IT(50347);
                if (isLexicalThisSlotSymbol && !JavascriptOperators::HasProperty(object, PropertyIds::_lexicalNewTargetSymbol))
                {TRACE_IT(50348);
                    continue;
                }

                return;
            }

            // In scoped set property, we need to set the property when it is available; it could be a setter
            // or normal property. we need to check setter first, and if no setter is available, but HasProperty
            // is true, this must be a normal property.
            // TODO: merge OP_HasProperty and GetSetter in one pass if there is perf problem. In fastDOM we have quite
            // a lot of setters so separating the two might be actually faster.
            Var setterValueOrProxy = nullptr;
            DescriptorFlags flags = None;
            if (JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(object, propertyId, &setterValueOrProxy, &flags, &info, scriptContext))
            {TRACE_IT(50349);
                if ((flags & Accessor) == Accessor)
                {TRACE_IT(50350);
                    if (setterValueOrProxy)
                    {TRACE_IT(50351);
                        JavascriptFunction* func = (JavascriptFunction*)setterValueOrProxy;
                        Assert(info.GetFlags() == InlineCacheSetterFlag || info.GetPropertyIndex() == Constants::NoSlot);
                        CacheOperators::CachePropertyWrite(object, false, type, propertyId, &info, scriptContext);
                        JavascriptOperators::CallSetter(func, object, newValue, scriptContext);
                    }

                    Assert(!isLexicalThisSlotSymbol);
                    return;
                }
                else if ((flags & Proxy) == Proxy)
                {TRACE_IT(50352);
                    Assert(JavascriptProxy::Is(setterValueOrProxy));
                    JavascriptProxy* proxy = JavascriptProxy::FromVar(setterValueOrProxy);
                    auto fn = [&](RecyclableObject* target) -> BOOL {TRACE_IT(50353);
                        return JavascriptOperators::SetProperty(object, target, propertyId, newValue, scriptContext, propertyOperationFlags);
                    };
                    // We can't cache the property at this time. both target and handler can be changed outside of the proxy, so the inline cache needs to be
                    // invalidate when target, handler, or handler prototype has changed. We don't have a way to achieve this yet.
                    PropertyValueInfo::SetNoCache(&info, proxy);
                    PropertyValueInfo::DisablePrototypeCache(&info, proxy); // We can't cache prototype property either
                    proxy->SetPropertyTrap(object, JavascriptProxy::SetPropertyTrapKind::SetPropertyKind, propertyId, newValue, scriptContext);
                }
                else
                {TRACE_IT(50354);
                    Assert((flags & Data) == Data && (flags & Writable) == None);
                    if (!allowUndecInConsoleScope)
                    {TRACE_IT(50355);
                        if (flags & Const)
                        {TRACE_IT(50356);
                            JavascriptError::ThrowTypeError(scriptContext, ERRAssignmentToConst);
                        }

                        Assert(!isLexicalThisSlotSymbol);
                        return;
                    }
                }
            }
            else if (!JavascriptOperators::IsObject(object))
            {TRACE_IT(50357);
                JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, scriptContext);
            }

            // Need to do a "get" of the current value (if any) to make sure that we're not writing to
            // let/const before declaration, but we need to disable implicit calls around the "get",
            // so we need to do a "has" first to make sure the "get" is valid (e.g., "get" on a HostDispatch
            // with implicit calls disabled will always "succeed").
            if (JavascriptOperators::HasProperty(object, propertyId))
            {TRACE_IT(50358);
                DisableImplicitFlags disableImplicitFlags = scriptContext->GetThreadContext()->GetDisableImplicitFlags();
                scriptContext->GetThreadContext()->SetDisableImplicitFlags(DisableImplicitCallAndExceptionFlag);

                Var value;
                BOOL result = JavascriptOperators::GetProperty(object, propertyId, &value, scriptContext, nullptr);

                scriptContext->GetThreadContext()->SetDisableImplicitFlags(disableImplicitFlags);

                if (result && scriptContext->IsUndeclBlockVar(value) && !allowUndecInConsoleScope && !isLexicalThisSlotSymbol)
                {TRACE_IT(50359);
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
                }

                PropertyValueInfo info2;
                PropertyValueInfo::SetCacheInfo(&info2, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
                PropertyOperationFlags setPropertyOpFlags = allowUndecInConsoleScope ? PropertyOperation_AllowUndeclInConsoleScope : PropertyOperation_None;
                object->SetProperty(propertyId, newValue, setPropertyOpFlags, &info2);

#if DBG_DUMP
                if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
                {TRACE_IT(50360);
                    CacheOperators::TraceCache(inlineCache, _u("PatchSetPropertyScoped"), propertyId, scriptContext, object);
                }
#endif
                if (!JavascriptProxy::Is(object) && !allowUndecInConsoleScope)
                {TRACE_IT(50361);
                    CacheOperators::CachePropertyWrite(object, false, type, propertyId, &info2, scriptContext);
                }

                if (isLexicalThisSlotSymbol && !JavascriptOperators::HasProperty(object, PropertyIds::_lexicalNewTargetSymbol))
                {TRACE_IT(50362);
                    continue;
                }

                return;
            }
        }

        Assert(!isLexicalThisSlotSymbol);

        // If we have console scope and no one in the scope had the property add it to console scope
        if ((length > 0) && ConsoleScopeActivationObject::Is(pDisplay->GetItem(length - 1)))
        {TRACE_IT(50363);
            // CheckPrototypesForAccessorOrNonWritableProperty does not check for const in global object. We should check it here.
            if ((length > 1) && GlobalObject::Is(pDisplay->GetItem(length - 2)))
            {TRACE_IT(50364);
                GlobalObject* globalObject = GlobalObject::FromVar(pDisplay->GetItem(length - 2));
                Var setterValue = nullptr;

                DescriptorFlags flags = JavascriptOperators::GetRootSetter(globalObject, propertyId, &setterValue, &info, scriptContext);
                Assert((flags & Accessor) != Accessor);
                Assert((flags & Proxy) != Proxy);
                if ((flags & Data) == Data && (flags & Writable) == None)
                {TRACE_IT(50365);
                    if (!allowUndecInConsoleScope)
                    {TRACE_IT(50366);
                        if (flags & Const)
                        {TRACE_IT(50367);
                            JavascriptError::ThrowTypeError(scriptContext, ERRAssignmentToConst);
                        }
                        Assert(!isLexicalThisSlotSymbol);
                        return;
                    }
                }
            }

            RecyclableObject* obj = RecyclableObject::FromVar((DynamicObject*)pDisplay->GetItem(length - 1));
            OUTPUT_TRACE(Js::ConsoleScopePhase, _u("Adding property '%s' to console scope object\n"), scriptContext->GetPropertyName(propertyId)->GetBuffer());
            JavascriptOperators::SetProperty(obj, obj, propertyId, newValue, scriptContext, propertyOperationFlags);
            return;
        }

        // No one in the scope stack has the property, so add it to the default instance provided by the caller.
        AssertMsg(!TaggedNumber::Is(defaultInstance), "Root object is an int or tagged float?");
        Assert(defaultInstance != nullptr);
        RecyclableObject* obj = RecyclableObject::FromVar(defaultInstance);
        {TRACE_IT(50368);
            //SetPropertyScoped does not use inline cache for default instance
            PropertyValueInfo info2;
            JavascriptOperators::SetRootProperty(obj, propertyId, newValue, &info2, scriptContext, (PropertyOperationFlags)(propertyOperationFlags | PropertyOperation_Root));
        }
    }
    template void JavascriptOperators::PatchSetPropertyScoped<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var newValue, Var defaultInstance, PropertyOperationFlags propertyOperationFlags);
    template void JavascriptOperators::PatchSetPropertyScoped<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var newValue, Var defaultInstance, PropertyOperationFlags propertyOperationFlags);
    template void JavascriptOperators::PatchSetPropertyScoped<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var newValue, Var defaultInstance, PropertyOperationFlags propertyOperationFlags);
    template void JavascriptOperators::PatchSetPropertyScoped<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var newValue, Var defaultInstance, PropertyOperationFlags propertyOperationFlags);

    BOOL JavascriptOperators::OP_InitFuncScoped(FrameDisplay *pScope, PropertyId propertyId, Var newValue, Var defaultInstance, ScriptContext* scriptContext)
    {TRACE_IT(50369);
        int i;
        int length = pScope->GetLength();
        DynamicObject *obj;

        for (i = 0; i < length; i++)
        {TRACE_IT(50370);
            obj = (DynamicObject*)pScope->GetItem(i);

            if (obj->InitFuncScoped(propertyId, newValue))
            {TRACE_IT(50371);
                return TRUE;
            }
        }

        AssertMsg(!TaggedNumber::Is(defaultInstance), "Root object is an int or tagged float?");
        return RecyclableObject::FromVar(defaultInstance)->InitFuncScoped(propertyId, newValue);
    }

    BOOL JavascriptOperators::OP_InitPropertyScoped(FrameDisplay *pScope, PropertyId propertyId, Var newValue, Var defaultInstance, ScriptContext* scriptContext)
    {TRACE_IT(50372);
        int i;
        int length = pScope->GetLength();
        DynamicObject *obj;

        for (i = 0; i < length; i++)
        {TRACE_IT(50373);
            obj = (DynamicObject*)pScope->GetItem(i);
            if (obj->InitPropertyScoped(propertyId, newValue))
            {TRACE_IT(50374);
                return TRUE;
            }
        }

        AssertMsg(!TaggedNumber::Is(defaultInstance), "Root object is an int or tagged float?");
        return RecyclableObject::FromVar(defaultInstance)->InitPropertyScoped(propertyId, newValue);
    }

    Var JavascriptOperators::OP_DeletePropertyScoped(
        FrameDisplay *pScope,
        PropertyId propertyId,
        Var defaultInstance,
        ScriptContext* scriptContext,
        PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50375);
        int i;
        int length = pScope->GetLength();

        for (i = 0; i < length; i++)
        {TRACE_IT(50376);
            DynamicObject *obj = (DynamicObject*)pScope->GetItem(i);
            if (JavascriptOperators::HasProperty(obj, propertyId))
            {TRACE_IT(50377);
                return scriptContext->GetLibrary()->CreateBoolean(JavascriptOperators::DeleteProperty(obj, propertyId, propertyOperationFlags));
            }
        }

        return JavascriptOperators::OP_DeleteRootProperty(RecyclableObject::FromVar(defaultInstance), propertyId, scriptContext, propertyOperationFlags);
    }

    Var JavascriptOperators::OP_TypeofPropertyScoped(FrameDisplay *pScope, PropertyId propertyId, Var defaultInstance, ScriptContext* scriptContext)
    {TRACE_IT(50378);
        int i;
        int length = pScope->GetLength();

        for (i = 0; i < length; i++)
        {TRACE_IT(50379);
            DynamicObject *obj = (DynamicObject*)pScope->GetItem(i);
            if (JavascriptOperators::HasProperty(obj, propertyId))
            {TRACE_IT(50380);
                return JavascriptOperators::TypeofFld(obj, propertyId, scriptContext);
            }
        }

        return JavascriptOperators::TypeofRootFld(RecyclableObject::FromVar(defaultInstance), propertyId, scriptContext);
    }

    BOOL JavascriptOperators::HasOwnItem(RecyclableObject* object, uint32 index)
    {TRACE_IT(50381);
        return object->HasOwnItem(index);
    }

    BOOL JavascriptOperators::HasItem(RecyclableObject* object, uint64 index)
    {TRACE_IT(50382);
        PropertyRecord const * propertyRecord;
        ScriptContext* scriptContext = object->GetScriptContext();
        JavascriptOperators::GetPropertyIdForInt(index, scriptContext, &propertyRecord);
        return JavascriptOperators::HasProperty(object, propertyRecord->GetPropertyId());
    }

    BOOL JavascriptOperators::HasItem(RecyclableObject* object, uint32 index)
    {TRACE_IT(50383);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(object);
#endif
        while (JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50384);
            if (object->HasItem(index))
            {TRACE_IT(50385);
                return true;
            }
            // CONSIDER: Numeric property values shouldn't be on the prototype for now but if this changes
            // we should add SkipsPrototype support here as well
            object = JavascriptOperators::GetPrototypeNoTrap(object);
        }
        return false;
    }

    BOOL JavascriptOperators::GetOwnItem(RecyclableObject* object, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(50386);
        return object->GetItem(object, index, value, requestContext);
    }

    BOOL JavascriptOperators::GetItem(Var instance, RecyclableObject* propertyObject, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(50387);
        RecyclableObject* object = propertyObject;
        while (JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50388);
            if (object->GetItem(instance, index, value, requestContext))
            {TRACE_IT(50389);
                return true;
            }
            if (object->SkipsPrototype())
            {TRACE_IT(50390);
                break;
            }
            object = JavascriptOperators::GetPrototypeNoTrap(object);
        }
        *value = requestContext->GetMissingItemResult();
        return false;
    }

    BOOL JavascriptOperators::GetItemReference(Var instance, RecyclableObject* propertyObject, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(50391);
        RecyclableObject* object = propertyObject;
        while (JavascriptOperators::GetTypeId(object) != TypeIds_Null)
        {TRACE_IT(50392);
            if (object->GetItemReference(instance, index, value, requestContext))
            {TRACE_IT(50393);
                return true;
            }
            if (object->SkipsPrototype())
            {TRACE_IT(50394);
                break;
            }
            object = JavascriptOperators::GetPrototypeNoTrap(object);
        }
        *value = requestContext->GetMissingItemResult();
        return false;
    }

    BOOL JavascriptOperators::SetItem(Var receiver, RecyclableObject* object, uint64 index, Var value, ScriptContext* scriptContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50395);
        PropertyRecord const * propertyRecord;
        JavascriptOperators::GetPropertyIdForInt(index, scriptContext, &propertyRecord);
        return JavascriptOperators::SetProperty(receiver, object, propertyRecord->GetPropertyId(), value, scriptContext, propertyOperationFlags);
    }

    BOOL JavascriptOperators::SetItem(Var receiver, RecyclableObject* object, uint32 index, Var value, ScriptContext* scriptContext, PropertyOperationFlags propertyOperationFlags, BOOL skipPrototypeCheck /* = FALSE */)
    {TRACE_IT(50396);
        Var setterValueOrProxy = nullptr;
        DescriptorFlags flags = None;
        Assert(!TaggedNumber::Is(receiver));
        if (JavascriptOperators::CheckPrototypesForAccessorOrNonWritableItem(object, index, &setterValueOrProxy, &flags, scriptContext, skipPrototypeCheck))
        {TRACE_IT(50397);
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
            if ((flags & Accessor) == Accessor)
            {TRACE_IT(50398);
                if (JavascriptError::ThrowIfStrictModeUndefinedSetter(propertyOperationFlags, setterValueOrProxy, scriptContext) ||
                    JavascriptError::ThrowIfNotExtensibleUndefinedSetter(propertyOperationFlags, setterValueOrProxy, scriptContext))
                {TRACE_IT(50399);
                    return TRUE;
                }
                if (setterValueOrProxy)
                {TRACE_IT(50400);
                    RecyclableObject* func = RecyclableObject::FromVar(setterValueOrProxy);
                    JavascriptOperators::CallSetter(func, receiver, value, scriptContext);
                }
                return TRUE;
            }
            else if ((flags & Proxy) == Proxy)
            {TRACE_IT(50401);
                Assert(JavascriptProxy::Is(setterValueOrProxy));
                JavascriptProxy* proxy = JavascriptProxy::FromVar(setterValueOrProxy);
                const PropertyRecord* propertyRecord;
                proxy->PropertyIdFromInt(index, &propertyRecord);
                return proxy->SetPropertyTrap(receiver, JavascriptProxy::SetPropertyTrapKind::SetItemKind, propertyRecord->GetPropertyId(), value, scriptContext, skipPrototypeCheck);
            }
            else
            {TRACE_IT(50402);
                Assert((flags & Data) == Data && (flags & Writable) == None);
                if ((propertyOperationFlags & PropertyOperationFlags::PropertyOperation_ThrowIfNotExtensible) == PropertyOperationFlags::PropertyOperation_ThrowIfNotExtensible)
                {TRACE_IT(50403);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_NonExtensibleObject);
                }

                JavascriptError::ThrowCantAssign(propertyOperationFlags, scriptContext, index);
                JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, scriptContext);
                return FALSE;
            }
        }
        else if (!JavascriptOperators::IsObject(receiver))
        {TRACE_IT(50404);
            JavascriptError::ThrowCantAssignIfStrictMode(propertyOperationFlags, scriptContext);
            return FALSE;
        }

        return (RecyclableObject::FromVar(receiver))->SetItem(index, value, propertyOperationFlags);
    }

    BOOL JavascriptOperators::DeleteItem(RecyclableObject* object, uint32 index, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50405);
        return object->DeleteItem(index, propertyOperationFlags);
    }
    BOOL JavascriptOperators::DeleteItem(RecyclableObject* object, uint64 index, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50406);
        PropertyRecord const * propertyRecord;
        JavascriptOperators::GetPropertyIdForInt(index, object->GetScriptContext(), &propertyRecord);
        return JavascriptOperators::DeleteProperty(object, propertyRecord->GetPropertyId(), propertyOperationFlags);
    }

    BOOL JavascriptOperators::OP_HasItem(Var instance, Var index, ScriptContext* scriptContext)
    {TRACE_IT(50407);
        RecyclableObject* object = TaggedNumber::Is(instance) ?
            scriptContext->GetLibrary()->GetNumberPrototype() :
            RecyclableObject::FromVar(instance);

        uint32 indexVal;
        PropertyRecord const * propertyRecord;
        IndexType indexType = GetIndexType(index, scriptContext, &indexVal, &propertyRecord, false);

        if (indexType == IndexType_Number)
        {TRACE_IT(50408);
            return HasItem(object, indexVal);
        }
        else
        {TRACE_IT(50409);
            Assert(indexType == IndexType_PropertyId);
            if (propertyRecord == nullptr && !JavascriptOperators::CanShortcutOnUnknownPropertyName(object))
            {TRACE_IT(50410);
                indexType = GetIndexTypeFromPrimitive(index, scriptContext, &indexVal, &propertyRecord, true);
                Assert(indexType == IndexType_PropertyId);
                Assert(propertyRecord != nullptr);
            }

            if (propertyRecord != nullptr)
            {TRACE_IT(50411);
                return HasProperty(object, propertyRecord->GetPropertyId());
            }
            else
            {TRACE_IT(50412);
#if DBG
                JavascriptString* indexStr = JavascriptConversion::ToString(index, scriptContext);
                PropertyRecord const * debugPropertyRecord;
                scriptContext->GetOrAddPropertyRecord(indexStr->GetString(), indexStr->GetLength(), &debugPropertyRecord);
                AssertMsg(!JavascriptOperators::HasProperty(object, debugPropertyRecord->GetPropertyId()), "how did this property come? See OS Bug 2727708 if you see this come from the web");
#endif

                return FALSE;
            }
        }
    }

#if ENABLE_PROFILE_INFO
    void JavascriptOperators::UpdateNativeArrayProfileInfoToCreateVarArray(Var instance, const bool expectingNativeFloatArray, const bool expectingVarArray)
    {TRACE_IT(50413);
        Assert(instance);
        Assert(expectingNativeFloatArray ^ expectingVarArray);

        if (!JavascriptNativeArray::Is(instance))
        {TRACE_IT(50414);
            return;
        }

        ArrayCallSiteInfo *const arrayCallSiteInfo = JavascriptNativeArray::FromVar(instance)->GetArrayCallSiteInfo();
        if (!arrayCallSiteInfo)
        {TRACE_IT(50415);
            return;
        }

        if (expectingNativeFloatArray)
        {TRACE_IT(50416);
            // Profile data is expecting a native float array. Ensure that at the array's creation site, that a native int array
            // is not created, such that the profiled array type would be correct.
            arrayCallSiteInfo->SetIsNotNativeIntArray();
        }
        else
        {TRACE_IT(50417);
            // Profile data is expecting a var array. Ensure that at the array's creation site, that a native array is not
            // created, such that the profiled array type would be correct.
            Assert(expectingVarArray);
            arrayCallSiteInfo->SetIsNotNativeArray();
        }
    }

    bool JavascriptOperators::SetElementMayHaveImplicitCalls(ScriptContext *const scriptContext)
    {TRACE_IT(50418);
        return
            scriptContext->optimizationOverrides.GetArraySetElementFastPathVtable() ==
                ScriptContextOptimizationOverrideInfo::InvalidVtable;
    }
#endif

    RecyclableObject *JavascriptOperators::GetCallableObjectOrThrow(const Var callee, ScriptContext *const scriptContext)
    {TRACE_IT(50419);
        Assert(callee);
        Assert(scriptContext);

        if (TaggedNumber::Is(callee))
        {TRACE_IT(50420);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction /* TODO-ERROR: get arg name - aFunc */);
        }
        return RecyclableObject::FromVar(callee);
    }

#if ENABLE_NATIVE_CODEGEN
    Var JavascriptOperators::OP_GetElementI_JIT(Var instance, Var index, ScriptContext *scriptContext)
    {TRACE_IT(50421);
        Assert(Js::JavascriptStackWalker::ValidateTopJitFrame(scriptContext));

        return OP_GetElementI(instance, index, scriptContext);
    }
#else
    Var JavascriptOperators::OP_GetElementI_JIT(Var instance, Var index, ScriptContext *scriptContext)
    {TRACE_IT(50422);
        return OP_GetElementI(instance, index, scriptContext);
    }
#endif

#if ENABLE_NATIVE_CODEGEN
    Var JavascriptOperators::OP_GetElementI_JIT_ExpectingNativeFloatArray(Var instance, Var index, ScriptContext *scriptContext)
    {TRACE_IT(50423);
        Assert(Js::JavascriptStackWalker::ValidateTopJitFrame(scriptContext));

        UpdateNativeArrayProfileInfoToCreateVarArray(instance, true, false);
        return OP_GetElementI_JIT(instance, index, scriptContext);
    }

    Var JavascriptOperators::OP_GetElementI_JIT_ExpectingVarArray(Var instance, Var index, ScriptContext *scriptContext)
    {TRACE_IT(50424);
        Assert(Js::JavascriptStackWalker::ValidateTopJitFrame(scriptContext));


        UpdateNativeArrayProfileInfoToCreateVarArray(instance, false, true);
        return OP_GetElementI_JIT(instance, index, scriptContext);
    }
#endif

    Var JavascriptOperators::OP_GetElementI_UInt32(Var instance, uint32 index, ScriptContext* scriptContext)
    {TRACE_IT(50425);
#if FLOATVAR
        return OP_GetElementI_JIT(instance, Js::JavascriptNumber::ToVar(index, scriptContext), scriptContext);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_GetElementI_JIT(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext);
#endif
    }

    Var JavascriptOperators::OP_GetElementI_UInt32_ExpectingNativeFloatArray(Var instance, uint32 index, ScriptContext* scriptContext)
    {TRACE_IT(50426);
#if ENABLE_PROFILE_INFO
        UpdateNativeArrayProfileInfoToCreateVarArray(instance, true, false);
#endif
        return OP_GetElementI_UInt32(instance, index, scriptContext);
    }

    Var JavascriptOperators::OP_GetElementI_UInt32_ExpectingVarArray(Var instance, uint32 index, ScriptContext* scriptContext)
    {TRACE_IT(50427);
#if ENABLE_PROFILE_INFO
        UpdateNativeArrayProfileInfoToCreateVarArray(instance, false, true);
#endif
        return OP_GetElementI_UInt32(instance, index, scriptContext);
    }

    Var JavascriptOperators::OP_GetElementI_Int32(Var instance, int32 index, ScriptContext* scriptContext)
    {TRACE_IT(50428);
#if FLOATVAR
        return OP_GetElementI_JIT(instance, Js::JavascriptNumber::ToVar(index, scriptContext), scriptContext);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_GetElementI_JIT(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext);
#endif
    }

    Var JavascriptOperators::OP_GetElementI_Int32_ExpectingNativeFloatArray(Var instance, int32 index, ScriptContext* scriptContext)
    {TRACE_IT(50429);
#if ENABLE_PROFILE_INFO
        UpdateNativeArrayProfileInfoToCreateVarArray(instance, true, false);
#endif
        return OP_GetElementI_Int32(instance, index, scriptContext);
    }

    Var JavascriptOperators::OP_GetElementI_Int32_ExpectingVarArray(Var instance, int32 index, ScriptContext* scriptContext)
    {TRACE_IT(50430);
#if ENABLE_PROFILE_INFO
        UpdateNativeArrayProfileInfoToCreateVarArray(instance, false, true);
#endif
        return OP_GetElementI_Int32(instance, index, scriptContext);
    }

    BOOL JavascriptOperators::GetItemFromArrayPrototype(JavascriptArray * arr, int32 indexInt, Var * result, ScriptContext * scriptContext)
    {TRACE_IT(50431);
        // try get from Array prototype
        RecyclableObject* prototype = arr->GetPrototype();
        if (JavascriptOperators::GetTypeId(prototype) != TypeIds_Array) //This can be TypeIds_ES5Array (or any other object changed through __proto__).
        {TRACE_IT(50432);
            return false;
        }

        JavascriptArray* arrayPrototype = JavascriptArray::FromVar(prototype); //Prototype must be Array.prototype (unless changed through __proto__)
        if (arrayPrototype->GetLength() && arrayPrototype->GetItem(arrayPrototype, (uint32)indexInt, result, scriptContext))
        {TRACE_IT(50433);
            return true;
        }

        prototype = arrayPrototype->GetPrototype(); //Its prototype must be Object.prototype (unless changed through __proto__)
        if (prototype->GetScriptContext()->GetLibrary()->GetObjectPrototype() != prototype)
        {TRACE_IT(50434);
            return false;
        }

        if (DynamicObject::FromVar(prototype)->HasNonEmptyObjectArray())
        {TRACE_IT(50435);
            if (prototype->GetItem(arr, (uint32)indexInt, result, scriptContext))
            {TRACE_IT(50436);
                return true;
            }
        }

        *result = scriptContext->GetMissingItemResult();
        return true;
    }

    template <typename T>
    BOOL JavascriptOperators::OP_GetElementI_ArrayFastPath(T * arr, int indexInt, Var * result, ScriptContext * scriptContext)
    {TRACE_IT(50437);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arr);
#endif
        if (indexInt >= 0)
        {TRACE_IT(50438);
            if (!CrossSite::IsCrossSiteObjectTyped(arr))
            {TRACE_IT(50439);
                if (arr->T::DirectGetVarItemAt((uint32)indexInt, result, scriptContext))
                {TRACE_IT(50440);
                    return true;
                }
            }
            else
            {TRACE_IT(50441);
                if (arr->GetItem(arr, (uint32)indexInt, result, scriptContext))
                {TRACE_IT(50442);
                    return true;
                }
            }
            return GetItemFromArrayPrototype(arr, indexInt, result, scriptContext);
        }
        return false;
    }

    Var JavascriptOperators::OP_GetElementI(Var instance, Var index, ScriptContext* scriptContext)
    {TRACE_IT(50443);
        JavascriptString *temp = NULL;
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif

        if (TaggedInt::Is(index))
        {
        TaggedIntIndex:
            switch (JavascriptOperators::GetTypeId(instance))
            {
            case TypeIds_Array: //fast path for array
            {TRACE_IT(50445);
                Var result;
                if (OP_GetElementI_ArrayFastPath(JavascriptArray::FromVar(instance), TaggedInt::ToInt32(index), &result, scriptContext))
                {TRACE_IT(50446);
                    return result;
                }
                break;
            }
            case TypeIds_NativeIntArray:
            {TRACE_IT(50447);
                Var result;
                if (OP_GetElementI_ArrayFastPath(JavascriptNativeIntArray::FromVar(instance), TaggedInt::ToInt32(index), &result, scriptContext))
                {TRACE_IT(50448);
                    return result;
                }
                break;
            }
            case TypeIds_NativeFloatArray:
            {TRACE_IT(50449);
                Var result;
                if (OP_GetElementI_ArrayFastPath(JavascriptNativeFloatArray::FromVar(instance), TaggedInt::ToInt32(index), &result, scriptContext))
                {TRACE_IT(50450);
                    return result;
                }
                break;
            }

            case TypeIds_String: // fast path for string
            {TRACE_IT(50451);
                charcount_t indexInt = TaggedInt::ToUInt32(index);
                JavascriptString* string = JavascriptString::FromVar(instance);
                Var result;
                if (string->JavascriptString::GetItem(instance, indexInt, &result, scriptContext))
                {TRACE_IT(50452);
                    return result;
                }
                break;
            }

            case TypeIds_Int8Array:
            {TRACE_IT(50453);
                // The typed array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);
                if (VirtualTableInfo<Int8VirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50454);
                    Int8VirtualArray* int8Array = Int8VirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(int8Array) && indexInt >= 0)
                    {TRACE_IT(50455);
                        return int8Array->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50456);
                    Int8Array* int8Array = Int8Array::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(int8Array) && indexInt >= 0)
                    {TRACE_IT(50457);
                        return int8Array->DirectGetItem(indexInt);
                    }
                }
                break;
            }

            case TypeIds_Uint8Array:
            {TRACE_IT(50458);
                // The typed array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);
                if (VirtualTableInfo<Uint8VirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50459);
                    Uint8VirtualArray* uint8Array = Uint8VirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(uint8Array) && indexInt >= 0)
                    {TRACE_IT(50460);
                        return uint8Array->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50461);
                    Uint8Array* uint8Array = Uint8Array::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(uint8Array) && indexInt >= 0)
                    {TRACE_IT(50462);
                        return uint8Array->DirectGetItem(indexInt);
                    }
                }
                break;
            }

            case TypeIds_Uint8ClampedArray:
            {TRACE_IT(50463);
                // The typed array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);
                if (VirtualTableInfo<Uint8ClampedVirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50464);
                    Uint8ClampedVirtualArray* uint8ClampedArray = Uint8ClampedVirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(uint8ClampedArray) && indexInt >= 0)
                    {TRACE_IT(50465);
                        return uint8ClampedArray->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50466);
                    Uint8ClampedArray* uint8ClampedArray = Uint8ClampedArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(uint8ClampedArray) && indexInt >= 0)
                    {TRACE_IT(50467);
                        return uint8ClampedArray->DirectGetItem(indexInt);
                    }
                }
                break;
            }

            case TypeIds_Int16Array:
            {TRACE_IT(50468);
                // The type array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);

                if (VirtualTableInfo<Int16VirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50469);
                    Int16VirtualArray* int16Array = Int16VirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(int16Array) && indexInt >= 0)
                    {TRACE_IT(50470);
                        return int16Array->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50471);
                    Int16Array* int16Array = Int16Array::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(int16Array) && indexInt >= 0)
                    {TRACE_IT(50472);
                        return int16Array->DirectGetItem(indexInt);
                    }
                }
                break;
            }

            case TypeIds_Uint16Array:
            {TRACE_IT(50473);
                // The type array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);

                if (VirtualTableInfo<Uint16VirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50474);
                    Uint16VirtualArray* uint16Array = Uint16VirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(uint16Array) && indexInt >= 0)
                    {TRACE_IT(50475);
                        return uint16Array->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50476);
                    Uint16Array* uint16Array = Uint16Array::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(uint16Array) && indexInt >= 0)
                    {TRACE_IT(50477);
                        return uint16Array->DirectGetItem(indexInt);
                    }
                }
                break;
            }
            case TypeIds_Int32Array:
            {TRACE_IT(50478);
                // The type array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);
                if (VirtualTableInfo<Int32VirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50479);
                    Int32VirtualArray* int32Array = Int32VirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(int32Array) && indexInt >= 0)
                    {TRACE_IT(50480);
                        return int32Array->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50481);
                    Int32Array* int32Array = Int32Array::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(int32Array) && indexInt >= 0)
                    {TRACE_IT(50482);
                        return int32Array->DirectGetItem(indexInt);
                    }
                }
                break;

            }
            case TypeIds_Uint32Array:
            {TRACE_IT(50483);
                // The type array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);
                if (VirtualTableInfo<Uint32VirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50484);
                    Uint32VirtualArray* uint32Array = Uint32VirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(uint32Array) && indexInt >= 0)
                    {TRACE_IT(50485);
                        return uint32Array->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50486);
                    Uint32Array* uint32Array = Uint32Array::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(uint32Array) && indexInt >= 0)
                    {TRACE_IT(50487);
                        return uint32Array->DirectGetItem(indexInt);
                    }
                }
                break;
            }
            case TypeIds_Float32Array:
            {TRACE_IT(50488);
                // The type array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);

                if (VirtualTableInfo<Float32VirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50489);
                    Float32VirtualArray* float32Array = Float32VirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(float32Array) && indexInt >= 0)
                    {TRACE_IT(50490);
                        return float32Array->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50491);
                    Float32Array* float32Array = Float32Array::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(float32Array) && indexInt >= 0)
                    {TRACE_IT(50492);
                        return float32Array->DirectGetItem(indexInt);
                    }
                }
                break;
            }
            case TypeIds_Float64Array:
            {TRACE_IT(50493);
                // The type array will deal with all possible values for the index
                int32 indexInt = TaggedInt::ToInt32(index);
                if (VirtualTableInfo<Float64VirtualArray>::HasVirtualTable(instance))
                {TRACE_IT(50494);
                    Float64VirtualArray* float64Array = Float64VirtualArray::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(float64Array) && indexInt >= 0)
                    {TRACE_IT(50495);
                        return float64Array->DirectGetItem(indexInt);
                    }
                }
                else
                {TRACE_IT(50496);
                    Float64Array* float64Array = Float64Array::FromVar(instance);
                    if (!CrossSite::IsCrossSiteObjectTyped(float64Array) && indexInt >= 0)
                    {TRACE_IT(50497);
                        return float64Array->DirectGetItem(indexInt);
                    }
                }
                break;
            }

            default:
                break;
            }
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(index))
        {TRACE_IT(50498);
            uint32 uint32Index = JavascriptConversion::ToUInt32(index, scriptContext);

            if ((double)uint32Index == JavascriptNumber::GetValue(index) && !TaggedInt::IsOverflow(uint32Index))
            {TRACE_IT(50499);
                index = TaggedInt::ToVarUnchecked(uint32Index);
                goto TaggedIntIndex;
            }
        }
        else if (JavascriptString::Is(index)) // fastpath for PropertyStrings
        {TRACE_IT(50500);
            temp = JavascriptString::FromVar(index);
            Assert(temp->GetScriptContext() == scriptContext);

            if (VirtualTableInfo<Js::PropertyString>::HasVirtualTable(temp))
            {TRACE_IT(50501);
                PropertyString * propertyString = (PropertyString*)temp;
                PropertyCache const *cache = propertyString->GetPropertyCache();
                RecyclableObject* object = nullptr;
                if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
                {TRACE_IT(50502);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined,
                        JavascriptString::FromVar(index)->GetSz());
                }
                if (object->GetType() == cache->type)
                {TRACE_IT(50503);
#if DBG_DUMP
                    scriptContext->forinCache++;
#endif
                    Assert(object->GetScriptContext() == scriptContext);
                    Var value;
                    if (cache->isInlineSlot)
                    {TRACE_IT(50504);
                        value = DynamicObject::FromVar(object)->GetInlineSlot(cache->dataSlotIndex);
                    }
                    else
                    {TRACE_IT(50505);
                        value = DynamicObject::FromVar(object)->GetAuxSlot(cache->dataSlotIndex);
                    }
                    Assert(!CrossSite::NeedMarshalVar(value, scriptContext));
                    Assert(value == JavascriptOperators::GetProperty(object, propertyString->GetPropertyRecord()->GetPropertyId(), scriptContext)
                        || value == JavascriptOperators::GetRootProperty(object, propertyString->GetPropertyRecord()->GetPropertyId(), scriptContext));
                    return value;
                }
#if DBG_DUMP
                scriptContext->forinNoCache++;
#endif
                PropertyRecord const * propertyRecord = propertyString->GetPropertyRecord();
                Var value;
                if (propertyRecord->IsNumeric())
                {TRACE_IT(50506);
                    if (JavascriptOperators::GetItem(instance, object, propertyRecord->GetNumericValue(), &value, scriptContext))
                    {TRACE_IT(50507);
                        return value;
                    }
                }
                else
                {TRACE_IT(50508);
                    if (JavascriptOperators::GetPropertyWPCache(instance, object, propertyRecord->GetPropertyId(), &value, scriptContext, propertyString))
                    {TRACE_IT(50509);
                        return value;
                    }
                }
                return scriptContext->GetLibrary()->GetUndefined();
            }
#if DBG_DUMP
            scriptContext->forinNoCache++;
#endif
        }

        return JavascriptOperators::GetElementIHelper(instance, index, instance, scriptContext);
    }

    Var JavascriptOperators::GetElementIHelper(Var instance, Var index, Var receiver, ScriptContext* scriptContext)
    {TRACE_IT(50510);
        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(50511);
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(50512);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined, GetPropertyDisplayNameForError(index, scriptContext));
            }
            else
            {TRACE_IT(50513);
                return scriptContext->GetLibrary()->GetUndefined();
            }
        }

        uint32 indexVal;
        PropertyRecord const * propertyRecord;
        JavascriptString * propertyNameString;
        Var value;

        IndexType indexType = GetIndexType(index, scriptContext, &indexVal, &propertyRecord, &propertyNameString, false, true);

        if (indexType == IndexType_Number)
        {TRACE_IT(50514);
            if (JavascriptOperators::GetItem(receiver, object, indexVal, &value, scriptContext))
            {TRACE_IT(50515);
                return value;
            }
        }
        else if (indexType == IndexType_JavascriptString)
        {TRACE_IT(50516);
            if (JavascriptOperators::GetPropertyWPCache(receiver, object, propertyNameString, &value, scriptContext, nullptr))
            {TRACE_IT(50517);
                return value;
            }
        }
        else
        {TRACE_IT(50518);
            Assert(indexType == IndexType_PropertyId);
            if (propertyRecord == nullptr && !JavascriptOperators::CanShortcutOnUnknownPropertyName(object))
            {TRACE_IT(50519);
                indexType = GetIndexTypeFromPrimitive(index, scriptContext, &indexVal, &propertyRecord, &propertyNameString, true, true);
                Assert(indexType == IndexType_PropertyId);
                Assert(propertyRecord != nullptr);
            }

            if (propertyRecord != nullptr)
            {TRACE_IT(50520);
                if (JavascriptOperators::GetPropertyWPCache(receiver, object, propertyRecord->GetPropertyId(), &value, scriptContext, nullptr))
                {TRACE_IT(50521);
                    return value;
                }
            }
#if DBG
            else
            {TRACE_IT(50522);
                JavascriptString* indexStr = JavascriptConversion::ToString(index, scriptContext);
                PropertyRecord const * debugPropertyRecord;
                scriptContext->GetOrAddPropertyRecord(indexStr->GetString(), indexStr->GetLength(), &debugPropertyRecord);
                AssertMsg(!JavascriptOperators::GetProperty(receiver, object, debugPropertyRecord->GetPropertyId(), &value, scriptContext), "how did this property come? See OS Bug 2727708 if you see this come from the web");
            }
#endif
        }

        return scriptContext->GetMissingItemResult();
    }

    int32 JavascriptOperators::OP_GetNativeIntElementI(Var instance, Var index)
    {TRACE_IT(50523);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        if (TaggedInt::Is(index))
        {TRACE_IT(50524);
            int32 indexInt = TaggedInt::ToInt32(index);
            if (indexInt < 0)
            {TRACE_IT(50525);
                return JavascriptNativeIntArray::MissingItem;
            }
            JavascriptArray * arr = JavascriptArray::FromVar(instance);
            int32 result;
            if (arr->DirectGetItemAt((uint32)indexInt, &result))
            {TRACE_IT(50526);
                return result;
            }
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(index))
        {TRACE_IT(50527);
            int32 indexInt;
            bool isInt32;
            double dIndex = JavascriptNumber::GetValue(index);
            if (JavascriptNumber::TryGetInt32OrUInt32Value(dIndex, &indexInt, &isInt32))
            {TRACE_IT(50528);
                if (isInt32 && indexInt < 0)
                {TRACE_IT(50529);
                    return JavascriptNativeIntArray::MissingItem;
                }
                JavascriptArray * arr = JavascriptArray::FromVar(instance);
                int32 result;
                if (arr->DirectGetItemAt((uint32)indexInt, &result))
                {TRACE_IT(50530);
                    return result;
                }
            }
        }
        else
        {
            AssertMsg(false, "Non-numerical index in this helper?");
        }

        return JavascriptNativeIntArray::MissingItem;
    }

    int32 JavascriptOperators::OP_GetNativeIntElementI_UInt32(Var instance, uint32 index, ScriptContext* scriptContext)
    {TRACE_IT(50531);
#if FLOATVAR
        return OP_GetNativeIntElementI(instance, Js::JavascriptNumber::ToVar(index, scriptContext));
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_GetNativeIntElementI(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer));
#endif
    }

    int32 JavascriptOperators::OP_GetNativeIntElementI_Int32(Var instance, int32 index, ScriptContext* scriptContext)
    {TRACE_IT(50532);
#if FLOATVAR
        return OP_GetNativeIntElementI(instance, Js::JavascriptNumber::ToVar(index, scriptContext));
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_GetNativeIntElementI(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer));
#endif
    }

    double JavascriptOperators::OP_GetNativeFloatElementI(Var instance, Var index)
    {TRACE_IT(50533);
        double result = 0;

        if (TaggedInt::Is(index))
        {TRACE_IT(50534);
            int32 indexInt = TaggedInt::ToInt32(index);
            if (indexInt < 0)
            {TRACE_IT(50535);
                result = JavascriptNativeFloatArray::MissingItem;
            }
            else
            {TRACE_IT(50536);
                JavascriptArray * arr = JavascriptArray::FromVar(instance);
                if (!arr->DirectGetItemAt((uint32)indexInt, &result))
                {TRACE_IT(50537);
                    result = JavascriptNativeFloatArray::MissingItem;
                }
            }
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(index))
        {TRACE_IT(50538);
            int32 indexInt;
            bool isInt32;
            double dIndex = JavascriptNumber::GetValue(index);
            if (JavascriptNumber::TryGetInt32OrUInt32Value(dIndex, &indexInt, &isInt32))
            {TRACE_IT(50539);
                if (isInt32 && indexInt < 0)
                {TRACE_IT(50540);
                    result = JavascriptNativeFloatArray::MissingItem;
                }
                else
                {TRACE_IT(50541);
                    JavascriptArray * arr = JavascriptArray::FromVar(instance);
                    if (!arr->DirectGetItemAt((uint32)indexInt, &result))
                    {TRACE_IT(50542);
                        result = JavascriptNativeFloatArray::MissingItem;
                    }
                }
            }
        }
        else
        {
            AssertMsg(false, "Non-numerical index in this helper?");
        }

        return result;
    }

    double JavascriptOperators::OP_GetNativeFloatElementI_UInt32(Var instance, uint32 index, ScriptContext* scriptContext)
    {TRACE_IT(50543);
#if FLOATVAR
        return OP_GetNativeFloatElementI(instance, Js::JavascriptNumber::ToVar(index, scriptContext));
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_GetNativeFloatElementI(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer));
#endif
    }

    double JavascriptOperators::OP_GetNativeFloatElementI_Int32(Var instance, int32 index, ScriptContext* scriptContext)
    {TRACE_IT(50544);
#if FLOATVAR
        return OP_GetNativeFloatElementI(instance, Js::JavascriptNumber::ToVar(index, scriptContext));
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_GetNativeFloatElementI(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer));
#endif
    }

    Var JavascriptOperators::OP_GetMethodElement_UInt32(Var instance, uint32 index, ScriptContext* scriptContext)
    {TRACE_IT(50545);
#if FLOATVAR
        return OP_GetMethodElement(instance, Js::JavascriptNumber::ToVar(index, scriptContext), scriptContext);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_GetMethodElement(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext);
#endif
    }

    Var JavascriptOperators::OP_GetMethodElement_Int32(Var instance, int32 index, ScriptContext* scriptContext)
    {TRACE_IT(50546);
#if FLOATVAR
        return OP_GetElementI(instance, Js::JavascriptNumber::ToVar(index, scriptContext), scriptContext);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_GetMethodElement(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext);
#endif
    }

    Var JavascriptOperators::OP_GetMethodElement(Var instance, Var index, ScriptContext* scriptContext)
    {TRACE_IT(50547);
        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(50548);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined, GetPropertyDisplayNameForError(index, scriptContext));
        }

        ThreadContext* threadContext = scriptContext->GetThreadContext();
        ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
        threadContext->ClearImplicitCallFlags();

        uint32 indexVal;
        PropertyRecord const * propertyRecord;
        Var value = NULL;
        BOOL hasProperty = FALSE;
        IndexType indexType = GetIndexType(index, scriptContext, &indexVal, &propertyRecord, false);

        if (indexType == IndexType_Number)
        {TRACE_IT(50549);
            hasProperty = JavascriptOperators::GetItemReference(instance, object, indexVal, &value, scriptContext);
        }
        else
        {TRACE_IT(50550);
            Assert(indexType == IndexType_PropertyId);

            if (propertyRecord == nullptr && !JavascriptOperators::CanShortcutOnUnknownPropertyName(object))
            {TRACE_IT(50551);
                indexType = GetIndexTypeFromPrimitive(index, scriptContext, &indexVal, &propertyRecord, true);
                Assert(indexType == IndexType_PropertyId);
                Assert(propertyRecord != nullptr);
            }

            if (propertyRecord != nullptr)
            {TRACE_IT(50552);
                hasProperty = JavascriptOperators::GetPropertyReference(instance, object, propertyRecord->GetPropertyId(), &value, scriptContext, NULL);
            }
#if DBG
            else
            {TRACE_IT(50553);
                JavascriptString* indexStr = JavascriptConversion::ToString(index, scriptContext);
                PropertyRecord const * debugPropertyRecord;
                scriptContext->GetOrAddPropertyRecord(indexStr->GetString(), indexStr->GetLength(), &debugPropertyRecord);
                AssertMsg(!JavascriptOperators::GetPropertyReference(instance, object, debugPropertyRecord->GetPropertyId(), &value, scriptContext, NULL),
                          "how did this property come? See OS Bug 2727708 if you see this come from the web");
            }
#endif
        }

        if (!hasProperty)
        {TRACE_IT(50554);
            JavascriptString* varName = nullptr;
            if (indexType == IndexType_PropertyId && propertyRecord != nullptr && propertyRecord->IsSymbol())
            {TRACE_IT(50555);
                varName = JavascriptSymbol::ToString(propertyRecord, scriptContext);
            }
            else
            {TRACE_IT(50556);
                varName = JavascriptConversion::ToString(index, scriptContext);
            }

            // ES5 11.2.3 #2: We evaluate the call target but don't throw yet if target member is missing. We need to evaluate argList
            // first (#3). Postpone throwing error to invoke time.
            value = ThrowErrorObject::CreateThrowTypeErrorObject(scriptContext, VBSERR_OLENoPropOrMethod, varName);
        }
        else if(!JavascriptConversion::IsCallable(value))
        {TRACE_IT(50557);
            // ES5 11.2.3 #2: We evaluate the call target but don't throw yet if target member is missing. We need to evaluate argList
            // first (#3). Postpone throwing error to invoke time.
            JavascriptString* varName = JavascriptConversion::ToString(index, scriptContext);
            value = ThrowErrorObject::CreateThrowTypeErrorObject(scriptContext, JSERR_Property_NeedFunction, varName);
        }

        threadContext->CheckAndResetImplicitCallAccessorFlag();
        threadContext->AddImplicitCallFlags(savedImplicitCallFlags);
        return value;
    }

    BOOL JavascriptOperators::OP_SetElementI_UInt32(Var instance, uint32 index, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50558);
#if FLOATVAR
        return OP_SetElementI_JIT(instance, Js::JavascriptNumber::ToVar(index, scriptContext), value, scriptContext, flags);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_SetElementI_JIT(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), value, scriptContext, flags);
#endif
    }

    BOOL JavascriptOperators::OP_SetElementI_Int32(Var instance, int32 index, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50559);
#if FLOATVAR
        return OP_SetElementI_JIT(instance, Js::JavascriptNumber::ToVar(index, scriptContext), value, scriptContext, flags);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_SetElementI_JIT(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), value, scriptContext, flags);
#endif
    }

    BOOL JavascriptOperators::OP_SetElementI_JIT(Var instance, Var index, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50560);
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50561);
            return OP_SetElementI(instance, index, value, scriptContext, flags);
        }

        INT_PTR vt = VirtualTableInfoBase::GetVirtualTable(instance);
        OP_SetElementI(instance, index, value, scriptContext, flags);
        return vt != VirtualTableInfoBase::GetVirtualTable(instance);
    }

    BOOL JavascriptOperators::OP_SetElementI(Var instance, Var index, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50562);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif

        TypeId instanceType = JavascriptOperators::GetTypeId(instance);

        bool isTypedArray = (instanceType >= TypeIds_Int8Array && instanceType <= TypeIds_Float64Array);

        if (isTypedArray)
        {TRACE_IT(50563);
            if (TaggedInt::Is(index) || JavascriptNumber::Is_NoTaggedIntCheck(index) || JavascriptString::Is(index))
            {TRACE_IT(50564);
                BOOL returnValue = FALSE;
                bool isNumericIndex = false;
                switch (instanceType)
                {
                case TypeIds_Int8Array:
                {TRACE_IT(50565);
                    // The typed array will deal with all possible values for the index

                    if (VirtualTableInfo<Int8VirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50566);
                        Int8VirtualArray* int8Array = Int8VirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(int8Array))
                        {TRACE_IT(50567);
                            returnValue = int8Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50568);
                        Int8Array* int8Array = Int8Array::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(int8Array))
                        {TRACE_IT(50569);
                            returnValue = int8Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }

                case TypeIds_Uint8Array:
                {TRACE_IT(50570);
                    // The typed array will deal with all possible values for the index
                    if (VirtualTableInfo<Uint8VirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50571);
                        Uint8VirtualArray* uint8Array = Uint8VirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(uint8Array))
                        {TRACE_IT(50572);
                            returnValue = uint8Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50573);
                        Uint8Array* uint8Array = Uint8Array::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(uint8Array))
                        {TRACE_IT(50574);
                            returnValue = uint8Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }

                case TypeIds_Uint8ClampedArray:
                {TRACE_IT(50575);
                    // The typed array will deal with all possible values for the index
                    if (VirtualTableInfo<Uint8ClampedVirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50576);
                        Uint8ClampedVirtualArray* uint8ClampedArray = Uint8ClampedVirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(uint8ClampedArray))
                        {TRACE_IT(50577);
                            returnValue = uint8ClampedArray->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50578);
                        Uint8ClampedArray* uint8ClampedArray = Uint8ClampedArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(uint8ClampedArray))
                        {TRACE_IT(50579);
                            returnValue = uint8ClampedArray->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }

                case TypeIds_Int16Array:
                {TRACE_IT(50580);
                    // The type array will deal with all possible values for the index
                    if (VirtualTableInfo<Int16VirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50581);
                        Int16VirtualArray* int16Array = Int16VirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(int16Array))
                        {TRACE_IT(50582);
                            returnValue = int16Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50583);
                        Int16Array* int16Array = Int16Array::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(int16Array))
                        {TRACE_IT(50584);
                            returnValue = int16Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }

                case TypeIds_Uint16Array:
                {TRACE_IT(50585);
                    // The type array will deal with all possible values for the index

                    if (VirtualTableInfo<Uint16VirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50586);
                        Uint16VirtualArray* uint16Array = Uint16VirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(uint16Array))
                        {TRACE_IT(50587);
                            returnValue = uint16Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50588);
                        Uint16Array* uint16Array = Uint16Array::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(uint16Array))
                        {TRACE_IT(50589);
                            returnValue = uint16Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }
                case TypeIds_Int32Array:
                {TRACE_IT(50590);
                    // The type array will deal with all possible values for the index
                    if (VirtualTableInfo<Int32VirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50591);
                        Int32VirtualArray* int32Array = Int32VirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(int32Array))
                        {TRACE_IT(50592);
                            returnValue = int32Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50593);
                        Int32Array* int32Array = Int32Array::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(int32Array))
                        {TRACE_IT(50594);
                            returnValue = int32Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }
                case TypeIds_Uint32Array:
                {TRACE_IT(50595);
                    // The type array will deal with all possible values for the index

                    if (VirtualTableInfo<Uint32VirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50596);
                        Uint32VirtualArray* uint32Array = Uint32VirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(uint32Array))
                        {TRACE_IT(50597);
                            returnValue = uint32Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50598);
                        Uint32Array* uint32Array = Uint32Array::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(uint32Array))
                        {TRACE_IT(50599);
                            returnValue = uint32Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }
                case TypeIds_Float32Array:
                {TRACE_IT(50600);
                    // The type array will deal with all possible values for the index
                    if (VirtualTableInfo<Float32VirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50601);
                        Float32VirtualArray* float32Array = Float32VirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(float32Array))
                        {TRACE_IT(50602);
                            returnValue = float32Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50603);
                        Float32Array* float32Array = Float32Array::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(float32Array))
                        {TRACE_IT(50604);
                            returnValue = float32Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }
                case TypeIds_Float64Array:
                {TRACE_IT(50605);
                    // The type array will deal with all possible values for the index

                    if (VirtualTableInfo<Float64VirtualArray>::HasVirtualTable(instance))
                    {TRACE_IT(50606);
                        Float64VirtualArray* float64Array = Float64VirtualArray::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(float64Array))
                        {TRACE_IT(50607);
                            returnValue = float64Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    else
                    {TRACE_IT(50608);
                        Float64Array* float64Array = Float64Array::FromVar(instance);
                        if (!CrossSite::IsCrossSiteObjectTyped(float64Array))
                        {TRACE_IT(50609);
                            returnValue = float64Array->ValidateIndexAndDirectSetItem(index, value, &isNumericIndex);
                        }
                    }
                    break;
                }
                }

                // if this was numeric index, return operation status else
                // Return the result of calling the default ordinary object [[Set]] internal method (9.1.8) on O passing P, V, and Receiver as arguments.
                if (isNumericIndex)
                    return returnValue;
            }
        }
        else
        {TRACE_IT(50610);
            if (TaggedInt::Is(index))
            {
            TaggedIntIndex:
                switch (instanceType)
                {
                case TypeIds_NativeIntArray:
                case TypeIds_NativeFloatArray:
                case TypeIds_Array: // fast path for array
                {TRACE_IT(50612);
                    int indexInt = TaggedInt::ToInt32(index);
                    if (indexInt >= 0 && scriptContext->optimizationOverrides.IsEnabledArraySetElementFastPath())
                    {TRACE_IT(50613);
                        JavascriptArray::FromVar(instance)->SetItem((uint32)indexInt, value, flags);
                        return true;
                    }
                    break;
                }
                }
            }
            else if (JavascriptNumber::Is_NoTaggedIntCheck(index))
            {TRACE_IT(50614);
                double dIndexValue = JavascriptNumber::GetValue(index);
                uint32 uint32Index = JavascriptConversion::ToUInt32(index, scriptContext);

                if ((double)uint32Index == dIndexValue && !TaggedInt::IsOverflow(uint32Index))
                {TRACE_IT(50615);
                    index = TaggedInt::ToVarUnchecked(uint32Index);
                    goto TaggedIntIndex;
                }
            }
        }

        RecyclableObject* object;
        BOOL isNullOrUndefined = !GetPropertyObject(instance, scriptContext, &object);

        Assert(object == instance || TaggedNumber::Is(instance));

        if (isNullOrUndefined)
        {TRACE_IT(50616);
            if (!scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(50617);
                return FALSE;
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotSet_NullOrUndefined, GetPropertyDisplayNameForError(index, scriptContext));
        }

        return JavascriptOperators::SetElementIHelper(instance, object, index, value, scriptContext, flags);
    }

    BOOL JavascriptOperators::SetElementIHelper(Var receiver, RecyclableObject* object, Var index, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50618);
        PropertyString * propertyString = nullptr;
        Js::IndexType indexType;
        uint32 indexVal = 0;
        PropertyRecord const * propertyRecord = nullptr;
        JavascriptString * propertyNameString = nullptr;

        if (TaggedNumber::Is(receiver))
        {TRACE_IT(50619);
            indexType = GetIndexType(index, scriptContext, &indexVal, &propertyRecord, true);
            if (indexType == IndexType_Number)
            {TRACE_IT(50620);
                return  JavascriptOperators::SetItemOnTaggedNumber(receiver, object, indexVal, value, scriptContext, flags);
            }
            else
            {TRACE_IT(50621);
                return  JavascriptOperators::SetPropertyOnTaggedNumber(receiver, object, propertyRecord->GetPropertyId(), value, scriptContext, flags);
            }
        }

        // fastpath for PropertyStrings only if receiver == object
        if (!TaggedInt::Is(index) && JavascriptString::Is(index) &&
            VirtualTableInfo<Js::PropertyString>::HasVirtualTable(JavascriptString::FromVar(index)))
        {TRACE_IT(50622);
            propertyString = (PropertyString *)JavascriptString::FromVar(index);

            Assert(propertyString->GetScriptContext() == scriptContext);

            PropertyCache const * cache = propertyString->GetPropertyCache();
            if (receiver == object && object->GetType() == cache->type && cache->isStoreFieldEnabled)
            {TRACE_IT(50623);
#if DBG
                propertyRecord = propertyString->GetPropertyRecord();
#endif
#if DBG_DUMP
                scriptContext->forinCache++;
#endif
                Assert(object->GetScriptContext() == scriptContext);
                Assert(!CrossSite::NeedMarshalVar(value, scriptContext));
                if (cache->isInlineSlot)
                {TRACE_IT(50624);
                    DynamicObject::FromVar(object)->SetInlineSlot(SetSlotArguments(propertyRecord->GetPropertyId(), cache->dataSlotIndex, value));
                }
                else
                {TRACE_IT(50625);
                    DynamicObject::FromVar(object)->SetAuxSlot(SetSlotArguments(propertyRecord->GetPropertyId(), cache->dataSlotIndex, value));
                }
                return true;
            }

            propertyRecord = propertyString->GetPropertyRecord();
            if (propertyRecord->IsNumeric())
            {TRACE_IT(50626);
                indexType = IndexType_Number;
                indexVal = propertyRecord->GetNumericValue();
            }
            else
            {TRACE_IT(50627);
                indexType = IndexType_PropertyId;
            }

#if DBG_DUMP
            scriptContext->forinNoCache++;
#endif
        }
        else
        {TRACE_IT(50628);
#if DBG_DUMP
            scriptContext->forinNoCache += (!TaggedInt::Is(index) && JavascriptString::Is(index));
#endif
            indexType = GetIndexType(index, scriptContext, &indexVal, &propertyRecord, &propertyNameString, false, true);
            if (scriptContext->GetThreadContext()->IsDisableImplicitCall() &&
                scriptContext->GetThreadContext()->GetImplicitCallFlags() != ImplicitCall_None)
            {TRACE_IT(50629);
                // We hit an implicit call trying to convert the index, and implicit calls are disabled, so
                // quit before we try to store the element.
                return FALSE;
            }
        }

        if (indexType == IndexType_Number)
        {TRACE_IT(50630);
            return JavascriptOperators::SetItem(receiver, object, indexVal, value, scriptContext, flags);
        }
        else if (indexType == IndexType_JavascriptString)
        {TRACE_IT(50631);
            Assert(propertyNameString);
            JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());

            if (BuiltInPropertyRecords::NaN.Equals(propertyName))
            {TRACE_IT(50632);
                // Follow SetProperty convention for NaN
                return JavascriptOperators::SetProperty(receiver, object, PropertyIds::NaN, value, scriptContext, flags);
            }
            else if (BuiltInPropertyRecords::Infinity.Equals(propertyName))
            {TRACE_IT(50633);
                // Follow SetProperty convention for Infinity
                return JavascriptOperators::SetProperty(receiver, object, PropertyIds::Infinity, value, scriptContext, flags);
            }

            return JavascriptOperators::SetPropertyWPCache(receiver, object, propertyNameString, value, scriptContext, nullptr, flags);
        }
        else if (indexType == IndexType_PropertyId)
        {TRACE_IT(50634);
            Assert(propertyRecord);
            PropertyId propId = propertyRecord->GetPropertyId();
            if (propId == PropertyIds::NaN || propId == PropertyIds::Infinity)
            {TRACE_IT(50635);
                // As we no longer convert o[x] into o.x for NaN and Infinity, we need to follow SetProperty convention for these,
                // which would check for read-only properties, strict mode, etc.
                // Note that "-Infinity" does not qualify as property name, so we don't have to take care of it.
                return JavascriptOperators::SetProperty(receiver, object, propId, value, scriptContext, flags);
            }
        }

        return JavascriptOperators::SetPropertyWPCache(receiver, object, propertyRecord->GetPropertyId(), value, scriptContext, propertyString, flags);
    }

    BOOL JavascriptOperators::OP_SetNativeIntElementI(
        Var instance,
        Var aElementIndex,
        int32 iValue,
        ScriptContext* scriptContext,
        PropertyOperationFlags flags)
    {TRACE_IT(50636);
        if (TaggedInt::Is(aElementIndex))
        {TRACE_IT(50637);
            int32 indexInt = TaggedInt::ToInt32(aElementIndex);
            if (indexInt >= 0 && scriptContext->optimizationOverrides.IsEnabledArraySetElementFastPath())
            {TRACE_IT(50638);
                JavascriptNativeIntArray *arr = JavascriptNativeIntArray::FromVar(instance);
                if (!(arr->TryGrowHeadSegmentAndSetItem<int32, JavascriptNativeIntArray>((uint32)indexInt, iValue)))
                {TRACE_IT(50639);
                    arr->SetItem(indexInt, iValue);
                }
                return TRUE;
            }
        }

        return JavascriptOperators::OP_SetElementI(instance, aElementIndex, JavascriptNumber::ToVar(iValue, scriptContext), scriptContext, flags);
    }

    BOOL JavascriptOperators::OP_SetNativeIntElementI_UInt32(
        Var instance,
        uint32 aElementIndex,
        int32 iValue,
        ScriptContext* scriptContext,
        PropertyOperationFlags flags)
    {TRACE_IT(50640);
#if FLOATVAR
        return OP_SetNativeIntElementI(instance, Js::JavascriptNumber::ToVar(aElementIndex, scriptContext), iValue, scriptContext, flags);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_SetNativeIntElementI(instance, Js::JavascriptNumber::ToVarInPlace(aElementIndex, scriptContext,
            (Js::JavascriptNumber *)buffer), iValue, scriptContext, flags);
#endif
    }

    BOOL JavascriptOperators::OP_SetNativeIntElementI_Int32(
        Var instance,
        int aElementIndex,
        int32 iValue,
        ScriptContext* scriptContext,
        PropertyOperationFlags flags)
    {TRACE_IT(50641);
#if FLOATVAR
        return OP_SetNativeIntElementI(instance, Js::JavascriptNumber::ToVar(aElementIndex, scriptContext), iValue, scriptContext, flags);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_SetNativeIntElementI(instance, Js::JavascriptNumber::ToVarInPlace(aElementIndex, scriptContext,
            (Js::JavascriptNumber *)buffer), iValue, scriptContext, flags);
#endif
    }

    BOOL JavascriptOperators::OP_SetNativeFloatElementI(
        Var instance,
        Var aElementIndex,
        ScriptContext* scriptContext,
        PropertyOperationFlags flags,
        double dValue)
    {TRACE_IT(50642);
        if (TaggedInt::Is(aElementIndex))
        {TRACE_IT(50643);
            int32 indexInt = TaggedInt::ToInt32(aElementIndex);
            if (indexInt >= 0 && scriptContext->optimizationOverrides.IsEnabledArraySetElementFastPath())
            {TRACE_IT(50644);
                JavascriptNativeFloatArray *arr = JavascriptNativeFloatArray::FromVar(instance);
                if (!(arr->TryGrowHeadSegmentAndSetItem<double, JavascriptNativeFloatArray>((uint32)indexInt, dValue)))
                {TRACE_IT(50645);
                    arr->SetItem(indexInt, dValue);
                }
                return TRUE;
            }
        }

        return JavascriptOperators::OP_SetElementI(instance, aElementIndex, JavascriptNumber::ToVarWithCheck(dValue, scriptContext), scriptContext, flags);
    }

    BOOL JavascriptOperators::OP_SetNativeFloatElementI_UInt32(
        Var instance, uint32
        aElementIndex,
        ScriptContext* scriptContext,
        PropertyOperationFlags flags,
        double dValue)
    {TRACE_IT(50646);
#if FLOATVAR
        return OP_SetNativeFloatElementI(instance, JavascriptNumber::ToVar(aElementIndex, scriptContext), scriptContext, flags, dValue);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_SetNativeFloatElementI(instance, JavascriptNumber::ToVarInPlace(aElementIndex, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext, flags, dValue);
#endif
    }

    BOOL JavascriptOperators::OP_SetNativeFloatElementI_Int32(
        Var instance,
        int aElementIndex,
        ScriptContext* scriptContext,
        PropertyOperationFlags flags,
        double dValue)
    {TRACE_IT(50647);
#if FLOATVAR
        return OP_SetNativeFloatElementI(instance, JavascriptNumber::ToVar(aElementIndex, scriptContext), scriptContext, flags, dValue);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_SetNativeFloatElementI(instance, JavascriptNumber::ToVarInPlace(aElementIndex, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext, flags, dValue);
#endif
    }
    BOOL JavascriptOperators::OP_Memcopy(Var dstInstance, int32 dstStart, Var srcInstance, int32 srcStart, int32 length, ScriptContext* scriptContext)
    {TRACE_IT(50648);
        if (length <= 0)
        {TRACE_IT(50649);
            return false;
        }

        TypeId instanceType = JavascriptOperators::GetTypeId(srcInstance);

        if (instanceType != JavascriptOperators::GetTypeId(dstInstance))
        {TRACE_IT(50650);
            return false;
        }

        if (srcStart != dstStart)
        {TRACE_IT(50651);
            return false;
        }

        BOOL  returnValue = false;
#define MEMCOPY_TYPED_ARRAY(type, conversion) type ## ::FromVar(dstInstance)->DirectSetItemAtRange( type ## ::FromVar(srcInstance), srcStart, dstStart, length, JavascriptConversion:: ## conversion)
        switch (instanceType)
        {
        case TypeIds_Int8Array:
        {TRACE_IT(50652);
            returnValue = MEMCOPY_TYPED_ARRAY(Int8Array, ToInt8);
            break;
        }
        case TypeIds_Uint8Array:
        {TRACE_IT(50653);
            returnValue = MEMCOPY_TYPED_ARRAY(Uint8Array, ToUInt8);
            break;
        }
        case TypeIds_Uint8ClampedArray:
        {TRACE_IT(50654);
            returnValue = MEMCOPY_TYPED_ARRAY(Uint8ClampedArray, ToUInt8Clamped);
            break;
        }
        case TypeIds_Int16Array:
        {TRACE_IT(50655);
            returnValue = MEMCOPY_TYPED_ARRAY(Int16Array, ToInt16);
            break;
        }
        case TypeIds_Uint16Array:
        {TRACE_IT(50656);
            returnValue = MEMCOPY_TYPED_ARRAY(Uint16Array, ToUInt16);
            break;
        }
        case TypeIds_Int32Array:
        {TRACE_IT(50657);
            returnValue = MEMCOPY_TYPED_ARRAY(Int32Array, ToInt32);
            break;
        }
        case TypeIds_Uint32Array:
        {TRACE_IT(50658);
            returnValue = MEMCOPY_TYPED_ARRAY(Uint32Array, ToUInt32);
            break;
        }
        case TypeIds_Float32Array:
        {TRACE_IT(50659);
            returnValue = MEMCOPY_TYPED_ARRAY(Float32Array, ToFloat);
            break;
        }
        case TypeIds_Float64Array:
        {TRACE_IT(50660);
            returnValue = MEMCOPY_TYPED_ARRAY(Float64Array, ToNumber);
            break;
        }
        case TypeIds_Array:
        case TypeIds_NativeFloatArray:
        case TypeIds_NativeIntArray:
        {TRACE_IT(50661);
            if (dstStart < 0 || srcStart < 0)
            {TRACE_IT(50662);
                // This is not supported, Bailout
                break;
            }
            // Upper bounds check for source array
            JavascriptArray* srcArray = JavascriptArray::FromVar(srcInstance);
            JavascriptArray* dstArray = JavascriptArray::FromVar(dstInstance);
            if (scriptContext->optimizationOverrides.IsEnabledArraySetElementFastPath())
            {TRACE_IT(50663);
                INT_PTR vt = VirtualTableInfoBase::GetVirtualTable(dstInstance);
                if (instanceType == TypeIds_Array)
                {TRACE_IT(50664);
                    returnValue = dstArray->DirectSetItemAtRangeFromArray<Var>(dstStart, length, srcArray, srcStart);
                }
                else if (instanceType == TypeIds_NativeIntArray)
                {TRACE_IT(50665);
                    returnValue = dstArray->DirectSetItemAtRangeFromArray<int32>(dstStart, length, srcArray, srcStart);
                }
                else
                {TRACE_IT(50666);
                    returnValue = dstArray->DirectSetItemAtRangeFromArray<double>(dstStart, length, srcArray, srcStart);
                }
                returnValue &= vt == VirtualTableInfoBase::GetVirtualTable(dstInstance);
            }
            break;
        }
        default:
            AssertMsg(false, "We don't support this type for memcopy yet.");
            break;
        }
#undef MEMCOPY_TYPED_ARRAY
        return returnValue;
    }

    BOOL JavascriptOperators::OP_Memset(Var instance, int32 start, Var value, int32 length, ScriptContext* scriptContext)
    {TRACE_IT(50667);
        if (length <= 0)
        {TRACE_IT(50668);
            return false;
        }
        TypeId instanceType = JavascriptOperators::GetTypeId(instance);
        BOOL  returnValue = false;

        // The typed array will deal with all possible values for the index
#define MEMSET_TYPED_ARRAY(type, conversion) type ## ::FromVar(instance)->DirectSetItemAtRange(start, length, value, JavascriptConversion:: ## conversion)
        switch (instanceType)
        {
        case TypeIds_Int8Array:
        {TRACE_IT(50669);
            returnValue = MEMSET_TYPED_ARRAY(Int8Array, ToInt8);
            break;
        }
        case TypeIds_Uint8Array:
        {TRACE_IT(50670);
            returnValue = MEMSET_TYPED_ARRAY(Uint8Array, ToUInt8);
            break;
        }
        case TypeIds_Uint8ClampedArray:
        {TRACE_IT(50671);
            returnValue = MEMSET_TYPED_ARRAY(Uint8ClampedArray, ToUInt8Clamped);
            break;
        }
        case TypeIds_Int16Array:
        {TRACE_IT(50672);
            returnValue = MEMSET_TYPED_ARRAY(Int16Array, ToInt16);
            break;
        }
        case TypeIds_Uint16Array:
        {TRACE_IT(50673);
            returnValue = MEMSET_TYPED_ARRAY(Uint16Array, ToUInt16);
            break;
        }
        case TypeIds_Int32Array:
        {TRACE_IT(50674);
            returnValue = MEMSET_TYPED_ARRAY(Int32Array, ToInt32);
            break;
        }
        case TypeIds_Uint32Array:
        {TRACE_IT(50675);
            returnValue = MEMSET_TYPED_ARRAY(Uint32Array, ToUInt32);
            break;
        }
        case TypeIds_Float32Array:
        {TRACE_IT(50676);
            returnValue = MEMSET_TYPED_ARRAY(Float32Array, ToFloat);
            break;
        }
        case TypeIds_Float64Array:
        {TRACE_IT(50677);
            returnValue = MEMSET_TYPED_ARRAY(Float64Array, ToNumber);
            break;
        }
        case TypeIds_NativeFloatArray:
        case TypeIds_NativeIntArray:
        case TypeIds_Array:
        {TRACE_IT(50678);
            if (start < 0)
            {TRACE_IT(50679);
                for (start; start < 0 && length > 0; ++start, --length)
                {
                    if (!OP_SetElementI(instance, JavascriptNumber::ToVar(start, scriptContext), value, scriptContext))
                    {TRACE_IT(50680);
                        return false;
                    }
                }
            }
            if (scriptContext->optimizationOverrides.IsEnabledArraySetElementFastPath())
            {TRACE_IT(50681);
                INT_PTR vt = VirtualTableInfoBase::GetVirtualTable(instance);
                if (instanceType == TypeIds_Array)
                {TRACE_IT(50682);
                    returnValue = JavascriptArray::FromVar(instance)->DirectSetItemAtRange<Var>(start, length, value);
                }
                else if (instanceType == TypeIds_NativeIntArray)
                {TRACE_IT(50683);
                    // Only accept tagged int. Also covers case for MissingItem
                    if (!TaggedInt::Is(value))
                    {TRACE_IT(50684);
                        return false;
                    }
                    int32 intValue = JavascriptConversion::ToInt32(value, scriptContext);
                    returnValue = JavascriptArray::FromVar(instance)->DirectSetItemAtRange<int32>(start, length, intValue);
                }
                else
                {TRACE_IT(50685);
                    // For native float arrays, the jit doesn't check the type of the source so we have to do it here
                    if (!JavascriptNumber::Is(value) && !TaggedNumber::Is(value))
                    {TRACE_IT(50686);
                        return false;
                    }

                    double doubleValue = JavascriptConversion::ToNumber(value, scriptContext);
                    // Special case for missing item
                    if (SparseArraySegment<double>::IsMissingItem(&doubleValue))
                    {TRACE_IT(50687);
                        return false;
                    }
                    returnValue = JavascriptArray::FromVar(instance)->DirectSetItemAtRange<double>(start, length, doubleValue);
                }
                returnValue &= vt == VirtualTableInfoBase::GetVirtualTable(instance);
            }
            break;
        }
        default:
            AssertMsg(false, "We don't support this type for memset yet.");
            break;
        }

#undef MEMSET_TYPED_ARRAY
        return returnValue;
    }

    Var JavascriptOperators::OP_DeleteElementI_UInt32(Var instance, uint32 index, ScriptContext* scriptContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50688);
#if FLOATVAR
        return OP_DeleteElementI(instance, Js::JavascriptNumber::ToVar(index, scriptContext), scriptContext, propertyOperationFlags);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_DeleteElementI(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext, propertyOperationFlags);
#endif
    }

    Var JavascriptOperators::OP_DeleteElementI_Int32(Var instance, int32 index, ScriptContext* scriptContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50689);
#if FLOATVAR
        return OP_DeleteElementI(instance, Js::JavascriptNumber::ToVar(index, scriptContext), scriptContext, propertyOperationFlags);
#else
        char buffer[sizeof(Js::JavascriptNumber)];
        return OP_DeleteElementI(instance, Js::JavascriptNumber::ToVarInPlace(index, scriptContext,
            (Js::JavascriptNumber *)buffer), scriptContext, propertyOperationFlags);
#endif
    }

    Var JavascriptOperators::OP_DeleteElementI(Var instance, Var index, ScriptContext* scriptContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(50690);
        if(TaggedNumber::Is(instance))
        {TRACE_IT(50691);
            return scriptContext->GetLibrary()->GetTrue();
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        TypeId typeId = JavascriptOperators::GetTypeId(instance);
        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {TRACE_IT(50692);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotDelete_NullOrUndefined, GetPropertyDisplayNameForError(index, scriptContext));
        }

        RecyclableObject* object = RecyclableObject::FromVar(instance);

        uint32 indexVal;
        PropertyRecord const * propertyRecord;
        JavascriptString * propertyNameString = nullptr;
        BOOL result = TRUE;
        IndexType indexType = GetIndexType(index, scriptContext, &indexVal, &propertyRecord, &propertyNameString, false, true);

        if (indexType == IndexType_Number)
        {TRACE_IT(50693);
            result = JavascriptOperators::DeleteItem(object, indexVal, propertyOperationFlags);
        }
        else if (indexType == IndexType_JavascriptString)
        {TRACE_IT(50694);
            result = JavascriptOperators::DeleteProperty(object, propertyNameString, propertyOperationFlags);
        }
        else
        {TRACE_IT(50695);
            Assert(indexType == IndexType_PropertyId);

            if (propertyRecord == nullptr && !JavascriptOperators::CanShortcutOnUnknownPropertyName(object))
            {TRACE_IT(50696);
                indexType = GetIndexTypeFromPrimitive(index, scriptContext, &indexVal, &propertyRecord, true);
                Assert(indexType == IndexType_PropertyId);
                Assert(propertyRecord != nullptr);
            }

            if (propertyRecord != nullptr)
            {TRACE_IT(50697);
                result = JavascriptOperators::DeleteProperty(object, propertyRecord->GetPropertyId(), propertyOperationFlags);
            }
#if DBG
            else
            {TRACE_IT(50698);
                JavascriptString* indexStr = JavascriptConversion::ToString(index, scriptContext);
                PropertyRecord const * debugPropertyRecord;
                scriptContext->GetOrAddPropertyRecord(indexStr->GetString(), indexStr->GetLength(), &debugPropertyRecord);
                AssertMsg(JavascriptOperators::DeleteProperty(object, debugPropertyRecord->GetPropertyId(), propertyOperationFlags), "delete should have been true. See OS Bug 2727708 if you see this come from the web");
            }
#endif
        }

        return scriptContext->GetLibrary()->CreateBoolean(result);
    }

    Var JavascriptOperators::OP_GetLength(Var instance, ScriptContext* scriptContext)
    {TRACE_IT(50699);
        return JavascriptOperators::OP_GetProperty(instance, PropertyIds::length, scriptContext);
    }

    Var JavascriptOperators::GetThisFromModuleRoot(Var thisVar)
    {TRACE_IT(50700);
        RootObjectBase * rootObject = static_cast<RootObjectBase*>(thisVar);
        RecyclableObject* hostObject = rootObject->GetHostObject();

        //
        // if the module root has the host object, use that as "this"
        //
        if (hostObject)
        {TRACE_IT(50701);
            thisVar = hostObject->GetHostDispatchVar();
        }
        return thisVar;
    }

    inline void JavascriptOperators::TryLoadRoot(Var& thisVar, TypeId typeId, int moduleID, ScriptContextInfo* scriptContext)
    {TRACE_IT(50702);
        bool loadRoot = false;
        if (JavascriptOperators::IsUndefinedOrNullType(typeId) || typeId == TypeIds_ActivationObject)
        {TRACE_IT(50703);
            loadRoot = true;
        }
        else if (typeId == TypeIds_HostDispatch)
        {TRACE_IT(50704);
            TypeId remoteTypeId;
            if (RecyclableObject::FromVar(thisVar)->GetRemoteTypeId(&remoteTypeId))
            {TRACE_IT(50705);
                if (remoteTypeId == TypeIds_Null || remoteTypeId == TypeIds_Undefined || remoteTypeId == TypeIds_ActivationObject)
                {TRACE_IT(50706);
                    loadRoot = true;
                }
            }
        }

        if (loadRoot)
        {TRACE_IT(50707);
            if (moduleID == 0)
            {TRACE_IT(50708);
                thisVar = (Js::Var)scriptContext->GetGlobalObjectThisAddr();
            }
            else
            {TRACE_IT(50709);
                // TODO: OOP JIT, create a copy of module roots in server side
                Js::ModuleRoot * moduleRoot = JavascriptOperators::GetModuleRoot(moduleID, (ScriptContext*)scriptContext);
                if (moduleRoot == nullptr)
                {TRACE_IT(50710);
                    Assert(false);
                    thisVar = (Js::Var)scriptContext->GetUndefinedAddr();
                }
                else
                {TRACE_IT(50711);
                    thisVar = GetThisFromModuleRoot(moduleRoot);
                }
            }
        }
    }

    Var JavascriptOperators::OP_GetThis(Var thisVar, int moduleID, ScriptContextInfo* scriptContext)
    {TRACE_IT(50712);
        //
        // if "this" is null or undefined
        //   Pass the global object
        // Else
        //   Pass ToObject(this)
        //
        TypeId typeId = JavascriptOperators::GetTypeId(thisVar);

        Assert(!JavascriptOperators::IsThisSelf(typeId));

        return JavascriptOperators::GetThisHelper(thisVar, typeId, moduleID, scriptContext);
    }

    Var JavascriptOperators::OP_GetThisNoFastPath(Var thisVar, int moduleID, ScriptContext* scriptContext)
    {TRACE_IT(50713);
        TypeId typeId = JavascriptOperators::GetTypeId(thisVar);

        if (JavascriptOperators::IsThisSelf(typeId))
        {TRACE_IT(50714);
            Assert(typeId != TypeIds_GlobalObject || ((Js::GlobalObject*)thisVar)->ToThis() == thisVar);
            Assert(typeId != TypeIds_ModuleRoot || JavascriptOperators::GetThisFromModuleRoot(thisVar) == thisVar);

            return thisVar;
        }

        return JavascriptOperators::GetThisHelper(thisVar, typeId, moduleID, scriptContext);
    }

    bool JavascriptOperators::IsThisSelf(TypeId typeId)
    {TRACE_IT(50715);
        return (JavascriptOperators::IsObjectType(typeId) && ! JavascriptOperators::IsSpecialObjectType(typeId));
    }

    Var JavascriptOperators::GetThisHelper(Var thisVar, TypeId typeId, int moduleID, ScriptContextInfo *scriptContext)
    {TRACE_IT(50716);
        if (! JavascriptOperators::IsObjectType(typeId) && ! JavascriptOperators::IsUndefinedOrNullType(typeId))
        {TRACE_IT(50717);
#if ENABLE_NATIVE_CODEGEN
            Assert(!JITManager::GetJITManager()->IsJITServer());
#endif
#if !FLOATVAR
            // We allowed stack number to be used as the "this" for getter and setter activation of
            // n.x and n[prop], where n is the Javascript Number
            return JavascriptOperators::ToObject(
                JavascriptNumber::BoxStackNumber(thisVar, (ScriptContext*)scriptContext), (ScriptContext*)scriptContext);
#else
            return JavascriptOperators::ToObject(thisVar, (ScriptContext*)scriptContext);
#endif

        }
        else
        {
            TryLoadRoot(thisVar, typeId, moduleID, scriptContext);
            return thisVar;
        }
    }

    Var JavascriptOperators::OP_StrictGetThis(Var thisVar, ScriptContext* scriptContext)
    {TRACE_IT(50718);
        TypeId typeId = JavascriptOperators::GetTypeId(thisVar);

        if (typeId == TypeIds_ActivationObject)
        {TRACE_IT(50719);
            return scriptContext->GetLibrary()->GetUndefined();
        }

        return thisVar;
    }

    BOOL JavascriptOperators::GetRemoteTypeId(Var aValue, TypeId* typeId)
    {TRACE_IT(50720);
        if (GetTypeId(aValue) != TypeIds_HostDispatch)
        {TRACE_IT(50721);
            return FALSE;
        }
        return RecyclableObject::FromVar(aValue)->GetRemoteTypeId(typeId);
    }

    BOOL JavascriptOperators::IsJsNativeObject(Var aValue)
    {TRACE_IT(50722);
        switch(GetTypeId(aValue))
        {
            case TypeIds_Object:
            case TypeIds_Function:
            case TypeIds_Array:
            case TypeIds_NativeIntArray:
#if ENABLE_COPYONACCESS_ARRAY
            case TypeIds_CopyOnAccessNativeIntArray:
#endif
            case TypeIds_NativeFloatArray:
            case TypeIds_ES5Array:
            case TypeIds_Date:
            case TypeIds_WinRTDate:
            case TypeIds_RegEx:
            case TypeIds_Error:
            case TypeIds_BooleanObject:
            case TypeIds_NumberObject:
#ifdef ENABLE_SIMDJS
            case TypeIds_SIMDObject:
#endif
            case TypeIds_StringObject:
            case TypeIds_Symbol:
            case TypeIds_SymbolObject:
            //case TypeIds_GlobalObject:
            //case TypeIds_ModuleRoot:
            //case TypeIds_HostObject:
            case TypeIds_Arguments:
            case TypeIds_ActivationObject:
            case TypeIds_Map:
            case TypeIds_Set:
            case TypeIds_WeakMap:
            case TypeIds_WeakSet:
            case TypeIds_ArrayIterator:
            case TypeIds_MapIterator:
            case TypeIds_SetIterator:
            case TypeIds_StringIterator:
            case TypeIds_Generator:
            case TypeIds_Promise:
            case TypeIds_Proxy:
                return true;
            default:
                return false;
        }
    }

    bool JavascriptOperators::CanShortcutOnUnknownPropertyName(RecyclableObject *instance)
    {TRACE_IT(50723);
        if (!CanShortcutInstanceOnUnknownPropertyName(instance))
        {TRACE_IT(50724);
            return false;
        }
        return CanShortcutPrototypeChainOnUnknownPropertyName(instance->GetPrototype());
    }

    bool JavascriptOperators::CanShortcutInstanceOnUnknownPropertyName(RecyclableObject *instance)
    {TRACE_IT(50725);
        if (PHASE_OFF1(Js::OptUnknownElementNamePhase))
        {TRACE_IT(50726);
            return false;
        }

        TypeId typeId = instance->GetTypeId();
        if (typeId == TypeIds_Proxy || typeId == TypeIds_HostDispatch)
        {TRACE_IT(50727);
            return false;
        }
        if (DynamicType::Is(typeId) &&
            static_cast<DynamicObject*>(instance)->GetTypeHandler()->IsStringTypeHandler())
        {TRACE_IT(50728);
            return false;
        }
        if (instance->IsExternal())
        {TRACE_IT(50729);
            return false;
        }
        return !(instance->HasDeferredTypeHandler() &&
                 JavascriptFunction::Is(instance) &&
                 JavascriptFunction::FromVar(instance)->IsExternalFunction());
    }

    bool JavascriptOperators::CanShortcutPrototypeChainOnUnknownPropertyName(RecyclableObject *prototype)
    {TRACE_IT(50730);
        Assert(prototype);

        for (; prototype->GetTypeId() != TypeIds_Null; prototype = prototype->GetPrototype())
        {TRACE_IT(50731);
            if (!CanShortcutInstanceOnUnknownPropertyName(prototype))
            {TRACE_IT(50732);
                return false;
            }
        }
        return true;
    }

    RecyclableObject* JavascriptOperators::GetPrototype(RecyclableObject* instance)
    {TRACE_IT(50733);
        if (JavascriptOperators::GetTypeId(instance) == TypeIds_Null)
        {TRACE_IT(50734);
            return instance;
        }
        return instance->GetPrototype();
    }

    RecyclableObject* JavascriptOperators::OP_GetPrototype(Var instance, ScriptContext* scriptContext)
    {TRACE_IT(50735);
        if (TaggedNumber::Is(instance))
        {TRACE_IT(50736);
            return scriptContext->GetLibrary()->GetNumberPrototype();
        }
        else if (JavascriptOperators::GetTypeId(instance) != TypeIds_Null)
        {TRACE_IT(50737);
            return JavascriptOperators::GetPrototype(RecyclableObject::FromVar(instance));
        }
        else
        {TRACE_IT(50738);
            return scriptContext->GetLibrary()->GetNull();
        }
    }

     BOOL JavascriptOperators::OP_BrFncEqApply(Var instance, ScriptContext *scriptContext)
     {TRACE_IT(50739);
         // JavascriptFunction && !HostDispatch
         if (JavascriptOperators::GetTypeId(instance) == TypeIds_Function)
         {TRACE_IT(50740);
             FunctionProxy *bod= ((JavascriptFunction*)instance)->GetFunctionProxy();
             if (bod != nullptr)
             {TRACE_IT(50741);
                 return bod->GetDirectEntryPoint(bod->GetDefaultEntryPointInfo()) == &Js::JavascriptFunction::EntryApply;
             }
             else
             {TRACE_IT(50742);
                 FunctionInfo* info = ((JavascriptFunction *)instance)->GetFunctionInfo();
                 if (info != nullptr)
                 {TRACE_IT(50743);
                     return &Js::JavascriptFunction::EntryApply == info->GetOriginalEntryPoint();
                 }
                 else
                 {TRACE_IT(50744);
                     return false;
                 }
             }
         }

         return false;
     }

     BOOL JavascriptOperators::OP_BrFncNeqApply(Var instance, ScriptContext *scriptContext)
     {TRACE_IT(50745);
         // JavascriptFunction and !HostDispatch
         if (JavascriptOperators::GetTypeId(instance) == TypeIds_Function)
         {TRACE_IT(50746);
             FunctionProxy *bod = ((JavascriptFunction *)instance)->GetFunctionProxy();
             if (bod != nullptr)
             {TRACE_IT(50747);
                 return bod->GetDirectEntryPoint(bod->GetDefaultEntryPointInfo()) != &Js::JavascriptFunction::EntryApply;
             }
             else
             {TRACE_IT(50748);
                 FunctionInfo* info = ((JavascriptFunction *)instance)->GetFunctionInfo();
                 if (info != nullptr)
                 {TRACE_IT(50749);
                     return &Js::JavascriptFunction::EntryApply != info->GetOriginalEntryPoint();
                 }
                 else
                 {TRACE_IT(50750);
                     return true;
                 }
             }
         }

         return true;
     }

    BOOL JavascriptOperators::OP_BrHasSideEffects(int se, ScriptContext* scriptContext)
    {TRACE_IT(50751);
        return (scriptContext->optimizationOverrides.GetSideEffects() & se) != SideEffects_None;
    }

    BOOL JavascriptOperators::OP_BrNotHasSideEffects(int se, ScriptContext* scriptContext)
    {TRACE_IT(50752);
        return (scriptContext->optimizationOverrides.GetSideEffects() & se) == SideEffects_None;
    }

    // returns NULL if there is no more elements to enumerate.
    Var JavascriptOperators::OP_BrOnEmpty(ForInObjectEnumerator * aEnumerator)
    {TRACE_IT(50753);
        PropertyId id;
        return aEnumerator->MoveAndGetNext(id);
    }

    void JavascriptOperators::OP_InitForInEnumerator(Var enumerable, ForInObjectEnumerator * enumerator, ScriptContext* scriptContext, ForInCache * forInCache)
    {TRACE_IT(50754);
        RecyclableObject* enumerableObject;
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(enumerable);
#endif
        if (!GetPropertyObject(enumerable, scriptContext, &enumerableObject))
        {TRACE_IT(50755);
            enumerableObject = nullptr;
        }

        enumerator->Initialize(enumerableObject, scriptContext, false, forInCache);
    }

    Js::Var JavascriptOperators::OP_CmEq_A(Var a, Var b, ScriptContext* scriptContext)
    {TRACE_IT(50756);
       return JavascriptBoolean::ToVar(JavascriptOperators::Equal(a, b, scriptContext), scriptContext);
    }

    Var JavascriptOperators::OP_CmNeq_A(Var a, Var b, ScriptContext* scriptContext)
    {TRACE_IT(50757);
        return JavascriptBoolean::ToVar(JavascriptOperators::NotEqual(a,b,scriptContext), scriptContext);
    }

    Var JavascriptOperators::OP_CmSrEq_A(Var a, Var b, ScriptContext* scriptContext)
    {TRACE_IT(50758);
       return JavascriptBoolean::ToVar(JavascriptOperators::StrictEqual(a, b, scriptContext), scriptContext);
    }

    Var JavascriptOperators::OP_CmSrEq_String(Var a, Var b, ScriptContext *scriptContext)
    {TRACE_IT(50759);
        return JavascriptBoolean::ToVar(JavascriptOperators::StrictEqualString(a, b), scriptContext);
    }

    Var JavascriptOperators::OP_CmSrEq_EmptyString(Var a, ScriptContext *scriptContext)
    {TRACE_IT(50760);
        return JavascriptBoolean::ToVar(JavascriptOperators::StrictEqualEmptyString(a), scriptContext);
    }

    Var JavascriptOperators::OP_CmSrNeq_A(Var a, Var b, ScriptContext* scriptContext)
    {TRACE_IT(50761);
        return JavascriptBoolean::ToVar(JavascriptOperators::NotStrictEqual(a, b, scriptContext), scriptContext);
    }

    Var JavascriptOperators::OP_CmLt_A(Var a, Var b, ScriptContext* scriptContext)
    {TRACE_IT(50762);
        return JavascriptBoolean::ToVar(JavascriptOperators::Less(a, b, scriptContext), scriptContext);
    }

    Var JavascriptOperators::OP_CmLe_A(Var a, Var b, ScriptContext* scriptContext)
    {TRACE_IT(50763);
        return JavascriptBoolean::ToVar(JavascriptOperators::LessEqual(a, b, scriptContext), scriptContext);
    }

    Var JavascriptOperators::OP_CmGt_A(Var a, Var b, ScriptContext* scriptContext)
    {TRACE_IT(50764);
        return JavascriptBoolean::ToVar(JavascriptOperators::Greater(a, b, scriptContext), scriptContext);
    }

    Var JavascriptOperators::OP_CmGe_A(Var a, Var b, ScriptContext* scriptContext)
    {TRACE_IT(50765);
        return JavascriptBoolean::ToVar(JavascriptOperators::GreaterEqual(a, b, scriptContext), scriptContext);
    }

    DetachedStateBase* JavascriptOperators::DetachVarAndGetState(Var var)
    {TRACE_IT(50766);
        switch (GetTypeId(var))
        {
        case TypeIds_ArrayBuffer:
            return Js::ArrayBuffer::FromVar(var)->DetachAndGetState();
        default:
            if (!Js::RecyclableObject::FromVar(var)->IsExternal())
            {
                AssertMsg(false, "We should explicitly have a case statement for each non-external object that can be detached.");
            }
            return nullptr;
        }
    }

    bool JavascriptOperators::IsObjectDetached(Var var)
    {TRACE_IT(50767);
        switch (GetTypeId(var))
        {
        case TypeIds_ArrayBuffer:
            return Js::ArrayBuffer::FromVar(var)->IsDetached();
        default:
            return false;
        }
    }

    Var JavascriptOperators::NewVarFromDetachedState(DetachedStateBase* state, JavascriptLibrary *library)
    {TRACE_IT(50768);
        switch (state->GetTypeId())
        {
        case TypeIds_SharedArrayBuffer:
            return Js::SharedArrayBuffer::NewFromSharedState(state, library);
        case TypeIds_ArrayBuffer:
            return Js::ArrayBuffer::NewFromDetachedState(state, library);
        default:
            AssertMsg(false, "We should explicitly have a case statement for each object which has detached state.");
            return nullptr;
        }
    }

    DynamicType *
    JavascriptOperators::EnsureObjectLiteralType(ScriptContext* scriptContext, const Js::PropertyIdArray *propIds, Field(DynamicType*)* literalType)
    {TRACE_IT(50769);
        DynamicType * newType = *literalType;
        if (newType != nullptr)
        {TRACE_IT(50770);
            if (!newType->GetIsShared())
            {TRACE_IT(50771);
                newType->ShareType();
            }
        }
        else
        {TRACE_IT(50772);
            DynamicType* objectType =
                FunctionBody::DoObjectHeaderInliningForObjectLiteral(propIds)
                    ?   scriptContext->GetLibrary()->GetObjectHeaderInlinedLiteralType((uint16)propIds->count)
                    :   scriptContext->GetLibrary()->GetObjectLiteralType(
                            static_cast<PropertyIndex>(
                                min(propIds->count, static_cast<uint32>(MaxPreInitializedObjectTypeInlineSlotCount))));
            newType = PathTypeHandlerBase::CreateTypeForNewScObject(scriptContext, objectType, propIds, false);
            *literalType = newType;
        }

        Assert(scriptContext);
        Assert(GetLiteralInlineSlotCapacity(propIds) == newType->GetTypeHandler()->GetInlineSlotCapacity());
        Assert(newType->GetTypeHandler()->GetSlotCapacity() >= 0);
        Assert(GetLiteralSlotCapacity(propIds) == (uint)newType->GetTypeHandler()->GetSlotCapacity());
        return newType;
    }

    Var JavascriptOperators::NewScObjectLiteral(ScriptContext* scriptContext, const Js::PropertyIdArray *propIds, Field(DynamicType*)* literalType)
    {TRACE_IT(50773);
        Assert(propIds->count != 0);
        Assert(!propIds->hadDuplicates);        // duplicates are removed by parser

#ifdef PROFILE_OBJECT_LITERALS
        // Empty objects not counted in the object literal counts
        scriptContext->objectLiteralInstanceCount++;
        if (propIds->count > scriptContext->objectLiteralMaxLength)
        {TRACE_IT(50774);
            scriptContext->objectLiteralMaxLength = propIds->count;
        }
#endif

        DynamicType* newType = EnsureObjectLiteralType(scriptContext, propIds, literalType);
        DynamicObject* instance = DynamicObject::New(scriptContext->GetRecycler(), newType);

        if (!newType->GetIsShared())
        {TRACE_IT(50775);
            newType->GetTypeHandler()->SetSingletonInstanceIfNeeded(instance);
        }
#ifdef PROFILE_OBJECT_LITERALS
        else
        {TRACE_IT(50776);
            scriptContext->objectLiteralCacheCount++;
        }
#endif
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(instance));
        // can't auto-proxy here as object literal is not exactly "new" object and cannot be intercepted as proxy.
        return instance;
    }

    uint JavascriptOperators::GetLiteralSlotCapacity(Js::PropertyIdArray const * propIds)
    {TRACE_IT(50777);
        const uint inlineSlotCapacity = GetLiteralInlineSlotCapacity(propIds);
        return DynamicTypeHandler::RoundUpSlotCapacity(propIds->count, static_cast<PropertyIndex>(inlineSlotCapacity));
    }

    uint JavascriptOperators::GetLiteralInlineSlotCapacity(
        Js::PropertyIdArray const * propIds)
    {TRACE_IT(50778);
        if (propIds->hadDuplicates)
        {TRACE_IT(50779);
            return 0;
        }

        return
            FunctionBody::DoObjectHeaderInliningForObjectLiteral(propIds)
                ?   DynamicTypeHandler::RoundUpObjectHeaderInlinedInlineSlotCapacity(static_cast<PropertyIndex>(propIds->count))
                :   DynamicTypeHandler::RoundUpInlineSlotCapacity(
                        static_cast<PropertyIndex>(
                            min(propIds->count, static_cast<uint32>(MaxPreInitializedObjectTypeInlineSlotCount))));
    }

    Var JavascriptOperators::OP_InitCachedScope(Var varFunc, const Js::PropertyIdArray *propIds, Field(DynamicType*)* literalType, bool formalsAreLetDecls, ScriptContext *scriptContext)
    {TRACE_IT(50780);
        bool isGAFunction = JavascriptFunction::Is(varFunc);
        Assert(isGAFunction);
        if (isGAFunction)
        {TRACE_IT(50781);
            JavascriptFunction *function = JavascriptFunction::FromVar(varFunc);
            isGAFunction = JavascriptGeneratorFunction::Test(function) || JavascriptAsyncFunction::Test(function);
        }

        ScriptFunction *func = isGAFunction ?
            JavascriptGeneratorFunction::FromVar(varFunc)->GetGeneratorVirtualScriptFunction() :
            ScriptFunction::FromVar(varFunc);

#ifdef PROFILE_OBJECT_LITERALS
        // Empty objects not counted in the object literal counts
        scriptContext->objectLiteralInstanceCount++;
        if (propIds->count > scriptContext->objectLiteralMaxLength)
        {TRACE_IT(50782);
            scriptContext->objectLiteralMaxLength = propIds->count;
        }
#endif

        PropertyId cachedFuncCount = ActivationObjectEx::GetCachedFuncCount(propIds);
        PropertyId firstFuncSlot = ActivationObjectEx::GetFirstFuncSlot(propIds);
        PropertyId firstVarSlot = ActivationObjectEx::GetFirstVarSlot(propIds);
        PropertyId lastFuncSlot = Constants::NoProperty;

        if (firstFuncSlot != Constants::NoProperty)
        {TRACE_IT(50783);
            if (firstVarSlot == Constants::NoProperty)
            {TRACE_IT(50784);
                lastFuncSlot = propIds->count - 1;
            }
            else
            {TRACE_IT(50785);
                lastFuncSlot = firstVarSlot - 1;
            }
        }

        DynamicType *type = *literalType;
        if (type != nullptr)
        {TRACE_IT(50786);
#ifdef PROFILE_OBJECT_LITERALS
            scriptContext->objectLiteralCacheCount++;
#endif
        }
        else
        {TRACE_IT(50787);
            type = scriptContext->GetLibrary()->GetActivationObjectType();
            if (formalsAreLetDecls)
            {TRACE_IT(50788);
                uint formalsSlotLimit = (firstFuncSlot != Constants::NoProperty) ? (uint)firstFuncSlot :
                                        (firstVarSlot != Constants::NoProperty) ? (uint)firstVarSlot :
                                        propIds->count;
                type = PathTypeHandlerBase::CreateNewScopeObject(scriptContext, type, propIds, PropertyLet, formalsSlotLimit);
            }
            else
            {TRACE_IT(50789);
                type = PathTypeHandlerBase::CreateNewScopeObject(scriptContext, type, propIds);
            }
            *literalType = type;
        }
        Var undef = scriptContext->GetLibrary()->GetUndefined();

        ActivationObjectEx *scopeObjEx = func->GetCachedScope();
        if (scopeObjEx && scopeObjEx->IsCommitted())
        {TRACE_IT(50790);
            scopeObjEx->ReplaceType(type);
            scopeObjEx->SetCommit(false);
#if DBG
            for (uint i = firstVarSlot; i < propIds->count; i++)
            {TRACE_IT(50791);
                AssertMsg(scopeObjEx->GetSlot(i) == undef, "Var attached to cached scope");
            }
#endif
        }
        else
        {TRACE_IT(50792);
            ActivationObjectEx *tmp = RecyclerNewPlus(scriptContext->GetRecycler(), (cachedFuncCount == 0 ? 0 : cachedFuncCount - 1) * sizeof(FuncCacheEntry), ActivationObjectEx, type, func, cachedFuncCount, firstFuncSlot, lastFuncSlot);
            if (!scopeObjEx)
            {TRACE_IT(50793);
                func->SetCachedScope(tmp);
            }
            scopeObjEx = tmp;

            for (uint i = firstVarSlot; i < propIds->count; i++)
            {TRACE_IT(50794);
                scopeObjEx->SetSlot(SetSlotArguments(propIds->elements[i], i, undef));
            }
        }

        return scopeObjEx;
    }

    void JavascriptOperators::OP_InvalidateCachedScope(void* varEnv, int32 envIndex)
    {TRACE_IT(50795);
        FrameDisplay *disp = (FrameDisplay*)varEnv;
        RecyclableObject *objScope = RecyclableObject::FromVar(disp->GetItem(envIndex));
        objScope->InvalidateCachedScope();
    }

    void JavascriptOperators::OP_InitCachedFuncs(Var varScope, FrameDisplay *pDisplay, const FuncInfoArray *info, ScriptContext *scriptContext)
    {TRACE_IT(50796);
        ActivationObjectEx *scopeObj = (ActivationObjectEx*)ActivationObjectEx::FromVar(varScope);
        Assert(scopeObj->GetTypeHandler()->GetInlineSlotCapacity() == 0);

        uint funcCount = info->count;

        if (funcCount == 0)
        {TRACE_IT(50797);
            // Degenerate case: no nested funcs at all
            return;
        }

        if (scopeObj->HasCachedFuncs())
        {TRACE_IT(50798);
            for (uint i = 0; i < funcCount; i++)
            {TRACE_IT(50799);
                const FuncCacheEntry *entry = scopeObj->GetFuncCacheEntry(i);
                ScriptFunction *func = entry->func;

                FunctionProxy * proxy = func->GetFunctionProxy();

                // Reset the function's type to the default type with no properties
                // Use the cached type on the function proxy rather than the type in the func cache entry
                // CONSIDER: Stop caching the function types in the scope object
                func->ReplaceType(proxy->EnsureDeferredPrototypeType());
                func->ResetConstructorCacheToDefault();

                uint scopeSlot = info->elements[i].scopeSlot;
                if (scopeSlot != Constants::NoProperty)
                {TRACE_IT(50800);
                    // CONSIDER: Store property IDs in FuncInfoArray in debug builds so we can properly assert in SetAuxSlot
                    scopeObj->SetAuxSlot(SetSlotArguments(Constants::NoProperty, scopeSlot, entry->func));
                }
            }
            return;
        }

        // No cached functions, so create them and cache them.
        JavascriptFunction *funcParent = scopeObj->GetParentFunc();
        for (uint i = 0; i < funcCount; i++)
        {TRACE_IT(50801);
            const FuncInfoEntry *entry = &info->elements[i];
            uint nestedIndex = entry->nestedIndex;
            uint scopeSlot = entry->scopeSlot;

            FunctionProxy * proxy = funcParent->GetFunctionBody()->GetNestedFunctionProxy(nestedIndex);

            ScriptFunction *func = scriptContext->GetLibrary()->CreateScriptFunction(proxy);

            func->SetEnvironment(pDisplay);
            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(func, EtwTrace::GetFunctionId(proxy)));

            scopeObj->SetCachedFunc(i, func);
            if (scopeSlot != Constants::NoProperty)
            {TRACE_IT(50802);
                // CONSIDER: Store property IDs in FuncInfoArray in debug builds so we can properly assert in SetAuxSlot
                scopeObj->SetAuxSlot(SetSlotArguments(Constants::NoProperty, scopeSlot, func));
            }
        }
    }

    Var JavascriptOperators::AddVarsToArraySegment(SparseArraySegment<Var> * segment, const Js::VarArray *vars)
    {TRACE_IT(50803);
        uint32 count = vars->count;

        Assert(segment->left == 0);
        Assert(count <= segment->size);

        if(count > segment->length)
        {TRACE_IT(50804);
            segment->length = count;
        }
        CopyArray(segment->elements, segment->length, vars->elements, count);

        return segment;
    }

    void JavascriptOperators::AddIntsToArraySegment(SparseArraySegment<int32> * segment, const Js::AuxArray<int32> *ints)
    {TRACE_IT(50805);
        uint32 count = ints->count;

        Assert(segment->left == 0);
        Assert(count <= segment->size);

        if(count > segment->length)
        {TRACE_IT(50806);
            segment->length = count;
        }
        js_memcpy_s(segment->elements, sizeof(int32) * segment->length, ints->elements, sizeof(int32) * count);
    }

    void JavascriptOperators::AddFloatsToArraySegment(SparseArraySegment<double> * segment, const Js::AuxArray<double> *doubles)
    {TRACE_IT(50807);
        uint32 count = doubles->count;

        Assert(segment->left == 0);
        Assert(count <= segment->size);

        if(count > segment->length)
        {TRACE_IT(50808);
            segment->length = count;
        }
        js_memcpy_s(segment->elements, sizeof(double) * segment->length, doubles->elements, sizeof(double) * count);
    }

    RecyclableObject * JavascriptOperators::GetPrototypeObject(RecyclableObject * constructorFunction, ScriptContext * scriptContext)
    {TRACE_IT(50809);
        Var prototypeProperty = JavascriptOperators::GetProperty(constructorFunction, PropertyIds::prototype, scriptContext);
        RecyclableObject* prototypeObject;
        PrototypeObject(prototypeProperty, constructorFunction, scriptContext, &prototypeObject);
        return prototypeObject;
    }

    RecyclableObject * JavascriptOperators::GetPrototypeObjectForConstructorCache(RecyclableObject * constructor, ScriptContext* requestContext, bool& canBeCached)
    {TRACE_IT(50810);
        PropertyValueInfo info;
        Var prototypeValue;
        RecyclableObject* prototypeObject;

        canBeCached = false;

        // Do a local property lookup.  Since a function's prototype property is a non-configurable data property, we don't need to worry
        // about the prototype being an accessor property, whose getter returns different values every time it's called.
        if (constructor->GetProperty(constructor, PropertyIds::prototype, &prototypeValue, &info, requestContext))
        {TRACE_IT(50811);
            if (!JavascriptOperators::PrototypeObject(prototypeValue, constructor, requestContext, &prototypeObject))
            {TRACE_IT(50812);
                // The value returned by the property lookup is not a valid prototype object, default to object prototype.
                Assert(prototypeObject == constructor->GetLibrary()->GetObjectPrototype());
            }

            // For these scenarios, we do not want to populate the cache.
            if (constructor->GetScriptContext() != requestContext || info.GetInstance() != constructor)
            {TRACE_IT(50813);
                return prototypeObject;
            }
        }
        else
        {TRACE_IT(50814);
            // It's ok to cache Object.prototype, because Object.prototype cannot be overwritten.
            prototypeObject = constructor->GetLibrary()->GetObjectPrototype();
        }

        canBeCached = true;
        return prototypeObject;
    }

    bool JavascriptOperators::PrototypeObject(Var prototypeProperty, RecyclableObject * constructorFunction, ScriptContext * scriptContext, RecyclableObject** prototypeObject)
    {TRACE_IT(50815);
        TypeId prototypeType = JavascriptOperators::GetTypeId(prototypeProperty);

        if (JavascriptOperators::IsObjectType(prototypeType))
        {TRACE_IT(50816);
            *prototypeObject = RecyclableObject::FromVar(prototypeProperty);
            return true;
        }
        *prototypeObject = constructorFunction->GetLibrary()->GetObjectPrototype();
        return false;
    }

    FunctionInfo* JavascriptOperators::GetConstructorFunctionInfo(Var instance, ScriptContext * scriptContext)
    {TRACE_IT(50817);
        TypeId typeId = JavascriptOperators::GetTypeId(instance);
        if (typeId == TypeIds_Function)
        {TRACE_IT(50818);
            JavascriptFunction * function =  JavascriptFunction::FromVar(instance);
            return function->GetFunctionInfo();
        }
        if (typeId != TypeIds_HostDispatch && typeId != TypeIds_Proxy)
        {TRACE_IT(50819);
            if (typeId == TypeIds_Null)
            {TRACE_IT(50820);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
            }

            JavascriptError::ThrowTypeError(scriptContext, VBSERR_ActionNotSupported);
        }
        return nullptr;
    }

    Var JavascriptOperators::NewJavascriptObjectNoArg(ScriptContext* requestContext)
    {TRACE_IT(50821);
        DynamicObject * newObject = requestContext->GetLibrary()->CreateObject(true);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(newObject));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {TRACE_IT(50822);
            newObject = DynamicObject::FromVar(JavascriptProxy::AutoProxyWrapper(newObject));
        }
#endif
        return newObject;
    }

    Var JavascriptOperators::NewJavascriptArrayNoArg(ScriptContext* requestContext)
    {TRACE_IT(50823);
        JavascriptArray * newArray = requestContext->GetLibrary()->CreateArray();
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(newArray));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {TRACE_IT(50824);
            newArray = static_cast<JavascriptArray*>(JavascriptProxy::AutoProxyWrapper(newArray));
        }
#endif
        return newArray;
    }

    Var JavascriptOperators::NewScObjectNoArgNoCtorFull(Var instance, ScriptContext* requestContext)
    {TRACE_IT(50825);
        return NewScObjectNoArgNoCtorCommon(instance, requestContext, true);
    }

    Var JavascriptOperators::NewScObjectNoArgNoCtor(Var instance, ScriptContext* requestContext)
    {TRACE_IT(50826);
        return NewScObjectNoArgNoCtorCommon(instance, requestContext, false);
    }

    Var JavascriptOperators::NewScObjectNoArgNoCtorCommon(Var instance, ScriptContext* requestContext, bool isBaseClassConstructorNewScObject)
    {TRACE_IT(50827);
        RecyclableObject * object = RecyclableObject::FromVar(instance);
        FunctionInfo* functionInfo = JavascriptOperators::GetConstructorFunctionInfo(instance, requestContext);
        Assert(functionInfo != &JavascriptObject::EntryInfo::NewInstance); // built-ins are not inlined
        Assert(functionInfo != &JavascriptArray::EntryInfo::NewInstance); // built-ins are not inlined

        return functionInfo != nullptr ?
            JavascriptOperators::NewScObjectCommon(object, functionInfo, requestContext, isBaseClassConstructorNewScObject) :
            JavascriptOperators::NewScObjectHostDispatchOrProxy(object, requestContext);
    }

    Var JavascriptOperators::NewScObjectNoArg(Var instance, ScriptContext * requestContext)
    {TRACE_IT(50828);
        if (JavascriptProxy::Is(instance))
        {TRACE_IT(50829);
            Arguments args(CallInfo(CallFlags_New, 1), &instance);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(instance);
            return proxy->ConstructorTrap(args, requestContext, 0);
        }

        FunctionInfo* functionInfo = JavascriptOperators::GetConstructorFunctionInfo(instance, requestContext);
        RecyclableObject * object = RecyclableObject::FromVar(instance);

        if (functionInfo == &JavascriptObject::EntryInfo::NewInstance)
        {TRACE_IT(50830);
            // Fast path for new Object()
            Assert((functionInfo->GetAttributes() & FunctionInfo::ErrorOnNew) == 0);
            JavascriptLibrary* library = object->GetLibrary();

            DynamicObject * newObject = library->CreateObject(true);
            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(newObject));
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
            {TRACE_IT(50831);
                newObject = DynamicObject::FromVar(JavascriptProxy::AutoProxyWrapper(newObject));
            }
#endif

#if DBG
            DynamicType* newObjectType = newObject->GetDynamicType();
            Assert(newObjectType->GetIsShared());

            JavascriptFunction* constructor = JavascriptFunction::FromVar(instance);
            Assert(!constructor->GetConstructorCache()->NeedsUpdateAfterCtor());
#endif

            ScriptContext * scriptContext = library->GetScriptContext();
            if (scriptContext != requestContext)
            {TRACE_IT(50832);
                CrossSite::MarshalDynamicObjectAndPrototype(requestContext, newObject);
            }

            return newObject;
        }
        else if (functionInfo == &JavascriptArray::EntryInfo::NewInstance)
        {TRACE_IT(50833);
            Assert((functionInfo->GetAttributes() & FunctionInfo::ErrorOnNew) == 0);
            JavascriptLibrary* library = object->GetLibrary();

            JavascriptArray * newArray = library->CreateArray();
            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(newArray));
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
            {TRACE_IT(50834);
                newArray = static_cast<JavascriptArray*>(JavascriptProxy::AutoProxyWrapper(newArray));
            }
#endif

#if DBG
            DynamicType* newArrayType = newArray->GetDynamicType();
            Assert(newArrayType->GetIsShared());

            JavascriptFunction* constructor = JavascriptFunction::FromVar(instance);
            Assert(!constructor->GetConstructorCache()->NeedsUpdateAfterCtor());
#endif

            ScriptContext * scriptContext = library->GetScriptContext();
            if (scriptContext != requestContext)
            {TRACE_IT(50835);
                CrossSite::MarshalDynamicObjectAndPrototype(requestContext, newArray);
            }
            return newArray;
        }

        Var newObject = functionInfo != nullptr ?
            JavascriptOperators::NewScObjectCommon(object, functionInfo, requestContext) :
            JavascriptOperators::NewScObjectHostDispatchOrProxy(object, requestContext);

        Var returnVar = CALL_FUNCTION(object, CallInfo(CallFlags_New, 1), newObject);
        if (JavascriptOperators::IsObject(returnVar))
        {TRACE_IT(50836);
            newObject = returnVar;
        }

        ConstructorCache * constructorCache = nullptr;
        if (JavascriptFunction::Is(instance))
        {TRACE_IT(50837);
            constructorCache = JavascriptFunction::FromVar(instance)->GetConstructorCache();
        }

        if (constructorCache != nullptr && constructorCache->NeedsUpdateAfterCtor())
        {TRACE_IT(50838);
            JavascriptOperators::UpdateNewScObjectCache(object, newObject, requestContext);
        }

#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {TRACE_IT(50839);
            newObject = DynamicObject::FromVar(JavascriptProxy::AutoProxyWrapper(newObject));
            // this might come from a different scriptcontext.
            newObject = CrossSite::MarshalVar(requestContext, newObject);
        }
#endif

        return newObject;
    }

    Var JavascriptOperators::NewScObjectNoCtorFull(Var instance, ScriptContext* requestContext)
    {TRACE_IT(50840);
        return NewScObjectNoCtorCommon(instance, requestContext, true);
    }

    Var JavascriptOperators::NewScObjectNoCtor(Var instance, ScriptContext * requestContext)
    {TRACE_IT(50841);
        return NewScObjectNoCtorCommon(instance, requestContext, false);
    }

    Var JavascriptOperators::NewScObjectNoCtorCommon(Var instance, ScriptContext* requestContext, bool isBaseClassConstructorNewScObject)
    {TRACE_IT(50842);
        FunctionInfo* functionInfo = JavascriptOperators::GetConstructorFunctionInfo(instance, requestContext);

        if (functionInfo)
        {TRACE_IT(50843);
            return JavascriptOperators::NewScObjectCommon(RecyclableObject::FromVar(instance), functionInfo, requestContext, isBaseClassConstructorNewScObject);
        }
        else
        {TRACE_IT(50844);
            return JavascriptOperators::NewScObjectHostDispatchOrProxy(RecyclableObject::FromVar(instance), requestContext);
        }
    }

    Var JavascriptOperators::NewScObjectHostDispatchOrProxy(RecyclableObject * function, ScriptContext * requestContext)
    {TRACE_IT(50845);
        ScriptContext* functionScriptContext = function->GetScriptContext();

        if (JavascriptProxy::Is(function))
        {TRACE_IT(50846);
            // We can still call into NewScObjectNoCtor variations in JIT code for performance; however for proxy we don't
            // really need the new object as the trap will handle the "this" pointer separately. pass back nullptr to ensure
            // failure in invalid case.
            return  nullptr;
        }
        RecyclableObject * prototype = JavascriptOperators::GetPrototypeObject(function, functionScriptContext);
        prototype = RecyclableObject::FromVar(CrossSite::MarshalVar(requestContext, prototype));
        Var object = requestContext->GetLibrary()->CreateObject(prototype);
        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(object));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {TRACE_IT(50847);
            object = DynamicObject::FromVar(JavascriptProxy::AutoProxyWrapper(object));
        }
#endif
        return object;
    }

    Var JavascriptOperators::NewScObjectCommon(RecyclableObject * function, FunctionInfo* functionInfo, ScriptContext * requestContext, bool isBaseClassConstructorNewScObject)
    {TRACE_IT(50848);
        // CONSIDER: Allow for the cache to be repopulated if the type got collected, and a new one got populated with
        // the same number of inlined slots. This requires that the JIT-ed code actually load the type from the cache
        // (instead of hard-coding it), but it can (and must) keep the hard-coded number of inline slots.
        // CONSIDER: Consider also not pinning the type in the cache.  This can be done by using a registration based
        // weak reference (we need to control the memory address), which we don't yet have, or by allocating the cache from
        // the inline cache arena to allow it to be zeroed, but retain a recycler-allocated portion to hold on to the size of
        // inlined slots.

        JavascriptFunction* constructor = JavascriptFunction::FromVar(function);
        if (functionInfo->IsClassConstructor() && !isBaseClassConstructorNewScObject)
        {TRACE_IT(50849);
            // If we are calling new on a class constructor, the contract is that we pass new.target as the 'this' argument.
            // function is the constructor on which we called new - which is new.target.
            // If we are trying to construct the object for a base class constructor as part of a super call, we should not
            // store new.target in the 'this' argument.
            return function;
        }
        ConstructorCache* constructorCache = constructor->GetConstructorCache();
        AssertMsg(constructorCache->GetScriptContext() == nullptr || constructorCache->GetScriptContext() == constructor->GetScriptContext(),
            "Why did we populate a constructor cache with a mismatched script context?");

        Assert(constructorCache != nullptr);
        DynamicType* type = constructorCache->GetGuardValueAsType();
        if (type != nullptr && constructorCache->GetScriptContext() == requestContext)
        {TRACE_IT(50850);
#if DBG
            bool cachedProtoCanBeCached;
            Assert(type->GetPrototype() == JavascriptOperators::GetPrototypeObjectForConstructorCache(constructor, requestContext, cachedProtoCanBeCached));
            Assert(cachedProtoCanBeCached);
            Assert(type->GetIsShared());
#endif

#if DBG_DUMP
            TraceUseConstructorCache(constructorCache, constructor, true);
#endif
            Var object = DynamicObject::New(requestContext->GetRecycler(), type);
            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(object));
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
            {TRACE_IT(50851);
                object = DynamicObject::FromVar(JavascriptProxy::AutoProxyWrapper(object));
            }
#endif
            return object;
        }

        if (constructorCache->SkipDefaultNewObject())
        {TRACE_IT(50852);
            Assert(!constructorCache->NeedsUpdateAfterCtor());

#if DBG_DUMP
            TraceUseConstructorCache(constructorCache, constructor, true);
#endif
            if (isBaseClassConstructorNewScObject)
            {TRACE_IT(50853);
                return JavascriptOperators::CreateFromConstructor(function, requestContext);
            }

            return nullptr;
        }

#if DBG_DUMP
        TraceUseConstructorCache(constructorCache, constructor, false);
#endif

        ScriptContext* constructorScriptContext = function->GetScriptContext();
        Assert(!constructorScriptContext->GetThreadContext()->IsDisableImplicitException());
        // we shouldn't try to call the constructor if it's closed already.
        constructorScriptContext->VerifyAlive(TRUE, requestContext);

        FunctionInfo::Attributes attributes = functionInfo->GetAttributes();
        if (attributes & FunctionInfo::ErrorOnNew)
        {TRACE_IT(50854);
            JavascriptError::ThrowTypeError(requestContext, JSERR_ErrorOnNew);
        }

        // Slow path
        FunctionProxy * ctorProxy = constructor->GetFunctionProxy();
        FunctionBody * functionBody = ctorProxy != nullptr ? ctorProxy->EnsureDeserialized()->Parse() : nullptr;

        if (attributes & FunctionInfo::SkipDefaultNewObject)
        {TRACE_IT(50855);
            // The constructor doesn't use the default new object.
#pragma prefast(suppress:6236, "DevDiv bug 830883. False positive when PHASE_OFF is #defined as '(false)'.")
            if (!PHASE_OFF1(ConstructorCachePhase) && (functionBody == nullptr || !PHASE_OFF(ConstructorCachePhase, functionBody)))
            {TRACE_IT(50856);
                constructorCache = constructor->EnsureValidConstructorCache();
                constructorCache->PopulateForSkipDefaultNewObject(constructorScriptContext);

#if DBG_DUMP
                if ((functionBody != nullptr && PHASE_TRACE(Js::ConstructorCachePhase, functionBody)) || (functionBody == nullptr && PHASE_TRACE1(Js::ConstructorCachePhase)))
                {TRACE_IT(50857);
                    const char16* ctorName = functionBody != nullptr ? functionBody->GetDisplayName() : _u("<unknown>");
                    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                    Output::Print(_u("CtorCache: populated cache (0x%p) for ctor %s (%s): "), constructorCache, ctorName,
                        functionBody ? functionBody->GetDebugNumberSet(debugStringBuffer) : _u("(null)"));
                    constructorCache->Dump();
                    Output::Print(_u("\n"));
                    Output::Flush();
                }
#endif

            }

            Assert(!constructorCache->NeedsUpdateAfterCtor());
            return nullptr;
        }

        // CONSIDER: Create some form of PatchGetProtoObjForCtorCache, which actually caches the prototype object in the constructor cache.
        // Make sure that it does NOT populate the guard field.  On the slow path (the only path for cross-context calls) we can do a faster lookup
        // after we fail the guard check.  When invalidating the cache for proto change, make sure we zap the prototype field of the cache in
        // addition to the guard value.
        bool prototypeCanBeCached;
        RecyclableObject* prototype = JavascriptOperators::GetPrototypeObjectForConstructorCache(function, constructorScriptContext, prototypeCanBeCached);
        prototype = RecyclableObject::FromVar(CrossSite::MarshalVar(requestContext, prototype));

        DynamicObject* newObject = requestContext->GetLibrary()->CreateObject(prototype, 8);

        JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_OBJECT(newObject));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::autoProxyFlag))
        {TRACE_IT(50858);
            newObject = DynamicObject::FromVar(JavascriptProxy::AutoProxyWrapper(newObject));
        }
#endif

        Assert(newObject->GetTypeHandler()->GetPropertyCount() == 0);

        if (prototypeCanBeCached && functionBody != nullptr && requestContext == constructorScriptContext &&
            !Js::JavascriptProxy::Is(newObject) &&
            !PHASE_OFF1(ConstructorCachePhase) && !PHASE_OFF(ConstructorCachePhase, functionBody))
        {TRACE_IT(50859);
            DynamicType* newObjectType = newObject->GetDynamicType();
            // Initial type (without any properties) should always be shared up-front.  This allows us to populate the cache right away.
            Assert(newObjectType->GetIsShared());

            // Populate the cache here and set the updateAfterCtor flag.  This way, if the ctor is called recursively the
            // recursive calls will hit the cache and use the initial type.  On the unwind path, we will update the cache
            // after the innermost ctor and clear the flag.  After subsequent ctors we won't attempt an update anymore.
            // As long as the updateAfterCtor flag is set it is safe to update the cache, because it would not have been
            // hard-coded in the JIT-ed code.
            constructorCache = constructor->EnsureValidConstructorCache();
            constructorCache->Populate(newObjectType, constructorScriptContext, functionBody->GetHasNoExplicitReturnValue(), true);
            Assert(constructorCache->IsConsistent());

#if DBG_DUMP
            if ((functionBody != nullptr && PHASE_TRACE(Js::ConstructorCachePhase, functionBody)) || (functionBody == nullptr && PHASE_TRACE1(Js::ConstructorCachePhase)))
            {TRACE_IT(50860);
                const char16* ctorName = functionBody != nullptr ? functionBody->GetDisplayName() : _u("<unknown>");
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                Output::Print(_u("CtorCache: populated cache (0x%p) for ctor %s (%s): "), constructorCache, ctorName,
                    functionBody ? functionBody->GetDebugNumberSet(debugStringBuffer) : _u("(null)"));
                constructorCache->Dump();
                Output::Print(_u("\n"));
                Output::Flush();
            }
#endif
        }
        else
        {TRACE_IT(50861);
#if DBG_DUMP
            if ((functionBody != nullptr && PHASE_TRACE(Js::ConstructorCachePhase, functionBody)) || (functionBody == nullptr && PHASE_TRACE1(Js::ConstructorCachePhase)))
            {TRACE_IT(50862);
                const char16* ctorName = functionBody != nullptr ? functionBody->GetDisplayName() : _u("<unknown>");
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                Output::Print(_u("CtorCache: did not populate cache (0x%p) for ctor %s (%s), because %s: prototype = 0x%p, functionBody = 0x%p, ctor context = 0x%p, request context = 0x%p"),
                    constructorCache, ctorName, functionBody ? functionBody->GetDebugNumberSet(debugStringBuffer) : _u("(null)"),
                    !prototypeCanBeCached ? _u("prototype cannot be cached") :
                    functionBody == nullptr ? _u("function has no body") :
                    requestContext != constructorScriptContext ? _u("of cross-context call") : _u("constructor cache phase is off"),
                    prototype, functionBody, constructorScriptContext, requestContext);
                Output::Print(_u("\n"));
                Output::Flush();
            }
#endif
        }

        return newObject;
    }

    void JavascriptOperators::UpdateNewScObjectCache(Var function, Var instance, ScriptContext* requestContext)
    {TRACE_IT(50863);
        JavascriptFunction* constructor = JavascriptFunction::FromVar(function);
        if(constructor->GetScriptContext() != requestContext)
        {TRACE_IT(50864);
            // The cache is populated only when the constructor function's context is the same as the calling context. However,
            // the cached type is not finalized yet and may not be until multiple calls to the constructor have been made (see
            // flag ConstructorCallsRequiredToFinalizeCachedType). A subsequent call to the constructor may be made from a
            // different context, so ignore those cross-context calls and wait for the constructor to be called from its own
            // context again to finalize the cached type.
            return;
        }

        // Review : What happens if the cache got invalidated between NewScObject and here?
        // Should we allocate new?  Should we mark it as polymorphic?
        ConstructorCache* constructorCache = constructor->GetConstructorCache();
        Assert(constructorCache->IsConsistent());
        Assert(!ConstructorCache::IsDefault(constructorCache));
        AssertMsg(constructorCache->GetScriptContext() == constructor->GetScriptContext(), "Why did we populate a constructor cache with a mismatched script context?");
        AssertMsg(constructorCache->IsPopulated(), "Why are we updating a constructor cache that hasn't been populated?");

        // The presence of the updateAfterCtor flag guarantees that this cache hasn't been used in JIT-ed fast path.  Even, if the
        // cache is invalidated, this flag is not changed.
        AssertMsg(constructorCache->NeedsUpdateAfterCtor(), "Why are we updating a constructor cache that doesn't need to be updated?");

        const bool finalizeCachedType =
            constructorCache->CallCount() >= CONFIG_FLAG(ConstructorCallsRequiredToFinalizeCachedType);
        if(!finalizeCachedType)
        {TRACE_IT(50865);
            constructorCache->IncCallCount();
        }
        else
        {TRACE_IT(50866);
            constructorCache->ClearUpdateAfterCtor();
        }

        FunctionBody* constructorBody = constructor->GetFunctionBody();
        AssertMsg(constructorBody != nullptr, "Constructor function doesn't have a function body.");
        Assert(RecyclableObject::Is(instance));

        // The cache might have been invalidated between NewScObjectCommon and UpdateNewScObjectCache.  This could occur, for example, if
        // the constructor updates its own prototype property.  If that happens we don't want to re-populate it here.  A new cache will
        // be created when the constructor is called again.
        if (constructorCache->IsInvalidated())
        {TRACE_IT(50867);
#if DBG_DUMP
            TraceUpdateConstructorCache(constructorCache, constructorBody, false, _u("because cache is invalidated"));
#endif
            return;
        }

        Assert(constructorCache->GetGuardValueAsType() != nullptr);

        if (DynamicType::Is(RecyclableObject::FromVar(instance)->GetTypeId()))
        {TRACE_IT(50868);
            DynamicObject *object = DynamicObject::FromVar(instance);
            DynamicType* type = object->GetDynamicType();
            DynamicTypeHandler* typeHandler = type->GetTypeHandler();

            if (constructorBody->GetHasOnlyThisStmts())
            {TRACE_IT(50869);
                if (typeHandler->IsSharable())
                {TRACE_IT(50870);
#if DBG
                    bool cachedProtoCanBeCached;
                    Assert(type->GetPrototype() == JavascriptOperators::GetPrototypeObjectForConstructorCache(constructor, requestContext, cachedProtoCanBeCached));
                    Assert(cachedProtoCanBeCached);
                    Assert(type->GetScriptContext() == constructorCache->GetScriptContext());
                    Assert(type->GetPrototype() == constructorCache->GetType()->GetPrototype());
#endif

                    typeHandler->SetMayBecomeShared();
                    // CONSIDER: Remove only this for delayed type sharing.
                    type->ShareType();

#if ENABLE_PROFILE_INFO
                    DynamicProfileInfo* profileInfo = constructorBody->HasDynamicProfileInfo() ? constructorBody->GetAnyDynamicProfileInfo() : nullptr;
                    if ((profileInfo != nullptr && profileInfo->GetImplicitCallFlags() <= ImplicitCall_None) ||
                        CheckIfPrototypeChainHasOnlyWritableDataProperties(type->GetPrototype()))
                    {TRACE_IT(50871);
                        Assert(typeHandler->GetPropertyCount() < Js::PropertyIndexRanges<PropertyIndex>::MaxValue);

                        for (PropertyIndex pi = 0; pi < typeHandler->GetPropertyCount(); pi++)
                        {TRACE_IT(50872);
                            requestContext->RegisterConstructorCache(typeHandler->GetPropertyId(requestContext, pi), constructorCache);
                        }

                        Assert(constructorBody->GetUtf8SourceInfo()->GetIsLibraryCode() || !constructor->GetScriptContext()->IsScriptContextInDebugMode());

                        if (constructorCache->TryUpdateAfterConstructor(type, constructor->GetScriptContext()))
                        {TRACE_IT(50873);
#if DBG_DUMP
                            TraceUpdateConstructorCache(constructorCache, constructorBody, true, _u(""));
#endif
                        }
                        else
                        {TRACE_IT(50874);
#if DBG_DUMP
                            TraceUpdateConstructorCache(constructorCache, constructorBody, false, _u("because number of slots > MaxCachedSlotCount"));
#endif
                        }
                    }
#if DBG_DUMP
                    else
                    {TRACE_IT(50875);
                        if (profileInfo &&
                            ((profileInfo->GetImplicitCallFlags() & ~(Js::ImplicitCall_External | Js::ImplicitCall_Accessor)) == 0) &&
                            profileInfo != nullptr && CheckIfPrototypeChainHasOnlyWritableDataProperties(type->GetPrototype()) &&
                            Js::Configuration::Global.flags.Trace.IsEnabled(Js::HostOptPhase))
                        {TRACE_IT(50876);
                            const char16* ctorName = constructorBody->GetDisplayName();
                            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                            Output::Print(_u("CtorCache: %s cache (0x%p) for ctor %s (#%u) did not update because external call"),
                                constructorCache, constructorBody, ctorName, constructorBody ? constructorBody->GetDebugNumberSet(debugStringBuffer) : _u("(null)"));
                            Output::Print(_u("\n"));
                            Output::Flush();
                        }
                    }
#endif
#endif
                }
                else
                {TRACE_IT(50877);
                    // Dynamic type created is not sharable.
                    // So in future don't try to check for "this assignment optimization".
                    constructorBody->SetHasOnlyThisStmts(false);
#if DBG_DUMP
                    TraceUpdateConstructorCache(constructorCache, constructorBody, false, _u("because final type is not shareable"));
#endif
                }
            }
            else
            {TRACE_IT(50878);
#if DBG_DUMP
                TraceUpdateConstructorCache(constructorCache, constructorBody, false, _u("because ctor has not only this statements"));
#endif
            }
        }
        else
        {TRACE_IT(50879);
            // Even though this constructor apparently returned something other than the default object we created,
            // it still makes sense to cache the parameters of the default object, since we must create it every time, anyway.
#if DBG_DUMP
            TraceUpdateConstructorCache(constructorCache, constructorBody, false, _u("because ctor return a non-object value"));
#endif
            return;
        }

        // Whatever the constructor returned, if we're caching a type we want to be sure we shrink its inline slot capacity.
        if (finalizeCachedType && constructorCache->IsEnabled())
        {TRACE_IT(50880);
            DynamicType* cachedType = constructorCache->NeedsTypeUpdate() ? constructorCache->GetPendingType() : constructorCache->GetType();
            DynamicTypeHandler* cachedTypeHandler = cachedType->GetTypeHandler();

            // Consider: We could delay inline slot capacity shrinking until the second time this constructor is invoked.  In some cases
            // this might permit more properties to remain inlined if the objects grow after constructor.  This would require flagging
            // the cache as special (already possible) and forcing the shrinking during work item creation if we happen to JIT this
            // constructor while the cache is in this special state.
            if (cachedTypeHandler->GetInlineSlotCapacity())
            {TRACE_IT(50881);
#if DBG_DUMP
                int inlineSlotCapacityBeforeShrink = cachedTypeHandler->GetInlineSlotCapacity();
#endif

                // Note that after the cache has been updated and might have been used in the JIT-ed code, it is no longer legal to
                // shrink the inline slot capacity of the type.  That's because we allocate memory for a fixed number of inlined properties
                // and if that number changed on the type, this update wouldn't get reflected in JIT-ed code and we would allocate objects
                // of a wrong size.  This could conceivably happen if the original object got collected, and with it some of the successor
                // types also.  If then another constructor has the same prototype and needs to populate its own cache, it would attempt to
                // shrink inlined slots again.  If all surviving type handlers have smaller inline slot capacity, we would shrink it further.
                // To address this problem the type handler has a bit indicating its inline slots have been shrunk already.  If that bit is
                // set ShrinkSlotAndInlineSlotCapacity does nothing.
                cachedTypeHandler->ShrinkSlotAndInlineSlotCapacity();
                constructorCache->UpdateInlineSlotCount();

#if DBG_DUMP
                Assert(inlineSlotCapacityBeforeShrink >= cachedTypeHandler->GetInlineSlotCapacity());
                if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlineSlotsPhase))
                {TRACE_IT(50882);
                    if (inlineSlotCapacityBeforeShrink != cachedTypeHandler->GetInlineSlotCapacity())
                    {TRACE_IT(50883);
                        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                        Output::Print(_u("Inline slot capacity shrunk: Function:%04s Before:%d After:%d\n"),
                            constructorBody->GetDebugNumberSet(debugStringBuffer), inlineSlotCapacityBeforeShrink, cachedTypeHandler->GetInlineSlotCapacity());
                    }
                }
#endif
            }
        }
    }

    void JavascriptOperators::TraceUseConstructorCache(const ConstructorCache* ctorCache, const JavascriptFunction* ctor, bool isHit)
    {TRACE_IT(50884);
#if DBG_DUMP
        // We are under debug, so we can incur the extra check here.
        FunctionProxy* ctorBody = ctor->GetFunctionProxy();
        if (ctorBody != nullptr && !ctorBody->GetScriptContext()->IsClosed())
        {TRACE_IT(50885);
            ctorBody = ctorBody->EnsureDeserialized();
        }
        if ((ctorBody != nullptr && PHASE_TRACE(Js::ConstructorCachePhase, ctorBody)) || (ctorBody == nullptr && PHASE_TRACE1(Js::ConstructorCachePhase)))
        {TRACE_IT(50886);
            const char16* ctorName = ctorBody != nullptr ? ctorBody->GetDisplayName() : _u("<unknown>");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

            Output::Print(_u("CtorCache: %s cache (0x%p) for ctor %s (%s): "), isHit ? _u("hit") : _u("missed"), ctorCache, ctorName,
                ctorBody ? ctorBody->GetDebugNumberSet(debugStringBuffer) : _u("(null)"));
            ctorCache->Dump();
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
    }

    void JavascriptOperators::TraceUpdateConstructorCache(const ConstructorCache* ctorCache, const FunctionBody* ctorBody, bool updated, const char16* reason)
    {TRACE_IT(50887);
#if DBG_DUMP
        if (PHASE_TRACE(Js::ConstructorCachePhase, ctorBody))
        {TRACE_IT(50888);
            const char16* ctorName = ctorBody->GetDisplayName();
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

            Output::Print(_u("CtorCache: %s cache (0x%p) for ctor %s (%s)%s %s: "),
                updated ? _u("updated") : _u("did not update"), ctorBody, ctorName,
                ctorBody ? const_cast<Js::FunctionBody *>(ctorBody)->GetDebugNumberSet(debugStringBuffer) : _u("(null)"),
                updated ? _u("") : _u(", because") , reason);
            ctorCache->Dump();
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
    }

    Var JavascriptOperators::NewScObject(const Var callee, const Arguments args, ScriptContext *const scriptContext, const Js::AuxArray<uint32> *spreadIndices)
    {TRACE_IT(50889);
        Assert(callee);
        Assert(args.Info.Count != 0);
        Assert(scriptContext);

        // Always save and restore implicit call flags when calling out
        // REVIEW: Can we avoid it if we don't collect dynamic profile info?
        ThreadContext *const threadContext = scriptContext->GetThreadContext();
        const ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();

        const Var newVarInstance = JavascriptFunction::CallAsConstructor(callee, /* overridingNewTarget = */nullptr, args, scriptContext, spreadIndices);

        threadContext->SetImplicitCallFlags(savedImplicitCallFlags);
        return newVarInstance;
    }

    Js::GlobalObject * JavascriptOperators::OP_LdRoot(ScriptContext* scriptContext)
    {TRACE_IT(50890);
        return scriptContext->GetGlobalObject();
    }

    Js::ModuleRoot * JavascriptOperators::GetModuleRoot(int moduleID, ScriptContext* scriptContext)
    {TRACE_IT(50891);
        Assert(moduleID != kmodGlobal);
        JavascriptLibrary* library = scriptContext->GetLibrary();
        HostObjectBase *hostObject = library->GetGlobalObject()->GetHostObject();
        if (hostObject)
        {TRACE_IT(50892);
            Js::ModuleRoot * moduleRoot = hostObject->GetModuleRoot(moduleID);
            Assert(!CrossSite::NeedMarshalVar(moduleRoot, scriptContext));
            return moduleRoot;
        }
        HostScriptContext *hostScriptContext = scriptContext->GetHostScriptContext();
        if (hostScriptContext)
        {TRACE_IT(50893);
            Js::ModuleRoot * moduleRoot = hostScriptContext->GetModuleRoot(moduleID);
            Assert(!CrossSite::NeedMarshalVar(moduleRoot, scriptContext));
            return moduleRoot;
        }
        Assert(FALSE);
        return nullptr;
    }

    Var JavascriptOperators::OP_LoadModuleRoot(int moduleID, ScriptContext* scriptContext)
    {TRACE_IT(50894);
        Js::ModuleRoot * moduleRoot = GetModuleRoot(moduleID, scriptContext);
        if (moduleRoot)
        {TRACE_IT(50895);
            return moduleRoot;
        }
        Assert(false);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptOperators::OP_LdNull(ScriptContext* scriptContext)
    {TRACE_IT(50896);
        return scriptContext->GetLibrary()->GetNull();
    }

    Var JavascriptOperators::OP_LdUndef(ScriptContext* scriptContext)
    {TRACE_IT(50897);
        return scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptOperators::OP_LdNaN(ScriptContext* scriptContext)
    {TRACE_IT(50898);
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptOperators::OP_LdInfinity(ScriptContext* scriptContext)
    {TRACE_IT(50899);
        return scriptContext->GetLibrary()->GetPositiveInfinite();
    }

    void JavascriptOperators::BuildHandlerScope(Var argThis, RecyclableObject * hostObject, FrameDisplay * pDisplay, ScriptContext * scriptContext)
    {TRACE_IT(50900);
        Assert(argThis != nullptr);
        pDisplay->SetItem(0, TaggedNumber::Is(argThis) ? scriptContext->GetLibrary()->CreateNumberObject(argThis) : argThis);
        uint16 i = 1;

        Var aChild = argThis;
        uint16 length = pDisplay->GetLength();

        // Now add any parent scopes
        // We need to support the namespace parent lookup in both fastDOM on and off scenario.
        while (aChild != NULL)
        {TRACE_IT(50901);
            Var aParent = hostObject->GetNamespaceParent(aChild);

            if (aParent == nullptr)
            {TRACE_IT(50902);
                break;
            }
            aParent = CrossSite::MarshalVar(scriptContext, aParent);
            if (i == length)
            {TRACE_IT(50903);
                length += 8;
                FrameDisplay * tmp = RecyclerNewPlus(scriptContext->GetRecycler(), length * sizeof(void*), FrameDisplay, length);
                js_memcpy_s((char*)tmp + tmp->GetOffsetOfScopes(), tmp->GetLength() * sizeof(void *), (char*)pDisplay + pDisplay->GetOffsetOfScopes(), pDisplay->GetLength() * sizeof(void*));
                pDisplay = tmp;
            }
            pDisplay->SetItem(i, aParent);
            aChild = aParent;
            i++;
        }

        Assert(i <= pDisplay->GetLength());
        pDisplay->SetLength(i);
    }
    FrameDisplay * JavascriptOperators::OP_LdHandlerScope(Var argThis, ScriptContext* scriptContext)
    {TRACE_IT(50904);

        // The idea here is to build a stack of nested scopes in the form of a JS array.
        //
        // The scope stack for an event handler looks like this:
        //
        // implicit "this"
        // implicit namespace parent scopes

        // Put the implicit "this"
        if (argThis != NULL)
        {TRACE_IT(50905);
            RecyclableObject* hostObject = scriptContext->GetGlobalObject()->GetHostObject();
            if (hostObject == nullptr)
            {TRACE_IT(50906);
                hostObject = scriptContext->GetGlobalObject()->GetDirectHostObject();
            }
            if (hostObject != nullptr)
            {TRACE_IT(50907);
                uint16 length = 7;
                FrameDisplay *pDisplay =
                    RecyclerNewPlus(scriptContext->GetRecycler(), length * sizeof(void*), FrameDisplay, length);
                BuildHandlerScope(argThis, hostObject, pDisplay, scriptContext);
                return pDisplay;
            }
        }

        return const_cast<FrameDisplay *>(&Js::NullFrameDisplay);
    }

    FrameDisplay* JavascriptOperators::OP_LdFrameDisplay(void *argHead, void *argEnv, ScriptContext* scriptContext)
    {TRACE_IT(50908);
        // Build a display of nested frame objects.
        // argHead is the current scope; argEnv is either the lone trailing scope or an array of scopes
        // which we append to the new display.

        // Note that there are cases in which a function with no local frame must construct a display to pass
        // to the function(s) nested within it. In such a case, argHead will be a null object, and it's not
        // strictly necessary to include it. But such cases are rare and not perf critical, so it's not
        // worth the extra complexity to notify the nested functions that they can "skip" this slot in the
        // frame display when they're loading scopes nested outside it.

        FrameDisplay *pDisplay = nullptr;
        FrameDisplay *envDisplay = (FrameDisplay*)argEnv;
        uint16 length = envDisplay->GetLength() + 1;

        pDisplay = RecyclerNewPlus(scriptContext->GetRecycler(), length * sizeof(void*), FrameDisplay, length);
        for (int j = 0; j < length - 1; j++)
        {TRACE_IT(50909);
            pDisplay->SetItem(j + 1, envDisplay->GetItem(j));
        }

        pDisplay->SetItem(0, argHead);

        return pDisplay;
    }

    FrameDisplay* JavascriptOperators::OP_LdFrameDisplayNoParent(void *argHead, ScriptContext* scriptContext)
    {TRACE_IT(50910);
        return OP_LdFrameDisplay(argHead, (void*)&NullFrameDisplay, scriptContext);
    }

    FrameDisplay* JavascriptOperators::OP_LdStrictFrameDisplay(void *argHead, void *argEnv, ScriptContext* scriptContext)
    {TRACE_IT(50911);
        FrameDisplay * pDisplay = OP_LdFrameDisplay(argHead, argEnv, scriptContext);
        pDisplay->SetStrictMode(true);

        return pDisplay;
    }

    FrameDisplay* JavascriptOperators::OP_LdStrictFrameDisplayNoParent(void *argHead, ScriptContext* scriptContext)
    {TRACE_IT(50912);
        return OP_LdStrictFrameDisplay(argHead, (void*)&StrictNullFrameDisplay, scriptContext);
    }

    FrameDisplay* JavascriptOperators::OP_LdInnerFrameDisplay(void *argHead, void *argEnv, ScriptContext* scriptContext)
    {TRACE_IT(50913);
        CheckInnerFrameDisplayArgument(argHead);
        return OP_LdFrameDisplay(argHead, argEnv, scriptContext);
    }

    FrameDisplay* JavascriptOperators::OP_LdInnerFrameDisplayNoParent(void *argHead, ScriptContext* scriptContext)
    {TRACE_IT(50914);
        CheckInnerFrameDisplayArgument(argHead);
        return OP_LdFrameDisplayNoParent(argHead, scriptContext);
    }

    FrameDisplay* JavascriptOperators::OP_LdStrictInnerFrameDisplay(void *argHead, void *argEnv, ScriptContext* scriptContext)
    {TRACE_IT(50915);
        CheckInnerFrameDisplayArgument(argHead);
        return OP_LdStrictFrameDisplay(argHead, argEnv, scriptContext);
    }

    FrameDisplay* JavascriptOperators::OP_LdStrictInnerFrameDisplayNoParent(void *argHead, ScriptContext* scriptContext)
    {TRACE_IT(50916);
        CheckInnerFrameDisplayArgument(argHead);
        return OP_LdStrictFrameDisplayNoParent(argHead, scriptContext);
    }

    void JavascriptOperators::CheckInnerFrameDisplayArgument(void *argHead)
    {TRACE_IT(50917);
        if (ThreadContext::IsOnStack(argHead))
        {
            AssertMsg(false, "Illegal byte code: stack object as with scope");
            Js::Throw::FatalInternalError();
        }
        if (!RecyclableObject::Is(argHead))
        {
            AssertMsg(false, "Illegal byte code: non-object as with scope");
            Js::Throw::FatalInternalError();
        }
    }

    Js::PropertyId JavascriptOperators::GetPropertyId(Var propertyName, ScriptContext* scriptContext)
    {TRACE_IT(50918);
        PropertyRecord const * propertyRecord = nullptr;
        if (JavascriptSymbol::Is(propertyName))
        {TRACE_IT(50919);
            propertyRecord = JavascriptSymbol::FromVar(propertyName)->GetValue();
        }
        else if (JavascriptSymbolObject::Is(propertyName))
        {TRACE_IT(50920);
            propertyRecord = JavascriptSymbolObject::FromVar(propertyName)->GetValue();
        }
        else
        {TRACE_IT(50921);
            JavascriptString * indexStr = JavascriptConversion::ToString(propertyName, scriptContext);
            scriptContext->GetOrAddPropertyRecord(indexStr->GetString(), indexStr->GetLength(), &propertyRecord);
        }

        return propertyRecord->GetPropertyId();
    }

    void JavascriptOperators::OP_InitSetter(Var object, PropertyId propertyId, Var setter)
    {TRACE_IT(50922);
        AssertMsg(!TaggedNumber::Is(object), "SetMember on a non-object?");
        RecyclableObject::FromVar(object)->SetAccessors(propertyId, nullptr, setter);
    }

    void JavascriptOperators::OP_InitClassMemberSet(Var object, PropertyId propertyId, Var setter)
    {TRACE_IT(50923);
        JavascriptOperators::OP_InitSetter(object, propertyId, setter);

        RecyclableObject::FromVar(object)->SetAttributes(propertyId, PropertyClassMemberDefaults);
    }

    Js::PropertyId JavascriptOperators::OP_InitElemSetter(Var object, Var elementName, Var setter, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50924);
        AssertMsg(!TaggedNumber::Is(object), "SetMember on a non-object?");

        PropertyId propertyId = JavascriptOperators::GetPropertyId(elementName, scriptContext);

        RecyclableObject::FromVar(object)->SetAccessors(propertyId, nullptr, setter);

        return propertyId;
    }

    Field(Var)* JavascriptOperators::OP_GetModuleExportSlotArrayAddress(uint moduleIndex, uint slotIndex, ScriptContextInfo* scriptContext)
    {TRACE_IT(50925);
        return scriptContext->GetModuleExportSlotArrayAddress(moduleIndex, slotIndex);
    }

    Field(Var)* JavascriptOperators::OP_GetModuleExportSlotAddress(uint moduleIndex, uint slotIndex, ScriptContext* scriptContext)
    {TRACE_IT(50926);
        Field(Var)* moduleRecordSlots = OP_GetModuleExportSlotArrayAddress(moduleIndex, slotIndex, scriptContext);
        Assert(moduleRecordSlots != nullptr);

        return &moduleRecordSlots[slotIndex];
    }

    Var JavascriptOperators::OP_LdModuleSlot(uint moduleIndex, uint slotIndex, ScriptContext* scriptContext)
    {TRACE_IT(50927);
        Field(Var)* addr = OP_GetModuleExportSlotAddress(moduleIndex, slotIndex, scriptContext);

        Assert(addr != nullptr);

        return *addr;
    }

    void JavascriptOperators::OP_StModuleSlot(uint moduleIndex, uint slotIndex, Var value, ScriptContext* scriptContext)
    {TRACE_IT(50928);
        Assert(value != nullptr);

        Field(Var)* addr = OP_GetModuleExportSlotAddress(moduleIndex, slotIndex, scriptContext);

        Assert(addr != nullptr);

        *addr = value;
    }

    void JavascriptOperators::OP_InitClassMemberSetComputedName(Var object, Var elementName, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50929);
        Js::PropertyId propertyId = JavascriptOperators::OP_InitElemSetter(object, elementName, value, scriptContext);
        RecyclableObject* instance = RecyclableObject::FromVar(object);

        // instance will be a function if it is the class constructor (otherwise it would be an object)
        if (JavascriptFunction::Is(instance) && Js::PropertyIds::prototype == propertyId)
        {TRACE_IT(50930);
            // It is a TypeError to have a static member with a computed name that evaluates to 'prototype'
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassStaticMethodCannotBePrototype);
        }

        instance->SetAttributes(propertyId, PropertyClassMemberDefaults);
    }

    BOOL JavascriptOperators::IsClassConstructor(Var instance)
    {TRACE_IT(50931);
        return JavascriptFunction::Is(instance) && (JavascriptFunction::FromVar(instance)->GetFunctionInfo()->IsClassConstructor() || !JavascriptFunction::FromVar(instance)->IsScriptFunction());
    }

    BOOL JavascriptOperators::IsBaseConstructorKind(Var instance)
    {TRACE_IT(50932);
        return JavascriptFunction::Is(instance) && (JavascriptFunction::FromVar(instance)->GetFunctionInfo()->GetBaseConstructorKind());
    }

    void JavascriptOperators::OP_InitGetter(Var object, PropertyId propertyId, Var getter)
    {TRACE_IT(50933);
        AssertMsg(!TaggedNumber::Is(object), "GetMember on a non-object?");
        RecyclableObject::FromVar(object)->SetAccessors(propertyId, getter, nullptr);
    }

    void JavascriptOperators::OP_InitClassMemberGet(Var object, PropertyId propertyId, Var getter)
    {TRACE_IT(50934);
        JavascriptOperators::OP_InitGetter(object, propertyId, getter);

        RecyclableObject::FromVar(object)->SetAttributes(propertyId, PropertyClassMemberDefaults);
    }

    Js::PropertyId JavascriptOperators::OP_InitElemGetter(Var object, Var elementName, Var getter, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50935);
        AssertMsg(!TaggedNumber::Is(object), "GetMember on a non-object?");

        PropertyId propertyId = JavascriptOperators::GetPropertyId(elementName, scriptContext);

        RecyclableObject::FromVar(object)->SetAccessors(propertyId, getter, nullptr);

        return propertyId;
    }

    void JavascriptOperators::OP_InitClassMemberGetComputedName(Var object, Var elementName, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50936);
        Js::PropertyId propertyId = JavascriptOperators::OP_InitElemGetter(object, elementName, value, scriptContext);
        RecyclableObject* instance = RecyclableObject::FromVar(object);

        // instance will be a function if it is the class constructor (otherwise it would be an object)
        if (JavascriptFunction::Is(instance) && Js::PropertyIds::prototype == propertyId)
        {TRACE_IT(50937);
            // It is a TypeError to have a static member with a computed name that evaluates to 'prototype'
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassStaticMethodCannotBePrototype);
        }

        instance->SetAttributes(propertyId, PropertyClassMemberDefaults);
    }

    void JavascriptOperators::OP_InitComputedProperty(Var object, Var elementName, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50938);
        PropertyId propertyId = JavascriptOperators::GetPropertyId(elementName, scriptContext);

        RecyclableObject::FromVar(object)->InitProperty(propertyId, value, flags);
    }

    void JavascriptOperators::OP_InitClassMemberComputedName(Var object, Var elementName, Var value, ScriptContext* scriptContext, PropertyOperationFlags flags)
    {TRACE_IT(50939);
        PropertyId propertyId = JavascriptOperators::GetPropertyId(elementName, scriptContext);
        RecyclableObject* instance = RecyclableObject::FromVar(object);

        // instance will be a function if it is the class constructor (otherwise it would be an object)
        if (JavascriptFunction::Is(instance) && Js::PropertyIds::prototype == propertyId)
        {TRACE_IT(50940);
            // It is a TypeError to have a static member with a computed name that evaluates to 'prototype'
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassStaticMethodCannotBePrototype);
        }

        instance->SetPropertyWithAttributes(propertyId, value, PropertyClassMemberDefaults, NULL, flags);
    }

    //
    // Used by object literal {..., __proto__: ..., }.
    //
    void JavascriptOperators::OP_InitProto(Var instance, PropertyId propertyId, Var value)
    {TRACE_IT(50941);
        AssertMsg(RecyclableObject::Is(instance), "__proto__ member on a non-object?");
        Assert(propertyId == PropertyIds::__proto__);

        RecyclableObject* object = RecyclableObject::FromVar(instance);
        ScriptContext* scriptContext = object->GetScriptContext();

        // B.3.1    __proto___ Property Names in Object Initializers
        //6.If propKey is the string value "__proto__" and if isComputedPropertyName(propKey) is false, then
        //    a.If Type(v) is either Object or Null, then
        //        i.Return the result of calling the [[SetInheritance]] internal method of object with argument propValue.
        //    b.Return NormalCompletion(empty).
        if (JavascriptOperators::IsObjectOrNull(value))
        {TRACE_IT(50942);
            JavascriptObject::ChangePrototype(object, RecyclableObject::FromVar(value), /*validate*/false, scriptContext);
        }
    }

    Var JavascriptOperators::ConvertToUnmappedArguments(HeapArgumentsObject *argumentsObject,
        uint32 paramCount,
        Var *paramAddr,
        DynamicObject* frameObject,
        Js::PropertyIdArray *propIds,
        uint32 formalsCount,
        ScriptContext* scriptContext)
    {TRACE_IT(50943);
        Var *paramIter = paramAddr;
        uint32 i = 0;

        for (paramIter = paramAddr + i; i < paramCount; i++, paramIter++)
        {TRACE_IT(50944);
            JavascriptOperators::SetItem(argumentsObject, argumentsObject, i, *paramIter, scriptContext, PropertyOperation_None, /* skipPrototypeCheck = */ TRUE);
        }

        argumentsObject = argumentsObject->ConvertToUnmappedArgumentsObject();

        // Now as the unmapping is done we need to fill those frame object with Undecl
        for (i = 0; i < formalsCount; i++)
        {TRACE_IT(50945);
            frameObject->SetSlot(SetSlotArguments(propIds != nullptr ? propIds->elements[i] : Js::Constants::NoProperty, i, scriptContext->GetLibrary()->GetUndeclBlockVar()));
        }

        return argumentsObject;
    }

    Var JavascriptOperators::LoadHeapArguments(JavascriptFunction *funcCallee, uint32 actualsCount, Var *paramAddr, Var frameObj, Var vArray, ScriptContext* scriptContext, bool nonSimpleParamList)
    {TRACE_IT(50946);
        AssertMsg(actualsCount != (unsigned int)-1, "Loading the arguments object in the global function?");

        // Create and initialize the Arguments object.

        uint32 formalsCount = 0;
        Js::PropertyIdArray *propIds = nullptr;
        if (vArray != scriptContext->GetLibrary()->GetNull())
        {TRACE_IT(50947);
            propIds = (Js::PropertyIdArray *)vArray;
            formalsCount = propIds->count;
            Assert(formalsCount != 0 && propIds != nullptr);
        }

        HeapArgumentsObject *argsObj = JavascriptOperators::CreateHeapArguments(funcCallee, actualsCount, formalsCount, frameObj, scriptContext);
        return FillScopeObject(funcCallee, actualsCount, formalsCount, frameObj, paramAddr, propIds, argsObj, scriptContext, nonSimpleParamList, false);
    }

    Var JavascriptOperators::LoadHeapArgsCached(JavascriptFunction *funcCallee, uint32 actualsCount, uint32 formalsCount, Var *paramAddr, Var frameObj, ScriptContext* scriptContext, bool nonSimpleParamList)
    {TRACE_IT(50948);
        // Disregard the "this" param.
        AssertMsg(actualsCount != (uint32)-1 && formalsCount != (uint32)-1,
                  "Loading the arguments object in the global function?");

        HeapArgumentsObject *argsObj = JavascriptOperators::CreateHeapArguments(funcCallee, actualsCount, formalsCount, frameObj, scriptContext);

        return FillScopeObject(funcCallee, actualsCount, formalsCount, frameObj, paramAddr, nullptr, argsObj, scriptContext, nonSimpleParamList, true);
    }

    Var JavascriptOperators::FillScopeObject(JavascriptFunction *funcCallee, uint32 actualsCount, uint32 formalsCount, Var frameObj, Var * paramAddr,
        Js::PropertyIdArray *propIds, HeapArgumentsObject * argsObj, ScriptContext * scriptContext, bool nonSimpleParamList, bool useCachedScope)
    {TRACE_IT(50949);
        Assert(frameObj);

        // Transfer formal arguments (that were actually passed) from their ArgIn slots to the local frame object.
        uint32 i;

        Var *tmpAddr = paramAddr;

        if (formalsCount != 0)
        {TRACE_IT(50950);
            DynamicObject* frameObject = nullptr;
            if (useCachedScope)
            {TRACE_IT(50951);
                frameObject = DynamicObject::FromVar(frameObj);
                __analysis_assume((uint32)frameObject->GetDynamicType()->GetTypeHandler()->GetSlotCapacity() >= formalsCount);
            }
            else
            {TRACE_IT(50952);
                frameObject = (DynamicObject*)frameObj;
                // No fixed fields for formal parameters of the arguments object.  Also, mark all fields as initialized up-front, because
                // we will set them directly using SetSlot below, so the type handler will not have a chance to mark them as initialized later.
                // CONSIDER : When we delay type sharing until the second instance is created, pass an argument indicating we want the types
                // and handlers created here to be marked as shared up-front. This is to ensure we don't get any fixed fields and that the handler
                // is ready for storing values directly to slots.
                DynamicType* newType = PathTypeHandlerBase::CreateNewScopeObject(scriptContext, frameObject->GetDynamicType(), propIds, nonSimpleParamList ? PropertyLetDefaults : PropertyNone);
                int oldSlotCapacity = frameObject->GetDynamicType()->GetTypeHandler()->GetSlotCapacity();
                int newSlotCapacity = newType->GetTypeHandler()->GetSlotCapacity();
                __analysis_assume((uint32)newSlotCapacity >= formalsCount);

                frameObject->EnsureSlots(oldSlotCapacity, newSlotCapacity, scriptContext, newType->GetTypeHandler());
                frameObject->ReplaceType(newType);
            }

            if (argsObj && nonSimpleParamList)
            {TRACE_IT(50953);
                return ConvertToUnmappedArguments(argsObj, actualsCount, paramAddr, frameObject, propIds, formalsCount, scriptContext);
            }

            for (i = 0; i < formalsCount && i < actualsCount; i++, tmpAddr++)
            {TRACE_IT(50954);
                frameObject->SetSlot(SetSlotArguments(propIds != nullptr? propIds->elements[i] : Constants::NoProperty, i, *tmpAddr));
            }

            if (i < formalsCount)
            {TRACE_IT(50955);
                // The formals that weren't passed still need to be put in the frame object so that
                // their names will be found. Initialize them to "undefined".
                for (; i < formalsCount; i++)
                {TRACE_IT(50956);
                    frameObject->SetSlot(SetSlotArguments(propIds != nullptr? propIds->elements[i] : Constants::NoProperty, i, scriptContext->GetLibrary()->GetUndefined()));
                }
            }
        }

        if (argsObj != nullptr)
        {TRACE_IT(50957);
            // Transfer the unnamed actual arguments, if any, to the Arguments object itself.
            for (i = formalsCount, tmpAddr = paramAddr + i; i < actualsCount; i++, tmpAddr++)
            {TRACE_IT(50958);
                // ES5 10.6.11: use [[DefineOwnProperty]] semantics (instead of [[Put]]):
                // do not check whether property is non-writable/etc in the prototype.
                // ES3 semantics is same.
                JavascriptOperators::SetItem(argsObj, argsObj, i, *tmpAddr, scriptContext, PropertyOperation_None, /* skipPrototypeCheck = */ TRUE);
            }

            if (funcCallee->IsStrictMode())
            {TRACE_IT(50959);
                // If the formals are let decls, then we just overwrote the frame object slots with
                // Undecl sentinels, and we can use the original arguments that were passed to the HeapArgumentsObject.
                return argsObj->ConvertToUnmappedArgumentsObject(!nonSimpleParamList);
            }
        }
        return argsObj;
    }

    HeapArgumentsObject *JavascriptOperators::CreateHeapArguments(JavascriptFunction *funcCallee, uint32 actualsCount, uint32 formalsCount, Var frameObj, ScriptContext* scriptContext)
    {TRACE_IT(50960);
        JavascriptLibrary *library = scriptContext->GetLibrary();
        HeapArgumentsObject *argsObj = library->CreateHeapArguments(frameObj, formalsCount, !!funcCallee->IsStrictMode());

        //
        // Set the number of arguments of Arguments Object
        //
        argsObj->SetNumberOfArguments(actualsCount);

        JavascriptOperators::SetProperty(argsObj, argsObj, PropertyIds::length, JavascriptNumber::ToVar(actualsCount, scriptContext), scriptContext);
        JavascriptOperators::SetProperty(argsObj, argsObj, PropertyIds::_symbolIterator, library->GetArrayPrototypeValuesFunction(), scriptContext);
        if (funcCallee->IsStrictMode())
        {TRACE_IT(50961);
            JavascriptFunction* restrictedPropertyAccessor = library->GetThrowTypeErrorRestrictedPropertyAccessorFunction();
            argsObj->SetAccessors(PropertyIds::caller, restrictedPropertyAccessor, restrictedPropertyAccessor, PropertyOperation_NonFixedValue);

            argsObj->SetAccessors(PropertyIds::callee, restrictedPropertyAccessor, restrictedPropertyAccessor, PropertyOperation_NonFixedValue);

        }
        else
        {TRACE_IT(50962);
            JavascriptOperators::SetProperty(argsObj, argsObj, PropertyIds::callee,
                StackScriptFunction::EnsureBoxed(BOX_PARAM(funcCallee, nullptr, _u("callee"))), scriptContext);
        }

        return argsObj;
    }

    Var JavascriptOperators::OP_NewScopeObject(ScriptContext* scriptContext)
    {TRACE_IT(50963);
        return scriptContext->GetLibrary()->CreateActivationObject();
    }

    Var JavascriptOperators::OP_NewScopeObjectWithFormals(ScriptContext* scriptContext, JavascriptFunction * funcCallee, bool nonSimpleParamList)
    {TRACE_IT(50964);
        Js::ActivationObject * frameObject = (ActivationObject*)OP_NewScopeObject(scriptContext);
        // No fixed fields for formal parameters of the arguments object.  Also, mark all fields as initialized up-front, because
        // we will set them directly using SetSlot below, so the type handler will not have a chance to mark them as initialized later.
        // CONSIDER : When we delay type sharing until the second instance is created, pass an argument indicating we want the types
        // and handlers created here to be marked as shared up-front. This is to ensure we don't get any fixed fields and that the handler
        // is ready for storing values directly to slots.
        DynamicType* newType = PathTypeHandlerBase::CreateNewScopeObject(scriptContext, frameObject->GetDynamicType(), funcCallee->GetFunctionBody()->GetFormalsPropIdArray(), nonSimpleParamList ? PropertyLetDefaults : PropertyNone);

        int oldSlotCapacity = frameObject->GetDynamicType()->GetTypeHandler()->GetSlotCapacity();
        int newSlotCapacity = newType->GetTypeHandler()->GetSlotCapacity();

        frameObject->EnsureSlots(oldSlotCapacity, newSlotCapacity, scriptContext, newType->GetTypeHandler());
        frameObject->ReplaceType(newType);

        return frameObject;
    }

    Field(Var)* JavascriptOperators::OP_NewScopeSlots(unsigned int size, ScriptContext *scriptContext, Var scope)
    {TRACE_IT(50965);
        Assert(size > ScopeSlots::FirstSlotIndex); // Should never see empty slot array
        Field(Var)* slotArray = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), size); // last initialized slot contains reference to array of propertyIds, correspondent to objects in previous slots
        uint count = size - ScopeSlots::FirstSlotIndex;
        ScopeSlots slots((Js::Var*)slotArray);
        slots.SetCount(count);
        AssertMsg(!FunctionBody::Is(scope), "Scope should only be FunctionInfo or DebuggerScope, not FunctionBody");
        slots.SetScopeMetadata(scope);

        Var undef = scriptContext->GetLibrary()->GetUndefined();
        for (unsigned int i = 0; i < count; i++)
        {TRACE_IT(50966);
            slots.Set(i, undef);
        }

        return slotArray;
    }

    Field(Var)* JavascriptOperators::OP_NewScopeSlotsWithoutPropIds(unsigned int count, int scopeIndex, ScriptContext *scriptContext, FunctionBody *functionBody)
    {TRACE_IT(50967);
        DebuggerScope* scope = reinterpret_cast<DebuggerScope*>(Constants::FunctionBodyUnavailable);
        if (scopeIndex != DebuggerScope::InvalidScopeIndex)
        {TRACE_IT(50968);
            AssertMsg(functionBody->GetScopeObjectChain(), "A scope chain should always be created when there are new scope slots for blocks.");
            scope = functionBody->GetScopeObjectChain()->pScopeChain->Item(scopeIndex);
        }
        return OP_NewScopeSlots(count, scriptContext, scope);
    }

    Field(Var)* JavascriptOperators::OP_CloneScopeSlots(Field(Var) *slotArray, ScriptContext *scriptContext)
    {TRACE_IT(50969);
        ScopeSlots slots((Js::Var*)slotArray);
        uint size = ScopeSlots::FirstSlotIndex + slots.GetCount();

        Field(Var)* slotArrayClone = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), size);
        CopyArray(slotArrayClone, size, slotArray, size);

        return slotArrayClone;
    }

    Var JavascriptOperators::OP_NewPseudoScope(ScriptContext *scriptContext)
    {TRACE_IT(50970);
        return scriptContext->GetLibrary()->CreatePseudoActivationObject();
    }

    Var JavascriptOperators::OP_NewBlockScope(ScriptContext *scriptContext)
    {TRACE_IT(50971);
        return scriptContext->GetLibrary()->CreateBlockActivationObject();
    }

    Var JavascriptOperators::OP_CloneBlockScope(BlockActivationObject *blockScope, ScriptContext *scriptContext)
    {TRACE_IT(50972);
        return blockScope->Clone(scriptContext);
    }

    Var JavascriptOperators::OP_IsInst(Var instance, Var aClass, ScriptContext* scriptContext, IsInstInlineCache* inlineCache)
    {TRACE_IT(50973);
        if (!RecyclableObject::Is(aClass))
        {TRACE_IT(50974);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedFunction, _u("instanceof"));
        }

        RecyclableObject* constructor = RecyclableObject::FromVar(aClass);
        if (scriptContext->GetConfig()->IsES6HasInstanceEnabled())
        {TRACE_IT(50975);
            Var instOfHandler = JavascriptOperators::GetProperty(constructor, PropertyIds::_symbolHasInstance, scriptContext);
            if (JavascriptOperators::IsUndefinedObject(instOfHandler)
                || instOfHandler == scriptContext->GetBuiltInLibraryFunction(JavascriptFunction::EntryInfo::SymbolHasInstance.GetOriginalEntryPoint()))
            {TRACE_IT(50976);
                return JavascriptBoolean::ToVar(constructor->HasInstance(instance, scriptContext, inlineCache), scriptContext);
            }
            else
            {TRACE_IT(50977);
                if (!JavascriptConversion::IsCallable(instOfHandler))
                {TRACE_IT(50978);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, _u("Symbol[Symbol.hasInstance]"));
                }

                ThreadContext * threadContext = scriptContext->GetThreadContext();
                RecyclableObject *instFunc = RecyclableObject::FromVar(instOfHandler);
                Var result = threadContext->ExecuteImplicitCall(instFunc, ImplicitCall_Accessor, [=]()->Js::Var
                {
                    return CALL_FUNCTION(instFunc, CallInfo(CallFlags_Value, 2), constructor, instance);
                });

                return  JavascriptBoolean::ToVar(JavascriptConversion::ToBoolean(result, scriptContext) ? TRUE : FALSE, scriptContext);
            }
        }
        else
        {TRACE_IT(50979);
            return JavascriptBoolean::ToVar(constructor->HasInstance(instance, scriptContext, inlineCache), scriptContext);
        }
    }

    void JavascriptOperators::OP_InitClass(Var constructor, Var extends, ScriptContext * scriptContext)
    {TRACE_IT(50980);
        if (JavascriptOperators::GetTypeId(constructor) != Js::TypeId::TypeIds_Function)
        {TRACE_IT(50981);
             JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedFunction, _u("class"));
        }

        RecyclableObject * ctor = RecyclableObject::FromVar(constructor);

        // This is a circular reference to the constructor, it associate the constructor with the class and also allows us to check if a
        // function is a constructor by comparing the homeObj to the this pointer. see ScriptFunction::IsClassConstructor() for implementation
        JavascriptOperators::OP_SetHomeObj(constructor, constructor);

        if (extends)
        {TRACE_IT(50982);
            switch (JavascriptOperators::GetTypeId(extends))
            {
                case Js::TypeId::TypeIds_Null:
                {TRACE_IT(50983);
                    Var ctorProto = JavascriptOperators::GetProperty(constructor, ctor, Js::PropertyIds::prototype, scriptContext);
                    RecyclableObject * ctorProtoObj = RecyclableObject::FromVar(ctorProto);

                    ctorProtoObj->SetPrototype(RecyclableObject::FromVar(extends));

                    ctorProtoObj->EnsureProperty(Js::PropertyIds::constructor);
                    ctorProtoObj->SetEnumerable(Js::PropertyIds::constructor, FALSE);

                    if (ScriptFunctionBase::Is(constructor))
                    {TRACE_IT(50984);
                        ScriptFunctionBase::FromVar(constructor)->GetFunctionInfo()->SetBaseConstructorKind();
                    }

                    break;
                }

                default:
                {
                    if (!RecyclableObject::Is(extends))
                    {TRACE_IT(50985);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidPrototype, _u("extends"));
                    }
                    RecyclableObject * extendsObj = RecyclableObject::FromVar(extends);
                    if (!JavascriptOperators::IsConstructor(extendsObj))
                    {TRACE_IT(50986);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_ErrorOnNew);
                    }
                    if (!extendsObj->HasProperty(Js::PropertyIds::prototype))
                    {TRACE_IT(50987);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidPrototype);
                    }

                    Var extendsProto = JavascriptOperators::GetProperty(extends, extendsObj, Js::PropertyIds::prototype, scriptContext);
                    uint extendsProtoTypeId = JavascriptOperators::GetTypeId(extendsProto);
                    if (extendsProtoTypeId <= Js::TypeId::TypeIds_LastJavascriptPrimitiveType && extendsProtoTypeId != Js::TypeId::TypeIds_Null)
                    {TRACE_IT(50988);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidPrototype);
                    }

                    Var ctorProto = JavascriptOperators::GetProperty(constructor, ctor, Js::PropertyIds::prototype, scriptContext);
                    RecyclableObject * ctorProtoObj = RecyclableObject::FromVar(ctorProto);

                    ctorProtoObj->SetPrototype(RecyclableObject::FromVar(extendsProto));

                    ctorProtoObj->EnsureProperty(Js::PropertyIds::constructor);
                    ctorProtoObj->SetEnumerable(Js::PropertyIds::constructor, FALSE);

                    Var protoCtor = JavascriptOperators::GetProperty(ctorProto, ctorProtoObj, Js::PropertyIds::constructor, scriptContext);
                    RecyclableObject * protoCtorObj = RecyclableObject::FromVar(protoCtor);
                    protoCtorObj->SetPrototype(extendsObj);

                    break;
                }
            }
        }
    }

    void JavascriptOperators::OP_LoadUndefinedToElement(Var instance, PropertyId propertyId)
    {TRACE_IT(50989);
        AssertMsg(!TaggedNumber::Is(instance), "Invalid scope/root object");
        JavascriptOperators::EnsureProperty(instance, propertyId);
    }

    void JavascriptOperators::OP_LoadUndefinedToElementScoped(FrameDisplay *pScope, PropertyId propertyId, Var defaultInstance, ScriptContext* scriptContext)
    {TRACE_IT(50990);
        int i;
        int length = pScope->GetLength();
        Var argInstance;
        for (i = 0; i < length; i++)
        {TRACE_IT(50991);
            argInstance = pScope->GetItem(i);
            if (JavascriptOperators::EnsureProperty(argInstance, propertyId))
            {TRACE_IT(50992);
                return;
            }
        }

        if (!JavascriptOperators::HasOwnPropertyNoHostObject(defaultInstance, propertyId))
        {TRACE_IT(50993);
            // CONSIDER : Consider adding pre-initialization support to activation objects.
            JavascriptOperators::OP_InitPropertyScoped(pScope, propertyId, scriptContext->GetLibrary()->GetUndefined(), defaultInstance, scriptContext);
        }
    }

    void JavascriptOperators::OP_LoadUndefinedToElementDynamic(Var instance, PropertyId propertyId, ScriptContext *scriptContext)
    {TRACE_IT(50994);
        if (!JavascriptOperators::HasOwnPropertyNoHostObject(instance, propertyId))
        {TRACE_IT(50995);
            RecyclableObject::FromVar(instance)->InitPropertyScoped(propertyId, scriptContext->GetLibrary()->GetUndefined());
        }
    }

    BOOL JavascriptOperators::EnsureProperty(Var instance, PropertyId propertyId)
    {TRACE_IT(50996);
        RecyclableObject *obj = RecyclableObject::FromVar(instance);
        return (obj && obj->EnsureProperty(propertyId));
    }

    void JavascriptOperators::OP_EnsureNoRootProperty(Var instance, PropertyId propertyId)
    {TRACE_IT(50997);
        Assert(RootObjectBase::Is(instance));
        RootObjectBase *obj = RootObjectBase::FromVar(instance);
        obj->EnsureNoProperty(propertyId);
    }

    void JavascriptOperators::OP_EnsureNoRootRedeclProperty(Var instance, PropertyId propertyId)
    {TRACE_IT(50998);
        Assert(RootObjectBase::Is(instance));
        RecyclableObject *obj = RecyclableObject::FromVar(instance);
        obj->EnsureNoRedeclProperty(propertyId);
    }

    void JavascriptOperators::OP_ScopedEnsureNoRedeclProperty(FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance)
    {TRACE_IT(50999);
        int i;
        int length = pDisplay->GetLength();
        RecyclableObject *object;

        for (i = 0; i < length; i++)
        {TRACE_IT(51000);
            object = RecyclableObject::FromVar(pDisplay->GetItem(i));
            if (object->EnsureNoRedeclProperty(propertyId))
            {TRACE_IT(51001);
                return;
            }
        }

        object = RecyclableObject::FromVar(defaultInstance);
        object->EnsureNoRedeclProperty(propertyId);
    }

    Var JavascriptOperators::IsIn(Var argProperty, Var instance, ScriptContext* scriptContext)
    {TRACE_IT(51002);
        // Note that the fact that we haven't seen a given name before doesn't mean that the instance doesn't

        if (!IsObject(instance))
        {TRACE_IT(51003);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Operand_Invalid_NeedObject, _u("in"));
        }

        PropertyRecord const * propertyRecord;
        uint32 index;
        IndexType indexType = GetIndexType(argProperty, scriptContext, &index, &propertyRecord, true);

        RecyclableObject* object = RecyclableObject::FromVar(instance);

        BOOL result;
        if( indexType == Js::IndexType_Number )
        {TRACE_IT(51004);
            result = JavascriptOperators::HasItem( object, index );
        }
        else
        {TRACE_IT(51005);
            PropertyId propertyId = propertyRecord->GetPropertyId();
            result = JavascriptOperators::HasProperty( object, propertyId );

#ifdef TELEMETRY_JSO
            {TRACE_IT(51006);
                Assert(indexType != Js::IndexType_JavascriptString);
                if( indexType == Js::IndexType_PropertyId )
                {TRACE_IT(51007);
                    scriptContext->GetTelemetry().GetOpcodeTelemetry().IsIn( instance, propertyId, result != 0 );
                }
            }
#endif
        }
        return JavascriptBoolean::ToVar(result, scriptContext);
    }

    template <bool IsFromFullJit, class TInlineCache>
    inline Var JavascriptOperators::PatchGetValue(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId)
    {TRACE_IT(51008);
        return PatchGetValueWithThisPtr<IsFromFullJit, TInlineCache>(functionBody, inlineCache, inlineCacheIndex, instance, propertyId, instance);
    }


    template <bool IsFromFullJit, class TInlineCache>
    __forceinline Var JavascriptOperators::PatchGetValueWithThisPtr(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var thisInstance)
    {TRACE_IT(51009);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        Assert(Js::JavascriptStackWalker::ValidateTopJitFrame(scriptContext));

        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(51010);
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(51011);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined,
                    scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            else
            {TRACE_IT(51012);
                return scriptContext->GetLibrary()->GetUndefined();
            }
        }

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        Var value;
        if (CacheOperators::TryGetProperty<true, true, true, true, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                thisInstance, false, object, propertyId, &value, scriptContext, nullptr, &info))
        {TRACE_IT(51013);
            return value;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51014);
            CacheOperators::TraceCache(inlineCache, _u("PatchGetValue"), propertyId, scriptContext, object);
        }
#endif

        return JavascriptOperators::GetProperty(thisInstance, object, propertyId, scriptContext, &info);
    }


    template Var JavascriptOperators::PatchGetValue<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetValue<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetValue<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetValue<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);

    template Var JavascriptOperators::PatchGetValueWithThisPtr<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var thisInstance);
    template Var JavascriptOperators::PatchGetValueWithThisPtr<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var thisInstance);
    template Var JavascriptOperators::PatchGetValueWithThisPtr<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var thisInstance);
    template Var JavascriptOperators::PatchGetValueWithThisPtr<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var thisInstance);

    template <bool IsFromFullJit, class TInlineCache>
    Var JavascriptOperators::PatchGetValueForTypeOf(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId)
    {TRACE_IT(51015);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        Assert(Js::JavascriptStackWalker::ValidateTopJitFrame(scriptContext));

        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(51016);
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(51017);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined,
                    scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            else
            {TRACE_IT(51018);
                return scriptContext->GetLibrary()->GetUndefined();
            }
        }

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        Var value;
        if (CacheOperators::TryGetProperty<true, true, true, true, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
            instance, false, object, propertyId, &value, scriptContext, nullptr, &info))
        {TRACE_IT(51019);
            return value;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51020);
            CacheOperators::TraceCache(inlineCache, _u("PatchGetValueForTypeOf"), propertyId, scriptContext, object);
        }
#endif
        Var prop = nullptr;

        BEGIN_TYPEOF_ERROR_HANDLER(scriptContext);
        prop = JavascriptOperators::GetProperty(instance, object, propertyId, scriptContext, &info);
        END_TYPEOF_ERROR_HANDLER(scriptContext, prop);

        return prop;
    }
    template Var JavascriptOperators::PatchGetValueForTypeOf<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetValueForTypeOf<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetValueForTypeOf<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetValueForTypeOf<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);


    Var JavascriptOperators::PatchGetValueUsingSpecifiedInlineCache(InlineCache * inlineCache, Var instance, RecyclableObject * object, PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(51021);
        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, inlineCache);
        Var value;
        if (CacheOperators::TryGetProperty<true, true, true, true, false, true, !InlineCache::IsPolymorphic, InlineCache::IsPolymorphic, false>(
                instance, false, object, propertyId, &value, scriptContext, nullptr, &info))
        {TRACE_IT(51022);
            return value;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51023);
            CacheOperators::TraceCache(inlineCache, _u("PatchGetValue"), propertyId, scriptContext, object);
        }
#endif

        return JavascriptOperators::GetProperty(instance, object, propertyId, scriptContext, &info);
    }

    Var JavascriptOperators::PatchGetValueNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId)
    {TRACE_IT(51024);
        return PatchGetValueWithThisPtrNoFastPath(functionBody, inlineCache, inlineCacheIndex, instance, propertyId, instance);
    }

    Var JavascriptOperators::PatchGetValueWithThisPtrNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var thisInstance)
    {TRACE_IT(51025);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(51026);
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(51027);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined,
                    scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            else
            {TRACE_IT(51028);
                return scriptContext->GetLibrary()->GetUndefined();
            }
        }

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, true);
        return JavascriptOperators::GetProperty(thisInstance, object, propertyId, scriptContext, &info);
    }

    template <bool IsFromFullJit, class TInlineCache>
    inline Var JavascriptOperators::PatchGetRootValue(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId)
    {TRACE_IT(51029);
        AssertMsg(RootObjectBase::Is(object), "Root must be a global object!");

        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        Var value;
        if (CacheOperators::TryGetProperty<true, true, true, false, true, false, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                object, true, object, propertyId, &value, scriptContext, nullptr, &info))
        {TRACE_IT(51030);
            return value;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51031);
            CacheOperators::TraceCache(inlineCache, _u("PatchGetRootValue"), propertyId, scriptContext, object);
        }
#endif

        return JavascriptOperators::OP_GetRootProperty(object, propertyId, &info, scriptContext);
    }
    template Var JavascriptOperators::PatchGetRootValue<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootValue<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootValue<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootValue<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId);

    template <bool IsFromFullJit, class TInlineCache>
    Var JavascriptOperators::PatchGetRootValueForTypeOf(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId)
    {TRACE_IT(51032);
        AssertMsg(RootObjectBase::Is(object), "Root must be a global object!");

        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        Var value = nullptr;
        if (CacheOperators::TryGetProperty<true, true, true, false, true, false, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
            object, true, object, propertyId, &value, scriptContext, nullptr, &info))
        {TRACE_IT(51033);
            return value;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51034);
            CacheOperators::TraceCache(inlineCache, _u("PatchGetRootValueForTypeOf"), propertyId, scriptContext, object);
        }
#endif
        value = nullptr;
        BEGIN_TYPEOF_ERROR_HANDLER(scriptContext);
        if (JavascriptOperators::GetRootProperty(RecyclableObject::FromVar(object), propertyId, &value, scriptContext, &info))
        {TRACE_IT(51035);
            if (scriptContext->IsUndeclBlockVar(value))
            {TRACE_IT(51036);
                JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
            }
            return value;
        }
        END_TYPEOF_ERROR_HANDLER(scriptContext, value);

        value = scriptContext->GetLibrary()->GetUndefined();
        return value;
    }
    template Var JavascriptOperators::PatchGetRootValueForTypeOf<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootValueForTypeOf<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootValueForTypeOf<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootValueForTypeOf<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject * object, PropertyId propertyId);


    Var JavascriptOperators::PatchGetRootValueNoFastPath_Var(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId)
    {TRACE_IT(51037);
        return
            PatchGetRootValueNoFastPath(
                functionBody,
                inlineCache,
                inlineCacheIndex,
                DynamicObject::FromVar(instance),
                propertyId);
    }

    Var JavascriptOperators::PatchGetRootValueNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject* object, PropertyId propertyId)
    {TRACE_IT(51038);
        AssertMsg(RootObjectBase::Is(object), "Root must be a global object!");

        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, true);
        return JavascriptOperators::OP_GetRootProperty(object, propertyId, &info, scriptContext);
    }

    template <bool IsFromFullJit, class TInlineCache>
    inline Var JavascriptOperators::PatchGetPropertyScoped(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance)
    {TRACE_IT(51039);
        // Get the property, using a scope stack rather than an individual instance.
        // Walk the stack until we find an instance that has the property.

        ScriptContext *const scriptContext = functionBody->GetScriptContext();
        uint16 length = pDisplay->GetLength();

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        for (uint16 i = 0; i < length; i++)
        {TRACE_IT(51040);
            DynamicObject* object = (DynamicObject*)pDisplay->GetItem(i);
            Var value;
            if (CacheOperators::TryGetProperty<true, true, true, false, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                    object, false, object, propertyId, &value, scriptContext, nullptr, &info))
            {TRACE_IT(51041);
                return value;
            }

#if DBG_DUMP
            if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
            {TRACE_IT(51042);
                CacheOperators::TraceCache(inlineCache, _u("PatchGetPropertyScoped"), propertyId, scriptContext, object);
            }
#endif
            if (JavascriptOperators::GetProperty(object, propertyId, &value, scriptContext, &info))
            {TRACE_IT(51043);
                if (scriptContext->IsUndeclBlockVar(value) && propertyId != PropertyIds::_lexicalThisSlotSymbol)
                {TRACE_IT(51044);
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
                }
                return value;
            }
        }

        // No one in the scope stack has the property, so get it from the default instance provided by the caller.
        Var value = JavascriptOperators::PatchGetRootValue<IsFromFullJit>(functionBody, inlineCache, inlineCacheIndex, DynamicObject::FromVar(defaultInstance), propertyId);
        if (scriptContext->IsUndeclBlockVar(value))
        {TRACE_IT(51045);
            JavascriptError::ThrowReferenceError(scriptContext, JSERR_UseBeforeDeclaration);
        }
        return value;
    }
    template Var JavascriptOperators::PatchGetPropertyScoped<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance);
    template Var JavascriptOperators::PatchGetPropertyScoped<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance);
    template Var JavascriptOperators::PatchGetPropertyScoped<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance);
    template Var JavascriptOperators::PatchGetPropertyScoped<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance);

    template <bool IsFromFullJit, class TInlineCache>
    Var JavascriptOperators::PatchGetPropertyForTypeOfScoped(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance)
    {TRACE_IT(51046);
        Var value = nullptr;
        ScriptContext *scriptContext = functionBody->GetScriptContext();

        BEGIN_TYPEOF_ERROR_HANDLER(scriptContext);
        value = JavascriptOperators::PatchGetPropertyScoped<IsFromFullJit, TInlineCache>(functionBody, inlineCache, inlineCacheIndex, pDisplay, propertyId, defaultInstance);
        END_TYPEOF_ERROR_HANDLER(scriptContext, value)

        return value;
    }
    template Var JavascriptOperators::PatchGetPropertyForTypeOfScoped<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance);
    template Var JavascriptOperators::PatchGetPropertyForTypeOfScoped<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance);
    template Var JavascriptOperators::PatchGetPropertyForTypeOfScoped<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance);
    template Var JavascriptOperators::PatchGetPropertyForTypeOfScoped<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, FrameDisplay *pDisplay, PropertyId propertyId, Var defaultInstance);


    template <bool IsFromFullJit, class TInlineCache>
    inline Var JavascriptOperators::PatchGetMethod(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId)
    {TRACE_IT(51047);
        Assert(inlineCache != nullptr);

        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        RecyclableObject* object = nullptr;
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(51048);
            // Don't error if we disabled implicit calls
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(51049);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined,
                    scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            else
            {TRACE_IT(51050);
#ifdef TELEMETRY_JSO
                if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
                {TRACE_IT(51051);
                    // `successful` will be true as PatchGetMethod throws an exception if not found.
                    scriptContext->GetTelemetry().GetOpcodeTelemetry().GetMethodProperty(object, propertyId, value, /*successful:*/false);
                }
#endif
                return scriptContext->GetLibrary()->GetUndefined();
            }
        }

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        Var value;
        if (CacheOperators::TryGetProperty<true, true, true, false, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                instance, false, object, propertyId, &value, scriptContext, nullptr, &info))
        {TRACE_IT(51052);
            return value;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51053);
            CacheOperators::TraceCache(inlineCache, _u("PatchGetMethod"), propertyId, scriptContext, object);
        }
#endif

        value = Js::JavascriptOperators::PatchGetMethodFromObject(instance, object, propertyId, &info, scriptContext, false);
#ifdef TELEMETRY_JSO
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {TRACE_IT(51054);
            // `successful` will be true as PatchGetMethod throws an exception if not found.
            scriptContext->GetTelemetry().GetOpcodeTelemetry().GetMethodProperty(object, propertyId, value, /*successful:*/true);
        }
#endif
        return value;
    }
    template Var JavascriptOperators::PatchGetMethod<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetMethod<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetMethod<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetMethod<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);

    template <bool IsFromFullJit, class TInlineCache>
    inline Var JavascriptOperators::PatchGetRootMethod(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject* object, PropertyId propertyId)
    {TRACE_IT(51055);
        Assert(inlineCache != nullptr);

        AssertMsg(RootObjectBase::Is(object), "Root must be a global object!");

        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        Var value;
        if (CacheOperators::TryGetProperty<true, true, true, false, true, false, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                object, true, object, propertyId, &value, scriptContext, nullptr, &info))
        {TRACE_IT(51056);
            return value;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51057);
            CacheOperators::TraceCache(inlineCache, _u("PatchGetRootMethod"), propertyId, scriptContext, object);
        }
#endif

        value = Js::JavascriptOperators::PatchGetMethodFromObject(object, object, propertyId, &info, scriptContext, true);
#ifdef TELEMETRY_JSO
        if (TELEMETRY_PROPERTY_OPCODE_FILTER(propertyId))
        {TRACE_IT(51058);
            // `successful` will be true as PatchGetMethod throws an exception if not found.
            scriptContext->GetTelemetry().GetOpcodeTelemetry().GetMethodProperty(object, propertyId, value, /*successful:*/ true);
        }
#endif
        return value;
    }
    template Var JavascriptOperators::PatchGetRootMethod<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject* object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootMethod<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject* object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootMethod<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject* object, PropertyId propertyId);
    template Var JavascriptOperators::PatchGetRootMethod<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject* object, PropertyId propertyId);

    template <bool IsFromFullJit, class TInlineCache>
    inline Var JavascriptOperators::PatchScopedGetMethod(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId)
    {TRACE_IT(51059);
        Assert(inlineCache != nullptr);

        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(51060);
            // Don't error if we disabled implicit calls
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(51061);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined,
                    scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            else
            {TRACE_IT(51062);
                return scriptContext->GetLibrary()->GetUndefined();
            }
        }

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        const bool isRoot = RootObjectBase::Is(object);
        Var value;
        if (CacheOperators::TryGetProperty<true, true, true, false, true, false, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                instance, isRoot, object, propertyId, &value, scriptContext, nullptr, &info))
        {TRACE_IT(51063);
            return value;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51064);
            CacheOperators::TraceCache(inlineCache, _u("PatchGetMethod"), propertyId, scriptContext, object);
        }
#endif

        return Js::JavascriptOperators::PatchGetMethodFromObject(instance, object, propertyId, &info, scriptContext, isRoot);
    }
    template Var JavascriptOperators::PatchScopedGetMethod<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchScopedGetMethod<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchScopedGetMethod<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);
    template Var JavascriptOperators::PatchScopedGetMethod<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId);

    Var JavascriptOperators::PatchGetMethodNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId)
    {TRACE_IT(51065);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptOperators::GetPropertyObject(instance, scriptContext, &object))
        {TRACE_IT(51066);
            // Don't error if we disabled implicit calls
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(51067);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotGet_NullOrUndefined,
                    scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            else
            {TRACE_IT(51068);
                return scriptContext->GetLibrary()->GetUndefined();
            }
        }

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, true);
        return Js::JavascriptOperators::PatchGetMethodFromObject(instance, object, propertyId, &info, scriptContext, false);
    }

    Var JavascriptOperators::PatchGetRootMethodNoFastPath_Var(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId)
    {TRACE_IT(51069);
        return
            PatchGetRootMethodNoFastPath(
                functionBody,
                inlineCache,
                inlineCacheIndex,
                DynamicObject::FromVar(instance),
                propertyId);
    }

    Var JavascriptOperators::PatchGetRootMethodNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, DynamicObject* object, PropertyId propertyId)
    {TRACE_IT(51070);
        AssertMsg(RootObjectBase::Is(object), "Root must be a global object!");

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, true);
        return Js::JavascriptOperators::PatchGetMethodFromObject(object, object, propertyId, &info, functionBody->GetScriptContext(), true);
    }

    Var JavascriptOperators::PatchGetMethodFromObject(Var instance, RecyclableObject* propertyObject, PropertyId propertyId, PropertyValueInfo * info, ScriptContext* scriptContext, bool isRootLd)
    {TRACE_IT(51071);
        Assert(IsPropertyObject(propertyObject));

        Var value = nullptr;
        BOOL foundValue = FALSE;

        if (isRootLd)
        {TRACE_IT(51072);
            RootObjectBase* rootObject = RootObjectBase::FromVar(instance);
            foundValue = JavascriptOperators::GetRootPropertyReference(rootObject, propertyId, &value, scriptContext, info);
        }
        else
        {TRACE_IT(51073);
            foundValue = JavascriptOperators::GetPropertyReference(instance, propertyObject, propertyId, &value, scriptContext, info);
        }

        if (!foundValue)
        {TRACE_IT(51074);
            // Don't error if we disabled implicit calls
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {TRACE_IT(51075);
                const char16* propertyName = scriptContext->GetPropertyName(propertyId)->GetBuffer();

                value = scriptContext->GetLibrary()->GetUndefined();
                JavascriptFunction * caller = NULL;
                if (JavascriptStackWalker::GetCaller(&caller, scriptContext))
                {TRACE_IT(51076);
                    FunctionBody * callerBody = caller->GetFunctionBody();
                    if (callerBody && callerBody->GetUtf8SourceInfo()->GetIsXDomain())
                    {TRACE_IT(51077);
                        propertyName = NULL;
                    }
                }

                // Prior to version 12 we had mistakenly immediately thrown an error for property reference method calls
                // (i.e. <expr>.foo() form) when the target object is the global object.  The spec says that a GetValue
                // on a reference should throw if the reference is unresolved, of which a property reference can never be,
                // however it can be unresolved in the case of an identifier expression, e.g. foo() with no qualification.
                // Such a case would come down to the global object if foo was undefined, hence the check for root object,
                // except that it should have been a check for isRootLd to be correct.
                //
                //   // (at global scope)
                //   foo(x());
                //
                // should throw an error before evaluating x() if foo is not defined, but
                //
                //   // (at global scope)
                //   this.foo(x());
                //
                // should evaluate x() before throwing an error if foo is not a property on the global object.
                // Maintain old behavior prior to version 12.
                bool isPropertyReference = !isRootLd;
                if (!isPropertyReference)
                {TRACE_IT(51078);
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_UndefVariable, propertyName);
                }
                else
                {TRACE_IT(51079);
                    // ES5 11.2.3 #2: We evaluate the call target but don't throw yet if target member is missing. We need to evaluate argList
                    // first (#3). Postpone throwing error to invoke time.
                    value = ThrowErrorObject::CreateThrowTypeErrorObject(scriptContext, VBSERR_OLENoPropOrMethod, propertyName);
                }
            }
        }
        return value;
    }

    template <bool IsFromFullJit, class TInlineCache>
    inline void JavascriptOperators::PatchPutValue(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags)
    {TRACE_IT(51080);
        return PatchPutValueWithThisPtr<IsFromFullJit, TInlineCache>(functionBody, inlineCache, inlineCacheIndex, instance, propertyId, newValue, instance, flags);
    }

    template <bool IsFromFullJit, class TInlineCache>
    inline void JavascriptOperators::PatchPutValueWithThisPtr(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, Var thisInstance, PropertyOperationFlags flags)
    {TRACE_IT(51081);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        if (TaggedNumber::Is(instance))
        {TRACE_IT(51082);
            JavascriptOperators::SetPropertyOnTaggedNumber(instance, nullptr, propertyId, newValue, scriptContext, flags);
            return;
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        RecyclableObject* object = RecyclableObject::FromVar(instance);
        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        if (CacheOperators::TrySetProperty<true, true, true, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                object, false, propertyId, newValue, scriptContext, flags, nullptr, &info))
        {TRACE_IT(51083);
            return;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51084);
            CacheOperators::TraceCache(inlineCache, _u("PatchPutValue"), propertyId, scriptContext, object);
        }
#endif

        ImplicitCallFlags prevImplicitCallFlags = ImplicitCall_None;
        ImplicitCallFlags currImplicitCallFlags = ImplicitCall_None;
        bool hasThisOnlyStatements = functionBody->GetHasOnlyThisStmts();
        if (hasThisOnlyStatements)
        {TRACE_IT(51085);
            prevImplicitCallFlags = CacheAndClearImplicitBit(scriptContext);
        }
        if (!JavascriptOperators::OP_SetProperty(object, propertyId, newValue, scriptContext, &info, flags, thisInstance))
        {TRACE_IT(51086);
            // Add implicit call flags, to bail out if field copy prop may propagate the wrong value.
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
        }
        if (hasThisOnlyStatements)
        {TRACE_IT(51087);
            currImplicitCallFlags = CheckAndUpdateFunctionBodyWithImplicitFlag(functionBody);
            RestoreImplicitFlag(scriptContext, prevImplicitCallFlags, currImplicitCallFlags);
        }
    }
    template void JavascriptOperators::PatchPutValue<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValue<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValue<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValue<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);

    template <bool IsFromFullJit, class TInlineCache>
    inline void JavascriptOperators::PatchPutRootValue(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags)
    {TRACE_IT(51088);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        RecyclableObject* object = RecyclableObject::FromVar(instance);
        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        if (CacheOperators::TrySetProperty<true, true, true, true, false, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                object, true, propertyId, newValue, scriptContext, flags, nullptr, &info))
        {TRACE_IT(51089);
            return;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51090);
            CacheOperators::TraceCache(inlineCache, _u("PatchPutRootValue"), propertyId, scriptContext, object);
        }
#endif

        ImplicitCallFlags prevImplicitCallFlags = ImplicitCall_None;
        ImplicitCallFlags currImplicitCallFlags = ImplicitCall_None;
        bool hasThisOnlyStatements = functionBody->GetHasOnlyThisStmts();
        if (hasThisOnlyStatements)
        {TRACE_IT(51091);
            prevImplicitCallFlags = CacheAndClearImplicitBit(scriptContext);
        }
        if (!JavascriptOperators::SetRootProperty(object, propertyId, newValue, &info, scriptContext, flags))
        {TRACE_IT(51092);
            // Add implicit call flags, to bail out if field copy prop may propagate the wrong value.
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
        }
        if (hasThisOnlyStatements)
        {TRACE_IT(51093);
            currImplicitCallFlags = CheckAndUpdateFunctionBodyWithImplicitFlag(functionBody);
            RestoreImplicitFlag(scriptContext, prevImplicitCallFlags, currImplicitCallFlags);
        }
    }
    template void JavascriptOperators::PatchPutRootValue<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutRootValue<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutRootValue<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutRootValue<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);

    template <bool IsFromFullJit, class TInlineCache>
    inline void JavascriptOperators::PatchPutValueNoLocalFastPath(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags)
    {TRACE_IT(51094);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        if (TaggedNumber::Is(instance))
        {TRACE_IT(51095);
            JavascriptOperators::SetPropertyOnTaggedNumber(instance,
                                        nullptr,
                                        propertyId,
                                        newValue,
                                        scriptContext,
                                        flags);
             return;
        }
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        RecyclableObject *object = RecyclableObject::FromVar(instance);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        if (CacheOperators::TrySetProperty<!TInlineCache::IsPolymorphic, true, true, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                object, false, propertyId, newValue, scriptContext, flags, nullptr, &info))
        {TRACE_IT(51096);
            return;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51097);
            CacheOperators::TraceCache(inlineCache, _u("PatchPutValueNoLocalFastPath"), propertyId, scriptContext, object);
        }
#endif

        ImplicitCallFlags prevImplicitCallFlags = ImplicitCall_None;
        ImplicitCallFlags currImplicitCallFlags = ImplicitCall_None;
        bool hasThisOnlyStatements = functionBody->GetHasOnlyThisStmts();
        if (hasThisOnlyStatements)
        {TRACE_IT(51098);
            prevImplicitCallFlags = CacheAndClearImplicitBit(scriptContext);
        }
        if (!JavascriptOperators::OP_SetProperty(instance, propertyId, newValue, scriptContext, &info, flags))
        {TRACE_IT(51099);
            // Add implicit call flags, to bail out if field copy prop may propagate the wrong value.
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
        }
        if (hasThisOnlyStatements)
        {TRACE_IT(51100);
            currImplicitCallFlags = CheckAndUpdateFunctionBodyWithImplicitFlag(functionBody);
            RestoreImplicitFlag(scriptContext, prevImplicitCallFlags, currImplicitCallFlags);
        }
    }
    template void JavascriptOperators::PatchPutValueNoLocalFastPath<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValueNoLocalFastPath<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValueNoLocalFastPath<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValueNoLocalFastPath<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);

    template <bool IsFromFullJit, class TInlineCache>
    inline void JavascriptOperators::PatchPutValueWithThisPtrNoLocalFastPath(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, Var thisInstance, PropertyOperationFlags flags)
    {TRACE_IT(51101);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        if (TaggedNumber::Is(instance))
        {TRACE_IT(51102);
            JavascriptOperators::SetPropertyOnTaggedNumber(instance,
                nullptr,
                propertyId,
                newValue,
                scriptContext,
                flags);
            return;
        }
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        RecyclableObject *object = RecyclableObject::FromVar(instance);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        if (CacheOperators::TrySetProperty<!TInlineCache::IsPolymorphic, true, true, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
            object, false, propertyId, newValue, scriptContext, flags, nullptr, &info))
        {TRACE_IT(51103);
            return;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51104);
            CacheOperators::TraceCache(inlineCache, _u("PatchPutValueNoLocalFastPath"), propertyId, scriptContext, object);
        }
#endif

        ImplicitCallFlags prevImplicitCallFlags = ImplicitCall_None;
        ImplicitCallFlags currImplicitCallFlags = ImplicitCall_None;
        bool hasThisOnlyStatements = functionBody->GetHasOnlyThisStmts();
        if (hasThisOnlyStatements)
        {TRACE_IT(51105);
            prevImplicitCallFlags = CacheAndClearImplicitBit(scriptContext);
        }
        if (!JavascriptOperators::OP_SetProperty(instance, propertyId, newValue, scriptContext, &info, flags, thisInstance))
        {TRACE_IT(51106);
            // Add implicit call flags, to bail out if field copy prop may propagate the wrong value.
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
        }
        if (hasThisOnlyStatements)
        {TRACE_IT(51107);
            currImplicitCallFlags = CheckAndUpdateFunctionBodyWithImplicitFlag(functionBody);
            RestoreImplicitFlag(scriptContext, prevImplicitCallFlags, currImplicitCallFlags);
        }
    }
    template void JavascriptOperators::PatchPutValueWithThisPtrNoLocalFastPath<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, Var thisInstance, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValueWithThisPtrNoLocalFastPath<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, Var thisInstance, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValueWithThisPtrNoLocalFastPath<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, Var thisInstance, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutValueWithThisPtrNoLocalFastPath<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, Var thisInstance, PropertyOperationFlags flags);

    template <bool IsFromFullJit, class TInlineCache>
    inline void JavascriptOperators::PatchPutRootValueNoLocalFastPath(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags)
    {TRACE_IT(51108);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        RecyclableObject *object = RecyclableObject::FromVar(instance);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        if (CacheOperators::TrySetProperty<!TInlineCache::IsPolymorphic, true, true, true, false, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                object, true, propertyId, newValue, scriptContext, flags, nullptr, &info))
        {TRACE_IT(51109);
            return;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51110);
            CacheOperators::TraceCache(inlineCache, _u("PatchPutRootValueNoLocalFastPath"), propertyId, scriptContext, object);
        }
#endif

        ImplicitCallFlags prevImplicitCallFlags = ImplicitCall_None;
        ImplicitCallFlags currImplicitCallFlags = ImplicitCall_None;
        bool hasThisOnlyStatements = functionBody->GetHasOnlyThisStmts();
        if (hasThisOnlyStatements)
        {TRACE_IT(51111);
            prevImplicitCallFlags = CacheAndClearImplicitBit(scriptContext);
        }
        if (!JavascriptOperators::SetRootProperty(object, propertyId, newValue, &info, scriptContext, flags))
        {TRACE_IT(51112);
            // Add implicit call flags, to bail out if field copy prop may propagate the wrong value.
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
        }
        if (hasThisOnlyStatements)
        {TRACE_IT(51113);
            currImplicitCallFlags = CheckAndUpdateFunctionBodyWithImplicitFlag(functionBody);
            RestoreImplicitFlag(scriptContext, prevImplicitCallFlags, currImplicitCallFlags);
        }
    }
    template void JavascriptOperators::PatchPutRootValueNoLocalFastPath<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutRootValueNoLocalFastPath<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutRootValueNoLocalFastPath<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);
    template void JavascriptOperators::PatchPutRootValueNoLocalFastPath<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags);

    void JavascriptOperators::PatchPutValueNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags)
    {
        PatchPutValueWithThisPtrNoFastPath(functionBody, inlineCache, inlineCacheIndex, instance, propertyId, newValue, instance, flags);
    }

    void JavascriptOperators::PatchPutValueWithThisPtrNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, Var thisInstance, PropertyOperationFlags flags)
    {TRACE_IT(51114);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        if (TaggedNumber::Is(instance))
        {TRACE_IT(51115);
            JavascriptOperators::SetPropertyOnTaggedNumber(instance, nullptr, propertyId, newValue, scriptContext, flags);
            return;
        }
        RecyclableObject* object = RecyclableObject::FromVar(instance);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, true);
        if (!JavascriptOperators::OP_SetProperty(object, propertyId, newValue, scriptContext, &info, flags, thisInstance))
        {TRACE_IT(51116);
            // Add implicit call flags, to bail out if field copy prop may propagate the wrong value.
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
        }
    }

    void JavascriptOperators::PatchPutRootValueNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, Var instance, PropertyId propertyId, Var newValue, PropertyOperationFlags flags)
    {TRACE_IT(51117);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        RecyclableObject* object = RecyclableObject::FromVar(instance);

        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, true);
        if (!JavascriptOperators::SetRootProperty(object, propertyId, newValue, &info, scriptContext, flags))
        {TRACE_IT(51118);
            // Add implicit call flags, to bail out if field copy prop may propagate the wrong value.
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_NoOpSet);
        }
    }

    template <bool IsFromFullJit, class TInlineCache>
    inline void JavascriptOperators::PatchInitValue(FunctionBody *const functionBody, TInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, RecyclableObject* object, PropertyId propertyId, Var newValue)
    {TRACE_IT(51119);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();

        const PropertyOperationFlags flags = newValue == NULL ? PropertyOperation_SpecialValue : PropertyOperation_None;
        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, !IsFromFullJit);
        if (CacheOperators::TrySetProperty<true, true, false, true, true, !TInlineCache::IsPolymorphic, TInlineCache::IsPolymorphic, false>(
                object, false, propertyId, newValue, scriptContext, flags, nullptr, &info))
        {TRACE_IT(51120);
            return;
        }

#if DBG_DUMP
        if (PHASE_VERBOSE_TRACE1(Js::InlineCachePhase))
        {TRACE_IT(51121);
            CacheOperators::TraceCache(inlineCache, _u("PatchInitValue"), propertyId, scriptContext, object);
        }
#endif

        Type *typeWithoutProperty = object->GetType();

        // Ideally the lowerer would emit a call to the right flavor of PatchInitValue, so that we can ensure that we only
        // ever initialize to NULL in the right cases.  But the backend uses the StFld opcode for initialization, and it
        // would be cumbersome to thread the different helper calls all the way down
        if (object->InitProperty(propertyId, newValue, flags, &info))
        {TRACE_IT(51122);
            CacheOperators::CachePropertyWrite(object, false, typeWithoutProperty, propertyId, &info, scriptContext);
        }
    }
    template void JavascriptOperators::PatchInitValue<false, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, RecyclableObject* object, PropertyId propertyId, Var newValue);
    template void JavascriptOperators::PatchInitValue<true, InlineCache>(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, RecyclableObject* object, PropertyId propertyId, Var newValue);
    template void JavascriptOperators::PatchInitValue<false, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, RecyclableObject* object, PropertyId propertyId, Var newValue);
    template void JavascriptOperators::PatchInitValue<true, PolymorphicInlineCache>(FunctionBody *const functionBody, PolymorphicInlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, RecyclableObject* object, PropertyId propertyId, Var newValue);

    void JavascriptOperators::PatchInitValueNoFastPath(FunctionBody *const functionBody, InlineCache *const inlineCache, const InlineCacheIndex inlineCacheIndex, RecyclableObject* object, PropertyId propertyId, Var newValue)
    {TRACE_IT(51123);
        PropertyValueInfo info;
        PropertyValueInfo::SetCacheInfo(&info, functionBody, inlineCache, inlineCacheIndex, true);
        Type *typeWithoutProperty = object->GetType();
        if (object->InitProperty(propertyId, newValue, PropertyOperation_None, &info))
        {TRACE_IT(51124);
            CacheOperators::CachePropertyWrite(object, false, typeWithoutProperty, propertyId, &info, functionBody->GetScriptContext());
        }
    }

#if ENABLE_DEBUG_CONFIG_OPTIONS
    void JavascriptOperators::TracePropertyEquivalenceCheck(const JitEquivalentTypeGuard* guard, const Type* type, const Type* refType, bool isEquivalent, uint failedPropertyIndex)
    {TRACE_IT(51125);
        if (PHASE_TRACE1(Js::EquivObjTypeSpecPhase))
        {TRACE_IT(51126);
            uint propertyCount = guard->GetCache()->record.propertyCount;

            Output::Print(_u("EquivObjTypeSpec: checking %u properties on operation %u, (type = 0x%p, ref type = 0x%p):\n"),
                propertyCount, guard->GetObjTypeSpecFldId(), type, refType);

            const Js::TypeEquivalenceRecord& record = guard->GetCache()->record;
            ScriptContext* scriptContext = type->GetScriptContext();
            if (isEquivalent)
            {TRACE_IT(51127);
                if (Js::Configuration::Global.flags.Verbose)
                {TRACE_IT(51128);
                    Output::Print(_u("    <start>, "));
                    for (uint pi = 0; pi < propertyCount; pi++)
                    {TRACE_IT(51129);
                        const EquivalentPropertyEntry* refInfo = &record.properties[pi];
                        const PropertyRecord* propertyRecord = scriptContext->GetPropertyName(refInfo->propertyId);
                        Output::Print(_u("%s(#%d)@%ua%dw%d, "), propertyRecord->GetBuffer(), propertyRecord->GetPropertyId(), refInfo->slotIndex, refInfo->isAuxSlot, refInfo->mustBeWritable);
                    }
                    Output::Print(_u("<end>\n"));
                }
            }
            else
            {TRACE_IT(51130);
                const EquivalentPropertyEntry* refInfo = &record.properties[failedPropertyIndex];
                Js::PropertyEquivalenceInfo info(Constants::NoSlot, false, false);
                const PropertyRecord* propertyRecord = scriptContext->GetPropertyName(refInfo->propertyId);
                if (DynamicType::Is(type->GetTypeId()))
                {TRACE_IT(51131);
                    Js::DynamicTypeHandler* typeHandler = (static_cast<const DynamicType*>(type))->GetTypeHandler();
                    typeHandler->GetPropertyEquivalenceInfo(propertyRecord, info);
                }

                Output::Print(_u("EquivObjTypeSpec: check failed for %s (#%d) on operation %u:\n"),
                    propertyRecord->GetBuffer(), propertyRecord->GetPropertyId(), guard->GetObjTypeSpecFldId());
                Output::Print(_u("    type = 0x%p, ref type = 0x%p, slot = 0x%u (%d), ref slot = 0x%u (%d), is writable = %d, required writable = %d\n"),
                    type, refType, info.slotIndex, refInfo->slotIndex, info.isAuxSlot, refInfo->isAuxSlot, info.isWritable, refInfo->mustBeWritable);
            }

            Output::Flush();
        }
    }
#endif

    bool JavascriptOperators::IsStaticTypeObjTypeSpecEquivalent(const TypeEquivalenceRecord& equivalenceRecord, uint& failedIndex)
    {TRACE_IT(51132);
        uint propertyCount = equivalenceRecord.propertyCount;
        Js::EquivalentPropertyEntry* properties = equivalenceRecord.properties;
        for (uint pi = 0; pi < propertyCount; pi++)
        {TRACE_IT(51133);
            const EquivalentPropertyEntry* refInfo = &properties[pi];
            if (!IsStaticTypeObjTypeSpecEquivalent(refInfo))
            {TRACE_IT(51134);
                failedIndex = pi;
                return false;
            }
        }
        return true;
    }

    bool JavascriptOperators::IsStaticTypeObjTypeSpecEquivalent(const EquivalentPropertyEntry *entry)
    {TRACE_IT(51135);
        // Objects of static types have no local properties, but they may load fields from their prototypes.
        return entry->slotIndex == Constants::NoSlot && !entry->mustBeWritable;
    }

    bool JavascriptOperators::CheckIfTypeIsEquivalentForFixedField(Type* type, JitEquivalentTypeGuard* guard)
    {TRACE_IT(51136);
        if (guard->GetValue() == PropertyGuard::GuardValue::Invalidated_DuringSweep)
        {TRACE_IT(51137);
            return false;
        }
        return CheckIfTypeIsEquivalent(type, guard);
    }

    bool JavascriptOperators::CheckIfTypeIsEquivalent(Type* type, JitEquivalentTypeGuard* guard)
    {TRACE_IT(51138);
        if (guard->GetValue() == PropertyGuard::GuardValue::Invalidated)
        {TRACE_IT(51139);
            return false;
        }

        AssertMsg(type && type->GetScriptContext(), "type and it's ScriptContext should be valid.");

        if (!guard->IsInvalidatedDuringSweep() && ((Js::Type*)guard->GetTypeAddr())->GetScriptContext() != type->GetScriptContext())
        {TRACE_IT(51140);
            // For valid guard value, can't cache cross-context objects
            return false;
        }

        // CONSIDER : Add stats on how often the cache hits, and simply force bailout if
        // the efficacy is too low.

        EquivalentTypeCache* cache = guard->GetCache();
        // CONSIDER : Consider emitting o.type == equivTypes[hash(o.type)] in machine code before calling
        // this helper, particularly if we want to handle polymorphism with frequently changing types.
        Assert(EQUIVALENT_TYPE_CACHE_SIZE == 8);
        Type** equivTypes = cache->types;

        Type* refType = equivTypes[0];
        if (refType == nullptr || refType->GetScriptContext() != type->GetScriptContext())
        {TRACE_IT(51141);
            // We could have guard that was invalidated while sweeping and now we have type coming from
            // different scriptContext. Make sure that it matches the scriptContext in cachedTypes.
            // If not, return false because as mentioned above, we don't cache cross-context objects.
#if DBG
            if (refType == nullptr)
            {TRACE_IT(51142);
                for (int i = 1;i < EQUIVALENT_TYPE_CACHE_SIZE;i++)
                {TRACE_IT(51143);
                    AssertMsg(equivTypes[i] == nullptr, "In equiv typed caches, if first element is nullptr, all others should be nullptr");
                }
            }
#endif
            return false;
        }

        if (type == equivTypes[0] || type == equivTypes[1] || type == equivTypes[2] || type == equivTypes[3] ||
            type == equivTypes[4] || type == equivTypes[5] || type == equivTypes[6] || type == equivTypes[7])
        {TRACE_IT(51144);
#if DBG
            if (PHASE_TRACE1(Js::EquivObjTypeSpecPhase))
            {TRACE_IT(51145);
                if (guard->WasReincarnated())
                {TRACE_IT(51146);
                    Output::Print(_u("EquivObjTypeSpec: Guard 0x%p was reincarnated and working now \n"), guard);
                    Output::Flush();
                }
            }
#endif
            guard->SetTypeAddr((intptr_t)type);
            return true;
        }

        // If we didn't find the type in the cache, let's check if it's equivalent the slow way, by comparing
        // each of its relevant property slots to its equivalent in one of the cached types.
        // We are making a few assumption that simplify the process:
        // 1. If two types have the same prototype, any properties loaded from a prototype must come from the same slot.
        //    If any of the prototypes in the chain was altered such that this is no longer true, the corresponding
        //    property guard would have been invalidated and we would bail out at the guard check (either on this
        //    type check or downstream, but before the property load is attempted).
        // 2. For polymorphic field loads fixed fields are only supported on prototypes.  Hence, if two types have the
        //    same prototype, any of the equivalent fixed properties will match. If any has been overwritten, the
        //    corresponding guard would have been invalidated and we would bail out (as above).

        if (cache->IsLoadedFromProto() && type->GetPrototype() != refType->GetPrototype())
        {TRACE_IT(51147);
            if (PHASE_TRACE1(Js::EquivObjTypeSpecPhase))
            {TRACE_IT(51148);
                Output::Print(_u("EquivObjTypeSpec: failed check on operation %u (type = 0x%x, ref type = 0x%x, proto = 0x%x, ref proto = 0x%x) \n"),
                    guard->GetObjTypeSpecFldId(), type, refType, type->GetPrototype(), refType->GetPrototype());
                Output::Flush();
            }

            return false;
        }

#pragma prefast(suppress:6011) // If type is nullptr, we would AV at the beginning of this method
        if (type->GetTypeId() != refType->GetTypeId())
        {TRACE_IT(51149);
            if (PHASE_TRACE1(Js::EquivObjTypeSpecPhase))
            {TRACE_IT(51150);
                Output::Print(_u("EquivObjTypeSpec: failed check on operation %u (type = 0x%x, ref type = 0x%x, proto = 0x%x, ref proto = 0x%x) \n"),
                    guard->GetObjTypeSpecFldId(), type, refType, type->GetPrototype(), refType->GetPrototype());
                Output::Flush();
            }

            return false;
        }

        // Review : This is quite slow.  We could make it somewhat faster, by keeping slot indexes instead
        // of property IDs, but that would mean we would need to look up property IDs from slot indexes when installing
        // property guards, or maintain a whole separate list of equivalent slot indexes.
        Assert(cache->record.propertyCount > 0);

        // Before checking for equivalence, track existing cached non-shared types
        DynamicType * dynamicType = (type && DynamicType::Is(type->GetTypeId())) ? static_cast<DynamicType*>(type) : nullptr;
        bool isEquivTypesCacheFull = equivTypes[EQUIVALENT_TYPE_CACHE_SIZE - 1] != nullptr;
        int emptySlotIndex = -1;
        int nonSharedTypeSlotIndex = -1;
        for (int i = 0;i < EQUIVALENT_TYPE_CACHE_SIZE;i++)
        {TRACE_IT(51151);
            // Track presence of cached non-shared type if cache is full
            if (isEquivTypesCacheFull)
            {TRACE_IT(51152);
                if (DynamicType::Is(equivTypes[i]->GetTypeId()) &&
                    nonSharedTypeSlotIndex == -1 &&
                    !(static_cast<DynamicType*>(equivTypes[i]))->GetIsShared())
                {TRACE_IT(51153);
                    nonSharedTypeSlotIndex = i;
                }
            }
            // Otherwise get the next available empty index
            else if (equivTypes[i] == nullptr)
            {TRACE_IT(51154);
                emptySlotIndex = i;
                break;
            };
        }

        // If we get non-shared type while cache is full and we don't have any non-shared type to evict
        // consider this type as non-equivalent
        if (dynamicType != nullptr &&
            isEquivTypesCacheFull &&
            !dynamicType->GetIsShared() &&
            nonSharedTypeSlotIndex == -1)
        {TRACE_IT(51155);
            return false;
        }

        // CONSIDER (EquivObjTypeSpec): Impose a limit on the number of properties guarded by an equivalent type check.
        // The trick is where in the glob opt to make the cut off. Perhaps in the forward pass we could track the number of
        // field operations protected by a type check (keep a counter on the type's value info), and if that counter exceeds
        // some threshold, simply stop optimizing any further instructions.

        bool isEquivalent;
        uint failedPropertyIndex;
        if (dynamicType != nullptr)
        {TRACE_IT(51156);
            Js::DynamicTypeHandler* typeHandler = dynamicType->GetTypeHandler();
            isEquivalent = typeHandler->IsObjTypeSpecEquivalent(type, cache->record, failedPropertyIndex);
        }
        else
        {TRACE_IT(51157);
            Assert(StaticType::Is(type->GetTypeId()));
            isEquivalent = IsStaticTypeObjTypeSpecEquivalent(cache->record, failedPropertyIndex);
        }

#if ENABLE_DEBUG_CONFIG_OPTIONS
        TracePropertyEquivalenceCheck(guard, type, refType, isEquivalent, failedPropertyIndex);
#endif

        if (!isEquivalent)
        {TRACE_IT(51158);
            return false;
        }

        AssertMsg(!isEquivTypesCacheFull || !dynamicType || dynamicType->GetIsShared() || nonSharedTypeSlotIndex > -1, "If equiv cache is full, then this should be sharedType or we will evict non-shared type.");

        // If cache is full, then this is definitely a sharedType, so evict non-shared type.
        // Else evict next empty slot (only applicable for DynamicTypes)
        emptySlotIndex = (isEquivTypesCacheFull && dynamicType) ? nonSharedTypeSlotIndex : emptySlotIndex;

        // We have some empty slots, let us use those first
        if (emptySlotIndex != -1)
        {TRACE_IT(51159);
            if (PHASE_TRACE1(Js::EquivObjTypeSpecPhase))
            {TRACE_IT(51160);
                Output::Print(_u("EquivObjTypeSpec: Saving type in unused slot of equiv types cache. \n"));
                Output::Flush();
            }
            equivTypes[emptySlotIndex] = type;
        }
        else
        {TRACE_IT(51161);
            // CONSIDER (EquivObjTypeSpec): Invent some form of least recently used eviction scheme.
            uintptr_t index = (reinterpret_cast<uintptr_t>(type) >> 4) & (EQUIVALENT_TYPE_CACHE_SIZE - 1);

            if (cache->nextEvictionVictim == EQUIVALENT_TYPE_CACHE_SIZE)
            {TRACE_IT(51162);
                __analysis_assume(index < EQUIVALENT_TYPE_CACHE_SIZE);
                // If nextEvictionVictim was never set, set it to next element after index
                cache->nextEvictionVictim = (index + 1) & (EQUIVALENT_TYPE_CACHE_SIZE - 1);
            }
            else
            {TRACE_IT(51163);
                Assert(cache->nextEvictionVictim < EQUIVALENT_TYPE_CACHE_SIZE);
                __analysis_assume(cache->nextEvictionVictim < EQUIVALENT_TYPE_CACHE_SIZE);
                equivTypes[cache->nextEvictionVictim] = equivTypes[index];
                // Else, set it to next element after current nextEvictionVictim index
                cache->nextEvictionVictim = (cache->nextEvictionVictim + 1) & (EQUIVALENT_TYPE_CACHE_SIZE - 1);
            }

            if (PHASE_TRACE1(Js::EquivObjTypeSpecPhase))
            {TRACE_IT(51164);
                Output::Print(_u("EquivObjTypeSpec: Saving type in used slot of equiv types cache at index = %d. NextEvictionVictim = %d. \n"), index, cache->nextEvictionVictim);
                Output::Flush();
            }
            Assert(index < EQUIVALENT_TYPE_CACHE_SIZE);
            __analysis_assume(index < EQUIVALENT_TYPE_CACHE_SIZE);
            equivTypes[index] = type;
        }

        // Fixed field checks allow us to assume a specific type ID, but the assumption is only
        // valid if we lock the type. Otherwise, the type ID may change out from under us without
        // evolving the type.
        // We also need to lock the type in case of, for instance, adding a property to a dictionary type handler.
        if (dynamicType != nullptr)
        {TRACE_IT(51165);
            if (!dynamicType->GetIsLocked())
            {TRACE_IT(51166);
                dynamicType->LockType();
            }
        }

        type->SetHasBeenCached();
        guard->SetTypeAddr((intptr_t)type);
        return true;
    }

    void JavascriptOperators::GetPropertyIdForInt(uint64 value, ScriptContext* scriptContext, PropertyRecord const ** propertyRecord)
    {TRACE_IT(51167);
        char16 buffer[20];
        ::_ui64tow_s(value, buffer, sizeof(buffer)/sizeof(char16), 10);
        scriptContext->GetOrAddPropertyRecord(buffer, JavascriptString::GetBufferLength(buffer), propertyRecord);
    }

    void JavascriptOperators::GetPropertyIdForInt(uint32 value, ScriptContext* scriptContext, PropertyRecord const ** propertyRecord)
    {
        GetPropertyIdForInt(static_cast<uint64>(value), scriptContext, propertyRecord);
    }

    Var JavascriptOperators::FromPropertyDescriptor(const PropertyDescriptor& descriptor, ScriptContext* scriptContext)
    {TRACE_IT(51168);
        DynamicObject* object = scriptContext->GetLibrary()->CreateObject();

        // ES5 Section 8.10.4 specifies the order for adding these properties.
        if (descriptor.IsDataDescriptor())
        {TRACE_IT(51169);
            if (descriptor.ValueSpecified())
            {TRACE_IT(51170);
                JavascriptOperators::InitProperty(object, PropertyIds::value, descriptor.GetValue());
            }
            JavascriptOperators::InitProperty(object, PropertyIds::writable, JavascriptBoolean::ToVar(descriptor.IsWritable(),scriptContext));
        }
        else if (descriptor.IsAccessorDescriptor())
        {TRACE_IT(51171);
            JavascriptOperators::InitProperty(object, PropertyIds::get, JavascriptOperators::CanonicalizeAccessor(descriptor.GetGetter(), scriptContext));
            JavascriptOperators::InitProperty(object, PropertyIds::set, JavascriptOperators::CanonicalizeAccessor(descriptor.GetSetter(), scriptContext));
        }

        if (descriptor.EnumerableSpecified())
        {TRACE_IT(51172);
            JavascriptOperators::InitProperty(object, PropertyIds::enumerable, JavascriptBoolean::ToVar(descriptor.IsEnumerable(), scriptContext));
        }
        if (descriptor.ConfigurableSpecified())
        {TRACE_IT(51173);
            JavascriptOperators::InitProperty(object, PropertyIds::configurable, JavascriptBoolean::ToVar(descriptor.IsConfigurable(), scriptContext));
        }
        return object;
    }

    // ES5 8.12.9 [[DefineOwnProperty]].
    // Return value:
    // - TRUE = success.
    // - FALSE (can throw depending on throwOnError parameter) = unsuccessful.
    BOOL JavascriptOperators::DefineOwnPropertyDescriptor(RecyclableObject* obj, PropertyId propId, const PropertyDescriptor& descriptor, bool throwOnError, ScriptContext* scriptContext)
    {TRACE_IT(51174);
        Assert(obj);
        Assert(scriptContext);

        if (JavascriptProxy::Is(obj))
        {TRACE_IT(51175);
            return JavascriptProxy::DefineOwnPropertyDescriptor(obj, propId, descriptor, throwOnError, scriptContext);
        }

        PropertyDescriptor currentDescriptor;
        BOOL isCurrentDescriptorDefined = JavascriptOperators::GetOwnPropertyDescriptor(obj, propId, scriptContext, &currentDescriptor);

        bool isExtensible = !!obj->IsExtensible();
        return ValidateAndApplyPropertyDescriptor<true>(obj, propId, descriptor, isCurrentDescriptorDefined ? &currentDescriptor : nullptr, isExtensible, throwOnError, scriptContext);
    }

    BOOL JavascriptOperators::IsCompatiblePropertyDescriptor(const PropertyDescriptor& descriptor, PropertyDescriptor* currentDescriptor, bool isExtensible, bool throwOnError, ScriptContext* scriptContext)
    {TRACE_IT(51176);
        return ValidateAndApplyPropertyDescriptor<false>(nullptr, Constants::NoProperty, descriptor, currentDescriptor, isExtensible, throwOnError, scriptContext);
    }

    template<bool needToSetProperty>
    BOOL JavascriptOperators::ValidateAndApplyPropertyDescriptor(RecyclableObject* obj, PropertyId propId, const PropertyDescriptor& descriptor,
        PropertyDescriptor* currentDescriptor, bool isExtensible, bool throwOnError, ScriptContext* scriptContext)
    {TRACE_IT(51177);
        Var defaultDataValue = scriptContext->GetLibrary()->GetUndefined();
        Var defaultAccessorValue = scriptContext->GetLibrary()->GetDefaultAccessorFunction();

        if (currentDescriptor == nullptr)
        {TRACE_IT(51178);
            if (!isExtensible) // ES5 8.12.9.3.
            {TRACE_IT(51179);
                return Reject(throwOnError, scriptContext, JSERR_DefineProperty_NotExtensible, propId);
            }
            else // ES5 8.12.9.4.
            {TRACE_IT(51180);
                if (needToSetProperty)
                {TRACE_IT(51181);
                    if (descriptor.IsGenericDescriptor() || descriptor.IsDataDescriptor())
                    {TRACE_IT(51182);
                        // ES5 8.12.9.4a: Create an own data property named P of object O whose [[Value]], [[Writable]],
                        // [[Enumerable]] and [[Configurable]]  attribute values are described by Desc.
                        // If the value of an attribute field of Desc is absent, the attribute of the newly created property
                        // is set to its default value.
                        PropertyDescriptor filledDescriptor = FillMissingPropertyDescriptorFields<false>(descriptor, scriptContext);

                        BOOL tempResult = obj->SetPropertyWithAttributes(propId, filledDescriptor.GetValue(), filledDescriptor.GetAttributes(), nullptr);
                        Assert(tempResult || obj->IsExternal());
                    }
                    else
                    {TRACE_IT(51183);
                        // ES5 8.12.9.4b: Create an own accessor property named P of object O whose [[Get]], [[Set]], [[Enumerable]]
                        // and [[Configurable]] attribute values are described by Desc. If the value of an attribute field of Desc is absent,
                        // the attribute of the newly created property is set to its default value.
                        Assert(descriptor.IsAccessorDescriptor());
                        PropertyDescriptor filledDescriptor = FillMissingPropertyDescriptorFields<true>(descriptor, scriptContext);

                        BOOL isSetAccessorsSuccess = obj->SetAccessors(propId, filledDescriptor.GetGetter(), filledDescriptor.GetSetter());

                        // It is valid for some objects to not-support getters and setters, specifically, for projection of an ABI method
                        // (CustomExternalObject => MapWithStringKey) which SetAccessors returns VBSErr_ActionNotSupported.
                        // But for non-external objects SetAccessors should succeed.
                        Assert(isSetAccessorsSuccess || obj->CanHaveInterceptors());

                        // If SetAccessors failed, the property wasn't created, so no need to change the attributes.
                        if (isSetAccessorsSuccess)
                        {TRACE_IT(51184);
                            JavascriptOperators::SetAttributes(obj, propId, filledDescriptor, true);   // use 'force' as default attributes in type system are different from ES5.
                        }
                    }
                }
                return TRUE;
            }
        }

        // ES5 8.12.9.5: Return true, if every field in Desc is absent.
        if (!descriptor.ConfigurableSpecified() && !descriptor.EnumerableSpecified() && !descriptor.WritableSpecified() &&
            !descriptor.ValueSpecified() && !descriptor.GetterSpecified() && !descriptor.SetterSpecified())
        {TRACE_IT(51185);
            return TRUE;
        }

        // ES5 8.12.9.6: Return true, if every field in Desc also occurs in current and the value of every field in Desc is the same value
        // as the corresponding field in current when compared using the SameValue algorithm (9.12).
        PropertyDescriptor filledDescriptor = descriptor.IsAccessorDescriptor() ? FillMissingPropertyDescriptorFields<true>(descriptor, scriptContext)
            : FillMissingPropertyDescriptorFields<false>(descriptor, scriptContext);
        if (JavascriptOperators::AreSamePropertyDescriptors(&filledDescriptor, currentDescriptor, scriptContext))
        {TRACE_IT(51186);
            return TRUE;
        }

        if (!currentDescriptor->IsConfigurable()) // ES5 8.12.9.7.
        {TRACE_IT(51187);
            if (descriptor.ConfigurableSpecified() && descriptor.IsConfigurable())
            {TRACE_IT(51188);
                return Reject(throwOnError, scriptContext, JSERR_DefineProperty_NotConfigurable, propId);
            }
            if (descriptor.EnumerableSpecified() && descriptor.IsEnumerable() != currentDescriptor->IsEnumerable())
            {TRACE_IT(51189);
                return Reject(throwOnError, scriptContext, JSERR_DefineProperty_NotConfigurable, propId);
            }
        }

        // Whether to merge attributes from tempDescriptor into descriptor to keep original values
        // of some attributes from the object/use tempDescriptor for SetAttributes, or just use descriptor.
        // This is optimization to avoid 2 calls to SetAttributes.
        bool mergeDescriptors = false;

        // Whether to call SetAttributes with 'force' flag which forces setting all attributes
        // rather than only specified or which have true values.
        // This is to make sure that the object has correct attributes, as default values in the object are not for ES5.
        bool forceSetAttributes = false;

        PropertyDescriptor tempDescriptor;

        // ES5 8.12.9.8: If IsGenericDescriptor(Desc) is true, then no further validation is required.
        if (!descriptor.IsGenericDescriptor())
        {TRACE_IT(51190);
            if (currentDescriptor->IsDataDescriptor() != descriptor.IsDataDescriptor())
            {TRACE_IT(51191);
                // ES5 8.12.9.9: Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) have different results...
                if (!currentDescriptor->IsConfigurable())
                {TRACE_IT(51192);
                    return Reject(throwOnError, scriptContext, JSERR_DefineProperty_NotConfigurable, propId);
                }

                if (needToSetProperty)
                {TRACE_IT(51193);
                    if (currentDescriptor->IsDataDescriptor())
                    {TRACE_IT(51194);
                        // ES5 8.12.9.9.b: Convert the property named P of object O from a data property to an accessor property.
                        // Preserve the existing values of the converted property's [[Configurable]] and [[Enumerable]] attributes
                        // and set the rest of the property's attributes to their default values.
                        PropertyAttributes preserveFromObject = currentDescriptor->GetAttributes() & (PropertyConfigurable | PropertyEnumerable);

                        BOOL isSetAccessorsSuccess = obj->SetAccessors(propId, defaultAccessorValue, defaultAccessorValue);

                        // It is valid for some objects to not-support getters and setters, specifically, for projection of an ABI method
                        // (CustomExternalObject => MapWithStringKey) which SetAccessors returns VBSErr_ActionNotSupported.
                        // But for non-external objects SetAccessors should succeed.
                        Assert(isSetAccessorsSuccess || obj->CanHaveInterceptors());

                        if (isSetAccessorsSuccess)
                        {TRACE_IT(51195);
                            tempDescriptor.SetAttributes(preserveFromObject, PropertyConfigurable | PropertyEnumerable);
                            forceSetAttributes = true;  // use SetAttrbiutes with 'force' as default attributes in type system are different from ES5.
                            mergeDescriptors = true;
                        }
                    }
                    else
                    {TRACE_IT(51196);
                        // ES5 8.12.9.9.c: Convert the property named P of object O from an accessor property to a data property.
                        // Preserve the existing values of the converted property's [[Configurable]] and [[Enumerable]] attributes
                        // and set the rest of the property's attributes to their default values.
                        // Note: avoid using SetProperty/SetPropertyWithAttributes here because they has undesired side-effects:
                        //       it calls previous setter and in some cases of attribute values throws.
                        //       To walk around, call DeleteProperty and then AddProperty.
                        PropertyAttributes preserveFromObject = currentDescriptor->GetAttributes() & (PropertyConfigurable | PropertyEnumerable);

                        tempDescriptor.SetAttributes(preserveFromObject, PropertyConfigurable | PropertyEnumerable);
                        tempDescriptor.MergeFrom(descriptor);   // Update only fields specified in 'descriptor'.
                        Var descriptorValue = descriptor.ValueSpecified() ? descriptor.GetValue() : defaultDataValue;

                        // Note: HostDispath'es implementation of DeleteProperty currently throws E_NOTIMPL.
                        obj->DeleteProperty(propId, PropertyOperation_None);
                        BOOL tempResult = obj->SetPropertyWithAttributes(propId, descriptorValue, tempDescriptor.GetAttributes(), NULL, PropertyOperation_Force);
                        Assert(tempResult);

                        // At this time we already set value and attributes to desired values,
                        // thus we can skip step ES5 8.12.9.12 and simply return true.
                        return TRUE;
                    }
                }
            }
            else if (currentDescriptor->IsDataDescriptor() && descriptor.IsDataDescriptor())
            {TRACE_IT(51197);
                // ES5 8.12.9.10: Else, if IsDataDescriptor(current) and IsDataDescriptor(Desc) are both true...
                if (!currentDescriptor->IsConfigurable())
                {TRACE_IT(51198);
                    if (!currentDescriptor->IsWritable())
                    {TRACE_IT(51199);
                        if ((descriptor.WritableSpecified() && descriptor.IsWritable()) ||  // ES5 8.12.9.10.a.i
                            (descriptor.ValueSpecified() &&
                                !JavascriptConversion::SameValue(descriptor.GetValue(), currentDescriptor->GetValue()))) // ES5 8.12.9.10.a.ii
                        {TRACE_IT(51200);
                            return Reject(throwOnError, scriptContext, JSERR_DefineProperty_NotWritable, propId);
                        }
                    }
                }
                // ES5 8.12.9.10.b: else, the [[Configurable]] field of current is true, so any change is acceptable.
            }
            else
            {TRACE_IT(51201);
                // ES5 8.12.9.11: Else, IsAccessorDescriptor(current) and IsAccessorDescriptor(Desc) are both true, so...
                Assert(currentDescriptor->IsAccessorDescriptor() && descriptor.IsAccessorDescriptor());
                if (!currentDescriptor->IsConfigurable())
                {TRACE_IT(51202);
                    if ((descriptor.SetterSpecified() &&
                            !JavascriptConversion::SameValue(
                            JavascriptOperators::CanonicalizeAccessor(descriptor.GetSetter(), scriptContext),
                                JavascriptOperators::CanonicalizeAccessor(currentDescriptor->GetSetter(), scriptContext))) ||
                        (descriptor.GetterSpecified() &&
                            !JavascriptConversion::SameValue(
                            JavascriptOperators::CanonicalizeAccessor(descriptor.GetGetter(), scriptContext),
                                JavascriptOperators::CanonicalizeAccessor(currentDescriptor->GetGetter(), scriptContext))))
                    {TRACE_IT(51203);
                        return Reject(throwOnError, scriptContext, JSERR_DefineProperty_NotConfigurable, propId);
                    }
                }
            }

            // This part is only for non-generic descriptors:
            //   ES5 8.12.9.12: For each attribute field of Desc that is present,
            //   set the correspondingly named attribute of the property named P of object O to the value of the field.
            if (descriptor.IsDataDescriptor())
            {TRACE_IT(51204);
                if (descriptor.ValueSpecified() && needToSetProperty)
                {TRACE_IT(51205);
                    // Set just the value by passing the current attributes of the property.
                    // If the property's attributes are also changing (perhaps becoming non-writable),
                    // this will be taken care of in the call to JavascriptOperators::SetAttributes below.
                    // Built-in Function.prototype properties 'length', 'arguments', and 'caller' are special cases.
                    BOOL tempResult = obj->SetPropertyWithAttributes(propId, descriptor.GetValue(), currentDescriptor->GetAttributes(), nullptr);
                    AssertMsg(tempResult || JavascriptFunction::IsBuiltinProperty(obj, propId), "If you hit this assert, most likely there is something wrong with the object/type.");
                }
            }
            else if (descriptor.IsAccessorDescriptor() && needToSetProperty)
            {TRACE_IT(51206);
                Assert(descriptor.GetterSpecified() || descriptor.SetterSpecified());
                Var oldGetter = defaultAccessorValue, oldSetter = defaultAccessorValue;
                if (!descriptor.GetterSpecified() || !descriptor.SetterSpecified())
                {TRACE_IT(51207);
                    // Unless both getter and setter are specified, make sure we don't overwrite old accessor.
                    obj->GetAccessors(propId, &oldGetter, &oldSetter, scriptContext);
                }

                Var getter = descriptor.GetterSpecified() ? descriptor.GetGetter() : oldGetter;
                Var setter = descriptor.SetterSpecified() ? descriptor.GetSetter() : oldSetter;

                obj->SetAccessors(propId, getter, setter);
            }
        } // if (!descriptor.IsGenericDescriptor())

        // Continue for all descriptors including generic:
        //   ES5 8.12.9.12: For each attribute field of Desc that is present,
        //   set the correspondingly named attribute of the property named P of object O to the value of the field.
        if (needToSetProperty)
        {TRACE_IT(51208);
            if (mergeDescriptors)
            {TRACE_IT(51209);
                tempDescriptor.MergeFrom(descriptor);
                JavascriptOperators::SetAttributes(obj, propId, tempDescriptor, forceSetAttributes);
            }
            else
            {TRACE_IT(51210);
                JavascriptOperators::SetAttributes(obj, propId, descriptor, forceSetAttributes);
            }
        }
        return TRUE;
    }

    template <bool isAccessor>
    PropertyDescriptor JavascriptOperators::FillMissingPropertyDescriptorFields(PropertyDescriptor descriptor, ScriptContext* scriptContext)
    {TRACE_IT(51211);
        PropertyDescriptor newDescriptor;
        const PropertyDescriptor* defaultDescriptor = scriptContext->GetLibrary()->GetDefaultPropertyDescriptor();
        if (isAccessor)
        {TRACE_IT(51212);
            newDescriptor.SetGetter(descriptor.GetterSpecified() ? descriptor.GetGetter() : defaultDescriptor->GetGetter());
            newDescriptor.SetSetter(descriptor.SetterSpecified() ? descriptor.GetSetter() : defaultDescriptor->GetSetter());
        }
        else
        {TRACE_IT(51213);
            newDescriptor.SetValue(descriptor.ValueSpecified() ? descriptor.GetValue() : defaultDescriptor->GetValue());
            newDescriptor.SetWritable(descriptor.WritableSpecified() ? descriptor.IsWritable() : defaultDescriptor->IsWritable());
        }
        newDescriptor.SetConfigurable(descriptor.ConfigurableSpecified() ? descriptor.IsConfigurable() : defaultDescriptor->IsConfigurable());
        newDescriptor.SetEnumerable(descriptor.EnumerableSpecified() ? descriptor.IsEnumerable() : defaultDescriptor->IsEnumerable());
        return newDescriptor;
    }
    // ES5: 15.4.5.1
    BOOL JavascriptOperators::DefineOwnPropertyForArray(JavascriptArray* arr, PropertyId propId, const PropertyDescriptor& descriptor, bool throwOnError, ScriptContext* scriptContext)
    {TRACE_IT(51214);
        if (propId == PropertyIds::length)
        {TRACE_IT(51215);
            if (!descriptor.ValueSpecified())
            {TRACE_IT(51216);
                return DefineOwnPropertyDescriptor(arr, PropertyIds::length, descriptor, throwOnError, scriptContext);
            }

            PropertyDescriptor newLenDesc = descriptor;
            uint32 newLen = ES5Array::ToLengthValue(descriptor.GetValue(), scriptContext);
            newLenDesc.SetValue(JavascriptNumber::ToVar(newLen, scriptContext));

            uint32 oldLen = arr->GetLength();
            if (newLen >= oldLen)
            {TRACE_IT(51217);
                return DefineOwnPropertyDescriptor(arr, PropertyIds::length, newLenDesc, throwOnError, scriptContext);
            }

            BOOL oldLenWritable = arr->IsWritable(PropertyIds::length);
            if (!oldLenWritable)
            {TRACE_IT(51218);
                return Reject(throwOnError, scriptContext, JSERR_DefineProperty_NotWritable, propId);
            }

            bool newWritable = (!newLenDesc.WritableSpecified() || newLenDesc.IsWritable());
            if (!newWritable)
            {TRACE_IT(51219);
                // Need to defer setting writable to false in case any elements cannot be deleted
                newLenDesc.SetWritable(true);
            }

            BOOL succeeded = DefineOwnPropertyDescriptor(arr, PropertyIds::length, newLenDesc, throwOnError, scriptContext);
            //
            // Our SetProperty(length) is also responsible to trim elements. When succeeded is
            //
            //  false:
            //      * length attributes rejected
            //      * elements not touched
            //  true:
            //      * length attributes are set successfully
            //      * elements trimming may be either completed or incompleted, length value is correct
            //
            //      * Strict mode TODO: Currently SetProperty(length) does not throw. If that throws, we need
            //        to update here to set correct newWritable even on exception.
            //
            if (!succeeded)
            {TRACE_IT(51220);
                return false;
            }

            if (!newWritable) // Now set requested newWritable.
            {TRACE_IT(51221);
                PropertyDescriptor newWritableDesc;
                newWritableDesc.SetWritable(false);
                DefineOwnPropertyDescriptor(arr, PropertyIds::length, newWritableDesc, false, scriptContext);
            }

            if (arr->GetLength() > newLen) // Delete incompleted
            {TRACE_IT(51222);
                // Since SetProperty(length) not throwing, we'll reject here
                return Reject(throwOnError, scriptContext, JSERR_DefineProperty_Default, propId);
            }

            return true;
        }

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propId, &index))
        {TRACE_IT(51223);
            if (index >= arr->GetLength() && !arr->IsWritable(PropertyIds::length))
            {TRACE_IT(51224);
                return Reject(throwOnError, scriptContext, JSERR_DefineProperty_LengthNotWritable, propId);
            }

            BOOL succeeded = DefineOwnPropertyDescriptor(arr, propId, descriptor, false, scriptContext);
            if (!succeeded)
            {TRACE_IT(51225);
                return Reject(throwOnError, scriptContext, JSERR_DefineProperty_Default, propId);
            }

            // Out SetItem takes care of growing "length". we are done.
            return true;
        }

        return DefineOwnPropertyDescriptor(arr, propId, descriptor, throwOnError, scriptContext);
    }

    BOOL JavascriptOperators::SetPropertyDescriptor(RecyclableObject* object, PropertyId propId, const PropertyDescriptor& descriptor)
    {TRACE_IT(51226);
        if (descriptor.ValueSpecified())
        {TRACE_IT(51227);
            ScriptContext* requestContext = object->GetScriptContext(); // Real requestContext?
            JavascriptOperators::SetProperty(object, object, propId, descriptor.GetValue(), requestContext);
        }
        else if (descriptor.GetterSpecified() || descriptor.SetterSpecified())
        {TRACE_IT(51228);
            JavascriptOperators::SetAccessors(object, propId, descriptor.GetGetter(), descriptor.GetSetter());
        }

        if (descriptor.EnumerableSpecified())
        {TRACE_IT(51229);
            object->SetEnumerable(propId, descriptor.IsEnumerable());
        }
        if (descriptor.ConfigurableSpecified())
        {TRACE_IT(51230);
            object->SetConfigurable(propId, descriptor.IsConfigurable());
        }
        if (descriptor.WritableSpecified())
        {TRACE_IT(51231);
            object->SetWritable(propId, descriptor.IsWritable());
        }

        return true;
    }

    BOOL JavascriptOperators::ToPropertyDescriptorForProxyObjects(Var propertySpec, PropertyDescriptor* descriptor, ScriptContext* scriptContext)
    {TRACE_IT(51232);
        if (!JavascriptOperators::IsObject(propertySpec))
        {TRACE_IT(51233);
            return FALSE;
        }

        Var value;
        RecyclableObject* propertySpecObj = RecyclableObject::FromVar(propertySpec);

        if (JavascriptOperators::HasProperty(propertySpecObj, PropertyIds::enumerable) == TRUE)
        {TRACE_IT(51234);
            if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::enumerable, &value, scriptContext))
            {TRACE_IT(51235);
                descriptor->SetEnumerable(JavascriptConversion::ToBoolean(value, scriptContext) ? true : false);
            }
            else
            {TRACE_IT(51236);
                // The proxy said we have the property, so we try to read the property and get the default value.
                descriptor->SetEnumerable(false);
            }
        }

        if (JavascriptOperators::HasProperty(propertySpecObj, PropertyIds::configurable) == TRUE)
        {TRACE_IT(51237);
            if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::configurable, &value, scriptContext))
            {TRACE_IT(51238);
                descriptor->SetConfigurable(JavascriptConversion::ToBoolean(value, scriptContext) ? true : false);
            }
            else
            {TRACE_IT(51239);
                // The proxy said we have the property, so we try to read the property and get the default value.
                descriptor->SetConfigurable(false);
            }
        }

        if (JavascriptOperators::HasProperty(propertySpecObj, PropertyIds::value) == TRUE)
        {TRACE_IT(51240);
            if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::value, &value, scriptContext))
            {TRACE_IT(51241);
                descriptor->SetValue(value);
            }
            else
            {TRACE_IT(51242);
                // The proxy said we have the property, so we try to read the property and get the default value.
                descriptor->SetValue(scriptContext->GetLibrary()->GetUndefined());
            }
        }

        if (JavascriptOperators::HasProperty(propertySpecObj, PropertyIds::writable) == TRUE)
        {TRACE_IT(51243);
            if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::writable, &value, scriptContext))
            {TRACE_IT(51244);
                descriptor->SetWritable(JavascriptConversion::ToBoolean(value, scriptContext) ? true : false);
            }
            else
            {TRACE_IT(51245);
                // The proxy said we have the property, so we try to read the property and get the default value.
                descriptor->SetWritable(false);
            }
        }

        if (JavascriptOperators::HasProperty(propertySpecObj, PropertyIds::get) == TRUE)
        {TRACE_IT(51246);
            if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::get, &value, scriptContext))
            {TRACE_IT(51247);
                if (JavascriptOperators::GetTypeId(value) != TypeIds_Undefined && (false == JavascriptConversion::IsCallable(value)))
                {TRACE_IT(51248);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, scriptContext->GetPropertyName(PropertyIds::get)->GetBuffer());
                }
                descriptor->SetGetter(value);
            }
            else
            {TRACE_IT(51249);
                // The proxy said we have the property, so we try to read the property and get the default value.
                descriptor->SetGetter(scriptContext->GetLibrary()->GetUndefined());
            }
        }

        if (JavascriptOperators::HasProperty(propertySpecObj, PropertyIds::set) == TRUE)
        {TRACE_IT(51250);
            if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::set, &value, scriptContext))
            {TRACE_IT(51251);
                if (JavascriptOperators::GetTypeId(value) != TypeIds_Undefined && (false == JavascriptConversion::IsCallable(value)))
                {TRACE_IT(51252);
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, scriptContext->GetPropertyName(PropertyIds::set)->GetBuffer());
                }
                descriptor->SetSetter(value);
            }
            else
            {TRACE_IT(51253);
                // The proxy said we have the property, so we try to read the property and get the default value.
                descriptor->SetSetter(scriptContext->GetLibrary()->GetUndefined());
            }
        }

        return TRUE;
    }

    BOOL JavascriptOperators::ToPropertyDescriptorForGenericObjects(Var propertySpec, PropertyDescriptor* descriptor, ScriptContext* scriptContext)
    {TRACE_IT(51254);
        if (!JavascriptOperators::IsObject(propertySpec))
        {TRACE_IT(51255);
            return FALSE;
        }

        Var value;
        RecyclableObject* propertySpecObj = RecyclableObject::FromVar(propertySpec);

        if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::enumerable, &value, scriptContext))
        {TRACE_IT(51256);
            descriptor->SetEnumerable(JavascriptConversion::ToBoolean(value, scriptContext) ? true : false);
        }

        if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::configurable, &value, scriptContext))
        {TRACE_IT(51257);
            descriptor->SetConfigurable(JavascriptConversion::ToBoolean(value, scriptContext) ? true : false);
        }

        if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::value, &value, scriptContext))
        {TRACE_IT(51258);
            descriptor->SetValue(value);
        }

        if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::writable, &value, scriptContext))
        {TRACE_IT(51259);
            descriptor->SetWritable(JavascriptConversion::ToBoolean(value, scriptContext) ? true : false);
        }

        if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::get, &value, scriptContext))
        {TRACE_IT(51260);
            if (JavascriptOperators::GetTypeId(value) != TypeIds_Undefined && (false == JavascriptConversion::IsCallable(value)))
            {TRACE_IT(51261);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, scriptContext->GetPropertyName(PropertyIds::get)->GetBuffer());
            }
            descriptor->SetGetter(value);
        }

        if (JavascriptOperators::GetProperty(propertySpecObj, PropertyIds::set, &value, scriptContext))
        {TRACE_IT(51262);
            if (JavascriptOperators::GetTypeId(value) != TypeIds_Undefined && (false == JavascriptConversion::IsCallable(value)))
            {TRACE_IT(51263);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, scriptContext->GetPropertyName(PropertyIds::set)->GetBuffer());
            }
            descriptor->SetSetter(value);
        }

        return TRUE;
    }

    BOOL JavascriptOperators::ToPropertyDescriptor(Var propertySpec, PropertyDescriptor* descriptor, ScriptContext* scriptContext)
    {TRACE_IT(51264);
        if (JavascriptProxy::Is(propertySpec) || (
            RecyclableObject::Is(propertySpec) &&
            JavascriptOperators::CheckIfPrototypeChainContainsProxyObject(RecyclableObject::FromVar(propertySpec)->GetPrototype())))
        {
            if (ToPropertyDescriptorForProxyObjects(propertySpec, descriptor, scriptContext) == FALSE)
            {TRACE_IT(51265);
                return FALSE;
            }
        }
        else
        {
            if (ToPropertyDescriptorForGenericObjects(propertySpec, descriptor, scriptContext) == FALSE)
            {TRACE_IT(51266);
                return FALSE;
            }
        }

        if (descriptor->GetterSpecified() || descriptor->SetterSpecified())
        {TRACE_IT(51267);
            if (descriptor->ValueSpecified())
            {TRACE_IT(51268);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_CannotHaveAccessorsAndValue);
            }
            if (descriptor->WritableSpecified())
            {TRACE_IT(51269);
                int32 hCode = descriptor->IsWritable() ? JSERR_InvalidAttributeTrue : JSERR_InvalidAttributeFalse;
                JavascriptError::ThrowTypeError(scriptContext, hCode, _u("writable"));
            }
        }

        descriptor->SetOriginal(propertySpec);

        return TRUE;
    }

    void JavascriptOperators::CompletePropertyDescriptor(PropertyDescriptor* resultDescriptor, PropertyDescriptor* likeDescriptor, ScriptContext* requestContext)
    {TRACE_IT(51270);
        const PropertyDescriptor* likePropertyDescriptor = likeDescriptor;
        //    1. Assert: LikeDesc is either a Property Descriptor or undefined.
        //    2. ReturnIfAbrupt(Desc).
        //    3. Assert : Desc is a Property Descriptor
        //    4. If LikeDesc is undefined, then set LikeDesc to Record{ [[Value]]: undefined, [[Writable]] : false, [[Get]] : undefined, [[Set]] : undefined, [[Enumerable]] : false, [[Configurable]] : false }.
        if (likePropertyDescriptor == nullptr)
        {TRACE_IT(51271);
            likePropertyDescriptor = requestContext->GetLibrary()->GetDefaultPropertyDescriptor();
        }
        //    5. If either IsGenericDescriptor(Desc) or IsDataDescriptor(Desc) is true, then
        if (resultDescriptor->IsDataDescriptor() || resultDescriptor->IsGenericDescriptor())
        {TRACE_IT(51272);
            //    a.If Desc does not have a[[Value]] field, then set Desc.[[Value]] to LikeDesc.[[Value]].
            //    b.If Desc does not have a[[Writable]] field, then set Desc.[[Writable]] to LikeDesc.[[Writable]].
            if (!resultDescriptor->ValueSpecified())
            {TRACE_IT(51273);
                resultDescriptor->SetValue(likePropertyDescriptor->GetValue());
            }
            if (!resultDescriptor->WritableSpecified())
            {TRACE_IT(51274);
                resultDescriptor->SetWritable(likePropertyDescriptor->IsWritable());
            }
        }
        else
        {TRACE_IT(51275);
            //    6. Else,
            //    a.If Desc does not have a[[Get]] field, then set Desc.[[Get]] to LikeDesc.[[Get]].
            //    b.If Desc does not have a[[Set]] field, then set Desc.[[Set]] to LikeDesc.[[Set]].
            if (!resultDescriptor->GetterSpecified())
            {TRACE_IT(51276);
                resultDescriptor->SetGetter(likePropertyDescriptor->GetGetter());
            }
            if (!resultDescriptor->SetterSpecified())
            {TRACE_IT(51277);
                resultDescriptor->SetSetter(likePropertyDescriptor->GetSetter());
            }
        }
        //    7. If Desc does not have an[[Enumerable]] field, then set Desc.[[Enumerable]] to LikeDesc.[[Enumerable]].
        //    8. If Desc does not have a[[Configurable]] field, then set Desc.[[Configurable]] to LikeDesc.[[Configurable]].
        //    9. Return Desc.
        if (!resultDescriptor->EnumerableSpecified())
        {TRACE_IT(51278);
            resultDescriptor->SetEnumerable(likePropertyDescriptor->IsEnumerable());
        }
        if (!resultDescriptor->ConfigurableSpecified())
        {TRACE_IT(51279);
            resultDescriptor->SetConfigurable(likePropertyDescriptor->IsConfigurable());
        }
    }

    Var JavascriptOperators::OP_InvokePut(Js::ScriptContext *scriptContext, Var instance, CallInfo callInfo, ...)
    {
        // Handle a store to a call result: x(y) = z.
        // This is not strictly permitted in JScript, but some scripts expect to be able to use
        // the syntax to set properties of ActiveX objects.
        // We handle this by deferring to a virtual method of type. This incurs an extra level of
        // indirection but seems preferable to adding the "put" method as a member of every type
        // and using the normal JScript calling mechanism.

        RUNTIME_ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Missing this argument in InvokePut");

        if (TaggedNumber::Is(instance))
        {TRACE_IT(51280);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction /* TODO-ERROR: get arg name - aFunc */);
        }
        RecyclableObject* function = RecyclableObject::FromVar(instance);
        return function->InvokePut(args);
    }

    // Conformance to: ES5 8.6.1.
    // Set attributes on the object as provided by property descriptor.
    // If force parameter is true, we force SetAttributes call even if none of the attributes are defined by the descriptor.
    // NOTE: does not set [[Get]], [Set]], [[Value]]
    void JavascriptOperators::SetAttributes(RecyclableObject* object, PropertyId propId, const PropertyDescriptor& descriptor, bool force)
    {TRACE_IT(51281);
        Assert(object);

        BOOL isWritable = FALSE;
        if (descriptor.IsDataDescriptor())
        {TRACE_IT(51282);
            isWritable = descriptor.WritableSpecified() ? descriptor.IsWritable() : FALSE;
        }
        else if (descriptor.IsAccessorDescriptor())
        {TRACE_IT(51283);
            // The reason is that JavascriptOperators::OP_SetProperty checks for RecyclableObject::FromVar(instance)->IsWritableOrAccessor(propertyId),
            // which should in fact check for 'is writable or accessor' but since there is no GetAttributes, we can't do that efficiently.
            isWritable = TRUE;
        }

        // CONSIDER: call object->SetAttributes which is much more efficient as that's 1 call instead of 3.
        //       Can't do that now as object->SetAttributes doesn't provide a way which attributes to modify and which not.
        if (force || descriptor.ConfigurableSpecified())
        {TRACE_IT(51284);
            object->SetConfigurable(propId, descriptor.ConfigurableSpecified() ? descriptor.IsConfigurable() : FALSE);
        }
        if (force || descriptor.EnumerableSpecified())
        {TRACE_IT(51285);
            object->SetEnumerable(propId, descriptor.EnumerableSpecified() ? descriptor.IsEnumerable() : FALSE);
        }
        if (force || descriptor.WritableSpecified() || isWritable)
        {TRACE_IT(51286);
            object->SetWritable(propId, isWritable);
        }
    }

    void JavascriptOperators::OP_ClearAttributes(Var instance, PropertyId propertyId)
    {TRACE_IT(51287);
        Assert(instance);

        if (RecyclableObject::Is(instance))
        {TRACE_IT(51288);
            RecyclableObject* obj = RecyclableObject::FromVar(instance);
            obj->SetAttributes(propertyId, PropertyNone);
        }
    }

    void JavascriptOperators::OP_Freeze(Var instance)
    {TRACE_IT(51289);
        Assert(instance);

        if (RecyclableObject::Is(instance))
        {TRACE_IT(51290);
            RecyclableObject* obj = RecyclableObject::FromVar(instance);
            obj->Freeze();
        }
    }

    BOOL JavascriptOperators::Reject(bool throwOnError, ScriptContext* scriptContext, int32 errorCode, PropertyId propertyId)
    {TRACE_IT(51291);
        Assert(scriptContext);

        if (throwOnError)
        {TRACE_IT(51292);
            JavascriptError::ThrowTypeError(scriptContext, errorCode, scriptContext->GetThreadContext()->GetPropertyName(propertyId)->GetBuffer());
        }
        return FALSE;
    }

    bool JavascriptOperators::AreSamePropertyDescriptors(const PropertyDescriptor* x, const PropertyDescriptor* y, ScriptContext* scriptContext)
    {TRACE_IT(51293);
        Assert(scriptContext);

        if (x->ConfigurableSpecified() != y->ConfigurableSpecified() || x->IsConfigurable() != y->IsConfigurable() ||
            x->EnumerableSpecified() != y->EnumerableSpecified() || x->IsEnumerable() != y->IsEnumerable())
        {TRACE_IT(51294);
            return false;
        }

        if (x->IsDataDescriptor())
        {TRACE_IT(51295);
            if (!y->IsDataDescriptor() || x->WritableSpecified() != y->WritableSpecified() || x->IsWritable() != y->IsWritable())
            {TRACE_IT(51296);
                return false;
            }

            if (x->ValueSpecified())
            {TRACE_IT(51297);
                if (!y->ValueSpecified() || !JavascriptConversion::SameValue(x->GetValue(), y->GetValue()))
                {TRACE_IT(51298);
                    return false;
                }
            }
        }
        else if (x->IsAccessorDescriptor())
        {TRACE_IT(51299);
            if (!y->IsAccessorDescriptor())
            {TRACE_IT(51300);
                return false;
            }

            if (x->GetterSpecified())
            {TRACE_IT(51301);
                if (!y->GetterSpecified() || !JavascriptConversion::SameValue(
                    JavascriptOperators::CanonicalizeAccessor(x->GetGetter(), scriptContext),
                    JavascriptOperators::CanonicalizeAccessor(y->GetGetter(), scriptContext)))
                {TRACE_IT(51302);
                    return false;
                }
            }

            if (x->SetterSpecified())
            {TRACE_IT(51303);
                if (!y->SetterSpecified() || !JavascriptConversion::SameValue(
                    JavascriptOperators::CanonicalizeAccessor(x->GetSetter(), scriptContext),
                    JavascriptOperators::CanonicalizeAccessor(y->GetSetter(), scriptContext)))
                {TRACE_IT(51304);
                    return false;
                }
            }
        }

        return true;
    }

    // Check if an accessor is undefined (null or defaultAccessor)
    bool JavascriptOperators::IsUndefinedAccessor(Var accessor, ScriptContext* scriptContext)
    {TRACE_IT(51305);
        return nullptr == accessor || scriptContext->GetLibrary()->GetDefaultAccessorFunction() == accessor;
    }

    // Converts default accessor to undefined.
    // Can be used when comparing accessors.
    Var JavascriptOperators::CanonicalizeAccessor(Var accessor, ScriptContext* scriptContext)
    {TRACE_IT(51306);
        Assert(scriptContext);

        if (IsUndefinedAccessor(accessor, scriptContext))
        {TRACE_IT(51307);
            return scriptContext->GetLibrary()->GetUndefined();
        }
        return accessor;
    }

    Var JavascriptOperators::DefaultAccessor(RecyclableObject* function, CallInfo callInfo, ...)
    {
        return function->GetLibrary()->GetUndefined();
    }

    void FrameDisplay::SetItem(uint index, void* item)
    {TRACE_IT(51308);
        AssertMsg(index < this->length, "Invalid frame display access");

        scopes[index] = item;
    }

    void *FrameDisplay::GetItem(uint index)
    {TRACE_IT(51309);
        AssertMsg(index < this->length, "Invalid frame display access");

        return scopes[index];
    }

    // Grab the "this" pointer, mapping a root object to its associated host object.
    Var JavascriptOperators::RootToThisObject(const Var object, ScriptContext* scriptContext)
    {TRACE_IT(51310);
        Js::Var thisVar = object;
        TypeId typeId = Js::JavascriptOperators::GetTypeId(thisVar);

        switch (typeId)
        {
        case Js::TypeIds_GlobalObject:
            return ((Js::GlobalObject*)thisVar)->ToThis();

        case Js::TypeIds_ModuleRoot:
            return Js::JavascriptOperators::GetThisFromModuleRoot(thisVar);

        default:
            if (typeId == scriptContext->GetDirectHostTypeId())
            {TRACE_IT(51311);
                return ((RecyclableObject*)thisVar)->GetLibrary()->GetGlobalObject()->ToThis();
            }

        }

        return thisVar;
    }

    Var JavascriptOperators::CallGetter(RecyclableObject * const function, Var const object, ScriptContext * requestContext)
    {TRACE_IT(51312);
#if ENABLE_TTD
        if(function->GetScriptContext()->ShouldSuppressGetterInvocationForDebuggerEvaluation())
        {TRACE_IT(51313);
            return requestContext->GetLibrary()->GetUndefined();
        }
#endif

        ScriptContext * scriptContext = function->GetScriptContext();
        ThreadContext * threadContext = scriptContext->GetThreadContext();
        return threadContext->ExecuteImplicitCall(function, ImplicitCall_Accessor, [=]() -> Js::Var
        {
            // Stack object should have a pre-op bail on implicit call.  We shouldn't see them here.
            // Stack numbers are ok, as we will call ToObject to wrap it in a number object anyway
            // See JavascriptOperators::GetThisHelper
            Assert(JavascriptOperators::GetTypeId(object) == TypeIds_Integer ||
                JavascriptOperators::GetTypeId(object) == TypeIds_Number ||
                threadContext->HasNoSideEffect(function) ||
                !ThreadContext::IsOnStack(object));

            // Verify that the scriptcontext is alive before firing getter/setter
            if (!scriptContext->VerifyAlive(!function->IsExternal(), requestContext))
            {TRACE_IT(51314);
                return nullptr;
            }
            CallFlags flags = CallFlags_Value;

            Var thisVar = RootToThisObject(object, scriptContext);

            RecyclableObject* marshalledFunction = RecyclableObject::FromVar(CrossSite::MarshalVar(requestContext, function));
            Var result = CALL_ENTRYPOINT(marshalledFunction->GetEntryPoint(), function, CallInfo(flags, 1), thisVar);
            result = CrossSite::MarshalVar(requestContext, result);

            return result;
        });
    }

    void JavascriptOperators::CallSetter(RecyclableObject * const function, Var const  object, Var const value, ScriptContext * requestContext)
    {TRACE_IT(51315);
        ScriptContext * scriptContext = function->GetScriptContext();
        ThreadContext * threadContext = scriptContext->GetThreadContext();
        threadContext->ExecuteImplicitCall(function, ImplicitCall_Accessor, [=]() -> Js::Var
        {
            // Stack object should have a pre-op bail on implicit call.  We shouldn't see them here.
            // Stack numbers are ok, as we will call ToObject to wrap it in a number object anyway
            // See JavascriptOperators::GetThisHelper
            Assert(JavascriptOperators::GetTypeId(object) == TypeIds_Integer ||
                JavascriptOperators::GetTypeId(object) == TypeIds_Number || !ThreadContext::IsOnStack(object));

            // Verify that the scriptcontext is alive before firing getter/setter
            if (!scriptContext->VerifyAlive(!function->IsExternal(), requestContext))
            {TRACE_IT(51316);
                return nullptr;
            }

            CallFlags flags = CallFlags_Value;
            Var putValue = value;

            // CONSIDER: Have requestContext everywhere, even in the setProperty related codepath.
            if (requestContext)
            {TRACE_IT(51317);
                putValue = CrossSite::MarshalVar(requestContext, value);
            }

            Var thisVar = RootToThisObject(object, scriptContext);

            RecyclableObject* marshalledFunction = function;
            if (requestContext)
            {TRACE_IT(51318);
                marshalledFunction = RecyclableObject::FromVar(CrossSite::MarshalVar(requestContext, function));
            }

            Var result = CALL_ENTRYPOINT(marshalledFunction->GetEntryPoint(), function, CallInfo(flags, 2), thisVar, putValue);
            Assert(result);
            return nullptr;
        });
    }

    void * JavascriptOperators::AllocMemForVarArray(size_t size, Recycler* recycler)
    {
        TRACK_ALLOC_INFO(recycler, Js::Var, Recycler, 0, (size_t)(size / sizeof(Js::Var)));
        return recycler->AllocZero(size);
    }

    void * JavascriptOperators::AllocUninitializedNumber(Js::RecyclerJavascriptNumberAllocator * allocator)
    {TRACE_IT(51319);
        TRACK_ALLOC_INFO(allocator->GetRecycler(), Js::JavascriptNumber, Recycler, 0, (size_t)-1);
        return allocator->Alloc(sizeof(Js::JavascriptNumber));
    }

    void JavascriptOperators::ScriptAbort()
    {TRACE_IT(51320);
        throw ScriptAbortException();
    }

    void PolymorphicInlineCache::Finalize(bool isShutdown)
    {TRACE_IT(51321);
        if (size == 0)
        {TRACE_IT(51322);
            // Already finalized
            Assert(!inlineCaches && !prev && !next);
            return;
        }

        uint unregisteredInlineCacheCount = 0;

        Assert(inlineCaches && size > 0);

        // If we're not shutting down (as in closing the script context), we need to remove our inline caches from
        // thread context's invalidation lists, and release memory back to the arena.  During script context shutdown,
        // we leave everything in place, because the inline cache arena will stay alive until script context is destroyed
        // (as in destructor has been called) and thus the invalidation lists are safe to keep references to caches from this
        // script context.  We will, however, zero all inline caches so that we don't have to process them on subsequent
        // collections, which may still happen from other script contexts.
        if (isShutdown)
        {
            memset(inlineCaches, 0, size * sizeof(InlineCache));
        }
        else
        {TRACE_IT(51323);
            for (int i = 0; i < size; i++)
            {TRACE_IT(51324);
                if (inlineCaches[i].RemoveFromInvalidationList())
                {TRACE_IT(51325);
                    unregisteredInlineCacheCount++;
                }
            }

            AllocatorDeleteArray(InlineCacheAllocator, functionBody->GetScriptContext()->GetInlineCacheAllocator(), size, inlineCaches);
#ifdef POLY_INLINE_CACHE_SIZE_STATS
            functionBody->GetScriptContext()->GetInlineCacheAllocator()->LogPolyCacheFree(size * sizeof(InlineCache));
#endif
        }

        // Remove this PolymorphicInlineCache from the list
        if (this == functionBody->GetPolymorphicInlineCachesHead())
        {TRACE_IT(51326);
            Assert(!prev);
            if (next)
            {TRACE_IT(51327);
                Assert(next->prev == this);
                next->prev = nullptr;
            }
            functionBody->SetPolymorphicInlineCachesHead(next);
        }
        else
        {TRACE_IT(51328);
            if (prev)
            {TRACE_IT(51329);
                Assert(prev->next == this);
                prev->next = next;
            }
            if (next)
            {TRACE_IT(51330);
                Assert(next->prev == this);
                next->prev = prev;
            }
        }
        prev = next = nullptr;
        inlineCaches = nullptr;
        size = 0;
        if (unregisteredInlineCacheCount > 0)
        {TRACE_IT(51331);
            functionBody->GetScriptContext()->GetThreadContext()->NotifyInlineCacheBatchUnregistered(unregisteredInlineCacheCount);
        }
    }

    JavascriptString * JavascriptOperators::Concat3(Var aLeft, Var aCenter, Var aRight, ScriptContext * scriptContext)
    {TRACE_IT(51332);
        // Make sure we do the conversion in order from left to right
        JavascriptString * strLeft = JavascriptConversion::ToPrimitiveString(aLeft, scriptContext);
        JavascriptString * strCenter = JavascriptConversion::ToPrimitiveString(aCenter, scriptContext);
        JavascriptString * strRight = JavascriptConversion::ToPrimitiveString(aRight, scriptContext);
        return JavascriptString::Concat3(strLeft, strCenter, strRight);
    }

    JavascriptString *
    JavascriptOperators::NewConcatStrMulti(Var a1, Var a2, uint count, ScriptContext * scriptContext)
    {TRACE_IT(51333);
        // Make sure we do the conversion in order
        JavascriptString * str1 = JavascriptConversion::ToPrimitiveString(a1, scriptContext);
        JavascriptString * str2 = JavascriptConversion::ToPrimitiveString(a2, scriptContext);
        return ConcatStringMulti::New(count, str1, str2, scriptContext);
    }

    void
    JavascriptOperators::SetConcatStrMultiItem(Var concatStr, Var str, uint index, ScriptContext * scriptContext)
    {TRACE_IT(51334);
        ConcatStringMulti::FromVar(concatStr)->SetItem(index,
            JavascriptConversion::ToPrimitiveString(str, scriptContext));
    }

    void
    JavascriptOperators::SetConcatStrMultiItem2(Var concatStr, Var str1, Var str2, uint index, ScriptContext * scriptContext)
    {TRACE_IT(51335);
        ConcatStringMulti * cs = ConcatStringMulti::FromVar(concatStr);
        cs->SetItem(index, JavascriptConversion::ToPrimitiveString(str1, scriptContext));
        cs->SetItem(index + 1, JavascriptConversion::ToPrimitiveString(str2, scriptContext));
    }

    void JavascriptOperators::OP_SetComputedNameVar(Var method, Var computedNameVar)
    {TRACE_IT(51336);
        ScriptFunctionBase *scriptFunction = ScriptFunctionBase::FromVar(method);
        scriptFunction->SetComputedNameVar(computedNameVar);
    }

    void JavascriptOperators::OP_SetHomeObj(Var method, Var homeObj)
    {TRACE_IT(51337);
        ScriptFunctionBase *scriptFunction = ScriptFunctionBase::FromVar(method);
        scriptFunction->SetHomeObj(homeObj);
    }

    Var JavascriptOperators::OP_LdHomeObj(Var scriptFunction, ScriptContext * scriptContext)
    {TRACE_IT(51338);
        // Ensure this is not a stack ScriptFunction
        if (!ScriptFunction::Is(scriptFunction) || ThreadContext::IsOnStack(scriptFunction))
        {TRACE_IT(51339);
            return scriptContext->GetLibrary()->GetUndefined();
        }

        ScriptFunction *instance = ScriptFunction::FromVar(scriptFunction);

        // We keep a reference to the current class rather than its super prototype
        // since the prototype could change.
        Var homeObj = instance->GetHomeObj();

        return (homeObj != nullptr) ? homeObj : scriptContext->GetLibrary()->GetUndefined();
    }

    Var JavascriptOperators::OP_LdHomeObjProto(Var homeObj, ScriptContext* scriptContext)
    {TRACE_IT(51340);
        if (homeObj == nullptr || !RecyclableObject::Is(homeObj))
        {TRACE_IT(51341);
            return scriptContext->GetLibrary()->GetUndefined();
        }

        RecyclableObject *thisObjPrototype = RecyclableObject::FromVar(homeObj);

        TypeId typeId = thisObjPrototype->GetTypeId();

        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {TRACE_IT(51342);
            JavascriptError::ThrowReferenceError(scriptContext, JSERR_BadSuperReference);
        }

        Assert(thisObjPrototype != nullptr);

        RecyclableObject *superBase = thisObjPrototype->GetPrototype();

        if (superBase == nullptr || !RecyclableObject::Is(superBase))
        {TRACE_IT(51343);
            return scriptContext->GetLibrary()->GetUndefined();
        }

        return superBase;
    }

    Var JavascriptOperators::OP_LdFuncObj(Var scriptFunction, ScriptContext * scriptContext)
    {TRACE_IT(51344);
        // use self as value of [[FunctionObject]] - this is true only for constructors

        Assert(RecyclableObject::Is(scriptFunction));

        return scriptFunction;
    }

    Var JavascriptOperators::OP_LdFuncObjProto(Var funcObj, ScriptContext* scriptContext)
    {TRACE_IT(51345);
        RecyclableObject *superCtor = RecyclableObject::FromVar(funcObj)->GetPrototype();

        if (superCtor == nullptr || !IsConstructor(superCtor))
        {TRACE_IT(51346);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NotAConstructor);
        }

        return superCtor;
    }

    Var JavascriptOperators::ScopedLdHomeObjFuncObjHelper(Var scriptFunction, Js::PropertyId propertyId, ScriptContext * scriptContext)
    {TRACE_IT(51347);
        ScriptFunction *instance = ScriptFunction::FromVar(scriptFunction);
        Var superRef = nullptr;

        FrameDisplay *frameDisplay = instance->GetEnvironment();

        if (frameDisplay->GetLength() == 0)
        {TRACE_IT(51348);
            // Globally scoped evals are a syntax error
            JavascriptError::ThrowSyntaxError(scriptContext, ERRSuperInGlobalEval, _u("super"));
        }

        // Iterate over the scopes in the FrameDisplay, looking for the super property.
        for (unsigned i = 0; i < frameDisplay->GetLength(); ++i)
        {TRACE_IT(51349);
            void *currScope = frameDisplay->GetItem(i);
            if (RecyclableObject::Is(currScope))
            {TRACE_IT(51350);
                if (BlockActivationObject::Is(currScope))
                {TRACE_IT(51351);
                    // We won't find super in a block scope.
                    continue;
                }

                RecyclableObject *recyclableObject = RecyclableObject::FromVar(currScope);
                if (GetProperty(recyclableObject, propertyId, &superRef, scriptContext))
                {TRACE_IT(51352);
                    return superRef;
                }

                if (HasProperty(recyclableObject, Js::PropertyIds::_lexicalThisSlotSymbol))
                {TRACE_IT(51353);
                    // If we reach 'this' and haven't found the super reference, we don't need to look any further.
                    JavascriptError::ThrowReferenceError(scriptContext, JSERR_BadSuperReference, _u("super"));
                }
            }
        }

        // We didn't find a super reference. Emit a reference error.
        JavascriptError::ThrowReferenceError(scriptContext, JSERR_BadSuperReference, _u("super"));
    }

    Var JavascriptOperators::OP_ScopedLdHomeObj(Var scriptFunction, ScriptContext * scriptContext)
    {TRACE_IT(51354);
        return JavascriptOperators::ScopedLdHomeObjFuncObjHelper(scriptFunction, Js::PropertyIds::_superReferenceSymbol, scriptContext);
    }

    Var JavascriptOperators::OP_ScopedLdFuncObj(Var scriptFunction, ScriptContext * scriptContext)
    {TRACE_IT(51355);
        return JavascriptOperators::ScopedLdHomeObjFuncObjHelper(scriptFunction, Js::PropertyIds::_superCtorReferenceSymbol, scriptContext);
    }

    Var JavascriptOperators::OP_ResumeYield(ResumeYieldData* yieldData, RecyclableObject* iterator)
    {TRACE_IT(51356);
        bool isNext = yieldData->exceptionObj == nullptr;
        bool isThrow = !isNext && !yieldData->exceptionObj->IsGeneratorReturnException();

        if (iterator != nullptr) // yield*
        {TRACE_IT(51357);
            ScriptContext* scriptContext = iterator->GetScriptContext();
            PropertyId propertyId = isNext ? PropertyIds::next : isThrow ? PropertyIds::throw_ : PropertyIds::return_;
            Var prop = JavascriptOperators::GetProperty(iterator, propertyId, scriptContext);

            if (!isNext && JavascriptOperators::IsUndefinedOrNull(prop))
            {TRACE_IT(51358);
                if (isThrow)
                {TRACE_IT(51359);
                    // 5.b.iii.2
                    // NOTE: If iterator does not have a throw method, this throw is going to terminate the yield* loop.
                    // But first we need to give iterator a chance to clean up.

                    prop = JavascriptOperators::GetProperty(iterator, PropertyIds::return_, scriptContext);
                    if (!JavascriptOperators::IsUndefinedOrNull(prop))
                    {TRACE_IT(51360);
                        if (!JavascriptConversion::IsCallable(prop))
                        {TRACE_IT(51361);
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, _u("return"));
                        }

                        RecyclableObject* method = RecyclableObject::FromVar(prop);
                        Var args[] = { iterator, yieldData->data };
                        CallInfo callInfo(CallFlags_Value, _countof(args));
                        Var result = JavascriptFunction::CallFunction<true>(method, method->GetEntryPoint(), Arguments(callInfo, args));

                        if (!JavascriptOperators::IsObject(result))
                        {TRACE_IT(51362);
                            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
                        }
                    }

                    // 5.b.iii.3
                    // NOTE: The next step throws a TypeError to indicate that there was a yield* protocol violation:
                    // iterator does not have a throw method.
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, _u("throw"));
                }

                // Do not use ThrowExceptionObject for return() API exceptions since these exceptions are not real exceptions
                JavascriptExceptionOperators::DoThrow(yieldData->exceptionObj, scriptContext);
            }

            if (!JavascriptConversion::IsCallable(prop))
            {TRACE_IT(51363);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, isNext ? _u("next") : isThrow ? _u("throw") : _u("return"));
            }

            RecyclableObject* method = RecyclableObject::FromVar(prop);
            Var args[] = { iterator, yieldData->data };
            CallInfo callInfo(CallFlags_Value, _countof(args));
            Var result = JavascriptFunction::CallFunction<true>(method, method->GetEntryPoint(), Arguments(callInfo, args));

            if (!JavascriptOperators::IsObject(result))
            {TRACE_IT(51364);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
            }

            if (isThrow || isNext)
            {TRACE_IT(51365);
                // 5.b.ii.2
                // NOTE: Exceptions from the inner iterator throw method are propagated.
                // Normal completions from an inner throw method are processed similarly to an inner next.
                return result;
            }

            RecyclableObject* obj = RecyclableObject::FromVar(result);
            Var done = JavascriptOperators::GetProperty(obj, PropertyIds::done, scriptContext);
            if (done == iterator->GetLibrary()->GetTrue())
            {TRACE_IT(51366);
                Var value = JavascriptOperators::GetProperty(obj, PropertyIds::value, scriptContext);
                yieldData->exceptionObj->SetThrownObject(value);
                // Do not use ThrowExceptionObject for return() API exceptions since these exceptions are not real exceptions
                JavascriptExceptionOperators::DoThrow(yieldData->exceptionObj, scriptContext);
            }
            return result;
        }

        // CONSIDER: Fast path this early out return path in JITed code before helper call to avoid the helper call overhead in the common case e.g. next() calls.
        if (isNext)
        {TRACE_IT(51367);
            return yieldData->data;
        }

        if (isThrow)
        {TRACE_IT(51368);
            // Use ThrowExceptionObject() to get debugger support for breaking on throw
            JavascriptExceptionOperators::ThrowExceptionObject(yieldData->exceptionObj, yieldData->exceptionObj->GetScriptContext(), true);
        }

        // CONSIDER: Using an exception to carry the return value and force finally code to execute is a bit of a janky
        // solution since we have to override the value here in the case of yield* expressions.  It works but is there
        // a more elegant way?
        //
        // Instead what if ResumeYield was a "set Dst then optionally branch" opcode, that could also throw? Then we could
        // avoid using a special exception entirely with byte code something like this:
        //
        // ;; Ry is the yieldData
        //
        // ResumeYield Rx Ry $returnPathLabel
        // ... code like normal
        // $returnPathLabel:
        // Ld_A R0 Rx
        // Br $exitFinallyAndReturn
        //
        // This would probably give better performance for the common case of calling next() on generators since we wouldn't
        // have to wrap the call to the generator code in a try catch.

        // Do not use ThrowExceptionObject for return() API exceptions since these exceptions are not real exceptions
        JavascriptExceptionOperators::DoThrow(yieldData->exceptionObj, yieldData->exceptionObj->GetScriptContext());
    }

    Js::Var
    JavascriptOperators::BoxStackInstance(Js::Var instance, ScriptContext * scriptContext, bool allowStackFunction)
    {TRACE_IT(51369);
        if (!ThreadContext::IsOnStack(instance) || (allowStackFunction && !TaggedNumber::Is(instance) && (*(int*)instance & 1)))
        {TRACE_IT(51370);
            return instance;
        }

        TypeId typeId = JavascriptOperators::GetTypeId(instance);
        switch (typeId)
        {
        case Js::TypeIds_Number:
#if !FLOATVAR
            return JavascriptNumber::BoxStackInstance(instance, scriptContext);
#endif
            // fall-through
        case Js::TypeIds_Integer:
            return instance;
        case Js::TypeIds_RegEx:
            return JavascriptRegExp::BoxStackInstance(JavascriptRegExp::FromVar(instance));
        case Js::TypeIds_Object:
            return DynamicObject::BoxStackInstance(DynamicObject::FromVar(instance));
        case Js::TypeIds_Array:
            return JavascriptArray::BoxStackInstance(JavascriptArray::FromVar(instance));
        case Js::TypeIds_NativeIntArray:
            return JavascriptNativeIntArray::BoxStackInstance(JavascriptNativeIntArray::FromVar(instance));
        case Js::TypeIds_NativeFloatArray:
            return JavascriptNativeFloatArray::BoxStackInstance(JavascriptNativeFloatArray::FromVar(instance));
        case Js::TypeIds_Function:
            Assert(allowStackFunction);
            // Stack functions are deal with not mar mark them, but by nested function escape analysis
            // in the front end.  No need to box here.
            return instance;
#if ENABLE_COPYONACCESS_ARRAY
        case Js::TypeIds_CopyOnAccessNativeIntArray:
            Assert(false);
            // fall-through
#endif
        default:
            Assert(false);
            return instance;
        };
    }
    ImplicitCallFlags
    JavascriptOperators::CacheAndClearImplicitBit(ScriptContext* scriptContext)
    {TRACE_IT(51371);
        ImplicitCallFlags prevImplicitCallFlags = scriptContext->GetThreadContext()->GetImplicitCallFlags();
        scriptContext->GetThreadContext()->ClearImplicitCallFlags();
        return prevImplicitCallFlags;
    }
    ImplicitCallFlags
    JavascriptOperators::CheckAndUpdateFunctionBodyWithImplicitFlag(FunctionBody* functionBody)
    {TRACE_IT(51372);
        ScriptContext* scriptContext = functionBody->GetScriptContext();
        ImplicitCallFlags currImplicitCallFlags = scriptContext->GetThreadContext()->GetImplicitCallFlags();
        if ((currImplicitCallFlags > ImplicitCall_None))
        {TRACE_IT(51373);
            functionBody->SetHasOnlyThisStmts(false);
        }
        return currImplicitCallFlags;
    }
    void
    JavascriptOperators::RestoreImplicitFlag(ScriptContext* scriptContext, ImplicitCallFlags prevImplicitCallFlags, ImplicitCallFlags currImplicitCallFlags)
    {TRACE_IT(51374);
        scriptContext->GetThreadContext()->SetImplicitCallFlags((ImplicitCallFlags)(prevImplicitCallFlags | currImplicitCallFlags));
    }

    FunctionProxy*
    JavascriptOperators::GetDeferredDeserializedFunctionProxy(JavascriptFunction* func)
    {TRACE_IT(51375);
        FunctionProxy* proxy = func->GetFunctionProxy();
        Assert(proxy->GetFunctionInfo()->GetFunctionProxy() != proxy);
        return proxy;
    }

    template <>
    Js::Var JavascriptOperators::GetElementAtIndex(Js::JavascriptArray* arrayObject, UINT index, Js::ScriptContext* scriptContext)
    {TRACE_IT(51376);
        Js::Var result;
        if (Js::JavascriptOperators::OP_GetElementI_ArrayFastPath(arrayObject, index, &result, scriptContext))
        {TRACE_IT(51377);
            return result;
        }
        return scriptContext->GetMissingItemResult();
    }

    template<>
    Js::Var JavascriptOperators::GetElementAtIndex(Js::JavascriptNativeIntArray* arrayObject, UINT index, Js::ScriptContext* scriptContext)
    {TRACE_IT(51378);
        Js::Var result;
        if (Js::JavascriptOperators::OP_GetElementI_ArrayFastPath(arrayObject, index, &result, scriptContext))
        {TRACE_IT(51379);
            return result;
        }
        return scriptContext->GetMissingItemResult();
    }

    template<>
    Js::Var JavascriptOperators::GetElementAtIndex(Js::JavascriptNativeFloatArray* arrayObject, UINT index, Js::ScriptContext* scriptContext)
    {TRACE_IT(51380);
        Js::Var result;
        if (Js::JavascriptOperators::OP_GetElementI_ArrayFastPath(arrayObject, index, &result, scriptContext))
        {TRACE_IT(51381);
            return result;
        }
        return scriptContext->GetMissingItemResult();
    }

    template<>
    Js::Var JavascriptOperators::GetElementAtIndex(Js::Var* arrayObject, UINT index, Js::ScriptContext* scriptContext)
    {TRACE_IT(51382);
        return Js::JavascriptOperators::OP_GetElementI_Int32(*arrayObject, index, scriptContext);
    }

    template<typename T>
    void JavascriptOperators::ObjectToNativeArray(T* arrayObject,
        JsNativeValueType valueType,
        __in UINT length,
        __in UINT elementSize,
        __out_bcount(length*elementSize) byte* buffer,
        Js::ScriptContext* scriptContext)
    {TRACE_IT(51383);
        Var element;
        uint64 allocSize = length * elementSize;

        // TODO:further fast path the call for things like IntArray convert to int, floatarray convert to float etc.
        // such that we don't need boxing.
        switch (valueType)
        {
        case JsInt8Type:
            AnalysisAssert(elementSize == sizeof(int8));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51384);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(int8) <= allocSize);
#pragma prefast(suppress:22102)
                ((int8*)buffer)[i] = Js::JavascriptConversion::ToInt8(element, scriptContext);
            }
            break;
        case JsUint8Type:
            AnalysisAssert(elementSize == sizeof(uint8));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51385);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(uint8) <= allocSize);
                ((uint8*)buffer)[i] = Js::JavascriptConversion::ToUInt8(element, scriptContext);
            }
            break;
        case JsInt16Type:
            AnalysisAssert(elementSize == sizeof(int16));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51386);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(int16) <= allocSize);
                ((int16*)buffer)[i] = Js::JavascriptConversion::ToInt16(element, scriptContext);
            }
            break;
        case JsUint16Type:
            AnalysisAssert(elementSize == sizeof(uint16));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51387);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(uint16) <= allocSize);
                ((uint16*)buffer)[i] = Js::JavascriptConversion::ToUInt16(element, scriptContext);
            }
            break;
        case JsInt32Type:
            AnalysisAssert(elementSize == sizeof(int32));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51388);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(int32) <= allocSize);
                ((int32*)buffer)[i] = Js::JavascriptConversion::ToInt32(element, scriptContext);
            }
            break;
        case JsUint32Type:
            AnalysisAssert(elementSize == sizeof(uint32));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51389);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(uint32) <= allocSize);
                ((uint32*)buffer)[i] = Js::JavascriptConversion::ToUInt32(element, scriptContext);
            }
            break;
        case JsInt64Type:
            AnalysisAssert(elementSize == sizeof(int64));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51390);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(int64) <= allocSize);
                ((int64*)buffer)[i] = Js::JavascriptConversion::ToInt64(element, scriptContext);
            }
            break;
        case JsUint64Type:
            AnalysisAssert(elementSize == sizeof(uint64));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51391);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(uint64) <= allocSize);
                ((uint64*)buffer)[i] = Js::JavascriptConversion::ToUInt64(element, scriptContext);
            }
            break;
        case JsFloatType:
            AnalysisAssert(elementSize == sizeof(float));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51392);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(float) <= allocSize);
                ((float*)buffer)[i] = Js::JavascriptConversion::ToFloat(element, scriptContext);
            }
            break;
        case JsDoubleType:
            AnalysisAssert(elementSize == sizeof(double));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51393);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(double) <= allocSize);
                ((double*)buffer)[i] = Js::JavascriptConversion::ToNumber(element, scriptContext);
            }
            break;
        case JsNativeStringType:
            AnalysisAssert(elementSize == sizeof(JsNativeString));
            for (UINT i = 0; i < length; i++)
            {TRACE_IT(51394);
                element = GetElementAtIndex(arrayObject, i, scriptContext);
                AnalysisAssert((i + 1) * sizeof(JsNativeString) <= allocSize);
                Js::JavascriptString* string = Js::JavascriptConversion::ToString(element, scriptContext);
                (((JsNativeString*)buffer)[i]).str = string->GetSz();
                (((JsNativeString*)buffer)[i]).length = string->GetLength();
            }
            break;
        default:
            Assert(FALSE);
        }
    }

    void JavascriptOperators::VarToNativeArray(Var arrayObject,
        JsNativeValueType valueType,
        __in UINT length,
        __in UINT elementSize,
        __out_bcount(length*elementSize) byte* buffer,
        Js::ScriptContext* scriptContext)
    {TRACE_IT(51395);
        Js::DynamicObject* dynamicObject = DynamicObject::FromVar(arrayObject);
        if (dynamicObject->IsCrossSiteObject() || Js::TaggedInt::IsOverflow(length))
        {TRACE_IT(51396);
            Js::JavascriptOperators::ObjectToNativeArray(&arrayObject, valueType, length, elementSize, buffer, scriptContext);
        }
        else
        {TRACE_IT(51397);
#if ENABLE_COPYONACCESS_ARRAY
            JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayObject);
#endif
            switch (Js::JavascriptOperators::GetTypeId(arrayObject))
            {
            case TypeIds_Array:
                Js::JavascriptOperators::ObjectToNativeArray(Js::JavascriptArray::FromVar(arrayObject), valueType, length, elementSize, buffer, scriptContext);
                break;
            case TypeIds_NativeFloatArray:
                Js::JavascriptOperators::ObjectToNativeArray(Js::JavascriptNativeFloatArray::FromVar(arrayObject), valueType, length, elementSize, buffer, scriptContext);
                break;
            case TypeIds_NativeIntArray:
                Js::JavascriptOperators::ObjectToNativeArray(Js::JavascriptNativeIntArray::FromVar(arrayObject), valueType, length, elementSize, buffer, scriptContext);
                break;
                // We can have more specialized template if needed.
            default:
                Js::JavascriptOperators::ObjectToNativeArray(&arrayObject, valueType, length, elementSize, buffer, scriptContext);
            }
        }
    }

    // SpeciesConstructor abstract operation as described in ES6.0 Section 7.3.20
    Var JavascriptOperators::SpeciesConstructor(RecyclableObject* object, Var defaultConstructor, ScriptContext* scriptContext)
    {TRACE_IT(51398);
        //1.Assert: Type(O) is Object.
        Assert(JavascriptOperators::IsObject(object));

        //2.Let C be Get(O, "constructor").
        //3.ReturnIfAbrupt(C).
        Var constructor = JavascriptOperators::GetProperty(object, PropertyIds::constructor, scriptContext);

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {TRACE_IT(51399);
            //4.If C is undefined, return defaultConstructor.
            if (JavascriptOperators::IsUndefinedObject(constructor))
            {TRACE_IT(51400);
                return defaultConstructor;
            }
            //5.If Type(C) is not Object, throw a TypeError exception.
            if (!JavascriptOperators::IsObject(constructor))
            {TRACE_IT(51401);
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject, _u("[constructor]"));
            }
            //6.Let S be Get(C, @@species).
            //7.ReturnIfAbrupt(S).
            Var species = nullptr;
            if (!JavascriptOperators::GetProperty(RecyclableObject::FromVar(constructor), PropertyIds::_symbolSpecies, &species, scriptContext)
                || JavascriptOperators::IsUndefinedOrNull(species))
            {TRACE_IT(51402);
                //8.If S is either undefined or null, return defaultConstructor.
                return defaultConstructor;
            }
            constructor = species;
        }
        //9.If IsConstructor(S) is true, return S.
        if (JavascriptOperators::IsConstructor(constructor))
        {TRACE_IT(51403);
            return constructor;
        }
        //10.Throw a TypeError exception.
        JavascriptError::ThrowTypeError(scriptContext, JSERR_NotAConstructor, _u("constructor[Symbol.species]"));
    }

    BOOL JavascriptOperators::GreaterEqual(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51404);
        if (TaggedInt::Is(aLeft))
        {TRACE_IT(51405);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(51406);
                // Works whether it is TaggedInt31 or TaggedInt32
                return ::Math::PointerCastToIntegralTruncate<int>(aLeft) >= ::Math::PointerCastToIntegralTruncate<int>(aRight);
            }
            if (JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(51407);
                return TaggedInt::ToDouble(aLeft) >= JavascriptNumber::GetValue(aRight);
            }
        }
        else if (TaggedInt::Is(aRight))
        {TRACE_IT(51408);
            if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft))
            {TRACE_IT(51409);
                return JavascriptNumber::GetValue(aLeft) >= TaggedInt::ToDouble(aRight);
            }
        }
        else
        {TRACE_IT(51410);
            if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft) && JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(51411);
                return JavascriptNumber::GetValue(aLeft) >= JavascriptNumber::GetValue(aRight);
            }
        }

        return !RelationalComparisonHelper(aLeft, aRight, scriptContext, true, true);
    }

    BOOL JavascriptOperators::LessEqual(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51412);
        if (TaggedInt::Is(aLeft))
        {TRACE_IT(51413);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(51414);
                // Works whether it is TaggedInt31 or TaggedInt32
                return ::Math::PointerCastToIntegralTruncate<int>(aLeft) <= ::Math::PointerCastToIntegralTruncate<int>(aRight);
            }

            if (JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(51415);
                return TaggedInt::ToDouble(aLeft) <= JavascriptNumber::GetValue(aRight);
            }
        }
        else if (TaggedInt::Is(aRight))
        {TRACE_IT(51416);
            if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft))
            {TRACE_IT(51417);
                return JavascriptNumber::GetValue(aLeft) <= TaggedInt::ToDouble(aRight);
            }
        }
        else
        {TRACE_IT(51418);
            if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft) && JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(51419);
                return JavascriptNumber::GetValue(aLeft) <= JavascriptNumber::GetValue(aRight);
            }
        }

        return !RelationalComparisonHelper(aRight, aLeft, scriptContext, false, true);
    }

    BOOL JavascriptOperators::NotEqual(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51420);
        //
        // TODO: Change to use Abstract Equality Comparison Algorithm (ES3.0: S11.9.3):
        // - Evaluate left, then right, operands to preserve correct evaluation order.
        // - Call algorithm, potentially reversing arguments.
        //

        return !Equal(aLeft, aRight, scriptContext);
    }

    // NotStrictEqual() returns whether the two vars have strict equality, as
    // described in (ES3.0: S11.9.5, S11.9.6).
    BOOL JavascriptOperators::NotStrictEqual(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51421);
        return !StrictEqual(aLeft, aRight, scriptContext);
    }

    bool JavascriptOperators::CheckIfObjectAndPrototypeChainHasOnlyWritableDataProperties(RecyclableObject* object)
    {TRACE_IT(51422);
        Assert(object);
        if (object->GetType()->HasSpecialPrototype())
        {TRACE_IT(51423);
            TypeId typeId = object->GetTypeId();
            if (typeId == TypeIds_Null)
            {TRACE_IT(51424);
                return true;
            }
            if (typeId == TypeIds_Proxy)
            {TRACE_IT(51425);
                return false;
            }
        }
        if (!object->HasOnlyWritableDataProperties())
        {TRACE_IT(51426);
            return false;
        }
        return CheckIfPrototypeChainHasOnlyWritableDataProperties(object->GetPrototype());
    }

    bool JavascriptOperators::CheckIfPrototypeChainHasOnlyWritableDataProperties(RecyclableObject* prototype)
    {TRACE_IT(51427);
        Assert(prototype);

        if (prototype->GetType()->AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties())
        {TRACE_IT(51428);
            Assert(DoCheckIfPrototypeChainHasOnlyWritableDataProperties(prototype));
            return true;
        }
        return DoCheckIfPrototypeChainHasOnlyWritableDataProperties(prototype);
    }

    // Does a quick check to see if the specified object (which should be a prototype object) and all objects in its prototype
    // chain have only writable data properties (i.e. no accessors or non-writable properties).
    bool JavascriptOperators::DoCheckIfPrototypeChainHasOnlyWritableDataProperties(RecyclableObject* prototype)
    {TRACE_IT(51429);
        Assert(prototype);

        Type *const originalType = prototype->GetType();
        ScriptContext *const scriptContext = prototype->GetScriptContext();
        bool onlyOneScriptContext = true;
        TypeId typeId;
        for (; (typeId = prototype->GetTypeId()) != TypeIds_Null; prototype = prototype->GetPrototype())
        {TRACE_IT(51430);
            if (typeId == TypeIds_Proxy)
            {TRACE_IT(51431);
                return false;
            }
            if (!prototype->HasOnlyWritableDataProperties())
            {TRACE_IT(51432);
                return false;
            }
            if (prototype->GetScriptContext() != scriptContext)
            {TRACE_IT(51433);
                onlyOneScriptContext = false;
            }
        }

        if (onlyOneScriptContext)
        {TRACE_IT(51434);
            // See JavascriptLibrary::typesEnsuredToHaveOnlyWritableDataPropertiesInItAndPrototypeChain for a description of
            // this cache. Technically, we could register all prototypes in the chain but this is good enough for now.
            originalType->SetAreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties(true);
        }

        return true;
    }

    // Checks to see if the specified object (which should be a prototype object)
    // contains a proxy anywhere in the prototype chain.
    bool JavascriptOperators::CheckIfPrototypeChainContainsProxyObject(RecyclableObject* prototype)
    {TRACE_IT(51435);
        if (prototype == nullptr)
        {TRACE_IT(51436);
            return false;
        }

        Assert(JavascriptOperators::IsObjectOrNull(prototype));

        while (prototype->GetTypeId() != TypeIds_Null)
        {TRACE_IT(51437);
            if (prototype->GetTypeId() == TypeIds_Proxy)
            {TRACE_IT(51438);
                return true;
            }

            prototype = prototype->GetPrototype();
        }

        return false;
    }

    BOOL JavascriptOperators::Equal(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51439);
        if (aLeft == aRight)
        {TRACE_IT(51440);
            if (TaggedInt::Is(aLeft) || JavascriptObject::Is(aLeft))
            {TRACE_IT(51441);
                return true;
            }
            else
            {TRACE_IT(51442);
                return Equal_Full(aLeft, aRight, scriptContext);
            }
        }

        if (JavascriptString::Is(aLeft) && JavascriptString::Is(aRight))
        {TRACE_IT(51443);
            JavascriptString* left = (JavascriptString*)aLeft;
            JavascriptString* right = (JavascriptString*)aRight;

            if (left->GetLength() == right->GetLength())
            {TRACE_IT(51444);
                if (left->UnsafeGetBuffer() != NULL && right->UnsafeGetBuffer() != NULL)
                {TRACE_IT(51445);
                    if (left->GetLength() == 1)
                    {TRACE_IT(51446);
                        return left->UnsafeGetBuffer()[0] == right->UnsafeGetBuffer()[0];
                    }
                    return memcmp(left->UnsafeGetBuffer(), right->UnsafeGetBuffer(), left->GetLength() * sizeof(left->UnsafeGetBuffer()[0])) == 0;
                }
                // fall through to Equal_Full
            }
            else
            {TRACE_IT(51447);
                return false;
            }
        }

        return Equal_Full(aLeft, aRight, scriptContext);
    }

    BOOL JavascriptOperators::Greater(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51448);
        if (TaggedInt::Is(aLeft))
        {TRACE_IT(51449);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(51450);
                // Works whether it is TaggedInt31 or TaggedInt32
                return ::Math::PointerCastToIntegralTruncate<int>(aLeft) > ::Math::PointerCastToIntegralTruncate<int>(aRight);
            }
            if (JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(51451);
                return TaggedInt::ToDouble(aLeft) > JavascriptNumber::GetValue(aRight);
            }
        }
        else if (TaggedInt::Is(aRight))
        {TRACE_IT(51452);
            if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft))
            {TRACE_IT(51453);
                return JavascriptNumber::GetValue(aLeft) > TaggedInt::ToDouble(aRight);
            }
        }
        else
        {TRACE_IT(51454);
            if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft) && JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(51455);
                return JavascriptNumber::GetValue(aLeft) > JavascriptNumber::GetValue(aRight);
            }
        }

        return Greater_Full(aLeft, aRight, scriptContext);
    }

    BOOL JavascriptOperators::Less(Var aLeft, Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51456);
        if (TaggedInt::Is(aLeft))
        {TRACE_IT(51457);
            if (TaggedInt::Is(aRight))
            {TRACE_IT(51458);
                // Works whether it is TaggedInt31 or TaggedInt32
                return ::Math::PointerCastToIntegralTruncate<int>(aLeft) < ::Math::PointerCastToIntegralTruncate<int>(aRight);
            }
            if (JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(51459);
                return TaggedInt::ToDouble(aLeft) < JavascriptNumber::GetValue(aRight);
            }
        }
        else if (TaggedInt::Is(aRight))
        {TRACE_IT(51460);
            if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft))
            {TRACE_IT(51461);
                return JavascriptNumber::GetValue(aLeft) < TaggedInt::ToDouble(aRight);
            }
        }
        else
        {TRACE_IT(51462);
            if (JavascriptNumber::Is_NoTaggedIntCheck(aLeft) && JavascriptNumber::Is_NoTaggedIntCheck(aRight))
            {TRACE_IT(51463);
                return JavascriptNumber::GetValue(aLeft) < JavascriptNumber::GetValue(aRight);
            }
        }

        return Less_Full(aLeft, aRight, scriptContext);
    }

    Var JavascriptOperators::ToObject(Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51464);
        RecyclableObject* object = nullptr;
        if (FALSE == JavascriptConversion::ToObject(aRight, scriptContext, &object))
        {TRACE_IT(51465);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject /* TODO-ERROR: get arg name - aValue */);
        }

        return object;
    }

    Var JavascriptOperators::ToWithObject(Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51466);
        RecyclableObject* object = RecyclableObject::FromVar(aRight);

        WithScopeObject* withWrapper = RecyclerNew(scriptContext->GetRecycler(), WithScopeObject, object, scriptContext->GetLibrary()->GetWithType());
        return withWrapper;
    }

    Var JavascriptOperators::ToNumber(Var aRight, ScriptContext* scriptContext)
    {TRACE_IT(51467);
        if (TaggedInt::Is(aRight) || (JavascriptNumber::Is_NoTaggedIntCheck(aRight)))
        {TRACE_IT(51468);
            return aRight;
        }

        return JavascriptNumber::ToVarNoCheck(JavascriptConversion::ToNumber_Full(aRight, scriptContext), scriptContext);
    }

    BOOL JavascriptOperators::IsObject(Var aValue)
    {TRACE_IT(51469);
        return GetTypeId(aValue) > TypeIds_LastJavascriptPrimitiveType;
    }

    BOOL JavascriptOperators::IsObjectType(TypeId typeId)
    {TRACE_IT(51470);
        return typeId > TypeIds_LastJavascriptPrimitiveType;
    }

    BOOL JavascriptOperators::IsExposedType(TypeId typeId)
    {TRACE_IT(51471);
        return typeId <= TypeIds_LastTrueJavascriptObjectType && typeId != TypeIds_HostDispatch;
    }

    BOOL JavascriptOperators::IsObjectOrNull(Var instance)
    {TRACE_IT(51472);
        TypeId typeId = GetTypeId(instance);
        return IsObjectType(typeId) || typeId == TypeIds_Null;
    }

    BOOL JavascriptOperators::IsUndefined(Var instance)
    {TRACE_IT(51473);
        return JavascriptOperators::GetTypeId(instance) == TypeIds_Undefined;
    }

    BOOL JavascriptOperators::IsUndefinedOrNullType(TypeId typeId)
    {TRACE_IT(51474);
        return typeId <= TypeIds_UndefinedOrNull;
    }

    BOOL JavascriptOperators::IsUndefinedOrNull(Var instance)
    {TRACE_IT(51475);
        return IsUndefinedOrNullType(JavascriptOperators::GetTypeId(instance));
    }

    BOOL JavascriptOperators::IsNull(Var instance)
    {TRACE_IT(51476);
        return JavascriptOperators::GetTypeId(instance) == TypeIds_Null;
    }

    BOOL JavascriptOperators::IsSpecialObjectType(TypeId typeId)
    {TRACE_IT(51477);
        return typeId > TypeIds_LastTrueJavascriptObjectType;
    }

    BOOL JavascriptOperators::IsUndefinedObject(Var instance)
    {TRACE_IT(51478);
        return JavascriptOperators::GetTypeId(instance) == TypeIds_Undefined;
    }

    BOOL JavascriptOperators::IsUndefinedObject(Var instance, RecyclableObject *libraryUndefined)
    {TRACE_IT(51479);
        Assert(JavascriptOperators::IsUndefinedObject(libraryUndefined));

        return instance == libraryUndefined;
    }

    BOOL JavascriptOperators::IsUndefinedObject(Var instance, ScriptContext *scriptContext)
    {TRACE_IT(51480);
        return JavascriptOperators::IsUndefinedObject(instance, scriptContext->GetLibrary()->GetUndefined());
    }

    BOOL JavascriptOperators::IsUndefinedObject(Var instance, JavascriptLibrary* library)
    {TRACE_IT(51481);
        return JavascriptOperators::IsUndefinedObject(instance, library->GetUndefined());
    }

    BOOL JavascriptOperators::IsAnyNumberValue(Var instance)
    {TRACE_IT(51482);
        TypeId typeId = GetTypeId(instance);
        return TypeIds_FirstNumberType <= typeId && typeId <= TypeIds_LastNumberType;
    }

    // GetIterator as described in ES6.0 (draft 22) Section 7.4.1
    RecyclableObject* JavascriptOperators::GetIterator(Var iterable, ScriptContext* scriptContext, bool optional)
    {TRACE_IT(51483);
        RecyclableObject* iterableObj = RecyclableObject::FromVar(JavascriptOperators::ToObject(iterable, scriptContext));
        return JavascriptOperators::GetIterator(iterableObj, scriptContext, optional);
    }

    RecyclableObject* JavascriptOperators::GetIteratorFunction(Var iterable, ScriptContext* scriptContext, bool optional)
    {TRACE_IT(51484);
        RecyclableObject* iterableObj = RecyclableObject::FromVar(JavascriptOperators::ToObject(iterable, scriptContext));
        return JavascriptOperators::GetIteratorFunction(iterableObj, scriptContext, optional);
    }

    RecyclableObject* JavascriptOperators::GetIteratorFunction(RecyclableObject* instance, ScriptContext * scriptContext, bool optional)
    {TRACE_IT(51485);
        Var func = JavascriptOperators::GetProperty(instance, PropertyIds::_symbolIterator, scriptContext);

        if (optional && JavascriptOperators::IsUndefinedOrNull(func))
        {TRACE_IT(51486);
            return nullptr;
        }

        if (!JavascriptConversion::IsCallable(func))
        {TRACE_IT(51487);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction);
        }

        RecyclableObject* function = RecyclableObject::FromVar(func);
        return function;
    }

    RecyclableObject* JavascriptOperators::GetIterator(RecyclableObject* instance, ScriptContext * scriptContext, bool optional)
    {TRACE_IT(51488);
        RecyclableObject* function = GetIteratorFunction(instance, scriptContext, optional);

        if (function == nullptr)
        {TRACE_IT(51489);
            Assert(optional);
            return nullptr;
        }

        Var iterator = CALL_FUNCTION(function, CallInfo(Js::CallFlags_Value, 1), instance);

        if (!JavascriptOperators::IsObject(iterator))
        {TRACE_IT(51490);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
        }

        return RecyclableObject::FromVar(iterator);
    }

    void JavascriptOperators::IteratorClose(RecyclableObject* iterator, ScriptContext* scriptContext)
    {TRACE_IT(51491);
        try
        {TRACE_IT(51492);
            Var func = JavascriptOperators::GetProperty(iterator, PropertyIds::return_, scriptContext);

            if (JavascriptConversion::IsCallable(func))
            {TRACE_IT(51493);
                RecyclableObject* callable = RecyclableObject::FromVar(func);
                Js::Var args[] = { iterator };
                Js::CallInfo callInfo(Js::CallFlags_Value, _countof(args));
                JavascriptFunction::CallFunction<true>(callable, callable->GetEntryPoint(), Js::Arguments(callInfo, args));
            }
        }
        catch (const JavascriptException& err)
        {TRACE_IT(51494);
            err.GetAndClear();  // discard exception object
            // We have arrived in this function due to AbruptCompletion (which is an exception), so we don't need to
            // propagate the exception of calling return function
        }
    }

    // IteratorNext as described in ES6.0 (draft 22) Section 7.4.2
    RecyclableObject* JavascriptOperators::IteratorNext(RecyclableObject* iterator, ScriptContext* scriptContext, Var value)
    {TRACE_IT(51495);
        Var func = JavascriptOperators::GetProperty(iterator, PropertyIds::next, scriptContext);

        ThreadContext *threadContext = scriptContext->GetThreadContext();
        if (!JavascriptConversion::IsCallable(func))
        {TRACE_IT(51496);
            if (!threadContext->RecordImplicitException())
            {TRACE_IT(51497);
                return scriptContext->GetLibrary()->GetUndefined();
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction);
        }

        RecyclableObject* callable = RecyclableObject::FromVar(func);
        Var result = threadContext->ExecuteImplicitCall(callable, ImplicitCall_Accessor, [=]() -> Var
            {
                Js::Var args[] = { iterator, value };
                Js::CallInfo callInfo(Js::CallFlags_Value, _countof(args) + (value == nullptr ? -1 : 0));
                return JavascriptFunction::CallFunction<true>(callable, callable->GetEntryPoint(), Arguments(callInfo, args));
            });

        if (!JavascriptOperators::IsObject(result))
        {TRACE_IT(51498);
            if (!threadContext->RecordImplicitException())
            {TRACE_IT(51499);
                return scriptContext->GetLibrary()->GetUndefined();
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject);
        }

        return RecyclableObject::FromVar(result);
    }

    // IteratorComplete as described in ES6.0 (draft 22) Section 7.4.3
    bool JavascriptOperators::IteratorComplete(RecyclableObject* iterResult, ScriptContext* scriptContext)
    {TRACE_IT(51500);
        Var done = JavascriptOperators::GetProperty(iterResult, Js::PropertyIds::done, scriptContext);

        return JavascriptConversion::ToBool(done, scriptContext);
    }

    // IteratorValue as described in ES6.0 (draft 22) Section 7.4.4
    Var JavascriptOperators::IteratorValue(RecyclableObject* iterResult, ScriptContext* scriptContext)
    {TRACE_IT(51501);
        return JavascriptOperators::GetProperty(iterResult, Js::PropertyIds::value, scriptContext);
    }

    // IteratorStep as described in ES6.0 (draft 22) Section 7.4.5
    bool JavascriptOperators::IteratorStep(RecyclableObject* iterator, ScriptContext* scriptContext, RecyclableObject** result)
    {TRACE_IT(51502);
        Assert(result);

        *result = JavascriptOperators::IteratorNext(iterator, scriptContext);
        return !JavascriptOperators::IteratorComplete(*result, scriptContext);
    }

    bool JavascriptOperators::IteratorStepAndValue(RecyclableObject* iterator, ScriptContext* scriptContext, Var* resultValue)
    {TRACE_IT(51503);
        // CONSIDER: Fast-pathing for iterators that are built-ins?
        RecyclableObject* result = JavascriptOperators::IteratorNext(iterator, scriptContext);

        if (!JavascriptOperators::IteratorComplete(result, scriptContext))
        {TRACE_IT(51504);
            *resultValue = JavascriptOperators::IteratorValue(result, scriptContext);
            return true;
        }

        return false;
    }

    RecyclableObject* JavascriptOperators::CreateFromConstructor(RecyclableObject* constructor, ScriptContext* scriptContext)
    {TRACE_IT(51505);
        // Create a regular object and set the internal proto from the constructor
        return JavascriptOperators::OrdinaryCreateFromConstructor(constructor, scriptContext->GetLibrary()->CreateObject(), nullptr, scriptContext);
    }

    RecyclableObject* JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject* constructor, RecyclableObject* obj, DynamicObject* intrinsicProto, ScriptContext* scriptContext)
    {TRACE_IT(51506);
        // There isn't a good way for us to add internal properties to objects in Chakra.
        // Thus, caller should take care to create obj with the correct internal properties.

        Var proto = JavascriptOperators::GetProperty(constructor, Js::PropertyIds::prototype, scriptContext);

        // If constructor.prototype is an object, we should use that as the [[Prototype]] for our obj.
        // Else, we set the [[Prototype]] internal slot of obj to %intrinsicProto% - which should be the default.
        if (JavascriptOperators::IsObjectType(JavascriptOperators::GetTypeId(proto)) &&
            DynamicObject::FromVar(proto) != intrinsicProto)
        {TRACE_IT(51507);
            JavascriptObject::ChangePrototype(obj, RecyclableObject::FromVar(proto), /*validate*/true, scriptContext);
        }

        return obj;
    }

    Var JavascriptOperators::GetProperty(RecyclableObject* instance, PropertyId propertyId, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(51508);
        return JavascriptOperators::GetProperty(instance, instance, propertyId, requestContext, info);
    }

    BOOL JavascriptOperators::GetProperty(RecyclableObject* instance, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(51509);
        return JavascriptOperators::GetProperty(instance, instance, propertyId, value, requestContext, info);
    }

    Var JavascriptOperators::GetProperty(Var instance, RecyclableObject* propertyObject, PropertyId propertyId, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(51510);
        Var value;
        if (JavascriptOperators::GetProperty(instance, propertyObject, propertyId, &value, requestContext, info))
        {TRACE_IT(51511);
            return value;
        }
        return requestContext->GetMissingPropertyResult();
    }

    Var JavascriptOperators::GetRootProperty(RecyclableObject* instance, PropertyId propertyId, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(51512);
        Var value;
        if (JavascriptOperators::GetRootProperty(instance, propertyId, &value, requestContext, info))
        {TRACE_IT(51513);
            return value;
        }
        return requestContext->GetMissingPropertyResult();
    }

    BOOL JavascriptOperators::GetPropertyReference(RecyclableObject *instance, PropertyId propertyId, Var* value, ScriptContext* requestContext, PropertyValueInfo* info)
    {TRACE_IT(51514);
        return JavascriptOperators::GetPropertyReference(instance, instance, propertyId, value, requestContext, info);
    }

    Var JavascriptOperators::GetItem(RecyclableObject* instance, uint32 index, ScriptContext* requestContext)
    {TRACE_IT(51515);
        Var value;
        if (GetItem(instance, index, &value, requestContext))
        {TRACE_IT(51516);
            return value;
        }

        return requestContext->GetMissingItemResult();
    }

    Var JavascriptOperators::GetItem(RecyclableObject* instance, uint64 index, ScriptContext* requestContext)
    {TRACE_IT(51517);
        Var value;
        if (GetItem(instance, index, &value, requestContext))
        {TRACE_IT(51518);
            return value;
        }

        return requestContext->GetMissingItemResult();
    }

    BOOL JavascriptOperators::GetItem(RecyclableObject* instance, uint64 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(51519);
        PropertyRecord const * propertyRecord;
        JavascriptOperators::GetPropertyIdForInt(index, requestContext, &propertyRecord);
        return JavascriptOperators::GetProperty(instance, propertyRecord->GetPropertyId(), value, requestContext);
    }

    BOOL JavascriptOperators::GetItem(RecyclableObject* instance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(51520);
        return JavascriptOperators::GetItem(instance, instance, index, value, requestContext);
    }

    BOOL JavascriptOperators::GetItemReference(RecyclableObject* instance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(51521);
        return GetItemReference(instance, instance, index, value, requestContext);
    }

    BOOL JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(RecyclableObject* instance, PropertyId propertyId, Var* setterValue, DescriptorFlags* flags, PropertyValueInfo* info, ScriptContext* scriptContext)
    {TRACE_IT(51522);
        if (propertyId == Js::PropertyIds::__proto__)
        {TRACE_IT(51523);
            return CheckPrototypesForAccessorOrNonWritablePropertyCore<PropertyId, false, false>(instance, propertyId, setterValue, flags, info, scriptContext);
        }
        else
        {TRACE_IT(51524);
            return CheckPrototypesForAccessorOrNonWritablePropertyCore<PropertyId, true, false>(instance, propertyId, setterValue, flags, info, scriptContext);
        }
    }

    BOOL JavascriptOperators::CheckPrototypesForAccessorOrNonWritableRootProperty(RecyclableObject* instance, PropertyId propertyId, Var* setterValue, DescriptorFlags* flags, PropertyValueInfo* info, ScriptContext* scriptContext)
    {TRACE_IT(51525);
        if (propertyId == Js::PropertyIds::__proto__)
        {TRACE_IT(51526);
            return CheckPrototypesForAccessorOrNonWritablePropertyCore<PropertyId, false, true>(instance, propertyId, setterValue, flags, info, scriptContext);
        }
        else
        {TRACE_IT(51527);
            return CheckPrototypesForAccessorOrNonWritablePropertyCore<PropertyId, true, true>(instance, propertyId, setterValue, flags, info, scriptContext);
        }
    }

    BOOL JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(RecyclableObject* instance, JavascriptString* propertyNameString, Var* setterValue, DescriptorFlags* flags, PropertyValueInfo* info, ScriptContext* scriptContext)
    {TRACE_IT(51528);
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (Js::BuiltInPropertyRecords::__proto__.Equals(propertyName))
        {TRACE_IT(51529);
            return CheckPrototypesForAccessorOrNonWritablePropertyCore<JavascriptString*, false, false>(instance, propertyNameString, setterValue, flags, info, scriptContext);
        }
        else
        {TRACE_IT(51530);
            return CheckPrototypesForAccessorOrNonWritablePropertyCore<JavascriptString*, true, false>(instance, propertyNameString, setterValue, flags, info, scriptContext);
        }
    }

    BOOL JavascriptOperators::SetProperty(Var instance, RecyclableObject* object, PropertyId propertyId, Var newValue, ScriptContext* requestContext, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(51531);
        PropertyValueInfo info;
        return JavascriptOperators::SetProperty(instance, object, propertyId, newValue, &info, requestContext, propertyOperationFlags);
    }

    BOOL JavascriptOperators::TryConvertToUInt32(const char16* str, int length, uint32* intVal)
    {TRACE_IT(51532);
        return NumberUtilities::TryConvertToUInt32(str, length, intVal);
    }

    template <typename TPropertyKey>
    DescriptorFlags JavascriptOperators::GetRootSetter(RecyclableObject* instance, TPropertyKey propertyKey, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(51533);
        // This is provided only so that CheckPrototypesForAccessorOrNonWritablePropertyCore will compile.
        // It will never be called.
        Throw::FatalInternalError();
    }

    template <>
    inline DescriptorFlags JavascriptOperators::GetRootSetter(RecyclableObject* instance, PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(51534);
        AssertMsg(JavascriptOperators::GetTypeId(instance) == TypeIds_GlobalObject
            || JavascriptOperators::GetTypeId(instance) == TypeIds_ModuleRoot,
            "Root must be a global object!");

        RootObjectBase* rootObject = static_cast<RootObjectBase*>(instance);
        return rootObject->GetRootSetter(propertyId, setterValue, info, requestContext);
    }

    // Helper to fetch @@species from a constructor object
    Var JavascriptOperators::GetSpecies(RecyclableObject* constructor, ScriptContext* scriptContext)
    {TRACE_IT(51535);
        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {TRACE_IT(51536);
            Var species = nullptr;

            // Let S be Get(C, @@species)
            if (JavascriptOperators::GetProperty(constructor, PropertyIds::_symbolSpecies, &species, scriptContext)
                && !JavascriptOperators::IsUndefinedOrNull(species))
            {TRACE_IT(51537);
                // If S is neither undefined nor null, let C be S
                return species;
            }
        }

        return constructor;
    }
} // namespace Js
