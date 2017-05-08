//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

namespace Js
{
    DebugContext::DebugContext(Js::ScriptContext * scriptContext) :
        scriptContext(scriptContext),
        hostDebugContext(nullptr),
        diagProbesContainer(nullptr),
        debuggerMode(DebuggerMode::NotDebugging)
    {TRACE_IT(42115);
        Assert(scriptContext != nullptr);
    }

    DebugContext::~DebugContext()
    {TRACE_IT(42116);
        Assert(this->scriptContext == nullptr);
        Assert(this->hostDebugContext == nullptr);
        Assert(this->diagProbesContainer == nullptr);
    }

    void DebugContext::Initialize()
    {TRACE_IT(42117);
        Assert(this->diagProbesContainer == nullptr);
        this->diagProbesContainer = HeapNew(ProbeContainer);
        this->diagProbesContainer->Initialize(this->scriptContext);
    }

    void DebugContext::Close()
    {TRACE_IT(42118);
        Assert(this->scriptContext != nullptr);
        this->scriptContext = nullptr;

        if (this->diagProbesContainer != nullptr)
        {TRACE_IT(42119);
            this->diagProbesContainer->Close();
            HeapDelete(this->diagProbesContainer);
            this->diagProbesContainer = nullptr;
        }

        if (this->hostDebugContext != nullptr)
        {TRACE_IT(42120);
            this->hostDebugContext->Delete();
            this->hostDebugContext = nullptr;
        }
    }

    void DebugContext::SetHostDebugContext(HostDebugContext * hostDebugContext)
    {TRACE_IT(42121);
        Assert(this->hostDebugContext == nullptr);
        Assert(hostDebugContext != nullptr);

        this->hostDebugContext = hostDebugContext;
    }

    bool DebugContext::CanRegisterFunction() const
    {TRACE_IT(42122);
        if (this->hostDebugContext == nullptr || this->scriptContext == nullptr || this->scriptContext->IsClosed() || this->IsDebugContextInNonDebugMode())
        {TRACE_IT(42123);
            return false;
        }
        return true;
    }

    void DebugContext::RegisterFunction(Js::ParseableFunctionInfo * func, LPCWSTR title)
    {TRACE_IT(42124);
        if (!this->CanRegisterFunction())
        {TRACE_IT(42125);
            return;
        }

        this->RegisterFunction(func, func->GetHostSourceContext(), title);
    }

