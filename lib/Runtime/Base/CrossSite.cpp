//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "Library/JavascriptProxy.h"
#include "Library/HostObjectBase.h"
#include "Types/WithScopeObject.h"

#if ENABLE_CROSSSITE_TRACE
#define TTD_XSITE_LOG(CTX, MSG, VAR) if((CTX)->ShouldPerformRecordOrReplayAction()) \
{TRACE_IT(33540); \
    (CTX)->GetThreadContext()->TTDLog->GetTraceLogger()->WriteLiteralMsg(" -XS- "); \
    (CTX)->GetThreadContext()->TTDLog->GetTraceLogger()->WriteLiteralMsg(MSG); \
    (CTX)->GetThreadContext()->TTDLog->GetTraceLogger()->WriteVar(VAR); \
    (CTX)->GetThreadContext()->TTDLog->GetTraceLogger()->WriteLiteralMsg("\n"); \
}
#else
#define TTD_XSITE_LOG(CTX, MSG, VAR)
#endif

namespace Js
{

    BOOL CrossSite::NeedMarshalVar(Var instance, ScriptContext * requestContext)
    {TRACE_IT(33541);
        if (TaggedNumber::Is(instance))
        {TRACE_IT(33542);
            return FALSE;
        }
        RecyclableObject * object = RecyclableObject::FromVar(instance);
        if (object->GetScriptContext() == requestContext)
        {TRACE_IT(33543);
            return FALSE;
        }
        if (DynamicType::Is(object->GetTypeId()))
        {TRACE_IT(33544);
            return !DynamicObject::FromVar(object)->IsCrossSiteObject() && !object->IsExternal();
        }
        return TRUE;
    }

    void CrossSite::MarshalDynamicObject(ScriptContext * scriptContext, DynamicObject * object)
    {TRACE_IT(33545);
        Assert(!object->IsExternal() && !object->IsCrossSiteObject());

        TTD_XSITE_LOG(scriptContext, "MarshalDynamicObject", object);

        object->MarshalToScriptContext(scriptContext);
        if (object->GetTypeId() == TypeIds_Function)
        {TRACE_IT(33546);
            AssertMsg(object != object->GetScriptContext()->GetLibrary()->GetDefaultAccessorFunction(), "default accessor marshalled");
            JavascriptFunction * function = JavascriptFunction::FromVar(object);

            // See if this function is one that the host needs to handle
            HostScriptContext * hostScriptContext = scriptContext->GetHostScriptContext();
            if (!hostScriptContext || !hostScriptContext->SetCrossSiteForFunctionType(function))
            {TRACE_IT(33547);
                if (function->GetDynamicType()->GetIsShared())
                {
                    TTD_XSITE_LOG(scriptContext, "SetCrossSiteForSharedFunctionType ", object);

                    function->GetLibrary()->SetCrossSiteForSharedFunctionType(function);
                }
                else
                {
                    TTD_XSITE_LOG(scriptContext, "setEntryPoint->CurrentCrossSiteThunk ", object);

                    function->SetEntryPoint(function->GetScriptContext()->CurrentCrossSiteThunk);
                }
            }
        }
    }

    void CrossSite::MarshalPrototypeChain(ScriptContext* scriptContext, DynamicObject * object)
    {TRACE_IT(33548);
        RecyclableObject * prototype = object->GetPrototype();
        while (prototype->GetTypeId() != TypeIds_Null && prototype->GetTypeId() != TypeIds_HostDispatch)
        {TRACE_IT(33549);
            // We should not see any static type or host dispatch here
            DynamicObject * prototypeObject = DynamicObject::FromVar(prototype);
            if (prototypeObject->IsCrossSiteObject())
            {TRACE_IT(33550);
                break;
            }
            if (scriptContext != prototypeObject->GetScriptContext() && !prototypeObject->IsExternal())
            {
                MarshalDynamicObject(scriptContext, prototypeObject);
            }
            prototype = prototypeObject->GetPrototype();
        }
    }

    void CrossSite::MarshalDynamicObjectAndPrototype(ScriptContext* scriptContext, DynamicObject * object)
    {
        MarshalDynamicObject(scriptContext, object);
        MarshalPrototypeChain(scriptContext, object);
    }

