//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"
#include "Debug/DebuggingFlags.h"
#include "Debug/DiagProbe.h"
#include "Debug/DebugManager.h"
#include "Language/JavascriptFunctionArgIndex.h"

extern const IRType RegTypes[RegNumCount];

void
BailOutInfo::Clear(JitArenaAllocator * allocator)
{LOGMEIN("BailOut.cpp] 15\n");
    // Currently, we don't have a case where we delete bailout info after we allocated the bailout record
    Assert(!bailOutRecord);
    this->capturedValues.constantValues.Clear(allocator);
    this->capturedValues.copyPropSyms.Clear(allocator);
    this->usedCapturedValues.constantValues.Clear(allocator);
    this->usedCapturedValues.copyPropSyms.Clear(allocator);
    if (byteCodeUpwardExposedUsed)
    {
        JitAdelete(allocator, byteCodeUpwardExposedUsed);
    }
    if (startCallInfo)
    {LOGMEIN("BailOut.cpp] 27\n");
        Assert(argOutSyms);
        JitAdeleteArray(allocator, startCallCount, startCallInfo);
        JitAdeleteArray(allocator, totalOutParamCount, argOutSyms);
    }
    if (liveVarSyms)
    {
        JitAdelete(allocator, liveVarSyms);
        JitAdelete(allocator, liveLosslessInt32Syms);
        JitAdelete(allocator, liveFloat64Syms);
    }
#ifdef _M_IX86
    if (outParamFrameAdjustArgSlot)
    {
        JitAdelete(allocator, outParamFrameAdjustArgSlot);
    }
#endif
}

#ifdef _M_IX86

uint
BailOutInfo::GetStartCallOutParamCount(uint i) const
{LOGMEIN("BailOut.cpp] 50\n");
    Assert(i < this->startCallCount);
    Assert(this->startCallInfo);

    return this->startCallInfo[i].argCount;
}

bool
BailOutInfo::NeedsStartCallAdjust(uint i, const IR::Instr * bailOutInstr) const
{LOGMEIN("BailOut.cpp] 59\n");
    Assert(i < this->startCallCount);
    Assert(this->startCallInfo);
    Assert(bailOutInstr->m_func->HasInstrNumber());

    IR::Instr * instr = this->startCallInfo[i].instr;

    if (instr == nullptr || instr->m_opcode == Js::OpCode::StartCall)
    {LOGMEIN("BailOut.cpp] 67\n");
        // The StartCall was unlinked (because it was being deleted, or we know it was
        // moved below the bailout instr).
        // -------- or --------
        // StartCall wasn't lowered because the argouts were orphaned, in which case we don't need
        // the adjustment as the orphaned argouts are not stored with the non-orphaned ones
        return false;
    }

    // In scenarios related to partial polymorphic inlining where we move the lowered version of the start call -  LEA esp, esp - argcount * 4
    // next to the call itself as part of one of the dispatch arms. In this scenario StartCall is marked
    // as cloned and we do not need to adjust the offsets from where the args need to be restored.
    return instr->GetNumber() < bailOutInstr->GetNumber() && !instr->IsCloned();
}

void
BailOutInfo::RecordStartCallInfo(uint i, uint argRestoreAdjustCount, IR::Instr *instr)
{LOGMEIN("BailOut.cpp] 84\n");
    Assert(i < this->startCallCount);
    Assert(this->startCallInfo);
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::StartCall);

    this->startCallInfo[i].instr = instr;
    this->startCallInfo[i].argCount = instr->GetArgOutCount(/*getInterpreterArgOutCount*/ true);
    this->startCallInfo[i].argRestoreAdjustCount = argRestoreAdjustCount;
}

void
BailOutInfo::UnlinkStartCall(const IR::Instr * instr)
{LOGMEIN("BailOut.cpp] 97\n");
    Assert(this->startCallCount == 0 || this->startCallInfo != nullptr);

    uint i;
    for (i = 0; i < this->startCallCount; i++)
    {LOGMEIN("BailOut.cpp] 102\n");
        StartCallInfo *info = &this->startCallInfo[i];
        if (info->instr == instr)
        {LOGMEIN("BailOut.cpp] 105\n");
            info->instr = nullptr;
            return;
        }
    }
}

#else

uint
BailOutInfo::GetStartCallOutParamCount(uint i) const
{LOGMEIN("BailOut.cpp] 116\n");
    Assert(i < this->startCallCount);
    Assert(this->startCallInfo);

    return this->startCallInfo[i];
}

void
BailOutInfo::RecordStartCallInfo(uint i, uint argRestoreAdjust, IR::Instr *instr)
{LOGMEIN("BailOut.cpp] 125\n");
    Assert(i < this->startCallCount);
    Assert(this->startCallInfo);
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::StartCall);
    Assert(instr->GetSrc1());

    this->startCallInfo[i] = instr->GetArgOutCount(/*getInterpreterArgOutCount*/ true);
}

#endif

#ifdef MD_GROW_LOCALS_AREA_UP
void
BailOutInfo::FinalizeOffsets(__in_ecount(count) int * offsets, uint count, Func *func, BVSparse<JitArenaAllocator> *bvInlinedArgSlot)
{LOGMEIN("BailOut.cpp] 140\n");
    // Turn positive SP-relative sym offsets into negative frame-pointer-relative offsets for the convenience
    // of the restore-value logic.
    int32 inlineeArgStackSize = func->GetInlineeArgumentStackSize();
    int localsSize = func->m_localStackHeight + func->m_ArgumentsOffset;
    for (uint i = 0; i < count; i++)
    {LOGMEIN("BailOut.cpp] 146\n");
        int offset = -(offsets[i] + StackSymBias);
        if (offset < 0)
        {LOGMEIN("BailOut.cpp] 149\n");
            // Not stack offset
            continue;
        }
        if (bvInlinedArgSlot && bvInlinedArgSlot->Test(i))
        {LOGMEIN("BailOut.cpp] 154\n");
            // Inlined out param: the positive offset is relative to the start of the inlinee arg area,
            // so we need to subtract the full locals area (including the inlined-arg-area) to get the proper result.
            offset -= localsSize;
        }
        else
        {
            // The locals size contains the inlined-arg-area size, so remove the inlined-arg-area size from the
            // adjustment for normal locals whose offsets are relative to the start of the locals area.
            offset -= (localsSize - inlineeArgStackSize);
        }
        Assert(offset < 0);
        offsets[i] = offset;
    }
}
#endif

void
BailOutInfo::FinalizeBailOutRecord(Func * func)
{LOGMEIN("BailOut.cpp] 173\n");
    Assert(func->IsTopFunc());
    BailOutRecord * bailOutRecord = this->bailOutRecord;
    if (bailOutRecord == nullptr)
    {LOGMEIN("BailOut.cpp] 177\n");
        return;
    }

    BailOutRecord * currentBailOutRecord = bailOutRecord;
    Func * currentBailOutFunc = this->bailOutFunc;

    // Top of the inlined arg stack is at the beginning of the locals, find the offset from EBP+2
#ifdef MD_GROW_LOCALS_AREA_UP
    uint inlinedArgSlotAdjust = (func->m_localStackHeight + func->m_ArgumentsOffset);
#else
    uint inlinedArgSlotAdjust = (func->m_localStackHeight + (2 * MachPtr));
#endif

    while (currentBailOutRecord->parent != nullptr)
    {LOGMEIN("BailOut.cpp] 192\n");
        Assert(currentBailOutRecord->globalBailOutRecordTable->firstActualStackOffset == -1 ||
            currentBailOutRecord->globalBailOutRecordTable->firstActualStackOffset == (int32)(currentBailOutFunc->firstActualStackOffset - inlinedArgSlotAdjust));
        Assert(!currentBailOutFunc->IsTopFunc());
        Assert(currentBailOutFunc->firstActualStackOffset != -1);

        // Find the top of the locals on the stack from EBP
        currentBailOutRecord->globalBailOutRecordTable->firstActualStackOffset = currentBailOutFunc->firstActualStackOffset - inlinedArgSlotAdjust;

        currentBailOutRecord = currentBailOutRecord->parent;
        currentBailOutFunc = currentBailOutFunc->GetParentFunc();
    }
    Assert(currentBailOutRecord->globalBailOutRecordTable->firstActualStackOffset == -1);
    Assert(currentBailOutFunc->IsTopFunc());
    Assert(currentBailOutFunc->firstActualStackOffset == -1);

#ifndef MD_GROW_LOCALS_AREA_UP
    if (this->totalOutParamCount != 0)
    {LOGMEIN("BailOut.cpp] 210\n");
        if (func->HasInlinee())
        {
            FOREACH_BITSET_IN_SPARSEBV(index, this->outParamInlinedArgSlot)
            {LOGMEIN("BailOut.cpp] 214\n");
                this->outParamOffsets[index] -= inlinedArgSlotAdjust;
            }
            NEXT_BITSET_IN_SPARSEBV;
        }

#ifdef _M_IX86
        int frameSize = func->frameSize;
        AssertMsg(frameSize != 0, "Frame size not calculated");

        FOREACH_BITSET_IN_SPARSEBV(index, this->outParamFrameAdjustArgSlot)
        {LOGMEIN("BailOut.cpp] 225\n");
            this->outParamOffsets[index] -= frameSize;
        }
        NEXT_BITSET_IN_SPARSEBV;
#endif
    }
#else
    if (func->IsJitInDebugMode())
    {LOGMEIN("BailOut.cpp] 233\n");
        // Turn positive SP-relative base locals offset into negative frame-pointer-relative offset
        func->AjustLocalVarSlotOffset();
    }

    currentBailOutRecord = bailOutRecord;
    int32 inlineeArgStackSize = func->GetInlineeArgumentStackSize();
    do
    {LOGMEIN("BailOut.cpp] 241\n");
        // Note: do this only once
        currentBailOutRecord->globalBailOutRecordTable->VisitGlobalBailOutRecordTableRowsAtFirstBailOut(
          currentBailOutRecord->m_bailOutRecordId, [=](GlobalBailOutRecordDataRow *row) {
            int offset = -(row->offset + StackSymBias);
            if (offset < 0)
            {LOGMEIN("BailOut.cpp] 247\n");
                // Not stack offset
                return;
            }
            // The locals size contains the inlined-arg-area size, so remove the inlined-arg-area size from the
            // adjustment for normal locals whose offsets are relative to the start of the locals area.
            offset -= (inlinedArgSlotAdjust - inlineeArgStackSize);
            Assert(offset < 0);
            row->offset = offset;
        });

        // Only adjust once
        int forInEnumeratorArrayRestoreOffset = currentBailOutRecord->globalBailOutRecordTable->forInEnumeratorArrayRestoreOffset;
        if (forInEnumeratorArrayRestoreOffset >= 0)
        {LOGMEIN("BailOut.cpp] 261\n");
            forInEnumeratorArrayRestoreOffset -= (inlinedArgSlotAdjust - inlineeArgStackSize);
            Assert(forInEnumeratorArrayRestoreOffset < 0);
            currentBailOutRecord->globalBailOutRecordTable->forInEnumeratorArrayRestoreOffset = forInEnumeratorArrayRestoreOffset;
        }
        currentBailOutRecord = currentBailOutRecord->parent;
    }
    while (currentBailOutRecord != nullptr);
    this->FinalizeOffsets(this->outParamOffsets, this->totalOutParamCount, func, func->HasInlinee() ? this->outParamInlinedArgSlot : nullptr);

#endif
    // set the bailOutRecord to null so we don't adjust it again if the info is shared
    bailOutRecord = nullptr;
}

#if DBG
bool
BailOutInfo::IsBailOutHelper(IR::JnHelperMethod helper)
{LOGMEIN("BailOut.cpp] 279\n");
    switch (helper)
    {LOGMEIN("BailOut.cpp] 281\n");
    case IR::HelperSaveAllRegistersAndBailOut:
    case IR::HelperSaveAllRegistersAndBranchBailOut:
#ifdef _M_IX86
    case IR::HelperSaveAllRegistersNoSse2AndBailOut:
    case IR::HelperSaveAllRegistersNoSse2AndBranchBailOut:
#endif
        return true;
    };

    return false;
};
#endif

//===================================================================================================================================
// BailOutRecord
//===================================================================================================================================
BailOutRecord::BailOutRecord(uint32 bailOutOffset, uint bailOutCacheIndex, IR::BailOutKind kind, Func * bailOutFunc) :
    argOutOffsetInfo(nullptr), bailOutOffset(bailOutOffset),
    bailOutCount(0), polymorphicCacheIndex(bailOutCacheIndex), bailOutKind(kind),
    branchValueRegSlot(Js::Constants::NoRegister),
    ehBailoutData(nullptr), m_bailOutRecordId(0), type(Normal)
#if DBG
    , inlineDepth(0)
#endif
{
    CompileAssert(offsetof(BailOutRecord, globalBailOutRecordTable) == 0); // the offset is hard-coded in LinearScanMD::SaveAllRegisters
    CompileAssert(offsetof(GlobalBailOutRecordDataTable, registerSaveSpace) == 0); // the offset is hard-coded in LinearScanMD::SaveAllRegisters}
    Assert(bailOutOffset != Js::Constants::NoByteCodeOffset);

#if DBG
    actualCount = bailOutFunc->actualCount;
    Assert(bailOutFunc->IsTopFunc() || actualCount != -1);
#endif
}

#if ENABLE_DEBUG_CONFIG_OPTIONS
#define REJIT_KIND_TESTTRACE(bailOutKind, ...) \
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::ReJITPhase)) \
    {LOGMEIN("BailOut.cpp] 320\n"); \
        if (Js::Configuration::Global.flags.RejitTraceFilter.Empty() || Js::Configuration::Global.flags.RejitTraceFilter.Contains(bailOutKind)) \
        {LOGMEIN("BailOut.cpp] 322\n"); \
            Output::Print(__VA_ARGS__); \
            Output::Flush(); \
        } \
    }
const char16 * const trueString = _u("true");
const char16 * const falseString = _u("false");
#else
#define REJIT_KIND_TESTTRACE(...)
#endif

#if ENABLE_DEBUG_CONFIG_OPTIONS
#define BAILOUT_KIND_TRACE(functionBody, bailOutKind, ...) \
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::BailOutPhase, functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId()) && \
        ((bailOutKind) != IR::BailOnSimpleJitToFullJitLoopBody || CONFIG_FLAG(Verbose))) \
    {LOGMEIN("BailOut.cpp] 337\n"); \
        if (Js::Configuration::Global.flags.BailoutTraceFilter.Empty() || Js::Configuration::Global.flags.BailoutTraceFilter.Contains(bailOutKind)) \
        {LOGMEIN("BailOut.cpp] 339\n"); \
            Output::Print(__VA_ARGS__); \
            if (bailOutKind != IR::BailOutInvalid) \
            {LOGMEIN("BailOut.cpp] 342\n"); \
                Output::Print(_u(" Kind: %S"), ::GetBailOutKindName(bailOutKind)); \
            } \
            Output::Print(_u("\n")); \
        } \
    }

#define BAILOUT_VERBOSE_TRACE(functionBody, bailOutKind, ...) \
    if (Js::Configuration::Global.flags.Verbose && Js::Configuration::Global.flags.Trace.IsEnabled(Js::BailOutPhase,functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId())) \
    {LOGMEIN("BailOut.cpp] 351\n"); \
        if (Js::Configuration::Global.flags.BailoutTraceFilter.Empty() || Js::Configuration::Global.flags.BailoutTraceFilter.Contains(bailOutKind)) \
        {LOGMEIN("BailOut.cpp] 353\n"); \
            Output::Print(__VA_ARGS__); \
        } \
    }

#define BAILOUT_TESTTRACE(functionBody, bailOutKind, ...) \
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::BailOutPhase, functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId()) && \
        ((bailOutKind) != IR::BailOnSimpleJitToFullJitLoopBody || CONFIG_FLAG(Verbose))) \
    {LOGMEIN("BailOut.cpp] 361\n"); \
        if (Js::Configuration::Global.flags.BailoutTraceFilter.Empty() || Js::Configuration::Global.flags.BailoutTraceFilter.Contains(bailOutKind)) \
        {LOGMEIN("BailOut.cpp] 363\n"); \
            Output::Print(__VA_ARGS__); \
        } \
    }

#define BAILOUT_FLUSH(functionBody) \
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::BailOutPhase, functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId()) || \
    Js::Configuration::Global.flags.Trace.IsEnabled(Js::BailOutPhase, functionBody->GetSourceContextId(),functionBody->GetLocalFunctionId())) \
    {LOGMEIN("BailOut.cpp] 371\n"); \
        Output::Flush(); \
    }
#else
#define BAILOUT_KIND_TRACE(functionBody, bailOutKind, ...)
#define BAILOUT_TESTTRACE(functionBody, bailOutKind, ...)
#define BAILOUT_VERBOSE_TRACE(functionBody, bailOutKind, ...)
#define BAILOUT_FLUSH(functionBody)
#endif

#if DBG
void BailOutRecord::DumpArgOffsets(uint count, int* offsets, int argOutSlotStart)
{LOGMEIN("BailOut.cpp] 383\n");
    char16 const * name = _u("OutParam");
    Js::RegSlot regSlotOffset = 0;

    for (uint i = 0; i < count; i++)
    {LOGMEIN("BailOut.cpp] 388\n");
        int offset = offsets[i];

        // The variables below determine whether we have a Var or native float/int.
        bool isFloat64 = this->argOutOffsetInfo->argOutFloat64Syms->Test(argOutSlotStart + i) != 0;
        bool isInt32 = this->argOutOffsetInfo->argOutLosslessInt32Syms->Test(argOutSlotStart + i) != 0;

        // SIMD_JS
        // Simd128 reside in Float64 regs
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128F4Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128I4Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128I8Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128I16Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128U4Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128U8Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128U16Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128B4Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128B8Syms->Test(argOutSlotStart + i) != 0;
        isFloat64 |= this->argOutOffsetInfo->argOutSimd128B16Syms->Test(argOutSlotStart + i) != 0;

        Assert(!isFloat64 || !isInt32);

        Output::Print(_u("%s #%3d: "), name, i + regSlotOffset);
        this->DumpValue(offset, isFloat64);
        Output::Print(_u("\n"));
    }
}

