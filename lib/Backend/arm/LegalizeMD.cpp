//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

#undef MACRO

#define MACRO(name, jnLayout, attrib, byte2, legalforms, opbyte, ...) legalforms,

static const LegalInstrForms _InstrForms[] =
{
#include "MdOpCodes.h"
};


static LegalForms LegalDstForms(IR::Instr * instr)
{TRACE_IT(16891);
    Assert(instr->IsLowered());
    return _InstrForms[instr->m_opcode - (Js::OpCode::MDStart+1)].dst;
}

static LegalForms LegalSrcForms(IR::Instr * instr, uint opndNum)
{TRACE_IT(16892);
    Assert(instr->IsLowered());
    return _InstrForms[instr->m_opcode - (Js::OpCode::MDStart+1)].src[opndNum-1];
}

void LegalizeMD::LegalizeInstr(IR::Instr * instr, bool fPostRegAlloc)
{TRACE_IT(16893);
    if (!instr->IsLowered())
    {
        AssertMsg(UNREACHED, "Unlowered instruction in m.d. legalizer");
        return;
    }

    LegalizeDst(instr, fPostRegAlloc);
    LegalizeSrc(instr, instr->GetSrc1(), 1, fPostRegAlloc);
    LegalizeSrc(instr, instr->GetSrc2(), 2, fPostRegAlloc);
}

void LegalizeMD::LegalizeDst(IR::Instr * instr, bool fPostRegAlloc)
{TRACE_IT(16894);
    LegalForms forms = LegalDstForms(instr);

    IR::Opnd * opnd = instr->GetDst();
    if (opnd == NULL)
    {TRACE_IT(16895);
#ifdef DBG
        // No legalization possible, just report error.
        if (forms != 0)
        {
            IllegalInstr(instr, _u("Expected dst opnd"));
        }
#endif
        return;
    }

    switch (opnd->GetKind())
    {
    case IR::OpndKindReg:
#ifdef DBG
        // No legalization possible, just report error.
        if (!(forms & L_RegMask))
        {
            IllegalInstr(instr, _u("Unexpected reg dst"));
        }
#endif
        break;

    case IR::OpndKindMemRef:
    {TRACE_IT(16896);
        // MemRefOpnd is a deference of the memory location.
        // So extract the location, load it to register, replace the MemRefOpnd with an IndirOpnd taking the
        // register as base, and fall through to legalize the IndirOpnd.
        intptr_t memLoc = opnd->AsMemRefOpnd()->GetMemLoc();
        IR::RegOpnd *newReg = IR::RegOpnd::New(TyMachPtr, instr->m_func);
        if (fPostRegAlloc)
        {TRACE_IT(16897);
            newReg->SetReg(SCRATCH_REG);
        }
        IR::Instr *newInstr = IR::Instr::New(Js::OpCode::LDIMM, newReg,
            IR::AddrOpnd::New(memLoc, opnd->AsMemRefOpnd()->GetAddrKind(), instr->m_func, true), instr->m_func);
        instr->InsertBefore(newInstr);
        LegalizeMD::LegalizeInstr(newInstr, fPostRegAlloc);
        IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(newReg, 0, opnd->GetType(), instr->m_func);
        opnd = instr->ReplaceDst(indirOpnd);
    }
    // FALL THROUGH
    case IR::OpndKindIndir:
        if (!(forms & L_IndirMask))
        {TRACE_IT(16898);
            instr = LegalizeStore(instr, forms, fPostRegAlloc);
            forms = LegalDstForms(instr);
        }
        LegalizeIndirOffset(instr, opnd->AsIndirOpnd(), forms, fPostRegAlloc);
        break;

    case IR::OpndKindSym:
        if (!(forms & L_SymMask))
        {TRACE_IT(16899);
            instr = LegalizeStore(instr, forms, fPostRegAlloc);
            forms = LegalDstForms(instr);
        }

        if (fPostRegAlloc)
        {
            // In order to legalize SymOffset we need to know final argument area, which is only available after lowerer.
            // So, don't legalize sym offset here, but it will be done as part of register allocator.
            LegalizeSymOffset(instr, opnd->AsSymOpnd(), forms, fPostRegAlloc);
        }
        break;

    default:
        AssertMsg(UNREACHED, "Unexpected dst opnd kind");
        break;
    }
}

