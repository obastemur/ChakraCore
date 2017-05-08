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
    {TRACE_IT(60148);
        return JavascriptNumber::NewInlined(value, scriptContext);
    }

    Var JavascriptNumber::ToVarWithCheck(double value, ScriptContext* scriptContext)
    {TRACE_IT(60149);
#if FLOATVAR
        if (IsNan(value))
        {TRACE_IT(60150);
            value = JavascriptNumber::NaN;
        }
#endif
        return JavascriptNumber::NewInlined(value, scriptContext);
    }

    Var JavascriptNumber::ToVarInPlace(double value, ScriptContext* scriptContext, JavascriptNumber *result)
    {TRACE_IT(60151);
        return InPlaceNew(value, scriptContext, result);
    }

    Var JavascriptNumber::ToVarInPlace(int64 value, ScriptContext* scriptContext, JavascriptNumber *result)
    {TRACE_IT(60152);
        return InPlaceNew((double)value, scriptContext, result);
    }


    Var JavascriptNumber::ToVarMaybeInPlace(double value, ScriptContext* scriptContext, JavascriptNumber *result)
    {TRACE_IT(60153);
        if (result)
        {TRACE_IT(60154);
            return InPlaceNew(value, scriptContext, result);
        }

        return ToVarNoCheck(value, scriptContext);
    }

    Var JavascriptNumber::ToVarInPlace(int32 nValue, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {TRACE_IT(60155);
        if (!TaggedInt::IsOverflow(nValue))
        {TRACE_IT(60156);
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return InPlaceNew(static_cast<double>(nValue), scriptContext, result);
    }

    Var JavascriptNumber::ToVarInPlace(uint32 nValue, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {TRACE_IT(60157);
        if (!TaggedInt::IsOverflow(nValue))
        {TRACE_IT(60158);
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return InPlaceNew(static_cast<double>(nValue), scriptContext, result);
    }

    Var JavascriptNumber::ToVarIntCheck(double value,ScriptContext* scriptContext)
    {TRACE_IT(60159);
        //
        // Check if a well-known value:
        // - This significantly cuts down on the below floating-point to integer conversions.
        //

        if (value == 0.0)
        {TRACE_IT(60160);
            if(IsNegZero(value))
            {TRACE_IT(60161);
                return scriptContext->GetLibrary()->GetNegativeZero();
            }
            return TaggedInt::ToVarUnchecked(0);
        }
        if (value == 1.0)
        {TRACE_IT(60162);
            return TaggedInt::ToVarUnchecked(1);
        }

        //
        // Check if number can be reduced back into a TaggedInt:
        // - This avoids extra GC.
        //

        int nValue      = (int) value;
        double dblCheck = (double) nValue;
        if ((dblCheck == value) && (!TaggedInt::IsOverflow(nValue)))
        {TRACE_IT(60163);
            return TaggedInt::ToVarUnchecked(nValue);
        }

        return JavascriptNumber::NewInlined(value,scriptContext);
    }

    bool JavascriptNumber::TryGetInt32OrUInt32Value(const double value, int32 *const int32Value, bool *const isInt32)
    {TRACE_IT(60164);
        Assert(int32Value);
        Assert(isInt32);

        if(value <= 0)
        {TRACE_IT(60165);
            return *isInt32 = TryGetInt32Value(value, int32Value);
        }

        const uint32 i = static_cast<uint32>(value);
        if(static_cast<double>(i) != value)
        {TRACE_IT(60166);
            return false;
        }

        *int32Value = i;
        *isInt32 = static_cast<int32>(i) >= 0;
        return true;
    }

    bool JavascriptNumber::IsInt32(const double value)
    {TRACE_IT(60167);
        int32 i;
        return TryGetInt32Value(value, &i);
    }

    bool JavascriptNumber::IsInt32OrUInt32(const double value)
    {TRACE_IT(60168);
        int32 i;
        bool isInt32;
        return TryGetInt32OrUInt32Value(value, &i, &isInt32);
    }

    bool JavascriptNumber::IsInt32_NoChecks(const Var number)
    {TRACE_IT(60169);
        Assert(number);
        Assert(Is(number));

        return IsInt32(GetValue(number));
    }

    bool JavascriptNumber::IsInt32OrUInt32_NoChecks(const Var number)
    {TRACE_IT(60170);
        Assert(number);
        Assert(Is(number));

        return IsInt32OrUInt32(GetValue(number));
    }

    int32 JavascriptNumber::GetNonzeroInt32Value_NoTaggedIntCheck(const Var object)
    {TRACE_IT(60171);
        Assert(object);
        Assert(!TaggedInt::Is(object));

        int32 i;
        return Is_NoTaggedIntCheck(object) && TryGetInt32Value(GetValue(object), &i) ? i : 0;
    }

    int32 JavascriptNumber::DirectPowIntInt(bool* isOverflow, int32 x, int32 y)
    {TRACE_IT(60172);
        if (y < 0)
        {TRACE_IT(60173);
            *isOverflow = true;
            return 0;
        }

        uint32 uexp = static_cast<uint32>(y);
        int32 result = 1;

        while (true)
        {TRACE_IT(60174);
            if ((uexp & 1) != 0)
            {TRACE_IT(60175);
                if (Int32Math::Mul(result, x, &result))
                {TRACE_IT(60176);
                    *isOverflow = true;
                    break;
                }
            }
            if ((uexp >>= 1) == 0)
            {TRACE_IT(60177);
                *isOverflow = false;
                break;
            }
            if (Int32Math::Mul(x, x, &x))
            {TRACE_IT(60178);
                *isOverflow = true;
                break;
            }
        }

        return *isOverflow ? 0 : result;
    }

    double JavascriptNumber::DirectPowDoubleInt(double x, int32 y)
    {TRACE_IT(60179);
        // For exponent in [-8, 8], aggregate the product according to binary representation
        // of exponent. This acceleration may lead to significant deviation for larger exponent
        if (y >= -8 && y <= 8)
        {TRACE_IT(60180);
            uint32 uexp = static_cast<uint32>(y >= 0 ? y : -y);
            for (double result = 1.0; ; x *= x)
            {TRACE_IT(60181);
                if ((uexp & 1) != 0)
                {TRACE_IT(60182);
                    result *= x;
                }
                if ((uexp >>= 1) == 0)
                {TRACE_IT(60183);
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
    {TRACE_IT(60184);
        return ::pow(x, y);
    }
#else

#pragma warning(push)
// C4740: flow in or out of inline asm code suppresses global optimization
// It is fine to disable glot opt on this function which is mostly written in assembly
#pragma warning(disable:4740)
    __declspec(naked)
    double JavascriptNumber::DirectPow(double x, double y)
    {TRACE_IT(60185);
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
        {TRACE_IT(60186);
            result = DirectPowDoubleInt(savedX, intY);
            __asm {TRACE_IT(60187);
                movsd xmm0, result
            }
        }
        else
        {TRACE_IT(60188);
            __asm {TRACE_IT(60189);
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
    {TRACE_IT(60190);
        if(y == 1.0)
        {TRACE_IT(60191);
            return x;
        }

        // For AMD64/ARM calling convention already uses SSE2/VFP registers so we don't have to use assembler.
        // We can't just use "if (0 == y)" because NaN compares
        // equal to 0 according to our compilers.
        int32 intY;
        if (0 == NumberUtilities::LuLoDbl(y) && 0 == (NumberUtilities::LuHiDbl(y) & 0x7FFFFFFF))
        {TRACE_IT(60192);
            // pow(x, 0) = 1 even if x is NaN.
            return 1;
        }
        else if (1.0 == fabs(x) && !NumberUtilities::IsFinite(y))
        {TRACE_IT(60193);
            // pow([+/-] 1, Infinity) = NaN according to javascript, but not for CRT pow.
            return JavascriptNumber::NaN;
        }
        else if (TryGetInt32Value(y, &intY))
        {TRACE_IT(60194);
            // check fast path
            return DirectPowDoubleInt(x, intY);
        }

        return ::pow(x, y);
    }

#else

    double JavascriptNumber::DirectPow(double x, double y)
    {TRACE_IT(60195);
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
        {TRACE_IT(60196);
            if (TaggedInt::Is(args[1]) || JavascriptNumber::Is(args[1]))
            {TRACE_IT(60197);
                result = args[1];
            }
            else if (JavascriptNumberObject::Is(args[1]))
            {TRACE_IT(60198);
                result = JavascriptNumber::ToVarNoCheck(JavascriptNumberObject::FromVar(args[1])->GetValue(), scriptContext);
            }
            else
            {TRACE_IT(60199);
                result = JavascriptNumber::ToVarNoCheck(JavascriptConversion::ToNumber(args[1], scriptContext), scriptContext);
            }
        }
        else
        {TRACE_IT(60200);
            result = TaggedInt::ToVarUnchecked(0);
        }

        if (callInfo.Flags & CallFlags_New)
        {TRACE_IT(60201);
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
        {TRACE_IT(60202);
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
        {TRACE_IT(60203);
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
        {TRACE_IT(60204);
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
        {TRACE_IT(60205);
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
        {TRACE_IT(60206);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toExponential"));
        }

        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToExp is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {TRACE_IT(60207);
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {TRACE_IT(60208);
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToExponential, args, &result))
                {TRACE_IT(60209);
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
        {TRACE_IT(60210);
            //use the first arg as the fraction digits, ignore the rest.
            Var aFractionDigits = args[1];
            bool noRangeCheck = false;

            // shortcut for tagged int's
            if(TaggedInt::Is(aFractionDigits))
            {TRACE_IT(60211);
                fractionDigits = TaggedInt::ToInt32(aFractionDigits);
            }
            else if(JavascriptOperators::GetTypeId(aFractionDigits) == TypeIds_Undefined)
            {TRACE_IT(60212);
                // fraction undefined -> digits = -1, output as many fractional digits as we can
                noRangeCheck = true;
            }
            else
            {TRACE_IT(60213);
                fractionDigits = (int)JavascriptConversion::ToInteger(aFractionDigits, scriptContext);
            }
            if(!noRangeCheck && (fractionDigits < 0 || fractionDigits >20))
            {TRACE_IT(60214);
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
        {TRACE_IT(60215);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toFixed"));
        }
        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToFixed is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {TRACE_IT(60216);
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {TRACE_IT(60217);
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToFixed, args, &result))
                {TRACE_IT(60218);
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toFixed"));
        }
        int fractionDigits = 0;
        bool isFractionDigitsInfinite = false;
        if(args.Info.Count > 1)
        {TRACE_IT(60219);
            //use the first arg as the fraction digits, ignore the rest.
            Var aFractionDigits = args[1];

            // shortcut for tagged int's
            if(TaggedInt::Is(aFractionDigits))
            {TRACE_IT(60220);
                fractionDigits = TaggedInt::ToInt32(aFractionDigits);
            }
            else if(JavascriptOperators::GetTypeId(aFractionDigits) == TypeIds_Undefined)
            {TRACE_IT(60221);
                // fraction digits = 0
            }
            else
            {TRACE_IT(60222);
                double fractionDigitsRaw = JavascriptConversion::ToInteger(aFractionDigits, scriptContext);
                isFractionDigitsInfinite =
                    fractionDigitsRaw == JavascriptNumber::NEGATIVE_INFINITY ||
                    fractionDigitsRaw == JavascriptNumber::POSITIVE_INFINITY;
                fractionDigits = (int)fractionDigitsRaw;
            }
        }

        if (fractionDigits < 0 || fractionDigits > 20)
        {TRACE_IT(60223);
            JavascriptError::ThrowRangeError(scriptContext, JSERR_FractionOutOfRange);
        }

        if(IsNan(value))
        {TRACE_IT(60224);
            return ToStringNan(scriptContext);
        }
        if(value >= 1e21)
        {TRACE_IT(60225);
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
        {TRACE_IT(60226);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toPrecision"));
        }
        AssertMsg(args.Info.Count > 0, "negative arg count");

        // spec implies ToPrec is not generic. 'this' must be a number
        double value;
        if (!GetThisValue(args[0], &value))
        {TRACE_IT(60227);
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {TRACE_IT(60228);
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToPrecision, args, &result))
                {TRACE_IT(60229);
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toPrecision"));
        }
        if(args.Info.Count < 2 || JavascriptOperators::GetTypeId(args[1]) == TypeIds_Undefined)
        {TRACE_IT(60230);
            return JavascriptConversion::ToString(args[0], scriptContext);
        }

        int precision;
        Var aPrecision = args[1];
        if(TaggedInt::Is(aPrecision))
        {TRACE_IT(60231);
            precision = TaggedInt::ToInt32(aPrecision);
        }
        else
        {TRACE_IT(60232);
            precision = (int) JavascriptConversion::ToInt32(aPrecision, scriptContext);
        }

        JavascriptString * nanF;
        if (nullptr != (nanF = ToStringNanOrInfinite(value, scriptContext)))
        {TRACE_IT(60233);
            return nanF;
        }

        if(precision < 1 || precision > 21)
        {TRACE_IT(60234);
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
        {TRACE_IT(60235);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toLocaleString"));
        }
        return JavascriptNumber::ToLocaleStringIntl(args, callInfo, scriptContext);
    }

    JavascriptString* JavascriptNumber::ToLocaleStringIntl(Var* values, CallInfo callInfo, ScriptContext* scriptContext)
    {TRACE_IT(60236);
        Assert(values);
        ArgumentReader args(&callInfo, values);
        return JavascriptNumber::ToLocaleStringIntl(args, callInfo, scriptContext);
    }

    JavascriptString* JavascriptNumber::ToLocaleStringIntl(ArgumentReader& args, CallInfo callInfo, ScriptContext* scriptContext)
    {TRACE_IT(60237);
       Assert(scriptContext);
#ifdef ENABLE_INTL_OBJECT
        if(CONFIG_FLAG(IntlBuiltIns) && scriptContext->IsIntlEnabled()){TRACE_IT(60238);

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {TRACE_IT(60239);
                IntlEngineInterfaceExtensionObject* intlExtensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = intlExtensionObject->GetNumberToLocaleString();
                if (func)
                {TRACE_IT(60240);
                    return JavascriptString::FromVar(func->CallFunction(args));
                }
                // Initialize Number.prototype.toLocaleString
                scriptContext->GetLibrary()->InitializeIntlForNumberPrototype();
                func = intlExtensionObject->GetNumberToLocaleString();
                if (func)
                {TRACE_IT(60241);
                    return JavascriptString::FromVar(func->CallFunction(args));
                }
            }
        }
#endif

        double value;
        if (!GetThisValue(args[0], &value))
        {TRACE_IT(60242);
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {TRACE_IT(60243);
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToLocaleString, args, &result))
                {TRACE_IT(60244);
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
        {TRACE_IT(60245);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toString"));
        }

        // Optimize base 10 of TaggedInt numbers
        if (TaggedInt::Is(args[0]) && (args.Info.Count == 1 || (TaggedInt::Is(args[1]) && TaggedInt::ToInt32(args[1]) == 10)))
        {TRACE_IT(60246);
            return scriptContext->GetIntegerString(args[0]);
        }

        double value;
        if (!GetThisValue(args[0], &value))
        {TRACE_IT(60247);
            if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
            {TRACE_IT(60248);
                Var result;
                if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(EntryToString, args, &result))
                {TRACE_IT(60249);
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.toString"));
        }

        int radix = 10;
        if(args.Info.Count > 1)
        {TRACE_IT(60250);
            //use the first arg as the radix, ignore the rest.
            Var aRadix = args[1];

           // shortcut for tagged int's
            if(TaggedInt::Is(aRadix))
            {TRACE_IT(60251);
                radix = TaggedInt::ToInt32(aRadix);
            }
            else if(JavascriptOperators::GetTypeId(aRadix) != TypeIds_Undefined)
            {TRACE_IT(60252);
                radix = (int)JavascriptConversion::ToInteger(aRadix,scriptContext);
            }

        }
        if(10 == radix)
        {TRACE_IT(60253);
            return ToStringRadix10(value, scriptContext);
        }

        if( radix < 2 || radix >36 )
        {TRACE_IT(60254);
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
        {TRACE_IT(60255);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.valueOf"));
        }

        //avoid creation of a new Number
        if (TaggedInt::Is(value) || JavascriptNumber::Is_NoTaggedIntCheck(value))
        {TRACE_IT(60256);
            return value;
        }
        else if (JavascriptNumberObject::Is(value))
        {TRACE_IT(60257);
            JavascriptNumberObject* obj = JavascriptNumberObject::FromVar(value);
            return CrossSite::MarshalVar(scriptContext, obj->Unwrap());
        }
        else if (Js::JavascriptOperators::GetTypeId(value) == TypeIds_Int64Number)
        {TRACE_IT(60258);
            return value;
        }
        else if (Js::JavascriptOperators::GetTypeId(value) == TypeIds_UInt64Number)
        {TRACE_IT(60259);
            return value;
        }
        else
        {TRACE_IT(60260);
            if (JavascriptOperators::GetTypeId(value) == TypeIds_HostDispatch)
            {TRACE_IT(60261);
                Var result;
                if (RecyclableObject::FromVar(value)->InvokeBuiltInOperationRemotely(EntryValueOf, args, &result))
                {TRACE_IT(60262);
                    return result;
                }
            }

            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedNumber, _u("Number.prototype.valueOf"));
        }
    }

    static const int bufSize = 256;

    JavascriptString* JavascriptNumber::ToString(double value, ScriptContext* scriptContext)
    {TRACE_IT(60263);
        char16 szBuffer[bufSize];
        int cchWritten = swprintf_s(szBuffer, _countof(szBuffer), _u("%g"), value);

        return JavascriptString::NewCopyBuffer(szBuffer, cchWritten, scriptContext);
    }

    JavascriptString* JavascriptNumber::ToStringNanOrInfiniteOrZero(double value, ScriptContext* scriptContext)
    {TRACE_IT(60264);
        JavascriptString* nanF;
        if (nullptr != (nanF = ToStringNanOrInfinite(value, scriptContext)))
        {TRACE_IT(60265);
            return nanF;
        }

        if (IsZero(value))
        {TRACE_IT(60266);
            return scriptContext->GetLibrary()->GetCharStringCache().GetStringForCharA('0');
        }

        return nullptr;
    }

    JavascriptString* JavascriptNumber::ToStringRadix10(double value, ScriptContext* scriptContext)
    {TRACE_IT(60267);
        JavascriptString* string = ToStringNanOrInfiniteOrZero(value, scriptContext);
        if (string != nullptr)
        {TRACE_IT(60268);
            return string;
        }

        string = scriptContext->GetLastNumberToStringRadix10(value);
        if (string == nullptr)
        {TRACE_IT(60269);
            char16 szBuffer[bufSize];

            if(!Js::NumberUtilities::FNonZeroFiniteDblToStr(value, szBuffer, bufSize))
            {TRACE_IT(60270);
                Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
            }
            string = JavascriptString::NewCopySz(szBuffer, scriptContext);
            scriptContext->SetLastNumberToStringRadix10(value, string);
        }
        return string;
    }

    JavascriptString* JavascriptNumber::ToStringRadixHelper(double value, int radix, ScriptContext* scriptContext)
    {TRACE_IT(60271);
        Assert(radix != 10);
        Assert(radix >= 2 && radix <= 36);

        JavascriptString* string = ToStringNanOrInfiniteOrZero(value, scriptContext);
        if (string != nullptr)
        {TRACE_IT(60272);
            return string;
        }

        char16 szBuffer[bufSize];

        if (!Js::NumberUtilities::FNonZeroFiniteDblToStr(value, radix, szBuffer, _countof(szBuffer)))
        {TRACE_IT(60273);
            Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
        }

        return JavascriptString::NewCopySz(szBuffer, scriptContext);
    }

    BOOL JavascriptNumber::GetThisValue(Var aValue, double* pDouble)
    {TRACE_IT(60274);
        TypeId typeId = JavascriptOperators::GetTypeId(aValue);

        if (typeId == TypeIds_Null || typeId == TypeIds_Undefined)
        {TRACE_IT(60275);
            return FALSE;
        }

        if (TaggedInt::Is(aValue))
        {TRACE_IT(60276);
            *pDouble = TaggedInt::ToDouble(aValue);
            return TRUE;
        }
        else if (Js::JavascriptOperators::GetTypeId(aValue) == TypeIds_Int64Number)
        {TRACE_IT(60277);
            *pDouble = (double)JavascriptInt64Number::FromVar(aValue)->GetValue();
            return TRUE;
        }
        else if (Js::JavascriptOperators::GetTypeId(aValue) == TypeIds_UInt64Number)
        {TRACE_IT(60278);
            *pDouble = (double)JavascriptUInt64Number::FromVar(aValue)->GetValue();
            return TRUE;
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(aValue))
        {TRACE_IT(60279);
            *pDouble = JavascriptNumber::GetValue(aValue);
            return TRUE;
        }
        else if (JavascriptNumberObject::Is(aValue))
        {TRACE_IT(60280);
            JavascriptNumberObject* obj = JavascriptNumberObject::FromVar(aValue);
            *pDouble = obj->GetValue();
            return TRUE;
        }
        else
        {TRACE_IT(60281);
            return FALSE;
        }
    }

    JavascriptString* JavascriptNumber::ToLocaleStringNanOrInfinite(double value, ScriptContext* scriptContext)
    {TRACE_IT(60282);
        if (!NumberUtilities::IsFinite(value))
        {TRACE_IT(60283);
            if (IsNan(value))
            {TRACE_IT(60284);
                return ToStringNan(scriptContext);
            }

            BSTR bstr = nullptr;
            if (IsPosInf(value))
            {TRACE_IT(60285);
                bstr = BstrGetResourceString(IDS_INFINITY);
            }
            else
            {
                AssertMsg(IsNegInf(value), "bad handling of infinite number");
                bstr = BstrGetResourceString(IDS_MINUSINFINITY);
            }

            if (bstr == nullptr)
            {TRACE_IT(60286);
                Js::JavascriptError::ThrowTypeError(scriptContext, VBSERR_InternalError);
            }
            JavascriptString* str = JavascriptString::NewCopyBuffer(bstr, SysStringLen(bstr), scriptContext);
            SysFreeString(bstr);
            return str;
        }
        return nullptr;
    }

    JavascriptString* JavascriptNumber::ToLocaleString(double value, ScriptContext* scriptContext)
    {TRACE_IT(60287);
        WCHAR   szRes[bufSize];
        WCHAR * pszRes = NULL;
        WCHAR * pszToBeFreed = NULL;
        size_t  count;

        if (!Js::NumberUtilities::IsFinite(value))
        {TRACE_IT(60288);
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
        {TRACE_IT(60289);
            return dblStr;
        }
        else
        {TRACE_IT(60290);
            if( count > bufSize )
            {TRACE_IT(60291);
                pszRes = pszToBeFreed = HeapNewArray(char16, count);

                count = Numbers::Utility::NumberToDefaultLocaleString(szValue, szLength, pszRes, count);

                if ( count == 0 )
                {
                     AssertMsg(false, "GetNumberFormatEx failed");
                     JavascriptError::ThrowError(scriptContext, VBSERR_InternalError);
                }
            }

            if ( count != 0 )
            {TRACE_IT(60292);
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
    {TRACE_IT(60293);
        return JavascriptNumber::New(JavascriptNumber::GetValue(aValue), requestContext);
    }

#if !FLOATVAR
    Var JavascriptNumber::BoxStackNumber(Var instance, ScriptContext* scriptContext)
    {TRACE_IT(60294);
        if (ThreadContext::IsOnStack(instance) && JavascriptNumber::Is(instance))
        {TRACE_IT(60295);
            return BoxStackInstance(JavascriptNumber::FromVar(instance), scriptContext);
        }
        else
        {TRACE_IT(60296);
            return instance;
        }
    }

    Var JavascriptNumber::BoxStackInstance(Var instance, ScriptContext* scriptContext)
    {TRACE_IT(60297);
        Assert(ThreadContext::IsOnStack(instance));
        double value = JavascriptNumber::FromVar(instance)->GetValue();
        return JavascriptNumber::New(value, scriptContext);
    }

    JavascriptNumber * JavascriptNumber::NewUninitialized(Recycler * recycler)
    {TRACE_IT(60298);
        return RecyclerNew(recycler, JavascriptNumber, VirtualTableInfoCtorValue);
    }
#endif
}
