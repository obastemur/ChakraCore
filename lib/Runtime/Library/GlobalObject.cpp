//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#ifndef USING_PAL_STDLIB
#include <strsafe.h>
#endif
#include "ByteCode/ByteCodeApi.h"
#include "Exceptions/EvalDisabledException.h"

#include "Types/PropertyIndexRanges.h"
#include "Types/SimpleDictionaryPropertyDescriptor.h"
#include "Types/SimpleDictionaryTypeHandler.h"

namespace Js
{
    GlobalObject * GlobalObject::New(ScriptContext * scriptContext)
    {LOGMEIN("GlobalObject.cpp] 18\n");
        SimpleDictionaryTypeHandler* globalTypeHandler = SimpleDictionaryTypeHandler::New(
            scriptContext->GetRecycler(), InitialCapacity, InlineSlotCapacity, sizeof(Js::GlobalObject));

        DynamicType* globalType = DynamicType::New(
            scriptContext, TypeIds_GlobalObject, nullptr, nullptr, globalTypeHandler);

        GlobalObject* globalObject = RecyclerNewPlus(scriptContext->GetRecycler(),
            sizeof(Var) * InlineSlotCapacity, GlobalObject, globalType, scriptContext);

        globalTypeHandler->SetSingletonInstanceIfNeeded(scriptContext->GetRecycler()->CreateWeakReferenceHandle<DynamicObject>(globalObject));

        return globalObject;
    }

    GlobalObject::GlobalObject(DynamicType * type, ScriptContext* scriptContext) :
        RootObjectBase(type, scriptContext),
        directHostObject(nullptr),
        secureDirectHostObject(nullptr),
        EvalHelper(&GlobalObject::DefaultEvalHelper),
        reservedProperties(nullptr)
    {LOGMEIN("GlobalObject.cpp] 39\n");
    }

    void GlobalObject::Initialize(ScriptContext * scriptContext)
    {LOGMEIN("GlobalObject.cpp] 43\n");
        Assert(type->javascriptLibrary == nullptr);
        JavascriptLibrary* localLibrary = RecyclerNewFinalized(scriptContext->GetRecycler(), JavascriptLibrary, this);
        scriptContext->SetLibrary(localLibrary);
        type->javascriptLibrary = localLibrary;
        scriptContext->InitializeCache();
        localLibrary->Initialize(scriptContext, this);
        library = localLibrary;
    }

    bool GlobalObject::Is(Var aValue)
    {LOGMEIN("GlobalObject.cpp] 54\n");
        return RecyclableObject::Is(aValue) && (RecyclableObject::FromVar(aValue)->GetTypeId() == TypeIds_GlobalObject);
    }

    GlobalObject* GlobalObject::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'GlobalObject'");
        return static_cast<GlobalObject*>(aValue);
    }

    HRESULT GlobalObject::SetDirectHostObject(RecyclableObject* hostObject, RecyclableObject* secureDirectHostObject)
    {LOGMEIN("GlobalObject.cpp] 65\n");
        HRESULT hr = S_OK;

        BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
        {
            // In fastDOM scenario, we should use the host object to lookup the prototype.
            this->SetPrototype(library->GetNull());
        }
        END_TRANSLATE_OOM_TO_HRESULT(hr)

        this->directHostObject = hostObject;
        this->secureDirectHostObject = secureDirectHostObject;
        return hr;
    }

    RecyclableObject* GlobalObject::GetDirectHostObject()
    {LOGMEIN("GlobalObject.cpp] 81\n");
        return this->directHostObject;
    }

    RecyclableObject* GlobalObject::GetSecureDirectHostObject()
    {LOGMEIN("GlobalObject.cpp] 86\n");
        return this->secureDirectHostObject;
    }

    // Converts the global object into the object that should be used as the 'this' parameter.
    // In a non-hosted environment, the global object (this) is returned.
    // In a hosted environment, the host object is returned.
    Var GlobalObject::ToThis()
    {LOGMEIN("GlobalObject.cpp] 94\n");
        Var ret;

        // In fast DOM, we need to give user the secure version of host object
        if (secureDirectHostObject)
        {LOGMEIN("GlobalObject.cpp] 99\n");
            ret = secureDirectHostObject;
        }
        else if (hostObject)
        {LOGMEIN("GlobalObject.cpp] 103\n");
            // If the global object has the host object, use that as "this"
            ret = hostObject->GetHostDispatchVar();
            Assert(ret);
        }
        else
        {
            // Otherwise just use the global object
            ret = this;
        }

        return ret;
    }

    BOOL GlobalObject::ReserveGlobalProperty(PropertyId propertyId)
    {LOGMEIN("GlobalObject.cpp] 118\n");
        if (DynamicObject::HasProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 120\n");
            return false;
        }
        if (reservedProperties == nullptr)
        {LOGMEIN("GlobalObject.cpp] 124\n");
            Recycler* recycler = this->GetScriptContext()->GetRecycler();
            reservedProperties = RecyclerNew(recycler, ReservedPropertiesHashSet, recycler, 3);
        }
        reservedProperties->AddNew(propertyId);
        return true;
    }

    BOOL GlobalObject::IsReservedGlobalProperty(PropertyId propertyId)
    {LOGMEIN("GlobalObject.cpp] 133\n");
        return reservedProperties != nullptr && reservedProperties->Contains(propertyId);
    }

#ifdef IR_VIEWER
    Var GlobalObject::EntryParseIR(RecyclableObject *function, CallInfo callInfo, ...)
    {
        //
        // retrieve arguments
        //

        RUNTIME_ARGUMENTS(args, callInfo);
        Js::Var codeVar = args[1];  // args[0] is (this)

        Js::JavascriptString *codeStringVar = nullptr;
        const char16 *source = nullptr;
        size_t sourceLength = 0;

        if (Js::JavascriptString::Is(codeVar))
        {LOGMEIN("GlobalObject.cpp] 152\n");
            codeStringVar = (Js::JavascriptString *)codeVar;
            source = codeStringVar->GetString();
            sourceLength = codeStringVar->GetLength();
        }
        else
        {
            AssertMsg(false, "The input to parseIR was not a string.");
        }

        //
        // collect arguments for eval
        //

        /* @see NativeCodeGenerator::CodeGen */
        ScriptContext *scriptContext = function->GetScriptContext();

        // used arbitrary defaults for these, but it seems to work
        ModuleID moduleID = 0;
        uint32 grfscr = 0;
        LPCOLESTR pszTitle = _u("");
        BOOL registerDocument = false;

        BOOL strictMode = true;

        return IRDumpEvalHelper(scriptContext, source, sourceLength, moduleID, grfscr,
            pszTitle, registerDocument, FALSE, strictMode);
    }

    // TODO remove when refactor
    Js::PropertyId GlobalObject::CreateProperty(Js::ScriptContext *scriptContext, const char16 *propertyName)
    {LOGMEIN("GlobalObject.cpp] 183\n");
        Js::PropertyRecord const *propertyRecord;
        scriptContext->GetOrAddPropertyRecord(propertyName, (int) wcslen(propertyName), &propertyRecord);
        Js::PropertyId propertyId = propertyRecord->GetPropertyId();

        return propertyId;
    }

    // TODO remove when refactor
    void GlobalObject::SetProperty(Js::DynamicObject *obj, const char16 *propertyName, Js::Var value)
    {LOGMEIN("GlobalObject.cpp] 193\n");
        const size_t len = wcslen(propertyName);
        if (!(len > 0))
        {LOGMEIN("GlobalObject.cpp] 196\n");
            return;
        }

        Js::PropertyId id = CreateProperty(obj->GetScriptContext(), propertyName);
        SetProperty(obj, id, value);
    }

    // TODO remove when refactor
    void GlobalObject::SetProperty(Js::DynamicObject *obj, Js::PropertyId id, Js::Var value)
    {LOGMEIN("GlobalObject.cpp] 206\n");
        if (value == NULL)
        {LOGMEIN("GlobalObject.cpp] 208\n");
            return;
        }

        Js::JavascriptOperators::SetProperty(obj, obj, id, value, obj->GetScriptContext());
    }

    Var GlobalObject::FunctionInfoObjectBuilder(ScriptContext *scriptContext, const char16 *file,
        const char16 *function, ULONG lineNum, ULONG colNum,
        uint functionId, Js::Utf8SourceInfo *utf8SrcInfo, Js::Var source)
    {LOGMEIN("GlobalObject.cpp] 218\n");
        Js::DynamicObject *fnInfoObj = scriptContext->GetLibrary()->CreateObject();

        // create javascript objects for properties
        Js::Var filenameString = Js::JavascriptString::NewCopyBuffer(file, wcslen(file), scriptContext);
        Js::Var funcnameString = Js::JavascriptString::NewCopyBuffer(function, wcslen(function), scriptContext);
        Js::Var lineNumber = Js::JavascriptNumber::ToVar((int64) lineNum, scriptContext);
        Js::Var colNumber = Js::JavascriptNumber::ToVar((int64) colNum, scriptContext);
        Js::Var functionIdNumberVar = Js::JavascriptNumber::ToVar(functionId, scriptContext);
        Js::Var utf8SourceInfoVar = Js::JavascriptNumber::ToVar((int32) utf8SrcInfo, scriptContext);

        // assign properties to function info object
        SetProperty(fnInfoObj, _u("filename"), filenameString);
        SetProperty(fnInfoObj, _u("function"), funcnameString);
        SetProperty(fnInfoObj, _u("line"), lineNumber);
        SetProperty(fnInfoObj, _u("col"), colNumber);
        SetProperty(fnInfoObj, _u("funcId"), functionIdNumberVar);
        SetProperty(fnInfoObj, _u("utf8SrcInfoPtr"), utf8SourceInfoVar);
        SetProperty(fnInfoObj, _u("source"), source);

        return fnInfoObj;
    }

    /**
     * Return a list of functions visible in the current ScriptContext.
     */
    Var GlobalObject::EntryFunctionList(RecyclableObject *function, CallInfo callInfo, ...)
    {
        // Note: the typedefs below help make the following code more readable

        // See: Js::ScriptContext::SourceList (declaration not visible from this file)
        typedef JsUtil::List<RecyclerWeakReference<Utf8SourceInfo>*, Recycler, false, Js::FreeListedRemovePolicy> SourceList;
        typedef RecyclerWeakReference<Js::Utf8SourceInfo> Utf8SourceInfoRef;

        ScriptContext *originalScriptContext = function->GetScriptContext();
        ThreadContext *threadContext = originalScriptContext->GetThreadContext();
        ScriptContext *scriptContext;

        int fnCount = 0;
        for (scriptContext = threadContext->GetScriptContextList();
            scriptContext;
            scriptContext = scriptContext->next)
        {LOGMEIN("GlobalObject.cpp] 260\n");
            if (scriptContext->IsClosed()) continue;

            //
            // get count of functions in all files in script context
            //
            SourceList *sourceList = scriptContext->GetSourceList();
            sourceList->Map([&fnCount](uint i, Utf8SourceInfoRef *sourceInfoWeakRef)
            {
                Js::Utf8SourceInfo *sourceInfo = sourceInfoWeakRef->Get();
                if (sourceInfo == nullptr || sourceInfo->GetIsLibraryCode()) // library code has no source, skip
                {LOGMEIN("GlobalObject.cpp] 271\n");
                    return;
                }

                fnCount += sourceInfo->GetFunctionBodyCount();
            });
        }

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("GlobalObject.cpp] 281\n");
            Output::Print(_u(">> There are %d total functions\n"), fnCount);
            Output::Flush();
        }
