//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Types/DeferredTypeHandler.h"
namespace Js
{
    // This is a wrapper class for javascript functions that are added directly to the JS engine via BuildDirectFunction
    // or CreateConstructor. We add a thunk before calling into user's direct C++ methods, with additional checks:
    // . check the script site is still alive.
    // . convert globalObject to hostObject
    // . leavescriptstart/end
    // . wrap the result value with potential cross site access

    JavascriptExternalFunction::JavascriptExternalFunction(ExternalMethod entryPoint, DynamicType* type)
        : RuntimeFunction(type, &EntryInfo::ExternalFunctionThunk), nativeMethod(entryPoint), signature(nullptr), callbackState(nullptr), initMethod(nullptr),
        oneBit(1), typeSlots(0), hasAccessors(0), prototypeTypeId(-1), flags(0)
    {LOGMEIN("JavascriptExternalFunction.cpp] 18\n");
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptExternalFunction::JavascriptExternalFunction(ExternalMethod entryPoint, DynamicType* type, InitializeMethod method, unsigned short deferredSlotCount, bool accessors)
        : RuntimeFunction(type, &EntryInfo::ExternalFunctionThunk), nativeMethod(entryPoint), signature(nullptr), callbackState(nullptr), initMethod(method),
        oneBit(1), typeSlots(deferredSlotCount), hasAccessors(accessors),prototypeTypeId(-1), flags(0)
    {LOGMEIN("JavascriptExternalFunction.cpp] 25\n");
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptExternalFunction::JavascriptExternalFunction(DynamicType* type, InitializeMethod method, unsigned short deferredSlotCount, bool accessors)
        : RuntimeFunction(type, &EntryInfo::DefaultExternalFunctionThunk), nativeMethod(nullptr), signature(nullptr), callbackState(nullptr), initMethod(method),
        oneBit(1), typeSlots(deferredSlotCount), hasAccessors(accessors), prototypeTypeId(-1), flags(0)
    {LOGMEIN("JavascriptExternalFunction.cpp] 32\n");
        DebugOnly(VerifyEntryPoint());
    }


    JavascriptExternalFunction::JavascriptExternalFunction(JavascriptExternalFunction* entryPoint, DynamicType* type)
        : RuntimeFunction(type, &EntryInfo::WrappedFunctionThunk), wrappedMethod(entryPoint), callbackState(nullptr), initMethod(nullptr),
        oneBit(1), typeSlots(0), hasAccessors(0), prototypeTypeId(-1), flags(0)
    {LOGMEIN("JavascriptExternalFunction.cpp] 40\n");
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptExternalFunction::JavascriptExternalFunction(StdCallJavascriptMethod entryPoint, DynamicType* type)
        : RuntimeFunction(type, &EntryInfo::StdCallExternalFunctionThunk), stdCallNativeMethod(entryPoint), signature(nullptr), callbackState(nullptr), initMethod(nullptr),
        oneBit(1), typeSlots(0), hasAccessors(0), prototypeTypeId(-1), flags(0)
    {LOGMEIN("JavascriptExternalFunction.cpp] 47\n");
        DebugOnly(VerifyEntryPoint());
    }

    JavascriptExternalFunction::JavascriptExternalFunction(DynamicType *type)
        : RuntimeFunction(type, &EntryInfo::ExternalFunctionThunk), nativeMethod(nullptr), signature(nullptr), callbackState(nullptr), initMethod(nullptr),
        oneBit(1), typeSlots(0), hasAccessors(0), prototypeTypeId(-1), flags(0)
    {LOGMEIN("JavascriptExternalFunction.cpp] 54\n");
        DebugOnly(VerifyEntryPoint());
    }

    void __cdecl JavascriptExternalFunction::DeferredInitializer(DynamicObject* instance, DeferredTypeHandlerBase* typeHandler, DeferredInitializeMode mode)
    {LOGMEIN("JavascriptExternalFunction.cpp] 59\n");
        JavascriptExternalFunction* object = static_cast<JavascriptExternalFunction*>(instance);
        HRESULT hr = E_FAIL;

        ScriptContext* scriptContext = object->GetScriptContext();
        AnalysisAssert(scriptContext);
        // Don't call the implicit call if disable implicit call
        if (scriptContext->GetThreadContext()->IsDisableImplicitCall())
        {LOGMEIN("JavascriptExternalFunction.cpp] 67\n");
            scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_External);
            //we will return if we get call further into implicitcalls.
            return;
        }

        if (scriptContext->IsClosed() || scriptContext->IsInvalidatedForHostObjects())
        {LOGMEIN("JavascriptExternalFunction.cpp] 74\n");
            Js::JavascriptError::MapAndThrowError(scriptContext, E_ACCESSDENIED);
        }
        ThreadContext* threadContext = scriptContext->GetThreadContext();

        typeHandler->Convert(instance, mode, object->typeSlots, object->hasAccessors);

        BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext)
        {LOGMEIN("JavascriptExternalFunction.cpp] 82\n");
            ASYNC_HOST_OPERATION_START(threadContext);

            hr = object->initMethod(instance);

            ASYNC_HOST_OPERATION_END(threadContext);
        }
        END_LEAVE_SCRIPT_INTERNAL(scriptContext);

        if (FAILED(hr))
        {LOGMEIN("JavascriptExternalFunction.cpp] 92\n");
            Js::JavascriptError::MapAndThrowError(scriptContext, hr);
        }

        JavascriptString * functionName = nullptr;
        if (scriptContext->GetConfig()->IsES6FunctionNameEnabled() &&
            object->GetFunctionName(&functionName))
        {LOGMEIN("JavascriptExternalFunction.cpp] 99\n");
            object->SetPropertyWithAttributes(PropertyIds::name, functionName, PropertyConfigurable, nullptr);
        }

    }

    void JavascriptExternalFunction::PrepareExternalCall(Js::Arguments * args)
    {LOGMEIN("JavascriptExternalFunction.cpp] 106\n");
        ScriptContext * scriptContext = this->type->GetScriptContext();
        Assert(!scriptContext->GetThreadContext()->IsDisableImplicitException());
        scriptContext->VerifyAlive();

        Assert(scriptContext->GetThreadContext()->IsScriptActive());

        if (args->Info.Count == 0)
        {LOGMEIN("JavascriptExternalFunction.cpp] 114\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_This_NullOrUndefined);
        }

        Var &thisVar = args->Values[0];

        Js::TypeId typeId = Js::JavascriptOperators::GetTypeId(thisVar);

        Js::RecyclableObject* directHostObject = nullptr;
        switch(typeId)
        {LOGMEIN("JavascriptExternalFunction.cpp] 124\n");
        case TypeIds_Integer:
#if FLOATVAR
        case TypeIds_Number:
#endif // FLOATVAR
            Assert(!Js::RecyclableObject::Is(thisVar));
            break;
        default:
            {LOGMEIN("JavascriptExternalFunction.cpp] 132\n");
                Assert(Js::RecyclableObject::Is(thisVar));

                ScriptContext* scriptContextThisVar = Js::RecyclableObject::FromVar(thisVar)->GetScriptContext();
                // We need to verify "this" pointer is active as well. The problem is that DOM prototype functions are
                // the same across multiple frames, and caller can do function.call(closedthis)
                Assert(!scriptContext->GetThreadContext()->IsDisableImplicitException());
                scriptContextThisVar->VerifyAlive();

                // translate direct host for fastDOM.
                switch(typeId)
                {LOGMEIN("JavascriptExternalFunction.cpp] 143\n");
                case Js::TypeIds_GlobalObject:
                    {LOGMEIN("JavascriptExternalFunction.cpp] 145\n");
                        Js::GlobalObject* srcGlobalObject = (Js::GlobalObject*)(void*)(thisVar);
                        directHostObject = srcGlobalObject->GetDirectHostObject();
                        // For jsrt, direct host object can be null. If thats the case don't change it.
                        if (directHostObject != nullptr)
                        {LOGMEIN("JavascriptExternalFunction.cpp] 150\n");
                            thisVar = directHostObject;
                        }

                    }
                    break;
                case Js::TypeIds_Undefined:
                case Js::TypeIds_Null:
                    {LOGMEIN("JavascriptExternalFunction.cpp] 158\n");
                        // Call to DOM function with this as "undefined" or "null"
                        // This should be converted to Global object
                        Js::GlobalObject* srcGlobalObject = scriptContextThisVar->GetGlobalObject() ;
                        directHostObject = srcGlobalObject->GetDirectHostObject();
                        // For jsrt, direct host object can be null. If thats the case don't change it.
                        if (directHostObject != nullptr)
                        {LOGMEIN("JavascriptExternalFunction.cpp] 165\n");
                            thisVar = directHostObject;
                        }
                    }
                    break;
                }
            }
            break;
        }
    }

    Var JavascriptExternalFunction::ExternalFunctionThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(function);

        ScriptContext * scriptContext = externalFunction->type->GetScriptContext();

#ifdef ENABLE_DIRECTCALL_TELEMETRY
        DirectCallTelemetry::AutoLogger logger(scriptContext, externalFunction, &args);
#endif

        externalFunction->PrepareExternalCall(&args);

#if ENABLE_TTD
        Var result = nullptr;

        if(scriptContext->ShouldPerformRecordOrReplayAction())
        {LOGMEIN("JavascriptExternalFunction.cpp] 193\n");
            result = JavascriptExternalFunction::HandleRecordReplayExternalFunction_Thunk(externalFunction, callInfo, args, scriptContext);
        }
        else
        {
            BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext)
            {LOGMEIN("JavascriptExternalFunction.cpp] 199\n");
                // Don't do stack probe since BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION does that for us already
                result = externalFunction->nativeMethod(function, callInfo, args.Values);
            }
            END_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext);
        }
#else
        Var result = nullptr;
        BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext)
        {LOGMEIN("JavascriptExternalFunction.cpp] 208\n");
            // Don't do stack probe since BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION does that for us already
            result = externalFunction->nativeMethod(function, callInfo, args.Values);
        }
        END_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext);
