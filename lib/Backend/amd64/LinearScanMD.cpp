//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"
#include "SccLiveness.h"

extern const IRType RegTypes[RegNumCount];

LinearScanMD::LinearScanMD(Func *func)
    : helperSpillSlots(nullptr),
      maxOpHelperSpilledLiveranges(0),
      func(func)
{LOGMEIN("LinearScanMD.cpp] 13\n");
    this->byteableRegsBv.ClearAll();

    FOREACH_REG(reg)
    {LOGMEIN("LinearScanMD.cpp] 17\n");
        if (LinearScan::GetRegAttribs(reg) & RA_BYTEABLE)
        {LOGMEIN("LinearScanMD.cpp] 19\n");
            this->byteableRegsBv.Set(reg);
        }
    } NEXT_REG;

    memset(this->xmmSymTable128, 0, sizeof(this->xmmSymTable128));
    memset(this->xmmSymTable64, 0, sizeof(this->xmmSymTable64));
    memset(this->xmmSymTable32, 0, sizeof(this->xmmSymTable32));
}

BitVector
LinearScanMD::FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const
{LOGMEIN("LinearScanMD.cpp] 31\n");
    // Requires byte-able reg?
    if (sizeUsageBv.Test(1))
    {LOGMEIN("LinearScanMD.cpp] 34\n");
        regsBv.And(this->byteableRegsBv);
    }

    return regsBv;
}

bool
LinearScanMD::FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const
{LOGMEIN("LinearScanMD.cpp] 43\n");
    // Requires byte-able reg?
    return !sizeUsageBv.Test(1) || this->byteableRegsBv.Test(reg);
}

bool
LinearScanMD::FitRegIntSizeConstraints(RegNum reg, IRType type) const
{LOGMEIN("LinearScanMD.cpp] 50\n");
    // Requires byte-able reg?
    return TySize[type] != 1 || this->byteableRegsBv.Test(reg);
}

StackSym *
LinearScanMD::EnsureSpillSymForXmmReg(RegNum reg, Func *func, IRType type)
{LOGMEIN("LinearScanMD.cpp] 57\n");
    Assert(REGNUM_ISXMMXREG(reg));

    __analysis_assume(reg - FIRST_XMM_REG < XMM_REGCOUNT);
    StackSym *sym;
    if (type == TyFloat32)
    {LOGMEIN("LinearScanMD.cpp] 63\n");
        sym = this->xmmSymTable32[reg - FIRST_XMM_REG];
    }
    else if (type == TyFloat64)
    {LOGMEIN("LinearScanMD.cpp] 67\n");
        sym = this->xmmSymTable64[reg - FIRST_XMM_REG];
    }
    else
    {
        Assert(IRType_IsSimd128(type));
        sym = this->xmmSymTable128[reg - FIRST_XMM_REG];
    }

    if (sym == nullptr)
    {LOGMEIN("LinearScanMD.cpp] 77\n");
        sym = StackSym::New(type, func);
        func->StackAllocate(sym, TySize[type]);

        __analysis_assume(reg - FIRST_XMM_REG < XMM_REGCOUNT);

        if (type == TyFloat32)
        {LOGMEIN("LinearScanMD.cpp] 84\n");
            this->xmmSymTable32[reg - FIRST_XMM_REG] = sym;
        }
        else if (type == TyFloat64)
        {LOGMEIN("LinearScanMD.cpp] 88\n");
            this->xmmSymTable64[reg - FIRST_XMM_REG] = sym;
        }
        else
        {
            Assert(IRType_IsSimd128(type));
            this->xmmSymTable128[reg - FIRST_XMM_REG] = sym;
        }
    }

    return sym;
}

