//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_TTD

namespace TTD
{
    TTDebuggerAbortException::TTDebuggerAbortException(uint32 abortCode, int64 optEventTime, int64 optMoveMode, const char16* staticAbortMessage)
        : m_abortCode(abortCode), m_optEventTime(optEventTime), m_optMoveMode(optMoveMode), m_staticAbortMessage(staticAbortMessage)
    {LOGMEIN("TTEvents.cpp] 12\n");
        ;
    }

    TTDebuggerAbortException::~TTDebuggerAbortException()
    {LOGMEIN("TTEvents.cpp] 17\n");
        ;
    }

    TTDebuggerAbortException TTDebuggerAbortException::CreateAbortEndOfLog(const char16* staticMessage)
    {LOGMEIN("TTEvents.cpp] 22\n");
        return TTDebuggerAbortException(1, -1, 0, staticMessage);
    }

    TTDebuggerAbortException TTDebuggerAbortException::CreateTopLevelAbortRequest(int64 targetEventTime, int64 moveMode, const char16* staticMessage)
    {LOGMEIN("TTEvents.cpp] 27\n");
        return TTDebuggerAbortException(2, targetEventTime, moveMode, staticMessage);
    }

    TTDebuggerAbortException TTDebuggerAbortException::CreateUncaughtExceptionAbortRequest(int64 targetEventTime, const char16* staticMessage)
    {LOGMEIN("TTEvents.cpp] 32\n");
        return TTDebuggerAbortException(3, targetEventTime, 0, staticMessage);
    }

    bool TTDebuggerAbortException::IsEndOfLog() const
    {LOGMEIN("TTEvents.cpp] 37\n");
        return this->m_abortCode == 1;
    }

    bool TTDebuggerAbortException::IsEventTimeMove() const
    {LOGMEIN("TTEvents.cpp] 42\n");
        return this->m_abortCode == 2;
    }

    bool TTDebuggerAbortException::IsTopLevelException() const
    {LOGMEIN("TTEvents.cpp] 47\n");
        return this->m_abortCode == 3;
    }

    int64 TTDebuggerAbortException::GetTargetEventTime() const
    {LOGMEIN("TTEvents.cpp] 52\n");
        return this->m_optEventTime;
    }

    int64 TTDebuggerAbortException::GetMoveMode() const
    {LOGMEIN("TTEvents.cpp] 57\n");
        return this->m_optMoveMode;
    }

    const char16* TTDebuggerAbortException::GetStaticAbortMessage() const
    {LOGMEIN("TTEvents.cpp] 62\n");
        return this->m_staticAbortMessage;
    }

    bool TTDebuggerSourceLocation::UpdatePostInflateFunctionBody_Helper(Js::FunctionBody* rootBody)
    {LOGMEIN("TTEvents.cpp] 67\n");
        for(uint32 i = 0; i < rootBody->GetNestedCount(); ++i)
        {LOGMEIN("TTEvents.cpp] 69\n");
            Js::ParseableFunctionInfo* ipfi = rootBody->GetNestedFunctionForExecution(i);
            Js::FunctionBody* ifb = JsSupport::ForceAndGetFunctionBody(ipfi);

            if(this->m_functionLine == ifb->GetLineNumber() && this->m_functionColumn == ifb->GetColumnNumber())
            {LOGMEIN("TTEvents.cpp] 74\n");
                this->m_functionBody = ifb;
                return true;
            }
            else
            {
                bool found = this->UpdatePostInflateFunctionBody_Helper(ifb);
                if(found)
                {LOGMEIN("TTEvents.cpp] 82\n");
                    return true;
                }
            }
        }

        return false;
    }

    TTDebuggerSourceLocation::TTDebuggerSourceLocation()
        : m_etime(-1), m_ftime(0), m_ltime(0), m_functionBody(nullptr), m_topLevelBodyId(0), m_functionLine(0), m_functionColumn(0), m_line(0), m_column(0)
    {LOGMEIN("TTEvents.cpp] 93\n");
        ;
    }

    TTDebuggerSourceLocation::TTDebuggerSourceLocation(int64 topLevelETime, const SingleCallCounter& callFrame)
        : m_etime(-1), m_ftime(0), m_ltime(0), m_functionBody(nullptr), m_topLevelBodyId(0), m_functionLine(0), m_functionColumn(0), m_line(0), m_column(0)
    {LOGMEIN("TTEvents.cpp] 99\n");
        this->SetLocation(topLevelETime, callFrame);
    }

