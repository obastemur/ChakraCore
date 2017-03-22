//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

FlowGraph *
FlowGraph::New(Func * func, JitArenaAllocator * alloc)
{LOGMEIN("FlowGraph.cpp] 8\n");
    FlowGraph * graph;

    graph = JitAnew(alloc, FlowGraph, func, alloc);

    return graph;
}

///----------------------------------------------------------------------------
///
/// FlowGraph::Build
///
/// Construct flow graph and loop structures for the current state of the function.
///
///----------------------------------------------------------------------------
void
FlowGraph::Build(void)
{LOGMEIN("FlowGraph.cpp] 25\n");
    Func * func = this->func;

    BEGIN_CODEGEN_PHASE(func, Js::FGPeepsPhase);
    this->RunPeeps();
    END_CODEGEN_PHASE(func, Js::FGPeepsPhase);

    // We don't optimize fully with SimpleJit. But, when JIT loop body is enabled, we do support
    // bailing out from a simple jitted function to do a full jit of a loop body in the function
    // (BailOnSimpleJitToFullJitLoopBody). For that purpose, we need the flow from try to catch.
    if (this->func->HasTry() &&
        (this->func->DoOptimizeTryCatch() ||
            (this->func->IsSimpleJit() && this->func->GetJITFunctionBody()->DoJITLoopBody())
        )
       )
    {LOGMEIN("FlowGraph.cpp] 40\n");
        this->catchLabelStack = JitAnew(this->alloc, SList<IR::LabelInstr*>, this->alloc);
    }

    IR::Instr * currLastInstr = nullptr;
    BasicBlock * currBlock = nullptr;
    BasicBlock * nextBlock = nullptr;
    bool hasCall = false;
    FOREACH_INSTR_IN_FUNC_BACKWARD_EDITING(instr, instrPrev, func)
    {LOGMEIN("FlowGraph.cpp] 49\n");
        if (currLastInstr == nullptr || instr->EndsBasicBlock())
        {LOGMEIN("FlowGraph.cpp] 51\n");
            // Start working on a new block.
            // If we're currently processing a block, then wrap it up before beginning a new one.
            if (currLastInstr != nullptr)
            {LOGMEIN("FlowGraph.cpp] 55\n");
                nextBlock = currBlock;
                currBlock = this->AddBlock(instr->m_next, currLastInstr, nextBlock);
                currBlock->hasCall = hasCall;
                hasCall = false;
            }

            currLastInstr = instr;
        }

        if (instr->StartsBasicBlock())
        {LOGMEIN("FlowGraph.cpp] 66\n");
            // Insert a BrOnException after the loop top if we are in a try-catch. This is required to
            // model flow from the loop to the catch block for loops that don't have a break condition.
            if (instr->IsLabelInstr() && instr->AsLabelInstr()->m_isLoopTop &&
                this->catchLabelStack && !this->catchLabelStack->Empty() &&
                instr->m_next->m_opcode != Js::OpCode::BrOnException)
            {LOGMEIN("FlowGraph.cpp] 72\n");
                IR::BranchInstr * brOnException = IR::BranchInstr::New(Js::OpCode::BrOnException, this->catchLabelStack->Top(), instr->m_func);
                instr->InsertAfter(brOnException);
                instrPrev = brOnException; // process BrOnException before adding a new block for loop top label.
                continue;
            }

            // Wrap up the current block and get ready to process a new one.
            nextBlock = currBlock;
            currBlock = this->AddBlock(instr, currLastInstr, nextBlock);
            currBlock->hasCall = hasCall;
            hasCall = false;
            currLastInstr = nullptr;
        }

        switch (instr->m_opcode)
        {LOGMEIN("FlowGraph.cpp] 88\n");
        case Js::OpCode::Catch:
            Assert(instr->m_prev->IsLabelInstr());
            if (this->catchLabelStack)
            {LOGMEIN("FlowGraph.cpp] 92\n");
                this->catchLabelStack->Push(instr->m_prev->AsLabelInstr());
            }
            break;

        case Js::OpCode::TryCatch:
            if (this->catchLabelStack)
            {LOGMEIN("FlowGraph.cpp] 99\n");
                this->catchLabelStack->Pop();
            }
            break;

        case Js::OpCode::CloneBlockScope:
        case Js::OpCode::CloneInnerScopeSlots:
            // It would be nice to do this in IRBuilder, but doing so gives us
            // trouble when doing the DoSlotArrayCheck since it assume single def
            // of the sym to do its check properly. So instead we assign the dst
            // here in FlowGraph.
            instr->SetDst(instr->GetSrc1());
            break;

        }

        if (OpCodeAttr::UseAllFields(instr->m_opcode))
        {LOGMEIN("FlowGraph.cpp] 116\n");
            // UseAllFields opcode are call instruction or opcode that would call.
            hasCall = true;

            if (OpCodeAttr::CallInstr(instr->m_opcode))
            {LOGMEIN("FlowGraph.cpp] 121\n");
                if (!instr->isCallInstrProtectedByNoProfileBailout)
                {LOGMEIN("FlowGraph.cpp] 123\n");
                    instr->m_func->SetHasCallsOnSelfAndParents();
                }

                // For ARM & X64 because of their register calling convention
                // the ArgOuts need to be moved next to the call.
#if defined(_M_ARM) || defined(_M_X64)

                IR::Instr* argInsertInstr = instr;
                instr->IterateArgInstrs([&](IR::Instr* argInstr)
                {
                    if (argInstr->m_opcode != Js::OpCode::LdSpreadIndices &&
                        argInstr->m_opcode != Js::OpCode::ArgOut_A_Dynamic &&
                        argInstr->m_opcode != Js::OpCode::ArgOut_A_FromStackArgs &&
                        argInstr->m_opcode != Js::OpCode::ArgOut_A_SpreadArg)
                    {LOGMEIN("FlowGraph.cpp] 138\n");
                        // don't have bailout in asm.js so we don't need BytecodeArgOutCapture
                        if (!argInstr->m_func->GetJITFunctionBody()->IsAsmJsMode())
                        {LOGMEIN("FlowGraph.cpp] 141\n");
                            // Need to always generate byte code arg out capture,
                            // because bailout can't restore from the arg out as it is
                            // replaced by new sym for register calling convention in lower
                            argInstr->GenerateBytecodeArgOutCapture();
                        }
                        // Check if the instruction is already next
                        if (argInstr != argInsertInstr->m_prev)
                        {LOGMEIN("FlowGraph.cpp] 149\n");
                            // It is not, move it.
                            argInstr->Move(argInsertInstr);
                        }
                        argInsertInstr = argInstr;
                    }
                    return false;
                });
#endif
            }
        }
    }
    NEXT_INSTR_IN_FUNC_BACKWARD_EDITING;
    this->func->isFlowGraphValid = true;
    Assert(!this->catchLabelStack || this->catchLabelStack->Empty());

    // We've been walking backward so that edge lists would be in the right order. Now walk the blocks
    // forward to number the blocks in lexical order.
    unsigned int blockNum = 0;
    FOREACH_BLOCK(block, this)
    {LOGMEIN("FlowGraph.cpp] 169\n");
        block->SetBlockNum(blockNum++);
    }NEXT_BLOCK;
    AssertMsg(blockNum == this->blockCount, "Block count is out of whack");

    this->RemoveUnreachableBlocks();

    this->FindLoops();

    bool breakBlocksRelocated = this->CanonicalizeLoops();

#if DBG
    this->VerifyLoopGraph();
#endif

    // Renumber the blocks. Break block remove code has likely inserted new basic blocks.
    blockNum = 0;

    // Regions need to be assigned before Globopt because:
    //     1. FullJit: The Backward Pass will set the write-through symbols on the regions and the forward pass will
    //        use this information to insert ToVars for those symbols. Also, for a symbol determined as write-through
    //        in the try region to be restored correctly by the bailout, it should not be removed from the
    //        byteCodeUpwardExposedUsed upon a def in the try region (the def might be preempted by an exception).
    //
    //     2. SimpleJit: Same case of correct restoration as above applies in SimpleJit too. However, the only bailout
    //        we have in Simple Jitted code right now is BailOnSimpleJitToFullJitLoopBody, installed in IRBuilder. So,
    //        for now, we can just check if the func has a bailout to assign regions pre globopt while running SimpleJit.

    bool assignRegionsBeforeGlobopt = this->func->HasTry() &&
                                (this->func->DoOptimizeTryCatch() || (this->func->IsSimpleJit() && this->func->hasBailout));
    Region ** blockToRegion = nullptr;
    if (assignRegionsBeforeGlobopt)
    {LOGMEIN("FlowGraph.cpp] 201\n");
        blockToRegion = JitAnewArrayZ(this->alloc, Region*, this->blockCount);
    }
    FOREACH_BLOCK_ALL(block, this)
    {LOGMEIN("FlowGraph.cpp] 205\n");
        block->SetBlockNum(blockNum++);
        if (assignRegionsBeforeGlobopt)
        {LOGMEIN("FlowGraph.cpp] 208\n");
            if (block->isDeleted && !block->isDead)
            {LOGMEIN("FlowGraph.cpp] 210\n");
                continue;
            }
            this->UpdateRegionForBlock(block, blockToRegion);
        }
    } NEXT_BLOCK_ALL;

    AssertMsg (blockNum == this->blockCount, "Block count is out of whack");

    if (breakBlocksRelocated)
    {LOGMEIN("FlowGraph.cpp] 220\n");
        // Sort loop lists only if there is break block removal.
        SortLoopLists();
    }
#if DBG_DUMP
    this->Dump(false, nullptr);
#endif
}

void
FlowGraph::SortLoopLists()
{LOGMEIN("FlowGraph.cpp] 231\n");
    // Sort the blocks in loopList
    for (Loop *loop = this->loopList; loop; loop = loop->next)
    {LOGMEIN("FlowGraph.cpp] 234\n");
        unsigned int lastBlockNumber = loop->GetHeadBlock()->GetBlockNum();
        // Insertion sort as the blockList is almost sorted in the loop.
        FOREACH_BLOCK_IN_LOOP_EDITING(block, loop, iter)
        {LOGMEIN("FlowGraph.cpp] 238\n");
            if (lastBlockNumber <= block->GetBlockNum())
            {LOGMEIN("FlowGraph.cpp] 240\n");
                lastBlockNumber = block->GetBlockNum();
            }
            else
            {
                iter.UnlinkCurrent();
                FOREACH_BLOCK_IN_LOOP_EDITING(insertBlock,loop,newIter)
                {LOGMEIN("FlowGraph.cpp] 247\n");
                    if (insertBlock->GetBlockNum() > block->GetBlockNum())
                    {LOGMEIN("FlowGraph.cpp] 249\n");
                        break;
                    }
                }NEXT_BLOCK_IN_LOOP_EDITING;
                newIter.InsertBefore(block);
            }
        }NEXT_BLOCK_IN_LOOP_EDITING;
    }
}

void
FlowGraph::RunPeeps()
{LOGMEIN("FlowGraph.cpp] 261\n");
    if (this->func->HasTry())
    {LOGMEIN("FlowGraph.cpp] 263\n");
        return;
    }

    if (PHASE_OFF(Js::FGPeepsPhase, this->func))
    {LOGMEIN("FlowGraph.cpp] 268\n");
        return;
    }

    IR::Instr * instrCm = nullptr;
    bool tryUnsignedCmpPeep = false;

    FOREACH_INSTR_IN_FUNC_EDITING(instr, instrNext, this->func)
    {LOGMEIN("FlowGraph.cpp] 276\n");
        switch(instr->m_opcode)
        {LOGMEIN("FlowGraph.cpp] 278\n");
        case Js::OpCode::Br:
        case Js::OpCode::BrEq_I4:
        case Js::OpCode::BrGe_I4:
        case Js::OpCode::BrGt_I4:
        case Js::OpCode::BrLt_I4:
        case Js::OpCode::BrLe_I4:
        case Js::OpCode::BrUnGe_I4:
        case Js::OpCode::BrUnGt_I4:
        case Js::OpCode::BrUnLt_I4:
        case Js::OpCode::BrUnLe_I4:
        case Js::OpCode::BrNeq_I4:
        case Js::OpCode::BrEq_A:
        case Js::OpCode::BrGe_A:
        case Js::OpCode::BrGt_A:
        case Js::OpCode::BrLt_A:
        case Js::OpCode::BrLe_A:
        case Js::OpCode::BrUnGe_A:
        case Js::OpCode::BrUnGt_A:
        case Js::OpCode::BrUnLt_A:
        case Js::OpCode::BrUnLe_A:
        case Js::OpCode::BrNotEq_A:
        case Js::OpCode::BrNotNeq_A:
        case Js::OpCode::BrSrNotEq_A:
        case Js::OpCode::BrSrNotNeq_A:
        case Js::OpCode::BrNotGe_A:
        case Js::OpCode::BrNotGt_A:
        case Js::OpCode::BrNotLt_A:
        case Js::OpCode::BrNotLe_A:
        case Js::OpCode::BrNeq_A:
        case Js::OpCode::BrNotNull_A:
        case Js::OpCode::BrNotAddr_A:
        case Js::OpCode::BrAddr_A:
        case Js::OpCode::BrSrEq_A:
        case Js::OpCode::BrSrNeq_A:
        case Js::OpCode::BrOnHasProperty:
        case Js::OpCode::BrOnNoProperty:
        case Js::OpCode::BrHasSideEffects:
        case Js::OpCode::BrNotHasSideEffects:
        case Js::OpCode::BrFncEqApply:
        case Js::OpCode::BrFncNeqApply:
        case Js::OpCode::BrOnEmpty:
        case Js::OpCode::BrOnNotEmpty:
        case Js::OpCode::BrFncCachedScopeEq:
        case Js::OpCode::BrFncCachedScopeNeq:
        case Js::OpCode::BrOnObject_A:
        case Js::OpCode::BrOnClassConstructor:
        case Js::OpCode::BrOnBaseConstructorKind:
            if (tryUnsignedCmpPeep)
            {LOGMEIN("FlowGraph.cpp] 327\n");
                this->UnsignedCmpPeep(instr);
            }
            instrNext = Peeps::PeepBranch(instr->AsBranchInstr());
            break;

        case Js::OpCode::MultiBr:
            // TODO: Run peeps on these as well...
            break;

        case Js::OpCode::BrTrue_I4:
        case Js::OpCode::BrFalse_I4:
        case Js::OpCode::BrTrue_A:
        case Js::OpCode::BrFalse_A:
            if (instrCm)
            {LOGMEIN("FlowGraph.cpp] 342\n");
                if (instrCm->GetDst()->IsInt32())
                {LOGMEIN("FlowGraph.cpp] 344\n");
                    Assert(instr->m_opcode == Js::OpCode::BrTrue_I4 || instr->m_opcode == Js::OpCode::BrFalse_I4);
                    instrNext = this->PeepTypedCm(instrCm);
                }
                else
                {
                    instrNext = this->PeepCm(instrCm);
                }
                instrCm = nullptr;

                if (instrNext == nullptr)
                {LOGMEIN("FlowGraph.cpp] 355\n");
                    // Set instrNext back to the current instr.
                    instrNext = instr;
                }
            }
            else
            {
                instrNext = Peeps::PeepBranch(instr->AsBranchInstr());
            }
            break;

        case Js::OpCode::CmEq_I4:
        case Js::OpCode::CmGe_I4:
        case Js::OpCode::CmGt_I4:
        case Js::OpCode::CmLt_I4:
        case Js::OpCode::CmLe_I4:
        case Js::OpCode::CmNeq_I4:
        case Js::OpCode::CmEq_A:
        case Js::OpCode::CmGe_A:
        case Js::OpCode::CmGt_A:
        case Js::OpCode::CmLt_A:
        case Js::OpCode::CmLe_A:
        case Js::OpCode::CmNeq_A:
        case Js::OpCode::CmSrEq_A:
        case Js::OpCode::CmSrNeq_A:
            if (tryUnsignedCmpPeep)
            {LOGMEIN("FlowGraph.cpp] 381\n");
                this->UnsignedCmpPeep(instr);
            }
        case Js::OpCode::CmUnGe_I4:
        case Js::OpCode::CmUnGt_I4:
        case Js::OpCode::CmUnLt_I4:
        case Js::OpCode::CmUnLe_I4:
        case Js::OpCode::CmUnGe_A:
        case Js::OpCode::CmUnGt_A:
        case Js::OpCode::CmUnLt_A:
        case Js::OpCode::CmUnLe_A:
            // There may be useless branches between the Cm instr and the branch that uses the result.
            // So save the last Cm instr seen, and trigger the peep on the next BrTrue/BrFalse.
            instrCm = instr;
            break;

        case Js::OpCode::Label:
            if (instr->AsLabelInstr()->IsUnreferenced())
            {LOGMEIN("FlowGraph.cpp] 399\n");
                instrNext = Peeps::PeepUnreachableLabel(instr->AsLabelInstr(), false);
            }
            break;

        case Js::OpCode::StatementBoundary:
            instr->ClearByteCodeOffset();
            instr->SetByteCodeOffset(instr->GetNextRealInstrOrLabel());
            break;

        case Js::OpCode::ShrU_I4:
        case Js::OpCode::ShrU_A:
            if (tryUnsignedCmpPeep)
            {LOGMEIN("FlowGraph.cpp] 412\n");
                break;
            }
            if (instr->GetDst()->AsRegOpnd()->m_sym->IsSingleDef()
                && instr->GetSrc2()->IsRegOpnd() && instr->GetSrc2()->AsRegOpnd()->m_sym->IsTaggableIntConst()
                && instr->GetSrc2()->AsRegOpnd()->m_sym->GetIntConstValue() == 0)
            {LOGMEIN("FlowGraph.cpp] 418\n");
                tryUnsignedCmpPeep = true;
            }
            break;
        default:
            Assert(!instr->IsBranchInstr());
        }
   } NEXT_INSTR_IN_FUNC_EDITING;
}