void BailOutRecord::DumpLocalOffsets(uint count, int argOutSlotStart)
{LOGMEIN("BailOut.cpp] 417\n");
    char16 const * name = _u("Register");
    globalBailOutRecordTable->IterateGlobalBailOutRecordTableRows(m_bailOutRecordId, [=](GlobalBailOutRecordDataRow *row) {
        Assert(row != nullptr);

        // The variables below determine whether we have a Var or native float/int.
        bool isFloat64 = row->isFloat;
        bool isInt32 = row->isInt;

        // SIMD_JS
        // Simd values are in float64 regs
        isFloat64 = isFloat64 || row->isSimd128F4;
        isFloat64 = isFloat64 || row->isSimd128I4;
        isFloat64 = isFloat64 || row->isSimd128I8;
        isFloat64 = isFloat64 || row->isSimd128I16;
        isFloat64 = isFloat64 || row->isSimd128U4;
        isFloat64 = isFloat64 || row->isSimd128U8;
        isFloat64 = isFloat64 || row->isSimd128U16;
        isFloat64 = isFloat64 || row->isSimd128B4;
        isFloat64 = isFloat64 || row->isSimd128B8;
        isFloat64 = isFloat64 || row->isSimd128B16;

        Assert(!isFloat64 || !isInt32);

        Output::Print(_u("%s #%3d: "), name, row->regSlot);
        this->DumpValue(row->offset, isFloat64);
        Output::Print(_u("\n"));
    });
}

void BailOutRecord::DumpValue(int offset, bool isFloat64)
{LOGMEIN("BailOut.cpp] 448\n");
    if (offset < 0)
    {LOGMEIN("BailOut.cpp] 450\n");
        Output::Print(_u("Stack offset %6d"),  offset);
    }
    else if (offset > 0)
    {LOGMEIN("BailOut.cpp] 454\n");
        if ((uint)offset <= GetBailOutRegisterSaveSlotCount())
        {LOGMEIN("BailOut.cpp] 456\n");
            if (isFloat64)
            {LOGMEIN("BailOut.cpp] 458\n");
#ifdef _M_ARM
                Output::Print(_u("Register %-4S  %4d"), RegNames[(offset - RegD0) / 2  + RegD0], offset);
#else
                Output::Print(_u("Register %-4S  %4d"), RegNames[offset], offset);
#endif
            }
            else
            {
                Output::Print(_u("Register %-4S  %4d"), RegNames[offset], offset);
            }
        }
        else if (BailOutRecord::IsArgumentsObject((uint)offset))
        {LOGMEIN("BailOut.cpp] 471\n");
            Output::Print(_u("Arguments object"));
        }
        else
        {
            // Constants offset starts from max bail out register save slot count
            uint constantIndex = offset - (GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount()) - 1;
            Output::Print(_u("Constant index %4d value:0x%p (Var)"), constantIndex, this->constants[constantIndex]);
            Assert(!isFloat64);
        }
    }
    else
    {
        Output::Print(_u("Not live"));
    }
}

void BailOutRecord::Dump()
{LOGMEIN("BailOut.cpp] 489\n");
    if (this->localOffsetsCount)
    {LOGMEIN("BailOut.cpp] 491\n");
        Output::Print(_u("**** Locals ***\n"));
        DumpLocalOffsets(this->localOffsetsCount, 0);
    }

    uint outParamSlot = 0;
    if(this->argOutOffsetInfo)
    {LOGMEIN("BailOut.cpp] 498\n");
        Output::Print(_u("**** Out params ***\n"));
        for (uint i = 0; i < this->argOutOffsetInfo->startCallCount; i++)
        {LOGMEIN("BailOut.cpp] 501\n");
            uint startCallOutParamCount = this->argOutOffsetInfo->startCallOutParamCounts[i];
            DumpArgOffsets(startCallOutParamCount, &this->argOutOffsetInfo->outParamOffsets[outParamSlot], this->argOutOffsetInfo->argOutSymStart + outParamSlot);
            outParamSlot += startCallOutParamCount;
        }
    }
}
#endif

/*static*/
bool BailOutRecord::IsArgumentsObject(uint32 offset)
{LOGMEIN("BailOut.cpp] 512\n");
    bool isArgumentsObject = (GetArgumentsObjectOffset() == offset);
    return isArgumentsObject;
}

/*static*/
uint32 BailOutRecord::GetArgumentsObjectOffset()
{LOGMEIN("BailOut.cpp] 519\n");
    uint32 argumentsObjectOffset = (GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount());
    return argumentsObjectOffset;
}

Js::Var BailOutRecord::EnsureArguments(Js::InterpreterStackFrame * newInstance, Js::JavascriptCallStackLayout * layout, Js::ScriptContext* scriptContext, Js::Var* pArgumentsObject) const
{LOGMEIN("BailOut.cpp] 525\n");
    Assert(globalBailOutRecordTable->hasStackArgOpt);
    if (PHASE_OFF1(Js::StackArgFormalsOptPhase))
    {LOGMEIN("BailOut.cpp] 528\n");
        newInstance->OP_LdHeapArguments(scriptContext);
    }
    else
    {
        newInstance->CreateEmptyHeapArgumentsObject(scriptContext);
    }

    Assert(newInstance->m_arguments);
    *pArgumentsObject = (Js::ArgumentsObject*)newInstance->m_arguments;
    return newInstance->m_arguments;
}

Js::JavascriptCallStackLayout *BailOutRecord::GetStackLayout() const
{LOGMEIN("BailOut.cpp] 542\n");
    return
        Js::JavascriptCallStackLayout::FromFramePointer(
            globalBailOutRecordTable->registerSaveSpace[LinearScanMD::GetRegisterSaveIndex(LowererMD::GetRegFramePointer()) - 1]);
}

void
BailOutRecord::RestoreValues(IR::BailOutKind bailOutKind, Js::JavascriptCallStackLayout * layout, Js::InterpreterStackFrame * newInstance,
    Js::ScriptContext * scriptContext, bool fromLoopBody, Js::Var * registerSaves, BailOutReturnValue * bailOutReturnValue, Js::Var* pArgumentsObject,
    Js::Var branchValue, void * returnAddress, bool useStartCall /* = true */, void * argoutRestoreAddress) const
{LOGMEIN("BailOut.cpp] 552\n");
    Js::AutoPushReturnAddressForStackWalker saveReturnAddress(scriptContext, returnAddress);

    if (this->stackLiteralBailOutRecordCount)
    {LOGMEIN("BailOut.cpp] 556\n");
        // Null out the field on the stack literal that hasn't fully initialized yet.

        globalBailOutRecordTable->IterateGlobalBailOutRecordTableRows(m_bailOutRecordId, [=](GlobalBailOutRecordDataRow  *row)
        {
            for (uint i = 0; i < this->stackLiteralBailOutRecordCount; i++)
            {LOGMEIN("BailOut.cpp] 562\n");
                BailOutRecord::StackLiteralBailOutRecord& record = this->stackLiteralBailOutRecord[i];

                if (record.regSlot == row->regSlot)
                {LOGMEIN("BailOut.cpp] 566\n");
                    // Partially initialized stack literal shouldn't be type specialized yet.
                    Assert(!row->isFloat);
                    Assert(!row->isInt);

                    int offset = row->offset;
                    Js::Var value;
                    if (offset < 0)
                    {LOGMEIN("BailOut.cpp] 574\n");
                        // Stack offset
                        value = layout->GetOffset(offset);
                    }
                    else
                    {
                        // The value is in register
                        // Index is one based, so subtract one
                        Assert((uint)offset <= GetBailOutRegisterSaveSlotCount());
                        Js::Var * registerSaveSpace = registerSaves ? registerSaves : (Js::Var *)scriptContext->GetThreadContext()->GetBailOutRegisterSaveSpace();
                        Assert(RegTypes[LinearScanMD::GetRegisterFromSaveIndex(offset)] != TyFloat64);
                        value = registerSaveSpace[offset - 1];
                    }
                    Assert(Js::DynamicObject::Is(value));
                    Assert(ThreadContext::IsOnStack(value));

                    Js::DynamicObject * obj = Js::DynamicObject::FromVar(value);
                    uint propertyCount = obj->GetPropertyCount();
                    for (uint j = record.initFldCount; j < propertyCount; j++)
                    {LOGMEIN("BailOut.cpp] 593\n");
                        obj->SetSlot(SetSlotArgumentsRoot(Js::Constants::NoProperty, false, j, nullptr));
                    }
                }
            }
        });
    }

    if (this->localOffsetsCount)
    {LOGMEIN("BailOut.cpp] 602\n");
        Js::FunctionBody* functionBody = newInstance->function->GetFunctionBody();
#if ENABLE_DEBUG_CONFIG_OPTIONS
        BAILOUT_VERBOSE_TRACE(functionBody, bailOutKind, _u("BailOut:   Register #%3d: Not live\n"), 0);

        for (uint i = 1; i < functionBody->GetConstantCount(); i++)
        {
            BAILOUT_VERBOSE_TRACE(functionBody, bailOutKind, _u("BailOut:   Register #%3d: Constant table\n"), i);
        }
#endif

        if (functionBody->IsInDebugMode())
        {LOGMEIN("BailOut.cpp] 614\n");
            this->AdjustOffsetsForDiagMode(layout, newInstance->GetJavascriptFunction());
        }

        this->RestoreValues(bailOutKind, layout, this->localOffsetsCount,
            nullptr, 0, newInstance->m_localSlots, scriptContext, fromLoopBody, registerSaves, newInstance, pArgumentsObject);
    }

    if (useStartCall && this->argOutOffsetInfo)
    {LOGMEIN("BailOut.cpp] 623\n");
        uint outParamSlot = 0;
        void * argRestoreAddr = nullptr;
        for (uint i = 0; i < this->argOutOffsetInfo->startCallCount; i++)
        {LOGMEIN("BailOut.cpp] 627\n");
            uint startCallOutParamCount = this->argOutOffsetInfo->startCallOutParamCounts[i];
#ifdef _M_IX86
            if (argoutRestoreAddress)
            {LOGMEIN("BailOut.cpp] 631\n");
                argRestoreAddr = (void*)((char*)argoutRestoreAddress + (this->startCallArgRestoreAdjustCounts[i] * MachPtr));
            }
#endif
            newInstance->OP_StartCall(startCallOutParamCount);
            this->RestoreValues(bailOutKind, layout, startCallOutParamCount, &this->argOutOffsetInfo->outParamOffsets[outParamSlot],
                this->argOutOffsetInfo->argOutSymStart + outParamSlot, newInstance->m_outParams,
                scriptContext, fromLoopBody, registerSaves, newInstance, pArgumentsObject, argRestoreAddr);
            outParamSlot += startCallOutParamCount;
        }
    }

    // If we're not in a loop body, then the arguments object is not on the local frame.
    // If the RestoreValues created an arguments object for us, then it's already on the interpreter instance.
    // Otherwise, we need to propagate the object from the jitted frame to the interpreter.
    Assert(newInstance->function && newInstance->function->GetFunctionBody());
    bool hasArgumentSlot =      // Be consistent with Func::HasArgumentSlot.
        !fromLoopBody && newInstance->function->GetFunctionBody()->GetInParamsCount() != 0;
    if (hasArgumentSlot && newInstance->m_arguments == nullptr)
    {LOGMEIN("BailOut.cpp] 650\n");
        newInstance->m_arguments = *pArgumentsObject;
    }

    if (bailOutReturnValue != nullptr && bailOutReturnValue->returnValueRegSlot != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 655\n");
        Assert(bailOutReturnValue->returnValue != nullptr);
        Assert(bailOutReturnValue->returnValueRegSlot < newInstance->GetJavascriptFunction()->GetFunctionBody()->GetLocalsCount());
        newInstance->m_localSlots[bailOutReturnValue->returnValueRegSlot] = bailOutReturnValue->returnValue;

        BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("BailOut:   Register #%3d: Return, value: 0x%p\n"),
            bailOutReturnValue->returnValueRegSlot, bailOutReturnValue->returnValue);
    }

    if (branchValueRegSlot != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 665\n");
        // Used when a t1 = CmCC is optimize to BrCC, and the branch bails out. T1 needs to be restored
        Assert(branchValue && Js::JavascriptBoolean::Is(branchValue));
        Assert(branchValueRegSlot < newInstance->GetJavascriptFunction()->GetFunctionBody()->GetLocalsCount());
        newInstance->m_localSlots[branchValueRegSlot] = branchValue;
    }

#if DBG
    // Clear the register save area for the next bailout
    memset((void*)scriptContext->GetThreadContext()->GetBailOutRegisterSaveSpace(), 0, GetBailOutRegisterSaveSlotCount() * sizeof(Js::Var));
#endif
}

void
BailOutRecord::AdjustOffsetsForDiagMode(Js::JavascriptCallStackLayout * layout, Js::ScriptFunction * function) const
{LOGMEIN("BailOut.cpp] 680\n");
    // In this function we are going to do
    // 1. Check if the value got changed (by checking at the particular location at the stack)
    // 2. In that case update the offset to point to the stack offset

    Js::FunctionBody *functionBody =  function->GetFunctionBody();
    Assert(functionBody != nullptr);

    Assert(functionBody->IsInDebugMode());

    Js::FunctionEntryPointInfo *entryPointInfo = functionBody->GetDefaultFunctionEntryPointInfo();
    Assert(entryPointInfo != nullptr);

    // Note: the offset may be not initialized/InvalidOffset when there are no non-temp local vars.
    if (entryPointInfo->localVarChangedOffset != Js::Constants::InvalidOffset)
    {LOGMEIN("BailOut.cpp] 695\n");
        Assert(functionBody->GetNonTempLocalVarCount() != 0);

        char * valueChangeOffset = layout->GetValueChangeOffset(entryPointInfo->localVarChangedOffset);
        if (*valueChangeOffset == Js::FunctionBody::LocalsChangeDirtyValue)
        {LOGMEIN("BailOut.cpp] 700\n");
            // The value got changed due to debugger, lets read values from the stack position
            // Get the corresponding offset on the stack related to the frame.

            globalBailOutRecordTable->IterateGlobalBailOutRecordTableRows(m_bailOutRecordId, [=](GlobalBailOutRecordDataRow *row) {
                int32 offset = row->offset;
                // offset is zero, is it possible that a locals is not living in the debug mode?
                Assert(offset != 0);
                int32 slotOffset;
                if (functionBody->GetSlotOffset(row->regSlot, &slotOffset))
                {LOGMEIN("BailOut.cpp] 710\n");
                    slotOffset = entryPointInfo->localVarSlotsOffset + slotOffset;
                    // If it was taken from the stack location, we should have arrived to the same stack location.
                    Assert(offset > 0 || offset == slotOffset);
                    row->offset = slotOffset;
                }
            });
        }
    }
}

void
BailOutRecord::IsOffsetNativeIntOrFloat(uint offsetIndex, int argOutSlotStart, bool * pIsFloat64, bool * pIsInt32,
    bool * pIsSimd128F4, bool * pIsSimd128I4, bool * pIsSimd128I8, bool * pIsSimd128I16, bool * pIsSimd128U4,
    bool * pIsSimd128U8, bool * pIsSimd128U16, bool * pIsSimd128B4, bool * pIsSimd128B8, bool * pIsSimd128B16) const
{LOGMEIN("BailOut.cpp] 725\n");
    bool isFloat64 = this->argOutOffsetInfo->argOutFloat64Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isInt32 = this->argOutOffsetInfo->argOutLosslessInt32Syms->Test(argOutSlotStart + offsetIndex) != 0;
    // SIMD_JS
    bool isSimd128F4    = this->argOutOffsetInfo->argOutSimd128F4Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128I4    = this->argOutOffsetInfo->argOutSimd128I4Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128I8    = this->argOutOffsetInfo->argOutSimd128I8Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128I16   = this->argOutOffsetInfo->argOutSimd128I16Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128U4    = this->argOutOffsetInfo->argOutSimd128U4Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128U8    = this->argOutOffsetInfo->argOutSimd128U8Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128U16   = this->argOutOffsetInfo->argOutSimd128U16Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128B4    = this->argOutOffsetInfo->argOutSimd128B4Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128B8    = this->argOutOffsetInfo->argOutSimd128B8Syms->Test(argOutSlotStart + offsetIndex) != 0;
    bool isSimd128B16   = this->argOutOffsetInfo->argOutSimd128B16Syms->Test(argOutSlotStart + offsetIndex) != 0;

    Assert(!isFloat64 || !isInt32 ||
        !isSimd128F4 || !isSimd128I4 || !isSimd128I8 || !isSimd128I16 || !
        !isSimd128U4 || !isSimd128U8 || !isSimd128U16);

    *pIsFloat64 = isFloat64;
    *pIsInt32 = isInt32;
    *pIsSimd128F4 = isSimd128F4;
    *pIsSimd128I4 = isSimd128I4;
    *pIsSimd128I8 = isSimd128I8;
    *pIsSimd128I16 = isSimd128I16;
    *pIsSimd128U4 = isSimd128U4;
    *pIsSimd128U8 = isSimd128U8;
    *pIsSimd128U16 = isSimd128U16;
    *pIsSimd128B4 = isSimd128B4;
    *pIsSimd128B8 = isSimd128B8;
    *pIsSimd128B16 = isSimd128B16;
}

