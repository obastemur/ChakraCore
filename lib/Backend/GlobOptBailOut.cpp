//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

void
GlobOpt::CaptureCopyPropValue(BasicBlock * block, Sym * sym, Value * val, SListBase<CopyPropSyms>::EditingIterator & bailOutCopySymsIter)
{TRACE_IT(5682);
    if (!sym->IsStackSym())
    {TRACE_IT(5683);
        return;
    }

    StackSym * copyPropSym = this->GetCopyPropSym(block, sym, val);
    if (copyPropSym != nullptr)
    {TRACE_IT(5684);
        bailOutCopySymsIter.InsertNodeBefore(this->func->m_alloc, sym->AsStackSym(), copyPropSym);
    }
}

void
GlobOpt::CaptureValuesFromScratch(BasicBlock * block,
    SListBase<ConstantStackSymValue>::EditingIterator & bailOutConstValuesIter,
    SListBase<CopyPropSyms>::EditingIterator & bailOutCopySymsIter)
{TRACE_IT(5685);
    Sym * sym = nullptr;
    Value * value = nullptr;
    ValueInfo * valueInfo = nullptr;

    block->globOptData.changedSyms->ClearAll();

    FOREACH_GLOBHASHTABLE_ENTRY(bucket, block->globOptData.symToValueMap)
    {TRACE_IT(5686);
        value = bucket.element;
        valueInfo = value->GetValueInfo();

        if (valueInfo->GetSymStore() == nullptr && !valueInfo->HasIntConstantValue())
        {TRACE_IT(5687);
            continue;
        }

        sym = bucket.value;
        if (sym == nullptr || !sym->IsStackSym() || !(sym->AsStackSym()->HasByteCodeRegSlot()))
        {TRACE_IT(5688);
            continue;
        }
        block->globOptData.changedSyms->Set(sym->m_id);
    }
    NEXT_GLOBHASHTABLE_ENTRY;

    FOREACH_BITSET_IN_SPARSEBV(symId, block->globOptData.changedSyms)
    {
        HashBucket<Sym*, Value*> * bucket = block->globOptData.symToValueMap->GetBucket(symId);
        StackSym * stackSym = bucket->value->AsStackSym();
        value =  bucket->element;
        valueInfo = value->GetValueInfo();

        int intConstantValue;
        if (valueInfo->TryGetIntConstantValue(&intConstantValue))
        {TRACE_IT(5689);
            BailoutConstantValue constValue;
            constValue.InitIntConstValue(intConstantValue);
            bailOutConstValuesIter.InsertNodeBefore(this->func->m_alloc, stackSym, constValue);
        }
        else if (valueInfo->IsVarConstant())
        {TRACE_IT(5690);
            BailoutConstantValue constValue;
            constValue.InitVarConstValue(valueInfo->AsVarConstant()->VarValue());
            bailOutConstValuesIter.InsertNodeBefore(this->func->m_alloc, stackSym, constValue);
        }
        else
        {
            CaptureCopyPropValue(block, stackSym, value, bailOutCopySymsIter);
        }
    }
    NEXT_BITSET_IN_SPARSEBV
}

void
GlobOpt::CaptureValuesIncremental(BasicBlock * block,
    SListBase<ConstantStackSymValue>::EditingIterator & bailOutConstValuesIter,
    SListBase<CopyPropSyms>::EditingIterator & bailOutCopySymsIter)
{TRACE_IT(5691);
    CapturedValues * currCapturedValues = block->globOptData.capturedValues;
    SListBase<ConstantStackSymValue>::Iterator iterConst(currCapturedValues ? &currCapturedValues->constantValues : nullptr);
    SListBase<CopyPropSyms>::Iterator iterCopyPropSym(currCapturedValues ? &currCapturedValues->copyPropSyms : nullptr);
    bool hasConstValue = currCapturedValues ? iterConst.Next() : false;
    bool hasCopyPropSym = currCapturedValues ? iterCopyPropSym.Next() : false;

    block->globOptData.changedSyms->Set(Js::Constants::InvalidSymID);

    FOREACH_BITSET_IN_SPARSEBV(symId, block->globOptData.changedSyms)
    {TRACE_IT(5692);
        Sym * sym = hasConstValue ? iterConst.Data().Key() : nullptr;
        Value * val = nullptr;
        HashBucket<Sym *, Value *> * symIdBucket = nullptr;

        // copy unchanged sym to new capturedValues
        while (sym && sym->m_id < symId)
        {TRACE_IT(5693);
            Assert(sym->IsStackSym());
            if (!sym->AsStackSym()->HasArgSlotNum())
            {TRACE_IT(5694);
                bailOutConstValuesIter.InsertNodeBefore(this->func->m_alloc, sym->AsStackSym(), iterConst.Data().Value());
            }

            hasConstValue = iterConst.Next();
            sym = hasConstValue ? iterConst.Data().Key() : nullptr;
        }
        if (sym && sym->m_id == symId)
        {TRACE_IT(5695);
            hasConstValue = iterConst.Next();
        }
        if (symId != Js::Constants::InvalidSymID)
        {TRACE_IT(5696);
            // recapture changed constant sym

            symIdBucket = block->globOptData.symToValueMap->GetBucket(symId);
            if (symIdBucket == nullptr)
            {TRACE_IT(5697);
                continue;
            }

            Sym * symIdSym = symIdBucket->value;
            Assert(symIdSym->IsStackSym() && (symIdSym->AsStackSym()->HasByteCodeRegSlot() || symIdSym->AsStackSym()->HasArgSlotNum()));

            val =  symIdBucket->element;
            ValueInfo* valueInfo = val->GetValueInfo();

            if (valueInfo->GetSymStore() != nullptr)
            {TRACE_IT(5698);
                int32 intConstValue;
                BailoutConstantValue constValue;

                if (valueInfo->TryGetIntConstantValue(&intConstValue))
                {TRACE_IT(5699);
                    constValue.InitIntConstValue(intConstValue);
                    bailOutConstValuesIter.InsertNodeBefore(this->func->m_alloc, symIdSym->AsStackSym(), constValue);

                    continue;
                }
                else if(valueInfo->IsVarConstant())
                {TRACE_IT(5700);
                    constValue.InitVarConstValue(valueInfo->AsVarConstant()->VarValue());
                    bailOutConstValuesIter.InsertNodeBefore(this->func->m_alloc, symIdSym->AsStackSym(), constValue);

                    continue;
                }
            }
            else if (!valueInfo->HasIntConstantValue())
            {TRACE_IT(5701);
                continue;
            }
        }

        sym = hasCopyPropSym ? iterCopyPropSym.Data().Key() : nullptr;

        // process unchanged sym, but copy sym might have changed
        while (sym && sym->m_id < symId)
        {TRACE_IT(5702);
            StackSym * copyPropSym = iterCopyPropSym.Data().Value();

            Assert(sym->IsStackSym());

            if (!block->globOptData.changedSyms->Test(copyPropSym->m_id))
            {TRACE_IT(5703);
                if (!sym->AsStackSym()->HasArgSlotNum())
                {TRACE_IT(5704);
                    bailOutCopySymsIter.InsertNodeBefore(this->func->m_alloc, sym->AsStackSym(), copyPropSym);
                }
            }
            else
            {TRACE_IT(5705);
                if (!sym->AsStackSym()->HasArgSlotNum())
                {TRACE_IT(5706);
                    val = FindValue(sym);
                    if (val != nullptr)
                    {
                        CaptureCopyPropValue(block, sym, val, bailOutCopySymsIter);
                    }
                }
            }

            hasCopyPropSym = iterCopyPropSym.Next();
            sym = hasCopyPropSym ? iterCopyPropSym.Data().Key() : nullptr;
        }
        if (sym && sym->m_id == symId)
        {TRACE_IT(5707);
            hasCopyPropSym = iterCopyPropSym.Next();
        }
        if (symId != Js::Constants::InvalidSymID)
        {TRACE_IT(5708);
            // recapture changed copy prop sym
            symIdBucket = block->globOptData.symToValueMap->GetBucket(symId);
            if (symIdBucket != nullptr)
            {TRACE_IT(5709);
                Sym * symIdSym = symIdBucket->value;
                val = FindValue(symIdSym);
                if (val != nullptr)
                {
                    CaptureCopyPropValue(block, symIdSym, val, bailOutCopySymsIter);
                }
            }
        }
    }
    NEXT_BITSET_IN_SPARSEBV
}


