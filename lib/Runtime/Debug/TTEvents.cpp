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
    {TRACE_IT(43972);
        ;
    }

    TTDebuggerAbortException::~TTDebuggerAbortException()
    {TRACE_IT(43973);
        ;
    }

    TTDebuggerAbortException TTDebuggerAbortException::CreateAbortEndOfLog(const char16* staticMessage)
    {TRACE_IT(43974);
        return TTDebuggerAbortException(1, -1, 0, staticMessage);
    }

    TTDebuggerAbortException TTDebuggerAbortException::CreateTopLevelAbortRequest(int64 targetEventTime, int64 moveMode, const char16* staticMessage)
    {TRACE_IT(43975);
        return TTDebuggerAbortException(2, targetEventTime, moveMode, staticMessage);
    }

    TTDebuggerAbortException TTDebuggerAbortException::CreateUncaughtExceptionAbortRequest(int64 targetEventTime, const char16* staticMessage)
    {TRACE_IT(43976);
        return TTDebuggerAbortException(3, targetEventTime, 0, staticMessage);
    }

    bool TTDebuggerAbortException::IsEndOfLog() const
    {TRACE_IT(43977);
        return this->m_abortCode == 1;
    }

    bool TTDebuggerAbortException::IsEventTimeMove() const
    {TRACE_IT(43978);
        return this->m_abortCode == 2;
    }

    bool TTDebuggerAbortException::IsTopLevelException() const
    {TRACE_IT(43979);
        return this->m_abortCode == 3;
    }

    int64 TTDebuggerAbortException::GetTargetEventTime() const
    {TRACE_IT(43980);
        return this->m_optEventTime;
    }

    int64 TTDebuggerAbortException::GetMoveMode() const
    {TRACE_IT(43981);
        return this->m_optMoveMode;
    }

    const char16* TTDebuggerAbortException::GetStaticAbortMessage() const
    {TRACE_IT(43982);
        return this->m_staticAbortMessage;
    }

    bool TTDebuggerSourceLocation::UpdatePostInflateFunctionBody_Helper(Js::FunctionBody* rootBody)
    {TRACE_IT(43983);
        for(uint32 i = 0; i < rootBody->GetNestedCount(); ++i)
        {TRACE_IT(43984);
            Js::ParseableFunctionInfo* ipfi = rootBody->GetNestedFunctionForExecution(i);
            Js::FunctionBody* ifb = JsSupport::ForceAndGetFunctionBody(ipfi);

            if(this->m_functionLine == ifb->GetLineNumber() && this->m_functionColumn == ifb->GetColumnNumber())
            {TRACE_IT(43985);
                this->m_functionBody = ifb;
                return true;
            }
            else
            {TRACE_IT(43986);
                bool found = this->UpdatePostInflateFunctionBody_Helper(ifb);
                if(found)
                {TRACE_IT(43987);
                    return true;
                }
            }
        }

        return false;
    }

    TTDebuggerSourceLocation::TTDebuggerSourceLocation()
        : m_etime(-1), m_ftime(0), m_ltime(0), m_functionBody(nullptr), m_topLevelBodyId(0), m_functionLine(0), m_functionColumn(0), m_line(0), m_column(0)
    {TRACE_IT(43988);
        ;
    }

    TTDebuggerSourceLocation::TTDebuggerSourceLocation(int64 topLevelETime, const SingleCallCounter& callFrame)
        : m_etime(-1), m_ftime(0), m_ltime(0), m_functionBody(nullptr), m_topLevelBodyId(0), m_functionLine(0), m_functionColumn(0), m_line(0), m_column(0)
    {TRACE_IT(43989);
        this->SetLocation(topLevelETime, callFrame);
    }

    TTDebuggerSourceLocation::TTDebuggerSourceLocation(const TTDebuggerSourceLocation& other)
        : m_etime(other.m_etime), m_ftime(other.m_ftime), m_ltime(other.m_ltime), m_functionBody(other.m_functionBody), m_topLevelBodyId(other.m_topLevelBodyId), m_functionLine(other.m_functionLine), m_functionColumn(other.m_functionColumn), m_line(other.m_line), m_column(other.m_column)
    {TRACE_IT(43990);
        ;
    }

    TTDebuggerSourceLocation::~TTDebuggerSourceLocation()
    {TRACE_IT(43991);
        this->Clear();
    }

    TTDebuggerSourceLocation& TTDebuggerSourceLocation::operator= (const TTDebuggerSourceLocation& other)
    {TRACE_IT(43992);
        if(this != &other)
        {TRACE_IT(43993);
            this->SetLocation(other);
        }

        return *this;
    }

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
    void TTDebuggerSourceLocation::PrintToConsole(bool newline) const
    {TRACE_IT(43994);
        if(!this->HasValue())
        {TRACE_IT(43995);
            wprintf(_u("undef"));
        }
        else
        {TRACE_IT(43996);
            const char16* fn = (this->m_functionBody != nullptr ? this->m_functionBody->GetDisplayName() : _u("[not set]"));
            wprintf(_u("%ls l:%I32u c:%I32u (%I64i, %I64i, %I64i)"), fn, this->m_line, this->m_column, this->m_etime, this->m_ftime, this->m_ltime);
        }

        if(newline)
        {TRACE_IT(43997);
            wprintf(_u("\n"));
        }
    }
