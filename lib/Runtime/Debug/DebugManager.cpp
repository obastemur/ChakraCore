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
    {LOGMEIN("DebugManager.cpp] 25\n");
        Assert(_pThreadContext != nullptr);
#if DBG
        // diagnosticPageAllocator may be used in multiple thread, but it's usage is synchronized.
        diagnosticPageAllocator.SetDisableThreadAccessCheck();
        diagnosticPageAllocator.debugName = _u("Diagnostic");
#endif
    }

    void DebugManager::Close()
    {LOGMEIN("DebugManager.cpp] 35\n");
        this->diagnosticPageAllocator.Close();

        if (this->pConsoleScope)
        {LOGMEIN("DebugManager.cpp] 39\n");
            this->pConsoleScope.Unroot(this->pThreadContext->GetRecycler());
        }
#if DBG
        this->pThreadContext->EnsureNoReturnedValueList();
#endif
        this->pThreadContext = nullptr;
    }

    DebugManager::~DebugManager()
    {LOGMEIN("DebugManager.cpp] 49\n");
        Assert(this->pThreadContext == nullptr);
    }

    DebuggingFlags* DebugManager::GetDebuggingFlags()
    {LOGMEIN("DebugManager.cpp] 54\n");
        return &this->debuggingFlags;
    }

    intptr_t DebugManager::GetDebuggingFlagsAddr() const
    {LOGMEIN("DebugManager.cpp] 59\n");
        return (intptr_t)&this->debuggingFlags;
    }

    ReferencedArenaAdapter* DebugManager::GetDiagnosticArena()
    {LOGMEIN("DebugManager.cpp] 64\n");
        if (pCurrentInterpreterLocation)
        {LOGMEIN("DebugManager.cpp] 66\n");
            return pCurrentInterpreterLocation->referencedDiagnosticArena;
        }
        return nullptr;
    }

    DWORD_PTR DebugManager::AllocateSecondaryHostSourceContext()
    {LOGMEIN("DebugManager.cpp] 73\n");
        Assert(secondaryCurrentSourceContext < ULONG_MAX);
        return secondaryCurrentSourceContext++; // The context is not valid, use the secondary context for identify the function body for further use.
    }

    void DebugManager::SetCurrentInterpreterLocation(InterpreterHaltState* pHaltState)
    {LOGMEIN("DebugManager.cpp] 79\n");
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
    {LOGMEIN("DebugManager.cpp] 97\n");
        Assert(pCurrentInterpreterLocation);

        if (pCurrentInterpreterLocation)
        {LOGMEIN("DebugManager.cpp] 101\n");
            // pCurrentInterpreterLocation->referencedDiagnosticArena could be null if we ran out of memory during SetCurrentInterpreterLocation
            if (pCurrentInterpreterLocation->referencedDiagnosticArena)
            {LOGMEIN("DebugManager.cpp] 104\n");
                pThreadContext->GetRecycler()->UnregisterExternalGuestArena(pCurrentInterpreterLocation->referencedDiagnosticArena->Arena());
                pCurrentInterpreterLocation->referencedDiagnosticArena->DeleteArena();
                pCurrentInterpreterLocation->referencedDiagnosticArena->Release();
            }

            pCurrentInterpreterLocation = nullptr;
        }
    }

    bool DebugManager::IsMatchTopFrameStackAddress(DiagStackFrame* frame) const
    {LOGMEIN("DebugManager.cpp] 115\n");
        return (frame != nullptr) && 
            (this->pCurrentInterpreterLocation != nullptr) &&
            (this->pCurrentInterpreterLocation->topFrame != nullptr) &&
            (this->pCurrentInterpreterLocation->topFrame->GetStackAddress() == frame->GetStackAddress());
    }

#ifdef ENABLE_MUTATION_BREAKPOINT
    MutationBreakpoint* DebugManager::GetActiveMutationBreakpoint() const
    {LOGMEIN("DebugManager.cpp] 124\n");
        Assert(this->pCurrentInterpreterLocation);
        return this->pCurrentInterpreterLocation->activeMutationBP;
    }
#endif

    DynamicObject* DebugManager::GetConsoleScope(ScriptContext* scriptContext)
    {LOGMEIN("DebugManager.cpp] 131\n");
        Assert(scriptContext);

        if (!this->pConsoleScope)
        {LOGMEIN("DebugManager.cpp] 135\n");
            this->pConsoleScope.Root(scriptContext->GetLibrary()->CreateConsoleScopeActivationObject(), this->pThreadContext->GetRecycler());
        }

        return (DynamicObject*)CrossSite::MarshalVar(scriptContext, (Var)this->pConsoleScope);
    }

    FrameDisplay *DebugManager::GetFrameDisplay(ScriptContext* scriptContext, DynamicObject* scopeAtZero, DynamicObject* scopeAtOne)
    {LOGMEIN("DebugManager.cpp] 143\n");
        // The scope chain for console eval looks like:
        //  - dummy empty object - new vars, let, consts, functions get added here
        //  - Active scope object containing all globals visible at this break (if at break)
        //  - Global this object so that existing properties are updated here
        //  - Console-1 Scope - all new globals will go here (like x = 1;)
        //  - NullFrameDisplay

        FrameDisplay* environment = JavascriptOperators::OP_LdFrameDisplay(this->GetConsoleScope(scriptContext), const_cast<FrameDisplay *>(&NullFrameDisplay), scriptContext);

        environment = JavascriptOperators::OP_LdFrameDisplay(scriptContext->GetGlobalObject()->ToThis(), environment, scriptContext);

        if (scopeAtOne != nullptr)
        {LOGMEIN("DebugManager.cpp] 156\n");
            environment = JavascriptOperators::OP_LdFrameDisplay((Var)scopeAtOne, environment, scriptContext);
        }

        environment = JavascriptOperators::OP_LdFrameDisplay((Var)scopeAtZero, environment, scriptContext);
        return environment;
    }

    void DebugManager::UpdateConsoleScope(DynamicObject* copyFromScope, ScriptContext* scriptContext)
    {LOGMEIN("DebugManager.cpp] 165\n");
        Assert(copyFromScope != nullptr);
        DynamicObject* consoleScope = this->GetConsoleScope(scriptContext);
        Js::RecyclableObject* recyclableObject = Js::RecyclableObject::FromVar(copyFromScope);

        uint32 newPropCount = recyclableObject->GetPropertyCount();
        for (uint32 i = 0; i < newPropCount; i++)
        {LOGMEIN("DebugManager.cpp] 172\n");
            Js::PropertyId propertyId = recyclableObject->GetPropertyId((Js::PropertyIndex)i);
            // For deleted properties we won't have a property id
            if (propertyId != Js::Constants::NoProperty)
            {LOGMEIN("DebugManager.cpp] 176\n");
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
    {LOGMEIN("DebugManager.cpp] 194\n");
        Js::JavascriptStackWalker walker(this->pThreadContext->GetScriptEntryExit()->scriptContext);
        Js::JavascriptFunction* javascriptFunction = nullptr;
        if (walker.GetCaller(&javascriptFunction))
        {LOGMEIN("DebugManager.cpp] 198\n");
            if (javascriptFunction != nullptr)
            {LOGMEIN("DebugManager.cpp] 200\n");
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
{LOGMEIN("DebugManager.cpp] 217\n");
    Assert(m_scriptContext != nullptr);
    Assert(m_threadContext != nullptr);

    Assert(!m_threadContext->GetDebugManager()->IsAtDispatchHalt());
    m_threadContext->GetDebugManager()->SetDispatchHalt(true);

    Assert(!m_scriptContext->GetDebugContext()->GetProbeContainer()->IsPrimaryBrokenToDebuggerContext());
    m_scriptContext->GetDebugContext()->GetProbeContainer()->SetIsPrimaryBrokenToDebuggerContext(true);
}
AutoSetDispatchHaltFlag::~AutoSetDispatchHaltFlag()
{LOGMEIN("DebugManager.cpp] 228\n");
    Assert(m_threadContext->GetDebugManager()->IsAtDispatchHalt());
    m_threadContext->GetDebugManager()->SetDispatchHalt(false);

    Assert(m_scriptContext->GetDebugContext()->GetProbeContainer()->IsPrimaryBrokenToDebuggerContext());
    m_scriptContext->GetDebugContext()->GetProbeContainer()->SetIsPrimaryBrokenToDebuggerContext(false);
}
