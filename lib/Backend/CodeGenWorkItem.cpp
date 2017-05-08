//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"
#include "Language/SourceDynamicProfileManager.h"

CodeGenWorkItem::CodeGenWorkItem(
    JsUtil::JobManager *const manager,
    Js::FunctionBody *const functionBody,
    Js::EntryPointInfo* entryPointInfo,
    bool isJitInDebugMode,
    CodeGenWorkItemType type)
    : JsUtil::Job(manager)
    , codeAddress(NULL)
    , functionBody(functionBody)
    , entryPointInfo(entryPointInfo)
    , recyclableData(nullptr)
    , isInJitQueue(false)
    , isAllocationCommitted(false)
    , queuedFullJitWorkItem(nullptr)
    , allocation(nullptr)
#ifdef IR_VIEWER
    , isRejitIRViewerFunction(false)
    , irViewerOutput(nullptr)
    , irViewerRequestContext(nullptr)
#endif
{
    this->jitData = {0};
    // work item data
    this->jitData.type = type;
    this->jitData.isJitInDebugMode = isJitInDebugMode;
    ResetJitMode();
}

CodeGenWorkItem::~CodeGenWorkItem()
{TRACE_IT(1563);
    if(queuedFullJitWorkItem)
    {TRACE_IT(1564);
        HeapDelete(queuedFullJitWorkItem);
    }
}

//
// Helps determine whether a function should be speculatively jitted.
// This function is only used once and is used in a time-critical area, so
// be careful with it (moving it around actually caused around a 5% perf
// regression on a test).
//
bool CodeGenWorkItem::ShouldSpeculativelyJit(uint byteCodeSizeGenerated) const
{TRACE_IT(1565);
    if(PHASE_OFF(Js::FullJitPhase, this->functionBody))
    {TRACE_IT(1566);
        return false;
    }

    byteCodeSizeGenerated += this->GetByteCodeCount();
    if(CONFIG_FLAG(ProfileBasedSpeculativeJit))
    {TRACE_IT(1567);
        Assert(!CONFIG_ISENABLED(Js::NoDynamicProfileInMemoryCacheFlag));

        // JIT this now if we are under the speculation cap.
        return
            byteCodeSizeGenerated < (uint)CONFIG_FLAG(SpeculationCap) ||
            (
                byteCodeSizeGenerated < (uint)CONFIG_FLAG(ProfileBasedSpeculationCap) &&
                this->ShouldSpeculativelyJitBasedOnProfile()
            );
    }
    else
    {TRACE_IT(1568);
        return byteCodeSizeGenerated < (uint)CONFIG_FLAG(SpeculationCap);
    }
}

bool CodeGenWorkItem::ShouldSpeculativelyJitBasedOnProfile() const
{TRACE_IT(1569);
    Js::FunctionBody* functionBody = this->GetFunctionBody();

    uint loopPercentage = (functionBody->GetByteCodeInLoopCount()*100) / (functionBody->GetByteCodeCount() + 1);
    uint straightLineSize = functionBody->GetByteCodeCount() - functionBody->GetByteCodeInLoopCount();

    // This ensures only small and loopy functions are prejitted.
    if(loopPercentage >= 50 || straightLineSize < 300)
    {TRACE_IT(1570);
        Js::SourceDynamicProfileManager* profileManager = functionBody->GetSourceContextInfo()->sourceDynamicProfileManager;
        if(profileManager != nullptr)
        {TRACE_IT(1571);
            functionBody->SetIsSpeculativeJitCandidate();

            if(!functionBody->HasDynamicProfileInfo())
            {TRACE_IT(1572);
                return false;
            }

            Js::ExecutionFlags executionFlags = profileManager->IsFunctionExecuted(functionBody->GetLocalFunctionId());
            if(executionFlags == Js::ExecutionFlags_Executed)
            {TRACE_IT(1573);
                return true;
            }
        }
    }
    return false;
}

/*
    A comment about how to cause certain phases to only be on:

    INT = Interpreted, SJ = SimpleJit, FJ = FullJit

    To get only the following levels on, use the flags:

    INT:         -noNative
    SJ :         -forceNative -off:fullJit
    FJ :         -forceNative -off:simpleJit
    INT, SJ:     -off:fullJit
    INT, FJ:     -off:simpleJit
    SJ, FG:      -forceNative
    INT, SJ, FG: (default)
*/

