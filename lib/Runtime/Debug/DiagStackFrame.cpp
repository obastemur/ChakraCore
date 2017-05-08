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
    {TRACE_IT(43038);
    }

    // Returns whether or not this frame is on the top of the callstack.
    bool DiagStackFrame::IsTopFrame()
    {TRACE_IT(43039);
        return this->isTopFrame && GetScriptContext()->GetDebugContext()->GetProbeContainer()->IsPrimaryBrokenToDebuggerContext();
    }

    void DiagStackFrame::SetIsTopFrame()
    {TRACE_IT(43040);
        this->isTopFrame = true;
    }

    ScriptFunction* DiagStackFrame::GetScriptFunction()
    {TRACE_IT(43041);
        return ScriptFunction::FromVar(GetJavascriptFunction());
    }

    FunctionBody* DiagStackFrame::GetFunction()
    {TRACE_IT(43042);
        return GetJavascriptFunction()->GetFunctionBody();
    }

    ScriptContext* DiagStackFrame::GetScriptContext()
    {TRACE_IT(43043);
        return GetJavascriptFunction()->GetScriptContext();
    }

    PCWSTR DiagStackFrame::GetDisplayName()
    {TRACE_IT(43044);
        return GetFunction()->GetExternalDisplayName();
    }

    bool DiagStackFrame::IsInterpreterFrame()
    {TRACE_IT(43045);
        return false;
    }

    InterpreterStackFrame* DiagStackFrame::AsInterpreterFrame()
    {
        AssertMsg(FALSE, "AsInterpreterFrame called for non-interpreter frame.");
        return nullptr;
    }

    ArenaAllocator * DiagStackFrame::GetArena()
    {TRACE_IT(43046);
        Assert(GetScriptContext() != NULL);
        return GetScriptContext()->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();
    }

    FrameDisplay * DiagStackFrame::GetFrameDisplay()
    {TRACE_IT(43047);
        FrameDisplay *display = NULL;

        Assert(this->GetFunction() != NULL);
        RegSlot frameDisplayReg = this->GetFunction()->GetFrameDisplayRegister();

        if (frameDisplayReg != Js::Constants::NoRegister && frameDisplayReg != 0)
        {TRACE_IT(43048);
            display = (FrameDisplay*)this->GetNonVarRegValue(frameDisplayReg);
        }
        else
        {TRACE_IT(43049);
            display = this->GetScriptFunction()->GetEnvironment();
        }
 
        return display;
    }

    Var DiagStackFrame::GetScopeObjectFromFrameDisplay(uint index)
    {TRACE_IT(43050);
        FrameDisplay * display = GetFrameDisplay();
        return (display != NULL && display->GetLength() > index) ? display->GetItem(index) : NULL;
    }

    Var DiagStackFrame::GetRootObject()
    {TRACE_IT(43051);
        Assert(this->GetFunction());
        return this->GetFunction()->LoadRootObject();
    }

    BOOL DiagStackFrame::IsStrictMode()
    {TRACE_IT(43052);
        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();
        return scopeFunction->IsStrictMode();
    }

    BOOL DiagStackFrame::IsThisAvailable()
    {TRACE_IT(43053);
        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();
        return !scopeFunction->IsLambda() || scopeFunction->GetParseableFunctionInfo()->GetCapturesThis();
    }

    Js::Var DiagStackFrame::GetThisFromFrame(Js::IDiagObjectAddress ** ppOutAddress, Js::IDiagObjectModelWalkerBase * localsWalker)
    {TRACE_IT(43054);
        Js::ScriptContext* scriptContext = this->GetScriptContext();
        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();
        Js::ModuleID moduleId = scopeFunction->IsScriptFunction() ? scopeFunction->GetFunctionBody()->GetModuleID() : 0;
        Js::Var varThis = scriptContext->GetLibrary()->GetNull();

        if (!scopeFunction->IsLambda())
        {TRACE_IT(43055);
            Js::JavascriptStackWalker::GetThis(&varThis, moduleId, scopeFunction, scriptContext);
        }
        else
        {TRACE_IT(43056);
            if (!scopeFunction->GetParseableFunctionInfo()->GetCapturesThis())
            {TRACE_IT(43057);
                return nullptr;
            }
            else
            {TRACE_IT(43058);
                // Emulate Js::JavascriptOperators::OP_GetThisScoped using a locals walker and assigning moduleId object if not found by locals walker
                if (localsWalker == nullptr)
                {TRACE_IT(43059);
                    ArenaAllocator *arena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();
                    localsWalker = Anew(arena, Js::LocalsWalker, this, Js::FrameWalkerFlags::FW_EnumWithScopeAlso | Js::FrameWalkerFlags::FW_AllowLexicalThis);
                }

                bool unused = false;
                Js::IDiagObjectAddress* address = localsWalker->FindPropertyAddress(Js::PropertyIds::_lexicalThisSlotSymbol, unused);

                if (ppOutAddress != nullptr)
                {TRACE_IT(43060);
                    *ppOutAddress = address;
                }

                if (address != nullptr)
                {TRACE_IT(43061);
                    varThis = address->GetValue(FALSE);
                }
                else if (moduleId == kmodGlobal)
                {TRACE_IT(43062);
                    varThis = Js::JavascriptOperators::OP_LdRoot(scriptContext)->ToThis();
                }
                else
                {TRACE_IT(43063);
                    varThis = (Var)Js::JavascriptOperators::GetModuleRoot(moduleId, scriptContext);
                }
            }
        }

        Js::GlobalObject::UpdateThisForEval(varThis, moduleId, scriptContext, this->IsStrictMode());

        return varThis;
    }

    void DiagStackFrame::TryFetchValueAndAddress(const char16 *source, int sourceLength, Js::ResolvedObject * pOutResolvedObj)
    {TRACE_IT(43064);
        Assert(source);
        Assert(pOutResolvedObj);

        Js::ScriptContext* scriptContext = this->GetScriptContext();
        Js::JavascriptFunction* scopeFunction = this->GetJavascriptFunction();

        // Do fast path for 'this', fields on slot, TODO : literals (integer,string)

        if (sourceLength == 4 && wcsncmp(source, _u("this"), 4) == 0)
        {TRACE_IT(43065);
            pOutResolvedObj->obj = this->GetThisFromFrame(&pOutResolvedObj->address);
            if (pOutResolvedObj->obj == nullptr)
            {TRACE_IT(43066);
                // TODO: Throw exception; this was not captured by the lambda
                Assert(scopeFunction->IsLambda());
                Assert(!scopeFunction->GetParseableFunctionInfo()->GetCapturesThis());
            }
        }
        else
        {TRACE_IT(43067);
            Js::PropertyRecord const * propRecord;
            scriptContext->FindPropertyRecord(source, sourceLength, &propRecord);
            if (propRecord != nullptr)
            {TRACE_IT(43068);
                ArenaAllocator *arena = scriptContext->GetThreadContext()->GetDebugManager()->GetDiagnosticArena()->Arena();

                Js::IDiagObjectModelWalkerBase * localsWalker = Anew(arena, Js::LocalsWalker, this, Js::FrameWalkerFlags::FW_EnumWithScopeAlso);

                bool isConst = false;
                pOutResolvedObj->address = localsWalker->FindPropertyAddress(propRecord->GetPropertyId(), isConst);
                if (pOutResolvedObj->address != nullptr)
                {TRACE_IT(43069);
                    pOutResolvedObj->obj = pOutResolvedObj->address->GetValue(FALSE);
                    pOutResolvedObj->isConst = isConst;
                }
            }
        }

    }

    Js::ScriptFunction* DiagStackFrame::TryGetFunctionForEval(Js::ScriptContext* scriptContext, const char16 *source, int sourceLength, BOOL isLibraryCode /* = FALSE */)
    {TRACE_IT(43070);
        // TODO: pass the real length of the source code instead of wcslen
        uint32 grfscr = fscrReturnExpression | fscrEval | fscrEvalCode | fscrGlobalCode | fscrConsoleScopeEval;
        if (!this->IsThisAvailable())
        {TRACE_IT(43071);
            grfscr |= fscrDebuggerErrorOnGlobalThis;
        }
        if (isLibraryCode)
        {TRACE_IT(43072);
            grfscr |= fscrIsLibraryCode;
        }
        return scriptContext->GetGlobalObject()->EvalHelper(scriptContext, source, sourceLength, kmodGlobal, grfscr, Js::Constants::EvalCode, FALSE, FALSE, this->IsStrictMode());
    }

    void DiagStackFrame::EvaluateImmediate(const char16 *source, int sourceLength, BOOL isLibraryCode, Js::ResolvedObject * resolvedObject)
    {TRACE_IT(43073);
        this->TryFetchValueAndAddress(source, sourceLength, resolvedObject);

        if (resolvedObject->obj == nullptr)
        {TRACE_IT(43074);
            Js::ScriptFunction* pfuncScript = this->TryGetFunctionForEval(this->GetScriptContext(), source, sourceLength, isLibraryCode);
            if (pfuncScript != nullptr)
            {TRACE_IT(43075);
                // Passing the nonuser code state from the enclosing function to the current function.
                // Treat native library frame (no function body) as non-user code.
                Js::FunctionBody* body = this->GetFunction();
                if (!body || body->IsNonUserCode())
                {TRACE_IT(43076);
                    Js::FunctionBody *pCurrentFuncBody = pfuncScript->GetFunctionBody();
                    if (pCurrentFuncBody != nullptr)
                    {TRACE_IT(43077);
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
    {TRACE_IT(43078);
        // For Conditional Object Mutation Breakpoint user can access the new value, changing property name and mutation type using special variables
        // $newValue$, $propertyName$ and $mutationType$. Add this variables to activation object.
        Js::DebugManager* debugManager = scriptContext->GetDebugContext()->GetProbeContainer()->GetDebugManager();
        Js::MutationBreakpoint *mutationBreakpoint = debugManager->GetActiveMutationBreakpoint();
        if (mutationBreakpoint != nullptr)
        {TRACE_IT(43079);
            if (Js::Constants::NoProperty == debugManager->mutationNewValuePid)
            {TRACE_IT(43080);
                debugManager->mutationNewValuePid = scriptContext->GetOrAddPropertyIdTracked(_u("$newValue$"), 10);
            }
            if (Js::Constants::NoProperty == debugManager->mutationPropertyNamePid)
            {TRACE_IT(43081);
                debugManager->mutationPropertyNamePid = scriptContext->GetOrAddPropertyIdTracked(_u("$propertyName$"), 14);
            }
            if (Js::Constants::NoProperty == debugManager->mutationTypePid)
            {TRACE_IT(43082);
                debugManager->mutationTypePid = scriptContext->GetOrAddPropertyIdTracked(_u("$mutationType$"), 14);
            }

            AssertMsg(debugManager->mutationNewValuePid != Js::Constants::NoProperty, "Should have a valid mutationNewValuePid");
            AssertMsg(debugManager->mutationPropertyNamePid != Js::Constants::NoProperty, "Should have a valid mutationPropertyNamePid");
            AssertMsg(debugManager->mutationTypePid != Js::Constants::NoProperty, "Should have a valid mutationTypePid");

            Js::Var newValue = mutationBreakpoint->GetBreakNewValueVar();

            // Incase of MutationTypeDelete we won't have new value
            if (nullptr != newValue)
            {TRACE_IT(43083);
                activeScopeObject->SetProperty(debugManager->mutationNewValuePid,
                    mutationBreakpoint->GetBreakNewValueVar(),
                    Js::PropertyOperationFlags::PropertyOperation_None,
                    nullptr);
            }
            else
            {TRACE_IT(43084);
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
    {TRACE_IT(43085);
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
            {TRACE_IT(43086);
                propIdtoDiagAddressMap->AddNew(resolveObject.propId, resolveObject.address);
            }
        });
        if (!activeScopeObject)
        {TRACE_IT(43087);
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
        {TRACE_IT(43088);
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
        {TRACE_IT(43089);
            Js::PropertyId propertyId = activeScopeObject->GetPropertyId((Js::PropertyIndex)i);
            if (propertyId != Js::Constants::NoProperty)
            {TRACE_IT(43090);
                Js::Var value;
                if (Js::JavascriptOperators::GetProperty(activeScopeObject, propertyId, &value, scriptContext))
                {TRACE_IT(43091);
                    Js::IDiagObjectAddress * pAddress = nullptr;
                    if (propIdtoDiagAddressMap->TryGetValue(propertyId, &pAddress))
                    {TRACE_IT(43092);
                        Assert(pAddress);
                        if (pAddress->GetValue(FALSE) != value)
                        {TRACE_IT(43093);
                            pAddress->Set(value);
                        }
                    }
                }
            }
        }

        return varResult;
    }

    Var DiagStackFrame::GetInnerScopeFromRegSlot(RegSlot location)
    {TRACE_IT(43094);
        return GetNonVarRegValue(location);
    }

    DiagInterpreterStackFrame::DiagInterpreterStackFrame(InterpreterStackFrame* frame) :
        m_interpreterFrame(frame)
    {TRACE_IT(43095);
        Assert(m_interpreterFrame != NULL);
        AssertMsg(m_interpreterFrame->GetScriptContext() && m_interpreterFrame->GetScriptContext()->IsScriptContextInDebugMode(),
            "This only supports interpreter stack frames running in debug mode.");
    }

    JavascriptFunction* DiagInterpreterStackFrame::GetJavascriptFunction()
    {TRACE_IT(43096);
        return m_interpreterFrame->GetJavascriptFunction();
    }

    ScriptContext* DiagInterpreterStackFrame::GetScriptContext()
    {TRACE_IT(43097);
        return m_interpreterFrame->GetScriptContext();
    }

    int DiagInterpreterStackFrame::GetByteCodeOffset()
    {TRACE_IT(43098);
        return m_interpreterFrame->GetReader()->GetCurrentOffset();
    }

    // Address on stack that belongs to current frame.
    // Currently we only use this to determine which of given frames is above/below another one.
    DWORD_PTR DiagInterpreterStackFrame::GetStackAddress()
    {TRACE_IT(43099);
        return m_interpreterFrame->GetStackAddress();
    }

    bool DiagInterpreterStackFrame::IsInterpreterFrame()
    {TRACE_IT(43100);
        return true;
    }

    InterpreterStackFrame* DiagInterpreterStackFrame::AsInterpreterFrame()
    {TRACE_IT(43101);
        return m_interpreterFrame;
    }

    Var DiagInterpreterStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {TRACE_IT(43102);
        return m_interpreterFrame->GetReg(slotId);
    }

    Var DiagInterpreterStackFrame::GetNonVarRegValue(RegSlot slotId)
    {TRACE_IT(43103);
        return m_interpreterFrame->GetNonVarReg(slotId);
    }

    void DiagInterpreterStackFrame::SetRegValue(RegSlot slotId, Var value)
    {TRACE_IT(43104);
        m_interpreterFrame->SetReg(slotId, value);
    }

    Var DiagInterpreterStackFrame::GetArgumentsObject()
    {TRACE_IT(43105);
        return m_interpreterFrame->GetArgumentsObject();
    }

    Var DiagInterpreterStackFrame::CreateHeapArguments()
    {TRACE_IT(43106);
        return m_interpreterFrame->CreateHeapArguments(GetScriptContext());
    }

    FrameDisplay * DiagInterpreterStackFrame::GetFrameDisplay()
    {TRACE_IT(43107);
        return m_interpreterFrame->GetFrameDisplayForNestedFunc();
    }

    Var DiagInterpreterStackFrame::GetInnerScopeFromRegSlot(RegSlot location)
    {TRACE_IT(43108);
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
    {TRACE_IT(43109);
        Assert(m_stackAddr != NULL);
        AssertMsg(m_function && m_function->GetScriptContext() && m_function->GetScriptContext()->IsScriptContextInDebugMode(),
            "This only supports functions in debug mode.");

        FunctionEntryPointInfo * entryPointInfo = GetFunction()->GetEntryPointFromNativeAddress((DWORD_PTR)codeAddr);
        if (entryPointInfo)
        {TRACE_IT(43110);
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
    {TRACE_IT(43111);
        return m_function;
    }

    ScriptContext* DiagNativeStackFrame::GetScriptContext()
    {TRACE_IT(43112);
        return m_function->GetScriptContext();
    }

    int DiagNativeStackFrame::GetByteCodeOffset()
    {TRACE_IT(43113);
        return m_byteCodeOffset;
    }

    // Address on stack that belongs to current frame.
    // Currently we only use this to determine which of given frames is above/below another one.
    DWORD_PTR DiagNativeStackFrame::GetStackAddress()
    {TRACE_IT(43114);
        return reinterpret_cast<DWORD_PTR>(m_stackAddr);
    }

    Var DiagNativeStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {TRACE_IT(43115);
        Js::Var *varPtr = GetSlotOffsetLocation(slotId, allowTemp);
        return (varPtr != NULL) ? *varPtr : NULL;
    }

    Var * DiagNativeStackFrame::GetSlotOffsetLocation(RegSlot slotId, bool allowTemp)
    {TRACE_IT(43116);
        Assert(GetFunction() != NULL);

        int32 slotOffset;
        if (GetFunction()->GetSlotOffset(slotId, &slotOffset, allowTemp))
        {TRACE_IT(43117);
            Assert(m_localVarSlotsOffset != InvalidOffset);
            slotOffset = m_localVarSlotsOffset + slotOffset;

            // We will have the var offset only (which is always the Var size. With TypeSpecialization, below will change to accommodate double offset.
            return (Js::Var *)(((char *)m_stackAddr) + slotOffset);
        }

        Assert(false);
        return NULL;
    }

    Var DiagNativeStackFrame::GetNonVarRegValue(RegSlot slotId)
    {TRACE_IT(43118);
        return GetRegValue(slotId);
    }

    void DiagNativeStackFrame::SetRegValue(RegSlot slotId, Var value)
    {TRACE_IT(43119);
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
    {TRACE_IT(43120);
        return (Var)((void **)m_stackAddr)[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    Var DiagNativeStackFrame::CreateHeapArguments()
    {TRACE_IT(43121);
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
    {TRACE_IT(43122);
    }

    JavascriptFunction* DiagRuntimeStackFrame::GetJavascriptFunction()
    {TRACE_IT(43123);
        return m_function;
    }

    PCWSTR DiagRuntimeStackFrame::GetDisplayName()
    {TRACE_IT(43124);
        return m_displayName;
    }

    DWORD_PTR DiagRuntimeStackFrame::GetStackAddress()
    {TRACE_IT(43125);
        return reinterpret_cast<DWORD_PTR>(m_stackAddr);
    }

    int DiagRuntimeStackFrame::GetByteCodeOffset()
    {TRACE_IT(43126);
        return 0;
    }

    Var DiagRuntimeStackFrame::GetRegValue(RegSlot slotId, bool allowTemp)
    {TRACE_IT(43127);
        return nullptr;
    }

    Var DiagRuntimeStackFrame::GetNonVarRegValue(RegSlot slotId)
    {TRACE_IT(43128);
        return nullptr;
    }

    void DiagRuntimeStackFrame::SetRegValue(RegSlot slotId, Var value)
    {TRACE_IT(43129);
    }

    Var DiagRuntimeStackFrame::GetArgumentsObject()
    {TRACE_IT(43130);
        return nullptr;
    }

    Var DiagRuntimeStackFrame::CreateHeapArguments()
    {TRACE_IT(43131);
        return nullptr;
    }

}  // namespace Js
