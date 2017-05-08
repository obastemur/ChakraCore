//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language/JavascriptStackWalker.h"
namespace Js
{
    DebugManager::DebugManager(ThreadContext* _pThreadContext, AllocationPolicyManager * allocationPolicyManager) :
        pCurrentInterpreterLocation(nullptr),
        secondaryCurrentSourceContext(0),
        debugSessionNumber(0),
        pThreadContext(_pThreadContext),
        isAtDispatchHalt(false),
        mutationNewValuePid(Js::Constants::NoProperty),
        mutationPropertyNamePid(Js::Constants::NoProperty),
        mutationTypePid(Js::Constants::NoProperty),
        diagnosticPageAllocator(allocationPolicyManager, Js::Configuration::Global.flags, PageAllocatorType_Diag, 0),
        evalCodeRegistrationCount(0),
        anonymousCodeRegistrationCount(0),
        jscriptBlockRegistrationCount(0),
        isDebuggerAttaching(false),
        nextBreakPointId(0),
        localsDisplayFlags(LocalsDisplayFlags_None),
        dispatchHaltFrameAddress(nullptr)
    {TRACE_IT(42221);
        Assert(_pThreadContext != nullptr);
#if DBG
        // diagnosticPageAllocator may be used in multiple thread, but it's usage is synchronized.
        diagnosticPageAllocator.SetDisableThreadAccessCheck();
        diagnosticPageAllocator.debugName = _u("Diagnostic");
#endif
    }

    void DebugManager::Close()
    {TRACE_IT(42222);
        this->diagnosticPageAllocator.Close();

        if (this->pConsoleScope)
        {TRACE_IT(42223);
            this->pConsoleScope.Unroot(this->pThreadContext->GetRecycler());
        }
#if DBG
        this->pThreadContext->EnsureNoReturnedValueList();
#endif
        this->pThreadContext = nullptr;
    }

    DebugManager::~DebugManager()
    {TRACE_IT(42224);
        Assert(this->pThreadContext == nullptr);
    }

    DebuggingFlags* DebugManager::GetDebuggingFlags()
    {TRACE_IT(42225);
        return &this->debuggingFlags;
    }

    intptr_t DebugManager::GetDebuggingFlagsAddr() const
    {TRACE_IT(42226);
        return (intptr_t)&this->debuggingFlags;
    }

    ReferencedArenaAdapter* DebugManager::GetDiagnosticArena()
    {TRACE_IT(42227);
        if (pCurrentInterpreterLocation)
        {TRACE_IT(42228);
            return pCurrentInterpreterLocation->referencedDiagnosticArena;
        }
        return nullptr;
    }

    DWORD_PTR DebugManager::AllocateSecondaryHostSourceContext()
    {TRACE_IT(42229);
        Assert(secondaryCurrentSourceContext < ULONG_MAX);
        return secondaryCurrentSourceContext++; // The context is not valid, use the secondary context for identify the function body for further use.
    }

    void DebugManager::SetCurrentInterpreterLocation(InterpreterHaltState* pHaltState)
    {TRACE_IT(42230);
        Assert(pHaltState);
        Assert(!pCurrentInterpreterLocation);

        pCurrentInterpreterLocation = pHaltState;

        AutoAllocatorObjectPtr<ArenaAllocator, HeapAllocator> pDiagArena(HeapNew(ArenaAllocator, _u("DiagHaltState"), this->pThreadContext->GetPageAllocator(), Js::Throw::OutOfMemory), &HeapAllocator::Instance);
        AutoAllocatorObjectPtr<ReferencedArenaAdapter, HeapAllocator> referencedDiagnosticArena(HeapNew(ReferencedArenaAdapter, pDiagArena), &HeapAllocator::Instance);
        pCurrentInterpreterLocation->referencedDiagnosticArena = referencedDiagnosticArena;

        pThreadContext->GetRecycler()->RegisterExternalGuestArena(pDiagArena);
        debugSessionNumber++;

        pDiagArena.Detach();
        referencedDiagnosticArena.Detach();
    }

    void DebugManager::UnsetCurrentInterpreterLocation()
    {TRACE_IT(42231);
        Assert(pCurrentInterpreterLocation);

        if (pCurrentInterpreterLocation)
        {TRACE_IT(42232);
            // pCurrentInterpreterLocation->referencedDiagnosticArena could be null if we ran out of memory during SetCurrentInterpreterLocation
            if (pCurrentInterpreterLocation->referencedDiagnosticArena)
            {TRACE_IT(42233);
                pThreadContext->GetRecycler()->UnregisterExternalGuestArena(pCurrentInterpreterLocation->referencedDiagnosticArena->Arena());
                pCurrentInterpreterLocation->referencedDiagnosticArena->DeleteArena();
                pCurrentInterpreterLocation->referencedDiagnosticArena->Release();
            }

            pCurrentInterpreterLocation = nullptr;
        }
    }

    bool DebugManager::IsMatchTopFrameStackAddress(DiagStackFrame* frame) const
    {TRACE_IT(42234);
        return (frame != nullptr) && 
            (this->pCurrentInterpreterLocation != nullptr) &&
            (this->pCurrentInterpreterLocation->topFrame != nullptr) &&
            (this->pCurrentInterpreterLocation->topFrame->GetStackAddress() == frame->GetStackAddress());
    }

#ifdef ENABLE_MUTATION_BREAKPOINT
    MutationBreakpoint* DebugManager::GetActiveMutationBreakpoint() const
    {TRACE_IT(42235);
        Assert(this->pCurrentInterpreterLocation);
        return this->pCurrentInterpreterLocation->activeMutationBP;
    }
#endif

