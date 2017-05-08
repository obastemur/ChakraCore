//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_TTD

namespace TTD
{
    TTDJsRTFunctionCallActionPopperRecorder::TTDJsRTFunctionCallActionPopperRecorder()
        : m_ctx(nullptr), m_beginTime(0.0), m_callAction(nullptr)
    {TRACE_IT(43553);
        ;
    }

    TTDJsRTFunctionCallActionPopperRecorder::~TTDJsRTFunctionCallActionPopperRecorder()
    {TRACE_IT(43554);
        if(this->m_ctx != nullptr)
        {TRACE_IT(43555);
            TTDAssert(this->m_callAction != nullptr, "Should be set in sync with ctx!!!");

            TTD::EventLog* elog = this->m_ctx->GetThreadContext()->TTDLog;
            NSLogEvents::JsRTCallFunctionAction* cfAction = NSLogEvents::GetInlineEventDataAs<NSLogEvents::JsRTCallFunctionAction, NSLogEvents::EventKind::CallExistingFunctionActionTag>(this->m_callAction);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            NSLogEvents::JsRTCallFunctionAction_ProcessDiagInfoPost(this->m_callAction, this->m_ctx->GetThreadContext()->TTDLog->GetLastEventTime());
#endif

            //Update the time elapsed since a snapshot if needed
            if(cfAction->CallbackDepth == 0)
            {TRACE_IT(43556);
                double elapsedTime = (elog->GetCurrentWallTime() - this->m_beginTime);
                elog->IncrementElapsedSnapshotTime(elapsedTime);
            }
        }
    }

    void TTDJsRTFunctionCallActionPopperRecorder::InitializeForRecording(Js::ScriptContext* ctx, double beginWallTime, NSLogEvents::EventLogEntry* callAction)
    {TRACE_IT(43557);
        TTDAssert(this->m_ctx == nullptr && this->m_callAction == nullptr, "Don't double initialize!!!");

        this->m_ctx = ctx;
        this->m_beginTime = beginWallTime;
        this->m_callAction = callAction;
    }

    TTLastReturnLocationInfo::TTLastReturnLocationInfo()
        : m_isExceptionFrame(false)
    {TRACE_IT(43558);
        this->m_lastFrame = { 0 };
    }

    void TTLastReturnLocationInfo::SetReturnLocation(const SingleCallCounter& cframe)
    {TRACE_IT(43559);
        this->m_isExceptionFrame = false;
        this->m_lastFrame = cframe;
    }

    void TTLastReturnLocationInfo::SetExceptionLocation(const SingleCallCounter& cframe)
    {TRACE_IT(43560);
        this->m_isExceptionFrame = true;
        this->m_lastFrame = cframe;
    }

    bool TTLastReturnLocationInfo::IsDefined() const
    {TRACE_IT(43561);
        return this->m_lastFrame.Function != nullptr;
    }

    bool TTLastReturnLocationInfo::IsReturnLocation() const
    {TRACE_IT(43562);
        return this->IsDefined() && !this->m_isExceptionFrame;
    }

    bool TTLastReturnLocationInfo::IsExceptionLocation() const
    {TRACE_IT(43563);
        return this->IsDefined() && this->m_isExceptionFrame;
    }

    const SingleCallCounter& TTLastReturnLocationInfo::GetLocation() const
    {TRACE_IT(43564);
        TTDAssert(this->IsDefined(), "Should check this!");

        return this->m_lastFrame;
    }

    void TTLastReturnLocationInfo::Clear()
    {TRACE_IT(43565);
        if(this->IsDefined())
        {TRACE_IT(43566);
            this->m_isExceptionFrame = false;
            this->m_lastFrame = { 0 };
        }
    }

    void TTLastReturnLocationInfo::ClearReturnOnly()
    {TRACE_IT(43567);
        if(this->IsDefined() && !this->m_isExceptionFrame)
        {TRACE_IT(43568);
            this->Clear();
        }
    }

    void TTLastReturnLocationInfo::ClearExceptionOnly()
    {TRACE_IT(43569);
        if(this->IsDefined() && this->m_isExceptionFrame)
        {TRACE_IT(43570);
            this->Clear();
        }
    }

    /////////////

    void TTEventList::AddArrayLink()
    {TRACE_IT(43571);
        TTEventListLink* newHeadBlock = this->m_alloc->SlabAllocateStruct<TTEventListLink>();
        newHeadBlock->BlockData = this->m_alloc->SlabAllocateFixedSizeArray<NSLogEvents::EventLogEntry, TTD_EVENTLOG_LIST_BLOCK_SIZE>();
        memset(newHeadBlock->BlockData, 0, TTD_EVENTLOG_LIST_BLOCK_SIZE * sizeof(NSLogEvents::EventLogEntry));

        newHeadBlock->CurrPos = 0;
        newHeadBlock->StartPos = 0;

        newHeadBlock->Next = nullptr;
        newHeadBlock->Previous = this->m_headBlock;

        if(this->m_headBlock != nullptr)
        {TRACE_IT(43572);
            this->m_headBlock->Next = newHeadBlock;
        }

        this->m_headBlock = newHeadBlock;
    }

    void TTEventList::RemoveArrayLink(TTEventListLink* block)
    {TRACE_IT(43573);
        TTDAssert(block->Previous == nullptr, "Not first event block in log!!!");
        TTDAssert(block->StartPos == block->CurrPos, "Haven't cleared all the events in this link");

        if(block->Next == nullptr)
        {TRACE_IT(43574);
            this->m_headBlock = nullptr; //was only 1 block to we are now all null
        }
        else
        {TRACE_IT(43575);
            block->Next->Previous = nullptr;
        }

        this->m_alloc->UnlinkAllocation(block->BlockData);
        this->m_alloc->UnlinkAllocation(block);
    }

    TTEventList::TTEventList(UnlinkableSlabAllocator* alloc)
        : m_alloc(alloc), m_headBlock(nullptr)
    {TRACE_IT(43576);
        ;
    }

    void TTEventList::UnloadEventList(NSLogEvents::EventLogEntryVTableEntry* vtable)
    {TRACE_IT(43577);
        if(this->m_headBlock == nullptr)
        {TRACE_IT(43578);
            return;
        }

        TTEventListLink* firstBlock = this->m_headBlock;
        while(firstBlock->Previous != nullptr)
        {TRACE_IT(43579);
            firstBlock = firstBlock->Previous;
        }

        TTEventListLink* curr = firstBlock;
        while(curr != nullptr)
        {TRACE_IT(43580);
            for(uint32 i = curr->StartPos; i < curr->CurrPos; ++i)
            {TRACE_IT(43581);
                const NSLogEvents::EventLogEntry* entry = curr->BlockData + i;
                auto unloadFP = vtable[(uint32)entry->EventKind].UnloadFP; //use vtable magic here

                if(unloadFP != nullptr)
                {TRACE_IT(43582);
                    unloadFP(curr->BlockData + i, *(this->m_alloc));
                }
            }
            curr->StartPos = curr->CurrPos;

            TTEventListLink* next = curr->Next;
            this->RemoveArrayLink(curr);
            curr = next;
        }

        this->m_headBlock = nullptr;
    }

    NSLogEvents::EventLogEntry* TTEventList::GetNextAvailableEntry()
    {TRACE_IT(43583);
        if((this->m_headBlock == nullptr) || (this->m_headBlock->CurrPos == TTD_EVENTLOG_LIST_BLOCK_SIZE))
        {TRACE_IT(43584);
            this->AddArrayLink();
        }

        NSLogEvents::EventLogEntry* entry = (this->m_headBlock->BlockData + this->m_headBlock->CurrPos);
        this->m_headBlock->CurrPos++;

        return entry;
    }

    void TTEventList::DeleteFirstEntry(TTEventListLink* block, NSLogEvents::EventLogEntry* data, NSLogEvents::EventLogEntryVTableEntry* vtable)
    {TRACE_IT(43585);
        TTDAssert((block->BlockData + block->StartPos) == data, "Not the data at the start of the list!!!");

        auto unloadFP = vtable[(uint32)data->EventKind].UnloadFP; //use vtable magic here

        if(unloadFP != nullptr)
        {
            unloadFP(data, *(this->m_alloc));
        }

        block->StartPos++;
        if(block->StartPos == block->CurrPos)
        {TRACE_IT(43586);
            this->RemoveArrayLink(block);
        }
    }

    bool TTEventList::IsEmpty() const
    {TRACE_IT(43587);
        return this->m_headBlock == nullptr;
    }

    uint32 TTEventList::Count() const
    {TRACE_IT(43588);
        uint32 count = 0;

        for(TTEventListLink* curr = this->m_headBlock; curr != nullptr; curr = curr->Previous)
        {TRACE_IT(43589);
            count += (curr->CurrPos - curr->StartPos);
        }

        return (uint32)count;
    }

    TTEventList::Iterator::Iterator()
        : m_currLink(nullptr), m_currIdx(0)
    {TRACE_IT(43590);
        ;
    }

    TTEventList::Iterator::Iterator(TTEventListLink* head, uint32 pos)
        : m_currLink(head), m_currIdx(pos)
    {TRACE_IT(43591);
        ;
    }

    const NSLogEvents::EventLogEntry* TTEventList::Iterator::Current() const
    {TRACE_IT(43592);
        TTDAssert(this->IsValid(), "Iterator is invalid!!!");

        return (this->m_currLink->BlockData + this->m_currIdx);
    }

    NSLogEvents::EventLogEntry* TTEventList::Iterator::Current()
    {TRACE_IT(43593);
        TTDAssert(this->IsValid(), "Iterator is invalid!!!");

        return (this->m_currLink->BlockData + this->m_currIdx);
    }

    TTEventList::TTEventListLink* TTEventList::Iterator::GetBlock()
    {TRACE_IT(43594);
        return this->m_currLink;
    }

    bool TTEventList::Iterator::IsValid() const
    {TRACE_IT(43595);
        return (this->m_currLink != nullptr && this->m_currLink->StartPos <= this->m_currIdx && this->m_currIdx < this->m_currLink->CurrPos);
    }

    void TTEventList::Iterator::MoveNext()
    {TRACE_IT(43596);
        if(this->m_currIdx < (this->m_currLink->CurrPos - 1))
        {TRACE_IT(43597);
            this->m_currIdx++;
        }
        else
        {TRACE_IT(43598);
            this->m_currLink = this->m_currLink->Next;
            this->m_currIdx = (this->m_currLink != nullptr) ? this->m_currLink->StartPos : 0;
        }
    }

    void TTEventList::Iterator::MovePrevious()
    {TRACE_IT(43599);
        if(this->m_currIdx > this->m_currLink->StartPos)
        {TRACE_IT(43600);
            this->m_currIdx--;
        }
        else
        {TRACE_IT(43601);
            this->m_currLink = this->m_currLink->Previous;
            this->m_currIdx = (this->m_currLink != nullptr) ? (this->m_currLink->CurrPos - 1) : 0;
        }
    }

    TTEventList::Iterator TTEventList::GetIteratorAtFirst() const
    {TRACE_IT(43602);
        if(this->m_headBlock == nullptr)
        {TRACE_IT(43603);
            return Iterator(nullptr, 0);
        }
        else
        {TRACE_IT(43604);
            TTEventListLink* firstBlock = this->m_headBlock;
            while(firstBlock->Previous != nullptr)
            {TRACE_IT(43605);
                firstBlock = firstBlock->Previous;
            }

            return Iterator(firstBlock, firstBlock->StartPos);
        }
    }

    TTEventList::Iterator TTEventList::GetIteratorAtLast() const
    {TRACE_IT(43606);
        if(this->m_headBlock == nullptr)
        {TRACE_IT(43607);
            return Iterator(nullptr, 0);
        }
        else
        {TRACE_IT(43608);
            return Iterator(this->m_headBlock, this->m_headBlock->CurrPos - 1);
        }
    }

    //////

    const SingleCallCounter& EventLog::GetTopCallCounter() const
    {TRACE_IT(43609);
        TTDAssert(this->m_callStack.Count() != 0, "Empty stack!");

        return this->m_callStack.Item(this->m_callStack.Count() - 1);
    }

    SingleCallCounter& EventLog::GetTopCallCounter()
    {TRACE_IT(43610);
        TTDAssert(this->m_callStack.Count() != 0, "Empty stack!");

        return this->m_callStack.Item(this->m_callStack.Count() - 1);
    }

    bool EventLog::TryGetTopCallCallerCounter(SingleCallCounter& caller) const
    {TRACE_IT(43611);
        if(this->m_callStack.Count() < 2)
        {TRACE_IT(43612);
            return false;
        }
        else
        {TRACE_IT(43613);
            caller = this->m_callStack.Item(this->m_callStack.Count() - 2);
            return true;
        }
    }

    int64 EventLog::GetCurrentEventTimeAndAdvance()
    {TRACE_IT(43614);
        return this->m_eventTimeCtr++;
    }

    void EventLog::AdvanceTimeAndPositionForReplay()
    {TRACE_IT(43615);
        this->m_eventTimeCtr++;
        this->m_currentReplayEventIterator.MoveNext();

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        TTDAssert(!this->m_currentReplayEventIterator.IsValid() || this->m_eventTimeCtr == this->m_currentReplayEventIterator.Current()->EventTimeStamp, "Something is out of sync.");
#endif
    }

    void EventLog::UpdateComputedMode()
    {TRACE_IT(43616);
        TTDAssert(this->m_modeStack.Count() > 0, "Should never be empty!!!");

        TTDMode cm = TTDMode::Invalid;
        for(uint32 i = 0; i < this->m_modeStack.Count(); ++i)
        {TRACE_IT(43617);
            TTDMode m = this->m_modeStack.GetAt(i);
            switch(m)
            {
            case TTDMode::RecordMode:
            case TTDMode::ReplayMode:
            case TTDMode::DebuggerMode:
                TTDAssert(i == 0, "One of these should always be first on the stack.");
                cm = m;
                break;
            case TTDMode::CurrentlyEnabled:
            case TTDMode::ExcludedExecutionTTAction:
            case TTDMode::ExcludedExecutionDebuggerAction:
            case TTDMode::DebuggerSuppressGetter:
            case TTDMode::DebuggerSuppressBreakpoints:
            case TTDMode::DebuggerLogBreakpoints:
                TTDAssert(i != 0, "A base mode should always be first on the stack.");
                cm |= m;
                break;
            default:
                TTDAssert(false, "This mode is unknown or should never appear.");
                break;
            }
        }

        this->m_currentMode = cm;

        //Set fast path values on ThreadContext
        const JsUtil::List<Js::ScriptContext*, HeapAllocator>& contexts = this->m_threadContext->TTDContext->GetTTDContexts();
        for(int32 i = 0; i < contexts.Count(); ++i)
        {TRACE_IT(43618);
            this->SetModeFlagsOnContext(contexts.Item(i));
        }
    }

    void EventLog::UnloadRetainedData()
    {TRACE_IT(43619);
        if(this->m_lastInflateMap != nullptr)
        {
            TT_HEAP_DELETE(InflateMap, this->m_lastInflateMap);
            this->m_lastInflateMap = nullptr;
        }

        if(this->m_propertyRecordPinSet != nullptr)
        {TRACE_IT(43620);
            this->m_propertyRecordPinSet.Unroot(this->m_propertyRecordPinSet->GetAllocator());
        }

        this->UnLoadPreservedBPInfo();
    }

    SnapShot* EventLog::DoSnapshotExtract_Helper()
    {TRACE_IT(43621);
        SnapShot* snap = nullptr;

        this->m_snapExtractor.BeginSnapshot(this->m_threadContext);
        this->m_snapExtractor.DoMarkWalk(this->m_threadContext);

        ///////////////////////////
        //Phase 2: Evacuate marked objects
        //Allows for parallel execute and evacuate (in conjunction with later refactoring)

        this->m_snapExtractor.EvacuateMarkedIntoSnapshot(this->m_threadContext);

        ///////////////////////////
        //Phase 3: Complete and return snapshot

        snap = this->m_snapExtractor.CompleteSnapshot();

        return snap;
    }

    void EventLog::ReplaySnapshotEvent()
    {TRACE_IT(43622);
#if ENABLE_SNAPSHOT_COMPARE
        SnapShot* snap = nullptr;
        try
        {TRACE_IT(43623);
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory | ExceptionType_StackOverflow));

            this->SetSnapshotOrInflateInProgress(true);
            this->PushMode(TTDMode::ExcludedExecutionTTAction);

            snap = this->DoSnapshotExtract_Helper();

            NSLogEvents::EventLogEntry* evt = this->m_currentReplayEventIterator.Current();
            NSLogEvents::SnapshotEventLogEntry_EnsureSnapshotDeserialized(evt, this->m_threadContext);

            const NSLogEvents::SnapshotEventLogEntry* recordedSnapEntry = NSLogEvents::GetInlineEventDataAs<NSLogEvents::SnapshotEventLogEntry, NSLogEvents::EventKind::SnapshotTag>(evt);
            const SnapShot* recordedSnap = recordedSnapEntry->Snap;

            TTDCompareMap compareMap(this->m_threadContext);
            SnapShot::InitializeForSnapshotCompare(recordedSnap, snap, compareMap);
            SnapShot::DoSnapshotCompare(recordedSnap, snap, compareMap);

            TT_HEAP_DELETE(SnapShot, snap);

            this->PopMode(TTDMode::ExcludedExecutionTTAction);
            this->SetSnapshotOrInflateInProgress(false);
        }
        catch(...)
        {
            TT_HEAP_DELETE(SnapShot, snap);
            TTDAssert(false, "OOM in snapshot replay... just continue");
        }
#endif