void
GlobOpt::CaptureValues(BasicBlock *block, BailOutInfo * bailOutInfo)
{TRACE_IT(5710);
    if (!this->func->DoGlobOptsForGeneratorFunc())
    {TRACE_IT(5711);
        // TODO[generators][ianhall]: Enable constprop and copyprop for generator functions; see GlobOpt::CopyProp()
        // Even though CopyProp is disabled for generator functions we must also not put the copy-prop sym into the
        // bailOutInfo so that the bailOutInfo keeps track of the key sym in its byteCodeUpwardExposed list.
        return;
    }

    CapturedValues capturedValues;
    SListBase<ConstantStackSymValue>::EditingIterator bailOutConstValuesIter(&capturedValues.constantValues);
    SListBase<CopyPropSyms>::EditingIterator bailOutCopySymsIter(&capturedValues.copyPropSyms);

    bailOutConstValuesIter.Next();
    bailOutCopySymsIter.Next();

    if (!block->globOptData.capturedValues)
    {
        CaptureValuesFromScratch(block, bailOutConstValuesIter, bailOutCopySymsIter);
    }
    else
    {
        CaptureValuesIncremental(block, bailOutConstValuesIter, bailOutCopySymsIter);
    }

    // attach capturedValues to bailOutInfo

    bailOutInfo->capturedValues.constantValues.Clear(this->func->m_alloc);
    bailOutConstValuesIter.SetNext(&bailOutInfo->capturedValues.constantValues);
    bailOutInfo->capturedValues.constantValues = capturedValues.constantValues;

    bailOutInfo->capturedValues.copyPropSyms.Clear(this->func->m_alloc);
    bailOutCopySymsIter.SetNext(&bailOutInfo->capturedValues.copyPropSyms);
    bailOutInfo->capturedValues.copyPropSyms = capturedValues.copyPropSyms;
    
    if (!PHASE_OFF(Js::IncrementalBailoutPhase, func))
    {TRACE_IT(5712);
        // cache the pointer of current bailout as potential baseline for later bailout in this block
        block->globOptData.capturedValuesCandidate = &bailOutInfo->capturedValues;

        // reset changed syms to track symbols change after the above captured values candidate
        this->changedSymsAfterIncBailoutCandidate->ClearAll();
    }
}

void
GlobOpt::CaptureArguments(BasicBlock *block, BailOutInfo * bailOutInfo, JitArenaAllocator *allocator)
{
    FOREACH_BITSET_IN_SPARSEBV(id, this->blockData.argObjSyms)
    {TRACE_IT(5713);
        StackSym * stackSym = this->func->m_symTable->FindStackSym(id);
        Assert(stackSym != nullptr);
        if (!stackSym->HasByteCodeRegSlot())
        {TRACE_IT(5714);
            continue;
        }

        if (!bailOutInfo->capturedValues.argObjSyms)
        {TRACE_IT(5715);
            bailOutInfo->capturedValues.argObjSyms = JitAnew(allocator, BVSparse<JitArenaAllocator>, allocator);
        }

        bailOutInfo->capturedValues.argObjSyms->Set(id);
        // Add to BailOutInfo
    }
    NEXT_BITSET_IN_SPARSEBV
}

void
GlobOpt::TrackByteCodeSymUsed(IR::Instr * instr, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed, PropertySym **pPropertySym)
{TRACE_IT(5716);
    IR::Opnd * src = instr->GetSrc1();
    if (src)
    {
        TrackByteCodeSymUsed(src, instrByteCodeStackSymUsed, pPropertySym);
        src = instr->GetSrc2();
        if (src)
        {
            TrackByteCodeSymUsed(src, instrByteCodeStackSymUsed, pPropertySym);
        }
    }

#if DBG
    // There should be no more than one property sym used.
    PropertySym *propertySymFromSrc = *pPropertySym;
#endif

    IR::Opnd * dst = instr->GetDst();
    if (dst)
    {TRACE_IT(5717);
        StackSym *stackSym = dst->GetStackSym();

        // We want stackSym uses: IndirOpnd and SymOpnds of propertySyms.
        // RegOpnd and SymOPnd of StackSyms are stack sym defs.
        if (stackSym == NULL)
        {
            TrackByteCodeSymUsed(dst, instrByteCodeStackSymUsed, pPropertySym);
        }
    }

#if DBG
    AssertMsg(propertySymFromSrc == NULL || propertySymFromSrc == *pPropertySym,
              "Lost a property sym use?");
#endif
}

void
GlobOpt::TrackByteCodeSymUsed(IR::RegOpnd * regOpnd, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed)
{TRACE_IT(5718);
    // Check JITOptimizedReg to catch case where baseOpnd of indir was optimized.
    if (!regOpnd->GetIsJITOptimizedReg())
    {TRACE_IT(5719);
        TrackByteCodeSymUsed(regOpnd->m_sym, instrByteCodeStackSymUsed);
    }
}

void
GlobOpt::TrackByteCodeSymUsed(IR::Opnd * opnd, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed, PropertySym **pPropertySym)
{TRACE_IT(5720);
    if (opnd->GetIsJITOptimizedReg())
    {TRACE_IT(5721);
        AssertMsg(!opnd->IsIndirOpnd(), "TrackByteCodeSymUsed doesn't expect IndirOpnd with IsJITOptimizedReg turned on");
        return;
    }

    switch(opnd->GetKind())
    {
    case IR::OpndKindReg:
        TrackByteCodeSymUsed(opnd->AsRegOpnd(), instrByteCodeStackSymUsed);
        break;
    case IR::OpndKindSym:
        {TRACE_IT(5722);
            Sym * sym = opnd->AsSymOpnd()->m_sym;
            if (sym->IsStackSym())
            {TRACE_IT(5723);
                TrackByteCodeSymUsed(sym->AsStackSym(), instrByteCodeStackSymUsed);
            }
            else
            {TRACE_IT(5724);
                TrackByteCodeSymUsed(sym->AsPropertySym()->m_stackSym, instrByteCodeStackSymUsed);
                *pPropertySym = sym->AsPropertySym();
            }
        }
        break;
    case IR::OpndKindIndir:
        TrackByteCodeSymUsed(opnd->AsIndirOpnd()->GetBaseOpnd(), instrByteCodeStackSymUsed);
        {TRACE_IT(5725);
            IR::RegOpnd * indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
            if (indexOpnd)
            {
                TrackByteCodeSymUsed(indexOpnd, instrByteCodeStackSymUsed);
            }
        }
        break;
    }
}

void
GlobOpt::TrackByteCodeSymUsed(StackSym * sym, BVSparse<JitArenaAllocator> * instrByteCodeStackSymUsed)
{TRACE_IT(5726);
    // We only care about stack sym that has a corresponding byte code register
    if (sym->HasByteCodeRegSlot())
    {TRACE_IT(5727);
        if (sym->IsTypeSpec())
        {TRACE_IT(5728);
            // It has to have a var version for byte code regs
            sym = sym->GetVarEquivSym(nullptr);
        }
        instrByteCodeStackSymUsed->Set(sym->m_id);
    }
}