void
LinearScanMD::LegalizeConstantUse(IR::Instr * instr, IR::Opnd * opnd)
{LOGMEIN("LinearScanMD.cpp] 103\n");
    Assert(opnd->IsAddrOpnd() || opnd->IsIntConstOpnd());
    intptr_t value = opnd->IsAddrOpnd() ? (intptr_t)opnd->AsAddrOpnd()->m_address : opnd->AsIntConstOpnd()->GetValue();
    if (value == 0
        && instr->m_opcode == Js::OpCode::MOV
        && !instr->GetDst()->IsRegOpnd()
        && TySize[opnd->GetType()] >= 4)
    {LOGMEIN("LinearScanMD.cpp] 110\n");
        Assert(this->linearScan->instrUseRegs.IsEmpty());

        // MOV doesn't have an imm8 encoding for 32-bit/64-bit assignment, so if we have a register available,
        // we should hoist it and generate xor reg, reg and MOV dst, reg
        BitVector regsBv;
        regsBv.Copy(this->linearScan->activeRegs);
        regsBv.Or(this->linearScan->callSetupRegs);

        regsBv.ComplimentAll();
        regsBv.And(this->linearScan->int32Regs);
        regsBv.Minus(this->linearScan->tempRegs);       // Avoid tempRegs
        BVIndex regIndex = regsBv.GetNextBit();
        if (regIndex != BVInvalidIndex)
        {LOGMEIN("LinearScanMD.cpp] 124\n");
            instr->HoistSrc1(Js::OpCode::MOV, (RegNum)regIndex);
            this->linearScan->instrUseRegs.Set(regIndex);
            this->func->m_regsUsed.Set(regIndex);

            // If we are in a loop, we need to mark the register being used by the loop so that
            // reload to that register will not be hoisted out of the loop
            this->linearScan->RecordLoopUse(nullptr, (RegNum)regIndex);
        }
    }
}

void
LinearScanMD::InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList)
{LOGMEIN("LinearScanMD.cpp] 138\n");
    if (maxOpHelperSpilledLiveranges)
    {LOGMEIN("LinearScanMD.cpp] 140\n");
        Assert(!helperSpillSlots);
        helperSpillSlots = AnewArrayZ(linearScan->GetTempAlloc(), StackSym *, maxOpHelperSpilledLiveranges);
    }

    FOREACH_SLIST_ENTRY(OpHelperBlock, opHelperBlock, opHelperBlockList)
    {LOGMEIN("LinearScanMD.cpp] 146\n");
        InsertOpHelperSpillsAndRestores(opHelperBlock);
    }
    NEXT_SLIST_ENTRY;
}

void
LinearScanMD::InsertOpHelperSpillsAndRestores(const OpHelperBlock& opHelperBlock)
{LOGMEIN("LinearScanMD.cpp] 154\n");
    uint32 index = 0;

    FOREACH_SLIST_ENTRY(OpHelperSpilledLifetime, opHelperSpilledLifetime, &opHelperBlock.spilledLifetime)
    {LOGMEIN("LinearScanMD.cpp] 158\n");
        // Use the original sym as spill slot if this is an inlinee arg
        StackSym* sym = nullptr;
        if (opHelperSpilledLifetime.spillAsArg)
        {LOGMEIN("LinearScanMD.cpp] 162\n");
            sym = opHelperSpilledLifetime.lifetime->sym;
            AnalysisAssert(sym);
            Assert(sym->IsAllocated());
        }

        if (RegTypes[opHelperSpilledLifetime.reg] == TyFloat64)
        {LOGMEIN("LinearScanMD.cpp] 169\n");
            IRType type = opHelperSpilledLifetime.lifetime->sym->GetType();
            IR::RegOpnd *regOpnd = IR::RegOpnd::New(nullptr, opHelperSpilledLifetime.reg, type, this->func);

            if (!sym)
            {LOGMEIN("LinearScanMD.cpp] 174\n");
                sym = EnsureSpillSymForXmmReg(regOpnd->GetReg(), this->func, type);
            }

            IR::Instr   *pushInstr = IR::Instr::New(LowererMDArch::GetAssignOp(type), IR::SymOpnd::New(sym, type, this->func), regOpnd, this->func);
            opHelperBlock.opHelperLabel->InsertAfter(pushInstr);
            pushInstr->CopyNumber(opHelperBlock.opHelperLabel);
            if (opHelperSpilledLifetime.reload)
            {LOGMEIN("LinearScanMD.cpp] 182\n");
                IR::Instr   *popInstr = IR::Instr::New(LowererMDArch::GetAssignOp(type), regOpnd, IR::SymOpnd::New(sym, type, this->func), this->func);
                opHelperBlock.opHelperEndInstr->InsertBefore(popInstr);
                popInstr->CopyNumber(opHelperBlock.opHelperEndInstr);
            }
        }
        else
        {
            Assert(helperSpillSlots);
            Assert(index < maxOpHelperSpilledLiveranges);

            if (!sym)
            {LOGMEIN("LinearScanMD.cpp] 194\n");
                // Lazily allocate only as many slots as we really need.
                if (!helperSpillSlots[index])
                {LOGMEIN("LinearScanMD.cpp] 197\n");
                    helperSpillSlots[index] = StackSym::New(TyMachReg, func);
                }

                sym = helperSpillSlots[index];
                index++;

                Assert(sym);
                func->StackAllocate(sym, MachRegInt);
            }
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(nullptr, opHelperSpilledLifetime.reg, sym->GetType(), func);
            LowererMD::CreateAssign(IR::SymOpnd::New(sym, sym->GetType(), func), regOpnd, opHelperBlock.opHelperLabel->m_next);
            if (opHelperSpilledLifetime.reload)
            {LOGMEIN("LinearScanMD.cpp] 210\n");
                LowererMD::CreateAssign(regOpnd, IR::SymOpnd::New(sym, sym->GetType(), func), opHelperBlock.opHelperEndInstr);
            }
        }
    }
    NEXT_SLIST_ENTRY;
}

