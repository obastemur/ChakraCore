//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "errstr.h"

#ifdef ERROR_TRACE
#define TRACE_ERROR(...) {LOGMEIN("JavascriptError.cpp] 8\n"); Trace(__VA_ARGS__); }
#else
#define TRACE_ERROR(...)
#endif

namespace Js
{
    DWORD JavascriptError::GetAdjustedResourceStringHr(DWORD hr, bool isFormatString)
    {LOGMEIN("JavascriptError.cpp] 16\n");
        AssertMsg(FACILITY_CONTROL == HRESULT_FACILITY(hr) || FACILITY_JSCRIPT == HRESULT_FACILITY(hr), "Chakra hr are either FACILITY_CONTROL (for private HRs) or FACILITY_JSCRIPT (for public HRs)");
        WORD scodeIncr = isFormatString ? RTERROR_STRINGFORMAT_OFFSET : 0; // default for FACILITY_CONTROL == HRESULT_FACILITY(hr)
        if (FACILITY_JSCRIPT == HRESULT_FACILITY(hr))
        {LOGMEIN("JavascriptError.cpp] 20\n");
            scodeIncr += RTERROR_PUBLIC_RESOURCEOFFSET;
        }

        hr += scodeIncr;

        return hr;
    }

    bool JavascriptError::Is(Var aValue)
    {LOGMEIN("JavascriptError.cpp] 30\n");
        AssertMsg(aValue != NULL, "Error is NULL - did it come from an out of memory exception?");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Error;
    }

    bool JavascriptError::IsRemoteError(Var aValue)
    {LOGMEIN("JavascriptError.cpp] 36\n");
        // IJscriptInfo is not remotable (we don't register the proxy),
        // so we can't query for actual remote object
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_HostDispatch;
    }

    bool JavascriptError::HasDebugInfo()
    {LOGMEIN("JavascriptError.cpp] 43\n");
        return false;
    }

    void JavascriptError::SetNotEnumerable(PropertyId propertyId)
    {
        // Not all the properties of Error objects (like description, stack, number etc.) are in the spec.
        // Other browsers have all the properties as not-enumerable.
        SetEnumerable(propertyId, false);
    }

    Var JavascriptError::NewInstance(RecyclableObject* function, JavascriptError* pError, CallInfo callInfo, Var newTarget, Var message)
    {LOGMEIN("JavascriptError.cpp] 55\n");
        ScriptContext* scriptContext = function->GetScriptContext();

        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        JavascriptString* messageString = nullptr;

        if (JavascriptOperators::GetTypeId(message) != TypeIds_Undefined)
        {LOGMEIN("JavascriptError.cpp] 62\n");
            messageString = JavascriptConversion::ToString(message, scriptContext);
        }

        if (messageString)
        {LOGMEIN("JavascriptError.cpp] 67\n");
            JavascriptOperators::SetProperty(pError, pError, PropertyIds::message, messageString, scriptContext);
            pError->SetNotEnumerable(PropertyIds::message);
        }

        JavascriptExceptionContext exceptionContext;
        JavascriptExceptionOperators::WalkStackForExceptionContext(*scriptContext, exceptionContext, pError,
            JavascriptExceptionOperators::StackCrawlLimitOnThrow(pError, *scriptContext), /*returnAddress=*/ nullptr, /*isThrownException=*/ false, /*resetSatck=*/ false);
        JavascriptExceptionOperators::AddStackTraceToObject(pError, exceptionContext.GetStackTrace(), *scriptContext, /*isThrownException=*/ false, /*resetSatck=*/ false);

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), pError, nullptr, scriptContext) :
            pError;
    }

    Var JavascriptError::NewErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetLibrary()->CreateError();

        // Process the arguments for IE specific behaviors for numbers and description

        JavascriptString* descriptionString = nullptr;
        Var message;
        bool hasNumber = false;
        double number = 0;
        if (args.Info.Count >= 3)
        {LOGMEIN("JavascriptError.cpp] 97\n");
            hasNumber = true;
            number = JavascriptConversion::ToNumber(args[1], scriptContext);
            message = args[2];

            descriptionString = JavascriptConversion::ToString(message, scriptContext);
        }
        else if (args.Info.Count == 2)
        {LOGMEIN("JavascriptError.cpp] 105\n");
            message = args[1];
            descriptionString = JavascriptConversion::ToString(message, scriptContext);
        }
        else
        {
            hasNumber = true;
            message = scriptContext->GetLibrary()->GetUndefined();
            descriptionString = scriptContext->GetLibrary()->GetEmptyString();
        }

        Assert(descriptionString != nullptr);
        if (hasNumber)
        {LOGMEIN("JavascriptError.cpp] 118\n");
            JavascriptOperators::InitProperty(pError, PropertyIds::number, JavascriptNumber::ToVarNoCheck(number, scriptContext));
            pError->SetNotEnumerable(PropertyIds::number);
        }
        JavascriptOperators::SetProperty(pError, pError, PropertyIds::description, descriptionString, scriptContext);
        pError->SetNotEnumerable(PropertyIds::description);

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        return JavascriptError::NewInstance(function, pError, callInfo, newTarget, message);
    }