#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteLiteralMsg("---SNAPSHOT EVENT---\n");
#endif

        this->AdvanceTimeAndPositionForReplay(); //move along
    }

    void EventLog::ReplayEventLoopYieldPointEvent()
    {TRACE_IT(43624);
        this->m_threadContext->TTDContext->ClearLocalRootsAndRefreshMap();

        this->AdvanceTimeAndPositionForReplay(); //move along
    }

    void EventLog::AbortReplayReturnToHost()
    {TRACE_IT(43625);
        throw TTDebuggerAbortException::CreateAbortEndOfLog(_u("End of log reached -- returning to top-level."));
    }

    void EventLog::InitializeEventListVTable()
    {TRACE_IT(43626);
        this->m_eventListVTable = this->m_miscSlabAllocator.SlabAllocateArray<NSLogEvents::EventLogEntryVTableEntry>((uint32)NSLogEvents::EventKind::Count);

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::SnapshotTag] = { NSLogEvents::ContextExecuteKind::GlobalAPIWrapper, nullptr, NSLogEvents::SnapshotEventLogEntry_UnloadEventMemory, NSLogEvents::SnapshotEventLogEntry_Emit, NSLogEvents::SnapshotEventLogEntry_Parse};
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::EventLoopYieldPointTag] = { NSLogEvents::ContextExecuteKind::GlobalAPIWrapper, nullptr, nullptr, NSLogEvents::EventLoopYieldPointEntry_Emit, NSLogEvents::EventLoopYieldPointEntry_Parse};
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::TopLevelCodeTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, nullptr, NSLogEvents::CodeLoadEventLogEntry_Emit, NSLogEvents::CodeLoadEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::TelemetryLogTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, NSLogEvents::TelemetryEventLogEntry_UnloadEventMemory, NSLogEvents::TelemetryEventLogEntry_Emit, NSLogEvents::TelemetryEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::DoubleTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, nullptr, NSLogEvents::DoubleEventLogEntry_Emit, NSLogEvents::DoubleEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::StringTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, NSLogEvents::StringValueEventLogEntry_UnloadEventMemory, NSLogEvents::StringValueEventLogEntry_Emit, NSLogEvents::StringValueEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::RandomSeedTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, nullptr, NSLogEvents::RandomSeedEventLogEntry_Emit, NSLogEvents::RandomSeedEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::PropertyEnumTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, NSLogEvents::PropertyEnumStepEventLogEntry_UnloadEventMemory, NSLogEvents::PropertyEnumStepEventLogEntry_Emit, NSLogEvents::PropertyEnumStepEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::SymbolCreationTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, nullptr, NSLogEvents::SymbolCreationEventLogEntry_Emit, NSLogEvents::SymbolCreationEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::ExternalCbRegisterCall] = { NSLogEvents::ContextExecuteKind::None, nullptr, nullptr, NSLogEvents::ExternalCbRegisterCallEventLogEntry_Emit, NSLogEvents::ExternalCbRegisterCallEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::ExternalCallTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, NSLogEvents::ExternalCallEventLogEntry_UnloadEventMemory, NSLogEvents::ExternalCallEventLogEntry_Emit, NSLogEvents::ExternalCallEventLogEntry_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::ExplicitLogWriteTag] = { NSLogEvents::ContextExecuteKind::None, nullptr, nullptr, NSLogEvents::ExplicitLogWriteEntry_Emit, NSLogEvents::ExplicitLogWriteEntry_Parse };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateScriptContextActionTag] = { NSLogEvents::ContextExecuteKind::GlobalAPIWrapper, NSLogEvents::CreateScriptContext_Execute, NSLogEvents::CreateScriptContext_UnloadEventMemory, NSLogEvents::CreateScriptContext_Emit, NSLogEvents::CreateScriptContext_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::SetActiveScriptContextActionTag] = { NSLogEvents::ContextExecuteKind::GlobalAPIWrapper, NSLogEvents::SetActiveScriptContext_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::SetActiveScriptContextActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::SetActiveScriptContextActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::DeadScriptContextActionTag] = { NSLogEvents::ContextExecuteKind::None, NSLogEvents::DeadScriptContext_Execute, NSLogEvents::DeadScriptContext_UnloadEventMemory, NSLogEvents::DeadScriptContext_Emit, NSLogEvents::DeadScriptContext_Parse };

#if !INT32VAR
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateIntegerActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::CreateInt_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::CreateIntegerActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::CreateIntegerActionTag> };
#endif

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateNumberActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::CreateNumber_Execute, nullptr, NSLogEvents::JsRTDoubleArgumentAction_Emit<NSLogEvents::EventKind::CreateNumberActionTag>, NSLogEvents::JsRTDoubleArgumentAction_Parse<NSLogEvents::EventKind::CreateNumberActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateBooleanActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::CreateBoolean_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::CreateBooleanActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::CreateBooleanActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateStringActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::CreateString_Execute, NSLogEvents::JsRTStringArgumentAction_UnloadEventMemory<NSLogEvents::EventKind::CreateStringActionTag>, NSLogEvents::JsRTStringArgumentAction_Emit<NSLogEvents::EventKind::CreateStringActionTag>, NSLogEvents::JsRTStringArgumentAction_Parse<NSLogEvents::EventKind::CreateStringActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateSymbolActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::CreateSymbol_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::CreateSymbolActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::CreateSymbolActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateErrorActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::CreateError_Execute<NSLogEvents::EventKind::CreateErrorActionTag>, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::CreateErrorActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::CreateErrorActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateRangeErrorActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::CreateError_Execute<NSLogEvents::EventKind::CreateRangeErrorActionTag>, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::CreateRangeErrorActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::CreateRangeErrorActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateReferenceErrorActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::CreateError_Execute<NSLogEvents::EventKind::CreateReferenceErrorActionTag>, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::CreateReferenceErrorActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::CreateReferenceErrorActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateSyntaxErrorActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::CreateError_Execute<NSLogEvents::EventKind::CreateSyntaxErrorActionTag>, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::CreateSyntaxErrorActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::CreateSyntaxErrorActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateTypeErrorActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::CreateError_Execute<NSLogEvents::EventKind::CreateTypeErrorActionTag>, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::CreateTypeErrorActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::CreateTypeErrorActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CreateURIErrorActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::CreateError_Execute<NSLogEvents::EventKind::CreateURIErrorActionTag>, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::CreateURIErrorActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::CreateURIErrorActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::VarConvertToNumberActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::VarConvertToNumber_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::VarConvertToNumberActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::VarConvertToNumberActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::VarConvertToBooleanActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::VarConvertToBoolean_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::VarConvertToBooleanActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::VarConvertToBooleanActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::VarConvertToStringActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::VarConvertToString_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::VarConvertToStringActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::VarConvertToStringActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::VarConvertToObjectActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::VarConvertToObject_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::VarConvertToObjectActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::VarConvertToObjectActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::AddRootRefActionTag] = { NSLogEvents::ContextExecuteKind::GlobalAPIWrapper, NSLogEvents::AddRootRef_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::AddRootRefActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::AddRootRefActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::RemoveRootRefActionTag] = { NSLogEvents::ContextExecuteKind::GlobalAPIWrapper, NSLogEvents::RemoveRootRef_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::RemoveRootRefActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::RemoveRootRefActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::AllocateObjectActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::AllocateObject_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::AllocateObjectActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::AllocateObjectActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::AllocateExternalObjectActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::AllocateExternalObject_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::AllocateExternalObjectActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::AllocateExternalObjectActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::AllocateArrayActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::AllocateArrayAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::AllocateArrayActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::AllocateArrayActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::AllocateArrayBufferActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::AllocateArrayBufferAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::AllocateArrayBufferActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::AllocateArrayBufferActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::AllocateExternalArrayBufferActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::AllocateExternalArrayBufferAction_Execute, NSLogEvents::JsRTByteBufferAction_UnloadEventMemory<NSLogEvents::EventKind::AllocateExternalArrayBufferActionTag>, NSLogEvents::JsRTByteBufferAction_Emit<NSLogEvents::EventKind::AllocateExternalArrayBufferActionTag>, NSLogEvents::JsRTByteBufferAction_Parse<NSLogEvents::EventKind::AllocateExternalArrayBufferActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::AllocateFunctionActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::AllocateFunctionAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::AllocateFunctionActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::AllocateFunctionActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::HostExitProcessTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::HostProcessExitAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::HostExitProcessTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::HostExitProcessTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetAndClearExceptionActionTag] = { NSLogEvents::ContextExecuteKind::None, NSLogEvents::GetAndClearExceptionAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::GetAndClearExceptionActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::GetAndClearExceptionActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::SetExceptionActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::SetExceptionAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::SetExceptionActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::SetExceptionActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::HasPropertyActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::HasPropertyAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::HasPropertyActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::HasPropertyActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::InstanceOfActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::InstanceOfAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::InstanceOfActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::InstanceOfActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::EqualsActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::EqualsAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::EqualsActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::EqualsActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetPropertyIdFromSymbolTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::GetPropertyIdFromSymbolAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::GetPropertyIdFromSymbolTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::GetPropertyIdFromSymbolTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetPrototypeActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::GetPrototypeAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::GetPrototypeActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::GetPrototypeActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetPropertyActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::GetPropertyAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::GetPropertyActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::GetPropertyActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetIndexActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::GetIndexAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::GetIndexActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::GetIndexActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetOwnPropertyInfoActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::GetOwnPropertyInfoAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::GetOwnPropertyInfoActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::GetOwnPropertyInfoActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetOwnPropertyNamesInfoActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::GetOwnPropertyNamesInfoAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::GetOwnPropertyNamesInfoActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::GetOwnPropertyNamesInfoActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetOwnPropertySymbolsInfoActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::GetOwnPropertySymbolsInfoAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::GetOwnPropertySymbolsInfoActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::GetOwnPropertySymbolsInfoActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::DefinePropertyActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::DefinePropertyAction_Execute, nullptr, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Emit<NSLogEvents::EventKind::DefinePropertyActionTag>, NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction_Parse<NSLogEvents::EventKind::DefinePropertyActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::DeletePropertyActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::DeletePropertyAction_Execute, nullptr, NSLogEvents::JsRTVarsWithBoolAndPIDArgumentAction_Emit<NSLogEvents::EventKind::DeletePropertyActionTag>, NSLogEvents::JsRTVarsWithBoolAndPIDArgumentAction_Parse<NSLogEvents::EventKind::DeletePropertyActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::SetPrototypeActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::SetPrototypeAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::SetPrototypeActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::SetPrototypeActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::SetPropertyActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::SetPropertyAction_Execute, nullptr, NSLogEvents::JsRTVarsWithBoolAndPIDArgumentAction_Emit<NSLogEvents::EventKind::SetPropertyActionTag>, NSLogEvents::JsRTVarsWithBoolAndPIDArgumentAction_Parse<NSLogEvents::EventKind::SetPropertyActionTag> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::SetIndexActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::SetIndexAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::SetIndexActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::SetIndexActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::GetTypedArrayInfoActionTag] = { NSLogEvents::ContextExecuteKind::None, NSLogEvents::GetTypedArrayInfoAction_Execute, nullptr, NSLogEvents::JsRTVarsArgumentAction_Emit<NSLogEvents::EventKind::GetTypedArrayInfoActionTag>, NSLogEvents::JsRTVarsArgumentAction_Parse<NSLogEvents::EventKind::GetTypedArrayInfoActionTag> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::RawBufferCopySync] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::RawBufferCopySync_Execute, nullptr, NSLogEvents::JsRTRawBufferCopyAction_Emit, NSLogEvents::JsRTRawBufferCopyAction_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::RawBufferModifySync] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::RawBufferModifySync_Execute, NSLogEvents::JsRTRawBufferModifyAction_UnloadEventMemory<NSLogEvents::EventKind::RawBufferModifySync>, NSLogEvents::JsRTRawBufferModifyAction_Emit<NSLogEvents::EventKind::RawBufferModifySync>, NSLogEvents::JsRTRawBufferModifyAction_Parse<NSLogEvents::EventKind::RawBufferModifySync> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::RawBufferAsyncModificationRegister] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::RawBufferAsyncModificationRegister_Execute, NSLogEvents::JsRTRawBufferModifyAction_UnloadEventMemory<NSLogEvents::EventKind::RawBufferAsyncModificationRegister>, NSLogEvents::JsRTRawBufferModifyAction_Emit<NSLogEvents::EventKind::RawBufferAsyncModificationRegister>, NSLogEvents::JsRTRawBufferModifyAction_Parse<NSLogEvents::EventKind::RawBufferAsyncModificationRegister> };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::RawBufferAsyncModifyComplete] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::RawBufferAsyncModifyComplete_Execute, NSLogEvents::JsRTRawBufferModifyAction_UnloadEventMemory<NSLogEvents::EventKind::RawBufferAsyncModifyComplete>, NSLogEvents::JsRTRawBufferModifyAction_Emit<NSLogEvents::EventKind::RawBufferAsyncModifyComplete>, NSLogEvents::JsRTRawBufferModifyAction_Parse<NSLogEvents::EventKind::RawBufferAsyncModifyComplete> };

        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::ConstructCallActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::JsRTConstructCallAction_Execute, NSLogEvents::JsRTConstructCallAction_UnloadEventMemory, NSLogEvents::JsRTConstructCallAction_Emit, NSLogEvents::JsRTConstructCallAction_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CallbackOpActionTag] = { NSLogEvents::ContextExecuteKind::None, NSLogEvents::JsRTCallbackAction_Execute, NSLogEvents::JsRTCallbackAction_UnloadEventMemory, NSLogEvents::JsRTCallbackAction_Emit, NSLogEvents::JsRTCallbackAction_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CodeParseActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper, NSLogEvents::JsRTCodeParseAction_Execute, NSLogEvents::JsRTCodeParseAction_UnloadEventMemory, NSLogEvents::JsRTCodeParseAction_Emit, NSLogEvents::JsRTCodeParseAction_Parse };
        this->m_eventListVTable[(uint32)NSLogEvents::EventKind::CallExistingFunctionActionTag] = { NSLogEvents::ContextExecuteKind::ContextAPIWrapper, NSLogEvents::JsRTCallFunctionAction_Execute, NSLogEvents::JsRTCallFunctionAction_UnloadEventMemory, NSLogEvents::JsRTCallFunctionAction_Emit, NSLogEvents::JsRTCallFunctionAction_Parse };
    }

    EventLog::EventLog(ThreadContext* threadContext)
        : m_threadContext(threadContext), m_eventSlabAllocator(TTD_SLAB_BLOCK_ALLOCATION_SIZE_MID), m_miscSlabAllocator(TTD_SLAB_BLOCK_ALLOCATION_SIZE_SMALL),
        m_eventTimeCtr(0), m_timer(), m_runningFunctionTimeCtr(0), m_topLevelCallbackEventTime(-1), m_hostCallbackId(-1),
        m_eventList(&this->m_eventSlabAllocator), m_eventListVTable(nullptr), m_currentReplayEventIterator(),
        m_callStack(&HeapAllocator::Instance, 32), 
        m_lastReturnLocation(), m_breakOnFirstUserCode(false), m_pendingTTDBP(), m_pendingTTDMoveMode(-1), m_activeBPId(-1), m_shouldRemoveWhenDone(false), m_activeTTDBP(), 
        m_continueBreakPoint(), m_preservedBPCount(0), m_preservedBreakPointSourceScriptArray(nullptr), m_preservedBreakPointLocationArray(nullptr),
#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        m_diagnosticLogger(),
#endif
        m_modeStack(), m_currentMode(TTDMode::Invalid),
        m_snapExtractor(), m_elapsedExecutionTimeSinceSnapshot(0.0),
        m_lastInflateSnapshotTime(-1), m_lastInflateMap(nullptr), m_propertyRecordList(&this->m_miscSlabAllocator),
        m_loadedTopLevelScripts(&this->m_miscSlabAllocator), m_newFunctionTopLevelScripts(&this->m_miscSlabAllocator), m_evalTopLevelScripts(&this->m_miscSlabAllocator)
    {TRACE_IT(43627);
        this->InitializeEventListVTable();

        this->m_modeStack.Push(TTDMode::Invalid);

        Recycler * recycler = threadContext->GetRecycler();
        this->m_propertyRecordPinSet.Root(RecyclerNew(recycler, PropertyRecordPinSet, recycler), recycler);
    }

    EventLog::~EventLog()
    {TRACE_IT(43628);
        this->m_eventList.UnloadEventList(this->m_eventListVTable);

        this->UnloadRetainedData();
    }

    void EventLog::UnloadAllLogData()
    {TRACE_IT(43629);
        this->m_eventList.UnloadEventList(this->m_eventListVTable);
    }

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
    TraceLogger* EventLog::GetTraceLogger()
    {TRACE_IT(43630);
        return &(this->m_diagnosticLogger);
    }
