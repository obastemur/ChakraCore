//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#include "SccLiveness.h"

#if DBG_DUMP || ENABLE_DEBUG_CONFIG_OPTIONS
char const * const RegNames[RegNumCount] =
{
#define REGDAT(Name, ListName, ...) "" STRINGIZE(ListName) "",
#include "RegList.h"
#undef REGDAT
};

char16 const * const RegNamesW[RegNumCount] =
{
#define REGDAT(Name, ListName, ...) _u("") STRINGIZEW(ListName) _u(""),
#include "RegList.h"
#undef REGDAT
};
#endif

static const uint8 RegAttribs[RegNumCount] =
{
#define REGDAT(Name, ListName, Encode, Type, Attribs) Attribs,
#include "RegList.h"
#undef REGDAT
};

extern const IRType RegTypes[RegNumCount] =
{
#define REGDAT(Name, ListName, Encode, Type, Attribs) Type,
#include "RegList.h"
#undef REGDAT
};


LoweredBasicBlock* LoweredBasicBlock::New(JitArenaAllocator* allocator)
{LOGMEIN("LinearScan.cpp] 41\n");
    return JitAnew(allocator, LoweredBasicBlock, allocator);
}

void LoweredBasicBlock::Copy(LoweredBasicBlock* block)
{LOGMEIN("LinearScan.cpp] 46\n");
    this->inlineeFrameLifetimes.Copy(&block->inlineeFrameLifetimes);
    this->inlineeStack.Copy(&block->inlineeStack);
    this->inlineeFrameSyms.Copy(&block->inlineeFrameSyms);
}

bool LoweredBasicBlock::HasData()
{LOGMEIN("LinearScan.cpp] 53\n");
    return this->inlineeFrameLifetimes.Count() > 0 || this->inlineeStack.Count() > 0;
}

LoweredBasicBlock* LoweredBasicBlock::Clone(JitArenaAllocator* allocator)
{LOGMEIN("LinearScan.cpp] 58\n");
    if (this->HasData())
    {LOGMEIN("LinearScan.cpp] 60\n");
        LoweredBasicBlock* clone = LoweredBasicBlock::New(allocator);
        clone->Copy(this);
        return clone;
    }
    return nullptr;
}

bool LoweredBasicBlock::Equals(LoweredBasicBlock* otherBlock)
{LOGMEIN("LinearScan.cpp] 69\n");
    if(this->HasData() != otherBlock->HasData())
    {LOGMEIN("LinearScan.cpp] 71\n");
        return false;
    }
    if (!this->inlineeFrameLifetimes.Equals(&otherBlock->inlineeFrameLifetimes))
    {LOGMEIN("LinearScan.cpp] 75\n");
        return false;
    }
    if (!this->inlineeStack.Equals(&otherBlock->inlineeStack))
    {LOGMEIN("LinearScan.cpp] 79\n");
        return false;
    }
    return true;
}

// LinearScan::RegAlloc
// This register allocator is based on the 1999 linear scan register allocation paper
// by Poletto and Sarkar.  This code however walks the IR while doing the lifetime
// allocations, and assigns the regs to all the RegOpnd as it goes.  It assumes
// the IR is in R-DFO, and that the lifetime list is sorted in starting order.
// Lifetimes are allocated as they become live, and retired as they go dead.  RegOpnd
// are assigned their register.  If a lifetime becomes active and there are no free
// registers left, a lifetime is picked to be spilled.
// When we spill, the whole lifetime is spilled.  All the loads and stores are done
// through memory for that lifetime, even the ones allocated before the current instruction.
// We do optimize this slightly by not reloading the previous loads that were not in loops.

void
LinearScan::RegAlloc()
{LOGMEIN("LinearScan.cpp] 99\n");
    NoRecoverMemoryJitArenaAllocator tempAlloc(_u("BE-LinearScan"), this->func->m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);
    this->tempAlloc = &tempAlloc;
    this->opHelperSpilledLiveranges = JitAnew(&tempAlloc, SList<Lifetime *>, &tempAlloc);
    this->activeLiveranges = JitAnew(&tempAlloc, SList<Lifetime *>, &tempAlloc);
    this->liveOnBackEdgeSyms = JitAnew(&tempAlloc, BVSparse<JitArenaAllocator>, &tempAlloc);
    this->stackPackInUseLiveRanges = JitAnew(&tempAlloc, SList<Lifetime *>, &tempAlloc);
    this->stackSlotsFreeList = JitAnew(&tempAlloc, SList<StackSlot *>, &tempAlloc);
    this->currentBlock = LoweredBasicBlock::New(&tempAlloc);

    IR::Instr *currentInstr = this->func->m_headInstr;

    SCCLiveness liveness(this->func, this->tempAlloc);
    BEGIN_CODEGEN_PHASE(this->func, Js::LivenessPhase);

    // Build the lifetime list
    liveness.Build();
    END_CODEGEN_PHASE(this->func, Js::LivenessPhase);


    this->lifetimeList = &liveness.lifetimeList;

    this->opHelperBlockList = &liveness.opHelperBlockList;
    this->opHelperBlockIter = SList<OpHelperBlock>::Iterator(this->opHelperBlockList);
    this->opHelperBlockIter.Next();

    this->Init();

    NativeCodeData::Allocator * nativeAllocator = this->func->GetNativeCodeDataAllocator();
    if (func->hasBailout)
    {LOGMEIN("LinearScan.cpp] 129\n");
        this->globalBailOutRecordTables = NativeCodeDataNewArrayZ(nativeAllocator, GlobalBailOutRecordDataTable *,  func->m_inlineeId + 1);
        this->lastUpdatedRowIndices = JitAnewArrayZ(this->tempAlloc, uint *, func->m_inlineeId + 1);

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
        {LOGMEIN("LinearScan.cpp] 135\n");
            this->func->GetScriptContext()->bailOutOffsetBytes += (sizeof(GlobalBailOutRecordDataTable *) * (func->m_inlineeId + 1));
            this->func->GetScriptContext()->bailOutRecordBytes += (sizeof(GlobalBailOutRecordDataTable *) * (func->m_inlineeId + 1));
        }
#endif
    }

    m_bailOutRecordCount = 0;
    IR::Instr * insertBailInAfter = nullptr;
    BailOutInfo * bailOutInfoForBailIn = nullptr;
    bool endOfBasicBlock = true;
    FOREACH_INSTR_EDITING(instr, instrNext, currentInstr)
    {LOGMEIN("LinearScan.cpp] 147\n");
        if (instr->GetNumber() == 0)
        {LOGMEIN("LinearScan.cpp] 149\n");
            AssertMsg(LowererMD::IsAssign(instr), "Only expect spill code here");
            continue;
        }

#if DBG_DUMP && defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::LinearScanPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {LOGMEIN("LinearScan.cpp] 156\n");
            instr->Dump();
        }
#endif // DBG

        this->currentInstr = instr;
        if(instr->StartsBasicBlock() || endOfBasicBlock)
        {LOGMEIN("LinearScan.cpp] 163\n");
            endOfBasicBlock = false;
            ++currentBlockNumber;
        }

        if (instr->IsLabelInstr())
        {LOGMEIN("LinearScan.cpp] 169\n");
            this->lastLabel = instr->AsLabelInstr();
            if (this->lastLabel->m_loweredBasicBlock)
            {LOGMEIN("LinearScan.cpp] 172\n");
                this->currentBlock = this->lastLabel->m_loweredBasicBlock;
            }
            else if(currentBlock->HasData())
            {LOGMEIN("LinearScan.cpp] 176\n");
                // Check if the previous block has fall-through. If so, retain the block info. If not, create empty info.
                IR::Instr *const prevInstr = instr->GetPrevRealInstrOrLabel();
                Assert(prevInstr);
                if(!prevInstr->HasFallThrough())
                {LOGMEIN("LinearScan.cpp] 181\n");
                    currentBlock = LoweredBasicBlock::New(&tempAlloc);
                }
            }
            this->currentRegion = this->lastLabel->GetRegion();
        }
        else if (instr->IsBranchInstr())
        {LOGMEIN("LinearScan.cpp] 188\n");
            if (this->func->HasTry() && this->func->DoOptimizeTryCatch())
            {LOGMEIN("LinearScan.cpp] 190\n");
                this->ProcessEHRegionBoundary(instr);
            }
            this->ProcessSecondChanceBoundary(instr->AsBranchInstr());
        }

        this->CheckIfInLoop(instr);

        if (this->RemoveDeadStores(instr))
        {LOGMEIN("LinearScan.cpp] 199\n");
            continue;
        }

        if (instr->HasBailOutInfo())
        {LOGMEIN("LinearScan.cpp] 204\n");
            if (this->currentRegion)
            {LOGMEIN("LinearScan.cpp] 206\n");
                RegionType curRegType = this->currentRegion->GetType();
                Assert(curRegType != RegionTypeFinally); //Finally regions are not optimized yet
                if (curRegType == RegionTypeTry || curRegType == RegionTypeCatch)
                {LOGMEIN("LinearScan.cpp] 210\n");
                    this->func->hasBailoutInEHRegion = true;
                }
            }

            this->FillBailOutRecord(instr);
            if (instr->GetBailOutKind() == IR::BailOutForGeneratorYield)
            {LOGMEIN("LinearScan.cpp] 217\n");
                Assert(instr->m_next->IsLabelInstr());
                insertBailInAfter = instr->m_next;
                bailOutInfoForBailIn = instr->GetBailOutInfo();
            }
        }

        this->SetSrcRegs(instr);
        this->EndDeadLifetimes(instr);

        this->CheckOpHelper(instr);

        this->KillImplicitRegs(instr);

        this->AllocateNewLifetimes(instr);
        this->SetDstReg(instr);

        this->EndDeadOpHelperLifetimes(instr);

        if (instr->IsLabelInstr())
        {LOGMEIN("LinearScan.cpp] 237\n");
            this->ProcessSecondChanceBoundary(instr->AsLabelInstr());
        }

#if DBG
        this->CheckInvariants();
#endif // DBG

        if(instr->EndsBasicBlock())
        {LOGMEIN("LinearScan.cpp] 246\n");
            endOfBasicBlock = true;
        }

        if (insertBailInAfter == instr)
        {LOGMEIN("LinearScan.cpp] 251\n");
            instrNext = linearScanMD.GenerateBailInForGeneratorYield(instr, bailOutInfoForBailIn);
            insertBailInAfter = nullptr;
            bailOutInfoForBailIn = nullptr;
        }
    }NEXT_INSTR_EDITING;

    if (func->hasBailout)
    {LOGMEIN("LinearScan.cpp] 259\n");
        for (uint i = 0; i <= func->m_inlineeId; i++)
        {LOGMEIN("LinearScan.cpp] 261\n");
            if (globalBailOutRecordTables[i] != nullptr)
            {LOGMEIN("LinearScan.cpp] 263\n");
                globalBailOutRecordTables[i]->Finalize(nativeAllocator, &tempAlloc);
#ifdef PROFILE_BAILOUT_RECORD_MEMORY
                if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
                {LOGMEIN("LinearScan.cpp] 267\n");
                    func->GetScriptContext()->bailOutOffsetBytes += sizeof(GlobalBailOutRecordDataRow) * globalBailOutRecordTables[i]->length;
                    func->GetScriptContext()->bailOutRecordBytes += sizeof(GlobalBailOutRecordDataRow) * globalBailOutRecordTables[i]->length;
                }
#endif
            }
        }
    }

    AssertMsg((this->intRegUsedCount + this->floatRegUsedCount) == this->linearScanMD.UnAllocatableRegCount(this->func) , "RegUsedCount is wrong");
    AssertMsg(this->activeLiveranges->Empty(), "Active list not empty");
    AssertMsg(this->stackPackInUseLiveRanges->Empty(), "Spilled list not empty");

    AssertMsg(!this->opHelperBlockIter.IsValid(), "Got to the end with a helper block still on the list?");

    Assert(this->currentBlock->inlineeStack.Count() == 0);
    this->InsertOpHelperSpillAndRestores();

#if _M_IX86
# if ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.Instrument.IsEnabled(Js::LinearScanPhase, this->func->GetSourceContextId(),this->func->GetLocalFunctionId()))
    {LOGMEIN("LinearScan.cpp] 288\n");
        this->DynamicStatsInstrument();
    }
# endif
#endif

#if DBG_DUMP
    if (PHASE_STATS(Js::LinearScanPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 296\n");
        this->PrintStats();
    }
    if (PHASE_TRACE(Js::StackPackPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 300\n");
        Output::Print(_u("---------------------------\n"));
    }
#endif // DBG_DUMP
    DebugOnly(this->func->allowRemoveBailOutArgInstr = true);
}

JitArenaAllocator *
LinearScan::GetTempAlloc()
{LOGMEIN("LinearScan.cpp] 309\n");
    Assert(tempAlloc);
    return tempAlloc;
}

#if DBG
void
LinearScan::CheckInvariants() const
{LOGMEIN("LinearScan.cpp] 317\n");
    BitVector bv = this->nonAllocatableRegs;
    uint32 lastend = 0;
    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->activeLiveranges)
    {LOGMEIN("LinearScan.cpp] 321\n");
        // Make sure there are only one lifetime per reg
        Assert(!bv.Test(lifetime->reg));
        bv.Set(lifetime->reg);
        Assert(!lifetime->isOpHelperSpilled);
        Assert(!lifetime->isSpilled);
        Assert(lifetime->end >= lastend);
        lastend = lifetime->end;
    }
    NEXT_SLIST_ENTRY;

    // Make sure the active reg bit vector is correct
    Assert(bv.Equal(this->activeRegs));

    uint ints = 0, floats = 0;
    FOREACH_BITSET_IN_UNITBV(index, this->activeRegs, BitVector)
    {LOGMEIN("LinearScan.cpp] 337\n");
        if (IRType_IsFloat(RegTypes[index]))
        {LOGMEIN("LinearScan.cpp] 339\n");
            floats++;
        }
        else
        {
            ints++;
        }
    }
    NEXT_BITSET_IN_UNITBV;
    Assert(ints == this->intRegUsedCount);
    Assert(floats == this->floatRegUsedCount);
    Assert((this->intRegUsedCount + this->floatRegUsedCount) == this->activeRegs.Count());

    bv.ClearAll();

    lastend = 0;
    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->opHelperSpilledLiveranges)
    {LOGMEIN("LinearScan.cpp] 356\n");
        // Make sure there are only one lifetime per reg in the op helper spilled liveranges
        Assert(!bv.Test(lifetime->reg));
        if (!lifetime->cantOpHelperSpill)
        {LOGMEIN("LinearScan.cpp] 360\n");
            bv.Set(lifetime->reg);
            Assert(lifetime->isOpHelperSpilled);
            Assert(!lifetime->isSpilled);
        }
        Assert(lifetime->end >= lastend);
        lastend = lifetime->end;
    }
    NEXT_SLIST_ENTRY;

    // Make sure the opHelperSpilledRegs bit vector is correct
    Assert(bv.Equal(this->opHelperSpilledRegs));

    for (int i = 0; i < RegNumCount; i++)
    {LOGMEIN("LinearScan.cpp] 374\n");
        if (this->tempRegs.Test(i))
        {LOGMEIN("LinearScan.cpp] 376\n");
            Assert(this->tempRegLifetimes[i]->reg == i);
        }
    }

    FOREACH_BITSET_IN_UNITBV(reg, this->secondChanceRegs, BitVector)
    {LOGMEIN("LinearScan.cpp] 382\n");
        Lifetime *lifetime = this->regContent[reg];
        Assert(lifetime);
        StackSym *sym = lifetime->sym;
        Assert(lifetime->isSecondChanceAllocated);
        Assert(sym->IsConst() || sym->IsAllocated());  // Should have been spilled already.
    } NEXT_BITSET_IN_UNITBV;

}
#endif // DBG

// LinearScan::Init
// Initialize bit vectors
void
LinearScan::Init()
{LOGMEIN("LinearScan.cpp] 397\n");
    FOREACH_REG(reg)
    {LOGMEIN("LinearScan.cpp] 399\n");
        // Registers that can't be used are set to active, and will remain this way
        if (!LinearScan::IsAllocatable(reg))
        {LOGMEIN("LinearScan.cpp] 402\n");
            this->activeRegs.Set(reg);
            if (IRType_IsFloat(RegTypes[reg]))
            {LOGMEIN("LinearScan.cpp] 405\n");
                this->floatRegUsedCount++;
            }
            else
            {
                this->intRegUsedCount++;
            }
        }
        if (RegTypes[reg] == TyMachReg)
        {LOGMEIN("LinearScan.cpp] 414\n");
            // JIT64_TODO: Rename int32Regs to machIntRegs.
            this->int32Regs.Set(reg);
            numInt32Regs++;
        }
        else if (RegTypes[reg] == TyFloat64)
        {LOGMEIN("LinearScan.cpp] 420\n");
            this->floatRegs.Set(reg);
            numFloatRegs++;
        }
        if (LinearScan::IsCallerSaved(reg))
        {LOGMEIN("LinearScan.cpp] 425\n");
            this->callerSavedRegs.Set(reg);
        }
        if (LinearScan::IsCalleeSaved(reg))
        {LOGMEIN("LinearScan.cpp] 429\n");
            this->calleeSavedRegs.Set(reg);
        }
        this->regContent[reg] = nullptr;
    } NEXT_REG;

    this->instrUseRegs.ClearAll();
    this->secondChanceRegs.ClearAll();

    this->linearScanMD.Init(this);

#if DBG
    this->nonAllocatableRegs = this->activeRegs;
#endif

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 446\n");
        this->func->DumpHeader();
    }
#endif
}

// LinearScan::CheckIfInLoop
// Track whether the current instruction is in a loop or not.
bool
LinearScan::CheckIfInLoop(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 456\n");
    if (this->IsInLoop())
    {LOGMEIN("LinearScan.cpp] 458\n");
        // Look for end of loop

        AssertMsg(this->curLoop->regAlloc.loopEnd != 0, "Something is wrong here....");

        if (instr->GetNumber() >= this->curLoop->regAlloc.loopEnd)
        {LOGMEIN("LinearScan.cpp] 464\n");
            AssertMsg(instr->IsBranchInstr(), "Loop tail should be a branchInstr");
            while (this->IsInLoop() && instr->GetNumber() >= this->curLoop->regAlloc.loopEnd)
            {LOGMEIN("LinearScan.cpp] 467\n");
                this->loopNest--;
                this->curLoop->isProcessed = true;
                this->curLoop = this->curLoop->parent;
                if (this->loopNest == 0)
                {LOGMEIN("LinearScan.cpp] 472\n");
                    this->liveOnBackEdgeSyms->ClearAll();
                }
            }
        }
    }
    if (instr->IsLabelInstr() && instr->AsLabelInstr()->m_isLoopTop)
    {LOGMEIN("LinearScan.cpp] 479\n");
        IR::LabelInstr * labelInstr = instr->AsLabelInstr();
        Loop *parentLoop = this->curLoop;
        if (parentLoop)
        {LOGMEIN("LinearScan.cpp] 483\n");
            parentLoop->isLeaf = false;
        }
        this->curLoop = labelInstr->GetLoop();
        this->curLoop->isProcessed = false;
        // Lexically nested may not always nest in a flow based way:
        //      while(i--) {
        //          if (cond) {
        //              while(j--) {
        //              }
        //              break;
        //          }
        //      }
        // These look nested, but they are not...
        // So update the flow based parent to be lexical or we won't be able to figure out when we get back
        // to the outer loop.
        // REVIEW: This isn't necessary anymore now that break blocks are moved out of the loops.

        this->curLoop->parent = parentLoop;
        this->curLoop->regAlloc.defdInLoopBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
        this->curLoop->regAlloc.symRegUseBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
        this->curLoop->regAlloc.loopStart = labelInstr->GetNumber();
        this->curLoop->regAlloc.exitRegContentList = JitAnew(this->tempAlloc, SList<Lifetime **>, this->tempAlloc);
        this->curLoop->regAlloc.regUseBv = 0;

        this->liveOnBackEdgeSyms->Or(this->curLoop->regAlloc.liveOnBackEdgeSyms);
        this->loopNest++;
    }
    return this->IsInLoop();
}

void
LinearScan::InsertOpHelperSpillAndRestores()
{LOGMEIN("LinearScan.cpp] 516\n");
    linearScanMD.InsertOpHelperSpillAndRestores(opHelperBlockList);
}

void
LinearScan::CheckOpHelper(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 522\n");
    if (this->IsInHelperBlock())
    {LOGMEIN("LinearScan.cpp] 524\n");
        if (this->currentOpHelperBlock->opHelperEndInstr == instr)
        {LOGMEIN("LinearScan.cpp] 526\n");
            // Get targetInstr if we can.
            // We can deterministically get it only for unconditional branches, as conditional branch may fall through.
            IR::Instr * targetInstr = nullptr;
            if (instr->IsBranchInstr() && instr->AsBranchInstr()->IsUnconditional())
            {LOGMEIN("LinearScan.cpp] 531\n");
                AssertMsg(!instr->AsBranchInstr()->IsMultiBranch(), "Not supported for Multibranch");
                targetInstr = instr->AsBranchInstr()->GetTarget();
            }

            /*
             * Keep track of the number of registers we've had to
             * store and restore around a helper block for LinearScanMD (on ARM
             * and X64). We need this to be able to allocate space in the frame.
             * We can't emit a PUSH/POP sequence around the block like IA32 because
             * the stack pointer can't move outside the prolog.
             */
            uint32 helperSpilledLiverangeCount = 0;

            // Exiting a helper block.  We are going to insert
            // the restore here after linear scan.  So put all the restored
            // lifetime back to active
            while (!this->opHelperSpilledLiveranges->Empty())
            {LOGMEIN("LinearScan.cpp] 549\n");
                Lifetime * lifetime = this->opHelperSpilledLiveranges->Pop();
                lifetime->isOpHelperSpilled = false;

                if (!lifetime->cantOpHelperSpill)
                {LOGMEIN("LinearScan.cpp] 554\n");
                    // Put the life time back to active
                    this->AssignActiveReg(lifetime, lifetime->reg);
                    bool reload = true;
                    // Lifetime ends before the target after helper block, don't need to save and restore helper spilled lifetime.
                    if (targetInstr && lifetime->end < targetInstr->GetNumber())
                    {LOGMEIN("LinearScan.cpp] 560\n");
                        // However, if lifetime is spilled as arg - we still need to spill it because the helper assumes the value
                        // to be available in the stack
                        if (lifetime->isOpHelperSpillAsArg)
                        {LOGMEIN("LinearScan.cpp] 564\n");
                            // we should not attempt to restore it as it is dead on return from the helper.
                            reload = false;
                        }
                        else
                        {
                            Assert(!instr->AsBranchInstr()->IsLoopTail(this->func));
                            continue;
                        }
                    }

                    // Save all the lifetime that needs to be restored
                    OpHelperSpilledLifetime spilledLifetime;
                    spilledLifetime.lifetime = lifetime;
                    spilledLifetime.spillAsArg = lifetime->isOpHelperSpillAsArg;
                    spilledLifetime.reload = reload;
                    /*
                     * Can't unfortunately move this into the else block above because we don't know if this
                     * lifetime will actually get spilled until register allocation completes.
                     * Instead we allocate a slot to this StackSym in LinearScanMD iff
                     * !(lifetime.isSpilled && lifetime.noReloadsIfSpilled).
                     */
                    helperSpilledLiverangeCount++;

                    // save the reg in case it is spilled later.  We still need to save and restore
                    // for the non-loop case.
                    spilledLifetime.reg = lifetime->reg;
                    this->currentOpHelperBlock->spilledLifetime.Prepend(spilledLifetime);
                }
                else
                {
                    // Clear it for the next helper block
                    lifetime->cantOpHelperSpill = false;
                }
                lifetime->isOpHelperSpillAsArg = false;
            }

            this->totalOpHelperFullVisitedLength += this->currentOpHelperBlock->Length();

            // Use a dummy label as the insertion point of the reloads, as second-chance-allocation
            // may insert compensation code right before the branch
            IR::PragmaInstr *dummyLabel = IR::PragmaInstr::New(Js::OpCode::Nop, 0, this->func);
            this->currentOpHelperBlock->opHelperEndInstr->InsertBefore(dummyLabel);
            dummyLabel->CopyNumber(this->currentOpHelperBlock->opHelperEndInstr);
            this->currentOpHelperBlock->opHelperEndInstr = dummyLabel;

            this->opHelperSpilledRegs.ClearAll();
            this->currentOpHelperBlock = nullptr;

            linearScanMD.EndOfHelperBlock(helperSpilledLiverangeCount);
        }
    }

    if (this->opHelperBlockIter.IsValid())
    {LOGMEIN("LinearScan.cpp] 618\n");
        AssertMsg(
            !instr->IsLabelInstr() ||
            !instr->AsLabelInstr()->isOpHelper ||
            this->opHelperBlockIter.Data().opHelperLabel == instr,
            "Found a helper label that doesn't begin the next helper block in the list?");

        if (this->opHelperBlockIter.Data().opHelperLabel == instr)
        {LOGMEIN("LinearScan.cpp] 626\n");
            this->currentOpHelperBlock = &this->opHelperBlockIter.Data();
            this->opHelperBlockIter.Next();
        }
    }
}

uint
LinearScan::HelperBlockStartInstrNumber() const
{LOGMEIN("LinearScan.cpp] 635\n");
    Assert(IsInHelperBlock());
    return this->currentOpHelperBlock->opHelperLabel->GetNumber();
}