    Var CrossSite::MarshalFrameDisplay(ScriptContext* scriptContext, FrameDisplay *display)
    {
        TTD_XSITE_LOG(scriptContext, "MarshalFrameDisplay", nullptr);

        uint16 length = display->GetLength();
        FrameDisplay *newDisplay =
            RecyclerNewPlus(scriptContext->GetRecycler(), length * sizeof(Var), FrameDisplay, length);
        for (uint16 i = 0; i < length; i++)
        {TRACE_IT(33551);
            Var value = display->GetItem(i);
            if (WithScopeObject::Is(value))
            {TRACE_IT(33552);
                // Here we are marshalling the wrappedObject and then ReWrapping th object in the new context.
                value = JavascriptOperators::ToWithObject(CrossSite::MarshalVar(scriptContext, WithScopeObject::FromVar(value)->GetWrappedObject()), scriptContext);
            }
            else
            {TRACE_IT(33553);
                value = CrossSite::MarshalVar(scriptContext, value);
            }
            newDisplay->SetItem(i, value);
        }

        return (Var)newDisplay;
    }

    // static
    Var CrossSite::MarshalVar(ScriptContext* scriptContext, Var value, bool fRequestWrapper)
    {TRACE_IT(33554);
        // value might be null from disable implicit call
        if (value == nullptr || Js::TaggedNumber::Is(value))
        {TRACE_IT(33555);
            return value;
        }
        Js::RecyclableObject* object =  RecyclableObject::FromVar(value);
        if (fRequestWrapper || scriptContext != object->GetScriptContext())
        {TRACE_IT(33556);
            return MarshalVarInner(scriptContext, object, fRequestWrapper);
        }
        return value;
    }
    
    Var CrossSite::MarshalStringVar(ScriptContext* scriptContext, Var value, bool fRequestWrapper)
    {
        Js::RecyclableObject* object =  RecyclableObject::FromVar(value);
        if (value && (fRequestWrapper || scriptContext != object->GetScriptContext()))
        {
            return MarshalVarInner(scriptContext, object, fRequestWrapper);
        }
        return value;
    }

    bool CrossSite::DoRequestWrapper(Js::RecyclableObject* object, bool fRequestWrapper)
    {TRACE_IT(33557);
        return fRequestWrapper && JavascriptFunction::Is(object) && JavascriptFunction::FromVar(object)->IsExternalFunction();
    }

#if ENABLE_TTD
    void CrossSite::MarshalCrossSite_TTDInflate(DynamicObject* obj)
    {TRACE_IT(33558);
        obj->MarshalCrossSite_TTDInflate();

        if(obj->GetTypeId() == TypeIds_Function)
        {TRACE_IT(33559);
            AssertMsg(obj != obj->GetScriptContext()->GetLibrary()->GetDefaultAccessorFunction(), "default accessor marshalled -- I don't think this should ever happen as it is marshalled in a special case?");
            JavascriptFunction * function = JavascriptFunction::FromVar(obj);

            //
            //TODO: what happens if the gaurd in marshal (MarshalDynamicObject) isn't true?
            //

            if(function->GetDynamicType()->GetIsShared())
            {TRACE_IT(33560);
                function->GetLibrary()->SetCrossSiteForSharedFunctionType(function);
            }
            else
            {TRACE_IT(33561);
                function->SetEntryPoint(function->GetScriptContext()->CurrentCrossSiteThunk);
            }
        }
    }
#endif