void
LinearScanMD::EndOfHelperBlock(uint32 helperSpilledLiveranges)
{LOGMEIN("LinearScanMD.cpp] 220\n");
    if (helperSpilledLiveranges > maxOpHelperSpilledLiveranges)
    {LOGMEIN("LinearScanMD.cpp] 222\n");
        maxOpHelperSpilledLiveranges = helperSpilledLiveranges;
    }
}

void
LinearScanMD::GenerateBailOut(IR::Instr * instr, __in_ecount(registerSaveSymsCount) StackSym ** registerSaveSyms, uint registerSaveSymsCount)
{LOGMEIN("LinearScanMD.cpp] 229\n");
    Func *const func = instr->m_func;
    BailOutInfo *const bailOutInfo = instr->GetBailOutInfo();
    IR::Instr *firstInstr = instr->m_prev;

    // Code analysis doesn't do inter-procesure analysis and cannot infer the value of registerSaveSymsCount,
    // but the passed in registerSaveSymsCount is static value RegNumCount-1, so reg-1 in below loop is always a valid index.
    __analysis_assume(static_cast<int>(registerSaveSymsCount) == static_cast<int>(RegNumCount-1));
    Assert(static_cast<int>(registerSaveSymsCount) == static_cast<int>(RegNumCount-1));

    // Save registers used for parameters, and rax, if necessary, into the shadow space allocated for register parameters:
    //     mov  [rsp + 16], RegArg1     (if branchConditionOpnd)
    //     mov  [rsp + 8], RegArg0
    //     mov  [rsp], rax
    const RegNum regs[3] = { RegRAX, RegArg0, RegArg1 };
    for (int i = (bailOutInfo->branchConditionOpnd ? 2 : 1); i >= 0; i--)
    {LOGMEIN("LinearScanMD.cpp] 245\n");
        RegNum reg = regs[i];
        StackSym *const stackSym = registerSaveSyms[reg - 1];
        if(!stackSym)
        {LOGMEIN("LinearScanMD.cpp] 249\n");
            continue;
        }

        const IRType regType = RegTypes[reg];
        Lowerer::InsertMove(
            IR::SymOpnd::New(func->m_symTable->GetArgSlotSym(static_cast<Js::ArgSlot>(i + 1)), regType, func),
            IR::RegOpnd::New(stackSym, reg, regType, func),
            instr);
    }

    if(bailOutInfo->branchConditionOpnd)
    {LOGMEIN("LinearScanMD.cpp] 261\n");
        // Pass in the branch condition
        //     mov  RegArg1, condition
        IR::Instr *const newInstr =
            Lowerer::InsertMove(
                IR::RegOpnd::New(nullptr, RegArg1, bailOutInfo->branchConditionOpnd->GetType(), func),
                bailOutInfo->branchConditionOpnd,
                instr);
        linearScan->SetSrcRegs(newInstr);
    }

    if (!func->IsOOPJIT())
    {LOGMEIN("LinearScanMD.cpp] 273\n");
        // Pass in the bailout record
        //     mov  RegArg0, bailOutRecord
        Lowerer::InsertMove(
            IR::RegOpnd::New(nullptr, RegArg0, TyMachPtr, func),
            IR::AddrOpnd::New(bailOutInfo->bailOutRecord, IR::AddrOpndKindDynamicBailOutRecord, func, true),
            instr);
    }
    else
    {
        // move RegArg0, dataAddr
        Lowerer::InsertMove(
            IR::RegOpnd::New(nullptr, RegArg0, TyMachPtr, func),
            IR::AddrOpnd::New(func->GetWorkItem()->GetWorkItemData()->nativeDataAddr, IR::AddrOpndKindDynamicNativeCodeDataRef, func),
            instr);

        // mov RegArg0, [RegArg0]
        Lowerer::InsertMove(
            IR::RegOpnd::New(nullptr, RegArg0, TyMachPtr, func),
            IR::IndirOpnd::New(IR::RegOpnd::New(nullptr, RegArg0, TyVar, this->func), 0, TyMachPtr, func),
            instr);

        // lea RegArg0, [RegArg0 + bailoutRecord_offset]
        int bailoutRecordOffset = NativeCodeData::GetDataTotalOffset(bailOutInfo->bailOutRecord);
        Lowerer::InsertLea(IR::RegOpnd::New(nullptr, RegArg0, TyVar, this->func),
            IR::IndirOpnd::New(IR::RegOpnd::New(nullptr, RegArg0, TyVar, this->func), bailoutRecordOffset, TyMachPtr,
#if DBG
            NativeCodeData::GetDataDescription(bailOutInfo->bailOutRecord, func->m_alloc),
#endif
            this->func), instr);

    }

    firstInstr = firstInstr->m_next;
    for(uint i = 0; i < registerSaveSymsCount; i++)
    {LOGMEIN("LinearScanMD.cpp] 308\n");
        StackSym *const stackSym = registerSaveSyms[i];
        if(!stackSym)
        {LOGMEIN("LinearScanMD.cpp] 311\n");
            continue;
        }

        // Record the use on the lifetime in case it spilled afterwards. Spill loads will be inserted before 'firstInstr', that
        // is, before the register saves are done.
        this->linearScan->RecordUse(stackSym->scratch.linearScan.lifetime, firstInstr, nullptr, true);
    }

    // Load the bailout target into rax
    //     mov  rax, BailOut
    //     call rax
    Assert(instr->GetSrc1()->IsHelperCallOpnd());
    Lowerer::InsertMove(IR::RegOpnd::New(nullptr, RegRAX, TyMachPtr, func), instr->GetSrc1(), instr);
    instr->ReplaceSrc1(IR::RegOpnd::New(nullptr, RegRAX, TyMachPtr, func));
}

