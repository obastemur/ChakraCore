//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Library/JavascriptNumberObject.h"
#include "Library/JavascriptStringObject.h"
#include "Library/JavascriptSimdObject.h"
#include "Library/DateImplementation.h"
#include "Library/JavascriptDate.h"

namespace Js
{
    static const double k_2to16 = 65536.0;
    static const double k_2to31 = 2147483648.0;
    static const double k_2to32 = 4294967296.0;

    // ES5 9.10 indicates that this method should throw a TypeError if the supplied value is Undefined or Null.
    // Our implementation returns FALSE in this scenario, expecting the caller to throw the TypeError.
    // This allows the caller to provide more context in the error message without having to unnecessarily
    // construct the message string before knowing whether or not the object is coercible.
    BOOL JavascriptConversion::CheckObjectCoercible(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 22\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);
        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {LOGMEIN("JavascriptConversion.cpp] 25\n");
            return FALSE;
        }
        return TRUE;
    }

    //ES5 9.11  Undefined, Null, Boolean, Number, String - return false
    //If Object has a [[Call]] internal method, then return true, otherwise return false
    bool JavascriptConversion::IsCallable(Var aValue)
    {LOGMEIN("JavascriptConversion.cpp] 34\n");
        if (!RecyclableObject::Is(aValue))
        {LOGMEIN("JavascriptConversion.cpp] 36\n");
            return false;
        }
        JavascriptMethod entryPoint = RecyclableObject::FromVar(aValue)->GetEntryPoint();
        return RecyclableObject::DefaultEntryPoint != entryPoint;
    }

    //----------------------------------------------------------------------------
    // ES5 9.12 SameValue algorithm implementation.
    // 1.    If Type(x) is different from Type(y), return false.
    // 2.    If Type(x) is Undefined, return true.
    // 3.    If Type(x) is Null, return true.
    // 4.    If Type(x) is Number, then.
    //          a.  If x is NaN and y is NaN, return true.
    //          b.  If x is +0 and y is -0, return false.
    //          c.  If x is -0 and y is +0, return false.
    //          d.  If x is the same number value as y, return true.
    //          e.  Return false.
    // 5.    If Type(x) is String, then return true if x and y are exactly the same sequence of characters (same length and same characters in corresponding positions); otherwise, return false.
    // 6.    If Type(x) is Boolean, return true if x and y are both true or both false; otherwise, return false.
    // 7.    Return true if x and y refer to the same object. Otherwise, return false.
    //----------------------------------------------------------------------------
    template<bool zero>
    bool JavascriptConversion::SameValueCommon(Var aLeft, Var aRight)
    {LOGMEIN("JavascriptConversion.cpp] 60\n");
        TypeId leftType = JavascriptOperators::GetTypeId(aLeft);
        TypeId rightType = JavascriptOperators::GetTypeId(aRight);

        //Check for undefined and null type;
        if (leftType == TypeIds_Undefined )
        {LOGMEIN("JavascriptConversion.cpp] 66\n");
            return rightType == TypeIds_Undefined;
        }

        if (leftType == TypeIds_Null)
        {LOGMEIN("JavascriptConversion.cpp] 71\n");
            return rightType == TypeIds_Null;
        }

        double dblLeft, dblRight;

        switch (leftType)
        {LOGMEIN("JavascriptConversion.cpp] 78\n");
        case TypeIds_Integer:
            switch (rightType)
            {LOGMEIN("JavascriptConversion.cpp] 81\n");
            case TypeIds_Integer:
                return aLeft == aRight;
            case TypeIds_Number:
                dblLeft     = TaggedInt::ToDouble(aLeft);
                dblRight    = JavascriptNumber::GetValue(aRight);
                goto CommonNumber;
            case TypeIds_Int64Number:
                {LOGMEIN("JavascriptConversion.cpp] 89\n");
                int leftValue = TaggedInt::ToInt32(aLeft);
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                return leftValue == rightValue;
                }
            case TypeIds_UInt64Number:
                {LOGMEIN("JavascriptConversion.cpp] 95\n");
                int leftValue = TaggedInt::ToInt32(aLeft);
                unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                return leftValue == rightValue;
                }
            }
            break;
        case TypeIds_Int64Number:
            switch (rightType)
            {LOGMEIN("JavascriptConversion.cpp] 104\n");
            case TypeIds_Integer:
                {LOGMEIN("JavascriptConversion.cpp] 106\n");
                __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                int rightValue = TaggedInt::ToInt32(aRight);
                return leftValue == rightValue;
                }
            case TypeIds_Number:
                dblLeft     = (double)JavascriptInt64Number::FromVar(aLeft)->GetValue();
                dblRight    = JavascriptNumber::GetValue(aRight);
                goto CommonNumber;
            case TypeIds_Int64Number:
                {LOGMEIN("JavascriptConversion.cpp] 116\n");
                __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                return leftValue == rightValue;
                }
            case TypeIds_UInt64Number:
                {LOGMEIN("JavascriptConversion.cpp] 122\n");
                __int64 leftValue = JavascriptInt64Number::FromVar(aLeft)->GetValue();
                unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                return ((unsigned __int64)leftValue == rightValue);
                }
            }
            break;
        case TypeIds_UInt64Number:
            switch (rightType)
            {LOGMEIN("JavascriptConversion.cpp] 131\n");
            case TypeIds_Integer:
                {LOGMEIN("JavascriptConversion.cpp] 133\n");
                unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                __int64 rightValue = TaggedInt::ToInt32(aRight);
                return (leftValue == (unsigned __int64)rightValue);
                }
            case TypeIds_Number:
                dblLeft     = (double)JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                dblRight    = JavascriptNumber::GetValue(aRight);
                goto CommonNumber;
            case TypeIds_Int64Number:
                {LOGMEIN("JavascriptConversion.cpp] 143\n");
                unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                return (leftValue == (unsigned __int64)rightValue);
                }
            case TypeIds_UInt64Number:
                {LOGMEIN("JavascriptConversion.cpp] 149\n");
                unsigned __int64 leftValue = JavascriptUInt64Number::FromVar(aLeft)->GetValue();
                unsigned __int64 rightValue = JavascriptInt64Number::FromVar(aRight)->GetValue();
                return leftValue == rightValue;
                }
            }
            break;
        case TypeIds_Number:
            switch (rightType)
            {LOGMEIN("JavascriptConversion.cpp] 158\n");
            case TypeIds_Integer:
                dblLeft     = JavascriptNumber::GetValue(aLeft);
                dblRight    = TaggedInt::ToDouble(aRight);
                goto CommonNumber;
            case TypeIds_Int64Number:
                dblLeft     = JavascriptNumber::GetValue(aLeft);
                dblRight    = (double)JavascriptInt64Number::FromVar(aRight)->GetValue();
                goto CommonNumber;
            case TypeIds_UInt64Number:
                dblLeft     = JavascriptNumber::GetValue(aLeft);
                dblRight    = (double)JavascriptUInt64Number::FromVar(aRight)->GetValue();
                goto CommonNumber;
            case TypeIds_Number:
                dblLeft     = JavascriptNumber::GetValue(aLeft);
                dblRight    = JavascriptNumber::GetValue(aRight);
CommonNumber:
                if (JavascriptNumber::IsNan(dblLeft) && JavascriptNumber::IsNan(dblRight))
                {LOGMEIN("JavascriptConversion.cpp] 176\n");
                    return true;
                }

                if (zero)
                {LOGMEIN("JavascriptConversion.cpp] 181\n");
                    // SameValueZero(+0,-0) returns true;
                    return dblLeft == dblRight;
                }
                else
                {
                    // SameValue(+0,-0) returns false;
                    return (NumberUtilities::LuLoDbl(dblLeft) == NumberUtilities::LuLoDbl(dblRight) &&
                        NumberUtilities::LuHiDbl(dblLeft) == NumberUtilities::LuHiDbl(dblRight));
                }
            }
            break;
        case TypeIds_Boolean:
            switch (rightType)
            {LOGMEIN("JavascriptConversion.cpp] 195\n");
            case TypeIds_Boolean:
                return aLeft == aRight;
            }
            break;
        case TypeIds_String:
            switch (rightType)
            {LOGMEIN("JavascriptConversion.cpp] 202\n");
            case TypeIds_String:
                return JavascriptString::Equals(aLeft, aRight);
            }
            break;
        case TypeIds_Symbol:
            switch (rightType)
            {LOGMEIN("JavascriptConversion.cpp] 209\n");
            case TypeIds_Symbol:
                {LOGMEIN("JavascriptConversion.cpp] 211\n");
                    JavascriptSymbol* leftSymbol = JavascriptSymbol::FromVar(aLeft);
                    JavascriptSymbol* rightSymbol = JavascriptSymbol::FromVar(aRight);
                    return leftSymbol->GetValue() == rightSymbol->GetValue();
                }
            }
            return false;
        default:
            break;
        }
        return aLeft == aRight;
    }

    template bool JavascriptConversion::SameValueCommon<false>(Var aLeft, Var aRight);
    template bool JavascriptConversion::SameValueCommon<true>(Var aLeft, Var aRight);

    //----------------------------------------------------------------------------
    // ToObject() takes a value and converts it to an Object type
    // Implementation of ES5 9.9
    // The spec indicates that this method should throw a TypeError if the supplied value is Undefined or Null.
    // Our implementation returns FALSE in this scenario, expecting the caller to throw the TypeError.
    // This allows the caller to provide more context in the error message without having to unnecessarily
    // construct the message string before knowing whether or not the value can be converted to an object.
    //
    //  Undefined   Return FALSE.
    //  Null        Return FALSE.
    //  Boolean     Create a new Boolean object whose [[PrimitiveValue]]
    //              internal property is set to the value of the boolean.
    //              See 15.6 for a description of Boolean objects.
    //              Return TRUE.
    //  Number      Create a new Number object whose [[PrimitiveValue]]
    //              internal property is set to the value of the number.
    //              See 15.7 for a description of Number objects.
    //              Return TRUE.
    //  String      Create a new String object whose [[PrimitiveValue]]
    //              internal property is set to the value of the string.
    //              See 15.5 for a description of String objects.
    //              Return TRUE.
    //  Object      The result is the input argument (no conversion).
    //              Return TRUE.
    //----------------------------------------------------------------------------
    BOOL JavascriptConversion::ToObject(Var aValue, ScriptContext* scriptContext, RecyclableObject** object)
    {LOGMEIN("JavascriptConversion.cpp] 253\n");
        Assert(object);

        switch (JavascriptOperators::GetTypeId(aValue))
        {LOGMEIN("JavascriptConversion.cpp] 257\n");
            case TypeIds_Undefined:
            case TypeIds_Null:
                return FALSE;

            case TypeIds_Number:
            case TypeIds_Integer:
                *object = scriptContext->GetLibrary()->CreateNumberObject(aValue);
                return TRUE;

            default:
            {LOGMEIN("JavascriptConversion.cpp] 268\n");
#ifdef ENABLE_SIMDJS
                if (SIMDUtils::IsSimdType(aValue))
                {LOGMEIN("JavascriptConversion.cpp] 271\n");
                    *object = scriptContext->GetLibrary()->CreateSIMDObject(aValue, JavascriptOperators::GetTypeId(aValue));
                }
                else
#endif
                {
                    *object = RecyclableObject::FromVar(aValue)->ToObject(scriptContext);
                }
                return TRUE;
            }
        }
    }

    //----------------------------------------------------------------------------
    // ToPropertyKey() takes a value and converts it to a property key
    // Implementation of ES6 7.1.14
    //----------------------------------------------------------------------------
    void JavascriptConversion::ToPropertyKey(Var argument, ScriptContext* scriptContext, const PropertyRecord** propertyRecord)
    {LOGMEIN("JavascriptConversion.cpp] 289\n");
        Var key = JavascriptConversion::ToPrimitive(argument, JavascriptHint::HintString, scriptContext);

        if (JavascriptSymbol::Is(key))
        {LOGMEIN("JavascriptConversion.cpp] 293\n");
            // If we are looking up a property keyed by a symbol, we already have the PropertyId in the symbol
            *propertyRecord = JavascriptSymbol::FromVar(key)->GetValue();
        }
        else
        {
            // For all other types, convert the key into a string and use that as the property name
            JavascriptString * propName = JavascriptConversion::ToString(key, scriptContext);

            if (VirtualTableInfo<Js::PropertyString>::HasVirtualTable(propName))
            {LOGMEIN("JavascriptConversion.cpp] 303\n");
                PropertyString * propertyString = (PropertyString *)propName;
                *propertyRecord = propertyString->GetPropertyRecord();
            }
            else
            {
                scriptContext->GetOrAddPropertyRecord(propName->GetString(), propName->GetLength(), propertyRecord);
            }
        }
    }

    //----------------------------------------------------------------------------
    // ToPrimitive() takes a value and an optional argument and converts it to a non Object type
    // Implementation of ES5 9.1
    //
    //    Undefined:The result equals the input argument (no conversion).
    //    Null:     The result equals the input argument (no conversion).
    //    Boolean:  The result equals the input argument (no conversion).
    //    Number:   The result equals the input argument (no conversion).
    //    String:   The result equals the input argument (no conversion).
    //    VariantDate:Returns the value for variant date by calling ToPrimitve directly.
    //    Object:   Return a default value for the Object.
    //              The default value of an object is retrieved by calling the [[DefaultValue]]
    //              internal method of the object, passing the optional hint PreferredType.
    //              The behavior of the [[DefaultValue]] internal method is defined by this specification
    //              for all native ECMAScript objects (8.12.9).
    //----------------------------------------------------------------------------
    Var JavascriptConversion::ToPrimitive(Var aValue, JavascriptHint hint, ScriptContext * requestContext)
    {LOGMEIN("JavascriptConversion.cpp] 331\n");
        switch (JavascriptOperators::GetTypeId(aValue))
        {LOGMEIN("JavascriptConversion.cpp] 333\n");
        case TypeIds_Undefined:
        case TypeIds_Null:
        case TypeIds_Integer:
        case TypeIds_Boolean:
        case TypeIds_Number:
        case TypeIds_String:
        case TypeIds_Symbol:
            return aValue;

        case TypeIds_VariantDate:
            {LOGMEIN("JavascriptConversion.cpp] 344\n");
                Var result = nullptr;
                if (JavascriptVariantDate::FromVar(aValue)->ToPrimitive(hint, &result, requestContext) != TRUE)
                {LOGMEIN("JavascriptConversion.cpp] 347\n");
                    result = nullptr;
                }
                return result;
            }

        case TypeIds_StringObject:
            {LOGMEIN("JavascriptConversion.cpp] 354\n");
                JavascriptStringObject * stringObject = JavascriptStringObject::FromVar(aValue);

                if (stringObject->GetScriptContext()->optimizationOverrides.GetSideEffects() & (hint == JavascriptHint::HintString ? SideEffects_ToString : SideEffects_ValueOf))
                {LOGMEIN("JavascriptConversion.cpp] 358\n");
                    return MethodCallToPrimitive(aValue, hint, requestContext);
                }

                return CrossSite::MarshalVar(requestContext, stringObject->Unwrap());
            }

        case TypeIds_NumberObject:
            {LOGMEIN("JavascriptConversion.cpp] 366\n");
                JavascriptNumberObject * numberObject = JavascriptNumberObject::FromVar(aValue);

                if (hint == JavascriptHint::HintString)
                {LOGMEIN("JavascriptConversion.cpp] 370\n");
                    if (numberObject->GetScriptContext()->optimizationOverrides.GetSideEffects() & SideEffects_ToString)
                    {LOGMEIN("JavascriptConversion.cpp] 372\n");
                        return MethodCallToPrimitive(aValue, hint, requestContext);
                    }
                    return JavascriptNumber::ToStringRadix10(numberObject->GetValue(), requestContext);
                }
                else
                {
                    if (numberObject->GetScriptContext()->optimizationOverrides.GetSideEffects() & SideEffects_ValueOf)
                    {LOGMEIN("JavascriptConversion.cpp] 380\n");
                        return MethodCallToPrimitive(aValue, hint, requestContext);
                    }
                    return CrossSite::MarshalVar(requestContext, numberObject->Unwrap());
                }
            }


        case TypeIds_SymbolObject:
            {LOGMEIN("JavascriptConversion.cpp] 389\n");
                JavascriptSymbolObject* symbolObject = JavascriptSymbolObject::FromVar(aValue);

                return requestContext->GetLibrary()->CreateSymbol(symbolObject->GetValue());
            }

        case TypeIds_Date:
        case TypeIds_WinRTDate:
            {LOGMEIN("JavascriptConversion.cpp] 397\n");
                JavascriptDate* dateObject = JavascriptDate::FromVar(aValue);
                if(hint == JavascriptHint::HintNumber)
                {LOGMEIN("JavascriptConversion.cpp] 400\n");
                    if (dateObject->GetScriptContext()->optimizationOverrides.GetSideEffects() & SideEffects_ValueOf)
                    {LOGMEIN("JavascriptConversion.cpp] 402\n");
                        // if no Method exists this function falls back to OrdinaryToPrimitive
                        // if IsES6ToPrimitiveEnabled flag is off we also fall back to OrdinaryToPrimitive
                        return MethodCallToPrimitive(aValue, hint, requestContext);
                    }
                    return JavascriptNumber::ToVarNoCheck(dateObject->GetTime(), requestContext);
                }
                else
                {
                    if (dateObject->GetScriptContext()->optimizationOverrides.GetSideEffects() & SideEffects_ToString)
                    {LOGMEIN("JavascriptConversion.cpp] 412\n");
                        // if no Method exists this function falls back to OrdinaryToPrimitive
                        // if IsES6ToPrimitiveEnabled flag is off we also fall back to OrdinaryToPrimitive
                        return MethodCallToPrimitive(aValue, hint, requestContext);
                    }
                    //NOTE: Consider passing requestContext to JavascriptDate::ToString
                    return CrossSite::MarshalVar(requestContext, JavascriptDate::ToString(dateObject));
                }
            }

        // convert to JavascriptNumber
        case TypeIds_Int64Number:
            return JavascriptInt64Number::FromVar(aValue)->ToJavascriptNumber();
        case TypeIds_UInt64Number:
            return JavascriptUInt64Number::FromVar(aValue)->ToJavascriptNumber();

        default:
#ifdef ENABLE_SIMDJS
            if (SIMDUtils::IsSimdType(aValue))
            {LOGMEIN("JavascriptConversion.cpp] 431\n");
                return aValue;
            }
            else
#endif
            {
                // if no Method exists this function falls back to OrdinaryToPrimitive
                // if IsES6ToPrimitiveEnabled flag is off we also fall back to OrdinaryToPrimitive
                return MethodCallToPrimitive(aValue, hint, requestContext);
            }
        }
    }

    //----------------------------------------------------------------------------
    //7.1.16 CanonicalNumericIndexString(argument)
    //1. Assert : Type(argument) is String.
    //2. If argument is "-0", then return -0.
    //3. Let n be ToNumber(argument).
    //4. If SameValue(ToString(n), argument) is false, then return undefined.
    //5. Return n.
    //----------------------------------------------------------------------------
    BOOL JavascriptConversion::CanonicalNumericIndexString(Var aValue, double *indexValue, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 453\n");
        AssertMsg(JavascriptString::Is(aValue), "CanonicalNumericIndexString expects only string");
        if (JavascriptString::IsNegZero(JavascriptString::FromVar(aValue)))
        {LOGMEIN("JavascriptConversion.cpp] 456\n");
            *indexValue = -0;
            return TRUE;
        }
        Var indexNumberValue = JavascriptOperators::ToNumber(aValue, scriptContext);
        if (JavascriptString::Equals(JavascriptConversion::ToString(indexNumberValue, scriptContext), aValue))
        {LOGMEIN("JavascriptConversion.cpp] 462\n");
            *indexValue = JavascriptNumber::GetValue(indexNumberValue);
            return TRUE;
        }
        return FALSE;
    }

    Var JavascriptConversion::MethodCallToPrimitive(Var aValue, JavascriptHint hint, ScriptContext * requestContext)
    {LOGMEIN("JavascriptConversion.cpp] 470\n");
        Var result = nullptr;
        RecyclableObject *const recyclableObject = RecyclableObject::FromVar(aValue);
        ScriptContext *const scriptContext = recyclableObject->GetScriptContext();

        //7.3.9 GetMethod(V, P)
        //  The abstract operation GetMethod is used to get the value of a specific property of an ECMAScript language value when the value of the
        //  property is expected to be a function. The operation is called with arguments V and P where V is the ECMAScript language value, P is the
        //  property key. This abstract operation performs the following steps:
        //  1. Assert: IsPropertyKey(P) is true.
        //  2. Let func be ? GetV(V, P).
        //  3. If func is either undefined or null, return undefined.
        //  4. If IsCallable(func) is false, throw a TypeError exception.
        //  5. Return func.
        Var varMethod;

        if (!(requestContext->GetConfig()->IsES6ToPrimitiveEnabled()
            && JavascriptOperators::GetPropertyReference(recyclableObject, PropertyIds::_symbolToPrimitive, &varMethod, requestContext)
            && !JavascriptOperators::IsUndefinedOrNull(varMethod)))
        {LOGMEIN("JavascriptConversion.cpp] 489\n");
            return OrdinaryToPrimitive(aValue, hint, requestContext);
        }
        if (!JavascriptFunction::Is(varMethod))
        {LOGMEIN("JavascriptConversion.cpp] 493\n");
            // Don't error if we disabled implicit calls
            JavascriptError::TryThrowTypeError(scriptContext, requestContext, JSERR_Property_NeedFunction, requestContext->GetPropertyName(PropertyIds::_symbolToPrimitive)->GetBuffer());
            return requestContext->GetLibrary()->GetNull();
        }

        // Let exoticToPrim be GetMethod(input, @@toPrimitive).
        JavascriptFunction* exoticToPrim = JavascriptFunction::FromVar(varMethod);
        JavascriptString* hintString = nullptr;

        if (hint == JavascriptHint::HintString)
        {LOGMEIN("JavascriptConversion.cpp] 504\n");
            hintString = requestContext->GetLibrary()->CreateStringFromCppLiteral(_u("string"));
        }
        else if (hint == JavascriptHint::HintNumber)
        {LOGMEIN("JavascriptConversion.cpp] 508\n");
            hintString = requestContext->GetLibrary()->CreateStringFromCppLiteral(_u("number"));
        }
        else
        {
            hintString = requestContext->GetLibrary()->CreateStringFromCppLiteral(_u("default"));
        }

        // If exoticToPrim is not undefined, then
        if (nullptr != exoticToPrim)
        {LOGMEIN("JavascriptConversion.cpp] 518\n");
            ThreadContext * threadContext = requestContext->GetThreadContext();
            result = threadContext->ExecuteImplicitCall(exoticToPrim, ImplicitCall_ToPrimitive, [=]()->Js::Var
            {
                // Stack object should have a pre-op bail on implicit call.  We shouldn't see them here.
                Assert(!ThreadContext::IsOnStack(recyclableObject));

                // Let result be the result of calling the[[Call]] internal method of exoticToPrim, with input as thisArgument and(hint) as argumentsList.
                return CALL_FUNCTION(exoticToPrim, CallInfo(CallFlags_Value, 2), recyclableObject, hintString);
            });

            if (!result)
            {LOGMEIN("JavascriptConversion.cpp] 530\n");
                // There was an implicit call and implicit calls are disabled. This would typically cause a bailout.
                Assert(threadContext->IsDisableImplicitCall());
                return requestContext->GetLibrary()->GetNull();
            }

            Assert(!CrossSite::NeedMarshalVar(result, requestContext));
        }
        // If result is an ECMAScript language value and Type(result) is not Object, then return result.
        if (TaggedInt::Is(result) || !JavascriptOperators::IsObjectType(JavascriptOperators::GetTypeId(result)))
        {LOGMEIN("JavascriptConversion.cpp] 540\n");
            return result;
        }
        // Else, throw a TypeError exception.
        else
        {
            // Don't error if we disabled implicit calls
            JavascriptError::TryThrowTypeError(scriptContext, requestContext, JSERR_FunctionArgument_Invalid, _u("[Symbol.toPrimitive]"));
            return requestContext->GetLibrary()->GetNull();
        }
    }

    Var JavascriptConversion::OrdinaryToPrimitive(Var aValue, JavascriptHint hint, ScriptContext * requestContext)
    {LOGMEIN("JavascriptConversion.cpp] 553\n");
        Var result;
        RecyclableObject *const recyclableObject = RecyclableObject::FromVar(aValue);
        if (!recyclableObject->ToPrimitive(hint, &result, requestContext))
        {LOGMEIN("JavascriptConversion.cpp] 557\n");
            ScriptContext *const scriptContext = recyclableObject->GetScriptContext();

            int32 hCode;

            switch (hint)
            {LOGMEIN("JavascriptConversion.cpp] 563\n");
            case JavascriptHint::HintNumber:
                hCode = JSERR_NeedNumber;
                break;
            case JavascriptHint::HintString:
                hCode = JSERR_NeedString;
                break;
            default:
                hCode = VBSERR_OLENoPropOrMethod;
                break;
            }
            JavascriptError::TryThrowTypeError(scriptContext, scriptContext, hCode);
            return requestContext->GetLibrary()->GetNull();
        }
        return result;
    }

    JavascriptString *JavascriptConversion::CoerseString(Var aValue, ScriptContext* scriptContext, const char16* apiNameForErrorMsg)
    {LOGMEIN("JavascriptConversion.cpp] 581\n");
        if (!JavascriptConversion::CheckObjectCoercible(aValue, scriptContext))
        {LOGMEIN("JavascriptConversion.cpp] 583\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, apiNameForErrorMsg);
        }

        return ToString(aValue, scriptContext);
    }

    //----------------------------------------------------------------------------
    // ToString - abstract operation
    // ES5 9.8
    //Input Type Result
    //    Undefined
    //    "undefined"
    //    Null
    //    "null"
    //    Boolean
    //    If the argument is true, then the result is "true". If the argument is false, then the result is "false".
    //    Number
    //    See 9.8.1 below.
    //    String
    //    Return the input argument (no conversion)
    //    Object
    //    Apply the following steps:
    // 1. Let primValue be ToPrimitive(input argument, hint String).
    // 2. Return ToString(primValue).
    //----------------------------------------------------------------------------
    JavascriptString *JavascriptConversion::ToString(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 610\n");
        Assert(scriptContext->GetThreadContext()->IsScriptActive());

        BOOL fPrimitiveOnly = false;
        while(true)
        {LOGMEIN("JavascriptConversion.cpp] 615\n");
            switch (JavascriptOperators::GetTypeId(aValue))
            {LOGMEIN("JavascriptConversion.cpp] 617\n");
            case TypeIds_Undefined:
                return scriptContext->GetLibrary()->GetUndefinedDisplayString();

            case TypeIds_Null:
                return scriptContext->GetLibrary()->GetNullDisplayString();

            case TypeIds_Integer:
                return scriptContext->GetIntegerString(aValue);

            case TypeIds_Boolean:
                return JavascriptBoolean::FromVar(aValue)->GetValue() ? scriptContext->GetLibrary()->GetTrueDisplayString() : scriptContext->GetLibrary()->GetFalseDisplayString();

            case TypeIds_Number:
                return JavascriptNumber::ToStringRadix10(JavascriptNumber::GetValue(aValue), scriptContext);

            case TypeIds_Int64Number:
                {LOGMEIN("JavascriptConversion.cpp] 634\n");
                    __int64 value = JavascriptInt64Number::FromVar(aValue)->GetValue();
                    if (!TaggedInt::IsOverflow(value))
                    {LOGMEIN("JavascriptConversion.cpp] 637\n");
                        return scriptContext->GetIntegerString((int)value);
                    }
                    else
                    {
                        return JavascriptInt64Number::ToString(aValue, scriptContext);
                    }
                }

            case TypeIds_UInt64Number:
                {LOGMEIN("JavascriptConversion.cpp] 647\n");
                    unsigned __int64 value = JavascriptUInt64Number::FromVar(aValue)->GetValue();
                    if (!TaggedInt::IsOverflow(value))
                    {LOGMEIN("JavascriptConversion.cpp] 650\n");
                        return scriptContext->GetIntegerString((uint)value);
                    }
                    else
                    {
                        return JavascriptUInt64Number::ToString(aValue, scriptContext);
                    }
                }

            case TypeIds_String:
                return JavascriptString::FromVar(CrossSite::MarshalVar(scriptContext, aValue));

            case TypeIds_VariantDate:
                return JavascriptVariantDate::FromVar(aValue)->GetValueString(scriptContext);

            case TypeIds_Symbol:
                return JavascriptSymbol::FromVar(aValue)->ToString(scriptContext);

            case TypeIds_SymbolObject:
                return JavascriptSymbol::ToString(JavascriptSymbolObject::FromVar(aValue)->GetValue(), scriptContext);

#ifdef ENABLE_SIMDJS
            case TypeIds_SIMDBool8x16:
            case TypeIds_SIMDBool16x8:
            case TypeIds_SIMDBool32x4:
            case TypeIds_SIMDInt8x16:
            case TypeIds_SIMDInt16x8:
            case TypeIds_SIMDInt32x4:
            case TypeIds_SIMDUint8x16:
            case TypeIds_SIMDUint16x8:
            case TypeIds_SIMDUint32x4:
            case TypeIds_SIMDFloat32x4:
            {LOGMEIN("JavascriptConversion.cpp] 682\n");
                Assert(aValue);
                RecyclableObject *obj = nullptr;
                if (!JavascriptConversion::ToObject(aValue, scriptContext, &obj))
                {LOGMEIN("JavascriptConversion.cpp] 686\n");
                    JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedSimd, _u("SIMDType.toString"));
                }
                JavascriptSIMDObject* simdObject = static_cast<JavascriptSIMDObject*>(obj);
                return JavascriptString::FromVar(simdObject->ToString(scriptContext));
            }
#endif

            case TypeIds_GlobalObject:
                aValue = static_cast<Js::GlobalObject*>(aValue)->ToThis();
                // fall through

            default:
                {LOGMEIN("JavascriptConversion.cpp] 699\n");
                    AssertMsg(JavascriptOperators::IsObject(aValue), "bad type object in conversion ToString");
                    if(fPrimitiveOnly)
                    {
                        AssertMsg(FALSE, "wrong call in ToString, no dynamic objects should get here");
                        JavascriptError::ThrowError(scriptContext, VBSERR_InternalError);
                    }
                    fPrimitiveOnly = true;
                    aValue = ToPrimitive(aValue, JavascriptHint::HintString, scriptContext);
                }
            }
        }
    }

    JavascriptString *JavascriptConversion::ToLocaleString(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 714\n");
        switch (JavascriptOperators::GetTypeId(aValue))
        {LOGMEIN("JavascriptConversion.cpp] 716\n");
        case TypeIds_Undefined:
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();

        case TypeIds_Null:
            return scriptContext->GetLibrary()->GetNullDisplayString();

        case TypeIds_Integer:
            return JavascriptNumber::ToLocaleString(TaggedInt::ToInt32(aValue), scriptContext);

        case TypeIds_Boolean:
            return JavascriptBoolean::FromVar(aValue)->GetValue() ? scriptContext->GetLibrary()->GetTrueDisplayString() : scriptContext->GetLibrary()->GetFalseDisplayString();

        case TypeIds_Int64Number:
            return JavascriptNumber::ToLocaleString((double)JavascriptInt64Number::FromVar(aValue)->GetValue(), scriptContext);

        case TypeIds_UInt64Number:
            return JavascriptNumber::ToLocaleString((double)JavascriptUInt64Number::FromVar(aValue)->GetValue(), scriptContext);

        case TypeIds_Number:
            return JavascriptNumber::ToLocaleString(JavascriptNumber::GetValue(aValue), scriptContext);

        case TypeIds_String:
            return JavascriptString::FromVar(aValue);

        case TypeIds_VariantDate:
            // Legacy behavior was to create an empty object and call toLocaleString on it, which would result in this value
            return scriptContext->GetLibrary()->GetObjectDisplayString();

        case TypeIds_Symbol:
            return JavascriptSymbol::FromVar(aValue)->ToString(scriptContext);

        default:
            {LOGMEIN("JavascriptConversion.cpp] 749\n");
                RecyclableObject* object = RecyclableObject::FromVar(aValue);
                Var value = JavascriptOperators::GetProperty(object, PropertyIds::toLocaleString, scriptContext, NULL);

                if (JavascriptConversion::IsCallable(value))
                {LOGMEIN("JavascriptConversion.cpp] 754\n");
                    RecyclableObject* toLocaleStringFunction = RecyclableObject::FromVar(value);
                    Var aResult = CALL_FUNCTION(toLocaleStringFunction, CallInfo(1), aValue);
                    if (JavascriptString::Is(aResult))
                    {LOGMEIN("JavascriptConversion.cpp] 758\n");
                        return JavascriptString::FromVar(aResult);
                    }
                    else
                    {
                        return JavascriptConversion::ToString(aResult, scriptContext);
                    }
                }

                JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, scriptContext->GetPropertyName(PropertyIds::toLocaleString)->GetBuffer());
            }
        }
    }

    //----------------------------------------------------------------------------
    // ToBoolean_Full:
    // (ES3.0: S9.2):
    //
    // Input        Output
    // -----        ------
    // 'undefined'  'false'
    // 'null'       'false'
    // Boolean      Value
    // Number       'false' if +0, -0, or Nan
    //              'true' otherwise
    // String       'false' if argument is ""
    //              'true' otherwise
    // Object       'true'
    // Falsy Object 'false'
    //----------------------------------------------------------------------------
    BOOL JavascriptConversion::ToBoolean_Full(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 789\n");
        AssertMsg(!TaggedInt::Is(aValue), "Should be detected");
        AssertMsg(RecyclableObject::Is(aValue), "Should be handled already");

        auto type = RecyclableObject::FromVar(aValue)->GetType();

        switch (type->GetTypeId())
        {LOGMEIN("JavascriptConversion.cpp] 796\n");
        case TypeIds_Undefined:
        case TypeIds_Null:
        case TypeIds_VariantDate:
            return false;

        case TypeIds_Symbol:
            return true;

        case TypeIds_Boolean:
            return JavascriptBoolean::FromVar(aValue)->GetValue();

#if !FLOATVAR
        case TypeIds_Number:
            {LOGMEIN("JavascriptConversion.cpp] 810\n");
                double value = JavascriptNumber::GetValue(aValue);
                return (!JavascriptNumber::IsNan(value)) && (!JavascriptNumber::IsZero(value));
            }
#endif

        case TypeIds_Int64Number:
            {LOGMEIN("JavascriptConversion.cpp] 817\n");
                __int64 value = JavascriptInt64Number::FromVar(aValue)->GetValue();
                return value != 0;
            }

        case TypeIds_UInt64Number:
            {LOGMEIN("JavascriptConversion.cpp] 823\n");
                unsigned __int64 value = JavascriptUInt64Number::FromVar(aValue)->GetValue();
                return value != 0;
            }

        case TypeIds_String:
            {LOGMEIN("JavascriptConversion.cpp] 829\n");
                JavascriptString * pstValue = JavascriptString::FromVar(aValue);
                return pstValue->GetLength() > 0;
            }

#ifdef ENABLE_SIMDJS
        case TypeIds_SIMDFloat32x4:
        case TypeIds_SIMDFloat64x2:
        case TypeIds_SIMDInt32x4:
        case TypeIds_SIMDInt16x8:
        case TypeIds_SIMDInt8x16:
        case TypeIds_SIMDBool32x4:
        case TypeIds_SIMDBool16x8:
        case TypeIds_SIMDBool8x16:
        case TypeIds_SIMDUint32x4:
        case TypeIds_SIMDUint16x8:
        case TypeIds_SIMDUint8x16:
        {LOGMEIN("JavascriptConversion.cpp] 846\n");
            return true;
        }
#endif

        default:
            {
                AssertMsg(JavascriptOperators::IsObject(aValue), "bad type object in conversion ToBoolean");

                // Falsy objects evaluate to false when converted to Boolean.
                return !type->IsFalsy();
            }
        }
    }

    void JavascriptConversion::ToFloat_Helper(Var aValue, float *pResult, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 862\n");
        *pResult = (float)ToNumber_Full(aValue, scriptContext);
    }

    void JavascriptConversion::ToNumber_Helper(Var aValue, double *pResult, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 867\n");
        Assert(Js::JavascriptStackWalker::ValidateTopJitFrame(scriptContext));
        *pResult = ToNumber_Full(aValue, scriptContext);
    }

    // Used for the JIT's float type specialization
    // Convert aValue to double, but only allow primitives.  Return false otherwise.
    BOOL JavascriptConversion::ToNumber_FromPrimitive(Var aValue, double *pResult, BOOL allowUndefined, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 875\n");
        Assert(Js::JavascriptStackWalker::ValidateTopJitFrame(scriptContext));
        Assert(!TaggedNumber::Is(aValue));
        RecyclableObject *obj = RecyclableObject::FromVar(aValue);

        // NOTE: Don't allow strings, otherwise JIT's float type specialization has to worry about concats
        if (obj->GetTypeId() >= TypeIds_String)
        {LOGMEIN("JavascriptConversion.cpp] 882\n");
            return false;
        }
        if (!allowUndefined && obj->GetTypeId() == TypeIds_Undefined)
        {LOGMEIN("JavascriptConversion.cpp] 886\n");
            return false;
        }

        *pResult = ToNumber_Full(aValue, scriptContext);
        return true;
    }

    //----------------------------------------------------------------------------
    // ToNumber_Full:
    // Implements ES6 Draft Rev 26 July 18, 2014
    //
    // Undefined: NaN
    // Null:      0
    // boolean:   v==true ? 1 : 0 ;
    // number:    v (original number)
    // String:    conversion by spec algorithm
    // object:    ToNumber(PrimitiveValue(v, hint_number))
    // Symbol:    TypeError
    //----------------------------------------------------------------------------
    double JavascriptConversion::ToNumber_Full(Var aValue,ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 907\n");
        AssertMsg(!TaggedInt::Is(aValue), "Should be detected");
        ScriptContext * objectScriptContext = RecyclableObject::Is(aValue) ? RecyclableObject::FromVar(aValue)->GetScriptContext() : nullptr;
        BOOL fPrimitiveOnly = false;
        while(true)
        {LOGMEIN("JavascriptConversion.cpp] 912\n");
            switch (JavascriptOperators::GetTypeId(aValue))
            {LOGMEIN("JavascriptConversion.cpp] 914\n");
            case TypeIds_Symbol:
                JavascriptError::TryThrowTypeError(objectScriptContext, scriptContext, JSERR_NeedNumber);
                // Fallthrough to return NaN if exceptions are disabled

            case TypeIds_Undefined:
                return JavascriptNumber::GetValue(scriptContext->GetLibrary()->GetNaN());

            case TypeIds_Null:
                return  0;

            case TypeIds_Integer:
                return TaggedInt::ToDouble(aValue);

            case TypeIds_Boolean:
                return JavascriptBoolean::FromVar(aValue)->GetValue() ? 1 : +0;

            case TypeIds_Number:
                return JavascriptNumber::GetValue(aValue);

            case TypeIds_Int64Number:
                return (double)JavascriptInt64Number::FromVar(aValue)->GetValue();

            case TypeIds_UInt64Number:
                return (double)JavascriptUInt64Number::FromVar(aValue)->GetValue();

            case TypeIds_String:
                return JavascriptString::FromVar(aValue)->ToDouble();

            case TypeIds_VariantDate:
                return Js::DateImplementation::GetTvUtc(Js::DateImplementation::JsLocalTimeFromVarDate(JavascriptVariantDate::FromVar(aValue)->GetValue()), scriptContext);

#ifdef ENABLE_SIMDJS
            case TypeIds_SIMDFloat32x4:
            case TypeIds_SIMDInt32x4:
            case TypeIds_SIMDInt16x8:
            case TypeIds_SIMDInt8x16:
            case TypeIds_SIMDFloat64x2:
            case TypeIds_SIMDBool32x4:
            case TypeIds_SIMDBool16x8:
            case TypeIds_SIMDBool8x16:
            case TypeIds_SIMDUint32x4:
            case TypeIds_SIMDUint16x8:
            case TypeIds_SIMDUint8x16:
                JavascriptError::ThrowError(scriptContext, JSERR_NeedNumber);
#endif

            default:
                {LOGMEIN("JavascriptConversion.cpp] 962\n");
                    AssertMsg(JavascriptOperators::IsObject(aValue), "bad type object in conversion ToInteger");
                    if(fPrimitiveOnly)
                    {LOGMEIN("JavascriptConversion.cpp] 965\n");
                        JavascriptError::ThrowError(scriptContext, VBSERR_OLENoPropOrMethod);
                    }
                    fPrimitiveOnly = true;
                    aValue = ToPrimitive(aValue, JavascriptHint::HintNumber, scriptContext);
                }
            }
        }
    }

    //----------------------------------------------------------------------------
    // second part of the ToInteger() implementation.(ES5.0: S9.4).
    //----------------------------------------------------------------------------
    double JavascriptConversion::ToInteger_Full(Var aValue,ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 979\n");
        AssertMsg(!TaggedInt::Is(aValue), "Should be detected");
        ScriptContext * objectScriptContext = RecyclableObject::Is(aValue) ? RecyclableObject::FromVar(aValue)->GetScriptContext() : nullptr;
        BOOL fPrimitiveOnly = false;
        while(true)
        {LOGMEIN("JavascriptConversion.cpp] 984\n");
            switch (JavascriptOperators::GetTypeId(aValue))
            {LOGMEIN("JavascriptConversion.cpp] 986\n");
            case TypeIds_Symbol:
                JavascriptError::TryThrowTypeError(objectScriptContext, scriptContext, JSERR_NeedNumber);
                // Fallthrough to return 0 if exceptions are disabled
            case TypeIds_Undefined:
            case TypeIds_Null:
                return 0;

            case TypeIds_Integer:
                return TaggedInt::ToInt32(aValue);

            case TypeIds_Boolean:
                return JavascriptBoolean::FromVar(aValue)->GetValue() ? 1 : +0;

            case TypeIds_Number:
                return ToInteger(JavascriptNumber::GetValue(aValue));

            case TypeIds_Int64Number:
                return ToInteger((double)JavascriptInt64Number::FromVar(aValue)->GetValue());

            case TypeIds_UInt64Number:
                return ToInteger((double)JavascriptUInt64Number::FromVar(aValue)->GetValue());

            case TypeIds_String:
                return ToInteger(JavascriptString::FromVar(aValue)->ToDouble());

            case TypeIds_VariantDate:
                return ToInteger(ToNumber_Full(aValue, scriptContext));

#ifdef ENABLE_SIMDJS
            case TypeIds_SIMDFloat32x4:
            case TypeIds_SIMDFloat64x2:
            case TypeIds_SIMDInt32x4:
            case TypeIds_SIMDInt16x8:
            case TypeIds_SIMDInt8x16:
            case TypeIds_SIMDBool32x4:
            case TypeIds_SIMDBool16x8:
            case TypeIds_SIMDBool8x16:
            case TypeIds_SIMDUint32x4:
            case TypeIds_SIMDUint16x8:
            case TypeIds_SIMDUint8x16:
                JavascriptError::ThrowError(scriptContext, JSERR_NeedNumber);
#endif

            default:
                {LOGMEIN("JavascriptConversion.cpp] 1031\n");
                    AssertMsg(JavascriptOperators::IsObject(aValue), "bad type object in conversion ToInteger");
                    if(fPrimitiveOnly)
                    {
                        AssertMsg(FALSE, "wrong call in ToInteger_Full, no dynamic objects should get here");
                        JavascriptError::ThrowError(scriptContext, VBSERR_OLENoPropOrMethod);
                    }
                    fPrimitiveOnly = true;
                    aValue = ToPrimitive(aValue, JavascriptHint::HintNumber, scriptContext);
                }
            }
        }
    }

    double JavascriptConversion::ToInteger(double val)
    {LOGMEIN("JavascriptConversion.cpp] 1046\n");
        if(JavascriptNumber::IsNan(val))
            return 0;
        if(JavascriptNumber::IsPosInf(val) || JavascriptNumber::IsNegInf(val) ||
            JavascriptNumber::IsZero(val))
        {LOGMEIN("JavascriptConversion.cpp] 1051\n");
            return val;
        }

        return ( ((val < 0) ? -1 : 1 ) * floor(fabs(val)));
    }

    //----------------------------------------------------------------------------
    // ToInt32() converts the given Var to an Int32 value, as described in
    // (ES3.0: S9.5).
    //----------------------------------------------------------------------------
    int32 JavascriptConversion::ToInt32_Full(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 1063\n");
        Assert(Js::JavascriptStackWalker::ValidateTopJitFrame(scriptContext));
        AssertMsg(!TaggedInt::Is(aValue), "Should be detected");

        ScriptContext * objectScriptContext = RecyclableObject::Is(aValue) ? RecyclableObject::FromVar(aValue)->GetScriptContext() : nullptr;
        // This is used when TaggedInt's overflow but remain under int32
        // so Number is our most critical case:

        TypeId typeId = JavascriptOperators::GetTypeId(aValue);

        if (typeId == TypeIds_Number)
        {LOGMEIN("JavascriptConversion.cpp] 1074\n");
            return JavascriptMath::ToInt32Core(JavascriptNumber::GetValue(aValue));
        }

        switch (typeId)
        {LOGMEIN("JavascriptConversion.cpp] 1079\n");
        case TypeIds_Symbol:
            JavascriptError::TryThrowTypeError(objectScriptContext, scriptContext, JSERR_NeedNumber);
            // Fallthrough to return 0 if exceptions are disabled
        case TypeIds_Undefined:
        case TypeIds_Null:
            return  0;

        case TypeIds_Integer:
            return TaggedInt::ToInt32(aValue);

        case TypeIds_Boolean:
            return JavascriptBoolean::FromVar(aValue)->GetValue() ? 1 : +0;

        case TypeIds_Int64Number:
            // we won't lose precision if the int64 is within 32bit boundary; otherwise we need to
            // treat it as double anyhow.
            return JavascriptMath::ToInt32Core((double)JavascriptInt64Number::FromVar(aValue)->GetValue());

        case TypeIds_UInt64Number:
            // we won't lose precision if the int64 is within 32bit boundary; otherwise we need to
            // treat it as double anyhow.
            return JavascriptMath::ToInt32Core((double)JavascriptUInt64Number::FromVar(aValue)->GetValue());

        case TypeIds_String:
        {LOGMEIN("JavascriptConversion.cpp] 1104\n");
            double result;
            if (JavascriptString::FromVar(aValue)->ToDouble(&result))
            {LOGMEIN("JavascriptConversion.cpp] 1107\n");
                return JavascriptMath::ToInt32Core(result);
            }
            // If the string isn't a valid number, ToDouble returns NaN, and ToInt32 of that is 0
            return 0;
        }

        case TypeIds_VariantDate:
            return ToInt32(ToNumber_Full(aValue, scriptContext));

#ifdef ENABLE_SIMDJS
        case TypeIds_SIMDFloat32x4:
        case TypeIds_SIMDFloat64x2:
        case TypeIds_SIMDInt32x4:
        case TypeIds_SIMDInt16x8:
        case TypeIds_SIMDInt8x16:
        case TypeIds_SIMDBool32x4:
        case TypeIds_SIMDBool16x8:
        case TypeIds_SIMDBool8x16:
        case TypeIds_SIMDUint32x4:
        case TypeIds_SIMDUint16x8:
        case TypeIds_SIMDUint8x16:
            JavascriptError::ThrowError(scriptContext, JSERR_NeedNumber);
#endif

        default:
            AssertMsg(JavascriptOperators::IsObject(aValue), "bad type object in conversion ToInteger32");
            aValue = ToPrimitive(aValue, JavascriptHint::HintNumber, scriptContext);
        }

        switch (JavascriptOperators::GetTypeId(aValue))
        {LOGMEIN("JavascriptConversion.cpp] 1138\n");
        case TypeIds_Symbol:
            JavascriptError::TryThrowTypeError(objectScriptContext, scriptContext, JSERR_NeedNumber);
            // Fallthrough to return 0 if exceptions are disabled
        case TypeIds_Undefined:
        case TypeIds_Null:
            return  0;

        case TypeIds_Integer:
            return TaggedInt::ToInt32(aValue);

        case TypeIds_Boolean:
            return JavascriptBoolean::FromVar(aValue)->GetValue() ? 1 : +0;

        case TypeIds_Number:
            return ToInt32(JavascriptNumber::GetValue(aValue));

        case TypeIds_Int64Number:
            // we won't lose precision if the int64 is within 32bit boundary; otherwise we need to
            // treat it as double anyhow.
            return JavascriptMath::ToInt32Core((double)JavascriptInt64Number::FromVar(aValue)->GetValue());

        case TypeIds_UInt64Number:
            // we won't lose precision if the int64 is within 32bit boundary; otherwise we need to
            // treat it as double anyhow.
            return JavascriptMath::ToInt32Core((double)JavascriptUInt64Number::FromVar(aValue)->GetValue());

        case TypeIds_String:
        {LOGMEIN("JavascriptConversion.cpp] 1166\n");
            double result;
            if (JavascriptString::FromVar(aValue)->ToDouble(&result))
            {LOGMEIN("JavascriptConversion.cpp] 1169\n");
                return ToInt32(result);
            }
            // If the string isn't a valid number, ToDouble returns NaN, and ToInt32 of that is 0
            return 0;
        }

        case TypeIds_VariantDate:
            return ToInt32(ToNumber_Full(aValue, scriptContext));

        default:
            AssertMsg(FALSE, "wrong call in ToInteger32_Full, no dynamic objects should get here.");
            JavascriptError::ThrowError(scriptContext, VBSERR_OLENoPropOrMethod);
        }
    }

    // a strict version of ToInt32 conversion that returns false for non int32 values like, inf, NaN, undef
    BOOL JavascriptConversion::ToInt32Finite(Var aValue, ScriptContext* scriptContext, int32* result)
    {LOGMEIN("JavascriptConversion.cpp] 1187\n");
        ScriptContext * objectScriptContext = RecyclableObject::Is(aValue) ? RecyclableObject::FromVar(aValue)->GetScriptContext() : nullptr;
        BOOL fPrimitiveOnly = false;
        while(true)
        {LOGMEIN("JavascriptConversion.cpp] 1191\n");
            switch (JavascriptOperators::GetTypeId(aValue))
            {LOGMEIN("JavascriptConversion.cpp] 1193\n");
            case TypeIds_Symbol:
                JavascriptError::TryThrowTypeError(objectScriptContext, scriptContext, JSERR_NeedNumber);
                // Fallthrough to return false and set result to 0 if exceptions are disabled
            case TypeIds_Undefined:
                *result = 0;
                return false;

            case TypeIds_Null:
                *result = 0;
                return true;

            case TypeIds_Integer:
                *result = TaggedInt::ToInt32(aValue);
                return true;

            case TypeIds_Boolean:
                *result = JavascriptBoolean::FromVar(aValue)->GetValue() ? 1 : +0;
                return true;

            case TypeIds_Number:
                return ToInt32Finite(JavascriptNumber::GetValue(aValue), result);

            case TypeIds_Int64Number:
                // we won't lose precision if the int64 is within 32bit boundary; otherwise we need to
                // treat it as double anyhow.
                return ToInt32Finite((double)JavascriptInt64Number::FromVar(aValue)->GetValue(), result);

            case TypeIds_UInt64Number:
                // we won't lose precision if the int64 is within 32bit boundary; otherwise we need to
                // treat it as double anyhow.
                return ToInt32Finite((double)JavascriptUInt64Number::FromVar(aValue)->GetValue(), result);

            case TypeIds_String:
                return ToInt32Finite(JavascriptString::FromVar(aValue)->ToDouble(), result);

            case TypeIds_VariantDate:
                return ToInt32Finite(ToNumber_Full(aValue, scriptContext), result);

#ifdef ENABLE_SIMDJS
            case TypeIds_SIMDFloat32x4:
            case TypeIds_SIMDFloat64x2:
            case TypeIds_SIMDInt32x4:
            case TypeIds_SIMDInt16x8:
            case TypeIds_SIMDInt8x16:
            case TypeIds_SIMDBool32x4:
            case TypeIds_SIMDBool16x8:
            case TypeIds_SIMDBool8x16:
            case TypeIds_SIMDUint32x4:
            case TypeIds_SIMDUint16x8:
            case TypeIds_SIMDUint8x16:
                JavascriptError::ThrowError(scriptContext, JSERR_NeedNumber);
#endif

            default:
                {LOGMEIN("JavascriptConversion.cpp] 1248\n");
                    AssertMsg(JavascriptOperators::IsObject(aValue), "bad type object in conversion ToInteger32");
                    if(fPrimitiveOnly)
                    {
                        AssertMsg(FALSE, "wrong call in ToInteger32_Full, no dynamic objects should get here");
                        JavascriptError::ThrowError(scriptContext, VBSERR_OLENoPropOrMethod);
                    }
                    fPrimitiveOnly = true;
                    aValue = ToPrimitive(aValue, JavascriptHint::HintNumber, scriptContext);
                }
            }
        }
    }

    int32 JavascriptConversion::ToInt32(double T1)
    {LOGMEIN("JavascriptConversion.cpp] 1263\n");
        return JavascriptMath::ToInt32Core(T1);
    }

    __int64 JavascriptConversion::ToInt64(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 1268\n");
        switch (JavascriptOperators::GetTypeId(aValue))
        {LOGMEIN("JavascriptConversion.cpp] 1270\n");
        case TypeIds_Integer:
            {LOGMEIN("JavascriptConversion.cpp] 1272\n");
                return TaggedInt::ToInt32(aValue);
            }
        case TypeIds_Int64Number:
            {LOGMEIN("JavascriptConversion.cpp] 1276\n");
            JavascriptInt64Number* int64Number = JavascriptInt64Number::FromVar(aValue);
            return int64Number->GetValue();
            }
        case TypeIds_UInt64Number:
            {LOGMEIN("JavascriptConversion.cpp] 1281\n");
            JavascriptUInt64Number* uint64Number = JavascriptUInt64Number::FromVar(aValue);
            return (__int64)uint64Number->GetValue();
            }
        case TypeIds_Number:
            return JavascriptMath::TryToInt64(JavascriptNumber::GetValue(aValue));
        default:
            return (unsigned __int64)JavascriptConversion::ToInt32_Full(aValue, scriptContext);
        }
    }

    unsigned __int64 JavascriptConversion::ToUInt64(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 1293\n");
        switch (JavascriptOperators::GetTypeId(aValue))
        {LOGMEIN("JavascriptConversion.cpp] 1295\n");
        case TypeIds_Integer:
            {LOGMEIN("JavascriptConversion.cpp] 1297\n");
                return (unsigned __int64)TaggedInt::ToInt32(aValue);
            }
        case TypeIds_Int64Number:
            {LOGMEIN("JavascriptConversion.cpp] 1301\n");
            JavascriptInt64Number* int64Number = JavascriptInt64Number::FromVar(aValue);
            return (unsigned __int64)int64Number->GetValue();
            }
        case TypeIds_UInt64Number:
            {LOGMEIN("JavascriptConversion.cpp] 1306\n");
            JavascriptUInt64Number* uint64Number = JavascriptUInt64Number::FromVar(aValue);
            return uint64Number->GetValue();
            }
        case TypeIds_Number:
            return static_cast<unsigned __int64>(JavascriptMath::TryToInt64(JavascriptNumber::GetValue(aValue)));
        default:
            return (unsigned __int64)JavascriptConversion::ToInt32_Full(aValue, scriptContext);
        }
    }

    BOOL JavascriptConversion::ToInt32Finite(double value, int32* result)
    {LOGMEIN("JavascriptConversion.cpp] 1318\n");
        if((!NumberUtilities::IsFinite(value)) || JavascriptNumber::IsNan(value))
        {LOGMEIN("JavascriptConversion.cpp] 1320\n");
            *result = 0;
            return false;
        }
        else
        {
            *result = JavascriptMath::ToInt32Core(value);
            return true;
        }
    }

    //----------------------------------------------------------------------------
    // (ES3.0: S9.6).
    //----------------------------------------------------------------------------
    uint32 JavascriptConversion::ToUInt32_Full(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 1335\n");
        AssertMsg(!TaggedInt::Is(aValue), "Should be detected");
        ScriptContext * objectScriptContext = RecyclableObject::Is(aValue) ? RecyclableObject::FromVar(aValue)->GetScriptContext() : nullptr;
        BOOL fPrimitiveOnly = false;
        while(true)
        {LOGMEIN("JavascriptConversion.cpp] 1340\n");
            switch (JavascriptOperators::GetTypeId(aValue))
            {LOGMEIN("JavascriptConversion.cpp] 1342\n");
            case TypeIds_Symbol:
                JavascriptError::TryThrowTypeError(objectScriptContext, scriptContext, JSERR_NeedNumber);
                // Fallthrough to return 0 if exceptions are disabled
            case TypeIds_Undefined:
            case TypeIds_Null:
                return  0;

            case TypeIds_Integer:
                return TaggedInt::ToUInt32(aValue);

            case TypeIds_Boolean:
                return JavascriptBoolean::FromVar(aValue)->GetValue() ? 1 : +0;

            case TypeIds_Number:
                return JavascriptMath::ToUInt32(JavascriptNumber::GetValue(aValue));

            case TypeIds_Int64Number:
                // we won't lose precision if the int64 is within 32bit boundary; otherwise we need to
                // treat it as double anyhow.
                return JavascriptMath::ToUInt32((double)JavascriptInt64Number::FromVar(aValue)->GetValue());

            case TypeIds_UInt64Number:
                // we won't lose precision if the int64 is within 32bit boundary; otherwise we need to
                // treat it as double anyhow.
                return JavascriptMath::ToUInt32((double)JavascriptUInt64Number::FromVar(aValue)->GetValue());

            case TypeIds_String:
            {LOGMEIN("JavascriptConversion.cpp] 1370\n");
                double result;
                if (JavascriptString::FromVar(aValue)->ToDouble(&result))
                {LOGMEIN("JavascriptConversion.cpp] 1373\n");
                    return JavascriptMath::ToUInt32(result);
                }
                // If the string isn't a valid number, ToDouble returns NaN, and ToUInt32 of that is 0
                return 0;
            }

            case TypeIds_VariantDate:
                return JavascriptMath::ToUInt32(ToNumber_Full(aValue, scriptContext));

#ifdef ENABLE_SIMDJS
            case TypeIds_SIMDFloat32x4:
            case TypeIds_SIMDFloat64x2:
            case TypeIds_SIMDInt32x4:
            case TypeIds_SIMDInt16x8:
            case TypeIds_SIMDInt8x16:
            case TypeIds_SIMDBool32x4:
            case TypeIds_SIMDBool16x8:
            case TypeIds_SIMDBool8x16:
            case TypeIds_SIMDUint32x4:
            case TypeIds_SIMDUint16x8:
            case TypeIds_SIMDUint8x16:
                JavascriptError::ThrowError(scriptContext, JSERR_NeedNumber);
#endif

            default:
                {LOGMEIN("JavascriptConversion.cpp] 1399\n");
                    AssertMsg(JavascriptOperators::IsObject(aValue), "bad type object in conversion ToUInt32");
                    if(fPrimitiveOnly)
                    {
                        AssertMsg(FALSE, "wrong call in ToUInt32_Full, no dynamic objects should get here");
                        JavascriptError::ThrowError(scriptContext, VBSERR_OLENoPropOrMethod);
                    }
                    fPrimitiveOnly = true;
                    aValue = ToPrimitive(aValue, JavascriptHint::HintNumber, scriptContext);
                }
            }
        }
    }

    uint32 JavascriptConversion::ToUInt32(double T1)
    {LOGMEIN("JavascriptConversion.cpp] 1414\n");
        // Same as doing ToInt32 and reinterpret the bits as uint32
        return (uint32)JavascriptMath::ToInt32Core(T1);
    }

    //----------------------------------------------------------------------------
    // ToUInt16() converts the given Var to a UInt16 value, as described in
    // (ES3.0: S9.6).
    //----------------------------------------------------------------------------
    uint16 JavascriptConversion::ToUInt16_Full(IN  Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 1424\n");
        AssertMsg(!TaggedInt::Is(aValue), "Should be detected");
        ScriptContext * objectScriptContext = RecyclableObject::Is(aValue) ? RecyclableObject::FromVar(aValue)->GetScriptContext() : nullptr;
        BOOL fPrimitiveOnly = false;
        while(true)
        {LOGMEIN("JavascriptConversion.cpp] 1429\n");
            switch (JavascriptOperators::GetTypeId(aValue))
            {LOGMEIN("JavascriptConversion.cpp] 1431\n");
            case TypeIds_Symbol:
                JavascriptError::TryThrowTypeError(objectScriptContext, scriptContext, JSERR_NeedNumber);
                // Fallthrough to return 0 if exceptions are disabled
            case TypeIds_Undefined:
            case TypeIds_Null:
                return  0;

            case TypeIds_Integer:
                return TaggedInt::ToUInt16(aValue);

            case TypeIds_Boolean:
                return JavascriptBoolean::FromVar(aValue)->GetValue() ? 1 : +0;

            case TypeIds_Number:
                return ToUInt16(JavascriptNumber::GetValue(aValue));

            case TypeIds_Int64Number:
                // we won't lose precision if the int64 is within 16bit boundary; otherwise we need to
                // treat it as double anyhow.
                return ToUInt16((double)JavascriptInt64Number::FromVar(aValue)->GetValue());

            case TypeIds_UInt64Number:
                // we won't lose precision if the int64 is within 16bit boundary; otherwise we need to
                // treat it as double anyhow.
                return ToUInt16((double)JavascriptUInt64Number::FromVar(aValue)->GetValue());

            case TypeIds_String:
            {LOGMEIN("JavascriptConversion.cpp] 1459\n");
                double result;
                if (JavascriptString::FromVar(aValue)->ToDouble(&result))
                {LOGMEIN("JavascriptConversion.cpp] 1462\n");
                    return ToUInt16(result);
                }
                // If the string isn't a valid number, ToDouble is NaN, and ToUInt16 of that is 0
                return 0;
            }

            case TypeIds_VariantDate:
                return ToUInt16(ToNumber_Full(aValue, scriptContext));

#ifdef ENABLE_SIMDJS
            case TypeIds_SIMDFloat32x4:
            case TypeIds_SIMDFloat64x2:
            case TypeIds_SIMDInt32x4:
            case TypeIds_SIMDInt16x8:
            case TypeIds_SIMDInt8x16:
            case TypeIds_SIMDBool32x4:
            case TypeIds_SIMDBool16x8:
            case TypeIds_SIMDBool8x16:
            case TypeIds_SIMDUint32x4:
            case TypeIds_SIMDUint16x8:
            case TypeIds_SIMDUint8x16:
                JavascriptError::ThrowError(scriptContext, JSERR_NeedNumber);
#endif

            default:
                {LOGMEIN("JavascriptConversion.cpp] 1488\n");
                    AssertMsg(JavascriptOperators::IsObject(aValue), "bad type object in conversion ToUIn16");
                    if(fPrimitiveOnly)
                    {
                        AssertMsg(FALSE, "wrong call in ToUInt16, no dynamic objects should get here");
                        JavascriptError::ThrowError(scriptContext, VBSERR_OLENoPropOrMethod);
                    }
                    fPrimitiveOnly = true;
                    aValue = ToPrimitive(aValue, JavascriptHint::HintNumber, scriptContext);
                }
            }
        }
    }

    inline uint16 JavascriptConversion::ToUInt16(double T1)
    {LOGMEIN("JavascriptConversion.cpp] 1503\n");
        //
        // VC does the right thing here, if we first convert to uint32 and then to uint16
        // Spec says mod should be done.
        //

        uint32 result = JavascriptConversion::ToUInt32(T1);
#if defined(_M_IX86) && _MSC_FULL_VER < 160030329
        // Well VC doesn't actually do the right thing...  It takes (uint16)(uint32)double and removes the
        // middle uint32 cast to (uint16)double, which isn't the same thing.  Somehow, it only seems to be a
        // problem for x86. Forcing a store to uint32 prevents the incorrect optimization.
        //
        // A bug has been filled in the Dev11 database: TF bug id #901495
        // Fixed in compiler 16.00.30329.00
        volatile uint32 volResult = result;
#endif
        return (uint16) result;
    }

    JavascriptString * JavascriptConversion::ToPrimitiveString(Var aValue, ScriptContext * scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 1523\n");
        return ToString(ToPrimitive(aValue, JavascriptHint::None, scriptContext), scriptContext);
    }

    double JavascriptConversion::LongToDouble(__int64 aValue)
    {LOGMEIN("JavascriptConversion.cpp] 1528\n");
        return static_cast<double>(aValue);
    }

    double JavascriptConversion::ULongToDouble(unsigned __int64 aValue)
    {LOGMEIN("JavascriptConversion.cpp] 1533\n");
        return static_cast<double>(aValue);
    }

    float JavascriptConversion::LongToFloat(__int64 aValue)
    {LOGMEIN("JavascriptConversion.cpp] 1538\n");
        return static_cast<float>(aValue);
    }

    float JavascriptConversion::ULongToFloat (unsigned __int64 aValue)
    {LOGMEIN("JavascriptConversion.cpp] 1543\n");
        return static_cast<float>(aValue);
    }

    int32 JavascriptConversion::F32TOI32(float src, ScriptContext * ctx)
    {LOGMEIN("JavascriptConversion.cpp] 1548\n");
        if (Wasm::WasmMath::isInRange<float, uint32, NumberConstants::k_Float32TwoTo31, NumberConstants::k_Float32NegZero, NumberConstants::k_Float32NegTwoTo31,
            &Wasm::WasmMath::LessThan<uint32>, &Wasm::WasmMath::LessOrEqual<uint32>>(src) &&
            !Wasm::WasmMath::isNaN<float>(src))
        {LOGMEIN("JavascriptConversion.cpp] 1552\n");
            return (int32)src;
        }

        JavascriptError::ThrowWebAssemblyRuntimeError(ctx, VBSERR_Overflow);
    }

    uint32 JavascriptConversion::F32TOU32(float src, ScriptContext * ctx)
    {LOGMEIN("JavascriptConversion.cpp] 1560\n");
        if (Wasm::WasmMath::isInRange<float, uint32, NumberConstants::k_Float32TwoTo32, NumberConstants::k_Float32NegZero, NumberConstants::k_Float32NegOne,
            &Wasm::WasmMath::LessThan<uint32>, &Wasm::WasmMath::LessThan<uint32>>(src) &&
            !Wasm::WasmMath::isNaN<float>(src))
        {LOGMEIN("JavascriptConversion.cpp] 1564\n");
            return (uint32)src;
        }

        JavascriptError::ThrowWebAssemblyRuntimeError(ctx, VBSERR_Overflow);
    }

    int32 JavascriptConversion::F64TOI32(double src, ScriptContext * ctx)
    {LOGMEIN("JavascriptConversion.cpp] 1572\n");
        if (Wasm::WasmMath::isInRange<double, uint64, NumberConstants::k_TwoTo31, NumberConstants::k_NegZero, NumberConstants::k_NegTwoTo31,
            &Wasm::WasmMath::LessOrEqual<uint64>, &Wasm::WasmMath::LessOrEqual<uint64>>(src) &&
            !Wasm::WasmMath::isNaN<double>(src))
        {LOGMEIN("JavascriptConversion.cpp] 1576\n");
            return (int32)src;
        }

        JavascriptError::ThrowWebAssemblyRuntimeError(ctx, VBSERR_Overflow);
    }

    uint32 JavascriptConversion::F64TOU32(double src, ScriptContext * ctx)
    {LOGMEIN("JavascriptConversion.cpp] 1584\n");
        if (Wasm::WasmMath::isInRange<double, uint64, NumberConstants::k_TwoTo32, NumberConstants::k_NegZero, NumberConstants::k_NegOne,
            &Wasm::WasmMath::LessOrEqual<uint64>, &Wasm::WasmMath::LessThan<uint64>>(src)
            && !Wasm::WasmMath::isNaN<double>(src))
        {LOGMEIN("JavascriptConversion.cpp] 1588\n");
            return (uint32)src;
        }

        JavascriptError::ThrowWebAssemblyRuntimeError(ctx, VBSERR_Overflow);
    }

    int64 JavascriptConversion::F32TOI64(float src, ScriptContext * ctx)
    {LOGMEIN("JavascriptConversion.cpp] 1596\n");
        if (Wasm::WasmMath::isInRange<float, uint32, NumberConstants::k_Float32TwoTo63, NumberConstants::k_Float32NegZero, NumberConstants::k_Float32NegTwoTo63,
            &Wasm::WasmMath::LessThan<uint32>, &Wasm::WasmMath::LessOrEqual<uint32>>(src) &&
            !Wasm::WasmMath::isNaN<float>(src))
        {LOGMEIN("JavascriptConversion.cpp] 1600\n");
            return (int64)src;
        }

        JavascriptError::ThrowWebAssemblyRuntimeError(ctx, VBSERR_Overflow);
    }

    uint64 JavascriptConversion::F32TOU64(float src, ScriptContext * ctx)
    {LOGMEIN("JavascriptConversion.cpp] 1608\n");
        if (Wasm::WasmMath::isInRange<float, uint32, NumberConstants::k_Float32TwoTo64, NumberConstants::k_Float32NegZero, NumberConstants::k_Float32NegOne,
            &Wasm::WasmMath::LessThan<uint32>, &Wasm::WasmMath::LessThan<uint32>>(src) &&
            !Wasm::WasmMath::isNaN<float>(src))
        {LOGMEIN("JavascriptConversion.cpp] 1612\n");
            return (uint64)src;
        }

        JavascriptError::ThrowWebAssemblyRuntimeError(ctx, VBSERR_Overflow);
    }

    int64 JavascriptConversion::F64TOI64(double src, ScriptContext * ctx)
    {LOGMEIN("JavascriptConversion.cpp] 1620\n");
        if (Wasm::WasmMath::isInRange<double, uint64, NumberConstants::k_TwoTo63, NumberConstants::k_NegZero, NumberConstants::k_NegTwoTo63,
            &Wasm::WasmMath::LessThan<uint64>, &Wasm::WasmMath::LessOrEqual<uint64>>(src) &&
            !Wasm::WasmMath::isNaN<double>(src))
        {LOGMEIN("JavascriptConversion.cpp] 1624\n");
            return (int64)src;
        }

        JavascriptError::ThrowWebAssemblyRuntimeError(ctx, VBSERR_Overflow);
    }

    uint64 JavascriptConversion::F64TOU64(double src, ScriptContext * ctx)
    {LOGMEIN("JavascriptConversion.cpp] 1632\n");
        if (Wasm::WasmMath::isInRange<double, uint64, NumberConstants::k_TwoTo64, NumberConstants::k_NegZero, NumberConstants::k_NegOne,
            &Wasm::WasmMath::LessThan<uint64>, &Wasm::WasmMath::LessThan<uint64>>(src) &&
            !Wasm::WasmMath::isNaN<double>(src))
        {LOGMEIN("JavascriptConversion.cpp] 1636\n");
            return (uint64)src;
        }

        JavascriptError::ThrowWebAssemblyRuntimeError(ctx, VBSERR_Overflow);
    }

    int64 JavascriptConversion::ToLength(Var aValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptConversion.cpp] 1644\n");
        if (TaggedInt::Is(aValue))
        {LOGMEIN("JavascriptConversion.cpp] 1646\n");
            int64 length = TaggedInt::ToInt64(aValue);
            return (length < 0) ? 0 : length;
        }

        double length = JavascriptConversion::ToInteger(aValue, scriptContext);

        if (length < 0.0 || JavascriptNumber::IsNegZero(length))
        {LOGMEIN("JavascriptConversion.cpp] 1654\n");
            length = 0.0;
        }
        else if (length > Math::MAX_SAFE_INTEGER)
        {LOGMEIN("JavascriptConversion.cpp] 1658\n");
            length = Math::MAX_SAFE_INTEGER;
        }

        return NumberUtilities::TryToInt64(length);
    }
} // namespace Js
