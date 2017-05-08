//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

// PeepsMD::Init
void
PeepsMD::Init(Peeps *peeps)
{TRACE_IT(18661);
    this->peeps = peeps;
}

// PeepsMD::ProcessImplicitRegs
// Note: only do calls for now
void
PeepsMD::ProcessImplicitRegs(IR::Instr *instr)
{TRACE_IT(18662);
    if (LowererMD::IsCall(instr))
    {TRACE_IT(18663);
        this->peeps->ClearReg(RegEAX);
        this->peeps->ClearReg(RegECX);
        this->peeps->ClearReg(RegEDX);

        this->peeps->ClearReg(RegXMM0);
        this->peeps->ClearReg(RegXMM1);
        this->peeps->ClearReg(RegXMM2);
        this->peeps->ClearReg(RegXMM3);
        this->peeps->ClearReg(RegXMM4);
        this->peeps->ClearReg(RegXMM5);
        this->peeps->ClearReg(RegXMM6);
        this->peeps->ClearReg(RegXMM7);
    }
    else if (instr->m_opcode == Js::OpCode::IMUL)
    {TRACE_IT(18664);
        this->peeps->ClearReg(RegEDX);
    }
    else if (instr->m_opcode == Js::OpCode::IDIV || instr->m_opcode == Js::OpCode::DIV)
    {TRACE_IT(18665);
        if (instr->GetDst()->AsRegOpnd()->GetReg() == RegEDX)
        {TRACE_IT(18666);
            this->peeps->ClearReg(RegEAX);
        }
        else
        {TRACE_IT(18667);
            Assert(instr->GetDst()->AsRegOpnd()->GetReg() == RegEAX);
            this->peeps->ClearReg(RegEDX);
        }
    }
}

void
PeepsMD::PeepAssign(IR::Instr *instr)
{TRACE_IT(18668);
    IR::Opnd *src = instr->GetSrc1();
    IR::Opnd *dst = instr->GetDst();

    if (instr->m_opcode == Js::OpCode::MOV && src->IsIntConstOpnd()
        && src->AsIntConstOpnd()->GetValue() == 0 && dst->IsRegOpnd())
    {TRACE_IT(18669);
        Assert(instr->GetSrc2() == NULL);

        instr->m_opcode = Js::OpCode::XOR;
        instr->ReplaceSrc1(dst);
        instr->SetSrc2(dst);
    } else if ((instr->m_opcode == Js::OpCode::MOVSD || instr->m_opcode == Js::OpCode::MOVSS || instr->m_opcode == Js::OpCode::MOVUPS) && src->IsRegOpnd() && dst->IsRegOpnd())
    {TRACE_IT(18670);
        // MOVAPS has 1 byte shorter encoding
        instr->m_opcode = Js::OpCode::MOVAPS;
    }
    else if (instr->m_opcode == Js::OpCode::MOVSD_ZERO)
    {TRACE_IT(18671);
        instr->m_opcode = Js::OpCode::XORPS;
        instr->SetSrc1(dst);
        instr->SetSrc2(dst);
    }
}

