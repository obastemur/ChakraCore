//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language/JavascriptFunctionArgIndex.h"
#include "Language/InterpreterStackFrame.h"
#include "Language/JavascriptStackWalker.h"

namespace Js
{
    DiagStackFrame::DiagStackFrame():
        isTopFrame(false)
    {LOGMEIN("DiagStackFrame.cpp] 13\n");
    }

    // Returns whether or not this frame is on the top of the callstack.
    bool DiagStackFrame::IsTopFrame()
    {LOGMEIN("DiagStackFrame.cpp] 18\n");
        return this->isTopFrame && GetScriptContext()->GetDebugContext()->GetProbeContainer()->IsPrimaryBrokenToDebuggerContext();
    }

    void DiagStackFrame::SetIsTopFrame()
    {LOGMEIN("DiagStackFrame.cpp] 23\n");
        this->isTopFrame = true;
    }

    ScriptFunction* DiagStackFrame::GetScriptFunction()
    {LOGMEIN("DiagStackFrame.cpp] 28\n");
        return ScriptFunction::FromVar(GetJavascriptFunction());
    }

    FunctionBody* DiagStackFrame::GetFunction()
    {LOGMEIN("DiagStackFrame.cpp] 33\n");
        return GetJavascriptFunction()->GetFunctionBody();
    }

    ScriptContext* DiagStackFrame::GetScriptContext()
    {LOGMEIN("DiagStackFrame.cpp] 38\n");
        return GetJavascriptFunction()->GetScriptContext();
    }

    PCWSTR DiagStackFrame::GetDisplayName()
    {LOGMEIN("DiagStackFrame.cpp] 43\n");
        return GetFunction()->GetExternalDisplayName();
    }

    bool DiagStackFrame::IsInterpreterFrame()
    {LOGMEIN("DiagStackFrame.cpp] 48\n");
        return false;
    }

    InterpreterStackFrame* DiagStackFrame::AsInterpreterFrame()
    {
        AssertMsg(FALSE, "AsInterpreterFrame called for non-interpreter frame.");
        return nullptr;
    }

    ArenaAllocator * DiagStackFrame::GetArena()
    {LOGMEIN("DiagStackFrame.cpp] 59\n");
        Assert(GetScriptContext() != NULL);
        return GetScriptContext()->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();
    }

    FrameDisplay * DiagStackFrame::GetFrameDisplay()
    {LOGMEIN("DiagStackFrame.cpp] 65\n");
        FrameDisplay *display = NULL;

        Assert(this->GetFunction() != NULL);
        RegSlot frameDisplayReg = this->GetFunction()->GetFrameDisplayRegister();

        if (frameDisplayReg != Js::Constants::NoRegister && frameDisplayReg != 0)
        {LOGMEIN("DiagStackFrame.cpp] 72\n");
            display = (FrameDisplay*)this->GetNonVarRegValue(frameDisplayReg);
        }
        else
        {
            display = this->GetScriptFunction()->GetEnvironment();
        }
 
        return display;
    }

    Var DiagStackFrame::GetScopeObjectFromFrameDisplay(uint index)
    {LOGMEIN("DiagStackFrame.cpp] 84\n");
        FrameDisplay * display = GetFrameDisplay();
        return (display != NULL && display->GetLength() > index) ? display->GetItem(index) : NULL;
    }

    Var DiagStackFrame::GetRootObject()
    {LOGMEIN("DiagStackFrame.cpp] 90\n");
        Assert(this->GetFunction());
        return this->GetFunction()->LoadRootObject();
    }

    BOOL DiagStackFrame::IsStrictMode()
    {LOGMEIN("DiagStackFrame.cpp] 96\n");
        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();
        return scopeFunction->IsStrictMode();
    }

    BOOL DiagStackFrame::IsThisAvailable()
    {LOGMEIN("DiagStackFrame.cpp] 102\n");
        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();
        return !scopeFunction->IsLambda() || scopeFunction->GetParseableFunctionInfo()->GetCapturesThis();
    }