IR::Instr * LegalizeMD::LegalizeStore(IR::Instr *instr, LegalForms forms, bool fPostRegAlloc)
{TRACE_IT(16900);
    if (LowererMD::IsAssign(instr) && instr->GetSrc1()->IsRegOpnd())
    {TRACE_IT(16901);
        // We can just change this to a store in place.
        instr->m_opcode = LowererMD::GetStoreOp(instr->GetSrc1()->GetType());
    }
    else
    {TRACE_IT(16902);
        // Sink the mem opnd. The caller will verify the offset.
        // We don't expect to hit this point after register allocation, because we
        // can't guarantee that the instruction will be legal.
        Assert(!fPostRegAlloc);
        instr = instr->SinkDst(LowererMD::GetStoreOp(instr->GetDst()->GetType()), RegNOREG);
    }

    return instr;
}

void LegalizeMD::LegalizeSrc(IR::Instr * instr, IR::Opnd * opnd, uint opndNum, bool fPostRegAlloc)
{TRACE_IT(16903);
    LegalForms forms = LegalSrcForms(instr, opndNum);
    if (opnd == NULL)
    {TRACE_IT(16904);
#ifdef DBG
        // No legalization possible, just report error.
        if (forms != 0)
        {
            IllegalInstr(instr, _u("Expected src %d opnd"), opndNum);
        }
#endif
        return;
    }

    switch (opnd->GetKind())
    {
    case IR::OpndKindReg:
        // No legalization possible, just report error.
#ifdef DBG
        if (!(forms & L_RegMask))
        {
            IllegalInstr(instr, _u("Unexpected reg as src%d opnd"), opndNum);
        }
#endif
        break;

    case IR::OpndKindAddr:
    case IR::OpndKindHelperCall:
    case IR::OpndKindIntConst:
        LegalizeImmed(instr, opnd, opndNum, opnd->GetImmediateValueAsInt32(instr->m_func), forms, fPostRegAlloc);
        break;

    case IR::OpndKindLabel:
        LegalizeLabelOpnd(instr, opnd, opndNum, fPostRegAlloc);
        break;

    case IR::OpndKindMemRef:
    {TRACE_IT(16905);
        // MemRefOpnd is a deference of the memory location.
        // So extract the location, load it to register, replace the MemRefOpnd with an IndirOpnd taking the
        // register as base, and fall through to legalize the IndirOpnd.
        intptr_t memLoc = opnd->AsMemRefOpnd()->GetMemLoc();
        IR::RegOpnd *newReg = IR::RegOpnd::New(TyMachPtr, instr->m_func);
        if (fPostRegAlloc)
        {TRACE_IT(16906);
            newReg->SetReg(SCRATCH_REG);
        }
        IR::Instr *newInstr = IR::Instr::New(Js::OpCode::LDIMM, newReg, IR::AddrOpnd::New(memLoc, IR::AddrOpndKindDynamicMisc, instr->m_func), instr->m_func);
        instr->InsertBefore(newInstr);
        LegalizeMD::LegalizeInstr(newInstr, fPostRegAlloc);
        IR::IndirOpnd *indirOpnd = IR::IndirOpnd::New(newReg, 0, opnd->GetType(), instr->m_func);
        if (opndNum == 1)
        {TRACE_IT(16907);
            opnd = instr->ReplaceSrc1(indirOpnd);
        }
        else
        {TRACE_IT(16908);
            opnd = instr->ReplaceSrc2(indirOpnd);
        }
    }
    // FALL THROUGH
    case IR::OpndKindIndir:
        if (!(forms & L_IndirMask))
        {TRACE_IT(16909);
            instr = LegalizeLoad(instr, opndNum, forms, fPostRegAlloc);
            forms = LegalSrcForms(instr, 1);
        }
        LegalizeIndirOffset(instr, opnd->AsIndirOpnd(), forms, fPostRegAlloc);
        break;

    case IR::OpndKindSym:
        if (!(forms & L_SymMask))
        {TRACE_IT(16910);
            instr = LegalizeLoad(instr, opndNum, forms, fPostRegAlloc);
            forms = LegalSrcForms(instr, 1);
        }

        if (fPostRegAlloc)
        {
            // In order to legalize SymOffset we need to know final argument area, which is only available after lowerer.
            // So, don't legalize sym offset here, but it will be done as part of register allocator.
            LegalizeSymOffset(instr, opnd->AsSymOpnd(), forms, fPostRegAlloc);
        }
        break;

    default:
        AssertMsg(UNREACHED, "Unexpected src opnd kind");
        break;
    }
}

