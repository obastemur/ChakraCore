//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

IR::Instr *Lowerer::PreLowerPeepInstr(IR::Instr *instr, IR::Instr **pInstrPrev)
{LOGMEIN("PreLowerPeeps.cpp] 7\n");
    if (PHASE_OFF(Js::PreLowererPeepsPhase, this->m_func))
    {LOGMEIN("PreLowerPeeps.cpp] 9\n");
        return instr;
    }

    switch (instr->m_opcode)
    {LOGMEIN("PreLowerPeeps.cpp] 14\n");
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
{LOGMEIN("PreLowerPeeps.cpp] 31\n");
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
    {LOGMEIN("PreLowerPeeps.cpp] 47\n");
        return instrShl;
    }
    if (!src1->AsRegOpnd()->m_sym->IsSingleDef())
    {LOGMEIN("PreLowerPeeps.cpp] 51\n");
        return instrShl;
    }
    if (instrShl->HasBailOutInfo())
    {LOGMEIN("PreLowerPeeps.cpp] 55\n");
        return instrShl;
    }
    instrDef = src1->AsRegOpnd()->m_sym->GetInstrDef();
    if (instrDef->m_opcode != Js::OpCode::Shr_I4 || !instrDef->GetSrc2()->IsIntConstOpnd()
        || instrDef->GetSrc2()->AsIntConstOpnd()->GetValue() != src2->AsIntConstOpnd()->GetValue()
        || !instrDef->GetSrc1()->IsRegOpnd())
    {LOGMEIN("PreLowerPeeps.cpp] 62\n");
        return instrShl;
    }
    if (!src1->GetIsDead())
    {LOGMEIN("PreLowerPeeps.cpp] 66\n");
        return instrShl;
    }
    if (instrDef->HasBailOutInfo())
    {LOGMEIN("PreLowerPeeps.cpp] 70\n");
        return instrShl;
    }

    FOREACH_INSTR_IN_RANGE(instrIter, instrDef->m_next, instrShl->m_prev)
    {LOGMEIN("PreLowerPeeps.cpp] 75\n");
        if (instrIter->HasBailOutInfo())
        {LOGMEIN("PreLowerPeeps.cpp] 77\n");
            return instrShl;
        }
        if (instrIter->FindRegDef(instrDef->GetSrc1()->AsRegOpnd()->m_sym))
        {LOGMEIN("PreLowerPeeps.cpp] 81\n");
            return instrShl;
        }
        if (instrIter->FindRegUse(src1->AsRegOpnd()->m_sym))
        {LOGMEIN("PreLowerPeeps.cpp] 85\n");
            return instrShl;
        }
        // if any branch between def-use, don't do peeps on it because branch target might depend on the def
        if (instrIter->IsBranchInstr())
        {LOGMEIN("PreLowerPeeps.cpp] 90\n");
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
{LOGMEIN("PreLowerPeeps.cpp] 115\n");
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
    {LOGMEIN("PreLowerPeeps.cpp] 132\n");
        return instrBr;
    }
    Assert(!instrBr->HasBailOutInfo());

    instrBinOp = instrBr->GetPrevRealInstrOrLabel();
    if (instrBinOp->m_opcode != Js::OpCode::And_I4 && instrBinOp->m_opcode != Js::OpCode::Or_I4)
    {LOGMEIN("PreLowerPeeps.cpp] 139\n");
        return instrBr;
    }
    if (!instrBinOp->GetDst()->IsEqual(src1))
    {LOGMEIN("PreLowerPeeps.cpp] 143\n");
        return instrBr;
    }
    IR::RegOpnd *src1Reg = src1->AsRegOpnd();
    if (!src1Reg->m_sym->IsSingleDef() || !src1Reg->GetIsDead())
    {LOGMEIN("PreLowerPeeps.cpp] 148\n");
        return instrBr;
    }
    Assert(!instrBinOp->HasBailOutInfo());

    instrCm2 = instrBinOp->GetPrevRealInstrOrLabel();
    if (!instrCm2->IsCmCC_I4())
    {LOGMEIN("PreLowerPeeps.cpp] 155\n");
        return instrBr;
    }
    IR::RegOpnd *cm2DstReg = instrCm2->GetDst()->AsRegOpnd();
    if (!cm2DstReg->m_sym->IsSingleDef())
    {LOGMEIN("PreLowerPeeps.cpp] 160\n");
        return instrBr;
    }
    if (cm2DstReg->IsEqual(instrBinOp->GetSrc1()))
    {LOGMEIN("PreLowerPeeps.cpp] 164\n");
        if (!instrBinOp->GetSrc1()->AsRegOpnd()->GetIsDead())
        {LOGMEIN("PreLowerPeeps.cpp] 166\n");
            return instrBr;
        }
    }
    else if (cm2DstReg->IsEqual(instrBinOp->GetSrc2()))
    {LOGMEIN("PreLowerPeeps.cpp] 171\n");
        if (!instrBinOp->GetSrc2()->AsRegOpnd()->GetIsDead())
        {LOGMEIN("PreLowerPeeps.cpp] 173\n");
            return instrBr;
        }
    }
    else
    {
        return instrBr;
    }

    Assert(!instrCm2->HasBailOutInfo());

    instrCm1 = instrCm2->GetPrevRealInstrOrLabel();
    if (!instrCm1->IsCmCC_I4())
    {LOGMEIN("PreLowerPeeps.cpp] 186\n");
        return instrBr;
    }
    Assert(!instrCm1->GetDst()->IsEqual(instrCm2->GetDst()));

    IR::RegOpnd *cm1DstReg = instrCm1->GetDst()->AsRegOpnd();
    if (!cm1DstReg->m_sym->IsSingleDef())
    {LOGMEIN("PreLowerPeeps.cpp] 193\n");
        return instrBr;
    }
    if (cm1DstReg->IsEqual(instrBinOp->GetSrc1()))
    {LOGMEIN("PreLowerPeeps.cpp] 197\n");
        if (!instrBinOp->GetSrc1()->AsRegOpnd()->GetIsDead())
        {LOGMEIN("PreLowerPeeps.cpp] 199\n");
            return instrBr;
        }
    }
    else if (cm1DstReg->IsEqual(instrBinOp->GetSrc2()))
    {LOGMEIN("PreLowerPeeps.cpp] 204\n");
        if (!instrBinOp->GetSrc2()->AsRegOpnd()->GetIsDead())
        {LOGMEIN("PreLowerPeeps.cpp] 206\n");
            return instrBr;
        }
    }
    else
    {
        return instrBr;
    }

    Assert(!instrCm1->HasBailOutInfo());

    IR::LabelInstr *falseLabel = instrBr->AsBranchInstr()->GetTarget();
    IR::LabelInstr *trueLabel = IR::LabelInstr::New(Js::OpCode::Label, this->m_func);
    instrBr->InsertAfter(trueLabel);
    IR::BranchInstr *instrBr1;
    IR::BranchInstr *instrBr2;

    if (instrBinOp->m_opcode == Js::OpCode::And_I4)
    {LOGMEIN("PreLowerPeeps.cpp] 224\n");
        instrBr1 = instrCm1->ChangeCmCCToBranchInstr(instrBr->m_opcode == Js::OpCode::BrFalse_I4 ? falseLabel : trueLabel);
        instrBr1->Invert();
        instrBr2 = instrCm2->ChangeCmCCToBranchInstr(falseLabel);
        if (instrBr->m_opcode == Js::OpCode::BrFalse_I4)
        {LOGMEIN("PreLowerPeeps.cpp] 229\n");
            instrBr2->Invert();
        }
    }
    else
    {
        Assert(instrBinOp->m_opcode == Js::OpCode::Or_I4);

        instrBr1 = instrCm1->ChangeCmCCToBranchInstr(instrBr->m_opcode == Js::OpCode::BrTrue_I4 ? falseLabel : trueLabel);
        instrBr2 = instrCm2->ChangeCmCCToBranchInstr(falseLabel);
        if (instrBr->m_opcode == Js::OpCode::BrFalse_I4)
        {LOGMEIN("PreLowerPeeps.cpp] 240\n");
            instrBr2->Invert();
        }
    }

    instrBinOp->Remove();
    instrBr->Remove();

    return instrBr2;
}