    Js::Var DiagStackFrame::GetThisFromFrame(Js::IDiagObjectAddress ** ppOutAddress, Js::IDiagObjectModelWalkerBase * localsWalker)
    {LOGMEIN("DiagStackFrame.cpp] 108\n");
        Js::ScriptContext* scriptContext = this->GetScriptContext();
        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();
        Js::ModuleID moduleId = scopeFunction->IsScriptFunction() ? scopeFunction->GetFunctionBody()->GetModuleID() : 0;
        Js::Var varThis = scriptContext->GetLibrary()->GetNull();

        if (!scopeFunction->IsLambda())
        {LOGMEIN("DiagStackFrame.cpp] 115\n");
            Js::JavascriptStackWalker::GetThis(&varThis, moduleId, scopeFunction, scriptContext);
        }
        else
        {
            if (!scopeFunction->GetParseableFunctionInfo()->GetCapturesThis())
            {LOGMEIN("DiagStackFrame.cpp] 121\n");
                return nullptr;
            }
            else
            {
                // Emulate Js::JavascriptOperators::OP_GetThisScoped using a locals walker and assigning moduleId object if not found by locals walker
                if (localsWalker == nullptr)
                {LOGMEIN("DiagStackFrame.cpp] 128\n");
                    ArenaAllocator *arena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();
                    localsWalker = Anew(arena, Js::LocalsWalker, this, Js::FrameWalkerFlags::FW_EnumWithScopeAlso | Js::FrameWalkerFlags::FW_AllowLexicalThis);
                }

                bool unused = false;
                Js::IDiagObjectAddress* address = localsWalker->FindPropertyAddress(Js::PropertyIds::_lexicalThisSlotSymbol, unused);

                if (ppOutAddress != nullptr)
                {LOGMEIN("DiagStackFrame.cpp] 137\n");
                    *ppOutAddress = address;
                }

                if (address != nullptr)
                {LOGMEIN("DiagStackFrame.cpp] 142\n");
                    varThis = address->GetValue(FALSE);
                }
                else if (moduleId == kmodGlobal)
                {LOGMEIN("DiagStackFrame.cpp] 146\n");
                    varThis = Js::JavascriptOperators::OP_LdRoot(scriptContext)->ToThis();
                }
                else
                {
                    varThis = (Var)Js::JavascriptOperators::GetModuleRoot(moduleId, scriptContext);
                }
            }
        }

        Js::GlobalObject::UpdateThisForEval(varThis, moduleId, scriptContext, this->IsStrictMode());

        return varThis;
    }

    void DiagStackFrame::TryFetchValueAndAddress(const char16 *source, int sourceLength, Js::ResolvedObject * pOutResolvedObj)
    {LOGMEIN("DiagStackFrame.cpp] 162\n");
        Assert(source);
        Assert(pOutResolvedObj);

        Js::ScriptContext* scriptContext = this->GetScriptContext();
        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();

        // Do fast path for 'this', fields on slot, TODO : literals (integer,string)

        if (sourceLength == 4 && wcsncmp(source, _u("this"), 4) == 0)
        {LOGMEIN("DiagStackFrame.cpp] 172\n");
            pOutResolvedObj->obj = this->GetThisFromFrame(&pOutResolvedObj->address);
            if (pOutResolvedObj->obj == nullptr)
            {LOGMEIN("DiagStackFrame.cpp] 175\n");
                // TODO: Throw exception; this was not captured by the lambda
                Assert(scopeFunction->IsLambda());
                Assert(!scopeFunction->GetParseableFunctionInfo()->GetCapturesThis());
            }
        }
        else
        {
            Js::PropertyRecord const * propRecord;
            scriptContext->FindPropertyRecord(source, sourceLength, &propRecord);
            if (propRecord != nullptr)
            {LOGMEIN("DiagStackFrame.cpp] 186\n");
                ArenaAllocator *arena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();

                Js::IDiagObjectModelWalkerBase * localsWalker = Anew(arena, Js::LocalsWalker, this, Js::FrameWalkerFlags::FW_EnumWithScopeAlso);

                bool isConst = false;
                pOutResolvedObj->address = localsWalker->FindPropertyAddress(propRecord->GetPropertyId(), isConst);
                if (pOutResolvedObj->address != nullptr)
                {LOGMEIN("DiagStackFrame.cpp] 194\n");
                    pOutResolvedObj->obj = pOutResolvedObj->address->GetValue(FALSE);
                    pOutResolvedObj->isConst = isConst;
                }
            }
        }

    }