void CodeGenWorkItem::OnAddToJitQueue()
{TRACE_IT(1574);
    Assert(!this->isInJitQueue);
    this->isInJitQueue = true;
    VerifyJitMode();

    this->entryPointInfo->SetCodeGenQueued();
    if(IS_JS_ETW(EventEnabledJSCRIPT_FUNCTION_JIT_QUEUED()))
    {TRACE_IT(1575);
        WCHAR displayNameBuffer[256];
        WCHAR* displayName = displayNameBuffer;
        size_t sizeInChars = this->GetDisplayName(displayName, 256);
        if(sizeInChars > 256)
        {TRACE_IT(1576);
            displayName = HeapNewArray(WCHAR, sizeInChars);
            this->GetDisplayName(displayName, 256);
        }
        JS_ETW(EventWriteJSCRIPT_FUNCTION_JIT_QUEUED(
            this->GetFunctionNumber(),
            displayName,
            this->GetScriptContext(),
            this->GetInterpretedCount()));

        if(displayName != displayNameBuffer)
        {
            HeapDeleteArray(sizeInChars, displayName);
        }
    }
}

void CodeGenWorkItem::OnRemoveFromJitQueue(NativeCodeGenerator* generator)
{TRACE_IT(1577);
    // This is called from within the lock

    this->isInJitQueue = false;
    this->entryPointInfo->SetCodeGenPending();
    functionBody->GetScriptContext()->GetThreadContext()->UnregisterCodeGenRecyclableData(this->recyclableData);
    this->recyclableData = nullptr;

    if(IS_JS_ETW(EventEnabledJSCRIPT_FUNCTION_JIT_DEQUEUED()))
    {TRACE_IT(1578);
        WCHAR displayNameBuffer[256];
        WCHAR* displayName = displayNameBuffer;
        size_t sizeInChars = this->GetDisplayName(displayName, 256);
        if(sizeInChars > 256)
        {TRACE_IT(1579);
            displayName = HeapNewArray(WCHAR, sizeInChars);
            this->GetDisplayName(displayName, 256);
        }
        JS_ETW(EventWriteJSCRIPT_FUNCTION_JIT_DEQUEUED(
            this->GetFunctionNumber(),
            displayName,
            this->GetScriptContext(),
            this->GetInterpretedCount()));

        if(displayName != displayNameBuffer)
        {
            HeapDeleteArray(sizeInChars, displayName);
        }
    }

    if(this->Type() == JsLoopBodyWorkItemType)
    {TRACE_IT(1580);
        // Go ahead and delete it and let it re-queue if more interpreting of the loop happens
        auto loopBodyWorkItem = static_cast<JsLoopBodyCodeGen*>(this);
        loopBodyWorkItem->loopHeader->ResetInterpreterCount();
        loopBodyWorkItem->GetEntryPoint()->Reset();
        HeapDelete(loopBodyWorkItem);
    }
    else
    {TRACE_IT(1581);
        Assert(GetJitMode() == ExecutionMode::FullJit); // simple JIT work items are not removed from the queue

        GetFunctionBody()->OnFullJitDequeued(static_cast<Js::FunctionEntryPointInfo *>(GetEntryPoint()));

        // Add it back to the list of available functions to be jitted
        generator->AddWorkItem(this);
    }
}

void CodeGenWorkItem::OnWorkItemProcessFail(NativeCodeGenerator* codeGen)
{TRACE_IT(1582);
    if (!isAllocationCommitted && this->allocation != nullptr && this->allocation->allocation != nullptr)
    {TRACE_IT(1583);
#if DBG
        this->allocation->allocation->isNotExecutableBecauseOOM = true;
#endif
        codeGen->FreeNativeCodeGenAllocation(this->allocation->allocation->address);
    }
}

QueuedFullJitWorkItem *CodeGenWorkItem::GetQueuedFullJitWorkItem() const
{TRACE_IT(1584);
    return queuedFullJitWorkItem;
}

QueuedFullJitWorkItem *CodeGenWorkItem::EnsureQueuedFullJitWorkItem()
{TRACE_IT(1585);
    if(queuedFullJitWorkItem)
    {TRACE_IT(1586);
        return queuedFullJitWorkItem;
    }

    queuedFullJitWorkItem = HeapNewNoThrow(QueuedFullJitWorkItem, this);
    return queuedFullJitWorkItem;
}
