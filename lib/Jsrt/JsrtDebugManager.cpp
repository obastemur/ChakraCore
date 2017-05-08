//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "JsrtPch.h"
#include "JsrtDebugManager.h"
#include "JsrtDebugEventObject.h"
#include "JsrtDebugUtils.h"
#include "JsrtDebuggerObject.h"
#include "RuntimeDebugPch.h"
#include "screrror.h"   // For CompileScriptException

JsrtDebugManager::JsrtDebugManager(ThreadContext* threadContext) :
    HostDebugContext(nullptr),
    threadContext(threadContext),
    debugEventCallback(nullptr),
    callbackState(nullptr),
    resumeAction(BREAKRESUMEACTION_CONTINUE),
    debugObjectArena(nullptr),
    debuggerObjectsManager(nullptr),
    debugDocumentManager(nullptr),
    stackFrames(nullptr),
    breakOnExceptionAttributes(JsDiagBreakOnExceptionAttributeUncaught)
{TRACE_IT(28052);
    Assert(threadContext != nullptr);
}

JsrtDebugManager::~JsrtDebugManager()
{TRACE_IT(28053);
    if (this->debuggerObjectsManager != nullptr)
    {TRACE_IT(28054);
        Adelete(this->debugObjectArena, this->debuggerObjectsManager);
        this->debuggerObjectsManager = nullptr;
    }

    if (this->debugDocumentManager != nullptr)
    {TRACE_IT(28055);
        Adelete(this->debugObjectArena, this->debugDocumentManager);
        this->debugDocumentManager = nullptr;
    }

    if (this->debugObjectArena != nullptr)
    {TRACE_IT(28056);
        this->threadContext->GetRecycler()->UnregisterExternalGuestArena(this->debugObjectArena);
        HeapDelete(this->debugObjectArena);
        this->debugObjectArena = nullptr;
    }

    this->debugEventCallback = nullptr;
    this->callbackState = nullptr;
    this->threadContext = nullptr;
}

void JsrtDebugManager::SetDebugEventCallback(JsDiagDebugEventCallback debugEventCallback, void* callbackState)
{TRACE_IT(28057);
    Assert(this->debugEventCallback == nullptr);
    Assert(this->callbackState == nullptr);

    this->debugEventCallback = debugEventCallback;
    this->callbackState = callbackState;
}

void* JsrtDebugManager::GetAndClearCallbackState()
{TRACE_IT(28058);
    void* currentCallbackState = this->callbackState;

    this->debugEventCallback = nullptr;
    this->callbackState = nullptr;

    return currentCallbackState;
}

bool JsrtDebugManager::IsDebugEventCallbackSet() const
{TRACE_IT(28059);
    return this->debugEventCallback != nullptr;
}

bool JsrtDebugManager::CanHalt(Js::InterpreterHaltState* haltState)
{TRACE_IT(28060);
    // This is registered as the callback for inline breakpoints.
    // We decide here if we are at a reasonable stop location that has source code.
    Assert(haltState->IsValid());

    Js::FunctionBody* pCurrentFuncBody = haltState->GetFunction();
    int byteOffset = haltState->GetCurrentOffset();
    Js::FunctionBody::StatementMap* map = pCurrentFuncBody->GetMatchingStatementMapFromByteCode(byteOffset, false);

    // Resolve the dummy ret code.
    return map != nullptr && (!pCurrentFuncBody->GetIsGlobalFunc() || !Js::FunctionBody::IsDummyGlobalRetStatement(&map->sourceSpan));
}

void JsrtDebugManager::DispatchHalt(Js::InterpreterHaltState* haltState)
{TRACE_IT(28061);
    switch (haltState->stopType)
    {
    case Js::STOP_BREAKPOINT: /*JsDiagDebugEventBreakpoint*/
    case Js::STOP_INLINEBREAKPOINT: /*JsDiagDebugEventDebuggerStatement*/
    case Js::STOP_ASYNCBREAK: /*JsDiagDebugEventAsyncBreak*/
        this->ReportBreak(haltState);
        break;
    case Js::STOP_STEPCOMPLETE: /*JsDiagDebugEventStepComplete*/
        this->SetResumeType(BREAKRESUMEACTION_CONTINUE);
        this->ReportBreak(haltState);
        break;
    case Js::STOP_EXCEPTIONTHROW: /*JsDiagDebugEventRuntimeException*/
        this->ReportExceptionBreak(haltState);
        break;
    case Js::STOP_MUTATIONBREAKPOINT:
        AssertMsg(false, "Not yet handled");
        break;
    default:
        AssertMsg(false, "Unhandled stop type");
    }

    this->HandleResume(haltState, this->resumeAction);
}