    DynamicObject* DebugManager::GetConsoleScope(ScriptContext* scriptContext)
    {TRACE_IT(42236);
        Assert(scriptContext);

        if (!this->pConsoleScope)
        {TRACE_IT(42237);
            this->pConsoleScope.Root(scriptContext->GetLibrary()->CreateConsoleScopeActivationObject(), this->pThreadContext->GetRecycler());
        }

        return (DynamicObject*)CrossSite::MarshalVar(scriptContext, (Var)this->pConsoleScope);
    }

    FrameDisplay *DebugManager::GetFrameDisplay(ScriptContext* scriptContext, DynamicObject* scopeAtZero, DynamicObject* scopeAtOne)
    {TRACE_IT(42238);
        // The scope chain for console eval looks like:
        //  - dummy empty object - new vars, let, consts, functions get added here
        //  - Active scope object containing all globals visible at this break (if at break)
        //  - Global this object so that existing properties are updated here
        //  - Console-1 Scope - all new globals will go here (like x = 1;)
        //  - NullFrameDisplay

        FrameDisplay* environment = JavascriptOperators::OP_LdFrameDisplay(this->GetConsoleScope(scriptContext), const_cast<FrameDisplay *>(&NullFrameDisplay), scriptContext);

        environment = JavascriptOperators::OP_LdFrameDisplay(scriptContext->GetGlobalObject()->ToThis(), environment, scriptContext);

        if (scopeAtOne != nullptr)
        {TRACE_IT(42239);
            environment = JavascriptOperators::OP_LdFrameDisplay((Var)scopeAtOne, environment, scriptContext);
        }

        environment = JavascriptOperators::OP_LdFrameDisplay((Var)scopeAtZero, environment, scriptContext);
        return environment;
    }

    void DebugManager::UpdateConsoleScope(DynamicObject* copyFromScope, ScriptContext* scriptContext)
    {TRACE_IT(42240);
        Assert(copyFromScope != nullptr);
        DynamicObject* consoleScope = this->GetConsoleScope(scriptContext);
        Js::RecyclableObject* recyclableObject = Js::RecyclableObject::FromVar(copyFromScope);

        uint32 newPropCount = recyclableObject->GetPropertyCount();
        for (uint32 i = 0; i < newPropCount; i++)
        {TRACE_IT(42241);
            Js::PropertyId propertyId = recyclableObject->GetPropertyId((Js::PropertyIndex)i);
            // For deleted properties we won't have a property id
            if (propertyId != Js::Constants::NoProperty)
            {TRACE_IT(42242);
                Js::PropertyValueInfo propertyValueInfo;
                Var propertyValue;
                BOOL gotPropertyValue = recyclableObject->GetProperty(recyclableObject, propertyId, &propertyValue, &propertyValueInfo, scriptContext);
                AssertMsg(gotPropertyValue, "DebugManager::UpdateConsoleScope Should have got valid value?");

                OUTPUT_TRACE(Js::ConsoleScopePhase, _u("Adding property '%s'\n"), scriptContext->GetPropertyName(propertyId)->GetBuffer());

                BOOL updateSuccess = consoleScope->SetPropertyWithAttributes(propertyId, propertyValue, propertyValueInfo.GetAttributes(), &propertyValueInfo);
                AssertMsg(updateSuccess, "DebugManager::UpdateConsoleScope Unable to update property value. Am I missing a scenario?");
            }
        }

        OUTPUT_TRACE(Js::ConsoleScopePhase, _u("Number of properties on console scope object after update are %d\n"), consoleScope->GetPropertyCount());
    }

#if DBG
    void DebugManager::ValidateDebugAPICall()
    {TRACE_IT(42243);
        Js::JavascriptStackWalker walker(this->pThreadContext->GetScriptEntryExit()->scriptContext);
        Js::JavascriptFunction* javascriptFunction = nullptr;
        if (walker.GetCaller(&javascriptFunction))
        {TRACE_IT(42244);
            if (javascriptFunction != nullptr)
            {TRACE_IT(42245);
                void *topJsFrameAddr = (void *)walker.GetCurrentArgv();
                Assert(this->dispatchHaltFrameAddress != nullptr);
                if (topJsFrameAddr < this->dispatchHaltFrameAddress)
                {
                    // we found the script frame after the break mode.
                    AssertMsg(false, "There are JavaScript frames between current API and dispatch halt");
                }
            }
        }
    }
#endif
}

AutoSetDispatchHaltFlag::AutoSetDispatchHaltFlag(Js::ScriptContext *scriptContext, ThreadContext *threadContext) :
    m_scriptContext(scriptContext),
    m_threadContext(threadContext)
{TRACE_IT(42246);
    Assert(m_scriptContext != nullptr);
    Assert(m_threadContext != nullptr);

    Assert(!m_threadContext->GetDebugManager()->IsAtDispatchHalt());
    m_threadContext->GetDebugManager()->SetDispatchHalt(true);

    Assert(!m_scriptContext->GetDebugContext()->GetProbeContainer()->IsPrimaryBrokenToDebuggerContext());
    m_scriptContext->GetDebugContext()->GetProbeContainer()->SetIsPrimaryBrokenToDebuggerContext(true);
}
AutoSetDispatchHaltFlag::~AutoSetDispatchHaltFlag()
{TRACE_IT(42247);
    Assert(m_threadContext->GetDebugManager()->IsAtDispatchHalt());
    m_threadContext->GetDebugManager()->SetDispatchHalt(false);

    Assert(m_scriptContext->GetDebugContext()->GetProbeContainer()->IsPrimaryBrokenToDebuggerContext());
    m_scriptContext->GetDebugContext()->GetProbeContainer()->SetIsPrimaryBrokenToDebuggerContext(false);
}