    Var CrossSite::MarshalVarInner(ScriptContext* scriptContext, __in Js::RecyclableObject* object, bool fRequestWrapper)
    {TRACE_IT(33562);
        if (scriptContext == object->GetScriptContext())
        {
            if (DoRequestWrapper(object, fRequestWrapper))
            {TRACE_IT(33563);
                // If we get here then we need to either wrap in the caller's type system or we need to return undefined.
                // VBScript will pass in the scriptContext (requestContext) from the JavascriptDispatch and this will be the
                // same as the object's script context and so we have to safely pretend this value doesn't exist.
                return scriptContext->GetLibrary()->GetUndefined();
            }
            return object;
        }

        AssertMsg(scriptContext->GetThreadContext() == object->GetScriptContext()->GetThreadContext(), "ScriptContexts should belong to same threadcontext for marshalling.");
        // In heapenum, we are traversing through the object graph to dump out the content of recyclable objects. The content
        // of the objects are duplicated to the heapenum result, and we are not storing/changing the object graph during heap enum.
        // We don't actually need to do cross site thunk here.
        if (scriptContext->GetRecycler()->IsHeapEnumInProgress())
        {TRACE_IT(33564);
            return object;
        }

#if ENABLE_TTD
        if (scriptContext->IsTTDSnapshotOrInflateInProgress())
        {TRACE_IT(33565);
            return object;
        }
#endif

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(object);
#endif
        TypeId typeId = object->GetTypeId();
        AssertMsg(typeId != TypeIds_Enumerator, "enumerator shouldn't be marshalled here");

        // At the moment the mental model for WithScopeObject Marshaling is this:
        // Are we trying to marshal a WithScopeObject in the Frame Display? - then 1) unwrap in MarshalFrameDisplay,
        // 2) marshal the wrapped object, 3) Create a new WithScopeObject in the current scriptContext and re-wrap.
        // We can avoid copying the WithScopeObject because it has no properties and never should.
        // Thus creating a new WithScopeObject per context in MarshalFrameDisplay should be kosher.
        // If it is not a FrameDisplay then we should not marshal. We can wrap cross context objects with a
        // withscopeObject in a different context. When we unwrap for property lookups and the wrapped object
        // is cross context, then we marshal the wrapped object into the current scriptContext, thus avoiding
        // the need to copy the WithScopeObject itself. Thus We don't have to handle marshaling the WithScopeObject
        // in non-FrameDisplay cases.
        AssertMsg(typeId != TypeIds_WithScopeObject, "WithScopeObject shouldn't be marshalled here");

        if (StaticType::Is(typeId))
        {TRACE_IT(33566);
            TTD_XSITE_LOG(object->GetScriptContext(), "CloneToScriptContext", object);

            return object->CloneToScriptContext(scriptContext);
        }

        if (typeId == TypeIds_ModuleRoot)
        {TRACE_IT(33567);
            RootObjectBase *moduleRoot = static_cast<RootObjectBase*>(object);
            HostObjectBase * hostObject = moduleRoot->GetHostObject();

            // When marshaling module root, all we need is the host object.
            // So, if the module root which is being marshaled has host object, marshal it.
            if (hostObject)
            {TRACE_IT(33568);
                TTD_XSITE_LOG(object->GetScriptContext(), "hostObject", hostObject);

                Var hostDispatch = hostObject->GetHostDispatchVar();
                return CrossSite::MarshalVar(scriptContext, hostDispatch);
            }
        }

        if (typeId == TypeIds_Function)
        {TRACE_IT(33569);
            if (object == object->GetScriptContext()->GetLibrary()->GetDefaultAccessorFunction() )
            {TRACE_IT(33570);
                TTD_XSITE_LOG(object->GetScriptContext(), "DefaultAccessorFunction", object);

                return scriptContext->GetLibrary()->GetDefaultAccessorFunction();
            }

            if (DoRequestWrapper(object, fRequestWrapper))
            {TRACE_IT(33571);
                TTD_XSITE_LOG(object->GetScriptContext(), "CreateWrappedExternalFunction", object);

                // Marshal as a cross-site thunk if necessary before re-wrapping in an external function thunk.
                MarshalVarInner(scriptContext, object, false);
                return scriptContext->GetLibrary()->CreateWrappedExternalFunction(static_cast<JavascriptExternalFunction*>(object));
            }
        }

        // We have an object marshaled, we need to keep track of the related script context
        // so optimization overrides can be updated as a group
        scriptContext->optimizationOverrides.Merge(&object->GetScriptContext()->optimizationOverrides);

        DynamicObject * dynamicObject = DynamicObject::FromVar(object);
        if (!dynamicObject->IsExternal())
        {TRACE_IT(33572);
            if (!dynamicObject->IsCrossSiteObject())
            {TRACE_IT(33573);
                TTD_XSITE_LOG(object->GetScriptContext(), "MarshalDynamicObjectAndPrototype", object);

                MarshalDynamicObjectAndPrototype(scriptContext, dynamicObject);
            }
        }
        else
        {
            MarshalPrototypeChain(scriptContext, dynamicObject);
            if (Js::JavascriptConversion::IsCallable(dynamicObject))
            {TRACE_IT(33574);
                TTD_XSITE_LOG(object->GetScriptContext(), "MarshalToScriptContext", object);

                dynamicObject->MarshalToScriptContext(scriptContext);
            }
        }

        return dynamicObject;
    }

