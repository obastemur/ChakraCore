//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

void
Inline::Optimize()
{LOGMEIN("Inline.cpp] 8\n");
    this->Optimize(this->topFunc);
}

void
Inline::Optimize(Func *func, __in_ecount_opt(callerArgOutCount) IR::Instr *callerArgOuts[], Js::ArgSlot callerArgOutCount, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 14\n");
    if (!func->DoInline() || !topFunc->DoInline() || func->GetJITFunctionBody()->IsAsmJsMode()) // disable inlining for asm
    {LOGMEIN("Inline.cpp] 16\n");
        return;
    }

    bool doFixedMethods = !PHASE_OFF(Js::FixedMethodsPhase, func);
    const FunctionJITTimeInfo * inlinerData = func->GetWorkItem()->GetJITTimeInfo();

    bool doInline = (inlinerData->GetInlineeCount() > 0 || inlinerData->IsLdFldInlineePresent());
    if (PHASE_OFF(Js::InlinePhase, this->topFunc) ||
        PHASE_OFF(Js::InlinePhase, func) ||
        func->IsJitInDebugMode())
    {LOGMEIN("Inline.cpp] 27\n");
        doInline = false;
    }

    func->actualCount = callerArgOutCount;

    // Current for..in depth starts with the inlinee's base depth
    this->currentForInDepth = func->m_forInLoopBaseDepth;

    // Keep the caller's "this" symbol (if any).
    StackSym *symThis = nullptr;
    lastStatementBoundary = nullptr;
    IR::LabelInstr* loopTop = nullptr;
    int32 backEdgeCount = 0;

    // Profile data already filter call site outside of loops if the function has loops, so we don't need to detect that here.
    FOREACH_INSTR_EDITING(instr, instrNext, func->m_headInstr)
    {LOGMEIN("Inline.cpp] 44\n");
        bool isInlined = false;
        bool isPolymorphic = false;
        bool isBuiltIn = false;
        bool isCtor = false;
        if (doInline)
        {LOGMEIN("Inline.cpp] 50\n");
            switch (instr->m_opcode)
            {LOGMEIN("Inline.cpp] 52\n");
            case Js::OpCode::StatementBoundary:
                lastStatementBoundary = instr->AsPragmaInstr();
                break;

            case Js::OpCode::Label:
                {LOGMEIN("Inline.cpp] 58\n");
                    if (!loopTop && instr->AsLabelInstr()->m_isLoopTop)
                    {LOGMEIN("Inline.cpp] 60\n");
                        // We only need to know if we are inside loop or not, it doesn't matter how many nested levels we are in.
                        // This is the cheap way of doing so.
                        loopTop = instr->AsLabelInstr();
                        AnalysisAssert(loopTop);
                        this->isInLoop++;
                        backEdgeCount = loopTop->labelRefs.Count();
                    }

                    if (instr->AsLabelInstr()->m_isForInExit)
                    {LOGMEIN("Inline.cpp] 70\n");
                        Assert(this->currentForInDepth != 0);
                        this->currentForInDepth--;
                    }
                }
                break;
            case Js::OpCode::InitForInEnumerator:
                // Loop body uses the for in enumerator in the interpreter frame.
                // No need to keep track of its depth
                if (!func->IsLoopBody())
                {LOGMEIN("Inline.cpp] 80\n");
                    this->currentForInDepth++;
                }
                break;
            case Js::OpCode::BrOnNotEmpty:
                // Byte code doesn't emit BrOnNotEmpty, and we have done any transformation yet.
                Assert(false);
                break;
            case Js::OpCode::BrOnEmpty:
                // Loop body uses the for in enumerator in the interpreter frame.
                // No need to keep track of its depth
                if (!func->IsLoopBody())
                {LOGMEIN("Inline.cpp] 92\n");
                    instr->AsBranchInstr()->GetTarget()->m_isForInExit = true;
                }
                break;
            case Js::OpCode::StFld:
            case Js::OpCode::LdFld:
            case Js::OpCode::LdFldForCallApplyTarget:
                {LOGMEIN("Inline.cpp] 99\n");
                    // Try inlining of getter setter
                    if (!inlinerData->IsLdFldInlineePresent())
                    {LOGMEIN("Inline.cpp] 102\n");
                        break;
                    }

                    if (!instr->IsProfiledInstr())
                    {LOGMEIN("Inline.cpp] 107\n");
                        break;
                    }

                    if (!(instr->AsProfiledInstr()->u.FldInfo().flags & Js::FldInfoFlags::FldInfo_FromAccessor))
                    {LOGMEIN("Inline.cpp] 112\n");
                        break;
                    }

                    bool getter = instr->m_opcode != Js::OpCode::StFld;

                    IR::Opnd *opnd = getter ? instr->GetSrc1() : instr->GetDst();
                    if (!(opnd && opnd->IsSymOpnd()))
                    {LOGMEIN("Inline.cpp] 120\n");
                        break;
                    }

                    IR::SymOpnd* symOpnd = opnd->AsSymOpnd();
                    if (!symOpnd->m_sym->IsPropertySym())
                    {LOGMEIN("Inline.cpp] 126\n");
                        break;
                    }
                    Assert(symOpnd->AsSymOpnd()->IsPropertySymOpnd());

                    const auto inlineCacheIndex = symOpnd->AsPropertySymOpnd()->m_inlineCacheIndex;
                    const FunctionJITTimeInfo * inlineeData = inlinerData->GetLdFldInlinee(inlineCacheIndex);
                    if (!inlineeData)
                    {LOGMEIN("Inline.cpp] 134\n");
                        break;
                    }

                    JITTimeFunctionBody * body = inlineeData->GetBody();
                    if (!body)
                    {LOGMEIN("Inline.cpp] 140\n");
#ifdef ENABLE_DOM_FAST_PATH
                        Assert(inlineeData->GetLocalFunctionId() == Js::JavascriptBuiltInFunction::DOMFastPathGetter ||
                            inlineeData->GetLocalFunctionId() == Js::JavascriptBuiltInFunction::DOMFastPathSetter);
                        if (PHASE_OFF1(Js::InlineHostCandidatePhase))
                        {LOGMEIN("Inline.cpp] 145\n");
                            break;
                        }
                        this->InlineDOMGetterSetterFunction(instr, inlineeData, inlinerData);
#endif
                        break;
                    }

                    bool isInlinePhaseOff = PHASE_OFF(Js::InlineCandidatePhase, inlineeData) ||
                                            PHASE_OFF(Js::InlineAccessorsPhase, inlineeData) ||
                                            (getter && PHASE_OFF(Js::InlineGettersPhase, inlineeData)) ||
                                            (!getter && PHASE_OFF(Js::InlineSettersPhase, inlineeData));

                    if (isInlinePhaseOff)
                    {LOGMEIN("Inline.cpp] 159\n");
                        break;
                    }

                    this->InlineGetterSetterFunction(instr, inlineeData, symThis, inlineCacheIndex, getter /*isGetter*/, &isInlined, recursiveInlineDepth);

                    break;
                }

            case Js::OpCode::NewScObjArray:
                // We know we're not going to inline these. Just break out and try to do a fixed function check.
                isCtor = true;
                isBuiltIn = true;
                break;

            case Js::OpCode::NewScObject:
                isCtor = true;
                if (PHASE_OFF(Js::InlineConstructorsPhase, this->topFunc))
                {LOGMEIN("Inline.cpp] 177\n");
                    break;
                }
                // fall-through

            case Js::OpCode::CallI:
                {LOGMEIN("Inline.cpp] 183\n");

                    IR::PropertySymOpnd* methodValueOpnd = GetMethodLdOpndForCallInstr(instr);

                    if (this->inlineesProcessed == inlinerData->GetInlineeCount())
                    {LOGMEIN("Inline.cpp] 188\n");
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        break;
                    }

                    if(!instr->IsProfiledInstr())
                    {LOGMEIN("Inline.cpp] 195\n");
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        break;
                    }

                    const auto profileId = static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId);
                    if(profileId >= func->GetJITFunctionBody()->GetProfiledCallSiteCount())
                    {LOGMEIN("Inline.cpp] 203\n");
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        break;
                    }

                    const auto inlineeData = inlinerData->GetInlinee(profileId);
                    if(!inlineeData)
                    {LOGMEIN("Inline.cpp] 211\n");
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        break;
                    }

                    if(inlinerData->IsPolymorphicCallSite(profileId))
                    {LOGMEIN("Inline.cpp] 218\n");
                        isPolymorphic = true;
                        if (isCtor ||
                            (PHASE_OFF(Js::PolymorphicInlinePhase, this->topFunc) || PHASE_OFF(Js::PolymorphicInlinePhase, func)) ||
                            (this->IsInliningOutSideLoops() && !PHASE_FORCE(Js::InlinePhase, this->topFunc) && !PHASE_FORCE(Js::InlinePhase, func)))
                        {LOGMEIN("Inline.cpp] 223\n");
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
                            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
                            POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Skip Inline: Inlining polymorphic call site outside loop\tIsConstructorCall: %s \tisTopFunc: %s\tCaller: %s (%s)\n"),
                                     (isCtor? _u("true"): _u("false")), (this->topFunc != func? _u("true"):_u("false")),
                                     inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));

                            // TODO: Constructor polymorphic inlining

                            TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                            TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                            break;
                        }
                        if (!PHASE_OFF(Js::FixedMethodsPhase, this->topFunc) && !PHASE_OFF(Js::PolymorphicInlineFixedMethodsPhase, this->topFunc))
                        {LOGMEIN("Inline.cpp] 238\n");
                            instrNext = InlinePolymorphicFunctionUsingFixedMethods(instr, inlinerData, symThis, profileId, methodValueOpnd, &isInlined, recursiveInlineDepth);
                        }
                        else
                        {
                            TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                            TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                            instrNext = InlinePolymorphicFunction(instr, inlinerData, symThis, profileId, &isInlined, recursiveInlineDepth);
                        }
                    }
                    else
                    {
                        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                        Js::OpCode builtInInlineCandidateOpCode;
                        ValueType builtInReturnType;

                        // If the inlinee info is the array constructor, just change the opcode to NewScObjArray
                        // so that we will inline the array allocation in lower
                        if (isCtor && inlineeData->GetFunctionInfoAddr() == this->topFunc->GetThreadContextInfo()->GetJavascriptArrayNewInstanceAddr())
                        {LOGMEIN("Inline.cpp] 258\n");
                            isBuiltIn = true;
                            instr->m_opcode = Js::OpCode::NewScObjArray;
                            instr->AsProfiledInstr()->u.profileId = Js::Constants::NoProfileId;
                            break;
                        }

                        isBuiltIn = InliningDecider::GetBuiltInInfo(inlineeData, &builtInInlineCandidateOpCode, &builtInReturnType);

                        if(!builtInReturnType.IsUninitialized() && instr->GetDst())
                        {LOGMEIN("Inline.cpp] 268\n");
                            Assert(!inlineeData->HasBody());
                            AssertMsg(instr->m_opcode != Js::OpCode::NewScObjArray, "We should have broken out of the switch statement earlier on this opcode.");
                            // Value types for the array built-in calls are pulled from the profile; don't change them here.
                            if ((instr->m_opcode != Js::OpCode::NewScObjArray) ||
                                !instr->GetDst()->GetValueType().IsLikelyNativeArray())
                            {LOGMEIN("Inline.cpp] 274\n");
                                // Assume that this built-in function is not going to be inlined, so the return type cannot be definite
                                instr->GetDst()->SetValueType(builtInReturnType.ToLikely());
                            }
                        }

                        bool isInlinePhaseOff = inlineeData->HasBody() ?
                            PHASE_OFF(Js::InlineCandidatePhase, inlineeData) :
                            PHASE_OFF1(Js::InlineBuiltInPhase);
                        if (isInlinePhaseOff)
                        {LOGMEIN("Inline.cpp] 284\n");
                            break;
                        }

                        if(!inlineeData->HasBody() && builtInInlineCandidateOpCode == 0)
                        {LOGMEIN("Inline.cpp] 289\n");
                            // This built-in function is not going to be inlined
                            break;
                        }

                        if(!inlineeData->HasBody())
                        {LOGMEIN("Inline.cpp] 295\n");
                            Assert(builtInInlineCandidateOpCode != 0);
                            if(isCtor)
                            {LOGMEIN("Inline.cpp] 298\n");
                                // Inlining a built-in function called as a constructor is currently not supported. Although InliningDecider
                                // already checks for this, profile data matching with a function does not take into account the difference
                                // between a constructor call and a regular function call, so need to check it again.
                                break;
                            }

                            // This built-in function is going to be inlined, so reset the destination's value type
                            if(!builtInReturnType.IsUninitialized())
                            {LOGMEIN("Inline.cpp] 307\n");
                                if(instr->GetDst())
                                {LOGMEIN("Inline.cpp] 309\n");
                                    instr->GetDst()->SetValueType(builtInReturnType);
                                    if(builtInReturnType.IsDefinite())
                                    {LOGMEIN("Inline.cpp] 312\n");
                                        instr->GetDst()->SetValueTypeFixed();
                                    }
                                }
                            }
                        }
                        else
                        {

                            if (!inlineeData->GetBody()->HasProfileInfo())        // Don't try to inline a function if it doesn't have profile data
                            {LOGMEIN("Inline.cpp] 322\n");
                                break;
                            }

                            uint16 constantArguments = 0;
                            if (!PHASE_OFF(Js::InlineRecursivePhase, func))
                            {LOGMEIN("Inline.cpp] 328\n");
                                instr->IterateArgInstrs([&](IR::Instr* argInstr) {
                                    IR::Opnd *src1 = argInstr->GetSrc1();
                                    if (!src1->IsRegOpnd())
                                    {LOGMEIN("Inline.cpp] 332\n");
                                        return false;
                                    }
                                    StackSym *sym = src1->AsRegOpnd()->m_sym;
                                    if (sym->IsIntConst())
                                    {LOGMEIN("Inline.cpp] 337\n");
                                        if (argInstr->GetSrc2() && argInstr->GetSrc2()->IsSymOpnd())
                                        {LOGMEIN("Inline.cpp] 339\n");
                                            StackSym *dstSym = argInstr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
                                            Assert(dstSym->IsSingleDef());
                                            Assert(dstSym->IsArgSlotSym());
                                            Js::ArgSlot argCount = dstSym->GetArgSlotNum() - 1;

                                            if (argCount == Js::Constants::MaximumArgumentCountForConstantArgumentInlining)
                                            {LOGMEIN("Inline.cpp] 346\n");
                                                return true;
                                            }
                                            constantArguments |= (1 << argCount);
                                        }
                                    }
                                    return false;
                                });
                            }

                            if (!inliningHeuristics.BackendInlineIntoInliner(inlineeData,
                                func, this->topFunc, profileId, isCtor, true /*isFixedMethodCall*/,
                                this->IsInliningOutSideLoops(), this->isInLoop != 0, recursiveInlineDepth, constantArguments))
                            {LOGMEIN("Inline.cpp] 359\n");
                                break;
                            }

                        }


                        instrNext = builtInInlineCandidateOpCode != 0 ?
                            this->InlineBuiltInFunction(instr, inlineeData, builtInInlineCandidateOpCode, inlinerData, symThis, &isInlined, profileId, recursiveInlineDepth) :
                            this->InlineScriptFunction(instr, inlineeData, symThis, profileId, &isInlined, recursiveInlineDepth);

                    }
                    if(++this->inlineesProcessed == inlinerData->GetInlineeCount())
                    {LOGMEIN("Inline.cpp] 372\n");
                        // getterSetter inline caches are shared and we have no way of knowing how many more are present
                        if (!inlinerData->IsLdFldInlineePresent() && !doFixedMethods)
                        {LOGMEIN("Inline.cpp] 375\n");
                            return ;
                        }
                    }
                    break;
                }

            case Js::OpCode::CallIExtended:
            {LOGMEIN("Inline.cpp] 383\n");
                if (this->inlineesProcessed == inlinerData->GetInlineeCount())
                {LOGMEIN("Inline.cpp] 385\n");
                    break;
                }

                if (!instr->IsProfiledInstr())
                {LOGMEIN("Inline.cpp] 390\n");
                    break;
                }

                const auto profileId = static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId);
                if (profileId >= func->GetJITFunctionBody()->GetProfiledCallSiteCount())
                {LOGMEIN("Inline.cpp] 396\n");
                    break;
                }

                const auto inlineeData = inlinerData->GetInlinee(profileId);
                if (!inlineeData)
                {LOGMEIN("Inline.cpp] 402\n");
                    break;
                }

                if (Lowerer::IsSpreadCall(instr))
                {LOGMEIN("Inline.cpp] 407\n");
                    InlineSpread(instr);
                }
                break;
            }

            case Js::OpCode::ArgOut_A:
                InlConstFoldArg(instr, callerArgOuts, callerArgOutCount);
                break;

            case Js::OpCode::LdThis:
                Assert(instr->GetDst() && instr->GetDst()->IsRegOpnd());
                Assert(symThis == nullptr);

                symThis = instr->GetDst()->AsRegOpnd()->m_sym;
                break;

            case Js::OpCode::CheckThis:
                // Is this possible? Can we be walking an inlinee here? Doesn't hurt to support this case...
                Assert(instr->GetSrc1() && instr->GetSrc1()->IsRegOpnd());
                Assert(symThis == nullptr);

                symThis = instr->GetSrc1()->AsRegOpnd()->m_sym;
                break;

            default:
                {LOGMEIN("Inline.cpp] 433\n");
                    if (loopTop && instr->IsBranchInstr())
                    {LOGMEIN("Inline.cpp] 435\n");
                        // Look for the back edge to loopTop.
                        IR::BranchInstr *branch = instr->AsBranchInstr();
                        IR::LabelInstr *labelDestination = branch->GetTarget();
                        if (labelDestination == loopTop) // We found the back edge
                        {LOGMEIN("Inline.cpp] 440\n");
                            backEdgeCount--;
                            if (backEdgeCount == 0) // We have seen all the back edges, hence we are outside loop now.
                            {LOGMEIN("Inline.cpp] 443\n");
                                Assert(this->isInLoop > 0);
                                --this->isInLoop;
                                loopTop = nullptr;
                            }
                        }
                    }
                }

            }
        }

        // If we chose not to inline, let's try to optimize this call if it uses a fixed method
        if (!isInlined)
        {LOGMEIN("Inline.cpp] 457\n");
            switch (instr->m_opcode)
            {LOGMEIN("Inline.cpp] 459\n");
            case Js::OpCode::NewScObject:
            case Js::OpCode::NewScObjArray:
                isCtor = true;
                // intentionally fall through.
            case Js::OpCode::CallI:
                {LOGMEIN("Inline.cpp] 465\n");
                    IR::PropertySymOpnd* methodValueOpnd = GetMethodLdOpndForCallInstr(instr);

                    TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
                    TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
                    StackSym* originalCallTargetStackSym = instr->GetSrc1()->GetStackSym();
                    bool originalCallTargetOpndIsJITOpt = instr->GetSrc1()->GetIsJITOptimizedReg();
                    bool safeThis = false;
                    if (TryOptimizeCallInstrWithFixedMethod(instr, nullptr, isPolymorphic /*isPolymorphic*/, isBuiltIn /*isBuiltIn*/, isCtor /*isCtor*/, false /*isInlined*/, safeThis /*unused here*/))
                    {LOGMEIN("Inline.cpp] 474\n");
                        Assert(originalCallTargetStackSym != nullptr);

                        // Insert a ByteCodeUsesInstr to make sure the methodValueDstOpnd's constant value is captured by any
                        // bailout that occurs between CheckFixedMethodField and CallI.
                        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(instr);
                        useCallTargetInstr->SetRemovedOpndSymbol(originalCallTargetOpndIsJITOpt, originalCallTargetStackSym->m_id);
                        instr->InsertBefore(useCallTargetInstr);

                        // Split NewScObject into NewScObjectNoCtor and CallI, but don't touch NewScObjectArray.
                        if (instr->m_opcode == Js::OpCode::NewScObject && !PHASE_OFF(Js::SplitNewScObjectPhase, this->topFunc))
                        {
                            SplitConstructorCall(instr, false, true);
                        }
                    }
                    else if (instr->m_opcode == Js::OpCode::NewScObjArray)
                    {LOGMEIN("Inline.cpp] 490\n");
                        if (instr->GetDst() && instr->GetDst()->GetValueType().IsLikelyNativeArray())
                        {LOGMEIN("Inline.cpp] 492\n");
                            // We expect to create a native array here, so we'll insert a check against the
                            // expected call target, which requires a bailout.
                            instr = instr->ConvertToBailOutInstr(instr, IR::BailOutOnNotNativeArray);
                        }
                    }
                }
                break;
            }
        }
    } NEXT_INSTR_EDITING;

    INLINE_FLUSH();
}

uint Inline::FillInlineesDataArray(
        const FunctionJITTimeInfo* inlineeJitTimeData,
        _Out_writes_to_(inlineesDataArrayLength, (return >= inlineesDataArrayLength ? inlineesDataArrayLength : return)) const FunctionJITTimeInfo ** inlineesDataArray,
        uint inlineesDataArrayLength
        )
{LOGMEIN("Inline.cpp] 512\n");
    uint inlineeCount = 0;
    while(inlineeJitTimeData)
    {LOGMEIN("Inline.cpp] 515\n");
        if (inlineeCount >= inlineesDataArrayLength)
        {LOGMEIN("Inline.cpp] 517\n");
            // Count the actual number of inlinees for logging.
            while (inlineeJitTimeData)
            {LOGMEIN("Inline.cpp] 520\n");
                inlineeCount++;
                inlineeJitTimeData = inlineeJitTimeData->GetNext();
            }
            return inlineeCount;
        }

        intptr_t inlineeFunctionInfoAddr = inlineeJitTimeData->GetFunctionInfoAddr();
#ifdef DBG
        if (inlineeJitTimeData->HasBody() && !PHASE_OFF(Js::PolymorphicInlinePhase, inlineeJitTimeData))
#endif
        {LOGMEIN("Inline.cpp] 531\n");
            const FunctionJITTimeInfo* rightInlineeJitTimeData = inlineeJitTimeData->GetJitTimeDataFromFunctionInfoAddr(inlineeFunctionInfoAddr);

            if (rightInlineeJitTimeData)
            {LOGMEIN("Inline.cpp] 535\n");
                inlineesDataArray[inlineeCount] = rightInlineeJitTimeData;
                Assert(rightInlineeJitTimeData->GetBody() == inlineeJitTimeData->GetBody());
#ifdef DBG
                for (uint k = 0; k < inlineeCount; k++)
                {LOGMEIN("Inline.cpp] 540\n");
                    if (inlineesDataArray[k]->GetBody()  == inlineeJitTimeData->GetBody())
                    {
                        AssertMsg(false, "We should never see duplicate function body here");
                    }
                }
#endif
                inlineeCount++;
            }
            else
            {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
                POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Missing jit time data skipped inlinee\tInlinee: %s (%s)\n"),
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer));
            }
        }
        inlineeJitTimeData = inlineeJitTimeData->GetNext();
    }

    return inlineeCount;
}

void Inline::FillInlineesDataArrayUsingFixedMethods(
    const FunctionJITTimeInfo* inlineeJitTimeData,
        __inout_ecount(inlineesDataArrayLength) const FunctionJITTimeInfo ** inlineesDataArray,
        uint inlineesDataArrayLength,
        __inout_ecount(cachedFixedInlineeCount) JITTimeFixedField* fixedFieldInfoArray,
        uint16 cachedFixedInlineeCount
        )
{LOGMEIN("Inline.cpp] 571\n");
    AnalysisAssert(cachedFixedInlineeCount <= inlineesDataArrayLength);

    JITTimeFunctionBody* inlineeFuncBody = nullptr;
    while (inlineeJitTimeData)
    {LOGMEIN("Inline.cpp] 576\n");
        if (inlineeJitTimeData->HasBody())
        {LOGMEIN("Inline.cpp] 578\n");
             inlineeFuncBody = inlineeJitTimeData->GetBody();
            if (!PHASE_OFF(Js::PolymorphicInlinePhase, inlineeJitTimeData) && !PHASE_OFF(Js::PolymorphicInlineFixedMethodsPhase, inlineeJitTimeData))
            {LOGMEIN("Inline.cpp] 581\n");
                const FunctionJITTimeInfo * jitTimeData = inlineeJitTimeData->GetJitTimeDataFromFunctionInfoAddr(inlineeJitTimeData->GetFunctionInfoAddr());
                if (jitTimeData)
                {LOGMEIN("Inline.cpp] 584\n");
                    for (uint16 i = 0; i < cachedFixedInlineeCount; i++)
                    {LOGMEIN("Inline.cpp] 586\n");
                        if (inlineeJitTimeData->GetFunctionInfoAddr() == fixedFieldInfoArray[i].GetFuncInfoAddr())
                        {LOGMEIN("Inline.cpp] 588\n");
                            inlineesDataArray[i] = inlineeJitTimeData->GetJitTimeDataFromFunctionInfoAddr(inlineeJitTimeData->GetFunctionInfoAddr());
                            break;
                        }
                    }
                }
                else
                {
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
                    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
                    POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Missing jit time data skipped inlinee\tInlinee: %s (%s)\n"),
                                inlineeFuncBody->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer));
                }
            }
        }
        inlineeJitTimeData = inlineeJitTimeData->GetNext();
    }
}

IR::Instr *
Inline::InlinePolymorphicFunctionUsingFixedMethods(IR::Instr *callInstr, const FunctionJITTimeInfo * inlinerData, const StackSym *symCallerThis, const Js::ProfileId profileId, IR::PropertySymOpnd* methodValueOpnd, bool* pIsInlined, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 610\n");
    IR::Instr* instrNext = callInstr->m_next;
    *pIsInlined = false;

    const FunctionJITTimeInfo* inlineeJitTimeData = inlinerData->GetInlinee(profileId);
    AnalysisAssert(inlineeJitTimeData);

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    // Abort conditions:
    if(!inlineeJitTimeData->GetNext())
    {LOGMEIN("Inline.cpp] 624\n");
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Skip Inline: Missing JitTime data \tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                 inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                 inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));

        // There are no multiple codegen jit-time data allocated for this call site, not sure how is this possible, abort
        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
        return instrNext;
    }

    // arguments exceed MaxInlineeArgoutCount
    if (callInstr->GetSrc2() &&
        callInstr->GetSrc2()->IsSymOpnd() &&
        callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
    {LOGMEIN("Inline.cpp] 639\n");
        // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo. Although
        // InliningDecider already checks for this, the check is against profile data that may not be accurate since profile
        // data matching does not take into account some types of changes to source code. Need to check this again with current
        // information.
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Skip Inline: ArgSlot > MaxInlineeArgoutCount\tInlinee: %s (%s)\tArgSlotNum: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (%s)\n"),
            inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer) , callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum(),
            Js::InlineeCallInfo::MaxInlineeArgoutCount, inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));

        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
        return instrNext;
    }

    uint inlineeCount = 0;
    const FunctionJITTimeInfo * tmpInlineeJitTimeData = inlineeJitTimeData;
    while(tmpInlineeJitTimeData)
    {LOGMEIN("Inline.cpp] 656\n");
        inlineeCount++;
        tmpInlineeJitTimeData = tmpInlineeJitTimeData->GetNext();
    }

    // Inlinee count too small (<2) or too large (>4)
    if (inlineeCount < 2 || inlineeCount > Js::DynamicProfileInfo::maxPolymorphicInliningSize)
    {LOGMEIN("Inline.cpp] 663\n");
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Skip Inline: Inlinee count either too small or too large: InlineeCount %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));

        TryResetObjTypeSpecFldInfoOn(methodValueOpnd);
        TryDisableRuntimePolymorphicCacheOn(methodValueOpnd);
        return instrNext;
    }

    *pIsInlined = true;

    IR::Instr* tmpInstr = callInstr->m_prev;
    while (tmpInstr->m_opcode != Js::OpCode::StartCall)
    {LOGMEIN("Inline.cpp] 678\n");
        if ((tmpInstr->m_opcode != Js::OpCode::ArgOut_A) && (tmpInstr->m_opcode != Js::OpCode::Ld_A))
        {LOGMEIN("Inline.cpp] 680\n");
            POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: ArgOuts may have side effects Inlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
            return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
        }
        tmpInstr = tmpInstr->m_prev;
    }

    StackSym* methodValueSym = callInstr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym();
    if (!methodValueSym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 691\n");
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    IR::Instr* ldMethodFldInstr = methodValueSym->GetInstrDef();
    if (!(ldMethodFldInstr->GetSrc1()->IsSymOpnd() && ldMethodFldInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd()))
    {LOGMEIN("Inline.cpp] 697\n");
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: Did not find property sym operand for the method load Inlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    IR::PropertySymOpnd* methodPropertyOpnd = ldMethodFldInstr->GetSrc1()->AsPropertySymOpnd();
    if (!methodPropertyOpnd->HasObjTypeSpecFldInfo())
    {LOGMEIN("Inline.cpp] 706\n");
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: no ObjTypeSpecFldInfo to get Fixed Methods from Inlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    if (!methodPropertyOpnd->HasFixedValue())
    {LOGMEIN("Inline.cpp] 714\n");
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: ObjTypeSpecFldInfo doesn't have Fixed Methods for one or some of the inlinees Inlinee: %s (%s):\tCaller: %s (%s)\n"),

                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    uint16 cachedFixedInlineeCount = methodPropertyOpnd->GetFixedFieldCount();
    if (cachedFixedInlineeCount < 2)
    {LOGMEIN("Inline.cpp] 724\n");
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: fixed function count too less %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    cachedFixedInlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    JITTimeFixedField* fixedFunctionInfoArray = methodPropertyOpnd->GetFixedFieldInfoArray();

    // It might so be the case that two objects of different types call the same function (body), for e.g., if they share the prototype on which the function is defined.
    uint uniqueFixedFunctionCount = HandleDifferentTypesSameFunction(fixedFunctionInfoArray, cachedFixedInlineeCount);

    if (uniqueFixedFunctionCount != inlineeCount)
    {LOGMEIN("Inline.cpp] 738\n");
        // inlineeCount obtained from the inlineeJitTimeData is more accurate than cached number of fixed methods for inlinees.
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: cached fixed function count (%d) doesn't match inlinee count (%d); (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    uniqueFixedFunctionCount, inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
        return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
    }

    Assert(cachedFixedInlineeCount <= Js::DynamicProfileInfo::maxPolymorphicInliningSize);
    const FunctionJITTimeInfo* inlineesDataArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = {};

    FillInlineesDataArrayUsingFixedMethods(inlineeJitTimeData, inlineesDataArray, Js::DynamicProfileInfo::maxPolymorphicInliningSize, fixedFunctionInfoArray, cachedFixedInlineeCount);

    for (uint i = 0; i < cachedFixedInlineeCount; i++)
    {LOGMEIN("Inline.cpp] 753\n");
        if(!inlineesDataArray[i] || !inlineesDataArray[i]->HasBody())
        {LOGMEIN("Inline.cpp] 755\n");
            POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: One of the inlinees doesn't have the corresponding object/prototype's type cached\tCaller: %s (%s)\n"),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
            return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
        }
#if DBG
        if(inlineesDataArray[i]->GetBody() && inlineesDataArray[i]->GetFunctionInfoAddr() != methodPropertyOpnd->GetFixedFunction(i)->GetFuncInfoAddr())
        {
            AssertMsg(false, "inlineesDataArray and fixedfunctionInfoArray should be aligned with each other at this point");
        }
#endif
        while (fixedFunctionInfoArray[i].NextHasSameFixedField())
        {LOGMEIN("Inline.cpp] 767\n");
            i++;
        }
    }

    bool safeThis = true; // Eliminate CheckThis for inlining.
    for (uint i = 0; i < cachedFixedInlineeCount; i++)
    {LOGMEIN("Inline.cpp] 774\n");
        if (!methodPropertyOpnd->GetFieldValue(i))
        {LOGMEIN("Inline.cpp] 776\n");
            POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: no fixed method for one of the inlinees; Inlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineesDataArray[i]->GetBody()->GetDisplayName(), inlineesDataArray[i]->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
            return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
        }
        if (i == 0)
        {
            // Do all the general, non-function-object-specific checks just once.
            if (!TryOptimizeCallInstrWithFixedMethod(callInstr, inlineesDataArray[i], true, false, false, true /*isInlined*/, safeThis, true /*dontOptimizeJustCheck*/, i))
            {LOGMEIN("Inline.cpp] 786\n");
                POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: can't optimize using Fixed Methods %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
                return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth, true);
            }
        }
        else
        {
            if (methodPropertyOpnd->GetFixedFunction(i) &&
                methodPropertyOpnd->GetFixedFunction(i)->GetFuncInfoAddr() != inlineesDataArray[i]->GetFunctionInfoAddr())
            {LOGMEIN("Inline.cpp] 798\n");
                POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Skip Inline: can't optimize using Fixed Methods %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
                return InlinePolymorphicFunction(callInstr, inlinerData, symCallerThis, profileId, pIsInlined, recursiveInlineDepth,  true);
            }
        }
        Js::TypeId typeId = methodPropertyOpnd->GetTypeId(i);
        if(!(typeId > Js::TypeIds_LastJavascriptPrimitiveType && typeId <= Js::TypeIds_LastTrueJavascriptObjectType))
        {LOGMEIN("Inline.cpp] 808\n");
            // Don't eliminate CheckThis if it cannot be done for any one of the inlinees
            safeThis = false;
        }
        while (fixedFunctionInfoArray[i].NextHasSameFixedField())
        {LOGMEIN("Inline.cpp] 813\n");
            i++;
        }
    }

    Assert(methodPropertyOpnd->IsPoly());

    // emit property guard check for the method load, and load type
    IR::RegOpnd *typeOpnd = IR::RegOpnd::New(TyVar, callInstr->m_func);
    IR::Instr* propertyGuardCheckInstr = IR::Instr::New(Js::OpCode::CheckPropertyGuardAndLoadType, typeOpnd, ldMethodFldInstr->GetSrc1(), callInstr->m_func);
    ldMethodFldInstr->InsertBefore(propertyGuardCheckInstr);
    propertyGuardCheckInstr->SetByteCodeOffset(ldMethodFldInstr);
    propertyGuardCheckInstr = propertyGuardCheckInstr->ConvertToBailOutInstr(ldMethodFldInstr, IR::BailOutFailedFixedFieldCheck);

    POLYMORPHIC_INLINE_TESTTRACE(_u("------------------------------------------------\n"));
    for (uint i = 0; i < cachedFixedInlineeCount; i++)
    {LOGMEIN("Inline.cpp] 829\n");
        JITTimeFunctionBody *inlineeFunctionBody = inlineesDataArray[i]->GetBody();
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic; Using Fixed Methods): Start inlining: \tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeFunctionBody->GetDisplayName(), inlineesDataArray[i]->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));

        while (fixedFunctionInfoArray[i].NextHasSameFixedField())
        {LOGMEIN("Inline.cpp] 836\n");
            i++;
        }
    }
    POLYMORPHIC_INLINE_TESTTRACE(_u("------------------------------------------------\n"));

    IR::RegOpnd * returnValueOpnd;
    if (callInstr->GetDst())
    {LOGMEIN("Inline.cpp] 844\n");
        returnValueOpnd = callInstr->UnlinkDst()->AsRegOpnd();
    }
    else
    {
        returnValueOpnd = nullptr;
    }

    callInstr->MoveArgs(/*generateByteCodeCapture*/ true);

    callInstr->m_opcode = Js::OpCode::CallIFixed;

    // iterate over inlineesDataArray to emit each inlinee
    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, false);
    IR::Instr* dispatchStartLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, false);
    callInstr->InsertBefore(dispatchStartLabel);
    for(uint i=0; i < cachedFixedInlineeCount; i++)
    {LOGMEIN("Inline.cpp] 861\n");
        IR::LabelInstr* inlineeStartLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func);
        callInstr->InsertBefore(inlineeStartLabel);

        IR::AddrOpnd * constMethodValueOpnd = IR::AddrOpnd::New(methodPropertyOpnd->GetFieldValue(i), IR::AddrOpndKind::AddrOpndKindDynamicVar, callInstr->m_func);
        constMethodValueOpnd->m_isFunction = true;
        constMethodValueOpnd->m_metadata = &methodPropertyOpnd->GetFixedFieldInfoArray()[i];

        InsertOneInlinee(callInstr, returnValueOpnd, constMethodValueOpnd, inlineesDataArray[i], inlineesDataArray[i]->GetRuntimeInfo(), doneLabel, symCallerThis, safeThis, recursiveInlineDepth);
        while (fixedFunctionInfoArray[i].NextHasSameFixedField())
        {LOGMEIN("Inline.cpp] 871\n");
            dispatchStartLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::BrAddr_A, inlineeStartLabel, typeOpnd, IR::AddrOpnd::New(methodPropertyOpnd->GetType(i)->GetAddr(),
                IR::AddrOpndKindDynamicType, dispatchStartLabel->m_func), dispatchStartLabel->m_func));
            this->topFunc->PinTypeRef(methodPropertyOpnd->GetType(i).t); // Keep the types alive as the types may not be equivalent and, hence, won't be kept alive by EquivalentTypeCache
            i++;
        }

        dispatchStartLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::BrAddr_A, inlineeStartLabel,
            typeOpnd, IR::AddrOpnd::New(methodPropertyOpnd->GetType(i)->GetAddr(), IR::AddrOpndKindDynamicType, dispatchStartLabel->m_func), dispatchStartLabel->m_func));
        this->topFunc->PinTypeRef(methodPropertyOpnd->GetType(i).t); // Keep the types alive as the types may not be equivalent and, hence, won't be kept alive by EquivalentTypeCache
    }

    ldMethodFldInstr->Unlink();
    ldMethodFldInstr->m_opcode = Js::OpCode::LdMethodFldPolyInlineMiss;
    Assert(cachedFixedInlineeCount > 0);
    CompletePolymorphicInlining(callInstr, returnValueOpnd, doneLabel, dispatchStartLabel, ldMethodFldInstr, IR::BailOutOnFailedPolymorphicInlineTypeCheck);

    this->topFunc->SetHasInlinee();
    InsertStatementBoundary(instrNext);
    InsertStatementBoundary(ldMethodFldInstr);

    return instrNext;
}