    TTDebuggerSourceLocation::TTDebuggerSourceLocation(const TTDebuggerSourceLocation& other)
        : m_etime(other.m_etime), m_ftime(other.m_ftime), m_ltime(other.m_ltime), m_functionBody(other.m_functionBody), m_topLevelBodyId(other.m_topLevelBodyId), m_functionLine(other.m_functionLine), m_functionColumn(other.m_functionColumn), m_line(other.m_line), m_column(other.m_column)
    {LOGMEIN("TTEvents.cpp] 105\n");
        ;
    }

    TTDebuggerSourceLocation::~TTDebuggerSourceLocation()
    {LOGMEIN("TTEvents.cpp] 110\n");
        this->Clear();
    }

    TTDebuggerSourceLocation& TTDebuggerSourceLocation::operator= (const TTDebuggerSourceLocation& other)
    {LOGMEIN("TTEvents.cpp] 115\n");
        if(this != &other)
        {LOGMEIN("TTEvents.cpp] 117\n");
            this->SetLocation(other);
        }

        return *this;
    }

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
    void TTDebuggerSourceLocation::PrintToConsole(bool newline) const
    {LOGMEIN("TTEvents.cpp] 126\n");
        if(!this->HasValue())
        {LOGMEIN("TTEvents.cpp] 128\n");
            wprintf(_u("undef"));
        }
        else
        {
            const char16* fn = (this->m_functionBody != nullptr ? this->m_functionBody->GetDisplayName() : _u("[not set]"));
            wprintf(_u("%ls l:%I32u c:%I32u (%I64i, %I64i, %I64i)"), fn, this->m_line, this->m_column, this->m_etime, this->m_ftime, this->m_ltime);
        }

        if(newline)
        {LOGMEIN("TTEvents.cpp] 138\n");
            wprintf(_u("\n"));
        }
    }
#endif

    void TTDebuggerSourceLocation::Initialize()
    {LOGMEIN("TTEvents.cpp] 145\n");
        this->m_etime = -1;
        this->m_ftime = 0;
        this->m_ltime = 0;

        this->m_functionBody = nullptr;
        this->m_topLevelBodyId = 0;

        this->m_functionLine = 0;
        this->m_functionColumn = 0;
        this->m_line = 0;
        this->m_column = 0;
    }

    bool TTDebuggerSourceLocation::HasValue() const
    {LOGMEIN("TTEvents.cpp] 160\n");
        return this->m_etime != -1;
    }

    void TTDebuggerSourceLocation::Clear()
    {LOGMEIN("TTEvents.cpp] 165\n");
        this->m_etime = -1;
        this->m_ftime = 0;
        this->m_ltime = 0;

        this->m_functionBody = nullptr;
        this->m_topLevelBodyId = 0;

        this->m_functionLine = 0;
        this->m_functionColumn = 0;

        this->m_line = 0;
        this->m_column = 0;
    }

    void TTDebuggerSourceLocation::SetLocation(const TTDebuggerSourceLocation& other)
    {LOGMEIN("TTEvents.cpp] 181\n");
        this->m_etime = other.m_etime;
        this->m_ftime = other.m_ftime;
        this->m_ltime = other.m_ltime;

        this->m_functionBody = other.m_functionBody;
        this->m_topLevelBodyId = other.m_topLevelBodyId;

        this->m_functionLine = other.m_functionLine;
        this->m_functionColumn = other.m_functionColumn;

        this->m_line = other.m_line;
        this->m_column = other.m_column;
    }

    void TTDebuggerSourceLocation::SetLocation(int64 topLevelETime, const SingleCallCounter& callFrame)
    {LOGMEIN("TTEvents.cpp] 197\n");
        ULONG srcLine = 0;
        LONG srcColumn = -1;
        uint32 startOffset = callFrame.Function->GetStatementStartOffset(callFrame.CurrentStatementIndex);
        callFrame.Function->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

        this->SetLocation(topLevelETime, callFrame.FunctionTime, callFrame.LoopTime, callFrame.Function, (uint32)srcLine, (uint32)srcColumn);
    }

    void TTDebuggerSourceLocation::SetLocation(int64 etime, int64 ftime, int64 ltime, Js::FunctionBody* body, ULONG line, LONG column)
    {LOGMEIN("TTEvents.cpp] 207\n");
        this->m_etime = etime;
        this->m_ftime = ftime;
        this->m_ltime = ltime;

        this->m_functionBody = body;
        this->m_topLevelBodyId = 0;

        this->m_functionLine = body->GetLineNumber();
        this->m_functionColumn = body->GetColumnNumber();

        this->m_line = (uint32)line;
        this->m_column = (uint32)column;
    }