#endif

        //
        // create a javascript array to hold info for all of the functions
        //
        Js::JavascriptArray *functionList = originalScriptContext->GetLibrary()->CreateArray(fnCount);

        int count = 0;
        for (scriptContext = threadContext->GetScriptContextList();
            scriptContext;
            scriptContext = scriptContext->next)
        {LOGMEIN("GlobalObject.cpp] 296\n");
            if (scriptContext->IsClosed()) continue;
            // if (scriptContext == originalScriptContext) continue;  // uncomment to ignore the originalScriptContext

            SourceList *sourceList = scriptContext->GetSourceList();
            sourceList->Map([&fnCount, &count, functionList, scriptContext](uint i, Utf8SourceInfoRef *sourceInfoWeakRef)
            {
                Js::Utf8SourceInfo *sourceInfo = sourceInfoWeakRef->Get();
                if (sourceInfo == nullptr || sourceInfo->GetIsLibraryCode()) // library code has no source, skip
                {LOGMEIN("GlobalObject.cpp] 305\n");
                    return;
                }

                const SRCINFO *srcInfo = sourceInfo->GetSrcInfo();
                SourceContextInfo *srcContextInfo = srcInfo->sourceContextInfo;

                //
                // get URL of source file
                //
                char16 filenameBuffer[128];  // hold dynamically built filename
                char16 const *srcFileUrl = NULL;
                if (!srcContextInfo->IsDynamic())
                {LOGMEIN("GlobalObject.cpp] 318\n");
                    Assert(srcContextInfo->url != NULL);
                    srcFileUrl = srcContextInfo->url;
                }
                else
                {
                    StringCchPrintf(filenameBuffer, 128, _u("[dynamic(hash:%u)]"), srcContextInfo->hash);
                    srcFileUrl = filenameBuffer;
                }

                sourceInfo->MapFunction([scriptContext, &count, functionList, srcFileUrl](Js::FunctionBody *functionBody)
                {
                    char16 const *functionName = functionBody->GetExternalDisplayName();

                    ULONG lineNum = functionBody->GetLineNumber();
                    ULONG colNum = functionBody->GetColumnNumber();

                    uint funcId = functionBody->GetLocalFunctionId();
                    Js::Utf8SourceInfo *utf8SrcInfo = functionBody->GetUtf8SourceInfo();
                    if (utf8SrcInfo == nullptr)
                    {LOGMEIN("GlobalObject.cpp] 338\n");
                        return;
                    }

                    Assert(utf8SrcInfo->FindFunction(funcId) == functionBody);

                    BufferStringBuilder builder(functionBody->LengthInChars(), scriptContext);
                    utf8::DecodeOptions options = utf8SrcInfo->IsCesu8() ? utf8::doAllowThreeByteSurrogates : utf8::doDefault;
                    utf8::DecodeInto(builder.DangerousGetWritableBuffer(), functionBody->GetSource(), functionBody->LengthInChars(), options);
                    Var cachedSourceString = builder.ToString();

                    Js::Var obj = FunctionInfoObjectBuilder(scriptContext, srcFileUrl,
                        functionName, lineNum, colNum, funcId, utf8SrcInfo, cachedSourceString);
                    functionList->SetItem(count, obj, Js::PropertyOperationFlags::PropertyOperation_None);

                    ++count;
                });
            });
        }

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("GlobalObject.cpp] 360\n");
            Output::Print(_u(">> Returning from functionList()\n"));
            Output::Flush();
        }
#endif

        return functionList;
    }

    /**
     * Return the parsed IR for the given function.
     */
    Var GlobalObject::EntryRejitFunction(RecyclableObject *function, CallInfo callInfo, ...)
    {
#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("GlobalObject.cpp] 376\n");
            Output::Print(_u(">> Entering rejitFunction()\n"));
            Output::Flush();
        }
#endif

        //
        // retrieve and coerce arguments
        //

        RUNTIME_ARGUMENTS(args, callInfo);  // args[0] is (callInfo)
        Js::Var jsVarUtf8SourceInfo = args[1];
        Js::Var jsVarFunctionId = args[2];

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("GlobalObject.cpp] 392\n");
            Output::Print(_u("jsVarUtf8SourceInfo: %d (0x%08X)\n"), jsVarUtf8SourceInfo, jsVarUtf8SourceInfo);
            Output::Print(_u("jsVarFunctionId: %d (0x%08X)\n"), jsVarFunctionId, jsVarFunctionId);
        }
#endif

        Js::JavascriptNumber *jsUtf8SourceInfoNumber = NULL;
        Js::JavascriptNumber *jsFunctionIdNumber = NULL;
        int32 utf8SourceInfoNumber = 0;  // null
        int32 functionIdNumber = -1;  // start with invalid function id

        // extract value of jsVarUtf8SourceInfo
        if (Js::TaggedInt::Is(jsVarUtf8SourceInfo))
        {LOGMEIN("GlobalObject.cpp] 405\n");
            utf8SourceInfoNumber = (int32)TaggedInt::ToInt64(jsVarUtf8SourceInfo); // REVIEW: just truncate?
        }
        else if (Js::JavascriptNumber::Is(jsVarUtf8SourceInfo))
        {LOGMEIN("GlobalObject.cpp] 409\n");
            jsUtf8SourceInfoNumber = (Js::JavascriptNumber *)jsVarUtf8SourceInfo;
            utf8SourceInfoNumber = (int32)JavascriptNumber::GetValue(jsUtf8SourceInfoNumber);    // REVIEW: just truncate?
        }
        else
        {
            // should not reach here
            AssertMsg(false, "Failed to extract value for jsVarUtf8SourceInfo as either TaggedInt or JavascriptNumber.\n");
        }

        // extract value of jsVarFunctionId
        if (Js::TaggedInt::Is(jsVarFunctionId))
        {LOGMEIN("GlobalObject.cpp] 421\n");
            functionIdNumber = (int32)TaggedInt::ToInt64(jsVarFunctionId); // REVIEW: just truncate?
        }
        else if (Js::JavascriptNumber::Is(jsVarFunctionId))
        {LOGMEIN("GlobalObject.cpp] 425\n");
            jsFunctionIdNumber = (Js::JavascriptNumber *)jsVarFunctionId;
            functionIdNumber = (int32)JavascriptNumber::GetValue(jsFunctionIdNumber); // REVIEW: just truncate?
        }
        else
        {
            // should not reach here
            AssertMsg(false, "Failed to extract value for jsVarFunctionId as either TaggedInt or JavascriptNumber.\n");
        }

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("GlobalObject.cpp] 437\n");
            Output::Print(_u("utf8SourceInfoNumber (value): %d (0x%08X)\n"), utf8SourceInfoNumber, utf8SourceInfoNumber);
            Output::Print(_u("jsVarFunctionId (value): %d (0x%08X)\n"), functionIdNumber, functionIdNumber);
            Output::Print(_u(">> Executing rejitFunction(%d, %d)\n"), utf8SourceInfoNumber, functionIdNumber);
            Output::Flush();
        }
#endif

        //
        // recover functionBody
        //

        Js::Utf8SourceInfo *sourceInfo = (Js::Utf8SourceInfo *)(utf8SourceInfoNumber);
        if (sourceInfo == NULL)
        {LOGMEIN("GlobalObject.cpp] 451\n");
            return NULL;
        }

        Js::FunctionBody *functionBody = sourceInfo->FindFunction((Js::LocalFunctionId)functionIdNumber);

        //
        // rejit the function body
        //

        Js::ScriptContext *scriptContext = function->GetScriptContext();
        NativeCodeGenerator *nativeCodeGenerator = scriptContext->GetNativeCodeGenerator();

        if (!functionBody || !functionBody->GetByteCode())
        {LOGMEIN("GlobalObject.cpp] 465\n");
            return scriptContext->GetLibrary()->GetUndefined();
        }

        Js::Var retVal = RejitIRViewerFunction(nativeCodeGenerator, functionBody, scriptContext);

        //
        // return the parsed IR object
        //

#ifdef ENABLE_IR_VIEWER_DBG_DUMP
        if (Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("GlobalObject.cpp] 477\n");
            Output::Print(_u("rejitFunction - retVal: 0x%08X\n"), retVal);
            Output::Flush();
        }
#endif

        return retVal;
    }