bool JsrtDebugManager::CanAllowBreakpoints()
{TRACE_IT(28062);
    return true;
}

void JsrtDebugManager::CleanupHalt()
{TRACE_IT(28063);
}

bool JsrtDebugManager::IsInClosedState()
{TRACE_IT(28064);
    return this->debugEventCallback == nullptr;
}

bool JsrtDebugManager::IsExceptionReportingEnabled()
{TRACE_IT(28065);
    return this->GetBreakOnException() != JsDiagBreakOnExceptionAttributeNone;
}

bool JsrtDebugManager::IsFirstChanceExceptionEnabled()
{TRACE_IT(28066);
    return (this->GetBreakOnException() & JsDiagBreakOnExceptionAttributeFirstChance) == JsDiagBreakOnExceptionAttributeFirstChance;
}

HRESULT JsrtDebugManager::DbgRegisterFunction(Js::ScriptContext* scriptContext, Js::FunctionBody* functionBody, DWORD_PTR dwDebugSourceContext, LPCWSTR title)
{TRACE_IT(28067);
    Js::Utf8SourceInfo* utf8SourceInfo = functionBody->GetUtf8SourceInfo();

    if (!utf8SourceInfo->GetIsLibraryCode() && !utf8SourceInfo->HasDebugDocument())
    {TRACE_IT(28068);
        JsrtDebugDocumentManager* debugDocumentManager = this->GetDebugDocumentManager();
        Assert(debugDocumentManager != nullptr);

        Js::DebugDocument* debugDocument = HeapNewNoThrow(Js::DebugDocument, utf8SourceInfo, functionBody);
        if (debugDocument != nullptr)
        {TRACE_IT(28069);
            utf8SourceInfo->SetDebugDocument(debugDocument);
        }
    }

    return S_OK;
}

void JsrtDebugManager::ReportScriptCompile(Js::JavascriptFunction* scriptFunction, Js::Utf8SourceInfo* utf8SourceInfo, CompileScriptException* compileException)
{TRACE_IT(28070);
    if (this->debugEventCallback != nullptr)
    {TRACE_IT(28071);
        Js::ScriptContext* scriptContext = utf8SourceInfo->GetScriptContext();

        JsrtDebugEventObject debugEventObject(scriptContext);

        Js::DynamicObject* eventDataObject = debugEventObject.GetEventDataObject();

        JsrtDebugUtils::AddFileNameOrScriptTypeToObject(eventDataObject, utf8SourceInfo);
        JsrtDebugUtils::AddLineCountToObject(eventDataObject, utf8SourceInfo);
        JsrtDebugUtils::AddPropertyToObject(eventDataObject, JsrtDebugPropertyId::sourceLength, utf8SourceInfo->GetCchLength(), utf8SourceInfo->GetScriptContext());

        JsDiagDebugEvent jsDiagDebugEvent = JsDiagDebugEventCompileError;

        if (scriptFunction == nullptr)
        {TRACE_IT(28072);
            // Report JsDiagDebugEventCompileError event
            JsrtDebugUtils::AddPropertyToObject(eventDataObject, JsrtDebugPropertyId::error, compileException->ei.bstrDescription, ::SysStringLen(compileException->ei.bstrDescription), scriptContext);
            JsrtDebugUtils::AddPropertyToObject(eventDataObject, JsrtDebugPropertyId::line, compileException->line, scriptContext);
            JsrtDebugUtils::AddPropertyToObject(eventDataObject, JsrtDebugPropertyId::column, compileException->ichMin - compileException->ichMinLine - 1, scriptContext); // Converted to 0-based
            JsrtDebugUtils::AddPropertyToObject(eventDataObject, JsrtDebugPropertyId::sourceText, compileException->bstrLine, ::SysStringLen(compileException->bstrLine), scriptContext);
        }
        else
        {TRACE_IT(28073);
            JsrtDebugDocumentManager* debugDocumentManager = this->GetDebugDocumentManager();
            Assert(debugDocumentManager != nullptr);

            // Create DebugDocument and then report JsDiagDebugEventSourceCompile event
            Js::DebugDocument* debugDocument = HeapNewNoThrow(Js::DebugDocument, utf8SourceInfo, scriptFunction->GetFunctionBody());
            if (debugDocument != nullptr)
            {TRACE_IT(28074);
                utf8SourceInfo->SetDebugDocument(debugDocument);

                // Only add scriptId if everything is ok as scriptId is used for other operations
                JsrtDebugUtils::AddScriptIdToObject(eventDataObject, utf8SourceInfo);
            }
            jsDiagDebugEvent = JsDiagDebugEventSourceCompile;
        }

        this->CallDebugEventCallback(jsDiagDebugEvent, eventDataObject, scriptContext, false /*isBreak*/);
    }
}

