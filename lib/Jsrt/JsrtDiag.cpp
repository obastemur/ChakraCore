//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "JsrtPch.h"
#include "JsrtInternal.h"
#include "RuntimeDebugPch.h"
#include "ThreadContextTlsEntry.h"
#include "JsrtDebugUtils.h"
#include "Codex/Utf8Helper.h"

#define VALIDATE_IS_DEBUGGING(jsrtDebugManager) \
    if (jsrtDebugManager == nullptr || !jsrtDebugManager->IsDebugEventCallbackSet()) \
    {TRACE_IT(28314); \
        return JsErrorDiagNotInDebugMode; \
    }

#define VALIDATE_RUNTIME_IS_AT_BREAK(runtime) \
    if (runtime->GetThreadContext()->GetDebugManager() == nullptr || !runtime->GetThreadContext()->GetDebugManager()->IsAtDispatchHalt()) \
    {TRACE_IT(28315); \
        return JsErrorDiagNotAtBreak; \
    }

#define VALIDATE_RUNTIME_STATE_FOR_START_STOP_DEBUGGING(threadContext) \
    if (threadContext->GetRecycler() && threadContext->GetRecycler()->IsHeapEnumInProgress()) \
    {TRACE_IT(28316); \
        return JsErrorHeapEnumInProgress; \
    } \
    else if (threadContext->IsInThreadServiceCallback()) \
    {TRACE_IT(28317); \
        return JsErrorInThreadServiceCallback; \
    } \
    else if (threadContext->IsInScript()) \
    {TRACE_IT(28318); \
        return JsErrorRuntimeInUse; \
    } \
    ThreadContextScope scope(threadContext); \
    if (!scope.IsValid()) \
    {TRACE_IT(28319); \
        return JsErrorWrongThread; \
    }

CHAKRA_API JsDiagStartDebugging(
    _In_ JsRuntimeHandle runtimeHandle,
    _In_ JsDiagDebugEventCallback debugEventCallback,
    _In_opt_ void* callbackState)
{TRACE_IT(28320);
    return GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        PARAM_NOT_NULL(debugEventCallback);

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);
        ThreadContext * threadContext = runtime->GetThreadContext();

        VALIDATE_RUNTIME_STATE_FOR_START_STOP_DEBUGGING(threadContext);

        if (runtime->GetJsrtDebugManager() != nullptr && runtime->GetJsrtDebugManager()->IsDebugEventCallbackSet())
        {TRACE_IT(28321);
            return JsErrorDiagAlreadyInDebugMode;
        }

        // Create the debug object to save callback function and data
        runtime->EnsureJsrtDebugManager();

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        jsrtDebugManager->SetDebugEventCallback(debugEventCallback, callbackState);

        if (threadContext->GetDebugManager() != nullptr)
        {TRACE_IT(28322);
            threadContext->GetDebugManager()->SetLocalsDisplayFlags(Js::DebugManager::LocalsDisplayFlags::LocalsDisplayFlags_NoGroupMethods);
        }

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {TRACE_IT(28323);
            Assert(!scriptContext->IsScriptContextInDebugMode());

            Js::DebugContext* debugContext = scriptContext->GetDebugContext();

            if (debugContext->GetHostDebugContext() == nullptr)
            {TRACE_IT(28324);
                debugContext->SetHostDebugContext(jsrtDebugManager);
            }

            HRESULT hr;
            if (FAILED(hr = scriptContext->OnDebuggerAttached()))
            {TRACE_IT(28325);
                Debugger_AttachDetach_fatal_error(hr); // Inconsistent state, we can't continue from here
                return JsErrorFatal;
            }

            Js::ProbeContainer* probeContainer = debugContext->GetProbeContainer();
            probeContainer->InitializeInlineBreakEngine(jsrtDebugManager);
            probeContainer->InitializeDebuggerScriptOptionCallback(jsrtDebugManager);
        }

        return JsNoError;
    });
}

