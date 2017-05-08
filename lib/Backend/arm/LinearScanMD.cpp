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
{TRACE_IT(16963);
}

void
LinearScanMD::Init(LinearScan *linearScan)
{TRACE_IT(16964);
    LinearScanMDShared::Init(linearScan);
    Func *func = linearScan->func;
    RegNum localsReg = func->GetLocalsPointer();
    if (localsReg != RegSP)
    {TRACE_IT(16965);
        func->m_regsUsed.Set(localsReg);
    }

    memset(this->vfpSymTable, 0, sizeof(this->vfpSymTable));
}

StackSym *
LinearScanMD::EnsureSpillSymForVFPReg(RegNum reg, Func *func)
{TRACE_IT(16966);
    Assert(REGNUM_ISVFPREG(reg));

    __analysis_assume(reg - RegD0 < VFP_REGCOUNT);
    StackSym *sym = this->vfpSymTable[reg - RegD0];

    if (sym == nullptr)
    {TRACE_IT(16967);
        sym = StackSym::New(TyFloat64, func);
        func->StackAllocate(sym, MachRegDouble);

        __analysis_assume(reg - RegD0 < VFP_REGCOUNT);
        this->vfpSymTable[reg - RegD0] = sym;
    }

    return sym;
}


bool
LinearScanMD::IsAllocatable(RegNum reg, Func *func) const
{TRACE_IT(16968);
    return reg != func->GetLocalsPointer();
}

BitVector
LinearScanMD::FilterRegIntSizeConstraints(BitVector regsBv, BitVector sizeUsageBv) const
{TRACE_IT(16969);
    return regsBv;
}

bool
LinearScanMD::FitRegIntSizeConstraints(RegNum reg, BitVector sizeUsageBv) const
{TRACE_IT(16970);
    return true;
}

bool
LinearScanMD::FitRegIntSizeConstraints(RegNum reg, IRType type) const
{TRACE_IT(16971);
    return true;
}

void
LinearScanMD::InsertOpHelperSpillAndRestores(SList<OpHelperBlock> *opHelperBlockList)
{TRACE_IT(16972);
    if (maxOpHelperSpilledLiveranges)
    {TRACE_IT(16973);
        Assert(!helperSpillSlots);
        helperSpillSlots = AnewArrayZ(linearScan->GetTempAlloc(), StackSym *, maxOpHelperSpilledLiveranges);
    }

    FOREACH_SLIST_ENTRY(OpHelperBlock, opHelperBlock, opHelperBlockList)
    {TRACE_IT(16974);
        InsertOpHelperSpillsAndRestores(opHelperBlock);
    }
    NEXT_SLIST_ENTRY;
}

void
LinearScanMD::InsertOpHelperSpillsAndRestores(const OpHelperBlock& opHelperBlock)
{TRACE_IT(16975);
    uint32 index = 0;

    FOREACH_SLIST_ENTRY(OpHelperSpilledLifetime, opHelperSpilledLifetime, &opHelperBlock.spilledLifetime)
    {TRACE_IT(16976);
        // Use the original sym as spill slot if this is an inlinee arg
        StackSym* sym = nullptr;
        if (opHelperSpilledLifetime.spillAsArg)
        {TRACE_IT(16977);
            sym = opHelperSpilledLifetime.lifetime->sym;
            AnalysisAssert(sym);
            Assert(sym->IsAllocated());
        }

        if (RegTypes[opHelperSpilledLifetime.reg] == TyFloat64)
        {TRACE_IT(16978);
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(nullptr, opHelperSpilledLifetime.reg, TyMachDouble, this->func);

            if (!sym)
            {TRACE_IT(16979);
                sym = EnsureSpillSymForVFPReg(regOpnd->GetReg(), this->func);
            }
            IR::Instr * pushInstr = IR::Instr::New(Js::OpCode::VSTR, IR::SymOpnd::New(sym, TyMachDouble, this->func), regOpnd, this->func);
            opHelperBlock.opHelperLabel->InsertAfter(pushInstr);
            pushInstr->CopyNumber(opHelperBlock.opHelperLabel);
            if (opHelperSpilledLifetime.reload)
            {TRACE_IT(16980);
                IR::Instr * popInstr = IR::Instr::New(Js::OpCode::VLDR, regOpnd, IR::SymOpnd::New(sym, TyMachDouble, this->func), this->func);
                opHelperBlock.opHelperEndInstr->InsertBefore(popInstr);
                popInstr->CopyNumber(opHelperBlock.opHelperEndInstr);
            }
        }
        else
        {TRACE_IT(16981);
            Assert(helperSpillSlots);
            Assert(index < maxOpHelperSpilledLiveranges);

            if (!sym)
            {TRACE_IT(16982);
                // Lazily allocate only as many slots as we really need.
                if (!helperSpillSlots[index])
                {TRACE_IT(16983);
                    helperSpillSlots[index] = StackSym::New(TyMachReg, func);
                }

                sym = helperSpillSlots[index];
                index++;

                Assert(sym);
                func->StackAllocate(sym, MachRegInt);
            }

            IR::RegOpnd * regOpnd = IR::RegOpnd::New(sym, opHelperSpilledLifetime.reg, sym->GetType(), func);
            IR::Instr * saveInstr = IR::Instr::New(Js::OpCode::STR, IR::SymOpnd::New(sym, sym->GetType(), func), regOpnd, func);
            opHelperBlock.opHelperLabel->InsertAfter(saveInstr);
            saveInstr->CopyNumber(opHelperBlock.opHelperLabel);
            this->LegalizeDef(saveInstr);

            if (opHelperSpilledLifetime.reload)
            {TRACE_IT(16984);
                IR::Instr * restoreInstr = IR::Instr::New(Js::OpCode::LDR, regOpnd, IR::SymOpnd::New(sym, sym->GetType(), func), func);
                opHelperBlock.opHelperEndInstr->InsertBefore(restoreInstr);
                restoreInstr->CopyNumber(opHelperBlock.opHelperEndInstr);
                this->LegalizeUse(restoreInstr, restoreInstr->GetSrc1());
            }
        }
    }
    NEXT_SLIST_ENTRY;
}