void
GlobOpt::MarkNonByteCodeUsed(IR::Instr * instr)
{TRACE_IT(5729);
    IR::Opnd * dst = instr->GetDst();
    if (dst)
    {TRACE_IT(5730);
        MarkNonByteCodeUsed(dst);
    }

    IR::Opnd * src1 = instr->GetSrc1();
    if (src1)
    {TRACE_IT(5731);
        MarkNonByteCodeUsed(src1);
        IR::Opnd * src2 = instr->GetSrc2();
        if (src2)
        {TRACE_IT(5732);
            MarkNonByteCodeUsed(src2);
        }
    }
}

void
GlobOpt::MarkNonByteCodeUsed(IR::Opnd * opnd)
{TRACE_IT(5733);
    switch(opnd->GetKind())
    {
    case IR::OpndKindReg:
        opnd->AsRegOpnd()->SetIsJITOptimizedReg(true);
        break;
    case IR::OpndKindIndir:
        opnd->AsIndirOpnd()->GetBaseOpnd()->SetIsJITOptimizedReg(true);
        {TRACE_IT(5734);
            IR::RegOpnd * indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
            if (indexOpnd)
            {TRACE_IT(5735);
                indexOpnd->SetIsJITOptimizedReg(true);
            }
        }
        break;
    }
}

void
GlobOpt::CaptureByteCodeSymUses(IR::Instr * instr)
{TRACE_IT(5736);
    if (this->byteCodeUses)
    {TRACE_IT(5737);
        // We already captured it before.
        return;
    }
    Assert(this->propertySymUse == NULL);
    this->byteCodeUses = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    GlobOpt::TrackByteCodeSymUsed(instr, this->byteCodeUses, &this->propertySymUse);

    AssertMsg(this->byteCodeUses->Equal(this->byteCodeUsesBeforeOpt),
        "Instruction edited before capturing the byte code use");
}

