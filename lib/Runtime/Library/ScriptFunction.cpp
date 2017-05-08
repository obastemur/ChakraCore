//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ScriptFunctionBase::ScriptFunctionBase(DynamicType * type) :
        JavascriptFunction(type)
    {TRACE_IT(62862);}

    ScriptFunctionBase::ScriptFunctionBase(DynamicType * type, FunctionInfo * functionInfo) :
        JavascriptFunction(type, functionInfo)
    {TRACE_IT(62863);}

    bool ScriptFunctionBase::Is(Var func)
    {TRACE_IT(62864);
        if (JavascriptFunction::Is(func))
        {TRACE_IT(62865);
            JavascriptFunction *function = JavascriptFunction::FromVar(func);
            return ScriptFunction::Test(function) || JavascriptGeneratorFunction::Test(function)
                || JavascriptAsyncFunction::Test(function);
        }

        return false;
    }

    ScriptFunctionBase * ScriptFunctionBase::FromVar(Var func)
    {TRACE_IT(62866);
        Assert(ScriptFunctionBase::Is(func));
        return reinterpret_cast<ScriptFunctionBase *>(func);
    }

    ScriptFunction::ScriptFunction(DynamicType * type) :
        ScriptFunctionBase(type), environment((FrameDisplay*)&NullFrameDisplay),
        cachedScopeObj(nullptr), hasInlineCaches(false), hasSuperReference(false), homeObj(nullptr),
        computedNameVar(nullptr), isActiveScript(false)
    {TRACE_IT(62867);}

    ScriptFunction::ScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType)
        : ScriptFunctionBase(deferredPrototypeType, proxy->GetFunctionInfo()),
        environment((FrameDisplay*)&NullFrameDisplay), cachedScopeObj(nullptr), homeObj(nullptr),
        hasInlineCaches(false), hasSuperReference(false), isActiveScript(false), computedNameVar(nullptr)
    {TRACE_IT(62868);
        Assert(proxy->GetFunctionInfo()->GetFunctionProxy() == proxy);
        Assert(proxy->EnsureDeferredPrototypeType() == deferredPrototypeType);
        DebugOnly(VerifyEntryPoint());

#if ENABLE_NATIVE_CODEGEN
#ifdef BGJIT_STATS
        if (!proxy->IsDeferred())
        {TRACE_IT(62869);
            FunctionBody* body = proxy->GetFunctionBody();
            if(!body->GetNativeEntryPointUsed() &&
                body->GetDefaultFunctionEntryPointInfo()->IsCodeGenDone())
            {TRACE_IT(62870);
                MemoryBarrier();

                type->GetScriptContext()->jitCodeUsed += body->GetByteCodeCount();
                type->GetScriptContext()->funcJitCodeUsed++;

                body->SetNativeEntryPointUsed(true);
            }
        }
#endif
#endif
    }

    ScriptFunction * ScriptFunction::OP_NewScFunc(FrameDisplay *environment, FunctionInfoPtrPtr infoRef)
    {TRACE_IT(62871);
        AssertMsg(infoRef!= nullptr, "BYTE-CODE VERIFY: Must specify a valid function to create");
        FunctionProxy* functionProxy = (*infoRef)->GetFunctionProxy();
        AssertMsg(functionProxy!= nullptr, "BYTE-CODE VERIFY: Must specify a valid function to create");

        ScriptContext* scriptContext = functionProxy->GetScriptContext();

        bool hasSuperReference = functionProxy->HasSuperReference();

        if (functionProxy->IsFunctionBody() && functionProxy->GetFunctionBody()->GetInlineCachesOnFunctionObject())
        {TRACE_IT(62872);
            Js::FunctionBody * functionBody = functionProxy->GetFunctionBody();
            ScriptFunctionWithInlineCache* pfuncScriptWithInlineCache = scriptContext->GetLibrary()->CreateScriptFunctionWithInlineCache(functionProxy);
            pfuncScriptWithInlineCache->SetEnvironment(environment);
            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(pfuncScriptWithInlineCache, EtwTrace::GetFunctionId(functionProxy)));

            Assert(functionBody->GetInlineCacheCount() + functionBody->GetIsInstInlineCacheCount());

            if (functionBody->GetIsFirstFunctionObject())
            {TRACE_IT(62873);
                // point the inline caches of the first function object to those on the function body.
                pfuncScriptWithInlineCache->SetInlineCachesFromFunctionBody();
                functionBody->SetIsNotFirstFunctionObject();
            }
            else
            {TRACE_IT(62874);
                // allocate inline cache for this function object
                pfuncScriptWithInlineCache->CreateInlineCache();
            }

            pfuncScriptWithInlineCache->SetHasSuperReference(hasSuperReference);

            if (PHASE_TRACE1(Js::ScriptFunctionWithInlineCachePhase))
            {TRACE_IT(62875);
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                Output::Print(_u("Function object with inline cache: function number: (%s)\tfunction name: %s\n"),
                    functionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName());
                Output::Flush();
            }
            return pfuncScriptWithInlineCache;
        }
        else if(functionProxy->IsFunctionBody() && functionProxy->GetFunctionBody()->GetIsAsmJsFunction())
        {TRACE_IT(62876);
            AsmJsScriptFunction* asmJsFunc = scriptContext->GetLibrary()->CreateAsmJsScriptFunction(functionProxy);
            asmJsFunc->SetEnvironment(environment);

            Assert(!hasSuperReference);
            asmJsFunc->SetHasSuperReference(hasSuperReference);

            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(asmJsFunc, EtwTrace::GetFunctionId(functionProxy)));

            return asmJsFunc;
        }
        else
        {TRACE_IT(62877);
            ScriptFunction* pfuncScript = scriptContext->GetLibrary()->CreateScriptFunction(functionProxy);
            pfuncScript->SetEnvironment(environment);

            pfuncScript->SetHasSuperReference(hasSuperReference);

            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(pfuncScript, EtwTrace::GetFunctionId(functionProxy)));

            return pfuncScript;
        }
    }

    void ScriptFunction::SetEnvironment(FrameDisplay * environment)
    {TRACE_IT(62878);
        //Assert(ThreadContext::IsOnStack(this) || !ThreadContext::IsOnStack(environment));
        this->environment = environment;
    }

    void ScriptFunction::InvalidateCachedScopeChain()
    {TRACE_IT(62879);
        // Note: Currently this helper assumes that we're in an eval-class case
        // where all the contents of the closure environment are dynamic objects.
        // Invalidating scopes that are raw slot arrays, etc., will have to be done
        // directly in byte code.

        // A function nested within this one has escaped.
        // Invalidate our own cached scope object, and walk the closure environment
        // doing this same.
        if (this->cachedScopeObj)
        {TRACE_IT(62880);
            this->cachedScopeObj->InvalidateCachedScope();
        }
        FrameDisplay *pDisplay = this->environment;
        uint length = (uint)pDisplay->GetLength();
        for (uint i = 0; i < length; i++)
        {TRACE_IT(62881);
            Var scope = pDisplay->GetItem(i);
            RecyclableObject *scopeObj = RecyclableObject::FromVar(scope);
            scopeObj->InvalidateCachedScope();
        }
    }

    bool ScriptFunction::Is(Var func)
    {TRACE_IT(62882);
        return JavascriptFunction::Is(func) && JavascriptFunction::FromVar(func)->GetFunctionInfo()->HasBody();
    }

    ScriptFunction * ScriptFunction::FromVar(Var func)
    {TRACE_IT(62883);
        Assert(ScriptFunction::Is(func));
        return reinterpret_cast<ScriptFunction *>(func);
    }

    ProxyEntryPointInfo * ScriptFunction::GetEntryPointInfo() const
    {TRACE_IT(62884);
        return this->GetScriptFunctionType()->GetEntryPointInfo();
    }

    ScriptFunctionType * ScriptFunction::GetScriptFunctionType() const
    {TRACE_IT(62885);
        return (ScriptFunctionType *)GetDynamicType();
    }

    ScriptFunctionType * ScriptFunction::DuplicateType()
    {TRACE_IT(62886);
        ScriptFunctionType* type = RecyclerNew(this->GetScriptContext()->GetRecycler(),
            ScriptFunctionType, this->GetScriptFunctionType());

        this->GetFunctionProxy()->RegisterFunctionObjectType(type);

        return type;
    }

    uint32 ScriptFunction::GetFrameHeight(FunctionEntryPointInfo* entryPointInfo) const
    {TRACE_IT(62887);
        Assert(this->GetFunctionBody() != nullptr);

        return this->GetFunctionBody()->GetFrameHeight(entryPointInfo);
    }

    bool ScriptFunction::HasFunctionBody()
    {TRACE_IT(62888);
        // for asmjs we want to first check if the FunctionObject has a function body. Check that the function is not deferred
        return  !this->GetFunctionInfo()->IsDeferredParseFunction() && !this->GetFunctionInfo()->IsDeferredDeserializeFunction() && GetParseableFunctionInfo()->IsFunctionParsed();
    }

    void ScriptFunction::ChangeEntryPoint(ProxyEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint)
    {TRACE_IT(62889);
        Assert(entryPoint != nullptr);
        Assert(this->GetTypeId() == TypeIds_Function);
#if ENABLE_NATIVE_CODEGEN
        Assert(!IsCrossSiteObject() || entryPoint != (Js::JavascriptMethod)checkCodeGenThunk);
#else
        Assert(!IsCrossSiteObject());
#endif

        Assert((entryPointInfo != nullptr && this->GetFunctionProxy() != nullptr));
        if (this->GetEntryPoint() == entryPoint && this->GetScriptFunctionType()->GetEntryPointInfo() == entryPointInfo)
        {TRACE_IT(62890);
            return;
        }

        bool isAsmJS = false;
        if (HasFunctionBody())
        {TRACE_IT(62891);
            isAsmJS = this->GetFunctionBody()->GetIsAsmjsMode();
        }

        // ASMJS:- for asmjs we don't need to update the entry point here as it updates the types entry point
        if (!isAsmJS)
        {TRACE_IT(62892);
            // We can't go from cross-site to non-cross-site. Update only in the non-cross site case
            if (!CrossSite::IsThunk(this->GetEntryPoint()))
            {TRACE_IT(62893);
                this->SetEntryPoint(entryPoint);
            }
        }
        // instead update the address in the function entrypoint info
        else
        {TRACE_IT(62894);
            entryPointInfo->jsMethod = entryPoint;
        }

        if (!isAsmJS)
        {TRACE_IT(62895);
            ProxyEntryPointInfo* oldEntryPointInfo = this->GetScriptFunctionType()->GetEntryPointInfo();
            if (oldEntryPointInfo
                && oldEntryPointInfo != entryPointInfo
                && oldEntryPointInfo->SupportsExpiration())
            {TRACE_IT(62896);
                // The old entry point could be executing so we need root it to make sure
                // it isn't prematurely collected. The rooting is done by queuing it up on the threadContext
                ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();

                threadContext->QueueFreeOldEntryPointInfoIfInScript((FunctionEntryPointInfo*)oldEntryPointInfo);
            }
        }

        this->GetScriptFunctionType()->SetEntryPointInfo(entryPointInfo);
    }

    FunctionProxy * ScriptFunction::GetFunctionProxy() const
    {TRACE_IT(62897);
        Assert(this->functionInfo->HasBody());
        return this->functionInfo->GetFunctionProxy();
    }
    JavascriptMethod ScriptFunction::UpdateUndeferredBody(FunctionBody* newFunctionInfo)
    {TRACE_IT(62898);
        // Update deferred parsed/serialized function to the real function body
        Assert(this->functionInfo->HasBody());
        Assert(this->functionInfo->GetFunctionBody() == newFunctionInfo);
        Assert(!newFunctionInfo->IsDeferred());

        DynamicType * type = this->GetDynamicType();

        // If the type is shared, it must be the shared one in the old function proxy

        this->functionInfo = newFunctionInfo->GetFunctionInfo();

        if (type->GetIsShared())
        {TRACE_IT(62899);
            // the type is still shared, we can't modify it, just migrate to the shared one in the function body
            this->ReplaceType(newFunctionInfo->EnsureDeferredPrototypeType());
        }

        // The type has change from the default, it is not share, just use that one.
        JavascriptMethod directEntryPoint = newFunctionInfo->GetDirectEntryPoint(newFunctionInfo->GetDefaultEntryPointInfo());
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
        Assert(directEntryPoint != DefaultDeferredParsingThunk
            && directEntryPoint != ProfileDeferredParsingThunk);
#else
        Assert(directEntryPoint != DefaultDeferredParsingThunk);
#endif

        Js::FunctionEntryPointInfo* defaultEntryPointInfo = newFunctionInfo->GetDefaultFunctionEntryPointInfo();
        JavascriptMethod thunkEntryPoint = this->UpdateThunkEntryPoint(defaultEntryPointInfo, directEntryPoint);

        this->GetScriptFunctionType()->SetEntryPointInfo(defaultEntryPointInfo);

        return thunkEntryPoint;

    }

    JavascriptMethod ScriptFunction::UpdateThunkEntryPoint(FunctionEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint)
    {TRACE_IT(62900);
        this->ChangeEntryPoint(entryPointInfo, entryPoint);

        if (!CrossSite::IsThunk(this->GetEntryPoint()))
        {TRACE_IT(62901);
            return entryPoint;
        }

        // We already pass through the cross site thunk, which would have called the profile thunk already if necessary
        // So just call the original entry point if our direct entry is the profile entry thunk
        // Otherwise, call the directEntryPoint which may have additional processing to do (e.g. ensure dynamic profile)
        Assert(this->IsCrossSiteObject());
        if (entryPoint != ProfileEntryThunk)
        {TRACE_IT(62902);
            return entryPoint;
        }
        // Based on the comment below, this shouldn't be a defer deserialization function as it would have a deferred thunk
        FunctionBody * functionBody = this->GetFunctionBody();
        // The original entry point should be an interpreter thunk or the native entry point;
        Assert(functionBody->IsInterpreterThunk() || functionBody->IsNativeOriginalEntryPoint());
        return functionBody->GetOriginalEntryPoint();
    }

    bool ScriptFunction::IsNewEntryPointAvailable()
    {TRACE_IT(62903);
        Js::FunctionEntryPointInfo *const defaultEntryPointInfo = this->GetFunctionBody()->GetDefaultFunctionEntryPointInfo();
        JavascriptMethod defaultEntryPoint = this->GetFunctionBody()->GetDirectEntryPoint(defaultEntryPointInfo);

        return this->GetEntryPoint() != defaultEntryPoint;
    }

    Var ScriptFunction::GetSourceString() const
    {TRACE_IT(62904);
        return this->GetFunctionProxy()->EnsureDeserialized()->GetCachedSourceString();
    }

    Var ScriptFunction::FormatToString(JavascriptString* inputString)
    {TRACE_IT(62905);
        FunctionProxy* proxy = this->GetFunctionProxy();
        ParseableFunctionInfo * pFuncBody = proxy->EnsureDeserialized();
        const char16 * inputStr = inputString->GetString();
        const char16 * paramStr = wcschr(inputStr, _u('('));

        if (paramStr == nullptr || wcscmp(pFuncBody->GetDisplayName(), Js::Constants::EvalCode) == 0)
        {TRACE_IT(62906);
            Assert(pFuncBody->IsEval());
            return inputString;
        }

        ScriptContext* scriptContext = this->GetScriptContext();
        JavascriptLibrary* library = scriptContext->GetLibrary();
        bool isClassMethod = this->GetFunctionInfo()->IsClassMethod() || this->GetFunctionInfo()->IsClassConstructor();

        JavascriptString* prefixString = nullptr;
        uint prefixStringLength = 0;
        const char16* name = _u("");
        charcount_t nameLength = 0;
        Var returnStr = nullptr;

        if (!isClassMethod)
        {TRACE_IT(62907);
            prefixString = library->GetFunctionPrefixString();
            if (pFuncBody->IsGenerator())
            {TRACE_IT(62908);
                prefixString = library->GetGeneratorFunctionPrefixString();
            }
            else if (pFuncBody->IsAsync())
            {TRACE_IT(62909);
                prefixString = library->GetAsyncFunctionPrefixString();
            }
            prefixStringLength = prefixString->GetLength();

            if (pFuncBody->GetIsAccessor())
            {TRACE_IT(62910);
                name = pFuncBody->GetShortDisplayName(&nameLength);

            }
            else if (pFuncBody->GetIsDeclaration() || pFuncBody->GetIsNamedFunctionExpression())
            {TRACE_IT(62911);
                name = pFuncBody->GetDisplayName();
                nameLength = pFuncBody->GetDisplayNameLength();
                if (name == Js::Constants::FunctionCode)
                {TRACE_IT(62912);
                    name = Js::Constants::Anonymous;
                    nameLength = Js::Constants::AnonymousLength;
                }

            }
        }
        else
        {TRACE_IT(62913);

            if (this->GetFunctionInfo()->IsClassConstructor())
            {TRACE_IT(62914);
                name = _u("constructor");
                nameLength = _countof(_u("constructor")) -1; //subtract off \0
            }
            else
            {TRACE_IT(62915);
                name = pFuncBody->GetShortDisplayName(&nameLength); //strip off prototype.
            }
        }

        ENTER_PINNED_SCOPE(JavascriptString, computedName);
        computedName = this->GetComputedName();
        if (computedName != nullptr)
        {TRACE_IT(62916);
            prefixString = nullptr;
            prefixStringLength = 0;
            name = computedName->GetString();
            nameLength = computedName->GetLength();
        }

        uint functionBodyLength = inputString->GetLength() - ((uint)(paramStr - inputStr));
        size_t totalLength = prefixStringLength + functionBodyLength + nameLength;

        if (!IsValidCharCount(totalLength))
        {TRACE_IT(62917);
            // We throw here because computed property names are evaluated at runtime and
            // thus are not a subset string of function body source (parameter inputString).
            // For all other cases totalLength <= inputString->GetLength().
            JavascriptExceptionOperators::ThrowOutOfMemory(this->GetScriptContext());
        }

        char16 * funcBodyStr = RecyclerNewArrayLeaf(this->GetScriptContext()->GetRecycler(), char16, totalLength);
        char16 * funcBodyStrStart = funcBodyStr;
        if (prefixString != nullptr)
        {
            js_wmemcpy_s(funcBodyStr, prefixStringLength, prefixString->GetString(), prefixStringLength);
            funcBodyStrStart += prefixStringLength;
        }

        js_wmemcpy_s(funcBodyStrStart, nameLength, name, nameLength);
        funcBodyStrStart = funcBodyStrStart + nameLength;
        js_wmemcpy_s(funcBodyStrStart, functionBodyLength, paramStr, functionBodyLength);

        returnStr = LiteralString::NewCopyBuffer(funcBodyStr, (charcount_t)totalLength, scriptContext);

        LEAVE_PINNED_SCOPE();

        return returnStr;
    }

    Var ScriptFunction::EnsureSourceString()
    {TRACE_IT(62918);
        // The function may be defer serialize, need to be deserialized
        FunctionProxy* proxy = this->GetFunctionProxy();
        ParseableFunctionInfo * pFuncBody = proxy->EnsureDeserialized();
        Var cachedSourceString = pFuncBody->GetCachedSourceString();
        if (cachedSourceString != nullptr)
        {TRACE_IT(62919);
            return cachedSourceString;
        }

        ScriptContext * scriptContext = this->GetScriptContext();

        //Library code should behave the same way as RuntimeFunctions
        Utf8SourceInfo* source = pFuncBody->GetUtf8SourceInfo();
        if ((source != nullptr && source->GetIsLibraryCode())
#ifdef ENABLE_WASM
            || (pFuncBody->IsWasmFunction())
#endif
            )
        {TRACE_IT(62920);
            //Don't display if it is anonymous function
            charcount_t displayNameLength = 0;
            PCWSTR displayName = pFuncBody->GetShortDisplayName(&displayNameLength);
            cachedSourceString = JavascriptFunction::GetLibraryCodeDisplayString(scriptContext, displayName);
        }
        else if (!pFuncBody->GetUtf8SourceInfo()->GetIsXDomain()
            // To avoid backward compat issue, we will not give out sourceString for function if it is called from
            // window.onerror trying to retrieve arguments.callee.caller.
            && !(pFuncBody->GetUtf8SourceInfo()->GetIsXDomainString() && scriptContext->GetThreadContext()->HasUnhandledException())
            )
        {TRACE_IT(62921);
            // Decode UTF8 into Unicode
            // Consider: Should we have a JavascriptUtf8Substring class which defers decoding
            // until it's needed?

            charcount_t cch = pFuncBody->LengthInChars();
            size_t cbLength = pFuncBody->LengthInBytes();
            LPCUTF8 pbStart = pFuncBody->GetSource(_u("ScriptFunction::EnsureSourceString"));
            BufferStringBuilder builder(cch, scriptContext);
            utf8::DecodeOptions options = pFuncBody->GetUtf8SourceInfo()->IsCesu8() ? utf8::doAllowThreeByteSurrogates : utf8::doDefault;
            utf8::DecodeUnitsInto(builder.DangerousGetWritableBuffer(), pbStart, pbStart + cbLength, options);
            if (pFuncBody->IsLambda() || isActiveScript || this->GetFunctionInfo()->IsClassConstructor()
#ifdef ENABLE_PROJECTION
                || scriptContext->GetConfig()->IsWinRTEnabled()
#endif
                )
            {TRACE_IT(62922);
                cachedSourceString = builder.ToString();
            }
            else
            {TRACE_IT(62923);
                cachedSourceString = FormatToString(builder.ToString());
            }
        }
        else
        {TRACE_IT(62924);
            cachedSourceString = scriptContext->GetLibrary()->GetXDomainFunctionDisplayString();
        }
        Assert(cachedSourceString != nullptr);
        pFuncBody->SetCachedSourceString(cachedSourceString);
        return cachedSourceString;
    }