void
BailOutRecord::RestoreValue(IR::BailOutKind bailOutKind, Js::JavascriptCallStackLayout * layout, Js::Var * values, Js::ScriptContext * scriptContext,
    bool fromLoopBody, Js::Var * registerSaves, Js::InterpreterStackFrame * newInstance, Js::Var* pArgumentsObject, void * argoutRestoreAddress,
    uint regSlot, int offset, bool isLocal, bool isFloat64, bool isInt32,
    bool isSimd128F4, bool isSimd128I4, bool isSimd128I8, bool isSimd128I16,
    bool isSimd128U4, bool isSimd128U8, bool isSimd128U16, bool isSimd128B4, bool isSimd128B8, bool isSimd128B16) const
{LOGMEIN("BailOut.cpp] 764\n");
    bool boxStackInstance = true;
    Js::Var value = 0;
    double dblValue = 0.0;
    int32 int32Value = 0;
    SIMDValue simdValue = { 0, 0, 0, 0 };

#if ENABLE_DEBUG_CONFIG_OPTIONS
    char16 const * name = _u("OutParam");
    if (isLocal)
    {LOGMEIN("BailOut.cpp] 774\n");
        name = _u("Register");
    }
#endif

    BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("BailOut:   %s #%3d: "), name, regSlot);

    if (offset < 0)
    {LOGMEIN("BailOut.cpp] 782\n");
        // Stack offset are negative
        if (!argoutRestoreAddress)
        {LOGMEIN("BailOut.cpp] 785\n");
            if (isFloat64)
            {LOGMEIN("BailOut.cpp] 787\n");
                dblValue = layout->GetDoubleAtOffset(offset);
            }
            else if (isInt32)
            {LOGMEIN("BailOut.cpp] 791\n");
                int32Value = layout->GetInt32AtOffset(offset);
            }
            else if (
                    isSimd128F4 || isSimd128I4 || isSimd128I8 || isSimd128I16 ||
                    isSimd128U4 || isSimd128U8 || isSimd128U16 || isSimd128B4 ||
                    isSimd128B8 || isSimd128B16
                    )
            {LOGMEIN("BailOut.cpp] 799\n");
                // SIMD_JS
                simdValue = layout->GetSimdValueAtOffset(offset);
            }
            else
            {
                value = layout->GetOffset(offset);
                AssertMsg(!(newInstance->function->GetFunctionBody()->IsInDebugMode() &&
                    newInstance->function->GetFunctionBody()->IsNonTempLocalVar(regSlot) &&
                    value == (Js::Var)Func::c_debugFillPattern),
                    "Uninitialized value (debug mode only)? Try -trace:bailout -verbose and check last traced reg in byte code.");
            }
        }
        else if (!isLocal)
        {LOGMEIN("BailOut.cpp] 813\n");
            // If we have:
            // try {
            //      bar(a, b, c);
            // } catch(..) {..}
            // and we bailout during bar args evaluation, we recover from args from argoutRestoreAddress, not from caller function frame.
            // This is because try-catch is implemented as a C wrapper, so args will be a different offset from rbp in that case.
            Assert(
                   !isFloat64 && !isInt32 &&
                   !isSimd128F4 && !isSimd128I4 && !isSimd128I8 && !isSimd128I16 && !isSimd128U4 &&
                   !isSimd128U8 && !isSimd128U16 && !isSimd128B4 && !isSimd128B8 && !isSimd128B16
                  );

            value = *((Js::Var *)(((char *)argoutRestoreAddress) + regSlot * MachPtr));
            AssertMsg(!(newInstance->function->GetFunctionBody()->IsInDebugMode() &&
                newInstance->function->GetFunctionBody()->IsNonTempLocalVar(regSlot) &&
                value == (Js::Var)Func::c_debugFillPattern),
                "Uninitialized value (debug mode only)? Try -trace:bailout -verbose and check last traced reg in byte code.");

        }

        BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("Stack offset %6d"), offset);
    }
    else if (offset > 0)
    {LOGMEIN("BailOut.cpp] 837\n");
        if ((uint)offset <= GetBailOutRegisterSaveSlotCount())
        {LOGMEIN("BailOut.cpp] 839\n");
            // Register save space (offset is the register number and index into the register save space)
            // Index is one based, so subtract one
            Js::Var * registerSaveSpace = registerSaves ? registerSaves : (Js::Var *)scriptContext->GetThreadContext()->GetBailOutRegisterSaveSpace();

            if (isFloat64)
            {LOGMEIN("BailOut.cpp] 845\n");
                Assert(RegTypes[LinearScanMD::GetRegisterFromSaveIndex(offset)] == TyFloat64);
                dblValue = *((double*)&(registerSaveSpace[offset - 1]));
#ifdef _M_ARM
                BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("Register %-4S  %4d"), RegNames[(offset - RegD0) / 2 + RegD0], offset);
#else
                BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("Register %-4S  %4d"), RegNames[LinearScanMD::GetRegisterFromSaveIndex(offset)], offset);
#endif
            }
            else
            {
                if (
                    isSimd128F4 || isSimd128I4 || isSimd128I8 || isSimd128I16 ||
                    isSimd128U4 || isSimd128U8 || isSimd128U16 || isSimd128B4 ||
                    isSimd128B8 || isSimd128B16
                   )
                {LOGMEIN("BailOut.cpp] 861\n");
                    simdValue = *((SIMDValue *)&(registerSaveSpace[offset - 1]));
                }
                else if (isInt32)
                {LOGMEIN("BailOut.cpp] 865\n");
                    Assert(RegTypes[LinearScanMD::GetRegisterFromSaveIndex(offset)] != TyFloat64);
                    int32Value = ::Math::PointerCastToIntegralTruncate<int32>(registerSaveSpace[offset - 1]);
                }
                else
                {
                    Assert(RegTypes[LinearScanMD::GetRegisterFromSaveIndex(offset)] != TyFloat64);
                    value = registerSaveSpace[offset - 1];
                }

                BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("Register %-4S  %4d"), RegNames[LinearScanMD::GetRegisterFromSaveIndex(offset)], offset);
            }
        }
        else if (BailOutRecord::IsArgumentsObject((uint)offset))
        {LOGMEIN("BailOut.cpp] 879\n");
            Assert(!isFloat64);
            Assert(!isInt32);
            Assert(!fromLoopBody);
            value = *pArgumentsObject;
            if (value == nullptr)
            {LOGMEIN("BailOut.cpp] 885\n");
                value = EnsureArguments(newInstance, layout, scriptContext, pArgumentsObject);
            }
            Assert(value);
            BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("Arguments object"));
            boxStackInstance = false;
        }
        else
        {
            // Constants offset starts from max bail out register save slot count;
            uint constantIndex = offset - (GetBailOutRegisterSaveSlotCount() + GetBailOutReserveSlotCount()) - 1;
            if (isInt32)
            {LOGMEIN("BailOut.cpp] 897\n");
                int32Value = ::Math::PointerCastToIntegralTruncate<int32>(this->constants[constantIndex]);
            }
            else
            {
                value = this->constants[constantIndex];
            }
            BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("Constant index %4d"), constantIndex);
            boxStackInstance = false;
        }
    }
    else
    {
        // Consider Assert(false) here
        BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("Not live\n"));
        return;
    }

    if (isFloat64)
    {LOGMEIN("BailOut.cpp] 916\n");
        value = Js::JavascriptNumber::New(dblValue, scriptContext);
        BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u(", value: %f (ToVar: 0x%p)"), dblValue, value);
    }
    else if (isInt32)
    {LOGMEIN("BailOut.cpp] 921\n");
        Assert(!value);
        value = Js::JavascriptNumber::ToVar(int32Value, scriptContext);
        BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u(", value: %10d (ToVar: 0x%p)"), int32Value, value);
    }
    // SIMD_JS
    else if (isSimd128F4)
    {LOGMEIN("BailOut.cpp] 928\n");
        Assert(!value);
        value = Js::JavascriptSIMDFloat32x4::New(&simdValue, scriptContext);
    }
    else if (isSimd128I4)
    {LOGMEIN("BailOut.cpp] 933\n");
        Assert(!value);
        value = Js::JavascriptSIMDInt32x4::New(&simdValue, scriptContext);
    }
    else if (isSimd128I8)
    {LOGMEIN("BailOut.cpp] 938\n");
        Assert(!value);
        value = Js::JavascriptSIMDInt16x8::New(&simdValue, scriptContext);
    }
    else if (isSimd128I16)
    {LOGMEIN("BailOut.cpp] 943\n");
        Assert(!value);
        value = Js::JavascriptSIMDInt8x16::New(&simdValue, scriptContext);
    }
    else if (isSimd128U4)
    {LOGMEIN("BailOut.cpp] 948\n");
        Assert(!value);
        value = Js::JavascriptSIMDUint32x4::New(&simdValue, scriptContext);
    }
    else if (isSimd128U8)
    {LOGMEIN("BailOut.cpp] 953\n");
        Assert(!value);
        value = Js::JavascriptSIMDUint16x8::New(&simdValue, scriptContext);
    }
    else if (isSimd128U16)
    {LOGMEIN("BailOut.cpp] 958\n");
        Assert(!value);
        value = Js::JavascriptSIMDUint8x16::New(&simdValue, scriptContext);
    }
    else if (isSimd128B4)
    {LOGMEIN("BailOut.cpp] 963\n");
        Assert(!value);
        value = Js::JavascriptSIMDBool32x4::New(&simdValue, scriptContext);
    }
    else if (isSimd128B8)
    {LOGMEIN("BailOut.cpp] 968\n");
        Assert(!value);
        value = Js::JavascriptSIMDBool16x8::New(&simdValue, scriptContext);
    }
    else if (isSimd128B16)
    {LOGMEIN("BailOut.cpp] 973\n");
        Assert(!value);
        value = Js::JavascriptSIMDBool8x16::New(&simdValue, scriptContext);
    }
    else
    {
        BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u(", value: 0x%p"), value);
        if (boxStackInstance)
        {LOGMEIN("BailOut.cpp] 981\n");
            Js::Var oldValue = value;
            value = Js::JavascriptOperators::BoxStackInstance(oldValue, scriptContext, /* allowStackFunction */ true);

            if (oldValue != value)
            {LOGMEIN("BailOut.cpp] 986\n");
                BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u(" (Boxed: 0x%p)"), value);
            }
        }
    }

    values[regSlot] = value;

    BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("\n"));
}

void
BailOutRecord::RestoreValues(IR::BailOutKind bailOutKind, Js::JavascriptCallStackLayout * layout, uint count, __in_ecount_opt(count) int * offsets, int argOutSlotStart,
    __out_ecount(count) Js::Var * values, Js::ScriptContext * scriptContext,
    bool fromLoopBody, Js::Var * registerSaves, Js::InterpreterStackFrame * newInstance, Js::Var* pArgumentsObject, void * argoutRestoreAddress) const
{LOGMEIN("BailOut.cpp] 1001\n");
    bool isLocal = offsets == nullptr;
    if (isLocal == true)
    {LOGMEIN("BailOut.cpp] 1004\n");
        globalBailOutRecordTable->IterateGlobalBailOutRecordTableRows(m_bailOutRecordId, [=](GlobalBailOutRecordDataRow *row) {
            Assert(row->offset != 0);
            RestoreValue(bailOutKind, layout, values, scriptContext, fromLoopBody, registerSaves, newInstance, pArgumentsObject,
                argoutRestoreAddress, row->regSlot, row->offset, true, row->isFloat, row->isInt, row->isSimd128F4,
                row->isSimd128I4, row->isSimd128I8, row->isSimd128I16, row->isSimd128U4, row->isSimd128U8, row->isSimd128U16,
                row->isSimd128B4, row->isSimd128B8, row->isSimd128B16);
        });
    }
    else
    {
        for (uint i = 0; i < count; i++)
        {LOGMEIN("BailOut.cpp] 1016\n");
            int offset = 0;

            // The variables below determine whether we have a Var or native float/int.
            bool isFloat64;
            bool isInt32;
            bool isSimd128F4;
            bool isSimd128I4;
            bool isSimd128I8;
            bool isSimd128I16;
            bool isSimd128U4;
            bool isSimd128U8;
            bool isSimd128U16;
            bool isSimd128B4;
            bool isSimd128B8;
            bool isSimd128B16;

            offset = offsets[i];
            this->IsOffsetNativeIntOrFloat(i, argOutSlotStart, &isFloat64, &isInt32,
                                           &isSimd128F4, &isSimd128I4, &isSimd128I8, &isSimd128I16,
                                           &isSimd128U4, &isSimd128U8, &isSimd128U16, &isSimd128B4, &isSimd128B8, &isSimd128B16);

            RestoreValue(bailOutKind, layout, values, scriptContext, fromLoopBody, registerSaves, newInstance, pArgumentsObject,
                         argoutRestoreAddress, i, offset, false, isFloat64, isInt32, isSimd128F4, isSimd128I4, isSimd128I8,
                         isSimd128I16, isSimd128U4, isSimd128U8, isSimd128U16, isSimd128B4, isSimd128B8, isSimd128B16);
        }
    }
}

Js::Var BailOutRecord::BailOut(BailOutRecord const * bailOutRecord)
{LOGMEIN("BailOut.cpp] 1046\n");
    Assert(bailOutRecord);

    void * argoutRestoreAddr = nullptr;
#ifdef _M_IX86
    void * addressOfRetAddress = _AddressOfReturnAddress();
    if (bailOutRecord->ehBailoutData && (bailOutRecord->ehBailoutData->catchOffset != 0))
    {LOGMEIN("BailOut.cpp] 1053\n");
        // For a bailout in argument evaluation from an EH region, the esp is offset by the TryCatch helper's frame. So, the argouts are not at the offsets
        // stored in the bailout record, which are relative to ebp. Need to restore the argouts from the actual value of esp before calling the Bailout helper
        argoutRestoreAddr = (void *)((char*)addressOfRetAddress + ((1 + 1) * MachPtr)); // Account for the parameter and return address of this function
    }
#endif

    Js::JavascriptCallStackLayout *const layout = bailOutRecord->GetStackLayout();
    Js::ScriptFunction * function = (Js::ScriptFunction *)layout->functionObject;

    if (bailOutRecord->bailOutKind == IR::BailOutOnImplicitCalls)
    {LOGMEIN("BailOut.cpp] 1064\n");
        function->GetScriptContext()->GetThreadContext()->CheckAndResetImplicitCallAccessorFlag();
    }

    Js::ImplicitCallFlags savedImplicitCallFlags = function->GetScriptContext()->GetThreadContext()->GetImplicitCallFlags();

    if(bailOutRecord->globalBailOutRecordTable->isLoopBody)
    {LOGMEIN("BailOut.cpp] 1071\n");
        if (bailOutRecord->globalBailOutRecordTable->isInlinedFunction)
        {LOGMEIN("BailOut.cpp] 1073\n");
            return reinterpret_cast<Js::Var>(BailOutFromLoopBodyInlined(layout, bailOutRecord, _ReturnAddress()));
        }
        return reinterpret_cast<Js::Var>(BailOutFromLoopBody(layout, bailOutRecord));
    }
    if(bailOutRecord->globalBailOutRecordTable->isInlinedFunction)
    {LOGMEIN("BailOut.cpp] 1079\n");
        return BailOutInlined(layout, bailOutRecord, _ReturnAddress(), savedImplicitCallFlags);
    }
    return BailOutFromFunction(layout, bailOutRecord, _ReturnAddress(), argoutRestoreAddr, savedImplicitCallFlags);
}

uint32
BailOutRecord::BailOutFromLoopBody(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord)
{LOGMEIN("BailOut.cpp] 1087\n");
    Assert(bailOutRecord->parent == nullptr);
    return BailOutFromLoopBodyCommon(layout, bailOutRecord, bailOutRecord->bailOutOffset, bailOutRecord->bailOutKind);
}

Js::Var
BailOutRecord::BailOutFromFunction(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord, void * returnAddress, void * argoutRestoreAddress, Js::ImplicitCallFlags savedImplicitCallFlags)
{LOGMEIN("BailOut.cpp] 1094\n");
    Assert(bailOutRecord->parent == nullptr);

    return BailOutCommon(layout, bailOutRecord, bailOutRecord->bailOutOffset, returnAddress, bailOutRecord->bailOutKind, savedImplicitCallFlags, nullptr, nullptr, argoutRestoreAddress);
}

Js::Var
BailOutRecord::BailOutInlined(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord, void * returnAddress, Js::ImplicitCallFlags savedImplicitCallFlags)
{LOGMEIN("BailOut.cpp] 1102\n");
    Assert(bailOutRecord->parent != nullptr);
    return BailOutInlinedCommon(layout, bailOutRecord, bailOutRecord->bailOutOffset, returnAddress, bailOutRecord->bailOutKind, savedImplicitCallFlags);
}

uint32
BailOutRecord::BailOutFromLoopBodyInlined(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord, void * returnAddress)
{LOGMEIN("BailOut.cpp] 1109\n");
    Assert(bailOutRecord->parent != nullptr);
    return BailOutFromLoopBodyInlinedCommon(layout, bailOutRecord, bailOutRecord->bailOutOffset, returnAddress, bailOutRecord->bailOutKind);
}

Js::Var
BailOutRecord::BailOutCommonNoCodeGen(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
    uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var branchValue, Js::Var * registerSaves,
    BailOutReturnValue * bailOutReturnValue, void * argoutRestoreAddress)
{LOGMEIN("BailOut.cpp] 1118\n");
    Assert(bailOutRecord->parent == nullptr);
    Assert(Js::ScriptFunction::Is(layout->functionObject));
    Js::ScriptFunction ** functionRef = (Js::ScriptFunction **)&layout->functionObject;
    Js::ArgumentReader args(&layout->callInfo, layout->args);
    Js::Var result = BailOutHelper(layout, functionRef, args, false, bailOutRecord, bailOutOffset, returnAddress, bailOutKind, registerSaves, bailOutReturnValue, layout->GetArgumentsObjectLocation(), branchValue, argoutRestoreAddress);
    return result;
}