#define NEW_ERROR(name) \
    Var JavascriptError::New##name##Instance(RecyclableObject* function, CallInfo callInfo, ...) \
    { \
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault); \
        ARGUMENTS(args, callInfo); \
        ScriptContext* scriptContext = function->GetScriptContext(); \
        JavascriptError* pError = scriptContext->GetLibrary()->Create##name(); \
        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0]; \
        Var message = args.Info.Count > 1 ? args[1] : scriptContext->GetLibrary()->GetUndefined(); \
        return JavascriptError::NewInstance(function, pError, callInfo, newTarget, message); \
    }
    NEW_ERROR(EvalError);
    NEW_ERROR(RangeError);
    NEW_ERROR(ReferenceError);
    NEW_ERROR(SyntaxError);
    NEW_ERROR(TypeError);
    NEW_ERROR(URIError);
    NEW_ERROR(WebAssemblyCompileError);
    NEW_ERROR(WebAssemblyRuntimeError);
    NEW_ERROR(WebAssemblyLinkError);

#undef NEW_ERROR

#ifdef ENABLE_PROJECTION
    Var JavascriptError::NewWinRTErrorInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();
        JavascriptError* pError = scriptContext->GetHostScriptContext()->CreateWinRTError(nullptr, nullptr);

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        Var message = args.Info.Count > 1 ? args[1] : scriptContext->GetLibrary()->GetUndefined();
        return JavascriptError::NewInstance(function, pError, callInfo, newTarget, message);
    }
#endif

    Var JavascriptError::EntryToString(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args[0] == 0 || !JavascriptOperators::IsObject(args[0]))
        {LOGMEIN("JavascriptError.cpp] 179\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NeedObject, _u("Error.prototype.toString"));
        }

        RecyclableObject * thisError = RecyclableObject::FromVar(args[0]);
        Var value = NULL;
        JavascriptString *outputStr, *message;

        // get error.name
        BOOL hasName = JavascriptOperators::GetProperty(thisError, PropertyIds::name, &value, scriptContext, NULL) &&
            JavascriptOperators::GetTypeId(value) != TypeIds_Undefined;

        if (hasName)
        {LOGMEIN("JavascriptError.cpp] 192\n");
            outputStr = JavascriptConversion::ToString(value, scriptContext);
        }
        else
        {
            outputStr = scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("Error"));
        }

        // get error.message
        if (JavascriptOperators::GetProperty(thisError, PropertyIds::message, &value, scriptContext, NULL)
            && JavascriptOperators::GetTypeId(value) != TypeIds_Undefined)
        {LOGMEIN("JavascriptError.cpp] 203\n");
            message = JavascriptConversion::ToString(value, scriptContext);
        }
        else
        {
            message = scriptContext->GetLibrary()->GetEmptyString();
        }

        charcount_t nameLen = outputStr->GetLength();
        charcount_t msgLen = message->GetLength();

        if (nameLen > 0 && msgLen > 0)
        {LOGMEIN("JavascriptError.cpp] 215\n");
           outputStr = JavascriptString::Concat(outputStr, scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u(": ")));
           outputStr = JavascriptString::Concat(outputStr, message);
        }
        else if (msgLen > 0)
        {LOGMEIN("JavascriptError.cpp] 220\n");
            outputStr = message;
        }

        return outputStr;
    }

    void __declspec(noreturn) JavascriptError::MapAndThrowError(ScriptContext* scriptContext, HRESULT hr)
    {LOGMEIN("JavascriptError.cpp] 228\n");
        ErrorTypeEnum errorType;
        hr = MapHr(hr, &errorType);

        JavascriptError::MapAndThrowError(scriptContext, hr, errorType, nullptr);
    }

    void __declspec(noreturn) JavascriptError::MapAndThrowError(ScriptContext* scriptContext, HRESULT hr, ErrorTypeEnum errorType, EXCEPINFO* pei)
    {LOGMEIN("JavascriptError.cpp] 236\n");
        JavascriptError* pError = MapError(scriptContext, errorType);
        SetMessageAndThrowError(scriptContext, pError, hr, pei);
    }

    void __declspec(noreturn) JavascriptError::SetMessageAndThrowError(ScriptContext* scriptContext, JavascriptError *pError, int32 hCode, EXCEPINFO* pei)
    {LOGMEIN("JavascriptError.cpp] 242\n");
        PCWSTR varName = (pei ? pei->bstrDescription : nullptr);

        JavascriptError::SetErrorMessage(pError, hCode, varName, scriptContext);

        if (pei)
        {LOGMEIN("JavascriptError.cpp] 248\n");
            FreeExcepInfo(pei);
        }

        JavascriptExceptionOperators::Throw(pError, scriptContext);
    }