#endif /* IR_VIEWER */
    Var GlobalObject::EntryEvalHelper(ScriptContext* scriptContext, RecyclableObject* function, CallInfo callInfo, Js::Arguments& args)
    {LOGMEIN("GlobalObject.cpp] 488\n");
        FrameDisplay* environment = (FrameDisplay*)&NullFrameDisplay;
        ModuleID moduleID = kmodGlobal;
        BOOL strictMode = FALSE;
        // TODO: Handle call from global scope, strict mode
        BOOL isIndirect = FALSE;

        if (Js::CallInfo::isDirectEvalCall(args.Info.Flags))
        {LOGMEIN("GlobalObject.cpp] 496\n");
            // This was recognized as an eval call at compile time. The last one or two args are internal to us.
            // Argcount will be one of the following when called from global code
            //  - eval("...")     : argcount 3 : this, evalString, frameDisplay
            //  - eval.call("..."): argcount 2 : this(which is string) , frameDisplay
            if (args.Info.Count >= 2)
            {LOGMEIN("GlobalObject.cpp] 502\n");
                environment = (FrameDisplay*)(args[args.Info.Count - 1]);

                // Check for a module root passed from the caller. If it's there, use its module ID to compile the eval.
                // when called inside a module root, module root would be added before the frame display in above scenarios

                // ModuleRoot is optional
                //  - eval("...")     : argcount 3/4 : this, evalString , [module root], frameDisplay
                //  - eval.call("..."): argcount 2/3 : this(which is string) , [module root], frameDisplay

                strictMode = environment->GetStrictMode();

                if (args.Info.Count >= 3 && JavascriptOperators::GetTypeId(args[args.Info.Count - 2]) == TypeIds_ModuleRoot)
                {LOGMEIN("GlobalObject.cpp] 515\n");
                    moduleID = ((Js::ModuleRoot*)(RecyclableObject::FromVar(args[args.Info.Count - 2])))->GetModuleID();
                    args.Info.Count--;
                }
                args.Info.Count--;
            }
        }
        else
        {
            // This must be an indirect "eval" call that we didn't detect at compile time.
            // Pass null as the environment, which will force all lookups in the eval code
            // to use the root for the current module.
            // Also pass "null" for "this", which will force the callee to use the current module root.
            isIndirect = !PHASE_OFF1(Js::FastIndirectEvalPhase);
        }

        return GlobalObject::VEval(function->GetLibrary(), environment, moduleID, !!strictMode, !!isIndirect, args,
            /* isLibraryCode = */ false, /* registerDocument */ true, /*additionalGrfscr */ 0);
    }

    Var GlobalObject::EntryEvalRestrictedMode(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        RUNTIME_ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptLibrary* library = function->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        scriptContext->CheckEvalRestriction();

        return EntryEvalHelper(scriptContext, function, callInfo, args);
    }

    // This function is used to decipher eval function parameters and we don't want the stack arguments optimization by C++ compiler so turning off the optimization
    Var GlobalObject::EntryEval(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        RUNTIME_ARGUMENTS(args, callInfo);
        Assert(!(callInfo.Flags & CallFlags_New));

        JavascriptLibrary* library = function->GetLibrary();
        ScriptContext* scriptContext = library->GetScriptContext();

        return EntryEvalHelper(scriptContext, function, callInfo, args);
    }

    Var GlobalObject::VEval(JavascriptLibrary* library, FrameDisplay* environment, ModuleID moduleID, bool strictMode, bool isIndirect,
        Arguments& args, bool isLibraryCode, bool registerDocument, uint32 additionalGrfscr)
    {LOGMEIN("GlobalObject.cpp] 566\n");
        Assert(library);
        ScriptContext* scriptContext = library->GetScriptContext();

        unsigned argCount = args.Info.Count;

        AssertMsg(argCount > 0, "Should always have implicit 'this'");

        bool doRegisterDocument = registerDocument & !isLibraryCode;
        Var varThis = library->GetUndefined();

        if (argCount < 2)
        {LOGMEIN("GlobalObject.cpp] 578\n");
            return library->GetUndefined();
        }

        Var evalArg = args[1];
        if (!JavascriptString::Is(evalArg))
        {LOGMEIN("GlobalObject.cpp] 584\n");
            // "If x is not a string value, return x."
            return evalArg;
        }

        // It might happen that no script parsed on this context (scriptContext) till now,
        // so this Eval acts as the first source compile for scriptContext, transition to debugMode as needed
        scriptContext->TransitionToDebugModeIfFirstSource(/* utf8SourceInfo = */ nullptr);

        JavascriptString *argString = JavascriptString::FromVar(evalArg);
        ScriptFunction *pfuncScript;
        char16 const * sourceString = argString->GetSz();
        charcount_t sourceLen = argString->GetLength();
        FastEvalMapString key(sourceString, sourceLen, moduleID, strictMode, isLibraryCode);
        if (!scriptContext->IsInEvalMap(key, isIndirect, &pfuncScript))
        {LOGMEIN("GlobalObject.cpp] 599\n");
            uint32 grfscr = additionalGrfscr | fscrReturnExpression | fscrEval | fscrEvalCode | fscrGlobalCode;

            if (isLibraryCode)
            {LOGMEIN("GlobalObject.cpp] 603\n");
                grfscr |= fscrIsLibraryCode;
            }

            pfuncScript = library->GetGlobalObject()->EvalHelper(scriptContext, argString->GetSz(), argString->GetLength(), moduleID,
                grfscr, Constants::EvalCode, doRegisterDocument, isIndirect, strictMode);
            Assert(!pfuncScript->GetFunctionInfo()->IsGenerator());

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            Js::Utf8SourceInfo* utf8SourceInfo = pfuncScript->GetFunctionBody()->GetUtf8SourceInfo();
            if (scriptContext->IsScriptContextInDebugMode() && !utf8SourceInfo->GetIsLibraryCode() && !utf8SourceInfo->IsInDebugMode())
            {LOGMEIN("GlobalObject.cpp] 614\n");
                // Identifying if any non library function escaped for not being in debug mode.
                Throw::FatalInternalError();
            }
#endif

#if ENABLE_TTD
            if(!scriptContext->IsTTDRecordOrReplayModeEnabled())
            {LOGMEIN("GlobalObject.cpp] 622\n");
                scriptContext->AddToEvalMap(key, isIndirect, pfuncScript);
            }
#else
            scriptContext->AddToEvalMap(key, isIndirect, pfuncScript);
#endif
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        else
        {
            Js::Utf8SourceInfo* utf8SourceInfo = pfuncScript->GetFunctionBody()->GetUtf8SourceInfo();
            if (scriptContext->IsScriptContextInDebugMode() && !utf8SourceInfo->GetIsLibraryCode() && !utf8SourceInfo->IsInDebugMode())
            {LOGMEIN("GlobalObject.cpp] 635\n");
                // Identifying if any non library function escaped for not being in debug mode.
                Throw::FatalInternalError();
            }
        }
#endif

#if ENABLE_TTD
        //
        //TODO: We may (probably?) want to use the debugger source rundown functionality here instead
        //
        if(!isLibraryCode && (scriptContext->IsTTDRecordModeEnabled() || scriptContext->ShouldPerformReplayAction()))
        {LOGMEIN("GlobalObject.cpp] 647\n");
            //Make sure we have the body and text information available
            FunctionBody* globalBody = TTD::JsSupport::ForceAndGetFunctionBody(pfuncScript->GetParseableFunctionInfo());
            if(!scriptContext->TTDContextInfo->IsBodyAlreadyLoadedAtTopLevel(globalBody))
            {LOGMEIN("GlobalObject.cpp] 651\n");
                uint64 bodyIdCtr = 0;

                if(scriptContext->IsTTDRecordModeEnabled())
                {LOGMEIN("GlobalObject.cpp] 655\n");
                    const TTD::NSSnapValues::TopLevelEvalFunctionBodyResolveInfo* tbfi = scriptContext->GetThreadContext()->TTDLog->AddEvalFunction(globalBody, moduleID, sourceString, sourceLen, additionalGrfscr, registerDocument, isIndirect, strictMode);

                    //We always want to register the top-level load but we don't always need to log the event
                    if(scriptContext->ShouldPerformRecordAction())
                    {LOGMEIN("GlobalObject.cpp] 660\n");
                        scriptContext->GetThreadContext()->TTDLog->RecordTopLevelCodeAction(tbfi->TopLevelBase.TopLevelBodyCtr);
                    }

                    bodyIdCtr = tbfi->TopLevelBase.TopLevelBodyCtr;
                }

                if(scriptContext->ShouldPerformReplayAction())
                {LOGMEIN("GlobalObject.cpp] 668\n");
                    bodyIdCtr = scriptContext->GetThreadContext()->TTDLog->ReplayTopLevelCodeAction();
                }

                //walk global body to (1) add functions to pin set (2) build parent map
                scriptContext->TTDContextInfo->ProcessFunctionBodyOnLoad(globalBody, nullptr);
                scriptContext->TTDContextInfo->RegisterEvalScript(globalBody, bodyIdCtr);
            }
        }
#endif

        //We shouldn't be serializing eval functions; unless with -ForceSerialized flag
        if (CONFIG_FLAG(ForceSerialized)) {LOGMEIN("GlobalObject.cpp] 680\n");
            pfuncScript->GetFunctionProxy()->EnsureDeserialized();
        }

        if (pfuncScript->GetFunctionBody()->GetHasThis())
        {LOGMEIN("GlobalObject.cpp] 685\n");
            // The eval expression refers to "this"
            if (args.Info.Flags & CallFlags_ExtraArg)
            {LOGMEIN("GlobalObject.cpp] 688\n");
                JavascriptFunction* pfuncCaller;
                JavascriptStackWalker::GetCaller(&pfuncCaller, scriptContext);
                // If we are non-hidden call to eval then look for the "this" object in the frame display if the caller is a lambda else get "this" from the caller's frame.

                FunctionInfo* functionInfo = pfuncCaller->GetFunctionInfo();
                if (functionInfo != nullptr && (functionInfo->IsLambda() || functionInfo->IsClassConstructor()))
                {LOGMEIN("GlobalObject.cpp] 695\n");
                    Var defaultInstance = (moduleID == kmodGlobal) ? JavascriptOperators::OP_LdRoot(scriptContext)->ToThis() : (Var)JavascriptOperators::GetModuleRoot(moduleID, scriptContext);
                    varThis = JavascriptOperators::OP_GetThisScoped(environment, defaultInstance, scriptContext);
                    UpdateThisForEval(varThis, moduleID, scriptContext, strictMode);
                }
                else
                {
                    JavascriptStackWalker::GetThis(&varThis, moduleID, scriptContext);
                    UpdateThisForEval(varThis, moduleID, scriptContext, strictMode);
                }
            }
            else
            {
                // The expression, which refers to "this", is evaluated by an indirect eval.
                // Set "this" to the current module root.
                varThis = JavascriptOperators::OP_GetThis(scriptContext->GetLibrary()->GetUndefined(), moduleID, scriptContext);
            }
        }

        if (pfuncScript->HasSuperReference())
        {LOGMEIN("GlobalObject.cpp] 715\n");
            // Indirect evals cannot have a super reference.
            if (!(args.Info.Flags & CallFlags_ExtraArg))
            {LOGMEIN("GlobalObject.cpp] 718\n");
                JavascriptError::ThrowSyntaxError(scriptContext, ERRSuperInIndirectEval, _u("super"));
            }
        }

        return library->GetGlobalObject()->ExecuteEvalParsedFunction(pfuncScript, environment, varThis);
    }

    void GlobalObject::UpdateThisForEval(Var &varThis, ModuleID moduleID, ScriptContext *scriptContext, BOOL strictMode)
    {LOGMEIN("GlobalObject.cpp] 727\n");
        if (strictMode)
        {LOGMEIN("GlobalObject.cpp] 729\n");
            varThis = JavascriptOperators::OP_StrictGetThis(varThis, scriptContext);
        }
        else
        {
            varThis = JavascriptOperators::OP_GetThisNoFastPath(varThis, moduleID, scriptContext);
        }
    }


    Var GlobalObject::ExecuteEvalParsedFunction(ScriptFunction *pfuncScript, FrameDisplay* environment, Var &varThis)
    {LOGMEIN("GlobalObject.cpp] 740\n");
        Assert(pfuncScript != nullptr);

        pfuncScript->SetEnvironment(environment);
        //This function is supposed to be deserialized
        Assert(pfuncScript->GetFunctionBody());
        if (pfuncScript->GetFunctionBody()->GetFuncEscapes())
        {LOGMEIN("GlobalObject.cpp] 747\n");
            // Executing the eval causes the scope chain to escape.
            pfuncScript->InvalidateCachedScopeChain();
        }
        Var varResult = CALL_FUNCTION(pfuncScript, CallInfo(CallFlags_Eval, 1), varThis);
        pfuncScript->SetEnvironment(nullptr);
        return varResult;
    }

#ifdef ENABLE_SCRIPT_PROFILING
    ScriptFunction* GlobalObject::ProfileModeEvalHelper(ScriptContext* scriptContext, const char16 *source, int sourceLength, ModuleID moduleID, uint32 grfscr, LPCOLESTR pszTitle, BOOL registerDocument, BOOL isIndirect, BOOL strictMode)
    {LOGMEIN("GlobalObject.cpp] 758\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        ScriptFunction *pEvalFunction = DefaultEvalHelper(scriptContext, source, sourceLength, moduleID, grfscr, pszTitle, registerDocument, isIndirect, strictMode);
        Assert(pEvalFunction);
        Js::FunctionProxy *proxy = pEvalFunction->GetFunctionProxy();
        Assert(proxy);

        OUTPUT_TRACE(Js::ScriptProfilerPhase, _u("GlobalObject::ProfileModeEvalHelper FunctionNumber : %s, Entrypoint : 0x%08X IsFunctionDefer : %d\n"),
                                    proxy->GetDebugNumberSet(debugStringBuffer), pEvalFunction->GetEntryPoint(), proxy->IsDeferred());

        if (proxy->IsDeferred())
        {LOGMEIN("GlobalObject.cpp] 771\n");
            // This could happen if the top level function is marked as deferred, we need to parse this to generate the script compile information (RegisterScript depends on that)
            Js::JavascriptFunction::DeferredParse(&pEvalFunction);
            proxy = pEvalFunction->GetFunctionProxy();
        }

        scriptContext->RegisterScript(proxy);

        return pEvalFunction;
    }
#endif

    void GlobalObject::ValidateSyntax(ScriptContext* scriptContext, const char16 *source, int sourceLength, bool isGenerator, bool isAsync, void (Parser::*validateSyntax)())
    {LOGMEIN("GlobalObject.cpp] 784\n");
        Assert(sourceLength >= 0);

        HRESULT hr = S_OK;
        HRESULT hrParser = E_FAIL;
        CompileScriptException se;

        BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext);
        BEGIN_TRANSLATE_EXCEPTION_TO_HRESULT
        {
            ArenaAllocator tempAlloc(_u("ValidateSyntaxArena"), scriptContext->GetThreadContext()->GetPageAllocator(), Throw::OutOfMemory);

            size_t cchSource = sourceLength;
            size_t cbUtf8Buffer = (cchSource + 1) * 3;
            LPUTF8 utf8Source = AnewArray(&tempAlloc, utf8char_t, cbUtf8Buffer);
            Assert(cchSource < MAXLONG);
            size_t cbSource = utf8::EncodeIntoAndNullTerminate(utf8Source, source, static_cast< charcount_t >(cchSource));
            utf8Source = reinterpret_cast< LPUTF8 >( tempAlloc.Realloc(utf8Source, cbUtf8Buffer, cbSource + 1) );

            Parser parser(scriptContext);
            hrParser = parser.ValidateSyntax(utf8Source, cbSource, isGenerator, isAsync, &se, validateSyntax);
        }
        END_TRANSLATE_EXCEPTION_TO_HRESULT(hr);
        END_LEAVE_SCRIPT_INTERNAL(scriptContext);

        if (FAILED(hr))
        {LOGMEIN("GlobalObject.cpp] 810\n");
            if (hr == E_OUTOFMEMORY)
            {LOGMEIN("GlobalObject.cpp] 812\n");
                JavascriptError::ThrowOutOfMemoryError(scriptContext);
            }
            if (hr == VBSERR_OutOfStack)
            {LOGMEIN("GlobalObject.cpp] 816\n");
                JavascriptError::ThrowStackOverflowError(scriptContext);
            }
        }
        if (!SUCCEEDED(hrParser))
        {LOGMEIN("GlobalObject.cpp] 821\n");
            hrParser = SCRIPT_E_RECORDED;
            EXCEPINFO ei;
            se.GetError(&hrParser, &ei);

            ErrorTypeEnum errorType;
            switch ((HRESULT)ei.scode)
            {LOGMEIN("GlobalObject.cpp] 828\n");
    #define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
            case name: \
                errorType = jst; \
                break;
    #define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
    #include "rterrors.h"
    #undef RT_PUBLICERROR_MSG
    #undef RT_ERROR_MSG
            default:
                errorType = kjstSyntaxError;
            }
            JavascriptError::MapAndThrowError(scriptContext, ei.scode, errorType, &ei);
        }
    }

    ScriptFunction* GlobalObject::DefaultEvalHelper(ScriptContext* scriptContext, const char16 *source, int sourceLength, ModuleID moduleID, uint32 grfscr, LPCOLESTR pszTitle, BOOL registerDocument, BOOL isIndirect, BOOL strictMode)
    {LOGMEIN("GlobalObject.cpp] 845\n");
        Assert(sourceLength >= 0);
        AnalysisAssert(scriptContext);
        if (scriptContext->GetThreadContext()->EvalDisabled())
        {LOGMEIN("GlobalObject.cpp] 849\n");
            throw Js::EvalDisabledException();
        }

#ifdef PROFILE_EXEC
        scriptContext->ProfileBegin(Js::EvalCompilePhase);
#endif
        void * frameAddr = nullptr;
        GET_CURRENT_FRAME_ID(frameAddr);

        HRESULT hr = S_OK;
        HRESULT hrParser = S_OK;
        HRESULT hrCodeGen = S_OK;
        CompileScriptException se;
        Js::ParseableFunctionInfo * funcBody = NULL;

        BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext);
        BEGIN_TRANSLATE_EXCEPTION_TO_HRESULT
        {
            uint cchSource = sourceLength;
            size_t cbUtf8Buffer = (cchSource + 1) * 3;

            ArenaAllocator tempArena(_u("EvalHelperArena"), scriptContext->GetThreadContext()->GetPageAllocator(), Js::Throw::OutOfMemory);
            LPUTF8 utf8Source = AnewArray(&tempArena, utf8char_t, cbUtf8Buffer);

            Assert(cchSource < MAXLONG);
            size_t cbSource = utf8::EncodeIntoAndNullTerminate(utf8Source, source, static_cast< charcount_t >(cchSource));
            Assert(cbSource + 1 <= cbUtf8Buffer);

            SRCINFO const * pSrcInfo = scriptContext->GetModuleSrcInfo(moduleID);
            // Source Info objects are kept alive by the function bodies that are referencing it
            // The function body is created in GenerateByteCode but the source info isn't passed in, only the index
            // So we need to pin it here (TODO: Change GenerateByteCode to take in the sourceInfo itself)
            ENTER_PINNED_SCOPE(Utf8SourceInfo, sourceInfo);
            sourceInfo = Utf8SourceInfo::New(scriptContext, utf8Source, cchSource,
              cbSource, pSrcInfo, ((grfscr & fscrIsLibraryCode) != 0), nullptr);

            Parser parser(scriptContext, strictMode);
            bool forceNoNative = false;

            ParseNodePtr parseTree;

            SourceContextInfo * sourceContextInfo = pSrcInfo->sourceContextInfo;
            ULONG deferParseThreshold = Parser::GetDeferralThreshold(sourceContextInfo->IsSourceProfileLoaded());
            if ((ULONG)sourceLength > deferParseThreshold && !PHASE_OFF1(Phase::DeferParsePhase))
            {LOGMEIN("GlobalObject.cpp] 894\n");
                // Defer function bodies declared inside large dynamic blocks.
                grfscr |= fscrDeferFncParse;
            }

            grfscr = grfscr | fscrDynamicCode;

            // fscrEval signifies direct eval in parser
            hrParser = parser.ParseCesu8Source(&parseTree, utf8Source, cbSource, isIndirect ? grfscr & ~fscrEval : grfscr, &se, &sourceContextInfo->nextLocalFunctionId,
                sourceContextInfo);
            sourceInfo->SetParseFlags(grfscr);

            if (SUCCEEDED(hrParser) && parseTree)
            {LOGMEIN("GlobalObject.cpp] 907\n");
                // This keeps function bodies generated by the byte code alive till we return
                Js::AutoDynamicCodeReference dynamicFunctionReference(scriptContext);

                Assert(cchSource < MAXLONG);
                uint sourceIndex = scriptContext->SaveSourceNoCopy(sourceInfo, cchSource, true);

                // Tell byte code gen not to attempt to interact with the caller's context if this is indirect eval.
                // TODO: Handle strict mode.
                if (isIndirect &&
                    !strictMode &&
                    !parseTree->sxFnc.GetStrictMode())
                {LOGMEIN("GlobalObject.cpp] 919\n");
                    grfscr &= ~fscrEval;
                }
                hrCodeGen = GenerateByteCode(parseTree, grfscr, scriptContext, &funcBody, sourceIndex, forceNoNative, &parser, &se);
                sourceInfo->SetByteCodeGenerationFlags(grfscr);
            }

            LEAVE_PINNED_SCOPE();
        }
        END_TRANSLATE_EXCEPTION_TO_HRESULT(hr);
        END_LEAVE_SCRIPT_INTERNAL(scriptContext);


#ifdef PROFILE_EXEC
        scriptContext->ProfileEnd(Js::EvalCompilePhase);
#endif
        THROW_KNOWN_HRESULT_EXCEPTIONS(hr, scriptContext);

        if (!SUCCEEDED(hrParser))
        {LOGMEIN("GlobalObject.cpp] 938\n");
            JavascriptError::ThrowParserError(scriptContext, hrParser, &se);
        }
        else if (!SUCCEEDED(hrCodeGen))
        {LOGMEIN("GlobalObject.cpp] 942\n");
            Assert(hrCodeGen == SCRIPT_E_RECORDED);
            hrCodeGen = se.ei.scode;
            /*
             * VBSERR_OutOfStack is of type kjstError but we throw a (more specific) StackOverflowError when a hard stack
             * overflow occurs. To keep the behavior consistent I'm special casing it here.
             */
            se.Free();
            if (hrCodeGen == VBSERR_OutOfStack)
            {LOGMEIN("GlobalObject.cpp] 951\n");
                JavascriptError::ThrowStackOverflowError(scriptContext);
            }
            JavascriptError::MapAndThrowError(scriptContext, hrCodeGen);
        }
        else
        {
            if (se.ei.scode == JSERR_AsmJsCompileError)
            {LOGMEIN("GlobalObject.cpp] 959\n");
                // if asm.js compilation succeeded, retry with asm.js disabled
                grfscr |= fscrNoAsmJs;
                se.Clear();
                return DefaultEvalHelper(scriptContext, source, sourceLength, moduleID, grfscr, pszTitle, registerDocument, isIndirect, strictMode);
            }

            Assert(funcBody != nullptr);
            funcBody->SetDisplayName(pszTitle);

            // Set the functionbody information to dynamic content PROFILER_SCRIPT_TYPE_DYNAMIC
            funcBody->SetIsTopLevel(true);

            // If not library code then let's find the parent, we may need to register this source if any exception happens later
            if ((grfscr & fscrIsLibraryCode) == 0)
            {LOGMEIN("GlobalObject.cpp] 974\n");
                // For parented eval get the caller's utf8SourceInfo
                JavascriptFunction* pfuncCaller;
                if (JavascriptStackWalker::GetCaller(&pfuncCaller, scriptContext) && pfuncCaller && pfuncCaller->IsScriptFunction())
                {LOGMEIN("GlobalObject.cpp] 978\n");
                    FunctionBody* parentFuncBody = pfuncCaller->GetFunctionBody();
                    Utf8SourceInfo* parentUtf8SourceInfo = parentFuncBody->GetUtf8SourceInfo();
                    Utf8SourceInfo* utf8SourceInfo = funcBody->GetUtf8SourceInfo();
                    utf8SourceInfo->SetCallerUtf8SourceInfo(parentUtf8SourceInfo);
                }
            }

            if (registerDocument)
            {LOGMEIN("GlobalObject.cpp] 987\n");
                funcBody->RegisterFuncToDiag(scriptContext, pszTitle);
                funcBody = funcBody->GetParseableFunctionInfo(); // RegisterFunction may parse and update function body
            }

            ScriptFunction* pfuncScript = funcBody->IsCoroutine() ?
                scriptContext->GetLibrary()->CreateGeneratorVirtualScriptFunction(funcBody) :
                scriptContext->GetLibrary()->CreateScriptFunction(funcBody);

            return pfuncScript;
        }
    }

