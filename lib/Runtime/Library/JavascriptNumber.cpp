//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "errstr.h"
#include "Library/EngineInterfaceObject.h"
#include "Library/IntlEngineInterfaceExtensionObject.h"

using namespace PlatformAgnostic;

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(JavascriptNumber);

    Var JavascriptNumber::ToVarNoCheck(double value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 16\n");
        return JavascriptNumber::NewInlined(value, scriptContext);
    }

    Var JavascriptNumber::ToVarWithCheck(double value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 21\n");
#if FLOATVAR
        if (IsNan(value))
        {LOGMEIN("JavascriptNumber.cpp] 24\n");
            value = JavascriptNumber::NaN;
        }
#endif
        return JavascriptNumber::NewInlined(value, scriptContext);
    }

    Var JavascriptNumber::ToVarInPlace(double value, ScriptContext* scriptContext, JavascriptNumber *result)
    {LOGMEIN("JavascriptNumber.cpp] 32\n");
        return InPlaceNew(value, scriptContext, result);
    }

    Var JavascriptNumber::ToVarInPlace(int64 value, ScriptContext* scriptContext, JavascriptNumber *result)
    {LOGMEIN("JavascriptNumber.cpp] 37\n");
        return InPlaceNew((double)value, scriptContext, result);
    }


    Var JavascriptNumber::ToVarMaybeInPlace(double value, ScriptContext* scriptContext, JavascriptNumber *result)
    {LOGMEIN("JavascriptNumber.cpp] 43\n");
        if (result)
        {LOGMEIN("JavascriptNumber.cpp] 45\n");
            return InPlaceNew(value, scriptContext, result);
        }

        return ToVarNoCheck(value, scriptContext);
    }

    Var JavascriptNumber::ToVarInPlace(int32 nValue, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {LOGMEIN("JavascriptNumber.cpp] 53\n");
        if (!TaggedInt::IsOverflow(nValue))
        {LOGMEIN("JavascriptNumber.cpp] 55\n");
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return InPlaceNew(static_cast<double>(nValue), scriptContext, result);
    }

    Var JavascriptNumber::ToVarInPlace(uint32 nValue, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {LOGMEIN("JavascriptNumber.cpp] 63\n");
        if (!TaggedInt::IsOverflow(nValue))
        {LOGMEIN("JavascriptNumber.cpp] 65\n");
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return InPlaceNew(static_cast<double>(nValue), scriptContext, result);
    }

    Var JavascriptNumber::ToVarIntCheck(double value,ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 73\n");
        //
        // Check if a well-known value:
        // - This significantly cuts down on the below floating-point to integer conversions.
        //

        if (value == 0.0)
        {LOGMEIN("JavascriptNumber.cpp] 80\n");
            if(IsNegZero(value))
            {LOGMEIN("JavascriptNumber.cpp] 82\n");
                return scriptContext->GetLibrary()->GetNegativeZero();
            }
            return TaggedInt::ToVarUnchecked(0);
        }
        if (value == 1.0)
        {LOGMEIN("JavascriptNumber.cpp] 88\n");
            return TaggedInt::ToVarUnchecked(1);
        }

        //
        // Check if number can be reduced back into a TaggedInt:
        // - This avoids extra GC.
        //

        int nValue      = (int) value;
        double dblCheck = (double) nValue;
        if ((dblCheck == value) && (!TaggedInt::IsOverflow(nValue)))
        {LOGMEIN("JavascriptNumber.cpp] 100\n");
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return JavascriptNumber::NewInlined(value,scriptContext);
    }

    bool JavascriptNumber::TryGetInt32OrUInt32Value(const double value, int32 *const int32Value, bool *const isInt32)
    {LOGMEIN("JavascriptNumber.cpp] 108\n");
        Assert(int32Value);
        Assert(isInt32);

        if(value <= 0)
        {LOGMEIN("JavascriptNumber.cpp] 113\n");
            return *isInt32 = TryGetInt32Value(value, int32Value);
        }

        const uint32 i = static_cast<uint32>(value);
        if(static_cast<double>(i) != value)
        {LOGMEIN("JavascriptNumber.cpp] 119\n");
            return false;
        }

        *int32Value = i;
        *isInt32 = static_cast<int32>(i) >= 0;
        return true;
    }

    bool JavascriptNumber::IsInt32(const double value)
    {LOGMEIN("JavascriptNumber.cpp] 129\n");
        int32 i;
        return TryGetInt32Value(value, &i);
    }

    bool JavascriptNumber::IsInt32OrUInt32(const double value)
    {LOGMEIN("JavascriptNumber.cpp] 135\n");
        int32 i;
        bool isInt32;
        return TryGetInt32OrUInt32Value(value, &i, &isInt32);
    }

    bool JavascriptNumber::IsInt32_NoChecks(const Var number)
    {LOGMEIN("JavascriptNumber.cpp] 142\n");
        Assert(number);
        Assert(Is(number));

        return IsInt32(GetValue(number));
    }

    bool JavascriptNumber::IsInt32OrUInt32_NoChecks(const Var number)
    {LOGMEIN("JavascriptNumber.cpp] 150\n");
        Assert(number);
        Assert(Is(number));

        return IsInt32OrUInt32(GetValue(number));
    }

    int32 JavascriptNumber::GetNonzeroInt32Value_NoTaggedIntCheck(const Var object)
    {LOGMEIN("JavascriptNumber.cpp] 158\n");
        Assert(object);
        Assert(!TaggedInt::Is(object));

        int32 i;
        return Is_NoTaggedIntCheck(object) && TryGetInt32Value(GetValue(object), &i) ? i : 0;
    }

    int32 JavascriptNumber::DirectPowIntInt(bool* isOverflow, int32 x, int32 y)
    {LOGMEIN("JavascriptNumber.cpp] 167\n");
        if (y < 0)
        {LOGMEIN("JavascriptNumber.cpp] 169\n");
            *isOverflow = true;
            return 0;
        }

        uint32 uexp = static_cast<uint32>(y);
        int32 result = 1;

        while (true)
        {LOGMEIN("JavascriptNumber.cpp] 178\n");
            if ((uexp & 1) != 0)
            {LOGMEIN("JavascriptNumber.cpp] 180\n");
                if (Int32Math::Mul(result, x, &result))
                {LOGMEIN("JavascriptNumber.cpp] 182\n");
                    *isOverflow = true;
                    break;
                }
            }
            if ((uexp >>= 1) == 0)
            {LOGMEIN("JavascriptNumber.cpp] 188\n");
                *isOverflow = false;
                break;
            }
            if (Int32Math::Mul(x, x, &x))
            {LOGMEIN("JavascriptNumber.cpp] 193\n");
                *isOverflow = true;
                break;
            }
        }

        return *isOverflow ? 0 : result;
    }

    double JavascriptNumber::DirectPowDoubleInt(double x, int32 y)
    {LOGMEIN("JavascriptNumber.cpp] 203\n");
        // For exponent in [-8, 8], aggregate the product according to binary representation
        // of exponent. This acceleration may lead to significant deviation for larger exponent
        if (y >= -8 && y <= 8)
        {LOGMEIN("JavascriptNumber.cpp] 207\n");
            uint32 uexp = static_cast<uint32>(y >= 0 ? y : -y);
            for (double result = 1.0; ; x *= x)
            {LOGMEIN("JavascriptNumber.cpp] 210\n");
                if ((uexp & 1) != 0)
                {LOGMEIN("JavascriptNumber.cpp] 212\n");
                    result *= x;
                }
                if ((uexp >>= 1) == 0)
                {LOGMEIN("JavascriptNumber.cpp] 216\n");
                    return (y < 0 ? (1.0 / result) : result);
                }
            }
        }

        // always call pow(double, double) in C runtime which has a bug to process pow(double, int).
        return ::pow(x, static_cast<double>(y));
    }

#if _M_IX86

    extern "C" double __cdecl __libm_sse2_pow(double, double);

    static const double d1_0 = 1.0;

#if !ENABLE_NATIVE_CODEGEN
    double JavascriptNumber::DirectPow(double x, double y)
    {LOGMEIN("JavascriptNumber.cpp] 234\n");
        return ::pow(x, y);
    }
#else

#pragma warning(push)
// C4740: flow in or out of inline asm code suppresses global optimization
// It is fine to disable glot opt on this function which is mostly written in assembly
#pragma warning(disable:4740)
    __declspec(naked)
    double JavascriptNumber::DirectPow(double x, double y)
    {LOGMEIN("JavascriptNumber.cpp] 245\n");
        UNREFERENCED_PARAMETER(x);
        UNREFERENCED_PARAMETER(y);

        double savedX, savedY, result;

        // This function is called directly from jitted, float-preferenced code.
        // It looks for x and y in xmm0 and xmm1 and returns the result in xmm0.
        // Check for pow(1, Infinity/NaN) and return NaN in that case;
        // then check fast path of small integer exponent, otherwise,
        // go to the fast CRT helper.
        __asm {
            // check y for 1.0
            ucomisd xmm1, d1_0
            jne pow_full
            jp pow_full
            ret
        pow_full:
            // Check y for non-finite value
            pextrw eax, xmm1, 3
            not eax
            test eax, 0x7ff0
            jne normal
            // check for |x| == 1
            movsd xmm2, xmm0
            andpd xmm2, AbsDoubleCst
            movsd xmm3, d1_0
            ucomisd xmm2, xmm3
            lahf
            test ah, 68
            jp normal
            movsd xmm0, JavascriptNumber::k_Nan
            ret
        normal:
            push ebp
            mov ebp, esp        // prepare stack frame for sub function call
            sub esp, 0x40       // 4 variables, reserve 0x10 for 1
            movsd savedX, xmm0
            movsd savedY, xmm1
        }

        int intY;
        if (TryGetInt32Value(savedY, &intY) && intY >= -8 && intY <= 8)
        {LOGMEIN("JavascriptNumber.cpp] 288\n");
            result = DirectPowDoubleInt(savedX, intY);
            __asm {LOGMEIN("JavascriptNumber.cpp] 290\n");
                movsd xmm0, result
            }
        }
        else
        {
            __asm {
                movsd xmm0, savedX
                movsd xmm1, savedY
                call dword ptr[__libm_sse2_pow]
            }
        }

        __asm {
            mov esp, ebp
            pop ebp
            ret
        }
    }
#pragma warning(pop)

#endif

#elif defined(_M_AMD64) || defined(_M_ARM32_OR_ARM64)

    double JavascriptNumber::DirectPow(double x, double y)
    {LOGMEIN("JavascriptNumber.cpp] 316\n");
        if(y == 1.0)
        {LOGMEIN("JavascriptNumber.cpp] 318\n");
            return x;
        }

        // For AMD64/ARM calling convention already uses SSE2/VFP registers so we don't have to use assembler.
        // We can't just use "if (0 == y)" because NaN compares
        // equal to 0 according to our compilers.
        int32 intY;
        if (0 == NumberUtilities::LuLoDbl(y) && 0 == (NumberUtilities::LuHiDbl(y) & 0x7FFFFFFF))
        {LOGMEIN("JavascriptNumber.cpp] 327\n");
            // pow(x, 0) = 1 even if x is NaN.
            return 1;
        }
        else if (1.0 == fabs(x) && !NumberUtilities::IsFinite(y))
        {LOGMEIN("JavascriptNumber.cpp] 332\n");
            // pow([+/-] 1, Infinity) = NaN according to javascript, but not for CRT pow.
            return JavascriptNumber::NaN;
        }
        else if (TryGetInt32Value(y, &intY))
        {LOGMEIN("JavascriptNumber.cpp] 337\n");
            // check fast path
            return DirectPowDoubleInt(x, intY);
        }

        return ::pow(x, y);
    }

#else

    double JavascriptNumber::DirectPow(double x, double y)
    {LOGMEIN("JavascriptNumber.cpp] 348\n");
        UNREFERENCED_PARAMETER(x);
        UNREFERENCED_PARAMETER(y);

        AssertMsg(0, "DirectPow NYI");
        return 0;
    }

#endif

    Var JavascriptNumber::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        //
        // Determine if called as a constructor or a function.
        //

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        Var result;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptNumber.cpp] 378\n");
            if (TaggedInt::Is(args[1]) || JavascriptNumber::Is(args[1]))
            {LOGMEIN("JavascriptNumber.cpp] 380\n");
                result = args[1];
            }
            else if (JavascriptNumberObject::Is(args[1]))
            {LOGMEIN("JavascriptNumber.cpp] 384\n");
                result = JavascriptNumber::ToVarNoCheck(JavascriptNumberObject::FromVar(args[1])->GetValue(), scriptContext);
            }
            else
            {
                result = JavascriptNumber::ToVarNoCheck(JavascriptConversion::ToNumber(args[1], scriptContext), scriptContext);
            }
        }
        else
        {
            result = TaggedInt::ToVarUnchecked(0);
        }

        if (callInfo.Flags & CallFlags_New)
        {LOGMEIN("JavascriptNumber.cpp] 398\n");
            JavascriptNumberObject* obj = scriptContext->GetLibrary()->CreateNumberObject(result);
            result = obj;
        }

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), RecyclableObject::FromVar(result), nullptr, scriptContext) :
            result;
    }

    ///----------------------------------------------------------------------------
    /// ParseInt() returns an integer value dictated by the interpretation of the
    /// given string argument according to the given radix argument, as described
    /// in (ES6.0: S20.1.2.13).
    ///
    /// Note: This is the same as the global parseInt() function as described in
    ///       (ES6.0: S18.2.5)
    ///
    /// We actually reuse GlobalObject::EntryParseInt, so no implementation here.
    ///----------------------------------------------------------------------------
    //Var JavascriptNumber::EntryParseInt(RecyclableObject* function, CallInfo callInfo, ...)

    ///----------------------------------------------------------------------------
    /// ParseFloat() returns a Number value dictated by the interpretation of the
    /// given string argument as a decimal literal, as described in
    /// (ES6.0: S20.1.2.12).
    ///
    /// Note: This is the same as the global parseFloat() function as described in
    ///       (ES6.0: S18.2.4)
    ///
    /// We actually reuse GlobalObject::EntryParseFloat, so no implementation here.
    ///----------------------------------------------------------------------------
    //Var JavascriptNumber::EntryParseFloat(RecyclableObject* function, CallInfo callInfo, ...)

    ///----------------------------------------------------------------------------
    /// IsNaN() return true if the given value is a Number value and is NaN, as
    /// described in (ES6.0: S20.1.2.4).
    /// Note: This is the same as the global isNaN() function as described in
    ///       (ES6.0: S18.2.3) except that it does not coerce the argument to
    ///       Number, instead returns false if not already a Number.
    ///----------------------------------------------------------------------------
    Var JavascriptNumber::EntryIsNaN(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Number_Constructor_isNaN);

        if (args.Info.Count < 2 || !JavascriptOperators::IsAnyNumberValue(args[1]))
        {LOGMEIN("JavascriptNumber.cpp] 451\n");
            return scriptContext->GetLibrary()->GetFalse();
        }

        return JavascriptBoolean::ToVar(
            JavascriptNumber::IsNan(JavascriptConversion::ToNumber(args[1],scriptContext)),
            scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// IsFinite() returns true if the given value is a Number value and is not
    /// one of NaN, +Infinity, or -Infinity, as described in (ES6.0: S20.1.2.2).
    /// Note: This is the same as the global isFinite() function as described in
    ///       (ES6.0: S18.2.2) except that it does not coerce the argument to
    ///       Number, instead returns false if not already a Number.
    ///----------------------------------------------------------------------------
    Var JavascriptNumber::EntryIsFinite(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Number_Constructor_isFinite);

        if (args.Info.Count < 2 || !JavascriptOperators::IsAnyNumberValue(args[1]))
        {LOGMEIN("JavascriptNumber.cpp] 479\n");
            return scriptContext->GetLibrary()->GetFalse();
        }

        return JavascriptBoolean::ToVar(
            NumberUtilities::IsFinite(JavascriptConversion::ToNumber(args[1],scriptContext)),
            scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// IsInteger() returns true if the given value is a Number value and is an
    /// integer, as described in (ES6.0: S20.1.2.3).
    ///----------------------------------------------------------------------------
    Var JavascriptNumber::EntryIsInteger(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Number_Constructor_isInteger);

        if (args.Info.Count < 2 || !JavascriptOperators::IsAnyNumberValue(args[1]))
        {LOGMEIN("JavascriptNumber.cpp] 504\n");
            return scriptContext->GetLibrary()->GetFalse();
        }

        double number = JavascriptConversion::ToNumber(args[1], scriptContext);

        return JavascriptBoolean::ToVar(
            number == JavascriptConversion::ToInteger(args[1], scriptContext) &&
            NumberUtilities::IsFinite(number),
            scriptContext);
    }

    ///----------------------------------------------------------------------------
    /// IsSafeInteger() returns true if the given value is a Number value and is an
    /// integer, as described in (ES6.0: S20.1.2.5).
    ///----------------------------------------------------------------------------
    Var JavascriptNumber::EntryIsSafeInteger(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Number_Constructor_isSafeInteger);

        if (args.Info.Count < 2 || !JavascriptOperators::IsAnyNumberValue(args[1]))
        {LOGMEIN("JavascriptNumber.cpp] 532\n");
            return scriptContext->GetLibrary()->GetFalse();
        }

        double number = JavascriptConversion::ToNumber(args[1], scriptContext);

        return JavascriptBoolean::ToVar(
            number == JavascriptConversion::ToInteger(args[1], scriptContext) &&
            number >= Js::Math::MIN_SAFE_INTEGER && number <= Js::Math::MAX_SAFE_INTEGER,
            scriptContext);
    }

    Var JavascriptNumber::EntryToExponential(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptNumber.cpp] 554\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toExponential"));
        }

        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToExp is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {LOGMEIN("JavascriptNumber.cpp] 563\n");
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {LOGMEIN("JavascriptNumber.cpp] 565\n");
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToExponential, args, &result))
                {LOGMEIN("JavascriptNumber.cpp] 568\n");
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toExponential"));
        }

        JavascriptString * nanF;
        if (nullptr != (nanF = ToStringNanOrInfinite(value, scriptContext)))
            return nanF;

        // If the Fraction param. is not present we have to output as many fractional digits as we can
        int fractionDigits = -1;

        if(args.Info.Count > 1)
        {LOGMEIN("JavascriptNumber.cpp] 584\n");
            //use the first arg as the fraction digits, ignore the rest.
            Var aFractionDigits = args[1];
            bool noRangeCheck = false;

            // shortcut for tagged int's
            if(TaggedInt::Is(aFractionDigits))
            {LOGMEIN("JavascriptNumber.cpp] 591\n");
                fractionDigits = TaggedInt::ToInt32(aFractionDigits);
            }
            else if(JavascriptOperators::GetTypeId(aFractionDigits) == TypeIds_Undefined)
            {LOGMEIN("JavascriptNumber.cpp] 595\n");
                // fraction undefined -> digits = -1, output as many fractional digits as we can
                noRangeCheck = true;
            }
            else
            {
                fractionDigits = (int)JavascriptConversion::ToInteger(aFractionDigits, scriptContext);
            }
            if(!noRangeCheck && (fractionDigits < 0 || fractionDigits >20))
            {LOGMEIN("JavascriptNumber.cpp] 604\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_FractionOutOfRange);
            }
        }

        return FormatDoubleToString(value, Js::NumberUtilities::FormatExponential, fractionDigits, scriptContext);
    }

    Var JavascriptNumber::EntryToFixed(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptNumber.cpp] 622\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toFixed"));
        }
        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToFixed is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {LOGMEIN("JavascriptNumber.cpp] 630\n");
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {LOGMEIN("JavascriptNumber.cpp] 632\n");
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToFixed, args, &result))
                {LOGMEIN("JavascriptNumber.cpp] 635\n");
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toFixed"));
        }
        int fractionDigits = 0;
        bool isFractionDigitsInfinite = false;
        if(args.Info.Count > 1)
        {LOGMEIN("JavascriptNumber.cpp] 645\n");
            //use the first arg as the fraction digits, ignore the rest.
            Var aFractionDigits = args[1];

            // shortcut for tagged int's
            if(TaggedInt::Is(aFractionDigits))
            {LOGMEIN("JavascriptNumber.cpp] 651\n");
                fractionDigits = TaggedInt::ToInt32(aFractionDigits);
            }
            else if(JavascriptOperators::GetTypeId(aFractionDigits) == TypeIds_Undefined)
            {LOGMEIN("JavascriptNumber.cpp] 655\n");
                // fraction digits = 0
            }
            else
            {
                double fractionDigitsRaw = JavascriptConversion::ToInteger(aFractionDigits, scriptContext);
                isFractionDigitsInfinite =
                    fractionDigitsRaw == JavascriptNumber::NEGATIVE_INFINITY ||
                    fractionDigitsRaw == JavascriptNumber::POSITIVE_INFINITY;
                fractionDigits = (int)fractionDigitsRaw;
            }
        }

        if (fractionDigits < 0 || fractionDigits > 20)
        {LOGMEIN("JavascriptNumber.cpp] 669\n");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FractionOutOfRange);
        }

        if(IsNan(value))
        {LOGMEIN("JavascriptNumber.cpp] 674\n");
            return ToStringNan(scriptContext);
        }
        if(value >= 1e21)
        {LOGMEIN("JavascriptNumber.cpp] 678\n");
            return ToStringRadix10(value, scriptContext);
        }

        return FormatDoubleToString(value, NumberUtilities::FormatFixed, fractionDigits, scriptContext);

    }

    Var JavascriptNumber::EntryToPrecision(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptNumber.cpp] 696\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toPrecision"));
        }
        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToPrec is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {LOGMEIN("JavascriptNumber.cpp] 704\n");
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {LOGMEIN("JavascriptNumber.cpp] 706\n");
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToPrecision, args, &result))
                {LOGMEIN("JavascriptNumber.cpp] 709\n");
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toPrecision"));
        }
        if(args.Info.Count < 2 || JavascriptOperators::GetTypeId(args[1]) == TypeIds_Undefined)
        {LOGMEIN("JavascriptNumber.cpp] 717\n");
            return JavascriptConversion::ToString(args[0], scriptContext);
        }

        int precision;
        Var aPrecision = args[1];
        if(TaggedInt::Is(aPrecision))
        {LOGMEIN("JavascriptNumber.cpp] 724\n");
            precision = TaggedInt::ToInt32(aPrecision);
        }
        else
        {
            precision = (int) JavascriptConversion::ToInt32(aPrecision, scriptContext);
        }

        JavascriptString * nanF;
        if (nullptr != (nanF = ToStringNanOrInfinite(value, scriptContext)))
        {LOGMEIN("JavascriptNumber.cpp] 734\n");
            return nanF;
        }

        if(precision < 1 || precision > 21)
        {LOGMEIN("JavascriptNumber.cpp] 739\n");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_PrecisionOutOfRange);
        }
        return FormatDoubleToString(value, NumberUtilities::FormatPrecision, precision, scriptContext);
    }

    Var JavascriptNumber::EntryToLocaleString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptNumber.cpp] 755\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toLocaleString"));
        }
        return JavascriptNumber::ToLocaleStringIntl(args, callInfo, scriptContext);
    }

    JavascriptString* JavascriptNumber::ToLocaleStringIntl(Var* values, CallInfo callInfo, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 762\n");
        Assert(values);
        ArgumentReader args(&callInfo, values);
        return JavascriptNumber::ToLocaleStringIntl(args, callInfo, scriptContext);
    }

    JavascriptString* JavascriptNumber::ToLocaleStringIntl(ArgumentReader& args, CallInfo callInfo, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 769\n");
       Assert(scriptContext);
#ifdef ENABLE_INTL_OBJECT
        if(CONFIG_FLAG(IntlBuiltIns) && scriptContext->IsIntlEnabled()){LOGMEIN("JavascriptNumber.cpp] 772\n");

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {LOGMEIN("JavascriptNumber.cpp] 776\n");
                IntlEngineInterfaceExtensionObject* intlExtensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = intlExtensionObject->GetNumberToLocaleString();
                if (func)
                {LOGMEIN("JavascriptNumber.cpp] 780\n");
                    return JavascriptString::FromVar(func->CallFunction(args));
                }
                // Initialize Number.prototype.toLocaleString
                scriptContext->GetLibrary()->InitializeIntlForNumberPrototype();
                func = intlExtensionObject->GetNumberToLocaleString();
                if (func)
                {LOGMEIN("JavascriptNumber.cpp] 787\n");
                    return JavascriptString::FromVar(func->CallFunction(args));
                }
            }
        }
#endif

        double value;
        if (!GetThisValue(args[0], &value))
        {LOGMEIN("JavascriptNumber.cpp] 796\n");
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {LOGMEIN("JavascriptNumber.cpp] 798\n");
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToLocaleString, args, &result))
                {LOGMEIN("JavascriptNumber.cpp] 801\n");
                    return JavascriptString::FromVar(result);
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toLocaleString"));
        }

        return JavascriptNumber::ToLocaleString(value, scriptContext);
    }

    Var JavascriptNumber::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptNumber.cpp] 822\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toString"));
        }

        // Optimize base 10 of TaggedInt numbers
        if (TaggedInt::Is(args[0]) && (args.Info.Count == 1 || (TaggedInt::Is(args[1]) && TaggedInt::ToInt32(args[1]) == 10)))
        {LOGMEIN("JavascriptNumber.cpp] 828\n");
            return scriptContext->GetIntegerString(args[0]);
        }

        double value;
        if (!GetThisValue(args[0], &value))
        {LOGMEIN("JavascriptNumber.cpp] 834\n");
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {LOGMEIN("JavascriptNumber.cpp] 836\n");
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToString, args, &result))
                {LOGMEIN("JavascriptNumber.cpp] 839\n");
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toString"));
        }

        int radix = 10;
        if(args.Info.Count > 1)
        {LOGMEIN("JavascriptNumber.cpp] 849\n");
            //use the first arg as the radix, ignore the rest.
            Var aRadix = args[1];

           // shortcut for tagged int's
            if(TaggedInt::Is(aRadix))
            {LOGMEIN("JavascriptNumber.cpp] 855\n");
                radix = TaggedInt::ToInt32(aRadix);
            }
            else if(JavascriptOperators::GetTypeId(aRadix) != TypeIds_Undefined)
            {LOGMEIN("JavascriptNumber.cpp] 859\n");
                radix = (int)JavascriptConversion::ToInteger(aRadix,scriptContext);
            }

        }
        if(10 == radix)
        {LOGMEIN("JavascriptNumber.cpp] 865\n");
            return ToStringRadix10(value, scriptContext);
        }

        if( radix < 2 || radix >36 )
        {LOGMEIN("JavascriptNumber.cpp] 870\n");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FunctionArgument_Invalid, _u("Number.prototype.toString"));
        }

        return ToStringRadixHelper(value, radix, scriptContext);
    }

    Var JavascriptNumber::EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Var value = args[0];

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptNumber.cpp] 888\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.valueOf"));
        }

        //avoid creation of a new Number
        if (TaggedInt::Is(value) || JavascriptNumber::Is_NoTaggedIntCheck(value))
        {LOGMEIN("JavascriptNumber.cpp] 894\n");
            return value;
        }
        else if (JavascriptNumberObject::Is(value))
        {LOGMEIN("JavascriptNumber.cpp] 898\n");
            JavascriptNumberObject* obj = JavascriptNumberObject::FromVar(value);
            return CrossSite::MarshalVar(scriptContext, obj->Unwrap());
        }
        else if (Js::JavascriptOperators::GetTypeId(value) == TypeIds_Int64Number)
        {LOGMEIN("JavascriptNumber.cpp] 903\n");
            return value;
        }
        else if (Js::JavascriptOperators::GetTypeId(value) == TypeIds_UInt64Number)
        {LOGMEIN("JavascriptNumber.cpp] 907\n");
            return value;
        }
        else
        {
            if (JavascriptOperators::GetTypeId(value) == TypeIds_HostDispatch)
            {LOGMEIN("JavascriptNumber.cpp] 913\n");
                Var result;
                if (RecyclableObject::FromVar(value)->InvokeBuiltInOperationRemotely(EntryValueOf, args, &result))
                {LOGMEIN("JavascriptNumber.cpp] 916\n");
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.valueOf"));
        }
    }

    static const int bufSize = 256;

    JavascriptString* JavascriptNumber::ToString(double value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 928\n");
        char16 szBuffer[bufSize];
        int cchWritten = swprintf_s(szBuffer, _countof(szBuffer), _u("%g"), value);

        return JavascriptString::NewCopyBuffer(szBuffer, cchWritten, scriptContext);
    }

    JavascriptString* JavascriptNumber::ToStringNanOrInfiniteOrZero(double value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 936\n");
        JavascriptString* nanF;
        if (nullptr != (nanF = ToStringNanOrInfinite(value, scriptContext)))
        {LOGMEIN("JavascriptNumber.cpp] 939\n");
            return nanF;
        }

        if (IsZero(value))
        {LOGMEIN("JavascriptNumber.cpp] 944\n");
            return scriptContext->GetLibrary()->GetCharStringCache().GetStringForCharA('0');
        }

        return nullptr;
    }

    JavascriptString* JavascriptNumber::ToStringRadix10(double value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 952\n");
        JavascriptString* string = ToStringNanOrInfiniteOrZero(value, scriptContext);
        if (string != nullptr)
        {LOGMEIN("JavascriptNumber.cpp] 955\n");
            return string;
        }

        string = scriptContext->GetLastNumberToStringRadix10(value);
        if (string == nullptr)
        {LOGMEIN("JavascriptNumber.cpp] 961\n");
            char16 szBuffer[bufSize];

            if(!Js::NumberUtilities::FNonZeroFiniteDblToStr(value, szBuffer, bufSize))
            {LOGMEIN("JavascriptNumber.cpp] 965\n");
                Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
            }
            string = JavascriptString::NewCopySz(szBuffer, scriptContext);
            scriptContext->SetLastNumberToStringRadix10(value, string);
        }
        return string;
    }

    JavascriptString* JavascriptNumber::ToStringRadixHelper(double value, int radix, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 975\n");
        Assert(radix != 10);
        Assert(radix >= 2 && radix <= 36);

        JavascriptString* string = ToStringNanOrInfiniteOrZero(value, scriptContext);
        if (string != nullptr)
        {LOGMEIN("JavascriptNumber.cpp] 981\n");
            return string;
        }

        char16 szBuffer[bufSize];

        if (!Js::NumberUtilities::FNonZeroFiniteDblToStr(value, radix, szBuffer, _countof(szBuffer)))
        {LOGMEIN("JavascriptNumber.cpp] 988\n");
            Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
        }

        return JavascriptString::NewCopySz(szBuffer, scriptContext);
    }

    BOOL JavascriptNumber::GetThisValue(Var aValue, double* pDouble)
    {LOGMEIN("JavascriptNumber.cpp] 996\n");
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);

        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {LOGMEIN("JavascriptNumber.cpp] 1000\n");
            return FALSE;
        }

        if (TaggedInt::Is(aValue))
        {LOGMEIN("JavascriptNumber.cpp] 1005\n");
            *pDouble = TaggedInt::ToDouble(aValue);
            return TRUE;
        }
        else if (Js::JavascriptOperators::GetTypeId(aValue) == TypeIds_Int64Number)
        {LOGMEIN("JavascriptNumber.cpp] 1010\n");
            *pDouble = (double)JavascriptInt64Number::FromVar(aValue)->GetValue();
            return TRUE;
        }
        else if (Js::JavascriptOperators::GetTypeId(aValue) == TypeIds_UInt64Number)
        {LOGMEIN("JavascriptNumber.cpp] 1015\n");
            *pDouble = (double)JavascriptUInt64Number::FromVar(aValue)->GetValue();
            return TRUE;
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(aValue))
        {LOGMEIN("JavascriptNumber.cpp] 1020\n");
            *pDouble = JavascriptNumber::GetValue(aValue);
            return TRUE;
        }
        else if (JavascriptNumberObject::Is(aValue))
        {LOGMEIN("JavascriptNumber.cpp] 1025\n");
            JavascriptNumberObject* obj = JavascriptNumberObject::FromVar(aValue);
            *pDouble = obj->GetValue();
            return TRUE;
        }
        else
        {
            return FALSE;
        }
    }

    JavascriptString* JavascriptNumber::ToLocaleStringNanOrInfinite(double value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 1037\n");
        if (!NumberUtilities::IsFinite(value))
        {LOGMEIN("JavascriptNumber.cpp] 1039\n");
            if (IsNan(value))
            {LOGMEIN("JavascriptNumber.cpp] 1041\n");
                return ToStringNan(scriptContext);
            }

            BSTR bstr = nullptr;
            if (IsPosInf(value))
            {LOGMEIN("JavascriptNumber.cpp] 1047\n");
                bstr = BstrGetResourceString(IDS_INFINITY);
            }
            else
            {
                AssertMsg(IsNegInf(value), "bad handling of infinite number");
                bstr = BstrGetResourceString(IDS_MINUSINFINITY);
            }

            if (bstr == nullptr)
            {LOGMEIN("JavascriptNumber.cpp] 1057\n");
                Js::JavascriptError::ThrowTypeError(scriptContext, VBSERR_InternalError);
            }
            JavascriptString* str = JavascriptString::NewCopyBuffer(bstr, SysStringLen(bstr), scriptContext);
            SysFreeString(bstr);
            return str;
        }
        return nullptr;
    }

    JavascriptString* JavascriptNumber::ToLocaleString(double value, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 1068\n");
        WCHAR   szRes[bufSize];
        WCHAR * pszRes = NULL;
        WCHAR * pszToBeFreed = NULL;
        size_t  count;

        if (!Js::NumberUtilities::IsFinite(value))
        {LOGMEIN("JavascriptNumber.cpp] 1075\n");
            //
            // +- Infinity : use the localized string
            // NaN would be returned as NaN
            //
            return ToLocaleStringNanOrInfinite(value, scriptContext);
        }

        JavascriptString *result = nullptr;

        JavascriptString *dblStr = JavascriptString::FromVar(FormatDoubleToString(value, NumberUtilities::FormatFixed, -1, scriptContext));
        const char16* szValue = dblStr->GetSz();
        const size_t szLength = dblStr->GetLength();

        pszRes = szRes;
        count = Numbers::Utility::NumberToDefaultLocaleString(szValue, szLength, pszRes, bufSize);

        if( count == 0 )
        {LOGMEIN("JavascriptNumber.cpp] 1093\n");
            return dblStr;
        }
        else
        {
            if( count > bufSize )
            {LOGMEIN("JavascriptNumber.cpp] 1099\n");
                pszRes = pszToBeFreed = HeapNewArray(char16, count);

                count = Numbers::Utility::NumberToDefaultLocaleString(szValue, szLength, pszRes, count);

                if ( count == 0 )
                {
                     AssertMsg(false, "GetNumberFormatEx failed");
                     JavascriptError::ThrowError(scriptContext, VBSERR_InternalError);
                }
            }

            if ( count != 0 )
            {LOGMEIN("JavascriptNumber.cpp] 1112\n");
                result = JavascriptString::NewCopySz(pszRes, scriptContext);
            }
        }

        if ( pszToBeFreed )
        {
            HeapDeleteArray(count, pszToBeFreed);
        }

        return result;
    }

    Var JavascriptNumber::CloneToScriptContext(Var aValue, ScriptContext* requestContext)
    {LOGMEIN("JavascriptNumber.cpp] 1126\n");
        return JavascriptNumber::New(JavascriptNumber::GetValue(aValue), requestContext);
    }

#if !FLOATVAR
    Var JavascriptNumber::BoxStackNumber(Var instance, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 1132\n");
        if (ThreadContext::IsOnStack(instance) && JavascriptNumber::Is(instance))
        {LOGMEIN("JavascriptNumber.cpp] 1134\n");
            return BoxStackInstance(JavascriptNumber::FromVar(instance), scriptContext);
        }
        else
        {
            return instance;
        }
    }

    Var JavascriptNumber::BoxStackInstance(Var instance, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptNumber.cpp] 1144\n");
        Assert(ThreadContext::IsOnStack(instance));
        double value = JavascriptNumber::FromVar(instance)->GetValue();
        return JavascriptNumber::New(value, scriptContext);
    }

    JavascriptNumber * JavascriptNumber::NewUninitialized(Recycler * recycler)
    {LOGMEIN("JavascriptNumber.cpp] 1151\n");
        return RecyclerNew(recycler, JavascriptNumber, VirtualTableInfoCtorValue);
    }
#endif
}