void Inline::CloneCallSequence(IR::Instr* callInstr, IR::Instr* clonedCallInstr)
{LOGMEIN("Inline.cpp] 896\n");
    IR::Instr* previousArg = nullptr;
    IR::Instr* previousClonedArg = clonedCallInstr;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr){
        IR::Instr* cloneArg = IR::Instr::New(argInstr->m_opcode,
            IR::SymOpnd::New(callInstr->m_func->m_symTable->GetArgSlotSym(argInstr->GetDst()->GetStackSym()->GetArgSlotNum()), 0, TyMachPtr, callInstr->m_func),
            argInstr->GetSrc1(), callInstr->m_func);
        cloneArg->SetByteCodeOffset(callInstr);
        cloneArg->GetDst()->GetStackSym()->m_isArgCaptured = true;
        previousClonedArg->SetSrc2(cloneArg->GetDst());
        previousClonedArg->InsertBefore(cloneArg);
        previousArg = argInstr;
        previousClonedArg = cloneArg;
        return false;
    });
    IR::Instr* startCall = previousArg->GetSrc2()->GetStackSym()->GetInstrDef();
    previousClonedArg->SetSrc2(startCall->GetDst());

}

IR::Instr *
Inline::InlinePolymorphicFunction(IR::Instr *callInstr, const FunctionJITTimeInfo * inlinerData, const StackSym *symCallerThis, const Js::ProfileId profileId, bool* pIsInlined, uint recursiveInlineDepth, bool triedUsingFixedMethods)
{LOGMEIN("Inline.cpp] 918\n");
    IR::Instr* instrNext = callInstr->m_next;
    *pIsInlined = false;


    if (triedUsingFixedMethods)
    {LOGMEIN("Inline.cpp] 924\n");
        if (callInstr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym()->IsSingleDef())
        {LOGMEIN("Inline.cpp] 926\n");
            IR::Instr* ldMethodFldInstr = callInstr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef();
            if (ldMethodFldInstr->GetSrc1()->IsSymOpnd() && ldMethodFldInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
            {LOGMEIN("Inline.cpp] 929\n");
                TryResetObjTypeSpecFldInfoOn(ldMethodFldInstr->GetSrc1()->AsPropertySymOpnd());
                TryDisableRuntimePolymorphicCacheOn(ldMethodFldInstr->GetSrc1()->AsPropertySymOpnd());
            }
        }
    }

    const FunctionJITTimeInfo * inlineeJitTimeData = inlinerData->GetInlinee(profileId);
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    if (!triedUsingFixedMethods) // We would have done the following two checks when we tried to inline using fixed methods
    {LOGMEIN("Inline.cpp] 943\n");
        if(!inlineeJitTimeData->GetNext())
        {LOGMEIN("Inline.cpp] 945\n");
            POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Skip Inline: Missing JitTime data \tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                     inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));

            //There are no multiple codegen jit-time data allocated for this call site, not sure how is this possible, abort
            return instrNext;
        }

        if (callInstr->GetSrc2() &&
            callInstr->GetSrc2()->IsSymOpnd() &&
            callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
        {LOGMEIN("Inline.cpp] 957\n");
            // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo. Although
            // InliningDecider already checks for this, the check is against profile data that may not be accurate since profile
            // data matching does not take into account some types of changes to source code. Need to check this again with current
            // information.
            POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Skip Inline: ArgSlot > MaxInlineeArgoutCount\tInlinee: %s (%s)\tArgSlotNum: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (%s)\n"),
                inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer) , callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum(),
                Js::InlineeCallInfo::MaxInlineeArgoutCount, inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));

            return instrNext;
        }
    }

    const FunctionJITTimeInfo * inlineesDataArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = {};

    uint inlineeCount = FillInlineesDataArray(inlineeJitTimeData, inlineesDataArray, Js::DynamicProfileInfo::maxPolymorphicInliningSize);
    if (inlineeCount < 2 || inlineeCount > Js::DynamicProfileInfo::maxPolymorphicInliningSize)
    {LOGMEIN("Inline.cpp] 974\n");
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Skip Inline: Inlinee count either too small or too large %d (Max: %d)\tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeCount, Js::DynamicProfileInfo::maxPolymorphicInliningSize,
                    inlineeJitTimeData->GetBody()->GetDisplayName(), inlineeJitTimeData->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));

        return instrNext;
    }

    // Begin inlining.
    POLYMORPHIC_INLINE_TESTTRACE(_u("------------------------------------------------\n"));
    for (uint i = 0; i < inlineeCount; i++)
    {LOGMEIN("Inline.cpp] 986\n");
        __analysis_assert(inlineesDataArray[i] != nullptr);
        JITTimeFunctionBody *inlineeFunctionBody = inlineesDataArray[i]->GetBody();
        POLYMORPHIC_INLINE_TESTTRACE(_u("INLINING (Polymorphic): Start inlining: \tInlinee: %s (%s):\tCaller: %s (%s)\n"),
                    inlineeFunctionBody->GetDisplayName(), inlineesDataArray[i]->GetDebugNumberSet(debugStringBuffer),
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2));
    }
    POLYMORPHIC_INLINE_TESTTRACE(_u("------------------------------------------------\n"));

    *pIsInlined = true;

    // This function is recursive, so when jitting in the foreground, probe the stack
    if (!this->topFunc->IsBackgroundJIT())
    {LOGMEIN("Inline.cpp] 999\n");
        PROBE_STACK(this->topFunc->GetScriptContext(), Js::Constants::MinStackDefault);
    }

    IR::RegOpnd * returnValueOpnd;
    Js::RegSlot returnRegSlot;
    if (callInstr->GetDst())
    {LOGMEIN("Inline.cpp] 1006\n");
        returnValueOpnd = callInstr->UnlinkDst()->AsRegOpnd();
        returnRegSlot = returnValueOpnd->m_sym->GetByteCodeRegSlot();
    }
    else
    {
        returnValueOpnd = nullptr;
        returnRegSlot = Js::Constants::NoRegister;
    }

    Assert(inlineeCount >= 2);

    // Shared bailout point for all the guard check bailouts.
    InsertJsFunctionCheck(callInstr, callInstr, IR::BailOutOnPolymorphicInlineFunction);

    callInstr->MoveArgs(/*generateByteCodeCapture*/ true);

    IR::LabelInstr * doneLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, false);
    IR::Instr* dispatchStartLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, false);
    callInstr->InsertBefore(dispatchStartLabel);
    for (uint i = 0; i < inlineeCount; i++)
    {LOGMEIN("Inline.cpp] 1027\n");
        IR::LabelInstr* inlineeStartLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func);
        callInstr->InsertBefore(inlineeStartLabel);
        InsertOneInlinee(callInstr, returnValueOpnd, callInstr->GetSrc1(), inlineesDataArray[i], inlineesDataArray[i]->GetRuntimeInfo(), doneLabel, symCallerThis, /*fixedFunctionSafeThis*/ false, recursiveInlineDepth);

        IR::RegOpnd* functionObject = callInstr->GetSrc1()->AsRegOpnd();
        dispatchStartLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::BrAddr_A, inlineeStartLabel,
            IR::IndirOpnd::New(functionObject, Js::JavascriptFunction::GetOffsetOfFunctionInfo(), TyMachPtr, dispatchStartLabel->m_func),
            IR::AddrOpnd::New(inlineesDataArray[i]->GetFunctionInfoAddr(), IR::AddrOpndKindDynamicFunctionBody, dispatchStartLabel->m_func), dispatchStartLabel->m_func));
    }

    CompletePolymorphicInlining(callInstr, returnValueOpnd, doneLabel, dispatchStartLabel, /*ldMethodFldInstr*/nullptr, IR::BailOutOnPolymorphicInlineFunction);

    this->topFunc->SetHasInlinee();
    InsertStatementBoundary(instrNext);

    return instrNext;

}

void Inline::CompletePolymorphicInlining(IR::Instr* callInstr, IR::RegOpnd* returnValueOpnd, IR::LabelInstr* doneLabel, IR::Instr* dispatchStartLabel, IR::Instr* ldMethodFldInstr, IR::BailOutKind bailoutKind)
{LOGMEIN("Inline.cpp] 1048\n");
    // Label $bailout:
    // LdMethodFldPolyInlineMiss
    // BailOnNotPolymorphicInlinee $callOutBytecodeOffset - BailOutOnFailedPolymorphicInlineTypeCheck
    // ByteCoudeUses
    // BytecodeArgoutUses
    // returnValueOpnd = EndCallForPolymorphicInlinee actualsCount
    IR::LabelInstr* bailOutLabel = IR::LabelInstr::New(Js::OpCode::Label, callInstr->m_func, /*helperLabel*/ true);
    callInstr->InsertBefore(bailOutLabel);
    dispatchStartLabel->InsertBefore(IR::BranchInstr::New(Js::OpCode::Br, bailOutLabel, callInstr->m_func));

    // Only fixed function inlining requires a ldMethodFldInstr
    if (ldMethodFldInstr)
    {LOGMEIN("Inline.cpp] 1061\n");
        callInstr->InsertBefore(ldMethodFldInstr);
    }

    callInstr->InsertBefore(IR::BailOutInstr::New(Js::OpCode::BailOnNotPolymorphicInlinee, bailoutKind, callInstr, callInstr->m_func));

    uint actualsCount = 0;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        IR::Instr* bytecodeArgOutUse = IR::Instr::New(Js::OpCode::BytecodeArgOutUse, callInstr->m_func);
        bytecodeArgOutUse->SetByteCodeOffset(callInstr);
        bytecodeArgOutUse->SetSrc1(argInstr->GetSrc1());
        callInstr->InsertBefore(bytecodeArgOutUse);
        actualsCount++;
        // Remove the original args
        argInstr->Remove();
        return false;
    });
    IR::ByteCodeUsesInstr* bytecodeUses = IR::ByteCodeUsesInstr::New(callInstr);
    bytecodeUses->Set(callInstr->GetSrc1());
    callInstr->InsertBefore(bytecodeUses);

    IR::Instr* endCallInstr = IR::Instr::New(Js::OpCode::EndCallForPolymorphicInlinee, callInstr->m_func);
    endCallInstr->SetSrc1(IR::IntConstOpnd::New(actualsCount + Js::Constants::InlineeMetaArgCount, TyInt32, callInstr->m_func, /*dontEncode*/ true));
    if (returnValueOpnd)
    {LOGMEIN("Inline.cpp] 1085\n");
        StackSym* returnValueSym = returnValueOpnd->m_sym->AsStackSym();
        IR::Opnd* dstOpnd = IR::RegOpnd::New(returnValueSym, returnValueSym->GetType(), callInstr->m_func);
        dstOpnd->SetValueType(returnValueOpnd->GetValueType());
        endCallInstr->SetDst(dstOpnd);
    }
    callInstr->InsertBefore(endCallInstr);
    callInstr->InsertBefore(doneLabel);
    callInstr->Remove(); // We don't need callInstr anymore.
}

//
// Inlines a function if it is a polymorphic inlining candidate.
// otherwise introduces a call to it.
// The IR for the args & calls is cloned to do this
//
void Inline::InsertOneInlinee(IR::Instr* callInstr, IR::RegOpnd* returnValueOpnd, IR::Opnd* methodOpnd,
    const FunctionJITTimeInfo * inlineeJITData, const FunctionJITRuntimeInfo * inlineeRuntimeData, IR::LabelInstr* doneLabel, const StackSym* symCallerThis, bool fixedFunctionSafeThis, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 1103\n");
    bool isInlined = inlineeJITData->IsInlined();

    IR::Instr* currentCallInstr;
    if (isInlined)
    {LOGMEIN("Inline.cpp] 1108\n");
        currentCallInstr = IR::Instr::New(Js::OpCode::InlineeStart, IR::RegOpnd::New(TyVar, callInstr->m_func), methodOpnd, callInstr->m_func);
    }
    else
    {
        currentCallInstr = IR::Instr::New(callInstr->m_opcode, callInstr->m_func);
        currentCallInstr->SetSrc1(methodOpnd);
        if (returnValueOpnd)
        {LOGMEIN("Inline.cpp] 1116\n");
            currentCallInstr->SetDst(returnValueOpnd);
        }
    }
    currentCallInstr->SetIsCloned(true);
    callInstr->InsertBefore(currentCallInstr);
    this->CloneCallSequence(callInstr, currentCallInstr);

    if (isInlined)
    {LOGMEIN("Inline.cpp] 1125\n");
        JITTimeFunctionBody *funcBody = inlineeJITData->GetBody();
        Func *inlinee = BuildInlinee(funcBody, inlineeJITData, inlineeRuntimeData, returnValueOpnd ? returnValueOpnd->m_sym->GetByteCodeRegSlot() : Js::Constants::NoRegister, callInstr, recursiveInlineDepth);

        IR::Instr *argOuts[Js::InlineeCallInfo::MaxInlineeArgoutCount];
#if DBG
        memset(argOuts, 0xFE, sizeof(argOuts));
#endif
        bool stackArgsArgOutExpanded = false;
        Js::ArgSlot actualCount = MapActuals(currentCallInstr, argOuts, Js::InlineeCallInfo::MaxInlineeArgoutCount, inlinee, (Js::ProfileId)callInstr->AsProfiledInstr()->u.profileId, &stackArgsArgOutExpanded);
        Assert(actualCount > 0);
        MapFormals(inlinee, argOuts, funcBody->GetInParamsCount(), actualCount, returnValueOpnd, currentCallInstr->GetSrc1(), symCallerThis, stackArgsArgOutExpanded, fixedFunctionSafeThis, argOuts);
        currentCallInstr->m_func = inlinee;

        // Put the meta arguments that the stack walker expects to find on the stack.
        // As all the argouts are shared among the inlinees, do this only once.
        SetupInlineeFrame(inlinee, currentCallInstr, actualCount, currentCallInstr->GetSrc1());

        IR::Instr* inlineeEndInstr = IR::Instr::New(Js::OpCode::InlineeEnd, inlinee);
        inlineeEndInstr->SetByteCodeOffset(inlinee->m_tailInstr->GetPrevRealInstr());
        inlineeEndInstr->SetSrc1(IR::IntConstOpnd::New(actualCount + Js::Constants::InlineeMetaArgCount, TyInt32, inlinee));
        inlineeEndInstr->SetSrc2(currentCallInstr->GetDst());
        inlinee->m_tailInstr->InsertBefore(inlineeEndInstr);

        // JMP to done at the end
        IR::Instr* doneInstr = IR::BranchInstr::New(Js::OpCode::Br, doneLabel, currentCallInstr->m_func);
        inlinee->m_tailInstr->InsertBefore(doneInstr);
        currentCallInstr->InsertRangeAfter(inlinee->m_headInstr->m_next, inlinee->m_tailInstr->m_prev);

        inlinee->m_headInstr->Free();
        inlinee->m_tailInstr->Free();
    }
    else
    {
        callInstr->InsertBefore(IR::BranchInstr::New(Js::OpCode::Br, doneLabel, callInstr->m_func));
    }
}

uint
Inline::HandleDifferentTypesSameFunction(__inout_ecount(cachedFixedInlineeCount) JITTimeFixedField* fixedFunctionInfoArray, uint16 cachedFixedInlineeCount)
{LOGMEIN("Inline.cpp] 1165\n");
    uint16 uniqueCount = cachedFixedInlineeCount;
    uint16 swapIndex;
    for (uint16 i = 0; i < cachedFixedInlineeCount; i++)
    {LOGMEIN("Inline.cpp] 1169\n");
        swapIndex = i+1;
        for (uint16 j = i+1; j < cachedFixedInlineeCount; j++)
        {LOGMEIN("Inline.cpp] 1172\n");
            if (fixedFunctionInfoArray[i].GetFieldValue() == fixedFunctionInfoArray[j].GetFieldValue())
            {LOGMEIN("Inline.cpp] 1174\n");
                JITTimeFixedField tmpInfo = fixedFunctionInfoArray[j];
                fixedFunctionInfoArray[j] = fixedFunctionInfoArray[swapIndex];
                fixedFunctionInfoArray[swapIndex] = tmpInfo;
                fixedFunctionInfoArray[swapIndex - 1].SetNextHasSameFixedField();
                swapIndex++;
                uniqueCount--;
            }
        }
        i = swapIndex-1;
    }
    return uniqueCount;
}

void
Inline::SetInlineeFrameStartSym(Func *inlinee, uint actualCount)
{LOGMEIN("Inline.cpp] 1190\n");
    StackSym    *stackSym = inlinee->m_symTable->GetArgSlotSym((Js::ArgSlot)actualCount + 1);
    stackSym->m_isInlinedArgSlot = true;
    this->topFunc->SetArgOffset(stackSym, (currentInlineeFrameSlot) * MachPtr);
    inlinee->SetInlineeFrameStartSym(stackSym);
}

Func *
Inline::BuildInlinee(JITTimeFunctionBody* funcBody, const FunctionJITTimeInfo * inlineeJITData, const FunctionJITRuntimeInfo * inlineeRuntimeData, Js::RegSlot returnRegSlot, IR::Instr *callInstr, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 1199\n");
    Assert(callInstr->IsProfiledInstr());
    Js::ProfileId callSiteId = static_cast<Js::ProfileId>(callInstr->AsProfiledInstr()->u.profileId);

    CodeGenWorkItemIDL * workItemData = JitAnewStruct(this->topFunc->m_alloc, CodeGenWorkItemIDL);

    workItemData->isJitInDebugMode = this->topFunc->IsJitInDebugMode();
    workItemData->type = JsFunctionType;
    workItemData->jitMode = static_cast<char>(this->topFunc->GetWorkItem()->GetJitMode());
    workItemData->nativeDataAddr = this->topFunc->GetWorkItem()->GetWorkItemData()->nativeDataAddr;
    workItemData->loopNumber = Js::LoopHeader::NoLoop;

    workItemData->jitData = (FunctionJITTimeDataIDL*)(inlineeJITData);
    JITTimeWorkItem * jitWorkItem = JitAnew(this->topFunc->m_alloc, JITTimeWorkItem, workItemData);

    JITTimePolymorphicInlineCacheInfo * entryPointPolymorphicInlineCacheInfo = this->topFunc->GetWorkItem()->GetInlineePolymorphicInlineCacheInfo(funcBody->GetAddr());
#if !FLOATVAR
    Func * inlinee = JitAnew(this->topFunc->m_alloc,
                            Func,
                            this->topFunc->m_alloc,
                            jitWorkItem,
                            this->topFunc->GetThreadContextInfo(),
                            this->topFunc->GetScriptContextInfo(),
                            this->topFunc->GetJITOutput()->GetOutputData(),
                            nullptr,
                            inlineeRuntimeData,
                            entryPointPolymorphicInlineCacheInfo,
                            this->topFunc->GetCodeGenAllocators(),
                            this->topFunc->GetNumberAllocator(),
                            this->topFunc->GetCodeGenProfiler(),
                            this->topFunc->IsBackgroundJIT(),
                            callInstr->m_func,
                            callInstr->m_next->GetByteCodeOffset(),
                            returnRegSlot,
                            false,
                            callSiteId,
                            false);
#else
        Func * inlinee = JitAnew(this->topFunc->m_alloc,
                            Func,
                            this->topFunc->m_alloc,
                            jitWorkItem,
                            this->topFunc->GetThreadContextInfo(),
                            this->topFunc->GetScriptContextInfo(),
                            this->topFunc->GetJITOutput()->GetOutputData(),
                            nullptr,
                            inlineeRuntimeData,
                            entryPointPolymorphicInlineCacheInfo,
                            this->topFunc->GetCodeGenAllocators(),
                            this->topFunc->GetCodeGenProfiler(),
                            this->topFunc->IsBackgroundJIT(),
                            callInstr->m_func,
                            callInstr->m_next->GetByteCodeOffset(),
                            returnRegSlot,
                            false,
                            callSiteId,
                            false);
#endif

    BuildIRForInlinee(inlinee, funcBody, callInstr, false, recursiveInlineDepth);
    return inlinee;
}

void
Inline::BuildIRForInlinee(Func *inlinee, JITTimeFunctionBody *funcBody, IR::Instr *callInstr, bool isApplyTarget, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 1264\n");
    // Update for..in max depth for the whole function
    this->topFunc->UpdateForInLoopMaxDepth(this->currentForInDepth + funcBody->GetForInLoopDepth());

    // Set for..in base depth of inlinee
    inlinee->m_forInLoopBaseDepth = this->currentForInDepth;

    Js::ArgSlot actualsCount = 0;
    IR::Instr *argOuts[Js::InlineeCallInfo::MaxInlineeArgoutCount];
#if DBG
    memset(argOuts, 0xFE, sizeof(argOuts));
#endif

    callInstr->IterateArgInstrs([&](IR::Instr* argInstr){
        StackSym *argSym = argInstr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
        argOuts[argSym->GetArgSlotNum() - 1] = argInstr;
        actualsCount++;
        return false;
    });

    inlinee->actualCount = actualsCount;

    inlinee->m_symTable = this->topFunc->m_symTable;

    inlinee->m_symTable->SetIDAdjustment();
    inlinee->m_symTable->IncreaseStartingID(funcBody->GetLocalsCount());

    BEGIN_CODEGEN_PHASE(this->topFunc, Js::IRBuilderPhase);

    IRBuilder irBuilder(inlinee);
    irBuilder.Build();

    END_CODEGEN_PHASE_NO_DUMP(this->topFunc, Js::IRBuilderPhase);

    inlinee->m_symTable->ClearIDAdjustment();

    Inline recursiveInliner(this->topFunc, this->inliningHeuristics, this->isInLoop, currentInlineeFrameSlot + Js::Constants::InlineeMetaArgCount + actualsCount, isApplyTarget);
    recursiveInliner.Optimize(inlinee, argOuts, actualsCount, inlinee->GetJITFunctionBody()->GetAddr() == callInstr->m_func->GetJITFunctionBody()->GetAddr() ? recursiveInlineDepth + 1 : 0);

#ifdef DBG
    Js::ArgSlot formalCount = funcBody->GetInParamsCount();

    if (formalCount > Js::InlineeCallInfo::MaxInlineeArgoutCount)
    {LOGMEIN("Inline.cpp] 1307\n");
        Fatal();
    }
#endif
}

bool
Inline::TryOptimizeCallInstrWithFixedMethod(IR::Instr *callInstr, const FunctionJITTimeInfo * inlineeInfo, bool isPolymorphic, bool isBuiltIn, bool isCtor, bool isInlined, bool &safeThis,
                                            bool dontOptimizeJustCheck, uint i /*i-th inlinee at a polymorphic call site*/)
{LOGMEIN("Inline.cpp] 1316\n");
    Assert(!callInstr->m_func->GetJITFunctionBody()->HasTry());

    if (PHASE_OFF(Js::FixedMethodsPhase, callInstr->m_func))
    {LOGMEIN("Inline.cpp] 1320\n");
        return false;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#define TRACE_FIXED_FIELDS 1
#endif

#if TRACE_FIXED_FIELDS
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    bool printFixedFieldsTrace =
        (
            PHASE_TRACE(Js::FixedMethodsPhase, callInstr->m_func) ||
            PHASE_TESTTRACE(Js::FixedMethodsPhase, callInstr->m_func) ||
            (isCtor && (
                PHASE_TRACE(Js::FixedNewObjPhase, callInstr->m_func) ||
                PHASE_TESTTRACE(Js::FixedNewObjPhase, callInstr->m_func)))
        ) && !dontOptimizeJustCheck && !JITManager::GetJITManager()->IsJITServer();

    if (printFixedFieldsTrace)
    {LOGMEIN("Inline.cpp] 1341\n");
        JITTimeFunctionBody * calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
        const char16* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");

        Output::Print(_u("FixedFields: function %s (%s): considering method (%s %s): polymorphic = %d, built-in = %d, ctor = %d, inlined = %d, functionInfo = %p.\n"),
            callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer), calleeName,
            calleeFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer2) : _u("(null)"),
            isPolymorphic, isBuiltIn, isCtor, isInlined, inlineeInfo ? inlineeInfo->GetFunctionInfoAddr() : 0);
        Output::Flush();
    }
#endif

    if (isPolymorphic && isInlined)
    {LOGMEIN("Inline.cpp] 1354\n");
        Assert(dontOptimizeJustCheck);
    }

    StackSym* methodValueSym = callInstr->GetSrc1()->AsRegOpnd()->m_sym->AsStackSym();
    if (!methodValueSym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 1360\n");
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {LOGMEIN("Inline.cpp] 1363\n");
            JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
            const char16* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");

            Output::Print(_u("FixedFields: function %s (%s): %s non-fixed method (%s %s), because callee is not single def.\n"),
                callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                inlineeInfo != nullptr ? _u("inlining") : _u("calling"), calleeName,
                calleeFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer2) : _u("(null)"));
            Output::Flush();
        }
#endif
        return false;
    }

    IR::Instr* ldMethodFldInstr = methodValueSym->GetInstrDef();
    while (ldMethodFldInstr->m_opcode == Js::OpCode::BytecodeArgOutCapture)
    {LOGMEIN("Inline.cpp] 1379\n");
        StackSym* sym = ldMethodFldInstr->GetSrc1()->GetStackSym();
        if (!sym->IsSingleDef())
        {LOGMEIN("Inline.cpp] 1382\n");
#if TRACE_FIXED_FIELDS
            if (printFixedFieldsTrace)
            {LOGMEIN("Inline.cpp] 1385\n");
                JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
                const char16* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");

                Output::Print(_u("FixedFields: function %s (%s): %s non-fixed method (%s %s), because callee is not single def.\n"),
                    callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                    inlineeInfo != nullptr ? _u("inlining") : _u("calling"), calleeName,
                    calleeFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer2) : _u("(null)"));
                Output::Flush();
            }
#endif
            return false;
        }
        ldMethodFldInstr = sym->GetInstrDef();
    }
    if (ldMethodFldInstr->m_opcode != Js::OpCode::ScopedLdMethodFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdRootMethodFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdMethodFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdRootFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdFld
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdFldForCallApplyTarget
        && ldMethodFldInstr->m_opcode != Js::OpCode::LdMethodFromFlags)
    {LOGMEIN("Inline.cpp] 1407\n");
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {LOGMEIN("Inline.cpp] 1410\n");
            JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
            const char16* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");

            Output::Print(_u("FixedFields: function %s (%s): %s non-fixed method (%s %s), because callee does not come from LdMethodFld.\n"),
                callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                inlineeInfo != nullptr ? _u("inlining") : _u("calling"), calleeName,
                calleeFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer2) : _u("(null)"));
            Output::Flush();
        }
#endif
        return false;
    }

    IR::PropertySymOpnd* methodPropertyOpnd = ldMethodFldInstr->GetSrc1()->AsPropertySymOpnd();

    if ((isCtor &&
            ((isInlined && PHASE_OFF(Js::FixedCtorInliningPhase, callInstr->m_func)) ||
            (!isInlined && PHASE_OFF(Js::FixedCtorCallsPhase, callInstr->m_func)) ||
            (methodPropertyOpnd->UsesAccessor()))) ||
        (!isCtor &&
            ((isBuiltIn &&
                ((isInlined && PHASE_OFF(Js::FixedBuiltInMethodInliningPhase, callInstr->m_func)) ||
                (!isInlined && PHASE_OFF(Js::FixedBuiltInMethodCallsPhase, callInstr->m_func)))) ||
            (!isBuiltIn &&
                ((isInlined && PHASE_OFF(Js::FixedScriptMethodInliningPhase, callInstr->m_func)) ||
                (!isInlined && !PHASE_ON(Js::FixedScriptMethodCallsPhase, callInstr->m_func))))))
       )
    {LOGMEIN("Inline.cpp] 1438\n");
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {LOGMEIN("Inline.cpp] 1441\n");
            JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
            const char16* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");

            Js::PropertyId methodPropertyId = callInstr->m_func->GetJITFunctionBody()->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callInstr->m_func->GetInProcThreadContext()->GetPropertyRecord(methodPropertyId);

            Output::Print(_u("FixedFields: function %s (#%u): %s non-fixed method %s (%s #%u) (cache id: %d), because %s fixed %s %s is disabled.\n"),
                callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                inlineeInfo != nullptr ? _u("inlining") : _u("calling"), methodPropertyRecord->GetBuffer(), calleeName,
                calleeFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer2) : _u("(null)"),
                methodPropertyOpnd->m_inlineCacheIndex, isInlined ? _u("inlining") : _u("calling"), isBuiltIn ? _u("built-in") : _u("script"),
                isCtor ? _u("ctors") : _u("methods"));
            Output::Flush();
        }
#endif
        return false;
    }

    if (!methodPropertyOpnd->IsObjTypeSpecCandidate() && !methodPropertyOpnd->IsRootObjectNonConfigurableFieldLoad())
    {LOGMEIN("Inline.cpp] 1461\n");
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {LOGMEIN("Inline.cpp] 1464\n");
            JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
            const char16* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");
            Js::PropertyId methodPropertyId = callInstr->m_func->GetJITFunctionBody()->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callInstr->m_func->GetInProcThreadContext()->GetPropertyRecord(methodPropertyId);

            Output::Print(_u("FixedFields: function %s (%s): %s non-fixed method %s (%s %s) (cache id: %d), because inline cache has no cached type.\n"),
                callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                inlineeInfo != nullptr ? _u("inlining") : _u("calling"), methodPropertyRecord->GetBuffer(), calleeName,
                calleeFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer2) : _u("(null)"),
                methodPropertyOpnd->m_inlineCacheIndex);
            Output::Flush();
        }
#endif
        return false;
    }

    JITTimeFixedField * fixedField = nullptr;
    if (!isPolymorphic)
    {LOGMEIN("Inline.cpp] 1483\n");
        fixedField = methodPropertyOpnd->HasFixedValue() ? methodPropertyOpnd->GetFixedFunction() : nullptr;
    }
    else if (isPolymorphic && isInlined)
    {LOGMEIN("Inline.cpp] 1487\n");
        fixedField = methodPropertyOpnd->HasFixedValue() ? methodPropertyOpnd->GetFixedFunction(i) : nullptr;
    }

    if (!fixedField)
    {LOGMEIN("Inline.cpp] 1492\n");
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {LOGMEIN("Inline.cpp] 1495\n");
            JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
            const char16* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");

            Js::PropertyId methodPropertyId = callInstr->m_func->GetJITFunctionBody()->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callInstr->m_func->GetInProcThreadContext()->GetPropertyRecord(methodPropertyId);

            Output::Print(_u("FixedFields: function %s (%s): %s non-fixed method %s (%s %s) (cache id: %d, layout: %s), because inline cache has no fixed function object.\n"),
                callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                inlineeInfo != nullptr ? _u("inlining") : _u("calling"), methodPropertyRecord->GetBuffer(), calleeName,
                calleeFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer2) : _u("(null)"),
                methodPropertyOpnd->m_inlineCacheIndex,
                methodPropertyOpnd->IsLoadedFromProto() ? _u("proto") : methodPropertyOpnd->UsesAccessor() ? _u("accessor") : _u("local"));
            Output::Flush();
        }