void
GlobOpt::TrackCalls(IR::Instr * instr)
{TRACE_IT(5738);
    // Keep track of out params for bailout
    switch (instr->m_opcode)
    {
    case Js::OpCode::StartCall:
        Assert(!this->isCallHelper);
        Assert(instr->GetDst()->IsRegOpnd());
        Assert(instr->GetDst()->AsRegOpnd()->m_sym->m_isSingleDef);

        if (this->blockData.callSequence == nullptr)
        {TRACE_IT(5739);
            this->blockData.callSequence = JitAnew(this->alloc, SListBase<IR::Opnd *>);
            this->currentBlock->globOptData.callSequence = this->blockData.callSequence;
        }
        this->blockData.callSequence->Prepend(this->alloc, instr->GetDst());

        this->currentBlock->globOptData.totalOutParamCount += instr->GetArgOutCount(/*getInterpreterArgOutCount*/ true);
        this->currentBlock->globOptData.startCallCount++;

        break;
    case Js::OpCode::BytecodeArgOutCapture:
        {TRACE_IT(5740);
            this->blockData.callSequence->Prepend(this->alloc, instr->GetDst());
            this->currentBlock->globOptData.argOutCount++;
            break;
        }
    case Js::OpCode::ArgOut_A:
    case Js::OpCode::ArgOut_A_Inline:
    case Js::OpCode::ArgOut_A_FixupForStackArgs:
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
    case Js::OpCode::ArgOut_A_Dynamic:
    case Js::OpCode::ArgOut_A_FromStackArgs:
    case Js::OpCode::ArgOut_A_SpreadArg:
    {TRACE_IT(5741);
        IR::Opnd * opnd = instr->GetDst();
        if (opnd->IsSymOpnd())
        {TRACE_IT(5742);
            Assert(!this->isCallHelper);
            Assert(!this->blockData.callSequence->Empty());
            StackSym* stackSym = opnd->AsSymOpnd()->m_sym->AsStackSym();

            // These scenarios are already tracked using BytecodeArgOutCapture,
            // and we don't want to be tracking ArgOut_A_FixupForStackArgs as these are only visible to the JIT and we should not be restoring them upon bailout.
            if (!stackSym->m_isArgCaptured && instr->m_opcode != Js::OpCode::ArgOut_A_FixupForStackArgs)
            {TRACE_IT(5743);
                this->blockData.callSequence->Prepend(this->alloc, instr->GetDst());
                this->currentBlock->globOptData.argOutCount++;
            }
            Assert(stackSym->IsArgSlotSym());
            if (stackSym->m_isInlinedArgSlot)
            {TRACE_IT(5744);
                this->currentBlock->globOptData.inlinedArgOutCount++;
                // We want to update the offsets only once: don't do in prepass.
                if (!this->IsLoopPrePass() && stackSym->m_offset >= 0)
                {TRACE_IT(5745);
                    Func * currentFunc = instr->m_func;
                    stackSym->FixupStackOffset(currentFunc);
                }
            }
        }
        else
        {TRACE_IT(5746);
            // It is a reg opnd if it is a helper call
            // It should be all ArgOut until the CallHelper instruction
            Assert(opnd->IsRegOpnd());
            this->isCallHelper = true;
        }

        if (instr->m_opcode == Js::OpCode::ArgOut_A_FixupForStackArgs && !this->IsLoopPrePass())
        {TRACE_IT(5747);
            instr->m_opcode = Js::OpCode::ArgOut_A_Inline;
        }
        break;
    }

    case Js::OpCode::InlineeStart:
        Assert(instr->m_func->GetParentFunc() == this->blockData.curFunc);
        Assert(instr->m_func->GetParentFunc());
        this->blockData.curFunc = instr->m_func;
        this->currentBlock->globOptData.curFunc = instr->m_func;

        this->func->UpdateMaxInlineeArgOutCount(this->currentBlock->globOptData.inlinedArgOutCount);
        this->EndTrackCall(instr);

        if (DoInlineArgsOpt(instr->m_func))
        {TRACE_IT(5748);
            instr->m_func->m_hasInlineArgsOpt = true;
            InlineeFrameInfo* frameInfo = InlineeFrameInfo::New(func->m_alloc);
            instr->m_func->frameInfo = frameInfo;
            frameInfo->floatSyms = currentBlock->globOptData.liveFloat64Syms->CopyNew(this->alloc);
            frameInfo->intSyms = currentBlock->globOptData.liveInt32Syms->MinusNew(currentBlock->globOptData.liveLossyInt32Syms, this->alloc);

            // SIMD_JS
            frameInfo->simd128F4Syms = currentBlock->globOptData.liveSimd128F4Syms->CopyNew(this->alloc);
            frameInfo->simd128I4Syms = currentBlock->globOptData.liveSimd128I4Syms->CopyNew(this->alloc);
        }
        break;

    case Js::OpCode::EndCallForPolymorphicInlinee:
        // Have this opcode mimic the functions of both InlineeStart and InlineeEnd in the bailout block of a polymorphic call inlined using fixed methods.
        this->EndTrackCall(instr);
        break;

    case Js::OpCode::CallHelper:
    case Js::OpCode::IsInst:
        Assert(this->isCallHelper);
        this->isCallHelper = false;
        break;

    case Js::OpCode::InlineeEnd:
        if (instr->m_func->m_hasInlineArgsOpt)
        {TRACE_IT(5749);
            RecordInlineeFrameInfo(instr);
        }
        EndTrackingOfArgObjSymsForInlinee();

        Assert(this->currentBlock->globOptData.inlinedArgOutCount >= instr->GetArgOutCount(/*getInterpreterArgOutCount*/ false));
        this->currentBlock->globOptData.inlinedArgOutCount -= instr->GetArgOutCount(/*getInterpreterArgOutCount*/ false);
        break;

    case Js::OpCode::InlineeMetaArg:
    {TRACE_IT(5750);
        Assert(instr->GetDst()->IsSymOpnd());
        StackSym * stackSym = instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
        Assert(stackSym->IsArgSlotSym());

        // InlineeMetaArg has the m_func set as the "inlinee" and not the "inliner"
        // TODO: Review this and fix the m_func of InlineeMetaArg to be "inliner" (as for the rest of the ArgOut's)
        // We want to update the offsets only once: don't do in prepass.
        if (!this->IsLoopPrePass())
        {TRACE_IT(5751);
            Func * currentFunc = instr->m_func->GetParentFunc();
            stackSym->FixupStackOffset(currentFunc);
        }
        this->currentBlock->globOptData.inlinedArgOutCount++;
        break;
    }

    case Js::OpCode::InlineBuiltInStart:
        this->inInlinedBuiltIn = true;
        break;

    case Js::OpCode::InlineNonTrackingBuiltInEnd:
    case Js::OpCode::InlineBuiltInEnd:
    {TRACE_IT(5752);
        // If extra bailouts were added for the InlineMathXXX call itself,
        // move InlineeBuiltInStart just above the InlineMathXXX.
        // This is needed so that the function argument has lifetime after all bailouts for InlineMathXXX,
        // otherwise when we bailout we would get wrong function.
        IR::Instr* inlineBuiltInStartInstr = instr->m_prev;
        while (inlineBuiltInStartInstr->m_opcode != Js::OpCode::InlineBuiltInStart)
        {TRACE_IT(5753);
            inlineBuiltInStartInstr = inlineBuiltInStartInstr->m_prev;
        }

        IR::Instr *byteCodeUsesInstr = inlineBuiltInStartInstr->m_prev;
        IR::Instr * insertBeforeInstr = instr->m_prev;
        IR::Instr * tmpInstr = insertBeforeInstr;
        while(tmpInstr->m_opcode != Js::OpCode::InlineBuiltInStart )
        {TRACE_IT(5754);
            if(tmpInstr->m_opcode == Js::OpCode::ByteCodeUses)
            {TRACE_IT(5755);
                insertBeforeInstr = tmpInstr;
            }
            tmpInstr = tmpInstr->m_prev;
        }
        inlineBuiltInStartInstr->Unlink();
        if(insertBeforeInstr == instr->m_prev)
        {TRACE_IT(5756);
            insertBeforeInstr->InsertBefore(inlineBuiltInStartInstr);
        }

        else
        {TRACE_IT(5757);
            insertBeforeInstr->m_prev->InsertBefore(inlineBuiltInStartInstr);
        }

        // Need to move the byte code uses instructions associated with inline built-in start instruction as well. For instance,
        // copy-prop may have replaced the function sym and inserted a byte code uses for the original sym holding the function.
        // That byte code uses instruction needs to appear after bailouts inserted for the InlinMathXXX instruction since the
        // byte code register holding the function object needs to be restored on bailout.
        IR::Instr *const insertByteCodeUsesAfterInstr = inlineBuiltInStartInstr->m_prev;
        if(byteCodeUsesInstr != insertByteCodeUsesAfterInstr)
        {TRACE_IT(5758);
            // The InlineBuiltInStart instruction was moved, look for its ByteCodeUses instructions that also need to be moved
            while(
                byteCodeUsesInstr->IsByteCodeUsesInstr() &&
                byteCodeUsesInstr->AsByteCodeUsesInstr()->GetByteCodeOffset() == inlineBuiltInStartInstr->GetByteCodeOffset())
            {TRACE_IT(5759);
                IR::Instr *const instrToMove = byteCodeUsesInstr;
                byteCodeUsesInstr = byteCodeUsesInstr->m_prev;
                instrToMove->Unlink();
                insertByteCodeUsesAfterInstr->InsertAfter(instrToMove);
            }
        }

        // The following code makes more sense to be processed when we hit InlineeBuiltInStart,
        // but when extra bailouts are added for the InlineMathXXX and InlineArrayPop instructions itself, those bailouts
        // need to know about current bailout record, but since they are added after TrackCalls is called
        // for InlineeBuiltInStart, we can't clear current record when got InlineeBuiltInStart

        // Do not track calls for InlineNonTrackingBuiltInEnd, as it is already tracked for InlineArrayPop
        if(instr->m_opcode == Js::OpCode::InlineBuiltInEnd)
        {TRACE_IT(5760);
            this->EndTrackCall(instr);
        }

        Assert(this->currentBlock->globOptData.inlinedArgOutCount >= instr->GetArgOutCount(/*getInterpreterArgOutCount*/ false));
        this->currentBlock->globOptData.inlinedArgOutCount -= instr->GetArgOutCount(/*getInterpreterArgOutCount*/ false);

        this->inInlinedBuiltIn = false;
        break;
    }

    case Js::OpCode::InlineArrayPop:
    {TRACE_IT(5761);
        // EndTrackCall should be called here as the Post-op BailOutOnImplicitCalls will bail out to the instruction after the Pop function call instr.
        // This bailout shouldn't be tracking the call sequence as it will then erroneously reserve stack space for arguments when the call would have already happened
        // Can't wait till InlineBuiltinEnd like we do for other InlineMathXXX because by then we would have filled bailout info for the BailOutOnImplicitCalls for InlineArrayPop.
        this->EndTrackCall(instr);
        break;
    }

    default:
        if (OpCodeAttr::CallInstr(instr->m_opcode))
        {TRACE_IT(5762);
            this->EndTrackCall(instr);
            if (this->inInlinedBuiltIn && instr->m_opcode == Js::OpCode::CallDirect)
            {TRACE_IT(5763);
                // We can end up in this situation when a built-in apply target is inlined to a CallDirect. We have the following IR:
                //
                // StartCall
                // ArgOut_InlineBuiltIn
                // ArgOut_InlineBuiltIn
                // ArgOut_InlineBuiltIn
                // InlineBuiltInStart
                //      ArgOut_A_InlineSpecialized
                //      ArgOut_A
                //      ArgOut_A
                //      CallDirect
                // InlineNonTrackingBuiltInEnd
                //
                // We need to call EndTrackCall twice for CallDirect in this case. The CallDirect may get a BailOutOnImplicitCalls later,
                // but it should not be tracking the call sequence for the apply call as it is a post op bailout and the call would have
                // happened when we bail out.
                // Can't wait till InlineBuiltinEnd like we do for other InlineMathXXX because by then we would have filled bailout info for the BailOutOnImplicitCalls for CallDirect.
                this->EndTrackCall(instr);
            }
        }
        break;
    }
}