void
LinearScanMD::EndOfHelperBlock(uint32 helperSpilledLiveranges)
{TRACE_IT(16985);
    if (helperSpilledLiveranges > maxOpHelperSpilledLiveranges)
    {TRACE_IT(16986);
        maxOpHelperSpilledLiveranges = helperSpilledLiveranges;
    }
}

void
LinearScanMD::LegalizeDef(IR::Instr * instr)
{TRACE_IT(16987);
    if (instr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn)
    {TRACE_IT(16988);
        // ArgOut_A_InlineBuiltIn pseudo instruction is kept through register allocator only to use for bailout as is,
        // and thus it must not be changed here by legalization.
        // It is removed in peeps, so only place to special case it is in register allocator.
        return;
    }

    // Legalize opcodes, etc., but do not expand symbol/indirs with large offsets
    // because we can't safely do this until all loads and stores are in place.
    LegalizeMD::LegalizeDst(instr, false);
}

void
LinearScanMD::LegalizeUse(IR::Instr * instr, IR::Opnd * opnd)
{TRACE_IT(16989);
    if (instr->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn)
    {TRACE_IT(16990);
        // ArgOut_A_InlineBuiltIn pseudo instruction is kept through register allocator only to use for bailout as is,
        // and thus it must not be changed here by legalization.
        // It is removed in peeps, so only place to special case it is in register allocator.
        return;
    }

    // Legalize opcodes, etc., but do not expand symbol/indirs with large offsets
    // because we can't safely do this until all loads and stores are in place.
    if (opnd == instr->GetSrc1())
    {TRACE_IT(16991);
        LegalizeMD::LegalizeSrc(instr, opnd, 1, false);
    }
    else
    {TRACE_IT(16992);
        LegalizeMD::LegalizeSrc(instr, opnd, 2, false);
    }
}