CHAKRA_API JsDiagStopDebugging(
    _In_ JsRuntimeHandle runtimeHandle,
    _Out_ void** callbackState)
{TRACE_IT(28326);
    return GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        PARAM_NOT_NULL(callbackState);

        *callbackState = nullptr;

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);
        ThreadContext * threadContext = runtime->GetThreadContext();

        VALIDATE_RUNTIME_STATE_FOR_START_STOP_DEBUGGING(threadContext);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {TRACE_IT(28327);
            Assert(scriptContext->IsScriptContextInDebugMode());

            HRESULT hr;
            if (FAILED(hr = scriptContext->OnDebuggerDetached()))
            {TRACE_IT(28328);
                Debugger_AttachDetach_fatal_error(hr); // Inconsistent state, we can't continue from here
                return JsErrorFatal;
            }

            Js::DebugContext* debugContext = scriptContext->GetDebugContext();

            Js::ProbeContainer* probeContainer = debugContext->GetProbeContainer();
            probeContainer->UninstallInlineBreakpointProbe(nullptr);
            probeContainer->UninstallDebuggerScriptOptionCallback();

            jsrtDebugManager->ClearBreakpointDebugDocumentDictionary();
        }

        *callbackState = jsrtDebugManager->GetAndClearCallbackState();

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetScripts(
    _Out_ JsValueRef *scriptsArray)
{TRACE_IT(28329);
    return ContextAPIWrapper_NoRecord<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(scriptsArray);

        *scriptsArray = JS_INVALID_REFERENCE;

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        JsrtDebugManager* jsrtDebugManager = currentContext->GetRuntime()->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        Js::JavascriptArray* scripts = jsrtDebugManager->GetScripts(scriptContext);

        if (scripts != nullptr)
        {TRACE_IT(28330);
            *scriptsArray = scripts;
            return JsNoError;
        }

        return JsErrorDiagUnableToPerformAction;
    });
}

CHAKRA_API JsDiagGetSource(
    _In_ unsigned int scriptId,
    _Out_ JsValueRef *source)
{TRACE_IT(28331);
    return ContextAPIWrapper_NoRecord<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(source);

        *source = JS_INVALID_REFERENCE;

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        JsrtDebugManager* jsrtDebugManager = currentContext->GetRuntime()->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        Js::DynamicObject* sourceObject = jsrtDebugManager->GetSource(scriptContext, scriptId);

        if (sourceObject != nullptr)
        {TRACE_IT(28332);
            *source = sourceObject;
            return JsNoError;
        }

        return JsErrorInvalidArgument;
    });
}

CHAKRA_API JsDiagRequestAsyncBreak(
    _In_ JsRuntimeHandle runtimeHandle)
{TRACE_IT(28333);
    return GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {TRACE_IT(28334);
            jsrtDebugManager->EnableAsyncBreak(scriptContext);
        }

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetBreakpoints(
    _Out_ JsValueRef *breakpoints)
{TRACE_IT(28335);
    return GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {

        PARAM_NOT_NULL(breakpoints);

        *breakpoints = JS_INVALID_REFERENCE;

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        Js::JavascriptArray* bpsArray = currentContext->GetScriptContext()->GetLibrary()->CreateArray();

        JsrtRuntime * runtime = currentContext->GetRuntime();

        ThreadContextScope scope(runtime->GetThreadContext());

        if (!scope.IsValid())
        {TRACE_IT(28336);
            return JsErrorWrongThread;
        }

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {TRACE_IT(28337);
            jsrtDebugManager->GetBreakpoints(&bpsArray, scriptContext);
        }

        *breakpoints = bpsArray;

        return JsNoError;
    });
}