#endif

    void EventLog::InitForTTDRecord()
    {TRACE_IT(43631);
        //pin all the current properties so they don't move/disappear on us
        for(Js::PropertyId pid = TotalNumberOfBuiltInProperties; pid < this->m_threadContext->GetMaxPropertyId(); ++pid)
        {TRACE_IT(43632);
            const Js::PropertyRecord* pRecord = this->m_threadContext->GetPropertyName(pid);
            this->AddPropertyRecord(pRecord);
        }

        this->SetGlobalMode(TTDMode::RecordMode);
    }

    void EventLog::InitForTTDReplay(TTDataIOInfo& iofp, const char* parseUri, size_t parseUriLength, bool debug)
    {TRACE_IT(43633);
        if (debug)
        {TRACE_IT(43634);
            this->SetGlobalMode(TTDMode::DebuggerMode);
        }
        else
        {TRACE_IT(43635);
            this->SetGlobalMode(TTDMode::ReplayMode);
        }

        this->ParseLogInto(iofp, parseUri, parseUriLength);

        Js::PropertyId maxPid = TotalNumberOfBuiltInProperties + 1;
        JsUtil::BaseDictionary<Js::PropertyId, NSSnapType::SnapPropertyRecord*, HeapAllocator> pidMap(&HeapAllocator::Instance);

        for(auto iter = this->m_propertyRecordList.GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43636);
            maxPid = max(maxPid, iter.Current()->PropertyId);
            pidMap.AddNew(iter.Current()->PropertyId, iter.Current());
        }

        for(Js::PropertyId cpid = TotalNumberOfBuiltInProperties; cpid <= maxPid; ++cpid)
        {TRACE_IT(43637);
            NSSnapType::SnapPropertyRecord* spRecord = pidMap.Item(cpid);
            const Js::PropertyRecord* newPropertyRecord = NSSnapType::InflatePropertyRecord(spRecord, this->m_threadContext);

            if(!this->m_propertyRecordPinSet->ContainsKey(const_cast<Js::PropertyRecord*>(newPropertyRecord)))
            {TRACE_IT(43638);
                this->m_propertyRecordPinSet->AddNew(const_cast<Js::PropertyRecord*>(newPropertyRecord));
            }
        }
    }

    void EventLog::SetGlobalMode(TTDMode m)
    {TRACE_IT(43639);
        TTDAssert(m == TTDMode::RecordMode || m == TTDMode::ReplayMode || m == TTDMode::DebuggerMode, "These are the only valid global modes");

        this->m_modeStack.SetAt(0, m);
        this->UpdateComputedMode();
    }

    void EventLog::SetSnapshotOrInflateInProgress(bool flag)
    {TRACE_IT(43640);
        const JsUtil::List<Js::ScriptContext*, HeapAllocator>& contexts = this->m_threadContext->TTDContext->GetTTDContexts();
        for(int32 i = 0; i < contexts.Count(); ++i)
        {TRACE_IT(43641);
            TTDAssert(contexts.Item(i)->TTDSnapshotOrInflateInProgress != flag, "This is not re-entrant!!!");

            contexts.Item(i)->TTDSnapshotOrInflateInProgress = flag;
        }
    }

    void EventLog::PushMode(TTDMode m)
    {TRACE_IT(43642);
        TTDAssert(m == TTDMode::CurrentlyEnabled || m == TTDMode::ExcludedExecutionTTAction || m == TTDMode::ExcludedExecutionDebuggerAction ||
            m == TTDMode::DebuggerSuppressGetter || m == TTDMode::DebuggerSuppressBreakpoints || m == TTDMode::DebuggerLogBreakpoints, "These are the only valid mode modifiers to push");

        this->m_modeStack.Push(m);
        this->UpdateComputedMode();
    }

    void EventLog::PopMode(TTDMode m)
    {TRACE_IT(43643);
        TTDAssert(m == TTDMode::CurrentlyEnabled || m == TTDMode::ExcludedExecutionTTAction || m == TTDMode::ExcludedExecutionDebuggerAction ||
            m == TTDMode::DebuggerSuppressGetter || m == TTDMode::DebuggerSuppressBreakpoints || m == TTDMode::DebuggerLogBreakpoints, "These are the only valid mode modifiers to pop");
        TTDAssert(this->m_modeStack.Peek() == m, "Push/Pop is not matched so something went wrong.");

        this->m_modeStack.Pop();
        this->UpdateComputedMode();
    }

    void EventLog::SetModeFlagsOnContext(Js::ScriptContext* ctx)
    {TRACE_IT(43644);
        TTDMode cm = this->m_currentMode;

        ctx->TTDRecordModeEnabled = (cm & (TTDMode::RecordMode | TTDMode::AnyExcludedMode)) == TTDMode::RecordMode;
        ctx->TTDReplayModeEnabled = (cm & (TTDMode::ReplayMode | TTDMode::AnyExcludedMode)) == TTDMode::ReplayMode;
        ctx->TTDRecordOrReplayModeEnabled = (ctx->TTDRecordModeEnabled | ctx->TTDReplayModeEnabled);

        ctx->TTDShouldPerformRecordAction = (cm & (TTDMode::RecordMode | TTDMode::CurrentlyEnabled | TTDMode::AnyExcludedMode)) == (TTDMode::RecordMode | TTDMode::CurrentlyEnabled);
        ctx->TTDShouldPerformReplayAction = (cm & (TTDMode::ReplayMode | TTDMode::CurrentlyEnabled | TTDMode::AnyExcludedMode)) == (TTDMode::ReplayMode | TTDMode::CurrentlyEnabled);
        ctx->TTDShouldPerformRecordOrReplayAction = (ctx->TTDShouldPerformRecordAction | ctx->TTDShouldPerformReplayAction);

        ctx->TTDShouldPerformDebuggerAction = (cm & (TTDMode::DebuggerMode | TTDMode::CurrentlyEnabled | TTDMode::AnyExcludedMode)) == (TTDMode::DebuggerMode | TTDMode::CurrentlyEnabled);
        ctx->TTDShouldSuppressGetterInvocationForDebuggerEvaluation = (cm & TTDMode::DebuggerSuppressGetter) == TTDMode::DebuggerSuppressGetter;
    }

    void EventLog::GetModesForExplicitContextCreate(bool& inRecord, bool& activelyRecording, bool& inReplay)
    {TRACE_IT(43645);
        inRecord = (this->m_currentMode & (TTDMode::RecordMode | TTDMode::AnyExcludedMode)) == TTDMode::RecordMode;
        activelyRecording = (this->m_currentMode & (TTDMode::RecordMode | TTDMode::CurrentlyEnabled | TTDMode::AnyExcludedMode)) == (TTDMode::RecordMode | TTDMode::CurrentlyEnabled);
        inReplay = (this->m_currentMode & (TTDMode::ReplayMode | TTDMode::AnyExcludedMode)) == TTDMode::ReplayMode;
    }

    bool EventLog::IsDebugModeFlagSet() const
    {TRACE_IT(43646);
        return (this->m_currentMode & TTDMode::DebuggerMode) == TTDMode::DebuggerMode;
    }

    bool EventLog::ShouldDoGetterInvocationSupression() const
    {TRACE_IT(43647);
        return (this->m_currentMode & TTD::TTDMode::DebuggerMode) == TTD::TTDMode::DebuggerMode;
    }

    bool EventLog::ShouldSuppressBreakpointsForTimeTravelMove() const
    {TRACE_IT(43648);
        return (this->m_currentMode & TTD::TTDMode::DebuggerSuppressBreakpoints) == TTD::TTDMode::DebuggerSuppressBreakpoints;
    }

    bool EventLog::ShouldRecordBreakpointsDuringTimeTravelScan() const
    {TRACE_IT(43649);
        return (this->m_currentMode & TTD::TTDMode::DebuggerLogBreakpoints) == TTD::TTDMode::DebuggerLogBreakpoints;
    }

    void EventLog::AddPropertyRecord(const Js::PropertyRecord* record)
    {TRACE_IT(43650);
        this->m_propertyRecordPinSet->AddNew(const_cast<Js::PropertyRecord*>(record));
    }

    const NSSnapValues::TopLevelScriptLoadFunctionBodyResolveInfo* EventLog::AddScriptLoad(Js::FunctionBody* fb, Js::ModuleID moduleId, uint64 sourceContextId, const byte* source, uint32 sourceLen, LoadScriptFlag loadFlag)
    {TRACE_IT(43651);
        NSSnapValues::TopLevelScriptLoadFunctionBodyResolveInfo* fbInfo = this->m_loadedTopLevelScripts.NextOpenEntry();
        uint64 fCount = (this->m_loadedTopLevelScripts.Count() + this->m_newFunctionTopLevelScripts.Count() + this->m_evalTopLevelScripts.Count());
        bool isUtf8 = ((loadFlag & LoadScriptFlag_Utf8Source) == LoadScriptFlag_Utf8Source);

        NSSnapValues::ExtractTopLevelLoadedFunctionBodyInfo(fbInfo, fb, fCount, moduleId, sourceContextId, isUtf8, source, sourceLen, loadFlag, this->m_miscSlabAllocator);

        return fbInfo;
    }

    const NSSnapValues::TopLevelNewFunctionBodyResolveInfo* EventLog::AddNewFunction(Js::FunctionBody* fb, Js::ModuleID moduleId, const char16* source, uint32 sourceLen)
    {TRACE_IT(43652);
        NSSnapValues::TopLevelNewFunctionBodyResolveInfo* fbInfo = this->m_newFunctionTopLevelScripts.NextOpenEntry();
        uint64 fCount = (this->m_loadedTopLevelScripts.Count() + this->m_newFunctionTopLevelScripts.Count() + this->m_evalTopLevelScripts.Count());

        NSSnapValues::ExtractTopLevelNewFunctionBodyInfo(fbInfo, fb, fCount, moduleId, source, sourceLen, this->m_miscSlabAllocator);

        return fbInfo;
    }

    const NSSnapValues::TopLevelEvalFunctionBodyResolveInfo* EventLog::AddEvalFunction(Js::FunctionBody* fb, Js::ModuleID moduleId, const char16* source, uint32 sourceLen, uint32 grfscr, bool registerDocument, BOOL isIndirect, BOOL strictMode)
    {TRACE_IT(43653);
        NSSnapValues::TopLevelEvalFunctionBodyResolveInfo* fbInfo = this->m_evalTopLevelScripts.NextOpenEntry();
        uint64 fCount = (this->m_loadedTopLevelScripts.Count() + this->m_newFunctionTopLevelScripts.Count() + this->m_evalTopLevelScripts.Count());

        NSSnapValues::ExtractTopLevelEvalFunctionBodyInfo(fbInfo, fb, fCount, moduleId, source, sourceLen, grfscr, registerDocument, isIndirect, strictMode, this->m_miscSlabAllocator);

        return fbInfo;
    }

    void EventLog::RecordTopLevelCodeAction(uint64 bodyCtrId)
    {TRACE_IT(43654);
        NSLogEvents::CodeLoadEventLogEntry* clEvent = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::CodeLoadEventLogEntry, NSLogEvents::EventKind::TopLevelCodeTag>();
        clEvent->BodyCounterId = bodyCtrId;
    }

    uint64 EventLog::ReplayTopLevelCodeAction()
    {TRACE_IT(43655);
        const NSLogEvents::CodeLoadEventLogEntry* clEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::CodeLoadEventLogEntry, NSLogEvents::EventKind::TopLevelCodeTag>();

        return clEvent->BodyCounterId;
    }

    void EventLog::RecordTelemetryLogEvent(Js::JavascriptString* infoStringJs, bool doPrint)
    {TRACE_IT(43656);
        NSLogEvents::TelemetryEventLogEntry* tEvent = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::TelemetryEventLogEntry, NSLogEvents::EventKind::TelemetryLogTag>();
        this->m_eventSlabAllocator.CopyStringIntoWLength(infoStringJs->GetSz(), infoStringJs->GetLength(), tEvent->InfoString);
        tEvent->DoPrint = doPrint;

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.ForceFlush();
#endif
    }

    void EventLog::ReplayTelemetryLogEvent(Js::JavascriptString* infoStringJs)
    {TRACE_IT(43657);
#if !ENABLE_TTD_INTERNAL_DIAGNOSTICS
        this->AdvanceTimeAndPositionForReplay(); //just eat the telemetry event
#else
        const NSLogEvents::TelemetryEventLogEntry* tEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::TelemetryEventLogEntry, NSLogEvents::EventKind::TelemetryLogTag>();

        uint32 infoStrLength = (uint32)infoStringJs->GetLength();
        const char16* infoStr = infoStringJs->GetSz();

        if(tEvent->InfoString.Length != infoStrLength)
        {TRACE_IT(43658);
            wprintf(_u("New Telemetry Msg: %ls\n"), infoStr);
            wprintf(_u("Original Telemetry Msg: %ls\n"), tEvent->InfoString.Contents);
            TTDAssert(false, "Telemetry messages differ??");
        }
        else
        {TRACE_IT(43659);
            for(uint32 i = 0; i < infoStrLength; ++i)
            {TRACE_IT(43660);
                if(tEvent->InfoString.Contents[i] != infoStr[i])
                {TRACE_IT(43661);
                    wprintf(_u("New Telemetry Msg: %ls\n"), infoStr);
                    wprintf(_u("Original Telemetry Msg: %ls\n"), tEvent->InfoString.Contents);
                    TTDAssert(false, "Telemetry messages differ??");

                    break;
                }
            }
        }
#endif

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.ForceFlush();
#endif
    }

    void EventLog::RecordEmitLogEvent(Js::JavascriptString* uriString)
    {TRACE_IT(43662);
        this->RecordGetInitializedEvent_DataOnly<void*, NSLogEvents::EventKind::ExplicitLogWriteTag>();

        AutoArrayPtr<char> uri(HeapNewArrayZ(char, uriString->GetLength() * 3), uriString->GetLength() * 3);
        size_t uriLength = utf8::EncodeInto((LPUTF8)((char*)uri), uriString->GetSz(), uriString->GetLength());

        this->EmitLog(uri, uriLength);
    }

    void EventLog::ReplayEmitLogEvent()
    {TRACE_IT(43663);
        this->ReplayGetReplayEvent_Helper<void*, NSLogEvents::EventKind::ExplicitLogWriteTag>();

        //check if at end of log -- if so we are done and don't want to execute any more
        if(!this->m_currentReplayEventIterator.IsValid())
        {TRACE_IT(43664);
            this->AbortReplayReturnToHost();
        }
    }

    void EventLog::RecordDateTimeEvent(double time)
    {TRACE_IT(43665);
        NSLogEvents::DoubleEventLogEntry* dEvent = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::DoubleEventLogEntry, NSLogEvents::EventKind::DoubleTag>();
        dEvent->DoubleValue = time;
    }

    void EventLog::RecordDateStringEvent(Js::JavascriptString* stringValue)
    {TRACE_IT(43666);
        NSLogEvents::StringValueEventLogEntry* sEvent = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::StringValueEventLogEntry, NSLogEvents::EventKind::StringTag>();
        this->m_eventSlabAllocator.CopyStringIntoWLength(stringValue->GetSz(), stringValue->GetLength(), sEvent->StringValue);
    }

    void EventLog::ReplayDateTimeEvent(double* result)
    {TRACE_IT(43667);
        const NSLogEvents::DoubleEventLogEntry* dEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::DoubleEventLogEntry, NSLogEvents::EventKind::DoubleTag>();
        *result = dEvent->DoubleValue;
    }

    void EventLog::ReplayDateStringEvent(Js::ScriptContext* ctx, Js::JavascriptString** result)
    {TRACE_IT(43668);
        const NSLogEvents::StringValueEventLogEntry* sEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::StringValueEventLogEntry, NSLogEvents::EventKind::StringTag>();

        const TTString& str = sEvent->StringValue;
        *result = Js::JavascriptString::NewCopyBuffer(str.Contents, str.Length, ctx);
    }

    void EventLog::RecordExternalEntropyRandomEvent(uint64 seed0, uint64 seed1)
    {TRACE_IT(43669);
        NSLogEvents::RandomSeedEventLogEntry* rsEvent = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::RandomSeedEventLogEntry, NSLogEvents::EventKind::RandomSeedTag>();
        rsEvent->Seed0 = seed0;
        rsEvent->Seed1 = seed1;
    }

    void EventLog::ReplayExternalEntropyRandomEvent(uint64* seed0, uint64* seed1)
    {TRACE_IT(43670);
        const NSLogEvents::RandomSeedEventLogEntry* rsEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::RandomSeedEventLogEntry, NSLogEvents::EventKind::RandomSeedTag>();
        *seed0 = rsEvent->Seed0;
        *seed1 = rsEvent->Seed1;
    }

    void EventLog::RecordPropertyEnumEvent(BOOL returnCode, Js::PropertyId pid, Js::PropertyAttributes attributes, Js::JavascriptString* propertyName)
    {TRACE_IT(43671);
        //When we replay we can just skip this pid cause it should never matter -- but if return code is false then we need to record the "at end" info
        if(returnCode && Js::IsInternalPropertyId(pid))
        {TRACE_IT(43672);
            return;
        }

        NSLogEvents::PropertyEnumStepEventLogEntry* peEvent = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::PropertyEnumStepEventLogEntry, NSLogEvents::EventKind::PropertyEnumTag>();
        peEvent->ReturnCode = returnCode;
        peEvent->Pid = pid;
        peEvent->Attributes = attributes;

        InitializeAsNullPtrTTString(peEvent->PropertyString);
#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        if(returnCode)
        {TRACE_IT(43673);
            this->m_eventSlabAllocator.CopyStringIntoWLength(propertyName->GetSz(), propertyName->GetLength(), peEvent->PropertyString);
        }
#else
        if(returnCode && pid == Js::Constants::NoProperty)
        {TRACE_IT(43674);
            this->m_eventSlabAllocator.CopyStringIntoWLength(propertyName->GetSz(), propertyName->GetLength(), peEvent->PropertyString);
        }
#endif

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteEnumAction(this->m_eventTimeCtr - 1, returnCode, pid, attributes, propertyName);
#endif
    }

    void EventLog::ReplayPropertyEnumEvent(Js::ScriptContext* requestContext, BOOL* returnCode, Js::BigPropertyIndex* newIndex, const Js::DynamicObject* obj, Js::PropertyId* pid, Js::PropertyAttributes* attributes, Js::JavascriptString** propertyName)
    {TRACE_IT(43675);
        const NSLogEvents::PropertyEnumStepEventLogEntry* peEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::PropertyEnumStepEventLogEntry, NSLogEvents::EventKind::PropertyEnumTag>();

        *returnCode = peEvent->ReturnCode;
        *pid = peEvent->Pid;
        *attributes = peEvent->Attributes;

        if(*returnCode)
        {TRACE_IT(43676);
            TTDAssert(*pid != Js::Constants::NoProperty, "This is so weird we need to figure out what this means.");
            TTDAssert(!Js::IsInternalPropertyId(*pid), "We should skip recording this.");

            Js::PropertyString* propertyString = requestContext->GetPropertyString(*pid);
            *propertyName = propertyString;

            const Js::PropertyRecord* pRecord = requestContext->GetPropertyName(*pid);
            *newIndex = obj->GetDynamicType()->GetTypeHandler()->GetPropertyIndex_EnumerateTTD(pRecord);

            TTDAssert(*newIndex != Js::Constants::NoBigSlot, "If *returnCode is true then we found it during record -- but missing in replay.");
        }
        else
        {TRACE_IT(43677);
            *propertyName = nullptr;

            *newIndex = obj->GetDynamicType()->GetTypeHandler()->GetPropertyCount();
        }

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteEnumAction(this->m_eventTimeCtr - 1, *returnCode, *pid, *attributes, *propertyName);
#endif
    }

    void EventLog::RecordSymbolCreationEvent(Js::PropertyId pid)
    {TRACE_IT(43678);
        NSLogEvents::SymbolCreationEventLogEntry* scEvent = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::SymbolCreationEventLogEntry, NSLogEvents::EventKind::SymbolCreationTag>();
        scEvent->Pid = pid;
    }

    void EventLog::ReplaySymbolCreationEvent(Js::PropertyId* pid)
    {TRACE_IT(43679);
        const NSLogEvents::SymbolCreationEventLogEntry* scEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::SymbolCreationEventLogEntry, NSLogEvents::EventKind::SymbolCreationTag>();
        *pid = scEvent->Pid;
    }

    NSLogEvents::EventLogEntry* EventLog::RecordExternalCallEvent(Js::JavascriptFunction* func, int32 rootDepth, uint32 argc, Js::Var* argv, bool checkExceptions)
    {TRACE_IT(43680);
        NSLogEvents::ExternalCallEventLogEntry* ecEvent = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::ExternalCallEventLogEntry, NSLogEvents::EventKind::ExternalCallTag>(&ecEvent);

        //We never fail with an exception (instead we set the HasRecordedException in script context)
        evt->ResultStatus = 0;

        NSLogEvents::ExternalCallEventLogEntry_ProcessArgs(evt, rootDepth, func, argc, argv, checkExceptions, this->m_eventSlabAllocator);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        NSLogEvents::ExternalCallEventLogEntry_ProcessDiagInfoPre(evt, func, this->m_eventSlabAllocator);
#endif

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteCall(func, true, argc, argv, this->GetLastEventTime());
#endif

        return evt;
    }

    void EventLog::RecordExternalCallEvent_Complete(Js::JavascriptFunction* efunction, NSLogEvents::EventLogEntry* evt, Js::Var result)
    {TRACE_IT(43681);
        NSLogEvents::ExternalCallEventLogEntry_ProcessReturn(evt, result, this->GetLastEventTime());

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteReturn(efunction, result, this->GetLastEventTime());
#endif
    }

    void EventLog::ReplayExternalCallEvent(Js::JavascriptFunction* function, uint32 argc, Js::Var* argv, Js::Var* result)
    {TRACE_IT(43682);
        TTDAssert(result != nullptr, "Must be non-null!!!");
        TTDAssert(*result == nullptr, "And initialized to a default value.");

        const NSLogEvents::ExternalCallEventLogEntry* ecEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::ExternalCallEventLogEntry, NSLogEvents::EventKind::ExternalCallTag>();

        Js::ScriptContext* ctx = function->GetScriptContext();
        TTDAssert(ctx != nullptr, "Not sure how this would be possible but check just in case.");

        ThreadContextTTD* executeContext = ctx->GetThreadContext()->TTDContext;

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteCall(function, true, argc, argv, this->GetLastEventTime());
#endif

        //make sure we log all of the passed arguments in the replay host
        TTDAssert(argc + 1 == ecEvent->ArgCount, "Mismatch in args!!!");

        TTDVar recordedFunction = ecEvent->ArgArray[0];
        NSLogEvents::PassVarToHostInReplay(executeContext, recordedFunction, function);

        for(uint32 i = 0; i < argc; ++i)
        {TRACE_IT(43683);
            Js::Var replayVar = argv[i];
            TTDVar recordedVar = ecEvent->ArgArray[i + 1];
            NSLogEvents::PassVarToHostInReplay(executeContext, recordedVar, replayVar);
        }

        //replay anything that happens in the external call
        BEGIN_LEAVE_SCRIPT(ctx)
        {TRACE_IT(43684);
            this->ReplayActionEventSequenceThroughTime(ecEvent->AdditionalInfo->LastNestedEventTime);
        }
        END_LEAVE_SCRIPT(ctx);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        TTDAssert(!this->m_currentReplayEventIterator.IsValid() || this->m_currentReplayEventIterator.Current()->EventTimeStamp == this->m_eventTimeCtr, "Out of Sync!!!");
#endif

        *result = NSLogEvents::InflateVarInReplay(executeContext, ecEvent->ReturnValue);

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteReturn(function, *result, this->GetLastEventTime());
#endif

        //if we had exception info then we need to patch it up and do what the external call did
        if(ecEvent->AdditionalInfo->CheckExceptionStatus)
        {TRACE_IT(43685);
            if(ctx->HasRecordedException())
            {TRACE_IT(43686);
                bool considerPassingToDebugger = false;
                Js::JavascriptExceptionObject* recordedException = ctx->GetAndClearRecordedException(&considerPassingToDebugger);
                if(recordedException != nullptr)
                {TRACE_IT(43687);
                    // If this is script termination, then throw ScriptAbortExceptio, else throw normal Exception object.
                    if(recordedException == ctx->GetThreadContext()->GetPendingTerminatedErrorObject())
                    {TRACE_IT(43688);
                        throw Js::ScriptAbortException();
                    }
                    else
                    {TRACE_IT(43689);
                        Js::JavascriptExceptionOperators::RethrowExceptionObject(recordedException, ctx, considerPassingToDebugger);
                    }
                }
            }
        }

        if(*result == nullptr)
        {TRACE_IT(43690);
            *result = ctx->GetLibrary()->GetUndefined();
        }
        else
        {TRACE_IT(43691);
            *result = Js::CrossSite::MarshalVar(ctx, *result);
        }
    }

    NSLogEvents::EventLogEntry* EventLog::RecordEnqueueTaskEvent(Js::Var taskVar)
    {TRACE_IT(43692);
        NSLogEvents::ExternalCbRegisterCallEventLogEntry* ecEvent = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::ExternalCbRegisterCallEventLogEntry, NSLogEvents::EventKind::ExternalCbRegisterCall>(&ecEvent);

        ecEvent->CallbackFunction = static_cast<TTDVar>(taskVar);
        ecEvent->LastNestedEventTime = TTD_EVENT_MAXTIME;

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteLiteralMsg("Enqueue Task: ");
        this->m_diagnosticLogger.WriteVar(taskVar);
#endif

        return evt;
    }

    void EventLog::RecordEnqueueTaskEvent_Complete(NSLogEvents::EventLogEntry* evt)
    {TRACE_IT(43693);
        NSLogEvents::ExternalCbRegisterCallEventLogEntry* ecEvent = NSLogEvents::GetInlineEventDataAs<NSLogEvents::ExternalCbRegisterCallEventLogEntry, NSLogEvents::EventKind::ExternalCbRegisterCall>(evt);

        ecEvent->LastNestedEventTime = this->GetLastEventTime();
    }

    void EventLog::ReplayEnqueueTaskEvent(Js::ScriptContext* ctx, Js::Var taskVar)
    {TRACE_IT(43694);
        const NSLogEvents::ExternalCbRegisterCallEventLogEntry* ecEvent = this->ReplayGetReplayEvent_Helper<NSLogEvents::ExternalCbRegisterCallEventLogEntry, NSLogEvents::EventKind::ExternalCbRegisterCall>();
        ThreadContextTTD* executeContext = ctx->GetThreadContext()->TTDContext;

        NSLogEvents::PassVarToHostInReplay(executeContext, ecEvent->CallbackFunction, taskVar);

        //replay anything that happens when we are out of the call
        BEGIN_LEAVE_SCRIPT(ctx)
        {TRACE_IT(43695);
            this->ReplayActionEventSequenceThroughTime(ecEvent->LastNestedEventTime);
        }
        END_LEAVE_SCRIPT(ctx);
    }

    void EventLog::PushCallEvent(Js::JavascriptFunction* function, uint32 argc, Js::Var* argv, bool isInFinally)
    {TRACE_IT(43696);
        //Clear any previous last return frame info
        this->m_lastReturnLocation.ClearReturnOnly();

        this->m_runningFunctionTimeCtr++;

        SingleCallCounter cfinfo;
        cfinfo.Function = function->GetFunctionBody();

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        cfinfo.Name = cfinfo.Function->GetExternalDisplayName();
#endif

        cfinfo.EventTime = this->m_eventTimeCtr; //don't need to advance just note what the event time was when this is called
        cfinfo.FunctionTime = this->m_runningFunctionTimeCtr;
        cfinfo.LoopTime = 0;

        cfinfo.CurrentStatementIndex = -1;
        cfinfo.CurrentStatementLoopTime = 0;

        cfinfo.LastStatementIndex = -1;
        cfinfo.LastStatementLoopTime = 0;

        cfinfo.CurrentStatementBytecodeMin = UINT32_MAX;
        cfinfo.CurrentStatementBytecodeMax = UINT32_MAX;

        this->m_callStack.Add(cfinfo);

        ////
        //If we are running for debugger then check if we need to set a breakpoint at entry to the first function we execute
        Js::FunctionBody* functionBody = function->GetFunctionBody();
        Js::Utf8SourceInfo* utf8SourceInfo = functionBody->GetUtf8SourceInfo();

        if(this->m_breakOnFirstUserCode)
        {TRACE_IT(43697);
            this->m_breakOnFirstUserCode = false;

            Js::DebugDocument* debugDocument = utf8SourceInfo->GetDebugDocument();
            if(debugDocument != nullptr && SUCCEEDED(utf8SourceInfo->EnsureLineOffsetCacheNoThrow()))
            {TRACE_IT(43698);
                ULONG lineNumber = functionBody->GetLineNumber();
                ULONG columnNumber = functionBody->GetColumnNumber();
                uint startOffset = functionBody->GetStatementStartOffset(0);
                ULONG firstStatementLine;
                LONG firstStatementColumn;

                functionBody->GetLineCharOffsetFromStartChar(startOffset, &firstStatementLine, &firstStatementColumn);

                charcount_t charPosition = 0;
                charcount_t byteOffset = 0;
                utf8SourceInfo->GetCharPositionForLineInfo(lineNumber, &charPosition, &byteOffset);
                long ibos = charPosition + columnNumber + 1;

                Js::StatementLocation statement;
                debugDocument->GetStatementLocation(ibos, &statement);

                // Don't see a use case for supporting multiple breakpoints at same location.
                // If a breakpoint already exists, just return that
                Js::BreakpointProbe* probe = debugDocument->FindBreakpoint(statement);
                bool isNewBP = (probe == nullptr);

                if(probe == nullptr)
                {TRACE_IT(43699);
                    probe = debugDocument->SetBreakPoint(statement, BREAKPOINT_ENABLED);
                }

                TTDebuggerSourceLocation bpLocation;
                bpLocation.SetLocation(-1, -1, -1, cfinfo.Function, firstStatementLine, firstStatementColumn);

                function->GetScriptContext()->GetThreadContext()->TTDLog->SetActiveBP(probe->GetId(), isNewBP, bpLocation);
            }
        }
        ////

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteCall(function, false, argc, argv, this->m_eventTimeCtr);
#endif
    }

    void EventLog::PopCallEvent(Js::JavascriptFunction* function, Js::Var result)
    {TRACE_IT(43700);
        this->m_lastReturnLocation.SetReturnLocation(this->m_callStack.Last());

        this->m_runningFunctionTimeCtr++;
        this->m_callStack.RemoveAtEnd();

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteReturn(function, result, this->m_eventTimeCtr);
#endif
    }

    void EventLog::PopCallEventException(Js::JavascriptFunction* function)
    {TRACE_IT(43701);
        //If we already have the last return as an exception then just leave it.
        //That is where the exception was first rasied, this return is just propagating it in this return.

        if(!this->m_lastReturnLocation.IsExceptionLocation())
        {TRACE_IT(43702);
            this->m_lastReturnLocation.SetExceptionLocation(this->m_callStack.Last());
        }

        this->m_runningFunctionTimeCtr++;
        this->m_callStack.RemoveAtEnd();

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteReturnException(function, this->m_eventTimeCtr);
#endif
    }

    void EventLog::ClearExceptionFrames()
    {TRACE_IT(43703);
        this->m_lastReturnLocation.Clear();
    }

    void EventLog::SetBreakOnFirstUserCode()
    {TRACE_IT(43704);
        this->m_breakOnFirstUserCode = true;
    }

    bool EventLog::HasPendingTTDBP() const
    {TRACE_IT(43705);
        return this->m_pendingTTDBP.HasValue();
    }

    int64 EventLog::GetPendingTTDBPTargetEventTime() const
    {TRACE_IT(43706);
        return this->m_pendingTTDBP.GetRootEventTime();
    }

    void EventLog::GetPendingTTDBPInfo(TTDebuggerSourceLocation& BPLocation) const
    {TRACE_IT(43707);
        BPLocation.SetLocation(this->m_pendingTTDBP);
    }

    void EventLog::ClearPendingTTDBPInfo()
    {TRACE_IT(43708);
        this->m_pendingTTDBP.Clear();
    }

    void EventLog::SetPendingTTDBPInfo(const TTDebuggerSourceLocation& BPLocation)
    {TRACE_IT(43709);
        this->m_pendingTTDBP.SetLocation(BPLocation);
    }

    void EventLog::EnsureTTDBPInfoTopLevelBodyCtrPreInflate()
    {TRACE_IT(43710);
        this->m_pendingTTDBP.EnsureTopLevelBodyCtrPreInflate();
    }

    int64 EventLog::GetPendingTTDMoveMode() const
    {TRACE_IT(43711);
        return this->m_pendingTTDMoveMode;
    }

    void EventLog::ClearPendingTTDMoveMode()
    {TRACE_IT(43712);
        this->m_pendingTTDMoveMode = -1;
    }

    void EventLog::SetPendingTTDMoveMode(int64 mode)
    {TRACE_IT(43713);
        this->m_pendingTTDMoveMode = mode;
    }

    bool EventLog::HasActiveBP() const
    {TRACE_IT(43714);
        return this->m_activeBPId != -1;
    }

    UINT EventLog::GetActiveBPId() const
    {TRACE_IT(43715);
        TTDAssert(this->HasActiveBP(), "Should check this first!!!");

        return (UINT)this->m_activeBPId;
    }

    void EventLog::ClearActiveBP()
    {TRACE_IT(43716);
        this->m_activeBPId = -1;
        this->m_shouldRemoveWhenDone = false;
        this->m_activeTTDBP.Clear();
    }

    void EventLog::SetActiveBP(UINT bpId, bool isNewBP, const TTDebuggerSourceLocation& bpLocation)
    {TRACE_IT(43717);
        this->m_activeBPId = bpId;
        this->m_shouldRemoveWhenDone = isNewBP;
        this->m_activeTTDBP.SetLocation(bpLocation);
    }

    bool EventLog::ProcessBPInfoPreBreak(Js::FunctionBody* fb)
    {TRACE_IT(43718);
        //if we aren't in debug mode then we always trigger BP's
        if(!fb->GetScriptContext()->ShouldPerformDebuggerAction())
        {TRACE_IT(43719);
            return true;
        }

        //If we are in debugger mode but are suppressing BP's for movement then suppress them
        if(this->ShouldSuppressBreakpointsForTimeTravelMove())
        {TRACE_IT(43720);
            //Check if we need to record the visit to this bp
            if(this->ShouldRecordBreakpointsDuringTimeTravelScan())
            {TRACE_IT(43721);
                this->AddCurrentLocationDuringScan();
            }

            return false;
        }

        //If we are in debug mode and don't have an active BP target then we treat BP's as usual
        if(!this->HasActiveBP())
        {TRACE_IT(43722);
            return true;
        }

        //Finally we are in debug mode and we have an active BP target so only break if the BP is satisfied
        const SingleCallCounter& cfinfo = this->GetTopCallCounter();
        ULONG srcLine = 0;
        LONG srcColumn = -1;
        uint32 startOffset = cfinfo.Function->GetStatementStartOffset(cfinfo.CurrentStatementIndex);
        cfinfo.Function->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

        bool locationOk = ((uint32)srcLine == this->m_activeTTDBP.GetLine()) & ((uint32)srcColumn == this->m_activeTTDBP.GetColumn());
        bool ftimeOk = (this->m_activeTTDBP.GetFunctionTime() == -1) | ((uint64)this->m_activeTTDBP.GetFunctionTime() == cfinfo.FunctionTime);
        bool ltimeOk = (this->m_activeTTDBP.GetLoopTime() == -1) | ((uint64)this->m_activeTTDBP.GetLoopTime() == cfinfo.CurrentStatementLoopTime);

        return locationOk & ftimeOk & ltimeOk;
    }

    void EventLog::ProcessBPInfoPostBreak(Js::FunctionBody* fb)
    {TRACE_IT(43723);
        if(!fb->GetScriptContext()->ShouldPerformDebuggerAction())
        {TRACE_IT(43724);
            return;
        }

        if(this->HasActiveBP())
        {TRACE_IT(43725);
            Js::DebugDocument* debugDocument = fb->GetUtf8SourceInfo()->GetDebugDocument();
            Js::StatementLocation statement;
            if(this->m_shouldRemoveWhenDone && debugDocument->FindBPStatementLocation(this->GetActiveBPId(), &statement))
            {TRACE_IT(43726);
                debugDocument->SetBreakPoint(statement, BREAKPOINT_DELETED);
            }

            this->ClearActiveBP();
        }

        if(this->HasPendingTTDBP())
        {TRACE_IT(43727);
            //Reset any step controller logic
            fb->GetScriptContext()->GetThreadContext()->GetDebugManager()->stepController.Deactivate();

            throw TTD::TTDebuggerAbortException::CreateTopLevelAbortRequest(this->GetPendingTTDBPTargetEventTime(), this->GetPendingTTDMoveMode(), _u("Reverse operation requested."));
        }
    }

    void EventLog::ClearBPScanInfo()
    {TRACE_IT(43728);
        this->m_continueBreakPoint.Clear();
    }

    void EventLog::AddCurrentLocationDuringScan()
    {TRACE_IT(43729);
        TTDebuggerSourceLocation current(this->m_topLevelCallbackEventTime, this->m_callStack.Last());
        if(this->m_pendingTTDBP.HasValue() && current.IsBefore(this->m_pendingTTDBP))
        {TRACE_IT(43730);
            this->m_continueBreakPoint.SetLocation(current);
        }
    }

    bool EventLog::TryFindAndSetPreviousBP()
    {TRACE_IT(43731);
        TTDAssert(this->m_pendingTTDBP.HasValue(), "This needs to have a value!!!");

        if(!this->m_continueBreakPoint.HasValue())
        {TRACE_IT(43732);
            return false;
        }
        else
        {TRACE_IT(43733);
            TTDAssert(this->m_continueBreakPoint.IsBefore(this->m_pendingTTDBP), "How did this happen?");

            this->m_pendingTTDBP.SetLocation(this->m_continueBreakPoint);
            return true;
        }
    }

    void EventLog::LoadPreservedBPInfo()
    {TRACE_IT(43734);
        //Unload this before we move again
        TTDAssert(this->m_preservedBPCount == 0, "This should always be clear???");

        uint32 bpCount = 0;
        const JsUtil::List<Js::ScriptContext*, HeapAllocator>& ctxs = this->m_threadContext->TTDContext->GetTTDContexts();
        for(int32 i = 0; i < ctxs.Count(); ++i)
        {TRACE_IT(43735);
            Js::ProbeContainer* probeContainer = ctxs.Item(i)->GetDebugContext()->GetProbeContainer();
            probeContainer->MapProbes([&](int j, Js::Probe* pProbe)
            {
                Js::BreakpointProbe* bp = (Js::BreakpointProbe*)pProbe;
                if((int64)bp->GetId() != this->m_activeBPId)
                {TRACE_IT(43736);
                    bpCount++;
                }
            });
        }

        if(bpCount != 0)
        {TRACE_IT(43737);
            this->m_preservedBreakPointSourceScriptArray = TT_HEAP_ALLOC_ARRAY_ZERO(TTD_LOG_PTR_ID, bpCount);
            this->m_preservedBreakPointLocationArray = TT_HEAP_ALLOC_ARRAY_ZERO(TTDebuggerSourceLocation*, bpCount);

            for(int32 i = 0; i < ctxs.Count(); ++i)
            {TRACE_IT(43738);
                Js::ProbeContainer* probeContainer = ctxs.Item(i)->GetDebugContext()->GetProbeContainer();
                probeContainer->MapProbes([&](int j, Js::Probe* pProbe)
                {
                    Js::BreakpointProbe* bp = (Js::BreakpointProbe*)pProbe;
                    if((int64)bp->GetId() != this->m_activeBPId)
                    {TRACE_IT(43739);
                        Js::FunctionBody* body = bp->GetFunctionBody();
                        int32 bpIndex = body->GetEnclosingStatementIndexFromByteCode(bp->GetBytecodeOffset());

                        ULONG srcLine = 0;
                        LONG srcColumn = -1;
                        uint32 startOffset = body->GetStatementStartOffset(bpIndex);
                        body->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

                        this->m_preservedBreakPointSourceScriptArray[this->m_preservedBPCount] = ctxs.Item(i)->ScriptContextLogTag;

                        this->m_preservedBreakPointLocationArray[this->m_preservedBPCount] = TT_HEAP_NEW(TTDebuggerSourceLocation);
                        this->m_preservedBreakPointLocationArray[this->m_preservedBPCount]->SetLocation(-1, -1, -1, body, srcLine, srcColumn);

                        this->m_preservedBPCount++;
                    }
                });
            }
        }

        TTDAssert(this->m_preservedBPCount == bpCount, "Something is wrong!!!");
    }

    void EventLog::UnLoadPreservedBPInfo()
    {TRACE_IT(43740);
        if(this->m_preservedBPCount != 0)
        {
            TT_HEAP_FREE_ARRAY(TTD_LOG_PTR_ID, this->m_preservedBreakPointSourceScriptArray, this->m_preservedBPCount);
            this->m_preservedBreakPointSourceScriptArray = nullptr;

            for(uint32 i = 0; i < this->m_preservedBPCount; ++i)
            {
                TT_HEAP_DELETE(TTDebuggerSourceLocation, this->m_preservedBreakPointLocationArray[i]);
            }
            TT_HEAP_FREE_ARRAY(TTDebuggerSourceLocation*, this->m_preservedBreakPointLocationArray, this->m_preservedBPCount);
            this->m_preservedBreakPointLocationArray = nullptr;

            this->m_preservedBPCount = 0;
        }
    }

    const uint32 EventLog::GetPerservedBPInfoCount() const
    {TRACE_IT(43741);
        return this->m_preservedBPCount;
    }

    TTD_LOG_PTR_ID* EventLog::GetPerservedBPInfoScriptArray()
    {TRACE_IT(43742);
        return this->m_preservedBreakPointSourceScriptArray;
    }

    TTDebuggerSourceLocation** EventLog::GetPerservedBPInfoLocationArray()
    {TRACE_IT(43743);
        return this->m_preservedBreakPointLocationArray;
    }

    void EventLog::UpdateLoopCountInfo()
    {TRACE_IT(43744);
        SingleCallCounter& cfinfo = this->m_callStack.Last();
        cfinfo.LoopTime++;
    }

    void EventLog::UpdateCurrentStatementInfo(uint bytecodeOffset)
    {TRACE_IT(43745);
        SingleCallCounter& cfinfo = this->GetTopCallCounter();

        if((cfinfo.CurrentStatementBytecodeMin <= bytecodeOffset) & (bytecodeOffset <= cfinfo.CurrentStatementBytecodeMax))
        {TRACE_IT(43746);
            return;
        }
        else
        {TRACE_IT(43747);
            Js::FunctionBody* fb = cfinfo.Function;

            int32 cIndex = fb->GetEnclosingStatementIndexFromByteCode(bytecodeOffset, true);
            TTDAssert(cIndex != -1, "Should always have a mapping.");

            //we moved to a new statement
            Js::FunctionBody::StatementMap* pstmt = fb->GetStatementMaps()->Item(cIndex);
            bool newstmt = (cIndex != cfinfo.CurrentStatementIndex && pstmt->byteCodeSpan.begin <= (int)bytecodeOffset && (int)bytecodeOffset <= pstmt->byteCodeSpan.end);
            if(newstmt)
            {TRACE_IT(43748);
                cfinfo.LastStatementIndex = cfinfo.CurrentStatementIndex;
                cfinfo.LastStatementLoopTime = cfinfo.CurrentStatementLoopTime;

                cfinfo.CurrentStatementIndex = cIndex;
                cfinfo.CurrentStatementLoopTime = cfinfo.LoopTime;

                cfinfo.CurrentStatementBytecodeMin = (uint32)pstmt->byteCodeSpan.begin;
                cfinfo.CurrentStatementBytecodeMax = (uint32)pstmt->byteCodeSpan.end;

#if ENABLE_FULL_BC_TRACE
                ULONG srcLine = 0;
                LONG srcColumn = -1;
                uint32 startOffset = cfinfo.Function->GetFunctionBody()->GetStatementStartOffset(cfinfo.CurrentStatementIndex);
                cfinfo.Function->GetFunctionBody()->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

                this->m_diagnosticLogger.WriteStmtIndex((uint32)srcLine, (uint32)srcColumn);
#endif
            }
        }
    }

    void EventLog::GetTimeAndPositionForDebugger(TTDebuggerSourceLocation& sourceLocation) const
    {TRACE_IT(43749);
        const SingleCallCounter& cfinfo = this->GetTopCallCounter();

        ULONG srcLine = 0;
        LONG srcColumn = -1;
        uint32 startOffset = cfinfo.Function->GetStatementStartOffset(cfinfo.CurrentStatementIndex);
        cfinfo.Function->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

        sourceLocation.SetLocation(this->m_topLevelCallbackEventTime, cfinfo.FunctionTime, cfinfo.LoopTime, cfinfo.Function, srcLine, srcColumn);
    }