uint
LinearScan::HelperBlockEndInstrNumber() const
{LOGMEIN("LinearScan.cpp] 642\n");
    Assert(IsInHelperBlock());
    return this->currentOpHelperBlock->opHelperEndInstr->GetNumber();
}
// LinearScan::AddToActive
// Add a lifetime to the active list.  The list is kept sorted in order lifetime end.
// This makes it easier to pick the lifetimes to retire.
void
LinearScan::AddToActive(Lifetime * lifetime)
{LOGMEIN("LinearScan.cpp] 651\n");
    LinearScan::AddLiveRange(this->activeLiveranges, lifetime);
    this->regContent[lifetime->reg] = lifetime;
    if (lifetime->isSecondChanceAllocated)
    {LOGMEIN("LinearScan.cpp] 655\n");
        this->secondChanceRegs.Set(lifetime->reg);
    }
    else
    {
        Assert(!this->secondChanceRegs.Test(lifetime->reg));
    }
}

void
LinearScan::AddOpHelperSpilled(Lifetime * lifetime)
{LOGMEIN("LinearScan.cpp] 666\n");
    RegNum reg = lifetime->reg;
    Assert(this->IsInHelperBlock());
    Assert(!this->opHelperSpilledRegs.Test(reg));
    Assert(lifetime->isOpHelperSpilled == false);
    Assert(lifetime->cantOpHelperSpill == false);


    this->opHelperSpilledRegs.Set(reg);
    lifetime->isOpHelperSpilled = true;

    this->regContent[reg] = nullptr;
    this->secondChanceRegs.Clear(reg);

    // If a lifetime is being OpHelper spilled and it's an inlinee arg sym
    // we need to make sure its spilled to the sym offset spill space, i.e. isOpHelperSpillAsArg
    // is set. Otherwise, it's value will not be available on inline frame reconstruction.
    if (this->currentBlock->inlineeFrameSyms.Count() > 0 &&
        this->currentBlock->inlineeFrameSyms.ContainsKey(lifetime->sym->m_id) &&
        (lifetime->sym->m_isSingleDef || !lifetime->defList.Empty()))
    {LOGMEIN("LinearScan.cpp] 686\n");
        lifetime->isOpHelperSpillAsArg = true;
        if (!lifetime->sym->IsAllocated())
        {LOGMEIN("LinearScan.cpp] 689\n");
            this->AllocateStackSpace(lifetime);
        }
        this->RecordLoopUse(lifetime, lifetime->reg);
    }
    LinearScan::AddLiveRange(this->opHelperSpilledLiveranges, lifetime);
}

void
LinearScan::RemoveOpHelperSpilled(Lifetime * lifetime)
{LOGMEIN("LinearScan.cpp] 699\n");
    Assert(this->IsInHelperBlock());
    Assert(lifetime->isOpHelperSpilled);
    Assert(lifetime->cantOpHelperSpill == false);
    Assert(this->opHelperSpilledRegs.Test(lifetime->reg));

    this->opHelperSpilledRegs.Clear(lifetime->reg);
    lifetime->isOpHelperSpilled = false;
    lifetime->cantOpHelperSpill = false;
    lifetime->isOpHelperSpillAsArg = false;
    this->opHelperSpilledLiveranges->Remove(lifetime);
}

void
LinearScan::SetCantOpHelperSpill(Lifetime * lifetime)
{LOGMEIN("LinearScan.cpp] 714\n");
    Assert(this->IsInHelperBlock());
    Assert(lifetime->isOpHelperSpilled);
    Assert(lifetime->cantOpHelperSpill == false);

    this->opHelperSpilledRegs.Clear(lifetime->reg);
    lifetime->isOpHelperSpilled = false;
    lifetime->cantOpHelperSpill = true;
}

void
LinearScan::AddLiveRange(SList<Lifetime *> * list, Lifetime * newLifetime)
{
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, list, iter)
    {LOGMEIN("LinearScan.cpp] 728\n");
        if (newLifetime->end < lifetime->end)
        {LOGMEIN("LinearScan.cpp] 730\n");
            break;
        }
    }
    NEXT_SLIST_ENTRY_EDITING;

    iter.InsertBefore(newLifetime);
}

Lifetime *
LinearScan::RemoveRegLiveRange(SList<Lifetime *> * list, RegNum reg)
{
    // Find the register in the active set
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, list, iter)
    {LOGMEIN("LinearScan.cpp] 744\n");
        if (lifetime->reg == reg)
        {LOGMEIN("LinearScan.cpp] 746\n");
            Lifetime * lifetimeReturn = lifetime;
            iter.RemoveCurrent();
            return lifetimeReturn;
        }
    } NEXT_SLIST_ENTRY_EDITING;

    AssertMsg(false, "Can't find life range for a reg");
    return nullptr;
}


// LinearScan::SetDstReg
// Set the reg on each RegOpnd def.
void
LinearScan::SetDstReg(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 762\n");
    //
    // Enregister dst
    //

    IR::Opnd *dst = instr->GetDst();

    if (dst == nullptr)
    {LOGMEIN("LinearScan.cpp] 770\n");
        return;
    }

    if (!dst->IsRegOpnd())
    {LOGMEIN("LinearScan.cpp] 775\n");
        // This could be, for instance, a store to a sym with a large offset
        // that was just assigned when we saw the use.
        this->linearScanMD.LegalizeDef(instr);
        return;
    }

    IR::RegOpnd * regOpnd = dst->AsRegOpnd();
    /*
     * If this is a register used to setup a callsite per
     * a calling convention then mark it unavailable to allocate
     * until we see a CALL.
     */
    if (regOpnd->m_isCallArg)
    {LOGMEIN("LinearScan.cpp] 789\n");
        RegNum callSetupReg = regOpnd->GetReg();
        callSetupRegs.Set(callSetupReg);
    }

    StackSym * stackSym = regOpnd->m_sym;

    // Arg slot sym can be in a RegOpnd for param passed via registers
    // Just use the assigned register
    if (stackSym == nullptr || stackSym->IsArgSlotSym())
    {LOGMEIN("LinearScan.cpp] 799\n");
        //
        // Already allocated register. just spill the destination
        //
        RegNum reg = regOpnd->GetReg();
        if(LinearScan::IsAllocatable(reg))
        {LOGMEIN("LinearScan.cpp] 805\n");
            this->SpillReg(reg);
        }
        this->tempRegs.Clear(reg);
    }
    else
    {
        if (regOpnd->GetReg() != RegNOREG)
        {LOGMEIN("LinearScan.cpp] 813\n");
            this->RecordLoopUse(nullptr, regOpnd->GetReg());
            // Nothing to do
            return;
        }

        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;
        uint32 useCountCost = LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr));
        // Optimistically decrease the useCount.  We'll undo this if we put it on the defList.
        lifetime->SubFromUseCount(useCountCost, this->curLoop);

        if (lifetime->isSpilled)
        {LOGMEIN("LinearScan.cpp] 825\n");
            if (stackSym->IsConst() && !IsSymNonTempLocalVar(stackSym))
            {LOGMEIN("LinearScan.cpp] 827\n");
                // We will reload the constant (but in debug mode, we still need to process this if this is a user var).
                return;
            }

            RegNum reg = regOpnd->GetReg();

            if (reg != RegNOREG)
            {LOGMEIN("LinearScan.cpp] 835\n");
                // It is already assigned, just record it as a temp reg
                this->AssignTempReg(lifetime, reg);
            }
            else
            {
                IR::Opnd *src1 = instr->GetSrc1();
                IR::Opnd *src2 = instr->GetSrc2();

                if ((src1 && src1->IsRegOpnd() && src1->AsRegOpnd()->m_sym == stackSym) ||
                    (src2 && src2->IsRegOpnd() && src2->AsRegOpnd()->m_sym == stackSym))
                {LOGMEIN("LinearScan.cpp] 846\n");
                    // OpEQ: src1 should have a valid reg (put src2 for other targets)
                    reg = this->GetAssignedTempReg(lifetime, dst->GetType());
                    Assert(reg != RegNOREG);
                    RecordDef(lifetime, instr, 0);
                }
                else
                {
                    // Try second chance
                    reg = this->SecondChanceAllocation(lifetime, false);
                    if (reg != RegNOREG)
                    {LOGMEIN("LinearScan.cpp] 857\n");

                        Assert(!stackSym->m_isSingleDef);

                        this->SetReg(regOpnd);
                        // Keep track of defs for this lifetime, in case it gets spilled.
                        RecordDef(lifetime, instr, useCountCost);
                        return;
                    }
                    else
                    {
                        reg = this->GetAssignedTempReg(lifetime, dst->GetType());
                        RecordDef(lifetime, instr, 0);
                    }
                }
                if (LowererMD::IsAssign(instr) && instr->GetSrc1()->IsRegOpnd())
                {LOGMEIN("LinearScan.cpp] 873\n");
                    // Fold the spilled store
                    if (reg != RegNOREG)
                    {LOGMEIN("LinearScan.cpp] 876\n");
                        // If the value is in a temp reg, it's not valid any more.
                        this->tempRegs.Clear(reg);
                    }

                    IRType srcType = instr->GetSrc1()->GetType();

                    instr->ReplaceDst(IR::SymOpnd::New(stackSym, srcType, this->func));
                    this->linearScanMD.LegalizeDef(instr);
                    return;
                }


                if (reg == RegNOREG)
                {LOGMEIN("LinearScan.cpp] 890\n");
                    IR::Opnd *src = instr->GetSrc1();
                    if (src && src->IsRegOpnd() && src->AsRegOpnd()->m_sym == stackSym)
                    {LOGMEIN("LinearScan.cpp] 893\n");
                        // Handle OPEQ's for x86/x64
                        reg = src->AsRegOpnd()->GetReg();
                        AssertMsg(!this->activeRegs.Test(reg), "Shouldn't be active");
                    }
                    else
                    {
                        // The lifetime was spilled, but we still need a reg for this operand.
                        reg = this->FindReg(nullptr, regOpnd);
                    }
                    this->AssignTempReg(lifetime, reg);
                }
            }

            if (!lifetime->isDeadStore && !lifetime->isSecondChanceAllocated)
            {LOGMEIN("LinearScan.cpp] 908\n");
                // Insert a store since the lifetime is spilled
                IR::Opnd *nextDst = instr->m_next->GetDst();

                // Don't need the store however if the next instruction has the same dst
                if (nextDst == nullptr || !nextDst->IsEqual(regOpnd))
                {LOGMEIN("LinearScan.cpp] 914\n");
                    this->InsertStore(instr, regOpnd->m_sym, reg);
                }
            }
        }
        else
        {
            if (lifetime->isOpHelperSpilled)
            {LOGMEIN("LinearScan.cpp] 922\n");
                // We must be in a helper block and the lifetime must
                // start before the helper block
                Assert(this->IsInHelperBlock());
                Assert(lifetime->start < this->HelperBlockStartInstrNumber());

                RegNum reg = lifetime->reg;
                Assert(this->opHelperSpilledRegs.Test(reg));

                if (this->activeRegs.Test(reg))
                {LOGMEIN("LinearScan.cpp] 932\n");
                    // The reg must have been used locally in the helper block
                    // by some other lifetime. Just spill it

                    this->SpillReg(reg);
                }

                // We can't save/restore this reg across the helper call because the restore would overwrite
                // this def, but the def means we don't need to spill at all.  Mark the lifetime as cantOpHelperSpill
                // however in case another helper call in this block tries to spill it.
                this->SetCantOpHelperSpill(lifetime);

                this->AddToActive(lifetime);

                this->tempRegs.Clear(reg);
                this->activeRegs.Set(reg);
                if (RegTypes[reg] == TyMachReg)
                {LOGMEIN("LinearScan.cpp] 949\n");
                    this->intRegUsedCount++;
                }
                else
                {
                    Assert(RegTypes[reg] == TyFloat64);
                    this->floatRegUsedCount++;
                }
            }

            // Keep track of defs for this lifetime, in case it gets spilled.
            RecordDef(lifetime, instr, useCountCost);
        }

        this->SetReg(regOpnd);
    }
}

// Get the stack offset of the non temp locals from the stack.
int32 LinearScan::GetStackOffset(Js::RegSlot regSlotId)
{LOGMEIN("LinearScan.cpp] 969\n");
    int32 stackSlotId = regSlotId - this->func->GetJITFunctionBody()->GetFirstNonTempLocalIndex();
    Assert(stackSlotId >= 0);
    return this->func->GetLocalVarSlotOffset(stackSlotId);
}


//
// This helper function is used for saving bytecode stack sym value to memory / local slots on stack so that we can read it for the locals inspection.
void
LinearScan::WriteThroughForLocal(IR::RegOpnd* regOpnd, Lifetime* lifetime, IR::Instr* instrInsertAfter)
{LOGMEIN("LinearScan.cpp] 980\n");
    Assert(regOpnd);
    Assert(lifetime);
    Assert(instrInsertAfter);

    StackSym* sym = regOpnd->m_sym;
    Assert(IsSymNonTempLocalVar(sym));

    Js::RegSlot slotIndex = sym->GetByteCodeRegSlot();

    // First we insert the write through moves

    sym->m_offset = GetStackOffset(slotIndex);
    sym->m_allocated = true;
    // Save the value on reg to local var slot.
    this->InsertStore(instrInsertAfter, sym, lifetime->reg);
}

bool
LinearScan::NeedsWriteThrough(StackSym * sym)
{LOGMEIN("LinearScan.cpp] 1000\n");
    return this->NeedsWriteThroughForEH(sym) || this->IsSymNonTempLocalVar(sym);
}

bool
LinearScan::NeedsWriteThroughForEH(StackSym * sym)
{LOGMEIN("LinearScan.cpp] 1006\n");
    if (!this->func->HasTry() || !this->func->DoOptimizeTryCatch() || !sym->HasByteCodeRegSlot())
    {LOGMEIN("LinearScan.cpp] 1008\n");
        return false;
    }

    Assert(this->currentRegion);
    return this->currentRegion->writeThroughSymbolsSet && this->currentRegion->writeThroughSymbolsSet->Test(sym->m_id);
}

// Helper routine to check if current sym belongs to non temp bytecodereg
bool
LinearScan::IsSymNonTempLocalVar(StackSym *sym)
{LOGMEIN("LinearScan.cpp] 1019\n");
    Assert(sym);

    if (this->func->IsJitInDebugMode() && sym->HasByteCodeRegSlot())
    {LOGMEIN("LinearScan.cpp] 1023\n");
        Js::RegSlot slotIndex = sym->GetByteCodeRegSlot();

        return this->func->IsNonTempLocalVar(slotIndex);
    }
    return false;
}


// LinearScan::SetSrcRegs
// Set the reg on each RegOpnd use.
// Note that this includes regOpnd of indir dsts...
void
LinearScan::SetSrcRegs(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 1037\n");
    //
    // Enregister srcs
    //

    IR::Opnd *src1 = instr->GetSrc1();

    if (src1 != nullptr)
    {LOGMEIN("LinearScan.cpp] 1045\n");
        // Capture src2 now as folding in SetUses could swab the srcs...
        IR::Opnd *src2 = instr->GetSrc2();

        this->SetUses(instr, src1);

        if (src2 != nullptr)
        {LOGMEIN("LinearScan.cpp] 1052\n");
            this->SetUses(instr, src2);
        }
    }

    IR::Opnd *dst = instr->GetDst();

    if (dst && dst->IsIndirOpnd())
    {LOGMEIN("LinearScan.cpp] 1060\n");
        this->SetUses(instr, dst);
    }

    this->instrUseRegs.ClearAll();
}

// LinearScan::SetUses
void
LinearScan::SetUses(IR::Instr *instr, IR::Opnd *opnd)
{LOGMEIN("LinearScan.cpp] 1070\n");
    switch (opnd->GetKind())
    {LOGMEIN("LinearScan.cpp] 1072\n");
    case IR::OpndKindReg:
        this->SetUse(instr, opnd->AsRegOpnd());
        break;

    case IR::OpndKindSym:
        {LOGMEIN("LinearScan.cpp] 1078\n");
            Sym * sym = opnd->AsSymOpnd()->m_sym;
            if (sym->IsStackSym())
            {LOGMEIN("LinearScan.cpp] 1081\n");
                StackSym* stackSym = sym->AsStackSym();
                if (!stackSym->IsAllocated())
                {LOGMEIN("LinearScan.cpp] 1084\n");
                    func->StackAllocate(stackSym, opnd->GetSize());
                    // StackSym's lifetime is allocated during SCCLiveness::ProcessDst
                    // we might not need to set the flag if the sym is not a dst.
                    if (stackSym->scratch.linearScan.lifetime)
                    {LOGMEIN("LinearScan.cpp] 1089\n");
                        stackSym->scratch.linearScan.lifetime->cantStackPack = true;
                    }
                }
                this->linearScanMD.LegalizeUse(instr, opnd);
            }
        }
        break;
    case IR::OpndKindIndir:
        {LOGMEIN("LinearScan.cpp] 1098\n");
            IR::IndirOpnd * indirOpnd = opnd->AsIndirOpnd();

            this->SetUse(instr, indirOpnd->GetBaseOpnd());

            if (indirOpnd->GetIndexOpnd())
            {LOGMEIN("LinearScan.cpp] 1104\n");
                this->SetUse(instr, indirOpnd->GetIndexOpnd());
            }
        }
        break;
    case IR::OpndKindIntConst:
    case IR::OpndKindAddr:
        this->linearScanMD.LegalizeConstantUse(instr, opnd);
        break;
    };
}

struct FillBailOutState
{
    SListCounted<Js::Var> constantList;
    uint registerSaveCount;
    StackSym * registerSaveSyms[RegNumCount - 1];

    FillBailOutState(JitArenaAllocator * allocator) : constantList(allocator) {LOGMEIN("LinearScan.cpp] 1122\n");}
};


void
LinearScan::FillBailOutOffset(int * offset, StackSym * stackSym, FillBailOutState * state, IR::Instr * instr)
{LOGMEIN("LinearScan.cpp] 1128\n");
    AssertMsg(*offset == 0, "Can't have two active lifetime for the same byte code register");
    if (stackSym->IsConst())
    {LOGMEIN("LinearScan.cpp] 1131\n");
        state->constantList.Prepend(reinterpret_cast<Js::Var>(stackSym->GetLiteralConstValue_PostGlobOpt()));

        // Constant offset are offset by the number of register save slots
        *offset = state->constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();
    }
    else if (stackSym->m_isEncodedConstant)
    {LOGMEIN("LinearScan.cpp] 1138\n");
        Assert(!stackSym->m_isSingleDef);
        state->constantList.Prepend((Js::Var)stackSym->constantValue);

        // Constant offset are offset by the number of register save slots
        *offset = state->constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();
    }
    else
    {
        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;
        Assert(lifetime && lifetime->start < instr->GetNumber() && instr->GetNumber() <= lifetime->end);
        if (instr->GetBailOutKind() == IR::BailOutOnException)
        {LOGMEIN("LinearScan.cpp] 1150\n");
            // Apart from the exception object sym, lifetimes for all other syms that need to be restored at this bailout,
            // must have been spilled at least once (at the TryCatch, or at the Leave, or both)
            // Post spilling, a lifetime could have been second chance allocated. But, it should still have stack allocated for its sym
            Assert(stackSym->IsAllocated() || (stackSym == this->currentRegion->GetExceptionObjectSym()));
        }

        this->PrepareForUse(lifetime);
        if (lifetime->isSpilled ||
            ((instr->GetBailOutKind() == IR::BailOutOnException) && (stackSym != this->currentRegion->GetExceptionObjectSym()))) // BailOutOnException must restore from memory
        {LOGMEIN("LinearScan.cpp] 1160\n");
            Assert(stackSym->IsAllocated());
#ifdef MD_GROW_LOCALS_AREA_UP
            *offset = -((int)stackSym->m_offset + BailOutInfo::StackSymBias);
#else
            // Stack offset are negative, includes the PUSH EBP and return address
            *offset = stackSym->m_offset - (2 * MachPtr);
#endif
        }
        else
        {
            Assert(lifetime->reg != RegNOREG);
            Assert(state->registerSaveSyms[lifetime->reg - 1] == nullptr ||
                state->registerSaveSyms[lifetime->reg - 1] == stackSym);
            AssertMsg((stackSym->IsFloat64() || stackSym->IsSimd128()) && RegTypes[lifetime->reg] == TyFloat64 ||
                !(stackSym->IsFloat64() || stackSym->IsSimd128()) && RegTypes[lifetime->reg] != TyFloat64,
                      "Trying to save float64 sym into non-float64 reg or non-float64 sym into float64 reg");

            // Save the register value to the register save space using the reg enum value as index
            state->registerSaveSyms[lifetime->reg - 1] = stackSym;
            *offset = LinearScanMD::GetRegisterSaveIndex(lifetime->reg);

            state->registerSaveCount++;
        }
    }
}

struct FuncBailOutData
{
    Func * func;
    BailOutRecord * bailOutRecord;
    int * localOffsets;
    BVFixed * losslessInt32Syms;
    BVFixed * float64Syms;

    // SIMD_JS
    BVFixed * simd128F4Syms;
    BVFixed * simd128I4Syms;
    BVFixed * simd128I8Syms;
    BVFixed * simd128I16Syms;
    BVFixed * simd128U4Syms;
    BVFixed * simd128U8Syms;
    BVFixed * simd128U16Syms;
    BVFixed * simd128B4Syms;
    BVFixed * simd128B8Syms;
    BVFixed * simd128B16Syms;

    void Initialize(Func * func, JitArenaAllocator * tempAllocator);
    void FinalizeLocalOffsets(JitArenaAllocator *allocator, GlobalBailOutRecordDataTable *table, uint **lastUpdatedRowIndices);
    void Clear(JitArenaAllocator * tempAllocator);
};

void
FuncBailOutData::Initialize(Func * func, JitArenaAllocator * tempAllocator)
{LOGMEIN("LinearScan.cpp] 1214\n");
    Js::RegSlot localsCount = func->GetJITFunctionBody()->GetLocalsCount();
    this->func = func;
    this->localOffsets = AnewArrayZ(tempAllocator, int, localsCount);
    this->losslessInt32Syms = BVFixed::New(localsCount, tempAllocator);
    this->float64Syms = BVFixed::New(localsCount, tempAllocator);
    // SIMD_JS
    this->simd128F4Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128I4Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128I8Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128I16Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128U4Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128U8Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128U16Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128B4Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128B8Syms = BVFixed::New(localsCount, tempAllocator);
    this->simd128B16Syms = BVFixed::New(localsCount, tempAllocator);
}

void
FuncBailOutData::FinalizeLocalOffsets(JitArenaAllocator *allocator, GlobalBailOutRecordDataTable *globalBailOutRecordDataTable, uint **lastUpdatedRowIndices)
{LOGMEIN("LinearScan.cpp] 1235\n");
    Js::RegSlot localsCount = func->GetJITFunctionBody()->GetLocalsCount();

    Assert(globalBailOutRecordDataTable != nullptr);
    Assert(lastUpdatedRowIndices != nullptr);

    if (*lastUpdatedRowIndices == nullptr)
    {LOGMEIN("LinearScan.cpp] 1242\n");
        *lastUpdatedRowIndices = JitAnewArrayZ(allocator, uint, localsCount);
        memset(*lastUpdatedRowIndices, -1, sizeof(uint)*localsCount);
    }
    uint32 bailOutRecordId = bailOutRecord->m_bailOutRecordId;
    bailOutRecord->localOffsetsCount = 0;
    for (uint32 i = 0; i < localsCount; i++)
    {LOGMEIN("LinearScan.cpp] 1249\n");
        // if the sym is live
        if (localOffsets[i] != 0)
        {LOGMEIN("LinearScan.cpp] 1252\n");
            bool isFloat = float64Syms->Test(i) != 0;
            bool isInt = losslessInt32Syms->Test(i) != 0;

            // SIMD_JS
            bool isSimd128F4  = simd128F4Syms->Test(i) != 0;
            bool isSimd128I4  = simd128I4Syms->Test(i) != 0;
            bool isSimd128I8  = simd128I8Syms->Test(i) != 0;
            bool isSimd128I16 = simd128I16Syms->Test(i) != 0;
            bool isSimd128U4  = simd128U4Syms->Test(i) != 0;
            bool isSimd128U8  = simd128U8Syms->Test(i) != 0;
            bool isSimd128U16 = simd128U16Syms->Test(i) != 0;
            bool isSimd128B4  = simd128B4Syms->Test(i) != 0;
            bool isSimd128B8  = simd128B8Syms->Test(i) != 0;
            bool isSimd128B16 = simd128B16Syms->Test(i) != 0;

            globalBailOutRecordDataTable->AddOrUpdateRow(allocator, bailOutRecordId, i, isFloat, isInt, 
                isSimd128F4, isSimd128I4, isSimd128I8, isSimd128I16, isSimd128U4, isSimd128U8, isSimd128U16,
                isSimd128B4, isSimd128B8, isSimd128B16, localOffsets[i], &((*lastUpdatedRowIndices)[i]));
            Assert(globalBailOutRecordDataTable->globalBailOutRecordDataRows[(*lastUpdatedRowIndices)[i]].regSlot  == i);
            bailOutRecord->localOffsetsCount++;
        }
    }
}

void
FuncBailOutData::Clear(JitArenaAllocator * tempAllocator)
{LOGMEIN("LinearScan.cpp] 1279\n");
    Js::RegSlot localsCount = func->GetJITFunctionBody()->GetLocalsCount();
    JitAdeleteArray(tempAllocator, localsCount, localOffsets);
    losslessInt32Syms->Delete(tempAllocator);
    float64Syms->Delete(tempAllocator);
    // SIMD_JS
    simd128F4Syms->Delete(tempAllocator);
    simd128I4Syms->Delete(tempAllocator);
    simd128I8Syms->Delete(tempAllocator);
    simd128I16Syms->Delete(tempAllocator);
    simd128U4Syms->Delete(tempAllocator);
    simd128U8Syms->Delete(tempAllocator);
    simd128U16Syms->Delete(tempAllocator);
    simd128B4Syms->Delete(tempAllocator);
    simd128B8Syms->Delete(tempAllocator);
    simd128B16Syms->Delete(tempAllocator);
}