    bool CrossSite::IsThunk(JavascriptMethod thunk)
    {TRACE_IT(33575);
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
        return (thunk == CrossSite::ProfileThunk || thunk == CrossSite::DefaultThunk);
#else
        return (thunk == CrossSite::DefaultThunk);
#endif
    }

#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
    Var CrossSite::ProfileThunk(RecyclableObject* callable, CallInfo callInfo, ...)
    {
        JavascriptFunction* function = JavascriptFunction::FromVar(callable);
        Assert(function->GetTypeId() == TypeIds_Function);
        Assert(function->GetEntryPoint() == CrossSite::ProfileThunk);
        RUNTIME_ARGUMENTS(args, callInfo);
        ScriptContext * scriptContext = function->GetScriptContext();
        // It is not safe to access the function body if the script context is not alive.
        scriptContext->VerifyAliveWithHostContext(!function->IsExternal(),
            scriptContext->GetThreadContext()->GetPreviousHostScriptContext());

        JavascriptMethod entryPoint;
        FunctionInfo *funcInfo = function->GetFunctionInfo();

        TTD_XSITE_LOG(callable->GetScriptContext(), "DefaultOrProfileThunk", callable);

#ifdef ENABLE_WASM
        if (AsmJsScriptFunction::IsWasmScriptFunction(function))
        {TRACE_IT(33576);
            AsmJsFunctionInfo* asmInfo = funcInfo->GetFunctionBody()->GetAsmJsFunctionInfo();
            Assert(asmInfo);
            if (asmInfo->IsWasmDeferredParse())
            {TRACE_IT(33577);
                entryPoint = WasmLibrary::WasmDeferredParseExternalThunk;
            }
            else
            {TRACE_IT(33578);
                entryPoint = Js::AsmJsExternalEntryPoint;
            }
        } else
#endif
        if (funcInfo->HasBody())
        {TRACE_IT(33579);
#if ENABLE_DEBUG_CONFIG_OPTIONS
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
            entryPoint = ScriptFunction::FromVar(function)->GetEntryPointInfo()->jsMethod;
            if (funcInfo->IsDeferred() && scriptContext->IsProfiling())
            {TRACE_IT(33580);
                // if the current entrypoint is deferred parse we need to update it appropriately for the profiler mode.
                entryPoint = Js::ScriptContext::GetProfileModeThunk(entryPoint);
            }
            OUTPUT_TRACE(Js::ScriptProfilerPhase, _u("CrossSite::ProfileThunk FunctionNumber : %s, Entrypoint : 0x%08X\n"), funcInfo->GetFunctionProxy()->GetDebugNumberSet(debugStringBuffer), entryPoint);
        }
        else
        {TRACE_IT(33581);
            entryPoint = ProfileEntryThunk;
        }


        return CommonThunk(function, entryPoint, args);
    }
#endif

