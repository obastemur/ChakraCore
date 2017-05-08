//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#include "SccLiveness.h"


// Build SCC liveness.  SCC stands for Strongly Connected Components.  It's a simple
// conservative algorithm which has the advantage of being O(N).  A simple forward walk
// of the IR looks at the first and last use of each symbols and creates the lifetimes.
// The code assumes the blocks are in R-DFO order to start with.  For loops, the lifetimes
// are simply extended to cover the whole loop.
//
// The disadvantages are:
//      o Separate lifetimes of a given symbol are not separated
//      o Very conservative in loops, which is where we'd like precise info...
//
// Single-def symbols do not have the first issue.  We also try to make up for number 2
// by not extending the lifetime of symbols if the first def and the last use are in
// same loop.
//
// The code builds a list of lifetimes sorted in start order.
// We actually build the list in reverse start order, and then reverse it.

void
SCCLiveness::Build()
{TRACE_IT(15150);
    // First, lets number each instruction to get an ordering.
    // Note that we assume the blocks are in RDFO.

    // NOTE: Currently the DoInterruptProbe pass will number the instructions. If it has,
    // then the numbering here is not necessary. But there should be no phase between the two
    // that can invalidate the numbering.
    if (!this->func->HasInstrNumber())
    {TRACE_IT(15151);
        this->func->NumberInstrs();
    }

    IR::LabelInstr *lastLabelInstr = nullptr;

    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {TRACE_IT(15152);
        IR::Opnd *dst, *src1, *src2;
        uint32 instrNum = instr->GetNumber();

        // End of loop?
        if (this->curLoop && instrNum >= this->curLoop->regAlloc.loopEnd)
        {TRACE_IT(15153);
            AssertMsg(this->loopNest > 0, "Loop nest is messed up");
            AssertMsg(instr->IsBranchInstr(), "Loop tail should be a branchInstr");
            AssertMsg(instr->AsBranchInstr()->IsLoopTail(this->func), "Loop tail not marked correctly");

            Loop *loop = this->curLoop;
            while (loop && loop->regAlloc.loopEnd == this->curLoop->regAlloc.loopEnd)
            {
                FOREACH_SLIST_ENTRY(Lifetime *, lifetime, loop->regAlloc.extendedLifetime)
                {TRACE_IT(15154);
                    if (loop->regAlloc.hasNonOpHelperCall)
                    {TRACE_IT(15155);
                        lifetime->isLiveAcrossUserCalls = true;
                    }
                    if (loop->regAlloc.hasCall)
                    {TRACE_IT(15156);
                        lifetime->isLiveAcrossCalls = true;
                    }
                    if (lifetime->end == loop->regAlloc.loopEnd)
                    {TRACE_IT(15157);
                        lifetime->totalOpHelperLengthByEnd = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(instr);
                    }
                }
                NEXT_SLIST_ENTRY;

                loop->regAlloc.helperLength = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(instr);
                Assert(!loop->parent || loop->parent && loop->parent->regAlloc.loopEnd >= loop->regAlloc.loopEnd);
                loop = loop->parent;
            }
            while (this->curLoop && instrNum >= this->curLoop->regAlloc.loopEnd)
            {TRACE_IT(15158);
                this->curLoop = this->curLoop->parent;
                this->loopNest--;
            }
        }

        if (instr->HasBailOutInfo())
        {TRACE_IT(15159);
            // At this point, the bailout should be lowered to a CALL to BailOut
#if DBG
            Assert(LowererMD::IsCall(instr));
            IR::Opnd * helperOpnd = nullptr;
            if (instr->GetSrc1()->IsHelperCallOpnd())
            {TRACE_IT(15160);
                helperOpnd = instr->GetSrc1();
            }
            else if (instr->GetSrc1()->AsRegOpnd()->m_sym)
            {TRACE_IT(15161);
                Assert(instr->GetSrc1()->AsRegOpnd()->m_sym->m_instrDef);
                helperOpnd = instr->GetSrc1()->AsRegOpnd()->m_sym->m_instrDef->GetSrc1();
            }
            Assert(!helperOpnd || BailOutInfo::IsBailOutHelper(helperOpnd->AsHelperCallOpnd()->m_fnHelper));
#endif
            ProcessBailOutUses(instr);
        }

        if (instr->m_opcode == Js::OpCode::InlineeEnd  && instr->m_func->m_hasInlineArgsOpt)
        {TRACE_IT(15162);
            instr->m_func->frameInfo->IterateSyms([=](StackSym* argSym)
            {
                this->ProcessStackSymUse(argSym, instr);
            });
        }

        // Process srcs
        src1 = instr->GetSrc1();
        if (src1)
        {TRACE_IT(15163);
            this->ProcessSrc(src1, instr);

            src2 = instr->GetSrc2();
            if (src2)
            {TRACE_IT(15164);
                this->ProcessSrc(src2, instr);
            }
        }

        // Keep track of the last call instruction number to find out whether a lifetime crosses a call
        // Do not count call to bailout which exits anyways
        if (LowererMD::IsCall(instr) && !instr->HasBailOutInfo())
        {TRACE_IT(15165);
            if (this->lastOpHelperLabel == nullptr)
            {TRACE_IT(15166);
                // Catch only user calls (non op helper calls)
                this->lastNonOpHelperCall = instr->GetNumber();
                if (this->curLoop)
                {TRACE_IT(15167);
                    this->curLoop->regAlloc.hasNonOpHelperCall = true;
                }
            }
            // Catch all calls
            this->lastCall = instr->GetNumber();
            if (this->curLoop)
            {TRACE_IT(15168);
                this->curLoop->regAlloc.hasCall = true;
            }
        }

        // Process dst
        dst = instr->GetDst();
        if (dst)
        {TRACE_IT(15169);
            this->ProcessDst(dst, instr);
        }


        if (instr->IsLabelInstr())
        {TRACE_IT(15170);
            IR::LabelInstr * const labelInstr = instr->AsLabelInstr();

            if (labelInstr->IsUnreferenced())
            {TRACE_IT(15171);
                // Unreferenced labels can potentially be removed. See if the label tells
                // us we're transitioning between a helper and non-helper block.
                if (labelInstr->isOpHelper == (this->lastOpHelperLabel != nullptr)
                    && lastLabelInstr && labelInstr->isOpHelper == lastLabelInstr->isOpHelper)
                {TRACE_IT(15172);
                    // No such transition. Remove the label.
                    Assert(!labelInstr->GetRegion() || labelInstr->GetRegion() == this->curRegion);
                    labelInstr->Remove();
                    continue;
                }
            }
            lastLabelInstr = labelInstr;

            Region * region = labelInstr->GetRegion();
            if (region != nullptr)
            {TRACE_IT(15173);
                if (this->curRegion && this->curRegion != region)
                {TRACE_IT(15174);
                    this->curRegion->SetEnd(labelInstr->m_prev);
                }
                if (region->GetStart() == nullptr)
                {TRACE_IT(15175);
                    region->SetStart(labelInstr);
                }
                region->SetEnd(nullptr);
                this->curRegion = region;
            }
            else
            {TRACE_IT(15176);
                labelInstr->SetRegion(this->curRegion);
            }

            // Look for start of loop
            if (labelInstr->m_isLoopTop)
            {TRACE_IT(15177);
                this->loopNest++;       // used in spill cost calculation.

                uint32 lastBranchNum = 0;
                IR::BranchInstr *lastBranchInstr = nullptr;

                FOREACH_SLISTCOUNTED_ENTRY(IR::BranchInstr *, ref, &labelInstr->labelRefs)
                {TRACE_IT(15178);
                    if (ref->GetNumber() > lastBranchNum)
                    {TRACE_IT(15179);
                        lastBranchInstr = ref;
                        lastBranchNum = lastBranchInstr->GetNumber();
                    }
                }
                NEXT_SLISTCOUNTED_ENTRY;

                AssertMsg(instrNum < lastBranchNum, "Didn't find back edge...");
                AssertMsg(lastBranchInstr->IsLoopTail(this->func), "Loop tail not marked properly");

                Loop * loop = labelInstr->GetLoop();
                loop->parent = this->curLoop;
                this->curLoop = loop;
                loop->regAlloc.loopStart = instrNum;
                loop->regAlloc.loopEnd = lastBranchNum;

                // Tail duplication can result in cases in which an outer loop lexically ends before the inner loop.
                // The register allocator could then thrash in the inner loop registers used for a live-on-back-edge
                // sym on the outer loop. To prevent this, we need to mark the end of the outer loop as the end of the
                // inner loop and update the lifetimes already extended in the outer loop in keeping with this change.
                for (Loop* parentLoop = loop->parent; parentLoop != nullptr; parentLoop = parentLoop->parent)
                {TRACE_IT(15180);
                    if (parentLoop->regAlloc.loopEnd < loop->regAlloc.loopEnd)
                    {
                        // We need to go over extended lifetimes in outer loops to update the lifetimes of symbols that might
                        // have had their lifetime extended to the outer loop end (which is before the current loop end) and
                        // may not have any uses in the current loop to extend their lifetimes to the current loop end.
                        FOREACH_SLIST_ENTRY(Lifetime *, lifetime, parentLoop->regAlloc.extendedLifetime)
                        {TRACE_IT(15181);
                            if (lifetime->end == parentLoop->regAlloc.loopEnd)
                            {TRACE_IT(15182);
                                lifetime->end = loop->regAlloc.loopEnd;
                            }
                        }
                        NEXT_SLIST_ENTRY;
                        parentLoop->regAlloc.loopEnd = loop->regAlloc.loopEnd;
                    }
                }
                loop->regAlloc.extendedLifetime = JitAnew(this->tempAlloc, SList<Lifetime *>, this->tempAlloc);
                loop->regAlloc.hasNonOpHelperCall = false;
                loop->regAlloc.hasCall = false;
                loop->regAlloc.hasAirLock = false;
            }

            // track whether we are in a helper block or not
            if (this->lastOpHelperLabel != nullptr)
            {TRACE_IT(15183);
                this->EndOpHelper(labelInstr);
            }
            if (labelInstr->isOpHelper && !PHASE_OFF(Js::OpHelperRegOptPhase, this->func))
            {TRACE_IT(15184);
                this->lastOpHelperLabel = labelInstr;
            }
        }
        else if (instr->IsBranchInstr() && !instr->AsBranchInstr()->IsMultiBranch())
        {TRACE_IT(15185);
            IR::LabelInstr * branchTarget = instr->AsBranchInstr()->GetTarget();
            Js::OpCode brOpcode = instr->m_opcode;
            if (branchTarget->GetRegion() == nullptr && this->func->HasTry())
            {TRACE_IT(15186);
                Assert(brOpcode != Js::OpCode::Leave && brOpcode != Js::OpCode::TryCatch && brOpcode != Js::OpCode::TryFinally);
                branchTarget->SetRegion(this->curRegion);
            }
        }
        if (this->lastOpHelperLabel != nullptr && instr->IsBranchInstr())
        {TRACE_IT(15187);
            IR::LabelInstr *targetLabel = instr->AsBranchInstr()->GetTarget();

            if (targetLabel->isOpHelper && instr->AsBranchInstr()->IsConditional())
            {TRACE_IT(15188);
                // If we have:
                //    L1: [helper]
                //           CMP
                //           JCC  helperLabel
                //           code
                // Insert a helper label before 'code' to mark this is also helper code.
                IR::Instr *branchInstrNext = instr->GetNextRealInstrOrLabel();
                if (!branchInstrNext->IsLabelInstr())
                {TRACE_IT(15189);
                    instrNext = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func, true);
                    instr->InsertAfter(instrNext);
                    instrNext->CopyNumber(instrNext->m_next);
                }
            }
            this->EndOpHelper(instr);
        }

    }NEXT_INSTR_IN_FUNC_EDITING;

    if (this->func->HasTry())
    {TRACE_IT(15190);
#if DBG
        FOREACH_INSTR_IN_FUNC(instr, this->func)
        {TRACE_IT(15191);
            if (instr->IsLabelInstr())
            {TRACE_IT(15192);
                Assert(instr->AsLabelInstr()->GetRegion() != nullptr);
            }
        }
        NEXT_INSTR_IN_FUNC
#endif
        AssertMsg(this->curRegion, "Function with try but no regions?");
        AssertMsg(this->curRegion->GetStart() && !this->curRegion->GetEnd(), "Current region not active?");

        // Check for lifetimes that have been extended such that they now span multiple regions.
        this->curRegion->SetEnd(this->func->m_exitInstr);
        if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
        {
            FOREACH_SLIST_ENTRY(Lifetime *, lifetime, &this->lifetimeList)
            {TRACE_IT(15193);
                if (lifetime->dontAllocate)
                {TRACE_IT(15194);
                    continue;
                }
                if (lifetime->start < lifetime->region->GetStart()->GetNumber() ||
                    lifetime->end > lifetime->region->GetEnd()->GetNumber())
                {TRACE_IT(15195);
                    lifetime->dontAllocate = true;
                }
            }
            NEXT_SLIST_ENTRY;
        }
    }

    AssertMsg(this->loopNest == 0, "LoopNest is messed up");

    // The list is built in reverse order.  Let's flip it in increasing start order.

    this->lifetimeList.Reverse();
    this->opHelperBlockList.Reverse();

