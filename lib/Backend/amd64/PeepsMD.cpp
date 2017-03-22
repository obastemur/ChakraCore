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
#define REGDAT(Name, Listing, Encode, Type, BitVec) \
        if (!((BitVec) & (RA_CALLEESAVE | RA_DONTALLOCATE))) \
        {LOGMEIN("PeepsMD.cpp] 22\n"); \
            this->peeps->ClearReg(Reg ## Name); \
        }
#include "RegList.h"
    }
    else if (instr->m_opcode == Js::OpCode::IMUL)
    {LOGMEIN("PeepsMD.cpp] 28\n");
        this->peeps->ClearReg(RegRDX);
    }
    else if (instr->m_opcode == Js::OpCode::IDIV)
    {LOGMEIN("PeepsMD.cpp] 32\n");
        if (instr->GetDst()->AsRegOpnd()->GetReg() == RegRDX)
        {LOGMEIN("PeepsMD.cpp] 34\n");
            this->peeps->ClearReg(RegRAX);
        }
        else
        {
            Assert(instr->GetDst()->AsRegOpnd()->GetReg() == RegRAX);
            this->peeps->ClearReg(RegRDX);
        }
    }
}

void
PeepsMD::PeepAssign(IR::Instr *instr)
{LOGMEIN("PeepsMD.cpp] 47\n");
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src = instr->GetSrc1();
    if(dst->IsRegOpnd() && instr->m_opcode == Js::OpCode::MOV)
    {LOGMEIN("PeepsMD.cpp] 51\n");
        if (src->IsImmediateOpnd() && src->GetImmediateValue(instr->m_func) == 0)
        {LOGMEIN("PeepsMD.cpp] 53\n");
            Assert(instr->GetSrc2() == NULL);

            // 32-bit XOR has a smaller encoding
            if (TySize[dst->GetType()] == MachPtr)
            {LOGMEIN("PeepsMD.cpp] 58\n");
                dst->SetType(TyInt32);
            }

            instr->m_opcode = Js::OpCode::XOR;
            instr->ReplaceSrc1(dst);
            instr->SetSrc2(dst);
        }
        else if (!instr->isInlineeEntryInstr)
        {LOGMEIN("PeepsMD.cpp] 67\n");
            if(src->IsIntConstOpnd() && src->GetSize() <= TySize[TyUint32])
            {LOGMEIN("PeepsMD.cpp] 69\n");
                dst->SetType(TyUint32);
                src->SetType(TyUint32);
            }
            else if(src->IsAddrOpnd() && (((size_t)src->AsAddrOpnd()->m_address >> 32) == 0 ))
            {LOGMEIN("PeepsMD.cpp] 74\n");
                instr->ReplaceSrc1(IR::IntConstOpnd::New(::Math::PointerCastToIntegral<UIntConstType>(src->AsAddrOpnd()->m_address), TyUint32, instr->m_func));
                dst->SetType(TyUint32);
            }
        }
    }
    else if (((instr->m_opcode == Js::OpCode::MOVSD || instr->m_opcode == Js::OpCode::MOVSS)
                && src->IsRegOpnd()
                && dst->IsRegOpnd()
                && (TySize[src->GetType()] == TySize[dst->GetType()]))
        || ((instr->m_opcode == Js::OpCode::MOVUPS)
                && src->IsRegOpnd()
                && dst->IsRegOpnd())
        || (instr->m_opcode == Js::OpCode::MOVAPD))
    {LOGMEIN("PeepsMD.cpp] 88\n");
        // MOVAPS has 1 byte shorter encoding
        instr->m_opcode = Js::OpCode::MOVAPS;
    }
    else if (instr->m_opcode == Js::OpCode::MOVSD_ZERO)
    {LOGMEIN("PeepsMD.cpp] 93\n");
        instr->m_opcode = Js::OpCode::XORPS;
        instr->SetSrc1(dst);
        instr->SetSrc2(dst);
    }
}