#endif

    void TTDebuggerSourceLocation::Initialize()
    {TRACE_IT(43998);
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
    {TRACE_IT(43999);
        return this->m_etime != -1;
    }

    void TTDebuggerSourceLocation::Clear()
    {TRACE_IT(44000);
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
    {TRACE_IT(44001);
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
    {TRACE_IT(44002);
        ULONG srcLine = 0;
        LONG srcColumn = -1;
        uint32 startOffset = callFrame.Function->GetStatementStartOffset(callFrame.CurrentStatementIndex);
        callFrame.Function->GetSourceLineFromStartOffset_TTD(startOffset, &srcLine, &srcColumn);

        this->SetLocation(topLevelETime, callFrame.FunctionTime, callFrame.LoopTime, callFrame.Function, (uint32)srcLine, (uint32)srcColumn);
    }

    void TTDebuggerSourceLocation::SetLocation(int64 etime, int64 ftime, int64 ltime, Js::FunctionBody* body, ULONG line, LONG column)
    {TRACE_IT(44003);
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
    {TRACE_IT(44004);
        return this->m_etime;
    }

    int64 TTDebuggerSourceLocation::GetFunctionTime() const
    {TRACE_IT(44005);
        return this->m_ftime;
    }

    int64 TTDebuggerSourceLocation::GetLoopTime() const
    {TRACE_IT(44006);
        return this->m_ltime;
    }

    Js::FunctionBody* TTDebuggerSourceLocation::LoadFunctionBodyIfPossible(Js::ScriptContext* inCtx)
    {TRACE_IT(44007);
        if(this->m_functionBody == nullptr)
        {TRACE_IT(44008);
            Js::FunctionBody* rootBody = inCtx->TTDContextInfo->FindRootBodyByTopLevelCtr(this->m_topLevelBodyId);
            if(rootBody == nullptr)
            {TRACE_IT(44009);
                return nullptr;
            }

            if(this->m_functionLine == rootBody->GetLineNumber() && this->m_functionColumn == rootBody->GetColumnNumber())
            {TRACE_IT(44010);
                this->m_functionBody = rootBody;
            }
            else
            {TRACE_IT(44011);
                this->UpdatePostInflateFunctionBody_Helper(rootBody);
            }

            TTDAssert(this->m_functionBody != nullptr, "We failed to remap a breakpoint during reverse move.");
        }

        return this->m_functionBody;
    }

    uint32 TTDebuggerSourceLocation::GetLine() const
    {TRACE_IT(44012);
        return this->m_line;
    }

    uint32 TTDebuggerSourceLocation::GetColumn() const
    {TRACE_IT(44013);
        return this->m_column;
    }

    void TTDebuggerSourceLocation::EnsureTopLevelBodyCtrPreInflate()
    {TRACE_IT(44014);
        if(this->m_functionBody != nullptr)
        {TRACE_IT(44015);
            this->m_topLevelBodyId = this->m_functionBody->GetScriptContext()->TTDContextInfo->FindTopLevelCtrForBody(this->m_functionBody);
            this->m_functionBody = nullptr;
        }
    }

    bool TTDebuggerSourceLocation::IsBefore(const TTDebuggerSourceLocation& other) const
    {TRACE_IT(44016);
        TTDAssert(this->m_ftime != -1 && other.m_ftime != -1, "These aren't orderable!!!");
        TTDAssert(this->m_ltime != -1 && other.m_ltime != -1, "These aren't orderable!!!");

        //first check the order of the time parts
        if(this->m_etime != other.m_etime)
        {TRACE_IT(44017);
            return this->m_etime < other.m_etime;
        }

        if(this->m_ftime != other.m_ftime)
        {TRACE_IT(44018);
            return this->m_ftime < other.m_ftime;
        }

        if(this->m_ltime != other.m_ltime)
        {TRACE_IT(44019);
            return this->m_ltime < other.m_ltime;
        }

        //so all times are the same => min column/min row decide
        if(this->m_line != other.m_line)
        {TRACE_IT(44020);
            return this->m_line < other.m_line;
        }

        if(this->m_column != other.m_column)
        {TRACE_IT(44021);
            return this->m_column < other.m_column;
        }

        //they are refering to the same location so this is *not* stricly before
        return false;
    }

    //////////////////

    namespace NSLogEvents
    {
        void PassVarToHostInReplay(ThreadContextTTD* executeContext, TTDVar origVar, Js::Var replayVar)
        {TRACE_IT(44022);
            static_assert(sizeof(TTDVar) == sizeof(Js::Var), "We assume the bit patterns on these types are the same!!!");

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            if(replayVar == nullptr || TTD::JsSupport::IsVarTaggedInline(replayVar))
            {TRACE_IT(44023);
                TTDAssert(TTD::JsSupport::AreInlineVarsEquiv(origVar, replayVar), "Should be same bit pattern.");
            }
#endif

            if(replayVar != nullptr && TTD::JsSupport::IsVarPtrValued(replayVar))
            {TRACE_IT(44024);
                Js::RecyclableObject* obj = Js::RecyclableObject::FromVar(replayVar);
                if(!ThreadContextTTD::IsSpecialRootObject(obj))
                {TRACE_IT(44025);
                    executeContext->AddLocalRoot(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(origVar), obj);
                }
            }
        }

        Js::Var InflateVarInReplay(ThreadContextTTD* executeContext, TTDVar origVar)
        {TRACE_IT(44026);
            static_assert(sizeof(TTDVar) == sizeof(Js::Var), "We assume the bit patterns on these types are the same!!!");

            if(origVar == nullptr || TTD::JsSupport::IsVarTaggedInline(origVar))
            {TRACE_IT(44027);
                return TTD_CONVERT_TTDVAR_TO_JSVAR(origVar);
            }
            else
            {TRACE_IT(44028);
                return executeContext->LookupObjectForLogID(TTD_CONVERT_OBJ_TO_LOG_PTR_ID(origVar));
            }
        }

        void EventLogEntry_Initialize(EventLogEntry* evt, EventKind tag, int64 etime)
        {TRACE_IT(44029);
            evt->EventKind = tag;
            evt->ResultStatus = -1;

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            evt->EventTimeStamp = etime;
#endif
        }

        void EventLogEntry_Emit(const EventLogEntry* evt, EventLogEntryVTableEntry* evtFPVTable, FileWriter* writer, ThreadContext* threadContext, NSTokens::Separator separator)
        {TRACE_IT(44030);
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
        {TRACE_IT(44031);
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
        {TRACE_IT(44032);
            return !(EventDoesNotReturn(evt) || EventCompletesNormally(evt) || EventCompletesWithException(evt));
        }

        bool EventDoesNotReturn(const EventLogEntry* evt)
        {TRACE_IT(44033);
            return evt->ResultStatus == -1;
        }

        bool EventCompletesNormally(const EventLogEntry* evt)
        {TRACE_IT(44034);
            return (evt->ResultStatus == 0) || (evt->ResultStatus == TTD_REPLAY_JsErrorInvalidArgument) || (evt->ResultStatus == TTD_REPLAY_JsErrorArgumentNotObject);
        }

        bool EventCompletesWithException(const EventLogEntry* evt)
        {TRACE_IT(44035);
            return (evt->ResultStatus == TTD_REPLAY_JsErrorCategoryScript) || (evt->ResultStatus == TTD_REPLAY_JsErrorScriptTerminated);
        }

        //////////////////

        void SnapshotEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44036);
            SnapshotEventLogEntry_UnloadSnapshot(evt);
        }

        void SnapshotEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44037);
            const SnapshotEventLogEntry* snapEvt = GetInlineEventDataAs<SnapshotEventLogEntry, EventKind::SnapshotTag>(evt);

            writer->WriteInt64(NSTokens::Key::restoreTime, snapEvt->RestoreTimestamp, NSTokens::Separator::CommaSeparator);

            if(snapEvt->Snap != nullptr)
            {TRACE_IT(44038);
                snapEvt->Snap->EmitSnapshot(snapEvt->RestoreTimestamp, threadContext);
            }
        }

        void SnapshotEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44039);
            SnapshotEventLogEntry* snapEvt = GetInlineEventDataAs<SnapshotEventLogEntry, EventKind::SnapshotTag>(evt);

            snapEvt->RestoreTimestamp = reader->ReadInt64(NSTokens::Key::restoreTime, true);
            snapEvt->Snap = nullptr;
        }

        void SnapshotEventLogEntry_EnsureSnapshotDeserialized(EventLogEntry* evt, ThreadContext* threadContext)
        {TRACE_IT(44040);
            SnapshotEventLogEntry* snapEvt = GetInlineEventDataAs<SnapshotEventLogEntry, EventKind::SnapshotTag>(evt);

            if(snapEvt->Snap == nullptr)
            {TRACE_IT(44041);
                snapEvt->Snap = SnapShot::Parse(snapEvt->RestoreTimestamp, threadContext);
            }
        }

        void SnapshotEventLogEntry_UnloadSnapshot(EventLogEntry* evt)
        {TRACE_IT(44042);
            SnapshotEventLogEntry* snapEvt = GetInlineEventDataAs<SnapshotEventLogEntry, EventKind::SnapshotTag>(evt);

            if(snapEvt->Snap != nullptr)
            {
                TT_HEAP_DELETE(SnapShot, snapEvt->Snap);
                snapEvt->Snap = nullptr;
            }
        }

        void EventLoopYieldPointEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44043);
            const EventLoopYieldPointEntry* ypEvt = GetInlineEventDataAs<EventLoopYieldPointEntry, EventKind::EventLoopYieldPointTag>(evt);

            writer->WriteUInt64(NSTokens::Key::eventTime, ypEvt->EventTimeStamp, NSTokens::Separator::CommaSeparator);
            writer->WriteDouble(NSTokens::Key::loopTime, ypEvt->EventWallTime, NSTokens::Separator::CommaSeparator);
        }

        void EventLoopYieldPointEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44044);
            EventLoopYieldPointEntry* ypEvt = GetInlineEventDataAs<EventLoopYieldPointEntry, EventKind::EventLoopYieldPointTag>(evt);

            ypEvt->EventTimeStamp = reader->ReadUInt64(NSTokens::Key::eventTime, true);
            ypEvt->EventWallTime = reader->ReadDouble(NSTokens::Key::loopTime, true);
        }

        //////////////////

        void CodeLoadEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44045);
            const CodeLoadEventLogEntry* codeEvt = GetInlineEventDataAs<CodeLoadEventLogEntry, EventKind::TopLevelCodeTag>(evt);

            writer->WriteUInt64(NSTokens::Key::u64Val, codeEvt->BodyCounterId, NSTokens::Separator::CommaSeparator);
        }

        void CodeLoadEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44046);
            CodeLoadEventLogEntry* codeEvt = GetInlineEventDataAs<CodeLoadEventLogEntry, EventKind::TopLevelCodeTag>(evt);

            codeEvt->BodyCounterId = reader->ReadUInt64(NSTokens::Key::u64Val, true);
        }

        void TelemetryEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44047);
            TelemetryEventLogEntry* telemetryEvt = GetInlineEventDataAs<TelemetryEventLogEntry, EventKind::TelemetryLogTag>(evt);

            alloc.UnlinkString(telemetryEvt->InfoString);
        }

        void TelemetryEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44048);
            const TelemetryEventLogEntry* telemetryEvt = GetInlineEventDataAs<TelemetryEventLogEntry, EventKind::TelemetryLogTag>(evt);

            writer->WriteString(NSTokens::Key::stringVal, telemetryEvt->InfoString, NSTokens::Separator::CommaSeparator);
            writer->WriteBool(NSTokens::Key::boolVal, telemetryEvt->DoPrint, NSTokens::Separator::CommaSeparator);
        }

        void TelemetryEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44049);
            TelemetryEventLogEntry* telemetryEvt = GetInlineEventDataAs<TelemetryEventLogEntry, EventKind::TelemetryLogTag>(evt);

            reader->ReadString(NSTokens::Key::stringVal, alloc, telemetryEvt->InfoString, true);
            telemetryEvt->DoPrint = reader->ReadBool(NSTokens::Key::boolVal, true);
        }

        //////////////////

        void RandomSeedEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44050);
            const RandomSeedEventLogEntry* rndEvt = GetInlineEventDataAs<RandomSeedEventLogEntry, EventKind::RandomSeedTag>(evt);

            writer->WriteUInt64(NSTokens::Key::u64Val, rndEvt->Seed0, NSTokens::Separator::CommaSeparator);
            writer->WriteUInt64(NSTokens::Key::u64Val, rndEvt->Seed1, NSTokens::Separator::CommaSeparator);

        }

        void RandomSeedEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44051);
            RandomSeedEventLogEntry* rndEvt = GetInlineEventDataAs<RandomSeedEventLogEntry, EventKind::RandomSeedTag>(evt);

            rndEvt->Seed0 = reader->ReadUInt64(NSTokens::Key::u64Val, true);
            rndEvt->Seed1 = reader->ReadUInt64(NSTokens::Key::u64Val, true);
        }

        void DoubleEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44052);
            const DoubleEventLogEntry* dblEvt = GetInlineEventDataAs<DoubleEventLogEntry, EventKind::DoubleTag>(evt);

            writer->WriteDouble(NSTokens::Key::doubleVal, dblEvt->DoubleValue, NSTokens::Separator::CommaSeparator);
        }

        void DoubleEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44053);
            DoubleEventLogEntry* dblEvt = GetInlineEventDataAs<DoubleEventLogEntry, EventKind::DoubleTag>(evt);

            dblEvt->DoubleValue = reader->ReadDouble(NSTokens::Key::doubleVal, true);
        }

        void StringValueEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44054);
            StringValueEventLogEntry* strEvt = GetInlineEventDataAs<StringValueEventLogEntry, EventKind::StringTag>(evt);

            alloc.UnlinkString(strEvt->StringValue);
        }

        void StringValueEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44055);
            const StringValueEventLogEntry* strEvt = GetInlineEventDataAs<StringValueEventLogEntry, EventKind::StringTag>(evt);

            writer->WriteString(NSTokens::Key::stringVal, strEvt->StringValue, NSTokens::Separator::CommaSeparator);
        }

        void StringValueEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44056);
            StringValueEventLogEntry* strEvt = GetInlineEventDataAs<StringValueEventLogEntry, EventKind::StringTag>(evt);

            reader->ReadString(NSTokens::Key::stringVal, alloc, strEvt->StringValue, true);
        }

        //////////////////

        void PropertyEnumStepEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44057);
            PropertyEnumStepEventLogEntry* propertyEvt = GetInlineEventDataAs<PropertyEnumStepEventLogEntry, EventKind::PropertyEnumTag>(evt);

            if(!IsNullPtrTTString(propertyEvt->PropertyString))
            {TRACE_IT(44058);
                alloc.UnlinkString(propertyEvt->PropertyString);
            }
        }

        void PropertyEnumStepEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44059);
            const PropertyEnumStepEventLogEntry* propertyEvt = GetInlineEventDataAs<PropertyEnumStepEventLogEntry, EventKind::PropertyEnumTag>(evt);

            writer->WriteBool(NSTokens::Key::boolVal, !!propertyEvt->ReturnCode, NSTokens::Separator::CommaSeparator);
            writer->WriteUInt32(NSTokens::Key::propertyId, propertyEvt->Pid, NSTokens::Separator::CommaSeparator);
            writer->WriteUInt32(NSTokens::Key::attributeFlags, propertyEvt->Attributes, NSTokens::Separator::CommaSeparator);

            if(propertyEvt->ReturnCode)
            {TRACE_IT(44060);
#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
                writer->WriteString(NSTokens::Key::stringVal, propertyEvt->PropertyString, NSTokens::Separator::CommaSeparator);
#else
                if(propertyEvt->Pid == Js::Constants::NoProperty)
                {TRACE_IT(44061);
                    writer->WriteString(NSTokens::Key::stringVal, propertyEvt->PropertyString, NSTokens::Separator::CommaSeparator);
                }
#endif
            }
        }

        void PropertyEnumStepEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44062);
            PropertyEnumStepEventLogEntry* propertyEvt = GetInlineEventDataAs<PropertyEnumStepEventLogEntry, EventKind::PropertyEnumTag>(evt);

            propertyEvt->ReturnCode = reader->ReadBool(NSTokens::Key::boolVal, true);
            propertyEvt->Pid = (Js::PropertyId)reader->ReadUInt32(NSTokens::Key::propertyId, true);
            propertyEvt->Attributes = (Js::PropertyAttributes)reader->ReadUInt32(NSTokens::Key::attributeFlags, true);

            InitializeAsNullPtrTTString(propertyEvt->PropertyString);

            if(propertyEvt->ReturnCode)
            {TRACE_IT(44063);
#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
                reader->ReadString(NSTokens::Key::stringVal, alloc, propertyEvt->PropertyString, true);
#else
                if(propertyEvt->Pid == Js::Constants::NoProperty)
                {TRACE_IT(44064);
                    reader->ReadString(NSTokens::Key::stringVal, alloc, propertyEvt->PropertyString, true);
                }
#endif
            }
        }

        //////////////////

        void SymbolCreationEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44065);
            const SymbolCreationEventLogEntry* symEvt = GetInlineEventDataAs<SymbolCreationEventLogEntry, EventKind::SymbolCreationTag>(evt);

            writer->WriteUInt32(NSTokens::Key::propertyId, symEvt->Pid, NSTokens::Separator::CommaSeparator);
        }

        void SymbolCreationEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44066);
            SymbolCreationEventLogEntry* symEvt = GetInlineEventDataAs<SymbolCreationEventLogEntry, EventKind::SymbolCreationTag>(evt);

            symEvt->Pid = (Js::PropertyId)reader->ReadUInt32(NSTokens::Key::propertyId, true);
        }

        //////////////////

        int64 ExternalCbRegisterCallEventLogEntry_GetLastNestedEventTime(const EventLogEntry* evt)
        {TRACE_IT(44067);
            const ExternalCbRegisterCallEventLogEntry* cbrEvt = GetInlineEventDataAs<ExternalCbRegisterCallEventLogEntry, EventKind::ExternalCbRegisterCall>(evt);

            return cbrEvt->LastNestedEventTime;
        }

        void ExternalCbRegisterCallEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44068);
            const ExternalCbRegisterCallEventLogEntry* cbrEvt = GetInlineEventDataAs<ExternalCbRegisterCallEventLogEntry, EventKind::ExternalCbRegisterCall>(evt);

            NSSnapValues::EmitTTDVar(cbrEvt->CallbackFunction, writer, NSTokens::Separator::CommaSeparator);
            writer->WriteInt64(NSTokens::Key::i64Val, cbrEvt->LastNestedEventTime, NSTokens::Separator::CommaSeparator);
        }

        void ExternalCbRegisterCallEventLogEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44069);
            ExternalCbRegisterCallEventLogEntry* cbrEvt = GetInlineEventDataAs<ExternalCbRegisterCallEventLogEntry, EventKind::ExternalCbRegisterCall>(evt);

            cbrEvt->CallbackFunction = NSSnapValues::ParseTTDVar(true, reader);
            cbrEvt->LastNestedEventTime = reader->ReadInt64(NSTokens::Key::i64Val, true);
        }

        //////////////////

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        void ExternalCallEventLogEntry_ProcessDiagInfoPre(EventLogEntry* evt, Js::JavascriptFunction* function, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44070);
            ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

            Js::JavascriptString* displayName = function->GetDisplayName();
            alloc.CopyStringIntoWLength(displayName->GetSz(), displayName->GetLength(), callEvt->AdditionalInfo->FunctionName);
        }