void
Loop::InsertLandingPad(FlowGraph *fg)
{LOGMEIN("FlowGraph.cpp] 430\n");
    BasicBlock *headBlock = this->GetHeadBlock();

    // Always create a landing pad.  This allows globopt to easily hoist instructions
    // and re-optimize the block if needed.
    BasicBlock *landingPad = BasicBlock::New(fg);
    this->landingPad = landingPad;
    IR::Instr * headInstr = headBlock->GetFirstInstr();
    IR::LabelInstr *landingPadLabel = IR::LabelInstr::New(Js::OpCode::Label, headInstr->m_func);
    landingPadLabel->SetByteCodeOffset(headInstr);
    headInstr->InsertBefore(landingPadLabel);

    landingPadLabel->SetBasicBlock(landingPad);

    landingPad->SetBlockNum(fg->blockCount++);
    landingPad->SetFirstInstr(landingPadLabel);
    landingPad->SetLastInstr(landingPadLabel);

    landingPad->prev = headBlock->prev;
    landingPad->prev->next = landingPad;
    landingPad->next = headBlock;
    headBlock->prev = landingPad;

    Loop *parentLoop = this->parent;
    landingPad->loop = parentLoop;

    // We need to add this block to the block list of the parent loops
    while (parentLoop)
    {
        // Find the head block in the block list of the parent loop
        FOREACH_BLOCK_IN_LOOP_EDITING(block, parentLoop, iter)
        {LOGMEIN("FlowGraph.cpp] 461\n");
            if (block == headBlock)
            {LOGMEIN("FlowGraph.cpp] 463\n");
                // Add the landing pad to the block list
                iter.InsertBefore(landingPad);
                break;
            }
        } NEXT_BLOCK_IN_LOOP_EDITING;

        parentLoop = parentLoop->parent;
    }

    // Fix predecessor flow edges
    FOREACH_PREDECESSOR_EDGE_EDITING(edge, headBlock, iter)
    {LOGMEIN("FlowGraph.cpp] 475\n");
        // Make sure it isn't a back-edge
        if (edge->GetPred()->loop != this && !this->IsDescendentOrSelf(edge->GetPred()->loop))
        {LOGMEIN("FlowGraph.cpp] 478\n");
            if (edge->GetPred()->GetLastInstr()->IsBranchInstr() && headBlock->GetFirstInstr()->IsLabelInstr())
            {LOGMEIN("FlowGraph.cpp] 480\n");
                IR::BranchInstr *branch = edge->GetPred()->GetLastInstr()->AsBranchInstr();
                branch->ReplaceTarget(headBlock->GetFirstInstr()->AsLabelInstr(), landingPadLabel);
            }
            headBlock->UnlinkPred(edge->GetPred(), false);
            landingPad->AddPred(edge, fg);
            edge->SetSucc(landingPad);
        }
    } NEXT_PREDECESSOR_EDGE_EDITING;

    fg->AddEdge(landingPad, headBlock);
}

bool
Loop::RemoveBreakBlocks(FlowGraph *fg)
{LOGMEIN("FlowGraph.cpp] 495\n");
    bool breakBlockRelocated = false;
    if (PHASE_OFF(Js::RemoveBreakBlockPhase, fg->GetFunc()))
    {LOGMEIN("FlowGraph.cpp] 498\n");
        return false;
    }

    BasicBlock *loopTailBlock = nullptr;
    FOREACH_BLOCK_IN_LOOP(block, this)
    {LOGMEIN("FlowGraph.cpp] 504\n");
        loopTailBlock = block;
    }NEXT_BLOCK_IN_LOOP;

    AnalysisAssert(loopTailBlock);

    FOREACH_BLOCK_BACKWARD_IN_RANGE_EDITING(breakBlockEnd, loopTailBlock, this->GetHeadBlock(), blockPrev)
    {LOGMEIN("FlowGraph.cpp] 511\n");
        while (!this->IsDescendentOrSelf(breakBlockEnd->loop))
        {LOGMEIN("FlowGraph.cpp] 513\n");
            // Found at least one break block;
            breakBlockRelocated = true;

#if DBG
            breakBlockEnd->isBreakBlock = true;
#endif
            // Find the first block in this break block sequence.
            BasicBlock *breakBlockStart = breakBlockEnd;
            BasicBlock *breakBlockStartPrev = breakBlockEnd->GetPrev();

            // Walk back the blocks until we find a block which belongs to that block.
            // Note: We don't really care if there are break blocks corresponding to different loops. We move the blocks conservatively to the end of the loop.

            // Algorithm works on one loop at a time.
            while((breakBlockStartPrev->loop == breakBlockEnd->loop) || !this->IsDescendentOrSelf(breakBlockStartPrev->loop))
            {LOGMEIN("FlowGraph.cpp] 529\n");
                breakBlockStart = breakBlockStartPrev;
                breakBlockStartPrev = breakBlockStartPrev->GetPrev();
            }

#if DBG
            breakBlockStart->isBreakBlock = true; // Mark the first block as well.
#endif

            BasicBlock *exitLoopTail = loopTailBlock;
            // Move these break blocks to the tail of the loop.
            fg->MoveBlocksBefore(breakBlockStart, breakBlockEnd, exitLoopTail->next);

#if DBG_DUMP
            fg->Dump(true /*needs verbose flag*/, _u("\n After Each iteration of canonicalization \n"));
#endif
            // Again be conservative, there are edits to the loop graph. Start fresh for this loop.
            breakBlockEnd = loopTailBlock;
            blockPrev = breakBlockEnd->prev;
        }
    } NEXT_BLOCK_BACKWARD_IN_RANGE_EDITING;

    return breakBlockRelocated;
}

void
FlowGraph::MoveBlocksBefore(BasicBlock *blockStart, BasicBlock *blockEnd, BasicBlock *insertBlock)
{LOGMEIN("FlowGraph.cpp] 556\n");
    BasicBlock *srcPredBlock = blockStart->prev;
    BasicBlock *srcNextBlock = blockEnd->next;
    BasicBlock *dstPredBlock = insertBlock->prev;
    IR::Instr* dstPredBlockLastInstr = dstPredBlock->GetLastInstr();
    IR::Instr* blockEndLastInstr = blockEnd->GetLastInstr();

    // Fix block linkage
    srcPredBlock->next = srcNextBlock;
    srcNextBlock->prev = srcPredBlock;

    dstPredBlock->next = blockStart;
    insertBlock->prev = blockEnd;

    blockStart->prev = dstPredBlock;
    blockEnd->next = insertBlock;

    // Fix instruction linkage
    IR::Instr::MoveRangeAfter(blockStart->GetFirstInstr(), blockEndLastInstr, dstPredBlockLastInstr);

    // Fix instruction flow
    IR::Instr *srcLastInstr = srcPredBlock->GetLastInstr();
    if (srcLastInstr->IsBranchInstr() && srcLastInstr->AsBranchInstr()->HasFallThrough())
    {LOGMEIN("FlowGraph.cpp] 579\n");
        // There was a fallthrough in the break blocks original position.
        IR::BranchInstr *srcBranch = srcLastInstr->AsBranchInstr();
        IR::Instr *srcBranchNextInstr = srcBranch->GetNextRealInstrOrLabel();

        // Save the target and invert the branch.
        IR::LabelInstr *srcBranchTarget = srcBranch->GetTarget();
        srcPredBlock->InvertBranch(srcBranch);
        IR::LabelInstr *srcLabel = blockStart->GetFirstInstr()->AsLabelInstr();

        // Point the inverted branch to break block.
        srcBranch->SetTarget(srcLabel);

        if (srcBranchNextInstr != srcBranchTarget)
        {LOGMEIN("FlowGraph.cpp] 593\n");
            FlowEdge *srcEdge  = this->FindEdge(srcPredBlock, srcBranchTarget->GetBasicBlock());
            Assert(srcEdge);

            BasicBlock *compensationBlock = this->InsertCompensationCodeForBlockMove(srcEdge, true /*insert compensation block to loop list*/, false /*At source*/);
            Assert(compensationBlock);
        }
    }

    IR::Instr *dstLastInstr = dstPredBlockLastInstr;
    if (dstLastInstr->IsBranchInstr() && dstLastInstr->AsBranchInstr()->HasFallThrough())
    {LOGMEIN("FlowGraph.cpp] 604\n");
        //There is a fallthrough in the block after which break block is inserted.
        FlowEdge *dstEdge = this->FindEdge(dstPredBlock, blockEnd->GetNext());
        Assert(dstEdge);

        BasicBlock *compensationBlock = this->InsertCompensationCodeForBlockMove(dstEdge, true /*insert compensation block to loop list*/, true /*At sink*/);
        Assert(compensationBlock);
    }
}

FlowEdge *
FlowGraph::FindEdge(BasicBlock *predBlock, BasicBlock *succBlock)
{LOGMEIN("FlowGraph.cpp] 616\n");
        FlowEdge *srcEdge = nullptr;
        FOREACH_SUCCESSOR_EDGE(edge, predBlock)
        {LOGMEIN("FlowGraph.cpp] 619\n");
            if (edge->GetSucc() == succBlock)
            {LOGMEIN("FlowGraph.cpp] 621\n");
                srcEdge = edge;
                break;
            }
        } NEXT_SUCCESSOR_EDGE;

        return srcEdge;
}

void
BasicBlock::InvertBranch(IR::BranchInstr *branch)
{LOGMEIN("FlowGraph.cpp] 632\n");
    Assert(this->GetLastInstr() == branch);
    Assert(this->GetSuccList()->HasTwo());

    branch->Invert();
    this->GetSuccList()->Reverse();
}

bool
FlowGraph::CanonicalizeLoops()
{LOGMEIN("FlowGraph.cpp] 642\n");
    if (this->func->HasProfileInfo())
    {LOGMEIN("FlowGraph.cpp] 644\n");
        this->implicitCallFlags = this->func->GetReadOnlyProfileInfo()->GetImplicitCallFlags();
        for (Loop *loop = this->loopList; loop; loop = loop->next)
        {LOGMEIN("FlowGraph.cpp] 647\n");
            this->implicitCallFlags = (Js::ImplicitCallFlags)(this->implicitCallFlags | loop->GetImplicitCallFlags());
        }
    }

#if DBG_DUMP
    this->Dump(true, _u("\n Before canonicalizeLoops \n"));
#endif

    bool breakBlockRelocated = false;

    for (Loop *loop = this->loopList; loop; loop = loop->next)
    {LOGMEIN("FlowGraph.cpp] 659\n");
        loop->InsertLandingPad(this);
        if (!this->func->HasTry() || this->func->DoOptimizeTryCatch())
        {LOGMEIN("FlowGraph.cpp] 662\n");
            bool relocated = loop->RemoveBreakBlocks(this);
            if (!breakBlockRelocated && relocated)
            {LOGMEIN("FlowGraph.cpp] 665\n");
                breakBlockRelocated  = true;
            }
        }
    }

#if DBG_DUMP
    this->Dump(true, _u("\n After canonicalizeLoops \n"));
#endif

    return breakBlockRelocated;
}

// Find the loops in this function, build the loop structure, and build a linked
// list of the basic blocks in this loop (including blocks of inner loops). The
// list preserves the reverse post-order of the blocks in the flowgraph block list.
void
FlowGraph::FindLoops()
{LOGMEIN("FlowGraph.cpp] 683\n");
    if (!this->hasLoop)
    {LOGMEIN("FlowGraph.cpp] 685\n");
        return;
    }

    Func * func = this->func;

    FOREACH_BLOCK_BACKWARD_IN_FUNC(block, func)
    {LOGMEIN("FlowGraph.cpp] 692\n");
        if (block->loop != nullptr)
        {LOGMEIN("FlowGraph.cpp] 694\n");
            // Block already visited
            continue;
        }
        FOREACH_SUCCESSOR_BLOCK(succ, block)
        {LOGMEIN("FlowGraph.cpp] 699\n");
            if (succ->isLoopHeader && succ->loop == nullptr)
            {
                // Found a loop back-edge
                BuildLoop(succ, block);
            }
        } NEXT_SUCCESSOR_BLOCK;
        if (block->isLoopHeader && block->loop == nullptr)
        {LOGMEIN("FlowGraph.cpp] 707\n");
            // We would have built a loop for it if it was a loop...
            block->isLoopHeader = false;
            block->GetFirstInstr()->AsLabelInstr()->m_isLoopTop = false;
        }
    } NEXT_BLOCK_BACKWARD_IN_FUNC;
}

void
FlowGraph::BuildLoop(BasicBlock *headBlock, BasicBlock *tailBlock, Loop *parentLoop)
{LOGMEIN("FlowGraph.cpp] 717\n");
    // This function is recursive, so when jitting in the foreground, probe the stack
    if(!func->IsBackgroundJIT())
    {LOGMEIN("FlowGraph.cpp] 720\n");
        PROBE_STACK(func->GetScriptContext(), Js::Constants::MinStackDefault);
    }

    if (tailBlock->number < headBlock->number)
    {LOGMEIN("FlowGraph.cpp] 725\n");
        // Not a loop.  We didn't see any back-edge.
        headBlock->isLoopHeader = false;
        headBlock->GetFirstInstr()->AsLabelInstr()->m_isLoopTop = false;
        return;
    }

    Assert(headBlock->isLoopHeader);
    Loop *loop = JitAnewZ(this->GetFunc()->m_alloc, Loop, this->GetFunc()->m_alloc, this->GetFunc());
    loop->next = this->loopList;
    this->loopList = loop;
    headBlock->loop = loop;
    loop->headBlock = headBlock;
    loop->int32SymsOnEntry = nullptr;
    loop->lossyInt32SymsOnEntry = nullptr;

    // If parentLoop is a parent of loop, it's headBlock better appear first.
    if (parentLoop && loop->headBlock->number > parentLoop->headBlock->number)
    {LOGMEIN("FlowGraph.cpp] 743\n");
        loop->parent = parentLoop;
        parentLoop->isLeaf = false;
    }
    loop->hasDeadStoreCollectionPass = false;
    loop->hasDeadStorePrepass = false;
    loop->memOpInfo = nullptr;
    loop->doMemOp = true;

    NoRecoverMemoryJitArenaAllocator tempAlloc(_u("BE-LoopBuilder"), this->func->m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);

    WalkLoopBlocks(tailBlock, loop, &tempAlloc);

    Assert(loop->GetHeadBlock() == headBlock);

    IR::LabelInstr * firstInstr = loop->GetLoopTopInstr();

    firstInstr->SetLoop(loop);

    if (firstInstr->IsProfiledLabelInstr())
    {LOGMEIN("FlowGraph.cpp] 763\n");
        loop->SetImplicitCallFlags(firstInstr->AsProfiledLabelInstr()->loopImplicitCallFlags);
        if (this->func->HasProfileInfo() && this->func->GetReadOnlyProfileInfo()->IsLoopImplicitCallInfoDisabled())
        {LOGMEIN("FlowGraph.cpp] 766\n");
            loop->SetImplicitCallFlags(this->func->GetReadOnlyProfileInfo()->GetImplicitCallFlags());
        }
        loop->SetLoopFlags(firstInstr->AsProfiledLabelInstr()->loopFlags);
    }
    else
    {
        // Didn't collect profile information, don't do optimizations
        loop->SetImplicitCallFlags(Js::ImplicitCall_All);
    }
}

Loop::MemCopyCandidate* Loop::MemOpCandidate::AsMemCopy()
{LOGMEIN("FlowGraph.cpp] 779\n");
    Assert(this->IsMemCopy());
    return (Loop::MemCopyCandidate*)this;
}

Loop::MemSetCandidate* Loop::MemOpCandidate::AsMemSet()
{LOGMEIN("FlowGraph.cpp] 785\n");
    Assert(this->IsMemSet());
    return (Loop::MemSetCandidate*)this;
}

void
Loop::EnsureMemOpVariablesInitialized()
{LOGMEIN("FlowGraph.cpp] 792\n");
    Assert(this->doMemOp);
    if (this->memOpInfo == nullptr)
    {LOGMEIN("FlowGraph.cpp] 795\n");
        JitArenaAllocator *allocator = this->GetFunc()->GetTopFunc()->m_fg->alloc;
        this->memOpInfo = JitAnewStruct(allocator, Loop::MemOpInfo);
        this->memOpInfo->inductionVariablesUsedAfterLoop = nullptr;
        this->memOpInfo->startIndexOpndCache[0] = nullptr;
        this->memOpInfo->startIndexOpndCache[1] = nullptr;
        this->memOpInfo->startIndexOpndCache[2] = nullptr;
        this->memOpInfo->startIndexOpndCache[3] = nullptr;
        this->memOpInfo->inductionVariableChangeInfoMap = JitAnew(allocator, Loop::InductionVariableChangeInfoMap, allocator);
        this->memOpInfo->inductionVariableOpndPerUnrollMap = JitAnew(allocator, Loop::InductionVariableOpndPerUnrollMap, allocator);
        this->memOpInfo->candidates = JitAnew(allocator, Loop::MemOpList, allocator);
    }
}