IR::Instr * LegalizeMD::LegalizeLoad(IR::Instr *instr, uint opndNum, LegalForms forms, bool fPostRegAlloc)
{TRACE_IT(16911);
    if (LowererMD::IsAssign(instr) && instr->GetDst()->IsRegOpnd())
    {TRACE_IT(16912);
        // We can just change this to a load in place.
        instr->m_opcode = LowererMD::GetLoadOp(instr->GetDst()->GetType());
    }
    else
    {TRACE_IT(16913);
        // Hoist the memory opnd. The caller will verify the offset.
        if (opndNum == 1)
        {TRACE_IT(16914);
            AssertMsg(!fPostRegAlloc || instr->GetSrc1()->GetType() == TyMachReg, "Post RegAlloc other types disallowed");
            instr = instr->HoistSrc1(LowererMD::GetLoadOp(instr->GetSrc1()->GetType()), fPostRegAlloc ? SCRATCH_REG : RegNOREG);
        }
        else
        {TRACE_IT(16915);
            AssertMsg(!fPostRegAlloc || instr->GetSrc2()->GetType() == TyMachReg, "Post RegAlloc other types disallowed");
            instr = instr->HoistSrc2(LowererMD::GetLoadOp(instr->GetSrc2()->GetType()), fPostRegAlloc ? SCRATCH_REG : RegNOREG);
        }
    }

    return instr;
}

void LegalizeMD::LegalizeIndirOffset(IR::Instr * instr, IR::IndirOpnd * indirOpnd, LegalForms forms, bool fPostRegAlloc)
{TRACE_IT(16916);
    if (forms & (L_VIndirI11))
    {TRACE_IT(16917);
        // Vfp doesn't support register indirect operation
        LegalizeMD::LegalizeIndirOpndForVFP(instr, indirOpnd, fPostRegAlloc);
        return;
    }

    int32 offset = indirOpnd->GetOffset();

    if (indirOpnd->GetIndexOpnd() != NULL && offset != 0)
    {TRACE_IT(16918);
        IR::Instr *addInstr = instr->HoistIndirOffset(indirOpnd, fPostRegAlloc ? SCRATCH_REG : RegNOREG);
        LegalizeMD::LegalizeInstr(addInstr, fPostRegAlloc);
        return;
    }

    if (forms & (L_IndirI8 | L_IndirU12I8))
    {TRACE_IT(16919);
        if (IS_CONST_INT8(offset))
        {TRACE_IT(16920);
            return;
        }
    }

    if (forms & (L_IndirU12I8 | L_IndirU12))
    {TRACE_IT(16921);
        if (IS_CONST_UINT12(offset))
        {TRACE_IT(16922);
            return;
        }
    }

    // Offset is too large, so hoist it and replace it with an index, only valid for Thumb & Thumb2
    IR::Instr *addInstr = instr->HoistIndirOffset(indirOpnd, fPostRegAlloc ? SCRATCH_REG : RegNOREG);
    LegalizeMD::LegalizeInstr(addInstr, fPostRegAlloc);
}

