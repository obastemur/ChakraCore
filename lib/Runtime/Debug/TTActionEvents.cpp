//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"

#if ENABLE_TTD

namespace TTD
{
    namespace NSLogEvents
    {
        bool IsJsRTActionRootCall(const EventLogEntry* evt)
        {LOGMEIN("TTActionEvents.cpp] 13\n");
            if(evt->EventKind != NSLogEvents::EventKind::CallExistingFunctionActionTag)
            {LOGMEIN("TTActionEvents.cpp] 15\n");
                return false;
            }

            const JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            return cfAction->CallbackDepth == 0;
        }

        int64 AccessTimeInRootCallOrSnapshot(const EventLogEntry* evt, bool& isSnap, bool& isRoot, bool& hasRtrSnap)
        {LOGMEIN("TTActionEvents.cpp] 24\n");
            isSnap = false;
            isRoot = false;
            hasRtrSnap = false;

            if(evt->EventKind == NSLogEvents::EventKind::SnapshotTag)
            {LOGMEIN("TTActionEvents.cpp] 30\n");
                const NSLogEvents::SnapshotEventLogEntry* snapEvent = NSLogEvents::GetInlineEventDataAs<NSLogEvents::SnapshotEventLogEntry, NSLogEvents::EventKind::SnapshotTag>(evt);

                isSnap = true;
                return snapEvent->RestoreTimestamp;
            }
            else if(NSLogEvents::IsJsRTActionRootCall(evt))
            {LOGMEIN("TTActionEvents.cpp] 37\n");
                const NSLogEvents::JsRTCallFunctionAction* rootEntry = NSLogEvents::GetInlineEventDataAs<NSLogEvents::JsRTCallFunctionAction, NSLogEvents::EventKind::CallExistingFunctionActionTag>(evt);

                isRoot = true;
                hasRtrSnap = (rootEntry->AdditionalInfo->AdditionalReplayInfo != nullptr && rootEntry->AdditionalInfo->AdditionalReplayInfo->RtRSnap != nullptr);
                return rootEntry->AdditionalInfo->CallEventTime;
            }
            else
            {
                return -1;
            }
        }

        bool TryGetTimeFromRootCallOrSnapshot(const EventLogEntry* evt, int64& res)
        {LOGMEIN("TTActionEvents.cpp] 51\n");
            bool isSnap = false;
            bool isRoot = false;
            bool hasRtrSnap = false;

            res = AccessTimeInRootCallOrSnapshot(evt, isSnap, isRoot, hasRtrSnap);
            return (isSnap | isRoot);
        }

        int64 GetTimeFromRootCallOrSnapshot(const EventLogEntry* evt)
        {LOGMEIN("TTActionEvents.cpp] 61\n");
            int64 res = -1;
            bool success = TryGetTimeFromRootCallOrSnapshot(evt, res);

            TTDAssert(success, "Not a root or snapshot!!!");
            return res;
        }

        void CreateScriptContext_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 70\n");
            const JsRTCreateScriptContextAction* cAction = GetInlineEventDataAs<JsRTCreateScriptContextAction, EventKind::CreateScriptContextActionTag>(evt);

            Js::ScriptContext* resCtx = nullptr;
            executeContext->TTDExternalObjectFunctions.pfCreateJsRTContextCallback(executeContext->GetRuntimeHandle(), &resCtx);
            TTDAssert(resCtx != nullptr, "Create failed");

            executeContext->AddTrackedRootSpecial(cAction->GlobalObject, resCtx->GetGlobalObject());
            resCtx->ScriptContextLogTag = cAction->GlobalObject;

            executeContext->AddTrackedRootSpecial(cAction->KnownObjects->UndefinedObject, resCtx->GetLibrary()->GetUndefined());
            executeContext->AddTrackedRootSpecial(cAction->KnownObjects->NullObject, resCtx->GetLibrary()->GetNull());
            executeContext->AddTrackedRootSpecial(cAction->KnownObjects->TrueObject, resCtx->GetLibrary()->GetTrue());
            executeContext->AddTrackedRootSpecial(cAction->KnownObjects->FalseObject, resCtx->GetLibrary()->GetFalse());
        }

        void CreateScriptContext_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 87\n");
            JsRTCreateScriptContextAction* cAction = GetInlineEventDataAs<JsRTCreateScriptContextAction, EventKind::CreateScriptContextActionTag>(evt);

            alloc.UnlinkAllocation(cAction->KnownObjects);
        }

        void CreateScriptContext_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTActionEvents.cpp] 94\n");
            const JsRTCreateScriptContextAction* cAction = GetInlineEventDataAs<JsRTCreateScriptContextAction, EventKind::CreateScriptContextActionTag>(evt);

            writer->WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, cAction->GlobalObject, NSTokens::Separator::NoSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, cAction->KnownObjects->UndefinedObject, NSTokens::Separator::CommaSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, cAction->KnownObjects->NullObject, NSTokens::Separator::CommaSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, cAction->KnownObjects->TrueObject, NSTokens::Separator::CommaSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, cAction->KnownObjects->FalseObject, NSTokens::Separator::CommaSeparator);
            writer->WriteSequenceEnd();
        }

        void CreateScriptContext_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 107\n");
            JsRTCreateScriptContextAction* cAction = GetInlineEventDataAs<JsRTCreateScriptContextAction, EventKind::CreateScriptContextActionTag>(evt);
            cAction->KnownObjects = alloc.SlabAllocateStruct<JsRTCreateScriptContextAction_KnownObjects>();

            reader->ReadSequenceStart_WDefaultKey(true);
            cAction->GlobalObject = reader->ReadLogTag(NSTokens::Key::logTag, false);
            cAction->KnownObjects->UndefinedObject = reader->ReadLogTag(NSTokens::Key::logTag, true);
            cAction->KnownObjects->NullObject = reader->ReadLogTag(NSTokens::Key::logTag, true);
            cAction->KnownObjects->TrueObject = reader->ReadLogTag(NSTokens::Key::logTag, true);
            cAction->KnownObjects->FalseObject = reader->ReadLogTag(NSTokens::Key::logTag, true);
            reader->ReadSequenceEnd();
        }

        void SetActiveScriptContext_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 121\n");
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::SetActiveScriptContextActionTag>(evt);
            Js::Var gvar = InflateVarInReplay(executeContext, action->Var1);
            TTDAssert(gvar == nullptr || Js::GlobalObject::Is(gvar), "Something is not right here!");

            Js::GlobalObject* gobj = static_cast<Js::GlobalObject*>(gvar);
            Js::ScriptContext* newCtx = (gobj != nullptr) ? gobj->GetScriptContext() : nullptr;

            executeContext->TTDExternalObjectFunctions.pfSetActiveJsRTContext(executeContext->GetRuntimeHandle(), newCtx);
        }

        void DeadScriptContext_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 133\n");
            const JsRTDestroyScriptContextAction* deadInfo = GetInlineEventDataAs<JsRTDestroyScriptContextAction, EventKind::DeadScriptContextActionTag>(evt);

            executeContext->NotifyCtxDestroyedInReplay(deadInfo->GlobalLogTag, deadInfo->KnownObjects->UndefinedLogTag, deadInfo->KnownObjects->NullLogTag, deadInfo->KnownObjects->TrueLogTag, deadInfo->KnownObjects->FalseLogTag);
        }

        void DeadScriptContext_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 140\n");
            JsRTDestroyScriptContextAction* dAction = GetInlineEventDataAs<JsRTDestroyScriptContextAction, EventKind::DeadScriptContextActionTag>(evt);

            alloc.UnlinkAllocation(dAction->KnownObjects);
        }

        void DeadScriptContext_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTActionEvents.cpp] 147\n");
            const JsRTDestroyScriptContextAction* dAction = GetInlineEventDataAs<JsRTDestroyScriptContextAction, EventKind::DeadScriptContextActionTag>(evt);

            writer->WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, dAction->GlobalLogTag, NSTokens::Separator::NoSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, dAction->KnownObjects->UndefinedLogTag, NSTokens::Separator::CommaSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, dAction->KnownObjects->NullLogTag, NSTokens::Separator::CommaSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, dAction->KnownObjects->TrueLogTag, NSTokens::Separator::CommaSeparator);
            writer->WriteLogTag(NSTokens::Key::logTag, dAction->KnownObjects->FalseLogTag, NSTokens::Separator::CommaSeparator);
            writer->WriteSequenceEnd();
        }

        void DeadScriptContext_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 160\n");
            JsRTDestroyScriptContextAction* dAction = GetInlineEventDataAs<JsRTDestroyScriptContextAction, EventKind::DeadScriptContextActionTag>(evt);
            dAction->KnownObjects = alloc.SlabAllocateStruct<JsRTDestroyScriptContextAction_KnownObjects>();

            reader->ReadSequenceStart_WDefaultKey(true);
            dAction->GlobalLogTag = reader->ReadLogTag(NSTokens::Key::logTag, false);
            dAction->KnownObjects->UndefinedLogTag = reader->ReadLogTag(NSTokens::Key::logTag, true);
            dAction->KnownObjects->NullLogTag = reader->ReadLogTag(NSTokens::Key::logTag, true);
            dAction->KnownObjects->TrueLogTag = reader->ReadLogTag(NSTokens::Key::logTag, true);
            dAction->KnownObjects->FalseLogTag = reader->ReadLogTag(NSTokens::Key::logTag, true);
            reader->ReadSequenceEnd();
        }