// Walk the basic blocks backwards until we find the loop header.
// Mark basic blocks in the loop by looking at the predecessors
// of blocks known to be in the loop.
// Recurse on inner loops.
void
FlowGraph::WalkLoopBlocks(BasicBlock *block, Loop *loop, JitArenaAllocator *tempAlloc)
{LOGMEIN("FlowGraph.cpp] 815\n");
    AnalysisAssert(loop);

    BVSparse<JitArenaAllocator> *loopBlocksBv = JitAnew(tempAlloc, BVSparse<JitArenaAllocator>, tempAlloc);
    BasicBlock *tailBlock = block;
    BasicBlock *lastBlock;
    loopBlocksBv->Set(block->GetBlockNum());

    this->AddBlockToLoop(block, loop);

    if (block == loop->headBlock)
    {LOGMEIN("FlowGraph.cpp] 826\n");
        // Single block loop, we're done
        return;
    }

    do
    {LOGMEIN("FlowGraph.cpp] 832\n");
        BOOL isInLoop = loopBlocksBv->Test(block->GetBlockNum());

        FOREACH_SUCCESSOR_BLOCK(succ, block)
        {LOGMEIN("FlowGraph.cpp] 836\n");
            if (succ->isLoopHeader)
            {LOGMEIN("FlowGraph.cpp] 838\n");
                // Found a loop back-edge
                if (loop->headBlock == succ)
                {LOGMEIN("FlowGraph.cpp] 841\n");
                    isInLoop = true;
                }
                else if (succ->loop == nullptr || succ->loop->headBlock != succ)
                {
                    // Recurse on inner loop
                    BuildLoop(succ, block, isInLoop ? loop : nullptr);
                }
            }
        } NEXT_SUCCESSOR_BLOCK;

        if (isInLoop)
        {
            // This block is in the loop.  All of it's predecessors should be contained in the loop as well.
            FOREACH_PREDECESSOR_BLOCK(pred, block)
            {LOGMEIN("FlowGraph.cpp] 856\n");
                // Fix up loop parent if it isn't set already.
                // If pred->loop != loop, we're looking at an inner loop, which was already visited.
                // If pred->loop->parent == nullptr, this is the first time we see this loop from an outer
                // loop, so this must be an immediate child.
                if (pred->loop && pred->loop != loop && loop->headBlock->number < pred->loop->headBlock->number
                    && (pred->loop->parent == nullptr || pred->loop->parent->headBlock->number < loop->headBlock->number))
                {LOGMEIN("FlowGraph.cpp] 863\n");
                    pred->loop->parent = loop;
                    loop->isLeaf = false;
                    if (pred->loop->hasCall)
                    {LOGMEIN("FlowGraph.cpp] 867\n");
                        loop->SetHasCall();
                    }
                    loop->SetImplicitCallFlags(pred->loop->GetImplicitCallFlags());
                }
                // Add pred to loop bit vector
                loopBlocksBv->Set(pred->GetBlockNum());
            } NEXT_PREDECESSOR_BLOCK;

            if (block->loop == nullptr || block->loop->IsDescendentOrSelf(loop))
            {LOGMEIN("FlowGraph.cpp] 877\n");
                block->loop = loop;
            }

            if (block != tailBlock)
            {LOGMEIN("FlowGraph.cpp] 882\n");
                this->AddBlockToLoop(block, loop);
            }
        }
        lastBlock = block;
        block = block->GetPrev();
    } while (lastBlock != loop->headBlock);
}

// Add block to this loop, and it's parent loops.
void
FlowGraph::AddBlockToLoop(BasicBlock *block, Loop *loop)
{LOGMEIN("FlowGraph.cpp] 894\n");
    loop->blockList.Prepend(block);
    if (block->hasCall)
    {LOGMEIN("FlowGraph.cpp] 897\n");
        loop->SetHasCall();
    }
}

///----------------------------------------------------------------------------
///
/// FlowGraph::AddBlock
///
/// Finish processing of a new block: hook up successor arcs, note loops, etc.
///
///----------------------------------------------------------------------------
BasicBlock *
FlowGraph::AddBlock(
    IR::Instr * firstInstr,
    IR::Instr * lastInstr,
    BasicBlock * nextBlock)
{LOGMEIN("FlowGraph.cpp] 914\n");
    BasicBlock * block;
    IR::LabelInstr * labelInstr;

    if (firstInstr->IsLabelInstr())
    {LOGMEIN("FlowGraph.cpp] 919\n");
        labelInstr = firstInstr->AsLabelInstr();
    }
    else
    {
        labelInstr = IR::LabelInstr::New(Js::OpCode::Label, firstInstr->m_func);
        labelInstr->SetByteCodeOffset(firstInstr);
        if (firstInstr->IsEntryInstr())
        {LOGMEIN("FlowGraph.cpp] 927\n");
            firstInstr->InsertAfter(labelInstr);
        }
        else
        {
            firstInstr->InsertBefore(labelInstr);
        }
        firstInstr = labelInstr;
    }

    block = labelInstr->GetBasicBlock();
    if (block == nullptr)
    {LOGMEIN("FlowGraph.cpp] 939\n");
        block = BasicBlock::New(this);
        labelInstr->SetBasicBlock(block);
        // Remember last block in function to target successor of RETs.
        if (!this->tailBlock)
        {LOGMEIN("FlowGraph.cpp] 944\n");
            this->tailBlock = block;
        }
    }

    // Hook up the successor edges
    if (lastInstr->EndsBasicBlock())
    {LOGMEIN("FlowGraph.cpp] 951\n");
        BasicBlock * blockTarget = nullptr;

        if (lastInstr->IsBranchInstr())
        {LOGMEIN("FlowGraph.cpp] 955\n");
            // Hook up a successor edge to the branch target.
            IR::BranchInstr * branchInstr = lastInstr->AsBranchInstr();

            if(branchInstr->IsMultiBranch())
            {LOGMEIN("FlowGraph.cpp] 960\n");
                BasicBlock * blockMultiBrTarget;

                IR::MultiBranchInstr * multiBranchInstr = branchInstr->AsMultiBrInstr();

                multiBranchInstr->MapUniqueMultiBrLabels([&](IR::LabelInstr * labelInstr) -> void
                {
                    blockMultiBrTarget = SetBlockTargetAndLoopFlag(labelInstr);
                    this->AddEdge(block, blockMultiBrTarget);
                });
            }
            else
            {
                IR::LabelInstr * targetLabelInstr = branchInstr->GetTarget();
                blockTarget = SetBlockTargetAndLoopFlag(targetLabelInstr);
                if (branchInstr->IsConditional())
                {LOGMEIN("FlowGraph.cpp] 976\n");
                    IR::Instr *instrNext = branchInstr->GetNextRealInstrOrLabel();

                    if (instrNext->IsLabelInstr())
                    {LOGMEIN("FlowGraph.cpp] 980\n");
                        SetBlockTargetAndLoopFlag(instrNext->AsLabelInstr());
                    }
                }
            }
        }
        else if (lastInstr->m_opcode == Js::OpCode::Ret && block != this->tailBlock)
        {LOGMEIN("FlowGraph.cpp] 987\n");
            blockTarget = this->tailBlock;
        }

        if (blockTarget)
        {LOGMEIN("FlowGraph.cpp] 992\n");
            this->AddEdge(block, blockTarget);
        }
    }

    if (lastInstr->HasFallThrough())
    {LOGMEIN("FlowGraph.cpp] 998\n");
        // Add a branch to next instruction so that we don't have to update the flow graph
        // when the glob opt tries to insert instructions.
        // We don't run the globopt with try/catch, don't need to insert branch to next for fall through blocks.
        if (!this->func->HasTry() && !lastInstr->IsBranchInstr())
        {LOGMEIN("FlowGraph.cpp] 1003\n");
            IR::BranchInstr * instr = IR::BranchInstr::New(Js::OpCode::Br,
                lastInstr->m_next->AsLabelInstr(), lastInstr->m_func);
            instr->SetByteCodeOffset(lastInstr->m_next);
            lastInstr->InsertAfter(instr);
            lastInstr = instr;
        }
        this->AddEdge(block, nextBlock);
    }

    block->SetBlockNum(this->blockCount++);
    block->SetFirstInstr(firstInstr);
    block->SetLastInstr(lastInstr);

    if (this->blockList)
    {LOGMEIN("FlowGraph.cpp] 1018\n");
        this->blockList->prev = block;
    }
    block->next = this->blockList;
    this->blockList = block;

    return block;
}

BasicBlock *
FlowGraph::SetBlockTargetAndLoopFlag(IR::LabelInstr * labelInstr)
{LOGMEIN("FlowGraph.cpp] 1029\n");
    BasicBlock * blockTarget = nullptr;
    blockTarget = labelInstr->GetBasicBlock();

    if (blockTarget == nullptr)
    {LOGMEIN("FlowGraph.cpp] 1034\n");
        blockTarget = BasicBlock::New(this);
        labelInstr->SetBasicBlock(blockTarget);
    }
    if (labelInstr->m_isLoopTop)
    {LOGMEIN("FlowGraph.cpp] 1039\n");
        blockTarget->isLoopHeader = true;
        this->hasLoop = true;
    }

    return blockTarget;
}

///----------------------------------------------------------------------------
///
/// FlowGraph::AddEdge
///
/// Add an edge connecting the two given blocks.
///
///----------------------------------------------------------------------------
FlowEdge *
FlowGraph::AddEdge(BasicBlock * blockPred, BasicBlock * blockSucc)
{LOGMEIN("FlowGraph.cpp] 1056\n");
    FlowEdge * edge = FlowEdge::New(this);
    edge->SetPred(blockPred);
    edge->SetSucc(blockSucc);
    blockPred->AddSucc(edge, this);
    blockSucc->AddPred(edge, this);

    return edge;
}

///----------------------------------------------------------------------------
///
/// FlowGraph::Destroy
///
/// Remove all references to FG structures from the IR in preparation for freeing
/// the FG.
///
///----------------------------------------------------------------------------
void
FlowGraph::Destroy(void)
{LOGMEIN("FlowGraph.cpp] 1076\n");
    BOOL fHasTry = this->func->HasTry();
    Region ** blockToRegion = nullptr;
    if (fHasTry)
    {LOGMEIN("FlowGraph.cpp] 1080\n");
        blockToRegion = JitAnewArrayZ(this->alloc, Region*, this->blockCount);
        // Do unreachable code removal up front to avoid problems
        // with unreachable back edges, etc.
        this->RemoveUnreachableBlocks();
    }

    FOREACH_BLOCK_ALL(block, this)
    {LOGMEIN("FlowGraph.cpp] 1088\n");
        IR::Instr * firstInstr = block->GetFirstInstr();
        if (block->isDeleted && !block->isDead)
        {LOGMEIN("FlowGraph.cpp] 1091\n");
            if (firstInstr->IsLabelInstr())
            {LOGMEIN("FlowGraph.cpp] 1093\n");
                IR::LabelInstr * labelInstr = firstInstr->AsLabelInstr();
                labelInstr->UnlinkBasicBlock();
                // Removing the label for non try blocks as we have a deleted block which has the label instruction
                // still not removed; this prevents the assert for cases where the deleted blocks fall through to a helper block,
                // i.e. helper introduced by polymorphic inlining bailout.
                // Skipping Try blocks as we have dependency on blocks to get the last instr(see below in this function)
                if (!fHasTry)
                {LOGMEIN("FlowGraph.cpp] 1101\n");
                    if (this->func->GetJITFunctionBody()->IsCoroutine())
                    {LOGMEIN("FlowGraph.cpp] 1103\n");
                        // the label could be a yield resume label, in which case we also need to remove it from the YieldOffsetResumeLabels list
                        this->func->MapUntilYieldOffsetResumeLabels([this, &labelInstr](int i, const YieldOffsetResumeLabel& yorl)
                        {
                            if (labelInstr == yorl.Second())
                            {LOGMEIN("FlowGraph.cpp] 1108\n");
                                labelInstr->m_hasNonBranchRef = false;
                                this->func->RemoveYieldOffsetResumeLabel(yorl);
                                return true;
                            }
                            return false;
                        });
                    }

                    Assert(labelInstr->IsUnreferenced());
                    labelInstr->Remove();
                }
            }
            continue;
        }

        if (block->isLoopHeader && !block->isDead)
        {LOGMEIN("FlowGraph.cpp] 1125\n");
            // Mark the tail block of this loop (the last back-edge).  The register allocator
            // uses this to lexically find loops.
            BasicBlock *loopTail = nullptr;

            AssertMsg(firstInstr->IsLabelInstr() && firstInstr->AsLabelInstr()->m_isLoopTop,
                "Label not marked as loop top...");
            FOREACH_BLOCK_IN_LOOP(loopBlock, block->loop)
            {
                FOREACH_SUCCESSOR_BLOCK(succ, loopBlock)
                {LOGMEIN("FlowGraph.cpp] 1135\n");
                    if (succ == block)
                    {LOGMEIN("FlowGraph.cpp] 1137\n");
                        loopTail = loopBlock;
                        break;
                    }
                } NEXT_SUCCESSOR_BLOCK;
            } NEXT_BLOCK_IN_LOOP;

            if (loopTail)
            {LOGMEIN("FlowGraph.cpp] 1145\n");
                AssertMsg(loopTail->GetLastInstr()->IsBranchInstr(), "LastInstr of loop should always be a branch no?");
                block->loop->SetLoopTopInstr(block->GetFirstInstr()->AsLabelInstr());
            }
            else
            {
                // This loop doesn't have a back-edge: that is, it is not a loop
                // anymore...
                firstInstr->AsLabelInstr()->m_isLoopTop = FALSE;
            }
        }

        if (fHasTry)
        {LOGMEIN("FlowGraph.cpp] 1158\n");
            this->UpdateRegionForBlock(block, blockToRegion);
        }

        if (firstInstr->IsLabelInstr())
        {LOGMEIN("FlowGraph.cpp] 1163\n");
            IR::LabelInstr * labelInstr = firstInstr->AsLabelInstr();
            labelInstr->UnlinkBasicBlock();
            if (labelInstr->IsUnreferenced() && !fHasTry)
            {LOGMEIN("FlowGraph.cpp] 1167\n");
                // This is an unreferenced label, probably added by FG building.
                // Delete it now to make extended basic blocks visible.
                if (firstInstr == block->GetLastInstr())
                {LOGMEIN("FlowGraph.cpp] 1171\n");
                    labelInstr->Remove();
                    continue;
                }
                else
                {
                    labelInstr->Remove();
                }
            }
        }

        // We don't run the globopt with try/catch, don't need to remove branch to next for fall through blocks
        IR::Instr * lastInstr = block->GetLastInstr();
        if (!fHasTry && lastInstr->IsBranchInstr())
        {LOGMEIN("FlowGraph.cpp] 1185\n");
            IR::BranchInstr * branchInstr = lastInstr->AsBranchInstr();
            if (!branchInstr->IsConditional() && branchInstr->GetTarget() == branchInstr->m_next)
            {LOGMEIN("FlowGraph.cpp] 1188\n");
                // Remove branch to next
                branchInstr->Remove();
            }
        }
    }
    NEXT_BLOCK;

#if DBG

    if (fHasTry)
    {
        // Now that all blocks have regions, we should see consistently propagated regions at all
        // block boundaries.
        FOREACH_BLOCK(block, this)
        {LOGMEIN("FlowGraph.cpp] 1203\n");
            Region * region = blockToRegion[block->GetBlockNum()];
            Region * predRegion = nullptr;
            FOREACH_PREDECESSOR_BLOCK(predBlock, block)
            {LOGMEIN("FlowGraph.cpp] 1207\n");
                predRegion = blockToRegion[predBlock->GetBlockNum()];
                if (predBlock->GetLastInstr() == nullptr)
                {LOGMEIN("FlowGraph.cpp] 1210\n");
                    AssertMsg(region == predRegion, "Bad region propagation through empty block");
                }
                else
                {
                    switch (predBlock->GetLastInstr()->m_opcode)
                    {LOGMEIN("FlowGraph.cpp] 1216\n");
                    case Js::OpCode::TryCatch:
                    case Js::OpCode::TryFinally:
                        AssertMsg(region->GetParent() == predRegion, "Bad region prop on entry to try-catch/finally");
                        if (block->GetFirstInstr() == predBlock->GetLastInstr()->AsBranchInstr()->GetTarget())
                        {LOGMEIN("FlowGraph.cpp] 1221\n");
                            if (predBlock->GetLastInstr()->m_opcode == Js::OpCode::TryCatch)
                            {LOGMEIN("FlowGraph.cpp] 1223\n");
                                AssertMsg(region->GetType() == RegionTypeCatch, "Bad region type on entry to catch");
                            }
                            else
                            {
                                AssertMsg(region->GetType() == RegionTypeFinally, "Bad region type on entry to finally");
                            }
                        }
                        else
                        {
                            AssertMsg(region->GetType() == RegionTypeTry, "Bad region type on entry to try");
                        }
                        break;
                    case Js::OpCode::Leave:
                    case Js::OpCode::LeaveNull:
                        AssertMsg(region == predRegion->GetParent() || (region == predRegion && this->func->IsLoopBodyInTry()), "Bad region prop on leaving try-catch/finally");
                        break;

                    // If the try region has a branch out of the loop,
                    // - the branch is moved out of the loop as part of break block removal, and
                    // - BrOnException is inverted to BrOnNoException and a Br is inserted after it.
                    // Otherwise,
                    // - FullJit: BrOnException is removed in the forward pass.
                    case Js::OpCode::BrOnException:
                        Assert(!this->func->DoGlobOpt());
                    case Js::OpCode::BrOnNoException:
                        Assert(this->func->HasTry() &&
                               ((!this->func->HasFinally() && !this->func->IsLoopBody() && !PHASE_OFF(Js::OptimizeTryCatchPhase, this->func)) ||
                               (this->func->IsSimpleJit() && this->func->GetJITFunctionBody()->DoJITLoopBody()))); // should be relaxed as more bailouts are added in Simple Jit

                        Assert(region->GetType() == RegionTypeTry || region->GetType() == RegionTypeCatch);
                        if (region->GetType() == RegionTypeCatch)
                        {LOGMEIN("FlowGraph.cpp] 1255\n");
                            Assert((predRegion->GetType() == RegionTypeTry) || (predRegion->GetType() == RegionTypeCatch));
                        }
                        else if (region->GetType() == RegionTypeTry)
                        {LOGMEIN("FlowGraph.cpp] 1259\n");
                            Assert(region == predRegion);
                        }
                        break;
                    case Js::OpCode::Br:
                        if (region->GetType() == RegionTypeCatch && region != predRegion)
                        {LOGMEIN("FlowGraph.cpp] 1265\n");
                            AssertMsg(predRegion->GetType() == RegionTypeTry, "Bad region type for the try");
                        }
                        else
                        {
                            AssertMsg(region == predRegion, "Bad region propagation through interior block");
                        }
                        break;
                    default:
                        AssertMsg(region == predRegion, "Bad region propagation through interior block");
                        break;
                    }
                }
            }
            NEXT_PREDECESSOR_BLOCK;

            switch (region->GetType())
            {LOGMEIN("FlowGraph.cpp] 1282\n");
            case RegionTypeRoot:
                Assert(!region->GetMatchingTryRegion() && !region->GetMatchingCatchRegion() && !region->GetMatchingFinallyRegion());
                break;

            case RegionTypeTry:
                Assert(!(region->GetMatchingCatchRegion() && region->GetMatchingFinallyRegion()));
                break;

            case RegionTypeCatch:
            case RegionTypeFinally:
                Assert(region->GetMatchingTryRegion());
                break;
            }
        }
        NEXT_BLOCK;
        FOREACH_BLOCK_DEAD_OR_ALIVE(block, this)
        {LOGMEIN("FlowGraph.cpp] 1299\n");
            if (block->GetFirstInstr()->IsLabelInstr())
            {LOGMEIN("FlowGraph.cpp] 1301\n");
                IR::LabelInstr *labelInstr = block->GetFirstInstr()->AsLabelInstr();
                if (labelInstr->IsUnreferenced())
                {LOGMEIN("FlowGraph.cpp] 1304\n");
                    // This is an unreferenced label, probably added by FG building.
                    // Delete it now to make extended basic blocks visible.
                    labelInstr->Remove();
                }
            }
        } NEXT_BLOCK_DEAD_OR_ALIVE;
    }
#endif

    this->func->isFlowGraphValid = false;
}