    Js::ScriptFunction* DiagStackFrame::TryGetFunctionForEval(Js::ScriptContext* scriptContext, const char16 *source, int sourceLength, BOOL isLibraryCode /* = FALSE */)
    {LOGMEIN("DiagStackFrame.cpp] 204\n");
        // TODO: pass the real length of the source code instead of wcslen
        uint32 grfscr = fscrReturnExpression | fscrEval | fscrEvalCode | fscrGlobalCode | fscrConsoleScopeEval;
        if (!this->IsThisAvailable())
        {LOGMEIN("DiagStackFrame.cpp] 208\n");
            grfscr |= fscrDebuggerErrorOnGlobalThis;
        }
        if (isLibraryCode)
        {LOGMEIN("DiagStackFrame.cpp] 212\n");
            grfscr |= fscrIsLibraryCode;
        }
        return scriptContext->GetGlobalObject()->EvalHelper(scriptContext, source, sourceLength, kmodGlobal, grfscr, Js::Constants::EvalCode, FALSE, FALSE, this->IsStrictMode());
    }

    void DiagStackFrame::EvaluateImmediate(const char16 *source, int sourceLength, BOOL isLibraryCode, Js::ResolvedObject * resolvedObject)
    {LOGMEIN("DiagStackFrame.cpp] 219\n");
        this->TryFetchValueAndAddress(source, sourceLength, resolvedObject);

        if (resolvedObject->obj == nullptr)
        {LOGMEIN("DiagStackFrame.cpp] 223\n");
            Js::ScriptFunction* pfuncScript = this->TryGetFunctionForEval(this->GetScriptContext(), source, sourceLength, isLibraryCode);
            if (pfuncScript != nullptr)
            {LOGMEIN("DiagStackFrame.cpp] 226\n");
                // Passing the nonuser code state from the enclosing function to the current function.
                // Treat native library frame (no function body) as non-user code.
                Js::FunctionBody* body = this->GetFunction();
                if (!body || body->IsNonUserCode())
                {LOGMEIN("DiagStackFrame.cpp] 231\n");
                    Js::FunctionBody *pCurrentFuncBody = pfuncScript->GetFunctionBody();
                    if (pCurrentFuncBody != nullptr)
                    {LOGMEIN("DiagStackFrame.cpp] 234\n");
                        pCurrentFuncBody->SetIsNonUserCode(true);
                    }
                }
                OUTPUT_TRACE(Js::ConsoleScopePhase, _u("EvaluateImmediate strict = %d, libraryCode = %d, source = '%s'\n"),
                    this->IsStrictMode(), isLibraryCode, source);
                resolvedObject->obj = this->DoEval(pfuncScript);
            }
        }
    }

#ifdef ENABLE_MUTATION_BREAKPOINT
    static void SetConditionalMutationBreakpointVariables(Js::DynamicObject * activeScopeObject, Js::ScriptContext * scriptContext)
    {LOGMEIN("DiagStackFrame.cpp] 247\n");
        // For Conditional Object Mutation Breakpoint user can access the new value, changing property name and mutation type using special variables
        // $newValue$, $propertyName$ and $mutationType$. Add this variables to activation object.
        Js::DebugManager* debugManager = scriptContext->GetDebugContext()->GetProbeContainer()->GetDebugManager();
        Js::MutationBreakpoint *mutationBreakpoint = debugManager->GetActiveMutationBreakpoint();
        if (mutationBreakpoint != nullptr)
        {LOGMEIN("DiagStackFrame.cpp] 253\n");
            if (Js::Constants::NoProperty == debugManager->mutationNewValuePid)
            {LOGMEIN("DiagStackFrame.cpp] 255\n");
                debugManager->mutationNewValuePid = scriptContext->GetOrAddPropertyIdTracked(_u("$newValue$"), 10);
            }
            if (Js::Constants::NoProperty == debugManager->mutationPropertyNamePid)
            {LOGMEIN("DiagStackFrame.cpp] 259\n");
                debugManager->mutationPropertyNamePid = scriptContext->GetOrAddPropertyIdTracked(_u("$propertyName$"), 14);
            }
            if (Js::Constants::NoProperty == debugManager->mutationTypePid)
            {LOGMEIN("DiagStackFrame.cpp] 263\n");
                debugManager->mutationTypePid = scriptContext->GetOrAddPropertyIdTracked(_u("$mutationType$"), 14);
            }

            AssertMsg(debugManager->mutationNewValuePid != Js::Constants::NoProperty, "Should have a valid mutationNewValuePid");
            AssertMsg(debugManager->mutationPropertyNamePid != Js::Constants::NoProperty, "Should have a valid mutationPropertyNamePid");
            AssertMsg(debugManager->mutationTypePid != Js::Constants::NoProperty, "Should have a valid mutationTypePid");

            Js::Var newValue = mutationBreakpoint->GetBreakNewValueVar();

            // Incase of MutationTypeDelete we won't have new value
            if (nullptr != newValue)
            {LOGMEIN("DiagStackFrame.cpp] 275\n");
                activeScopeObject->SetProperty(debugManager->mutationNewValuePid,
                    mutationBreakpoint->GetBreakNewValueVar(),
                    Js::PropertyOperationFlags::PropertyOperation_None,
                    nullptr);
            }
            else
            {
                activeScopeObject->SetProperty(debugManager->mutationNewValuePid,
                    scriptContext->GetLibrary()->GetUndefined(),
                    Js::PropertyOperationFlags::PropertyOperation_None,
                    nullptr);
            }

            // User should not be able to change $propertyName$ and $mutationType$ variables
            // Since we don't have address for $propertyName$ and $mutationType$ even if user change these varibales it won't be reflected after eval
            // But declaring these as const to prevent accidental typos by user so that we throw error in case user changes these variables

            Js::PropertyOperationFlags flags = static_cast<Js::PropertyOperationFlags>(Js::PropertyOperation_SpecialValue | Js::PropertyOperation_AllowUndecl);

            activeScopeObject->SetPropertyWithAttributes(debugManager->mutationPropertyNamePid,
                Js::JavascriptString::NewCopySz(mutationBreakpoint->GetBreakPropertyName(), scriptContext),
                PropertyConstDefaults, nullptr, flags);

            activeScopeObject->SetPropertyWithAttributes(debugManager->mutationTypePid,
                Js::JavascriptString::NewCopySz(mutationBreakpoint->GetMutationTypeForConditionalEval(mutationBreakpoint->GetBreakMutationType()), scriptContext),
                PropertyConstDefaults, nullptr, flags);
        }
    }
#endif