Js::Var
BailOutRecord::BailOutCommon(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::ImplicitCallFlags savedImplicitCallFlags, Js::Var branchValue, BailOutReturnValue * bailOutReturnValue, void * argoutRestoreAddress)
{LOGMEIN("BailOut.cpp] 1130\n");
    // Do not remove the following code.
    // Need to capture the int registers on stack as threadContext->bailOutRegisterSaveSpace is allocated from ThreadAlloc and is not scanned by recycler.
    // We don't want to save float (xmm) registers as they can be huge and they cannot contain a var.
    Js::Var registerSaves[INT_REG_COUNT];
    js_memcpy_s(registerSaves, sizeof(registerSaves), (Js::Var *)layout->functionObject->GetScriptContext()->GetThreadContext()->GetBailOutRegisterSaveSpace(),
        sizeof(registerSaves));

    Js::Var result = BailOutCommonNoCodeGen(layout, bailOutRecord, bailOutOffset, returnAddress, bailOutKind, branchValue, nullptr, bailOutReturnValue, argoutRestoreAddress);
    ScheduleFunctionCodeGen(Js::ScriptFunction::FromVar(layout->functionObject), nullptr, bailOutRecord, bailOutKind, bailOutOffset, savedImplicitCallFlags, returnAddress);
    return result;
}

Js::Var
BailOutRecord::BailOutInlinedCommon(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord, uint32 bailOutOffset,
    void * returnAddress, IR::BailOutKind bailOutKind, Js::ImplicitCallFlags savedImplicitCallFlags, Js::Var branchValue)
{LOGMEIN("BailOut.cpp] 1146\n");
    Assert(bailOutRecord->parent != nullptr);

    // Need to capture the register save, one of the bailout might get into jitted code again and bailout again
    // overwriting the current register saves
    Js::Var registerSaves[BailOutRegisterSaveSlotCount];
    js_memcpy_s(registerSaves, sizeof(registerSaves), (Js::Var *)layout->functionObject->GetScriptContext()->GetThreadContext()->GetBailOutRegisterSaveSpace(),
        sizeof(registerSaves));
    BailOutRecord const * currentBailOutRecord = bailOutRecord;
    BailOutReturnValue bailOutReturnValue;
    Js::ScriptFunction * innerMostInlinee;
    BailOutInlinedHelper(layout, currentBailOutRecord, bailOutOffset, returnAddress, bailOutKind, registerSaves, &bailOutReturnValue, &innerMostInlinee, false, branchValue);
    Js::Var result = BailOutCommonNoCodeGen(layout, currentBailOutRecord, currentBailOutRecord->bailOutOffset, returnAddress, bailOutKind, branchValue,
        registerSaves, &bailOutReturnValue);
    ScheduleFunctionCodeGen(Js::ScriptFunction::FromVar(layout->functionObject), innerMostInlinee, currentBailOutRecord, bailOutKind, bailOutOffset, savedImplicitCallFlags, returnAddress);
    return result;
}

uint32
BailOutRecord::BailOutFromLoopBodyCommon(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord, uint32 bailOutOffset,
    IR::BailOutKind bailOutKind, Js::Var branchValue)
{LOGMEIN("BailOut.cpp] 1167\n");
    uint32 result = BailOutFromLoopBodyHelper(layout, bailOutRecord, bailOutOffset, bailOutKind, branchValue);
    ScheduleLoopBodyCodeGen(Js::ScriptFunction::FromVar(layout->functionObject), nullptr, bailOutRecord, bailOutKind);
    return result;
}

uint32
BailOutRecord::BailOutFromLoopBodyInlinedCommon(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
    uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var branchValue)
{LOGMEIN("BailOut.cpp] 1176\n");
    Assert(bailOutRecord->parent != nullptr);
    Js::Var registerSaves[BailOutRegisterSaveSlotCount];
    js_memcpy_s(registerSaves, sizeof(registerSaves), (Js::Var *)layout->functionObject->GetScriptContext()->GetThreadContext()->GetBailOutRegisterSaveSpace(),
        sizeof(registerSaves));
    BailOutRecord const * currentBailOutRecord = bailOutRecord;
    BailOutReturnValue bailOutReturnValue;
    Js::ScriptFunction * innerMostInlinee;
    BailOutInlinedHelper(layout, currentBailOutRecord, bailOutOffset, returnAddress, bailOutKind, registerSaves, &bailOutReturnValue, &innerMostInlinee, true, branchValue);
    uint32 result = BailOutFromLoopBodyHelper(layout, currentBailOutRecord, currentBailOutRecord->bailOutOffset,
        bailOutKind, nullptr, registerSaves, &bailOutReturnValue);
    ScheduleLoopBodyCodeGen(Js::ScriptFunction::FromVar(layout->functionObject), innerMostInlinee, currentBailOutRecord, bailOutKind);
    return result;
}

void
BailOutRecord::BailOutInlinedHelper(Js::JavascriptCallStackLayout * layout, BailOutRecord const *& currentBailOutRecord,
    uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var * registerSaves, BailOutReturnValue * bailOutReturnValue, Js::ScriptFunction ** innerMostInlinee, bool isInLoopBody, Js::Var branchValue)
{LOGMEIN("BailOut.cpp] 1194\n");
    Assert(currentBailOutRecord->parent != nullptr);
    BailOutReturnValue * lastBailOutReturnValue = nullptr;
    *innerMostInlinee = nullptr;

    Js::FunctionBody* functionBody = Js::ScriptFunction::FromVar(layout->functionObject)->GetFunctionBody();

    Js::EntryPointInfo *entryPointInfo;
    if(isInLoopBody)
    {LOGMEIN("BailOut.cpp] 1203\n");
        Js::InterpreterStackFrame * interpreterFrame = functionBody->GetScriptContext()->GetThreadContext()->GetLeafInterpreterFrame();
        uint loopNum = interpreterFrame->GetCurrentLoopNum();

        entryPointInfo = (Js::EntryPointInfo*)functionBody->GetLoopEntryPointInfoFromNativeAddress((DWORD_PTR)returnAddress, loopNum);
    }
    else
    {
        entryPointInfo = (Js::EntryPointInfo*)functionBody->GetEntryPointFromNativeAddress((DWORD_PTR)returnAddress);
    }

    // Let's restore the inline stack - so that in case of a stack walk we have it available
    if (entryPointInfo->HasInlinees())
    {LOGMEIN("BailOut.cpp] 1216\n");
        InlineeFrameRecord* inlineeFrameRecord = entryPointInfo->FindInlineeFrame(returnAddress);
        if (inlineeFrameRecord)
        {LOGMEIN("BailOut.cpp] 1219\n");
            InlinedFrameLayout* outerMostFrame = (InlinedFrameLayout *)(((uint8 *)Js::JavascriptCallStackLayout::ToFramePointer(layout)) - entryPointInfo->frameHeight);
            inlineeFrameRecord->RestoreFrames(functionBody, outerMostFrame, layout);
        }
    }

    do
    {LOGMEIN("BailOut.cpp] 1226\n");
        InlinedFrameLayout *inlinedFrame = (InlinedFrameLayout *)(((char *)layout) + currentBailOutRecord->globalBailOutRecordTable->firstActualStackOffset);
        Js::InlineeCallInfo inlineeCallInfo = inlinedFrame->callInfo;
        Assert((Js::ArgSlot)inlineeCallInfo.Count == currentBailOutRecord->actualCount);
        Js::CallInfo callInfo(Js::CallFlags_Value, (Js::ArgSlot)inlineeCallInfo.Count);

        Js::ScriptFunction ** functionRef = (Js::ScriptFunction **)&(inlinedFrame->function);
        AnalysisAssert(*functionRef);
        Assert(Js::ScriptFunction::Is(inlinedFrame->function));

        if (*innerMostInlinee == nullptr)
        {LOGMEIN("BailOut.cpp] 1237\n");
            *innerMostInlinee = *functionRef;
        }
        Js::ArgumentReader args(&callInfo, inlinedFrame->GetArguments());
        Js::Var* pArgumentsObject = &inlinedFrame->arguments;

        (*functionRef)->GetFunctionBody()->EnsureDynamicProfileInfo();

        bailOutReturnValue->returnValue  = BailOutHelper(layout, functionRef, args, true, currentBailOutRecord, bailOutOffset,
                                                            returnAddress, bailOutKind, registerSaves, lastBailOutReturnValue, pArgumentsObject, branchValue);
        // Clear the inlinee frame CallInfo, just like we'd have done in JITted code.
        inlinedFrame->callInfo.Clear();

        bailOutReturnValue->returnValueRegSlot = currentBailOutRecord->globalBailOutRecordTable->returnValueRegSlot;

        lastBailOutReturnValue = bailOutReturnValue;

        currentBailOutRecord = currentBailOutRecord->parent;
        bailOutOffset = currentBailOutRecord->bailOutOffset;
    }
    while (currentBailOutRecord->parent != nullptr);
}

uint32
BailOutRecord::BailOutFromLoopBodyHelper(Js::JavascriptCallStackLayout * layout, BailOutRecord const * bailOutRecord,
    uint32 bailOutOffset, IR::BailOutKind bailOutKind, Js::Var branchValue, Js::Var *registerSaves, BailOutReturnValue * bailOutReturnValue)
{LOGMEIN("BailOut.cpp] 1263\n");
    Assert(bailOutRecord->parent == nullptr);

    Js::JavascriptFunction * function = layout->functionObject;

    Js::FunctionBody * executeFunction = function->GetFunctionBody();
    executeFunction->SetRecentlyBailedOutOfJittedLoopBody(true);

    Js::ScriptContext * functionScriptContext = executeFunction->GetScriptContext();

    // Clear the disable implicit call bit in case we bail from that region
    functionScriptContext->GetThreadContext()->ClearDisableImplicitFlags();

    // The current interpreter frame for the loop body
    Js::InterpreterStackFrame * interpreterFrame = functionScriptContext->GetThreadContext()->GetLeafInterpreterFrame();
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    BAILOUT_KIND_TRACE(executeFunction, bailOutKind, _u("BailOut: function: %s (%s) Loop: %d offset: #%04x Opcode: %s"),
        executeFunction->GetDisplayName(), executeFunction->GetDebugNumberSet(debugStringBuffer), interpreterFrame->GetCurrentLoopNum(),
        bailOutOffset, Js::OpCodeUtil::GetOpCodeName(bailOutRecord->bailOutOpcode));

    BAILOUT_TESTTRACE(executeFunction, bailOutKind, _u("BailOut: function: %s (%s) Loop: %d Opcode: %s\n"), executeFunction->GetDisplayName(),
        executeFunction->GetDebugNumberSet(debugStringBuffer), interpreterFrame->GetCurrentLoopNum(), Js::OpCodeUtil::GetOpCodeName(bailOutRecord->bailOutOpcode));

    // Restore bailout values
    bailOutRecord->RestoreValues(bailOutKind, layout, interpreterFrame, functionScriptContext, true, registerSaves, bailOutReturnValue, layout->GetArgumentsObjectLocation(), branchValue);

    BAILOUT_FLUSH(executeFunction);

    UpdatePolymorphicFieldAccess(function, bailOutRecord);

    // Return the resume byte code offset from the loop body to restart interpreter execution.
    return bailOutOffset;
}

void BailOutRecord::UpdatePolymorphicFieldAccess(Js::JavascriptFunction * function, BailOutRecord const * bailOutRecord)
{LOGMEIN("BailOut.cpp] 1300\n");
    Js::FunctionBody * executeFunction = bailOutRecord->type == Shared ? ((SharedBailOutRecord*)bailOutRecord)->functionBody : function->GetFunctionBody();
    Js::DynamicProfileInfo *dynamicProfileInfo = nullptr;
    if (executeFunction->HasDynamicProfileInfo())
    {LOGMEIN("BailOut.cpp] 1304\n");
        dynamicProfileInfo = executeFunction->GetAnyDynamicProfileInfo();
        Assert(dynamicProfileInfo);

        if (bailOutRecord->polymorphicCacheIndex != (uint)-1)
        {LOGMEIN("BailOut.cpp] 1309\n");
            dynamicProfileInfo->RecordPolymorphicFieldAccess(executeFunction, bailOutRecord->polymorphicCacheIndex);
            if (IR::IsEquivalentTypeCheckBailOutKind(bailOutRecord->bailOutKind))
            {LOGMEIN("BailOut.cpp] 1312\n");
                // If we've already got a polymorphic inline cache, and if we've got an equivalent type check
                // bailout here, make sure we don't try any more equivalent obj type spec using that cache.
                Js::PolymorphicInlineCache *polymorphicInlineCache = executeFunction->GetPolymorphicInlineCache(
                    bailOutRecord->polymorphicCacheIndex);
                if (polymorphicInlineCache)
                {LOGMEIN("BailOut.cpp] 1318\n");
                    polymorphicInlineCache->SetIgnoreForEquivalentObjTypeSpec(true);
                }
            }
        }
    }
}