CHAKRA_API JsDiagSetBreakpoint(
    _In_ unsigned int scriptId,
    _In_ unsigned int lineNumber,
    _In_ unsigned int columnNumber,
    _Out_ JsValueRef *breakpoint)
{TRACE_IT(28338);
    return GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {

        PARAM_NOT_NULL(breakpoint);

        *breakpoint = JS_INVALID_REFERENCE;

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        JsrtRuntime * runtime = currentContext->GetRuntime();

        ThreadContextScope scope(runtime->GetThreadContext());

        if (!scope.IsValid())
        {TRACE_IT(28339);
            return JsErrorWrongThread;
        }

        VALIDATE_IS_DEBUGGING(runtime->GetJsrtDebugManager());

        Js::Utf8SourceInfo* utf8SourceInfo = nullptr;

        for (Js::ScriptContext *scriptContext = runtime->GetThreadContext()->GetScriptContextList();
        scriptContext != nullptr && utf8SourceInfo == nullptr && !scriptContext->IsClosed();
            scriptContext = scriptContext->next)
        {TRACE_IT(28340);
            scriptContext->MapScript([&](Js::Utf8SourceInfo* sourceInfo) -> bool
            {
                if (sourceInfo->GetSourceInfoId() == scriptId)
                {TRACE_IT(28341);
                    utf8SourceInfo = sourceInfo;
                    return true;
                }
                return false;
            });
        }

        if (utf8SourceInfo != nullptr && utf8SourceInfo->HasDebugDocument())
        {TRACE_IT(28342);
            JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

            Js::DynamicObject* bpObject = jsrtDebugManager->SetBreakPoint(currentContext->GetScriptContext(), utf8SourceInfo, lineNumber, columnNumber);

            if(bpObject != nullptr)
            {TRACE_IT(28343);
                *breakpoint = bpObject;
                return JsNoError;
            }

            return JsErrorDiagUnableToPerformAction;
        }

        return JsErrorDiagObjectNotFound;
    });
}

CHAKRA_API JsDiagRemoveBreakpoint(
    _In_ unsigned int breakpointId)
{TRACE_IT(28344);
    return GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {

        JsrtContext *currentContext = JsrtContext::GetCurrent();

        JsrtRuntime* runtime = currentContext->GetRuntime();

        ThreadContextScope scope(runtime->GetThreadContext());

        if (!scope.IsValid())
        {TRACE_IT(28345);
            return JsErrorWrongThread;
        }

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        if (!jsrtDebugManager->RemoveBreakpoint(breakpointId))
        {TRACE_IT(28346);
            return JsErrorInvalidArgument;
        }

        return JsNoError;
    });
}