void LegalizeMD::LegalizeSymOffset(
    IR::Instr * instr,
    IR::SymOpnd * symOpnd,
    LegalForms forms,
    bool fPostRegAlloc)
{
    AssertMsg(fPostRegAlloc, "LegalizeMD::LegalizeSymOffset can (and will) be called as part of register allocation. Can't call it as part of lowerer, as final argument area is not available yet.");

    RegNum baseReg;
    int32 offset;

    if (!symOpnd->m_sym->IsStackSym())
    {TRACE_IT(16923);
        return;
    }

    EncoderMD::BaseAndOffsetFromSym(symOpnd, &baseReg, &offset, instr->m_func->GetTopFunc());

    if (forms & (L_SymU12I8 | L_SymU12))
    {TRACE_IT(16924);
        if (IS_CONST_UINT12(offset))
        {TRACE_IT(16925);
            return;
        }
    }

    if (forms & L_SymU12I8)
    {TRACE_IT(16926);
        if (IS_CONST_INT8(offset))
        {TRACE_IT(16927);
            return;
        }
    }

    if (forms & (L_VSymI11))
    {TRACE_IT(16928);
        if (IS_CONST_UINT10((offset < 0? -offset: offset)))
        {TRACE_IT(16929);
            return;
        }

        IR::RegOpnd *baseOpnd = IR::RegOpnd::New(NULL, baseReg, TyMachPtr, instr->m_func);
        IR::Instr* instrAdd = instr->HoistSymOffsetAsAdd(symOpnd, baseOpnd, offset, fPostRegAlloc ? SCRATCH_REG : RegNOREG);
        LegalizeMD::LegalizeInstr(instrAdd, fPostRegAlloc);
        return;
    }

    IR::Instr * newInstr;
    if (instr->m_opcode == Js::OpCode::LEA)
    {TRACE_IT(16930);
        instr->m_opcode = Js::OpCode::ADD;
        instr->ReplaceSrc1(IR::RegOpnd::New(NULL, baseReg, TyMachPtr, instr->m_func));
        instr->SetSrc2(IR::IntConstOpnd::New(offset, TyMachReg, instr->m_func));
        newInstr = instr->HoistSrc2(Js::OpCode::LDIMM, fPostRegAlloc ? SCRATCH_REG : RegNOREG);
        LegalizeMD::LegalizeInstr(newInstr, fPostRegAlloc);
        LegalizeMD::LegalizeInstr(instr, fPostRegAlloc);
    }
    else
    {TRACE_IT(16931);
        newInstr = instr->HoistSymOffset(symOpnd, baseReg, offset, fPostRegAlloc ? SCRATCH_REG : RegNOREG);
        LegalizeMD::LegalizeInstr(newInstr, fPostRegAlloc);
    }
}

void LegalizeMD::LegalizeImmed(
    IR::Instr * instr,
    IR::Opnd * opnd,
    uint opndNum,
    IntConstType immed,
    LegalForms forms,
    bool fPostRegAlloc)
{TRACE_IT(16932);
    if (!(((forms & L_ImmModC12) && EncoderMD::CanEncodeModConst12(immed)) ||
          ((forms & L_ImmU5) && IS_CONST_UINT5(immed)) ||
          ((forms & L_ImmU12) && IS_CONST_UINT12(immed)) ||
          ((forms & L_ImmU16) && IS_CONST_UINT16(immed))))
    {TRACE_IT(16933);
        if (instr->m_opcode != Js::OpCode::LDIMM)
        {TRACE_IT(16934);
            instr = LegalizeMD::GenerateLDIMM(instr, opndNum, fPostRegAlloc ? SCRATCH_REG : RegNOREG);
        }

        if (fPostRegAlloc)
        {TRACE_IT(16935);
            LegalizeMD::LegalizeLDIMM(instr, immed);
        }
    }
}