    Js::Var DiagStackFrame::DoEval(Js::ScriptFunction* pfuncScript)
    {LOGMEIN("DiagStackFrame.cpp] 307\n");
        Js::Var varResult = nullptr;

        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();
        Js::ScriptContext* scriptContext = this->GetScriptContext();

        ArenaAllocator *arena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();
        Js::LocalsWalker *localsWalker = Anew(arena, Js::LocalsWalker, this, 
            Js::FrameWalkerFlags::FW_EnumWithScopeAlso | Js::FrameWalkerFlags::FW_AllowLexicalThis | Js::FrameWalkerFlags::FW_AllowSuperReference | Js::FrameWalkerFlags::FW_DontAddGlobalsDirectly);

        // Store the diag address of a var to the map so that it will be used for editing the value.
        typedef JsUtil::BaseDictionary<Js::PropertyId, Js::IDiagObjectAddress*, ArenaAllocator, PrimeSizePolicy> PropIdToDiagAddressMap;
        PropIdToDiagAddressMap * propIdtoDiagAddressMap = Anew(arena, PropIdToDiagAddressMap, arena);

        // Create one scope object and init all scope properties in it, and push this object in front of the environment.
        Js::DynamicObject * activeScopeObject = localsWalker->CreateAndPopulateActivationObject(scriptContext, [propIdtoDiagAddressMap](Js::ResolvedObject& resolveObject)
        {
            if (!resolveObject.isConst)
            {LOGMEIN("DiagStackFrame.cpp] 325\n");
                propIdtoDiagAddressMap->AddNew(resolveObject.propId, resolveObject.address);
            }
        });
        if (!activeScopeObject)
        {LOGMEIN("DiagStackFrame.cpp] 330\n");
            activeScopeObject = scriptContext->GetLibrary()->CreateActivationObject();
        }

#ifdef ENABLE_MUTATION_BREAKPOINT
        SetConditionalMutationBreakpointVariables(activeScopeObject, scriptContext);
#endif

#if DBG
        uint32 countForVerification = activeScopeObject->GetPropertyCount();
#endif

        // Dummy scope object in the front, so that no new variable will be added to the scope.
        Js::DynamicObject * dummyObject = scriptContext->GetLibrary()->CreateActivationObject();

        // Remove its prototype object so that those item will not be visible to the expression evaluation.
        dummyObject->SetPrototype(scriptContext->GetLibrary()->GetNull());
        Js::DebugManager* debugManager = scriptContext->GetDebugContext()->GetProbeContainer()->GetDebugManager();
        Js::FrameDisplay* env = debugManager->GetFrameDisplay(scriptContext, dummyObject, activeScopeObject);
        pfuncScript->SetEnvironment(env);

        Js::Var varThis = this->GetThisFromFrame(nullptr, localsWalker);
        if (varThis == nullptr)
        {LOGMEIN("DiagStackFrame.cpp] 353\n");
            Assert(scopeFunction->IsLambda());
            Assert(!scopeFunction->GetParseableFunctionInfo()->GetCapturesThis());
            varThis = scriptContext->GetLibrary()->GetNull();
        }

        Js::Arguments args(1, (Js::Var*) &varThis);
        varResult = pfuncScript->CallFunction(args);

        debugManager->UpdateConsoleScope(dummyObject, scriptContext);

        // We need to find out the edits have been done to the dummy scope object during the eval. We need to apply those mutations to the actual vars.
        uint32 count = activeScopeObject->GetPropertyCount();

#if DBG
        Assert(countForVerification == count);
#endif

        for (uint32 i = 0; i < count; i++)
        {LOGMEIN("DiagStackFrame.cpp] 372\n");
            Js::PropertyId propertyId = activeScopeObject->GetPropertyId((Js::PropertyIndex)i);
            if (propertyId != Js::Constants::NoProperty)
            {LOGMEIN("DiagStackFrame.cpp] 375\n");
                Js::Var value;
                if (Js::JavascriptOperators::GetProperty(activeScopeObject, propertyId, &value, scriptContext))
                {LOGMEIN("DiagStackFrame.cpp] 378\n");
                    Js::IDiagObjectAddress * pAddress = nullptr;
                    if (propIdtoDiagAddressMap->TryGetValue(propertyId, &pAddress))
                    {LOGMEIN("DiagStackFrame.cpp] 381\n");
                        Assert(pAddress);
                        if (pAddress->GetValue(FALSE) != value)
                        {LOGMEIN("DiagStackFrame.cpp] 384\n");
                            pAddress->Set(value);
                        }
                    }
                }
            }
        }

        return varResult;
    }