    int64 TTDebuggerSourceLocation::GetRootEventTime() const
    {LOGMEIN("TTEvents.cpp] 223\n");
        return this->m_etime;
    }

    int64 TTDebuggerSourceLocation::GetFunctionTime() const
    {LOGMEIN("TTEvents.cpp] 228\n");
        return this->m_ftime;
    }

    int64 TTDebuggerSourceLocation::GetLoopTime() const
    {LOGMEIN("TTEvents.cpp] 233\n");
        return this->m_ltime;
    }

    Js::FunctionBody* TTDebuggerSourceLocation::LoadFunctionBodyIfPossible(Js::ScriptContext* inCtx)
    {LOGMEIN("TTEvents.cpp] 238\n");
        if(this->m_functionBody == nullptr)
        {LOGMEIN("TTEvents.cpp] 240\n");
            Js::FunctionBody* rootBody = inCtx->TTDContextInfo->FindRootBodyByTopLevelCtr(this->m_topLevelBodyId);
            if(rootBody == nullptr)
            {LOGMEIN("TTEvents.cpp] 243\n");
                return nullptr;
            }

            if(this->m_functionLine == rootBody->GetLineNumber() && this->m_functionColumn == rootBody->GetColumnNumber())
            {LOGMEIN("TTEvents.cpp] 248\n");
                this->m_functionBody = rootBody;
            }
            else
            {
                this->UpdatePostInflateFunctionBody_Helper(rootBody);
            }

            TTDAssert(this->m_functionBody != nullptr, "We failed to remap a breakpoint during reverse move.");
        }

        return this->m_functionBody;
    }

    uint32 TTDebuggerSourceLocation::GetLine() const
    {LOGMEIN("TTEvents.cpp] 263\n");
        return this->m_line;
    }

    uint32 TTDebuggerSourceLocation::GetColumn() const
    {LOGMEIN("TTEvents.cpp] 268\n");
        return this->m_column;
    }

    void TTDebuggerSourceLocation::EnsureTopLevelBodyCtrPreInflate()
    {LOGMEIN("TTEvents.cpp] 273\n");
        if(this->m_functionBody != nullptr)
        {LOGMEIN("TTEvents.cpp] 275\n");
            this->m_topLevelBodyId = this->m_functionBody->GetScriptContext()->TTDContextInfo->FindTopLevelCtrForBody(this->m_functionBody);
            this->m_functionBody = nullptr;
        }
    }

    bool TTDebuggerSourceLocation::IsBefore(const TTDebuggerSourceLocation& other) const
    {LOGMEIN("TTEvents.cpp] 282\n");
        TTDAssert(this->m_ftime != -1 && other.m_ftime != -1, "These aren't orderable!!!");
        TTDAssert(this->m_ltime != -1 && other.m_ltime != -1, "These aren't orderable!!!");

        //first check the order of the time parts
        if(this->m_etime != other.m_etime)
        {LOGMEIN("TTEvents.cpp] 288\n");
            return this->m_etime < other.m_etime;
        }

        if(this->m_ftime != other.m_ftime)
        {LOGMEIN("TTEvents.cpp] 293\n");
            return this->m_ftime < other.m_ftime;
        }

        if(this->m_ltime != other.m_ltime)
        {LOGMEIN("TTEvents.cpp] 298\n");
            return this->m_ltime < other.m_ltime;
        }

        //so all times are the same => min column/min row decide
        if(this->m_line != other.m_line)
        {LOGMEIN("TTEvents.cpp] 304\n");
            return this->m_line < other.m_line;
        }

        if(this->m_column != other.m_column)
        {LOGMEIN("TTEvents.cpp] 309\n");
            return this->m_column < other.m_column;
        }

        //they are refering to the same location so this is *not* stricly before
        return false;
    }

    //////////////////

    namespace NSLogEvents
    {
        void PassVarToHostInReplay(ThreadContextTTD* executeContext, TTDVar origVar, Js::Var replayVar)
        {LOGMEIN("TTEvents.cpp] 322\n");
            static_assert(sizeof(TTDVar) == sizeof(Js::Var), "We assume the bit patterns on these types are the same!!!");

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            if(replayVar == nullptr || TTD::JsSupport::IsVarTaggedInline(replayVar))
            {LOGMEIN("TTEvents.cpp] 327\n");
                TTDAssert(TTD::JsSupport::AreInlineVarsEquiv(origVar, replayVar), "Should be same bit pattern.");
            }
#endif

            if(replayVar != nullptr && TTD::JsSupport::IsVarPtrValued(replayVar))
            {LOGMEIN("TTEvents.cpp] 333\n");
                Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(replayVar);
                if(!ThreadContextTTD::IsSpecialRootObject(obj))
                {LOGMEIN("TTEvents.cpp] 336\n");
                    executeContext->AddLocalRoot(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(origVar), obj);
                }
            }
        }