void LegalizeMD::LegalizeLabelOpnd(
    IR::Instr * instr,
    IR::Opnd * opnd,
    uint opndNum,
    bool fPostRegAlloc)
{TRACE_IT(16936);
    if (instr->m_opcode != Js::OpCode::LDIMM)
    {TRACE_IT(16937);
        instr = LegalizeMD::GenerateLDIMM(instr, opndNum, fPostRegAlloc ? SCRATCH_REG : RegNOREG);
    }
    if (fPostRegAlloc)
    {TRACE_IT(16938);
        LegalizeMD::LegalizeLdLabel(instr, opnd);
    }
}

IR::Instr * LegalizeMD::GenerateLDIMM(IR::Instr * instr, uint opndNum, RegNum scratchReg)
{TRACE_IT(16939);
    if (LowererMD::IsAssign(instr) && instr->GetDst()->IsRegOpnd())
    {TRACE_IT(16940);
        instr->m_opcode = Js::OpCode::LDIMM;
    }
    else
    {TRACE_IT(16941);
        if (opndNum == 1)
        {TRACE_IT(16942);
            instr = instr->HoistSrc1(Js::OpCode::LDIMM, scratchReg);
        }
        else
        {TRACE_IT(16943);
            instr = instr->HoistSrc2(Js::OpCode::LDIMM, scratchReg);
        }
    }

    return instr;
}

void LegalizeMD::LegalizeLDIMM(IR::Instr * instr, IntConstType immed)
{TRACE_IT(16944);
    // In case of inlined entry instruction, we don't know the offset till the encoding phase
    if (!instr->isInlineeEntryInstr)
    {TRACE_IT(16945);
        if (IS_CONST_UINT16(immed) || EncoderMD::CanEncodeModConst12(immed))
        {TRACE_IT(16946);
            instr->m_opcode = Js::OpCode::MOV;
            return;
        }
        bool fDontEncode = Security::DontEncode(instr->GetSrc1());

        IR::IntConstOpnd *src1 = IR::IntConstOpnd::New(immed & 0x0000FFFF, TyInt16, instr->m_func);
        IR::Instr * instrMov = IR::Instr::New(Js::OpCode::MOV, instr->GetDst(), src1, instr->m_func);
        instr->InsertBefore(instrMov);

        src1 = IR::IntConstOpnd::New((immed & 0xFFFF0000)>>16, TyInt16, instr->m_func);
        instr->ReplaceSrc1(src1);
        instr->m_opcode = Js::OpCode::MOVT;

        if (!fDontEncode)
        {TRACE_IT(16947);
            LegalizeMD::ObfuscateLDIMM(instrMov, instr);
        }
    }
    else
    {TRACE_IT(16948);
        Assert(Security::DontEncode(instr->GetSrc1()));
        IR::LabelInstr *label = IR::LabelInstr::New(Js::OpCode::Label, instr->m_func, false);
        instr->InsertBefore(label);
        Assert((immed & 0x0000000F) == immed);
        label->SetOffset(immed);

        IR::LabelOpnd *target = IR::LabelOpnd::New(label, instr->m_func);

        IR::Instr * instrMov = IR::Instr::New(Js::OpCode::MOVW, instr->GetDst(), target, instr->m_func);
        instr->InsertBefore(instrMov);

        instr->ReplaceSrc1(target);
        instr->m_opcode = Js::OpCode::MOVT;

        label->isInlineeEntryInstr = true;
        instr->isInlineeEntryInstr = false;
    }
}