#if DBG_DUMP
    if (PHASE_DUMP(Js::LivenessPhase, this->func))
    {TRACE_IT(15196);
        this->Dump();
    }
#endif
}

void
SCCLiveness::EndOpHelper(IR::Instr * instr)
{TRACE_IT(15197);
    Assert(this->lastOpHelperLabel != nullptr);

    OpHelperBlock * opHelperBlock = this->opHelperBlockList.PrependNode(this->tempAlloc);
    Assert(opHelperBlock != nullptr);
    opHelperBlock->opHelperLabel = this->lastOpHelperLabel;
    opHelperBlock->opHelperEndInstr = instr;

    this->totalOpHelperFullVisitedLength += opHelperBlock->Length();
    this->lastOpHelperLabel = nullptr;
}

// SCCLiveness::ProcessSrc
void
SCCLiveness::ProcessSrc(IR::Opnd *src, IR::Instr *instr)
{TRACE_IT(15198);
    if (src->IsRegOpnd())
    {TRACE_IT(15199);
        this->ProcessRegUse(src->AsRegOpnd(), instr);
    }
    else if (src->IsIndirOpnd())
    {TRACE_IT(15200);
        IR::IndirOpnd *indirOpnd = src->AsIndirOpnd();

        AssertMsg(indirOpnd->GetBaseOpnd(), "Indir should have a base...");

        if (!this->FoldIndir(instr, indirOpnd))
        {TRACE_IT(15201);
            this->ProcessRegUse(indirOpnd->GetBaseOpnd(), instr);

            if (indirOpnd->GetIndexOpnd())
            {TRACE_IT(15202);
                this->ProcessRegUse(indirOpnd->GetIndexOpnd(), instr);
            }
        }
    }
    else if (!this->lastCall && src->IsSymOpnd() && src->AsSymOpnd()->m_sym->AsStackSym()->IsParamSlotSym())
    {TRACE_IT(15203);
        IR::SymOpnd *symOpnd = src->AsSymOpnd();
        RegNum reg = LinearScanMD::GetParamReg(symOpnd, this->func);

        if (reg != RegNOREG && PHASE_ON(Js::RegParamsPhase, this->func))
        {TRACE_IT(15204);
            StackSym *stackSym = symOpnd->m_sym->AsStackSym();
            Lifetime *lifetime = stackSym->scratch.linearScan.lifetime;

            if (lifetime == nullptr)
            {TRACE_IT(15205);
                lifetime = this->InsertLifetime(stackSym, reg, this->func->m_headInstr->m_next);
                lifetime->region = this->curRegion;
                lifetime->isFloat = symOpnd->IsFloat();
                lifetime->isSimd128F4 = symOpnd->IsSimd128F4();
                lifetime->isSimd128I4 = symOpnd->IsSimd128I4();
                lifetime->isSimd128I8 = symOpnd->IsSimd128I8();
                lifetime->isSimd128I16 = symOpnd->IsSimd128I16();
                lifetime->isSimd128U4 = symOpnd->IsSimd128U4();
                lifetime->isSimd128U8 = symOpnd->IsSimd128U8();
                lifetime->isSimd128U16 = symOpnd->IsSimd128U16();
                lifetime->isSimd128B4 = symOpnd->IsSimd128B4();
                lifetime->isSimd128B8 = symOpnd->IsSimd128B8();
                lifetime->isSimd128B16 = symOpnd->IsSimd128B16();
                lifetime->isSimd128D2 = symOpnd->IsSimd128D2();
            }

            IR::RegOpnd * newRegOpnd = IR::RegOpnd::New(stackSym, reg, symOpnd->GetType(), this->func);
            instr->ReplaceSrc(symOpnd, newRegOpnd);
            this->ProcessRegUse(newRegOpnd, instr);
        }
    }
}