    Var DiagStackFrame::GetInnerScopeFromRegSlot(RegSlot location)
    {LOGMEIN("DiagStackFrame.cpp] 396\n");
        return GetNonVarRegValue(location);
    }

    DiagInterpreterStackFrame::DiagInterpreterStackFrame(InterpreterStackFrame* frame) :
        m_interpreterFrame(frame)
    {LOGMEIN("DiagStackFrame.cpp] 402\n");
        Assert(m_interpreterFrame != NULL);
        AssertMsg(m_interpreterFrame->GetScriptContext() && m_interpreterFrame->GetScriptContext()->IsScriptContextInDebugMode(),
            "This only supports interpreter stack frames running in debug mode.");
    }

    JavascriptFunction* DiagInterpreterStackFrame::GetJavascriptFunction()
    {LOGMEIN("DiagStackFrame.cpp] 409\n");
        return m_interpreterFrame->GetJavascriptFunction();
    }

    ScriptContext* DiagInterpreterStackFrame::GetScriptContext()
    {LOGMEIN("DiagStackFrame.cpp] 414\n");
        return m_interpreterFrame->GetScriptContext();
    }

    int DiagInterpreterStackFrame::GetByteCodeOffset()
    {LOGMEIN("DiagStackFrame.cpp] 419\n");
        return m_interpreterFrame->GetReader()->GetCurrentOffset();
    }