        Js::Var InflateVarInReplay(ThreadContextTTD* executeContext, TTDVar origVar)
        {LOGMEIN("TTEvents.cpp] 343\n");
            static_assert(sizeof(TTDVar) == sizeof(Js::Var), "We assume the bit patterns on these types are the same!!!");

            if(origVar == nullptr || TTD::JsSupport::IsVarTaggedInline(origVar))
            {LOGMEIN("TTEvents.cpp] 347\n");
                return TTD_CONVERT_TTDVAR_TO_JSVAR(origVar);
            }
            else
            {
                return executeContext->LookupObjectForLogID(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(origVar));
            }
        }

        void EventLogEntry_Initialize(EventLogEntry* evt, EventKind tag, int64 etime)
        {LOGMEIN("TTEvents.cpp] 357\n");
            evt->EventKind = tag;
            evt->ResultStatus = -1;

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            evt->EventTimeStamp = etime;
#endif
        }

        void EventLogEntry_Emit(const EventLogEntry* evt, EventLogEntryVTableEntry* evtFPVTable, FileWriter* writer, ThreadContext* threadContext, NSTokens::Separator separator)
        {LOGMEIN("TTEvents.cpp] 367\n");
            writer->WriteRecordStart(separator);

            writer->WriteTag<EventKind>(NSTokens::Key::eventKind, evt->EventKind);
            writer->WriteInt32(NSTokens::Key::eventResultStatus, evt->ResultStatus, NSTokens::Separator::CommaSeparator);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            writer->WriteInt64(NSTokens::Key::eventTime, evt->EventTimeStamp, NSTokens::Separator::CommaSeparator);
#endif

            auto emitFP = evtFPVTable[(uint32)evt->EventKind].EmitFP;
            if(emitFP != nullptr)
            {
                emitFP(evt, writer, threadContext);
            }

            writer->WriteRecordEnd();
        }

        void EventLogEntry_Parse(EventLogEntry* evt, EventLogEntryVTableEntry* evtFPVTable, bool readSeperator, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 387\n");
            reader->ReadRecordStart(readSeperator);

            evt->EventKind = reader->ReadTag<EventKind>(NSTokens::Key::eventKind);
            evt->ResultStatus = reader->ReadInt32(NSTokens::Key::eventResultStatus, true);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            evt->EventTimeStamp = reader->ReadInt64(NSTokens::Key::eventTime, true);
#endif

            auto parseFP = evtFPVTable[(uint32)evt->EventKind].ParseFP;
            if(parseFP != nullptr)
            {
                parseFP(evt, threadContext, reader, alloc);
            }

            reader->ReadRecordEnd();
        }

        bool EventFailsWithRuntimeError(const EventLogEntry* evt)
        {LOGMEIN("TTEvents.cpp] 407\n");
            return !(EventDoesNotReturn(evt) || EventCompletesNormally(evt) || EventCompletesWithException(evt));
        }

        bool EventDoesNotReturn(const EventLogEntry* evt)
        {LOGMEIN("TTEvents.cpp] 412\n");
            return evt->ResultStatus == -1;
        }

        bool EventCompletesNormally(const EventLogEntry* evt)
        {LOGMEIN("TTEvents.cpp] 417\n");
            return (evt->ResultStatus == 0) || (evt->ResultStatus == TTD_REPLAY_JsErrorInvalidArgument) || (evt->ResultStatus == TTD_REPLAY_JsErrorArgumentNotObject);
        }

        bool EventCompletesWithException(const EventLogEntry* evt)
        {LOGMEIN("TTEvents.cpp] 422\n");
            return (evt->ResultStatus == TTD_REPLAY_JsErrorCategoryScript) || (evt->ResultStatus == TTD_REPLAY_JsErrorScriptTerminated);
        }

        //////////////////

        void SnapshotEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 429\n");
            SnapshotEventLogEntry_UnloadSnapshot(evt);
        }

        void SnapshotEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 434\n");
            const SnapshotEventLogEntry* snapEvt = GetInlineEventDataAs<SnapshotEventLogEntry, EventKind::SnapshotTag>(evt);