// SCCLiveness::ProcessDst
void
SCCLiveness::ProcessDst(IR::Opnd *dst, IR::Instr *instr)
{TRACE_IT(15206);
    if (dst->IsIndirOpnd())
    {TRACE_IT(15207);
        // Indir regs are really uses

        IR::IndirOpnd *indirOpnd = dst->AsIndirOpnd();

        AssertMsg(indirOpnd->GetBaseOpnd(), "Indir should have a base...");

        if (!this->FoldIndir(instr, indirOpnd))
        {TRACE_IT(15208);
            this->ProcessRegUse(indirOpnd->GetBaseOpnd(), instr);

            if (indirOpnd->GetIndexOpnd())
            {TRACE_IT(15209);
                this->ProcessRegUse(indirOpnd->GetIndexOpnd(), instr);
            }
        }
    }
#if defined(_M_X64) || defined(_M_IX86)
    else if (instr->m_opcode == Js::OpCode::SHUFPS || instr->m_opcode == Js::OpCode::SHUFPD)
    {TRACE_IT(15210);
        // dst is the first src, make sure it gets the same live reg
        this->ProcessRegUse(dst->AsRegOpnd(), instr);
    }
#endif
    else if (dst->IsRegOpnd())
    {TRACE_IT(15211);
        this->ProcessRegDef(dst->AsRegOpnd(), instr);
    }
}