void GlobOpt::RecordInlineeFrameInfo(IR::Instr* inlineeEnd)
{TRACE_IT(5764);
    if (this->IsLoopPrePass())
    {TRACE_IT(5765);
        return;
    }
    InlineeFrameInfo* frameInfo = inlineeEnd->m_func->frameInfo;
    if (frameInfo->isRecorded)
    {TRACE_IT(5766);
        Assert(frameInfo->function.type != InlineeFrameInfoValueType_None);
        // Due to Cmp peeps in flow graph - InlineeEnd can be cloned.
        return;
    }
    inlineeEnd->IterateArgInstrs([=] (IR::Instr* argInstr)
    {
        if (argInstr->m_opcode == Js::OpCode::InlineeStart)
        {TRACE_IT(5767);
            Assert(frameInfo->function.type == InlineeFrameInfoValueType_None);
            IR::RegOpnd* functionObject = argInstr->GetSrc1()->AsRegOpnd();
            if (functionObject->m_sym->IsConst())
            {TRACE_IT(5768);
                frameInfo->function = InlineFrameInfoValue(functionObject->m_sym->GetConstValueForBailout());
            }
            else
            {TRACE_IT(5769);
                frameInfo->function = InlineFrameInfoValue(functionObject->m_sym);
            }
        }
        else
        {TRACE_IT(5770);
            Js::ArgSlot argSlot = argInstr->GetDst()->AsSymOpnd()->m_sym->AsStackSym()->GetArgSlotNum();
            IR::Opnd* argOpnd = argInstr->GetSrc1();
            InlineFrameInfoValue frameInfoValue;
            StackSym* argSym = argOpnd->GetStackSym();
            if (!argSym)
            {TRACE_IT(5771);
                frameInfoValue = InlineFrameInfoValue(argOpnd->GetConstValue());
            }
            else if (argSym->IsConst())
            {TRACE_IT(5772);
                frameInfoValue = InlineFrameInfoValue(argSym->GetConstValueForBailout());
            }
            else
            {TRACE_IT(5773);
                if (PHASE_ON(Js::CopyPropPhase, func))
                {TRACE_IT(5774);
                    Value* value = FindValue(argSym);

                    StackSym * copyPropSym = this->GetCopyPropSym(this->currentBlock, argSym, value);
                    if (copyPropSym)
                    {TRACE_IT(5775);
                        argSym = copyPropSym;
                    }
                }

                GlobOptBlockData& globOptData = this->currentBlock->globOptData;

                if (frameInfo->intSyms->TestEmpty() && frameInfo->intSyms->Test(argSym->m_id))
                {TRACE_IT(5776);
                    // Var version of the sym is not live, use the int32 version
                    argSym = argSym->GetInt32EquivSym(nullptr);
                    Assert(argSym);
                }
                else if (frameInfo->floatSyms->TestEmpty() && frameInfo->floatSyms->Test(argSym->m_id))
                {TRACE_IT(5777);
                    // Var/int32 version of the sym is not live, use the float64 version
                    argSym = argSym->GetFloat64EquivSym(nullptr);
                    Assert(argSym);
                }
                // SIMD_JS
                else if (frameInfo->simd128F4Syms->TestEmpty() && frameInfo->simd128F4Syms->Test(argSym->m_id))
                {TRACE_IT(5778);
                    argSym = argSym->GetSimd128F4EquivSym(nullptr);
                }
                else if (frameInfo->simd128I4Syms->TestEmpty() && frameInfo->simd128I4Syms->Test(argSym->m_id))
                {TRACE_IT(5779);
                    argSym = argSym->GetSimd128I4EquivSym(nullptr);
                }
                else
                {TRACE_IT(5780);
                    Assert(globOptData.liveVarSyms->Test(argSym->m_id));
                }

                if (argSym->IsConst())
                {TRACE_IT(5781);
                    frameInfoValue = InlineFrameInfoValue(argSym->GetConstValueForBailout());
                }
                else
                {TRACE_IT(5782);
                    frameInfoValue = InlineFrameInfoValue(argSym);
                }
            }
            Assert(argSlot >= 1);
            frameInfo->arguments->SetItem(argSlot - 1, frameInfoValue);
        }
        return false;
    });

    JitAdelete(this->alloc, frameInfo->intSyms);
    frameInfo->intSyms = nullptr;
    JitAdelete(this->alloc, frameInfo->floatSyms);
    frameInfo->floatSyms = nullptr;

    // SIMD_JS
    JitAdelete(this->alloc, frameInfo->simd128F4Syms);
    frameInfo->simd128F4Syms = nullptr;
    JitAdelete(this->alloc, frameInfo->simd128I4Syms);
    frameInfo->simd128I4Syms = nullptr;

    frameInfo->isRecorded = true;
}

void GlobOpt::EndTrackingOfArgObjSymsForInlinee()
{TRACE_IT(5783);
    Assert(this->blockData.curFunc->GetParentFunc());
    if (this->blockData.curFunc->argObjSyms && TrackArgumentsObject())
    {TRACE_IT(5784);
        BVSparse<JitArenaAllocator> * tempBv = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
        tempBv->Minus(this->blockData.curFunc->argObjSyms, this->blockData.argObjSyms);
        if(!tempBv->IsEmpty())
        {TRACE_IT(5785);
            // This means there are arguments object symbols in the current function which are not in the current block.
            // This could happen when one of the blocks has a throw and arguments object aliased in it and other blocks don't see it.
            // Rare case, abort stack arguments optimization in this case.
            CannotAllocateArgumentsObjectOnStack();
        }
        else
        {TRACE_IT(5786);
            Assert(this->blockData.argObjSyms->OrNew(this->blockData.curFunc->argObjSyms)->Equal(this->blockData.argObjSyms));
            this->blockData.argObjSyms->Minus(this->blockData.curFunc->argObjSyms);
        }
        JitAdelete(this->tempAlloc, tempBv);
    }
    this->blockData.curFunc = this->blockData.curFunc->GetParentFunc();
    this->currentBlock->globOptData.curFunc = this->blockData.curFunc;
}

void GlobOpt::EndTrackCall(IR::Instr* instr)
{TRACE_IT(5787);
    Assert(instr);
    Assert(OpCodeAttr::CallInstr(instr->m_opcode) || instr->m_opcode == Js::OpCode::InlineeStart || instr->m_opcode == Js::OpCode::InlineBuiltInEnd
        || instr->m_opcode == Js::OpCode::InlineArrayPop || instr->m_opcode == Js::OpCode::EndCallForPolymorphicInlinee);

    Assert(!this->isCallHelper);
    Assert(!this->blockData.callSequence->Empty());


#if DBG
    uint origArgOutCount = this->currentBlock->globOptData.argOutCount;
#endif
    while (this->blockData.callSequence->Head()->GetStackSym()->HasArgSlotNum())
    {TRACE_IT(5788);
        this->currentBlock->globOptData.argOutCount--;
        this->blockData.callSequence->RemoveHead(this->alloc);
    }
    StackSym * sym = this->blockData.callSequence->Head()->AsRegOpnd()->m_sym->AsStackSym();
    this->blockData.callSequence->RemoveHead(this->alloc);

#if DBG
    Assert(sym->m_isSingleDef);
    Assert(sym->m_instrDef->m_opcode == Js::OpCode::StartCall);

    // Number of argument set should be the same as indicated at StartCall
    // except NewScObject has an implicit arg1
    Assert((uint)sym->m_instrDef->GetArgOutCount(/*getInterpreterArgOutCount*/ true) ==
        origArgOutCount - this->currentBlock->globOptData.argOutCount +
           (instr->m_opcode == Js::OpCode::NewScObject || instr->m_opcode == Js::OpCode::NewScObjArray
           || instr->m_opcode == Js::OpCode::NewScObjectSpread || instr->m_opcode == Js::OpCode::NewScObjArraySpread));

#endif

    this->currentBlock->globOptData.totalOutParamCount -= sym->m_instrDef->GetArgOutCount(/*getInterpreterArgOutCount*/ true);
    this->currentBlock->globOptData.startCallCount--;
}

