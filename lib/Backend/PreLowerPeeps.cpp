//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

IR::Instr *Lowerer::PreLowerPeepInstr(IR::Instr *instr, IR::Instr **pInstrPrev)
{TRACE_IT(15040);
    if (PHASE_OFF(Js::PreLowererPeepsPhase, this->m_func))
    {TRACE_IT(15041);
        return instr;
    }

    switch (instr->m_opcode)
    {
    case Js::OpCode::Shl_I4:
        instr = this->PeepShl(instr);
        *pInstrPrev = instr->m_prev;
        break;

    case Js::OpCode::BrTrue_I4:
    case Js::OpCode::BrFalse_I4:
        instr = this->PeepBrBool(instr);
        *pInstrPrev = instr->m_prev;
        break;
    }

    return instr;
}

IR::Instr *Lowerer::PeepShl(IR::Instr *instrShl)
{TRACE_IT(15042);
    IR::Opnd *src1;
    IR::Opnd *src2;
    IR::Instr *instrDef;

    src1 = instrShl->GetSrc1();
    src2 = instrShl->GetSrc2();

    // Peep:
    //    t1 = SHR X, cst
    //    t2 = SHL t1, cst
    //
    // Into:
    //    t2 = AND X, mask

    if (!src1->IsRegOpnd() || !src2->IsIntConstOpnd())
    {TRACE_IT(15043);
        return instrShl;
    }
    if (!src1->AsRegOpnd()->m_sym->IsSingleDef())
    {TRACE_IT(15044);
        return instrShl;
    }
    if (instrShl->HasBailOutInfo())
    {TRACE_IT(15045);
        return instrShl;
    }
    instrDef = src1->AsRegOpnd()->m_sym->GetInstrDef();
    if (instrDef->m_opcode != Js::OpCode::Shr_I4 || !instrDef->GetSrc2()->IsIntConstOpnd()
        || instrDef->GetSrc2()->AsIntConstOpnd()->GetValue() != src2->AsIntConstOpnd()->GetValue()
        || !instrDef->GetSrc1()->IsRegOpnd())
    {TRACE_IT(15046);
        return instrShl;
    }
    if (!src1->GetIsDead())
    {TRACE_IT(15047);
        return instrShl;
    }
    if (instrDef->HasBailOutInfo())
    {TRACE_IT(15048);
        return instrShl;
    }

    FOREACH_INSTR_IN_RANGE(instrIter, instrDef->m_next, instrShl->m_prev)
    {TRACE_IT(15049);
        if (instrIter->HasBailOutInfo())
        {TRACE_IT(15050);
            return instrShl;
        }
        if (instrIter->FindRegDef(instrDef->GetSrc1()->AsRegOpnd()->m_sym))
        {TRACE_IT(15051);
            return instrShl;
        }
        if (instrIter->FindRegUse(src1->AsRegOpnd()->m_sym))
        {TRACE_IT(15052);
            return instrShl;
        }
        // if any branch between def-use, don't do peeps on it because branch target might depend on the def
        if (instrIter->IsBranchInstr())
        {TRACE_IT(15053);
            return instrShl;
        }
    } NEXT_INSTR_IN_RANGE;

    instrShl->FreeSrc1();
    instrShl->SetSrc1(instrDef->UnlinkSrc1());
    instrDef->Remove();

    IntConstType oldValue = src2->AsIntConstOpnd()->GetValue();

    // Left shift operator (<<) on arm32 is implemented by LSL which doesn't discard bits beyond lowerest 5-bit.
    // Need to discard such bits to conform to << in JavaScript. This is not a problem for x86 and x64 because
    // behavior of SHL is consistent with JavaScript but keep the below line for clarity.
    oldValue %= sizeof(int32) * 8;

    oldValue = ~((1 << oldValue) - 1);
    src2->AsIntConstOpnd()->SetValue(oldValue);

    instrShl->m_opcode = Js::OpCode::And_I4;

    return instrShl;
}