void
SCCLiveness::ProcessBailOutUses(IR::Instr * instr)
{TRACE_IT(15212);
    BailOutInfo * bailOutInfo = instr->GetBailOutInfo();
    FOREACH_BITSET_IN_SPARSEBV(id, bailOutInfo->byteCodeUpwardExposedUsed)
    {TRACE_IT(15213);
        StackSym * stackSym = this->func->m_symTable->FindStackSym(id);
        Assert(stackSym != nullptr);
        ProcessStackSymUse(stackSym, instr);
    }
    NEXT_BITSET_IN_SPARSEBV;

    FOREACH_SLISTBASE_ENTRY(CopyPropSyms, copyPropSyms, &bailOutInfo->usedCapturedValues.copyPropSyms)
    {TRACE_IT(15214);
        ProcessStackSymUse(copyPropSyms.Value(), instr);
    }
    NEXT_SLISTBASE_ENTRY;


    bailOutInfo->IterateArgOutSyms([=] (uint, uint, StackSym* sym) {
        if(!sym->IsArgSlotSym() && sym->m_isBailOutReferenced)
        {
            ProcessStackSymUse(sym, instr);
        }
    });

    if(bailOutInfo->branchConditionOpnd)
    {TRACE_IT(15215);
        ProcessSrc(bailOutInfo->branchConditionOpnd, instr);
    }

    // BailOnNoProfile might have caused the deletion of a cloned InlineeEnd. As a result, argument
    // lifetimes wouldn't have been extended beyond the bailout point (InlineeEnd extends the lifetimes)
    // Extend argument lifetimes up to the bail out point to allow LinearScan::SpillInlineeArgs to spill
    // inlinee args.
    if ((instr->GetBailOutKind() == IR::BailOutOnNoProfile) && !instr->m_func->IsTopFunc())
    {TRACE_IT(15216);
        Func * inlinee = instr->m_func;
        while (!inlinee->IsTopFunc())
        {TRACE_IT(15217);
            if (inlinee->m_hasInlineArgsOpt && inlinee->frameInfo->isRecorded)
            {TRACE_IT(15218);
                inlinee->frameInfo->IterateSyms([=](StackSym* argSym)
                {
                    this->ProcessStackSymUse(argSym, instr);
                });
                inlinee = inlinee->GetParentFunc();
            }
            else
            {TRACE_IT(15219);
                // if an inlinee's arguments haven't been optimized away, it's ancestors' shouldn't have been too.
                break;
            }
        }
    }
}