void
GlobOpt::FillBailOutInfo(BasicBlock *block, BailOutInfo * bailOutInfo)
{TRACE_IT(5789);
    AssertMsg(!this->isCallHelper, "Bail out can't be inserted the middle of CallHelper sequence");

    bailOutInfo->liveVarSyms = block->globOptData.liveVarSyms->CopyNew(this->func->m_alloc);
    bailOutInfo->liveFloat64Syms = block->globOptData.liveFloat64Syms->CopyNew(this->func->m_alloc);
    // SIMD_JS
    bailOutInfo->liveSimd128F4Syms = block->globOptData.liveSimd128F4Syms->CopyNew(this->func->m_alloc);
    bailOutInfo->liveSimd128I4Syms = block->globOptData.liveSimd128I4Syms->CopyNew(this->func->m_alloc);  

    // The live int32 syms in the bailout info are only the syms resulting from lossless conversion to int. If the int32 value
    // was created from a lossy conversion to int, the original var value cannot be re-materialized from the int32 value. So, the
    // int32 version is considered to be not live for the purposes of bailout, which forces the var or float versions to be used
    // directly for restoring the value during bailout. Otherwise, bailout may try to re-materialize the var value by converting
    // the lossily-converted int value back into a var, restoring the wrong value.
    bailOutInfo->liveLosslessInt32Syms =
        block->globOptData.liveInt32Syms->MinusNew(block->globOptData.liveLossyInt32Syms, this->func->m_alloc);

    // Save the stack literal init field count so we can null out the uninitialized fields
    StackLiteralInitFldDataMap * stackLiteralInitFldDataMap = block->globOptData.stackLiteralInitFldDataMap;
    if (stackLiteralInitFldDataMap != nullptr)
    {TRACE_IT(5790);
        uint stackLiteralInitFldDataCount = stackLiteralInitFldDataMap->Count();
        if (stackLiteralInitFldDataCount != 0)
        {TRACE_IT(5791);
            auto stackLiteralBailOutInfo = AnewArray(this->func->m_alloc,
                BailOutInfo::StackLiteralBailOutInfo, stackLiteralInitFldDataCount);
            uint i = 0;
            stackLiteralInitFldDataMap->Map(
                [stackLiteralBailOutInfo, stackLiteralInitFldDataCount, &i](StackSym * stackSym, StackLiteralInitFldData const& data)
            {
                Assert(i < stackLiteralInitFldDataCount);
                stackLiteralBailOutInfo[i].stackSym = stackSym;
                stackLiteralBailOutInfo[i].initFldCount = data.currentInitFldCount;
                i++;
            });

            Assert(i == stackLiteralInitFldDataCount);
            bailOutInfo->stackLiteralBailOutInfoCount = stackLiteralInitFldDataCount;
            bailOutInfo->stackLiteralBailOutInfo = stackLiteralBailOutInfo;
        }
    }

    if (TrackArgumentsObject())
    {TRACE_IT(5792);
        this->CaptureArguments(block, bailOutInfo, this->func->m_alloc);
    }

    if (block->globOptData.callSequence && !block->globOptData.callSequence->Empty())
    {TRACE_IT(5793);
        uint currentArgOutCount = 0;
        uint startCallNumber = block->globOptData.startCallCount;

        bailOutInfo->startCallInfo = JitAnewArray(this->func->m_alloc, BailOutInfo::StartCallInfo, startCallNumber);
        bailOutInfo->startCallCount = startCallNumber;

        // Save the start call's func to identify the function (inlined) that the call sequence is for
        // We might not have any arg out yet to get the function from
        bailOutInfo->startCallFunc = JitAnewArray(this->func->m_alloc, Func *, startCallNumber);
#ifdef _M_IX86
        bailOutInfo->inlinedStartCall = BVFixed::New(startCallNumber, this->func->m_alloc, false);
#endif
        uint totalOutParamCount = block->globOptData.totalOutParamCount;
        bailOutInfo->totalOutParamCount = totalOutParamCount;
        bailOutInfo->argOutSyms = JitAnewArrayZ(this->func->m_alloc, StackSym *, totalOutParamCount);

        uint argRestoreAdjustCount = 0;
        FOREACH_SLISTBASE_ENTRY(IR::Opnd *, opnd, block->globOptData.callSequence)
        {TRACE_IT(5794);
            if(opnd->GetStackSym()->HasArgSlotNum())
            {TRACE_IT(5795);
                StackSym * sym;
                if(opnd->IsSymOpnd())
                {TRACE_IT(5796);
                    sym = opnd->AsSymOpnd()->m_sym->AsStackSym();
                    Assert(sym->IsArgSlotSym());
                    Assert(sym->m_isSingleDef);
                    Assert(sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A
                        || sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A_Inline
                        || sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A_InlineBuiltIn
                        || sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A_SpreadArg
                        || sym->m_instrDef->m_opcode == Js::OpCode::ArgOut_A_Dynamic);
                }
                else
                {TRACE_IT(5797);
                    sym = opnd->GetStackSym();
                    Assert(FindValue(sym));
                    // StackSym args need to be re-captured
                    this->SetChangedSym(sym->m_id);
                }

                Assert(totalOutParamCount != 0);
                Assert(totalOutParamCount > currentArgOutCount);
                currentArgOutCount++;
#pragma prefast(suppress:26000, "currentArgOutCount is never 0");
                bailOutInfo->argOutSyms[totalOutParamCount - currentArgOutCount] = sym;
                // Note that there could be ArgOuts below current bailout instr that belong to current call (currentArgOutCount < argOutCount),
                // in which case we will have nulls in argOutSyms[] in start of section for current call, because we fill from tail.
                // Example: StartCall 3, ArgOut1,.. ArgOut2, Bailout,.. Argout3 -> [NULL, ArgOut1, ArgOut2].
            }
            else
            {TRACE_IT(5798);
                Assert(opnd->IsRegOpnd());
                StackSym * sym = opnd->AsRegOpnd()->m_sym;
                Assert(!sym->IsArgSlotSym());
                Assert(sym->m_isSingleDef);
                Assert(sym->m_instrDef->m_opcode == Js::OpCode::StartCall);

                Assert(startCallNumber != 0);
                startCallNumber--;

                bailOutInfo->startCallFunc[startCallNumber] = sym->m_instrDef->m_func;
#ifdef _M_IX86
                if (this->currentRegion && this->currentRegion->GetType() == RegionTypeTry)
                {TRACE_IT(5799);
                    // For a bailout in argument evaluation from an EH region, the esp is offset by the TryCatch helper�s frame. So, the argouts are not actually pushed at the
                    // offsets stored in the bailout record, which are relative to ebp. Need to restore the argouts from the actual value of esp before calling the Bailout helper.
                    // For nested calls, argouts for the outer call need to be restored from an offset of stack-adjustment-done-by-the-inner-call from esp.
                    if (startCallNumber + 1 == bailOutInfo->startCallCount)
                    {TRACE_IT(5800);
                        argRestoreAdjustCount = 0;
                    }
                    else
                    {TRACE_IT(5801);
                        argRestoreAdjustCount = bailOutInfo->startCallInfo[startCallNumber + 1].argRestoreAdjustCount + bailOutInfo->startCallInfo[startCallNumber + 1].argCount;
                        if ((Math::Align<int32>(bailOutInfo->startCallInfo[startCallNumber + 1].argCount * MachPtr, MachStackAlignment) - (bailOutInfo->startCallInfo[startCallNumber + 1].argCount * MachPtr)) != 0)
                        {TRACE_IT(5802);
                            argRestoreAdjustCount++;
                        }
                    }
                }

                if (sym->m_isInlinedArgSlot)
                {TRACE_IT(5803);
                    bailOutInfo->inlinedStartCall->Set(startCallNumber);
                }
#endif
                uint argOutCount = sym->m_instrDef->GetArgOutCount(/*getInterpreterArgOutCount*/ true);
                Assert(totalOutParamCount >= argOutCount);
                Assert(argOutCount >= currentArgOutCount);

                bailOutInfo->RecordStartCallInfo(startCallNumber, argRestoreAdjustCount, sym->m_instrDef);
                totalOutParamCount -= argOutCount;
                currentArgOutCount = 0;

            }
        }
        NEXT_SLISTBASE_ENTRY;

        Assert(totalOutParamCount == 0);
        Assert(startCallNumber == 0);
        Assert(currentArgOutCount == 0);
    }

    // Save the constant values that we know so we can restore them directly.
    // This allows us to dead store the constant value assign.
    this->CaptureValues(block, bailOutInfo);
}

