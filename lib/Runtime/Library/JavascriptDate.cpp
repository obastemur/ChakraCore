//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Library/EngineInterfaceObject.h"
#include "Library/IntlEngineInterfaceExtensionObject.h"
#ifdef ENABLE_BASIC_TELEMETRY
#include "ScriptContextTelemetry.h"
#endif

namespace Js
{
    JavascriptDate::JavascriptDate(double value, DynamicType * type)
        : DynamicObject(type), m_date(value, type->GetScriptContext())
    {LOGMEIN("JavascriptDate.cpp] 15\n");
        Assert(IsDateTypeId(type->GetTypeId()));
    }

    JavascriptDate::JavascriptDate(DynamicType * type)
        : DynamicObject(type), m_date(0, type->GetScriptContext())
    {LOGMEIN("JavascriptDate.cpp] 21\n");
        Assert(type->GetTypeId() == TypeIds_Date);
    }

    bool JavascriptDate::Is(Var aValue)
    {LOGMEIN("JavascriptDate.cpp] 26\n");
        // All WinRT Date's are also implicitly Javascript dates
        return IsDateTypeId(JavascriptOperators::GetTypeId(aValue));
    }

    JavascriptDate* JavascriptDate::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'Date'");

        return static_cast<JavascriptDate *>(RecyclableObject::FromVar(aValue));
    }

    Var JavascriptDate::GetDateData(JavascriptDate* date, DateImplementation::DateData dd, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptDate.cpp] 39\n");
        return JavascriptNumber::ToVarIntCheck(date->m_date.GetDateData(dd, false, scriptContext), scriptContext);
    }

    Var JavascriptDate::GetUTCDateData(JavascriptDate* date, DateImplementation::DateData dd, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptDate.cpp] 44\n");
        return JavascriptNumber::ToVarIntCheck(date->m_date.GetDateData(dd, true, scriptContext), scriptContext);
    }

    Var JavascriptDate::SetDateData(JavascriptDate* date, Arguments args, DateImplementation::DateData dd, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptDate.cpp] 49\n");
        return JavascriptNumber::ToVarNoCheck(date->m_date.SetDateData(args, dd, false, scriptContext), scriptContext);
    }

    Var JavascriptDate::SetUTCDateData(JavascriptDate* date, Arguments args, DateImplementation::DateData dd, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptDate.cpp] 54\n");
        return JavascriptNumber::ToVarNoCheck(date->m_date.SetDateData(args, dd, true, scriptContext), scriptContext);
    }

    Var JavascriptDate::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        //
        // Determine if called as a constructor or a function.
        //

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        // SkipDefaultNewObject function flag should have prevented the default object from
        // being created, except when call true a host dispatch.
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr
            || JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch);

        if (!(callInfo.Flags & CallFlags_New))
        {LOGMEIN("JavascriptDate.cpp] 79\n");
            //
            // Called as a function.
            //

            //
            // Be sure the latest time zone info is loaded
            //

            // ES5 15.9.2.1: Date() should returns a string exactly the same as (new Date().toString()).
            JavascriptDate* pDate = NewInstanceAsConstructor(args, scriptContext, /* forceCurrentDate */ true);
            JavascriptString* res = JavascriptDate::ToString(pDate);

#if ENABLE_TTD
            if(scriptContext->ShouldPerformReplayAction())
            {LOGMEIN("JavascriptDate.cpp] 94\n");
                scriptContext->GetThreadContext()->TTDLog->ReplayDateStringEvent(scriptContext, &res);
            }
            else if(scriptContext->ShouldPerformRecordAction())
            {LOGMEIN("JavascriptDate.cpp] 98\n");
                scriptContext->GetThreadContext()->TTDLog->RecordDateStringEvent(res);
            }
            else
            {
                ;
            }
#endif
            return res;
        }
        else
        {
            //
            // Called as a constructor.
            //
            RecyclableObject* pNew = NewInstanceAsConstructor(args, scriptContext);
            return isCtorSuperCall ?
                JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pNew, nullptr, scriptContext) :
                pNew;
        }
    }

    JavascriptDate* JavascriptDate::NewInstanceAsConstructor(Js::Arguments args, ScriptContext* scriptContext, bool forceCurrentDate)
    {LOGMEIN("JavascriptDate.cpp] 121\n");
        Assert(scriptContext);

        double timeValue = 0;
        JavascriptDate* pDate;

        pDate = scriptContext->GetLibrary()->CreateDate();


        //
        // [15.9.3.3]
        // No arguments passed. Return the current time
        //
        if (forceCurrentDate || args.Info.Count == 1)
        {LOGMEIN("JavascriptDate.cpp] 135\n");
            double resTime = DateImplementation::NowFromHiResTimer(scriptContext);

#if ENABLE_TTD
            if(scriptContext->ShouldPerformReplayAction())
            {LOGMEIN("JavascriptDate.cpp] 140\n");
                scriptContext->GetThreadContext()->TTDLog->ReplayDateTimeEvent(&resTime);
            }
            else if(scriptContext->ShouldPerformRecordAction())
            {LOGMEIN("JavascriptDate.cpp] 144\n");
                scriptContext->GetThreadContext()->TTDLog->RecordDateTimeEvent(resTime);
            }
            else
            {
                ;
            }
#endif

            pDate->m_date.SetTvUtc(resTime);
            return pDate;
        }

        //
        // [15.9.3.2]
        // One argument given
        // If string parse it and use that timeValue
        // Else convert to Number and use that as timeValue
        //
        if (args.Info.Count == 2)
        {LOGMEIN("JavascriptDate.cpp] 164\n");
            if (JavascriptDate::Is(args[1]))
            {LOGMEIN("JavascriptDate.cpp] 166\n");
                JavascriptDate* dateObject = JavascriptDate::FromVar(args[1]);
                timeValue = ((dateObject)->m_date).m_tvUtc;
            }
            else
            {
                Var value = JavascriptConversion::ToPrimitive(args[1], Js::JavascriptHint::None, scriptContext);
                if (JavascriptString::Is(value))
                {LOGMEIN("JavascriptDate.cpp] 174\n");
                    timeValue = ParseHelper(scriptContext, JavascriptString::FromVar(value));
                }
                else
                {
                    timeValue = JavascriptConversion::ToNumber(value, scriptContext);
                }
            }

            pDate->m_date.SetTvUtc(timeValue);
            return pDate;
        }

        //
        // [15.9.3.1]
        // Date called with two to seven arguments
        //

        const int parameterCount = 7;
        double values[parameterCount];

        for (uint i=1; i < args.Info.Count && i < parameterCount+1; i++)
        {LOGMEIN("JavascriptDate.cpp] 196\n");
            double curr = JavascriptConversion::ToNumber(args[i], scriptContext);
            values[i-1] = curr;
            if (JavascriptNumber::IsNan(curr) || !NumberUtilities::IsFinite(curr))
            {LOGMEIN("JavascriptDate.cpp] 200\n");
                pDate->m_date.SetTvUtc(curr);
                return pDate;
            }
        }

        for (uint i=0; i < parameterCount; i++)
        {LOGMEIN("JavascriptDate.cpp] 207\n");
            if ( i >= args.Info.Count-1 )
            {LOGMEIN("JavascriptDate.cpp] 209\n");
                values[i] = ( i == 2 );
                continue;
            }
            // MakeTime (ES5 15.9.1.11) && MakeDay (ES5 15.9.1.12) always
            // call ToInteger (which is same as JavascriptConversion::ToInteger) on arguments.
            // All are finite (not Inf or Nan) as we check them explicitly in the previous loop.
            // +-0 & +0 are same in this context.
#pragma prefast(suppress:6001, "value index i < args.Info.Count - 1 are initialized")
            values[i] = JavascriptConversion::ToInteger(values[i]);
        }

        // adjust the year
        if (values[0] < 100 && values[0] >= 0)
            values[0] += 1900;

        // Get the local time value.
        timeValue = DateImplementation::TvFromDate(values[0], values[1], values[2] - 1,
            values[3] * 3600000 + values[4] * 60000 + values[5] * 1000 + values[6]);

        // Set the time.
        pDate->m_date.SetTvLcl(timeValue);

        return pDate;
    }

    // Date.prototype[@@toPrimitive] as described in ES6 spec (Draft May 22, 2014) 20.3.4.45
    Var JavascriptDate::EntrySymbolToPrimitive(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        // One argument given will be hint
        //The allowed values for hint are "default", "number", and "string"
        if (args.Info.Count == 2)
        {LOGMEIN("JavascriptDate.cpp] 248\n");
            if (!JavascriptOperators::IsObjectType(JavascriptOperators::GetTypeId(args[0])))
            {LOGMEIN("JavascriptDate.cpp] 250\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Date[Symbol.toPrimitive]"));
            }

            if (JavascriptString::Is(args[1]))
            {LOGMEIN("JavascriptDate.cpp] 255\n");
                JavascriptString* StringObject = JavascriptString::FromVar(args[1]);

                if (wcscmp(StringObject->UnsafeGetBuffer(), _u("default")) == 0 || wcscmp(StringObject->UnsafeGetBuffer(), _u("string")) == 0)
                {LOGMEIN("JavascriptDate.cpp] 259\n");
                    // Date objects, are unique among built-in ECMAScript object in that they treat "default" as being equivalent to "string"
                    // If hint is the string value "string" or the string value "default", then
                    // Let tryFirst be "string".
                    return JavascriptConversion::OrdinaryToPrimitive(args[0], JavascriptHint::HintString/*tryFirst*/, scriptContext);
                }
                // Else if hint is the string value "number", then
                // Let tryFirst be "number".
                else if(wcscmp(StringObject->UnsafeGetBuffer(), _u("number")) == 0)
                {LOGMEIN("JavascriptDate.cpp] 268\n");
                    return JavascriptConversion::OrdinaryToPrimitive(args[0], JavascriptHint::HintNumber/*tryFirst*/, scriptContext);
                }
                //anything else should throw a type error
            }
        }

        JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidHint, _u("Date[Symbol.toPrimitive]"));
    }

    Var JavascriptDate::EntryGetDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 288\n");
            Var result;
            if (TryInvokeRemotely(EntryGetDate, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 291\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getDate"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 299\n");
            return date->m_date.GetDate();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetDay(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 315\n");
            Var result;
            if (TryInvokeRemotely(EntryGetDay, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 318\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getDay"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 326\n");
            return date->m_date.GetDay();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetFullYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 342\n");
            Var result;
            if (TryInvokeRemotely(EntryGetFullYear, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 345\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getFullYear"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 353\n");
            return date->m_date.GetFullYear();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 369\n");
            Var result;
            if (TryInvokeRemotely(EntryGetYear, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 372\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getYear"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 380\n");
            return date->m_date.GetYear();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetHours(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 396\n");
            Var result;
            if (TryInvokeRemotely(EntryGetHours, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 399\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getHours"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 407\n");
            return date->m_date.GetHours();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetMilliseconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 423\n");
            Var result;
            if (TryInvokeRemotely(EntryGetMilliseconds, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 426\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getMilliseconds"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 434\n");
            return date->m_date.GetDateMilliSeconds();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetMinutes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 450\n");
            Var result;
            if (TryInvokeRemotely(EntryGetMinutes, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 453\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getMinutes"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 461\n");
            return date->m_date.GetMinutes();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetMonth(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 477\n");
            Var result;
            if (TryInvokeRemotely(EntryGetMonth, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 480\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getMonth"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 488\n");
            return date->m_date.GetMonth();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetSeconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 504\n");
            Var result;
            if (TryInvokeRemotely(EntryGetSeconds, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 507\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getSeconds"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        if (!date->m_date.IsNaN())
        {LOGMEIN("JavascriptDate.cpp] 515\n");
            return date->m_date.GetSeconds();
        }
        return scriptContext->GetLibrary()->GetNaN();
    }

    Var JavascriptDate::EntryGetTime(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 531\n");
            Var result;
            if (TryInvokeRemotely(EntryGetTime, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 534\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getTime"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptNumber::ToVarNoCheck(date->GetTime(), scriptContext);
    }

    Var JavascriptDate::EntryGetTimezoneOffset(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 554\n");
            Var result;
            if (TryInvokeRemotely(EntryGetTimezoneOffset, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 557\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getTimezoneOffset"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetDateData(date, DateImplementation::DateData::TimezoneOffset, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 577\n");
            Var result;
            if (TryInvokeRemotely(EntryGetUTCDate, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 580\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getUTCDate"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Date, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCDay(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 600\n");
            Var result;
            if (TryInvokeRemotely(EntryGetUTCDay, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 603\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getUTCDay"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Day, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCFullYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 623\n");
            Var result;
            if (TryInvokeRemotely(EntryGetUTCFullYear, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 626\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getUTCFullYear"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::FullYear, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCHours(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 646\n");
            Var result;
            if (TryInvokeRemotely(EntryGetUTCHours, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 649\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getUTCHours"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Hours, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCMilliseconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 669\n");
            Var result;
            if (TryInvokeRemotely(EntryGetUTCMilliseconds, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 672\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getUTCMilliseconds"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Milliseconds, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCMinutes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 692\n");
            Var result;
            if (TryInvokeRemotely(EntryGetUTCMinutes, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 695\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getUTCMinutes"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Minutes, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCMonth(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 715\n");
            Var result;
            if (TryInvokeRemotely(EntryGetUTCMonth, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 718\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getUTCMonth"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Month, scriptContext);
    }

    Var JavascriptDate::EntryGetUTCSeconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 738\n");
            Var result;
            if (TryInvokeRemotely(EntryGetUTCSeconds, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 741\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getUTCSeconds"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::GetUTCDateData(date, DateImplementation::DateData::Seconds, scriptContext);
    }

    Var JavascriptDate::EntryGetVarDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 761\n");
            Var result;
            if (TryInvokeRemotely(EntryGetVarDate, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 764\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.getVarDate"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return scriptContext->GetLibrary()->CreateVariantDate(
            DateImplementation::VarDateFromJsUtcTime(date->GetTime(), scriptContext)
            );
    }

    Var JavascriptDate::EntryParse(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        double dblRetVal = JavascriptNumber::NaN;

        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptDate.cpp] 788\n");
            // We convert to primitive value based on hint == String, which JavascriptConversion::ToString does.
            JavascriptString *pParseString = JavascriptConversion::ToString(args[1], scriptContext);
            dblRetVal = ParseHelper(scriptContext, pParseString);
        }

        return JavascriptNumber::ToVarNoCheck(dblRetVal,scriptContext);
    }

    Var JavascriptDate::EntryNow(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        double dblRetVal = DateImplementation::NowInMilliSeconds(scriptContext);

#if ENABLE_TTD
        if(scriptContext->ShouldPerformReplayAction())
        {LOGMEIN("JavascriptDate.cpp] 809\n");
            scriptContext->GetThreadContext()->TTDLog->ReplayDateTimeEvent(&dblRetVal);
        }
        else if(scriptContext->ShouldPerformRecordAction())
        {LOGMEIN("JavascriptDate.cpp] 813\n");
            scriptContext->GetThreadContext()->TTDLog->RecordDateTimeEvent(dblRetVal);
        }
        else
        {
            ;
        }
#endif

        return JavascriptNumber::ToVarNoCheck(dblRetVal,scriptContext);
    }

    Var JavascriptDate::EntryUTC(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        Assert(!(callInfo.Flags & CallFlags_New));

        double dblRetVal = DateImplementation::DateFncUTC(scriptContext, args);
        return JavascriptNumber::ToVarNoCheck(dblRetVal, scriptContext);
    }

    double JavascriptDate::ParseHelper(ScriptContext *scriptContext, JavascriptString *str)
    {LOGMEIN("JavascriptDate.cpp] 838\n");
        return DateImplementation::UtcTimeFromStr(scriptContext, str);
    }

    Var JavascriptDate::EntrySetDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 852\n");
            Var result;
            if (TryInvokeRemotely(EntrySetDate, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 855\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setDate"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Date, scriptContext);
    }

    Var JavascriptDate::EntrySetFullYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 875\n");
            Var result;
            if (TryInvokeRemotely(EntrySetFullYear, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 878\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setFullYear"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::FullYear, scriptContext);
    }

    Var JavascriptDate::EntrySetYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 898\n");
            Var result;
            if (TryInvokeRemotely(EntrySetYear, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 901\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setYear"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Year, scriptContext);
    }

    Var JavascriptDate::EntrySetHours(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 921\n");
            Var result;
            if (TryInvokeRemotely(EntrySetHours, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 924\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setHours"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Hours, scriptContext);
    }

    Var JavascriptDate::EntrySetMilliseconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 944\n");
            Var result;
            if (TryInvokeRemotely(EntrySetMilliseconds, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 947\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setMilliseconds"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Milliseconds, scriptContext);
    }

    Var JavascriptDate::EntrySetMinutes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 967\n");
            Var result;
            if (TryInvokeRemotely(EntrySetMinutes, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 970\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setMinutes"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Minutes, scriptContext);
    }

    Var JavascriptDate::EntrySetMonth(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 990\n");
            Var result;
            if (TryInvokeRemotely(EntrySetMonth, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 993\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setMonth"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Month, scriptContext);
    }

    Var JavascriptDate::EntrySetSeconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1013\n");
            Var result;
            if (TryInvokeRemotely(EntrySetSeconds, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1016\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setSeconds"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetDateData(date, args, DateImplementation::DateData::Seconds, scriptContext);
    }

    Var JavascriptDate::EntrySetTime(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1036\n");
            Var result;
            if (TryInvokeRemotely(EntrySetTime, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1039\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setTime"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        double value;
        if (args.Info.Count > 1)
        {LOGMEIN("JavascriptDate.cpp] 1049\n");
            value = JavascriptConversion::ToNumber(args[1], scriptContext);
            if (Js::NumberUtilities::IsFinite(value))
            {LOGMEIN("JavascriptDate.cpp] 1052\n");
                value = JavascriptConversion::ToInteger(value);
            }
        }
        else
        {
            value = JavascriptNumber::NaN;
        }

        date->m_date.SetTvUtc(value);

        return JavascriptNumber::ToVarNoCheck(value, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCDate(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1076\n");
            Var result;
            if (TryInvokeRemotely(EntrySetUTCDate, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1079\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setUTCDate"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Date, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCFullYear(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1099\n");
            Var result;
            if (TryInvokeRemotely(EntrySetUTCFullYear, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1102\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setUTCFullYear"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::FullYear, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCHours(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1122\n");
            Var result;
            if (TryInvokeRemotely(EntrySetUTCHours, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1125\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setUTCHours"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Hours, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCMilliseconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1145\n");
            Var result;
            if (TryInvokeRemotely(EntrySetUTCMilliseconds, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1148\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setUTCMilliseconds"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Milliseconds, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCMinutes(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1168\n");
            Var result;
            if (TryInvokeRemotely(EntrySetUTCMinutes, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1171\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setUTCMinutes"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Minutes, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCMonth(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1191\n");
            Var result;
            if (TryInvokeRemotely(EntrySetUTCMonth, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1194\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setUTCMonth"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Month, scriptContext);
    }

    Var JavascriptDate::EntrySetUTCSeconds(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1214\n");
            Var result;
            if (TryInvokeRemotely(EntrySetUTCSeconds, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1217\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.setUTCSeconds"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        return JavascriptDate::SetUTCDateData(date, args, DateImplementation::DateData::Seconds, scriptContext);
    }

    Var JavascriptDate::EntryToDateString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1237\n");
            Var result;
            if (TryInvokeRemotely(EntryToDateString, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1240\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.toDateString"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::Default,
            DateImplementation::DateTimeFlag::NoTime);
    }

    Var JavascriptDate::EntryToISOString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));
        CHAKRATEL_LANGSTATS_INC_BUILTINCOUNT(Date_Prototype_toISOString);

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1264\n");
            Var result;
            if (TryInvokeRemotely(EntryToISOString, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1267\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.toISOString"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetISOString();
    }

    Var JavascriptDate::EntryToJSON(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0)
        {LOGMEIN("JavascriptDate.cpp] 1288\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Data.prototype.toJSON"));
        }
        RecyclableObject* thisObj = nullptr;
        if (FALSE == JavascriptConversion::ToObject(args[0], scriptContext, &thisObj))
        {LOGMEIN("JavascriptDate.cpp] 1293\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined, _u("Date.prototype.toJSON"));
        }

        Var result;
        if (TryInvokeRemotely(EntryToJSON, scriptContext, args, &result))
        {LOGMEIN("JavascriptDate.cpp] 1299\n");
            return result;
        }

        Var num = JavascriptConversion::ToPrimitive(thisObj, JavascriptHint::HintNumber, scriptContext);
        if (JavascriptNumber::Is(num)
            && !NumberUtilities::IsFinite(JavascriptNumber::GetValue(num)))
        {LOGMEIN("JavascriptDate.cpp] 1306\n");
            return scriptContext->GetLibrary()->GetNull();
        }

        Var toISO = JavascriptOperators::GetProperty(thisObj, PropertyIds::toISOString, scriptContext, NULL);
        if (!JavascriptConversion::IsCallable(toISO))
        {LOGMEIN("JavascriptDate.cpp] 1312\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_Property_NeedFunction, scriptContext->GetPropertyName(PropertyIds::toISOString)->GetBuffer());
        }
        RecyclableObject* toISOFunc = RecyclableObject::FromVar(toISO);
        return CALL_FUNCTION(toISOFunc, CallInfo(1), thisObj);
    }

    Var JavascriptDate::EntryToLocaleDateString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1329\n");
            Var result;
            if (TryInvokeRemotely(EntryToLocaleDateString, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1332\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.toLocaleDateString"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

#ifdef ENABLE_INTL_OBJECT
        if (CONFIG_FLAG(IntlBuiltIns) && scriptContext->IsIntlEnabled()){LOGMEIN("JavascriptDate.cpp] 1340\n");

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {LOGMEIN("JavascriptDate.cpp] 1344\n");
                IntlEngineInterfaceExtensionObject* extensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = extensionObject->GetDateToLocaleDateString();
                if (func)
                {LOGMEIN("JavascriptDate.cpp] 1348\n");
                    return func->CallFunction(args);
                }

                // Initialize Date.prototype.toLocaleDateString
                scriptContext->GetLibrary()->InitializeIntlForDatePrototype();
                func = extensionObject->GetDateToLocaleDateString();
                if (func)
                {LOGMEIN("JavascriptDate.cpp] 1356\n");
                    return func->CallFunction(args);
                }
            }
        }
#endif

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::Locale,
            DateImplementation::DateTimeFlag::NoTime);
    }

    Var JavascriptDate::EntryToLocaleString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1379\n");
            Var result;
            if (TryInvokeRemotely(EntryToLocaleString, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1382\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.toLocaleString"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

#ifdef ENABLE_INTL_OBJECT
        if (CONFIG_FLAG(IntlBuiltIns) && scriptContext->IsIntlEnabled()){LOGMEIN("JavascriptDate.cpp] 1390\n");

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {LOGMEIN("JavascriptDate.cpp] 1394\n");
                IntlEngineInterfaceExtensionObject* extensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = extensionObject->GetDateToLocaleString();
                if (func)
                {LOGMEIN("JavascriptDate.cpp] 1398\n");
                    return func->CallFunction(args);
                }
                // Initialize Date.prototype.toLocaleString
                scriptContext->GetLibrary()->InitializeIntlForDatePrototype();
                func = extensionObject->GetDateToLocaleString();
                if (func)
                {LOGMEIN("JavascriptDate.cpp] 1405\n");
                    return func->CallFunction(args);
                }
            }
        }
#endif

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return JavascriptDate::ToLocaleString(date);
    }

    JavascriptString* JavascriptDate::ToLocaleString(JavascriptDate* date)
    {LOGMEIN("JavascriptDate.cpp] 1417\n");
        return date->m_date.GetString(DateImplementation::DateStringFormat::Locale);
    }

    JavascriptString* JavascriptDate::ToString(JavascriptDate* date)
    {LOGMEIN("JavascriptDate.cpp] 1422\n");
        Assert(date);
        return date->m_date.GetString(DateImplementation::DateStringFormat::Default);
    }

    Var JavascriptDate::EntryToLocaleTimeString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1437\n");
            Var result;
            if (TryInvokeRemotely(EntryToLocaleTimeString, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1440\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.toLocaleTimeString"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

#ifdef ENABLE_INTL_OBJECT
        if (CONFIG_FLAG(IntlBuiltIns) && scriptContext->IsIntlEnabled()){LOGMEIN("JavascriptDate.cpp] 1448\n");

            EngineInterfaceObject* nativeEngineInterfaceObj = scriptContext->GetLibrary()->GetEngineInterfaceObject();
            if (nativeEngineInterfaceObj)
            {LOGMEIN("JavascriptDate.cpp] 1452\n");
                IntlEngineInterfaceExtensionObject* extensionObject = static_cast<IntlEngineInterfaceExtensionObject*>(nativeEngineInterfaceObj->GetEngineExtension(EngineInterfaceExtensionKind_Intl));
                JavascriptFunction* func = extensionObject->GetDateToLocaleTimeString();
                if (func)
                {LOGMEIN("JavascriptDate.cpp] 1456\n");
                    return func->CallFunction(args);
                }
                // Initialize Date.prototype.toLocaleTimeString
                scriptContext->GetLibrary()->InitializeIntlForDatePrototype();
                func = extensionObject->GetDateToLocaleTimeString();
                if (func)
                {LOGMEIN("JavascriptDate.cpp] 1463\n");
                    return func->CallFunction(args);
                }
            }
        }
#endif

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::Locale,
            DateImplementation::DateTimeFlag::NoDate);
    }

    Var JavascriptDate::EntryToTimeString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1486\n");
            Var result;
            if (TryInvokeRemotely(EntryToTimeString, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1489\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.toTimeString"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);


        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::Default,
            DateImplementation::DateTimeFlag::NoDate);
    }

    // CONSIDER: ToGMTString and ToUTCString is the same, but currently the profiler use the entry point address to identify
    // the entry point. So we will have to make the function different. Consider using FunctionInfo to identify the function
    Var JavascriptDate::EntryToGMTString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        return JavascriptFunction::CallFunction<true>(function, JavascriptDate::EntryToUTCString, args);
    }

    Var JavascriptDate::EntryToUTCString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1523\n");
            Var result;
            if (TryInvokeRemotely(EntryToUTCString, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1526\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.toUTCString"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return date->m_date.GetString(
            DateImplementation::DateStringFormat::GMT,
            DateImplementation::DateTimeFlag::None);
    }

    Var JavascriptDate::EntryValueOf(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1549\n");
            Var result;
            if (TryInvokeRemotely(EntryValueOf, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1552\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.valueOf"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        double value = date->m_date.GetMilliSeconds();
        return JavascriptNumber::ToVarNoCheck(value, scriptContext);
    }

    Var JavascriptDate::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !JavascriptDate::Is(args[0]))
        {LOGMEIN("JavascriptDate.cpp] 1574\n");
            Var result;
            if (TryInvokeRemotely(EntryToString, scriptContext, args, &result))
            {LOGMEIN("JavascriptDate.cpp] 1577\n");
                return result;
            }
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedDate, _u("Date.prototype.toString"));
        }
        JavascriptDate* date = JavascriptDate::FromVar(args[0]);

        AssertMsg(args.Info.Count > 0, "Negative argument count");
        return JavascriptDate::ToString(date);
    }

    BOOL JavascriptDate::TryInvokeRemotely(JavascriptMethod entryPoint, ScriptContext * scriptContext, Arguments & args, Var * result)
    {LOGMEIN("JavascriptDate.cpp] 1589\n");
        if (JavascriptOperators::GetTypeId(args[0]) == TypeIds_HostDispatch)
        {LOGMEIN("JavascriptDate.cpp] 1591\n");
            if (RecyclableObject::FromVar(args[0])->InvokeBuiltInOperationRemotely(entryPoint, args, result))
            {LOGMEIN("JavascriptDate.cpp] 1593\n");
                return TRUE;
            }
        }
        return FALSE;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptDate::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptDate.cpp] 1602\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapDateObject;
    }

    void JavascriptDate::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptDate.cpp] 1607\n");
        TTDAssert(this->GetTypeId() == TypeIds_Date, "We don't handle WinRT or other types of dates yet!");

        double* millis = alloc.SlabAllocateStruct<double>();
        *millis = m_date.GetMilliSeconds();

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<double*, TTD::NSSnapObjects::SnapObjectType::SnapDateObject>(objData, millis);
    }
#endif

    BOOL JavascriptDate::ToPrimitive(JavascriptHint hint, Var* result, ScriptContext * requestContext)
    {LOGMEIN("JavascriptDate.cpp] 1618\n");
        if (hint == JavascriptHint::None)
        {LOGMEIN("JavascriptDate.cpp] 1620\n");
            hint = JavascriptHint::HintString;
        }

        return DynamicObject::ToPrimitive(hint, result, requestContext);
    }

    BOOL JavascriptDate::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {
        ENTER_PINNED_SCOPE(JavascriptString, valueStr);
        valueStr = this->m_date.GetString(DateImplementation::DateStringFormat::Default);
        stringBuilder->Append(valueStr->GetString(), valueStr->GetLength());
        LEAVE_PINNED_SCOPE();
        return TRUE;
    }

    BOOL JavascriptDate::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptDate.cpp] 1637\n");
        stringBuilder->AppendCppLiteral(_u("Object, (Date)"));
        return TRUE;
    }
} // namespace Js