#endif
        return false;
    }

    // Certain built-ins that we decide not to inline will get a fast path emitted by the lowerer.
    // The lowering code cannot handle a call with a fixed function target, because it needs access to
    // the original property sym. Turn off fixed method calls for these cases.
    if (inlineeInfo == nullptr && Func::IsBuiltInInlinedInLowerer(callInstr->GetSrc1()))
    {LOGMEIN("Inline.cpp] 1518\n");
#if TRACE_FIXED_FIELDS
        if (printFixedFieldsTrace)
        {LOGMEIN("Inline.cpp] 1521\n");
            JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
            const char16* calleeName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");

            Js::PropertyId methodPropertyId = callInstr->m_func->GetJITFunctionBody()->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callInstr->m_func->GetInProcThreadContext()->GetPropertyRecord(methodPropertyId);

            Output::Print(_u("FixedFields: function %s (%s): %s non-fixed method %s (%s %s) (cache id: %d, layout: %s), because callee is a built-in with fast path in lowerer.\n"),
                callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                inlineeInfo != nullptr ? _u("inlining") : _u("calling"), methodPropertyRecord->GetBuffer(), calleeName,
                calleeFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer2) : _u("(null)"),
                methodPropertyOpnd->m_inlineCacheIndex,
                methodPropertyOpnd->IsLoadedFromProto() ? _u("proto") : methodPropertyOpnd->UsesAccessor() ? _u("accessor") : _u("local"));
            Output::Flush();
        }
#endif
        return false;
    }

    if (inlineeInfo != nullptr && fixedField->GetFuncInfoAddr() != inlineeInfo->GetFunctionInfoAddr())
    {LOGMEIN("Inline.cpp] 1541\n");
#if TRACE_FIXED_FIELDS && 0// TODO: OOP JIT, trace fixed fields
        if (printFixedFieldsTrace)
        {LOGMEIN("Inline.cpp] 1544\n");
            char16 debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Js::PropertyId methodPropertyId = callInstr->m_func->GetJITFunctionBody()->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callInstr->m_func->GetInProcThreadContext()->GetPropertyRecord(methodPropertyId);
            bool isProto = methodPropertyOpnd->IsLoadedFromProto();
            bool isAccessor = methodPropertyOpnd->UsesAccessor();
            Js::FunctionBody* fixedFunctionBody   = functionObject->GetFunctionInfo()->GetFunctionBody();
            const char16* fixedFunctionNumbers   = fixedFunctionBody ? fixedFunctionBody->GetDebugNumberSet(debugStringBuffer2) : _u("(null)");
            JITTimeFunctionBody* profileFunctionBody = inlineeInfo->GetBody();
            const char16* profileFunctionName    = profileFunctionBody != nullptr ? profileFunctionBody->GetDisplayName() : _u("<unknown>");
            const char16* profileFunctionNumbers = profileFunctionBody ? inlineeInfo->GetDebugNumberSet(debugStringBuffer3) : _u("(null)");

            if (PHASE_TRACE(Js::FixedMethodsPhase, callInstr->m_func))
            {LOGMEIN("Inline.cpp] 1557\n");
                intptr_t protoObject = isProto ? methodPropertyOpnd->GetProtoObject() : 0;
                Output::Print(_u("FixedFields: function %s (#%s): function body mismatch for inlinee: %s (%s) 0x%p->0x%p != %s (%s) 0x%p (cache id: %d, layout: %s, type: 0x%p, proto: 0x%p, proto type: 0x%p).\n"),
                    callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                    methodPropertyRecord->GetBuffer(), fixedFunctionNumbers, functionObject, functionObject->GetFunctionInfo(),
                    profileFunctionName, profileFunctionNumbers, inlineeInfo->GetFunctionInfoAddr(),
                    methodPropertyOpnd->m_inlineCacheIndex, isProto ? _u("proto") : isAccessor ? _u("accessor") : _u("local"),
                    methodPropertyOpnd->GetType(), protoObject, protoObject != nullptr ? protoObject->GetType() : nullptr);
            }
            if (PHASE_TESTTRACE(Js::FixedMethodsPhase, callInstr->m_func))
            {LOGMEIN("Inline.cpp] 1567\n");
                Output::Print(_u("FixedFields: function %s (%s): function body mismatch for inlinee: %s (%s) != %s (%s) (cache id: %d, layout: %s).\n"),
                    callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                    methodPropertyRecord->GetBuffer(), fixedFunctionNumbers, profileFunctionName, profileFunctionNumbers,
                    methodPropertyOpnd->m_inlineCacheIndex, isProto ? _u("proto") : isAccessor ? _u("accessor") : _u("local"));
            }
            Output::Flush();
        }
#endif
        // It appears that under certain bailout and re-JIT conditions we may end up with an updated
        // inline cache pointing to a new function object, while the call site profile info still
        // holds the old function body.  If the two don't match, let's fall back on the regular LdMethodFld.
        return false;
    }
    else
    {
#if TRACE_FIXED_FIELDS && 0// TODO: OOP JIT, trace fixed fields
        if (printFixedFieldsTrace)
        {LOGMEIN("Inline.cpp] 1585\n");
            JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
            Js::PropertyId methodPropertyId = callInstr->m_func->GetJITFunctionBody()->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
            Js::PropertyRecord const * const methodPropertyRecord = callInstr->m_func->GetInProcThreadContext()->GetPropertyRecord(methodPropertyId);
            const char16* fixedFunctionName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");
            Js::FunctionBody* fixedFunctionBody = functionObject->GetFunctionInfo()->GetFunctionBody();
            const char16* fixedFunctionNumbers = fixedFunctionBody ? fixedFunctionBody->GetDebugNumberSet(debugStringBuffer2) : _u("(null)");

            Output::Print(_u("FixedFields: function %s (%s): %s fixed method %s (%s %s) (cache id: %d, layout: %s).\n"),
                callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer),
                inlineeInfo != nullptr ? _u("inlining") : _u("calling"),
                methodPropertyRecord->GetBuffer(), fixedFunctionName, fixedFunctionNumbers,
                methodPropertyOpnd->m_inlineCacheIndex,
                methodPropertyOpnd->IsLoadedFromProto() ? _u("proto") : methodPropertyOpnd->UsesAccessor() ? _u("accessor") : _u("local"));
            Output::Flush();
        }
#endif
    }

#undef TRACE_FIXED_FIELDS

    if (dontOptimizeJustCheck)
    {LOGMEIN("Inline.cpp] 1607\n");
        return true;
    }

    // Change Ld[Root]MethodFld, LdMethodFromFlags to CheckFixedFld, which doesn't need a dst.
    if(ldMethodFldInstr->m_opcode == Js::OpCode::LdMethodFromFlags)
    {LOGMEIN("Inline.cpp] 1613\n");
        Assert(ldMethodFldInstr->HasBailOutInfo());
        ldMethodFldInstr->ClearBailOutInfo();
    }
    ldMethodFldInstr->m_opcode = Js::OpCode::CheckFixedFld;
    IR::Opnd * methodValueDstOpnd = ldMethodFldInstr->UnlinkDst();
    IR::Instr * chkMethodFldInstr = ldMethodFldInstr->ConvertToBailOutInstr(ldMethodFldInstr,
        !methodPropertyOpnd->HasEquivalentTypeSet() ? IR::BailOutFailedFixedFieldTypeCheck : IR::BailOutFailedEquivalentFixedFieldTypeCheck);
    chkMethodFldInstr->GetBailOutInfo()->polymorphicCacheIndex = methodPropertyOpnd->m_inlineCacheIndex;

    Assert(chkMethodFldInstr->GetSrc1()->IsSymOpnd());
    if (chkMethodFldInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
    {LOGMEIN("Inline.cpp] 1625\n");
        Assert(chkMethodFldInstr->m_opcode == Js::OpCode::CheckFixedFld);
        IR::PropertySymOpnd* chkMethodFldOpnd = chkMethodFldInstr->GetSrc1()->AsPropertySymOpnd();
        // For polymorphic field loads we only support fixed functions on prototypes. This helps keep the equivalence check helper simple.
        Assert(chkMethodFldOpnd->IsMono() || chkMethodFldOpnd->IsLoadedFromProto() || chkMethodFldOpnd->UsesAccessor());
        chkMethodFldOpnd->SetUsesFixedValue(true);
    }

    if (isCtor)
    {LOGMEIN("Inline.cpp] 1634\n");
        JITTimeConstructorCache* constructorCache = methodPropertyOpnd->GetCtorCache();
        if (constructorCache != nullptr && callInstr->IsProfiledInstr())
        {LOGMEIN("Inline.cpp] 1637\n");

#if ENABLE_DEBUG_CONFIG_OPTIONS && 0// TODO: OOP JIT, trace fixed fields
            if (PHASE_TRACE(Js::FixedNewObjPhase, callInstr->m_func) || PHASE_TESTTRACE(Js::FixedNewObjPhase, callInstr->m_func))
            {LOGMEIN("Inline.cpp] 1641\n");
                JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
                Js::PropertyId methodPropertyId = callInstr->m_func->GetJITFunctionBody()->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
                Js::PropertyRecord const * const methodPropertyRecord = callInstr->m_func->GetThreadContextInfo()->GetPropertyRecord(methodPropertyId);
                const char16* fixedFunctionName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");
                Js::FunctionBody* fixedFunctionBody = functionObject->GetFunctionInfo()->GetFunctionBody();
                const char16* fixedFunctionNumbers = fixedFunctionBody ? fixedFunctionBody->GetDebugNumberSet(debugStringBuffer2) : _u("(null)");

                Output::Print(_u("FixedNewObj: function %s (%s): fixed new object for %s with %s ctor %s (%s %s)%s\n"),
                    callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer), Js::OpCodeUtil::GetOpCodeName(callInstr->m_opcode),
                    inlineeInfo != nullptr ? _u("inlined") : _u("called"),
                    methodPropertyRecord->GetBuffer(), fixedFunctionName, fixedFunctionNumbers,
                    constructorCache->SkipNewScObject() ? _u(" skip default object") : _u(""));
                Output::Flush();
            }
#endif

            // The profile ID's hung from array ctor opcodes don't match up with normal profiled call sites.
            if (callInstr->m_opcode != Js::OpCode::NewScObjArray)
            {LOGMEIN("Inline.cpp] 1660\n");
                // Because we are storing flow sensitive info in the cache (guarded property operations),
                // we must make sure the same cache cannot be used multiple times in the flow.
                if (constructorCache->IsUsed())
                {LOGMEIN("Inline.cpp] 1664\n");
                    // It's okay to allocate a JitTimeConstructorCache from the func's allocator (rather than recycler),
                    // because we only use these during JIT. We use the underlying runtime cache as a guard that must
                    // live after JIT, and these are added to the EntryPointInfo during work item creation and thus kept alive.
                    constructorCache = constructorCache->Clone(this->topFunc->m_alloc);
                }
                Assert(!constructorCache->IsUsed());
                constructorCache->SetUsed(true);
                callInstr->m_func->SetConstructorCache(static_cast<Js::ProfileId>(callInstr->AsProfiledInstr()->u.profileId), constructorCache);
            }
        }
        else
        {
#if ENABLE_DEBUG_CONFIG_OPTIONS && 0// TODO: OOP JIT, trace fixed fields
            if (PHASE_TRACE(Js::FixedNewObjPhase, callInstr->m_func) || PHASE_TESTTRACE(Js::FixedNewObjPhase, callInstr->m_func))
            {LOGMEIN("Inline.cpp] 1679\n");
                JITTimeFunctionBody* calleeFunctionBody = inlineeInfo != nullptr && inlineeInfo->HasBody() ? inlineeInfo->GetBody() : nullptr;
                Js::PropertyId methodPropertyId = callInstr->m_func->GetJITFunctionBody()->GetPropertyIdFromCacheId(methodPropertyOpnd->m_inlineCacheIndex);
                Js::PropertyRecord const * const methodPropertyRecord = callInstr->m_func->GetThreadContextInfo()->GetPropertyRecord(methodPropertyId);
                const char16* fixedFunctionName = calleeFunctionBody != nullptr ? calleeFunctionBody->GetDisplayName() : _u("<unknown>");
                Js::FunctionBody* fixedFunctionBody = functionObject->GetFunctionInfo()->GetFunctionBody();
                const char16* fixedFunctionNumbers = fixedFunctionBody ? fixedFunctionBody->GetDebugNumberSet(debugStringBuffer2) : _u("(null)");

                Output::Print(_u("FixedNewObj: function %s (%s): non-fixed new object for %s with %s ctor %s (%s %s), because %s.\n"),
                    callInstr->m_func->GetJITFunctionBody()->GetDisplayName(), callInstr->m_func->GetDebugNumberSet(debugStringBuffer), Js::OpCodeUtil::GetOpCodeName(callInstr->m_opcode),
                    inlineeInfo != nullptr ? _u("inlined") : _u("called"),
                    methodPropertyRecord->GetBuffer(), fixedFunctionName, fixedFunctionNumbers,
                    constructorCache == nullptr ? _u("constructor cache hasn't been cloned") : _u("instruction isn't profiled"));
                Output::Flush();
            }
#endif
        }
    }

    // Insert a load instruction to place the constant address in methodOpnd (the Ld[Root]MethodFld's original dst).
    IR::AddrOpnd * constMethodValueOpnd = IR::AddrOpnd::New(fixedField->GetFieldValue(), IR::AddrOpndKind::AddrOpndKindDynamicVar, callInstr->m_func);
    constMethodValueOpnd->m_metadata = fixedField;
    constMethodValueOpnd->m_isFunction = true;
    IR::Instr * ldMethodValueInstr = IR::Instr::New(Js::OpCode::Ld_A, methodValueDstOpnd, constMethodValueOpnd, callInstr->m_func);
    StackSym* methodSym = methodValueDstOpnd->AsRegOpnd()->m_sym;
    if (methodSym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 1705\n");
        methodSym->SetIsConst();
    }
    methodValueDstOpnd->SetValueType(fixedField->GetValueType());
    chkMethodFldInstr->InsertAfter(ldMethodValueInstr);
    callInstr->ReplaceSrc1(constMethodValueOpnd);
    if (callInstr->m_opcode == Js::OpCode::CallI || callInstr->CallsAccessor(methodPropertyOpnd))
    {LOGMEIN("Inline.cpp] 1712\n");
        callInstr->m_opcode = Js::OpCode::CallIFixed;
    }
    else
    {
        // We patch later for constructor inlining.
        Assert(
            callInstr->m_opcode == Js::OpCode::NewScObject ||
            callInstr->m_opcode == Js::OpCode::NewScObjArray);
    }

    if (!isBuiltIn && isInlined)
    {LOGMEIN("Inline.cpp] 1724\n");
        // We eliminate CheckThis for fixed method inlining. Assert here that our assumption is true.
        Js::TypeId typeId = methodPropertyOpnd->IsRootObjectNonConfigurableField() ?
            Js::TypeIds_GlobalObject : methodPropertyOpnd->GetTypeId();
        if(typeId > Js::TypeIds_LastJavascriptPrimitiveType && typeId <= Js::TypeIds_LastTrueJavascriptObjectType)
        {LOGMEIN("Inline.cpp] 1729\n");
            // Eliminate CheckThis for inlining.
            safeThis = true;
        }
    }
    return true;
}