    // Address on stack that belongs to current frame.
    // Currently we only use this to determine which of given frames is above/below another one.
    DWORD_PTR DiagInterpreterStackFrame::GetStackAddress()
    {LOGMEIN("DiagStackFrame.cpp] 426\n");
        return m_interpreterFrame->GetStackAddress();
    }

    bool DiagInterpreterStackFrame::IsInterpreterFrame()
    {LOGMEIN("DiagStackFrame.cpp] 431\n");
        return true;
    }

    InterpreterStackFrame* DiagInterpreterStackFrame::AsInterpreterFrame()
    {LOGMEIN("DiagStackFrame.cpp] 436\n");
        return m_interpreterFrame;
    }

    Var DiagInterpreterStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {LOGMEIN("DiagStackFrame.cpp] 441\n");
        return m_interpreterFrame->GetReg(slotId);
    }

    Var DiagInterpreterStackFrame::GetNonVarRegValue(RegSlot slotId)
    {LOGMEIN("DiagStackFrame.cpp] 446\n");
        return m_interpreterFrame->GetNonVarReg(slotId);
    }

    void DiagInterpreterStackFrame::SetRegValue(RegSlot slotId, Var value)
    {LOGMEIN("DiagStackFrame.cpp] 451\n");
        m_interpreterFrame->SetReg(slotId, value);
    }

    Var DiagInterpreterStackFrame::GetArgumentsObject()
    {LOGMEIN("DiagStackFrame.cpp] 456\n");
        return m_interpreterFrame->GetArgumentsObject();
    }

    Var DiagInterpreterStackFrame::CreateHeapArguments()
    {LOGMEIN("DiagStackFrame.cpp] 461\n");
        return m_interpreterFrame->CreateHeapArguments(GetScriptContext());
    }

    FrameDisplay * DiagInterpreterStackFrame::GetFrameDisplay()
    {LOGMEIN("DiagStackFrame.cpp] 466\n");
        return m_interpreterFrame->GetFrameDisplayForNestedFunc();
    }