#if ENABLE_TTD
    void ScriptFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(62925);
        Js::FunctionBody* fb = TTD::JsSupport::ForceAndGetFunctionBody(this->GetParseableFunctionInfo());
        extractor->MarkFunctionBody(fb);

        Js::FrameDisplay* environment = this->GetEnvironment();
        if(environment->GetLength() != 0)
        {TRACE_IT(62926);
            extractor->MarkScriptFunctionScopeInfo(environment);
        }

        if(this->cachedScopeObj != nullptr)
        {TRACE_IT(62927);
            extractor->MarkVisitVar(this->cachedScopeObj);
        }

        if(this->homeObj != nullptr)
        {TRACE_IT(62928);
            extractor->MarkVisitVar(this->homeObj);
        }

        if(this->computedNameVar != nullptr)
        {TRACE_IT(62929);
            extractor->MarkVisitVar(this->computedNameVar);
        }
    }

    void ScriptFunction::ProcessCorePaths()
    {TRACE_IT(62930);
        TTD::RuntimeContextInfo* rctxInfo = this->GetScriptContext()->TTDWellKnownInfo;

        //do the body path mark
        Js::FunctionBody* fb = TTD::JsSupport::ForceAndGetFunctionBody(this->GetParseableFunctionInfo());
        rctxInfo->EnqueueNewFunctionBodyObject(this, fb, _u("!fbody"));

        Js::FrameDisplay* environment = this->GetEnvironment();
        uint32 scopeCount = environment->GetLength();

        for(uint32 i = 0; i < scopeCount; ++i)
        {TRACE_IT(62931);
            TTD::UtilSupport::TTAutoString scopePathString;
            rctxInfo->BuildEnvironmentIndexBuffer(i, scopePathString);

            void* scope = environment->GetItem(i);
            switch(environment->GetScopeType(scope))
            {
            case Js::ScopeType::ScopeType_ActivationObject:
            case Js::ScopeType::ScopeType_WithScope:
            {TRACE_IT(62932);
                rctxInfo->EnqueueNewPathVarAsNeeded(this, (Js::Var)scope, scopePathString.GetStrValue());
                break;
            }
            case Js::ScopeType::ScopeType_SlotArray:
            {TRACE_IT(62933);
                Js::ScopeSlots slotArray = (Js::Var*)scope;
                uint slotArrayCount = slotArray.GetCount();

                //get the function body associated with the scope
                if(slotArray.IsFunctionScopeSlotArray())
                {TRACE_IT(62934);
                    rctxInfo->EnqueueNewFunctionBodyObject(this, slotArray.GetFunctionInfo()->GetFunctionBody(), scopePathString.GetStrValue());
                }
                else
                {TRACE_IT(62935);
                    rctxInfo->AddWellKnownDebuggerScopePath(this, slotArray.GetDebuggerScope(), i);
                }

                for(uint j = 0; j < slotArrayCount; j++)
                {TRACE_IT(62936);
                    Js::Var sval = slotArray.Get(j);

                    TTD::UtilSupport::TTAutoString slotPathString;
                    rctxInfo->BuildEnvironmentIndexAndSlotBuffer(i, j, slotPathString);

                    rctxInfo->EnqueueNewPathVarAsNeeded(this, sval, slotPathString.GetStrValue());
                }

                break;
            }
            default:
                TTDAssert(false, "Unknown scope kind");
            }
        }

        if(this->cachedScopeObj != nullptr)
        {TRACE_IT(62937);
            this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->cachedScopeObj, _u("_cachedScopeObj"));
        }

        if(this->homeObj != nullptr)
        {TRACE_IT(62938);
            this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->homeObj, _u("_homeObj"));
        }
    }

    TTD::NSSnapObjects::SnapObjectType ScriptFunction::GetSnapTag_TTD() const
    {TRACE_IT(62939);
        return TTD::NSSnapObjects::SnapObjectType::SnapScriptFunctionObject;
    }

    void ScriptFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(62940);
        TTDAssert(this->GetFunctionInfo() != nullptr, "We are only doing this for functions with ParseableFunctionInfo.");

        TTD::NSSnapObjects::SnapScriptFunctionInfo* ssfi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapScriptFunctionInfo>();
        Js::FunctionBody* fb = TTD::JsSupport::ForceAndGetFunctionBody(this->GetParseableFunctionInfo());

        alloc.CopyNullTermStringInto(fb->GetDisplayName(), ssfi->DebugFunctionName);

        ssfi->BodyRefId = TTD_CONVERT_FUNCTIONBODY_TO_PTR_ID(fb);

        Js::FrameDisplay* environment = this->GetEnvironment();
        ssfi->ScopeId = TTD_INVALID_PTR_ID;
        if(environment->GetLength() != 0)
        {TRACE_IT(62941);
            ssfi->ScopeId = TTD_CONVERT_SCOPE_TO_PTR_ID(environment);
        }

        ssfi->CachedScopeObjId = TTD_INVALID_PTR_ID;
        if(this->cachedScopeObj != nullptr)
        {TRACE_IT(62942);
            ssfi->CachedScopeObjId = TTD_CONVERT_VAR_TO_PTR_ID(this->cachedScopeObj);
        }

        ssfi->HomeObjId = TTD_INVALID_PTR_ID;
        if(this->homeObj != nullptr)
        {TRACE_IT(62943);
            ssfi->HomeObjId = TTD_CONVERT_VAR_TO_PTR_ID(this->homeObj);
        }

        ssfi->ComputedNameInfo = TTD_CONVERT_JSVAR_TO_TTDVAR(this->computedNameVar);

        ssfi->HasSuperReference = this->hasSuperReference;

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapScriptFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapScriptFunctionObject>(objData, ssfi);
    }