    void DebugContext::RegisterFunction(Js::ParseableFunctionInfo * func, DWORD_PTR dwDebugSourceContext, LPCWSTR title)
    {TRACE_IT(42126);
        if (!this->CanRegisterFunction())
        {TRACE_IT(42127);
            return;
        }

        FunctionBody * functionBody = nullptr;
        if (func->IsDeferredParseFunction())
        {TRACE_IT(42128);
            HRESULT hr = S_OK;
            Assert(!this->scriptContext->GetThreadContext()->IsScriptActive());

            BEGIN_JS_RUNTIME_CALL_EX_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED(this->scriptContext, false)
            {TRACE_IT(42129);
                functionBody = func->Parse();
            }
            END_JS_RUNTIME_CALL_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr);

            if (FAILED(hr))
            {TRACE_IT(42130);
                return;
            }
        }
        else
        {TRACE_IT(42131);
            functionBody = func->GetFunctionBody();
        }
        this->RegisterFunction(functionBody, dwDebugSourceContext, title);
    }

    void DebugContext::RegisterFunction(Js::FunctionBody * functionBody, DWORD_PTR dwDebugSourceContext, LPCWSTR title)
    {TRACE_IT(42132);
        if (!this->CanRegisterFunction())
        {TRACE_IT(42133);
            return;
        }

        this->hostDebugContext->DbgRegisterFunction(this->scriptContext, functionBody, dwDebugSourceContext, title);
    }

    // Sets the specified mode for the debugger.  The mode is used to inform
    // the runtime of whether or not functions should be JITed or interpreted
    // when they are defer parsed.
    // Note: Transitions back to NotDebugging are not allowed.  Once the debugger
    // is in SourceRundown or Debugging mode, it can only transition between those
    // two modes.
    void DebugContext::SetDebuggerMode(DebuggerMode mode)
    {TRACE_IT(42134);
        if (this->debuggerMode == mode)
        {TRACE_IT(42135);
            // Already in this mode so return.
            return;
        }

        if (mode == DebuggerMode::NotDebugging)
        {
            AssertMsg(false, "Transitioning to non-debug mode is not allowed.");
            return;
        }

        this->debuggerMode = mode;
    }

    HRESULT DebugContext::RundownSourcesAndReparse(bool shouldPerformSourceRundown, bool shouldReparseFunctions)
    {TRACE_IT(42136);
        OUTPUT_TRACE(Js::DebuggerPhase, _u("DebugContext::RundownSourcesAndReparse scriptContext 0x%p, shouldPerformSourceRundown %d, shouldReparseFunctions %d\n"),
            this->scriptContext, shouldPerformSourceRundown, shouldReparseFunctions);

        Js::TempArenaAllocatorObject *tempAllocator = nullptr;
        JsUtil::List<Js::FunctionInfo *, Recycler>* pFunctionsToRegister = nullptr;
        JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer>* utf8SourceInfoList = nullptr;

        HRESULT hr = S_OK;
        ThreadContext* threadContext = this->scriptContext->GetThreadContext();

        BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
        tempAllocator = threadContext->GetTemporaryAllocator(_u("debuggerAlloc"));

        utf8SourceInfoList = JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer>::New(this->scriptContext->GetRecycler());

        this->MapUTF8SourceInfoUntil([&](Js::Utf8SourceInfo * sourceInfo) -> bool
        {
            WalkAndAddUtf8SourceInfo(sourceInfo, utf8SourceInfoList);
            return false;
        });
        END_TRANSLATE_OOM_TO_HRESULT(hr);

        if (hr != S_OK)
        {TRACE_IT(42137);
            Assert(FALSE);
            return hr;
        }

        // Cache ScriptContext as multiple calls below can go out of engine and ScriptContext can be closed which will delete DebugContext
        Js::ScriptContext* cachedScriptContext = this->scriptContext;

        utf8SourceInfoList->MapUntil([&](int index, Js::Utf8SourceInfo * sourceInfo) -> bool
        {
            if (cachedScriptContext->IsClosed())
            {TRACE_IT(42138);
                // ScriptContext could be closed in previous iteration
                hr = E_FAIL;
                return true;
            }

            OUTPUT_TRACE(Js::DebuggerPhase, _u("DebugContext::RundownSourcesAndReparse scriptContext 0x%p, sourceInfo 0x%p, HasDebugDocument %d\n"),
                this->scriptContext, sourceInfo, sourceInfo->HasDebugDocument());

            if (sourceInfo->GetIsLibraryCode())
            {TRACE_IT(42139);
                // Not putting the internal library code to the debug mode, but need to reinitialize execution mode limits of each
                // function body upon debugger detach, even for library code at the moment.
                if (shouldReparseFunctions)
                {TRACE_IT(42140);
                    sourceInfo->MapFunction([](Js::FunctionBody *const pFuncBody)
                    {
                        if (pFuncBody->IsFunctionParsed())
                        {TRACE_IT(42141);
                            pFuncBody->ReinitializeExecutionModeAndLimits();
                        }
                    });
                }
                return false;
            }

            Assert(sourceInfo->GetSrcInfo() && sourceInfo->GetSrcInfo()->sourceContextInfo);

#if DBG
            if (shouldPerformSourceRundown)
            {TRACE_IT(42142);
                // We shouldn't have a debug document if we're running source rundown for the first time.
                Assert(!sourceInfo->HasDebugDocument());
            }
#endif // DBG

            DWORD_PTR dwDebugHostSourceContext = Js::Constants::NoHostSourceContext;

            if (shouldPerformSourceRundown && this->hostDebugContext != nullptr)
            {TRACE_IT(42143);
                dwDebugHostSourceContext = this->hostDebugContext->GetHostSourceContext(sourceInfo);
            }

            pFunctionsToRegister = sourceInfo->GetTopLevelFunctionInfoList();

            if (pFunctionsToRegister == nullptr || pFunctionsToRegister->Count() == 0)
            {TRACE_IT(42144);
                // This could happen if there are no functions to re-compile.
                return false;
            }

            if (this->hostDebugContext != nullptr && sourceInfo->GetSourceContextInfo())
            {TRACE_IT(42145);
                // This call goes out of engine
                this->hostDebugContext->SetThreadDescription(sourceInfo->GetSourceContextInfo()->url); // the HRESULT is omitted.
            }

            bool fHasDoneSourceRundown = false;
            for (int i = 0; i < pFunctionsToRegister->Count(); i++)
            {TRACE_IT(42146);
                if (cachedScriptContext->IsClosed())
                {TRACE_IT(42147);
                    // ScriptContext could be closed in previous iteration
                    hr = E_FAIL;
                    return true;
                }

                Js::FunctionInfo *functionInfo = pFunctionsToRegister->Item(i);
                if (functionInfo == nullptr)
                {TRACE_IT(42148);
                    continue;
                }

                if (shouldReparseFunctions)
                {
                    BEGIN_JS_RUNTIME_CALL_EX_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED(cachedScriptContext, false)
                    {TRACE_IT(42149);
                        functionInfo->GetParseableFunctionInfo()->Parse();
                        // This is the first call to the function, ensure dynamic profile info
#if ENABLE_PROFILE_INFO
                        functionInfo->GetFunctionBody()->EnsureDynamicProfileInfo();
#endif
                    }
                    END_JS_RUNTIME_CALL_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr);

                    // Debugger attach/detach failure is catastrophic, take down the process
                    DEBUGGER_ATTACHDETACH_FATAL_ERROR_IF_FAILED(hr);
                }

                // Parsing the function may change its FunctionProxy.
                Js::ParseableFunctionInfo *parseableFunctionInfo = functionInfo->GetParseableFunctionInfo();

                if (!fHasDoneSourceRundown && shouldPerformSourceRundown && !cachedScriptContext->IsClosed())
                {TRACE_IT(42150);
                    BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
                    {TRACE_IT(42151);
                        this->RegisterFunction(parseableFunctionInfo, dwDebugHostSourceContext, parseableFunctionInfo->GetSourceName());
                    }
                    END_TRANSLATE_OOM_TO_HRESULT(hr);

                    fHasDoneSourceRundown = true;
                }
            }

            if (shouldReparseFunctions)
            {TRACE_IT(42152);
                sourceInfo->MapFunction([](Js::FunctionBody *const pFuncBody)
                {
                    if (pFuncBody->IsFunctionParsed())
                    {TRACE_IT(42153);
                        pFuncBody->ReinitializeExecutionModeAndLimits();
                    }
                });
            }

            return false;
        });

        if (!cachedScriptContext->IsClosed())
        {TRACE_IT(42154);
            if (shouldPerformSourceRundown && cachedScriptContext->HaveCalleeSources() && this->hostDebugContext != nullptr)
            {TRACE_IT(42155);
                cachedScriptContext->MapCalleeSources([=](Js::Utf8SourceInfo* calleeSourceInfo)
                {
                    if (!cachedScriptContext->IsClosed())
                    {TRACE_IT(42156);
                        // This call goes out of engine
                        this->hostDebugContext->ReParentToCaller(calleeSourceInfo);
                    }
                });
            }
        }
        else
        {TRACE_IT(42157);
            hr = E_FAIL;
        }

        threadContext->ReleaseTemporaryAllocator(tempAllocator);

        return hr;
    }

    // Create an ordered flat list of sources to reparse. Caller of a source should be added to the list before we add the source itself.
    void DebugContext::WalkAndAddUtf8SourceInfo(Js::Utf8SourceInfo* sourceInfo, JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer> *utf8SourceInfoList)
    {TRACE_IT(42158);
        Js::Utf8SourceInfo* callerUtf8SourceInfo = sourceInfo->GetCallerUtf8SourceInfo();
        if (callerUtf8SourceInfo)
        {TRACE_IT(42159);
            Js::ScriptContext* callerScriptContext = callerUtf8SourceInfo->GetScriptContext();
            OUTPUT_TRACE(Js::DebuggerPhase, _u("DebugContext::WalkAndAddUtf8SourceInfo scriptContext 0x%p, sourceInfo 0x%p, callerUtf8SourceInfo 0x%p, sourceInfo scriptContext 0x%p, callerUtf8SourceInfo scriptContext 0x%p\n"),
                this->scriptContext, sourceInfo, callerUtf8SourceInfo, sourceInfo->GetScriptContext(), callerScriptContext);

            if (sourceInfo->GetScriptContext() == callerScriptContext)
            {
                WalkAndAddUtf8SourceInfo(callerUtf8SourceInfo, utf8SourceInfoList);
            }
            else if (callerScriptContext->IsScriptContextInNonDebugMode())
            {TRACE_IT(42160);
                // The caller scriptContext is not in run down/debug mode so let's save the relationship so that we can re-parent callees afterwards.
                callerScriptContext->AddCalleeSourceInfoToList(sourceInfo);
            }
        }
        if (!utf8SourceInfoList->Contains(sourceInfo))
        {TRACE_IT(42161);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("DebugContext::WalkAndAddUtf8SourceInfo Adding to utf8SourceInfoList scriptContext 0x%p, sourceInfo 0x%p, sourceInfo scriptContext 0x%p\n"),
                this->scriptContext, sourceInfo, sourceInfo->GetScriptContext());
#if DBG
            bool found = false;
            this->MapUTF8SourceInfoUntil([&](Js::Utf8SourceInfo * sourceInfoTemp) -> bool
            {
                if (sourceInfoTemp == sourceInfo)
                {TRACE_IT(42162);
                    found = true;
                }
                return found;
            });
            AssertMsg(found, "Parented eval feature have extra source");
#endif
            utf8SourceInfoList->Add(sourceInfo);
        }
    }

    template<class TMapFunction>
    void DebugContext::MapUTF8SourceInfoUntil(TMapFunction map)
    {TRACE_IT(42163);
        this->scriptContext->MapScript([=](Js::Utf8SourceInfo* sourceInfo) -> bool {
            return map(sourceInfo);
        });
    }
}