CHAKRA_API JsDiagSetBreakOnException(
    _In_ JsRuntimeHandle runtimeHandle,
    _In_ JsDiagBreakOnExceptionAttributes exceptionAttributes)
{TRACE_IT(28347);
    return GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        jsrtDebugManager->SetBreakOnException(exceptionAttributes);

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetBreakOnException(
    _In_ JsRuntimeHandle runtimeHandle,
    _Out_ JsDiagBreakOnExceptionAttributes* exceptionAttributes)
{TRACE_IT(28348);
    return GlobalAPIWrapper_NoRecord([&]() -> JsErrorCode {

        VALIDATE_INCOMING_RUNTIME_HANDLE(runtimeHandle);

        PARAM_NOT_NULL(exceptionAttributes);

        *exceptionAttributes = JsDiagBreakOnExceptionAttributeNone;

        JsrtRuntime * runtime = JsrtRuntime::FromHandle(runtimeHandle);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        *exceptionAttributes = jsrtDebugManager->GetBreakOnException();

        return JsNoError;
    });
}

CHAKRA_API JsDiagSetStepType(
    _In_ JsDiagStepType stepType)
{TRACE_IT(28349);
    return ContextAPIWrapper_NoRecord<true>([&](Js::ScriptContext * scriptContext) -> JsErrorCode {

        JsrtContext *currentContext = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = currentContext->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        if (stepType == JsDiagStepTypeStepIn)
        {TRACE_IT(28350);
            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_STEP_INTO);
        }
        else if (stepType == JsDiagStepTypeStepOut)
        {TRACE_IT(28351);
            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_STEP_OUT);
        }
        else if (stepType == JsDiagStepTypeStepOver)
        {TRACE_IT(28352);
            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_STEP_OVER);
        }
        else if (stepType == JsDiagStepTypeStepBack)
        {TRACE_IT(28353);
#if ENABLE_TTD
            ThreadContext* threadContext = runtime->GetThreadContext();
            if(!threadContext->IsRuntimeInTTDMode())
            {
                TTDAssert(false, "Must be in replay mode to use reverse-step - launch with \"--replay-debug\" flag in Node.");
                return JsErrorInvalidArgument;
            }

            TTD::TTDebuggerSourceLocation bpLocation;
            threadContext->TTDLog->GetPreviousTimeAndPositionForDebugger(bpLocation);
            threadContext->TTDLog->SetPendingTTDBPInfo(bpLocation);
            threadContext->TTDLog->SetPendingTTDMoveMode(JsTTDMoveMode::JsTTDMoveNone);

            //don't worry about BP suppression because we are just going to throw after we return
            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_CONTINUE);
#else
            return JsErrorInvalidArgument;
#endif
        }
        else if(stepType == JsDiagStepTypeStepReverseContinue)
        {TRACE_IT(28354);
#if ENABLE_TTD
            ThreadContext* threadContext = runtime->GetThreadContext();
            if(!threadContext->IsRuntimeInTTDMode())
            {
                TTDAssert(false, "Must be in replay mode to use reverse-continue - launch with \"--replay-debug\" flag in Node.");
                return JsErrorInvalidArgument;
            }

            TTD::TTDebuggerSourceLocation bpLocation;
            threadContext->TTDLog->GetTimeAndPositionForDebugger(bpLocation);
            threadContext->TTDLog->SetPendingTTDBPInfo(bpLocation);
            threadContext->TTDLog->SetPendingTTDMoveMode(JsTTDMoveMode::JsTTDMoveScanIntervalForContinue);

            //don't worry about BP suppression because we are just going to throw after we return

            jsrtDebugManager->SetResumeType(BREAKRESUMEACTION_CONTINUE);
#else
            return JsErrorInvalidArgument;
#endif
        }

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetFunctionPosition(
    _In_ JsValueRef function,
    _Out_ JsValueRef *functionPosition)
{TRACE_IT(28355);
    return ContextAPIWrapper_NoRecord<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        VALIDATE_INCOMING_REFERENCE(function, scriptContext);
        PARAM_NOT_NULL(functionPosition);

        *functionPosition = JS_INVALID_REFERENCE;

        if (!Js::RecyclableObject::Is(function) || !Js::ScriptFunction::Is(function))
        {TRACE_IT(28356);
            return JsErrorInvalidArgument;
        }

        Js::ScriptFunction* jsFunction = Js::ScriptFunction::FromVar(function);

        Js::FunctionBody* functionBody = jsFunction->GetFunctionBody();
        if (functionBody != nullptr)
        {TRACE_IT(28357);
            Js::Utf8SourceInfo* utf8SourceInfo = functionBody->GetUtf8SourceInfo();
            if (utf8SourceInfo != nullptr && !utf8SourceInfo->GetIsLibraryCode())
            {TRACE_IT(28358);
                ULONG lineNumber = functionBody->GetLineNumber();
                ULONG columnNumber = functionBody->GetColumnNumber();
                uint startOffset = functionBody->GetStatementStartOffset(0);
                ULONG firstStatementLine;
                LONG firstStatementColumn;

                if (functionBody->GetLineCharOffsetFromStartChar(startOffset, &firstStatementLine, &firstStatementColumn))
                {TRACE_IT(28359);
                    Js::DynamicObject* funcPositionObject = (Js::DynamicObject*)Js::CrossSite::MarshalVar(utf8SourceInfo->GetScriptContext(), scriptContext->GetLibrary()->CreateObject());

                    if (funcPositionObject != nullptr)
                    {TRACE_IT(28360);
                        JsrtDebugUtils::AddScriptIdToObject(funcPositionObject, utf8SourceInfo);
                        JsrtDebugUtils::AddFileNameOrScriptTypeToObject(funcPositionObject, utf8SourceInfo);
                        JsrtDebugUtils::AddPropertyToObject(funcPositionObject, JsrtDebugPropertyId::line, (uint32) lineNumber, scriptContext);
                        JsrtDebugUtils::AddPropertyToObject(funcPositionObject, JsrtDebugPropertyId::column, (uint32) columnNumber, scriptContext);
                        JsrtDebugUtils::AddPropertyToObject(funcPositionObject, JsrtDebugPropertyId::firstStatementLine, (uint32) firstStatementLine, scriptContext);
                        JsrtDebugUtils::AddPropertyToObject(funcPositionObject, JsrtDebugPropertyId::firstStatementColumn, (int32) firstStatementColumn, scriptContext);

                        *functionPosition = funcPositionObject;

                        return JsNoError;
                    }
                }
            }
        }

        return JsErrorDiagObjectNotFound;
    });
}