IR::ByteCodeUsesInstr *
GlobOpt::InsertByteCodeUses(IR::Instr * instr, bool includeDef)
{TRACE_IT(5804);
    IR::ByteCodeUsesInstr * byteCodeUsesInstr = nullptr;
    Assert(this->byteCodeUses);
    IR::RegOpnd * dstOpnd = nullptr;
    if (includeDef)
    {TRACE_IT(5805);
        IR::Opnd * opnd = instr->GetDst();
        if (opnd && opnd->IsRegOpnd())
        {TRACE_IT(5806);
            dstOpnd = opnd->AsRegOpnd();
            if (dstOpnd->GetIsJITOptimizedReg() || !dstOpnd->m_sym->HasByteCodeRegSlot())
            {TRACE_IT(5807);
                dstOpnd = nullptr;
            }
        }
    }
    if (!this->byteCodeUses->IsEmpty() || this->propertySymUse || dstOpnd != nullptr)
    {TRACE_IT(5808);
        byteCodeUsesInstr = IR::ByteCodeUsesInstr::New(instr);
        if (!this->byteCodeUses->IsEmpty())
        {TRACE_IT(5809);
            byteCodeUsesInstr->SetBV(byteCodeUses->CopyNew(instr->m_func->m_alloc));
        }
        if (dstOpnd != nullptr)
        {TRACE_IT(5810);
            byteCodeUsesInstr->SetFakeDst(dstOpnd);
        }
        if (this->propertySymUse)
        {TRACE_IT(5811);
            byteCodeUsesInstr->propertySymUse = this->propertySymUse;
        }
        instr->InsertBefore(byteCodeUsesInstr);
    }

    JitAdelete(this->alloc, this->byteCodeUses);
    this->byteCodeUses = nullptr;
    this->propertySymUse = nullptr;
    return byteCodeUsesInstr;
}

IR::ByteCodeUsesInstr *
GlobOpt::ConvertToByteCodeUses(IR::Instr * instr)
{TRACE_IT(5812);
#if DBG
    PropertySym *propertySymUseBefore = NULL;
    Assert(this->byteCodeUses == nullptr);
    this->byteCodeUsesBeforeOpt->ClearAll();
    GlobOpt::TrackByteCodeSymUsed(instr, this->byteCodeUsesBeforeOpt, &propertySymUseBefore);
#endif
    this->CaptureByteCodeSymUses(instr);
    IR::ByteCodeUsesInstr * byteCodeUsesInstr = this->InsertByteCodeUses(instr, true);
    instr->Remove();
    if (byteCodeUsesInstr)
    {TRACE_IT(5813);
        byteCodeUsesInstr->Aggregate();
    }
    return byteCodeUsesInstr;
}

bool
GlobOpt::MayNeedBailOut(Loop * loop) const
{TRACE_IT(5814);
    Assert(this->IsLoopPrePass());
    return loop->CanHoistInvariants() ||
        this->DoFieldCopyProp(loop) || (this->DoFieldHoisting(loop) && !loop->fieldHoistCandidates->IsEmpty());
}

bool
GlobOpt::MaySrcNeedBailOnImplicitCall(IR::Opnd * opnd, Value *val)
{TRACE_IT(5815);
    switch (opnd->GetKind())
    {
    case IR::OpndKindAddr:
    case IR::OpndKindFloatConst:
    case IR::OpndKindIntConst:
        return false;
    case IR::OpndKindReg:
        // Only need implicit call if the operation will call ToPrimitive and we haven't prove
        // that it is already a primitive
        return 
            !(val && val->GetValueInfo()->IsPrimitive()) &&
            !opnd->AsRegOpnd()->GetValueType().IsPrimitive() &&
            !opnd->AsRegOpnd()->m_sym->IsInt32() &&
            !opnd->AsRegOpnd()->m_sym->IsFloat64() &&
            !opnd->AsRegOpnd()->m_sym->IsFloatConst() &&
            !opnd->AsRegOpnd()->m_sym->IsIntConst();
    case IR::OpndKindSym:
        if (opnd->AsSymOpnd()->IsPropertySymOpnd())
        {TRACE_IT(5816);
            IR::PropertySymOpnd* propertySymOpnd = opnd->AsSymOpnd()->AsPropertySymOpnd();
            if (!propertySymOpnd->MayHaveImplicitCall())
            {TRACE_IT(5817);
                return false;
            }
        }
        return true;
    default:
        return true;
    };
}

bool
GlobOpt::IsImplicitCallBailOutCurrentlyNeeded(IR::Instr * instr, Value *src1Val, Value *src2Val)
{TRACE_IT(5818);
    Assert(!this->IsLoopPrePass());

    return this->IsImplicitCallBailOutCurrentlyNeeded(instr, src1Val, src2Val, this->currentBlock,
        (!this->blockData.liveFields->IsEmpty()), !this->currentBlock->IsLandingPad(), true);
}

bool
GlobOpt::IsImplicitCallBailOutCurrentlyNeeded(IR::Instr * instr, Value *src1Val, Value *src2Val, BasicBlock * block, bool hasLiveFields, bool mayNeedImplicitCallBailOut, bool isForwardPass)
{TRACE_IT(5819);
    if (mayNeedImplicitCallBailOut &&
        !instr->CallsAccessor() &&
        (
            NeedBailOnImplicitCallForLiveValues(block, isForwardPass) ||
            NeedBailOnImplicitCallForCSE(block, isForwardPass) ||
            NeedBailOnImplicitCallWithFieldOpts(block->loop, hasLiveFields) ||
            NeedBailOnImplicitCallForArrayCheckHoist(block, isForwardPass)
        ) &&
        (!instr->HasTypeCheckBailOut() && MayNeedBailOnImplicitCall(instr, src1Val, src2Val)))
    {TRACE_IT(5820);
        return true;
    }

#if DBG
    if (Js::Configuration::Global.flags.IsEnabled(Js::BailOutAtEveryImplicitCallFlag) &&
        !instr->HasBailOutInfo() && MayNeedBailOnImplicitCall(instr, nullptr, nullptr))
    {TRACE_IT(5821);
        // always add implicit call bailout even if we don't need it, but only on opcode that supports it
        return true;
    }
#endif

    return false;
}

bool
GlobOpt::IsTypeCheckProtected(const IR::Instr * instr)
{TRACE_IT(5822);
#if DBG
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src1 = instr->GetSrc1();
    IR::Opnd* src2 = instr->GetSrc2();
    AssertMsg(!dst || !dst->IsSymOpnd() || !dst->AsSymOpnd()->IsPropertySymOpnd() ||
        !src1 || !src1->IsSymOpnd() || !src1->AsSymOpnd()->IsPropertySymOpnd(), "No instruction should have a src1 and dst be a PropertySymOpnd.");
    AssertMsg(!src2 || !src2->IsSymOpnd() || !src2->AsSymOpnd()->IsPropertySymOpnd(), "No instruction should have a src2 be a PropertySymOpnd.");
#endif

    IR::Opnd * opnd = instr->GetDst();
    if (opnd && opnd->IsSymOpnd() && opnd->AsSymOpnd()->IsPropertySymOpnd())
    {TRACE_IT(5823);
        return opnd->AsPropertySymOpnd()->IsTypeCheckProtected();
    }
    opnd = instr->GetSrc1();
    if (opnd && opnd->IsSymOpnd() && opnd->AsSymOpnd()->IsPropertySymOpnd())
    {TRACE_IT(5824);
        return opnd->AsPropertySymOpnd()->IsTypeCheckProtected();
    }
    return false;
}