// Propagate the region forward from the block's predecessor(s), tracking the effect
// of the flow transition. Record the region in the block-to-region map provided
// and on the label at the entry to the block (if any).
void
FlowGraph::UpdateRegionForBlock(BasicBlock * block, Region ** blockToRegion)
{LOGMEIN("FlowGraph.cpp] 1322\n");
    Region *region;
    Region * predRegion = nullptr;
    IR::Instr * tryInstr = nullptr;
    IR::Instr * firstInstr = block->GetFirstInstr();
    if (firstInstr->IsLabelInstr() && firstInstr->AsLabelInstr()->GetRegion())
    {LOGMEIN("FlowGraph.cpp] 1328\n");
        Assert(this->func->HasTry() && (this->func->DoOptimizeTryCatch() || (this->func->IsSimpleJit() && this->func->hasBailout)));
        blockToRegion[block->GetBlockNum()] = firstInstr->AsLabelInstr()->GetRegion();
        return;
    }

    if (block == this->blockList)
    {LOGMEIN("FlowGraph.cpp] 1335\n");
        // Head of the graph: create the root region.
        region = Region::New(RegionTypeRoot, nullptr, this->func);
    }
    else
    {
        // Propagate the region forward by finding a predecessor we've already processed.
        // We require that there be one, since we've already removed unreachable blocks.
        region = nullptr;
        FOREACH_PREDECESSOR_BLOCK(predBlock, block)
        {LOGMEIN("FlowGraph.cpp] 1345\n");
            AssertMsg(predBlock->GetBlockNum() < this->blockCount, "Misnumbered block at teardown time?");
            predRegion = blockToRegion[predBlock->GetBlockNum()];
            if (predRegion != nullptr)
            {LOGMEIN("FlowGraph.cpp] 1349\n");
                region = this->PropagateRegionFromPred(block, predBlock, predRegion, tryInstr);
                break;
            }
        }
        NEXT_PREDECESSOR_BLOCK;
    }

    AnalysisAssertMsg(region != nullptr, "Failed to find region for block");
    if (!region->ehBailoutData)
    {LOGMEIN("FlowGraph.cpp] 1359\n");
        region->AllocateEHBailoutData(this->func, tryInstr);
    }

    // Record the region in the block-to-region map.
    blockToRegion[block->GetBlockNum()] = region;
    if (firstInstr->IsLabelInstr())
    {LOGMEIN("FlowGraph.cpp] 1366\n");
        // Record the region on the label and make sure it stays around as a region
        // marker if we're entering a region at this point.
        IR::LabelInstr * labelInstr = firstInstr->AsLabelInstr();
        labelInstr->SetRegion(region);
        if (region != predRegion)
        {LOGMEIN("FlowGraph.cpp] 1372\n");
            labelInstr->m_hasNonBranchRef = true;
        }
    }
}

Region *
FlowGraph::PropagateRegionFromPred(BasicBlock * block, BasicBlock * predBlock, Region * predRegion, IR::Instr * &tryInstr)
{LOGMEIN("FlowGraph.cpp] 1380\n");
    // Propagate predRegion to region, looking at the flow transition for an opcode
    // that affects the region.
    Region * region = nullptr;
    IR::Instr * predLastInstr = predBlock->GetLastInstr();
    IR::Instr * firstInstr = block->GetFirstInstr();
    if (predLastInstr == nullptr)
    {LOGMEIN("FlowGraph.cpp] 1387\n");
        // Empty block: trivially propagate the region.
        region = predRegion;
    }
    else
    {
        Region * tryRegion = nullptr;
        IR::LabelInstr * tryInstrNext = nullptr;
        switch (predLastInstr->m_opcode)
        {LOGMEIN("FlowGraph.cpp] 1396\n");
        case Js::OpCode::TryCatch:
            // Entry to a try-catch. See whether we're entering the try or the catch
            // by looking for the handler label.
            Assert(predLastInstr->m_next->IsLabelInstr());
            tryInstrNext = predLastInstr->m_next->AsLabelInstr();
            tryRegion = tryInstrNext->GetRegion();

            if (firstInstr == predLastInstr->AsBranchInstr()->GetTarget())
            {LOGMEIN("FlowGraph.cpp] 1405\n");
                region = Region::New(RegionTypeCatch, predRegion, this->func);
                Assert(tryRegion);
                region->SetMatchingTryRegion(tryRegion);
                tryRegion->SetMatchingCatchRegion(region);
            }
            else
            {
                region = Region::New(RegionTypeTry, predRegion, this->func);
                tryInstr = predLastInstr;
            }
            break;

        case Js::OpCode::TryFinally:
            // Entry to a try-finally. See whether we're entering the try or the finally
            // by looking for the handler label.
            Assert(predLastInstr->m_next->IsLabelInstr());
            tryInstrNext = predLastInstr->m_next->AsLabelInstr();
            tryRegion = tryInstrNext->GetRegion();

            if (firstInstr == predLastInstr->AsBranchInstr()->GetTarget())
            {LOGMEIN("FlowGraph.cpp] 1426\n");
                region = Region::New(RegionTypeFinally, predRegion, this->func);
                Assert(tryRegion);
                region->SetMatchingTryRegion(tryRegion);
                tryRegion->SetMatchingFinallyRegion(region);
            }
            else
            {
                region = Region::New(RegionTypeTry, predRegion, this->func);
                tryInstr = predLastInstr;
            }
            break;

        case Js::OpCode::Leave:
        case Js::OpCode::LeaveNull:
            // Exiting a try or handler. Retrieve the current region's parent.
            region = predRegion->GetParent();
            if (region == nullptr)
            {LOGMEIN("FlowGraph.cpp] 1444\n");
                // We found a Leave in the root region- this can only happen when a jitted loop body
                // in a try block has a return statement.
                Assert(this->func->IsLoopBodyInTry());
                predLastInstr->AsBranchInstr()->m_isOrphanedLeave = true;
                region = predRegion;
            }
            break;

        default:
            // Normal (non-EH) transition: just propagate the region.
            region = predRegion;
            break;
        }
    }
    return region;
}

void
FlowGraph::InsertCompBlockToLoopList(Loop *loop, BasicBlock* compBlock, BasicBlock* targetBlock, bool postTarget)
{LOGMEIN("FlowGraph.cpp] 1464\n");
    if (loop)
    {LOGMEIN("FlowGraph.cpp] 1466\n");
        bool found = false;
        FOREACH_BLOCK_IN_LOOP_EDITING(loopBlock, loop, iter)
        {LOGMEIN("FlowGraph.cpp] 1469\n");
            if (loopBlock == targetBlock)
            {LOGMEIN("FlowGraph.cpp] 1471\n");
                found = true;
                break;
            }
        } NEXT_BLOCK_IN_LOOP_EDITING;
        if (found)
        {LOGMEIN("FlowGraph.cpp] 1477\n");
            if (postTarget)
            {LOGMEIN("FlowGraph.cpp] 1479\n");
                iter.Next();
            }
            iter.InsertBefore(compBlock);
        }
        InsertCompBlockToLoopList(loop->parent, compBlock, targetBlock, postTarget);
    }
}

// Insert a block on the given edge
BasicBlock *
FlowGraph::InsertAirlockBlock(FlowEdge * edge)
{LOGMEIN("FlowGraph.cpp] 1491\n");
    BasicBlock * airlockBlock = BasicBlock::New(this);
    BasicBlock * sourceBlock = edge->GetPred();
    BasicBlock * sinkBlock = edge->GetSucc();

    BasicBlock * sinkPrevBlock = sinkBlock->prev;
    IR::Instr *  sinkPrevBlockLastInstr = sinkPrevBlock->GetLastInstr();
    IR::Instr * sourceLastInstr = sourceBlock->GetLastInstr();

    airlockBlock->loop = sinkBlock->loop;
    airlockBlock->SetBlockNum(this->blockCount++);
#ifdef DBG
    airlockBlock->isAirLockBlock = true;
#endif
    //
    // Fixup block linkage
    //

    // airlock block is inserted right before sourceBlock
    airlockBlock->prev = sinkBlock->prev;
    sinkBlock->prev = airlockBlock;

    airlockBlock->next = sinkBlock;
    airlockBlock->prev->next = airlockBlock;

    //
    // Fixup flow edges
    //

    sourceBlock->RemoveSucc(sinkBlock, this, false);

    // Add sourceBlock -> airlockBlock
    this->AddEdge(sourceBlock, airlockBlock);

    // Add airlockBlock -> sinkBlock
    edge->SetPred(airlockBlock);
    airlockBlock->AddSucc(edge, this);

    // Fixup data use count
    airlockBlock->SetDataUseCount(1);
    sourceBlock->DecrementDataUseCount();

    //
    // Fixup IR
    //

    // Maintain the instruction region for inlining
    IR::LabelInstr *sinkLabel = sinkBlock->GetFirstInstr()->AsLabelInstr();
    Func * sinkLabelFunc = sinkLabel->m_func;

    IR::LabelInstr *airlockLabel = IR::LabelInstr::New(Js::OpCode::Label, sinkLabelFunc);

    sinkPrevBlockLastInstr->InsertAfter(airlockLabel);

    airlockBlock->SetFirstInstr(airlockLabel);
    airlockLabel->SetBasicBlock(airlockBlock);

    // Add br to sinkBlock from airlock block
    IR::BranchInstr *airlockBr = IR::BranchInstr::New(Js::OpCode::Br, sinkLabel, sinkLabelFunc);
    airlockBr->SetByteCodeOffset(sinkLabel);
    airlockLabel->InsertAfter(airlockBr);
    airlockBlock->SetLastInstr(airlockBr);

    airlockLabel->SetByteCodeOffset(sinkLabel);

    // Fixup flow out of sourceBlock
    IR::BranchInstr *sourceBr = sourceLastInstr->AsBranchInstr();
    if (sourceBr->IsMultiBranch())
    {LOGMEIN("FlowGraph.cpp] 1559\n");
        const bool replaced = sourceBr->AsMultiBrInstr()->ReplaceTarget(sinkLabel, airlockLabel);
        Assert(replaced);
    }
    else if (sourceBr->GetTarget() == sinkLabel)
    {LOGMEIN("FlowGraph.cpp] 1564\n");
        sourceBr->SetTarget(airlockLabel);
    }

    if (!sinkPrevBlockLastInstr->IsBranchInstr() || sinkPrevBlockLastInstr->AsBranchInstr()->HasFallThrough())
    {LOGMEIN("FlowGraph.cpp] 1569\n");
        if (!sinkPrevBlock->isDeleted)
        {LOGMEIN("FlowGraph.cpp] 1571\n");
            FlowEdge *dstEdge = this->FindEdge(sinkPrevBlock, sinkBlock);
            if (dstEdge) // Possibility that sourceblock may be same as sinkPrevBlock
            {LOGMEIN("FlowGraph.cpp] 1574\n");
                BasicBlock* compensationBlock = this->InsertCompensationCodeForBlockMove(dstEdge, true /*insert comp block to loop list*/, true);
                compensationBlock->IncrementDataUseCount();
                // We need to skip airlock compensation block in globopt as its inserted while globopt is iteration over the blocks.
                compensationBlock->isAirLockCompensationBlock = true;
            }
        }
    }

#if DBG_DUMP
    this->Dump(true, _u("\n After insertion of airlock block \n"));
#endif

    return airlockBlock;
}

// Insert a block on the given edge
BasicBlock *
FlowGraph::InsertCompensationCodeForBlockMove(FlowEdge * edge,  bool insertToLoopList, bool sinkBlockLoop)
{LOGMEIN("FlowGraph.cpp] 1593\n");
    BasicBlock * compBlock = BasicBlock::New(this);
    BasicBlock * sourceBlock = edge->GetPred();
    BasicBlock * sinkBlock = edge->GetSucc();
    BasicBlock * fallthroughBlock = sourceBlock->next;
    IR::Instr *  sourceLastInstr = sourceBlock->GetLastInstr();

    compBlock->SetBlockNum(this->blockCount++);

    if (insertToLoopList)
    {LOGMEIN("FlowGraph.cpp] 1603\n");
        // For flow graph edits in
        if (sinkBlockLoop)
        {LOGMEIN("FlowGraph.cpp] 1606\n");
            if (sinkBlock->loop && sinkBlock->loop->GetHeadBlock() == sinkBlock)
            {LOGMEIN("FlowGraph.cpp] 1608\n");
                // BLUE 531255: sinkblock may be the head block of new loop, we shouldn't insert compensation block to that loop
                // Insert it to all the parent loop lists.
                compBlock->loop = sinkBlock->loop->parent;
                InsertCompBlockToLoopList(compBlock->loop, compBlock, sinkBlock, false);
            }
            else
            {
                compBlock->loop = sinkBlock->loop;
                InsertCompBlockToLoopList(compBlock->loop, compBlock, sinkBlock, false); // sinkBlock or fallthroughBlock?
            }
#ifdef DBG
            compBlock->isBreakCompensationBlockAtSink = true;
#endif
        }
        else
        {
            compBlock->loop = sourceBlock->loop;
            InsertCompBlockToLoopList(compBlock->loop, compBlock, sourceBlock, true);
#ifdef DBG
            compBlock->isBreakCompensationBlockAtSource = true;
#endif
        }
    }

    //
    // Fixup block linkage
    //

    // compensation block is inserted right after sourceBlock
    compBlock->next = fallthroughBlock;
    fallthroughBlock->prev = compBlock;

    compBlock->prev = sourceBlock;
    sourceBlock->next = compBlock;

    //
    // Fixup flow edges
    //
    sourceBlock->RemoveSucc(sinkBlock, this, false);

    // Add sourceBlock -> airlockBlock
    this->AddEdge(sourceBlock, compBlock);

    // Add airlockBlock -> sinkBlock
    edge->SetPred(compBlock);
    compBlock->AddSucc(edge, this);

    //
    // Fixup IR
    //

    // Maintain the instruction region for inlining
    IR::LabelInstr *sinkLabel = sinkBlock->GetFirstInstr()->AsLabelInstr();
    Func * sinkLabelFunc = sinkLabel->m_func;

    IR::LabelInstr *compLabel = IR::LabelInstr::New(Js::OpCode::Label, sinkLabelFunc);
    sourceLastInstr->InsertAfter(compLabel);
    compBlock->SetFirstInstr(compLabel);
    compLabel->SetBasicBlock(compBlock);

    // Add br to sinkBlock from compensation block
    IR::BranchInstr *compBr = IR::BranchInstr::New(Js::OpCode::Br, sinkLabel, sinkLabelFunc);
    compBr->SetByteCodeOffset(sinkLabel);
    compLabel->InsertAfter(compBr);
    compBlock->SetLastInstr(compBr);

    compLabel->SetByteCodeOffset(sinkLabel);

    // Fixup flow out of sourceBlock
    if (sourceLastInstr->IsBranchInstr())
    {LOGMEIN("FlowGraph.cpp] 1679\n");
        IR::BranchInstr *sourceBr = sourceLastInstr->AsBranchInstr();
        Assert(sourceBr->IsMultiBranch() || sourceBr->IsConditional());
        if (sourceBr->IsMultiBranch())
        {LOGMEIN("FlowGraph.cpp] 1683\n");
            const bool replaced = sourceBr->AsMultiBrInstr()->ReplaceTarget(sinkLabel, compLabel);
            Assert(replaced);
        }
    }

    return compBlock;
}