    Var DiagInterpreterStackFrame::GetInnerScopeFromRegSlot(RegSlot location)
    {LOGMEIN("DiagStackFrame.cpp] 471\n");
        return m_interpreterFrame->InnerScopeFromRegSlot(location);
    }

#if ENABLE_NATIVE_CODEGEN
    DiagNativeStackFrame::DiagNativeStackFrame(
        ScriptFunction* function,
        int byteCodeOffset,
        void* stackAddr,
        void *codeAddr) :
        m_function(function),
        m_byteCodeOffset(byteCodeOffset),
        m_stackAddr(stackAddr),
        m_localVarSlotsOffset(InvalidOffset),
        m_localVarChangedOffset(InvalidOffset)
    {LOGMEIN("DiagStackFrame.cpp] 486\n");
        Assert(m_stackAddr != NULL);
        AssertMsg(m_function && m_function->GetScriptContext() && m_function->GetScriptContext()->IsScriptContextInDebugMode(),
            "This only supports functions in debug mode.");

        FunctionEntryPointInfo * entryPointInfo = GetFunction()->GetEntryPointFromNativeAddress((DWORD_PTR)codeAddr);
        if (entryPointInfo)
        {LOGMEIN("DiagStackFrame.cpp] 493\n");
            m_localVarSlotsOffset = entryPointInfo->localVarSlotsOffset;
            m_localVarChangedOffset = entryPointInfo->localVarChangedOffset;
        }
        else
        {
            AssertMsg(FALSE, "Failed to get entry point for native address. Most likely the frame is old/gone.");
        }
        OUTPUT_TRACE(Js::DebuggerPhase, _u("DiagNativeStackFrame::DiagNativeStackFrame: e.p(addr %p)=%p varOff=%d changedOff=%d\n"), codeAddr, entryPointInfo, m_localVarSlotsOffset, m_localVarChangedOffset);
    }

    JavascriptFunction* DiagNativeStackFrame::GetJavascriptFunction()
    {LOGMEIN("DiagStackFrame.cpp] 505\n");
        return m_function;
    }

    ScriptContext* DiagNativeStackFrame::GetScriptContext()
    {LOGMEIN("DiagStackFrame.cpp] 510\n");
        return m_function->GetScriptContext();
    }

    int DiagNativeStackFrame::GetByteCodeOffset()
    {LOGMEIN("DiagStackFrame.cpp] 515\n");
        return m_byteCodeOffset;
    }

    // Address on stack that belongs to current frame.
    // Currently we only use this to determine which of given frames is above/below another one.
    DWORD_PTR DiagNativeStackFrame::GetStackAddress()
    {LOGMEIN("DiagStackFrame.cpp] 522\n");
        return reinterpret_cast<DWORD_PTR>(m_stackAddr);
    }

    Var DiagNativeStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {LOGMEIN("DiagStackFrame.cpp] 527\n");
        Js::Var *varPtr = GetSlotOffsetLocation(slotId, allowTemp);
        return (varPtr != NULL) ? *varPtr : NULL;
    }

    Var * DiagNativeStackFrame::GetSlotOffsetLocation(RegSlot slotId, bool allowTemp)
    {LOGMEIN("DiagStackFrame.cpp] 533\n");
        Assert(GetFunction() != NULL);

        int32 slotOffset;
        if (GetFunction()->GetSlotOffset(slotId, &slotOffset, allowTemp))
        {LOGMEIN("DiagStackFrame.cpp] 538\n");
            Assert(m_localVarSlotsOffset != InvalidOffset);
            slotOffset = m_localVarSlotsOffset + slotOffset;

            // We will have the var offset only (which is always the Var size. With TypeSpecialization, below will change to accommodate double offset.
            return (Js::Var *)(((char *)m_stackAddr) + slotOffset);
        }

        Assert(false);
        return NULL;
    }

    Var DiagNativeStackFrame::GetNonVarRegValue(RegSlot slotId)
    {LOGMEIN("DiagStackFrame.cpp] 551\n");
        return GetRegValue(slotId);
    }