void LegalizeMD::ObfuscateLDIMM(IR::Instr * instrMov, IR::Instr * instrMovt)
{TRACE_IT(16949);
    // Are security measures disabled?

    if (CONFIG_ISENABLED(Js::DebugFlag) ||
        CONFIG_ISENABLED(Js::BenchmarkFlag) ||
        PHASE_OFF(Js::EncodeConstantsPhase, instrMov->m_func->GetTopFunc())
        )
    {TRACE_IT(16950);
        return;
    }

    UINT_PTR rand = Math::Rand();

    // Use this random value as follows:
    // bits 0-3: reg to use in pre-LDIMM instr
    // bits 4: do/don't emit pre-LDIMM instr
    // bits 5-6:  emit and/or/add/mov as pre-LDIMM instr
    // Similarly for bits 7-13 (mid-LDIMM) and 14-20 (post-LDIMM)

    RegNum targetReg = instrMov->GetDst()->AsRegOpnd()->GetReg();

    LegalizeMD::EmitRandomNopBefore(instrMov, rand, targetReg);
    LegalizeMD::EmitRandomNopBefore(instrMovt, rand >> 7, targetReg);
    LegalizeMD::EmitRandomNopBefore(instrMovt->m_next, rand >> 14, targetReg);
}

void LegalizeMD::EmitRandomNopBefore(IR::Instr *insertInstr, UINT_PTR rand, RegNum targetReg)
{TRACE_IT(16951);
    // bits 0-3: reg to use in pre-LDIMM instr
    // bits 4: do/don't emit pre-LDIMM instr
    // bits 5-6: emit and/or/add/mov as pre-LDIMM instr

    if (!(rand & (1 << 4)) && !PHASE_FORCE(Js::EncodeConstantsPhase, insertInstr->m_func->GetTopFunc()))
    {TRACE_IT(16952);
        return;
    }

    IR::Instr * instr;
    IR::RegOpnd * opnd1;
    IR::Opnd * opnd2 = NULL;
    Js::OpCode op = Js::OpCode::InvalidOpCode;

    RegNum regNum = (RegNum)((rand & ((1 << 4) - 1)) + RegR0);

    opnd1 = IR::RegOpnd::New(NULL, regNum, TyMachReg, insertInstr->m_func);

    if (regNum == RegSP || regNum == RegPC || regNum == targetReg) //skip sp & pc & the target reg
    {TRACE_IT(16953);
        // ORR pc,pc,0 has unpredicted behavior.
        // AND sp,sp,sp has unpredicted behavior.
        // We avoid target reg to avoid pipeline stalls.
        // Less likely target reg will be RegR12 as we insert nops only for user defined constants and
        // RegR12 is mostly used for temporary data such as legalizer post regalloc.
        opnd1->SetReg(RegR12);
    }

    switch ((rand >> 5) & 3)
    {
    case 0:
        op = Js::OpCode::AND;
        opnd2 = opnd1;
        break;
    case 1:
        op = Js::OpCode::ORR;
        opnd2 = IR::IntConstOpnd::New(0, TyMachReg, insertInstr->m_func);
        break;
    case 2:
        op = Js::OpCode::ADD;
        opnd2 = IR::IntConstOpnd::New(0, TyMachReg, insertInstr->m_func);
        break;
    case 3:
        op = Js::OpCode::MOV;
        break;
    }

    instr = IR::Instr::New(op, opnd1, opnd1, insertInstr->m_func);
    if (opnd2)
    {TRACE_IT(16954);
        instr->SetSrc2(opnd2);
    }
    insertInstr->InsertBefore(instr);
}

void LegalizeMD::LegalizeLdLabel(IR::Instr * instr, IR::Opnd * opnd)
{TRACE_IT(16955);
    Assert(instr->m_opcode == Js::OpCode::LDIMM);
    Assert(opnd->IsLabelOpnd());

    IR::Instr * instrMov = IR::Instr::New(Js::OpCode::MOVW, instr->GetDst(), opnd, instr->m_func);
    instr->InsertBefore(instrMov);

    instr->m_opcode = Js::OpCode::MOVT;
}

