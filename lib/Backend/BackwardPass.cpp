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
{TRACE_IT(80);
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
{TRACE_IT(81);
    // Note: Dead bit on the Opnd records flow-based liveness.
    // This is distinct from isLastUse, which records lexical last-ness.
    if (isDead && this->tag == Js::BackwardPhase && !this->IsPrePass())
    {TRACE_IT(82);
        opnd->SetIsDead();
    }
    else if (this->tag == Js::DeadStorePhase)
    {TRACE_IT(83);
        // Set or reset in DeadStorePhase.
        // CSE could make previous dead operand not the last use, so reset it.
        opnd->SetIsDead(isDead);
    }
}

bool
BackwardPass::DoByteCodeUpwardExposedUsed() const
{TRACE_IT(84);
    return (this->tag == Js::DeadStorePhase && this->func->hasBailout) ||
        (this->func->HasTry() && this->func->DoOptimizeTryCatch() && this->tag == Js::BackwardPhase);
}

bool
BackwardPass::DoFieldHoistCandidates() const
{TRACE_IT(85);
    return DoFieldHoistCandidates(this->currentBlock->loop);
}

bool
BackwardPass::DoFieldHoistCandidates(Loop * loop) const
{TRACE_IT(86);
    // We only need to do one pass to generate this data
    return this->tag == Js::BackwardPhase
        && !this->IsPrePass() && loop && GlobOpt::DoFieldHoisting(loop);
}

bool
BackwardPass::DoMarkTempNumbers() const
{TRACE_IT(87);
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
{TRACE_IT(88);
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
{TRACE_IT(89);
    return !PHASE_OFF(Js::MarkTempNumberOnTempObjectPhase, this->func) && DoMarkTempNumbers() && this->func->GetHasMarkTempObjects();
}

#if DBG
bool
BackwardPass::DoMarkTempObjectVerify() const
{TRACE_IT(90);
    // only mark temp object on the backward store phase
    return (tag == Js::DeadStorePhase) && !PHASE_OFF(Js::MarkTempPhase, this->func) &&
        !PHASE_OFF(Js::MarkTempObjectPhase, this->func) && func->DoGlobOpt() && func->GetHasTempObjectProducingInstr();
}
#endif

// static
bool
BackwardPass::DoDeadStore(Func* func)
{TRACE_IT(91);
    return
        !PHASE_OFF(Js::DeadStorePhase, func) &&
        (!func->HasTry() || func->DoOptimizeTryCatch());
}

bool
BackwardPass::DoDeadStore() const
{TRACE_IT(92);
    return
        this->tag == Js::DeadStorePhase &&
        DoDeadStore(this->func);
}

bool
BackwardPass::DoDeadStoreSlots() const
{TRACE_IT(93);
    // only dead store fields if glob opt is on to generate the trackable fields bitvector
    return (tag == Js::DeadStorePhase && this->func->DoGlobOpt()
        && (!this->func->HasTry()));
}

// Whether dead store is enabled for given func and sym.
// static
bool
BackwardPass::DoDeadStore(Func* func, StackSym* sym)
{TRACE_IT(94);
    // Dead store is disabled under debugger for non-temp local vars.
    return
        DoDeadStore(func) &&
        !(func->IsJitInDebugMode() && sym->HasByteCodeRegSlot() && func->IsNonTempLocalVar(sym->GetByteCodeRegSlot())) &&
        func->DoGlobOptsForGeneratorFunc();
}

bool
BackwardPass::DoTrackNegativeZero() const
{TRACE_IT(95);
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
{TRACE_IT(96);
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
{TRACE_IT(97);
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
{TRACE_IT(98);
    return
        !PHASE_OFF(Js::TrackCompoundedIntOverflowPhase, func) &&
        DoTrackIntOverflow() &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsTrackCompoundedIntOverflowDisabled());
}

bool
BackwardPass::DoTrackNon32BitOverflow() const
{TRACE_IT(99);
    // enabled only for IA
#if defined(_M_IX86) || defined(_M_X64)
    return true;
#else
    return false;
#endif
}

void
BackwardPass::CleanupBackwardPassInfoInFlowGraph()
{TRACE_IT(100);
    if (!this->func->m_fg->hasBackwardPassInfo)
    {TRACE_IT(101);
        // No information to clean up
        return;
    }

    // The backward pass temp arena has already been deleted, we can just reset the data

    FOREACH_BLOCK_IN_FUNC_DEAD_OR_ALIVE(block, this->func)
    {TRACE_IT(102);
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
        {TRACE_IT(103);
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
{TRACE_IT(104);
    if (func->IsStackArgsEnabled() && !func->GetJITFunctionBody()->HasImplicitArgIns())
    {TRACE_IT(105);
        IR::Instr * insertAfterInstr = func->m_headInstr->m_next;
        AssertMsg(insertAfterInstr->IsLabelInstr(), "First Instr of the first block should always have a label");

        Js::ArgSlot paramsCount = insertAfterInstr->m_func->GetJITFunctionBody()->GetInParamsCount() - 1;
        IR::Instr *     argInInstr = nullptr;
        for (Js::ArgSlot argumentIndex = 1; argumentIndex <= paramsCount; argumentIndex++)
        {TRACE_IT(106);
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
        {TRACE_IT(107);
            Output::Print(_u("StackArgFormals : %s (%d) :Inserting ArgIn_A for LdSlot (formals) in the start of Deadstore pass. \n"), func->GetJITFunctionBody()->GetDisplayName(), func->GetFunctionNumber());
            Output::Flush();
        }
    }
}

void
BackwardPass::MarkScopeObjSymUseForStackArgOpt()
{TRACE_IT(108);
    IR::Instr * instr = this->currentInstr;
    if (tag == Js::DeadStorePhase)
    {TRACE_IT(109);
        if (instr->DoStackArgsOpt(this->func) && instr->m_func->GetScopeObjSym() != nullptr)
        {TRACE_IT(110);
            if (this->currentBlock->byteCodeUpwardExposedUsed == nullptr)
            {TRACE_IT(111);
                this->currentBlock->byteCodeUpwardExposedUsed = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
            this->currentBlock->byteCodeUpwardExposedUsed->Set(instr->m_func->GetScopeObjSym()->m_id);
        }
    }
}

void
BackwardPass::ProcessBailOnStackArgsOutOfActualsRange()
{TRACE_IT(112);
    IR::Instr * instr = this->currentInstr;

    if (tag == Js::DeadStorePhase && 
        (instr->m_opcode == Js::OpCode::LdElemI_A || instr->m_opcode == Js::OpCode::TypeofElem) && 
        instr->HasBailOutInfo() && !IsPrePass())
    {TRACE_IT(113);
        if (instr->DoStackArgsOpt(this->func))
        {TRACE_IT(114);
            AssertMsg(instr->GetBailOutKind() & IR::BailOnStackArgsOutOfActualsRange, "Stack args bail out is not set when the optimization is turned on? ");
            if (instr->GetBailOutKind() & ~IR::BailOnStackArgsOutOfActualsRange)
            {TRACE_IT(115);
                Assert(instr->GetBailOutKind() == (IR::BailOnStackArgsOutOfActualsRange | IR::BailOutOnImplicitCallsPreOp));
                //We are sure at this point, that we will not have any implicit calls as we wouldn't have done this optimization in the first place.
                instr->SetBailOutKind(IR::BailOnStackArgsOutOfActualsRange);
            }
        }
        else if (instr->GetBailOutKind() & IR::BailOnStackArgsOutOfActualsRange)
        {TRACE_IT(116);
            //If we don't decide to do StackArgs, then remove the bail out at this point.
            //We would have optimistically set the bailout in the forward pass, and by the end of forward pass - we
            //turned off stack args for some reason. So we are removing it in the deadstore pass.
            IR::BailOutKind bailOutKind = instr->GetBailOutKind() & ~IR::BailOnStackArgsOutOfActualsRange;
            if (bailOutKind == IR::BailOutInvalid)
            {TRACE_IT(117);
                instr->ClearBailOutInfo();
            }
            else
            {TRACE_IT(118);
                instr->SetBailOutKind(bailOutKind);
            }
        }
    }
}

void
BackwardPass::Optimize()
{TRACE_IT(119);
    if (tag == Js::BackwardPhase && PHASE_OFF(tag, this->func))
    {TRACE_IT(120);
        return;
    }

    if (tag == Js::DeadStorePhase)
    {TRACE_IT(121);
        if (!this->func->DoLoopFastPaths() || !this->func->DoFastPaths())
        {TRACE_IT(122);
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
    {TRACE_IT(123);
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
    {TRACE_IT(124);
        this->OptBlock(block);
    }
    NEXT_BLOCK_BACKWARD_IN_FUNC_DEAD_OR_ALIVE;

    if (this->tag == Js::DeadStorePhase && !PHASE_OFF(Js::MemOpPhase, this->func))
    {TRACE_IT(125);
        this->RemoveEmptyLoops();
    }
    this->func->m_fg->hasBackwardPassInfo = true;

    if(DoTrackCompoundedIntOverflow())
    {TRACE_IT(126);
        // Tracking int overflow makes use of a scratch field in stack syms, which needs to be cleared
        func->m_symTable->ClearStackSymScratch();
    }

#if DBG_DUMP
    if (PHASE_STATS(this->tag, this->func))
    {TRACE_IT(127);
        this->func->DumpHeader();
        Output::Print(this->tag == Js::BackwardPhase? _u("Backward Phase Stats:\n") : _u("Deadstore Phase Stats:\n"));
        if (this->DoDeadStore())
        {TRACE_IT(128);
            Output::Print(_u("  Deadstore              : %3d\n"), this->numDeadStore);
        }
        if (this->DoMarkTempNumbers())
        {TRACE_IT(129);
            Output::Print(_u("  Temp Number            : %3d\n"), this->numMarkTempNumber);
            Output::Print(_u("  Transferred Temp Number: %3d\n"), this->numMarkTempNumberTransferred);
        }
        if (this->DoMarkTempObjects())
        {TRACE_IT(130);
            Output::Print(_u("  Temp Object            : %3d\n"), this->numMarkTempObject);
        }
    }
#endif
}

void
BackwardPass::MergeSuccBlocksInfo(BasicBlock * block)
{TRACE_IT(131);
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
    {TRACE_IT(132);
        byteCodeUpwardExposedUsed = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
#if DBG
        byteCodeRestoreSyms = JitAnewArrayZ(this->tempAlloc, StackSym *, byteCodeLocalsCount);
#endif
    }

#if DBG
    if (!IsCollectionPass() && this->DoMarkTempObjectVerify())
    {TRACE_IT(133);
        tempObjectVerifyTracker = JitAnew(this->tempAlloc, TempObjectVerifyTracker, this->tempAlloc, block->loop != nullptr);
    }
#endif

    if (!block->isDead)
    {TRACE_IT(134);
        bool keepUpwardExposed = (this->tag == Js::BackwardPhase);
        JitArenaAllocator *upwardExposedArena = nullptr;
        if(!IsCollectionPass())
        {TRACE_IT(135);
            upwardExposedArena = keepUpwardExposed ? this->globOpt->alloc : this->tempAlloc;
            upwardExposedUses = JitAnew(upwardExposedArena, BVSparse<JitArenaAllocator>, upwardExposedArena);
            upwardExposedFields = JitAnew(upwardExposedArena, BVSparse<JitArenaAllocator>, upwardExposedArena);

            if (this->tag == Js::DeadStorePhase)
            {TRACE_IT(136);
                typesNeedingKnownObjectLayout = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }

            if (this->DoFieldHoistCandidates())
            {TRACE_IT(137);
                fieldHoistCandidates = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
            if (this->DoDeadStoreSlots())
            {TRACE_IT(138);
                slotDeadStoreCandidates = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
            if (this->DoMarkTempNumbers())
            {TRACE_IT(139);
                tempNumberTracker = JitAnew(this->tempAlloc, TempNumberTracker, this->tempAlloc,  block->loop != nullptr);
            }
            if (this->DoMarkTempObjects())
            {TRACE_IT(140);
                tempObjectTracker = JitAnew(this->tempAlloc, TempObjectTracker, this->tempAlloc, block->loop != nullptr);
            }

            noImplicitCallUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallNoMissingValuesUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallNativeArrayUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallJsArrayHeadSegmentSymUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            noImplicitCallArrayLengthSymUses = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            if (this->tag == Js::BackwardPhase)
            {TRACE_IT(141);
                cloneStrCandidates = JitAnew(this->globOpt->alloc, BVSparse<JitArenaAllocator>, this->globOpt->alloc);
            }
            else
            {TRACE_IT(142);
                couldRemoveNegZeroBailoutForDef = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            }
        }

        bool firstSucc = true;
        FOREACH_SUCCESSOR_BLOCK(blockSucc, block)
        {TRACE_IT(143);
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)

            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
            // save the byteCodeUpwardExposedUsed from deleting for the block right after the memop loop
            if (this->tag == Js::DeadStorePhase && !this->IsPrePass() && globOpt->HasMemOp(block->loop) && blockSucc->loop != block->loop)
            {TRACE_IT(144);
                Assert(block->loop->memOpInfo->inductionVariablesUsedAfterLoop == nullptr);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop->Or(blockSucc->byteCodeUpwardExposedUsed);
                block->loop->memOpInfo->inductionVariablesUsedAfterLoop->Or(blockSucc->upwardExposedUses);
            }

            bool deleteData = false;
            if (!blockSucc->isLoopHeader && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
            {TRACE_IT(145);
                Assert(blockSucc->GetDataUseCount() != 0);
                deleteData = (blockSucc->DecrementDataUseCount() == 0);
            }

            Assert((byteCodeUpwardExposedUsed == nullptr) == !this->DoByteCodeUpwardExposedUsed());
            if (byteCodeUpwardExposedUsed && blockSucc->byteCodeUpwardExposedUsed)
            {TRACE_IT(146);
                byteCodeUpwardExposedUsed->Or(blockSucc->byteCodeUpwardExposedUsed);
                if (this->tag == Js::DeadStorePhase)
                {TRACE_IT(147);
#if DBG
                    for (uint i = 0; i < byteCodeLocalsCount; i++)
                    {TRACE_IT(148);
                        if (byteCodeRestoreSyms[i] == nullptr)
                        {TRACE_IT(149);
                            byteCodeRestoreSyms[i] = blockSucc->byteCodeRestoreSyms[i];
                        }
                        else
                        {TRACE_IT(150);
                            Assert(blockSucc->byteCodeRestoreSyms[i] == nullptr
                                || byteCodeRestoreSyms[i] == blockSucc->byteCodeRestoreSyms[i]);
                        }
                    }
#endif
                    if (deleteData)
                    {TRACE_IT(151);
                        // byteCodeUpwardExposedUsed is required to populate the writeThroughSymbolsSet for the try region. So, don't delete it in the backwards pass.
                        JitAdelete(this->tempAlloc, blockSucc->byteCodeUpwardExposedUsed);
                        blockSucc->byteCodeUpwardExposedUsed = nullptr;
                    }
                }
#if DBG
                if (deleteData)
                {TRACE_IT(152);
                    JitAdeleteArray(this->tempAlloc, byteCodeLocalsCount, blockSucc->byteCodeRestoreSyms);
                    blockSucc->byteCodeRestoreSyms = nullptr;
                }
#endif

            }
            else
            {TRACE_IT(153);
                Assert(blockSucc->byteCodeUpwardExposedUsed == nullptr);
                Assert(blockSucc->byteCodeRestoreSyms == nullptr);
            }

            if(IsCollectionPass())
            {TRACE_IT(154);
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
            {TRACE_IT(155);
                upwardExposedUses->Or(blockSucc->upwardExposedUses);

                if (deleteData && (!keepUpwardExposed
                                   || (this->IsPrePass() && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)))
                {
                    JitAdelete(upwardExposedArena, blockSucc->upwardExposedUses);
                    blockSucc->upwardExposedUses = nullptr;
                }
            }

            if (blockSucc->upwardExposedFields != nullptr)
            {TRACE_IT(156);
                upwardExposedFields->Or(blockSucc->upwardExposedFields);

                if (deleteData && (!keepUpwardExposed
                                   || (this->IsPrePass() && blockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)))
                {
                    JitAdelete(upwardExposedArena, blockSucc->upwardExposedFields);
                    blockSucc->upwardExposedFields = nullptr;
                }
            }

            if (blockSucc->typesNeedingKnownObjectLayout != nullptr)
            {TRACE_IT(157);
                typesNeedingKnownObjectLayout->Or(blockSucc->typesNeedingKnownObjectLayout);
                if (deleteData)
                {TRACE_IT(158);
                    JitAdelete(this->tempAlloc, blockSucc->typesNeedingKnownObjectLayout);
                    blockSucc->typesNeedingKnownObjectLayout = nullptr;
                }
            }

            if (fieldHoistCandidates && blockSucc->fieldHoistCandidates != nullptr)
            {TRACE_IT(159);
                fieldHoistCandidates->Or(blockSucc->fieldHoistCandidates);
                if (deleteData)
                {TRACE_IT(160);
                    JitAdelete(this->tempAlloc, blockSucc->fieldHoistCandidates);
                    blockSucc->fieldHoistCandidates = nullptr;
                }
            }
            if (blockSucc->slotDeadStoreCandidates != nullptr)
            {TRACE_IT(161);
                slotDeadStoreCandidates->And(blockSucc->slotDeadStoreCandidates);
                if (deleteData)
                {TRACE_IT(162);
                    JitAdelete(this->tempAlloc, blockSucc->slotDeadStoreCandidates);
                    blockSucc->slotDeadStoreCandidates = nullptr;
                }
            }
            if (blockSucc->tempNumberTracker != nullptr)
            {TRACE_IT(163);
                Assert((blockSucc->loop != nullptr) == blockSucc->tempNumberTracker->HasTempTransferDependencies());
                tempNumberTracker->MergeData(blockSucc->tempNumberTracker, deleteData);
                if (deleteData)
                {TRACE_IT(164);
                    blockSucc->tempNumberTracker = nullptr;
                }
            }
            if (blockSucc->tempObjectTracker != nullptr)
            {TRACE_IT(165);
                Assert((blockSucc->loop != nullptr) == blockSucc->tempObjectTracker->HasTempTransferDependencies());
                tempObjectTracker->MergeData(blockSucc->tempObjectTracker, deleteData);
                if (deleteData)
                {TRACE_IT(166);
                    blockSucc->tempObjectTracker = nullptr;
                }
            }
#if DBG
            if (blockSucc->tempObjectVerifyTracker != nullptr)
            {TRACE_IT(167);
                Assert((blockSucc->loop != nullptr) == blockSucc->tempObjectVerifyTracker->HasTempTransferDependencies());
                tempObjectVerifyTracker->MergeData(blockSucc->tempObjectVerifyTracker, deleteData);
                if (deleteData)
                {TRACE_IT(168);
                    blockSucc->tempObjectVerifyTracker = nullptr;
                }
            }
#endif

            PHASE_PRINT_TRACE(Js::ObjTypeSpecStorePhase, this->func,
                              _u("ObjTypeSpecStore: func %s, edge %d => %d: "),
                              this->func->GetDebugNumberSet(debugStringBuffer),
                              block->GetBlockNum(), blockSucc->GetBlockNum());

            auto fixupFrom = [block, blockSucc, this](Bucket<AddPropertyCacheBucket> &bucket)
            {TRACE_IT(169);
                AddPropertyCacheBucket *fromData = &bucket.element;
                if (fromData->GetInitialType() == nullptr ||
                    fromData->GetFinalType() == fromData->GetInitialType())
                {TRACE_IT(170);
                    return;
                }

                this->InsertTypeTransitionsAtPriorSuccessors(block, blockSucc, bucket.value, fromData);
            };

            auto fixupTo = [blockSucc, this](Bucket<AddPropertyCacheBucket> &bucket)
            {TRACE_IT(171);
                AddPropertyCacheBucket *toData = &bucket.element;
                if (toData->GetInitialType() == nullptr ||
                    toData->GetFinalType() == toData->GetInitialType())
                {TRACE_IT(172);
                    return;
                }

                this->InsertTypeTransitionAtBlock(blockSucc, bucket.value, toData);
            };

            if (blockSucc->stackSymToFinalType != nullptr)
            {TRACE_IT(173);
#if DBG_DUMP
                if (PHASE_TRACE(Js::ObjTypeSpecStorePhase, this->func))
                {TRACE_IT(174);
                    blockSucc->stackSymToFinalType->Dump();
                }
#endif
                if (firstSucc)
                {TRACE_IT(175);
                    stackSymToFinalType = blockSucc->stackSymToFinalType->Copy();
                }
                else if (stackSymToFinalType != nullptr)
                {TRACE_IT(176);
                    if (this->IsPrePass())
                    {TRACE_IT(177);
                        stackSymToFinalType->And(blockSucc->stackSymToFinalType);
                    }
                    else
                    {TRACE_IT(178);
                        // Insert any type transitions that can't be merged past this point.
                        stackSymToFinalType->AndWithFixup(blockSucc->stackSymToFinalType, fixupFrom, fixupTo);
                    }
                }
                else if (!this->IsPrePass())
                {
                    FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, blockSucc->stackSymToFinalType)
                    {TRACE_IT(179);
                        fixupTo(bucket);
                    }
                    NEXT_HASHTABLE_ENTRY;
                }

                if (deleteData)
                {TRACE_IT(180);
                    blockSucc->stackSymToFinalType->Delete();
                    blockSucc->stackSymToFinalType = nullptr;
                }
            }
            else
            {TRACE_IT(181);
                PHASE_PRINT_TRACE(Js::ObjTypeSpecStorePhase, this->func, _u("null\n"));
                if (stackSymToFinalType)
                {TRACE_IT(182);
                    if (!this->IsPrePass())
                    {
                        FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, stackSymToFinalType)
                        {TRACE_IT(183);
                            fixupFrom(bucket);
                        }
                        NEXT_HASHTABLE_ENTRY;
                    }

                    stackSymToFinalType->Delete();
                    stackSymToFinalType = nullptr;
                }
            }
            if (tag == Js::BackwardPhase)
            {TRACE_IT(184);
                if (blockSucc->cloneStrCandidates != nullptr)
                {TRACE_IT(185);
                    Assert(cloneStrCandidates != nullptr);
                    cloneStrCandidates->Or(blockSucc->cloneStrCandidates);
                    if (deleteData)
                    {TRACE_IT(186);
                        JitAdelete(this->globOpt->alloc, blockSucc->cloneStrCandidates);
                        blockSucc->cloneStrCandidates = nullptr;
                    }
                }
#if DBG_DUMP
                if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                {TRACE_IT(187);
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
                {TRACE_IT(188);
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                    {TRACE_IT(189);
                        Output::Print(_u("\n"));
                        blockSucc->stackSymToWriteGuardsMap->Dump();
                    }
#endif
                    if (stackSymToWriteGuardsMap == nullptr)
                    {TRACE_IT(190);
                        stackSymToWriteGuardsMap = blockSucc->stackSymToWriteGuardsMap->Copy();
                    }
                    else
                    {TRACE_IT(191);
                        stackSymToWriteGuardsMap->Or(
                            blockSucc->stackSymToWriteGuardsMap, &BackwardPass::MergeWriteGuards);
                    }

                    if (deleteData)
                    {TRACE_IT(192);
                        blockSucc->stackSymToWriteGuardsMap->Delete();
                        blockSucc->stackSymToWriteGuardsMap = nullptr;
                    }
                }
                else
                {TRACE_IT(193);
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
                    {TRACE_IT(194);
                        Output::Print(_u("null\n"));
                    }
#endif
                }
            }
            else
            {TRACE_IT(195);
#if DBG_DUMP
                if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                {TRACE_IT(196);
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
                {TRACE_IT(197);
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                    {TRACE_IT(198);
                        blockSucc->stackSymToGuardedProperties->Dump();
                        Output::Print(_u("\n"));
                    }
#endif
                    if (stackSymToGuardedProperties == nullptr)
                    {TRACE_IT(199);
                        stackSymToGuardedProperties = blockSucc->stackSymToGuardedProperties->Copy();
                    }
                    else
                    {TRACE_IT(200);
                        stackSymToGuardedProperties->Or(
                            blockSucc->stackSymToGuardedProperties, &BackwardPass::MergeGuardedProperties);
                    }

                    if (deleteData)
                    {TRACE_IT(201);
                        blockSucc->stackSymToGuardedProperties->Delete();
                        blockSucc->stackSymToGuardedProperties = nullptr;
                    }
                }
                else
                {TRACE_IT(202);
#if DBG_DUMP
                    if (PHASE_VERBOSE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
                    {TRACE_IT(203);
                        Output::Print(_u("null\n"));
                    }
#endif
                }

                if (blockSucc->couldRemoveNegZeroBailoutForDef != nullptr)
                {TRACE_IT(204);
                    couldRemoveNegZeroBailoutForDef->And(blockSucc->couldRemoveNegZeroBailoutForDef);
                    if (deleteData)
                    {TRACE_IT(205);
                        JitAdelete(this->tempAlloc, blockSucc->couldRemoveNegZeroBailoutForDef);
                        blockSucc->couldRemoveNegZeroBailoutForDef = nullptr;
                    }
                }
            }

            if (blockSucc->noImplicitCallUses != nullptr)
            {TRACE_IT(206);
                noImplicitCallUses->Or(blockSucc->noImplicitCallUses);
                if (deleteData)
                {TRACE_IT(207);
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallUses);
                    blockSucc->noImplicitCallUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallNoMissingValuesUses != nullptr)
            {TRACE_IT(208);
                noImplicitCallNoMissingValuesUses->Or(blockSucc->noImplicitCallNoMissingValuesUses);
                if (deleteData)
                {TRACE_IT(209);
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallNoMissingValuesUses);
                    blockSucc->noImplicitCallNoMissingValuesUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallNativeArrayUses != nullptr)
            {TRACE_IT(210);
                noImplicitCallNativeArrayUses->Or(blockSucc->noImplicitCallNativeArrayUses);
                if (deleteData)
                {TRACE_IT(211);
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallNativeArrayUses);
                    blockSucc->noImplicitCallNativeArrayUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallJsArrayHeadSegmentSymUses != nullptr)
            {TRACE_IT(212);
                noImplicitCallJsArrayHeadSegmentSymUses->Or(blockSucc->noImplicitCallJsArrayHeadSegmentSymUses);
                if (deleteData)
                {TRACE_IT(213);
                    JitAdelete(this->tempAlloc, blockSucc->noImplicitCallJsArrayHeadSegmentSymUses);
                    blockSucc->noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
                }
            }
            if (blockSucc->noImplicitCallArrayLengthSymUses != nullptr)
            {TRACE_IT(214);
                noImplicitCallArrayLengthSymUses->Or(blockSucc->noImplicitCallArrayLengthSymUses);
                if (deleteData)
                {TRACE_IT(215);
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
        {TRACE_IT(216);
            Output::Print(_u("ObjTypeSpecStore: func %s, block %d: "),
                          this->func->GetDebugNumberSet(debugStringBuffer),
                          block->GetBlockNum());
            if (stackSymToFinalType)
            {TRACE_IT(217);
                stackSymToFinalType->Dump();
            }
            else
            {TRACE_IT(218);
                Output::Print(_u("null\n"));
            }
        }

        if (PHASE_TRACE(Js::TraceObjTypeSpecTypeGuardsPhase, this->func))
        {TRACE_IT(219);
            Output::Print(_u("ObjTypeSpec: func %s, block %d, guarded properties:\n"),
                this->func->GetDebugNumberSet(debugStringBuffer), block->GetBlockNum());
            if (stackSymToGuardedProperties)
            {TRACE_IT(220);
                stackSymToGuardedProperties->Dump();
                Output::Print(_u("\n"));
            }
            else
            {TRACE_IT(221);
                Output::Print(_u("null\n"));
            }
        }

        if (PHASE_TRACE(Js::TraceObjTypeSpecWriteGuardsPhase, this->func))
        {TRACE_IT(222);
            Output::Print(_u("ObjTypeSpec: func %s, block %d, write guards: "),
                this->func->GetDebugNumberSet(debugStringBuffer), block->GetBlockNum());
            if (stackSymToWriteGuardsMap)
            {TRACE_IT(223);
                Output::Print(_u("\n"));
                stackSymToWriteGuardsMap->Dump();
                Output::Print(_u("\n"));
            }
            else
            {TRACE_IT(224);
                Output::Print(_u("null\n"));
            }
        }
#endif
    }

#if DBG
    if (tempObjectVerifyTracker)
    {
        FOREACH_DEAD_SUCCESSOR_BLOCK(deadBlockSucc, block)
        {TRACE_IT(225);
            Assert(deadBlockSucc->tempObjectVerifyTracker || deadBlockSucc->isLoopHeader);
            if (deadBlockSucc->tempObjectVerifyTracker != nullptr)
            {TRACE_IT(226);
                Assert((deadBlockSucc->loop != nullptr) == deadBlockSucc->tempObjectVerifyTracker->HasTempTransferDependencies());
                // Dead block don't effect non temp use,  we only need to carry the removed use bit vector forward
                // and put all the upward exposed use to the set that we might found out to be mark temp
                // after globopt
                tempObjectVerifyTracker->MergeDeadData(deadBlockSucc);
            }

            if (!byteCodeUpwardExposedUsed)
            {TRACE_IT(227);
                if (!deadBlockSucc->isLoopHeader && deadBlockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
                {TRACE_IT(228);
                    Assert(deadBlockSucc->GetDataUseCount() != 0);
                    if (deadBlockSucc->DecrementDataUseCount() == 0)
                    {TRACE_IT(229);
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
        {TRACE_IT(230);
            Assert(deadBlockSucc->byteCodeUpwardExposedUsed || deadBlockSucc->isLoopHeader);
            if (deadBlockSucc->byteCodeUpwardExposedUsed)
            {TRACE_IT(231);
                byteCodeUpwardExposedUsed->Or(deadBlockSucc->byteCodeUpwardExposedUsed);
                if (this->tag == Js::DeadStorePhase)
                {TRACE_IT(232);
#if DBG
                    for (uint i = 0; i < byteCodeLocalsCount; i++)
                    {TRACE_IT(233);
                        if (byteCodeRestoreSyms[i] == nullptr)
                        {TRACE_IT(234);
                            byteCodeRestoreSyms[i] = deadBlockSucc->byteCodeRestoreSyms[i];
                        }
                        else
                        {TRACE_IT(235);
                            Assert(deadBlockSucc->byteCodeRestoreSyms[i] == nullptr
                                || byteCodeRestoreSyms[i] == deadBlockSucc->byteCodeRestoreSyms[i]);
                        }
                    }
#endif
                }
            }

            if (!deadBlockSucc->isLoopHeader && deadBlockSucc->backwardPassCurrentLoop == this->currentPrePassLoop)
            {TRACE_IT(236);
                Assert(deadBlockSucc->GetDataUseCount() != 0);
                if (deadBlockSucc->DecrementDataUseCount() == 0)
                {TRACE_IT(237);
                    this->DeleteBlockData(deadBlockSucc);
                }
            }
        }
        NEXT_DEAD_SUCCESSOR_BLOCK;
    }

    if (block->isLoopHeader)
    {TRACE_IT(238);
        this->DeleteBlockData(block);
    }
    else
    {TRACE_IT(239);
        if(block->GetDataUseCount() == 0)
        {TRACE_IT(240);
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
        {TRACE_IT(241);
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
        {TRACE_IT(242);
            block->SetDataUseCount(block->GetPredList()->Count() + block->GetDeadPredList()->Count());
        }
        else
        {TRACE_IT(243);
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
{TRACE_IT(244);
    BVSparse<JitArenaAllocator> *guardedPropertyOps1 = bucket1.GetGuardedPropertyOps();
    BVSparse<JitArenaAllocator> *guardedPropertyOps2 = bucket2.GetGuardedPropertyOps();
    Assert(guardedPropertyOps1 || guardedPropertyOps2);

    BVSparse<JitArenaAllocator> *mergedPropertyOps;
    if (guardedPropertyOps1)
    {TRACE_IT(245);
        mergedPropertyOps = guardedPropertyOps1->CopyNew();
        if (guardedPropertyOps2)
        {TRACE_IT(246);
            mergedPropertyOps->Or(guardedPropertyOps2);
        }
    }
    else
    {TRACE_IT(247);
        mergedPropertyOps = guardedPropertyOps2->CopyNew();
    }

    ObjTypeGuardBucket bucket;
    bucket.SetGuardedPropertyOps(mergedPropertyOps);
    JITTypeHolder monoGuardType = bucket1.GetMonoGuardType();
    if (monoGuardType != nullptr)
    {TRACE_IT(248);
        Assert(!bucket2.NeedsMonoCheck() || monoGuardType == bucket2.GetMonoGuardType());
    }
    else
    {TRACE_IT(249);
        monoGuardType = bucket2.GetMonoGuardType();
    }
    bucket.SetMonoGuardType(monoGuardType);

    return bucket;
}

ObjWriteGuardBucket
BackwardPass::MergeWriteGuards(ObjWriteGuardBucket bucket1, ObjWriteGuardBucket bucket2)
{TRACE_IT(250);
    BVSparse<JitArenaAllocator> *writeGuards1 = bucket1.GetWriteGuards();
    BVSparse<JitArenaAllocator> *writeGuards2 = bucket2.GetWriteGuards();
    Assert(writeGuards1 || writeGuards2);

    BVSparse<JitArenaAllocator> *mergedWriteGuards;
    if (writeGuards1)
    {TRACE_IT(251);
        mergedWriteGuards = writeGuards1->CopyNew();
        if (writeGuards2)
        {TRACE_IT(252);
            mergedWriteGuards->Or(writeGuards2);
        }
    }
    else
    {TRACE_IT(253);
        mergedWriteGuards = writeGuards2->CopyNew();
    }

    ObjWriteGuardBucket bucket;
    bucket.SetWriteGuards(mergedWriteGuards);
    return bucket;
}

void
BackwardPass::DeleteBlockData(BasicBlock * block)
{TRACE_IT(254);
    if (block->slotDeadStoreCandidates != nullptr)
    {TRACE_IT(255);
        JitAdelete(this->tempAlloc, block->slotDeadStoreCandidates);
        block->slotDeadStoreCandidates = nullptr;
    }
    if (block->tempNumberTracker != nullptr)
    {TRACE_IT(256);
        JitAdelete(this->tempAlloc, block->tempNumberTracker);
        block->tempNumberTracker = nullptr;
    }
    if (block->tempObjectTracker != nullptr)
    {TRACE_IT(257);
        JitAdelete(this->tempAlloc, block->tempObjectTracker);
        block->tempObjectTracker = nullptr;
    }
#if DBG
    if (block->tempObjectVerifyTracker != nullptr)
    {TRACE_IT(258);
        JitAdelete(this->tempAlloc, block->tempObjectVerifyTracker);
        block->tempObjectVerifyTracker = nullptr;
    }
#endif
    if (block->stackSymToFinalType != nullptr)
    {TRACE_IT(259);
        block->stackSymToFinalType->Delete();
        block->stackSymToFinalType = nullptr;
    }
    if (block->stackSymToGuardedProperties != nullptr)
    {TRACE_IT(260);
        block->stackSymToGuardedProperties->Delete();
        block->stackSymToGuardedProperties = nullptr;
    }
    if (block->stackSymToWriteGuardsMap != nullptr)
    {TRACE_IT(261);
        block->stackSymToWriteGuardsMap->Delete();
        block->stackSymToWriteGuardsMap = nullptr;
    }
    if (block->cloneStrCandidates != nullptr)
    {TRACE_IT(262);
        Assert(this->tag == Js::BackwardPhase);
        JitAdelete(this->globOpt->alloc, block->cloneStrCandidates);
        block->cloneStrCandidates = nullptr;
    }
    if (block->noImplicitCallUses != nullptr)
    {TRACE_IT(263);
        JitAdelete(this->tempAlloc, block->noImplicitCallUses);
        block->noImplicitCallUses = nullptr;
    }
    if (block->noImplicitCallNoMissingValuesUses != nullptr)
    {TRACE_IT(264);
        JitAdelete(this->tempAlloc, block->noImplicitCallNoMissingValuesUses);
        block->noImplicitCallNoMissingValuesUses = nullptr;
    }
    if (block->noImplicitCallNativeArrayUses != nullptr)
    {TRACE_IT(265);
        JitAdelete(this->tempAlloc, block->noImplicitCallNativeArrayUses);
        block->noImplicitCallNativeArrayUses = nullptr;
    }
    if (block->noImplicitCallJsArrayHeadSegmentSymUses != nullptr)
    {TRACE_IT(266);
        JitAdelete(this->tempAlloc, block->noImplicitCallJsArrayHeadSegmentSymUses);
        block->noImplicitCallJsArrayHeadSegmentSymUses = nullptr;
    }
    if (block->noImplicitCallArrayLengthSymUses != nullptr)
    {TRACE_IT(267);
        JitAdelete(this->tempAlloc, block->noImplicitCallArrayLengthSymUses);
        block->noImplicitCallArrayLengthSymUses = nullptr;
    }
    if (block->upwardExposedUses != nullptr)
    {TRACE_IT(268);
        JitArenaAllocator *upwardExposedArena = (this->tag == Js::BackwardPhase) ? this->globOpt->alloc : this->tempAlloc;
        JitAdelete(upwardExposedArena, block->upwardExposedUses);
        block->upwardExposedUses = nullptr;
    }
    if (block->upwardExposedFields != nullptr)
    {TRACE_IT(269);
        JitArenaAllocator *upwardExposedArena = (this->tag == Js::BackwardPhase) ? this->globOpt->alloc : this->tempAlloc;
        JitAdelete(upwardExposedArena, block->upwardExposedFields);
        block->upwardExposedFields = nullptr;
    }
    if (block->typesNeedingKnownObjectLayout != nullptr)
    {TRACE_IT(270);
        JitAdelete(this->tempAlloc, block->typesNeedingKnownObjectLayout);
        block->typesNeedingKnownObjectLayout = nullptr;
    }
    if (block->fieldHoistCandidates != nullptr)
    {TRACE_IT(271);
        JitAdelete(this->tempAlloc, block->fieldHoistCandidates);
        block->fieldHoistCandidates = nullptr;
    }
    if (block->byteCodeUpwardExposedUsed != nullptr)
    {TRACE_IT(272);
        JitAdelete(this->tempAlloc, block->byteCodeUpwardExposedUsed);
        block->byteCodeUpwardExposedUsed = nullptr;
#if DBG
        JitAdeleteArray(this->tempAlloc, func->GetJITFunctionBody()->GetLocalsCount(), block->byteCodeRestoreSyms);
        block->byteCodeRestoreSyms = nullptr;
#endif
    }
    if (block->couldRemoveNegZeroBailoutForDef != nullptr)
    {TRACE_IT(273);
        JitAdelete(this->tempAlloc, block->couldRemoveNegZeroBailoutForDef);
        block->couldRemoveNegZeroBailoutForDef = nullptr;
    }
}

void
BackwardPass::ProcessLoopCollectionPass(BasicBlock *const lastBlock)
{TRACE_IT(274);
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
        {TRACE_IT(275);
            Output::Print(_u("******* COLLECTION PASS 1 START: Loop %u ********\n"), collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif

        FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, nullptr)
        {TRACE_IT(276);
            ProcessBlock(block);

            if(block->isLoopHeader)
            {TRACE_IT(277);
                if(block->loop == collectionPassLoop)
                {TRACE_IT(278);
                    break;
                }

                // Keep track of the first inner loop's header for the second pass, which need only walk up to that block
                firstInnerLoopHeader = block;
            }
        } NEXT_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE;

#if DBG_DUMP
        if(IsTraceEnabled())
        {TRACE_IT(279);
            Output::Print(_u("******** COLLECTION PASS 1 END: Loop %u *********\n"), collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif
    }

    // Second pass, only needs to run if there are any inner loops, to propagate collected information into those loops
    if(firstInnerLoopHeader)
    {TRACE_IT(280);
#if DBG_DUMP
        if(IsTraceEnabled())
        {TRACE_IT(281);
            Output::Print(_u("******* COLLECTION PASS 2 START: Loop %u ********\n"), collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif

        FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, firstInnerLoopHeader)
        {TRACE_IT(282);
            Loop *const loop = block->loop;
            if(loop && loop != collectionPassLoop && !loop->hasDeadStoreCollectionPass)
            {TRACE_IT(283);
                // About to make a recursive call, so when jitting in the foreground, probe the stack
                if(!func->IsBackgroundJIT())
                {TRACE_IT(284);
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
        {TRACE_IT(285);
            Output::Print(_u("******** COLLECTION PASS 2 END: Loop %u *********\n"), collectionPassLoop->GetLoopTopInstr()->m_id);
        }
#endif
    }

    currentPrePassLoop = previousPrepassLoop;
}

void
BackwardPass::ProcessLoop(BasicBlock * lastBlock)
{TRACE_IT(286);
#if DBG_DUMP
    if (this->IsTraceEnabled())
    {TRACE_IT(287);
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
    {TRACE_IT(288);
        if (this->globOpt->DoFieldOpts(loop) || this->globOpt->DoFieldRefOpts(loop))
        {TRACE_IT(289);
            // Get the live-out set at the loop bottom.
            // This may not be the only loop exit, but all loop exits either leave the function or pass through here.
            // In the forward pass, we'll use this set to trim the live fields on exit from the loop
            // in order to limit the number of bailout points following the loop.
            BVSparse<JitArenaAllocator> *bv = JitAnew(this->func->m_fg->alloc, BVSparse<JitArenaAllocator>, this->func->m_fg->alloc);
            FOREACH_SUCCESSOR_BLOCK(blockSucc, lastBlock)
            {TRACE_IT(290);
                if (blockSucc->loop != loop)
                {TRACE_IT(291);
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
    {TRACE_IT(292);
        Assert(!IsCollectionPass());
        Assert(!IsPrePass());
        isCollectionPass = true;
        ProcessLoopCollectionPass(lastBlock);
        isCollectionPass = false;
    }

    Assert(!this->IsPrePass());
    this->currentPrePassLoop = loop;

    FOREACH_BLOCK_BACKWARD_IN_RANGE_DEAD_OR_ALIVE(block, lastBlock, nullptr)
    {TRACE_IT(293);
        this->ProcessBlock(block);

        if (block->isLoopHeader && block->loop == lastBlock->loop)
        {TRACE_IT(294);
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
    {TRACE_IT(295);
        Output::Print(_u("******** PREPASS END *********\n"));
    }
#endif
}

void
BackwardPass::OptBlock(BasicBlock * block)
{TRACE_IT(296);
    this->func->ThrowIfScriptClosed();

    if (block->loop && !block->loop->hasDeadStorePrepass)
    {TRACE_IT(297);
        ProcessLoop(block);
    }

    this->ProcessBlock(block);

    if(DoTrackNegativeZero())
    {TRACE_IT(298);
        negativeZeroDoesNotMatterBySymId->ClearAll();
    }
    if (DoTrackBitOpsOrNumber())
    {TRACE_IT(299);
        symUsedOnlyForBitOpsBySymId->ClearAll();
        symUsedOnlyForNumberBySymId->ClearAll();
    }

    if(DoTrackIntOverflow())
    {TRACE_IT(300);
        intOverflowDoesNotMatterBySymId->ClearAll();
        if(DoTrackCompoundedIntOverflow())
        {TRACE_IT(301);
            intOverflowDoesNotMatterInRangeBySymId->ClearAll();
        }
    }
}

void
BackwardPass::ProcessBailOutArgObj(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed)
{TRACE_IT(302);
    Assert(this->tag != Js::BackwardPhase);

    if (this->globOpt->TrackArgumentsObject() && bailOutInfo->capturedValues.argObjSyms)
    {
        FOREACH_BITSET_IN_SPARSEBV(symId, bailOutInfo->capturedValues.argObjSyms)
        {TRACE_IT(303);
            if (byteCodeUpwardExposedUsed->TestAndClear(symId))
            {TRACE_IT(304);
                if (bailOutInfo->usedCapturedValues.argObjSyms == nullptr)
                {TRACE_IT(305);
                    bailOutInfo->usedCapturedValues.argObjSyms = JitAnew(this->func->m_alloc,
                        BVSparse<JitArenaAllocator>, this->func->m_alloc);
                }
                bailOutInfo->usedCapturedValues.argObjSyms->Set(symId);
            }
        }
        NEXT_BITSET_IN_SPARSEBV;
    }
    if (bailOutInfo->usedCapturedValues.argObjSyms)
    {TRACE_IT(306);
        byteCodeUpwardExposedUsed->Minus(bailOutInfo->usedCapturedValues.argObjSyms);
    }
}

void
BackwardPass::ProcessBailOutConstants(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed, BVSparse<JitArenaAllocator>* bailoutReferencedArgSymsBv)
{TRACE_IT(307);
    Assert(this->tag != Js::BackwardPhase);

    // Remove constants that we are already going to restore
    SListBase<ConstantStackSymValue> * usedConstantValues = &bailOutInfo->usedCapturedValues.constantValues;
    FOREACH_SLISTBASE_ENTRY(ConstantStackSymValue, value, usedConstantValues)
    {TRACE_IT(308);
        byteCodeUpwardExposedUsed->Clear(value.Key()->m_id);
        bailoutReferencedArgSymsBv->Clear(value.Key()->m_id);
    }
    NEXT_SLISTBASE_ENTRY;

    // Find other constants that we need to restore
    FOREACH_SLISTBASE_ENTRY_EDITING(ConstantStackSymValue, value, &bailOutInfo->capturedValues.constantValues, iter)
    {TRACE_IT(309);
        if (byteCodeUpwardExposedUsed->TestAndClear(value.Key()->m_id) || bailoutReferencedArgSymsBv->TestAndClear(value.Key()->m_id))
        {TRACE_IT(310);
            // Constant need to be restore, move it to the restore list
            iter.MoveCurrentTo(usedConstantValues);
        }
        else if (!this->IsPrePass())
        {TRACE_IT(311);
            // Constants don't need to be restored, delete
            iter.RemoveCurrent(this->func->m_alloc);
        }
    }
    NEXT_SLISTBASE_ENTRY_EDITING;
}

void
BackwardPass::ProcessBailOutCopyProps(BailOutInfo * bailOutInfo, BVSparse<JitArenaAllocator> * byteCodeUpwardExposedUsed, BVSparse<JitArenaAllocator>* bailoutReferencedArgSymsBv)
{TRACE_IT(312);
    Assert(this->tag != Js::BackwardPhase);
    Assert(!this->func->GetJITFunctionBody()->IsAsmJsMode());

    // Remove copy prop that we were already going to restore
    SListBase<CopyPropSyms> * usedCopyPropSyms = &bailOutInfo->usedCapturedValues.copyPropSyms;
    FOREACH_SLISTBASE_ENTRY(CopyPropSyms, copyPropSyms, usedCopyPropSyms)
    {TRACE_IT(313);
        byteCodeUpwardExposedUsed->Clear(copyPropSyms.Key()->m_id);
        this->currentBlock->upwardExposedUses->Set(copyPropSyms.Value()->m_id);
    }
    NEXT_SLISTBASE_ENTRY;

    JitArenaAllocator * allocator = this->func->m_alloc;
    BasicBlock * block = this->currentBlock;
    BVSparse<JitArenaAllocator> * upwardExposedUses = block->upwardExposedUses;

    // Find other copy prop that we need to restore
    FOREACH_SLISTBASE_ENTRY_EDITING(CopyPropSyms, copyPropSyms, &bailOutInfo->capturedValues.copyPropSyms, iter)
    {TRACE_IT(314);
        // Copy prop syms should be vars
        Assert(!copyPropSyms.Key()->IsTypeSpec());
        Assert(!copyPropSyms.Value()->IsTypeSpec());
        if (byteCodeUpwardExposedUsed->TestAndClear(copyPropSyms.Key()->m_id) || bailoutReferencedArgSymsBv->TestAndClear(copyPropSyms.Key()->m_id))
        {TRACE_IT(315);
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
            {TRACE_IT(316);
                Assert(bailOutInfo->bailOutOffset == instr->GetByteCodeOffset());

                // Need to use the original sym to restore. The original sym is byte-code upwards-exposed, which is why it needs
                // to be restored. Because the original sym needs to be restored and the copy-prop sym is changing here, the
                // original sym must be live in some fashion at the point of this instruction, that will be verified below. The
                // original sym will also be made upwards-exposed from here, so the aforementioned transferring store of the
                // copy-prop sym to the original sym will not be a dead store.
            }
            else if (block->upwardExposedUses->Test(stackSym->m_id) && !block->upwardExposedUses->Test(copyPropSyms.Value()->m_id))
            {TRACE_IT(317);
                // Don't use the copy prop sym if it is not used and the orig sym still has uses.
                // No point in extending the lifetime of the copy prop sym unnecessarily.
            }
            else
            {TRACE_IT(318);
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
            {TRACE_IT(319);
                // Var version of the sym is not live, use the int32 version
                int32StackSym = stackSym->GetInt32EquivSym(nullptr);
                Assert(int32StackSym);
            }
            else if(bailOutInfo->liveFloat64Syms->Test(symId))
            {TRACE_IT(320);
                // Var/int32 version of the sym is not live, use the float64 version
                float64StackSym = stackSym->GetFloat64EquivSym(nullptr);
                Assert(float64StackSym);
            }
            // SIMD_JS
            else if (bailOutInfo->liveSimd128F4Syms->Test(symId))
            {TRACE_IT(321);
                simd128StackSym = stackSym->GetSimd128F4EquivSym(nullptr);
            }
            else if (bailOutInfo->liveSimd128I4Syms->Test(symId))
            {TRACE_IT(322);
                simd128StackSym = stackSym->GetSimd128I4EquivSym(nullptr);
            }
            else
            {TRACE_IT(323);
                Assert(bailOutInfo->liveVarSyms->Test(symId));
            }

            // We did not end up using the copy prop sym. Let's make sure the use of the original sym by the bailout is captured.
            if (stackSym != copyPropSyms.Value() && stackSym->HasArgSlotNum())
            {TRACE_IT(324);
                bailoutReferencedArgSymsBv->Set(stackSym->m_id);
            }

            if (int32StackSym != nullptr)
            {TRACE_IT(325);
                Assert(float64StackSym == nullptr);
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), int32StackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(int32StackSym->m_id);
            }
            else if (float64StackSym != nullptr)
            {TRACE_IT(326);
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
            {TRACE_IT(327);
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), simd128StackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(simd128StackSym->m_id);
            }
            else
            {TRACE_IT(328);
                usedCopyPropSyms->PrependNode(allocator, copyPropSyms.Key(), stackSym);
                iter.RemoveCurrent(allocator);
                upwardExposedUses->Set(symId);
            }
        }
        else if (!this->IsPrePass())
        {TRACE_IT(329);
            // Copy prop sym doesn't need to be restored, delete.
            iter.RemoveCurrent(allocator);
        }
    }
    NEXT_SLISTBASE_ENTRY_EDITING;
}

bool
BackwardPass::ProcessBailOutInfo(IR::Instr * instr)
{TRACE_IT(330);
    if (this->tag == Js::BackwardPhase)
    {TRACE_IT(331);
        // We don't need to fill in the bailout instruction in backward pass
        Assert(this->func->hasBailout || !instr->HasBailOutInfo());
        Assert(!instr->HasBailOutInfo() || instr->GetBailOutInfo()->byteCodeUpwardExposedUsed == nullptr || (this->func->HasTry() && this->func->DoOptimizeTryCatch()));

        if (instr->IsByteCodeUsesInstr())
        {TRACE_IT(332);
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
            {TRACE_IT(333);
                this->currentBlock->upwardExposedUses->Or(byteCodeUpwardExposedUsed);
            }
            return true;
        }
        return false;
    }

    if (instr->IsByteCodeUsesInstr())
    {TRACE_IT(334);
        Assert(instr->m_opcode == Js::OpCode::ByteCodeUses);
#if DBG
        if (this->DoMarkTempObjectVerify() && (this->currentBlock->isDead || !this->func->hasBailout))
        {TRACE_IT(335);
            if (IsCollectionPass())
            {TRACE_IT(336);
                if (!this->func->hasBailout)
                {TRACE_IT(337);
                    // Prevent byte code uses from being remove on collection pass for mark temp object verify
                    // if we don't have any bailout
                    return true;
                }
            }
            else
            {TRACE_IT(338);
                this->currentBlock->tempObjectVerifyTracker->NotifyDeadByteCodeUses(instr);
            }
        }
#endif

        if (this->func->hasBailout)
        {TRACE_IT(339);
            Assert(this->DoByteCodeUpwardExposedUsed());

            // Just collect the byte code uses, and remove the instruction
            // We are going backward, process the dst first and then the src
            IR::Opnd * dst = instr->GetDst();
            if (dst)
            {TRACE_IT(340);
                IR::RegOpnd * dstRegOpnd = dst->AsRegOpnd();
                StackSym * dstStackSym = dstRegOpnd->m_sym->AsStackSym();
                Assert(!dstRegOpnd->GetIsJITOptimizedReg());
                Assert(dstStackSym->GetByteCodeRegSlot() != Js::Constants::NoRegister);
                if (dstStackSym->GetType() != TyVar)
                {TRACE_IT(341);
                    dstStackSym = dstStackSym->GetVarEquivSym(nullptr);
                }

                // If the current region is a Try, symbols in its write-through set shouldn't be cleared.
                // Otherwise, symbols in the write-through set of the first try ancestor shouldn't be cleared.
                if (!this->currentRegion ||
                    !this->CheckWriteThroughSymInRegion(this->currentRegion, dstStackSym))
                {TRACE_IT(342);
                    this->currentBlock->byteCodeUpwardExposedUsed->Clear(dstStackSym->m_id);
#if DBG
                    // We can only track first level function stack syms right now
                    if (dstStackSym->GetByteCodeFunc() == this->func)
                    {TRACE_IT(343);
                        this->currentBlock->byteCodeRestoreSyms[dstStackSym->GetByteCodeRegSlot()] = nullptr;
                    }
#endif
                }
            }

            IR::ByteCodeUsesInstr *byteCodeUsesInstr = instr->AsByteCodeUsesInstr();
            if (byteCodeUsesInstr->GetByteCodeUpwardExposedUsed() != nullptr)
            {TRACE_IT(344);
                this->currentBlock->byteCodeUpwardExposedUsed->Or(byteCodeUsesInstr->GetByteCodeUpwardExposedUsed());
#if DBG
                FOREACH_BITSET_IN_SPARSEBV(symId, byteCodeUsesInstr->GetByteCodeUpwardExposedUsed())
                {TRACE_IT(345);
                    StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                    Assert(!stackSym->IsTypeSpec());
                    // We can only track first level function stack syms right now
                    if (stackSym->GetByteCodeFunc() == this->func)
                    {TRACE_IT(346);
                        Js::RegSlot byteCodeRegSlot = stackSym->GetByteCodeRegSlot();
                        Assert(byteCodeRegSlot != Js::Constants::NoRegister);
                        if (this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] != stackSym)
                        {TRACE_IT(347);
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
            {TRACE_IT(348);
                return true;
            }

            ProcessPendingPreOpBailOutInfo(instr);

            PropertySym *propertySymUse = byteCodeUsesInstr->propertySymUse;
            if (propertySymUse && !this->currentBlock->isDead)
            {TRACE_IT(349);
                this->currentBlock->upwardExposedFields->Set(propertySymUse->m_id);
            }

            if (this->IsPrePass())
            {TRACE_IT(350);
                // Don't remove the instruction yet if we are in the prepass
                // But tell the caller we don't need to process the instruction any more
                return true;
            }
        }

        this->currentBlock->RemoveInstr(instr);
        return true;
    }

    if(IsCollectionPass())
    {TRACE_IT(351);
        return false;
    }

    if (instr->HasBailOutInfo())
    {TRACE_IT(352);
        Assert(this->func->hasBailout);
        Assert(this->DoByteCodeUpwardExposedUsed());

        BailOutInfo * bailOutInfo = instr->GetBailOutInfo();

        // Only process the bailout info if this is the main bailout point (instead of shared)
        if (bailOutInfo->bailOutInstr == instr)
        {TRACE_IT(353);
            if(instr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset ||
                bailOutInfo->bailOutOffset > instr->GetByteCodeOffset())
            {TRACE_IT(354);
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
            {TRACE_IT(355);
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
{TRACE_IT(356);
    return this->globOpt->IsImplicitCallBailOutCurrentlyNeeded(
        instr, nullptr, nullptr, this->currentBlock, hasLiveFields, mayNeedImplicitCallBailOut, false);
}

void
BackwardPass::DeadStoreTypeCheckBailOut(IR::Instr * instr)
{TRACE_IT(357);
    // Good news: There are cases where the forward pass installs BailOutFailedTypeCheck, but the dead store pass
    // discovers that the checked type is dead.
    // Bad news: We may still need implicit call bailout, and it's up to the dead store pass to figure this out.
    // Worse news: BailOutFailedTypeCheck is pre-op, and BailOutOnImplicitCall is post-op. We'll use a special
    // bailout kind to indicate implicit call bailout that targets its own instruction. The lowerer will emit
    // code to disable/re-enable implicit calls around the operation.

    Assert(this->tag == Js::DeadStorePhase);

    if (this->IsPrePass() || !instr->HasBailOutInfo())
    {TRACE_IT(358);
        return;
    }

    IR::BailOutKind oldBailOutKind = instr->GetBailOutKind();
    if (!IR::IsTypeCheckBailOutKind(oldBailOutKind))
    {TRACE_IT(359);
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
    {TRACE_IT(360);
        // If we installed a failed type check bailout in the forward pass, but we are now discovering that the checked
        // type is dead, we may still need a bailout on failed fixed field type check. These type checks are required
        // regardless of whether the checked type is dead.  Hence, the bailout kind may change here.
        Assert((oldBailOutKind & ~IR::BailOutKindBits) == bailOutKind ||
            bailOutKind == IR::BailOutFailedFixedFieldTypeCheck || bailOutKind == IR::BailOutFailedEquivalentFixedFieldTypeCheck);
        instr->SetBailOutKind(bailOutKind);
        return;
    }
    else if (isTypeCheckProtected)
    {TRACE_IT(361);
        instr->ClearBailOutInfo();
        if (preOpBailOutInstrToProcess == instr)
        {TRACE_IT(362);
            preOpBailOutInstrToProcess = nullptr;
        }
        return;
    }

    Assert(!propertySymOpnd->IsTypeCheckProtected());

    // If all we're doing here is checking the type (e.g. because we've hoisted a field load or store out of the loop, but needed
    // the type check to remain in the loop), and now it turns out we don't need the type checked, we can simply turn this into
    // a NOP and remove the bailout.
    if (instr->m_opcode == Js::OpCode::CheckObjType)
    {TRACE_IT(363);
        Assert(instr->GetDst() == nullptr && instr->GetSrc1() != nullptr && instr->GetSrc2() == nullptr);
        instr->m_opcode = Js::OpCode::Nop;
        instr->FreeSrc1();
        instr->ClearBailOutInfo();
        if (this->preOpBailOutInstrToProcess == instr)
        {TRACE_IT(364);
            this->preOpBailOutInstrToProcess = nullptr;
        }
        return;
    }

    // We don't need BailOutFailedTypeCheck but may need BailOutOnImplicitCall.
    // Consider: are we in the loop landing pad? If so, no bailout, since implicit calls will be checked at
    // the end of the block.
    if (this->currentBlock->IsLandingPad())
    {TRACE_IT(365);
        // We're in the landing pad.
        if (preOpBailOutInstrToProcess == instr)
        {TRACE_IT(366);
            preOpBailOutInstrToProcess = nullptr;
        }
        instr->UnlinkBailOutInfo();
        return;
    }

    // If bailOutKind is equivTypeCheck then leave alone the bailout
    if (bailOutKind == IR::BailOutFailedEquivalentTypeCheck ||
        bailOutKind == IR::BailOutFailedEquivalentFixedFieldTypeCheck)
    {TRACE_IT(367);
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
{TRACE_IT(368);
    Assert(this->tag == Js::DeadStorePhase);

    if (this->IsPrePass() || !instr->HasBailOutInfo())
    {TRACE_IT(369);
        // Don't do this in the pre-pass, because, for instance, we don't have live-on-back-edge fields yet.
        return;
    }

    if (OpCodeAttr::BailOutRec(instr->m_opcode))
    {TRACE_IT(370);
        // This is something like OpCode::BailOutOnNotEqual. Assume it needs what it's got.
        return;
    }

    UpdateArrayBailOutKind(instr);

    // Install the implicit call PreOp for mark temp object if we need one.
    IR::BailOutKind kind = instr->GetBailOutKind();
    IR::BailOutKind kindNoBits = kind & ~IR::BailOutKindBits;
    if ((kind & IR::BailOutMarkTempObject) != 0 && kindNoBits != IR::BailOutOnImplicitCallsPreOp)
    {TRACE_IT(371);
        Assert(kindNoBits != IR::BailOutOnImplicitCalls);
        if (kindNoBits == IR::BailOutInvalid)
        {TRACE_IT(372);
            // We should only have combined with array bits
            Assert((kind & ~IR::BailOutForArrayBits) == IR::BailOutMarkTempObject);
            // Don't need to install if we are not going to do helper calls,
            // or we are in the landingPad since implicit calls are already turned off.
            if ((kind & IR::BailOutOnArrayAccessHelperCall) == 0 && !this->currentBlock->IsLandingPad())
            {TRACE_IT(373);
                kind += IR::BailOutOnImplicitCallsPreOp;
                instr->SetBailOutKind(kind);
            }
        }
    }

    // Currently only try to eliminate these bailout kinds. The others are required in cases
    // where we don't necessarily have live/hoisted fields.
    const bool mayNeedBailOnImplicitCall = BailOutInfo::IsBailOutOnImplicitCalls(kind);
    if (!mayNeedBailOnImplicitCall)
    {TRACE_IT(374);
        if (kind & IR::BailOutMarkTempObject)
        {TRACE_IT(375);
            if (kind == IR::BailOutMarkTempObject)
            {TRACE_IT(376);
                // Landing pad does not need per-instr implicit call bailouts.
                Assert(this->currentBlock->IsLandingPad());
                instr->ClearBailOutInfo();
                if (this->preOpBailOutInstrToProcess == instr)
                {TRACE_IT(377);
                    this->preOpBailOutInstrToProcess = nullptr;
                }
            }
            else
            {TRACE_IT(378);
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
    {TRACE_IT(379);
        instr->ClearBailOutInfo();
        if (preOpBailOutInstrToProcess == instr)
        {TRACE_IT(380);
            preOpBailOutInstrToProcess = nullptr;
        }
#if DBG
        if (this->DoMarkTempObjectVerify())
        {TRACE_IT(381);
            this->currentBlock->tempObjectVerifyTracker->NotifyBailOutRemoval(instr, this);
        }
#endif
    }
}

void
BackwardPass::ProcessPendingPreOpBailOutInfo(IR::Instr *const currentInstr)
{TRACE_IT(382);
    Assert(!IsCollectionPass());

    if(!preOpBailOutInstrToProcess)
    {TRACE_IT(383);
        return;
    }

    IR::Instr *const prevInstr = currentInstr->m_prev;
    if(prevInstr &&
        prevInstr->IsByteCodeUsesInstr() &&
        prevInstr->AsByteCodeUsesInstr()->GetByteCodeOffset() == preOpBailOutInstrToProcess->GetByteCodeOffset())
    {TRACE_IT(384);
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
{TRACE_IT(385);
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
    {TRACE_IT(386);
        // Create the BV of symbols that need to be restored in the BailOutRecord
        byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed->CopyNew(this->func->m_alloc);
        bailOutInfo->byteCodeUpwardExposedUsed = byteCodeUpwardExposedUsed;
    }
    else
    {TRACE_IT(387);
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
    {TRACE_IT(388);
        bailOutInfo->IterateArgOutSyms([=](uint, uint, StackSym* sym) {
            if (!sym->IsArgSlotSym())
            {TRACE_IT(389);
                bailoutReferencedArgSymsBv->Set(sym->m_id);
            }
        });
    }

    // Process Argument object first, as they can be found on the stack and don't need to rely on copy prop
    this->ProcessBailOutArgObj(bailOutInfo, byteCodeUpwardExposedUsed);

    if (instr->m_opcode != Js::OpCode::BailOnException) // see comment at the beginning of this function
    {TRACE_IT(390);
        this->ProcessBailOutConstants(bailOutInfo, byteCodeUpwardExposedUsed, bailoutReferencedArgSymsBv);
        this->ProcessBailOutCopyProps(bailOutInfo, byteCodeUpwardExposedUsed, bailoutReferencedArgSymsBv);
    }

    BVSparse<JitArenaAllocator> * tempBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);

    if (bailOutInfo->liveVarSyms)
    {TRACE_IT(391);
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
        {TRACE_IT(392);
            // Add to byteCodeUpwardExposedUsed the non-temp local vars used so far to restore during bail out.
            // The ones that are not used so far will get their values from bytecode when we continue after bail out in interpreter.
            Assert(this->func->m_nonTempLocalVars);
            tempBv->And(this->func->m_nonTempLocalVars, bailOutInfo->liveVarSyms);

            // Remove syms that are restored in other ways than byteCodeUpwardExposedUsed.
            FOREACH_SLIST_ENTRY(ConstantStackSymValue, value, &bailOutInfo->usedCapturedValues.constantValues)
            {TRACE_IT(393);
                Assert(value.Key()->HasByteCodeRegSlot() || value.Key()->GetInstrDef()->m_opcode == Js::OpCode::BytecodeArgOutCapture);
                if (value.Key()->HasByteCodeRegSlot())
                {TRACE_IT(394);
                    tempBv->Clear(value.Key()->GetByteCodeRegSlot());
                }
            }
            NEXT_SLIST_ENTRY;
            FOREACH_SLIST_ENTRY(CopyPropSyms, value, &bailOutInfo->usedCapturedValues.copyPropSyms)
            {TRACE_IT(395);
                Assert(value.Key()->HasByteCodeRegSlot() || value.Key()->GetInstrDef()->m_opcode == Js::OpCode::BytecodeArgOutCapture);
                if (value.Key()->HasByteCodeRegSlot())
                {TRACE_IT(396);
                    tempBv->Clear(value.Key()->GetByteCodeRegSlot());
                }
            }
            NEXT_SLIST_ENTRY;
            if (bailOutInfo->usedCapturedValues.argObjSyms)
            {TRACE_IT(397);
                tempBv->Minus(bailOutInfo->usedCapturedValues.argObjSyms);
            }

            byteCodeUpwardExposedUsed->Or(tempBv);
        }

        if (instr->m_opcode != Js::OpCode::BailOnException) // see comment at the beginning of this function
        {TRACE_IT(398);
            // Int32
            tempBv->And(byteCodeUpwardExposedUsed, bailOutInfo->liveLosslessInt32Syms);
            byteCodeUpwardExposedUsed->Minus(tempBv);
            FOREACH_BITSET_IN_SPARSEBV(symId, tempBv)
            {TRACE_IT(399);
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
            {TRACE_IT(400);
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
            {TRACE_IT(401);
                StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
                Assert(stackSym->GetType() == TyVar);
                StackSym * simd128Sym = nullptr;
                if (bailOutInfo->liveSimd128F4Syms->Test(symId))
                {TRACE_IT(402);
                    simd128Sym = stackSym->GetSimd128F4EquivSym(nullptr);
                }
                else
                {TRACE_IT(403);
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
    {TRACE_IT(404);
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
    {TRACE_IT(405);
        instr->m_func->frameInfo->IterateSyms([=](StackSym* argSym)
        {
            this->currentBlock->upwardExposedUses->Set(argSym->m_id);
        });
    }

    // Mark all the register that we need to restore as used (excluding constants)
    block->upwardExposedUses->Or(byteCodeUpwardExposedUsed);
    block->upwardExposedUses->Or(bailoutReferencedArgSymsBv);

    if (!this->IsPrePass())
    {TRACE_IT(406);
        bailOutInfo->IterateArgOutSyms([=](uint index, uint, StackSym* sym) {
            if (sym->IsArgSlotSym() || bailoutReferencedArgSymsBv->Test(sym->m_id))
            {TRACE_IT(407);
                bailOutInfo->argOutSyms[index]->m_isBailOutReferenced = true;
            }
        });
    }
    JitAdelete(this->tempAlloc, bailoutReferencedArgSymsBv);

    if (this->IsPrePass())
    {TRACE_IT(408);
        JitAdelete(this->tempAlloc, byteCodeUpwardExposedUsed);
    }
}

void
BackwardPass::ProcessBlock(BasicBlock * block)
{TRACE_IT(409);
    this->currentBlock = block;
    this->MergeSuccBlocksInfo(block);
#if DBG_DUMP
    if (this->IsTraceEnabled())
    {TRACE_IT(410);
        Output::Print(_u("******************************* Before Process Block *******************************n"));
        DumpBlockData(block);
    }
#endif
    FOREACH_INSTR_BACKWARD_IN_BLOCK_EDITING(instr, instrPrev, block)
    {TRACE_IT(411);
#if DBG_DUMP
        if (!IsCollectionPass() && IsTraceEnabled() && Js::Configuration::Global.flags.Verbose)
        {TRACE_IT(412);
            Output::Print(_u(">>>>>>>>>>>>>>>>>>>>>> %s: Instr Start\n"), tag == Js::BackwardPhase? _u("BACKWARD") : _u("DEADSTORE"));
            instr->Dump();
            if (block->upwardExposedUses)
            {TRACE_IT(413);
                Output::SkipToColumn(10);
                Output::Print(_u("   Exposed Use: "));
                block->upwardExposedUses->Dump();
            }
            if (block->upwardExposedFields)
            {TRACE_IT(414);
                Output::SkipToColumn(10);
                Output::Print(_u("Exposed Fields: "));
                block->upwardExposedFields->Dump();
            }
            if (block->byteCodeUpwardExposedUsed)
            {TRACE_IT(415);
                Output::SkipToColumn(10);
                Output::Print(_u(" Byte Code Use: "));
                block->byteCodeUpwardExposedUsed->Dump();
            }
            Output::Print(_u("--------------------\n"));
        }
#endif

        AssertOrFailFastMsg(!instr->IsLowered(), "Lowered instruction detected in pre-lower context!");

        this->currentInstr = instr;
        this->currentRegion = this->currentBlock->GetFirstInstr()->AsLabelInstr()->GetRegion();
        
        IR::Instr * insertedInstr = TryChangeInstrForStackArgOpt();
        if (insertedInstr != nullptr)
        {TRACE_IT(416);
            instrPrev = insertedInstr;
            continue;
        }

        MarkScopeObjSymUseForStackArgOpt();
        ProcessBailOnStackArgsOutOfActualsRange();
        
        if (ProcessNoImplicitCallUses(instr) || this->ProcessBailOutInfo(instr))
        {TRACE_IT(417);
            continue;
        }

        IR::Instr *instrNext = instr->m_next;
        if (this->TrackNoImplicitCallInlinees(instr))
        {TRACE_IT(418);
            instrPrev = instrNext->m_prev;
            continue;
        }

        if (CanDeadStoreInstrForScopeObjRemoval() && DeadStoreOrChangeInstrForScopeObjRemoval(&instrPrev))
        {TRACE_IT(419);
            continue;
        }

        bool hasLiveFields = (block->upwardExposedFields && !block->upwardExposedFields->IsEmpty());

        IR::Opnd * opnd = instr->GetDst();
        if (opnd != nullptr)
        {TRACE_IT(420);
            bool isRemoved = ReverseCopyProp(instr);
            if (isRemoved)
            {TRACE_IT(421);
                instrPrev = instrNext->m_prev;
                continue;
            }
            if (instr->m_opcode == Js::OpCode::Conv_Bool)
            {TRACE_IT(422);
                isRemoved = this->FoldCmBool(instr);
                if (isRemoved)
                {TRACE_IT(423);
                    continue;
                }
            }

            ProcessNewScObject(instr);

            this->ProcessTransfers(instr);

            isRemoved = this->ProcessDef(opnd);
            if (isRemoved)
            {TRACE_IT(424);
                continue;
            }
        }

        if(!IsCollectionPass())
        {TRACE_IT(425);
            this->MarkTempProcessInstr(instr);
            this->ProcessFieldKills(instr);

            if (this->DoDeadStoreSlots()
                && (instr->HasAnyImplicitCalls() || instr->HasBailOutInfo() || instr->UsesAllFields()))
            {TRACE_IT(426);
                // Can't dead-store slots if there can be an implicit-call, an exception, or a bailout
                block->slotDeadStoreCandidates->ClearAll();
            }

            if (this->DoFieldHoistCandidates())
            {TRACE_IT(427);
                this->ProcessFieldHoistKills(instr);
            }

            TrackIntUsage(instr);
            TrackBitWiseOrNumberOp(instr);

            TrackFloatSymEquivalence(instr);
        }

        opnd = instr->GetSrc1();
        if (opnd != nullptr)
        {TRACE_IT(428);
            this->ProcessUse(opnd);

            opnd = instr->GetSrc2();
            if (opnd != nullptr)
            {TRACE_IT(429);
                this->ProcessUse(opnd);
            }
        }

        if(IsCollectionPass())
        {TRACE_IT(430);
            continue;
        }

        if (this->tag == Js::DeadStorePhase)
        {TRACE_IT(431);
            switch(instr->m_opcode)
            {
                case Js::OpCode::LdSlot:
                {TRACE_IT(432);
                    DeadStoreOrChangeInstrForScopeObjRemoval(&instrPrev);
                    break;
                }
                case Js::OpCode::InlineArrayPush:
                case Js::OpCode::InlineArrayPop:
                {TRACE_IT(433);
                    IR::Opnd *const thisOpnd = instr->GetSrc1();
                    if(thisOpnd && thisOpnd->IsRegOpnd())
                    {TRACE_IT(434);
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
                {TRACE_IT(435);
                    if(IsPrePass())
                    {TRACE_IT(436);
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
                    {TRACE_IT(437);
                        break;
                    }
                    if(instr->GetDst())
                    {TRACE_IT(438);
                        const int c = instr->GetDst()->AsIntConstOpnd()->GetValue();
                        if(c != 0 && c != -1)
                        {TRACE_IT(439);
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
                    {TRACE_IT(440);
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
            {TRACE_IT(441);
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
        {TRACE_IT(442);
            switch (instr->m_opcode)
            {
                case Js::OpCode::BailOnNoProfile:
                {TRACE_IT(443);
                    this->ProcessBailOnNoProfile(instr, block);
                    // this call could change the last instr of the previous block...  Adjust instrStop.
                    instrStop = block->GetFirstInstr()->m_prev;
                    Assert(this->tag != Js::DeadStorePhase);
                    continue;
                }
                case Js::OpCode::Catch:
                {TRACE_IT(444);
                    if (this->func->DoOptimizeTryCatch() && !this->IsPrePass())
                    {TRACE_IT(445);
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
        {TRACE_IT(446);
            this->ProcessInlineeEnd(instr);
        }

        if (instr->IsLabelInstr() && instr->m_next->m_opcode == Js::OpCode::Catch)
        {TRACE_IT(447);
            if (!this->currentRegion)
            {TRACE_IT(448);
                Assert(!this->func->DoOptimizeTryCatch() && !(this->func->IsSimpleJit() && this->func->hasBailout));
            }
            else
            {TRACE_IT(449);
                Assert(this->currentRegion->GetType() == RegionTypeCatch);
                Region * matchingTryRegion = this->currentRegion->GetMatchingTryRegion();
                Assert(matchingTryRegion);

                // We need live-on-back-edge info to accurately set write-through symbols for try-catches in a loop.
                // Don't set write-through symbols in pre-pass
                if (!this->IsPrePass() && !matchingTryRegion->writeThroughSymbolsSet)
                {TRACE_IT(450);
                    if (this->tag == Js::DeadStorePhase)
                    {TRACE_IT(451);
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
        {TRACE_IT(452);
            if (!this->IsPrePass() && (this->func->DoOptimizeTryCatch() || (this->func->IsSimpleJit() && this->func->hasBailout)))
            {TRACE_IT(453);
                Assert(instr->m_next->IsLabelInstr() && (instr->m_next->AsLabelInstr()->GetRegion() != nullptr));
                Region * tryRegion = instr->m_next->AsLabelInstr()->GetRegion();
                Assert(tryRegion->writeThroughSymbolsSet);
            }
        }
#endif
        ProcessPendingPreOpBailOutInfo(instr);

#if DBG_DUMP
        if (!IsCollectionPass() && IsTraceEnabled() && Js::Configuration::Global.flags.Verbose)
        {TRACE_IT(454);
            Output::Print(_u("-------------------\n"));
            instr->Dump();
            if (block->upwardExposedUses)
            {TRACE_IT(455);
                Output::SkipToColumn(10);
                Output::Print(_u("   Exposed Use: "));
                block->upwardExposedUses->Dump();
            }
            if (block->upwardExposedFields)
            {TRACE_IT(456);
                Output::SkipToColumn(10);
                Output::Print(_u("Exposed Fields: "));
                block->upwardExposedFields->Dump();
            }
            if (block->byteCodeUpwardExposedUsed)
            {TRACE_IT(457);
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
    {TRACE_IT(458);
        Assert(block->loop->fieldHoistCandidates == nullptr);
        block->loop->fieldHoistCandidates = block->fieldHoistCandidates->CopyNew(this->func->m_alloc);
    }

    if (!this->IsPrePass() && !block->isDead && block->isLoopHeader)
    {TRACE_IT(459);
        // Copy the upward exposed use as the live on back edge regs
        block->loop->regAlloc.liveOnBackEdgeSyms = block->upwardExposedUses->CopyNew(this->func->m_alloc);
    }

    Assert(!considerSymAsRealUseInNoImplicitCallUses);

#if DBG_DUMP
    if (this->IsTraceEnabled())
    {TRACE_IT(460);
        Output::Print(_u("******************************* After Process Block *******************************n"));
        DumpBlockData(block);
    }
#endif
}

bool 
BackwardPass::CanDeadStoreInstrForScopeObjRemoval(Sym *sym) const
{TRACE_IT(461);
    if (tag == Js::DeadStorePhase && this->currentInstr->m_func->IsStackArgsEnabled())
    {TRACE_IT(462);
        Func * currFunc = this->currentInstr->m_func;
        bool doScopeObjCreation = currFunc->GetJITFunctionBody()->GetDoScopeObjectCreation();
        switch (this->currentInstr->m_opcode)
        {
            case Js::OpCode::InitCachedScope:
            {TRACE_IT(463);
                if(!doScopeObjCreation && this->currentInstr->GetDst()->IsScopeObjOpnd(currFunc))
                {TRACE_IT(464);
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
            {TRACE_IT(465);
                if (sym && IsFormalParamSym(currFunc, sym))
                {TRACE_IT(466);
                    return true;
                }
                break;
            }
            case Js::OpCode::CommitScope:
            case Js::OpCode::GetCachedFunc:
            {TRACE_IT(467);
                return !doScopeObjCreation && this->currentInstr->GetSrc1()->IsScopeObjOpnd(currFunc);
            }
            case Js::OpCode::BrFncCachedScopeEq:
            case Js::OpCode::BrFncCachedScopeNeq:
            {TRACE_IT(468);
                return !doScopeObjCreation && this->currentInstr->GetSrc2()->IsScopeObjOpnd(currFunc);
            }
            case Js::OpCode::CallHelper:
            {TRACE_IT(469);
                if (!doScopeObjCreation && this->currentInstr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper == IR::JnHelperMethod::HelperOP_InitCachedFuncs)
                {TRACE_IT(470);
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
{TRACE_IT(471);
    IR::Instr * instr = this->currentInstr;
    Func * currFunc = instr->m_func;

    if (this->tag == Js::DeadStorePhase && instr->m_func->IsStackArgsEnabled() && !IsPrePass())
    {TRACE_IT(472);
        switch (instr->m_opcode)
        {
            /*
            *   This LdSlot loads the formal from the formals array. We replace this a Ld_A <ArgInSym>.
            *   ArgInSym is inserted at the beginning of the function during the start of the deadstore pass- for the top func.
            *   In case of inlinee, it will be from the source sym of the ArgOut Instruction to the inlinee.
            */
            case Js::OpCode::LdSlot:
            {TRACE_IT(473);
                IR::Opnd * src1 = instr->GetSrc1();
                if (src1 && src1->IsSymOpnd())
                {TRACE_IT(474);
                    Sym * sym = src1->AsSymOpnd()->m_sym;
                    Assert(sym);
                    if (IsFormalParamSym(currFunc, sym))
                    {TRACE_IT(475);
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
                        {TRACE_IT(476);
                            Output::Print(_u("StackArgFormals : %s (%d) :Replacing LdSlot with Ld_A in Deadstore pass. \n"), instr->m_func->GetJITFunctionBody()->GetDisplayName(), instr->m_func->GetFunctionNumber());
                            Output::Flush();
                        }
                    }
                }
                break;
            }
            case Js::OpCode::CommitScope:
            {TRACE_IT(477);
                if (instr->GetSrc1()->IsScopeObjOpnd(currFunc))
                {TRACE_IT(478);
                    instr->Remove();
                    return true;
                }
                break;
            }
            case Js::OpCode::BrFncCachedScopeEq:
            case Js::OpCode::BrFncCachedScopeNeq:
            {TRACE_IT(479);
                if (instr->GetSrc2()->IsScopeObjOpnd(currFunc))
                {TRACE_IT(480);
                    instr->Remove();
                    return true;
                }
                break;
            }
            case Js::OpCode::CallHelper:
            {TRACE_IT(481);
                //Remove the CALL and all its Argout instrs.
                if (instr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper == IR::JnHelperMethod::HelperOP_InitCachedFuncs)
                {TRACE_IT(482);
                    IR::RegOpnd * scopeObjOpnd = instr->GetSrc2()->GetStackSym()->GetInstrDef()->GetSrc1()->AsRegOpnd();
                    if (scopeObjOpnd->IsScopeObjOpnd(currFunc))
                    {TRACE_IT(483);
                        IR::Instr * instrDef = instr;
                        IR::Instr * nextInstr = instr->m_next;

                        while (instrDef != nullptr)
                        {TRACE_IT(484);
                            IR::Instr * instrToDelete = instrDef;
                            if (instrDef->GetSrc2() != nullptr)
                            {TRACE_IT(485);
                                instrDef = instrDef->GetSrc2()->GetStackSym()->GetInstrDef();
                                Assert(instrDef->m_opcode == Js::OpCode::ArgOut_A);
                            }
                            else
                            {TRACE_IT(486);
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
            {TRACE_IT(487);
                // <dst> = GetCachedFunc <scopeObject>, <functionNum>
                // is converted to 
                // <dst> = NewScFunc <functionNum>, <env: FrameDisplay>

                if (instr->GetSrc1()->IsScopeObjOpnd(currFunc))
                {TRACE_IT(488);
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
{TRACE_IT(489);
    IR::Instr * instr = this->currentInstr;
    if (tag == Js::DeadStorePhase && instr->DoStackArgsOpt(this->func))
    {TRACE_IT(490);
        switch (instr->m_opcode)
        {
            case Js::OpCode::TypeofElem:
            {TRACE_IT(491);
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
    {TRACE_IT(492);
        this->currentBlock->upwardExposedUses->Set(instr->m_func->GetScopeObjSym()->m_id);
    }

    return nullptr;
}

void
BackwardPass::TraceDeadStoreOfInstrsForScopeObjectRemoval()
{TRACE_IT(493);
    IR::Instr * instr = this->currentInstr;

    if (instr->m_func->IsStackArgsEnabled())
    {TRACE_IT(494);
        if ((instr->m_opcode == Js::OpCode::InitCachedScope || instr->m_opcode == Js::OpCode::NewScopeObject) && !IsPrePass())
        {TRACE_IT(495);
            if (PHASE_TRACE1(Js::StackArgFormalsOptPhase))
            {TRACE_IT(496);
                Output::Print(_u("StackArgFormals : %s (%d) :Removing Scope object creation in Deadstore pass. \n"), instr->m_func->GetJITFunctionBody()->GetDisplayName(), instr->m_func->GetFunctionNumber());
                Output::Flush();
            }
        }
    }
}

bool
BackwardPass::IsFormalParamSym(Func * func, Sym * sym) const
{TRACE_IT(497);
    Assert(sym);
    
    if (sym->IsPropertySym())
    {TRACE_IT(498);
        //If the sym is a propertySym, then see if the propertyId is within the range of the formals 
        //We can have other properties stored in the scope object other than the formals (following the formals).
        PropertySym * propSym = sym->AsPropertySym();
        IntConstType    value = propSym->m_propertyId;
        return func->IsFormalsArraySym(propSym->m_stackSym->m_id) &&
            (value >= 0 && value < func->GetJITFunctionBody()->GetInParamsCount() - 1);
    }
    else
    {TRACE_IT(499);
        Assert(sym->IsStackSym());
        return !!func->IsFormalsArraySym(sym->AsStackSym()->m_id);
    }
}

#if DBG_DUMP
void
BackwardPass::DumpBlockData(BasicBlock * block)
{TRACE_IT(500);
    block->DumpHeader();
    if (block->upwardExposedUses) // may be null for dead blocks
    {TRACE_IT(501);
        Output::Print(_u("             Exposed Uses: "));
        block->upwardExposedUses->Dump();
    }

    if (block->typesNeedingKnownObjectLayout)
    {TRACE_IT(502);
        Output::Print(_u("            Needs Known Object Layout: "));
        block->typesNeedingKnownObjectLayout->Dump();
    }

    if (this->DoFieldHoistCandidates() && !block->isDead)
    {TRACE_IT(503);
        Output::Print(_u("            Exposed Field: "));
        block->fieldHoistCandidates->Dump();
    }

    if (block->byteCodeUpwardExposedUsed)
    {TRACE_IT(504);
        Output::Print(_u("   Byte Code Exposed Uses: "));
        block->byteCodeUpwardExposedUsed->Dump();
    }

    if (!this->IsCollectionPass())
    {TRACE_IT(505);
        if (!block->isDead)
        {TRACE_IT(506);
            if (this->DoDeadStoreSlots())
            {TRACE_IT(507);
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
{TRACE_IT(508);
    Assert(instr);
    Assert(instr->HasBailOutInfo());

    IR::BailOutKind implicitCallBailOutKind = needsBailOutOnImplicitCall ? IR::BailOutOnImplicitCalls : IR::BailOutInvalid;

    const IR::BailOutKind instrBailOutKind = instr->GetBailOutKind();
    if (instrBailOutKind & IR::BailOutMarkTempObject)
    {TRACE_IT(509);
        // Don't remove the implicit call pre op bailout for mark temp object
        // Remove the mark temp object bit, as we don't need it after the dead store pass
        instr->SetBailOutKind(instrBailOutKind & ~IR::BailOutMarkTempObject);
        return true;
    }

    const IR::BailOutKind instrImplicitCallBailOutKind = instrBailOutKind & ~IR::BailOutKindBits;
    if(instrImplicitCallBailOutKind == IR::BailOutOnImplicitCallsPreOp)
    {TRACE_IT(510);
        if(needsBailOutOnImplicitCall)
        {TRACE_IT(511);
            implicitCallBailOutKind = IR::BailOutOnImplicitCallsPreOp;
        }
    }
    else if(instrImplicitCallBailOutKind != IR::BailOutOnImplicitCalls && instrImplicitCallBailOutKind != IR::BailOutInvalid)
    {TRACE_IT(512);
        // This bailout kind (the value of 'instrImplicitCallBailOutKind') must guarantee that implicit calls will not happen.
        // If it doesn't make such a guarantee, it must be possible to merge this bailout kind with an implicit call bailout
        // kind, and therefore should be part of BailOutKindBits.
        Assert(!needsBailOutOnImplicitCall);
        return true;
    }

    if(instrImplicitCallBailOutKind == implicitCallBailOutKind)
    {TRACE_IT(513);
        return true;
    }

    const IR::BailOutKind newBailOutKind = instrBailOutKind - instrImplicitCallBailOutKind + implicitCallBailOutKind;
    if(newBailOutKind == IR::BailOutInvalid)
    {TRACE_IT(514);
        return false;
    }

    instr->SetBailOutKind(newBailOutKind);
    return true;
}

bool
BackwardPass::ProcessNoImplicitCallUses(IR::Instr *const instr)
{TRACE_IT(515);
    Assert(instr);

    if(instr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {TRACE_IT(516);
        return false;
    }
    Assert(tag == Js::DeadStorePhase);
    Assert(!instr->GetDst());
    Assert(instr->GetSrc1());
    Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsSymOpnd());
    Assert(!instr->GetSrc2() || instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsSymOpnd());

    if(IsCollectionPass())
    {TRACE_IT(517);
        return true;
    }

    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < sizeof(srcs) / sizeof(srcs[0]) && srcs[i]; ++i)
    {TRACE_IT(518);
        IR::Opnd *const src = srcs[i];
        IR::ArrayRegOpnd *arraySrc = nullptr;
        Sym *sym;
        switch(src->GetKind())
        {
            case IR::OpndKindReg:
            {TRACE_IT(519);
                IR::RegOpnd *const regSrc = src->AsRegOpnd();
                sym = regSrc->m_sym;
                if(considerSymAsRealUseInNoImplicitCallUses && considerSymAsRealUseInNoImplicitCallUses == sym)
                {TRACE_IT(520);
                    considerSymAsRealUseInNoImplicitCallUses = nullptr;
                    ProcessStackSymUse(sym->AsStackSym(), true);
                }
                if(regSrc->IsArrayRegOpnd())
                {TRACE_IT(521);
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
        {TRACE_IT(522);
            if(valueType.HasNoMissingValues())
            {TRACE_IT(523);
                currentBlock->noImplicitCallNoMissingValuesUses->Set(sym->m_id);
            }
            if(!valueType.HasVarElements())
            {TRACE_IT(524);
                currentBlock->noImplicitCallNativeArrayUses->Set(sym->m_id);
            }
            if(arraySrc)
            {
                ProcessArrayRegOpndUse(instr, arraySrc);
            }
        }
    }

    if(!IsPrePass())
    {TRACE_IT(525);
        currentBlock->RemoveInstr(instr);
    }
    return true;
}

void
BackwardPass::ProcessNoImplicitCallDef(IR::Instr *const instr)
{TRACE_IT(526);
    Assert(tag == Js::DeadStorePhase);
    Assert(instr);

    IR::Opnd *const dst = instr->GetDst();
    if(!dst)
    {TRACE_IT(527);
        return;
    }

    Sym *dstSym;
    switch(dst->GetKind())
    {
        case IR::OpndKindReg:
            dstSym = dst->AsRegOpnd()->m_sym;
            break;

        case IR::OpndKindSym:
            dstSym = dst->AsSymOpnd()->m_sym;
            if(!dstSym->IsPropertySym())
            {TRACE_IT(528);
                return;
            }
            break;

        default:
            return;
    }

    if(!currentBlock->noImplicitCallUses->TestAndClear(dstSym->m_id))
    {TRACE_IT(529);
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
    {TRACE_IT(530);
        return;
    }
    if(dst->IsRegOpnd() && src->IsRegOpnd())
    {TRACE_IT(531);
        if(!OpCodeAttr::NonIntTransfer(instr->m_opcode))
        {TRACE_IT(532);
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
    {TRACE_IT(533);
        return;
    }

    Sym *srcSym;
    switch(src->GetKind())
    {
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
    {TRACE_IT(534);
        currentBlock->noImplicitCallNoMissingValuesUses->Set(srcSym->m_id);
    }
    if(transferNativeArrayUse)
    {TRACE_IT(535);
        currentBlock->noImplicitCallNativeArrayUses->Set(srcSym->m_id);
    }
    if(transferJsArrayHeadSegmentSymUse)
    {TRACE_IT(536);
        currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->Set(srcSym->m_id);
    }
    if(transferArrayLengthSymUse)
    {TRACE_IT(537);
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
{TRACE_IT(538);
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
{TRACE_IT(539);
    Assert(instr);
    Assert(instr->m_opcode != Js::OpCode::NoImplicitCallUses);

    // Skip byte-code uses
    IR::Instr *prevInstr = instr->m_prev;
    while(
        prevInstr &&
        !prevInstr->IsLabelInstr() &&
        (!prevInstr->IsRealInstr() || prevInstr->IsByteCodeUsesInstr()) &&
        prevInstr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {TRACE_IT(540);
        prevInstr = prevInstr->m_prev;
    }

    // Find the corresponding use in a NoImplicitCallUses instruction
    for(; prevInstr && prevInstr->m_opcode == Js::OpCode::NoImplicitCallUses; prevInstr = prevInstr->m_prev)
    {TRACE_IT(541);
        IR::Opnd *const checkedSrcs[] = { prevInstr->GetSrc1(), prevInstr->GetSrc2() };
        for(int i = 0; i < sizeof(checkedSrcs) / sizeof(checkedSrcs[0]) && checkedSrcs[i]; ++i)
        {TRACE_IT(542);
            IR::Opnd *const checkedSrc = checkedSrcs[i];
            if(checkedSrc->IsEqual(opnd) && IsCheckedUse(checkedSrc))
            {TRACE_IT(543);
                if(noImplicitCallUsesInstrRef)
                {TRACE_IT(544);
                    *noImplicitCallUsesInstrRef = prevInstr;
                }
                return checkedSrc;
            }
        }
    }

    if(noImplicitCallUsesInstrRef)
    {TRACE_IT(545);
        *noImplicitCallUsesInstrRef = nullptr;
    }
    return nullptr;
}

void
BackwardPass::ProcessArrayRegOpndUse(IR::Instr *const instr, IR::ArrayRegOpnd *const arrayRegOpnd)
{TRACE_IT(546);
    Assert(tag == Js::DeadStorePhase);
    Assert(!IsCollectionPass());
    Assert(instr);
    Assert(arrayRegOpnd);

    if(!(arrayRegOpnd->HeadSegmentSym() || arrayRegOpnd->HeadSegmentLengthSym() || arrayRegOpnd->LengthSym()))
    {TRACE_IT(547);
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
    {TRACE_IT(548);
        bool headSegmentIsLoadedButUnused =
            instr->loadedArrayHeadSegment &&
            arrayRegOpnd->HeadSegmentSym() &&
            !block->upwardExposedUses->Test(arrayRegOpnd->HeadSegmentSym()->m_id);
        const bool headSegmentLengthIsLoadedButUnused =
            instr->loadedArrayHeadSegmentLength &&
            arrayRegOpnd->HeadSegmentLengthSym() &&
            !block->upwardExposedUses->Test(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
        if(headSegmentLengthIsLoadedButUnused && instr->extractedUpperBoundCheckWithoutHoisting)
        {TRACE_IT(549);
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
            {TRACE_IT(550);
                // The head segment length is on the head segment, so the bound check now uses the head segment sym
                headSegmentIsLoadedButUnused = false;
            }
        }

        if(headSegmentIsLoadedButUnused || headSegmentLengthIsLoadedButUnused)
        {TRACE_IT(551);
            // Check if the head segment / head segment length are being loaded here. If so, remove them and let the fast
            // path load them since it does a better job.
            IR::ArrayRegOpnd *noImplicitCallArrayUse = nullptr;
            if(isJsArray)
            {TRACE_IT(552);
                IR::Opnd *const use =
                    FindNoImplicitCallUse(
                        instr,
                        arrayRegOpnd,
                        [&](IR::Opnd *const checkedSrc) -> bool
                        {
                            const ValueType checkedSrcValueType(checkedSrc->GetValueType());
                            if(!checkedSrcValueType.IsLikelyObject() ||
                                checkedSrcValueType.GetObjectType() != arrayValueType.GetObjectType())
                            {TRACE_IT(553);
                                return false;
                            }

                            IR::RegOpnd *const checkedRegSrc = checkedSrc->AsRegOpnd();
                            if(!checkedRegSrc->IsArrayRegOpnd())
                            {TRACE_IT(554);
                                return false;
                            }

                            IR::ArrayRegOpnd *const checkedArraySrc = checkedRegSrc->AsArrayRegOpnd();
                            if(headSegmentIsLoadedButUnused &&
                                checkedArraySrc->HeadSegmentSym() != arrayRegOpnd->HeadSegmentSym())
                            {TRACE_IT(555);
                                return false;
                            }
                            if(headSegmentLengthIsLoadedButUnused &&
                                checkedArraySrc->HeadSegmentLengthSym() != arrayRegOpnd->HeadSegmentLengthSym())
                            {TRACE_IT(556);
                                return false;
                            }
                            return true;
                        });
                if(use)
                {TRACE_IT(557);
                    noImplicitCallArrayUse = use->AsRegOpnd()->AsArrayRegOpnd();
                }
            }
            else if(headSegmentLengthIsLoadedButUnused)
            {TRACE_IT(558);
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
                {TRACE_IT(559);
                    Assert(noImplicitCallUsesInstr);
                    Assert(!noImplicitCallUsesInstr->GetDst());
                    Assert(noImplicitCallUsesInstr->GetSrc1());
                    if(use == noImplicitCallUsesInstr->GetSrc1())
                    {TRACE_IT(560);
                        if(noImplicitCallUsesInstr->GetSrc2())
                        {TRACE_IT(561);
                            noImplicitCallUsesInstr->ReplaceSrc1(noImplicitCallUsesInstr->UnlinkSrc2());
                        }
                        else
                        {TRACE_IT(562);
                            noImplicitCallUsesInstr->FreeSrc1();
                            noImplicitCallUsesInstr->m_opcode = Js::OpCode::Nop;
                        }
                    }
                    else
                    {TRACE_IT(563);
                        Assert(use == noImplicitCallUsesInstr->GetSrc2());
                        noImplicitCallUsesInstr->FreeSrc2();
                    }
                }
            }

            if(headSegmentIsLoadedButUnused &&
                (!isJsArray || !arrayRegOpnd->HeadSegmentLengthSym() || headSegmentLengthIsLoadedButUnused))
            {TRACE_IT(564);
                // For JS arrays, the head segment length load is dependent on the head segment. So, only remove the head
                // segment load if the head segment length load can also be removed.
                arrayRegOpnd->RemoveHeadSegmentSym();
                instr->loadedArrayHeadSegment = false;
                if(noImplicitCallArrayUse)
                {TRACE_IT(565);
                    noImplicitCallArrayUse->RemoveHeadSegmentSym();
                }
            }
            if(headSegmentLengthIsLoadedButUnused)
            {TRACE_IT(566);
                arrayRegOpnd->RemoveHeadSegmentLengthSym();
                instr->loadedArrayHeadSegmentLength = false;
                if(noImplicitCallArrayUse)
                {TRACE_IT(567);
                    noImplicitCallArrayUse->RemoveHeadSegmentLengthSym();
                }
            }
        }
    }

    if(isJsArray && instr->m_opcode != Js::OpCode::NoImplicitCallUses)
    {TRACE_IT(568);
        // Only uses in NoImplicitCallUses instructions are counted toward liveness
        return;
    }

    // Treat dependent syms as uses. For JS arrays, only uses in NoImplicitCallUses count because only then the assumptions made
    // on the dependent syms are guaranteed to be valid. Similarly for typed arrays, a head segment length sym use counts toward
    // liveness only in a NoImplicitCallUses instruction.
    if(arrayRegOpnd->HeadSegmentSym())
    {TRACE_IT(569);
        ProcessStackSymUse(arrayRegOpnd->HeadSegmentSym(), true);
        if(isJsArray)
        {TRACE_IT(570);
            block->noImplicitCallUses->Set(arrayRegOpnd->HeadSegmentSym()->m_id);
            block->noImplicitCallJsArrayHeadSegmentSymUses->Set(arrayRegOpnd->HeadSegmentSym()->m_id);
        }
    }
    if(arrayRegOpnd->HeadSegmentLengthSym())
    {TRACE_IT(571);
        if(isJsArray)
        {TRACE_IT(572);
            ProcessStackSymUse(arrayRegOpnd->HeadSegmentLengthSym(), true);
            block->noImplicitCallUses->Set(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
            block->noImplicitCallJsArrayHeadSegmentSymUses->Set(arrayRegOpnd->HeadSegmentLengthSym()->m_id);
        }
        else
        {TRACE_IT(573);
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
            {TRACE_IT(574);
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
{TRACE_IT(575);
    if (this->tag != Js::DeadStorePhase || IsCollectionPass())
    {TRACE_IT(576);
        return;
    }

    if (!instr->IsNewScObjectInstr())
    {TRACE_IT(577);
        return;
    }

    if (instr->HasBailOutInfo())
    {TRACE_IT(578);
        Assert(instr->IsProfiledInstr());
        Assert(instr->GetBailOutKind() == IR::BailOutFailedCtorGuardCheck);
        Assert(instr->GetDst()->IsRegOpnd());

        BasicBlock * block = this->currentBlock;
        StackSym* objSym = instr->GetDst()->AsRegOpnd()->GetStackSym();

        if (block->upwardExposedUses->Test(objSym->m_id))
        {TRACE_IT(579);
            // If the object created here is used downstream, let's capture any property operations we must protect.

            Assert(instr->GetDst()->AsRegOpnd()->GetStackSym()->HasObjectTypeSym());

            JITTimeConstructorCache* ctorCache = instr->m_func->GetConstructorCache(static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId));

            if (block->stackSymToFinalType != nullptr)
            {TRACE_IT(580);
                // NewScObject is the origin of the object pointer. If we have a final type in hand, do the
                // transition here.
                AddPropertyCacheBucket *pBucket = block->stackSymToFinalType->Get(objSym->m_id);
                if (pBucket &&
                    pBucket->GetInitialType() != nullptr &&
                    pBucket->GetFinalType() != pBucket->GetInitialType())
                {TRACE_IT(581);
                    Assert(pBucket->GetInitialType() == ctorCache->GetType());
                    if (!this->IsPrePass())
                    {TRACE_IT(582);
                        this->InsertTypeTransition(instr->m_next, objSym, pBucket);
                    }
#if DBG
                    pBucket->deadStoreUnavailableInitialType = pBucket->GetInitialType();
                    if (pBucket->deadStoreUnavailableFinalType == nullptr)
                    {TRACE_IT(583);
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
            {TRACE_IT(584);
                ObjTypeGuardBucket* bucket = block->stackSymToGuardedProperties->Get(objSym->m_id);
                if (bucket != nullptr)
                {TRACE_IT(585);
                    BVSparse<JitArenaAllocator>* guardedPropertyOps = bucket->GetGuardedPropertyOps();
                    if (guardedPropertyOps != nullptr)
                    {TRACE_IT(586);
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
        {TRACE_IT(587);
            // If the object is not used downstream, let's remove the bailout and let the lowerer emit a fast path along with
            // the fallback on helper, if the ctor cache ever became invalid.
            instr->ClearBailOutInfo();
            if (preOpBailOutInstrToProcess == instr)
            {TRACE_IT(588);
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
{TRACE_IT(589);
    Assert(tag == Js::DeadStorePhase);
    Assert(!IsPrePass());
    Assert(instr);

    if(!origOpnd)
    {TRACE_IT(590);
        return;
    }

    IR::Instr *opndOwnerInstr = instr;
    switch(instr->m_opcode)
    {
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
    {
        case IR::OpndKindIndir:
            opnd = opnd->AsIndirOpnd()->GetBaseOpnd();
            // fall-through

        case IR::OpndKindReg:
        {TRACE_IT(591);
            IR::RegOpnd *const regOpnd = opnd->AsRegOpnd();
            sym = regOpnd->m_sym;
            arrayOpnd = regOpnd->IsArrayRegOpnd() ? regOpnd->AsArrayRegOpnd() : nullptr;
            break;
        }

        case IR::OpndKindSym:
            sym = opnd->AsSymOpnd()->m_sym;
            if(!sym->IsPropertySym())
            {TRACE_IT(592);
                return;
            }
            arrayOpnd = nullptr;
            break;

        default:
            return;
    }

    const ValueType valueType(opnd->GetValueType());
    if(!valueType.IsAnyOptimizedArray())
    {TRACE_IT(593);
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
    {TRACE_IT(594);
        return;
    }

    // We have a definitely-array value type for the base, but either implicit calls are not currently being disabled for
    // legally using the value type as a definite array, or we are not currently bailing out upon creating a missing value
    // for legally using the value type as a definite array with no missing values.

    // For source opnds, ensure that a NoImplicitCallUses immediately precedes this instruction. Otherwise, convert the value
    // type to an appropriate version so that the lowerer doesn't incorrectly treat it as it says.
    if(opnd != opndOwnerInstr->GetDst())
    {TRACE_IT(595);
        if(isJsArray)
        {TRACE_IT(596);
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
            {TRACE_IT(597);
                // Implicit calls will be disabled to the point immediately before this instruction
                changeArray = false;

                const ValueType checkedSrcValueType(checkedSrc->GetValueType());
                if(changeNativeArray &&
                    !checkedSrcValueType.HasVarElements() &&
                    checkedSrcValueType.HasIntElements() == valueType.HasIntElements())
                {TRACE_IT(598);
                    // If necessary, instructions before this will bail out on converting a native array
                    changeNativeArray = false;
                }

                if(changeNoMissingValues && checkedSrcValueType.HasNoMissingValues())
                {TRACE_IT(599);
                    // If necessary, instructions before this will bail out on creating a missing value
                    changeNoMissingValues = false;
                }

                if((removeHeadSegmentSym || removeHeadSegmentLengthSym || removeLengthSym) && checkedSrc->IsRegOpnd())
                {TRACE_IT(600);
                    IR::RegOpnd *const checkedRegSrc = checkedSrc->AsRegOpnd();
                    if(checkedRegSrc->IsArrayRegOpnd())
                    {TRACE_IT(601);
                        IR::ArrayRegOpnd *const checkedArraySrc = checkedSrc->AsRegOpnd()->AsArrayRegOpnd();
                        if(removeHeadSegmentSym && checkedArraySrc->HeadSegmentSym() == arrayOpnd->HeadSegmentSym())
                        {TRACE_IT(602);
                            // If necessary, instructions before this will bail out upon invalidating head segment sym
                            removeHeadSegmentSym = false;
                        }
                        if(removeHeadSegmentLengthSym &&
                            checkedArraySrc->HeadSegmentLengthSym() == arrayOpnd->HeadSegmentLengthSym())
                        {TRACE_IT(603);
                            // If necessary, instructions before this will bail out upon invalidating head segment length sym
                            removeHeadSegmentLengthSym = false;
                        }
                        if(removeLengthSym && checkedArraySrc->LengthSym() == arrayOpnd->LengthSym())
                        {TRACE_IT(604);
                            // If necessary, instructions before this will bail out upon invalidating a length sym
                            removeLengthSym = false;
                        }
                    }
                }
            }
        }
        else
        {TRACE_IT(605);
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
            {TRACE_IT(606);
                // Implicit calls will be disabled to the point immediately before this instruction
                removeHeadSegmentLengthSym = false;
            }
        }
    }

    if(changeArray || changeNativeArray)
    {TRACE_IT(607);
        if(arrayOpnd)
        {TRACE_IT(608);
            opnd = arrayOpnd->CopyAsRegOpnd(opndOwnerInstr->m_func);
            if (origOpnd->IsIndirOpnd())
            {TRACE_IT(609);
                origOpnd->AsIndirOpnd()->ReplaceBaseOpnd(opnd->AsRegOpnd());
            }
            else
            {TRACE_IT(610);
                opndOwnerInstr->Replace(arrayOpnd, opnd);
            }
            arrayOpnd = nullptr;
        }
        opnd->SetValueType(valueType.ToLikely());
    }
    else
    {TRACE_IT(611);
        if(changeNoMissingValues)
        {TRACE_IT(612);
            opnd->SetValueType(valueType.SetHasNoMissingValues(false));
        }
        if(removeHeadSegmentSym)
        {TRACE_IT(613);
            Assert(arrayOpnd);
            arrayOpnd->RemoveHeadSegmentSym();
        }
        if(removeHeadSegmentLengthSym)
        {TRACE_IT(614);
            Assert(arrayOpnd);
            arrayOpnd->RemoveHeadSegmentLengthSym();
        }
        if(removeLengthSym)
        {TRACE_IT(615);
            Assert(arrayOpnd);
            arrayOpnd->RemoveLengthSym();
        }
    }
}

void
BackwardPass::UpdateArrayBailOutKind(IR::Instr *const instr)
{TRACE_IT(616);
    Assert(!IsPrePass());
    Assert(instr);
    Assert(instr->HasBailOutInfo());

    if ((instr->m_opcode != Js::OpCode::StElemI_A && instr->m_opcode != Js::OpCode::StElemI_A_Strict &&
        instr->m_opcode != Js::OpCode::Memcopy && instr->m_opcode != Js::OpCode::Memset) ||
        !instr->GetDst()->IsIndirOpnd())
    {TRACE_IT(617);
        return;
    }

    IR::RegOpnd *const baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
    const ValueType baseValueType(baseOpnd->GetValueType());
    if(baseValueType.IsNotArrayOrObjectWithArray())
    {TRACE_IT(618);
        return;
    }

    IR::BailOutKind includeBailOutKinds = IR::BailOutInvalid;
    if(!baseValueType.IsNotNativeArray() &&
        (!baseValueType.IsLikelyNativeArray() || instr->GetSrc1()->IsVar()) &&
        !currentBlock->noImplicitCallNativeArrayUses->IsEmpty())
    {TRACE_IT(619);
        // There is an upwards-exposed use of a native array. Since the array referenced by this instruction can be aliased,
        // this instruction needs to bail out if it converts the native array even if this array specifically is not
        // upwards-exposed.
        includeBailOutKinds |= IR::BailOutConvertedNativeArray;
    }

    if(baseOpnd->IsArrayRegOpnd() && baseOpnd->AsArrayRegOpnd()->EliminatedUpperBoundCheck())
    {TRACE_IT(620);
        if(instr->extractedUpperBoundCheckWithoutHoisting && !currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty())
        {TRACE_IT(621);
            // See comment below regarding head segment invalidation. A failed upper bound check usually means that it will
            // invalidate the head segment length, so change the bailout kind on the upper bound check to have it bail out for
            // the right reason. Even though the store may actually occur in a non-head segment, which would not invalidate the
            // head segment or length, any store outside the head segment bounds causes head segment load elimination to be
            // turned off for the store, because the segment structure of the array is not guaranteed to be the same every time.
            IR::Instr *upperBoundCheck = this->globOpt->FindUpperBoundsCheckInstr(instr);
            Assert(upperBoundCheck && upperBoundCheck != instr);

            if(upperBoundCheck->GetBailOutKind() == IR::BailOutOnArrayAccessHelperCall)
            {TRACE_IT(622);
                upperBoundCheck->SetBailOutKind(IR::BailOutOnInvalidatedArrayHeadSegment);
            }
            else
            {TRACE_IT(623);
                Assert(upperBoundCheck->GetBailOutKind() == IR::BailOutOnFailedHoistedBoundCheck);
            }
        }
    }
    else
    {TRACE_IT(624);
        if(!currentBlock->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty())
        {TRACE_IT(625);
            // There is an upwards-exposed use of a segment sym. Since the head segment syms referenced by this instruction can
            // be aliased, this instruction needs to bail out if it changes the segment syms it references even if the ones it
            // references specifically are not upwards-exposed. This bailout kind also guarantees that this element store will
            // not create missing values.
            includeBailOutKinds |= IR::BailOutOnInvalidatedArrayHeadSegment;
        }
        else if(
            !currentBlock->noImplicitCallNoMissingValuesUses->IsEmpty() &&
            !(instr->GetBailOutKind() & IR::BailOutOnArrayAccessHelperCall))
        {TRACE_IT(626);
            // There is an upwards-exposed use of an array with no missing values. Since the array referenced by this
            // instruction can be aliased, this instruction needs to bail out if it creates a missing value in the array even if
            // this array specifically is not upwards-exposed.
            includeBailOutKinds |= IR::BailOutOnMissingValue;
        }

        if(!baseValueType.IsNotArray() && !currentBlock->noImplicitCallArrayLengthSymUses->IsEmpty())
        {TRACE_IT(627);
            // There is an upwards-exposed use of a length sym. Since the length sym referenced by this instruction can be
            // aliased, this instruction needs to bail out if it changes the length sym it references even if the ones it
            // references specifically are not upwards-exposed.
            includeBailOutKinds |= IR::BailOutOnInvalidatedArrayLength;
        }
    }

    if(!includeBailOutKinds)
    {TRACE_IT(628);
        return;
    }

    Assert(!(includeBailOutKinds & ~IR::BailOutKindBits));
    instr->SetBailOutKind(instr->GetBailOutKind() | includeBailOutKinds);
}

bool
BackwardPass::ProcessStackSymUse(StackSym * stackSym, BOOLEAN isNonByteCodeUse)
{TRACE_IT(629);
    BasicBlock * block = this->currentBlock;

    if (this->DoByteCodeUpwardExposedUsed())
    {TRACE_IT(630);
        if (!isNonByteCodeUse && stackSym->HasByteCodeRegSlot())
        {TRACE_IT(631);
            // Always track the sym use on the var sym.
            StackSym * byteCodeUseSym = stackSym;
            if (byteCodeUseSym->IsTypeSpec())
            {TRACE_IT(632);
                // It has to have a var version for byte code regs
                byteCodeUseSym = byteCodeUseSym->GetVarEquivSym(nullptr);
            }
            block->byteCodeUpwardExposedUsed->Set(byteCodeUseSym->m_id);
#if DBG
            // We can only track first level function stack syms right now
            if (byteCodeUseSym->GetByteCodeFunc() == this->func)
            {TRACE_IT(633);
                Js::RegSlot byteCodeRegSlot = byteCodeUseSym->GetByteCodeRegSlot();
                if (block->byteCodeRestoreSyms[byteCodeRegSlot] != byteCodeUseSym)
                {TRACE_IT(634);
                    AssertMsg(block->byteCodeRestoreSyms[byteCodeRegSlot] == nullptr,
                        "Can't have two active lifetime for the same byte code register");
                    block->byteCodeRestoreSyms[byteCodeRegSlot] = byteCodeUseSym;
                }
            }
#endif
        }
    }

    if(IsCollectionPass())
    {TRACE_IT(635);
        return true;
    }

    if (this->DoMarkTempObjects())
    {TRACE_IT(636);
        Assert((block->loop != nullptr) == block->tempObjectTracker->HasTempTransferDependencies());
        block->tempObjectTracker->ProcessUse(stackSym, this);
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {TRACE_IT(637);
        Assert((block->loop != nullptr) == block->tempObjectVerifyTracker->HasTempTransferDependencies());
        block->tempObjectVerifyTracker->ProcessUse(stackSym, this);
    }
#endif
    return !!block->upwardExposedUses->TestAndSet(stackSym->m_id);
}

bool
BackwardPass::ProcessSymUse(Sym * sym, bool isRegOpndUse, BOOLEAN isNonByteCodeUse)
{TRACE_IT(638);
    BasicBlock * block = this->currentBlock;
    
    if (CanDeadStoreInstrForScopeObjRemoval(sym))   
    {TRACE_IT(639);
        return false;
    }

    if (sym->IsPropertySym())
    {TRACE_IT(640);
        PropertySym * propertySym = sym->AsPropertySym();
        ProcessStackSymUse(propertySym->m_stackSym, isNonByteCodeUse);

        if(IsCollectionPass())
        {TRACE_IT(641);
            return true;
        }

        Assert((block->fieldHoistCandidates != nullptr) == this->DoFieldHoistCandidates());

        if (block->fieldHoistCandidates && GlobOpt::TransferSrcValue(this->currentInstr))
        {TRACE_IT(642);
            // If the instruction doesn't transfer the src value to dst, it will not be copyprop'd
            // So we can't hoist those.
            block->fieldHoistCandidates->Set(propertySym->m_id);
        }

        if (this->DoDeadStoreSlots())
        {TRACE_IT(643);
            block->slotDeadStoreCandidates->Clear(propertySym->m_id);
        }

        if (tag == Js::BackwardPhase)
        {TRACE_IT(644);
            // Backward phase tracks liveness of fields to tell GlobOpt where we may need bailout.
            return this->ProcessPropertySymUse(propertySym);
        }
        else
        {TRACE_IT(645);
            // Dead-store phase tracks copy propped syms, so it only cares about ByteCodeUses we inserted,
            // not live fields.
            return false;
        }
    }

    StackSym * stackSym = sym->AsStackSym();
    bool isUsed = ProcessStackSymUse(stackSym, isNonByteCodeUse);

    if (!IsCollectionPass() && isRegOpndUse && this->DoMarkTempNumbers())
    {TRACE_IT(646);
        // Collect mark temp number information
        Assert((block->loop != nullptr) == block->tempNumberTracker->HasTempTransferDependencies());
        block->tempNumberTracker->ProcessUse(stackSym, this);
    }

    return isUsed;
}

bool
BackwardPass::MayPropertyBeWrittenTo(Js::PropertyId propertyId)
{TRACE_IT(647);
    return this->func->anyPropertyMayBeWrittenTo ||
        (this->func->propertiesWrittenTo != nullptr && this->func->propertiesWrittenTo->ContainsKey(propertyId));
}

void
BackwardPass::ProcessPropertySymOpndUse(IR::PropertySymOpnd * opnd)
{TRACE_IT(648);

    // If this operand doesn't participate in the type check sequence it's a pass-through.
    // We will not set any bits on the operand and we will ignore them when lowering.
    if (!opnd->IsTypeCheckSeqCandidate())
    {TRACE_IT(649);
        return;
    }

    AssertMsg(opnd->HasObjectTypeSym(), "Optimized property sym operand without a type sym?");
    SymID typeSymId = opnd->GetObjectTypeSym()->m_id;

    BasicBlock * block = this->currentBlock;
    if (this->tag == Js::BackwardPhase)
    {TRACE_IT(650);
        // In the backward phase, we have no availability info, and we're trying to see
        // where there are live fields so we can decide where to put bailouts.

        Assert(opnd->MayNeedTypeCheckProtection());

        block->upwardExposedFields->Set(typeSymId);

        TrackObjTypeSpecWriteGuards(opnd, block);
    }
    else
    {TRACE_IT(651);
        // In the dead-store phase, we're trying to see where the lowered code needs to make sure to check
        // types for downstream load/stores. We're also setting up the upward-exposed uses at loop headers
        // so register allocation will be correct.

        Assert(opnd->MayNeedTypeCheckProtection());

        const bool isStore = opnd == this->currentInstr->GetDst();

        // Note that we don't touch upwardExposedUses here.
        if (opnd->IsTypeAvailable())
        {TRACE_IT(652);
            opnd->SetTypeDead(!block->upwardExposedFields->TestAndSet(typeSymId));

            if (opnd->IsTypeChecked() && opnd->IsObjectHeaderInlined())
            {TRACE_IT(653);
                // The object's type must not change in a way that changes the layout.
                // If we see a StFld with a type check bailout between here and the type check that guards this
                // property, we must not dead-store the StFld's type check bailout, even if that operand's type appears
                // dead, because that object may alias this one.
                BVSparse<JitArenaAllocator>* bv = block->typesNeedingKnownObjectLayout;
                if (bv == nullptr)
                {TRACE_IT(654);
                    bv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
                    block->typesNeedingKnownObjectLayout = bv;
                }
                bv->Set(typeSymId);
            }
        }
        else
        {TRACE_IT(655);
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
            {TRACE_IT(656);
                bv->Clear(typeSymId);
            }
        }

        bool mayNeedTypeTransition = true;
        if (!opnd->HasTypeMismatch() && func->DoGlobOpt())
        {TRACE_IT(657);
            mayNeedTypeTransition = !isStore;
        }
        if (mayNeedTypeTransition &&
            !this->IsPrePass() &&
            !this->currentInstr->HasBailOutInfo() &&
            (opnd->NeedsPrimaryTypeCheck() ||
             opnd->NeedsLocalTypeCheck() ||
             opnd->NeedsLoadFromProtoTypeCheck()))
        {TRACE_IT(658);
            // This is a "checked" opnd that nevertheless will have some kind of type check generated for it.
            // (Typical case is a load from prototype with no upstream guard.)
            // If the type check fails, we will call a helper, which will require that the type be correct here.
            // Final type can't be pushed up past this point. Do whatever type transition is required.
            if (block->stackSymToFinalType != nullptr)
            {TRACE_IT(659);
                StackSym *baseSym = opnd->GetObjectSym();
                AddPropertyCacheBucket *pBucket = block->stackSymToFinalType->Get(baseSym->m_id);
                if (pBucket &&
                    pBucket->GetFinalType() != nullptr &&
                    pBucket->GetFinalType() != pBucket->GetInitialType())
                {TRACE_IT(660);
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
{TRACE_IT(661);
    Assert(tag == Js::DeadStorePhase);
    Assert(opnd->IsTypeCheckSeqCandidate());

    // Now that we're in the dead store pass and we know definitively which operations will have a type
    // check and which are protected by an upstream type check, we can push the lists of guarded properties
    // up the flow graph and drop them on the type checks for the corresponding object symbol.
    if (opnd->IsTypeCheckSeqParticipant())
    {TRACE_IT(662);
        // Add this operation to the list of guarded operations for this object symbol.
        HashTable<ObjTypeGuardBucket>* stackSymToGuardedProperties = block->stackSymToGuardedProperties;
        if (stackSymToGuardedProperties == nullptr)
        {TRACE_IT(663);
            stackSymToGuardedProperties = HashTable<ObjTypeGuardBucket>::New(this->tempAlloc, 8);
            block->stackSymToGuardedProperties = stackSymToGuardedProperties;
        }

        StackSym* objSym = opnd->GetObjectSym();
        ObjTypeGuardBucket* bucket = stackSymToGuardedProperties->FindOrInsertNew(objSym->m_id);
        BVSparse<JitArenaAllocator>* guardedPropertyOps = bucket->GetGuardedPropertyOps();
        if (guardedPropertyOps == nullptr)
        {TRACE_IT(664);
            // The bit vectors we push around the flow graph only need to live as long as this phase.
            guardedPropertyOps = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
            bucket->SetGuardedPropertyOps(guardedPropertyOps);
        }

#if DBG
        FOREACH_BITSET_IN_SPARSEBV(propOpId, guardedPropertyOps)
        {TRACE_IT(665);
            JITObjTypeSpecFldInfo* existingFldInfo = this->func->GetGlobalObjTypeSpecFldInfo(propOpId);
            Assert(existingFldInfo != nullptr);

            if (existingFldInfo->GetPropertyId() != opnd->GetPropertyId())
            {TRACE_IT(666);
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
            {TRACE_IT(667);
                if (existingFldInfo->IsPoly() && opnd->IsPoly() &&
                    (!GlobOpt::AreTypeSetsIdentical(existingFldInfo->GetEquivalentTypeSet(), opnd->GetEquivalentTypeSet()) ||
                    (existingFldInfo->GetSlotIndex() != opnd->GetSlotIndex())))
                {TRACE_IT(668);
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
        {TRACE_IT(669);
            Assert(opnd->IsMono());
            JITTypeHolder monoGuardType = opnd->IsInitialTypeChecked() ? opnd->GetInitialType() : opnd->GetType();
            bucket->SetMonoGuardType(monoGuardType);
        }

        if (opnd->NeedsPrimaryTypeCheck())
        {TRACE_IT(670);
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
            {TRACE_IT(671);
                // Stop pushing the mono guard type up if it is being checked here.
                if (bucket->NeedsMonoCheck())
                {TRACE_IT(672);
                    if (this->currentInstr->HasEquivalentTypeCheckBailOut())
                    {TRACE_IT(673);
                        // Some instr protected by this one requires a monomorphic type check. (E.g., final type opt,
                        // fixed field not loaded from prototype.) Note the IsTypeAvailable test above: only do this at
                        // the initial type check that protects this path.
                        opnd->SetMonoGuardType(bucket->GetMonoGuardType());
                        this->currentInstr->ChangeEquivalentToMonoTypeCheckBailOut();
                    }
                    bucket->SetMonoGuardType(nullptr);
                }
                
                if (!opnd->IsTypeAvailable())
                {TRACE_IT(674);
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
    {TRACE_IT(675);
        opnd->EnsureGuardedPropOps(this->func->m_alloc);
        opnd->SetGuardedPropOp(opnd->GetObjTypeSpecFldId());
    }
}

void
BackwardPass::TrackObjTypeSpecWriteGuards(IR::PropertySymOpnd *opnd, BasicBlock *block)
{TRACE_IT(676);
    // TODO (ObjTypeSpec): Move write guard tracking to the forward pass, by recording on the type value
    // which property IDs have been written since the last type check. This will result in more accurate
    // tracking in cases when object pointer copy prop kicks in.
    if (this->tag == Js::BackwardPhase)
    {TRACE_IT(677);
        // If this operation may need a write guard (load from proto or fixed field check) then add its
        // write guard symbol to the map for this object. If it remains live (hasn't been written to)
        // until the type check upstream, it will get recorded there so that the type check can be registered
        // for invalidation on this property used in this operation.

        // (ObjTypeSpec): Consider supporting polymorphic write guards as well. We can't currently distinguish between mono and
        // poly write guards, and a type check can only protect operations matching with respect to polymorphism (see
        // BackwardPass::TrackObjTypeSpecProperties for details), so for now we only target monomorphic operations.
        if (opnd->IsMono() && opnd->MayNeedWriteGuardProtection())
        {TRACE_IT(678);
            if (block->stackSymToWriteGuardsMap == nullptr)
            {TRACE_IT(679);
                block->stackSymToWriteGuardsMap = HashTable<ObjWriteGuardBucket>::New(this->tempAlloc, 8);
            }

            ObjWriteGuardBucket* bucket = block->stackSymToWriteGuardsMap->FindOrInsertNew(opnd->GetObjectSym()->m_id);

            BVSparse<JitArenaAllocator>* writeGuards = bucket->GetWriteGuards();
            if (writeGuards == nullptr)
            {TRACE_IT(680);
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
        {TRACE_IT(681);
            Assert(opnd->GetWriteGuards() == nullptr);
            if (block->stackSymToWriteGuardsMap != nullptr)
            {TRACE_IT(682);
                ObjWriteGuardBucket* bucket = block->stackSymToWriteGuardsMap->Get(opnd->GetObjectSym()->m_id);
                if (bucket != nullptr)
                {TRACE_IT(683);
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
    {TRACE_IT(684);
        // If we know this property has never been written to in this function (either on this object or any
        // of its aliases) we don't need the local type check.
        if (opnd->MayNeedWriteGuardProtection() && !opnd->IsWriteGuardChecked() && !MayPropertyBeWrittenTo(opnd->GetPropertyId()))
        {TRACE_IT(685);
            opnd->SetWriteGuardChecked(true);
        }

        // If we don't need a primary type check here let's clear the write guards. The primary type check upstream will
        // register the type check for the corresponding properties.
        if (!IsPrePass() && !opnd->NeedsPrimaryTypeCheck())
        {TRACE_IT(686);
            opnd->ClearWriteGuards();
        }
    }
}

void
BackwardPass::TrackAddPropertyTypes(IR::PropertySymOpnd *opnd, BasicBlock *block)
{TRACE_IT(687);
    // Do the work of objtypespec add-property opt even if it's disabled by PHASE option, so that we have
    // the dataflow info that can be inspected.

    Assert(this->tag == Js::DeadStorePhase);
    Assert(opnd->IsMono() || opnd->HasEquivalentTypeSet());

    JITTypeHolder typeWithProperty = opnd->IsMono() ? opnd->GetType() : opnd->GetFirstEquivalentType();
    JITTypeHolder typeWithoutProperty = opnd->HasInitialType() ? opnd->GetInitialType() : JITTypeHolder(nullptr);

    if (typeWithoutProperty == nullptr ||
        typeWithProperty == typeWithoutProperty ||
        (opnd->IsTypeChecked() && !opnd->IsInitialTypeChecked()))
    {TRACE_IT(688);
        if (!this->IsPrePass() && block->stackSymToFinalType != nullptr && !this->currentInstr->HasBailOutInfo())
        {TRACE_IT(689);
            PropertySym *propertySym = opnd->m_sym->AsPropertySym();
            AddPropertyCacheBucket *pBucket =
                block->stackSymToFinalType->Get(propertySym->m_stackSym->m_id);
            if (pBucket && pBucket->GetFinalType() != nullptr && pBucket->GetInitialType() != pBucket->GetFinalType())
            {TRACE_IT(690);
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
    {TRACE_IT(691);
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
    {TRACE_IT(692);
#if DBG
        if (opnd->GetType() == pBucket->deadStoreUnavailableInitialType)
        {TRACE_IT(693);
            deadStoreUnavailableFinalType = pBucket->deadStoreUnavailableFinalType;
        }
#endif
        // No info found, or the info was bad, so initialize it from this cache.
        finalType = opnd->GetType();
        pBucket->SetFinalType(finalType);
    }
    else
    {TRACE_IT(694);
        // Match: The type we push upward is now the typeWithoutProperty at this point,
        // and the final type is the one we've been tracking.
        finalType = pBucket->GetFinalType();
#if DBG
        deadStoreUnavailableFinalType = pBucket->deadStoreUnavailableFinalType;
#endif
    }

    pBucket->SetInitialType(typeWithoutProperty);

    if (!PHASE_OFF(Js::ObjTypeSpecStorePhase, this->func))
    {TRACE_IT(695);
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
        {TRACE_IT(696);
            // This is the type that would have been propagated if we didn't kill it because the type isn't available
            JITTypeHolder checkFinalType = deadStoreUnavailableFinalType != nullptr ? deadStoreUnavailableFinalType : finalType;
            if (opnd->HasFinalType() && opnd->GetFinalType() != checkFinalType)
            {TRACE_IT(697);
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
        {TRACE_IT(698);
            opnd->SetFinalType(finalType);
        }
        if (!opnd->IsTypeChecked())
        {TRACE_IT(699);
            // Transition from initial to final type will only happen at type check points.
            if (opnd->IsTypeAvailable())
            {TRACE_IT(700);
                pBucket->SetFinalType(pBucket->GetInitialType());
            }
        }
    }

#if DBG_DUMP
    if (PHASE_TRACE(Js::ObjTypeSpecStorePhase, this->func))
    {TRACE_IT(701);
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
    {TRACE_IT(702);
#if DBG
        pBucket->deadStoreUnavailableInitialType = pBucket->GetInitialType();
        if (pBucket->deadStoreUnavailableFinalType == nullptr)
        {TRACE_IT(703);
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
{TRACE_IT(704);
    StackSym *objSym = this->func->m_symTable->FindStackSym(symId);
    Assert(objSym);
    this->InsertTypeTransition(instrInsertBefore, objSym, data);
}

void
BackwardPass::InsertTypeTransition(IR::Instr *instrInsertBefore, StackSym *objSym, AddPropertyCacheBucket *data)
{TRACE_IT(705);
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
{TRACE_IT(706);
    if (!this->IsPrePass())
    {TRACE_IT(707);
        // Transition to the final type if we don't bail out.
        if (instr->EndsBasicBlock())
        {TRACE_IT(708);
            // The instr with the bailout is something like a branch that may not fall through.
            // Insert the transitions instead at the beginning of each successor block.
            this->InsertTypeTransitionsAtPriorSuccessors(this->currentBlock, nullptr, symId, data);
        }
        else
        {TRACE_IT(709);
            this->InsertTypeTransition(instr->m_next, symId, data);
        }
    }
    // Note: we could probably clear this entry out of the table, but I don't know
    // whether it's worth it, because it's likely coming right back.
    data->SetFinalType(data->GetInitialType());
}

void
BackwardPass::InsertTypeTransitionAtBlock(BasicBlock *block, int symId, AddPropertyCacheBucket *data)
{TRACE_IT(710);
    bool inserted = false;
    FOREACH_INSTR_IN_BLOCK(instr, block)
    {TRACE_IT(711);
        if (instr->IsRealInstr())
        {TRACE_IT(712);
            // Check for pre-existing type transition. There may be more than one AdjustObjType here,
            // so look at them all.
            if (instr->m_opcode == Js::OpCode::AdjustObjType)
            {TRACE_IT(713);
                if (instr->GetSrc1()->AsRegOpnd()->m_sym->m_id == (SymID)symId)
                {TRACE_IT(714);
                    // This symbol already has a type transition at this point.
                    // It *must* be doing the same transition we're already trying to do.
                    Assert((intptr_t)instr->GetDst()->AsAddrOpnd()->m_address == data->GetFinalType()->GetAddr() &&
                           (intptr_t)instr->GetSrc2()->AsAddrOpnd()->m_address == data->GetInitialType()->GetAddr());
                    // Nothing to do.
                    return;
                }
            }
            else
            {TRACE_IT(715);
                this->InsertTypeTransition(instr, symId, data);
                inserted = true;
                break;
            }
        }
    }
    NEXT_INSTR_IN_BLOCK;
    if (!inserted)
    {TRACE_IT(716);
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
    {TRACE_IT(717);
        if (blockFix == blockSucc)
        {TRACE_IT(718);
            return;
        }

        this->InsertTypeTransitionAtBlock(blockFix, symId, data);
    }
    NEXT_SUCCESSOR_BLOCK;
}

void
BackwardPass::InsertTypeTransitionsAtPotentialKills()
{TRACE_IT(719);
    // Final types can't be pushed up past certain instructions.
    IR::Instr *instr = this->currentInstr;

    if (instr->HasBailOutInfo() || instr->m_opcode == Js::OpCode::UpdateNewScObjectCache)
    {TRACE_IT(720);
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
    {TRACE_IT(721);
        // If this is a load/store that expects an object-header-inlined type, don't push another sym's transition from
        // object-header-inlined to non-object-header-inlined type past it, because the two syms may be aliases.
        IR::PropertySymOpnd *propertySymOpnd = instr->GetPropertySymOpnd();
        if (propertySymOpnd && propertySymOpnd->IsObjectHeaderInlined())
        {TRACE_IT(722);
            SymID opndId = propertySymOpnd->m_sym->AsPropertySym()->m_stackSym->m_id;
            this->ForEachAddPropertyCacheBucket([&](int symId, AddPropertyCacheBucket *data)->bool {
                if ((SymID)symId == opndId)
                {TRACE_IT(723);
                    // This is the sym we're tracking. No aliasing to worry about.
                    return false;
                }
                if (propertySymOpnd->IsMono() && data->GetInitialType() != propertySymOpnd->GetType())
                {TRACE_IT(724);
                    // Type mismatch in a monomorphic case -- no aliasing.
                    return false;
                }
                if (this->TransitionUndoesObjectHeaderInlining(data))
                {TRACE_IT(725);
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
{TRACE_IT(726);
    BasicBlock *block = this->currentBlock;
    if (block->stackSymToFinalType == nullptr)
    {TRACE_IT(727);
        return;
    }

    FOREACH_HASHTABLE_ENTRY(AddPropertyCacheBucket, bucket, block->stackSymToFinalType)
    {TRACE_IT(728);
        AddPropertyCacheBucket *data = &bucket.element;
        if (data->GetInitialType() != nullptr &&
            data->GetInitialType() != data->GetFinalType())
        {TRACE_IT(729);
            bool done = fn(bucket.value, data);
            if (done)
            {TRACE_IT(730);
                break;
            }
        }
    }
    NEXT_HASHTABLE_ENTRY;
}

bool
BackwardPass::TransitionUndoesObjectHeaderInlining(AddPropertyCacheBucket *data) const
{TRACE_IT(731);
    JITTypeHolder type = data->GetInitialType();
    if (type == nullptr || !Js::DynamicType::Is(type->GetTypeId()))
    {TRACE_IT(732);
        return false;
    }

    if (!type->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler())
    {TRACE_IT(733);
        return false;
    }

    type = data->GetFinalType();
    if (type == nullptr || !Js::DynamicType::Is(type->GetTypeId()))
    {TRACE_IT(734);
        return false;
    }
    return !type->GetTypeHandler()->IsObjectHeaderInlinedTypeHandler();
}

void
BackwardPass::CollectCloneStrCandidate(IR::Opnd * opnd)
{TRACE_IT(735);
    IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
    Assert(regOpnd != nullptr);
    StackSym *sym = regOpnd->m_sym;

    if (tag == Js::BackwardPhase
        && currentInstr->m_opcode == Js::OpCode::Add_A
        && currentInstr->GetSrc1() == opnd
        && !this->IsPrePass()
        && !this->IsCollectionPass()
        &&  this->currentBlock->loop)
    {TRACE_IT(736);
        Assert(currentBlock->cloneStrCandidates != nullptr);

        currentBlock->cloneStrCandidates->Set(sym->m_id);
    }
}

void
BackwardPass::InvalidateCloneStrCandidate(IR::Opnd * opnd)
{TRACE_IT(737);
    IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
    Assert(regOpnd != nullptr);
    StackSym *sym = regOpnd->m_sym;

    if (tag == Js::BackwardPhase &&
        (currentInstr->m_opcode != Js::OpCode::Add_A || currentInstr->GetSrc1()->AsRegOpnd()->m_sym->m_id != sym->m_id) &&
        !this->IsPrePass() &&
        !this->IsCollectionPass() &&
        this->currentBlock->loop)
    {TRACE_IT(738);
            currentBlock->cloneStrCandidates->Clear(sym->m_id);
    }
}

void
BackwardPass::ProcessUse(IR::Opnd * opnd)
{TRACE_IT(739);
    switch (opnd->GetKind())
    {
    case IR::OpndKindReg:
        {TRACE_IT(740);
            IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
            StackSym *sym = regOpnd->m_sym;

            if (!IsCollectionPass())
            {TRACE_IT(741);
                // isTempLastUse is only used for string concat right now, so lets not mark it if it's not a string.
                // If it's upward exposed, it is not it's last use.
                if (regOpnd->m_isTempLastUse && (regOpnd->GetValueType().IsNotString() || this->currentBlock->upwardExposedUses->Test(sym->m_id) || sym->m_mayNotBeTempLastUse))
                {TRACE_IT(742);
                    regOpnd->m_isTempLastUse = false;
                }
                this->CollectCloneStrCandidate(opnd);
            }

            this->DoSetDead(regOpnd, !this->ProcessSymUse(sym, true, regOpnd->GetIsJITOptimizedReg()));

            if (IsCollectionPass())
            {TRACE_IT(743);
                break;
            }

            if (tag == Js::DeadStorePhase && regOpnd->IsArrayRegOpnd())
            {
                ProcessArrayRegOpndUse(currentInstr, regOpnd->AsArrayRegOpnd());
            }

            if (currentInstr->m_opcode == Js::OpCode::BailOnNotArray)
            {TRACE_IT(744);
                Assert(tag == Js::DeadStorePhase);

                const ValueType valueType(regOpnd->GetValueType());
                if(valueType.IsLikelyArrayOrObjectWithArray())
                {TRACE_IT(745);
                    currentBlock->noImplicitCallUses->Clear(sym->m_id);

                    // We are being conservative here to always check for missing value
                    // if any of them expect no missing value. That is because we don't know
                    // what set of sym is equivalent (copied) from the one we are testing for right now.
                    if(valueType.HasNoMissingValues() &&
                        !currentBlock->noImplicitCallNoMissingValuesUses->IsEmpty() &&
                        !IsPrePass())
                    {TRACE_IT(746);
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
        {TRACE_IT(747);
            IR::SymOpnd *symOpnd = opnd->AsSymOpnd();
            Sym * sym = symOpnd->m_sym;

            this->DoSetDead(symOpnd, !this->ProcessSymUse(sym, false, opnd->GetIsJITOptimizedReg()));

            if (IsCollectionPass())
            {TRACE_IT(748);
                break;
            }

            if (sym->IsPropertySym())
            {TRACE_IT(749);
                // TODO: We don't have last use info for property sym
                // and we don't set the last use of the stacksym inside the property sym
                if (tag == Js::BackwardPhase)
                {TRACE_IT(750);
                    if (opnd->AsSymOpnd()->IsPropertySymOpnd())
                    {TRACE_IT(751);
                        this->globOpt->PreparePropertySymOpndForTypeCheckSeq(symOpnd->AsPropertySymOpnd(), this->currentInstr, this->currentBlock->loop);
                    }
                }

                if (this->DoMarkTempNumbersOnTempObjects())
                {TRACE_IT(752);
                    this->currentBlock->tempNumberTracker->ProcessPropertySymUse(symOpnd, this->currentInstr, this);
                }

                if (symOpnd->IsPropertySymOpnd())
                {TRACE_IT(753);
                    this->ProcessPropertySymOpndUse(symOpnd->AsPropertySymOpnd());
                }
            }
        }
        break;
    case IR::OpndKindIndir:
        {TRACE_IT(754);
            IR::IndirOpnd * indirOpnd = opnd->AsIndirOpnd();
            IR::RegOpnd * baseOpnd = indirOpnd->GetBaseOpnd();

            this->DoSetDead(baseOpnd, !this->ProcessSymUse(baseOpnd->m_sym, false, baseOpnd->GetIsJITOptimizedReg()));

            IR::RegOpnd * indexOpnd = indirOpnd->GetIndexOpnd();
            if (indexOpnd)
            {TRACE_IT(755);
                this->DoSetDead(indexOpnd, !this->ProcessSymUse(indexOpnd->m_sym, false, indexOpnd->GetIsJITOptimizedReg()));
            }

            if(IsCollectionPass())
            {TRACE_IT(756);
                break;
            }

            if (this->DoMarkTempNumbersOnTempObjects())
            {TRACE_IT(757);
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
{TRACE_IT(758);
    Assert(this->tag == Js::BackwardPhase);

    BasicBlock *block = this->currentBlock;

    bool isLive = !!block->upwardExposedFields->TestAndSet(propertySym->m_id);

    if (propertySym->m_propertyEquivSet)
    {TRACE_IT(759);
        block->upwardExposedFields->Or(propertySym->m_propertyEquivSet);
    }

    return isLive;
}

void
BackwardPass::MarkTemp(StackSym * sym)
{TRACE_IT(760);
    Assert(!IsCollectionPass());
    // Don't care about type specialized syms
    if (!sym->IsVar())
    {TRACE_IT(761);
        return;
    }

    BasicBlock * block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {TRACE_IT(762);
        Assert((block->loop != nullptr) == block->tempNumberTracker->HasTempTransferDependencies());
        block->tempNumberTracker->MarkTemp(sym, this);
    }
    if (this->DoMarkTempObjects())
    {TRACE_IT(763);
        Assert((block->loop != nullptr) == block->tempObjectTracker->HasTempTransferDependencies());
        block->tempObjectTracker->MarkTemp(sym, this);
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {TRACE_IT(764);
        Assert((block->loop != nullptr) == block->tempObjectVerifyTracker->HasTempTransferDependencies());
        block->tempObjectVerifyTracker->MarkTemp(sym, this);
    }
#endif
}

void
BackwardPass::MarkTempProcessInstr(IR::Instr * instr)
{TRACE_IT(765);
    Assert(!IsCollectionPass());

    if (this->currentBlock->isDead)
    {TRACE_IT(766);
        return;
    }

    BasicBlock * block;
    block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {TRACE_IT(767);
        block->tempNumberTracker->ProcessInstr(instr, this);
    }

    if (this->DoMarkTempObjects())
    {TRACE_IT(768);
        block->tempObjectTracker->ProcessInstr(instr);
    }

#if DBG
    if (this->DoMarkTempObjectVerify())
    {TRACE_IT(769);
        block->tempObjectVerifyTracker->ProcessInstr(instr, this);
    }
#endif
}

#if DBG_DUMP
void
BackwardPass::DumpMarkTemp()
{TRACE_IT(770);
    Assert(!IsCollectionPass());

    BasicBlock * block = this->currentBlock;
    if (this->DoMarkTempNumbers())
    {TRACE_IT(771);
        block->tempNumberTracker->Dump();
    }
    if (this->DoMarkTempObjects())
    {TRACE_IT(772);
        block->tempObjectTracker->Dump();
    }
#if DBG
    if (this->DoMarkTempObjectVerify())
    {TRACE_IT(773);
        block->tempObjectVerifyTracker->Dump();
    }
#endif
}
#endif

void
BackwardPass::SetSymIsUsedOnlyInNumberIfLastUse(IR::Opnd *const opnd)
{TRACE_IT(774);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {TRACE_IT(775);
        symUsedOnlyForNumberBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsNotUsedOnlyInNumber(IR::Opnd *const opnd)
{TRACE_IT(776);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym)
    {TRACE_IT(777);
        symUsedOnlyForNumberBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsUsedOnlyInBitOpsIfLastUse(IR::Opnd *const opnd)
{TRACE_IT(778);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {TRACE_IT(779);
        symUsedOnlyForBitOpsBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetSymIsNotUsedOnlyInBitOps(IR::Opnd *const opnd)
{TRACE_IT(780);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym)
    {TRACE_IT(781);
        symUsedOnlyForBitOpsBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::TrackBitWiseOrNumberOp(IR::Instr *const instr)
{TRACE_IT(782);
    Assert(instr);
    const bool trackBitWiseop = DoTrackBitOpsOrNumber();
    const bool trackNumberop = trackBitWiseop;
    const Js::OpCode opcode = instr->m_opcode;
    StackSym *const dstSym = IR::RegOpnd::TryGetStackSym(instr->GetDst());
    if (!trackBitWiseop && !trackNumberop)
    {TRACE_IT(783);
        return;
    }

    if (!instr->IsRealInstr())
    {TRACE_IT(784);
        return;
    }

    if (dstSym)
    {TRACE_IT(785);
        // For a dst where the def is in this block, transfer the current info into the instruction
        if (trackBitWiseop && symUsedOnlyForBitOpsBySymId->TestAndClear(dstSym->m_id))
        {TRACE_IT(786);
            instr->dstIsAlwaysConvertedToInt32 = true;
        }
        if (trackNumberop && symUsedOnlyForNumberBySymId->TestAndClear(dstSym->m_id))
        {TRACE_IT(787);
            instr->dstIsAlwaysConvertedToNumber = true;
        }
    }

    // If the instruction can cause src values to escape the local scope, the srcs can't be optimized
    if (OpCodeAttr::NonTempNumberSources(opcode))
    {TRACE_IT(788);
        if (trackBitWiseop)
        {TRACE_IT(789);
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc1());
            SetSymIsNotUsedOnlyInBitOps(instr->GetSrc2());
        }
        if (trackNumberop)
        {TRACE_IT(790);
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc1());
            SetSymIsNotUsedOnlyInNumber(instr->GetSrc2());
        }
        return;
    }

    if (trackBitWiseop)
    {TRACE_IT(791);
        switch (opcode)
        {
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
    {TRACE_IT(792);
        switch (opcode)
        {
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
{TRACE_IT(793);
    Assert(instr->HasBailOutInfo() && (instr->GetBailOutKind() & IR::BailOutOnNegativeZero));
    IR::BailOutKind bailOutKind = instr->GetBailOutKind();
    bailOutKind = bailOutKind & ~IR::BailOutOnNegativeZero;
    if (bailOutKind)
    {TRACE_IT(794);
        instr->SetBailOutKind(bailOutKind);
    }
    else
    {TRACE_IT(795);
        instr->ClearBailOutInfo();
        if (preOpBailOutInstrToProcess == instr)
        {TRACE_IT(796);
            preOpBailOutInstrToProcess = nullptr;
        }
    }
}

void
BackwardPass::TrackIntUsage(IR::Instr *const instr)
{TRACE_IT(797);
    Assert(instr);

    const bool trackNegativeZero = DoTrackNegativeZero();
    const bool trackIntOverflow = DoTrackIntOverflow();
    const bool trackCompoundedIntOverflow = DoTrackCompoundedIntOverflow();
    const bool trackNon32BitOverflow = DoTrackNon32BitOverflow();

    if(!(trackNegativeZero || trackIntOverflow || trackCompoundedIntOverflow))
    {TRACE_IT(798);
        return;
    }

    const Js::OpCode opcode = instr->m_opcode;
    if(trackCompoundedIntOverflow && opcode == Js::OpCode::StatementBoundary && instr->AsPragmaInstr()->m_statementIndex == 0)
    {TRACE_IT(799);
        // Cannot bail out before the first statement boundary, so the range cannot extend beyond this instruction
        Assert(!instr->ignoreIntOverflowInRange);
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(!instr->IsRealInstr())
    {TRACE_IT(800);
        return;
    }

    StackSym *const dstSym = IR::RegOpnd::TryGetStackSym(instr->GetDst());
    bool ignoreIntOverflowCandidate = false;
    if(dstSym)
    {TRACE_IT(801);
        // For a dst where the def is in this block, transfer the current info into the instruction
        if(trackNegativeZero)
        {TRACE_IT(802);
            if (negativeZeroDoesNotMatterBySymId->Test(dstSym->m_id))
            {TRACE_IT(803);
                instr->ignoreNegativeZero = true;
            }

            if (tag == Js::DeadStorePhase)
            {TRACE_IT(804);
                if (negativeZeroDoesNotMatterBySymId->TestAndClear(dstSym->m_id))
                {TRACE_IT(805);
                    if (instr->HasBailOutInfo())
                    {TRACE_IT(806);
                        IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                        if (bailOutKind & IR::BailOutOnNegativeZero)
                        {TRACE_IT(807);
                            RemoveNegativeZeroBailout(instr);
                        }
                    }
                }
                else
                {TRACE_IT(808);
                    if (instr->HasBailOutInfo())
                    {TRACE_IT(809);
                        if (instr->GetBailOutKind() & IR::BailOutOnNegativeZero)
                        {TRACE_IT(810);
                            if (this->currentBlock->couldRemoveNegZeroBailoutForDef->TestAndClear(dstSym->m_id))
                            {TRACE_IT(811);
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
            {TRACE_IT(812);
                this->negativeZeroDoesNotMatterBySymId->Clear(dstSym->m_id);
            }
        }
        if(trackIntOverflow)
        {TRACE_IT(813);
            ignoreIntOverflowCandidate = !!intOverflowDoesNotMatterBySymId->TestAndClear(dstSym->m_id);
            if(trackCompoundedIntOverflow)
            {TRACE_IT(814);
                instr->ignoreIntOverflowInRange = !!intOverflowDoesNotMatterInRangeBySymId->TestAndClear(dstSym->m_id);
            }
        }
    }

    // If the instruction can cause src values to escape the local scope, the srcs can't be optimized
    if(OpCodeAttr::NonTempNumberSources(opcode))
    {TRACE_IT(815);
        if(trackNegativeZero)
        {TRACE_IT(816);
            SetNegativeZeroMatters(instr->GetSrc1());
            SetNegativeZeroMatters(instr->GetSrc2());
        }
        if(trackIntOverflow)
        {TRACE_IT(817);
            SetIntOverflowMatters(instr->GetSrc1());
            SetIntOverflowMatters(instr->GetSrc2());
            if(trackCompoundedIntOverflow)
            {TRACE_IT(818);
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
    {TRACE_IT(819);
        switch(opcode)
        {
            // Instructions that can cause src values to escape the local scope have already been excluded

            case Js::OpCode::FromVar:
            case Js::OpCode::Conv_Prim:
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(!instr->GetSrc2());

                if(instr->GetDst()->IsInt32())
                {TRACE_IT(820);
                    // Conversion to int32 that is either explicit, or has a bailout check ensuring that it's an int value
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    break;
                }
                // fall-through

            default:
                if(dstSym && !instr->ignoreNegativeZero)
                {TRACE_IT(821);
                    // -0 matters for dst, so -0 also matters for srcs
                    SetNegativeZeroMatters(instr->GetSrc1());
                    SetNegativeZeroMatters(instr->GetSrc2());
                    break;
                }
                if(opcode == Js::OpCode::Div_A || opcode == Js::OpCode::Div_I4)
                {TRACE_IT(822);
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
            {TRACE_IT(823);
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsIntConstOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsIntConstOpnd());

                if (instr->ignoreNegativeZero ||
                    (instr->GetSrc1()->IsIntConstOpnd() && instr->GetSrc1()->AsIntConstOpnd()->GetValue() != 0) ||
                    (instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() != 0))
                {TRACE_IT(824);
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
                {TRACE_IT(825);
                    if (instr->GetSrc2()->IsRegOpnd() &&
                        !currentBlock->upwardExposedUses->Test(instr->GetSrc2()->AsRegOpnd()->m_sym->m_id))
                    {TRACE_IT(826);
                        SetCouldRemoveNegZeroBailoutForDefIfLastUse(instr->GetSrc2());
                    }
                    else
                    {TRACE_IT(827);
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
                {TRACE_IT(828);
                    // -0 does not matter for dst, or this instruction does not generate -0 since one of the srcs is not -0
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                    break;
                }

                SetNegativeZeroMatters(instr->GetSrc1());
                SetNegativeZeroMatters(instr->GetSrc2());
                break;

            case Js::OpCode::Sub_I4:
            {TRACE_IT(829);
                Assert(dstSym);
                Assert(instr->GetSrc1());
                Assert(instr->GetSrc1()->IsRegOpnd() || instr->GetSrc1()->IsIntConstOpnd());
                Assert(instr->GetSrc2());
                Assert(instr->GetSrc2()->IsRegOpnd() || instr->GetSrc2()->IsIntConstOpnd());

                if (instr->ignoreNegativeZero ||
                    (instr->GetSrc1()->IsIntConstOpnd() && instr->GetSrc1()->AsIntConstOpnd()->GetValue() != 0) ||
                    (instr->GetSrc2()->IsIntConstOpnd() && instr->GetSrc2()->AsIntConstOpnd()->GetValue() != 0))
                {TRACE_IT(830);
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc1());
                    SetNegativeZeroDoesNotMatterIfLastUse(instr->GetSrc2());
                }
                else
                {TRACE_IT(831);
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
                {TRACE_IT(832);
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
                {TRACE_IT(833);
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
    {TRACE_IT(834);
        return;
    }

    switch(opcode)
    {
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
            {TRACE_IT(835);
                if (ignoreIntOverflowCandidate)
                    instr->ignoreOverflowBitCount = 53;
            }
            else
            {TRACE_IT(836);
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
            {TRACE_IT(837);
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
    {TRACE_IT(838);
        instr->ignoreIntOverflow = true;
    }

    // Compounded int overflow tracking

    if(!trackCompoundedIntOverflow)
    {TRACE_IT(839);
        return;
    }

    if(instr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset)
    {TRACE_IT(840);
        // The forward pass may need to insert conversions with bailouts before the first instruction in the range. Since this
        // instruction does not have a valid byte code offset for bailout purposes, end the current range now.
        instr->ignoreIntOverflowInRange = false;
        SetIntOverflowMattersInRange(instr->GetSrc1());
        SetIntOverflowMattersInRange(instr->GetSrc2());
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(ignoreIntOverflowCandidate)
    {TRACE_IT(841);
        instr->ignoreIntOverflowInRange = true;
        if(dstSym)
        {TRACE_IT(842);
            dstSym->scratch.globOpt.numCompoundedAddSubUses = 0;
        }
    }

    bool lossy = false;
    switch(opcode)
    {
        // Instructions that can cause src values to escape the local scope have already been excluded

        case Js::OpCode::Incr_A:
        case Js::OpCode::Decr_A:
        case Js::OpCode::Add_A:
        case Js::OpCode::Sub_A:
        {TRACE_IT(843);
            if(!instr->ignoreIntOverflowInRange)
            {TRACE_IT(844);
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
            {TRACE_IT(845);
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
        {TRACE_IT(846);
            if(!instr->ignoreIntOverflowInRange)
            {TRACE_IT(847);
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
        {TRACE_IT(848);
            Assert(dstSym);
            Assert(!instr->GetSrc2()); // at the moment, this list contains only unary operations

            if(intOverflowCurrentlyMattersInRange)
            {TRACE_IT(849);
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
            {TRACE_IT(850);
                candidateSymsRequiredToBeInt->Set(dstSym->m_id);
                if(currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Test(dstSym->m_id))
                {TRACE_IT(851);
                    candidateSymsRequiredToBeLossyInt->Set(dstSym->m_id);
                }
            }

            if(!instr->ignoreIntOverflowInRange)
            {TRACE_IT(852);
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
            {TRACE_IT(853);
                if(srcIncludedAsLossy && srcNeedsToBeLossless)
                {TRACE_IT(854);
                    candidateSymsRequiredToBeLossyInt->Compliment(srcSymId);
                }
            }
            else
            {TRACE_IT(855);
                candidateSymsRequiredToBeInt->Compliment(srcSymId);
                if(!srcNeedsToBeLossless)
                {TRACE_IT(856);
                    candidateSymsRequiredToBeLossyInt->Compliment(srcSymId);
                }
            }

            // These instructions will not end a range, so just return. They may be included in the middle of a range, and the
            // src has been included as a candidate input into the range.
            return;
        }

        case Js::OpCode::Mul_A:
            if (trackNon32BitOverflow)
            {TRACE_IT(857);
                // MULs will always be at the start of a range. Either included in the range if int32 overflow is ignored, or excluded if int32 overflow matters. Even if int32 can be ignored, MULs can still bailout on 53-bit.
                // That's why it cannot be in the middle of a range.
                if (instr->ignoreIntOverflowInRange)
                {TRACE_IT(858);
                    AnalysisAssert(dstSym);
                    Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses >= 0);
                    Assert(dstSym->scratch.globOpt.numCompoundedAddSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);
                    instr->ignoreOverflowBitCount = (uint8) (53 - dstSym->scratch.globOpt.numCompoundedAddSubUses);

                    // We have the max number of compounded adds/subs. 32-bit overflow cannot be ignored.
                    if (instr->ignoreOverflowBitCount == 32)
                    {TRACE_IT(859);
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
    {TRACE_IT(860);
        EndIntOverflowDoesNotMatterRange();
        return;
    }

    if(intOverflowCurrentlyMattersInRange)
    {TRACE_IT(861);
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
    {TRACE_IT(862);
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
    {TRACE_IT(863);
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Clear(dstSym->m_id);
        currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Clear(dstSym->m_id);
    }
    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < sizeof(srcs) / sizeof(srcs[0]) && srcs[i]; ++i)
    {TRACE_IT(864);
        StackSym *srcSym = IR::RegOpnd::TryGetStackSym(srcs[i]);
        if(!srcSym)
        {TRACE_IT(865);
            continue;
        }

        if(currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->TestAndSet(srcSym->m_id))
        {TRACE_IT(866);
            if(!lossy)
            {TRACE_IT(867);
                currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Clear(srcSym->m_id);
            }
        }
        else if(lossy)
        {TRACE_IT(868);
            currentBlock->intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Set(srcSym->m_id);
        }
    }

    // If the last instruction included in the range is a MUL, we have to end the range.
    // MULs with ignoreIntOverflow can still bailout on 53-bit overflow, so they cannot be in the middle of a range
    if (trackNon32BitOverflow && instr->m_opcode == Js::OpCode::Mul_A)
    {TRACE_IT(869);
        // range would have ended already if int32 overflow matters
        Assert(instr->ignoreIntOverflowInRange && instr->ignoreOverflowBitCount != 32);
        EndIntOverflowDoesNotMatterRange();
    }
}

void
BackwardPass::SetNegativeZeroDoesNotMatterIfLastUse(IR::Opnd *const opnd)
{TRACE_IT(870);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {TRACE_IT(871);
        negativeZeroDoesNotMatterBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetNegativeZeroMatters(IR::Opnd *const opnd)
{TRACE_IT(872);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {TRACE_IT(873);
        negativeZeroDoesNotMatterBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::SetCouldRemoveNegZeroBailoutForDefIfLastUse(IR::Opnd *const opnd)
{TRACE_IT(874);
    StackSym * stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if (stackSym && !this->currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {TRACE_IT(875);
        this->currentBlock->couldRemoveNegZeroBailoutForDef->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetIntOverflowDoesNotMatterIfLastUse(IR::Opnd *const opnd)
{TRACE_IT(876);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym && !currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {TRACE_IT(877);
        intOverflowDoesNotMatterBySymId->Set(stackSym->m_id);
    }
}

void
BackwardPass::SetIntOverflowMatters(IR::Opnd *const opnd)
{TRACE_IT(878);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {TRACE_IT(879);
        intOverflowDoesNotMatterBySymId->Clear(stackSym->m_id);
    }
}

bool
BackwardPass::SetIntOverflowDoesNotMatterInRangeIfLastUse(IR::Opnd *const opnd, const int addSubUses)
{TRACE_IT(880);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    return stackSym && SetIntOverflowDoesNotMatterInRangeIfLastUse(stackSym, addSubUses);
}

bool
BackwardPass::SetIntOverflowDoesNotMatterInRangeIfLastUse(StackSym *const stackSym, const int addSubUses)
{TRACE_IT(881);
    Assert(stackSym);
    Assert(addSubUses >= 0);
    Assert(addSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

    if(currentBlock->upwardExposedUses->Test(stackSym->m_id))
    {TRACE_IT(882);
        return false;
    }

    intOverflowDoesNotMatterInRangeBySymId->Set(stackSym->m_id);
    stackSym->scratch.globOpt.numCompoundedAddSubUses = addSubUses;
    return true;
}

void
BackwardPass::SetIntOverflowMattersInRange(IR::Opnd *const opnd)
{TRACE_IT(883);
    StackSym *const stackSym = IR::RegOpnd::TryGetStackSym(opnd);
    if(stackSym)
    {TRACE_IT(884);
        intOverflowDoesNotMatterInRangeBySymId->Clear(stackSym->m_id);
    }
}

void
BackwardPass::TransferCompoundedAddSubUsesToSrcs(IR::Instr *const instr, const int addSubUses)
{TRACE_IT(885);
    Assert(instr);
    Assert(addSubUses >= 0);
    Assert(addSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

    IR::Opnd *const srcs[] = { instr->GetSrc1(), instr->GetSrc2() };
    for(int i = 0; i < _countof(srcs) && srcs[i]; ++i)
    {TRACE_IT(886);
        StackSym *const srcSym = IR::RegOpnd::TryGetStackSym(srcs[i]);
        if(!srcSym)
        {TRACE_IT(887);
            // Int overflow tracking is only done for StackSyms in RegOpnds. Int overflow matters for the src, so it is
            // guaranteed to be in the int range at this point if the instruction is int-specialized.
            continue;
        }

        Assert(srcSym->scratch.globOpt.numCompoundedAddSubUses >= 0);
        Assert(srcSym->scratch.globOpt.numCompoundedAddSubUses <= MaxCompoundedUsesInAddSubForIgnoringIntOverflow);

        if(SetIntOverflowDoesNotMatterInRangeIfLastUse(srcSym, addSubUses))
        {TRACE_IT(888);
            // This is the last use of the src
            continue;
        }

        if(intOverflowDoesNotMatterInRangeBySymId->Test(srcSym->m_id))
        {TRACE_IT(889);
            // Since a src may be compounded through different chains of add/sub instructions, the greater number must be
            // preserved
            srcSym->scratch.globOpt.numCompoundedAddSubUses =
                max(srcSym->scratch.globOpt.numCompoundedAddSubUses, addSubUses);
        }
        else
        {TRACE_IT(890);
            // Int overflow matters for the src, so it is guaranteed to be in the int range at this point if the instruction is
            // int-specialized
        }
    }
}

void
BackwardPass::EndIntOverflowDoesNotMatterRange()
{TRACE_IT(891);
    if(intOverflowCurrentlyMattersInRange)
    {TRACE_IT(892);
        return;
    }
    intOverflowCurrentlyMattersInRange = true;

    if(currentBlock->intOverflowDoesNotMatterRange->FirstInstr()->m_next ==
        currentBlock->intOverflowDoesNotMatterRange->LastInstr())
    {TRACE_IT(893);
        // Don't need a range for a single-instruction range
        IntOverflowDoesNotMatterRange *const rangeToDelete = currentBlock->intOverflowDoesNotMatterRange;
        currentBlock->intOverflowDoesNotMatterRange = currentBlock->intOverflowDoesNotMatterRange->Next();
        currentBlock->RemoveInstr(rangeToDelete->LastInstr());
        rangeToDelete->Delete(globOpt->alloc);
    }
    else
    {TRACE_IT(894);
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
        {TRACE_IT(895);
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
{TRACE_IT(896);
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
    {TRACE_IT(897);
        return;
    }

    if(!instr->GetDst() || !instr->GetDst()->IsRegOpnd())
    {TRACE_IT(898);
        return;
    }
    const auto dst = instr->GetDst()->AsRegOpnd()->m_sym;
    if(!dst->IsFloat64())
    {TRACE_IT(899);
        return;
    }

    if(!instr->GetSrc1() || !instr->GetSrc1()->IsRegOpnd())
    {TRACE_IT(900);
        return;
    }
    const auto src = instr->GetSrc1()->AsRegOpnd()->m_sym;

    if(OpCodeAttr::NonIntTransfer(instr->m_opcode) && (!currentBlock->loop || IsPrePass()))
    {TRACE_IT(901);
        Assert(src->IsFloat64()); // dst is specialized, and since this is a float transfer, src must be specialized too

        if(dst == src)
        {TRACE_IT(902);
            return;
        }

        if(!func->m_fg->hasLoop)
        {TRACE_IT(903);
            // Special case for functions with no loops, since there can only be in-order dependencies. Just merge the two
            // non-number bailout bits and put the result in the source.
            if(dst->m_requiresBailOnNotNumber)
            {TRACE_IT(904);
                src->m_requiresBailOnNotNumber = true;
            }
            return;
        }

        FloatSymEquivalenceClass *dstEquivalenceClass, *srcEquivalenceClass;
        const bool dstHasEquivalenceClass = floatSymEquivalenceMap->TryGetValue(dst->m_id, &dstEquivalenceClass);
        const bool srcHasEquivalenceClass = floatSymEquivalenceMap->TryGetValue(src->m_id, &srcEquivalenceClass);

        if(!dstHasEquivalenceClass)
        {TRACE_IT(905);
            if(srcHasEquivalenceClass)
            {TRACE_IT(906);
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
        {TRACE_IT(907);
            // Just add the source into the destination's equivalence class
            dstEquivalenceClass->Set(src);
            floatSymEquivalenceMap->Add(src->m_id, dstEquivalenceClass);
            return;
        }

        if(dstEquivalenceClass == srcEquivalenceClass)
        {TRACE_IT(908);
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
        {TRACE_IT(909);
            floatSymEquivalenceMap->Item(id, dstEquivalenceClass);
        } NEXT_BITSET_IN_SPARSEBV;
        JitAdelete(tempAlloc, srcEquivalenceClass);

        return;
    }

    // Not a float transfer, and non-prepass (not necessarily in a loop)

    if(!instr->HasBailOutInfo() || instr->GetBailOutKind() != IR::BailOutPrimitiveButString)
    {TRACE_IT(910);
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
    {TRACE_IT(911);
        instr->SetBailOutKind(IR::BailOutNumberOnly);
    }
}

bool
BackwardPass::ProcessDef(IR::Opnd * opnd)
{TRACE_IT(912);
    BOOLEAN isJITOptimizedReg = false;
    Sym * sym;
    if (opnd->IsRegOpnd())
    {TRACE_IT(913);
        sym = opnd->AsRegOpnd()->m_sym;
        isJITOptimizedReg = opnd->GetIsJITOptimizedReg();
        if (!IsCollectionPass())
        {TRACE_IT(914);
            this->InvalidateCloneStrCandidate(opnd);
        }
    }
    else if (opnd->IsSymOpnd())
    {TRACE_IT(915);
        sym = opnd->AsSymOpnd()->m_sym;
        isJITOptimizedReg = opnd->GetIsJITOptimizedReg();
    }
    else
    {TRACE_IT(916);
        if (opnd->IsIndirOpnd())
        {TRACE_IT(917);
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
    {TRACE_IT(918);
        if(IsCollectionPass())
        {TRACE_IT(919);
            return false;
        }

        Assert((block->fieldHoistCandidates != nullptr) == this->DoFieldHoistCandidates());
        if (block->fieldHoistCandidates)
        {TRACE_IT(920);
            block->fieldHoistCandidates->Clear(sym->m_id);
        }
        PropertySym *propertySym = sym->AsPropertySym();
        if (this->DoDeadStoreSlots())
        {TRACE_IT(921);
            if (propertySym->m_fieldKind == PropertyKindLocalSlots || propertySym->m_fieldKind == PropertyKindSlots)
            {TRACE_IT(922);
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
        {TRACE_IT(923);
            if (opnd->AsSymOpnd()->IsPropertySymOpnd())
            {TRACE_IT(924);
                this->globOpt->PreparePropertySymOpndForTypeCheckSeq(opnd->AsPropertySymOpnd(), instr, this->currentBlock->loop);
            }
        }
        if (opnd->AsSymOpnd()->IsPropertySymOpnd())
        {TRACE_IT(925);
            this->ProcessPropertySymOpndUse(opnd->AsPropertySymOpnd());
        }
    }
    else
    {TRACE_IT(926);
        Assert(!instr->IsByteCodeUsesInstr());

        if (this->DoByteCodeUpwardExposedUsed())
        {TRACE_IT(927);
            if (sym->AsStackSym()->HasByteCodeRegSlot())
            {TRACE_IT(928);
                StackSym * varSym = sym->AsStackSym();
                if (varSym->IsTypeSpec())
                {TRACE_IT(929);
                    // It has to have a var version for byte code regs
                    varSym = varSym->GetVarEquivSym(nullptr);
                }

                if (this->currentRegion)
                {TRACE_IT(930);
                    keepSymLiveForException = this->CheckWriteThroughSymInRegion(this->currentRegion, sym->AsStackSym());
                    keepVarSymLiveForException = this->CheckWriteThroughSymInRegion(this->currentRegion, varSym);
                }

                if (!isJITOptimizedReg)
                {TRACE_IT(931);
                    if (!DoDeadStore(this->func, sym->AsStackSym()))
                    {TRACE_IT(932);
                        // Don't deadstore the bytecodereg sym, so that we could do write to get the locals inspection
                        if (opnd->IsRegOpnd())
                        {TRACE_IT(933);
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
                    {TRACE_IT(934);
                        // Always track the sym use on the var sym.
                        block->byteCodeUpwardExposedUsed->Clear(varSym->m_id);
#if DBG
                        // TODO: We can only track first level function stack syms right now
                        if (varSym->GetByteCodeFunc() == this->func)
                        {TRACE_IT(935);
                            block->byteCodeRestoreSyms[varSym->GetByteCodeRegSlot()] = nullptr;
                        }
#endif
                    }
                }
            }
        }

        if(IsCollectionPass())
        {TRACE_IT(936);
            return false;
        }

        // Don't care about property sym for mark temps
        if (opnd->IsRegOpnd())
        {TRACE_IT(937);
            this->MarkTemp(sym->AsStackSym());
        }

        if (this->tag == Js::BackwardPhase &&
            instr->m_opcode == Js::OpCode::Ld_A &&
            instr->GetSrc1()->IsRegOpnd() &&
            block->upwardExposedFields->Test(sym->m_id))
        {TRACE_IT(938);
            block->upwardExposedFields->Set(instr->GetSrc1()->AsRegOpnd()->m_sym->m_id);
        }

        if (!keepSymLiveForException)
        {TRACE_IT(939);
            isUsed = block->upwardExposedUses->TestAndClear(sym->m_id);
        }
    }

    if (isUsed || !this->DoDeadStore())
    {TRACE_IT(940);
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
    {TRACE_IT(941);
        return false;
    }

    if (opnd->IsRegOpnd() && opnd->AsRegOpnd()->m_dontDeadStore)
    {TRACE_IT(942);
        return false;
    }

    if (instr->HasBailOutInfo())
    {TRACE_IT(943);
        // A bailout inserted for aggressive or lossy int type specialization causes assumptions to be made on the value of
        // the instruction's destination later on, as though the bailout did not happen. If the value is an int constant and
        // that value is propagated forward, it can cause the bailout instruction to become a dead store and be removed,
        // thereby invalidating the assumptions made. Or for lossy int type specialization, the lossy conversion to int32
        // may have side effects and so cannot be dead-store-removed. As one way of solving that problem, bailout
        // instructions resulting from aggressive or lossy int type spec are not dead-stored.
        const auto bailOutKind = instr->GetBailOutKind();
        if(bailOutKind & IR::BailOutOnResultConditions)
        {TRACE_IT(944);
            return false;
        }
        switch(bailOutKind & ~IR::BailOutKindBits)
        {
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
{TRACE_IT(945);
    BasicBlock * block = this->currentBlock;

#if DBG_DUMP
    if (this->IsTraceEnabled())
    {TRACE_IT(946);
        Output::Print(_u("Deadstore instr: "));
        instr->Dump();
    }
    this->numDeadStore++;
#endif

    // Before we remove the dead store, we need to track the byte code uses
    if (this->DoByteCodeUpwardExposedUsed())
    {TRACE_IT(947);
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
        {TRACE_IT(948);
            StackSym * stackSym = this->func->m_symTable->FindStackSym(symId);
            Assert(stackSym->GetType() == TyVar);
            // TODO: We can only track first level function stack syms right now
            if (stackSym->GetByteCodeFunc() == this->func)
            {TRACE_IT(949);
                Js::RegSlot byteCodeRegSlot = stackSym->GetByteCodeRegSlot();
                Assert(byteCodeRegSlot != Js::Constants::NoRegister);
                if (this->currentBlock->byteCodeRestoreSyms[byteCodeRegSlot] != stackSym)
                {TRACE_IT(950);
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
    {TRACE_IT(951);
        this->currentBlock->tempObjectVerifyTracker->NotifyDeadStore(instr, this);
    }
#endif

    
    if (instr->m_opcode == Js::OpCode::ArgIn_A)
    {TRACE_IT(952);
        //Ignore tracking ArgIn for "this", as argInsCount only tracks other params - unless it is a asmjs function(which doesn't have a "this").
        if (instr->GetSrc1()->AsSymOpnd()->m_sym->AsStackSym()->GetParamSlotNum() != 1 || func->GetJITFunctionBody()->IsAsmJsMode())
        {TRACE_IT(953);
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
{TRACE_IT(954);
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
    {TRACE_IT(955);
        StackSym * dstStackSym = instr->GetDst()->GetStackSym();
        PropertySym * dstPropertySym = dstStackSym->GetObjectInfo()->m_propertySymList;
        BVSparse<JitArenaAllocator> transferFields(this->tempAlloc);
        while (dstPropertySym != nullptr)
        {TRACE_IT(956);
            Assert(dstPropertySym->m_stackSym == dstStackSym);
            transferFields.Set(dstPropertySym->m_id);
            dstPropertySym = dstPropertySym->m_nextInStackSymList;
        }

        StackSym * srcStackSym = instr->GetSrc1()->GetStackSym();
        PropertySym * srcPropertySym = srcStackSym->GetObjectInfo()->m_propertySymList;
        BVSparse<JitArenaAllocator> equivFields(this->tempAlloc);

        while (srcPropertySym != nullptr && !transferFields.IsEmpty())
        {TRACE_IT(957);
            Assert(srcPropertySym->m_stackSym == srcStackSym);
            if (srcPropertySym->m_propertyEquivSet)
            {TRACE_IT(958);
                equivFields.And(&transferFields, srcPropertySym->m_propertyEquivSet);
                if (!equivFields.IsEmpty())
                {TRACE_IT(959);
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
{TRACE_IT(960);
    if (this->currentBlock->upwardExposedFields)
    {TRACE_IT(961);
        this->globOpt->ProcessFieldKills(instr, this->currentBlock->upwardExposedFields, false);
    }

    this->ClearBucketsOnFieldKill(instr, currentBlock->stackSymToFinalType);
    this->ClearBucketsOnFieldKill(instr, currentBlock->stackSymToGuardedProperties);
}

template<typename T>
void
BackwardPass::ClearBucketsOnFieldKill(IR::Instr *instr, HashTable<T> *table)
{TRACE_IT(962);
    if (table)
    {TRACE_IT(963);
        if (instr->UsesAllFields())
        {TRACE_IT(964);
            table->ClearAll();
        }
        else
        {TRACE_IT(965);
            IR::Opnd *dst = instr->GetDst();
            if (dst && dst->IsRegOpnd())
            {TRACE_IT(966);
                table->Clear(dst->AsRegOpnd()->m_sym->m_id);
            }
        }
    }
}

void
BackwardPass::ProcessFieldHoistKills(IR::Instr * instr)
{TRACE_IT(967);
    // The backward pass, we optimistically will not kill on a[] access
    // So that the field hoist candidate will be more then what can be hoisted
    // The root prepass will figure out the exact set of field that is hoisted
    this->globOpt->ProcessFieldKills(instr, this->currentBlock->fieldHoistCandidates, false);

    switch (instr->m_opcode)
    {
    case Js::OpCode::BrOnHasProperty:
    case Js::OpCode::BrOnNoProperty:
        // Should not hoist pass these instructions
        this->currentBlock->fieldHoistCandidates->Clear(instr->GetSrc1()->AsSymOpnd()->m_sym->m_id);
        break;
    }
}

bool
BackwardPass::TrackNoImplicitCallInlinees(IR::Instr *instr)
{TRACE_IT(968);
    if (this->tag != Js::DeadStorePhase || this->IsPrePass())
    {TRACE_IT(969);
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
    {TRACE_IT(970);
        // This func has instrs with bailouts or implicit calls
        Assert(instr->m_opcode != Js::OpCode::InlineeStart);
        instr->m_func->SetHasImplicitCallsOnSelfAndParents();
        return false;
    }

    if (instr->m_opcode == Js::OpCode::InlineeStart)
    {TRACE_IT(971);
        if (!instr->GetSrc1())
        {TRACE_IT(972);
            Assert(instr->m_func->m_hasInlineArgsOpt);
            return false;
        }
        return this->ProcessInlineeStart(instr);
    }

    return false;
}

bool
BackwardPass::ProcessInlineeStart(IR::Instr* inlineeStart)
{TRACE_IT(973);
    inlineeStart->m_func->SetFirstArgOffset(inlineeStart);

    IR::Instr* startCallInstr = nullptr;
    bool noImplicitCallsInInlinee = false;
    // Inlinee has no bailouts or implicit calls.  Get rid of the inline overhead.
    auto removeInstr = [&](IR::Instr* argInstr)
    {TRACE_IT(974);
        Assert(argInstr->m_opcode == Js::OpCode::InlineeStart || argInstr->m_opcode == Js::OpCode::ArgOut_A || argInstr->m_opcode == Js::OpCode::ArgOut_A_Inline);
        IR::Opnd *opnd = argInstr->GetSrc1();
        StackSym *sym = opnd->GetStackSym();
        if (!opnd->GetIsJITOptimizedReg() && sym && sym->HasByteCodeRegSlot())
        {TRACE_IT(975);
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
    {TRACE_IT(976);
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
    {TRACE_IT(977);
        PHASE_PRINT_TESTTRACE(Js::InlineArgsOptPhase, func, _u("%s[%d]: Skipping inline args optimization: %s[%d] HasCalls: %s 'arguments' access: %s Can do inlinee args opt: %s\n"),
                func->GetJITFunctionBody()->GetDisplayName(), func->GetJITFunctionBody()->GetFunctionNumber(),
                inlineeStart->m_func->GetJITFunctionBody()->GetDisplayName(), inlineeStart->m_func->GetJITFunctionBody()->GetFunctionNumber(),
                IsTrueOrFalse(inlineeStart->m_func->GetHasCalls()),
                IsTrueOrFalse(inlineeStart->m_func->GetHasUnoptimizedArgumentsAcccess()),
                IsTrueOrFalse(inlineeStart->m_func->m_canDoInlineArgsOpt));
        return false;
    }

    if (!inlineeStart->m_func->frameInfo->isRecorded)
    {TRACE_IT(978);
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
        {TRACE_IT(979);
            Assert(!inlineeStart->m_func->GetHasUnoptimizedArgumentsAcccess());
            // Do not remove arguments object meta arg if there is a reference to arguments object
        }
        else
        {TRACE_IT(980);
            FlowGraph::SafeRemoveInstr(metaArg);
        }
        i++;
        return false;
    });

    IR::Opnd *src1 = inlineeStart->GetSrc1();

    StackSym *sym = src1->GetStackSym();
    if (!src1->GetIsJITOptimizedReg() && sym && sym->HasByteCodeRegSlot())
    {TRACE_IT(981);
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
{TRACE_IT(982);
    if (this->IsPrePass())
    {TRACE_IT(983);
        return;
    }
    if (this->tag == Js::BackwardPhase)
    {TRACE_IT(984);
        if (!GlobOpt::DoInlineArgsOpt(instr->m_func))
        {TRACE_IT(985);
            return;
        }

        // This adds a use for function sym as part of InlineeStart & all the syms referenced by the args.
        // It ensure they do not get cleared from the copy prop sym map.
        instr->IterateArgInstrs([=](IR::Instr* argInstr){
            if (argInstr->GetSrc1()->IsRegOpnd())
            {TRACE_IT(986);
                this->currentBlock->upwardExposedUses->Set(argInstr->GetSrc1()->AsRegOpnd()->m_sym->m_id);
            }
            return false;
        });
    }
    else if (this->tag == Js::DeadStorePhase)
    {TRACE_IT(987);
        if (instr->m_func->m_hasInlineArgsOpt)
        {TRACE_IT(988);
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
{TRACE_IT(989);
    Assert(this->tag == Js::BackwardPhase);
    Assert(instr->m_opcode == Js::OpCode::BailOnNoProfile);
    Assert(!instr->HasBailOutInfo());
    AnalysisAssert(block);

    if (this->IsPrePass())
    {TRACE_IT(990);
        return false;
    }

    IR::Instr *curInstr = instr->m_prev;

    if (curInstr->IsLabelInstr() && curInstr->AsLabelInstr()->isOpHelper)
    {TRACE_IT(991);
        // Already processed

        if (this->DoMarkTempObjects())
        {TRACE_IT(992);
            block->tempObjectTracker->ProcessBailOnNoProfile(instr);
        }
        return false;
    }

    // Don't hoist if we see calls with profile data (recursive calls)
    while(!curInstr->StartsBasicBlock())
    {TRACE_IT(993);
        // If a function was inlined, it must have had profile info.
        if (curInstr->m_opcode == Js::OpCode::InlineeEnd || curInstr->m_opcode == Js::OpCode::InlineBuiltInEnd || curInstr->m_opcode == Js::OpCode::InlineNonTrackingBuiltInEnd
            || curInstr->m_opcode == Js::OpCode::InlineeStart || curInstr->m_opcode == Js::OpCode::EndCallForPolymorphicInlinee)
        {TRACE_IT(994);
            break;
        }
        else if (OpCodeAttr::CallInstr(curInstr->m_opcode))
        {TRACE_IT(995);
            if (curInstr->m_prev->m_opcode != Js::OpCode::BailOnNoProfile)
            {TRACE_IT(996);
                break;
            }
        }
        curInstr = curInstr->m_prev;
    }

    // Didn't get to the top of the block, delete this BailOnNoProfile.
    if (!curInstr->IsLabelInstr())
    {TRACE_IT(997);
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
    {TRACE_IT(998);
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
    {TRACE_IT(999);
        // Delete redundant BailOnNoProfile
        if (curInstr->m_opcode == Js::OpCode::BailOnNoProfile)
        {TRACE_IT(1000);
            Assert(!curInstr->HasBailOutInfo());
            curInstr = curInstr->m_next;
            curInstr->m_prev->Remove();
        }
        curInstr = curInstr->m_prev;
    }

    if (instr == block->GetLastInstr())
    {TRACE_IT(1001);
        block->SetLastInstr(instr->m_prev);
    }

    instr->Unlink();

    // Now try to move this up the flowgraph to the predecessor blocks
    FOREACH_PREDECESSOR_BLOCK(pred, block)
    {TRACE_IT(1002);
        bool hoistBailToPred = true;

        if (block->isLoopHeader && pred->loop == block->loop)
        {TRACE_IT(1003);
            // Skip loop back-edges
            continue;
        }

        // If all successors of this predecessor start with a BailOnNoProfile, we should be
        // okay to hoist this bail to the predecessor.
        FOREACH_SUCCESSOR_BLOCK(predSucc, pred)
        {TRACE_IT(1004);
            if (predSucc == block)
            {TRACE_IT(1005);
                continue;
            }
            if (!predSucc->beginsBailOnNoProfile)
            {TRACE_IT(1006);
                hoistBailToPred = false;
                break;
            }
        } NEXT_SUCCESSOR_BLOCK;

        if (hoistBailToPred)
        {TRACE_IT(1007);
            IR::Instr *predInstr = pred->GetLastInstr();
            IR::Instr *instrCopy = instr->Copy();

            if (predInstr->EndsBasicBlock())
            {TRACE_IT(1008);
                if (predInstr->m_prev->m_opcode == Js::OpCode::BailOnNoProfile)
                {TRACE_IT(1009);
                    // We already have one, we don't need a second.
                    instrCopy->Free();
                }
                else if (!predInstr->AsBranchInstr()->m_isSwitchBr)
                {TRACE_IT(1010);
                    // Don't put a bailout in the middle of a switch dispatch sequence.
                    // The bytecode offsets are not in order, and it would lead to incorrect
                    // bailout info.
                    instrCopy->m_func = predInstr->m_func;
                    predInstr->InsertBefore(instrCopy);
                }
            }
            else
            {TRACE_IT(1011);
                if (predInstr->m_opcode == Js::OpCode::BailOnNoProfile)
                {TRACE_IT(1012);
                    // We already have one, we don't need a second.
                    instrCopy->Free();
                }
                else
                {TRACE_IT(1013);
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
    {TRACE_IT(1014);
        blockHeadInstr->isOpHelper = true;
#if DBG
        blockHeadInstr->m_noHelperAssert = true;
#endif
        block->beginsBailOnNoProfile = true;

        instr->m_func = curInstr->m_func;
        curInstr->InsertAfter(instr);

        bool setLastInstr = (curInstr == block->GetLastInstr());
        if (setLastInstr)
        {TRACE_IT(1015);
            block->SetLastInstr(instr);
        }

        if (this->DoMarkTempObjects())
        {TRACE_IT(1016);
            block->tempObjectTracker->ProcessBailOnNoProfile(instr);
        }
        return false;
    }
    else
    {TRACE_IT(1017);
        instr->Free();
        return true;
    }
}

bool
BackwardPass::ReverseCopyProp(IR::Instr *instr)
{TRACE_IT(1018);
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
    {TRACE_IT(1019);
        return false;
    }
    if (this->tag != Js::DeadStorePhase || this->IsPrePass() || this->IsCollectionPass())
    {TRACE_IT(1020);
        return false;
    }
    if (this->func->HasTry())
    {TRACE_IT(1021);
        // UpwardExposedUsed info can't be relied on
        return false;
    }

    // Find t2 = Ld_A t1
    switch (instr->m_opcode)
    {
    case Js::OpCode::Ld_A:
    case Js::OpCode::Ld_I4:
        break;

    default:
        return false;
    }

    if (!instr->GetDst()->IsRegOpnd())
    {TRACE_IT(1022);
        return false;
    }
    if (!instr->GetSrc1()->IsRegOpnd())
    {TRACE_IT(1023);
        return false;
    }
    if (instr->HasBailOutInfo())
    {TRACE_IT(1024);
        return false;
    }

    IR::RegOpnd *dst = instr->GetDst()->AsRegOpnd();
    IR::RegOpnd *src = instr->GetSrc1()->AsRegOpnd();
    IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();

    IR::ByteCodeUsesInstr *byteCodeUseInstr = nullptr;
    StackSym *varSym = src->m_sym;

    if (varSym->IsTypeSpec())
    {TRACE_IT(1025);
        varSym = varSym->GetVarEquivSym(this->func);
    }

    // SKip ByteCodeUse instr if possible
    //       [bytecodeuse t1]
    if (!instrPrev->GetDst())
    {TRACE_IT(1026);
        if (instrPrev->m_opcode == Js::OpCode::ByteCodeUses)
        {TRACE_IT(1027);
            byteCodeUseInstr = instrPrev->AsByteCodeUsesInstr();
            const BVSparse<JitArenaAllocator>* byteCodeUpwardExposedUsed = byteCodeUseInstr->GetByteCodeUpwardExposedUsed();
            if (byteCodeUpwardExposedUsed && byteCodeUpwardExposedUsed->Test(varSym->m_id) && byteCodeUpwardExposedUsed->Count() == 1)
            {TRACE_IT(1028);
                instrPrev = byteCodeUseInstr->GetPrevRealInstrOrLabel();

                if (!instrPrev->GetDst())
                {TRACE_IT(1029);
                    return false;
                }
            }
            else
            {TRACE_IT(1030);
                return false;
            }
        }
        else
        {TRACE_IT(1031);
            return false;
        }
    }

    // The fast-path for these doesn't handle dst == src.
    // REVIEW: I believe the fast-path for LdElemI_A has been fixed... Nope, still broken for "i = A[i]" for prejit
    switch (instrPrev->m_opcode)
    {
    case Js::OpCode::LdElemI_A:
    case Js::OpCode::IsInst:
    case Js::OpCode::ByteCodeUses:
        return false;
    }

    // Can't do it if post-op bailout would need result
    // REVIEW: enable for pre-opt bailout?
    if (instrPrev->HasBailOutInfo() && instrPrev->GetByteCodeOffset() != instrPrev->GetBailOutInfo()->bailOutOffset)
    {TRACE_IT(1032);
        return false;
    }

    // Make sure src of Ld_A == dst of instr
    //  t1 = instr
    if (!instrPrev->GetDst()->IsEqual(src))
    {TRACE_IT(1033);
        return false;
    }

    // Make sure t1 isn't used later
    if (this->currentBlock->upwardExposedUses->Test(src->m_sym->m_id))
    {TRACE_IT(1034);
        return false;
    }

    if (this->currentBlock->byteCodeUpwardExposedUsed && this->currentBlock->byteCodeUpwardExposedUsed->Test(varSym->m_id))
    {TRACE_IT(1035);
        return false;
    }

    // Make sure we can dead-store this sym (debugger mode?)
    if (!this->DoDeadStore(this->func, src->m_sym))
    {TRACE_IT(1036);
        return false;
    }

    StackSym *const dstSym = dst->m_sym;
    if(instrPrev->HasBailOutInfo() && dstSym->IsInt32() && dstSym->IsTypeSpec())
    {TRACE_IT(1037);
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
            {TRACE_IT(1038);
                if(dstSym == usedCopyPropSym.Value())
                {TRACE_IT(1039);
                    return false;
                }
            } NEXT_SLISTBASE_ENTRY;
        }
    }

    if (byteCodeUseInstr)
    {TRACE_IT(1040);
        if (this->currentBlock->byteCodeUpwardExposedUsed && instrPrev->GetDst()->AsRegOpnd()->GetIsJITOptimizedReg() && varSym->HasByteCodeRegSlot())
        {TRACE_IT(1041);
            if(varSym->HasByteCodeRegSlot())
            {TRACE_IT(1042);
                this->currentBlock->byteCodeUpwardExposedUsed->Set(varSym->m_id);
            }

            if (src->IsEqual(dst) && instrPrev->GetDst()->GetIsJITOptimizedReg())
            {TRACE_IT(1043);
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
    {TRACE_IT(1044);
        this->currentBlock->byteCodeUpwardExposedUsed->Set(varSym->m_id);
    }

#if DBG
    if (this->DoMarkTempObjectVerify())
    {TRACE_IT(1045);
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
{TRACE_IT(1046);
    Assert(instr->m_opcode == Js::OpCode::Conv_Bool);

    if (this->tag != Js::DeadStorePhase || this->IsPrePass() || this->IsCollectionPass())
    {TRACE_IT(1047);
        return false;
    }
    if (this->func->HasTry())
    {TRACE_IT(1048);
        // UpwardExposedUsed info can't be relied on
        return false;
    }

    IR::RegOpnd *intOpnd = instr->GetSrc1()->AsRegOpnd();

    Assert(intOpnd->m_sym->IsInt32());

    if (!intOpnd->m_sym->IsSingleDef())
    {TRACE_IT(1049);
        return false;
    }

    IR::Instr *cmInstr = intOpnd->m_sym->GetInstrDef();

    // Should be a Cm instr...
    if (!cmInstr->GetSrc2())
    {TRACE_IT(1050);
        return false;
    }

    IR::Instr *instrPrev = instr->GetPrevRealInstrOrLabel();

    if (instrPrev != cmInstr)
    {TRACE_IT(1051);
        return false;
    }

    switch (cmInstr->m_opcode)
    {
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
    {TRACE_IT(1052);
        return false;
    }

    varDst = instr->UnlinkDst()->AsRegOpnd();

    cmInstr->ReplaceDst(varDst);

    this->currentBlock->RemoveInstr(instr);

    return true;
}

void
BackwardPass::SetWriteThroughSymbolsSetForRegion(BasicBlock * catchBlock, Region * tryRegion)
{TRACE_IT(1053);
    tryRegion->writeThroughSymbolsSet = JitAnew(this->func->m_alloc, BVSparse<JitArenaAllocator>, this->func->m_alloc);

    if (this->DoByteCodeUpwardExposedUsed())
    {TRACE_IT(1054);
        Assert(catchBlock->byteCodeUpwardExposedUsed);
        if (!catchBlock->byteCodeUpwardExposedUsed->IsEmpty())
        {
            FOREACH_BITSET_IN_SPARSEBV(id, catchBlock->byteCodeUpwardExposedUsed)
            {TRACE_IT(1055);
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
        {TRACE_IT(1056);
            Region * parentTry = tryRegion->GetParent();
            Assert(parentTry->writeThroughSymbolsSet);
            FOREACH_BITSET_IN_SPARSEBV(id, parentTry->writeThroughSymbolsSet)
            {TRACE_IT(1057);
                Assert(tryRegion->writeThroughSymbolsSet->Test(id));
            }
            NEXT_BITSET_IN_SPARSEBV
        }
#endif
    }
    else
    {TRACE_IT(1058);
        // this can happen with -off:globopt
        return;
    }
}

bool
BackwardPass::CheckWriteThroughSymInRegion(Region* region, StackSym* sym)
{TRACE_IT(1059);
    if (region->GetType() == RegionTypeRoot || region->GetType() == RegionTypeFinally)
    {TRACE_IT(1060);
        return false;
    }

    // if the current region is a try region, check in its write-through set,
    // otherwise (current = catch region) look in the first try ancestor's write-through set
    Region * selfOrFirstTryAncestor = region->GetSelfOrFirstTryAncestor();
    if (!selfOrFirstTryAncestor)
    {TRACE_IT(1061);
        return false;
    }
    Assert(selfOrFirstTryAncestor->GetType() == RegionTypeTry);
    return selfOrFirstTryAncestor->writeThroughSymbolsSet && selfOrFirstTryAncestor->writeThroughSymbolsSet->Test(sym->m_id);
}

bool
BackwardPass::DoDeadStoreLdStForMemop(IR::Instr *instr)
{TRACE_IT(1062);
    Assert(this->tag == Js::DeadStorePhase && this->currentBlock->loop != nullptr);

    Loop *loop = this->currentBlock->loop;

    if (globOpt->HasMemOp(loop))
    {TRACE_IT(1063);
        if (instr->m_opcode == Js::OpCode::StElemI_A && instr->GetDst()->IsIndirOpnd())
        {TRACE_IT(1064);
            SymID base = this->globOpt->GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym());
            SymID index = this->globOpt->GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym());

            FOREACH_MEMOP_CANDIDATES(candidate, loop)
            {TRACE_IT(1065);
                if (base == candidate->base && index == candidate->index)
                {TRACE_IT(1066);
                    return true;
                }
            } NEXT_MEMOP_CANDIDATE
        }
        else if (instr->m_opcode == Js::OpCode::LdElemI_A &&  instr->GetSrc1()->IsIndirOpnd())
        {TRACE_IT(1067);
            SymID base = this->globOpt->GetVarSymID(instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym());
            SymID index = this->globOpt->GetVarSymID(instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym());

            FOREACH_MEMCOPY_CANDIDATES(candidate, loop)
            {TRACE_IT(1068);
                if (base == candidate->ldBase && index == candidate->index)
                {TRACE_IT(1069);
                    return true;
                }
            } NEXT_MEMCOPY_CANDIDATE
        }
    }
    return false;
}

void
BackwardPass::RestoreInductionVariableValuesAfterMemOp(Loop *loop)
{TRACE_IT(1070);
    const auto RestoreInductionVariable = [&](SymID symId, Loop::InductionVariableChangeInfo inductionVariableChangeInfo, Loop *loop)
    {TRACE_IT(1071);
        Js::OpCode opCode = Js::OpCode::Add_I4;
        if (!inductionVariableChangeInfo.isIncremental)
        {TRACE_IT(1072);
            opCode = Js::OpCode::Sub_I4;
        }
        Func *localFunc = loop->GetFunc();
        StackSym *sym = localFunc->m_symTable->FindStackSym(symId)->GetInt32EquivSym(localFunc);

        IR::Opnd *inductionVariableOpnd = IR::RegOpnd::New(sym, IRType::TyInt32, localFunc);
        IR::Opnd *sizeOpnd = globOpt->GenerateInductionVariableChangeForMemOp(loop, inductionVariableChangeInfo.unroll);
        loop->landingPad->InsertAfter(IR::Instr::New(opCode, inductionVariableOpnd, inductionVariableOpnd, sizeOpnd, loop->GetFunc()));
    };

    for (auto it = loop->memOpInfo->inductionVariableChangeInfoMap->GetIterator(); it.IsValid(); it.MoveNext())
    {TRACE_IT(1073);
        Loop::InductionVariableChangeInfo iv = it.CurrentValue();
        SymID sym = it.CurrentKey();
        if (iv.unroll != Js::Constants::InvalidLoopUnrollFactor)
        {TRACE_IT(1074);
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
{TRACE_IT(1075);
    if (globOpt->HasMemOp(loop))
    {TRACE_IT(1076);
        const auto IsInductionVariableUse = [&](IR::Opnd *opnd) -> bool
        {TRACE_IT(1077);
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
            {TRACE_IT(1078);
                if (instr->IsLabelInstr() || !instr->IsRealInstr() || instr->m_opcode == Js::OpCode::IncrLoopBodyCount || instr->m_opcode == Js::OpCode::StLoopBodyCount
                    || (instr->IsBranchInstr() && instr->AsBranchInstr()->IsUnconditional()))
                {TRACE_IT(1079);
                    continue;
                }
                else
                {TRACE_IT(1080);
                    switch (instr->m_opcode)
                    {
                    case Js::OpCode::Nop:
                        break;
                    case Js::OpCode::Ld_I4:
                    case Js::OpCode::Add_I4:
                    case Js::OpCode::Sub_I4:

                        if (!IsInductionVariableUse(instr->GetDst()))
                        {TRACE_IT(1081);
                            Assert(instr->GetDst());
                            if (instr->GetDst()->GetStackSym()
                                && loop->memOpInfo->inductionVariablesUsedAfterLoop->Test(globOpt->GetVarSymID(instr->GetDst()->GetStackSym())))
                            {TRACE_IT(1082);
                                // We have use after the loop for a variable defined inside the loop. So the loop can't be removed.
                                return false;
                            }
                        }
                        break;
                    case Js::OpCode::Decr_A:
                    case Js::OpCode::Incr_A:
                        if (!IsInductionVariableUse(instr->GetSrc1()))
                        {TRACE_IT(1083);
                            return false;
                        }
                        break;
                    default:
                        if (instr->IsBranchInstr())
                        {TRACE_IT(1084);
                            if (IsInductionVariableUse(instr->GetSrc1()) || IsInductionVariableUse(instr->GetSrc2()))
                            {TRACE_IT(1085);
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
{TRACE_IT(1086);
    if (PHASE_OFF(Js::MemOpPhase, this->func))
    {TRACE_IT(1087);
        return;

    }
    const auto DeleteMemOpInfo = [&](Loop *loop)
    {TRACE_IT(1088);
        JitArenaAllocator *alloc = this->func->GetTopFunc()->m_fg->alloc;

        if (!loop->memOpInfo)
        {TRACE_IT(1089);
            return;
        }

        if (loop->memOpInfo->candidates)
        {TRACE_IT(1090);
            loop->memOpInfo->candidates->Clear();
            JitAdelete(alloc, loop->memOpInfo->candidates);
        }

        if (loop->memOpInfo->inductionVariableChangeInfoMap)
        {TRACE_IT(1091);
            loop->memOpInfo->inductionVariableChangeInfoMap->Clear();
            JitAdelete(alloc, loop->memOpInfo->inductionVariableChangeInfoMap);
        }

        if (loop->memOpInfo->inductionVariableOpndPerUnrollMap)
        {TRACE_IT(1092);
            loop->memOpInfo->inductionVariableOpndPerUnrollMap->Clear();
            JitAdelete(alloc, loop->memOpInfo->inductionVariableOpndPerUnrollMap);
        }

        if (loop->memOpInfo->inductionVariablesUsedAfterLoop)
        {TRACE_IT(1093);
            JitAdelete(this->tempAlloc, loop->memOpInfo->inductionVariablesUsedAfterLoop);
        }
        JitAdelete(alloc, loop->memOpInfo);
    };

    FOREACH_LOOP_IN_FUNC_EDITING(loop, this->func)
    {TRACE_IT(1094);
        if (IsEmptyLoopAfterMemOp(loop))
        {TRACE_IT(1095);
            RestoreInductionVariableValuesAfterMemOp(loop);
            RemoveEmptyLoopAfterMemOp(loop);
        }
        // Remove memop info as we don't need them after this point.
        DeleteMemOpInfo(loop);

    } NEXT_LOOP_IN_FUNC_EDITING;

}

void
BackwardPass::RemoveEmptyLoopAfterMemOp(Loop *loop)
{TRACE_IT(1096);
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
    {TRACE_IT(1097);
        iter.Next();
        outerBlock = iter.Data()->GetSucc();
    }
    else
    {TRACE_IT(1098);
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
    {TRACE_IT(1099);
        this->func->m_fg->RemoveBlock(tail, nullptr);
    }
}

#if DBG_DUMP
bool
BackwardPass::IsTraceEnabled() const
{TRACE_IT(1100);
    return
        Js::Configuration::Global.flags.Trace.IsEnabled(tag, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()) &&
        (PHASE_TRACE(Js::SimpleJitPhase, func) || !func->IsSimpleJit());
}
#endif