intptr_t
Inline::TryOptimizeInstrWithFixedDataProperty(IR::Instr *&instr)
{LOGMEIN("Inline.cpp] 1739\n");
    if (PHASE_OFF(Js::UseFixedDataPropsPhase, instr->m_func) ||
        PHASE_OFF(Js::UseFixedDataPropsInInlinerPhase, instr->m_func))
    {LOGMEIN("Inline.cpp] 1742\n");
        return 0;
    }
    if (!instr->IsProfiledInstr() ||
        !instr->GetSrc1()->IsSymOpnd() || !instr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
    {LOGMEIN("Inline.cpp] 1747\n");
        return 0;
    }
    if (!OpCodeAttr::CanLoadFixedFields(instr->m_opcode))
    {LOGMEIN("Inline.cpp] 1751\n");
        return 0;
    }
    return instr->TryOptimizeInstrWithFixedDataProperty(&instr, nullptr);
}

// Inline a built-in/math function call, such as Math.sin(x).
// Main idea on what happens with IR during different stages.
// 1) Copy args from ArgOuts into inline instr.
// 2) Change opcode: ArgOut_A -> ArgOut_A_InlineBuiltIn (aka BIA).
// 3) Notes:
//    - General logic is similar to inlining regular functions, except that:
//      - There are no inner instructions to inline.
//      - We don't need to support arguments object inside the inlinee - don't need inlinee meta frame, etc.
//    - ArgOuts are linked through src2->m_sym->m_instrDef.
//    - ArgOuts are not needed for the inlined call itself, but we can't remove them because they are needed for bailout.
//      We convert them to ArgOut_A_InlineBuiltIn.
// Example for Math.pow(x, y), x86 case.
// Original:
//     instrS: dstS = StartCall <N=count>, NULL -- N is actual number of parameters, including "this".
//     instr0: arg0 = ArgOut t, link(->instrS)  -- "this" arg
//     instr1: arg1 = ArgOut x, link(->instr0)  -- src1
//     instr2: arg2 = ArgOut y, link(->instr1)  -- src2
//     instr3: dstC = CallI fn, link(->instr2)  -- links to instr2, etc.
// After Inline:
//     instrS: dstS = StartCall <N=count>, NULL -- N is actual number of parameters, including "this".
//             tmpt = BytecodeArgOutCapture t                                     -- create assigns to temps to snapshot argout values in case they are modified later before the call
//             tmpx = BytecodeArgOutCapture x
//             tmpy = BytecodeArgOutCapture y
//     instr1: arg1 = ArgOut_InlineBuiltIn tmpx, link(->instr0)  -- src1
//     instr0: arg0 = ArgOut_InlineBuiltIn tmpt, link(->instrS)  -- "this" arg    -- Change ArgOut_a to ArgOut_A_InlineBuiltIn
//     instr2: arg2 = ArgOut_InlineBuiltIn tmpy, link(->instr1)  -- src2
//             NULL = InlineBuiltInStart fn, link(->instr2)
//             dstC = InlineMathPow, tmpx, tmpy       -- actual native math call.
//             NULL = InlineBuiltInEnd <N=count>, link(->instr2)
// After Globopt:
//     instrS: dstS = StartCall <N=count>, NULL -- N is actual number of parameters, including "this".
//             tmpt = BytecodeArgOutCapture t                                     -- create assigns to temps to snapshot argout values in case they are modified later before the call
//             tmpx = BytecodeArgOutCapture x
//             Bailout 1
//             tmpy = BytecodeArgOutCapture y
//             Bailout 2
//     instr1: arg1 = ArgOut_InlineBuiltIn tmpx, link(->instr0)  -- src1
//     instr0: arg0 = ArgOut_InlineBuiltIn tmpt, link(->instrS)  -- "this" arg    -- Change ArgOut_a to ArgOut_A_InlineBuiltIn
//     instr2: arg2 = ArgOut_InlineBuiltIn tmpy, link(->instr1)  -- src2
//                    ...
//             NULL = InlineBuiltInStart fn, link(->instr2) -- Note that InlineBuiltInStart is after last bailout.
//                                                             This is important so that fn used for bailout is after last bailout.
//             dstC = InlineMathPow, tmpx, tmpy       -- actual native math call.
//             NULL = InlineBuiltInEnd <N=count>, link(->instr2)
// After Lowerer:
//                    ...
//         s1(XMM0) = MOVSD tmpx
//         s2(XMM1) = MOVSD tmpy
//         s1(XMM0) = CALL pow                  -- actual native math call.
//             dstC = MOVSD s1(XMM0)

IR::Instr *
Inline::InlineBuiltInFunction(IR::Instr *callInstr, const FunctionJITTimeInfo * inlineeData, Js::OpCode inlineCallOpCode, const FunctionJITTimeInfo * inlinerData, const StackSym *symCallerThis, bool* pIsInlined, uint profileId, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 1810\n");
    Assert(callInstr);
    Assert(inlinerData);
    Assert(inlineCallOpCode != 0);

    // We may still decide not to inline.
    *pIsInlined = false;

    // Inlining is profile-based, so get the built-in function from profile rather than from the callInstr's opnd.
    Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInForFuncInfo(inlineeData->GetFunctionInfoAddr(), this->topFunc->GetThreadContextInfo());

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    if(inlineCallOpCode == Js::OpCode::InlineMathFloor || inlineCallOpCode == Js::OpCode::InlineMathCeil || inlineCallOpCode == Js::OpCode::InlineMathRound)
    {LOGMEIN("Inline.cpp] 1825\n");
#if defined(_M_IX86) || defined(_M_X64)
        if (!AutoSystemInfo::Data.SSE4_1Available())
        {LOGMEIN("Inline.cpp] 1828\n");
            INLINE_TESTTRACE(_u("INLINING: Skip Inline: SSE4.1 not available\tInlinee: %s (#%d)\tCaller: %s\n"), Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId, inlinerData->GetBody()->GetDisplayName());
            return callInstr->m_next;
        }
#endif
        if(callInstr->m_func->GetTopFunc()->HasProfileInfo() && callInstr->m_func->GetTopFunc()->GetReadOnlyProfileInfo()->IsFloorInliningDisabled())
        {LOGMEIN("Inline.cpp] 1834\n");
            INLINE_TESTTRACE(_u("INLINING: Skip Inline: Floor Inlining Disabled\tInlinee: %s (#%d)\tCaller: %s\n"), Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId, inlinerData->GetBody()->GetDisplayName());
            return callInstr->m_next;
        }
    }

    if (callInstr->GetSrc2() &&
        callInstr->GetSrc2()->IsSymOpnd() &&
        callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
    {LOGMEIN("Inline.cpp] 1843\n");
        // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo. Although
        // InliningDecider already checks for this, the check is against profile data that may not be accurate since profile
        // data matching does not take into account some types of changes to source code. Need to check this again with current
        // information.
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: ArgSlot > MaxInlineeArgoutCount\tInlinee: %s (#%d)\tArgSlotNum: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (#%d)\n"),
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId, callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum(),
            Js::InlineeCallInfo::MaxInlineeArgoutCount, inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    Js::BuiltInFlags builtInFlags = Js::JavascriptLibrary::GetFlagsForBuiltIn(builtInId);

    bool isAnyArgFloat = (builtInFlags & Js::BuiltInFlags::BIF_TypeSpecAllToFloat) != 0;
    if (isAnyArgFloat && !GlobOpt::DoFloatTypeSpec(this->topFunc))
    {LOGMEIN("Inline.cpp] 1858\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: float type spec is off\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    bool canDstBeFloat = (builtInFlags & Js::BuiltInFlags::BIF_TypeSpecDstToFloat) != 0;
    if (canDstBeFloat && !Js::JavascriptLibrary::CanFloatPreferenceFunc(builtInId) && inlineCallOpCode != Js::OpCode::InlineArrayPop)
    {LOGMEIN("Inline.cpp] 1867\n");
        // Note that for Math.abs that means that even though it can potentially be type-spec'd to int, we won't inline it.
        // Some built-in functions, such as atan2, are disabled for float-pref.
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: Cannot float-type-spec the inlinee\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId, // Get the _value (cause operator _E) to avoid using struct directly.
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    bool isAnyArgInt = (builtInFlags & (Js::BuiltInFlags::BIF_TypeSpecDstToInt | Js::BuiltInFlags::BIF_TypeSpecSrc1ToInt | Js::BuiltInFlags::BIF_TypeSpecSrc2ToInt)) != 0;
    if (isAnyArgInt && !GlobOpt::DoAggressiveIntTypeSpec(this->topFunc))
    {LOGMEIN("Inline.cpp] 1878\n");
        // Note that for Math.abs that means that even though it can potentially be type-spec'd to float, we won't inline it.
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: int type spec is off\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    if(inlineCallOpCode == Js::OpCode::InlineMathImul && !GlobOpt::DoLossyIntTypeSpec(topFunc))
    {LOGMEIN("Inline.cpp] 1887\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: lossy int type spec is off, it's required for Math.imul to do | 0 on src opnds\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    if(inlineCallOpCode == Js::OpCode::InlineMathClz && !GlobOpt::DoLossyIntTypeSpec(topFunc))
    {LOGMEIN("Inline.cpp] 1895\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: lossy int type spec is off, it's required for Math.clz32 to do | 0 on src opnds\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    if (inlineCallOpCode == Js::OpCode::InlineFunctionApply && (!callInstr->m_func->GetHasStackArgs() || this->topFunc->GetJITFunctionBody()->IsInlineApplyDisabled()))
    {LOGMEIN("Inline.cpp] 1903\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: stack args of inlining is off\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    // TODO: when adding support for other type spec args (array, string) do appropriate check as well.

    Assert(callInstr->GetSrc1());
    Assert(callInstr->GetSrc1()->IsRegOpnd());
    Assert(callInstr->GetSrc1()->AsRegOpnd()->m_sym);

    if (!(builtInFlags & Js::BuiltInFlags::BIF_IgnoreDst) && callInstr->GetDst() == nullptr && inlineCallOpCode != Js::OpCode::InlineArrayPop)
    {LOGMEIN("Inline.cpp] 1917\n");
        // Is seems that it's not worth optimizing odd cases where the result is unused.
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: inlinee's return value is not assigned to anything\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
            Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
        return callInstr->m_next;
    }

    // Number of arguments, not including "this".
    IntConstType requiredInlineCallArgCount = (IntConstType)Js::JavascriptLibrary::GetArgCForBuiltIn(builtInId);

    IR::Opnd* linkOpnd = callInstr->GetSrc2();
    Js::ArgSlot actualCount = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();

    // Check for missing actuals:
    // if number of passed params to built-in function is not what it needs, don't inline.
    bool usesThisArgument = (builtInFlags & Js::BuiltInFlags::BIF_UseSrc0) != 0;
    int inlineCallArgCount = (int)(usesThisArgument ? actualCount : actualCount - 1);
    Assert(inlineCallArgCount >= 0);

    if (linkOpnd->IsSymOpnd())
    {LOGMEIN("Inline.cpp] 1938\n");
        if((builtInFlags & Js::BuiltInFlags::BIF_VariableArgsNumber) != 0)
        {LOGMEIN("Inline.cpp] 1940\n");
            if(inlineCallArgCount > requiredInlineCallArgCount)
            {LOGMEIN("Inline.cpp] 1942\n");
                INLINE_TESTTRACE(_u("INLINING: Skip Inline: parameter count exceeds the maximum number of parameters allowed\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
                    Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
                    inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
                return callInstr->m_next;
            }
        }
        else if(inlineCallArgCount != requiredInlineCallArgCount)
        {LOGMEIN("Inline.cpp] 1950\n");
            INLINE_TESTTRACE(_u("INLINING: Skip Inline: parameter count doesn't match dynamic profile\tInlinee: %s (#%d)\tCaller: %s (%s)\n"),
                Js::JavascriptLibrary::GetNameForBuiltIn(builtInId), (int)builtInId,
                inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
            return callInstr->m_next;
        }
    }

    IR::Instr *inlineBuiltInEndInstr = nullptr;
    if (inlineCallOpCode ==  Js::OpCode::InlineFunctionApply)
    {LOGMEIN("Inline.cpp] 1960\n");
       inlineBuiltInEndInstr = InlineApply(callInstr, inlineeData, inlinerData, symCallerThis, pIsInlined, profileId, recursiveInlineDepth, inlineCallArgCount - (usesThisArgument ? 1 : 0));
       return inlineBuiltInEndInstr->m_next;
    }

    if (inlineCallOpCode ==  Js::OpCode::InlineFunctionCall)
    {LOGMEIN("Inline.cpp] 1966\n");
       inlineBuiltInEndInstr = InlineCall(callInstr, inlineeData, inlinerData, symCallerThis, pIsInlined, profileId, recursiveInlineDepth);
       return inlineBuiltInEndInstr->m_next;
    }


#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    TraceInlining(inlinerData, Js::JavascriptLibrary::GetNameForBuiltIn(builtInId),
        nullptr, 0, this->topFunc->GetWorkItem()->GetJITTimeInfo(), 0, nullptr, profileId, callInstr->m_func->GetTopFunc()->IsLoopBody(), builtInId);
#endif

    // From now on we are committed to inlining.
    *pIsInlined = true;

    // Save off the call target operand (function object) so we can extend its lifetime as needed, even if
    // the call instruction gets transformed to CallIFixed.
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool originalCallTargetOpndIsJITOpt = callInstr->GetSrc1()->GetIsJITOptimizedReg();

    // We are committed to inlining, optimize the call instruction for fixed fields now and don't attempt it later.
    bool safeThis = false;
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, inlineeData, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/))
    {LOGMEIN("Inline.cpp] 1988\n");
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
        Assert(callInstr->GetFixedFunction()->GetFuncInfoAddr() == inlineeData->GetFunctionInfoAddr());
    }
    else
    {
        // FunctionObject check for built-ins
        IR::BailOutInstr * bailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotBuiltIn, IR::BailOutOnInlineFunction, callInstr, callInstr->m_func);
        InsertFunctionObjectCheck(callInstr, callInstr, bailOutInstr, inlineeData);
    }

    // To push function object for cases when we have to make calls to helper method to assist in inlining
    if(inlineCallOpCode == Js::OpCode::CallDirect)
    {LOGMEIN("Inline.cpp] 2001\n");
        IR::Instr* argoutInstr;
        StackSym *dstSym = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
        argoutInstr = IR::Instr::New(Js::OpCode::ArgOut_A_InlineSpecialized, IR::SymOpnd::New(dstSym, 0, TyMachPtr, callInstr->m_func), callInstr->UnlinkSrc1(), callInstr->UnlinkSrc2(), callInstr->m_func);
        argoutInstr->SetByteCodeOffset(callInstr);
        callInstr->GetInsertBeforeByteCodeUsesInstr()->InsertBefore(argoutInstr);

        Js::BuiltinFunction builtInFunctionId = Js::JavascriptLibrary::GetBuiltInForFuncInfo(inlineeData->GetFunctionInfoAddr(), this->topFunc->GetThreadContextInfo());


        callInstr->m_opcode = inlineCallOpCode;
        SetupInlineInstrForCallDirect(builtInFunctionId, callInstr, argoutInstr);

        // Generate ByteCodeArgOutCaptures and move the ArgOut_A/ArgOut_A_Inline close to the call instruction
        callInstr->MoveArgs(/*generateByteCodeCapture*/ true);

        WrapArgsOutWithCoerse(builtInFunctionId, callInstr);

        inlineBuiltInEndInstr = callInstr;
    }
    else
    {
        inlineBuiltInEndInstr = InsertInlineeBuiltInStartEndTags(callInstr, actualCount);

        // InlineArrayPop - TrackCalls Need to be done at InlineArrayPop and not at the InlineBuiltInEnd
        // Hence we use a new opcode, to detect that it is an InlineArrayPop and we don't track the call during End of inlineBuiltInCall sequence
        if(inlineCallOpCode == Js::OpCode::InlineArrayPop)
        {LOGMEIN("Inline.cpp] 2028\n");
            inlineBuiltInEndInstr->m_opcode = Js::OpCode::InlineNonTrackingBuiltInEnd;
        }
    }

    // Insert a byteCodeUsesInstr to make sure the function object's lifetime is extended beyond the last bailout point
    // at which we may need to call the inlinee again in the interpreter.
    {
        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr);
        useCallTargetInstr->SetRemovedOpndSymbol(originalCallTargetOpndIsJITOpt, originalCallTargetStackSym->m_id);
        callInstr->InsertBefore(useCallTargetInstr);
    }

    if(Js::JavascriptLibrary::IsTypeSpecRequired(builtInFlags)
// SIMD_JS
        || IsSimd128Opcode(inlineCallOpCode)
//
        )
    {LOGMEIN("Inline.cpp] 2046\n");
        // Emit byteCodeUses for function object
        IR::Instr * inlineBuiltInStartInstr = inlineBuiltInEndInstr;
        while(inlineBuiltInStartInstr->m_opcode != Js::OpCode::InlineBuiltInStart)
        {LOGMEIN("Inline.cpp] 2050\n");
            inlineBuiltInStartInstr = inlineBuiltInStartInstr->m_prev;
        }

        IR::Opnd * tmpDst = nullptr;
        IR::Opnd * callInstrDst = callInstr->GetDst();

        if(callInstrDst && inlineCallOpCode != Js::OpCode::InlineArrayPop)
        {LOGMEIN("Inline.cpp] 2058\n");
            StackSym * tmpSym = StackSym::New(callInstr->GetDst()->GetType(), callInstr->m_func);
            tmpDst = IR::RegOpnd::New(tmpSym, tmpSym->GetType(), callInstr->m_func);

            callInstrDst = callInstr->UnlinkDst();
            callInstr->SetDst(tmpDst);
        }
        else
        {
            AssertMsg(inlineCallOpCode == Js::OpCode::InlineArrayPush || inlineCallOpCode == Js::OpCode::InlineArrayPop || Js::IsSimd128Opcode(inlineCallOpCode),
                "Currently Dst can be null only for InlineArrayPush/InlineArrayPop");
        }

        // Insert a byteCodeUsesInstr to make sure the function object's lifetime is extended beyond the last bailout point
        // at which we may need to call the inlinee again in the interpreter.
        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr->GetPrevRealInstrOrLabel());
        useCallTargetInstr->SetRemovedOpndSymbol(originalCallTargetOpndIsJITOpt, originalCallTargetStackSym->m_id);

        if(inlineCallOpCode == Js::OpCode::InlineArrayPop)
        {LOGMEIN("Inline.cpp] 2077\n");
           callInstr->InsertBefore(useCallTargetInstr);
        }
        else
        {
            inlineBuiltInEndInstr->InsertBefore(useCallTargetInstr);
        }

        if(tmpDst)
        {LOGMEIN("Inline.cpp] 2086\n");
            IR::Instr * ldInstr = IR::Instr::New(Js::OpCode::Ld_A, callInstrDst, tmpDst, callInstr->m_func);
            inlineBuiltInEndInstr->InsertBefore(ldInstr);
        }

        // Set srcs of the callInstr, and process ArgOuts.
        callInstr->UnlinkSrc1();
        callInstr->UnlinkSrc2();
        callInstr->m_opcode = inlineCallOpCode;

        int argIndex = inlineCallArgCount;    // We'll use it to fill call instr srcs from upper to lower.


        IR::ByteCodeUsesInstr * byteCodeUsesInstr = IR::ByteCodeUsesInstr::New(callInstr);
        IR::Instr *argInsertInstr = inlineBuiltInStartInstr;

#ifdef ENABLE_SIMDJS
        // SIMD_JS
        IR::Instr *eaInsertInstr = callInstr;
        IR::Opnd *eaLinkOpnd = nullptr;
        ThreadContext::SimdFuncSignature simdFuncSignature;
        if (IsSimd128Opcode(callInstr->m_opcode))
        {LOGMEIN("Inline.cpp] 2108\n");
            callInstr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(callInstr->m_opcode, simdFuncSignature);
            Assert(simdFuncSignature.valid);
            // if we have decided to inline, then actual arg count == signature arg count == required arg count from inlinee list (LibraryFunction.h)
            Assert(simdFuncSignature.argCount == (uint)inlineCallArgCount);
            Assert(simdFuncSignature.argCount == (uint)requiredInlineCallArgCount);
        }
#endif

        inlineBuiltInEndInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
            StackSym *linkSym = linkOpnd->GetStackSym();
            linkSym->m_isInlinedArgSlot = true;
            linkSym->m_allocated = true;

            // We are going to replace the use on the call (below), insert byte code use if necessary
            if (OpCodeAttr::BailOutRec(inlineCallOpCode) || Js::IsSimd128Opcode(inlineCallOpCode))
            {LOGMEIN("Inline.cpp] 2124\n");
                StackSym * sym = argInstr->GetSrc1()->GetStackSym();
                if (!sym->m_isSingleDef || !sym->m_instrDef->GetSrc1() || !sym->m_instrDef->GetSrc1()->IsConstOpnd())
                {LOGMEIN("Inline.cpp] 2127\n");
                    if (!sym->IsFromByteCodeConstantTable())
                    {LOGMEIN("Inline.cpp] 2129\n");
                        byteCodeUsesInstr->Set(argInstr->GetSrc1());
                    }
                }
            }

            // Convert the arg out to built in arg out, and get the src of the arg out
            IR::Opnd * argOpnd = ConvertToInlineBuiltInArgOut(argInstr);

#ifdef ENABLE_SIMDJS
            // SIMD_JS
            if (inlineCallArgCount > 2 && argIndex != 0 /* don't include 'this' */)
            {LOGMEIN("Inline.cpp] 2141\n");
                Assert(IsSimd128Opcode(callInstr->m_opcode));
                // Insert ExtendedArgs

                IR::Instr *eaInstr;

                // inliner sets the dst type of the ExtendedArg to the expected arg type for the operation. The globOpt uses this info to know the type-spec target for each ExtendedArg.
                eaInstr = IR::Instr::New(Js::OpCode::ExtendArg_A, callInstr->m_func);
                eaInstr->SetByteCodeOffset(callInstr);
                if (argIndex == inlineCallArgCount)
                {LOGMEIN("Inline.cpp] 2151\n");
                    // fix callInstr
                    eaLinkOpnd = IR::RegOpnd::New(TyVar, callInstr->m_func);
                    eaLinkOpnd->GetStackSym()->m_isInlinedArgSlot = true;
                    eaLinkOpnd->GetStackSym()->m_allocated = true;

                    Assert(callInstr->GetSrc1() == nullptr && callInstr->GetSrc2() == nullptr);
                    callInstr->SetSrc1(eaLinkOpnd);
                }
                Assert(eaLinkOpnd);
                eaInstr->SetDst(eaLinkOpnd);
                eaInstr->SetSrc1(argInstr->GetSrc1());

                // insert link opnd, except for first ExtendedArg
                if (argIndex > 1)
                {LOGMEIN("Inline.cpp] 2166\n");
                    eaInstr->SetSrc2(IR::RegOpnd::New(TyVar, callInstr->m_func));
                    eaLinkOpnd = eaInstr->GetSrc2();
                    eaLinkOpnd->GetStackSym()->m_isInlinedArgSlot = true;
                    eaLinkOpnd->GetStackSym()->m_allocated = true;
                }

                eaInstr->GetDst()->SetValueType(simdFuncSignature.args[argIndex - 1]);

                eaInsertInstr->InsertBefore(eaInstr);
                eaInsertInstr = eaInstr;
            }
            else
#endif
            {
                // Use parameter to the inline call to tempDst.
                if (argIndex == 2)
                {LOGMEIN("Inline.cpp] 2183\n");
                    callInstr->SetSrc2(argOpnd);
                    // Prevent inserting ByteCodeUses instr during globopt, as we already track the src in ArgOut.
                    callInstr->GetSrc2()->SetIsJITOptimizedReg(true);
                }
                else if (argIndex == 1)
                {LOGMEIN("Inline.cpp] 2189\n");
                    callInstr->SetSrc1(argOpnd);
                    // Prevent inserting ByteCodeUses instr during globopt, as we already track the src in ArgOut.
                    callInstr->GetSrc1()->SetIsJITOptimizedReg(true);
                }
            }


            argIndex--;

            linkOpnd = argInstr->GetSrc2();

            // Move the arguments next to the call.
            argInstr->Move(argInsertInstr);
            argInsertInstr = argInstr;
            return false;
        });

#ifdef ENABLE_SIMDJS
        //SIMD_JS
        Simd128FixLoadStoreInstr(builtInId, callInstr);
#endif

        if(inlineCallOpCode == Js::OpCode::InlineMathImul || inlineCallOpCode == Js::OpCode::InlineMathClz)
        {LOGMEIN("Inline.cpp] 2213\n");
            // Convert:
            //     s1 = InlineMathImul s2, s3
            // Into:
            //     s4 = Or_A s2, 0
            //     s5 = Or_A s3, 0
            //     s1 = InlineMathImul s4, s5

            Func *const func = callInstr->m_func;
            IR::AddrOpnd *const zeroOpnd = IR::AddrOpnd::NewFromNumber(0, func, true);

            IR::RegOpnd *const s4 = IR::RegOpnd::New(TyVar, func);
            s4->SetIsJITOptimizedReg(true);
            IR::Instr *orInstr = IR::Instr::New(Js::OpCode::Or_A, s4, callInstr->UnlinkSrc1(), zeroOpnd, func);
            orInstr->SetByteCodeOffset(callInstr);
            callInstr->InsertBefore(orInstr);
            callInstr->SetSrc1(s4);
            if (inlineCallOpCode == Js::OpCode::InlineMathImul)
            {LOGMEIN("Inline.cpp] 2231\n");
                if (callInstr->GetSrc2()->IsEqual(callInstr->GetSrc1()))
                {LOGMEIN("Inline.cpp] 2233\n");
                    callInstr->ReplaceSrc2(s4);
                }
                else
                {
                    IR::RegOpnd *const s5 = IR::RegOpnd::New(TyVar, func);
                    s5->SetIsJITOptimizedReg(true);
                    orInstr = IR::Instr::New(Js::OpCode::Or_A, s5, callInstr->UnlinkSrc2(), zeroOpnd, func);
                    orInstr->SetByteCodeOffset(callInstr);
                    callInstr->InsertBefore(orInstr);
                    callInstr->SetSrc2(s5);
                }
            }
        }

        if(OpCodeAttr::BailOutRec(inlineCallOpCode))
        {LOGMEIN("Inline.cpp] 2249\n");
            inlineBuiltInEndInstr->InsertBefore(byteCodeUsesInstr);
        }

        Assert(linkOpnd->AsRegOpnd()->m_sym->GetInstrDef()->m_opcode == Js::OpCode::StartCall);
        Assert(linkOpnd->AsRegOpnd()->m_sym->GetInstrDef()->GetArgOutCount(/*getInterpreterArgOutCount*/ false) == actualCount);

        // Mark the StartCall's dst as an inlined arg slot as well so we know this is an inlined start call
        // and not adjust the stack height on x86
        linkOpnd->AsRegOpnd()->m_sym->m_isInlinedArgSlot = true;

        if(OpCodeAttr::BailOutRec(inlineCallOpCode))
        {LOGMEIN("Inline.cpp] 2261\n");
            callInstr = callInstr->ConvertToBailOutInstr(callInstr, IR::BailOutOnFloor);
        }
    }
    return inlineBuiltInEndInstr->m_next;
}

IR::Instr* Inline::InsertInlineeBuiltInStartEndTags(IR::Instr* callInstr, uint actualCount, IR::Instr** builtinStartInstr)
{LOGMEIN("Inline.cpp] 2269\n");
    IR::Instr* inlineBuiltInStartInstr = IR::Instr::New(Js::OpCode::InlineBuiltInStart, callInstr->m_func);
    inlineBuiltInStartInstr->SetSrc1(callInstr->GetSrc1());
    inlineBuiltInStartInstr->SetSrc2(callInstr->GetSrc2());
    inlineBuiltInStartInstr->SetByteCodeOffset(callInstr);
    callInstr->InsertBefore(inlineBuiltInStartInstr);
    if (builtinStartInstr)
    {LOGMEIN("Inline.cpp] 2276\n");
        *builtinStartInstr = inlineBuiltInStartInstr;
    }

    IR::Instr* inlineBuiltInEndInstr = IR::Instr::New(Js::OpCode::InlineBuiltInEnd, callInstr->m_func);
    inlineBuiltInEndInstr->SetSrc1(IR::IntConstOpnd::New(actualCount, TyInt32, callInstr->m_func));
    inlineBuiltInEndInstr->SetSrc2(callInstr->GetSrc2());
    inlineBuiltInEndInstr->SetByteCodeOffset(callInstr->GetNextRealInstrOrLabel());
    callInstr->InsertAfter(inlineBuiltInEndInstr);
    return inlineBuiltInEndInstr;
}

IR::Instr* Inline::GetDefInstr(IR::Opnd* linkOpnd)
{LOGMEIN("Inline.cpp] 2289\n");
    StackSym *linkSym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();
    Assert(linkSym->m_isSingleDef);
    Assert(linkSym->IsArgSlotSym());

    return linkSym->m_instrDef;
}

IR::Instr* Inline::InlineApply(IR::Instr *callInstr, const FunctionJITTimeInfo *applyData, const FunctionJITTimeInfo * inlinerData, const StackSym *symCallerThis, bool* pIsInlined, uint callSiteId, uint recursiveInlineDepth, uint argsCount)
{LOGMEIN("Inline.cpp] 2298\n");
    // We may still decide not to inline.
    *pIsInlined = false;

    Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInForFuncInfo(applyData->GetFunctionInfoAddr(), this->topFunc->GetThreadContextInfo());
    const FunctionJITTimeInfo * inlineeData = nullptr;

    IR::Instr* arrayArgInstr = nullptr;
    IR::Opnd *arrayArgOpnd = nullptr;
    if (argsCount == 2) // apply was called with 2 arguments, most common case
    {LOGMEIN("Inline.cpp] 2308\n");
        IR::SymOpnd* linkOpnd = callInstr->GetSrc2()->AsSymOpnd();
        StackSym *arrayArgsym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();
        Assert(arrayArgsym->m_isSingleDef);
        Assert(arrayArgsym->IsArgSlotSym());

        arrayArgInstr = arrayArgsym->m_instrDef;
        arrayArgOpnd = arrayArgInstr->GetSrc1();
    }

    // if isArrayOpndArgumentsObject == false, the array opnd can still be the arguments object; we just can't say that for sure
    bool isArrayOpndArgumentsObject = arrayArgOpnd && arrayArgOpnd->IsArgumentsObject();

    IR::Instr * returnInstr = nullptr;
    if (!PHASE_OFF(Js::InlineApplyTargetPhase, this->topFunc))
    {
        if (InlineApplyScriptTarget(callInstr, inlinerData, &inlineeData, applyData, symCallerThis, &returnInstr, recursiveInlineDepth, isArrayOpndArgumentsObject, argsCount))
        {LOGMEIN("Inline.cpp] 2325\n");
            *pIsInlined = true;
            Assert(returnInstr);
            return returnInstr;
        }
    }

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    if (argsCount == 1) // apply called with just 1 argument, the 'this' object.
    {LOGMEIN("Inline.cpp] 2337\n");
        if (PHASE_OFF1(Js::InlineApplyWithoutArrayArgPhase))
        {LOGMEIN("Inline.cpp] 2339\n");
            *pIsInlined = false;
            return callInstr;
        }

        *pIsInlined = true;

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        TraceInlining(inlinerData, Js::JavascriptLibrary::GetNameForBuiltIn(builtInId),
            nullptr, 0, this->topFunc->GetWorkItem()->GetJITTimeInfo(), 0, nullptr, callSiteId, callInstr->m_func->GetTopFunc()->IsLoopBody(), builtInId);
#endif

        return InlineApplyWithoutArrayArgument(callInstr, applyData, inlineeData);
    }

    if (!isArrayOpndArgumentsObject)
    {LOGMEIN("Inline.cpp] 2355\n");
        if (inlineeData && inlineeData->GetBody() == nullptr)
        {LOGMEIN("Inline.cpp] 2357\n");
            *pIsInlined = true;

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
            TraceInlining(inlinerData, Js::JavascriptLibrary::GetNameForBuiltIn(builtInId),
                nullptr, 0, this->topFunc->GetWorkItem()->GetJITTimeInfo(), 0, nullptr, callSiteId, callInstr->m_func->GetTopFunc()->IsLoopBody(), builtInId);
#endif

            // TODO: OOP JIT enable assert (readprocessmemory?)
            //Assert((inlineeData->GetFunctionInfo()->GetAttributes() & Js::FunctionInfo::Attributes::BuiltInInlinableAsLdFldInlinee) != 0);
            return InlineApplyWithArray(callInstr, applyData, Js::JavascriptLibrary::GetBuiltInForFuncInfo(inlineeData->GetFunctionInfoAddr(), this->topFunc->GetThreadContextInfo()));
        }
        else
        {
            INLINE_TESTTRACE(_u("INLINING: Skip Inline: Supporting inlining func.apply(this, array) or func.apply(this, arguments) with formals in the parent function only when func is a built-in inlinable as apply target \tCaller: %s (%s)\n"),
                inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer));
            return callInstr;
        }
    }

    *pIsInlined = true;

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    TraceInlining(inlinerData, Js::JavascriptLibrary::GetNameForBuiltIn(builtInId),
        nullptr, 0, this->topFunc->GetWorkItem()->GetJITTimeInfo(), 0, nullptr, callSiteId, callInstr->m_func->GetTopFunc()->IsLoopBody(), builtInId);
#endif

    return InlineApplyWithArgumentsObject(callInstr, arrayArgInstr, applyData);
}

IR::Instr * Inline::InlineApplyWithArgumentsObject(IR::Instr * callInstr, IR::Instr * argsObjectArgInstr, const FunctionJITTimeInfo * funcInfo)
{LOGMEIN("Inline.cpp] 2388\n");
    IR::Instr* ldHeapArguments = argsObjectArgInstr->GetSrc1()->GetStackSym()->GetInstrDef();
    argsObjectArgInstr->ReplaceSrc1(ldHeapArguments->GetDst());

    IR::Instr * implicitThisArgOut = nullptr;
    IR::Instr * explicitThisArgOut = nullptr;
    IR::Instr * argumentsObjArgOut = nullptr;
    uint argOutCount = 0;
    this->GetArgInstrsForCallAndApply(callInstr, &implicitThisArgOut, &explicitThisArgOut, &argumentsObjArgOut, argOutCount);

    //      BailOnNotEqual  s4.var                  ---------------New additional BAILOUT if not stack args or actuals exceed 16 at runtime.
    //      Bailout: #004e (BailOutOnInlineFunction)
    //      linkOpnd      Argout_FromStackArgs s4.var
    //      linkOpnd1     ArgOut_A_Dynamic  s3.var, linkOpnd
    //                    CallI_Dynamic     s6.var,  linkOpnd1

    IR::Instr* bailOutOnNotStackArgs;
    IR::Instr* bailOutOnNotStackArgsInsertionPoint = implicitThisArgOut;

    // Save off the call target operand (function object) so we can extend its lifetime as needed, even if
    // the call instruction gets transformed to CallIFixed.
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool originalCallTargetOpndIsJITOpt = callInstr->GetSrc1()->GetIsJITOptimizedReg();

    // If we optimized the call instruction for a fixed function we will have bailed out earlier if the function
    // wasn't what we expected or was not a function at all.  However, we must still check and bail out on heap arguments.
    bool safeThis = false;
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, funcInfo, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/))
    {LOGMEIN("Inline.cpp] 2416\n");
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
        bailOutOnNotStackArgs = IR::BailOutInstr::New(Js::OpCode::BailOnNotStackArgs, IR::BailOutOnInlineFunction, callInstr, callInstr->m_func);
    }
    else
    {
        IR::Instr *primaryBailoutInstr = PrepareInsertionPoint(callInstr, funcInfo, implicitThisArgOut);
        bailOutOnNotStackArgs = IR::BailOutInstr::New(Js::OpCode::BailOnNotStackArgs, IR::BailOutOnInlineFunction, primaryBailoutInstr->GetBailOutInfo(), callInstr->m_func);
        bailOutOnNotStackArgsInsertionPoint = primaryBailoutInstr;
    }

    // set src1 to avoid CSE on BailOnNotStackArgs for different arguments object
    bailOutOnNotStackArgs->SetSrc1(ldHeapArguments->GetDst()->Copy(this->topFunc));
    bailOutOnNotStackArgsInsertionPoint->InsertBefore(bailOutOnNotStackArgs);

    // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after
    // the bailout on non-stack arguments.
    if (callInstr->m_opcode == Js::OpCode::CallIFixed)
    {LOGMEIN("Inline.cpp] 2434\n");
        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr);
        useCallTargetInstr->SetRemovedOpndSymbol(originalCallTargetOpndIsJITOpt, originalCallTargetStackSym->m_id);
        callInstr->InsertBefore(useCallTargetInstr);
    }

    // Optimize .init.apply(this, arguments);
    IR::Instr* builtInStartInstr;
    InsertInlineeBuiltInStartEndTags(callInstr, 3, &builtInStartInstr); //3 args (implicit this + explicit this + arguments = 3)

    // Move argouts close to call. Globopt expects this for arguments object tracking.
    IR::Instr* argInsertInstr = builtInStartInstr;
    builtInStartInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        argInstr->Move(argInsertInstr);
        argInsertInstr = argInstr;
        return false;
    });

    IR::Instr *startCall = IR::Instr::New(Js::OpCode::StartCall, callInstr->m_func);
    startCall->SetDst(IR::RegOpnd::New(TyVar, callInstr->m_func));
    startCall->SetSrc1(IR::IntConstOpnd::New(2, TyInt32, callInstr->m_func)); //2 args (this pointer & ArgOut_A_From_StackArgs for this direct call to init

    callInstr->InsertBefore(startCall);

    StackSym *symDst = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(2));
    IR::SymOpnd* linkOpnd1 = IR::SymOpnd::New(symDst, 0, TyMachPtr, callInstr->m_func);

    symDst = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
    IR::Opnd *linkOpnd2 = IR::SymOpnd::New(symDst, 0, TyMachPtr, callInstr->m_func);

    // This keeps the stack args alive for bailout to recover
    IR::Instr* argout = IR::Instr::New(Js::OpCode::ArgOut_A_FromStackArgs, linkOpnd1, ldHeapArguments->GetDst(), startCall->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(argout);

    callInstr->ReplaceSrc1(implicitThisArgOut->GetSrc1());
    callInstr->ReplaceSrc2(linkOpnd2);
    callInstr->m_opcode = Js::OpCode::CallIDynamic;

    argout = IR::Instr::New(Js::OpCode::ArgOut_A_Dynamic, linkOpnd2, explicitThisArgOut->GetSrc1(), linkOpnd1, callInstr->m_func); // push explicit this as this pointer
    callInstr->InsertBefore(argout);
    return callInstr;
}

/*
This method will only do CallDirect style inlining of built-in targets. No script function inlining.
*/
IR::Instr * Inline::InlineApplyWithArray(IR::Instr * callInstr, const FunctionJITTimeInfo * funcInfo, Js::BuiltinFunction builtInId)
{LOGMEIN("Inline.cpp] 2481\n");
    IR::Instr * implicitThisArgOut = nullptr;
    IR::Instr * explicitThisArgOut = nullptr;
    IR::Instr * arrayArgOut = nullptr;
    uint argOutCount = 0;
    this->GetArgInstrsForCallAndApply(callInstr, &implicitThisArgOut, &explicitThisArgOut, &arrayArgOut, argOutCount);

    TryFixedMethodAndPrepareInsertionPoint(callInstr, funcInfo, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/);

    IR::Instr* builtInEndInstr = InsertInlineeBuiltInStartEndTags(callInstr, 3); // 3 args (implicit this + explicit this + array = 3)
    builtInEndInstr->m_opcode = Js::OpCode::InlineNonTrackingBuiltInEnd; // We will call EndTrackCall when we see CallDirect for reasons explained in GlobOpt::TrackCalls

    IR::Instr * startCall = IR::Instr::New(Js::OpCode::StartCall,
                                           IR::RegOpnd::New(TyVar, callInstr->m_func),
                                           IR::IntConstOpnd::New(2, TyInt32, callInstr->m_func),
                                           callInstr->m_func);
    callInstr->InsertBefore(startCall);

    IR::Opnd * linkOpnd;
    StackSym * sym;

    sym = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
    linkOpnd = IR::SymOpnd::New(sym, 0, TyMachPtr, callInstr->m_func);
    IR::Instr * argOut = IR::Instr::New(Js::OpCode::ArgOut_A, linkOpnd, explicitThisArgOut->GetSrc1(), startCall->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(argOut);

    sym = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(2));
    linkOpnd = IR::SymOpnd::New(sym, 0, TyMachPtr, callInstr->m_func);
    argOut = IR::Instr::New(Js::OpCode::ArgOut_A, linkOpnd, arrayArgOut->GetSrc1(), argOut->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(argOut);

    linkOpnd = IR::SymOpnd::New(callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1)), 0, TyMachPtr, callInstr->m_func);
    argOut = IR::Instr::New(Js::OpCode::ArgOut_A_InlineSpecialized, linkOpnd, implicitThisArgOut->GetSrc1(), argOut->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(argOut);

    IR::HelperCallOpnd * helperCallOpnd = nullptr;
    switch (builtInId)
    {LOGMEIN("Inline.cpp] 2518\n");
    case Js::BuiltinFunction::Math_Max:
        helperCallOpnd = IR::HelperCallOpnd::New(IR::HelperOp_MaxInAnArray, callInstr->m_func);
        break;

    case Js::BuiltinFunction::Math_Min:
        helperCallOpnd = IR::HelperCallOpnd::New(IR::HelperOp_MinInAnArray, callInstr->m_func);
        break;

    default:
        Assert(false);
        __assume(UNREACHED);
    }
    callInstr->m_opcode = Js::OpCode::CallDirect;
    callInstr->ReplaceSrc1(helperCallOpnd);
    callInstr->ReplaceSrc2(argOut->GetDst());

    return callInstr;
}

IR::Instr * Inline::InlineApplyWithoutArrayArgument(IR::Instr *callInstr, const FunctionJITTimeInfo * applyInfo, const FunctionJITTimeInfo * applyTargetInfo)
{LOGMEIN("Inline.cpp] 2539\n");
    IR::Instr * implicitThisArgOut = nullptr;
    IR::Instr * explicitThisArgOut = nullptr;
    IR::Instr * dummyInstr = nullptr;
    uint argOutCount = 0;
    this->GetArgInstrsForCallAndApply(callInstr, &implicitThisArgOut, &explicitThisArgOut, &dummyInstr, argOutCount);

    TryFixedMethodAndPrepareInsertionPoint(callInstr, applyInfo, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/);

    InsertInlineeBuiltInStartEndTags(callInstr, 2); // 2 args (implicit this + explicit this)

    IR::Instr * startCall = IR::Instr::New(Js::OpCode::StartCall,
        IR::RegOpnd::New(TyVar, callInstr->m_func),
        IR::IntConstOpnd::New(1, TyInt32, callInstr->m_func),
        callInstr->m_func);
    callInstr->InsertBefore(startCall);

    StackSym* symDst = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
    IR::SymOpnd* linkOpnd = IR::SymOpnd::New(symDst, 0, TyMachPtr, callInstr->m_func);
    IR::Instr* thisArgOut = IR::Instr::New(Js::OpCode::ArgOut_A, linkOpnd, explicitThisArgOut->GetSrc1(), startCall->GetDst(), callInstr->m_func);
    callInstr->InsertBefore(thisArgOut);

    callInstr->ReplaceSrc1(implicitThisArgOut->GetSrc1());
    callInstr->ReplaceSrc2(linkOpnd);
    callInstr->m_opcode = Js::OpCode::CallI;

    StackSym* callTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool callTargetOpndIsJITOpt = callInstr->GetSrc1()->GetIsJITOptimizedReg();
    while (callTargetStackSym->IsSingleDef() && callTargetStackSym->GetInstrDef()->m_opcode == Js::OpCode::BytecodeArgOutCapture)
    {LOGMEIN("Inline.cpp] 2568\n");
        callTargetOpndIsJITOpt = callTargetStackSym->GetInstrDef()->GetSrc1()->GetIsJITOptimizedReg();
        callTargetStackSym = callTargetStackSym->GetInstrDef()->GetSrc1()->GetStackSym();
    }

    if (!callTargetStackSym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 2574\n");
        return callInstr;
    }

    bool safeThis = false;
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, applyTargetInfo, false /*isPolymorphic*/, false /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/))
    {LOGMEIN("Inline.cpp] 2580\n");
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
        IR::ByteCodeUsesInstr* bytecodeUses = IR::ByteCodeUsesInstr::New(callInstr);
        bytecodeUses->SetRemovedOpndSymbol(callTargetOpndIsJITOpt, callTargetStackSym->m_id);
        callInstr->InsertBefore(bytecodeUses);
    }

    return callInstr;
}

void Inline::GetArgInstrsForCallAndApply(IR::Instr* callInstr, IR::Instr** implicitThisArgOut, IR::Instr** explicitThisArgOut, IR::Instr** argumentsOrArrayArgOut, uint &argOutCount)
{LOGMEIN("Inline.cpp] 2591\n");
    IR::Opnd * linkOpnd = callInstr->GetSrc2()->AsSymOpnd();
    IR::Instr * argInsertInstr = callInstr;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr) {
        argOutCount++;

        *argumentsOrArrayArgOut = *explicitThisArgOut;
        *explicitThisArgOut = *implicitThisArgOut;
        *implicitThisArgOut = argInstr;

        linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_isInlinedArgSlot = true;
        linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_allocated = true;
        ConvertToInlineBuiltInArgOut(argInstr);

        argInstr->Move(argInsertInstr);
        argInsertInstr = argInstr;

        linkOpnd = argInstr->GetSrc2();
        return false;
    });
    linkOpnd->AsRegOpnd()->m_sym->m_isInlinedArgSlot = true;
}

/* 
This method only inlines targets which are script functions, under the
condition that the second argument (if any) passed to apply is arguments object.
*/
bool Inline::InlineApplyScriptTarget(IR::Instr *callInstr, const FunctionJITTimeInfo* inlinerData, const FunctionJITTimeInfo** pInlineeData, const FunctionJITTimeInfo *applyFuncInfo,
                            const StackSym *symCallerThis, IR::Instr ** returnInstr, uint recursiveInlineDepth, bool isArrayOpndArgumentsObject, uint argsCount)
{LOGMEIN("Inline.cpp] 2620\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    if (this->isApplyTargetInliningInProgress)
    {LOGMEIN("Inline.cpp] 2627\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: Skipping apply target inlining, Recursive apply inlining is not supported \tCaller: %s\t(%s) \tTop Func:%s\t(%s)\n"), inlinerData->GetBody()->GetDisplayName(),
                                inlinerData->GetDebugNumberSet(debugStringBuffer), this->topFunc->GetJITFunctionBody()->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer2));
        return false;
    }

    // Begin inlining apply target

    IR::Opnd* applyOpnd = callInstr->GetSrc1();
    Assert(applyOpnd->IsRegOpnd());
    StackSym* applySym = applyOpnd->AsRegOpnd()->m_sym->AsStackSym();
    if (!applySym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 2639\n");
        return false;
    }
    IR::Instr* applyLdInstr = applySym->GetInstrDef();
    IR::Instr* applyTargetLdInstr = applyLdInstr->m_prev;

    if(applyTargetLdInstr->m_opcode != Js::OpCode::LdFldForCallApplyTarget ||
        ((applyTargetLdInstr->AsProfiledInstr()->u.FldInfo().flags & Js::FldInfo_FromAccessor) != 0))
    {LOGMEIN("Inline.cpp] 2647\n");
        return false;
    }

    IR::Opnd *applyTargetLdOpnd = applyTargetLdInstr->GetSrc1();
    if (!applyTargetLdOpnd->IsSymOpnd() || !applyTargetLdOpnd->AsSymOpnd()->IsPropertySymOpnd())
    {LOGMEIN("Inline.cpp] 2653\n");
        return false;
    }

    const auto inlineCacheIndex = applyTargetLdOpnd->AsPropertySymOpnd()->m_inlineCacheIndex;
    const auto inlineeData = inlinerData->GetLdFldInlinee(inlineCacheIndex);

    if ((!isArrayOpndArgumentsObject && (argsCount == 2)) || SkipCallApplyScriptTargetInlining_Shared(callInstr, inlinerData, inlineeData, /*isApplyTarget*/ true, /*isCallTarget*/ false))
    {LOGMEIN("Inline.cpp] 2661\n");
        *pInlineeData = inlineeData;
        return false;
    }

    if (callInstr->m_func->IsTopFunc())
    {LOGMEIN("Inline.cpp] 2667\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: Skipping apply target inlining in top func\tCaller: %s\t(%s) \tTop Func:%s\t(%s)\n"), inlinerData->GetBody()->GetDisplayName(),
            inlinerData->GetDebugNumberSet(debugStringBuffer), this->topFunc->GetJITFunctionBody()->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer2));
        return false;
    }

    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool originalCallTargetOpndIsJITOpt = callInstr->GetSrc1()->GetIsJITOptimizedReg();
    bool safeThis = false;
    if (!TryGetFixedMethodsForBuiltInAndTarget(callInstr, inlinerData, inlineeData, applyFuncInfo, applyLdInstr, applyTargetLdInstr, safeThis, /*isApplyTarget*/ true))
    {LOGMEIN("Inline.cpp] 2677\n");
        return false;
    }

    // o.foo.apply(obj[, arguments])
    //
    // StartCall
    // ArgOut_A         <-- implicit "this" (foo) argout
    // ArgOut_A         <-- explicit "this" (obj) argout
    // ArgOut_A         <-- arguments object argout
    // CallIFixed

    IR::Instr* implicitThisArgOut = nullptr;
    IR::Instr* explicitThisArgOut = nullptr;
    IR::Instr* argumentsObjArgOut = nullptr;
    callInstr->IterateArgInstrs([&](IR::Instr* argInstr)
    {
        argumentsObjArgOut = explicitThisArgOut;
        explicitThisArgOut = implicitThisArgOut;
        implicitThisArgOut = argInstr;

        argInstr->GenerateBytecodeArgOutCapture(); // Generate BytecodeArgOutCapture here to capture the implicit "this" (to be removed) and arguments object (to be expanded) argouts,
                                                   // so that any bailout in the call sequence restores the argouts stack as the interpreter would expect it to be.
        argInstr->GetDst()->AsSymOpnd()->GetStackSym()->DecrementArgSlotNum(); // We will be removing implicit "this" argout
        return false;
    });

    if (safeThis)
    {LOGMEIN("Inline.cpp] 2705\n");
        IR::Instr * byteCodeArgOutCapture = explicitThisArgOut->GetBytecodeArgOutCapture();
        Assert(byteCodeArgOutCapture->GetSrc1()->IsRegOpnd());

        if (byteCodeArgOutCapture->GetSrc1()->AsRegOpnd()->GetStackSym() != symCallerThis)
        {LOGMEIN("Inline.cpp] 2710\n");
            safeThis = false;
        }
    }

    Assert(implicitThisArgOut->GetSrc2()->IsRegOpnd());
    IR::Instr * startCall = implicitThisArgOut->GetSrc2()->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef();
    Assert(startCall->m_opcode == Js::OpCode::StartCall);

    if (argumentsObjArgOut)
    {LOGMEIN("Inline.cpp] 2720\n");
        Assert(argsCount == 2);
        IR::Instr* argObjByteCodeArgoutCapture = argumentsObjArgOut->GetBytecodeArgOutCapture();
        argObjByteCodeArgoutCapture->GetDst()->GetStackSym()->m_nonEscapingArgObjAlias = true;

        argumentsObjArgOut->m_opcode = Js::OpCode::ArgOut_A_FromStackArgs;
    
        IR::Instr *  bailOutOnNotStackArgs = IR::BailOutInstr::New(Js::OpCode::BailOnNotStackArgs, IR::BailOutOnInlineFunction,
            callInstr, callInstr->m_func);
        // set src1 to avoid CSE on BailOnNotStackArgs for different arguments object
        bailOutOnNotStackArgs->SetSrc1(argumentsObjArgOut->GetSrc1()->Copy(this->topFunc));
        argumentsObjArgOut->InsertBefore(bailOutOnNotStackArgs);
    }

    IR::Instr* byteCodeArgOutUse = IR::Instr::New(Js::OpCode::BytecodeArgOutUse, callInstr->m_func);
    byteCodeArgOutUse->SetSrc1(implicitThisArgOut->GetSrc1());
    if (argumentsObjArgOut)
    {LOGMEIN("Inline.cpp] 2737\n");
        byteCodeArgOutUse->SetSrc2(argumentsObjArgOut->GetSrc1());
    }
    callInstr->InsertBefore(byteCodeArgOutUse);

    // don't need the implicit "this" anymore
    explicitThisArgOut->ReplaceSrc2(startCall->GetDst());
    implicitThisArgOut->Remove();

    startCall->SetSrc2(IR::IntConstOpnd::New(startCall->GetArgOutCount(/*getInterpreterArgOutCount*/ false), TyUint32, startCall->m_func));
    startCall->GetSrc1()->AsIntConstOpnd()->IncrValue(-1); // update the count of argouts as seen by JIT, in the start call instruction

    *returnInstr = InlineCallApplyTarget_Shared(callInstr, originalCallTargetOpndIsJITOpt, originalCallTargetStackSym, inlineeData, inlineCacheIndex,
                                                safeThis, /*isApplyTarget*/ true, /*isCallTarget*/ false, recursiveInlineDepth);
    return true;
}