GlobalBailOutRecordDataTable *
LinearScan::EnsureGlobalBailOutRecordTable(Func *func)
{LOGMEIN("LinearScan.cpp] 1299\n");
    Assert(globalBailOutRecordTables != nullptr);
    Func *topFunc = func->GetTopFunc();
    bool isTopFunc = (func == topFunc);
    uint32 inlineeID = isTopFunc ? 0 : func->m_inlineeId;
    NativeCodeData::Allocator * allocator = this->func->GetNativeCodeDataAllocator();

    GlobalBailOutRecordDataTable *globalBailOutRecordDataTable = globalBailOutRecordTables[inlineeID];
    if (globalBailOutRecordDataTable == nullptr)
    {LOGMEIN("LinearScan.cpp] 1308\n");
        globalBailOutRecordDataTable = globalBailOutRecordTables[inlineeID] = NativeCodeDataNew(allocator, GlobalBailOutRecordDataTable);
        globalBailOutRecordDataTable->length = globalBailOutRecordDataTable->size = 0;
        globalBailOutRecordDataTable->isInlinedFunction = !isTopFunc;
        globalBailOutRecordDataTable->hasNonSimpleParams = func->GetHasNonSimpleParams();
        globalBailOutRecordDataTable->hasStackArgOpt = func->IsStackArgsEnabled();
        globalBailOutRecordDataTable->isInlinedConstructor = func->IsInlinedConstructor();
        globalBailOutRecordDataTable->isLoopBody = topFunc->IsLoopBody();
        globalBailOutRecordDataTable->returnValueRegSlot = func->returnValueRegSlot;
        globalBailOutRecordDataTable->isScopeObjRestored = false;
        globalBailOutRecordDataTable->firstActualStackOffset = -1;
        globalBailOutRecordDataTable->registerSaveSpace = (Js::Var*)func->GetThreadContextInfo()->GetBailOutRegisterSaveSpaceAddr();
        globalBailOutRecordDataTable->globalBailOutRecordDataRows = nullptr;
        if (func->GetJITFunctionBody()->GetForInLoopDepth() != 0)
        {LOGMEIN("LinearScan.cpp] 1322\n");
#ifdef MD_GROW_LOCALS_AREA_UP
            Assert(func->GetForInEnumeratorArrayOffset() >= 0);
            globalBailOutRecordDataTable->forInEnumeratorArrayRestoreOffset = func->GetForInEnumeratorArrayOffset();
#else
            // Stack offset are negative, includes the PUSH EBP and return address
            globalBailOutRecordDataTable->forInEnumeratorArrayRestoreOffset = func->GetForInEnumeratorArrayOffset() - (2 * MachPtr);
#endif
        }

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
        {LOGMEIN("LinearScan.cpp] 1334\n");
            topFunc->GetScriptContext()->bailOutOffsetBytes += sizeof(GlobalBailOutRecordDataTable);
            topFunc->GetScriptContext()->bailOutRecordBytes += sizeof(GlobalBailOutRecordDataTable);
        }
#endif
    }
    return globalBailOutRecordDataTable;
}

void
LinearScan::FillBailOutRecord(IR::Instr * instr)
{LOGMEIN("LinearScan.cpp] 1345\n");
    BailOutInfo * bailOutInfo = instr->GetBailOutInfo();

    if (this->func->HasTry())
    {LOGMEIN("LinearScan.cpp] 1349\n");
        RegionType currentRegionType = this->currentRegion->GetType();
        if (currentRegionType == RegionTypeTry || currentRegionType == RegionTypeCatch)
        {LOGMEIN("LinearScan.cpp] 1352\n");
            bailOutInfo->bailOutRecord->ehBailoutData = this->currentRegion->ehBailoutData;
        }
    }

    BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = bailOutInfo->byteCodeUpwardExposedUsed;

    Func * bailOutFunc = bailOutInfo->bailOutFunc;
    uint funcCount = bailOutFunc->inlineDepth + 1;
    FuncBailOutData * funcBailOutData = AnewArray(this->tempAlloc, FuncBailOutData, funcCount);
    uint funcIndex = funcCount - 1;
    funcBailOutData[funcIndex].Initialize(bailOutFunc, this->tempAlloc);
    funcBailOutData[funcIndex].bailOutRecord = bailOutInfo->bailOutRecord;
    bailOutInfo->bailOutRecord->m_bailOutRecordId = m_bailOutRecordCount++;
    bailOutInfo->bailOutRecord->globalBailOutRecordTable = EnsureGlobalBailOutRecordTable(bailOutFunc);

    NativeCodeData::Allocator * allocator = this->func->GetNativeCodeDataAllocator();

#if DBG_DUMP
    if(PHASE_DUMP(Js::BailOutPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 1372\n");
        Output::Print(_u("-------------------Bailout dump -------------------------\n"));
        instr->Dump();
    }
#endif


    // Generate chained bailout record for inlined functions
    Func * currentFunc = bailOutFunc->GetParentFunc();
    uint bailOutOffset = bailOutFunc->postCallByteCodeOffset;
    while (currentFunc != nullptr)
    {LOGMEIN("LinearScan.cpp] 1383\n");
        Assert(funcIndex > 0);
        Assert(bailOutOffset != Js::Constants::NoByteCodeOffset);
        BailOutRecord * bailOutRecord = NativeCodeDataNewZ(allocator, BailOutRecord, bailOutOffset, (uint)-1, IR::BailOutInvalid, currentFunc);
        bailOutRecord->m_bailOutRecordId = m_bailOutRecordCount++;
        bailOutRecord->globalBailOutRecordTable = EnsureGlobalBailOutRecordTable(currentFunc);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        // To indicate this is a subsequent bailout from an inlinee
        bailOutRecord->bailOutOpcode = Js::OpCode::InlineeEnd;
#endif
        funcBailOutData[funcIndex].bailOutRecord->parent = bailOutRecord;
        funcIndex--;
        funcBailOutData[funcIndex].bailOutRecord = bailOutRecord;
        funcBailOutData[funcIndex].Initialize(currentFunc, this->tempAlloc);

        bailOutOffset = currentFunc->postCallByteCodeOffset;
        currentFunc = currentFunc->GetParentFunc();
    }

    Assert(funcIndex == 0);
    Assert(bailOutOffset == Js::Constants::NoByteCodeOffset);

    FillBailOutState state(this->tempAlloc);
    state.registerSaveCount = 0;
    memset(state.registerSaveSyms, 0, sizeof(state.registerSaveSyms));

    // Fill in the constants
    FOREACH_SLISTBASE_ENTRY_EDITING(ConstantStackSymValue, value, &bailOutInfo->usedCapturedValues.constantValues, constantValuesIterator)
    {LOGMEIN("LinearScan.cpp] 1411\n");
        AssertMsg(bailOutInfo->bailOutRecord->bailOutKind != IR::BailOutForGeneratorYield, "constant prop syms unexpected for bail-in for generator yield");
        StackSym * stackSym = value.Key();
        if(stackSym->HasArgSlotNum())
        {LOGMEIN("LinearScan.cpp] 1415\n");
            continue;
        }
        Assert(stackSym->HasByteCodeRegSlot());
        Js::RegSlot i = stackSym->GetByteCodeRegSlot();
        Func * stackSymFunc = stackSym->GetByteCodeFunc();
        uint index = stackSymFunc->inlineDepth;

        Assert(i != Js::Constants::NoRegister);
        Assert(i < stackSymFunc->GetJITFunctionBody()->GetLocalsCount());

        Assert(index < funcCount);
        __analysis_assume(index < funcCount);
        Assert(funcBailOutData[index].func == stackSymFunc);

        Assert(!byteCodeUpwardExposedUsed->Test(stackSym->m_id));

        BailoutConstantValue constValue = value.Value();
        Js::Var varValue = constValue.ToVar(this->func);

        state.constantList.Prepend(varValue);
        AssertMsg(funcBailOutData[index].localOffsets[i] == 0, "Can't have two active lifetime for the same byte code register");
        // Constant offset are offset by the number of register save slots
        funcBailOutData[index].localOffsets[i] = state.constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();

#if DBG_DUMP
        if(PHASE_DUMP(Js::BailOutPhase, this->func))
        {LOGMEIN("LinearScan.cpp] 1442\n");
            Output::Print(_u("Constant stack sym #%d (argOut:%s): "),  i, IsTrueOrFalse(stackSym->HasArgSlotNum()));
            stackSym->Dump();
            Output::Print(_u(" (0x%p (Var) Offset: %d)\n"), varValue, funcBailOutData[index].localOffsets[i]);
        }
#endif
        constantValuesIterator.RemoveCurrent(this->func->m_alloc);
    }
    NEXT_SLISTBASE_ENTRY_EDITING;

    // Fill in the copy prop syms
    FOREACH_SLISTBASE_ENTRY_EDITING(CopyPropSyms, copyPropSyms, &bailOutInfo->usedCapturedValues.copyPropSyms, copyPropSymsIter)
    {LOGMEIN("LinearScan.cpp] 1454\n");
        AssertMsg(bailOutInfo->bailOutRecord->bailOutKind != IR::BailOutForGeneratorYield, "copy prop syms unexpected for bail-in for generator yield");
        StackSym * stackSym = copyPropSyms.Key();
        if(stackSym->HasArgSlotNum())
        {LOGMEIN("LinearScan.cpp] 1458\n");
            continue;
        }
        Js::RegSlot i = stackSym->GetByteCodeRegSlot();
        Func * stackSymFunc = stackSym->GetByteCodeFunc();
        uint index = stackSymFunc->inlineDepth;

        Assert(i != Js::Constants::NoRegister);
        Assert(i < stackSymFunc->GetJITFunctionBody()->GetLocalsCount());

        Assert(index < funcCount);
        __analysis_assume(index < funcCount);
        Assert(funcBailOutData[index].func == stackSymFunc);

        AssertMsg(funcBailOutData[index].localOffsets[i] == 0, "Can't have two active lifetime for the same byte code register");
        Assert(!byteCodeUpwardExposedUsed->Test(stackSym->m_id));

        StackSym * copyStackSym = copyPropSyms.Value();
        this->FillBailOutOffset(&funcBailOutData[index].localOffsets[i], copyStackSym, &state, instr);
        if (copyStackSym->IsInt32())
        {LOGMEIN("LinearScan.cpp] 1478\n");
            funcBailOutData[index].losslessInt32Syms->Set(i);
        }
        else if (copyStackSym->IsFloat64())
        {LOGMEIN("LinearScan.cpp] 1482\n");
            funcBailOutData[index].float64Syms->Set(i);
        }
        // SIMD_JS
        else if (copyStackSym->IsSimd128F4())
        {LOGMEIN("LinearScan.cpp] 1487\n");
            funcBailOutData[index].simd128F4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128I4())
        {LOGMEIN("LinearScan.cpp] 1491\n");
            funcBailOutData[index].simd128I4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128I8())
        {LOGMEIN("LinearScan.cpp] 1495\n");
            funcBailOutData[index].simd128I8Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128I16())
        {LOGMEIN("LinearScan.cpp] 1499\n");
            funcBailOutData[index].simd128I16Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128U4())
        {LOGMEIN("LinearScan.cpp] 1503\n");
            funcBailOutData[index].simd128U4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128U8())
        {LOGMEIN("LinearScan.cpp] 1507\n");
            funcBailOutData[index].simd128U8Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128U16())
        {LOGMEIN("LinearScan.cpp] 1511\n");
            funcBailOutData[index].simd128U16Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128B4())
        {LOGMEIN("LinearScan.cpp] 1515\n");
            funcBailOutData[index].simd128B4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128B8())
        {LOGMEIN("LinearScan.cpp] 1519\n");
            funcBailOutData[index].simd128B8Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128B16())
        {LOGMEIN("LinearScan.cpp] 1523\n");
            funcBailOutData[index].simd128B16Syms->Set(i);
        }
        copyPropSymsIter.RemoveCurrent(this->func->m_alloc);
    }
    NEXT_SLISTBASE_ENTRY_EDITING;

    // Fill in the upward exposed syms
    FOREACH_BITSET_IN_SPARSEBV(id, byteCodeUpwardExposedUsed)
    {LOGMEIN("LinearScan.cpp] 1532\n");
        StackSym * stackSym = this->func->m_symTable->FindStackSym(id);
        Assert(stackSym != nullptr);
        Js::RegSlot i = stackSym->GetByteCodeRegSlot();
        Func * stackSymFunc = stackSym->GetByteCodeFunc();
        uint index = stackSymFunc->inlineDepth;

        Assert(i != Js::Constants::NoRegister);
        Assert(i < stackSymFunc->GetJITFunctionBody()->GetLocalsCount());

        Assert(index < funcCount);
         __analysis_assume(index < funcCount);
        Assert(funcBailOutData[index].func == stackSymFunc);

        AssertMsg(funcBailOutData[index].localOffsets[i] == 0, "Can't have two active lifetime for the same byte code register");

        this->FillBailOutOffset(&funcBailOutData[index].localOffsets[i], stackSym, &state, instr);
        if (stackSym->IsInt32())
        {LOGMEIN("LinearScan.cpp] 1550\n");
            funcBailOutData[index].losslessInt32Syms->Set(i);
        }
        else if (stackSym->IsFloat64())
        {LOGMEIN("LinearScan.cpp] 1554\n");
            funcBailOutData[index].float64Syms->Set(i);
        }
        // SIMD_JS
        else if (stackSym->IsSimd128F4())
        {LOGMEIN("LinearScan.cpp] 1559\n");
            funcBailOutData[index].simd128F4Syms->Set(i);
        }
        else if (stackSym->IsSimd128I4())
        {LOGMEIN("LinearScan.cpp] 1563\n");
            funcBailOutData[index].simd128I4Syms->Set(i);
        }
        else if (stackSym->IsSimd128I8())
        {LOGMEIN("LinearScan.cpp] 1567\n");
            funcBailOutData[index].simd128I8Syms->Set(i);
        }
        else if (stackSym->IsSimd128I16())
        {LOGMEIN("LinearScan.cpp] 1571\n");
            funcBailOutData[index].simd128I16Syms->Set(i);
        }
        else if (stackSym->IsSimd128U4())
        {LOGMEIN("LinearScan.cpp] 1575\n");
            funcBailOutData[index].simd128U4Syms->Set(i);
        }
        else if (stackSym->IsSimd128U8())
        {LOGMEIN("LinearScan.cpp] 1579\n");
            funcBailOutData[index].simd128U8Syms->Set(i);
        }
        else if (stackSym->IsSimd128U16())
        {LOGMEIN("LinearScan.cpp] 1583\n");
            funcBailOutData[index].simd128U16Syms->Set(i);
        }
        else if (stackSym->IsSimd128B4())
        {LOGMEIN("LinearScan.cpp] 1587\n");
            funcBailOutData[index].simd128B4Syms->Set(i);
        }
        else if (stackSym->IsSimd128B8())
        {LOGMEIN("LinearScan.cpp] 1591\n");
            funcBailOutData[index].simd128B8Syms->Set(i);
        }
        else if (stackSym->IsSimd128B16())
        {LOGMEIN("LinearScan.cpp] 1595\n");
            funcBailOutData[index].simd128B16Syms->Set(i);
        }
    }
    NEXT_BITSET_IN_SPARSEBV;

    if (bailOutInfo->usedCapturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(id, bailOutInfo->usedCapturedValues.argObjSyms)
        {LOGMEIN("LinearScan.cpp] 1604\n");
            StackSym * stackSym = this->func->m_symTable->FindStackSym(id);
            Assert(stackSym != nullptr);
            Js::RegSlot i = stackSym->GetByteCodeRegSlot();
            Func * stackSymFunc = stackSym->GetByteCodeFunc();
            uint index = stackSymFunc->inlineDepth;

            Assert(i != Js::Constants::NoRegister);
            Assert(i < stackSymFunc->GetJITFunctionBody()->GetLocalsCount());

            Assert(index < funcCount);
            __analysis_assume(index < funcCount);

            Assert(funcBailOutData[index].func == stackSymFunc);
            AssertMsg(funcBailOutData[index].localOffsets[i] == 0, "Can't have two active lifetime for the same byte code register");

            funcBailOutData[index].localOffsets[i] =  BailOutRecord::GetArgumentsObjectOffset();
        }
        NEXT_BITSET_IN_SPARSEBV;
    }

    // In the debug mode, fill in the rest of non temp locals as well in the records so that the restore stub will just get it automatically.

    if (this->func->IsJitInDebugMode())
    {LOGMEIN("LinearScan.cpp] 1628\n");
        // Need to allow filling the formal args slots.

        if (func->GetJITFunctionBody()->HasPropIdToFormalsMap())
        {LOGMEIN("LinearScan.cpp] 1632\n");
            Assert(func->GetJITFunctionBody()->GetInParamsCount() > 0);
            uint32 endIndex = min(func->GetJITFunctionBody()->GetFirstNonTempLocalIndex() + func->GetJITFunctionBody()->GetInParamsCount() - 1, func->GetJITFunctionBody()->GetEndNonTempLocalIndex());
            for (uint32 index = func->GetJITFunctionBody()->GetFirstNonTempLocalIndex(); index < endIndex; index++)
            {LOGMEIN("LinearScan.cpp] 1636\n");
                StackSym * stackSym = this->func->m_symTable->FindStackSym(index);
                if (stackSym != nullptr)
                {LOGMEIN("LinearScan.cpp] 1639\n");
                    Func * stackSymFunc = stackSym->GetByteCodeFunc();

                    Js::RegSlot regSlotId = stackSym->GetByteCodeRegSlot();
                    if (func->IsNonTempLocalVar(regSlotId))
                    {LOGMEIN("LinearScan.cpp] 1644\n");
                        if (!func->GetJITFunctionBody()->IsRegSlotFormal(regSlotId - func->GetJITFunctionBody()->GetFirstNonTempLocalIndex()))
                        {LOGMEIN("LinearScan.cpp] 1646\n");
                            continue;
                        }

                        uint dataIndex = stackSymFunc->inlineDepth;
                        Assert(dataIndex == 0);     // There is no inlining while in debug mode

                        // Filling in which are not filled already.
                        __analysis_assume(dataIndex == 0);
                        if (funcBailOutData[dataIndex].localOffsets[regSlotId] == 0)
                        {LOGMEIN("LinearScan.cpp] 1656\n");
                            int32 offset = GetStackOffset(regSlotId);

#ifdef MD_GROW_LOCALS_AREA_UP
                            Assert(offset >= 0);
#else
                            Assert(offset < 0);
#endif

                            funcBailOutData[dataIndex].localOffsets[regSlotId] = this->func->AdjustOffsetValue(offset);

                            // We don't support typespec for debug, rework on the bellow assert once we start support them.
                            Assert(!stackSym->IsInt32() && !stackSym->IsFloat64() && !stackSym->IsSimd128());
                        }
                    }
                }
            }
        }
    }

    // fill in the out params
    uint startCallCount = bailOutInfo->startCallCount;

    if (bailOutInfo->totalOutParamCount != 0)
    {LOGMEIN("LinearScan.cpp] 1680\n");
        Assert(startCallCount != 0);
        uint argOutSlot = 0;
        uint * startCallOutParamCounts = (uint*)NativeCodeDataNewArrayNoFixup(allocator, UIntType<DataDesc_ArgOutOffsetInfo_StartCallOutParamCounts>, startCallCount);
#ifdef _M_IX86
        uint * startCallArgRestoreAdjustCounts = (uint*)NativeCodeDataNewArrayNoFixup(allocator, UIntType<DataDesc_ArgOutOffsetInfo_StartCallOutParamCounts>, startCallCount);
#endif
        NativeCodeData::AllocatorNoFixup<BVFixed>* allocatorT = (NativeCodeData::AllocatorNoFixup<BVFixed>*)allocator;
        BVFixed * argOutFloat64Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutLosslessInt32Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        // SIMD_JS
        BVFixed * argOutSimd128F4Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128I4Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128I8Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128I16Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128U4Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128U8Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128U16Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128B4Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128B8Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);
        BVFixed * argOutSimd128B16Syms = BVFixed::New(bailOutInfo->totalOutParamCount, allocatorT);

        int* outParamOffsets = bailOutInfo->outParamOffsets = (int*)NativeCodeDataNewArrayZNoFixup(allocator, IntType<DataDesc_BailoutInfo_CotalOutParamCount>, bailOutInfo->totalOutParamCount);
#ifdef _M_IX86
        int currentStackOffset = 0;
        bailOutInfo->outParamFrameAdjustArgSlot = JitAnew(this->func->m_alloc, BVSparse<JitArenaAllocator>, this->func->m_alloc);
#endif
        if (this->func->HasInlinee())
        {LOGMEIN("LinearScan.cpp] 1708\n");
            bailOutInfo->outParamInlinedArgSlot = JitAnew(this->func->m_alloc, BVSparse<JitArenaAllocator>, this->func->m_alloc);
        }

#if DBG
        uint lastFuncIndex = 0;
#endif
        for (uint i = 0; i < startCallCount; i++)
        {LOGMEIN("LinearScan.cpp] 1716\n");
            uint outParamStart = argOutSlot;                     // Start of the out param offset for the current start call
            // Number of out param for the current start call
            uint outParamCount = bailOutInfo->GetStartCallOutParamCount(i);
            startCallOutParamCounts[i] = outParamCount;
#ifdef _M_IX86
            startCallArgRestoreAdjustCounts[i] = bailOutInfo->startCallInfo[i].argRestoreAdjustCount;
            // Only x86 has a progression of pushes of out args, with stack alignment.
            bool fDoStackAdjust = false;
            if (!bailOutInfo->inlinedStartCall->Test(i))
            {LOGMEIN("LinearScan.cpp] 1726\n");
                // Only do the stack adjustment if the StartCall has not been moved down past the bailout.
                fDoStackAdjust = bailOutInfo->NeedsStartCallAdjust(i, instr);
                if (fDoStackAdjust)
                {LOGMEIN("LinearScan.cpp] 1730\n");
                    currentStackOffset -= Math::Align<int>(outParamCount * MachPtr, MachStackAlignment);
                }
            }
#endif

            Func * currentStartCallFunc = bailOutInfo->startCallFunc[i];
#if DBG
            Assert(lastFuncIndex <= currentStartCallFunc->inlineDepth);
            lastFuncIndex = currentStartCallFunc->inlineDepth;
#endif

            FuncBailOutData& currentFuncBailOutData = funcBailOutData[currentStartCallFunc->inlineDepth];
            BailOutRecord * currentBailOutRecord = currentFuncBailOutData.bailOutRecord;
            if (currentBailOutRecord->argOutOffsetInfo == nullptr)
            {LOGMEIN("LinearScan.cpp] 1745\n");
                currentBailOutRecord->argOutOffsetInfo = NativeCodeDataNew(allocator, BailOutRecord::ArgOutOffsetInfo);
                currentBailOutRecord->argOutOffsetInfo->argOutFloat64Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutLosslessInt32Syms = nullptr;
                // SIMD_JS
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128F4Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128I4Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128I8Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128I16Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128U4Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128U8Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128U16Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128B4Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128B8Syms = nullptr;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128B16Syms = nullptr;

                currentBailOutRecord->argOutOffsetInfo->argOutSymStart = 0;
                currentBailOutRecord->argOutOffsetInfo->outParamOffsets = nullptr;
                currentBailOutRecord->argOutOffsetInfo->startCallOutParamCounts = nullptr;

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
                if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
                {LOGMEIN("LinearScan.cpp] 1767\n");
                    this->func->GetScriptContext()->bailOutRecordBytes += sizeof(BailOutRecord::ArgOutOffsetInfo);
                }
#endif
            }

            currentBailOutRecord->argOutOffsetInfo->startCallCount++;
            if (currentBailOutRecord->argOutOffsetInfo->outParamOffsets == nullptr)
            {LOGMEIN("LinearScan.cpp] 1775\n");
                Assert(currentBailOutRecord->argOutOffsetInfo->startCallOutParamCounts == nullptr);
                currentBailOutRecord->argOutOffsetInfo->startCallIndex = i;
                currentBailOutRecord->argOutOffsetInfo->startCallOutParamCounts = &startCallOutParamCounts[i];
#ifdef _M_IX86
                currentBailOutRecord->startCallArgRestoreAdjustCounts = &startCallArgRestoreAdjustCounts[i];
#endif
                currentBailOutRecord->argOutOffsetInfo->outParamOffsets = &outParamOffsets[outParamStart];
                currentBailOutRecord->argOutOffsetInfo->argOutSymStart = outParamStart;
                currentBailOutRecord->argOutOffsetInfo->argOutFloat64Syms = argOutFloat64Syms;
                currentBailOutRecord->argOutOffsetInfo->argOutLosslessInt32Syms = argOutLosslessInt32Syms;
                // SIMD_JS
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128F4Syms  = argOutSimd128F4Syms;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128I4Syms  = argOutSimd128I4Syms  ;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128I8Syms  = argOutSimd128I8Syms  ;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128I16Syms = argOutSimd128I16Syms ;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128U4Syms  = argOutSimd128U4Syms  ;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128U8Syms  = argOutSimd128U8Syms  ;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128U16Syms = argOutSimd128U16Syms ;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128B4Syms = argOutSimd128U4Syms;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128B8Syms = argOutSimd128U8Syms;
                currentBailOutRecord->argOutOffsetInfo->argOutSimd128B16Syms = argOutSimd128U16Syms;


            }
#if DBG_DUMP
            if (PHASE_DUMP(Js::BailOutPhase, this->func))
            {LOGMEIN("LinearScan.cpp] 1802\n");
                Output::Print(_u("Bailout function: %s [#%d] \n"), currentStartCallFunc->GetJITFunctionBody()->GetDisplayName(),
                    currentStartCallFunc->GetJITFunctionBody()->GetFunctionNumber());
            }
#endif
            for (uint j = 0; j < outParamCount; j++, argOutSlot++)
            {LOGMEIN("LinearScan.cpp] 1808\n");
                StackSym * sym = bailOutInfo->argOutSyms[argOutSlot];
                if (sym == nullptr)
                {LOGMEIN("LinearScan.cpp] 1811\n");
                    // This can happen when instr with bailout occurs before all ArgOuts for current call instr are processed.
                    continue;
                }

                Assert(sym->GetArgSlotNum() > 0 && sym->GetArgSlotNum() <= outParamCount);
                uint argSlot = sym->GetArgSlotNum() - 1;
                uint outParamOffsetIndex = outParamStart + argSlot;
                if (!sym->m_isBailOutReferenced && !sym->IsArgSlotSym())
                {
                    FOREACH_SLISTBASE_ENTRY_EDITING(ConstantStackSymValue, constantValue, &bailOutInfo->usedCapturedValues.constantValues, iterator)
                    {LOGMEIN("LinearScan.cpp] 1822\n");
                        if (constantValue.Key()->m_id == sym->m_id)
                        {LOGMEIN("LinearScan.cpp] 1824\n");
                            Js::Var varValue = constantValue.Value().ToVar(func);
                            state.constantList.Prepend(varValue);
                            outParamOffsets[outParamOffsetIndex] = state.constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();

#if DBG_DUMP
                            if (PHASE_DUMP(Js::BailOutPhase, this->func))
                            {LOGMEIN("LinearScan.cpp] 1831\n");
                                Output::Print(_u("OutParam #%d: "), argSlot);
                                sym->Dump();
                                Output::Print(_u(" (0x%p (Var)))\n"), varValue);
                            }
#endif
                            iterator.RemoveCurrent(func->m_alloc);
                            break;
                        }
                    }
                    NEXT_SLISTBASE_ENTRY_EDITING;
                    if (outParamOffsets[outParamOffsetIndex])
                    {LOGMEIN("LinearScan.cpp] 1843\n");
                        continue;
                    }

                    FOREACH_SLISTBASE_ENTRY_EDITING(CopyPropSyms, copyPropSym, &bailOutInfo->usedCapturedValues.copyPropSyms, iter)
                    {LOGMEIN("LinearScan.cpp] 1848\n");
                        if (copyPropSym.Key()->m_id == sym->m_id)
                        {LOGMEIN("LinearScan.cpp] 1850\n");
                            StackSym * copyStackSym = copyPropSym.Value();

                            BVSparse<JitArenaAllocator>* argObjSyms = bailOutInfo->usedCapturedValues.argObjSyms;
                            if (argObjSyms && argObjSyms->Test(copyStackSym->m_id))
                            {LOGMEIN("LinearScan.cpp] 1855\n");
                                outParamOffsets[outParamOffsetIndex] = BailOutRecord::GetArgumentsObjectOffset();
                            }
                            else
                            {
                                this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], copyStackSym, &state, instr);
                                if (copyStackSym->IsInt32())
                                {LOGMEIN("LinearScan.cpp] 1862\n");
                                    argOutLosslessInt32Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsFloat64())
                                {LOGMEIN("LinearScan.cpp] 1866\n");
                                    argOutFloat64Syms->Set(outParamOffsetIndex);
                                }
                                // SIMD_JS
                                else if (copyStackSym->IsSimd128F4())
                                {LOGMEIN("LinearScan.cpp] 1871\n");
                                    argOutSimd128F4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128I4())
                                {LOGMEIN("LinearScan.cpp] 1875\n");
                                    argOutSimd128I4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128I8())
                                {LOGMEIN("LinearScan.cpp] 1879\n");
                                    argOutSimd128I8Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128I16())
                                {LOGMEIN("LinearScan.cpp] 1883\n");
                                    argOutSimd128I16Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128U4())
                                {LOGMEIN("LinearScan.cpp] 1887\n");
                                    argOutSimd128U4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128U8())
                                {LOGMEIN("LinearScan.cpp] 1891\n");
                                    argOutSimd128U8Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128U16())
                                {LOGMEIN("LinearScan.cpp] 1895\n");
                                    argOutSimd128U16Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128B4())
                                {LOGMEIN("LinearScan.cpp] 1899\n");
                                    argOutSimd128B4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128B8())
                                {LOGMEIN("LinearScan.cpp] 1903\n");
                                    argOutSimd128B8Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128B16())
                                {LOGMEIN("LinearScan.cpp] 1907\n");
                                    argOutSimd128B16Syms->Set(outParamOffsetIndex);
                                }
                            }
#if DBG_DUMP
                            if (PHASE_DUMP(Js::BailOutPhase, this->func))
                            {LOGMEIN("LinearScan.cpp] 1913\n");
                                Output::Print(_u("OutParam #%d: "), argSlot);
                                sym->Dump();
                                Output::Print(_u(" Copy Prop sym:"));
                                copyStackSym->Dump();
                                Output::Print(_u("\n"));
                            }
#endif
                            iter.RemoveCurrent(func->m_alloc);
                            break;
                        }
                    }
                    NEXT_SLISTBASE_ENTRY_EDITING;
                    Assert(outParamOffsets[outParamOffsetIndex] != 0);
                }
                else
                {
                    if (sym->IsArgSlotSym())
                    {LOGMEIN("LinearScan.cpp] 1931\n");
                        if (sym->m_isSingleDef)
                        {LOGMEIN("LinearScan.cpp] 1933\n");
                            Assert(sym->m_instrDef->m_func == currentStartCallFunc);

                            IR::Instr * instrDef = sym->m_instrDef;
                            Assert(LowererMD::IsAssign(instrDef));

                            if (instrDef->GetNumber() < instr->GetNumber())
                            {LOGMEIN("LinearScan.cpp] 1940\n");
                                // The ArgOut instr is above current bailout instr.
                                AssertMsg(sym->IsVar(), "Arg out slot can only be var.");
                                if (sym->m_isInlinedArgSlot)
                                {LOGMEIN("LinearScan.cpp] 1944\n");
                                    Assert(this->func->HasInlinee());
#ifdef MD_GROW_LOCALS_AREA_UP
                                    outParamOffsets[outParamOffsetIndex] = -((int)sym->m_offset + BailOutInfo::StackSymBias);
#else
                                    outParamOffsets[outParamOffsetIndex] = sym->m_offset;
#endif
                                    bailOutInfo->outParamInlinedArgSlot->Set(outParamOffsetIndex);
                                }
                                else if (sym->m_isOrphanedArg)
                                {LOGMEIN("LinearScan.cpp] 1954\n");
#ifdef MD_GROW_LOCALS_AREA_UP
                                    outParamOffsets[outParamOffsetIndex] = -((int)sym->m_offset + BailOutInfo::StackSymBias);
#else
                                    // Stack offset are negative, includes the PUSH EBP and return address
                                    outParamOffsets[outParamOffsetIndex] = sym->m_offset - (2 * MachPtr);
#endif
                                }
#ifdef _M_IX86
                                else if (fDoStackAdjust)
                                {LOGMEIN("LinearScan.cpp] 1964\n");
                                    // If we've got args on the stack, then we must have seen (and adjusted for) the StartCall.
                                    // The values is already on the stack
                                    // On AMD64/ARM, ArgOut should have been moved next to the call, and shouldn't have bailout between them
                                    // Except for inlined arg outs
                                    outParamOffsets[outParamOffsetIndex] = currentStackOffset + argSlot * MachPtr;
                                    bailOutInfo->outParamFrameAdjustArgSlot->Set(outParamOffsetIndex);
                                }
#endif
                                else
                                {
                                    this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                                }
                            }
                            else
                            {
                                // The ArgOut instruction might have moved down right next to the call,
                                // because of a register calling convention, cloning, etc.  This loop walks the chain
                                // of assignments to try to find the original location of the assignment where
                                // the value is available.
                                while (!sym->IsConst())
                                {LOGMEIN("LinearScan.cpp] 1985\n");
                                    // the value is in the register
                                    IR::RegOpnd * regOpnd = instrDef->GetSrc1()->AsRegOpnd();
                                    sym = regOpnd->m_sym;

                                    if (sym->scratch.linearScan.lifetime->start < instr->GetNumber())
                                    {LOGMEIN("LinearScan.cpp] 1991\n");
                                        break;
                                    }

                                    if (sym->m_isEncodedConstant)
                                    {LOGMEIN("LinearScan.cpp] 1996\n");
                                        break;
                                    }
                                    // For out parameter we might need to follow multiple assignments
                                    Assert(sym->m_isSingleDef);
                                    instrDef = sym->m_instrDef;
                                    Assert(LowererMD::IsAssign(instrDef));
                                }

                                if (bailOutInfo->usedCapturedValues.argObjSyms && bailOutInfo->usedCapturedValues.argObjSyms->Test(sym->m_id))
                                {LOGMEIN("LinearScan.cpp] 2006\n");
                                    //foo.apply(this,arguments) case and we bailout when the apply is overridden. We need to restore the arguments object.
                                    outParamOffsets[outParamOffsetIndex] = BailOutRecord::GetArgumentsObjectOffset();
                                }
                                else
                                {
                                    this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                                }
                            }
                        }
                    }
                    else
                    {
                        this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                    }

                    if (sym->IsFloat64())
                    {LOGMEIN("LinearScan.cpp] 2023\n");
                        argOutFloat64Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsInt32())
                    {LOGMEIN("LinearScan.cpp] 2027\n");
                        argOutLosslessInt32Syms->Set(outParamOffsetIndex);
                    }
                    // SIMD_JS
                    else if (sym->IsSimd128F4())
                    {LOGMEIN("LinearScan.cpp] 2032\n");
                        argOutSimd128F4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128I4())
                    {LOGMEIN("LinearScan.cpp] 2036\n");
                        argOutSimd128I4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128I8())
                    {LOGMEIN("LinearScan.cpp] 2040\n");
                        argOutSimd128I8Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128I16())
                    {LOGMEIN("LinearScan.cpp] 2044\n");
                        argOutSimd128I16Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128U4())
                    {LOGMEIN("LinearScan.cpp] 2048\n");
                        argOutSimd128U4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128U8())
                    {LOGMEIN("LinearScan.cpp] 2052\n");
                        argOutSimd128U8Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128U16())
                    {LOGMEIN("LinearScan.cpp] 2056\n");
                        argOutSimd128U16Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128B4())
                    {LOGMEIN("LinearScan.cpp] 2060\n");
                        argOutSimd128B4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128B8())
                    {LOGMEIN("LinearScan.cpp] 2064\n");
                        argOutSimd128B8Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128B16())
                    {LOGMEIN("LinearScan.cpp] 2068\n");
                        argOutSimd128B16Syms->Set(outParamOffsetIndex);
                    }
#if DBG_DUMP
                    if (PHASE_DUMP(Js::BailOutPhase, this->func))
                    {LOGMEIN("LinearScan.cpp] 2073\n");
                        Output::Print(_u("OutParam #%d: "), argSlot);
                        sym->Dump();
                        Output::Print(_u("\n"));
                    }
#endif
                }
            }
        }
    }
    else
    {
        Assert(bailOutInfo->argOutSyms == nullptr);
        Assert(bailOutInfo->startCallCount == 0);
    }

    if (this->currentBlock->inlineeStack.Count() > 0)
    {LOGMEIN("LinearScan.cpp] 2090\n");
        this->SpillInlineeArgs(instr);
    }
    else
    {
        // There is a chance that the instruction was hoisting from an inlinee func
        // but if there are no inlinee frames - make sure the instr belongs to the outer func
        // to ensure encoder does not encode an inline frame here - which does not really exist
        instr->m_func = this->func;
    }

    linearScanMD.GenerateBailOut(instr, state.registerSaveSyms, _countof(state.registerSaveSyms));

    // generate the constant table
    Js::Var * constants = NativeCodeDataNewArrayNoFixup(allocator, Js::Var, state.constantList.Count());
    uint constantCount = state.constantList.Count();
    while (!state.constantList.Empty())
    {LOGMEIN("LinearScan.cpp] 2107\n");
        Js::Var value = state.constantList.Head();
        state.constantList.RemoveHead();
        constants[state.constantList.Count()] = value;
    }

    // Generate the stack literal bail out info
    FillStackLiteralBailOutRecord(instr, bailOutInfo, funcBailOutData, funcCount);

    for (uint i = 0; i < funcCount; i++)
    {LOGMEIN("LinearScan.cpp] 2117\n");

        funcBailOutData[i].bailOutRecord->constants = constants;
#if DBG
        funcBailOutData[i].bailOutRecord->inlineDepth = funcBailOutData[i].func->inlineDepth;
        funcBailOutData[i].bailOutRecord->constantCount = constantCount;
#endif
        uint32 tableIndex = funcBailOutData[i].func->IsTopFunc() ? 0 : funcBailOutData[i].func->m_inlineeId;
        funcBailOutData[i].FinalizeLocalOffsets(tempAlloc, this->globalBailOutRecordTables[tableIndex], &(this->lastUpdatedRowIndices[tableIndex]));
#if DBG_DUMP
        if(PHASE_DUMP(Js::BailOutPhase, this->func))
        {LOGMEIN("LinearScan.cpp] 2128\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(_u("Bailout function: %s [%s]\n"), funcBailOutData[i].func->GetJITFunctionBody()->GetDisplayName(), funcBailOutData[i].func->GetDebugNumberSet(debugStringBuffer), i);
            funcBailOutData[i].bailOutRecord->Dump();
        }
#endif
        funcBailOutData[i].Clear(this->tempAlloc);

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
        {LOGMEIN("LinearScan.cpp] 2138\n");
            this->func->GetScriptContext()->bailOutRecordBytes += sizeof(BailOutRecord);
        }
#endif
    }
    JitAdeleteArray(this->tempAlloc, funcCount, funcBailOutData);
}