#ifdef IR_VIEWER
    Var GlobalObject::IRDumpEvalHelper(ScriptContext* scriptContext, const char16 *source,
        int sourceLength, ModuleID moduleID, uint32 grfscr, LPCOLESTR pszTitle,
        BOOL registerDocument, BOOL isIndirect, BOOL strictMode)
    {LOGMEIN("GlobalObject.cpp] 1004\n");
        // TODO (t-doilij) clean up this function, specifically used for IR dump (don't execute bytecode; potentially dangerous)

        Assert(sourceLength >= 0);

        if (scriptContext->GetThreadContext()->EvalDisabled())
        {LOGMEIN("GlobalObject.cpp] 1010\n");
            throw Js::EvalDisabledException();
        }

#ifdef PROFILE_EXEC
        scriptContext->ProfileBegin(Js::EvalCompilePhase);
#endif
        void * frameAddr = nullptr;
        GET_CURRENT_FRAME_ID(frameAddr);

        HRESULT hr = S_OK;
        HRESULT hrParser = S_OK;
        HRESULT hrCodeGen = S_OK;
        CompileScriptException se;
        Js::ParseableFunctionInfo * funcBody = NULL;

        BEGIN_LEAVE_SCRIPT_INTERNAL(scriptContext);
        BEGIN_TRANSLATE_EXCEPTION_TO_HRESULT
        {
            size_t cchSource = sourceLength;
            size_t cbUtf8Buffer = (cchSource + 1) * 3;

            ArenaAllocator tempArena(_u("EvalHelperArena"), scriptContext->GetThreadContext()->GetPageAllocator(), Js::Throw::OutOfMemory);
            LPUTF8 utf8Source = AnewArray(&tempArena, utf8char_t, cbUtf8Buffer);

            Assert(cchSource < MAXLONG);
            size_t cbSource = utf8::EncodeIntoAndNullTerminate(utf8Source, source, static_cast< charcount_t >(cchSource));
            Assert(cbSource + 1 <= cbUtf8Buffer);

            SRCINFO const * pSrcInfo = scriptContext->GetModuleSrcInfo(moduleID);
            // Source Info objects are kept alive by the function bodies that are referencing it
            // The function body is created in GenerateByteCode but the source info isn't passed in, only the index
            // So we need to pin it here (TODO: Change GenerateByteCode to take in the sourceInfo itself)
            ENTER_PINNED_SCOPE(Utf8SourceInfo, sourceInfo);
            sourceInfo = Utf8SourceInfo::New(scriptContext, utf8Source, cchSource, cbSource, pSrcInfo, ((grfscr & fscrIsLibraryCode) != 0));

            Parser parser(scriptContext, strictMode);
            bool forceNoNative = false;

            ParseNodePtr parseTree;

            SourceContextInfo * sourceContextInfo = pSrcInfo->sourceContextInfo;

            grfscr = grfscr | fscrDynamicCode;

            hrParser = parser.ParseCesu8Source(&parseTree, utf8Source, cbSource, grfscr, &se, &sourceContextInfo->nextLocalFunctionId,
                sourceContextInfo);
            sourceInfo->SetParseFlags(grfscr);

            if (SUCCEEDED(hrParser) && parseTree)
            {LOGMEIN("GlobalObject.cpp] 1060\n");
                // This keeps function bodies generated by the byte code alive till we return
                Js::AutoDynamicCodeReference dynamicFunctionReference(scriptContext);

                Assert(cchSource < MAXLONG);
                uint sourceIndex = scriptContext->SaveSourceNoCopy(sourceInfo, cchSource, true);

                grfscr = grfscr | fscrIrDumpEnable;

                hrCodeGen = GenerateByteCode(parseTree, grfscr, scriptContext, &funcBody, sourceIndex, forceNoNative, &parser, &se);
                sourceInfo->SetByteCodeGenerationFlags(grfscr);
            }

            LEAVE_PINNED_SCOPE();
        }
        END_TRANSLATE_EXCEPTION_TO_HRESULT(hr);
        END_LEAVE_SCRIPT_INTERNAL(scriptContext);


#ifdef PROFILE_EXEC
        scriptContext->ProfileEnd(Js::EvalCompilePhase);
#endif
        THROW_KNOWN_HRESULT_EXCEPTIONS(hr);

        if (!SUCCEEDED(hrParser))
        {LOGMEIN("GlobalObject.cpp] 1085\n");
            hrParser = SCRIPT_E_RECORDED;
            EXCEPINFO ei;
            se.GetError(&hrParser, &ei);

            ErrorTypeEnum errorType;
            switch ((HRESULT)ei.scode)
            {LOGMEIN("GlobalObject.cpp] 1092\n");
    #define RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) \
            case name: \
                errorType = jst; \
                break;
    #define RT_PUBLICERROR_MSG(name, errnum, str1, str2, jst, errorNumSource) RT_ERROR_MSG(name, errnum, str1, str2, jst, errorNumSource)
    #include "rterrors.h"
    #undef RT_PUBLICERROR_MSG
    #undef RT_ERROR_MSG
            default:
                errorType = kjstSyntaxError;
            }
            JavascriptError::MapAndThrowError(scriptContext, ei.scode, errorType, &ei);
        }
        else if (!SUCCEEDED(hrCodeGen))
        {LOGMEIN("GlobalObject.cpp] 1107\n");
            Assert(hrCodeGen == SCRIPT_E_RECORDED);
            hrCodeGen = se.ei.scode;
            /*
             * VBSERR_OutOfStack is of type kjstError but we throw a (more specific) StackOverflowError when a hard stack
             * overflow occurs. To keep the behavior consistent I'm special casing it here.
             */
            se.Free();
            if (hrCodeGen == VBSERR_OutOfStack)
            {LOGMEIN("GlobalObject.cpp] 1116\n");
                JavascriptError::ThrowStackOverflowError(scriptContext);
            }
            JavascriptError::MapAndThrowError(scriptContext, hrCodeGen);
        }
        else
        {
            Assert(funcBody != nullptr);
            funcBody->SetDisplayName(pszTitle);

            // Set the functionbody information to dynamic content PROFILER_SCRIPT_TYPE_DYNAMIC
            funcBody->SetIsTopLevel(true);

            if (registerDocument)
            {LOGMEIN("GlobalObject.cpp] 1130\n");
                funcBody->RegisterFuncToDiag(scriptContext, pszTitle);
                funcBody = funcBody->GetParseableFunctionInfo(); // RegisterFunction may parse and update function body
            }

            NativeCodeGenerator *nativeCodeGenerator = scriptContext->GetNativeCodeGenerator();
            Js::FunctionBody *functionBody = funcBody->GetFunctionBody(); // funcBody is ParseableFunctionInfo

            if (!functionBody || !functionBody->GetByteCode())
            {LOGMEIN("GlobalObject.cpp] 1139\n");
                return scriptContext->GetLibrary()->GetUndefined();
            }

            Js::Var retVal = RejitIRViewerFunction(nativeCodeGenerator, functionBody, scriptContext);
            return retVal;
        }
    }
