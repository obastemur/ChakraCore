//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

#if DBG

void
DbCheckPostLower::Check()
{TRACE_IT(1617);
    bool doOpHelperCheck = Js::Configuration::Global.flags.CheckOpHelpers && !this->func->isPostLayout;
    bool isInHelperBlock = false;

    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {TRACE_IT(1618);
        Assert(Lowerer::ValidOpcodeAfterLower(instr, this->func));
        LowererMD::Legalize</*verify*/true>(instr);
        switch(instr->GetKind())
        {
        case IR::InstrKindLabel:
        case IR::InstrKindProfiledLabel:
            isInHelperBlock = instr->AsLabelInstr()->isOpHelper;
            if (doOpHelperCheck && !isInHelperBlock && !instr->AsLabelInstr()->m_noHelperAssert)
            {TRACE_IT(1619);
                bool foundNonHelperPath = false;
                bool isDeadLabel = true;

                IR::LabelInstr* labelInstr = instr->AsLabelInstr();

                while (1)
                {TRACE_IT(1620);
                    FOREACH_SLIST_ENTRY(IR::BranchInstr *, branchInstr, &labelInstr->labelRefs)
                    {TRACE_IT(1621);
                        isDeadLabel = false;
                        IR::Instr *instrPrev = branchInstr->m_prev;
                        while (instrPrev && !instrPrev->IsLabelInstr())
                        {TRACE_IT(1622);
                            instrPrev = instrPrev->m_prev;
                        }
                        if (!instrPrev || !instrPrev->AsLabelInstr()->isOpHelper || branchInstr->m_isHelperToNonHelperBranch)
                        {TRACE_IT(1623);
                            foundNonHelperPath = true;
                            break;
                        }
                    } NEXT_SLIST_ENTRY;

                    if (!labelInstr->m_next->IsLabelInstr())
                    {TRACE_IT(1624);
                        break;
                    }
                    IR::LabelInstr *const nextLabel = labelInstr->m_next->AsLabelInstr();

                    // It is generally not expected for a non-helper label to be immediately followed by a helper label. Some
                    // special cases may flag the helper label with m_noHelperAssert = true. Peeps can cause non-helper blocks
                    // to fall through into helper blocks, so skip this check after peeps.
                    Assert(func->isPostPeeps || nextLabel->m_noHelperAssert || !nextLabel->isOpHelper);

                    if(nextLabel->isOpHelper)
                    {TRACE_IT(1625);
                        break;
                    }
                    labelInstr = nextLabel;
                }

                instrNext = labelInstr->m_next;

                // This label is unreachable or at least one path to it is not from a helper block.

                if (!foundNonHelperPath && !instr->GetNextRealInstrOrLabel()->IsExitInstr() && !isDeadLabel)
                {TRACE_IT(1626);
                    IR::Instr *prevInstr = labelInstr->GetPrevRealInstrOrLabel();
                    if (prevInstr->HasFallThrough() && !(prevInstr->IsBranchInstr() && prevInstr->AsBranchInstr()->m_isHelperToNonHelperBranch))
                    {TRACE_IT(1627);
                        while (prevInstr && !prevInstr->IsLabelInstr())
                        {TRACE_IT(1628);
                            prevInstr = prevInstr->m_prev;
                        }

                        AssertMsg(prevInstr && prevInstr->IsLabelInstr() && !prevInstr->AsLabelInstr()->isOpHelper, "Inconsistency in Helper label annotations");
                    }
                }
            }
            break;

        case IR::InstrKindBranch:
            if (doOpHelperCheck && !isInHelperBlock)
            {TRACE_IT(1629);
                IR::LabelInstr *targetLabel = instr->AsBranchInstr()->GetTarget();

                // This branch needs a path to a non-helper block.
                if (instr->AsBranchInstr()->IsConditional())
                {TRACE_IT(1630);
                    if (targetLabel->isOpHelper && !targetLabel->m_noHelperAssert)
                    {TRACE_IT(1631);
                        IR::Instr *instrNextDebug = instr->GetNextRealInstrOrLabel();
                        Assert(!(instrNextDebug->IsLabelInstr() && instrNextDebug->AsLabelInstr()->isOpHelper));
                    }
                }
                else
                {TRACE_IT(1632);
                    Assert(instr->AsBranchInstr()->IsUnconditional());

                    if (targetLabel)
                    {TRACE_IT(1633);
                        if (!targetLabel->isOpHelper || targetLabel->m_noHelperAssert)
                        {TRACE_IT(1634);
                            break;
                        }
                        // Target is opHelper

                        IR::Instr *instrPrev = instr->m_prev;

                        if (this->func->isPostRegAlloc)
                        {TRACE_IT(1635);
                            while (LowererMD::IsAssign(instrPrev))
                            {TRACE_IT(1636);
                                // Skip potential register allocation compensation code
                                instrPrev = instrPrev->m_prev;
                            }
                        }

                        if (instrPrev->m_opcode == Js::OpCode::DeletedNonHelperBranch)
                        {TRACE_IT(1637);
                            break;
                        }

                        Assert((instrPrev->IsBranchInstr() && instrPrev->AsBranchInstr()->IsConditional()
                            && (!instrPrev->AsBranchInstr()->GetTarget()->isOpHelper || instrPrev->AsBranchInstr()->GetTarget()->m_noHelperAssert)));
                    }
                    else
                    {TRACE_IT(1638);
                        Assert(instr->GetSrc1());
                    }
                }
            }
            break;

        default:
            this->Check(instr->GetDst());
            this->Check(instr->GetSrc1());
            this->Check(instr->GetSrc2());

#if defined(_M_IX86) || defined(_M_X64)
            // for op-eq's and assignment operators, make  sure the types match
            // for shift operators make sure the types match and the third is an 8-bit immediate
            // for cmp operators similarly check types are same
            if (EncoderMD::IsOPEQ(instr))
            {TRACE_IT(1639);
                Assert(instr->GetDst()->IsEqual(instr->GetSrc1()));

#if defined(_M_X64)
                Assert(!instr->GetSrc2() || instr->GetDst()->GetSize() == instr->GetSrc2()->GetSize() ||
                    ((EncoderMD::IsSHIFT(instr) || instr->m_opcode == Js::OpCode::BTR ||
                        instr->m_opcode == Js::OpCode::BTS ||
                        instr->m_opcode == Js::OpCode::BT) && instr->GetSrc2()->GetSize() == 1) ||
                    // Is src2 is TyVar and src1 is TyInt32/TyUint32, make sure the address fits in 32 bits 
                        (instr->GetSrc2()->GetType() == TyVar && instr->GetDst()->GetSize() == 4 &&
                         instr->GetSrc2()->IsAddrOpnd() && Math::FitsInDWord(reinterpret_cast<int64>(instr->GetSrc2()->AsAddrOpnd()->m_address))));
#else
                Assert(!instr->GetSrc2() || instr->GetDst()->GetSize() == instr->GetSrc2()->GetSize() ||
                    ((EncoderMD::IsSHIFT(instr) || instr->m_opcode == Js::OpCode::BTR ||
                        instr->m_opcode == Js::OpCode::BT) && instr->GetSrc2()->GetSize() == 1));
#endif
            }
            Assert(!LowererMD::IsAssign(instr) || instr->GetDst()->GetSize() == instr->GetSrc1()->GetSize());
            Assert(instr->m_opcode != Js::OpCode::CMP || instr->GetSrc1()->GetType() == instr->GetSrc1()->GetType());

            switch (instr->m_opcode)
            {
            case Js::OpCode::CMOVA:
            case Js::OpCode::CMOVAE:
            case Js::OpCode::CMOVB:
            case Js::OpCode::CMOVBE:
            case Js::OpCode::CMOVE:
            case Js::OpCode::CMOVG:
            case Js::OpCode::CMOVGE:
            case Js::OpCode::CMOVL:
            case Js::OpCode::CMOVLE:
            case Js::OpCode::CMOVNE:
            case Js::OpCode::CMOVNO:
            case Js::OpCode::CMOVNP:
            case Js::OpCode::CMOVNS:
            case Js::OpCode::CMOVO:
            case Js::OpCode::CMOVP:
            case Js::OpCode::CMOVS:
                if (instr->GetSrc2())
                {TRACE_IT(1640);
                    // CMOV inserted before regAlloc need a fake use of the dst register to make up for the
                    // fact that the CMOV may not set the dst. Regalloc needs to assign the same physical register for dst and src1.
                    Assert(instr->GetDst()->IsEqual(instr->GetSrc1()));
                }
                else
                {TRACE_IT(1641);
                    // These must have been inserted post-regalloc.
                    Assert(instr->GetDst()->AsRegOpnd()->GetReg() != RegNOREG);
                }
                break;
            case Js::OpCode::CALL:
                Assert(!instr->m_func->IsTrueLeaf());
                break;
            }
#endif
        }
    } NEXT_INSTR_IN_FUNC_EDITING;
}