Js::Var
BailOutRecord::BailOutHelper(Js::JavascriptCallStackLayout * layout, Js::ScriptFunction ** functionRef, Js::Arguments& args, const bool isInlinee,
    BailOutRecord const * bailOutRecord, uint32 bailOutOffset, void * returnAddress, IR::BailOutKind bailOutKind, Js::Var * registerSaves, BailOutReturnValue * bailOutReturnValue, Js::Var* pArgumentsObject,
    Js::Var branchValue, void * argoutRestoreAddress)
{LOGMEIN("BailOut.cpp] 1330\n");
    Js::ScriptFunction * function = *functionRef;
    Js::FunctionBody * executeFunction = function->GetFunctionBody();
    Js::ScriptContext * functionScriptContext = executeFunction->GetScriptContext();

    // Whether to enter StartCall while doing RestoreValues. We don't do that when bailout due to ignore exception under debugger.
    bool useStartCall = true;

    // Clear the disable implicit call bit in case we bail from that region
    functionScriptContext->GetThreadContext()->ClearDisableImplicitFlags();

    bool isInDebugMode = executeFunction->IsInDebugMode();
    AssertMsg(!isInDebugMode || Js::Configuration::Global.EnableJitInDebugMode(),
        "In diag mode we can get here (function has to be JIT'ed) only when EnableJitInDiagMode is true!");

    // Adjust bailout offset for debug mode (only scenario when we ignore exception).
    if (isInDebugMode)
    {LOGMEIN("BailOut.cpp] 1347\n");
        Js::DebugManager* debugManager = functionScriptContext->GetThreadContext()->GetDebugManager();
        DebuggingFlags* debuggingFlags = debugManager->GetDebuggingFlags();
        int byteCodeOffsetAfterEx = debuggingFlags->GetByteCodeOffsetAfterIgnoreException();

        // Note that in case where bailout for ignore exception immediately follows regular bailout after a helper,
        // and ignore exception happens, we would bail out with non-exception kind with exception data recorded.
        // In this case we need to treat the bailout as ignore exception one and continue to next/set stmt.
        // This is fine because we only set byteCodeOffsetAfterEx for helpers (HelperMethodWrapper, when enabled)
        // and ignore exception is needed for all helpers.
        if ((bailOutKind & IR::BailOutIgnoreException) || byteCodeOffsetAfterEx != DebuggingFlags::InvalidByteCodeOffset)
        {LOGMEIN("BailOut.cpp] 1358\n");
            bool needResetData = true;

            // Note: the func # in debuggingFlags still can be 0 in case actual b/o reason was not BailOutIgnoreException,
            //       but BailOutIgnoreException was on the OR'ed values for b/o check.
            bool isSameFunction = debuggingFlags->GetFuncNumberAfterIgnoreException() == DebuggingFlags::InvalidFuncNumber ||
                debuggingFlags->GetFuncNumberAfterIgnoreException() == function->GetFunctionBody()->GetFunctionNumber();
            AssertMsg(isSameFunction, "Bailout due to ignore exception in different function, can't bail out cross functions!");

            if (isSameFunction)
            {LOGMEIN("BailOut.cpp] 1368\n");
                Assert(!(byteCodeOffsetAfterEx == DebuggingFlags::InvalidByteCodeOffset && debuggingFlags->GetFuncNumberAfterIgnoreException() != DebuggingFlags::InvalidFuncNumber));

                if (byteCodeOffsetAfterEx != DebuggingFlags::InvalidByteCodeOffset)
                {LOGMEIN("BailOut.cpp] 1372\n");
                    // We got an exception in native frame, and need to bail out to interpreter
                    if (debugManager->stepController.IsActive())
                    {LOGMEIN("BailOut.cpp] 1375\n");
                        // Native frame went away, and there will be interpreter frame on its place.
                        // Make sure that frameAddrWhenSet it less than current interpreter frame -- we use it to detect stack depth.
                        debugManager->stepController.SetFrameAddr(0);
                    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
                    if (bailOutOffset != (uint)byteCodeOffsetAfterEx || !(bailOutKind & IR::BailOutIgnoreException))
                    {LOGMEIN("BailOut.cpp] 1383\n");
                        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                        BAILOUT_KIND_TRACE(executeFunction, bailOutKind, _u("BailOut: changing due to ignore exception: function: %s (%s) offset: #%04x -> #%04x Opcode: %s Treating as: %S"), executeFunction->GetDisplayName(),
                            executeFunction->GetDebugNumberSet(debugStringBuffer), bailOutOffset, byteCodeOffsetAfterEx, Js::OpCodeUtil::GetOpCodeName(bailOutRecord->bailOutOpcode), ::GetBailOutKindName(IR::BailOutIgnoreException));
                    }
#endif

                    // Set the byte code offset to continue from next user statement.
                    bailOutOffset = byteCodeOffsetAfterEx;

                    // Reset current call count so that we don't do StartCall for inner calls. See WinBlue 272569.
                    // The idea is that next statement can never be set to the inner StartCall (another call as part of an ArgOut),
                    // it will be next statement in the function.
                    useStartCall = false;
                }
                else
                {
                    needResetData = false;
                }
            }

            if (needResetData)
            {LOGMEIN("BailOut.cpp] 1405\n");
                // Reset/correct the flag as either we processed it or we need to correct wrong flag.
                debuggingFlags->ResetByteCodeOffsetAndFuncAfterIgnoreException();
            }
        }
    }
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    BAILOUT_KIND_TRACE(executeFunction, bailOutKind, _u("BailOut: function: %s (%s) offset: #%04x Opcode: %s"), executeFunction->GetDisplayName(),
        executeFunction->GetDebugNumberSet(debugStringBuffer), bailOutOffset, Js::OpCodeUtil::GetOpCodeName(bailOutRecord->bailOutOpcode));
    BAILOUT_TESTTRACE(executeFunction, bailOutKind, _u("BailOut: function: %s (%s) Opcode: %s\n"), executeFunction->GetDisplayName(),
        executeFunction->GetDebugNumberSet(debugStringBuffer), Js::OpCodeUtil::GetOpCodeName(bailOutRecord->bailOutOpcode));

    if (isInlinee && args.Info.Count != 0)
    {LOGMEIN("BailOut.cpp] 1420\n");
        // Box arguments. Inlinee arguments may be allocated on the stack.
        for(uint i = 0; i < args.Info.Count; ++i)
        {LOGMEIN("BailOut.cpp] 1423\n");
            const Js::Var arg = args.Values[i];
            BAILOUT_VERBOSE_TRACE(executeFunction, bailOutKind, _u("BailOut:   Argument #%3u: value: 0x%p"), i, arg);
            const Js::Var boxedArg = Js::JavascriptOperators::BoxStackInstance(arg, functionScriptContext, true);
            if(boxedArg != arg)
            {LOGMEIN("BailOut.cpp] 1428\n");
                args.Values[i] = boxedArg;
                BAILOUT_VERBOSE_TRACE(executeFunction, bailOutKind, _u(" (Boxed: 0x%p)"), boxedArg);
            }
            BAILOUT_VERBOSE_TRACE(executeFunction, bailOutKind, _u("\n"));
        }
    }

    bool fReleaseAlloc = false;
    Js::InterpreterStackFrame* newInstance = nullptr;
    Js::Var* allocation = nullptr;

    if (executeFunction->IsCoroutine())
    {LOGMEIN("BailOut.cpp] 1441\n");
        // If the FunctionBody is a generator then this call is being made by one of the three
        // generator resuming methods: next(), throw(), or return().  They all pass the generator
        // object as the first of two arguments.  The real user arguments are obtained from the
        // generator object.  The second argument is the ResumeYieldData which is only needed
        // when resuming a generator and not needed when yielding from a generator, as is occurring
        // here.
        AssertMsg(args.Info.Count == 2, "Generator ScriptFunctions should only be invoked by generator APIs with the pair of arguments they pass in -- the generator object and a ResumeYieldData pointer");
        Js::JavascriptGenerator* generator = Js::JavascriptGenerator::FromVar(args[0]);
        newInstance = generator->GetFrame();

        if (newInstance != nullptr)
        {LOGMEIN("BailOut.cpp] 1453\n");
            // BailOut will recompute OutArg pointers based on BailOutRecord.  Reset them back
            // to initial position before that happens so that OP_StartCall calls don't accumulate
            // incorrectly over multiple yield bailouts.
            newInstance->ResetOut();

            // The debugger relies on comparing stack addresses of frames to decide when a step_out is complete so
            // give the InterpreterStackFrame a legit enough stack address to make this comparison work.
            newInstance->m_stackAddress = reinterpret_cast<DWORD_PTR>(&generator);
        }
        else
        {
            //
            // Allocate a new InterpreterStackFrame instance on the recycler heap.
            // It will live with the JavascriptGenerator object.
            //
            Js::Arguments generatorArgs = generator->GetArguments();
            Js::InterpreterStackFrame::Setup setup(function, generatorArgs, true, isInlinee);
            size_t varAllocCount = setup.GetAllocationVarCount();
            size_t varSizeInBytes = varAllocCount * sizeof(Js::Var);
            DWORD_PTR stackAddr = reinterpret_cast<DWORD_PTR>(&generator); // as mentioned above, use any stack address from this frame to ensure correct debugging functionality
            Js::Var loopHeaderArray = executeFunction->GetHasAllocatedLoopHeaders() ? executeFunction->GetLoopHeaderArrayPtr() : nullptr;

            allocation = RecyclerNewPlus(functionScriptContext->GetRecycler(), varSizeInBytes, Js::Var);

            // Initialize the interpreter stack frame (constants) but not the param, the bailout record will restore the value
#if DBG
            // Allocate invalidVar on GC instead of stack since this InterpreterStackFrame will out live the current real frame
            Js::Var invalidVar = (Js::RecyclableObject*)RecyclerNewPlusLeaf(functionScriptContext->GetRecycler(), sizeof(Js::RecyclableObject), Js::Var);
            memset(invalidVar, 0xFE, sizeof(Js::RecyclableObject));
            newInstance = setup.InitializeAllocation(allocation, false, false, loopHeaderArray, stackAddr, invalidVar);
#else
            newInstance = setup.InitializeAllocation(allocation, false, false, loopHeaderArray, stackAddr);
#endif

            newInstance->m_reader.Create(executeFunction);

            generator->SetFrame(newInstance, varSizeInBytes);
        }
    }
    else
    {
        Js::InterpreterStackFrame::Setup setup(function, args, true, isInlinee);
        size_t varAllocCount = setup.GetAllocationVarCount();
        size_t varSizeInBytes = varAllocCount * sizeof(Js::Var);

        // If the locals area exceeds a certain limit, allocate it from a private arena rather than
        // this frame. The current limit is based on an old assert on the number of locals we would allow here.
        if (varAllocCount > Js::InterpreterStackFrame::LocalsThreshold)
        {LOGMEIN("BailOut.cpp] 1502\n");
            ArenaAllocator *tmpAlloc = nullptr;
            fReleaseAlloc = functionScriptContext->EnsureInterpreterArena(&tmpAlloc);
            allocation = (Js::Var*)tmpAlloc->Alloc(varSizeInBytes);
        }
        else
        {
            PROBE_STACK_PARTIAL_INITIALIZED_BAILOUT_FRAME(functionScriptContext, Js::Constants::MinStackInterpreter + varSizeInBytes, returnAddress);
            allocation = (Js::Var*)_alloca(varSizeInBytes);
        }

        Js::Var loopHeaderArray = nullptr;

        if (executeFunction->GetHasAllocatedLoopHeaders())
        {LOGMEIN("BailOut.cpp] 1516\n");
            // Loop header array is recycler allocated, so we push it on the stack
            // When we scan the stack, we'll recognize it as a recycler allocated
            // object, and mark it's contents and keep the individual loop header
            // wrappers alive
            loopHeaderArray = executeFunction->GetLoopHeaderArrayPtr();
        }

        // Set stack address for STEP_OUT/recursion detection for new frame.
        // This frame is originally jitted frame for which we create a new interpreter frame on top of it on stack,
        // set the stack address to some stack location that belong to the original jitted frame.
        DWORD_PTR frameStackAddr = reinterpret_cast<DWORD_PTR>(layout->GetArgv());

        // Initialize the interpreter stack frame (constants) but not the param, the bailout record will restore the value
#if DBG
        Js::Var invalidStackVar = (Js::RecyclableObject*)_alloca(sizeof(Js::RecyclableObject));
        memset(invalidStackVar, 0xFE, sizeof(Js::RecyclableObject));
        newInstance = setup.InitializeAllocation(allocation, false, false, loopHeaderArray, frameStackAddr, invalidStackVar);
#else
        newInstance = setup.InitializeAllocation(allocation, false, false, loopHeaderArray, frameStackAddr);
#endif

        newInstance->m_reader.Create(executeFunction);
    }

    int forInEnumeratorArrayRestoreOffset = bailOutRecord->globalBailOutRecordTable->forInEnumeratorArrayRestoreOffset;
    if (forInEnumeratorArrayRestoreOffset != -1)
    {LOGMEIN("BailOut.cpp] 1543\n");
        newInstance->forInObjectEnumerators = layout->GetForInObjectEnumeratorArrayAtOffset(forInEnumeratorArrayRestoreOffset);
    }

    newInstance->ehBailoutData = bailOutRecord->ehBailoutData;
    newInstance->OrFlags(Js::InterpreterStackFrameFlags_FromBailOut);

    ThreadContext *threadContext = newInstance->GetScriptContext()->GetThreadContext();

    // If this is a bailout on implicit calls, then it must have occurred at the current statement.
    // Otherwise, assume that the bits are stale, so clear them before entering the interpreter.
    if (!BailOutInfo::IsBailOutOnImplicitCalls(bailOutKind))
    {LOGMEIN("BailOut.cpp] 1555\n");
        threadContext->ClearImplicitCallFlags();
    }

    Js::RegSlot varCount = function->GetFunctionBody()->GetVarCount();
    if (varCount)
    {LOGMEIN("BailOut.cpp] 1561\n");
        Js::RegSlot constantCount = function->GetFunctionBody()->GetConstantCount();
        memset(newInstance->m_localSlots + constantCount, 0, varCount * sizeof(Js::Var));
    }

    Js::RegSlot localFrameDisplayReg = executeFunction->GetLocalFrameDisplayRegister();
    Js::RegSlot localClosureReg = executeFunction->GetLocalClosureRegister();
    Js::RegSlot paramClosureReg = executeFunction->GetParamClosureRegister();

    if (!isInlinee)
    {LOGMEIN("BailOut.cpp] 1571\n");
        // If byte code was generated to do stack closures, restore closure pointers before the normal RestoreValues.
        // If code was jitted for stack closures, we have to restore the pointers from known stack locations.
        // (RestoreValues won't do it.) If stack closures were disabled for this function before we jitted,
        // then the values on the stack are garbage, but if we need them then RestoreValues will overwrite with
        // the correct values.
        if (localFrameDisplayReg != Js::Constants::NoRegister)
        {LOGMEIN("BailOut.cpp] 1578\n");
            Js::FrameDisplay *localFrameDisplay;
            uintptr_t frameDisplayIndex = (uintptr_t)(
#if _M_IX86 || _M_AMD64
                executeFunction->GetInParamsCount() == 0 ?
                Js::JavascriptFunctionArgIndex_StackFrameDisplayNoArg :
#endif
                Js::JavascriptFunctionArgIndex_StackFrameDisplay) - 2;

            localFrameDisplay = (Js::FrameDisplay*)layout->GetArgv()[frameDisplayIndex];
            newInstance->SetLocalFrameDisplay(localFrameDisplay);
        }

        if (localClosureReg != Js::Constants::NoRegister)
        {LOGMEIN("BailOut.cpp] 1592\n");
            Js::Var localClosure;
            uintptr_t scopeSlotsIndex = (uintptr_t)(
#if _M_IX86 || _M_AMD64
                executeFunction->GetInParamsCount() == 0 ?
                Js::JavascriptFunctionArgIndex_StackScopeSlotsNoArg :
#endif
                Js::JavascriptFunctionArgIndex_StackScopeSlots) - 2;

            localClosure = layout->GetArgv()[scopeSlotsIndex];
            newInstance->SetLocalClosure(localClosure);
        }
    }

    // Restore bailout values
    bailOutRecord->RestoreValues(bailOutKind, layout, newInstance, functionScriptContext, false, registerSaves, bailOutReturnValue, pArgumentsObject, branchValue, returnAddress, useStartCall, argoutRestoreAddress);

    // For functions that don't get the scope slot and frame display pointers back from the known stack locations
    // (see above), get them back from the designated registers.
    // In either case, clear the values from those registers, because the interpreter should not be able to access
    // those values through the registers (only through its private fields).

    if (localFrameDisplayReg != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 1615\n");
        Js::FrameDisplay *frameDisplay = (Js::FrameDisplay*)newInstance->GetNonVarReg(localFrameDisplayReg);
        if (frameDisplay)
        {LOGMEIN("BailOut.cpp] 1618\n");
            newInstance->SetLocalFrameDisplay(frameDisplay);
            newInstance->SetNonVarReg(localFrameDisplayReg, nullptr);
        }
    }

    if (localClosureReg != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 1625\n");
        Js::Var closure = newInstance->GetNonVarReg(localClosureReg);
        if (closure)
        {LOGMEIN("BailOut.cpp] 1628\n");
            bailOutRecord->globalBailOutRecordTable->isScopeObjRestored = true;
            newInstance->SetLocalClosure(closure);
            newInstance->SetNonVarReg(localClosureReg, nullptr);
        }
    }

    if (paramClosureReg != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 1636\n");
        Js::Var closure = newInstance->GetNonVarReg(paramClosureReg);
        if (closure)
        {LOGMEIN("BailOut.cpp] 1639\n");
            newInstance->SetParamClosure(closure);
            newInstance->SetNonVarReg(paramClosureReg, nullptr);
        }
    }

    if (bailOutRecord->globalBailOutRecordTable->hasStackArgOpt)
    {LOGMEIN("BailOut.cpp] 1646\n");
        newInstance->TrySetFrameObjectInHeapArgObj(functionScriptContext, bailOutRecord->globalBailOutRecordTable->hasNonSimpleParams,
            bailOutRecord->globalBailOutRecordTable->isScopeObjRestored);
    }

    //Reset the value for tracking the restoration during next bail out.
    bailOutRecord->globalBailOutRecordTable->isScopeObjRestored = false;

    uint32 innerScopeCount = executeFunction->GetInnerScopeCount();
    for (uint32 i = 0; i < innerScopeCount; i++)
    {LOGMEIN("BailOut.cpp] 1656\n");
        Js::RegSlot reg = executeFunction->GetFirstInnerScopeRegister() + i;
        newInstance->SetInnerScopeFromIndex(i, newInstance->GetNonVarReg(reg));
        newInstance->SetNonVarReg(reg, nullptr);
    }

    newInstance->SetClosureInitDone(bailOutOffset != 0 || !(bailOutKind & IR::BailOutForDebuggerBits));

    // RestoreValues may call EnsureArguments and cause functions to be boxed.
    // Since the interpreter frame that hasn't started yet, StackScriptFunction::Box would not have replaced the function object
    // in the restoring interpreter frame. Let's make sure the current interpreter frame has the unboxed version.
    // Note: Only use the unboxed version if we have replaced the function argument on the stack via boxing
    // so that the interpreter frame we are bailing out to matches the one in the function argument list
    // (which is used by the stack walker to match up stack frame and the interpreter frame).
    // Some function are boxed but we continue to use the stack version to call the function - those that only live in register
    // and are not captured in frame displays.
    // Those uses are fine, but that means the function argument list will have the stack function object that is passed it and
    // not be replaced with a just boxed one.

    Js::ScriptFunction * currentFunctionObject = *functionRef;
    if (function != currentFunctionObject)
    {LOGMEIN("BailOut.cpp] 1677\n");
        Assert(currentFunctionObject == Js::StackScriptFunction::GetCurrentFunctionObject(function));
        newInstance->SetExecutingStackFunction(currentFunctionObject);
    }

    UpdatePolymorphicFieldAccess(function, bailOutRecord);

    BAILOUT_FLUSH(executeFunction);

    executeFunction->BeginExecution();

    // Restart at the bailout byte code offset.
    newInstance->m_reader.SetCurrentOffset(bailOutOffset);

    Js::Var aReturn = nullptr;

    {
        // Following _AddressOfReturnAddress <= real address of "returnAddress". Suffices for RemoteStackWalker to test partially initialized interpreter frame.
        Js::InterpreterStackFrame::PushPopFrameHelper pushPopFrameHelper(newInstance, returnAddress, _AddressOfReturnAddress());
        aReturn = isInDebugMode ? newInstance->DebugProcess() : newInstance->Process();
        // Note: in debug mode we always have to bailout to debug thunk,
        //       as normal interpreter thunk expects byte code compiled w/o debugging.
    }

    executeFunction->EndExecution();

    if (executeFunction->HasDynamicProfileInfo())
    {LOGMEIN("BailOut.cpp] 1704\n");
        Js::DynamicProfileInfo *dynamicProfileInfo = executeFunction->GetAnyDynamicProfileInfo();
        dynamicProfileInfo->RecordImplicitCallFlags(threadContext->GetImplicitCallFlags());
    }

    BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("BailOut:   Return Value: 0x%p"), aReturn);
    if (bailOutRecord->globalBailOutRecordTable->isInlinedConstructor)
    {LOGMEIN("BailOut.cpp] 1711\n");
        AssertMsg(!executeFunction->IsGenerator(), "Generator functions are not expected to be inlined. If this changes then need to use the real user args here from the generator object");
        Assert(args.Info.Count != 0);
        aReturn = Js::JavascriptFunction::FinishConstructor(aReturn, args.Values[0], function);

        Js::Var oldValue = aReturn;
        aReturn = Js::JavascriptOperators::BoxStackInstance(oldValue, functionScriptContext, /* allowStackFunction */ true);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (oldValue != aReturn)
        {LOGMEIN("BailOut.cpp] 1720\n");
            BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u(" (Boxed: 0x%p)"), aReturn);
        }
#endif
    }
    BAILOUT_VERBOSE_TRACE(newInstance->function->GetFunctionBody(), bailOutKind, _u("\n"));
    return aReturn;
}

// Note on rejit states
//
// To avoid always incurring the cost of collecting runtime stats (function calls count and valid bailOutKind),
// the initial codegen'd version of a function does not collect them. After a second bailout we rejit the function
// with runtime stats collection. On subsequent bailouts we can evaluate our heuristics and decide whether to rejit.
//
// Function bodies always use the least optimized version of the code as default. At the same time, there can be
// function objects with some older, more optimized, version of the code active. When a bailout occurs out of such
// code we avoid a rejit by checking if the offending optimization has been disabled in the default code and if so
// we "rethunk" the bailing out function rather that incurring a rejit.