CHAKRA_API JsDiagGetStackTrace(
    _Out_ JsValueRef *stackTrace)
{TRACE_IT(28361);
    return ContextAPIWrapper_NoRecord<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(stackTrace);

        *stackTrace = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        *stackTrace = jsrtDebugManager->GetStackFrames(scriptContext);

        return JsNoError;
    });
}

CHAKRA_API JsDiagGetStackProperties(
    _In_ unsigned int stackFrameIndex,
    _Out_ JsValueRef *properties)
{TRACE_IT(28362);
    return ContextAPIWrapper_NoRecord<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(properties);

        *properties = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        JsrtDebuggerStackFrame* debuggerStackFrame = nullptr;
        if (!jsrtDebugManager->TryGetFrameObjectFromFrameIndex(scriptContext, stackFrameIndex, &debuggerStackFrame))
        {TRACE_IT(28363);
            return JsErrorDiagObjectNotFound;
        }

        Js::DynamicObject* localsObject = debuggerStackFrame->GetLocalsObject(scriptContext);

        if (localsObject != nullptr)
        {TRACE_IT(28364);
            *properties = localsObject;
            return JsNoError;
        }

        return JsErrorDiagUnableToPerformAction;
    });
}

CHAKRA_API JsDiagGetProperties(
    _In_ unsigned int objectHandle,
    _In_ unsigned int fromCount,
    _In_ unsigned int totalCount,
    _Out_ JsValueRef *propertiesObject)
{TRACE_IT(28365);

    return ContextAPIWrapper_NoRecord<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(propertiesObject);

        *propertiesObject = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        JsrtDebuggerObjectBase* debuggerObject = nullptr;
        if (!jsrtDebugManager->GetDebuggerObjectsManager()->TryGetDebuggerObjectFromHandle(objectHandle, &debuggerObject) || debuggerObject == nullptr)
        {TRACE_IT(28366);
            return JsErrorDiagInvalidHandle;
        }

        Js::DynamicObject* properties = debuggerObject->GetChildren(scriptContext, fromCount, totalCount);

        if (properties != nullptr)
        {TRACE_IT(28367);
            *propertiesObject = properties;
            return JsNoError;
        }

        return JsErrorDiagUnableToPerformAction;
    });
}