            writer->WriteInt64(NSTokens::Key::restoreTime, snapEvt->RestoreTimestamp, NSTokens::Separator::CommaSeparator);

            if(snapEvt->Snap != nullptr)
            {LOGMEIN("TTEvents.cpp] 440\n");
                snapEvt->Snap->EmitSnapshot(snapEvt->RestoreTimestamp, threadContext);
            }
        }

        void SnapshotEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 446\n");
            SnapshotEventLogEntry* snapEvt = GetInlineEventDataAs<SnapshotEventLogEntry, EventKind::SnapshotTag>(evt);

            snapEvt->RestoreTimestamp = reader->ReadInt64(NSTokens::Key::restoreTime, true);
            snapEvt->Snap = nullptr;
        }

        void SnapshotEventLogEntry_EnsureSnapshotDeserialized(EventLogEntry* evt, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 454\n");
            SnapshotEventLogEntry* snapEvt = GetInlineEventDataAs<SnapshotEventLogEntry, EventKind::SnapshotTag>(evt);

            if(snapEvt->Snap == nullptr)
            {LOGMEIN("TTEvents.cpp] 458\n");
                snapEvt->Snap = SnapShot::Parse(snapEvt->RestoreTimestamp, threadContext);
            }
        }

        void SnapshotEventLogEntry_UnloadSnapshot(EventLogEntry* evt)
        {LOGMEIN("TTEvents.cpp] 464\n");
            SnapshotEventLogEntry* snapEvt = GetInlineEventDataAs<SnapshotEventLogEntry, EventKind::SnapshotTag>(evt);

            if(snapEvt->Snap != nullptr)
            {
                TT_HEAP_DELETE(SnapShot, snapEvt->Snap);
                snapEvt->Snap = nullptr;
            }
        }

        void EventLoopYieldPointEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 475\n");
            const EventLoopYieldPointEntry* ypEvt = GetInlineEventDataAs<EventLoopYieldPointEntry, EventKind::EventLoopYieldPointTag>(evt);

            writer->WriteUInt64(NSTokens::Key::eventTime, ypEvt->EventTimeStamp, NSTokens::Separator::CommaSeparator);
            writer->WriteDouble(NSTokens::Key::loopTime, ypEvt->EventWallTime, NSTokens::Separator::CommaSeparator);
        }

        void EventLoopYieldPointEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 483\n");
            EventLoopYieldPointEntry* ypEvt = GetInlineEventDataAs<EventLoopYieldPointEntry, EventKind::EventLoopYieldPointTag>(evt);

            ypEvt->EventTimeStamp = reader->ReadUInt64(NSTokens::Key::eventTime, true);
            ypEvt->EventWallTime = reader->ReadDouble(NSTokens::Key::loopTime, true);
        }

        //////////////////

        void CodeLoadEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 493\n");
            const CodeLoadEventLogEntry* codeEvt = GetInlineEventDataAs<CodeLoadEventLogEntry, EventKind::TopLevelCodeTag>(evt);

            writer->WriteUInt64(NSTokens::Key::u64Val, codeEvt->BodyCounterId, NSTokens::Separator::CommaSeparator);
        }

        void CodeLoadEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 500\n");
            CodeLoadEventLogEntry* codeEvt = GetInlineEventDataAs<CodeLoadEventLogEntry, EventKind::TopLevelCodeTag>(evt);

            codeEvt->BodyCounterId = reader->ReadUInt64(NSTokens::Key::u64Val, true);
        }

        void TelemetryEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 507\n");
            TelemetryEventLogEntry* telemetryEvt = GetInlineEventDataAs<TelemetryEventLogEntry, EventKind::TelemetryLogTag>(evt);

            alloc.UnlinkString(telemetryEvt->InfoString);
        }

        void TelemetryEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 514\n");
            const TelemetryEventLogEntry* telemetryEvt = GetInlineEventDataAs<TelemetryEventLogEntry, EventKind::TelemetryLogTag>(evt);

            writer->WriteString(NSTokens::Key::stringVal, telemetryEvt->InfoString, NSTokens::Separator::CommaSeparator);
            writer->WriteBool(NSTokens::Key::boolVal, telemetryEvt->DoPrint, NSTokens::Separator::CommaSeparator);
        }

        void TelemetryEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 522\n");
            TelemetryEventLogEntry* telemetryEvt = GetInlineEventDataAs<TelemetryEventLogEntry, EventKind::TelemetryLogTag>(evt);

            reader->ReadString(NSTokens::Key::stringVal, alloc, telemetryEvt->InfoString, true);
            telemetryEvt->DoPrint = reader->ReadBool(NSTokens::Key::boolVal, true);
        }

        //////////////////

        void RandomSeedEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 532\n");
            const RandomSeedEventLogEntry* rndEvt = GetInlineEventDataAs<RandomSeedEventLogEntry, EventKind::RandomSeedTag>(evt);

            writer->WriteUInt64(NSTokens::Key::u64Val, rndEvt->Seed0, NSTokens::Separator::CommaSeparator);
            writer->WriteUInt64(NSTokens::Key::u64Val, rndEvt->Seed1, NSTokens::Separator::CommaSeparator);

        }

        void RandomSeedEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 541\n");
            RandomSeedEventLogEntry* rndEvt = GetInlineEventDataAs<RandomSeedEventLogEntry, EventKind::RandomSeedTag>(evt);

            rndEvt->Seed0 = reader->ReadUInt64(NSTokens::Key::u64Val, true);
            rndEvt->Seed1 = reader->ReadUInt64(NSTokens::Key::u64Val, true);
        }

        void DoubleEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 549\n");
            const DoubleEventLogEntry* dblEvt = GetInlineEventDataAs<DoubleEventLogEntry, EventKind::DoubleTag>(evt);

            writer->WriteDouble(NSTokens::Key::doubleVal, dblEvt->DoubleValue, NSTokens::Separator::CommaSeparator);
        }

        void DoubleEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 556\n");
            DoubleEventLogEntry* dblEvt = GetInlineEventDataAs<DoubleEventLogEntry, EventKind::DoubleTag>(evt);

            dblEvt->DoubleValue = reader->ReadDouble(NSTokens::Key::doubleVal, true);
        }

        void StringValueEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 563\n");
            StringValueEventLogEntry* strEvt = GetInlineEventDataAs<StringValueEventLogEntry, EventKind::StringTag>(evt);

            alloc.UnlinkString(strEvt->StringValue);
        }

        void StringValueEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 570\n");
            const StringValueEventLogEntry* strEvt = GetInlineEventDataAs<StringValueEventLogEntry, EventKind::StringTag>(evt);

            writer->WriteString(NSTokens::Key::stringVal, strEvt->StringValue, NSTokens::Separator::CommaSeparator);
        }

        void StringValueEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 577\n");
            StringValueEventLogEntry* strEvt = GetInlineEventDataAs<StringValueEventLogEntry, EventKind::StringTag>(evt);

            reader->ReadString(NSTokens::Key::stringVal, alloc, strEvt->StringValue, true);
        }

        //////////////////

        void PropertyEnumStepEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 586\n");
            PropertyEnumStepEventLogEntry* propertyEvt = GetInlineEventDataAs<PropertyEnumStepEventLogEntry, EventKind::PropertyEnumTag>(evt);

            if(!IsNullPtrTTString(propertyEvt->PropertyString))
            {LOGMEIN("TTEvents.cpp] 590\n");
                alloc.UnlinkString(propertyEvt->PropertyString);
            }
        }

        void PropertyEnumStepEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 596\n");
            const PropertyEnumStepEventLogEntry* propertyEvt = GetInlineEventDataAs<PropertyEnumStepEventLogEntry, EventKind::PropertyEnumTag>(evt);

            writer->WriteBool(NSTokens::Key::boolVal, !!propertyEvt->ReturnCode, NSTokens::Separator::CommaSeparator);
            writer->WriteUInt32(NSTokens::Key::propertyId, propertyEvt->Pid, NSTokens::Separator::CommaSeparator);
            writer->WriteUInt32(NSTokens::Key::attributeFlags, propertyEvt->Attributes, NSTokens::Separator::CommaSeparator);

            if(propertyEvt->ReturnCode)
            {LOGMEIN("TTEvents.cpp] 604\n");
#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
                writer->WriteString(NSTokens::Key::stringVal, propertyEvt->PropertyString, NSTokens::Separator::CommaSeparator);
#else
                if(propertyEvt->Pid == Js::Constants::NoProperty)
                {LOGMEIN("TTEvents.cpp] 609\n");
                    writer->WriteString(NSTokens::Key::stringVal, propertyEvt->PropertyString, NSTokens::Separator::CommaSeparator);
                }
#endif
            }
        }

        void PropertyEnumStepEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 617\n");
            PropertyEnumStepEventLogEntry* propertyEvt = GetInlineEventDataAs<PropertyEnumStepEventLogEntry, EventKind::PropertyEnumTag>(evt);

            propertyEvt->ReturnCode = reader->ReadBool(NSTokens::Key::boolVal, true);
            propertyEvt->Pid = (Js::PropertyId)reader->ReadUInt32(NSTokens::Key::propertyId, true);
            propertyEvt->Attributes = (Js::PropertyAttributes)reader->ReadUInt32(NSTokens::Key::attributeFlags, true);

            InitializeAsNullPtrTTString(propertyEvt->PropertyString);

            if(propertyEvt->ReturnCode)
            {LOGMEIN("TTEvents.cpp] 627\n");
#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
                reader->ReadString(NSTokens::Key::stringVal, alloc, propertyEvt->PropertyString, true);
#else
                if(propertyEvt->Pid == Js::Constants::NoProperty)
                {LOGMEIN("TTEvents.cpp] 632\n");
                    reader->ReadString(NSTokens::Key::stringVal, alloc, propertyEvt->PropertyString, true);
                }
#endif
            }
        }

        //////////////////

        void SymbolCreationEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 642\n");
            const SymbolCreationEventLogEntry* symEvt = GetInlineEventDataAs<SymbolCreationEventLogEntry, EventKind::SymbolCreationTag>(evt);

            writer->WriteUInt32(NSTokens::Key::propertyId, symEvt->Pid, NSTokens::Separator::CommaSeparator);
        }

        void SymbolCreationEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 649\n");
            SymbolCreationEventLogEntry* symEvt = GetInlineEventDataAs<SymbolCreationEventLogEntry, EventKind::SymbolCreationTag>(evt);

            symEvt->Pid = (Js::PropertyId)reader->ReadUInt32(NSTokens::Key::propertyId, true);
        }

        //////////////////

        int64 ExternalCbRegisterCallEventLogEntry_GetLastNestedEventTime(const EventLogEntry* evt)
        {LOGMEIN("TTEvents.cpp] 658\n");
            const ExternalCbRegisterCallEventLogEntry* cbrEvt = GetInlineEventDataAs<ExternalCbRegisterCallEventLogEntry, EventKind::ExternalCbRegisterCall>(evt);

            return cbrEvt->LastNestedEventTime;
        }

        void ExternalCbRegisterCallEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 665\n");
            const ExternalCbRegisterCallEventLogEntry* cbrEvt = GetInlineEventDataAs<ExternalCbRegisterCallEventLogEntry, EventKind::ExternalCbRegisterCall>(evt);

            NSSnapValues::EmitTTDVar(cbrEvt->CallbackFunction, writer, NSTokens::Separator::CommaSeparator);
            writer->WriteInt64(NSTokens::Key::i64Val, cbrEvt->LastNestedEventTime, NSTokens::Separator::CommaSeparator);
        }

        void ExternalCbRegisterCallEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 673\n");
            ExternalCbRegisterCallEventLogEntry* cbrEvt = GetInlineEventDataAs<ExternalCbRegisterCallEventLogEntry, EventKind::ExternalCbRegisterCall>(evt);

            cbrEvt->CallbackFunction = NSSnapValues::ParseTTDVar(true, reader);
            cbrEvt->LastNestedEventTime = reader->ReadInt64(NSTokens::Key::i64Val, true);
        }

        //////////////////

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        void ExternalCallEventLogEntry_ProcessDiagInfoPre(EventLogEntry* evt, Js::JavascriptFunction* function, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 684\n");
            ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

            Js::JavascriptString* displayName = function->GetDisplayName();
            alloc.CopyStringIntoWLength(displayName->GetSz(), displayName->GetLength(), callEvt->AdditionalInfo->FunctionName);
        }