#endif

    AsmJsScriptFunction::AsmJsScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType) :
        ScriptFunction(proxy, deferredPrototypeType), m_moduleMemory(nullptr)
    {TRACE_IT(62944);}

    AsmJsScriptFunction::AsmJsScriptFunction(DynamicType * type) :
        ScriptFunction(type), m_moduleMemory(nullptr)
    {TRACE_IT(62945);}

    bool AsmJsScriptFunction::Is(Var func)
    {TRACE_IT(62946);
        return ScriptFunction::Is(func) &&
            ScriptFunction::FromVar(func)->HasFunctionBody() &&
            ScriptFunction::FromVar(func)->GetFunctionBody()->GetIsAsmJsFunction();
    }

    bool AsmJsScriptFunction::IsWasmScriptFunction(Var func)
    {TRACE_IT(62947);
        return ScriptFunction::Is(func) &&
            ScriptFunction::FromVar(func)->HasFunctionBody() &&
            ScriptFunction::FromVar(func)->GetFunctionBody()->IsWasmFunction();
    }

    AsmJsScriptFunction* AsmJsScriptFunction::FromVar(Var func)
    {TRACE_IT(62948);
        Assert(AsmJsScriptFunction::Is(func));
        return reinterpret_cast<AsmJsScriptFunction *>(func);
    }

    ScriptFunctionWithInlineCache::ScriptFunctionWithInlineCache(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType) :
        ScriptFunction(proxy, deferredPrototypeType), hasOwnInlineCaches(false)
    {TRACE_IT(62949);}

    ScriptFunctionWithInlineCache::ScriptFunctionWithInlineCache(DynamicType * type) :
        ScriptFunction(type), hasOwnInlineCaches(false)
    {TRACE_IT(62950);}

    bool ScriptFunctionWithInlineCache::Is(Var func)
    {TRACE_IT(62951);
        return ScriptFunction::Is(func) && ScriptFunction::FromVar(func)->GetHasInlineCaches();
    }

    ScriptFunctionWithInlineCache* ScriptFunctionWithInlineCache::FromVar(Var func)
    {TRACE_IT(62952);
        Assert(ScriptFunctionWithInlineCache::Is(func));
        return reinterpret_cast<ScriptFunctionWithInlineCache *>(func);
    }

    InlineCache * ScriptFunctionWithInlineCache::GetInlineCache(uint index)
    {TRACE_IT(62953);
        Assert(this->m_inlineCaches != nullptr);
        Assert(index < this->GetInlineCacheCount());
#if DBG
        Assert(this->m_inlineCacheTypes[index] == InlineCacheTypeNone ||
            this->m_inlineCacheTypes[index] == InlineCacheTypeInlineCache);
        this->m_inlineCacheTypes[index] = InlineCacheTypeInlineCache;
#endif
        return reinterpret_cast<InlineCache *>(PointerValue(this->m_inlineCaches[index]));
    }

    void ScriptFunctionWithInlineCache::SetInlineCachesFromFunctionBody()
    {TRACE_IT(62954);
        SetHasInlineCaches(true);
        Js::FunctionBody* functionBody = this->GetFunctionBody();
        this->m_inlineCaches = (Field(void*)*)functionBody->GetInlineCaches();
#if DBG
        this->m_inlineCacheTypes = functionBody->GetInlineCacheTypes();
#endif
        this->rootObjectLoadInlineCacheStart = functionBody->GetRootObjectLoadInlineCacheStart();
        this->rootObjectLoadMethodInlineCacheStart = functionBody->GetRootObjectLoadMethodInlineCacheStart();
        this->rootObjectStoreInlineCacheStart = functionBody->GetRootObjectStoreInlineCacheStart();
        this->inlineCacheCount = functionBody->GetInlineCacheCount();
        this->isInstInlineCacheCount = functionBody->GetIsInstInlineCacheCount();
    }

    void ScriptFunctionWithInlineCache::CreateInlineCache()
    {TRACE_IT(62955);
        Js::FunctionBody *functionBody = this->GetFunctionBody();
        this->rootObjectLoadInlineCacheStart = functionBody->GetRootObjectLoadInlineCacheStart();
        this->rootObjectStoreInlineCacheStart = functionBody->GetRootObjectStoreInlineCacheStart();
        this->inlineCacheCount = functionBody->GetInlineCacheCount();
        this->isInstInlineCacheCount = functionBody->GetIsInstInlineCacheCount();

        SetHasInlineCaches(true);
        AllocateInlineCache();
        hasOwnInlineCaches = true;
    }

    void ScriptFunctionWithInlineCache::Finalize(bool isShutdown)
    {TRACE_IT(62956);
        if (isShutdown)
        {TRACE_IT(62957);
            FreeOwnInlineCaches<true>();
        }
        else
        {TRACE_IT(62958);
            FreeOwnInlineCaches<false>();
        }
    }
    template<bool isShutdown>
    void ScriptFunctionWithInlineCache::FreeOwnInlineCaches()
    {TRACE_IT(62959);
        uint isInstInlineCacheStart = this->GetInlineCacheCount();
        uint totalCacheCount = isInstInlineCacheStart + isInstInlineCacheCount;
        if (this->GetHasInlineCaches() && this->m_inlineCaches && this->hasOwnInlineCaches)
        {TRACE_IT(62960);
            Js::ScriptContext* scriptContext = this->GetParseableFunctionInfo()->GetScriptContext();
            uint i = 0;
            uint unregisteredInlineCacheCount = 0;
            uint plainInlineCacheEnd = rootObjectLoadInlineCacheStart;
            __analysis_assume(plainInlineCacheEnd < totalCacheCount);
            for (; i < plainInlineCacheEnd; i++)
            {TRACE_IT(62961);
                if (this->m_inlineCaches[i])
                {TRACE_IT(62962);
                    InlineCache* inlineCache = (InlineCache*)(void*)this->m_inlineCaches[i];
                    if (isShutdown)
                    {TRACE_IT(62963);
                        memset(this->m_inlineCaches[i], 0, sizeof(InlineCache));
                    }
                    else if(!scriptContext->IsClosed())
                    {TRACE_IT(62964);
                        if (inlineCache->RemoveFromInvalidationList())
                        {TRACE_IT(62965);
                            unregisteredInlineCacheCount++;
                        }
                        AllocatorDelete(InlineCacheAllocator, scriptContext->GetInlineCacheAllocator(), inlineCache);
                    }
                    this->m_inlineCaches[i] = nullptr;
                }
            }

            i = isInstInlineCacheStart;
            for (; i < totalCacheCount; i++)
            {TRACE_IT(62966);
                if (this->m_inlineCaches[i])
                {TRACE_IT(62967);
                    if (isShutdown)
                    {TRACE_IT(62968);
                        memset(this->m_inlineCaches[i], 0, sizeof(IsInstInlineCache));
                    }
                    else if (!scriptContext->IsClosed())
                    {
                        AllocatorDelete(CacheAllocator, scriptContext->GetIsInstInlineCacheAllocator(), (IsInstInlineCache*)(void*)this->m_inlineCaches[i]);
                    }
                    this->m_inlineCaches[i] = nullptr;
                }
            }

            if (unregisteredInlineCacheCount > 0)
            {TRACE_IT(62969);
                AssertMsg(!isShutdown && !scriptContext->IsClosed(), "Unregistration of inlineCache should only be done if this is not shutdown or scriptContext closing.");
                scriptContext->GetThreadContext()->NotifyInlineCacheBatchUnregistered(unregisteredInlineCacheCount);
            }
        }
    }

    void ScriptFunctionWithInlineCache::AllocateInlineCache()
    {TRACE_IT(62970);
        Assert(this->m_inlineCaches == nullptr);
        uint isInstInlineCacheStart = this->GetInlineCacheCount();
        uint totalCacheCount = isInstInlineCacheStart + isInstInlineCacheCount;
        Js::FunctionBody* functionBody = this->GetFunctionBody();

        if (totalCacheCount != 0)
        {TRACE_IT(62971);
            // Root object inline cache are not leaf
            Js::ScriptContext* scriptContext = this->GetFunctionBody()->GetScriptContext();
            void ** inlineCaches = RecyclerNewArrayZ(scriptContext->GetRecycler() ,
                void*, totalCacheCount);
#if DBG
            this->m_inlineCacheTypes = RecyclerNewArrayLeafZ(scriptContext->GetRecycler(),
                byte, totalCacheCount);
#endif
            uint i = 0;
            uint plainInlineCacheEnd = rootObjectLoadInlineCacheStart;
            __analysis_assume(plainInlineCacheEnd <= totalCacheCount);
            for (; i < plainInlineCacheEnd; i++)
            {TRACE_IT(62972);
                inlineCaches[i] = AllocatorNewZ(InlineCacheAllocator,
                    scriptContext->GetInlineCacheAllocator(), InlineCache);
            }
            Js::RootObjectBase * rootObject = functionBody->GetRootObject();
            ThreadContext * threadContext = scriptContext->GetThreadContext();
            uint rootObjectLoadInlineCacheEnd = rootObjectLoadMethodInlineCacheStart;
            __analysis_assume(rootObjectLoadInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectLoadInlineCacheEnd; i++)
            {TRACE_IT(62973);
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), false, false);
            }
            uint rootObjectLoadMethodInlineCacheEnd = rootObjectStoreInlineCacheStart;
            __analysis_assume(rootObjectLoadMethodInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectLoadMethodInlineCacheEnd; i++)
            {TRACE_IT(62974);
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), true, false);
            }
            uint rootObjectStoreInlineCacheEnd = isInstInlineCacheStart;
            __analysis_assume(rootObjectStoreInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectStoreInlineCacheEnd; i++)
            {TRACE_IT(62975);
#pragma prefast(suppress:6386, "The analysis assume didn't help prefast figure out this is in range")
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), false, true);
            }
            for (; i < totalCacheCount; i++)
            {TRACE_IT(62976);
                inlineCaches[i] = AllocatorNewStructZ(CacheAllocator,
                    functionBody->GetScriptContext()->GetIsInstInlineCacheAllocator(), IsInstInlineCache);
            }