#endif /* IR_VIEWER */

    Var GlobalObject::EntryParseInt(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        JavascriptString *str;
        int radix = 0;


        if(args.Info.Count < 2)
        {LOGMEIN("GlobalObject.cpp] 1165\n");
            // We're really converting "undefined" here, but "undefined" produces
            // NaN, so just return that directly.
            return JavascriptNumber::ToVarNoCheck(JavascriptNumber::NaN, scriptContext);
        }


        // Short cut for numbers and radix 10
        if (((args.Info.Count == 2) ||
            (TaggedInt::Is(args[2]) && TaggedInt::ToInt32(args[2]) == 10) ||
            (JavascriptNumber::Is(args[2]) && JavascriptNumber::GetValue(args[2]) == 10)))
        {LOGMEIN("GlobalObject.cpp] 1176\n");
            if (TaggedInt::Is(args[1]))
            {LOGMEIN("GlobalObject.cpp] 1178\n");
                return args[1];
            }
            if (JavascriptNumber::Is(args[1]))
            {LOGMEIN("GlobalObject.cpp] 1182\n");
                double value = JavascriptNumber::GetValue(args[1]);

                // make sure we are in the ranges that don't have exponential notation.
                double absValue = Math::Abs(value);
                if (absValue < 1.0e21 && absValue >= 1e-5)
                {LOGMEIN("GlobalObject.cpp] 1188\n");
                    double result;
#pragma prefast(suppress:6031, "We don't care about the fraction part")
                    ::modf(value, &result);
                    return JavascriptNumber::ToVarIntCheck(result, scriptContext);
                }
            }
        }

        // convert input to a string
        if (JavascriptString::Is(args[1]))
        {LOGMEIN("GlobalObject.cpp] 1199\n");
            str = JavascriptString::FromVar(args[1]);
        }
        else
        {
            str = JavascriptConversion::ToString(args[1], scriptContext);
        }

        if(args.Info.Count > 2)
        {LOGMEIN("GlobalObject.cpp] 1208\n");
            // retrieve the radix
            // TODO : verify for ES5 : radix is 10 when undefined or 0, except when it starts with 0x when it is 16.
            radix = JavascriptConversion::ToInt32(args[2], scriptContext);
            if(radix < 0 || radix == 1 || radix > 36)
            {LOGMEIN("GlobalObject.cpp] 1213\n");
                //illegal, return NaN
                return JavascriptNumber::ToVarNoCheck(JavascriptNumber::NaN, scriptContext);
            }
        }

        Var result = str->ToInteger(radix);
        return result;
    }

    Var GlobalObject::EntryParseFloat(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        JavascriptString *str;

        if(args.Info.Count < 2)
        {LOGMEIN("GlobalObject.cpp] 1237\n");
            // We're really converting "undefined" here, but "undefined" produces
            // NaN, so just return that directly.
            return JavascriptNumber::ToVarNoCheck(JavascriptNumber::NaN, scriptContext);
        }

        // Short cut for numbers
        if (TaggedInt::Is(args[1]))
        {LOGMEIN("GlobalObject.cpp] 1245\n");
            return args[1];
        }
        if(JavascriptNumber::Is_NoTaggedIntCheck(args[1]))
        {LOGMEIN("GlobalObject.cpp] 1249\n");
            // If the value is negative zero, return positive zero. Since the parameter type is string, -0 would first be
            // converted to the string "0", then parsed to result in positive zero.
            return
                JavascriptNumber::IsNegZero(JavascriptNumber::GetValue(args[1])) ?
                    TaggedInt::ToVarUnchecked(0) :
                    args[1];
        }

        // convert input to a string
        if (JavascriptString::Is(args[1]))
        {LOGMEIN("GlobalObject.cpp] 1260\n");
            str = JavascriptString::FromVar(args[1]);
        }
        else
        {
            str = JavascriptConversion::ToString(args[1], scriptContext);
        }

        // skip preceding whitespace
        const char16 *pch = scriptContext->GetCharClassifier()->SkipWhiteSpace(str->GetSz());

        // perform the string -> float conversion
        double result = NumberUtilities::StrToDbl(pch, &pch, scriptContext);

        return JavascriptNumber::ToVarNoCheck(result, scriptContext);
    }

    Var GlobalObject::EntryIsNaN(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        if(args.Info.Count < 2)
        {LOGMEIN("GlobalObject.cpp] 1289\n");
            return scriptContext->GetLibrary()->GetTrue();
        }
        return JavascriptBoolean::ToVar(
            JavascriptNumber::IsNan(JavascriptConversion::ToNumber(args[1],scriptContext)),
            scriptContext);
    }

    Var GlobalObject::EntryIsFinite(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        if(args.Info.Count < 2)
        {LOGMEIN("GlobalObject.cpp] 1308\n");
            return scriptContext->GetLibrary()->GetFalse();
        }
        return JavascriptBoolean::ToVar(
            NumberUtilities::IsFinite(JavascriptConversion::ToNumber(args[1],scriptContext)),
            scriptContext);
    }

    Var GlobalObject::EntryDecodeURI(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        return UriHelper::DecodeCoreURI(scriptContext, args, UriHelper::URIReserved);
    }

    Var GlobalObject::EntryDecodeURIComponent(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        return UriHelper::DecodeCoreURI(scriptContext, args, UriHelper::URINone);
    }

    Var GlobalObject::EntryEncodeURI(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
        return UriHelper::EncodeCoreURI(scriptContext, args, UriHelper::URIReserved | UriHelper::URIUnescaped);
    }

    Var GlobalObject::EntryEncodeURIComponent(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        return UriHelper::EncodeCoreURI(scriptContext, args, UriHelper::URIUnescaped);
    }

    //
    // Part of Appendix B of ES5 spec
    //

    // This is a bit field for the hex values: 00-29, 2C, 3A-3F, 5B-5E, 60, 7B-FF
    // These are the values escape encodes using the default mask (or mask >= 4)
    static const BYTE _grfbitEscape[] =
    {
        0xFF, 0xFF, // 00 - 0F
        0xFF, 0xFF, // 10 - 1F
        0xFF, 0x13, // 20 - 2F
        0x00, 0xFC, // 30 - 3F
        0x00, 0x00, // 40 - 4F
        0x00, 0x78, // 50 - 5F
        0x01, 0x00, // 60 - 6F
        0x00, 0xF8, // 70 - 7F
        0xFF, 0xFF, // 80 - 8F
        0xFF, 0xFF, // 90 - 9F
        0xFF, 0xFF, // A0 - AF
        0xFF, 0xFF, // B0 - BF
        0xFF, 0xFF, // C0 - CF
        0xFF, 0xFF, // D0 - DF
        0xFF, 0xFF, // E0 - EF
        0xFF, 0xFF, // F0 - FF
    };
    Var GlobalObject::EntryEscape(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count <= 1)
        {LOGMEIN("GlobalObject.cpp] 1404\n");
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }

        JavascriptString *src = JavascriptConversion::ToString(args[1], scriptContext);

        CompoundString *const bs = CompoundString::NewWithCharCapacity(src->GetLength(), scriptContext->GetLibrary());
        char16 chw;
        char16 * pchSrc;
        char16 * pchLim;

        const char _rgchHex[] = "0123456789ABCDEF";

        pchSrc = const_cast<char16 *>(src->GetString());
        pchLim = pchSrc + src->GetLength();
        while (pchSrc < pchLim)
        {LOGMEIN("GlobalObject.cpp] 1420\n");
            chw = *pchSrc++;

            if (0 != (chw & 0xFF00))
            {LOGMEIN("GlobalObject.cpp] 1424\n");
                bs->AppendChars(_u("%u"));
                bs->AppendChars(static_cast<char16>(_rgchHex[(chw >> 12) & 0x0F]));
                bs->AppendChars(static_cast<char16>(_rgchHex[(chw >> 8) & 0x0F]));
                bs->AppendChars(static_cast<char16>(_rgchHex[(chw >> 4) & 0x0F]));
                bs->AppendChars(static_cast<char16>(_rgchHex[chw & 0x0F]));
            }
            else if (_grfbitEscape[chw >> 3] & (1 << (chw & 7)))
            {LOGMEIN("GlobalObject.cpp] 1432\n");
                // Escape the byte.
                bs->AppendChars(_u('%'));
                bs->AppendChars(static_cast<char16>(_rgchHex[chw >> 4]));
                bs->AppendChars(static_cast<char16>(_rgchHex[chw & 0x0F]));
            }
            else
            {
                bs->AppendChars(chw);
            }
        }

        return bs;
    }

    //
    // Part of Appendix B of ES5 spec
    //
    Var GlobalObject::EntryUnEscape(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count <= 1)
        {LOGMEIN("GlobalObject.cpp] 1460\n");
            return scriptContext->GetLibrary()->GetUndefinedDisplayString();
        }

        char16 chw;
        char16 chT;
        char16 * pchSrc;
        char16 * pchLim;
        char16 * pchMin;

        JavascriptString *src = JavascriptConversion::ToString(args[1], scriptContext);

        CompoundString *const bs = CompoundString::NewWithCharCapacity(src->GetLength(), scriptContext->GetLibrary());

        pchSrc = const_cast<char16 *>(src->GetString());
        pchLim = pchSrc + src->GetLength();
        while (pchSrc < pchLim)
        {LOGMEIN("GlobalObject.cpp] 1477\n");
            if ('%' != (chT = *pchSrc++))
            {LOGMEIN("GlobalObject.cpp] 1479\n");
                bs->AppendChars(chT);
                continue;
            }

            pchMin = pchSrc;
            chT = *pchSrc++;
            if ('u' != chT)
            {LOGMEIN("GlobalObject.cpp] 1487\n");
                chw = 0;
                goto LTwoHexDigits;
            }

            // 4 hex digits.
            if ((chT = *pchSrc++ - '0') <= 9)
                chw = chT * 0x1000;
            else if ((chT -= 'A' - '0') <= 5 || (chT -= 'a' - 'A') <= 5)
                chw = (chT + 10) * 0x1000;
            else
                goto LHexError;
            if ((chT = *pchSrc++ - '0') <= 9)
                chw += chT * 0x0100;
            else if ((chT -= 'A' - '0') <= 5 || (chT -= 'a' - 'A') <= 5)
                chw += (chT + 10) * 0x0100;
            else
                goto LHexError;
            chT = *pchSrc++;
LTwoHexDigits:
            if ((chT -= '0') <= 9)
                chw += chT * 0x0010;
            else if ((chT -= 'A' - '0') <= 5 || (chT -= 'a' - 'A') <= 5)
                chw += (chT + 10) * 0x0010;
            else
                goto LHexError;
            if ((chT = *pchSrc++ - '0') <= 9)
                chw += chT;
            else if ((chT -= 'A' - '0') <= 5 || (chT -= 'a' - 'A') <= 5)
                chw += chT + 10;
            else
            {LOGMEIN("GlobalObject.cpp] 1518\n");
LHexError:
                pchSrc = pchMin;
                chw = '%';
            }

            bs->AppendChars(chw);
        }

        return bs;
    }