#endif

        int64 ExternalCallEventLogEntry_GetLastNestedEventTime(const EventLogEntry* evt)
        {LOGMEIN("TTEvents.cpp] 693\n");
            const ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

            return callEvt->AdditionalInfo->LastNestedEventTime;
        }

        void ExternalCallEventLogEntry_ProcessArgs(EventLogEntry* evt, int32 rootDepth, Js::JavascriptFunction* function, uint32 argc, Js::Var* argv, bool checkExceptions, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 700\n");
            ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);
            callEvt->AdditionalInfo = alloc.SlabAllocateStruct<ExternalCallEventLogEntry_AdditionalInfo>();

            callEvt->RootNestingDepth = rootDepth;
            callEvt->ArgCount = argc + 1;

            static_assert(sizeof(TTDVar) == sizeof(Js::Var), "These need to be the same size (and have same bit layout) for this to work!");

            callEvt->ArgArray = alloc.SlabAllocateArray<TTDVar>(callEvt->ArgCount);
            callEvt->ArgArray[0] = static_cast<TTDVar>(function);
            js_memcpy_s(callEvt->ArgArray + 1, (callEvt->ArgCount - 1) * sizeof(TTDVar), argv, argc * sizeof(Js::Var));

            callEvt->ReturnValue = nullptr;
            callEvt->AdditionalInfo->LastNestedEventTime = TTD_EVENT_MAXTIME;

            callEvt->AdditionalInfo->CheckExceptionStatus = checkExceptions;
        }

        void ExternalCallEventLogEntry_ProcessReturn(EventLogEntry* evt, Js::Var res, int64 lastNestedEvent)
        {LOGMEIN("TTEvents.cpp] 720\n");
            ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

            callEvt->ReturnValue = TTD_CONVERT_JSVAR_TO_TTDVAR(res);
            callEvt->AdditionalInfo->LastNestedEventTime = lastNestedEvent;
        }

        void ExternalCallEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 728\n");
            ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

            alloc.UnlinkAllocation(callEvt->ArgArray);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            alloc.UnlinkString(callEvt->AdditionalInfo->FunctionName);