#define THROW_ERROR_IMPL(err_method, create_method, get_type_method, err_type) \
    static JavascriptError* create_method(ScriptContext* scriptContext) \
    {LOGMEIN("JavascriptError.cpp] 257\n"); \
        JavascriptLibrary *library = scriptContext->GetLibrary(); \
        JavascriptError *pError = library->create_method(); \
        return pError; \
    } \
    \
    void __declspec(noreturn) JavascriptError::err_method(ScriptContext* scriptContext, int32 hCode, EXCEPINFO* pei) \
    {LOGMEIN("JavascriptError.cpp] 264\n"); \
        JavascriptError *pError = create_method(scriptContext); \
        SetMessageAndThrowError(scriptContext, pError, hCode, pei); \
    } \
    \
    void __declspec(noreturn) JavascriptError::err_method(ScriptContext* scriptContext, int32 hCode, PCWSTR varName) \
    {LOGMEIN("JavascriptError.cpp] 270\n"); \
        JavascriptLibrary *library = scriptContext->GetLibrary(); \
        JavascriptError *pError = library->create_method(); \
        JavascriptError::SetErrorMessage(pError, hCode, varName, scriptContext); \
        JavascriptExceptionOperators::Throw(pError, scriptContext); \
    } \
    \
    void __declspec(noreturn) JavascriptError::err_method(ScriptContext* scriptContext, int32 hCode, JavascriptString* varName) \
    {LOGMEIN("JavascriptError.cpp] 278\n"); \
        JavascriptLibrary *library = scriptContext->GetLibrary(); \
        JavascriptError *pError = library->create_method(); \
        JavascriptError::SetErrorMessage(pError, hCode, varName->GetSz(), scriptContext); \
        JavascriptExceptionOperators::Throw(pError, scriptContext); \
    } \
    \
    void __declspec(noreturn) JavascriptError::err_method##Var(ScriptContext* scriptContext, int32 hCode, ...) \
    { \
        JavascriptLibrary *library = scriptContext->GetLibrary(); \
        JavascriptError *pError = library->create_method(); \
        va_list argList; \
        va_start(argList, hCode); \
        JavascriptError::SetErrorMessage(pError, hCode, scriptContext, argList); \
        va_end(argList); \
        JavascriptExceptionOperators::Throw(pError, scriptContext); \
    }

    THROW_ERROR_IMPL(ThrowError, CreateError, GetErrorType, kjstError)
    THROW_ERROR_IMPL(ThrowRangeError, CreateRangeError, GetRangeErrorType, kjstRangeError)
    THROW_ERROR_IMPL(ThrowReferenceError, CreateReferenceError, GetReferenceErrorType, kjstReferenceError)
    THROW_ERROR_IMPL(ThrowSyntaxError, CreateSyntaxError, GetSyntaxErrorType, kjstSyntaxError)
    THROW_ERROR_IMPL(ThrowTypeError, CreateTypeError, GetTypeErrorType, kjstTypeError)
    THROW_ERROR_IMPL(ThrowURIError, CreateURIError, GetURIErrorType, kjstURIError)
    THROW_ERROR_IMPL(ThrowWebAssemblyCompileError, CreateWebAssemblyCompileError, GetWebAssemblyCompileErrorType, kjstWebAssemblyCompileError)
    THROW_ERROR_IMPL(ThrowWebAssemblyRuntimeError, CreateWebAssemblyRuntimeError, GetWebAssemblyRuntimeErrorType, kjstWebAssemblyRuntimeError)
    THROW_ERROR_IMPL(ThrowWebAssemblyLinkError, CreateWebAssemblyLinkError, GetWebAssemblyLinkErrorType, kjstWebAssemblyLinkError)