void
FlowGraph::RemoveUnreachableBlocks()
{LOGMEIN("FlowGraph.cpp] 1694\n");
    AnalysisAssert(this->blockList);

    FOREACH_BLOCK(block, this)
    {LOGMEIN("FlowGraph.cpp] 1698\n");
        block->isVisited = false;
    }
    NEXT_BLOCK;

    this->blockList->isVisited = true;

    FOREACH_BLOCK_EDITING(block, this)
    {LOGMEIN("FlowGraph.cpp] 1706\n");
        if (block->isVisited)
        {
            FOREACH_SUCCESSOR_BLOCK(succ, block)
            {LOGMEIN("FlowGraph.cpp] 1710\n");
                succ->isVisited = true;
            } NEXT_SUCCESSOR_BLOCK;
        }
        else
        {
            this->RemoveBlock(block);
        }
    }
    NEXT_BLOCK_EDITING;
}

// If block has no predecessor, remove it.
bool
FlowGraph::RemoveUnreachableBlock(BasicBlock *block, GlobOpt * globOpt)
{LOGMEIN("FlowGraph.cpp] 1725\n");
    bool isDead = false;

    if ((block->GetPredList() == nullptr || block->GetPredList()->Empty()) && block != this->func->m_fg->blockList)
    {LOGMEIN("FlowGraph.cpp] 1729\n");
        isDead = true;
    }
    else if (block->isLoopHeader)
    {LOGMEIN("FlowGraph.cpp] 1733\n");
        // A dead loop still has back-edges pointing to it...
        isDead = true;
        FOREACH_PREDECESSOR_BLOCK(pred, block)
        {LOGMEIN("FlowGraph.cpp] 1737\n");
            if (!block->loop->IsDescendentOrSelf(pred->loop))
            {LOGMEIN("FlowGraph.cpp] 1739\n");
                isDead = false;
            }
        } NEXT_PREDECESSOR_BLOCK;
    }

    if (isDead)
    {LOGMEIN("FlowGraph.cpp] 1746\n");
        this->RemoveBlock(block, globOpt);
        return true;
    }
    return false;
}

IR::Instr *
FlowGraph::PeepTypedCm(IR::Instr *instr)
{LOGMEIN("FlowGraph.cpp] 1755\n");
    // Basic pattern, peep:
    //      t1 = CmEq a, b
    //      BrTrue_I4 $L1, t1
    // Into:
    //      t1 = 1
    //      BrEq $L1, a, b
    //      t1 = 0

    IR::Instr * instrNext = instr->GetNextRealInstrOrLabel();

    // find intermediate Lds
    IR::Instr * instrLd = nullptr;
    if (instrNext->m_opcode == Js::OpCode::Ld_I4)
    {LOGMEIN("FlowGraph.cpp] 1769\n");
        instrLd = instrNext;
        instrNext = instrNext->GetNextRealInstrOrLabel();
    }

    IR::Instr * instrLd2 = nullptr;
    if (instrNext->m_opcode == Js::OpCode::Ld_I4)
    {LOGMEIN("FlowGraph.cpp] 1776\n");
        instrLd2 = instrNext;
        instrNext = instrNext->GetNextRealInstrOrLabel();
    }

    // Find BrTrue/BrFalse
    IR::Instr *instrBr;
    bool brIsTrue;
    if (instrNext->m_opcode == Js::OpCode::BrTrue_I4)
    {LOGMEIN("FlowGraph.cpp] 1785\n");
        instrBr = instrNext;
        brIsTrue = true;
    }
    else if (instrNext->m_opcode == Js::OpCode::BrFalse_I4)
    {LOGMEIN("FlowGraph.cpp] 1790\n");
        instrBr = instrNext;
        brIsTrue = false;
    }
    else
    {
        return nullptr;
    }

    AssertMsg(instrLd || (!instrLd && !instrLd2), "Either instrLd is non-null or both null");

    // if we have intermediate Lds, then make sure pattern is:
    //      t1 = CmEq a, b
    //      t2 = Ld_A t1
    //      BrTrue $L1, t2
    if (instrLd && !instrLd->GetSrc1()->IsEqual(instr->GetDst()))
    {LOGMEIN("FlowGraph.cpp] 1806\n");
        return nullptr;
    }

    if (instrLd2 && !instrLd2->GetSrc1()->IsEqual(instrLd->GetDst()))
    {LOGMEIN("FlowGraph.cpp] 1811\n");
        return nullptr;
    }

    // Make sure we have:
    //      t1 = CmEq a, b
    //           BrTrue/BrFalse t1
    if (!(instr->GetDst()->IsEqual(instrBr->GetSrc1()) || (instrLd && instrLd->GetDst()->IsEqual(instrBr->GetSrc1())) || (instrLd2 && instrLd2->GetDst()->IsEqual(instrBr->GetSrc1()))))
    {LOGMEIN("FlowGraph.cpp] 1819\n");
        return nullptr;
    }

    IR::Opnd * src1 = instr->UnlinkSrc1();
    IR::Opnd * src2 = instr->UnlinkSrc2();

    IR::Instr * instrNew;
    IR::Opnd * tmpOpnd;
    if (instr->GetDst()->IsEqual(src1) || (instrLd && instrLd->GetDst()->IsEqual(src1)) || (instrLd2 && instrLd2->GetDst()->IsEqual(src1)))
    {LOGMEIN("FlowGraph.cpp] 1829\n");
        Assert(src1->IsInt32());

        tmpOpnd = IR::RegOpnd::New(TyInt32, instr->m_func);
        instrNew = IR::Instr::New(Js::OpCode::Ld_I4, tmpOpnd, src1, instr->m_func);
        instrNew->SetByteCodeOffset(instr);
        instr->InsertBefore(instrNew);
        src1 = tmpOpnd;
    }

    if (instr->GetDst()->IsEqual(src2) || (instrLd && instrLd->GetDst()->IsEqual(src2)) || (instrLd2 && instrLd2->GetDst()->IsEqual(src2)))
    {LOGMEIN("FlowGraph.cpp] 1840\n");
        Assert(src2->IsInt32());

        tmpOpnd = IR::RegOpnd::New(TyInt32, instr->m_func);
        instrNew = IR::Instr::New(Js::OpCode::Ld_I4, tmpOpnd, src2, instr->m_func);
        instrNew->SetByteCodeOffset(instr);
        instr->InsertBefore(instrNew);
        src2 = tmpOpnd;
    }

    instrBr->ReplaceSrc1(src1);
    instrBr->SetSrc2(src2);

    Js::OpCode newOpcode;
    switch (instr->m_opcode)
    {LOGMEIN("FlowGraph.cpp] 1855\n");
    case Js::OpCode::CmEq_I4:
        newOpcode = Js::OpCode::BrEq_I4;
        break;
    case Js::OpCode::CmGe_I4:
        newOpcode = Js::OpCode::BrGe_I4;
        break;
    case Js::OpCode::CmGt_I4:
        newOpcode = Js::OpCode::BrGt_I4;
        break;
    case Js::OpCode::CmLt_I4:
        newOpcode = Js::OpCode::BrLt_I4;
        break;
    case Js::OpCode::CmLe_I4:
        newOpcode = Js::OpCode::BrLe_I4;
        break;
    case Js::OpCode::CmUnGe_I4:
        newOpcode = Js::OpCode::BrUnGe_I4;
        break;
    case Js::OpCode::CmUnGt_I4:
        newOpcode = Js::OpCode::BrUnGt_I4;
        break;
    case Js::OpCode::CmUnLt_I4:
        newOpcode = Js::OpCode::BrUnLt_I4;
        break;
    case Js::OpCode::CmUnLe_I4:
        newOpcode = Js::OpCode::BrUnLe_I4;
        break;
    case Js::OpCode::CmNeq_I4:
        newOpcode = Js::OpCode::BrNeq_I4;
        break;
    case Js::OpCode::CmEq_A:
        newOpcode = Js::OpCode::BrEq_A;
        break;
    case Js::OpCode::CmGe_A:
        newOpcode = Js::OpCode::BrGe_A;
        break;
    case Js::OpCode::CmGt_A:
        newOpcode = Js::OpCode::BrGt_A;
        break;
    case Js::OpCode::CmLt_A:
        newOpcode = Js::OpCode::BrLt_A;
        break;
    case Js::OpCode::CmLe_A:
        newOpcode = Js::OpCode::BrLe_A;
        break;
    case Js::OpCode::CmUnGe_A:
        newOpcode = Js::OpCode::BrUnGe_A;
        break;
    case Js::OpCode::CmUnGt_A:
        newOpcode = Js::OpCode::BrUnGt_A;
        break;
    case Js::OpCode::CmUnLt_A:
        newOpcode = Js::OpCode::BrUnLt_A;
        break;
    case Js::OpCode::CmUnLe_A:
        newOpcode = Js::OpCode::BrUnLe_A;
        break;
    case Js::OpCode::CmNeq_A:
        newOpcode = Js::OpCode::BrNeq_A;
        break;
    case Js::OpCode::CmSrEq_A:
        newOpcode = Js::OpCode::BrSrEq_A;
        break;
    case Js::OpCode::CmSrNeq_A:
        newOpcode = Js::OpCode::BrSrNeq_A;
        break;
    default:
        newOpcode = Js::OpCode::InvalidOpCode;
        Assume(UNREACHED);
    }

    instrBr->m_opcode = newOpcode;

    if (brIsTrue)
    {LOGMEIN("FlowGraph.cpp] 1930\n");
        instr->SetSrc1(IR::IntConstOpnd::New(1, TyInt8, instr->m_func));
        instr->m_opcode = Js::OpCode::Ld_I4;
        instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instr->GetDst(), IR::IntConstOpnd::New(0, TyInt8, instr->m_func), instr->m_func);
        instrNew->SetByteCodeOffset(instrBr);
        instrBr->InsertAfter(instrNew);
        if (instrLd)
        {LOGMEIN("FlowGraph.cpp] 1937\n");
            instrLd->ReplaceSrc1(IR::IntConstOpnd::New(1, TyInt8, instr->m_func));
            instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instrLd->GetDst(), IR::IntConstOpnd::New(0, TyInt8, instr->m_func), instr->m_func);
            instrNew->SetByteCodeOffset(instrBr);
            instrBr->InsertAfter(instrNew);

            if (instrLd2)
            {LOGMEIN("FlowGraph.cpp] 1944\n");
                instrLd2->ReplaceSrc1(IR::IntConstOpnd::New(1, TyInt8, instr->m_func));
                instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instrLd2->GetDst(), IR::IntConstOpnd::New(0, TyInt8, instr->m_func), instr->m_func);
                instrNew->SetByteCodeOffset(instrBr);
                instrBr->InsertAfter(instrNew);
            }
        }
    }
    else
    {
        instrBr->AsBranchInstr()->Invert();

        instr->SetSrc1(IR::IntConstOpnd::New(0, TyInt8, instr->m_func));
        instr->m_opcode = Js::OpCode::Ld_I4;
        instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instr->GetDst(), IR::IntConstOpnd::New(1, TyInt8, instr->m_func), instr->m_func);
        instrNew->SetByteCodeOffset(instrBr);
        instrBr->InsertAfter(instrNew);
        if (instrLd)
        {LOGMEIN("FlowGraph.cpp] 1962\n");
            instrLd->ReplaceSrc1(IR::IntConstOpnd::New(0, TyInt8, instr->m_func));
            instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instrLd->GetDst(), IR::IntConstOpnd::New(1, TyInt8, instr->m_func), instr->m_func);
            instrNew->SetByteCodeOffset(instrBr);
            instrBr->InsertAfter(instrNew);

            if (instrLd2)
            {LOGMEIN("FlowGraph.cpp] 1969\n");
                instrLd2->ReplaceSrc1(IR::IntConstOpnd::New(0, TyInt8, instr->m_func));
                instrNew = IR::Instr::New(Js::OpCode::Ld_I4, instrLd2->GetDst(), IR::IntConstOpnd::New(1, TyInt8, instr->m_func), instr->m_func);
                instrNew->SetByteCodeOffset(instrBr);
                instrBr->InsertAfter(instrNew);
            }
        }
    }

    return instrBr;
}