#endif

            alloc.UnlinkAllocation(callEvt->AdditionalInfo);
        }

        void ExternalCallEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 741\n");
            const ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            writer->WriteString(NSTokens::Key::name, callEvt->AdditionalInfo->FunctionName, NSTokens::Separator::CommaSeparator);
#endif

            writer->WriteInt32(NSTokens::Key::rootNestingDepth, callEvt->RootNestingDepth, NSTokens::Separator::CommaSeparator);

            writer->WriteLengthValue(callEvt->ArgCount, NSTokens::Separator::CommaSeparator);
            writer->WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
            for(uint32 i = 0; i < callEvt->ArgCount; ++i)
            {LOGMEIN("TTEvents.cpp] 753\n");
                NSTokens::Separator sep = (i != 0) ? NSTokens::Separator::CommaSeparator : NSTokens::Separator::NoSeparator;
                NSSnapValues::EmitTTDVar(callEvt->ArgArray[i], writer, sep);
            }
            writer->WriteSequenceEnd();

            writer->WriteKey(NSTokens::Key::argRetVal, NSTokens::Separator::CommaSeparator);
            NSSnapValues::EmitTTDVar(callEvt->ReturnValue, writer, NSTokens::Separator::NoSeparator);

            writer->WriteBool(NSTokens::Key::boolVal, callEvt->AdditionalInfo->CheckExceptionStatus, NSTokens::Separator::CommaSeparator);

            writer->WriteInt64(NSTokens::Key::i64Val, callEvt->AdditionalInfo->LastNestedEventTime, NSTokens::Separator::CommaSeparator);
        }

        void ExternalCallEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 768\n");
            ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);
            callEvt->AdditionalInfo = alloc.SlabAllocateStruct<ExternalCallEventLogEntry_AdditionalInfo>();

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            reader->ReadString(NSTokens::Key::name, alloc, callEvt->AdditionalInfo->FunctionName, true);
#endif

            callEvt->RootNestingDepth = reader->ReadInt32(NSTokens::Key::rootNestingDepth, true);

            callEvt->ArgCount = reader->ReadLengthValue(true);
            callEvt->ArgArray = alloc.SlabAllocateArray<TTDVar>(callEvt->ArgCount);

            reader->ReadSequenceStart_WDefaultKey(true);
            for(uint32 i = 0; i < callEvt->ArgCount; ++i)
            {LOGMEIN("TTEvents.cpp] 783\n");
                callEvt->ArgArray[i] = NSSnapValues::ParseTTDVar(i != 0, reader);
            }
            reader->ReadSequenceEnd();

            reader->ReadKey(NSTokens::Key::argRetVal, true);
            callEvt->ReturnValue = NSSnapValues::ParseTTDVar(false, reader);

            callEvt->AdditionalInfo->CheckExceptionStatus = reader->ReadBool(NSTokens::Key::boolVal, true);

            callEvt->AdditionalInfo->LastNestedEventTime = reader->ReadInt64(NSTokens::Key::i64Val, true);
        }

        //////////////////

        void ExplicitLogWriteEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTEvents.cpp] 799\n");
            ; //We don't track any extra data with this
        }

        void ExplicitLogWriteEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTEvents.cpp] 804\n");
            ; //We don't track any extra data with this
        }
    }
}

#endif
