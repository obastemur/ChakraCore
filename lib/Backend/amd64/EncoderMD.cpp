//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#include "X64Encode.h"

static const BYTE OpcodeByte2[]={
#define MACRO(name, jnLayout, attrib, byte2, ...) byte2,
#include "MdOpCodes.h"
#undef MACRO
};

struct FormTemplate{ BYTE form[6]; };

#define f(form) TEMPLATE_FORM_ ## form
static const struct FormTemplate OpcodeFormTemplate[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, ...) form ,
#include "MdOpCodes.h"
#undef MACRO
};
#undef f

struct OpbyteTemplate { byte opbyte[6]; };

static const struct OpbyteTemplate Opbyte[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, opbyte, ...) opbyte,
#include "MdOpCodes.h"
#undef MACRO
};

static const uint32 Opdope[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, opbyte, dope, ...) dope,
#include "MdOpCodes.h"
#undef MACRO
};

static const BYTE OpEncoding[] =
{
#define REGDAT(name,  dispName, encoding, ...) encoding ,
#include "RegList.h"
#undef MACRO
};

static const uint32 OpcodeLeadIn[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, opByte, dope, leadIn, ...) leadIn,
#include "MdOpCodes.h"
#undef MACRO
};

static const enum Forms OpcodeForms[] =
{
#define MACRO(name, jnLayout, attrib, byte2, form, ...) form,
#include "MdOpCodes.h"
#undef MACRO
};

static const BYTE  Nop1[] = { 0x90 };                   /* nop                     */
static const BYTE  Nop2[] = { 0x66, 0x90 };             /* xchg ax, ax             */
static const BYTE  Nop3[] = { 0x0F, 0x1F, 0x00 };       /* nop dword ptr [rax]     */
static const BYTE  Nop4[] = { 0x0F, 0x1F, 0x40, 0x00 }; /* nop dword ptr [rax + 0] */
static const BYTE * const Nop[4] = { Nop1, Nop2, Nop3, Nop4 };

enum CMP_IMM8
{
    EQ,
    LT,
    LE,
    UNORD,
    NEQ,
    NLT,
    NLE,
    ORD
};

///----------------------------------------------------------------------------
///
/// EncoderMD::Init
///
///----------------------------------------------------------------------------

void
EncoderMD::Init(Encoder *encoder)
{TRACE_IT(16036);
    m_encoder = encoder;
    m_relocList = nullptr;
    m_lastLoopLabelPosition = -1;
}


///----------------------------------------------------------------------------
///
/// EncoderMD::GetOpcodeByte2
///
///     Get the second byte encoding of the given instr.
///
///----------------------------------------------------------------------------