template <typename Fn>
void
LinearScan::ForEachStackLiteralBailOutInfo(IR::Instr * instr, BailOutInfo * bailOutInfo, FuncBailOutData * funcBailOutData, uint funcCount, Fn fn)
{LOGMEIN("LinearScan.cpp] 2149\n");
    for (uint i = 0; i < bailOutInfo->stackLiteralBailOutInfoCount; i++)
    {LOGMEIN("LinearScan.cpp] 2151\n");
        BailOutInfo::StackLiteralBailOutInfo& stackLiteralBailOutInfo = bailOutInfo->stackLiteralBailOutInfo[i];
        StackSym * stackSym = stackLiteralBailOutInfo.stackSym;
        Assert(stackSym->scratch.linearScan.lifetime->start < instr->GetNumber());
        Assert(stackSym->scratch.linearScan.lifetime->end >= instr->GetNumber());

        Js::RegSlot regSlot = stackSym->GetByteCodeRegSlot();
        Func * stackSymFunc = stackSym->GetByteCodeFunc();
        uint index = stackSymFunc->inlineDepth;

        Assert(regSlot != Js::Constants::NoRegister);
        Assert(regSlot < stackSymFunc->GetJITFunctionBody()->GetLocalsCount());
        Assert(index < funcCount);
        Assert(funcBailOutData[index].func == stackSymFunc);
        Assert(funcBailOutData[index].localOffsets[regSlot] != 0);
        fn(index, stackLiteralBailOutInfo, regSlot);
    }
}

void
LinearScan::FillStackLiteralBailOutRecord(IR::Instr * instr, BailOutInfo * bailOutInfo, FuncBailOutData * funcBailOutData, uint funcCount)
{LOGMEIN("LinearScan.cpp] 2172\n");
    if (bailOutInfo->stackLiteralBailOutInfoCount)
    {
        // Count the data
        ForEachStackLiteralBailOutInfo(instr, bailOutInfo, funcBailOutData, funcCount,
            [=](uint funcIndex, BailOutInfo::StackLiteralBailOutInfo& stackLiteralBailOutInfo, Js::RegSlot regSlot)
        {
            funcBailOutData[funcIndex].bailOutRecord->stackLiteralBailOutRecordCount++;
        });

        // Allocate the data
        NativeCodeData::Allocator * allocator = this->func->GetNativeCodeDataAllocator();
        for (uint i = 0; i < funcCount; i++)
        {LOGMEIN("LinearScan.cpp] 2185\n");
            uint stackLiteralBailOutRecordCount = funcBailOutData[i].bailOutRecord->stackLiteralBailOutRecordCount;
            if (stackLiteralBailOutRecordCount)
            {LOGMEIN("LinearScan.cpp] 2188\n");
                funcBailOutData[i].bailOutRecord->stackLiteralBailOutRecord =
                    NativeCodeDataNewArrayNoFixup(allocator, BailOutRecord::StackLiteralBailOutRecord, stackLiteralBailOutRecordCount);
                // reset the count so we can track how much we have filled below
                funcBailOutData[i].bailOutRecord->stackLiteralBailOutRecordCount = 0;
            }
        }

        // Fill out the data
        ForEachStackLiteralBailOutInfo(instr, bailOutInfo, funcBailOutData, funcCount,
            [=](uint funcIndex, BailOutInfo::StackLiteralBailOutInfo& stackLiteralBailOutInfo, Js::RegSlot regSlot)
        {
            uint& recordIndex = funcBailOutData[funcIndex].bailOutRecord->stackLiteralBailOutRecordCount;
            BailOutRecord::StackLiteralBailOutRecord& stackLiteralBailOutRecord =
                funcBailOutData[funcIndex].bailOutRecord->stackLiteralBailOutRecord[recordIndex++];
            stackLiteralBailOutRecord.regSlot = regSlot;
            stackLiteralBailOutRecord.initFldCount = stackLiteralBailOutInfo.initFldCount;
        });
    }
}

void
LinearScan::PrepareForUse(Lifetime * lifetime)
{LOGMEIN("LinearScan.cpp] 2211\n");
    if (lifetime->isOpHelperSpilled)
    {LOGMEIN("LinearScan.cpp] 2213\n");
        // using a value in a helper that has been spilled in the helper block.
        // Just spill it for real

        // We must be in a helper block and the lifetime must
        // start before the helper block

        Assert(this->IsInHelperBlock());
        Assert(lifetime->start < this->HelperBlockStartInstrNumber());

        IR::Instr *insertionInstr = this->currentOpHelperBlock->opHelperLabel;

        this->RemoveOpHelperSpilled(lifetime);
        this->SpillLiveRange(lifetime, insertionInstr);
    }
}

void
LinearScan::RecordUse(Lifetime * lifetime, IR::Instr * instr, IR::RegOpnd * regOpnd, bool isFromBailout)
{LOGMEIN("LinearScan.cpp] 2232\n");
    uint32 useCountCost = LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr || isFromBailout));

    // We only spill at the use for constants (i.e. reload) or for function with try blocks. We don't
    // have real accurate flow info for the later.
    if ((regOpnd && regOpnd->m_sym->IsConst())
          || (
                 (this->func->HasTry() && !this->func->DoOptimizeTryCatch()) &&
                 this->IsInLoop() &&
                 lifetime->lastUseLabel != this->lastLabel &&
                 this->liveOnBackEdgeSyms->Test(lifetime->sym->m_id) &&
                 !(lifetime->previousDefBlockNumber == currentBlockNumber && !lifetime->defList.Empty())
             ))
    {LOGMEIN("LinearScan.cpp] 2245\n");
        // Keep track of all the uses of this lifetime in case we decide to spill it.
        // Note that we won't need to insert reloads if the use are not in a loop,
        // unless it is a const. We always reload const instead of spilling to the stack.
        //
        // We also don't need to insert reloads if the previous use was in the same basic block (the first use in the block
        // would have done the reload), or the previous def is in the same basic block and the value is still live. Furthermore,
        // if the previous def is in the same basic block, the value is still live, and there's another def after this use in
        // the same basic block, the previous def may not do a spill store, so we must not reload the value from the stack.
        lifetime->useList.Prepend(instr);
        lifetime->lastUseLabel = this->lastLabel;
        lifetime->AddToUseCountAdjust(useCountCost, this->curLoop, this->func);
    }
    else
    {
        if (!isFromBailout)
        {LOGMEIN("LinearScan.cpp] 2261\n");
            // Since we won't reload this use if the lifetime gets spilled, adjust the spill cost to reflect this.
            lifetime->SubFromUseCount(useCountCost, this->curLoop);
        }
    }
    if (this->IsInLoop())
    {LOGMEIN("LinearScan.cpp] 2267\n");
        this->RecordLoopUse(lifetime, lifetime->reg);
    }
}

void LinearScan::RecordLoopUse(Lifetime *lifetime, RegNum reg)
{LOGMEIN("LinearScan.cpp] 2273\n");
    if (!this->IsInLoop())
    {LOGMEIN("LinearScan.cpp] 2275\n");
        return;
    }

    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {LOGMEIN("LinearScan.cpp] 2280\n");
        return;
    }

    // Record on each loop which register live into the loop ended up being used.
    // We are trying to avoid the need for compensation at the bottom of the loop if
    // the reg ends up being spilled before it is actually used.
    Loop *curLoop = this->curLoop;
    SymID symId = (SymID)-1;

    if (lifetime)
    {LOGMEIN("LinearScan.cpp] 2291\n");
        symId = lifetime->sym->m_id;
    }

    while (curLoop)
    {LOGMEIN("LinearScan.cpp] 2296\n");
        // Note that if the lifetime is spilled and reallocated to the same register,
        // will mark it as used when we shouldn't.  However, it is hard at this point to handle
        // the case were a flow edge from the previous allocation merges in with the new allocation.
        // No compensation is inserted to let us know with previous lifetime needs reloading at the bottom of the loop...
        if (lifetime && curLoop->regAlloc.loopTopRegContent[reg] == lifetime)
        {LOGMEIN("LinearScan.cpp] 2302\n");
            curLoop->regAlloc.symRegUseBv->Set(symId);
        }
        curLoop->regAlloc.regUseBv.Set(reg);
        curLoop = curLoop->parent;
    }
}

void
LinearScan::RecordDef(Lifetime *const lifetime, IR::Instr *const instr, const uint32 useCountCost)
{LOGMEIN("LinearScan.cpp] 2312\n");
    Assert(lifetime);
    Assert(instr);
    Assert(instr->GetDst());

    IR::RegOpnd * regOpnd = instr->GetDst()->AsRegOpnd();
    Assert(regOpnd);

    StackSym *const sym = regOpnd->m_sym;

    if (this->IsInLoop())
    {LOGMEIN("LinearScan.cpp] 2323\n");
        Loop *curLoop = this->curLoop;

        while (curLoop)
        {LOGMEIN("LinearScan.cpp] 2327\n");
            curLoop->regAlloc.defdInLoopBv->Set(lifetime->sym->m_id);
            curLoop->regAlloc.regUseBv.Set(lifetime->reg);
            curLoop = curLoop->parent;
        }
    }

    if (lifetime->isSpilled)
    {LOGMEIN("LinearScan.cpp] 2335\n");
        return;
    }

    if (this->NeedsWriteThrough(sym))
    {LOGMEIN("LinearScan.cpp] 2340\n");
        if (this->IsSymNonTempLocalVar(sym))
        {
            // In the debug mode, we will write through on the stack location.
            WriteThroughForLocal(regOpnd, lifetime, instr);
        }
        else
        {
            // If this is a write-through sym, it should be live on the entry to 'try' and should have already
            // been allocated when we spilled all active lifetimes there.
            // If it was not part of the active lifetimes on entry to the 'try' then it must have been spilled
            // earlier and should have stack allocated for it.

            Assert(this->NeedsWriteThroughForEH(sym) && sym->IsAllocated());
            this->InsertStore(instr, sym, lifetime->reg);
        }

        // No need to record-def further as we already have stack allocated for it.
        return;
    }

    if (sym->m_isSingleDef)
    {LOGMEIN("LinearScan.cpp] 2362\n");
        lifetime->AddToUseCount(useCountCost, this->curLoop, this->func);
        // the def of a single-def sym is already on the sym
        return;
    }

    if(lifetime->previousDefBlockNumber == currentBlockNumber && !lifetime->defList.Empty())
    {LOGMEIN("LinearScan.cpp] 2369\n");
        // Only keep track of the last def in each basic block. When there are multiple defs of a sym in a basic block, upon
        // spill of that sym, a store needs to be inserted only after the last def of the sym.
        Assert(lifetime->defList.Head()->GetDst()->AsRegOpnd()->m_sym == sym);
        lifetime->defList.Head() = instr;
    }
    else
    {
        // First def of this sym in the current basic block
        lifetime->previousDefBlockNumber = currentBlockNumber;
        lifetime->defList.Prepend(instr);

        // Keep track of the cost of reinserting all the defs if we choose to spill this way.
        lifetime->allDefsCost += useCountCost;
    }
}

// LinearScan::SetUse
void
LinearScan::SetUse(IR::Instr *instr, IR::RegOpnd *regOpnd)
{LOGMEIN("LinearScan.cpp] 2389\n");
    if (regOpnd->GetReg() != RegNOREG)
    {LOGMEIN("LinearScan.cpp] 2391\n");
        this->RecordLoopUse(nullptr, regOpnd->GetReg());
        return;
    }

    StackSym *sym = regOpnd->m_sym;
    Lifetime * lifetime = sym->scratch.linearScan.lifetime;

    this->PrepareForUse(lifetime);

    if (lifetime->isSpilled)
    {LOGMEIN("LinearScan.cpp] 2402\n");
        // See if it has been loaded in this basic block
        RegNum reg = this->GetAssignedTempReg(lifetime, regOpnd->GetType());
        if (reg == RegNOREG)
        {LOGMEIN("LinearScan.cpp] 2406\n");
            if (sym->IsConst() && EncoderMD::TryConstFold(instr, regOpnd))
            {LOGMEIN("LinearScan.cpp] 2408\n");
                return;
            }

            reg = this->SecondChanceAllocation(lifetime, false);
            if (reg != RegNOREG)
            {LOGMEIN("LinearScan.cpp] 2414\n");
                IR::Instr *insertInstr = this->TryHoistLoad(instr, lifetime);
                this->InsertLoad(insertInstr, sym, reg);
            }
            else
            {
                // Try folding if there are no registers available
                if (!sym->IsConst() && !this->RegsAvailable(regOpnd->GetType()) && EncoderMD::TryFold(instr, regOpnd))
                {LOGMEIN("LinearScan.cpp] 2422\n");
                    return;
                }

                // We need a reg no matter what.  Try to force second chance to re-allocate this.
                reg = this->SecondChanceAllocation(lifetime, true);

                if (reg == RegNOREG)
                {LOGMEIN("LinearScan.cpp] 2430\n");
                    // Forcing second chance didn't work.
                    // Allocate a new temp reg for it
                    reg = this->FindReg(nullptr, regOpnd);
                    this->AssignTempReg(lifetime, reg);
                }

                this->InsertLoad(instr, sym, reg);
            }
        }
    }
    if (!lifetime->isSpilled && instr->GetNumber() < lifetime->end)
    {LOGMEIN("LinearScan.cpp] 2442\n");
        // Don't border to record the use if this is the last use of the lifetime.
        this->RecordUse(lifetime, instr, regOpnd);
    }
    else
    {
        lifetime->SubFromUseCount(LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr)), this->curLoop);
    }
    this->instrUseRegs.Set(lifetime->reg);

    this->SetReg(regOpnd);
}

