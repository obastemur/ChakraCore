//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#define INLINEEMETAARG_COUNT 3

BackwardPass::BackwardPass(Func * func, GlobOpt * globOpt, Js::Phase tag)
    : func(func), globOpt(globOpt), tag(tag), currentPrePassLoop(nullptr), tempAlloc(nullptr),
    preOpBailOutInstrToProcess(nullptr),
    considerSymAsRealUseInNoImplicitCallUses(nullptr),
    isCollectionPass(false), currentRegion(nullptr)
{LOGMEIN("BackwardPass.cpp] 14\n");
    // Those are the only two phase dead store will be used currently
    Assert(tag == Js::BackwardPhase || tag == Js::DeadStorePhase);

    this->implicitCallBailouts = 0;
    this->fieldOpts = 0;

#if DBG_DUMP
    this->numDeadStore = 0;
    this->numMarkTempNumber = 0;
    this->numMarkTempNumberTransferred = 0;
    this->numMarkTempObject = 0;
#endif
}

void
BackwardPass::DoSetDead(IR::Opnd * opnd, bool isDead) const
{LOGMEIN("BackwardPass.cpp] 31\n");
    // Note: Dead bit on the Opnd records flow-based liveness.
    // This is distinct from isLastUse, which records lexical last-ness.
    if (isDead && this->tag == Js::BackwardPhase && !this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 35\n");
        opnd->SetIsDead();
    }
    else if (this->tag == Js::DeadStorePhase)
    {LOGMEIN("BackwardPass.cpp] 39\n");
        // Set or reset in DeadStorePhase.
        // CSE could make previous dead operand not the last use, so reset it.
        opnd->SetIsDead(isDead);
    }
}

bool
BackwardPass::DoByteCodeUpwardExposedUsed() const
{LOGMEIN("BackwardPass.cpp] 48\n");
    return (this->tag == Js::DeadStorePhase && this->func->hasBailout) ||
        (this->func->HasTry() && this->func->DoOptimizeTryCatch() && this->tag == Js::BackwardPhase);
}

bool
BackwardPass::DoFieldHoistCandidates() const
{LOGMEIN("BackwardPass.cpp] 55\n");
    return DoFieldHoistCandidates(this->currentBlock->loop);
}

bool
BackwardPass::DoFieldHoistCandidates(Loop * loop) const
{LOGMEIN("BackwardPass.cpp] 61\n");
    // We only need to do one pass to generate this data
    return this->tag == Js::BackwardPhase
        && !this->IsPrePass() && loop && GlobOpt::DoFieldHoisting(loop);
}

bool
BackwardPass::DoMarkTempNumbers() const
{LOGMEIN("BackwardPass.cpp] 69\n");
#if FLOATVAR
    return false;
#else
    // only mark temp number on the dead store phase
    return (tag == Js::DeadStorePhase) && !PHASE_OFF(Js::MarkTempPhase, this->func) &&
        !PHASE_OFF(Js::MarkTempNumberPhase, this->func) && func->DoFastPaths() && (!this->func->HasTry());
#endif
}

bool
BackwardPass::DoMarkTempObjects() const
{LOGMEIN("BackwardPass.cpp] 81\n");
    // only mark temp object on the backward store phase
    return (tag == Js::BackwardPhase) && !PHASE_OFF(Js::MarkTempPhase, this->func) &&
        !PHASE_OFF(Js::MarkTempObjectPhase, this->func) && func->DoGlobOpt() && func->GetHasTempObjectProducingInstr() &&
        !func->IsJitInDebugMode() &&
        func->DoGlobOptsForGeneratorFunc();

    // Why MarkTempObject is disabled under debugger:
    //   We add 'identified so far dead non-temp locals' to byteCodeUpwardExposedUsed in ProcessBailOutInfo,
    //   this may cause MarkTempObject to convert some temps back to non-temp when it sees a 'transferred exposed use'
    //   from a temp to non-temp. That's in general not a supported conversion (while non-temp -> temp is fine).
}

bool
BackwardPass::DoMarkTempNumbersOnTempObjects() const
{LOGMEIN("BackwardPass.cpp] 96\n");
    return !PHASE_OFF(Js::MarkTempNumberOnTempObjectPhase, this->func) && DoMarkTempNumbers() && this->func->GetHasMarkTempObjects();
}

#if DBG
bool
BackwardPass::DoMarkTempObjectVerify() const
{LOGMEIN("BackwardPass.cpp] 103\n");
    // only mark temp object on the backward store phase
    return (tag == Js::DeadStorePhase) && !PHASE_OFF(Js::MarkTempPhase, this->func) &&
        !PHASE_OFF(Js::MarkTempObjectPhase, this->func) && func->DoGlobOpt() && func->GetHasTempObjectProducingInstr();
}
#endif

// static
bool
BackwardPass::DoDeadStore(Func* func)
{LOGMEIN("BackwardPass.cpp] 113\n");
    return
        !PHASE_OFF(Js::DeadStorePhase, func) &&
        (!func->HasTry() || func->DoOptimizeTryCatch());
}

bool
BackwardPass::DoDeadStore() const
{LOGMEIN("BackwardPass.cpp] 121\n");
    return
        this->tag == Js::DeadStorePhase &&
        DoDeadStore(this->func);
}

bool
BackwardPass::DoDeadStoreSlots() const
{LOGMEIN("BackwardPass.cpp] 129\n");
    // only dead store fields if glob opt is on to generate the trackable fields bitvector
    return (tag == Js::DeadStorePhase && this->func->DoGlobOpt()
        && (!this->func->HasTry()));
}

// Whether dead store is enabled for given func and sym.
// static
bool
BackwardPass::DoDeadStore(Func* func, StackSym* sym)
{LOGMEIN("BackwardPass.cpp] 139\n");
    // Dead store is disabled under debugger for non-temp local vars.
    return
        DoDeadStore(func) &&
        !(func->IsJitInDebugMode() && sym->HasByteCodeRegSlot() && func->IsNonTempLocalVar(sym->GetByteCodeRegSlot())) &&
        func->DoGlobOptsForGeneratorFunc();
}

bool
BackwardPass::DoTrackNegativeZero() const
{LOGMEIN("BackwardPass.cpp] 149\n");
    return
        !PHASE_OFF(Js::TrackIntUsagePhase, func) &&
        !PHASE_OFF(Js::TrackNegativeZeroPhase, func) &&
        func->DoGlobOpt() &&
        !IsPrePass() &&
        !func->IsJitInDebugMode() &&
        func->DoGlobOptsForGeneratorFunc();
}

bool
BackwardPass::DoTrackBitOpsOrNumber() const
{LOGMEIN("BackwardPass.cpp] 161\n");
#if _WIN64
    return
        !PHASE_OFF1(Js::TypedArrayVirtualPhase) &&
        tag == Js::BackwardPhase &&
        func->DoGlobOpt() &&
        !IsPrePass() &&
        !func->IsJitInDebugMode() &&
        func->DoGlobOptsForGeneratorFunc();
#else
    return false;
#endif
}

bool
BackwardPass::DoTrackIntOverflow() const
{LOGMEIN("BackwardPass.cpp] 177\n");
    return
        !PHASE_OFF(Js::TrackIntUsagePhase, func) &&
        !PHASE_OFF(Js::TrackIntOverflowPhase, func) &&
        tag == Js::BackwardPhase &&
        !IsPrePass() &&
        globOpt->DoLossyIntTypeSpec() &&
        !func->IsJitInDebugMode() &&
        func->DoGlobOptsForGeneratorFunc();
}

bool
BackwardPass::DoTrackCompoundedIntOverflow() const
{LOGMEIN("BackwardPass.cpp] 190\n");
    return
        !PHASE_OFF(Js::TrackCompoundedIntOverflowPhase, func) &&
        DoTrackIntOverflow() &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsTrackCompoundedIntOverflowDisabled());
}

bool
BackwardPass::DoTrackNon32BitOverflow() const
{LOGMEIN("BackwardPass.cpp] 199\n");
    // enabled only for IA
#if defined(_M_IX86) || defined(_M_X64)
    return true;
#else
    return false;
#endif
}

void
BackwardPass::CleanupBackwardPassInfoInFlowGraph()
{LOGMEIN("BackwardPass.cpp] 210\n");
    if (!this->func->m_fg->hasBackwardPassInfo)
    {LOGMEIN("BackwardPass.cpp] 212\n");
        // No information to clean up
        return;
    }

    // The backward pass temp arena has already been deleted, we can just reset the data

    FOREACH_BLOCK_IN_FUNC_DEAD_OR_ALIVE(block, this->func)
    {LOGMEIN("BackwardPass.cpp] 220\n");
        block->upwardExposedUses = nullptr;
        block->upwardExposedFields = nullptr;
        block->typesNeedingKnownObjectLayout = nullptr;
        block->fieldHoistCandidates = nullptr;
        block->slotDeadStoreCandidates = nullptr;
        block->byteCodeUpwardExposedUsed = nullptr;
#if DBG
        block->byteCodeRestoreSyms = nullptr;
#endif
        block->tempNumberTracker = nullptr;
        block->tempObjectTracker = nullptr;
#if DBG
        block->tempObjectVerifyTracker = nullptr;
#endif
        block->stackSymToFinalType = nullptr;
        block->stackSymToGuardedProperties = nullptr;
        block->stackSymToWriteGuardsMap = nullptr;
        block->cloneStrCandidates = nullptr;
        block->noImplicitCallUses = nullptr;
        block->noImplicitCallNoMissingValuesUses = nullptr;
        block->noImplicitCallNativeArrayUses = nullptr;
        block->noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
        block->noImplicitCallArrayLengthSymUses = nullptr;
        block->couldRemoveNegZeroBailoutForDef = nullptr;

        if (block->loop != nullptr)
        {LOGMEIN("BackwardPass.cpp] 247\n");
            block->loop->hasDeadStoreCollectionPass = false;
            block->loop->hasDeadStorePrepass = false;
        }
    }
    NEXT_BLOCK_IN_FUNC_DEAD_OR_ALIVE;
}

/*
*   We Insert ArgIns at the start of the function for all the formals.
*   Unused formals will be deadstored during the deadstore pass.
*   We need ArgIns only for the outermost function(inliner).
*/
void
BackwardPass::InsertArgInsForFormals()
{LOGMEIN("BackwardPass.cpp] 262\n");
    if (func->IsStackArgsEnabled() && !func->GetJITFunctionBody()->HasImplicitArgIns())
    {LOGMEIN("BackwardPass.cpp] 264\n");
        IR::Instr * insertAfterInstr = func->m_headInstr->m_next;
        AssertMsg(insertAfterInstr->IsLabelInstr(), "First Instr of the first block should always have a label");

        Js::ArgSlot paramsCount = insertAfterInstr->m_func->GetJITFunctionBody()->GetInParamsCount() - 1;
        IR::Instr *     argInInstr = nullptr;
        for (Js::ArgSlot argumentIndex = 1; argumentIndex <= paramsCount; argumentIndex++)
        {LOGMEIN("BackwardPass.cpp] 271\n");
            IR::SymOpnd *   srcOpnd;
            StackSym *      symSrc = StackSym::NewParamSlotSym(argumentIndex + 1, func);
            StackSym *      symDst = StackSym::New(func);
            IR::RegOpnd * dstOpnd = IR::RegOpnd::New(symDst, TyVar, func);

            func->SetArgOffset(symSrc, (argumentIndex + LowererMD::GetFormalParamOffset()) * MachPtr);

            srcOpnd = IR::SymOpnd::New(symSrc, TyVar, func);

            argInInstr = IR::Instr::New(Js::OpCode::ArgIn_A, dstOpnd, srcOpnd, func);
            insertAfterInstr->InsertAfter(argInInstr);
            insertAfterInstr = argInInstr;

            AssertMsg(!func->HasStackSymForFormal(argumentIndex - 1), "Already has a stack sym for this formal?");
            this->func->TrackStackSymForFormalIndex(argumentIndex - 1, symDst);
        }

        if (PHASE_VERBOSE_TRACE1(Js::StackArgFormalsOptPhase) && paramsCount > 0)
        {LOGMEIN("BackwardPass.cpp] 290\n");
            Output::Print(_u("StackArgFormals : %s (%d) :Inserting ArgIn_A for LdSlot (formals) in the start of Deadstore pass. \n"), func->GetJITFunctionBody()->GetDisplayName(), func->GetFunctionNumber());
            Output::Flush();
        }
    }
}

void
BackwardPass::MarkScopeObjSymUseForStackArgOpt()
{LOGMEIN("BackwardPass.cpp] 299\n");
    IR::Instr * instr = this->currentInstr;
    if (tag == Js::DeadStorePhase)
    {LOGMEIN("BackwardPass.cpp] 302\n");
        if (instr->DoStackArgsOpt(this->func) && instr->m_func->GetScopeObjSym() != nullptr)
        {LOGMEIN("BackwardPass.cpp] 304\n");
            if (this->currentBlock->byteCodeUpwardExposedUsed == nullptr)
            {LOGMEIN("BackwardPass.cpp] 306\n");
                this->currentBlock->byteCodeUpwardExposedUsed = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
            this->currentBlock->byteCodeUpwardExposedUsed->Set(instr->m_func->GetScopeObjSym()->m_id);
        }
    }
}

void
BackwardPass::ProcessBailOnStackArgsOutOfActualsRange()
{LOGMEIN("BackwardPass.cpp] 316\n");
    IR::Instr * instr = this->currentInstr;

    if (tag == Js::DeadStorePhase && 
        (instr->m_opcode == Js::OpCode::LdElemI_A || instr->m_opcode == Js::OpCode::TypeofElem) && 
        instr->HasBailOutInfo() && !IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 322\n");
        if (instr->DoStackArgsOpt(this->func))
        {LOGMEIN("BackwardPass.cpp] 324\n");
            AssertMsg(instr->GetBailOutKind() & IR::BailOnStackArgsOutOfActualsRange, "Stack args bail out is not set when the optimization is turned on? ");
            if (instr->GetBailOutKind() & ~IR::BailOnStackArgsOutOfActualsRange)
            {LOGMEIN("BackwardPass.cpp] 327\n");
                Assert(instr->GetBailOutKind() == (IR::BailOnStackArgsOutOfActualsRange | IR::BailOutOnImplicitCallsPreOp));
                //We are sure at this point, that we will not have any implicit calls as we wouldn't have done this optimization in the first place.
                instr->SetBailOutKind(IR::BailOnStackArgsOutOfActualsRange);
            }
        }
        else if (instr->GetBailOutKind() & IR::BailOnStackArgsOutOfActualsRange)
        {LOGMEIN("BackwardPass.cpp] 334\n");
            //If we don't decide to do StackArgs, then remove the bail out at this point.
            //We would have optimistically set the bailout in the forward pass, and by the end of forward pass - we
            //turned off stack args for some reason. So we are removing it in the deadstore pass.
            IR::BailOutKind bailOutKind = instr->GetBailOutKind() & ~IR::BailOnStackArgsOutOfActualsRange;
            if (bailOutKind == IR::BailOutInvalid)
            {LOGMEIN("BackwardPass.cpp] 340\n");
                instr->ClearBailOutInfo();
            }
            else
            {
                instr->SetBailOutKind(bailOutKind);
            }
        }
    }
}

void
BackwardPass::Optimize()
{LOGMEIN("BackwardPass.cpp] 353\n");
    if (tag == Js::BackwardPhase && PHASE_OFF(tag, this->func))
    {LOGMEIN("BackwardPass.cpp] 355\n");
        return;
    }

    if (tag == Js::DeadStorePhase)
    {LOGMEIN("BackwardPass.cpp] 360\n");
        if (!this->func->DoLoopFastPaths() || !this->func->DoFastPaths())
        {LOGMEIN("BackwardPass.cpp] 362\n");
            //arguments[] access is similar to array fast path hence disable when array fastpath is disabled.
            //loopFastPath is always true except explicitly disabled
            //defaultDoFastPath can be false when we the source code size is huge
            func->SetHasStackArgs(false);
        }
        InsertArgInsForFormals();
    }

    NoRecoverMemoryJitArenaAllocator localAlloc(tag == Js::BackwardPhase? _u("BE-Backward") : _u("BE-DeadStore"),
        this->func->m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);

    this->tempAlloc = &localAlloc;
#if DBG_DUMP
    if (this->IsTraceEnabled())
    {LOGMEIN("BackwardPass.cpp] 377\n");
        this->func->DumpHeader();
    }
#endif

    this->CleanupBackwardPassInfoInFlowGraph();

    // Info about whether a sym is used in a way in which -0 differs from +0, or whether the sym is used in a way in which an
    // int32 overflow when generating the value of the sym matters, in the current block. The info is transferred to
    // instructions that define the sym in the current block as they are encountered. The info in these bit vectors is discarded
    // after optimizing each block, so the only info that remains for GlobOpt is that which is transferred to instructions.
    BVSparse<JitArenaAllocator> localNegativeZeroDoesNotMatterBySymId(tempAlloc);
    negativeZeroDoesNotMatterBySymId = &localNegativeZeroDoesNotMatterBySymId;

    BVSparse<JitArenaAllocator> localSymUsedOnlyForBitOpsBySymId(tempAlloc);
    symUsedOnlyForBitOpsBySymId = &localSymUsedOnlyForBitOpsBySymId;
    BVSparse<JitArenaAllocator> localSymUsedOnlyForNumberBySymId(tempAlloc);
    symUsedOnlyForNumberBySymId = &localSymUsedOnlyForNumberBySymId;

    BVSparse<JitArenaAllocator> localIntOverflowDoesNotMatterBySymId(tempAlloc);
    intOverflowDoesNotMatterBySymId = &localIntOverflowDoesNotMatterBySymId;
    BVSparse<JitArenaAllocator> localIntOverflowDoesNotMatterInRangeBySymId(tempAlloc);
    intOverflowDoesNotMatterInRangeBySymId = &localIntOverflowDoesNotMatterInRangeBySymId;
    BVSparse<JitArenaAllocator> localCandidateSymsRequiredToBeInt(tempAlloc);
    candidateSymsRequiredToBeInt = &localCandidateSymsRequiredToBeInt;
    BVSparse<JitArenaAllocator> localCandidateSymsRequiredToBeLossyInt(tempAlloc);
    candidateSymsRequiredToBeLossyInt = &localCandidateSymsRequiredToBeLossyInt;
    intOverflowCurrentlyMattersInRange = true;

    FloatSymEquivalenceMap localFloatSymEquivalenceMap(tempAlloc);
    floatSymEquivalenceMap = &localFloatSymEquivalenceMap;

    NumberTempRepresentativePropertySymMap localNumberTempRepresentativePropertySym(tempAlloc);
    numberTempRepresentativePropertySym = &localNumberTempRepresentativePropertySym;

    FOREACH_BLOCK_BACKWARD_IN_FUNC_DEAD_OR_ALIVE(block, this->func)
    {LOGMEIN("BackwardPass.cpp] 413\n");
        this->OptBlock(block);
    }
    NEXT_BLOCK_BACKWARD_IN_FUNC_DEAD_OR_ALIVE;

    if (this->tag == Js::DeadStorePhase && !PHASE_OFF(Js::MemOpPhase, this->func))
    {LOGMEIN("BackwardPass.cpp] 419\n");
        this->RemoveEmptyLoops();
    }
    this->func->m_fg->hasBackwardPassInfo = true;

    if(DoTrackCompoundedIntOverflow())
    {LOGMEIN("BackwardPass.cpp] 425\n");
        // Tracking int overflow makes use of a scratch field in stack syms, which needs to be cleared
        func->m_symTable->ClearStackSymScratch();
    }

#if DBG_DUMP
    if (PHASE_STATS(this->tag, this->func))
    {LOGMEIN("BackwardPass.cpp] 432\n");
        this->func->DumpHeader();
        Output::Print(this->tag == Js::BackwardPhase? _u("Backward Phase Stats:\n") : _u("Deadstore Phase Stats:\n"));
        if (this->DoDeadStore())
        {LOGMEIN("BackwardPass.cpp] 436\n");
            Output::Print(_u("  Deadstore              : %3d\n"), this->numDeadStore);
        }
        if (this->DoMarkTempNumbers())
        {LOGMEIN("BackwardPass.cpp] 440\n");
            Output::Print(_u("  Temp Number            : %3d\n"), this->numMarkTempNumber);
            Output::Print(_u("  Transferred Temp Number: %3d\n"), this->numMarkTempNumberTransferred);
        }
        if (this->DoMarkTempObjects())
        {LOGMEIN("BackwardPass.cpp] 445\n");
            Output::Print(_u("  Temp Object            : %3d\n"), this->numMarkTempObject);
        }
    }
#endif
}

void
BackwardPass::MergeSuccBlocksInfo(BasicBlock * block)
{LOGMEIN("BackwardPass.cpp] 454\n");
    // Can't reuse the bv in the current block, because its successor can be itself.

    TempNumberTracker * tempNumberTracker = nullptr;
    TempObjectTracker * tempObjectTracker = nullptr;
#if DBG
    TempObjectVerifyTracker * tempObjectVerifyTracker = nullptr;
#endif
    HashTable<AddPropertyCacheBucket> * stackSymToFinalType = nullptr;
    HashTable<ObjTypeGuardBucket> * stackSymToGuardedProperties = nullptr;
    HashTable<ObjWriteGuardBucket> * stackSymToWriteGuardsMap = nullptr;
    BVSparse<JitArenaAllocator> * cloneStrCandidates = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallUses = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallNoMissingValuesUses = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallNativeArrayUses = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
    BVSparse<JitArenaAllocator> * noImplicitCallArrayLengthSymUses = nullptr;
    BVSparse<JitArenaAllocator> * upwardExposedUses = nullptr;
    BVSparse<JitArenaAllocator> * upwardExposedFields = nullptr;
    BVSparse<JitArenaAllocator> * typesNeedingKnownObjectLayout = nullptr;
    BVSparse<JitArenaAllocator> * fieldHoistCandidates = nullptr;
    BVSparse<JitArenaAllocator> * slotDeadStoreCandidates = nullptr;
    BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = nullptr;
    BVSparse<JitArenaAllocator> * couldRemoveNegZeroBailoutForDef = nullptr;
#if DBG
    uint byteCodeLocalsCount = func->GetJITFunctionBody()->GetLocalsCount();
    StackSym ** byteCodeRestoreSyms = nullptr;
#endif

    Assert(!block->isDead || block->GetSuccList()->Empty());

    if (this->DoByteCodeUpwardExposedUsed())
    {LOGMEIN("BackwardPass.cpp] 486\n");
        byteCodeUpwardExposedUsed = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
#if DBG
        byteCodeRestoreSyms = JitAnewArrayZ(this->tempAlloc, StackSym *, byteCodeLocalsCount);
#endif
    }

#if DBG
    if (!IsCollectionPass() && this->DoMarkTempObjectVerify())
    {LOGMEIN("BackwardPass.cpp] 495\n");
        tempObjectVerifyTracker = JitAnew(this->tempAlloc, TempObjectVerifyTracker, this->tempAlloc, block->loop != nullptr);
    }
#endif

    if (!block->isDead)
    {LOGMEIN("BackwardPass.cpp] 501\n");
        bool keepUpwardExposed = (this->tag == Js::BackwardPhase);
        JitArenaAllocator *upwardExposedArena = nullptr;
        if(!IsCollectionPass())
        {LOGMEIN("BackwardPass.cpp] 505\n");
            upwardExposedArena = keepUpwardExposed ? this->globOpt->alloc : this->tempAlloc;
            upwardExposedUses = JitAnew(upwardExposedArena, BVSparse<JitArenaAllocator>, upwardExposedArena);
            upwardExposedFields = JitAnew(upwardExposedArena, BVSparse<JitArenaAllocator>, upwardExposedArena);

            if (this->tag == Js::DeadStorePhase)
            {LOGMEIN("BackwardPass.cpp] 511\n");
                typesNeedingKnownObjectLayout = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }

            if (this->DoFieldHoistCandidates())
            {LOGMEIN("BackwardPass.cpp] 516\n");
                fieldHoistCandidates = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
            if (this->DoDeadStoreSlots())
            {LOGMEIN("BackwardPass.cpp] 520\n");
                slotDeadStoreCandidates = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
            if (this->DoMarkTempNumbers())
            {LOGMEIN("BackwardPass.cpp] 524\n");
                tempNumberTracker = JitAnew(this->tempAlloc, TempNumberTracker, this->tempAlloc,  block->loop != nullptr);
            }
            if (this->DoMarkTempObjects())
            {LOGMEIN("BackwardPass.cpp] 528\n");
                tempObjectTracker = JitAnew(this->tempAlloc, TempObjectTracker, this->tempAlloc, block->loop != nullptr);
            }

            noImplicitCallUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallNoMissingValuesUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallNativeArrayUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallJsArrayHeadSegmentSymUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallArrayLengthSymUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            if (this->tag == Js::BackwardPhase)
            {LOGMEIN("BackwardPass.cpp] 538\n");
                cloneStrCandidates = JitAnew(this->globOpt->alloc, BVSparse<JitArenaAllocator>, this->globOpt->alloc);
            }
            else
            {
                couldRemoveNegZeroBailoutForDef = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
        }

        bool firstSucc = true;
        FOREACH_SUCCESSOR_BLOCK(blockSucc, block)
        {LOGMEIN("BackwardPass.cpp] 549\n");
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)

            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
            // save the byteCodeUpwardExposedUsed from deleting for the block right after the memop loop
            if (this->tag == Js::DeadStorePhase && !this->IsPrePass() && globOpt->HasMemOp(block->loop) && blockSucc->loop != block->loop)
            {LOGMEIN("BackwardPass.cpp] 556\n");
                Assert(block->loop->memOpInfo->inductionVariablesUsedAfterLoop == nullptr);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop->Or(blockSucc->byteCodeUpwardExposedUsed);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop->Or(blockSucc->upwardExposedUses);
            }

            bool deleteData = false;
            if (!blockSucc->isLoopHeader && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
            {LOGMEIN("BackwardPass.cpp] 565\n");
                Assert(blockSucc->GetDataUseCount() != 0);
                deleteData = (blockSucc->DecrementDataUseCount() == 0);
            }

            Assert((byteCodeUpwardExposedUsed == nullptr) == !this->DoByteCodeUpwardExposedUsed());
            if (byteCodeUpwardExposedUsed && blockSucc->byteCodeUpwardExposedUsed)
            {LOGMEIN("BackwardPass.cpp] 572\n");
                byteCodeUpwardExposedUsed->Or(blockSucc->byteCodeUpwardExposedUsed);
                if (this->tag == Js::DeadStorePhase)
                {LOGMEIN("BackwardPass.cpp] 575\n");
#if DBG
                    for (uint i = 0; i < byteCodeLocalsCount; i++)
                    {LOGMEIN("BackwardPass.cpp] 578\n");
                        if (byteCodeRestoreSyms[i] == nullptr)
                        {LOGMEIN("BackwardPass.cpp] 580\n");
                            byteCodeRestoreSyms[i] = blockSucc->byteCodeRestoreSyms[i];
                        }
                        else
                        {
                            Assert(blockSucc->byteCodeRestoreSyms[i] == nullptr
                                || byteCodeRestoreSyms[i] == blockSucc->byteCodeRestoreSyms[i]);
                        }
                    }
#endif
                    if (deleteData)
                    {LOGMEIN("BackwardPass.cpp] 591\n");
                        // byteCodeUpwardExposedUsed is required to populate the writeThroughSymbolsSet for the try region. So, don't delete it in the backwards pass.
                        JitAdelete(this->tempAlloc, blockSucc->byteCodeUpwardExposedUsed);
                        blockSucc->byteCodeUpwardExposedUsed = nullptr;
                    }
                }
#if DBG
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 599\n");
                    JitAdeleteArray(this->tempAlloc, byteCodeLocalsCount, blockSucc->byteCodeRestoreSyms);
                    blockSucc->byteCodeRestoreSyms = nullptr;
                }
#endif

            }
            else
            {
                Assert(blockSucc->byteCodeUpwardExposedUsed == nullptr);
                Assert(blockSucc->byteCodeRestoreSyms == nullptr);
            }

            if(IsCollectionPass())
            {LOGMEIN("BackwardPass.cpp] 613\n");
                continue;
            }

            Assert((blockSucc->upwardExposedUses != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop))));
            Assert((blockSucc->upwardExposedFields != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop))));
            Assert((blockSucc->typesNeedingKnownObjectLayout != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || this->tag != Js::DeadStorePhase);
            Assert((blockSucc->fieldHoistCandidates != nullptr)
                || blockSucc->isLoopHeader
                || !this->DoFieldHoistCandidates(blockSucc->loop));
            Assert((blockSucc->slotDeadStoreCandidates != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || !this->DoDeadStoreSlots());
            Assert((blockSucc->tempNumberTracker != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || !this->DoMarkTempNumbers());
            Assert((blockSucc->tempObjectTracker != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || !this->DoMarkTempObjects());
            Assert((blockSucc->tempObjectVerifyTracker != nullptr)
                || (blockSucc->isLoopHeader && (this->IsPrePass() || blockSucc->loop->IsDescendentOrSelf(block->loop)))
                || !this->DoMarkTempObjectVerify());
            if (blockSucc->upwardExposedUses != nullptr)
            {LOGMEIN("BackwardPass.cpp] 640\n");
                upwardExposedUses->Or(blockSucc->upwardExposedUses);

                if (deleteData && (!keepUpwardExposed
                                   || (this->IsPrePass() && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)))
                {
                    JitAdelete(upwardExposedArena, blockSucc->upwardExposedUses);
                    blockSucc->upwardExposedUses = nullptr;
                }
            }

            if (blockSucc->upwardExposedFields != nullptr)
            {LOGMEIN("BackwardPass.cpp] 652\n");
                upwardExposedFields->Or(blockSucc->upwardExposedFields);

                if (deleteData && (!keepUpwardExposed
                                   || (this->IsPrePass() && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)))
                {
                    JitAdelete(upwardExposedArena, blockSucc->upwardExposedFields);
                    blockSucc->upwardExposedFields = nullptr;
                }
            }

            if (blockSucc->typesNeedingKnownObjectLayout != nullptr)
            {LOGMEIN("BackwardPass.cpp] 664\n");
                typesNeedingKnownObjectLayout->Or(blockSucc->typesNeedingKnownObjectLayout);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 667\n");
                    JitAdelete(this->tempAlloc, blockSucc->typesNeedingKnownObjectLayout);
                    blockSucc->typesNeedingKnownObjectLayout = nullptr;
                }
            }

            if (fieldHoistCandidates && blockSucc->fieldHoistCandidates != nullptr)
            {LOGMEIN("BackwardPass.cpp] 674\n");
                fieldHoistCandidates->Or(blockSucc->fieldHoistCandidates);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 677\n");
                    JitAdelete(this->tempAlloc, blockSucc->fieldHoistCandidates);
                    blockSucc->fieldHoistCandidates = nullptr;
                }
            }
            if (blockSucc->slotDeadStoreCandidates != nullptr)
            {LOGMEIN("BackwardPass.cpp] 683\n");
                slotDeadStoreCandidates->And(blockSucc->slotDeadStoreCandidates);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 686\n");
                    JitAdelete(this->tempAlloc, blockSucc->slotDeadStoreCandidates);
                    blockSucc->slotDeadStoreCandidates = nullptr;
                }
            }
            if (blockSucc->tempNumberTracker != nullptr)
            {LOGMEIN("BackwardPass.cpp] 692\n");
                Assert((blockSucc->loop != nullptr) == blockSucc->tempNumberTracker->HasTempTransferDependencies());
                tempNumberTracker->MergeData(blockSucc->tempNumberTracker, deleteData);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 696\n");
                    blockSucc->tempNumberTracker = nullptr;
                }
            }
            if (blockSucc->tempObjectTracker != nullptr)
            {LOGMEIN("BackwardPass.cpp] 701\n");
                Assert((blockSucc->loop != nullptr) == blockSucc->tempObjectTracker->HasTempTransferDependencies());
                tempObjectTracker->MergeData(blockSucc->tempObjectTracker, deleteData);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 705\n");
                    blockSucc->tempObjectTracker = nullptr;
                }
            }
#if DBG
            if (blockSucc->tempObjectVerifyTracker != nullptr)
            {LOGMEIN("BackwardPass.cpp] 711\n");
                Assert((blockSucc->loop != nullptr) == blockSucc->tempObjectVerifyTracker->HasTempTransferDependencies());
                tempObjectVerifyTracker->MergeData(blockSucc->tempObjectVerifyTracker, deleteData);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 715\n");
                    blockSucc->tempObjectVerifyTracker = nullptr;
                }
            }
#endif

            PHASE_PRINT_TRACE(Js::ObjTypeSpecStorePhase, this->func,
                              _u("ObjTypeSpecStore: func %s, edge %d => %d: "),
                              this->func->GetDebugNumberSet(debugStringBuffer),
                              block->GetBlockNum(), blockSucc->GetBlockNum());

            auto fixupFrom = [block, blockSucc, this](Bucket<AddPropertyCacheBucket> &bucket)
            {LOGMEIN("BackwardPass.cpp] 727\n");
                AddPropertyCacheBucket *fromData = &bucket.element;
                if (fromData->GetInitialType() == nullptr ||
                    fromData->GetFinalType() == fromData->GetInitialType())
                {LOGMEIN("BackwardPass.cpp] 731\n");
                    return;
                }

                this->InsertTypeTransitionsAtPriorSuccessors(block, blockSucc, bucket.value, fromData);
            };

            auto fixupTo = [blockSucc, this](Bucket<AddPropertyCacheBucket> &bucket)
            {LOGMEIN("BackwardPass.cpp] 739\n");
                AddPropertyCacheBucket *toData = &bucket.element;
                if (toData->GetInitialType() == nullptr ||
                    toData->GetFinalType() == toData->GetInitialType())
                {LOGMEIN("BackwardPass.cpp] 743\n");
                    return;
                }

                this->InsertTypeTransitionAtBlock(blockSucc, bucket.value, toData);
            };

            if (blockSucc->stackSymToFinalType != nullptr)
            {LOGMEIN("BackwardPass.cpp] 751\n");
#if DBG_DUMP
                if (PHASE_TRACE(Js::ObjTypeSpecStorePhase, this->func))
                {LOGMEIN("BackwardPass.cpp] 754\n");
                    blockSucc->stackSymToFinalType->Dump();
                }
#endif
                if (firstSucc)
                {LOGMEIN("BackwardPass.cpp] 759\n");
                    stackSymToFinalType = blockSucc->stackSymToFinalType->Copy();
                }
                else if (stackSymToFinalType != nullptr)
                {LOGMEIN("BackwardPass.cpp] 763\n");
                    if (this->IsPrePass())
                    {LOGMEIN("BackwardPass.cpp] 765\n");
                        stackSymToFinalType->And(blockSucc->stackSymToFinalType);
                    }
                    else
                    {
                        // Insert any type transitions that can't be merged past this point.
                        stackSymToFinalType->AndWithFixup(blockSucc->stackSymToFinalType, fixupFrom, fixupTo);
                    }
                }
                else if (!this->IsPrePass())
                {
                    FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, blockSucc->stackSymToFinalType)
                    {LOGMEIN("BackwardPass.cpp] 777\n");
                        fixupTo(bucket);
                    }
                    NEXT_HASHTABLE_ENTRY;
                }

                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 784\n");
                    blockSucc->stackSymToFinalType->Delete();
                    blockSucc->stackSymToFinalType = nullptr;
                }
            }
            else
            {
                PHASE_PRINT_TRACE(Js::ObjTypeSpecStorePhase, this->func, _u("null\n"));
                if (stackSymToFinalType)
                {LOGMEIN("BackwardPass.cpp] 793\n");
                    if (!this->IsPrePass())
                    {
                        FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, stackSymToFinalType)
                        {LOGMEIN("BackwardPass.cpp] 797\n");
                            fixupFrom(bucket);
                        }
                        NEXT_HASHTABLE_ENTRY;
                    }

                    stackSymToFinalType->Delete();
                    stackSymToFinalType = nullptr;
                }
            }
            if (tag == Js::BackwardPhase)
            {LOGMEIN("BackwardPass.cpp] 808\n");
                if (blockSucc->cloneStrCandidates != nullptr)
                {LOGMEIN("BackwardPass.cpp] 810\n");
                    Assert(cloneStrCandidates != nullptr);
                    cloneStrCandidates->Or(blockSucc->cloneStrCandidates);
                    if (deleteData)
                    {LOGMEIN("BackwardPass.cpp] 814\n");
                        JitAdelete(this->globOpt->alloc, blockSucc->cloneStrCandidates);
                        blockSucc->cloneStrCandidates = nullptr;
                    }
                }
#if DBG_DUMP
                if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                {LOGMEIN("BackwardPass.cpp] 821\n");
                    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Output::Print(_u("ObjTypeSpec: top function %s (%s), function %s (%s), write guard symbols on edge %d => %d: "),
                        this->func->GetTopFunc()->GetJITFunctionBody()->GetDisplayName(),
                        this->func->GetTopFunc()->GetDebugNumberSet(debugStringBuffer),
                        this->func->GetJITFunctionBody()->GetDisplayName(),
                        this->func->GetDebugNumberSet(debugStringBuffer2), block->GetBlockNum(),
                        blockSucc->GetBlockNum());
                }
#endif
                if (blockSucc->stackSymToWriteGuardsMap != nullptr)
                {LOGMEIN("BackwardPass.cpp] 832\n");
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                    {LOGMEIN("BackwardPass.cpp] 835\n");
                        Output::Print(_u("\n"));
                        blockSucc->stackSymToWriteGuardsMap->Dump();
                    }
#endif
                    if (stackSymToWriteGuardsMap == nullptr)
                    {LOGMEIN("BackwardPass.cpp] 841\n");
                        stackSymToWriteGuardsMap = blockSucc->stackSymToWriteGuardsMap->Copy();
                    }
                    else
                    {
                        stackSymToWriteGuardsMap->Or(
                            blockSucc->stackSymToWriteGuardsMap, &BackwardPass::MergeWriteGuards);
                    }

                    if (deleteData)
                    {LOGMEIN("BackwardPass.cpp] 851\n");
                        blockSucc->stackSymToWriteGuardsMap->Delete();
                        blockSucc->stackSymToWriteGuardsMap = nullptr;
                    }
                }
                else
                {
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                    {LOGMEIN("BackwardPass.cpp] 860\n");
                        Output::Print(_u("null\n"));
                    }
#endif
                }
            }
            else
            {
#if DBG_DUMP
                if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                {LOGMEIN("BackwardPass.cpp] 870\n");
                    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Output::Print(_u("ObjTypeSpec: top function %s (%s), function %s (%s), guarded property operations on edge %d => %d: \n"),
                        this->func->GetTopFunc()->GetJITFunctionBody()->GetDisplayName(),
                        this->func->GetTopFunc()->GetDebugNumberSet(debugStringBuffer),
                        this->func->GetJITFunctionBody()->GetDisplayName(),
                        this->func->GetDebugNumberSet(debugStringBuffer2),
                        block->GetBlockNum(), blockSucc->GetBlockNum());
                }
#endif
                if (blockSucc->stackSymToGuardedProperties != nullptr)
                {LOGMEIN("BackwardPass.cpp] 881\n");
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                    {LOGMEIN("BackwardPass.cpp] 884\n");
                        blockSucc->stackSymToGuardedProperties->Dump();
                        Output::Print(_u("\n"));
                    }
#endif
                    if (stackSymToGuardedProperties == nullptr)
                    {LOGMEIN("BackwardPass.cpp] 890\n");
                        stackSymToGuardedProperties = blockSucc->stackSymToGuardedProperties->Copy();
                    }
                    else
                    {
                        stackSymToGuardedProperties->Or(
                            blockSucc->stackSymToGuardedProperties, &BackwardPass::MergeGuardedProperties);
                    }

                    if (deleteData)
                    {LOGMEIN("BackwardPass.cpp] 900\n");
                        blockSucc->stackSymToGuardedProperties->Delete();
                        blockSucc->stackSymToGuardedProperties = nullptr;
                    }
                }
                else
                {
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                    {LOGMEIN("BackwardPass.cpp] 909\n");
                        Output::Print(_u("null\n"));
                    }
#endif
                }

                if (blockSucc->couldRemoveNegZeroBailoutForDef != nullptr)
                {LOGMEIN("BackwardPass.cpp] 916\n");
                    couldRemoveNegZeroBailoutForDef->And(blockSucc->couldRemoveNegZeroBailoutForDef);
                    if (deleteData)
                    {LOGMEIN("BackwardPass.cpp] 919\n");
                        JitAdelete(this->tempAlloc, blockSucc->couldRemoveNegZeroBailoutForDef);
                        blockSucc->couldRemoveNegZeroBailoutForDef = nullptr;
                    }
                }
            }

            if (blockSucc->noImplicitCallUses != nullptr)
            {LOGMEIN("BackwardPass.cpp] 927\n");
                noImplicitCallUses->Or(blockSucc->noImplicitCallUses);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 930\n");
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallUses);
                    blockSucc->noImplicitCallUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallNoMissingValuesUses != nullptr)
            {LOGMEIN("BackwardPass.cpp] 936\n");
                noImplicitCallNoMissingValuesUses->Or(blockSucc->noImplicitCallNoMissingValuesUses);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 939\n");
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallNoMissingValuesUses);
                    blockSucc->noImplicitCallNoMissingValuesUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallNativeArrayUses != nullptr)
            {LOGMEIN("BackwardPass.cpp] 945\n");
                noImplicitCallNativeArrayUses->Or(blockSucc->noImplicitCallNativeArrayUses);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 948\n");
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallNativeArrayUses);
                    blockSucc->noImplicitCallNativeArrayUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallJsArrayHeadSegmentSymUses != nullptr)
            {LOGMEIN("BackwardPass.cpp] 954\n");
                noImplicitCallJsArrayHeadSegmentSymUses->Or(blockSucc->noImplicitCallJsArrayHeadSegmentSymUses);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 957\n");
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallJsArrayHeadSegmentSymUses);
                    blockSucc->noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallArrayLengthSymUses != nullptr)
            {LOGMEIN("BackwardPass.cpp] 963\n");
                noImplicitCallArrayLengthSymUses->Or(blockSucc->noImplicitCallArrayLengthSymUses);
                if (deleteData)
                {LOGMEIN("BackwardPass.cpp] 966\n");
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallArrayLengthSymUses);
                    blockSucc->noImplicitCallArrayLengthSymUses = nullptr;
                }
            }

            firstSucc = false;
        }
        NEXT_SUCCESSOR_BLOCK;

#if DBG_DUMP
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        if (PHASE_TRACE(Js::ObjTypeSpecStorePhase, this->func))
        {LOGMEIN("BackwardPass.cpp] 979\n");
            Output::Print(_u("ObjTypeSpecStore: func %s, block %d: "),
                          this->func->GetDebugNumberSet(debugStringBuffer),
                          block->GetBlockNum());
            if (stackSymToFinalType)
            {LOGMEIN("BackwardPass.cpp] 984\n");
                stackSymToFinalType->Dump();
            }
            else
            {
                Output::Print(_u("null\n"));
            }
        }

        if (PHASE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
        {LOGMEIN("BackwardPass.cpp] 994\n");
            Output::Print(_u("ObjTypeSpec: func %s, block %d, guarded properties:\n"),
                this->func->GetDebugNumberSet(debugStringBuffer), block->GetBlockNum());
            if (stackSymToGuardedProperties)
            {LOGMEIN("BackwardPass.cpp] 998\n");
                stackSymToGuardedProperties->Dump();
                Output::Print(_u("\n"));
            }
            else
            {
                Output::Print(_u("null\n"));
            }
        }

        if (PHASE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
        {LOGMEIN("BackwardPass.cpp] 1009\n");
            Output::Print(_u("ObjTypeSpec: func %s, block %d, write guards: "),
                this->func->GetDebugNumberSet(debugStringBuffer), block->GetBlockNum());
            if (stackSymToWriteGuardsMap)
            {LOGMEIN("BackwardPass.cpp] 1013\n");
                Output::Print(_u("\n"));
                stackSymToWriteGuardsMap->Dump();
                Output::Print(_u("\n"));
            }
            else
            {
                Output::Print(_u("null\n"));
            }
        }
#endif
    }

#if DBG
    if (tempObjectVerifyTracker)
    {
        FOREACH_DEAD_SUCCESSOR_BLOCK(deadBlockSucc, block)
        {LOGMEIN("BackwardPass.cpp] 1030\n");
            Assert(deadBlockSucc->tempObjectVerifyTracker || deadBlockSucc->isLoopHeader);
            if (deadBlockSucc->tempObjectVerifyTracker != nullptr)
            {LOGMEIN("BackwardPass.cpp] 1033\n");
                Assert((deadBlockSucc->loop != nullptr) == deadBlockSucc->tempObjectVerifyTracker->HasTempTransferDependencies());
                // Dead block don't effect non temp use,  we only need to carry the removed use bit vector forward
                // and put all the upward exposed use to the set that we might found out to be mark temp
                // after globopt
                tempObjectVerifyTracker->MergeDeadData(deadBlockSucc);
            }

            if (!byteCodeUpwardExposedUsed)
            {LOGMEIN("BackwardPass.cpp] 1042\n");
                if (!deadBlockSucc->isLoopHeader && deadBlockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
                {LOGMEIN("BackwardPass.cpp] 1044\n");
                    Assert(deadBlockSucc->GetDataUseCount() != 0);
                    if (deadBlockSucc->DecrementDataUseCount() == 0)
                    {LOGMEIN("BackwardPass.cpp] 1047\n");
                        this->DeleteBlockData(deadBlockSucc);
                    }
                }
            }
        }
        NEXT_DEAD_SUCCESSOR_BLOCK;
    }
#endif

    if (byteCodeUpwardExposedUsed)
    {
        FOREACH_DEAD_SUCCESSOR_BLOCK(deadBlockSucc, block)
        {LOGMEIN("BackwardPass.cpp] 1060\n");
            Assert(deadBlockSucc->byteCodeUpwardExposedUsed || deadBlockSucc->isLoopHeader);
            if (deadBlockSucc->byteCodeUpwardExposedUsed)
            {LOGMEIN("BackwardPass.cpp] 1063\n");
                byteCodeUpwardExposedUsed->Or(deadBlockSucc->byteCodeUpwardExposedUsed);
                if (this->tag == Js::DeadStorePhase)
                {LOGMEIN("BackwardPass.cpp] 1066\n");
#if DBG
                    for (uint i = 0; i < byteCodeLocalsCount; i++)
                    {LOGMEIN("BackwardPass.cpp] 1069\n");
                        if (byteCodeRestoreSyms[i] == nullptr)
                        {LOGMEIN("BackwardPass.cpp] 1071\n");
                            byteCodeRestoreSyms[i] = deadBlockSucc->byteCodeRestoreSyms[i];
                        }
                        else
                        {
                            Assert(deadBlockSucc->byteCodeRestoreSyms[i] == nullptr
                                || byteCodeRestoreSyms[i] == deadBlockSucc->byteCodeRestoreSyms[i]);
                        }
                    }
#endif
                }
            }

            if (!deadBlockSucc->isLoopHeader && deadBlockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
            {LOGMEIN("BackwardPass.cpp] 1085\n");
                Assert(deadBlockSucc->GetDataUseCount() != 0);
                if (deadBlockSucc->DecrementDataUseCount() == 0)
                {LOGMEIN("BackwardPass.cpp] 1088\n");
                    this->DeleteBlockData(deadBlockSucc);
                }
            }
        }
        NEXT_DEAD_SUCCESSOR_BLOCK;
    }

    if (block->isLoopHeader)
    {LOGMEIN("BackwardPass.cpp] 1097\n");
        this->DeleteBlockData(block);
    }
    else
    {
        if(block->GetDataUseCount() == 0)
        {LOGMEIN("BackwardPass.cpp] 1103\n");
            Assert(block->slotDeadStoreCandidates == nullptr);
            Assert(block->tempNumberTracker == nullptr);
            Assert(block->tempObjectTracker == nullptr);
            Assert(block->tempObjectVerifyTracker == nullptr);
            Assert(block->upwardExposedUses == nullptr);
            Assert(block->upwardExposedFields == nullptr);
            Assert(block->typesNeedingKnownObjectLayout == nullptr);
            Assert(block->fieldHoistCandidates == nullptr);
            // byteCodeUpwardExposedUsed is required to populate the writeThroughSymbolsSet for the try region in the backwards pass
            Assert(block->byteCodeUpwardExposedUsed == nullptr || (this->tag == Js::BackwardPhase && this->func->HasTry() && this->func->DoOptimizeTryCatch()));
            Assert(block->byteCodeRestoreSyms == nullptr);
            Assert(block->stackSymToFinalType == nullptr);
            Assert(block->stackSymToGuardedProperties == nullptr);
            Assert(block->stackSymToWriteGuardsMap == nullptr);
            Assert(block->cloneStrCandidates == nullptr);
            Assert(block->noImplicitCallUses == nullptr);
            Assert(block->noImplicitCallNoMissingValuesUses == nullptr);
            Assert(block->noImplicitCallNativeArrayUses == nullptr);
            Assert(block->noImplicitCallJsArrayHeadSegmentSymUses == nullptr);
            Assert(block->noImplicitCallArrayLengthSymUses == nullptr);
            Assert(block->couldRemoveNegZeroBailoutForDef == nullptr);
        }
        else
        {
            // The collection pass sometimes does not know whether it can delete a successor block's data, so it may leave some
            // blocks with data intact. Delete the block data now.
            Assert(block->backwardPassCurrentLoop);
            Assert(block->backwardPassCurrentLoop->hasDeadStoreCollectionPass);
            Assert(!block->backwardPassCurrentLoop->hasDeadStorePrepass);

            DeleteBlockData(block);
        }

        block->backwardPassCurrentLoop = this->currentPrePassLoop;

        if (this->DoByteCodeUpwardExposedUsed()
#if DBG
            || this->DoMarkTempObjectVerify()
#endif
            )
        {LOGMEIN("BackwardPass.cpp] 1144\n");
            block->SetDataUseCount(block->GetPredList()->Count() + block->GetDeadPredList()->Count());
        }
        else
        {
            block->SetDataUseCount(block->GetPredList()->Count());
        }
    }
    block->upwardExposedUses = upwardExposedUses;
    block->upwardExposedFields = upwardExposedFields;
    block->typesNeedingKnownObjectLayout = typesNeedingKnownObjectLayout;
    block->fieldHoistCandidates = fieldHoistCandidates;
    block->byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed;
#if DBG
    block->byteCodeRestoreSyms = byteCodeRestoreSyms;
#endif
    block->slotDeadStoreCandidates = slotDeadStoreCandidates;

    block->tempNumberTracker = tempNumberTracker;
    block->tempObjectTracker = tempObjectTracker;
#if DBG
    block->tempObjectVerifyTracker = tempObjectVerifyTracker;
#endif
    block->stackSymToFinalType = stackSymToFinalType;
    block->stackSymToGuardedProperties = stackSymToGuardedProperties;
    block->stackSymToWriteGuardsMap = stackSymToWriteGuardsMap;
    block->cloneStrCandidates = cloneStrCandidates;
    block->noImplicitCallUses = noImplicitCallUses;
    block->noImplicitCallNoMissingValuesUses = noImplicitCallNoMissingValuesUses;
    block->noImplicitCallNativeArrayUses = noImplicitCallNativeArrayUses;
    block->noImplicitCallJsArrayHeadSegmentSymUses = noImplicitCallJsArrayHeadSegmentSymUses;
    block->noImplicitCallArrayLengthSymUses = noImplicitCallArrayLengthSymUses;
    block->couldRemoveNegZeroBailoutForDef = couldRemoveNegZeroBailoutForDef;
}

ObjTypeGuardBucket
BackwardPass::MergeGuardedProperties(ObjTypeGuardBucket bucket1, ObjTypeGuardBucket bucket2)
{LOGMEIN("BackwardPass.cpp] 1181\n");
    BVSparse<JitArenaAllocator> *guardedPropertyOps1 = bucket1.GetGuardedPropertyOps();
    BVSparse<JitArenaAllocator> *guardedPropertyOps2 = bucket2.GetGuardedPropertyOps();
    Assert(guardedPropertyOps1 || guardedPropertyOps2);

    BVSparse<JitArenaAllocator> *mergedPropertyOps;
    if (guardedPropertyOps1)
    {LOGMEIN("BackwardPass.cpp] 1188\n");
        mergedPropertyOps = guardedPropertyOps1->CopyNew();
        if (guardedPropertyOps2)
        {LOGMEIN("BackwardPass.cpp] 1191\n");
            mergedPropertyOps->Or(guardedPropertyOps2);
        }
    }
    else
    {
        mergedPropertyOps = guardedPropertyOps2->CopyNew();
    }

    ObjTypeGuardBucket bucket;
    bucket.SetGuardedPropertyOps(mergedPropertyOps);
    JITTypeHolder monoGuardType = bucket1.GetMonoGuardType();
    if (monoGuardType != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1204\n");
        Assert(!bucket2.NeedsMonoCheck() || monoGuardType == bucket2.GetMonoGuardType());
    }
    else
    {
        monoGuardType = bucket2.GetMonoGuardType();
    }
    bucket.SetMonoGuardType(monoGuardType);

    return bucket;
}

ObjWriteGuardBucket
BackwardPass::MergeWriteGuards(ObjWriteGuardBucket bucket1, ObjWriteGuardBucket bucket2)
{LOGMEIN("BackwardPass.cpp] 1218\n");
    BVSparse<JitArenaAllocator> *writeGuards1 = bucket1.GetWriteGuards();
    BVSparse<JitArenaAllocator> *writeGuards2 = bucket2.GetWriteGuards();
    Assert(writeGuards1 || writeGuards2);

    BVSparse<JitArenaAllocator> *mergedWriteGuards;
    if (writeGuards1)
    {LOGMEIN("BackwardPass.cpp] 1225\n");
        mergedWriteGuards = writeGuards1->CopyNew();
        if (writeGuards2)
        {LOGMEIN("BackwardPass.cpp] 1228\n");
            mergedWriteGuards->Or(writeGuards2);
        }
    }
    else
    {
        mergedWriteGuards = writeGuards2->CopyNew();
    }

    ObjWriteGuardBucket bucket;
    bucket.SetWriteGuards(mergedWriteGuards);
    return bucket;
}

void
BackwardPass::DeleteBlockData(BasicBlock * block)
{LOGMEIN("BackwardPass.cpp] 1244\n");
    if (block->slotDeadStoreCandidates != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1246\n");
        JitAdelete(this->tempAlloc, block->slotDeadStoreCandidates);
        block->slotDeadStoreCandidates = nullptr;
    }
    if (block->tempNumberTracker != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1251\n");
        JitAdelete(this->tempAlloc, block->tempNumberTracker);
        block->tempNumberTracker = nullptr;
    }
    if (block->tempObjectTracker != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1256\n");
        JitAdelete(this->tempAlloc, block->tempObjectTracker);
        block->tempObjectTracker = nullptr;
    }
#if DBG
    if (block->tempObjectVerifyTracker != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1262\n");
        JitAdelete(this->tempAlloc, block->tempObjectVerifyTracker);
        block->tempObjectVerifyTracker = nullptr;
    }
#endif
    if (block->stackSymToFinalType != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1268\n");
        block->stackSymToFinalType->Delete();
        block->stackSymToFinalType = nullptr;
    }
    if (block->stackSymToGuardedProperties != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1273\n");
        block->stackSymToGuardedProperties->Delete();
        block->stackSymToGuardedProperties = nullptr;
    }
    if (block->stackSymToWriteGuardsMap != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1278\n");
        block->stackSymToWriteGuardsMap->Delete();
        block->stackSymToWriteGuardsMap = nullptr;
    }
    if (block->cloneStrCandidates != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1283\n");
        Assert(this->tag == Js::BackwardPhase);
        JitAdelete(this->globOpt->alloc, block->cloneStrCandidates);
        block->cloneStrCandidates = nullptr;
    }
    if (block->noImplicitCallUses != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1289\n");
        JitAdelete(this->tempAlloc, block->noImplicitCallUses);
        block->noImplicitCallUses = nullptr;
    }
    if (block->noImplicitCallNoMissingValuesUses != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1294\n");
        JitAdelete(this->tempAlloc, block->noImplicitCallNoMissingValuesUses);
        block->noImplicitCallNoMissingValuesUses = nullptr;
    }
    if (block->noImplicitCallNativeArrayUses != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1299\n");
        JitAdelete(this->tempAlloc, block->noImplicitCallNativeArrayUses);
        block->noImplicitCallNativeArrayUses = nullptr;
    }
    if (block->noImplicitCallJsArrayHeadSegmentSymUses != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1304\n");
        JitAdelete(this->tempAlloc, block->noImplicitCallJsArrayHeadSegmentSymUses);
        block->noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
    }
    if (block->noImplicitCallArrayLengthSymUses != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1309\n");
        JitAdelete(this->tempAlloc, block->noImplicitCallArrayLengthSymUses);
        block->noImplicitCallArrayLengthSymUses = nullptr;
    }
    if (block->upwardExposedUses != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1314\n");
        JitArenaAllocator *upwardExposedArena = (this->tag == Js::BackwardPhase) ? this->globOpt->alloc : this->tempAlloc;
        JitAdelete(upwardExposedArena, block->upwardExposedUses);
        block->upwardExposedUses = nullptr;
    }
    if (block->upwardExposedFields != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1320\n");
        JitArenaAllocator *upwardExposedArena = (this->tag == Js::BackwardPhase) ? this->globOpt->alloc : this->tempAlloc;
        JitAdelete(upwardExposedArena, block->upwardExposedFields);
        block->upwardExposedFields = nullptr;
    }
    if (block->typesNeedingKnownObjectLayout != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1326\n");
        JitAdelete(this->tempAlloc, block->typesNeedingKnownObjectLayout);
        block->typesNeedingKnownObjectLayout = nullptr;
    }
    if (block->fieldHoistCandidates != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1331\n");
        JitAdelete(this->tempAlloc, block->fieldHoistCandidates);
        block->fieldHoistCandidates = nullptr;
    }
    if (block->byteCodeUpwardExposedUsed != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1336\n");
        JitAdelete(this->tempAlloc, block->byteCodeUpwardExposedUsed);
        block->byteCodeUpwardExposedUsed = nullptr;
#if DBG
        JitAdeleteArray(this->tempAlloc, func->GetJITFunctionBody()->GetLocalsCount(), block->byteCodeRestoreSyms);
        block->byteCodeRestoreSyms = nullptr;
#endif
    }
    if (block->couldRemoveNegZeroBailoutForDef != nullptr)
    {LOGMEIN("BackwardPass.cpp] 1345\n");
        JitAdelete(this->tempAlloc, block->couldRemoveNegZeroBailoutForDef);
        block->couldRemoveNegZeroBailoutForDef = nullptr;
    }
}

void
BackwardPass::ProcessLoopCollectionPass(BasicBlock *const lastBlock)
{LOGMEIN("BackwardPass.cpp] 1353\n");
    // The collection pass is done before the prepass, to collect and propagate a minimal amount of information into nested
    // loops, for cases where the information is needed to make appropriate decisions on changing other state. For instance,
    // bailouts in nested loops need to be able to see all byte-code uses that are exposed to the bailout so that the
    // appropriate syms can be made upwards-exposed during the prepass. Byte-code uses that occur before the bailout in the
    // flow, or byte-code uses after the current loop, are not seen by bailouts inside the loop. The collection pass collects
    // byte-code uses and propagates them at least into each loop's header such that when bailouts are processed in the prepass,
    // they will have full visibility of byte-code upwards-exposed uses.
    //
    // For the collection pass, one pass is needed to collect all byte-code uses of a loop to the loop header. If the loop has
    // inner loops, another pass is needed to propagate byte-code uses in the outer loop into the inner loop's header, since
    // some byte-code uses may occur before the inner loop in the flow. The process continues recursively for inner loops. The
    // second pass only needs to walk as far as the first inner loop's header, since the purpose of that pass is only to
    // propagate collected information into the inner loops' headers.
    //
    // Consider the following case:
    //   (Block 1, Loop 1 header)
    //     ByteCodeUses s1
    //       (Block 2, Loop 2 header)
    //           (Block 3, Loop 3 header)
    //           (Block 4)
    //             BailOut
    //           (Block 5, Loop 3 back-edge)
    //       (Block 6, Loop 2 back-edge)
    //   (Block 7, Loop 1 back-edge)
    //
    // Assume that the exit branch in each of these loops is in the loop's header block, like a 'while' loop. For the byte-code
    // use of 's1' to become visible to the bailout in the innermost loop, we need to walk the following blocks:
    // - Collection pass
    //     - 7, 6, 5, 4, 3, 2, 1, 7 - block 1 is the first block in loop 1 that sees 's1', and since block 7 has block 1 as its
    //       successor, block 7 sees 's1' now as well
    //     - 6, 5, 4, 3, 2, 6 -  block 2 is the first block in loop 2 that sees 's1', and since block 6 has block 2 as its
    //       successor, block 6 sees 's1' now as well
    //     - 5, 4, 3 - block 3 is the first block in loop 3 that sees 's1'
    //     - The collection pass does not have to do another pass through the innermost loop because it does not have any inner
    //       loops of its own. It's sufficient to propagate the byte-code uses up to the loop header of each loop, as the
    //       prepass will do the remaining propagation.
    // - Prepass
    //     - 7, 6, 5, 4, ... - since block 5 has block 3 as its successor, block 5 sees 's1', and so does block 4. So, the bailout
    //       finally sees 's1' as a byte-code upwards-exposed use.
    //
    // The collection pass walks as described above, and consists of one pass, followed by another pass if there are inner
    // loops. The second pass only walks up to the first inner loop's header block, and during this pass upon reaching an inner
    // loop, the algorithm goes recursively for that inner loop, and once it returns, the second pass continues from above that
    // inner loop. Each bullet of the walk in the example above is a recursive call to ProcessLoopCollectionPass, except the
    // first line, which is the initial call.
    //
    // Imagine the whole example above is inside another loop, and at the bottom of that loop there is an assignment to 's1'. If
    // the bailout is the only use of 's1', then it needs to register 's1' as a use in the prepass to prevent treating the
    // assignment to 's1' as a dead store.

    Assert(tag == Js::DeadStorePhase);
    Assert(IsCollectionPass());
    Assert(lastBlock);

    Loop *const collectionPassLoop = lastBlock->loop;
    Assert(collectionPassLoop);
    Assert(!collectionPassLoop->hasDeadStoreCollectionPass);
    collectionPassLoop->hasDeadStoreCollectionPass = true;

    Loop *const previousPrepassLoop = currentPrePassLoop;
    currentPrePassLoop = collectionPassLoop;
    Assert(IsPrePass());

    // First pass
    BasicBlock *firstInnerLoopHeader = nullptr;
    {
#if DBG_DUMP
        if(IsTraceEnabled())
        {LOGMEIN("BackwardPass.cpp] 1422\n");
            Output::Print(_u("******* COLLECTION PASS 1 START: Loop %u ********\n"), collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif

        FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, nullptr)
        {LOGMEIN("BackwardPass.cpp] 1428\n");
            ProcessBlock(block);

            if(block->isLoopHeader)
            {LOGMEIN("BackwardPass.cpp] 1432\n");
                if(block->loop == collectionPassLoop)
                {LOGMEIN("BackwardPass.cpp] 1434\n");
                    break;
                }

                // Keep track of the first inner loop's header for the second pass, which need only walk up to that block
                firstInnerLoopHeader = block;
            }
        } NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE;

#if DBG_DUMP
        if(IsTraceEnabled())
        {LOGMEIN("BackwardPass.cpp] 1445\n");
            Output::Print(_u("******** COLLECTION PASS 1 END: Loop %u *********\n"), collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif
    }

    // Second pass, only needs to run if there are any inner loops, to propagate collected information into those loops
    if(firstInnerLoopHeader)
    {LOGMEIN("BackwardPass.cpp] 1453\n");
#if DBG_DUMP
        if(IsTraceEnabled())
        {LOGMEIN("BackwardPass.cpp] 1456\n");
            Output::Print(_u("******* COLLECTION PASS 2 START: Loop %u ********\n"), collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif

        FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, firstInnerLoopHeader)
        {LOGMEIN("BackwardPass.cpp] 1462\n");
            Loop *const loop = block->loop;
            if(loop && loop != collectionPassLoop && !loop->hasDeadStoreCollectionPass)
            {LOGMEIN("BackwardPass.cpp] 1465\n");
                // About to make a recursive call, so when jitting in the foreground, probe the stack
                if(!func->IsBackgroundJIT())
                {LOGMEIN("BackwardPass.cpp] 1468\n");
                    PROBE_STACK(func->GetScriptContext(), Js::Constants::MinStackDefault);
                }
                ProcessLoopCollectionPass(block);

                // The inner loop's collection pass would have propagated collected information to its header block. Skip to the
                // inner loop's header block and continue from the block before it.
                block = loop->GetHeadBlock();
                Assert(block->isLoopHeader);
                continue;
            }

            ProcessBlock(block);
        } NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE;

#if DBG_DUMP
        if(IsTraceEnabled())
        {LOGMEIN("BackwardPass.cpp] 1485\n");
            Output::Print(_u("******** COLLECTION PASS 2 END: Loop %u *********\n"), collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif
    }

    currentPrePassLoop = previousPrepassLoop;
}

void
BackwardPass::ProcessLoop(BasicBlock * lastBlock)
{LOGMEIN("BackwardPass.cpp] 1496\n");
#if DBG_DUMP
    if (this->IsTraceEnabled())
    {LOGMEIN("BackwardPass.cpp] 1499\n");
        Output::Print(_u("******* PREPASS START ********\n"));
    }
#endif

    Loop *loop = lastBlock->loop;

    // This code doesn't work quite as intended. It is meant to capture fields that are live out of a loop to limit the
    // number of implicit call bailouts the forward pass must create (only compiler throughput optimization, no impact
    // on emitted code), but because it looks only at the lexically last block in the loop, it does the right thing only
    // for do-while loops. For other loops (for and while) the last block does not exit the loop. Even for do-while loops
    // this tracking can have the adverse effect of killing fields that should stay live after copy prop. Disabled by default.
    // Left in under a flag, in case we find compiler throughput issues and want to do additional experiments.
    if (PHASE_ON(Js::LiveOutFieldsPhase, this->func))
    {LOGMEIN("BackwardPass.cpp] 1513\n");
        if (this->globOpt->DoFieldOpts(loop) || this->globOpt->DoFieldRefOpts(loop))
        {LOGMEIN("BackwardPass.cpp] 1515\n");
            // Get the live-out set at the loop bottom.
            // This may not be the only loop exit, but all loop exits either leave the function or pass through here.
            // In the forward pass, we'll use this set to trim the live fields on exit from the loop
            // in order to limit the number of bailout points following the loop.
            BVSparse<JitArenaAllocator> *bv = JitAnew(this->func->m_fg->alloc, BVSparse<JitArenaAllocator>, this->func->m_fg->alloc);
            FOREACH_SUCCESSOR_BLOCK(blockSucc, lastBlock)
            {LOGMEIN("BackwardPass.cpp] 1522\n");
                if (blockSucc->loop != loop)
                {LOGMEIN("BackwardPass.cpp] 1524\n");
                    // Would like to assert this, but in strange exprgen cases involving "break LABEL" in nested
                    // loops the loop graph seems to get confused.
                    //Assert(!blockSucc->loop || blockSucc->loop->IsDescendentOrSelf(loop));
                    Assert(!blockSucc->loop || blockSucc->loop->hasDeadStorePrepass);

                    bv->Or(blockSucc->upwardExposedFields);
                }
            }
            NEXT_SUCCESSOR_BLOCK;
            lastBlock->loop->liveOutFields = bv;
        }
    }

    if(tag == Js::DeadStorePhase && !loop->hasDeadStoreCollectionPass)
    {LOGMEIN("BackwardPass.cpp] 1539\n");
        Assert(!IsCollectionPass());
        Assert(!IsPrePass());
        isCollectionPass = true;
        ProcessLoopCollectionPass(lastBlock);
        isCollectionPass = false;
    }

    Assert(!this->IsPrePass());
    this->currentPrePassLoop = loop;

    FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, nullptr)
    {LOGMEIN("BackwardPass.cpp] 1551\n");
        this->ProcessBlock(block);

        if (block->isLoopHeader && block->loop == lastBlock->loop)
        {LOGMEIN("BackwardPass.cpp] 1555\n");
            Assert(block->fieldHoistCandidates == nullptr);
            break;
        }
    }
    NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE;

    this->currentPrePassLoop = nullptr;
    Assert(lastBlock);
    __analysis_assume(lastBlock);
    lastBlock->loop->hasDeadStorePrepass = true;

#if DBG_DUMP
    if (this->IsTraceEnabled())
    {LOGMEIN("BackwardPass.cpp] 1569\n");
        Output::Print(_u("******** PREPASS END *********\n"));
    }
#endif
}

void
BackwardPass::OptBlock(BasicBlock * block)
{LOGMEIN("BackwardPass.cpp] 1577\n");
    this->func->ThrowIfScriptClosed();

    if (block->loop && !block->loop->hasDeadStorePrepass)
    {LOGMEIN("BackwardPass.cpp] 1581\n");
        ProcessLoop(block);
    }

    this->ProcessBlock(block);

    if(DoTrackNegativeZero())
    {LOGMEIN("BackwardPass.cpp] 1588\n");
        negativeZeroDoesNotMatterBySymId->ClearAll();
    }
    if (DoTrackBitOpsOrNumber())
    {LOGMEIN("BackwardPass.cpp] 1592\n");
        symUsedOnlyForBitOpsBySymId->ClearAll();
        symUsedOnlyForNumberBySymId->ClearAll();
    }

    if(DoTrackIntOverflow())
    {LOGMEIN("BackwardPass.cpp] 1598\n");
        intOverflowDoesNotMatterBySymId->ClearAll();
        if(DoTrackCompoundedIntOverflow())
        {LOGMEIN("BackwardPass.cpp] 1601\n");
            intOverflowDoesNotMatterInRangeBySymId->ClearAll();
        }
    }
}

void
BackwardPass::ProcessBailOutArgObj(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed)
{LOGMEIN("BackwardPass.cpp] 1609\n");
    Assert(this->tag != Js::BackwardPhase);

    if (this->globOpt->TrackArgumentsObject() && bailOutInfo->capturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->capturedValues.argObjSyms)
        {LOGMEIN("BackwardPass.cpp] 1615\n");
            if (byteCodeUpwardExposedUsed->TestAndClear(symId))
            {LOGMEIN("BackwardPass.cpp] 1617\n");
                if (bailOutInfo->usedCapturedValues.argObjSyms == nullptr)
                {LOGMEIN("BackwardPass.cpp] 1619\n");
                    bailOutInfo->usedCapturedValues.argObjSyms = JitAnew(this->func->m_alloc,
                        BVSparse<JitArenaAllocator>, this->func->m_alloc);
                }
                bailOutInfo->usedCapturedValues.argObjSyms->Set(symId);
            }
        }
        NEXT_BITSET_IN_SPARSEBV;
    }
    if (bailOutInfo->usedCapturedValues.argObjSyms)
    {LOGMEIN("BackwardPass.cpp] 1629\n");
        byteCodeUpwardExposedUsed->Minus(bailOutInfo->usedCapturedValues.argObjSyms);
    }
}

void
BackwardPass::ProcessBailOutConstants(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed, BVSparse<JitArenaAllocator>* bailoutReferencedArgSymsBv)
{LOGMEIN("BackwardPass.cpp] 1636\n");
    Assert(this->tag != Js::BackwardPhase);

    // Remove constants that we are already going to restore
    SListBase<ConstantStackSymValue> * usedConstantValues = &bailOutInfo->usedCapturedValues.constantValues;
    FOREACH_SLISTBASE_ENTRY(ConstantStackSymValue, value, usedConstantValues)
    {LOGMEIN("BackwardPass.cpp] 1642\n");
        byteCodeUpwardExposedUsed->Clear(value.Key()->m_id);
        bailoutReferencedArgSymsBv->Clear(value.Key()->m_id);
    }
    NEXT_SLISTBASE_ENTRY;

    // Find other constants that we need to restore
    FOREACH_SLISTBASE_ENTRY_EDITING(ConstantStackSymValue, value, &bailOutInfo->capturedValues.constantValues, iter)
    {LOGMEIN("BackwardPass.cpp] 1650\n");
        if (byteCodeUpwardExposedUsed->TestAndClear(value.Key()->m_id) || bailoutReferencedArgSymsBv->TestAndClear(value.Key()->m_id))
        {LOGMEIN("BackwardPass.cpp] 1652\n");
            // Constant need to be restore, move it to the restore list
            iter.MoveCurrentTo(usedConstantValues);
        }
        else if (!this->IsPrePass())
        {LOGMEIN("BackwardPass.cpp] 1657\n");
            // Constants don't need to be restored, delete
            iter.RemoveCurrent(this->func->m_alloc);
        }
    }
    NEXT_SLISTBASE_ENTRY_EDITING;
}

void
BackwardPass::ProcessBailOutCopyProps(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed, BVSparse<JitArenaAllocator>* bailoutReferencedArgSymsBv)
{LOGMEIN("BackwardPass.cpp] 1667\n");
    Assert(this->tag != Js::BackwardPhase);
    Assert(!this->func->GetJITFunctionBody()->IsAsmJsMode());

    // Remove copy prop that we were already going to restore
    SListBase<CopyPropSyms> * usedCopyPropSyms = &bailOutInfo->usedCapturedValues.copyPropSyms;
    FOREACH_SLISTBASE_ENTRY(CopyPropSyms, copyPropSyms, usedCopyPropSyms)
    {LOGMEIN("BackwardPass.cpp] 1674\n");
        byteCodeUpwardExposedUsed->Clear(copyPropSyms.Key()->m_id);
        this->currentBlock->upwardExposedUses->Set(copyPropSyms.Value()->m_id);
    }
    NEXT_SLISTBASE_ENTRY;

    JitArenaAllocator * allocator = this->func->m_alloc;
    BasicBlock * block = this->currentBlock;
    BVSparse<JitArenaAllocator> * upwardExposedUses = block->upwardExposedUses;

    // Find other copy prop that we need to restore
    FOREACH_SLISTBASE_ENTRY_EDITING(CopyPropSyms, copyPropSyms, &bailOutInfo->capturedValues.copyPropSyms, iter)
    {LOGMEIN("BackwardPass.cpp] 1686\n");
        // Copy prop syms should be vars
        Assert(!copyPropSyms.Key()->IsTypeSpec());
        Assert(!copyPropSyms.Value()->IsTypeSpec());
        if (byteCodeUpwardExposedUsed->TestAndClear(copyPropSyms.Key()->m_id) || bailoutReferencedArgSymsBv->TestAndClear(copyPropSyms.Key()->m_id))
        {LOGMEIN("BackwardPass.cpp] 1691\n");
            // This copy-prop sym needs to be restored; add it to the restore list.

            /*
            - copyPropSyms.Key() - original sym that is byte-code upwards-exposed, its corresponding byte-code register needs
              to be restored
            - copyPropSyms.Value() - copy-prop sym whose value the original sym has at the point of this instruction

            Heuristic:
            - By default, use the copy-prop sym to restore its corresponding byte code register
            - This is typically better because that allows the value of the original sym, if it's not used after the copy-prop
              sym is changed, to be discarded and we only have one lifetime (the copy-prop sym's lifetime) in to deal with for
              register allocation
            - Additionally, if the transferring store, which caused the original sym to have the same value as the copy-prop
              sym, becomes a dead store, the original sym won't actually attain the value of the copy-prop sym. In that case,
              the copy-prop sym must be used to restore the byte code register corresponding to original sym.

            Special case for functional correctness:
            - Consider that we always use the copy-prop sym to restore, and consider the following case:
                b = a
                a = c * d <Pre-op bail-out>
                  = b
            - This is rewritten by the lowerer as follows:
                b = a
                a = c
                a = a * d <Pre-op bail-out> (to make dst and src1 the same)
                  = b
            - The problem here is that at the point of the bail-out instruction, 'a' would be used to restore the value of 'b',
              but the value of 'a' has changed before the bail-out (at 'a = c').
            - In this case, we need to use 'b' (the original sym) to restore the value of 'b'. Because 'b' is upwards-exposed,
              'b = a' cannot be a dead store, therefore making it valid to use 'b' to restore.
            - Use the original sym to restore when all of the following are true:
                - The bailout is a pre-op bailout, and the bailout check is done after overwriting the destination
                - It's an int-specialized unary or binary operation that produces a value
                - The copy-prop sym is the destination of this instruction
                - None of the sources are the copy-prop sym. Otherwise, the value of the copy-prop sym will be saved as
                  necessary by the bailout code.
            */
            StackSym * stackSym = copyPropSyms.Key(); // assume that we'll use the original sym to restore
            SymID symId = stackSym->m_id;
            IR::Instr *const instr = bailOutInfo->bailOutInstr;
            StackSym *const dstSym = IR::RegOpnd::TryGetStackSym(instr->GetDst());
            if(instr->GetBailOutKind() & IR::BailOutOnResultConditions &&
                instr->GetByteCodeOffset() != Js::Constants::NoByteCodeOffset &&
                bailOutInfo->bailOutOffset <= instr->GetByteCodeOffset() &&
                dstSym &&
                dstSym->IsInt32() &&
                dstSym->IsTypeSpec() &&
                dstSym->GetVarEquivSym(nullptr) == copyPropSyms.Value() &&
                instr->GetSrc1() &&
                !instr->GetDst()->IsEqual(instr->GetSrc1()) &&
                !(instr->GetSrc2() && instr->GetDst()->IsEqual(instr->GetSrc2())))
            {LOGMEIN("BackwardPass.cpp] 1743\n");
                Assert(bailOutInfo->bailOutOffset == instr->GetByteCodeOffset());

                // Need to use the original sym to restore. The original sym is byte-code upwards-exposed, which is why it needs
                // to be restored. Because the original sym needs to be restored and the copy-prop sym is changing here, the
                // original sym must be live in some fashion at the point of this instruction, that will be verified below. The
                // original sym will also be made upwards-exposed from here, so the aforementioned transferring store of the
                // copy-prop sym to the original sym will not be a dead store.
            }
            else if (block->upwardExposedUses->Test(stackSym->m_id) && !block->upwardExposedUses->Test(copyPropSyms.Value()->m_id))
            {LOGMEIN("BackwardPass.cpp] 1753\n");
                // Don't use the copy prop sym if it is not used and the orig sym still has uses.
                // No point in extending the lifetime of the copy prop sym unnecessarily.
            }
            else
            {
                // Need to use the copy-prop sym to restore
                stackSym = copyPropSyms.Value();
                symId = stackSym->m_id;
            }

            // Prefer to restore from type-specialized versions of the sym, as that will reduce the need for potentially
            // expensive ToVars that can more easily be eliminated due to being dead stores
            StackSym * int32StackSym = nullptr;
            StackSym * float64StackSym = nullptr;
            StackSym * simd128StackSym = nullptr;
            if (bailOutInfo->liveLosslessInt32Syms->Test(symId))
            {LOGMEIN("BackwardPass.cpp] 1770\n");
                // Var version of the sym is not live, use the int32 version
                int32StackSym = stackSym->GetInt32EquivSym(nullptr);
                Assert(int32StackSym);
            }
            else if(bailOutInfo->liveFloat64Syms->Test(symId))
            {LOGMEIN("BackwardPass.cpp] 1776\n");
                // Var/int32 version of the sym is not live, use the float64 version
                float64StackSym = stackSym->GetFloat64EquivSym(nullptr);
                Assert(float64StackSym);
            }
            // SIMD_JS
            else if (bailOutInfo->liveSimd128F4Syms->Test(symId))
            {LOGMEIN("BackwardPass.cpp] 1783\n");
                simd128StackSym = stackSym->GetSimd128F4EquivSym(nullptr);
            }
            else if (bailOutInfo->liveSimd128I4Syms->Test(symId))
            {LOGMEIN("BackwardPass.cpp] 1787\n");
                simd128StackSym = stackSym->GetSimd128I4EquivSym(nullptr);
            }
            else
            {
                Assert(bailOutInfo->liveVarSyms->Test(symId));
            }

            // We did not end up using the copy prop sym. Let's make sure the use of the original sym by the bailout is captured.
            if (stackSym != copyPropSyms.Value() && stackSym->HasArgSlotNum())
            {LOGMEIN("BackwardPass.cpp] 1797\n");
                bailoutReferencedArgSymsBv->Set(stackSym->m_id);
            }

            if (int32StackSym != nullptr)
            {LOGMEIN("BackwardPass.cpp] 1802\n");
                Assert(float64StackSym == nullptr);
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), int32StackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(int32StackSym->m_id);
            }
            else if (float64StackSym != nullptr)
            {LOGMEIN("BackwardPass.cpp] 1809\n");
                // This float-specialized sym is going to be used to restore the corresponding byte-code register. Need to
                // ensure that the float value can be precisely coerced back to the original Var value by requiring that it is
                // specialized using BailOutNumberOnly.
                float64StackSym->m_requiresBailOnNotNumber = true;

                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), float64StackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(float64StackSym->m_id);
            }
            // SIMD_JS
            else if (simd128StackSym != nullptr)
            {LOGMEIN("BackwardPass.cpp] 1821\n");
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), simd128StackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(simd128StackSym->m_id);
            }
            else
            {
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), stackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(symId);
            }
        }
        else if (!this->IsPrePass())
        {LOGMEIN("BackwardPass.cpp] 1834\n");
            // Copy prop sym doesn't need to be restored, delete.
            iter.RemoveCurrent(allocator);
        }
    }
    NEXT_SLISTBASE_ENTRY_EDITING;
}

bool
BackwardPass::ProcessBailOutInfo(IR::Instr * instr)
{LOGMEIN("BackwardPass.cpp] 1844\n");
    if (this->tag == Js::BackwardPhase)
    {LOGMEIN("BackwardPass.cpp] 1846\n");
        // We don't need to fill in the bailout instruction in backward pass
        Assert(this->func->hasBailout || !instr->HasBailOutInfo());
        Assert(!instr->HasBailOutInfo() || instr->GetBailOutInfo()->byteCodeUpwardExposedUsed == nullptr || (this->func->HasTry() && this->func->DoOptimizeTryCatch()));

        if (instr->IsByteCodeUsesInstr())
        {LOGMEIN("BackwardPass.cpp] 1852\n");
            // FGPeeps inserts bytecodeuses instrs with srcs.  We need to look at them to set the proper
            // UpwardExposedUsed info and keep the defs alive.
            // The inliner inserts bytecodeuses instrs withs dsts, but we don't want to look at them for upwardExposedUsed
            // as it would cause real defs to look dead.  We use these for bytecodeUpwardExposedUsed info only, which is needed
            // in the dead-store pass only.
            //
            // Handle the source side.
            IR::ByteCodeUsesInstr *byteCodeUsesInstr = instr->AsByteCodeUsesInstr();
            const BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = byteCodeUsesInstr->GetByteCodeUpwardExposedUsed();
            if (byteCodeUpwardExposedUsed != nullptr)
            {LOGMEIN("BackwardPass.cpp] 1863\n");
                this->currentBlock->upwardExposedUses->Or(byteCodeUpwardExposedUsed);
            }
            return true;
        }
        return false;
    }

    if (instr->IsByteCodeUsesInstr())
    {LOGMEIN("BackwardPass.cpp] 1872\n");
        Assert(instr->m_opcode == Js::OpCode::ByteCodeUses);
#if DBG
        if (this->DoMarkTempObjectVerify() && (this->currentBlock->isDead || !this->func->hasBailout))
        {LOGMEIN("BackwardPass.cpp] 1876\n");
            if (IsCollectionPass())
            {LOGMEIN("BackwardPass.cpp] 1878\n");
                if (!this->func->hasBailout)
                {LOGMEIN("BackwardPass.cpp] 1880\n");
                    // Prevent byte code uses from being remove on collection pass for mark temp object verify
                    // if we don't have any bailout
                    return true;
                }
            }
            else
            {
                this->currentBlock->tempObjectVerifyTracker->NotifyDeadByteCodeUses(instr);
            }
        }
#endif

        if (this->func->hasBailout)
        {LOGMEIN("BackwardPass.cpp] 1894\n");
            Assert(this->DoByteCodeUpwardExposedUsed());

            // Just collect the byte code uses, and remove the instruction
            // We are going backward, process the dst first and then the src
            IR::Opnd * dst = instr->GetDst();
            if (dst)
            {LOGMEIN("BackwardPass.cpp] 1901\n");
                IR::RegOpnd * dstRegOpnd = dst->AsRegOpnd();
                StackSym * dstStackSym = dstRegOpnd->m_sym->AsStackSym();
                Assert(!dstRegOpnd->GetIsJITOptimizedReg());
                Assert(dstStackSym->GetByteCodeRegSlot() != Js::Constants::NoRegister);
                if (dstStackSym->GetType() != TyVar)
                {LOGMEIN("BackwardPass.cpp] 1907\n");
                    dstStackSym = dstStackSym->GetVarEquivSym(nullptr);
                }

                // If the current region is a Try, symbols in its write-through set shouldn't be cleared.
                // Otherwise, symbols in the write-through set of the first try ancestor shouldn't be cleared.
                if (!this->currentRegion ||
                    !this->CheckWriteThroughSymInRegion(this->currentRegion, dstStackSym))
                {LOGMEIN("BackwardPass.cpp] 1915\n");
                    this->currentBlock->byteCodeUpwardExposedUsed->Clear(dstStackSym->m_id);
#if DBG
                    // We can only track first level function stack syms right now
                    if (dstStackSym->GetByteCodeFunc() == this->func)
                    {LOGMEIN("BackwardPass.cpp] 1920\n");
                        this->currentBlock->byteCodeRestoreSyms[dstStackSym->GetByteCodeRegSlot()] = nullptr;
                    }
#endif
                }
            }

            IR::ByteCodeUsesInstr *byteCodeUsesInstr = instr->AsByteCodeUsesInstr();
            if (byteCodeUsesInstr->GetByteCodeUpwardExposedUsed() != nullptr)
            {LOGMEIN("BackwardPass.cpp] 1929\n");
                this->currentBlock->byteCodeUpwardExposedUsed->Or(byteCodeUsesInstr->GetByteCodeUpwardExposedUsed());
#if DBG
                FOREACH_BITSET_IN_SPARSEBV(symId, byteCodeUsesInstr->GetByteCodeUpwardExposedUsed())
                {LOGMEIN("BackwardPass.cpp] 1933\n");
                    StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                    Assert(!stackSym->IsTypeSpec());
                    // We can only track first level function stack syms right now
                    if (stackSym->GetByteCodeFunc() == this->func)
                    {LOGMEIN("BackwardPass.cpp] 1938\n");
                        Js::RegSlot byteCodeRegSlot = stackSym->GetByteCodeRegSlot();
                        Assert(byteCodeRegSlot != Js::Constants::NoRegister);
                        if (this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] != stackSym)
                        {LOGMEIN("BackwardPass.cpp] 1942\n");
                            AssertMsg(this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] == nullptr,
                                "Can't have two active lifetime for the same byte code register");
                            this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] = stackSym;
                        }
                    }
                }
                NEXT_BITSET_IN_SPARSEBV;
#endif
            }

            if(IsCollectionPass())
            {LOGMEIN("BackwardPass.cpp] 1954\n");
                return true;
            }

            ProcessPendingPreOpBailOutInfo(instr);

            PropertySym *propertySymUse = byteCodeUsesInstr->propertySymUse;
            if (propertySymUse && !this->currentBlock->isDead)
            {LOGMEIN("BackwardPass.cpp] 1962\n");
                this->currentBlock->upwardExposedFields->Set(propertySymUse->m_id);
            }

            if (this->IsPrePass())
            {LOGMEIN("BackwardPass.cpp] 1967\n");
                // Don't remove the instruction yet if we are in the prepass
                // But tell the caller we don't need to process the instruction any more
                return true;
            }
        }

        this->currentBlock->RemoveInstr(instr);
        return true;
    }

    if(IsCollectionPass())
    {LOGMEIN("BackwardPass.cpp] 1979\n");
        return false;
    }

    if (instr->HasBailOutInfo())
    {LOGMEIN("BackwardPass.cpp] 1984\n");
        Assert(this->func->hasBailout);
        Assert(this->DoByteCodeUpwardExposedUsed());

        BailOutInfo * bailOutInfo = instr->GetBailOutInfo();

        // Only process the bailout info if this is the main bailout point (instead of shared)
        if (bailOutInfo->bailOutInstr == instr)
        {LOGMEIN("BackwardPass.cpp] 1992\n");
            if(instr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset ||
                bailOutInfo->bailOutOffset > instr->GetByteCodeOffset())
            {LOGMEIN("BackwardPass.cpp] 1995\n");
                // Currently, we only have post-op bailout with BailOutOnImplicitCalls
                // or JIT inserted operation (which no byte code offsets).
                // If there are other bailouts that we want to bailout after the operation,
                // we have to make sure that it still doesn't do the implicit call
                // if it is done on the stack object.
                // Otherwise, the stack object will be passed to the implicit call functions.
                Assert(instr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset
                    || (instr->GetBailOutKind() & ~IR::BailOutKindBits) == IR::BailOutOnImplicitCalls
                    || (instr->GetBailOutKind() & ~IR::BailOutKindBits) == IR::BailOutInvalid);

                // This instruction bails out to a later byte-code instruction, so process the bailout info now
                ProcessBailOutInfo(instr, bailOutInfo);
            }
            else
            {
                // This instruction bails out to the equivalent byte code instruction. This instruction and ByteCodeUses
                // instructions relevant to this instruction need to be processed before the bailout info for this instruction
                // can be processed, so that it can be determined what byte code registers are used by the equivalent byte code
                // instruction and need to be restored. Save the instruction for bailout info processing later.
                Assert(bailOutInfo->bailOutOffset == instr->GetByteCodeOffset());
                Assert(!preOpBailOutInstrToProcess);
                preOpBailOutInstrToProcess = instr;
            }
        }
    }

    return false;
}

bool
BackwardPass::IsImplicitCallBailOutCurrentlyNeeded(IR::Instr * instr, bool mayNeedImplicitCallBailOut, bool hasLiveFields)
{LOGMEIN("BackwardPass.cpp] 2027\n");
    return this->globOpt->IsImplicitCallBailOutCurrentlyNeeded(
        instr, nullptr, nullptr, this->currentBlock, hasLiveFields, mayNeedImplicitCallBailOut, false);
}

void
BackwardPass::DeadStoreTypeCheckBailOut(IR::Instr * instr)
{LOGMEIN("BackwardPass.cpp] 2034\n");
    // Good news: There are cases where the forward pass installs BailOutFailedTypeCheck, but the dead store pass
    // discovers that the checked type is dead.
    // Bad news: We may still need implicit call bailout, and it's up to the dead store pass to figure this out.
    // Worse news: BailOutFailedTypeCheck is pre-op, and BailOutOnImplicitCall is post-op. We'll use a special
    // bailout kind to indicate implicit call bailout that targets its own instruction. The lowerer will emit
    // code to disable/re-enable implicit calls around the operation.

    Assert(this->tag == Js::DeadStorePhase);

    if (this->IsPrePass() || !instr->HasBailOutInfo())
    {LOGMEIN("BackwardPass.cpp] 2045\n");
        return;
    }

    IR::BailOutKind oldBailOutKind = instr->GetBailOutKind();
    if (!IR::IsTypeCheckBailOutKind(oldBailOutKind))
    {LOGMEIN("BackwardPass.cpp] 2051\n");
        return;
    }

    // Either src1 or dst must be a property sym operand
    Assert((instr->GetSrc1() && instr->GetSrc1()->IsSymOpnd() && instr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd()) ||
        (instr->GetDst() && instr->GetDst()->IsSymOpnd() && instr->GetDst()->AsSymOpnd()->IsPropertySymOpnd()));

    IR::PropertySymOpnd *propertySymOpnd =
        (instr->GetDst() && instr->GetDst()->IsSymOpnd()) ? instr->GetDst()->AsPropertySymOpnd() : instr->GetSrc1()->AsPropertySymOpnd();

    bool isTypeCheckProtected = false;
    IR::BailOutKind bailOutKind;
    if (GlobOpt::NeedsTypeCheckBailOut(instr, propertySymOpnd, propertySymOpnd == instr->GetDst(), &isTypeCheckProtected, &bailOutKind))
    {LOGMEIN("BackwardPass.cpp] 2065\n");
        // If we installed a failed type check bailout in the forward pass, but we are now discovering that the checked
        // type is dead, we may still need a bailout on failed fixed field type check. These type checks are required
        // regardless of whether the checked type is dead.  Hence, the bailout kind may change here.
        Assert((oldBailOutKind & ~IR::BailOutKindBits) == bailOutKind ||
            bailOutKind == IR::BailOutFailedFixedFieldTypeCheck || bailOutKind == IR::BailOutFailedEquivalentFixedFieldTypeCheck);
        instr->SetBailOutKind(bailOutKind);
        return;
    }
    else if (isTypeCheckProtected)
    {LOGMEIN("BackwardPass.cpp] 2075\n");
        instr->ClearBailOutInfo();
        if (preOpBailOutInstrToProcess == instr)
        {LOGMEIN("BackwardPass.cpp] 2078\n");
            preOpBailOutInstrToProcess = nullptr;
        }
        return;
    }

    Assert(!propertySymOpnd->IsTypeCheckProtected());

    // If all we're doing here is checking the type (e.g. because we've hoisted a field load or store out of the loop, but needed
    // the type check to remain in the loop), and now it turns out we don't need the type checked, we can simply turn this into
    // a NOP and remove the bailout.
    if (instr->m_opcode == Js::OpCode::CheckObjType)
    {LOGMEIN("BackwardPass.cpp] 2090\n");
        Assert(instr->GetDst() == nullptr && instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr);
        instr->m_opcode = Js::OpCode::Nop;
        instr->FreeSrc1();
        instr->ClearBailOutInfo();
        if (this->preOpBailOutInstrToProcess == instr)
        {LOGMEIN("BackwardPass.cpp] 2096\n");
            this->preOpBailOutInstrToProcess = nullptr;
        }
        return;
    }

    // We don't need BailOutFailedTypeCheck but may need BailOutOnImplicitCall.
    // Consider: are we in the loop landing pad? If so, no bailout, since implicit calls will be checked at
    // the end of the block.
    if (this->currentBlock->IsLandingPad())
    {LOGMEIN("BackwardPass.cpp] 2106\n");
        // We're in the landing pad.
        if (preOpBailOutInstrToProcess == instr)
        {LOGMEIN("BackwardPass.cpp] 2109\n");
            preOpBailOutInstrToProcess = nullptr;
        }
        instr->UnlinkBailOutInfo();
        return;
    }

    // If bailOutKind is equivTypeCheck then leave alone the bailout
    if (bailOutKind == IR::BailOutFailedEquivalentTypeCheck ||
        bailOutKind == IR::BailOutFailedEquivalentFixedFieldTypeCheck)
    {LOGMEIN("BackwardPass.cpp] 2119\n");
        return;
    }

    // We're not checking for polymorphism, so don't let the bailout indicate that we
    // detected polymorphism.
    instr->GetBailOutInfo()->polymorphicCacheIndex = (uint)-1;

    // Keep the mark temp object bit if it is there so that we will not remove the implicit call check
    instr->SetBailOutKind(IR::BailOutOnImplicitCallsPreOp | (oldBailOutKind & IR::BailOutMarkTempObject));
}

void
BackwardPass::DeadStoreImplicitCallBailOut(IR::Instr * instr, bool hasLiveFields)
{LOGMEIN("BackwardPass.cpp] 2133\n");
    Assert(this->tag == Js::DeadStorePhase);

    if (this->IsPrePass() || !instr->HasBailOutInfo())
    {LOGMEIN("BackwardPass.cpp] 2137\n");
        // Don't do this in the pre-pass, because, for instance, we don't have live-on-back-edge fields yet.
        return;
    }

    if (OpCodeAttr::BailOutRec(instr->m_opcode))
    {LOGMEIN("BackwardPass.cpp] 2143\n");
        // This is something like OpCode::BailOutOnNotEqual. Assume it needs what it's got.
        return;
    }

    UpdateArrayBailOutKind(instr);

    // Install the implicit call PreOp for mark temp object if we need one.
    IR::BailOutKind kind = instr->GetBailOutKind();
    IR::BailOutKind kindNoBits = kind & ~IR::BailOutKindBits;
    if ((kind & IR::BailOutMarkTempObject) != 0 && kindNoBits != IR::BailOutOnImplicitCallsPreOp)
    {LOGMEIN("BackwardPass.cpp] 2154\n");
        Assert(kindNoBits != IR::BailOutOnImplicitCalls);
        if (kindNoBits == IR::BailOutInvalid)
        {LOGMEIN("BackwardPass.cpp] 2157\n");
            // We should only have combined with array bits
            Assert((kind & ~IR::BailOutForArrayBits) == IR::BailOutMarkTempObject);
            // Don't need to install if we are not going to do helper calls,
            // or we are in the landingPad since implicit calls are already turned off.
            if ((kind & IR::BailOutOnArrayAccessHelperCall) == 0 && !this->currentBlock->IsLandingPad())
            {LOGMEIN("BackwardPass.cpp] 2163\n");
                kind += IR::BailOutOnImplicitCallsPreOp;
                instr->SetBailOutKind(kind);
            }
        }
    }

    // Currently only try to eliminate these bailout kinds. The others are required in cases
    // where we don't necessarily have live/hoisted fields.
    const bool mayNeedBailOnImplicitCall = BailOutInfo::IsBailOutOnImplicitCalls(kind);
    if (!mayNeedBailOnImplicitCall)
    {LOGMEIN("BackwardPass.cpp] 2174\n");
        if (kind & IR::BailOutMarkTempObject)
        {LOGMEIN("BackwardPass.cpp] 2176\n");
            if (kind == IR::BailOutMarkTempObject)
            {LOGMEIN("BackwardPass.cpp] 2178\n");
                // Landing pad does not need per-instr implicit call bailouts.
                Assert(this->currentBlock->IsLandingPad());
                instr->ClearBailOutInfo();
                if (this->preOpBailOutInstrToProcess == instr)
                {LOGMEIN("BackwardPass.cpp] 2183\n");
                    this->preOpBailOutInstrToProcess = nullptr;
                }
            }
            else
            {
                // Mark temp object bit is not needed after dead store pass
                instr->SetBailOutKind(kind & ~IR::BailOutMarkTempObject);
            }
        }
        return;
    }

    // We have an implicit call bailout in the code, and we want to make sure that it's required.
    // Do this now, because only in the dead store pass do we have complete forward and backward liveness info.
    bool needsBailOutOnImplicitCall = this->IsImplicitCallBailOutCurrentlyNeeded(instr, mayNeedBailOnImplicitCall, hasLiveFields);

    if(!UpdateImplicitCallBailOutKind(instr, needsBailOutOnImplicitCall))
    {LOGMEIN("BackwardPass.cpp] 2201\n");
        instr->ClearBailOutInfo();
        if (preOpBailOutInstrToProcess == instr)
        {LOGMEIN("BackwardPass.cpp] 2204\n");
            preOpBailOutInstrToProcess = nullptr;
        }
#if DBG
        if (this->DoMarkTempObjectVerify())
        {LOGMEIN("BackwardPass.cpp] 2209\n");
            this->currentBlock->tempObjectVerifyTracker->NotifyBailOutRemoval(instr, this);
        }
#endif
    }
}

void
BackwardPass::ProcessPendingPreOpBailOutInfo(IR::Instr *const currentInstr)
{LOGMEIN("BackwardPass.cpp] 2218\n");
    Assert(!IsCollectionPass());

    if(!preOpBailOutInstrToProcess)
    {LOGMEIN("BackwardPass.cpp] 2222\n");
        return;
    }

    IR::Instr *const prevInstr = currentInstr->m_prev;
    if(prevInstr &&
        prevInstr->IsByteCodeUsesInstr() &&
        prevInstr->AsByteCodeUsesInstr()->GetByteCodeOffset() == preOpBailOutInstrToProcess->GetByteCodeOffset())
    {LOGMEIN("BackwardPass.cpp] 2230\n");
        return;
    }

    // A pre-op bailout instruction was saved for bailout info processing after the instruction and relevant ByteCodeUses
    // instructions before it have been processed. We can process the bailout info for that instruction now.
    BailOutInfo *const bailOutInfo = preOpBailOutInstrToProcess->GetBailOutInfo();
    Assert(bailOutInfo->bailOutInstr == preOpBailOutInstrToProcess);
    Assert(bailOutInfo->bailOutOffset == preOpBailOutInstrToProcess->GetByteCodeOffset());
    ProcessBailOutInfo(preOpBailOutInstrToProcess, bailOutInfo);
    preOpBailOutInstrToProcess = nullptr;
}

void
BackwardPass::ProcessBailOutInfo(IR::Instr * instr, BailOutInfo * bailOutInfo)
{LOGMEIN("BackwardPass.cpp] 2245\n");
    /*
    When we optimize functions having try-catch, we install a bailout at the starting of the catch block, namely, BailOnException.
    We don't have flow edges from all the possible exception points in the try to the catch block. As a result, this bailout should
    not try to restore from the constant values or copy-prop syms or the type specialized syms, as these may not necessarily be/have
    the right values. For example,

        //constant values
        c =
        try
        {
            <exception>
            c = k (constant)
        }
        catch
        {
            BailOnException
            = c  <-- We need to restore c from the value outside the try.
        }

        //copy-prop syms
        c =
        try
        {
            b = a
            <exception>
            c = b
        }
        catch
        {
            BailOnException
            = c  <-- We really want to restore c from its original sym, and not from its copy-prop sym, a
        }

        //type specialized syms
        a =
        try
        {
            <exception>
            a++  <-- type specializes a
        }
        catch
        {
            BailOnException
            = a  <-- We need to restore a from its var version.
        }
    */
    BasicBlock * block = this->currentBlock;
    BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed = block->byteCodeUpwardExposedUsed;

    Assert(bailOutInfo->bailOutInstr == instr);

    // The byteCodeUpwardExposedUsed should only be assigned once. The only case which would break this
    // assumption is when we are optimizing a function having try-catch. In that case, we need the
    // byteCodeUpwardExposedUsed analysis in the initial backward pass too.
    Assert(bailOutInfo->byteCodeUpwardExposedUsed == nullptr || (this->func->HasTry() && this->func->DoOptimizeTryCatch()));

    // Make a copy of the byteCodeUpwardExposedUsed so we can remove the constants
    if (!this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 2304\n");
        // Create the BV of symbols that need to be restored in the BailOutRecord
        byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed->CopyNew(this->func->m_alloc);
        bailOutInfo->byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed;
    }
    else
    {
        // Create a temporary byteCodeUpwardExposedUsed
        byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed->CopyNew(this->tempAlloc);
    }

    // All the register-based argument syms need to be tracked. They are either:
    //      1. Referenced as constants in bailOutInfo->usedcapturedValues.constantValues
    //      2. Referenced using copy prop syms in bailOutInfo->usedcapturedValues.copyPropSyms
    //      3. Marked as m_isBailOutReferenced = true & added to upwardExposedUsed bit vector to ensure we do not dead store their defs.
    //      The third set of syms is represented by the bailoutReferencedArgSymsBv.
    BVSparse<JitArenaAllocator>* bailoutReferencedArgSymsBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
    if (!this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 2322\n");
        bailOutInfo->IterateArgOutSyms([=](uint, uint, StackSym* sym) {
            if (!sym->IsArgSlotSym())
            {LOGMEIN("BackwardPass.cpp] 2325\n");
                bailoutReferencedArgSymsBv->Set(sym->m_id);
            }
        });
    }

    // Process Argument object first, as they can be found on the stack and don't need to rely on copy prop
    this->ProcessBailOutArgObj(bailOutInfo, byteCodeUpwardExposedUsed);

    if (instr->m_opcode != Js::OpCode::BailOnException) // see comment at the beginning of this function
    {LOGMEIN("BackwardPass.cpp] 2335\n");
        this->ProcessBailOutConstants(bailOutInfo, byteCodeUpwardExposedUsed, bailoutReferencedArgSymsBv);
        this->ProcessBailOutCopyProps(bailOutInfo, byteCodeUpwardExposedUsed, bailoutReferencedArgSymsBv);
    }

    BVSparse<JitArenaAllocator> * tempBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);

    if (bailOutInfo->liveVarSyms)
    {LOGMEIN("BackwardPass.cpp] 2343\n");
        // Prefer to restore from type-specialized versions of the sym, as that will reduce the need for potentially expensive
        // ToVars that can more easily be eliminated due to being dead stores.

#if DBG
        // SIMD_JS
        // Simd128 syms should be live in at most one form
        tempBv->And(bailOutInfo->liveSimd128F4Syms, bailOutInfo->liveSimd128I4Syms);
        Assert(tempBv->IsEmpty());

        // Verify that all syms to restore are live in some fashion
        tempBv->Minus(byteCodeUpwardExposedUsed, bailOutInfo->liveVarSyms);
        tempBv->Minus(bailOutInfo->liveLosslessInt32Syms);
        tempBv->Minus(bailOutInfo->liveFloat64Syms);
        tempBv->Minus(bailOutInfo->liveSimd128F4Syms);
        tempBv->Minus(bailOutInfo->liveSimd128I4Syms);
        Assert(tempBv->IsEmpty());
#endif

        if (this->func->IsJitInDebugMode())
        {LOGMEIN("BackwardPass.cpp] 2363\n");
            // Add to byteCodeUpwardExposedUsed the non-temp local vars used so far to restore during bail out.
            // The ones that are not used so far will get their values from bytecode when we continue after bail out in interpreter.
            Assert(this->func->m_nonTempLocalVars);
            tempBv->And(this->func->m_nonTempLocalVars, bailOutInfo->liveVarSyms);

            // Remove syms that are restored in other ways than byteCodeUpwardExposedUsed.
            FOREACH_SLIST_ENTRY(ConstantStackSymValue, value, &bailOutInfo->usedCapturedValues.constantValues)
            {LOGMEIN("BackwardPass.cpp] 2371\n");
                Assert(value.Key()->HasByteCodeRegSlot() || value.Key()->GetInstrDef()->m_opcode == Js::OpCode::BytecodeArgOutCapture);
                if (value.Key()->HasByteCodeRegSlot())
                {LOGMEIN("BackwardPass.cpp] 2374\n");
                    tempBv->Clear(value.Key()->GetByteCodeRegSlot());
                }
            }
            NEXT_SLIST_ENTRY;
            FOREACH_SLIST_ENTRY(CopyPropSyms, value, &bailOutInfo->usedCapturedValues.copyPropSyms)
            {LOGMEIN("BackwardPass.cpp] 2380\n");
                Assert(value.Key()->HasByteCodeRegSlot() || value.Key()->GetInstrDef()->m_opcode == Js::OpCode::BytecodeArgOutCapture);
                if (value.Key()->HasByteCodeRegSlot())
                {LOGMEIN("BackwardPass.cpp] 2383\n");
                    tempBv->Clear(value.Key()->GetByteCodeRegSlot());
                }
            }
            NEXT_SLIST_ENTRY;
            if (bailOutInfo->usedCapturedValues.argObjSyms)
            {LOGMEIN("BackwardPass.cpp] 2389\n");
                tempBv->Minus(bailOutInfo->usedCapturedValues.argObjSyms);
            }

            byteCodeUpwardExposedUsed->Or(tempBv);
        }

        if (instr->m_opcode != Js::OpCode::BailOnException) // see comment at the beginning of this function
        {LOGMEIN("BackwardPass.cpp] 2397\n");
            // Int32
            tempBv->And(byteCodeUpwardExposedUsed, bailOutInfo->liveLosslessInt32Syms);
            byteCodeUpwardExposedUsed->Minus(tempBv);
            FOREACH_BITSET_IN_SPARSEBV(symId, tempBv)
            {LOGMEIN("BackwardPass.cpp] 2402\n");
                StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                Assert(stackSym->GetType() == TyVar);
                StackSym * int32StackSym = stackSym->GetInt32EquivSym(nullptr);
                Assert(int32StackSym);
                byteCodeUpwardExposedUsed->Set(int32StackSym->m_id);
            }
            NEXT_BITSET_IN_SPARSEBV;

            // Float64
            tempBv->And(byteCodeUpwardExposedUsed, bailOutInfo->liveFloat64Syms);
            byteCodeUpwardExposedUsed->Minus(tempBv);
            FOREACH_BITSET_IN_SPARSEBV(symId, tempBv)
            {LOGMEIN("BackwardPass.cpp] 2415\n");
                StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                Assert(stackSym->GetType() == TyVar);
                StackSym * float64StackSym = stackSym->GetFloat64EquivSym(nullptr);
                Assert(float64StackSym);
                byteCodeUpwardExposedUsed->Set(float64StackSym->m_id);

                // This float-specialized sym is going to be used to restore the corresponding byte-code register. Need to
                // ensure that the float value can be precisely coerced back to the original Var value by requiring that it is
                // specialized using BailOutNumberOnly.
                float64StackSym->m_requiresBailOnNotNumber = true;
            }
            NEXT_BITSET_IN_SPARSEBV;

            // SIMD_JS
            tempBv->Or(bailOutInfo->liveSimd128F4Syms, bailOutInfo->liveSimd128I4Syms);
            tempBv->And(byteCodeUpwardExposedUsed);
            byteCodeUpwardExposedUsed->Minus(tempBv);
            FOREACH_BITSET_IN_SPARSEBV(symId, tempBv)
            {LOGMEIN("BackwardPass.cpp] 2434\n");
                StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                Assert(stackSym->GetType() == TyVar);
                StackSym * simd128Sym = nullptr;
                if (bailOutInfo->liveSimd128F4Syms->Test(symId))
                {LOGMEIN("BackwardPass.cpp] 2439\n");
                    simd128Sym = stackSym->GetSimd128F4EquivSym(nullptr);
                }
                else
                {
                    Assert(bailOutInfo->liveSimd128I4Syms->Test(symId));
                    simd128Sym = stackSym->GetSimd128I4EquivSym(nullptr);
                }
                byteCodeUpwardExposedUsed->Set(simd128Sym->m_id);
            }
            NEXT_BITSET_IN_SPARSEBV;
        }
        // Var
        // Any remaining syms to restore will be restored from their var versions
    }
    else
    {
        Assert(!this->func->DoGlobOpt());
    }

    JitAdelete(this->tempAlloc, tempBv);

    // BailOnNoProfile makes some edges dead. Upward exposed symbols info set after the BailOnProfile won't
    // flow through these edges, and, in turn, not through predecessor edges of the block containing the
    // BailOnNoProfile. This is specifically bad for an inlinee's argout syms as they are set as upward exposed
    // when we see the InlineeEnd, but may not look so to some blocks and may get overwritten.
    // Set the argout syms as upward exposed here.
    if (instr->m_opcode == Js::OpCode::BailOnNoProfile && instr->m_func->IsInlinee() &&
        instr->m_func->m_hasInlineArgsOpt && instr->m_func->frameInfo->isRecorded)
    {LOGMEIN("BackwardPass.cpp] 2468\n");
        instr->m_func->frameInfo->IterateSyms([=](StackSym* argSym)
        {
            this->currentBlock->upwardExposedUses->Set(argSym->m_id);
        });
    }

    // Mark all the register that we need to restore as used (excluding constants)
    block->upwardExposedUses->Or(byteCodeUpwardExposedUsed);
    block->upwardExposedUses->Or(bailoutReferencedArgSymsBv);

    if (!this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 2480\n");
        bailOutInfo->IterateArgOutSyms([=](uint index, uint, StackSym* sym) {
            if (sym->IsArgSlotSym() || bailoutReferencedArgSymsBv->Test(sym->m_id))
            {LOGMEIN("BackwardPass.cpp] 2483\n");
                bailOutInfo->argOutSyms[index]->m_isBailOutReferenced = true;
            }
        });
    }
    JitAdelete(this->tempAlloc, bailoutReferencedArgSymsBv);

    if (this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 2491\n");
        JitAdelete(this->tempAlloc, byteCodeUpwardExposedUsed);
    }
}

void
BackwardPass::ProcessBlock(BasicBlock * block)
{LOGMEIN("BackwardPass.cpp] 2498\n");
    this->currentBlock = block;
    this->MergeSuccBlocksInfo(block);
#if DBG_DUMP
    if (this->IsTraceEnabled())
    {LOGMEIN("BackwardPass.cpp] 2503\n");
        Output::Print(_u("******************************* Before Process Block *******************************n"));
        DumpBlockData(block);
    }
#endif
    FOREACH_INSTR_BACKWARD_IN_BLOCK_EDITING(instr, instrPrev, block)
    {LOGMEIN("BackwardPass.cpp] 2509\n");
#if DBG_DUMP
        if (!IsCollectionPass() && IsTraceEnabled() && Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("BackwardPass.cpp] 2512\n");
            Output::Print(_u(">>>>>>>>>>>>>>>>>>>>>> %s: Instr Start\n"), tag == Js::BackwardPhase? _u("BACKWARD") : _u("DEADSTORE"));
            instr->Dump();
            if (block->upwardExposedUses)
            {LOGMEIN("BackwardPass.cpp] 2516\n");
                Output::SkipToColumn(10);
                Output::Print(_u("   Exposed Use: "));
                block->upwardExposedUses->Dump();
            }
            if (block->upwardExposedFields)
            {LOGMEIN("BackwardPass.cpp] 2522\n");
                Output::SkipToColumn(10);
                Output::Print(_u("Exposed Fields: "));
                block->upwardExposedFields->Dump();
            }
            if (block->byteCodeUpwardExposedUsed)
            {LOGMEIN("BackwardPass.cpp] 2528\n");
                Output::SkipToColumn(10);
                Output::Print(_u(" Byte Code Use: "));
                block->byteCodeUpwardExposedUsed->Dump();
            }
            Output::Print(_u("--------------------\n"));
        }
#endif

        this->currentInstr = instr;
        this->currentRegion = this->currentBlock->GetFirstInstr()->AsLabelInstr()->GetRegion();
        
        IR::Instr * insertedInstr = TryChangeInstrForStackArgOpt();
        if (insertedInstr != nullptr)
        {LOGMEIN("BackwardPass.cpp] 2542\n");
            instrPrev = insertedInstr;
            continue;
        }

        MarkScopeObjSymUseForStackArgOpt();
        ProcessBailOnStackArgsOutOfActualsRange();
        
        if (ProcessNoImplicitCallUses(instr) || this->ProcessBailOutInfo(instr))
        {LOGMEIN("BackwardPass.cpp] 2551\n");
            continue;
        }

        IR::Instr *instrNext = instr->m_next;
        if (this->TrackNoImplicitCallInlinees(instr))
        {LOGMEIN("BackwardPass.cpp] 2557\n");
            instrPrev = instrNext->m_prev;
            continue;
        }

        if (CanDeadStoreInstrForScopeObjRemoval() && DeadStoreOrChangeInstrForScopeObjRemoval(&instrPrev))
        {LOGMEIN("BackwardPass.cpp] 2563\n");
            continue;
        }

        bool hasLiveFields = (block->upwardExposedFields && !block->upwardExposedFields->IsEmpty());

        IR::Opnd * opnd = instr->GetDst();
        if (opnd != nullptr)
        {LOGMEIN("BackwardPass.cpp] 2571\n");
            bool isRemoved = ReverseCopyProp(instr);
            if (isRemoved)
            {LOGMEIN("BackwardPass.cpp] 2574\n");
                instrPrev = instrNext->m_prev;
                continue;
            }
            if (instr->m_opcode == Js::OpCode::Conv_Bool)
            {LOGMEIN("BackwardPass.cpp] 2579\n");
                isRemoved = this->FoldCmBool(instr);
                if (isRemoved)
                {LOGMEIN("BackwardPass.cpp] 2582\n");
                    continue;
                }
            }

            ProcessNewScObject(instr);

            this->ProcessTransfers(instr);

            isRemoved = this->ProcessDef(opnd);
            if (isRemoved)
            {LOGMEIN("BackwardPass.cpp] 2593\n");
                continue;
            }
        }

        if(!IsCollectionPass())
        {LOGMEIN("BackwardPass.cpp] 2599\n");
            this->MarkTempProcessInstr(instr);
            this->ProcessFieldKills(instr);

            if (this->DoDeadStoreSlots()
                && (instr->HasAnyImplicitCalls() || instr->HasBailOutInfo() || instr->UsesAllFields()))
            {LOGMEIN("BackwardPass.cpp] 2605\n");
                // Can't dead-store slots if there can be an implicit-call, an exception, or a bailout
                block->slotDeadStoreCandidates->ClearAll();
            }

            if (this->DoFieldHoistCandidates())
            {LOGMEIN("BackwardPass.cpp] 2611\n");
                this->ProcessFieldHoistKills(instr);
            }

            TrackIntUsage(instr);
            TrackBitWiseOrNumberOp(instr);

            TrackFloatSymEquivalence(instr);
        }

        opnd = instr->GetSrc1();
        if (opnd != nullptr)
        {LOGMEIN("BackwardPass.cpp] 2623\n");
            this->ProcessUse(opnd);

            opnd = instr->GetSrc2();
            if (opnd != nullptr)
            {LOGMEIN("BackwardPass.cpp] 2628\n");
                this->ProcessUse(opnd);
            }
        }

        if(IsCollectionPass())
        {LOGMEIN("BackwardPass.cpp] 2634\n");
            continue;
        }

        if (this->tag == Js::DeadStorePhase)
        {LOGMEIN("BackwardPass.cpp] 2639\n");
            switch(instr->m_opcode)
            {LOGMEIN("BackwardPass.cpp] 2641\n");
                case Js::OpCode::LdSlot:
                {LOGMEIN("BackwardPass.cpp] 2643\n");
                    DeadStoreOrChangeInstrForScopeObjRemoval(&instrPrev);
                    break;
                }
                case Js::OpCode::InlineArrayPush:
                case Js::OpCode::InlineArrayPop:
                {LOGMEIN("BackwardPass.cpp] 2649\n");
                    IR::Opnd *const thisOpnd = instr->GetSrc1();
                    if(thisOpnd && thisOpnd->IsRegOpnd())
                    {LOGMEIN("BackwardPass.cpp] 2652\n");
                        IR::RegOpnd *const thisRegOpnd = thisOpnd->AsRegOpnd();
                        if(thisRegOpnd->IsArrayRegOpnd())
                        {
                            // Process the array use at the point of the array built-in call, since the array will actually
                            // be used at the call, not at the ArgOut_A_InlineBuiltIn
                            ProcessArrayRegOpndUse(instr, thisRegOpnd->AsArrayRegOpnd());
                        }
                    }
                }

            #if !INT32VAR // the following is not valid on 64-bit platforms
                case Js::OpCode::BoundCheck:
                {LOGMEIN("BackwardPass.cpp] 2665\n");
                    if(IsPrePass())
                    {LOGMEIN("BackwardPass.cpp] 2667\n");
                        break;
                    }

                    // Look for:
                    //     BoundCheck 0 <= s1
                    //     BoundCheck s1 <= s2 + c, where c == 0 || c == -1
                    //
                    // And change it to:
                    //     UnsignedBoundCheck s1 <= s2 + c
                    //
                    // The BoundCheck instruction is a signed operation, so any unsigned operand used in the instruction must be
                    // guaranteed to be >= 0 and <= int32 max when its value is interpreted as signed. Due to the restricted
                    // range of s2 above, by using an unsigned comparison instead, the negative check on s1 will also be
                    // covered.
                    //
                    // A BoundCheck instruction takes the form (src1 <= src2 + dst).

                    // Check the current instruction's pattern for:
                    //     BoundCheck s1 <= s2 + c, where c <= 0
                    if(!instr->GetSrc1()->IsRegOpnd() ||
                        !instr->GetSrc1()->IsInt32() ||
                        !instr->GetSrc2() ||
                        instr->GetSrc2()->IsIntConstOpnd())
                    {LOGMEIN("BackwardPass.cpp] 2691\n");
                        break;
                    }
                    if(instr->GetDst())
                    {LOGMEIN("BackwardPass.cpp] 2695\n");
                        const int c = instr->GetDst()->AsIntConstOpnd()->GetValue();
                        if(c != 0 && c != -1)
                        {LOGMEIN("BackwardPass.cpp] 2698\n");
                            break;
                        }
                    }

                    // Check the previous instruction's pattern for:
                    //     BoundCheck 0 <= s1
                    IR::Instr *const lowerBoundCheck = instr->m_prev;
                    if(lowerBoundCheck->m_opcode != Js::OpCode::BoundCheck ||
                        !lowerBoundCheck->GetSrc1()->IsIntConstOpnd() ||
                        lowerBoundCheck->GetSrc1()->AsIntConstOpnd()->GetValue() != 0 ||
                        !lowerBoundCheck->GetSrc2() ||
                        !instr->GetSrc1()->AsRegOpnd()->IsEqual(lowerBoundCheck->GetSrc2()) ||
                        lowerBoundCheck->GetDst() && lowerBoundCheck->GetDst()->AsIntConstOpnd()->GetValue() != 0)
                    {LOGMEIN("BackwardPass.cpp] 2712\n");
                        break;
                    }

                    // Remove the previous lower bound check, and change the current upper bound check to:
                    //     UnsignedBoundCheck s1 <= s2 + c
                    instr->m_opcode = Js::OpCode::UnsignedBoundCheck;
                    currentBlock->RemoveInstr(lowerBoundCheck);
                    instrPrev = instr->m_prev;
                    break;
                }
            #endif
            }

            DeadStoreTypeCheckBailOut(instr);
            DeadStoreImplicitCallBailOut(instr, hasLiveFields);

            if (block->stackSymToFinalType != nullptr)
            {LOGMEIN("BackwardPass.cpp] 2730\n");
                this->InsertTypeTransitionsAtPotentialKills();
            }

            // NoImplicitCallUses transfers need to be processed after determining whether implicit calls need to be disabled
            // for the current instruction, because the instruction where the def occurs also needs implicit calls disabled.
            // Array value type for the destination needs to be updated before transfers have been processed by
            // ProcessNoImplicitCallDef, and array value types for sources need to be updated after transfers have been
            // processed by ProcessNoImplicitCallDef, as it requires the no-implicit-call tracking bit-vectors to be precise at
            // the point of the update.
            if(!IsPrePass())
            {
                UpdateArrayValueTypes(instr, instr->GetDst());
            }
            ProcessNoImplicitCallDef(instr);
            if(!IsPrePass())
            {
                UpdateArrayValueTypes(instr, instr->GetSrc1());
                UpdateArrayValueTypes(instr, instr->GetSrc2());
            }
        }
        else
        {
            switch (instr->m_opcode)
            {LOGMEIN("BackwardPass.cpp] 2754\n");
                case Js::OpCode::BailOnNoProfile:
                {LOGMEIN("BackwardPass.cpp] 2756\n");
                    this->ProcessBailOnNoProfile(instr, block);
                    // this call could change the last instr of the previous block...  Adjust instrStop.
                    instrStop = block->GetFirstInstr()->m_prev;
                    Assert(this->tag != Js::DeadStorePhase);
                    continue;
                }
                case Js::OpCode::Catch:
                {LOGMEIN("BackwardPass.cpp] 2764\n");
                    if (this->func->DoOptimizeTryCatch() && !this->IsPrePass())
                    {LOGMEIN("BackwardPass.cpp] 2766\n");
                        // Execute the "Catch" in the JIT'ed code, and bailout to the next instruction. This way, the bailout will restore the exception object automatically.
                        IR::BailOutInstr* bailOnException = IR::BailOutInstr::New(Js::OpCode::BailOnException, IR::BailOutOnException, instr->m_next, instr->m_func);
                        instr->InsertAfter(bailOnException);

                        Assert(instr->GetDst()->IsRegOpnd() && instr->GetDst()->GetStackSym()->HasByteCodeRegSlot());
                        StackSym * exceptionObjSym = instr->GetDst()->GetStackSym();

                        Assert(instr->m_prev->IsLabelInstr() && (instr->m_prev->AsLabelInstr()->GetRegion()->GetType() == RegionTypeCatch));
                        instr->m_prev->AsLabelInstr()->GetRegion()->SetExceptionObjectSym(exceptionObjSym);
                    }
                    break;
                }
                case Js::OpCode::Throw:
                case Js::OpCode::EHThrow:
                case Js::OpCode::InlineThrow:
                    this->func->SetHasThrow();
                    break;
            }
        }

        if (instr->m_opcode == Js::OpCode::InlineeEnd)
        {LOGMEIN("BackwardPass.cpp] 2788\n");
            this->ProcessInlineeEnd(instr);
        }

        if (instr->IsLabelInstr() && instr->m_next->m_opcode == Js::OpCode::Catch)
        {LOGMEIN("BackwardPass.cpp] 2793\n");
            if (!this->currentRegion)
            {LOGMEIN("BackwardPass.cpp] 2795\n");
                Assert(!this->func->DoOptimizeTryCatch() && !(this->func->IsSimpleJit() && this->func->hasBailout));
            }
            else
            {
                Assert(this->currentRegion->GetType() == RegionTypeCatch);
                Region * matchingTryRegion = this->currentRegion->GetMatchingTryRegion();
                Assert(matchingTryRegion);

                // We need live-on-back-edge info to accurately set write-through symbols for try-catches in a loop.
                // Don't set write-through symbols in pre-pass
                if (!this->IsPrePass() && !matchingTryRegion->writeThroughSymbolsSet)
                {LOGMEIN("BackwardPass.cpp] 2807\n");
                    if (this->tag == Js::DeadStorePhase)
                    {LOGMEIN("BackwardPass.cpp] 2809\n");
                        Assert(!this->func->DoGlobOpt());
                    }
                    // FullJit: Write-through symbols info must be populated in the backward pass as
                    //      1. the forward pass needs it to insert ToVars.
                    //      2. the deadstore pass needs it to not clear such symbols from the
                    //         byteCodeUpwardExposedUsed BV upon a def in the try region. This is required
                    //         because any bailout in the try region needs to restore all write-through
                    //         symbols.
                    // SimpleJit: Won't run the initial backward pass, but write-through symbols info is still
                    //      needed in the deadstore pass for <2> above.
                    this->SetWriteThroughSymbolsSetForRegion(this->currentBlock, matchingTryRegion);
                }
            }
        }
#if DBG
        if (instr->m_opcode == Js::OpCode::TryCatch)
        {LOGMEIN("BackwardPass.cpp] 2826\n");
            if (!this->IsPrePass() && (this->func->DoOptimizeTryCatch() || (this->func->IsSimpleJit() && this->func->hasBailout)))
            {LOGMEIN("BackwardPass.cpp] 2828\n");
                Assert(instr->m_next->IsLabelInstr() && (instr->m_next->AsLabelInstr()->GetRegion() != nullptr));
                Region * tryRegion = instr->m_next->AsLabelInstr()->GetRegion();
                Assert(tryRegion->writeThroughSymbolsSet);
            }
        }
#endif
        ProcessPendingPreOpBailOutInfo(instr);

#if DBG_DUMP
        if (!IsCollectionPass() && IsTraceEnabled() && Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("BackwardPass.cpp] 2839\n");
            Output::Print(_u("-------------------\n"));
            instr->Dump();
            if (block->upwardExposedUses)
            {LOGMEIN("BackwardPass.cpp] 2843\n");
                Output::SkipToColumn(10);
                Output::Print(_u("   Exposed Use: "));
                block->upwardExposedUses->Dump();
            }
            if (block->upwardExposedFields)
            {LOGMEIN("BackwardPass.cpp] 2849\n");
                Output::SkipToColumn(10);
                Output::Print(_u("Exposed Fields: "));
                block->upwardExposedFields->Dump();
            }
            if (block->byteCodeUpwardExposedUsed)
            {LOGMEIN("BackwardPass.cpp] 2855\n");
                Output::SkipToColumn(10);
                Output::Print(_u(" Byte Code Use: "));
                block->byteCodeUpwardExposedUsed->Dump();
            }
            Output::Print(_u("<<<<<<<<<<<<<<<<<<<<<< %s: Instr End\n"), tag == Js::BackwardPhase? _u("BACKWARD") : _u("DEADSTORE"));
        }
#endif
    }
    NEXT_INSTR_BACKWARD_IN_BLOCK_EDITING;

    EndIntOverflowDoesNotMatterRange();

    if (this->DoFieldHoistCandidates() && !block->isDead && block->isLoopHeader)
    {LOGMEIN("BackwardPass.cpp] 2869\n");
        Assert(block->loop->fieldHoistCandidates == nullptr);
        block->loop->fieldHoistCandidates = block->fieldHoistCandidates->CopyNew(this->func->m_alloc);
    }

    if (!this->IsPrePass() && !block->isDead && block->isLoopHeader)
    {LOGMEIN("BackwardPass.cpp] 2875\n");
        // Copy the upward exposed use as the live on back edge regs
        block->loop->regAlloc.liveOnBackEdgeSyms = block->upwardExposedUses->CopyNew(this->func->m_alloc);
    }

    Assert(!considerSymAsRealUseInNoImplicitCallUses);

#if DBG_DUMP
    if (this->IsTraceEnabled())
    {LOGMEIN("BackwardPass.cpp] 2884\n");
        Output::Print(_u("******************************* After Process Block *******************************n"));
        DumpBlockData(block);
    }
#endif
}

bool 
BackwardPass::CanDeadStoreInstrForScopeObjRemoval(Sym *sym) const
{LOGMEIN("BackwardPass.cpp] 2893\n");
    if (tag == Js::DeadStorePhase && this->currentInstr->m_func->IsStackArgsEnabled())
    {LOGMEIN("BackwardPass.cpp] 2895\n");
        Func * currFunc = this->currentInstr->m_func;
        bool doScopeObjCreation = currFunc->GetJITFunctionBody()->GetDoScopeObjectCreation();
        switch (this->currentInstr->m_opcode)
        {LOGMEIN("BackwardPass.cpp] 2899\n");
            case Js::OpCode::InitCachedScope:
            {LOGMEIN("BackwardPass.cpp] 2901\n");
                if(!doScopeObjCreation && this->currentInstr->GetDst()->IsScopeObjOpnd(currFunc))
                {LOGMEIN("BackwardPass.cpp] 2903\n");
                    /*
                    *   We don't really dead store this instruction. We just want the source sym of this instruction
                    *   to NOT be tracked as USED by this instruction.
                    *   This instr will effectively be lowered to dest = MOV NULLObject, in the lowerer phase.
                    */
                    return true;
                }
                break;
            }
            case Js::OpCode::LdSlot:
            {LOGMEIN("BackwardPass.cpp] 2914\n");
                if (sym && IsFormalParamSym(currFunc, sym))
                {LOGMEIN("BackwardPass.cpp] 2916\n");
                    return true;
                }
                break;
            }
            case Js::OpCode::CommitScope:
            case Js::OpCode::GetCachedFunc:
            {LOGMEIN("BackwardPass.cpp] 2923\n");
                return !doScopeObjCreation && this->currentInstr->GetSrc1()->IsScopeObjOpnd(currFunc);
            }
            case Js::OpCode::BrFncCachedScopeEq:
            case Js::OpCode::BrFncCachedScopeNeq:
            {LOGMEIN("BackwardPass.cpp] 2928\n");
                return !doScopeObjCreation && this->currentInstr->GetSrc2()->IsScopeObjOpnd(currFunc);
            }
            case Js::OpCode::CallHelper:
            {LOGMEIN("BackwardPass.cpp] 2932\n");
                if (!doScopeObjCreation && this->currentInstr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper == IR::JnHelperMethod::HelperOP_InitCachedFuncs)
                {LOGMEIN("BackwardPass.cpp] 2934\n");
                    IR::RegOpnd * scopeObjOpnd = this->currentInstr->GetSrc2()->GetStackSym()->GetInstrDef()->GetSrc1()->AsRegOpnd();
                    return scopeObjOpnd->IsScopeObjOpnd(currFunc);
                }
                break;
            }
        }
    }
    return false;
}

/*
* This is for Eliminating Scope Object Creation during Heap arguments optimization.
*/
bool
BackwardPass::DeadStoreOrChangeInstrForScopeObjRemoval(IR::Instr ** pInstrPrev)
{LOGMEIN("BackwardPass.cpp] 2950\n");
    IR::Instr * instr = this->currentInstr;
    Func * currFunc = instr->m_func;

    if (this->tag == Js::DeadStorePhase && instr->m_func->IsStackArgsEnabled() && !IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 2955\n");
        switch (instr->m_opcode)
        {LOGMEIN("BackwardPass.cpp] 2957\n");
            /*
            *   This LdSlot loads the formal from the formals array. We replace this a Ld_A <ArgInSym>.
            *   ArgInSym is inserted at the beginning of the function during the start of the deadstore pass- for the top func.
            *   In case of inlinee, it will be from the source sym of the ArgOut Instruction to the inlinee.
            */
            case Js::OpCode::LdSlot:
            {LOGMEIN("BackwardPass.cpp] 2964\n");
                IR::Opnd * src1 = instr->GetSrc1();
                if (src1 && src1->IsSymOpnd())
                {LOGMEIN("BackwardPass.cpp] 2967\n");
                    Sym * sym = src1->AsSymOpnd()->m_sym;
                    Assert(sym);
                    if (IsFormalParamSym(currFunc, sym))
                    {LOGMEIN("BackwardPass.cpp] 2971\n");
                        AssertMsg(!currFunc->GetJITFunctionBody()->HasImplicitArgIns(), "We don't have mappings between named formals and arguments object here");

                        instr->m_opcode = Js::OpCode::Ld_A;
                        PropertySym * propSym = sym->AsPropertySym();
                        Js::ArgSlot    value = (Js::ArgSlot)propSym->m_propertyId;

                        Assert(currFunc->HasStackSymForFormal(value));
                        StackSym * paramStackSym = currFunc->GetStackSymForFormal(value);
                        IR::RegOpnd * srcOpnd = IR::RegOpnd::New(paramStackSym, TyVar, currFunc);
                        instr->ReplaceSrc1(srcOpnd);
                        this->ProcessSymUse(paramStackSym, true, true);

                        if (PHASE_VERBOSE_TRACE1(Js::StackArgFormalsOptPhase))
                        {LOGMEIN("BackwardPass.cpp] 2985\n");
                            Output::Print(_u("StackArgFormals : %s (%d) :Replacing LdSlot with Ld_A in Deadstore pass. \n"), instr->m_func->GetJITFunctionBody()->GetDisplayName(), instr->m_func->GetFunctionNumber());
                            Output::Flush();
                        }
                    }
                }
                break;
            }
            case Js::OpCode::CommitScope:
            {LOGMEIN("BackwardPass.cpp] 2994\n");
                if (instr->GetSrc1()->IsScopeObjOpnd(currFunc))
                {LOGMEIN("BackwardPass.cpp] 2996\n");
                    instr->Remove();
                    return true;
                }
                break;
            }
            case Js::OpCode::BrFncCachedScopeEq:
            case Js::OpCode::BrFncCachedScopeNeq:
            {LOGMEIN("BackwardPass.cpp] 3004\n");
                if (instr->GetSrc2()->IsScopeObjOpnd(currFunc))
                {LOGMEIN("BackwardPass.cpp] 3006\n");
                    instr->Remove();
                    return true;
                }
                break;
            }
            case Js::OpCode::CallHelper:
            {LOGMEIN("BackwardPass.cpp] 3013\n");
                //Remove the CALL and all its Argout instrs.
                if (instr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper == IR::JnHelperMethod::HelperOP_InitCachedFuncs)
                {LOGMEIN("BackwardPass.cpp] 3016\n");
                    IR::RegOpnd * scopeObjOpnd = instr->GetSrc2()->GetStackSym()->GetInstrDef()->GetSrc1()->AsRegOpnd();
                    if (scopeObjOpnd->IsScopeObjOpnd(currFunc))
                    {LOGMEIN("BackwardPass.cpp] 3019\n");
                        IR::Instr * instrDef = instr;
                        IR::Instr * nextInstr = instr->m_next;

                        while (instrDef != nullptr)
                        {LOGMEIN("BackwardPass.cpp] 3024\n");
                            IR::Instr * instrToDelete = instrDef;
                            if (instrDef->GetSrc2() != nullptr)
                            {LOGMEIN("BackwardPass.cpp] 3027\n");
                                instrDef = instrDef->GetSrc2()->GetStackSym()->GetInstrDef();
                                Assert(instrDef->m_opcode == Js::OpCode::ArgOut_A);
                            }
                            else
                            {
                                instrDef = nullptr;
                            }
                            instrToDelete->Remove();
                        }
                        Assert(nextInstr != nullptr);
                        *pInstrPrev = nextInstr->m_prev;
                        return true;
                    }
                }
                break;
            }
            case Js::OpCode::GetCachedFunc:
            {LOGMEIN("BackwardPass.cpp] 3045\n");
                // <dst> = GetCachedFunc <scopeObject>, <functionNum>
                // is converted to 
                // <dst> = NewScFunc <functionNum>, <env: FrameDisplay>

                if (instr->GetSrc1()->IsScopeObjOpnd(currFunc))
                {LOGMEIN("BackwardPass.cpp] 3051\n");
                    instr->m_opcode = Js::OpCode::NewScFunc;
                    IR::Opnd * intConstOpnd = instr->UnlinkSrc2();

                    instr->ReplaceSrc1(intConstOpnd);
                    instr->SetSrc2(IR::RegOpnd::New(currFunc->GetLocalFrameDisplaySym(), IRType::TyVar, currFunc));
                }
                break;
            }
        }
    }
    return false;
}

IR::Instr *
BackwardPass::TryChangeInstrForStackArgOpt()
{LOGMEIN("BackwardPass.cpp] 3067\n");
    IR::Instr * instr = this->currentInstr;
    if (tag == Js::DeadStorePhase && instr->DoStackArgsOpt(this->func))
    {LOGMEIN("BackwardPass.cpp] 3070\n");
        switch (instr->m_opcode)
        {LOGMEIN("BackwardPass.cpp] 3072\n");
            case Js::OpCode::TypeofElem:
            {LOGMEIN("BackwardPass.cpp] 3074\n");
                /*
                    Before:
                        dst = TypeOfElem arguments[i] <(BailOnStackArgsOutOfActualsRange)>

                    After:
                        tmpdst = LdElemI_A arguments[i] <(BailOnStackArgsOutOfActualsRange)>
                        dst = TypeOf tmpdst
                */

                AssertMsg(instr->HasBailOutInfo() && (instr->GetBailOutKind() & IR::BailOutKind::BailOnStackArgsOutOfActualsRange), "Why is the bailout kind not set, when it is StackArgOptimized?");

                instr->m_opcode = Js::OpCode::LdElemI_A;
                IR::Opnd * dstOpnd = instr->UnlinkDst();

                IR::RegOpnd * elementOpnd = IR::RegOpnd::New(StackSym::New(instr->m_func), IRType::TyVar, instr->m_func);
                instr->SetDst(elementOpnd);

                IR::Instr * typeOfInstr = IR::Instr::New(Js::OpCode::Typeof, dstOpnd, elementOpnd, instr->m_func);
                instr->InsertAfter(typeOfInstr);

                return typeOfInstr;
            }
        }
    }

    /*
    *   Scope Object Sym is kept alive in all code paths.
    *   -This is to facilitate Bailout to record the live Scope object Sym, whenever required.
    *   -Reason for doing is this because - Scope object has to be implicitly live whenever Heap Arguments object is live.
    *   -When we restore HeapArguments object in the bail out path, it expects the scope object also to be restored - if one was created.
    *   -We do not know detailed information about Heap arguments obj syms(aliasing etc.) until we complete Forward Pass. 
    *   -And we want to avoid dead sym clean up (in this case, scope object though not explicitly live, it is live implicitly) during Block merging in the forward pass. 
    *   -Hence this is the optimal spot to do this.
    */

    if (tag == Js::BackwardPhase && instr->m_func->GetScopeObjSym() != nullptr)
    {LOGMEIN("BackwardPass.cpp] 3111\n");
        this->currentBlock->upwardExposedUses->Set(instr->m_func->GetScopeObjSym()->m_id);
    }

    return nullptr;
}

void
BackwardPass::TraceDeadStoreOfInstrsForScopeObjectRemoval()
{LOGMEIN("BackwardPass.cpp] 3120\n");
    IR::Instr * instr = this->currentInstr;

    if (instr->m_func->IsStackArgsEnabled())
    {LOGMEIN("BackwardPass.cpp] 3124\n");
        if ((instr->m_opcode == Js::OpCode::InitCachedScope || instr->m_opcode == Js::OpCode::NewScopeObject) && !IsPrePass())
        {LOGMEIN("BackwardPass.cpp] 3126\n");
            if (PHASE_TRACE1(Js::StackArgFormalsOptPhase))
            {LOGMEIN("BackwardPass.cpp] 3128\n");
                Output::Print(_u("StackArgFormals : %s (%d) :Removing Scope object creation in Deadstore pass. \n"), instr->m_func->GetJITFunctionBody()->GetDisplayName(), instr->m_func->GetFunctionNumber());
                Output::Flush();
            }
        }
    }
}

bool
BackwardPass::IsFormalParamSym(Func * func, Sym * sym) const
{LOGMEIN("BackwardPass.cpp] 3138\n");
    Assert(sym);
    
    if (sym->IsPropertySym())
    {LOGMEIN("BackwardPass.cpp] 3142\n");
        //If the sym is a propertySym, then see if the propertyId is within the range of the formals 
        //We can have other properties stored in the scope object other than the formals (following the formals).
        PropertySym * propSym = sym->AsPropertySym();
        IntConstType    value = propSym->m_propertyId;
        return func->IsFormalsArraySym(propSym->m_stackSym->m_id) &&
            (value >= 0 && value < func->GetJITFunctionBody()->GetInParamsCount() - 1);
    }
    else
    {
        Assert(sym->IsStackSym());
        return !!func->IsFormalsArraySym(sym->AsStackSym()->m_id);
    }
}

#if DBG_DUMP
void
BackwardPass::DumpBlockData(BasicBlock * block)
{LOGMEIN("BackwardPass.cpp] 3160\n");
    block->DumpHeader();
    if (block->upwardExposedUses) // may be null for dead blocks
    {LOGMEIN("BackwardPass.cpp] 3163\n");
        Output::Print(_u("             Exposed Uses: "));
        block->upwardExposedUses->Dump();
    }

    if (block->typesNeedingKnownObjectLayout)
    {LOGMEIN("BackwardPass.cpp] 3169\n");
        Output::Print(_u("            Needs Known Object Layout: "));
        block->typesNeedingKnownObjectLayout->Dump();
    }

    if (this->DoFieldHoistCandidates() && !block->isDead)
    {LOGMEIN("BackwardPass.cpp] 3175\n");
        Output::Print(_u("            Exposed Field: "));
        block->fieldHoistCandidates->Dump();
    }

    if (block->byteCodeUpwardExposedUsed)
    {LOGMEIN("BackwardPass.cpp] 3181\n");
        Output::Print(_u("   Byte Code Exposed Uses: "));
        block->byteCodeUpwardExposedUsed->Dump();
    }

    if (!this->IsCollectionPass())
    {LOGMEIN("BackwardPass.cpp] 3187\n");
        if (!block->isDead)
        {LOGMEIN("BackwardPass.cpp] 3189\n");
            if (this->DoDeadStoreSlots())
            {LOGMEIN("BackwardPass.cpp] 3191\n");
                Output::Print(_u("Slot deadStore candidates: "));
                block->slotDeadStoreCandidates->Dump();
            }
            DumpMarkTemp();
        }
    }
    Output::Flush();
}
#endif

bool
BackwardPass::UpdateImplicitCallBailOutKind(IR::Instr *const instr, bool needsBailOutOnImplicitCall)
{LOGMEIN("BackwardPass.cpp] 3204\n");
    Assert(instr);
    Assert(instr->HasBailOutInfo());

    IR::BailOutKind implicitCallBailOutKind = needsBailOutOnImplicitCall ? IR::BailOutOnImplicitCalls : IR::BailOutInvalid;

    const IR::BailOutKind instrBailOutKind = instr->GetBailOutKind();
    if (instrBailOutKind & IR::BailOutMarkTempObject)
    {LOGMEIN("BackwardPass.cpp] 3212\n");
        // Don't remove the implicit call pre op bailout for mark temp object
        // Remove the mark temp object bit, as we don't need it after the dead store pass
        instr->SetBailOutKind(instrBailOutKind & ~IR::BailOutMarkTempObject);
        return true;
    }

    const IR::BailOutKind instrImplicitCallBailOutKind = instrBailOutKind & ~IR::BailOutKindBits;
    if(instrImplicitCallBailOutKind == IR::BailOutOnImplicitCallsPreOp)
    {LOGMEIN("BackwardPass.cpp] 3221\n");
        if(needsBailOutOnImplicitCall)
        {LOGMEIN("BackwardPass.cpp] 3223\n");
            implicitCallBailOutKind = IR::BailOutOnImplicitCallsPreOp;
        }
    }
    else if(instrImplicitCallBailOutKind != IR::BailOutOnImplicitCalls && instrImplicitCallBailOutKind != IR::BailOutInvalid)
    {LOGMEIN("BackwardPass.cpp] 3228\n");
        // This bailout kind (the value of 'instrImplicitCallBailOutKind') must guarantee that implicit calls will not happen.
        // If it doesn't make such a guarantee, it must be possible to merge this bailout kind with an implicit call bailout
        // kind, and therefore should be part of BailOutKindBits.
        Assert(!needsBailOutOnImplicitCall);
        return true;
    }

    if(instrImplicitCallBailOutKind == implicitCallBailOutKind)
    {LOGMEIN("BackwardPass.cpp] 3237\n");
        return true;
    }

    const IR::BailOutKind newBailOutKind = instrBailOutKind - instrImplicitCallBailOutKind + implicitCallBailOutKind;
    if(newBailOutKind == IR::BailOutInvalid)
    {LOGMEIN("BackwardPass.cpp] 3243\n");
        return false;
    }

    instr->SetBailOutKind(newBailOutKind);
    return true;
}

bool
BackwardPass::ProcessNoImplicitCallUses(IR::Instr *const instr)
{LOGMEIN("BackwardPass.cpp] 3253\n");
    Assert(instr);

    if(instr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {LOGMEIN("BackwardPass.cpp] 3257\n");
        return false;
    }
    Assert(tag == Js::DeadStorePhase);
    Assert(!instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsSymOpnd());
    Assert(!instr->GetSrc2() || instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsSymOpnd());

    if(IsCollectionPass())
    {LOGMEIN("BackwardPass.cpp] 3267\n");
        return true;
    }

    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < sizeof(srcs) / sizeof(srcs[0]) && srcs[i]; ++i)
    {LOGMEIN("BackwardPass.cpp] 3273\n");
        IR::Opnd *const src = srcs[i];
        IR::ArrayRegOpnd *arraySrc = nullptr;
        Sym *sym;
        switch(src->GetKind())
        {LOGMEIN("BackwardPass.cpp] 3278\n");
            case IR::OpndKindReg:
            {LOGMEIN("BackwardPass.cpp] 3280\n");
                IR::RegOpnd *const regSrc = src->AsRegOpnd();
                sym = regSrc->m_sym;
                if(considerSymAsRealUseInNoImplicitCallUses && considerSymAsRealUseInNoImplicitCallUses == sym)
                {LOGMEIN("BackwardPass.cpp] 3284\n");
                    considerSymAsRealUseInNoImplicitCallUses = nullptr;
                    ProcessStackSymUse(sym->AsStackSym(), true);
                }
                if(regSrc->IsArrayRegOpnd())
                {LOGMEIN("BackwardPass.cpp] 3289\n");
                    arraySrc = regSrc->AsArrayRegOpnd();
                }
                break;
            }

            case IR::OpndKindSym:
                sym = src->AsSymOpnd()->m_sym;
                Assert(sym->IsPropertySym());
                break;

            default:
                Assert(false);
                __assume(false);
        }

        currentBlock->noImplicitCallUses->Set(sym->m_id);
        const ValueType valueType(src->GetValueType());
        if(valueType.IsArrayOrObjectWithArray())
        {LOGMEIN("BackwardPass.cpp] 3308\n");
            if(valueType.HasNoMissingValues())
            {LOGMEIN("BackwardPass.cpp] 3310\n");
                currentBlock->noImplicitCallNoMissingValuesUses->Set(sym->m_id);
            }
            if(!valueType.HasVarElements())
            {LOGMEIN("BackwardPass.cpp] 3314\n");
                currentBlock->noImplicitCallNativeArrayUses->Set(sym->m_id);
            }
            if(arraySrc)
            {
                ProcessArrayRegOpndUse(instr, arraySrc);
            }
        }
    }

    if(!IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 3325\n");
        currentBlock->RemoveInstr(instr);
    }
    return true;
}

void
BackwardPass::ProcessNoImplicitCallDef(IR::Instr *const instr)
{LOGMEIN("BackwardPass.cpp] 3333\n");
    Assert(tag == Js::DeadStorePhase);
    Assert(instr);

    IR::Opnd *const dst = instr->GetDst();
    if(!dst)
    {LOGMEIN("BackwardPass.cpp] 3339\n");
        return;
    }

    Sym *dstSym;
    switch(dst->GetKind())
    {LOGMEIN("BackwardPass.cpp] 3345\n");
        case IR::OpndKindReg:
            dstSym = dst->AsRegOpnd()->m_sym;
            break;

        case IR::OpndKindSym:
            dstSym = dst->AsSymOpnd()->m_sym;
            if(!dstSym->IsPropertySym())
            {LOGMEIN("BackwardPass.cpp] 3353\n");
                return;
            }
            break;

        default:
            return;
    }

    if(!currentBlock->noImplicitCallUses->TestAndClear(dstSym->m_id))
    {LOGMEIN("BackwardPass.cpp] 3363\n");
        Assert(!currentBlock->noImplicitCallNoMissingValuesUses->Test(dstSym->m_id));
        Assert(!currentBlock->noImplicitCallNativeArrayUses->Test(dstSym->m_id));
        Assert(!currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->Test(dstSym->m_id));
        Assert(!currentBlock->noImplicitCallArrayLengthSymUses->Test(dstSym->m_id));
        return;
    }
    const bool transferNoMissingValuesUse = !!currentBlock->noImplicitCallNoMissingValuesUses->TestAndClear(dstSym->m_id);
    const bool transferNativeArrayUse = !!currentBlock->noImplicitCallNativeArrayUses->TestAndClear(dstSym->m_id);
    const bool transferJsArrayHeadSegmentSymUse =
        !!currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->TestAndClear(dstSym->m_id);
    const bool transferArrayLengthSymUse = !!currentBlock->noImplicitCallArrayLengthSymUses->TestAndClear(dstSym->m_id);

    IR::Opnd *const src = instr->GetSrc1();
    if(!src || instr->GetSrc2())
    {LOGMEIN("BackwardPass.cpp] 3378\n");
        return;
    }
    if(dst->IsRegOpnd() && src->IsRegOpnd())
    {LOGMEIN("BackwardPass.cpp] 3382\n");
        if(!OpCodeAttr::NonIntTransfer(instr->m_opcode))
        {LOGMEIN("BackwardPass.cpp] 3384\n");
            return;
        }
    }
    else if(
        !(
            // LdFld or similar
            (dst->IsRegOpnd() && src->IsSymOpnd() && src->AsSymOpnd()->m_sym->IsPropertySym()) ||

            // StFld or similar. Don't transfer a field opnd from StFld into the reg opnd src unless the field's value type is
            // definitely array or object with array, because only those value types require implicit calls to be disabled as
            // long as they are live. Other definite value types only require implicit calls to be disabled as long as a live
            // field holds the value, which is up to the StFld when going backwards.
            (src->IsRegOpnd() && dst->GetValueType().IsArrayOrObjectWithArray())
        ) ||
        !GlobOpt::TransferSrcValue(instr))
    {LOGMEIN("BackwardPass.cpp] 3400\n");
        return;
    }

    Sym *srcSym;
    switch(src->GetKind())
    {LOGMEIN("BackwardPass.cpp] 3406\n");
        case IR::OpndKindReg:
            srcSym = src->AsRegOpnd()->m_sym;
            break;

        case IR::OpndKindSym:
            srcSym = src->AsSymOpnd()->m_sym;
            Assert(srcSym->IsPropertySym());
            break;

        default:
            Assert(false);
            __assume(false);
    }

    currentBlock->noImplicitCallUses->Set(srcSym->m_id);
    if(transferNoMissingValuesUse)
    {LOGMEIN("BackwardPass.cpp] 3423\n");
        currentBlock->noImplicitCallNoMissingValuesUses->Set(srcSym->m_id);
    }
    if(transferNativeArrayUse)
    {LOGMEIN("BackwardPass.cpp] 3427\n");
        currentBlock->noImplicitCallNativeArrayUses->Set(srcSym->m_id);
    }
    if(transferJsArrayHeadSegmentSymUse)
    {LOGMEIN("BackwardPass.cpp] 3431\n");
        currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->Set(srcSym->m_id);
    }
    if(transferArrayLengthSymUse)
    {LOGMEIN("BackwardPass.cpp] 3435\n");
        currentBlock->noImplicitCallArrayLengthSymUses->Set(srcSym->m_id);
    }
}

template<class F>
IR::Opnd *
BackwardPass::FindNoImplicitCallUse(
    IR::Instr *const instr,
    StackSym *const sym,
    const F IsCheckedUse,
    IR::Instr * *const noImplicitCallUsesInstrRef)
{LOGMEIN("BackwardPass.cpp] 3447\n");
    IR::RegOpnd *const opnd = IR::RegOpnd::New(sym, sym->GetType(), instr->m_func);
    IR::Opnd *const use = FindNoImplicitCallUse(instr, opnd, IsCheckedUse, noImplicitCallUsesInstrRef);
    opnd->FreeInternal(instr->m_func);
    return use;
}

template<class F>
IR::Opnd *
BackwardPass::FindNoImplicitCallUse(
    IR::Instr *const instr,
    IR::Opnd *const opnd,
    const F IsCheckedUse,
    IR::Instr * *const noImplicitCallUsesInstrRef)
{LOGMEIN("BackwardPass.cpp] 3461\n");
    Assert(instr);
    Assert(instr->m_opcode != Js::OpCode::NoImplicitCallUses);

    // Skip byte-code uses
    IR::Instr *prevInstr = instr->m_prev;
    while(
        prevInstr &&
        !prevInstr->IsLabelInstr() &&
        (!prevInstr->IsRealInstr() || prevInstr->IsByteCodeUsesInstr()) &&
        prevInstr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {LOGMEIN("BackwardPass.cpp] 3472\n");
        prevInstr = prevInstr->m_prev;
    }

    // Find the corresponding use in a NoImplicitCallUses instruction
    for(; prevInstr && prevInstr->m_opcode == Js::OpCode::NoImplicitCallUses; prevInstr = prevInstr->m_prev)
    {LOGMEIN("BackwardPass.cpp] 3478\n");
        IR::Opnd *const checkedSrcs[] = { prevInstr->GetSrc1(), prevInstr->GetSrc2() };
        for(int i = 0; i < sizeof(checkedSrcs) / sizeof(checkedSrcs[0]) && checkedSrcs[i]; ++i)
        {LOGMEIN("BackwardPass.cpp] 3481\n");
            IR::Opnd *const checkedSrc = checkedSrcs[i];
            if(checkedSrc->IsEqual(opnd) && IsCheckedUse(checkedSrc))
            {LOGMEIN("BackwardPass.cpp] 3484\n");
                if(noImplicitCallUsesInstrRef)
                {LOGMEIN("BackwardPass.cpp] 3486\n");
                    *noImplicitCallUsesInstrRef = prevInstr;
                }
                return checkedSrc;
            }
        }
    }

    if(noImplicitCallUsesInstrRef)
    {LOGMEIN("BackwardPass.cpp] 3495\n");
        *noImplicitCallUsesInstrRef = nullptr;
    }
    return nullptr;
}

void
BackwardPass::ProcessArrayRegOpndUse(IR::Instr *const instr, IR::ArrayRegOpnd *const arrayRegOpnd)
{LOGMEIN("BackwardPass.cpp] 3503\n");
    Assert(tag == Js::DeadStorePhase);
    Assert(!IsCollectionPass());
    Assert(instr);
    Assert(arrayRegOpnd);

    if(!(arrayRegOpnd->HeadSegmentSym() || arrayRegOpnd->HeadSegmentLengthSym() || arrayRegOpnd->LengthSym()))
    {LOGMEIN("BackwardPass.cpp] 3510\n");
        return;
    }

    const ValueType arrayValueType(arrayRegOpnd->GetValueType());
    const bool isJsArray = !arrayValueType.IsLikelyTypedArray();
    Assert(isJsArray == arrayValueType.IsArrayOrObjectWithArray());
    Assert(!isJsArray == arrayValueType.IsOptimizedTypedArray());

    BasicBlock *const block = currentBlock;
    if(!IsPrePass() &&
        (arrayRegOpnd->HeadSegmentSym() || arrayRegOpnd->HeadSegmentLengthSym()) &&
        (!isJsArray || instr->m_opcode != Js::OpCode::NoImplicitCallUses))
    {LOGMEIN("BackwardPass.cpp] 3523\n");
        bool headSegmentIsLoadedButUnused =
            instr->loadedArrayHeadSegment &&
            arrayRegOpnd->HeadSegmentSym() &&
            !block->upwardExposedUses->Test(arrayRegOpnd->HeadSegmentSym()->m_id);
        const bool headSegmentLengthIsLoadedButUnused =
            instr->loadedArrayHeadSegmentLength &&
            arrayRegOpnd->HeadSegmentLengthSym() &&
            !block->upwardExposedUses->Test(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
        if(headSegmentLengthIsLoadedButUnused && instr->extractedUpperBoundCheckWithoutHoisting)
        {LOGMEIN("BackwardPass.cpp] 3533\n");
            // Find the upper bound check (index[src1] <= headSegmentLength[src2] + offset[dst])
            IR::Instr *upperBoundCheck = this->globOpt->FindUpperBoundsCheckInstr(instr);
            Assert(upperBoundCheck && upperBoundCheck != instr);
            Assert(upperBoundCheck->GetSrc2()->AsRegOpnd()->m_sym == arrayRegOpnd->HeadSegmentLengthSym());

            // Find the head segment length load
            IR::Instr *headSegmentLengthLoad = this->globOpt->FindArraySegmentLoadInstr(upperBoundCheck);

            Assert(headSegmentLengthLoad->GetDst()->AsRegOpnd()->m_sym == arrayRegOpnd->HeadSegmentLengthSym());
            Assert(
                headSegmentLengthLoad->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->m_sym ==
                (isJsArray ? arrayRegOpnd->HeadSegmentSym() : arrayRegOpnd->m_sym));

            // Fold the head segment length load into the upper bound check. Keep the load instruction there with a Nop so that
            // the head segment length sym can be marked as unused before the Nop. The lowerer will remove it.
            upperBoundCheck->ReplaceSrc2(headSegmentLengthLoad->UnlinkSrc1());
            headSegmentLengthLoad->m_opcode = Js::OpCode::Nop;

            if(isJsArray)
            {LOGMEIN("BackwardPass.cpp] 3553\n");
                // The head segment length is on the head segment, so the bound check now uses the head segment sym
                headSegmentIsLoadedButUnused = false;
            }
        }

        if(headSegmentIsLoadedButUnused || headSegmentLengthIsLoadedButUnused)
        {LOGMEIN("BackwardPass.cpp] 3560\n");
            // Check if the head segment / head segment length are being loaded here. If so, remove them and let the fast
            // path load them since it does a better job.
            IR::ArrayRegOpnd *noImplicitCallArrayUse = nullptr;
            if(isJsArray)
            {LOGMEIN("BackwardPass.cpp] 3565\n");
                IR::Opnd *const use =
                    FindNoImplicitCallUse(
                        instr,
                        arrayRegOpnd,
                        [&](IR::Opnd *const checkedSrc) -> bool
                        {
                            const ValueType checkedSrcValueType(checkedSrc->GetValueType());
                            if(!checkedSrcValueType.IsLikelyObject() ||
                                checkedSrcValueType.GetObjectType() != arrayValueType.GetObjectType())
                            {LOGMEIN("BackwardPass.cpp] 3575\n");
                                return false;
                            }

                            IR::RegOpnd *const checkedRegSrc = checkedSrc->AsRegOpnd();
                            if(!checkedRegSrc->IsArrayRegOpnd())
                            {LOGMEIN("BackwardPass.cpp] 3581\n");
                                return false;
                            }

                            IR::ArrayRegOpnd *const checkedArraySrc = checkedRegSrc->AsArrayRegOpnd();
                            if(headSegmentIsLoadedButUnused &&
                                checkedArraySrc->HeadSegmentSym() != arrayRegOpnd->HeadSegmentSym())
                            {LOGMEIN("BackwardPass.cpp] 3588\n");
                                return false;
                            }
                            if(headSegmentLengthIsLoadedButUnused &&
                                checkedArraySrc->HeadSegmentLengthSym() != arrayRegOpnd->HeadSegmentLengthSym())
                            {LOGMEIN("BackwardPass.cpp] 3593\n");
                                return false;
                            }
                            return true;
                        });
                if(use)
                {LOGMEIN("BackwardPass.cpp] 3599\n");
                    noImplicitCallArrayUse = use->AsRegOpnd()->AsArrayRegOpnd();
                }
            }
            else if(headSegmentLengthIsLoadedButUnused)
            {LOGMEIN("BackwardPass.cpp] 3604\n");
                // A typed array's head segment length may be zeroed when the typed array's buffer is transferred to a web
                // worker, so the head segment length sym use is included in a NoImplicitCallUses instruction. Since there
                // are no forward uses of the head segment length sym, to allow removing the extracted head segment length
                // load, the corresponding head segment length sym use in the NoImplicitCallUses instruction must also be
                // removed.
                IR::Instr *noImplicitCallUsesInstr;
                IR::Opnd *const use =
                    FindNoImplicitCallUse(
                        instr,
                        arrayRegOpnd->HeadSegmentLengthSym(),
                        [&](IR::Opnd *const checkedSrc) -> bool
                        {
                            return checkedSrc->AsRegOpnd()->m_sym == arrayRegOpnd->HeadSegmentLengthSym();
                        },
                        &noImplicitCallUsesInstr);
                if(use)
                {LOGMEIN("BackwardPass.cpp] 3621\n");
                    Assert(noImplicitCallUsesInstr);
                    Assert(!noImplicitCallUsesInstr->GetDst());
                    Assert(noImplicitCallUsesInstr->GetSrc1());
                    if(use == noImplicitCallUsesInstr->GetSrc1())
                    {LOGMEIN("BackwardPass.cpp] 3626\n");
                        if(noImplicitCallUsesInstr->GetSrc2())
                        {LOGMEIN("BackwardPass.cpp] 3628\n");
                            noImplicitCallUsesInstr->ReplaceSrc1(noImplicitCallUsesInstr->UnlinkSrc2());
                        }
                        else
                        {
                            noImplicitCallUsesInstr->FreeSrc1();
                            noImplicitCallUsesInstr->m_opcode = Js::OpCode::Nop;
                        }
                    }
                    else
                    {
                        Assert(use == noImplicitCallUsesInstr->GetSrc2());
                        noImplicitCallUsesInstr->FreeSrc2();
                    }
                }
            }

            if(headSegmentIsLoadedButUnused &&
                (!isJsArray || !arrayRegOpnd->HeadSegmentLengthSym() || headSegmentLengthIsLoadedButUnused))
            {LOGMEIN("BackwardPass.cpp] 3647\n");
                // For JS arrays, the head segment length load is dependent on the head segment. So, only remove the head
                // segment load if the head segment length load can also be removed.
                arrayRegOpnd->RemoveHeadSegmentSym();
                instr->loadedArrayHeadSegment = false;
                if(noImplicitCallArrayUse)
                {LOGMEIN("BackwardPass.cpp] 3653\n");
                    noImplicitCallArrayUse->RemoveHeadSegmentSym();
                }
            }
            if(headSegmentLengthIsLoadedButUnused)
            {LOGMEIN("BackwardPass.cpp] 3658\n");
                arrayRegOpnd->RemoveHeadSegmentLengthSym();
                instr->loadedArrayHeadSegmentLength = false;
                if(noImplicitCallArrayUse)
                {LOGMEIN("BackwardPass.cpp] 3662\n");
                    noImplicitCallArrayUse->RemoveHeadSegmentLengthSym();
                }
            }
        }
    }

    if(isJsArray && instr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {LOGMEIN("BackwardPass.cpp] 3670\n");
        // Only uses in NoImplicitCallUses instructions are counted toward liveness
        return;
    }

    // Treat dependent syms as uses. For JS arrays, only uses in NoImplicitCallUses count because only then the assumptions made
    // on the dependent syms are guaranteed to be valid. Similarly for typed arrays, a head segment length sym use counts toward
    // liveness only in a NoImplicitCallUses instruction.
    if(arrayRegOpnd->HeadSegmentSym())
    {LOGMEIN("BackwardPass.cpp] 3679\n");
        ProcessStackSymUse(arrayRegOpnd->HeadSegmentSym(), true);
        if(isJsArray)
        {LOGMEIN("BackwardPass.cpp] 3682\n");
            block->noImplicitCallUses->Set(arrayRegOpnd->HeadSegmentSym()->m_id);
            block->noImplicitCallJsArrayHeadSegmentSymUses->Set(arrayRegOpnd->HeadSegmentSym()->m_id);
        }
    }
    if(arrayRegOpnd->HeadSegmentLengthSym())
    {LOGMEIN("BackwardPass.cpp] 3688\n");
        if(isJsArray)
        {LOGMEIN("BackwardPass.cpp] 3690\n");
            ProcessStackSymUse(arrayRegOpnd->HeadSegmentLengthSym(), true);
            block->noImplicitCallUses->Set(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
            block->noImplicitCallJsArrayHeadSegmentSymUses->Set(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
        }
        else
        {
            // ProcessNoImplicitCallUses automatically marks JS array reg opnds and their corresponding syms as live. A typed
            // array's head segment length sym also needs to be marked as live at its use in the NoImplicitCallUses instruction,
            // but it is just in a reg opnd. Flag the opnd to have the sym be marked as live when that instruction is processed.
            Assert(!considerSymAsRealUseInNoImplicitCallUses);
            IR::Opnd *const use =
                FindNoImplicitCallUse(
                    instr,
                    arrayRegOpnd->HeadSegmentLengthSym(),
                    [&](IR::Opnd *const checkedSrc) -> bool
                    {
                        return checkedSrc->AsRegOpnd()->m_sym == arrayRegOpnd->HeadSegmentLengthSym();
                    });
            if(use)
            {LOGMEIN("BackwardPass.cpp] 3710\n");
                considerSymAsRealUseInNoImplicitCallUses = arrayRegOpnd->HeadSegmentLengthSym();
            }
        }
    }
    StackSym *const lengthSym = arrayRegOpnd->LengthSym();
    if(lengthSym && lengthSym != arrayRegOpnd->HeadSegmentLengthSym())
    {
        ProcessStackSymUse(lengthSym, true);
        Assert(arrayValueType.IsArray());
        block->noImplicitCallUses->Set(lengthSym->m_id);
        block->noImplicitCallArrayLengthSymUses->Set(lengthSym->m_id);
    }
}

void
BackwardPass::ProcessNewScObject(IR::Instr* instr)
{LOGMEIN("BackwardPass.cpp] 3727\n");
    if (this->tag != Js::DeadStorePhase || IsCollectionPass())
    {LOGMEIN("BackwardPass.cpp] 3729\n");
        return;
    }

    if (!instr->IsNewScObjectInstr())
    {LOGMEIN("BackwardPass.cpp] 3734\n");
        return;
    }

    if (instr->HasBailOutInfo())
    {LOGMEIN("BackwardPass.cpp] 3739\n");
        Assert(instr->IsProfiledInstr());
        Assert(instr->GetBailOutKind() == IR::BailOutFailedCtorGuardCheck);
        Assert(instr->GetDst()->IsRegOpnd());

        BasicBlock * block = this->currentBlock;
        StackSym* objSym = instr->GetDst()->AsRegOpnd()->GetStackSym();

        if (block->upwardExposedUses->Test(objSym->m_id))
        {LOGMEIN("BackwardPass.cpp] 3748\n");
            // If the object created here is used downstream, let's capture any property operations we must protect.

            Assert(instr->GetDst()->AsRegOpnd()->GetStackSym()->HasObjectTypeSym());

            JITTimeConstructorCache* ctorCache = instr->m_func->GetConstructorCache(static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId));

            if (block->stackSymToFinalType != nullptr)
            {LOGMEIN("BackwardPass.cpp] 3756\n");
                // NewScObject is the origin of the object pointer. If we have a final type in hand, do the
                // transition here.
                AddPropertyCacheBucket *pBucket = block->stackSymToFinalType->Get(objSym->m_id);
                if (pBucket &&
                    pBucket->GetInitialType() != nullptr &&
                    pBucket->GetFinalType() != pBucket->GetInitialType())
                {LOGMEIN("BackwardPass.cpp] 3763\n");
                    Assert(pBucket->GetInitialType() == ctorCache->GetType());
                    if (!this->IsPrePass())
                    {LOGMEIN("BackwardPass.cpp] 3766\n");
                        this->InsertTypeTransition(instr->m_next, objSym, pBucket);
                    }
#if DBG
                    pBucket->deadStoreUnavailableInitialType = pBucket->GetInitialType();
                    if (pBucket->deadStoreUnavailableFinalType == nullptr)
                    {LOGMEIN("BackwardPass.cpp] 3772\n");
                        pBucket->deadStoreUnavailableFinalType = pBucket->GetFinalType();
                    }
                    pBucket->SetInitialType(nullptr);
                    pBucket->SetFinalType(nullptr);
#else
                    block->stackSymToFinalType->Clear(objSym->m_id);
#endif
                }
            }

            if (block->stackSymToGuardedProperties != nullptr)
            {LOGMEIN("BackwardPass.cpp] 3784\n");
                ObjTypeGuardBucket* bucket = block->stackSymToGuardedProperties->Get(objSym->m_id);
                if (bucket != nullptr)
                {LOGMEIN("BackwardPass.cpp] 3787\n");
                    BVSparse<JitArenaAllocator>* guardedPropertyOps = bucket->GetGuardedPropertyOps();
                    if (guardedPropertyOps != nullptr)
                    {LOGMEIN("BackwardPass.cpp] 3790\n");
                        ctorCache->EnsureGuardedPropOps(this->func->m_alloc);
                        ctorCache->AddGuardedPropOps(guardedPropertyOps);

                        bucket->SetGuardedPropertyOps(nullptr);
                        JitAdelete(this->tempAlloc, guardedPropertyOps);
                        block->stackSymToGuardedProperties->Clear(objSym->m_id);
                    }
                }
            }
        }
        else
        {
            // If the object is not used downstream, let's remove the bailout and let the lowerer emit a fast path along with
            // the fallback on helper, if the ctor cache ever became invalid.
            instr->ClearBailOutInfo();
            if (preOpBailOutInstrToProcess == instr)
            {LOGMEIN("BackwardPass.cpp] 3807\n");
                preOpBailOutInstrToProcess = nullptr;
            }

#if DBG
            // We're creating a brand new object here, so no type check upstream could protect any properties of this
            // object. Let's make sure we don't have any left to protect.
            ObjTypeGuardBucket* bucket = block->stackSymToGuardedProperties != nullptr ?
                block->stackSymToGuardedProperties->Get(objSym->m_id) : nullptr;
            Assert(bucket == nullptr || bucket->GetGuardedPropertyOps()->IsEmpty());
#endif
        }
    }
}

void
BackwardPass::UpdateArrayValueTypes(IR::Instr *const instr, IR::Opnd *origOpnd)
{LOGMEIN("BackwardPass.cpp] 3824\n");
    Assert(tag == Js::DeadStorePhase);
    Assert(!IsPrePass());
    Assert(instr);

    if(!origOpnd)
    {LOGMEIN("BackwardPass.cpp] 3830\n");
        return;
    }

    IR::Instr *opndOwnerInstr = instr;
    switch(instr->m_opcode)
    {LOGMEIN("BackwardPass.cpp] 3836\n");
        case Js::OpCode::StElemC:
        case Js::OpCode::StArrSegElemC:
            // These may not be fixed if we are unsure about the type of the array they're storing to
            // (because it relies on profile data) and we weren't able to hoist the array check.
            return;
    }

    Sym *sym;
    IR::Opnd* opnd = origOpnd;
    IR::ArrayRegOpnd *arrayOpnd;
    switch(opnd->GetKind())
    {LOGMEIN("BackwardPass.cpp] 3848\n");
        case IR::OpndKindIndir:
            opnd = opnd->AsIndirOpnd()->GetBaseOpnd();
            // fall-through

        case IR::OpndKindReg:
        {LOGMEIN("BackwardPass.cpp] 3854\n");
            IR::RegOpnd *const regOpnd = opnd->AsRegOpnd();
            sym = regOpnd->m_sym;
            arrayOpnd = regOpnd->IsArrayRegOpnd() ? regOpnd->AsArrayRegOpnd() : nullptr;
            break;
        }

        case IR::OpndKindSym:
            sym = opnd->AsSymOpnd()->m_sym;
            if(!sym->IsPropertySym())
            {LOGMEIN("BackwardPass.cpp] 3864\n");
                return;
            }
            arrayOpnd = nullptr;
            break;

        default:
            return;
    }

    const ValueType valueType(opnd->GetValueType());
    if(!valueType.IsAnyOptimizedArray())
    {LOGMEIN("BackwardPass.cpp] 3876\n");
        return;
    }

    const bool isJsArray = valueType.IsArrayOrObjectWithArray();
    Assert(!isJsArray == valueType.IsOptimizedTypedArray());

    const bool noForwardImplicitCallUses = currentBlock->noImplicitCallUses->IsEmpty();
    bool changeArray = isJsArray && !opnd->IsValueTypeFixed() && noForwardImplicitCallUses;
    bool changeNativeArray =
        isJsArray &&
        !opnd->IsValueTypeFixed() &&
        !valueType.HasVarElements() &&
        currentBlock->noImplicitCallNativeArrayUses->IsEmpty();
    bool changeNoMissingValues =
        isJsArray &&
        !opnd->IsValueTypeFixed() &&
        valueType.HasNoMissingValues() &&
        currentBlock->noImplicitCallNoMissingValuesUses->IsEmpty();
    const bool noForwardJsArrayHeadSegmentSymUses = currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty();
    bool removeHeadSegmentSym = isJsArray && arrayOpnd && arrayOpnd->HeadSegmentSym() && noForwardJsArrayHeadSegmentSymUses;
    bool removeHeadSegmentLengthSym =
        arrayOpnd &&
        arrayOpnd->HeadSegmentLengthSym() &&
        (isJsArray ? noForwardJsArrayHeadSegmentSymUses : noForwardImplicitCallUses);
    Assert(!isJsArray || !arrayOpnd || !arrayOpnd->LengthSym() || valueType.IsArray());
    bool removeLengthSym =
        isJsArray &&
        arrayOpnd &&
        arrayOpnd->LengthSym() &&
        currentBlock->noImplicitCallArrayLengthSymUses->IsEmpty();
    if(!(changeArray || changeNoMissingValues || changeNativeArray || removeHeadSegmentSym || removeHeadSegmentLengthSym))
    {LOGMEIN("BackwardPass.cpp] 3908\n");
        return;
    }

    // We have a definitely-array value type for the base, but either implicit calls are not currently being disabled for
    // legally using the value type as a definite array, or we are not currently bailing out upon creating a missing value
    // for legally using the value type as a definite array with no missing values.

    // For source opnds, ensure that a NoImplicitCallUses immediately precedes this instruction. Otherwise, convert the value
    // type to an appropriate version so that the lowerer doesn't incorrectly treat it as it says.
    if(opnd != opndOwnerInstr->GetDst())
    {LOGMEIN("BackwardPass.cpp] 3919\n");
        if(isJsArray)
        {LOGMEIN("BackwardPass.cpp] 3921\n");
            IR::Opnd *const checkedSrc =
                FindNoImplicitCallUse(
                    instr,
                    opnd,
                    [&](IR::Opnd *const checkedSrc) -> bool
                    {
                        const ValueType checkedSrcValueType(checkedSrc->GetValueType());
                        return
                            checkedSrcValueType.IsLikelyObject() &&
                            checkedSrcValueType.GetObjectType() == valueType.GetObjectType();
                    });
            if(checkedSrc)
            {LOGMEIN("BackwardPass.cpp] 3934\n");
                // Implicit calls will be disabled to the point immediately before this instruction
                changeArray = false;

                const ValueType checkedSrcValueType(checkedSrc->GetValueType());
                if(changeNativeArray &&
                    !checkedSrcValueType.HasVarElements() &&
                    checkedSrcValueType.HasIntElements() == valueType.HasIntElements())
                {LOGMEIN("BackwardPass.cpp] 3942\n");
                    // If necessary, instructions before this will bail out on converting a native array
                    changeNativeArray = false;
                }

                if(changeNoMissingValues && checkedSrcValueType.HasNoMissingValues())
                {LOGMEIN("BackwardPass.cpp] 3948\n");
                    // If necessary, instructions before this will bail out on creating a missing value
                    changeNoMissingValues = false;
                }

                if((removeHeadSegmentSym || removeHeadSegmentLengthSym || removeLengthSym) && checkedSrc->IsRegOpnd())
                {LOGMEIN("BackwardPass.cpp] 3954\n");
                    IR::RegOpnd *const checkedRegSrc = checkedSrc->AsRegOpnd();
                    if(checkedRegSrc->IsArrayRegOpnd())
                    {LOGMEIN("BackwardPass.cpp] 3957\n");
                        IR::ArrayRegOpnd *const checkedArraySrc = checkedSrc->AsRegOpnd()->AsArrayRegOpnd();
                        if(removeHeadSegmentSym && checkedArraySrc->HeadSegmentSym() == arrayOpnd->HeadSegmentSym())
                        {LOGMEIN("BackwardPass.cpp] 3960\n");
                            // If necessary, instructions before this will bail out upon invalidating head segment sym
                            removeHeadSegmentSym = false;
                        }
                        if(removeHeadSegmentLengthSym &&
                            checkedArraySrc->HeadSegmentLengthSym() == arrayOpnd->HeadSegmentLengthSym())
                        {LOGMEIN("BackwardPass.cpp] 3966\n");
                            // If necessary, instructions before this will bail out upon invalidating head segment length sym
                            removeHeadSegmentLengthSym = false;
                        }
                        if(removeLengthSym && checkedArraySrc->LengthSym() == arrayOpnd->LengthSym())
                        {LOGMEIN("BackwardPass.cpp] 3971\n");
                            // If necessary, instructions before this will bail out upon invalidating a length sym
                            removeLengthSym = false;
                        }
                    }
                }
            }
        }
        else
        {
            Assert(removeHeadSegmentLengthSym);

            // A typed array's head segment length may be zeroed when the typed array's buffer is transferred to a web worker,
            // so the head segment length sym use is included in a NoImplicitCallUses instruction. Since there are no forward
            // uses of any head segment length syms, to allow removing the extracted head segment length
            // load, the corresponding head segment length sym use in the NoImplicitCallUses instruction must also be
            // removed.
            IR::Opnd *const use =
                FindNoImplicitCallUse(
                    instr,
                    arrayOpnd->HeadSegmentLengthSym(),
                    [&](IR::Opnd *const checkedSrc) -> bool
                    {
                        return checkedSrc->AsRegOpnd()->m_sym == arrayOpnd->HeadSegmentLengthSym();
                    });
            if(use)
            {LOGMEIN("BackwardPass.cpp] 3997\n");
                // Implicit calls will be disabled to the point immediately before this instruction
                removeHeadSegmentLengthSym = false;
            }
        }
    }

    if(changeArray || changeNativeArray)
    {LOGMEIN("BackwardPass.cpp] 4005\n");
        if(arrayOpnd)
        {LOGMEIN("BackwardPass.cpp] 4007\n");
            opnd = arrayOpnd->CopyAsRegOpnd(opndOwnerInstr->m_func);
            if (origOpnd->IsIndirOpnd())
            {LOGMEIN("BackwardPass.cpp] 4010\n");
                origOpnd->AsIndirOpnd()->ReplaceBaseOpnd(opnd->AsRegOpnd());
            }
            else
            {
                opndOwnerInstr->Replace(arrayOpnd, opnd);
            }
            arrayOpnd = nullptr;
        }
        opnd->SetValueType(valueType.ToLikely());
    }
    else
    {
        if(changeNoMissingValues)
        {LOGMEIN("BackwardPass.cpp] 4024\n");
            opnd->SetValueType(valueType.SetHasNoMissingValues(false));
        }
        if(removeHeadSegmentSym)
        {LOGMEIN("BackwardPass.cpp] 4028\n");
            Assert(arrayOpnd);
            arrayOpnd->RemoveHeadSegmentSym();
        }
        if(removeHeadSegmentLengthSym)
        {LOGMEIN("BackwardPass.cpp] 4033\n");
            Assert(arrayOpnd);
            arrayOpnd->RemoveHeadSegmentLengthSym();
        }
        if(removeLengthSym)
        {LOGMEIN("BackwardPass.cpp] 4038\n");
            Assert(arrayOpnd);
            arrayOpnd->RemoveLengthSym();
        }
    }
}

void
BackwardPass::UpdateArrayBailOutKind(IR::Instr *const instr)
{LOGMEIN("BackwardPass.cpp] 4047\n");
    Assert(!IsPrePass());
    Assert(instr);
    Assert(instr->HasBailOutInfo());

    if ((instr->m_opcode != Js::OpCode::StElemI_A && instr->m_opcode != Js::OpCode::StElemI_A_Strict &&
        instr->m_opcode != Js::OpCode::Memcopy && instr->m_opcode != Js::OpCode::Memset) ||
        !instr->GetDst()->IsIndirOpnd())
    {LOGMEIN("BackwardPass.cpp] 4055\n");
        return;
    }

    IR::RegOpnd *const baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
    const ValueType baseValueType(baseOpnd->GetValueType());
    if(baseValueType.IsNotArrayOrObjectWithArray())
    {LOGMEIN("BackwardPass.cpp] 4062\n");
        return;
    }

    IR::BailOutKind includeBailOutKinds = IR::BailOutInvalid;
    if(!baseValueType.IsNotNativeArray() &&
        (!baseValueType.IsLikelyNativeArray() || instr->GetSrc1()->IsVar()) &&
        !currentBlock->noImplicitCallNativeArrayUses->IsEmpty())
    {LOGMEIN("BackwardPass.cpp] 4070\n");
        // There is an upwards-exposed use of a native array. Since the array referenced by this instruction can be aliased,
        // this instruction needs to bail out if it converts the native array even if this array specifically is not
        // upwards-exposed.
        includeBailOutKinds |= IR::BailOutConvertedNativeArray;
    }

    if(baseOpnd->IsArrayRegOpnd() && baseOpnd->AsArrayRegOpnd()->EliminatedUpperBoundCheck())
    {LOGMEIN("BackwardPass.cpp] 4078\n");
        if(instr->extractedUpperBoundCheckWithoutHoisting && !currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty())
        {LOGMEIN("BackwardPass.cpp] 4080\n");
            // See comment below regarding head segment invalidation. A failed upper bound check usually means that it will
            // invalidate the head segment length, so change the bailout kind on the upper bound check to have it bail out for
            // the right reason. Even though the store may actually occur in a non-head segment, which would not invalidate the
            // head segment or length, any store outside the head segment bounds causes head segment load elimination to be
            // turned off for the store, because the segment structure of the array is not guaranteed to be the same every time.
            IR::Instr *upperBoundCheck = this->globOpt->FindUpperBoundsCheckInstr(instr);
            Assert(upperBoundCheck && upperBoundCheck != instr);

            if(upperBoundCheck->GetBailOutKind() == IR::BailOutOnArrayAccessHelperCall)
            {LOGMEIN("BackwardPass.cpp] 4090\n");
                upperBoundCheck->SetBailOutKind(IR::BailOutOnInvalidatedArrayHeadSegment);
            }
            else
            {
                Assert(upperBoundCheck->GetBailOutKind() == IR::BailOutOnFailedHoistedBoundCheck);
            }
        }
    }
    else
    {
        if(!currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty())
        {LOGMEIN("BackwardPass.cpp] 4102\n");
            // There is an upwards-exposed use of a segment sym. Since the head segment syms referenced by this instruction can
            // be aliased, this instruction needs to bail out if it changes the segment syms it references even if the ones it
            // references specifically are not upwards-exposed. This bailout kind also guarantees that this element store will
            // not create missing values.
            includeBailOutKinds |= IR::BailOutOnInvalidatedArrayHeadSegment;
        }
        else if(
            !currentBlock->noImplicitCallNoMissingValuesUses->IsEmpty() &&
            !(instr->GetBailOutKind() & IR::BailOutOnArrayAccessHelperCall))
        {LOGMEIN("BackwardPass.cpp] 4112\n");
            // There is an upwards-exposed use of an array with no missing values. Since the array referenced by this
            // instruction can be aliased, this instruction needs to bail out if it creates a missing value in the array even if
            // this array specifically is not upwards-exposed.
            includeBailOutKinds |= IR::BailOutOnMissingValue;
        }

        if(!baseValueType.IsNotArray() && !currentBlock->noImplicitCallArrayLengthSymUses->IsEmpty())
        {LOGMEIN("BackwardPass.cpp] 4120\n");
            // There is an upwards-exposed use of a length sym. Since the length sym referenced by this instruction can be
            // aliased, this instruction needs to bail out if it changes the length sym it references even if the ones it
            // references specifically are not upwards-exposed.
            includeBailOutKinds |= IR::BailOutOnInvalidatedArrayLength;
        }
    }

    if(!includeBailOutKinds)
    {LOGMEIN("BackwardPass.cpp] 4129\n");
        return;
    }

    Assert(!(includeBailOutKinds & ~IR::BailOutKindBits));
    instr->SetBailOutKind(instr->GetBailOutKind() | includeBailOutKinds);
}

bool
BackwardPass::ProcessStackSymUse(StackSym * stackSym, BOOLEAN isNonByteCodeUse)
{LOGMEIN("BackwardPass.cpp] 4139\n");
    BasicBlock * block = this->currentBlock;

    if (this->DoByteCodeUpwardExposedUsed())
    {LOGMEIN("BackwardPass.cpp] 4143\n");
        if (!isNonByteCodeUse && stackSym->HasByteCodeRegSlot())
        {LOGMEIN("BackwardPass.cpp] 4145\n");
            // Always track the sym use on the var sym.
            StackSym * byteCodeUseSym = stackSym;
            if (byteCodeUseSym->IsTypeSpec())
            {LOGMEIN("BackwardPass.cpp] 4149\n");
                // It has to have a var version for byte code regs
                byteCodeUseSym = byteCodeUseSym->GetVarEquivSym(nullptr);
            }
            block->byteCodeUpwardExposedUsed->Set(byteCodeUseSym->m_id);
#if DBG
            // We can only track first level function stack syms right now
            if (byteCodeUseSym->GetByteCodeFunc() == this->func)
            {LOGMEIN("BackwardPass.cpp] 4157\n");
                Js::RegSlot byteCodeRegSlot = byteCodeUseSym->GetByteCodeRegSlot();
                if (block->byteCodeRestoreSyms[byteCodeRegSlot] != byteCodeUseSym)
                {LOGMEIN("BackwardPass.cpp] 4160\n");
                    AssertMsg(block->byteCodeRestoreSyms[byteCodeRegSlot] == nullptr,
                        "Can't have two active lifetime for the same byte code register");
                    block->byteCodeRestoreSyms[byteCodeRegSlot] = byteCodeUseSym;
                }
            }
#endif
        }
    }

    if(IsCollectionPass())
    {LOGMEIN("BackwardPass.cpp] 4171\n");
        return true;
    }

    if (this->DoMarkTempObjects())
    {LOGMEIN("BackwardPass.cpp] 4176\n");
        Assert((block->loop != nullptr) == block->tempObjectTracker->HasTempTransferDependencies());
        block->tempObjectTracker->ProcessUse(stackSym, this);
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {LOGMEIN("BackwardPass.cpp] 4182\n");
        Assert((block->loop != nullptr) == block->tempObjectVerifyTracker->HasTempTransferDependencies());
        block->tempObjectVerifyTracker->ProcessUse(stackSym, this);
    }
#endif
    return !!block->upwardExposedUses->TestAndSet(stackSym->m_id);
}

bool
BackwardPass::ProcessSymUse(Sym * sym, bool isRegOpndUse, BOOLEAN isNonByteCodeUse)
{LOGMEIN("BackwardPass.cpp] 4192\n");
    BasicBlock * block = this->currentBlock;
    
    if (CanDeadStoreInstrForScopeObjRemoval(sym))   
    {LOGMEIN("BackwardPass.cpp] 4196\n");
        return false;
    }

    if (sym->IsPropertySym())
    {LOGMEIN("BackwardPass.cpp] 4201\n");
        PropertySym * propertySym = sym->AsPropertySym();
        ProcessStackSymUse(propertySym->m_stackSym, isNonByteCodeUse);

        if(IsCollectionPass())
        {LOGMEIN("BackwardPass.cpp] 4206\n");
            return true;
        }

        Assert((block->fieldHoistCandidates != nullptr) == this->DoFieldHoistCandidates());

        if (block->fieldHoistCandidates && GlobOpt::TransferSrcValue(this->currentInstr))
        {LOGMEIN("BackwardPass.cpp] 4213\n");
            // If the instruction doesn't transfer the src value to dst, it will not be copyprop'd
            // So we can't hoist those.
            block->fieldHoistCandidates->Set(propertySym->m_id);
        }

        if (this->DoDeadStoreSlots())
        {LOGMEIN("BackwardPass.cpp] 4220\n");
            block->slotDeadStoreCandidates->Clear(propertySym->m_id);
        }

        if (tag == Js::BackwardPhase)
        {LOGMEIN("BackwardPass.cpp] 4225\n");
            // Backward phase tracks liveness of fields to tell GlobOpt where we may need bailout.
            return this->ProcessPropertySymUse(propertySym);
        }
        else
        {
            // Dead-store phase tracks copy propped syms, so it only cares about ByteCodeUses we inserted,
            // not live fields.
            return false;
        }
    }

    StackSym * stackSym = sym->AsStackSym();
    bool isUsed = ProcessStackSymUse(stackSym, isNonByteCodeUse);

    if (!IsCollectionPass() && isRegOpndUse && this->DoMarkTempNumbers())
    {LOGMEIN("BackwardPass.cpp] 4241\n");
        // Collect mark temp number information
        Assert((block->loop != nullptr) == block->tempNumberTracker->HasTempTransferDependencies());
        block->tempNumberTracker->ProcessUse(stackSym, this);
    }

    return isUsed;
}

bool
BackwardPass::MayPropertyBeWrittenTo(Js::PropertyId propertyId)
{LOGMEIN("BackwardPass.cpp] 4252\n");
    return this->func->anyPropertyMayBeWrittenTo ||
        (this->func->propertiesWrittenTo != nullptr && this->func->propertiesWrittenTo->ContainsKey(propertyId));
}

void
BackwardPass::ProcessPropertySymOpndUse(IR::PropertySymOpnd * opnd)
{LOGMEIN("BackwardPass.cpp] 4259\n");

    // If this operand doesn't participate in the type check sequence it's a pass-through.
    // We will not set any bits on the operand and we will ignore them when lowering.
    if (!opnd->IsTypeCheckSeqCandidate())
    {LOGMEIN("BackwardPass.cpp] 4264\n");
        return;
    }

    AssertMsg(opnd->HasObjectTypeSym(), "Optimized property sym operand without a type sym?");
    SymID typeSymId = opnd->GetObjectTypeSym()->m_id;

    BasicBlock * block = this->currentBlock;
    if (this->tag == Js::BackwardPhase)
    {LOGMEIN("BackwardPass.cpp] 4273\n");
        // In the backward phase, we have no availability info, and we're trying to see
        // where there are live fields so we can decide where to put bailouts.

        Assert(opnd->MayNeedTypeCheckProtection());

        block->upwardExposedFields->Set(typeSymId);

        TrackObjTypeSpecWriteGuards(opnd, block);
    }
    else
    {
        // In the dead-store phase, we're trying to see where the lowered code needs to make sure to check
        // types for downstream load/stores. We're also setting up the upward-exposed uses at loop headers
        // so register allocation will be correct.

        Assert(opnd->MayNeedTypeCheckProtection());

        const bool isStore = opnd == this->currentInstr->GetDst();

        // Note that we don't touch upwardExposedUses here.
        if (opnd->IsTypeAvailable())
        {LOGMEIN("BackwardPass.cpp] 4295\n");
            opnd->SetTypeDead(!block->upwardExposedFields->TestAndSet(typeSymId));

            if (opnd->IsTypeChecked() && opnd->IsObjectHeaderInlined())
            {LOGMEIN("BackwardPass.cpp] 4299\n");
                // The object's type must not change in a way that changes the layout.
                // If we see a StFld with a type check bailout between here and the type check that guards this
                // property, we must not dead-store the StFld's type check bailout, even if that operand's type appears
                // dead, because that object may alias this one.
                BVSparse<JitArenaAllocator>* bv = block->typesNeedingKnownObjectLayout;
                if (bv == nullptr)
                {LOGMEIN("BackwardPass.cpp] 4306\n");
                    bv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
                    block->typesNeedingKnownObjectLayout = bv;
                }
                bv->Set(typeSymId);
            }
        }
        else
        {
            opnd->SetTypeDead(
                !block->upwardExposedFields->TestAndClear(typeSymId) &&
                (
                    // Don't set the type dead if this is a store that may change the layout in a way that invalidates
                    // optimized load/stores downstream. Leave it non-dead in that case so the type check bailout
                    // is preserved and so that Lower will generate the bailout properly.
                    !isStore ||
                    !block->typesNeedingKnownObjectLayout ||
                    block->typesNeedingKnownObjectLayout->IsEmpty()
                )
            );

            BVSparse<JitArenaAllocator>* bv = block->typesNeedingKnownObjectLayout;
            if (bv != nullptr)
            {LOGMEIN("BackwardPass.cpp] 4329\n");
                bv->Clear(typeSymId);
            }
        }

        bool mayNeedTypeTransition = true;
        if (!opnd->HasTypeMismatch() && func->DoGlobOpt())
        {LOGMEIN("BackwardPass.cpp] 4336\n");
            mayNeedTypeTransition = !isStore;
        }
        if (mayNeedTypeTransition &&
            !this->IsPrePass() &&
            !this->currentInstr->HasBailOutInfo() &&
            (opnd->NeedsPrimaryTypeCheck() ||
             opnd->NeedsLocalTypeCheck() ||
             opnd->NeedsLoadFromProtoTypeCheck()))
        {LOGMEIN("BackwardPass.cpp] 4345\n");
            // This is a "checked" opnd that nevertheless will have some kind of type check generated for it.
            // (Typical case is a load from prototype with no upstream guard.)
            // If the type check fails, we will call a helper, which will require that the type be correct here.
            // Final type can't be pushed up past this point. Do whatever type transition is required.
            if (block->stackSymToFinalType != nullptr)
            {LOGMEIN("BackwardPass.cpp] 4351\n");
                StackSym *baseSym = opnd->GetObjectSym();
                AddPropertyCacheBucket *pBucket = block->stackSymToFinalType->Get(baseSym->m_id);
                if (pBucket &&
                    pBucket->GetFinalType() != nullptr &&
                    pBucket->GetFinalType() != pBucket->GetInitialType())
                {LOGMEIN("BackwardPass.cpp] 4357\n");
                    this->InsertTypeTransition(this->currentInstr->m_next, baseSym, pBucket);
                    pBucket->SetFinalType(pBucket->GetInitialType());
                }
            }
        }
        if (!opnd->HasTypeMismatch() && func->DoGlobOpt())
        {
            // Do this after the above code, as the value of the final type may change there.
            TrackAddPropertyTypes(opnd, block);
        }

        TrackObjTypeSpecProperties(opnd, block);
        TrackObjTypeSpecWriteGuards(opnd, block);
    }
}

void
BackwardPass::TrackObjTypeSpecProperties(IR::PropertySymOpnd *opnd, BasicBlock *block)
{LOGMEIN("BackwardPass.cpp] 4376\n");
    Assert(tag == Js::DeadStorePhase);
    Assert(opnd->IsTypeCheckSeqCandidate());

    // Now that we're in the dead store pass and we know definitively which operations will have a type
    // check and which are protected by an upstream type check, we can push the lists of guarded properties
    // up the flow graph and drop them on the type checks for the corresponding object symbol.
    if (opnd->IsTypeCheckSeqParticipant())
    {LOGMEIN("BackwardPass.cpp] 4384\n");
        // Add this operation to the list of guarded operations for this object symbol.
        HashTable<ObjTypeGuardBucket>* stackSymToGuardedProperties = block->stackSymToGuardedProperties;
        if (stackSymToGuardedProperties == nullptr)
        {LOGMEIN("BackwardPass.cpp] 4388\n");
            stackSymToGuardedProperties = HashTable<ObjTypeGuardBucket>::New(this->tempAlloc, 8);
            block->stackSymToGuardedProperties = stackSymToGuardedProperties;
        }

        StackSym* objSym = opnd->GetObjectSym();
        ObjTypeGuardBucket* bucket = stackSymToGuardedProperties->FindOrInsertNew(objSym->m_id);
        BVSparse<JitArenaAllocator>* guardedPropertyOps = bucket->GetGuardedPropertyOps();
        if (guardedPropertyOps == nullptr)
        {LOGMEIN("BackwardPass.cpp] 4397\n");
            // The bit vectors we push around the flow graph only need to live as long as this phase.
            guardedPropertyOps = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            bucket->SetGuardedPropertyOps(guardedPropertyOps);
        }

#if DBG
        FOREACH_BITSET_IN_SPARSEBV(propOpId, guardedPropertyOps)
        {LOGMEIN("BackwardPass.cpp] 4405\n");
            JITObjTypeSpecFldInfo* existingFldInfo = this->func->GetGlobalObjTypeSpecFldInfo(propOpId);
            Assert(existingFldInfo != nullptr);

            if (existingFldInfo->GetPropertyId() != opnd->GetPropertyId())
            {LOGMEIN("BackwardPass.cpp] 4410\n");
                continue;
            }
            // It would be very nice to assert that the info we have for this property matches all properties guarded thus far.
            // Unfortunately, in some cases of object pointer copy propagation into a loop, we may end up with conflicting
            // information for the same property. We simply ignore the conflict and emit an equivalent type check, which
            // will attempt to check for one property on two different slots, and obviously fail. Thus we may have a
            // guaranteed bailout, but we'll simply re-JIT with equivalent object type spec disabled. To avoid this
            // issue altogether, we would need to track the set of guarded properties along with the type value in the
            // forward pass, and when a conflict is detected either not optimize the offending instruction, or correct
            // its information based on the info from the property in the type value info.
            //Assert(!existingFldInfo->IsPoly() || !opnd->IsPoly() || GlobOpt::AreTypeSetsIdentical(existingFldInfo->GetEquivalentTypeSet(), opnd->GetEquivalentTypeSet()));
            //Assert(existingFldInfo->GetSlotIndex() == opnd->GetSlotIndex());

            if (PHASE_TRACE(Js::EquivObjTypeSpecPhase, this->func) && !JITManager::GetJITManager()->IsJITServer())
            {LOGMEIN("BackwardPass.cpp] 4425\n");
                if (existingFldInfo->IsPoly() && opnd->IsPoly() &&
                    (!GlobOpt::AreTypeSetsIdentical(existingFldInfo->GetEquivalentTypeSet(), opnd->GetEquivalentTypeSet()) ||
                    (existingFldInfo->GetSlotIndex() != opnd->GetSlotIndex())))
                {LOGMEIN("BackwardPass.cpp] 4429\n");
                    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                    Output::Print(_u("EquivObjTypeSpec: top function %s (%s): duplicate property clash on %s(#%d) on operation %u \n"),
                        this->func->GetJITFunctionBody()->GetDisplayName(), this->func->GetDebugNumberSet(debugStringBuffer),
                        this->func->GetInProcThreadContext()->GetPropertyRecord(opnd->GetPropertyId())->GetBuffer(), opnd->GetPropertyId(), opnd->GetObjTypeSpecFldId());
                    Output::Flush();
                }
            }
        }
        NEXT_BITSET_IN_SPARSEBV
#endif

        bucket->AddToGuardedPropertyOps(opnd->GetObjTypeSpecFldId());
        if (opnd->NeedsMonoCheck())
        {LOGMEIN("BackwardPass.cpp] 4444\n");
            Assert(opnd->IsMono());
            JITTypeHolder monoGuardType = opnd->IsInitialTypeChecked() ? opnd->GetInitialType() : opnd->GetType();
            bucket->SetMonoGuardType(monoGuardType);
        }

        if (opnd->NeedsPrimaryTypeCheck())
        {LOGMEIN("BackwardPass.cpp] 4451\n");
            // Grab the guarded properties which match this type check with respect to polymorphism and drop them
            // on the operand. Only equivalent type checks can protect polymorphic properties to avoid a case where
            // we have 1) a cache with type set {t1, t2} and property a, followed by 2) a cache with type t3 and
            // property b, and 3) a cache with type set {t1, t2} and property c, where the slot index of property c
            // on t1 and t2 is different than on t3. If cache 2 were to protect property c it would not verify that
            // it resides on the correct slot for cache 3.  Yes, an equivalent type check could protect monomorphic
            // properties, but it would then unnecessarily verify their equivalence on the slow path.

            // Also, make sure the guarded properties on the operand are allocated from the func's allocator to
            // persists until lowering.

            Assert(guardedPropertyOps != nullptr);
            opnd->EnsureGuardedPropOps(this->func->m_alloc);
            opnd->AddGuardedPropOps(guardedPropertyOps);
            if (this->currentInstr->HasTypeCheckBailOut())
            {LOGMEIN("BackwardPass.cpp] 4467\n");
                // Stop pushing the mono guard type up if it is being checked here.
                if (bucket->NeedsMonoCheck())
                {LOGMEIN("BackwardPass.cpp] 4470\n");
                    if (this->currentInstr->HasEquivalentTypeCheckBailOut())
                    {LOGMEIN("BackwardPass.cpp] 4472\n");
                        // Some instr protected by this one requires a monomorphic type check. (E.g., final type opt,
                        // fixed field not loaded from prototype.) Note the IsTypeAvailable test above: only do this at
                        // the initial type check that protects this path.
                        opnd->SetMonoGuardType(bucket->GetMonoGuardType());
                        this->currentInstr->ChangeEquivalentToMonoTypeCheckBailOut();
                    }
                    bucket->SetMonoGuardType(nullptr);
                }
                
                if (!opnd->IsTypeAvailable())
                {LOGMEIN("BackwardPass.cpp] 4483\n");
                    // Stop tracking the guarded properties if there's not another type check upstream.
                    bucket->SetGuardedPropertyOps(nullptr);
                    JitAdelete(this->tempAlloc, guardedPropertyOps);
                    block->stackSymToGuardedProperties->Clear(objSym->m_id);
                }
            }
#if DBG
            {
                // If there is no upstream type check that is live and could protect guarded properties, we better
                // not have any properties remaining.
                ObjTypeGuardBucket* objTypeGuardBucket = block->stackSymToGuardedProperties->Get(opnd->GetObjectSym()->m_id);
                Assert(opnd->IsTypeAvailable() || objTypeGuardBucket == nullptr || objTypeGuardBucket->GetGuardedPropertyOps()->IsEmpty());
            }
#endif
        }
    }
    else if (opnd->NeedsLocalTypeCheck())
    {LOGMEIN("BackwardPass.cpp] 4501\n");
        opnd->EnsureGuardedPropOps(this->func->m_alloc);
        opnd->SetGuardedPropOp(opnd->GetObjTypeSpecFldId());
    }
}

void
BackwardPass::TrackObjTypeSpecWriteGuards(IR::PropertySymOpnd *opnd, BasicBlock *block)
{LOGMEIN("BackwardPass.cpp] 4509\n");
    // TODO (ObjTypeSpec): Move write guard tracking to the forward pass, by recording on the type value
    // which property IDs have been written since the last type check. This will result in more accurate
    // tracking in cases when object pointer copy prop kicks in.
    if (this->tag == Js::BackwardPhase)
    {LOGMEIN("BackwardPass.cpp] 4514\n");
        // If this operation may need a write guard (load from proto or fixed field check) then add its
        // write guard symbol to the map for this object. If it remains live (hasn't been written to)
        // until the type check upstream, it will get recorded there so that the type check can be registered
        // for invalidation on this property used in this operation.

        // (ObjTypeSpec): Consider supporting polymorphic write guards as well. We can't currently distinguish between mono and
        // poly write guards, and a type check can only protect operations matching with respect to polymorphism (see
        // BackwardPass::TrackObjTypeSpecProperties for details), so for now we only target monomorphic operations.
        if (opnd->IsMono() && opnd->MayNeedWriteGuardProtection())
        {LOGMEIN("BackwardPass.cpp] 4524\n");
            if (block->stackSymToWriteGuardsMap == nullptr)
            {LOGMEIN("BackwardPass.cpp] 4526\n");
                block->stackSymToWriteGuardsMap = HashTable<ObjWriteGuardBucket>::New(this->tempAlloc, 8);
            }

            ObjWriteGuardBucket* bucket = block->stackSymToWriteGuardsMap->FindOrInsertNew(opnd->GetObjectSym()->m_id);

            BVSparse<JitArenaAllocator>* writeGuards = bucket->GetWriteGuards();
            if (writeGuards == nullptr)
            {LOGMEIN("BackwardPass.cpp] 4534\n");
                // The bit vectors we push around the flow graph only need to live as long as this phase.
                writeGuards = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
                bucket->SetWriteGuards(writeGuards);
            }

            PropertySym *propertySym = opnd->m_sym->AsPropertySym();
            Assert(propertySym->m_writeGuardSym != nullptr);
            SymID writeGuardSymId = propertySym->m_writeGuardSym->m_id;
            writeGuards->Set(writeGuardSymId);
        }

        // Record any live (upward exposed) write guards on this operation, if this operation may end up with
        // a type check.  If we ultimately don't need a type check here, we will simply ignore the guards, because
        // an earlier type check will protect them.
        if (!IsPrePass() && opnd->IsMono() && !opnd->IsTypeDead())
        {LOGMEIN("BackwardPass.cpp] 4550\n");
            Assert(opnd->GetWriteGuards() == nullptr);
            if (block->stackSymToWriteGuardsMap != nullptr)
            {LOGMEIN("BackwardPass.cpp] 4553\n");
                ObjWriteGuardBucket* bucket = block->stackSymToWriteGuardsMap->Get(opnd->GetObjectSym()->m_id);
                if (bucket != nullptr)
                {LOGMEIN("BackwardPass.cpp] 4556\n");
                    // Get all the write guards associated with this object sym and filter them down to those that
                    // are upward exposed. If we end up emitting a type check for this instruction, we will create
                    // a type property guard registered for all guarded proto properties and we will set the write
                    // guard syms live during forward pass, such that we can avoid unnecessary write guard type
                    // checks and bailouts on every proto property (as long as it hasn't been written to since the
                    // primary type check).
                    auto writeGuards = bucket->GetWriteGuards()->CopyNew(this->func->m_alloc);
                    writeGuards->And(block->upwardExposedFields);
                    opnd->SetWriteGuards(writeGuards);
                }
            }
        }
    }
    else
    {
        // If we know this property has never been written to in this function (either on this object or any
        // of its aliases) we don't need the local type check.
        if (opnd->MayNeedWriteGuardProtection() && !opnd->IsWriteGuardChecked() && !MayPropertyBeWrittenTo(opnd->GetPropertyId()))
        {LOGMEIN("BackwardPass.cpp] 4575\n");
            opnd->SetWriteGuardChecked(true);
        }

        // If we don't need a primary type check here let's clear the write guards. The primary type check upstream will
        // register the type check for the corresponding properties.
        if (!IsPrePass() && !opnd->NeedsPrimaryTypeCheck())
        {LOGMEIN("BackwardPass.cpp] 4582\n");
            opnd->ClearWriteGuards();
        }
    }
}

void
BackwardPass::TrackAddPropertyTypes(IR::PropertySymOpnd *opnd, BasicBlock *block)
{LOGMEIN("BackwardPass.cpp] 4590\n");
    // Do the work of objtypespec add-property opt even if it's disabled by PHASE option, so that we have
    // the dataflow info that can be inspected.

    Assert(this->tag == Js::DeadStorePhase);
    Assert(opnd->IsMono() || opnd->HasEquivalentTypeSet());

    JITTypeHolder typeWithProperty = opnd->IsMono() ? opnd->GetType() : opnd->GetFirstEquivalentType();
    JITTypeHolder typeWithoutProperty = opnd->HasInitialType() ? opnd->GetInitialType() : JITTypeHolder(nullptr);

    if (typeWithoutProperty == nullptr ||
        typeWithProperty == typeWithoutProperty ||
        (opnd->IsTypeChecked() && !opnd->IsInitialTypeChecked()))
    {LOGMEIN("BackwardPass.cpp] 4603\n");
        if (!this->IsPrePass() && block->stackSymToFinalType != nullptr && !this->currentInstr->HasBailOutInfo())
        {LOGMEIN("BackwardPass.cpp] 4605\n");
            PropertySym *propertySym = opnd->m_sym->AsPropertySym();
            AddPropertyCacheBucket *pBucket =
                block->stackSymToFinalType->Get(propertySym->m_stackSym->m_id);
            if (pBucket && pBucket->GetFinalType() != nullptr && pBucket->GetInitialType() != pBucket->GetFinalType())
            {LOGMEIN("BackwardPass.cpp] 4610\n");
                opnd->SetFinalType(pBucket->GetFinalType());
            }
        }

        return;
    }

#if DBG
    Assert(typeWithProperty != nullptr);
    const JITTypeHandler * typeWithoutPropertyTypeHandler = typeWithoutProperty->GetTypeHandler();
    const JITTypeHandler * typeWithPropertyTypeHandler = typeWithProperty->GetTypeHandler();
    // TODO: OOP JIT, reenable assert
    //Assert(typeWithoutPropertyTypeHandler->GetPropertyCount() + 1 == typeWithPropertyTypeHandler->GetPropertyCount());
    AssertMsg(JITTypeHandler::IsTypeHandlerCompatibleForObjectHeaderInlining(typeWithoutPropertyTypeHandler, typeWithPropertyTypeHandler),
        "TypeHandlers are not compatible for transition?");
    Assert(typeWithoutPropertyTypeHandler->GetSlotCapacity() <= typeWithPropertyTypeHandler->GetSlotCapacity());
#endif

    // If there's already a final type for this instance, record it on the operand.
    // If not, start tracking it.
    if (block->stackSymToFinalType == nullptr)
    {LOGMEIN("BackwardPass.cpp] 4632\n");
        block->stackSymToFinalType = HashTable<AddPropertyCacheBucket>::New(this->tempAlloc, 8);
    }

    // Find or create the type-tracking record for this instance in this block.
    PropertySym *propertySym = opnd->m_sym->AsPropertySym();
    AddPropertyCacheBucket *pBucket =
        block->stackSymToFinalType->FindOrInsertNew(propertySym->m_stackSym->m_id);

    JITTypeHolder finalType(nullptr);
#if DBG
    JITTypeHolder deadStoreUnavailableFinalType(nullptr);
#endif
    if (pBucket->GetInitialType() == nullptr || opnd->GetType() != pBucket->GetInitialType())
    {LOGMEIN("BackwardPass.cpp] 4646\n");
#if DBG
        if (opnd->GetType() == pBucket->deadStoreUnavailableInitialType)
        {LOGMEIN("BackwardPass.cpp] 4649\n");
            deadStoreUnavailableFinalType = pBucket->deadStoreUnavailableFinalType;
        }
#endif
        // No info found, or the info was bad, so initialize it from this cache.
        finalType = opnd->GetType();
        pBucket->SetFinalType(finalType);
    }
    else
    {
        // Match: The type we push upward is now the typeWithoutProperty at this point,
        // and the final type is the one we've been tracking.
        finalType = pBucket->GetFinalType();
#if DBG
        deadStoreUnavailableFinalType = pBucket->deadStoreUnavailableFinalType;
#endif
    }

    pBucket->SetInitialType(typeWithoutProperty);

    if (!PHASE_OFF(Js::ObjTypeSpecStorePhase, this->func))
    {LOGMEIN("BackwardPass.cpp] 4670\n");
#if DBG

        // We may regress in this case:
        // if (b)
        //      t1 = {};
        //      o = t1;
        //      o.x =
        // else
        //      t2 = {};
        //      o = t2;
        //      o.x =
        // o.y =
        //
        // Where the backward pass will propagate the final type in o.y to o.x, then globopt will copy prop t1 and t2 to o.x.
        // But not o.y (because of the merge).  Then, in the dead store pass, o.y's final type will not propagate to t1.x and t2.x
        // respectively, thus regression the final type.  However, in both cases, the types of t1 and t2 are dead anyways.
        //
        // if the type is dead, we don't care if we have regressed the type, as no one is depending on it to skip type check anyways
        if (!opnd->IsTypeDead())
        {LOGMEIN("BackwardPass.cpp] 4690\n");
            // This is the type that would have been propagated if we didn't kill it because the type isn't available
            JITTypeHolder checkFinalType = deadStoreUnavailableFinalType != nullptr ? deadStoreUnavailableFinalType : finalType;
            if (opnd->HasFinalType() && opnd->GetFinalType() != checkFinalType)
            {LOGMEIN("BackwardPass.cpp] 4694\n");
                // Final type discovery must be progressively better (unless we kill it in the deadstore pass
                // when the type is not available during the forward pass)
                const JITTypeHandler * oldFinalTypeHandler = opnd->GetFinalType()->GetTypeHandler();
                const JITTypeHandler * checkFinalTypeHandler = checkFinalType->GetTypeHandler();

                // TODO: OOP JIT, enable assert
                //Assert(oldFinalTypeHandler->GetPropertyCount() < checkFinalTypeHandler->GetPropertyCount());
                AssertMsg(JITTypeHandler::IsTypeHandlerCompatibleForObjectHeaderInlining(oldFinalTypeHandler, checkFinalTypeHandler),
                    "TypeHandlers should be compatible for transition.");
                Assert(oldFinalTypeHandler->GetSlotCapacity() <= checkFinalTypeHandler->GetSlotCapacity());
            }
        }
#endif
        Assert(opnd->IsBeingAdded());
        if (!this->IsPrePass())
        {LOGMEIN("BackwardPass.cpp] 4710\n");
            opnd->SetFinalType(finalType);
        }
        if (!opnd->IsTypeChecked())
        {LOGMEIN("BackwardPass.cpp] 4714\n");
            // Transition from initial to final type will only happen at type check points.
            if (opnd->IsTypeAvailable())
            {LOGMEIN("BackwardPass.cpp] 4717\n");
                pBucket->SetFinalType(pBucket->GetInitialType());
            }
        }
    }

#if DBG_DUMP
    if (PHASE_TRACE(Js::ObjTypeSpecStorePhase, this->func))
    {LOGMEIN("BackwardPass.cpp] 4725\n");
        Output::Print(_u("ObjTypeSpecStore: "));
        this->currentInstr->Dump();
        pBucket->Dump();
    }
#endif

    // In the dead-store pass, we have forward information that tells us whether a "final type"
    // reached this point from an earlier store. If it didn't (i.e., it's not available here),
    // remove it from the backward map so that upstream stores will use the final type that is
    // live there. (This avoids unnecessary bailouts in cases where the final type is only live
    // on one branch of an "if", a case that the initial backward pass can't detect.)
    // An example:
    //  if (cond)
    //      o.x =
    //  o.y =

    if (!opnd->IsTypeAvailable())
    {LOGMEIN("BackwardPass.cpp] 4743\n");
#if DBG
        pBucket->deadStoreUnavailableInitialType = pBucket->GetInitialType();
        if (pBucket->deadStoreUnavailableFinalType == nullptr)
        {LOGMEIN("BackwardPass.cpp] 4747\n");
            pBucket->deadStoreUnavailableFinalType = pBucket->GetFinalType();
        }
        pBucket->SetInitialType(nullptr);
        pBucket->SetFinalType(nullptr);
#else
        block->stackSymToFinalType->Clear(propertySym->m_stackSym->m_id);
#endif
    }
}

void
BackwardPass::InsertTypeTransition(IR::Instr *instrInsertBefore, int symId, AddPropertyCacheBucket *data)
{LOGMEIN("BackwardPass.cpp] 4760\n");
    StackSym *objSym = this->func->m_symTable->FindStackSym(symId);
    Assert(objSym);
    this->InsertTypeTransition(instrInsertBefore, objSym, data);
}

void
BackwardPass::InsertTypeTransition(IR::Instr *instrInsertBefore, StackSym *objSym, AddPropertyCacheBucket *data)
{LOGMEIN("BackwardPass.cpp] 4768\n");
    IR::RegOpnd *baseOpnd = IR::RegOpnd::New(objSym, TyMachReg, this->func);
    baseOpnd->SetIsJITOptimizedReg(true);

    IR::AddrOpnd *initialTypeOpnd =
        IR::AddrOpnd::New(data->GetInitialType()->GetAddr(), IR::AddrOpndKindDynamicType, this->func);
    initialTypeOpnd->m_metadata = data->GetInitialType().t;

    IR::AddrOpnd *finalTypeOpnd =
        IR::AddrOpnd::New(data->GetFinalType()->GetAddr(), IR::AddrOpndKindDynamicType, this->func);
    finalTypeOpnd->m_metadata = data->GetFinalType().t;

    IR::Instr *adjustTypeInstr =
        IR::Instr::New(Js::OpCode::AdjustObjType, finalTypeOpnd, baseOpnd, initialTypeOpnd, this->func);

    instrInsertBefore->InsertBefore(adjustTypeInstr);
}

void
BackwardPass::InsertTypeTransitionAfterInstr(IR::Instr *instr, int symId, AddPropertyCacheBucket *data)
{LOGMEIN("BackwardPass.cpp] 4788\n");
    if (!this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 4790\n");
        // Transition to the final type if we don't bail out.
        if (instr->EndsBasicBlock())
        {LOGMEIN("BackwardPass.cpp] 4793\n");
            // The instr with the bailout is something like a branch that may not fall through.
            // Insert the transitions instead at the beginning of each successor block.
            this->InsertTypeTransitionsAtPriorSuccessors(this->currentBlock, nullptr, symId, data);
        }
        else
        {
            this->InsertTypeTransition(instr->m_next, symId, data);
        }
    }
    // Note: we could probably clear this entry out of the table, but I don't know
    // whether it's worth it, because it's likely coming right back.
    data->SetFinalType(data->GetInitialType());
}

void
BackwardPass::InsertTypeTransitionAtBlock(BasicBlock *block, int symId, AddPropertyCacheBucket *data)
{LOGMEIN("BackwardPass.cpp] 4810\n");
    bool inserted = false;
    FOREACH_INSTR_IN_BLOCK(instr, block)
    {LOGMEIN("BackwardPass.cpp] 4813\n");
        if (instr->IsRealInstr())
        {LOGMEIN("BackwardPass.cpp] 4815\n");
            // Check for pre-existing type transition. There may be more than one AdjustObjType here,
            // so look at them all.
            if (instr->m_opcode == Js::OpCode::AdjustObjType)
            {LOGMEIN("BackwardPass.cpp] 4819\n");
                if (instr->GetSrc1()->AsRegOpnd()->m_sym->m_id == (SymID)symId)
                {LOGMEIN("BackwardPass.cpp] 4821\n");
                    // This symbol already has a type transition at this point.
                    // It *must* be doing the same transition we're already trying to do.
                    Assert((intptr_t)instr->GetDst()->AsAddrOpnd()->m_address == data->GetFinalType()->GetAddr() &&
                           (intptr_t)instr->GetSrc2()->AsAddrOpnd()->m_address == data->GetInitialType()->GetAddr());
                    // Nothing to do.
                    return;
                }
            }
            else
            {
                this->InsertTypeTransition(instr, symId, data);
                inserted = true;
                break;
            }
        }
    }
    NEXT_INSTR_IN_BLOCK;
    if (!inserted)
    {LOGMEIN("BackwardPass.cpp] 4840\n");
        Assert(block->GetLastInstr()->m_next);
        this->InsertTypeTransition(block->GetLastInstr()->m_next, symId, data);
    }
}

void
BackwardPass::InsertTypeTransitionsAtPriorSuccessors(
    BasicBlock *block,
    BasicBlock *blockSucc,
    int symId,
    AddPropertyCacheBucket *data)
{
    // For each successor of block prior to blockSucc, adjust the type.
    FOREACH_SUCCESSOR_BLOCK(blockFix, block)
    {LOGMEIN("BackwardPass.cpp] 4855\n");
        if (blockFix == blockSucc)
        {LOGMEIN("BackwardPass.cpp] 4857\n");
            return;
        }

        this->InsertTypeTransitionAtBlock(blockFix, symId, data);
    }
    NEXT_SUCCESSOR_BLOCK;
}

void
BackwardPass::InsertTypeTransitionsAtPotentialKills()
{LOGMEIN("BackwardPass.cpp] 4868\n");
    // Final types can't be pushed up past certain instructions.
    IR::Instr *instr = this->currentInstr;

    if (instr->HasBailOutInfo() || instr->m_opcode == Js::OpCode::UpdateNewScObjectCache)
    {LOGMEIN("BackwardPass.cpp] 4873\n");
        // Final types can't be pushed up past a bailout point.
        // Insert any transitions called for by the current state of add-property buckets.
        // Also do this for ctor cache updates, to avoid putting a type in the ctor cache that extends past
        // the end of the ctor that the cache covers.
        this->ForEachAddPropertyCacheBucket([&](int symId, AddPropertyCacheBucket *data)->bool {
            this->InsertTypeTransitionAfterInstr(instr, symId, data);
            return false;
        });
    }
    else
    {
        // If this is a load/store that expects an object-header-inlined type, don't push another sym's transition from
        // object-header-inlined to non-object-header-inlined type past it, because the two syms may be aliases.
        IR::PropertySymOpnd *propertySymOpnd = instr->GetPropertySymOpnd();
        if (propertySymOpnd && propertySymOpnd->IsObjectHeaderInlined())
        {LOGMEIN("BackwardPass.cpp] 4889\n");
            SymID opndId = propertySymOpnd->m_sym->AsPropertySym()->m_stackSym->m_id;
            this->ForEachAddPropertyCacheBucket([&](int symId, AddPropertyCacheBucket *data)->bool {
                if ((SymID)symId == opndId)
                {LOGMEIN("BackwardPass.cpp] 4893\n");
                    // This is the sym we're tracking. No aliasing to worry about.
                    return false;
                }
                if (propertySymOpnd->IsMono() && data->GetInitialType() != propertySymOpnd->GetType())
                {LOGMEIN("BackwardPass.cpp] 4898\n");
                    // Type mismatch in a monomorphic case -- no aliasing.
                    return false;
                }
                if (this->TransitionUndoesObjectHeaderInlining(data))
                {LOGMEIN("BackwardPass.cpp] 4903\n");
                    // We're transitioning from inlined to non-inlined, so we can't push it up any farther.
                    this->InsertTypeTransitionAfterInstr(instr, symId, data);
                }
                return false;
            });
        }
    }
}

template<class Fn>
void
BackwardPass::ForEachAddPropertyCacheBucket(Fn fn)
{LOGMEIN("BackwardPass.cpp] 4916\n");
    BasicBlock *block = this->currentBlock;
    if (block->stackSymToFinalType == nullptr)
    {LOGMEIN("BackwardPass.cpp] 4919\n");
        return;
    }

    FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, block->stackSymToFinalType)
    {LOGMEIN("BackwardPass.cpp] 4924\n");
        AddPropertyCacheBucket *data = &bucket.element;
        if (data->GetInitialType() != nullptr &&
            data->GetInitialType() != data->GetFinalType())
        {LOGMEIN("BackwardPass.cpp] 4928\n");
            bool done = fn(bucket.value, data);
            if (done)
            {LOGMEIN("BackwardPass.cpp] 4931\n");
                break;
            }
        }
    }
    NEXT_HASHTABLE_ENTRY;
}

bool
BackwardPass::TransitionUndoesObjectHeaderInlining(AddPropertyCacheBucket *data) const
{LOGMEIN("BackwardPass.cpp] 4941\n");
    JITTypeHolder type = data->GetInitialType();
    if (type == nullptr || !Js::DynamicType::Is(type->GetTypeId()))
    {LOGMEIN("BackwardPass.cpp] 4944\n");
        return false;
    }

    if (!type->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler())
    {LOGMEIN("BackwardPass.cpp] 4949\n");
        return false;
    }

    type = data->GetFinalType();
    if (type == nullptr || !Js::DynamicType::Is(type->GetTypeId()))
    {LOGMEIN("BackwardPass.cpp] 4955\n");
        return false;
    }
    return !type->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler();
}

void
BackwardPass::CollectCloneStrCandidate(IR::Opnd * opnd)
{LOGMEIN("BackwardPass.cpp] 4963\n");
    IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
    Assert(regOpnd != nullptr);
    StackSym *sym = regOpnd->m_sym;

    if (tag == Js::BackwardPhase
        && currentInstr->m_opcode == Js::OpCode::Add_A
        && currentInstr->GetSrc1() == opnd
        && !this->IsPrePass()
        && !this->IsCollectionPass()
        &&  this->currentBlock->loop)
    {LOGMEIN("BackwardPass.cpp] 4974\n");
        Assert(currentBlock->cloneStrCandidates != nullptr);

        currentBlock->cloneStrCandidates->Set(sym->m_id);
    }
}

void
BackwardPass::InvalidateCloneStrCandidate(IR::Opnd * opnd)
{LOGMEIN("BackwardPass.cpp] 4983\n");
    IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
    Assert(regOpnd != nullptr);
    StackSym *sym = regOpnd->m_sym;

    if (tag == Js::BackwardPhase &&
        (currentInstr->m_opcode != Js::OpCode::Add_A || currentInstr->GetSrc1()->AsRegOpnd()->m_sym->m_id != sym->m_id) &&
        !this->IsPrePass() &&
        !this->IsCollectionPass() &&
        this->currentBlock->loop)
    {LOGMEIN("BackwardPass.cpp] 4993\n");
            currentBlock->cloneStrCandidates->Clear(sym->m_id);
    }
}

void
BackwardPass::ProcessUse(IR::Opnd * opnd)
{LOGMEIN("BackwardPass.cpp] 5000\n");
    switch (opnd->GetKind())
    {LOGMEIN("BackwardPass.cpp] 5002\n");
    case IR::OpndKindReg:
        {LOGMEIN("BackwardPass.cpp] 5004\n");
            IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
            StackSym *sym = regOpnd->m_sym;

            if (!IsCollectionPass())
            {LOGMEIN("BackwardPass.cpp] 5009\n");
                // isTempLastUse is only used for string concat right now, so lets not mark it if it's not a string.
                // If it's upward exposed, it is not it's last use.
                if (regOpnd->m_isTempLastUse && (regOpnd->GetValueType().IsNotString() || this->currentBlock->upwardExposedUses->Test(sym->m_id) || sym->m_mayNotBeTempLastUse))
                {LOGMEIN("BackwardPass.cpp] 5013\n");
                    regOpnd->m_isTempLastUse = false;
                }
                this->CollectCloneStrCandidate(opnd);
            }

            this->DoSetDead(regOpnd, !this->ProcessSymUse(sym, true, regOpnd->GetIsJITOptimizedReg()));

            if (IsCollectionPass())
            {LOGMEIN("BackwardPass.cpp] 5022\n");
                break;
            }

            if (tag == Js::DeadStorePhase && regOpnd->IsArrayRegOpnd())
            {
                ProcessArrayRegOpndUse(currentInstr, regOpnd->AsArrayRegOpnd());
            }

            if (currentInstr->m_opcode == Js::OpCode::BailOnNotArray)
            {LOGMEIN("BackwardPass.cpp] 5032\n");
                Assert(tag == Js::DeadStorePhase);

                const ValueType valueType(regOpnd->GetValueType());
                if(valueType.IsLikelyArrayOrObjectWithArray())
                {LOGMEIN("BackwardPass.cpp] 5037\n");
                    currentBlock->noImplicitCallUses->Clear(sym->m_id);

                    // We are being conservative here to always check for missing value
                    // if any of them expect no missing value. That is because we don't know
                    // what set of sym is equivalent (copied) from the one we are testing for right now.
                    if(valueType.HasNoMissingValues() &&
                        !currentBlock->noImplicitCallNoMissingValuesUses->IsEmpty() &&
                        !IsPrePass())
                    {LOGMEIN("BackwardPass.cpp] 5046\n");
                        // There is a use of this sym that requires this array to have no missing values, so this instruction
                        // needs to bail out if the array has missing values.
                        Assert(currentInstr->GetBailOutKind() == IR::BailOutOnNotArray ||
                               currentInstr->GetBailOutKind() == IR::BailOutOnNotNativeArray);
                        currentInstr->SetBailOutKind(currentInstr->GetBailOutKind() | IR::BailOutOnMissingValue);
                    }

                    currentBlock->noImplicitCallNoMissingValuesUses->Clear(sym->m_id);
                    currentBlock->noImplicitCallNativeArrayUses->Clear(sym->m_id);
                }
            }
        }
        break;
    case IR::OpndKindSym:
        {LOGMEIN("BackwardPass.cpp] 5061\n");
            IR::SymOpnd *symOpnd = opnd->AsSymOpnd();
            Sym * sym = symOpnd->m_sym;

            this->DoSetDead(symOpnd, !this->ProcessSymUse(sym, false, opnd->GetIsJITOptimizedReg()));

            if (IsCollectionPass())
            {LOGMEIN("BackwardPass.cpp] 5068\n");
                break;
            }

            if (sym->IsPropertySym())
            {LOGMEIN("BackwardPass.cpp] 5073\n");
                // TODO: We don't have last use info for property sym
                // and we don't set the last use of the stacksym inside the property sym
                if (tag == Js::BackwardPhase)
                {LOGMEIN("BackwardPass.cpp] 5077\n");
                    if (opnd->AsSymOpnd()->IsPropertySymOpnd())
                    {LOGMEIN("BackwardPass.cpp] 5079\n");
                        this->globOpt->PreparePropertySymOpndForTypeCheckSeq(symOpnd->AsPropertySymOpnd(), this->currentInstr, this->currentBlock->loop);
                    }
                }

                if (this->DoMarkTempNumbersOnTempObjects())
                {LOGMEIN("BackwardPass.cpp] 5085\n");
                    this->currentBlock->tempNumberTracker->ProcessPropertySymUse(symOpnd, this->currentInstr, this);
                }

                if (symOpnd->IsPropertySymOpnd())
                {LOGMEIN("BackwardPass.cpp] 5090\n");
                    this->ProcessPropertySymOpndUse(symOpnd->AsPropertySymOpnd());
                }
            }
        }
        break;
    case IR::OpndKindIndir:
        {LOGMEIN("BackwardPass.cpp] 5097\n");
            IR::IndirOpnd * indirOpnd = opnd->AsIndirOpnd();
            IR::RegOpnd * baseOpnd = indirOpnd->GetBaseOpnd();

            this->DoSetDead(baseOpnd, !this->ProcessSymUse(baseOpnd->m_sym, false, baseOpnd->GetIsJITOptimizedReg()));

            IR::RegOpnd * indexOpnd = indirOpnd->GetIndexOpnd();
            if (indexOpnd)
            {LOGMEIN("BackwardPass.cpp] 5105\n");
                this->DoSetDead(indexOpnd, !this->ProcessSymUse(indexOpnd->m_sym, false, indexOpnd->GetIsJITOptimizedReg()));
            }

            if(IsCollectionPass())
            {LOGMEIN("BackwardPass.cpp] 5110\n");
                break;
            }

            if (this->DoMarkTempNumbersOnTempObjects())
            {LOGMEIN("BackwardPass.cpp] 5115\n");
                this->currentBlock->tempNumberTracker->ProcessIndirUse(indirOpnd, currentInstr, this);
            }

            if(tag == Js::DeadStorePhase && baseOpnd->IsArrayRegOpnd())
            {
                ProcessArrayRegOpndUse(currentInstr, baseOpnd->AsArrayRegOpnd());
            }
        }
        break;
    }
}

bool
BackwardPass::ProcessPropertySymUse(PropertySym *propertySym)
{LOGMEIN("BackwardPass.cpp] 5130\n");
    Assert(this->tag == Js::BackwardPhase);

    BasicBlock *block = this->currentBlock;

    bool isLive = !!block->upwardExposedFields->TestAndSet(propertySym->m_id);

    if (propertySym->m_propertyEquivSet)
    {LOGMEIN("BackwardPass.cpp] 5138\n");
        block->upwardExposedFields->Or(propertySym->m_propertyEquivSet);
    }

    return isLive;
}

void
BackwardPass::MarkTemp(StackSym * sym)
{LOGMEIN("BackwardPass.cpp] 5147\n");
    Assert(!IsCollectionPass());
    // Don't care about type specialized syms
    if (!sym->IsVar())
    {LOGMEIN("BackwardPass.cpp] 5151\n");
        return;
    }

    BasicBlock * block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {LOGMEIN("BackwardPass.cpp] 5157\n");
        Assert((block->loop != nullptr) == block->tempNumberTracker->HasTempTransferDependencies());
        block->tempNumberTracker->MarkTemp(sym, this);
    }
    if (this->DoMarkTempObjects())
    {LOGMEIN("BackwardPass.cpp] 5162\n");
        Assert((block->loop != nullptr) == block->tempObjectTracker->HasTempTransferDependencies());
        block->tempObjectTracker->MarkTemp(sym, this);
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {LOGMEIN("BackwardPass.cpp] 5168\n");
        Assert((block->loop != nullptr) == block->tempObjectVerifyTracker->HasTempTransferDependencies());
        block->tempObjectVerifyTracker->MarkTemp(sym, this);
    }
#endif
}

void
BackwardPass::MarkTempProcessInstr(IR::Instr * instr)
{LOGMEIN("BackwardPass.cpp] 5177\n");
    Assert(!IsCollectionPass());

    if (this->currentBlock->isDead)
    {LOGMEIN("BackwardPass.cpp] 5181\n");
        return;
    }

    BasicBlock * block;
    block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {LOGMEIN("BackwardPass.cpp] 5188\n");
        block->tempNumberTracker->ProcessInstr(instr, this);
    }

    if (this->DoMarkTempObjects())
    {LOGMEIN("BackwardPass.cpp] 5193\n");
        block->tempObjectTracker->ProcessInstr(instr);
    }

#if DBG
    if (this->DoMarkTempObjectVerify())
    {LOGMEIN("BackwardPass.cpp] 5199\n");
        block->tempObjectVerifyTracker->ProcessInstr(instr, this);
    }
#endif
}

#if DBG_DUMP
void
BackwardPass::DumpMarkTemp()
{LOGMEIN("BackwardPass.cpp] 5208\n");
    Assert(!IsCollectionPass());

    BasicBlock * block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {LOGMEIN("BackwardPass.cpp] 5213\n");
        block->tempNumberTracker->Dump();
    }
    if (this->DoMarkTempObjects())
    {LOGMEIN("BackwardPass.cpp] 5217\n");
        block->tempObjectTracker->Dump();
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {LOGMEIN("BackwardPass.cpp] 5222\n");
        block->tempObjectVerifyTracker->Dump();
    }
#endif
}
#endif

void
BackwardPass::SetSymIsUsedOnlyInNumberIfLastUse(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 5231\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {LOGMEIN("BackwardPass.cpp] 5234\n");
        symUsedOnlyForNumberBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsNotUsedOnlyInNumber(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 5241\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym)
    {LOGMEIN("BackwardPass.cpp] 5244\n");
        symUsedOnlyForNumberBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsUsedOnlyInBitOpsIfLastUse(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 5251\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {LOGMEIN("BackwardPass.cpp] 5254\n");
        symUsedOnlyForBitOpsBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsNotUsedOnlyInBitOps(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 5261\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym)
    {LOGMEIN("BackwardPass.cpp] 5264\n");
        symUsedOnlyForBitOpsBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::TrackBitWiseOrNumberOp(IR::Instr *const instr)
{LOGMEIN("BackwardPass.cpp] 5271\n");
    Assert(instr);
    const bool trackBitWiseop = DoTrackBitOpsOrNumber();
    const bool trackNumberop = trackBitWiseop;
    const Js::OpCode opcode = instr->m_opcode;
    StackSym *const dstSym = IR::RegOpnd::TryGetStackSym(instr->GetDst());
    if (!trackBitWiseop && !trackNumberop)
    {LOGMEIN("BackwardPass.cpp] 5278\n");
        return;
    }

    if (!instr->IsRealInstr())
    {LOGMEIN("BackwardPass.cpp] 5283\n");
        return;
    }

    if (dstSym)
    {LOGMEIN("BackwardPass.cpp] 5288\n");
        // For a dst where the def is in this block, transfer the current info into the instruction
        if (trackBitWiseop && symUsedOnlyForBitOpsBySymId->TestAndClear(dstSym->m_id))
        {LOGMEIN("BackwardPass.cpp] 5291\n");
            instr->dstIsAlwaysConvertedToInt32 = true;
        }
        if (trackNumberop && symUsedOnlyForNumberBySymId->TestAndClear(dstSym->m_id))
        {LOGMEIN("BackwardPass.cpp] 5295\n");
            instr->dstIsAlwaysConvertedToNumber = true;
        }
    }

    // If the instruction can cause src values to escape the local scope, the srcs can't be optimized
    if (OpCodeAttr::NonTempNumberSources(opcode))
    {LOGMEIN("BackwardPass.cpp] 5302\n");
        if (trackBitWiseop)
        {LOGMEIN("BackwardPass.cpp] 5304\n");
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc1());
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc2());
        }
        if (trackNumberop)
        {LOGMEIN("BackwardPass.cpp] 5309\n");
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc1());
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc2());
        }
        return;
    }

    if (trackBitWiseop)
    {LOGMEIN("BackwardPass.cpp] 5317\n");
        switch (opcode)
        {LOGMEIN("BackwardPass.cpp] 5319\n");
            // Instructions that can cause src values to escape the local scope have already been excluded

        case Js::OpCode::Not_A:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:

        case Js::OpCode::Not_I4:
        case Js::OpCode::And_I4:
        case Js::OpCode::Or_I4:
        case Js::OpCode::Xor_I4:
        case Js::OpCode::Shl_I4:
        case Js::OpCode::Shr_I4:
            // These instructions don't generate -0, and their behavior is the same for any src that is -0 or +0
            SetSymIsUsedOnlyInBitOpsIfLastUse(instr->GetSrc1());
            SetSymIsUsedOnlyInBitOpsIfLastUse(instr->GetSrc2());
            break;
        default:
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc1());
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc2());
            break;
        }
    }

    if (trackNumberop)
    {LOGMEIN("BackwardPass.cpp] 5347\n");
        switch (opcode)
        {LOGMEIN("BackwardPass.cpp] 5349\n");
            // Instructions that can cause src values to escape the local scope have already been excluded

        case Js::OpCode::Conv_Num:
        case Js::OpCode::Div_A:
        case Js::OpCode::Mul_A:
        case Js::OpCode::Sub_A:
        case Js::OpCode::Rem_A:
        case Js::OpCode::Incr_A:
        case Js::OpCode::Decr_A:
        case Js::OpCode::Neg_A:
        case Js::OpCode::Not_A:
        case Js::OpCode::ShrU_A:
        case Js::OpCode::ShrU_I4:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:
            // These instructions don't generate -0, and their behavior is the same for any src that is -0 or +0
            SetSymIsUsedOnlyInNumberIfLastUse(instr->GetSrc1());
            SetSymIsUsedOnlyInNumberIfLastUse(instr->GetSrc2());
            break;
        default:
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc1());
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc2());
            break;
        }
    }
}

void
BackwardPass::RemoveNegativeZeroBailout(IR::Instr* instr)
{LOGMEIN("BackwardPass.cpp] 5382\n");
    Assert(instr->HasBailOutInfo() && (instr->GetBailOutKind() & IR::BailOutOnNegativeZero));
    IR::BailOutKind bailOutKind = instr->GetBailOutKind();
    bailOutKind = bailOutKind & ~IR::BailOutOnNegativeZero;
    if (bailOutKind)
    {LOGMEIN("BackwardPass.cpp] 5387\n");
        instr->SetBailOutKind(bailOutKind);
    }
    else
    {
        instr->ClearBailOutInfo();
        if (preOpBailOutInstrToProcess == instr)
        {LOGMEIN("BackwardPass.cpp] 5394\n");
            preOpBailOutInstrToProcess = nullptr;
        }
    }
}

void
BackwardPass::TrackIntUsage(IR::Instr *const instr)
{LOGMEIN("BackwardPass.cpp] 5402\n");
    Assert(instr);

    const bool trackNegativeZero = DoTrackNegativeZero();
    const bool trackIntOverflow = DoTrackIntOverflow();
    const bool trackCompoundedIntOverflow = DoTrackCompoundedIntOverflow();
    const bool trackNon32BitOverflow = DoTrackNon32BitOverflow();

    if(!(trackNegativeZero || trackIntOverflow || trackCompoundedIntOverflow))
    {LOGMEIN("BackwardPass.cpp] 5411\n");
        return;
    }

    const Js::OpCode opcode = instr->m_opcode;
    if(trackCompoundedIntOverflow && opcode == Js::OpCode::StatementBoundary && instr->AsPragmaInstr()->m_statementIndex == 0)
    {LOGMEIN("BackwardPass.cpp] 5417\n");
        // Cannot bail out before the first statement boundary, so the range cannot extend beyond this instruction
        Assert(!instr->ignoreIntOverflowInRange);
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(!instr->IsRealInstr())
    {LOGMEIN("BackwardPass.cpp] 5425\n");
        return;
    }

    StackSym *const dstSym = IR::RegOpnd::TryGetStackSym(instr->GetDst());
    bool ignoreIntOverflowCandidate = false;
    if(dstSym)
    {LOGMEIN("BackwardPass.cpp] 5432\n");
        // For a dst where the def is in this block, transfer the current info into the instruction
        if(trackNegativeZero)
        {LOGMEIN("BackwardPass.cpp] 5435\n");
            if (negativeZeroDoesNotMatterBySymId->Test(dstSym->m_id))
            {LOGMEIN("BackwardPass.cpp] 5437\n");
                instr->ignoreNegativeZero = true;
            }

            if (tag == Js::DeadStorePhase)
            {LOGMEIN("BackwardPass.cpp] 5442\n");
                if (negativeZeroDoesNotMatterBySymId->TestAndClear(dstSym->m_id))
                {LOGMEIN("BackwardPass.cpp] 5444\n");
                    if (instr->HasBailOutInfo())
                    {LOGMEIN("BackwardPass.cpp] 5446\n");
                        IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                        if (bailOutKind & IR::BailOutOnNegativeZero)
                        {LOGMEIN("BackwardPass.cpp] 5449\n");
                            RemoveNegativeZeroBailout(instr);
                        }
                    }
                }
                else
                {
                    if (instr->HasBailOutInfo())
                    {LOGMEIN("BackwardPass.cpp] 5457\n");
                        if (instr->GetBailOutKind() & IR::BailOutOnNegativeZero)
                        {LOGMEIN("BackwardPass.cpp] 5459\n");
                            if (this->currentBlock->couldRemoveNegZeroBailoutForDef->TestAndClear(dstSym->m_id))
                            {LOGMEIN("BackwardPass.cpp] 5461\n");
                                RemoveNegativeZeroBailout(instr);
                            }
                        }
                        // This instruction could potentially bail out. Hence, we cannot reliably remove negative zero
                        // bailouts upstream. If we did, and the operation actually produced a -0, and this instruction
                        // bailed out, we'd use +0 instead of -0 in the interpreter.
                        this->currentBlock->couldRemoveNegZeroBailoutForDef->ClearAll();
                    }
                }
            }
            else
            {
                this->negativeZeroDoesNotMatterBySymId->Clear(dstSym->m_id);
            }
        }
        if(trackIntOverflow)
        {LOGMEIN("BackwardPass.cpp] 5478\n");
            ignoreIntOverflowCandidate = !!intOverflowDoesNotMatterBySymId->TestAndClear(dstSym->m_id);
            if(trackCompoundedIntOverflow)
            {LOGMEIN("BackwardPass.cpp] 5481\n");
                instr->ignoreIntOverflowInRange = !!intOverflowDoesNotMatterInRangeBySymId->TestAndClear(dstSym->m_id);
            }
        }
    }

    // If the instruction can cause src values to escape the local scope, the srcs can't be optimized
    if(OpCodeAttr::NonTempNumberSources(opcode))
    {LOGMEIN("BackwardPass.cpp] 5489\n");
        if(trackNegativeZero)
        {LOGMEIN("BackwardPass.cpp] 5491\n");
            SetNegativeZeroMatters(instr->GetSrc1());
            SetNegativeZeroMatters(instr->GetSrc2());
        }
        if(trackIntOverflow)
        {LOGMEIN("BackwardPass.cpp] 5496\n");
            SetIntOverflowMatters(instr->GetSrc1());
            SetIntOverflowMatters(instr->GetSrc2());
            if(trackCompoundedIntOverflow)
            {LOGMEIN("BackwardPass.cpp] 5500\n");
                instr->ignoreIntOverflowInRange = false;
                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                EndIntOverflowDoesNotMatterRange();
            }
        }
        return;
    }

    // -0 tracking

    if(trackNegativeZero)
    {LOGMEIN("BackwardPass.cpp] 5513\n");
        switch(opcode)
        {LOGMEIN("BackwardPass.cpp] 5515\n");
            // Instructions that can cause src values to escape the local scope have already been excluded

            case Js::OpCode::FromVar:
            case Js::OpCode::Conv_Prim:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(!instr->GetSrc2());

                if(instr->GetDst()->IsInt32())
                {LOGMEIN("BackwardPass.cpp] 5525\n");
                    // Conversion to int32 that is either explicit, or has a bailout check ensuring that it's an int value
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    break;
                }
                // fall-through

            default:
                if(dstSym && !instr->ignoreNegativeZero)
                {LOGMEIN("BackwardPass.cpp] 5534\n");
                    // -0 matters for dst, so -0 also matters for srcs
                    SetNegativeZeroMatters(instr->GetSrc1());
                    SetNegativeZeroMatters(instr->GetSrc2());
                    break;
                }
                if(opcode == Js::OpCode::Div_A || opcode == Js::OpCode::Div_I4)
                {LOGMEIN("BackwardPass.cpp] 5541\n");
                    // src1 is being divided by src2, so -0 matters for src2
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroMatters(instr->GetSrc2());
                    break;
                }
                // fall-through

            case Js::OpCode::Incr_A:
            case Js::OpCode::Decr_A:
                // Adding 1 to something or subtracting 1 from something does not generate -0

            case Js::OpCode::Not_A:
            case Js::OpCode::And_A:
            case Js::OpCode::Or_A:
            case Js::OpCode::Xor_A:
            case Js::OpCode::Shl_A:
            case Js::OpCode::Shr_A:
            case Js::OpCode::ShrU_A:

            case Js::OpCode::Not_I4:
            case Js::OpCode::And_I4:
            case Js::OpCode::Or_I4:
            case Js::OpCode::Xor_I4:
            case Js::OpCode::Shl_I4:
            case Js::OpCode::Shr_I4:
            case Js::OpCode::ShrU_I4:

            case Js::OpCode::Conv_Str:
            case Js::OpCode::Coerce_Str:
            case Js::OpCode::Coerce_Regex:
            case Js::OpCode::Coerce_StrOrRegex:
            case Js::OpCode::Conv_PrimStr:

            case Js::OpCode::Add_Ptr:
                // These instructions don't generate -0, and their behavior is the same for any src that is -0 or +0
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                break;

            case Js::OpCode::Add_I4:
            {LOGMEIN("BackwardPass.cpp] 5582\n");
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsIntConstOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsIntConstOpnd());

                if (instr->ignoreNegativeZero ||
                    (instr->GetSrc1()->IsIntConstOpnd() && instr->GetSrc1()->AsIntConstOpnd()->GetValue() != 0) ||
                    (instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() != 0))
                {LOGMEIN("BackwardPass.cpp] 5592\n");
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    break;
                }
                
                // -0 + -0 == -0. As long as one src is guaranteed to not be -0, -0 does not matter for the other src. Pick a
                // src for which to ignore negative zero, based on which sym is last-use. If both syms are last-use, src2 is
                // picked arbitrarily.
                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroMatters(instr->GetSrc2());
                if (tag == Js::DeadStorePhase)
                {LOGMEIN("BackwardPass.cpp] 5604\n");
                    if (instr->GetSrc2()->IsRegOpnd() &&
                        !currentBlock->upwardExposedUses->Test(instr->GetSrc2()->AsRegOpnd()->m_sym->m_id))
                    {LOGMEIN("BackwardPass.cpp] 5607\n");
                        SetCouldRemoveNegZeroBailoutForDefIfLastUse(instr->GetSrc2());
                    }
                    else
                    {
                        SetCouldRemoveNegZeroBailoutForDefIfLastUse(instr->GetSrc1());
                    }
                }
                break;
            }

            case Js::OpCode::Add_A:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsAddrOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsAddrOpnd());

                if(instr->ignoreNegativeZero || instr->GetSrc1()->IsAddrOpnd() || instr->GetSrc2()->IsAddrOpnd())
                {LOGMEIN("BackwardPass.cpp] 5626\n");
                    // -0 does not matter for dst, or this instruction does not generate -0 since one of the srcs is not -0
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    break;
                }

                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroMatters(instr->GetSrc2());
                break;

            case Js::OpCode::Sub_I4:
            {LOGMEIN("BackwardPass.cpp] 5638\n");
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsIntConstOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsIntConstOpnd());

                if (instr->ignoreNegativeZero ||
                    (instr->GetSrc1()->IsIntConstOpnd() && instr->GetSrc1()->AsIntConstOpnd()->GetValue() != 0) ||
                    (instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() != 0))
                {LOGMEIN("BackwardPass.cpp] 5648\n");
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                }
                else
                {
                    goto NegativeZero_Sub_Default;
                }
                break;
            }
            case Js::OpCode::Sub_A:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsAddrOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsAddrOpnd() || instr->GetSrc2()->IsIntConstOpnd());

                if(instr->ignoreNegativeZero ||
                    instr->GetSrc1()->IsAddrOpnd() ||
                    (
                        instr->GetSrc2()->IsAddrOpnd() &&
                        instr->GetSrc2()->AsAddrOpnd()->IsVar() &&
                        Js::TaggedInt::ToInt32(instr->GetSrc2()->AsAddrOpnd()->m_address) != 0
                    ))
                {LOGMEIN("BackwardPass.cpp] 5672\n");
                    // At least one of the following is true:
                    //     - -0 does not matter for dst
                    //     - Src1 is not -0, and so this instruction cannot generate -0
                    //     - Src2 is a nonzero tagged int constant, and so this instruction cannot generate -0
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    break;
                }
                // fall-through

            NegativeZero_Sub_Default:
                // -0 - 0 == -0. As long as src1 is guaranteed to not be -0, -0 does not matter for src2.
                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroMatters(instr->GetSrc2());
                if (this->tag == Js::DeadStorePhase)
                {LOGMEIN("BackwardPass.cpp] 5688\n");
                    SetCouldRemoveNegZeroBailoutForDefIfLastUse(instr->GetSrc2());
                }
                break;

            case Js::OpCode::BrEq_I4:
            case Js::OpCode::BrTrue_I4:
            case Js::OpCode::BrFalse_I4:
            case Js::OpCode::BrGe_I4:
            case Js::OpCode::BrUnGe_I4:
            case Js::OpCode::BrGt_I4:
            case Js::OpCode::BrUnGt_I4:
            case Js::OpCode::BrLt_I4:
            case Js::OpCode::BrUnLt_I4:
            case Js::OpCode::BrLe_I4:
            case Js::OpCode::BrUnLe_I4:
            case Js::OpCode::BrNeq_I4:
                // Int-specialized branches may prove that one of the src must be zero purely based on the int range, in which
                // case they rely on prior -0 bailouts to guarantee that the src cannot be -0. So, consider that -0 matters for
                // the srcs.

                // fall-through

            case Js::OpCode::InlineMathAtan2:
                // Atan(y,x) - signs of y, x is used to determine the quadrant of the result
                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroMatters(instr->GetSrc2());
                break;

            case Js::OpCode::Expo_A:
            case Js::OpCode::InlineMathPow:
                // Negative zero matters for src1
                //   Pow( 0, <neg>) is  Infinity
                //   Pow(-0, <neg>) is -Infinity
                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                break;

            case Js::OpCode::LdElemI_A:
                // There is an implicit ToString on the index operand, which doesn't differentiate -0 from +0
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd());
                break;

            case Js::OpCode::StElemI_A:
            case Js::OpCode::StElemI_A_Strict:
                // There is an implicit ToString on the index operand, which doesn't differentiate -0 from +0
                SetNegativeZeroDoesNotMatterIfLastUse(instr->GetDst()->AsIndirOpnd()->GetIndexOpnd());
                break;
        }
    }

    // Int overflow tracking

    if(!trackIntOverflow)
    {LOGMEIN("BackwardPass.cpp] 5742\n");
        return;
    }

    switch(opcode)
    {LOGMEIN("BackwardPass.cpp] 5747\n");
        // Instructions that can cause src values to escape the local scope have already been excluded

        default:
            // Unlike the -0 tracking, we use an inclusion list of op-codes for overflow tracking rather than an exclusion list.
            // Assume for any instructions other than those listed above, that int-overflowed values in the srcs are
            // insufficient.
            ignoreIntOverflowCandidate = false;
            // fall-through
        case Js::OpCode::Incr_A:
        case Js::OpCode::Decr_A:
        case Js::OpCode::Add_A:
        case Js::OpCode::Sub_A:
            // The sources are not guaranteed to be converted to int32. Let the compounded int overflow tracking handle this.
            SetIntOverflowMatters(instr->GetSrc1());
            SetIntOverflowMatters(instr->GetSrc2());
            break;

        case Js::OpCode::Mul_A:
            if (trackNon32BitOverflow)
            {LOGMEIN("BackwardPass.cpp] 5767\n");
                if (ignoreIntOverflowCandidate)
                    instr->ignoreOverflowBitCount = 53;
            }
            else
            {
                ignoreIntOverflowCandidate = false;
            }
            SetIntOverflowMatters(instr->GetSrc1());
            SetIntOverflowMatters(instr->GetSrc2());
            break;

        case Js::OpCode::Neg_A:
        case Js::OpCode::Ld_A:
        case Js::OpCode::Conv_Num:
        case Js::OpCode::ShrU_A:
            if(!ignoreIntOverflowCandidate)
            {LOGMEIN("BackwardPass.cpp] 5784\n");
                // Int overflow matters for dst, so int overflow also matters for srcs
                SetIntOverflowMatters(instr->GetSrc1());
                SetIntOverflowMatters(instr->GetSrc2());
                break;
            }
            // fall-through

        case Js::OpCode::Not_A:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:
            // These instructions convert their srcs to int32s, and hence don't care about int-overflowed values in the srcs (as
            // long as the overflowed values did not overflow the 53 bits that 'double' values have to precisely represent
            // ints). ShrU_A is not included here because it converts its srcs to uint32 rather than int32, so it would make a
            // difference if the srcs have int32-overflowed values.
            SetIntOverflowDoesNotMatterIfLastUse(instr->GetSrc1());
            SetIntOverflowDoesNotMatterIfLastUse(instr->GetSrc2());
            break;
    }

    if(ignoreIntOverflowCandidate)
    {LOGMEIN("BackwardPass.cpp] 5808\n");
        instr->ignoreIntOverflow = true;
    }

    // Compounded int overflow tracking

    if(!trackCompoundedIntOverflow)
    {LOGMEIN("BackwardPass.cpp] 5815\n");
        return;
    }

    if(instr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset)
    {LOGMEIN("BackwardPass.cpp] 5820\n");
        // The forward pass may need to insert conversions with bailouts before the first instruction in the range. Since this
        // instruction does not have a valid byte code offset for bailout purposes, end the current range now.
        instr->ignoreIntOverflowInRange = false;
        SetIntOverflowMattersInRange(instr->GetSrc1());
        SetIntOverflowMattersInRange(instr->GetSrc2());
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(ignoreIntOverflowCandidate)
    {LOGMEIN("BackwardPass.cpp] 5831\n");
        instr->ignoreIntOverflowInRange = true;
        if(dstSym)
        {LOGMEIN("BackwardPass.cpp] 5834\n");
            dstSym->scratch.globOpt.numCompoundedAddSubUses = 0;
        }
    }

    bool lossy = false;
    switch(opcode)
    {LOGMEIN("BackwardPass.cpp] 5841\n");
        // Instructions that can cause src values to escape the local scope have already been excluded

        case Js::OpCode::Incr_A:
        case Js::OpCode::Decr_A:
        case Js::OpCode::Add_A:
        case Js::OpCode::Sub_A:
        {LOGMEIN("BackwardPass.cpp] 5848\n");
            if(!instr->ignoreIntOverflowInRange)
            {LOGMEIN("BackwardPass.cpp] 5850\n");
                // Int overflow matters for dst, so int overflow also matters for srcs
                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                break;
            }
            AnalysisAssert(dstSym);

            // The number of compounded add/sub uses of each src is at least the number of compounded add/sub uses of the dst,
            // + 1 for the current instruction
            Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses >= 0);
            Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);
            const int addSubUses = dstSym->scratch.globOpt.numCompoundedAddSubUses + 1;
            if(addSubUses > MaxCompoundedUsesInAddSubForIgnoringIntOverflow)
            {LOGMEIN("BackwardPass.cpp] 5864\n");
                // There are too many compounded add/sub uses of the srcs. There is a possibility that combined, the number
                // eventually overflows the 53 bits that 'double' values have to precisely represent ints
                instr->ignoreIntOverflowInRange = false;
                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                break;
            }

            TransferCompoundedAddSubUsesToSrcs(instr, addSubUses);
            break;
        }

        case Js::OpCode::Neg_A:
        case Js::OpCode::Ld_A:
        case Js::OpCode::Conv_Num:
        case Js::OpCode::ShrU_A:
        {LOGMEIN("BackwardPass.cpp] 5881\n");
            if(!instr->ignoreIntOverflowInRange)
            {LOGMEIN("BackwardPass.cpp] 5883\n");
                // Int overflow matters for dst, so int overflow also matters for srcs
                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                break;
            }
            AnalysisAssert(dstSym);

            TransferCompoundedAddSubUsesToSrcs(instr, dstSym->scratch.globOpt.numCompoundedAddSubUses);
            lossy = opcode == Js::OpCode::ShrU_A;
            break;
        }

        case Js::OpCode::Not_A:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:
            // These instructions convert their srcs to int32s, and hence don't care about int-overflowed values in the srcs (as
            // long as the overflowed values did not overflow the 53 bits that 'double' values have to precisely represent
            // ints). ShrU_A is not included here because it converts its srcs to uint32 rather than int32, so it would make a
            // difference if the srcs have int32-overflowed values.
            instr->ignoreIntOverflowInRange = true;
            lossy = true;
            SetIntOverflowDoesNotMatterInRangeIfLastUse(instr->GetSrc1(), 0);
            SetIntOverflowDoesNotMatterInRangeIfLastUse(instr->GetSrc2(), 0);
            break;

        case Js::OpCode::LdSlotArr:
        case Js::OpCode::LdSlot:
        {LOGMEIN("BackwardPass.cpp] 5914\n");
            Assert(dstSym);
            Assert(!instr->GetSrc2()); // at the moment, this list contains only unary operations

            if(intOverflowCurrentlyMattersInRange)
            {LOGMEIN("BackwardPass.cpp] 5919\n");
                // These instructions will not begin a range, so just return. They don't begin a range because their initial
                // value may not be available until after the instruction is processed in the forward pass.
                Assert(!instr->ignoreIntOverflowInRange);
                return;
            }
            Assert(currentBlock->intOverflowDoesNotMatterRange);

            // Int overflow does not matter for dst, so the srcs need to be tracked as inputs into the region of
            // instructions where int overflow does not matter. Since these instructions will not begin or end a range, they
            // are tracked in separate candidates bit-vectors and once we have confirmed that they don't begin the range,
            // they will be transferred to 'SymsRequiredToBe[Lossy]Int'. Furthermore, once this instruction is included in
            // the range, its dst sym has to be removed. Since this instructions may not be included in the range, add the
            // dst sym to the candidates bit-vectors. If they are included, the process of transferring will remove the dst
            // syms and add the src syms.

            // Remove the dst using the candidate bit-vectors
            Assert(
                !instr->ignoreIntOverflowInRange ||
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Test(dstSym->m_id));
            if(instr->ignoreIntOverflowInRange ||
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Test(dstSym->m_id))
            {LOGMEIN("BackwardPass.cpp] 5941\n");
                candidateSymsRequiredToBeInt->Set(dstSym->m_id);
                if(currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Test(dstSym->m_id))
                {LOGMEIN("BackwardPass.cpp] 5944\n");
                    candidateSymsRequiredToBeLossyInt->Set(dstSym->m_id);
                }
            }

            if(!instr->ignoreIntOverflowInRange)
            {LOGMEIN("BackwardPass.cpp] 5950\n");
                // These instructions will not end a range, so just return. They may be included in the middle of a range, but
                // since int overflow matters for the dst, the src does not need to be counted as an input into the range.
                return;
            }
            instr->ignoreIntOverflowInRange = false;

            // Add the src using the candidate bit-vectors. The src property sym may already be included in the range or as
            // a candidate. The xor of the final bit-vector with the candidate is the set of syms required to be int,
            // assuming all instructions up to and not including this one are included in the range.
            const SymID srcSymId = instr->GetSrc1()->AsSymOpnd()->m_sym->m_id;
            const bool srcIncluded =
                !!currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Test(srcSymId) ^
                !!candidateSymsRequiredToBeInt->Test(srcSymId);
            const bool srcIncludedAsLossy =
                srcIncluded &&
                !!currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Test(srcSymId) ^
                !!candidateSymsRequiredToBeLossyInt->Test(srcSymId);
            const bool srcNeedsToBeLossless =
                !currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Test(dstSym->m_id) ||
                (srcIncluded && !srcIncludedAsLossy);
            if(srcIncluded)
            {LOGMEIN("BackwardPass.cpp] 5972\n");
                if(srcIncludedAsLossy && srcNeedsToBeLossless)
                {LOGMEIN("BackwardPass.cpp] 5974\n");
                    candidateSymsRequiredToBeLossyInt->Compliment(srcSymId);
                }
            }
            else
            {
                candidateSymsRequiredToBeInt->Compliment(srcSymId);
                if(!srcNeedsToBeLossless)
                {LOGMEIN("BackwardPass.cpp] 5982\n");
                    candidateSymsRequiredToBeLossyInt->Compliment(srcSymId);
                }
            }

            // These instructions will not end a range, so just return. They may be included in the middle of a range, and the
            // src has been included as a candidate input into the range.
            return;
        }

        case Js::OpCode::Mul_A:
            if (trackNon32BitOverflow)
            {LOGMEIN("BackwardPass.cpp] 5994\n");
                // MULs will always be at the start of a range. Either included in the range if int32 overflow is ignored, or excluded if int32 overflow matters. Even if int32 can be ignored, MULs can still bailout on 53-bit.
                // That's why it cannot be in the middle of a range.
                if (instr->ignoreIntOverflowInRange)
                {LOGMEIN("BackwardPass.cpp] 5998\n");
                    AnalysisAssert(dstSym);
                    Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses >= 0);
                    Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);
                    instr->ignoreOverflowBitCount = (uint8) (53 - dstSym->scratch.globOpt.numCompoundedAddSubUses);

                    // We have the max number of compounded adds/subs. 32-bit overflow cannot be ignored.
                    if (instr->ignoreOverflowBitCount == 32)
                    {LOGMEIN("BackwardPass.cpp] 6006\n");
                        instr->ignoreIntOverflowInRange = false;
                    }
                }

                SetIntOverflowMattersInRange(instr->GetSrc1());
                SetIntOverflowMattersInRange(instr->GetSrc2());
                break;
            }
            // fall-through

        default:
            // Unlike the -0 tracking, we use an inclusion list of op-codes for overflow tracking rather than an exclusion list.
            // Assume for any instructions other than those listed above, that int-overflowed values in the srcs are
            // insufficient.
            instr->ignoreIntOverflowInRange = false;
            SetIntOverflowMattersInRange(instr->GetSrc1());
            SetIntOverflowMattersInRange(instr->GetSrc2());
            break;
    }

    if(!instr->ignoreIntOverflowInRange)
    {LOGMEIN("BackwardPass.cpp] 6028\n");
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(intOverflowCurrentlyMattersInRange)
    {LOGMEIN("BackwardPass.cpp] 6034\n");
        // This is the last instruction in a new range of instructions where int overflow does not matter
        intOverflowCurrentlyMattersInRange = false;
        IR::Instr *const boundaryInstr = IR::PragmaInstr::New(Js::OpCode::NoIntOverflowBoundary, 0, instr->m_func);
        boundaryInstr->SetByteCodeOffset(instr);
        currentBlock->InsertInstrAfter(boundaryInstr, instr);
        currentBlock->intOverflowDoesNotMatterRange =
            IntOverflowDoesNotMatterRange::New(
                globOpt->alloc,
                instr,
                boundaryInstr,
                currentBlock->intOverflowDoesNotMatterRange);
    }
    else
    {
        Assert(currentBlock->intOverflowDoesNotMatterRange);

        // Extend the current range of instructions where int overflow does not matter, to include this instruction. We also need to
        // include the tracked syms for instructions that have not yet been included in the range, which are tracked in the range's
        // bit-vector. 'SymsRequiredToBeInt' will contain both the dst and src syms of instructions not yet included in the range;
        // the xor will remove the dst syms and add the src syms.
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Xor(candidateSymsRequiredToBeInt);
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Xor(candidateSymsRequiredToBeLossyInt);
        candidateSymsRequiredToBeInt->ClearAll();
        candidateSymsRequiredToBeLossyInt->ClearAll();
        currentBlock->intOverflowDoesNotMatterRange->SetFirstInstr(instr);
    }

    // Track syms that are inputs into the range based on the current instruction, which was just added to the range. The dst
    // sym is obtaining a new value so it isn't required to be an int at the start of the range, but the srcs are.
    if(dstSym)
    {LOGMEIN("BackwardPass.cpp] 6065\n");
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Clear(dstSym->m_id);
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Clear(dstSym->m_id);
    }
    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < sizeof(srcs) / sizeof(srcs[0]) && srcs[i]; ++i)
    {LOGMEIN("BackwardPass.cpp] 6071\n");
        StackSym *srcSym = IR::RegOpnd::TryGetStackSym(srcs[i]);
        if(!srcSym)
        {LOGMEIN("BackwardPass.cpp] 6074\n");
            continue;
        }

        if(currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->TestAndSet(srcSym->m_id))
        {LOGMEIN("BackwardPass.cpp] 6079\n");
            if(!lossy)
            {LOGMEIN("BackwardPass.cpp] 6081\n");
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Clear(srcSym->m_id);
            }
        }
        else if(lossy)
        {LOGMEIN("BackwardPass.cpp] 6086\n");
            currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Set(srcSym->m_id);
        }
    }

    // If the last instruction included in the range is a MUL, we have to end the range.
    // MULs with ignoreIntOverflow can still bailout on 53-bit overflow, so they cannot be in the middle of a range
    if (trackNon32BitOverflow && instr->m_opcode == Js::OpCode::Mul_A)
    {LOGMEIN("BackwardPass.cpp] 6094\n");
        // range would have ended already if int32 overflow matters
        Assert(instr->ignoreIntOverflowInRange && instr->ignoreOverflowBitCount != 32);
        EndIntOverflowDoesNotMatterRange();
    }
}

void
BackwardPass::SetNegativeZeroDoesNotMatterIfLastUse(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 6103\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {LOGMEIN("BackwardPass.cpp] 6106\n");
        negativeZeroDoesNotMatterBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetNegativeZeroMatters(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 6113\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {LOGMEIN("BackwardPass.cpp] 6116\n");
        negativeZeroDoesNotMatterBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::SetCouldRemoveNegZeroBailoutForDefIfLastUse(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 6123\n");
    StackSym * stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym && !this->currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {LOGMEIN("BackwardPass.cpp] 6126\n");
        this->currentBlock->couldRemoveNegZeroBailoutForDef->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetIntOverflowDoesNotMatterIfLastUse(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 6133\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {LOGMEIN("BackwardPass.cpp] 6136\n");
        intOverflowDoesNotMatterBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetIntOverflowMatters(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 6143\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {LOGMEIN("BackwardPass.cpp] 6146\n");
        intOverflowDoesNotMatterBySymId->Clear(stackSym->m_id);
    }
}

bool
BackwardPass::SetIntOverflowDoesNotMatterInRangeIfLastUse(IR::Opnd *const opnd, const int addSubUses)
{LOGMEIN("BackwardPass.cpp] 6153\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    return stackSym && SetIntOverflowDoesNotMatterInRangeIfLastUse(stackSym, addSubUses);
}

bool
BackwardPass::SetIntOverflowDoesNotMatterInRangeIfLastUse(StackSym *const stackSym, const int addSubUses)
{LOGMEIN("BackwardPass.cpp] 6160\n");
    Assert(stackSym);
    Assert(addSubUses >= 0);
    Assert(addSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

    if(currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {LOGMEIN("BackwardPass.cpp] 6166\n");
        return false;
    }

    intOverflowDoesNotMatterInRangeBySymId->Set(stackSym->m_id);
    stackSym->scratch.globOpt.numCompoundedAddSubUses = addSubUses;
    return true;
}

void
BackwardPass::SetIntOverflowMattersInRange(IR::Opnd *const opnd)
{LOGMEIN("BackwardPass.cpp] 6177\n");
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {LOGMEIN("BackwardPass.cpp] 6180\n");
        intOverflowDoesNotMatterInRangeBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::TransferCompoundedAddSubUsesToSrcs(IR::Instr *const instr, const int addSubUses)
{LOGMEIN("BackwardPass.cpp] 6187\n");
    Assert(instr);
    Assert(addSubUses >= 0);
    Assert(addSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < _countof(srcs) && srcs[i]; ++i)
    {LOGMEIN("BackwardPass.cpp] 6194\n");
        StackSym *const srcSym = IR::RegOpnd::TryGetStackSym(srcs[i]);
        if(!srcSym)
        {LOGMEIN("BackwardPass.cpp] 6197\n");
            // Int overflow tracking is only done for StackSyms in RegOpnds. Int overflow matters for the src, so it is
            // guaranteed to be in the int range at this point if the instruction is int-specialized.
            continue;
        }

        Assert(srcSym->scratch.globOpt.numCompoundedAddSubUses >= 0);
        Assert(srcSym->scratch.globOpt.numCompoundedAddSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

        if(SetIntOverflowDoesNotMatterInRangeIfLastUse(srcSym, addSubUses))
        {LOGMEIN("BackwardPass.cpp] 6207\n");
            // This is the last use of the src
            continue;
        }

        if(intOverflowDoesNotMatterInRangeBySymId->Test(srcSym->m_id))
        {LOGMEIN("BackwardPass.cpp] 6213\n");
            // Since a src may be compounded through different chains of add/sub instructions, the greater number must be
            // preserved
            srcSym->scratch.globOpt.numCompoundedAddSubUses =
                max(srcSym->scratch.globOpt.numCompoundedAddSubUses, addSubUses);
        }
        else
        {
            // Int overflow matters for the src, so it is guaranteed to be in the int range at this point if the instruction is
            // int-specialized
        }
    }
}

void
BackwardPass::EndIntOverflowDoesNotMatterRange()
{LOGMEIN("BackwardPass.cpp] 6229\n");
    if(intOverflowCurrentlyMattersInRange)
    {LOGMEIN("BackwardPass.cpp] 6231\n");
        return;
    }
    intOverflowCurrentlyMattersInRange = true;

    if(currentBlock->intOverflowDoesNotMatterRange->FirstInstr()->m_next ==
        currentBlock->intOverflowDoesNotMatterRange->LastInstr())
    {LOGMEIN("BackwardPass.cpp] 6238\n");
        // Don't need a range for a single-instruction range
        IntOverflowDoesNotMatterRange *const rangeToDelete = currentBlock->intOverflowDoesNotMatterRange;
        currentBlock->intOverflowDoesNotMatterRange = currentBlock->intOverflowDoesNotMatterRange->Next();
        currentBlock->RemoveInstr(rangeToDelete->LastInstr());
        rangeToDelete->Delete(globOpt->alloc);
    }
    else
    {
        // End the current range of instructions where int overflow does not matter
        IR::Instr *const boundaryInstr =
            IR::PragmaInstr::New(
                Js::OpCode::NoIntOverflowBoundary,
                0,
                currentBlock->intOverflowDoesNotMatterRange->FirstInstr()->m_func);
        boundaryInstr->SetByteCodeOffset(currentBlock->intOverflowDoesNotMatterRange->FirstInstr());
        currentBlock->InsertInstrBefore(boundaryInstr, currentBlock->intOverflowDoesNotMatterRange->FirstInstr());
        currentBlock->intOverflowDoesNotMatterRange->SetFirstInstr(boundaryInstr);

#if DBG_DUMP
        if(PHASE_TRACE(Js::TrackCompoundedIntOverflowPhase, func))
        {LOGMEIN("BackwardPass.cpp] 6259\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(
                _u("TrackCompoundedIntOverflow - Top function: %s (%s), Phase: %s, Block: %u\n"),
                func->GetJITFunctionBody()->GetDisplayName(),
                func->GetDebugNumberSet(debugStringBuffer),
                Js::PhaseNames[Js::BackwardPhase],
                currentBlock->GetBlockNum());
            Output::Print(_u("    Input syms to be int-specialized (lossless): "));
            candidateSymsRequiredToBeInt->Minus(
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt(),
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()); // candidate bit-vectors are cleared below anyway
            candidateSymsRequiredToBeInt->Dump();
            Output::Print(_u("    Input syms to be converted to int (lossy):   "));
            currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Dump();
            Output::Print(_u("    First instr: "));
            currentBlock->intOverflowDoesNotMatterRange->FirstInstr()->m_next->Dump();
            Output::Flush();
        }
#endif
    }

    // Reset candidates for the next range
    candidateSymsRequiredToBeInt->ClearAll();
    candidateSymsRequiredToBeLossyInt->ClearAll();

    // Syms are not tracked across different ranges of instructions where int overflow does not matter, since instructions
    // between the ranges may bail out. The value of the dst of an int operation where overflow is ignored is incorrect until
    // the last use of that sym is converted to int. If the int operation and the last use of the sym are in different ranges
    // and an instruction between the ranges bails out, other inputs into the second range are no longer guaranteed to be ints,
    // so the incorrect value of the sym may be used in non-int operations.
    intOverflowDoesNotMatterInRangeBySymId->ClearAll();
}

void
BackwardPass::TrackFloatSymEquivalence(IR::Instr *const instr)
{LOGMEIN("BackwardPass.cpp] 6295\n");
    /*
    This function determines sets of float-specialized syms where any two syms in a set may have the same value number at some
    point in the function. Conversely, if two float-specialized syms are not in the same set, it guarantees that those two syms
    will never have the same value number. These sets are referred to as equivalence classes here.

    The equivalence class for a sym is used to determine whether a bailout FromVar generating a float value for the sym needs to
    bail out on any non-number value. For instance, for syms s1 and s5 in an equivalence class (say we have s5 = s1 at some
    point), if there's a FromVar that generates a float value for s1 but only bails out on strings or non-primitives, and s5 is
    returned from the function, it has to be ensured that s5 is not converted to Var. If the source of the FromVar was null, the
    FromVar would not have bailed out, and s1 and s5 would have the value +0. When s5 is returned, we need to return null and
    not +0, so the equivalence class is used to determine that since s5 requires a bailout on any non-number value, so does s1.

    The tracking is very conservative because the bit that says "I require bailout on any non-number value" is on the sym itself
    (referred to as non-number bailout bit below).

    Data:
    - BackwardPass::floatSymEquivalenceMap
        - hash table mapping a float sym ID to its equivalence class
    - FloatSymEquivalenceClass
        - bit vector of float sym IDs that are in the equivalence class
        - one non-number bailout bit for all syms in the equivalence class

    Algorithm:
    - In a loop prepass or when not in loop:
        - For a float sym transfer (s0.f = s1.f), add both syms to an equivalence class (set the syms in a bit vector)
            - If either sym requires bailout on any non-number value, set the equivalence class' non-number bailout bit
        - If one of the syms is already in an equivalence class, merge the two equivalence classes by OR'ing the two bit vectors
          and the non-number bailout bit.
        - Note that for functions with a loop, dependency tracking is done using equivalence classes and that information is not
          transferred back into each sym's non-number bailout bit
    - In a loop non-prepass or when not in loop, for a FromVar instruction that requires bailout only on strings and
      non-primitives:
        - If the destination float sym's non-number bailout bit is set, or the sym is in an equivalence class whose non-number
          bailout bit is set, change the bailout to bail out on any non-number value

    The result is that if a float-specialized sym's value is used in a way in which it would be invalid to use the float value
    through any other float-specialized sym that acquires the value, the FromVar generating the float value will be modified to
    bail out on any non-number value.
    */

    Assert(instr);

    if(tag != Js::DeadStorePhase || instr->GetSrc2() || !instr->m_func->hasBailout)
    {LOGMEIN("BackwardPass.cpp] 6339\n");
        return;
    }

    if(!instr->GetDst() || !instr->GetDst()->IsRegOpnd())
    {LOGMEIN("BackwardPass.cpp] 6344\n");
        return;
    }
    const auto dst = instr->GetDst()->AsRegOpnd()->m_sym;
    if(!dst->IsFloat64())
    {LOGMEIN("BackwardPass.cpp] 6349\n");
        return;
    }

    if(!instr->GetSrc1() || !instr->GetSrc1()->IsRegOpnd())
    {LOGMEIN("BackwardPass.cpp] 6354\n");
        return;
    }
    const auto src = instr->GetSrc1()->AsRegOpnd()->m_sym;

    if(OpCodeAttr::NonIntTransfer(instr->m_opcode) && (!currentBlock->loop || IsPrePass()))
    {LOGMEIN("BackwardPass.cpp] 6360\n");
        Assert(src->IsFloat64()); // dst is specialized, and since this is a float transfer, src must be specialized too

        if(dst == src)
        {LOGMEIN("BackwardPass.cpp] 6364\n");
            return;
        }

        if(!func->m_fg->hasLoop)
        {LOGMEIN("BackwardPass.cpp] 6369\n");
            // Special case for functions with no loops, since there can only be in-order dependencies. Just merge the two
            // non-number bailout bits and put the result in the source.
            if(dst->m_requiresBailOnNotNumber)
            {LOGMEIN("BackwardPass.cpp] 6373\n");
                src->m_requiresBailOnNotNumber = true;
            }
            return;
        }

        FloatSymEquivalenceClass *dstEquivalenceClass, *srcEquivalenceClass;
        const bool dstHasEquivalenceClass = floatSymEquivalenceMap->TryGetValue(dst->m_id, &dstEquivalenceClass);
        const bool srcHasEquivalenceClass = floatSymEquivalenceMap->TryGetValue(src->m_id, &srcEquivalenceClass);

        if(!dstHasEquivalenceClass)
        {LOGMEIN("BackwardPass.cpp] 6384\n");
            if(srcHasEquivalenceClass)
            {LOGMEIN("BackwardPass.cpp] 6386\n");
                // Just add the destination into the source's equivalence class
                srcEquivalenceClass->Set(dst);
                floatSymEquivalenceMap->Add(dst->m_id, srcEquivalenceClass);
                return;
            }

            dstEquivalenceClass = JitAnew(tempAlloc, FloatSymEquivalenceClass, tempAlloc);
            dstEquivalenceClass->Set(dst);
            floatSymEquivalenceMap->Add(dst->m_id, dstEquivalenceClass);
        }

        if(!srcHasEquivalenceClass)
        {LOGMEIN("BackwardPass.cpp] 6399\n");
            // Just add the source into the destination's equivalence class
            dstEquivalenceClass->Set(src);
            floatSymEquivalenceMap->Add(src->m_id, dstEquivalenceClass);
            return;
        }

        if(dstEquivalenceClass == srcEquivalenceClass)
        {LOGMEIN("BackwardPass.cpp] 6407\n");
            return;
        }

        Assert(!dstEquivalenceClass->Bv()->Test(src->m_id));
        Assert(!srcEquivalenceClass->Bv()->Test(dst->m_id));

        // Merge the two equivalence classes. The source's equivalence class is typically smaller, so it's merged into the
        // destination's equivalence class. To save space and prevent a potential explosion of bit vector size,
        // 'floatSymEquivalenceMap' is updated for syms in the source's equivalence class to map to the destination's now merged
        // equivalence class, and the source's equivalence class is discarded.
        dstEquivalenceClass->Or(srcEquivalenceClass);
        FOREACH_BITSET_IN_SPARSEBV(id, srcEquivalenceClass->Bv())
        {LOGMEIN("BackwardPass.cpp] 6420\n");
            floatSymEquivalenceMap->Item(id, dstEquivalenceClass);
        } NEXT_BITSET_IN_SPARSEBV;
        JitAdelete(tempAlloc, srcEquivalenceClass);

        return;
    }

    // Not a float transfer, and non-prepass (not necessarily in a loop)

    if(!instr->HasBailOutInfo() || instr->GetBailOutKind() != IR::BailOutPrimitiveButString)
    {LOGMEIN("BackwardPass.cpp] 6431\n");
        return;
    }
    Assert(instr->m_opcode == Js::OpCode::FromVar);

    // If either the destination or its equivalence class says it requires bailout on any non-number value, adjust the bailout
    // kind on the instruction. Both are checked because in functions without loops, equivalence tracking is not done and only
    // the sym's non-number bailout bit will have the information, and in functions with loops, equivalence tracking is done
    // throughout the function and checking just the sym's non-number bailout bit is insufficient.
    FloatSymEquivalenceClass *dstEquivalenceClass;
    if(dst->m_requiresBailOnNotNumber ||
        (floatSymEquivalenceMap->TryGetValue(dst->m_id, &dstEquivalenceClass) && dstEquivalenceClass->RequiresBailOnNotNumber()))
    {LOGMEIN("BackwardPass.cpp] 6443\n");
        instr->SetBailOutKind(IR::BailOutNumberOnly);
    }
}

bool
BackwardPass::ProcessDef(IR::Opnd * opnd)
{LOGMEIN("BackwardPass.cpp] 6450\n");
    BOOLEAN isJITOptimizedReg = false;
    Sym * sym;
    if (opnd->IsRegOpnd())
    {LOGMEIN("BackwardPass.cpp] 6454\n");
        sym = opnd->AsRegOpnd()->m_sym;
        isJITOptimizedReg = opnd->GetIsJITOptimizedReg();
        if (!IsCollectionPass())
        {LOGMEIN("BackwardPass.cpp] 6458\n");
            this->InvalidateCloneStrCandidate(opnd);
        }
    }
    else if (opnd->IsSymOpnd())
    {LOGMEIN("BackwardPass.cpp] 6463\n");
        sym = opnd->AsSymOpnd()->m_sym;
        isJITOptimizedReg = opnd->GetIsJITOptimizedReg();
    }
    else
    {
        if (opnd->IsIndirOpnd())
        {LOGMEIN("BackwardPass.cpp] 6470\n");
            this->ProcessUse(opnd);
        }
        return false;
    }

    BasicBlock * block = this->currentBlock;
    BOOLEAN isUsed = true;
    BOOLEAN keepSymLiveForException = false;
    BOOLEAN keepVarSymLiveForException = false;
    IR::Instr * instr = this->currentInstr;
    Assert(!instr->IsByteCodeUsesInstr());
    if (sym->IsPropertySym())
    {LOGMEIN("BackwardPass.cpp] 6483\n");
        if(IsCollectionPass())
        {LOGMEIN("BackwardPass.cpp] 6485\n");
            return false;
        }

        Assert((block->fieldHoistCandidates != nullptr) == this->DoFieldHoistCandidates());
        if (block->fieldHoistCandidates)
        {LOGMEIN("BackwardPass.cpp] 6491\n");
            block->fieldHoistCandidates->Clear(sym->m_id);
        }
        PropertySym *propertySym = sym->AsPropertySym();
        if (this->DoDeadStoreSlots())
        {LOGMEIN("BackwardPass.cpp] 6496\n");
            if (propertySym->m_fieldKind == PropertyKindLocalSlots || propertySym->m_fieldKind == PropertyKindSlots)
            {LOGMEIN("BackwardPass.cpp] 6498\n");
                BOOLEAN isPropertySymUsed = !block->slotDeadStoreCandidates->TestAndSet(propertySym->m_id);
                // we should not do any dead slots in asmjs loop body
                Assert(!(this->func->GetJITFunctionBody()->IsAsmJsMode() && this->func->IsLoopBody() && !isPropertySymUsed));
                Assert(isPropertySymUsed || !block->upwardExposedUses->Test(propertySym->m_id));

                isUsed = isPropertySymUsed || block->upwardExposedUses->Test(propertySym->m_stackSym->m_id);
            }
        }

        this->DoSetDead(opnd, !block->upwardExposedFields->TestAndClear(propertySym->m_id));

        ProcessStackSymUse(propertySym->m_stackSym, isJITOptimizedReg);
        if (tag == Js::BackwardPhase)
        {LOGMEIN("BackwardPass.cpp] 6512\n");
            if (opnd->AsSymOpnd()->IsPropertySymOpnd())
            {LOGMEIN("BackwardPass.cpp] 6514\n");
                this->globOpt->PreparePropertySymOpndForTypeCheckSeq(opnd->AsPropertySymOpnd(), instr, this->currentBlock->loop);
            }
        }
        if (opnd->AsSymOpnd()->IsPropertySymOpnd())
        {LOGMEIN("BackwardPass.cpp] 6519\n");
            this->ProcessPropertySymOpndUse(opnd->AsPropertySymOpnd());
        }
    }
    else
    {
        Assert(!instr->IsByteCodeUsesInstr());

        if (this->DoByteCodeUpwardExposedUsed())
        {LOGMEIN("BackwardPass.cpp] 6528\n");
            if (sym->AsStackSym()->HasByteCodeRegSlot())
            {LOGMEIN("BackwardPass.cpp] 6530\n");
                StackSym * varSym = sym->AsStackSym();
                if (varSym->IsTypeSpec())
                {LOGMEIN("BackwardPass.cpp] 6533\n");
                    // It has to have a var version for byte code regs
                    varSym = varSym->GetVarEquivSym(nullptr);
                }

                if (this->currentRegion)
                {LOGMEIN("BackwardPass.cpp] 6539\n");
                    keepSymLiveForException = this->CheckWriteThroughSymInRegion(this->currentRegion, sym->AsStackSym());
                    keepVarSymLiveForException = this->CheckWriteThroughSymInRegion(this->currentRegion, varSym);
                }

                if (!isJITOptimizedReg)
                {LOGMEIN("BackwardPass.cpp] 6545\n");
                    if (!DoDeadStore(this->func, sym->AsStackSym()))
                    {LOGMEIN("BackwardPass.cpp] 6547\n");
                        // Don't deadstore the bytecodereg sym, so that we could do write to get the locals inspection
                        if (opnd->IsRegOpnd())
                        {LOGMEIN("BackwardPass.cpp] 6550\n");
                            opnd->AsRegOpnd()->m_dontDeadStore = true;
                        }
                    }

                    // write through symbols should not be cleared from the byteCodeUpwardExposedUsed BV upon defs in the Try region:
                    //      try
                    //          x =
                    //          <bailout> <-- this bailout should restore x from its first def. This would not happen if x is cleared
                    //                        from byteCodeUpwardExposedUsed when we process its second def
                    //          <exception>
                    //          x =
                    //      catch
                    //          = x
                    if (!keepVarSymLiveForException)
                    {LOGMEIN("BackwardPass.cpp] 6565\n");
                        // Always track the sym use on the var sym.
                        block->byteCodeUpwardExposedUsed->Clear(varSym->m_id);
#if DBG
                        // TODO: We can only track first level function stack syms right now
                        if (varSym->GetByteCodeFunc() == this->func)
                        {LOGMEIN("BackwardPass.cpp] 6571\n");
                            block->byteCodeRestoreSyms[varSym->GetByteCodeRegSlot()] = nullptr;
                        }
#endif
                    }
                }
            }
        }

        if(IsCollectionPass())
        {LOGMEIN("BackwardPass.cpp] 6581\n");
            return false;
        }

        // Don't care about property sym for mark temps
        if (opnd->IsRegOpnd())
        {LOGMEIN("BackwardPass.cpp] 6587\n");
            this->MarkTemp(sym->AsStackSym());
        }

        if (this->tag == Js::BackwardPhase &&
            instr->m_opcode == Js::OpCode::Ld_A &&
            instr->GetSrc1()->IsRegOpnd() &&
            block->upwardExposedFields->Test(sym->m_id))
        {LOGMEIN("BackwardPass.cpp] 6595\n");
            block->upwardExposedFields->Set(instr->GetSrc1()->AsRegOpnd()->m_sym->m_id);
        }

        if (!keepSymLiveForException)
        {LOGMEIN("BackwardPass.cpp] 6600\n");
            isUsed = block->upwardExposedUses->TestAndClear(sym->m_id);
        }
    }

    if (isUsed || !this->DoDeadStore())
    {LOGMEIN("BackwardPass.cpp] 6606\n");
        return false;
    }

    // FromVar on a primitive value has no side-effects
    // TODO: There may be more cases where FromVars can be dead-stored, such as cases where they have a bailout that would bail
    // out on non-primitive vars, thereby causing no side effects anyway. However, it needs to be ensured that no assumptions
    // that depend on the bailout are made later in the function.

    // Special case StFld for trackable fields
    bool hasSideEffects = instr->HasAnySideEffects()
        && instr->m_opcode != Js::OpCode::StFld
        && instr->m_opcode != Js::OpCode::StRootFld
        && instr->m_opcode != Js::OpCode::StFldStrict
        && instr->m_opcode != Js::OpCode::StRootFldStrict;

    if (this->IsPrePass() || hasSideEffects)
    {LOGMEIN("BackwardPass.cpp] 6623\n");
        return false;
    }

    if (opnd->IsRegOpnd() && opnd->AsRegOpnd()->m_dontDeadStore)
    {LOGMEIN("BackwardPass.cpp] 6628\n");
        return false;
    }

    if (instr->HasBailOutInfo())
    {LOGMEIN("BackwardPass.cpp] 6633\n");
        // A bailout inserted for aggressive or lossy int type specialization causes assumptions to be made on the value of
        // the instruction's destination later on, as though the bailout did not happen. If the value is an int constant and
        // that value is propagated forward, it can cause the bailout instruction to become a dead store and be removed,
        // thereby invalidating the assumptions made. Or for lossy int type specialization, the lossy conversion to int32
        // may have side effects and so cannot be dead-store-removed. As one way of solving that problem, bailout
        // instructions resulting from aggressive or lossy int type spec are not dead-stored.
        const auto bailOutKind = instr->GetBailOutKind();
        if(bailOutKind & IR::BailOutOnResultConditions)
        {LOGMEIN("BackwardPass.cpp] 6642\n");
            return false;
        }
        switch(bailOutKind & ~IR::BailOutKindBits)
        {LOGMEIN("BackwardPass.cpp] 6646\n");
            case IR::BailOutIntOnly:
            case IR::BailOutNumberOnly:
            case IR::BailOutExpectingInteger:
            case IR::BailOutPrimitiveButString:
            case IR::BailOutExpectingString:
            case IR::BailOutOnNotPrimitive:
            case IR::BailOutFailedInlineTypeCheck:
            case IR::BailOutOnFloor:
            case IR::BailOnModByPowerOf2:
            case IR::BailOnDivResultNotInt:
            case IR::BailOnIntMin:
                return false;
        }
    }

    // Dead store
    DeadStoreInstr(instr);
    return true;
}

bool
BackwardPass::DeadStoreInstr(IR::Instr *instr)
{LOGMEIN("BackwardPass.cpp] 6669\n");
    BasicBlock * block = this->currentBlock;

#if DBG_DUMP
    if (this->IsTraceEnabled())
    {LOGMEIN("BackwardPass.cpp] 6674\n");
        Output::Print(_u("Deadstore instr: "));
        instr->Dump();
    }
    this->numDeadStore++;
#endif

    // Before we remove the dead store, we need to track the byte code uses
    if (this->DoByteCodeUpwardExposedUsed())
    {LOGMEIN("BackwardPass.cpp] 6683\n");
#if DBG
        BVSparse<JitArenaAllocator> tempBv(this->tempAlloc);
        tempBv.Copy(this->currentBlock->byteCodeUpwardExposedUsed);
#endif
        PropertySym *unusedPropertySym = nullptr;
        
        GlobOpt::TrackByteCodeSymUsed(instr, this->currentBlock->byteCodeUpwardExposedUsed, &unusedPropertySym);
        
#if DBG
        BVSparse<JitArenaAllocator> tempBv2(this->tempAlloc);
        tempBv2.Copy(this->currentBlock->byteCodeUpwardExposedUsed);
        tempBv2.Minus(&tempBv);
        FOREACH_BITSET_IN_SPARSEBV(symId, &tempBv2)
        {LOGMEIN("BackwardPass.cpp] 6697\n");
            StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
            Assert(stackSym->GetType() == TyVar);
            // TODO: We can only track first level function stack syms right now
            if (stackSym->GetByteCodeFunc() == this->func)
            {LOGMEIN("BackwardPass.cpp] 6702\n");
                Js::RegSlot byteCodeRegSlot = stackSym->GetByteCodeRegSlot();
                Assert(byteCodeRegSlot != Js::Constants::NoRegister);
                if (this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] != stackSym)
                {LOGMEIN("BackwardPass.cpp] 6706\n");
                    AssertMsg(this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] == nullptr,
                        "Can't have two active lifetime for the same byte code register");
                    this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] = stackSym;
                }
            }
        }
        NEXT_BITSET_IN_SPARSEBV;
#endif
    }

    // If this is a pre-op bailout instruction, we may have saved it for bailout info processing. It's being removed now, so no
    // need to process the bailout info anymore.
    Assert(!preOpBailOutInstrToProcess || preOpBailOutInstrToProcess == instr);
    preOpBailOutInstrToProcess = nullptr;

#if DBG
    if (this->DoMarkTempObjectVerify())
    {LOGMEIN("BackwardPass.cpp] 6724\n");
        this->currentBlock->tempObjectVerifyTracker->NotifyDeadStore(instr, this);
    }
#endif

    
    if (instr->m_opcode == Js::OpCode::ArgIn_A)
    {LOGMEIN("BackwardPass.cpp] 6731\n");
        //Ignore tracking ArgIn for "this", as argInsCount only tracks other params - unless it is a asmjs function(which doesn't have a "this").
        if (instr->GetSrc1()->AsSymOpnd()->m_sym->AsStackSym()->GetParamSlotNum() != 1 || func->GetJITFunctionBody()->IsAsmJsMode())
        {LOGMEIN("BackwardPass.cpp] 6734\n");
            Assert(this->func->argInsCount > 0);
            this->func->argInsCount--;
        }
    }

    TraceDeadStoreOfInstrsForScopeObjectRemoval();
    
    block->RemoveInstr(instr);
    return true;
}

void
BackwardPass::ProcessTransfers(IR::Instr * instr)
{LOGMEIN("BackwardPass.cpp] 6748\n");
    if (this->tag == Js::DeadStorePhase &&
        this->currentBlock->upwardExposedFields &&
        instr->m_opcode == Js::OpCode::Ld_A &&
        instr->GetDst()->GetStackSym() &&
        !instr->GetDst()->GetStackSym()->IsTypeSpec() &&
        instr->GetDst()->GetStackSym()->HasObjectInfo() &&
        instr->GetSrc1() &&
        instr->GetSrc1()->GetStackSym() &&
        !instr->GetSrc1()->GetStackSym()->IsTypeSpec() &&
        instr->GetSrc1()->GetStackSym()->HasObjectInfo())
    {LOGMEIN("BackwardPass.cpp] 6759\n");
        StackSym * dstStackSym = instr->GetDst()->GetStackSym();
        PropertySym * dstPropertySym = dstStackSym->GetObjectInfo()->m_propertySymList;
        BVSparse<JitArenaAllocator> transferFields(this->tempAlloc);
        while (dstPropertySym != nullptr)
        {LOGMEIN("BackwardPass.cpp] 6764\n");
            Assert(dstPropertySym->m_stackSym == dstStackSym);
            transferFields.Set(dstPropertySym->m_id);
            dstPropertySym = dstPropertySym->m_nextInStackSymList;
        }

        StackSym * srcStackSym = instr->GetSrc1()->GetStackSym();
        PropertySym * srcPropertySym = srcStackSym->GetObjectInfo()->m_propertySymList;
        BVSparse<JitArenaAllocator> equivFields(this->tempAlloc);

        while (srcPropertySym != nullptr && !transferFields.IsEmpty())
        {LOGMEIN("BackwardPass.cpp] 6775\n");
            Assert(srcPropertySym->m_stackSym == srcStackSym);
            if (srcPropertySym->m_propertyEquivSet)
            {LOGMEIN("BackwardPass.cpp] 6778\n");
                equivFields.And(&transferFields, srcPropertySym->m_propertyEquivSet);
                if (!equivFields.IsEmpty())
                {LOGMEIN("BackwardPass.cpp] 6781\n");
                    transferFields.Minus(&equivFields);
                    this->currentBlock->upwardExposedFields->Set(srcPropertySym->m_id);
                }
            }
            srcPropertySym = srcPropertySym->m_nextInStackSymList;
        }
    }
}

void
BackwardPass::ProcessFieldKills(IR::Instr * instr)
{LOGMEIN("BackwardPass.cpp] 6793\n");
    if (this->currentBlock->upwardExposedFields)
    {LOGMEIN("BackwardPass.cpp] 6795\n");
        this->globOpt->ProcessFieldKills(instr, this->currentBlock->upwardExposedFields, false);
    }

    this->ClearBucketsOnFieldKill(instr, currentBlock->stackSymToFinalType);
    this->ClearBucketsOnFieldKill(instr, currentBlock->stackSymToGuardedProperties);
}

template<typename T>
void
BackwardPass::ClearBucketsOnFieldKill(IR::Instr *instr, HashTable<T> *table)
{LOGMEIN("BackwardPass.cpp] 6806\n");
    if (table)
    {LOGMEIN("BackwardPass.cpp] 6808\n");
        if (instr->UsesAllFields())
        {LOGMEIN("BackwardPass.cpp] 6810\n");
            table->ClearAll();
        }
        else
        {
            IR::Opnd *dst = instr->GetDst();
            if (dst && dst->IsRegOpnd())
            {LOGMEIN("BackwardPass.cpp] 6817\n");
                table->Clear(dst->AsRegOpnd()->m_sym->m_id);
            }
        }
    }
}

void
BackwardPass::ProcessFieldHoistKills(IR::Instr * instr)
{LOGMEIN("BackwardPass.cpp] 6826\n");
    // The backward pass, we optimistically will not kill on a[] access
    // So that the field hoist candidate will be more then what can be hoisted
    // The root prepass will figure out the exact set of field that is hoisted
    this->globOpt->ProcessFieldKills(instr, this->currentBlock->fieldHoistCandidates, false);

    switch (instr->m_opcode)
    {LOGMEIN("BackwardPass.cpp] 6833\n");
    case Js::OpCode::BrOnHasProperty:
    case Js::OpCode::BrOnNoProperty:
        // Should not hoist pass these instructions
        this->currentBlock->fieldHoistCandidates->Clear(instr->GetSrc1()->AsSymOpnd()->m_sym->m_id);
        break;
    }
}

bool
BackwardPass::TrackNoImplicitCallInlinees(IR::Instr *instr)
{LOGMEIN("BackwardPass.cpp] 6844\n");
    if (this->tag != Js::DeadStorePhase || this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 6846\n");
        return false;
    }

    if (instr->HasBailOutInfo()
        || OpCodeAttr::CallInstr(instr->m_opcode)
        || instr->CallsAccessor()
        || GlobOpt::MayNeedBailOnImplicitCall(instr, nullptr, nullptr)
        || instr->m_opcode == Js::OpCode::LdHeapArguments
        || instr->m_opcode == Js::OpCode::LdLetHeapArguments
        || instr->m_opcode == Js::OpCode::LdHeapArgsCached
        || instr->m_opcode == Js::OpCode::LdLetHeapArgsCached
        || instr->m_opcode == Js::OpCode::LdFuncExpr)
    {LOGMEIN("BackwardPass.cpp] 6859\n");
        // This func has instrs with bailouts or implicit calls
        Assert(instr->m_opcode != Js::OpCode::InlineeStart);
        instr->m_func->SetHasImplicitCallsOnSelfAndParents();
        return false;
    }

    if (instr->m_opcode == Js::OpCode::InlineeStart)
    {LOGMEIN("BackwardPass.cpp] 6867\n");
        if (!instr->GetSrc1())
        {LOGMEIN("BackwardPass.cpp] 6869\n");
            Assert(instr->m_func->m_hasInlineArgsOpt);
            return false;
        }
        return this->ProcessInlineeStart(instr);
    }

    return false;
}

bool
BackwardPass::ProcessInlineeStart(IR::Instr* inlineeStart)
{LOGMEIN("BackwardPass.cpp] 6881\n");
    inlineeStart->m_func->SetFirstArgOffset(inlineeStart);

    IR::Instr* startCallInstr = nullptr;
    bool noImplicitCallsInInlinee = false;
    // Inlinee has no bailouts or implicit calls.  Get rid of the inline overhead.
    auto removeInstr = [&](IR::Instr* argInstr)
    {LOGMEIN("BackwardPass.cpp] 6888\n");
        Assert(argInstr->m_opcode == Js::OpCode::InlineeStart || argInstr->m_opcode == Js::OpCode::ArgOut_A || argInstr->m_opcode == Js::OpCode::ArgOut_A_Inline);
        IR::Opnd *opnd = argInstr->GetSrc1();
        StackSym *sym = opnd->GetStackSym();
        if (!opnd->GetIsJITOptimizedReg() && sym && sym->HasByteCodeRegSlot())
        {LOGMEIN("BackwardPass.cpp] 6893\n");
            // Replace instrs with bytecodeUses
            IR::ByteCodeUsesInstr *bytecodeUse = IR::ByteCodeUsesInstr::New(argInstr);
            bytecodeUse->Set(opnd);
            argInstr->InsertBefore(bytecodeUse);
        }
        startCallInstr = argInstr->GetSrc2()->GetStackSym()->m_instrDef;
        FlowGraph::SafeRemoveInstr(argInstr);
        return false;
    };

    // If there are no implicit calls - bailouts/throws - we can remove all inlining overhead.
    if (!inlineeStart->m_func->GetHasImplicitCalls())
    {LOGMEIN("BackwardPass.cpp] 6906\n");
        noImplicitCallsInInlinee = true;
        inlineeStart->IterateArgInstrs(removeInstr);

        inlineeStart->IterateMetaArgs([](IR::Instr* metArg)
        {
            FlowGraph::SafeRemoveInstr(metArg);
            return false;
        });
        inlineeStart->m_func->m_hasInlineArgsOpt = false;
        removeInstr(inlineeStart);
        return true;
    }

    if (!inlineeStart->m_func->m_hasInlineArgsOpt)
    {LOGMEIN("BackwardPass.cpp] 6921\n");
        PHASE_PRINT_TESTTRACE(Js::InlineArgsOptPhase, func, _u("%s[%d]: Skipping inline args optimization: %s[%d] HasCalls: %s 'arguments' access: %s Can do inlinee args opt: %s\n"),
                func->GetJITFunctionBody()->GetDisplayName(), func->GetJITFunctionBody()->GetFunctionNumber(),
                inlineeStart->m_func->GetJITFunctionBody()->GetDisplayName(), inlineeStart->m_func->GetJITFunctionBody()->GetFunctionNumber(),
                IsTrueOrFalse(inlineeStart->m_func->GetHasCalls()),
                IsTrueOrFalse(inlineeStart->m_func->GetHasUnoptimizedArgumentsAcccess()),
                IsTrueOrFalse(inlineeStart->m_func->m_canDoInlineArgsOpt));
        return false;
    }

    if (!inlineeStart->m_func->frameInfo->isRecorded)
    {LOGMEIN("BackwardPass.cpp] 6932\n");
        PHASE_PRINT_TESTTRACE(Js::InlineArgsOptPhase, func, _u("%s[%d]: InlineeEnd not found - usually due to a throw or a BailOnNoProfile (stressed, most likely)\n"),
            func->GetJITFunctionBody()->GetDisplayName(), func->GetJITFunctionBody()->GetFunctionNumber());
        inlineeStart->m_func->DisableCanDoInlineArgOpt();
        return false;
    }

    inlineeStart->IterateArgInstrs(removeInstr);
    int i = 0;
    inlineeStart->IterateMetaArgs([&](IR::Instr* metaArg)
    {
        if (i == Js::Constants::InlineeMetaArgIndex_ArgumentsObject &&
            inlineeStart->m_func->GetJITFunctionBody()->UsesArgumentsObject())
        {LOGMEIN("BackwardPass.cpp] 6945\n");
            Assert(!inlineeStart->m_func->GetHasUnoptimizedArgumentsAcccess());
            // Do not remove arguments object meta arg if there is a reference to arguments object
        }
        else
        {
            FlowGraph::SafeRemoveInstr(metaArg);
        }
        i++;
        return false;
    });

    IR::Opnd *src1 = inlineeStart->GetSrc1();

    StackSym *sym = src1->GetStackSym();
    if (!src1->GetIsJITOptimizedReg() && sym && sym->HasByteCodeRegSlot())
    {LOGMEIN("BackwardPass.cpp] 6961\n");
        // Replace instrs with bytecodeUses
        IR::ByteCodeUsesInstr *bytecodeUse = IR::ByteCodeUsesInstr::New(inlineeStart);
        bytecodeUse->Set(src1);
        inlineeStart->InsertBefore(bytecodeUse);
    }

    // This indicates to the lowerer that this inlinee has been optimized
    // and it should not be lowered - Now this instruction is used to mark inlineeStart
    inlineeStart->FreeSrc1();
    inlineeStart->FreeSrc2();
    inlineeStart->FreeDst();
    return true;
}

void
BackwardPass::ProcessInlineeEnd(IR::Instr* instr)
{LOGMEIN("BackwardPass.cpp] 6978\n");
    if (this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 6980\n");
        return;
    }
    if (this->tag == Js::BackwardPhase)
    {LOGMEIN("BackwardPass.cpp] 6984\n");
        if (!GlobOpt::DoInlineArgsOpt(instr->m_func))
        {LOGMEIN("BackwardPass.cpp] 6986\n");
            return;
        }

        // This adds a use for function sym as part of InlineeStart & all the syms referenced by the args.
        // It ensure they do not get cleared from the copy prop sym map.
        instr->IterateArgInstrs([=](IR::Instr* argInstr){
            if (argInstr->GetSrc1()->IsRegOpnd())
            {LOGMEIN("BackwardPass.cpp] 6994\n");
                this->currentBlock->upwardExposedUses->Set(argInstr->GetSrc1()->AsRegOpnd()->m_sym->m_id);
            }
            return false;
        });
    }
    else if (this->tag == Js::DeadStorePhase)
    {LOGMEIN("BackwardPass.cpp] 7001\n");
        if (instr->m_func->m_hasInlineArgsOpt)
        {LOGMEIN("BackwardPass.cpp] 7003\n");
            Assert(instr->m_func->frameInfo);
            instr->m_func->frameInfo->IterateSyms([=](StackSym* argSym)
            {
                this->currentBlock->upwardExposedUses->Set(argSym->m_id);
            });
        }
    }
}

bool
BackwardPass::ProcessBailOnNoProfile(IR::Instr *instr, BasicBlock *block)
{LOGMEIN("BackwardPass.cpp] 7015\n");
    Assert(this->tag == Js::BackwardPhase);
    Assert(instr->m_opcode == Js::OpCode::BailOnNoProfile);
    Assert(!instr->HasBailOutInfo());
    AnalysisAssert(block);

    if (this->IsPrePass())
    {LOGMEIN("BackwardPass.cpp] 7022\n");
        return false;
    }

    IR::Instr *curInstr = instr->m_prev;

    if (curInstr->IsLabelInstr() && curInstr->AsLabelInstr()->isOpHelper)
    {LOGMEIN("BackwardPass.cpp] 7029\n");
        // Already processed

        if (this->DoMarkTempObjects())
        {LOGMEIN("BackwardPass.cpp] 7033\n");
            block->tempObjectTracker->ProcessBailOnNoProfile(instr);
        }
        return false;
    }

    // Don't hoist if we see calls with profile data (recursive calls)
    while(!curInstr->StartsBasicBlock())
    {LOGMEIN("BackwardPass.cpp] 7041\n");
        // If a function was inlined, it must have had profile info.
        if (curInstr->m_opcode == Js::OpCode::InlineeEnd || curInstr->m_opcode == Js::OpCode::InlineBuiltInEnd || curInstr->m_opcode == Js::OpCode::InlineNonTrackingBuiltInEnd
            || curInstr->m_opcode == Js::OpCode::InlineeStart || curInstr->m_opcode == Js::OpCode::EndCallForPolymorphicInlinee)
        {LOGMEIN("BackwardPass.cpp] 7045\n");
            break;
        }
        else if (OpCodeAttr::CallInstr(curInstr->m_opcode))
        {LOGMEIN("BackwardPass.cpp] 7049\n");
            if (curInstr->m_prev->m_opcode != Js::OpCode::BailOnNoProfile)
            {LOGMEIN("BackwardPass.cpp] 7051\n");
                break;
            }
        }
        curInstr = curInstr->m_prev;
    }

    // Didn't get to the top of the block, delete this BailOnNoProfile.
    if (!curInstr->IsLabelInstr())
    {LOGMEIN("BackwardPass.cpp] 7060\n");
        block->RemoveInstr(instr);
        return true;
    }

    // Save the head instruction for later use.
    IR::LabelInstr *blockHeadInstr = curInstr->AsLabelInstr();

    // We can't bail in the middle of a "tmp = CmEq s1, s2; BrTrue tmp" turned into a "BrEq s1, s2",
    // because the bailout wouldn't be able to restore tmp.
    IR::Instr *curNext = curInstr->GetNextRealInstrOrLabel();
    IR::Instr *instrNope = nullptr;
    if (curNext->m_opcode == Js::OpCode::Ld_A && curNext->GetDst()->IsRegOpnd() && curNext->GetDst()->AsRegOpnd()->m_fgPeepTmp)
    {LOGMEIN("BackwardPass.cpp] 7073\n");
        block->RemoveInstr(instr);
        return true;
        /*while (curNext->m_opcode == Js::OpCode::Ld_A && curNext->GetDst()->IsRegOpnd() && curNext->GetDst()->AsRegOpnd()->m_fgPeepTmp)
        {
            // Instead of just giving up, we can be a little trickier. We can instead treat the tmp declaration(s) as a
            // part of the block prefix, and put the bailonnoprofile immediately after them. This has the added benefit
            // that we can still merge up blocks beginning with bailonnoprofile, even if they would otherwise not allow
            // us to, due to the fact that these tmp declarations would be pre-empted by the higher-level bailout.
            instrNope = curNext;
            curNext = curNext->GetNextRealInstrOrLabel();
        }*/
    }

    curInstr = instr->m_prev;

    // Move to top of block (but just below any fgpeeptemp lds).
    while(!curInstr->StartsBasicBlock() && curInstr != instrNope)
    {LOGMEIN("BackwardPass.cpp] 7091\n");
        // Delete redundant BailOnNoProfile
        if (curInstr->m_opcode == Js::OpCode::BailOnNoProfile)
        {LOGMEIN("BackwardPass.cpp] 7094\n");
            Assert(!curInstr->HasBailOutInfo());
            curInstr = curInstr->m_next;
            curInstr->m_prev->Remove();
        }
        curInstr = curInstr->m_prev;
    }

    if (instr == block->GetLastInstr())
    {LOGMEIN("BackwardPass.cpp] 7103\n");
        block->SetLastInstr(instr->m_prev);
    }

    instr->Unlink();

    // Now try to move this up the flowgraph to the predecessor blocks
    FOREACH_PREDECESSOR_BLOCK(pred, block)
    {LOGMEIN("BackwardPass.cpp] 7111\n");
        bool hoistBailToPred = true;

        if (block->isLoopHeader && pred->loop == block->loop)
        {LOGMEIN("BackwardPass.cpp] 7115\n");
            // Skip loop back-edges
            continue;
        }

        // If all successors of this predecessor start with a BailOnNoProfile, we should be
        // okay to hoist this bail to the predecessor.
        FOREACH_SUCCESSOR_BLOCK(predSucc, pred)
        {LOGMEIN("BackwardPass.cpp] 7123\n");
            if (predSucc == block)
            {LOGMEIN("BackwardPass.cpp] 7125\n");
                continue;
            }
            if (!predSucc->beginsBailOnNoProfile)
            {LOGMEIN("BackwardPass.cpp] 7129\n");
                hoistBailToPred = false;
                break;
            }
        } NEXT_SUCCESSOR_BLOCK;

        if (hoistBailToPred)
        {LOGMEIN("BackwardPass.cpp] 7136\n");
            IR::Instr *predInstr = pred->GetLastInstr();
            IR::Instr *instrCopy = instr->Copy();

            if (predInstr->EndsBasicBlock())
            {LOGMEIN("BackwardPass.cpp] 7141\n");
                if (predInstr->m_prev->m_opcode == Js::OpCode::BailOnNoProfile)
                {LOGMEIN("BackwardPass.cpp] 7143\n");
                    // We already have one, we don't need a second.
                    instrCopy->Free();
                }
                else if (!predInstr->AsBranchInstr()->m_isSwitchBr)
                {LOGMEIN("BackwardPass.cpp] 7148\n");
                    // Don't put a bailout in the middle of a switch dispatch sequence.
                    // The bytecode offsets are not in order, and it would lead to incorrect
                    // bailout info.
                    instrCopy->m_func = predInstr->m_func;
                    predInstr->InsertBefore(instrCopy);
                }
            }
            else
            {
                if (predInstr->m_opcode == Js::OpCode::BailOnNoProfile)
                {LOGMEIN("BackwardPass.cpp] 7159\n");
                    // We already have one, we don't need a second.
                    instrCopy->Free();
                }
                else
                {
                    instrCopy->m_func = predInstr->m_func;
                    predInstr->InsertAfter(instrCopy);
                    pred->SetLastInstr(instrCopy);
                }
            }
        }
    } NEXT_PREDECESSOR_BLOCK;

    // If we have a BailOnNoProfile in the first block, there must have been at least one path out of this block that always throws.
    // Don't bother keeping the bailout in the first block as there are some issues in restoring the ArgIn bytecode registers on bailout
    // and throw case should be rare enough that it won't matter for perf.
    if (block->GetBlockNum() != 0)
    {LOGMEIN("BackwardPass.cpp] 7177\n");
        blockHeadInstr->isOpHelper = true;
#if DBG
        blockHeadInstr->m_noHelperAssert = true;
#endif
        block->beginsBailOnNoProfile = true;

        instr->m_func = curInstr->m_func;
        curInstr->InsertAfter(instr);

        bool setLastInstr = (curInstr == block->GetLastInstr());
        if (setLastInstr)
        {LOGMEIN("BackwardPass.cpp] 7189\n");
            block->SetLastInstr(instr);
        }

        if (this->DoMarkTempObjects())
        {LOGMEIN("BackwardPass.cpp] 7194\n");
            block->tempObjectTracker->ProcessBailOnNoProfile(instr);
        }
        return false;
    }
    else
    {
        instr->Free();
        return true;
    }
}

bool
BackwardPass::ReverseCopyProp(IR::Instr *instr)
{LOGMEIN("BackwardPass.cpp] 7208\n");
    // Look for :
    //
    //  t1 = instr
    //       [bytecodeuse t1]
    //  t2 = Ld_A t1            >> t1 !upwardExposed
    //
    // Transform into:
    //
    //  t2 = instr
    //
    if (PHASE_OFF(Js::ReverseCopyPropPhase, this->func))
    {LOGMEIN("BackwardPass.cpp] 7220\n");
        return false;
    }
    if (this->tag != Js::DeadStorePhase || this->IsPrePass() || this->IsCollectionPass())
    {LOGMEIN("BackwardPass.cpp] 7224\n");
        return false;
    }
    if (this->func->HasTry())
    {LOGMEIN("BackwardPass.cpp] 7228\n");
        // UpwardExposedUsed info can't be relied on
        return false;
    }

    // Find t2 = Ld_A t1
    switch (instr->m_opcode)
    {LOGMEIN("BackwardPass.cpp] 7235\n");
    case Js::OpCode::Ld_A:
    case Js::OpCode::Ld_I4:
        break;

    default:
        return false;
    }

    if (!instr->GetDst()->IsRegOpnd())
    {LOGMEIN("BackwardPass.cpp] 7245\n");
        return false;
    }
    if (!instr->GetSrc1()->IsRegOpnd())
    {LOGMEIN("BackwardPass.cpp] 7249\n");
        return false;
    }
    if (instr->HasBailOutInfo())
    {LOGMEIN("BackwardPass.cpp] 7253\n");
        return false;
    }

    IR::RegOpnd *dst = instr->GetDst()->AsRegOpnd();
    IR::RegOpnd *src = instr->GetSrc1()->AsRegOpnd();
    IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();

    IR::ByteCodeUsesInstr *byteCodeUseInstr = nullptr;
    StackSym *varSym = src->m_sym;

    if (varSym->IsTypeSpec())
    {LOGMEIN("BackwardPass.cpp] 7265\n");
        varSym = varSym->GetVarEquivSym(this->func);
    }

    // SKip ByteCodeUse instr if possible
    //       [bytecodeuse t1]
    if (!instrPrev->GetDst())
    {LOGMEIN("BackwardPass.cpp] 7272\n");
        if (instrPrev->m_opcode == Js::OpCode::ByteCodeUses)
        {LOGMEIN("BackwardPass.cpp] 7274\n");
            byteCodeUseInstr = instrPrev->AsByteCodeUsesInstr();
            const BVSparse<JitArenaAllocator>* byteCodeUpwardExposedUsed = byteCodeUseInstr->GetByteCodeUpwardExposedUsed();
            if (byteCodeUpwardExposedUsed && byteCodeUpwardExposedUsed->Test(varSym->m_id) && byteCodeUpwardExposedUsed->Count() == 1)
            {LOGMEIN("BackwardPass.cpp] 7278\n");
                instrPrev = byteCodeUseInstr->GetPrevRealInstrOrLabel();

                if (!instrPrev->GetDst())
                {LOGMEIN("BackwardPass.cpp] 7282\n");
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }

    // The fast-path for these doesn't handle dst == src.
    // REVIEW: I believe the fast-path for LdElemI_A has been fixed... Nope, still broken for "i = A[i]" for prejit
    switch (instrPrev->m_opcode)
    {LOGMEIN("BackwardPass.cpp] 7300\n");
    case Js::OpCode::LdElemI_A:
    case Js::OpCode::IsInst:
    case Js::OpCode::ByteCodeUses:
        return false;
    }

    // Can't do it if post-op bailout would need result
    // REVIEW: enable for pre-opt bailout?
    if (instrPrev->HasBailOutInfo() && instrPrev->GetByteCodeOffset() != instrPrev->GetBailOutInfo()->bailOutOffset)
    {LOGMEIN("BackwardPass.cpp] 7310\n");
        return false;
    }

    // Make sure src of Ld_A == dst of instr
    //  t1 = instr
    if (!instrPrev->GetDst()->IsEqual(src))
    {LOGMEIN("BackwardPass.cpp] 7317\n");
        return false;
    }

    // Make sure t1 isn't used later
    if (this->currentBlock->upwardExposedUses->Test(src->m_sym->m_id))
    {LOGMEIN("BackwardPass.cpp] 7323\n");
        return false;
    }

    if (this->currentBlock->byteCodeUpwardExposedUsed && this->currentBlock->byteCodeUpwardExposedUsed->Test(varSym->m_id))
    {LOGMEIN("BackwardPass.cpp] 7328\n");
        return false;
    }

    // Make sure we can dead-store this sym (debugger mode?)
    if (!this->DoDeadStore(this->func, src->m_sym))
    {LOGMEIN("BackwardPass.cpp] 7334\n");
        return false;
    }

    StackSym *const dstSym = dst->m_sym;
    if(instrPrev->HasBailOutInfo() && dstSym->IsInt32() && dstSym->IsTypeSpec())
    {LOGMEIN("BackwardPass.cpp] 7340\n");
        StackSym *const prevDstSym = IR::RegOpnd::TryGetStackSym(instrPrev->GetDst());
        if(instrPrev->GetBailOutKind() & IR::BailOutOnResultConditions &&
            prevDstSym &&
            prevDstSym->IsInt32() &&
            prevDstSym->IsTypeSpec() &&
            instrPrev->GetSrc1() &&
            !instrPrev->GetDst()->IsEqual(instrPrev->GetSrc1()) &&
            !(instrPrev->GetSrc2() && instrPrev->GetDst()->IsEqual(instrPrev->GetSrc2())))
        {
            // The previous instruction's dst value may be trashed by the time of the pre-op bailout. Skip reverse copy-prop if
            // it would replace the previous instruction's dst with a sym that bailout had decided to use to restore a value for
            // the pre-op bailout, which can't be trashed before bailout. See big comment in ProcessBailOutCopyProps for the
            // reasoning behind the tests above.
            FOREACH_SLISTBASE_ENTRY(
                CopyPropSyms,
                usedCopyPropSym,
                &instrPrev->GetBailOutInfo()->usedCapturedValues.copyPropSyms)
            {LOGMEIN("BackwardPass.cpp] 7358\n");
                if(dstSym == usedCopyPropSym.Value())
                {LOGMEIN("BackwardPass.cpp] 7360\n");
                    return false;
                }
            } NEXT_SLISTBASE_ENTRY;
        }
    }

    if (byteCodeUseInstr)
    {LOGMEIN("BackwardPass.cpp] 7368\n");
        if (this->currentBlock->byteCodeUpwardExposedUsed && instrPrev->GetDst()->AsRegOpnd()->GetIsJITOptimizedReg() && varSym->HasByteCodeRegSlot())
        {LOGMEIN("BackwardPass.cpp] 7370\n");
            if(varSym->HasByteCodeRegSlot())
            {LOGMEIN("BackwardPass.cpp] 7372\n");
                this->currentBlock->byteCodeUpwardExposedUsed->Set(varSym->m_id);
            }

            if (src->IsEqual(dst) && instrPrev->GetDst()->GetIsJITOptimizedReg())
            {LOGMEIN("BackwardPass.cpp] 7377\n");
                //      s2(s1).i32     =  FromVar        s1.var      #0000  Bailout: #0000 (BailOutIntOnly)
                //                        ByteCodeUses   s1
                //      s2(s1).i32     =  Ld_A           s2(s1).i32
                //
                // Since the dst on the FromVar is marked JITOptimized, we need to set it on the new dst as well,
                // or we'll change the bytecode liveness of s1

                dst->SetIsJITOptimizedReg(true);
            }
        }
        byteCodeUseInstr->Remove();
    }
    else if (instrPrev->GetDst()->AsRegOpnd()->GetIsJITOptimizedReg() && !src->GetIsJITOptimizedReg() && varSym->HasByteCodeRegSlot())
    {LOGMEIN("BackwardPass.cpp] 7391\n");
        this->currentBlock->byteCodeUpwardExposedUsed->Set(varSym->m_id);
    }

#if DBG
    if (this->DoMarkTempObjectVerify())
    {LOGMEIN("BackwardPass.cpp] 7397\n");
        this->currentBlock->tempObjectVerifyTracker->NotifyReverseCopyProp(instrPrev);
    }
#endif

    dst->SetValueType(instrPrev->GetDst()->GetValueType());
    instrPrev->ReplaceDst(dst);

    instr->Remove();

    return true;
}

bool
BackwardPass::FoldCmBool(IR::Instr *instr)
{LOGMEIN("BackwardPass.cpp] 7412\n");
    Assert(instr->m_opcode == Js::OpCode::Conv_Bool);

    if (this->tag != Js::DeadStorePhase || this->IsPrePass() || this->IsCollectionPass())
    {LOGMEIN("BackwardPass.cpp] 7416\n");
        return false;
    }
    if (this->func->HasTry())
    {LOGMEIN("BackwardPass.cpp] 7420\n");
        // UpwardExposedUsed info can't be relied on
        return false;
    }

    IR::RegOpnd *intOpnd = instr->GetSrc1()->AsRegOpnd();

    Assert(intOpnd->m_sym->IsInt32());

    if (!intOpnd->m_sym->IsSingleDef())
    {LOGMEIN("BackwardPass.cpp] 7430\n");
        return false;
    }

    IR::Instr *cmInstr = intOpnd->m_sym->GetInstrDef();

    // Should be a Cm instr...
    if (!cmInstr->GetSrc2())
    {LOGMEIN("BackwardPass.cpp] 7438\n");
        return false;
    }

    IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();

    if (instrPrev != cmInstr)
    {LOGMEIN("BackwardPass.cpp] 7445\n");
        return false;
    }

    switch (cmInstr->m_opcode)
    {LOGMEIN("BackwardPass.cpp] 7450\n");
    case Js::OpCode::CmEq_A:
    case Js::OpCode::CmGe_A:
    case Js::OpCode::CmUnGe_A:
    case Js::OpCode::CmGt_A:
    case Js::OpCode::CmUnGt_A:
    case Js::OpCode::CmLt_A:
    case Js::OpCode::CmUnLt_A:
    case Js::OpCode::CmLe_A:
    case Js::OpCode::CmUnLe_A:
    case Js::OpCode::CmNeq_A:
    case Js::OpCode::CmSrEq_A:
    case Js::OpCode::CmSrNeq_A:
    case Js::OpCode::CmEq_I4:
    case Js::OpCode::CmNeq_I4:
    case Js::OpCode::CmLt_I4:
    case Js::OpCode::CmLe_I4:
    case Js::OpCode::CmGt_I4:
    case Js::OpCode::CmGe_I4:
    case Js::OpCode::CmUnLt_I4:
    case Js::OpCode::CmUnLe_I4:
    case Js::OpCode::CmUnGt_I4:
    case Js::OpCode::CmUnGe_I4:
        break;

    default:
        return false;
    }

    IR::RegOpnd *varDst = instr->GetDst()->AsRegOpnd();

    if (this->currentBlock->upwardExposedUses->Test(intOpnd->m_sym->m_id) || !this->currentBlock->upwardExposedUses->Test(varDst->m_sym->m_id))
    {LOGMEIN("BackwardPass.cpp] 7482\n");
        return false;
    }

    varDst = instr->UnlinkDst()->AsRegOpnd();

    cmInstr->ReplaceDst(varDst);

    this->currentBlock->RemoveInstr(instr);

    return true;
}

void
BackwardPass::SetWriteThroughSymbolsSetForRegion(BasicBlock * catchBlock, Region * tryRegion)
{LOGMEIN("BackwardPass.cpp] 7497\n");
    tryRegion->writeThroughSymbolsSet = JitAnew(this->func->m_alloc, BVSparse<JitArenaAllocator>, this->func->m_alloc);

    if (this->DoByteCodeUpwardExposedUsed())
    {LOGMEIN("BackwardPass.cpp] 7501\n");
        Assert(catchBlock->byteCodeUpwardExposedUsed);
        if (!catchBlock->byteCodeUpwardExposedUsed->IsEmpty())
        {
            FOREACH_BITSET_IN_SPARSEBV(id, catchBlock->byteCodeUpwardExposedUsed)
            {LOGMEIN("BackwardPass.cpp] 7506\n");
                tryRegion->writeThroughSymbolsSet->Set(id);
            }
            NEXT_BITSET_IN_SPARSEBV
        }
#if DBG
        // Symbols write-through in the parent try region should be marked as write-through in the current try region as well.
        // x =
        // try{
        //      try{
        //          x =         <-- x needs to be write-through here. With the current mechanism of not clearing a write-through
        //                          symbol from the bytecode upward-exposed on a def, x should be marked as write-through as
        //                          write-through symbols for a try are basically the bytecode upward exposed symbols at the
        //                          beginning of the corresponding catch block).
        //                          Verify that it still holds.
        //          <exception>
        //      }
        //      catch(){}
        //      x =
        // }
        // catch(){}
        // = x
        if (tryRegion->GetParent()->GetType() == RegionTypeTry)
        {LOGMEIN("BackwardPass.cpp] 7529\n");
            Region * parentTry = tryRegion->GetParent();
            Assert(parentTry->writeThroughSymbolsSet);
            FOREACH_BITSET_IN_SPARSEBV(id, parentTry->writeThroughSymbolsSet)
            {LOGMEIN("BackwardPass.cpp] 7533\n");
                Assert(tryRegion->writeThroughSymbolsSet->Test(id));
            }
            NEXT_BITSET_IN_SPARSEBV
        }
#endif
    }
    else
    {
        // this can happen with -off:globopt
        return;
    }
}

bool
BackwardPass::CheckWriteThroughSymInRegion(Region* region, StackSym* sym)
{LOGMEIN("BackwardPass.cpp] 7549\n");
    if (region->GetType() == RegionTypeRoot || region->GetType() == RegionTypeFinally)
    {LOGMEIN("BackwardPass.cpp] 7551\n");
        return false;
    }

    // if the current region is a try region, check in its write-through set,
    // otherwise (current = catch region) look in the first try ancestor's write-through set
    Region * selfOrFirstTryAncestor = region->GetSelfOrFirstTryAncestor();
    if (!selfOrFirstTryAncestor)
    {LOGMEIN("BackwardPass.cpp] 7559\n");
        return false;
    }
    Assert(selfOrFirstTryAncestor->GetType() == RegionTypeTry);
    return selfOrFirstTryAncestor->writeThroughSymbolsSet && selfOrFirstTryAncestor->writeThroughSymbolsSet->Test(sym->m_id);
}

bool
BackwardPass::DoDeadStoreLdStForMemop(IR::Instr *instr)
{LOGMEIN("BackwardPass.cpp] 7568\n");
    Assert(this->tag == Js::DeadStorePhase && this->currentBlock->loop != nullptr);

    Loop *loop = this->currentBlock->loop;

    if (globOpt->HasMemOp(loop))
    {LOGMEIN("BackwardPass.cpp] 7574\n");
        if (instr->m_opcode == Js::OpCode::StElemI_A && instr->GetDst()->IsIndirOpnd())
        {LOGMEIN("BackwardPass.cpp] 7576\n");
            SymID base = this->globOpt->GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym());
            SymID index = this->globOpt->GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym());

            FOREACH_MEMOP_CANDIDATES(candidate, loop)
            {LOGMEIN("BackwardPass.cpp] 7581\n");
                if (base == candidate->base && index == candidate->index)
                {LOGMEIN("BackwardPass.cpp] 7583\n");
                    return true;
                }
            } NEXT_MEMOP_CANDIDATE
        }
        else if (instr->m_opcode == Js::OpCode::LdElemI_A &&  instr->GetSrc1()->IsIndirOpnd())
        {LOGMEIN("BackwardPass.cpp] 7589\n");
            SymID base = this->globOpt->GetVarSymID(instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym());
            SymID index = this->globOpt->GetVarSymID(instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym());

            FOREACH_MEMCOPY_CANDIDATES(candidate, loop)
            {LOGMEIN("BackwardPass.cpp] 7594\n");
                if (base == candidate->ldBase && index == candidate->index)
                {LOGMEIN("BackwardPass.cpp] 7596\n");
                    return true;
                }
            } NEXT_MEMCOPY_CANDIDATE
        }
    }
    return false;
}

void
BackwardPass::RestoreInductionVariableValuesAfterMemOp(Loop *loop)
{LOGMEIN("BackwardPass.cpp] 7607\n");
    const auto RestoreInductionVariable = [&](SymID symId, Loop::InductionVariableChangeInfo inductionVariableChangeInfo, Loop *loop)
    {LOGMEIN("BackwardPass.cpp] 7609\n");
        Js::OpCode opCode = Js::OpCode::Add_I4;
        if (!inductionVariableChangeInfo.isIncremental)
        {LOGMEIN("BackwardPass.cpp] 7612\n");
            opCode = Js::OpCode::Sub_I4;
        }
        Func *localFunc = loop->GetFunc();
        StackSym *sym = localFunc->m_symTable->FindStackSym(symId)->GetInt32EquivSym(localFunc);

        IR::Opnd *inductionVariableOpnd = IR::RegOpnd::New(sym, IRType::TyInt32, localFunc);
        IR::Opnd *sizeOpnd = globOpt->GenerateInductionVariableChangeForMemOp(loop, inductionVariableChangeInfo.unroll);
        loop->landingPad->InsertAfter(IR::Instr::New(opCode, inductionVariableOpnd, inductionVariableOpnd, sizeOpnd, loop->GetFunc()));
    };

    for (auto it = loop->memOpInfo->inductionVariableChangeInfoMap->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("BackwardPass.cpp] 7624\n");
        Loop::InductionVariableChangeInfo iv = it.CurrentValue();
        SymID sym = it.CurrentKey();
        if (iv.unroll != Js::Constants::InvalidLoopUnrollFactor)
        {LOGMEIN("BackwardPass.cpp] 7628\n");
            // if the variable is being used after the loop restore it
            if (loop->memOpInfo->inductionVariablesUsedAfterLoop->Test(sym))
            {
                RestoreInductionVariable(sym, iv, loop);
            }
        }
    }
}

bool
BackwardPass::IsEmptyLoopAfterMemOp(Loop *loop)
{LOGMEIN("BackwardPass.cpp] 7640\n");
    if (globOpt->HasMemOp(loop))
    {LOGMEIN("BackwardPass.cpp] 7642\n");
        const auto IsInductionVariableUse = [&](IR::Opnd *opnd) -> bool
        {LOGMEIN("BackwardPass.cpp] 7644\n");
            Loop::InductionVariableChangeInfo  inductionVariableChangeInfo = { 0, 0 };
            return (opnd &&
                opnd->GetStackSym() &&
                loop->memOpInfo->inductionVariableChangeInfoMap->ContainsKey(this->globOpt->GetVarSymID(opnd->GetStackSym())) &&
                (((Loop::InductionVariableChangeInfo)
                    loop->memOpInfo->inductionVariableChangeInfoMap->
                    LookupWithKey(this->globOpt->GetVarSymID(opnd->GetStackSym()), inductionVariableChangeInfo)).unroll != Js::Constants::InvalidLoopUnrollFactor));
        };

        Assert(loop->blockList.HasTwo());

        FOREACH_BLOCK_IN_LOOP(bblock, loop)
        {

            FOREACH_INSTR_IN_BLOCK_EDITING(instr, instrPrev, bblock)
            {LOGMEIN("BackwardPass.cpp] 7660\n");
                if (instr->IsLabelInstr() || !instr->IsRealInstr() || instr->m_opcode == Js::OpCode::IncrLoopBodyCount || instr->m_opcode == Js::OpCode::StLoopBodyCount
                    || (instr->IsBranchInstr() && instr->AsBranchInstr()->IsUnconditional()))
                {LOGMEIN("BackwardPass.cpp] 7663\n");
                    continue;
                }
                else
                {
                    switch (instr->m_opcode)
                    {LOGMEIN("BackwardPass.cpp] 7669\n");
                    case Js::OpCode::Nop:
                        break;
                    case Js::OpCode::Ld_I4:
                    case Js::OpCode::Add_I4:
                    case Js::OpCode::Sub_I4:

                        if (!IsInductionVariableUse(instr->GetDst()))
                        {LOGMEIN("BackwardPass.cpp] 7677\n");
                            Assert(instr->GetDst());
                            if (instr->GetDst()->GetStackSym()
                                && loop->memOpInfo->inductionVariablesUsedAfterLoop->Test(globOpt->GetVarSymID(instr->GetDst()->GetStackSym())))
                            {LOGMEIN("BackwardPass.cpp] 7681\n");
                                // We have use after the loop for a variable defined inside the loop. So the loop can't be removed.
                                return false;
                            }
                        }
                        break;
                    case Js::OpCode::Decr_A:
                    case Js::OpCode::Incr_A:
                        if (!IsInductionVariableUse(instr->GetSrc1()))
                        {LOGMEIN("BackwardPass.cpp] 7690\n");
                            return false;
                        }
                        break;
                    default:
                        if (instr->IsBranchInstr())
                        {LOGMEIN("BackwardPass.cpp] 7696\n");
                            if (IsInductionVariableUse(instr->GetSrc1()) || IsInductionVariableUse(instr->GetSrc2()))
                            {LOGMEIN("BackwardPass.cpp] 7698\n");
                                break;
                            }
                        }
                        return false;
                    }
                }

            }
            NEXT_INSTR_IN_BLOCK_EDITING;

        }NEXT_BLOCK_IN_LIST;

        return true;
    }

    return false;
}

void
BackwardPass::RemoveEmptyLoops()
{LOGMEIN("BackwardPass.cpp] 7719\n");
    if (PHASE_OFF(Js::MemOpPhase, this->func))
    {LOGMEIN("BackwardPass.cpp] 7721\n");
        return;

    }
    const auto DeleteMemOpInfo = [&](Loop *loop)
    {LOGMEIN("BackwardPass.cpp] 7726\n");
        JitArenaAllocator *alloc = this->func->GetTopFunc()->m_fg->alloc;

        if (!loop->memOpInfo)
        {LOGMEIN("BackwardPass.cpp] 7730\n");
            return;
        }

        if (loop->memOpInfo->candidates)
        {LOGMEIN("BackwardPass.cpp] 7735\n");
            loop->memOpInfo->candidates->Clear();
            JitAdelete(alloc, loop->memOpInfo->candidates);
        }

        if (loop->memOpInfo->inductionVariableChangeInfoMap)
        {LOGMEIN("BackwardPass.cpp] 7741\n");
            loop->memOpInfo->inductionVariableChangeInfoMap->Clear();
            JitAdelete(alloc, loop->memOpInfo->inductionVariableChangeInfoMap);
        }

        if (loop->memOpInfo->inductionVariableOpndPerUnrollMap)
        {LOGMEIN("BackwardPass.cpp] 7747\n");
            loop->memOpInfo->inductionVariableOpndPerUnrollMap->Clear();
            JitAdelete(alloc, loop->memOpInfo->inductionVariableOpndPerUnrollMap);
        }

        if (loop->memOpInfo->inductionVariablesUsedAfterLoop)
        {LOGMEIN("BackwardPass.cpp] 7753\n");
            JitAdelete(this->tempAlloc, loop->memOpInfo->inductionVariablesUsedAfterLoop);
        }
        JitAdelete(alloc, loop->memOpInfo);
    };

    FOREACH_LOOP_IN_FUNC_EDITING(loop, this->func)
    {LOGMEIN("BackwardPass.cpp] 7760\n");
        if (IsEmptyLoopAfterMemOp(loop))
        {LOGMEIN("BackwardPass.cpp] 7762\n");
            RestoreInductionVariableValuesAfterMemOp(loop);
            RemoveEmptyLoopAfterMemOp(loop);
        }
        // Remove memop info as we don't need them after this point.
        DeleteMemOpInfo(loop);

    } NEXT_LOOP_IN_FUNC_EDITING;

}

void
BackwardPass::RemoveEmptyLoopAfterMemOp(Loop *loop)
{LOGMEIN("BackwardPass.cpp] 7775\n");
    BasicBlock *head = loop->GetHeadBlock();
    BasicBlock *tail = head->next;
    BasicBlock *landingPad = loop->landingPad;
    BasicBlock *outerBlock = nullptr;
    SListBaseCounted<FlowEdge *> *succList = head->GetSuccList();
    Assert(succList->HasTwo());

    // Between the two successors of head, one is tail and the other one is the outerBlock
    SListBaseCounted<FlowEdge *>::Iterator  iter(succList);
    iter.Next();
    if (iter.Data()->GetSucc() == tail)
    {LOGMEIN("BackwardPass.cpp] 7787\n");
        iter.Next();
        outerBlock = iter.Data()->GetSucc();
    }
    else
    {
        outerBlock = iter.Data()->GetSucc();
#ifdef DBG
        iter.Next();
        Assert(iter.Data()->GetSucc() == tail);
#endif
    }

    outerBlock->RemovePred(head, this->func->m_fg);
    landingPad->RemoveSucc(head, this->func->m_fg);
    this->func->m_fg->AddEdge(landingPad, outerBlock);

    this->func->m_fg->RemoveBlock(head, nullptr);

    if (head != tail)
    {LOGMEIN("BackwardPass.cpp] 7807\n");
        this->func->m_fg->RemoveBlock(tail, nullptr);
    }
}

#if DBG_DUMP
bool
BackwardPass::IsTraceEnabled() const
{LOGMEIN("BackwardPass.cpp] 7815\n");
    return
        Js::Configuration::Global.flags.Trace.IsEnabled(tag, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()) &&
        (PHASE_TRACE(Js::SimpleJitPhase, func) || !func->IsSimpleJit());
}
#endif