void JsrtDebugManager::ReportBreak(Js::InterpreterHaltState* haltState)
{TRACE_IT(28075);
    if (this->debugEventCallback != nullptr)
    {TRACE_IT(28076);
        Js::FunctionBody* functionBody = haltState->GetFunction();
        Assert(functionBody != nullptr);

        Js::Utf8SourceInfo* utf8SourceInfo = functionBody->GetUtf8SourceInfo();
        int currentByteCodeOffset = haltState->GetCurrentOffset();
        Js::ScriptContext* scriptContext = utf8SourceInfo->GetScriptContext();

        JsDiagDebugEvent jsDiagDebugEvent = this->GetDebugEventFromStopType(haltState->stopType);

        JsrtDebugEventObject debugEventObject(scriptContext);

        Js::DynamicObject* eventDataObject = debugEventObject.GetEventDataObject();

        Js::ProbeContainer* probeContainer = scriptContext->GetDebugContext()->GetProbeContainer();

        if (jsDiagDebugEvent == JsDiagDebugEventBreakpoint)
        {TRACE_IT(28077);
            UINT bpId = 0;
            probeContainer->MapProbesUntil([&](int i, Js::Probe* pProbe)
            {
                Js::BreakpointProbe* bp = (Js::BreakpointProbe*)pProbe;
                if (bp->Matches(functionBody, utf8SourceInfo->GetDebugDocument(), currentByteCodeOffset))
                {TRACE_IT(28078);
                    bpId = bp->GetId();
                    return true;
                }
                return false;
            });

            AssertMsg(bpId != 0, "How come we don't have a breakpoint id for JsDiagDebugEventBreakpoint");

            JsrtDebugUtils::AddPropertyToObject(eventDataObject, JsrtDebugPropertyId::breakpointId, bpId, scriptContext);
        }

        JsrtDebugUtils::AddScriptIdToObject(eventDataObject, utf8SourceInfo);
        JsrtDebugUtils::AddLineColumnToObject(eventDataObject, functionBody, currentByteCodeOffset);
        JsrtDebugUtils::AddSourceLengthAndTextToObject(eventDataObject, functionBody, currentByteCodeOffset);

        this->CallDebugEventCallbackForBreak(jsDiagDebugEvent, eventDataObject, scriptContext);
    }
}