#endif

        int64 ExternalCallEventLogEntry_GetLastNestedEventTime(const EventLogEntry* evt)
        {TRACE_IT(44071);
            const ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

            return callEvt->AdditionalInfo->LastNestedEventTime;
        }

        void ExternalCallEventLogEntry_ProcessArgs(EventLogEntry* evt, int32 rootDepth, Js::JavascriptFunction* function, uint32 argc, Js::Var* argv, bool checkExceptions, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44072);
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
        {TRACE_IT(44073);
            ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

            callEvt->ReturnValue = TTD_CONVERT_JSVAR_TO_TTDVAR(res);
            callEvt->AdditionalInfo->LastNestedEventTime = lastNestedEvent;
        }

        void ExternalCallEventLogEntry_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44074);
            ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

            alloc.UnlinkAllocation(callEvt->ArgArray);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            alloc.UnlinkString(callEvt->AdditionalInfo->FunctionName);
#endif

            alloc.UnlinkAllocation(callEvt->AdditionalInfo);
        }

        void ExternalCallEventLogEntry_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {TRACE_IT(44075);
            const ExternalCallEventLogEntry* callEvt = GetInlineEventDataAs<ExternalCallEventLogEntry, EventKind::ExternalCallTag>(evt);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            writer->WriteString(NSTokens::Key::name, callEvt->AdditionalInfo->FunctionName, NSTokens::Separator::CommaSeparator);
#endif

            writer->WriteInt32(NSTokens::Key::rootNestingDepth, callEvt->RootNestingDepth, NSTokens::Separator::CommaSeparator);

            writer->WriteLengthValue(callEvt->ArgCount, NSTokens::Separator::CommaSeparator);
            writer->WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
            for(uint32 i = 0; i < callEvt->ArgCount; ++i)
            {TRACE_IT(44076);
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
        {TRACE_IT(44077);
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
            {TRACE_IT(44078);
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
        {TRACE_IT(44079);
            ; //We don't track any extra data with this
        }

        void ExplicitLogWriteEntry_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {TRACE_IT(44080);
            ; //We don't track any extra data with this
        }
    }
}

#endif