#if ENABLE_OBJECT_SOURCE_TRACKING
    void EventLog::GetTimeAndPositionForDiagnosticObjectTracking(DiagnosticOrigin& originInfo) const
    {TRACE_IT(43750);
        const SingleCallCounter& cfinfo = this->GetTopCallCounter();

        ULONG srcLine = 0;
        LONG srcColumn = -1;
        uint32 startOffset = cfinfo.Function->GetStatementStartOffset(cfinfo.CurrentStatementIndex);
        cfinfo.Function->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

        SetDiagnosticOriginInformation(originInfo, srcLine, cfinfo.EventTime, cfinfo.FunctionTime, cfinfo.LoopTime);
    }
#endif

    bool EventLog::GetPreviousTimeAndPositionForDebugger(TTDebuggerSourceLocation& sourceLocation) const
    {TRACE_IT(43751);
        bool noPrevious = false;
        const SingleCallCounter& cfinfo = this->GetTopCallCounter();

        //if we are at the first statement in the function then we want the parents current
        Js::FunctionBody* fbody = nullptr;
        int32 statementIndex = -1;
        uint64 ftime = 0;
        uint64 ltime = 0;
        if(cfinfo.LastStatementIndex == -1)
        {TRACE_IT(43752);
            SingleCallCounter cfinfoCaller = { 0 }; 
            bool hasCaller = this->TryGetTopCallCallerCounter(cfinfoCaller);

            //check if we are at the first statement in the callback event
            if(!hasCaller)
            {TRACE_IT(43753);
                //Set the position info to the current statement and return true
                noPrevious = true;

                ftime = cfinfo.FunctionTime;
                ltime = cfinfo.CurrentStatementLoopTime;

                fbody = cfinfo.Function;
                statementIndex = cfinfo.CurrentStatementIndex;
            }
            else
            {TRACE_IT(43754);
                ftime = cfinfoCaller.FunctionTime;
                ltime = cfinfoCaller.CurrentStatementLoopTime;

                fbody = cfinfoCaller.Function;
                statementIndex = cfinfoCaller.CurrentStatementIndex;
            }
        }
        else
        {TRACE_IT(43755);
            ftime = cfinfo.FunctionTime;
            ltime = cfinfo.LastStatementLoopTime;

            fbody = cfinfo.Function;
            statementIndex = cfinfo.LastStatementIndex;
        }

        ULONG srcLine = 0;
        LONG srcColumn = -1;
        uint32 startOffset = fbody->GetStatementStartOffset(statementIndex);
        fbody->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

        sourceLocation.SetLocation(this->m_topLevelCallbackEventTime, ftime, ltime, fbody, srcLine, srcColumn);

        return noPrevious;
    }

    void EventLog::GetLastExecutedTimeAndPositionForDebugger(TTDebuggerSourceLocation& sourceLocation) const
    {TRACE_IT(43756);
        const TTLastReturnLocationInfo& cframe = this->m_lastReturnLocation;
        if(!cframe.IsDefined())
        {TRACE_IT(43757);
            sourceLocation.Clear();
            return;
        }
        else
        {TRACE_IT(43758);
            ULONG srcLine = 0;
            LONG srcColumn = -1;
            uint32 startOffset = cframe.GetLocation().Function->GetStatementStartOffset(cframe.GetLocation().CurrentStatementIndex);
            cframe.GetLocation().Function->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

            sourceLocation.SetLocation(this->m_topLevelCallbackEventTime, cframe.GetLocation().FunctionTime, cframe.GetLocation().CurrentStatementLoopTime, cframe.GetLocation().Function, srcLine, srcColumn);
        }
    }

    int64 EventLog::GetCurrentHostCallbackId() const
    {TRACE_IT(43759);
        return this->m_hostCallbackId;
    }

    int64 EventLog::GetCurrentTopLevelEventTime() const
    {TRACE_IT(43760);
        return this->m_topLevelCallbackEventTime;
    }

    const NSLogEvents::JsRTCallbackAction* EventLog::GetEventForHostCallbackId(bool wantRegisterOp, int64 hostIdOfInterest) const
    {TRACE_IT(43761);
        if(hostIdOfInterest == -1)
        {TRACE_IT(43762);
            return nullptr;
        }

        for(auto iter = this->m_currentReplayEventIterator; iter.IsValid(); iter.MovePrevious())
        {TRACE_IT(43763);
            if(iter.Current()->EventKind == NSLogEvents::EventKind::CallbackOpActionTag)
            {TRACE_IT(43764);
                const NSLogEvents::JsRTCallbackAction* callbackAction = NSLogEvents::GetInlineEventDataAs<NSLogEvents::JsRTCallbackAction, NSLogEvents::EventKind::CallbackOpActionTag>(iter.Current());
                if(callbackAction->NewCallbackId == hostIdOfInterest && callbackAction->IsCreate == wantRegisterOp)
                {TRACE_IT(43765);
                    return callbackAction;
                }
            }
        }

        return nullptr;
    }

    int64 EventLog::GetFirstEventTimeInLog() const
    {TRACE_IT(43766);
        for(auto iter = this->m_eventList.GetIteratorAtFirst(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43767);
            if(NSLogEvents::IsJsRTActionRootCall(iter.Current()))
            {TRACE_IT(43768);
                return NSLogEvents::GetTimeFromRootCallOrSnapshot(iter.Current());
            }
        }

        return -1;
    }

    int64 EventLog::GetLastEventTimeInLog() const
    {TRACE_IT(43769);
        for(auto iter = this->m_eventList.GetIteratorAtLast(); iter.IsValid(); iter.MovePrevious())
        {TRACE_IT(43770);
            if(NSLogEvents::IsJsRTActionRootCall(iter.Current()))
            {TRACE_IT(43771);
                return NSLogEvents::GetTimeFromRootCallOrSnapshot(iter.Current());
            }
        }

        return -1;
    }

    int64 EventLog::GetKthEventTimeInLog(uint32 k) const
    {TRACE_IT(43772);
        uint32 topLevelCount = 0;
        for(auto iter = this->m_eventList.GetIteratorAtFirst(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43773);
            if(NSLogEvents::IsJsRTActionRootCall(iter.Current()))
            {TRACE_IT(43774);
                topLevelCount++;

                if(topLevelCount == k)
                {TRACE_IT(43775);
                    return NSLogEvents::GetTimeFromRootCallOrSnapshot(iter.Current());
                }
            }
        }

        return -1;
    }

    void EventLog::ResetCallStackForTopLevelCall(int64 topLevelCallbackEventTime)
    {TRACE_IT(43776);
        TTDAssert(this->m_callStack.Count() == 0, "We should be at the top-level entry!!!");

        this->m_runningFunctionTimeCtr = 0;
        this->m_topLevelCallbackEventTime = topLevelCallbackEventTime;
        this->m_hostCallbackId = -1;

        this->m_lastReturnLocation.Clear();
    }

    bool EventLog::IsTimeForSnapshot() const
    {TRACE_IT(43777);
        return (this->m_elapsedExecutionTimeSinceSnapshot > this->m_threadContext->TTDContext->SnapInterval);
    }

    void EventLog::PruneLogLength()
    {TRACE_IT(43778);
        uint32 maxEvents = this->m_threadContext->TTDContext->SnapHistoryLength;
        auto tailIter = this->m_eventList.GetIteratorAtLast();
        while(maxEvents != 0 && tailIter.IsValid())
        {TRACE_IT(43779);
            if(tailIter.Current()->EventKind == NSLogEvents::EventKind::SnapshotTag)
            {TRACE_IT(43780);
                maxEvents--;
            }

            if(maxEvents != 0)
            {TRACE_IT(43781);
                //don't move when we point to the last snapshot we want to preserve (have as the new eventList start)
                tailIter.MovePrevious();
            }
        }

        if(maxEvents == 0 && tailIter.IsValid())
        {TRACE_IT(43782);
            auto delIter = this->m_eventList.GetIteratorAtFirst(); //we know tailIter is valid so at least 1 entry
            while(delIter.Current() != tailIter.Current())
            {TRACE_IT(43783);
                NSLogEvents::EventLogEntry* evt = delIter.Current();
                TTEventList::TTEventListLink* block = delIter.GetBlock();
                delIter.MoveNext();

                this->m_eventList.DeleteFirstEntry(block, evt, this->m_eventListVTable);
            }
        }
    }

    void EventLog::IncrementElapsedSnapshotTime(double addtlTime)
    {TRACE_IT(43784);
        this->m_elapsedExecutionTimeSinceSnapshot += addtlTime;
    }

    void EventLog::DoSnapshotExtract()
    {TRACE_IT(43785);
        this->SetSnapshotOrInflateInProgress(true);
        this->PushMode(TTDMode::ExcludedExecutionTTAction);

        ///////////////////////////
        //Create the event object and add it to the log
        NSLogEvents::SnapshotEventLogEntry* snapEvent = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::SnapshotEventLogEntry, NSLogEvents::EventKind::SnapshotTag>();
        snapEvent->RestoreTimestamp = this->GetLastEventTime();
        snapEvent->Snap = this->DoSnapshotExtract_Helper();

        this->m_elapsedExecutionTimeSinceSnapshot = 0.0;

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteLiteralMsg("---SNAPSHOT EVENT---\n");
#endif

        this->PopMode(TTDMode::ExcludedExecutionTTAction);
        this->SetSnapshotOrInflateInProgress(false);
    }

    void EventLog::DoRtrSnapIfNeeded()
    {TRACE_IT(43786);
        TTDAssert(this->m_currentReplayEventIterator.IsValid() && NSLogEvents::IsJsRTActionRootCall(this->m_currentReplayEventIterator.Current()), "Something in wrong with the event position.");

        this->SetSnapshotOrInflateInProgress(true);
        this->PushMode(TTDMode::ExcludedExecutionTTAction);

        NSLogEvents::JsRTCallFunctionAction* rootCall = NSLogEvents::GetInlineEventDataAs<NSLogEvents::JsRTCallFunctionAction, NSLogEvents::EventKind::CallExistingFunctionActionTag>(this->m_currentReplayEventIterator.Current());
        if(rootCall->AdditionalInfo->AdditionalReplayInfo->RtRSnap == nullptr)
        {TRACE_IT(43787);
            //Be careful to ensure that caller is actually doing this
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory | ExceptionType_JavascriptException));

            rootCall->AdditionalInfo->AdditionalReplayInfo->RtRSnap = this->DoSnapshotExtract_Helper();
        }

        this->PopMode(TTDMode::ExcludedExecutionTTAction);
        this->SetSnapshotOrInflateInProgress(false);
    }

    int64 EventLog::FindSnapTimeForEventTime(int64 targetTime, int64* optEndSnapTime)
    {TRACE_IT(43788);
        int64 snapTime = -1;
        if(optEndSnapTime != nullptr)
        {TRACE_IT(43789);
            *optEndSnapTime = -1;
        }

        for(auto iter = this->m_eventList.GetIteratorAtLast(); iter.IsValid(); iter.MovePrevious())
        {TRACE_IT(43790);
            bool isSnap = false;
            bool isRoot = false;
            bool hasRtrSnap = false;
            int64 time = NSLogEvents::AccessTimeInRootCallOrSnapshot(iter.Current(), isSnap, isRoot, hasRtrSnap);

            bool validSnap =  isSnap | (isRoot & hasRtrSnap);
            if(validSnap && time <= targetTime)
            {TRACE_IT(43791);
                snapTime = time;
                break;
            }
        }

        if(optEndSnapTime != nullptr)
        {TRACE_IT(43792);
            for(auto iter = this->m_eventList.GetIteratorAtFirst(); iter.IsValid(); iter.MoveNext())
            {TRACE_IT(43793);
                if(iter.Current()->EventKind == NSLogEvents::EventKind::SnapshotTag)
                {TRACE_IT(43794);
                    NSLogEvents::SnapshotEventLogEntry* snapEvent = NSLogEvents::GetInlineEventDataAs<NSLogEvents::SnapshotEventLogEntry, NSLogEvents::EventKind::SnapshotTag>(iter.Current());
                    if(snapEvent->RestoreTimestamp > snapTime)
                    {TRACE_IT(43795);
                        *optEndSnapTime = snapEvent->RestoreTimestamp;
                        break;
                    }
                }
            }
        }

        return snapTime;
    }

    void EventLog::GetSnapShotBoundInterval(int64 targetTime, int64* snapIntervalStart, int64* snapIntervalEnd) const
    {TRACE_IT(43796);
        *snapIntervalStart = -1;
        *snapIntervalEnd = -1;

        //move the iterator to the current snapshot just before the event
        auto iter = this->m_eventList.GetIteratorAtLast();
        while(iter.IsValid())
        {TRACE_IT(43797);
            NSLogEvents::EventLogEntry* evt = iter.Current();
            if(evt->EventKind == NSLogEvents::EventKind::SnapshotTag)
            {TRACE_IT(43798);
                NSLogEvents::SnapshotEventLogEntry* snapEvent = NSLogEvents::GetInlineEventDataAs<NSLogEvents::SnapshotEventLogEntry, NSLogEvents::EventKind::SnapshotTag>(iter.Current());
                if(snapEvent->RestoreTimestamp <= targetTime)
                {TRACE_IT(43799);
                    *snapIntervalStart = snapEvent->RestoreTimestamp;
                    break;
                }
            }

            iter.MovePrevious();
        }

        //now move the iter to the next snapshot
        while(iter.IsValid())
        {TRACE_IT(43800);
            NSLogEvents::EventLogEntry* evt = iter.Current();
            if(evt->EventKind == NSLogEvents::EventKind::SnapshotTag)
            {TRACE_IT(43801);
                NSLogEvents::SnapshotEventLogEntry* snapEvent = NSLogEvents::GetInlineEventDataAs<NSLogEvents::SnapshotEventLogEntry, NSLogEvents::EventKind::SnapshotTag>(iter.Current());
                if(*snapIntervalStart < snapEvent->RestoreTimestamp)
                {TRACE_IT(43802);
                    *snapIntervalEnd = snapEvent->RestoreTimestamp;
                    break;
                }
            }

            iter.MoveNext();
        }
    }

    int64 EventLog::GetPreviousSnapshotInterval(int64 currentSnapTime) const
    {TRACE_IT(43803);
        //move the iterator to the current snapshot just before the event
        for(auto iter = this->m_eventList.GetIteratorAtLast(); iter.IsValid(); iter.MovePrevious())
        {TRACE_IT(43804);
            NSLogEvents::EventLogEntry* evt = iter.Current();
            if(evt->EventKind == NSLogEvents::EventKind::SnapshotTag)
            {TRACE_IT(43805);
                NSLogEvents::SnapshotEventLogEntry* snapEvent = NSLogEvents::GetInlineEventDataAs<NSLogEvents::SnapshotEventLogEntry, NSLogEvents::EventKind::SnapshotTag>(iter.Current());
                if(snapEvent->RestoreTimestamp < currentSnapTime)
                {TRACE_IT(43806);
                    return snapEvent->RestoreTimestamp;
                }
            }
        }

        return -1;
    }

    void EventLog::DoSnapshotInflate(int64 etime)
    {TRACE_IT(43807);
        this->PushMode(TTDMode::ExcludedExecutionTTAction);

        const SnapShot* snap = nullptr;
        int64 restoreEventTime = -1;

        for(auto iter = this->m_eventList.GetIteratorAtLast(); iter.IsValid(); iter.MovePrevious())
        {TRACE_IT(43808);
            NSLogEvents::EventLogEntry* evt = iter.Current();
            if(evt->EventKind == NSLogEvents::EventKind::SnapshotTag)
            {TRACE_IT(43809);
                NSLogEvents::SnapshotEventLogEntry* snapEvent = NSLogEvents::GetInlineEventDataAs<NSLogEvents::SnapshotEventLogEntry, NSLogEvents::EventKind::SnapshotTag>(evt);
                if(snapEvent->RestoreTimestamp == etime)
                {TRACE_IT(43810);
                    NSLogEvents::SnapshotEventLogEntry_EnsureSnapshotDeserialized(evt, this->m_threadContext);

                    restoreEventTime = snapEvent->RestoreTimestamp;
                    snap = snapEvent->Snap;
                    break;
                }
            }

            if(NSLogEvents::IsJsRTActionRootCall(evt))
            {TRACE_IT(43811);
                const NSLogEvents::JsRTCallFunctionAction* rootEntry = NSLogEvents::GetInlineEventDataAs<NSLogEvents::JsRTCallFunctionAction, NSLogEvents::EventKind::CallExistingFunctionActionTag>(evt);

                if(rootEntry->AdditionalInfo->CallEventTime == etime)
                {TRACE_IT(43812);
                    restoreEventTime = rootEntry->AdditionalInfo->CallEventTime;
                    snap = rootEntry->AdditionalInfo->AdditionalReplayInfo->RtRSnap;
                    break;
                }
            }
        }
        TTDAssert(snap != nullptr, "Log should start with a snapshot!!!");

        uint32 dbgScopeCount = snap->GetDbgScopeCountNonTopLevel();

        TTDIdentifierDictionary<uint64, NSSnapValues::TopLevelScriptLoadFunctionBodyResolveInfo*> topLevelLoadScriptMap;
        topLevelLoadScriptMap.Initialize(this->m_loadedTopLevelScripts.Count());
        for(auto iter = this->m_loadedTopLevelScripts.GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43813);
            topLevelLoadScriptMap.AddItem(iter.Current()->TopLevelBase.TopLevelBodyCtr, iter.Current());
            dbgScopeCount += iter.Current()->TopLevelBase.ScopeChainInfo.ScopeCount;
        }

        TTDIdentifierDictionary<uint64, NSSnapValues::TopLevelNewFunctionBodyResolveInfo*> topLevelNewScriptMap;
        topLevelNewScriptMap.Initialize(this->m_newFunctionTopLevelScripts.Count());
        for(auto iter = this->m_newFunctionTopLevelScripts.GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43814);
            topLevelNewScriptMap.AddItem(iter.Current()->TopLevelBase.TopLevelBodyCtr, iter.Current());
            dbgScopeCount += iter.Current()->TopLevelBase.ScopeChainInfo.ScopeCount;
        }

        TTDIdentifierDictionary<uint64, NSSnapValues::TopLevelEvalFunctionBodyResolveInfo*> topLevelEvalScriptMap;
        topLevelEvalScriptMap.Initialize(this->m_evalTopLevelScripts.Count());
        for(auto iter = this->m_evalTopLevelScripts.GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43815);
            topLevelEvalScriptMap.AddItem(iter.Current()->TopLevelBase.TopLevelBodyCtr, iter.Current());
            dbgScopeCount += iter.Current()->TopLevelBase.ScopeChainInfo.ScopeCount;
        }

        ThreadContextTTD* threadCtx = this->m_threadContext->TTDContext;
        const UnorderedArrayList<NSSnapValues::SnapContext, TTD_ARRAY_LIST_SIZE_XSMALL>& snpCtxs = snap->GetContextList();

        //check if we can reuse script contexts or we need to create new ones
        bool reuseInflateMap = (this->m_lastInflateMap != nullptr && this->m_lastInflateSnapshotTime == etime && !threadCtx->ContextCreatedOrDestoyedInReplay());

        //Fast checks ok but make sure we aren't blocked by a non-restorable well known object
        if(reuseInflateMap)
        {TRACE_IT(43816);
            reuseInflateMap = snap->AllWellKnownObjectsReusable(this->m_lastInflateMap);
        }

        if(reuseInflateMap)
        {TRACE_IT(43817);
            this->m_lastInflateMap->PrepForReInflate(snap->ContextCount(), snap->HandlerCount(), snap->TypeCount(), snap->PrimitiveCount() + snap->ObjectCount(), snap->BodyCount(), dbgScopeCount, snap->EnvCount(), snap->SlotArrayCount());

            //collect anything that is dead
            threadCtx->ClearRootsForSnapRestore();
            this->m_threadContext->GetRecycler()->CollectNow<CollectNowForceInThread>();

            //inflate into existing contexts
            const JsUtil::List<Js::ScriptContext*, HeapAllocator>& oldCtxts = threadCtx->GetTTDContexts();
            for(auto iter = snpCtxs.GetIterator(); iter.IsValid(); iter.MoveNext())
            {TRACE_IT(43818);
                const NSSnapValues::SnapContext* sCtx = iter.Current();
                Js::ScriptContext* vCtx = nullptr;
                for(int32 i = 0; i < oldCtxts.Count(); ++i)
                {TRACE_IT(43819);
                    if(oldCtxts.Item(i)->ScriptContextLogTag == sCtx->ScriptContextLogId)
                    {TRACE_IT(43820);
                        vCtx = oldCtxts.Item(i);
                        break;
                    }
                }
                TTDAssert(vCtx != nullptr, "We lost a context somehow!!!");

                NSSnapValues::InflateScriptContext(sCtx, vCtx, this->m_lastInflateMap, topLevelLoadScriptMap, topLevelNewScriptMap, topLevelEvalScriptMap);
            }
        }
        else
        {TRACE_IT(43821);
            bool shouldReleaseCtxs = false;
            if(this->m_lastInflateMap != nullptr)
            {TRACE_IT(43822);
                shouldReleaseCtxs = true;

                TT_HEAP_DELETE(InflateMap, this->m_lastInflateMap);
                this->m_lastInflateMap = nullptr;
            }

            this->m_lastInflateMap = TT_HEAP_NEW(InflateMap);
            this->m_lastInflateMap->PrepForInitialInflate(this->m_threadContext, snap->ContextCount(), snap->HandlerCount(), snap->TypeCount(), snap->PrimitiveCount() + snap->ObjectCount(), snap->BodyCount(), dbgScopeCount, snap->EnvCount(), snap->SlotArrayCount());
            this->m_lastInflateSnapshotTime = etime;

            //collect anything that is dead
            JsUtil::List<FinalizableObject*, HeapAllocator> deadCtxs(&HeapAllocator::Instance);
            threadCtx->ClearContextsForSnapRestore(deadCtxs);
            threadCtx->ClearRootsForSnapRestore();

            //allocate and inflate into new contexts
            for(auto iter = snpCtxs.GetIterator(); iter.IsValid(); iter.MoveNext())
            {TRACE_IT(43823);
                const NSSnapValues::SnapContext* sCtx = iter.Current();

                Js::ScriptContext* vCtx = nullptr;
                threadCtx->TTDExternalObjectFunctions.pfCreateJsRTContextCallback(threadCtx->GetRuntimeHandle(), &vCtx);

                NSSnapValues::InflateScriptContext(sCtx, vCtx, this->m_lastInflateMap, topLevelLoadScriptMap, topLevelNewScriptMap, topLevelEvalScriptMap);
            }
            threadCtx->ResetContextCreatedOrDestoyedInReplay();

            if(shouldReleaseCtxs)
            {TRACE_IT(43824);
                for(int32 i = 0; i < deadCtxs.Count(); ++i)
                {TRACE_IT(43825);
                    threadCtx->TTDExternalObjectFunctions.pfReleaseJsRTContextCallback(deadCtxs.Item(i));
                }
                this->m_threadContext->GetRecycler()->CollectNow<CollectNowForceInThread>();
            }

            //We don't want to have a bunch of snapshots in memory (that will get big fast) so unload all but the current one
            for(auto iter = this->m_eventList.GetIteratorAtLast(); iter.IsValid(); iter.MovePrevious())
            {TRACE_IT(43826);
                bool isSnap = false;
                bool isRoot = false;
                bool hasRtrSnap = false;
                int64 time = NSLogEvents::AccessTimeInRootCallOrSnapshot(iter.Current(), isSnap, isRoot, hasRtrSnap);

                bool hasSnap = isSnap | (isRoot & hasRtrSnap);
                if(hasSnap && time != etime)
                {TRACE_IT(43827);
                    if(isSnap)
                    {TRACE_IT(43828);
                        NSLogEvents::SnapshotEventLogEntry_UnloadSnapshot(iter.Current());
                    }
                    else
                    {TRACE_IT(43829);
                        NSLogEvents::JsRTCallFunctionAction_UnloadSnapshot(iter.Current());
                    }
                }
            }
        }

        this->SetSnapshotOrInflateInProgress(true); //make sure we don't do any un-intended CrossSite conversions

        snap->Inflate(this->m_lastInflateMap, this->m_threadContext->TTDContext);
        this->m_lastInflateMap->CleanupAfterInflate();

        this->SetSnapshotOrInflateInProgress(false); //re-enable CrossSite conversions

        this->m_eventTimeCtr = restoreEventTime;
        if(!this->m_eventList.IsEmpty())
        {TRACE_IT(43830);
            this->m_currentReplayEventIterator = this->m_eventList.GetIteratorAtLast();

            while(true)
            {TRACE_IT(43831);
                bool isSnap = false;
                bool isRoot = false;
                bool hasRtrSnap = false;
                int64 time = NSLogEvents::AccessTimeInRootCallOrSnapshot(this->m_currentReplayEventIterator.Current(), isSnap, isRoot, hasRtrSnap);

                if((isSnap | isRoot) && time == this->m_eventTimeCtr)
                {TRACE_IT(43832);
                    break;
                }

                this->m_currentReplayEventIterator.MovePrevious();
            }

            //we want to advance to the event immediately after the snapshot as well so do that
            if(this->m_currentReplayEventIterator.Current()->EventKind == NSLogEvents::EventKind::SnapshotTag)
            {TRACE_IT(43833);
                this->m_eventTimeCtr++;
                this->m_currentReplayEventIterator.MoveNext();
            }

            //clear this out -- it shouldn't matter for most JsRT actions (alloc etc.) and should be reset by any call actions
            this->ResetCallStackForTopLevelCall(-1);
        }

        this->PopMode(TTDMode::ExcludedExecutionTTAction);