IR::Instr *
Inline::InlineCallApplyTarget_Shared(IR::Instr *callInstr, bool originalCallTargetOpndIsJITOpt, StackSym* originalCallTargetStackSym, const FunctionJITTimeInfo *const inlineeData,
                                        uint inlineCacheIndex, bool safeThis, bool isApplyTarget, bool isCallTarget, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 2757\n");
    Assert(isApplyTarget ^ isCallTarget);

    // returnValueOpnd
    IR::RegOpnd * returnValueOpnd;
    Js::RegSlot returnRegSlot;
    if (callInstr->GetDst())
    {LOGMEIN("Inline.cpp] 2764\n");
        returnValueOpnd = callInstr->UnlinkDst()->AsRegOpnd();
        returnRegSlot = returnValueOpnd->m_sym->GetByteCodeRegSlot();
    }
    else
    {
        returnValueOpnd = nullptr;
        returnRegSlot = Js::Constants::NoRegister;
    }

    Assert(callInstr->IsProfiledInstr());
    Js::ProfileId callSiteId = static_cast<Js::ProfileId>(callInstr->AsProfiledInstr()->u.profileId);

    // inlinee
    CodeGenWorkItemIDL * workItemData = JitAnewStruct(this->topFunc->m_alloc, CodeGenWorkItemIDL);

    workItemData->isJitInDebugMode = this->topFunc->IsJitInDebugMode();
    workItemData->type = JsFunctionType;
    workItemData->jitMode = static_cast<char>(this->topFunc->GetWorkItem()->GetJitMode());
    workItemData->nativeDataAddr = this->topFunc->GetWorkItem()->GetWorkItemData()->nativeDataAddr;
    workItemData->loopNumber = Js::LoopHeader::NoLoop;

    workItemData->jitData = (FunctionJITTimeDataIDL*)(inlineeData);
    JITTimeWorkItem * jitWorkItem = JitAnew(this->topFunc->m_alloc, JITTimeWorkItem, workItemData);

    JITTimePolymorphicInlineCacheInfo * entryPointPolymorphicInlineCacheInfo = inlineeData->HasBody() ? this->topFunc->GetWorkItem()->GetInlineePolymorphicInlineCacheInfo(inlineeData->GetBody()->GetAddr()) : nullptr;
#if !FLOATVAR
    Func * inlinee = JitAnew(this->topFunc->m_alloc,
        Func,
        this->topFunc->m_alloc,
        jitWorkItem,
        this->topFunc->GetThreadContextInfo(),
        this->topFunc->GetScriptContextInfo(),
        this->topFunc->GetJITOutput()->GetOutputData(),
        nullptr,
        callInstr->m_func->GetWorkItem()->GetJITTimeInfo()->GetLdFldInlineeRuntimeData(inlineCacheIndex),
        entryPointPolymorphicInlineCacheInfo,
        this->topFunc->GetCodeGenAllocators(),
        this->topFunc->GetNumberAllocator(),
        this->topFunc->GetCodeGenProfiler(),
        this->topFunc->IsBackgroundJIT(),
        callInstr->m_func,
        callInstr->m_next->GetByteCodeOffset(),
        returnRegSlot,
        false,
        callSiteId,
        false);
#else
    Func * inlinee = JitAnew(this->topFunc->m_alloc,
        Func,
        this->topFunc->m_alloc,
        jitWorkItem,
        this->topFunc->GetThreadContextInfo(),
        this->topFunc->GetScriptContextInfo(),
        this->topFunc->GetJITOutput()->GetOutputData(),
        nullptr,
        callInstr->m_func->GetWorkItem()->GetJITTimeInfo()->GetLdFldInlineeRuntimeData(inlineCacheIndex),
        entryPointPolymorphicInlineCacheInfo,
        this->topFunc->GetCodeGenAllocators(),
        this->topFunc->GetCodeGenProfiler(),
        this->topFunc->IsBackgroundJIT(),
        callInstr->m_func,
        callInstr->m_next->GetByteCodeOffset(),
        returnRegSlot,
        false,
        callSiteId,
        false);
#endif

    // instrNext
    IR::Instr* instrNext = callInstr->m_next;

    return InlineFunctionCommon(callInstr, originalCallTargetOpndIsJITOpt, originalCallTargetStackSym, inlineeData, inlinee, instrNext, returnValueOpnd, callInstr, nullptr, recursiveInlineDepth, safeThis, isApplyTarget);
}

IR::Opnd *
Inline::ConvertToInlineBuiltInArgOut(IR::Instr * argInstr)
{LOGMEIN("Inline.cpp] 2841\n");
    argInstr->m_opcode = Js::OpCode::ArgOut_A_InlineBuiltIn;
    argInstr->GenerateBytecodeArgOutCapture();
    return argInstr->GetSrc1();
}

IR::Instr*
Inline::InlineCall(IR::Instr *callInstr, const FunctionJITTimeInfo *funcInfo, const FunctionJITTimeInfo * inlinerData, const StackSym *symCallerThis, bool* pIsInlined, uint callSiteId, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 2849\n");
    Func *func = callInstr->m_func;
    Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInForFuncInfo(funcInfo->GetFunctionInfoAddr(), func->GetThreadContextInfo());

    *pIsInlined = false;
    if (PHASE_OFF(Js::InlineCallPhase, this->topFunc) || PHASE_OFF(Js::InlineCallPhase, func)
        || !this->topFunc->GetJITFunctionBody()->GetInParamsCount())
    {LOGMEIN("Inline.cpp] 2856\n");
        return callInstr;
    }

    // Convert all the current ARG_OUT to  ArgOut_A_InlineBuiltIn
    IR::Opnd *linkOpnd = callInstr->GetSrc2();

    if (!GetDefInstr(linkOpnd)->GetSrc2()->IsSymOpnd())
    {LOGMEIN("Inline.cpp] 2864\n");
        // There is no benefit of inlining.call() with no arguments.
        return callInstr;
    }

    *pIsInlined = true;
    const FunctionJITTimeInfo * inlineeData = nullptr;

    IR::Instr * returnInstr = nullptr;
    if (!PHASE_OFF(Js::InlineCallTargetPhase, this->topFunc))
    {
        if (InlineCallTarget(callInstr, inlinerData, &inlineeData, funcInfo, symCallerThis, &returnInstr, recursiveInlineDepth))
        {LOGMEIN("Inline.cpp] 2876\n");
            Assert(returnInstr);
            return returnInstr;
        }
    }

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    TraceInlining(inlinerData, Js::JavascriptLibrary::GetNameForBuiltIn(builtInId),
        nullptr, 0, this->topFunc->GetWorkItem()->GetJITTimeInfo(), 0, nullptr, callSiteId, callInstr->m_func->GetTopFunc()->IsLoopBody(), builtInId);
#endif

    uint actualCount = 0;
    Assert(linkOpnd->IsSymOpnd());

    // We are trying to optimize this.superConstructor.call(this, a, b,c);
    // argImplicitInstr represents this.superConstructor which we need to call directly.
    IR::Instr * argImplicitInstr = nullptr;
    IR::Instr * dummyInstr1 = nullptr;
    IR::Instr * dummyInstr2 = nullptr;
    this->GetArgInstrsForCallAndApply(callInstr, &argImplicitInstr, &dummyInstr1, &dummyInstr2, actualCount);

    IR::SymOpnd* orgLinkOpnd = callInstr->GetSrc2()->AsSymOpnd();

    TryFixedMethodAndPrepareInsertionPoint(callInstr, funcInfo, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/);

    InsertInlineeBuiltInStartEndTags(callInstr, actualCount);

    uint actualCountToInlinedCall = actualCount - 1;

    IR::Instr *startCall = IR::Instr::New(Js::OpCode::StartCall, func);
    startCall->SetDst(IR::RegOpnd::New(TyVar, func));
    startCall->SetSrc1(IR::IntConstOpnd::New(actualCountToInlinedCall, TyInt32, func)); // New call will have one less parameter.

    callInstr->InsertBefore(startCall);

    callInstr->ReplaceSrc1(argImplicitInstr->GetSrc1());
    callInstr->UnlinkSrc2();
    callInstr->m_opcode = Js::OpCode::CallI;

    IR::Instr* insertBeforeInstr = callInstr;
    IR::Instr* clonedArgout = nullptr;
    IR::Instr* orgArgout = nullptr;

    for (uint i = actualCountToInlinedCall ; i > 0; i--)
    {LOGMEIN("Inline.cpp] 2920\n");
        orgArgout = GetDefInstr(orgLinkOpnd);
        orgLinkOpnd = orgArgout->GetSrc2()->AsSymOpnd();
        IR::Opnd *orgSrc1 = orgArgout->GetSrc1();

        // Change ArgOut to use temp as src1.
        StackSym * stackSym = StackSym::New(orgSrc1->GetStackSym()->GetType(), argImplicitInstr->m_func);
        IR::Opnd* tempDst = IR::RegOpnd::New(stackSym, orgSrc1->GetType(), argImplicitInstr->m_func);
        IR::Instr *assignInstr = IR::Instr::New(Js::OpCode::Ld_A, tempDst, orgSrc1, argImplicitInstr->m_func);
        assignInstr->SetByteCodeOffset(orgArgout);
        tempDst->SetIsJITOptimizedReg(true);
        orgArgout->InsertBefore(assignInstr);

        StackSym *symDst = callInstr->m_func->m_symTable->GetArgSlotSym((uint16)(i));
        IR::SymOpnd* newLinkOpnd = IR::SymOpnd::New(symDst, 0, TyMachPtr, func);

        clonedArgout = IR::Instr::New(Js::OpCode::ArgOut_A, newLinkOpnd, tempDst, func);
        insertBeforeInstr->SetSrc2(newLinkOpnd);

        insertBeforeInstr->InsertBefore(clonedArgout);
        insertBeforeInstr = clonedArgout;
    }
    clonedArgout->SetSrc2(startCall->GetDst());
    Assert(GetDefInstr(orgLinkOpnd) == argImplicitInstr);
    return callInstr;
}

bool
Inline::InlineCallTarget(IR::Instr *callInstr, const FunctionJITTimeInfo* inlinerData, const FunctionJITTimeInfo** pInlineeData, const FunctionJITTimeInfo *callFuncInfo,
                            const StackSym *symCallerThis, IR::Instr ** returnInstr, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 2950\n");
    IR::Opnd* src1 = callInstr->GetSrc1();
    Assert(src1->IsRegOpnd());
    StackSym* sym = src1->AsRegOpnd()->GetStackSym();
    if (!sym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 2955\n");
        return false;
    }
    IR::Instr* callLdInstr = sym->GetInstrDef();
    Assert(callLdInstr);

    IR::Instr* callTargetLdInstr = callLdInstr->m_prev;
    if (callTargetLdInstr->m_opcode != Js::OpCode::LdFldForCallApplyTarget ||
        ((callTargetLdInstr->AsProfiledInstr()->u.FldInfo().flags & Js::FldInfoFlags::FldInfo_FromAccessor) != 0))
    {LOGMEIN("Inline.cpp] 2964\n");
        return false;
    }

    IR::Opnd* callTargetLdOpnd = callTargetLdInstr->GetSrc1();
    if (!callTargetLdOpnd->IsSymOpnd() || !callTargetLdOpnd->AsSymOpnd()->IsPropertySymOpnd())
    {LOGMEIN("Inline.cpp] 2970\n");
        return false;
    }

    const auto inlineCacheIndex = callTargetLdOpnd->AsPropertySymOpnd()->m_inlineCacheIndex;
    const auto inlineeData = inlinerData->GetLdFldInlinee(inlineCacheIndex);

    if (SkipCallApplyScriptTargetInlining_Shared(callInstr, inlinerData, inlineeData, /*isApplyTarget*/ false, /*isCallTarget*/ true))
    {LOGMEIN("Inline.cpp] 2978\n");
        *pInlineeData = inlineeData;
        return false;
    }

    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool originalCallTargetOpndIsJITOpt = callInstr->GetSrc1()->GetIsJITOptimizedReg();
    bool safeThis = false;
    if (!TryGetFixedMethodsForBuiltInAndTarget(callInstr, inlinerData, inlineeData, callFuncInfo, callLdInstr, callTargetLdInstr, safeThis, /*isApplyTarget*/ false))
    {LOGMEIN("Inline.cpp] 2987\n");
        return false;
    }

    IR::Instr* implicitThisArgOut = nullptr;
    IR::Instr* explicitThisArgOut = nullptr;

    callInstr->IterateArgInstrs([&] (IR::Instr* argInstr)
    {
        explicitThisArgOut = implicitThisArgOut;
        implicitThisArgOut = argInstr;

        argInstr->GenerateBytecodeArgOutCapture(); // Generate BytecodeArgOutCapture here to capture the implicit "this" argout (which will be removed) as well,
                                                   // so that any bailout in the call sequence restores the argouts stack as the interpreter would expect it to be.
        argInstr->GetDst()->AsSymOpnd()->GetStackSym()->DecrementArgSlotNum(); // We will be removing implicit "this" argout
        return false;
    });

    Assert(explicitThisArgOut);
    Assert(explicitThisArgOut->HasByteCodeArgOutCapture());
    if (safeThis)
    {LOGMEIN("Inline.cpp] 3008\n");
        IR::Instr * byteCodeArgOutCapture = explicitThisArgOut->GetBytecodeArgOutCapture();
        Assert(byteCodeArgOutCapture->GetSrc1()->IsRegOpnd());

        if (byteCodeArgOutCapture->GetSrc1()->AsRegOpnd()->GetStackSym() != symCallerThis)
        {LOGMEIN("Inline.cpp] 3013\n");
            safeThis = false;
        }
    }

    IR::Opnd* linkOpnd = implicitThisArgOut->GetSrc2();
    Assert(linkOpnd->IsRegOpnd() && linkOpnd->AsRegOpnd()->GetStackSym()->IsSingleDef());
    Assert(linkOpnd->AsRegOpnd()->GetStackSym()->GetInstrDef()->m_opcode == Js::OpCode::StartCall);

    IR::Instr* startCall = linkOpnd->AsRegOpnd()->GetStackSym()->GetInstrDef();

    explicitThisArgOut->ReplaceSrc2(startCall->GetDst());

    IR::Instr * bytecodeArgOutUse = IR::Instr::New(Js::OpCode::BytecodeArgOutUse, callInstr->m_func);
    bytecodeArgOutUse->SetSrc1(implicitThisArgOut->GetSrc1());
    callInstr->InsertBefore(bytecodeArgOutUse); // Need to keep the implicit "this" argout live till the call instruction for it to be captured by any bailout in the call sequence.
    implicitThisArgOut->Remove();

    startCall->SetSrc2(IR::IntConstOpnd::New(startCall->GetArgOutCount(/*getInterpreterArgOutCount*/ false), TyUint32, startCall->m_func));
    startCall->GetSrc1()->AsIntConstOpnd()->SetValue(startCall->GetSrc1()->AsIntConstOpnd()->GetValue() - 1);

    *returnInstr = InlineCallApplyTarget_Shared(callInstr, originalCallTargetOpndIsJITOpt, originalCallTargetStackSym, inlineeData, inlineCacheIndex,
                                                safeThis, /*isApplyTarget*/ false, /*isCallTarget*/ true, recursiveInlineDepth);

    return true;
}

bool
Inline::SkipCallApplyScriptTargetInlining_Shared(IR::Instr *callInstr, const FunctionJITTimeInfo* inlinerData, const FunctionJITTimeInfo* inlineeData, bool isApplyTarget, bool isCallTarget)
{LOGMEIN("Inline.cpp] 3042\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    Assert(isApplyTarget ^ isCallTarget);

    if (PHASE_OFF(Js::FixedMethodsPhase, callInstr->m_func))
    {LOGMEIN("Inline.cpp] 3052\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: Skipping %s target inlining, Fixed Methods turned off\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n"), isApplyTarget ? _u("apply") : _u("call") ,
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer),
            this->topFunc->GetJITFunctionBody()->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer2));
        return true;
    }

    if (!inlineeData)
    {LOGMEIN("Inline.cpp] 3060\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: Skipping %s target inlining, inlineeData not present\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n"), isApplyTarget ? _u("apply") : _u("call"),
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer),
            this->topFunc->GetJITFunctionBody()->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer2));
        return true;
    }

    if (!inlineeData->GetBody())
    {LOGMEIN("Inline.cpp] 3068\n");
        if (isCallTarget)
        {LOGMEIN("Inline.cpp] 3070\n");
            INLINE_TESTTRACE(_u("INLINING: Skip Inline: Skipping .call inlining, target is a built-in\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n"),
                inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer),
                this->topFunc->GetJITFunctionBody()->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer2));
        }
        return true;
    }

    if (!inlinerData->IsLdFldInlineePresent())
    {LOGMEIN("Inline.cpp] 3079\n");
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: Skipping %s target inlining, not registered as a LdFld inlinee \tInlinee: %s (#%d)\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n"), isApplyTarget ? _u("apply") : _u("call"),
            inlineeData->GetBody()->GetDisplayName(), inlineeData->GetDebugNumberSet(debugStringBuffer),
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2),
            this->topFunc->GetJITFunctionBody()->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer3));
        return true;
    }

    return false;
}

bool
Inline::TryGetFixedMethodsForBuiltInAndTarget(IR::Instr *callInstr, const FunctionJITTimeInfo* inlinerData, const FunctionJITTimeInfo* inlineeData, const FunctionJITTimeInfo *builtInFuncInfo,
                                              IR::Instr* builtInLdInstr, IR::Instr* targetLdInstr, bool& safeThis, bool isApplyTarget)
{LOGMEIN("Inline.cpp] 3093\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

    Assert(isApplyTarget || Js::JavascriptLibrary::GetBuiltInForFuncInfo(builtInFuncInfo->GetFunctionInfoAddr(), this->topFunc->GetThreadContextInfo()));

    Js::OpCode originalCallOpCode = callInstr->m_opcode;
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool originalCallTargetOpndJITOpt = callInstr->GetSrc1()->GetIsJITOptimizedReg();

    IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr);

    safeThis = false;
    // Check if we can get fixed method for call
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, builtInFuncInfo/*funcinfo for call*/, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/,
        safeThis /*unused here*/, true /*dontOptimizeJustCheck*/))
    {LOGMEIN("Inline.cpp] 3112\n");
        Assert(callInstr->m_opcode == originalCallOpCode); // check that we didn't change the opcode to CallIFixed.
        callInstr->ReplaceSrc1(targetLdInstr->GetDst());
        safeThis = false;
        // Check if we can get fixed method for call target
        if (!TryOptimizeCallInstrWithFixedMethod(callInstr, inlineeData, false /*isPolymorphic*/, false /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/,
            safeThis /*unused here*/, true /*dontOptimizeJustCheck*/))
        {LOGMEIN("Inline.cpp] 3119\n");
            callInstr->ReplaceSrc1(builtInLdInstr->GetDst());
            INLINE_TESTTRACE(_u("INLINING: Skip Inline: Skipping %s target inlining, did not get fixed method for %s target \tInlinee: %s (#%d)\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n"), isApplyTarget ? _u("apply") : _u("call"), isApplyTarget ? _u("apply") : _u("call"),
                inlineeData->GetBody()->GetDisplayName(), inlineeData->GetDebugNumberSet(debugStringBuffer),
                inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2),
                this->topFunc->GetJITFunctionBody()->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer3));
            return false;
        }
    }
    else
    {
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: Skipping %s target inlining, did not get fixed method for %s \tInlinee: %s (#%d)\tCaller: %s\t(#%d) \tTop Func:%s\t(#%d)\n"), isApplyTarget ? _u("apply") : _u("call"), isApplyTarget ? _u("apply") : _u("call"),
            inlineeData->GetBody()->GetDisplayName(), inlineeData->GetDebugNumberSet(debugStringBuffer),
            inlinerData->GetBody()->GetDisplayName(), inlinerData->GetDebugNumberSet(debugStringBuffer2),
            this->topFunc->GetJITFunctionBody()->GetDisplayName(), this->topFunc->GetDebugNumberSet(debugStringBuffer3));
        return false;
    }

    if (isApplyTarget)
    {LOGMEIN("Inline.cpp] 3138\n");
        callInstr->m_func->SetHasApplyTargetInlining();
    }

    Assert(callInstr->m_opcode == originalCallOpCode);
    callInstr->ReplaceSrc1(builtInLdInstr->GetDst());

    // Emit Fixed Method check for apply/call
    safeThis = false;
    TryOptimizeCallInstrWithFixedMethod(callInstr, builtInFuncInfo/*funcinfo for apply/call */, false /*isPolymorphic*/, true /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/);

    // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after
    // the bailout on non-stack arguments.
    Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
    useCallTargetInstr->SetRemovedOpndSymbol(originalCallTargetOpndJITOpt, originalCallTargetStackSym->m_id);

    // Make the target of apply/call as the target of the call instruction
    callInstr->ReplaceSrc1(targetLdInstr->GetDst());
    callInstr->m_opcode = originalCallOpCode;

    //Emit Fixed Method check for apply/call target
    originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    safeThis = false;
    TryOptimizeCallInstrWithFixedMethod(callInstr, inlineeData, false /*isPolymorphic*/, false /*isBuiltIn*/, false /*isCtor*/, true /*isInlined*/, safeThis /*unused here*/);

    // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after
    // the bailout on non-stack arguments.
    Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
    useCallTargetInstr->SetRemovedOpndSymbol(originalCallTargetOpndJITOpt, originalCallTargetStackSym->m_id);

    callInstr->InsertBefore(useCallTargetInstr);

    return true;
}

void
Inline::SetupInlineInstrForCallDirect(Js::BuiltinFunction builtInId, IR::Instr* callInstr, IR::Instr* argoutInstr)
{LOGMEIN("Inline.cpp] 3175\n");
    switch(builtInId)
    {LOGMEIN("Inline.cpp] 3177\n");
    case Js::BuiltinFunction::JavascriptArray_Concat:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Concat, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_IndexOf:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_IndexOf, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_Includes:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Includes, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_Join:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Join, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_LastIndexOf:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_LastIndexOf, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_Reverse:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Reverse, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_Shift:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Shift, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_Slice:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Slice, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_Splice:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Splice, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptArray_Unshift:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperArray_Unshift, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Concat:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Concat, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_CharCodeAt:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_CharCodeAt, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_CharAt:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_CharAt, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_FromCharCode:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_FromCharCode, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_FromCodePoint:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_FromCodePoint, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_IndexOf:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_IndexOf, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_LastIndexOf:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_LastIndexOf, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Link:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Link, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_LocaleCompare:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_LocaleCompare, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Match:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Match, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Replace:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Replace, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Search:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Search, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Slice:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Slice, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Split:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Split, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Substr:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Substr, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Substring:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Substring, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_ToLocaleLowerCase:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_ToLocaleLowerCase, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_ToLocaleUpperCase:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_ToLocaleUpperCase, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_ToLowerCase:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_ToLowerCase, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_ToUpperCase:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_ToUpperCase, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_Trim:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_Trim, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_TrimLeft:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_TrimLeft, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_TrimRight:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_TrimRight, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_PadStart:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_PadStart, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptString_PadEnd:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperString_PadEnd, callInstr->m_func));
        break;

    case Js::BuiltinFunction::GlobalObject_ParseInt:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperGlobalObject_ParseInt, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptRegExp_Exec:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperRegExp_Exec, callInstr->m_func));
        break;

    case Js::BuiltinFunction::JavascriptRegExp_SymbolSearch:
        callInstr->SetSrc1(IR::HelperCallOpnd::New(IR::JnHelperMethod::HelperRegExp_SymbolSearch, callInstr->m_func));
        break;

    };
    callInstr->SetSrc2(argoutInstr->GetDst());
    return;
}

void
Inline::WrapArgsOutWithCoerse(Js::BuiltinFunction builtInId, IR::Instr* callInstr)
{LOGMEIN("Inline.cpp] 3337\n");
    switch (builtInId)
    {LOGMEIN("Inline.cpp] 3339\n");
    case Js::BuiltinFunction::JavascriptString_Match:
        callInstr->ForEachCallDirectArgOutInstrBackward([&](IR::Instr *argOutInstr, uint argNum)
        {
            IR::Instr * newInstr = nullptr;
            bool isPreOpBailOutNeeded = false;
            if (argNum == 0)
            {LOGMEIN("Inline.cpp] 3346\n");
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerce_Str);
                isPreOpBailOutNeeded = true;
                newInstr->GetDst()->SetValueType(ValueType::String);
                newInstr->SetSrc2(IR::AddrOpnd::New(newInstr->m_func->GetThreadContextInfo()->GetStringMatchNameAddr(), IR::AddrOpndKindSz, newInstr->m_func));
                argOutInstr->GetSrc1()->SetValueType(ValueType::String);
            }
            else if (argNum == 1)
            {LOGMEIN("Inline.cpp] 3354\n");
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerce_Regex);
                isPreOpBailOutNeeded = true;
            }
            if (isPreOpBailOutNeeded)
            {LOGMEIN("Inline.cpp] 3359\n");
                newInstr->SetByteCodeOffset(argOutInstr);
                newInstr->forcePreOpBailOutIfNeeded = true;
            }
            return false;
        }, 2);
        break;

    case Js::BuiltinFunction::JavascriptString_Replace:
        callInstr->ForEachCallDirectArgOutInstrBackward([&](IR::Instr *argOutInstr, uint argNum)
        {
            IR::Instr * newInstr = nullptr;
            bool isPreOpBailOutNeeded = false;
            if (argNum == 0)
            {LOGMEIN("Inline.cpp] 3373\n");
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerce_Str);
                isPreOpBailOutNeeded = true;
                newInstr->GetDst()->SetValueType(ValueType::String);
                newInstr->SetSrc2(IR::AddrOpnd::New(newInstr->m_func->GetThreadContextInfo()->GetStringReplaceNameAddr(), IR::AddrOpndKindSz, newInstr->m_func));
                argOutInstr->GetSrc1()->SetValueType(ValueType::String);
            }
            if (argNum == 1)
            {LOGMEIN("Inline.cpp] 3381\n");
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerce_StrOrRegex);
                isPreOpBailOutNeeded = true;
            }
            if (isPreOpBailOutNeeded)
            {LOGMEIN("Inline.cpp] 3386\n");
                newInstr->SetByteCodeOffset(argOutInstr);
                newInstr->forcePreOpBailOutIfNeeded = true;
            }
            return false;
        }, 3);
        break;
    case Js::BuiltinFunction::JavascriptRegExp_Exec:
        callInstr->ForEachCallDirectArgOutInstrBackward([&](IR::Instr *argOutInstr, uint argNum)
        {
            IR::Instr * newInstr = nullptr;
            bool isPreOpBailOutNeeded = false;
            if (argNum == 0)
            {LOGMEIN("Inline.cpp] 3399\n");
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Coerce_Regex);
                isPreOpBailOutNeeded = true;
            }
            else if (argNum == 1)
            {LOGMEIN("Inline.cpp] 3404\n");
                newInstr = argOutInstr->HoistSrc1(Js::OpCode::Conv_Str);
                newInstr->GetDst()->SetValueType(ValueType::String);
                argOutInstr->GetSrc1()->SetValueType(ValueType::String);
                isPreOpBailOutNeeded = true;
            }
            if (isPreOpBailOutNeeded)
            {LOGMEIN("Inline.cpp] 3411\n");
                newInstr->SetByteCodeOffset(argOutInstr);
                newInstr->forcePreOpBailOutIfNeeded = true;
            }
            return false;
        }, 2);
        break;
    }
}

IR::Instr *
Inline::SimulateCallForGetterSetter(IR::Instr *accessorInstr, IR::Instr* insertInstr, IR::PropertySymOpnd* methodOpnd, bool isGetter)
{LOGMEIN("Inline.cpp] 3423\n");
    Assert(methodOpnd->UsesAccessor());

    IntConstType argOutCount = isGetter ? 1 : 2; // A setter would have an additional ArgOut in the form of the value being set.

    IR::Instr *ldMethodFld = IR::Instr::New(Js::OpCode::LdMethodFromFlags, IR::RegOpnd::New(TyVar, accessorInstr->m_func), methodOpnd, accessorInstr->m_func);
    insertInstr->InsertBefore(ldMethodFld);
    ldMethodFld = ldMethodFld->ConvertToBailOutInstr(accessorInstr, IR::BailOutFailedInlineTypeCheck);
    ldMethodFld->SetByteCodeOffset(accessorInstr);

    IR::Instr *startCall = IR::Instr::New(Js::OpCode::StartCall, accessorInstr->m_func);
    startCall->SetDst(IR::RegOpnd::New(TyVar, accessorInstr->m_func));
    startCall->SetSrc1(IR::IntConstOpnd::New(argOutCount, TyInt32, accessorInstr->m_func));
    insertInstr->InsertBefore(startCall);
    startCall->SetByteCodeOffset(accessorInstr);

    PropertySym * fieldSym = methodOpnd->AsSymOpnd()->m_sym->AsPropertySym();
    IR::RegOpnd * instanceOpnd = IR::RegOpnd::New(fieldSym->m_stackSym, TyVar, accessorInstr->m_func);

    IR::Instr *argOutThis = IR::Instr::New(Js::OpCode::ArgOut_A, accessorInstr->m_func);

    StackSym *symDst = accessorInstr->m_func->m_symTable->GetArgSlotSym((uint16)(1));
    argOutThis->SetDst(IR::SymOpnd::New(symDst, 0, TyVar, accessorInstr->m_func));

    argOutThis->SetSrc1(instanceOpnd);
    argOutThis->SetSrc2(startCall->GetDst());
    insertInstr->InsertBefore(argOutThis);

    IR::Instr * argOut = nullptr;
    if(!isGetter)
    {LOGMEIN("Inline.cpp] 3453\n");
        // Set the src1 of the StFld to be the second ArgOut.
        argOut = IR::Instr::New(Js::OpCode::ArgOut_A, accessorInstr->m_func);
        symDst = accessorInstr->m_func->m_symTable->GetArgSlotSym((uint16)(2));

        argOut->SetDst(IR::SymOpnd::New(symDst, 0, TyVar, accessorInstr->m_func));
        argOut->SetSrc1(accessorInstr->GetSrc1());
        argOut->SetSrc2(argOutThis->GetDst());

        insertInstr->InsertBefore(argOut);
    }

    accessorInstr->ReplaceSrc1(ldMethodFld->GetDst());
    isGetter ? accessorInstr->SetSrc2(argOutThis->GetDst()) : accessorInstr->SetSrc2(argOut->GetDst());

    if(!isGetter)
    {LOGMEIN("Inline.cpp] 3469\n");
        accessorInstr->UnlinkDst();
    }

    return startCall;
}

IR::Instr *
Inline::InlineGetterSetterFunction(IR::Instr *accessorInstr, const FunctionJITTimeInfo *const inlineeData, const StackSym *symCallerThis, const uint inlineCacheIndex, bool isGetter, bool *pIsInlined, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 3478\n");
    // This function is recursive, so when jitting in the foreground, probe the stack
    if (!this->topFunc->IsBackgroundJIT())
    {LOGMEIN("Inline.cpp] 3481\n");
        PROBE_STACK(this->topFunc->GetScriptContext(), Js::Constants::MinStackDefault);
    }

    *pIsInlined = true;
    IR::Instr *instrNext = accessorInstr->m_next;

    const JITTimeFunctionBody *funcCaller = accessorInstr->m_func->GetJITFunctionBody();
    JITTimeFunctionBody *funcBody = inlineeData->GetBody();

    Assert(!accessorInstr->GetSrc2());

    JS_ETW(EventWriteJSCRIPT_BACKEND_INLINE(
        funcCaller->GetFunctionNumber(), funcBody->GetFunctionNumber(),
        funcCaller->GetDisplayName(), funcBody->GetDisplayName()));

    IR::Instr *inlineBailoutChecksBeforeInstr = accessorInstr;


    IR::RegOpnd * returnValueOpnd;
    Js::RegSlot returnRegSlot;
    if (isGetter && accessorInstr->GetDst())
    {LOGMEIN("Inline.cpp] 3503\n");
        returnValueOpnd = accessorInstr->UnlinkDst()->AsRegOpnd();
        returnRegSlot = returnValueOpnd->m_sym->GetByteCodeRegSlot();
    }
    else
    {
        returnValueOpnd = nullptr;
        returnRegSlot = Js::Constants::NoRegister;
    }

    // inlinee
    CodeGenWorkItemIDL * workItemData = JitAnewStruct(this->topFunc->m_alloc, CodeGenWorkItemIDL);

    workItemData->isJitInDebugMode = this->topFunc->IsJitInDebugMode();
    workItemData->type = JsFunctionType;
    workItemData->jitMode = static_cast<char>(this->topFunc->GetWorkItem()->GetJitMode());
    workItemData->nativeDataAddr = this->topFunc->GetWorkItem()->GetWorkItemData()->nativeDataAddr;
    workItemData->loopNumber = Js::LoopHeader::NoLoop;

    workItemData->jitData = (FunctionJITTimeDataIDL*)(inlineeData);
    JITTimeWorkItem * jitWorkItem = JitAnew(this->topFunc->m_alloc, JITTimeWorkItem, workItemData);

    JITTimePolymorphicInlineCacheInfo * entryPointPolymorphicInlineCacheInfo = this->topFunc->GetWorkItem()->GetInlineePolymorphicInlineCacheInfo(funcBody->GetAddr());
#if !FLOATVAR
    Func * inlinee = JitAnew(this->topFunc->m_alloc,
        Func,
        this->topFunc->m_alloc,
        jitWorkItem,
        this->topFunc->GetThreadContextInfo(),
        this->topFunc->GetScriptContextInfo(),
        this->topFunc->GetJITOutput()->GetOutputData(),
        nullptr,
        accessorInstr->m_func->GetWorkItem()->GetJITTimeInfo()->GetLdFldInlineeRuntimeData(inlineCacheIndex),
        entryPointPolymorphicInlineCacheInfo,
        this->topFunc->GetCodeGenAllocators(),
        this->topFunc->GetNumberAllocator(),
        this->topFunc->GetCodeGenProfiler(),
        this->topFunc->IsBackgroundJIT(),
        accessorInstr->m_func,
        accessorInstr->m_next->GetByteCodeOffset(),
        returnRegSlot,
        false,
        UINT16_MAX,
        true);
#else
    Func * inlinee = JitAnew(this->topFunc->m_alloc,
        Func,
        this->topFunc->m_alloc,
        jitWorkItem,
        this->topFunc->GetThreadContextInfo(),
        this->topFunc->GetScriptContextInfo(),
        this->topFunc->GetJITOutput()->GetOutputData(),
        nullptr,
        accessorInstr->m_func->GetWorkItem()->GetJITTimeInfo()->GetLdFldInlineeRuntimeData(inlineCacheIndex),
        entryPointPolymorphicInlineCacheInfo,
        this->topFunc->GetCodeGenAllocators(),
        this->topFunc->GetCodeGenProfiler(),
        this->topFunc->IsBackgroundJIT(),
        accessorInstr->m_func,
        accessorInstr->m_next->GetByteCodeOffset(),
        returnRegSlot,
        false,
        UINT16_MAX,
        true);
#endif

    // funcBody->GetInParamsCount() can be greater than one even if it is all undefined. Example defineProperty(a,"foo", {get:function(a,b,c){}});

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::InlinePhase) ||
        Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::InlineAccessorsPhase) || Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlineAccessorsPhase))
    {LOGMEIN("Inline.cpp] 3574\n");
        char16 debugStringBuffer [MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        PropertySym *propertySym = isGetter ? accessorInstr->GetSrc1()->AsSymOpnd()->m_sym->AsPropertySym() : accessorInstr->GetDst()->AsSymOpnd()->m_sym->AsPropertySym();
        if (JITManager::GetJITManager()->IsOOPJITEnabled())
        {LOGMEIN("Inline.cpp] 3579\n");
            Output::Print(_u("INLINING: %s: \tInlinee: %s (%s)\tCaller: %s (%s)\t fieldId: %d\n"), isGetter ? _u("Getter") : _u("Setter"),
                funcBody->GetDisplayName(), inlineeData->GetDebugNumberSet(debugStringBuffer), funcCaller->GetDisplayName(), accessorInstr->m_func->GetWorkItem()->GetJITTimeInfo()->GetDebugNumberSet(debugStringBuffer2),
                propertySym->m_propertyId);
        }
        else
        {
            Output::Print(_u("INLINING: %s: \tInlinee: %s (%s)\tCaller: %s (%s)\t fieldName: %s\n"), isGetter ? _u("Getter") : _u("Setter"),
                funcBody->GetDisplayName(), inlineeData->GetDebugNumberSet(debugStringBuffer), funcCaller->GetDisplayName(), accessorInstr->m_func->GetWorkItem()->GetJITTimeInfo()->GetDebugNumberSet(debugStringBuffer2),
                propertySym->GetFunc()->GetInProcThreadContext()->GetPropertyRecord(propertySym->m_propertyId)->GetBuffer());
        }
        Output::Flush();
    }
#endif
    IR::Opnd * methodOpnd = isGetter ? accessorInstr->GetSrc1() : accessorInstr->GetDst();
    Assert(methodOpnd->IsSymOpnd() && methodOpnd->AsSymOpnd()->IsPropertySymOpnd());

    inlineBailoutChecksBeforeInstr = SimulateCallForGetterSetter(accessorInstr, accessorInstr, methodOpnd->AsPropertySymOpnd(), isGetter);

    bool safeThis = false;
    TryOptimizeCallInstrWithFixedMethod(accessorInstr, inlineeData, false, false, false, true, safeThis);

    return InlineFunctionCommon(accessorInstr, false, nullptr, inlineeData, inlinee, instrNext, returnValueOpnd, inlineBailoutChecksBeforeInstr, symCallerThis, recursiveInlineDepth, safeThis);
}