// Gets the InterpreterStackFrame pointer into RAX.
// Restores the live stack locations followed by the live registers from
// the interpreter's register slots.
// RecordDefs each live register that is restored.
//
// Generates the following code:
//
// MOV rax, param0
// MOV rax, [rax + JavascriptGenerator::GetFrameOffset()]
//
// for each live stack location, sym
//
//   MOV rcx, [rax + regslot offset]
//   MOV sym(stack location), rcx
//
// for each live register, sym (rax is restore last if it is live)
//
//   MOV sym(register), [rax + regslot offset]
//
IR::Instr *
LinearScanMD::GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo)
{LOGMEIN("LinearScanMD.cpp] 349\n");
    IR::Instr * instrAfter = resumeLabelInstr->m_next;

    IR::RegOpnd * raxRegOpnd = IR::RegOpnd::New(nullptr, RegRAX, TyMachPtr, this->func);
    IR::RegOpnd * rcxRegOpnd = IR::RegOpnd::New(nullptr, RegRCX, TyVar, this->func);

    StackSym * sym = StackSym::NewParamSlotSym(1, this->func);
    this->func->SetArgOffset(sym, LowererMD::GetFormalParamOffset() * MachPtr);
    IR::SymOpnd * symOpnd = IR::SymOpnd::New(sym, TyMachPtr, this->func);
    LinearScan::InsertMove(raxRegOpnd, symOpnd, instrAfter);

    IR::IndirOpnd * indirOpnd = IR::IndirOpnd::New(raxRegOpnd, Js::JavascriptGenerator::GetFrameOffset(), TyMachPtr, this->func);
    LinearScan::InsertMove(raxRegOpnd, indirOpnd, instrAfter);


    // rax points to the frame, restore stack syms and registers except rax, restore rax last

    IR::Instr * raxRestoreInstr = nullptr;
    IR::Instr * instrInsertStackSym = instrAfter;
    IR::Instr * instrInsertRegSym = instrAfter;

    Assert(bailOutInfo->capturedValues.constantValues.Empty());
    Assert(bailOutInfo->capturedValues.copyPropSyms.Empty());
    Assert(bailOutInfo->liveLosslessInt32Syms->IsEmpty());
    Assert(bailOutInfo->liveFloat64Syms->IsEmpty());

    auto restoreSymFn = [this, &raxRegOpnd, &rcxRegOpnd, &raxRestoreInstr, &instrInsertStackSym, &instrInsertRegSym](Js::RegSlot regSlot, StackSym* stackSym)
    {LOGMEIN("LinearScanMD.cpp] 376\n");
        Assert(stackSym->IsVar());

        int32 offset = regSlot * sizeof(Js::Var) + Js::InterpreterStackFrame::GetOffsetOfLocals();

        IR::Opnd * srcOpnd = IR::IndirOpnd::New(raxRegOpnd, offset, stackSym->GetType(), this->func);
        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;

        if (lifetime->isSpilled)
        {LOGMEIN("LinearScanMD.cpp] 385\n");
            // stack restores require an extra register since we can't move an indir directly to an indir on amd64
            IR::SymOpnd * dstOpnd = IR::SymOpnd::New(stackSym, stackSym->GetType(), this->func);
            LinearScan::InsertMove(rcxRegOpnd, srcOpnd, instrInsertStackSym);
            LinearScan::InsertMove(dstOpnd, rcxRegOpnd, instrInsertStackSym);
        }
        else
        {
            // register restores must come after stack restores so that we have RAX and RCX free to
            // use for stack restores and further RAX must be restored last since it holds the
            // pointer to the InterpreterStackFrame from which we are restoring values.
            // We must also track these restores using RecordDef in case the symbols are spilled.

            IR::RegOpnd * dstRegOpnd = IR::RegOpnd::New(stackSym, stackSym->GetType(), this->func);
            dstRegOpnd->SetReg(lifetime->reg);

            IR::Instr * instr = LinearScan::InsertMove(dstRegOpnd, srcOpnd, instrInsertRegSym);

            if (instrInsertRegSym == instrInsertStackSym)
            {LOGMEIN("LinearScanMD.cpp] 404\n");
                // this is the first register sym, make sure we don't insert stack stores
                // after this instruction so we can ensure rax and rcx remain free to use
                // for restoring spilled stack syms.
                instrInsertStackSym = instr;
            }

            if (lifetime->reg == RegRAX)
            {LOGMEIN("LinearScanMD.cpp] 412\n");
                // ensure rax is restored last
                Assert(instrInsertRegSym != instrInsertStackSym);

                instrInsertRegSym = instr;

                if (raxRestoreInstr != nullptr)
                {
                    AssertMsg(false, "this is unexpected until copy prop is enabled");
                    // rax was mapped to multiple bytecode registers.  Obviously only the first
                    // restore we do will work so change all following stores to `mov rax, rax`.
                    // We still need to keep them around for RecordDef in case the corresponding
                    // dst sym is spilled later on.
                    raxRestoreInstr->FreeSrc1();
                    raxRestoreInstr->SetSrc1(raxRegOpnd);
                }

                raxRestoreInstr = instr;
            }

            this->linearScan->RecordDef(lifetime, instr, 0);
        }
    };

    FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->byteCodeUpwardExposedUsed)
    {LOGMEIN("LinearScanMD.cpp] 437\n");
        StackSym* stackSym = this->func->m_symTable->FindStackSym(symId);
        restoreSymFn(stackSym->GetByteCodeRegSlot(), stackSym);
    }
    NEXT_BITSET_IN_SPARSEBV;

    if (bailOutInfo->capturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->capturedValues.argObjSyms)
        {LOGMEIN("LinearScanMD.cpp] 446\n");
            StackSym* stackSym = this->func->m_symTable->FindStackSym(symId);
            restoreSymFn(stackSym->GetByteCodeRegSlot(), stackSym);
        }
        NEXT_BITSET_IN_SPARSEBV;
    }

    Js::RegSlot localsCount = this->func->GetJITFunctionBody()->GetLocalsCount();
    bailOutInfo->IterateArgOutSyms([localsCount, &restoreSymFn](uint, uint argOutSlotOffset, StackSym* sym) {
        restoreSymFn(localsCount + argOutSlotOffset, sym);
    });

    return instrAfter;
}