#undef THROW_ERROR_IMPL

    void __declspec(noreturn) JavascriptError::ThrowUnreachable(ScriptContext* scriptContext) { ThrowWebAssemblyRuntimeError(scriptContext, WASMERR_Unreachable); }
    JavascriptError* JavascriptError::MapError(ScriptContext* scriptContext, ErrorTypeEnum errorType)
    {LOGMEIN("JavascriptError.cpp] 309\n");
        switch (errorType)
        {LOGMEIN("JavascriptError.cpp] 311\n");
        case kjstError:
          return CreateError(scriptContext);
        case kjstTypeError:
          return CreateTypeError(scriptContext);
        case kjstRangeError:
          return CreateRangeError(scriptContext);
        case kjstSyntaxError:
          return CreateSyntaxError(scriptContext);
        case kjstReferenceError:
          return CreateReferenceError(scriptContext);
        case kjstURIError:
          return CreateURIError(scriptContext);
        case kjstWebAssemblyCompileError:
          return CreateWebAssemblyCompileError(scriptContext);
        case kjstWebAssemblyRuntimeError:
          return CreateWebAssemblyRuntimeError(scriptContext);
        case kjstWebAssemblyLinkError:
            return CreateWebAssemblyLinkError(scriptContext);
        default:
            AssertMsg(FALSE, "Invalid error type");
            __assume(false);
        };
    }

    void __declspec(noreturn) JavascriptError::ThrowDispatchError(ScriptContext* scriptContext, HRESULT hCode, PCWSTR message)
    {LOGMEIN("JavascriptError.cpp] 337\n");
        JavascriptError *pError = scriptContext->GetLibrary()->CreateError();
        JavascriptError::SetErrorMessageProperties(pError, hCode, message, scriptContext);
        JavascriptExceptionOperators::Throw(pError, scriptContext);
    }

    void JavascriptError::SetErrorMessageProperties(JavascriptError *pError, HRESULT hr, PCWSTR message, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptError.cpp] 344\n");
        JavascriptString * messageString;
        if (message != nullptr)
        {LOGMEIN("JavascriptError.cpp] 347\n");
            // Save the runtime error message to be reported to IE.
            pError->originalRuntimeErrorMessage = message;
            messageString = Js::JavascriptString::NewWithSz(message, scriptContext);
        }
        else
        {
            messageString = scriptContext->GetLibrary()->GetEmptyString();
            // Set an empty string so we will return it as a runtime message with the error code to IE
            pError->originalRuntimeErrorMessage = _u("");
        }

        JavascriptOperators::InitProperty(pError, PropertyIds::message, messageString);
        pError->SetNotEnumerable(PropertyIds::message);

        JavascriptOperators::InitProperty(pError, PropertyIds::description, messageString);
        pError->SetNotEnumerable(PropertyIds::description);

        hr = JavascriptError::GetErrorNumberFromResourceID(hr);
        JavascriptOperators::InitProperty(pError, PropertyIds::number, JavascriptNumber::ToVar((int32)hr, scriptContext));
        pError->SetNotEnumerable(PropertyIds::number);
    }

    void JavascriptError::SetErrorMessage(JavascriptError *pError, HRESULT hr, ScriptContext* scriptContext, va_list argList)
    {LOGMEIN("JavascriptError.cpp] 371\n");
        Assert(FAILED(hr));
        char16 * allocatedString = nullptr;

        if (FACILITY_CONTROL == HRESULT_FACILITY(hr) || FACILITY_JSCRIPT == HRESULT_FACILITY(hr))
        {LOGMEIN("JavascriptError.cpp] 376\n");
#if !(defined(_M_ARM) && defined(__clang__))
            if (argList != nullptr)
#endif
            {LOGMEIN("JavascriptError.cpp] 380\n");
                HRESULT hrAdjusted = GetAdjustedResourceStringHr(hr, /* isFormatString */ true);

                BSTR message = BstrGetResourceString(hrAdjusted);
                if (message != nullptr)
                {LOGMEIN("JavascriptError.cpp] 385\n");
                    size_t len = _vscwprintf(message, argList);
                    Assert(len > 0);
                    len = AllocSizeMath::Add(len, 1);
                    allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), char16, len);

#pragma prefast(push)
#pragma prefast(disable:26014, "allocatedString allocated size more than msglen")
#pragma prefast(disable:26000, "allocatedString allocated size more than msglen")
                    len = vswprintf_s(allocatedString, len, message, argList);
                    Assert(len > 0);
#pragma prefast(pop)

                    SysFreeString(message);
                }
            }
            if (allocatedString == nullptr)
            {LOGMEIN("JavascriptError.cpp] 402\n");
                HRESULT hrAdjusted = GetAdjustedResourceStringHr(hr, /* isFormatString */ false);

                BSTR message = BstrGetResourceString(hrAdjusted);
                if (message == nullptr)
                {LOGMEIN("JavascriptError.cpp] 407\n");
                    message = BstrGetResourceString(IDS_UNKNOWN_RUNTIME_ERROR);
                }
                if (message != nullptr)
                {LOGMEIN("JavascriptError.cpp] 411\n");
                    uint32 len = SysStringLen(message) +1;
                    allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), char16, len);
                    wcscpy_s(allocatedString, len, message);
                    SysFreeString(message);
                }
            }
        }
        JavascriptError::SetErrorMessageProperties(pError, hr, allocatedString, scriptContext);
    }

    void JavascriptError::SetErrorMessage(JavascriptError *pError, HRESULT hr, PCWSTR varName, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptError.cpp] 423\n");
        Assert(FAILED(hr));
        char16 * allocatedString = nullptr;

        if (FACILITY_CONTROL == HRESULT_FACILITY(hr) || FACILITY_JSCRIPT == HRESULT_FACILITY(hr))
        {LOGMEIN("JavascriptError.cpp] 428\n");
            if (varName != nullptr)
            {LOGMEIN("JavascriptError.cpp] 430\n");
                HRESULT hrAdjusted = GetAdjustedResourceStringHr(hr, /* isFormatString */ true);

                BSTR message = BstrGetResourceString(hrAdjusted);
                if (message != nullptr)
                {LOGMEIN("JavascriptError.cpp] 435\n");
                    uint32 msglen = SysStringLen(message);
                    size_t varlen = wcslen(varName);
                    size_t len = AllocSizeMath::Add(msglen, varlen);
                    allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), char16, len);
                    size_t outputIndex = 0;
                    for (size_t i = 0; i < msglen; i++)
                    {LOGMEIN("JavascriptError.cpp] 442\n");
                        Assert(outputIndex < len);
                        if (message[i] == _u('%') && i + 1 < msglen && message[i+1] == _u('s'))
                        {LOGMEIN("JavascriptError.cpp] 445\n");
                            Assert(len - outputIndex >= varlen);
                            wcscpy_s(allocatedString + outputIndex, len - outputIndex, varName);
                            outputIndex += varlen;
                            wcscpy_s(allocatedString + outputIndex, len - outputIndex, message + i + 2);
                            outputIndex += (msglen - i);
                            break;
                        }
#pragma prefast(push)
#pragma prefast(disable:26014, "allocatedString allocated size more than msglen")
#pragma prefast(disable:26000, "allocatedString allocated size more than msglen")
                        allocatedString[outputIndex++] = message[i];
#pragma prefast(pop)
                    }
                    SysFreeString(message);
                    if (outputIndex != len)
                    {LOGMEIN("JavascriptError.cpp] 461\n");
                        allocatedString = nullptr;
                    }
                }
            }
            if (allocatedString == nullptr)
            {LOGMEIN("JavascriptError.cpp] 467\n");
                HRESULT hrAdjusted = GetAdjustedResourceStringHr(hr, /* isFormatString */ false);

                BSTR message = BstrGetResourceString(hrAdjusted);
                if (message == nullptr)
                {LOGMEIN("JavascriptError.cpp] 472\n");
                    message = BstrGetResourceString(IDS_UNKNOWN_RUNTIME_ERROR);
                }
                if (message != nullptr)
                {LOGMEIN("JavascriptError.cpp] 476\n");
                    uint32 len = SysStringLen(message) +1;
                    allocatedString = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), char16, len);
                    wcscpy_s(allocatedString, len, message);
                    SysFreeString(message);
                }
            }
        }
        JavascriptError::SetErrorMessageProperties(pError, hr, allocatedString, scriptContext);
    }

    void JavascriptError::SetErrorType(JavascriptError *pError, ErrorTypeEnum errorType)
    {LOGMEIN("JavascriptError.cpp] 488\n");
        pError->m_errorType = errorType;
    }

    BOOL JavascriptError::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptError.cpp] 493\n");
        char16 const *pszMessage = nullptr;

        if (!this->GetScriptContext()->GetThreadContext()->IsScriptActive())
        {
            GetRuntimeErrorWithScriptEnter(this, &pszMessage);
        }
        else
        {
            GetRuntimeError(this, &pszMessage);
        }

        if (pszMessage)
        {LOGMEIN("JavascriptError.cpp] 506\n");
            stringBuilder->AppendSz(pszMessage);
            return TRUE;
        }

        return TRUE; // Return true to display an empty string
    }

    BOOL JavascriptError::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptError.cpp] 515\n");
        stringBuilder->AppendCppLiteral(_u("Error"));
        return TRUE;
    }

    HRESULT JavascriptError::GetRuntimeError(RecyclableObject* errorObject, __out_opt LPCWSTR * pMessage)
    {LOGMEIN("JavascriptError.cpp] 521\n");
        // Only report the error number if it is a runtime error
        HRESULT hr = JSERR_UncaughtException;
        ScriptContext* scriptContext = errorObject->GetScriptContext();

        // This version needs to be called in script.
        Assert(scriptContext->GetThreadContext()->IsScriptActive());

        Var number = JavascriptOperators::GetProperty(errorObject, Js::PropertyIds::number, scriptContext, NULL);
        if (TaggedInt::Is(number))
        {LOGMEIN("JavascriptError.cpp] 531\n");
            hr = TaggedInt::ToInt32(number);
        }
        else if (JavascriptNumber::Is_NoTaggedIntCheck(number))
        {LOGMEIN("JavascriptError.cpp] 535\n");
            hr = (HRESULT)JavascriptNumber::GetValue(number);
        }
        if (!FAILED(hr))
        {LOGMEIN("JavascriptError.cpp] 539\n");
            hr = E_FAIL;
        }

        if (pMessage != NULL)
        {LOGMEIN("JavascriptError.cpp] 544\n");
            *pMessage = _u("");  // default to have IE load the error message, by returning empty-string

            // The description property always overrides any error message
            Var description = Js::JavascriptOperators::GetProperty(errorObject, Js::PropertyIds::description, scriptContext, NULL);
            if (JavascriptString::Is(description))
            {LOGMEIN("JavascriptError.cpp] 550\n");
                // Always report the description to IE if it is a string, even if the user sets it
                JavascriptString * messageString = JavascriptString::FromVar(description);
                *pMessage = messageString->GetSz();
            }
            else if (Js::JavascriptError::Is(errorObject) && Js::JavascriptError::FromVar(errorObject)->originalRuntimeErrorMessage != nullptr)
            {LOGMEIN("JavascriptError.cpp] 556\n");
                // use the runtime error message
                *pMessage = Js::JavascriptError::FromVar(errorObject)->originalRuntimeErrorMessage;
            }
            else if (FACILITY_CONTROL == HRESULT_FACILITY(hr))
            {LOGMEIN("JavascriptError.cpp] 561\n");
                // User might have create it's own Error object with JS error code, try to load the
                // resource string from the HResult by returning null;
                *pMessage = nullptr;
            }
        }

        // If neither the description or original runtime error message is set, and there are no error message.
        // Then just return false and we will report Uncaught exception to IE.
        return hr;
    }

    HRESULT JavascriptError::GetRuntimeErrorWithScriptEnter(RecyclableObject* errorObject, __out_opt LPCWSTR * pMessage)
    {LOGMEIN("JavascriptError.cpp] 574\n");
        ScriptContext* scriptContext = errorObject->GetScriptContext();
        Assert(!scriptContext->GetThreadContext()->IsScriptActive());

        // Use _NOT_SCRIPT. We enter runtime to get error info, likely inside a catch.
        BEGIN_JS_RUNTIME_CALL_NOT_SCRIPT(scriptContext)
        {LOGMEIN("JavascriptError.cpp] 580\n");
            return GetRuntimeError(errorObject, pMessage);
        }
        END_JS_RUNTIME_CALL(scriptContext);
    }

    void __declspec(noreturn) JavascriptError::ThrowOutOfMemoryError(ScriptContext *scriptContext)
    {LOGMEIN("JavascriptError.cpp] 587\n");
        JavascriptExceptionOperators::ThrowOutOfMemory(scriptContext);
    }

    void __declspec(noreturn) JavascriptError::ThrowStackOverflowError(ScriptContext *scriptContext, PVOID returnAddress)
    {LOGMEIN("JavascriptError.cpp] 592\n");
        JavascriptExceptionOperators::ThrowStackOverflow(scriptContext, returnAddress);
    }

    void __declspec(noreturn) JavascriptError::ThrowParserError(ScriptContext* scriptContext, HRESULT hrParser, CompileScriptException* se)
    {LOGMEIN("JavascriptError.cpp] 597\n");
        Assert(FAILED(hrParser));

        hrParser = SCRIPT_E_RECORDED;
        EXCEPINFO ei;
        se->GetError(&hrParser, &ei);

        JavascriptError* pError = MapParseError(scriptContext, ei.scode);
        JavascriptError::SetMessageAndThrowError(scriptContext, pError, ei.scode, &ei);
    }

    ErrorTypeEnum JavascriptError::MapParseError(int32 hCode)
    {LOGMEIN("JavascriptError.cpp] 609\n");
        switch (hCode)
        {LOGMEIN("JavascriptError.cpp] 611\n");
#define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
        case name: \
            return jst; \
        break;
#define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
#include "rterrors.h"
#undef RT_PUBLICERROR_MSG
#undef RT_ERROR_MSG
        default:
            return kjstSyntaxError;
        }
    }

    JavascriptError* JavascriptError::MapParseError(ScriptContext* scriptContext, int32 hCode)
    {LOGMEIN("JavascriptError.cpp] 626\n");
        ErrorTypeEnum errorType = JavascriptError::MapParseError(hCode);
        return MapError(scriptContext, errorType);
    }

    bool JavascriptError::ThrowCantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext, PropertyId propertyId)
    {LOGMEIN("JavascriptError.cpp] 632\n");
        if (flags & PropertyOperation_ThrowIfNonWritable)
        {LOGMEIN("JavascriptError.cpp] 634\n");
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("JavascriptError.cpp] 636\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotWritable, scriptContext->GetPropertyName(propertyId)->GetBuffer());
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext, uint32 index)
    {LOGMEIN("JavascriptError.cpp] 645\n");
        if (flags & PropertyOperation_ThrowIfNonWritable)
        {LOGMEIN("JavascriptError.cpp] 647\n");
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("JavascriptError.cpp] 649\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotWritable, JavascriptConversion::ToString(JavascriptNumber::ToVar(index, scriptContext), scriptContext)->GetSz());
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantAssignIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptError.cpp] 658\n");
        if (flags & PropertyOperation_StrictMode)
        {LOGMEIN("JavascriptError.cpp] 660\n");
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("JavascriptError.cpp] 662\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_CantAssignToReadOnly);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantExtendIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptError.cpp] 671\n");
        if (flags & PropertyOperation_StrictMode)
        {LOGMEIN("JavascriptError.cpp] 673\n");
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("JavascriptError.cpp] 675\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NonExtensibleObject);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantDeleteIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext, PCWSTR varName)
    {LOGMEIN("JavascriptError.cpp] 684\n");
        if (flags & PropertyOperation_StrictMode)
        {LOGMEIN("JavascriptError.cpp] 686\n");
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("JavascriptError.cpp] 688\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_CantDeleteExpr, varName);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowCantDelete(PropertyOperationFlags flags, ScriptContext* scriptContext, PCWSTR varName)
    {LOGMEIN("JavascriptError.cpp] 697\n");
        bool isNonConfigThrow = (flags & PropertyOperation_ThrowOnDeleteIfNotConfig) == PropertyOperation_ThrowOnDeleteIfNotConfig;

        if (isNonConfigThrow || flags & PropertyOperation_StrictMode)
        {LOGMEIN("JavascriptError.cpp] 701\n");
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("JavascriptError.cpp] 703\n");
                JavascriptError::ThrowTypeError(scriptContext, isNonConfigThrow ? JSERR_CantDeleteNonConfigProp : JSERR_CantDeleteExpr, varName);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowIfStrictModeUndefinedSetter(
        PropertyOperationFlags flags, Var setterValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptError.cpp] 713\n");
        if ((flags & PropertyOperation_StrictMode)
            && JavascriptOperators::IsUndefinedAccessor(setterValue, scriptContext))
        {LOGMEIN("JavascriptError.cpp] 716\n");
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("JavascriptError.cpp] 718\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_CantAssignToReadOnly);
            }
            return true;
        }
        return false;
    }

    bool JavascriptError::ThrowIfNotExtensibleUndefinedSetter(PropertyOperationFlags flags, Var setterValue, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptError.cpp] 727\n");
        if ((flags & PropertyOperation_ThrowIfNotExtensible)
            && JavascriptOperators::IsUndefinedAccessor(setterValue, scriptContext))
        {LOGMEIN("JavascriptError.cpp] 730\n");
            if (scriptContext->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("JavascriptError.cpp] 732\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotExtensible);
            }
            return true;
        }
        return false;
    }

    // Gets the error number associated with the resource ID for an error message.
    // When 'errorNumSource' is 0 (the default case), the resource ID is used as the error number.
    int32 JavascriptError::GetErrorNumberFromResourceID(int32 resourceId)
    {LOGMEIN("JavascriptError.cpp] 743\n");
        int32 result;
        switch (resourceId)
        {LOGMEIN("JavascriptError.cpp] 746\n");
    #define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
            case name: \
                result = (errorNumSource == 0) ? name : errorNumSource; \
                break;
    #define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
    #include "rterrors.h"
    #undef RT_PUBLICERROR_MSG
    #undef RT_ERROR_MSG
            default:
                result = resourceId;
        }

        return result;
    }

    JavascriptError* JavascriptError::CreateNewErrorOfSameType(JavascriptLibrary* targetJavascriptLibrary)
    {LOGMEIN("JavascriptError.cpp] 763\n");
        JavascriptError* jsNewError = nullptr;
        switch (this->GetErrorType())
        {LOGMEIN("JavascriptError.cpp] 766\n");
        case kjstError:
            jsNewError = targetJavascriptLibrary->CreateError();
            break;
        case kjstEvalError:
            jsNewError = targetJavascriptLibrary->CreateEvalError();
            break;
        case kjstRangeError:
            jsNewError = targetJavascriptLibrary->CreateRangeError();
            break;
        case kjstReferenceError:
            jsNewError = targetJavascriptLibrary->CreateReferenceError();
            break;
        case kjstSyntaxError:
            jsNewError = targetJavascriptLibrary->CreateSyntaxError();
            break;
        case kjstTypeError:
            jsNewError = targetJavascriptLibrary->CreateTypeError();
            break;
        case kjstURIError:
            jsNewError = targetJavascriptLibrary->CreateURIError();
            break;
        case kjstWebAssemblyCompileError:
            jsNewError = targetJavascriptLibrary->CreateWebAssemblyCompileError();
        case kjstWebAssemblyRuntimeError:
            jsNewError = targetJavascriptLibrary->CreateWebAssemblyRuntimeError();
        case kjstWebAssemblyLinkError:
            jsNewError = targetJavascriptLibrary->CreateWebAssemblyLinkError();

        case kjstCustomError:
        default:
            AssertMsg(false, "Unhandled error type?");
            break;
        }
        return jsNewError;
    }

    JavascriptError* JavascriptError::CloneErrorMsgAndNumber(JavascriptLibrary* targetJavascriptLibrary)
    {LOGMEIN("JavascriptError.cpp] 804\n");
        JavascriptError* jsNewError = this->CreateNewErrorOfSameType(targetJavascriptLibrary);
        if (jsNewError)
        {LOGMEIN("JavascriptError.cpp] 807\n");
            LPCWSTR msg = nullptr;
            HRESULT hr = JavascriptError::GetRuntimeError(this, &msg);
            jsNewError->SetErrorMessageProperties(jsNewError, hr, msg, targetJavascriptLibrary->GetScriptContext());
        }
        return jsNewError;
    }

    void JavascriptError::TryThrowTypeError(ScriptContext * checkScriptContext, ScriptContext * scriptContext, int32 hCode, PCWSTR varName)
    {LOGMEIN("JavascriptError.cpp] 816\n");
        // Don't throw if implicit exceptions are disabled
        if (checkScriptContext->GetThreadContext()->RecordImplicitException())
        {
            ThrowTypeError(scriptContext, hCode, varName);
        }
    }

    JavascriptError* JavascriptError::CreateFromCompileScriptException(ScriptContext* scriptContext, CompileScriptException* cse)
    {LOGMEIN("JavascriptError.cpp] 825\n");
        HRESULT hr = cse->ei.scode;
        Js::JavascriptError * error = Js::JavascriptError::MapParseError(scriptContext, hr);
        const Js::PropertyRecord *record;
        Var value;

        if (cse->ei.bstrDescription)
        {LOGMEIN("JavascriptError.cpp] 832\n");
            value = JavascriptString::NewCopySz(cse->ei.bstrDescription, scriptContext);
            JavascriptOperators::OP_SetProperty(error, PropertyIds::message, value, scriptContext);
        }

        if (cse->hasLineNumberInfo)
        {LOGMEIN("JavascriptError.cpp] 838\n");
            value = JavascriptNumber::New(cse->line, scriptContext);
            scriptContext->GetOrAddPropertyRecord(_u("line"), &record);
            JavascriptOperators::OP_SetProperty(error, record->GetPropertyId(), value, scriptContext);
        }

        if (cse->hasLineNumberInfo)
        {LOGMEIN("JavascriptError.cpp] 845\n");
            value = JavascriptNumber::New(cse->ichMin - cse->ichMinLine, scriptContext);
            scriptContext->GetOrAddPropertyRecord(_u("column"), &record);
            JavascriptOperators::OP_SetProperty(error, record->GetPropertyId(), value, scriptContext);
        }

        if (cse->hasLineNumberInfo)
        {LOGMEIN("JavascriptError.cpp] 852\n");
            value = JavascriptNumber::New(cse->ichLim - cse->ichMin, scriptContext);
            JavascriptOperators::OP_SetProperty(error, PropertyIds::length, value, scriptContext);
        }

        if (cse->bstrLine != nullptr)
        {LOGMEIN("JavascriptError.cpp] 858\n");
            value = JavascriptString::NewCopySz(cse->bstrLine, scriptContext);
            JavascriptOperators::OP_SetProperty(error, PropertyIds::source, value, scriptContext);
        }
        return error;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptError::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptError.cpp] 867\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapErrorObject;
    }

    void JavascriptError::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptError.cpp] 872\n");
        //
        //TODO: we don't capture the details of the error right now (and just create a generic one on inflate) so we need to fix this eventually
        //

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapErrorObject>(objData, nullptr);
    }
#endif
}