// LinearScan::SetReg
void
LinearScan::SetReg(IR::RegOpnd *regOpnd)
{LOGMEIN("LinearScan.cpp] 2458\n");
    if (regOpnd->GetReg() == RegNOREG)
    {LOGMEIN("LinearScan.cpp] 2460\n");
        RegNum reg = regOpnd->m_sym->scratch.linearScan.lifetime->reg;
        AssertMsg(reg != RegNOREG, "Reg should be allocated here...");
        regOpnd->SetReg(reg);
    }
}

bool
LinearScan::SkipNumberedInstr(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 2469\n");
    if (instr->IsLabelInstr())
    {LOGMEIN("LinearScan.cpp] 2471\n");
        if (instr->AsLabelInstr()->m_isLoopTop)
        {LOGMEIN("LinearScan.cpp] 2473\n");
            Assert(instr->GetNumber() != instr->m_next->GetNumber()
                && (instr->GetNumber() != instr->m_prev->GetNumber() || instr->m_prev->m_opcode == Js::OpCode::Nop));
        }
        else
        {
            return true;
        }
    }
    return false;
}

// LinearScan::EndDeadLifetimes
// Look for lifetimes that are ending here, and retire them.
void
LinearScan::EndDeadLifetimes(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 2489\n");
    Lifetime * deadLifetime;

    if (this->SkipNumberedInstr(instr))
    {LOGMEIN("LinearScan.cpp] 2493\n");
        return;
    }

    // Retire all active lifetime ending at this instruction
    while (!this->activeLiveranges->Empty() && this->activeLiveranges->Head()->end <= instr->GetNumber())
    {LOGMEIN("LinearScan.cpp] 2499\n");
        deadLifetime = this->activeLiveranges->Head();
        deadLifetime->defList.Clear();
        deadLifetime->useList.Clear();

        this->activeLiveranges->RemoveHead();
        RegNum reg = deadLifetime->reg;
        this->activeRegs.Clear(reg);
        this->regContent[reg] = nullptr;
        this->secondChanceRegs.Clear(reg);
        if (RegTypes[reg] == TyMachReg)
        {LOGMEIN("LinearScan.cpp] 2510\n");
            this->intRegUsedCount--;
        }
        else
        {
            Assert(RegTypes[reg] == TyFloat64);
            this->floatRegUsedCount--;
        }
    }

    // Look for spilled lifetimes which end here such that we can make their stack slot
    // available for stack-packing.
    while (!this->stackPackInUseLiveRanges->Empty() && this->stackPackInUseLiveRanges->Head()->end <= instr->GetNumber())
    {LOGMEIN("LinearScan.cpp] 2523\n");
        deadLifetime = this->stackPackInUseLiveRanges->Head();
        deadLifetime->defList.Clear();
        deadLifetime->useList.Clear();

        this->stackPackInUseLiveRanges->RemoveHead();
        if (!deadLifetime->cantStackPack)
        {LOGMEIN("LinearScan.cpp] 2530\n");
            Assert(deadLifetime->spillStackSlot);
            deadLifetime->spillStackSlot->lastUse = deadLifetime->end;
            this->stackSlotsFreeList->Push(deadLifetime->spillStackSlot);
        }
    }
}

void
LinearScan::EndDeadOpHelperLifetimes(IR::Instr * instr)
{LOGMEIN("LinearScan.cpp] 2540\n");
    if (this->SkipNumberedInstr(instr))
    {LOGMEIN("LinearScan.cpp] 2542\n");
        return;
    }

    while (!this->opHelperSpilledLiveranges->Empty() &&
           this->opHelperSpilledLiveranges->Head()->end <= instr->GetNumber())
    {LOGMEIN("LinearScan.cpp] 2548\n");
        Lifetime * deadLifetime;

        // The lifetime doesn't extend beyond the helper block
        // No need to save and restore around the helper block
        Assert(this->IsInHelperBlock());

        deadLifetime = this->opHelperSpilledLiveranges->Head();
        this->opHelperSpilledLiveranges->RemoveHead();
        if (!deadLifetime->cantOpHelperSpill)
        {LOGMEIN("LinearScan.cpp] 2558\n");
            this->opHelperSpilledRegs.Clear(deadLifetime->reg);
        }
        deadLifetime->isOpHelperSpilled = false;
        deadLifetime->cantOpHelperSpill = false;
        deadLifetime->isOpHelperSpillAsArg = false;
    }
}

// LinearScan::AllocateNewLifetimes
// Look for lifetimes coming live, and allocate a register for them.
void
LinearScan::AllocateNewLifetimes(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 2571\n");
    if (this->SkipNumberedInstr(instr))
    {LOGMEIN("LinearScan.cpp] 2573\n");
        return;
    }

    // Try to catch:
    //      x = MOV y(r1)
    // where y's lifetime just ended and x's lifetime is starting.
    // If so, set r1 as a preferred register for x, which may allow peeps to remove the MOV
    if (instr->GetSrc1() && instr->GetSrc1()->IsRegOpnd() && LowererMD::IsAssign(instr) && instr->GetDst() && instr->GetDst()->IsRegOpnd() && instr->GetDst()->AsRegOpnd()->m_sym)
    {LOGMEIN("LinearScan.cpp] 2582\n");
        IR::RegOpnd *src = instr->GetSrc1()->AsRegOpnd();
        StackSym *srcSym = src->m_sym;
        // If src is a physReg ref, or src's lifetime ends here.
        if (!srcSym || srcSym->scratch.linearScan.lifetime->end == instr->GetNumber())
        {LOGMEIN("LinearScan.cpp] 2587\n");
            Lifetime *dstLifetime = instr->GetDst()->AsRegOpnd()->m_sym->scratch.linearScan.lifetime;
            if (dstLifetime)
            {LOGMEIN("LinearScan.cpp] 2590\n");
                dstLifetime->regPreference.Set(src->GetReg());
            }
        }
    }

    // Look for starting lifetimes
    while (!this->lifetimeList->Empty() && this->lifetimeList->Head()->start <= instr->GetNumber())
    {LOGMEIN("LinearScan.cpp] 2598\n");
        // We're at the start of a new live range

        Lifetime * newLifetime = this->lifetimeList->Head();
        newLifetime->lastAllocationStart = instr->GetNumber();

        this->lifetimeList->RemoveHead();

        if (newLifetime->dontAllocate)
        {LOGMEIN("LinearScan.cpp] 2607\n");
            // Lifetime spilled before beginning allocation (e.g., a lifetime known to span
            // multiple EH regions.) Do the work of spilling it now without adding it to the list.
            this->SpillLiveRange(newLifetime);
            continue;
        }

        RegNum reg;
        if (newLifetime->reg == RegNOREG)
        {LOGMEIN("LinearScan.cpp] 2616\n");
            if (newLifetime->isDeadStore)
            {LOGMEIN("LinearScan.cpp] 2618\n");
                // No uses, let's not waste a reg.
                newLifetime->isSpilled = true;
                continue;
            }
            reg = this->FindReg(newLifetime, nullptr);
        }
        else
        {
            // This lifetime is already assigned a physical register.  Make
            // sure that register is available by calling SpillReg
            reg = newLifetime->reg;

            // If we're in a helper block, the physical register we're trying to ensure is available might get helper
            // spilled. Don't allow that if this lifetime's end lies beyond the end of the helper block because
            // spill code assumes that this physical register isn't active at the end of the helper block when it tries
            // to restore it. So we'd have to really spill the lifetime then anyway.
            this->SpillReg(reg, IsInHelperBlock() ? (newLifetime->end > currentOpHelperBlock->opHelperEndInstr->GetNumber()) : false);
            newLifetime->cantSpill = true;
        }

        // If we did get a register for this lifetime, add it to the active set.
        if (newLifetime->isSpilled == false)
        {LOGMEIN("LinearScan.cpp] 2641\n");
            this->AssignActiveReg(newLifetime, reg);
        }
    }
}

// LinearScan::FindReg
// Look for an available register.  If one isn't available, spill something.
// Note that the newLifetime passed in could be the one we end up spilling.
RegNum
LinearScan::FindReg(Lifetime *newLifetime, IR::RegOpnd *regOpnd, bool force)
{LOGMEIN("LinearScan.cpp] 2652\n");
    BVIndex regIndex = BVInvalidIndex;
    IRType type;
    bool tryCallerSavedRegs = false;
    BitVector callerSavedAvailableBv;

    if (newLifetime)
    {LOGMEIN("LinearScan.cpp] 2659\n");
        if (newLifetime->isFloat)
        {LOGMEIN("LinearScan.cpp] 2661\n");
            type = TyFloat64;
        }
        else if (newLifetime->isSimd128F4)
        {LOGMEIN("LinearScan.cpp] 2665\n");
            type = TySimd128F4;
        }
        else if (newLifetime->isSimd128I4)
        {LOGMEIN("LinearScan.cpp] 2669\n");
            type = TySimd128I4;
        }
        else if (newLifetime->isSimd128I8)
        {LOGMEIN("LinearScan.cpp] 2673\n");
            type = TySimd128I8;
        }
        else if (newLifetime->isSimd128I16)
        {LOGMEIN("LinearScan.cpp] 2677\n");
            type = TySimd128I16;
        }
        else if (newLifetime->isSimd128U4)
        {LOGMEIN("LinearScan.cpp] 2681\n");
            type = TySimd128U4;
        }
        else if (newLifetime->isSimd128U8)
        {LOGMEIN("LinearScan.cpp] 2685\n");
            type = TySimd128U8;
        }
        else if (newLifetime->isSimd128U16)
        {LOGMEIN("LinearScan.cpp] 2689\n");
            type = TySimd128U16;
        }
        else if (newLifetime->isSimd128B4)
        {LOGMEIN("LinearScan.cpp] 2693\n");
            type = TySimd128B4;
        }
        else if (newLifetime->isSimd128B8)
        {LOGMEIN("LinearScan.cpp] 2697\n");
            type = TySimd128B8;
        }
        else if (newLifetime->isSimd128B16)
        {LOGMEIN("LinearScan.cpp] 2701\n");
            type = TySimd128B16;
        }
        else if (newLifetime->isSimd128D2)
        {LOGMEIN("LinearScan.cpp] 2705\n");
            type = TySimd128D2;
        }
        else
        {
            type = TyMachReg;
        }
    }
    else
    {
        Assert(regOpnd);
        type = regOpnd->GetType();
    }

    if (this->RegsAvailable(type))
    {LOGMEIN("LinearScan.cpp] 2720\n");
        BitVector regsBv;
        regsBv.Copy(this->activeRegs);
        regsBv.Or(this->instrUseRegs);
        regsBv.Or(this->callSetupRegs);
        regsBv.ComplimentAll();

        if (newLifetime)
        {LOGMEIN("LinearScan.cpp] 2728\n");
            if (this->IsInHelperBlock())
            {LOGMEIN("LinearScan.cpp] 2730\n");
                if (newLifetime->end >= this->HelperBlockEndInstrNumber())
                {LOGMEIN("LinearScan.cpp] 2732\n");
                    // this lifetime goes beyond the helper function
                    // We need to exclude the helper spilled register as well.
                    regsBv.Minus(this->opHelperSpilledRegs);
                }
            }

            if (newLifetime->isFloat || newLifetime->isSimd128())
            {LOGMEIN("LinearScan.cpp] 2740\n");
#ifdef _M_IX86
                Assert(AutoSystemInfo::Data.SSE2Available());
#endif
                regsBv.And(this->floatRegs);
            }
            else
            {
                regsBv.And(this->int32Regs);
                regsBv = this->linearScanMD.FilterRegIntSizeConstraints(regsBv, newLifetime->intUsageBv);
            }


            if (newLifetime->isLiveAcrossCalls)
            {LOGMEIN("LinearScan.cpp] 2754\n");
                // Try to find a callee saved regs
                BitVector regsBvTemp = regsBv;
                regsBvTemp.And(this->calleeSavedRegs);

                regIndex = GetPreferencedRegIndex(newLifetime, regsBvTemp);

                if (regIndex == BVInvalidIndex)
                {LOGMEIN("LinearScan.cpp] 2762\n");
                    if (!newLifetime->isLiveAcrossUserCalls)
                    {LOGMEIN("LinearScan.cpp] 2764\n");
                        // No callee saved regs is found and the lifetime only across helper
                        // calls, we can also use a caller saved regs to make use of the
                        // save and restore around helper blocks
                        regIndex = GetPreferencedRegIndex(newLifetime, regsBv);
                    }
                    else
                    {
                        // If we can't find a callee-saved reg, we can try using a caller-saved reg instead.
                        // We'll hopefully get a few loads enregistered that way before we get to the call.
                        tryCallerSavedRegs = true;
                        callerSavedAvailableBv = regsBv;
                    }
                }
            }
            else
            {
                regIndex = GetPreferencedRegIndex(newLifetime, regsBv);
            }
        }
        else
        {
            AssertMsg(regOpnd, "Need a lifetime or a regOpnd passed in");

            if (regOpnd->IsFloat() || regOpnd->IsSimd128())
            {LOGMEIN("LinearScan.cpp] 2789\n");
#ifdef _M_IX86
                Assert(AutoSystemInfo::Data.SSE2Available());
#endif
                regsBv.And(this->floatRegs);
            }
            else
            {
                regsBv.And(this->int32Regs);
                BitVector regSizeBv;
                regSizeBv.ClearAll();
                regSizeBv.Set(TySize[regOpnd->GetType()]);

                regsBv = this->linearScanMD.FilterRegIntSizeConstraints(regsBv, regSizeBv);
            }

            if (!this->tempRegs.IsEmpty())
            {LOGMEIN("LinearScan.cpp] 2806\n");
                // avoid the temp reg that we have loaded in this basic block
                BitVector regsBvTemp = regsBv;
                regsBvTemp.Minus(this->tempRegs);
                regIndex = regsBvTemp.GetPrevBit();
            }

            if (regIndex == BVInvalidIndex)
            {LOGMEIN("LinearScan.cpp] 2814\n");
                // allocate a temp reg from the other end of the bit vector so that it can
                // keep live for longer.
                regIndex = regsBv.GetPrevBit();
            }
        }
    }

    RegNum reg;

    if (BVInvalidIndex != regIndex)
    {LOGMEIN("LinearScan.cpp] 2825\n");
        Assert(regIndex < RegNumCount);
        reg = (RegNum)regIndex;
    }
    else
    {
        if (tryCallerSavedRegs)
        {LOGMEIN("LinearScan.cpp] 2832\n");
            Assert(newLifetime);
            regIndex = GetPreferencedRegIndex(newLifetime, callerSavedAvailableBv);
            if (BVInvalidIndex == regIndex)
            {LOGMEIN("LinearScan.cpp] 2836\n");
                tryCallerSavedRegs = false;
            }
        }

        bool dontSpillCurrent = tryCallerSavedRegs;

        if (newLifetime && newLifetime->isSpilled)
        {LOGMEIN("LinearScan.cpp] 2844\n");
            // Second chance allocation
            dontSpillCurrent = true;
        }

        // Can't find reg, spill some lifetime.
        reg = this->Spill(newLifetime, regOpnd, dontSpillCurrent, force);

        if (reg == RegNOREG && tryCallerSavedRegs)
        {LOGMEIN("LinearScan.cpp] 2853\n");
            Assert(BVInvalidIndex != regIndex);
            reg = (RegNum)regIndex;
            // This lifetime will get spilled once we get to the call it overlaps with (note: this may not be true
            // for second chance allocation as we may be beyond the call).  Mark it as a cheap spill to give up the register
            // if some lifetime not overlapping with a call needs it.
            newLifetime->isCheapSpill = true;
        }
    }

    // We always have to return a reg if we are allocate temp reg.
    // If we are allocating for a new lifetime, we return RegNOREG, if we
    // spill the new lifetime
    Assert(newLifetime != nullptr || (reg != RegNOREG && reg < RegNumCount));
    return reg;
}

BVIndex
LinearScan::GetPreferencedRegIndex(Lifetime *lifetime, BitVector freeRegs)
{LOGMEIN("LinearScan.cpp] 2872\n");
    BitVector freePreferencedRegs = freeRegs;

    freePreferencedRegs.And(lifetime->regPreference);

    // If one of the preferred register (if any) is available, use it.  Otherwise, just pick one of free register.
    if (!freePreferencedRegs.IsEmpty())
    {LOGMEIN("LinearScan.cpp] 2879\n");
        return freePreferencedRegs.GetNextBit();
    }
    else
    {
        return freeRegs.GetNextBit();
    }
}


// LinearScan::Spill
// We need to spill something to free up a reg. If the newLifetime
// past in isn't NULL, we can spill this one instead of an active one.
RegNum
LinearScan::Spill(Lifetime *newLifetime, IR::RegOpnd *regOpnd, bool dontSpillCurrent, bool force)
{LOGMEIN("LinearScan.cpp] 2894\n");
    uint minSpillCost = (uint)-1;

    Assert(!newLifetime || !regOpnd || newLifetime->isFloat == (regOpnd->GetType() == TyMachDouble) || newLifetime->isSimd128() == (regOpnd->IsSimd128()));
    bool isFloatReg;
    BitVector intUsageBV;
    bool needCalleeSaved;

    // For now, we just spill the lifetime with the lowest spill cost.
    if (newLifetime)
    {LOGMEIN("LinearScan.cpp] 2904\n");
        isFloatReg = newLifetime->isFloat || newLifetime->isSimd128();

        if (!force)
        {LOGMEIN("LinearScan.cpp] 2908\n");
            minSpillCost = this->GetSpillCost(newLifetime);
        }
        intUsageBV = newLifetime->intUsageBv;
        needCalleeSaved = newLifetime->isLiveAcrossUserCalls;
    }
    else
    {
        needCalleeSaved = false;
        if (regOpnd->IsFloat() || regOpnd->IsSimd128())
        {LOGMEIN("LinearScan.cpp] 2918\n");
            isFloatReg = true;
        }
        else
        {
            // Filter for int reg size constraints
            isFloatReg = false;
            intUsageBV.ClearAll();
            intUsageBV.Set(TySize[regOpnd->GetType()]);
        }
    }

    SList<Lifetime *>::EditingIterator candidate;
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, this->activeLiveranges, iter)
    {LOGMEIN("LinearScan.cpp] 2932\n");
        uint spillCost = this->GetSpillCost(lifetime);
        if (spillCost < minSpillCost                        &&
            this->instrUseRegs.Test(lifetime->reg) == false &&
            (lifetime->isFloat || lifetime->isSimd128()) == isFloatReg  &&
            !lifetime->cantSpill                            &&
            (!needCalleeSaved || this->calleeSavedRegs.Test(lifetime->reg)) &&
            this->linearScanMD.FitRegIntSizeConstraints(lifetime->reg, intUsageBV))
        {LOGMEIN("LinearScan.cpp] 2940\n");
            minSpillCost = spillCost;
            candidate = iter;
        }
    } NEXT_SLIST_ENTRY_EDITING;
    AssertMsg(newLifetime || candidate.IsValid(), "Didn't find anything to spill?!?");

    Lifetime * spilledRange;
    if (candidate.IsValid())
    {LOGMEIN("LinearScan.cpp] 2949\n");
        spilledRange = candidate.Data();
        candidate.RemoveCurrent();

        this->activeRegs.Clear(spilledRange->reg);
        if (spilledRange->isFloat || spilledRange->isSimd128())
        {LOGMEIN("LinearScan.cpp] 2955\n");
            this->floatRegUsedCount--;
        }
        else
        {
            this->intRegUsedCount--;
        }
    }
    else if (dontSpillCurrent)
    {LOGMEIN("LinearScan.cpp] 2964\n");
        return RegNOREG;
    }
    else
    {
        spilledRange = newLifetime;
    }

    return this->SpillLiveRange(spilledRange);
}

// LinearScan::SpillLiveRange
RegNum
LinearScan::SpillLiveRange(Lifetime * spilledRange, IR::Instr *insertionInstr)
{LOGMEIN("LinearScan.cpp] 2978\n");
    Assert(!spilledRange->isSpilled);

    RegNum reg = spilledRange->reg;
    StackSym *sym = spilledRange->sym;

    spilledRange->isSpilled = true;
    spilledRange->isCheapSpill = false;
    spilledRange->reg = RegNOREG;

    // Don't allocate stack space for const, we always reload them. (For debugm mode, allocate on the stack)
    if (!sym->IsAllocated() && (!sym->IsConst() || IsSymNonTempLocalVar(sym)))
    {LOGMEIN("LinearScan.cpp] 2990\n");
       this->AllocateStackSpace(spilledRange);
    }

    // No need to insert loads or stores if there are no uses.
    if (!spilledRange->isDeadStore)
    {LOGMEIN("LinearScan.cpp] 2996\n");
        // In the debug mode, don't do insertstore for this stacksym, as we want to retain the IsConst for the sym,
        // and later we are going to find the reg for it.
        if (!IsSymNonTempLocalVar(sym))
        {LOGMEIN("LinearScan.cpp] 3000\n");
            this->InsertStores(spilledRange, reg, insertionInstr);
        }

        if (this->IsInLoop() || sym->IsConst())
        {LOGMEIN("LinearScan.cpp] 3005\n");
            this->InsertLoads(sym, reg);
        }
        else
        {
            sym->scratch.linearScan.lifetime->useList.Clear();
        }
        // Adjust useCount in case of second chance allocation
        spilledRange->ApplyUseCountAdjust(this->curLoop);
    }

    Assert(reg == RegNOREG || spilledRange->reg == RegNOREG || this->regContent[reg] == spilledRange);
    if (spilledRange->isSecondChanceAllocated)
    {LOGMEIN("LinearScan.cpp] 3018\n");
        Assert(reg == RegNOREG || spilledRange->reg == RegNOREG
            || (this->regContent[reg] == spilledRange && this->secondChanceRegs.Test(reg)));
        this->secondChanceRegs.Clear(reg);
        spilledRange->isSecondChanceAllocated = false;
    }
    else
    {
        Assert(!this->secondChanceRegs.Test(reg));
    }
    this->regContent[reg] = nullptr;

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 3032\n");
        Output::Print(_u("**** Spill: "));
        sym->Dump();
        Output::Print(_u("(%S)"), RegNames[reg]);
        Output::Print(_u("  SpillCount:%d  Length:%d   Cost:%d\n"),
            spilledRange->useCount, spilledRange->end - spilledRange->start, this->GetSpillCost(spilledRange));
    }
#endif
    return reg;
}

// LinearScan::SpillReg
// Spill a given register.
void
LinearScan::SpillReg(RegNum reg, bool forceSpill /* = false */)
{LOGMEIN("LinearScan.cpp] 3047\n");
    Lifetime *spilledRange = nullptr;
    if (activeRegs.Test(reg))
    {LOGMEIN("LinearScan.cpp] 3050\n");
        spilledRange = LinearScan::RemoveRegLiveRange(activeLiveranges, reg);
    }
    else if (opHelperSpilledRegs.Test(reg) && forceSpill)
    {
        // If a lifetime that was assigned this register was helper spilled,
        // really spill it now.
        Assert(IsInHelperBlock());

        // Look for the liverange in opHelperSpilledLiveranges instead of
        // activeLiveranges.
        FOREACH_SLIST_ENTRY(Lifetime *, lifetime, opHelperSpilledLiveranges)
        {LOGMEIN("LinearScan.cpp] 3062\n");
            if (lifetime->reg == reg)
            {LOGMEIN("LinearScan.cpp] 3064\n");
                spilledRange = lifetime;
                break;
            }
        } NEXT_SLIST_ENTRY;

        Assert(spilledRange);
        Assert(!spilledRange->cantSpill);
        RemoveOpHelperSpilled(spilledRange);
        // Really spill this liverange below.
    }
    else
    {
        return;
    }

    AnalysisAssert(spilledRange);
    Assert(!spilledRange->cantSpill);

    if ((!forceSpill) && this->IsInHelperBlock() && spilledRange->start < this->HelperBlockStartInstrNumber() && !spilledRange->cantOpHelperSpill)
    {LOGMEIN("LinearScan.cpp] 3084\n");
        // if the lifetime starts before the helper block, we can do save and restore
        // around the helper block instead.

        this->AddOpHelperSpilled(spilledRange);
    }
    else
    {
        if (spilledRange->cantOpHelperSpill)
        {LOGMEIN("LinearScan.cpp] 3093\n");
            // We're really spilling this liverange, so take it out of the helper-spilled liveranges
            // to avoid confusion (see Win8 313433).
            Assert(!spilledRange->isOpHelperSpilled);
            spilledRange->cantOpHelperSpill = false;
            this->opHelperSpilledLiveranges->Remove(spilledRange);
        }
        this->SpillLiveRange(spilledRange);
    }

    if (this->activeRegs.Test(reg))
    {LOGMEIN("LinearScan.cpp] 3104\n");
        this->activeRegs.Clear(reg);
        if (RegTypes[reg] == TyMachReg)
        {LOGMEIN("LinearScan.cpp] 3107\n");
            this->intRegUsedCount--;
        }
        else
        {
            Assert(RegTypes[reg] == TyFloat64);
            this->floatRegUsedCount--;
        }
    }
}