#if DBG
    void DebugClearStack()
    {LOGMEIN("GlobalObject.cpp] 1532\n");
        char buffer[1024];
        memset(buffer, 0, sizeof(buffer));
    }
#endif

    Var GlobalObject::EntryCollectGarbage(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

#if DBG_DUMP
        if (Js::Configuration::Global.flags.TraceWin8Allocations)
        {LOGMEIN("GlobalObject.cpp] 1544\n");
            Output::Print(_u("MemoryTrace: GlobalObject::EntryCollectGarbage Invoke\n"));
            Output::Flush();
        }
#endif

#if DBG
        // Clear 1K of stack to avoid false positive in debug build.
        // Because we don't debug build don't stack pack
        DebugClearStack();
#endif
        Assert(!(callInfo.Flags & CallFlags_New));

        ScriptContext* scriptContext = function->GetScriptContext();
        if (!scriptContext->GetConfig()->IsCollectGarbageEnabled()
#ifdef ENABLE_PROJECTION
            && scriptContext->GetConfig()->GetHostType() != HostType::HostTypeApplication
            && scriptContext->GetConfig()->GetHostType() != HostType::HostTypeWebview
#endif
            )
        {LOGMEIN("GlobalObject.cpp] 1564\n");
            // We expose the CollectGarbage API with flag for compat reasons.
            // If CollectGarbage key is not enabled, and if the HostType is neither
            // HostType::HostTypeApplication nor HostType::HostTypeWebview,
            // then we do not trigger collection.
            return scriptContext->GetLibrary()->GetUndefined();
        }

        Recycler* recycler = scriptContext->GetRecycler();
        if (recycler)
        {LOGMEIN("GlobalObject.cpp] 1574\n");
#if DBG && defined(RECYCLER_DUMP_OBJECT_GRAPH)
            ARGUMENTS(args, callInfo);
            if (args.Info.Count > 1)
            {LOGMEIN("GlobalObject.cpp] 1578\n");
                double value = JavascriptConversion::ToNumber(args[1], function->GetScriptContext());
                if (value == 1.0)
                {LOGMEIN("GlobalObject.cpp] 1581\n");
                    recycler->dumpObjectOnceOnCollect = true;
                }
            }
#endif
            recycler->CollectNow<CollectNowDecommitNowExplicit>();
        }

#if DBG_DUMP
#ifdef ENABLE_PROJECTION
        scriptContext->GetThreadContext()->DumpProjectionContextMemoryStats(_u("Stats after GlobalObject::EntryCollectGarbage call"));
#endif

        if (Js::Configuration::Global.flags.TraceWin8Allocations)
        {LOGMEIN("GlobalObject.cpp] 1595\n");
            Output::Print(_u("MemoryTrace: GlobalObject::EntryCollectGarbage Exit\n"));
            Output::Flush();
        }
#endif

        return scriptContext->GetLibrary()->GetUndefined();
    }

#if ENABLE_TTD
    //Log a string in the telemetry system (and print to the console)
    Var GlobalObject::EntryTelemetryLog(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        TTDAssert(args.Info.Count >= 2 && Js::JavascriptString::Is(args[1]), "Bad arguments!!!");

        Js::JavascriptString* jsString = Js::JavascriptString::FromVar(args[1]);
        bool doPrint = (args.Info.Count == 3) && Js::JavascriptBoolean::Is(args[2]) && (Js::JavascriptBoolean::FromVar(args[2])->GetValue());

        if(function->GetScriptContext()->ShouldPerformReplayAction())
        {LOGMEIN("GlobalObject.cpp] 1617\n");
            function->GetScriptContext()->GetThreadContext()->TTDLog->ReplayTelemetryLogEvent(jsString);
        }

        if(function->GetScriptContext()->ShouldPerformRecordAction())
        {LOGMEIN("GlobalObject.cpp] 1622\n");
            function->GetScriptContext()->GetThreadContext()->TTDLog->RecordTelemetryLogEvent(jsString, doPrint);
        }

        if(doPrint)
        {LOGMEIN("GlobalObject.cpp] 1627\n");
            //
            //TODO: the host should give us a print callback which we can use here
            //
            wprintf(_u("%ls\n"), jsString->GetSz());
            fflush(stdout);
        }

        return function->GetScriptContext()->GetLibrary()->GetUndefined();
    }

    //Check if diagnostic trace writing is enabled
    Var GlobalObject::EntryEnabledDiagnosticsTrace(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        if(function->GetScriptContext()->ShouldPerformRecordOrReplayAction())
        {LOGMEIN("GlobalObject.cpp] 1645\n");
            return function->GetScriptContext()->GetLibrary()->GetTrue();
        }
        else
        {
            return function->GetScriptContext()->GetLibrary()->GetFalse();
        }
    }

    //Write a copy of the current TTD log to a specified location
    Var GlobalObject::EntryEmitTTDLog(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);
        ARGUMENTS(args, callInfo);

        Js::JavascriptLibrary* jslib = function->GetScriptContext()->GetLibrary();

        if(args.Info.Count != 2 || !Js::JavascriptString::Is(args[1]))
        {LOGMEIN("GlobalObject.cpp] 1663\n");
            return jslib->GetFalse();
        }

        if(function->GetScriptContext()->ShouldPerformReplayAction())
        {LOGMEIN("GlobalObject.cpp] 1668\n");
            function->GetScriptContext()->GetThreadContext()->TTDLog->ReplayEmitLogEvent();

            return jslib->GetTrue();
        }

        if(function->GetScriptContext()->ShouldPerformRecordAction())
        {LOGMEIN("GlobalObject.cpp] 1675\n");
            Js::JavascriptString* jsString = Js::JavascriptString::FromVar(args[1]);
            function->GetScriptContext()->GetThreadContext()->TTDLog->RecordEmitLogEvent(jsString);

            return jslib->GetTrue();
        }

        return jslib->GetFalse();
    }