uint LinearScanMD::GetRegisterSaveIndex(RegNum reg)
{LOGMEIN("LinearScanMD.cpp] 462\n");
    if (RegTypes[reg] == TyFloat64)
    {LOGMEIN("LinearScanMD.cpp] 464\n");
        // make room for maximum XMM reg size
        Assert(reg >= RegXMM0);
        return (reg - RegXMM0) * (sizeof(SIMDValue) / sizeof(Js::Var)) + RegXMM0;
    }
    else
    {
        return reg;
    }
}

RegNum LinearScanMD::GetRegisterFromSaveIndex(uint offset)
{LOGMEIN("LinearScanMD.cpp] 476\n");
    return (RegNum)(offset >= RegXMM0 ? (offset - RegXMM0) / (sizeof(SIMDValue) / sizeof(Js::Var)) + RegXMM0 : offset);
}

RegNum LinearScanMD::GetParamReg(IR::SymOpnd *symOpnd, Func *func)
{LOGMEIN("LinearScanMD.cpp] 481\n");
    RegNum reg = RegNOREG;
    StackSym *paramSym = symOpnd->m_sym->AsStackSym();

    if (func->GetJITFunctionBody()->IsAsmJsMode() && !func->IsLoopBody())
    {LOGMEIN("LinearScanMD.cpp] 486\n");
        // Asm.js function only have 1 implicit param as they have no CallInfo, and they have float/SIMD params.
        // Asm.js loop bodies however are called like normal JS functions.
        if (IRType_IsFloat(symOpnd->GetType()) || IRType_IsSimd(symOpnd->GetType()))
        {LOGMEIN("LinearScanMD.cpp] 490\n");
            switch (paramSym->GetParamSlotNum())
            {LOGMEIN("LinearScanMD.cpp] 492\n");
            case 1:
                reg = RegXMM1;
                break;
            case 2:
                reg = RegXMM2;
                break;
            case 3:
                reg = RegXMM3;
                break;
            }
        }
        else
        {
            if (paramSym->IsImplicitParamSym())
            {LOGMEIN("LinearScanMD.cpp] 507\n");
                switch (paramSym->GetParamSlotNum())
                {LOGMEIN("LinearScanMD.cpp] 509\n");
                case 1:
                    reg = RegArg0;
                    break;
                default:
                    Assert(UNREACHED);
                }
            }
            else
            {
                switch (paramSym->GetParamSlotNum())
                {LOGMEIN("LinearScanMD.cpp] 520\n");
                case 1:
                    reg = RegArg1;
                    break;
                case 2:
                    reg = RegArg2;
                    break;
                case 3:
                    reg = RegArg3;
                    break;
                }
            }
        }
    }
    else // Non-Asm.js
    {
        Assert(symOpnd->GetType() == TyVar || IRType_IsNativeInt(symOpnd->GetType()));

        if (paramSym->IsImplicitParamSym())
        {LOGMEIN("LinearScanMD.cpp] 539\n");
            switch (paramSym->GetParamSlotNum())
            {LOGMEIN("LinearScanMD.cpp] 541\n");
            case 1:
                reg = RegArg0;
                break;
            case 2:
                reg = RegArg1;
                break;
            }
        }
        else
        {
            switch (paramSym->GetParamSlotNum())
            {LOGMEIN("LinearScanMD.cpp] 553\n");
            case 1:
                reg = RegArg2;
                break;
            case 2:
                reg = RegArg3;
                break;
            }
        }
    }

    return reg;
}
