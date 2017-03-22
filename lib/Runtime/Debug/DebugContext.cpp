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
    {LOGMEIN("DebugContext.cpp] 13\n");
        Assert(scriptContext != nullptr);
    }

    DebugContext::~DebugContext()
    {LOGMEIN("DebugContext.cpp] 18\n");
        Assert(this->scriptContext == nullptr);
        Assert(this->hostDebugContext == nullptr);
        Assert(this->diagProbesContainer == nullptr);
    }

    void DebugContext::Initialize()
    {LOGMEIN("DebugContext.cpp] 25\n");
        Assert(this->diagProbesContainer == nullptr);
        this->diagProbesContainer = HeapNew(ProbeContainer);
        this->diagProbesContainer->Initialize(this->scriptContext);
    }

    void DebugContext::Close()
    {LOGMEIN("DebugContext.cpp] 32\n");
        Assert(this->scriptContext != nullptr);
        this->scriptContext = nullptr;

        if (this->diagProbesContainer != nullptr)
        {LOGMEIN("DebugContext.cpp] 37\n");
            this->diagProbesContainer->Close();
            HeapDelete(this->diagProbesContainer);
            this->diagProbesContainer = nullptr;
        }

        if (this->hostDebugContext != nullptr)
        {LOGMEIN("DebugContext.cpp] 44\n");
            this->hostDebugContext->Delete();
            this->hostDebugContext = nullptr;
        }
    }

    void DebugContext::SetHostDebugContext(HostDebugContext * hostDebugContext)
    {LOGMEIN("DebugContext.cpp] 51\n");
        Assert(this->hostDebugContext == nullptr);
        Assert(hostDebugContext != nullptr);

        this->hostDebugContext = hostDebugContext;
    }

    bool DebugContext::CanRegisterFunction() const
    {LOGMEIN("DebugContext.cpp] 59\n");
        if (this->hostDebugContext == nullptr || this->scriptContext == nullptr || this->scriptContext->IsClosed() || this->IsDebugContextInNonDebugMode())
        {LOGMEIN("DebugContext.cpp] 61\n");
            return false;
        }
        return true;
    }

    void DebugContext::RegisterFunction(Js::ParseableFunctionInfo * func, LPCWSTR title)
    {LOGMEIN("DebugContext.cpp] 68\n");
        if (!this->CanRegisterFunction())
        {LOGMEIN("DebugContext.cpp] 70\n");
            return;
        }

        this->RegisterFunction(func, func->GetHostSourceContext(), title);
    }

    void DebugContext::RegisterFunction(Js::ParseableFunctionInfo * func, DWORD_PTR dwDebugSourceContext, LPCWSTR title)
    {LOGMEIN("DebugContext.cpp] 78\n");
        if (!this->CanRegisterFunction())
        {LOGMEIN("DebugContext.cpp] 80\n");
            return;
        }

        FunctionBody * functionBody = nullptr;
        if (func->IsDeferredParseFunction())
        {LOGMEIN("DebugContext.cpp] 86\n");
            HRESULT hr = S_OK;
            Assert(!this->scriptContext->GetThreadContext()->IsScriptActive());

            BEGIN_JS_RUNTIME_CALL_EX_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED(this->scriptContext, false)
            {LOGMEIN("DebugContext.cpp] 91\n");
                functionBody = func->Parse();
            }
            END_JS_RUNTIME_CALL_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT(hr);

            if (FAILED(hr))
            {LOGMEIN("DebugContext.cpp] 97\n");
                return;
            }
        }
        else
        {
            functionBody = func->GetFunctionBody();
        }
        this->RegisterFunction(functionBody, dwDebugSourceContext, title);
    }

    void DebugContext::RegisterFunction(Js::FunctionBody * functionBody, DWORD_PTR dwDebugSourceContext, LPCWSTR title)
    {LOGMEIN("DebugContext.cpp] 109\n");
        if (!this->CanRegisterFunction())
        {LOGMEIN("DebugContext.cpp] 111\n");
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
    {LOGMEIN("DebugContext.cpp] 125\n");
        if (this->debuggerMode == mode)
        {LOGMEIN("DebugContext.cpp] 127\n");
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
    {LOGMEIN("DebugContext.cpp] 142\n");
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
        {LOGMEIN("DebugContext.cpp] 166\n");
            Assert(FALSE);
            return hr;
        }

        // Cache ScriptContext as multiple calls below can go out of engine and ScriptContext can be closed which will delete DebugContext
        Js::ScriptContext* cachedScriptContext = this->scriptContext;

        utf8SourceInfoList->MapUntil([&](int index, Js::Utf8SourceInfo * sourceInfo) -> bool
        {
            if (cachedScriptContext->IsClosed())
            {LOGMEIN("DebugContext.cpp] 177\n");
                // ScriptContext could be closed in previous iteration
                hr = E_FAIL;
                return true;
            }

            OUTPUT_TRACE(Js::DebuggerPhase, _u("DebugContext::RundownSourcesAndReparse scriptContext 0x%p, sourceInfo 0x%p, HasDebugDocument %d\n"),
                this->scriptContext, sourceInfo, sourceInfo->HasDebugDocument());

            if (sourceInfo->GetIsLibraryCode())
            {LOGMEIN("DebugContext.cpp] 187\n");
                // Not putting the internal library code to the debug mode, but need to reinitialize execution mode limits of each
                // function body upon debugger detach, even for library code at the moment.
                if (shouldReparseFunctions)
                {LOGMEIN("DebugContext.cpp] 191\n");
                    sourceInfo->MapFunction([](Js::FunctionBody *const pFuncBody)
                    {
                        if (pFuncBody->IsFunctionParsed())
                        {LOGMEIN("DebugContext.cpp] 195\n");
                            pFuncBody->ReinitializeExecutionModeAndLimits();
                        }
                    });
                }
                return false;
            }

            Assert(sourceInfo->GetSrcInfo() && sourceInfo->GetSrcInfo()->sourceContextInfo);

#if DBG
            if (shouldPerformSourceRundown)
            {LOGMEIN("DebugContext.cpp] 207\n");
                // We shouldn't have a debug document if we're running source rundown for the first time.
                Assert(!sourceInfo->HasDebugDocument());
            }
#endif // DBG

            DWORD_PTR dwDebugHostSourceContext = Js::Constants::NoHostSourceContext;

            if (shouldPerformSourceRundown && this->hostDebugContext != nullptr)
            {LOGMEIN("DebugContext.cpp] 216\n");
                dwDebugHostSourceContext = this->hostDebugContext->GetHostSourceContext(sourceInfo);
            }

            pFunctionsToRegister = sourceInfo->GetTopLevelFunctionInfoList();

            if (pFunctionsToRegister == nullptr || pFunctionsToRegister->Count() == 0)
            {LOGMEIN("DebugContext.cpp] 223\n");
                // This could happen if there are no functions to re-compile.
                return false;
            }

            if (this->hostDebugContext != nullptr && sourceInfo->GetSourceContextInfo())
            {LOGMEIN("DebugContext.cpp] 229\n");
                // This call goes out of engine
                this->hostDebugContext->SetThreadDescription(sourceInfo->GetSourceContextInfo()->url); // the HRESULT is omitted.
            }

            bool fHasDoneSourceRundown = false;
            for (int i = 0; i < pFunctionsToRegister->Count(); i++)
            {LOGMEIN("DebugContext.cpp] 236\n");
                if (cachedScriptContext->IsClosed())
                {LOGMEIN("DebugContext.cpp] 238\n");
                    // ScriptContext could be closed in previous iteration
                    hr = E_FAIL;
                    return true;
                }

                Js::FunctionInfo *functionInfo = pFunctionsToRegister->Item(i);
                if (functionInfo == nullptr)
                {LOGMEIN("DebugContext.cpp] 246\n");
                    continue;
                }

                if (shouldReparseFunctions)
                {
                    BEGIN_JS_RUNTIME_CALL_EX_AND_TRANSLATE_EXCEPTION_AND_ERROROBJECT_TO_HRESULT_NESTED(cachedScriptContext, false)
                    {LOGMEIN("DebugContext.cpp] 253\n");
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
                {LOGMEIN("DebugContext.cpp] 270\n");
                    BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
                    {LOGMEIN("DebugContext.cpp] 272\n");
                        this->RegisterFunction(parseableFunctionInfo, dwDebugHostSourceContext, parseableFunctionInfo->GetSourceName());
                    }
                    END_TRANSLATE_OOM_TO_HRESULT(hr);

                    fHasDoneSourceRundown = true;
                }
            }

            if (shouldReparseFunctions)
            {LOGMEIN("DebugContext.cpp] 282\n");
                sourceInfo->MapFunction([](Js::FunctionBody *const pFuncBody)
                {
                    if (pFuncBody->IsFunctionParsed())
                    {LOGMEIN("DebugContext.cpp] 286\n");
                        pFuncBody->ReinitializeExecutionModeAndLimits();
                    }
                });
            }

            return false;
        });

        if (!cachedScriptContext->IsClosed())
        {LOGMEIN("DebugContext.cpp] 296\n");
            if (shouldPerformSourceRundown && cachedScriptContext->HaveCalleeSources() && this->hostDebugContext != nullptr)
            {LOGMEIN("DebugContext.cpp] 298\n");
                cachedScriptContext->MapCalleeSources([=](Js::Utf8SourceInfo* calleeSourceInfo)
                {
                    if (!cachedScriptContext->IsClosed())
                    {LOGMEIN("DebugContext.cpp] 302\n");
                        // This call goes out of engine
                        this->hostDebugContext->ReParentToCaller(calleeSourceInfo);
                    }
                });
            }
        }
        else
        {
            hr = E_FAIL;
        }

        threadContext->ReleaseTemporaryAllocator(tempAllocator);

        return hr;
    }

    // Create an ordered flat list of sources to reparse. Caller of a source should be added to the list before we add the source itself.
    void DebugContext::WalkAndAddUtf8SourceInfo(Js::Utf8SourceInfo* sourceInfo, JsUtil::List<Js::Utf8SourceInfo *, Recycler, false, Js::CopyRemovePolicy, RecyclerPointerComparer> *utf8SourceInfoList)
    {LOGMEIN("DebugContext.cpp] 321\n");
        Js::Utf8SourceInfo* callerUtf8SourceInfo = sourceInfo->GetCallerUtf8SourceInfo();
        if (callerUtf8SourceInfo)
        {LOGMEIN("DebugContext.cpp] 324\n");
            Js::ScriptContext* callerScriptContext = callerUtf8SourceInfo->GetScriptContext();
            OUTPUT_TRACE(Js::DebuggerPhase, _u("DebugContext::WalkAndAddUtf8SourceInfo scriptContext 0x%p, sourceInfo 0x%p, callerUtf8SourceInfo 0x%p, sourceInfo scriptContext 0x%p, callerUtf8SourceInfo scriptContext 0x%p\n"),
                this->scriptContext, sourceInfo, callerUtf8SourceInfo, sourceInfo->GetScriptContext(), callerScriptContext);

            if (sourceInfo->GetScriptContext() == callerScriptContext)
            {
                WalkAndAddUtf8SourceInfo(callerUtf8SourceInfo, utf8SourceInfoList);
            }
            else if (callerScriptContext->IsScriptContextInNonDebugMode())
            {LOGMEIN("DebugContext.cpp] 334\n");
                // The caller scriptContext is not in run down/debug mode so let's save the relationship so that we can re-parent callees afterwards.
                callerScriptContext->AddCalleeSourceInfoToList(sourceInfo);
            }
        }
        if (!utf8SourceInfoList->Contains(sourceInfo))
        {LOGMEIN("DebugContext.cpp] 340\n");
            OUTPUT_TRACE(Js::DebuggerPhase, _u("DebugContext::WalkAndAddUtf8SourceInfo Adding to utf8SourceInfoList scriptContext 0x%p, sourceInfo 0x%p, sourceInfo scriptContext 0x%p\n"),
                this->scriptContext, sourceInfo, sourceInfo->GetScriptContext());
#if DBG
            bool found = false;
            this->MapUTF8SourceInfoUntil([&](Js::Utf8SourceInfo * sourceInfoTemp) -> bool
            {
                if (sourceInfoTemp == sourceInfo)
                {LOGMEIN("DebugContext.cpp] 348\n");
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
    {LOGMEIN("DebugContext.cpp] 361\n");
        this->scriptContext->MapScript([=](Js::Utf8SourceInfo* sourceInfo) -> bool {
            return map(sourceInfo);
        });
    }
}