CHAKRA_API JsDiagGetObjectFromHandle(
    _In_ unsigned int objectHandle,
    _Out_ JsValueRef *handleObject)
{TRACE_IT(28368);
    return ContextAPIWrapper_NoRecord<false>([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(handleObject);

        *handleObject = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        JsrtDebuggerObjectBase* debuggerObject = nullptr;
        if (!jsrtDebugManager->GetDebuggerObjectsManager()->TryGetDebuggerObjectFromHandle(objectHandle, &debuggerObject) || debuggerObject == nullptr)
        {TRACE_IT(28369);
            return JsErrorDiagInvalidHandle;
        }

        Js::DynamicObject* object = debuggerObject->GetJSONObject(scriptContext);

        if (object != nullptr)
        {TRACE_IT(28370);
            *handleObject = object;
            return JsNoError;
        }

        return JsErrorDiagUnableToPerformAction;
    });
}

CHAKRA_API JsDiagEvaluate(
    _In_ JsValueRef expressionVal,
    _In_ unsigned int stackFrameIndex,
    _In_ JsParseScriptAttributes parseAttributes,
    _Out_ JsValueRef *evalResult)
{TRACE_IT(28371);
    return ContextAPINoScriptWrapper_NoRecord([&](Js::ScriptContext *scriptContext) -> JsErrorCode {

        PARAM_NOT_NULL(expressionVal);
        PARAM_NOT_NULL(evalResult);

        bool isArrayBuffer = Js::ArrayBuffer::Is(expressionVal),
             isString = false;
        bool isUtf8   = !(parseAttributes & JsParseScriptAttributeArrayBufferIsUtf16Encoded);

        if (!isArrayBuffer)
        {TRACE_IT(28372);
            isString = Js::JavascriptString::Is(expressionVal);
            if (!isString)
            {TRACE_IT(28373);
                return JsErrorInvalidArgument;
            }
        }

        const size_t len = isArrayBuffer ?
            Js::ArrayBuffer::FromVar(expressionVal)->GetByteLength() :
            Js::JavascriptString::FromVar(expressionVal)->GetLength();

        if (len > INT_MAX)
        {TRACE_IT(28374);
            return JsErrorInvalidArgument;
        }

        const WCHAR* expression;
        utf8::NarrowToWide wide_expression;
        if (isArrayBuffer && isUtf8)
        {TRACE_IT(28375);
            wide_expression.Initialize(
                (const char*)Js::ArrayBuffer::FromVar(expressionVal)->GetBuffer(), len);
            if (!wide_expression)
            {TRACE_IT(28376);
                return JsErrorOutOfMemory;
            }
            expression = wide_expression;
        }
        else
        {TRACE_IT(28377);
            expression = !isArrayBuffer ?
                Js::JavascriptString::FromVar(expressionVal)->GetSz() // String
                :
                (const WCHAR*)Js::ArrayBuffer::FromVar(expressionVal)->GetBuffer(); // ArrayBuffer;
        }

        *evalResult = JS_INVALID_REFERENCE;

        JsrtContext* context = JsrtContext::GetCurrent();
        JsrtRuntime* runtime = context->GetRuntime();

        VALIDATE_RUNTIME_IS_AT_BREAK(runtime);

        JsrtDebugManager* jsrtDebugManager = runtime->GetJsrtDebugManager();

        VALIDATE_IS_DEBUGGING(jsrtDebugManager);

        JsrtDebuggerStackFrame* debuggerStackFrame = nullptr;
        if (!jsrtDebugManager->TryGetFrameObjectFromFrameIndex(scriptContext, stackFrameIndex, &debuggerStackFrame))
        {TRACE_IT(28378);
            return JsErrorDiagObjectNotFound;
        }

        Js::DynamicObject* result = nullptr;
        bool success = debuggerStackFrame->Evaluate(scriptContext, expression, static_cast<int>(len), false, &result);

        if (result != nullptr)
        {TRACE_IT(28379);
            *evalResult = result;
        }

        return success ? JsNoError : JsErrorScriptException;

    }, false /*allowInObjectBeforeCollectCallback*/, true /*scriptExceptionAllowed*/);
}