void
SCCLiveness::ProcessStackSymUse(StackSym * stackSym, IR::Instr * instr, int usageSize)
{TRACE_IT(15220);
    Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;

    if (lifetime == nullptr)
    {TRACE_IT(15221);
#if DBG
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(_u("Function: %s (%s)       "), this->func->GetJITFunctionBody()->GetDisplayName(), this->func->GetDebugNumberSet(debugStringBuffer));
        Output::Print(_u("Reg: "));
        stackSym->Dump();
        Output::Print(_u("\n"));
        Output::Flush();
#endif
        AnalysisAssertMsg(UNREACHED, "Uninitialized reg?");
    }
    else
    {TRACE_IT(15222);
        if (lifetime->region != this->curRegion && !this->func->DoOptimizeTryCatch())
        {TRACE_IT(15223);
            lifetime->dontAllocate = true;
        }

        ExtendLifetime(lifetime, instr);
    }
    lifetime->AddToUseCount(LinearScan::GetUseSpillCost(this->loopNest, (this->lastOpHelperLabel != nullptr)), this->curLoop, this->func);
    if (lifetime->start < this->lastCall)
    {TRACE_IT(15224);
        lifetime->isLiveAcrossCalls = true;
    }
    if (lifetime->start < this->lastNonOpHelperCall)
    {TRACE_IT(15225);
        lifetime->isLiveAcrossUserCalls = true;
    }
    lifetime->isDeadStore = false;

    lifetime->intUsageBv.Set(usageSize);
}