IR::Instr *
FlowGraph::PeepCm(IR::Instr *instr)
{LOGMEIN("FlowGraph.cpp] 1983\n");
    // Basic pattern, peep:
    //      t1 = CmEq a, b
    //      t2 = Ld_A t1
    //      BrTrue $L1, t2
    // Into:
    //      t1 = True
    //      t2 = True
    //      BrEq $L1, a, b
    //      t1 = False
    //      t2 = False
    //
    //  The true/false Ld_A's will most likely end up being dead-stores...

    //  Alternate Pattern
    //      t1= CmEq a, b
    //      BrTrue $L1, t1
    // Into:
    //      BrEq $L1, a, b

    Func *func = instr->m_func;

    // Find Ld_A
    IR::Instr *instrNext = instr->GetNextRealInstrOrLabel();
    IR::Instr *inlineeEndInstr = nullptr;
    IR::Instr *instrNew;
    IR::Instr *instrLd = nullptr, *instrLd2 = nullptr;
    IR::Instr *instrByteCode = instrNext;
    bool ldFound = false;
    IR::Opnd *brSrc = instr->GetDst();

    if (instrNext->m_opcode == Js::OpCode::Ld_A && instrNext->GetSrc1()->IsEqual(instr->GetDst()))
    {LOGMEIN("FlowGraph.cpp] 2015\n");
        ldFound = true;
        instrLd = instrNext;
        brSrc = instrNext->GetDst();

        if (brSrc->IsEqual(instr->GetSrc1()) || brSrc->IsEqual(instr->GetSrc2()))
        {LOGMEIN("FlowGraph.cpp] 2021\n");
            return nullptr;
        }

        instrNext = instrLd->GetNextRealInstrOrLabel();

        // Is there a second Ld_A?
        if (instrNext->m_opcode == Js::OpCode::Ld_A && instrNext->GetSrc1()->IsEqual(brSrc))
        {LOGMEIN("FlowGraph.cpp] 2029\n");
            // We have:
            //      t1 = Cm
            //      t2 = t1     // ldSrc = t1
            //      t3 = t2     // ldDst = t3
            //      BrTrue/BrFalse t3

            instrLd2 = instrNext;
            brSrc = instrLd2->GetDst();
            instrNext = instrLd2->GetNextRealInstrOrLabel();
            if (brSrc->IsEqual(instr->GetSrc1()) || brSrc->IsEqual(instr->GetSrc2()))
            {LOGMEIN("FlowGraph.cpp] 2040\n");
                return nullptr;
            }
        }
    }

    // Skip over InlineeEnd
    if (instrNext->m_opcode == Js::OpCode::InlineeEnd)
    {LOGMEIN("FlowGraph.cpp] 2048\n");
        inlineeEndInstr = instrNext;
        instrNext = inlineeEndInstr->GetNextRealInstrOrLabel();
    }

    // Find BrTrue/BrFalse
    bool brIsTrue;
    if (instrNext->m_opcode == Js::OpCode::BrTrue_A)
    {LOGMEIN("FlowGraph.cpp] 2056\n");
        brIsTrue = true;
    }
    else if (instrNext->m_opcode == Js::OpCode::BrFalse_A)
    {LOGMEIN("FlowGraph.cpp] 2060\n");
        brIsTrue = false;
    }
    else
    {
        return nullptr;
    }

    IR::Instr *instrBr = instrNext;

    // Make sure we have:
    //      t1 = Ld_A
    //         BrTrue/BrFalse t1
    if (!instr->GetDst()->IsEqual(instrBr->GetSrc1()) && !brSrc->IsEqual(instrBr->GetSrc1()))
    {LOGMEIN("FlowGraph.cpp] 2074\n");
        return nullptr;
    }

    //
    // We have a match.  Generate the new branch
    //

    // BrTrue/BrFalse t1
    // Keep a copy of the inliner func and the bytecode offset of the original BrTrue/BrFalse if we end up inserting a new branch out of the inlinee,
    // and sym id of t1 for proper restoration on a bailout before the branch.
    Func* origBrFunc = instrBr->m_func;
    uint32 origBrByteCodeOffset = instrBr->GetByteCodeOffset();
    uint32 origBranchSrcSymId = instrBr->GetSrc1()->GetStackSym()->m_id;
    bool origBranchSrcOpndIsJITOpt = instrBr->GetSrc1()->GetIsJITOptimizedReg();

    instrBr->Unlink();
    instr->InsertBefore(instrBr);
    instrBr->ClearByteCodeOffset();
    instrBr->SetByteCodeOffset(instr);
    instrBr->FreeSrc1();
    instrBr->SetSrc1(instr->UnlinkSrc1());
    instrBr->SetSrc2(instr->UnlinkSrc2());
    instrBr->m_func = instr->m_func;

    Js::OpCode newOpcode;

    switch(instr->m_opcode)
    {LOGMEIN("FlowGraph.cpp] 2102\n");
    case Js::OpCode::CmEq_A:
        newOpcode = Js::OpCode::BrEq_A;
        break;
    case Js::OpCode::CmGe_A:
        newOpcode = Js::OpCode::BrGe_A;
        break;
    case Js::OpCode::CmGt_A:
        newOpcode = Js::OpCode::BrGt_A;
        break;
    case Js::OpCode::CmLt_A:
        newOpcode = Js::OpCode::BrLt_A;
        break;
    case Js::OpCode::CmLe_A:
        newOpcode = Js::OpCode::BrLe_A;
        break;
    case Js::OpCode::CmUnGe_A:
        newOpcode = Js::OpCode::BrUnGe_A;
        break;
    case Js::OpCode::CmUnGt_A:
        newOpcode = Js::OpCode::BrUnGt_A;
        break;
    case Js::OpCode::CmUnLt_A:
        newOpcode = Js::OpCode::BrUnLt_A;
        break;
    case Js::OpCode::CmUnLe_A:
        newOpcode = Js::OpCode::BrUnLe_A;
        break;
    case Js::OpCode::CmNeq_A:
        newOpcode = Js::OpCode::BrNeq_A;
        break;
    case Js::OpCode::CmSrEq_A:
        newOpcode = Js::OpCode::BrSrEq_A;
        break;
    case Js::OpCode::CmSrNeq_A:
        newOpcode = Js::OpCode::BrSrNeq_A;
        break;
    default:
        Assert(UNREACHED);
        __assume(UNREACHED);
    }

    instrBr->m_opcode = newOpcode;

    IR::AddrOpnd* trueOpnd = IR::AddrOpnd::New(func->GetScriptContextInfo()->GetTrueAddr(), IR::AddrOpndKindDynamicVar, func, true);
    IR::AddrOpnd* falseOpnd = IR::AddrOpnd::New(func->GetScriptContextInfo()->GetFalseAddr(), IR::AddrOpndKindDynamicVar, func, true);

    trueOpnd->SetValueType(ValueType::Boolean);
    falseOpnd->SetValueType(ValueType::Boolean);

    if (ldFound)
    {LOGMEIN("FlowGraph.cpp] 2153\n");
        // Split Ld_A into "Ld_A TRUE"/"Ld_A FALSE"
        if (brIsTrue)
        {LOGMEIN("FlowGraph.cpp] 2156\n");
            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), trueOpnd, instrBr->m_func);
            instrNew->SetByteCodeOffset(instrBr);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            instrBr->InsertBefore(instrNew);
            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetDst(), trueOpnd, instrBr->m_func);
            instrNew->SetByteCodeOffset(instrBr);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            instrBr->InsertBefore(instrNew);

            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), falseOpnd, instrLd->m_func);
            instrLd->InsertBefore(instrNew);
            instrNew->SetByteCodeOffset(instrLd);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            instrLd->ReplaceSrc1(falseOpnd);

            if (instrLd2)
            {LOGMEIN("FlowGraph.cpp] 2173\n");
                instrLd2->ReplaceSrc1(falseOpnd);

                instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd2->GetDst(), trueOpnd, instrBr->m_func);
                instrBr->InsertBefore(instrNew);
                instrNew->SetByteCodeOffset(instrBr);
                instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            }
        }
        else
        {
            instrBr->AsBranchInstr()->Invert();

            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), falseOpnd, instrBr->m_func);
            instrBr->InsertBefore(instrNew);
            instrNew->SetByteCodeOffset(instrBr);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetDst(), falseOpnd, instrBr->m_func);
            instrBr->InsertBefore(instrNew);
            instrNew->SetByteCodeOffset(instrBr);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;

            instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), trueOpnd, instrLd->m_func);
            instrLd->InsertBefore(instrNew);
            instrNew->SetByteCodeOffset(instrLd);
            instrLd->ReplaceSrc1(trueOpnd);
            instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;

            if (instrLd2)
            {LOGMEIN("FlowGraph.cpp] 2202\n");
                instrLd2->ReplaceSrc1(trueOpnd);
                instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd->GetSrc1(), trueOpnd, instrBr->m_func);
                instrBr->InsertBefore(instrNew);
                instrNew->SetByteCodeOffset(instrBr);
                instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
            }
        }
    }

    // Fix InlineeEnd
    if (inlineeEndInstr)
    {LOGMEIN("FlowGraph.cpp] 2214\n");
        this->InsertInlineeOnFLowEdge(instrBr->AsBranchInstr(), inlineeEndInstr, instrByteCode , origBrFunc, origBrByteCodeOffset, origBranchSrcOpndIsJITOpt, origBranchSrcSymId);
    }

    if (instr->GetDst()->AsRegOpnd()->m_sym->HasByteCodeRegSlot())
    {LOGMEIN("FlowGraph.cpp] 2219\n");
        Assert(!instrBr->AsBranchInstr()->HasByteCodeReg());
        StackSym *dstSym = instr->GetDst()->AsRegOpnd()->m_sym;
        instrBr->AsBranchInstr()->SetByteCodeReg(dstSym->GetByteCodeRegSlot());
    }
    instr->Remove();

    //
    // Try optimizing through a second branch.
    // Peep:
    //
    //      t2 = True;
    //      BrTrue  $L1
    //      ...
    //   L1:
    //      t1 = Ld_A t2
    //      BrTrue  $L2
    //
    // Into:
    //      t2 = True;
    //      t1 = True;
    //      BrTrue  $L2 <---
    //      ...
    //   L1:
    //      t1 = Ld_A t2
    //      BrTrue  $L2
    //
    // This cleanup helps expose second level Cm peeps.

    IR::Instr *instrLd3 = instrBr->AsBranchInstr()->GetTarget()->GetNextRealInstrOrLabel();

    // Skip over branch to branch
    while (instrLd3->m_opcode == Js::OpCode::Br)
    {LOGMEIN("FlowGraph.cpp] 2252\n");
        instrLd3 = instrLd3->AsBranchInstr()->GetTarget()->GetNextRealInstrOrLabel();
    }

    // Find Ld_A
    if (instrLd3->m_opcode != Js::OpCode::Ld_A)
    {LOGMEIN("FlowGraph.cpp] 2258\n");
        return instrBr;
    }

    IR::Instr *instrBr2 = instrLd3->GetNextRealInstrOrLabel();
    IR::Instr *inlineeEndInstr2 = nullptr;

    // InlineeEnd?
    // REVIEW: Can we handle 2 inlineeEnds?
    if (instrBr2->m_opcode == Js::OpCode::InlineeEnd && !inlineeEndInstr)
    {LOGMEIN("FlowGraph.cpp] 2268\n");
        inlineeEndInstr2 = instrBr2;
        instrBr2 = instrBr2->GetNextRealInstrOrLabel();
    }

    // Find branch
    bool brIsTrue2;
    if (instrBr2->m_opcode == Js::OpCode::BrTrue_A)
    {LOGMEIN("FlowGraph.cpp] 2276\n");
        brIsTrue2 = true;
    }
    else if (instrBr2->m_opcode == Js::OpCode::BrFalse_A)
    {LOGMEIN("FlowGraph.cpp] 2280\n");
        brIsTrue2 = false;
    }
    else
    {
        return nullptr;
    }

    // Make sure Ld_A operates on the right tmps.
    if (!instrLd3->GetDst()->IsEqual(instrBr2->GetSrc1()) || !brSrc->IsEqual(instrLd3->GetSrc1()))
    {LOGMEIN("FlowGraph.cpp] 2290\n");
        return nullptr;
    }

    if (instrLd3->GetDst()->IsEqual(instrBr->GetSrc1()) || instrLd3->GetDst()->IsEqual(instrBr->GetSrc2()))
    {LOGMEIN("FlowGraph.cpp] 2295\n");
        return nullptr;
    }

    // Make sure that the reg we're assigning to is not live in the intervening instructions (if this is a forward branch).
    if (instrLd3->GetByteCodeOffset() > instrBr->GetByteCodeOffset())
    {LOGMEIN("FlowGraph.cpp] 2301\n");
        StackSym *symLd3 = instrLd3->GetDst()->AsRegOpnd()->m_sym;
        if (IR::Instr::FindRegUseInRange(symLd3, instrBr->m_next, instrLd3))
        {LOGMEIN("FlowGraph.cpp] 2304\n");
            return nullptr;
        }
    }

    //
    // We have a match!
    //

    if(inlineeEndInstr2)
    {LOGMEIN("FlowGraph.cpp] 2314\n");
        origBrFunc = instrBr2->m_func;
        origBrByteCodeOffset = instrBr2->GetByteCodeOffset();
        origBranchSrcSymId = instrBr2->GetSrc1()->GetStackSym()->m_id;
    }

    // Fix Ld_A
    if (brIsTrue)
    {LOGMEIN("FlowGraph.cpp] 2322\n");
        instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd3->GetDst(), trueOpnd, instrBr->m_func);
        instrBr->InsertBefore(instrNew);
        instrNew->SetByteCodeOffset(instrBr);
        instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
    }
    else
    {
        instrNew = IR::Instr::New(Js::OpCode::Ld_A, instrLd3->GetDst(), falseOpnd, instrBr->m_func);
        instrBr->InsertBefore(instrNew);
        instrNew->SetByteCodeOffset(instrBr);
        instrNew->GetDst()->AsRegOpnd()->m_fgPeepTmp = true;
    }

    IR::LabelInstr *brTarget2;

    // Retarget branch
    if (brIsTrue2 == brIsTrue)
    {LOGMEIN("FlowGraph.cpp] 2340\n");
        brTarget2 = instrBr2->AsBranchInstr()->GetTarget();
    }
    else
    {
        brTarget2 = IR::LabelInstr::New(Js::OpCode::Label, instrBr2->m_func);
        brTarget2->SetByteCodeOffset(instrBr2->m_next);
        instrBr2->InsertAfter(brTarget2);
    }

    instrBr->AsBranchInstr()->SetTarget(brTarget2);

    // InlineeEnd?
    if (inlineeEndInstr2)
    {LOGMEIN("FlowGraph.cpp] 2354\n");
        this->InsertInlineeOnFLowEdge(instrBr->AsBranchInstr(), inlineeEndInstr2, instrByteCode, origBrFunc, origBrByteCodeOffset, origBranchSrcOpndIsJITOpt, origBranchSrcSymId);
    }

    return instrBr;
}

void
FlowGraph::InsertInlineeOnFLowEdge(IR::BranchInstr *instrBr, IR::Instr *inlineeEndInstr, IR::Instr *instrBytecode, Func* origBrFunc, uint32 origByteCodeOffset, bool origBranchSrcOpndIsJITOpt, uint32 origBranchSrcSymId)
{LOGMEIN("FlowGraph.cpp] 2363\n");
    // Helper for PeepsCm code.
    //
    // We've skipped some InlineeEnd.  Globopt expects to see these
    // on all flow paths out of the inlinee.  Insert an InlineeEnd
    // on the new path:
    //      BrEq $L1, a, b
    // Becomes:
    //      BrNeq $L2, a, b
    //      InlineeEnd
    //      Br $L1
    //  L2:

    instrBr->AsBranchInstr()->Invert();

    IR::BranchInstr *newBr = IR::BranchInstr::New(Js::OpCode::Br, instrBr->AsBranchInstr()->GetTarget(), origBrFunc);
    newBr->SetByteCodeOffset(origByteCodeOffset);
    instrBr->InsertAfter(newBr);

    IR::LabelInstr *newLabel = IR::LabelInstr::New(Js::OpCode::Label, instrBr->m_func);
    newLabel->SetByteCodeOffset(instrBytecode);
    newBr->InsertAfter(newLabel);
    instrBr->AsBranchInstr()->SetTarget(newLabel);

    IR::Instr *newInlineeEnd = IR::Instr::New(Js::OpCode::InlineeEnd, inlineeEndInstr->m_func);
    newInlineeEnd->SetSrc1(inlineeEndInstr->GetSrc1());
    newInlineeEnd->SetSrc2(inlineeEndInstr->GetSrc2());
    newInlineeEnd->SetByteCodeOffset(instrBytecode);
    newInlineeEnd->SetIsCloned(true);  // Mark it as cloned - this is used later by the inlinee args optimization
    newBr->InsertBefore(newInlineeEnd);

    IR::ByteCodeUsesInstr * useOrigBranchSrcInstr = IR::ByteCodeUsesInstr::New(origBrFunc, origByteCodeOffset);
    useOrigBranchSrcInstr->SetRemovedOpndSymbol(origBranchSrcOpndIsJITOpt, origBranchSrcSymId);
    newBr->InsertBefore(useOrigBranchSrcInstr);

    uint newBrFnNumber = newBr->m_func->GetFunctionNumber();
    Assert(newBrFnNumber == origBrFunc->GetFunctionNumber());

    // The function numbers of the new branch and the inlineeEnd instruction should be different (ensuring that the new branch is not added in the inlinee but in the inliner).
    // Only case when they can be same is recursive calls - inlinee and inliner are the same function
    Assert(newBrFnNumber != inlineeEndInstr->m_func->GetFunctionNumber() ||
        newBrFnNumber == inlineeEndInstr->m_func->GetParentFunc()->GetFunctionNumber());
}

BasicBlock *
BasicBlock::New(FlowGraph * graph)
{LOGMEIN("FlowGraph.cpp] 2409\n");
    BasicBlock * block;

    block = JitAnew(graph->alloc, BasicBlock, graph->alloc, graph->GetFunc());

    return block;
}

void
BasicBlock::AddPred(FlowEdge * edge, FlowGraph * graph)
{LOGMEIN("FlowGraph.cpp] 2419\n");
    this->predList.Prepend(graph->alloc, edge);
}

void
BasicBlock::AddSucc(FlowEdge * edge, FlowGraph * graph)
{LOGMEIN("FlowGraph.cpp] 2425\n");
    this->succList.Prepend(graph->alloc, edge);
}

void
BasicBlock::RemovePred(BasicBlock *block, FlowGraph * graph)
{LOGMEIN("FlowGraph.cpp] 2431\n");
    this->RemovePred(block, graph, true, false);
}

void
BasicBlock::RemoveSucc(BasicBlock *block, FlowGraph * graph)
{LOGMEIN("FlowGraph.cpp] 2437\n");
    this->RemoveSucc(block, graph, true, false);
}

void
BasicBlock::RemoveDeadPred(BasicBlock *block, FlowGraph * graph)
{LOGMEIN("FlowGraph.cpp] 2443\n");
    this->RemovePred(block, graph, true, true);
}

void
BasicBlock::RemoveDeadSucc(BasicBlock *block, FlowGraph * graph)
{LOGMEIN("FlowGraph.cpp] 2449\n");
    this->RemoveSucc(block, graph, true, true);
}

void
BasicBlock::RemovePred(BasicBlock *block, FlowGraph * graph, bool doCleanSucc, bool moveToDead)
{
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, this->GetPredList(), iter)
    {LOGMEIN("FlowGraph.cpp] 2457\n");
        if (edge->GetPred() == block)
        {LOGMEIN("FlowGraph.cpp] 2459\n");
            BasicBlock *blockSucc = edge->GetSucc();
            if (moveToDead)
            {LOGMEIN("FlowGraph.cpp] 2462\n");
                iter.MoveCurrentTo(this->GetDeadPredList());

            }
            else
            {
                iter.RemoveCurrent(graph->alloc);
            }
            if (doCleanSucc)
            {LOGMEIN("FlowGraph.cpp] 2471\n");
                block->RemoveSucc(this, graph, false, moveToDead);
            }
            if (blockSucc->isLoopHeader && blockSucc->loop && blockSucc->GetPredList()->HasOne())
            {LOGMEIN("FlowGraph.cpp] 2475\n");
                Loop *loop = blockSucc->loop;
                loop->isDead = true;
            }
            return;
        }
    } NEXT_SLISTBASECOUNTED_ENTRY_EDITING;
    AssertMsg(UNREACHED, "Edge not found.");
}