void
LinearScan::ProcessEHRegionBoundary(IR::Instr * instr)
{LOGMEIN("LinearScan.cpp] 3120\n");
    Assert(instr->IsBranchInstr());
    Assert(instr->m_opcode != Js::OpCode::TryFinally); // finallys are not supported for optimization yet.
    if (instr->m_opcode != Js::OpCode::TryCatch && instr->m_opcode != Js::OpCode::Leave)
    {LOGMEIN("LinearScan.cpp] 3124\n");
        return;
    }

    // Spill everything upon entry to the try region and upon a Leave.
    IR::Instr* insertionInstr = instr->m_opcode != Js::OpCode::Leave ? instr : instr->m_prev;
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, this->activeLiveranges, iter)
    {LOGMEIN("LinearScan.cpp] 3131\n");
        this->activeRegs.Clear(lifetime->reg);
        if (lifetime->isFloat || lifetime->isSimd128())
        {LOGMEIN("LinearScan.cpp] 3134\n");
            this->floatRegUsedCount--;
        }
        else
        {
            this->intRegUsedCount--;
        }
        this->SpillLiveRange(lifetime, insertionInstr);
        iter.RemoveCurrent();
    }
    NEXT_SLIST_ENTRY_EDITING;
}

void
LinearScan::AllocateStackSpace(Lifetime *spilledRange)
{LOGMEIN("LinearScan.cpp] 3149\n");
    if (spilledRange->sym->IsAllocated())
    {LOGMEIN("LinearScan.cpp] 3151\n");
        return;
    }

    uint32 size = TySize[spilledRange->sym->GetType()];

    // For the bytecodereg syms instead of spilling to the any other location lets re-use the already created slot.
    if (IsSymNonTempLocalVar(spilledRange->sym))
    {LOGMEIN("LinearScan.cpp] 3159\n");
        Js::RegSlot slotIndex = spilledRange->sym->GetByteCodeRegSlot();

        // Get the offset which is already allocated from this local, and always spill on that location.

        spilledRange->sym->m_offset = GetStackOffset(slotIndex);
        spilledRange->sym->m_allocated = true;

        return;
    }

    StackSlot * newStackSlot = nullptr;

    if (!PHASE_OFF(Js::StackPackPhase, this->func) && !this->func->IsJitInDebugMode() && !spilledRange->cantStackPack)
    {
        // Search for a free stack slot to re-use
        FOREACH_SLIST_ENTRY_EDITING(StackSlot *, slot, this->stackSlotsFreeList, iter)
        {LOGMEIN("LinearScan.cpp] 3176\n");
            // Heuristic: should we use '==' or '>=' for the size?
            if (slot->lastUse <= spilledRange->start && slot->size >= size)
            {LOGMEIN("LinearScan.cpp] 3179\n");
                StackSym *spilledSym = spilledRange->sym;

                Assert(!spilledSym->IsArgSlotSym() && !spilledSym->IsParamSlotSym());
                Assert(!spilledSym->IsAllocated());
                spilledRange->spillStackSlot = slot;
                spilledSym->m_offset = slot->offset;
                spilledSym->m_allocated = true;

                iter.RemoveCurrent();

#if DBG_DUMP
                if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::StackPackPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
                {LOGMEIN("LinearScan.cpp] 3192\n");
                    spilledSym->Dump();
                    Output::Print(_u(" *** stack packed at offset %3d  (%4d - %4d)\n"), spilledSym->m_offset, spilledRange->start, spilledRange->end);
                }
#endif
                break;
            }
        } NEXT_SLIST_ENTRY_EDITING;

        if (spilledRange->spillStackSlot == nullptr)
        {LOGMEIN("LinearScan.cpp] 3202\n");
            newStackSlot = JitAnewStruct(this->tempAlloc, StackSlot);
            newStackSlot->size = size;
            spilledRange->spillStackSlot = newStackSlot;
        }
        this->AddLiveRange(this->stackPackInUseLiveRanges, spilledRange);
    }

    if (!spilledRange->sym->IsAllocated())
    {LOGMEIN("LinearScan.cpp] 3211\n");
        // Can't stack pack, allocate new stack slot.
        StackSym *spilledSym = spilledRange->sym;
        this->func->StackAllocate(spilledSym, size);

#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::StackPackPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {LOGMEIN("LinearScan.cpp] 3218\n");
            spilledSym->Dump();
            Output::Print(_u(" at offset %3d  (%4d - %4d)\n"), spilledSym->m_offset, spilledRange->start, spilledRange->end);
        }
#endif
        if (newStackSlot != nullptr)
        {LOGMEIN("LinearScan.cpp] 3224\n");
            newStackSlot->offset = spilledSym->m_offset;
        }
    }
}

// LinearScan::InsertLoads
void
LinearScan::InsertLoads(StackSym *sym, RegNum reg)
{LOGMEIN("LinearScan.cpp] 3233\n");
    Lifetime *lifetime = sym->scratch.linearScan.lifetime;

    FOREACH_SLIST_ENTRY(IR::Instr *, instr, &lifetime->useList)
    {LOGMEIN("LinearScan.cpp] 3237\n");
        this->InsertLoad(instr, sym, reg);
    } NEXT_SLIST_ENTRY;

    lifetime->useList.Clear();
}

// LinearScan::InsertStores
void
LinearScan::InsertStores(Lifetime *lifetime, RegNum reg, IR::Instr *insertionInstr)
{LOGMEIN("LinearScan.cpp] 3247\n");
    StackSym *sym = lifetime->sym;

    // If single def, use instrDef on the symbol
    if (sym->m_isSingleDef)
    {LOGMEIN("LinearScan.cpp] 3252\n");
        IR::Instr * defInstr = sym->m_instrDef;
        if ((!sym->IsConst() && defInstr->GetDst()->AsRegOpnd()->GetReg() == RegNOREG)
            || this->secondChanceRegs.Test(reg))
        {LOGMEIN("LinearScan.cpp] 3256\n");
            // This can happen if we were trying to allocate this lifetime,
            // and it is getting spilled right away.
            // For second chance allocations, this should have already been handled.

            return;
        }
        this->InsertStore(defInstr, defInstr->FindRegDef(sym)->m_sym, reg);
        return;
    }

    if (reg == RegNOREG)
    {LOGMEIN("LinearScan.cpp] 3268\n");
        return;
    }

    uint localStoreCost = LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr));

    // Is it cheaper to spill all the defs we've seen so far or just insert a store at the current point?
    if ((this->func->HasTry() && !this->func->DoOptimizeTryCatch()) || localStoreCost >= lifetime->allDefsCost)
    {LOGMEIN("LinearScan.cpp] 3276\n");
        // Insert a store for each def point we've seen so far
        FOREACH_SLIST_ENTRY(IR::Instr *, instr, &(lifetime->defList))
        {LOGMEIN("LinearScan.cpp] 3279\n");
            if (instr->GetDst()->AsRegOpnd()->GetReg() != RegNOREG)
            {LOGMEIN("LinearScan.cpp] 3281\n");
                IR::RegOpnd *regOpnd = instr->FindRegDef(sym);

                // Note that reg may not be equal to regOpnd->GetReg() if the lifetime has been re-allocated since we've seen this def
                this->InsertStore(instr, regOpnd->m_sym, regOpnd->GetReg());
            }

        } NEXT_SLIST_ENTRY;

        lifetime->defList.Clear();
        lifetime->allDefsCost = 0;
        lifetime->needsStoreCompensation = false;
    }
    else if (!lifetime->defList.Empty())
    {LOGMEIN("LinearScan.cpp] 3295\n");
        // Insert a def right here at the current instr, and then we'll use compensation code for paths not covered by this def.
        if (!insertionInstr)
        {LOGMEIN("LinearScan.cpp] 3298\n");
            insertionInstr = this->currentInstr->m_prev;
        }

        this->InsertStore(insertionInstr, sym, reg);
        if (this->IsInLoop())
        {
            RecordLoopUse(lifetime, reg);
        }
        // We now need to insert all store compensations when needed, unless we spill all the defs later on.
        lifetime->needsStoreCompensation = true;
    }
}

// LinearScan::InsertStore
void
LinearScan::InsertStore(IR::Instr *instr, StackSym *sym, RegNum reg)
{LOGMEIN("LinearScan.cpp] 3315\n");
    // Win8 Bug 391484: We cannot use regOpnd->GetType() here because it
    // can lead to truncation as downstream usage of the register might be of a size
    // greater than the current use. Using RegTypes[reg] works only if the stack slot size
    // is always at least of size MachPtr

    // In the debug mode, if the current sym belongs to the byte code locals, then do not unlink this instruction, as we need to have this instruction to be there
    // to produce the write-through instruction.
    if (sym->IsConst() && !IsSymNonTempLocalVar(sym))
    {LOGMEIN("LinearScan.cpp] 3324\n");
        // Let's just delete the def.  We'll reload the constant.
        // We can't just delete the instruction however since the
        // uses will look at the def to get the value.

        // Make sure it wasn't already deleted.
        if (sym->m_instrDef->m_next)
        {LOGMEIN("LinearScan.cpp] 3331\n");
            sym->m_instrDef->Unlink();
            sym->m_instrDef->m_next = nullptr;
        }
        return;
    }

    Assert(reg != RegNOREG);

    IRType type = sym->GetType();

    if (sym->IsSimd128())
    {LOGMEIN("LinearScan.cpp] 3343\n");
        type = sym->GetType();
    }

    IR::Instr *store = IR::Instr::New(LowererMD::GetStoreOp(type),
        IR::SymOpnd::New(sym, type, this->func),
        IR::RegOpnd::New(sym, reg, type, this->func), this->func);
    instr->InsertAfter(store);
    store->CopyNumber(instr);
    this->linearScanMD.LegalizeDef(store);

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 3356\n");
        Output::Print(_u("...Inserting store for "));
        sym->Dump();
        Output::Print(_u("  Cost:%d\n"), this->GetSpillCost(sym->scratch.linearScan.lifetime));
    }
#endif
}

// LinearScan::InsertLoad
void
LinearScan::InsertLoad(IR::Instr *instr, StackSym *sym, RegNum reg)
{LOGMEIN("LinearScan.cpp] 3367\n");
    IR::Opnd *src;
    // The size of loads and stores to memory need to match. See the comment
    // around type in InsertStore above.
    IRType type = sym->GetType();

    if (sym->IsSimd128())
    {LOGMEIN("LinearScan.cpp] 3374\n");
        type = sym->GetType();
    }

    bool isMovSDZero = false;
    if (sym->IsConst())
    {LOGMEIN("LinearScan.cpp] 3380\n");
        Assert(!sym->IsAllocated() || IsSymNonTempLocalVar(sym));
        // For an intConst, reload the constant instead of using the stack.
        // Create a new StackSym to make sure the old sym remains singleDef
        src = sym->GetConstOpnd();
        if (!src)
        {LOGMEIN("LinearScan.cpp] 3386\n");
            isMovSDZero = true;
            sym = StackSym::New(sym->GetType(), this->func);
            sym->m_isConst = true;
            sym->m_isFltConst = true;
        }
        else
        {
            StackSym * oldSym = sym;
            sym = StackSym::New(TyVar, this->func);
            sym->m_isConst = true;
            sym->m_isIntConst = oldSym->m_isIntConst;
            sym->m_isInt64Const = oldSym->m_isInt64Const;
            sym->m_isTaggableIntConst = sym->m_isTaggableIntConst;
        }
    }
    else
    {
        src = IR::SymOpnd::New(sym, type, this->func);
    }
    IR::Instr * load;
#if defined(_M_IX86) || defined(_M_X64)
    if (isMovSDZero)
    {LOGMEIN("LinearScan.cpp] 3409\n");
        load = IR::Instr::New(Js::OpCode::MOVSD_ZERO,
            IR::RegOpnd::New(sym, reg, type, this->func), this->func);
        instr->InsertBefore(load);
    }
    else
#endif
    {
        load = Lowerer::InsertMove(IR::RegOpnd::New(sym, reg, type, this->func), src, instr);
    }
    load->CopyNumber(instr);
    if (!isMovSDZero)
    {LOGMEIN("LinearScan.cpp] 3421\n");
        this->linearScanMD.LegalizeUse(load, src);
    }

    this->RecordLoopUse(nullptr, reg);

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 3429\n");
        Output::Print(_u("...Inserting load for "));
        sym->Dump();
        if (sym->scratch.linearScan.lifetime)
        {LOGMEIN("LinearScan.cpp] 3433\n");
            Output::Print(_u("  Cost:%d\n"), this->GetSpillCost(sym->scratch.linearScan.lifetime));
        }
        else
        {
            Output::Print(_u("\n"));
        }
    }
#endif
}

uint8
LinearScan::GetRegAttribs(RegNum reg)
{LOGMEIN("LinearScan.cpp] 3446\n");
    return RegAttribs[reg];
}

IRType
LinearScan::GetRegType(RegNum reg)
{LOGMEIN("LinearScan.cpp] 3452\n");
    return RegTypes[reg];
}

bool
LinearScan::IsCalleeSaved(RegNum reg)
{LOGMEIN("LinearScan.cpp] 3458\n");
    return (RegAttribs[reg] & RA_CALLEESAVE) != 0;
}

bool
LinearScan::IsCallerSaved(RegNum reg) const
{LOGMEIN("LinearScan.cpp] 3464\n");
    return !LinearScan::IsCalleeSaved(reg) && LinearScan::IsAllocatable(reg);
}

bool
LinearScan::IsAllocatable(RegNum reg) const
{LOGMEIN("LinearScan.cpp] 3470\n");
    return !(RegAttribs[reg] & RA_DONTALLOCATE) && this->linearScanMD.IsAllocatable(reg, this->func);
}

void
LinearScan::KillImplicitRegs(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 3476\n");
    if (instr->IsLabelInstr() || instr->IsBranchInstr())
    {LOGMEIN("LinearScan.cpp] 3478\n");
        // Note: need to clear these for branch as well because this info isn't recorded for second chance
        //       allocation on branch boundaries
        this->tempRegs.ClearAll();
    }

#if defined(_M_IX86) || defined(_M_X64)
    if (instr->m_opcode == Js::OpCode::IMUL)
    {LOGMEIN("LinearScan.cpp] 3486\n");
        this->SpillReg(LowererMDArch::GetRegIMulHighDestLower());
        this->tempRegs.Clear(LowererMDArch::GetRegIMulHighDestLower());

        this->RecordLoopUse(nullptr, LowererMDArch::GetRegIMulHighDestLower());
        return;
    }
#endif

    this->TrackInlineeArgLifetimes(instr);

    // Don't care about kills on bailout calls as we are going to exit anyways
    // Also, for bailout scenarios we have already handled the inlinee frame spills
    Assert(LowererMD::IsCall(instr) || !instr->HasBailOutInfo());
    if (!LowererMD::IsCall(instr) || instr->HasBailOutInfo())
    {LOGMEIN("LinearScan.cpp] 3501\n");
        return;
    }

    if (this->currentBlock->inlineeStack.Count() > 0)
    {LOGMEIN("LinearScan.cpp] 3506\n");
        this->SpillInlineeArgs(instr);
    }
    else
    {
        instr->m_func = this->func;
    }

    //
    // Spill caller-saved registers that are active.
    //
    BitVector deadRegs;
    deadRegs.Copy(this->activeRegs);
    deadRegs.And(this->callerSavedRegs);
    FOREACH_BITSET_IN_UNITBV(reg, deadRegs, BitVector)
    {LOGMEIN("LinearScan.cpp] 3521\n");
        this->SpillReg((RegNum)reg);
    }
    NEXT_BITSET_IN_UNITBV;
    this->tempRegs.And(this->calleeSavedRegs);

    if (callSetupRegs.Count())
    {LOGMEIN("LinearScan.cpp] 3528\n");
        callSetupRegs.ClearAll();
    }
    Loop *loop = this->curLoop;
    while (loop)
    {LOGMEIN("LinearScan.cpp] 3533\n");
        loop->regAlloc.regUseBv.Or(this->callerSavedRegs);
        loop = loop->parent;
    }
}

//
// Before a call, all inlinee frame syms need to be spilled to a pre-defined location
//
void LinearScan::SpillInlineeArgs(IR::Instr* instr)
{LOGMEIN("LinearScan.cpp] 3543\n");
    Assert(this->currentBlock->inlineeStack.Count() > 0);

    // Ensure the call instruction is tied to the current inlinee
    // This is used in the encoder to encode mapping or return offset and InlineeFrameRecord
    instr->m_func = this->currentBlock->inlineeStack.Last();

    BitVector spilledRegs;
    this->currentBlock->inlineeFrameLifetimes.Map([&](uint i, Lifetime* lifetime){
        Assert(lifetime->start < instr->GetNumber() && lifetime->end >= instr->GetNumber());
        Assert(!lifetime->sym->IsConst());
        Assert(this->currentBlock->inlineeFrameSyms.ContainsKey(lifetime->sym->m_id));
        if (lifetime->reg == RegNOREG)
        {LOGMEIN("LinearScan.cpp] 3556\n");
            return;
        }

        StackSym* sym = lifetime->sym;
        if (!lifetime->isSpilled && !lifetime->isOpHelperSpilled &&
            (!lifetime->isDeadStore && (lifetime->sym->m_isSingleDef || !lifetime->defList.Empty()))) // if deflist is empty - we have already spilled at all defs - and the value is current
        {LOGMEIN("LinearScan.cpp] 3563\n");
            if (!spilledRegs.Test(lifetime->reg))
            {LOGMEIN("LinearScan.cpp] 3565\n");
                spilledRegs.Set(lifetime->reg);
                if (!sym->IsAllocated())
                {LOGMEIN("LinearScan.cpp] 3568\n");
                    this->AllocateStackSpace(lifetime);
                }

                this->RecordLoopUse(lifetime, lifetime->reg);
                Assert(this->regContent[lifetime->reg] != nullptr);
                if (sym->m_isSingleDef)
                {LOGMEIN("LinearScan.cpp] 3575\n");
                    // For a single def - we do not track the deflist - the def below will remove the single def on the sym
                    // hence, we need to track the original def.
                    Assert(lifetime->defList.Empty());
                    lifetime->defList.Prepend(sym->m_instrDef);
                }

                this->InsertStore(instr->m_prev, sym, lifetime->reg);
            }
        }
    });
}

void LinearScan::TrackInlineeArgLifetimes(IR::Instr* instr)
{LOGMEIN("LinearScan.cpp] 3589\n");
    if (instr->m_opcode == Js::OpCode::InlineeStart)
    {LOGMEIN("LinearScan.cpp] 3591\n");
        if (instr->m_func->m_hasInlineArgsOpt)
        {LOGMEIN("LinearScan.cpp] 3593\n");
            instr->m_func->frameInfo->IterateSyms([=](StackSym* sym){
                Lifetime* lifetime = sym->scratch.linearScan.lifetime;
                this->currentBlock->inlineeFrameLifetimes.Add(lifetime);

                // We need to maintain as count because the same sym can be used for multiple arguments
                uint* value;
                if (this->currentBlock->inlineeFrameSyms.TryGetReference(sym->m_id, &value))
                {LOGMEIN("LinearScan.cpp] 3601\n");
                    *value = *value + 1;
                }
                else
                {
                    this->currentBlock->inlineeFrameSyms.Add(sym->m_id, 1);
                }
            });
            if (this->currentBlock->inlineeStack.Count() > 0)
            {LOGMEIN("LinearScan.cpp] 3610\n");
                Assert(instr->m_func->inlineDepth == this->currentBlock->inlineeStack.Last()->inlineDepth + 1);
            }
            this->currentBlock->inlineeStack.Add(instr->m_func);
        }
        else
        {
            Assert(this->currentBlock->inlineeStack.Count() == 0);
        }
    }
    else if (instr->m_opcode == Js::OpCode::InlineeEnd)
    {LOGMEIN("LinearScan.cpp] 3621\n");
        if (instr->m_func->m_hasInlineArgsOpt)
        {LOGMEIN("LinearScan.cpp] 3623\n");
            instr->m_func->frameInfo->AllocateRecord(this->func, instr->m_func->GetJITFunctionBody()->GetAddr());

            if(this->currentBlock->inlineeStack.Count() == 0)
            {LOGMEIN("LinearScan.cpp] 3627\n");
                // Block is unreachable
                Assert(this->currentBlock->inlineeFrameLifetimes.Count() == 0);
                Assert(this->currentBlock->inlineeFrameSyms.Count() == 0);
            }
            else
            {
                Func* func = this->currentBlock->inlineeStack.RemoveAtEnd();
                Assert(func == instr->m_func);

                instr->m_func->frameInfo->IterateSyms([=](StackSym* sym){
                    Lifetime* lifetime = this->currentBlock->inlineeFrameLifetimes.RemoveAtEnd();

                    uint* value;
                    if (this->currentBlock->inlineeFrameSyms.TryGetReference(sym->m_id, &value))
                    {LOGMEIN("LinearScan.cpp] 3642\n");
                        *value = *value - 1;
                        if (*value == 0)
                        {LOGMEIN("LinearScan.cpp] 3645\n");
                            bool removed = this->currentBlock->inlineeFrameSyms.Remove(sym->m_id);
                            Assert(removed);
                        }
                    }
                    else
                    {
                        Assert(UNREACHED);
                    }
                    Assert(sym->scratch.linearScan.lifetime == lifetime);
                }, /*reverse*/ true);
            }
        }
    }
}

// GetSpillCost
// The spill cost is trying to estimate the usage density of the lifetime,
// by dividing the useCount by the lifetime length.
uint
LinearScan::GetSpillCost(Lifetime *lifetime)
{LOGMEIN("LinearScan.cpp] 3666\n");
    uint useCount = lifetime->GetRegionUseCount(this->curLoop);
    uint spillCost;
    // Get local spill cost.  Ignore helper blocks as we'll also need compensation on the main path.
    uint localUseCost = LinearScan::GetUseSpillCost(this->loopNest, false);

    if (lifetime->reg && !lifetime->isSpilled)
    {LOGMEIN("LinearScan.cpp] 3673\n");
        // If it is in a reg, we'll need a store
        if (localUseCost >= lifetime->allDefsCost)
        {LOGMEIN("LinearScan.cpp] 3676\n");
            useCount += lifetime->allDefsCost;
        }
        else
        {
            useCount += localUseCost;
        }

        if (this->curLoop && !lifetime->sym->IsConst()
            && this->curLoop->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id))
        {LOGMEIN("LinearScan.cpp] 3686\n");
            // If we spill here, we'll need to insert a load at the bottom of the loop
            // (it would be nice to be able to check is was in a reg at the top of the loop)...
            useCount += localUseCost;
        }
    }

    // When comparing 2 lifetimes, we don't really care about the actual length of the lifetimes.
    // What matters is how much longer will they use the register.
    const uint start = currentInstr->GetNumber();
    uint end = max(start, lifetime->end);
    uint lifetimeTotalOpHelperFullVisitedLength = lifetime->totalOpHelperLengthByEnd;

    if (this->curLoop && this->curLoop->regAlloc.loopEnd < end && !PHASE_OFF(Js::RegionUseCountPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 3700\n");
        end = this->curLoop->regAlloc.loopEnd;
        lifetimeTotalOpHelperFullVisitedLength  = this->curLoop->regAlloc.helperLength;
    }
    uint length = end - start + 1;

    // Exclude helper block length since helper block paths are typically infrequently taken paths and not as important
    const uint totalOpHelperVisitedLength = this->totalOpHelperFullVisitedLength + CurrentOpHelperVisitedLength(currentInstr);
    Assert(lifetimeTotalOpHelperFullVisitedLength >= totalOpHelperVisitedLength);
    const uint lifetimeHelperLength = lifetimeTotalOpHelperFullVisitedLength - totalOpHelperVisitedLength;
    Assert(length >= lifetimeHelperLength);
    length -= lifetimeHelperLength;
    if(length == 0)
    {LOGMEIN("LinearScan.cpp] 3713\n");
        length = 1;
    }

    // Add a base length so that the difference between a length of 1 and a length of 2 is not so large
#ifdef _M_X64
    length += 64;
#else
    length += 16;
#endif

    spillCost = (useCount << 13) / length;

    if (lifetime->isSecondChanceAllocated)
    {LOGMEIN("LinearScan.cpp] 3727\n");
        // Second chance allocation have additional overhead, so de-prioritize them
        // Note: could use more tuning...
        spillCost = spillCost * 4/5;
    }
    if (lifetime->isCheapSpill)
    {LOGMEIN("LinearScan.cpp] 3733\n");
        // This lifetime will get spilled eventually, so lower the spill cost to favor other lifetimes
        // Note: could use more tuning...
        spillCost /= 2;
    }

    if (lifetime->sym->IsConst())
    {LOGMEIN("LinearScan.cpp] 3740\n");
        spillCost = spillCost / 16;
    }

    return spillCost;
}