// SCCLiveness::ProcessRegUse
void
SCCLiveness::ProcessRegUse(IR::RegOpnd *regUse, IR::Instr *instr)
{TRACE_IT(15226);
    StackSym * stackSym = regUse->m_sym;

    if (stackSym == nullptr)
    {TRACE_IT(15227);
        return;
    }

    ProcessStackSymUse(stackSym, instr, TySize[regUse->GetType()]);

}

// SCCLiveness::ProcessRegDef
void
SCCLiveness::ProcessRegDef(IR::RegOpnd *regDef, IR::Instr *instr)
{TRACE_IT(15228);
    StackSym * stackSym = regDef->m_sym;

    // PhysReg
    if (stackSym == nullptr || regDef->GetReg() != RegNOREG)
    {TRACE_IT(15229);
        IR::Opnd *src = instr->GetSrc1();

        // If this symbol is assigned to a physical register, let's tell the register
        // allocator to prefer assigning that register to the lifetime.
        //
        // Note: this only pays off if this is the last-use of the symbol, but
        // unfortunately we don't have a way to tell that currently...
        if (LowererMD::IsAssign(instr) && src->IsRegOpnd() && src->AsRegOpnd()->m_sym)
        {TRACE_IT(15230);
            StackSym *srcSym = src->AsRegOpnd()->m_sym;

            srcSym->scratch.linearScan.lifetime->regPreference.Set(regDef->GetReg());
        }

        // This physreg doesn't have a lifetime, just return.
        if (stackSym == nullptr)
        {TRACE_IT(15231);
            return;
        }
    }

    // Arg slot sym can be in a RegOpnd for param passed via registers
    // Skip creating a lifetime for those.
    if (stackSym->IsArgSlotSym())
    {TRACE_IT(15232);
        return;
    }

    // We'll extend the lifetime only if there are uses in a different loop region
    // from one of the defs.

    Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;

    if (lifetime == nullptr)
    {TRACE_IT(15233);
        lifetime = this->InsertLifetime(stackSym, regDef->GetReg(), instr);
        lifetime->region = this->curRegion;
        lifetime->isFloat = regDef->IsFloat();
        lifetime->isSimd128F4   = regDef->IsSimd128F4();
        lifetime->isSimd128I4   = regDef->IsSimd128I4 ();
        lifetime->isSimd128I8   = regDef->IsSimd128I8 ();
        lifetime->isSimd128I16  = regDef->IsSimd128I16();
        lifetime->isSimd128U4   = regDef->IsSimd128U4 ();
        lifetime->isSimd128U8   = regDef->IsSimd128U8 ();
        lifetime->isSimd128U16  = regDef->IsSimd128U16();
        lifetime->isSimd128B4 = regDef->IsSimd128B4();
        lifetime->isSimd128B8 = regDef->IsSimd128B8();
        lifetime->isSimd128B16 = regDef->IsSimd128B16();
        lifetime->isSimd128D2   = regDef->IsSimd128D2();
    }
    else
    {TRACE_IT(15234);
        AssertMsg(lifetime->start <= instr->GetNumber(), "Lifetime start not set correctly");

        ExtendLifetime(lifetime, instr);

        if (lifetime->region != this->curRegion && !this->func->DoOptimizeTryCatch())
        {TRACE_IT(15235);
            lifetime->dontAllocate = true;
        }
    }
    lifetime->AddToUseCount(LinearScan::GetUseSpillCost(this->loopNest, (this->lastOpHelperLabel != nullptr)), this->curLoop, this->func);
    lifetime->intUsageBv.Set(TySize[regDef->GetType()]);
}