void
LinearScanMD::GenerateBailOut(
    IR::Instr * instr,
    __in_ecount(registerSaveSymsCount) StackSym ** registerSaveSyms,
    uint registerSaveSymsCount)
{TRACE_IT(16993);
    Func *const func = instr->m_func;
    BailOutInfo *const bailOutInfo = instr->GetBailOutInfo();
    IR::Instr *firstInstr = instr->m_prev;
    Js::Var *const registerSaveSpace = (Js::Var*)func->GetThreadContextInfo()->GetBailOutRegisterSaveSpaceAddr();

    const auto LoadRegSaveSpaceIntoScratch = [&](const RegNum reg)
    {TRACE_IT(16994);
        // Load the register save space address for the specified register into the scratch register:
        //     ldimm SCRATCH_REG, regSaveSpace
        LinearScan::InsertMove(
            IR::RegOpnd::New(nullptr, SCRATCH_REG, TyMachPtr, func),
            IR::AddrOpnd::New(&registerSaveSpace[reg - 1], IR::AddrOpndKindDynamicMisc, func),
            instr);
    };

    const auto SaveReg = [&](const RegNum reg)
    {TRACE_IT(16995);
        Assert(registerSaveSyms[reg - 1]);

        //     LoadRegSaveSpaceIntoScratch(reg)
        //     mov  [SCRATCH_REG], reg
        LoadRegSaveSpaceIntoScratch(reg);
        const IRType regType = RegTypes[reg];
        LinearScan::InsertMove(
            IR::IndirOpnd::New(
                IR::RegOpnd::New(nullptr, SCRATCH_REG, TyMachPtr, func),
                0,
                regType,
                func),
            IR::RegOpnd::New(registerSaveSyms[reg - 1], reg, regType, func),
            instr);
    };

    // Save registers used for parameters, and lr, if necessary, into the register save space
    if(bailOutInfo->branchConditionOpnd && registerSaveSyms[RegR1 - 1] && registerSaveSyms[RegR0 - 1])
    {TRACE_IT(16996);
        // Save r1 and r0 with one push:
        //     LoadRegSaveSpaceIntoScratch(RegR2)
        //     push [SCRATCH_REG], {r0 - r1}
        LoadRegSaveSpaceIntoScratch(RegR2);
        IR::Instr *instrPush = IR::Instr::New(
            Js::OpCode::PUSH,
            IR::IndirOpnd::New(
            IR::RegOpnd::New(nullptr, SCRATCH_REG, TyMachPtr, func),
            0,
            TyMachReg,
            func),
            IR::RegBVOpnd::New(BVUnit32((1 << RegR1) - 1), TyMachReg, func),
            func);

        instr->InsertBefore(instrPush);
        instrPush->CopyNumber(instr);
    }
    else if(bailOutInfo->branchConditionOpnd && registerSaveSyms[RegR1 - 1])
    {TRACE_IT(16997);
        SaveReg(RegR1);
    }
    else if(registerSaveSyms[RegR0 - 1])
    {TRACE_IT(16998);
        SaveReg(RegR0);
    }
    if(registerSaveSyms[RegLR - 1])
    {TRACE_IT(16999);
        SaveReg(RegLR);
    }

    if(bailOutInfo->branchConditionOpnd)
    {TRACE_IT(17000);
        // Pass in the branch condition
        //     mov  r1, condition
        IR::Instr *const newInstr =
            LinearScan::InsertMove(
                IR::RegOpnd::New(nullptr, RegR1, bailOutInfo->branchConditionOpnd->GetType(), func),
                bailOutInfo->branchConditionOpnd,
                instr);
        linearScan->SetSrcRegs(newInstr);
    }

    if (func->IsOOPJIT())
    {TRACE_IT(17001);
        // ldimm r0, dataAddr
        intptr_t nativeDataAddr = func->GetWorkItem()->GetWorkItemData()->nativeDataAddr;
        IR::RegOpnd * r0 = IR::RegOpnd::New(nullptr, RegR0, TyMachPtr, func);
        LinearScan::InsertMove(r0, IR::AddrOpnd::New(nativeDataAddr, IR::AddrOpndKindDynamicNativeCodeDataRef, func), instr);

        // mov r0, [r0]
        LinearScan::InsertMove(r0, IR::IndirOpnd::New(r0, 0, TyMachPtr, func), instr);

        // lea r0, [r0 + bailoutRecord_offset]
        unsigned int bailoutRecordOffset = NativeCodeData::GetDataTotalOffset(bailOutInfo->bailOutRecord);
        LinearScan::InsertLea(
            r0,
            IR::IndirOpnd::New(r0, bailoutRecordOffset, TyUint32,
#if DBG
                NativeCodeData::GetDataDescription(bailOutInfo->bailOutRecord, func->m_alloc),
#endif
                this->func), instr);
    }
    else
    {TRACE_IT(17002);
        // Pass in the bailout record
        //     ldimm r0, bailOutRecord
        LinearScan::InsertMove(
            IR::RegOpnd::New(nullptr, RegR0, TyMachPtr, func),
            IR::AddrOpnd::New(bailOutInfo->bailOutRecord, IR::AddrOpndKindDynamicBailOutRecord, func, true),
            instr);
    }

    firstInstr = firstInstr->m_next;
    for(uint i = 0; i < registerSaveSymsCount; i++)
    {TRACE_IT(17003);
        StackSym *const stackSym = registerSaveSyms[i];
        if(!stackSym)
        {TRACE_IT(17004);
            continue;
        }

        // Record the use on the lifetime in case it spilled afterwards. Spill loads will be inserted before 'firstInstr', that
        // is, before the register saves are done.
        this->linearScan->RecordUse(stackSym->scratch.linearScan.lifetime, firstInstr, nullptr, true);
    }

    // Load the bailout target into lr
    //     ldimm lr, BailOut
    //     blx  lr
    Assert(instr->GetSrc1()->IsHelperCallOpnd());
    LinearScan::InsertMove(IR::RegOpnd::New(nullptr, RegLR, TyMachPtr, func), instr->GetSrc1(), instr);
    instr->ReplaceSrc1(IR::RegOpnd::New(nullptr, RegLR, TyMachPtr, func));
}

IR::Instr *
LinearScanMD::GenerateBailInForGeneratorYield(IR::Instr * resumeLabelInstr, BailOutInfo * bailOutInfo)
{TRACE_IT(17005);
    Js::Throw::NotImplemented();
}

uint LinearScanMD::GetRegisterSaveIndex(RegNum reg)
{TRACE_IT(17006);
    if (RegTypes[reg] == TyFloat64)
    {TRACE_IT(17007);
        Assert(reg+1 >= RegD0);
        return (reg - RegD0) * 2 + RegD0;
    }
    else
    {TRACE_IT(17008);
        return reg;
    }
}

// static
RegNum LinearScanMD::GetRegisterFromSaveIndex(uint offset)
{TRACE_IT(17009);
    return (RegNum)(offset >= RegD0 ? (offset - RegD0) / 2  + RegD0 : offset);
}

RegNum LinearScanMD::GetParamReg(IR::SymOpnd *symOpnd, Func *func)
{TRACE_IT(17010);
    /* TODO - Add ARM32 support according to register calling convention */
    return RegNOREG;
}