#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.WriteLiteralMsg("---INFLATED SNAPSHOT---\n");
#endif
    }

    void EventLog::ReplayRootEventsToTime(int64 eventTime)
    {TRACE_IT(43834);
        while(this->m_eventTimeCtr < eventTime)
        {TRACE_IT(43835);
            this->ReplaySingleRootEntry();
        }
    }

    void EventLog::ReplaySingleRootEntry()
    {TRACE_IT(43836);
        if(!this->m_currentReplayEventIterator.IsValid())
        {TRACE_IT(43837);
            this->AbortReplayReturnToHost();
        }

        NSLogEvents::EventKind eKind = this->m_currentReplayEventIterator.Current()->EventKind;
        if(eKind == NSLogEvents::EventKind::SnapshotTag)
        {TRACE_IT(43838);
            this->ReplaySnapshotEvent();
        }
        else if(eKind == NSLogEvents::EventKind::EventLoopYieldPointTag)
        {TRACE_IT(43839);
            this->ReplayEventLoopYieldPointEvent();
        }
        else
        {TRACE_IT(43840);
            TTDAssert(eKind > NSLogEvents::EventKind::JsRTActionTag, "Either this is an invalid tag to replay directly (should be driven internally) or it is not known!!!");

            this->ReplaySingleActionEventEntry();
        }

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        TTDAssert(!this->m_currentReplayEventIterator.IsValid() || this->m_currentReplayEventIterator.Current()->EventTimeStamp == this->m_eventTimeCtr, "We are out of sync here");
#endif
    }

    void EventLog::ReplayActionEventSequenceThroughTime(int64 eventTime)
    {TRACE_IT(43841);
        while(this->m_eventTimeCtr <= eventTime)
        {TRACE_IT(43842);
            this->ReplaySingleActionEventEntry();
        }
    }

    void EventLog::ReplaySingleActionEventEntry()
    {TRACE_IT(43843);
        if(!this->m_currentReplayEventIterator.IsValid())
        {TRACE_IT(43844);
            this->AbortReplayReturnToHost();
        }

        NSLogEvents::EventLogEntry* evt = this->m_currentReplayEventIterator.Current();
        this->AdvanceTimeAndPositionForReplay();

        NSLogEvents::ContextExecuteKind execKind = this->m_eventListVTable[(uint32)evt->EventKind].ContextKind;
        auto executeFP = this->m_eventListVTable[(uint32)evt->EventKind].ExecuteFP;

        TTDAssert(!NSLogEvents::EventFailsWithRuntimeError(evt), "We have a failing Event in the Log -- we assume host is correct!");

        ThreadContextTTD* executeContext = this->m_threadContext->TTDContext;
        if(execKind == NSLogEvents::ContextExecuteKind::GlobalAPIWrapper)
        {TRACE_IT(43845);
            //enter/exit global wrapper -- see JsrtInternal.h
            try
            {TRACE_IT(43846);
                AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory | ExceptionType_StackOverflow));

                executeFP(evt, executeContext);

                TTDAssert(NSLogEvents::EventCompletesNormally(evt), "All my action events should exit or terminate before return so no need to loop yet but may want to later");
            }
            catch(TTD::TTDebuggerAbortException)
            {TRACE_IT(43847);
                throw;
            }
            catch(...)
            {
                TTDAssert(false, "Encountered other kind of exception in replay??");
            }
        }
        else if(execKind == NSLogEvents::ContextExecuteKind::ContextAPIWrapper)
        {TRACE_IT(43848);
            //enter/exit context wrapper -- see JsrtInternal.h
            Js::ScriptContext* ctx = executeContext->GetActiveScriptContext();

            TTDAssert(ctx != nullptr, "This should be set!!!");
            TTDAssert(ctx->GetThreadContext()->GetRecordedException() == nullptr, "Shouldn't have outstanding exceptions (assume always CheckContext when recording).");
            TTDAssert(this->m_threadContext->TTDContext->GetActiveScriptContext() == ctx, "Make sure the replay host didn't change contexts on us unexpectedly without resetting back to the correct one.");

            try
            {TRACE_IT(43849);
                AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory | ExceptionType_JavascriptException));

                // Enter script
                BEGIN_ENTER_SCRIPT(ctx, true, true, true)
                {
                    executeFP(evt, executeContext);
                }
                END_ENTER_SCRIPT

                TTDAssert(NSLogEvents::EventCompletesNormally(evt), "All my action events should exit / terminate before return so no need to loop yet but may want to later");
            }
            catch(const Js::JavascriptException& err)
            {TRACE_IT(43850);
                TTDAssert(NSLogEvents::EventCompletesWithException(evt), "Should see same execption here");

                ctx->GetThreadContext()->SetRecordedException(err.GetAndClear());
            }
            catch(Js::ScriptAbortException)
            {TRACE_IT(43851);
                TTDAssert(NSLogEvents::EventCompletesWithException(evt), "Should see same execption here");

                Assert(ctx->GetThreadContext()->GetRecordedException() == nullptr);
                ctx->GetThreadContext()->SetRecordedException(ctx->GetThreadContext()->GetPendingTerminatedErrorObject());
            }
            catch(TTD::TTDebuggerAbortException)
            {TRACE_IT(43852);
                throw;
            }
            catch(...)
            {
                TTDAssert(false, "Encountered other kind of exception in replay??");
            }
        }
        else if(execKind == NSLogEvents::ContextExecuteKind::ContextAPINoScriptWrapper)
        {TRACE_IT(43853);
            //enter/exit context no script wrapper -- see JsrtInternal.h
            Js::ScriptContext* ctx = executeContext->GetActiveScriptContext();

            TTDAssert(ctx != nullptr, "This should be set!!!");
            TTDAssert(ctx->GetThreadContext()->GetRecordedException() == nullptr, "Shouldn't have outstanding exceptions (assume always CheckContext when recording).");
            TTDAssert(this->m_threadContext->TTDContext->GetActiveScriptContext() == ctx, "Make sure the replay host didn't change contexts on us unexpectedly without resetting back to the correct one.");

            try
            {TRACE_IT(43854);
                AUTO_NESTED_HANDLED_EXCEPTION_TYPE((ExceptionType)(ExceptionType_OutOfMemory | ExceptionType_StackOverflow));

                executeFP(evt, executeContext);

                TTDAssert(NSLogEvents::EventCompletesNormally(evt), "All my action events should both exit / terminate before return so no need to loop yet but may want to later");
            }
            catch(const Js::JavascriptException& err)
            {TRACE_IT(43855);
                TTDAssert(NSLogEvents::EventCompletesWithException(evt), "Should see same execption here");

                TTDAssert(false, "Should never get JavascriptExceptionObject for ContextAPINoScriptWrapper.");
                ctx->GetThreadContext()->SetRecordedException(err.GetAndClear());
            }
            catch(Js::ScriptAbortException)
            {TRACE_IT(43856);
                TTDAssert(NSLogEvents::EventCompletesWithException(evt), "Should see same execption here");

                Assert(ctx->GetThreadContext()->GetRecordedException() == nullptr);
                ctx->GetThreadContext()->SetRecordedException(ctx->GetThreadContext()->GetPendingTerminatedErrorObject());
            }
            catch(TTD::TTDebuggerAbortException)
            {TRACE_IT(43857);
                throw;
            }
            catch(...)
            {
                TTDAssert(false, "Encountered other kind of exception in replay??");
            }
        }
        else
        {TRACE_IT(43858);
            TTDAssert(executeContext->GetActiveScriptContext() == nullptr || !executeContext->GetActiveScriptContext()->GetThreadContext()->IsScriptActive(), "These should all be outside of script context!!!");

            //No need to move into script context just execute the action
            executeFP(evt, executeContext);
        }

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        TTDAssert(!this->m_currentReplayEventIterator.IsValid() || this->m_currentReplayEventIterator.Current()->EventTimeStamp == this->m_eventTimeCtr, "We are out of sync here");
#endif
    }

    bool EventLog::IsPropertyRecordRef(void* ref) const
    {TRACE_IT(43859);
        //This is an ugly cast but we just want to know if the pointer is in the set so it is ok here
        return this->m_propertyRecordPinSet->ContainsKey((Js::PropertyRecord*)ref);
    }

    double EventLog::GetCurrentWallTime()
    {TRACE_IT(43860);
        return this->m_timer.Now();
    }

    int64 EventLog::GetLastEventTime() const
    {TRACE_IT(43861);
        return this->m_eventTimeCtr - 1;
    }

    NSLogEvents::EventLogEntry* EventLog::RecordJsRTCreateScriptContext(TTDJsRTActionResultAutoRecorder& actionPopper)
    {TRACE_IT(43862);
        NSLogEvents::JsRTCreateScriptContextAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTCreateScriptContextAction, NSLogEvents::EventKind::CreateScriptContextActionTag>(&cAction);

        cAction->KnownObjects = this->m_eventSlabAllocator.SlabAllocateStruct<NSLogEvents::JsRTCreateScriptContextAction_KnownObjects>();
        cAction->KnownObjects = { 0 };

        actionPopper.InitializeWithEventAndEnter(evt);

        return evt;
    }

    void EventLog::RecordJsRTCreateScriptContextResult(NSLogEvents::EventLogEntry* evt, Js::ScriptContext* newCtx)
    {TRACE_IT(43863);
        NSLogEvents::JsRTCreateScriptContextAction* cAction = NSLogEvents::GetInlineEventDataAs<NSLogEvents::JsRTCreateScriptContextAction, NSLogEvents::EventKind::CreateScriptContextActionTag>(evt);
        cAction->KnownObjects = this->m_eventSlabAllocator.SlabAllocateStruct<NSLogEvents::JsRTCreateScriptContextAction_KnownObjects>();

        cAction->GlobalObject = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(newCtx->GetGlobalObject());
        cAction->KnownObjects->UndefinedObject = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(newCtx->GetLibrary()->GetUndefined());
        cAction->KnownObjects->NullObject = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(newCtx->GetLibrary()->GetNull());
        cAction->KnownObjects->TrueObject = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(newCtx->GetLibrary()->GetTrue());
        cAction->KnownObjects->FalseObject = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(newCtx->GetLibrary()->GetFalse());
    }

    void EventLog::RecordJsRTSetCurrentContext(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var globalObject)
    {TRACE_IT(43864);
        NSLogEvents::JsRTVarsArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::SetActiveScriptContextActionTag>(&sAction);
        sAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(globalObject);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTDeadScriptEvent(const DeadScriptLogTagInfo& deadCtx)
    {TRACE_IT(43865);
        NSLogEvents::JsRTDestroyScriptContextAction* dAction = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::JsRTDestroyScriptContextAction, NSLogEvents::EventKind::DeadScriptContextActionTag>();
        dAction->KnownObjects = this->m_eventSlabAllocator.SlabAllocateStruct<NSLogEvents::JsRTDestroyScriptContextAction_KnownObjects>();

        dAction->GlobalLogTag = deadCtx.GlobalLogTag;
        dAction->KnownObjects->UndefinedLogTag = deadCtx.UndefinedLogTag;
        dAction->KnownObjects->NullLogTag = deadCtx.NullLogTag;
        dAction->KnownObjects->TrueLogTag = deadCtx.TrueLogTag;
        dAction->KnownObjects->FalseLogTag = deadCtx.FalseLogTag;
    }

#if !INT32VAR
    void EventLog::RecordJsRTCreateInteger(TTDJsRTActionResultAutoRecorder& actionPopper, int value)
    {TRACE_IT(43866);
        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* iAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::CreateIntegerActionTag>(&iAction);
        iAction->u_iVal = value;

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(iAction->Result));
    }