void JsrtDebugManager::ReportExceptionBreak(Js::InterpreterHaltState* haltState)
{TRACE_IT(28079);
    if (this->debugEventCallback != nullptr)
    {TRACE_IT(28080);
        Assert(haltState->stopType == Js::STOP_EXCEPTIONTHROW);

        Js::Utf8SourceInfo* utf8SourceInfo = haltState->GetFunction()->GetUtf8SourceInfo();
        Js::ScriptContext* scriptContext = utf8SourceInfo->GetScriptContext();

        JsDiagDebugEvent jsDiagDebugEvent = JsDiagDebugEventRuntimeException;

        JsrtDebugEventObject debugEventObject(scriptContext);

        Js::DynamicObject* eventDataObject = debugEventObject.GetEventDataObject();

        JsrtDebugUtils::AddScriptIdToObject(eventDataObject, utf8SourceInfo);

        Js::FunctionBody* functionBody = haltState->topFrame->GetFunction();

        Assert(functionBody != nullptr);
        
        int currentByteCodeOffset = haltState->topFrame->GetByteCodeOffset();
        JsrtDebugUtils::AddLineColumnToObject(eventDataObject, functionBody, currentByteCodeOffset);
        JsrtDebugUtils::AddSourceLengthAndTextToObject(eventDataObject, functionBody, currentByteCodeOffset);
        JsrtDebugUtils::AddPropertyToObject(eventDataObject, JsrtDebugPropertyId::uncaught, !haltState->exceptionObject->IsFirstChanceException(), scriptContext);

        Js::ResolvedObject resolvedObject;
        resolvedObject.scriptContext = scriptContext;
        resolvedObject.name = _u("{exception}");
        resolvedObject.typeId = Js::TypeIds_Error;
        resolvedObject.address = nullptr;
        resolvedObject.obj = scriptContext->GetDebugContext()->GetProbeContainer()->GetExceptionObject();

        if (resolvedObject.obj == nullptr)
        {TRACE_IT(28081);
            resolvedObject.obj = resolvedObject.scriptContext->GetLibrary()->GetUndefined();
        }

        JsrtDebuggerObjectBase::CreateDebuggerObject<JsrtDebuggerObjectProperty>(this->GetDebuggerObjectsManager(), resolvedObject, scriptContext, [&](Js::Var marshaledObj)
        {
            JsrtDebugUtils::AddPropertyToObject(eventDataObject, JsrtDebugPropertyId::exception, marshaledObj, scriptContext);
        });
        
        this->CallDebugEventCallbackForBreak(jsDiagDebugEvent, eventDataObject, scriptContext);
    }
}

void JsrtDebugManager::HandleResume(Js::InterpreterHaltState* haltState, BREAKRESUMEACTION resumeAction)
{TRACE_IT(28082);
    Assert(resumeAction != BREAKRESUMEACTION_ABORT);

    Js::ScriptContext* scriptContext = haltState->framePointers->Peek()->GetScriptContext();

    scriptContext->GetThreadContext()->GetDebugManager()->stepController.HandleResumeAction(haltState, resumeAction);
}

void JsrtDebugManager::SetResumeType(BREAKRESUMEACTION resumeAction)
{TRACE_IT(28083);
    this->resumeAction = resumeAction;
}

bool JsrtDebugManager::EnableAsyncBreak(Js::ScriptContext* scriptContext)
{TRACE_IT(28084);
    // This can be called when we are already at break
    if (!scriptContext->GetDebugContext()->GetProbeContainer()->IsAsyncActivate())
    {TRACE_IT(28085);
        scriptContext->GetDebugContext()->GetProbeContainer()->AsyncActivate(this);
        if (Js::Configuration::Global.EnableJitInDebugMode())
        {TRACE_IT(28086);
            scriptContext->GetThreadContext()->GetDebugManager()->GetDebuggingFlags()->SetForceInterpreter(true);
        }
        return true;
    }
    return false;
}

void JsrtDebugManager::CallDebugEventCallback(JsDiagDebugEvent debugEvent, Js::DynamicObject* eventDataObject, Js::ScriptContext* scriptContext, bool isBreak)
{TRACE_IT(28087);
    class AutoClear
    {
    public:
        AutoClear(JsrtDebugManager* jsrtDebug, void* dispatchHaltFrameAddress)
        {TRACE_IT(28088);
            this->jsrtDebugManager = jsrtDebug;
            this->jsrtDebugManager->GetThreadContext()->GetDebugManager()->SetDispatchHaltFrameAddress(dispatchHaltFrameAddress);
        }

        ~AutoClear()
        {TRACE_IT(28089);
            if (jsrtDebugManager->debuggerObjectsManager != nullptr)
            {TRACE_IT(28090);
                jsrtDebugManager->GetDebuggerObjectsManager()->ClearAll();
            }

            if (jsrtDebugManager->stackFrames != nullptr)
            {TRACE_IT(28091);
                Adelete(jsrtDebugManager->GetDebugObjectArena(), jsrtDebugManager->stackFrames);
                jsrtDebugManager->stackFrames = nullptr;
            }
            this->jsrtDebugManager->GetThreadContext()->GetDebugManager()->SetDispatchHaltFrameAddress(nullptr);
            this->jsrtDebugManager = nullptr;
        }
    private:
        JsrtDebugManager* jsrtDebugManager;
    };

    auto funcPtr = [&]()
    {
        if (isBreak)
        {TRACE_IT(28092);
            void *frameAddress = _AddressOfReturnAddress();
            // If we are reporting break we should clear all objects after call returns

            // Save the frame address, when asking for stack we will only give stack which is under this address
            // because host can execute javascript after break which should not be part of stack.
            AutoClear autoClear(this, frameAddress);
            this->debugEventCallback(debugEvent, eventDataObject, this->callbackState);
        }
        else
        {TRACE_IT(28093);
            this->debugEventCallback(debugEvent, eventDataObject, this->callbackState);
        }
    };

    if (scriptContext->GetThreadContext()->IsScriptActive())
    {TRACE_IT(28094);
        BEGIN_LEAVE_SCRIPT(scriptContext)
        {TRACE_IT(28095);
            funcPtr();
        }
        END_LEAVE_SCRIPT(scriptContext);
    }
    else
    {TRACE_IT(28096);
        funcPtr();
    }
}

