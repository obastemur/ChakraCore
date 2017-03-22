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
{LOGMEIN("Peeps.cpp] 12\n");
    bool peepsEnabled = true;
    if (PHASE_OFF(Js::PeepsPhase, this->func))
    {LOGMEIN("Peeps.cpp] 15\n");
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
    {LOGMEIN("Peeps.cpp] 36\n");
        switch (instr->GetKind())
        {LOGMEIN("Peeps.cpp] 38\n");
        case IR::InstrKindLabel:
        case IR::InstrKindProfiledLabel:
        {LOGMEIN("Peeps.cpp] 41\n");
            if (!peepsEnabled)
            {LOGMEIN("Peeps.cpp] 43\n");
                break;
            }
            // Don't carry any regMap info across label
            this->ClearRegMap();

            // Remove unreferenced labels
            if (instr->AsLabelInstr()->IsUnreferenced())
            {LOGMEIN("Peeps.cpp] 51\n");
                bool peeped;
                instrNext = PeepUnreachableLabel(instr->AsLabelInstr(), isInHelper, &peeped);
                if(peeped)
                {LOGMEIN("Peeps.cpp] 55\n");
                    continue;
                }
            }
            else
            {
                // Try to peep a previous branch again after dead label blocks are removed. For instance:
                //         jmp L2
                //     L3:
                //         // dead code
                //     L2:
                // L3 is unreferenced, so after that block is removed, the branch-to-next can be removed. After that, if L2 is
                // unreferenced and only has fallthrough, it can be removed as well.
                IR::Instr *const prevInstr = instr->GetPrevRealInstr();
                if(prevInstr->IsBranchInstr())
                {LOGMEIN("Peeps.cpp] 70\n");
                    IR::BranchInstr *const branch = prevInstr->AsBranchInstr();
                    if(branch->IsUnconditional() && !branch->IsMultiBranch() && branch->GetTarget() == instr)
                    {LOGMEIN("Peeps.cpp] 73\n");
                        bool peeped;
                        IR::Instr *const branchNext = branch->m_next;
                        IR::Instr *const branchNextAfterPeep = PeepBranch(branch, &peeped);
                        if(peeped || branchNextAfterPeep != branchNext)
                        {LOGMEIN("Peeps.cpp] 78\n");
                            // The peep did something, restart from after the branch
                            instrNext = branchNextAfterPeep;
                            continue;
                        }
                    }
                }
            }

            isInHelper = instr->AsLabelInstr()->isOpHelper;

            if (instrNext->IsLabelInstr())
            {LOGMEIN("Peeps.cpp] 90\n");
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
            {LOGMEIN("Peeps.cpp] 107\n");
                break;
            }
            instrNext = Peeps::PeepBranch(instr->AsBranchInstr());
#if defined(_M_IX86) || defined(_M_X64)
            Assert(instrNext && instrNext->m_prev);
            if (instrNext->m_prev->IsBranchInstr())
            {LOGMEIN("Peeps.cpp] 114\n");
                instrNext = this->HoistSameInstructionAboveSplit(instrNext->m_prev->AsBranchInstr(), instrNext);
            }

#endif
            break;

        case IR::InstrKindPragma:
            if (instr->m_opcode == Js::OpCode::Nop)
            {LOGMEIN("Peeps.cpp] 123\n");
                instr->Remove();
            }
            break;

        default:
            if (LowererMD::IsAssign(instr))
            {LOGMEIN("Peeps.cpp] 130\n");
                if (!peepsEnabled)
                {LOGMEIN("Peeps.cpp] 132\n");
                    break;
                }
                // Cleanup spill code
                instrNext = this->PeepAssign(instr);
            }
            else if (instr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn
                || instr->m_opcode == Js::OpCode::StartCall
                || instr->m_opcode == Js::OpCode::LoweredStartCall)
            {LOGMEIN("Peeps.cpp] 141\n");
                // ArgOut/StartCall are normally lowered by the lowering of the associated call instr.
                // If the call becomes unreachable, we could end up with an orphan ArgOut or StartCall.
                // Just delete these StartCalls
                instr->Remove();
            }
#if defined(_M_IX86) || defined(_M_X64)
            else if (instr->m_opcode == Js::OpCode::MOVSD_ZERO)
            {LOGMEIN("Peeps.cpp] 149\n");
                this->peepsMD.PeepAssign(instr);
                IR::Opnd *dst = instr->GetDst();

                // Look for explicit reg kills
                if (dst && dst->IsRegOpnd())
                {LOGMEIN("Peeps.cpp] 155\n");
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
            }
            else if ( (instr->m_opcode == Js::OpCode::INC ) || (instr->m_opcode == Js::OpCode::DEC) )
            {LOGMEIN("Peeps.cpp] 160\n");
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
                {LOGMEIN("Peeps.cpp] 173\n");
                    break;
                }

                if (AutoSystemInfo::Data.IsAtomPlatform() || PHASE_FORCE(Js::AtomPhase, this->func))
                {LOGMEIN("Peeps.cpp] 178\n");
                    bool pattern_found=false;

                    if ( !(instr->IsEntryInstr()) )
                    {LOGMEIN("Peeps.cpp] 182\n");
                        IR::Instr *prevInstr = instr->GetPrevRealInstr();
                        if ( IsJccOrShiftInstr(prevInstr)  )
                        {LOGMEIN("Peeps.cpp] 185\n");
                            pattern_found = true;
                        }
                        else if ( !(prevInstr->IsEntryInstr()) && IsJccOrShiftInstr(prevInstr->GetPrevRealInstr()) )
                        {LOGMEIN("Peeps.cpp] 189\n");
                            pattern_found=true;
                        }
                    }

                    if ( !pattern_found && !(instr->IsExitInstr()) )
                    {LOGMEIN("Peeps.cpp] 195\n");
                        IR::Instr *nextInstr = instr->GetNextRealInstr();
                        if ( IsJccOrShiftInstr(nextInstr) )
                        {LOGMEIN("Peeps.cpp] 198\n");
                            pattern_found = true;
                        }
                        else if ( !(nextInstr->IsExitInstr() ) && (IsJccOrShiftInstr(nextInstr->GetNextRealInstr())) )
                        {LOGMEIN("Peeps.cpp] 202\n");
                            pattern_found = true;
                        }
                    }

                    if (pattern_found)
                    {LOGMEIN("Peeps.cpp] 208\n");
                        IR::IntConstOpnd* constOne  = IR::IntConstOpnd::New((IntConstType) 1, instr->GetDst()->GetType(), instr->m_func);
                        IR::Instr * addOrSubInstr = IR::Instr::New(Js::OpCode::ADD, instr->GetDst(), instr->GetDst(), constOne, instr->m_func);

                        if (instr->m_opcode == Js::OpCode::DEC)
                        {LOGMEIN("Peeps.cpp] 213\n");
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
                {LOGMEIN("Peeps.cpp] 227\n");
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
            }

#endif
            else
            {
                if (!peepsEnabled)
                {LOGMEIN("Peeps.cpp] 236\n");
                    break;
                }
#if defined(_M_IX86) || defined(_M_X64)
               instr = this->PeepRedundant(instr);
#endif

                IR::Opnd *dst = instr->GetDst();

                // Look for explicit reg kills
                if (dst && dst->IsRegOpnd())
                {LOGMEIN("Peeps.cpp] 247\n");
                    this->ClearReg(dst->AsRegOpnd()->GetReg());
                }
                // Kill callee-saved regs across calls and other implicit regs
                this->peepsMD.ProcessImplicitRegs(instr);

#if defined(_M_IX86) || defined(_M_X64)
                if (instr->m_opcode == Js::OpCode::TEST && instr->GetSrc2()->IsIntConstOpnd()
                    && ((instr->GetSrc2()->AsIntConstOpnd()->GetValue() & 0xFFFFFF00) == 0)
                    && instr->GetSrc1()->IsRegOpnd() && (LinearScan::GetRegAttribs(instr->GetSrc1()->AsRegOpnd()->GetReg()) & RA_BYTEABLE))
                {LOGMEIN("Peeps.cpp] 257\n");
                    // Only support if the branch is JEQ or JNE to ensure we don't look at the sign flag
                    if (instrNext->IsBranchInstr() &&
                        (instrNext->m_opcode == Js::OpCode::JNE || instrNext->m_opcode == Js::OpCode::JEQ))
                    {LOGMEIN("Peeps.cpp] 261\n");
                        instr->GetSrc1()->SetType(TyInt8);
                        instr->GetSrc2()->SetType(TyInt8);
                    }
                }

                if (instr->m_opcode == Js::OpCode::CVTSI2SD)
                {LOGMEIN("Peeps.cpp] 268\n");
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
{LOGMEIN("Peeps.cpp] 283\n");
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
{LOGMEIN("Peeps.cpp] 299\n");
    IR::Opnd *dst = assign->GetDst();
    IR::Opnd *src = assign->GetSrc1();
    IR::Instr *instrNext = assign->m_next;

    // MOV reg, sym
    if (src->IsSymOpnd() && src->AsSymOpnd()->m_offset == 0)
    {LOGMEIN("Peeps.cpp] 306\n");
        AssertMsg(src->AsSymOpnd()->m_sym->IsStackSym(), "Only expect stackSyms at this point");
        StackSym *sym = src->AsSymOpnd()->m_sym->AsStackSym();

        if (sym->scratch.peeps.reg != RegNOREG)
        {LOGMEIN("Peeps.cpp] 311\n");
            // Found a redundant load
            AssertMsg(this->regMap[sym->scratch.peeps.reg] == sym, "Something is wrong...");
            assign->ReplaceSrc1(IR::RegOpnd::New(sym, sym->scratch.peeps.reg, src->GetType(), this->func));
            src = assign->GetSrc1();
        }
        else
        {
            // Keep track of this load

            AssertMsg(dst->IsRegOpnd(), "For now, we assume dst = sym means dst is a reg");

            RegNum reg = dst->AsRegOpnd()->GetReg();
            this->SetReg(reg, sym);

            return instrNext;
        }
    }
    if (dst->IsRegOpnd())
    {LOGMEIN("Peeps.cpp] 330\n");
        // MOV reg, reg

        // Useless?
        if (src->IsRegOpnd() && src->AsRegOpnd()->IsSameReg(dst))
        {LOGMEIN("Peeps.cpp] 335\n");
            assign->Remove();
            if (instrNext->m_prev->IsBranchInstr())
            {LOGMEIN("Peeps.cpp] 338\n");
                return this->PeepBranch(instrNext->m_prev->AsBranchInstr());
            }
            else
            {
                return instrNext;
            }
        }
        else
        {
            // We could copy the a of the src, but we don't have
            // a way to track 2 regs on the sym...  So let's just clear
            // the info of the dst.
            RegNum dstReg = dst->AsRegOpnd()->GetReg();
            this->ClearReg(dstReg);
        }
    }
    else if (dst->IsSymOpnd() && dst->AsSymOpnd()->m_offset == 0 && src->IsRegOpnd())
    {LOGMEIN("Peeps.cpp] 356\n");
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
{LOGMEIN("Peeps.cpp] 374\n");
    for (RegNum reg = (RegNum)(RegNOREG+1); reg != RegNumCount; reg = (RegNum)(reg+1))
    {LOGMEIN("Peeps.cpp] 376\n");
        this->ClearReg(reg);
    }
}

// Peeps::SetReg
// Track that this sym is live in this reg
void
Peeps::SetReg(RegNum reg, StackSym *sym)
{LOGMEIN("Peeps.cpp] 385\n");
    this->ClearReg(sym->scratch.peeps.reg);
    this->ClearReg(reg);

    this->regMap[reg] = sym;
    sym->scratch.peeps.reg = reg;
}

// Peeps::ClearReg
void
Peeps::ClearReg(RegNum reg)
{LOGMEIN("Peeps.cpp] 396\n");
    StackSym *sym = this->regMap[reg];

    if (sym)
    {LOGMEIN("Peeps.cpp] 400\n");
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
{LOGMEIN("Peeps.cpp] 413\n");
    if(peepedRef)
    {LOGMEIN("Peeps.cpp] 415\n");
        *peepedRef = false;
    }

    IR::LabelInstr *targetInstr = branchInstr->GetTarget();
    IR::Instr *instrNext;

    if (branchInstr->IsUnconditional())
    {LOGMEIN("Peeps.cpp] 423\n");
        // Cleanup unreachable code after unconditional branch
        instrNext = RemoveDeadBlock(branchInstr->m_next);
    }

    instrNext = branchInstr->GetNextRealInstrOrLabel();

    if (instrNext != NULL && instrNext->IsLabelInstr())
    {LOGMEIN("Peeps.cpp] 431\n");
        //
        // Remove branch-to-next
        //
        if (targetInstr == instrNext)
        {LOGMEIN("Peeps.cpp] 436\n");
            if (!branchInstr->IsLowered())
            {LOGMEIN("Peeps.cpp] 438\n");
                if (branchInstr->HasAnyImplicitCalls())
                {LOGMEIN("Peeps.cpp] 440\n");
                    Assert(!branchInstr->m_func->GetJITFunctionBody()->IsAsmJsMode());
                    // if (x > y) might trigger a call to valueof() or something for x and y.
                    // We can't just delete them.
                    Js::OpCode newOpcode;
                    switch(branchInstr->m_opcode)
                    {LOGMEIN("Peeps.cpp] 446\n");
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
                {LOGMEIN("Peeps.cpp] 492\n");
                    // We only have cases with one src
                    Assert(branchInstr->GetSrc1()->IsRegOpnd());

                    IR::RegOpnd *regSrc = branchInstr->GetSrc1()->AsRegOpnd();
                    StackSym *symSrc = regSrc->GetStackSym();

                    if (symSrc->HasByteCodeRegSlot() && !regSrc->GetIsJITOptimizedReg())
                    {LOGMEIN("Peeps.cpp] 500\n");
                        // No side-effects to worry about, but need to insert bytecodeUse.
                        IR::ByteCodeUsesInstr *byteCodeUsesInstr = IR::ByteCodeUsesInstr::New(branchInstr);
                        byteCodeUsesInstr->Set(regSrc);
                        branchInstr->InsertBefore(byteCodeUsesInstr);
                    }
                }
            }
            // Note: if branch is conditional, we have a dead test/cmp left behind...
            if(peepedRef)
            {LOGMEIN("Peeps.cpp] 510\n");
                *peepedRef = true;
            }
            branchInstr->Remove();
            if (targetInstr->IsUnreferenced())
            {LOGMEIN("Peeps.cpp] 515\n");
                // We may have exposed an unreachable label by deleting the branch
                instrNext = Peeps::PeepUnreachableLabel(targetInstr, false);
            }
            else
            {
                instrNext = targetInstr;
            }
            IR::Instr *instrPrev = instrNext->GetPrevRealInstrOrLabel();
            if (instrPrev->IsBranchInstr())
            {LOGMEIN("Peeps.cpp] 525\n");
                // The branch removal could have exposed a branch to next opportunity.
                return Peeps::PeepBranch(instrPrev->AsBranchInstr());
            }
            return instrNext;
        }
    }
    else if (branchInstr->IsConditional())
    {LOGMEIN("Peeps.cpp] 533\n");
        AnalysisAssert(instrNext);
        if (instrNext->IsBranchInstr()
            && instrNext->AsBranchInstr()->IsUnconditional()
            && targetInstr == instrNext->AsBranchInstr()->GetNextRealInstrOrLabel()
            && !instrNext->AsBranchInstr()->IsMultiBranch())
        {LOGMEIN("Peeps.cpp] 539\n");
            //
            // Invert condBranch/uncondBranch/label:
            //
            //      JCC L1                   JinvCC L3
            //      JMP L3       =>
            //      L1:
            IR::BranchInstr *uncondBranch = instrNext->AsBranchInstr();

            if (branchInstr->IsLowered())
            {LOGMEIN("Peeps.cpp] 549\n");
                LowererMD::InvertBranch(branchInstr);
            }
            else
            {
                branchInstr->Invert();
            }

            targetInstr = uncondBranch->GetTarget();
            branchInstr->SetTarget(targetInstr);
            if (targetInstr->IsUnreferenced())
            {LOGMEIN("Peeps.cpp] 560\n");
                Peeps::PeepUnreachableLabel(targetInstr, false);
            }

            uncondBranch->Remove();

            return PeepBranch(branchInstr, peepedRef);
        }
    }

    if(branchInstr->IsMultiBranch())
    {LOGMEIN("Peeps.cpp] 571\n");
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
{LOGMEIN("Peeps.cpp] 622\n");
    Assert(branchInstr);
    if (!branchInstr->IsConditional() || branchInstr->IsMultiBranch() || !branchInstr->IsLowered())
    {LOGMEIN("Peeps.cpp] 625\n");
        return instrNext;   // this optimization only applies to single conditional branch
    }

    IR::LabelInstr *targetLabel = branchInstr->GetTarget();
    Assert(targetLabel);

    // give up if there are other branch entries to the target label
    if (targetLabel->labelRefs.Count() > 1)
    {LOGMEIN("Peeps.cpp] 634\n");
        return instrNext;
    }

    // Give up if previous instruction before target label has fallthrough, cannot hoist up
    IR::Instr *targetPrev = targetLabel->GetPrevRealInstrOrLabel();
    Assert(targetPrev);
    if (targetPrev->HasFallThrough())
    {LOGMEIN("Peeps.cpp] 642\n");
        return instrNext;
    }

    IR::Instr *instrSetCondition = NULL;
    IR::Instr *branchPrev = branchInstr->GetPrevRealInstrOrLabel();
    while (branchPrev && !branchPrev->StartsBasicBlock())
    {LOGMEIN("Peeps.cpp] 649\n");
        if (!instrSetCondition && EncoderMD::SetsConditionCode(branchPrev))
        {LOGMEIN("Peeps.cpp] 651\n");   // located compare instruction for the branch
            instrSetCondition = branchPrev;
        }
        branchPrev = branchPrev->GetPrevRealInstrOrLabel(); // keep looking previous instr in BB
    }

    if (branchPrev && branchPrev->IsLabelInstr() && branchPrev->AsLabelInstr()->isOpHelper)
    {LOGMEIN("Peeps.cpp] 658\n");   // don't apply the optimization when branch is in helper section
        return instrNext;
    }

    if (!instrSetCondition)
    {LOGMEIN("Peeps.cpp] 663\n");   // give up if we cannot find the compare instruction in the BB, should be very rare
        return instrNext;
    }
    Assert(instrSetCondition);

    bool hoistAboveSetConditionInstr = false;
    if (instrSetCondition == branchInstr->GetPrevRealInstrOrLabel())
    {LOGMEIN("Peeps.cpp] 670\n");   // if compare instruction is right before branch instruction, we can hoist above cmp instr
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
    {LOGMEIN("Peeps.cpp] 687\n");
        branchNextInstr = instr->GetNextRealInstrOrLabel();
        targetNextInstr = targetInstr->GetNextRealInstrOrLabel();

        instr->Unlink();                            // hoist up instr in fallthrough branch
        if (hoistAboveSetConditionInstr)
        {LOGMEIN("Peeps.cpp] 693\n");
            instrSetCondition->InsertBefore(instr); // hoist above compare instruction
        }
        else
        {
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
    {LOGMEIN("Peeps.cpp] 711\n");   // instructions have been hoisted, now check tail branch to see if it can be duplicated
        if (targetInstr->IsBranchInstr())
        {LOGMEIN("Peeps.cpp] 713\n");
            IR::BranchInstr *tailBranch = targetInstr->AsBranchInstr();
            if (tailBranch->IsUnconditional() && !tailBranch->IsMultiBranch())
            {LOGMEIN("Peeps.cpp] 716\n");   // target can be replaced since tail branch is a single unconditional jmp
                branchInstr->ReplaceTarget(targetLabel, tailBranch->GetTarget());
            }

            // now targeLabel is an empty Basic Block, remove it if it's not referenced
            if (targetLabel->IsUnreferenced())
            {LOGMEIN("Peeps.cpp] 722\n");
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
{LOGMEIN("Peeps.cpp] 735\n");
    AnalysisAssert(targetInstr);
    IR::Instr *targetInstrNext = targetInstr->GetNextRealInstr();
    AnalysisAssertMsg(targetInstrNext, "GetNextRealInstr() failed to get next target");

    // Removing branch to branch breaks some lexical assumptions about loop in sccliveness/linearscan/second chance.
    if (!branchInstr->IsLowered())
    {LOGMEIN("Peeps.cpp] 742\n");
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
    {LOGMEIN("Peeps.cpp] 756\n");
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
        {LOGMEIN("Peeps.cpp] 772\n");
            branchAtTarget = targetInstrNext->AsBranchInstr();
        }
        else
        {
            // We don't have the information here to decide whether or not to continue the branch chain.
            break;
        }
        // This used to just be a targetInstrNext->AsBranchInstr()->IsUnconditional(), but, in order
        // to optimize further, it became necessary to handle more than just unconditional jumps. In
        // order to keep the code relatively clean, the "is it an inherently-taken jump chain" check
        // code now follows here:
        if (!targetInstrNext->AsBranchInstr()->IsUnconditional())
        {LOGMEIN("Peeps.cpp] 785\n");
            bool safetofollow = false;
            if(targetInstrNext->m_opcode == branchInstr->m_opcode)
            {LOGMEIN("Peeps.cpp] 788\n");
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
                {LOGMEIN("Peeps.cpp] 803\n");
                    safetofollow = true;
                }
            }
            if (!safetofollow)
            {LOGMEIN("Peeps.cpp] 808\n");
                // We can't say safely that this branch is something that we can implicitly take, so instead
                // cut off the branch chain optimization here.
                break;
            }
        }

        // We don't want to skip the loop entry, unless we're right before the encoder
        if (targetInstr->m_isLoopTop && !branchAtTarget->IsLowered())
        {LOGMEIN("Peeps.cpp] 817\n");
            break;
        }

        if (targetInstr->m_isLoopTop)
        {LOGMEIN("Peeps.cpp] 822\n");
            if (targetInstr == lastLoopTop)
            {LOGMEIN("Peeps.cpp] 824\n");
                // We are back to a loopTop already visited.
                // Looks like an infinite loop somewhere...
                break;
            }
            lastLoopTop = targetInstr;
        }
#if DBG
        if (!branchInstr->IsMultiBranch() && branchInstr->GetTarget()->m_noHelperAssert && !branchAtTarget->IsMultiBranch())
        {LOGMEIN("Peeps.cpp] 833\n");
            branchAtTarget->GetTarget()->m_noHelperAssert = true;
        }

        AssertMsg(++counter < 10000, "We should not be looping this many times!");
#endif

        IR::LabelInstr * reTargetLabel = branchAtTarget->GetTarget();
        AnalysisAssert(reTargetLabel);
        if (targetInstr == reTargetLabel)
        {LOGMEIN("Peeps.cpp] 843\n");
            // Infinite loop.
            //    JCC $L1
            // L1:
            //    JMP $L1
            break;
        }

        if(branchInstr->IsMultiBranch())
        {LOGMEIN("Peeps.cpp] 852\n");
            IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();
            multiBranchInstr->ChangeLabelRef(targetInstr, reTargetLabel);
        }
        else
        {
            branchInstr->SetTarget(reTargetLabel);
        }

        if (targetInstr->IsUnreferenced())
        {LOGMEIN("Peeps.cpp] 862\n");
            Peeps::PeepUnreachableLabel(targetInstr, false);
        }

        targetInstr = reTargetLabel;
        targetInstrNext = targetInstr->GetNextRealInstr();
    }
    return targetInstr;
}

IR::Instr *
Peeps::PeepUnreachableLabel(IR::LabelInstr *deadLabel, const bool isInHelper, bool *const peepedRef)
{LOGMEIN("Peeps.cpp] 874\n");
    Assert(deadLabel);
    Assert(deadLabel->IsUnreferenced());

    IR::Instr *prevFallthroughInstr = deadLabel;
    do
    {LOGMEIN("Peeps.cpp] 880\n");
        prevFallthroughInstr = prevFallthroughInstr->GetPrevRealInstrOrLabel();
        // The previous dead label may have been kept around due to a StatementBoundary, see comment in RemoveDeadBlock.
    } while(prevFallthroughInstr->IsLabelInstr() && prevFallthroughInstr->AsLabelInstr()->IsUnreferenced());

    IR::Instr *instrReturn;
    bool removeLabel;

    // If code is now unreachable, delete block
    if (!prevFallthroughInstr->HasFallThrough())
    {LOGMEIN("Peeps.cpp] 890\n");
        bool wasStatementBoundaryKeptInDeadBlock = false;
        instrReturn = RemoveDeadBlock(deadLabel->m_next, &wasStatementBoundaryKeptInDeadBlock);

        // Remove label only if we didn't have to keep last StatementBoundary in the dead block,
        // see comment in RemoveDeadBlock.
        removeLabel = !wasStatementBoundaryKeptInDeadBlock;

        if(peepedRef)
        {LOGMEIN("Peeps.cpp] 899\n");
            *peepedRef = true;
        }
    }
    else
    {
        instrReturn = deadLabel->m_next;
        removeLabel =
            deadLabel->isOpHelper == isInHelper
#if DBG
            && !deadLabel->m_noHelperAssert
#endif
            ;
        if(peepedRef)
        {LOGMEIN("Peeps.cpp] 913\n");
            *peepedRef = removeLabel;
        }
    }

    if (removeLabel && deadLabel->IsUnreferenced())
    {LOGMEIN("Peeps.cpp] 919\n");
        deadLabel->Remove();
    }

    return instrReturn;
}

IR::Instr *
Peeps::CleanupLabel(IR::LabelInstr * instr, IR::LabelInstr * instrNext)
{LOGMEIN("Peeps.cpp] 928\n");
    IR::Instr * returnInstr;

    IR::LabelInstr * labelToRemove;
    IR::LabelInstr * labelToKeep;

    // Just for dump, always keep loop top labels
    // We also can remove label that has non branch references
    if (instrNext->m_isLoopTop || instrNext->m_hasNonBranchRef)
    {LOGMEIN("Peeps.cpp] 937\n");
        if (instr->m_isLoopTop || instr->m_hasNonBranchRef)
        {LOGMEIN("Peeps.cpp] 939\n");
            // Don't remove loop top labels or labels with non branch references
            return instrNext;
        }
        labelToRemove = instr;
        labelToKeep = instrNext;
        returnInstr = instrNext;
    }
    else
    {
        labelToRemove = instrNext;
        labelToKeep = instr;
        returnInstr = instrNext->m_next;
    }
    while (!labelToRemove->labelRefs.Empty())
    {LOGMEIN("Peeps.cpp] 954\n");
        bool replaced = labelToRemove->labelRefs.Head()->ReplaceTarget(labelToRemove, labelToKeep);
        Assert(replaced);
    }

    if (labelToRemove->isOpHelper)
    {LOGMEIN("Peeps.cpp] 960\n");
        labelToKeep->isOpHelper = true;
#if DBG
        if (labelToRemove->m_noHelperAssert)
        {LOGMEIN("Peeps.cpp] 964\n");
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
{LOGMEIN("Peeps.cpp] 987\n");
    IR::Instr* lastStatementBoundary = nullptr;

    while (instr && !instr->IsLabelInstr() && !instr->IsExitInstr())
    {LOGMEIN("Peeps.cpp] 991\n");
        IR::Instr *deadInstr = instr;
        instr = instr->m_next;

        if (deadInstr->IsPragmaInstr() && deadInstr->m_opcode == Js::OpCode::StatementBoundary)
        {LOGMEIN("Peeps.cpp] 996\n");
            if (lastStatementBoundary)
            {LOGMEIN("Peeps.cpp] 998\n");
                //Its enough if we keep latest statement boundary. Rest are dead anyway.
                lastStatementBoundary->Remove();
            }
            lastStatementBoundary = deadInstr;
        }
        else
        {
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
    {LOGMEIN("Peeps.cpp] 1018\n");
        lastStatementBoundary->Unlink();
        instr->InsertAfter(lastStatementBoundary);
    }

    if (wasStmtBoundaryKeptInDeadBlock)
    {LOGMEIN("Peeps.cpp] 1024\n");
        *wasStmtBoundaryKeptInDeadBlock = lastStatementBoundary && !canMoveStatementBoundaryUnderNextLabel;
    }

    return instr;
}

// Shared code for x86 and amd64
#if defined(_M_IX86) || defined(_M_X64)
IR::Instr *
Peeps::PeepRedundant(IR::Instr *instr)
{LOGMEIN("Peeps.cpp] 1035\n");
    IR::Instr *retInstr = instr;

    if (instr->m_opcode == Js::OpCode::ADD || instr->m_opcode == Js::OpCode::SUB || instr->m_opcode == Js::OpCode::OR)
    {LOGMEIN("Peeps.cpp] 1039\n");
        Assert(instr->GetSrc1() && instr->GetSrc2());
        if( (instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() == 0))
        {LOGMEIN("Peeps.cpp] 1042\n");
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
    {LOGMEIN("Peeps.cpp] 1055\n");
        // dummy def used for non-32bit ovf check for IMUL
        // check edx is not killed between IMUL and edx = NOP, then remove the NOP
        bool found = false;
        IR::Instr *nopInstr = instr;
        do
        {LOGMEIN("Peeps.cpp] 1061\n");
            instr = instr->GetPrevRealInstrOrLabel();
            if (
                instr->m_opcode == Js::OpCode::IMUL ||
                (instr->m_opcode == Js::OpCode::CALL && this->func->GetJITFunctionBody()->IsWasmFunction())
            )
            {LOGMEIN("Peeps.cpp] 1067\n");
                found = true;
                break;
            }
        } while(!instr->StartsBasicBlock());

        if (found)
        {LOGMEIN("Peeps.cpp] 1074\n");
            retInstr = nopInstr->m_next;
            nopInstr->Remove();
        }
        else
        {
            instr = nopInstr;
            do
            {LOGMEIN("Peeps.cpp] 1082\n");
                instr = instr->GetNextRealInstrOrLabel();
                if (instr->m_opcode == Js::OpCode::DIV)
                {LOGMEIN("Peeps.cpp] 1085\n");
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
{LOGMEIN("Peeps.cpp] 1112\n");
    IR::Instr *instr = labelInstr->GetPrevRealInstrOrLabel();

    Js::OpCode newOpCode;

    // Check if BB is all MOVs with both RegOpnd
    while(instr->m_opcode == Js::OpCode::MOV)
    {LOGMEIN("Peeps.cpp] 1119\n");
        if (!instr->GetSrc1()->IsRegOpnd() || !instr->GetDst()->IsRegOpnd())
            return nextInstr;
        instr = instr->GetPrevRealInstrOrLabel();
    }

    // Did we hit a conditional branch ?
    if (instr->IsBranchInstr() && instr->AsBranchInstr()->IsConditional() &&
        !instr->AsBranchInstr()->IsMultiBranch() &&
        instr->AsBranchInstr()->GetTarget() == labelInstr &&
        instr->m_opcode != Js::OpCode::Leave)
    {LOGMEIN("Peeps.cpp] 1130\n");
        IR::BranchInstr *brInstr = instr->AsBranchInstr();

        // Get the correct CMOVcc
        switch(brInstr->m_opcode)
        {LOGMEIN("Peeps.cpp] 1135\n");
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
        {LOGMEIN("Peeps.cpp] 1192\n");
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