#endif

    void EventLog::RecordJsRTCreateNumber(TTDJsRTActionResultAutoRecorder& actionPopper, double value)
    {TRACE_IT(43867);
        NSLogEvents::JsRTDoubleArgumentAction* dAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTDoubleArgumentAction, NSLogEvents::EventKind::CreateNumberActionTag>(&dAction);
        dAction->DoubleValue = value;

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(dAction->Result));
    }

    void EventLog::RecordJsRTCreateBoolean(TTDJsRTActionResultAutoRecorder& actionPopper, bool value)
    {TRACE_IT(43868);
        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* bAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::CreateBooleanActionTag>(&bAction);
        bAction->u_iVal = value;

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(bAction->Result));
    }

    void EventLog::RecordJsRTCreateString(TTDJsRTActionResultAutoRecorder& actionPopper, const char16* stringValue, size_t stringLength)
    {TRACE_IT(43869);
        NSLogEvents::JsRTStringArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTStringArgumentAction, NSLogEvents::EventKind::CreateStringActionTag>(&sAction);
        this->m_eventSlabAllocator.CopyStringIntoWLength(stringValue, (uint32)stringLength, sAction->StringValue);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(sAction->Result));
    }

    void EventLog::RecordJsRTCreateSymbol(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43870);
        NSLogEvents::JsRTVarsArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::CreateSymbolActionTag>(&sAction);
        sAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(sAction->Result));
    }

    void EventLog::RecordJsRTCreateError(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var msg)
    {TRACE_IT(43871);
        NSLogEvents::JsRTVarsArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::CreateErrorActionTag>(&sAction);
        sAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(msg);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(sAction->Result));
    }

    void EventLog::RecordJsRTCreateRangeError(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var msg)
    {TRACE_IT(43872);
        NSLogEvents::JsRTVarsArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::CreateRangeErrorActionTag>(&sAction);
        sAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(msg);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(sAction->Result));
    }

    void EventLog::RecordJsRTCreateReferenceError(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var msg)
    {TRACE_IT(43873);
        NSLogEvents::JsRTVarsArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::CreateReferenceErrorActionTag>(&sAction);
        sAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(msg);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(sAction->Result));
    }

    void EventLog::RecordJsRTCreateSyntaxError(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var msg)
    {TRACE_IT(43874);
        NSLogEvents::JsRTVarsArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::CreateSyntaxErrorActionTag>(&sAction);
        sAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(msg);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(sAction->Result));
    }

    void EventLog::RecordJsRTCreateTypeError(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var msg)
    {TRACE_IT(43875);
        NSLogEvents::JsRTVarsArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::CreateTypeErrorActionTag>(&sAction);
        sAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(msg);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(sAction->Result));
    }

    void EventLog::RecordJsRTCreateURIError(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var msg)
    {TRACE_IT(43876);
        NSLogEvents::JsRTVarsArgumentAction* sAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::CreateURIErrorActionTag>(&sAction);
        sAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(msg);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(sAction->Result));
    }

    void EventLog::RecordJsRTVarToNumberConversion(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43877);
        NSLogEvents::JsRTVarsArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::VarConvertToNumberActionTag>(&cAction);
        cAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTVarToBooleanConversion(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43878);
        NSLogEvents::JsRTVarsArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::VarConvertToBooleanActionTag>(&cAction);
        cAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTVarToStringConversion(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43879);
        NSLogEvents::JsRTVarsArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::VarConvertToStringActionTag>(&cAction);
        cAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTVarToObjectConversion(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43880);
        NSLogEvents::JsRTVarsArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::VarConvertToObjectActionTag>(&cAction);
        cAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTAddRootRef(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43881);
        NSLogEvents::JsRTVarsArgumentAction* addAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::AddRootRefActionTag>(&addAction);
        addAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTRemoveRootRef(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43882);
        NSLogEvents::JsRTVarsArgumentAction* removeAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::RemoveRootRefActionTag>(&removeAction);
        removeAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTEventLoopYieldPoint()
    {TRACE_IT(43883);
        NSLogEvents::EventLoopYieldPointEntry* ypEvt = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::EventLoopYieldPointEntry, NSLogEvents::EventKind::EventLoopYieldPointTag >();
        ypEvt->EventTimeStamp = this->GetLastEventTime();
        ypEvt->EventWallTime = this->GetCurrentWallTime();

        //Put this here in the hope that after handling an event there is an idle period where we can work without blocking user work
        if(this->IsTimeForSnapshot())
        {TRACE_IT(43884);
            this->DoSnapshotExtract();
            this->PruneLogLength();
        }
    }

    void EventLog::RecordJsRTAllocateBasicObject(TTDJsRTActionResultAutoRecorder& actionPopper)
    {TRACE_IT(43885);
        NSLogEvents::JsRTVarsArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::AllocateObjectActionTag>(&cAction);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTAllocateExternalObject(TTDJsRTActionResultAutoRecorder& actionPopper)
    {TRACE_IT(43886);
        NSLogEvents::JsRTVarsArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::AllocateExternalObjectActionTag>(&cAction);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTAllocateBasicArray(TTDJsRTActionResultAutoRecorder& actionPopper, uint32 length)
    {TRACE_IT(43887);
        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::AllocateArrayActionTag>(&cAction);
        cAction->u_iVal = length;

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTAllocateArrayBuffer(TTDJsRTActionResultAutoRecorder& actionPopper, uint32 size)
    {TRACE_IT(43888);
        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::AllocateArrayBufferActionTag>(&cAction);
        cAction->u_iVal = size;

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTAllocateExternalArrayBuffer(TTDJsRTActionResultAutoRecorder& actionPopper, byte* buff, uint32 size)
    {TRACE_IT(43889);
        NSLogEvents::JsRTByteBufferAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTByteBufferAction, NSLogEvents::EventKind::AllocateExternalArrayBufferActionTag>(&cAction);
        cAction->Length = size;

        cAction->Buffer = nullptr; 
        if(cAction->Length != 0)
        {TRACE_IT(43890);
            cAction->Buffer = this->m_eventSlabAllocator.SlabAllocateArray<byte>(cAction->Length);
            js_memcpy_s(cAction->Buffer, cAction->Length, buff, size);
        }

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTAllocateFunction(TTDJsRTActionResultAutoRecorder& actionPopper, bool isNamed, Js::Var optName)
    {TRACE_IT(43891);
        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::AllocateFunctionActionTag>(&cAction);
        cAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(optName);
        cAction->u_bVal = isNamed;

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));
    }

    void EventLog::RecordJsRTHostExitProcess(TTDJsRTActionResultAutoRecorder& actionPopper, int32 exitCode)
    {TRACE_IT(43892);
        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* eAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::HostExitProcessTag>(&eAction);
        eAction->u_iVal = exitCode;

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTGetAndClearException(TTDJsRTActionResultAutoRecorder& actionPopper)
    {TRACE_IT(43893);
        NSLogEvents::JsRTVarsArgumentAction* gcAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::GetAndClearExceptionActionTag>(&gcAction);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(gcAction->Result));
    }

    void EventLog::RecordJsRTSetException(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var, bool propagateToDebugger)
    {TRACE_IT(43894);
        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* spAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::SetExceptionActionTag>(&spAction);
        spAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        spAction->u_bVal = propagateToDebugger;

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTHasProperty(TTDJsRTActionResultAutoRecorder& actionPopper, const Js::PropertyRecord* pRecord, Js::Var var)
    {TRACE_IT(43895);
        //The host may not have validated this yet (and will exit early if the check fails) so we check it here as well before getting the property id below
        if(pRecord == nullptr || Js::IsInternalPropertyId(pRecord->GetPropertyId()))
        {TRACE_IT(43896);
            return;
        }

        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::HasPropertyActionTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        gpAction->u_pid = pRecord->GetPropertyId();

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTInstanceOf(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var object, Js::Var constructor)
    {TRACE_IT(43897);
        NSLogEvents::JsRTVarsArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::InstanceOfActionTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(object);
        gpAction->Var2 = TTD_CONVERT_JSVAR_TO_TTDVAR(constructor);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTEquals(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var1, Js::Var var2, bool doStrict)
    {TRACE_IT(43898);
        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::EqualsActionTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var1);
        gpAction->Var2 = TTD_CONVERT_JSVAR_TO_TTDVAR(var2);
        gpAction->u_bVal = doStrict;

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTGetPropertyIdFromSymbol(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var sym)
    {TRACE_IT(43899);
        NSLogEvents::JsRTVarsArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::GetPropertyIdFromSymbolTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(sym);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTGetPrototype(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43900);
        NSLogEvents::JsRTVarsArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::GetPrototypeActionTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(gpAction->Result));
    }

    void EventLog::RecordJsRTGetProperty(TTDJsRTActionResultAutoRecorder& actionPopper, const Js::PropertyRecord* pRecord, Js::Var var)
    {TRACE_IT(43901);
        //The host may not have validated this yet (and will exit early if the check fails) so we check it here as well before getting the property id below
        if(pRecord == nullptr || Js::IsInternalPropertyId(pRecord->GetPropertyId()))
        {TRACE_IT(43902);
            return;
        }

        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::GetPropertyActionTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        gpAction->u_pid = pRecord->GetPropertyId();

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(gpAction->Result));
    }

    void EventLog::RecordJsRTGetIndex(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var index, Js::Var var)
    {TRACE_IT(43903);
        NSLogEvents::JsRTVarsArgumentAction* giAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::GetIndexActionTag>(&giAction);
        giAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        giAction->Var2 = TTD_CONVERT_JSVAR_TO_TTDVAR(index);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(giAction->Result));
    }

    void EventLog::RecordJsRTGetOwnPropertyInfo(TTDJsRTActionResultAutoRecorder& actionPopper, const Js::PropertyRecord* pRecord, Js::Var var)
    {TRACE_IT(43904);
        //The host may not have validated this yet (and will exit early if the check fails) so we check it here as well before getting the property id below
        if(pRecord == nullptr || Js::IsInternalPropertyId(pRecord->GetPropertyId()))
        {TRACE_IT(43905);
            return;
        }

        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::GetOwnPropertyInfoActionTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        gpAction->u_pid = pRecord->GetPropertyId();

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(gpAction->Result));
    }

    void EventLog::RecordJsRTGetOwnPropertyNamesInfo(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43906);
        NSLogEvents::JsRTVarsArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::GetOwnPropertyNamesInfoActionTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(gpAction->Result));
    }

    void EventLog::RecordJsRTGetOwnPropertySymbolsInfo(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var)
    {TRACE_IT(43907);
        NSLogEvents::JsRTVarsArgumentAction* gpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::GetOwnPropertySymbolsInfoActionTag>(&gpAction);
        gpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(gpAction->Result));
    }

    void EventLog::RecordJsRTDefineProperty(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var, const Js::PropertyRecord* pRecord, Js::Var propertyDescriptor)
    {TRACE_IT(43908);
        //The host may not have validated this yet (and will exit early if the check fails) so we check it here as well before getting the property id below
        if(pRecord == nullptr || Js::IsInternalPropertyId(pRecord->GetPropertyId()))
        {TRACE_IT(43909);
            return;
        }

        NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction* dpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithIntegralUnionArgumentAction, NSLogEvents::EventKind::DefinePropertyActionTag>(&dpAction);
        dpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        dpAction->Var2 = TTD_CONVERT_JSVAR_TO_TTDVAR(propertyDescriptor);
        dpAction->u_pid = pRecord->GetPropertyId();

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTDeleteProperty(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var, const Js::PropertyRecord* pRecord, bool useStrictRules)
    {TRACE_IT(43910);
        //The host may not have validated this yet (and will exit early if the check fails) so we check it here as well before getting the property id below
        if(pRecord == nullptr || Js::IsInternalPropertyId(pRecord->GetPropertyId()))
        {TRACE_IT(43911);
            return;
        }

        NSLogEvents::JsRTVarsWithBoolAndPIDArgumentAction* dpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithBoolAndPIDArgumentAction, NSLogEvents::EventKind::DeletePropertyActionTag>(&dpAction);
        dpAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        dpAction->Pid = pRecord->GetPropertyId();
        dpAction->BoolVal = useStrictRules;

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(dpAction->Result));
    }

    void EventLog::RecordJsRTSetPrototype(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var, Js::Var proto)
    {TRACE_IT(43912);
        NSLogEvents::JsRTVarsArgumentAction* spAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::SetPrototypeActionTag>(&spAction);
        spAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        spAction->Var2 = TTD_CONVERT_JSVAR_TO_TTDVAR(proto);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTSetProperty(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var, const Js::PropertyRecord* pRecord, Js::Var val, bool useStrictRules)
    {TRACE_IT(43913);
        //The host may not have validated this yet (and will exit early if the check fails) so we check it here as well before getting the property id below
        if(pRecord == nullptr || Js::IsInternalPropertyId(pRecord->GetPropertyId()))
        {TRACE_IT(43914);
            return;
        }

        NSLogEvents::JsRTVarsWithBoolAndPIDArgumentAction* spAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsWithBoolAndPIDArgumentAction, NSLogEvents::EventKind::SetPropertyActionTag>(&spAction);
        spAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        spAction->Var2 = TTD_CONVERT_JSVAR_TO_TTDVAR(val);
        spAction->Pid = pRecord->GetPropertyId();
        spAction->BoolVal = useStrictRules;

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTSetIndex(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var var, Js::Var index, Js::Var val)
    {TRACE_IT(43915);
        NSLogEvents::JsRTVarsArgumentAction* spAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::SetIndexActionTag>(&spAction);
        spAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);
        spAction->Var2 = TTD_CONVERT_JSVAR_TO_TTDVAR(index);
        spAction->Var3 = TTD_CONVERT_JSVAR_TO_TTDVAR(val);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTGetTypedArrayInfo(Js::Var var, Js::Var result)
    {TRACE_IT(43916);
        NSLogEvents::JsRTVarsArgumentAction* giAction = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::JsRTVarsArgumentAction, NSLogEvents::EventKind::GetTypedArrayInfoActionTag>();
        giAction->Var1 = TTD_CONVERT_JSVAR_TO_TTDVAR(var);

        //entry/exit status should be set to clead by initialization so don't need to do anything
        giAction->Result = TTD_CONVERT_JSVAR_TO_TTDVAR(result);
    }

    void EventLog::RecordJsRTRawBufferCopySync(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var dst, uint32 dstIndex, Js::Var src, uint32 srcIndex, uint32 length)
    {TRACE_IT(43917);
        TTDAssert(Js::ArrayBuffer::Is(dst) && Js::ArrayBuffer::Is(src), "Not array buffer objects!!!");
        TTDAssert(dstIndex + length <= Js::ArrayBuffer::FromVar(dst)->GetByteLength(), "Copy off end of buffer!!!");
        TTDAssert(srcIndex + length <= Js::ArrayBuffer::FromVar(src)->GetByteLength(), "Copy off end of buffer!!!");

        NSLogEvents::JsRTRawBufferCopyAction* rbcAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTRawBufferCopyAction, NSLogEvents::EventKind::RawBufferCopySync>(&rbcAction);
        rbcAction->Dst = TTD_CONVERT_JSVAR_TO_TTDVAR(dst);
        rbcAction->Src = TTD_CONVERT_JSVAR_TO_TTDVAR(src);
        rbcAction->DstIndx = dstIndex;
        rbcAction->SrcIndx = srcIndex;
        rbcAction->Count = length;

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTRawBufferModifySync(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var dst, uint32 index, uint32 count)
    {TRACE_IT(43918);
        TTDAssert(Js::ArrayBuffer::Is(dst), "Not array buffer object!!!");
        TTDAssert(index + count <= Js::ArrayBuffer::FromVar(dst)->GetByteLength(), "Copy off end of buffer!!!");

        NSLogEvents::JsRTRawBufferModifyAction* rbmAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTRawBufferModifyAction, NSLogEvents::EventKind::RawBufferModifySync>(&rbmAction);
        rbmAction->Trgt = TTD_CONVERT_JSVAR_TO_TTDVAR(dst);
        rbmAction->Index = index;
        rbmAction->Length = count;

        rbmAction->Data = (rbmAction->Length != 0) ? this->m_eventSlabAllocator.SlabAllocateArray<byte>(rbmAction->Length) : nullptr;
        byte* copyBuff = Js::ArrayBuffer::FromVar(dst)->GetBuffer() + index;
        js_memcpy_s(rbmAction->Data, rbmAction->Length, copyBuff, count);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTRawBufferAsyncModificationRegister(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var dst, uint32 index)
    {TRACE_IT(43919);
            NSLogEvents::JsRTRawBufferModifyAction* rbrAction = nullptr;
            NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTRawBufferModifyAction, NSLogEvents::EventKind::RawBufferAsyncModificationRegister>(&rbrAction);
            rbrAction->Trgt = TTD_CONVERT_JSVAR_TO_TTDVAR(dst);
            rbrAction->Index = index;

            actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTRawBufferAsyncModifyComplete(TTDJsRTActionResultAutoRecorder& actionPopper, TTDPendingAsyncBufferModification& pendingAsyncInfo, byte* finalModPos)
    {TRACE_IT(43920);
        Js::ArrayBuffer* dstBuff = Js::ArrayBuffer::FromVar(pendingAsyncInfo.ArrayBufferVar);
        byte* copyBuff = dstBuff->GetBuffer() + pendingAsyncInfo.Index;

        NSLogEvents::JsRTRawBufferModifyAction* rbrAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTRawBufferModifyAction, NSLogEvents::EventKind::RawBufferAsyncModifyComplete>(&rbrAction);
        rbrAction->Trgt = TTD_CONVERT_JSVAR_TO_TTDVAR(dstBuff);
        rbrAction->Index = (uint32)pendingAsyncInfo.Index;
        rbrAction->Length = (uint32)(finalModPos - copyBuff);

        rbrAction->Data = (rbrAction->Length != 0) ? this->m_eventSlabAllocator.SlabAllocateArray<byte>(rbrAction->Length) : nullptr;
        js_memcpy_s(rbrAction->Data, rbrAction->Length, copyBuff, rbrAction->Length);

        actionPopper.InitializeWithEventAndEnter(evt);
    }

    void EventLog::RecordJsRTConstructCall(TTDJsRTActionResultAutoRecorder& actionPopper, Js::Var funcVar, uint32 argCount, Js::Var* args)
    {TRACE_IT(43921);
        NSLogEvents::JsRTConstructCallAction* ccAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTConstructCallAction, NSLogEvents::EventKind::ConstructCallActionTag>(&ccAction);

        ccAction->ArgCount = argCount + 1;

        static_assert(sizeof(TTDVar) == sizeof(Js::Var), "These need to be the same size (and have same bit layout) for this to work!");

        ccAction->ArgArray = this->m_eventSlabAllocator.SlabAllocateArray<TTDVar>(ccAction->ArgCount);
        ccAction->ArgArray[0] = TTD_CONVERT_JSVAR_TO_TTDVAR(funcVar);
        js_memcpy_s(ccAction->ArgArray + 1, (ccAction->ArgCount - 1) * sizeof(TTDVar), args, argCount * sizeof(Js::Var));

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(ccAction->Result));
    }

    void EventLog::RecordJsRTCallbackOperation(Js::ScriptContext* ctx, bool isCreate, bool isCancel, bool isRepeating, Js::JavascriptFunction* func, int64 callbackId)
    {TRACE_IT(43922);
        NSLogEvents::JsRTCallbackAction* cbrAction = this->RecordGetInitializedEvent_DataOnly<NSLogEvents::JsRTCallbackAction, NSLogEvents::EventKind::CallbackOpActionTag>();
        cbrAction->CurrentCallbackId = this->m_hostCallbackId;
        cbrAction->NewCallbackId = callbackId;

        //Register location is blank in record -- we only fill it in during debug replay

        cbrAction->IsCreate = isCreate;
        cbrAction->IsCancel = isCancel;
        cbrAction->IsRepeating = isRepeating;

        cbrAction->RegisterLocation = nullptr;
    }

    NSLogEvents::EventLogEntry* EventLog::RecordJsRTCodeParse(TTDJsRTActionResultAutoRecorder& actionPopper, LoadScriptFlag loadFlag, bool isUft8, const byte* script, uint32 scriptByteLength, uint64 sourceContextId, const char16* sourceUri)
    {TRACE_IT(43923);
        NSLogEvents::JsRTCodeParseAction* cpAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTCodeParseAction, NSLogEvents::EventKind::CodeParseActionTag>(&cpAction);
        cpAction->AdditionalInfo = this->m_eventSlabAllocator.SlabAllocateStruct<NSLogEvents::JsRTCodeParseAction_AdditionalInfo>();

        cpAction->BodyCtrId = 0; //initialize to known default -- should always update later or something is wrong

        cpAction->AdditionalInfo->IsUtf8 = isUft8;
        cpAction->AdditionalInfo->SourceByteLength = scriptByteLength;

        cpAction->AdditionalInfo->SourceCode = this->m_eventSlabAllocator.SlabAllocateArray<byte>(cpAction->AdditionalInfo->SourceByteLength);
        js_memcpy_s(cpAction->AdditionalInfo->SourceCode, cpAction->AdditionalInfo->SourceByteLength, script, scriptByteLength);

        this->m_eventSlabAllocator.CopyNullTermStringInto(sourceUri, cpAction->AdditionalInfo->SourceUri);
        cpAction->AdditionalInfo->SourceContextId = sourceContextId;

        cpAction->AdditionalInfo->LoadFlag = loadFlag;

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cpAction->Result));

        return evt;
    }

    NSLogEvents::EventLogEntry* EventLog::RecordJsRTCallFunction(TTDJsRTActionResultAutoRecorder& actionPopper, int32 rootDepth, Js::Var funcVar, uint32 argCount, Js::Var* args)
    {TRACE_IT(43924);
        NSLogEvents::JsRTCallFunctionAction* cAction = nullptr;
        NSLogEvents::EventLogEntry* evt = this->RecordGetInitializedEvent<NSLogEvents::JsRTCallFunctionAction, NSLogEvents::EventKind::CallExistingFunctionActionTag>(&cAction);

        int64 evtTime = this->GetLastEventTime();
        int64 topLevelCallTime = (rootDepth == 0) ? evtTime : this->m_topLevelCallbackEventTime;
        NSLogEvents::JsRTCallFunctionAction_ProcessArgs(evt, rootDepth, evtTime, funcVar, argCount, args, topLevelCallTime, this->m_eventSlabAllocator);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        NSLogEvents::JsRTCallFunctionAction_ProcessDiagInfoPre(evt, funcVar, this->m_eventSlabAllocator);
#endif

        actionPopper.InitializeWithEventAndEnterWResult(evt, &(cAction->Result));

        return evt;
    }

    void EventLog::EmitLog(const char* emitUri, size_t emitUriLength)
    {TRACE_IT(43925);
#if ENABLE_BASIC_TRACE || ENABLE_FULL_BC_TRACE
        this->m_diagnosticLogger.ForceFlush();
#endif

        TTDataIOInfo& iofp = this->m_threadContext->TTDContext->TTDataIOInfo;
        iofp.ActiveTTUriLength = emitUriLength;
        iofp.ActiveTTUri = emitUri;

        const char* logfilename = "ttdlog.log";
        JsTTDStreamHandle logHandle = iofp.pfOpenResourceStream(iofp.ActiveTTUriLength, iofp.ActiveTTUri, strlen(logfilename), logfilename, false, true);
        TTDAssert(logHandle != nullptr, "Failed to initialize strem for writing TTD Log.");

        TTD_LOG_WRITER writer(logHandle, iofp.pfWriteBytesToStream, iofp.pfFlushAndCloseStream);

        writer.WriteRecordStart();
        writer.AdjustIndent(1);

        TTString archString;
#if defined(_M_IX86)
        this->m_miscSlabAllocator.CopyNullTermStringInto(_u("x86"), archString);
#elif defined(_M_X64)
        this->m_miscSlabAllocator.CopyNullTermStringInto(_u("x64"), archString);
#elif defined(_M_ARM)
        this->m_miscSlabAllocator.CopyNullTermStringInto(_u("arm"), archString);
#elif defined(_M_ARM64)
        this->m_miscSlabAllocator.CopyNullTermStringInto(_u("arm64"), archString);
#else
        this->m_miscSlabAllocator.CopyNullTermStringInto(_u("unknown"), archString);
#endif

        writer.WriteString(NSTokens::Key::arch, archString);

        TTString platformString;
#if defined(_WIN32)
        this->m_miscSlabAllocator.CopyNullTermStringInto(_u("Windows"), platformString);
#elif defined(__APPLE__)
        this->m_miscSlabAllocator.CopyNullTermStringInto(_u("macOS"), platformString);
#else
        this->m_miscSlabAllocator.CopyNullTermStringInto(_u("Linux"), platformString);
#endif

        writer.WriteString(NSTokens::Key::platform, platformString, NSTokens::Separator::CommaSeparator);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        bool diagEnabled = true;
#else
        bool diagEnabled = false;
#endif

        writer.WriteBool(NSTokens::Key::diagEnabled, diagEnabled, NSTokens::Separator::CommaSeparator);

        uint64 usedSpace = 0;
        uint64 reservedSpace = 0;
        this->m_eventSlabAllocator.ComputeMemoryUsed(&usedSpace, &reservedSpace);

        writer.WriteUInt64(NSTokens::Key::usedMemory, usedSpace, NSTokens::Separator::CommaSeparator);
        writer.WriteUInt64(NSTokens::Key::reservedMemory, reservedSpace, NSTokens::Separator::CommaSeparator);

        uint32 ecount = this->m_eventList.Count();
        writer.WriteLengthValue(ecount, NSTokens::Separator::CommaAndBigSpaceSeparator);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        JsUtil::Stack<int64, HeapAllocator> callNestingStack(&HeapAllocator::Instance);
#endif

        bool firstElem = true;

        writer.WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
        writer.AdjustIndent(1);
        writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
        for(auto iter = this->m_eventList.GetIteratorAtFirst(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43926);
            const NSLogEvents::EventLogEntry* evt = iter.Current();

            NSTokens::Separator sep = firstElem ? NSTokens::Separator::NoSeparator : NSTokens::Separator::BigSpaceSeparator;
            NSLogEvents::EventLogEntry_Emit(evt, this->m_eventListVTable, &writer, this->m_threadContext, sep);

            firstElem = false;
#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            bool isJsRTCall = (evt->EventKind == NSLogEvents::EventKind::CallExistingFunctionActionTag);
            bool isExternalCall = (evt->EventKind == NSLogEvents::EventKind::ExternalCallTag);
            bool isRegisterCall = (evt->EventKind == NSLogEvents::EventKind::ExternalCbRegisterCall);
            if(isJsRTCall | isExternalCall | isRegisterCall)
            {TRACE_IT(43927);
                writer.WriteSequenceStart(NSTokens::Separator::BigSpaceSeparator);

                int64 lastNestedTime = -1;
                if(isJsRTCall)
                {TRACE_IT(43928);
                    lastNestedTime = NSLogEvents::JsRTCallFunctionAction_GetLastNestedEventTime(evt);
                }
                else if(isExternalCall)
                {TRACE_IT(43929);
                    lastNestedTime = NSLogEvents::ExternalCallEventLogEntry_GetLastNestedEventTime(evt);
                }
                else
                {TRACE_IT(43930);
                    lastNestedTime = NSLogEvents::ExternalCbRegisterCallEventLogEntry_GetLastNestedEventTime(evt);
                }
                callNestingStack.Push(lastNestedTime);

                if(lastNestedTime != evt->EventTimeStamp)
                {TRACE_IT(43931);
                    writer.AdjustIndent(1);

                    writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
                    firstElem = true;
                }
            }

            if(!callNestingStack.Empty() && evt->EventTimeStamp == callNestingStack.Peek())
            {TRACE_IT(43932);
                int64 eTime = callNestingStack.Pop();

                if(!isJsRTCall & !isExternalCall & !isRegisterCall)
                {TRACE_IT(43933);
                    writer.AdjustIndent(-1);
                    writer.WriteSeperator(NSTokens::Separator::BigSpaceSeparator);
                }
                writer.WriteSequenceEnd();

                while(!callNestingStack.Empty() && eTime == callNestingStack.Peek())
                {TRACE_IT(43934);
                    callNestingStack.Pop();

                    writer.AdjustIndent(-1);
                    writer.WriteSequenceEnd(NSTokens::Separator::BigSpaceSeparator);
                }
            }
#endif
        }
        writer.AdjustIndent(-1);
        writer.WriteSequenceEnd(NSTokens::Separator::BigSpaceSeparator);

        //emit the properties
        writer.WriteLengthValue(this->m_propertyRecordPinSet->Count(), NSTokens::Separator::CommaSeparator);

        writer.WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
        writer.AdjustIndent(1);
        bool firstProperty = true;
        for(auto iter = this->m_propertyRecordPinSet->GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43935);
            NSTokens::Separator sep = (!firstProperty) ? NSTokens::Separator::CommaAndBigSpaceSeparator : NSTokens::Separator::BigSpaceSeparator;
            NSSnapType::EmitPropertyRecordAsSnapPropertyRecord(iter.CurrentValue(), &writer, sep);

            firstProperty = false;
        }
        writer.AdjustIndent(-1);
        writer.WriteSequenceEnd(NSTokens::Separator::BigSpaceSeparator);

        //do top level script processing here
        writer.WriteLengthValue(this->m_loadedTopLevelScripts.Count(), NSTokens::Separator::CommaSeparator);
        writer.WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
        writer.AdjustIndent(1);
        bool firstLoadScript = true;
        for(auto iter = this->m_loadedTopLevelScripts.GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43936);
            NSTokens::Separator sep = (!firstLoadScript) ? NSTokens::Separator::CommaAndBigSpaceSeparator : NSTokens::Separator::BigSpaceSeparator;
            NSSnapValues::EmitTopLevelLoadedFunctionBodyInfo(iter.Current(), this->m_threadContext, &writer, sep);

            firstLoadScript = false;
        }
        writer.AdjustIndent(-1);
        writer.WriteSequenceEnd(NSTokens::Separator::BigSpaceSeparator);

        writer.WriteLengthValue(this->m_newFunctionTopLevelScripts.Count(), NSTokens::Separator::CommaSeparator);
        writer.WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
        writer.AdjustIndent(1);
        bool firstNewScript = true;
        for(auto iter = this->m_newFunctionTopLevelScripts.GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43937);
            NSTokens::Separator sep = (!firstNewScript) ? NSTokens::Separator::CommaAndBigSpaceSeparator : NSTokens::Separator::BigSpaceSeparator;
            NSSnapValues::EmitTopLevelNewFunctionBodyInfo(iter.Current(), this->m_threadContext, &writer, sep);

            firstNewScript = false;
        }
        writer.AdjustIndent(-1);
        writer.WriteSequenceEnd(NSTokens::Separator::BigSpaceSeparator);

        writer.WriteLengthValue(this->m_evalTopLevelScripts.Count(), NSTokens::Separator::CommaSeparator);
        writer.WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
        writer.AdjustIndent(1);
        bool firstEvalScript = true;
        for(auto iter = this->m_evalTopLevelScripts.GetIterator(); iter.IsValid(); iter.MoveNext())
        {TRACE_IT(43938);
            NSTokens::Separator sep = (!firstEvalScript) ? NSTokens::Separator::CommaAndBigSpaceSeparator : NSTokens::Separator::BigSpaceSeparator;
            NSSnapValues::EmitTopLevelEvalFunctionBodyInfo(iter.Current(), this->m_threadContext, &writer, sep);

            firstEvalScript = false;
        }
        writer.AdjustIndent(-1);
        writer.WriteSequenceEnd(NSTokens::Separator::BigSpaceSeparator);
        //

        writer.AdjustIndent(-1);
        writer.WriteRecordEnd(NSTokens::Separator::BigSpaceSeparator);

        writer.FlushAndClose();

        iofp.ActiveTTUriLength = 0;
        iofp.ActiveTTUri = nullptr;
    }

    void EventLog::ParseLogInto(TTDataIOInfo& iofp, const char* parseUri, size_t parseUriLength)
    {TRACE_IT(43939);
        iofp.ActiveTTUriLength = parseUriLength;
        iofp.ActiveTTUri = parseUri;

        const char* logfilename = "ttdlog.log";
        JsTTDStreamHandle logHandle = iofp.pfOpenResourceStream(iofp.ActiveTTUriLength, iofp.ActiveTTUri, strlen(logfilename), logfilename, true, false);
        TTDAssert(logHandle != nullptr, "Failed to initialize strem for reading TTD Log.");

        TTD_LOG_READER reader(logHandle, iofp.pfReadBytesFromStream, iofp.pfFlushAndCloseStream);

        reader.ReadRecordStart();

        TTString archString;
        reader.ReadString(NSTokens::Key::arch, this->m_miscSlabAllocator, archString);

#if defined(_M_IX86)
        TTDAssert(wcscmp(_u("x86"), archString.Contents) == 0, "Mismatch in arch between record and replay!!!");
#elif defined(_M_X64)
        TTDAssert(wcscmp(_u("x64"), archString.Contents) == 0, "Mismatch in arch between record and replay!!!");
#elif defined(_M_ARM)
        TTDAssert(wcscmp(_u("arm64"), archString.Contents) == 0, "Mismatch in arch between record and replay!!!");
#else
        TTDAssert(false, "Unknown arch!!!");
#endif

        //This is informational only so just read off the value and ignore
        TTString platformString;
        reader.ReadString(NSTokens::Key::platform, this->m_miscSlabAllocator, platformString, true);

        bool diagEnabled = reader.ReadBool(NSTokens::Key::diagEnabled, true);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        TTDAssert(diagEnabled, "Diag was enabled in record so it shoud be in replay as well!!!");
#else
        TTDAssert(!diagEnabled, "Diag was *not* enabled in record so it shoud *not* be in replay either!!!");
#endif

        reader.ReadUInt64(NSTokens::Key::usedMemory, true);
        reader.ReadUInt64(NSTokens::Key::reservedMemory, true);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        JsUtil::Stack<int64, HeapAllocator> callNestingStack(&HeapAllocator::Instance);

        bool doSep = false;
#endif

        uint32 ecount = reader.ReadLengthValue(true);
        reader.ReadSequenceStart_WDefaultKey(true);
        for(uint32 i = 0; i < ecount; ++i)
        {TRACE_IT(43940);
            NSLogEvents::EventLogEntry* evt = this->m_eventList.GetNextAvailableEntry();
            NSLogEvents::EventLogEntry_Parse(evt, this->m_eventListVTable, false, this->m_threadContext, &reader, this->m_eventSlabAllocator);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            bool isJsRTCall = (evt->EventKind == NSLogEvents::EventKind::CallExistingFunctionActionTag);
            bool isExternalCall = (evt->EventKind == NSLogEvents::EventKind::ExternalCallTag);
            bool isRegisterCall = (evt->EventKind == NSLogEvents::EventKind::ExternalCbRegisterCall);
            if(isJsRTCall | isExternalCall | isRegisterCall)
            {TRACE_IT(43941);
                reader.ReadSequenceStart(false);

                int64 lastNestedTime = -1;
                if(isJsRTCall)
                {TRACE_IT(43942);
                    lastNestedTime = NSLogEvents::JsRTCallFunctionAction_GetLastNestedEventTime(evt);
                }
                else if(isExternalCall)
                {TRACE_IT(43943);
                    lastNestedTime = NSLogEvents::ExternalCallEventLogEntry_GetLastNestedEventTime(evt);
                }
                else
                {TRACE_IT(43944);
                    lastNestedTime = NSLogEvents::ExternalCbRegisterCallEventLogEntry_GetLastNestedEventTime(evt);
                }
                callNestingStack.Push(lastNestedTime);
            }

            doSep = (!isJsRTCall & !isExternalCall & !isRegisterCall);

            while(callNestingStack.Count() != 0 && evt->EventTimeStamp == callNestingStack.Peek())
            {TRACE_IT(43945);
                callNestingStack.Pop();
                reader.ReadSequenceEnd();
            }
#endif
        }
        reader.ReadSequenceEnd();

        //parse the properties
        uint32 propertyCount = reader.ReadLengthValue(true);
        reader.ReadSequenceStart_WDefaultKey(true);
        for(uint32 i = 0; i < propertyCount; ++i)
        {TRACE_IT(43946);
            NSSnapType::SnapPropertyRecord* sRecord = this->m_propertyRecordList.NextOpenEntry();
            NSSnapType::ParseSnapPropertyRecord(sRecord, i != 0, &reader, this->m_miscSlabAllocator);
        }
        reader.ReadSequenceEnd();

        //do top level script processing here
        uint32 loadedScriptCount = reader.ReadLengthValue(true);
        reader.ReadSequenceStart_WDefaultKey(true);
        for(uint32 i = 0; i < loadedScriptCount; ++i)
        {TRACE_IT(43947);
            NSSnapValues::TopLevelScriptLoadFunctionBodyResolveInfo* fbInfo = this->m_loadedTopLevelScripts.NextOpenEntry();
            NSSnapValues::ParseTopLevelLoadedFunctionBodyInfo(fbInfo, i != 0, this->m_threadContext, &reader, this->m_miscSlabAllocator);
        }
        reader.ReadSequenceEnd();

        uint32 newScriptCount = reader.ReadLengthValue(true);
        reader.ReadSequenceStart_WDefaultKey(true);
        for(uint32 i = 0; i < newScriptCount; ++i)
        {TRACE_IT(43948);
            NSSnapValues::TopLevelNewFunctionBodyResolveInfo* fbInfo = this->m_newFunctionTopLevelScripts.NextOpenEntry();
            NSSnapValues::ParseTopLevelNewFunctionBodyInfo(fbInfo, i != 0, this->m_threadContext, &reader, this->m_miscSlabAllocator);
        }
        reader.ReadSequenceEnd();

        uint32 evalScriptCount = reader.ReadLengthValue(true);
        reader.ReadSequenceStart_WDefaultKey(true);
        for(uint32 i = 0; i < evalScriptCount; ++i)
        {TRACE_IT(43949);
            NSSnapValues::TopLevelEvalFunctionBodyResolveInfo* fbInfo = this->m_evalTopLevelScripts.NextOpenEntry();
            NSSnapValues::ParseTopLevelEvalFunctionBodyInfo(fbInfo, i != 0, this->m_threadContext, &reader, this->m_miscSlabAllocator);
        }
        reader.ReadSequenceEnd();
        //

        reader.ReadRecordEnd();
    }
}

#endif