void JsrtDebugManager::CallDebugEventCallbackForBreak(JsDiagDebugEvent debugEvent, Js::DynamicObject* eventDataObject, Js::ScriptContext* scriptContext)
{TRACE_IT(28097);
    AutoSetDispatchHaltFlag autoSetDispatchHaltFlag(scriptContext, scriptContext->GetThreadContext());

    this->CallDebugEventCallback(debugEvent, eventDataObject, scriptContext, true /*isBreak*/);

    for (Js::ScriptContext *tempScriptContext = scriptContext->GetThreadContext()->GetScriptContextList();
    tempScriptContext != nullptr && !tempScriptContext->IsClosed();
        tempScriptContext = tempScriptContext->next)
    {TRACE_IT(28098);
        tempScriptContext->GetDebugContext()->GetProbeContainer()->AsyncDeactivate();
    }

    if (Js::Configuration::Global.EnableJitInDebugMode())
    {TRACE_IT(28099);
        scriptContext->GetThreadContext()->GetDebugManager()->GetDebuggingFlags()->SetForceInterpreter(false);
    }
}

Js::DynamicObject* JsrtDebugManager::GetScript(Js::Utf8SourceInfo* utf8SourceInfo)
{TRACE_IT(28100);
    Js::DynamicObject* scriptObject = utf8SourceInfo->GetScriptContext()->GetLibrary()->CreateObject();

    JsrtDebugUtils::AddScriptIdToObject(scriptObject, utf8SourceInfo);
    JsrtDebugUtils::AddFileNameOrScriptTypeToObject(scriptObject, utf8SourceInfo);
    JsrtDebugUtils::AddLineCountToObject(scriptObject, utf8SourceInfo);
    JsrtDebugUtils::AddPropertyToObject(scriptObject, JsrtDebugPropertyId::sourceLength, utf8SourceInfo->GetCchLength(), utf8SourceInfo->GetScriptContext());

    return scriptObject;
}

Js::JavascriptArray* JsrtDebugManager::GetScripts(Js::ScriptContext* scriptContext)
{TRACE_IT(28101);
    Js::JavascriptArray* scriptsArray = scriptContext->GetLibrary()->CreateArray();

    int index = 0;

    for (Js::ScriptContext *tempScriptContext = scriptContext->GetThreadContext()->GetScriptContextList();
    tempScriptContext != nullptr && !tempScriptContext->IsClosed();
        tempScriptContext = tempScriptContext->next)
    {TRACE_IT(28102);
        tempScriptContext->MapScript([&](Js::Utf8SourceInfo* utf8SourceInfo)
        {
            if (!utf8SourceInfo->GetIsLibraryCode() && utf8SourceInfo->HasDebugDocument())
            {TRACE_IT(28103);
                bool isCallerLibraryCode = false;

                bool isDynamic = utf8SourceInfo->IsDynamic();

                if (isDynamic)
                {TRACE_IT(28104);
                    // If the code is dynamic (eval or new Function) only return the script if parent is non-library
                    Js::Utf8SourceInfo* callerUtf8SourceInfo = utf8SourceInfo->GetCallerUtf8SourceInfo();

                    while (callerUtf8SourceInfo != nullptr && !isCallerLibraryCode)
                    {TRACE_IT(28105);
                        isCallerLibraryCode = callerUtf8SourceInfo->GetIsLibraryCode();
                        callerUtf8SourceInfo = callerUtf8SourceInfo->GetCallerUtf8SourceInfo();
                    }
                }

                if (!isCallerLibraryCode)
                {TRACE_IT(28106);
                    Js::DynamicObject* sourceObj = this->GetScript(utf8SourceInfo);

                    if (sourceObj != nullptr)
                    {TRACE_IT(28107);
                        Js::Var marshaledObj = Js::CrossSite::MarshalVar(scriptContext, sourceObj);
                        scriptsArray->DirectSetItemAt(index, marshaledObj);
                        index++;
                    }
                }
            }
        });
    }

    return scriptsArray;
}