#endif

    //Pattern match is unique to RuntimeObject. Only leading and trailing * are implemented
    //Example: *pat*tern* actually matches all the strings having pat*tern as substring.
    BOOL GlobalObject::MatchPatternHelper(JavascriptString *propertyName, JavascriptString *pattern, ScriptContext *scriptContext)
    {LOGMEIN("GlobalObject.cpp] 1689\n");
        if (nullptr == propertyName || nullptr == pattern)
        {LOGMEIN("GlobalObject.cpp] 1691\n");
            return FALSE;
        }

        charcount_t     patternLength = pattern->GetLength();
        charcount_t     nameLength = propertyName->GetLength();
        BOOL    bStart;
        BOOL    bEnd;

        if (0 == patternLength)
        {LOGMEIN("GlobalObject.cpp] 1701\n");
            return TRUE; // empty pattern matches all
        }

        bStart = (_u('*') == pattern->GetItem(0));// Is there a start star?
        bEnd = (_u('*') == pattern->GetItem(patternLength - 1));// Is there an end star?

        //deal with the stars
        if (bStart || bEnd)
        {LOGMEIN("GlobalObject.cpp] 1710\n");
            JavascriptString *subPattern;
            int idxStart = bStart? 1: 0;
            int idxEnd   = bEnd? (patternLength - 1): patternLength;
            if (idxEnd <= idxStart)
            {LOGMEIN("GlobalObject.cpp] 1715\n");
                return TRUE; //scenario double star **
            }

            //get a pattern which doesn't contain leading and trailing stars
            subPattern = JavascriptString::FromVar(JavascriptString::SubstringCore(pattern, idxStart, idxEnd - idxStart, scriptContext));

            uint index = JavascriptString::strstr(propertyName, subPattern, false);

            if (index == (uint)-1)
            {LOGMEIN("GlobalObject.cpp] 1725\n");
                return FALSE;
            }
            if (FALSE == bStart)
            {LOGMEIN("GlobalObject.cpp] 1729\n");
                return index == 0; //begin at the same place;
            }
            else if (FALSE == bEnd)
            {LOGMEIN("GlobalObject.cpp] 1733\n");
                return (index + patternLength - 1) == (nameLength);
            }
            return TRUE;
        }
        else
        {
            //full match
            if (0 == JavascriptString::strcmp(propertyName, pattern))
            {LOGMEIN("GlobalObject.cpp] 1742\n");
                return TRUE;
            }
        }

        return FALSE;
    }

    BOOL GlobalObject::HasProperty(PropertyId propertyId)
    {LOGMEIN("GlobalObject.cpp] 1751\n");
        return DynamicObject::HasProperty(propertyId) ||
            (this->directHostObject && JavascriptOperators::HasProperty(this->directHostObject, propertyId)) ||
            (this->hostObject && JavascriptOperators::HasProperty(this->hostObject, propertyId));
    }

    BOOL GlobalObject::HasRootProperty(PropertyId propertyId)
    {LOGMEIN("GlobalObject.cpp] 1758\n");
        return __super::HasRootProperty(propertyId) ||
            (this->directHostObject && JavascriptOperators::HasProperty(this->directHostObject, propertyId)) ||
            (this->hostObject && JavascriptOperators::HasProperty(this->hostObject, propertyId));
    }

    BOOL GlobalObject::HasOwnProperty(PropertyId propertyId)
    {LOGMEIN("GlobalObject.cpp] 1765\n");
        return DynamicObject::HasProperty(propertyId) ||
            (this->directHostObject && this->directHostObject->HasProperty(propertyId));
    }

    BOOL GlobalObject::HasOwnPropertyNoHostObject(PropertyId propertyId)
    {LOGMEIN("GlobalObject.cpp] 1771\n");
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL GlobalObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 1776\n");
        if (DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {LOGMEIN("GlobalObject.cpp] 1778\n");
            return TRUE;
        }
        return (this->directHostObject && JavascriptOperators::GetProperty(this->directHostObject, propertyId, value, requestContext, info)) ||
            (this->hostObject && JavascriptOperators::GetProperty(this->hostObject, propertyId, value, requestContext, info));
    }

    BOOL GlobalObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 1786\n");
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return GlobalObject::GetProperty(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }

    BOOL GlobalObject::GetRootProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 1793\n");
        if (__super::GetRootProperty(originalInstance, propertyId, value, info, requestContext))
        {LOGMEIN("GlobalObject.cpp] 1795\n");
            return TRUE;
        }
        return (this->directHostObject && JavascriptOperators::GetProperty(this->directHostObject, propertyId, value, requestContext, info)) ||
            (this->hostObject && JavascriptOperators::GetProperty(this->hostObject, propertyId, value, requestContext, info));
    }

    BOOL GlobalObject::GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext)
    {LOGMEIN("GlobalObject.cpp] 1803\n");
        if (DynamicObject::GetAccessors(propertyId, getter, setter, requestContext))
        {LOGMEIN("GlobalObject.cpp] 1805\n");
            return TRUE;
        }
        if (this->directHostObject)
        {LOGMEIN("GlobalObject.cpp] 1809\n");
            return this->directHostObject->GetAccessors(propertyId, getter, setter, requestContext);
        }
        else if (this->hostObject)
        {LOGMEIN("GlobalObject.cpp] 1813\n");
            return this->hostObject->GetAccessors(propertyId, getter, setter, requestContext);
        }
        return FALSE;
    }

    BOOL GlobalObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info,
        ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 1821\n");
        if (DynamicObject::GetPropertyReference(originalInstance, propertyId, value, info, requestContext))
        {LOGMEIN("GlobalObject.cpp] 1823\n");
            return true;
        }
        return (this->directHostObject && JavascriptOperators::GetPropertyReference(this->directHostObject, propertyId, value, requestContext, info)) ||
            (this->hostObject && JavascriptOperators::GetPropertyReference(this->hostObject, propertyId, value, requestContext, info));
    }

    BOOL GlobalObject::GetRootPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info,
        ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 1832\n");
        if (__super::GetRootPropertyReference(originalInstance, propertyId, value, info, requestContext))
        {LOGMEIN("GlobalObject.cpp] 1834\n");
            return true;
        }
        return (this->directHostObject && JavascriptOperators::GetPropertyReference(this->directHostObject, propertyId, value, requestContext, info)) ||
            (this->hostObject && JavascriptOperators::GetPropertyReference(this->hostObject, propertyId, value, requestContext, info));
    }

    BOOL GlobalObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("GlobalObject.cpp] 1842\n");
        // var x = 10; variables declared with "var" at global scope
        // These cannot be deleted
        // In ES5 they are enumerable.

        PropertyAttributes attributes = PropertyWritable | PropertyEnumerable | PropertyDeclaredGlobal;
        flags = static_cast<PropertyOperationFlags>(flags | PropertyOperation_ThrowIfNotExtensible);
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, info, flags);
    }

    BOOL GlobalObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {LOGMEIN("GlobalObject.cpp] 1853\n");
        // var x = 10; variables declared with "var" inside "eval"
        // These CAN be deleted
        // in ES5 they are enumerable.
        //
        PropertyAttributes attributes = PropertyDynamicTypeDefaults | PropertyDeclaredGlobal;
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, NULL, PropertyOperation_ThrowIfNotExtensible);
    }

    BOOL GlobalObject::InitFuncScoped(PropertyId propertyId, Var value)
    {LOGMEIN("GlobalObject.cpp] 1863\n");
        // Var binding of functions declared in eval are elided when conflicting
        // with global scope let/const variables, so do not actually set the
        // property if it exists and is a let/const variable.
        bool noRedecl = false;
        if (!GetTypeHandler()->HasRootProperty(this, propertyId, &noRedecl) || !noRedecl)
        {LOGMEIN("GlobalObject.cpp] 1869\n");
            //
            // var x = 10; variables declared with "var" inside "eval"
            // These CAN be deleted
            // in ES5 they are enumerable.

            PropertyAttributes attributes = PropertyDynamicTypeDefaults;

            DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, NULL, PropertyOperation_ThrowIfNotExtensible);
        }
        return true;
    }

    BOOL GlobalObject::SetExistingProperty(PropertyId propertyId, Var value, PropertyValueInfo* info, BOOL *setAttempted)
    {LOGMEIN("GlobalObject.cpp] 1883\n");
        BOOL hasOwnProperty = DynamicObject::HasProperty(propertyId);
        BOOL hasProperty = JavascriptOperators::HasProperty(this->GetPrototype(), propertyId);
        *setAttempted = TRUE;

        if (this->directHostObject &&
            !hasOwnProperty &&
            !hasProperty &&
            this->directHostObject->HasProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 1892\n");
            // directHostObject->HasProperty returns true for things in global collections, however, they are not able to be set.
            // When we call SetProperty we'll return FALSE which means fall back to GlobalObject::SetProperty and shadow our collection
            // object rather than failing the set altogether. See bug Windows Out Of Band Releases 1144780 and linked bugs for details.
            if (this->directHostObject->SetProperty(propertyId, value, PropertyOperation_None, info))
            {LOGMEIN("GlobalObject.cpp] 1897\n");
                return TRUE;
            }
        }
        else
            if (this->hostObject &&
                // Consider to revert to the commented line and ignore the prototype chain when direct host is on by default in IE9 mode
                !hasOwnProperty &&
                !hasProperty &&
                this->hostObject->HasProperty(propertyId))
            {LOGMEIN("GlobalObject.cpp] 1907\n");
                return this->hostObject->SetProperty(propertyId, value, PropertyOperation_None, NULL);
            }

        if (hasOwnProperty || hasProperty)
        {LOGMEIN("GlobalObject.cpp] 1912\n");
            return DynamicObject::SetProperty(propertyId, value, PropertyOperation_None, info);
        }

        *setAttempted = FALSE;
        return FALSE;
    }

    BOOL GlobalObject::SetExistingRootProperty(PropertyId propertyId, Var value, PropertyValueInfo* info, BOOL *setAttempted)
    {LOGMEIN("GlobalObject.cpp] 1921\n");
        BOOL hasOwnProperty = __super::HasRootProperty(propertyId);
        BOOL hasProperty = JavascriptOperators::HasProperty(this->GetPrototype(), propertyId);
        *setAttempted = TRUE;

        if (this->directHostObject &&
            !hasOwnProperty &&
            !hasProperty &&
            this->directHostObject->HasProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 1930\n");
            // directHostObject->HasProperty returns true for things in global collections, however, they are not able to be set.
            // When we call SetProperty we'll return FALSE which means fall back to GlobalObject::SetProperty and shadow our collection
            // object rather than failing the set altogether. See bug Windows Out Of Band Releases 1144780 and linked bugs for details.
            if (this->directHostObject->SetProperty(propertyId, value, PropertyOperation_None, info))
            {LOGMEIN("GlobalObject.cpp] 1935\n");
                return TRUE;
            }
        }
        else
            if (this->hostObject &&
                // Consider to revert to the commented line and ignore the prototype chain when direct host is on by default in IE9 mode
                !hasOwnProperty &&
                !hasProperty &&
                this->hostObject->HasProperty(propertyId))
            {LOGMEIN("GlobalObject.cpp] 1945\n");
                return this->hostObject->SetProperty(propertyId, value, PropertyOperation_None, NULL);
            }

        if (hasOwnProperty || hasProperty)
        {LOGMEIN("GlobalObject.cpp] 1950\n");
            return __super::SetRootProperty(propertyId, value, PropertyOperation_None, info);
        }

        *setAttempted = FALSE;
        return FALSE;
    }

    BOOL GlobalObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("GlobalObject.cpp] 1959\n");
        Assert(!(flags & PropertyOperation_Root));

        BOOL setAttempted = TRUE;
        if (this->SetExistingProperty(propertyId, value, info, &setAttempted))
        {LOGMEIN("GlobalObject.cpp] 1964\n");
            return TRUE;
        }

        //
        // Set was attempted. But the set operation returned false.
        // This happens, when the property is read only.
        // In those scenarios, we should be setting the property with default attributes
        //
        if (setAttempted)
        {LOGMEIN("GlobalObject.cpp] 1974\n");
            return FALSE;
        }

        // Windows 8 430092: When we set a new property on globalObject there may be stale inline caches holding onto directHostObject->prototype
        // properties of the same name. So we need to clear proto caches in this scenario. But check for same property in directHostObject->prototype
        // chain is expensive (call to DOM) compared to just invalidating the cache.
        // If this blind invalidation is expensive in any scenario then we need to revisit this.
        // Another solution proposed was not to cache any of the properties of window->directHostObject->prototype.
        // if ((this->directHostObject && JavascriptOperators::HasProperty(this->directHostObject->GetPrototype(), propertyId)) ||
        //    (this->hostObject && JavascriptOperators::HasProperty(this->hostObject->GetPrototype(), propertyId)))

        this->GetScriptContext()->InvalidateProtoCaches(propertyId);

        //
        // x = 10; implicit/phantom variable at global scope
        // These CAN be deleted
        // In ES5 they are enumerable.

        PropertyAttributes attributes = PropertyDynamicTypeDefaults;
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, info);
    }

    BOOL GlobalObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("GlobalObject.cpp] 1998\n");
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return GlobalObject::SetProperty(propertyRecord->GetPropertyId(), value, flags, info);
    }

    BOOL GlobalObject::SetRootProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("GlobalObject.cpp] 2005\n");
        Assert(flags & PropertyOperation_Root);

        BOOL setAttempted = TRUE;
        if (this->SetExistingRootProperty(propertyId, value, info, &setAttempted))
        {LOGMEIN("GlobalObject.cpp] 2010\n");
            return TRUE;
        }

        //
        // Set was attempted. But the set operation returned false.
        // This happens, when the property is read only.
        // In those scenarios, we should be setting the property with default attributes
        //
        if (setAttempted)
        {LOGMEIN("GlobalObject.cpp] 2020\n");
            return FALSE;
        }

        if (flags & PropertyOperation_StrictMode)
        {LOGMEIN("GlobalObject.cpp] 2025\n");
            if (!GetScriptContext()->GetThreadContext()->RecordImplicitException())
            {LOGMEIN("GlobalObject.cpp] 2027\n");
                return FALSE;
            }
            JavascriptError::ThrowReferenceError(GetScriptContext(), JSERR_RefErrorUndefVariable);
        }

        // Windows 8 430092: When we set a new property on globalObject there may be stale inline caches holding onto directHostObject->prototype
        // properties of the same name. So we need to clear proto caches in this scenario. But check for same property in directHostObject->prototype
        // chain is expensive (call to DOM) compared to just invalidating the cache.
        // If this blind invalidation is expensive in any scenario then we need to revisit this.
        // Another solution proposed was not to cache any of the properties of window->directHostObject->prototype.
        // if ((this->directHostObject && JavascriptOperators::HasProperty(this->directHostObject->GetPrototype(), propertyId)) ||
        //    (this->hostObject && JavascriptOperators::HasProperty(this->hostObject->GetPrototype(), propertyId)))

        this->GetScriptContext()->InvalidateProtoCaches(propertyId);

        return __super::SetRootProperty(propertyId, value, flags, info);
    }

    BOOL GlobalObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("GlobalObject.cpp] 2047\n");
        if (DynamicObject::SetAccessors(propertyId, getter, setter, flags))
        {LOGMEIN("GlobalObject.cpp] 2049\n");
            return TRUE;
        }
        if (this->directHostObject)
        {LOGMEIN("GlobalObject.cpp] 2053\n");
            return this->directHostObject->SetAccessors(propertyId, getter, setter, flags);
        }
        if (this->hostObject)
        {LOGMEIN("GlobalObject.cpp] 2057\n");
            return this->hostObject->SetAccessors(propertyId, getter, setter, flags);
        }
        return TRUE;
    }

    DescriptorFlags GlobalObject::GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 2064\n");
        DescriptorFlags flags = DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
        if (flags == None)
        {LOGMEIN("GlobalObject.cpp] 2067\n");
            if (this->directHostObject)
            {LOGMEIN("GlobalObject.cpp] 2069\n");
                // We need to look up the prototype chain here.
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(this->directHostObject, propertyId, setterValue, &flags, info, requestContext);
            }
            else if (this->hostObject)
            {LOGMEIN("GlobalObject.cpp] 2074\n");
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(this->hostObject, propertyId, setterValue, &flags, info, requestContext);
            }
        }

        return flags;
    }

    DescriptorFlags GlobalObject::GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 2083\n");
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return GlobalObject::GetSetter(propertyRecord->GetPropertyId(), setterValue, info, requestContext);
    }

    DescriptorFlags GlobalObject::GetRootSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 2090\n");
        DescriptorFlags flags = __super::GetRootSetter(propertyId, setterValue, info, requestContext);
        if (flags == None)
        {LOGMEIN("GlobalObject.cpp] 2093\n");
            if (this->directHostObject)
            {LOGMEIN("GlobalObject.cpp] 2095\n");
                // We need to look up the prototype chain here.
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(this->directHostObject, propertyId, setterValue, &flags, info, requestContext);
            }
            else if (this->hostObject)
            {LOGMEIN("GlobalObject.cpp] 2100\n");
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableProperty(this->hostObject, propertyId, setterValue, &flags, info, requestContext);
            }
        }

        return flags;
    }

    DescriptorFlags GlobalObject::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 2109\n");
        DescriptorFlags flags = DynamicObject::GetItemSetter(index, setterValue, requestContext);
        if (flags == None)
        {LOGMEIN("GlobalObject.cpp] 2112\n");
            if (this->directHostObject)
            {LOGMEIN("GlobalObject.cpp] 2114\n");
                // We need to look up the prototype chain here.
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableItem(this->directHostObject, index, setterValue, &flags, requestContext);
            }
            else if (this->hostObject)
            {LOGMEIN("GlobalObject.cpp] 2119\n");
                JavascriptOperators::CheckPrototypesForAccessorOrNonWritableItem(this->hostObject, index, setterValue, &flags, requestContext);
            }
        }

        return flags;
    }

    BOOL GlobalObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("GlobalObject.cpp] 2128\n");
        if (__super::HasProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 2130\n");
            return __super::DeleteProperty(propertyId, flags);
        }
        else if (this->directHostObject && this->directHostObject->HasProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 2134\n");
            return this->directHostObject->DeleteProperty(propertyId, flags);
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 2138\n");
            return this->hostObject->DeleteProperty(propertyId, flags);
        }

        // Non-existent property
        return TRUE;
    }

    BOOL GlobalObject::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("GlobalObject.cpp] 2147\n");
        PropertyRecord const *propertyRecord = nullptr;
        if (JavascriptOperators::ShouldTryDeleteProperty(this, propertyNameString, &propertyRecord))
        {LOGMEIN("GlobalObject.cpp] 2150\n");
            Assert(propertyRecord);
            return DeleteProperty(propertyRecord->GetPropertyId(), flags);
        }

        return TRUE;
    }

    BOOL GlobalObject::DeleteRootProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("GlobalObject.cpp] 2159\n");
        if (__super::HasRootProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 2161\n");
            return __super::DeleteRootProperty(propertyId, flags);
        }
        else if (this->directHostObject && this->directHostObject->HasProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 2165\n");
            return this->directHostObject->DeleteProperty(propertyId, flags);
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {LOGMEIN("GlobalObject.cpp] 2169\n");
            return this->hostObject->DeleteProperty(propertyId, flags);
        }

        // Non-existent property
        return TRUE;
    }

    BOOL GlobalObject::HasItem(uint32 index)
    {LOGMEIN("GlobalObject.cpp] 2178\n");
        return DynamicObject::HasItem(index)
            || (this->directHostObject && JavascriptOperators::HasItem(this->directHostObject, index))
            || (this->hostObject && JavascriptOperators::HasItem(this->hostObject, index));
    }

    BOOL GlobalObject::HasOwnItem(uint32 index)
    {LOGMEIN("GlobalObject.cpp] 2185\n");
        return DynamicObject::HasItem(index)
            || (this->directHostObject && this->directHostObject->HasItem(index));
    }

    BOOL GlobalObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 2191\n");
        if (DynamicObject::GetItemReference(originalInstance, index, value, requestContext))
        {LOGMEIN("GlobalObject.cpp] 2193\n");
            return TRUE;
        }
        return (this->directHostObject && this->directHostObject->GetItemReference(originalInstance, index, value, requestContext)) ||
            (this->hostObject && this->hostObject->GetItemReference(originalInstance, index, value, requestContext));
    }

    BOOL GlobalObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("GlobalObject.cpp] 2201\n");
        BOOL result = DynamicObject::SetItem(index, value, flags);
        return result;
    }

    BOOL GlobalObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 2207\n");
        if (DynamicObject::GetItem(originalInstance, index, value, requestContext))
        {LOGMEIN("GlobalObject.cpp] 2209\n");
            return TRUE;
        }

        return (this->directHostObject && this->directHostObject->GetItem(originalInstance, index, value, requestContext)) ||
            (this->hostObject && this->hostObject->GetItem(originalInstance, index, value, requestContext));
    }

    BOOL GlobalObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("GlobalObject.cpp] 2218\n");
        if (DynamicObject::DeleteItem(index, flags))
        {LOGMEIN("GlobalObject.cpp] 2220\n");
            return TRUE;
        }

        if (this->directHostObject)
        {LOGMEIN("GlobalObject.cpp] 2225\n");
            return this->directHostObject->DeleteItem(index, flags);
        }

        if (this->hostObject)
        {LOGMEIN("GlobalObject.cpp] 2230\n");
            return this->hostObject->DeleteItem(index, flags);
        }
        return FALSE;
    }

    BOOL GlobalObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 2237\n");
        stringBuilder->AppendCppLiteral(_u("{...}"));
        return TRUE;
    }

    BOOL GlobalObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("GlobalObject.cpp] 2243\n");
        stringBuilder->AppendCppLiteral(_u("Object, (Global)"));
        return TRUE;
    }

    BOOL GlobalObject::StrictEquals(__in Js::Var other, __out BOOL* value, ScriptContext * requestContext)
    {LOGMEIN("GlobalObject.cpp] 2249\n");
        if (this == other)
        {LOGMEIN("GlobalObject.cpp] 2251\n");
            *value = TRUE;
            return TRUE;
        }
        else if (this->directHostObject)
        {LOGMEIN("GlobalObject.cpp] 2256\n");
            return this->directHostObject->StrictEquals(other, value, requestContext);
        }
        else if (this->hostObject)
        {LOGMEIN("GlobalObject.cpp] 2260\n");
            return this->hostObject->StrictEquals(other, value, requestContext);
        }
        *value = FALSE;
        return FALSE;
    }

    BOOL GlobalObject::Equals(__in Js::Var other, __out BOOL* value, ScriptContext * requestContext)
    {LOGMEIN("GlobalObject.cpp] 2268\n");
        if (this == other)
        {LOGMEIN("GlobalObject.cpp] 2270\n");
            *value = TRUE;
            return TRUE;
        }
        else if (this->directHostObject)
        {LOGMEIN("GlobalObject.cpp] 2275\n");
            return this->directHostObject->Equals(other, value, requestContext);
        }
        else if (this->hostObject)
        {LOGMEIN("GlobalObject.cpp] 2279\n");
            return this->hostObject->Equals(other, value, requestContext);
        }

        *value = FALSE;
        return TRUE;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType GlobalObject::GetSnapTag_TTD() const
    {LOGMEIN("GlobalObject.cpp] 2289\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapWellKnownObject;
    }

    void GlobalObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("GlobalObject.cpp] 2294\n");
        //
        //TODO: we assume that the global object is always set right (e.g., ignore the directHostObject and secureDirectHostObject values) we need to verify that this is ok and/or what to do about it
        //

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapWellKnownObject>(objData, nullptr);
    }
#endif
}
