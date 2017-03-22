//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ScriptFunctionBase::ScriptFunctionBase(DynamicType * type) :
        JavascriptFunction(type)
    {LOGMEIN("ScriptFunction.cpp] 10\n");}

    ScriptFunctionBase::ScriptFunctionBase(DynamicType * type, FunctionInfo * functionInfo) :
        JavascriptFunction(type, functionInfo)
    {LOGMEIN("ScriptFunction.cpp] 14\n");}

    bool ScriptFunctionBase::Is(Var func)
    {LOGMEIN("ScriptFunction.cpp] 17\n");
        return ScriptFunction::Is(func) || JavascriptGeneratorFunction::Is(func) || JavascriptAsyncFunction::Is(func);
    }

    ScriptFunctionBase * ScriptFunctionBase::FromVar(Var func)
    {LOGMEIN("ScriptFunction.cpp] 22\n");
        Assert(ScriptFunctionBase::Is(func));
        return reinterpret_cast<ScriptFunctionBase *>(func);
    }

    ScriptFunction::ScriptFunction(DynamicType * type) :
        ScriptFunctionBase(type), environment((FrameDisplay*)&NullFrameDisplay),
        cachedScopeObj(nullptr), hasInlineCaches(false), hasSuperReference(false), homeObj(nullptr),
        computedNameVar(nullptr), isActiveScript(false)
    {LOGMEIN("ScriptFunction.cpp] 31\n");}

    ScriptFunction::ScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType)
        : ScriptFunctionBase(deferredPrototypeType, proxy->GetFunctionInfo()),
        environment((FrameDisplay*)&NullFrameDisplay), cachedScopeObj(nullptr), homeObj(nullptr),
        hasInlineCaches(false), hasSuperReference(false), isActiveScript(false), computedNameVar(nullptr)
    {LOGMEIN("ScriptFunction.cpp] 37\n");
        Assert(proxy->GetFunctionInfo()->GetFunctionProxy() == proxy);
        Assert(proxy->EnsureDeferredPrototypeType() == deferredPrototypeType);
        DebugOnly(VerifyEntryPoint());

#if ENABLE_NATIVE_CODEGEN
#ifdef BGJIT_STATS
        if (!proxy->IsDeferred())
        {LOGMEIN("ScriptFunction.cpp] 45\n");
            FunctionBody* body = proxy->GetFunctionBody();
            if(!body->GetNativeEntryPointUsed() &&
                body->GetDefaultFunctionEntryPointInfo()->IsCodeGenDone())
            {LOGMEIN("ScriptFunction.cpp] 49\n");
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
    {LOGMEIN("ScriptFunction.cpp] 63\n");
        AssertMsg(infoRef!= nullptr, "BYTE-CODE VERIFY: Must specify a valid function to create");
        FunctionProxy* functionProxy = (*infoRef)->GetFunctionProxy();
        AssertMsg(functionProxy!= nullptr, "BYTE-CODE VERIFY: Must specify a valid function to create");

        ScriptContext* scriptContext = functionProxy->GetScriptContext();

        bool hasSuperReference = functionProxy->HasSuperReference();

        if (functionProxy->IsFunctionBody() && functionProxy->GetFunctionBody()->GetInlineCachesOnFunctionObject())
        {LOGMEIN("ScriptFunction.cpp] 73\n");
            Js::FunctionBody * functionBody = functionProxy->GetFunctionBody();
            ScriptFunctionWithInlineCache* pfuncScriptWithInlineCache = scriptContext->GetLibrary()->CreateScriptFunctionWithInlineCache(functionProxy);
            pfuncScriptWithInlineCache->SetEnvironment(environment);
            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(pfuncScriptWithInlineCache, EtwTrace::GetFunctionId(functionProxy)));

            Assert(functionBody->GetInlineCacheCount() + functionBody->GetIsInstInlineCacheCount());

            if (functionBody->GetIsFirstFunctionObject())
            {LOGMEIN("ScriptFunction.cpp] 82\n");
                // point the inline caches of the first function object to those on the function body.
                pfuncScriptWithInlineCache->SetInlineCachesFromFunctionBody();
                functionBody->SetIsNotFirstFunctionObject();
            }
            else
            {
                // allocate inline cache for this function object
                pfuncScriptWithInlineCache->CreateInlineCache();
            }

            pfuncScriptWithInlineCache->SetHasSuperReference(hasSuperReference);

            if (PHASE_TRACE1(Js::ScriptFunctionWithInlineCachePhase))
            {LOGMEIN("ScriptFunction.cpp] 96\n");
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                Output::Print(_u("Function object with inline cache: function number: (%s)\tfunction name: %s\n"),
                    functionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName());
                Output::Flush();
            }
            return pfuncScriptWithInlineCache;
        }
        else if(functionProxy->IsFunctionBody() && functionProxy->GetFunctionBody()->GetIsAsmJsFunction())
        {LOGMEIN("ScriptFunction.cpp] 106\n");
            AsmJsScriptFunction* asmJsFunc = scriptContext->GetLibrary()->CreateAsmJsScriptFunction(functionProxy);
            asmJsFunc->SetEnvironment(environment);

            Assert(!hasSuperReference);
            asmJsFunc->SetHasSuperReference(hasSuperReference);

            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(asmJsFunc, EtwTrace::GetFunctionId(functionProxy)));

            return asmJsFunc;
        }
        else
        {
            ScriptFunction* pfuncScript = scriptContext->GetLibrary()->CreateScriptFunction(functionProxy);
            pfuncScript->SetEnvironment(environment);

            pfuncScript->SetHasSuperReference(hasSuperReference);

            JS_ETW(EventWriteJSCRIPT_RECYCLER_ALLOCATE_FUNCTION(pfuncScript, EtwTrace::GetFunctionId(functionProxy)));

            return pfuncScript;
        }
    }

    void ScriptFunction::SetEnvironment(FrameDisplay * environment)
    {LOGMEIN("ScriptFunction.cpp] 131\n");
        //Assert(ThreadContext::IsOnStack(this) || !ThreadContext::IsOnStack(environment));
        this->environment = environment;
    }

    void ScriptFunction::InvalidateCachedScopeChain()
    {LOGMEIN("ScriptFunction.cpp] 137\n");
        // Note: Currently this helper assumes that we're in an eval-class case
        // where all the contents of the closure environment are dynamic objects.
        // Invalidating scopes that are raw slot arrays, etc., will have to be done
        // directly in byte code.

        // A function nested within this one has escaped.
        // Invalidate our own cached scope object, and walk the closure environment
        // doing this same.
        if (this->cachedScopeObj)
        {LOGMEIN("ScriptFunction.cpp] 147\n");
            this->cachedScopeObj->InvalidateCachedScope();
        }
        FrameDisplay *pDisplay = this->environment;
        uint length = (uint)pDisplay->GetLength();
        for (uint i = 0; i < length; i++)
        {LOGMEIN("ScriptFunction.cpp] 153\n");
            Var scope = pDisplay->GetItem(i);
            RecyclableObject *scopeObj = RecyclableObject::FromVar(scope);
            scopeObj->InvalidateCachedScope();
        }
    }

    bool ScriptFunction::Is(Var func)
    {LOGMEIN("ScriptFunction.cpp] 161\n");
        return JavascriptFunction::Is(func) && JavascriptFunction::FromVar(func)->GetFunctionInfo()->HasBody();
    }

    ScriptFunction * ScriptFunction::FromVar(Var func)
    {LOGMEIN("ScriptFunction.cpp] 166\n");
        Assert(ScriptFunction::Is(func));
        return reinterpret_cast<ScriptFunction *>(func);
    }

    ProxyEntryPointInfo * ScriptFunction::GetEntryPointInfo() const
    {LOGMEIN("ScriptFunction.cpp] 172\n");
        return this->GetScriptFunctionType()->GetEntryPointInfo();
    }

    ScriptFunctionType * ScriptFunction::GetScriptFunctionType() const
    {LOGMEIN("ScriptFunction.cpp] 177\n");
        return (ScriptFunctionType *)GetDynamicType();
    }

    ScriptFunctionType * ScriptFunction::DuplicateType()
    {LOGMEIN("ScriptFunction.cpp] 182\n");
        ScriptFunctionType* type = RecyclerNew(this->GetScriptContext()->GetRecycler(),
            ScriptFunctionType, this->GetScriptFunctionType());

        this->GetFunctionProxy()->RegisterFunctionObjectType(type);

        return type;
    }

    uint32 ScriptFunction::GetFrameHeight(FunctionEntryPointInfo* entryPointInfo) const
    {LOGMEIN("ScriptFunction.cpp] 192\n");
        Assert(this->GetFunctionBody() != nullptr);

        return this->GetFunctionBody()->GetFrameHeight(entryPointInfo);
    }

    bool ScriptFunction::HasFunctionBody()
    {LOGMEIN("ScriptFunction.cpp] 199\n");
        // for asmjs we want to first check if the FunctionObject has a function body. Check that the function is not deferred
        return  !this->GetFunctionInfo()->IsDeferredParseFunction() && !this->GetFunctionInfo()->IsDeferredDeserializeFunction() && GetParseableFunctionInfo()->IsFunctionParsed();
    }

    void ScriptFunction::ChangeEntryPoint(ProxyEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint)
    {LOGMEIN("ScriptFunction.cpp] 205\n");
        Assert(entryPoint != nullptr);
        Assert(this->GetTypeId() == TypeIds_Function);
#if ENABLE_NATIVE_CODEGEN
        Assert(!IsCrossSiteObject() || entryPoint != (Js::JavascriptMethod)checkCodeGenThunk);
#else
        Assert(!IsCrossSiteObject());
#endif

        Assert((entryPointInfo != nullptr && this->GetFunctionProxy() != nullptr));
        if (this->GetEntryPoint() == entryPoint && this->GetScriptFunctionType()->GetEntryPointInfo() == entryPointInfo)
        {LOGMEIN("ScriptFunction.cpp] 216\n");
            return;
        }

        bool isAsmJS = false;
        if (HasFunctionBody())
        {LOGMEIN("ScriptFunction.cpp] 222\n");
            isAsmJS = this->GetFunctionBody()->GetIsAsmjsMode();
        }

        // ASMJS:- for asmjs we don't need to update the entry point here as it updates the types entry point
        if (!isAsmJS)
        {LOGMEIN("ScriptFunction.cpp] 228\n");
            // We can't go from cross-site to non-cross-site. Update only in the non-cross site case
            if (!CrossSite::IsThunk(this->GetEntryPoint()))
            {LOGMEIN("ScriptFunction.cpp] 231\n");
                this->SetEntryPoint(entryPoint);
            }
        }
        // instead update the address in the function entrypoint info
        else
        {
            entryPointInfo->jsMethod = entryPoint;
        }

        if (!isAsmJS)
        {LOGMEIN("ScriptFunction.cpp] 242\n");
            ProxyEntryPointInfo* oldEntryPointInfo = this->GetScriptFunctionType()->GetEntryPointInfo();
            if (oldEntryPointInfo
                && oldEntryPointInfo != entryPointInfo
                && oldEntryPointInfo->SupportsExpiration())
            {LOGMEIN("ScriptFunction.cpp] 247\n");
                // The old entry point could be executing so we need root it to make sure
                // it isn't prematurely collected. The rooting is done by queuing it up on the threadContext
                ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();

                threadContext->QueueFreeOldEntryPointInfoIfInScript((FunctionEntryPointInfo*)oldEntryPointInfo);
            }
        }

        this->GetScriptFunctionType()->SetEntryPointInfo(entryPointInfo);
    }

    FunctionProxy * ScriptFunction::GetFunctionProxy() const
    {LOGMEIN("ScriptFunction.cpp] 260\n");
        Assert(this->functionInfo->HasBody());
        return this->functionInfo->GetFunctionProxy();
    }
    JavascriptMethod ScriptFunction::UpdateUndeferredBody(FunctionBody* newFunctionInfo)
    {LOGMEIN("ScriptFunction.cpp] 265\n");
        // Update deferred parsed/serialized function to the real function body
        Assert(this->functionInfo->HasBody());
        Assert(this->functionInfo->GetFunctionBody() == newFunctionInfo);
        Assert(!newFunctionInfo->IsDeferred());

        DynamicType * type = this->GetDynamicType();

        // If the type is shared, it must be the shared one in the old function proxy

        this->functionInfo = newFunctionInfo->GetFunctionInfo();

        if (type->GetIsShared())
        {LOGMEIN("ScriptFunction.cpp] 278\n");
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
    {LOGMEIN("ScriptFunction.cpp] 302\n");
        this->ChangeEntryPoint(entryPointInfo, entryPoint);

        if (!CrossSite::IsThunk(this->GetEntryPoint()))
        {LOGMEIN("ScriptFunction.cpp] 306\n");
            return entryPoint;
        }

        // We already pass through the cross site thunk, which would have called the profile thunk already if necessary
        // So just call the original entry point if our direct entry is the profile entry thunk
        // Otherwise, call the directEntryPoint which may have additional processing to do (e.g. ensure dynamic profile)
        Assert(this->IsCrossSiteObject());
        if (entryPoint != ProfileEntryThunk)
        {LOGMEIN("ScriptFunction.cpp] 315\n");
            return entryPoint;
        }
        // Based on the comment below, this shouldn't be a defer deserialization function as it would have a deferred thunk
        FunctionBody * functionBody = this->GetFunctionBody();
        // The original entry point should be an interpreter thunk or the native entry point;
        Assert(functionBody->IsInterpreterThunk() || functionBody->IsNativeOriginalEntryPoint());
        return functionBody->GetOriginalEntryPoint();
    }

    bool ScriptFunction::IsNewEntryPointAvailable()
    {LOGMEIN("ScriptFunction.cpp] 326\n");
        Js::FunctionEntryPointInfo *const defaultEntryPointInfo = this->GetFunctionBody()->GetDefaultFunctionEntryPointInfo();
        JavascriptMethod defaultEntryPoint = this->GetFunctionBody()->GetDirectEntryPoint(defaultEntryPointInfo);

        return this->GetEntryPoint() != defaultEntryPoint;
    }

    Var ScriptFunction::GetSourceString() const
    {LOGMEIN("ScriptFunction.cpp] 334\n");
        return this->GetFunctionProxy()->EnsureDeserialized()->GetCachedSourceString();
    }

    Var ScriptFunction::FormatToString(JavascriptString* inputString)
    {LOGMEIN("ScriptFunction.cpp] 339\n");
        FunctionProxy* proxy = this->GetFunctionProxy();
        ParseableFunctionInfo * pFuncBody = proxy->EnsureDeserialized();
        const char16 * inputStr = inputString->GetString();
        const char16 * paramStr = wcschr(inputStr, _u('('));

        if (paramStr == nullptr || wcscmp(pFuncBody->GetDisplayName(), Js::Constants::EvalCode) == 0)
        {LOGMEIN("ScriptFunction.cpp] 346\n");
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
        {LOGMEIN("ScriptFunction.cpp] 362\n");
            prefixString = library->GetFunctionPrefixString();
            if (pFuncBody->IsGenerator())
            {LOGMEIN("ScriptFunction.cpp] 365\n");
                prefixString = library->GetGeneratorFunctionPrefixString();
            }
            else if (pFuncBody->IsAsync())
            {LOGMEIN("ScriptFunction.cpp] 369\n");
                prefixString = library->GetAsyncFunctionPrefixString();
            }
            prefixStringLength = prefixString->GetLength();

            if (pFuncBody->GetIsAccessor())
            {LOGMEIN("ScriptFunction.cpp] 375\n");
                name = pFuncBody->GetShortDisplayName(&nameLength);

            }
            else if (pFuncBody->GetIsDeclaration() || pFuncBody->GetIsNamedFunctionExpression())
            {LOGMEIN("ScriptFunction.cpp] 380\n");
                name = pFuncBody->GetDisplayName();
                nameLength = pFuncBody->GetDisplayNameLength();
                if (name == Js::Constants::FunctionCode)
                {LOGMEIN("ScriptFunction.cpp] 384\n");
                    name = Js::Constants::Anonymous;
                    nameLength = Js::Constants::AnonymousLength;
                }

            }
        }
        else
        {

            if (this->GetFunctionInfo()->IsClassConstructor())
            {LOGMEIN("ScriptFunction.cpp] 395\n");
                name = _u("constructor");
                nameLength = _countof(_u("constructor")) -1; //subtract off \0
            }
            else
            {
                name = pFuncBody->GetShortDisplayName(&nameLength); //strip off prototype.
            }
        }

        ENTER_PINNED_SCOPE(JavascriptString, computedName);
        computedName = this->GetComputedName();
        if (computedName != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 408\n");
            prefixString = nullptr;
            prefixStringLength = 0;
            name = computedName->GetString();
            nameLength = computedName->GetLength();
        }

        uint functionBodyLength = inputString->GetLength() - ((uint)(paramStr - inputStr));
        size_t totalLength = prefixStringLength + functionBodyLength + nameLength;

        if (!IsValidCharCount(totalLength))
        {LOGMEIN("ScriptFunction.cpp] 419\n");
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
    {LOGMEIN("ScriptFunction.cpp] 446\n");
        // The function may be defer serialize, need to be deserialized
        FunctionProxy* proxy = this->GetFunctionProxy();
        ParseableFunctionInfo * pFuncBody = proxy->EnsureDeserialized();
        Var cachedSourceString = pFuncBody->GetCachedSourceString();
        if (cachedSourceString != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 452\n");
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
        {LOGMEIN("ScriptFunction.cpp] 465\n");
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
        {LOGMEIN("ScriptFunction.cpp] 476\n");
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
            {LOGMEIN("ScriptFunction.cpp] 492\n");
                cachedSourceString = builder.ToString();
            }
            else
            {
                cachedSourceString = FormatToString(builder.ToString());
            }
        }
        else
        {
            cachedSourceString = scriptContext->GetLibrary()->GetXDomainFunctionDisplayString();
        }
        Assert(cachedSourceString != nullptr);
        pFuncBody->SetCachedSourceString(cachedSourceString);
        return cachedSourceString;
    }

#if ENABLE_TTD
    void ScriptFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("ScriptFunction.cpp] 511\n");
        Js::FunctionBody* fb = TTD::JsSupport::ForceAndGetFunctionBody(this->GetParseableFunctionInfo());
        extractor->MarkFunctionBody(fb);

        Js::FrameDisplay* environment = this->GetEnvironment();
        if(environment->GetLength() != 0)
        {LOGMEIN("ScriptFunction.cpp] 517\n");
            extractor->MarkScriptFunctionScopeInfo(environment);
        }

        if(this->cachedScopeObj != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 522\n");
            extractor->MarkVisitVar(this->cachedScopeObj);
        }

        if(this->homeObj != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 527\n");
            extractor->MarkVisitVar(this->homeObj);
        }

        if(this->computedNameVar != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 532\n");
            extractor->MarkVisitVar(this->computedNameVar);
        }
    }

    void ScriptFunction::ProcessCorePaths()
    {LOGMEIN("ScriptFunction.cpp] 538\n");
        TTD::RuntimeContextInfo* rctxInfo = this->GetScriptContext()->TTDWellKnownInfo;

        //do the body path mark
        Js::FunctionBody* fb = TTD::JsSupport::ForceAndGetFunctionBody(this->GetParseableFunctionInfo());
        rctxInfo->EnqueueNewFunctionBodyObject(this, fb, _u("!fbody"));

        Js::FrameDisplay* environment = this->GetEnvironment();
        uint32 scopeCount = environment->GetLength();

        for(uint32 i = 0; i < scopeCount; ++i)
        {LOGMEIN("ScriptFunction.cpp] 549\n");
            TTD::UtilSupport::TTAutoString scopePathString;
            rctxInfo->BuildEnvironmentIndexBuffer(i, scopePathString);

            void* scope = environment->GetItem(i);
            switch(environment->GetScopeType(scope))
            {LOGMEIN("ScriptFunction.cpp] 555\n");
            case Js::ScopeType::ScopeType_ActivationObject:
            case Js::ScopeType::ScopeType_WithScope:
            {LOGMEIN("ScriptFunction.cpp] 558\n");
                rctxInfo->EnqueueNewPathVarAsNeeded(this, (Js::Var)scope, scopePathString.GetStrValue());
                break;
            }
            case Js::ScopeType::ScopeType_SlotArray:
            {LOGMEIN("ScriptFunction.cpp] 563\n");
                Js::ScopeSlots slotArray = (Js::Var*)scope;
                uint slotArrayCount = slotArray.GetCount();

                //get the function body associated with the scope
                if(slotArray.IsFunctionScopeSlotArray())
                {LOGMEIN("ScriptFunction.cpp] 569\n");
                    rctxInfo->EnqueueNewFunctionBodyObject(this, slotArray.GetFunctionInfo()->GetFunctionBody(), scopePathString.GetStrValue());
                }
                else
                {
                    rctxInfo->AddWellKnownDebuggerScopePath(this, slotArray.GetDebuggerScope(), i);
                }

                for(uint j = 0; j < slotArrayCount; j++)
                {LOGMEIN("ScriptFunction.cpp] 578\n");
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
        {LOGMEIN("ScriptFunction.cpp] 595\n");
            this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->cachedScopeObj, _u("_cachedScopeObj"));
        }

        if(this->homeObj != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 600\n");
            this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->homeObj, _u("_homeObj"));
        }
    }

    TTD::NSSnapObjects::SnapObjectType ScriptFunction::GetSnapTag_TTD() const
    {LOGMEIN("ScriptFunction.cpp] 606\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapScriptFunctionObject;
    }

    void ScriptFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ScriptFunction.cpp] 611\n");
        TTDAssert(this->GetFunctionInfo() != nullptr, "We are only doing this for functions with ParseableFunctionInfo.");

        TTD::NSSnapObjects::SnapScriptFunctionInfo* ssfi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapScriptFunctionInfo>();
        Js::FunctionBody* fb = TTD::JsSupport::ForceAndGetFunctionBody(this->GetParseableFunctionInfo());

        alloc.CopyNullTermStringInto(fb->GetDisplayName(), ssfi->DebugFunctionName);

        ssfi->BodyRefId = TTD_CONVERT_FUNCTIONBODY_TO_PTR_ID(fb);

        Js::FrameDisplay* environment = this->GetEnvironment();
        ssfi->ScopeId = TTD_INVALID_PTR_ID;
        if(environment->GetLength() != 0)
        {LOGMEIN("ScriptFunction.cpp] 624\n");
            ssfi->ScopeId = TTD_CONVERT_SCOPE_TO_PTR_ID(environment);
        }

        ssfi->CachedScopeObjId = TTD_INVALID_PTR_ID;
        if(this->cachedScopeObj != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 630\n");
            ssfi->CachedScopeObjId = TTD_CONVERT_VAR_TO_PTR_ID(this->cachedScopeObj);
        }

        ssfi->HomeObjId = TTD_INVALID_PTR_ID;
        if(this->homeObj != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 636\n");
            ssfi->HomeObjId = TTD_CONVERT_VAR_TO_PTR_ID(this->homeObj);
        }

        ssfi->ComputedNameInfo = TTD_CONVERT_JSVAR_TO_TTDVAR(this->computedNameVar);

        ssfi->HasSuperReference = this->hasSuperReference;

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapScriptFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapScriptFunctionObject>(objData, ssfi);
    }
#endif

    AsmJsScriptFunction::AsmJsScriptFunction(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType) :
        ScriptFunction(proxy, deferredPrototypeType), m_moduleMemory(nullptr)
    {LOGMEIN("ScriptFunction.cpp] 650\n");}

    AsmJsScriptFunction::AsmJsScriptFunction(DynamicType * type) :
        ScriptFunction(type), m_moduleMemory(nullptr)
    {LOGMEIN("ScriptFunction.cpp] 654\n");}

    bool AsmJsScriptFunction::Is(Var func)
    {LOGMEIN("ScriptFunction.cpp] 657\n");
        return ScriptFunction::Is(func) &&
            ScriptFunction::FromVar(func)->HasFunctionBody() &&
            ScriptFunction::FromVar(func)->GetFunctionBody()->GetIsAsmJsFunction();
    }

    bool AsmJsScriptFunction::IsWasmScriptFunction(Var func)
    {LOGMEIN("ScriptFunction.cpp] 664\n");
        return ScriptFunction::Is(func) &&
            ScriptFunction::FromVar(func)->HasFunctionBody() &&
            ScriptFunction::FromVar(func)->GetFunctionBody()->IsWasmFunction();
    }

    AsmJsScriptFunction* AsmJsScriptFunction::FromVar(Var func)
    {LOGMEIN("ScriptFunction.cpp] 671\n");
        Assert(AsmJsScriptFunction::Is(func));
        return reinterpret_cast<AsmJsScriptFunction *>(func);
    }

    ScriptFunctionWithInlineCache::ScriptFunctionWithInlineCache(FunctionProxy * proxy, ScriptFunctionType* deferredPrototypeType) :
        ScriptFunction(proxy, deferredPrototypeType), hasOwnInlineCaches(false)
    {LOGMEIN("ScriptFunction.cpp] 678\n");}

    ScriptFunctionWithInlineCache::ScriptFunctionWithInlineCache(DynamicType * type) :
        ScriptFunction(type), hasOwnInlineCaches(false)
    {LOGMEIN("ScriptFunction.cpp] 682\n");}

    bool ScriptFunctionWithInlineCache::Is(Var func)
    {LOGMEIN("ScriptFunction.cpp] 685\n");
        return ScriptFunction::Is(func) && ScriptFunction::FromVar(func)->GetHasInlineCaches();
    }

    ScriptFunctionWithInlineCache* ScriptFunctionWithInlineCache::FromVar(Var func)
    {LOGMEIN("ScriptFunction.cpp] 690\n");
        Assert(ScriptFunctionWithInlineCache::Is(func));
        return reinterpret_cast<ScriptFunctionWithInlineCache *>(func);
    }

    InlineCache * ScriptFunctionWithInlineCache::GetInlineCache(uint index)
    {LOGMEIN("ScriptFunction.cpp] 696\n");
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
    {LOGMEIN("ScriptFunction.cpp] 708\n");
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
    {LOGMEIN("ScriptFunction.cpp] 723\n");
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
    {LOGMEIN("ScriptFunction.cpp] 736\n");
        if (isShutdown)
        {LOGMEIN("ScriptFunction.cpp] 738\n");
            FreeOwnInlineCaches<true>();
        }
        else
        {
            FreeOwnInlineCaches<false>();
        }
    }
    template<bool isShutdown>
    void ScriptFunctionWithInlineCache::FreeOwnInlineCaches()
    {LOGMEIN("ScriptFunction.cpp] 748\n");
        uint isInstInlineCacheStart = this->GetInlineCacheCount();
        uint totalCacheCount = isInstInlineCacheStart + isInstInlineCacheCount;
        if (this->GetHasInlineCaches() && this->m_inlineCaches && this->hasOwnInlineCaches)
        {LOGMEIN("ScriptFunction.cpp] 752\n");
            Js::ScriptContext* scriptContext = this->GetParseableFunctionInfo()->GetScriptContext();
            uint i = 0;
            uint unregisteredInlineCacheCount = 0;
            uint plainInlineCacheEnd = rootObjectLoadInlineCacheStart;
            __analysis_assume(plainInlineCacheEnd < totalCacheCount);
            for (; i < plainInlineCacheEnd; i++)
            {LOGMEIN("ScriptFunction.cpp] 759\n");
                if (this->m_inlineCaches[i])
                {LOGMEIN("ScriptFunction.cpp] 761\n");
                    InlineCache* inlineCache = (InlineCache*)(void*)this->m_inlineCaches[i];
                    if (isShutdown)
                    {LOGMEIN("ScriptFunction.cpp] 764\n");
                        memset(this->m_inlineCaches[i], 0, sizeof(InlineCache));
                    }
                    else if(!scriptContext->IsClosed())
                    {LOGMEIN("ScriptFunction.cpp] 768\n");
                        if (inlineCache->RemoveFromInvalidationList())
                        {LOGMEIN("ScriptFunction.cpp] 770\n");
                            unregisteredInlineCacheCount++;
                        }
                        AllocatorDelete(InlineCacheAllocator, scriptContext->GetInlineCacheAllocator(), inlineCache);
                    }
                    this->m_inlineCaches[i] = nullptr;
                }
            }

            i = isInstInlineCacheStart;
            for (; i < totalCacheCount; i++)
            {LOGMEIN("ScriptFunction.cpp] 781\n");
                if (this->m_inlineCaches[i])
                {LOGMEIN("ScriptFunction.cpp] 783\n");
                    if (isShutdown)
                    {LOGMEIN("ScriptFunction.cpp] 785\n");
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
            {LOGMEIN("ScriptFunction.cpp] 797\n");
                AssertMsg(!isShutdown && !scriptContext->IsClosed(), "Unregistration of inlineCache should only be done if this is not shutdown or scriptContext closing.");
                scriptContext->GetThreadContext()->NotifyInlineCacheBatchUnregistered(unregisteredInlineCacheCount);
            }
        }
    }

    void ScriptFunctionWithInlineCache::AllocateInlineCache()
    {LOGMEIN("ScriptFunction.cpp] 805\n");
        Assert(this->m_inlineCaches == nullptr);
        uint isInstInlineCacheStart = this->GetInlineCacheCount();
        uint totalCacheCount = isInstInlineCacheStart + isInstInlineCacheCount;
        Js::FunctionBody* functionBody = this->GetFunctionBody();

        if (totalCacheCount != 0)
        {LOGMEIN("ScriptFunction.cpp] 812\n");
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
            {LOGMEIN("ScriptFunction.cpp] 825\n");
                inlineCaches[i] = AllocatorNewZ(InlineCacheAllocator,
                    scriptContext->GetInlineCacheAllocator(), InlineCache);
            }
            Js::RootObjectBase * rootObject = functionBody->GetRootObject();
            ThreadContext * threadContext = scriptContext->GetThreadContext();
            uint rootObjectLoadInlineCacheEnd = rootObjectLoadMethodInlineCacheStart;
            __analysis_assume(rootObjectLoadInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectLoadInlineCacheEnd; i++)
            {LOGMEIN("ScriptFunction.cpp] 834\n");
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), false, false);
            }
            uint rootObjectLoadMethodInlineCacheEnd = rootObjectStoreInlineCacheStart;
            __analysis_assume(rootObjectLoadMethodInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectLoadMethodInlineCacheEnd; i++)
            {LOGMEIN("ScriptFunction.cpp] 841\n");
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), true, false);
            }
            uint rootObjectStoreInlineCacheEnd = isInstInlineCacheStart;
            __analysis_assume(rootObjectStoreInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectStoreInlineCacheEnd; i++)
            {LOGMEIN("ScriptFunction.cpp] 848\n");
#pragma prefast(suppress:6386, "The analysis assume didn't help prefast figure out this is in range")
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(functionBody->GetPropertyIdFromCacheId(i)), false, true);
            }
            for (; i < totalCacheCount; i++)
            {LOGMEIN("ScriptFunction.cpp] 854\n");
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
    {LOGMEIN("ScriptFunction.cpp] 867\n");
        if (nullptr != this->computedNameVar && JavascriptSymbol::Is(this->computedNameVar))
        {LOGMEIN("ScriptFunction.cpp] 869\n");
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
    {LOGMEIN("ScriptFunction.cpp] 881\n");
        Assert(this->GetFunctionProxy() != nullptr); // The caller should guarantee a proxy exists
        ParseableFunctionInfo * func = this->GetFunctionProxy()->EnsureDeserialized();
        const char16* name = nullptr;
        charcount_t length = 0;
        JavascriptString* returnStr = nullptr;
        ENTER_PINNED_SCOPE(JavascriptString, computedName);

        if (computedNameVar != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 890\n");
            const char16* symbolName = nullptr;
            charcount_t symbolNameLength = 0;
            if (this->GetSymbolName(&symbolName, &symbolNameLength))
            {LOGMEIN("ScriptFunction.cpp] 894\n");
                if (symbolNameLength == 0)
                {LOGMEIN("ScriptFunction.cpp] 896\n");
                    name = symbolName;
                }
                else
                {
                    name = FunctionProxy::WrapWithBrackets(symbolName, symbolNameLength, this->GetScriptContext());
                    length = symbolNameLength + 2; //adding 2 to length for  brackets
                }
            }
            else
            {
                computedName = this->GetComputedName();
                if (!func->GetIsAccessor())
                {LOGMEIN("ScriptFunction.cpp] 909\n");
                    return computedName;
                }
                name = computedName->GetString();
                length = computedName->GetLength();
            }
        }
        else
        {
            name = Constants::Empty;
            if (func->GetIsNamedFunctionExpression()) // GetIsNamedFunctionExpression -> ex. var a = function foo() {} where name is foo
            {LOGMEIN("ScriptFunction.cpp] 920\n");
                name = func->GetShortDisplayName(&length);
            }
            else if (func->GetIsNameIdentifierRef()) // GetIsNameIdentifierRef        -> confirms a name is not attached like o.x = function() {}
            {LOGMEIN("ScriptFunction.cpp] 924\n");
                if (this->GetScriptContext()->GetConfig()->IsES6FunctionNameFullEnabled())
                {LOGMEIN("ScriptFunction.cpp] 926\n");
                    name = func->GetShortDisplayName(&length);
                }
                else if (func->GetIsDeclaration() || // GetIsDeclaration -> ex. function foo () {}
                         func->GetIsAccessor()    || // GetIsAccessor    -> ex. var a = { get f() {}} new enough syntax that we do not have to disable by default
                         func->IsLambda()         || // IsLambda         -> ex. var y = { o : () => {}}
                         GetHomeObj())               // GetHomeObj       -> ex. var o = class {}, confirms this is a constructor or method on a class
                {LOGMEIN("ScriptFunction.cpp] 933\n");
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
    {LOGMEIN("ScriptFunction.cpp] 947\n");
        return this->GetFunctionProxy()->GetIsAnonymousFunction();
    }

    JavascriptString* ScriptFunction::GetComputedName() const
    {LOGMEIN("ScriptFunction.cpp] 952\n");
        JavascriptString* computedName = nullptr;
        ScriptContext* scriptContext = this->GetScriptContext();
        if (computedNameVar != nullptr)
        {LOGMEIN("ScriptFunction.cpp] 956\n");
            if (TaggedInt::Is(computedNameVar))
            {LOGMEIN("ScriptFunction.cpp] 958\n");
                computedName = TaggedInt::ToString(computedNameVar, scriptContext);
            }
            else
            {
                computedName = JavascriptConversion::ToString(computedNameVar, scriptContext);
            }
            return computedName;
        }
        return nullptr;
    }

    void ScriptFunctionWithInlineCache::ClearInlineCacheOnFunctionObject()
    {LOGMEIN("ScriptFunction.cpp] 971\n");
        if (NULL != this->m_inlineCaches)
        {LOGMEIN("ScriptFunction.cpp] 973\n");
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
    {LOGMEIN("ScriptFunction.cpp] 986\n");
        if (this->hasOwnInlineCaches)
        {LOGMEIN("ScriptFunction.cpp] 988\n");
            return;
        }
        ClearInlineCacheOnFunctionObject();
    }
}