#if !INT32VAR
        void CreateInt_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 175\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::CreateIntegerActionTag>(evt);

            Js::Var res = Js::JavascriptNumber::ToVar((int32)action->u_iVal, ctx);

            JsRTActionHandleResultForReplay<JsRTVarsWithIntegralUnionArgumentAction, EventKind::CreateIntegerActionTag>(executeContext, evt, res);
        }
#endif

        void CreateNumber_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 186\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTDoubleArgumentAction* action = GetInlineEventDataAs<JsRTDoubleArgumentAction, EventKind::CreateNumberActionTag>(evt);

            Js::Var res = Js::JavascriptNumber::ToVarNoCheck(action->DoubleValue, ctx);

            JsRTActionHandleResultForReplay<JsRTDoubleArgumentAction, EventKind::CreateNumberActionTag>(executeContext, evt, res);
        }

        void CreateBoolean_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 196\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::CreateBooleanActionTag>(evt);

            Js::Var res = action->u_bVal ? ctx->GetLibrary()->GetTrue() : ctx->GetLibrary()->GetFalse();

            JsRTActionHandleResultForReplay<JsRTVarsWithIntegralUnionArgumentAction, EventKind::CreateBooleanActionTag>(executeContext, evt, res);
        }

        void CreateString_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 206\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTStringArgumentAction* action = GetInlineEventDataAs<JsRTStringArgumentAction, EventKind::CreateStringActionTag>(evt);

            Js::Var res = Js::JavascriptString::NewCopyBuffer(action->StringValue.Contents, action->StringValue.Length, ctx);

            JsRTActionHandleResultForReplay<JsRTStringArgumentAction, EventKind::CreateStringActionTag>(executeContext, evt, res);
        }

        void CreateSymbol_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 216\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::CreateSymbolActionTag>(evt);
            Js::Var description = InflateVarInReplay(executeContext, action->Var1);

            Js::JavascriptString* descriptionString;
            if(description != nullptr)
            {
                TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(description, ctx);
                descriptionString = Js::JavascriptConversion::ToString(description, ctx);
            }
            else
            {
                descriptionString = ctx->GetLibrary()->GetEmptyString();
            }
            Js::Var res = ctx->GetLibrary()->CreateSymbol(descriptionString);

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::CreateSymbolActionTag>(executeContext, evt, res);
        }

        void Execute_CreateErrorHelper(const JsRTVarsArgumentAction* errorData, ThreadContextTTD* executeContext, Js::ScriptContext* ctx, EventKind eventKind, Js::Var* res)
        {LOGMEIN("TTActionEvents.cpp] 237\n");
            Js::Var message = InflateVarInReplay(executeContext, errorData->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(message, ctx);

            *res = nullptr; 
            switch(eventKind)
            {LOGMEIN("TTActionEvents.cpp] 243\n");
            case EventKind::CreateErrorActionTag:
                *res = ctx->GetLibrary()->CreateError();
                break;
            case EventKind::CreateRangeErrorActionTag:
                *res = ctx->GetLibrary()->CreateRangeError();
                break;
            case EventKind::CreateReferenceErrorActionTag:
                *res = ctx->GetLibrary()->CreateReferenceError();
                break;
            case EventKind::CreateSyntaxErrorActionTag:
                *res = ctx->GetLibrary()->CreateSyntaxError();
                break;
            case EventKind::CreateTypeErrorActionTag:
                *res = ctx->GetLibrary()->CreateTypeError();
                break;
            case EventKind::CreateURIErrorActionTag:
                *res = ctx->GetLibrary()->CreateURIError();
                break;
            default:
                TTDAssert(false, "Missing error kind!!!");
            }

            Js::JavascriptOperators::OP_SetProperty(*res, Js::PropertyIds::message, message, ctx);
        }

        void VarConvertToNumber_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 270\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::VarConvertToNumberActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(var, ctx);

            Js::Var res = Js::JavascriptOperators::ToNumber(var, ctx);

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::VarConvertToNumberActionTag>(executeContext, evt, res);
        }

        void VarConvertToBoolean_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 282\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::VarConvertToBooleanActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(var, ctx);

            Js::JavascriptConversion::ToBool(var, ctx) ? ctx->GetLibrary()->GetTrue() : ctx->GetLibrary()->GetFalse();

            //It is either true or false which we always track so no need to do result mapping
        }

        void VarConvertToString_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 294\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::VarConvertToStringActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(var, ctx);

            Js::Var res = Js::JavascriptConversion::ToString(var, ctx);

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::VarConvertToStringActionTag>(executeContext, evt, res);
        }

        void VarConvertToObject_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 306\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::VarConvertToObjectActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(var, ctx);

            Js::Var res = Js::JavascriptOperators::ToObject(var, ctx);
            Assert(res == nullptr || !Js::CrossSite::NeedMarshalVar(res, ctx));

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::VarConvertToObjectActionTag>(executeContext, evt, res);
        }

        void AddRootRef_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 319\n");
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::AddRootRefActionTag>(evt);

            TTD_LOG_PTR_ID origId = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(TTD_CONVERT_TTDVAR_TO_JSVAR(action->Var1));

            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            Js::RecyclableObject* newObj = Js::RecyclableObject::FromVar(var);

            executeContext->AddTrackedRootGeneral(origId, newObj);
        }

        void RemoveRootRef_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 331\n");
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::RemoveRootRefActionTag>(evt);

            TTD_LOG_PTR_ID origId = TTD_CONVERT_OBJ_TO_LOG_PTR_ID(TTD_CONVERT_TTDVAR_TO_JSVAR(action->Var1));

            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            Js::RecyclableObject* deleteObj = Js::RecyclableObject::FromVar(var);

            executeContext->RemoveTrackedRootGeneral(origId, deleteObj);
        }

        void AllocateObject_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 343\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            Js::RecyclableObject* res = ctx->GetLibrary()->CreateObject();

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::AllocateObjectActionTag>(executeContext, evt, res);
        }

        void AllocateExternalObject_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 351\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);

            Js::Var res = nullptr;
            executeContext->TTDExternalObjectFunctions.pfCreateExternalObject(ctx, &res);

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::AllocateExternalObjectActionTag>(executeContext, evt, res);
        }

        void AllocateArrayAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 361\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::AllocateArrayActionTag>(evt);

            Js::Var res = ctx->GetLibrary()->CreateArray((uint32)action->u_iVal);

            JsRTActionHandleResultForReplay<JsRTVarsWithIntegralUnionArgumentAction, EventKind::AllocateArrayActionTag>(executeContext, evt, res);
        }

        void AllocateArrayBufferAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 371\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::AllocateArrayBufferActionTag>(evt);

            Js::ArrayBuffer* abuff = ctx->GetLibrary()->CreateArrayBuffer((uint32)action->u_iVal);
            TTDAssert(abuff->GetByteLength() == (uint32)action->u_iVal, "Something is wrong with our sizes.");

            JsRTActionHandleResultForReplay<JsRTVarsWithIntegralUnionArgumentAction, EventKind::AllocateArrayBufferActionTag>(executeContext, evt, (Js::Var)abuff);
        }

        void AllocateExternalArrayBufferAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 382\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTByteBufferAction* action = GetInlineEventDataAs<JsRTByteBufferAction, EventKind::AllocateExternalArrayBufferActionTag>(evt);

            Js::ArrayBuffer* abuff = ctx->GetLibrary()->CreateArrayBuffer(action->Length);
            TTDAssert(abuff->GetByteLength() == action->Length, "Something is wrong with our sizes.");

            if(action->Length != 0)
            {LOGMEIN("TTActionEvents.cpp] 390\n");
                js_memcpy_s(abuff->GetBuffer(), abuff->GetByteLength(), action->Buffer, action->Length);
            }

            JsRTActionHandleResultForReplay<JsRTByteBufferAction, EventKind::AllocateExternalArrayBufferActionTag>(executeContext, evt, (Js::Var)abuff);
        }

        void AllocateFunctionAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 398\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::AllocateFunctionActionTag>(evt);

            Js::Var res = nullptr;
            if(!action->u_bVal)
            {LOGMEIN("TTActionEvents.cpp] 404\n");
                res = ctx->GetLibrary()->CreateStdCallExternalFunction(&Js::JavascriptExternalFunction::TTDReplayDummyExternalMethod, 0, nullptr);
            }
            else
            {
                Js::Var nameVar = InflateVarInReplay(executeContext, action->Var1);
                TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(nameVar, ctx);

                Js::JavascriptString* name = nullptr;
                if(nameVar != nullptr)
                {LOGMEIN("TTActionEvents.cpp] 414\n");
                    name = Js::JavascriptConversion::ToString(nameVar, ctx);
                }
                else
                {
                    name = ctx->GetLibrary()->GetEmptyString();
                }

                res = ctx->GetLibrary()->CreateStdCallExternalFunction(&Js::JavascriptExternalFunction::TTDReplayDummyExternalMethod, name, nullptr);
            }

            JsRTActionHandleResultForReplay<JsRTVarsWithIntegralUnionArgumentAction, EventKind::AllocateFunctionActionTag>(executeContext, evt, res);
        }

        void HostProcessExitAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 429\n");
            throw TTDebuggerAbortException::CreateAbortEndOfLog(_u("End of log reached with Host Process Exit -- returning to top-level."));
        }

        void GetAndClearExceptionAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 434\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);

            HRESULT hr = S_OK;
            Js::JavascriptExceptionObject *recordedException = nullptr;

            BEGIN_TRANSLATE_OOM_TO_HRESULT
              recordedException = ctx->GetAndClearRecordedException();
            END_TRANSLATE_OOM_TO_HRESULT(hr)

            Js::Var exception = nullptr;
            if(recordedException != nullptr)
            {LOGMEIN("TTActionEvents.cpp] 446\n");
                exception = recordedException->GetThrownObject(nullptr);
            }

            if(exception != nullptr)
            {
                JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::GetAndClearExceptionActionTag>(executeContext, evt, exception);
            }
        }

        void SetExceptionAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 457\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::SetExceptionActionTag>(evt);
            Js::Var exception = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(exception, ctx);

            bool propagateToDebugger = action->u_bVal ? true : false;

            Js::JavascriptExceptionObject *exceptionObject;
            exceptionObject = RecyclerNew(ctx->GetRecycler(), Js::JavascriptExceptionObject, exception, ctx, nullptr);

            ctx->RecordException(exceptionObject, propagateToDebugger);
        }

        void HasPropertyAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 472\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::HasPropertyActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);

            //Result is not needed but trigger computation for any effects
            Js::JavascriptOperators::OP_HasProperty(var, action->u_pid, ctx);
        }

        void InstanceOfAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 483\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::InstanceOfActionTag>(evt);
            Js::Var object = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(object, ctx);
            Js::Var constructor = InflateVarInReplay(executeContext, action->Var2);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(constructor, ctx);

            //Result is not needed but trigger computation for any effects
            Js::RecyclableObject::FromVar(constructor)->HasInstance(object, ctx);
        }

        void EqualsAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 496\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::EqualsActionTag>(evt);
            Js::Var object1 = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(object1, ctx);
            Js::Var object2 = InflateVarInReplay(executeContext, action->Var2);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(object2, ctx);

            //Result is not needed but trigger computation for any effects
            if(action->u_bVal)
            {LOGMEIN("TTActionEvents.cpp] 506\n");
                Js::JavascriptOperators::StrictEqual(object1, object2, ctx);
            }
            else
            {
                Js::JavascriptOperators::Equal(object1, object2, ctx);
            }
        }

        void GetPropertyIdFromSymbolAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 516\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::GetPropertyIdFromSymbolTag>(evt);
            Js::Var sym = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(sym, ctx);

            //These really don't have any effect, we need the marshal in validate, so just skip since Js::JavascriptSymbol has strange declaration order
            //
            //if(!Js::JavascriptSymbol::Is(sym))
            //{
            //    return JsErrorPropertyNotSymbol;
            //}
            //
            //Js::JavascriptSymbol::FromVar(symbol)->GetValue();
        }

        void GetPrototypeAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 533\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::GetPrototypeActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);

            Js::Var res = Js::JavascriptOperators::OP_GetPrototype(var,ctx);
            Assert(res == nullptr || !Js::CrossSite::NeedMarshalVar(res, ctx));

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::GetPrototypeActionTag>(executeContext, evt, res);
        }

        void GetPropertyAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 546\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::GetPropertyActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);

            Js::Var res = Js::JavascriptOperators::OP_GetProperty(var, action->u_pid, ctx);
            Assert(res == nullptr || !Js::CrossSite::NeedMarshalVar(res, ctx));

            JsRTActionHandleResultForReplay<JsRTVarsWithIntegralUnionArgumentAction, EventKind::GetPropertyActionTag>(executeContext, evt, res);
        }

        void GetIndexAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 559\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::GetIndexActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);
            Js::Var index = InflateVarInReplay(executeContext, action->Var2);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(index, ctx);

            Js::Var res = Js::JavascriptOperators::OP_GetElementI(var, index, ctx);

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::GetIndexActionTag>(executeContext, evt, res);
        }

        void GetOwnPropertyInfoAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 573\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::GetOwnPropertyInfoActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);

            Js::Var res = nullptr;
            Js::PropertyDescriptor propertyDescriptorValue;
            if(Js::JavascriptOperators::GetOwnPropertyDescriptor(Js::RecyclableObject::FromVar(var), action->u_pid, ctx, &propertyDescriptorValue))
            {LOGMEIN("TTActionEvents.cpp] 582\n");
                res = Js::JavascriptOperators::FromPropertyDescriptor(propertyDescriptorValue, ctx);
            }
            else
            {
                res = ctx->GetLibrary()->GetUndefined();
            }
            Assert(res == nullptr || !Js::CrossSite::NeedMarshalVar(res, ctx));

            JsRTActionHandleResultForReplay<JsRTVarsWithIntegralUnionArgumentAction, EventKind::GetOwnPropertyInfoActionTag>(executeContext, evt, res);
        }

        void GetOwnPropertyNamesInfoAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 595\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::GetOwnPropertyNamesInfoActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);

            Js::JavascriptArray* res = Js::JavascriptOperators::GetOwnPropertyNames(var, ctx);
            Assert(res == nullptr || !Js::CrossSite::NeedMarshalVar(res, ctx));

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::GetOwnPropertyNamesInfoActionTag>(executeContext, evt, res);
        }

        void GetOwnPropertySymbolsInfoAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 608\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::GetOwnPropertySymbolsInfoActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);

            Js::JavascriptArray* res = Js::JavascriptOperators::GetOwnPropertySymbols(var, ctx);
            Assert(res == nullptr || !Js::CrossSite::NeedMarshalVar(res, ctx));

            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::GetOwnPropertySymbolsInfoActionTag>(executeContext, evt, res);
        }

        void DefinePropertyAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 621\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithIntegralUnionArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithIntegralUnionArgumentAction, EventKind::DefinePropertyActionTag>(evt);
            Js::Var object = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(object, ctx);
            Js::Var propertyDescriptor = InflateVarInReplay(executeContext, action->Var2);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(propertyDescriptor, ctx);

            Js::PropertyDescriptor propertyDescriptorValue;
            Js::JavascriptOperators::ToPropertyDescriptor(propertyDescriptor, &propertyDescriptorValue, ctx);

            Js::JavascriptOperators::DefineOwnPropertyDescriptor(Js::RecyclableObject::FromVar(object), action->u_pid, propertyDescriptorValue, true, ctx);
        }

        void DeletePropertyAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 636\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithBoolAndPIDArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithBoolAndPIDArgumentAction, EventKind::DeletePropertyActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);

            Js::Var res = Js::JavascriptOperators::OP_DeleteProperty(var, action->Pid, ctx, action->BoolVal ? Js::PropertyOperation_StrictMode : Js::PropertyOperation_None);
            Assert(res == nullptr || !Js::CrossSite::NeedMarshalVar(res, ctx));

            JsRTActionHandleResultForReplay<JsRTVarsWithBoolAndPIDArgumentAction, EventKind::DeletePropertyActionTag>(executeContext, evt, res);
        }

        void SetPrototypeAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 649\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::SetPrototypeActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);
            Js::Var proto = InflateVarInReplay(executeContext, action->Var2);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT_OR_NULL(proto, ctx);

            Js::JavascriptObject::ChangePrototype(Js::RecyclableObject::FromVar(var), Js::RecyclableObject::FromVar(proto), true, ctx);
        }

        void SetPropertyAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 661\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsWithBoolAndPIDArgumentAction* action = GetInlineEventDataAs<JsRTVarsWithBoolAndPIDArgumentAction, EventKind::SetPropertyActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);
            Js::Var value = InflateVarInReplay(executeContext, action->Var2);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(value, ctx);

            Js::JavascriptOperators::OP_SetProperty(var, action->Pid, value, ctx, nullptr, action->BoolVal ? Js::PropertyOperation_StrictMode : Js::PropertyOperation_None);
        }

        void SetIndexAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 673\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::SetIndexActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);
            TTD_REPLAY_VALIDATE_INCOMING_OBJECT(var, ctx);
            Js::Var index = InflateVarInReplay(executeContext, action->Var2);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(index, ctx);
            Js::Var value = InflateVarInReplay(executeContext, action->Var3);
            TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(value, ctx);

            Js::JavascriptOperators::OP_SetElementI(var, index, value, ctx);
        }

        void GetTypedArrayInfoAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 687\n");
            const JsRTVarsArgumentAction* action = GetInlineEventDataAs<JsRTVarsArgumentAction, EventKind::GetTypedArrayInfoActionTag>(evt);
            Js::Var var = InflateVarInReplay(executeContext, action->Var1);

            Js::TypedArrayBase* typedArrayBase = Js::TypedArrayBase::FromVar(var);
            Js::Var res = typedArrayBase->GetArrayBuffer();

            //Need additional notify since JsRTActionHandleResultForReplay may allocate but GetTypedArrayInfo does not enter runtime
            //Failure will kick all the way out to replay loop -- which is what we want
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);
            JsRTActionHandleResultForReplay<JsRTVarsArgumentAction, EventKind::GetTypedArrayInfoActionTag>(executeContext, evt, res);
        }

        //////////////////

        void JsRTRawBufferCopyAction_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTActionEvents.cpp] 703\n");
            const JsRTRawBufferCopyAction* rbcAction = GetInlineEventDataAs<JsRTRawBufferCopyAction, EventKind::RawBufferCopySync>(evt);

            writer->WriteKey(NSTokens::Key::argRetVal, NSTokens::Separator::CommaSeparator);
            NSSnapValues::EmitTTDVar(rbcAction->Dst, writer, NSTokens::Separator::NoSeparator);

            writer->WriteKey(NSTokens::Key::argRetVal, NSTokens::Separator::CommaSeparator);
            NSSnapValues::EmitTTDVar(rbcAction->Src, writer, NSTokens::Separator::NoSeparator);

            writer->WriteUInt32(NSTokens::Key::u32Val, rbcAction->DstIndx, NSTokens::Separator::CommaSeparator);
            writer->WriteUInt32(NSTokens::Key::u32Val, rbcAction->SrcIndx, NSTokens::Separator::CommaSeparator);
            writer->WriteUInt32(NSTokens::Key::u32Val, rbcAction->Count, NSTokens::Separator::CommaSeparator);
        }

        void JsRTRawBufferCopyAction_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 718\n");
            JsRTRawBufferCopyAction* rbcAction = GetInlineEventDataAs<JsRTRawBufferCopyAction, EventKind::RawBufferCopySync>(evt);

            reader->ReadKey(NSTokens::Key::argRetVal, true);
            rbcAction->Dst = NSSnapValues::ParseTTDVar(false, reader);

            reader->ReadKey(NSTokens::Key::argRetVal, true);
            rbcAction->Src = NSSnapValues::ParseTTDVar(false, reader);

            rbcAction->DstIndx = reader->ReadUInt32(NSTokens::Key::u32Val, true);
            rbcAction->SrcIndx = reader->ReadUInt32(NSTokens::Key::u32Val, true);
            rbcAction->Count = reader->ReadUInt32(NSTokens::Key::u32Val, true);
        }

        void RawBufferCopySync_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 733\n");
            const JsRTRawBufferCopyAction* action = GetInlineEventDataAs<JsRTRawBufferCopyAction, EventKind::RawBufferCopySync>(evt);
            Js::Var dst = InflateVarInReplay(executeContext, action->Dst); //never cross context
            Js::Var src = InflateVarInReplay(executeContext, action->Src); //never cross context

            TTDAssert(Js::ArrayBuffer::Is(dst) && Js::ArrayBuffer::Is(src), "Not array buffer objects!!!");
            TTDAssert(action->DstIndx + action->Count <= Js::ArrayBuffer::FromVar(dst)->GetByteLength(), "Copy off end of buffer!!!");
            TTDAssert(action->SrcIndx + action->Count <= Js::ArrayBuffer::FromVar(src)->GetByteLength(), "Copy off end of buffer!!!");

            byte* dstBuff = Js::ArrayBuffer::FromVar(dst)->GetBuffer() + action->DstIndx;
            byte* srcBuff = Js::ArrayBuffer::FromVar(src)->GetBuffer() + action->SrcIndx;

            //node uses mmove so we do too
            memmove(dstBuff, srcBuff, action->Count);
        }

        void RawBufferModifySync_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 750\n");
            const JsRTRawBufferModifyAction* action = GetInlineEventDataAs<JsRTRawBufferModifyAction, EventKind::RawBufferModifySync>(evt);
            Js::Var trgt = InflateVarInReplay(executeContext, action->Trgt); //never cross context

            TTDAssert(Js::ArrayBuffer::Is(trgt), "Not array buffer object!!!");
            TTDAssert(action->Index + action->Length <= Js::ArrayBuffer::FromVar(trgt)->GetByteLength(), "Copy off end of buffer!!!");

            byte* trgtBuff = Js::ArrayBuffer::FromVar(trgt)->GetBuffer() + action->Index;
            js_memcpy_s(trgtBuff, action->Length, action->Data, action->Length);
        }

        void RawBufferAsyncModificationRegister_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 762\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTRawBufferModifyAction* action = GetInlineEventDataAs<JsRTRawBufferModifyAction, EventKind::RawBufferAsyncModificationRegister>(evt);
            Js::Var trgt = InflateVarInReplay(executeContext, action->Trgt); //never cross context

            ctx->TTDContextInfo->AddToAsyncPendingList(Js::ArrayBuffer::FromVar(trgt), action->Index);
        }

        void RawBufferAsyncModifyComplete_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 771\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTRawBufferModifyAction* action = GetInlineEventDataAs<JsRTRawBufferModifyAction, EventKind::RawBufferAsyncModifyComplete>(evt);
            Js::Var trgt = InflateVarInReplay(executeContext, action->Trgt); //never cross context

            const Js::ArrayBuffer* dstBuff = Js::ArrayBuffer::FromVar(trgt);
            byte* copyBuff = dstBuff->GetBuffer() + action->Index;
            byte* finalModPos = dstBuff->GetBuffer() + action->Index + action->Length;

            TTDPendingAsyncBufferModification pendingAsyncInfo = { 0 };
            ctx->TTDContextInfo->GetFromAsyncPendingList(&pendingAsyncInfo, finalModPos);
            TTDAssert(dstBuff == pendingAsyncInfo.ArrayBufferVar && action->Index == pendingAsyncInfo.Index, "Something is not right.");

            js_memcpy_s(copyBuff, action->Length, action->Data, action->Length);
        }

        //////////////////

        void JsRTConstructCallAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 790\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTConstructCallAction* ccAction = GetInlineEventDataAs<JsRTConstructCallAction, EventKind::ConstructCallActionTag>(evt);

            Js::Var jsFunctionVar = InflateVarInReplay(executeContext, ccAction->ArgArray[0]);
            TTD_REPLAY_VALIDATE_INCOMING_FUNCTION(jsFunctionVar, ctx);

            //remove implicit constructor function as first arg in callInfo and argument loop below
            for(uint32 i = 1; i < ccAction->ArgCount; ++i)
            {LOGMEIN("TTActionEvents.cpp] 799\n");
                 Js::Var argi = InflateVarInReplay(executeContext, ccAction->ArgArray[i]);
                 TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(argi, ctx);

                 ccAction->ExecArgs[i - 1] = argi;
            }

            Js::JavascriptFunction* jsFunction = Js::JavascriptFunction::FromVar(jsFunctionVar);
            Js::CallInfo callInfo(Js::CallFlags::CallFlags_New, (ushort)(ccAction->ArgCount - 1));
            Js::Arguments jsArgs(callInfo, ccAction->ExecArgs);

            //
            //TODO: we will want to look at this at some point -- either treat as "top-level" call or maybe constructors are fast so we can just jump back to previous "real" code
            //TTDAssert(!Js::ScriptFunction::Is(jsFunction) || execContext->GetThreadContext()->TTDRootNestingCount != 0, "This will cause user code to execute and we need to add support for that as a top-level call source!!!!");
            //

            Js::Var res = Js::JavascriptFunction::CallAsConstructor(jsFunction, /* overridingNewTarget = */nullptr, jsArgs, ctx);
            Assert(res == nullptr || !Js::CrossSite::NeedMarshalVar(res, ctx));

            JsRTActionHandleResultForReplay<JsRTConstructCallAction, EventKind::ConstructCallActionTag>(executeContext, evt, res);
        }

        void JsRTConstructCallAction_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 822\n");
            JsRTConstructCallAction* ccAction = GetInlineEventDataAs<JsRTConstructCallAction, EventKind::ConstructCallActionTag>(evt);

            if(ccAction->ArgArray != nullptr)
            {LOGMEIN("TTActionEvents.cpp] 826\n");
                alloc.UnlinkAllocation(ccAction->ArgArray);
            }

            if(ccAction->ExecArgs != nullptr)
            {LOGMEIN("TTActionEvents.cpp] 831\n");
                alloc.UnlinkAllocation(ccAction->ExecArgs);
            }
        }

        void JsRTConstructCallAction_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTActionEvents.cpp] 837\n");
            const JsRTConstructCallAction* ccAction = GetInlineEventDataAs<JsRTConstructCallAction, EventKind::ConstructCallActionTag>(evt);

            writer->WriteKey(NSTokens::Key::argRetVal, NSTokens::Separator::CommaSeparator);
            NSSnapValues::EmitTTDVar(ccAction->Result, writer, NSTokens::Separator::NoSeparator);

            writer->WriteLengthValue(ccAction->ArgCount, NSTokens::Separator::CommaSeparator);
            writer->WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
            for(uint32 i = 0; i < ccAction->ArgCount; ++i)
            {LOGMEIN("TTActionEvents.cpp] 846\n");
                NSTokens::Separator sep = (i != 0) ? NSTokens::Separator::CommaSeparator : NSTokens::Separator::NoSeparator;
                NSSnapValues::EmitTTDVar(ccAction->ArgArray[i], writer, sep);
            }
            writer->WriteSequenceEnd();
        }

        void JsRTConstructCallAction_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 854\n");
            JsRTConstructCallAction* ccAction = GetInlineEventDataAs<JsRTConstructCallAction, EventKind::ConstructCallActionTag>(evt);

            reader->ReadKey(NSTokens::Key::argRetVal, true);
            ccAction->Result = NSSnapValues::ParseTTDVar(false, reader);

            ccAction->ArgCount = reader->ReadLengthValue(true);
            ccAction->ArgArray = alloc.SlabAllocateArray<TTDVar>(ccAction->ArgCount);

            reader->ReadSequenceStart_WDefaultKey(true);
            for(uint32 i = 0; i < ccAction->ArgCount; ++i)
            {LOGMEIN("TTActionEvents.cpp] 865\n");
                ccAction->ArgArray[i] = NSSnapValues::ParseTTDVar(i != 0, reader);
            }
            reader->ReadSequenceEnd();

            ccAction->ExecArgs = (ccAction->ArgCount > 1) ? alloc.SlabAllocateArray<Js::Var>(ccAction->ArgCount - 1) : nullptr; //ArgCount includes slot for function which we don't use in exec
        }

        void JsRTCallbackAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 874\n");
            if(executeContext->GetActiveScriptContext()->ShouldPerformDebuggerAction())
            {LOGMEIN("TTActionEvents.cpp] 876\n");
                const JsRTCallbackAction* cbAction = GetInlineEventDataAs<JsRTCallbackAction, EventKind::CallbackOpActionTag>(evt);

                if(cbAction->RegisterLocation == nullptr)
                {LOGMEIN("TTActionEvents.cpp] 880\n");
                    const_cast<JsRTCallbackAction*>(cbAction)->RegisterLocation = TT_HEAP_NEW(TTDebuggerSourceLocation);
                }

                if(!cbAction->RegisterLocation->HasValue())
                {LOGMEIN("TTActionEvents.cpp] 885\n");
                    executeContext->GetThreadContext()->TTDLog->GetTimeAndPositionForDebugger(*(cbAction->RegisterLocation));
                }
            }
        }

        void JsRTCallbackAction_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 892\n");
            JsRTCallbackAction* cbAction = GetInlineEventDataAs<JsRTCallbackAction, EventKind::CallbackOpActionTag>(evt);

            if(cbAction->RegisterLocation != nullptr)
            {LOGMEIN("TTActionEvents.cpp] 896\n");
                cbAction->RegisterLocation->Clear();

                TT_HEAP_DELETE(TTDebuggerSourceLocation, cbAction->RegisterLocation);
                cbAction->RegisterLocation = nullptr;
            }
        }

        void JsRTCallbackAction_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTActionEvents.cpp] 905\n");
            const JsRTCallbackAction* cbAction = GetInlineEventDataAs<JsRTCallbackAction, EventKind::CallbackOpActionTag>(evt);

            writer->WriteBool(NSTokens::Key::boolVal, cbAction->IsCreate, NSTokens::Separator::CommaSeparator);
            writer->WriteBool(NSTokens::Key::boolVal, cbAction->IsCancel, NSTokens::Separator::CommaSeparator);
            writer->WriteBool(NSTokens::Key::boolVal, cbAction->IsRepeating, NSTokens::Separator::CommaSeparator);

            writer->WriteInt64(NSTokens::Key::hostCallbackId, cbAction->CurrentCallbackId, NSTokens::Separator::CommaSeparator);
            writer->WriteInt64(NSTokens::Key::newCallbackId, cbAction->NewCallbackId, NSTokens::Separator::CommaSeparator);
        }

        void JsRTCallbackAction_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 917\n");
            JsRTCallbackAction* cbAction = GetInlineEventDataAs<JsRTCallbackAction, EventKind::CallbackOpActionTag>(evt);

            cbAction->IsCreate = reader->ReadBool(NSTokens::Key::boolVal, true);
            cbAction->IsCancel = reader->ReadBool(NSTokens::Key::boolVal, true);
            cbAction->IsRepeating = reader->ReadBool(NSTokens::Key::boolVal, true);

            cbAction->CurrentCallbackId = reader->ReadInt64(NSTokens::Key::hostCallbackId, true);
            cbAction->NewCallbackId = reader->ReadInt64(NSTokens::Key::newCallbackId, true);

            cbAction->RegisterLocation = nullptr;
        }

        bool JsRTCallbackAction_GetActionTimeInfoForDebugger(const EventLogEntry* evt, TTDebuggerSourceLocation& sourceLocation)
        {LOGMEIN("TTActionEvents.cpp] 931\n");
            const JsRTCallbackAction* cbAction = GetInlineEventDataAs<JsRTCallbackAction, EventKind::CallbackOpActionTag>(evt);

            if(cbAction->RegisterLocation != nullptr && cbAction->RegisterLocation->HasValue())
            {LOGMEIN("TTActionEvents.cpp] 935\n");
                sourceLocation.SetLocation(*(cbAction->RegisterLocation));
                return true;
            }
            else
            {
                sourceLocation.Clear();
                return false; //we haven't been re-executed in replay so we don't have our info yet
            }
        }

        void JsRTCodeParseAction_SetBodyCtrId(EventLogEntry* parseEvent, uint64 bodyCtrId)
        {LOGMEIN("TTActionEvents.cpp] 947\n");
            JsRTCodeParseAction* cpAction = GetInlineEventDataAs<JsRTCodeParseAction, EventKind::CodeParseActionTag>(parseEvent);
            cpAction->BodyCtrId = bodyCtrId;
        }

        void JsRTCodeParseAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 953\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);
            const JsRTCodeParseAction* cpAction = GetInlineEventDataAs<JsRTCodeParseAction, EventKind::CodeParseActionTag>(evt);
            JsRTCodeParseAction_AdditionalInfo* cpInfo = cpAction->AdditionalInfo;

            Js::JavascriptFunction* function = nullptr;

            byte* script = cpInfo->SourceCode;
            uint32 scriptByteLength = cpInfo->SourceByteLength;

            TTDAssert(cpAction->AdditionalInfo->IsUtf8 == ((cpAction->AdditionalInfo->LoadFlag & LoadScriptFlag_Utf8Source) == LoadScriptFlag_Utf8Source), "Utf8 status is inconsistent!!!");

            SourceContextInfo * sourceContextInfo = ctx->GetSourceContextInfo((DWORD_PTR)cpInfo->SourceContextId, nullptr);

            if(sourceContextInfo == nullptr)
            {LOGMEIN("TTActionEvents.cpp] 968\n");
                const char16* srcUri = cpInfo->SourceUri.Contents;
                uint32 srcUriLength = cpInfo->SourceUri.Length;

                sourceContextInfo = ctx->CreateSourceContextInfo((DWORD_PTR)cpInfo->SourceContextId, srcUri, srcUriLength, nullptr);
            }

            TTDAssert(cpAction->AdditionalInfo->IsUtf8 || sizeof(wchar) == sizeof(char16), "Non-utf8 code only allowed on windows!!!");
            const int chsize = (cpAction->AdditionalInfo->LoadFlag & LoadScriptFlag_Utf8Source) ? sizeof(char) : sizeof(char16);
            SRCINFO si = {
                /* sourceContextInfo   */ sourceContextInfo,
                /* dlnHost             */ 0,
                /* ulColumnHost        */ 0,
                /* lnMinHost           */ 0,
                /* ichMinHost          */ 0,
                /* ichLimHost          */ static_cast<ULONG>(scriptByteLength / chsize), // OK to truncate since this is used to limit sourceText in debugDocument/compilation errors.
                /* ulCharOffset        */ 0,
                /* mod                 */ kmodGlobal,
                /* grfsi               */ 0
            };

            Js::Utf8SourceInfo* utf8SourceInfo = nullptr;
            CompileScriptException se;
            function = ctx->LoadScript(script, scriptByteLength, &si, &se,
                &utf8SourceInfo, Js::Constants::GlobalCode, cpInfo->LoadFlag, nullptr);

            TTDAssert(function != nullptr, "Something went wrong");

            Js::FunctionBody* fb = TTD::JsSupport::ForceAndGetFunctionBody(function->GetParseableFunctionInfo());

            ////
            //We don't do this automatically in the eval helper so do it here
            BEGIN_JS_RUNTIME_CALL(ctx);
            {LOGMEIN("TTActionEvents.cpp] 1001\n");
                ctx->TTDContextInfo->ProcessFunctionBodyOnLoad(fb, nullptr);
                ctx->TTDContextInfo->RegisterLoadedScript(fb, cpAction->BodyCtrId);
            }
            END_JS_RUNTIME_CALL(ctx);

            const HostScriptContextCallbackFunctor& hostFunctor = ctx->TTDHostCallbackFunctor;
            if(hostFunctor.pfOnScriptLoadCallback != nullptr)
            {LOGMEIN("TTActionEvents.cpp] 1009\n");
                hostFunctor.pfOnScriptLoadCallback(hostFunctor.HostData, function, utf8SourceInfo, &se);
            }
            ////

            JsRTActionHandleResultForReplay<JsRTCodeParseAction, EventKind::CodeParseActionTag>(executeContext, evt, (Js::Var)function);
        }

        void JsRTCodeParseAction_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 1018\n");
            JsRTCodeParseAction* cpAction = GetInlineEventDataAs<JsRTCodeParseAction, EventKind::CodeParseActionTag>(evt);
            JsRTCodeParseAction_AdditionalInfo* cpInfo = cpAction->AdditionalInfo;

            alloc.UnlinkAllocation(cpInfo->SourceCode);

            if(!IsNullPtrTTString(cpInfo->SourceUri))
            {LOGMEIN("TTActionEvents.cpp] 1025\n");
                alloc.UnlinkString(cpInfo->SourceUri);
            }

            alloc.UnlinkAllocation(cpAction->AdditionalInfo);
        }

        void JsRTCodeParseAction_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTActionEvents.cpp] 1033\n");
            const JsRTCodeParseAction* cpAction = GetInlineEventDataAs<JsRTCodeParseAction, EventKind::CodeParseActionTag>(evt);
            JsRTCodeParseAction_AdditionalInfo* cpInfo = cpAction->AdditionalInfo;

            writer->WriteKey(NSTokens::Key::argRetVal, NSTokens::Separator::CommaSeparator);
            NSSnapValues::EmitTTDVar(cpAction->Result, writer, NSTokens::Separator::NoSeparator);

            writer->WriteUInt64(NSTokens::Key::sourceContextId, cpInfo->SourceContextId, NSTokens::Separator::CommaSeparator);
            writer->WriteTag<LoadScriptFlag>(NSTokens::Key::loadFlag, cpInfo->LoadFlag, NSTokens::Separator::CommaSeparator);

            writer->WriteUInt64(NSTokens::Key::bodyCounterId, cpAction->BodyCtrId, NSTokens::Separator::CommaSeparator);

            writer->WriteString(NSTokens::Key::uri, cpInfo->SourceUri, NSTokens::Separator::CommaSeparator);

            writer->WriteBool(NSTokens::Key::boolVal, cpInfo->IsUtf8, NSTokens::Separator::CommaSeparator);
            writer->WriteLengthValue(cpInfo->SourceByteLength, NSTokens::Separator::CommaSeparator);

            JsSupport::WriteCodeToFile(threadContext, true, cpAction->BodyCtrId, cpInfo->IsUtf8, cpInfo->SourceCode, cpInfo->SourceByteLength);
        }

        void JsRTCodeParseAction_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 1054\n");
            JsRTCodeParseAction* cpAction = GetInlineEventDataAs<JsRTCodeParseAction, EventKind::CodeParseActionTag>(evt);
            cpAction->AdditionalInfo = alloc.SlabAllocateStruct<JsRTCodeParseAction_AdditionalInfo>();

            JsRTCodeParseAction_AdditionalInfo* cpInfo = cpAction->AdditionalInfo;

            reader->ReadKey(NSTokens::Key::argRetVal, true);
            cpAction->Result = NSSnapValues::ParseTTDVar(false, reader);

            cpInfo->SourceContextId = reader->ReadUInt64(NSTokens::Key::sourceContextId, true);
            cpInfo->LoadFlag = reader->ReadTag<LoadScriptFlag>(NSTokens::Key::loadFlag, true);

            cpAction->BodyCtrId = reader->ReadUInt64(NSTokens::Key::bodyCounterId, true);

            reader->ReadString(NSTokens::Key::uri, alloc, cpInfo->SourceUri, true);

            cpInfo->IsUtf8 = reader->ReadBool(NSTokens::Key::boolVal, true);
            cpInfo->SourceByteLength = reader->ReadLengthValue(true);

            cpInfo->SourceCode = alloc.SlabAllocateArray<byte>(cpAction->AdditionalInfo->SourceByteLength);

            JsSupport::ReadCodeFromFile(threadContext, true, cpAction->BodyCtrId, cpInfo->IsUtf8, cpInfo->SourceCode, cpInfo->SourceByteLength);
        }

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
        int64 JsRTCallFunctionAction_GetLastNestedEventTime(const EventLogEntry* evt)
        {LOGMEIN("TTActionEvents.cpp] 1080\n");
            const JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);

            return cfAction->AdditionalInfo->LastNestedEvent;
        }

        void JsRTCallFunctionAction_ProcessDiagInfoPre(EventLogEntry* evt, Js::Var funcVar, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 1087\n");
            JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);

            if(Js::JavascriptFunction::Is(funcVar))
            {LOGMEIN("TTActionEvents.cpp] 1091\n");
                Js::JavascriptString* displayName = Js::JavascriptFunction::FromVar(funcVar)->GetDisplayName();
                alloc.CopyStringIntoWLength(displayName->GetSz(), displayName->GetLength(), cfAction->AdditionalInfo->FunctionName);
            }
            else
            {
                alloc.CopyNullTermStringInto(_u("#not a function#"), cfAction->AdditionalInfo->FunctionName);
            }

            //In case we don't terminate add these nicely
            cfAction->AdditionalInfo->LastNestedEvent = TTD_EVENT_MAXTIME;
        }

        void JsRTCallFunctionAction_ProcessDiagInfoPost(EventLogEntry* evt, int64 lastNestedEvent)
        {LOGMEIN("TTActionEvents.cpp] 1105\n");
            JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);

            cfAction->AdditionalInfo->LastNestedEvent = lastNestedEvent;
        }
