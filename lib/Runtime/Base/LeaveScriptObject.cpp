//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

namespace Js
{
    EnterScriptObject::EnterScriptObject(ScriptContext* scriptContext, ScriptEntryExitRecord* entryExitRecord,
        void * returnAddress, bool doCleanup, bool isCallRoot, bool hasCaller)
    {TRACE_IT(35774);
        Assert(scriptContext);

#ifdef PROFILE_EXEC
        scriptContext->ProfileBegin(Js::RunPhase);
#endif

        if (scriptContext->GetThreadContext() &&
            scriptContext->GetThreadContext()->IsNoScriptScope())
        {TRACE_IT(35775);
            FromDOM_NoScriptScope_fatal_error();
        }

        // Keep a copy locally so the optimizer can just copy prop it to the dtor
        this->scriptContext = scriptContext;
        this->entryExitRecord = entryExitRecord;
        this->doCleanup = doCleanup;
        this->isCallRoot = isCallRoot;
        this->hr = NOERROR;
        this->hasForcedEnter =
#ifdef ENABLE_SCRIPT_DEBUGGING
         scriptContext->GetDebugContext() != nullptr ?
            scriptContext->GetDebugContext()->GetProbeContainer()->isForcedToEnterScriptStart :
#endif
            false;

        // Initialize the entry exit record
        entryExitRecord->returnAddrOfScriptEntryFunction = returnAddress;
        entryExitRecord->hasCaller = hasCaller;
        entryExitRecord->scriptContext = scriptContext;
#ifdef EXCEPTION_CHECK
        entryExitRecord->handledExceptionType = ExceptionCheck::ClearHandledExceptionType();
#endif
#if DBG_DUMP
        entryExitRecord->isCallRoot = isCallRoot;
#endif
        if (!scriptContext->IsClosed())
        {TRACE_IT(35776);
            library = scriptContext->GetLibrary();
        }
        try
        {TRACE_IT(35777);
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);
            scriptContext->GetThreadContext()->PushHostScriptContext(scriptContext->GetHostScriptContext());
        }
        catch (Js::OutOfMemoryException)
        {TRACE_IT(35778);
            this->hr = E_OUTOFMEMORY;
        }
        BEGIN_NO_EXCEPTION
        {
            // We can not have any exception in the constructor, otherwise the destructor will
            // not run and we might be in an inconsistent state

            // Put any code that may raise an exception in OnScriptStart
            scriptContext->GetThreadContext()->EnterScriptStart(entryExitRecord, doCleanup);
        }
        END_NO_EXCEPTION
    }

    void EnterScriptObject::VerifyEnterScript()
    {TRACE_IT(35779);
        if (FAILED(hr))
        {TRACE_IT(35780);
            Assert(hr == E_OUTOFMEMORY);
            throw Js::OutOfMemoryException();
        }
    }

    EnterScriptObject::~EnterScriptObject()
    {TRACE_IT(35781);
        scriptContext->OnScriptEnd(isCallRoot, hasForcedEnter);
        if (SUCCEEDED(hr))
        {TRACE_IT(35782);
            scriptContext->GetThreadContext()->PopHostScriptContext();
        }
        scriptContext->GetThreadContext()->EnterScriptEnd(entryExitRecord, doCleanup);
#ifdef PROFILE_EXEC
        scriptContext->ProfileEnd(Js::RunPhase);
#endif
    }
};