IR::Instr *
Inline::InlineFunctionCommon(IR::Instr *callInstr, bool originalCallTargetOpndIsJITOpt, StackSym* originalCallTargetStackSym, const FunctionJITTimeInfo *funcInfo, Func *inlinee, IR::Instr *instrNext,
                                IR::RegOpnd * returnValueOpnd, IR::Instr *inlineBailoutChecksBeforeInstr, const StackSym *symCallerThis, uint recursiveInlineDepth, bool safeThis, bool isApplyTarget)
{
    BuildIRForInlinee(inlinee, funcInfo->GetBody(), callInstr, isApplyTarget, recursiveInlineDepth);

    Js::ArgSlot formalCount = funcInfo->GetBody()->GetInParamsCount();
    IR::Instr *argOuts[Js::InlineeCallInfo::MaxInlineeArgoutCount];
#if DBG
    memset(argOuts, 0xFE, sizeof(argOuts));
#endif
    if (callInstr->m_opcode == Js::OpCode::CallIFixed)
    {LOGMEIN("Inline.cpp] 3616\n");
        Assert(callInstr->GetFixedFunction()->GetFuncInfoAddr() == funcInfo->GetFunctionInfoAddr());
    }
    else
    {
        PrepareInsertionPoint(callInstr, funcInfo, inlineBailoutChecksBeforeInstr);
    }

    Assert(formalCount <= Js::InlineeCallInfo::MaxInlineeArgoutCount);
    __analysis_assume(formalCount <= Js::InlineeCallInfo::MaxInlineeArgoutCount);

    IR::Instr *argOutsExtra[Js::InlineeCallInfo::MaxInlineeArgoutCount];
#if DBG
    memset(argOutsExtra, 0xFE, sizeof(argOutsExtra));
#endif

    bool stackArgsArgOutExpanded = false;
    Js::ArgSlot actualCount = MapActuals(callInstr, argOuts, formalCount, inlinee, (Js::ProfileId)callInstr->AsProfiledInstr()->u.profileId, &stackArgsArgOutExpanded, argOutsExtra);
    inlinee->actualCount = actualCount;
    Assert(actualCount > 0);

#if DBG
    if(safeThis)
    {LOGMEIN("Inline.cpp] 3639\n");
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);
    }
#endif

    MapFormals(inlinee, argOuts, formalCount, actualCount, returnValueOpnd, callInstr->GetSrc1(), symCallerThis, stackArgsArgOutExpanded, safeThis, argOutsExtra);

    if (callInstr->m_opcode == Js::OpCode::CallIFixed && !inlinee->isGetterSetter)
    {LOGMEIN("Inline.cpp] 3647\n");
        Assert(originalCallTargetStackSym != nullptr);

        // Insert a ByteCodeUsesInstr to make sure the function object's lifetimes is extended beyond the last bailout point
        // at which we may have to call the function again in the interpreter.
        // Don't need to do this for a getter/setter inlinee as, upon bailout, the execution will start in the interpreter at the LdFld/StFld itself.
        IR::ByteCodeUsesInstr* bytecodeUses = IR::ByteCodeUsesInstr::New(callInstr);
        bytecodeUses->SetRemovedOpndSymbol(originalCallTargetOpndIsJITOpt, originalCallTargetStackSym->m_id);
        callInstr->InsertBefore(bytecodeUses);
    }

    // InlineeStart indicate the beginning of the inlinee, and we need the stack arg for the inlinee until InlineeEnd
    callInstr->m_opcode = Js::OpCode::InlineeStart;

    // Set it to belong to the inlinee, so that we can use the actual count when lowering InlineeStart
    callInstr->m_func = inlinee;
    callInstr->SetDst(IR::RegOpnd::New(TyVar, inlinee));
    // Put the meta arguments that the stack walker expects to find on the stack.
    SetupInlineeFrame(inlinee, callInstr, actualCount, callInstr->GetSrc1());

    // actualCount + MetaArgCount to include the meta arguments to pop from the inlinee argout stack.
    IR::Instr *inlineeEndInstr = IR::Instr::New(Js::OpCode::InlineeEnd, inlinee);
    inlineeEndInstr->SetByteCodeOffset(inlinee->m_tailInstr->GetPrevRealInstr());
    inlineeEndInstr->SetSrc1(IR::IntConstOpnd::New(actualCount + Js::Constants::InlineeMetaArgCount, TyInt32, callInstr->m_func));
    inlineeEndInstr->SetSrc2(callInstr->GetDst()); // Link the inlinee end to the inlinee Start
    callInstr->InsertAfter(inlineeEndInstr);

    // Move the ArgOut_A_Inlines close to the InlineeStart
    callInstr->MoveArgs();

    inlineeEndInstr->InsertRangeBefore(inlinee->m_headInstr->m_next, inlinee->m_tailInstr->m_prev);
    inlinee->m_headInstr->Free();
    inlinee->m_tailInstr->Free();

    this->topFunc->SetHasInlinee();

    InsertStatementBoundary(instrNext);

    return instrNext;
}

#ifdef ENABLE_DOM_FAST_PATH
// we have LdFld, src1 obj, src2: null; dest: return value
// We need to convert it to inlined method call.
// We cannot do CallDirect as it requires ArgOut and that cannot be hoisted/copyprop'd
// Create a new OpCode, DOMFastPathGetter. The OpCode takes three arguments:
// The function object, the "this" instance object, and the helper routine as we have one for each index
// A functionInfo->Index# table is created in scriptContext (and potentially movable to threadContext if WS is not a concern).
// we use the table to identify the helper that needs to be lowered.
// At lower time we create the call to helper, which is function entrypoint at this time.
IR::Instr * Inline::InlineDOMGetterSetterFunction(IR::Instr *ldFldInstr, const FunctionJITTimeInfo *const inlineeData, const FunctionJITTimeInfo *const inlinerData)
{LOGMEIN("Inline.cpp] 3698\n");
    intptr_t functionInfo = inlineeData->GetFunctionInfoAddr();

    Assert(ldFldInstr->GetSrc1()->IsSymOpnd() && ldFldInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd());

    Assert(ldFldInstr->GetSrc1()->AsPropertySymOpnd()->HasObjTypeSpecFldInfo());
    Assert(ldFldInstr->GetSrc1()->AsPropertySymOpnd()->GetObjTypeSpecInfo()->UsesAccessor());

    // Find the helper routine for this functionInfo.
    IR::JnHelperMethod helperMethod = this->topFunc->GetScriptContextInfo()->GetDOMFastPathHelper(functionInfo);

    // Find the instance object (External object).
    PropertySym * fieldSym = ldFldInstr->GetSrc1()->AsSymOpnd()->m_sym->AsPropertySym();
    IR::RegOpnd * instanceOpnd = IR::RegOpnd::New(fieldSym->m_stackSym, TyMachPtr, ldFldInstr->m_func);

    // Find the function object from getter inline cache. Need bailout to verify.
    IR::Instr *ldMethodFld = IR::Instr::New(Js::OpCode::LdMethodFromFlags, IR::RegOpnd::New(TyVar, ldFldInstr->m_func), ldFldInstr->GetSrc1(), ldFldInstr->m_func);
    ldFldInstr->InsertBefore(ldMethodFld);
    ldMethodFld = ldMethodFld->ConvertToBailOutInstr(ldFldInstr, IR::BailOutFailedInlineTypeCheck);

    ldFldInstr->ReplaceSrc1(ldMethodFld->GetDst());
    ldMethodFld->SetByteCodeOffset(ldFldInstr);

    // generate further object/type bailout
    PrepareInsertionPoint(ldFldInstr, inlineeData, ldFldInstr);

    // We have three arguments to pass to the OpCode. Create a new ExtendArg_A opcode to chain up the argument. It is similar to ArgOut chain
    // except that it is not argout.
    // The Opcode sequence is like:
    // (dst)helpArg1: ExtendArg_A (src1)thisObject (src2)null
    // (dst)helpArg2: ExtendArg_A (src1)funcObject (src2)helpArg1
    // method: DOMFastPathGetter (src1)HelperCall (src2)helpArg2
    IR::Instr* extendArg0 = IR::Instr::New(Js::OpCode::ExtendArg_A, IR::RegOpnd::New(TyVar, ldFldInstr->m_func), instanceOpnd, ldFldInstr->m_func);
    ldFldInstr->InsertBefore(extendArg0);
    IR::Instr* extendArg1 = IR::Instr::New(Js::OpCode::ExtendArg_A, IR::RegOpnd::New(TyVar, ldFldInstr->m_func), ldMethodFld->GetDst(), extendArg0->GetDst(), ldFldInstr->m_func);
    ldFldInstr->InsertBefore(extendArg1);
    ldFldInstr->ReplaceSrc1(IR::HelperCallOpnd::New(helperMethod, ldFldInstr->m_func));
    ldFldInstr->SetSrc2(extendArg1->GetDst());
    ldFldInstr->m_opcode = Js::OpCode::DOMFastPathGetter;

    StackSym * tmpSym = StackSym::New(ldFldInstr->GetDst()->GetType(), ldFldInstr->m_func);
    IR::Opnd * tmpDst = IR::RegOpnd::New(tmpSym, tmpSym->GetType(), ldFldInstr->m_func);

    IR::Opnd * callInstrDst = ldFldInstr->UnlinkDst();
    ldFldInstr->SetDst(tmpDst);

    IR::Instr * ldInstr = IR::Instr::New(Js::OpCode::Ld_A, callInstrDst, tmpDst, ldFldInstr->m_func);
    ldFldInstr->InsertAfter(ldInstr);

    this->topFunc->SetHasInlinee();

    InsertStatementBoundary(ldInstr->m_next);

    return ldInstr->m_next;
}
#endif
void
Inline::InsertStatementBoundary(IR::Instr * instrNext)
{LOGMEIN("Inline.cpp] 3756\n");
    if (lastStatementBoundary)
    {LOGMEIN("Inline.cpp] 3758\n");
        Assert(lastStatementBoundary->m_func == instrNext->m_func);
        IR::PragmaInstr * pragmaInstr = IR::PragmaInstr::New(Js::OpCode::StatementBoundary,
            lastStatementBoundary->m_statementIndex,
            lastStatementBoundary->m_func);
        pragmaInstr->SetByteCodeOffset(instrNext);
        instrNext->InsertBefore(pragmaInstr);
    }
}

IR::Instr *
Inline::InlineScriptFunction(IR::Instr *callInstr, const FunctionJITTimeInfo *const inlineeData, const StackSym *symCallerThis, const Js::ProfileId profileId, bool* pIsInlined, uint recursiveInlineDepth)
{LOGMEIN("Inline.cpp] 3770\n");
    *pIsInlined = false;

    // This function is recursive, so when jitting in the foreground, probe the stack
    if (!this->topFunc->IsBackgroundJIT())
    {LOGMEIN("Inline.cpp] 3775\n");
        PROBE_STACK(this->topFunc->GetScriptContext(), Js::Constants::MinStackDefault);
    }

    IR::Instr *instrNext = callInstr->m_next;

    Func *funcCaller = callInstr->m_func;
    JITTimeFunctionBody *funcBody = inlineeData->GetBody();

    // We don't do stack args optimization in jitted loop body (because of lack of information about the code before and after the loop)
    // and we turn off stack arg optimization for the whole inline chain if we can't do it for one of the functionss.
    // Inlining a function that uses arguments object could potentially hurt perf because we'll have to create arguments object on the
    // heap for that function (versus otherwise the function will be jitted and have its arguments object creation optimized).
    // TODO: Allow arguments object creation to be optimized on a function level instead of an all-or-nothing approach.
    if (callInstr->m_func->IsLoopBody() && funcBody->UsesArgumentsObject())
    {LOGMEIN("Inline.cpp] 3790\n");
        return instrNext;
    }

    if (callInstr->GetSrc2() &&
        callInstr->GetSrc2()->IsSymOpnd() &&
        callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum() > Js::InlineeCallInfo::MaxInlineeArgoutCount)
    {LOGMEIN("Inline.cpp] 3797\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo. Although
        // InliningDecider already checks for this, the check is against profile data that may not be accurate since profile
        // data matching does not take into account some types of changes to source code. Need to check this again with current
        // information.
        INLINE_TESTTRACE(_u("INLINING: Skip Inline: ArgSlot > MaxInlineeArgoutCount\tInlinee: %s (%s)\tArgSlotNum: %d\tMaxInlineeArgoutCount: %d\tCaller: %s (%s)\n"),
            funcBody->GetDisplayName(), inlineeData->GetDebugNumberSet(debugStringBuffer), callInstr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum(),
            Js::InlineeCallInfo::MaxInlineeArgoutCount, funcCaller->GetJITFunctionBody()->GetDisplayName(), funcCaller->GetDebugNumberSet(debugStringBuffer2));
        return instrNext;
    }

    *pIsInlined = true;

    // Save off the call target operand (function object) so we can extend its lifetime as needed, even if
    // the call instruction gets transformed to CallIFixed.
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool originalCallTargetOpndIsJITOpt = callInstr->GetSrc1()->GetIsJITOptimizedReg();

    // We are committed to inlining, optimize the call instruction for fixed fields now and don't attempt it later.
    bool isFixed = false;

    JS_ETW(EventWriteJSCRIPT_BACKEND_INLINE(
        funcCaller->GetFunctionNumber(), funcBody->GetFunctionNumber(),
        funcCaller->GetJITFunctionBody()->GetDisplayName(), funcBody->GetDisplayName()));// REVIEW: OOP JIT, was GetExternalDisplayName, does this matter?

    bool isCtor = false;
    bool safeThis = false;
    IR::Instr *inlineBailoutChecksBeforeInstr;
    if (callInstr->m_opcode == Js::OpCode::NewScObject || callInstr->m_opcode == Js::OpCode::NewScObjArray)
    {LOGMEIN("Inline.cpp] 3830\n");
        isCtor = true;
        isFixed = TryOptimizeCallInstrWithFixedMethod(callInstr, inlineeData,
            false /*isPolymorphic*/, false /*isBuiltIn*/, isCtor /*isCtor*/, true /*isInlined*/, safeThis /*&safeThis*/);
        bool split = SplitConstructorCall(callInstr, true, isFixed, &inlineBailoutChecksBeforeInstr);
        Assert(split && inlineBailoutChecksBeforeInstr != nullptr);
    }
    else
    {
        isFixed = TryOptimizeCallInstrWithFixedMethod(callInstr, inlineeData,
            false /*isPolymorphic*/, false /*isBuiltIn*/, isCtor /*isCtor*/, true /*isInlined*/, safeThis /*&safeThis*/);
        inlineBailoutChecksBeforeInstr = callInstr;
    }

    Assert(callInstr->IsProfiledInstr());
    Js::ProfileId callSiteId = static_cast<Js::ProfileId>(callInstr->AsProfiledInstr()->u.profileId);

    IR::RegOpnd * returnValueOpnd;
    Js::RegSlot returnRegSlot;
    if (callInstr->GetDst())
    {LOGMEIN("Inline.cpp] 3850\n");
        returnValueOpnd = callInstr->UnlinkDst()->AsRegOpnd();
        returnRegSlot = returnValueOpnd->m_sym->GetByteCodeRegSlot();
    }
    else
    {
        returnValueOpnd = nullptr;
        returnRegSlot = Js::Constants::NoRegister;
    }

    CodeGenWorkItemIDL * workItemData = JitAnewStruct(this->topFunc->m_alloc, CodeGenWorkItemIDL);

    workItemData->isJitInDebugMode = this->topFunc->IsJitInDebugMode();
    workItemData->type = JsFunctionType;
    workItemData->jitMode = static_cast<char>(this->topFunc->GetWorkItem()->GetJitMode());
    workItemData->nativeDataAddr = this->topFunc->GetWorkItem()->GetWorkItemData()->nativeDataAddr;
    workItemData->loopNumber = Js::LoopHeader::NoLoop;

    workItemData->jitData = (FunctionJITTimeDataIDL*)(inlineeData);
    JITTimeWorkItem * jitWorkItem = JitAnew(this->topFunc->m_alloc, JITTimeWorkItem, workItemData);


    JITTimePolymorphicInlineCacheInfo * entryPointPolymorphicInlineCacheInfo = this->topFunc->GetWorkItem()->GetInlineePolymorphicInlineCacheInfo(funcBody->GetAddr());
#if !FLOATVAR
    Func * inlinee = JitAnew(this->topFunc->m_alloc,
        Func,
        this->topFunc->m_alloc,
        jitWorkItem,
        this->topFunc->GetThreadContextInfo(),
        this->topFunc->GetScriptContextInfo(),
        this->topFunc->GetJITOutput()->GetOutputData(),
        nullptr,
        funcCaller->GetWorkItem()->GetJITTimeInfo()->GetInlineeForTargetInlineeRuntimeData(profileId, funcBody->GetAddr()),
        entryPointPolymorphicInlineCacheInfo,
        this->topFunc->GetCodeGenAllocators(),
        this->topFunc->GetNumberAllocator(),
        this->topFunc->GetCodeGenProfiler(),
        this->topFunc->IsBackgroundJIT(),
        callInstr->m_func,
        callInstr->GetNextRealInstr()->GetByteCodeOffset(),
        returnRegSlot,
        isCtor,
        callSiteId,
        false);
#else
    Func * inlinee = JitAnew(this->topFunc->m_alloc,
        Func,
        this->topFunc->m_alloc,
        jitWorkItem,
        this->topFunc->GetThreadContextInfo(),
        this->topFunc->GetScriptContextInfo(),
        this->topFunc->GetJITOutput()->GetOutputData(),
        nullptr,
        funcCaller->GetWorkItem()->GetJITTimeInfo()->GetInlineeForTargetInlineeRuntimeData(profileId, funcBody->GetAddr()),
        entryPointPolymorphicInlineCacheInfo,
        this->topFunc->GetCodeGenAllocators(),
        this->topFunc->GetCodeGenProfiler(),
        this->topFunc->IsBackgroundJIT(),
        callInstr->m_func,
        callInstr->GetNextRealInstr()->GetByteCodeOffset(),
        returnRegSlot,
        isCtor,
        callSiteId,
        false);
#endif

    return InlineFunctionCommon(callInstr, originalCallTargetOpndIsJITOpt, originalCallTargetStackSym, inlineeData, inlinee, instrNext, returnValueOpnd, inlineBailoutChecksBeforeInstr, symCallerThis, recursiveInlineDepth, safeThis);
}

bool
Inline::SplitConstructorCall(IR::Instr *const newObjInstr, const bool isInlined, const bool isFixed, IR::Instr** createObjInstrOut, IR::Instr** callCtorInstrOut) const
{LOGMEIN("Inline.cpp] 3921\n");
    Assert(newObjInstr);
    Assert(newObjInstr->m_opcode == Js::OpCode::NewScObject);
    Assert(newObjInstr->GetSrc1());
    Assert(newObjInstr->GetSrc2());

    this->topFunc->SetHasTempObjectProducingInstr(true);

    return
        SplitConstructorCallCommon(
            newObjInstr,
            newObjInstr->GetSrc2(),
            Js::OpCode::NewScObjectNoCtor,
            isInlined,
            isFixed,
            createObjInstrOut,
            callCtorInstrOut);
}

bool
Inline::SplitConstructorCallCommon(
    IR::Instr *const newObjInstr,
    IR::Opnd *const lastArgOpnd,
    const Js::OpCode newObjOpCode,
    const bool isInlined,
    const bool isFixed,
    IR::Instr** createObjInstrOut,
    IR::Instr** callCtorInstrOut) const
{LOGMEIN("Inline.cpp] 3949\n");
    Assert(newObjInstr);
    Assert(newObjInstr->GetSrc1());
    Assert(lastArgOpnd);
    Assert(isInlined || isFixed);

    const auto callerFunc = newObjInstr->m_func;

    // Call the NoCtor version of NewScObject

    // Use a temporary register for the newly allocated object (before the call to ctor) - even if we know we'll return this
    // object from the whole operation.  That's so that we don't trash the bytecode register if we need to bail out at
    // object allocation (bytecode instruction has the form [Profiled]NewScObject R6 = R6).
    IR::RegOpnd* createObjDst = nullptr;
    IR::Instr* createObjInstr = nullptr;
    const JITTimeConstructorCache* constructorCache;
    bool returnCreatedObject = false;
    bool skipNewScObj = false;

    if (newObjInstr->IsProfiledInstr())
    {LOGMEIN("Inline.cpp] 3969\n");
        Js::ProfileId profiledCallSiteId = static_cast<Js::ProfileId>(newObjInstr->AsProfiledInstr()->u.profileId);
        constructorCache = newObjInstr->m_func->GetConstructorCache(profiledCallSiteId);
        returnCreatedObject = constructorCache != nullptr && constructorCache->CtorHasNoExplicitReturnValue();
        skipNewScObj = constructorCache != nullptr && constructorCache->SkipNewScObject();
        if (!skipNewScObj)
        {LOGMEIN("Inline.cpp] 3975\n");
            createObjDst = IR::RegOpnd::New(TyVar, callerFunc);
            createObjInstr = IR::ProfiledInstr::New(newObjOpCode, createObjDst, newObjInstr->GetSrc1(), callerFunc);
            createObjInstr->AsProfiledInstr()->u.profileId = profiledCallSiteId;
        }
    }
    else
    {
        constructorCache = nullptr;
        createObjDst = IR::RegOpnd::New(TyVar, callerFunc);
        createObjInstr = IR::Instr::New(newObjOpCode, createObjDst, newObjInstr->GetSrc1(), callerFunc);
    }

    Assert(!isInlined || !skipNewScObj);
    Assert(isFixed || !skipNewScObj);

    // For new Object() and new Array() we have special fast helpers.  We'll let the lowerer convert this instruction directly
    // into a call to one of these helpers.
    if (skipNewScObj)
    {LOGMEIN("Inline.cpp] 3994\n");
        JITTimeFixedField* ctor = newObjInstr->GetFixedFunction();
        intptr_t ctorInfo = ctor->GetFuncInfoAddr();
        if ((ctorInfo == topFunc->GetThreadContextInfo()->GetJavascriptObjectNewInstanceAddr() ||
            ctorInfo == topFunc->GetThreadContextInfo()->GetJavascriptArrayNewInstanceAddr()) &&
            newObjInstr->HasEmptyArgOutChain())
        {LOGMEIN("Inline.cpp] 4000\n");
            return false;
        }
    }

    IR::Opnd* thisPtrOpnd;
    if (createObjInstr != nullptr)
    {LOGMEIN("Inline.cpp] 4007\n");
        createObjInstr->SetByteCodeOffset(newObjInstr);
        createObjInstr->GetSrc1()->SetIsJITOptimizedReg(true);
        newObjInstr->InsertBefore(createObjInstr);

        createObjDst->SetValueType(ValueType::GetObject(ObjectType::UninitializedObject));
        thisPtrOpnd = createObjDst;
    }
    else
    {
        thisPtrOpnd = IR::AddrOpnd::NewNull(newObjInstr->m_func);
    }

    // Pass the new object to the constructor function with an ArgOut
    const auto thisArgOpnd = IR::SymOpnd::New(callerFunc->m_symTable->GetArgSlotSym(1), TyVar, callerFunc);
    auto instr = IR::Instr::New(Js::OpCode::ArgOut_A, thisArgOpnd, thisPtrOpnd, lastArgOpnd, callerFunc);
    instr->SetByteCodeOffset(newObjInstr);
    instr->GetDst()->SetIsJITOptimizedReg(true);
    instr->GetSrc2()->SetIsJITOptimizedReg(true);
    newObjInstr->InsertBefore(instr);

    // Call the constructor using CallI with isCtorCall set.  If we inline the constructor, and the inlined constructor
    // bails out, the interpreter would be entered with CallFlags_Value as well. If the interpreter starts using the
    // call flags, the proper call flags will need to be specified here by using a different op code specific to constructors.
    if (isFixed)
    {LOGMEIN("Inline.cpp] 4032\n");
        newObjInstr->m_opcode = Js::OpCode::CallIFixed;
    }
    else
    {
        newObjInstr->m_opcode = Js::OpCode::CallI;
    }

    newObjInstr->isCtorCall = true;

    if(newObjInstr->GetSrc2())
    {LOGMEIN("Inline.cpp] 4043\n");
        newObjInstr->FreeSrc2();
    }
    newObjInstr->SetSrc2(thisArgOpnd);

    const auto insertBeforeInstr = newObjInstr->m_next;
    Assert(insertBeforeInstr);
    const auto nextByteCodeOffsetInstr = newObjInstr->GetNextRealInstrOrLabel();

    // Determine which object to use as the final result of NewScObject, the object passed into the constructor as 'this', or
    // the object returned by the constructor.  We only need this if we don't have a hard-coded constructor cache, or if the
    // constructor returns something explicitly.  Otherwise, we simply return the object we allocated and passed to the constructor.
    if (returnCreatedObject)
    {LOGMEIN("Inline.cpp] 4056\n");
        instr = IR::Instr::New(Js::OpCode::Ld_A, newObjInstr->GetDst(), createObjDst, callerFunc);
        instr->SetByteCodeOffset(nextByteCodeOffsetInstr);
        instr->GetDst()->SetIsJITOptimizedReg(true);
        instr->GetSrc1()->SetIsJITOptimizedReg(true);
        insertBeforeInstr->InsertBefore(instr);
    }
    else if (!skipNewScObj)
    {LOGMEIN("Inline.cpp] 4064\n");
        Assert(createObjDst != newObjInstr->GetDst());

        // Since we're not returning the default new object, the constructor must be returning something explicitly.  We don't
        // know at this point whether it's an object or not.  If the constructor is later inlined, the value type will be determined
        // from the flow in glob opt.  Otherwise, we'll need to emit an object check.
        newObjInstr->GetDst()->SetValueType(ValueType::Uninitialized);

        instr = IR::Instr::New(Js::OpCode::GetNewScObject, newObjInstr->GetDst(), newObjInstr->GetDst(), createObjDst, callerFunc);
        instr->SetByteCodeOffset(nextByteCodeOffsetInstr);
        instr->GetDst()->SetIsJITOptimizedReg(true);
        instr->GetSrc1()->SetIsJITOptimizedReg(true);
        insertBeforeInstr->InsertBefore(instr);
    }

    // Update the NewScObject cache, but only if we don't have a hard-coded constructor cache.  We only clone caches that
    // don't require update, and once updated a cache never requires an update again.
    if (constructorCache == nullptr)
    {LOGMEIN("Inline.cpp] 4082\n");
        instr = IR::Instr::New(Js::OpCode::UpdateNewScObjectCache, callerFunc);
        instr->SetSrc1(newObjInstr->GetSrc1()); // constructor function
        instr->SetSrc2(newObjInstr->GetDst());  // the new object
        instr->SetByteCodeOffset(nextByteCodeOffsetInstr);
        instr->GetSrc1()->SetIsJITOptimizedReg(true);
        instr->GetSrc2()->SetIsJITOptimizedReg(true);
        insertBeforeInstr->InsertBefore(instr);
    }

    if (createObjInstrOut != nullptr)
    {LOGMEIN("Inline.cpp] 4093\n");
        *createObjInstrOut = createObjInstr;
    }

    if (callCtorInstrOut != nullptr)
    {LOGMEIN("Inline.cpp] 4098\n");
        *callCtorInstrOut = newObjInstr;
    }

    return true;
}

void
Inline::InsertObjectCheck(IR::Instr *callInstr, IR::Instr* insertBeforeInstr, IR::Instr*bailOutIfNotObject)
{LOGMEIN("Inline.cpp] 4107\n");
    // Bailout if 'functionRegOpnd' is not an object.
    bailOutIfNotObject->SetSrc1(callInstr->GetSrc1()->AsRegOpnd());
    bailOutIfNotObject->SetByteCodeOffset(insertBeforeInstr);
    insertBeforeInstr->InsertBefore(bailOutIfNotObject);
}

void
Inline::InsertFunctionTypeIdCheck(IR::Instr *callInstr, IR::Instr* insertBeforeInstr, IR::Instr* bailOutIfNotJsFunction)
{LOGMEIN("Inline.cpp] 4116\n");
    // functionTypeRegOpnd = Ld functionRegOpnd->type
    IR::IndirOpnd *functionTypeIndirOpnd = IR::IndirOpnd::New(callInstr->GetSrc1()->AsRegOpnd(), Js::RecyclableObject::GetOffsetOfType(), TyMachPtr, callInstr->m_func);
    IR::RegOpnd *functionTypeRegOpnd = IR::RegOpnd::New(TyVar, this->topFunc);
    IR::Instr *instr = IR::Instr::New(Js::OpCode::Ld_A, functionTypeRegOpnd, functionTypeIndirOpnd, callInstr->m_func);
    if(instr->m_func->HasByteCodeOffset())
    {LOGMEIN("Inline.cpp] 4122\n");
        instr->SetByteCodeOffset(insertBeforeInstr);
    }
    insertBeforeInstr->InsertBefore(instr);

    CompileAssert(sizeof(Js::TypeId) == sizeof(int32));
    // if (functionTypeRegOpnd->typeId != TypeIds_Function) goto $noInlineLabel
    // BrNeq_I4 $noInlineLabel, functionTypeRegOpnd->typeId, TypeIds_Function
    IR::IndirOpnd *functionTypeIdIndirOpnd = IR::IndirOpnd::New(functionTypeRegOpnd, Js::Type::GetOffsetOfTypeId(), TyInt32, callInstr->m_func);
    IR::IntConstOpnd *typeIdFunctionConstOpnd = IR::IntConstOpnd::New(Js::TypeIds_Function, TyInt32, callInstr->m_func);
    bailOutIfNotJsFunction->SetSrc1(functionTypeIdIndirOpnd);
    bailOutIfNotJsFunction->SetSrc2(typeIdFunctionConstOpnd);
    insertBeforeInstr->InsertBefore(bailOutIfNotJsFunction);
}

void
Inline::InsertJsFunctionCheck(IR::Instr *callInstr, IR::Instr *insertBeforeInstr, IR::BailOutKind bailOutKind)
{LOGMEIN("Inline.cpp] 4139\n");
    // This function only inserts bailout for tagged int & TypeIds_Function.
    // As of now this is only used for polymorphic inlining.
    Assert(bailOutKind == IR::BailOutOnPolymorphicInlineFunction);

    Assert(insertBeforeInstr);
    Assert(insertBeforeInstr->m_func == callInstr->m_func);

    // bailOutIfNotFunction is primary bailout instruction
    IR::Instr* bailOutIfNotFunction = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, bailOutKind, insertBeforeInstr, callInstr->m_func);

    IR::Instr *bailOutIfNotObject = IR::BailOutInstr::New(Js::OpCode::BailOnNotObject, bailOutKind, bailOutIfNotFunction->GetBailOutInfo(),callInstr->m_func);
    InsertObjectCheck(callInstr, insertBeforeInstr, bailOutIfNotObject);

    InsertFunctionTypeIdCheck(callInstr, insertBeforeInstr, bailOutIfNotFunction);

}