Js::DynamicObject* JsrtDebugManager::GetSource(Js::ScriptContext* scriptContext, uint scriptId)
{TRACE_IT(28108);
    Js::Utf8SourceInfo* utf8SourceInfo = nullptr;

    for (Js::ScriptContext *tempScriptContext = this->threadContext->GetScriptContextList();
    tempScriptContext != nullptr && utf8SourceInfo == nullptr && !tempScriptContext->IsClosed();
        tempScriptContext = tempScriptContext->next)
    {TRACE_IT(28109);
        tempScriptContext->MapScript([&](Js::Utf8SourceInfo* sourceInfo) -> bool
        {
            if (sourceInfo->IsInDebugMode() && sourceInfo->GetSourceInfoId() == scriptId)
            {TRACE_IT(28110);
                utf8SourceInfo = sourceInfo;
                return true;
            }
            return false;
        });
    }

    Js::DynamicObject* sourceObject = nullptr;

    if (utf8SourceInfo != nullptr)
    {TRACE_IT(28111);
        sourceObject = (Js::DynamicObject*)Js::CrossSite::MarshalVar(utf8SourceInfo->GetScriptContext(), scriptContext->GetLibrary()->CreateObject());

        JsrtDebugUtils::AddScriptIdToObject(sourceObject, utf8SourceInfo);
        JsrtDebugUtils::AddFileNameOrScriptTypeToObject(sourceObject, utf8SourceInfo);
        JsrtDebugUtils::AddLineCountToObject(sourceObject, utf8SourceInfo);
        JsrtDebugUtils::AddPropertyToObject(sourceObject, JsrtDebugPropertyId::sourceLength, utf8SourceInfo->GetCchLength(), utf8SourceInfo->GetScriptContext());
        JsrtDebugUtils::AddSouceToObject(sourceObject, utf8SourceInfo);
    }

    return sourceObject;
}

Js::JavascriptArray* JsrtDebugManager::GetStackFrames(Js::ScriptContext* scriptContext)
{TRACE_IT(28112);
    if (this->stackFrames == nullptr)
    {TRACE_IT(28113);
        this->stackFrames = Anew(this->GetDebugObjectArena(), JsrtDebugStackFrames, this);
    }

    return this->stackFrames->StackFrames(scriptContext);
}

bool JsrtDebugManager::TryGetFrameObjectFromFrameIndex(Js::ScriptContext *scriptContext, uint frameIndex, JsrtDebuggerStackFrame ** debuggerStackFrame)
{TRACE_IT(28114);
    if (this->stackFrames == nullptr)
    {TRACE_IT(28115);
        this->GetStackFrames(scriptContext);
    }

    return this->stackFrames->TryGetFrameObjectFromFrameIndex(frameIndex, debuggerStackFrame);
}