// SCCLiveness::ExtendLifetime
//      Manages extend lifetimes to the end of loops if the corresponding symbol
//      is live on the back edge of the loop
void
SCCLiveness::ExtendLifetime(Lifetime *lifetime, IR::Instr *instr)
{TRACE_IT(15236);
    AssertMsg(lifetime != nullptr, "Lifetime not provided");
    AssertMsg(lifetime->sym != nullptr, "Lifetime has no symbol");
    Assert(this->extendedLifetimesLoopList->Empty());

    // Find the loop that we need to extend the lifetime to
    StackSym * sym = lifetime->sym;
    Loop * loop = this->curLoop;
    uint32 extendedLifetimeStart = lifetime->start;
    uint32 extendedLifetimeEnd = lifetime->end;
    bool isLiveOnBackEdge = false;
    bool loopAddedToList = false;

    while (loop)
    {TRACE_IT(15237);
        if (loop->regAlloc.liveOnBackEdgeSyms->Test(sym->m_id))
        {TRACE_IT(15238);
            isLiveOnBackEdge = true;
            if (loop->regAlloc.loopStart < extendedLifetimeStart)
            {TRACE_IT(15239);
                extendedLifetimeStart = loop->regAlloc.loopStart;
                this->extendedLifetimesLoopList->Prepend(this->tempAlloc, loop);
                loopAddedToList = true;
            }
            if (loop->regAlloc.loopEnd > extendedLifetimeEnd)
            {TRACE_IT(15240);
                extendedLifetimeEnd = loop->regAlloc.loopEnd;
                if (!loopAddedToList)
                {TRACE_IT(15241);
                    this->extendedLifetimesLoopList->Prepend(this->tempAlloc, loop);
                }
            }
        }
        loop = loop->parent;
        loopAddedToList = false;
    }

    if (!isLiveOnBackEdge)
    {TRACE_IT(15242);
        // Don't extend lifetime to loop boundary if the use are not live on back edge
        // Note: the above loop doesn't detect a reg that is live on an outer back edge
        // but not an inner one, so we can't assume here that the lifetime hasn't been extended
        // past the current instruction.
        if (lifetime->end < instr->GetNumber())
        {TRACE_IT(15243);
            lifetime->end = instr->GetNumber();
            lifetime->totalOpHelperLengthByEnd = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(instr);
        }
    }
    else
    {TRACE_IT(15244);
        // extend lifetime to the outer most loop boundary that have the symbol live on back edge.
        bool isLifetimeExtended = false;
        if (lifetime->start > extendedLifetimeStart)
        {TRACE_IT(15245);
            isLifetimeExtended = true;
            lifetime->start = extendedLifetimeStart;
        }

        if (lifetime->end < extendedLifetimeEnd)
        {TRACE_IT(15246);
            isLifetimeExtended = true;
            lifetime->end = extendedLifetimeEnd;
            // The total op helper length by the end of this lifetime will be updated once we reach the loop tail
        }

        if (isLifetimeExtended)
        {
            // Keep track of the lifetime extended for this loop so we can update the call bits
            FOREACH_SLISTBASE_ENTRY(Loop *, currLoop, this->extendedLifetimesLoopList)
            {TRACE_IT(15247);
                currLoop->regAlloc.extendedLifetime->Prepend(lifetime);
            }
            NEXT_SLISTBASE_ENTRY
        }
        AssertMsg(lifetime->end > instr->GetNumber(), "Lifetime end not set correctly");
    }
    this->extendedLifetimesLoopList->Clear(this->tempAlloc);
}

// SCCLiveness::InsertLifetime
//      Insert a new lifetime in the list of lifetime.  The lifetime are inserted
//      in the reverse order of the lifetime starts.
Lifetime *
SCCLiveness::InsertLifetime(StackSym *stackSym, RegNum reg, IR::Instr *const currentInstr)
{TRACE_IT(15248);
    const uint start = currentInstr->GetNumber(), end = start;
    Lifetime * newLlifetime = JitAnew(tempAlloc, Lifetime, tempAlloc, stackSym, reg, start, end, this->func);
    newLlifetime->totalOpHelperLengthByEnd = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(currentInstr);

    // Find insertion point
    // This looks like a search, but we should almost exit on the first iteration, except
    // when we have loops and some lifetimes where extended.
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, &this->lifetimeList, iter)
    {TRACE_IT(15249);
        if (lifetime->start <= start)
        {TRACE_IT(15250);
            break;
        }
    }
    NEXT_SLIST_ENTRY_EDITING;

    iter.InsertBefore(newLlifetime);

    // let's say 'var a = 10;'. if a is not used in the function, we still want to have the instr, otherwise the write-through will not happen and upon debug bailout
    // we would not be able to restore the values to see in locals window.
    if (this->func->IsJitInDebugMode() && stackSym->HasByteCodeRegSlot() && this->func->IsNonTempLocalVar(stackSym->GetByteCodeRegSlot()))
    {TRACE_IT(15251);
        newLlifetime->isDeadStore = false;
    }

    stackSym->scratch.linearScan.lifetime = newLlifetime;
    return newLlifetime;
}