// actualBailOutOffset - bail out offset in the function, inlinee or otherwise, that had the bailout.

void BailOutRecord::ScheduleFunctionCodeGen(Js::ScriptFunction * function, Js::ScriptFunction * innerMostInlinee,
    BailOutRecord const * bailOutRecord, IR::BailOutKind bailOutKind, uint32 actualBailOutOffset, Js::ImplicitCallFlags savedImplicitCallFlags, void * returnAddress)
{LOGMEIN("BailOut.cpp] 1744\n");
    if (bailOutKind == IR::BailOnSimpleJitToFullJitLoopBody ||
        bailOutKind == IR::BailOutForGeneratorYield ||
        bailOutKind == IR::LazyBailOut)
    {LOGMEIN("BailOut.cpp] 1748\n");
        return;
    }

    Js::FunctionBody * executeFunction = function->GetFunctionBody();

    if (PHASE_OFF(Js::ReJITPhase, executeFunction))
    {LOGMEIN("BailOut.cpp] 1755\n");
        return;
    }

    Js::AutoPushReturnAddressForStackWalker saveReturnAddress(executeFunction->GetScriptContext(), returnAddress);

    BailOutRecord * bailOutRecordNotConst = (BailOutRecord *)(void *)bailOutRecord;
    bailOutRecordNotConst->bailOutCount++;

    Js::FunctionEntryPointInfo *entryPointInfo = function->GetFunctionEntryPointInfo();
    uint8 callsCount = entryPointInfo->callsCount > 255 ? 255 : static_cast<uint8>(entryPointInfo->callsCount);
    RejitReason rejitReason = RejitReason::None;
    bool reThunk = false;

    callsCount = callsCount <= Js::FunctionEntryPointInfo::GetDecrCallCountPerBailout() ? 0 : callsCount - Js::FunctionEntryPointInfo::GetDecrCallCountPerBailout() ;

    CheckPreemptiveRejit(executeFunction, bailOutKind, bailOutRecordNotConst, callsCount, -1);

    entryPointInfo->callsCount = callsCount;

    Assert(bailOutKind != IR::BailOutInvalid);

    if ((executeFunction->HasDynamicProfileInfo() && callsCount == 0) ||
        PHASE_FORCE(Js::ReJITPhase, executeFunction))
    {LOGMEIN("BailOut.cpp] 1779\n");
        Js::DynamicProfileInfo * profileInfo = executeFunction->GetAnyDynamicProfileInfo();

        if ((bailOutKind & (IR::BailOutOnResultConditions | IR::BailOutOnDivSrcConditions)) || bailOutKind == IR::BailOutIntOnly || bailOutKind == IR::BailOnIntMin || bailOutKind == IR::BailOnDivResultNotInt)
        {LOGMEIN("BailOut.cpp] 1783\n");
            // Note WRT BailOnIntMin: it wouldn't make sense to re-jit without changing anything here, as interpreter will not change the (int) type,
            // so the options are: (1) rejit with disabling int type spec, (2) don't rejit, always bailout.
            // It seems to be better to rejit.
            if (bailOutKind & IR::BailOutOnMulOverflow)
            {LOGMEIN("BailOut.cpp] 1788\n");
                if (profileInfo->IsAggressiveMulIntTypeSpecDisabled(false))
                {LOGMEIN("BailOut.cpp] 1790\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableAggressiveMulIntTypeSpec(false);
                    rejitReason = RejitReason::AggressiveMulIntTypeSpecDisabled;
                }
            }
            else if ((bailOutKind & (IR::BailOutOnDivByZero | IR::BailOutOnDivOfMinInt)) || bailOutKind == IR::BailOnDivResultNotInt)
            {LOGMEIN("BailOut.cpp] 1800\n");
                if (profileInfo->IsDivIntTypeSpecDisabled(false))
                {LOGMEIN("BailOut.cpp] 1802\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableDivIntTypeSpec(false);
                    rejitReason = RejitReason::DivIntTypeSpecDisabled;
                }
            }
            else
            {
                if (profileInfo->IsAggressiveIntTypeSpecDisabled(false))
                {LOGMEIN("BailOut.cpp] 1814\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableAggressiveIntTypeSpec(false);
                    rejitReason = RejitReason::AggressiveIntTypeSpecDisabled;
                }
            }
        }
        else if (bailOutKind & IR::BailOutForDebuggerBits)
        {LOGMEIN("BailOut.cpp] 1825\n");
            // Do not rejit, do not rethunk, just ignore the bailout.
        }
        else switch(bailOutKind)
        {LOGMEIN("BailOut.cpp] 1829\n");
            case IR::BailOutOnNotPrimitive:
                if (profileInfo->IsLossyIntTypeSpecDisabled())
                {LOGMEIN("BailOut.cpp] 1832\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableLossyIntTypeSpec();
                    rejitReason = RejitReason::LossyIntTypeSpecDisabled;
                }
                break;
            case IR::BailOutOnMemOpError:
                if (profileInfo->IsMemOpDisabled())
                {LOGMEIN("BailOut.cpp] 1843\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableMemOp();
                    rejitReason = RejitReason::MemOpDisabled;
                }
                break;

            case IR::BailOutPrimitiveButString:
            case IR::BailOutNumberOnly:
                if (profileInfo->IsFloatTypeSpecDisabled())
                {LOGMEIN("BailOut.cpp] 1856\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableFloatTypeSpec();
                    rejitReason = RejitReason::FloatTypeSpecDisabled;
                }
                break;

            case IR::BailOutOnImplicitCalls:
            case IR::BailOutOnImplicitCallsPreOp:
                // Check if the implicit call flags in the profile have changed since we last JITed this
                // function body. If so, and they indicate an implicit call of some sort occurred
                // then we need to reJIT.
                if ((executeFunction->GetSavedImplicitCallsFlags() & savedImplicitCallFlags) == Js::ImplicitCall_None)
                {LOGMEIN("BailOut.cpp] 1872\n");
                    profileInfo->RecordImplicitCallFlags(savedImplicitCallFlags);
                    profileInfo->DisableLoopImplicitCallInfo();
                    rejitReason = RejitReason::ImplicitCallFlagsChanged;
                }
                else
                {
                    reThunk = true;
                }
                break;

            case IR::BailOnModByPowerOf2:
                rejitReason = RejitReason::ModByPowerOf2;
                break;

            case IR::BailOutOnNotArray:
                if(profileInfo->IsArrayCheckHoistDisabled(false))
                {LOGMEIN("BailOut.cpp] 1889\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableArrayCheckHoist(false);
                    rejitReason = RejitReason::ArrayCheckHoistDisabled;
                }
                break;

            case IR::BailOutOnNotNativeArray:

                // REVIEW: We have an issue with array profile info.  The info on the type of array we have won't
                //         get fixed by rejitting.  For now, just give up after 50 rejits.
                if (profileInfo->GetRejitCount() >= 50)
                {LOGMEIN("BailOut.cpp] 1904\n");
                    reThunk = true;
                }
                else
                {
                    rejitReason = RejitReason::ExpectingNativeArray;
                }
                break;

            case IR::BailOutConvertedNativeArray:
                rejitReason = RejitReason::ConvertedNativeArray;
                break;

            case IR::BailOutConventionalTypedArrayAccessOnly:
                if(profileInfo->IsTypedArrayTypeSpecDisabled(false))
                {LOGMEIN("BailOut.cpp] 1919\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableTypedArrayTypeSpec(false);
                    rejitReason = RejitReason::TypedArrayTypeSpecDisabled;
                }
                break;

            case IR::BailOutConventionalNativeArrayAccessOnly:
                rejitReason = RejitReason::ExpectingConventionalNativeArrayAccess;
                break;

            case IR::BailOutOnMissingValue:
                if(profileInfo->IsArrayMissingValueCheckHoistDisabled(false))
                {LOGMEIN("BailOut.cpp] 1935\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableArrayMissingValueCheckHoist(false);
                    rejitReason = RejitReason::ArrayMissingValueCheckHoistDisabled;
                }
                break;

            case IR::BailOutOnArrayAccessHelperCall:
                // This is a pre-op bailout, so the interpreter will update the profile data for this byte-code instruction to
                // prevent excessive bailouts here in the future
                rejitReason = RejitReason::ArrayAccessNeededHelperCall;
                break;

            case IR::BailOutOnInvalidatedArrayHeadSegment:
                if(profileInfo->IsJsArraySegmentHoistDisabled(false))
                {LOGMEIN("BailOut.cpp] 1953\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableJsArraySegmentHoist(false);
                    rejitReason = RejitReason::JsArraySegmentHoistDisabled;
                }
                break;

            case IR::BailOutOnIrregularLength:
                if(profileInfo->IsLdLenIntSpecDisabled())
                {LOGMEIN("BailOut.cpp] 1965\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableLdLenIntSpec();
                    rejitReason = RejitReason::LdLenIntSpecDisabled;
                }
                break;

            case IR::BailOutOnFailedHoistedBoundCheck:
                if(profileInfo->IsBoundCheckHoistDisabled(false))
                {LOGMEIN("BailOut.cpp] 1977\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableBoundCheckHoist(false);
                    rejitReason = RejitReason::BoundCheckHoistDisabled;
                }
                break;

            case IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck:
                if(profileInfo->IsLoopCountBasedBoundCheckHoistDisabled(false))
                {LOGMEIN("BailOut.cpp] 1989\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableLoopCountBasedBoundCheckHoist(false);
                    rejitReason = RejitReason::LoopCountBasedBoundCheckHoistDisabled;
                }
                break;

            case IR::BailOutExpectingInteger:
                if (profileInfo->IsSwitchOptDisabled())
                {LOGMEIN("BailOut.cpp] 2001\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableSwitchOpt();
                    rejitReason = RejitReason::DisableSwitchOptExpectingInteger;
                }
                break;

            case IR::BailOutExpectingString:
                if (profileInfo->IsSwitchOptDisabled())
                {LOGMEIN("BailOut.cpp] 2013\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableSwitchOpt();
                    rejitReason = RejitReason::DisableSwitchOptExpectingString;
                }
                break;

            case IR::BailOutOnFailedPolymorphicInlineTypeCheck:
                rejitReason = RejitReason::FailedPolymorphicInlineeTypeCheck;
                break;

            case IR::BailOnStackArgsOutOfActualsRange:
                if (profileInfo->IsStackArgOptDisabled())
                {LOGMEIN("BailOut.cpp] 2029\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableStackArgOpt();
                    rejitReason = RejitReason::DisableStackArgOpt;
                }
                break;
            case IR::BailOutOnPolymorphicInlineFunction:
            case IR::BailOutFailedInlineTypeCheck:
            case IR::BailOutOnInlineFunction:
                // Check if the inliner state has changed since we last JITed this function body. If so
                // then we need to reJIT.
                if (innerMostInlinee)
                {LOGMEIN("BailOut.cpp] 2044\n");
                    // There is no way now to check if the inlinee version has changed. Just rejit.
                    // This should be changed to getting the inliner version corresponding to inlinee.
                    rejitReason = RejitReason::InlineeChanged;
                }
                else
                {
                    if (executeFunction->GetSavedInlinerVersion() == profileInfo->GetInlinerVersion())
                    {LOGMEIN("BailOut.cpp] 2052\n");
                        reThunk = true;
                    }
                    else
                    {
                        rejitReason = RejitReason::InlineeChanged;
                    }
                }
                break;

            case IR::BailOutOnNoProfile:
                if (profileInfo->IsNoProfileBailoutsDisabled())
                {LOGMEIN("BailOut.cpp] 2064\n");
                    reThunk = true;
                }
                else if (executeFunction->IncrementBailOnMisingProfileRejitCount() >  (uint)CONFIG_FLAG(BailOnNoProfileRejitLimit))
                {LOGMEIN("BailOut.cpp] 2068\n");
                    profileInfo->DisableNoProfileBailouts();
                    rejitReason = RejitReason::NoProfile;
                }
                else
                {
                    executeFunction->ResetBailOnMisingProfileCount();
                    rejitReason = RejitReason::NoProfile;
                }
                break;

            case IR::BailOutCheckThis:
                // Presumably we've started passing a different "this" pointer to callees.
                if (profileInfo->IsCheckThisDisabled())
                {LOGMEIN("BailOut.cpp] 2082\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableCheckThis();
                    rejitReason = RejitReason::CheckThisDisabled;
                }
                break;

            case IR::BailOutOnTaggedValue:
                if (profileInfo->IsTagCheckDisabled())
                {LOGMEIN("BailOut.cpp] 2094\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableTagCheck();
                    rejitReason = RejitReason::FailedTagCheck;
                }
                break;

            case IR::BailOutFailedTypeCheck:
            case IR::BailOutFailedFixedFieldTypeCheck:
            {LOGMEIN("BailOut.cpp] 2106\n");
                // An inline cache must have gone from monomorphic to polymorphic.
                // This is already noted in the profile data, so optimization of the given ld/st will
                // be inhibited on re-jit.
                // Consider disabling the optimization across the function after n failed type checks.
                if (innerMostInlinee)
                {LOGMEIN("BailOut.cpp] 2112\n");
                    rejitReason = bailOutKind == IR::BailOutFailedTypeCheck ? RejitReason::FailedTypeCheck : RejitReason::FailedFixedFieldTypeCheck;
                }
                else
                {
                    uint32 state;
                    state = profileInfo->GetPolymorphicCacheState();

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
                    if (PHASE_TRACE(Js::ObjTypeSpecPhase, executeFunction))
                    {LOGMEIN("BailOut.cpp] 2122\n");
                        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                        Output::Print(
                            _u("Objtypespec (%s): States on bailout: Saved cache: %d, Live cache: %d\n"),
                            executeFunction->GetDebugNumberSet(debugStringBuffer), executeFunction->GetSavedPolymorphicCacheState(), state);
                        Output::Flush();
                    }
#endif
                    if (state <= executeFunction->GetSavedPolymorphicCacheState())
                    {LOGMEIN("BailOut.cpp] 2131\n");
                        reThunk = true;
                    }
                    else
                    {
                        rejitReason = bailOutKind == IR::BailOutFailedTypeCheck ?
                            RejitReason::FailedTypeCheck : RejitReason::FailedFixedFieldTypeCheck;
                    }
                }
                break;
            }

            case IR::BailOutFailedEquivalentTypeCheck:
            case IR::BailOutFailedEquivalentFixedFieldTypeCheck:
                if (profileInfo->IsEquivalentObjTypeSpecDisabled())
                {LOGMEIN("BailOut.cpp] 2146\n");
                    reThunk = true;
                }
                else
                {
                    rejitReason = bailOutKind == IR::BailOutFailedEquivalentTypeCheck ?
                        RejitReason::FailedEquivalentTypeCheck : RejitReason::FailedEquivalentFixedFieldTypeCheck;
                }
                break;

            case IR::BailOutFailedFixedFieldCheck:
                rejitReason = RejitReason::FailedFixedFieldCheck;
                break;

            case IR::BailOutFailedCtorGuardCheck:
                // (ObjTypeSpec): Consider scheduling re-JIT right after the first bailout.  We will never successfully execute the
                // function from which we just bailed out, unless we take a different code path through it.

                // A constructor cache guard may be invalidated for one of two reasons:
                // a) the constructor's prototype property has changed, or
                // b) one of the properties protected by the guard (this constructor cache served as) has changed in some way (e.g. became read-only).
                // In the former case, the cache itself will be marked as polymorphic and on re-JIT we won't do the optimization.
                // In the latter case, the inline cache for the offending property will be cleared and on re-JIT the guard will not be enlisted
                // to protect that property operation.
                rejitReason = RejitReason::CtorGuardInvalidated;
                break;

            case IR::BailOutOnFloor:
            {LOGMEIN("BailOut.cpp] 2174\n");
                if (profileInfo->IsFloorInliningDisabled())
                {LOGMEIN("BailOut.cpp] 2176\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisableFloorInlining();
                    rejitReason = RejitReason::FloorInliningDisabled;
                }
                break;
            }
            case IR::BailOutOnPowIntIntOverflow:
            {LOGMEIN("BailOut.cpp] 2187\n");
                if (profileInfo->IsPowIntIntTypeSpecDisabled())
                {LOGMEIN("BailOut.cpp] 2189\n");
                    reThunk = true;
                }
                else
                {
                    profileInfo->DisablePowIntIntTypeSpec();
                    rejitReason = RejitReason::PowIntIntTypeSpecDisabled;
                }
            }
        }

        Assert(!(rejitReason != RejitReason::None && reThunk));
    }

    if(PHASE_FORCE(Js::ReJITPhase, executeFunction) && rejitReason == RejitReason::None)
    {LOGMEIN("BailOut.cpp] 2204\n");
        rejitReason = RejitReason::Forced;
    }

    if (!reThunk && rejitReason != RejitReason::None)
    {LOGMEIN("BailOut.cpp] 2209\n");
        Js::DynamicProfileInfo * profileInfo = executeFunction->GetAnyDynamicProfileInfo();
        // REVIEW: Temporary fix for RS1.  Disable Rejiting if it looks like it is not fixing the problem.
        //         For RS2, turn the rejitCount check into an assert and let's fix all these issues.
        if (profileInfo->GetRejitCount() >= 100 ||
            (profileInfo->GetBailOutOffsetForLastRejit() == actualBailOutOffset && function->IsNewEntryPointAvailable()))
        {LOGMEIN("BailOut.cpp] 2215\n");
            reThunk = true;
            rejitReason = RejitReason::None;
        }
        else
        {
            profileInfo->IncRejitCount();
            profileInfo->SetBailOutOffsetForLastRejit(actualBailOutOffset);
        }
    }

    REJIT_KIND_TESTTRACE(bailOutKind, _u("Bailout from function: function: %s, bailOutKindName: (%S), bailOutCount: %d, callCount: %d, reJitReason: %S, reThunk: %s\r\n"),
        function->GetFunctionBody()->GetDisplayName(), ::GetBailOutKindName(bailOutKind), bailOutRecord->bailOutCount, callsCount,
        RejitReasonNames[rejitReason], reThunk ? trueString : falseString);

#ifdef REJIT_STATS
    if(PHASE_STATS(Js::ReJITPhase, executeFunction))
    {LOGMEIN("BailOut.cpp] 2232\n");
        executeFunction->GetScriptContext()->LogBailout(executeFunction, bailOutKind);
    }
#endif

    if (reThunk && executeFunction->DontRethunkAfterBailout())
    {LOGMEIN("BailOut.cpp] 2238\n");
        // This function is marked for rethunking, but the last ReJIT we've done was for a JIT loop body
        // So the latest rejitted version of this function may not have the right optimization disabled.
        // Rejit just to be safe.
        reThunk = false;
        rejitReason = RejitReason::AfterLoopBodyRejit;
    }
    if (reThunk)
    {LOGMEIN("BailOut.cpp] 2246\n");
        Js::FunctionEntryPointInfo *const defaultEntryPointInfo = executeFunction->GetDefaultFunctionEntryPointInfo();
        function->UpdateThunkEntryPoint(defaultEntryPointInfo, executeFunction->GetDirectEntryPoint(defaultEntryPointInfo));
    }
    else if (rejitReason != RejitReason::None)
    {LOGMEIN("BailOut.cpp] 2251\n");
#ifdef REJIT_STATS
        if(PHASE_STATS(Js::ReJITPhase, executeFunction))
        {LOGMEIN("BailOut.cpp] 2254\n");
            executeFunction->GetScriptContext()->LogRejit(executeFunction, rejitReason);
        }
#endif
        executeFunction->ClearDontRethunkAfterBailout();

        GenerateFunction(executeFunction->GetScriptContext()->GetNativeCodeGenerator(), executeFunction, function);

        if(executeFunction->GetExecutionMode() != ExecutionMode::FullJit)
        {LOGMEIN("BailOut.cpp] 2263\n");
            // With expiry, it's possible that the execution mode is currently interpreter or simple JIT. Transition to full JIT
            // after successfully scheduling the rejit work item (in case of OOM).
            executeFunction->TraceExecutionMode("Rejit (before)");
            executeFunction->TransitionToFullJitExecutionMode();
            executeFunction->TraceExecutionMode("Rejit");
        }

#if ENABLE_DEBUG_CONFIG_OPTIONS
        if(PHASE_TRACE(Js::ReJITPhase, executeFunction))
        {LOGMEIN("BailOut.cpp] 2273\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(
                _u("Rejit: function: %s (%s), bailOutCount: %hu"),
                executeFunction->GetDisplayName(),
                executeFunction->GetDebugNumberSet(debugStringBuffer),
                bailOutRecord->bailOutCount);

            Output::Print(_u(" callCount: %u"), callsCount);
            Output::Print(_u(" reason: %S"), RejitReasonNames[rejitReason]);
            if(bailOutKind != IR::BailOutInvalid)
            {LOGMEIN("BailOut.cpp] 2284\n");
                Output::Print(_u(" (%S)"), ::GetBailOutKindName(bailOutKind));
            }
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
    }
}

// To avoid always incurring the cost of collecting runtime stats (valid bailOutKind),
// the initial codegen'd version of a loop body does not collect them. After a second bailout we rejit the body
// with runtime stats collection. On subsequent bailouts we can evaluate our heuristics.
void BailOutRecord::ScheduleLoopBodyCodeGen(Js::ScriptFunction * function, Js::ScriptFunction * innerMostInlinee, BailOutRecord const * bailOutRecord, IR::BailOutKind bailOutKind)
{LOGMEIN("BailOut.cpp] 2298\n");
    Assert(bailOutKind != IR::LazyBailOut);
    Js::FunctionBody * executeFunction = function->GetFunctionBody();

    if (PHASE_OFF(Js::ReJITPhase, executeFunction))
    {LOGMEIN("BailOut.cpp] 2303\n");
        return;
    }

    Js::LoopHeader * loopHeader = nullptr;

    Js::InterpreterStackFrame * interpreterFrame = executeFunction->GetScriptContext()->GetThreadContext()->GetLeafInterpreterFrame();

    loopHeader = executeFunction->GetLoopHeader(interpreterFrame->GetCurrentLoopNum());

    Assert(loopHeader != nullptr);

    BailOutRecord * bailOutRecordNotConst = (BailOutRecord *)(void *)bailOutRecord;
    bailOutRecordNotConst->bailOutCount++;

    RejitReason rejitReason = RejitReason::None;
    Assert(bailOutKind != IR::BailOutInvalid);

    Js::LoopEntryPointInfo* entryPointInfo = loopHeader->GetCurrentEntryPointInfo();

    entryPointInfo->totalJittedLoopIterations += entryPointInfo->jittedLoopIterationsSinceLastBailout;
    entryPointInfo->jittedLoopIterationsSinceLastBailout = 0;
    uint8 totalJittedLoopIterations = entryPointInfo->totalJittedLoopIterations > 255 ? 255 : static_cast<uint8>(entryPointInfo->totalJittedLoopIterations);
    totalJittedLoopIterations = totalJittedLoopIterations <= Js::LoopEntryPointInfo::GetDecrLoopCountPerBailout() ? 0 : totalJittedLoopIterations - Js::LoopEntryPointInfo::GetDecrLoopCountPerBailout();

    CheckPreemptiveRejit(executeFunction, bailOutKind, bailOutRecordNotConst, totalJittedLoopIterations, interpreterFrame->GetCurrentLoopNum());

    entryPointInfo->totalJittedLoopIterations = totalJittedLoopIterations;

    if ((executeFunction->HasDynamicProfileInfo() && totalJittedLoopIterations == 0) ||
        PHASE_FORCE(Js::ReJITPhase, executeFunction))
    {LOGMEIN("BailOut.cpp] 2334\n");
        Js::DynamicProfileInfo * profileInfo = executeFunction->GetAnyDynamicProfileInfo();

        if ((bailOutKind & (IR::BailOutOnResultConditions | IR::BailOutOnDivSrcConditions)) || bailOutKind == IR::BailOutIntOnly || bailOutKind == IR::BailOnIntMin)
        {LOGMEIN("BailOut.cpp] 2338\n");
            if (bailOutKind & IR::BailOutOnMulOverflow)
            {LOGMEIN("BailOut.cpp] 2340\n");
                profileInfo->DisableAggressiveMulIntTypeSpec(true);
                rejitReason = RejitReason::AggressiveMulIntTypeSpecDisabled;
            }
            else if ((bailOutKind & (IR::BailOutOnDivByZero | IR::BailOutOnDivOfMinInt)) || bailOutKind == IR::BailOnDivResultNotInt)
            {LOGMEIN("BailOut.cpp] 2345\n");
                profileInfo->DisableDivIntTypeSpec(true);
                rejitReason = RejitReason::DivIntTypeSpecDisabled;
            }
            else
            {
                profileInfo->DisableAggressiveIntTypeSpec(true);
                rejitReason = RejitReason::AggressiveIntTypeSpecDisabled;
            }
            executeFunction->SetDontRethunkAfterBailout();
        }
        else switch(bailOutKind)
        {LOGMEIN("BailOut.cpp] 2357\n");
            case IR::BailOutOnNotPrimitive:
                profileInfo->DisableLossyIntTypeSpec();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::LossyIntTypeSpecDisabled;
                break;

            case IR::BailOutOnMemOpError:
                profileInfo->DisableMemOp();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::MemOpDisabled;
                break;

            case IR::BailOutPrimitiveButString:
            case IR::BailOutNumberOnly:
                profileInfo->DisableFloatTypeSpec();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::FloatTypeSpecDisabled;
                break;

            case IR::BailOutOnImplicitCalls:
            case IR::BailOutOnImplicitCallsPreOp:
                rejitReason = RejitReason::ImplicitCallFlagsChanged;
                break;

            case IR::BailOutExpectingInteger:
                profileInfo->DisableSwitchOpt();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::DisableSwitchOptExpectingInteger;
                break;

            case IR::BailOutExpectingString:
                profileInfo->DisableSwitchOpt();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::DisableSwitchOptExpectingString;
                break;

            case IR::BailOnStackArgsOutOfActualsRange:
                AssertMsg(false, "How did we reach here ? Stack args opt is currently disabled in loop body gen.");
                break;

            case IR::BailOnModByPowerOf2:
                rejitReason = RejitReason::ModByPowerOf2;
                break;

            case IR::BailOutOnNotArray:
                profileInfo->DisableArrayCheckHoist(true);
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::ArrayCheckHoistDisabled;
                break;

            case IR::BailOutOnNotNativeArray:
                rejitReason = RejitReason::ExpectingNativeArray;
                break;

            case IR::BailOutConvertedNativeArray:
                rejitReason = RejitReason::ConvertedNativeArray;
                break;

            case IR::BailOutConventionalTypedArrayAccessOnly:
                profileInfo->DisableTypedArrayTypeSpec(true);
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::TypedArrayTypeSpecDisabled;
                break;

            case IR::BailOutConventionalNativeArrayAccessOnly:
                rejitReason = RejitReason::ExpectingConventionalNativeArrayAccess;
                break;

            case IR::BailOutOnMissingValue:
                profileInfo->DisableArrayMissingValueCheckHoist(true);
                rejitReason = RejitReason::ArrayMissingValueCheckHoistDisabled;
                break;

            case IR::BailOutOnArrayAccessHelperCall:
                // This is a pre-op bailout, so the interpreter will update the profile data for this byte-code instruction to
                // prevent excessive bailouts here in the future
                rejitReason = RejitReason::ArrayAccessNeededHelperCall;
                break;

            case IR::BailOutOnInvalidatedArrayHeadSegment:
                profileInfo->DisableJsArraySegmentHoist(true);
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::JsArraySegmentHoistDisabled;
                break;

            case IR::BailOutOnIrregularLength:
                profileInfo->DisableLdLenIntSpec();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::LdLenIntSpecDisabled;
                break;

            case IR::BailOutOnFailedHoistedBoundCheck:
                profileInfo->DisableBoundCheckHoist(true);
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::BoundCheckHoistDisabled;
                break;

            case IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck:
                profileInfo->DisableLoopCountBasedBoundCheckHoist(true);
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::LoopCountBasedBoundCheckHoistDisabled;
                break;

            case IR::BailOutOnInlineFunction:
            case IR::BailOutOnPolymorphicInlineFunction:
            case IR::BailOutOnFailedPolymorphicInlineTypeCheck:
                rejitReason = RejitReason::InlineeChanged;
                break;

            case IR::BailOutOnNoProfile:
                rejitReason = RejitReason::NoProfile;
                executeFunction->ResetBailOnMisingProfileCount();
                break;

            case IR::BailOutCheckThis:
                profileInfo->DisableCheckThis();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::CheckThisDisabled;
                break;

            case IR::BailOutFailedTypeCheck:
                // An inline cache must have gone from monomorphic to polymorphic.
                // This is already noted in the profile data, so optimization of the given ld/st will
                // be inhibited on re-jit.
                // Consider disabling the optimization across the function after n failed type checks.

                // Disable ObjTypeSpec in a large loop body after the first rejit itself.
                // Rejitting a large loop body takes more time and the fact that loop bodies are prioritized ahead of functions to be jitted only augments the problem.
                if(executeFunction->GetByteCodeInLoopCount() > (uint)CONFIG_FLAG(LoopBodySizeThresholdToDisableOpts))
                {LOGMEIN("BailOut.cpp] 2487\n");
                    profileInfo->DisableObjTypeSpecInJitLoopBody();
                    if(PHASE_TRACE1(Js::DisabledObjTypeSpecPhase))
                    {LOGMEIN("BailOut.cpp] 2490\n");
                        Output::Print(_u("Disabled obj type spec in jit loop body for loop %d in %s (%d)\n"),
                            executeFunction->GetLoopNumber(loopHeader), executeFunction->GetDisplayName(), executeFunction->GetFunctionNumber());
                        Output::Flush();
                    }
                }

                rejitReason = RejitReason::FailedTypeCheck;
                break;

            case IR::BailOutFailedFixedFieldTypeCheck:
                // An inline cache must have gone from monomorphic to polymorphic or some fixed field
                // became non-fixed.  Either one is already noted in the profile data and type system,
                // so optimization of the given instruction will be inhibited on re-jit.
                // Consider disabling the optimization across the function after n failed type checks.
                rejitReason = RejitReason::FailedFixedFieldTypeCheck;
                break;

            case IR::BailOutFailedEquivalentTypeCheck:
            case IR::BailOutFailedEquivalentFixedFieldTypeCheck:
                rejitReason = bailOutKind == IR::BailOutFailedEquivalentTypeCheck ?
                    RejitReason::FailedEquivalentTypeCheck : RejitReason::FailedEquivalentFixedFieldTypeCheck;
                break;

            case IR::BailOutFailedCtorGuardCheck:
                // (ObjTypeSpec): Consider scheduling re-JIT right after the first bailout.  We will never successfully execute the
                // function from which we just bailed out, unless we take a different code path through it.

                // A constructor cache guard may be invalidated for one of two reasons:
                // a) the constructor's prototype property has changed, or
                // b) one of the properties protected by the guard (this constructor cache served as) has changed in some way (e.g. became
                // read-only).
                // In the former case, the cache itself will be marked as polymorphic and on re-JIT we won't do the optimization.
                // In the latter case, the inline cache for the offending property will be cleared and on re-JIT the guard will not be enlisted
                // to protect that property operation.
                rejitReason = RejitReason::CtorGuardInvalidated;
                break;

            case IR::BailOutOnFloor:
            {LOGMEIN("BailOut.cpp] 2529\n");
                profileInfo->DisableFloorInlining();
                rejitReason = RejitReason::FloorInliningDisabled;
                break;
            }

            case IR::BailOutFailedFixedFieldCheck:
                rejitReason = RejitReason::FailedFixedFieldCheck;
                break;

            case IR::BailOutOnTaggedValue:
                profileInfo->DisableTagCheck();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::FailedTagCheck;
                break;

            case IR::BailOutOnPowIntIntOverflow:
                profileInfo->DisablePowIntIntTypeSpec();
                executeFunction->SetDontRethunkAfterBailout();
                rejitReason = RejitReason::PowIntIntTypeSpecDisabled;
                break;
        }

        if(PHASE_FORCE(Js::ReJITPhase, executeFunction) && rejitReason == RejitReason::None)
        {LOGMEIN("BailOut.cpp] 2553\n");
            rejitReason = RejitReason::Forced;
        }
    }

    if (PHASE_FORCE(Js::ReJITPhase, executeFunction) && rejitReason == RejitReason::None)
    {LOGMEIN("BailOut.cpp] 2559\n");
        rejitReason = RejitReason::Forced;
    }

    REJIT_KIND_TESTTRACE(bailOutKind, _u("Bailout from loop: function: %s, loopNumber: %d, bailOutKindName: (%S), reJitReason: %S\r\n"),
        function->GetFunctionBody()->GetDisplayName(), executeFunction->GetLoopNumber(loopHeader),
        ::GetBailOutKindName(bailOutKind), RejitReasonNames[rejitReason]);

#ifdef REJIT_STATS
    if(PHASE_STATS(Js::ReJITPhase, executeFunction))
    {LOGMEIN("BailOut.cpp] 2569\n");
        executeFunction->GetScriptContext()->LogBailout(executeFunction, bailOutKind);
    }
#endif

    if (rejitReason != RejitReason::None)
    {LOGMEIN("BailOut.cpp] 2575\n");
#ifdef REJIT_STATS
        if(PHASE_STATS(Js::ReJITPhase, executeFunction))
        {LOGMEIN("BailOut.cpp] 2578\n");
            executeFunction->GetScriptContext()->LogRejit(executeFunction, rejitReason);
        }
#endif
        // Single bailout triggers re-JIT of loop body. the actual codegen scheduling of the new
        // loop body happens in the interpreter
        loopHeader->interpretCount = executeFunction->GetLoopInterpretCount(loopHeader) - 2;
        loopHeader->CreateEntryPoint();

#if ENABLE_DEBUG_CONFIG_OPTIONS
        if(PHASE_TRACE(Js::ReJITPhase, executeFunction))
        {LOGMEIN("BailOut.cpp] 2589\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            Output::Print(
                _u("Rejit(loop): function: %s (%s) loop: %u bailOutCount: %hu reason: %S"),
                executeFunction->GetDisplayName(),
                executeFunction->GetDebugNumberSet(debugStringBuffer),
                executeFunction->GetLoopNumber(loopHeader),
                bailOutRecord->bailOutCount,
                RejitReasonNames[rejitReason]);
            if(bailOutKind != IR::BailOutInvalid)
            {LOGMEIN("BailOut.cpp] 2599\n");
                Output::Print(_u(" (%S)"), ::GetBailOutKindName(bailOutKind));
            }
            Output::Print(_u("\n"));
            Output::Flush();
        }
#endif
    }
}

void BailOutRecord::CheckPreemptiveRejit(Js::FunctionBody* executeFunction, IR::BailOutKind bailOutKind, BailOutRecord* bailoutRecord, uint8& callsOrIterationsCount, int loopNumber)
{LOGMEIN("BailOut.cpp] 2610\n");
    if (bailOutKind == IR::BailOutOnNoProfile && executeFunction->IncrementBailOnMisingProfileCount() > CONFIG_FLAG(BailOnNoProfileLimit))
    {LOGMEIN("BailOut.cpp] 2612\n");
        // A rejit here should improve code quality, so lets avoid too many unnecessary bailouts.
        executeFunction->ResetBailOnMisingProfileCount();
        bailoutRecord->bailOutCount = 0;
        callsOrIterationsCount = 0;
    }
    else if (bailoutRecord->bailOutCount > CONFIG_FLAG(RejitMaxBailOutCount))
    {LOGMEIN("BailOut.cpp] 2619\n");
        switch (bailOutKind)
        {LOGMEIN("BailOut.cpp] 2621\n");
        case IR::BailOutOnPolymorphicInlineFunction:
        case IR::BailOutOnFailedPolymorphicInlineTypeCheck:
        case IR::BailOutFailedInlineTypeCheck:
        case IR::BailOutOnInlineFunction:
        case IR::BailOutFailedTypeCheck:
        case IR::BailOutFailedFixedFieldTypeCheck:
        case IR::BailOutFailedCtorGuardCheck:
        case IR::BailOutFailedFixedFieldCheck:
        case IR::BailOutFailedEquivalentTypeCheck:
        case IR::BailOutFailedEquivalentFixedFieldTypeCheck:
        {LOGMEIN("BailOut.cpp] 2632\n");
            // If we consistently see RejitMaxBailOutCount bailouts for these kinds, then likely we have stale profile data and it is beneficial to rejit.
            // Note you need to include only bailout kinds which don't disable the entire optimizations.
            if (loopNumber == -1)
            {
                REJIT_KIND_TESTTRACE(bailOutKind, _u("Force rejit as RejitMaxBailoOutCount reached for a bailout record: function: %s, bailOutKindName: (%S), bailOutCount: %d, callCount: %d RejitMaxBailoutCount: %d\r\n"),
                    executeFunction->GetDisplayName(), ::GetBailOutKindName(bailOutKind), bailoutRecord->bailOutCount, callsOrIterationsCount, CONFIG_FLAG(RejitMaxBailOutCount));
            }
            else
            {
                REJIT_KIND_TESTTRACE(bailOutKind, _u("Force rejit as RejitMaxBailoOutCount reached for a bailout record: function: %s, loopNumber: %d, bailOutKindName: (%S), bailOutCount: %d, callCount: %d RejitMaxBailoutCount: %d\r\n"),
                    executeFunction->GetDisplayName(), loopNumber, ::GetBailOutKindName(bailOutKind), bailoutRecord->bailOutCount, callsOrIterationsCount, CONFIG_FLAG(RejitMaxBailOutCount));
            }
            bailoutRecord->bailOutCount = 0;
            callsOrIterationsCount = 0;
            break;
        }
        default: break;
        }
    }
}

Js::Var BailOutRecord::BailOutForElidedYield(void * framePointer)
{LOGMEIN("BailOut.cpp] 2655\n");
    Js::JavascriptCallStackLayout * const layout = Js::JavascriptCallStackLayout::FromFramePointer(framePointer);
    Js::ScriptFunction ** functionRef = (Js::ScriptFunction **)&layout->functionObject;
    Js::ScriptFunction * function = *functionRef;
    Js::FunctionBody * executeFunction = function->GetFunctionBody();
    bool isInDebugMode = executeFunction->IsInDebugMode();

    Js::JavascriptGenerator* generator = static_cast<Js::JavascriptGenerator*>(layout->args[0]);
    Js::InterpreterStackFrame* frame = generator->GetFrame();
    ThreadContext *threadContext = frame->GetScriptContext()->GetThreadContext();

    Js::ResumeYieldData* resumeYieldData = static_cast<Js::ResumeYieldData*>(layout->args[1]);
    frame->SetNonVarReg(executeFunction->GetYieldRegister(), resumeYieldData);

    // The debugger relies on comparing stack addresses of frames to decide when a step_out is complete so
    // give the InterpreterStackFrame a legit enough stack address to make this comparison work.
    frame->m_stackAddress = reinterpret_cast<DWORD_PTR>(&generator);

    executeFunction->BeginExecution();

    Js::Var aReturn = nullptr;

    {
        // Following _AddressOfReturnAddress <= real address of "returnAddress". Suffices for RemoteStackWalker to test partially initialized interpreter frame.
        Js::InterpreterStackFrame::PushPopFrameHelper pushPopFrameHelper(frame, _ReturnAddress(), _AddressOfReturnAddress());
        aReturn = isInDebugMode ? frame->DebugProcess() : frame->Process();
        // Note: in debug mode we always have to bailout to debug thunk,
        //       as normal interpreter thunk expects byte code compiled w/o debugging.
    }

    executeFunction->EndExecution();

    if (executeFunction->HasDynamicProfileInfo())
    {LOGMEIN("BailOut.cpp] 2688\n");
        Js::DynamicProfileInfo *dynamicProfileInfo = executeFunction->GetAnyDynamicProfileInfo();
        dynamicProfileInfo->RecordImplicitCallFlags(threadContext->GetImplicitCallFlags());
    }

    return aReturn;
}

BranchBailOutRecord::BranchBailOutRecord(uint32 trueBailOutOffset, uint32 falseBailOutOffset, Js::RegSlot resultByteCodeReg, IR::BailOutKind kind, Func * bailOutFunc)
    : BailOutRecord(trueBailOutOffset, (uint)-1, kind, bailOutFunc), falseBailOutOffset(falseBailOutOffset)
{LOGMEIN("BailOut.cpp] 2698\n");
    branchValueRegSlot = resultByteCodeReg;
    type = BailoutRecordType::Branch;
};

Js::Var BranchBailOutRecord::BailOut(BranchBailOutRecord const * bailOutRecord, BOOL cond)
{LOGMEIN("BailOut.cpp] 2704\n");
    Assert(bailOutRecord);

    void * argoutRestoreAddr = nullptr;
#ifdef _M_IX86
    void * addressOfRetAddress = _AddressOfReturnAddress();
    if (bailOutRecord->ehBailoutData && (bailOutRecord->ehBailoutData->catchOffset != 0))
    {LOGMEIN("BailOut.cpp] 2711\n");
        argoutRestoreAddr = (void *)((char*)addressOfRetAddress + ((2 + 1) * MachPtr)); // Account for the parameters and return address of this function
    }
#endif

    Js::JavascriptCallStackLayout *const layout = bailOutRecord->GetStackLayout();
    Js::ScriptFunction * function = (Js::ScriptFunction *)layout->functionObject;

    if (bailOutRecord->bailOutKind == IR::BailOutOnImplicitCalls)
    {LOGMEIN("BailOut.cpp] 2720\n");
        function->GetScriptContext()->GetThreadContext()->CheckAndResetImplicitCallAccessorFlag();
    }

    Js::ImplicitCallFlags savedImplicitCallFlags = function->GetScriptContext()->GetThreadContext()->GetImplicitCallFlags();

    if(bailOutRecord->globalBailOutRecordTable->isLoopBody)
    {LOGMEIN("BailOut.cpp] 2727\n");
        if (bailOutRecord->globalBailOutRecordTable->isInlinedFunction)
        {LOGMEIN("BailOut.cpp] 2729\n");
            return reinterpret_cast<Js::Var>(BailOutFromLoopBodyInlined(layout, bailOutRecord, cond, _ReturnAddress()));
        }
        return reinterpret_cast<Js::Var>(BailOutFromLoopBody(layout, bailOutRecord, cond));
    }
    if(bailOutRecord->globalBailOutRecordTable->isInlinedFunction)
    {LOGMEIN("BailOut.cpp] 2735\n");
        return BailOutInlined(layout, bailOutRecord, cond, _ReturnAddress(), savedImplicitCallFlags);
    }
    return BailOutFromFunction(layout, bailOutRecord, cond, _ReturnAddress(), argoutRestoreAddr, savedImplicitCallFlags);
}

Js::Var
BranchBailOutRecord::BailOutFromFunction(Js::JavascriptCallStackLayout * layout, BranchBailOutRecord const * bailOutRecord, BOOL cond, void * returnAddress, void * argoutRestoreAddress, Js::ImplicitCallFlags savedImplicitCallFlags)
{LOGMEIN("BailOut.cpp] 2743\n");
    Assert(bailOutRecord->parent == nullptr);
    uint32 bailOutOffset = cond? bailOutRecord->bailOutOffset : bailOutRecord->falseBailOutOffset;
    Js::Var branchValue = nullptr;
    if (bailOutRecord->branchValueRegSlot != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 2748\n");
        Js::ScriptContext *scriptContext = layout->functionObject->GetScriptContext();
        branchValue = (cond ? scriptContext->GetLibrary()->GetTrue() : scriptContext->GetLibrary()->GetFalse());
    }
    return __super::BailOutCommon(layout, bailOutRecord, bailOutOffset, returnAddress, bailOutRecord->bailOutKind, savedImplicitCallFlags, branchValue, nullptr, argoutRestoreAddress);
}

uint32
BranchBailOutRecord::BailOutFromLoopBody(Js::JavascriptCallStackLayout * layout, BranchBailOutRecord const * bailOutRecord, BOOL cond)
{LOGMEIN("BailOut.cpp] 2757\n");
    Assert(bailOutRecord->parent == nullptr);
    uint32 bailOutOffset = cond? bailOutRecord->bailOutOffset : bailOutRecord->falseBailOutOffset;
    Js::Var branchValue = nullptr;
    if (bailOutRecord->branchValueRegSlot != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 2762\n");
        Js::ScriptContext *scriptContext = layout->functionObject->GetScriptContext();
        branchValue = (cond ? scriptContext->GetLibrary()->GetTrue() : scriptContext->GetLibrary()->GetFalse());
    }
    return __super::BailOutFromLoopBodyCommon(layout, bailOutRecord, bailOutOffset, bailOutRecord->bailOutKind, branchValue);
}

Js::Var
BranchBailOutRecord::BailOutInlined(Js::JavascriptCallStackLayout * layout, BranchBailOutRecord const * bailOutRecord, BOOL cond, void * returnAddress, Js::ImplicitCallFlags savedImplicitCallFlags)
{LOGMEIN("BailOut.cpp] 2771\n");
    Assert(bailOutRecord->parent != nullptr);
    uint32 bailOutOffset = cond? bailOutRecord->bailOutOffset : bailOutRecord->falseBailOutOffset;
    Js::Var branchValue = nullptr;
    if (bailOutRecord->branchValueRegSlot != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 2776\n");
        Js::ScriptContext *scriptContext = layout->functionObject->GetScriptContext();
        branchValue = (cond ? scriptContext->GetLibrary()->GetTrue() : scriptContext->GetLibrary()->GetFalse());
    }
    return __super::BailOutInlinedCommon(layout, bailOutRecord, bailOutOffset, returnAddress, bailOutRecord->bailOutKind, savedImplicitCallFlags, branchValue);
}

uint32
BranchBailOutRecord::BailOutFromLoopBodyInlined(Js::JavascriptCallStackLayout * layout, BranchBailOutRecord const * bailOutRecord, BOOL cond, void * returnAddress)
{LOGMEIN("BailOut.cpp] 2785\n");
    Assert(bailOutRecord->parent != nullptr);
    uint32 bailOutOffset = cond? bailOutRecord->bailOutOffset : bailOutRecord->falseBailOutOffset;
    Js::Var branchValue = nullptr;
    if (bailOutRecord->branchValueRegSlot != Js::Constants::NoRegister)
    {LOGMEIN("BailOut.cpp] 2790\n");
        Js::ScriptContext *scriptContext = layout->functionObject->GetScriptContext();
        branchValue = (cond ? scriptContext->GetLibrary()->GetTrue() : scriptContext->GetLibrary()->GetFalse());
    }
    return __super::BailOutFromLoopBodyInlinedCommon(layout, bailOutRecord, bailOutOffset, returnAddress, bailOutRecord->bailOutKind, branchValue);
}

SharedBailOutRecord::SharedBailOutRecord(uint32 bailOutOffset, uint bailOutCacheIndex, IR::BailOutKind kind, Func *bailOutFunc)
    : BailOutRecord(bailOutOffset, bailOutCacheIndex, kind, bailOutFunc)
{LOGMEIN("BailOut.cpp] 2799\n");
    this->functionBody = nullptr;
    this->type = BailoutRecordType::Shared;
}

void LazyBailOutRecord::SetBailOutKind()
{LOGMEIN("BailOut.cpp] 2805\n");
    this->bailoutRecord->SetBailOutKind(IR::BailOutKind::LazyBailOut);
}

#if DBG
void LazyBailOutRecord::Dump(Js::FunctionBody* functionBody)
{LOGMEIN("BailOut.cpp] 2811\n");
    OUTPUT_PRINT(functionBody);
    Output::Print(_u("Bytecode Offset: #%04x opcode: %s"), this->bailoutRecord->GetBailOutOffset(), Js::OpCodeUtil::GetOpCodeName(this->bailoutRecord->GetBailOutOpCode()));
}
#endif

void GlobalBailOutRecordDataTable::Finalize(NativeCodeData::Allocator *allocator, JitArenaAllocator *tempAlloc)
{LOGMEIN("BailOut.cpp] 2818\n");
    GlobalBailOutRecordDataRow *newRows = NativeCodeDataNewArrayZNoFixup(allocator, GlobalBailOutRecordDataRow, length);
    memcpy(newRows, globalBailOutRecordDataRows, sizeof(GlobalBailOutRecordDataRow) * length);
    JitAdeleteArray(tempAlloc, length, globalBailOutRecordDataRows);
    globalBailOutRecordDataRows = newRows;
    size = length;

#if DBG
    if (length > 0)
    {LOGMEIN("BailOut.cpp] 2827\n");
        uint32 currStart = globalBailOutRecordDataRows[0].start;
        for (uint32 i = 1; i < length; i++)
        {LOGMEIN("BailOut.cpp] 2830\n");
            AssertMsg(currStart <= globalBailOutRecordDataRows[i].start,
                      "Rows in the table must be in order by start ID");
            currStart = globalBailOutRecordDataRows[i].start;
        }
    }
#endif
}

void  GlobalBailOutRecordDataTable::AddOrUpdateRow(JitArenaAllocator *allocator, uint32 bailOutRecordId, uint32 regSlot, bool isFloat, bool isInt,
                                                   bool isSimd128F4, bool isSimd128I4, bool isSimd128I8, bool isSimd128I16, bool isSimd128U4, bool isSimd128U8, bool isSimd128U16,
                                                   bool isSimd128B4, bool isSimd128B8, bool isSimd128B16, int32 offset, uint *lastUpdatedRowIndex)
{LOGMEIN("BailOut.cpp] 2842\n");
    Assert(offset != 0);
    const int INITIAL_TABLE_SIZE = 64;
    if (size == 0)
    {LOGMEIN("BailOut.cpp] 2846\n");
        Assert(length == 0);
        size = INITIAL_TABLE_SIZE;
        globalBailOutRecordDataRows = JitAnewArrayZ(allocator, GlobalBailOutRecordDataRow, size);
    }

    Assert(lastUpdatedRowIndex != nullptr);

    if ((*lastUpdatedRowIndex) != -1)
    {LOGMEIN("BailOut.cpp] 2855\n");
        GlobalBailOutRecordDataRow *rowToUpdate = &globalBailOutRecordDataRows[(*lastUpdatedRowIndex)];
        if(rowToUpdate->offset == offset &&
            rowToUpdate->isInt == (unsigned)isInt &&
            rowToUpdate->isFloat == (unsigned)isFloat &&
            // SIMD_JS
            rowToUpdate->isSimd128F4    == (unsigned) isSimd128F4 &&
            rowToUpdate->isSimd128I4    == (unsigned) isSimd128I4  &&
            rowToUpdate->isSimd128I8    == (unsigned) isSimd128I8  &&
            rowToUpdate->isSimd128I16   == (unsigned) isSimd128I16 &&
            rowToUpdate->isSimd128U4    == (unsigned) isSimd128U4  &&
            rowToUpdate->isSimd128U8    == (unsigned) isSimd128U8  &&
            rowToUpdate->isSimd128U16   == (unsigned) isSimd128U16 &&
            rowToUpdate->isSimd128B4    == (unsigned) isSimd128B4  &&
            rowToUpdate->isSimd128B8    == (unsigned) isSimd128B8  &&
            rowToUpdate->isSimd128B16   == (unsigned) isSimd128B16 &&

            rowToUpdate->end + 1 == bailOutRecordId)
        {LOGMEIN("BailOut.cpp] 2873\n");
            Assert(rowToUpdate->regSlot == regSlot);
            rowToUpdate->end = bailOutRecordId;
            return;
        }
    }

    if (length == size)
    {LOGMEIN("BailOut.cpp] 2881\n");
        size = length << 1;
        globalBailOutRecordDataRows = (GlobalBailOutRecordDataRow *)allocator->Realloc(globalBailOutRecordDataRows, length * sizeof(GlobalBailOutRecordDataRow), size * sizeof(GlobalBailOutRecordDataRow));
    }
    GlobalBailOutRecordDataRow *rowToInsert = &globalBailOutRecordDataRows[length];
    rowToInsert->start = bailOutRecordId;
    rowToInsert->end = bailOutRecordId;
    rowToInsert->offset = offset;
    rowToInsert->isFloat = isFloat;
    rowToInsert->isInt = isInt;
    // SIMD_JS
    rowToInsert->isSimd128F4    = isSimd128F4;
    rowToInsert->isSimd128I4    = isSimd128I4;
    rowToInsert->isSimd128I8    = isSimd128I8 ;
    rowToInsert->isSimd128I16   = isSimd128I16;
    rowToInsert->isSimd128U4    = isSimd128U4 ;
    rowToInsert->isSimd128U8    = isSimd128U8 ;
    rowToInsert->isSimd128U16   = isSimd128U16;
    rowToInsert->isSimd128B4    = isSimd128B4 ;
    rowToInsert->isSimd128B8    = isSimd128B8 ;
    rowToInsert->isSimd128B16   = isSimd128B16;
    rowToInsert->regSlot = regSlot;
    *lastUpdatedRowIndex = length++;
}