bool
LinearScan::RemoveDeadStores(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 3749\n");
    IR::Opnd *dst = instr->GetDst();

    if (dst && dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym && !dst->AsRegOpnd()->m_isCallArg)
    {LOGMEIN("LinearScan.cpp] 3753\n");
        IR::RegOpnd *regOpnd = dst->AsRegOpnd();
        Lifetime * lifetime = regOpnd->m_sym->scratch.linearScan.lifetime;

        if (lifetime->isDeadStore)
        {LOGMEIN("LinearScan.cpp] 3758\n");
            if (Lowerer::HasSideEffects(instr) == false)
            {LOGMEIN("LinearScan.cpp] 3760\n");
                // If all the bailouts referencing this arg are removed (which can happen in some scenarios)
                //- then it's OK to remove this def of the arg
                DebugOnly(this->func->allowRemoveBailOutArgInstr = true);

                // We are removing this instruction, end dead life time now
                this->EndDeadLifetimes(instr);
                instr->Remove();

                DebugOnly(this->func->allowRemoveBailOutArgInstr = false);
                return true;
            }
        }
    }

    return false;
}

void
LinearScan::AssignActiveReg(Lifetime * lifetime, RegNum reg)
{LOGMEIN("LinearScan.cpp] 3780\n");
    Assert(!this->activeRegs.Test(reg));
    Assert(!lifetime->isSpilled);
    Assert(lifetime->reg == RegNOREG || lifetime->reg == reg);
    this->func->m_regsUsed.Set(reg);
    lifetime->reg = reg;
    this->activeRegs.Set(reg);
    if (lifetime->isFloat || lifetime->isSimd128())
    {LOGMEIN("LinearScan.cpp] 3788\n");
        this->floatRegUsedCount++;
    }
    else
    {
        this->intRegUsedCount++;
    }
    this->AddToActive(lifetime);

    this->tempRegs.Clear(reg);
}
void
LinearScan::AssignTempReg(Lifetime * lifetime, RegNum reg)
{LOGMEIN("LinearScan.cpp] 3801\n");
    Assert(reg > RegNOREG && reg < RegNumCount);
    Assert(!this->activeRegs.Test(reg));
    Assert(lifetime->isSpilled);
    this->func->m_regsUsed.Set(reg);
    lifetime->reg = reg;
    this->tempRegs.Set(reg);
    __analysis_assume(reg > 0 && reg < RegNumCount);
    this->tempRegLifetimes[reg] = lifetime;

    this->RecordLoopUse(nullptr, reg);
}

RegNum
LinearScan::GetAssignedTempReg(Lifetime * lifetime, IRType type)
{LOGMEIN("LinearScan.cpp] 3816\n");
    if (this->tempRegs.Test(lifetime->reg) && this->tempRegLifetimes[lifetime->reg] == lifetime)
    {LOGMEIN("LinearScan.cpp] 3818\n");
        if (this->linearScanMD.FitRegIntSizeConstraints(lifetime->reg, type))
        {LOGMEIN("LinearScan.cpp] 3820\n");
            this->RecordLoopUse(nullptr, lifetime->reg);
            return lifetime->reg;
        }
        else
        {
            // Free this temp, we'll need to find another one.
            this->tempRegs.Clear(lifetime->reg);
            lifetime->reg = RegNOREG;
        }
    }
    return RegNOREG;
}

uint
LinearScan::GetUseSpillCost(uint loopNest, BOOL isInHelperBlock)
{LOGMEIN("LinearScan.cpp] 3836\n");
    if (isInHelperBlock)
    {LOGMEIN("LinearScan.cpp] 3838\n");
        // Helper block uses are not as important.
        return 0;
    }
    else if (loopNest < 6)
    {LOGMEIN("LinearScan.cpp] 3843\n");
        return (1 << (loopNest * 3));
    }
    else
    {
        // Slow growth for deep nest to avoid overflow
        return (1 << (5 * 3)) * (loopNest-5);
    }
}

void
LinearScan::ProcessSecondChanceBoundary(IR::BranchInstr *branchInstr)
{LOGMEIN("LinearScan.cpp] 3855\n");
    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {LOGMEIN("LinearScan.cpp] 3857\n");
        return;
    }

    if (this->currentOpHelperBlock && this->currentOpHelperBlock->opHelperEndInstr == branchInstr)
    {
        // Lifetimes opHelperSpilled won't get recorded by SaveRegContent().  Do it here.
        FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->opHelperSpilledLiveranges)
        {LOGMEIN("LinearScan.cpp] 3865\n");
            if (!lifetime->cantOpHelperSpill)
            {LOGMEIN("LinearScan.cpp] 3867\n");
                if (lifetime->isSecondChanceAllocated)
                {LOGMEIN("LinearScan.cpp] 3869\n");
                    this->secondChanceRegs.Set(lifetime->reg);
                }
                this->regContent[lifetime->reg] = lifetime;
            }
        } NEXT_SLIST_ENTRY;
    }

    if(branchInstr->IsMultiBranch())
    {LOGMEIN("LinearScan.cpp] 3878\n");
        IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

        multiBranchInstr->MapUniqueMultiBrLabels([=](IR::LabelInstr * branchLabel) -> void
        {
            this->ProcessSecondChanceBoundaryHelper(branchInstr, branchLabel);
        });
    }
    else
    {
        IR::LabelInstr *branchLabel = branchInstr->GetTarget();
        this->ProcessSecondChanceBoundaryHelper(branchInstr, branchLabel);
    }

    this->SaveRegContent(branchInstr);
}

void
LinearScan::ProcessSecondChanceBoundaryHelper(IR::BranchInstr *branchInstr, IR::LabelInstr *branchLabel)
{LOGMEIN("LinearScan.cpp] 3897\n");
    if (branchInstr->GetNumber() > branchLabel->GetNumber())
    {LOGMEIN("LinearScan.cpp] 3899\n");
        // Loop back-edge
        Assert(branchLabel->m_isLoopTop);
        branchInstr->m_regContent = nullptr;
        this->InsertSecondChanceCompensation(this->regContent, branchLabel->m_regContent, branchInstr, branchLabel);
    }
    else
    {
        // Forward branch
        this->SaveRegContent(branchInstr);
        if (this->curLoop)
        {LOGMEIN("LinearScan.cpp] 3910\n");
            this->curLoop->regAlloc.exitRegContentList->Prepend(branchInstr->m_regContent);
        }
        if (!branchLabel->m_loweredBasicBlock)
        {LOGMEIN("LinearScan.cpp] 3914\n");
            if (branchInstr->IsConditional() || branchInstr->IsMultiBranch())
            {LOGMEIN("LinearScan.cpp] 3916\n");
                // Clone with deep copy
                branchLabel->m_loweredBasicBlock = this->currentBlock->Clone(this->tempAlloc);
            }
            else
            {
                // If the unconditional branch leads to the end of the function for the scenario of a bailout - we do not want to
                // copy the lowered inlinee info.
                IR::Instr* nextInstr = branchLabel->GetNextRealInstr();
                if (nextInstr->m_opcode != Js::OpCode::FunctionExit &&
                    nextInstr->m_opcode != Js::OpCode::BailOutStackRestore &&
                    this->currentBlock->HasData())
                {LOGMEIN("LinearScan.cpp] 3928\n");
                    // Clone with shallow copy
                    branchLabel->m_loweredBasicBlock = this->currentBlock;

                }
            }
        }
        else
        {
            // The lowerer sometimes generates unreachable blocks that would have empty data.
            Assert(!currentBlock->HasData() || branchLabel->m_loweredBasicBlock->Equals(this->currentBlock));
        }
    }
}

void
LinearScan::ProcessSecondChanceBoundary(IR::LabelInstr *labelInstr)
{LOGMEIN("LinearScan.cpp] 3945\n");
    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {LOGMEIN("LinearScan.cpp] 3947\n");
        return;
    }

    if (labelInstr->m_isLoopTop)
    {LOGMEIN("LinearScan.cpp] 3952\n");
        this->SaveRegContent(labelInstr);
        Lifetime ** regContent = AnewArrayZ(this->tempAlloc, Lifetime *, RegNumCount);
        js_memcpy_s(regContent, (RegNumCount * sizeof(Lifetime *)), this->regContent, sizeof(this->regContent));
        this->curLoop->regAlloc.loopTopRegContent = regContent;
    }

    FOREACH_SLISTCOUNTED_ENTRY_EDITING(IR::BranchInstr *, branchInstr, &labelInstr->labelRefs, iter)
    {LOGMEIN("LinearScan.cpp] 3960\n");
        if (branchInstr->m_isAirlock)
        {LOGMEIN("LinearScan.cpp] 3962\n");
            // This branch was just inserted... Skip it.
            continue;
        }

        Assert(branchInstr->GetNumber() && labelInstr->GetNumber());
        if (branchInstr->GetNumber() < labelInstr->GetNumber())
        {LOGMEIN("LinearScan.cpp] 3969\n");
            // Normal branch
            this->InsertSecondChanceCompensation(branchInstr->m_regContent, this->regContent, branchInstr, labelInstr);
        }
        else
        {
            // Loop back-edge
            Assert(labelInstr->m_isLoopTop);
        }
    } NEXT_SLISTCOUNTED_ENTRY_EDITING;
}

IR::Instr * LinearScan::EnsureAirlock(bool needsAirlock, bool *pHasAirlock, IR::Instr *insertionInstr,
                                      IR::Instr **pInsertionStartInstr, IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr)
{LOGMEIN("LinearScan.cpp] 3983\n");
    if (needsAirlock && !(*pHasAirlock))
    {LOGMEIN("LinearScan.cpp] 3985\n");
        // We need an extra block for the compensation code.
        insertionInstr = this->InsertAirlock(branchInstr, labelInstr);
        *pInsertionStartInstr = insertionInstr->m_prev;
        *pHasAirlock = true;
    }
    return insertionInstr;
}

bool LinearScan::NeedsLoopBackEdgeCompensation(Lifetime *lifetime, IR::LabelInstr *loopTopLabel)
{LOGMEIN("LinearScan.cpp] 3995\n");
    if (!lifetime)
    {LOGMEIN("LinearScan.cpp] 3997\n");
        return false;
    }

    if (lifetime->sym->IsConst())
    {LOGMEIN("LinearScan.cpp] 4002\n");
        return false;
    }

    // No need if lifetime begins in the loop
    if (lifetime->start > loopTopLabel->GetNumber())
    {LOGMEIN("LinearScan.cpp] 4008\n");
        return false;
    }

    // Only needed if lifetime is live on the back-edge, and the register is used inside the loop, or the lifetime extends
    // beyond the loop (and compensation out of the loop may use this reg)...
    if (!loopTopLabel->GetLoop()->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id)
        || (this->currentInstr->GetNumber() >= lifetime->end && !this->curLoop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id)))
    {LOGMEIN("LinearScan.cpp] 4016\n");
        return false;
    }

    return true;
}

void
LinearScan::InsertSecondChanceCompensation(Lifetime ** branchRegContent, Lifetime **labelRegContent,
                                           IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr)
{LOGMEIN("LinearScan.cpp] 4026\n");
    IR::Instr *prevInstr = branchInstr->GetPrevRealInstrOrLabel();
    bool needsAirlock = branchInstr->IsConditional() || (prevInstr->IsBranchInstr() && prevInstr->AsBranchInstr()->IsConditional()) || branchInstr->IsMultiBranch();
    bool hasAirlock = false;
    IR::Instr *insertionInstr = branchInstr;
    IR::Instr *insertionStartInstr = branchInstr->m_prev;
    // For loop back-edge, we want to keep the insertionStartInstr before the branch as spill need to happen on all paths
    // Pass a dummy instr address to airLockBlock insertion code.
    BitVector thrashedRegs(0);
    bool isLoopBackEdge = (this->regContent == branchRegContent);
    Lifetime * tmpRegContent[RegNumCount];
    Lifetime **regContent = this->regContent;

    if (isLoopBackEdge)
    {LOGMEIN("LinearScan.cpp] 4040\n");
        Loop *loop = labelInstr->GetLoop();

        js_memcpy_s(&tmpRegContent, (RegNumCount * sizeof(Lifetime *)), this->regContent, sizeof(this->regContent));

        branchRegContent = tmpRegContent;
        regContent = tmpRegContent;

#if defined(_M_IX86) || defined(_M_X64)
        // Insert XCHG to avoid some conflicts for int regs
        // Note: no XCHG on ARM or SSE2.  We could however use 3 XOR on ARM...
        this->AvoidCompensationConflicts(labelInstr, branchInstr, labelRegContent, branchRegContent,
                                         &insertionInstr, &insertionStartInstr, needsAirlock, &hasAirlock);
#endif


        FOREACH_BITSET_IN_UNITBV(reg, this->secondChanceRegs, BitVector)
        {LOGMEIN("LinearScan.cpp] 4057\n");
            Lifetime *labelLifetime = labelRegContent[reg];
            Lifetime *lifetime = branchRegContent[reg];

            // 1.  Insert Stores
            //          Lifetime starts before the loop
            //          Lifetime was re-allocated within the loop (i.e.: a load was most likely inserted)
            //          Lifetime is live on back-edge and has unsaved defs.

            if (lifetime && lifetime->start < labelInstr->GetNumber() && lifetime->lastAllocationStart > labelInstr->GetNumber()
                && (labelInstr->GetLoop()->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id))
                && !lifetime->defList.Empty())
            {LOGMEIN("LinearScan.cpp] 4069\n");
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                // If the lifetime was second chance allocated inside the loop, there might
                // be spilled loads of this symbol in the loop.  Insert the stores.
                // We don't need to do this if the lifetime was re-allocated before the loop.
                //
                // Note that reg may not be equal to lifetime->reg because of inserted XCHG...
                this->InsertStores(lifetime, lifetime->reg, insertionStartInstr);
            }

            if (lifetime == labelLifetime)
            {LOGMEIN("LinearScan.cpp] 4080\n");
                continue;
            }

            // 2.   MOV labelReg/MEM, branchReg
            //          Move current register to match content at the top of the loop
            if (this->NeedsLoopBackEdgeCompensation(lifetime, labelInstr))
            {LOGMEIN("LinearScan.cpp] 4087\n");
                // Mismatch, we need to insert compensation code
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);

                // MOV ESI, EAX
                // MOV EDI, ECX
                // MOV ECX, ESI
                // MOV EAX, EDI <<<
                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          lifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }

            // 2.   MOV labelReg, MEM
            //          Lifetime was in a reg at the top of the loop but is spilled right now.
            if (labelLifetime && labelLifetime->isSpilled && !labelLifetime->sym->IsConst() && labelLifetime->end >= branchInstr->GetNumber())
            {LOGMEIN("LinearScan.cpp] 4102\n");
                if (!loop->regAlloc.liveOnBackEdgeSyms->Test(labelLifetime->sym->m_id))
                {LOGMEIN("LinearScan.cpp] 4104\n");
                    continue;
                }
                if (this->ClearLoopExitIfRegUnused(labelLifetime, (RegNum)reg, branchInstr, loop))
                {LOGMEIN("LinearScan.cpp] 4108\n");
                    continue;
                }
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);

                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          labelLifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
        } NEXT_BITSET_IN_UNITBV;

        // 3.   MOV labelReg, MEM
        //          Finish up reloading lifetimes needed at the top.  #2 only handled secondChanceRegs.

        FOREACH_REG(reg)
        {LOGMEIN("LinearScan.cpp] 4122\n");
            // Handle lifetimes in a register at the top of the loop, but not currently.
            Lifetime *labelLifetime = labelRegContent[reg];
            if (labelLifetime && !labelLifetime->sym->IsConst() && labelLifetime != branchRegContent[reg] && !thrashedRegs.Test(reg)
                && (loop->regAlloc.liveOnBackEdgeSyms->Test(labelLifetime->sym->m_id)))
            {LOGMEIN("LinearScan.cpp] 4127\n");
                if (this->ClearLoopExitIfRegUnused(labelLifetime, (RegNum)reg, branchInstr, loop))
                {LOGMEIN("LinearScan.cpp] 4129\n");
                    continue;
                }
                // Mismatch, we need to insert compensation code
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);

                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                            labelLifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
        } NEXT_REG;

        if (hasAirlock)
        {LOGMEIN("LinearScan.cpp] 4141\n");
            loop->regAlloc.hasAirLock = true;
        }
    }
    else
    {
        //
        // Non-loop-back-edge merge
        //
        FOREACH_REG(reg)
        {LOGMEIN("LinearScan.cpp] 4151\n");
            Lifetime *branchLifetime = branchRegContent[reg];
            Lifetime *lifetime = regContent[reg];

            if (lifetime == branchLifetime)
            {LOGMEIN("LinearScan.cpp] 4156\n");
                continue;
            }

            if (branchLifetime && branchLifetime->isSpilled && !branchLifetime->sym->IsConst() && branchLifetime->end > labelInstr->GetNumber())
            {LOGMEIN("LinearScan.cpp] 4161\n");
                // The lifetime was in a reg at the branch and is now spilled.  We need a store on this path.
                //
                //  MOV  MEM, branch_REG
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          branchLifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
            if (lifetime && !lifetime->sym->IsConst() && lifetime->start <= branchInstr->GetNumber())
            {LOGMEIN("LinearScan.cpp] 4170\n");
                // MOV  label_REG, branch_REG / MEM
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          lifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
        } NEXT_REG;
    }

    if (hasAirlock)
    {LOGMEIN("LinearScan.cpp] 4180\n");
        // Fix opHelper on airlock label.
        if (insertionInstr->m_prev->IsLabelInstr() && insertionInstr->IsLabelInstr())
        {LOGMEIN("LinearScan.cpp] 4183\n");
            if (insertionInstr->m_prev->AsLabelInstr()->isOpHelper && !insertionInstr->AsLabelInstr()->isOpHelper)
            {LOGMEIN("LinearScan.cpp] 4185\n");
                insertionInstr->m_prev->AsLabelInstr()->isOpHelper = false;
            }
        }
    }
}

void
LinearScan::ReconcileRegContent(Lifetime ** branchRegContent, Lifetime **labelRegContent,
                                IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr,
                                Lifetime *lifetime, RegNum reg, BitVector *thrashedRegs, IR::Instr *insertionInstr, IR::Instr *insertionStartInstr)
{LOGMEIN("LinearScan.cpp] 4196\n");
    RegNum originalReg = RegNOREG;
    IRType type = RegTypes[reg];
    Assert(labelRegContent[reg] != branchRegContent[reg]);
    bool matchBranchReg = (branchRegContent[reg] == lifetime);
    Lifetime **originalRegContent = (matchBranchReg ? labelRegContent : branchRegContent);
    bool isLoopBackEdge = (branchInstr->GetNumber() > labelInstr->GetNumber());

    if (lifetime->sym->IsConst())
    {LOGMEIN("LinearScan.cpp] 4205\n");
        return;
    }

    // Look if this lifetime was in a different register in the previous block.
    // Split the search in 2 to speed this up.
    if (type == TyMachReg)
    {LOGMEIN("LinearScan.cpp] 4212\n");
        FOREACH_INT_REG(regIter)
        {LOGMEIN("LinearScan.cpp] 4214\n");
            if (originalRegContent[regIter] == lifetime)
            {LOGMEIN("LinearScan.cpp] 4216\n");
                originalReg = regIter;
                break;
            }
        } NEXT_INT_REG;
    }
    else
    {
        Assert(type == TyFloat64 || IRType_IsSimd128(type));

        FOREACH_FLOAT_REG(regIter)
        {LOGMEIN("LinearScan.cpp] 4227\n");
            if (originalRegContent[regIter] == lifetime)
            {LOGMEIN("LinearScan.cpp] 4229\n");
                originalReg = regIter;
                break;
            }
        } NEXT_FLOAT_REG;
    }

    RegNum branchReg, labelReg;
    if (matchBranchReg)
    {LOGMEIN("LinearScan.cpp] 4238\n");
        branchReg = reg;
        labelReg = originalReg;
    }
    else
    {
        branchReg = originalReg;
        labelReg = reg;
    }

    if (branchReg != RegNOREG && !thrashedRegs->Test(branchReg) && !lifetime->sym->IsConst())
    {LOGMEIN("LinearScan.cpp] 4249\n");
        Assert(branchRegContent[branchReg] == lifetime);
        if (labelReg != RegNOREG)
        {LOGMEIN("LinearScan.cpp] 4252\n");
            // MOV labelReg, branchReg
            Assert(labelRegContent[labelReg] == lifetime);
            IR::Instr *load = IR::Instr::New(LowererMD::GetLoadOp(type),
                IR::RegOpnd::New(lifetime->sym, labelReg, type, this->func),
                IR::RegOpnd::New(lifetime->sym, branchReg, type, this->func), this->func);

            insertionInstr->InsertBefore(load);
            load->CopyNumber(insertionInstr);

            // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
            // which allocation it was at the source of the branch.
            if (this->IsInLoop())
            {LOGMEIN("LinearScan.cpp] 4265\n");
                this->RecordLoopUse(lifetime, branchReg);
            }
            thrashedRegs->Set(labelReg);
        }
        else if (!lifetime->sym->IsSingleDef() && lifetime->needsStoreCompensation && !isLoopBackEdge)
        {LOGMEIN("LinearScan.cpp] 4271\n");
            Assert(!lifetime->sym->IsConst());
            Assert(matchBranchReg);
            Assert(branchRegContent[branchReg] == lifetime);

            // MOV mem, branchReg
            this->InsertStores(lifetime, branchReg, insertionInstr->m_prev);

            // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
            // which allocation it was at the source of the branch.
            if (this->IsInLoop())
            {LOGMEIN("LinearScan.cpp] 4282\n");
                this->RecordLoopUse(lifetime, branchReg);
            }
        }
    }
    else if (labelReg != RegNOREG)
    {LOGMEIN("LinearScan.cpp] 4288\n");
        Assert(labelRegContent[labelReg] == lifetime);
        Assert(lifetime->sym->IsConst() || lifetime->sym->IsAllocated());

        if (branchReg != RegNOREG && !lifetime->sym->IsSingleDef())
        {LOGMEIN("LinearScan.cpp] 4293\n");
            Assert(thrashedRegs->Test(branchReg));

            // We can't insert a "MOV labelReg, branchReg" at the insertion point
            // because branchReg was thrashed by a previous reload.
            // Look for that reload to see if we can insert before it.
            IR::Instr *newInsertionInstr = insertionInstr->m_prev;
            bool foundIt = false;
            while (LowererMD::IsAssign(newInsertionInstr))
            {LOGMEIN("LinearScan.cpp] 4302\n");
                IR::Opnd *dst = newInsertionInstr->GetDst();
                IR::Opnd *src = newInsertionInstr->GetSrc1();
                if (src->IsRegOpnd() && src->AsRegOpnd()->GetReg() == labelReg)
                {LOGMEIN("LinearScan.cpp] 4306\n");
                    // This uses labelReg, give up...
                    break;
                }
                if (dst->IsRegOpnd() && dst->AsRegOpnd()->GetReg() == branchReg)
                {LOGMEIN("LinearScan.cpp] 4311\n");
                    // Success!
                    foundIt = true;
                    break;
                }
                newInsertionInstr = newInsertionInstr->m_prev;
            }

            if (foundIt)
            {LOGMEIN("LinearScan.cpp] 4320\n");
                // MOV labelReg, branchReg
                Assert(labelRegContent[labelReg] == lifetime);
                IR::Instr *load = IR::Instr::New(LowererMD::GetLoadOp(type),
                    IR::RegOpnd::New(lifetime->sym, labelReg, type, this->func),
                    IR::RegOpnd::New(lifetime->sym, branchReg, type, this->func), this->func);

                newInsertionInstr->InsertBefore(load);
                load->CopyNumber(newInsertionInstr);

                // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
                // which allocation it was at the source of the branch.
                if (this->IsInLoop())
                {LOGMEIN("LinearScan.cpp] 4333\n");
                    this->RecordLoopUse(lifetime, branchReg);
                }
                thrashedRegs->Set(labelReg);
                return;
            }

            Assert(thrashedRegs->Test(branchReg));
            this->InsertStores(lifetime, branchReg, insertionStartInstr);
            // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
            // which allocation it was at the source of the branch.
            if (this->IsInLoop())
            {LOGMEIN("LinearScan.cpp] 4345\n");
                this->RecordLoopUse(lifetime, branchReg);
            }
        }

        // MOV labelReg, mem
        this->InsertLoad(insertionInstr, lifetime->sym, labelReg);

        thrashedRegs->Set(labelReg);
    }
    else if (!lifetime->sym->IsConst())
    {LOGMEIN("LinearScan.cpp] 4356\n");
        Assert(matchBranchReg);
        Assert(branchReg != RegNOREG);
        // The lifetime was in a register at the top of the loop, but we thrashed it with a previous reload...
        if (!lifetime->sym->IsSingleDef())
        {LOGMEIN("LinearScan.cpp] 4361\n");
            this->InsertStores(lifetime, branchReg, insertionStartInstr);
        }
#if DBG_DUMP
        if (PHASE_TRACE(Js::SecondChancePhase, this->func))
        {LOGMEIN("LinearScan.cpp] 4366\n");
            Output::Print(_u("****** Spilling reg because of bad compensation code order: "));
            lifetime->sym->Dump();
            Output::Print(_u("\n"));
        }
#endif
    }
}

bool LinearScan::ClearLoopExitIfRegUnused(Lifetime *lifetime, RegNum reg, IR::BranchInstr *branchInstr, Loop *loop)
{LOGMEIN("LinearScan.cpp] 4376\n");
    // If a lifetime was enregistered into the loop and then spilled, we need compensation at the bottom
    // of the loop to reload the lifetime into that register.
    // If that lifetime was spilled before it was ever used, we don't need the compensation code.
    // We do however need to clear the regContent on any loop exit as the register will not
    // be available anymore on that path.
    // Note: If the lifetime was reloaded into the same register, we might clear the regContent unnecessarily...
    if (!PHASE_OFF(Js::ClearRegLoopExitPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 4384\n");
        return false;
    }
    if (!loop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id) && !lifetime->needsStoreCompensation)
    {LOGMEIN("LinearScan.cpp] 4388\n");
        if (lifetime->end > branchInstr->GetNumber())
        {
            FOREACH_SLIST_ENTRY(Lifetime **, regContent, loop->regAlloc.exitRegContentList)
            {LOGMEIN("LinearScan.cpp] 4392\n");
                if (regContent[reg] == lifetime)
                {LOGMEIN("LinearScan.cpp] 4394\n");
                    regContent[reg] = nullptr;
                }
            } NEXT_SLIST_ENTRY;
        }
        return true;
    }

    return false;
}
#if defined(_M_IX86) || defined(_M_X64)

void LinearScan::AvoidCompensationConflicts(IR::LabelInstr *labelInstr, IR::BranchInstr *branchInstr,
                                            Lifetime *labelRegContent[], Lifetime *branchRegContent[],
                                            IR::Instr **pInsertionInstr, IR::Instr **pInsertionStartInstr, bool needsAirlock, bool *pHasAirlock)
{LOGMEIN("LinearScan.cpp] 4409\n");
    bool changed = true;

    // Look for conflicts in the incoming compensation code:
    //      MOV     ESI, EAX
    //      MOV     ECX, ESI    <<  ESI was lost...
    // Using XCHG:
    //      XCHG    ESI, EAX
    //      MOV     ECX, EAX
    //
    // Note that we need to iterate while(changed) to catch all conflicts
    while(changed) {LOGMEIN("LinearScan.cpp] 4420\n");
        RegNum conflictRegs[RegNumCount] = {RegNOREG};
        changed = false;

        FOREACH_BITSET_IN_UNITBV(reg, this->secondChanceRegs, BitVector)
        {LOGMEIN("LinearScan.cpp] 4425\n");
            Lifetime *labelLifetime = labelRegContent[reg];
            Lifetime *lifetime = branchRegContent[reg];

            // We don't have an XCHG for SSE2 regs
            if (lifetime == labelLifetime || IRType_IsFloat(RegTypes[reg]))
            {LOGMEIN("LinearScan.cpp] 4431\n");
                continue;
            }

            if (this->NeedsLoopBackEdgeCompensation(lifetime, labelInstr))
            {LOGMEIN("LinearScan.cpp] 4436\n");
                // Mismatch, we need to insert compensation code
                *pInsertionInstr = this->EnsureAirlock(needsAirlock, pHasAirlock, *pInsertionInstr, pInsertionStartInstr, branchInstr, labelInstr);

                if (conflictRegs[reg] != RegNOREG)
                {LOGMEIN("LinearScan.cpp] 4441\n");
                    // Eliminate conflict with an XCHG
                    IR::RegOpnd *reg1 = IR::RegOpnd::New(branchRegContent[reg]->sym, (RegNum)reg, RegTypes[reg], this->func);
                    IR::RegOpnd *reg2 = IR::RegOpnd::New(branchRegContent[reg]->sym, conflictRegs[reg], RegTypes[reg], this->func);
                    IR::Instr *instrXchg = IR::Instr::New(Js::OpCode::XCHG, reg1, reg1, reg2, this->func);
                    (*pInsertionInstr)->InsertBefore(instrXchg);
                    instrXchg->CopyNumber(*pInsertionInstr);

                    Lifetime *tmpLifetime = branchRegContent[reg];
                    branchRegContent[reg] = branchRegContent[conflictRegs[reg]];
                    branchRegContent[conflictRegs[reg]] = tmpLifetime;
                    reg = conflictRegs[reg];

                    changed = true;
                }
                RegNum labelReg = RegNOREG;
                FOREACH_INT_REG(regIter)
                {LOGMEIN("LinearScan.cpp] 4458\n");
                    if (labelRegContent[regIter] == branchRegContent[reg])
                    {LOGMEIN("LinearScan.cpp] 4460\n");
                        labelReg = regIter;
                        break;
                    }
                } NEXT_INT_REG;

                if (labelReg != RegNOREG)
                {LOGMEIN("LinearScan.cpp] 4467\n");
                    conflictRegs[labelReg] = (RegNum)reg;
                }
            }
        } NEXT_BITSET_IN_UNITBV;
    }
}
#endif
RegNum
LinearScan::SecondChanceAllocation(Lifetime *lifetime, bool force)
{LOGMEIN("LinearScan.cpp] 4477\n");
    if (PHASE_OFF(Js::SecondChancePhase, this->func) || this->func->HasTry())
    {LOGMEIN("LinearScan.cpp] 4479\n");
        return RegNOREG;
    }

    // Don't start a second chance allocation from a helper block
    if (lifetime->dontAllocate || this->IsInHelperBlock() || lifetime->isDeadStore)
    {LOGMEIN("LinearScan.cpp] 4485\n");
        return RegNOREG;
    }

    Assert(lifetime->isSpilled);
    Assert(lifetime->sym->IsConst() || lifetime->sym->IsAllocated());

    RegNum oldReg = lifetime->reg;
    RegNum reg;

    if (lifetime->start == this->currentInstr->GetNumber() || lifetime->end == this->currentInstr->GetNumber())
    {LOGMEIN("LinearScan.cpp] 4496\n");
        // No point doing second chance if the lifetime ends here, or starts here (normal allocation would
        // have found a register if one is available).
        return RegNOREG;
    }
    if (lifetime->sym->IsConst())
    {LOGMEIN("LinearScan.cpp] 4502\n");
        // Can't second-chance allocate because we might have deleted the initial def instr, after
        //         having set the reg content on a forward branch...
        return RegNOREG;
    }

    lifetime->reg = RegNOREG;
    lifetime->isSecondChanceAllocated = true;
    reg = this->FindReg(lifetime, nullptr, force);
    lifetime->reg = oldReg;

    if (reg == RegNOREG)
    {LOGMEIN("LinearScan.cpp] 4514\n");
        lifetime->isSecondChanceAllocated = false;
        return reg;
    }

    // Success!! We're re-allocating this lifetime...

    this->SecondChanceAllocateToReg(lifetime, reg);

    return reg;
}

void LinearScan::SecondChanceAllocateToReg(Lifetime *lifetime, RegNum reg)
{LOGMEIN("LinearScan.cpp] 4527\n");
    RegNum oldReg = lifetime->reg;

    if (oldReg != RegNOREG && this->tempRegLifetimes[oldReg] == lifetime)
    {LOGMEIN("LinearScan.cpp] 4531\n");
        this->tempRegs.Clear(oldReg);
    }

    lifetime->isSpilled = false;
    lifetime->isSecondChanceAllocated = true;
    lifetime->lastAllocationStart = this->currentInstr->GetNumber();

    lifetime->reg = RegNOREG;
    this->AssignActiveReg(lifetime, reg);
    this->secondChanceRegs.Set(reg);

    lifetime->sym->scratch.linearScan.lifetime->useList.Clear();

#if DBG_DUMP
    if (PHASE_TRACE(Js::SecondChancePhase, this->func))
    {LOGMEIN("LinearScan.cpp] 4547\n");
        Output::Print(_u("**** Second chance: "));
        lifetime->sym->Dump();
        Output::Print(_u("\t Reg: %S  "), RegNames[reg]);
        Output::Print(_u("  SpillCount:%d  Length:%d   Cost:%d  %S\n"),
            lifetime->useCount, lifetime->end - lifetime->start, this->GetSpillCost(lifetime),
            lifetime->isLiveAcrossCalls ? "LiveAcrossCalls" : "");
    }
#endif
}

IR::Instr *
LinearScan::InsertAirlock(IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr)
{LOGMEIN("LinearScan.cpp] 4560\n");
    // Insert a new block on a flow arc:
    //   JEQ L1             JEQ L2
    //   ...        =>      ...
    //  <fallthrough>       JMP L1
    // L1:                 L2:
    //                       <new block>
    //                     L1:
    // An airlock is needed when we need to add code on a flow arc, and the code can't
    // be added directly at the source or sink of that flow arc without impacting other
    // code paths.

    bool isOpHelper = labelInstr->isOpHelper;

    if (!isOpHelper)
    {LOGMEIN("LinearScan.cpp] 4575\n");
        // Check if branch is coming from helper block.
        IR::Instr *prevLabel = branchInstr->m_prev;
        while (prevLabel && !prevLabel->IsLabelInstr())
        {LOGMEIN("LinearScan.cpp] 4579\n");
            prevLabel = prevLabel->m_prev;
        }
        if (prevLabel && prevLabel->AsLabelInstr()->isOpHelper)
        {LOGMEIN("LinearScan.cpp] 4583\n");
            isOpHelper = true;
        }
    }
    IR::LabelInstr *airlockLabel = IR::LabelInstr::New(Js::OpCode::Label, this->func, isOpHelper);
    airlockLabel->SetRegion(this->currentRegion);
#if DBG
    if (isOpHelper)
    {LOGMEIN("LinearScan.cpp] 4591\n");
        if (branchInstr->m_isHelperToNonHelperBranch)
        {LOGMEIN("LinearScan.cpp] 4593\n");
            labelInstr->m_noHelperAssert = true;
        }
        if (labelInstr->isOpHelper && labelInstr->m_noHelperAssert)
        {LOGMEIN("LinearScan.cpp] 4597\n");
            airlockLabel->m_noHelperAssert = true;
        }
    }
#endif
    bool replaced = branchInstr->ReplaceTarget(labelInstr, airlockLabel);
    Assert(replaced);

    IR::Instr * prevInstr = labelInstr->GetPrevRealInstrOrLabel();
    if (prevInstr->HasFallThrough())
    {LOGMEIN("LinearScan.cpp] 4607\n");
        IR::BranchInstr *branchOverAirlock = IR::BranchInstr::New(LowererMD::MDUncondBranchOpcode, labelInstr, this->func);
        prevInstr->InsertAfter(branchOverAirlock);
        branchOverAirlock->CopyNumber(prevInstr);
        prevInstr = branchOverAirlock;
        branchOverAirlock->m_isAirlock = true;
        branchOverAirlock->m_regContent = nullptr;
    }

    prevInstr->InsertAfter(airlockLabel);
    airlockLabel->CopyNumber(prevInstr);
    prevInstr = labelInstr->GetPrevRealInstrOrLabel();

    return labelInstr;
}

void
LinearScan::SaveRegContent(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 4625\n");
    bool isLabelLoopTop = false;
    Lifetime ** regContent = AnewArrayZ(this->tempAlloc, Lifetime *, RegNumCount);

    if (instr->IsBranchInstr())
    {LOGMEIN("LinearScan.cpp] 4630\n");
        instr->AsBranchInstr()->m_regContent = regContent;
    }
    else
    {
        Assert(instr->IsLabelInstr());
        Assert(instr->AsLabelInstr()->m_isLoopTop);
        instr->AsLabelInstr()->m_regContent = regContent;
        isLabelLoopTop = true;
    }

    js_memcpy_s(regContent, (RegNumCount * sizeof(Lifetime *)), this->regContent, sizeof(this->regContent));

#if DBG
    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->activeLiveranges)
    {LOGMEIN("LinearScan.cpp] 4645\n");
        Assert(regContent[lifetime->reg] == lifetime);
    } NEXT_SLIST_ENTRY;
#endif
}