    Var CrossSite::DefaultThunk(RecyclableObject* callable, CallInfo callInfo, ...)
    {
        JavascriptFunction* function = JavascriptFunction::FromVar(callable);
        Assert(function->GetTypeId() == TypeIds_Function);
        Assert(function->GetEntryPoint() == CrossSite::DefaultThunk);
        RUNTIME_ARGUMENTS(args, callInfo);

        // It is not safe to access the function body if the script context is not alive.
        function->GetScriptContext()->VerifyAliveWithHostContext(!function->IsExternal(),
            ThreadContext::GetContextForCurrentThread()->GetPreviousHostScriptContext());

        JavascriptMethod entryPoint;
        FunctionInfo *funcInfo = function->GetFunctionInfo();

        TTD_XSITE_LOG(callable->GetScriptContext(), "DefaultOrProfileThunk", callable);

        if (funcInfo->HasBody())
        {TRACE_IT(33582);
#ifdef ASMJS_PLAT
            if (funcInfo->GetFunctionProxy()->IsFunctionBody() &&
                funcInfo->GetFunctionBody()->GetIsAsmJsFunction())
            {TRACE_IT(33583);
#ifdef ENABLE_WASM
                AsmJsFunctionInfo* asmInfo = funcInfo->GetFunctionBody()->GetAsmJsFunctionInfo();
                if (asmInfo && asmInfo->IsWasmDeferredParse())
                {TRACE_IT(33584);
                    entryPoint = WasmLibrary::WasmDeferredParseExternalThunk;
                }
                else
#endif
                {TRACE_IT(33585);
                    entryPoint = Js::AsmJsExternalEntryPoint;
                }
            }
            else
#endif
            {TRACE_IT(33586);
                entryPoint = ScriptFunction::FromVar(function)->GetEntryPointInfo()->jsMethod;
            }
        }
        else
        {TRACE_IT(33587);
            entryPoint = funcInfo->GetOriginalEntryPoint();
        }
        return CommonThunk(function, entryPoint, args);
    }