#endif

        if (result == nullptr)
        {LOGMEIN("JavascriptExternalFunction.cpp] 216\n");
#pragma warning(push)
#pragma warning(disable:6011) // scriptContext cannot be null here
            result = scriptContext->GetLibrary()->GetUndefined();
#pragma warning(pop)
        }
        else
        {
            result = CrossSite::MarshalVar(scriptContext, result);
        }

        return result;
    }

    Var JavascriptExternalFunction::WrappedFunctionThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(function);
        ScriptContext* scriptContext = externalFunction->type->GetScriptContext();
        Assert(!scriptContext->GetThreadContext()->IsDisableImplicitException());
        scriptContext->VerifyAlive();
        Assert(scriptContext->GetThreadContext()->IsScriptActive());

        // Make sure the callee knows we are a wrapped function thunk
        args.Info.Flags = (Js::CallFlags) (((int32) args.Info.Flags) | CallFlags_Wrapped);

        // don't need to leave script here, ExternalFunctionThunk will
        Assert(externalFunction->wrappedMethod->GetFunctionInfo()->GetOriginalEntryPoint() == JavascriptExternalFunction::ExternalFunctionThunk);
        return JavascriptFunction::CallFunction<true>(externalFunction->wrappedMethod, externalFunction->wrappedMethod->GetEntryPoint(), args);
    }

    Var JavascriptExternalFunction::DefaultExternalFunctionThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        TypeId typeId = function->GetTypeId();
        rtErrors err = typeId == TypeIds_Undefined || typeId == TypeIds_Null ? JSERR_NeedObject : JSERR_NeedFunction;
        JavascriptError::ThrowTypeError(function->GetScriptContext(), err);
    }

    Var JavascriptExternalFunction::StdCallExternalFunctionThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(function);

        externalFunction->PrepareExternalCall(&args);

        ScriptContext * scriptContext = externalFunction->type->GetScriptContext();
        AnalysisAssert(scriptContext);
        Var result = NULL;

