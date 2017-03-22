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
{LOGMEIN("CodeGenWorkItem.cpp] 36\n");
    if(queuedFullJitWorkItem)
    {LOGMEIN("CodeGenWorkItem.cpp] 38\n");
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
{LOGMEIN("CodeGenWorkItem.cpp] 50\n");
    if(PHASE_OFF(Js::FullJitPhase, this->functionBody))
    {LOGMEIN("CodeGenWorkItem.cpp] 52\n");
        return false;
    }

    byteCodeSizeGenerated += this->GetByteCodeCount();
    if(CONFIG_FLAG(ProfileBasedSpeculativeJit))
    {LOGMEIN("CodeGenWorkItem.cpp] 58\n");
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
    {
        return byteCodeSizeGenerated < (uint)CONFIG_FLAG(SpeculationCap);
    }
}

bool CodeGenWorkItem::ShouldSpeculativelyJitBasedOnProfile() const
{LOGMEIN("CodeGenWorkItem.cpp] 76\n");
    Js::FunctionBody* functionBody = this->GetFunctionBody();

    uint loopPercentage = (functionBody->GetByteCodeInLoopCount()*100) / (functionBody->GetByteCodeCount() + 1);
    uint straightLineSize = functionBody->GetByteCodeCount() - functionBody->GetByteCodeInLoopCount();

    // This ensures only small and loopy functions are prejitted.
    if(loopPercentage >= 50 || straightLineSize < 300)
    {LOGMEIN("CodeGenWorkItem.cpp] 84\n");
        Js::SourceDynamicProfileManager* profileManager = functionBody->GetSourceContextInfo()->sourceDynamicProfileManager;
        if(profileManager != nullptr)
        {LOGMEIN("CodeGenWorkItem.cpp] 87\n");
            functionBody->SetIsSpeculativeJitCandidate();

            if(!functionBody->HasDynamicProfileInfo())
            {LOGMEIN("CodeGenWorkItem.cpp] 91\n");
                return false;
            }

            Js::ExecutionFlags executionFlags = profileManager->IsFunctionExecuted(functionBody->GetLocalFunctionId());
            if(executionFlags == Js::ExecutionFlags_Executed)
            {LOGMEIN("CodeGenWorkItem.cpp] 97\n");
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
{LOGMEIN("CodeGenWorkItem.cpp] 122\n");
    Assert(!this->isInJitQueue);
    this->isInJitQueue = true;
    VerifyJitMode();

    this->entryPointInfo->SetCodeGenQueued();
    if(IS_JS_ETW(EventEnabledJSCRIPT_FUNCTION_JIT_QUEUED()))
    {LOGMEIN("CodeGenWorkItem.cpp] 129\n");
        WCHAR displayNameBuffer[256];
        WCHAR* displayName = displayNameBuffer;
        size_t sizeInChars = this->GetDisplayName(displayName, 256);
        if(sizeInChars > 256)
        {LOGMEIN("CodeGenWorkItem.cpp] 134\n");
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
{LOGMEIN("CodeGenWorkItem.cpp] 152\n");
    // This is called from within the lock

    this->isInJitQueue = false;
    this->entryPointInfo->SetCodeGenPending();
    functionBody->GetScriptContext()->GetThreadContext()->UnregisterCodeGenRecyclableData(this->recyclableData);
    this->recyclableData = nullptr;

    if(IS_JS_ETW(EventEnabledJSCRIPT_FUNCTION_JIT_DEQUEUED()))
    {LOGMEIN("CodeGenWorkItem.cpp] 161\n");
        WCHAR displayNameBuffer[256];
        WCHAR* displayName = displayNameBuffer;
        size_t sizeInChars = this->GetDisplayName(displayName, 256);
        if(sizeInChars > 256)
        {LOGMEIN("CodeGenWorkItem.cpp] 166\n");
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
    {LOGMEIN("CodeGenWorkItem.cpp] 183\n");
        // Go ahead and delete it and let it re-queue if more interpreting of the loop happens
        auto loopBodyWorkItem = static_cast<JsLoopBodyCodeGen*>(this);
        loopBodyWorkItem->loopHeader->ResetInterpreterCount();
        loopBodyWorkItem->GetEntryPoint()->Reset();
        HeapDelete(loopBodyWorkItem);
    }
    else
    {
        Assert(GetJitMode() == ExecutionMode::FullJit); // simple JIT work items are not removed from the queue

        GetFunctionBody()->OnFullJitDequeued(static_cast<Js::FunctionEntryPointInfo *>(GetEntryPoint()));

        // Add it back to the list of available functions to be jitted
        generator->AddWorkItem(this);
    }
}

void CodeGenWorkItem::OnWorkItemProcessFail(NativeCodeGenerator* codeGen)
{LOGMEIN("CodeGenWorkItem.cpp] 202\n");
    if (!isAllocationCommitted && this->allocation != nullptr && this->allocation->allocation != nullptr)
    {LOGMEIN("CodeGenWorkItem.cpp] 204\n");
#if DBG
        this->allocation->allocation->isNotExecutableBecauseOOM = true;
#endif
        codeGen->FreeNativeCodeGenAllocation(this->allocation->allocation->address);
    }
}

QueuedFullJitWorkItem *CodeGenWorkItem::GetQueuedFullJitWorkItem() const
{LOGMEIN("CodeGenWorkItem.cpp] 213\n");
    return queuedFullJitWorkItem;
}

QueuedFullJitWorkItem *CodeGenWorkItem::EnsureQueuedFullJitWorkItem()
{LOGMEIN("CodeGenWorkItem.cpp] 218\n");
    if(queuedFullJitWorkItem)
    {LOGMEIN("CodeGenWorkItem.cpp] 220\n");
        return queuedFullJitWorkItem;
    }

    queuedFullJitWorkItem = HeapNewNoThrow(QueuedFullJitWorkItem, this);
    return queuedFullJitWorkItem;
}