    Var CrossSite::CommonThunk(RecyclableObject* recyclableObject, JavascriptMethod entryPoint, Arguments args)
    {TRACE_IT(33588);
        DynamicObject* function = DynamicObject::FromVar(recyclableObject);
        ScriptContext* targetScriptContext = function->GetScriptContext();
        Assert(!targetScriptContext->IsClosed());
        Assert(function->IsExternal() || function->IsCrossSiteObject());
        Assert(targetScriptContext->GetThreadContext()->IsScriptActive());

        HostScriptContext* calleeHostScriptContext = targetScriptContext->GetHostScriptContext();
        HostScriptContext* callerHostScriptContext = targetScriptContext->GetThreadContext()->GetPreviousHostScriptContext();

        if (callerHostScriptContext == calleeHostScriptContext || (callerHostScriptContext == nullptr && !calleeHostScriptContext->HasCaller()))
        {TRACE_IT(33589);
            return JavascriptFunction::CallFunction<true>(function, entryPoint, args);
        }

#if DBG_DUMP || defined(PROFILE_EXEC) || defined(PROFILE_MEM)
        calleeHostScriptContext->EnsureParentInfo(callerHostScriptContext->GetScriptContext());
#endif

        TTD_XSITE_LOG(recyclableObject->GetScriptContext(), "CommonThunk -- Pass Through", recyclableObject);

        uint i = 0;
        if (args.Values[0] == nullptr)
        {TRACE_IT(33590);
            i = 1;
            Assert(args.Info.Flags & CallFlags_New);
            Assert(JavascriptFunction::Is(function) && JavascriptFunction::FromVar(function)->GetFunctionInfo()->GetAttributes() & FunctionInfo::SkipDefaultNewObject);
        }
        uint count = args.Info.Count;
        if ((args.Info.Flags & CallFlags_ExtraArg) && ((args.Info.Flags & CallFlags_NewTarget) == 0))
        {TRACE_IT(33591);
            // The final eval arg is a frame display that needs to be marshaled specially.
            args.Values[count-1] = CrossSite::MarshalFrameDisplay(targetScriptContext, (FrameDisplay*)args.Values[count-1]);
            count--;
        }
        for (; i < count; i++)
        {TRACE_IT(33592);
            args.Values[i] = CrossSite::MarshalVar(targetScriptContext, args.Values[i]);
        }

#if ENABLE_NATIVE_CODEGEN
        CheckCodeGenFunction checkCodeGenFunction = GetCheckCodeGenFunction(entryPoint);
        if (checkCodeGenFunction != nullptr)
        {TRACE_IT(33593);
            ScriptFunction* callFunc = ScriptFunction::FromVar(function);
            entryPoint = checkCodeGenFunction(callFunc);
            Assert(CrossSite::IsThunk(function->GetEntryPoint()));
        }
#endif

        // We need to setup the caller chain when we go across script site boundary. Property access
        // is OK, and we need to let host know who the caller is when a call is from another script site.
        // CrossSiteObject is the natural place but it is in the target site. We build up the site
        // chain through PushDispatchExCaller/PopDispatchExCaller, and we call SetCaller in the target site
        // to indicate who the caller is. We first need to get the site from the previously pushed site
        // and set that as the caller for current call, and push a new DispatchExCaller for future calls
        // off this site. GetDispatchExCaller and ReleaseDispatchExCaller is used to get the current caller.
        // currentDispatchExCaller is cached to avoid multiple allocations.
        IUnknown* sourceCaller = nullptr, *previousSourceCaller = nullptr;
        HRESULT hr = NOERROR;
        Var result = nullptr;
        BOOL wasDispatchExCallerPushed = FALSE, wasCallerSet = FALSE;

        TryFinally([&]()
        {
            hr = callerHostScriptContext->GetDispatchExCaller((void**)&sourceCaller);

            if (SUCCEEDED(hr))
            {TRACE_IT(33594);
                hr = calleeHostScriptContext->SetCaller((IUnknown*)sourceCaller, (IUnknown**)&previousSourceCaller);
            }

            if (SUCCEEDED(hr))
            {TRACE_IT(33595);
                wasCallerSet = TRUE;
                hr = calleeHostScriptContext->PushHostScriptContext();
            }
            if (FAILED(hr))
            {TRACE_IT(33596);
                // CONSIDER: Should this be callerScriptContext if we failed?
                JavascriptError::MapAndThrowError(targetScriptContext, hr);
            }
            wasDispatchExCallerPushed = TRUE;

            result = JavascriptFunction::CallFunction<true>(function, entryPoint, args);
            ScriptContext* callerScriptContext = callerHostScriptContext->GetScriptContext();
            result = CrossSite::MarshalVar(callerScriptContext, result);
        },
        [&](bool hasException)
        {TRACE_IT(33597);
            if (sourceCaller != nullptr)
            {TRACE_IT(33598);
                callerHostScriptContext->ReleaseDispatchExCaller(sourceCaller);
            }
            IUnknown* originalCaller = nullptr;
            if (wasDispatchExCallerPushed)
            {TRACE_IT(33599);
                calleeHostScriptContext->PopHostScriptContext();
            }
            if (wasCallerSet)
            {TRACE_IT(33600);
                calleeHostScriptContext->SetCaller(previousSourceCaller, &originalCaller);
                if (previousSourceCaller)
                {TRACE_IT(33601);
                    previousSourceCaller->Release();
                }
                if (originalCaller)
                {TRACE_IT(33602);
                    originalCaller->Release();
                }
            }
        });
        Assert(result != nullptr);
        return result;
    }

    // For prototype chain to install cross-site thunk.
    // When we change prototype using __proto__, those prototypes might not have cross-site thunks
    // installed even though the CEO is accessed from a different context. During ChangePrototype time
    // we don't really know where the requestContext is.
    // Force installing cross-site thunk for all prototype changes. It's a relatively less frequently used
    // scenario.
    void CrossSite::ForceCrossSiteThunkOnPrototypeChain(RecyclableObject* object)
    {TRACE_IT(33603);
        if (TaggedNumber::Is(object))
        {TRACE_IT(33604);
            return;
        }
        while (DynamicType::Is(object->GetTypeId()) && !JavascriptProxy::Is(object))
        {TRACE_IT(33605);
            DynamicObject* dynamicObject = DynamicObject::FromVar(object);
            if (!dynamicObject->IsCrossSiteObject() && !dynamicObject->IsExternal())
            {TRACE_IT(33606);
                // force to install cross-site thunk on prototype objects.
                dynamicObject->MarshalToScriptContext(nullptr);
            }
            object = object->GetPrototype();
        }
        return;

    }
};