#if DBG
            this->m_inlineCacheTypes = RecyclerNewArrayLeafZ(functionBody->GetScriptContext()->GetRecycler(),
                byte, totalCacheCount);
#endif
            this->m_inlineCaches = (Field(void*)*)inlineCaches;
        }
    }

    bool ScriptFunction::GetSymbolName(const char16** symbolName, charcount_t* length) const
    {TRACE_IT(62977);
        if (nullptr != this->computedNameVar && JavascriptSymbol::Is(this->computedNameVar))
        {TRACE_IT(62978);
            const PropertyRecord* symbolRecord = JavascriptSymbol::FromVar(this->computedNameVar)->GetValue();
            *symbolName = symbolRecord->GetBuffer();
            *length = symbolRecord->GetLength();
            return true;
        }
        *symbolName = nullptr;
        *length = 0;
        return false;
    }

    JavascriptString* ScriptFunction::GetDisplayNameImpl() const
    {TRACE_IT(62979);
        Assert(this->GetFunctionProxy() != nullptr); // The caller should guarantee a proxy exists
        ParseableFunctionInfo * func = this->GetFunctionProxy()->EnsureDeserialized();
        const char16* name = nullptr;
        charcount_t length = 0;
        JavascriptString* returnStr = nullptr;
        ENTER_PINNED_SCOPE(JavascriptString, computedName);

        if (computedNameVar != nullptr)
        {TRACE_IT(62980);
            const char16* symbolName = nullptr;
            charcount_t symbolNameLength = 0;
            if (this->GetSymbolName(&symbolName, &symbolNameLength))
            {TRACE_IT(62981);
                if (symbolNameLength == 0)
                {TRACE_IT(62982);
                    name = symbolName;
                }
                else
                {TRACE_IT(62983);
                    name = FunctionProxy::WrapWithBrackets(symbolName, symbolNameLength, this->GetScriptContext());
                    length = symbolNameLength + 2; //adding 2 to length for  brackets
                }
            }
            else
            {TRACE_IT(62984);
                computedName = this->GetComputedName();
                if (!func->GetIsAccessor())
                {TRACE_IT(62985);
                    return computedName;
                }
                name = computedName->GetString();
                length = computedName->GetLength();
            }
        }
        else
        {TRACE_IT(62986);
            name = Constants::Empty;
            if (func->GetIsNamedFunctionExpression()) // GetIsNamedFunctionExpression -> ex. var a = function foo() {} where name is foo
            {TRACE_IT(62987);
                name = func->GetShortDisplayName(&length);
            }
            else if (func->GetIsNameIdentifierRef()) // GetIsNameIdentifierRef        -> confirms a name is not attached like o.x = function() {}
            {TRACE_IT(62988);
                if (this->GetScriptContext()->GetConfig()->IsES6FunctionNameFullEnabled())
                {TRACE_IT(62989);
                    name = func->GetShortDisplayName(&length);
                }
                else if (func->GetIsDeclaration() || // GetIsDeclaration -> ex. function foo () {}
                         func->GetIsAccessor()    || // GetIsAccessor    -> ex. var a = { get f() {}} new enough syntax that we do not have to disable by default
                         func->IsLambda()         || // IsLambda         -> ex. var y = { o : () => {}}
                         GetHomeObj())               // GetHomeObj       -> ex. var o = class {}, confirms this is a constructor or method on a class
                {TRACE_IT(62990);
                    name = func->GetShortDisplayName(&length);
                }
            }
        }
        AssertMsg(IsValidCharCount(length), "JavascriptString can't be larger than charcount_t");
        returnStr = DisplayNameHelper(name, static_cast<charcount_t>(length));

        LEAVE_PINNED_SCOPE();

        return returnStr;
    }

    bool ScriptFunction::IsAnonymousFunction() const
    {TRACE_IT(62991);
        return this->GetFunctionProxy()->GetIsAnonymousFunction();
    }

    JavascriptString* ScriptFunction::GetComputedName() const
    {TRACE_IT(62992);
        JavascriptString* computedName = nullptr;
        ScriptContext* scriptContext = this->GetScriptContext();
        if (computedNameVar != nullptr)
        {TRACE_IT(62993);
            if (TaggedInt::Is(computedNameVar))
            {TRACE_IT(62994);
                computedName = TaggedInt::ToString(computedNameVar, scriptContext);
            }
            else
            {TRACE_IT(62995);
                computedName = JavascriptConversion::ToString(computedNameVar, scriptContext);
            }
            return computedName;
        }
        return nullptr;
    }

    void ScriptFunctionWithInlineCache::ClearInlineCacheOnFunctionObject()
    {TRACE_IT(62996);
        if (NULL != this->m_inlineCaches)
        {TRACE_IT(62997);
            FreeOwnInlineCaches<false>();
            this->m_inlineCaches = nullptr;
            this->inlineCacheCount = 0;
            this->rootObjectLoadInlineCacheStart = 0;
            this->rootObjectLoadMethodInlineCacheStart = 0;
            this->rootObjectStoreInlineCacheStart = 0;
            this->isInstInlineCacheCount = 0;
        }
        SetHasInlineCaches(false);
    }

    void ScriptFunctionWithInlineCache::ClearBorrowedInlineCacheOnFunctionObject()
    {TRACE_IT(62998);
        if (this->hasOwnInlineCaches)
        {TRACE_IT(62999);
            return;
        }
        ClearInlineCacheOnFunctionObject();
    }
}