const BYTE
EncoderMD::GetOpcodeByte2(IR::Instr *instr)
{TRACE_IT(16037);
    return OpcodeByte2[instr->m_opcode - (Js::OpCode::MDStart+1)];
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetInstrForm
///
///     Get the form list of the given instruction.  The form list contains
///     the possible encoding forms of an instruction.
///
///----------------------------------------------------------------------------

Forms
EncoderMD::GetInstrForm(IR::Instr *instr)
{TRACE_IT(16038);
    return OpcodeForms[instr->m_opcode - (Js::OpCode::MDStart + 1)];
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetFormTemplate
///
///     Get the form list of the given instruction.  The form list contains
///     the possible encoding forms of an instruction.
///
///----------------------------------------------------------------------------

const BYTE *
EncoderMD::GetFormTemplate(IR::Instr *instr)
{TRACE_IT(16039);
    return OpcodeFormTemplate[instr->m_opcode - (Js::OpCode::MDStart + 1)].form;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetOpbyte
///
///     Get the first byte opcode of an instr.
///
///----------------------------------------------------------------------------

const BYTE *
EncoderMD::GetOpbyte(IR::Instr *instr)
{TRACE_IT(16040);
    return Opbyte[instr->m_opcode - (Js::OpCode::MDStart+1)].opbyte;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetRegEncode
///
///     Get the amd64 encoding of a given register.
///
///----------------------------------------------------------------------------

const BYTE
EncoderMD::GetRegEncode(IR::RegOpnd *regOpnd)
{TRACE_IT(16041);
    return this->GetRegEncode(regOpnd->GetReg());
}

const BYTE
EncoderMD::GetRegEncode(RegNum reg)
{TRACE_IT(16042);
    AssertMsg(reg != RegNOREG, "should have valid reg in encoder");

    //
    // Instead of lookup. We can also AND with 0x7.
    //


    return (BYTE)(OpEncoding[1 + reg - RegRAX]);
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetOpdope
///
///     Get the dope vector of a particular instr.  The dope vector describes
///     certain properties of an instr.
///
///----------------------------------------------------------------------------

const uint32
EncoderMD::GetOpdope(IR::Instr *instr)
{TRACE_IT(16043);
    return Opdope[instr->m_opcode - (Js::OpCode::MDStart+1)];
}

///----------------------------------------------------------------------------
///
/// EncoderMD::GetLeadIn
///
///     Get the leadin of a particular instr.
///
///----------------------------------------------------------------------------

const uint32
EncoderMD::GetLeadIn(IR::Instr * instr)
{TRACE_IT(16044);
    return OpcodeLeadIn[instr->m_opcode - (Js::OpCode::MDStart+1)];
}
///----------------------------------------------------------------------------
///
/// EncoderMD::FitsInByte
///
///     Can we encode this value into a signed extended byte?
///
///----------------------------------------------------------------------------

bool
EncoderMD::FitsInByte(size_t value)
{TRACE_IT(16045);
    return ((size_t)(signed char)(value & 0xFF) == value);
}


///----------------------------------------------------------------------------
///
/// EncoderMD::GetMod
///
///     Get the "MOD" part of a MODRM encoding for a given operand.
///
///----------------------------------------------------------------------------

BYTE
EncoderMD::GetMod(IR::IndirOpnd * indirOpnd, int* pDispSize)
{TRACE_IT(16046);
    RegNum reg = indirOpnd->GetBaseOpnd()->GetReg();
    return GetMod(indirOpnd->GetOffset(), (reg == RegR13 || reg == RegRBP), pDispSize);
}

BYTE
EncoderMD::GetMod(IR::SymOpnd * symOpnd, int * pDispSize, RegNum& rmReg)
{TRACE_IT(16047);
    StackSym * stackSym = symOpnd->m_sym->AsStackSym();
    int32 offset = stackSym->m_offset;
    rmReg = RegRBP;
    if (stackSym->IsArgSlotSym() && !stackSym->m_isOrphanedArg)
    {TRACE_IT(16048);
        if (stackSym->m_isInlinedArgSlot)
        {TRACE_IT(16049);
            Assert(offset >= 0);
            offset -= this->m_func->m_localStackHeight;
            stackSym->m_offset = offset;
            stackSym->m_allocated = true;
        }
        else
        {TRACE_IT(16050);
            rmReg = RegRSP;
        }
    }
    else
    {TRACE_IT(16051);
        Assert(offset != 0);
    }
    return GetMod((size_t)offset + (size_t)symOpnd->m_offset, rmReg == RegRBP, pDispSize);

}

BYTE
EncoderMD::GetMod(size_t offset, bool regIsRbpOrR13, int * pDispSize)
{TRACE_IT(16052);
    if (offset == 0 && !regIsRbpOrR13)
    {TRACE_IT(16053);
        *(pDispSize) = 0;
        return Mod00;
    }
    else if (this->FitsInByte(offset))
    {TRACE_IT(16054);
        *(pDispSize) = 1;
        return Mod01;
    }
    else if(Math::FitsInDWord(offset))
    {TRACE_IT(16055);
        *(pDispSize) = 4;
        return Mod10;
    }
    else
    {
        AssertMsg(0, "Cannot encode offsets more than 32 bits in MODRM");
        return 0xff;
    }
}

///----------------------------------------------------------------------------
///
/// EncoderMD::EmitModRM
///
///     Emit an effective address using the MODRM byte.
///
///----------------------------------------------------------------------------

BYTE
EncoderMD::EmitModRM(IR::Instr * instr, IR::Opnd *opnd, BYTE reg1)
{TRACE_IT(16056);
    int dispSize = -1; // Initialize to suppress C4701 false positive
    IR::IndirOpnd *indirOpnd;
    IR::RegOpnd *regOpnd;
    IR::RegOpnd *baseOpnd;
    IR::RegOpnd *indexOpnd;
    BYTE reg;
    BYTE regBase;
    BYTE regIndex;
    BYTE baseRegEncode;
    BYTE rexEncoding = 0;

    AssertMsg( (reg1 & 7) == reg1, "Invalid reg1");

    reg1 = (reg1 & 7) << 3;       // mask and put in reg field

    switch (opnd->GetKind())
    {
    case IR::OpndKindReg:
        regOpnd = opnd->AsRegOpnd();

        AssertMsg(regOpnd->GetReg() != RegNOREG, "All regOpnd should have a valid reg set during encoder");

        reg = this->GetRegEncode(regOpnd);
        this->EmitConst((Mod11 | reg1 | reg), 1);

        if(this->IsExtendedRegister(regOpnd->GetReg()))
        {TRACE_IT(16057);
            return REXB;
        }
        else
        {TRACE_IT(16058);
            return 0;
        }

    case IR::OpndKindSym:
        AssertMsg(opnd->AsSymOpnd()->m_sym->IsStackSym(), "Should only see stackSym syms in encoder.");

        BYTE byte;
        RegNum rmReg;
        BYTE mod;
        mod = this->GetMod(opnd->AsSymOpnd(), &dispSize, rmReg);
        baseRegEncode = this->GetRegEncode(rmReg);
        byte = (BYTE)(mod | reg1 | baseRegEncode);
        *(m_pc++) = byte;
        if (rmReg == RegRSP)
        {TRACE_IT(16059);
            byte = (BYTE)(((baseRegEncode & 7) << 3) | (baseRegEncode & 7));
            *(m_pc++) = byte;
        }
        else
        {TRACE_IT(16060);
            AssertMsg(opnd->AsSymOpnd()->m_sym->AsStackSym()->m_offset, "Expected stackSym offset to be set.");
        }
        break;

    case IR::OpndKindIndir:

        indirOpnd = opnd->AsIndirOpnd();
        AssertMsg(indirOpnd->GetBaseOpnd() != nullptr, "Expected base to be set in indirOpnd");

        baseOpnd = indirOpnd->GetBaseOpnd();
        indexOpnd = indirOpnd->GetIndexOpnd();
        AssertMsg(!indexOpnd || indexOpnd->GetReg() != RegRSP, "ESP cannot be the index of an indir.");

        regBase = this->GetRegEncode(baseOpnd);

        if (indexOpnd != nullptr)
        {TRACE_IT(16061);
            regIndex = this->GetRegEncode(indexOpnd);
            *(m_pc++) = (this->GetMod(indirOpnd, &dispSize) | reg1 | 0x4);
            *(m_pc++) = (((indirOpnd->GetScale() & 3) << 6) | ((regIndex & 7) << 3) | (regBase & 7));

            rexEncoding |= this->GetRexByte(this->REXX, indexOpnd);
            rexEncoding |= this->GetRexByte(this->REXB, baseOpnd);
        }
        else if (baseOpnd->GetReg() == RegR12 || baseOpnd->GetReg() == RegRSP)
        {TRACE_IT(16062);
            //
            // Using RSP/R12 as base requires the SIB byte even where there is no index.
            //
            *(m_pc++) = (this->GetMod(indirOpnd, &dispSize) | reg1 | regBase);
            *(m_pc++) = (BYTE)(((regBase & 7) << 3) | (regBase & 7));

            rexEncoding |= this->GetRexByte(this->REXB, baseOpnd);
        }
        else
        {TRACE_IT(16063);
            *(m_pc++) = (this->GetMod(indirOpnd, &dispSize) | reg1 | regBase);
            rexEncoding |= this->GetRexByte(this->REXB, baseOpnd);
        }
        break;

    case IR::OpndKindMemRef:
        *(m_pc++)   = (char)(reg1 | 0x4);
        *(m_pc++)   = 0x25;       // SIB displacement
        AssertMsg(Math::FitsInDWord((size_t)opnd->AsMemRefOpnd()->GetMemLoc()), "Size overflow");
        dispSize    = 4;
        break;

    default:
        AssertMsg(UNREACHED, "Unexpected operand kind");
        break;
    }

    AssertMsg(dispSize != -1, "Uninitialized dispSize");

    BYTE retval = this->EmitImmed(opnd, dispSize, 0);

    AssertMsg(retval == 0, "Not possible.");
    return rexEncoding;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::EmitConst
///
///     Emit a constant of the given size.
///
///----------------------------------------------------------------------------

void
EncoderMD::EmitConst(size_t val, int size, bool allowImm64 /* = false */)
{TRACE_IT(16064);
    AssertMsg(allowImm64 || size != 8, "Invalid size of immediate. It can only be 8 for certain instructions, MOV being the most popular");

    switch (size)
    {
    case 0:
        return;

    case 1:
        *(uint8*)m_pc = (char)val;
        break;

    case 2:
        *(uint16*)m_pc = (short)val;
        break;

    case 4:
        *(uint32*)m_pc = (uint32)val;
        break;

    case 8:
        *(uint64*)m_pc = (uint64)val;
        break;

    default:
        AssertMsg(UNREACHED, "Unexpected size");
    }

    m_pc += size;
}

///----------------------------------------------------------------------------
///
/// EncoderMD::EmitImmed
///
///     Emit the immediate value of the given operand.  It returns 0x2 for a
///     sbit encoding, 0 otherwise.
///
///----------------------------------------------------------------------------

BYTE
EncoderMD::EmitImmed(IR::Opnd * opnd, int opSize, int sbit, bool allow64Immediates)
{TRACE_IT(16065);
    StackSym *stackSym   = nullptr;
    BYTE      retval     = 0;
    size_t    value      = 0;

    switch (opnd->GetKind())
    {
    case IR::OpndKindInt64Const:
        value = (size_t)opnd->AsInt64ConstOpnd()->GetValue();
        goto intConst;

    case IR::OpndKindAddr:
        value = (size_t)opnd->AsAddrOpnd()->m_address;
        goto intConst;

    case IR::OpndKindIntConst:
        value = opnd->AsIntConstOpnd()->GetValue();
intConst:
        if (sbit && opSize > 1 && this->FitsInByte(value))
        {TRACE_IT(16066);
            opSize = 1;
            retval = 0x2;   /* set S bit */
        }
        break;

    case IR::OpndKindSym:
        AssertMsg(opnd->AsSymOpnd()->m_sym->IsStackSym(), "Should only see stackSym here");
        stackSym = opnd->AsSymOpnd()->m_sym->AsStackSym();
        value = stackSym->m_offset + opnd->AsSymOpnd()->m_offset;
        break;

    case IR::OpndKindHelperCall:
        AssertMsg(this->GetOpndSize(opnd) == 8, "HelperCall opnd must be 64 bit");
        value = (size_t)IR::GetMethodAddress(m_func->GetThreadContextInfo(), opnd->AsHelperCallOpnd());
        break;

    case IR::OpndKindIndir:
        value = opnd->AsIndirOpnd()->GetOffset();
        break;

    case IR::OpndKindMemRef:
        value = (size_t)opnd->AsMemRefOpnd()->GetMemLoc();
        break;

    case IR::OpndKindLabel:
        value = 0;
        AppendRelocEntry(RelocTypeLabelUse, (void*) m_pc, opnd->AsLabelOpnd()->GetLabel());
        break;

    default:
        AssertMsg(UNREACHED, "Unimplemented...");
    }

    if (!allow64Immediates && opSize == 8)
    {TRACE_IT(16067);
        Assert(Math::FitsInDWord(value));
        opSize = 4;
    }

    this->EmitConst(value, opSize, allow64Immediates);

    return retval;
}

int
EncoderMD::GetOpndSize(IR::Opnd * opnd)
{TRACE_IT(16068);
    return TySize[opnd->GetType()];
}

///----------------------------------------------------------------------------
///
/// EncoderMD::Encode
///
///     Emit the x64 encoding for the given instruction in the passed in
///     buffer ptr.
///
///----------------------------------------------------------------------------

ptrdiff_t
EncoderMD::Encode(IR::Instr *instr, BYTE *pc, BYTE* beginCodeAddress)
{TRACE_IT(16069);
    BYTE     *popcodeByte         = nullptr,
             *prexByte            = nullptr,
             *instrStart         = nullptr,
             *instrRestart       = nullptr;
    IR::Opnd *dst                = instr->GetDst();
    IR::Opnd *src1               = instr->GetSrc1();
    IR::Opnd *src2               = instr->GetSrc2();
    IR::Opnd *opr1               = nullptr;
    IR::Opnd *opr2               = nullptr;
    int       instrSize          = -1;
    bool      skipRexByte        = false;

    m_pc = pc;

    if (instr->IsLowered() == false)
    {TRACE_IT(16070);
        if (instr->IsLabelInstr())
        {TRACE_IT(16071);
            IR::LabelInstr *labelInstr = instr->AsLabelInstr();
            labelInstr->SetPC(m_pc);
            if (!labelInstr->IsUnreferenced())
            {TRACE_IT(16072);
                int relocEntryPosition = AppendRelocEntry(RelocTypeLabel, labelInstr);
                if (!PHASE_OFF(Js::LoopAlignPhase, m_func))
                {TRACE_IT(16073);
                    // we record position of last loop-top label (leaf loops) for loop alignment
                    if (labelInstr->m_isLoopTop && labelInstr->GetLoop()->isLeaf)
                    {TRACE_IT(16074);
                        m_relocList->Item(relocEntryPosition).m_type = RelocType::RelocTypeAlignedLabel;
                    }
                }
            }
        }
#if DBG_DUMP
        if (instr->IsEntryInstr() && Js::Configuration::Global.flags.DebugBreak.Contains(m_func->GetFunctionNumber()))
        {TRACE_IT(16075);
            IR::Instr *int3 = IR::Instr::New(Js::OpCode::INT, m_func);
            int3->SetSrc1(IR::IntConstOpnd::New(3, TyInt32, m_func));

            return this->Encode(int3, m_pc);
        }
#endif
        return 0;
    }

    const BYTE *form = EncoderMD::GetFormTemplate(instr);
    const BYTE *opcodeTemplate = EncoderMD::GetOpbyte(instr);
    const uint32 leadIn = EncoderMD::GetLeadIn(instr);
    uint32 opdope = EncoderMD::GetOpdope(instr);

    //
    // Canonicalize operands.
    //
    if (opdope & DDST)
    {TRACE_IT(16076);
        opr1 = dst;
        opr2 = src1;
    }
    else
    {TRACE_IT(16077);
        opr1 = src1;
        opr2 = src2;
    }

    if (opr1)
    {TRACE_IT(16078);
        instrSize = this->GetOpndSize(opdope & DREXSRC ? opr2 : opr1);

#if DBG
        switch (instr->m_opcode)
        {
        case Js::OpCode::MOVSXD:
            if (8 != this->GetOpndSize(opr1) || 4 != this->GetOpndSize(opr2))
            {
                AssertMsg(0, "Invalid Operand size ");
            }
            break;

        case Js::OpCode::IMUL2:
            // If this is IMUL3 then make sure a 32 bit encoding is possible.
            Assert((!opr2) || (!opr2->IsImmediateOpnd()) || (GetOpndSize(opr1) < 8));
            break;

        case Js::OpCode::SHL:
        case Js::OpCode::SHR:
        case Js::OpCode::SAR:
            //
            // TODO_JIT64:
            // These should be MachInt instead of Int32. Just ignore for now.
            //
            //if (instrSize != TySize[TyInt32])
            //{
            //  AssertMsg(0, "Operand size mismatch");
            //}
            break;

        default:
            if (opr2)
            {TRACE_IT(16079);
                //
                // review: this should eventually be enabled. Currently our type system does not
                // allow copying from DWORD to QWORD. hence the problem. This is required to read
                // JavascriptArray::length which is 4 bytes but the length needs to be operated as
                // normal Machine Integer.
                //

                //
                // TODO_JIT64:
                // These should be MachInt instead of Int32. Just ignore for now.
                //
                /*
                if (instrSize != this->GetOpndSize(opr2))
                {
                    AssertMsg(0, "Operand size mismatch");
                }
                */
            }
        }
#endif
    }

    instrRestart = instrStart = m_pc;

    // put out 16bit override if any
    if (instrSize == 2 && (opdope & (DNO16 | DFLT)) == 0)
    {TRACE_IT(16080);
        *instrRestart++ = 0x66;
    }

    //
    // Instruction format is REX [0xF] OP_BYTE ...
    //

    // Emit the leading byte(s) of multibyte instructions.
    if (opdope & D66EX)
    {TRACE_IT(16081);
        Assert((opdope & (D66EX | D66 | DF2 | DF3 )) == D66EX);
        if (opr1->IsFloat64() || opr2->IsFloat64())
        {TRACE_IT(16082);
            *instrRestart++ = 0x66;
        }
    }
    else if (opdope & D66)
    {TRACE_IT(16083);
        Assert((opdope & (D66 | DF2 | DF3)) == D66);
        Assert(leadIn == OLB_0F || leadIn == OLB_0F3A);
        *instrRestart++ = 0x66;
    }
    else if (opdope & DF2)
    {TRACE_IT(16084);
        Assert((opdope & (DF2 | DF3)) == DF2);
        Assert(leadIn == OLB_0F);
        *instrRestart++ = 0xf2;
    }
    else if (opdope & DF3)
    {TRACE_IT(16085);
        Assert(leadIn == OLB_0F);
        *instrRestart++ = 0xf3;
    }

    // REX byte.
    prexByte = instrRestart;

    // This is a heuristic to determine whether we really need to have the Rex bytes
    // This heuristics is always correct for instrSize == 8
    // For instrSize < 8, we might use extended registers and we will have to adjust in EmitRexByte
    bool reservedRexByte = (instrSize == 8);
    if (reservedRexByte)
    {TRACE_IT(16086);
        instrRestart++;
    }

    switch(leadIn)
    {
    case OLB_NONE:
        break;
    case OLB_0F:
        *instrRestart++ = 0x0f;
        break;
    case OLB_0F3A:
        *instrRestart++ = 0x0f;
        *instrRestart++ = 0x3a;
        break;
    default:
        Assert(UNREACHED);
        __assume(UNREACHED);
    }
    // Actual opcode byte.
    popcodeByte = instrRestart++;

    //
    // Try each form 1 by 1, until we find the one appropriate for this instruction
    // and its operands.
    //
    for (;; opcodeTemplate++, form++)
    {TRACE_IT(16087);
        m_pc = instrRestart;
        AssertMsg(m_pc - instrStart <= MachMaxInstrSize, "MachMaxInstrSize not set correctly");

        // Setup default values.
        BYTE opcodeByte = *opcodeTemplate;
        BYTE rexByte = REXOVERRIDE | REXW;

        switch ((*form) & FORM_MASK)
        {
            //
            // This would only be required for mov rax, [64bit memory address].
            //
        case AX_MEM:
            if (!opr2 || !opr2->IsMemRefOpnd())
            {TRACE_IT(16088);
                continue;
            }

            AnalysisAssert(opr1);
            if (opr1->IsRegOpnd() && RegRAX == opr1->AsRegOpnd()->GetReg())
            {
                AssertMsg(opr2, "Operand 2 must be present in AX_MEM mode");

                if (TyInt64 == instrSize)
                {TRACE_IT(16089);
                    this->EmitImmed(opr2, instrSize, 0);
                }
                else
                {TRACE_IT(16090);
                    //
                    // we don't have a 64 bit opnd. Hence we can will have to use
                    // the normal MODRM/SIB encoding.
                    //
                    continue;
                }
            }

        // NYI
        case AX_IM:
            continue;

        // General immediate case. Special cases have already been checked.
        case IMM:
            if (!opr2->IsImmediateOpnd())
            {TRACE_IT(16091);
                continue;
            }

            rexByte    |= this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr) >> 3);
            opcodeByte |= this->EmitImmed(opr2, instrSize, *form & SBIT);
            break;

        case NO:
            {TRACE_IT(16092);
                Assert(instr->m_opcode == Js::OpCode::CQO || instr->m_opcode == Js::OpCode::CDQ);

                instrSize = instr->m_opcode == Js::OpCode::CQO ? 8 : 4;
                BYTE byte2 = this->GetOpcodeByte2(instr);

                if (byte2)
                {TRACE_IT(16093);
                    *(m_pc)++ = byte2;
                }
            }
            break;

        // Short immediate/reg.
        case SHIMR:
            AnalysisAssert(opr1);
            if (!opr1->IsRegOpnd())
            {TRACE_IT(16094);
                continue;
            }

            if (opr2->IsImmediateOpnd())
            {TRACE_IT(16095);
                Assert(EncoderMD::IsMOVEncoding(instr));
                if (instrSize == 8 && !instr->isInlineeEntryInstr && Math::FitsInDWord(opr2->GetImmediateValue(instr->m_func)))
                {TRACE_IT(16096);
                    // Better off using the C7 encoding as it will sign extend
                    continue;
                }
            }
            else if (!opr2->IsLabelOpnd())
            {TRACE_IT(16097);
                continue;
            }

            opcodeByte |= this->GetRegEncode(opr1->AsRegOpnd());
            rexByte    |= this->GetRexByte(this->REXB, opr1);

            if (instrSize > 1)
            {TRACE_IT(16098);
                opcodeByte |= 0x8; /* set the W bit */
            }

            this->EmitImmed(opr2, instrSize, 0, true);  /* S bit known to be 0 */
            break;

        case AXMODRM:
            AssertMsg(opr1->AsRegOpnd()->GetReg() == RegRAX, "Expected src1 of IMUL/IDIV to be RAX");

            opr1 = opr2;
            opr2 = nullptr;

        // FALLTHROUGH
        case MODRM:
        modrm:
            AnalysisAssert(opr1);
            if (opr2 == nullptr)
            {TRACE_IT(16099);
                BYTE byte2  = (this->GetOpcodeByte2(instr) >> 3);
                rexByte    |= this->EmitModRM(instr, opr1, byte2);
            }
            else if (opr1->IsRegOpnd())
            {TRACE_IT(16100);
                rexByte |= this->GetRexByte(this->REXR, opr1);
                rexByte    |= this->EmitModRM(instr, opr2, this->GetRegEncode(opr1->AsRegOpnd()));
                if ((*form) & DBIT)
                {TRACE_IT(16101);
                    opcodeByte |= 0x2;     // set D bit
                }
            }
            else
            {TRACE_IT(16102);
                AssertMsg(opr2->IsRegOpnd(), "Expected opr2 to be a valid reg");
                AssertMsg(instrSize == this->GetOpndSize(opr2), "sf");
                AssertMsg(instrSize == this->GetOpndSize(opr1), "Opnd Size mismatch");

                rexByte    |= this->GetRexByte(this->REXR, opr2);
                rexByte    |= this->EmitModRM(instr, opr1, this->GetRegEncode(opr2->AsRegOpnd()));
            }
            break;

        // reg in opbyte. Only whole register allowed .
        case SH_REG:
            AnalysisAssert(opr1);
            if (!opr1->IsRegOpnd())
            {TRACE_IT(16103);
                continue;
            }
            opcodeByte |= this->GetRegEncode(opr1->AsRegOpnd());
            rexByte    |= this->GetRexByte(this->REXB, opr1);
            break;

        // short form immed. (must be unary)
        case SH_IM:
            AnalysisAssert(opr1);
            if (!opr1->IsIntConstOpnd() && !opr1->IsAddrOpnd())
            {TRACE_IT(16104);
                continue;
            }
            instrSize    = this->GetOpndSize(opr1);
            opcodeByte |= this->EmitImmed(opr1, instrSize, 1);
            break;

        case SHFT:
            rexByte     |= this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr) >> 3);
            if (opr2->IsRegOpnd())
            {TRACE_IT(16105);
                AssertMsg(opr2->AsRegOpnd()->GetReg() == RegRCX, "Expected ECX as opr2 of variable shift");
                opcodeByte |= *(opcodeTemplate + 1);
            }
            else
            {TRACE_IT(16106);
                IntConstType value;
                AssertMsg(opr2->IsIntConstOpnd(), "Expected register or constant as shift amount opnd");
                value = opr2->AsIntConstOpnd()->GetValue();
                if (value == 1)
                {TRACE_IT(16107);
                    opcodeByte |= 0x10;
                }
                else
                {TRACE_IT(16108);
                    this->EmitConst(value, 1);
                }
            }
            break;

        // NYI
        case LABREL1:
            continue;

        // jmp, call with full relative disp.
        case LABREL2:
            if (opr1 == nullptr)
            {TRACE_IT(16109);
                AssertMsg(sizeof(size_t) == sizeof(void*), "Sizes of void* assumed to be 64-bits");
                AssertMsg(instr->IsBranchInstr(), "Invalid LABREL2 form");
                AppendRelocEntry(RelocTypeBranch, (void*)m_pc, instr->AsBranchInstr()->GetTarget());
                this->EmitConst(0 , 4);
            }
            else if (opr1->IsIntConstOpnd())
            {
                // pc relative calls are not supported on x64.
                AssertMsg(UNREACHED, "Pc relative calls are not supported on x64");
            }
            else
            {TRACE_IT(16110);
                continue;
            }
            break;

        // Special form which doesn't fit any existing patterns.
        case SPECIAL:
            switch (instr->m_opcode)
            {
            case Js::OpCode::RET:
            {TRACE_IT(16111);
                AssertMsg(opr1->IsIntConstOpnd(), "RET should have intConst as src");
                IntConstType value = opr1->AsIntConstOpnd()->GetValue();
                if (value==0)
                {TRACE_IT(16112);
                    opcodeByte |= 0x1; // no imm16 follows
                }
                else
                {TRACE_IT(16113);
                    this->EmitConst(value, 2);
                }
                break;
            }

            case Js::OpCode::BT:
            case Js::OpCode::BTR:
            case Js::OpCode::BTS:
                /*
                 *       0F A3 /r      BT  r/m32, r32
                 * REX.W 0F A3 /r      BT  r/m64, r64
                 *       0F BA /6 ib   BT  r/m32, imm8
                 * REX.W 0F BA /6 ib   BT  r/m64, imm8
                 * or
                 *       0F B3 /r      BTR r/m32, r32
                 * REX.W 0F B3 /r      BTR r/m64, r64
                 *       0F BA /6 ib   BTR r/m32, imm8
                 * REX.W 0F BA /6 ib   BTR r/m64, imm8
                 * or
                 *       0F AB /r      BTS r/m32, r32
                 * REX.W 0F AB /r      BTS r/m64, r64
                 *       0F BA /5 ib   BTS r/m32, imm8
                 * REX.W 0F BA /5 ib   BTS r/m64, imm8
                 */
                Assert(instr->m_opcode != Js::OpCode::BT || dst == nullptr);
                AssertMsg(instr->m_opcode == Js::OpCode::BT ||
                    dst && (dst->IsRegOpnd() || dst->IsMemRefOpnd() || dst->IsIndirOpnd()), "Invalid dst type on BTR/BTS instruction.");

                if (src2->IsImmediateOpnd())
                {TRACE_IT(16114);
                    rexByte |= this->GetRexByte(this->REXR, src1);
                    rexByte |= this->EmitModRM(instr, src1, this->GetOpcodeByte2(instr) >> 3);
                    Assert(src2->IsIntConstOpnd() && src2->GetType() == TyInt8);
                    opcodeByte |= EmitImmed(src2, 1, 0);
                }
                else
                {TRACE_IT(16115);
                    /* this is special dbit modrm in which opr1 can be a reg*/
                    Assert(src2->IsRegOpnd());
                    form++;
                    opcodeTemplate++;
                    Assert(((*form) & FORM_MASK) == MODRM);
                    opcodeByte = *opcodeTemplate;
                    rexByte |= this->GetRexByte(this->REXR, src2);
                    rexByte |= this->EmitModRM(instr, src1, this->GetRegEncode(src2->AsRegOpnd()));
                }
                break;
            case Js::OpCode::SHLD:
                /*
                 *       0F A4   SHLD r/m32, r32, imm8
                 * REX.W 0F A4   SHLD r/m64, r64, imm8
                 */
                AssertMsg(dst && (dst->IsRegOpnd() || dst->IsMemRefOpnd() || dst->IsIndirOpnd()), "Invalid dst type on SHLD instruction.");
                AssertMsg(src1 && src1->IsRegOpnd(), "Expected SHLD's second operand to be a register.");
                AssertMsg(src2 && src2->IsImmediateOpnd(), "Expected SHLD's third operand to be an immediate.");
                rexByte    |= this->GetRexByte(this->REXR, opr2);
                rexByte    |= EmitModRM(instr, opr1, GetRegEncode(opr2->AsRegOpnd()));
                opcodeByte |= EmitImmed(src2, 1, 1);
                break;

            case Js::OpCode::IMUL2:
                AssertMsg(opr1->IsRegOpnd() && instrSize != 1, "Illegal IMUL2");
                if (!opr2->IsImmediateOpnd())
                {TRACE_IT(16116);
                    continue;
                }
                Assert(instrSize < 8);

                // turn an 'imul2 reg, immed' into an 'imul3 reg, reg, immed'.

                // Reset pointers
                prexByte = instrStart;
                reservedRexByte = true;
                // imul3 uses a one-byte opcode, so no need to set the first byte of the opcode to 0xf
                popcodeByte = instrStart + 1;
                m_pc = instrStart + 2;

                // Set up default values
                rexByte = REXOVERRIDE;
                opcodeByte = *opcodeTemplate; // hammer prefix

                rexByte |= this->GetRexByte(REXR, opr1);
                rexByte |= this->EmitModRM(instr, opr1, this->GetRegEncode(opr1->AsRegOpnd()));

                // Currently, we only support 32-bit operands (Assert(instrSize < 8) above), so REXW is not set
                Assert(!(rexByte & REXW));

                opcodeByte |= this->EmitImmed(opr2, instrSize, 1);
                break;

            case Js::OpCode::INT:
                if (opr1->AsIntConstOpnd()->GetValue() != 3)
                {TRACE_IT(16117);
                    opcodeByte |= 1;
                    *(m_pc)++ = (char)opr1->AsIntConstOpnd()->GetValue();
                }
                break;

            case Js::OpCode::MOVQ:
            {TRACE_IT(16118);
                // The usual mechanism for encoding SSE/SSE2 instructions
                // doesn't work quite right for this one, so we must fixup
                // the encoding.
                if (REGNUM_ISXMMXREG(opr1->AsRegOpnd()->GetReg())) {TRACE_IT(16119);
                    if (opr2->IsRegOpnd() && this->GetOpndSize(opr2) == 8) {TRACE_IT(16120); // QWORD_SIZE

                        // Encoded as 66 REX.W 0F 6E ModRM
                        opdope &= ~(DF2 | DF3);
                        opdope |= D66;
                        opcodeByte = 0x6E;

                        goto modrm;
                    }
                    else {TRACE_IT(16121);
                        // Encoded as F3 0F 7E ModRM
                        opdope &= ~(D66 | D66EX | DF2);
                        opdope |= DF3;
                        opcodeByte = 0x7E;

                        goto modrm;
                    }

                }
                //else if (TU_ISXMMXREG(opr2)) {
                else if (REGNUM_ISXMMXREG(opr2->AsRegOpnd()->GetReg())) {TRACE_IT(16122);
                    //if (TU_ISREG(opr1) && (TU_SIZE(opr1) == QWORD_SIZE)) {
                    if (opr1->IsRegOpnd() && this->GetOpndSize(opr1) == 8) {TRACE_IT(16123);// QWORD_SIZE
                        // Encoded as 66 REX.W 0F 7E ModRM
                        opdope &= ~(DF2 | DF3);
                        opdope |= D66;
                        opcodeByte = 0x7E;

                        // Swap operands to get right behavior from modrm encode.

                        IR::Opnd* tmp = opr1;
                        opr1 = opr2;
                        opr2 = tmp;

                        goto modrm;
                    }
                    else {TRACE_IT(16124);
                        // Encoded as 66 0F D6
                        opdope &= ~(DF2 | DF3);
                        opdope |= D66;
                        opcodeByte = 0xD6;

                        goto modrm;
                    }
                }
            }
                // FALL-THROUGH to MOVD                

            case Js::OpCode::MOVD:
                if (opr2->IsRegOpnd() && REGNUM_ISXMMXREG(opr2->AsRegOpnd()->GetReg()))
                {TRACE_IT(16125);
                    // If the second operand is an XMM register, use the "store" form.
                    if (opr1->IsRegOpnd())
                    {TRACE_IT(16126);
                        // Swap operands to get right behavior from MODRM.
                        IR::Opnd *tmp = opr1;
                        opr1 = opr2;
                        opr2 = tmp;
                    }
                    opcodeByte |= 0x10;
                }
                Assert(opr1->IsRegOpnd() && REGNUM_ISXMMXREG(opr1->AsRegOpnd()->GetReg()));
                goto modrm;

            case Js::OpCode::MOVLHPS:
            case Js::OpCode::MOVHLPS:
                Assert(opr1->IsRegOpnd() && REGNUM_ISXMMXREG(opr1->AsRegOpnd()->GetReg()));
                Assert(opr2->IsRegOpnd() && REGNUM_ISXMMXREG(opr2->AsRegOpnd()->GetReg()));
                goto modrm;

            case Js::OpCode::MOVMSKPD:
            case Js::OpCode::MOVMSKPS:
            case Js::OpCode::PMOVMSKB:
                /* Instruction form is "MOVMSKP[S/D] r32, xmm" */
                Assert(opr1->IsRegOpnd() && opr2->IsRegOpnd() && REGNUM_ISXMMXREG(opr2->AsRegOpnd()->GetReg()));
                goto modrm;

            case Js::OpCode::MOVSS:
            case Js::OpCode::MOVSD:
            case Js::OpCode::MOVAPD:
            case Js::OpCode::MOVAPS:
            case Js::OpCode::MOVUPS:
            case Js::OpCode::MOVHPD:
                if (!opr1->IsRegOpnd())
                {TRACE_IT(16127);
                    Assert(opr1->IsIndirOpnd() || opr1->IsMemRefOpnd() || opr1->IsSymOpnd());
                    Assert(opr2->IsRegOpnd());
                    Assert(REGNUM_ISXMMXREG(opr2->AsRegOpnd()->GetReg()));
                    opcodeByte |= 0x01;
                }
                goto modrm;

            case Js::OpCode::NOP:
                if (instr->GetSrc1())
                {TRACE_IT(16128);
                    // Multibyte NOP.
                    Assert(instr->GetSrc1()->IsIntConstOpnd() && instr->GetSrc1()->GetType() == TyInt8);
                    unsigned nopSize = instr->GetSrc1()->AsIntConstOpnd()->AsUint32();
                    Assert(nopSize >= 2 && nopSize <= 4);
                    nopSize = max(2u, min(4u, nopSize)); // satisfy oacr
                    const BYTE *nopEncoding = Nop[nopSize - 1];
                    opcodeByte = nopEncoding[0];
                    for (unsigned i = 1; i < nopSize; i++)
                    {TRACE_IT(16129);
                        *(m_pc)++ = nopEncoding[i];
                    }
                }
                skipRexByte = true;
                break;

            case Js::OpCode::XCHG:
                if (instrSize == 1)
                    continue;

                if (opr1->IsRegOpnd() && opr1->AsRegOpnd()->GetReg() == RegRAX
                    && opr2->IsRegOpnd())
                {TRACE_IT(16130);
                    uint8 reg = this->GetRegEncode(opr2->AsRegOpnd());
                    rexByte |= this->GetRexByte(REXR, opr2);
                    opcodeByte |= reg;
                }
                else if (opr2->IsRegOpnd() && opr2->AsRegOpnd()->GetReg() == RegRAX
                    && opr1->IsRegOpnd())
                {TRACE_IT(16131);
                    uint8 reg = this->GetRegEncode(opr1->AsRegOpnd());
                    rexByte |= this->GetRexByte(REXB, opr1);
                    opcodeByte |= reg;
                }
                else
                {TRACE_IT(16132);
                    continue;
                }
                break;

            case Js::OpCode::PSLLW:
            case Js::OpCode::PSLLD:
            case Js::OpCode::PSRLW:
            case Js::OpCode::PSRLD:
            case Js::OpCode::PSRAW:
            case Js::OpCode::PSRAD:
            case Js::OpCode::PSLLDQ:
            case Js::OpCode::PSRLDQ:
                Assert(opr1->IsRegOpnd());
                if (src2 &&src2->IsIntConstOpnd())
                {TRACE_IT(16133);
                    // SSE shift with IMM
                    rexByte |= this->EmitModRM(instr, opr1, this->GetOpcodeByte2(instr) >> 3);
                    break;
                }
                else
                {TRACE_IT(16134);
                    // Variable shift amount

                    // fix opcode byte
                    switch (instr->m_opcode)
                    {
                    case Js::OpCode::PSLLW:
                        opcodeByte = 0xF1;
                        break;
                    case Js::OpCode::PSLLD:
                        opcodeByte = 0xF2;
                        break;
                    case Js::OpCode::PSRLW:
                        opcodeByte = 0xD1;
                        break;
                    case Js::OpCode::PSRLD:
                        opcodeByte = 0xD2;
                        break;
                    case Js::OpCode::PSRAW:
                        opcodeByte = 0xE1;
                        break;
                    case Js::OpCode::PSRAD:
                        opcodeByte = 0xE2;
                        break;
                    default:
                        Assert(UNREACHED);
                    }
                    goto modrm;
                }

            default:
                AssertMsg(UNREACHED, "Unhandled opcode in SPECIAL form");
            }
            break;

        case INVALID:
            AssertMsg(UNREACHED, "No encoder form found");
            return m_pc - instrStart;

        default:
            AssertMsg(UNREACHED, "Unhandled encoder form");
        }

        // If instr has W bit, set it appropriately.
        if ((*form & WBIT) && !(opdope & DFLT) && instrSize != 1)
        {TRACE_IT(16135);
            opcodeByte |= 0x1; // set WBIT
        }

        *popcodeByte = opcodeByte;

        AssertMsg(m_pc - instrStart <= MachMaxInstrSize, "MachMaxInstrSize not set correctly");

        // Near JMPs, all instructions that reference RSP implicitly and
        // instructions that operate on registers smaller than 64 bits don't
        // get a REX prefix.
        EmitRexByte(prexByte, rexByte, skipRexByte || (instrSize < 8), reservedRexByte);

        if (opdope & DSSE)
        {TRACE_IT(16136);
            // extra imm8 byte for SSE instructions.
            uint valueImm = 0;
            bool writeImm = true;
            if (src2 &&src2->IsIntConstOpnd())
            {TRACE_IT(16137);
                valueImm = (uint)src2->AsIntConstOpnd()->GetImmediateValue(instr->m_func);
            }
            else
            {TRACE_IT(16138);
                // Variable src2, we are either encoding a CMP op, or don't need an Imm.
                // src2(comparison byte) is missing in CMP instructions and is part of the opcode instead.
                switch (instr->m_opcode)
                {
                case Js::OpCode::CMPLTPS:
                case Js::OpCode::CMPLTPD:
                    valueImm = CMP_IMM8::LT;
                    break;
                case Js::OpCode::CMPLEPS:
                case Js::OpCode::CMPLEPD:
                    valueImm = CMP_IMM8::LE;
                    break;
                case Js::OpCode::CMPEQPS:
                case Js::OpCode::CMPEQPD:
                    valueImm = CMP_IMM8::EQ;
                    break;
                case Js::OpCode::CMPNEQPS:
                case Js::OpCode::CMPNEQPD:
                    valueImm = CMP_IMM8::NEQ;
                    break;
                case Js::OpCode::CMPUNORDPS:
                    valueImm = CMP_IMM8::UNORD;
                    break;
                default:
                    // none comparison op, should have non-constant src2 already encoded as MODRM reg.
                    Assert(src2 && !src2->IsIntConstOpnd());
                    writeImm = false;
                }
            }
            if (writeImm)
            {TRACE_IT(16139);
                *(m_pc++) = (valueImm & 0xff);
            }
        }

#if DEBUG
        // Verify that the disassembly code for out-of-bounds typedArray handling can decode all the MOVs we emit.
        // Call it on every MOV
        if (LowererMD::IsAssign(instr) && (instr->GetDst()->IsIndirOpnd() || instr->GetSrc1()->IsIndirOpnd()))
        {TRACE_IT(16140);
            CONTEXT context = { 0 };
            EXCEPTION_POINTERS exceptionInfo;
            exceptionInfo.ContextRecord = &context;
            Js::ArrayAccessDecoder::InstructionData instrData;
            BYTE *tempPc = instrStart;

            instrData = Js::ArrayAccessDecoder::CheckValidInstr(tempPc, &exceptionInfo);

            // Make sure we can decode the instr
            Assert(!instrData.isInvalidInstr);
            // Verify the instruction size matches
            Assert(instrData.instrSizeInByte == m_pc - instrStart);
            // Note: We could verify other properties if deemed useful, like types, register values, etc...
        }
#endif

        return m_pc - instrStart;
    }
}

void
EncoderMD::EmitRexByte(BYTE * prexByte, BYTE rexByte, bool skipRexByte, bool reservedRexByte)
{TRACE_IT(16141);
    if (skipRexByte)
    {TRACE_IT(16142);
        // REX byte is not needed - let's remove it and move everything else by 1 byte
        if (((rexByte)& 0x0F) == 0x8)
        {TRACE_IT(16143);
            // If we didn't reserve the rex byte, we don't have to do anything
            if (reservedRexByte)
            {TRACE_IT(16144);
                Assert(m_pc > prexByte);
                BYTE* current = prexByte;
                while (current < m_pc)
                {TRACE_IT(16145);
                    *current = *(current + 1);
                    current++;
                }

                if (m_relocList != nullptr)
                {TRACE_IT(16146);
                    // if a reloc record was added as part of encoding this instruction - fix the pc in the reloc
                    EncodeRelocAndLabels &lastRelocEntry = m_relocList->Item(m_relocList->Count() - 1);
                    if (lastRelocEntry.m_ptr > prexByte && lastRelocEntry.m_ptr < m_pc)
                    {TRACE_IT(16147);
                        Assert(lastRelocEntry.m_type != RelocTypeLabel);
                        lastRelocEntry.m_ptr = (BYTE*)lastRelocEntry.m_ptr - 1;
                        lastRelocEntry.m_origPtr = (BYTE*)lastRelocEntry.m_origPtr - 1;
                    }
                }
            }
            return;
        }

        rexByte = rexByte & 0xF7;
    }

    // If we didn't reserve the rex byte, we need to move everything by 1 and make
    // room for it.
    if (!reservedRexByte)
    {TRACE_IT(16148);
        Assert(m_pc > prexByte);
        BYTE* current = m_pc;
        while (current > prexByte)
        {TRACE_IT(16149);
            *current = *(current - 1);
            current--;
        }

        if (m_relocList != nullptr)
        {TRACE_IT(16150);
            // if a reloc record was added as part of encoding this instruction - fix the pc in the reloc
            EncodeRelocAndLabels &lastRelocEntry = m_relocList->Item(m_relocList->Count() - 1);
            if (lastRelocEntry.m_ptr > prexByte && lastRelocEntry.m_ptr < m_pc)
            {TRACE_IT(16151);
                Assert(lastRelocEntry.m_type != RelocTypeLabel);
                lastRelocEntry.m_ptr = (BYTE*)lastRelocEntry.m_ptr + 1;
                lastRelocEntry.m_origPtr = (BYTE*)lastRelocEntry.m_origPtr + 1;
            }
        }
        m_pc++;
    }

    // Emit the rex byte
    *prexByte = rexByte;
}

bool
EncoderMD::IsExtendedRegister(RegNum reg)
{TRACE_IT(16152);
    return REGNUM_ISXMMXREG(reg) ? (reg >= RegXMM8) : (reg >= RegR8);
}

int
EncoderMD::AppendRelocEntry(RelocType type, void *ptr, IR::LabelInstr *label)
{TRACE_IT(16153);
    if (m_relocList == nullptr)
        m_relocList = Anew(m_encoder->m_tempAlloc, RelocList, m_encoder->m_tempAlloc);

    EncodeRelocAndLabels reloc;
    reloc.init(type, ptr, label);
    return m_relocList->Add(reloc);
}

int
EncoderMD::FixRelocListEntry(uint32 index, int totalBytesSaved, BYTE *buffStart, BYTE* buffEnd)
{TRACE_IT(16154);
    BYTE* currentPc;
    EncodeRelocAndLabels &relocRecord = m_relocList->Item(index);
    int result = totalBytesSaved;

    // LabelInstr ?
    if (relocRecord.isLabel())
    {TRACE_IT(16155);
        BYTE* newPC;

        currentPc = relocRecord.getLabelCurrPC();
        AssertMsg(currentPc >= buffStart && currentPc < buffEnd, "LabelInstr offset has to be within buffer.");
        newPC = currentPc - totalBytesSaved;

        // find the number of nops needed to align this loop top
        if (relocRecord.isAlignedLabel() && !PHASE_OFF(Js::LoopAlignPhase, m_func))
        {TRACE_IT(16156);
            ptrdiff_t diff = newPC - buffStart;
            Assert(diff >= 0 && diff <= UINT_MAX);
            uint32 offset = (uint32)diff;
            // Since the final code buffer is page aligned, it is enough to align the offset of the label.
            BYTE nopCount = (16 - (BYTE)(offset & 0xf)) % 16;
            if (nopCount <= Js::Configuration::Global.flags.LoopAlignNopLimit)
            {TRACE_IT(16157);
                // new label pc
                newPC += nopCount;
                relocRecord.setLabelNopCount(nopCount);
                // adjust bytes saved
                result -= nopCount;
            }
        }
        relocRecord.setLabelCurrPC(newPC);
    }
    else
    {TRACE_IT(16158);
        currentPc = (BYTE*) relocRecord.m_origPtr;
        // ignore outside buffer offsets (e.g. JumpTable entries)
        if (currentPc >= buffStart && currentPc < buffEnd)
        {TRACE_IT(16159);
            if (relocRecord.m_type == RelocTypeInlineeEntryOffset)
            {TRACE_IT(16160);
                // ptr points to imm32 offset of the instruction that needs to be adjusted
                // offset is in top 28-bits, arg count in bottom 4
                size_t field = *((size_t*) relocRecord.m_origPtr);
                size_t offset = field >> 4;
                uint32 count = field & 0xf;

                AssertMsg(offset < (size_t)(buffEnd - buffStart), "Inlinee entry offset out of range");
                relocRecord.SetInlineOffset(((offset - totalBytesSaved) << 4) | count);
            }
            // adjust the ptr to the buffer itself
            relocRecord.m_ptr = (BYTE*) relocRecord.m_ptr - totalBytesSaved;
        }
    }
    return result;
}

void EncoderMD::AddLabelReloc(BYTE* relocAddress)
{
    AppendRelocEntry(RelocTypeLabel, relocAddress);
}

///----------------------------------------------------------------------------
///
/// EncoderMD::FixMaps
/// Fixes the inlinee frame records and map based on BR shortening
///
///----------------------------------------------------------------------------
void
EncoderMD::FixMaps(uint32 brOffset, uint32 bytesSaved, uint32 *inlineeFrameRecordsIndex, uint32 *inlineeFrameMapIndex,  uint32 *pragmaInstToRecordOffsetIndex, uint32 *offsetBuffIndex)

{TRACE_IT(16161);
    InlineeFrameRecords *recList = m_encoder->m_inlineeFrameRecords;
    InlineeFrameMap *mapList = m_encoder->m_inlineeFrameMap;
    PragmaInstrList *pInstrList = m_encoder->m_pragmaInstrToRecordOffset;
    int32 i;
    for (i = *inlineeFrameRecordsIndex; i < recList->Count() && recList->Item(i)->inlineeStartOffset <= brOffset; i++)
        recList->Item(i)->inlineeStartOffset -= bytesSaved;

    *inlineeFrameRecordsIndex = i;

    for (i = *inlineeFrameMapIndex; i < mapList->Count() && mapList->Item(i).offset <= brOffset; i++)
            mapList->Item(i).offset -= bytesSaved;

    *inlineeFrameMapIndex = i;

    for (i = *pragmaInstToRecordOffsetIndex; i < pInstrList->Count() && pInstrList->Item(i)->m_offsetInBuffer <= brOffset; i++)
        pInstrList->Item(i)->m_offsetInBuffer -= bytesSaved;

    *pragmaInstToRecordOffsetIndex = i;

#if DBG_DUMP
    for (i = *offsetBuffIndex; (uint)i < m_encoder->m_instrNumber && m_encoder->m_offsetBuffer[i] <= brOffset; i++)
        m_encoder->m_offsetBuffer[i] -= bytesSaved;

    *offsetBuffIndex = i;
#endif
}

///----------------------------------------------------------------------------
///
/// EncoderMD::ApplyRelocs
///
///----------------------------------------------------------------------------

void
EncoderMD::ApplyRelocs(size_t codeBufferAddress_, size_t codeSize, uint * bufferCRC, BOOL isBrShorteningSucceeded, bool isFinalBufferValidation)
{TRACE_IT(16162);
    if (m_relocList == nullptr)
    {TRACE_IT(16163);
        return;
    }

    for (int32 i = 0; i < m_relocList->Count(); i++)
    {TRACE_IT(16164);
        EncodeRelocAndLabels *reloc = &m_relocList->Item(i);
        BYTE * relocAddress = (BYTE*)reloc->m_ptr;
        uint32 pcrel;

        switch (reloc->m_type)
        {
        case RelocTypeCallPcrel:
            AssertMsg(UNREACHED, "PC relative calls not yet supported on amd64");
#if 0
            {TRACE_IT(16165);
                pcrel = (uint32)(codeBufferAddress + (BYTE*)reloc->m_ptr - m_encoder->m_encodeBuffer + 4);
                 *(uint32 *)relocAddress -= pcrel;
                break;
            }
#endif
        case RelocTypeBranch:
            {TRACE_IT(16166);
                // The address of the target LabelInstr is saved at the reloc address.
                IR::LabelInstr * labelInstr = reloc->getBrTargetLabel();
                AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");
                if (reloc->isShortBr() )
                {TRACE_IT(16167);
                    // short branch
                    pcrel = (uint32)(labelInstr->GetPC() - ((BYTE*)reloc->m_ptr + 1));
                    AssertMsg((int32)pcrel >= -128 && (int32)pcrel <= 127, "Offset doesn't fit in imm8.");
                    if (!isFinalBufferValidation)
                    {TRACE_IT(16168);
                        Assert(*(BYTE*)relocAddress == 0);
                        *(BYTE*)relocAddress = (BYTE)pcrel;
                    }
                    else
                    {TRACE_IT(16169);
                        Encoder::EnsureRelocEntryIntegrity(codeBufferAddress_, codeSize, (size_t)m_encoder->m_encodeBuffer, (size_t)relocAddress, sizeof(BYTE), (ptrdiff_t)labelInstr->GetPC() - ((ptrdiff_t)reloc->m_ptr + 1));
                    }
                }
                else
                {TRACE_IT(16170);
                    pcrel = (uint32)(labelInstr->GetPC() - ((BYTE*)reloc->m_ptr + 4));
                    if (!isFinalBufferValidation)
                    {TRACE_IT(16171);
                        Assert(*(uint32*)relocAddress == 0);
                        *(uint32 *)relocAddress = pcrel;
                    }
                    else
                    {TRACE_IT(16172);
                        Encoder::EnsureRelocEntryIntegrity(codeBufferAddress_, codeSize, (size_t)m_encoder->m_encodeBuffer, (size_t)relocAddress, sizeof(uint32), (ptrdiff_t)labelInstr->GetPC() - ((ptrdiff_t)reloc->m_ptr + 4));
                    }
                }

                *bufferCRC = Encoder::CalculateCRC(*bufferCRC, pcrel);

                break;
            }

        case RelocTypeLabelUse:
            {TRACE_IT(16173);
                IR::LabelInstr *labelInstr = reloc->getBrTargetLabel();
                AssertMsg(labelInstr->GetPC() != nullptr, "Branch to unemitted label?");

                size_t offset = (size_t)(labelInstr->GetPC() - m_encoder->m_encodeBuffer);
                size_t targetAddress = (size_t)(offset + codeBufferAddress_);
                if (!isFinalBufferValidation)
                {TRACE_IT(16174);
                    Assert(*(size_t *)relocAddress == 0);
                    *(size_t *)relocAddress = targetAddress;
                }
                else
                {TRACE_IT(16175);
                    Encoder::EnsureRelocEntryIntegrity(codeBufferAddress_, codeSize, (size_t)m_encoder->m_encodeBuffer, (size_t)relocAddress, sizeof(size_t), targetAddress, false);
                }
                *bufferCRC = Encoder::CalculateCRC(*bufferCRC, offset);
                break;
            }
        case RelocTypeLabel:
        case RelocTypeAlignedLabel:
        case RelocTypeInlineeEntryOffset:
            break;
        default:
            AssertMsg(UNREACHED, "Unknown reloc type");
        }
    }
}

uint 
EncoderMD::GetRelocDataSize(EncodeRelocAndLabels *reloc)
{TRACE_IT(16176);
    switch (reloc->m_type)
    {
        case RelocTypeBranch:
        {TRACE_IT(16177);
            if (reloc->isShortBr())
            {TRACE_IT(16178);
                return sizeof(BYTE);
            }
            else
            {TRACE_IT(16179);
                return sizeof(uint);
            }
        }
        case RelocTypeLabelUse:
        {TRACE_IT(16180);
            return sizeof(size_t);
        }
        default:
        {
            return 0;
        }
    }
}

BYTE * 
EncoderMD::GetRelocBufferAddress(EncodeRelocAndLabels * reloc)
{TRACE_IT(16181);
    return (BYTE*)reloc->m_ptr;
}

#ifdef DBG
///----------------------------------------------------------------------------
///
/// EncodeRelocAndLabels::VerifyRelocList
/// Verify that the list of offsets within the encoder buffer range are in ascending order.
/// This includes offsets of immediate fields in the code and offsets of LabelInstrs
///----------------------------------------------------------------------------
void
EncoderMD::VerifyRelocList(BYTE *buffStart, BYTE *buffEnd)
{TRACE_IT(16182);
    BYTE *last_pc = 0, *pc;

    if (m_relocList == nullptr)
    {TRACE_IT(16183);
        return;
    }

    for (int32 i = 0; i < m_relocList->Count(); i ++)
    {TRACE_IT(16184);
        EncodeRelocAndLabels &p = m_relocList->Item(i);
        // LabelInstr ?
        if (p.isLabel())
        {TRACE_IT(16185);
            AssertMsg(p.m_ptr < buffStart || p.m_ptr >= buffEnd, "Invalid label instruction pointer.");
            pc = ((IR::LabelInstr*)p.m_ptr)->GetPC();
            AssertMsg(pc >= buffStart && pc < buffEnd, "LabelInstr offset has to be within buffer.");
        }
        else
            pc = (BYTE*)p.m_ptr;

        // The list is partially sorted, out of bound ptrs (JumpTable entries) don't follow.
        if (pc >= buffStart && pc < buffEnd)
        {TRACE_IT(16186);
            if (last_pc)
                AssertMsg(pc >= last_pc, "Unordered reloc list.");
            last_pc = pc;
        }
    }
}
#endif

BYTE
EncoderMD::GetRexByte(BYTE rexCode, IR::Opnd * opnd)
{TRACE_IT(16187);
    return this->GetRexByte(rexCode, opnd->AsRegOpnd()->GetReg());
}

BYTE
EncoderMD::GetRexByte(BYTE rexCode, RegNum reg)
{TRACE_IT(16188);
    if (this->IsExtendedRegister(reg))
    {TRACE_IT(16189);
        return rexCode;
    }
    else
    {TRACE_IT(16190);
        return 0;
    }
}

void
EncoderMD::EncodeInlineeCallInfo(IR::Instr *instr, uint32 codeOffset)
{TRACE_IT(16191);
    Assert(instr->GetSrc1() &&
        instr->GetSrc1()->IsIntConstOpnd() &&
        (instr->GetSrc1()->AsIntConstOpnd()->GetValue() == (instr->GetSrc1()->AsIntConstOpnd()->GetValue() & 0xF)));
    intptr_t inlineeCallInfo = 0;
    // 60 (AMD64) bits on the InlineeCallInfo to store the
    // offset of the start of the inlinee. We shouldn't have gotten here with more arguments
    // than can fit in as many bits.
    const bool encodeResult = Js::InlineeCallInfo::Encode(inlineeCallInfo,
        instr->GetSrc1()->AsIntConstOpnd()->GetValue(), codeOffset);
    Assert(encodeResult);

    instr->GetSrc1()->AsIntConstOpnd()->SetValue(inlineeCallInfo);
}

bool EncoderMD::TryConstFold(IR::Instr *instr, IR::RegOpnd *regOpnd)
{TRACE_IT(16192);
    Assert(regOpnd->m_sym->IsConst());

    if (regOpnd->m_sym->IsFloatConst() || regOpnd->m_sym->IsInt64Const())
    {TRACE_IT(16193);
        return false;
    }

    bool isNotLargeConstant = Math::FitsInDWord(regOpnd->m_sym->GetLiteralConstValue_PostGlobOpt());

    if (!isNotLargeConstant && (!EncoderMD::IsMOVEncoding(instr) || !instr->GetDst()->IsRegOpnd()))
    {TRACE_IT(16194);
        return false;
    }

    switch (GetInstrForm(instr))
    {
    case FORM_MOV:
        if (!instr->GetSrc1()->IsRegOpnd())
        {TRACE_IT(16195);
            return false;
        }
        break;

    case FORM_PSHPOP:
        if (instr->m_opcode != Js::OpCode::PUSH)
        {TRACE_IT(16196);
            return false;
        }
        if (!instr->GetSrc1()->IsRegOpnd())
        {TRACE_IT(16197);
            return false;
        }
        break;

    case FORM_BINOP:
    case FORM_SHIFT:
        if (regOpnd != instr->GetSrc2())
        {TRACE_IT(16198);
            return false;
        }
        break;

    default:
        return false;
    }

    if (regOpnd != instr->GetSrc1() && regOpnd != instr->GetSrc2())
    {TRACE_IT(16199);
        if (!regOpnd->m_sym->IsConst() || regOpnd->m_sym->IsFloatConst())
        {TRACE_IT(16200);
            return false;
        }

        // Check if it's the index opnd inside an indir
        bool foundUse = false;
        bool foldedAllUses = true;
        IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2(), instr->GetDst() };
        for (int i = 0; i < sizeof(srcs) / sizeof(srcs[0]); ++i)
        {TRACE_IT(16201);
            const auto src = srcs[i];
            if (!src || !src->IsIndirOpnd())
            {TRACE_IT(16202);
                continue;
            }

            const auto indir = src->AsIndirOpnd();
            if (regOpnd == indir->GetBaseOpnd())
            {TRACE_IT(16203);
                // Can't const-fold into the base opnd
                foundUse = true;
                foldedAllUses = false;
                continue;
            }
            if (regOpnd != indir->GetIndexOpnd())
            {TRACE_IT(16204);
                continue;
            }

            foundUse = true;
            if (!regOpnd->m_sym->IsIntConst())
            {TRACE_IT(16205);
                foldedAllUses = false;
                continue;
            }

            // offset = indir.offset + (index << scale)
            int32 offset = regOpnd->m_sym->GetIntConstValue();
            if ((indir->GetScale() != 0 && Int32Math::Shl(offset, indir->GetScale(), &offset)) ||
                (indir->GetOffset() != 0 && Int32Math::Add(indir->GetOffset(), offset, &offset)))
            {TRACE_IT(16206);
                foldedAllUses = false;
                continue;
            }
            indir->SetOffset(offset);
            indir->SetIndexOpnd(nullptr);
        }

        return foundUse && foldedAllUses;
    }

    instr->ReplaceSrc(regOpnd, regOpnd->m_sym->GetConstOpnd());
    return true;
}

bool EncoderMD::TryFold(IR::Instr *instr, IR::RegOpnd *regOpnd)
{TRACE_IT(16207);
    IR::Opnd *src1 = instr->GetSrc1();
    IR::Opnd *src2 = instr->GetSrc2();

    if (IRType_IsSimd128(regOpnd->GetType()))
    {TRACE_IT(16208);
        // No folding for SIMD values. Alignment is not guaranteed.
        return false;
    }

    switch (GetInstrForm(instr))
    {
    case FORM_MOV:
        if (!instr->GetDst()->IsRegOpnd() || regOpnd != src1)
        {TRACE_IT(16209);
            return false;
        }
        break;

    case FORM_BINOP:

        if (regOpnd == src1 && instr->m_opcode == Js::OpCode::CMP && (src2->IsRegOpnd() || src1->IsImmediateOpnd()))
        {TRACE_IT(16210);
            IR::Instr *instrNext = instr->GetNextRealInstrOrLabel();

            if (instrNext->IsBranchInstr() && instrNext->AsBranchInstr()->IsConditional())
            {TRACE_IT(16211);
                // Swap src and reverse branch
                src2 = instr->UnlinkSrc1();
                src1 = instr->UnlinkSrc2();
                instr->SetSrc1(src1);
                instr->SetSrc2(src2);
                LowererMD::ReverseBranch(instrNext->AsBranchInstr());
            }
            else
            {TRACE_IT(16212);
                return false;
            }
        }
        if (regOpnd != src2 || !src1->IsRegOpnd())
        {TRACE_IT(16213);
            return false;
        }
        break;

    case FORM_MODRM:
        if (src2 == nullptr)
        {TRACE_IT(16214);
            if (!instr->GetDst()->IsRegOpnd() || regOpnd != src1 || EncoderMD::IsOPEQ(instr))
            {TRACE_IT(16215);
                return false;
            }
        }
        else
        {TRACE_IT(16216);
            if (regOpnd != src2 || !src1->IsRegOpnd())
            {TRACE_IT(16217);
                return false;
            }
        }
        break;

    case FORM_PSHPOP:
        if (instr->m_opcode != Js::OpCode::PUSH)
        {TRACE_IT(16218);
            return false;
        }
        if (!instr->GetSrc1()->IsRegOpnd())
        {TRACE_IT(16219);
            return false;
        }
        break;

    case FORM_TEST:
        if (regOpnd == src1)
        {TRACE_IT(16220);
            if (!src2->IsRegOpnd() && !src2->IsIntConstOpnd())
            {TRACE_IT(16221);
                return false;
            }
        }
        else if (src1->IsRegOpnd())
        {TRACE_IT(16222);
            instr->SwapOpnds();
        }
        else
        {TRACE_IT(16223);
            return false;
        }
        break;

    default:
        return false;
    }

    IR::SymOpnd *symOpnd = IR::SymOpnd::New(regOpnd->m_sym, regOpnd->GetType(), instr->m_func);
    instr->ReplaceSrc(regOpnd, symOpnd);
    return true;
}

bool EncoderMD::SetsConditionCode(IR::Instr *instr)
{TRACE_IT(16224);
    return instr->IsLowered() && (EncoderMD::GetOpdope(instr) & DSETCC);
}

bool EncoderMD::UsesConditionCode(IR::Instr *instr)
{TRACE_IT(16225);
    return instr->IsLowered() && (EncoderMD::GetOpdope(instr) & DUSECC);
}

bool EncoderMD::IsOPEQ(IR::Instr *instr)
{TRACE_IT(16226);
    return instr->IsLowered() && (EncoderMD::GetOpdope(instr) & DOPEQ);
}

bool EncoderMD::IsSHIFT(IR::Instr *instr)
{TRACE_IT(16227);
    return (instr->IsLowered() && EncoderMD::GetInstrForm(instr) == FORM_SHIFT) ||
        instr->m_opcode == Js::OpCode::PSLLDQ || instr->m_opcode == Js::OpCode::PSRLDQ ||
        instr->m_opcode == Js::OpCode::PSLLW || instr->m_opcode == Js::OpCode::PSRLW ||
        instr->m_opcode == Js::OpCode::PSLLD;
}

bool EncoderMD::IsMOVEncoding(IR::Instr *instr)
{TRACE_IT(16228);
    return instr->IsLowered() && (EncoderMD::GetOpdope(instr) & DMOV);
}

void EncoderMD::UpdateRelocListWithNewBuffer(RelocList * relocList, BYTE * newBuffer, BYTE * oldBufferStart, BYTE * oldBufferEnd)
{TRACE_IT(16229);
    for (int32 i = 0; i < relocList->Count(); i++)
    {TRACE_IT(16230);
        EncodeRelocAndLabels &reloc = relocList->Item(i);
        if (reloc.isLabel())
        {TRACE_IT(16231);
            IR::LabelInstr* label = reloc.getLabel();
            BYTE* labelPC = label->GetPC();
            Assert((BYTE*) reloc.m_origPtr >= oldBufferStart && (BYTE*) reloc.m_origPtr < oldBufferEnd);
            label->SetPC(labelPC - oldBufferStart + newBuffer);
            // nothing more to be done for a label
            continue;
        }
        else if (reloc.m_type >= RelocTypeBranch && reloc.m_type <= RelocTypeLabelUse &&
            (BYTE*) reloc.m_origPtr >= oldBufferStart && (BYTE*) reloc.m_origPtr < oldBufferEnd)
        {TRACE_IT(16232);
            // we need to relocate all new offset that were originally within buffer
            reloc.m_ptr = (BYTE*) reloc.m_ptr - oldBufferStart + newBuffer;
        }
    }
}