void DbCheckPostLower::Check(IR::Opnd *opnd)
{TRACE_IT(1642);
    if (opnd == NULL)
    {TRACE_IT(1643);
        return;
    }

    if (opnd->IsRegOpnd())
    {TRACE_IT(1644);
        this->Check(opnd->AsRegOpnd());
    }
    else if (opnd->IsIndirOpnd())
    {TRACE_IT(1645);
        this->Check(opnd->AsIndirOpnd()->GetBaseOpnd());
        this->Check(opnd->AsIndirOpnd()->GetIndexOpnd());
    }
    else if (opnd->IsSymOpnd() && opnd->AsSymOpnd()->m_sym->IsStackSym())
    {TRACE_IT(1646);
        if (this->func->isPostRegAlloc)
        {TRACE_IT(1647);
            AssertMsg(opnd->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated(), "No Stack space allocated for StackSym?");
        }
        IRType symType = opnd->AsSymOpnd()->m_sym->AsStackSym()->GetType();
        if (symType != TyMisc)
        {TRACE_IT(1648);
            uint symSize = static_cast<uint>(max(TySize[symType], MachRegInt));
            AssertMsg(static_cast<uint>(TySize[opnd->AsSymOpnd()->GetType()]) + opnd->AsSymOpnd()->m_offset <= symSize, "SymOpnd cannot refer to a size greater than Sym's reference");
        }
    }
}

void DbCheckPostLower::Check(IR::RegOpnd *regOpnd)
{TRACE_IT(1649);
    if (regOpnd == NULL)
    {TRACE_IT(1650);
        return;
    }

    RegNum reg = regOpnd->GetReg();
    if (reg != RegNOREG)
    {TRACE_IT(1651);
        if (IRType_IsFloat(LinearScan::GetRegType(reg)))
        {TRACE_IT(1652);
            // both simd128 and float64 map to float64 regs
            Assert(IRType_IsFloat(regOpnd->GetType()) || IRType_IsSimd128(regOpnd->GetType()));
        }
        else
        {TRACE_IT(1653);
            Assert(IRType_IsNativeInt(regOpnd->GetType()) || regOpnd->GetType() == TyVar);
#if defined(_M_IX86) || defined(_M_X64)
            if (regOpnd->GetSize() == 1)
            {TRACE_IT(1654);
                Assert(LinearScan::GetRegAttribs(reg) & RA_BYTEABLE);
            }
#endif
        }
    }
}

#endif // DBG