#endif

        void JsRTCallFunctionAction_ProcessArgs(EventLogEntry* evt, int32 rootDepth, int64 callEventTime, Js::Var funcVar, uint32 argc, Js::Var* argv, int64 topLevelCallbackEventTime, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 1113\n");
            JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            cfAction->AdditionalInfo = alloc.SlabAllocateStruct<JsRTCallFunctionAction_AdditionalInfo>();

            cfAction->CallbackDepth = rootDepth;
            cfAction->ArgCount = argc + 1;

            static_assert(sizeof(TTDVar) == sizeof(Js::Var), "These need to be the same size (and have same bit layout) for this to work!");

            cfAction->ArgArray = alloc.SlabAllocateArray<TTDVar>(cfAction->ArgCount);
            cfAction->ArgArray[0] = TTD_CONVERT_JSVAR_TO_TTDVAR(funcVar);
            js_memcpy_s(cfAction->ArgArray + 1, (cfAction->ArgCount -1) * sizeof(TTDVar), argv, argc * sizeof(Js::Var));

            cfAction->AdditionalInfo->CallEventTime = callEventTime;

            cfAction->AdditionalInfo->TopLevelCallbackEventTime = topLevelCallbackEventTime;

            cfAction->AdditionalInfo->AdditionalReplayInfo = nullptr;

            //Result is initialized when we register this with the popper
        }

        void JsRTCallFunctionAction_Execute(const EventLogEntry* evt, ThreadContextTTD* executeContext)
        {LOGMEIN("TTActionEvents.cpp] 1136\n");
            TTD_REPLAY_ACTIVE_CONTEXT(executeContext);

            const JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            JsRTCallFunctionAction_AdditionalInfo* cfInfo = cfAction->AdditionalInfo;

            ThreadContext* threadContext = ctx->GetThreadContext();

            Js::Var jsFunctionVar = InflateVarInReplay(executeContext, cfAction->ArgArray[0]);
            TTD_REPLAY_VALIDATE_INCOMING_FUNCTION(jsFunctionVar, ctx);

            Js::JavascriptFunction *jsFunction = Js::JavascriptFunction::FromVar(jsFunctionVar);

            //remove implicit constructor function as first arg in callInfo and argument loop below
            Js::CallInfo callInfo((ushort)(cfAction->ArgCount - 1));
            for(uint32 i = 1; i < cfAction->ArgCount; ++i)
            {LOGMEIN("TTActionEvents.cpp] 1152\n");
                 Js::Var argi = InflateVarInReplay(executeContext, cfAction->ArgArray[i]);
                 TTD_REPLAY_VALIDATE_INCOMING_REFERENCE(argi, ctx);

                 cfAction->AdditionalInfo->AdditionalReplayInfo->ExecArgs[i - 1] = argi;
            }
            Js::Arguments jsArgs(callInfo, cfAction->AdditionalInfo->AdditionalReplayInfo->ExecArgs);

            //If this isn't a root function then just call it -- don't need to reset anything and exceptions can just continue
            if(cfAction->CallbackDepth != 0)
            {LOGMEIN("TTActionEvents.cpp] 1162\n");
                Js::Var result = jsFunction->CallRootFunction(jsArgs, ctx, true);
                if(result != nullptr)
                {LOGMEIN("TTActionEvents.cpp] 1165\n");
                    Assert(result == nullptr || !Js::CrossSite::NeedMarshalVar(result, ctx));
                }

                //since we tag in JsRT we need to tag here too
                JsRTActionHandleResultForReplay<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(executeContext, evt, result);

                TTDAssert(NSLogEvents::EventCompletesNormally(evt), "Why did we get a different completion");
            }
            else
            {
                threadContext->TTDLog->ResetCallStackForTopLevelCall(cfInfo->TopLevelCallbackEventTime);

                try
                {LOGMEIN("TTActionEvents.cpp] 1179\n");
                    Js::Var result = jsFunction->CallRootFunction(jsArgs, ctx, true);

                    //since we tag in JsRT we need to tag here too
                    JsRTActionHandleResultForReplay<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(executeContext, evt, result);

                    TTDAssert(NSLogEvents::EventCompletesNormally(evt), "Why did we get a different completion");
                }
                catch(const Js::JavascriptException& err)
                {LOGMEIN("TTActionEvents.cpp] 1188\n");
                    TTDAssert(NSLogEvents::EventCompletesWithException(evt), "Why did we get a different exception");

                    if(executeContext->GetActiveScriptContext()->ShouldPerformDebuggerAction())
                    {LOGMEIN("TTActionEvents.cpp] 1192\n");
                        //convert to uncaught debugger exception for host
                        TTDebuggerSourceLocation lastLocation;
                        threadContext->TTDLog->GetLastExecutedTimeAndPositionForDebugger(lastLocation);
                        JsRTCallFunctionAction_SetLastExecutedStatementAndFrameInfo(const_cast<EventLogEntry*>(evt), lastLocation);

                        err.GetAndClear();  // discard exception object

                        //Reset any step controller logic
                        if(ctx->GetThreadContext()->GetDebugManager() != nullptr)
                        {LOGMEIN("TTActionEvents.cpp] 1202\n");
                            ctx->GetThreadContext()->GetDebugManager()->stepController.Deactivate();
                        }

                        throw TTDebuggerAbortException::CreateUncaughtExceptionAbortRequest(lastLocation.GetRootEventTime(), _u("Uncaught JavaScript exception -- Propagate to top-level."));
                    }

                    throw;
                }
                catch(Js::ScriptAbortException)
                {LOGMEIN("TTActionEvents.cpp] 1212\n");
                    TTDAssert(NSLogEvents::EventCompletesWithException(evt), "Why did we get a different exception");

                    if(executeContext->GetActiveScriptContext()->ShouldPerformDebuggerAction())
                    {LOGMEIN("TTActionEvents.cpp] 1216\n");
                        //convert to uncaught debugger exception for host
                        TTDebuggerSourceLocation lastLocation;
                        threadContext->TTDLog->GetLastExecutedTimeAndPositionForDebugger(lastLocation);
                        JsRTCallFunctionAction_SetLastExecutedStatementAndFrameInfo(const_cast<EventLogEntry*>(evt), lastLocation);

                        throw TTDebuggerAbortException::CreateUncaughtExceptionAbortRequest(lastLocation.GetRootEventTime(), _u("Uncaught Script exception -- Propagate to top-level."));
                    }
                    else
                    {
                        throw;
                    }
                }
                catch(...)
                {LOGMEIN("TTActionEvents.cpp] 1230\n");
                    if(executeContext->GetActiveScriptContext()->ShouldPerformDebuggerAction())
                    {LOGMEIN("TTActionEvents.cpp] 1232\n");
                        TTDebuggerSourceLocation lastLocation;
                        threadContext->TTDLog->GetLastExecutedTimeAndPositionForDebugger(lastLocation);
                        JsRTCallFunctionAction_SetLastExecutedStatementAndFrameInfo(const_cast<EventLogEntry*>(evt), lastLocation);
                    }

                    throw;
                }
            }
        }

        void JsRTCallFunctionAction_UnloadEventMemory(EventLogEntry* evt, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 1244\n");
            JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            JsRTCallFunctionAction_AdditionalInfo* cfInfo = cfAction->AdditionalInfo;

            alloc.UnlinkAllocation(cfAction->ArgArray);

            if(cfInfo->AdditionalReplayInfo != nullptr)
            {LOGMEIN("TTActionEvents.cpp] 1251\n");
                if(cfInfo->AdditionalReplayInfo->ExecArgs != nullptr)
                {LOGMEIN("TTActionEvents.cpp] 1253\n");
                    alloc.UnlinkAllocation(cfInfo->AdditionalReplayInfo->ExecArgs);
                }

                JsRTCallFunctionAction_UnloadSnapshot(evt);

                if(cfInfo->AdditionalReplayInfo->LastExecutedLocation.HasValue())
                {LOGMEIN("TTActionEvents.cpp] 1260\n");
                    cfInfo->AdditionalReplayInfo->LastExecutedLocation.Clear();
                }

                alloc.UnlinkAllocation(cfInfo->AdditionalReplayInfo);
            }

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            alloc.UnlinkString(cfInfo->FunctionName);
#endif

            alloc.UnlinkAllocation(cfInfo);
        }

        void JsRTCallFunctionAction_Emit(const EventLogEntry* evt, FileWriter* writer, ThreadContext* threadContext)
        {LOGMEIN("TTActionEvents.cpp] 1275\n");
            const JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            const JsRTCallFunctionAction_AdditionalInfo* cfInfo = cfAction->AdditionalInfo;

            writer->WriteKey(NSTokens::Key::argRetVal, NSTokens::Separator::CommaSeparator);
            NSSnapValues::EmitTTDVar(cfAction->Result, writer, NSTokens::Separator::NoSeparator);

            writer->WriteInt32(NSTokens::Key::rootNestingDepth, cfAction->CallbackDepth, NSTokens::Separator::CommaSeparator);

            writer->WriteLengthValue(cfAction->ArgCount, NSTokens::Separator::CommaSeparator);

            writer->WriteSequenceStart_DefaultKey(NSTokens::Separator::CommaSeparator);
            for(uint32 i = 0; i < cfAction->ArgCount; ++i)
            {LOGMEIN("TTActionEvents.cpp] 1288\n");
                NSTokens::Separator sep = (i != 0) ? NSTokens::Separator::CommaSeparator : NSTokens::Separator::NoSeparator;
                NSSnapValues::EmitTTDVar(cfAction->ArgArray[i], writer, sep);
            }
            writer->WriteSequenceEnd();

            writer->WriteInt64(NSTokens::Key::eventTime, cfInfo->CallEventTime, NSTokens::Separator::CommaSeparator);

            writer->WriteInt64(NSTokens::Key::eventTime, cfInfo->TopLevelCallbackEventTime, NSTokens::Separator::CommaSeparator);

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            writer->WriteInt64(NSTokens::Key::i64Val, cfInfo->LastNestedEvent, NSTokens::Separator::CommaSeparator);
            writer->WriteString(NSTokens::Key::name, cfInfo->FunctionName, NSTokens::Separator::CommaSeparator);
#endif
        }

        void JsRTCallFunctionAction_Parse(EventLogEntry* evt, ThreadContext* threadContext, FileReader* reader, UnlinkableSlabAllocator& alloc)
        {LOGMEIN("TTActionEvents.cpp] 1305\n");
            JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            cfAction->AdditionalInfo = alloc.SlabAllocateStruct<JsRTCallFunctionAction_AdditionalInfo>();

            reader->ReadKey(NSTokens::Key::argRetVal, true);
            cfAction->Result = NSSnapValues::ParseTTDVar(false, reader);

            cfAction->CallbackDepth = reader->ReadInt32(NSTokens::Key::rootNestingDepth, true);

            cfAction->ArgCount = reader->ReadLengthValue(true);
            cfAction->ArgArray = alloc.SlabAllocateArray<TTDVar>(cfAction->ArgCount);

            reader->ReadSequenceStart_WDefaultKey(true);
            for(uint32 i = 0; i < cfAction->ArgCount; ++i)
            {LOGMEIN("TTActionEvents.cpp] 1319\n");
                cfAction->ArgArray[i] = NSSnapValues::ParseTTDVar(i != 0, reader);
            }
            reader->ReadSequenceEnd();

            JsRTCallFunctionAction_AdditionalInfo* cfInfo = cfAction->AdditionalInfo;

            cfInfo->CallEventTime = reader->ReadInt64(NSTokens::Key::eventTime, true);

            cfInfo->TopLevelCallbackEventTime = reader->ReadInt64(NSTokens::Key::eventTime, true);

            cfInfo->AdditionalReplayInfo = alloc.SlabAllocateStruct<JsRTCallFunctionAction_ReplayAdditionalInfo>();

            cfInfo->AdditionalReplayInfo->RtRSnap = nullptr;
            cfInfo->AdditionalReplayInfo->ExecArgs = (cfAction->ArgCount > 1) ? alloc.SlabAllocateArray<Js::Var>(cfAction->ArgCount - 1) : nullptr; //ArgCount includes slot for function which we don't use in exec

            cfInfo->AdditionalReplayInfo->LastExecutedLocation.Initialize();

#if ENABLE_TTD_INTERNAL_DIAGNOSTICS
            cfInfo->LastNestedEvent = reader->ReadInt64(NSTokens::Key::i64Val, true);
            reader->ReadString(NSTokens::Key::name, alloc, cfInfo->FunctionName, true);
#endif
        }

        void JsRTCallFunctionAction_UnloadSnapshot(EventLogEntry* evt)
        {LOGMEIN("TTActionEvents.cpp] 1344\n");
            JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            JsRTCallFunctionAction_AdditionalInfo* cfInfo = cfAction->AdditionalInfo;

            if(cfInfo->AdditionalReplayInfo != nullptr && cfInfo->AdditionalReplayInfo->RtRSnap != nullptr)
            {
                TT_HEAP_DELETE(SnapShot, cfInfo->AdditionalReplayInfo->RtRSnap);
                cfInfo->AdditionalReplayInfo->RtRSnap = nullptr;
            }
        }

        void JsRTCallFunctionAction_SetLastExecutedStatementAndFrameInfo(EventLogEntry* evt, const TTDebuggerSourceLocation& lastSourceLocation)
        {LOGMEIN("TTActionEvents.cpp] 1356\n");
            JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            JsRTCallFunctionAction_AdditionalInfo* cfInfo = cfAction->AdditionalInfo;

            cfInfo->AdditionalReplayInfo->LastExecutedLocation.SetLocation(lastSourceLocation);
        }

        bool JsRTCallFunctionAction_GetLastExecutedStatementAndFrameInfoForDebugger(const EventLogEntry* evt, TTDebuggerSourceLocation& lastSourceInfo)
        {LOGMEIN("TTActionEvents.cpp] 1364\n");
            const JsRTCallFunctionAction* cfAction = GetInlineEventDataAs<JsRTCallFunctionAction, EventKind::CallExistingFunctionActionTag>(evt);
            JsRTCallFunctionAction_AdditionalInfo* cfInfo = cfAction->AdditionalInfo;
            if(cfInfo->AdditionalReplayInfo->LastExecutedLocation.HasValue())
            {LOGMEIN("TTActionEvents.cpp] 1368\n");
                lastSourceInfo.SetLocation(cfInfo->AdditionalReplayInfo->LastExecutedLocation);
                return true;
            }
            else
            {
                lastSourceInfo.Clear();
                return false;
            }
        }
    }
}

#endif