#if ENABLE_TTD
        if(scriptContext->ShouldPerformRecordOrReplayAction())
        {LOGMEIN("JavascriptExternalFunction.cpp] 267\n");
            result = JavascriptExternalFunction::HandleRecordReplayExternalFunction_StdThunk(function, callInfo, args, scriptContext);
        }
        else
        {
            BEGIN_LEAVE_SCRIPT(scriptContext)
            {LOGMEIN("JavascriptExternalFunction.cpp] 273\n");
                result = externalFunction->stdCallNativeMethod(function, ((callInfo.Flags & CallFlags_New) != 0), args.Values, args.Info.Count, externalFunction->callbackState);
            }
            END_LEAVE_SCRIPT(scriptContext);
        }
#else
        BEGIN_LEAVE_SCRIPT(scriptContext)
        {LOGMEIN("JavascriptExternalFunction.cpp] 280\n");
            result = externalFunction->stdCallNativeMethod(function, ((callInfo.Flags & CallFlags_New) != 0), args.Values, args.Info.Count, externalFunction->callbackState);
        }
        END_LEAVE_SCRIPT(scriptContext);
#endif

        if (result != nullptr && !Js::TaggedNumber::Is(result))
        {LOGMEIN("JavascriptExternalFunction.cpp] 287\n");
            if (!Js::RecyclableObject::Is(result))
            {LOGMEIN("JavascriptExternalFunction.cpp] 289\n");
                Js::Throw::InternalError();
            }

            Js::RecyclableObject * obj = Js::RecyclableObject::FromVar(result);

            // For JSRT, we could get result marshalled in different context.
            bool isJSRT = scriptContext->GetThreadContext()->IsJSRT();
            if (!isJSRT && obj->GetScriptContext() != scriptContext)
            {LOGMEIN("JavascriptExternalFunction.cpp] 298\n");
                Js::Throw::InternalError();
            }
        }

        if (scriptContext->HasRecordedException())
        {LOGMEIN("JavascriptExternalFunction.cpp] 304\n");
            bool considerPassingToDebugger = false;
            JavascriptExceptionObject* recordedException = scriptContext->GetAndClearRecordedException(&considerPassingToDebugger);
            if (recordedException != nullptr)
            {LOGMEIN("JavascriptExternalFunction.cpp] 308\n");
                // If this is script termination, then throw ScriptAbortExceptio, else throw normal Exception object.
                if (recordedException == scriptContext->GetThreadContext()->GetPendingTerminatedErrorObject())
                {LOGMEIN("JavascriptExternalFunction.cpp] 311\n");
                    throw Js::ScriptAbortException();
                }
                else
                {
                    JavascriptExceptionOperators::RethrowExceptionObject(recordedException, scriptContext, considerPassingToDebugger);
                }
            }
        }

        if (result == nullptr)
        {LOGMEIN("JavascriptExternalFunction.cpp] 322\n");
            result = scriptContext->GetLibrary()->GetUndefined();
        }
        else
        {
            result = CrossSite::MarshalVar(scriptContext, result);
        }

        return result;
    }

    BOOL JavascriptExternalFunction::SetLengthProperty(Var length)
    {LOGMEIN("JavascriptExternalFunction.cpp] 334\n");
        return DynamicObject::SetPropertyWithAttributes(PropertyIds::length, length, PropertyConfigurable, NULL, PropertyOperation_None, SideEffects_None);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType JavascriptExternalFunction::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptExternalFunction.cpp] 340\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapExternalFunctionObject;
    }

    void JavascriptExternalFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptExternalFunction.cpp] 345\n");
        TTD::TTDVar fnameId = TTD_CONVERT_JSVAR_TO_TTDVAR(this->functionNameId);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapExternalFunctionObject>(objData, fnameId);
    }

    Var JavascriptExternalFunction::HandleRecordReplayExternalFunction_Thunk(Js::JavascriptFunction* function, CallInfo& callInfo, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptExternalFunction.cpp] 351\n");
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(function);

        Var result = nullptr;

        if(scriptContext->ShouldPerformReplayAction())
        {LOGMEIN("JavascriptExternalFunction.cpp] 357\n");
            TTD::TTDNestingDepthAutoAdjuster logPopper(scriptContext->GetThreadContext());
            scriptContext->GetThreadContext()->TTDLog->ReplayExternalCallEvent(externalFunction, args.Info.Count, args.Values, &result);
        }
        else
        {
            TTDAssert(scriptContext->ShouldPerformRecordAction(), "Check either record/replay before calling!!!");

            TTD::EventLog* elog = scriptContext->GetThreadContext()->TTDLog;

            TTD::TTDNestingDepthAutoAdjuster logPopper(scriptContext->GetThreadContext());
            TTD::NSLogEvents::EventLogEntry* callEvent = elog->RecordExternalCallEvent(externalFunction, scriptContext->GetThreadContext()->TTDRootNestingCount, args.Info.Count, args.Values, false);

            BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext)
            {LOGMEIN("JavascriptExternalFunction.cpp] 371\n");
                // Don't do stack probe since BEGIN_LEAVE_SCRIPT_WITH_EXCEPTION does that for us already
                result = externalFunction->nativeMethod(function, callInfo, args.Values);
            }
            END_LEAVE_SCRIPT_WITH_EXCEPTION(scriptContext);

            //Exceptions should be prohibited so no need to do extra work
            elog->RecordExternalCallEvent_Complete(externalFunction, callEvent, result);
        }

        return result;
    }

    Var JavascriptExternalFunction::HandleRecordReplayExternalFunction_StdThunk(Js::RecyclableObject* function, CallInfo& callInfo, Arguments& args, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptExternalFunction.cpp] 385\n");
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(function);

        Var result = nullptr;

        if(scriptContext->ShouldPerformReplayAction())
        {LOGMEIN("JavascriptExternalFunction.cpp] 391\n");
            TTD::TTDNestingDepthAutoAdjuster logPopper(scriptContext->GetThreadContext());
            scriptContext->GetThreadContext()->TTDLog->ReplayExternalCallEvent(externalFunction, args.Info.Count, args.Values, &result);
        }
        else
        {
            TTDAssert(scriptContext->ShouldPerformRecordAction(), "Check either record/replay before calling!!!");

            TTD::EventLog* elog = scriptContext->GetThreadContext()->TTDLog;

            TTD::TTDNestingDepthAutoAdjuster logPopper(scriptContext->GetThreadContext());
            TTD::NSLogEvents::EventLogEntry* callEvent = elog->RecordExternalCallEvent(externalFunction, scriptContext->GetThreadContext()->TTDRootNestingCount, args.Info.Count, args.Values, true);

            BEGIN_LEAVE_SCRIPT(scriptContext)
            {LOGMEIN("JavascriptExternalFunction.cpp] 405\n");
                result = externalFunction->stdCallNativeMethod(function, ((callInfo.Flags & CallFlags_New) != 0), args.Values, args.Info.Count, externalFunction->callbackState);
            }
            END_LEAVE_SCRIPT(scriptContext);

            elog->RecordExternalCallEvent_Complete(externalFunction, callEvent, result);
        }

        return result;
    }

    Var __stdcall JavascriptExternalFunction::TTDReplayDummyExternalMethod(Js::Var callee, bool isConstructCall, Var *args, USHORT cargs, void *callbackState)
    {LOGMEIN("JavascriptExternalFunction.cpp] 417\n");
        JavascriptExternalFunction* externalFunction = static_cast<JavascriptExternalFunction*>(callee);

        ScriptContext* scriptContext = externalFunction->type->GetScriptContext();
        TTD::EventLog* elog = scriptContext->GetThreadContext()->TTDLog;
        TTDAssert(elog != nullptr, "How did this get created then???");

        //If this flag is set then this is ok (the debugger may be evaluating this so just return undef -- otherwise this is an error
        if(!elog->IsDebugModeFlagSet())
        {
            TTDAssert(false, "This should never be reached in pure replay mode!!!");
            return nullptr;
        }

        return scriptContext->GetLibrary()->GetUndefined();
    }
#endif
}
