//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

// PeepsMD::Init
void
PeepsMD::Init(Peeps *peeps)
{LOGMEIN("PeepsMD.cpp] 9\n");
    this->peeps = peeps;
}

// PeepsMD::ProcessImplicitRegs
// Note: only do calls for now
void
PeepsMD::ProcessImplicitRegs(IR::Instr *instr)
{LOGMEIN("PeepsMD.cpp] 17\n");
    if (LowererMD::IsCall(instr))
    {LOGMEIN("PeepsMD.cpp] 19\n");
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
    {LOGMEIN("PeepsMD.cpp] 34\n");
        this->peeps->ClearReg(RegEDX);
    }
    else if (instr->m_opcode == Js::OpCode::IDIV || instr->m_opcode == Js::OpCode::DIV)
    {LOGMEIN("PeepsMD.cpp] 38\n");
        if (instr->GetDst()->AsRegOpnd()->GetReg() == RegEDX)
        {LOGMEIN("PeepsMD.cpp] 40\n");
            this->peeps->ClearReg(RegEAX);
        }
        else
        {
            Assert(instr->GetDst()->AsRegOpnd()->GetReg() == RegEAX);
            this->peeps->ClearReg(RegEDX);
        }
    }
}

void
PeepsMD::PeepAssign(IR::Instr *instr)
{LOGMEIN("PeepsMD.cpp] 53\n");
    IR::Opnd *src = instr->GetSrc1();
    IR::Opnd *dst = instr->GetDst();

    if (instr->m_opcode == Js::OpCode::MOV && src->IsIntConstOpnd()
        && src->AsIntConstOpnd()->GetValue() == 0 && dst->IsRegOpnd())
    {LOGMEIN("PeepsMD.cpp] 59\n");
        Assert(instr->GetSrc2() == NULL);

        instr->m_opcode = Js::OpCode::XOR;
        instr->ReplaceSrc1(dst);
        instr->SetSrc2(dst);
    } else if ((instr->m_opcode == Js::OpCode::MOVSD || instr->m_opcode == Js::OpCode::MOVSS || instr->m_opcode == Js::OpCode::MOVUPS) && src->IsRegOpnd() && dst->IsRegOpnd())
    {LOGMEIN("PeepsMD.cpp] 66\n");
        // MOVAPS has 1 byte shorter encoding
        instr->m_opcode = Js::OpCode::MOVAPS;
    }
    else if (instr->m_opcode == Js::OpCode::MOVSD_ZERO)
    {LOGMEIN("PeepsMD.cpp] 71\n");
        instr->m_opcode = Js::OpCode::XORPS;
        instr->SetSrc1(dst);
        instr->SetSrc2(dst);
    }
}