void
BasicBlock::RemoveSucc(BasicBlock *block, FlowGraph * graph, bool doCleanPred, bool moveToDead)
{
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, this->GetSuccList(), iter)
    {LOGMEIN("FlowGraph.cpp] 2489\n");
        if (edge->GetSucc() == block)
        {LOGMEIN("FlowGraph.cpp] 2491\n");
            if (moveToDead)
            {LOGMEIN("FlowGraph.cpp] 2493\n");
                iter.MoveCurrentTo(this->GetDeadSuccList());
            }
            else
            {
                iter.RemoveCurrent(graph->alloc);
            }

            if (doCleanPred)
            {LOGMEIN("FlowGraph.cpp] 2502\n");
                block->RemovePred(this, graph, false, moveToDead);
            }

            if (block->isLoopHeader && block->loop && block->GetPredList()->HasOne())
            {LOGMEIN("FlowGraph.cpp] 2507\n");
                Loop *loop = block->loop;
                loop->isDead = true;
            }
            return;
        }
    } NEXT_SLISTBASECOUNTED_ENTRY_EDITING;
    AssertMsg(UNREACHED, "Edge not found.");
}

void
BasicBlock::UnlinkPred(BasicBlock *block)
{LOGMEIN("FlowGraph.cpp] 2519\n");
    this->UnlinkPred(block, true);
}

void
BasicBlock::UnlinkSucc(BasicBlock *block)
{LOGMEIN("FlowGraph.cpp] 2525\n");
    this->UnlinkSucc(block, true);
}

void
BasicBlock::UnlinkPred(BasicBlock *block, bool doCleanSucc)
{
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, this->GetPredList(), iter)
    {LOGMEIN("FlowGraph.cpp] 2533\n");
        if (edge->GetPred() == block)
        {LOGMEIN("FlowGraph.cpp] 2535\n");
            iter.UnlinkCurrent();
            if (doCleanSucc)
            {LOGMEIN("FlowGraph.cpp] 2538\n");
                block->UnlinkSucc(this, false);
            }
            return;
        }
    } NEXT_SLISTBASECOUNTED_ENTRY_EDITING;
    AssertMsg(UNREACHED, "Edge not found.");
}

void
BasicBlock::UnlinkSucc(BasicBlock *block, bool doCleanPred)
{
    FOREACH_SLISTBASECOUNTED_ENTRY_EDITING(FlowEdge*, edge, this->GetSuccList(), iter)
    {LOGMEIN("FlowGraph.cpp] 2551\n");
        if (edge->GetSucc() == block)
        {LOGMEIN("FlowGraph.cpp] 2553\n");
            iter.UnlinkCurrent();
            if (doCleanPred)
            {LOGMEIN("FlowGraph.cpp] 2556\n");
                block->UnlinkPred(this, false);
            }
            return;
        }
    } NEXT_SLISTBASECOUNTED_ENTRY_EDITING;
    AssertMsg(UNREACHED, "Edge not found.");
}

bool
BasicBlock::IsLandingPad()
{LOGMEIN("FlowGraph.cpp] 2567\n");
    BasicBlock * nextBlock = this->GetNext();
    return nextBlock && nextBlock->loop && nextBlock->isLoopHeader && nextBlock->loop->landingPad == this;
}

IR::Instr *
FlowGraph::RemoveInstr(IR::Instr *instr, GlobOpt * globOpt)
{LOGMEIN("FlowGraph.cpp] 2574\n");
    IR::Instr *instrPrev = instr->m_prev;
    if (globOpt)
    {LOGMEIN("FlowGraph.cpp] 2577\n");
        // Removing block during glob opt.  Need to maintain the graph so that
        // bailout will record the byte code use in case the dead code is exposed
        // by dyno-pogo optimization (where bailout need the byte code uses from
        // the dead blocks where it may not be dead after bailing out)
        if (instr->IsLabelInstr())
        {LOGMEIN("FlowGraph.cpp] 2583\n");
            instr->AsLabelInstr()->m_isLoopTop = false;
            return instr;
        }
        else if (instr->IsByteCodeUsesInstr())
        {LOGMEIN("FlowGraph.cpp] 2588\n");
            return instr;
        }

        /*
        *   Scope object has to be implicitly live whenever Heap Arguments object is live.
        *       - When we restore HeapArguments object in the bail out path, it expects the scope object also to be restored - if one was created.
        */
        Js::OpCode opcode = instr->m_opcode;
        if (opcode == Js::OpCode::LdElemI_A && instr->DoStackArgsOpt(this->func) &&
            globOpt->IsArgumentsOpnd(instr->GetSrc1()) && instr->m_func->GetScopeObjSym())
        {LOGMEIN("FlowGraph.cpp] 2599\n");
            IR::ByteCodeUsesInstr * byteCodeUsesInstr = IR::ByteCodeUsesInstr::New(instr);
            byteCodeUsesInstr->SetNonOpndSymbol(instr->m_func->GetScopeObjSym()->m_id);
            instr->InsertAfter(byteCodeUsesInstr);
        }

        IR::ByteCodeUsesInstr * newByteCodeUseInstr = globOpt->ConvertToByteCodeUses(instr);
        if (newByteCodeUseInstr != nullptr)
        {LOGMEIN("FlowGraph.cpp] 2607\n");
            // We don't care about property used in these instruction
            // It is only necessary for field copy prop so that we will keep the implicit call
            // up to the copy prop location.
            newByteCodeUseInstr->propertySymUse = nullptr;

            if (opcode == Js::OpCode::Yield)
            {LOGMEIN("FlowGraph.cpp] 2614\n");
                IR::Instr *instrLabel = newByteCodeUseInstr->m_next;
                while (instrLabel->m_opcode != Js::OpCode::Label)
                {LOGMEIN("FlowGraph.cpp] 2617\n");
                    instrLabel = instrLabel->m_next;
                }
                func->RemoveDeadYieldOffsetResumeLabel(instrLabel->AsLabelInstr());
                instrLabel->AsLabelInstr()->m_hasNonBranchRef = false;
            }

            // Save the last instruction to update the block with
            return newByteCodeUseInstr;
        }
        else
        {
            return instrPrev;
        }
    }
    else
    {
        instr->Remove();
        return instrPrev;
    }
}

void
FlowGraph::RemoveBlock(BasicBlock *block, GlobOpt * globOpt, bool tailDuping)
{LOGMEIN("FlowGraph.cpp] 2641\n");
    Assert(!block->isDead && !block->isDeleted);
    IR::Instr * lastInstr = nullptr;
    FOREACH_INSTR_IN_BLOCK_EDITING(instr, instrNext, block)
    {LOGMEIN("FlowGraph.cpp] 2645\n");
        if (instr->m_opcode == Js::OpCode::FunctionExit)
        {LOGMEIN("FlowGraph.cpp] 2647\n");
            // Removing FunctionExit causes problems downstream...
            // We could change the opcode, or have FunctionEpilog/FunctionExit to get
            // rid of the epilog.
            break;
        }
        if (instr == block->GetFirstInstr())
        {LOGMEIN("FlowGraph.cpp] 2654\n");
            Assert(instr->IsLabelInstr());
            instr->AsLabelInstr()->m_isLoopTop = false;
        }
        else
        {
            lastInstr = this->RemoveInstr(instr, globOpt);
        }
    } NEXT_INSTR_IN_BLOCK_EDITING;

    if (lastInstr)
    {LOGMEIN("FlowGraph.cpp] 2665\n");
        block->SetLastInstr(lastInstr);
    }
    FOREACH_SLISTBASECOUNTED_ENTRY(FlowEdge*, edge, block->GetPredList())
    {LOGMEIN("FlowGraph.cpp] 2669\n");
        edge->GetPred()->RemoveSucc(block, this, false, globOpt != nullptr);
    } NEXT_SLISTBASECOUNTED_ENTRY;

    FOREACH_SLISTBASECOUNTED_ENTRY(FlowEdge*, edge, block->GetSuccList())
    {LOGMEIN("FlowGraph.cpp] 2674\n");
        edge->GetSucc()->RemovePred(block, this, false, globOpt != nullptr);
    } NEXT_SLISTBASECOUNTED_ENTRY;

    if (block->isLoopHeader && this->loopList)
    {LOGMEIN("FlowGraph.cpp] 2679\n");
        // If loop graph is built, remove loop from loopList
        Loop **pPrevLoop = &this->loopList;

        while (*pPrevLoop != block->loop)
        {LOGMEIN("FlowGraph.cpp] 2684\n");
            pPrevLoop = &((*pPrevLoop)->next);
        }
        *pPrevLoop = (*pPrevLoop)->next;
        this->hasLoop = (this->loopList != nullptr);
    }
    if (globOpt != nullptr)
    {LOGMEIN("FlowGraph.cpp] 2691\n");
        block->isDead = true;
        block->GetPredList()->MoveTo(block->GetDeadPredList());
        block->GetSuccList()->MoveTo(block->GetDeadSuccList());
    }
    if (tailDuping)
    {LOGMEIN("FlowGraph.cpp] 2697\n");
        block->isDead = true;
    }
    block->isDeleted = true;
    block->SetDataUseCount(0);
}

void
BasicBlock::UnlinkInstr(IR::Instr * instr)
{LOGMEIN("FlowGraph.cpp] 2706\n");
    Assert(this->Contains(instr));
    Assert(this->GetFirstInstr() != this->GetLastInstr());
    if (instr == this->GetFirstInstr())
    {LOGMEIN("FlowGraph.cpp] 2710\n");
        Assert(!this->GetFirstInstr()->IsLabelInstr());
        this->SetFirstInstr(instr->m_next);
    }
    else if (instr == this->GetLastInstr())
    {LOGMEIN("FlowGraph.cpp] 2715\n");
        this->SetLastInstr(instr->m_prev);
    }

    instr->Unlink();
}

void
BasicBlock::RemoveInstr(IR::Instr * instr)
{LOGMEIN("FlowGraph.cpp] 2724\n");
    Assert(this->Contains(instr));
    if (instr == this->GetFirstInstr())
    {LOGMEIN("FlowGraph.cpp] 2727\n");
        this->SetFirstInstr(instr->m_next);
    }
    else if (instr == this->GetLastInstr())
    {LOGMEIN("FlowGraph.cpp] 2731\n");
        this->SetLastInstr(instr->m_prev);
    }

    instr->Remove();
}

void
BasicBlock::InsertInstrBefore(IR::Instr *newInstr, IR::Instr *beforeThisInstr)
{LOGMEIN("FlowGraph.cpp] 2740\n");
    Assert(this->Contains(beforeThisInstr));
    beforeThisInstr->InsertBefore(newInstr);

    if(this->GetFirstInstr() == beforeThisInstr)
    {LOGMEIN("FlowGraph.cpp] 2745\n");
        Assert(!beforeThisInstr->IsLabelInstr());
        this->SetFirstInstr(newInstr);
    }
}

void
BasicBlock::InsertInstrAfter(IR::Instr *newInstr, IR::Instr *afterThisInstr)
{LOGMEIN("FlowGraph.cpp] 2753\n");
    Assert(this->Contains(afterThisInstr));
    afterThisInstr->InsertAfter(newInstr);

    if (this->GetLastInstr() == afterThisInstr)
    {LOGMEIN("FlowGraph.cpp] 2758\n");
        Assert(afterThisInstr->HasFallThrough());
        this->SetLastInstr(newInstr);
    }
}

void
BasicBlock::InsertAfter(IR::Instr *newInstr)
{LOGMEIN("FlowGraph.cpp] 2766\n");
    Assert(this->GetLastInstr()->HasFallThrough());
    this->GetLastInstr()->InsertAfter(newInstr);
    this->SetLastInstr(newInstr);
}

void
Loop::SetHasCall()
{LOGMEIN("FlowGraph.cpp] 2774\n");
    Loop * current = this;
    do
    {LOGMEIN("FlowGraph.cpp] 2777\n");
        if (current->hasCall)
        {LOGMEIN("FlowGraph.cpp] 2779\n");
#if DBG
            current = current->parent;
            while (current)
            {LOGMEIN("FlowGraph.cpp] 2783\n");
                Assert(current->hasCall);
                current = current->parent;
            }
#endif
            break;
        }
        current->hasCall = true;
        current = current->parent;
    }
    while (current != nullptr);
}

void
Loop::SetImplicitCallFlags(Js::ImplicitCallFlags newFlags)
{LOGMEIN("FlowGraph.cpp] 2798\n");
    Loop * current = this;
    do
    {LOGMEIN("FlowGraph.cpp] 2801\n");
        if ((current->implicitCallFlags & newFlags) == newFlags)
        {LOGMEIN("FlowGraph.cpp] 2803\n");
#if DBG
            current = current->parent;
            while (current)
            {LOGMEIN("FlowGraph.cpp] 2807\n");
                Assert((current->implicitCallFlags & newFlags) == newFlags);
                current = current->parent;
            }
#endif
            break;
        }
        newFlags = (Js::ImplicitCallFlags)(implicitCallFlags | newFlags);
        current->implicitCallFlags = newFlags;
        current = current->parent;
    }
    while (current != nullptr);
}

Js::ImplicitCallFlags
Loop::GetImplicitCallFlags()
{LOGMEIN("FlowGraph.cpp] 2823\n");
    if (this->implicitCallFlags == Js::ImplicitCall_HasNoInfo)
    {LOGMEIN("FlowGraph.cpp] 2825\n");
        if (this->parent == nullptr)
        {LOGMEIN("FlowGraph.cpp] 2827\n");
            // We don't have any information, and we don't have any parent, so just assume that there aren't any implicit calls
            this->implicitCallFlags = Js::ImplicitCall_None;
        }
        else
        {
            // We don't have any information, get it from the parent and hope for the best
            this->implicitCallFlags = this->parent->GetImplicitCallFlags();
        }
    }
    return this->implicitCallFlags;
}

bool
Loop::CanDoFieldCopyProp()
{LOGMEIN("FlowGraph.cpp] 2842\n");
#if DBG_DUMP
    if (((this->implicitCallFlags & ~(Js::ImplicitCall_External)) == 0) &&
        Js::Configuration::Global.flags.Trace.IsEnabled(Js::HostOptPhase))
    {LOGMEIN("FlowGraph.cpp] 2846\n");
        Output::Print(_u("fieldcopyprop disabled because external: loop count: %d"), GetLoopNumber());
        GetFunc()->DumpFullFunctionName();
        Output::Print(_u("\n"));
        Output::Flush();
    }
#endif
    return GlobOpt::ImplicitCallFlagsAllowOpts(this);
}

bool
Loop::CanDoFieldHoist()
{LOGMEIN("FlowGraph.cpp] 2858\n");
    // We can do field hoist wherever we can do copy prop
    return CanDoFieldCopyProp();
}

bool
Loop::CanHoistInvariants()
{LOGMEIN("FlowGraph.cpp] 2865\n");
    Func * func = this->GetHeadBlock()->firstInstr->m_func->GetTopFunc();

    if (PHASE_OFF(Js::InvariantsPhase, func))
    {LOGMEIN("FlowGraph.cpp] 2869\n");
        return false;
    }

    return true;
}

IR::LabelInstr *
Loop::GetLoopTopInstr() const
{LOGMEIN("FlowGraph.cpp] 2878\n");
    IR::LabelInstr * instr = nullptr;
    if (this->topFunc->isFlowGraphValid)
    {LOGMEIN("FlowGraph.cpp] 2881\n");
        instr = this->GetHeadBlock()->GetFirstInstr()->AsLabelInstr();
    }
    else
    {
        // Flowgraph gets torn down after the globopt, so can't get the loopTop from the head block.
        instr = this->loopTopLabel;
    }
    if (instr)
    {LOGMEIN("FlowGraph.cpp] 2890\n");
        Assert(instr->m_isLoopTop);
    }
    return instr;
}

void
Loop::SetLoopTopInstr(IR::LabelInstr * loopTop)
{LOGMEIN("FlowGraph.cpp] 2898\n");
    this->loopTopLabel = loopTop;
}

#if DBG_DUMP
uint
Loop::GetLoopNumber() const
{LOGMEIN("FlowGraph.cpp] 2905\n");
    IR::LabelInstr * loopTopInstr = this->GetLoopTopInstr();
    if (loopTopInstr->IsProfiledLabelInstr())
    {LOGMEIN("FlowGraph.cpp] 2908\n");
        return loopTopInstr->AsProfiledLabelInstr()->loopNum;
    }
    return Js::LoopHeader::NoLoop;
}

bool
BasicBlock::Contains(IR::Instr * instr)
{
    FOREACH_INSTR_IN_BLOCK(blockInstr, this)
    {LOGMEIN("FlowGraph.cpp] 2918\n");
        if (instr == blockInstr)
        {LOGMEIN("FlowGraph.cpp] 2920\n");
            return true;
        }
    }
    NEXT_INSTR_IN_BLOCK;
    return false;
}
#endif

