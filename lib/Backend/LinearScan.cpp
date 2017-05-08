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
{TRACE_IT(10135);
    return JitAnew(allocator, LoweredBasicBlock, allocator);
}

void LoweredBasicBlock::Copy(LoweredBasicBlock* block)
{TRACE_IT(10136);
    this->inlineeFrameLifetimes.Copy(&block->inlineeFrameLifetimes);
    this->inlineeStack.Copy(&block->inlineeStack);
    this->inlineeFrameSyms.Copy(&block->inlineeFrameSyms);
}

bool LoweredBasicBlock::HasData()
{TRACE_IT(10137);
    return this->inlineeFrameLifetimes.Count() > 0 || this->inlineeStack.Count() > 0;
}

LoweredBasicBlock* LoweredBasicBlock::Clone(JitArenaAllocator* allocator)
{TRACE_IT(10138);
    if (this->HasData())
    {TRACE_IT(10139);
        LoweredBasicBlock* clone = LoweredBasicBlock::New(allocator);
        clone->Copy(this);
        return clone;
    }
    return nullptr;
}

bool LoweredBasicBlock::Equals(LoweredBasicBlock* otherBlock)
{TRACE_IT(10140);
    if(this->HasData() != otherBlock->HasData())
    {TRACE_IT(10141);
        return false;
    }
    if (!this->inlineeFrameLifetimes.Equals(&otherBlock->inlineeFrameLifetimes))
    {TRACE_IT(10142);
        return false;
    }
    if (!this->inlineeStack.Equals(&otherBlock->inlineeStack))
    {TRACE_IT(10143);
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
{TRACE_IT(10144);
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
    {TRACE_IT(10145);
        this->globalBailOutRecordTables = NativeCodeDataNewArrayZ(nativeAllocator, GlobalBailOutRecordDataTable *,  func->m_inlineeId + 1);
        this->lastUpdatedRowIndices = JitAnewArrayZ(this->tempAlloc, uint *, func->m_inlineeId + 1);

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
        {TRACE_IT(10146);
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
    {TRACE_IT(10147);
        if (instr->GetNumber() == 0)
        {TRACE_IT(10148);
            AssertMsg(LowererMD::IsAssign(instr), "Only expect spill code here");
            continue;
        }

#if DBG_DUMP && defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::LinearScanPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {TRACE_IT(10149);
            instr->Dump();
        }
#endif // DBG

        this->currentInstr = instr;
        if(instr->StartsBasicBlock() || endOfBasicBlock)
        {TRACE_IT(10150);
            endOfBasicBlock = false;
            ++currentBlockNumber;
        }

        if (instr->IsLabelInstr())
        {TRACE_IT(10151);
            this->lastLabel = instr->AsLabelInstr();
            if (this->lastLabel->m_loweredBasicBlock)
            {TRACE_IT(10152);
                this->currentBlock = this->lastLabel->m_loweredBasicBlock;
            }
            else if(currentBlock->HasData())
            {TRACE_IT(10153);
                // Check if the previous block has fall-through. If so, retain the block info. If not, create empty info.
                IR::Instr *const prevInstr = instr->GetPrevRealInstrOrLabel();
                Assert(prevInstr);
                if(!prevInstr->HasFallThrough())
                {TRACE_IT(10154);
                    currentBlock = LoweredBasicBlock::New(&tempAlloc);
                }
            }
            this->currentRegion = this->lastLabel->GetRegion();
        }
        else if (instr->IsBranchInstr())
        {TRACE_IT(10155);
            if (this->func->HasTry() && this->func->DoOptimizeTryCatch())
            {TRACE_IT(10156);
                this->ProcessEHRegionBoundary(instr);
            }
            this->ProcessSecondChanceBoundary(instr->AsBranchInstr());
        }

        this->CheckIfInLoop(instr);

        if (this->RemoveDeadStores(instr))
        {TRACE_IT(10157);
            continue;
        }

        if (instr->HasBailOutInfo())
        {TRACE_IT(10158);
            if (this->currentRegion)
            {TRACE_IT(10159);
                RegionType curRegType = this->currentRegion->GetType();
                Assert(curRegType != RegionTypeFinally); //Finally regions are not optimized yet
                if (curRegType == RegionTypeTry || curRegType == RegionTypeCatch)
                {TRACE_IT(10160);
                    this->func->hasBailoutInEHRegion = true;
                }
            }

            this->FillBailOutRecord(instr);
            if (instr->GetBailOutKind() == IR::BailOutForGeneratorYield)
            {TRACE_IT(10161);
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
        {TRACE_IT(10162);
            this->ProcessSecondChanceBoundary(instr->AsLabelInstr());
        }

#if DBG
        this->CheckInvariants();
#endif // DBG

        if(instr->EndsBasicBlock())
        {TRACE_IT(10163);
            endOfBasicBlock = true;
        }

        if (insertBailInAfter == instr)
        {TRACE_IT(10164);
            instrNext = linearScanMD.GenerateBailInForGeneratorYield(instr, bailOutInfoForBailIn);
            insertBailInAfter = nullptr;
            bailOutInfoForBailIn = nullptr;
        }
    }NEXT_INSTR_EDITING;

    if (func->hasBailout)
    {TRACE_IT(10165);
        for (uint i = 0; i <= func->m_inlineeId; i++)
        {TRACE_IT(10166);
            if (globalBailOutRecordTables[i] != nullptr)
            {TRACE_IT(10167);
                globalBailOutRecordTables[i]->Finalize(nativeAllocator, &tempAlloc);
#ifdef PROFILE_BAILOUT_RECORD_MEMORY
                if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
                {TRACE_IT(10168);
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
    {TRACE_IT(10169);
        this->DynamicStatsInstrument();
    }
# endif
#endif

#if DBG_DUMP
    if (PHASE_STATS(Js::LinearScanPhase, this->func))
    {TRACE_IT(10170);
        this->PrintStats();
    }
    if (PHASE_TRACE(Js::StackPackPhase, this->func))
    {TRACE_IT(10171);
        Output::Print(_u("---------------------------\n"));
    }
#endif // DBG_DUMP
    DebugOnly(this->func->allowRemoveBailOutArgInstr = true);
}

JitArenaAllocator *
LinearScan::GetTempAlloc()
{TRACE_IT(10172);
    Assert(tempAlloc);
    return tempAlloc;
}

#if DBG
void
LinearScan::CheckInvariants() const
{TRACE_IT(10173);
    BitVector bv = this->nonAllocatableRegs;
    uint32 lastend = 0;
    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->activeLiveranges)
    {TRACE_IT(10174);
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
    {TRACE_IT(10175);
        if (IRType_IsFloat(RegTypes[index]))
        {TRACE_IT(10176);
            floats++;
        }
        else
        {TRACE_IT(10177);
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
    {TRACE_IT(10178);
        // Make sure there are only one lifetime per reg in the op helper spilled liveranges
        Assert(!bv.Test(lifetime->reg));
        if (!lifetime->cantOpHelperSpill)
        {TRACE_IT(10179);
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
    {TRACE_IT(10180);
        if (this->tempRegs.Test(i))
        {TRACE_IT(10181);
            Assert(this->tempRegLifetimes[i]->reg == i);
        }
    }

    FOREACH_BITSET_IN_UNITBV(reg, this->secondChanceRegs, BitVector)
    {TRACE_IT(10182);
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
{TRACE_IT(10183);
    FOREACH_REG(reg)
    {TRACE_IT(10184);
        // Registers that can't be used are set to active, and will remain this way
        if (!LinearScan::IsAllocatable(reg))
        {TRACE_IT(10185);
            this->activeRegs.Set(reg);
            if (IRType_IsFloat(RegTypes[reg]))
            {TRACE_IT(10186);
                this->floatRegUsedCount++;
            }
            else
            {TRACE_IT(10187);
                this->intRegUsedCount++;
            }
        }
        if (RegTypes[reg] == TyMachReg)
        {TRACE_IT(10188);
            // JIT64_TODO: Rename int32Regs to machIntRegs.
            this->int32Regs.Set(reg);
            numInt32Regs++;
        }
        else if (RegTypes[reg] == TyFloat64)
        {TRACE_IT(10189);
            this->floatRegs.Set(reg);
            numFloatRegs++;
        }
        if (LinearScan::IsCallerSaved(reg))
        {TRACE_IT(10190);
            this->callerSavedRegs.Set(reg);
        }
        if (LinearScan::IsCalleeSaved(reg))
        {TRACE_IT(10191);
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
    {TRACE_IT(10192);
        this->func->DumpHeader();
    }
#endif
}

// LinearScan::CheckIfInLoop
// Track whether the current instruction is in a loop or not.
bool
LinearScan::CheckIfInLoop(IR::Instr *instr)
{TRACE_IT(10193);
    if (this->IsInLoop())
    {TRACE_IT(10194);
        // Look for end of loop

        AssertMsg(this->curLoop->regAlloc.loopEnd != 0, "Something is wrong here....");

        if (instr->GetNumber() >= this->curLoop->regAlloc.loopEnd)
        {TRACE_IT(10195);
            AssertMsg(instr->IsBranchInstr(), "Loop tail should be a branchInstr");
            while (this->IsInLoop() && instr->GetNumber() >= this->curLoop->regAlloc.loopEnd)
            {TRACE_IT(10196);
                this->loopNest--;
                this->curLoop->isProcessed = true;
                this->curLoop = this->curLoop->parent;
                if (this->loopNest == 0)
                {TRACE_IT(10197);
                    this->liveOnBackEdgeSyms->ClearAll();
                }
            }
        }
    }
    if (instr->IsLabelInstr() && instr->AsLabelInstr()->m_isLoopTop)
    {TRACE_IT(10198);
        IR::LabelInstr * labelInstr = instr->AsLabelInstr();
        Loop *parentLoop = this->curLoop;
        if (parentLoop)
        {TRACE_IT(10199);
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
{TRACE_IT(10200);
    linearScanMD.InsertOpHelperSpillAndRestores(opHelperBlockList);
}

void
LinearScan::CheckOpHelper(IR::Instr *instr)
{TRACE_IT(10201);
    if (this->IsInHelperBlock())
    {TRACE_IT(10202);
        if (this->currentOpHelperBlock->opHelperEndInstr == instr)
        {TRACE_IT(10203);
            // Get targetInstr if we can.
            // We can deterministically get it only for unconditional branches, as conditional branch may fall through.
            IR::Instr * targetInstr = nullptr;
            if (instr->IsBranchInstr() && instr->AsBranchInstr()->IsUnconditional())
            {TRACE_IT(10204);
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
            {TRACE_IT(10205);
                Lifetime * lifetime = this->opHelperSpilledLiveranges->Pop();
                lifetime->isOpHelperSpilled = false;

                if (!lifetime->cantOpHelperSpill)
                {TRACE_IT(10206);
                    // Put the life time back to active
                    this->AssignActiveReg(lifetime, lifetime->reg);
                    bool reload = true;
                    // Lifetime ends before the target after helper block, don't need to save and restore helper spilled lifetime.
                    if (targetInstr && lifetime->end < targetInstr->GetNumber())
                    {TRACE_IT(10207);
                        // However, if lifetime is spilled as arg - we still need to spill it because the helper assumes the value
                        // to be available in the stack
                        if (lifetime->isOpHelperSpillAsArg)
                        {TRACE_IT(10208);
                            // we should not attempt to restore it as it is dead on return from the helper.
                            reload = false;
                        }
                        else
                        {TRACE_IT(10209);
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
                {TRACE_IT(10210);
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
    {TRACE_IT(10211);
        AssertMsg(
            !instr->IsLabelInstr() ||
            !instr->AsLabelInstr()->isOpHelper ||
            this->opHelperBlockIter.Data().opHelperLabel == instr,
            "Found a helper label that doesn't begin the next helper block in the list?");

        if (this->opHelperBlockIter.Data().opHelperLabel == instr)
        {TRACE_IT(10212);
            this->currentOpHelperBlock = &this->opHelperBlockIter.Data();
            this->opHelperBlockIter.Next();
        }
    }
}

uint
LinearScan::HelperBlockStartInstrNumber() const
{TRACE_IT(10213);
    Assert(IsInHelperBlock());
    return this->currentOpHelperBlock->opHelperLabel->GetNumber();
}

uint
LinearScan::HelperBlockEndInstrNumber() const
{TRACE_IT(10214);
    Assert(IsInHelperBlock());
    return this->currentOpHelperBlock->opHelperEndInstr->GetNumber();
}
// LinearScan::AddToActive
// Add a lifetime to the active list.  The list is kept sorted in order lifetime end.
// This makes it easier to pick the lifetimes to retire.
void
LinearScan::AddToActive(Lifetime * lifetime)
{TRACE_IT(10215);
    LinearScan::AddLiveRange(this->activeLiveranges, lifetime);
    this->regContent[lifetime->reg] = lifetime;
    if (lifetime->isSecondChanceAllocated)
    {TRACE_IT(10216);
        this->secondChanceRegs.Set(lifetime->reg);
    }
    else
    {TRACE_IT(10217);
        Assert(!this->secondChanceRegs.Test(lifetime->reg));
    }
}

void
LinearScan::AddOpHelperSpilled(Lifetime * lifetime)
{TRACE_IT(10218);
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
    {TRACE_IT(10219);
        lifetime->isOpHelperSpillAsArg = true;
        if (!lifetime->sym->IsAllocated())
        {TRACE_IT(10220);
            this->AllocateStackSpace(lifetime);
        }
        this->RecordLoopUse(lifetime, lifetime->reg);
    }
    LinearScan::AddLiveRange(this->opHelperSpilledLiveranges, lifetime);
}

void
LinearScan::RemoveOpHelperSpilled(Lifetime * lifetime)
{TRACE_IT(10221);
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
{TRACE_IT(10222);
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
    {TRACE_IT(10223);
        if (newLifetime->end < lifetime->end)
        {TRACE_IT(10224);
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
    {TRACE_IT(10225);
        if (lifetime->reg == reg)
        {TRACE_IT(10226);
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
{TRACE_IT(10227);
    //
    // Enregister dst
    //

    IR::Opnd *dst = instr->GetDst();

    if (dst == nullptr)
    {TRACE_IT(10228);
        return;
    }

    if (!dst->IsRegOpnd())
    {TRACE_IT(10229);
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
    {TRACE_IT(10230);
        RegNum callSetupReg = regOpnd->GetReg();
        callSetupRegs.Set(callSetupReg);
    }

    StackSym * stackSym = regOpnd->m_sym;

    // Arg slot sym can be in a RegOpnd for param passed via registers
    // Just use the assigned register
    if (stackSym == nullptr || stackSym->IsArgSlotSym())
    {TRACE_IT(10231);
        //
        // Already allocated register. just spill the destination
        //
        RegNum reg = regOpnd->GetReg();
        if(LinearScan::IsAllocatable(reg))
        {TRACE_IT(10232);
            this->SpillReg(reg);
        }
        this->tempRegs.Clear(reg);
    }
    else
    {TRACE_IT(10233);
        if (regOpnd->GetReg() != RegNOREG)
        {TRACE_IT(10234);
            this->RecordLoopUse(nullptr, regOpnd->GetReg());
            // Nothing to do
            return;
        }

        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;
        uint32 useCountCost = LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr));
        // Optimistically decrease the useCount.  We'll undo this if we put it on the defList.
        lifetime->SubFromUseCount(useCountCost, this->curLoop);

        if (lifetime->isSpilled)
        {TRACE_IT(10235);
            if (stackSym->IsConst() && !IsSymNonTempLocalVar(stackSym))
            {TRACE_IT(10236);
                // We will reload the constant (but in debug mode, we still need to process this if this is a user var).
                return;
            }

            RegNum reg = regOpnd->GetReg();

            if (reg != RegNOREG)
            {TRACE_IT(10237);
                // It is already assigned, just record it as a temp reg
                this->AssignTempReg(lifetime, reg);
            }
            else
            {TRACE_IT(10238);
                IR::Opnd *src1 = instr->GetSrc1();
                IR::Opnd *src2 = instr->GetSrc2();

                if ((src1 && src1->IsRegOpnd() && src1->AsRegOpnd()->m_sym == stackSym) ||
                    (src2 && src2->IsRegOpnd() && src2->AsRegOpnd()->m_sym == stackSym))
                {TRACE_IT(10239);
                    // OpEQ: src1 should have a valid reg (put src2 for other targets)
                    reg = this->GetAssignedTempReg(lifetime, dst->GetType());
                    Assert(reg != RegNOREG);
                    RecordDef(lifetime, instr, 0);
                }
                else
                {TRACE_IT(10240);
                    // Try second chance
                    reg = this->SecondChanceAllocation(lifetime, false);
                    if (reg != RegNOREG)
                    {TRACE_IT(10241);

                        Assert(!stackSym->m_isSingleDef);

                        this->SetReg(regOpnd);
                        // Keep track of defs for this lifetime, in case it gets spilled.
                        RecordDef(lifetime, instr, useCountCost);
                        return;
                    }
                    else
                    {TRACE_IT(10242);
                        reg = this->GetAssignedTempReg(lifetime, dst->GetType());
                        RecordDef(lifetime, instr, 0);
                    }
                }
                if (LowererMD::IsAssign(instr) && instr->GetSrc1()->IsRegOpnd())
                {TRACE_IT(10243);
                    // Fold the spilled store
                    if (reg != RegNOREG)
                    {TRACE_IT(10244);
                        // If the value is in a temp reg, it's not valid any more.
                        this->tempRegs.Clear(reg);
                    }

                    IRType srcType = instr->GetSrc1()->GetType();

                    instr->ReplaceDst(IR::SymOpnd::New(stackSym, srcType, this->func));
                    this->linearScanMD.LegalizeDef(instr);
                    return;
                }


                if (reg == RegNOREG)
                {TRACE_IT(10245);
                    IR::Opnd *src = instr->GetSrc1();
                    if (src && src->IsRegOpnd() && src->AsRegOpnd()->m_sym == stackSym)
                    {TRACE_IT(10246);
                        // Handle OPEQ's for x86/x64
                        reg = src->AsRegOpnd()->GetReg();
                        AssertMsg(!this->activeRegs.Test(reg), "Shouldn't be active");
                    }
                    else
                    {TRACE_IT(10247);
                        // The lifetime was spilled, but we still need a reg for this operand.
                        reg = this->FindReg(nullptr, regOpnd);
                    }
                    this->AssignTempReg(lifetime, reg);
                }
            }

            if (!lifetime->isDeadStore && !lifetime->isSecondChanceAllocated)
            {TRACE_IT(10248);
                // Insert a store since the lifetime is spilled
                IR::Opnd *nextDst = instr->m_next->GetDst();

                // Don't need the store however if the next instruction has the same dst
                if (nextDst == nullptr || !nextDst->IsEqual(regOpnd))
                {TRACE_IT(10249);
                    this->InsertStore(instr, regOpnd->m_sym, reg);
                }
            }
        }
        else
        {TRACE_IT(10250);
            if (lifetime->isOpHelperSpilled)
            {TRACE_IT(10251);
                // We must be in a helper block and the lifetime must
                // start before the helper block
                Assert(this->IsInHelperBlock());
                Assert(lifetime->start < this->HelperBlockStartInstrNumber());

                RegNum reg = lifetime->reg;
                Assert(this->opHelperSpilledRegs.Test(reg));

                if (this->activeRegs.Test(reg))
                {TRACE_IT(10252);
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
                {TRACE_IT(10253);
                    this->intRegUsedCount++;
                }
                else
                {TRACE_IT(10254);
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
{TRACE_IT(10255);
    int32 stackSlotId = regSlotId - this->func->GetJITFunctionBody()->GetFirstNonTempLocalIndex();
    Assert(stackSlotId >= 0);
    return this->func->GetLocalVarSlotOffset(stackSlotId);
}


//
// This helper function is used for saving bytecode stack sym value to memory / local slots on stack so that we can read it for the locals inspection.
void
LinearScan::WriteThroughForLocal(IR::RegOpnd* regOpnd, Lifetime* lifetime, IR::Instr* instrInsertAfter)
{TRACE_IT(10256);
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
{TRACE_IT(10257);
    return this->NeedsWriteThroughForEH(sym) || this->IsSymNonTempLocalVar(sym);
}

bool
LinearScan::NeedsWriteThroughForEH(StackSym * sym)
{TRACE_IT(10258);
    if (!this->func->HasTry() || !this->func->DoOptimizeTryCatch() || !sym->HasByteCodeRegSlot())
    {TRACE_IT(10259);
        return false;
    }

    Assert(this->currentRegion);
    return this->currentRegion->writeThroughSymbolsSet && this->currentRegion->writeThroughSymbolsSet->Test(sym->m_id);
}

// Helper routine to check if current sym belongs to non temp bytecodereg
bool
LinearScan::IsSymNonTempLocalVar(StackSym *sym)
{TRACE_IT(10260);
    Assert(sym);

    if (this->func->IsJitInDebugMode() && sym->HasByteCodeRegSlot())
    {TRACE_IT(10261);
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
{TRACE_IT(10262);
    //
    // Enregister srcs
    //

    IR::Opnd *src1 = instr->GetSrc1();

    if (src1 != nullptr)
    {TRACE_IT(10263);
        // Capture src2 now as folding in SetUses could swab the srcs...
        IR::Opnd *src2 = instr->GetSrc2();

        this->SetUses(instr, src1);

        if (src2 != nullptr)
        {TRACE_IT(10264);
            this->SetUses(instr, src2);
        }
    }

    IR::Opnd *dst = instr->GetDst();

    if (dst && dst->IsIndirOpnd())
    {TRACE_IT(10265);
        this->SetUses(instr, dst);
    }

    this->instrUseRegs.ClearAll();
}

// LinearScan::SetUses
void
LinearScan::SetUses(IR::Instr *instr, IR::Opnd *opnd)
{TRACE_IT(10266);
    switch (opnd->GetKind())
    {
    case IR::OpndKindReg:
        this->SetUse(instr, opnd->AsRegOpnd());
        break;

    case IR::OpndKindSym:
        {TRACE_IT(10267);
            Sym * sym = opnd->AsSymOpnd()->m_sym;
            if (sym->IsStackSym())
            {TRACE_IT(10268);
                StackSym* stackSym = sym->AsStackSym();
                if (!stackSym->IsAllocated())
                {TRACE_IT(10269);
                    func->StackAllocate(stackSym, opnd->GetSize());
                    // StackSym's lifetime is allocated during SCCLiveness::ProcessDst
                    // we might not need to set the flag if the sym is not a dst.
                    if (stackSym->scratch.linearScan.lifetime)
                    {TRACE_IT(10270);
                        stackSym->scratch.linearScan.lifetime->cantStackPack = true;
                    }
                }
                this->linearScanMD.LegalizeUse(instr, opnd);
            }
        }
        break;
    case IR::OpndKindIndir:
        {TRACE_IT(10271);
            IR::IndirOpnd * indirOpnd = opnd->AsIndirOpnd();

            this->SetUse(instr, indirOpnd->GetBaseOpnd());

            if (indirOpnd->GetIndexOpnd())
            {TRACE_IT(10272);
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

    FillBailOutState(JitArenaAllocator * allocator) : constantList(allocator) {TRACE_IT(10273);}
};


void
LinearScan::FillBailOutOffset(int * offset, StackSym * stackSym, FillBailOutState * state, IR::Instr * instr)
{TRACE_IT(10274);
    AssertMsg(*offset == 0, "Can't have two active lifetime for the same byte code register");
    if (stackSym->IsConst())
    {TRACE_IT(10275);
        state->constantList.Prepend(reinterpret_cast<Js::Var>(stackSym->GetLiteralConstValue_PostGlobOpt()));

        // Constant offset are offset by the number of register save slots
        *offset = state->constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();
    }
    else if (stackSym->m_isEncodedConstant)
    {TRACE_IT(10276);
        Assert(!stackSym->m_isSingleDef);
        state->constantList.Prepend((Js::Var)stackSym->constantValue);

        // Constant offset are offset by the number of register save slots
        *offset = state->constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();
    }
    else
    {TRACE_IT(10277);
        Lifetime * lifetime = stackSym->scratch.linearScan.lifetime;
        Assert(lifetime && lifetime->start < instr->GetNumber() && instr->GetNumber() <= lifetime->end);
        if (instr->GetBailOutKind() == IR::BailOutOnException)
        {TRACE_IT(10278);
            // Apart from the exception object sym, lifetimes for all other syms that need to be restored at this bailout,
            // must have been spilled at least once (at the TryCatch, or at the Leave, or both)
            // Post spilling, a lifetime could have been second chance allocated. But, it should still have stack allocated for its sym
            Assert(stackSym->IsAllocated() || (stackSym == this->currentRegion->GetExceptionObjectSym()));
        }

        this->PrepareForUse(lifetime);
        if (lifetime->isSpilled ||
            ((instr->GetBailOutKind() == IR::BailOutOnException) && (stackSym != this->currentRegion->GetExceptionObjectSym()))) // BailOutOnException must restore from memory
        {TRACE_IT(10279);
            Assert(stackSym->IsAllocated());
#ifdef MD_GROW_LOCALS_AREA_UP
            *offset = -((int)stackSym->m_offset + BailOutInfo::StackSymBias);
#else
            // Stack offset are negative, includes the PUSH EBP and return address
            *offset = stackSym->m_offset - (2 * MachPtr);
#endif
        }
        else
        {TRACE_IT(10280);
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
{TRACE_IT(10281);
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
{TRACE_IT(10282);
    Js::RegSlot localsCount = func->GetJITFunctionBody()->GetLocalsCount();

    Assert(globalBailOutRecordDataTable != nullptr);
    Assert(lastUpdatedRowIndices != nullptr);

    if (*lastUpdatedRowIndices == nullptr)
    {TRACE_IT(10283);
        *lastUpdatedRowIndices = JitAnewArrayZ(allocator, uint, localsCount);
        memset(*lastUpdatedRowIndices, -1, sizeof(uint)*localsCount);
    }
    uint32 bailOutRecordId = bailOutRecord->m_bailOutRecordId;
    bailOutRecord->localOffsetsCount = 0;
    for (uint32 i = 0; i < localsCount; i++)
    {TRACE_IT(10284);
        // if the sym is live
        if (localOffsets[i] != 0)
        {TRACE_IT(10285);
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
{TRACE_IT(10286);
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
{TRACE_IT(10287);
    Assert(globalBailOutRecordTables != nullptr);
    Func *topFunc = func->GetTopFunc();
    bool isTopFunc = (func == topFunc);
    uint32 inlineeID = isTopFunc ? 0 : func->m_inlineeId;
    NativeCodeData::Allocator * allocator = this->func->GetNativeCodeDataAllocator();

    GlobalBailOutRecordDataTable *globalBailOutRecordDataTable = globalBailOutRecordTables[inlineeID];
    if (globalBailOutRecordDataTable == nullptr)
    {TRACE_IT(10288);
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
        {TRACE_IT(10289);
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
        {TRACE_IT(10290);
            topFunc->GetScriptContext()->bailOutOffsetBytes += sizeof(GlobalBailOutRecordDataTable);
            topFunc->GetScriptContext()->bailOutRecordBytes += sizeof(GlobalBailOutRecordDataTable);
        }
#endif
    }
    return globalBailOutRecordDataTable;
}

void
LinearScan::FillBailOutRecord(IR::Instr * instr)
{TRACE_IT(10291);
    BailOutInfo * bailOutInfo = instr->GetBailOutInfo();

    if (this->func->HasTry())
    {TRACE_IT(10292);
        RegionType currentRegionType = this->currentRegion->GetType();
        if (currentRegionType == RegionTypeTry || currentRegionType == RegionTypeCatch)
        {TRACE_IT(10293);
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
    {TRACE_IT(10294);
        Output::Print(_u("-------------------Bailout dump -------------------------\n"));
        instr->Dump();
    }
#endif


    // Generate chained bailout record for inlined functions
    Func * currentFunc = bailOutFunc->GetParentFunc();
    uint bailOutOffset = bailOutFunc->postCallByteCodeOffset;
    while (currentFunc != nullptr)
    {TRACE_IT(10295);
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
    {TRACE_IT(10296);
        AssertMsg(bailOutInfo->bailOutRecord->bailOutKind != IR::BailOutForGeneratorYield, "constant prop syms unexpected for bail-in for generator yield");
        StackSym * stackSym = value.Key();
        if(stackSym->HasArgSlotNum())
        {TRACE_IT(10297);
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
        {TRACE_IT(10298);
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
    {TRACE_IT(10299);
        AssertMsg(bailOutInfo->bailOutRecord->bailOutKind != IR::BailOutForGeneratorYield, "copy prop syms unexpected for bail-in for generator yield");
        StackSym * stackSym = copyPropSyms.Key();
        if(stackSym->HasArgSlotNum())
        {TRACE_IT(10300);
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
        {TRACE_IT(10301);
            funcBailOutData[index].losslessInt32Syms->Set(i);
        }
        else if (copyStackSym->IsFloat64())
        {TRACE_IT(10302);
            funcBailOutData[index].float64Syms->Set(i);
        }
        // SIMD_JS
        else if (copyStackSym->IsSimd128F4())
        {TRACE_IT(10303);
            funcBailOutData[index].simd128F4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128I4())
        {TRACE_IT(10304);
            funcBailOutData[index].simd128I4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128I8())
        {TRACE_IT(10305);
            funcBailOutData[index].simd128I8Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128I16())
        {TRACE_IT(10306);
            funcBailOutData[index].simd128I16Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128U4())
        {TRACE_IT(10307);
            funcBailOutData[index].simd128U4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128U8())
        {TRACE_IT(10308);
            funcBailOutData[index].simd128U8Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128U16())
        {TRACE_IT(10309);
            funcBailOutData[index].simd128U16Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128B4())
        {TRACE_IT(10310);
            funcBailOutData[index].simd128B4Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128B8())
        {TRACE_IT(10311);
            funcBailOutData[index].simd128B8Syms->Set(i);
        }
        else if (copyStackSym->IsSimd128B16())
        {TRACE_IT(10312);
            funcBailOutData[index].simd128B16Syms->Set(i);
        }
        copyPropSymsIter.RemoveCurrent(this->func->m_alloc);
    }
    NEXT_SLISTBASE_ENTRY_EDITING;

    // Fill in the upward exposed syms
    FOREACH_BITSET_IN_SPARSEBV(id, byteCodeUpwardExposedUsed)
    {TRACE_IT(10313);
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
        {TRACE_IT(10314);
            funcBailOutData[index].losslessInt32Syms->Set(i);
        }
        else if (stackSym->IsFloat64())
        {TRACE_IT(10315);
            funcBailOutData[index].float64Syms->Set(i);
        }
        // SIMD_JS
        else if (stackSym->IsSimd128F4())
        {TRACE_IT(10316);
            funcBailOutData[index].simd128F4Syms->Set(i);
        }
        else if (stackSym->IsSimd128I4())
        {TRACE_IT(10317);
            funcBailOutData[index].simd128I4Syms->Set(i);
        }
        else if (stackSym->IsSimd128I8())
        {TRACE_IT(10318);
            funcBailOutData[index].simd128I8Syms->Set(i);
        }
        else if (stackSym->IsSimd128I16())
        {TRACE_IT(10319);
            funcBailOutData[index].simd128I16Syms->Set(i);
        }
        else if (stackSym->IsSimd128U4())
        {TRACE_IT(10320);
            funcBailOutData[index].simd128U4Syms->Set(i);
        }
        else if (stackSym->IsSimd128U8())
        {TRACE_IT(10321);
            funcBailOutData[index].simd128U8Syms->Set(i);
        }
        else if (stackSym->IsSimd128U16())
        {TRACE_IT(10322);
            funcBailOutData[index].simd128U16Syms->Set(i);
        }
        else if (stackSym->IsSimd128B4())
        {TRACE_IT(10323);
            funcBailOutData[index].simd128B4Syms->Set(i);
        }
        else if (stackSym->IsSimd128B8())
        {TRACE_IT(10324);
            funcBailOutData[index].simd128B8Syms->Set(i);
        }
        else if (stackSym->IsSimd128B16())
        {TRACE_IT(10325);
            funcBailOutData[index].simd128B16Syms->Set(i);
        }
    }
    NEXT_BITSET_IN_SPARSEBV;

    if (bailOutInfo->usedCapturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(id, bailOutInfo->usedCapturedValues.argObjSyms)
        {TRACE_IT(10326);
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
    {TRACE_IT(10327);
        // Need to allow filling the formal args slots.

        if (func->GetJITFunctionBody()->HasPropIdToFormalsMap())
        {TRACE_IT(10328);
            Assert(func->GetJITFunctionBody()->GetInParamsCount() > 0);
            uint32 endIndex = min(func->GetJITFunctionBody()->GetFirstNonTempLocalIndex() + func->GetJITFunctionBody()->GetInParamsCount() - 1, func->GetJITFunctionBody()->GetEndNonTempLocalIndex());
            for (uint32 index = func->GetJITFunctionBody()->GetFirstNonTempLocalIndex(); index < endIndex; index++)
            {TRACE_IT(10329);
                StackSym * stackSym = this->func->m_symTable->FindStackSym(index);
                if (stackSym != nullptr)
                {TRACE_IT(10330);
                    Func * stackSymFunc = stackSym->GetByteCodeFunc();

                    Js::RegSlot regSlotId = stackSym->GetByteCodeRegSlot();
                    if (func->IsNonTempLocalVar(regSlotId))
                    {TRACE_IT(10331);
                        if (!func->GetJITFunctionBody()->IsRegSlotFormal(regSlotId - func->GetJITFunctionBody()->GetFirstNonTempLocalIndex()))
                        {TRACE_IT(10332);
                            continue;
                        }

                        uint dataIndex = stackSymFunc->inlineDepth;
                        Assert(dataIndex == 0);     // There is no inlining while in debug mode

                        // Filling in which are not filled already.
                        __analysis_assume(dataIndex == 0);
                        if (funcBailOutData[dataIndex].localOffsets[regSlotId] == 0)
                        {TRACE_IT(10333);
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
    {TRACE_IT(10334);
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
        {TRACE_IT(10335);
            bailOutInfo->outParamInlinedArgSlot = JitAnew(this->func->m_alloc, BVSparse<JitArenaAllocator>, this->func->m_alloc);
        }

#if DBG
        uint lastFuncIndex = 0;
#endif
        for (uint i = 0; i < startCallCount; i++)
        {TRACE_IT(10336);
            uint outParamStart = argOutSlot;                     // Start of the out param offset for the current start call
            // Number of out param for the current start call
            uint outParamCount = bailOutInfo->GetStartCallOutParamCount(i);
            startCallOutParamCounts[i] = outParamCount;
#ifdef _M_IX86
            startCallArgRestoreAdjustCounts[i] = bailOutInfo->startCallInfo[i].argRestoreAdjustCount;
            // Only x86 has a progression of pushes of out args, with stack alignment.
            bool fDoStackAdjust = false;
            if (!bailOutInfo->inlinedStartCall->Test(i))
            {TRACE_IT(10337);
                // Only do the stack adjustment if the StartCall has not been moved down past the bailout.
                fDoStackAdjust = bailOutInfo->NeedsStartCallAdjust(i, instr);
                if (fDoStackAdjust)
                {TRACE_IT(10338);
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
            {TRACE_IT(10339);
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
                {TRACE_IT(10340);
                    this->func->GetScriptContext()->bailOutRecordBytes += sizeof(BailOutRecord::ArgOutOffsetInfo);
                }
#endif
            }

            currentBailOutRecord->argOutOffsetInfo->startCallCount++;
            if (currentBailOutRecord->argOutOffsetInfo->outParamOffsets == nullptr)
            {TRACE_IT(10341);
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
            {TRACE_IT(10342);
                Output::Print(_u("Bailout function: %s [#%d] \n"), currentStartCallFunc->GetJITFunctionBody()->GetDisplayName(),
                    currentStartCallFunc->GetJITFunctionBody()->GetFunctionNumber());
            }
#endif
            for (uint j = 0; j < outParamCount; j++, argOutSlot++)
            {TRACE_IT(10343);
                StackSym * sym = bailOutInfo->argOutSyms[argOutSlot];
                if (sym == nullptr)
                {TRACE_IT(10344);
                    // This can happen when instr with bailout occurs before all ArgOuts for current call instr are processed.
                    continue;
                }

                Assert(sym->GetArgSlotNum() > 0 && sym->GetArgSlotNum() <= outParamCount);
                uint argSlot = sym->GetArgSlotNum() - 1;
                uint outParamOffsetIndex = outParamStart + argSlot;
                if (!sym->m_isBailOutReferenced && !sym->IsArgSlotSym())
                {
                    FOREACH_SLISTBASE_ENTRY_EDITING(ConstantStackSymValue, constantValue, &bailOutInfo->usedCapturedValues.constantValues, iterator)
                    {TRACE_IT(10345);
                        if (constantValue.Key()->m_id == sym->m_id)
                        {TRACE_IT(10346);
                            Js::Var varValue = constantValue.Value().ToVar(func);
                            state.constantList.Prepend(varValue);
                            outParamOffsets[outParamOffsetIndex] = state.constantList.Count() + GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount();

#if DBG_DUMP
                            if (PHASE_DUMP(Js::BailOutPhase, this->func))
                            {TRACE_IT(10347);
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
                    {TRACE_IT(10348);
                        continue;
                    }

                    FOREACH_SLISTBASE_ENTRY_EDITING(CopyPropSyms, copyPropSym, &bailOutInfo->usedCapturedValues.copyPropSyms, iter)
                    {TRACE_IT(10349);
                        if (copyPropSym.Key()->m_id == sym->m_id)
                        {TRACE_IT(10350);
                            StackSym * copyStackSym = copyPropSym.Value();

                            BVSparse<JitArenaAllocator>* argObjSyms = bailOutInfo->usedCapturedValues.argObjSyms;
                            if (argObjSyms && argObjSyms->Test(copyStackSym->m_id))
                            {TRACE_IT(10351);
                                outParamOffsets[outParamOffsetIndex] = BailOutRecord::GetArgumentsObjectOffset();
                            }
                            else
                            {TRACE_IT(10352);
                                this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], copyStackSym, &state, instr);
                                if (copyStackSym->IsInt32())
                                {TRACE_IT(10353);
                                    argOutLosslessInt32Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsFloat64())
                                {TRACE_IT(10354);
                                    argOutFloat64Syms->Set(outParamOffsetIndex);
                                }
                                // SIMD_JS
                                else if (copyStackSym->IsSimd128F4())
                                {TRACE_IT(10355);
                                    argOutSimd128F4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128I4())
                                {TRACE_IT(10356);
                                    argOutSimd128I4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128I8())
                                {TRACE_IT(10357);
                                    argOutSimd128I8Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128I16())
                                {TRACE_IT(10358);
                                    argOutSimd128I16Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128U4())
                                {TRACE_IT(10359);
                                    argOutSimd128U4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128U8())
                                {TRACE_IT(10360);
                                    argOutSimd128U8Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128U16())
                                {TRACE_IT(10361);
                                    argOutSimd128U16Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128B4())
                                {TRACE_IT(10362);
                                    argOutSimd128B4Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128B8())
                                {TRACE_IT(10363);
                                    argOutSimd128B8Syms->Set(outParamOffsetIndex);
                                }
                                else if (copyStackSym->IsSimd128B16())
                                {TRACE_IT(10364);
                                    argOutSimd128B16Syms->Set(outParamOffsetIndex);
                                }
                            }
#if DBG_DUMP
                            if (PHASE_DUMP(Js::BailOutPhase, this->func))
                            {TRACE_IT(10365);
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
                {TRACE_IT(10366);
                    if (sym->IsArgSlotSym())
                    {TRACE_IT(10367);
                        if (sym->m_isSingleDef)
                        {TRACE_IT(10368);
                            Assert(sym->m_instrDef->m_func == currentStartCallFunc);

                            IR::Instr * instrDef = sym->m_instrDef;
                            Assert(LowererMD::IsAssign(instrDef));

                            if (instrDef->GetNumber() < instr->GetNumber())
                            {TRACE_IT(10369);
                                // The ArgOut instr is above current bailout instr.
                                AssertMsg(sym->IsVar(), "Arg out slot can only be var.");
                                if (sym->m_isInlinedArgSlot)
                                {TRACE_IT(10370);
                                    Assert(this->func->HasInlinee());
#ifdef MD_GROW_LOCALS_AREA_UP
                                    outParamOffsets[outParamOffsetIndex] = -((int)sym->m_offset + BailOutInfo::StackSymBias);
#else
                                    outParamOffsets[outParamOffsetIndex] = sym->m_offset;
#endif
                                    bailOutInfo->outParamInlinedArgSlot->Set(outParamOffsetIndex);
                                }
                                else if (sym->m_isOrphanedArg)
                                {TRACE_IT(10371);
#ifdef MD_GROW_LOCALS_AREA_UP
                                    outParamOffsets[outParamOffsetIndex] = -((int)sym->m_offset + BailOutInfo::StackSymBias);
#else
                                    // Stack offset are negative, includes the PUSH EBP and return address
                                    outParamOffsets[outParamOffsetIndex] = sym->m_offset - (2 * MachPtr);
#endif
                                }
#ifdef _M_IX86
                                else if (fDoStackAdjust)
                                {TRACE_IT(10372);
                                    // If we've got args on the stack, then we must have seen (and adjusted for) the StartCall.
                                    // The values is already on the stack
                                    // On AMD64/ARM, ArgOut should have been moved next to the call, and shouldn't have bailout between them
                                    // Except for inlined arg outs
                                    outParamOffsets[outParamOffsetIndex] = currentStackOffset + argSlot * MachPtr;
                                    bailOutInfo->outParamFrameAdjustArgSlot->Set(outParamOffsetIndex);
                                }
#endif
                                else
                                {TRACE_IT(10373);
                                    this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                                }
                            }
                            else
                            {TRACE_IT(10374);
                                // The ArgOut instruction might have moved down right next to the call,
                                // because of a register calling convention, cloning, etc.  This loop walks the chain
                                // of assignments to try to find the original location of the assignment where
                                // the value is available.
                                while (!sym->IsConst())
                                {TRACE_IT(10375);
                                    // the value is in the register
                                    IR::RegOpnd * regOpnd = instrDef->GetSrc1()->AsRegOpnd();
                                    sym = regOpnd->m_sym;

                                    if (sym->scratch.linearScan.lifetime->start < instr->GetNumber())
                                    {TRACE_IT(10376);
                                        break;
                                    }

                                    if (sym->m_isEncodedConstant)
                                    {TRACE_IT(10377);
                                        break;
                                    }
                                    // For out parameter we might need to follow multiple assignments
                                    Assert(sym->m_isSingleDef);
                                    instrDef = sym->m_instrDef;
                                    Assert(LowererMD::IsAssign(instrDef));
                                }

                                if (bailOutInfo->usedCapturedValues.argObjSyms && bailOutInfo->usedCapturedValues.argObjSyms->Test(sym->m_id))
                                {TRACE_IT(10378);
                                    //foo.apply(this,arguments) case and we bailout when the apply is overridden. We need to restore the arguments object.
                                    outParamOffsets[outParamOffsetIndex] = BailOutRecord::GetArgumentsObjectOffset();
                                }
                                else
                                {TRACE_IT(10379);
                                    this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                                }
                            }
                        }
                    }
                    else
                    {TRACE_IT(10380);
                        this->FillBailOutOffset(&outParamOffsets[outParamOffsetIndex], sym, &state, instr);
                    }

                    if (sym->IsFloat64())
                    {TRACE_IT(10381);
                        argOutFloat64Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsInt32())
                    {TRACE_IT(10382);
                        argOutLosslessInt32Syms->Set(outParamOffsetIndex);
                    }
                    // SIMD_JS
                    else if (sym->IsSimd128F4())
                    {TRACE_IT(10383);
                        argOutSimd128F4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128I4())
                    {TRACE_IT(10384);
                        argOutSimd128I4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128I8())
                    {TRACE_IT(10385);
                        argOutSimd128I8Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128I16())
                    {TRACE_IT(10386);
                        argOutSimd128I16Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128U4())
                    {TRACE_IT(10387);
                        argOutSimd128U4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128U8())
                    {TRACE_IT(10388);
                        argOutSimd128U8Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128U16())
                    {TRACE_IT(10389);
                        argOutSimd128U16Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128B4())
                    {TRACE_IT(10390);
                        argOutSimd128B4Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128B8())
                    {TRACE_IT(10391);
                        argOutSimd128B8Syms->Set(outParamOffsetIndex);
                    }
                    else if (sym->IsSimd128B16())
                    {TRACE_IT(10392);
                        argOutSimd128B16Syms->Set(outParamOffsetIndex);
                    }
#if DBG_DUMP
                    if (PHASE_DUMP(Js::BailOutPhase, this->func))
                    {TRACE_IT(10393);
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
    {TRACE_IT(10394);
        Assert(bailOutInfo->argOutSyms == nullptr);
        Assert(bailOutInfo->startCallCount == 0);
    }

    if (this->currentBlock->inlineeStack.Count() > 0)
    {TRACE_IT(10395);
        this->SpillInlineeArgs(instr);
    }
    else
    {TRACE_IT(10396);
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
    {TRACE_IT(10397);
        Js::Var value = state.constantList.Head();
        state.constantList.RemoveHead();
        constants[state.constantList.Count()] = value;
    }

    // Generate the stack literal bail out info
    FillStackLiteralBailOutRecord(instr, bailOutInfo, funcBailOutData, funcCount);

    for (uint i = 0; i < funcCount; i++)
    {TRACE_IT(10398);

        funcBailOutData[i].bailOutRecord->constants = constants;
#if DBG
        funcBailOutData[i].bailOutRecord->inlineDepth = funcBailOutData[i].func->inlineDepth;
        funcBailOutData[i].bailOutRecord->constantCount = constantCount;
#endif
        uint32 tableIndex = funcBailOutData[i].func->IsTopFunc() ? 0 : funcBailOutData[i].func->m_inlineeId;
        funcBailOutData[i].FinalizeLocalOffsets(tempAlloc, this->globalBailOutRecordTables[tableIndex], &(this->lastUpdatedRowIndices[tableIndex]));
#if DBG_DUMP
        if(PHASE_DUMP(Js::BailOutPhase, this->func))
        {TRACE_IT(10399);
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(_u("Bailout function: %s [%s]\n"), funcBailOutData[i].func->GetJITFunctionBody()->GetDisplayName(), funcBailOutData[i].func->GetDebugNumberSet(debugStringBuffer), i);
            funcBailOutData[i].bailOutRecord->Dump();
        }
#endif
        funcBailOutData[i].Clear(this->tempAlloc);

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
        if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
        {TRACE_IT(10400);
            this->func->GetScriptContext()->bailOutRecordBytes += sizeof(BailOutRecord);
        }
#endif
    }
    JitAdeleteArray(this->tempAlloc, funcCount, funcBailOutData);
}

template <typename Fn>
void
LinearScan::ForEachStackLiteralBailOutInfo(IR::Instr * instr, BailOutInfo * bailOutInfo, FuncBailOutData * funcBailOutData, uint funcCount, Fn fn)
{TRACE_IT(10401);
    for (uint i = 0; i < bailOutInfo->stackLiteralBailOutInfoCount; i++)
    {TRACE_IT(10402);
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
{TRACE_IT(10403);
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
        {TRACE_IT(10404);
            uint stackLiteralBailOutRecordCount = funcBailOutData[i].bailOutRecord->stackLiteralBailOutRecordCount;
            if (stackLiteralBailOutRecordCount)
            {TRACE_IT(10405);
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
{TRACE_IT(10406);
    if (lifetime->isOpHelperSpilled)
    {TRACE_IT(10407);
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
{TRACE_IT(10408);
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
    {TRACE_IT(10409);
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
    {TRACE_IT(10410);
        if (!isFromBailout)
        {TRACE_IT(10411);
            // Since we won't reload this use if the lifetime gets spilled, adjust the spill cost to reflect this.
            lifetime->SubFromUseCount(useCountCost, this->curLoop);
        }
    }
    if (this->IsInLoop())
    {TRACE_IT(10412);
        this->RecordLoopUse(lifetime, lifetime->reg);
    }
}

void LinearScan::RecordLoopUse(Lifetime *lifetime, RegNum reg)
{TRACE_IT(10413);
    if (!this->IsInLoop())
    {TRACE_IT(10414);
        return;
    }

    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {TRACE_IT(10415);
        return;
    }

    // Record on each loop which register live into the loop ended up being used.
    // We are trying to avoid the need for compensation at the bottom of the loop if
    // the reg ends up being spilled before it is actually used.
    Loop *curLoop = this->curLoop;
    SymID symId = (SymID)-1;

    if (lifetime)
    {TRACE_IT(10416);
        symId = lifetime->sym->m_id;
    }

    while (curLoop)
    {TRACE_IT(10417);
        // Note that if the lifetime is spilled and reallocated to the same register,
        // will mark it as used when we shouldn't.  However, it is hard at this point to handle
        // the case were a flow edge from the previous allocation merges in with the new allocation.
        // No compensation is inserted to let us know with previous lifetime needs reloading at the bottom of the loop...
        if (lifetime && curLoop->regAlloc.loopTopRegContent[reg] == lifetime)
        {TRACE_IT(10418);
            curLoop->regAlloc.symRegUseBv->Set(symId);
        }
        curLoop->regAlloc.regUseBv.Set(reg);
        curLoop = curLoop->parent;
    }
}

void
LinearScan::RecordDef(Lifetime *const lifetime, IR::Instr *const instr, const uint32 useCountCost)
{TRACE_IT(10419);
    Assert(lifetime);
    Assert(instr);
    Assert(instr->GetDst());

    IR::RegOpnd * regOpnd = instr->GetDst()->AsRegOpnd();
    Assert(regOpnd);

    StackSym *const sym = regOpnd->m_sym;

    if (this->IsInLoop())
    {TRACE_IT(10420);
        Loop *curLoop = this->curLoop;

        while (curLoop)
        {TRACE_IT(10421);
            curLoop->regAlloc.defdInLoopBv->Set(lifetime->sym->m_id);
            curLoop->regAlloc.regUseBv.Set(lifetime->reg);
            curLoop = curLoop->parent;
        }
    }

    if (lifetime->isSpilled)
    {TRACE_IT(10422);
        return;
    }

    if (this->NeedsWriteThrough(sym))
    {TRACE_IT(10423);
        if (this->IsSymNonTempLocalVar(sym))
        {
            // In the debug mode, we will write through on the stack location.
            WriteThroughForLocal(regOpnd, lifetime, instr);
        }
        else
        {TRACE_IT(10424);
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
    {TRACE_IT(10425);
        lifetime->AddToUseCount(useCountCost, this->curLoop, this->func);
        // the def of a single-def sym is already on the sym
        return;
    }

    if(lifetime->previousDefBlockNumber == currentBlockNumber && !lifetime->defList.Empty())
    {TRACE_IT(10426);
        // Only keep track of the last def in each basic block. When there are multiple defs of a sym in a basic block, upon
        // spill of that sym, a store needs to be inserted only after the last def of the sym.
        Assert(lifetime->defList.Head()->GetDst()->AsRegOpnd()->m_sym == sym);
        lifetime->defList.Head() = instr;
    }
    else
    {TRACE_IT(10427);
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
{TRACE_IT(10428);
    if (regOpnd->GetReg() != RegNOREG)
    {TRACE_IT(10429);
        this->RecordLoopUse(nullptr, regOpnd->GetReg());
        return;
    }

    StackSym *sym = regOpnd->m_sym;
    Lifetime * lifetime = sym->scratch.linearScan.lifetime;

    this->PrepareForUse(lifetime);

    if (lifetime->isSpilled)
    {TRACE_IT(10430);
        // See if it has been loaded in this basic block
        RegNum reg = this->GetAssignedTempReg(lifetime, regOpnd->GetType());
        if (reg == RegNOREG)
        {TRACE_IT(10431);
            if (sym->IsConst() && EncoderMD::TryConstFold(instr, regOpnd))
            {TRACE_IT(10432);
                return;
            }

            reg = this->SecondChanceAllocation(lifetime, false);
            if (reg != RegNOREG)
            {TRACE_IT(10433);
                IR::Instr *insertInstr = this->TryHoistLoad(instr, lifetime);
                this->InsertLoad(insertInstr, sym, reg);
            }
            else
            {TRACE_IT(10434);
                // Try folding if there are no registers available
                if (!sym->IsConst() && !this->RegsAvailable(regOpnd->GetType()) && EncoderMD::TryFold(instr, regOpnd))
                {TRACE_IT(10435);
                    return;
                }

                // We need a reg no matter what.  Try to force second chance to re-allocate this.
                reg = this->SecondChanceAllocation(lifetime, true);

                if (reg == RegNOREG)
                {TRACE_IT(10436);
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
    {TRACE_IT(10437);
        // Don't border to record the use if this is the last use of the lifetime.
        this->RecordUse(lifetime, instr, regOpnd);
    }
    else
    {TRACE_IT(10438);
        lifetime->SubFromUseCount(LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr)), this->curLoop);
    }
    this->instrUseRegs.Set(lifetime->reg);

    this->SetReg(regOpnd);
}

// LinearScan::SetReg
void
LinearScan::SetReg(IR::RegOpnd *regOpnd)
{TRACE_IT(10439);
    if (regOpnd->GetReg() == RegNOREG)
    {TRACE_IT(10440);
        RegNum reg = regOpnd->m_sym->scratch.linearScan.lifetime->reg;
        AssertMsg(reg != RegNOREG, "Reg should be allocated here...");
        regOpnd->SetReg(reg);
    }
}

bool
LinearScan::SkipNumberedInstr(IR::Instr *instr)
{TRACE_IT(10441);
    if (instr->IsLabelInstr())
    {TRACE_IT(10442);
        if (instr->AsLabelInstr()->m_isLoopTop)
        {TRACE_IT(10443);
            Assert(instr->GetNumber() != instr->m_next->GetNumber()
                && (instr->GetNumber() != instr->m_prev->GetNumber() || instr->m_prev->m_opcode == Js::OpCode::Nop));
        }
        else
        {TRACE_IT(10444);
            return true;
        }
    }
    return false;
}

// LinearScan::EndDeadLifetimes
// Look for lifetimes that are ending here, and retire them.
void
LinearScan::EndDeadLifetimes(IR::Instr *instr)
{TRACE_IT(10445);
    Lifetime * deadLifetime;

    if (this->SkipNumberedInstr(instr))
    {TRACE_IT(10446);
        return;
    }

    // Retire all active lifetime ending at this instruction
    while (!this->activeLiveranges->Empty() && this->activeLiveranges->Head()->end <= instr->GetNumber())
    {TRACE_IT(10447);
        deadLifetime = this->activeLiveranges->Head();
        deadLifetime->defList.Clear();
        deadLifetime->useList.Clear();

        this->activeLiveranges->RemoveHead();
        RegNum reg = deadLifetime->reg;
        this->activeRegs.Clear(reg);
        this->regContent[reg] = nullptr;
        this->secondChanceRegs.Clear(reg);
        if (RegTypes[reg] == TyMachReg)
        {TRACE_IT(10448);
            this->intRegUsedCount--;
        }
        else
        {TRACE_IT(10449);
            Assert(RegTypes[reg] == TyFloat64);
            this->floatRegUsedCount--;
        }
    }

    // Look for spilled lifetimes which end here such that we can make their stack slot
    // available for stack-packing.
    while (!this->stackPackInUseLiveRanges->Empty() && this->stackPackInUseLiveRanges->Head()->end <= instr->GetNumber())
    {TRACE_IT(10450);
        deadLifetime = this->stackPackInUseLiveRanges->Head();
        deadLifetime->defList.Clear();
        deadLifetime->useList.Clear();

        this->stackPackInUseLiveRanges->RemoveHead();
        if (!deadLifetime->cantStackPack)
        {TRACE_IT(10451);
            Assert(deadLifetime->spillStackSlot);
            deadLifetime->spillStackSlot->lastUse = deadLifetime->end;
            this->stackSlotsFreeList->Push(deadLifetime->spillStackSlot);
        }
    }
}

void
LinearScan::EndDeadOpHelperLifetimes(IR::Instr * instr)
{TRACE_IT(10452);
    if (this->SkipNumberedInstr(instr))
    {TRACE_IT(10453);
        return;
    }

    while (!this->opHelperSpilledLiveranges->Empty() &&
           this->opHelperSpilledLiveranges->Head()->end <= instr->GetNumber())
    {TRACE_IT(10454);
        Lifetime * deadLifetime;

        // The lifetime doesn't extend beyond the helper block
        // No need to save and restore around the helper block
        Assert(this->IsInHelperBlock());

        deadLifetime = this->opHelperSpilledLiveranges->Head();
        this->opHelperSpilledLiveranges->RemoveHead();
        if (!deadLifetime->cantOpHelperSpill)
        {TRACE_IT(10455);
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
{TRACE_IT(10456);
    if (this->SkipNumberedInstr(instr))
    {TRACE_IT(10457);
        return;
    }

    // Try to catch:
    //      x = MOV y(r1)
    // where y's lifetime just ended and x's lifetime is starting.
    // If so, set r1 as a preferred register for x, which may allow peeps to remove the MOV
    if (instr->GetSrc1() && instr->GetSrc1()->IsRegOpnd() && LowererMD::IsAssign(instr) && instr->GetDst() && instr->GetDst()->IsRegOpnd() && instr->GetDst()->AsRegOpnd()->m_sym)
    {TRACE_IT(10458);
        IR::RegOpnd *src = instr->GetSrc1()->AsRegOpnd();
        StackSym *srcSym = src->m_sym;
        // If src is a physReg ref, or src's lifetime ends here.
        if (!srcSym || srcSym->scratch.linearScan.lifetime->end == instr->GetNumber())
        {TRACE_IT(10459);
            Lifetime *dstLifetime = instr->GetDst()->AsRegOpnd()->m_sym->scratch.linearScan.lifetime;
            if (dstLifetime)
            {TRACE_IT(10460);
                dstLifetime->regPreference.Set(src->GetReg());
            }
        }
    }

    // Look for starting lifetimes
    while (!this->lifetimeList->Empty() && this->lifetimeList->Head()->start <= instr->GetNumber())
    {TRACE_IT(10461);
        // We're at the start of a new live range

        Lifetime * newLifetime = this->lifetimeList->Head();
        newLifetime->lastAllocationStart = instr->GetNumber();

        this->lifetimeList->RemoveHead();

        if (newLifetime->dontAllocate)
        {TRACE_IT(10462);
            // Lifetime spilled before beginning allocation (e.g., a lifetime known to span
            // multiple EH regions.) Do the work of spilling it now without adding it to the list.
            this->SpillLiveRange(newLifetime);
            continue;
        }

        RegNum reg;
        if (newLifetime->reg == RegNOREG)
        {TRACE_IT(10463);
            if (newLifetime->isDeadStore)
            {TRACE_IT(10464);
                // No uses, let's not waste a reg.
                newLifetime->isSpilled = true;
                continue;
            }
            reg = this->FindReg(newLifetime, nullptr);
        }
        else
        {TRACE_IT(10465);
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
        {TRACE_IT(10466);
            this->AssignActiveReg(newLifetime, reg);
        }
    }
}

// LinearScan::FindReg
// Look for an available register.  If one isn't available, spill something.
// Note that the newLifetime passed in could be the one we end up spilling.
RegNum
LinearScan::FindReg(Lifetime *newLifetime, IR::RegOpnd *regOpnd, bool force)
{TRACE_IT(10467);
    BVIndex regIndex = BVInvalidIndex;
    IRType type;
    bool tryCallerSavedRegs = false;
    BitVector callerSavedAvailableBv;

    if (newLifetime)
    {TRACE_IT(10468);
        if (newLifetime->isFloat)
        {TRACE_IT(10469);
            type = TyFloat64;
        }
        else if (newLifetime->isSimd128F4)
        {TRACE_IT(10470);
            type = TySimd128F4;
        }
        else if (newLifetime->isSimd128I4)
        {TRACE_IT(10471);
            type = TySimd128I4;
        }
        else if (newLifetime->isSimd128I8)
        {TRACE_IT(10472);
            type = TySimd128I8;
        }
        else if (newLifetime->isSimd128I16)
        {TRACE_IT(10473);
            type = TySimd128I16;
        }
        else if (newLifetime->isSimd128U4)
        {TRACE_IT(10474);
            type = TySimd128U4;
        }
        else if (newLifetime->isSimd128U8)
        {TRACE_IT(10475);
            type = TySimd128U8;
        }
        else if (newLifetime->isSimd128U16)
        {TRACE_IT(10476);
            type = TySimd128U16;
        }
        else if (newLifetime->isSimd128B4)
        {TRACE_IT(10477);
            type = TySimd128B4;
        }
        else if (newLifetime->isSimd128B8)
        {TRACE_IT(10478);
            type = TySimd128B8;
        }
        else if (newLifetime->isSimd128B16)
        {TRACE_IT(10479);
            type = TySimd128B16;
        }
        else if (newLifetime->isSimd128D2)
        {TRACE_IT(10480);
            type = TySimd128D2;
        }
        else
        {TRACE_IT(10481);
            type = TyMachReg;
        }
    }
    else
    {TRACE_IT(10482);
        Assert(regOpnd);
        type = regOpnd->GetType();
    }

    if (this->RegsAvailable(type))
    {TRACE_IT(10483);
        BitVector regsBv;
        regsBv.Copy(this->activeRegs);
        regsBv.Or(this->instrUseRegs);
        regsBv.Or(this->callSetupRegs);
        regsBv.ComplimentAll();

        if (newLifetime)
        {TRACE_IT(10484);
            if (this->IsInHelperBlock())
            {TRACE_IT(10485);
                if (newLifetime->end >= this->HelperBlockEndInstrNumber())
                {TRACE_IT(10486);
                    // this lifetime goes beyond the helper function
                    // We need to exclude the helper spilled register as well.
                    regsBv.Minus(this->opHelperSpilledRegs);
                }
            }

            if (newLifetime->isFloat || newLifetime->isSimd128())
            {TRACE_IT(10487);
#ifdef _M_IX86
                Assert(AutoSystemInfo::Data.SSE2Available());
#endif
                regsBv.And(this->floatRegs);
            }
            else
            {TRACE_IT(10488);
                regsBv.And(this->int32Regs);
                regsBv = this->linearScanMD.FilterRegIntSizeConstraints(regsBv, newLifetime->intUsageBv);
            }


            if (newLifetime->isLiveAcrossCalls)
            {TRACE_IT(10489);
                // Try to find a callee saved regs
                BitVector regsBvTemp = regsBv;
                regsBvTemp.And(this->calleeSavedRegs);

                regIndex = GetPreferencedRegIndex(newLifetime, regsBvTemp);

                if (regIndex == BVInvalidIndex)
                {TRACE_IT(10490);
                    if (!newLifetime->isLiveAcrossUserCalls)
                    {TRACE_IT(10491);
                        // No callee saved regs is found and the lifetime only across helper
                        // calls, we can also use a caller saved regs to make use of the
                        // save and restore around helper blocks
                        regIndex = GetPreferencedRegIndex(newLifetime, regsBv);
                    }
                    else
                    {TRACE_IT(10492);
                        // If we can't find a callee-saved reg, we can try using a caller-saved reg instead.
                        // We'll hopefully get a few loads enregistered that way before we get to the call.
                        tryCallerSavedRegs = true;
                        callerSavedAvailableBv = regsBv;
                    }
                }
            }
            else
            {TRACE_IT(10493);
                regIndex = GetPreferencedRegIndex(newLifetime, regsBv);
            }
        }
        else
        {
            AssertMsg(regOpnd, "Need a lifetime or a regOpnd passed in");

            if (regOpnd->IsFloat() || regOpnd->IsSimd128())
            {TRACE_IT(10494);
#ifdef _M_IX86
                Assert(AutoSystemInfo::Data.SSE2Available());
#endif
                regsBv.And(this->floatRegs);
            }
            else
            {TRACE_IT(10495);
                regsBv.And(this->int32Regs);
                BitVector regSizeBv;
                regSizeBv.ClearAll();
                regSizeBv.Set(TySize[regOpnd->GetType()]);

                regsBv = this->linearScanMD.FilterRegIntSizeConstraints(regsBv, regSizeBv);
            }

            if (!this->tempRegs.IsEmpty())
            {TRACE_IT(10496);
                // avoid the temp reg that we have loaded in this basic block
                BitVector regsBvTemp = regsBv;
                regsBvTemp.Minus(this->tempRegs);
                regIndex = regsBvTemp.GetPrevBit();
            }

            if (regIndex == BVInvalidIndex)
            {TRACE_IT(10497);
                // allocate a temp reg from the other end of the bit vector so that it can
                // keep live for longer.
                regIndex = regsBv.GetPrevBit();
            }
        }
    }

    RegNum reg;

    if (BVInvalidIndex != regIndex)
    {TRACE_IT(10498);
        Assert(regIndex < RegNumCount);
        reg = (RegNum)regIndex;
    }
    else
    {TRACE_IT(10499);
        if (tryCallerSavedRegs)
        {TRACE_IT(10500);
            Assert(newLifetime);
            regIndex = GetPreferencedRegIndex(newLifetime, callerSavedAvailableBv);
            if (BVInvalidIndex == regIndex)
            {TRACE_IT(10501);
                tryCallerSavedRegs = false;
            }
        }

        bool dontSpillCurrent = tryCallerSavedRegs;

        if (newLifetime && newLifetime->isSpilled)
        {TRACE_IT(10502);
            // Second chance allocation
            dontSpillCurrent = true;
        }

        // Can't find reg, spill some lifetime.
        reg = this->Spill(newLifetime, regOpnd, dontSpillCurrent, force);

        if (reg == RegNOREG && tryCallerSavedRegs)
        {TRACE_IT(10503);
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
{TRACE_IT(10504);
    BitVector freePreferencedRegs = freeRegs;

    freePreferencedRegs.And(lifetime->regPreference);

    // If one of the preferred register (if any) is available, use it.  Otherwise, just pick one of free register.
    if (!freePreferencedRegs.IsEmpty())
    {TRACE_IT(10505);
        return freePreferencedRegs.GetNextBit();
    }
    else
    {TRACE_IT(10506);
        return freeRegs.GetNextBit();
    }
}


// LinearScan::Spill
// We need to spill something to free up a reg. If the newLifetime
// past in isn't NULL, we can spill this one instead of an active one.
RegNum
LinearScan::Spill(Lifetime *newLifetime, IR::RegOpnd *regOpnd, bool dontSpillCurrent, bool force)
{TRACE_IT(10507);
    uint minSpillCost = (uint)-1;

    Assert(!newLifetime || !regOpnd || newLifetime->isFloat == (regOpnd->GetType() == TyMachDouble) || newLifetime->isSimd128() == (regOpnd->IsSimd128()));
    bool isFloatReg;
    BitVector intUsageBV;
    bool needCalleeSaved;

    // For now, we just spill the lifetime with the lowest spill cost.
    if (newLifetime)
    {TRACE_IT(10508);
        isFloatReg = newLifetime->isFloat || newLifetime->isSimd128();

        if (!force)
        {TRACE_IT(10509);
            minSpillCost = this->GetSpillCost(newLifetime);
        }
        intUsageBV = newLifetime->intUsageBv;
        needCalleeSaved = newLifetime->isLiveAcrossUserCalls;
    }
    else
    {TRACE_IT(10510);
        needCalleeSaved = false;
        if (regOpnd->IsFloat() || regOpnd->IsSimd128())
        {TRACE_IT(10511);
            isFloatReg = true;
        }
        else
        {TRACE_IT(10512);
            // Filter for int reg size constraints
            isFloatReg = false;
            intUsageBV.ClearAll();
            intUsageBV.Set(TySize[regOpnd->GetType()]);
        }
    }

    SList<Lifetime *>::EditingIterator candidate;
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, this->activeLiveranges, iter)
    {TRACE_IT(10513);
        uint spillCost = this->GetSpillCost(lifetime);
        if (spillCost < minSpillCost                        &&
            this->instrUseRegs.Test(lifetime->reg) == false &&
            (lifetime->isFloat || lifetime->isSimd128()) == isFloatReg  &&
            !lifetime->cantSpill                            &&
            (!needCalleeSaved || this->calleeSavedRegs.Test(lifetime->reg)) &&
            this->linearScanMD.FitRegIntSizeConstraints(lifetime->reg, intUsageBV))
        {TRACE_IT(10514);
            minSpillCost = spillCost;
            candidate = iter;
        }
    } NEXT_SLIST_ENTRY_EDITING;
    AssertMsg(newLifetime || candidate.IsValid(), "Didn't find anything to spill?!?");

    Lifetime * spilledRange;
    if (candidate.IsValid())
    {TRACE_IT(10515);
        spilledRange = candidate.Data();
        candidate.RemoveCurrent();

        this->activeRegs.Clear(spilledRange->reg);
        if (spilledRange->isFloat || spilledRange->isSimd128())
        {TRACE_IT(10516);
            this->floatRegUsedCount--;
        }
        else
        {TRACE_IT(10517);
            this->intRegUsedCount--;
        }
    }
    else if (dontSpillCurrent)
    {TRACE_IT(10518);
        return RegNOREG;
    }
    else
    {TRACE_IT(10519);
        spilledRange = newLifetime;
    }

    return this->SpillLiveRange(spilledRange);
}

// LinearScan::SpillLiveRange
RegNum
LinearScan::SpillLiveRange(Lifetime * spilledRange, IR::Instr *insertionInstr)
{TRACE_IT(10520);
    Assert(!spilledRange->isSpilled);

    RegNum reg = spilledRange->reg;
    StackSym *sym = spilledRange->sym;

    spilledRange->isSpilled = true;
    spilledRange->isCheapSpill = false;
    spilledRange->reg = RegNOREG;

    // Don't allocate stack space for const, we always reload them. (For debugm mode, allocate on the stack)
    if (!sym->IsAllocated() && (!sym->IsConst() || IsSymNonTempLocalVar(sym)))
    {TRACE_IT(10521);
       this->AllocateStackSpace(spilledRange);
    }

    // No need to insert loads or stores if there are no uses.
    if (!spilledRange->isDeadStore)
    {TRACE_IT(10522);
        // In the debug mode, don't do insertstore for this stacksym, as we want to retain the IsConst for the sym,
        // and later we are going to find the reg for it.
        if (!IsSymNonTempLocalVar(sym))
        {TRACE_IT(10523);
            this->InsertStores(spilledRange, reg, insertionInstr);
        }

        if (this->IsInLoop() || sym->IsConst())
        {TRACE_IT(10524);
            this->InsertLoads(sym, reg);
        }
        else
        {TRACE_IT(10525);
            sym->scratch.linearScan.lifetime->useList.Clear();
        }
        // Adjust useCount in case of second chance allocation
        spilledRange->ApplyUseCountAdjust(this->curLoop);
    }

    Assert(reg == RegNOREG || spilledRange->reg == RegNOREG || this->regContent[reg] == spilledRange);
    if (spilledRange->isSecondChanceAllocated)
    {TRACE_IT(10526);
        Assert(reg == RegNOREG || spilledRange->reg == RegNOREG
            || (this->regContent[reg] == spilledRange && this->secondChanceRegs.Test(reg)));
        this->secondChanceRegs.Clear(reg);
        spilledRange->isSecondChanceAllocated = false;
    }
    else
    {TRACE_IT(10527);
        Assert(!this->secondChanceRegs.Test(reg));
    }
    this->regContent[reg] = nullptr;

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {TRACE_IT(10528);
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
{TRACE_IT(10529);
    Lifetime *spilledRange = nullptr;
    if (activeRegs.Test(reg))
    {TRACE_IT(10530);
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
        {TRACE_IT(10531);
            if (lifetime->reg == reg)
            {TRACE_IT(10532);
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
    {TRACE_IT(10533);
        return;
    }

    AnalysisAssert(spilledRange);
    Assert(!spilledRange->cantSpill);

    if ((!forceSpill) && this->IsInHelperBlock() && spilledRange->start < this->HelperBlockStartInstrNumber() && !spilledRange->cantOpHelperSpill)
    {TRACE_IT(10534);
        // if the lifetime starts before the helper block, we can do save and restore
        // around the helper block instead.

        this->AddOpHelperSpilled(spilledRange);
    }
    else
    {TRACE_IT(10535);
        if (spilledRange->cantOpHelperSpill)
        {TRACE_IT(10536);
            // We're really spilling this liverange, so take it out of the helper-spilled liveranges
            // to avoid confusion (see Win8 313433).
            Assert(!spilledRange->isOpHelperSpilled);
            spilledRange->cantOpHelperSpill = false;
            this->opHelperSpilledLiveranges->Remove(spilledRange);
        }
        this->SpillLiveRange(spilledRange);
    }

    if (this->activeRegs.Test(reg))
    {TRACE_IT(10537);
        this->activeRegs.Clear(reg);
        if (RegTypes[reg] == TyMachReg)
        {TRACE_IT(10538);
            this->intRegUsedCount--;
        }
        else
        {TRACE_IT(10539);
            Assert(RegTypes[reg] == TyFloat64);
            this->floatRegUsedCount--;
        }
    }
}

void
LinearScan::ProcessEHRegionBoundary(IR::Instr * instr)
{TRACE_IT(10540);
    Assert(instr->IsBranchInstr());
    Assert(instr->m_opcode != Js::OpCode::TryFinally); // finallys are not supported for optimization yet.
    if (instr->m_opcode != Js::OpCode::TryCatch && instr->m_opcode != Js::OpCode::Leave)
    {TRACE_IT(10541);
        return;
    }

    // Spill everything upon entry to the try region and upon a Leave.
    IR::Instr* insertionInstr = instr->m_opcode != Js::OpCode::Leave ? instr : instr->m_prev;
    FOREACH_SLIST_ENTRY_EDITING(Lifetime *, lifetime, this->activeLiveranges, iter)
    {TRACE_IT(10542);
        this->activeRegs.Clear(lifetime->reg);
        if (lifetime->isFloat || lifetime->isSimd128())
        {TRACE_IT(10543);
            this->floatRegUsedCount--;
        }
        else
        {TRACE_IT(10544);
            this->intRegUsedCount--;
        }
        this->SpillLiveRange(lifetime, insertionInstr);
        iter.RemoveCurrent();
    }
    NEXT_SLIST_ENTRY_EDITING;
}

void
LinearScan::AllocateStackSpace(Lifetime *spilledRange)
{TRACE_IT(10545);
    if (spilledRange->sym->IsAllocated())
    {TRACE_IT(10546);
        return;
    }

    uint32 size = TySize[spilledRange->sym->GetType()];

    // For the bytecodereg syms instead of spilling to the any other location lets re-use the already created slot.
    if (IsSymNonTempLocalVar(spilledRange->sym))
    {TRACE_IT(10547);
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
        {TRACE_IT(10548);
            // Heuristic: should we use '==' or '>=' for the size?
            if (slot->lastUse <= spilledRange->start && slot->size >= size)
            {TRACE_IT(10549);
                StackSym *spilledSym = spilledRange->sym;

                Assert(!spilledSym->IsArgSlotSym() && !spilledSym->IsParamSlotSym());
                Assert(!spilledSym->IsAllocated());
                spilledRange->spillStackSlot = slot;
                spilledSym->m_offset = slot->offset;
                spilledSym->m_allocated = true;

                iter.RemoveCurrent();

#if DBG_DUMP
                if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::StackPackPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
                {TRACE_IT(10550);
                    spilledSym->Dump();
                    Output::Print(_u(" *** stack packed at offset %3d  (%4d - %4d)\n"), spilledSym->m_offset, spilledRange->start, spilledRange->end);
                }
#endif
                break;
            }
        } NEXT_SLIST_ENTRY_EDITING;

        if (spilledRange->spillStackSlot == nullptr)
        {TRACE_IT(10551);
            newStackSlot = JitAnewStruct(this->tempAlloc, StackSlot);
            newStackSlot->size = size;
            spilledRange->spillStackSlot = newStackSlot;
        }
        this->AddLiveRange(this->stackPackInUseLiveRanges, spilledRange);
    }

    if (!spilledRange->sym->IsAllocated())
    {TRACE_IT(10552);
        // Can't stack pack, allocate new stack slot.
        StackSym *spilledSym = spilledRange->sym;
        this->func->StackAllocate(spilledSym, size);

#if DBG_DUMP
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::StackPackPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {TRACE_IT(10553);
            spilledSym->Dump();
            Output::Print(_u(" at offset %3d  (%4d - %4d)\n"), spilledSym->m_offset, spilledRange->start, spilledRange->end);
        }
#endif
        if (newStackSlot != nullptr)
        {TRACE_IT(10554);
            newStackSlot->offset = spilledSym->m_offset;
        }
    }
}

// LinearScan::InsertLoads
void
LinearScan::InsertLoads(StackSym *sym, RegNum reg)
{TRACE_IT(10555);
    Lifetime *lifetime = sym->scratch.linearScan.lifetime;

    FOREACH_SLIST_ENTRY(IR::Instr *, instr, &lifetime->useList)
    {TRACE_IT(10556);
        this->InsertLoad(instr, sym, reg);
    } NEXT_SLIST_ENTRY;

    lifetime->useList.Clear();
}

// LinearScan::InsertStores
void
LinearScan::InsertStores(Lifetime *lifetime, RegNum reg, IR::Instr *insertionInstr)
{TRACE_IT(10557);
    StackSym *sym = lifetime->sym;

    // If single def, use instrDef on the symbol
    if (sym->m_isSingleDef)
    {TRACE_IT(10558);
        IR::Instr * defInstr = sym->m_instrDef;
        if ((!sym->IsConst() && defInstr->GetDst()->AsRegOpnd()->GetReg() == RegNOREG)
            || this->secondChanceRegs.Test(reg))
        {TRACE_IT(10559);
            // This can happen if we were trying to allocate this lifetime,
            // and it is getting spilled right away.
            // For second chance allocations, this should have already been handled.

            return;
        }
        this->InsertStore(defInstr, defInstr->FindRegDef(sym)->m_sym, reg);
        return;
    }

    if (reg == RegNOREG)
    {TRACE_IT(10560);
        return;
    }

    uint localStoreCost = LinearScan::GetUseSpillCost(this->loopNest, (this->currentOpHelperBlock != nullptr));

    // Is it cheaper to spill all the defs we've seen so far or just insert a store at the current point?
    if ((this->func->HasTry() && !this->func->DoOptimizeTryCatch()) || localStoreCost >= lifetime->allDefsCost)
    {TRACE_IT(10561);
        // Insert a store for each def point we've seen so far
        FOREACH_SLIST_ENTRY(IR::Instr *, instr, &(lifetime->defList))
        {TRACE_IT(10562);
            if (instr->GetDst()->AsRegOpnd()->GetReg() != RegNOREG)
            {TRACE_IT(10563);
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
    {TRACE_IT(10564);
        // Insert a def right here at the current instr, and then we'll use compensation code for paths not covered by this def.
        if (!insertionInstr)
        {TRACE_IT(10565);
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
{TRACE_IT(10566);
    // Win8 Bug 391484: We cannot use regOpnd->GetType() here because it
    // can lead to truncation as downstream usage of the register might be of a size
    // greater than the current use. Using RegTypes[reg] works only if the stack slot size
    // is always at least of size MachPtr

    // In the debug mode, if the current sym belongs to the byte code locals, then do not unlink this instruction, as we need to have this instruction to be there
    // to produce the write-through instruction.
    if (sym->IsConst() && !IsSymNonTempLocalVar(sym))
    {TRACE_IT(10567);
        // Let's just delete the def.  We'll reload the constant.
        // We can't just delete the instruction however since the
        // uses will look at the def to get the value.

        // Make sure it wasn't already deleted.
        if (sym->m_instrDef->m_next)
        {TRACE_IT(10568);
            sym->m_instrDef->Unlink();
            sym->m_instrDef->m_next = nullptr;
        }
        return;
    }

    Assert(reg != RegNOREG);

    IRType type = sym->GetType();

    if (sym->IsSimd128())
    {TRACE_IT(10569);
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
    {TRACE_IT(10570);
        Output::Print(_u("...Inserting store for "));
        sym->Dump();
        Output::Print(_u("  Cost:%d\n"), this->GetSpillCost(sym->scratch.linearScan.lifetime));
    }
#endif
}

// LinearScan::InsertLoad
void
LinearScan::InsertLoad(IR::Instr *instr, StackSym *sym, RegNum reg)
{TRACE_IT(10571);
    IR::Opnd *src;
    // The size of loads and stores to memory need to match. See the comment
    // around type in InsertStore above.
    IRType type = sym->GetType();

    if (sym->IsSimd128())
    {TRACE_IT(10572);
        type = sym->GetType();
    }

    bool isMovSDZero = false;
    if (sym->IsConst())
    {TRACE_IT(10573);
        Assert(!sym->IsAllocated() || IsSymNonTempLocalVar(sym));
        // For an intConst, reload the constant instead of using the stack.
        // Create a new StackSym to make sure the old sym remains singleDef
        src = sym->GetConstOpnd();
        if (!src)
        {TRACE_IT(10574);
            isMovSDZero = true;
            sym = StackSym::New(sym->GetType(), this->func);
            sym->m_isConst = true;
            sym->m_isFltConst = true;
        }
        else
        {TRACE_IT(10575);
            StackSym * oldSym = sym;
            sym = StackSym::New(TyVar, this->func);
            sym->m_isConst = true;
            sym->m_isIntConst = oldSym->m_isIntConst;
            sym->m_isInt64Const = oldSym->m_isInt64Const;
            sym->m_isTaggableIntConst = sym->m_isTaggableIntConst;
        }
    }
    else
    {TRACE_IT(10576);
        src = IR::SymOpnd::New(sym, type, this->func);
    }
    IR::Instr * load;
#if defined(_M_IX86) || defined(_M_X64)
    if (isMovSDZero)
    {TRACE_IT(10577);
        load = IR::Instr::New(Js::OpCode::MOVSD_ZERO,
            IR::RegOpnd::New(sym, reg, type, this->func), this->func);
        instr->InsertBefore(load);
    }
    else
#endif
    {TRACE_IT(10578);
        load = Lowerer::InsertMove(IR::RegOpnd::New(sym, reg, type, this->func), src, instr);
    }
    load->CopyNumber(instr);
    if (!isMovSDZero)
    {TRACE_IT(10579);
        this->linearScanMD.LegalizeUse(load, src);
    }

    this->RecordLoopUse(nullptr, reg);

#if DBG_DUMP
    if (PHASE_TRACE(Js::LinearScanPhase, this->func))
    {TRACE_IT(10580);
        Output::Print(_u("...Inserting load for "));
        sym->Dump();
        if (sym->scratch.linearScan.lifetime)
        {TRACE_IT(10581);
            Output::Print(_u("  Cost:%d\n"), this->GetSpillCost(sym->scratch.linearScan.lifetime));
        }
        else
        {TRACE_IT(10582);
            Output::Print(_u("\n"));
        }
    }
#endif
}

uint8
LinearScan::GetRegAttribs(RegNum reg)
{TRACE_IT(10583);
    return RegAttribs[reg];
}

IRType
LinearScan::GetRegType(RegNum reg)
{TRACE_IT(10584);
    return RegTypes[reg];
}

bool
LinearScan::IsCalleeSaved(RegNum reg)
{TRACE_IT(10585);
    return (RegAttribs[reg] & RA_CALLEESAVE) != 0;
}

bool
LinearScan::IsCallerSaved(RegNum reg) const
{TRACE_IT(10586);
    return !LinearScan::IsCalleeSaved(reg) && LinearScan::IsAllocatable(reg);
}

bool
LinearScan::IsAllocatable(RegNum reg) const
{TRACE_IT(10587);
    return !(RegAttribs[reg] & RA_DONTALLOCATE) && this->linearScanMD.IsAllocatable(reg, this->func);
}

void
LinearScan::KillImplicitRegs(IR::Instr *instr)
{TRACE_IT(10588);
    if (instr->IsLabelInstr() || instr->IsBranchInstr())
    {TRACE_IT(10589);
        // Note: need to clear these for branch as well because this info isn't recorded for second chance
        //       allocation on branch boundaries
        this->tempRegs.ClearAll();
    }

#if defined(_M_IX86) || defined(_M_X64)
    if (instr->m_opcode == Js::OpCode::IMUL)
    {TRACE_IT(10590);
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
    {TRACE_IT(10591);
        return;
    }

    if (this->currentBlock->inlineeStack.Count() > 0)
    {TRACE_IT(10592);
        this->SpillInlineeArgs(instr);
    }
    else
    {TRACE_IT(10593);
        instr->m_func = this->func;
    }

    //
    // Spill caller-saved registers that are active.
    //
    BitVector deadRegs;
    deadRegs.Copy(this->activeRegs);
    deadRegs.And(this->callerSavedRegs);
    FOREACH_BITSET_IN_UNITBV(reg, deadRegs, BitVector)
    {TRACE_IT(10594);
        this->SpillReg((RegNum)reg);
    }
    NEXT_BITSET_IN_UNITBV;
    this->tempRegs.And(this->calleeSavedRegs);

    if (callSetupRegs.Count())
    {TRACE_IT(10595);
        callSetupRegs.ClearAll();
    }
    Loop *loop = this->curLoop;
    while (loop)
    {TRACE_IT(10596);
        loop->regAlloc.regUseBv.Or(this->callerSavedRegs);
        loop = loop->parent;
    }
}

//
// Before a call, all inlinee frame syms need to be spilled to a pre-defined location
//
void LinearScan::SpillInlineeArgs(IR::Instr* instr)
{TRACE_IT(10597);
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
        {TRACE_IT(10598);
            return;
        }

        StackSym* sym = lifetime->sym;
        if (!lifetime->isSpilled && !lifetime->isOpHelperSpilled &&
            (!lifetime->isDeadStore && (lifetime->sym->m_isSingleDef || !lifetime->defList.Empty()))) // if deflist is empty - we have already spilled at all defs - and the value is current
        {TRACE_IT(10599);
            if (!spilledRegs.Test(lifetime->reg))
            {TRACE_IT(10600);
                spilledRegs.Set(lifetime->reg);
                if (!sym->IsAllocated())
                {TRACE_IT(10601);
                    this->AllocateStackSpace(lifetime);
                }

                this->RecordLoopUse(lifetime, lifetime->reg);
                Assert(this->regContent[lifetime->reg] != nullptr);
                if (sym->m_isSingleDef)
                {TRACE_IT(10602);
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
{TRACE_IT(10603);
    if (instr->m_opcode == Js::OpCode::InlineeStart)
    {TRACE_IT(10604);
        if (instr->m_func->m_hasInlineArgsOpt)
        {TRACE_IT(10605);
            instr->m_func->frameInfo->IterateSyms([=](StackSym* sym){
                Lifetime* lifetime = sym->scratch.linearScan.lifetime;
                this->currentBlock->inlineeFrameLifetimes.Add(lifetime);

                // We need to maintain as count because the same sym can be used for multiple arguments
                uint* value;
                if (this->currentBlock->inlineeFrameSyms.TryGetReference(sym->m_id, &value))
                {TRACE_IT(10606);
                    *value = *value + 1;
                }
                else
                {TRACE_IT(10607);
                    this->currentBlock->inlineeFrameSyms.Add(sym->m_id, 1);
                }
            });
            if (this->currentBlock->inlineeStack.Count() > 0)
            {TRACE_IT(10608);
                Assert(instr->m_func->inlineDepth == this->currentBlock->inlineeStack.Last()->inlineDepth + 1);
            }
            this->currentBlock->inlineeStack.Add(instr->m_func);
        }
        else
        {TRACE_IT(10609);
            Assert(this->currentBlock->inlineeStack.Count() == 0);
        }
    }
    else if (instr->m_opcode == Js::OpCode::InlineeEnd)
    {TRACE_IT(10610);
        if (instr->m_func->m_hasInlineArgsOpt)
        {TRACE_IT(10611);
            instr->m_func->frameInfo->AllocateRecord(this->func, instr->m_func->GetJITFunctionBody()->GetAddr());

            if(this->currentBlock->inlineeStack.Count() == 0)
            {TRACE_IT(10612);
                // Block is unreachable
                Assert(this->currentBlock->inlineeFrameLifetimes.Count() == 0);
                Assert(this->currentBlock->inlineeFrameSyms.Count() == 0);
            }
            else
            {TRACE_IT(10613);
                Func* func = this->currentBlock->inlineeStack.RemoveAtEnd();
                Assert(func == instr->m_func);

                instr->m_func->frameInfo->IterateSyms([=](StackSym* sym){
                    Lifetime* lifetime = this->currentBlock->inlineeFrameLifetimes.RemoveAtEnd();

                    uint* value;
                    if (this->currentBlock->inlineeFrameSyms.TryGetReference(sym->m_id, &value))
                    {TRACE_IT(10614);
                        *value = *value - 1;
                        if (*value == 0)
                        {TRACE_IT(10615);
                            bool removed = this->currentBlock->inlineeFrameSyms.Remove(sym->m_id);
                            Assert(removed);
                        }
                    }
                    else
                    {TRACE_IT(10616);
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
{TRACE_IT(10617);
    uint useCount = lifetime->GetRegionUseCount(this->curLoop);
    uint spillCost;
    // Get local spill cost.  Ignore helper blocks as we'll also need compensation on the main path.
    uint localUseCost = LinearScan::GetUseSpillCost(this->loopNest, false);

    if (lifetime->reg && !lifetime->isSpilled)
    {TRACE_IT(10618);
        // If it is in a reg, we'll need a store
        if (localUseCost >= lifetime->allDefsCost)
        {TRACE_IT(10619);
            useCount += lifetime->allDefsCost;
        }
        else
        {TRACE_IT(10620);
            useCount += localUseCost;
        }

        if (this->curLoop && !lifetime->sym->IsConst()
            && this->curLoop->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id))
        {TRACE_IT(10621);
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
    {TRACE_IT(10622);
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
    {TRACE_IT(10623);
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
    {TRACE_IT(10624);
        // Second chance allocation have additional overhead, so de-prioritize them
        // Note: could use more tuning...
        spillCost = spillCost * 4/5;
    }
    if (lifetime->isCheapSpill)
    {TRACE_IT(10625);
        // This lifetime will get spilled eventually, so lower the spill cost to favor other lifetimes
        // Note: could use more tuning...
        spillCost /= 2;
    }

    if (lifetime->sym->IsConst())
    {TRACE_IT(10626);
        spillCost = spillCost / 16;
    }

    return spillCost;
}

bool
LinearScan::RemoveDeadStores(IR::Instr *instr)
{TRACE_IT(10627);
    IR::Opnd *dst = instr->GetDst();

    if (dst && dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym && !dst->AsRegOpnd()->m_isCallArg)
    {TRACE_IT(10628);
        IR::RegOpnd *regOpnd = dst->AsRegOpnd();
        Lifetime * lifetime = regOpnd->m_sym->scratch.linearScan.lifetime;

        if (lifetime->isDeadStore)
        {TRACE_IT(10629);
            if (Lowerer::HasSideEffects(instr) == false)
            {TRACE_IT(10630);
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
{TRACE_IT(10631);
    Assert(!this->activeRegs.Test(reg));
    Assert(!lifetime->isSpilled);
    Assert(lifetime->reg == RegNOREG || lifetime->reg == reg);
    this->func->m_regsUsed.Set(reg);
    lifetime->reg = reg;
    this->activeRegs.Set(reg);
    if (lifetime->isFloat || lifetime->isSimd128())
    {TRACE_IT(10632);
        this->floatRegUsedCount++;
    }
    else
    {TRACE_IT(10633);
        this->intRegUsedCount++;
    }
    this->AddToActive(lifetime);

    this->tempRegs.Clear(reg);
}
void
LinearScan::AssignTempReg(Lifetime * lifetime, RegNum reg)
{TRACE_IT(10634);
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
{TRACE_IT(10635);
    if (this->tempRegs.Test(lifetime->reg) && this->tempRegLifetimes[lifetime->reg] == lifetime)
    {TRACE_IT(10636);
        if (this->linearScanMD.FitRegIntSizeConstraints(lifetime->reg, type))
        {TRACE_IT(10637);
            this->RecordLoopUse(nullptr, lifetime->reg);
            return lifetime->reg;
        }
        else
        {TRACE_IT(10638);
            // Free this temp, we'll need to find another one.
            this->tempRegs.Clear(lifetime->reg);
            lifetime->reg = RegNOREG;
        }
    }
    return RegNOREG;
}

uint
LinearScan::GetUseSpillCost(uint loopNest, BOOL isInHelperBlock)
{TRACE_IT(10639);
    if (isInHelperBlock)
    {TRACE_IT(10640);
        // Helper block uses are not as important.
        return 0;
    }
    else if (loopNest < 6)
    {TRACE_IT(10641);
        return (1 << (loopNest * 3));
    }
    else
    {TRACE_IT(10642);
        // Slow growth for deep nest to avoid overflow
        return (1 << (5 * 3)) * (loopNest-5);
    }
}

void
LinearScan::ProcessSecondChanceBoundary(IR::BranchInstr *branchInstr)
{TRACE_IT(10643);
    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {TRACE_IT(10644);
        return;
    }

    if (this->currentOpHelperBlock && this->currentOpHelperBlock->opHelperEndInstr == branchInstr)
    {
        // Lifetimes opHelperSpilled won't get recorded by SaveRegContent().  Do it here.
        FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->opHelperSpilledLiveranges)
        {TRACE_IT(10645);
            if (!lifetime->cantOpHelperSpill)
            {TRACE_IT(10646);
                if (lifetime->isSecondChanceAllocated)
                {TRACE_IT(10647);
                    this->secondChanceRegs.Set(lifetime->reg);
                }
                this->regContent[lifetime->reg] = lifetime;
            }
        } NEXT_SLIST_ENTRY;
    }

    if(branchInstr->IsMultiBranch())
    {TRACE_IT(10648);
        IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

        multiBranchInstr->MapUniqueMultiBrLabels([=](IR::LabelInstr * branchLabel) -> void
        {
            this->ProcessSecondChanceBoundaryHelper(branchInstr, branchLabel);
        });
    }
    else
    {TRACE_IT(10649);
        IR::LabelInstr *branchLabel = branchInstr->GetTarget();
        this->ProcessSecondChanceBoundaryHelper(branchInstr, branchLabel);
    }

    this->SaveRegContent(branchInstr);
}

void
LinearScan::ProcessSecondChanceBoundaryHelper(IR::BranchInstr *branchInstr, IR::LabelInstr *branchLabel)
{TRACE_IT(10650);
    if (branchInstr->GetNumber() > branchLabel->GetNumber())
    {TRACE_IT(10651);
        // Loop back-edge
        Assert(branchLabel->m_isLoopTop);
        branchInstr->m_regContent = nullptr;
        this->InsertSecondChanceCompensation(this->regContent, branchLabel->m_regContent, branchInstr, branchLabel);
    }
    else
    {TRACE_IT(10652);
        // Forward branch
        this->SaveRegContent(branchInstr);
        if (this->curLoop)
        {TRACE_IT(10653);
            this->curLoop->regAlloc.exitRegContentList->Prepend(branchInstr->m_regContent);
        }
        if (!branchLabel->m_loweredBasicBlock)
        {TRACE_IT(10654);
            if (branchInstr->IsConditional() || branchInstr->IsMultiBranch())
            {TRACE_IT(10655);
                // Clone with deep copy
                branchLabel->m_loweredBasicBlock = this->currentBlock->Clone(this->tempAlloc);
            }
            else
            {TRACE_IT(10656);
                // If the unconditional branch leads to the end of the function for the scenario of a bailout - we do not want to
                // copy the lowered inlinee info.
                IR::Instr* nextInstr = branchLabel->GetNextRealInstr();
                if (nextInstr->m_opcode != Js::OpCode::FunctionExit &&
                    nextInstr->m_opcode != Js::OpCode::BailOutStackRestore &&
                    this->currentBlock->HasData())
                {TRACE_IT(10657);
                    // Clone with shallow copy
                    branchLabel->m_loweredBasicBlock = this->currentBlock;

                }
            }
        }
        else
        {TRACE_IT(10658);
            // The lowerer sometimes generates unreachable blocks that would have empty data.
            Assert(!currentBlock->HasData() || branchLabel->m_loweredBasicBlock->Equals(this->currentBlock));
        }
    }
}

void
LinearScan::ProcessSecondChanceBoundary(IR::LabelInstr *labelInstr)
{TRACE_IT(10659);
    if (this->func->HasTry() && !this->func->DoOptimizeTryCatch())
    {TRACE_IT(10660);
        return;
    }

    if (labelInstr->m_isLoopTop)
    {TRACE_IT(10661);
        this->SaveRegContent(labelInstr);
        Lifetime ** regContent = AnewArrayZ(this->tempAlloc, Lifetime *, RegNumCount);
        js_memcpy_s(regContent, (RegNumCount * sizeof(Lifetime *)), this->regContent, sizeof(this->regContent));
        this->curLoop->regAlloc.loopTopRegContent = regContent;
    }

    FOREACH_SLISTCOUNTED_ENTRY_EDITING(IR::BranchInstr *, branchInstr, &labelInstr->labelRefs, iter)
    {TRACE_IT(10662);
        if (branchInstr->m_isAirlock)
        {TRACE_IT(10663);
            // This branch was just inserted... Skip it.
            continue;
        }

        Assert(branchInstr->GetNumber() && labelInstr->GetNumber());
        if (branchInstr->GetNumber() < labelInstr->GetNumber())
        {TRACE_IT(10664);
            // Normal branch
            this->InsertSecondChanceCompensation(branchInstr->m_regContent, this->regContent, branchInstr, labelInstr);
        }
        else
        {TRACE_IT(10665);
            // Loop back-edge
            Assert(labelInstr->m_isLoopTop);
        }
    } NEXT_SLISTCOUNTED_ENTRY_EDITING;
}

IR::Instr * LinearScan::EnsureAirlock(bool needsAirlock, bool *pHasAirlock, IR::Instr *insertionInstr,
                                      IR::Instr **pInsertionStartInstr, IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr)
{TRACE_IT(10666);
    if (needsAirlock && !(*pHasAirlock))
    {TRACE_IT(10667);
        // We need an extra block for the compensation code.
        insertionInstr = this->InsertAirlock(branchInstr, labelInstr);
        *pInsertionStartInstr = insertionInstr->m_prev;
        *pHasAirlock = true;
    }
    return insertionInstr;
}

bool LinearScan::NeedsLoopBackEdgeCompensation(Lifetime *lifetime, IR::LabelInstr *loopTopLabel)
{TRACE_IT(10668);
    if (!lifetime)
    {TRACE_IT(10669);
        return false;
    }

    if (lifetime->sym->IsConst())
    {TRACE_IT(10670);
        return false;
    }

    // No need if lifetime begins in the loop
    if (lifetime->start > loopTopLabel->GetNumber())
    {TRACE_IT(10671);
        return false;
    }

    // Only needed if lifetime is live on the back-edge, and the register is used inside the loop, or the lifetime extends
    // beyond the loop (and compensation out of the loop may use this reg)...
    if (!loopTopLabel->GetLoop()->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id)
        || (this->currentInstr->GetNumber() >= lifetime->end && !this->curLoop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id)))
    {TRACE_IT(10672);
        return false;
    }

    return true;
}

void
LinearScan::InsertSecondChanceCompensation(Lifetime ** branchRegContent, Lifetime **labelRegContent,
                                           IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr)
{TRACE_IT(10673);
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
    {TRACE_IT(10674);
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
        {TRACE_IT(10675);
            Lifetime *labelLifetime = labelRegContent[reg];
            Lifetime *lifetime = branchRegContent[reg];

            // 1.  Insert Stores
            //          Lifetime starts before the loop
            //          Lifetime was re-allocated within the loop (i.e.: a load was most likely inserted)
            //          Lifetime is live on back-edge and has unsaved defs.

            if (lifetime && lifetime->start < labelInstr->GetNumber() && lifetime->lastAllocationStart > labelInstr->GetNumber()
                && (labelInstr->GetLoop()->regAlloc.liveOnBackEdgeSyms->Test(lifetime->sym->m_id))
                && !lifetime->defList.Empty())
            {TRACE_IT(10676);
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                // If the lifetime was second chance allocated inside the loop, there might
                // be spilled loads of this symbol in the loop.  Insert the stores.
                // We don't need to do this if the lifetime was re-allocated before the loop.
                //
                // Note that reg may not be equal to lifetime->reg because of inserted XCHG...
                this->InsertStores(lifetime, lifetime->reg, insertionStartInstr);
            }

            if (lifetime == labelLifetime)
            {TRACE_IT(10677);
                continue;
            }

            // 2.   MOV labelReg/MEM, branchReg
            //          Move current register to match content at the top of the loop
            if (this->NeedsLoopBackEdgeCompensation(lifetime, labelInstr))
            {TRACE_IT(10678);
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
            {TRACE_IT(10679);
                if (!loop->regAlloc.liveOnBackEdgeSyms->Test(labelLifetime->sym->m_id))
                {TRACE_IT(10680);
                    continue;
                }
                if (this->ClearLoopExitIfRegUnused(labelLifetime, (RegNum)reg, branchInstr, loop))
                {TRACE_IT(10681);
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
        {TRACE_IT(10682);
            // Handle lifetimes in a register at the top of the loop, but not currently.
            Lifetime *labelLifetime = labelRegContent[reg];
            if (labelLifetime && !labelLifetime->sym->IsConst() && labelLifetime != branchRegContent[reg] && !thrashedRegs.Test(reg)
                && (loop->regAlloc.liveOnBackEdgeSyms->Test(labelLifetime->sym->m_id)))
            {TRACE_IT(10683);
                if (this->ClearLoopExitIfRegUnused(labelLifetime, (RegNum)reg, branchInstr, loop))
                {TRACE_IT(10684);
                    continue;
                }
                // Mismatch, we need to insert compensation code
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);

                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                            labelLifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
        } NEXT_REG;

        if (hasAirlock)
        {TRACE_IT(10685);
            loop->regAlloc.hasAirLock = true;
        }
    }
    else
    {TRACE_IT(10686);
        //
        // Non-loop-back-edge merge
        //
        FOREACH_REG(reg)
        {TRACE_IT(10687);
            Lifetime *branchLifetime = branchRegContent[reg];
            Lifetime *lifetime = regContent[reg];

            if (lifetime == branchLifetime)
            {TRACE_IT(10688);
                continue;
            }

            if (branchLifetime && branchLifetime->isSpilled && !branchLifetime->sym->IsConst() && branchLifetime->end > labelInstr->GetNumber())
            {TRACE_IT(10689);
                // The lifetime was in a reg at the branch and is now spilled.  We need a store on this path.
                //
                //  MOV  MEM, branch_REG
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          branchLifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
            if (lifetime && !lifetime->sym->IsConst() && lifetime->start <= branchInstr->GetNumber())
            {TRACE_IT(10690);
                // MOV  label_REG, branch_REG / MEM
                insertionInstr = this->EnsureAirlock(needsAirlock, &hasAirlock, insertionInstr, &insertionStartInstr, branchInstr, labelInstr);
                this->ReconcileRegContent(branchRegContent, labelRegContent, branchInstr, labelInstr,
                                          lifetime, (RegNum)reg, &thrashedRegs, insertionInstr, insertionStartInstr);
            }
        } NEXT_REG;
    }

    if (hasAirlock)
    {TRACE_IT(10691);
        // Fix opHelper on airlock label.
        if (insertionInstr->m_prev->IsLabelInstr() && insertionInstr->IsLabelInstr())
        {TRACE_IT(10692);
            if (insertionInstr->m_prev->AsLabelInstr()->isOpHelper && !insertionInstr->AsLabelInstr()->isOpHelper)
            {TRACE_IT(10693);
                insertionInstr->m_prev->AsLabelInstr()->isOpHelper = false;
            }
        }
    }
}

void
LinearScan::ReconcileRegContent(Lifetime ** branchRegContent, Lifetime **labelRegContent,
                                IR::BranchInstr *branchInstr, IR::LabelInstr *labelInstr,
                                Lifetime *lifetime, RegNum reg, BitVector *thrashedRegs, IR::Instr *insertionInstr, IR::Instr *insertionStartInstr)
{TRACE_IT(10694);
    RegNum originalReg = RegNOREG;
    IRType type = RegTypes[reg];
    Assert(labelRegContent[reg] != branchRegContent[reg]);
    bool matchBranchReg = (branchRegContent[reg] == lifetime);
    Lifetime **originalRegContent = (matchBranchReg ? labelRegContent : branchRegContent);
    bool isLoopBackEdge = (branchInstr->GetNumber() > labelInstr->GetNumber());

    if (lifetime->sym->IsConst())
    {TRACE_IT(10695);
        return;
    }

    // Look if this lifetime was in a different register in the previous block.
    // Split the search in 2 to speed this up.
    if (type == TyMachReg)
    {TRACE_IT(10696);
        FOREACH_INT_REG(regIter)
        {TRACE_IT(10697);
            if (originalRegContent[regIter] == lifetime)
            {TRACE_IT(10698);
                originalReg = regIter;
                break;
            }
        } NEXT_INT_REG;
    }
    else
    {TRACE_IT(10699);
        Assert(type == TyFloat64 || IRType_IsSimd128(type));

        FOREACH_FLOAT_REG(regIter)
        {TRACE_IT(10700);
            if (originalRegContent[regIter] == lifetime)
            {TRACE_IT(10701);
                originalReg = regIter;
                break;
            }
        } NEXT_FLOAT_REG;
    }

    RegNum branchReg, labelReg;
    if (matchBranchReg)
    {TRACE_IT(10702);
        branchReg = reg;
        labelReg = originalReg;
    }
    else
    {TRACE_IT(10703);
        branchReg = originalReg;
        labelReg = reg;
    }

    if (branchReg != RegNOREG && !thrashedRegs->Test(branchReg) && !lifetime->sym->IsConst())
    {TRACE_IT(10704);
        Assert(branchRegContent[branchReg] == lifetime);
        if (labelReg != RegNOREG)
        {TRACE_IT(10705);
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
            {TRACE_IT(10706);
                this->RecordLoopUse(lifetime, branchReg);
            }
            thrashedRegs->Set(labelReg);
        }
        else if (!lifetime->sym->IsSingleDef() && lifetime->needsStoreCompensation && !isLoopBackEdge)
        {TRACE_IT(10707);
            Assert(!lifetime->sym->IsConst());
            Assert(matchBranchReg);
            Assert(branchRegContent[branchReg] == lifetime);

            // MOV mem, branchReg
            this->InsertStores(lifetime, branchReg, insertionInstr->m_prev);

            // symRegUseBv needs to be set properly.  Unfortunately, we need to go conservative as we don't know
            // which allocation it was at the source of the branch.
            if (this->IsInLoop())
            {TRACE_IT(10708);
                this->RecordLoopUse(lifetime, branchReg);
            }
        }
    }
    else if (labelReg != RegNOREG)
    {TRACE_IT(10709);
        Assert(labelRegContent[labelReg] == lifetime);
        Assert(lifetime->sym->IsConst() || lifetime->sym->IsAllocated());

        if (branchReg != RegNOREG && !lifetime->sym->IsSingleDef())
        {TRACE_IT(10710);
            Assert(thrashedRegs->Test(branchReg));

            // We can't insert a "MOV labelReg, branchReg" at the insertion point
            // because branchReg was thrashed by a previous reload.
            // Look for that reload to see if we can insert before it.
            IR::Instr *newInsertionInstr = insertionInstr->m_prev;
            bool foundIt = false;
            while (LowererMD::IsAssign(newInsertionInstr))
            {TRACE_IT(10711);
                IR::Opnd *dst = newInsertionInstr->GetDst();
                IR::Opnd *src = newInsertionInstr->GetSrc1();
                if (src->IsRegOpnd() && src->AsRegOpnd()->GetReg() == labelReg)
                {TRACE_IT(10712);
                    // This uses labelReg, give up...
                    break;
                }
                if (dst->IsRegOpnd() && dst->AsRegOpnd()->GetReg() == branchReg)
                {TRACE_IT(10713);
                    // Success!
                    foundIt = true;
                    break;
                }
                newInsertionInstr = newInsertionInstr->m_prev;
            }

            if (foundIt)
            {TRACE_IT(10714);
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
                {TRACE_IT(10715);
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
            {TRACE_IT(10716);
                this->RecordLoopUse(lifetime, branchReg);
            }
        }

        // MOV labelReg, mem
        this->InsertLoad(insertionInstr, lifetime->sym, labelReg);

        thrashedRegs->Set(labelReg);
    }
    else if (!lifetime->sym->IsConst())
    {TRACE_IT(10717);
        Assert(matchBranchReg);
        Assert(branchReg != RegNOREG);
        // The lifetime was in a register at the top of the loop, but we thrashed it with a previous reload...
        if (!lifetime->sym->IsSingleDef())
        {TRACE_IT(10718);
            this->InsertStores(lifetime, branchReg, insertionStartInstr);
        }
#if DBG_DUMP
        if (PHASE_TRACE(Js::SecondChancePhase, this->func))
        {TRACE_IT(10719);
            Output::Print(_u("****** Spilling reg because of bad compensation code order: "));
            lifetime->sym->Dump();
            Output::Print(_u("\n"));
        }
#endif
    }
}

bool LinearScan::ClearLoopExitIfRegUnused(Lifetime *lifetime, RegNum reg, IR::BranchInstr *branchInstr, Loop *loop)
{TRACE_IT(10720);
    // If a lifetime was enregistered into the loop and then spilled, we need compensation at the bottom
    // of the loop to reload the lifetime into that register.
    // If that lifetime was spilled before it was ever used, we don't need the compensation code.
    // We do however need to clear the regContent on any loop exit as the register will not
    // be available anymore on that path.
    // Note: If the lifetime was reloaded into the same register, we might clear the regContent unnecessarily...
    if (!PHASE_OFF(Js::ClearRegLoopExitPhase, this->func))
    {TRACE_IT(10721);
        return false;
    }
    if (!loop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id) && !lifetime->needsStoreCompensation)
    {TRACE_IT(10722);
        if (lifetime->end > branchInstr->GetNumber())
        {
            FOREACH_SLIST_ENTRY(Lifetime **, regContent, loop->regAlloc.exitRegContentList)
            {TRACE_IT(10723);
                if (regContent[reg] == lifetime)
                {TRACE_IT(10724);
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
{TRACE_IT(10725);
    bool changed = true;

    // Look for conflicts in the incoming compensation code:
    //      MOV     ESI, EAX
    //      MOV     ECX, ESI    <<  ESI was lost...
    // Using XCHG:
    //      XCHG    ESI, EAX
    //      MOV     ECX, EAX
    //
    // Note that we need to iterate while(changed) to catch all conflicts
    while(changed) {TRACE_IT(10726);
        RegNum conflictRegs[RegNumCount] = {RegNOREG};
        changed = false;

        FOREACH_BITSET_IN_UNITBV(reg, this->secondChanceRegs, BitVector)
        {TRACE_IT(10727);
            Lifetime *labelLifetime = labelRegContent[reg];
            Lifetime *lifetime = branchRegContent[reg];

            // We don't have an XCHG for SSE2 regs
            if (lifetime == labelLifetime || IRType_IsFloat(RegTypes[reg]))
            {TRACE_IT(10728);
                continue;
            }

            if (this->NeedsLoopBackEdgeCompensation(lifetime, labelInstr))
            {TRACE_IT(10729);
                // Mismatch, we need to insert compensation code
                *pInsertionInstr = this->EnsureAirlock(needsAirlock, pHasAirlock, *pInsertionInstr, pInsertionStartInstr, branchInstr, labelInstr);

                if (conflictRegs[reg] != RegNOREG)
                {TRACE_IT(10730);
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
                {TRACE_IT(10731);
                    if (labelRegContent[regIter] == branchRegContent[reg])
                    {TRACE_IT(10732);
                        labelReg = regIter;
                        break;
                    }
                } NEXT_INT_REG;

                if (labelReg != RegNOREG)
                {TRACE_IT(10733);
                    conflictRegs[labelReg] = (RegNum)reg;
                }
            }
        } NEXT_BITSET_IN_UNITBV;
    }
}
#endif
RegNum
LinearScan::SecondChanceAllocation(Lifetime *lifetime, bool force)
{TRACE_IT(10734);
    if (PHASE_OFF(Js::SecondChancePhase, this->func) || this->func->HasTry())
    {TRACE_IT(10735);
        return RegNOREG;
    }

    // Don't start a second chance allocation from a helper block
    if (lifetime->dontAllocate || this->IsInHelperBlock() || lifetime->isDeadStore)
    {TRACE_IT(10736);
        return RegNOREG;
    }

    Assert(lifetime->isSpilled);
    Assert(lifetime->sym->IsConst() || lifetime->sym->IsAllocated());

    RegNum oldReg = lifetime->reg;
    RegNum reg;

    if (lifetime->start == this->currentInstr->GetNumber() || lifetime->end == this->currentInstr->GetNumber())
    {TRACE_IT(10737);
        // No point doing second chance if the lifetime ends here, or starts here (normal allocation would
        // have found a register if one is available).
        return RegNOREG;
    }
    if (lifetime->sym->IsConst())
    {TRACE_IT(10738);
        // Can't second-chance allocate because we might have deleted the initial def instr, after
        //         having set the reg content on a forward branch...
        return RegNOREG;
    }

    lifetime->reg = RegNOREG;
    lifetime->isSecondChanceAllocated = true;
    reg = this->FindReg(lifetime, nullptr, force);
    lifetime->reg = oldReg;

    if (reg == RegNOREG)
    {TRACE_IT(10739);
        lifetime->isSecondChanceAllocated = false;
        return reg;
    }

    // Success!! We're re-allocating this lifetime...

    this->SecondChanceAllocateToReg(lifetime, reg);

    return reg;
}

void LinearScan::SecondChanceAllocateToReg(Lifetime *lifetime, RegNum reg)
{TRACE_IT(10740);
    RegNum oldReg = lifetime->reg;

    if (oldReg != RegNOREG && this->tempRegLifetimes[oldReg] == lifetime)
    {TRACE_IT(10741);
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
    {TRACE_IT(10742);
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
{TRACE_IT(10743);
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
    {TRACE_IT(10744);
        // Check if branch is coming from helper block.
        IR::Instr *prevLabel = branchInstr->m_prev;
        while (prevLabel && !prevLabel->IsLabelInstr())
        {TRACE_IT(10745);
            prevLabel = prevLabel->m_prev;
        }
        if (prevLabel && prevLabel->AsLabelInstr()->isOpHelper)
        {TRACE_IT(10746);
            isOpHelper = true;
        }
    }
    IR::LabelInstr *airlockLabel = IR::LabelInstr::New(Js::OpCode::Label, this->func, isOpHelper);
    airlockLabel->SetRegion(this->currentRegion);
#if DBG
    if (isOpHelper)
    {TRACE_IT(10747);
        if (branchInstr->m_isHelperToNonHelperBranch)
        {TRACE_IT(10748);
            labelInstr->m_noHelperAssert = true;
        }
        if (labelInstr->isOpHelper && labelInstr->m_noHelperAssert)
        {TRACE_IT(10749);
            airlockLabel->m_noHelperAssert = true;
        }
    }
#endif
    bool replaced = branchInstr->ReplaceTarget(labelInstr, airlockLabel);
    Assert(replaced);

    IR::Instr * prevInstr = labelInstr->GetPrevRealInstrOrLabel();
    if (prevInstr->HasFallThrough())
    {TRACE_IT(10750);
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
{TRACE_IT(10751);
    bool isLabelLoopTop = false;
    Lifetime ** regContent = AnewArrayZ(this->tempAlloc, Lifetime *, RegNumCount);

    if (instr->IsBranchInstr())
    {TRACE_IT(10752);
        instr->AsBranchInstr()->m_regContent = regContent;
    }
    else
    {TRACE_IT(10753);
        Assert(instr->IsLabelInstr());
        Assert(instr->AsLabelInstr()->m_isLoopTop);
        instr->AsLabelInstr()->m_regContent = regContent;
        isLabelLoopTop = true;
    }

    js_memcpy_s(regContent, (RegNumCount * sizeof(Lifetime *)), this->regContent, sizeof(this->regContent));

#if DBG
    FOREACH_SLIST_ENTRY(Lifetime *, lifetime, this->activeLiveranges)
    {TRACE_IT(10754);
        Assert(regContent[lifetime->reg] == lifetime);
    } NEXT_SLIST_ENTRY;
#endif
}

bool LinearScan::RegsAvailable(IRType type)
{TRACE_IT(10755);
    if (IRType_IsFloat(type) || IRType_IsSimd128(type))
    {TRACE_IT(10756);
        return (this->floatRegUsedCount < FLOAT_REG_COUNT);
    }
    else
    {TRACE_IT(10757);
        return (this->intRegUsedCount < INT_REG_COUNT);
    }
}

uint LinearScan::GetRemainingHelperLength(Lifetime *const lifetime)
{TRACE_IT(10758);
    // Walk the helper block linked list starting from the next helper block until the end of the lifetime
    uint helperLength = 0;
    SList<OpHelperBlock>::Iterator it(opHelperBlockIter);
    Assert(it.IsValid());
    const uint end = max(currentInstr->GetNumber(), lifetime->end);
    do
    {TRACE_IT(10759);
        const OpHelperBlock &helper = it.Data();
        const uint helperStart = helper.opHelperLabel->GetNumber();
        if(helperStart > end)
        {TRACE_IT(10760);
            break;
        }

        const uint helperEnd = min(end, helper.opHelperEndInstr->GetNumber());
        helperLength += helperEnd - helperStart;
        if(helperEnd != helper.opHelperEndInstr->GetNumber() || !helper.opHelperEndInstr->IsLabelInstr())
        {TRACE_IT(10761);
            // A helper block that ends at a label does not return to the function. Since this helper block does not end
            // at a label, include the end instruction as well.
            ++helperLength;
        }
    } while(it.Next());

    return helperLength;
}

uint LinearScan::CurrentOpHelperVisitedLength(IR::Instr *const currentInstr) const
{TRACE_IT(10762);
    Assert(currentInstr);

    if(!currentOpHelperBlock)
    {TRACE_IT(10763);
        return 0;
    }

    // Consider the current instruction to have not yet been visited
    Assert(currentInstr->GetNumber() >= currentOpHelperBlock->opHelperLabel->GetNumber());
    return currentInstr->GetNumber() - currentOpHelperBlock->opHelperLabel->GetNumber();
}

IR::Instr * LinearScan::TryHoistLoad(IR::Instr *instr, Lifetime *lifetime)
{TRACE_IT(10764);
    // If we are loading a lifetime into a register inside a loop, try to hoist that load outside the loop
    // if that register hasn't been used yet.
    RegNum reg = lifetime->reg;
    IR::Instr *insertInstr = instr;

    if (PHASE_OFF(Js::RegHoistLoadsPhase, this->func))
    {TRACE_IT(10765);
        return insertInstr;
    }

    if ((this->func->HasTry() && !this->func->DoOptimizeTryCatch()) || (this->currentRegion && this->currentRegion->GetType() != RegionTypeRoot))
    {TRACE_IT(10766);
        return insertInstr;
    }

    // Register unused, and lifetime unused yet.
    if (this->IsInLoop() && !this->curLoop->regAlloc.regUseBv.Test(reg)
        && !this->curLoop->regAlloc.defdInLoopBv->Test(lifetime->sym->m_id)
        && !this->curLoop->regAlloc.symRegUseBv->Test(lifetime->sym->m_id)
        && !this->curLoop->regAlloc.hasAirLock)
    {TRACE_IT(10767);
        // Let's hoist!
        insertInstr = insertInstr->m_prev;

        // Walk each instructions until the top of the loop looking for branches
        while (!insertInstr->IsLabelInstr() || !insertInstr->AsLabelInstr()->m_isLoopTop || !insertInstr->AsLabelInstr()->GetLoop()->IsDescendentOrSelf(this->curLoop))
        {TRACE_IT(10768);
            if (insertInstr->IsBranchInstr() && insertInstr->AsBranchInstr()->m_regContent)
            {TRACE_IT(10769);
                IR::BranchInstr *branchInstr = insertInstr->AsBranchInstr();
                // That lifetime might have been in another register coming into the loop, and spilled before used.
                // Clear the reg content.
                FOREACH_REG(regIter)
                {TRACE_IT(10770);
                    if (branchInstr->m_regContent[regIter] == lifetime)
                    {TRACE_IT(10771);
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
        {TRACE_IT(10772);
            if (loopTopLabel->m_regContent[regIter] == lifetime)
            {TRACE_IT(10773);
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
        {TRACE_IT(10774);
            Assert(branchInstr->GetNumber() != Js::Constants::NoByteCodeOffset);
            // <= because the branch may be newly inserted and have the same instr number as the loop top...
            if (branchInstr->GetNumber() <= loopTopLabel->GetNumber())
            {TRACE_IT(10775);
                if (!loopLandingPad)
                {TRACE_IT(10776);
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
{TRACE_IT(10777);
    uint loopNest = 0;
    uint storeCount = 0;
    uint loadCount = 0;
    uint wStoreCount = 0;
    uint wLoadCount = 0;
    uint instrCount = 0;
    bool isInHelper = false;

    FOREACH_INSTR_IN_FUNC_BACKWARD(instr, this->func)
    {TRACE_IT(10778);
        switch (instr->GetKind())
        {
        case IR::InstrKindPragma:
            continue;

        case IR::InstrKindBranch:
            if (instr->AsBranchInstr()->IsLoopTail(this->func))
            {TRACE_IT(10779);
                loopNest++;
            }

            instrCount++;
            break;

        case IR::InstrKindLabel:
        case IR::InstrKindProfiledLabel:
            if (instr->AsLabelInstr()->m_isLoopTop)
            {TRACE_IT(10780);
                Assert(loopNest);
                loopNest--;
            }

            isInHelper = instr->AsLabelInstr()->isOpHelper;
            break;

        default:
            {
                Assert(instr->IsRealInstr());

                if (isInHelper)
                {TRACE_IT(10781);
                    continue;
                }

                IR::Opnd *dst = instr->GetDst();
                if (dst && dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsStackSym() && dst->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                {TRACE_IT(10782);
                    storeCount++;
                    wStoreCount += LinearScan::GetUseSpillCost(loopNest, false);
                }
                IR::Opnd *src1 = instr->GetSrc1();
                if (src1)
                {TRACE_IT(10783);
                    if (src1->IsSymOpnd() && src1->AsSymOpnd()->m_sym->IsStackSym() && src1->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                    {TRACE_IT(10784);
                        loadCount++;
                        wLoadCount += LinearScan::GetUseSpillCost(loopNest, false);
                    }
                    IR::Opnd *src2 = instr->GetSrc2();
                    if (src2 && src2->IsSymOpnd() && src2->AsSymOpnd()->m_sym->IsStackSym() && src2->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
                    {TRACE_IT(10785);
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
{TRACE_IT(10786);
    // Make sure we don't insert an INC between an instr setting the condition code, and one using it.
    IR::Instr *instrNext = instr;
    while(!EncoderMD::UsesConditionCode(instrNext) && !EncoderMD::SetsConditionCode(instrNext))
    {TRACE_IT(10787);
        if (instrNext->IsLabelInstr() || instrNext->IsExitInstr() || instrNext->IsBranchInstr())
        {TRACE_IT(10788);
            break;
        }
        instrNext = instrNext->GetNextRealInstrOrLabel();
    }

    if (instrNext->IsLowered() && EncoderMD::UsesConditionCode(instrNext))
    {TRACE_IT(10789);
        IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();
        while(!EncoderMD::SetsConditionCode(instrPrev))
        {TRACE_IT(10790);
            instrPrev = instrPrev->GetPrevRealInstrOrLabel();
            Assert(!instrPrev->IsLabelInstr());
        }

        return instrPrev;
    }

    return instr;
}

void LinearScan::DynamicStatsInstrument()
{TRACE_IT(10791);    
    {TRACE_IT(10792);
        IR::Instr *firstInstr = this->func->m_headInstr;
    IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(this->func->GetJITFunctionBody()->GetCallCountStatsAddr(), TyUint32, this->func);
        firstInstr->InsertAfter(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
    }

    FOREACH_INSTR_IN_FUNC(instr, this->func)
    {TRACE_IT(10793);
        if (!instr->IsRealInstr() || !instr->IsLowered())
        {TRACE_IT(10794);
            continue;
        }

        if (EncoderMD::UsesConditionCode(instr) && instr->GetPrevRealInstrOrLabel()->IsLabelInstr())
        {TRACE_IT(10795);
            continue;
        }

        IR::Opnd *dst = instr->GetDst();
        if (dst && dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsStackSym() && dst->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
        {TRACE_IT(10796);
            IR::Instr *insertionInstr = this->GetIncInsertionPoint(instr);
            IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(this->func->GetJITFunctionBody()->GetRegAllocStoreCountAddr(), TyUint32, this->func);
            insertionInstr->InsertBefore(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
        }
        IR::Opnd *src1 = instr->GetSrc1();
        if (src1)
        {TRACE_IT(10797);
            if (src1->IsSymOpnd() && src1->AsSymOpnd()->m_sym->IsStackSym() && src1->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
            {TRACE_IT(10798);
                IR::Instr *insertionInstr = this->GetIncInsertionPoint(instr);
                IR::MemRefOpnd *memRefOpnd = IR::MemRefOpnd::New(this->func->GetJITFunctionBody()->GetRegAllocStoreCountAddr(), TyUint32, this->func);
                insertionInstr->InsertBefore(IR::Instr::New(Js::OpCode::INC, memRefOpnd, memRefOpnd, this->func));
            }
            IR::Opnd *src2 = instr->GetSrc2();
            if (src2 && src2->IsSymOpnd() && src2->AsSymOpnd()->m_sym->IsStackSym() && src2->AsSymOpnd()->m_sym->AsStackSym()->IsAllocated())
            {TRACE_IT(10799);
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
{TRACE_IT(10800);
    IR::Instr *instrPrev = insertBeforeInstr->m_prev;

    IR::Instr *instrRet = Lowerer::InsertMove(dst, src, insertBeforeInstr);

    for (IR::Instr *instr = instrPrev->m_next; instr != insertBeforeInstr; instr = instr->m_next)
    {TRACE_IT(10801);
        instr->CopyNumber(insertBeforeInstr);
    }

    return instrRet;
}

IR::Instr* LinearScan::InsertLea(IR::RegOpnd *dst, IR::Opnd *src, IR::Instr *const insertBeforeInstr)
{TRACE_IT(10802);
    IR::Instr *instrPrev = insertBeforeInstr->m_prev;

    IR::Instr *instrRet = Lowerer::InsertLea(dst, src, insertBeforeInstr, true);

    for (IR::Instr *instr = instrPrev->m_next; instr != insertBeforeInstr; instr = instr->m_next)
    {TRACE_IT(10803);
        instr->CopyNumber(insertBeforeInstr);
    }

    return instrRet;
}