void
Inline::InsertFunctionInfoCheck(IR::Instr *callInstr, IR::Instr *insertBeforeInstr, IR::Instr* bailoutInstr, const FunctionJITTimeInfo *funcInfo)
{LOGMEIN("Inline.cpp] 4159\n");
    // if (JavascriptFunction::FromVar(r1)->functionInfo != funcInfo) goto noInlineLabel
    // BrNeq_I4 noInlineLabel, r1->functionInfo, funcInfo
    IR::IndirOpnd* opndFuncInfo = IR::IndirOpnd::New(callInstr->GetSrc1()->AsRegOpnd(), Js::JavascriptFunction::GetOffsetOfFunctionInfo(), TyMachPtr, callInstr->m_func);
    IR::AddrOpnd* inlinedFuncInfo = IR::AddrOpnd::New(funcInfo->GetFunctionInfoAddr(), IR::AddrOpndKindDynamicFunctionInfo, callInstr->m_func);
    bailoutInstr->SetSrc1(opndFuncInfo);
    bailoutInstr->SetSrc2(inlinedFuncInfo);

    insertBeforeInstr->InsertBefore(bailoutInstr);
}

void
Inline::InsertFunctionObjectCheck(IR::Instr *callInstr, IR::Instr *insertBeforeInstr, IR::Instr *bailOutInstr, const FunctionJITTimeInfo *funcInfo)
{LOGMEIN("Inline.cpp] 4172\n");
     Js::BuiltinFunction index = Js::JavascriptLibrary::GetBuiltInForFuncInfo(funcInfo->GetFunctionInfoAddr(), this->topFunc->GetThreadContextInfo());
    AssertMsg(index < Js::BuiltinFunction::Count, "Invalid built-in index on a call target marked as built-in");

    bailOutInstr->SetSrc1(callInstr->GetSrc1()->AsRegOpnd());
    bailOutInstr->SetSrc2(IR::IntConstOpnd::New(index, TyInt32, callInstr->m_func));
    insertBeforeInstr->InsertBefore(bailOutInstr);
}

IR::Instr *
Inline::PrepareInsertionPoint(IR::Instr *callInstr, const FunctionJITTimeInfo *funcInfo, IR::Instr *insertBeforeInstr, IR::BailOutKind bailOutKind)
{LOGMEIN("Inline.cpp] 4183\n");
    Assert(insertBeforeInstr);
    Assert(insertBeforeInstr->m_func == callInstr->m_func);
    Assert(bailOutKind == IR::BailOutOnInlineFunction);

    // FunctionBody check is the primary bailout instruction, create it first
    IR::BailOutInstr* primaryBailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, bailOutKind, insertBeforeInstr, callInstr->m_func);

    // 1. Bailout if function object is not an object.
    IR::Instr *bailOutIfNotObject = IR::BailOutInstr::New(Js::OpCode::BailOnNotObject,
                                                          bailOutKind,
                                                          primaryBailOutInstr->GetBailOutInfo(),
                                                          callInstr->m_func);
    InsertObjectCheck(callInstr, insertBeforeInstr, bailOutIfNotObject);

    // 2. Bailout if function object is not a TypeId_Function
    IR::Instr* bailOutIfNotJsFunction = IR::BailOutInstr::New(Js::OpCode::BailOnNotEqual, bailOutKind, primaryBailOutInstr->GetBailOutInfo(), callInstr->m_func);
    InsertFunctionTypeIdCheck(callInstr, insertBeforeInstr, bailOutIfNotJsFunction);

    // 3. Bailout if function body doesn't match funcInfo
    InsertFunctionInfoCheck(callInstr, insertBeforeInstr, primaryBailOutInstr, funcInfo);

    return primaryBailOutInstr;
}

void
Inline::TryFixedMethodAndPrepareInsertionPoint(IR::Instr *callInstr, const FunctionJITTimeInfo * inlineeInfo, bool isPolymorphic, bool isBuiltIn, bool isCtor, bool isInlined)
{LOGMEIN("Inline.cpp] 4210\n");
    StackSym* originalCallTargetStackSym = callInstr->GetSrc1()->GetStackSym();
    bool originalCallTargetIsJITOpt = callInstr->GetSrc1()->GetIsJITOptimizedReg();

    bool safeThis = false;
    if (TryOptimizeCallInstrWithFixedMethod(callInstr, inlineeInfo, isPolymorphic, isBuiltIn, isCtor, isInlined, safeThis))
    {LOGMEIN("Inline.cpp] 4216\n");
        Assert(callInstr->m_opcode == Js::OpCode::CallIFixed);

        // If we optimized the call instruction for a fixed function, we must extend the function object's lifetime until after the last bailout before the call.
        IR::ByteCodeUsesInstr * useCallTargetInstr = IR::ByteCodeUsesInstr::New(callInstr);
        useCallTargetInstr->SetRemovedOpndSymbol(originalCallTargetIsJITOpt, originalCallTargetStackSym->m_id);
        callInstr->InsertBefore(useCallTargetInstr);
    }
    else
    {
        PrepareInsertionPoint(callInstr, inlineeInfo, callInstr);
    }
}

uint Inline::CountActuals(IR::Instr *callInstr)
{LOGMEIN("Inline.cpp] 4231\n");
    IR::Opnd *linkOpnd = callInstr->GetSrc2();
    uint actualCount = 0;
    if (linkOpnd->IsSymOpnd())
    {LOGMEIN("Inline.cpp] 4235\n");
        IR::Instr *argInstr;
        do
        {LOGMEIN("Inline.cpp] 4238\n");
            Assert(linkOpnd->IsSymOpnd());
            StackSym *sym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();
            Assert(sym->m_isSingleDef);
            Assert(sym->IsArgSlotSym());
            argInstr = sym->m_instrDef;
            ++actualCount;
            linkOpnd = argInstr->GetSrc2();
        }
        while (linkOpnd->IsSymOpnd());
    }
    return actualCount;
}

bool Inline::InlConstFoldArg(IR::Instr *instr, __in_ecount_opt(callerArgOutCount) IR::Instr *callerArgOuts[], Js::ArgSlot callerArgOutCount)
{LOGMEIN("Inline.cpp] 4253\n");
    Assert(instr->m_opcode == Js::OpCode::ArgOut_A);

    if (PHASE_OFF(Js::InlinerConstFoldPhase, instr->m_func->GetTopFunc()))
    {LOGMEIN("Inline.cpp] 4257\n");
        return false;
    }

    IR::Opnd *src1 = instr->GetSrc1();
    IntConstType value;

    if (!src1->IsRegOpnd())
    {LOGMEIN("Inline.cpp] 4265\n");
        return false;
    }

    StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;

    if (!sym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 4272\n");
        return false;
    }

    IR::Instr *instrDef = sym->GetInstrDef();
    if (!this->InlConstFold(instrDef, &value, callerArgOuts, callerArgOutCount))
    {LOGMEIN("Inline.cpp] 4278\n");
        return false;
    }
    return true;
}

bool Inline::InlConstFold(IR::Instr *instr, IntConstType *pValue, __in_ecount_opt(callerArgOutCount) IR::Instr *callerArgOuts[], Js::ArgSlot callerArgOutCount)
{LOGMEIN("Inline.cpp] 4285\n");
    IR::Opnd *src1 = instr->GetSrc1();

    if (!src1)
    {LOGMEIN("Inline.cpp] 4289\n");
        return false;
    }

    switch (src1->GetKind())
    {LOGMEIN("Inline.cpp] 4294\n");
    case IR::OpndKindReg:
        // Walk the tree below
        break;

    case IR::OpndKindIntConst:
        if (instr->m_opcode == Js::OpCode::LdC_A_I4)
        {LOGMEIN("Inline.cpp] 4301\n");
            // Found a constant
            *pValue = src1->AsIntConstOpnd()->GetValue();
            return true;
        }
        return false;

    case IR::OpndKindSym:
        if (callerArgOuts && instr->m_opcode == Js::OpCode::ArgIn_A)
        {LOGMEIN("Inline.cpp] 4310\n");
            // We have an ArgIn.  Walk the caller's ArgOut tree to see if a constant
            // is passed in to the inlinee.

            Assert(callerArgOuts && callerArgOutCount != (Js::ArgSlot) - 1);
            Assert(src1->AsSymOpnd()->m_sym->AsStackSym()->IsParamSlotSym());
            Js::ArgSlot paramSlot = src1->AsSymOpnd()->m_sym->AsStackSym()->GetParamSlotNum();
            if (paramSlot <= callerArgOutCount)
            {LOGMEIN("Inline.cpp] 4318\n");
                IR::Instr *argOut = callerArgOuts[paramSlot - 1];
                IR::Opnd *argOutSrc1 = argOut->GetSrc1();

                if (!argOutSrc1->IsRegOpnd())
                {LOGMEIN("Inline.cpp] 4323\n");
                    return false;
                }

                StackSym *sym = argOutSrc1->AsRegOpnd()->m_sym;

                if (!sym->IsSingleDef())
                {LOGMEIN("Inline.cpp] 4330\n");
                    return false;
                }

                IR::Instr *instrDef = sym->GetInstrDef();
                // Walk the caller
                return InlConstFold(instrDef, pValue, nullptr, (Js::ArgSlot) - 1);
            }
        }
        else if (src1->AsSymOpnd()->IsPropertySymOpnd())
        {LOGMEIN("Inline.cpp] 4340\n");
            // See if we have a LdFld of a fixed field.
            intptr_t var = TryOptimizeInstrWithFixedDataProperty(instr);
            if (!Js::TaggedInt::Is(var))
            {LOGMEIN("Inline.cpp] 4344\n");
                return false;
            }
            else
            {
                *pValue = Js::TaggedInt::ToInt32((Js::Var)var);
                return true;
            }
        }
        return false;

    default:
        return false;
    }

    // All that is left is RegOpnds
    Assert(src1->IsRegOpnd());

    StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;

    if (!sym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 4365\n");
        return false;
    }

    if (!src1 || !src1->IsRegOpnd() || !src1->AsRegOpnd()->m_sym->IsSingleDef())
    {LOGMEIN("Inline.cpp] 4370\n");
        return false;
    }

    IR::Opnd *src2 = instr->GetSrc2();

    if (src2)
    {LOGMEIN("Inline.cpp] 4377\n");
        if (!src2->IsRegOpnd() || !src2->AsRegOpnd()->m_sym->IsSingleDef())
        {LOGMEIN("Inline.cpp] 4379\n");
            return false;
        }
    }

    // See if src1 can be folded to a constant
    if (!InlConstFold(src1->AsRegOpnd()->m_sym->GetInstrDef(), pValue, callerArgOuts, callerArgOutCount))
    {LOGMEIN("Inline.cpp] 4386\n");
        return false;
    }
    IntConstType src1Constant = *pValue;

    // See if src2 (unless it is unary) can be folded to a constant
    if (src2 && !InlConstFold(src2->AsRegOpnd()->m_sym->GetInstrDef(), pValue, callerArgOuts, callerArgOutCount))
    {LOGMEIN("Inline.cpp] 4393\n");
        return false;
    }

    // Now let's try to constant fold the current instruction
    if (src2)
    {LOGMEIN("Inline.cpp] 4399\n");
        IntConstType src2Constant = *pValue;

        if (!instr->BinaryCalculator(src1Constant, src2Constant, pValue)
            || !Math::FitsInDWord(*pValue))
        {LOGMEIN("Inline.cpp] 4404\n");
            return false;
        }

        // Success
        StackSym *src1Sym = src1->AsRegOpnd()->m_sym;
        StackSym *src2Sym = src2->AsRegOpnd()->m_sym;

        if (src1Sym->HasByteCodeRegSlot() || src2Sym->HasByteCodeRegSlot())
        {LOGMEIN("Inline.cpp] 4413\n");
            IR::ByteCodeUsesInstr * byteCodeInstr = IR::ByteCodeUsesInstr::New(instr);
            if (src1Sym->HasByteCodeRegSlot())
            {LOGMEIN("Inline.cpp] 4416\n");
                byteCodeInstr->Set(src1);
            }
            if (src2Sym->HasByteCodeRegSlot())
            {LOGMEIN("Inline.cpp] 4420\n");
                byteCodeInstr->Set(src2);
            }
            instr->InsertBefore(byteCodeInstr);
        }

#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlinerConstFoldPhase, this->topFunc->GetSourceContextId(), this->topFunc->GetLocalFunctionId()))
        {LOGMEIN("Inline.cpp] 4428\n");
            Output::Print(_u("Constant folding to %d\n"), *pValue);
            instr->Dump();
        }
#endif
        instr->m_opcode = Js::OpCode::LdC_A_I4;
        instr->ReplaceSrc1(IR::IntConstOpnd::New(*pValue, TyInt32, instr->m_func));
        instr->GetDst()->AsRegOpnd()->m_sym->SetIsConst();
        instr->FreeSrc2();
    }
    else
    {
        if (!instr->UnaryCalculator(src1Constant, pValue)
            || !Math::FitsInDWord(*pValue))
        {LOGMEIN("Inline.cpp] 4442\n");
            // Skip over BytecodeArgOutCapture
            if (instr->m_opcode == Js::OpCode::BytecodeArgOutCapture)
            {LOGMEIN("Inline.cpp] 4445\n");
                return true;
            }
            return false;
        }
        // Success
        StackSym *src1Sym = src1->AsRegOpnd()->m_sym;
        if (src1Sym->HasByteCodeRegSlot())
        {LOGMEIN("Inline.cpp] 4453\n");
            IR::ByteCodeUsesInstr * byteCodeInstr = IR::ByteCodeUsesInstr::New(instr);
            byteCodeInstr->Set(src1);
            instr->InsertBefore(byteCodeInstr);
        }

#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::InlinerConstFoldPhase, this->topFunc->GetSourceContextId(), this->topFunc->GetLocalFunctionId()))
        {LOGMEIN("Inline.cpp] 4461\n");
            Output::Print(_u("Constant folding to %d\n"), *pValue);
            instr->Dump();
        }
#endif
        instr->m_opcode = Js::OpCode::LdC_A_I4;
        instr->ReplaceSrc1(IR::IntConstOpnd::New(*pValue, TyInt32, instr->m_func));
        instr->GetDst()->AsRegOpnd()->m_sym->SetIsConst();
    }

    return true;
}


Js::ArgSlot
Inline::MapActuals(IR::Instr *callInstr, __out_ecount(maxParamCount) IR::Instr *argOuts[],
    Js::ArgSlot formalCount,
    Func* inlinee,
    Js::ProfileId callSiteId,
    bool *stackArgsArgOutExpanded,
    IR::Instr *argOutsExtra[],
    Js::ArgSlot maxParamCount /* = Js::InlineeCallInfo::MaxInlineeArgoutCount*/)
{LOGMEIN("Inline.cpp] 4483\n");
    AnalysisAssert(formalCount <= maxParamCount);

    IR::Opnd *linkOpnd = callInstr->GetSrc2();
    Js::ArgSlot actualCount = 0;

    *stackArgsArgOutExpanded = false;
    uint inlineeFrameSlot = currentInlineeFrameSlot + (Js::Constants::InlineeMetaArgCount - 1);
    uint fixupArgoutCount = 0;

    if (inlinee)
    {LOGMEIN("Inline.cpp] 4494\n");
        bool hasArgumentsAccess = this->GetInlineeHasArgumentObject(inlinee);
        inlinee->SetHasUnoptimizedArgumentsAccess(hasArgumentsAccess);
    }

    if (linkOpnd->IsSymOpnd())
    {LOGMEIN("Inline.cpp] 4500\n");
        IR::Instr *argInstr;
        do
        {LOGMEIN("Inline.cpp] 4503\n");
            Assert(linkOpnd->IsSymOpnd());
            StackSym *sym = linkOpnd->AsSymOpnd()->m_sym->AsStackSym();

            Assert(sym->m_isSingleDef);
            Assert(sym->IsArgSlotSym());
            sym->m_isInlinedArgSlot = true;
            this->topFunc->SetArgOffset(sym, (inlineeFrameSlot + sym->GetArgSlotNum()) * MachPtr);
            argInstr = sym->m_instrDef;
            if (argInstr->m_opcode == Js::OpCode::ArgOut_A)
            {LOGMEIN("Inline.cpp] 4513\n");
                if(inlinee)
                {LOGMEIN("Inline.cpp] 4515\n");
                    if (!inlinee->GetHasUnoptimizedArgumentsAcccess())
                    {LOGMEIN("Inline.cpp] 4517\n");
                        // This allows us to markTemp the argOut source.
                        argInstr->m_opcode = Js::OpCode::ArgOut_A_Inline;
                    }
                }
                argInstr->GenerateBytecodeArgOutCapture();
            }

            // Expand
            //
            // s31 ArgOut_A s32
            // s30 ArgOut_A_FromStackArgs s31
            //
            // to
            //
            // s31 ArgOut_A(_Inline) s32
            // sXX ArgOut_A_FixupForStackArgs s31
            // .
            // .
            // s34 ArgOut_A_FixupForStackArgs sXX
            // s30 ArgOut_A_FromStackArgs s34

            if (inlinee && argInstr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs)
            {LOGMEIN("Inline.cpp] 4540\n");
                IR::Instr * argFixupInstr;
                for(uint currentFormal = 1; currentFormal < formalCount; currentFormal++)
                {LOGMEIN("Inline.cpp] 4543\n");
                    StackSym* newStackSym = StackSym::NewArgSlotSym(sym->GetArgSlotNum(), argInstr->m_func);
                    newStackSym->m_isInlinedArgSlot = true;

                    IR::SymOpnd * newLinkOpnd = IR::SymOpnd::New(newStackSym, sym->GetType(), argInstr->m_func);
                    IR::Opnd * undefined = IR::AddrOpnd::New(this->topFunc->GetScriptContextInfo()->GetUndefinedAddr(),
                                                IR::AddrOpndKindDynamicVar, this->topFunc, true);
                    undefined->SetValueType(ValueType::Undefined);

                    argFixupInstr = IR::Instr::New(Js::OpCode::ArgOut_A_FixupForStackArgs, newLinkOpnd, undefined, argInstr->GetSrc2(), argInstr->m_func);
                    argInstr->InsertBefore(argFixupInstr);
                    argInstr->ReplaceSrc2(argFixupInstr->GetDst());
                    sym->IncrementArgSlotNum();
                    argInstr->m_func->SetArgOffset(sym, (inlineeFrameSlot + sym->GetArgSlotNum()) * MachPtr);

                    argFixupInstr->GenerateArgOutSnapshot();
                    fixupArgoutCount++;
                }
                // Now that the arguments object has been expanded, we don't require the sym corresponding to it.
                IR::IntConstOpnd* callSiteIdOpnd = IR::IntConstOpnd::New(callSiteId, TyUint16, argInstr->m_func);
                argInstr->ReplaceSrc1(callSiteIdOpnd);
                // Don't count ArgOut_A_FromStackArgs as an actual, when it has been expanded
                --actualCount;
                *stackArgsArgOutExpanded = true;
            }
            ++actualCount;
            const Js::ArgSlot currentActual = sym->GetArgSlotNum() - 1;
            if (currentActual < formalCount)
            {LOGMEIN("Inline.cpp] 4571\n");
                Assert(currentActual < Js::InlineeCallInfo::MaxInlineeArgoutCount);
                argOuts[currentActual] = argInstr;
            }

            // We don't want to treat ArgOut_A_FromStackArgs as an actual arg.
            else if (argInstr->m_opcode != Js::OpCode::ArgOut_A_FromStackArgs)
            {LOGMEIN("Inline.cpp] 4578\n");
                Assert(currentActual <= Js::InlineeCallInfo::MaxInlineeArgoutCount);
                if(argOutsExtra)
                {LOGMEIN("Inline.cpp] 4581\n");
                    argOutsExtra[currentActual] = argInstr;
                    if (currentActual < maxParamCount)
                    {LOGMEIN("Inline.cpp] 4584\n");
                        __analysis_assume(currentActual < Js::InlineeCallInfo::MaxInlineeArgoutCount);
                        argOuts[currentActual] = nullptr;
                    }
                }
            }
            linkOpnd = argInstr->GetSrc2();
        }
        while (linkOpnd->IsSymOpnd());
#if DBG
        Assert(actualCount <= Js::InlineeCallInfo::MaxInlineeArgoutCount);
        for(Js::ArgSlot i = 0; i < min(actualCount, formalCount); ++i)
        {LOGMEIN("Inline.cpp] 4596\n");
#pragma prefast(suppress:6001)
            Assert(argOuts[i]);
        }
#endif
    }

    Assert(linkOpnd->IsRegOpnd());
    Assert(linkOpnd->AsRegOpnd()->m_sym->m_isSingleDef);
    Js::OpCode startCallOpCode = linkOpnd->AsRegOpnd()->m_sym->m_instrDef->m_opcode;
    Assert(startCallOpCode == Js::OpCode::StartCall);

    // Update the count in StartCall to reflect
    //  1. ArgOut_A_FromStackArgs is not an actual once it has been expanded.
    //  2. The expanded argouts (from ArgOut_A_FromStackArgs).
    //
    // Note that the StartCall will reflect the formal count only as of now; the actual count would be set during MapFormals
    if(*stackArgsArgOutExpanded)
    {LOGMEIN("Inline.cpp] 4614\n");
        // TODO: Is an underflow here intended, it triggers on test\inlining\OS_2733280.js
        IR::IntConstOpnd * countOpnd = linkOpnd->AsRegOpnd()->m_sym->m_instrDef->GetSrc1()->AsIntConstOpnd();
        int32 count = countOpnd->AsInt32();
        count += fixupArgoutCount - 1;
        countOpnd->SetValue(count);

        callInstr->m_func->EnsureCallSiteToArgumentsOffsetFixupMap();
        Assert(!(callInstr->m_func->callSiteToArgumentsOffsetFixupMap->ContainsKey(callSiteId)));
        callInstr->m_func->callSiteToArgumentsOffsetFixupMap->Add(callSiteId, fixupArgoutCount - 1);
    }

    Assert(linkOpnd->AsRegOpnd()->m_sym->m_instrDef->GetArgOutCount(/*getInterpreterArgOutCount*/ false) == actualCount);

    // Mark the StartCall's dst as an inlined arg slot as well so we know this is an inlined start call
    // and not adjust the stack height on x86
    linkOpnd->AsRegOpnd()->m_sym->m_isInlinedArgSlot = true;

    // Missing arguments...
    for (Js::ArgSlot i = actualCount; i < formalCount; i++)
    {LOGMEIN("Inline.cpp] 4634\n");
        argOuts[i] = nullptr;
    }

    // We may not know the exact number of actuals that "b" gets in a.b.apply just yet, since we have expanded the ArgOut_A_FromStackArgs based on the number of formals "b" accepts.
    // So, return the actualCount stored on the func if the ArgOut_A_FromStackArgs was expanded (and thus, the expanded argouts were accounted for in calculating the local actualCount)
    return *stackArgsArgOutExpanded ? callInstr->m_func->actualCount : actualCount;
}

void
Inline::MapFormals(Func *inlinee,
    __in_ecount(formalCount) IR::Instr *argOuts[],
    uint formalCount,
    uint actualCount,
    IR::RegOpnd *retOpnd,
    IR::Opnd * funcObjOpnd,
    const StackSym *symCallerThis,
    bool stackArgsArgOutExpanded,
    bool fixedFunctionSafeThis,
    IR::Instr *argOutsExtra[])
{LOGMEIN("Inline.cpp] 4654\n");
    IR::SymOpnd *formalOpnd;
    uint argIndex;
    uint formalCountForInlinee;
    IR::Instr * argInstr;
    IR::Opnd * linkOpnd;

    bool fUsesSafeThis = false;
    bool fUsesConstThis = false;
    StackSym *symThis = nullptr;
    StackSym *thisConstSym = nullptr;

    FOREACH_INSTR_EDITING(instr, instrNext, inlinee->m_headInstr)
    {LOGMEIN("Inline.cpp] 4667\n");
        switch (instr->m_opcode)
        {LOGMEIN("Inline.cpp] 4669\n");
        case Js::OpCode::ArgIn_Rest:
        {LOGMEIN("Inline.cpp] 4671\n");
            // We only currently support a statically known number of actuals.
            if (stackArgsArgOutExpanded)
            {LOGMEIN("Inline.cpp] 4674\n");
                break;
            }
            if (instr->m_func != inlinee)
            {LOGMEIN("Inline.cpp] 4678\n");
                // this can happen only when we are inlining a function which has inlined an apply call with the arguments object
                formalCount = instr->m_func->GetJITFunctionBody()->GetInParamsCount();
            }

            IR::Opnd *restDst = instr->GetDst();

            Assert(actualCount < 1 << 24 && formalCount < 1 << 24); // 24 bits for arg count (see CallInfo.h)
            int excess = actualCount - formalCount;

            if (excess < 0)
            {LOGMEIN("Inline.cpp] 4689\n");
                excess = 0;
            }

            // Set the type info about the destination so the array offsets get calculated properly.
            restDst->SetValueType(
                ValueType::GetObject(ObjectType::Array)
                .SetHasNoMissingValues(true)
                .SetArrayTypeId(Js::TypeIds_Array));
            restDst->SetValueTypeFixed();

            // Create the array and assign the elements.
            IR::Instr *newArrInstr = IR::Instr::New(Js::OpCode::NewScArray, restDst, IR::IntConstOpnd::New(excess, TyUint32, inlinee), inlinee);
            instr->InsertBefore(newArrInstr);

            for (uint i = formalCount; i < actualCount; ++i)
            {LOGMEIN("Inline.cpp] 4705\n");
                IR::IndirOpnd *arrayLocOpnd = IR::IndirOpnd::New(restDst->AsRegOpnd(), i - formalCount, TyVar, inlinee);
                IR::Instr *stElemInstr = IR::Instr::New(Js::OpCode::StElemC, arrayLocOpnd, argOutsExtra[i]->GetBytecodeArgOutCapture()->GetDst(), inlinee);
                instr->InsertBefore(stElemInstr);
            }

            instr->Remove();

            break;
        }

        case Js::OpCode::ArgIn_A:
            formalOpnd = instr->UnlinkSrc1()->AsSymOpnd();
            argIndex = formalOpnd->m_sym->AsStackSym()->GetParamSlotNum() - 1;
            if (argIndex >= formalCount)
            {LOGMEIN("Inline.cpp] 4720\n");
                Fatal();
            }
            formalOpnd->Free(this->topFunc);
            if (argOuts[argIndex])
            {LOGMEIN("Inline.cpp] 4725\n");
                IR::Instr *argOut = argOuts[argIndex];
                IR::Instr* instrDef;
                if (argOut->HasByteCodeArgOutCapture())
                {LOGMEIN("Inline.cpp] 4729\n");
                    instrDef = argOut->GetBytecodeArgOutCapture();
                }
                else
                {
                    Assert(argOut->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs);
                    instrDef = argOut->GetArgOutSnapshot();
                }

                instr->SetSrc1(instrDef->GetDst());
                instr->m_opcode = Js::OpCode::Ld_A;
                IR::Opnd* dst = instr->GetDst();
                IR::Opnd* src = instrDef->GetSrc1();

                if (argIndex == 0)
                {LOGMEIN("Inline.cpp] 4744\n");
                    // Look at the "this" argument source.
                    // If it's known to be a normal object (the caller has already guaranteed that, or
                    // it was defined by an instruction that produces normal objects), we'll omit CheckThis.
                    // If it's a constant value, we'll do the mapping at jit time and copy the final value.
                    if (src->IsRegOpnd())
                    {LOGMEIN("Inline.cpp] 4750\n");
                        symThis = dst->AsRegOpnd()->m_sym;

                        StackSym *symSrc = src->AsRegOpnd()->m_sym;
                        if (symSrc == symCallerThis ||
                            symSrc->m_isSafeThis ||
                            inlinee->IsInlinedConstructor())
                        {LOGMEIN("Inline.cpp] 4757\n");
                            fUsesSafeThis = true;
                        }
                        else if (symSrc->m_isSingleDef && symSrc->IsConst() && !symSrc->IsIntConst() && !symSrc->IsFloatConst())
                        {LOGMEIN("Inline.cpp] 4761\n");
                            thisConstSym = symSrc;
                            fUsesConstThis = true;
                        }
                        else if(fixedFunctionSafeThis)
                        {LOGMEIN("Inline.cpp] 4766\n");
                            // Note this need to come after we determined that this pointer is not const (undefined/null)
                            fUsesSafeThis = true;
                        }
                    }
                }
            }
            else
            {
                instr->SetSrc1(IR::AddrOpnd::New(this->topFunc->GetScriptContextInfo()->GetUndefinedAddr(),
                    IR::AddrOpndKindDynamicVar, this->topFunc, true));
                instr->GetSrc1()->SetValueType(ValueType::Undefined);
                instr->m_opcode = Js::OpCode::Ld_A;
            }
            break;

        case Js::OpCode::ArgOut_A_FromStackArgs:
            {LOGMEIN("Inline.cpp] 4783\n");
                linkOpnd = instr->GetSrc2();
                if(!linkOpnd->IsSymOpnd())
                {LOGMEIN("Inline.cpp] 4786\n");
                    break;
                }

                Assert(instr->GetSrc1()->IsIntConstOpnd());
                Js::ProfileId callSiteId = static_cast<Js::ProfileId>(instr->GetSrc1()->AsIntConstOpnd()->GetValue());

                argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                while(linkOpnd->IsSymOpnd())
                {LOGMEIN("Inline.cpp] 4795\n");
                    argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                    linkOpnd = argInstr->GetSrc2();
                }

                Assert(linkOpnd->IsRegOpnd());
                Js::OpCode startCallOpCode = linkOpnd->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef()->m_opcode;
                Assert(startCallOpCode == Js::OpCode::StartCall);
                IR::Instr* startCallForInlinee = linkOpnd->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef();
                formalCountForInlinee = startCallForInlinee->GetArgOutCount(false); // As of now, StartCall has the formal count

                if(actualCount < formalCountForInlinee)
                {
                    RemoveExtraFixupArgouts(instr, formalCountForInlinee - actualCount, callSiteId);
                    startCallForInlinee->GetSrc1()->AsIntConstOpnd()->DecrValue(formalCountForInlinee - actualCount); //account for the extra formals
                }

                linkOpnd = instr->GetSrc2();
                argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                argIndex = ((actualCount < formalCountForInlinee) ? actualCount : formalCountForInlinee) - 1;
                for ( ; argIndex > 0; argIndex--)
                {LOGMEIN("Inline.cpp] 4816\n");
                    if(argInstr->m_opcode != Js::OpCode::ArgOut_A_FixupForStackArgs)
                    {LOGMEIN("Inline.cpp] 4818\n");
                        break;
                    }

                    Assert(!argInstr->HasByteCodeArgOutCapture()); // ArgOut_A_FixupForStackArgs should not be restored on bailout, so we don't generate ByteCodeArgOutCapture for these argouts.

                    IR::Instr* currentArgOutInstr = nullptr;
                    if(argOuts[argIndex])
                    {LOGMEIN("Inline.cpp] 4826\n");
                        currentArgOutInstr = argOuts[argIndex];
                    }
                    else if(argOutsExtra && argOutsExtra[argIndex])
                    {LOGMEIN("Inline.cpp] 4830\n");
                        currentArgOutInstr = argOutsExtra[argIndex];
                    }
                    if(currentArgOutInstr)
                    {LOGMEIN("Inline.cpp] 4834\n");
                        Assert(currentArgOutInstr->m_opcode == Js::OpCode::ArgOut_A || currentArgOutInstr->m_opcode == Js::OpCode::ArgOut_A_Inline);

                        IR::Instr* bytecodeArgoutCapture = currentArgOutInstr->GetBytecodeArgOutCapture();
                        IR::Instr* formalArgOutUse = argInstr->GetArgOutSnapshot();

                        Assert(formalArgOutUse->m_opcode == Js::OpCode::Ld_A);
                        Assert((intptr_t)formalArgOutUse->GetSrc1()->AsAddrOpnd()->m_address == this->topFunc->GetScriptContextInfo()->GetUndefinedAddr());

                        formalArgOutUse->ReplaceSrc1(bytecodeArgoutCapture->GetDst());

                        linkOpnd = argInstr->GetSrc2();
                        argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                    }
                }

                if (formalCountForInlinee < actualCount)
                {
                    FixupExtraActualParams(instr, argOuts, argOutsExtra, formalCountForInlinee, actualCount, callSiteId);
                    startCallForInlinee->GetSrc1()->AsIntConstOpnd()->IncrValue(actualCount - formalCountForInlinee); //account for the extra actuals
                }

                break;
            }

        case Js::OpCode::InlineeStart:
            {LOGMEIN("Inline.cpp] 4860\n");
                linkOpnd = instr->GetSrc2();
                if(!linkOpnd->IsSymOpnd())
                {LOGMEIN("Inline.cpp] 4863\n");
                    break;
                }
                IR::Instr* stackArgsInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                if (stackArgsInstr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs)
                {LOGMEIN("Inline.cpp] 4868\n");
                    linkOpnd = stackArgsInstr->GetSrc2();
                    argInstr = linkOpnd->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
                    Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A_Inline || argInstr->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs || argInstr->m_opcode == Js::OpCode::ArgOut_A);
                    stackArgsInstr->Remove();
                    Assert(argInstr->GetDst()->IsSymOpnd());
                    instr->ReplaceSrc2(argInstr->GetDst());
                }
                break;
            }

        case Js::OpCode::LdEnv:
            if (instr->m_func == inlinee)
            {LOGMEIN("Inline.cpp] 4881\n");
                // Need to give the inlinee's function to load the environment
                if (funcObjOpnd->IsAddrOpnd())
                {LOGMEIN("Inline.cpp] 4884\n");
                    instr->m_opcode = Js::OpCode::Ld_A;
                    instr->SetSrc1(IR::AddrOpnd::New(((JITTimeFixedField*)funcObjOpnd->AsAddrOpnd()->m_metadata)->GetEnvironmentAddr(),
                        IR::AddrOpndKindDynamicFrameDisplay, instr->m_func));
                }
                else
                {
                    instr->SetSrc1(funcObjOpnd);
                }
            }
            else
            {
                Assert(instr->GetSrc1() != nullptr);
            }
            break;

        case Js::OpCode::LdNewTarget:
            if (instr->m_func == inlinee)
            {LOGMEIN("Inline.cpp] 4902\n");
                if (instr->m_func->IsInlinedConstructor())
                {LOGMEIN("Inline.cpp] 4904\n");
                    instr->SetSrc1(funcObjOpnd);
                }
                else
                {
                    instr->SetSrc1(IR::AddrOpnd::New(this->topFunc->GetScriptContextInfo()->GetUndefinedAddr(),
                        IR::AddrOpndKindDynamicVar, this->topFunc, true));
                    instr->GetSrc1()->SetValueType(ValueType::Undefined);
                }
                instr->m_opcode = Js::OpCode::Ld_A;
            }
            break;

        case Js::OpCode::ChkNewCallFlag:
            if (instr->m_func == inlinee)
            {LOGMEIN("Inline.cpp] 4919\n");
                if (instr->m_func->IsInlinedConstructor())
                {LOGMEIN("Inline.cpp] 4921\n");
                    instr->Remove();
                }
                else
                {
                    // InliningDecider::Inline should have decided not to inline this since we are going to end up throwing anyway
                    Assert(false);
                }
            }
            break;

        case Js::OpCode::LdHomeObj:
        case Js::OpCode::LdFuncObj:
            if (instr->m_func == inlinee)
            {LOGMEIN("Inline.cpp] 4935\n");
                instr->SetSrc1(funcObjOpnd);
            }
            else
            {
                Assert(instr->GetSrc1() != nullptr);
            }
            break;


        case Js::OpCode::LdThis:
        case Js::OpCode::StrictLdThis:
            // Optimization of LdThis may be possible.
            // Verify that this is a use of the "this" passed by the caller (not a nested function).
            if (instr->GetSrc1()->AsRegOpnd()->m_sym == symThis)
            {LOGMEIN("Inline.cpp] 4950\n");
                if (fUsesSafeThis)
                {LOGMEIN("Inline.cpp] 4952\n");
                    // No need for any "this" mapping.
                    instrNext = this->RemoveLdThis(instr);
                    break;
                }
                else if (fUsesConstThis)
                {LOGMEIN("Inline.cpp] 4958\n");
                    // "this" is a constant, so map it now.
                    // Don't bother mapping if it's not an object, though, since we'd have to create a
                    // boxed value at JIT time, and that case doesn't seem worth it.
                    Js::TypeId typeId = Js::TypeIds_Limit;
                    Js::Var localVar = thisConstSym->GetConstAddress(topFunc->IsOOPJIT());
                    if (localVar != nullptr)
                    {LOGMEIN("Inline.cpp] 4965\n");
                        typeId = Js::JavascriptOperators::GetTypeIdNoCheck(localVar);
                    }
                    else
                    {
                        Assert(JITManager::GetJITManager()->IsJITServer());
                        // with OOP JIT we may create const Opnds for library vars without materializing a JITRecyclableObject
                        IR::Opnd * thisConstOpnd = thisConstSym->GetConstOpnd();
                        if (thisConstOpnd->GetValueType().IsUndefined())
                        {LOGMEIN("Inline.cpp] 4974\n");
                            typeId = Js::TypeIds_Undefined;
                        }
                        else if (thisConstOpnd->GetValueType().IsNull())
                        {LOGMEIN("Inline.cpp] 4978\n");
                            typeId = Js::TypeIds_Null;
                        }
                    }
                    if (typeId != Js::TypeIds_Limit && (Js::JavascriptOperators::IsObjectType(typeId) || Js::JavascriptOperators::IsUndefinedOrNullType(typeId)))
                    {LOGMEIN("Inline.cpp] 4983\n");
                        auto scriptContext = inlinee->GetScriptContextInfo();
                        Js::Var thisConstVar;
                        if (instr->m_opcode == Js::OpCode::LdThis)
                        {LOGMEIN("Inline.cpp] 4987\n");
                            int moduleId = instr->GetSrc2()->AsIntConstOpnd()->AsInt32();
                            // TODO OOP JIT, create and use server copy of module roots
                            Assert(!topFunc->IsOOPJIT() || moduleId == 0);
                            thisConstVar = Js::JavascriptOperators::GetThisHelper(localVar, typeId, moduleId, scriptContext);
                            instr->FreeSrc2();
                        }
                        else if (typeId == Js::TypeIds_ActivationObject)
                        {LOGMEIN("Inline.cpp] 4995\n");
                            thisConstVar = (Js::Var)scriptContext->GetUndefinedAddr();
                        }
                        else
                        {
                            thisConstVar = thisConstSym->GetConstAddress();
                        }
                        Assert(thisConstVar != nullptr);
                        IR::Opnd *thisOpnd = IR::AddrOpnd::New((intptr_t)thisConstVar, IR::AddrOpndKindDynamicVar, inlinee, true);

                        instr->m_opcode = Js::OpCode::Ld_A;
                        instr->ReplaceSrc1(thisOpnd);
                        break;
                    }
                }
            }

            // Couldn't eliminate the execution-time "this" mapping. Try to change it to a check.
            instrNext = this->DoCheckThisOpt(instr);
            break;

        case Js::OpCode::Throw:
            instr->m_opcode = Js::OpCode::InlineThrow;
            instr->m_func->SetHasImplicitCallsOnSelfAndParents();
            break;

        case Js::OpCode::RuntimeTypeError:
            instr->m_opcode = Js::OpCode::InlineRuntimeTypeError;
            instr->m_func->SetHasImplicitCallsOnSelfAndParents();
            break;

        case Js::OpCode::RuntimeReferenceError:
            instr->m_opcode = Js::OpCode::InlineRuntimeReferenceError;
            instr->m_func->SetHasImplicitCallsOnSelfAndParents();
            break;

        case Js::OpCode::Ret:
            if (!retOpnd)
            {LOGMEIN("Inline.cpp] 5033\n");
                instr->Remove();
            }
            else
            {
                instr->m_opcode = Js::OpCode::Ld_A;
                instr->SetDst(retOpnd);
            }
            break;
        }
    } NEXT_INSTR_EDITING;
}