FlowEdge *
FlowEdge::New(FlowGraph * graph)
{LOGMEIN("FlowGraph.cpp] 2931\n");
    FlowEdge * edge;

    edge = JitAnew(graph->alloc, FlowEdge);

    return edge;
}

bool
Loop::IsDescendentOrSelf(Loop const * loop) const
{LOGMEIN("FlowGraph.cpp] 2941\n");
    Loop const * currentLoop = loop;
    while (currentLoop != nullptr)
    {LOGMEIN("FlowGraph.cpp] 2944\n");
        if (currentLoop == this)
        {LOGMEIN("FlowGraph.cpp] 2946\n");
            return true;
        }
        currentLoop = currentLoop->parent;
    }
    return false;
}

void FlowGraph::SafeRemoveInstr(IR::Instr *instr)
{LOGMEIN("FlowGraph.cpp] 2955\n");
    BasicBlock *block;

    if (instr->m_next->IsLabelInstr())
    {LOGMEIN("FlowGraph.cpp] 2959\n");
        block = instr->m_next->AsLabelInstr()->GetBasicBlock()->GetPrev();
        block->RemoveInstr(instr);
    }
    else if (instr->IsLabelInstr())
    {LOGMEIN("FlowGraph.cpp] 2964\n");
        block = instr->AsLabelInstr()->GetBasicBlock();
        block->RemoveInstr(instr);
    }
    else
    {
        Assert(!instr->EndsBasicBlock() && !instr->StartsBasicBlock());
        instr->Remove();
    }
}

bool FlowGraph::IsUnsignedOpnd(IR::Opnd *src, IR::Opnd **pShrSrc1)
{LOGMEIN("FlowGraph.cpp] 2976\n");
    // Look for an unsigned constant, or the result of an unsigned shift by zero
    if (!src->IsRegOpnd())
    {LOGMEIN("FlowGraph.cpp] 2979\n");
        return false;
    }
    if (!src->AsRegOpnd()->m_sym->IsSingleDef())
    {LOGMEIN("FlowGraph.cpp] 2983\n");
        return false;
    }

    if (src->AsRegOpnd()->m_sym->IsIntConst())
    {LOGMEIN("FlowGraph.cpp] 2988\n");
        int32 intConst = src->AsRegOpnd()->m_sym->GetIntConstValue();

        if (intConst >= 0)
        {LOGMEIN("FlowGraph.cpp] 2992\n");
            *pShrSrc1 = src;
            return true;
        }
        else
        {
            return false;
        }
    }

    IR::Instr * shrUInstr = src->AsRegOpnd()->m_sym->GetInstrDef();

    if (shrUInstr->m_opcode != Js::OpCode::ShrU_A)
    {LOGMEIN("FlowGraph.cpp] 3005\n");
        return false;
    }

    IR::Opnd *shrCnt = shrUInstr->GetSrc2();

    if (!shrCnt->IsRegOpnd() || !shrCnt->AsRegOpnd()->m_sym->IsTaggableIntConst() || shrCnt->AsRegOpnd()->m_sym->GetIntConstValue() != 0)
    {LOGMEIN("FlowGraph.cpp] 3012\n");
        return false;
    }

    *pShrSrc1 = shrUInstr->GetSrc1();
    return true;
}

bool FlowGraph::UnsignedCmpPeep(IR::Instr *cmpInstr)
{LOGMEIN("FlowGraph.cpp] 3021\n");
    IR::Opnd *cmpSrc1 = cmpInstr->GetSrc1();
    IR::Opnd *cmpSrc2 = cmpInstr->GetSrc2();
    IR::Opnd *newSrc1;
    IR::Opnd *newSrc2;

    // Look for something like:
    //  t1 = ShrU_A x, 0
    //  t2 = 10;
    //  BrGt t1, t2, L
    //
    // Peep to:
    //
    //  t1 = ShrU_A x, 0
    //  t2 = 10;
    //  BrUnGt x, t2, L
    //       ByteCodeUse t1
    //
    // Hopefully dead-store can get rid of the ShrU

    if (!this->func->DoGlobOpt() || !GlobOpt::DoAggressiveIntTypeSpec(this->func) || !GlobOpt::DoLossyIntTypeSpec(this->func))
    {LOGMEIN("FlowGraph.cpp] 3042\n");
        return false;
    }

    if (cmpInstr->IsBranchInstr() && !cmpInstr->AsBranchInstr()->IsConditional())
    {LOGMEIN("FlowGraph.cpp] 3047\n");
        return false;
    }

    if (!cmpInstr->GetSrc2())
    {LOGMEIN("FlowGraph.cpp] 3052\n");
        return false;
    }

    if (!this->IsUnsignedOpnd(cmpSrc1, &newSrc1))
    {LOGMEIN("FlowGraph.cpp] 3057\n");
        return false;
    }
    if (!this->IsUnsignedOpnd(cmpSrc2, &newSrc2))
    {LOGMEIN("FlowGraph.cpp] 3061\n");
        return false;
    }

    switch(cmpInstr->m_opcode)
    {LOGMEIN("FlowGraph.cpp] 3066\n");
    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrSrNeq_A:
        break;
    case Js::OpCode::BrLe_A:
        cmpInstr->m_opcode = Js::OpCode::BrUnLe_A;
        break;
    case Js::OpCode::BrLt_A:
        cmpInstr->m_opcode = Js::OpCode::BrUnLt_A;
        break;
    case Js::OpCode::BrGe_A:
        cmpInstr->m_opcode = Js::OpCode::BrUnGe_A;
        break;
    case Js::OpCode::BrGt_A:
        cmpInstr->m_opcode = Js::OpCode::BrUnGt_A;
        break;
    case Js::OpCode::CmLe_A:
        cmpInstr->m_opcode = Js::OpCode::CmUnLe_A;
        break;
    case Js::OpCode::CmLt_A:
        cmpInstr->m_opcode = Js::OpCode::CmUnLt_A;
        break;
    case Js::OpCode::CmGe_A:
        cmpInstr->m_opcode = Js::OpCode::CmUnGe_A;
        break;
    case Js::OpCode::CmGt_A:
        cmpInstr->m_opcode = Js::OpCode::CmUnGt_A;
        break;

    default:
        return false;
    }

    IR::ByteCodeUsesInstr * bytecodeInstr = IR::ByteCodeUsesInstr::New(cmpInstr);
    cmpInstr->InsertAfter(bytecodeInstr);

    if (cmpSrc1 != newSrc1)
    {LOGMEIN("FlowGraph.cpp] 3105\n");
        if (cmpSrc1->IsRegOpnd() && !cmpSrc1->GetIsJITOptimizedReg())
        {LOGMEIN("FlowGraph.cpp] 3107\n");
            bytecodeInstr->Set(cmpSrc1);
        }
        cmpInstr->ReplaceSrc1(newSrc1);
        if (newSrc1->IsRegOpnd())
        {LOGMEIN("FlowGraph.cpp] 3112\n");
            cmpInstr->GetSrc1()->AsRegOpnd()->SetIsJITOptimizedReg(true);
        }
    }
    if (cmpSrc2 != newSrc2)
    {LOGMEIN("FlowGraph.cpp] 3117\n");
        if (cmpSrc2->IsRegOpnd() && !cmpSrc2->GetIsJITOptimizedReg())
        {LOGMEIN("FlowGraph.cpp] 3119\n");
            bytecodeInstr->Set(cmpSrc2);
        }
        cmpInstr->ReplaceSrc2(newSrc2);
        if (newSrc2->IsRegOpnd())
        {LOGMEIN("FlowGraph.cpp] 3124\n");
            cmpInstr->GetSrc2()->AsRegOpnd()->SetIsJITOptimizedReg(true);
        }
    }

    return true;
}


#if DBG

void
FlowGraph::VerifyLoopGraph()
{
    FOREACH_BLOCK(block, this)
    {LOGMEIN("FlowGraph.cpp] 3139\n");
        Loop *loop = block->loop;
        FOREACH_SUCCESSOR_BLOCK(succ, block)
        {LOGMEIN("FlowGraph.cpp] 3142\n");
            if (loop == succ->loop)
            {LOGMEIN("FlowGraph.cpp] 3144\n");
                Assert(succ->isLoopHeader == false || loop->GetHeadBlock() == succ);
                continue;
            }
            if (succ->isLoopHeader)
            {LOGMEIN("FlowGraph.cpp] 3149\n");
                Assert(succ->loop->parent == loop
                    || (!loop->IsDescendentOrSelf(succ->loop)));
                continue;
            }
            Assert(succ->loop == nullptr || succ->loop->IsDescendentOrSelf(loop));
        } NEXT_SUCCESSOR_BLOCK;

        if (!PHASE_OFF(Js::RemoveBreakBlockPhase, this->GetFunc()))
        {LOGMEIN("FlowGraph.cpp] 3158\n");
            // Make sure all break blocks have been removed.
            if (loop && !block->isLoopHeader && !(this->func->HasTry() && !this->func->DoOptimizeTryCatch()))
            {LOGMEIN("FlowGraph.cpp] 3161\n");
                Assert(loop->IsDescendentOrSelf(block->GetPrev()->loop));
            }
        }
    } NEXT_BLOCK;
}

#endif

#if DBG_DUMP

void
FlowGraph::Dump(bool onlyOnVerboseMode, const char16 *form)
{LOGMEIN("FlowGraph.cpp] 3174\n");
    if(PHASE_DUMP(Js::FGBuildPhase, this->GetFunc()))
    {LOGMEIN("FlowGraph.cpp] 3176\n");
        if (!onlyOnVerboseMode || Js::Configuration::Global.flags.Verbose)
        {LOGMEIN("FlowGraph.cpp] 3178\n");
            if (form)
            {LOGMEIN("FlowGraph.cpp] 3180\n");
                Output::Print(form);
            }
            this->Dump();
        }
    }
}

void
FlowGraph::Dump()
{LOGMEIN("FlowGraph.cpp] 3190\n");
    Output::Print(_u("\nFlowGraph\n"));
    FOREACH_BLOCK(block, this)
    {LOGMEIN("FlowGraph.cpp] 3193\n");
        Loop * loop = block->loop;
        while (loop)
        {LOGMEIN("FlowGraph.cpp] 3196\n");
            Output::Print(_u("    "));
            loop = loop->parent;
        }
        block->DumpHeader(false);
    } NEXT_BLOCK;

    Output::Print(_u("\nLoopGraph\n"));

    for (Loop *loop = this->loopList; loop; loop = loop->next)
    {LOGMEIN("FlowGraph.cpp] 3206\n");
        Output::Print(_u("\nLoop\n"));
        FOREACH_BLOCK_IN_LOOP(block, loop)
        {LOGMEIN("FlowGraph.cpp] 3209\n");
            block->DumpHeader(false);
        }NEXT_BLOCK_IN_LOOP;
        Output::Print(_u("Loop  Ends\n"));
    }
}

void
BasicBlock::DumpHeader(bool insertCR)
{LOGMEIN("FlowGraph.cpp] 3218\n");
    if (insertCR)
    {LOGMEIN("FlowGraph.cpp] 3220\n");
        Output::Print(_u("\n"));
    }
    Output::Print(_u("BLOCK %d:"), this->number);

    if (this->isDead)
    {LOGMEIN("FlowGraph.cpp] 3226\n");
        Output::Print(_u(" **** DEAD ****"));
    }

    if (this->isBreakBlock)
    {LOGMEIN("FlowGraph.cpp] 3231\n");
        Output::Print(_u(" **** Break Block ****"));
    }
    else if (this->isAirLockBlock)
    {LOGMEIN("FlowGraph.cpp] 3235\n");
        Output::Print(_u(" **** Air lock Block ****"));
    }
    else if (this->isBreakCompensationBlockAtSource)
    {LOGMEIN("FlowGraph.cpp] 3239\n");
        Output::Print(_u(" **** Break Source Compensation Code ****"));
    }
    else if (this->isBreakCompensationBlockAtSink)
    {LOGMEIN("FlowGraph.cpp] 3243\n");
        Output::Print(_u(" **** Break Sink Compensation Code ****"));
    }
    else if (this->isAirLockCompensationBlock)
    {LOGMEIN("FlowGraph.cpp] 3247\n");
        Output::Print(_u(" **** Airlock block Compensation Code ****"));
    }

    if (!this->predList.Empty())
    {LOGMEIN("FlowGraph.cpp] 3252\n");
        BOOL fFirst = TRUE;
        Output::Print(_u(" In("));
        FOREACH_PREDECESSOR_BLOCK(blockPred, this)
        {LOGMEIN("FlowGraph.cpp] 3256\n");
            if (!fFirst)
            {LOGMEIN("FlowGraph.cpp] 3258\n");
                Output::Print(_u(", "));
            }
            Output::Print(_u("%d"), blockPred->GetBlockNum());
            fFirst = FALSE;
        }
        NEXT_PREDECESSOR_BLOCK;
        Output::Print(_u(")"));
    }


    if (!this->succList.Empty())
    {LOGMEIN("FlowGraph.cpp] 3270\n");
        BOOL fFirst = TRUE;
        Output::Print(_u(" Out("));
        FOREACH_SUCCESSOR_BLOCK(blockSucc, this)
        {LOGMEIN("FlowGraph.cpp] 3274\n");
            if (!fFirst)
            {LOGMEIN("FlowGraph.cpp] 3276\n");
                Output::Print(_u(", "));
            }
            Output::Print(_u("%d"), blockSucc->GetBlockNum());
            fFirst = FALSE;
        }
        NEXT_SUCCESSOR_BLOCK;
        Output::Print(_u(")"));
    }

    if (!this->deadPredList.Empty())
    {LOGMEIN("FlowGraph.cpp] 3287\n");
        BOOL fFirst = TRUE;
        Output::Print(_u(" DeadIn("));
        FOREACH_DEAD_PREDECESSOR_BLOCK(blockPred, this)
        {LOGMEIN("FlowGraph.cpp] 3291\n");
            if (!fFirst)
            {LOGMEIN("FlowGraph.cpp] 3293\n");
                Output::Print(_u(", "));
            }
            Output::Print(_u("%d"), blockPred->GetBlockNum());
            fFirst = FALSE;
        }
        NEXT_DEAD_PREDECESSOR_BLOCK;
        Output::Print(_u(")"));
    }

    if (!this->deadSuccList.Empty())
    {LOGMEIN("FlowGraph.cpp] 3304\n");
        BOOL fFirst = TRUE;
        Output::Print(_u(" DeadOut("));
        FOREACH_DEAD_SUCCESSOR_BLOCK(blockSucc, this)
        {LOGMEIN("FlowGraph.cpp] 3308\n");
            if (!fFirst)
            {LOGMEIN("FlowGraph.cpp] 3310\n");
                Output::Print(_u(", "));
            }
            Output::Print(_u("%d"), blockSucc->GetBlockNum());
            fFirst = FALSE;
        }
        NEXT_DEAD_SUCCESSOR_BLOCK;
        Output::Print(_u(")"));
    }

    if (this->loop)
    {LOGMEIN("FlowGraph.cpp] 3321\n");
        Output::Print(_u("   Loop(%d) header: %d"), this->loop->loopNumber, this->loop->GetHeadBlock()->GetBlockNum());

        if (this->loop->parent)
        {LOGMEIN("FlowGraph.cpp] 3325\n");
            Output::Print(_u(" parent(%d): %d"), this->loop->parent->loopNumber, this->loop->parent->GetHeadBlock()->GetBlockNum());
        }

        if (this->loop->GetHeadBlock() == this)
        {LOGMEIN("FlowGraph.cpp] 3330\n");
            Output::SkipToColumn(50);
            Output::Print(_u("Call Exp/Imp: "));
            if (this->loop->GetHasCall())
            {LOGMEIN("FlowGraph.cpp] 3334\n");
                Output::Print(_u("yes/"));
            }
            else
            {
                Output::Print(_u(" no/"));
            }
            Output::Print(Js::DynamicProfileInfo::GetImplicitCallFlagsString(this->loop->GetImplicitCallFlags()));
        }
    }

    Output::Print(_u("\n"));
    if (insertCR)
    {LOGMEIN("FlowGraph.cpp] 3347\n");
        Output::Print(_u("\n"));
    }
}

void
BasicBlock::Dump()
{
    // Dumping the first instruction (label) will dump the block header as well.
    FOREACH_INSTR_IN_BLOCK(instr, this)
    {LOGMEIN("FlowGraph.cpp] 3357\n");
        instr->Dump();
    }
    NEXT_INSTR_IN_BLOCK;
}

void
AddPropertyCacheBucket::Dump() const
{LOGMEIN("FlowGraph.cpp] 3365\n");
    Assert(this->initialType != nullptr);
    Assert(this->finalType != nullptr);
    Output::Print(_u(" initial type: 0x%x, final type: 0x%x "), this->initialType->GetAddr(), this->finalType->GetAddr());
}

void
ObjTypeGuardBucket::Dump() const
{LOGMEIN("FlowGraph.cpp] 3373\n");
    Assert(this->guardedPropertyOps != nullptr);
    this->guardedPropertyOps->Dump();
}

void
ObjWriteGuardBucket::Dump() const
{LOGMEIN("FlowGraph.cpp] 3380\n");
    Assert(this->writeGuards != nullptr);
    this->writeGuards->Dump();
}

#endif