bool
SCCLiveness::FoldIndir(IR::Instr *instr, IR::Opnd *opnd)
{TRACE_IT(15252);
#ifdef _M_ARM
    // Can't be folded on ARM
    return false;
#else
    IR::IndirOpnd *indir = opnd->AsIndirOpnd();

    if(indir->GetIndexOpnd())
    {TRACE_IT(15253);
        IR::RegOpnd *index = indir->GetIndexOpnd();
        if (!index->m_sym || !index->m_sym->IsIntConst())
        {TRACE_IT(15254);
            return false;
        }

        // offset = indir.offset + (index << scale)
        int32 offset = index->m_sym->GetIntConstValue();
        if((indir->GetScale() != 0 && Int32Math::Shl(offset, indir->GetScale(), &offset)) ||
           (indir->GetOffset() != 0 && Int32Math::Add(indir->GetOffset(), offset, &offset)))
        {TRACE_IT(15255);
            return false;
        }
        indir->SetOffset(offset);
        indir->SetIndexOpnd(nullptr);
    }

    IR::RegOpnd *base = indir->GetBaseOpnd();
    if (!base->m_sym || !base->m_sym->IsConst() || base->m_sym->IsIntConst() || base->m_sym->IsFloatConst())
    {TRACE_IT(15256);
        return false;
    }

    uint8 *constValue = static_cast<uint8 *>(base->m_sym->GetConstAddress());
    if(indir->GetOffset() != 0)
    {TRACE_IT(15257);
        if(indir->GetOffset() < 0 ? constValue + indir->GetOffset() > constValue : constValue + indir->GetOffset() < constValue)
        {TRACE_IT(15258);
            return false;
        }
        constValue += indir->GetOffset();
    }

#ifdef _M_X64
    // Encoding only allows 32bits worth
    if(!Math::FitsInDWord((size_t)constValue))
    {TRACE_IT(15259);
        return false;
    }
#endif

    IR::MemRefOpnd *memref = IR::MemRefOpnd::New(constValue, indir->GetType(), instr->m_func);

    if (indir == instr->GetDst())
    {TRACE_IT(15260);
        instr->ReplaceDst(memref);
    }
    else
    {TRACE_IT(15261);
        instr->ReplaceSrc(indir, memref);
    }
    return true;
#endif
}

uint SCCLiveness::CurrentOpHelperVisitedLength(IR::Instr *const currentInstr) const
{TRACE_IT(15262);
    Assert(currentInstr);

    if(!lastOpHelperLabel)
    {TRACE_IT(15263);
        return 0;
    }

    Assert(currentInstr->GetNumber() >= lastOpHelperLabel->GetNumber());
    uint visitedLength = currentInstr->GetNumber() - lastOpHelperLabel->GetNumber();
    if(!currentInstr->IsLabelInstr())
    {TRACE_IT(15264);
        // Consider the current instruction to have been visited
        ++visitedLength;
    }
    return visitedLength;
}

#if DBG_DUMP

// SCCLiveness::Dump
void
SCCLiveness::Dump()
{TRACE_IT(15265);
    this->func->DumpHeader();
    Output::Print(_u("************   Liveness   ************\n"));

    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, &this->lifetimeList)
    {TRACE_IT(15266);
        lifetime->sym->Dump();
        Output::Print(_u(": live range %3d - %3d (XUserCall: %d, XCall: %d)\n"), lifetime->start, lifetime->end,
            lifetime->isLiveAcrossUserCalls,
            lifetime->isLiveAcrossCalls);
    }
    NEXT_SLIST_ENTRY;


    FOREACH_INSTR_IN_FUNC(instr, func)
    {TRACE_IT(15267);
        Output::Print(_u("%3d > "), instr->GetNumber());
        instr->Dump();
    } NEXT_INSTR_IN_FUNC;
}

#endif

uint OpHelperBlock::Length() const
{TRACE_IT(15268);
    Assert(opHelperLabel);
    Assert(opHelperEndInstr);

    uint length = opHelperEndInstr->GetNumber() - opHelperLabel->GetNumber();
    if(!opHelperEndInstr->IsLabelInstr())
    {TRACE_IT(15269);
        ++length;
    }
    return length;
}