bool
GlobOpt::NeedsTypeCheckBailOut(const IR::Instr *instr, IR::PropertySymOpnd *propertySymOpnd, bool isStore, bool* pIsTypeCheckProtected, IR::BailOutKind *pBailOutKind)
{TRACE_IT(5825);
    if (instr->m_opcode == Js::OpCode::CheckPropertyGuardAndLoadType || instr->m_opcode == Js::OpCode::LdMethodFldPolyInlineMiss)
    {TRACE_IT(5826);
        return false;
    }
    // CheckFixedFld always requires a type check and bailout either at the instruction or upstream.
    Assert(instr->m_opcode != Js::OpCode::CheckFixedFld || (propertySymOpnd->UsesFixedValue() && propertySymOpnd->MayNeedTypeCheckProtection()));

    if (propertySymOpnd->MayNeedTypeCheckProtection())
    {TRACE_IT(5827);
        bool isCheckFixedFld = instr->m_opcode == Js::OpCode::CheckFixedFld;
        AssertMsg(!isCheckFixedFld || !PHASE_OFF(Js::FixedMethodsPhase, instr->m_func) ||
            !PHASE_OFF(Js::UseFixedDataPropsPhase, instr->m_func), "CheckFixedFld with fixed method/data phase disabled?");
        Assert(!isStore || !isCheckFixedFld);
        // We don't share caches between field loads and stores.  We should never have a field store involving a proto cache.
        Assert(!isStore || !propertySymOpnd->IsLoadedFromProto());

        if (propertySymOpnd->NeedsTypeCheckAndBailOut())
        {TRACE_IT(5828);
            *pBailOutKind = propertySymOpnd->HasEquivalentTypeSet() && !propertySymOpnd->MustDoMonoCheck() ?
                (isCheckFixedFld ? IR::BailOutFailedEquivalentFixedFieldTypeCheck : IR::BailOutFailedEquivalentTypeCheck) :
                (isCheckFixedFld ? IR::BailOutFailedFixedFieldTypeCheck : IR::BailOutFailedTypeCheck);
            return true;
        }
        else
        {TRACE_IT(5829);
            *pIsTypeCheckProtected = propertySymOpnd->IsTypeCheckProtected();
            *pBailOutKind = IR::BailOutInvalid;
            return false;
        }
    }
    else
    {TRACE_IT(5830);
        Assert(instr->m_opcode != Js::OpCode::CheckFixedFld);
        *pBailOutKind = IR::BailOutInvalid;
        return false;
    }
}

bool
GlobOpt::MayNeedBailOnImplicitCall(const IR::Instr * instr, Value *src1Val, Value *src2Val)
{TRACE_IT(5831);
    if (!instr->HasAnyImplicitCalls())
    {TRACE_IT(5832);
        return false;
    }

    bool isLdElem = false;
    switch (instr->m_opcode)
    {
    case Js::OpCode::LdLen_A:
    {TRACE_IT(5833);
        const ValueType baseValueType(instr->GetSrc1()->GetValueType());
        return
            !(
                baseValueType.IsString() ||
                (baseValueType.IsAnyArray() && baseValueType.GetObjectType() != ObjectType::ObjectWithArray) ||
                (instr->HasBailOutInfo() && instr->GetBailOutKindNoBits() == IR::BailOutOnIrregularLength) // guarantees no implicit calls
            );
    }

    case Js::OpCode::LdElemI_A:
    case Js::OpCode::LdMethodElem:
    case Js::OpCode::InlineArrayPop:
        isLdElem = true;
        // fall-through

    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
    case Js::OpCode::InlineArrayPush:
    {TRACE_IT(5834);
        if(!instr->HasBailOutInfo())
        {TRACE_IT(5835);
            return true;
        }

        // The following bailout kinds already prevent implicit calls from happening. Any conditions that could trigger an
        // implicit call result in a pre-op bailout.
        const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
        return
            !(
                (bailOutKind & ~IR::BailOutKindBits) == IR::BailOutConventionalTypedArrayAccessOnly ||
                bailOutKind & IR::BailOutOnArrayAccessHelperCall ||
                (isLdElem && bailOutKind & IR::BailOutConventionalNativeArrayAccessOnly)
            );
    }

    default:
        break;
    }

    if (OpCodeAttr::HasImplicitCall(instr->m_opcode))
    {TRACE_IT(5836);
        // Operation has an implicit call regardless of operand attributes.
        return true;
    }

    IR::Opnd * opnd = instr->GetDst();

    if (opnd)
    {TRACE_IT(5837);
        switch (opnd->GetKind())
        {
        case IR::OpndKindReg:
            break;

        case IR::OpndKindSym:
            // No implicit call if we are just storing to a stack sym. Note that stores to non-configurable root
            // object fields may still need implicit call bailout. That's because a non-configurable field may still
            // become read-only and thus the store field will not take place (or throw in strict mode). Hence, we
            // can't optimize (e.g. copy prop) across such field stores.
            if (opnd->AsSymOpnd()->m_sym->IsStackSym())
            {TRACE_IT(5838);
                return false;
            }

            if (opnd->AsSymOpnd()->IsPropertySymOpnd())
            {TRACE_IT(5839);
                IR::PropertySymOpnd* propertySymOpnd = opnd->AsSymOpnd()->AsPropertySymOpnd();
                if (!propertySymOpnd->MayHaveImplicitCall())
                {TRACE_IT(5840);
                    return false;
                }
            }

            return true;

        case IR::OpndKindIndir:
            return true;

        default:
            Assume(UNREACHED);
        }
    }

    opnd = instr->GetSrc1();
    if (opnd != nullptr && MaySrcNeedBailOnImplicitCall(opnd, src1Val))
    {TRACE_IT(5841);
        return true;
    }
    opnd = instr->GetSrc2();
    if (opnd != nullptr && MaySrcNeedBailOnImplicitCall(opnd, src2Val))
    {TRACE_IT(5842);
        return true;
    }

    return false;
}

void
GlobOpt::GenerateBailAfterOperation(IR::Instr * *const pInstr, IR::BailOutKind kind)
{TRACE_IT(5843);
    Assert(pInstr);

    IR::Instr* instr = *pInstr;
    Assert(instr);

    IR::Instr * nextInstr = instr->GetNextRealInstrOrLabel();
    uint32 currentOffset = instr->GetByteCodeOffset();
    while (nextInstr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset ||
        nextInstr->GetByteCodeOffset() == currentOffset)
    {TRACE_IT(5844);
        nextInstr = nextInstr->GetNextRealInstrOrLabel();
    }
    IR::Instr * bailOutInstr = instr->ConvertToBailOutInstr(nextInstr, kind);
    if (this->currentBlock->GetLastInstr() == instr)
    {TRACE_IT(5845);
        this->currentBlock->SetLastInstr(bailOutInstr);
    }
    FillBailOutInfo(this->currentBlock, bailOutInstr->GetBailOutInfo());
    *pInstr = bailOutInstr;
}

void
GlobOpt::GenerateBailAtOperation(IR::Instr * *const pInstr, const IR::BailOutKind bailOutKind)
{TRACE_IT(5846);
    Assert(pInstr);

    IR::Instr * instr = *pInstr;
    Assert(instr);
    Assert(instr->GetByteCodeOffset() != Js::Constants::NoByteCodeOffset);
    Assert(bailOutKind != IR::BailOutInvalid);

    IR::Instr * bailOutInstr = instr->ConvertToBailOutInstr(instr, bailOutKind);
    if (this->currentBlock->GetLastInstr() == instr)
    {TRACE_IT(5847);
        this->currentBlock->SetLastInstr(bailOutInstr);
    }
    FillBailOutInfo(currentBlock, bailOutInstr->GetBailOutInfo());
    *pInstr = bailOutInstr;
}

IR::Instr *
GlobOpt::EnsureBailTarget(Loop * loop)
{TRACE_IT(5848);
    BailOutInfo * bailOutInfo = loop->bailOutInfo;
    IR::Instr * bailOutInstr = bailOutInfo->bailOutInstr;
    if (bailOutInstr == nullptr)
    {TRACE_IT(5849);
        bailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailTarget, IR::BailOutShared, bailOutInfo, bailOutInfo->bailOutFunc);
        loop->landingPad->InsertAfter(bailOutInstr);
    }
    return bailOutInstr;
}