void
Inline::SetupInlineeFrame(Func *inlinee, IR::Instr *inlineeStart, Js::ArgSlot actualCount, IR::Opnd *functionObject)
{LOGMEIN("Inline.cpp] 5048\n");
    Js::ArgSlot argSlots[Js::Constants::InlineeMetaArgCount] = {
        actualCount + 1u, /* argc */
        actualCount + 2u, /* function object */
        actualCount + 3u  /* arguments object slot */
    };

    IR::Opnd *srcs[Js::Constants::InlineeMetaArgCount] = {
        IR::IntConstOpnd::New(actualCount, TyInt16, inlinee, true /*dontEncode*/),

        /*
         * Don't initialize this slot with the function object yet. In compat mode we evaluate
         * the target only after evaluating all arguments. Having this SymOpnd here ensures it gets
         * the correct slot in the frame. Lowerer fills this slot with the function object just
         * before entering the inlinee when we're sure we've evaluated the target in all modes.
         */
        nullptr,

        IR::AddrOpnd::NewNull(inlinee)
    };

    const IRType types[Js::Constants::InlineeMetaArgCount] = {
        TyMachReg,
        TyVar,
        TyMachReg
    };

    for (unsigned instrIndex = 0; instrIndex < Js::Constants::InlineeMetaArgCount; instrIndex++)
    {LOGMEIN("Inline.cpp] 5076\n");
        StackSym    *stackSym = inlinee->m_symTable->GetArgSlotSym(argSlots[instrIndex]);
        stackSym->m_isInlinedArgSlot = true;
        this->topFunc->SetArgOffset(stackSym, (currentInlineeFrameSlot + instrIndex) * MachPtr);
        IR::SymOpnd *symOpnd  = IR::SymOpnd::New(stackSym, 0, types[instrIndex], inlinee);

        IR::Instr   *instr    = IR::Instr::New(Js::OpCode::InlineeMetaArg, inlinee);
        instr->SetDst(symOpnd);
        if (srcs[instrIndex])
        {LOGMEIN("Inline.cpp] 5085\n");
            instr->SetSrc1(srcs[instrIndex]);
        }
        inlineeStart->InsertBefore(instr);

        if (instrIndex == 0)
        {LOGMEIN("Inline.cpp] 5091\n");
            inlinee->SetInlineeFrameStartSym(stackSym);
        }
    }
}

void
Inline::FixupExtraActualParams(IR::Instr * instr, IR::Instr *argOuts[], IR::Instr *argOutsExtra[], uint index, uint actualCount, Js::ProfileId callSiteId)
{LOGMEIN("Inline.cpp] 5099\n");
    Assert(instr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);

    int offsetFixup;
    Assert(instr->m_func->callSiteToArgumentsOffsetFixupMap->ContainsKey(callSiteId));
    instr->m_func->callSiteToArgumentsOffsetFixupMap->TryGetValue(callSiteId, &offsetFixup);

    StackSym *sym = instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
    while (index < actualCount)
    {LOGMEIN("Inline.cpp] 5108\n");
        IR::Instr* argOutToMapTo = argOuts[index] ? argOuts[index] : argOutsExtra[index];
        StackSym* newStackSym = StackSym::NewArgSlotSym(sym->GetArgSlotNum(), instr->m_func);
        newStackSym->m_isInlinedArgSlot = true;
        this->topFunc->SetArgOffset(newStackSym, sym->m_offset);
        sym->IncrementArgSlotNum();
        this->topFunc->SetArgOffset(sym, sym->m_offset + MachPtr);

        IR::SymOpnd * linkOpnd = IR::SymOpnd::New(newStackSym, sym->GetType(), instr->m_func);

        IR::Instr * extraActualParamInstr = IR::Instr::New(Js::OpCode::ArgOut_A_FixupForStackArgs, linkOpnd, argOutToMapTo->GetSrc1(), instr->GetSrc2(), instr->m_func);
        instr->InsertBefore(extraActualParamInstr);
        extraActualParamInstr->GenerateArgOutSnapshot();

        instr->m_func->callSiteToArgumentsOffsetFixupMap->Item(callSiteId, ++offsetFixup);

        instr->ReplaceSrc2(extraActualParamInstr->GetDst());

        index++;
    }
}

void
Inline::RemoveExtraFixupArgouts(IR::Instr* instr, uint argoutRemoveCount, Js::ProfileId callSiteId)
{LOGMEIN("Inline.cpp] 5132\n");
    Assert(instr->m_opcode == Js::OpCode::ArgOut_A_FromStackArgs);

    int offsetFixup;
    Assert(instr->m_func->callSiteToArgumentsOffsetFixupMap->ContainsKey(callSiteId));
    instr->m_func->callSiteToArgumentsOffsetFixupMap->TryGetValue(callSiteId, &offsetFixup);

    StackSym* argSym = instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
    IR::Instr* argInstr = instr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
    for(uint argIndex = 0; argIndex < argoutRemoveCount; argIndex++)
    {LOGMEIN("Inline.cpp] 5142\n");
        Assert(argInstr->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs);
        Assert(!argInstr->HasByteCodeArgOutCapture()); // ArgOut_A_FixupForStackArgs should not be restored on bailout, so we don't generate ByteCodeArgOutCapture for these argouts.

        instr->ReplaceSrc2(argInstr->GetSrc2());
        argSym->DecrementArgSlotNum();
        argSym->m_offset -= MachPtr;
        argSym->m_allocated = true;
        argInstr->Remove();

        instr->m_func->callSiteToArgumentsOffsetFixupMap->Item(callSiteId, --offsetFixup);

        argInstr = instr->GetSrc2()->AsSymOpnd()->m_sym->AsStackSym()->GetInstrDef();
    }
}

IR::Instr *
Inline::DoCheckThisOpt(IR::Instr * instr)
{LOGMEIN("Inline.cpp] 5160\n");
    IR::Instr * instrNext = instr->m_next;

    if (PHASE_OFF(Js::CheckThisPhase, instr->m_func->GetTopFunc()))
    {LOGMEIN("Inline.cpp] 5164\n");
        return instrNext;
    }

    if (!PHASE_FORCE(Js::CheckThisPhase, instr->m_func->GetTopFunc()))
    {LOGMEIN("Inline.cpp] 5169\n");
        if (!instr->m_func->HasProfileInfo())
        {LOGMEIN("Inline.cpp] 5171\n");
            return instrNext;
        }

        if (instr->m_func->GetReadOnlyProfileInfo()->GetThisInfo().thisType != Js::ThisType_Simple)
        {LOGMEIN("Inline.cpp] 5176\n");
            return instrNext;
        }

        if (instr->m_func->GetReadOnlyProfileInfo()->IsCheckThisDisabled())
        {LOGMEIN("Inline.cpp] 5181\n");
            return instrNext;
        }
    }

    // If the instr is an inlined LdThis, try to replace it with a CheckThis
    // that will bail out if a helper call is required to get the real "this" pointer.

    Assert(instr->m_opcode == Js::OpCode::LdThis || instr->m_opcode == Js::OpCode::StrictLdThis);
    Assert(instr->IsInlined());

    // Create the CheckThis. The target is the original offset, i.e., the LdThis still has to be executed.
    if(instr->m_opcode == Js::OpCode::LdThis)
    {LOGMEIN("Inline.cpp] 5194\n");
        instr->FreeSrc2();
    }
    IR::Instr *newInstr =
        IR::BailOutInstr::New( instr->m_opcode == Js::OpCode::LdThis ? Js::OpCode::CheckThis : Js::OpCode::StrictCheckThis, IR::BailOutCheckThis, instr, instr->m_func);
    // Just re-use the original src1 since the LdThis will usually be deleted.
    newInstr->SetSrc1(instr->GetSrc1());
    newInstr->SetByteCodeOffset(instr);
    instr->InsertBefore(newInstr);

    return this->RemoveLdThis(instr);
}

IR::Instr *
Inline::RemoveLdThis(IR::Instr *instr)
{LOGMEIN("Inline.cpp] 5209\n");
    // Replace the original instr with a copy, if needed.
    if (instr->GetDst()->IsEqual(instr->GetSrc1()))
    {LOGMEIN("Inline.cpp] 5212\n");
        // The copy would be a nop, so just delete.
        IR::Instr *instrNext = instr->m_next;
        instr->Remove();
        return instrNext;
    }
    else
    {
        instr->m_opcode = Js::OpCode::Ld_A;
        return instr;
    }
}

bool
Inline::IsArgumentsOpnd(IR::Opnd* opnd, SymID argumentsSymId)
{LOGMEIN("Inline.cpp] 5227\n");
    if (opnd->IsRegOpnd())
    {LOGMEIN("Inline.cpp] 5229\n");
        return argumentsSymId ==  opnd->AsRegOpnd()->m_sym->m_id;
    }
    else if (opnd->IsSymOpnd())
    {LOGMEIN("Inline.cpp] 5233\n");
        Sym *sym = opnd->AsSymOpnd()->m_sym;
        if (sym && sym->IsPropertySym())
        {LOGMEIN("Inline.cpp] 5236\n");
            PropertySym *propertySym = sym->AsPropertySym();
            return argumentsSymId == propertySym->m_stackSym->m_id;
        }
        return false;
    }
    else if (opnd->IsIndirOpnd())
    {LOGMEIN("Inline.cpp] 5243\n");
        IR::RegOpnd *indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
        IR::RegOpnd *baseOpnd = opnd->AsIndirOpnd()->GetBaseOpnd();
        return  (argumentsSymId == baseOpnd->m_sym->m_id) || (indexOpnd && indexOpnd->m_sym->m_id == argumentsSymId);
    }
    AssertMsg(false, "Unknown type");
    return false;
}


bool
Inline::HasArgumentsAccess(IR::Opnd *opnd, SymID argumentsSymId)
{LOGMEIN("Inline.cpp] 5255\n");
    // We should look at dst last to correctly handle cases where it's the same as one of the src operands.
    if (opnd)
    {LOGMEIN("Inline.cpp] 5258\n");
        if (opnd->IsRegOpnd() || opnd->IsSymOpnd() || opnd->IsIndirOpnd())
        {
            if (IsArgumentsOpnd(opnd, argumentsSymId))
            {LOGMEIN("Inline.cpp] 5262\n");
                return true;
            }
        }
    }
    return false;
}

bool
Inline::HasArgumentsAccess(IR::Instr * instr, SymID argumentsSymId)
{LOGMEIN("Inline.cpp] 5272\n");
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src1 = instr->GetSrc1();
    IR::Opnd* src2 = instr->GetSrc2();

    // Super conservative here, if we see the arguments or any of its alias being used in any
    // other opcode just don't do this optimization.
    if (HasArgumentsAccess(src1, argumentsSymId) || HasArgumentsAccess(src2, argumentsSymId))
    {LOGMEIN("Inline.cpp] 5280\n");
        return true;
    }

    if (dst)
    {LOGMEIN("Inline.cpp] 5285\n");
        // For dst no need to check for RegOpnd
        if (dst->IsSymOpnd() || dst->IsIndirOpnd())
        {
            if (IsArgumentsOpnd(dst, argumentsSymId))
            {LOGMEIN("Inline.cpp] 5290\n");
                return true;
            }
        }
    }

    return false;
}

bool
Inline::GetInlineeHasArgumentObject(Func * inlinee)
{LOGMEIN("Inline.cpp] 5301\n");
    if (!inlinee->GetJITFunctionBody()->UsesArgumentsObject())
    {LOGMEIN("Inline.cpp] 5303\n");
        // If inlinee has no arguments access return false
        return false;
    }

    // Inlinee has arguments access

    if (!inlinee->GetHasApplyTargetInlining())
    {LOGMEIN("Inline.cpp] 5311\n");
        // There is no apply target inlining (this.init.apply(this, arguments))
        // So arguments access continues to exist
        return true;
    }

    // Its possible there is no more arguments access after we inline apply target validate the same.
    // This sounds expensive, but we are only walking inlinee which has apply target inlining optimization enabled.
    // Also we walk only instruction in that inlinee and not nested inlinees. So it is not expensive.
    SymID argumentsSymId = 0;
    FOREACH_INSTR_IN_FUNC(instr, inlinee)
    {LOGMEIN("Inline.cpp] 5322\n");
        if (instr->m_func != inlinee)
        {LOGMEIN("Inline.cpp] 5324\n");
            // Skip nested inlinees
            continue;
        }

        if (instr->m_opcode == Js::OpCode::LdHeapArguments || instr->m_opcode == Js::OpCode::LdLetHeapArguments)
        {LOGMEIN("Inline.cpp] 5330\n");
            argumentsSymId = instr->GetDst()->AsRegOpnd()->m_sym->m_id;
        }
        else if (argumentsSymId != 0)
        {LOGMEIN("Inline.cpp] 5334\n");
            // Once we find the arguments object i.e. argumentsSymId is set
            // Make sure no one refers to it.
            switch (instr->m_opcode)
            {LOGMEIN("Inline.cpp] 5338\n");
                case Js::OpCode::InlineBuiltInStart:
                    {LOGMEIN("Inline.cpp] 5340\n");
                        IR::Opnd* builtInOpnd = instr->GetSrc1();
                        if (builtInOpnd->IsAddrOpnd())
                        {LOGMEIN("Inline.cpp] 5343\n");
                            Assert(builtInOpnd->AsAddrOpnd()->m_isFunction);

                            Js::BuiltinFunction builtinFunction = Js::JavascriptLibrary::GetBuiltInForFuncInfo(((JITTimeFixedField*)builtInOpnd->AsAddrOpnd()->m_metadata)->GetFuncInfoAddr(), this->topFunc->GetThreadContextInfo());
                            if (builtinFunction == Js::BuiltinFunction::JavascriptFunction_Apply)
                            {LOGMEIN("Inline.cpp] 5348\n");
                                this->SetIsInInlinedApplyCall(true);
                            }
                        }
                        else if (builtInOpnd->IsRegOpnd())
                        {LOGMEIN("Inline.cpp] 5353\n");
                            if (builtInOpnd->AsRegOpnd()->m_sym->m_builtInIndex == Js::BuiltinFunction::JavascriptFunction_Apply)
                            {LOGMEIN("Inline.cpp] 5355\n");
                                this->SetIsInInlinedApplyCall(true);
                            }
                        }
                        break;
                    }

                case Js::OpCode::InlineBuiltInEnd:
                    {LOGMEIN("Inline.cpp] 5363\n");
                        if(this->GetIsInInlinedApplyCall())
                        {LOGMEIN("Inline.cpp] 5365\n");
                            this->SetIsInInlinedApplyCall(false);
                        }
                        break;
                    }

                case Js::OpCode::BailOnNotStackArgs:
                case Js::OpCode::ArgOut_A_InlineBuiltIn:
                case Js::OpCode::BytecodeArgOutCapture:
                case Js::OpCode::BytecodeArgOutUse:
                    // These are part of arguments optimization and we are fine if they access stack args.
                    break;

                case Js::OpCode::ArgOut_A_FromStackArgs:
                    {LOGMEIN("Inline.cpp] 5379\n");
                        // If ArgOut_A_FromStackArgs is part of the call sequence for apply built-in inlining (as opposed to apply target inlining),
                        // then arguments access continues to exist.
                        if (this->GetIsInInlinedApplyCall() && HasArgumentsAccess(instr, argumentsSymId))
                        {LOGMEIN("Inline.cpp] 5383\n");
                            return true;
                        }
                        break;
                    }

                default:
                    {
                        if (HasArgumentsAccess(instr, argumentsSymId))
                        {LOGMEIN("Inline.cpp] 5392\n");
                            return true;
                        }
                    }
            }
        }
    }
    NEXT_INSTR_IN_FUNC;
    return false;
}

IR::Instr *
Inline::InlineSpread(IR::Instr *spreadCall)
{LOGMEIN("Inline.cpp] 5405\n");
    Assert(Lowerer::IsSpreadCall(spreadCall));

    if (spreadCall->m_func->GetJITFunctionBody()->IsInlineSpreadDisabled()
        || this->topFunc->GetJITFunctionBody()->IsInlineSpreadDisabled())
    {LOGMEIN("Inline.cpp] 5410\n");
        return spreadCall;
    }

    IR::Instr *spreadIndicesInstr = Lowerer::GetLdSpreadIndicesInstr(spreadCall);

    IR::Opnd *spreadIndicesOpnd = spreadIndicesInstr->GetSrc1();
    Assert(spreadIndicesOpnd->AsAddrOpnd()->GetAddrOpndKind() == IR::AddrOpndKindDynamicAuxBufferRef);

    Js::AuxArray<uint32>* spreadIndices = static_cast<Js::AuxArray<uint32>*>(spreadIndicesOpnd->AsAddrOpnd()->m_metadata);
    Assert(spreadIndices->count > 0);

    IR::Instr *argInstr = spreadIndicesInstr;

    IR::SymOpnd *argLinkOpnd = argInstr->GetSrc2()->AsSymOpnd();
    StackSym *argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
    argInstr = argLinkSym->m_instrDef;

    // We only support one spread argument for inlining.
    if (argLinkSym->GetArgSlotNum() > 2)
    {LOGMEIN("Inline.cpp] 5430\n");
        return spreadCall;
    }

    // We are now committed to inlining spread. Remove the LdSpreadIndices instr
    // and convert the spread and 'this' ArgOuts.
    spreadCall->ReplaceSrc2(argLinkOpnd);
    spreadIndicesInstr->Remove();

    // Insert the bailout before the array ArgOut
    IR::Opnd *arrayOpnd = argInstr->GetSrc1();
    argInstr->m_opcode = Js::OpCode::ArgOut_A_SpreadArg;
    IR::Instr *bailoutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotSpreadable, IR::BailOutOnInlineFunction, argInstr, argInstr->m_func);
    bailoutInstr->SetSrc1(arrayOpnd);
    argInstr->InsertBefore(bailoutInstr);

    argLinkOpnd = argInstr->GetSrc2()->AsSymOpnd();
    argLinkSym  = argLinkOpnd->m_sym->AsStackSym();
    argInstr = argLinkSym->m_instrDef;
    argInstr->m_opcode = Js::OpCode::ArgOut_A_Dynamic;

    IR::RegOpnd *startCallDstOpnd = argInstr->GetSrc2()->AsRegOpnd();
    argLinkSym = startCallDstOpnd->m_sym->AsStackSym();
    argInstr = argLinkSym->m_instrDef;
    Assert(argInstr->m_opcode == Js::OpCode::StartCall);

    spreadCall->m_opcode = Js::OpCode::CallIDynamicSpread;

    return spreadCall;
}

void
Inline::TryResetObjTypeSpecFldInfoOn(IR::PropertySymOpnd* propertySymOpnd)
{LOGMEIN("Inline.cpp] 5463\n");
    // if an objTypeSpecFldInfo was created just for the purpose of polymorphic inlining but didn't get used for the same (for some reason or the other), and the polymorphic cache it was created from, wasn't equivalent,
    // we should null out this info on the propertySymOpnd so that assumptions downstream around equivalent object type spec still hold.
    if (propertySymOpnd)
    {LOGMEIN("Inline.cpp] 5467\n");
        propertySymOpnd->TryResetObjTypeSpecFldInfo();
    }
}

void
Inline::TryDisableRuntimePolymorphicCacheOn(IR::PropertySymOpnd* propertySymOpnd)
{LOGMEIN("Inline.cpp] 5474\n");
    if (propertySymOpnd)
    {LOGMEIN("Inline.cpp] 5476\n");
        propertySymOpnd->TryDisableRuntimePolymorphicCache();
    }
}

IR::PropertySymOpnd*
Inline::GetMethodLdOpndForCallInstr(IR::Instr* callInstr)
{LOGMEIN("Inline.cpp] 5483\n");
    IR::Opnd* methodOpnd = callInstr->GetSrc1();
    if (methodOpnd->IsRegOpnd())
    {LOGMEIN("Inline.cpp] 5486\n");
        if (methodOpnd->AsRegOpnd()->m_sym->IsStackSym())
        {LOGMEIN("Inline.cpp] 5488\n");
            if (methodOpnd->AsRegOpnd()->m_sym->AsStackSym()->IsSingleDef())
            {LOGMEIN("Inline.cpp] 5490\n");
                IR::Instr* defInstr = methodOpnd->AsRegOpnd()->m_sym->AsStackSym()->GetInstrDef();
                if (defInstr->GetSrc1() && defInstr->GetSrc1()->IsSymOpnd() && defInstr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
                {LOGMEIN("Inline.cpp] 5493\n");
                    return defInstr->GetSrc1()->AsSymOpnd()->AsPropertySymOpnd();
                }
                return nullptr;
            }
            return nullptr;
        }
        return nullptr;
    }
    return nullptr;
}

#ifdef ENABLE_SIMDJS
// SIMD_JS
/*
Fixes the format of a SIMD load/store to match format expected by globOpt. Namely:
Load:
    dst = Simd128LdArr arr, index
    becomes
    dst = Simd128LdArr [arr, indx]

Store:
    t3 =    EA arr
    t2 =    EA index, t3
    t1 =    EA value, t2
            Simd128StArr t1
    becomes
    [arr, index] = Simd128StArr value

It also sets width in bytes of data to be loaded. Needed for bound check generation in GlobOpt.
*/
void
Inline::Simd128FixLoadStoreInstr(Js::BuiltinFunction builtInId, IR::Instr * callInstr)
{LOGMEIN("Inline.cpp] 5526\n");
    bool isStore = false;
    callInstr->dataWidth = 0;
    switch (builtInId)
    {LOGMEIN("Inline.cpp] 5530\n");
        case Js::BuiltinFunction::SIMDFloat32x4Lib_Store:
        case Js::BuiltinFunction::SIMDInt32x4Lib_Store:
            isStore = true;
            // fall through
        case Js::BuiltinFunction::SIMDFloat32x4Lib_Load:
        case Js::BuiltinFunction::SIMDInt32x4Lib_Load:
            callInstr->dataWidth = 16;
            break;

        case Js::BuiltinFunction::SIMDFloat32x4Lib_Store3:
        case Js::BuiltinFunction::SIMDInt32x4Lib_Store3:
            isStore = true;
            // fall through
        case Js::BuiltinFunction::SIMDFloat32x4Lib_Load3:
        case Js::BuiltinFunction::SIMDInt32x4Lib_Load3:
            callInstr->dataWidth = 12;
            break;

        case Js::BuiltinFunction::SIMDFloat32x4Lib_Store2:
        case Js::BuiltinFunction::SIMDInt32x4Lib_Store2:
            isStore = true;
            // fall through
        case Js::BuiltinFunction::SIMDFloat32x4Lib_Load2:
        case Js::BuiltinFunction::SIMDInt32x4Lib_Load2:
            callInstr->dataWidth = 8;
            break;

        case Js::BuiltinFunction::SIMDFloat32x4Lib_Store1:
        case Js::BuiltinFunction::SIMDInt32x4Lib_Store1:
            isStore = true;
            // fall through
        case Js::BuiltinFunction::SIMDFloat32x4Lib_Load1:
        case Js::BuiltinFunction::SIMDInt32x4Lib_Load1:
            callInstr->dataWidth = 4;
            break;
        default:
            // nothing to do
            return;
    }

    IR::IndirOpnd *indirOpnd;
    if (!isStore)
    {LOGMEIN("Inline.cpp] 5573\n");
        // load
        indirOpnd = IR::IndirOpnd::New(callInstr->GetSrc1()->AsRegOpnd(), callInstr->GetSrc2()->AsRegOpnd(), TyVar, callInstr->m_func);
        callInstr->ReplaceSrc1(indirOpnd);
        callInstr->FreeSrc2();
    }
    else
    {
        IR::Opnd *linkOpnd = callInstr->GetSrc1();
        IR::Instr *eaInstr1, *eaInstr2, *eaInstr3;
        IR::Opnd *value, *index, *arr;
        IR::Opnd *dst = callInstr->GetDst();

        eaInstr1 = linkOpnd->GetStackSym()->m_instrDef;
        value = eaInstr1->GetSrc1();
        linkOpnd = eaInstr1->GetSrc2();

        eaInstr2 = linkOpnd->GetStackSym()->m_instrDef;
        index = eaInstr2->GetSrc1();
        linkOpnd = eaInstr2->GetSrc2();

        eaInstr3 = linkOpnd->GetStackSym()->m_instrDef;
        Assert(!eaInstr3->GetSrc2()); // end of args list
        arr = eaInstr3->GetSrc1();

        indirOpnd = IR::IndirOpnd::New(arr->AsRegOpnd(), index->AsRegOpnd(), TyVar, callInstr->m_func);
        if (dst)
        {LOGMEIN("Inline.cpp] 5600\n");
            //Load value to be stored to dst. Store returns the value being stored.
            IR::Instr * ldInstr = IR::Instr::New(Js::OpCode::Ld_A, dst, value, callInstr->m_func);
            callInstr->InsertBefore(ldInstr);

            //Replace dst
            callInstr->ReplaceDst(indirOpnd);
        }
        else
        {
            callInstr->SetDst(indirOpnd);
        }

        callInstr->ReplaceSrc1(value);

        // remove ea instructions
        eaInstr1->Remove(); eaInstr2->Remove(); eaInstr3->Remove();

    }
}
#endif

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS)
// static
void Inline::TraceInlining(const FunctionJITTimeInfo *const inliner, const char16* inlineeName, const char16* inlineeFunctionIdandNumberString, uint inlineeByteCodeCount,
    const FunctionJITTimeInfo* topFunc, uint inlinedByteCodeCount, const FunctionJITTimeInfo *const inlinee, uint callSiteId, bool inLoopBody, uint builtIn)
{LOGMEIN("Inline.cpp] 5626\n");
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    char16 debugStringBuffer3[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
    if (inlineeName == nullptr)
    {LOGMEIN("Inline.cpp] 5631\n");

        int len = swprintf_s(debugStringBuffer3, MAX_FUNCTION_BODY_DEBUG_STRING_SIZE, _u("built In Id: %u"), builtIn);
        Assert(len > 14);
        inlineeName = debugStringBuffer3;
    }
    INLINE_TESTTRACE(_u("INLINING %s: Inlinee: %s (%s)\tSize: %d\tCaller: %s (%s)\tSize: %d\tInlineCount: %d\tRoot: %s (%s)\tSize: %d\tCallSiteId: %d\n"),
        inLoopBody ? _u("IN LOOP BODY") : _u(""),
        inlineeName, inlineeFunctionIdandNumberString, inlineeByteCodeCount,
        inliner->GetBody()->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer), inliner->GetBody()->GetByteCodeCount(), inlinedByteCodeCount,
        topFunc->GetBody()->GetDisplayName(),
        topFunc->GetDebugNumberSet(debugStringBuffer2), topFunc->GetBody()->GetByteCodeCount(),
        callSiteId
    );

    INLINE_TRACE(_u("INLINING %s: Inlinee: %s (%s)\tSize: %d\tCaller: %s (%s)\tSize: %d\tInlineCount: %d\tRoot: %s (%s)\tSize: %d\tCallSiteId: %d\n"),
        inLoopBody ? _u("IN LOOP BODY") : _u(""),
        inlineeName, inlineeFunctionIdandNumberString, inlineeByteCodeCount,
        inliner->GetBody()->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer), inliner->GetBody()->GetByteCodeCount(), inlinedByteCodeCount,
        topFunc->GetBody()->GetDisplayName(),
        topFunc->GetDebugNumberSet(debugStringBuffer2), topFunc->GetBody()->GetByteCodeCount(),
        callSiteId
    );

    // Now Trace inlining across files cases

    if (builtIn != -1)  // built-in functions
    {LOGMEIN("Inline.cpp] 5658\n");
        return;
    }

    Assert(inliner && inlinee);

    if (inliner->GetSourceContextId() != inlinee->GetSourceContextId())
    {LOGMEIN("Inline.cpp] 5665\n");
        INLINE_TESTTRACE(_u("INLINING_ACROSS_FILES: Inlinee: %s (%s)\tSize: %d\tCaller: %s (%s)\tSize: %d\tInlineCount: %d\tRoot: %s (%s)\tSize: %d\n"),
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inlinee->GetBody()->GetByteCodeCount(),
            inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2), inliner->GetBody()->GetByteCodeCount(), inlinedByteCodeCount,
            topFunc->GetBody()->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3), topFunc->GetBody()->GetByteCodeCount()
        );

        INLINE_TRACE(_u("INLINING_ACROSS_FILES: Inlinee: %s (%s)\tSize: %d\tCaller: %s (%s)\tSize: %d\tInlineCount: %d\tRoot: %s (%s)\tSize: %d\n"),
            inlinee->GetDisplayName(), inlinee->GetDebugNumberSet(debugStringBuffer), inlinee->GetBody()->GetByteCodeCount(),
            inliner->GetDisplayName(), inliner->GetDebugNumberSet(debugStringBuffer2), inliner->GetBody()->GetByteCodeCount(), inlinedByteCodeCount,
            topFunc->GetBody()->GetDisplayName(), topFunc->GetDebugNumberSet(debugStringBuffer3), topFunc->GetBody()->GetByteCodeCount()
        );
    }

}
#endif