Js::DynamicObject* JsrtDebugManager::SetBreakPoint(Js::ScriptContext* scriptContext, Js::Utf8SourceInfo* utf8SourceInfo, UINT lineNumber, UINT columnNumber)
{TRACE_IT(28116);
    Js::DebugDocument* debugDocument = utf8SourceInfo->GetDebugDocument();
    if (debugDocument != nullptr && SUCCEEDED(utf8SourceInfo->EnsureLineOffsetCacheNoThrow()) && lineNumber < utf8SourceInfo->GetLineCount())
    {TRACE_IT(28117);
        charcount_t charPosition = 0;
        charcount_t byteOffset = 0;
        utf8SourceInfo->GetCharPositionForLineInfo((charcount_t)lineNumber, &charPosition, &byteOffset);
        long ibos = charPosition + columnNumber + 1;

        Js::StatementLocation statement;
        if (!debugDocument->GetStatementLocation(ibos, &statement))
        {TRACE_IT(28118);
            return nullptr;
        }

        // Don't see a use case for supporting multiple breakpoints at same location.
        // If a breakpoint already exists, just return that
        Js::BreakpointProbe* probe = debugDocument->FindBreakpoint(statement);
        if (probe == nullptr)
        {TRACE_IT(28119);
            probe = debugDocument->SetBreakPoint(statement, BREAKPOINT_ENABLED);

            if(probe == nullptr)
            {TRACE_IT(28120);
                return nullptr;
            }

            this->GetDebugDocumentManager()->AddDocument(probe->GetId(), debugDocument);
        }

        probe->GetStatementLocation(&statement);

        Js::DynamicObject* bpObject = (Js::DynamicObject*)Js::CrossSite::MarshalVar(debugDocument->GetUtf8SourceInfo()->GetScriptContext(), scriptContext->GetLibrary()->CreateObject());

        JsrtDebugUtils::AddPropertyToObject(bpObject, JsrtDebugPropertyId::breakpointId, probe->GetId(), scriptContext);
        JsrtDebugUtils::AddLineColumnToObject(bpObject, statement.function, statement.bytecodeSpan.begin);
        JsrtDebugUtils::AddScriptIdToObject(bpObject, utf8SourceInfo);

        return bpObject;
    }

    return nullptr;
}

void JsrtDebugManager::GetBreakpoints(Js::JavascriptArray** bpsArray, Js::ScriptContext* scriptContext)
{TRACE_IT(28121);
    Js::ScriptContext* arrayScriptContext = (*bpsArray)->GetScriptContext();
    Js::ProbeContainer* probeContainer = scriptContext->GetDebugContext()->GetProbeContainer();

    probeContainer->MapProbes([&](int i, Js::Probe* pProbe)
    {
        Js::BreakpointProbe* bp = (Js::BreakpointProbe*)pProbe;
        Js::DynamicObject* bpObject = scriptContext->GetLibrary()->CreateObject();

        JsrtDebugUtils::AddPropertyToObject(bpObject, JsrtDebugPropertyId::breakpointId, bp->GetId(), scriptContext);
        JsrtDebugUtils::AddLineColumnToObject(bpObject, bp->GetFunctionBody(), bp->GetBytecodeOffset());

        Js::Utf8SourceInfo* utf8SourceInfo = bp->GetDbugDocument()->GetUtf8SourceInfo();
        JsrtDebugUtils::AddScriptIdToObject(bpObject, utf8SourceInfo);

        Js::Var marshaledObj = Js::CrossSite::MarshalVar(arrayScriptContext, bpObject);
        Js::JavascriptOperators::OP_SetElementI((Js::Var)(*bpsArray), Js::JavascriptNumber::ToVar((*bpsArray)->GetLength(), arrayScriptContext), marshaledObj, arrayScriptContext);
    });
}

#if ENABLE_TTD
Js::BreakpointProbe* JsrtDebugManager::SetBreakpointHelper_TTD(Js::ScriptContext* scriptContext, Js::Utf8SourceInfo* utf8SourceInfo, UINT lineNumber, UINT columnNumber, bool* isNewBP)
{TRACE_IT(28122);
    *isNewBP = false;
    Js::DebugDocument* debugDocument = utf8SourceInfo->GetDebugDocument();
    if(debugDocument != nullptr && SUCCEEDED(utf8SourceInfo->EnsureLineOffsetCacheNoThrow()) && lineNumber < utf8SourceInfo->GetLineCount())
    {TRACE_IT(28123);
        charcount_t charPosition = 0;
        charcount_t byteOffset = 0;
        utf8SourceInfo->GetCharPositionForLineInfo((charcount_t)lineNumber, &charPosition, &byteOffset);
        long ibos = charPosition + columnNumber + 1;

        Js::StatementLocation statement;
        if(!debugDocument->GetStatementLocation(ibos, &statement))
        {TRACE_IT(28124);
            return nullptr;
        }

        // Don't see a use case for supporting multiple breakpoints at same location.
        // If a breakpoint already exists, just return that
        Js::BreakpointProbe* probe = debugDocument->FindBreakpoint(statement);
        if(probe == nullptr)
        {TRACE_IT(28125);
            probe = debugDocument->SetBreakPoint(statement, BREAKPOINT_ENABLED);

            if(probe == nullptr)
            {TRACE_IT(28126);
                return nullptr;
            }

            *isNewBP = true;
            this->GetDebugDocumentManager()->AddDocument(probe->GetId(), debugDocument);
        }

        return probe;
    }

    return nullptr;
}
#endif