    void DiagNativeStackFrame::SetRegValue(RegSlot slotId, Var value)
    {LOGMEIN("DiagStackFrame.cpp] 556\n");
        Js::Var *varPtr = GetSlotOffsetLocation(slotId);
        Assert(varPtr != NULL);

        // First assign the value
        *varPtr = value;

        Assert(m_localVarChangedOffset != InvalidOffset);

        // Now change the bit in the stack which tells that current stack values got changed.
        char *stackOffset = (((char *)m_stackAddr) + m_localVarChangedOffset);

        Assert(*stackOffset == 0 || *stackOffset == FunctionBody::LocalsChangeDirtyValue);

        *stackOffset = FunctionBody::LocalsChangeDirtyValue;
    }

    Var DiagNativeStackFrame::GetArgumentsObject()
    {LOGMEIN("DiagStackFrame.cpp] 574\n");
        return (Var)((void **)m_stackAddr)[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    Var DiagNativeStackFrame::CreateHeapArguments()
    {LOGMEIN("DiagStackFrame.cpp] 579\n");
        // We would be creating the arguments object if there is no default arguments object present.
        Assert(GetArgumentsObject() == NULL);

        CallInfo const * callInfo  = (CallInfo const *)&(((void **)m_stackAddr)[JavascriptFunctionArgIndex_CallInfo]);

        // At the least we will have 'this' by default.
        Assert(callInfo->Count > 0);

        // Get the passed parameter's position (which is starting from 'this')
        Var * inParams = (Var *)&(((void **)m_stackAddr)[JavascriptFunctionArgIndex_This]);

        return JavascriptOperators::LoadHeapArguments(
                                        m_function,
                                        callInfo->Count - 1,
                                        &inParams[1],
                                        GetScriptContext()->GetLibrary()->GetNull(),
                                        (PropertyId*)GetScriptContext()->GetLibrary()->GetNull(),
                                        GetScriptContext(),
                                        /* formalsAreLetDecls */ false);
    }
#endif


    DiagRuntimeStackFrame::DiagRuntimeStackFrame(JavascriptFunction* function, PCWSTR displayName, void* stackAddr):
        m_function(function),
        m_displayName(displayName),
        m_stackAddr(stackAddr)
    {LOGMEIN("DiagStackFrame.cpp] 607\n");
    }

    JavascriptFunction* DiagRuntimeStackFrame::GetJavascriptFunction()
    {LOGMEIN("DiagStackFrame.cpp] 611\n");
        return m_function;
    }

    PCWSTR DiagRuntimeStackFrame::GetDisplayName()
    {LOGMEIN("DiagStackFrame.cpp] 616\n");
        return m_displayName;
    }

    DWORD_PTR DiagRuntimeStackFrame::GetStackAddress()
    {LOGMEIN("DiagStackFrame.cpp] 621\n");
        return reinterpret_cast<DWORD_PTR>(m_stackAddr);
    }

    int DiagRuntimeStackFrame::GetByteCodeOffset()
    {LOGMEIN("DiagStackFrame.cpp] 626\n");
        return 0;
    }

    Var DiagRuntimeStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {LOGMEIN("DiagStackFrame.cpp] 631\n");
        return nullptr;
    }

    Var DiagRuntimeStackFrame::GetNonVarRegValue(RegSlot slotId)
    {LOGMEIN("DiagStackFrame.cpp] 636\n");
        return nullptr;
    }

    void DiagRuntimeStackFrame::SetRegValue(RegSlot slotId, Var value)
    {LOGMEIN("DiagStackFrame.cpp] 641\n");
    }

    Var DiagRuntimeStackFrame::GetArgumentsObject()
    {LOGMEIN("DiagStackFrame.cpp] 645\n");
        return nullptr;
    }

    Var DiagRuntimeStackFrame::CreateHeapArguments()
    {LOGMEIN("DiagStackFrame.cpp] 650\n");
        return nullptr;
    }

}  // namespace Js