bool LegalizeMD::LegalizeDirectBranch(IR::BranchInstr *branchInstr, uint32 branchOffset)
{TRACE_IT(16956);
    Assert(branchInstr->IsBranchInstr());

    uint32 labelOffset = branchInstr->GetTarget()->GetOffset();
    Assert(labelOffset); //Label offset must be set.

    int32 offset = labelOffset - branchOffset;
    //We should never run out of 24 bits which corresponds to +-16MB of code size.
    AssertMsg(IS_CONST_INT24(offset >> 1), "Cannot encode more that 16 MB offset");

    if (LowererMD::IsUnconditionalBranch(branchInstr))
    {TRACE_IT(16957);
        return false;
    }

    if (IS_CONST_INT21(offset))
    {TRACE_IT(16958);
        return false;
    }

    // Convert a conditional branch which can only be +-1MB to unconditional branch which is +-16MB
    // Convert beq Label (where Label is long jump) to something like this
    //          bne Fallback
    //          b Label
    // Fallback:

    IR::LabelInstr *doneLabelInstr = IR::LabelInstr::New(Js::OpCode::Label, branchInstr->m_func, false);
    IR::BranchInstr *newBranchInstr = IR::BranchInstr::New(branchInstr->m_opcode, doneLabelInstr, branchInstr->m_func);
    LowererMD::InvertBranch(newBranchInstr);

    branchInstr->InsertBefore(newBranchInstr);
    branchInstr->InsertAfter(doneLabelInstr);
    branchInstr->m_opcode = Js::OpCode::B;
    return true;
}

void LegalizeMD::LegalizeIndirOpndForVFP(IR::Instr* insertInstr, IR::IndirOpnd *indirOpnd, bool fPostRegAlloc)
{TRACE_IT(16959);
    IR::RegOpnd *baseOpnd = indirOpnd->GetBaseOpnd();
    int32 offset = indirOpnd->GetOffset();

    IR::RegOpnd *indexOpnd = indirOpnd->UnlinkIndexOpnd(); //Clears index operand
    byte scale = indirOpnd->GetScale();
    IR::Instr *instr = NULL;

    if (indexOpnd)
    {TRACE_IT(16960);
        if (scale > 0)
        {TRACE_IT(16961);
            // There is no support for ADD instruction with barrel shifter in encoder, hence add an explicit instruction to left shift the index operand
            // Reason is this requires 4 operand in IR and there is no support for this yet.
            // If we encounter more such scenarios, its better to solve the root cause.
            // Also VSTR & VLDR don't take index operand as parameter
            IR::RegOpnd* newIndexOpnd = IR::RegOpnd::New(indexOpnd->GetType(), insertInstr->m_func);
            instr = IR::Instr::New(Js::OpCode::LSL, newIndexOpnd, indexOpnd,
                                   IR::IntConstOpnd::New(scale, TyMachReg, insertInstr->m_func), insertInstr->m_func);
            insertInstr->InsertBefore(instr);
            indirOpnd->SetScale(0); //Clears scale
            indexOpnd = newIndexOpnd;
        }

        insertInstr->HoistIndirIndexOpndAsAdd(indirOpnd, baseOpnd, indexOpnd, fPostRegAlloc? SCRATCH_REG : RegNOREG);
    }

    if (IS_CONST_UINT10((offset < 0? -offset: offset)))
    {TRACE_IT(16962);
        return;
    }
    IR::Instr* instrAdd = insertInstr->HoistIndirOffsetAsAdd(indirOpnd, indirOpnd->GetBaseOpnd(), offset, fPostRegAlloc ? SCRATCH_REG : RegNOREG);
    LegalizeMD::LegalizeInstr(instrAdd, fPostRegAlloc);
}


#ifdef DBG

void LegalizeMD::IllegalInstr(IR::Instr * instr, const char16 * msg, ...)
{
    va_list argptr;
    va_start(argptr, msg);
    Output::Print(_u("Illegal instruction: "));
    instr->Dump();
    Output::Print(msg, argptr);
    Assert(UNREACHED);
}

#endif