JsrtDebuggerObjectsManager* JsrtDebugManager::GetDebuggerObjectsManager()
{TRACE_IT(28127);
    if (this->debuggerObjectsManager == nullptr)
    {TRACE_IT(28128);
        this->debuggerObjectsManager = Anew(this->GetDebugObjectArena(), JsrtDebuggerObjectsManager, this);
    }
    return this->debuggerObjectsManager;
}

void JsrtDebugManager::ClearDebuggerObjects()
{TRACE_IT(28129);
    if (this->debuggerObjectsManager != nullptr)
    {TRACE_IT(28130);
        this->debuggerObjectsManager->ClearAll();
    }
}

ArenaAllocator* JsrtDebugManager::GetDebugObjectArena()
{TRACE_IT(28131);
    if (this->debugObjectArena == nullptr)
    {TRACE_IT(28132);
        this->debugObjectArena = HeapNew(ArenaAllocator, _u("DebugObjectArena"), this->threadContext->GetPageAllocator(), Js::Throw::OutOfMemory);

        this->threadContext->GetRecycler()->RegisterExternalGuestArena(this->debugObjectArena);
    }

    return this->debugObjectArena;
}

JsrtDebugDocumentManager* JsrtDebugManager::GetDebugDocumentManager()
{TRACE_IT(28133);
    if (this->debugDocumentManager == nullptr)
    {TRACE_IT(28134);
        this->debugDocumentManager = Anew(this->GetDebugObjectArena(), JsrtDebugDocumentManager, this);
    }
    return this->debugDocumentManager;
}

void JsrtDebugManager::ClearDebugDocument(Js::ScriptContext* scriptContext)
{TRACE_IT(28135);
    if (this->debugDocumentManager != nullptr)
    {TRACE_IT(28136);
        this->debugDocumentManager->ClearDebugDocument(scriptContext);
    }
}

void JsrtDebugManager::ClearBreakpointDebugDocumentDictionary()
{TRACE_IT(28137);
    if (this->debugDocumentManager != nullptr)
    {TRACE_IT(28138);
        this->debugDocumentManager->ClearBreakpointDebugDocumentDictionary();
    }
}

bool JsrtDebugManager::RemoveBreakpoint(UINT breakpointId)
{TRACE_IT(28139);
    if (this->debugDocumentManager != nullptr)
    {TRACE_IT(28140);
        return this->GetDebugDocumentManager()->RemoveBreakpoint(breakpointId);
    }

    return false;
}

void JsrtDebugManager::SetBreakOnException(JsDiagBreakOnExceptionAttributes exceptionAttributes)
{TRACE_IT(28141);
    this->breakOnExceptionAttributes = exceptionAttributes;
}

JsDiagBreakOnExceptionAttributes JsrtDebugManager::GetBreakOnException()
{TRACE_IT(28142);
    return this->breakOnExceptionAttributes;
}

JsDiagDebugEvent JsrtDebugManager::GetDebugEventFromStopType(Js::StopType stopType)
{TRACE_IT(28143);
    switch (stopType)
    {
    case Js::STOP_BREAKPOINT: return JsDiagDebugEventBreakpoint;
    case Js::STOP_INLINEBREAKPOINT: return JsDiagDebugEventDebuggerStatement;
    case Js::STOP_STEPCOMPLETE: return JsDiagDebugEventStepComplete;
    case Js::STOP_EXCEPTIONTHROW: return JsDiagDebugEventRuntimeException;
    case Js::STOP_ASYNCBREAK: return JsDiagDebugEventAsyncBreak;

    case Js::STOP_MUTATIONBREAKPOINT:
    default:
        Assert("Unhandled stoptype");
        break;
    }

    return JsDiagDebugEventBreakpoint;
}
