//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

// Peeps::PeepFunc
// Run peeps on this function.  For now, it just cleans the redundant reloads
// from the register allocator, and reg/reg movs.  The code tracks which sym is
// in which registers on extended basic blocks.
void
Peeps::PeepFunc()
{TRACE_IT(14897);
    bool peepsEnabled = true;
    if (PHASE_OFF(Js::PeepsPhase, this->func))
    {TRACE_IT(14898);
        peepsEnabled = false;
    }

#if defined(_M_IX86) || defined(_M_X64)
    // Agen dependency elimination pass
    // Since it can reveal load elimination opportunities for the normal peeps pass, we do it separately.
    this->peepsAgen.PeepFunc();
#endif

    this->peepsMD.Init(this);

    // Init regMap
    memset(this->regMap, 0, sizeof(this->regMap));

    // Scratch field needs to be cleared.
    this->func->m_symTable->ClearStackSymScratch();

    bool isInHelper = false;

    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {TRACE_IT(14899);
        switch (instr->GetKind())
        {
        case IR::InstrKindLabel:
        case IR::InstrKindProfiledLabel:
        {TRACE_IT(14900);
            if (!peepsEnabled)
            {TRACE_IT(14901);
                break;
            }
            // Don't carry any regMap info across label
            this->ClearRegMap();

            // Remove unreferenced labels
            if (instr->AsLabelInstr()->IsUnreferenced())
            {TRACE_IT(14902);
                bool peeped;
                instrNext = PeepUnreachableLabel(instr->AsLabelInstr(), isInHelper, &peeped);
                if(peeped)
                {TRACE_IT(14903);
                    continue;
                }
            }
            else
            {TRACE_IT(14904);
                // Try to peep a previous branch again after dead label blocks are removed. For instance:
                //         jmp L2
                //     L3:
                //         // dead code
                //     L2:
                // L3 is unreferenced, so after that block is removed, the branch-to-next can be removed. After that, if L2 is
                // unreferenced and only has fallthrough, it can be removed as well.
                IR::Instr *const prevInstr = instr->GetPrevRealInstr();
                if(prevInstr->IsBranchInstr())
                {TRACE_IT(14905);
                    IR::BranchInstr *const branch = prevInstr->AsBranchInstr();
                    if(branch->IsUnconditional() && !branch->IsMultiBranch() && branch->GetTarget() == instr)
                    {TRACE_IT(14906);
                        bool peeped;
                        IR::Instr *const branchNext = branch->m_next;
                        IR::Instr *const branchNextAfterPeep = PeepBranch(branch, &peeped);
                        if(peeped || branchNextAfterPeep != branchNext)
                        {TRACE_IT(14907);
                            // The peep did something, restart from after the branch
                            instrNext = branchNextAfterPeep;
                            continue;
                        }
                    }
                }
            }

            isInHelper = instr->AsLabelInstr()->isOpHelper;

            if (instrNext->IsLabelInstr())
            {TRACE_IT(14908);
                // CLean up double label
                instrNext = this->CleanupLabel(instr->AsLabelInstr(), instrNext->AsLabelInstr());
            }

#if defined(_M_IX86) || defined(_M_X64)
            Assert(instrNext->IsLabelInstr() || instrNext->m_prev->IsLabelInstr());
            IR::LabelInstr *const peepCondMoveLabel =
                instrNext->IsLabelInstr() ? instrNext->AsLabelInstr() : instrNext->m_prev->AsLabelInstr();
            instrNext = PeepCondMove(peepCondMoveLabel, instrNext, isInHelper && peepCondMoveLabel->isOpHelper);
#endif

            break;
        }

        case IR::InstrKindBranch:
            if (!peepsEnabled || instr->m_opcode == Js::OpCode::Leave)
            {TRACE_IT(14909);
                break;
            }
            instrNext = Peeps::PeepBranch(instr->AsBranchInstr());
#if defined(_M_IX86) || defined(_M_X64)
            Assert(instrNext && instrNext->m_prev);
            if (instrNext->m_prev->IsBranchInstr())
            {TRACE_IT(14910);
                instrNext = this->HoistSameInstructionAboveSplit(instrNext->m_prev->AsBranchInstr(), instrNext);
            }

#endif
            break;

        case IR::InstrKindPragma:
            if (instr->m_opcode == Js::OpCode::Nop)
            {TRACE_IT(14911);
                instr->Remove();
            }
            break;

        default:
            if (LowererMD::IsAssign(instr))
            {TRACE_IT(14912);
                if (!peepsEnabled)
                {TRACE_IT(14913);
                    break;
                }
                // Cleanup spill code
                instrNext = this->PeepAssign(instr);
            }
            else if (instr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn
                || instr->m_opcode == Js::OpCode::StartCall
                || instr->m_opcode == Js::OpCode::LoweredStartCall)
            {TRACE_IT(14914);
                // ArgOut/StartCall are normally lowered by the lowering of the associated call instr.
                // If the call becomes unreachable, we could end up with an orphan ArgOut or StartCall.
                // Just delete these StartCalls
                instr->Remove();
            }
#if defined(_M_IX86) || defined(_M_X64)
            else if (instr->m_opcode == Js::OpCode::MOVSD_ZERO)
            {TRACE_IT(14915);
                this->peepsMD.PeepAssign(instr);
                IR::Opnd *dst = instr->GetDst();

                // Look for explicit reg kills
                if (dst && dst->IsRegOpnd())
                {TRACE_IT(14916);
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
            }
            else if ( (instr->m_opcode == Js::OpCode::INC ) || (instr->m_opcode == Js::OpCode::DEC) )
            {TRACE_IT(14917);
                // Check for any of the following patterns which can cause partial flag dependency
                //
                //                                                        Jcc or SHL or SHR or SAR or SHLD(in case of x64)
                // Jcc or SHL or SHR or SAR or SHLD(in case of x64)       Any Instruction
                // INC or DEC                                             INC or DEC
                // -------------------------------------------------- OR -----------------------
                // INC or DEC                                             INC or DEC
                // Jcc or SHL or SHR or SAR or SHLD(in case of x64)       Any Instruction
                //                                                        Jcc or SHL or SHR or SAR or SHLD(in case of x64)
                //
                // With this optimization if any of the above pattern found, substitute INC/DEC with ADD/SUB respectively.
                if (!peepsEnabled)
                {TRACE_IT(14918);
                    break;
                }

                if (AutoSystemInfo::Data.IsAtomPlatform() || PHASE_FORCE(Js::AtomPhase, this->func))
                {TRACE_IT(14919);
                    bool pattern_found=false;

                    if ( !(instr->IsEntryInstr()) )
                    {TRACE_IT(14920);
                        IR::Instr *prevInstr = instr->GetPrevRealInstr();
                        if ( IsJccOrShiftInstr(prevInstr)  )
                        {TRACE_IT(14921);
                            pattern_found = true;
                        }
                        else if ( !(prevInstr->IsEntryInstr()) && IsJccOrShiftInstr(prevInstr->GetPrevRealInstr()) )
                        {TRACE_IT(14922);
                            pattern_found=true;
                        }
                    }

                    if ( !pattern_found && !(instr->IsExitInstr()) )
                    {TRACE_IT(14923);
                        IR::Instr *nextInstr = instr->GetNextRealInstr();
                        if ( IsJccOrShiftInstr(nextInstr) )
                        {TRACE_IT(14924);
                            pattern_found = true;
                        }
                        else if ( !(nextInstr->IsExitInstr() ) && (IsJccOrShiftInstr(nextInstr->GetNextRealInstr())) )
                        {TRACE_IT(14925);
                            pattern_found = true;
                        }
                    }

                    if (pattern_found)
                    {TRACE_IT(14926);
                        IR::IntConstOpnd* constOne  = IR::IntConstOpnd::New((IntConstType) 1, instr->GetDst()->GetType(), instr->m_func);
                        IR::Instr * addOrSubInstr = IR::Instr::New(Js::OpCode::ADD, instr->GetDst(), instr->GetDst(), constOne, instr->m_func);

                        if (instr->m_opcode == Js::OpCode::DEC)
                        {TRACE_IT(14927);
                            addOrSubInstr->m_opcode = Js::OpCode::SUB;
                        }

                        instr->InsertAfter(addOrSubInstr);
                        instr->Remove();
                        instr = addOrSubInstr;
                    }
                }

                IR::Opnd *dst = instr->GetDst();

                // Look for explicit reg kills
                if (dst && dst->IsRegOpnd())
                {TRACE_IT(14928);
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
            }

#endif
            else
            {TRACE_IT(14929);
                if (!peepsEnabled)
                {TRACE_IT(14930);
                    break;
                }
#if defined(_M_IX86) || defined(_M_X64)
               instr = this->PeepRedundant(instr);
#endif

                IR::Opnd *dst = instr->GetDst();

                // Look for explicit reg kills
                if (dst && dst->IsRegOpnd())
                {TRACE_IT(14931);
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
                // Kill callee-saved regs across calls and other implicit regs
                this->peepsMD.ProcessImplicitRegs(instr);

#if defined(_M_IX86) || defined(_M_X64)
                if (instr->m_opcode == Js::OpCode::TEST && instr->GetSrc2()->IsIntConstOpnd()
                    && ((instr->GetSrc2()->AsIntConstOpnd()->GetValue() & 0xFFFFFF00) == 0)
                    && instr->GetSrc1()->IsRegOpnd() && (LinearScan::GetRegAttribs(instr->GetSrc1()->AsRegOpnd()->GetReg()) & RA_BYTEABLE))
                {TRACE_IT(14932);
                    // Only support if the branch is JEQ or JNE to ensure we don't look at the sign flag
                    if (instrNext->IsBranchInstr() &&
                        (instrNext->m_opcode == Js::OpCode::JNE || instrNext->m_opcode == Js::OpCode::JEQ))
                    {TRACE_IT(14933);
                        instr->GetSrc1()->SetType(TyInt8);
                        instr->GetSrc2()->SetType(TyInt8);
                    }
                }

                if (instr->m_opcode == Js::OpCode::CVTSI2SD)
                {TRACE_IT(14934);
                    IR::Instr *xorps = IR::Instr::New(Js::OpCode::XORPS, instr->GetDst(), instr->GetDst(), instr->GetDst(), instr->m_func);
                    instr->InsertBefore(xorps);
                }
#endif
            }
        }
    } NEXT_INSTR_IN_FUNC_EDITING;
}

#if defined(_M_IX86) || defined(_M_X64)
// Peeps::IsJccOrShiftInstr()
// Check if instruction is any of the Shift or conditional jump variant
bool
Peeps::IsJccOrShiftInstr(IR::Instr *instr)
{TRACE_IT(14935);
    bool instrFound = (instr->IsBranchInstr() && instr->AsBranchInstr()->IsConditional()) ||
        (instr->m_opcode == Js::OpCode::SHL) || (instr->m_opcode == Js::OpCode::SHR) || (instr->m_opcode == Js::OpCode::SAR);

#if defined(_M_X64)
    instrFound = instrFound || (instr->m_opcode == Js::OpCode::SHLD);
#endif

    return (instrFound);
}
#endif

// Peeps::PeepAssign
// Remove useless MOV reg, reg as well as redundant reloads
IR::Instr *
Peeps::PeepAssign(IR::Instr *assign)
{TRACE_IT(14936);
    IR::Opnd *dst = assign->GetDst();
    IR::Opnd *src = assign->GetSrc1();
    IR::Instr *instrNext = assign->m_next;

    // MOV reg, sym
    if (src->IsSymOpnd() && src->AsSymOpnd()->m_offset == 0)
    {TRACE_IT(14937);
        AssertMsg(src->AsSymOpnd()->m_sym->IsStackSym(), "Only expect stackSyms at this point");
        StackSym *sym = src->AsSymOpnd()->m_sym->AsStackSym();

        if (sym->scratch.peeps.reg != RegNOREG)
        {TRACE_IT(14938);
            // Found a redundant load
            AssertMsg(this->regMap[sym->scratch.peeps.reg] == sym, "Something is wrong...");
            assign->ReplaceSrc1(IR::RegOpnd::New(sym, sym->scratch.peeps.reg, src->GetType(), this->func));
            src = assign->GetSrc1();
        }
        else
        {TRACE_IT(14939);
            // Keep track of this load

            AssertMsg(dst->IsRegOpnd(), "For now, we assume dst = sym means dst is a reg");

            RegNum reg = dst->AsRegOpnd()->GetReg();
            this->SetReg(reg, sym);

            return instrNext;
        }
    }
    if (dst->IsRegOpnd())
    {TRACE_IT(14940);
        // MOV reg, reg

        // Useless?
        if (src->IsRegOpnd() && src->AsRegOpnd()->IsSameReg(dst))
        {TRACE_IT(14941);
            assign->Remove();
            if (instrNext->m_prev->IsBranchInstr())
            {TRACE_IT(14942);
                return this->PeepBranch(instrNext->m_prev->AsBranchInstr());
            }
            else
            {TRACE_IT(14943);
                return instrNext;
            }
        }
        else
        {TRACE_IT(14944);
            // We could copy the a of the src, but we don't have
            // a way to track 2 regs on the sym...  So let's just clear
            // the info of the dst.
            RegNum dstReg = dst->AsRegOpnd()->GetReg();
            this->ClearReg(dstReg);
        }
    }
    else if (dst->IsSymOpnd() && dst->AsSymOpnd()->m_offset == 0 && src->IsRegOpnd())
    {TRACE_IT(14945);
        // MOV Sym, Reg
        // Track this reg
        RegNum reg = src->AsRegOpnd()->GetReg();
        StackSym *sym = dst->AsSymOpnd()->m_sym->AsStackSym();
        this->SetReg(reg, sym);

    }
    this->peepsMD.PeepAssign(assign);

    return instrNext;
}

// Peeps::ClearRegMap
// Empty the regMap.
// Note: might be faster to have a count and exit if zero?
void
Peeps::ClearRegMap()
{TRACE_IT(14946);
    for (RegNum reg = (RegNum)(RegNOREG+1); reg != RegNumCount; reg = (RegNum)(reg+1))
    {TRACE_IT(14947);
        this->ClearReg(reg);
    }
}

// Peeps::SetReg
// Track that this sym is live in this reg
void
Peeps::SetReg(RegNum reg, StackSym *sym)
{TRACE_IT(14948);
    this->ClearReg(sym->scratch.peeps.reg);
    this->ClearReg(reg);

    this->regMap[reg] = sym;
    sym->scratch.peeps.reg = reg;
}

// Peeps::ClearReg
void
Peeps::ClearReg(RegNum reg)
{TRACE_IT(14949);
    StackSym *sym = this->regMap[reg];

    if (sym)
    {TRACE_IT(14950);
        AssertMsg(sym->scratch.peeps.reg == reg, "Something is wrong here...");
        sym->scratch.peeps.reg = RegNOREG;
        this->regMap[reg] = NULL;
    }
}

// Peeps::PeepBranch
//      Remove branch-to-next
//      Invert condBranch/uncondBranch/label
//      Retarget branch to branch
IR::Instr *
Peeps::PeepBranch(IR::BranchInstr *branchInstr, bool *const peepedRef)
{TRACE_IT(14951);
    if(peepedRef)
    {TRACE_IT(14952);
        *peepedRef = false;
    }

    IR::LabelInstr *targetInstr = branchInstr->GetTarget();
    IR::Instr *instrNext;

    if (branchInstr->IsUnconditional())
    {TRACE_IT(14953);
        // Cleanup unreachable code after unconditional branch
        instrNext = RemoveDeadBlock(branchInstr->m_next);
    }

    instrNext = branchInstr->GetNextRealInstrOrLabel();

    if (instrNext != NULL && instrNext->IsLabelInstr())
    {TRACE_IT(14954);
        //
        // Remove branch-to-next
        //
        if (targetInstr == instrNext)
        {TRACE_IT(14955);
            if (!branchInstr->IsLowered())
            {TRACE_IT(14956);
                if (branchInstr->HasAnyImplicitCalls())
                {TRACE_IT(14957);
                    Assert(!branchInstr->m_func->GetJITFunctionBody()->IsAsmJsMode());
                    // if (x > y) might trigger a call to valueof() or something for x and y.
                    // We can't just delete them.
                    Js::OpCode newOpcode;
                    switch(branchInstr->m_opcode)
                    {
                    case Js::OpCode::BrEq_A:
                    case Js::OpCode::BrNeq_A:
                    case Js::OpCode::BrNotEq_A:
                    case Js::OpCode::BrNotNeq_A:
                        newOpcode = Js::OpCode::DeadBrEqual;
                        break;

                    case Js::OpCode::BrSrEq_A:
                    case Js::OpCode::BrSrNeq_A:
                    case Js::OpCode::BrSrNotEq_A:
                    case Js::OpCode::BrSrNotNeq_A:
                        newOpcode = Js::OpCode::DeadBrSrEqual;
                        break;

                    case Js::OpCode::BrGe_A:
                    case Js::OpCode::BrGt_A:
                    case Js::OpCode::BrLe_A:
                    case Js::OpCode::BrLt_A:
                    case Js::OpCode::BrNotGe_A:
                    case Js::OpCode::BrNotGt_A:
                    case Js::OpCode::BrNotLe_A:
                    case Js::OpCode::BrNotLt_A:
                    case Js::OpCode::BrUnGe_A:
                    case Js::OpCode::BrUnGt_A:
                    case Js::OpCode::BrUnLe_A:
                    case Js::OpCode::BrUnLt_A:
                        newOpcode = Js::OpCode::DeadBrRelational;
                        break;

                    case Js::OpCode::BrOnHasProperty:
                    case Js::OpCode::BrOnNoProperty:
                        newOpcode = Js::OpCode::DeadBrOnHasProperty;
                        break;

                    default:
                        Assert(UNREACHED);
                        newOpcode = Js::OpCode::Nop;
                    }
                    IR::Instr *newInstr = IR::Instr::New(newOpcode, branchInstr->m_func);
                    newInstr->SetSrc1(branchInstr->GetSrc1());
                    newInstr->SetSrc2(branchInstr->GetSrc2());
                    branchInstr->InsertBefore(newInstr);
                    newInstr->SetByteCodeOffset(branchInstr);
                }
                else if (branchInstr->GetSrc1() && !branchInstr->GetSrc2())
                {TRACE_IT(14958);
                    // We only have cases with one src
                    Assert(branchInstr->GetSrc1()->IsRegOpnd());

                    IR::RegOpnd *regSrc = branchInstr->GetSrc1()->AsRegOpnd();
                    StackSym *symSrc = regSrc->GetStackSym();

                    if (symSrc->HasByteCodeRegSlot() && !regSrc->GetIsJITOptimizedReg())
                    {TRACE_IT(14959);
                        // No side-effects to worry about, but need to insert bytecodeUse.
                        IR::ByteCodeUsesInstr *byteCodeUsesInstr = IR::ByteCodeUsesInstr::New(branchInstr);
                        byteCodeUsesInstr->Set(regSrc);
                        branchInstr->InsertBefore(byteCodeUsesInstr);
                    }
                }
            }
            // Note: if branch is conditional, we have a dead test/cmp left behind...
            if(peepedRef)
            {TRACE_IT(14960);
                *peepedRef = true;
            }
            branchInstr->Remove();
            if (targetInstr->IsUnreferenced())
            {TRACE_IT(14961);
                // We may have exposed an unreachable label by deleting the branch
                instrNext = Peeps::PeepUnreachableLabel(targetInstr, false);
            }
            else
            {TRACE_IT(14962);
                instrNext = targetInstr;
            }
            IR::Instr *instrPrev = instrNext->GetPrevRealInstrOrLabel();
            if (instrPrev->IsBranchInstr())
            {TRACE_IT(14963);
                // The branch removal could have exposed a branch to next opportunity.
                return Peeps::PeepBranch(instrPrev->AsBranchInstr());
            }
            return instrNext;
        }
    }
    else if (branchInstr->IsConditional())
    {TRACE_IT(14964);
        AnalysisAssert(instrNext);
        if (instrNext->IsBranchInstr()
            && instrNext->AsBranchInstr()->IsUnconditional()
            && targetInstr == instrNext->AsBranchInstr()->GetNextRealInstrOrLabel()
            && !instrNext->AsBranchInstr()->IsMultiBranch())
        {TRACE_IT(14965);
            //
            // Invert condBranch/uncondBranch/label:
            //
            //      JCC L1                   JinvCC L3
            //      JMP L3       =>
            //      L1:
            IR::BranchInstr *uncondBranch = instrNext->AsBranchInstr();

            if (branchInstr->IsLowered())
            {TRACE_IT(14966);
                LowererMD::InvertBranch(branchInstr);
            }
            else
            {TRACE_IT(14967);
                branchInstr->Invert();
            }

            targetInstr = uncondBranch->GetTarget();
            branchInstr->SetTarget(targetInstr);
            if (targetInstr->IsUnreferenced())
            {TRACE_IT(14968);
                Peeps::PeepUnreachableLabel(targetInstr, false);
            }

            uncondBranch->Remove();

            return PeepBranch(branchInstr, peepedRef);
        }
    }

    if(branchInstr->IsMultiBranch())
    {TRACE_IT(14969);
        IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

        multiBranchInstr->UpdateMultiBrLabels([=](IR::LabelInstr * targetInstr) -> IR::LabelInstr *
        {
            IR::LabelInstr * labelInstr = RetargetBrToBr(branchInstr, targetInstr);
            return labelInstr;
        });
    }
    else
    {
        RetargetBrToBr(branchInstr, targetInstr);
    }

    return branchInstr->m_next;
}

#if defined(_M_IX86) || defined(_M_X64)
//
// For conditional branch JE $LTarget, if both target and fallthrough branch has the same
// instruction B, hoist it up and tail dup target branch:
//
//      A                      <unconditional branch>
//      JE  $LTarget           $LTarget:
//      B                           B
//      ...                         JMP $L2
//
//====> hoist B up: move B up from fallthrough branch, remove B in target branch, retarget to $L2
// $LTarget to $L2
//
//      A                      <unconditional branch>
//      B                      $LTarget:
//      JE  $L2                     JMP $L2
//      ...
//
//====> now $LTarget becomes to be an empty BB, which can be removed if it's an unreferenced label
//
//      A
//      B
//      JE  $L2
//      ...
//
//====> Note B will be hoist above compare instruction A if there are no dependency between A and B
//
//      B
//      A          (cmp instr)
//      JE  $L2
//      ...
//
IR::Instr *
Peeps::HoistSameInstructionAboveSplit(IR::BranchInstr *branchInstr, IR::Instr *instrNext)
{TRACE_IT(14970);
    Assert(branchInstr);
    if (!branchInstr->IsConditional() || branchInstr->IsMultiBranch() || !branchInstr->IsLowered())
    {TRACE_IT(14971);
        return instrNext;   // this optimization only applies to single conditional branch
    }

    IR::LabelInstr *targetLabel = branchInstr->GetTarget();
    Assert(targetLabel);

    // give up if there are other branch entries to the target label
    if (targetLabel->labelRefs.Count() > 1)
    {TRACE_IT(14972);
        return instrNext;
    }

    // Give up if previous instruction before target label has fallthrough, cannot hoist up
    IR::Instr *targetPrev = targetLabel->GetPrevRealInstrOrLabel();
    Assert(targetPrev);
    if (targetPrev->HasFallThrough())
    {TRACE_IT(14973);
        return instrNext;
    }

    IR::Instr *instrSetCondition = NULL;
    IR::Instr *branchPrev = branchInstr->GetPrevRealInstrOrLabel();
    while (branchPrev && !branchPrev->StartsBasicBlock())
    {TRACE_IT(14974);
        if (!instrSetCondition && EncoderMD::SetsConditionCode(branchPrev))
        {TRACE_IT(14975);   // located compare instruction for the branch
            instrSetCondition = branchPrev;
        }
        branchPrev = branchPrev->GetPrevRealInstrOrLabel(); // keep looking previous instr in BB
    }

    if (branchPrev && branchPrev->IsLabelInstr() && branchPrev->AsLabelInstr()->isOpHelper)
    {TRACE_IT(14976);   // don't apply the optimization when branch is in helper section
        return instrNext;
    }

    if (!instrSetCondition)
    {TRACE_IT(14977);   // give up if we cannot find the compare instruction in the BB, should be very rare
        return instrNext;
    }
    Assert(instrSetCondition);

    bool hoistAboveSetConditionInstr = false;
    if (instrSetCondition == branchInstr->GetPrevRealInstrOrLabel())
    {TRACE_IT(14978);   // if compare instruction is right before branch instruction, we can hoist above cmp instr
        hoistAboveSetConditionInstr = true;
    }   // otherwise we hoist the identical instructions above conditional branch split only

    IR::Instr *instr = branchInstr->GetNextRealInstrOrLabel();
    IR::Instr *targetInstr = targetLabel->GetNextRealInstrOrLabel();
    IR::Instr *branchNextInstr = NULL;
    IR::Instr *targetNextInstr = NULL;
    IR::Instr *instrHasHoisted = NULL;

    Assert(instr && targetInstr);
    while (!instr->EndsBasicBlock() && !instr->IsLabelInstr() && instr->IsEqual(targetInstr) &&
        !EncoderMD::UsesConditionCode(instr) && !EncoderMD::SetsConditionCode(instr) &&
        !this->peepsAgen.DependentInstrs(instrSetCondition, instr) &&
        // cannot hoist InlineeStart from branch targets even for the same inlinee function.
        // it is used by encoder to generate InlineeFrameRecord for each inlinee
        instr->m_opcode != Js::OpCode::InlineeStart)
    {TRACE_IT(14979);
        branchNextInstr = instr->GetNextRealInstrOrLabel();
        targetNextInstr = targetInstr->GetNextRealInstrOrLabel();

        instr->Unlink();                            // hoist up instr in fallthrough branch
        if (hoistAboveSetConditionInstr)
        {TRACE_IT(14980);
            instrSetCondition->InsertBefore(instr); // hoist above compare instruction
        }
        else
        {TRACE_IT(14981);
            branchInstr->InsertBefore(instr);       // hoist above branch split
        }
        targetInstr->Remove();                      // remove the same instruction in target branch

        if (!instrHasHoisted)
            instrHasHoisted = instr;                // points to the first hoisted instruction

        instr = branchNextInstr;
        targetInstr = targetNextInstr;
        Assert(instr && targetInstr);
    }

    if (instrHasHoisted)
    {TRACE_IT(14982);   // instructions have been hoisted, now check tail branch to see if it can be duplicated
        if (targetInstr->IsBranchInstr())
        {TRACE_IT(14983);
            IR::BranchInstr *tailBranch = targetInstr->AsBranchInstr();
            if (tailBranch->IsUnconditional() && !tailBranch->IsMultiBranch())
            {TRACE_IT(14984);   // target can be replaced since tail branch is a single unconditional jmp
                branchInstr->ReplaceTarget(targetLabel, tailBranch->GetTarget());
            }

            // now targeLabel is an empty Basic Block, remove it if it's not referenced
            if (targetLabel->IsUnreferenced())
            {TRACE_IT(14985);
                Peeps::PeepUnreachableLabel(targetLabel, targetLabel->isOpHelper);
            }
        }
        return instrHasHoisted;
    }

    return instrNext;
}
#endif

IR::LabelInstr *
Peeps::RetargetBrToBr(IR::BranchInstr *branchInstr, IR::LabelInstr * targetInstr)
{TRACE_IT(14986);
    AnalysisAssert(targetInstr);
    IR::Instr *targetInstrNext = targetInstr->GetNextRealInstr();
    AnalysisAssertMsg(targetInstrNext, "GetNextRealInstr() failed to get next target");

    // Removing branch to branch breaks some lexical assumptions about loop in sccliveness/linearscan/second chance.
    if (!branchInstr->IsLowered())
    {TRACE_IT(14987);
        return targetInstr;
    }

    //
    // Retarget branch-to-branch
    //
#if DBG
    uint counter = 0;
#endif

    IR::LabelInstr *lastLoopTop = NULL;

    while (true)
    {TRACE_IT(14988);
        // There's very few cases where we can safely follow a branch chain with intervening instrs.
        // One of them, which comes up occasionally, is where there is a copy of a single-def symbol
        // to another single-def symbol which is only used for the branch instruction (i.e. one dead
        // after the branch). Another is where a single-def symbol is declared of a constant (e.g. a
        // symbol created to store "True"
        // Unfortuantely, to properly do this, we'd need to do it somewhere else (i.e. not in peeps)
        // and make use of the additional information that we'd have there. Having the flow graph or
        // just any more information about variable liveness is necessary to determine that the load
        // instructions between jumps can be safely skipped.
        // The general case where this would be useful, on a higher level, is where a long statement
        // containing many branches returns a value; the branching here can put the result into some
        // different stacksym at each level, meaning that there'd be a load between each branch. The
        // result is that we don't currently optimize it.
        IR::BranchInstr *branchAtTarget = nullptr;
        if (targetInstrNext->IsBranchInstr())
        {TRACE_IT(14989);
            branchAtTarget = targetInstrNext->AsBranchInstr();
        }
        else
        {TRACE_IT(14990);
            // We don't have the information here to decide whether or not to continue the branch chain.
            break;
        }
        // This used to just be a targetInstrNext->AsBranchInstr()->IsUnconditional(), but, in order
        // to optimize further, it became necessary to handle more than just unconditional jumps. In
        // order to keep the code relatively clean, the "is it an inherently-taken jump chain" check
        // code now follows here:
        if (!targetInstrNext->AsBranchInstr()->IsUnconditional())
        {TRACE_IT(14991);
            bool safetofollow = false;
            if(targetInstrNext->m_opcode == branchInstr->m_opcode)
            {TRACE_IT(14992);
                // If it's the same branch instruction, with the same arguments, the branch decision should,
                // similarly, be the same. There's a bit more that can be done with this (e.g. for inverted,
                // but otherwise similar instructions like brTrue and brFalse, the destination could go down
                // the other path), but this is something that should probably be done more generally, so we
                // can optimize branch chains that have other interesting bechaviors.
                if (
                    (
                        (branchInstr->GetSrc1() && targetInstrNext->GetSrc1() && branchInstr->GetSrc1()->IsEqual(targetInstrNext->GetSrc1())) ||
                        !(branchInstr->GetSrc1() || targetInstrNext->GetSrc1())
                    ) && (
                        (branchInstr->GetSrc2() && targetInstrNext->GetSrc2() && branchInstr->GetSrc2()->IsEqual(targetInstrNext->GetSrc2())) ||
                        !(branchInstr->GetSrc2() || targetInstrNext->GetSrc2())
                    )
                   )
                {TRACE_IT(14993);
                    safetofollow = true;
                }
            }
            if (!safetofollow)
            {TRACE_IT(14994);
                // We can't say safely that this branch is something that we can implicitly take, so instead
                // cut off the branch chain optimization here.
                break;
            }
        }

        // We don't want to skip the loop entry, unless we're right before the encoder
        if (targetInstr->m_isLoopTop && !branchAtTarget->IsLowered())
        {TRACE_IT(14995);
            break;
        }

        if (targetInstr->m_isLoopTop)
        {TRACE_IT(14996);
            if (targetInstr == lastLoopTop)
            {TRACE_IT(14997);
                // We are back to a loopTop already visited.
                // Looks like an infinite loop somewhere...
                break;
            }
            lastLoopTop = targetInstr;
        }
#if DBG
        if (!branchInstr->IsMultiBranch() && branchInstr->GetTarget()->m_noHelperAssert && !branchAtTarget->IsMultiBranch())
        {TRACE_IT(14998);
            branchAtTarget->GetTarget()->m_noHelperAssert = true;
        }

        AssertMsg(++counter < 10000, "We should not be looping this many times!");
#endif

        IR::LabelInstr * reTargetLabel = branchAtTarget->GetTarget();
        AnalysisAssert(reTargetLabel);
        if (targetInstr == reTargetLabel)
        {TRACE_IT(14999);
            // Infinite loop.
            //    JCC $L1
            // L1:
            //    JMP $L1
            break;
        }

        if(branchInstr->IsMultiBranch())
        {TRACE_IT(15000);
            IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();
            multiBranchInstr->ChangeLabelRef(targetInstr, reTargetLabel);
        }
        else
        {TRACE_IT(15001);
            branchInstr->SetTarget(reTargetLabel);
        }

        if (targetInstr->IsUnreferenced())
        {TRACE_IT(15002);
            Peeps::PeepUnreachableLabel(targetInstr, false);
        }

        targetInstr = reTargetLabel;
        targetInstrNext = targetInstr->GetNextRealInstr();
    }
    return targetInstr;
}

IR::Instr *
Peeps::PeepUnreachableLabel(IR::LabelInstr *deadLabel, const bool isInHelper, bool *const peepedRef)
{TRACE_IT(15003);
    Assert(deadLabel);
    Assert(deadLabel->IsUnreferenced());

    IR::Instr *prevFallthroughInstr = deadLabel;
    do
    {TRACE_IT(15004);
        prevFallthroughInstr = prevFallthroughInstr->GetPrevRealInstrOrLabel();
        // The previous dead label may have been kept around due to a StatementBoundary, see comment in RemoveDeadBlock.
    } while(prevFallthroughInstr->IsLabelInstr() && prevFallthroughInstr->AsLabelInstr()->IsUnreferenced());

    IR::Instr *instrReturn;
    bool removeLabel;

    // If code is now unreachable, delete block
    if (!prevFallthroughInstr->HasFallThrough())
    {TRACE_IT(15005);
        bool wasStatementBoundaryKeptInDeadBlock = false;
        instrReturn = RemoveDeadBlock(deadLabel->m_next, &wasStatementBoundaryKeptInDeadBlock);

        // Remove label only if we didn't have to keep last StatementBoundary in the dead block,
        // see comment in RemoveDeadBlock.
        removeLabel = !wasStatementBoundaryKeptInDeadBlock;

        if(peepedRef)
        {TRACE_IT(15006);
            *peepedRef = true;
        }
    }
    else
    {TRACE_IT(15007);
        instrReturn = deadLabel->m_next;
        removeLabel =
            deadLabel->isOpHelper == isInHelper
#if DBG
            && !deadLabel->m_noHelperAssert
#endif
            ;
        if(peepedRef)
        {TRACE_IT(15008);
            *peepedRef = removeLabel;
        }
    }

    if (removeLabel && deadLabel->IsUnreferenced())
    {TRACE_IT(15009);
        deadLabel->Remove();
    }

    return instrReturn;
}

IR::Instr *
Peeps::CleanupLabel(IR::LabelInstr * instr, IR::LabelInstr * instrNext)
{TRACE_IT(15010);
    IR::Instr * returnInstr;

    IR::LabelInstr * labelToRemove;
    IR::LabelInstr * labelToKeep;

    // Just for dump, always keep loop top labels
    // We also can remove label that has non branch references
    if (instrNext->m_isLoopTop || instrNext->m_hasNonBranchRef)
    {TRACE_IT(15011);
        if (instr->m_isLoopTop || instr->m_hasNonBranchRef)
        {TRACE_IT(15012);
            // Don't remove loop top labels or labels with non branch references
            return instrNext;
        }
        labelToRemove = instr;
        labelToKeep = instrNext;
        returnInstr = instrNext;
    }
    else
    {TRACE_IT(15013);
        labelToRemove = instrNext;
        labelToKeep = instr;
        returnInstr = instrNext->m_next;
    }
    while (!labelToRemove->labelRefs.Empty())
    {TRACE_IT(15014);
        bool replaced = labelToRemove->labelRefs.Head()->ReplaceTarget(labelToRemove, labelToKeep);
        Assert(replaced);
    }

    if (labelToRemove->isOpHelper)
    {TRACE_IT(15015);
        labelToKeep->isOpHelper = true;
#if DBG
        if (labelToRemove->m_noHelperAssert)
        {TRACE_IT(15016);
            labelToKeep->m_noHelperAssert = true;
        }
#endif
    }

    labelToRemove->Remove();
    return returnInstr;
}

//
// Removes instrs starting from one specified by the 'instr' parameter.
// Keeps last statement boundary in the whole block to remove.
// Stops at label or exit instr.
// Return value:
// - 1st instr that is label or end instr, except the case when forceRemoveFirstInstr is true, in which case
//   we start checking for exit loop condition from next instr.
// Notes:
// - if wasStmtBoundaryKeptInDeadBlock is not NULL, it receives true when we didn't remove last
//   StatementBoundary pragma instr as otherwise it would be non-helper/helper move of the pragma instr.
//   If there was no stmt boundary or last stmt boundary moved to after next label, that would receive false.
//
IR::Instr *Peeps::RemoveDeadBlock(IR::Instr *instr, bool* wasStmtBoundaryKeptInDeadBlock /* = nullptr */)
{TRACE_IT(15017);
    IR::Instr* lastStatementBoundary = nullptr;

    while (instr && !instr->IsLabelInstr() && !instr->IsExitInstr())
    {TRACE_IT(15018);
        IR::Instr *deadInstr = instr;
        instr = instr->m_next;

        if (deadInstr->IsPragmaInstr() && deadInstr->m_opcode == Js::OpCode::StatementBoundary)
        {TRACE_IT(15019);
            if (lastStatementBoundary)
            {TRACE_IT(15020);
                //Its enough if we keep latest statement boundary. Rest are dead anyway.
                lastStatementBoundary->Remove();
            }
            lastStatementBoundary = deadInstr;
        }
        else
        {TRACE_IT(15021);
            deadInstr->Remove();
        }
    }

    // Do not let StatementBoundary to move across non-helper and helper blocks, very important under debugger:
    // if we let that happen, helper block can be moved to the end of the func so that statement maps will miss one statement.
    // Issues can be when (normally, StatementBoundary should never belong to a helper label):
    // - if we remove the label and prev label is a helper, StatementBoundary will be moved inside helper.
    // - if we move StatementBoundary under next label which is a helper, same problem again.
    bool canMoveStatementBoundaryUnderNextLabel = instr && instr->IsLabelInstr() && !instr->AsLabelInstr()->isOpHelper;

    if (lastStatementBoundary && canMoveStatementBoundaryUnderNextLabel)
    {TRACE_IT(15022);
        lastStatementBoundary->Unlink();
        instr->InsertAfter(lastStatementBoundary);
    }

    if (wasStmtBoundaryKeptInDeadBlock)
    {TRACE_IT(15023);
        *wasStmtBoundaryKeptInDeadBlock = lastStatementBoundary && !canMoveStatementBoundaryUnderNextLabel;
    }

    return instr;
}

// Shared code for x86 and amd64
#if defined(_M_IX86) || defined(_M_X64)
IR::Instr *
Peeps::PeepRedundant(IR::Instr *instr)
{TRACE_IT(15024);
    IR::Instr *retInstr = instr;

    if (instr->m_opcode == Js::OpCode::ADD || instr->m_opcode == Js::OpCode::SUB || instr->m_opcode == Js::OpCode::OR)
    {TRACE_IT(15025);
        Assert(instr->GetSrc1() && instr->GetSrc2());
        if( (instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() == 0))
        {TRACE_IT(15026);
            // remove instruction
            retInstr = instr->m_next;
            instr->Remove();
        }
    }
#if _M_IX86
    RegNum edx = RegEDX;
#else
    RegNum edx = RegRDX;
#endif
    if (instr->m_opcode == Js::OpCode::NOP && instr->GetDst() != NULL
        && instr->GetDst()->IsRegOpnd() && instr->GetDst()->AsRegOpnd()->GetReg() == edx)
    {TRACE_IT(15027);
        // dummy def used for non-32bit ovf check for IMUL
        // check edx is not killed between IMUL and edx = NOP, then remove the NOP
        bool found = false;
        IR::Instr *nopInstr = instr;
        do
        {TRACE_IT(15028);
            instr = instr->GetPrevRealInstrOrLabel();
            if (
                instr->m_opcode == Js::OpCode::IMUL ||
                (instr->m_opcode == Js::OpCode::CALL && this->func->GetJITFunctionBody()->IsWasmFunction())
            )
            {TRACE_IT(15029);
                found = true;
                break;
            }
        } while(!instr->StartsBasicBlock());

        if (found)
        {TRACE_IT(15030);
            retInstr = nopInstr->m_next;
            nopInstr->Remove();
        }
        else
        {TRACE_IT(15031);
            instr = nopInstr;
            do
            {TRACE_IT(15032);
                instr = instr->GetNextRealInstrOrLabel();
                if (instr->m_opcode == Js::OpCode::DIV)
                {TRACE_IT(15033);
                    found = true;
                    retInstr = nopInstr->m_next;
                    nopInstr->Remove();
                    break;
                }
            } while (!instr->EndsBasicBlock());

            AssertMsg(found, "edx = NOP without an IMUL or DIV");
        }
    }
    return retInstr;
}

/*
    Work backwards from the label instruction to look for this pattern:
        Jcc $Label
        Mov
        Mov
        ..
        Mov
    Label:

    If found, we remove the Jcc, convert MOVs to CMOVcc and remove the label if unreachable.
*/
IR::Instr*
Peeps::PeepCondMove(IR::LabelInstr *labelInstr, IR::Instr *nextInstr, const bool isInHelper)
{TRACE_IT(15034);
    IR::Instr *instr = labelInstr->GetPrevRealInstrOrLabel();

    Js::OpCode newOpCode;

    // Check if BB is all MOVs with both RegOpnd
    while(instr->m_opcode == Js::OpCode::MOV)
    {TRACE_IT(15035);
        if (!instr->GetSrc1()->IsRegOpnd() || !instr->GetDst()->IsRegOpnd())
            return nextInstr;
        instr = instr->GetPrevRealInstrOrLabel();
    }

    // Did we hit a conditional branch ?
    if (instr->IsBranchInstr() && instr->AsBranchInstr()->IsConditional() &&
        !instr->AsBranchInstr()->IsMultiBranch() &&
        instr->AsBranchInstr()->GetTarget() == labelInstr &&
        instr->m_opcode != Js::OpCode::Leave)
    {TRACE_IT(15036);
        IR::BranchInstr *brInstr = instr->AsBranchInstr();

        // Get the correct CMOVcc
        switch(brInstr->m_opcode)
        {
        case Js::OpCode::JA:
                newOpCode = Js::OpCode::CMOVBE;
                break;
        case Js::OpCode::JAE:
                newOpCode = Js::OpCode::CMOVB;
                break;
        case Js::OpCode::JB:
                newOpCode = Js::OpCode::CMOVAE;
                break;
        case Js::OpCode::JBE:
                newOpCode = Js::OpCode::CMOVA;
                break;
        case Js::OpCode::JEQ:
                newOpCode = Js::OpCode::CMOVNE;
                break;
        case Js::OpCode::JNE:
                newOpCode = Js::OpCode::CMOVE;
                break;
        case Js::OpCode::JNP:
                newOpCode = Js::OpCode::CMOVP;
                break;
        case Js::OpCode::JLT:
                newOpCode = Js::OpCode::CMOVGE;
                break;
        case Js::OpCode::JLE:
                newOpCode = Js::OpCode::CMOVG;
                break;
        case Js::OpCode::JGT:
                newOpCode = Js::OpCode::CMOVLE;
                break;
        case Js::OpCode::JGE:
                newOpCode = Js::OpCode::CMOVL;
                break;
        case Js::OpCode::JNO:
                newOpCode = Js::OpCode::CMOVO;
                break;
        case Js::OpCode::JO:
                newOpCode = Js::OpCode::CMOVNO;
                break;
        case Js::OpCode::JP:
                newOpCode = Js::OpCode::CMOVNP;
                break;
        case Js::OpCode::JNSB:
                newOpCode = Js::OpCode::CMOVS;
                break;
        case Js::OpCode::JSB:
                newOpCode = Js::OpCode::CMOVNS;
                break;
        default:
                Assert(UNREACHED);
                __assume(UNREACHED);
        }

        // convert the entire block to CMOVs
        instr = brInstr->GetNextRealInstrOrLabel();
        while(instr != labelInstr)
        {TRACE_IT(15037);
            instr->m_opcode = newOpCode;
            instr = instr->GetNextRealInstrOrLabel();
        }

        // remove the Jcc
        brInstr->Remove();
        // We may have exposed an unreachable label by deleting the branch
        if (labelInstr->IsUnreferenced())
           return Peeps::PeepUnreachableLabel(labelInstr, isInHelper);
    }
    return nextInstr;
}

#endif