bool LinearScan::RegsAvailable(IRType type)
{LOGMEIN("LinearScan.cpp] 4652\n");
    if (IRType_IsFloat(type) || IRType_IsSimd128(type))
    {LOGMEIN("LinearScan.cpp] 4654\n");
        return (this->floatRegUsedCount < FLOAT_REG_COUNT);
    }
    else
    {
        return (this->intRegUsedCount < INT_REG_COUNT);
    }
}

uint LinearScan::GetRemainingHelperLength(Lifetime *const lifetime)
{LOGMEIN("LinearScan.cpp] 4664\n");
    // Walk the helper block linked list starting from the next helper block until the end of the lifetime
    uint helperLength = 0;
    SList<OpHelperBlock>::Iterator it(opHelperBlockIter);
    Assert(it.IsValid());
    const uint end = max(currentInstr->GetNumber(), lifetime->end);
    do
    {LOGMEIN("LinearScan.cpp] 4671\n");
        const OpHelperBlock &helper = it.Data();
        const uint helperStart = helper.opHelperLabel->GetNumber();
        if(helperStart > end)
        {LOGMEIN("LinearScan.cpp] 4675\n");
            break;
        }

        const uint helperEnd = min(end, helper.opHelperEndInstr->GetNumber());
        helperLength += helperEnd - helperStart;
        if(helperEnd != helper.opHelperEndInstr->GetNumber() || !helper.opHelperEndInstr->IsLabelInstr())
        {LOGMEIN("LinearScan.cpp] 4682\n");
            // A helper block that ends at a label does not return to the function. Since this helper block does not end
            // at a label, include the end instruction as well.
            ++helperLength;
        }
    } while(it.Next());

    return helperLength;
}

uint LinearScan::CurrentOpHelperVisitedLength(IR::Instr *const currentInstr) const
{LOGMEIN("LinearScan.cpp] 4693\n");
    Assert(currentInstr);

    if(!currentOpHelperBlock)
    {LOGMEIN("LinearScan.cpp] 4697\n");
        return 0;
    }

    // Consider the current instruction to have not yet been visited
    Assert(currentInstr->GetNumber() >= currentOpHelperBlock->opHelperLabel->GetNumber());
    return currentInstr->GetNumber() - currentOpHelperBlock->opHelperLabel->GetNumber();
}

IR::Instr * LinearScan::TryHoistLoad(IR::Instr *instr, Lifetime *lifetime)
{LOGMEIN("LinearScan.cpp] 4707\n");
    // If we are loading a lifetime into a register inside a loop, try to hoist that load outside the loop
    // if that register hasn't been used yet.
    RegNum reg = lifetime->reg;
    IR::Instr *insertInstr = instr;

    if (PHASE_OFF(Js::RegHoistLoadsPhase, this->func))
    {LOGMEIN("LinearScan.cpp] 4714\n");
        return insertInstr;
    }

    if ((this->func->HasTry() && !this->func->DoOptimizeTryCatch()) || (this->currentRegion && this->currentRegion->GetType() != RegionTypeRoot))
    {LOGMEIN("LinearScan.cpp] 4719\n");
        return insertInstr;
    }

    // Register unused, and lifetime unused yet.
    if (this->IsInLoop() && !this->curLoop->regAlloc.regUseBv.Test(reg)
        && !this->curLoop->regAlloc.defdInLoopBv->Test(lifetime->sym->m_id)
        && !this->curLoop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id)
        && !this->curLoop->regAlloc.hasAirLock)
    {LOGMEIN("LinearScan.cpp] 4728\n");
        // Let's hoist!
        insertInstr = insertInstr->m_prev;

        // Walk each instructions until the top of the loop looking for branches
        while (!insertInstr->IsLabelInstr() || !insertInstr->AsLabelInstr()->m_isLoopTop || !insertInstr->AsLabelInstr()->GetLoop()->IsDescendentOrSelf(this->curLoop))
        {LOGMEIN("LinearScan.cpp] 4734\n");
            if (insertInstr->IsBranchInstr() && insertInstr->AsBranchInstr()->m_regContent)
            {LOGMEIN("LinearScan.cpp] 4736\n");
                IR::BranchInstr *branchInstr = insertInstr->AsBranchInstr();
                // That lifetime might have been in another register coming into the loop, and spilled before used.
                // Clear the reg content.
                FOREACH_REG(regIter)
                {LOGMEIN("LinearScan.cpp] 4741\n");
                    if (branchInstr->m_regContent[regIter] == lifetime)
                    {LOGMEIN("LinearScan.cpp] 4743\n");
                        branchInstr->m_regContent[regIter] = nullptr;
                    }
                } NEXT_REG;
                // Set the regContent for that reg to the lifetime on this branch
                branchInstr->m_regContent[reg] = lifetime;
            }
            insertInstr = insertInstr->m_prev;
        }

        IR::LabelInstr *loopTopLabel = insertInstr->AsLabelInstr();

        // Set the reg content for the loop top correctly as well
        FOREACH_REG(regIter)
        {LOGMEIN("LinearScan.cpp] 4757\n");
            if (loopTopLabel->m_regContent[regIter] == lifetime)
            {LOGMEIN("LinearScan.cpp] 4759\n");
                loopTopLabel->m_regContent[regIter] = nullptr;
                this->curLoop->regAlloc.loopTopRegContent[regIter] = nullptr;
            }
        } NEXT_REG;

        Assert(loopTopLabel->GetLoop() == this->curLoop);
        loopTopLabel->m_regContent[reg] = lifetime;
        this->curLoop->regAlloc.loopTopRegContent[reg] = lifetime;

        this->RecordLoopUse(lifetime, reg);

        IR::LabelInstr *loopLandingPad = nullptr;

        Assert(loopTopLabel->GetNumber() != Js::Constants::NoByteCodeOffset);

        // Insert load in landing pad.
        // Redirect branches to new landing pad.
        FOREACH_SLISTCOUNTED_ENTRY_EDITING(IR::BranchInstr *, branchInstr, &loopTopLabel->labelRefs, iter)
        {LOGMEIN("LinearScan.cpp] 4778\n");
            Assert(branchInstr->GetNumber() != Js::Constants::NoByteCodeOffset);
            // <= because the branch may be newly inserted and have the same instr number as the loop top...
            if (branchInstr->GetNumber() <= loopTopLabel->GetNumber())
            {LOGMEIN("LinearScan.cpp] 4782\n");
                if (!loopLandingPad)
                {LOGMEIN("LinearScan.cpp] 4784\n");
                    loopLandingPad = IR::LabelInstr::New(Js::OpCode::Label, this->func);
                    loopLandingPad->SetRegion(this->currentRegion);
                    loopTopLabel->InsertBefore(loopLandingPad);
                    loopLandingPad->CopyNumber(loopTopLabel);
                }
                branchInstr->ReplaceTarget(loopTopLabel, loopLandingPad);
            }
        } NEXT_SLISTCOUNTED_ENTRY_EDITING;
    }

    return insertInstr;
}

#if DBG_DUMP

void LinearScan::PrintStats() const
{LOGMEIN("LinearScan.cpp] 4801\n");
    uint loopNest = 0;
    uint storeCount = 0;
    uint loadCount = 0;
    uint wStoreCount = 0;
    uint wLoadCount = 0;
    uint instrCount = 0;
    bool isInHelper = false;

    FOREACH_INSTR_IN_FUNC_BACKWARD(instr, this->func)
    {LOGMEIN("LinearScan.cpp] 4811\n");
        switch (instr->GetKind())
        {LOGMEIN("LinearScan.cpp] 4813\n");
        case IR::InstrKindPragma:
            continue;

        case IR::InstrKindBranch:
            if (instr->AsBranchInstr()->IsLoopTail(this->func))
            {LOGMEIN("LinearScan.cpp] 4819\n");
                loopNest++;
            }

            instrCount++;
            break;

        case IR::InstrKindLabel:
        case IR::InstrKindProfiledLabel:
            if (instr->AsLabelInstr()->m_isLoopTop)
            {LOGMEIN("LinearScan.cpp] 4829\n");
                Assert(loopNest);
                loopNest--;
            }

            isInHelper = instr->AsLabelInstr()->isOpHelper;
            break;

        default:
            {
                Assert(instr->IsRealInstr());

                if (isInHelper)
                {LOGMEIN("LinearScan.cpp] 4842\n");
                    continue;
                }

                IR::Opnd *dst = instr->GetDst();
                if (dst && dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsStackSym() && dst->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                {LOGMEIN("LinearScan.cpp] 4848\n");
                    storeCount++;
                    wStoreCount += LinearScan::GetUseSpillCost(loopNest, false);
                }
                IR::Opnd *src1 = instr->GetSrc1();
                if (src1)
                {LOGMEIN("LinearScan.cpp] 4854\n");
                    if (src1->IsSymOpnd() && src1->AsSymOpnd()->m_sym->IsStackSym() && src1->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                    {LOGMEIN("LinearScan.cpp] 4856\n");
                        loadCount++;
                        wLoadCount += LinearScan::GetUseSpillCost(loopNest, false);
                    }
                    IR::Opnd *src2 = instr->GetSrc2();
                    if (src2 && src2->IsSymOpnd() && src2->AsSymOpnd()->m_sym->IsStackSym() && src2->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                    {LOGMEIN("LinearScan.cpp] 4862\n");
                        loadCount++;
                        wLoadCount += LinearScan::GetUseSpillCost(loopNest, false);
                    }
                }
            }
            break;
        }
    } NEXT_INSTR_IN_FUNC_BACKWARD;

    Assert(loopNest == 0);

    this->func->DumpFullFunctionName();
    Output::SkipToColumn(45);

    Output::Print(_u("Instrs:%5d, Lds:%4d, Strs:%4d, WLds: %4d, WStrs: %4d, WRefs: %4d\n"),
        instrCount, loadCount, storeCount, wLoadCount, wStoreCount, wLoadCount+wStoreCount);
}

#endif


#ifdef _M_IX86

# if ENABLE_DEBUG_CONFIG_OPTIONS

IR::Instr * LinearScan::GetIncInsertionPoint(IR::Instr *instr)
{LOGMEIN("LinearScan.cpp] 4889\n");
    // Make sure we don't insert an INC between an instr setting the condition code, and one using it.
    IR::Instr *instrNext = instr;
    while(!EncoderMD::UsesConditionCode(instrNext) && !EncoderMD::SetsConditionCode(instrNext))
    {LOGMEIN("LinearScan.cpp] 4893\n");
        if (instrNext->IsLabelInstr() || instrNext->IsExitInstr() || instrNext->IsBranchInstr())
        {LOGMEIN("LinearScan.cpp] 4895\n");
            break;
        }
        instrNext = instrNext->GetNextRealInstrOrLabel();
    }

    if (instrNext->IsLowered() && EncoderMD::UsesConditionCode(instrNext))
    {LOGMEIN("LinearScan.cpp] 4902\n");
        IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();
        while(!EncoderMD::SetsConditionCode(instrPrev))
        {LOGMEIN("LinearScan.cpp] 4905\n");
            instrPrev = instrPrev->GetPrevRealInstrOrLabel();
            Assert(!instrPrev->IsLabelInstr());
        }

        return instrPrev;
    }

    return instr;
}

void LinearScan::DynamicStatsInstrument()
{LOGMEIN("LinearScan.cpp] 4917\n");    
    {LOGMEIN("LinearScan.cpp] 4918\n");
        IR::Instr *firstInstr = this->func->m_headInstr;
    IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(this->func->GetJITFunctionBody()->GetCallCountStatsAddr(), TyUint32, this->func);
        firstInstr->InsertAfter(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
    }

    FOREACH_INSTR_IN_FUNC(instr, this->func)
    {LOGMEIN("LinearScan.cpp] 4925\n");
        if (!instr->IsRealInstr() || !instr->IsLowered())
        {LOGMEIN("LinearScan.cpp] 4927\n");
            continue;
        }

        if (EncoderMD::UsesConditionCode(instr) && instr->GetPrevRealInstrOrLabel()->IsLabelInstr())
        {LOGMEIN("LinearScan.cpp] 4932\n");
            continue;
        }

        IR::Opnd *dst = instr->GetDst();
        if (dst && dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsStackSym() && dst->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
        {LOGMEIN("LinearScan.cpp] 4938\n");
            IR::Instr *insertionInstr = this->GetIncInsertionPoint(instr);
            IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(this->func->GetJITFunctionBody()->GetRegAllocStoreCountAddr(), TyUint32, this->func);
            insertionInstr->InsertBefore(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
        }
        IR::Opnd *src1 = instr->GetSrc1();
        if (src1)
        {LOGMEIN("LinearScan.cpp] 4945\n");
            if (src1->IsSymOpnd() && src1->AsSymOpnd()->m_sym->IsStackSym() && src1->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
            {LOGMEIN("LinearScan.cpp] 4947\n");
                IR::Instr *insertionInstr = this->GetIncInsertionPoint(instr);
                IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(this->func->GetJITFunctionBody()->GetRegAllocStoreCountAddr(), TyUint32, this->func);
                insertionInstr->InsertBefore(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
            }
            IR::Opnd *src2 = instr->GetSrc2();
            if (src2 && src2->IsSymOpnd() && src2->AsSymOpnd()->m_sym->IsStackSym() && src2->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
            {LOGMEIN("LinearScan.cpp] 4954\n");
                IR::Instr *insertionInstr = this->GetIncInsertionPoint(instr);
                IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(this->func->GetJITFunctionBody()->GetRegAllocStoreCountAddr(), TyUint32, this->func);
                insertionInstr->InsertBefore(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
            }
        }
    } NEXT_INSTR_IN_FUNC;
}

# endif  //ENABLE_DEBUG_CONFIG_OPTIONS
#endif  // _M_IX86

IR::Instr* LinearScan::InsertMove(IR::Opnd *dst, IR::Opnd *src, IR::Instr *const insertBeforeInstr)
{LOGMEIN("LinearScan.cpp] 4967\n");
    IR::Instr *instrPrev = insertBeforeInstr->m_prev;

    IR::Instr *instrRet = Lowerer::InsertMove(dst, src, insertBeforeInstr);

    for (IR::Instr *instr = instrPrev->m_next; instr != insertBeforeInstr; instr = instr->m_next)
    {LOGMEIN("LinearScan.cpp] 4973\n");
        instr->CopyNumber(insertBeforeInstr);
    }

    return instrRet;
}

IR::Instr* LinearScan::InsertLea(IR::RegOpnd *dst, IR::Opnd *src, IR::Instr *const insertBeforeInstr)
{LOGMEIN("LinearScan.cpp] 4981\n");
    IR::Instr *instrPrev = insertBeforeInstr->m_prev;

    IR::Instr *instrRet = Lowerer::InsertLea(dst, src, insertBeforeInstr, true);

    for (IR::Instr *instr = instrPrev->m_next; instr != insertBeforeInstr; instr = instr->m_next)
    {LOGMEIN("LinearScan.cpp] 4987\n");
        instr->CopyNumber(insertBeforeInstr);
    }

    return instrRet;
}