IR::Instr *Lowerer::PeepBrBool(IR::Instr *instrBr)
{TRACE_IT(15054);
    IR::Opnd *src1;
    IR::Instr *instrBinOp, *instrCm1, *instrCm2;

    // Peep:
    //    t1 = CmCC_I4 a, b
    //    t2 = CmCC_i4 c, d
    //    t3 = AND/OR t1, t2
    //    BrTrue/False t3, $L_true
    //
    // Into:
    //    BrCC a, b,  $L_true/false
    //    BrCC c, d,  $L_true
    //$L_false:

    src1 = instrBr->GetSrc1();
    if (!src1->IsRegOpnd())
    {TRACE_IT(15055);
        return instrBr;
    }
    Assert(!instrBr->HasBailOutInfo());

    instrBinOp = instrBr->GetPrevRealInstrOrLabel();
    if (instrBinOp->m_opcode != Js::OpCode::And_I4 && instrBinOp->m_opcode != Js::OpCode::Or_I4)
    {TRACE_IT(15056);
        return instrBr;
    }
    if (!instrBinOp->GetDst()->IsEqual(src1))
    {TRACE_IT(15057);
        return instrBr;
    }
    IR::RegOpnd *src1Reg = src1->AsRegOpnd();
    if (!src1Reg->m_sym->IsSingleDef() || !src1Reg->GetIsDead())
    {TRACE_IT(15058);
        return instrBr;
    }
    Assert(!instrBinOp->HasBailOutInfo());

    instrCm2 = instrBinOp->GetPrevRealInstrOrLabel();
    if (!instrCm2->IsCmCC_I4())
    {TRACE_IT(15059);
        return instrBr;
    }
    IR::RegOpnd *cm2DstReg = instrCm2->GetDst()->AsRegOpnd();
    if (!cm2DstReg->m_sym->IsSingleDef())
    {TRACE_IT(15060);
        return instrBr;
    }
    if (cm2DstReg->IsEqual(instrBinOp->GetSrc1()))
    {TRACE_IT(15061);
        if (!instrBinOp->GetSrc1()->AsRegOpnd()->GetIsDead())
        {TRACE_IT(15062);
            return instrBr;
        }
    }
    else if (cm2DstReg->IsEqual(instrBinOp->GetSrc2()))
    {TRACE_IT(15063);
        if (!instrBinOp->GetSrc2()->AsRegOpnd()->GetIsDead())
        {TRACE_IT(15064);
            return instrBr;
        }
    }
    else
    {TRACE_IT(15065);
        return instrBr;
    }

    Assert(!instrCm2->HasBailOutInfo());

    instrCm1 = instrCm2->GetPrevRealInstrOrLabel();
    if (!instrCm1->IsCmCC_I4())
    {TRACE_IT(15066);
        return instrBr;
    }
    Assert(!instrCm1->GetDst()->IsEqual(instrCm2->GetDst()));

    IR::RegOpnd *cm1DstReg = instrCm1->GetDst()->AsRegOpnd();
    if (!cm1DstReg->m_sym->IsSingleDef())
    {TRACE_IT(15067);
        return instrBr;
    }
    if (cm1DstReg->IsEqual(instrBinOp->GetSrc1()))
    {TRACE_IT(15068);
        if (!instrBinOp->GetSrc1()->AsRegOpnd()->GetIsDead())
        {TRACE_IT(15069);
            return instrBr;
        }
    }
    else if (cm1DstReg->IsEqual(instrBinOp->GetSrc2()))
    {TRACE_IT(15070);
        if (!instrBinOp->GetSrc2()->AsRegOpnd()->GetIsDead())
        {TRACE_IT(15071);
            return instrBr;
        }
    }
    else
    {TRACE_IT(15072);
        return instrBr;
    }

    Assert(!instrCm1->HasBailOutInfo());

    IR::LabelInstr *falseLabel = instrBr->AsBranchInstr()->GetTarget();
    IR::LabelInstr *trueLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instrBr->InsertAfter(trueLabel);
    IR::BranchInstr *instrBr1;
    IR::BranchInstr *instrBr2;

    if (instrBinOp->m_opcode == Js::OpCode::And_I4)
    {TRACE_IT(15073);
        instrBr1 = instrCm1->ChangeCmCCToBranchInstr(instrBr->m_opcode == Js::OpCode::BrFalse_I4 ? falseLabel : trueLabel);
        instrBr1->Invert();
        instrBr2 = instrCm2->ChangeCmCCToBranchInstr(falseLabel);
        if (instrBr->m_opcode == Js::OpCode::BrFalse_I4)
        {TRACE_IT(15074);
            instrBr2->Invert();
        }
    }
    else
    {TRACE_IT(15075);
        Assert(instrBinOp->m_opcode == Js::OpCode::Or_I4);

        instrBr1 = instrCm1->ChangeCmCCToBranchInstr(instrBr->m_opcode == Js::OpCode::BrTrue_I4 ? falseLabel : trueLabel);
        instrBr2 = instrCm2->ChangeCmCCToBranchInstr(falseLabel);
        if (instrBr->m_opcode == Js::OpCode::BrFalse_I4)
        {TRACE_IT(15076);
            instrBr2->Invert();
        }
    }

    instrBinOp->Remove();
    instrBr->Remove();

    return instrBr2;
}
