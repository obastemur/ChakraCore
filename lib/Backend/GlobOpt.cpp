//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

#if ENABLE_DEBUG_CONFIG_OPTIONS

#define TESTTRACE_PHASE_INSTR(phase, instr, ...) \
    if(PHASE_TESTTRACE(phase, this->func)) \
    {LOGMEIN("GlobOpt.cpp] 10\n"); \
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE]; \
        Output::Print( \
            _u("Testtrace: %s function %s (%s): "), \
            Js::PhaseNames[phase], \
            instr->m_func->GetJITFunctionBody()->GetDisplayName(), \
            instr->m_func->GetDebugNumberSet(debugStringBuffer)); \
        Output::Print(__VA_ARGS__); \
        Output::Flush(); \
    }

#else // ENABLE_DEBUG_CONFIG_OPTIONS

#define TESTTRACE_PHASE_INSTR(phase, instr, ...)

#endif // ENABLE_DEBUG_CONFIG_OPTIONS

#if ENABLE_DEBUG_CONFIG_OPTIONS && DBG_DUMP

#define GOPT_TRACE_OPND(opnd, ...) \
    if (PHASE_TRACE(Js::GlobOptPhase, this->func) && !this->IsLoopPrePass()) \
    {LOGMEIN("GlobOpt.cpp] 31\n"); \
        Output::Print(_u("TRACE: ")); \
        opnd->Dump(); \
        Output::Print(_u(" : ")); \
        Output::Print(__VA_ARGS__); \
        Output::Flush(); \
    }
#define GOPT_TRACE(...) \
    if (PHASE_TRACE(Js::GlobOptPhase, this->func) && !this->IsLoopPrePass()) \
    {LOGMEIN("GlobOpt.cpp] 40\n"); \
        Output::Print(_u("TRACE: ")); \
        Output::Print(__VA_ARGS__); \
        Output::Flush(); \
    }

#define GOPT_TRACE_INSTRTRACE(instr) \
    if (PHASE_TRACE(Js::GlobOptPhase, this->func) && !this->IsLoopPrePass()) \
    {LOGMEIN("GlobOpt.cpp] 48\n"); \
        instr->Dump(); \
        Output::Flush(); \
    }

#define GOPT_TRACE_INSTR(instr, ...) \
    if (PHASE_TRACE(Js::GlobOptPhase, this->func) && !this->IsLoopPrePass()) \
    {LOGMEIN("GlobOpt.cpp] 55\n"); \
        Output::Print(_u("TRACE: ")); \
        Output::Print(__VA_ARGS__); \
        instr->Dump(); \
        Output::Flush(); \
    }

#define GOPT_TRACE_BLOCK(block, before) \
    this->Trace(block, before); \
    Output::Flush();

// TODO: OOP JIT, add back line number
#define TRACE_PHASE_INSTR(phase, instr, ...) \
    if(PHASE_TRACE(phase, this->func)) \
    {LOGMEIN("GlobOpt.cpp] 69\n"); \
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE]; \
        Output::Print( \
            _u("Function %s (%s)"), \
            this->func->GetJITFunctionBody()->GetDisplayName(), \
            this->func->GetDebugNumberSet(debugStringBuffer)); \
        if(this->func->IsLoopBody()) \
        {LOGMEIN("GlobOpt.cpp] 76\n"); \
            Output::Print(_u(", loop %u"), this->func->GetWorkItem()->GetLoopNumber()); \
        } \
        if(instr->m_func != this->func) \
        {LOGMEIN("GlobOpt.cpp] 80\n"); \
            Output::Print( \
                _u(", Inlinee %s (%s)"), \
                instr->m_func->GetJITFunctionBody()->GetDisplayName(), \
                instr->m_func->GetDebugNumberSet(debugStringBuffer)); \
        } \
        Output::Print(_u(" - %s\n    "), Js::PhaseNames[phase]); \
        instr->Dump(); \
        Output::Print(_u("    ")); \
        Output::Print(__VA_ARGS__); \
        Output::Flush(); \
    }

#define TRACE_PHASE_INSTR_VERBOSE(phase, instr, ...) \
    if(CONFIG_FLAG(Verbose)) \
    { \
        TRACE_PHASE_INSTR(phase, instr, __VA_ARGS__); \
    }

#define TRACE_TESTTRACE_PHASE_INSTR(phase, instr, ...) \
    TRACE_PHASE_INSTR(phase, instr, __VA_ARGS__); \
    TESTTRACE_PHASE_INSTR(phase, instr, __VA_ARGS__);

#else   // ENABLE_DEBUG_CONFIG_OPTIONS && DBG_DUMP

#define GOPT_TRACE(...)
#define GOPT_TRACE_OPND(opnd, ...)
#define GOPT_TRACE_INSTRTRACE(instr)
#define GOPT_TRACE_INSTR(instr, ...)
#define GOPT_TRACE_BLOCK(block, before)
#define TRACE_PHASE_INSTR(phase, instr, ...)
#define TRACE_PHASE_INSTR_VERBOSE(phase, instr, ...)
#define TRACE_TESTTRACE_PHASE_INSTR(phase, instr, ...) TESTTRACE_PHASE_INSTR(phase, instr, __VA_ARGS__);

#endif  // ENABLE_DEBUG_CONFIG_OPTIONS && DBG_DUMP

#if DBG_DUMP
#define DO_MEMOP_TRACE() (PHASE_TRACE(Js::MemOpPhase, this->func) ||\
        PHASE_TRACE(Js::MemSetPhase, this->func) ||\
        PHASE_TRACE(Js::MemCopyPhase, this->func))
#define DO_MEMOP_TRACE_PHASE(phase) (PHASE_TRACE(Js::MemOpPhase, this->func) || PHASE_TRACE(Js::phase ## Phase, this->func))

#define OUTPUT_MEMOP_TRACE(loop, instr, ...) {\
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];\
    Output::Print(15, _u("Function: %s%s, Loop: %u: "), this->func->GetJITFunctionBody()->GetDisplayName(), this->func->GetDebugNumberSet(debugStringBuffer), loop->GetLoopNumber());\
    Output::Print(__VA_ARGS__);\
    IR::Instr* __instr__ = instr;\
    if(__instr__) __instr__->DumpByteCodeOffset();\
    if(__instr__) Output::Print(_u(" (%s)"), Js::OpCodeUtil::GetOpCodeName(__instr__->m_opcode));\
    Output::Print(_u("\n"));\
    Output::Flush(); \
}
#define TRACE_MEMOP(loop, instr, ...) \
    if (DO_MEMOP_TRACE()) {LOGMEIN("GlobOpt.cpp] 133\n");\
        Output::Print(_u("TRACE MemOp:"));\
        OUTPUT_MEMOP_TRACE(loop, instr, __VA_ARGS__)\
    }
#define TRACE_MEMOP_VERBOSE(loop, instr, ...) if(CONFIG_FLAG(Verbose)) {TRACE_MEMOP(loop, instr, __VA_ARGS__)}

#define TRACE_MEMOP_PHASE(phase, loop, instr, ...) \
    if (DO_MEMOP_TRACE_PHASE(phase))\
    {LOGMEIN("GlobOpt.cpp] 141\n");\
        Output::Print(_u("TRACE ") _u(#phase) _u(":"));\
        OUTPUT_MEMOP_TRACE(loop, instr, __VA_ARGS__)\
    }
#define TRACE_MEMOP_PHASE_VERBOSE(phase, loop, instr, ...) if(CONFIG_FLAG(Verbose)) {TRACE_MEMOP_PHASE(phase, loop, instr, __VA_ARGS__)}

#else
#define DO_MEMOP_TRACE()
#define DO_MEMOP_TRACE_PHASE(phase)
#define OUTPUT_MEMOP_TRACE(loop, instr, ...)
#define TRACE_MEMOP(loop, instr, ...)
#define TRACE_MEMOP_VERBOSE(loop, instr, ...)
#define TRACE_MEMOP_PHASE(phase, loop, instr, ...)
#define TRACE_MEMOP_PHASE_VERBOSE(phase, loop, instr, ...)
#endif

class AutoRestoreVal
{
private:
    Value *const originalValue;
    Value *const tempValue;
    Value * *const valueRef;

public:
    AutoRestoreVal(Value *const originalValue, Value * *const tempValueRef)
        : originalValue(originalValue), tempValue(*tempValueRef), valueRef(tempValueRef)
    {LOGMEIN("GlobOpt.cpp] 167\n");
    }

    ~AutoRestoreVal()
    {LOGMEIN("GlobOpt.cpp] 171\n");
        if(*valueRef == tempValue)
        {LOGMEIN("GlobOpt.cpp] 173\n");
            *valueRef = originalValue;
        }
    }

    PREVENT_COPY(AutoRestoreVal);
};

GlobOpt::GlobOpt(Func * func)
    : func(func),
    intConstantToStackSymMap(nullptr),
    intConstantToValueMap(nullptr),
    currentValue(FirstNewValueNumber),
    prePassLoop(nullptr),
    alloc(nullptr),
    isCallHelper(false),
    inInlinedBuiltIn(false),
    rootLoopPrePass(nullptr),
    noImplicitCallUsesToInsert(nullptr),
    valuesCreatedForClone(nullptr),
    valuesCreatedForMerge(nullptr),
    blockData(func),
    instrCountSinceLastCleanUp(0),
    isRecursiveCallOnLandingPad(false),
    updateInductionVariableValueNumber(false),
    isPerformingLoopBackEdgeCompensation(false),
    currentRegion(nullptr),
    changedSymsAfterIncBailoutCandidate(nullptr),
    doTypeSpec(
        !IsTypeSpecPhaseOff(func)),
    doAggressiveIntTypeSpec(
        doTypeSpec &&
        DoAggressiveIntTypeSpec(func)),
    doAggressiveMulIntTypeSpec(
        doTypeSpec &&
        !PHASE_OFF(Js::AggressiveMulIntTypeSpecPhase, func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsAggressiveMulIntTypeSpecDisabled(func->IsLoopBody()))),
    doDivIntTypeSpec(
        doAggressiveIntTypeSpec &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsDivIntTypeSpecDisabled(func->IsLoopBody()))),
    doLossyIntTypeSpec(
        doTypeSpec &&
        DoLossyIntTypeSpec(func)),
    doFloatTypeSpec(
        doTypeSpec &&
        DoFloatTypeSpec(func)),
    doArrayCheckHoist(
        DoArrayCheckHoist(func)),
    doArrayMissingValueCheckHoist(
        doArrayCheckHoist &&
        DoArrayMissingValueCheckHoist(func)),
    doArraySegmentHoist(
        doArrayCheckHoist &&
        DoArraySegmentHoist(ValueType::GetObject(ObjectType::Int32Array), func)),
    doJsArraySegmentHoist(
        doArraySegmentHoist &&
        DoArraySegmentHoist(ValueType::GetObject(ObjectType::Array), func)),
    doArrayLengthHoist(
        doArrayCheckHoist &&
        DoArrayLengthHoist(func)),
    doEliminateArrayAccessHelperCall(
        doArrayCheckHoist &&
        !PHASE_OFF(Js::EliminateArrayAccessHelperCallPhase, func)),
    doTrackRelativeIntBounds(
        doAggressiveIntTypeSpec &&
        DoPathDependentValues() &&
        !PHASE_OFF(Js::Phase::TrackRelativeIntBoundsPhase, func)),
    doBoundCheckElimination(
        doTrackRelativeIntBounds &&
        !PHASE_OFF(Js::Phase::BoundCheckEliminationPhase, func)),
    doBoundCheckHoist(
        doEliminateArrayAccessHelperCall &&
        doBoundCheckElimination &&
        DoConstFold() &&
        !PHASE_OFF(Js::Phase::BoundCheckHoistPhase, func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsBoundCheckHoistDisabled(func->IsLoopBody()))),
    doLoopCountBasedBoundCheckHoist(
        doBoundCheckHoist &&
        !PHASE_OFF(Js::Phase::LoopCountBasedBoundCheckHoistPhase, func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsLoopCountBasedBoundCheckHoistDisabled(func->IsLoopBody()))),
    doPowIntIntTypeSpec(
        doAggressiveIntTypeSpec &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsPowIntIntTypeSpecDisabled())),
    doTagChecks(
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsTagCheckDisabled())),
    isAsmJSFunc(func->GetJITFunctionBody()->IsAsmJsMode())
{LOGMEIN("GlobOpt.cpp] 259\n");
}

void
GlobOpt::BackwardPass(Js::Phase tag)
{LOGMEIN("GlobOpt.cpp] 264\n");
    BEGIN_CODEGEN_PHASE(this->func, tag);

    ::BackwardPass backwardPass(this->func, this, tag);
    backwardPass.Optimize();

    END_CODEGEN_PHASE(this->func, tag);
}

void
GlobOpt::Optimize()
{LOGMEIN("GlobOpt.cpp] 275\n");
    this->objectTypeSyms = nullptr;
    this->func->argInsCount = this->func->GetInParamsCount() - 1;   //Don't include "this" pointer in the count.

    if (!func->DoGlobOpt())
    {LOGMEIN("GlobOpt.cpp] 280\n");
        this->lengthEquivBv = nullptr;
        this->argumentsEquivBv = nullptr;
        this->callerEquivBv = nullptr;

        // Still need to run the dead store phase to calculate the live reg on back edge
        this->BackwardPass(Js::DeadStorePhase);
        CannotAllocateArgumentsObjectOnStack();
        return;
    }

    {
        this->lengthEquivBv = this->func->m_symTable->m_propertyEquivBvMap->Lookup(Js::PropertyIds::length, nullptr); // Used to kill live "length" properties
        this->argumentsEquivBv = func->m_symTable->m_propertyEquivBvMap->Lookup(Js::PropertyIds::arguments, nullptr); // Used to kill live "arguments" properties
        this->callerEquivBv = func->m_symTable->m_propertyEquivBvMap->Lookup(Js::PropertyIds::caller, nullptr); // Used to kill live "caller" properties

        // The backward phase needs the glob opt's allocator to allocate the propertyTypeValueMap
        // in GlobOpt::EnsurePropertyTypeValue and ranges of instructions where int overflow may be ignored.
        // (see BackwardPass::TrackIntUsage)
        PageAllocator * pageAllocator = this->func->m_alloc->GetPageAllocator();
        NoRecoverMemoryJitArenaAllocator localAlloc(_u("BE-GlobOpt"), pageAllocator, Js::Throw::OutOfMemory);
        this->alloc = &localAlloc;

        NoRecoverMemoryJitArenaAllocator localTempAlloc(_u("BE-GlobOpt temp"), pageAllocator, Js::Throw::OutOfMemory);
        this->tempAlloc = &localTempAlloc;

        // The forward passes use info (upwardExposedUses) from the backward pass. This info
        // isn't available for some of the symbols created during the backward pass, or the forward pass.
        // Keep track of the last symbol for which we're guaranteed to have data.
        this->maxInitialSymID = this->func->m_symTable->GetMaxSymID();
        this->BackwardPass(Js::BackwardPhase);
        this->ForwardPass();
    }
    this->BackwardPass(Js::DeadStorePhase);
    this->TailDupPass();
}

bool GlobOpt::ShouldExpectConventionalArrayIndexValue(IR::IndirOpnd *const indirOpnd)
{LOGMEIN("GlobOpt.cpp] 318\n");
    Assert(indirOpnd);

    if(!indirOpnd->GetIndexOpnd())
    {LOGMEIN("GlobOpt.cpp] 322\n");
        return indirOpnd->GetOffset() >= 0;
    }

    IR::RegOpnd *const indexOpnd = indirOpnd->GetIndexOpnd();
    if(indexOpnd->m_sym->m_isNotInt)
    {LOGMEIN("GlobOpt.cpp] 328\n");
        // Typically, single-def or any sym-specific information for type-specialized syms should not be used because all of
        // their defs will not have been accounted for until after the forward pass. But m_isNotInt is only ever changed from
        // false to true, so it's okay in this case.
        return false;
    }

    StackSym *indexVarSym = indexOpnd->m_sym;
    if(indexVarSym->IsTypeSpec())
    {LOGMEIN("GlobOpt.cpp] 337\n");
        indexVarSym = indexVarSym->GetVarEquivSym(nullptr);
        Assert(indexVarSym);
    }
    else if(!IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 342\n");
        // Don't use single-def info or const flags for type-specialized syms, as all of their defs will not have been accounted
        // for until after the forward pass. Also, don't use the const flags in a loop prepass because the const flags may not
        // be up-to-date.
        StackSym *const indexSym = indexOpnd->m_sym;
        if(indexSym->IsIntConst())
        {LOGMEIN("GlobOpt.cpp] 348\n");
            return indexSym->GetIntConstValue() >= 0;
        }
    }

    Value *const indexValue = FindValue(indexVarSym);
    if(!indexValue)
    {LOGMEIN("GlobOpt.cpp] 355\n");
        // Treat it as Uninitialized, assume it's going to be valid
        return true;
    }

    ValueInfo *const indexValueInfo = indexValue->GetValueInfo();
    int32 indexConstantValue;
    if(indexValueInfo->TryGetIntConstantValue(&indexConstantValue))
    {LOGMEIN("GlobOpt.cpp] 363\n");
        return indexConstantValue >= 0;
    }

    if(indexValueInfo->IsUninitialized())
    {LOGMEIN("GlobOpt.cpp] 368\n");
        // Assume it's going to be valid
        return true;
    }

    return indexValueInfo->HasBeenNumber() && !indexValueInfo->HasBeenFloat();
}

//
// Either result is float or 1/x  or cst1/cst2 where cst1%cst2 != 0
//
ValueType GlobOpt::GetDivValueType(IR::Instr* instr, Value* src1Val, Value* src2Val, bool specialize)
{LOGMEIN("GlobOpt.cpp] 380\n");
    ValueInfo *src1ValueInfo = (src1Val ? src1Val->GetValueInfo() : nullptr);
    ValueInfo *src2ValueInfo = (src2Val ? src2Val->GetValueInfo() : nullptr);

    if (instr->IsProfiledInstr() && instr->m_func->HasProfileInfo())
    {LOGMEIN("GlobOpt.cpp] 385\n");
        ValueType resultType = instr->m_func->GetReadOnlyProfileInfo()->GetDivProfileInfo(static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId));
        if (resultType.IsLikelyInt())
        {LOGMEIN("GlobOpt.cpp] 388\n");
            if (specialize && src1ValueInfo && src2ValueInfo
                && ((src1ValueInfo->IsInt() && src2ValueInfo->IsInt()) ||
                (this->DoDivIntTypeSpec() && src1ValueInfo->IsLikelyInt() && src2ValueInfo->IsLikelyInt())))
            {LOGMEIN("GlobOpt.cpp] 392\n");
                return ValueType::GetInt(true);
            }
            return resultType;
        }

        // Consider: Checking that the sources are numbers.
        if (resultType.IsLikelyFloat())
        {LOGMEIN("GlobOpt.cpp] 400\n");
            return ValueType::Float;
        }
        return resultType;
    }
    int32 src1IntConstantValue;
    if(!src1ValueInfo || !src1ValueInfo->TryGetIntConstantValue(&src1IntConstantValue))
    {LOGMEIN("GlobOpt.cpp] 407\n");
        return ValueType::Number;
    }
    if (src1IntConstantValue == 1)
    {LOGMEIN("GlobOpt.cpp] 411\n");
        return ValueType::Float;
    }
    int32 src2IntConstantValue;
    if(!src2Val || !src2ValueInfo->TryGetIntConstantValue(&src2IntConstantValue))
    {LOGMEIN("GlobOpt.cpp] 416\n");
        return ValueType::Number;
    }
    if (src2IntConstantValue     // Avoid divide by zero
        && !(src1IntConstantValue == 0x80000000 && src2IntConstantValue == -1)  // Avoid integer overflow
        && (src1IntConstantValue % src2IntConstantValue) != 0)
    {LOGMEIN("GlobOpt.cpp] 422\n");
        return ValueType::Float;
    }
    return ValueType::Number;
}

void
GlobOpt::ForwardPass()
{LOGMEIN("GlobOpt.cpp] 430\n");
    BEGIN_CODEGEN_PHASE(this->func, Js::ForwardPhase);

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::GlobOptPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {LOGMEIN("GlobOpt.cpp] 435\n");
        this->func->DumpHeader();
    }
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::GlobOptPhase))
    {LOGMEIN("GlobOpt.cpp] 439\n");
        this->TraceSettings();
    }
#endif

    // GetConstantCount() gives us the right size to pick for the SparseArray, but we may need more if we've inlined
    // functions with constants. There will be a gap in the symbol numbering between the main constants and
    // the inlined ones, so we'll most likely need a new array chunk. Make the min size of the array chunks be 64
    // in case we have a main function with very few constants and a bunch of constants from inlined functions.
    this->byteCodeConstantValueArray = SparseArray<Value>::New(this->alloc, max(this->func->GetJITFunctionBody()->GetConstCount(), 64U));
    this->byteCodeConstantValueNumbersBv = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    this->tempBv = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    this->prePassCopyPropSym = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    this->byteCodeUses = nullptr;
    this->propertySymUse = nullptr;

    // changedSymsAfterIncBailoutCandidate helps track building incremental bailout in ForwardPass
    this->changedSymsAfterIncBailoutCandidate = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);

#if DBG
    this->byteCodeUsesBeforeOpt = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldCopyPropPhase) && this->DoFunctionFieldCopyProp())
    {LOGMEIN("GlobOpt.cpp] 461\n");
        Output::Print(_u("TRACE: CanDoFieldCopyProp Func: "));
        this->func->DumpFullFunctionName();
        Output::Print(_u("\n"));
    }
#endif

    OpndList localNoImplicitCallUsesToInsert(alloc);
    this->noImplicitCallUsesToInsert = &localNoImplicitCallUsesToInsert;
    IntConstantToStackSymMap localIntConstantToStackSymMap(alloc);
    this->intConstantToStackSymMap = &localIntConstantToStackSymMap;
    IntConstantToValueMap localIntConstantToValueMap(alloc);
    this->intConstantToValueMap = &localIntConstantToValueMap;
    AddrConstantToValueMap localAddrConstantToValueMap(alloc);
    this->addrConstantToValueMap = &localAddrConstantToValueMap;
    StringConstantToValueMap localStringConstantToValueMap(alloc);
    this->stringConstantToValueMap = &localStringConstantToValueMap;
    SymIdToInstrMap localPrePassInstrMap(alloc);
    this->prePassInstrMap = &localPrePassInstrMap;
    ValueSetByValueNumber localValuesCreatedForClone(alloc, 64);
    this->valuesCreatedForClone = &localValuesCreatedForClone;
    ValueNumberPairToValueMap localValuesCreatedForMerge(alloc, 64);
    this->valuesCreatedForMerge = &localValuesCreatedForMerge;

#if DBG
    BVSparse<JitArenaAllocator> localFinishedStackLiteralInitFld(alloc);
    this->finishedStackLiteralInitFld = &localFinishedStackLiteralInitFld;
#endif

    FOREACH_BLOCK_IN_FUNC_EDITING(block, this->func)
    {LOGMEIN("GlobOpt.cpp] 491\n");
        this->OptBlock(block);
    } NEXT_BLOCK_IN_FUNC_EDITING;

    if (!PHASE_OFF(Js::MemOpPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 496\n");
        ProcessMemOp();
    }

    this->noImplicitCallUsesToInsert = nullptr;
    this->intConstantToStackSymMap = nullptr;
    this->intConstantToValueMap = nullptr;
    this->addrConstantToValueMap = nullptr;
    this->stringConstantToValueMap = nullptr;
#if DBG
    this->finishedStackLiteralInitFld = nullptr;
    uint freedCount = 0;
    uint spilledCount = 0;
#endif

    FOREACH_BLOCK_IN_FUNC(block, this->func)
    {LOGMEIN("GlobOpt.cpp] 512\n");
#if DBG
        if (block->GetDataUseCount() == 0)
        {LOGMEIN("GlobOpt.cpp] 515\n");
            freedCount++;
        }
        else
        {
            spilledCount++;
        }
#endif
        block->SetDataUseCount(0);
        if (block->cloneStrCandidates)
        {LOGMEIN("GlobOpt.cpp] 525\n");
            JitAdelete(this->alloc, block->cloneStrCandidates);
            block->cloneStrCandidates = nullptr;
        }
    } NEXT_BLOCK_IN_FUNC;

    // Make sure we free most of them.
    Assert(freedCount >= spilledCount);

    // this->alloc will be freed right after return, no need to free it here
    this->changedSymsAfterIncBailoutCandidate = nullptr;

    END_CODEGEN_PHASE(this->func, Js::ForwardPhase);
}

void
GlobOpt::OptBlock(BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 542\n");
    if (this->func->m_fg->RemoveUnreachableBlock(block, this))
    {LOGMEIN("GlobOpt.cpp] 544\n");
        GOPT_TRACE(_u("Removing unreachable block #%d\n"), block->GetBlockNum());
        return;
    }

    Loop * loop = block->loop;
    if (loop && block->isLoopHeader)
    {LOGMEIN("GlobOpt.cpp] 551\n");
        if (loop != this->prePassLoop)
        {LOGMEIN("GlobOpt.cpp] 553\n");
            OptLoops(loop);
            if (!this->IsLoopPrePass() && DoFieldPRE(loop))
            {LOGMEIN("GlobOpt.cpp] 556\n");
                // Note: !IsLoopPrePass means this was a root loop pre-pass. FieldPre() is called once per loop.
                this->FieldPRE(loop);

                // Re-optimize the landing pad
                BasicBlock *landingPad = loop->landingPad;
                this->isRecursiveCallOnLandingPad = true;

                this->OptBlock(landingPad);

                this->isRecursiveCallOnLandingPad = false;
                this->currentBlock = block;
            }
        }
    }

    this->currentBlock = block;
    PrepareLoopArrayCheckHoist();

    this->MergePredBlocksValueMaps(block);

    this->intOverflowCurrentlyMattersInRange = true;
    this->intOverflowDoesNotMatterRange = this->currentBlock->intOverflowDoesNotMatterRange;

    if (loop && DoFieldHoisting(loop))
    {LOGMEIN("GlobOpt.cpp] 581\n");
        if (block->isLoopHeader)
        {LOGMEIN("GlobOpt.cpp] 583\n");
            if (!this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 585\n");
                this->PrepareFieldHoisting(loop);
            }
            else if (loop == this->rootLoopPrePass)
            {LOGMEIN("GlobOpt.cpp] 589\n");
                this->PreparePrepassFieldHoisting(loop);
            }
        }
    }
    else
    {
        Assert(!TrackHoistableFields() || !HasHoistableFields(&this->blockData));
        if (!DoFieldCopyProp() && !DoFieldRefOpts())
        {LOGMEIN("GlobOpt.cpp] 598\n");
            this->KillAllFields(blockData.liveFields);
        }
    }

    this->tempAlloc->Reset();

    if(loop && block->isLoopHeader)
    {LOGMEIN("GlobOpt.cpp] 606\n");
        loop->firstValueNumberInLoop = this->currentValue;
    }

    GOPT_TRACE_BLOCK(block, true);

    FOREACH_INSTR_IN_BLOCK_EDITING(instr, instrNext, block)
    {LOGMEIN("GlobOpt.cpp] 613\n");
        GOPT_TRACE_INSTRTRACE(instr);
        BailOutInfo* oldBailOutInfo = nullptr;
        bool isCheckAuxBailoutNeeded = this->func->IsJitInDebugMode() && !this->IsLoopPrePass();
        if (isCheckAuxBailoutNeeded && instr->HasAuxBailOut() && !instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 618\n");
            oldBailOutInfo = instr->GetBailOutInfo();
            Assert(oldBailOutInfo);
        }
        bool isInstrRemoved = false;
        instrNext = this->OptInstr(instr, &isInstrRemoved);

        // If we still have instrs with only aux bail out, convert aux bail out back to regular bail out and fill it.
        // During OptInstr some instr can be moved out to a different block, in this case bailout info is going to be replaced
        // with e.g. loop bailout info which is filled as part of processing that block, thus we don't need to fill it here.
        if (isCheckAuxBailoutNeeded && !isInstrRemoved && instr->HasAuxBailOut() && !instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 629\n");
            if (instr->GetBailOutInfo() == oldBailOutInfo)
            {LOGMEIN("GlobOpt.cpp] 631\n");
                instr->PromoteAuxBailOut();
                FillBailOutInfo(block, instr->GetBailOutInfo());
            }
            else
            {
                AssertMsg(instr->GetBailOutInfo(), "With aux bailout, the bailout info should not be removed by OptInstr.");
            }
        }

    } NEXT_INSTR_IN_BLOCK_EDITING;

    GOPT_TRACE_BLOCK(block, false);
    if (block->loop)
    {LOGMEIN("GlobOpt.cpp] 645\n");
        if (IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 647\n");
            if (DoBoundCheckHoist())
            {LOGMEIN("GlobOpt.cpp] 649\n");
                DetectUnknownChangesToInductionVariables(&block->globOptData);
            }
        }
        else
        {
            isPerformingLoopBackEdgeCompensation = true;

            Assert(this->tempBv->IsEmpty());
            BVSparse<JitArenaAllocator> tempBv2(this->tempAlloc);

            // On loop back-edges, we need to restore the state of the type specialized
            // symbols to that of the loop header.
            FOREACH_SUCCESSOR_BLOCK(succ, block)
            {LOGMEIN("GlobOpt.cpp] 663\n");
                if (succ->isLoopHeader && succ->loop->IsDescendentOrSelf(block->loop))
                {LOGMEIN("GlobOpt.cpp] 665\n");
                    BVSparse<JitArenaAllocator> *liveOnBackEdge = block->loop->regAlloc.liveOnBackEdgeSyms;

                    this->tempBv->Minus(block->loop->varSymsOnEntry, block->globOptData.liveVarSyms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToVar(this->tempBv, block);

                    // Lossy int in the loop header, and no int on the back-edge - need a lossy conversion to int
                    this->tempBv->Minus(block->loop->lossyInt32SymsOnEntry, block->globOptData.liveInt32Syms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToInt32(this->tempBv, block, true /* lossy */);

                    // Lossless int in the loop header, and no lossless int on the back-edge - need a lossless conversion to int
                    this->tempBv->Minus(block->loop->int32SymsOnEntry, block->loop->lossyInt32SymsOnEntry);
                    tempBv2.Minus(block->globOptData.liveInt32Syms, block->globOptData.liveLossyInt32Syms);
                    this->tempBv->Minus(&tempBv2);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToInt32(this->tempBv, block, false /* lossy */);

                    this->tempBv->Minus(block->loop->float64SymsOnEntry, block->globOptData.liveFloat64Syms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToFloat64(this->tempBv, block);

                    // SIMD_JS
                    // Compensate on backedge if sym is live on loop entry but not on backedge
                    this->tempBv->Minus(block->loop->simd128F4SymsOnEntry, block->globOptData.liveSimd128F4Syms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToTypeSpec(this->tempBv, block, TySimd128F4, IR::BailOutSimd128F4Only);

                    this->tempBv->Minus(block->loop->simd128I4SymsOnEntry, block->globOptData.liveSimd128I4Syms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToTypeSpec(this->tempBv, block, TySimd128I4, IR::BailOutSimd128I4Only);

                    // For ints and floats, go aggressive and type specialize in the landing pad any symbol which was specialized on
                    // entry to the loop body (in the loop header), and is still specialized on this tail, but wasn't specialized in
                    // the landing pad.

                    // Lossy int in the loop header and no int in the landing pad - need a lossy conversion to int
                    // (entry.lossyInt32 - landingPad.int32)
                    this->tempBv->Minus(block->loop->lossyInt32SymsOnEntry, block->loop->landingPad->globOptData.liveInt32Syms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToInt32(this->tempBv, block->loop->landingPad, true /* lossy */);

                    // Lossless int in the loop header, and no lossless int in the landing pad - need a lossless conversion to int
                    // ((entry.int32 - entry.lossyInt32) - (landingPad.int32 - landingPad.lossyInt32))
                    this->tempBv->Minus(block->loop->int32SymsOnEntry, block->loop->lossyInt32SymsOnEntry);
                    tempBv2.Minus(
                        block->loop->landingPad->globOptData.liveInt32Syms,
                        block->loop->landingPad->globOptData.liveLossyInt32Syms);
                    this->tempBv->Minus(&tempBv2);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToInt32(this->tempBv, block->loop->landingPad, false /* lossy */);

                    // ((entry.float64 - landingPad.float64) & block.float64)
                    this->tempBv->Minus(block->loop->float64SymsOnEntry, block->loop->landingPad->globOptData.liveFloat64Syms);
                    this->tempBv->And(block->globOptData.liveFloat64Syms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToFloat64(this->tempBv, block->loop->landingPad);

                    // SIMD_JS
                    // compensate on landingpad if live on loopEntry and Backedge.
                    this->tempBv->Minus(block->loop->simd128F4SymsOnEntry, block->loop->landingPad->globOptData.liveSimd128F4Syms);
                    this->tempBv->And(block->globOptData.liveSimd128F4Syms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToTypeSpec(this->tempBv, block->loop->landingPad, TySimd128F4, IR::BailOutSimd128F4Only);

                    this->tempBv->Minus(block->loop->simd128I4SymsOnEntry, block->loop->landingPad->globOptData.liveSimd128I4Syms);
                    this->tempBv->And(block->globOptData.liveSimd128I4Syms);
                    this->tempBv->And(liveOnBackEdge);
                    this->ToTypeSpec(this->tempBv, block->loop->landingPad, TySimd128I4, IR::BailOutSimd128I4Only);

                    // Now that we're done with the liveFields within this loop, trim the set to those syms
                    // that the backward pass told us were live out of the loop.
                    // This assumes we have no further need of the liveFields within the loop.
                    if (block->loop->liveOutFields)
                    {LOGMEIN("GlobOpt.cpp] 740\n");
                        block->globOptData.liveFields->And(block->loop->liveOutFields);
                    }
                }
            } NEXT_SUCCESSOR_BLOCK;

            this->tempBv->ClearAll();

            isPerformingLoopBackEdgeCompensation = false;
        }
    }
    block->globOptData.hasCSECandidates = this->blockData.hasCSECandidates;

#if DBG
    // The set of live lossy int32 syms should be a subset of all live int32 syms
    this->tempBv->And(block->globOptData.liveInt32Syms, block->globOptData.liveLossyInt32Syms);
    Assert(this->tempBv->Count() == block->globOptData.liveLossyInt32Syms->Count());

    // The set of live lossy int32 syms should be a subset of live var or float syms (var or float sym containing the lossless
    // value of the sym should be live)
    this->tempBv->Or(block->globOptData.liveVarSyms, block->globOptData.liveFloat64Syms);
    this->tempBv->And(block->globOptData.liveLossyInt32Syms);
    Assert(this->tempBv->Count() == block->globOptData.liveLossyInt32Syms->Count());

    this->tempBv->ClearAll();
#endif
}

void
GlobOpt::OptLoops(Loop *loop)
{LOGMEIN("GlobOpt.cpp] 770\n");
    Assert(loop != nullptr);
#if DBG
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldCopyPropPhase) &&
        !DoFunctionFieldCopyProp() && DoFieldCopyProp(loop))
    {LOGMEIN("GlobOpt.cpp] 775\n");
        Output::Print(_u("TRACE: CanDoFieldCopyProp Loop: "));
        this->func->DumpFullFunctionName();
        uint loopNumber = loop->GetLoopNumber();
        Assert(loopNumber != Js::LoopHeader::NoLoop);
        Output::Print(_u(" Loop: %d\n"), loopNumber);
    }
#endif

    Loop *previousLoop = this->prePassLoop;
    this->prePassLoop = loop;

    if (previousLoop == nullptr)
    {LOGMEIN("GlobOpt.cpp] 788\n");
        Assert(this->rootLoopPrePass == nullptr);
        this->rootLoopPrePass = loop;
        this->prePassInstrMap->Clear();
        if (loop->parent == nullptr)
        {LOGMEIN("GlobOpt.cpp] 793\n");
            // Outer most loop...
            this->prePassCopyPropSym->ClearAll();
        }
    }

    if (loop->symsUsedBeforeDefined == nullptr)
    {LOGMEIN("GlobOpt.cpp] 800\n");
        loop->symsUsedBeforeDefined = JitAnew(alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->likelyIntSymsUsedBeforeDefined = JitAnew(alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->likelyNumberSymsUsedBeforeDefined = JitAnew(alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->likelySimd128F4SymsUsedBeforeDefined = JitAnew(alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->likelySimd128I4SymsUsedBeforeDefined = JitAnew(alloc, BVSparse<JitArenaAllocator>, this->alloc);

        loop->forceFloat64SymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->forceSimd128F4SymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->forceSimd128I4SymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);

        loop->symsDefInLoop = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->fieldKilled = JitAnew(alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->fieldPRESymStore = JitAnew(alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->allFieldsKilled = false;
    }
    else
    {
        loop->symsUsedBeforeDefined->ClearAll();
        loop->likelyIntSymsUsedBeforeDefined->ClearAll();
        loop->likelyNumberSymsUsedBeforeDefined->ClearAll();
        loop->likelySimd128F4SymsUsedBeforeDefined->ClearAll();
        loop->likelySimd128I4SymsUsedBeforeDefined->ClearAll();

        loop->forceFloat64SymsOnEntry->ClearAll();
        loop->forceSimd128F4SymsOnEntry->ClearAll();
        loop->forceSimd128I4SymsOnEntry->ClearAll();

        loop->symsDefInLoop->ClearAll();
        loop->fieldKilled->ClearAll();
        loop->allFieldsKilled = false;
        loop->initialValueFieldMap.Reset();
    }

    FOREACH_BLOCK_IN_LOOP(block, loop)
    {LOGMEIN("GlobOpt.cpp] 835\n");
        block->SetDataUseCount(block->GetSuccList()->Count());
        OptBlock(block);
    } NEXT_BLOCK_IN_LOOP;

    if (previousLoop == nullptr)
    {LOGMEIN("GlobOpt.cpp] 841\n");
        Assert(this->rootLoopPrePass == loop);
        this->rootLoopPrePass = nullptr;
    }

    this->prePassLoop = previousLoop;
}

void
GlobOpt::TailDupPass()
{
    FOREACH_LOOP_IN_FUNC_EDITING(loop, this->func)
    {LOGMEIN("GlobOpt.cpp] 853\n");
        BasicBlock* header = loop->GetHeadBlock();
        BasicBlock* loopTail = nullptr;
        FOREACH_PREDECESSOR_BLOCK(pred, header)
        {LOGMEIN("GlobOpt.cpp] 857\n");
            if (loop->IsDescendentOrSelf(pred->loop))
            {LOGMEIN("GlobOpt.cpp] 859\n");
                loopTail = pred;
                break;
            }
        } NEXT_PREDECESSOR_BLOCK;

        if (loopTail)
        {LOGMEIN("GlobOpt.cpp] 866\n");
            AssertMsg(loopTail->GetLastInstr()->IsBranchInstr(), "LastInstr of loop should always be a branch no?");

            if (!loopTail->GetPredList()->HasOne())
            {LOGMEIN("GlobOpt.cpp] 870\n");
                TryTailDup(loopTail->GetLastInstr()->AsBranchInstr());
            }
        }
    } NEXT_LOOP_IN_FUNC_EDITING;
}

bool
GlobOpt::TryTailDup(IR::BranchInstr *tailBranch)
{LOGMEIN("GlobOpt.cpp] 879\n");
    if (PHASE_OFF(Js::TailDupPhase, tailBranch->m_func->GetTopFunc()))
    {LOGMEIN("GlobOpt.cpp] 881\n");
        return false;
    }

    if (tailBranch->IsConditional())
    {LOGMEIN("GlobOpt.cpp] 886\n");
        return false;
    }

    IR::Instr *instr;
    uint instrCount = 0;
    for (instr = tailBranch->GetPrevRealInstrOrLabel(); !instr->IsLabelInstr(); instr = instr->GetPrevRealInstrOrLabel())
    {LOGMEIN("GlobOpt.cpp] 893\n");
        if (instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 895\n");
            break;
        }
        if (!OpCodeAttr::CanCSE(instr->m_opcode))
        {LOGMEIN("GlobOpt.cpp] 899\n");
            // Consider: We could be more aggressive here
            break;
        }

        instrCount++;

        if (instrCount > 1)
        {LOGMEIN("GlobOpt.cpp] 907\n");
            // Consider: If copy handled single-def tmps renaming, we could do more instrs
            break;
        }
    }

    if (!instr->IsLabelInstr())
    {LOGMEIN("GlobOpt.cpp] 914\n");
        return false;
    }

    IR::LabelInstr *mergeLabel = instr->AsLabelInstr();
    IR::Instr *mergeLabelPrev = mergeLabel->m_prev;

    // Skip unreferenced labels
    while (mergeLabelPrev->IsLabelInstr() && mergeLabelPrev->AsLabelInstr()->labelRefs.Empty())
    {LOGMEIN("GlobOpt.cpp] 923\n");
        mergeLabelPrev = mergeLabelPrev->m_prev;
    }

    BasicBlock* labelBlock = mergeLabel->GetBasicBlock();
    uint origPredCount = labelBlock->GetPredList()->Count();
    uint dupCount = 0;

    // We are good to go. Let's do the tail duplication.
    FOREACH_SLISTCOUNTED_ENTRY_EDITING(IR::BranchInstr*, branchEntry, &mergeLabel->labelRefs, iter)
    {LOGMEIN("GlobOpt.cpp] 933\n");
        if (branchEntry->IsUnconditional() && !branchEntry->IsMultiBranch() && branchEntry != mergeLabelPrev && branchEntry != tailBranch)
        {LOGMEIN("GlobOpt.cpp] 935\n");
            for (instr = mergeLabel->m_next; instr != tailBranch; instr = instr->m_next)
            {LOGMEIN("GlobOpt.cpp] 937\n");
                branchEntry->InsertBefore(instr->Copy());
            }

            instr = branchEntry;
            branchEntry->ReplaceTarget(mergeLabel, tailBranch->GetTarget());

            while(!instr->IsLabelInstr())
            {LOGMEIN("GlobOpt.cpp] 945\n");
                instr = instr->m_prev;
            }
            BasicBlock* branchBlock = instr->AsLabelInstr()->GetBasicBlock();

            labelBlock->RemovePred(branchBlock, func->m_fg);
            func->m_fg->AddEdge(branchBlock, tailBranch->GetTarget()->GetBasicBlock());

            dupCount++;
        }
    } NEXT_SLISTCOUNTED_ENTRY_EDITING;

    // If we've duplicated everywhere, tail block is dead and should be removed.
    if (dupCount == origPredCount)
    {LOGMEIN("GlobOpt.cpp] 959\n");
        AssertMsg(mergeLabel->IsUnreferenced(), "Should not remove block with referenced label.");
        func->m_fg->RemoveBlock(labelBlock, nullptr, true);
    }

    return true;
}

void
GlobOpt::MergePredBlocksValueMaps(BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 969\n");
    Assert(!this->isCallHelper);

    if (!this->isRecursiveCallOnLandingPad)
    {LOGMEIN("GlobOpt.cpp] 973\n");
        this->NulloutBlockData(&this->blockData);
    }
    else
    {
        // If we are going over the landing pad again after field PRE, just start again
        // with the value table where we left off.
        this->CopyBlockData(&this->blockData, &block->globOptData);
        return;
    }

    BVSparse<JitArenaAllocator> symsRequiringCompensation(tempAlloc);
    {LOGMEIN("GlobOpt.cpp] 985\n");
        BVSparse<JitArenaAllocator> symsCreatedForMerge(tempAlloc);
        bool forceTypeSpecOnLoopHeader = true;
        FOREACH_PREDECESSOR_BLOCK(pred, block)
        {LOGMEIN("GlobOpt.cpp] 989\n");
            if (pred->globOptData.callSequence && pred->globOptData.callSequence->Empty())
            {LOGMEIN("GlobOpt.cpp] 991\n");
                JitAdelete(this->alloc, pred->globOptData.callSequence);
                pred->globOptData.callSequence = nullptr;
            }
            if (block->isLoopHeader && this->IsLoopPrePass() && this->prePassLoop == block->loop && block->loop->IsDescendentOrSelf(pred->loop))
            {LOGMEIN("GlobOpt.cpp] 996\n");
                // Loop back-edge.
                // First pass on loop runs optimistically, without doing transforms.
                // Skip this edge for now.
                continue;
            }

            PathDependentInfo *const pathDependentInfo = __edge->GetPathDependentInfo();
            PathDependentInfoToRestore pathDependentInfoToRestore;
            if (pathDependentInfo)
            {LOGMEIN("GlobOpt.cpp] 1006\n");
                pathDependentInfoToRestore = UpdatePathDependentInfo(pathDependentInfo);
            }

            Assert(pred->GetDataUseCount());

            // First pred?
            if (this->blockData.symToValueMap == nullptr)
            {LOGMEIN("GlobOpt.cpp] 1014\n");
                // Only one edge?
                if (pred->GetSuccList()->HasOne() && block->GetPredList()->HasOne() && block->loop == nullptr)
                {LOGMEIN("GlobOpt.cpp] 1017\n");
                    this->ReuseBlockData(&this->blockData, &pred->globOptData);

                    // Don't need to restore the old value info
                    pathDependentInfoToRestore.Clear();
                }
                else
                {
                    this->CloneBlockData(currentBlock, &this->blockData, pred);
                }
            }
            else
            {
                const bool isLoopPrePass = IsLoopPrePass();
                this->MergeBlockData(
                    &this->blockData,
                    block,
                    pred,
                    isLoopPrePass ? nullptr : &symsRequiringCompensation,
                    isLoopPrePass ? nullptr : &symsCreatedForMerge,
                    forceTypeSpecOnLoopHeader);
                forceTypeSpecOnLoopHeader = false; // can force type-spec on the loop header only for the first back edge.
            }

            // Restore the value for the next edge
            if (pathDependentInfo)
            {
                RestorePathDependentInfo(pathDependentInfo, pathDependentInfoToRestore);
                __edge->ClearPathDependentInfo(this->alloc);
            }

        } NEXT_PREDECESSOR_BLOCK;
    }

    // Consider: We can recreate values for hoisted field so it can copy prop out of the loop
    if (this->blockData.symToValueMap == nullptr)
    {LOGMEIN("GlobOpt.cpp] 1053\n");
        Assert(this->blockData.hoistableFields == nullptr);
        this->InitBlockData();
    }
    else if (this->blockData.hoistableFields)
    {LOGMEIN("GlobOpt.cpp] 1058\n");
        Assert(TrackHoistableFields());
        this->blockData.hoistableFields->And(this->blockData.liveFields);
    }

    if (!this->DoObjTypeSpec())
    {LOGMEIN("GlobOpt.cpp] 1064\n");
        // Object type specialization is off, but if copy prop is on (e.g., /force:fieldhoist) we're not clearing liveFields,
        // so we may be letting type syms slip through this block.
        this->KillAllObjectTypes();
    }

    this->CopyBlockData(&block->globOptData, &this->blockData);

    if (this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 1073\n");
        Assert(block->loop);

        if(DoBoundCheckHoist())
        {LOGMEIN("GlobOpt.cpp] 1077\n");
            SetInductionVariableValueNumbers(&blockData);
        }

        if (block->isLoopHeader && this->rootLoopPrePass == block->loop)
        {LOGMEIN("GlobOpt.cpp] 1082\n");
            // Capture bail out info in case we have optimization that needs it
            Assert(block->loop->bailOutInfo == nullptr);
            IR::Instr * firstInstr = block->GetFirstInstr();
            block->loop->bailOutInfo = JitAnew(this->func->m_alloc, BailOutInfo,
                firstInstr->GetByteCodeOffset(), firstInstr->m_func);
            this->FillBailOutInfo(block, block->loop->bailOutInfo);
#if ENABLE_DEBUG_CONFIG_OPTIONS
            block->loop->bailOutInfo->bailOutOpcode = Js::OpCode::LoopBodyStart;
#endif
        }

        // If loop pre-pass, don't insert convert from type-spec to var
        return;
    }

    this->CleanUpValueMaps();
    Sym *symIV = nullptr;

    // Clean up the syms requiring compensation by checking the final value in the merged block to see if the sym still requires
    // compensation. All the while, create a mapping from sym to value info in the merged block. This dictionary helps avoid a
    // value lookup in the merged block per predecessor.
    SymToValueInfoMap symsRequiringCompensationToMergedValueInfoMap(tempAlloc);
    if(!symsRequiringCompensation.IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 1106\n");
        const SymTable *const symTable = func->m_symTable;
        GlobHashTable *const symToValueMap = blockData.symToValueMap;
        FOREACH_BITSET_IN_SPARSEBV(id, &symsRequiringCompensation)
        {LOGMEIN("GlobOpt.cpp] 1110\n");
            Sym *const sym = symTable->Find(id);
            Assert(sym);

            Value *const value = FindValue(symToValueMap, sym);
            if(!value)
            {LOGMEIN("GlobOpt.cpp] 1116\n");
                continue;
            }

            ValueInfo *const valueInfo = value->GetValueInfo();
            if(!valueInfo->IsArrayValueInfo())
            {LOGMEIN("GlobOpt.cpp] 1122\n");
                continue;
            }

            // At least one new sym was created while merging and associated with the merged value info, so those syms will
            // require compensation in predecessors. For now, the dead store phase is relied upon to remove compensation that is
            // dead due to no further uses of the new sym.
            symsRequiringCompensationToMergedValueInfoMap.Add(sym, valueInfo);
        } NEXT_BITSET_IN_SPARSEBV;
        symsRequiringCompensation.ClearAll();
    }

    if (block->isLoopHeader)
    {LOGMEIN("GlobOpt.cpp] 1135\n");
        // Values on the back-edge in the prepass may be conservative for syms defined in the loop, and type specialization in
        // the prepass is not reflective of the value, but rather, is used to determine whether the sym should be specialized
        // around the loop. Additionally, some syms that are used before defined in the loop may be specialized in the loop
        // header despite not being specialized in the landing pad. Now that the type specialization bit-vectors are merged,
        // specialize the corresponding value infos in the loop header too.

        Assert(tempBv->IsEmpty());
        Loop *const loop = block->loop;
        SymTable *const symTable = func->m_symTable;
        GlobHashTable *const symToValueMap = blockData.symToValueMap;
        JitArenaAllocator *const alloc = this->alloc;

        // Int-specialized syms
        tempBv->Or(loop->likelyIntSymsUsedBeforeDefined, loop->symsDefInLoop);
        tempBv->And(blockData.liveInt32Syms);
        tempBv->Minus(blockData.liveLossyInt32Syms);
        FOREACH_BITSET_IN_SPARSEBV(id, tempBv)
        {LOGMEIN("GlobOpt.cpp] 1153\n");
            StackSym *const varSym = symTable->FindStackSym(id);
            Assert(varSym);
            Value *const value = FindValue(symToValueMap, varSym);
            Assert(value);
            ValueInfo *const valueInfo = value->GetValueInfo();
            if(!valueInfo->IsInt())
            {
                ChangeValueInfo(nullptr, value, valueInfo->SpecializeToInt32(alloc));
            }
        } NEXT_BITSET_IN_SPARSEBV;

        // Float-specialized syms
        tempBv->Or(loop->likelyNumberSymsUsedBeforeDefined, loop->symsDefInLoop);
        tempBv->Or(loop->forceFloat64SymsOnEntry);
        tempBv->And(blockData.liveFloat64Syms);
        GlobOptBlockData &landingPadBlockData = loop->landingPad->globOptData;
        FOREACH_BITSET_IN_SPARSEBV(id, tempBv)
        {LOGMEIN("GlobOpt.cpp] 1171\n");
            StackSym *const varSym = symTable->FindStackSym(id);
            Assert(varSym);

            // If the type-spec sym is null or if the sym is not float-specialized in the loop landing pad, the sym may have
            // been merged to float on a loop back-edge when it was live as float on the back-edge, and live as int in the loop
            // header. In this case, compensation inserted in the loop landing pad will use BailOutNumberOnly, and so it is
            // guaranteed that the value will be float. Otherwise, if the type-spec sym exists, its field can be checked to see
            // if it's prevented from being anything but a number.
            StackSym *const typeSpecSym = varSym->GetFloat64EquivSym(nullptr);
            if(!typeSpecSym ||
                typeSpecSym->m_requiresBailOnNotNumber ||
                !IsFloat64TypeSpecialized(varSym, &landingPadBlockData))
            {LOGMEIN("GlobOpt.cpp] 1184\n");
                Value *const value = FindValue(symToValueMap, varSym);
                if(value)
                {LOGMEIN("GlobOpt.cpp] 1187\n");
                    ValueInfo *const valueInfo = value->GetValueInfo();
                    if(!valueInfo->IsNumber())
                    {
                        ChangeValueInfo(block, value, valueInfo->SpecializeToFloat64(alloc));
                    }
                }
                else
                {
                    SetValue(&block->globOptData, NewGenericValue(ValueType::Float), varSym);
                }
            }
        } NEXT_BITSET_IN_SPARSEBV;

        // SIMD_JS
        // Simd128 type-spec syms
        BVSparse<JitArenaAllocator> tempBv2(this->tempAlloc);

        // For syms we made alive in loop header because of hoisting, use-before-def, or def in Loop body, set their valueInfo to definite.
        // Make live on header AND in one of forceSimd128* or likelySimd128* vectors.
        tempBv->Or(loop->likelySimd128F4SymsUsedBeforeDefined, loop->symsDefInLoop);
        tempBv->Or(loop->likelySimd128I4SymsUsedBeforeDefined);
        tempBv->Or(loop->forceSimd128F4SymsOnEntry);
        tempBv->Or(loop->forceSimd128I4SymsOnEntry);
        tempBv2.Or(blockData.liveSimd128F4Syms, blockData.liveSimd128I4Syms);
        tempBv->And(&tempBv2);

        FOREACH_BITSET_IN_SPARSEBV(id, tempBv)
        {LOGMEIN("GlobOpt.cpp] 1215\n");
            StackSym * typeSpecSym = nullptr;
            StackSym *const varSym = symTable->FindStackSym(id);
            Assert(varSym);

            if (blockData.liveSimd128F4Syms->Test(id))
            {LOGMEIN("GlobOpt.cpp] 1221\n");
                typeSpecSym = varSym->GetSimd128F4EquivSym(nullptr);


                if (!typeSpecSym || !IsSimd128F4TypeSpecialized(varSym, &landingPadBlockData))
                {LOGMEIN("GlobOpt.cpp] 1226\n");
                    Value *const value = FindValue(symToValueMap, varSym);
                    if (value)
                    {LOGMEIN("GlobOpt.cpp] 1229\n");
                        ValueInfo *const valueInfo = value->GetValueInfo();
                        if (!valueInfo->IsSimd128Float32x4())
                        {
                            ChangeValueInfo(block, value, valueInfo->SpecializeToSimd128F4(alloc));
                        }
                    }
                    else
                    {
                        SetValue(&block->globOptData, NewGenericValue(ValueType::GetSimd128(ObjectType::Simd128Float32x4), varSym), varSym);
                    }
                }
            }
            else if (blockData.liveSimd128I4Syms->Test(id))
            {LOGMEIN("GlobOpt.cpp] 1243\n");

                typeSpecSym = varSym->GetSimd128I4EquivSym(nullptr);
                if (!typeSpecSym || !IsSimd128I4TypeSpecialized(varSym, &landingPadBlockData))
                {LOGMEIN("GlobOpt.cpp] 1247\n");
                    Value *const value = FindValue(symToValueMap, varSym);
                    if (value)
                    {LOGMEIN("GlobOpt.cpp] 1250\n");
                        ValueInfo *const valueInfo = value->GetValueInfo();
                        if (!valueInfo->IsSimd128Int32x4())
                        {
                            ChangeValueInfo(block, value, valueInfo->SpecializeToSimd128I4(alloc));
                        }
                    }
                    else
                    {
                        SetValue(&block->globOptData, NewGenericValue(ValueType::GetSimd128(ObjectType::Simd128Int32x4), varSym), varSym);
                    }
                }
            }
            else
            {
                Assert(UNREACHED);
            }
        } NEXT_BITSET_IN_SPARSEBV;
        tempBv->ClearAll();
    }

    // We need to handle the case where a symbol is type-spec'd coming from some predecessors,
    // but not from others.
    //
    // We can do this by inserting the right conversion in the predecessor block, but we
    // can only do this if we are the first successor of that block, since the previous successors
    // would have already been processed.  Instead, we'll need to break the edge and insert a block
    // (airlock block) to put in the conversion code.
    Assert(this->tempBv->IsEmpty());

    BVSparse<JitArenaAllocator> tempBv2(this->tempAlloc);
    BVSparse<JitArenaAllocator> tempBv3(this->tempAlloc);
    BVSparse<JitArenaAllocator> tempBv4(this->tempAlloc);

    // SIMD_JS
    BVSparse<JitArenaAllocator> simd128F4SymsToUnbox(this->tempAlloc);
    BVSparse<JitArenaAllocator> simd128I4SymsToUnbox(this->tempAlloc);

    FOREACH_PREDECESSOR_EDGE_EDITING(edge, block, iter)
    {LOGMEIN("GlobOpt.cpp] 1289\n");
        BasicBlock *pred = edge->GetPred();

        if (pred->loop && pred->loop->GetHeadBlock() == block)
        {LOGMEIN("GlobOpt.cpp] 1293\n");
            pred->DecrementDataUseCount();
            // Skip loop back-edges. We will handle these when we get to the exit blocks.
            continue;
        }

        BasicBlock *orgPred = nullptr;
        if (pred->isAirLockCompensationBlock)
        {LOGMEIN("GlobOpt.cpp] 1301\n");
            Assert(pred->GetPredList()->HasOne());
            orgPred = pred;
            pred = (pred->GetPredList()->Head())->GetPred();
        }

        // Lossy int in the merged block, and no int in the predecessor - need a lossy conversion to int
        tempBv2.Minus(this->blockData.liveLossyInt32Syms, pred->globOptData.liveInt32Syms);

        // Lossless int in the merged block, and no lossless int in the predecessor - need a lossless conversion to int
        tempBv3.Minus(this->blockData.liveInt32Syms, this->blockData.liveLossyInt32Syms);
        this->tempBv->Minus(pred->globOptData.liveInt32Syms, pred->globOptData.liveLossyInt32Syms);
        tempBv3.Minus(this->tempBv);

        this->tempBv->Minus(this->blockData.liveVarSyms, pred->globOptData.liveVarSyms);
        tempBv4.Minus(this->blockData.liveFloat64Syms, pred->globOptData.liveFloat64Syms);

        bool symIVNeedsSpecializing = (symIV && !pred->globOptData.liveInt32Syms->Test(symIV->m_id) && !tempBv3.Test(symIV->m_id));

        // SIMD_JS
        simd128F4SymsToUnbox.Minus(this->blockData.liveSimd128F4Syms, pred->globOptData.liveSimd128F4Syms);
        simd128I4SymsToUnbox.Minus(this->blockData.liveSimd128I4Syms, pred->globOptData.liveSimd128I4Syms);


        if (!this->tempBv->IsEmpty() ||
            !tempBv2.IsEmpty() ||
            !tempBv3.IsEmpty() ||
            !tempBv4.IsEmpty() ||
            !simd128F4SymsToUnbox.IsEmpty() ||
            !simd128I4SymsToUnbox.IsEmpty() ||
            symIVNeedsSpecializing ||
            symsRequiringCompensationToMergedValueInfoMap.Count() != 0)
        {LOGMEIN("GlobOpt.cpp] 1333\n");
            // We can't un-specialize a symbol in a predecessor if we've already processed
            // a successor of that block. Instead, insert a new block on the flow edge
            // (an airlock block) and do the un-specialization there.
            //
            // Alternatively, the current block could be an exit block out of this loop, and so the predecessor may exit the
            // loop. In that case, if the predecessor may continue into the loop without exiting, then we need an airlock block
            // to do the appropriate conversions only on the exit path (preferring not to do the conversions inside the loop).
            // If, on the other hand, the predecessor always flows into the current block, then it always exits, so we don't need
            // an airlock block and can just do the conversions in the predecessor.
            if (pred->GetSuccList()->Head()->GetSucc() != block ||
                (pred->loop && pred->loop->parent == block->loop && pred->GetSuccList()->Count() > 1))
            {LOGMEIN("GlobOpt.cpp] 1345\n");
                BasicBlock *airlockBlock = nullptr;
                if (!orgPred)
                {LOGMEIN("GlobOpt.cpp] 1348\n");
                    GOPT_TRACE(_u("Inserting airlock block to convert syms to var between block %d and %d\n"),
                        pred->GetBlockNum(), block->GetBlockNum());
                    airlockBlock = this->func->m_fg->InsertAirlockBlock(edge);
                }
                else
                {
                    Assert(orgPred->isAirLockCompensationBlock);
                    airlockBlock = orgPred;
                    pred->DecrementDataUseCount();
                    airlockBlock->isAirLockCompensationBlock = false; // This is airlock block now. So remove the attribute.
                }
                this->CloneBlockData(airlockBlock, pred);

                pred = airlockBlock;
            }
            if (!this->tempBv->IsEmpty())
            {LOGMEIN("GlobOpt.cpp] 1365\n");
                this->ToVar(this->tempBv, pred);
            }
            if (!tempBv2.IsEmpty())
            {LOGMEIN("GlobOpt.cpp] 1369\n");
                this->ToInt32(&tempBv2, pred, true /* lossy */);
            }
            if (!tempBv3.IsEmpty())
            {LOGMEIN("GlobOpt.cpp] 1373\n");
                this->ToInt32(&tempBv3, pred, false /* lossy */);
            }
            if (!tempBv4.IsEmpty())
            {LOGMEIN("GlobOpt.cpp] 1377\n");
                this->ToFloat64(&tempBv4, pred);
            }
            if (symIVNeedsSpecializing)
            {LOGMEIN("GlobOpt.cpp] 1381\n");
                this->tempBv->ClearAll();
                this->tempBv->Set(symIV->m_id);
                this->ToInt32(this->tempBv, pred, false /* lossy */);
            }
            if(symsRequiringCompensationToMergedValueInfoMap.Count() != 0)
            {
                InsertValueCompensation(pred, symsRequiringCompensationToMergedValueInfoMap);
            }

            // SIMD_JS
            if (!simd128F4SymsToUnbox.IsEmpty())
            {LOGMEIN("GlobOpt.cpp] 1393\n");
                this->ToTypeSpec(&simd128F4SymsToUnbox, pred, TySimd128F4, IR::BailOutSimd128F4Only);
            }

            if (!simd128I4SymsToUnbox.IsEmpty())
            {LOGMEIN("GlobOpt.cpp] 1398\n");
                this->ToTypeSpec(&simd128I4SymsToUnbox, pred, TySimd128I4, IR::BailOutSimd128I4Only);
            }
        }
    } NEXT_PREDECESSOR_EDGE_EDITING;

    FOREACH_PREDECESSOR_EDGE(edge, block)
    {LOGMEIN("GlobOpt.cpp] 1405\n");
        // Peak Memory optimization:
        // These are in an arena, but putting them on the free list greatly reduces
        // the peak memory used by the global optimizer for complex flow graphs.
        BasicBlock *pred = edge->GetPred();
        if (!block->isLoopHeader || block->loop != pred->loop)
        {LOGMEIN("GlobOpt.cpp] 1411\n");
            // Skip airlock compensation block as we are not going to walk this block.
            if (pred->isAirLockCompensationBlock)
            {LOGMEIN("GlobOpt.cpp] 1414\n");
                pred->DecrementDataUseCount();
                Assert(pred->GetPredList()->HasOne());
                pred = (pred->GetPredList()->Head())->GetPred();
            }

            if (pred->DecrementDataUseCount() == 0 && (!block->loop || block->loop->landingPad != pred))
            {LOGMEIN("GlobOpt.cpp] 1421\n");
                if (!(pred->GetSuccList()->HasOne() && block->GetPredList()->HasOne() && block->loop == nullptr))
                {LOGMEIN("GlobOpt.cpp] 1423\n");
                    this->DeleteBlockData(&pred->globOptData);
                }
                else
                {
                    this->NulloutBlockData(&pred->globOptData);
                }
            }
        }
    } NEXT_PREDECESSOR_EDGE;

    this->tempBv->ClearAll();
    Assert(!this->IsLoopPrePass());   // We already early return if we are in prepass

    if (block->isLoopHeader)
    {LOGMEIN("GlobOpt.cpp] 1438\n");
        Loop *const loop = block->loop;

        // Save values live on loop entry, such that we can adjust the state of the
        // values on the back-edge to match.
        loop->varSymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->varSymsOnEntry->Copy(block->globOptData.liveVarSyms);

        loop->int32SymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->int32SymsOnEntry->Copy(block->globOptData.liveInt32Syms);

        loop->lossyInt32SymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->lossyInt32SymsOnEntry->Copy(block->globOptData.liveLossyInt32Syms);

        loop->float64SymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->float64SymsOnEntry->Copy(block->globOptData.liveFloat64Syms);

        // SIMD_JS
        loop->simd128F4SymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->simd128F4SymsOnEntry->Copy(block->globOptData.liveSimd128F4Syms);

        loop->simd128I4SymsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->simd128I4SymsOnEntry->Copy(block->globOptData.liveSimd128I4Syms);


        loop->liveFieldsOnEntry = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        loop->liveFieldsOnEntry->Copy(block->globOptData.liveFields);

        if(DoBoundCheckHoist() && loop->inductionVariables)
        {
            FinalizeInductionVariables(loop, &blockData);
            if(DoLoopCountBasedBoundCheckHoist())
            {
                DetermineDominatingLoopCountableBlock(loop, block);
            }
        }
    }
    else if (!block->loop)
    {LOGMEIN("GlobOpt.cpp] 1476\n");
        block->SetDataUseCount(block->GetSuccList()->Count());
    }
    else if(block == block->loop->dominatingLoopCountableBlock)
    {LOGMEIN("GlobOpt.cpp] 1480\n");
        DetermineLoopCount(block->loop);
    }
}

void
GlobOpt::NulloutBlockData(GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 1487\n");
    data->symToValueMap = nullptr;
    data->exprToValueMap = nullptr;
    data->liveFields = nullptr;
    data->maybeWrittenTypeSyms = nullptr;
    data->isTempSrc = nullptr;
    data->liveVarSyms = nullptr;
    data->liveInt32Syms = nullptr;
    data->liveLossyInt32Syms = nullptr;
    data->liveFloat64Syms = nullptr;
    // SIMD_JS
    data->liveSimd128F4Syms = nullptr;
    data->liveSimd128I4Syms = nullptr;

    data->hoistableFields = nullptr;
    data->argObjSyms = nullptr;
    data->maybeTempObjectSyms = nullptr;
    data->canStoreTempObjectSyms = nullptr;
    data->valuesToKillOnCalls = nullptr;
    data->inductionVariables = nullptr;
    data->availableIntBoundChecks = nullptr;
    data->callSequence = nullptr;
    data->startCallCount = 0;
    data->argOutCount = 0;
    data->totalOutParamCount = 0;
    data->inlinedArgOutCount = 0;
    data->hasCSECandidates = false;
    data->curFunc = this->func;

    data->stackLiteralInitFldDataMap = nullptr;

    data->capturedValues = nullptr;
    data->changedSyms = nullptr;

    data->OnDataUnreferenced();
}

void
GlobOpt::InitBlockData()
{LOGMEIN("GlobOpt.cpp] 1526\n");
    GlobOptBlockData *const data = &this->blockData;
    JitArenaAllocator *const alloc = this->alloc;

    data->symToValueMap = GlobHashTable::New(alloc, 64);
    data->exprToValueMap = ExprHashTable::New(alloc, 64);
    data->liveFields = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    data->liveArrayValues = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    data->isTempSrc = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    data->liveVarSyms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    data->liveInt32Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    data->liveLossyInt32Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    data->liveFloat64Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);

    // SIMD_JS
    data->liveSimd128F4Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    data->liveSimd128I4Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);

    data->hoistableFields = nullptr;
    data->argObjSyms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    data->maybeTempObjectSyms = nullptr;
    data->canStoreTempObjectSyms = nullptr;
    data->valuesToKillOnCalls = JitAnew(alloc, ValueSet, alloc);

    if(DoBoundCheckHoist())
    {LOGMEIN("GlobOpt.cpp] 1551\n");
        data->inductionVariables = IsLoopPrePass() ? JitAnew(alloc, InductionVariableSet, alloc) : nullptr;
        data->availableIntBoundChecks = JitAnew(alloc, IntBoundCheckSet, alloc);
    }

    data->maybeWrittenTypeSyms = nullptr;
    data->callSequence = nullptr;
    data->startCallCount = 0;
    data->argOutCount = 0;
    data->totalOutParamCount = 0;
    data->inlinedArgOutCount = 0;
    data->hasCSECandidates = false;
    data->curFunc = this->func;

    data->stackLiteralInitFldDataMap = nullptr;

    data->changedSyms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);

    data->OnDataInitialized(alloc);
}

void
GlobOpt::ReuseBlockData(GlobOptBlockData *toData, GlobOptBlockData *fromData)
{LOGMEIN("GlobOpt.cpp] 1574\n");
    // Reuse dead map
    toData->symToValueMap = fromData->symToValueMap;
    toData->exprToValueMap = fromData->exprToValueMap;
    toData->liveFields = fromData->liveFields;
    toData->liveArrayValues = fromData->liveArrayValues;
    toData->maybeWrittenTypeSyms = fromData->maybeWrittenTypeSyms;
    toData->isTempSrc = fromData->isTempSrc;
    toData->liveVarSyms = fromData->liveVarSyms;
    toData->liveInt32Syms = fromData->liveInt32Syms;
    toData->liveLossyInt32Syms = fromData->liveLossyInt32Syms;
    toData->liveFloat64Syms = fromData->liveFloat64Syms;

    // SIMD_JS
    toData->liveSimd128F4Syms = fromData->liveSimd128F4Syms;
    toData->liveSimd128I4Syms = fromData->liveSimd128I4Syms;

    if (TrackHoistableFields())
    {LOGMEIN("GlobOpt.cpp] 1592\n");
        toData->hoistableFields = fromData->hoistableFields;
    }

    if (TrackArgumentsObject())
    {LOGMEIN("GlobOpt.cpp] 1597\n");
        toData->argObjSyms = fromData->argObjSyms;
    }
    toData->maybeTempObjectSyms = fromData->maybeTempObjectSyms;
    toData->canStoreTempObjectSyms = fromData->canStoreTempObjectSyms;
    toData->curFunc = fromData->curFunc;

    toData->valuesToKillOnCalls = fromData->valuesToKillOnCalls;
    toData->inductionVariables = fromData->inductionVariables;
    toData->availableIntBoundChecks = fromData->availableIntBoundChecks;
    toData->callSequence = fromData->callSequence;

    toData->startCallCount = fromData->startCallCount;
    toData->argOutCount = fromData->argOutCount;
    toData->totalOutParamCount = fromData->totalOutParamCount;
    toData->inlinedArgOutCount = fromData->inlinedArgOutCount;
    toData->hasCSECandidates = fromData->hasCSECandidates;

    toData->stackLiteralInitFldDataMap = fromData->stackLiteralInitFldDataMap;

    toData->changedSyms = fromData->changedSyms;
    toData->changedSyms->ClearAll();

    toData->OnDataReused(fromData);
}

void
GlobOpt::CopyBlockData(GlobOptBlockData *toData, GlobOptBlockData *fromData)
{LOGMEIN("GlobOpt.cpp] 1625\n");
    toData->symToValueMap = fromData->symToValueMap;
    toData->exprToValueMap = fromData->exprToValueMap;
    toData->liveFields = fromData->liveFields;
    toData->liveArrayValues = fromData->liveArrayValues;
    toData->maybeWrittenTypeSyms = fromData->maybeWrittenTypeSyms;
    toData->isTempSrc = fromData->isTempSrc;
    toData->liveVarSyms = fromData->liveVarSyms;
    toData->liveInt32Syms = fromData->liveInt32Syms;
    toData->liveLossyInt32Syms = fromData->liveLossyInt32Syms;
    toData->liveFloat64Syms = fromData->liveFloat64Syms;

    // SIMD_JS
    toData->liveSimd128F4Syms = fromData->liveSimd128F4Syms;
    toData->liveSimd128I4Syms = fromData->liveSimd128I4Syms;

    toData->hoistableFields = fromData->hoistableFields;
    toData->argObjSyms = fromData->argObjSyms;
    toData->maybeTempObjectSyms = fromData->maybeTempObjectSyms;
    toData->canStoreTempObjectSyms = fromData->canStoreTempObjectSyms;
    toData->curFunc = fromData->curFunc;
    toData->valuesToKillOnCalls = fromData->valuesToKillOnCalls;
    toData->inductionVariables = fromData->inductionVariables;
    toData->availableIntBoundChecks = fromData->availableIntBoundChecks;
    toData->callSequence = fromData->callSequence;
    toData->startCallCount = fromData->startCallCount;
    toData->argOutCount = fromData->argOutCount;
    toData->totalOutParamCount = fromData->totalOutParamCount;
    toData->inlinedArgOutCount = fromData->inlinedArgOutCount;
    toData->hasCSECandidates = fromData->hasCSECandidates;

    toData->changedSyms = fromData->changedSyms;

    toData->stackLiteralInitFldDataMap = fromData->stackLiteralInitFldDataMap;
    toData->OnDataReused(fromData);
}

void GlobOpt::CloneBlockData(BasicBlock *const toBlock, BasicBlock *const fromBlock)
{
    CloneBlockData(toBlock, &toBlock->globOptData, fromBlock);
}

void GlobOpt::CloneBlockData(BasicBlock *const toBlock, GlobOptBlockData *const toData, BasicBlock *const fromBlock)
{LOGMEIN("GlobOpt.cpp] 1668\n");
    GlobOptBlockData *const fromData = &fromBlock->globOptData;
    JitArenaAllocator *const alloc = this->alloc;

    toData->symToValueMap = fromData->symToValueMap->Copy();
    toData->exprToValueMap = fromData->exprToValueMap->Copy();

    // Clone the values as well to allow for flow-sensitive ValueInfo
    this->CloneValues(toBlock, toData, fromData);

    if(DoBoundCheckHoist())
    {
        CloneBoundCheckHoistBlockData(toBlock, toData, fromBlock, fromData);
    }

    toData->liveFields = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->liveFields->Copy(fromData->liveFields);

    toData->liveArrayValues = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->liveArrayValues->Copy(fromData->liveArrayValues);

    if (fromData->maybeWrittenTypeSyms)
    {LOGMEIN("GlobOpt.cpp] 1690\n");
        toData->maybeWrittenTypeSyms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
        toData->maybeWrittenTypeSyms->Copy(fromData->maybeWrittenTypeSyms);
    }

    toData->isTempSrc = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->isTempSrc->Copy(fromData->isTempSrc);

    toData->liveVarSyms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->liveVarSyms->Copy(fromData->liveVarSyms);

    toData->liveInt32Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->liveInt32Syms->Copy(fromData->liveInt32Syms);

    toData->liveLossyInt32Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->liveLossyInt32Syms->Copy(fromData->liveLossyInt32Syms);

    toData->liveFloat64Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->liveFloat64Syms->Copy(fromData->liveFloat64Syms);

    // SIMD_JS
    toData->liveSimd128F4Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->liveSimd128F4Syms->Copy(fromData->liveSimd128F4Syms);

    toData->liveSimd128I4Syms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->liveSimd128I4Syms->Copy(fromData->liveSimd128I4Syms);

    if (TrackHoistableFields())
    {LOGMEIN("GlobOpt.cpp] 1718\n");
        if (fromData->hoistableFields)
        {LOGMEIN("GlobOpt.cpp] 1720\n");
            toData->hoistableFields = fromData->hoistableFields->CopyNew(alloc);
        }
    }

    if (TrackArgumentsObject() && fromData->argObjSyms)
    {LOGMEIN("GlobOpt.cpp] 1726\n");
        toData->argObjSyms = fromData->argObjSyms->CopyNew(alloc);
    }
    if (fromData->maybeTempObjectSyms && !fromData->maybeTempObjectSyms->IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 1730\n");
        toData->maybeTempObjectSyms = fromData->maybeTempObjectSyms->CopyNew(alloc);
        if (fromData->canStoreTempObjectSyms && !fromData->canStoreTempObjectSyms->IsEmpty())
        {LOGMEIN("GlobOpt.cpp] 1733\n");
            toData->canStoreTempObjectSyms = fromData->canStoreTempObjectSyms->CopyNew(alloc);
        }
    }
    else
    {
        Assert(fromData->canStoreTempObjectSyms == nullptr || fromData->canStoreTempObjectSyms->IsEmpty());
    }

    toData->curFunc = fromData->curFunc;
    if (fromData->callSequence != nullptr)
    {LOGMEIN("GlobOpt.cpp] 1744\n");
        toData->callSequence = JitAnew(alloc, SListBase<IR::Opnd *>);
        fromData->callSequence->CopyTo(alloc, *(toData->callSequence));
    }
    else
    {
        toData->callSequence = nullptr;
    }

    toData->startCallCount = fromData->startCallCount;
    toData->argOutCount = fromData->argOutCount;
    toData->totalOutParamCount = fromData->totalOutParamCount;
    toData->inlinedArgOutCount = fromData->inlinedArgOutCount;
    toData->hasCSECandidates = fromData->hasCSECandidates;

    // Although we don't need the data on loop pre pass, we need to do it for the loop header
    // because we capture the loop header bailout on loop prepass
    if (fromData->stackLiteralInitFldDataMap != nullptr &&
        (!this->IsLoopPrePass() || (toBlock->isLoopHeader && toBlock->loop == this->rootLoopPrePass)))
    {LOGMEIN("GlobOpt.cpp] 1763\n");
        toData->stackLiteralInitFldDataMap = fromData->stackLiteralInitFldDataMap->Clone();
    }
    else
    {
        toData->stackLiteralInitFldDataMap = nullptr;
    }

    toData->changedSyms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    toData->changedSyms->Copy(fromData->changedSyms);

    Assert(fromData->HasData());
    toData->OnDataInitialized(alloc);
}

void
GlobOpt::CloneValues(BasicBlock *const toBlock, GlobOptBlockData *toData, GlobOptBlockData *fromData)
{LOGMEIN("GlobOpt.cpp] 1780\n");
    ValueSet *const valuesToKillOnCalls = JitAnew(this->alloc, ValueSet, this->alloc);
    toData->valuesToKillOnCalls = valuesToKillOnCalls;

    // Values are shared between symbols with the same ValueNumber.
    // Use a dictionary to share the clone values.
    ValueSetByValueNumber *const valuesCreatedForClone = this->valuesCreatedForClone;
    Assert(valuesCreatedForClone);
    Assert(valuesCreatedForClone->Count() == 0);
    DebugOnly(ValueSetByValueNumber originalValues(tempAlloc, 64));

    const uint tableSize = toData->symToValueMap->tableSize;
    SListBase<GlobHashBucket> *const table = toData->symToValueMap->table;
    for (uint i = 0; i < tableSize; i++)
    {
        FOREACH_SLISTBASE_ENTRY(GlobHashBucket, bucket, &table[i])
        {LOGMEIN("GlobOpt.cpp] 1796\n");
            Value *value = bucket.element;
            ValueNumber valueNum = value->GetValueNumber();
#if DBG
            // Ensure that the set of values in fromData contains only one value per value number. Byte-code constant values
            // are reused in multiple blocks without cloning, so exclude those value numbers.
            {LOGMEIN("GlobOpt.cpp] 1802\n");
                Value *const previouslyClonedOriginalValue = originalValues.Lookup(valueNum);
                if (previouslyClonedOriginalValue)
                {LOGMEIN("GlobOpt.cpp] 1805\n");
                    if (!byteCodeConstantValueNumbersBv->Test(valueNum))
                    {LOGMEIN("GlobOpt.cpp] 1807\n");
                        Assert(value == previouslyClonedOriginalValue);
                    }
                }
                else
                {
                    originalValues.Add(value);
                }
            }
#endif

            Value *newValue = valuesCreatedForClone->Lookup(valueNum);
            if (!newValue)
            {LOGMEIN("GlobOpt.cpp] 1820\n");
                newValue = CopyValue(value, valueNum);
                TrackMergedValueForKills(newValue, toData, nullptr);
                valuesCreatedForClone->Add(newValue);
            }
            bucket.element = newValue;
        } NEXT_SLISTBASE_ENTRY;
    }

    valuesCreatedForClone->Clear();

    ProcessValueKills(toBlock, toData);
}

template <typename CapturedList, typename CapturedItemsAreEqual>
void
GlobOpt::MergeCapturedValues(
    GlobOptBlockData * toData,
    SListBase<CapturedList> * toList,
    SListBase<CapturedList> * fromList,
    CapturedItemsAreEqual itemsAreEqual)
{LOGMEIN("GlobOpt.cpp] 1841\n");
    typename SListBase<CapturedList>::Iterator iterTo(toList);
    typename SListBase<CapturedList>::Iterator iterFrom(fromList);
    bool hasTo = iterTo.Next();
    bool hasFrom = fromList == nullptr ? false : iterFrom.Next();

    // to be conservative, only copy the captured value for common sym Ids
    // in from and to CapturedList, mark all non-common sym Ids for re-capture
    while (hasFrom && hasTo)
    {LOGMEIN("GlobOpt.cpp] 1850\n");
        Sym * symFrom = iterFrom.Data().Key();
        Sym * symTo = iterTo.Data().Key();

        if (symFrom->m_id < symTo->m_id)
        {LOGMEIN("GlobOpt.cpp] 1855\n");
            toData->changedSyms->Set(symFrom->m_id);
            hasFrom = iterFrom.Next();
        }
        else if(symFrom->m_id > symTo->m_id)
        {LOGMEIN("GlobOpt.cpp] 1860\n");
            toData->changedSyms->Set(symTo->m_id);
            hasTo = iterTo.Next();
        }
        else
        {
            if (!itemsAreEqual(&iterFrom.Data(), &iterTo.Data()))
            {LOGMEIN("GlobOpt.cpp] 1867\n");
                toData->changedSyms->Set(symTo->m_id);
            }

            hasFrom = iterFrom.Next();
            hasTo = iterTo.Next();
        }
    }
    bool hasRemain = hasFrom || hasTo;
    if (hasRemain)
    {LOGMEIN("GlobOpt.cpp] 1877\n");
        typename SListBase<CapturedList>::Iterator iterRemain(hasFrom ? iterFrom : iterTo);
        do
        {LOGMEIN("GlobOpt.cpp] 1880\n");
            Sym * symRemain = iterRemain.Data().Key();
            toData->changedSyms->Set(symRemain->m_id);
            hasRemain = iterRemain.Next();
        } while (hasRemain);
    }
}

void
GlobOpt::MergeBlockData(
    GlobOptBlockData *toData,
    BasicBlock *toBlock,
    BasicBlock *fromBlock,
    BVSparse<JitArenaAllocator> *const symsRequiringCompensation,
    BVSparse<JitArenaAllocator> *const symsCreatedForMerge,
    bool forceTypeSpecOnLoopHeader)
{LOGMEIN("GlobOpt.cpp] 1896\n");
    GlobOptBlockData *fromData = &(fromBlock->globOptData);

    if(DoBoundCheckHoist())
    {
        // Do this before merging values so that it can see whether a sym's value was changed on one side or the other
        MergeBoundCheckHoistBlockData(toBlock, toData, fromBlock, fromData);
    }

    bool isLoopBackEdge = toBlock->isLoopHeader;
    this->MergeValueMaps(toData, toBlock, fromBlock, symsRequiringCompensation, symsCreatedForMerge);

    this->InsertCloneStrs(toBlock, toData, fromData);

    toData->liveFields->And(fromData->liveFields);
    toData->liveArrayValues->And(fromData->liveArrayValues);
    toData->isTempSrc->And(fromData->isTempSrc);
    toData->hasCSECandidates &= fromData->hasCSECandidates;

    if (toData->capturedValues == nullptr)
    {LOGMEIN("GlobOpt.cpp] 1916\n");
        toData->capturedValues = fromData->capturedValues;
        toData->changedSyms->Or(fromData->changedSyms);
    }
    else
    {
        MergeCapturedValues(
            toData,
            &toData->capturedValues->constantValues,
            fromData->capturedValues == nullptr ? nullptr : &fromData->capturedValues->constantValues,
            [&](ConstantStackSymValue * symValueFrom, ConstantStackSymValue * symValueTo)
            {
                return symValueFrom->Value().IsEqual(symValueTo->Value());
            });

        MergeCapturedValues(
            toData,
            &toData->capturedValues->copyPropSyms,
            fromData->capturedValues == nullptr ? nullptr : &fromData->capturedValues->copyPropSyms,
            [&](CopyPropSyms * copyPropSymFrom, CopyPropSyms * copyPropSymTo)
            {
                if (copyPropSymFrom->Value()->m_id == copyPropSymTo->Value()->m_id)
                {LOGMEIN("GlobOpt.cpp] 1938\n");
                    Value * val = FindValue(copyPropSymFrom->Key());
                    Value * copyVal = FindValue(copyPropSymTo->Key());
                    return (val != nullptr && copyVal != nullptr &&
                        val->GetValueNumber() == copyVal->GetValueNumber());
                }
                return false;
            });
    }

    if (fromData->maybeWrittenTypeSyms)
    {LOGMEIN("GlobOpt.cpp] 1949\n");
        if (toData->maybeWrittenTypeSyms == nullptr)
        {LOGMEIN("GlobOpt.cpp] 1951\n");
            toData->maybeWrittenTypeSyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
            toData->maybeWrittenTypeSyms->Copy(fromData->maybeWrittenTypeSyms);
        }
        else
        {
            toData->maybeWrittenTypeSyms->Or(fromData->maybeWrittenTypeSyms);
        }
    }

    {LOGMEIN("GlobOpt.cpp] 1961\n");
        // - Keep the var sym live if any of the following is true:
        //     - The var sym is live on both sides
        //     - The var sym is the only live sym that contains the lossless value of the sym on a side (that is, the lossless
        //       int32 sym is not live, and the float64 sym is not live on that side), and the sym of any type is live on the
        //       other side
        //     - On a side, the var and float64 syms are live, the lossless int32 sym is not live, the sym's merged value is
        //       likely int, and the sym of any type is live on the other side. Since the value is likely int, it may be
        //       int-specialized (with lossless conversion) later. Keeping only the float64 sym live requires doing a lossless
        //       conversion from float64 to int32, with bailout if the value of the float is not a true 32-bit integer. Checking
        //       that is costly, and if the float64 sym is converted back to var, it does not become a tagged int, causing a
        //       guaranteed bailout if a lossless conversion to int happens later. Keep the var sym live to preserve its
        //       tagged-ness so that it can be int-specialized while avoiding unnecessary bailouts.
        // - Keep the int32 sym live if it's live on both sides
        //     - Mark the sym as lossy if it's lossy on any side
        // - Keep the float64 sym live if it's live on a side and the sym of a specialized lossless type is live on the other
        //   side
        //
        // fromData.temp =
        //     (fromData.var - (fromData.int32 - fromData.lossyInt32)) &
        //     (toData.var | toData.int32 | toData.float64)
        // toData.temp =
        //     (toData.var - (toData.int32 - toData.lossyInt32)) &
        //     (fromData.var | fromData.int32 | fromData.float64)
        // toData.var =
        //     (fromData.var & toData.var) |
        //     (fromData.temp - fromData.float64) |
        //     (toData.temp - toData.float64) |
        //     (fromData.temp & fromData.float64 | toData.temp & toData.float64) & (value ~ int)
        //
        // toData.float64 =
        //     fromData.float64 & ((toData.int32 - toData.lossyInt32) | toData.float64) |
        //     toData.float64 & ((fromData.int32 - fromData.lossyInt32) | fromData.float64)
        // toData.int32 &= fromData.int32
        // toData.lossyInt32 = (fromData.lossyInt32 | toData.lossyInt32) & toData.int32

        BVSparse<JitArenaAllocator> tempBv1(this->tempAlloc);
        BVSparse<JitArenaAllocator> tempBv2(this->tempAlloc);

        if (isLoopBackEdge && forceTypeSpecOnLoopHeader)
        {LOGMEIN("GlobOpt.cpp] 2001\n");
            Loop *const loop = toBlock->loop;

            // Force to lossless int32:
            // forceLosslessInt32 =
            //     ((fromData.int32 - fromData.lossyInt32) - (toData.int32 - toData.lossyInt32)) &
            //     loop.likelyIntSymsUsedBeforeDefined &
            //     toData.var
            tempBv1.Minus(fromData->liveInt32Syms, fromData->liveLossyInt32Syms);
            tempBv2.Minus(toData->liveInt32Syms, toData->liveLossyInt32Syms);
            tempBv1.Minus(&tempBv2);
            tempBv1.And(loop->likelyIntSymsUsedBeforeDefined);
            tempBv1.And(toData->liveVarSyms);
            toData->liveInt32Syms->Or(&tempBv1);
            toData->liveLossyInt32Syms->Minus(&tempBv1);

            if(DoLossyIntTypeSpec())
            {LOGMEIN("GlobOpt.cpp] 2018\n");
                // Force to lossy int32:
                // forceLossyInt32 = (fromData.int32 - toData.int32) & loop.symsUsedBeforeDefined & toData.var
                tempBv1.Minus(fromData->liveInt32Syms, toData->liveInt32Syms);
                tempBv1.And(loop->symsUsedBeforeDefined);
                tempBv1.And(toData->liveVarSyms);
                toData->liveInt32Syms->Or(&tempBv1);
                toData->liveLossyInt32Syms->Or(&tempBv1);
            }

            // Force to float64:
            // forceFloat64 =
            //     fromData.float64 & loop.forceFloat64 |
            //     (fromData.float64 - toData.float64) & loop.likelyNumberSymsUsedBeforeDefined
            tempBv1.And(fromData->liveFloat64Syms, loop->forceFloat64SymsOnEntry);
            toData->liveFloat64Syms->Or(&tempBv1);
            tempBv1.Minus(fromData->liveFloat64Syms, toData->liveFloat64Syms);
            tempBv1.And(loop->likelyNumberSymsUsedBeforeDefined);
            toData->liveFloat64Syms->Or(&tempBv1);

            // Force to Simd128 type:
            // if live on the backedge and we are hoisting the operand.
            // or if live on the backedge only and used before def in the loop.
            tempBv1.And(fromData->liveSimd128F4Syms, loop->forceSimd128F4SymsOnEntry);
            toData->liveSimd128F4Syms->Or(&tempBv1);
            tempBv1.Minus(fromData->liveSimd128F4Syms, toData->liveSimd128F4Syms);
            tempBv1.And(loop->likelySimd128F4SymsUsedBeforeDefined);
            toData->liveSimd128F4Syms->Or(&tempBv1);

            tempBv1.And(fromData->liveSimd128I4Syms, loop->forceSimd128I4SymsOnEntry);
            toData->liveSimd128I4Syms->Or(&tempBv1);
            tempBv1.Minus(fromData->liveSimd128I4Syms, toData->liveSimd128I4Syms);
            tempBv1.And(loop->likelySimd128I4SymsUsedBeforeDefined);
            toData->liveSimd128I4Syms->Or(&tempBv1);
        }

        BVSparse<JitArenaAllocator> simdSymsToVar(this->tempAlloc);
        {LOGMEIN("GlobOpt.cpp] 2055\n");
            // SIMD_JS
            // If we have simd128 type-spec sym live as one type on one side, but not of same type on the other, we look at the merged ValueType.
            // If it's Likely the simd128 type, we choose to keep the type-spec sym (compensate with a FromVar), if the following is true:
            //     - We are not in jitLoopBody. Introducing a FromVar for compensation extends bytecode syms lifetime. If the value
            //       is actually dead, and we enter the loop-body after bailing out from SimpleJit, the value will not be restored in
            //       the bailout code.
            //     - Value was never Undefined/Null. Avoid unboxing of possibly uninitialized values.
            //     - Not loop back-edge. To keep unboxed value, the value has to be used-before def in the loop-body. This is done
            //       separately in forceSimd128*SymsOnEntry and included in loop-header.

            // Live syms as F4 on one edge only
            tempBv1.Xor(fromData->liveSimd128F4Syms, toData->liveSimd128F4Syms);
            FOREACH_BITSET_IN_SPARSEBV(id, &tempBv1)
            {LOGMEIN("GlobOpt.cpp] 2069\n");
                StackSym *const stackSym = this->func->m_symTable->FindStackSym(id);
                Assert(stackSym);
                Value *const value = this->FindValue(toData->symToValueMap, stackSym);
                ValueInfo * valueInfo = value ? value->GetValueInfo() : nullptr;

                // There are two possible representations for Simd128F4 Value: F4 or Var.
                // If the merged ValueType is LikelySimd128F4, then on the edge where F4 is dead,  Var must be alive.
                // Unbox to F4 type-spec sym.
                if (
                    valueInfo && valueInfo->IsLikelySimd128Float32x4() &&
                    !valueInfo->HasBeenUndefined() && !valueInfo->HasBeenNull() &&
                    !isLoopBackEdge && !func->IsLoopBody()
                   )
                {LOGMEIN("GlobOpt.cpp] 2083\n");
                    toData->liveSimd128F4Syms->Set(id);
                }
                else
                {
                    // If live on both edges, box it.
                    if (IsLive(stackSym, fromData) && IsLive(stackSym, toData))
                    {LOGMEIN("GlobOpt.cpp] 2090\n");
                        simdSymsToVar.Set(id);
                    }
                    // kill F4 sym
                    toData->liveSimd128F4Syms->Clear(id);
                }
            } NEXT_BITSET_IN_SPARSEBV;

            // Same for I4
            tempBv1.Xor(fromData->liveSimd128I4Syms, toData->liveSimd128I4Syms);
            FOREACH_BITSET_IN_SPARSEBV(id, &tempBv1)
            {LOGMEIN("GlobOpt.cpp] 2101\n");
                StackSym *const stackSym = this->func->m_symTable->FindStackSym(id);
                Assert(stackSym);
                Value *const value = this->FindValue(toData->symToValueMap, stackSym);
                ValueInfo * valueInfo = value ? value->GetValueInfo() : nullptr;
                if (
                    valueInfo && valueInfo->IsLikelySimd128Int32x4() &&
                    !valueInfo->HasBeenUndefined() && !valueInfo->HasBeenNull() &&
                    !isLoopBackEdge && !func->IsLoopBody()
                   )
                {LOGMEIN("GlobOpt.cpp] 2111\n");
                    toData->liveSimd128I4Syms->Set(id);
                }
                else
                {
                    if (IsLive(stackSym, fromData) && IsLive(stackSym, toData))
                    {LOGMEIN("GlobOpt.cpp] 2117\n");
                        simdSymsToVar.Set(id);
                    }
                    toData->liveSimd128I4Syms->Clear(id);
                }
            } NEXT_BITSET_IN_SPARSEBV;
        }

        {
            BVSparse<JitArenaAllocator> tempBv3(this->tempAlloc);

            // fromData.temp =
            //     (fromData.var - (fromData.int32 - fromData.lossyInt32)) &
            //     (toData.var | toData.int32 | toData.float64)
            tempBv2.Minus(fromData->liveInt32Syms, fromData->liveLossyInt32Syms);
            tempBv1.Minus(fromData->liveVarSyms, &tempBv2);
            tempBv2.Or(toData->liveVarSyms, toData->liveInt32Syms);
            tempBv2.Or(toData->liveFloat64Syms);
            tempBv1.And(&tempBv2);

            // toData.temp =
            //     (toData.var - (toData.int32 - toData.lossyInt32)) &
            //     (fromData.var | fromData.int32 | fromData.float64)
            tempBv3.Minus(toData->liveInt32Syms, toData->liveLossyInt32Syms);
            tempBv2.Minus(toData->liveVarSyms, &tempBv3);
            tempBv3.Or(fromData->liveVarSyms, fromData->liveInt32Syms);
            tempBv3.Or(fromData->liveFloat64Syms);
            tempBv2.And(&tempBv3);

            {LOGMEIN("GlobOpt.cpp] 2146\n");
                BVSparse<JitArenaAllocator> tempBv4(this->tempAlloc);

                // fromData.temp & fromData.float64 | toData.temp & toData.float64
                tempBv3.And(&tempBv1, fromData->liveFloat64Syms);
                tempBv4.And(&tempBv2, toData->liveFloat64Syms);
                tempBv3.Or(&tempBv4);
            }

            //     (fromData.temp - fromData.float64) |
            //     (toData.temp - toData.float64)
            tempBv1.Minus(fromData->liveFloat64Syms);
            tempBv2.Minus(toData->liveFloat64Syms);
            tempBv1.Or(&tempBv2);

            // toData.var =
            //     (fromData.var & toData.var) |
            //     (fromData.temp - fromData.float64) |
            //     (toData.temp - toData.float64)
            toData->liveVarSyms->And(fromData->liveVarSyms);
            toData->liveVarSyms->Or(&tempBv1);

            // toData.var |=
            //     (fromData.temp & fromData.float64 | toData.temp & toData.float64) & (value ~ int)
            FOREACH_BITSET_IN_SPARSEBV(id, &tempBv3)
            {LOGMEIN("GlobOpt.cpp] 2171\n");
                StackSym *const stackSym = this->func->m_symTable->FindStackSym(id);
                Assert(stackSym);
                Value *const value = this->FindValue(toData->symToValueMap, stackSym);
                if(value)
                {LOGMEIN("GlobOpt.cpp] 2176\n");
                    ValueInfo *const valueInfo = value->GetValueInfo();
                    if(valueInfo->IsInt() || (valueInfo->IsLikelyInt() && DoAggressiveIntTypeSpec()))
                    {LOGMEIN("GlobOpt.cpp] 2179\n");
                        toData->liveVarSyms->Set(id);
                    }
                }
            } NEXT_BITSET_IN_SPARSEBV;

            // SIMD_JS
            // Simd syms that need boxing
            toData->liveVarSyms->Or(&simdSymsToVar);
        }

        //     fromData.float64 & ((toData.int32 - toData.lossyInt32) | toData.float64)
        tempBv1.Minus(toData->liveInt32Syms, toData->liveLossyInt32Syms);
        tempBv1.Or(toData->liveFloat64Syms);
        tempBv1.And(fromData->liveFloat64Syms);

        //     toData.float64 & ((fromData.int32 - fromData.lossyInt32) | fromData.float64)
        tempBv2.Minus(fromData->liveInt32Syms, fromData->liveLossyInt32Syms);
        tempBv2.Or(fromData->liveFloat64Syms);
        tempBv2.And(toData->liveFloat64Syms);

        // toData.float64 =
        //     fromData.float64 & ((toData.int32 - toData.lossyInt32) | toData.float64) |
        //     toData.float64 & ((fromData.int32 - fromData.lossyInt32) | fromData.float64)
        toData->liveFloat64Syms->Or(&tempBv1, &tempBv2);

        // toData.int32 &= fromData.int32
        // toData.lossyInt32 = (fromData.lossyInt32 | toData.lossyInt32) & toData.int32
        toData->liveInt32Syms->And(fromData->liveInt32Syms);
        toData->liveLossyInt32Syms->Or(fromData->liveLossyInt32Syms);
        toData->liveLossyInt32Syms->And(toData->liveInt32Syms);
    }

    if (TrackHoistableFields() && HasHoistableFields(fromData))
    {LOGMEIN("GlobOpt.cpp] 2213\n");
        if (toData->hoistableFields)
        {LOGMEIN("GlobOpt.cpp] 2215\n");
            toData->hoistableFields->Or(fromData->hoistableFields);
        }
        else
        {
            toData->hoistableFields = fromData->hoistableFields->CopyNew(this->alloc);
        }
    }

    if (TrackArgumentsObject())
    {LOGMEIN("GlobOpt.cpp] 2225\n");
        if (!toData->argObjSyms->Equal(fromData->argObjSyms))
        {LOGMEIN("GlobOpt.cpp] 2227\n");
            CannotAllocateArgumentsObjectOnStack();
        }
    }

    if (fromData->maybeTempObjectSyms && !fromData->maybeTempObjectSyms->IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 2233\n");
        if (toData->maybeTempObjectSyms)
        {LOGMEIN("GlobOpt.cpp] 2235\n");
            toData->maybeTempObjectSyms->Or(fromData->maybeTempObjectSyms);
        }
        else
        {
            toData->maybeTempObjectSyms = fromData->maybeTempObjectSyms->CopyNew(this->alloc);
        }

        if (fromData->canStoreTempObjectSyms && !fromData->canStoreTempObjectSyms->IsEmpty())
        {LOGMEIN("GlobOpt.cpp] 2244\n");
            if (toData->canStoreTempObjectSyms)
            {LOGMEIN("GlobOpt.cpp] 2246\n");
                // Both need to be temp object
                toData->canStoreTempObjectSyms->And(fromData->canStoreTempObjectSyms);
            }
        }
        else if (toData->canStoreTempObjectSyms)
        {LOGMEIN("GlobOpt.cpp] 2252\n");
            toData->canStoreTempObjectSyms->ClearAll();
        }
    }
    else
    {
        Assert(!fromData->canStoreTempObjectSyms || fromData->canStoreTempObjectSyms->IsEmpty());
        if (toData->canStoreTempObjectSyms)
        {LOGMEIN("GlobOpt.cpp] 2260\n");
            toData->canStoreTempObjectSyms->ClearAll();
        }
    }

    Assert(toData->curFunc == fromData->curFunc);
    Assert((toData->callSequence == nullptr && fromData->callSequence == nullptr) || toData->callSequence->Equals(*(fromData->callSequence)));
    Assert(toData->startCallCount == fromData->startCallCount);
    Assert(toData->argOutCount == fromData->argOutCount);
    Assert(toData->totalOutParamCount == fromData->totalOutParamCount);
    Assert(toData->inlinedArgOutCount == fromData->inlinedArgOutCount);

    // stackLiteralInitFldDataMap is a union of the stack literal from two path.
    // Although we don't need the data on loop prepass, we need to do it for the loop header
    // because we capture the loop header bailout on loop prepass.
    if (fromData->stackLiteralInitFldDataMap != nullptr &&
        (!this->IsLoopPrePass() || (toBlock->isLoopHeader && toBlock->loop == this->rootLoopPrePass)))
    {LOGMEIN("GlobOpt.cpp] 2277\n");
        if (toData->stackLiteralInitFldDataMap == nullptr)
        {LOGMEIN("GlobOpt.cpp] 2279\n");
            toData->stackLiteralInitFldDataMap = fromData->stackLiteralInitFldDataMap->Clone();
        }
        else
        {
            StackLiteralInitFldDataMap * toMap = toData->stackLiteralInitFldDataMap;
            fromData->stackLiteralInitFldDataMap->Map([toMap](StackSym * stackSym, StackLiteralInitFldData const& data)
            {
                if (toMap->AddNew(stackSym, data) == -1)
                {LOGMEIN("GlobOpt.cpp] 2288\n");
                    // If there is an existing data for the stackSym, both path should match
                    DebugOnly(StackLiteralInitFldData const * currentData);
                    Assert(toMap->TryGetReference(stackSym, &currentData));
                    Assert(currentData->currentInitFldCount == data.currentInitFldCount);
                    Assert(currentData->propIds == data.propIds);
                }
            });
        }
    }
}

void
GlobOpt::DeleteBlockData(GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 2302\n");
    JitArenaAllocator *const alloc = this->alloc;

    data->symToValueMap->Delete();
    data->exprToValueMap->Delete();

    JitAdelete(alloc, data->liveFields);
    JitAdelete(alloc, data->liveArrayValues);
    if (data->maybeWrittenTypeSyms)
    {
        JitAdelete(alloc, data->maybeWrittenTypeSyms);
    }
    JitAdelete(alloc, data->isTempSrc);
    JitAdelete(alloc, data->liveVarSyms);
    JitAdelete(alloc, data->liveInt32Syms);
    JitAdelete(alloc, data->liveLossyInt32Syms);
    JitAdelete(alloc, data->liveFloat64Syms);

    // SIMD_JS
    JitAdelete(alloc, data->liveSimd128F4Syms);
    JitAdelete(alloc, data->liveSimd128I4Syms);

    if (data->hoistableFields)
    {
        JitAdelete(alloc, data->hoistableFields);
    }
    if (data->argObjSyms)
    {
        JitAdelete(alloc, data->argObjSyms);
    }
    if (data->maybeTempObjectSyms)
    {
        JitAdelete(alloc, data->maybeTempObjectSyms);
        if (data->canStoreTempObjectSyms)
        {
            JitAdelete(alloc, data->canStoreTempObjectSyms);
        }
    }
    else
    {
        Assert(!data->canStoreTempObjectSyms);
    }

    JitAdelete(alloc, data->valuesToKillOnCalls);

    if(data->inductionVariables)
    {
        JitAdelete(alloc, data->inductionVariables);
    }
    if(data->availableIntBoundChecks)
    {
        JitAdelete(alloc, data->availableIntBoundChecks);
    }

    if (data->stackLiteralInitFldDataMap)
    {
        JitAdelete(alloc, data->stackLiteralInitFldDataMap);
    }

    JitAdelete(alloc, data->changedSyms);
    data->changedSyms = nullptr;

    data->OnDataDeleted();
}

void
GlobOpt::ToVar(BVSparse<JitArenaAllocator> *bv, BasicBlock *block)
{
    FOREACH_BITSET_IN_SPARSEBV(id, bv)
    {LOGMEIN("GlobOpt.cpp] 2371\n");
        StackSym *stackSym = this->func->m_symTable->FindStackSym(id);
        IR::RegOpnd *newOpnd = IR::RegOpnd::New(stackSym, TyVar, this->func);
        IR::Instr *lastInstr = block->GetLastInstr();
        if (lastInstr->IsBranchInstr() || lastInstr->m_opcode == Js::OpCode::BailTarget)
        {LOGMEIN("GlobOpt.cpp] 2376\n");
            // If branch is using this symbol, hoist the operand as the ToVar load will get
            // inserted right before the branch.
            IR::Opnd *src1 = lastInstr->GetSrc1();
            if (src1)
            {LOGMEIN("GlobOpt.cpp] 2381\n");
                if (src1->IsRegOpnd() && src1->AsRegOpnd()->m_sym == stackSym)
                {LOGMEIN("GlobOpt.cpp] 2383\n");
                    lastInstr->HoistSrc1(Js::OpCode::Ld_A);
                }
                IR::Opnd *src2 = lastInstr->GetSrc2();
                if (src2)
                {LOGMEIN("GlobOpt.cpp] 2388\n");
                    if (src2->IsRegOpnd() && src2->AsRegOpnd()->m_sym == stackSym)
                    {LOGMEIN("GlobOpt.cpp] 2390\n");
                        lastInstr->HoistSrc2(Js::OpCode::Ld_A);
                    }
                }
            }
            this->ToVar(lastInstr, newOpnd, block, nullptr, false);
        }
        else
        {
            IR::Instr *lastNextInstr = lastInstr->m_next;
            this->ToVar(lastNextInstr, newOpnd, block, nullptr, false);
        }
    } NEXT_BITSET_IN_SPARSEBV;
}

void
GlobOpt::ToInt32(BVSparse<JitArenaAllocator> *bv, BasicBlock *block, bool lossy, IR::Instr *insertBeforeInstr)
{LOGMEIN("GlobOpt.cpp] 2407\n");
    return this->ToTypeSpec(bv, block, TyInt32, IR::BailOutIntOnly, lossy, insertBeforeInstr);
}

void
GlobOpt::ToFloat64(BVSparse<JitArenaAllocator> *bv, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 2413\n");
    return this->ToTypeSpec(bv, block, TyFloat64, IR::BailOutNumberOnly);
}

void
GlobOpt::ToTypeSpec(BVSparse<JitArenaAllocator> *bv, BasicBlock *block, IRType toType, IR::BailOutKind bailOutKind, bool lossy, IR::Instr *insertBeforeInstr)
{
    FOREACH_BITSET_IN_SPARSEBV(id, bv)
    {LOGMEIN("GlobOpt.cpp] 2421\n");
        StackSym *stackSym = this->func->m_symTable->FindStackSym(id);
        IRType fromType;

        // Win8 bug: 757126. If we are trying to type specialize the arguments object,
        // let's make sure stack args optimization is not enabled. This is a problem, particularly,
        // if the instruction comes from an unreachable block. In other cases, the pass on the
        // instruction itself should disable arguments object optimization.
        if(block->globOptData.argObjSyms && IsArgumentsSymID(id, block->globOptData))
        {LOGMEIN("GlobOpt.cpp] 2430\n");
            CannotAllocateArgumentsObjectOnStack();
        }

        if (block->globOptData.liveVarSyms->Test(id))
        {LOGMEIN("GlobOpt.cpp] 2435\n");
            fromType = TyVar;
        }
        else if (block->globOptData.liveInt32Syms->Test(id) && !block->globOptData.liveLossyInt32Syms->Test(id))
        {LOGMEIN("GlobOpt.cpp] 2439\n");
            fromType = TyInt32;
            stackSym = stackSym->GetInt32EquivSym(this->func);
        }
        else if (block->globOptData.liveFloat64Syms->Test(id))
        {LOGMEIN("GlobOpt.cpp] 2444\n");

            fromType = TyFloat64;
            stackSym = stackSym->GetFloat64EquivSym(this->func);
        }
        else
        {
            Assert(IsLiveAsSimd128(stackSym, &block->globOptData));
            if (IsLiveAsSimd128F4(stackSym, &block->globOptData))
            {LOGMEIN("GlobOpt.cpp] 2453\n");
                fromType = TySimd128F4;
                stackSym = stackSym->GetSimd128F4EquivSym(this->func);
            }
            else
            {
                fromType = TySimd128I4;
                stackSym = stackSym->GetSimd128I4EquivSym(this->func);
            }
        }


        IR::RegOpnd *newOpnd = IR::RegOpnd::New(stackSym, fromType, this->func);
        IR::Instr *lastInstr = block->GetLastInstr();

        if (!insertBeforeInstr && lastInstr->IsBranchInstr())
        {LOGMEIN("GlobOpt.cpp] 2469\n");
            // If branch is using this symbol, hoist the operand as the ToInt32 load will get
            // inserted right before the branch.
            IR::Instr *instrPrev = lastInstr->m_prev;
            IR::Opnd *src1 = lastInstr->GetSrc1();
            if (src1)
            {LOGMEIN("GlobOpt.cpp] 2475\n");
                if (src1->IsRegOpnd() && src1->AsRegOpnd()->m_sym == stackSym)
                {LOGMEIN("GlobOpt.cpp] 2477\n");
                    lastInstr->HoistSrc1(Js::OpCode::Ld_A);
                }
                IR::Opnd *src2 = lastInstr->GetSrc2();
                if (src2)
                {LOGMEIN("GlobOpt.cpp] 2482\n");
                    if (src2->IsRegOpnd() && src2->AsRegOpnd()->m_sym == stackSym)
                    {LOGMEIN("GlobOpt.cpp] 2484\n");
                        lastInstr->HoistSrc2(Js::OpCode::Ld_A);
                    }
                }

                // Did we insert anything?
                if (lastInstr->m_prev != instrPrev)
                {LOGMEIN("GlobOpt.cpp] 2491\n");
                    // If we had ByteCodeUses right before the branch, move them back down.
                    IR::Instr *insertPoint = lastInstr;
                    for (IR::Instr *instrBytecode = instrPrev; instrBytecode->m_opcode == Js::OpCode::ByteCodeUses; instrBytecode = instrBytecode->m_prev)
                    {LOGMEIN("GlobOpt.cpp] 2495\n");
                        instrBytecode->Unlink();
                        insertPoint->InsertBefore(instrBytecode);
                        insertPoint = instrBytecode;
                    }
                }
            }
        }
        this->ToTypeSpecUse(nullptr, newOpnd, block, nullptr, nullptr, toType, bailOutKind, lossy, insertBeforeInstr);
    } NEXT_BITSET_IN_SPARSEBV;
}

void
GlobOpt::CleanUpValueMaps()
{LOGMEIN("GlobOpt.cpp] 2509\n");
    // Don't do cleanup if it's been done recently.
    // Landing pad could get optimized twice...
    // We want the same info out the first and second time. So always cleanup.
    // Increasing the cleanup threshold count for asmjs to 500
    uint cleanupCount = (!GetIsAsmJSFunc()) ? CONFIG_FLAG(GoptCleanupThreshold) : CONFIG_FLAG(AsmGoptCleanupThreshold);
    if (!this->currentBlock->IsLandingPad() && this->instrCountSinceLastCleanUp < cleanupCount)
    {LOGMEIN("GlobOpt.cpp] 2516\n");
        return;
    }
    this->instrCountSinceLastCleanUp = 0;

    GlobHashTable *thisTable = this->blockData.symToValueMap;
    BVSparse<JitArenaAllocator> deadSymsBv(this->tempAlloc);
    BVSparse<JitArenaAllocator> keepAliveSymsBv(this->tempAlloc);
    BVSparse<JitArenaAllocator> availableValueNumbers(this->tempAlloc);
    availableValueNumbers.Copy(byteCodeConstantValueNumbersBv);
    BVSparse<JitArenaAllocator> *upwardExposedUses = this->currentBlock->upwardExposedUses;
    BVSparse<JitArenaAllocator> *upwardExposedFields = this->currentBlock->upwardExposedFields;
    bool isInLoop = !!this->currentBlock->loop;

    BVSparse<JitArenaAllocator> symsInCallSequence(this->tempAlloc);
    SListBase<IR::Opnd *> * callSequence = this->currentBlock->globOptData.callSequence;
    if (callSequence && !callSequence->Empty())
    {LOGMEIN("GlobOpt.cpp] 2533\n");
        FOREACH_SLISTBASE_ENTRY(IR::Opnd *, opnd, callSequence)
        {LOGMEIN("GlobOpt.cpp] 2535\n");
            StackSym * sym = opnd->GetStackSym();
            symsInCallSequence.Set(sym->m_id);
        }
    }
    NEXT_SLISTBASE_ENTRY;

    for (uint i = 0; i < thisTable->tableSize; i++)
    {
        FOREACH_SLISTBASE_ENTRY_EDITING(GlobHashBucket, bucket, &thisTable->table[i], iter)
        {LOGMEIN("GlobOpt.cpp] 2545\n");
            bool isSymUpwardExposed = upwardExposedUses->Test(bucket.value->m_id) || upwardExposedFields->Test(bucket.value->m_id);
            if (!isSymUpwardExposed && symsInCallSequence.Test(bucket.value->m_id))
            {LOGMEIN("GlobOpt.cpp] 2548\n");
                // Don't remove/shrink sym-value pair if the sym is referenced in callSequence even if the sym is dead according to backward data flow.
                // This is possible in some edge cases that an infinite loop is involved when evaluating parameter for a function (between StartCall and Call),
                // there is no backward data flow into the infinite loop block, but non empty callSequence still populates to it in this (forward) pass
                // which causes error when looking up value for the syms in callSequence (cannot find the value).
                // It would cause error to fill out the bailout information for the loop blocks.
                // Remove dead syms from callSequence has some risk because there are various associated counters which need to be consistent.
                continue;
            }
            // Make sure symbol was created before backward pass.
            // If symbols isn't upward exposed, mark it as dead.
            // If a symbol was copy-prop'd in a loop prepass, the upwardExposedUses info could be wrong.  So wait until we are out of the loop before clearing it.
            if ((SymID)bucket.value->m_id <= this->maxInitialSymID && !isSymUpwardExposed
                && (!isInLoop || !this->prePassCopyPropSym->Test(bucket.value->m_id)))
            {LOGMEIN("GlobOpt.cpp] 2562\n");
                Value *val = bucket.element;
                ValueInfo *valueInfo = val->GetValueInfo();

                Sym * sym = bucket.value;
                Sym *symStore = valueInfo->GetSymStore();

                if (symStore && symStore == bucket.value)
                {LOGMEIN("GlobOpt.cpp] 2570\n");
                    // Keep constants around, as we don't know if there will be further uses
                    if (!bucket.element->GetValueInfo()->IsVarConstant() && !bucket.element->GetValueInfo()->HasIntConstantValue())
                    {LOGMEIN("GlobOpt.cpp] 2573\n");
                        // Symbol may still be a copy-prop candidate.  Wait before deleting it.
                        deadSymsBv.Set(bucket.value->m_id);

                        // Make sure the type sym is added to the dead syms vector as well, because type syms are
                        // created in backward pass and so their symIds > maxInitialSymID.
                        if (sym->IsStackSym() && sym->AsStackSym()->HasObjectTypeSym())
                        {LOGMEIN("GlobOpt.cpp] 2580\n");
                            deadSymsBv.Set(sym->AsStackSym()->GetObjectTypeSym()->m_id);
                        }
                    }
                    availableValueNumbers.Set(val->GetValueNumber());
                }
                else
                {
                    // Make sure the type sym is added to the dead syms vector as well, because type syms are
                    // created in backward pass and so their symIds > maxInitialSymID. Perhaps we could remove
                    // it explicitly here, but would it work alright with the iterator?
                    if (sym->IsStackSym() && sym->AsStackSym()->HasObjectTypeSym())
                    {LOGMEIN("GlobOpt.cpp] 2592\n");
                        deadSymsBv.Set(sym->AsStackSym()->GetObjectTypeSym()->m_id);
                    }

                    // Not a copy-prop candidate; delete it right away.
                    iter.RemoveCurrent(thisTable->alloc);
                    this->blockData.liveInt32Syms->Clear(sym->m_id);
                    this->blockData.liveLossyInt32Syms->Clear(sym->m_id);
                    this->blockData.liveFloat64Syms->Clear(sym->m_id);
                }
            }
            else
            {
                Sym * sym = bucket.value;

                if (sym->IsPropertySym() && !this->blockData.liveFields->Test(sym->m_id))
                {LOGMEIN("GlobOpt.cpp] 2608\n");
                    // Remove propertySyms which are not live anymore.
                    iter.RemoveCurrent(thisTable->alloc);
                    this->blockData.liveInt32Syms->Clear(sym->m_id);
                    this->blockData.liveLossyInt32Syms->Clear(sym->m_id);
                    this->blockData.liveFloat64Syms->Clear(sym->m_id);
                }
                else
                {
                    // Look at the copy-prop candidate. We don't want to get rid of the data for a symbol which is
                    // a copy-prop candidate.
                    Value *val = bucket.element;
                    ValueInfo *valueInfo = val->GetValueInfo();

                    Sym *symStore = valueInfo->GetSymStore();

                    if (symStore && symStore != bucket.value)
                    {LOGMEIN("GlobOpt.cpp] 2625\n");
                        keepAliveSymsBv.Set(symStore->m_id);
                        if (symStore->IsStackSym() && symStore->AsStackSym()->HasObjectTypeSym())
                        {LOGMEIN("GlobOpt.cpp] 2628\n");
                            keepAliveSymsBv.Set(symStore->AsStackSym()->GetObjectTypeSym()->m_id);
                        }
                    }
                    availableValueNumbers.Set(val->GetValueNumber());
                }
            }
        } NEXT_SLISTBASE_ENTRY_EDITING;
    }

    deadSymsBv.Minus(&keepAliveSymsBv);

    // Now cleanup exprToValueMap table
    ExprHashTable *thisExprTable = this->blockData.exprToValueMap;
    bool oldHasCSECandidatesValue = this->currentBlock->globOptData.hasCSECandidates;  // Could be false if none need bailout.
    this->currentBlock->globOptData.hasCSECandidates = false;

    for (uint i = 0; i < thisExprTable->tableSize; i++)
    {
        FOREACH_SLISTBASE_ENTRY_EDITING(ExprHashBucket, bucket, &thisExprTable->table[i], iter)
        {LOGMEIN("GlobOpt.cpp] 2648\n");
            ExprHash hash = bucket.value;
            ValueNumber src1ValNum = hash.GetSrc1ValueNumber();
            ValueNumber src2ValNum = hash.GetSrc2ValueNumber();

            // If src1Val or src2Val are not available anymore, no point keeping this CSE candidate
            bool removeCurrent = false;
            if ((src1ValNum && !availableValueNumbers.Test(src1ValNum))
                || (src2ValNum && !availableValueNumbers.Test(src2ValNum)))
            {LOGMEIN("GlobOpt.cpp] 2657\n");
                removeCurrent = true;
            }
            else
            {
                // If we are keeping this value, make sure we also keep the symStore in the value table
                removeCurrent = true; // Remove by default, unless it's set to false later below.
                Value *val = bucket.element;
                if (val)
                {LOGMEIN("GlobOpt.cpp] 2666\n");
                    Sym *symStore = val->GetValueInfo()->GetSymStore();
                    if (symStore)
                    {LOGMEIN("GlobOpt.cpp] 2669\n");
                        Value *symStoreVal = this->FindValue(this->currentBlock->globOptData.symToValueMap, symStore);

                        if (symStoreVal && symStoreVal->GetValueNumber() == val->GetValueNumber())
                        {LOGMEIN("GlobOpt.cpp] 2673\n");
                            removeCurrent = false;
                            deadSymsBv.Clear(symStore->m_id);
                            if (symStore->IsStackSym() && symStore->AsStackSym()->HasObjectTypeSym())
                            {LOGMEIN("GlobOpt.cpp] 2677\n");
                                deadSymsBv.Clear(symStore->AsStackSym()->GetObjectTypeSym()->m_id);
                            }
                        }
                    }
                }
            }

            if(removeCurrent)
            {LOGMEIN("GlobOpt.cpp] 2686\n");
                iter.RemoveCurrent(thisExprTable->alloc);
            }
            else
            {
                this->currentBlock->globOptData.hasCSECandidates = oldHasCSECandidatesValue;
            }
        } NEXT_SLISTBASE_ENTRY_EDITING;
    }

    FOREACH_BITSET_IN_SPARSEBV(dead_id, &deadSymsBv)
    {LOGMEIN("GlobOpt.cpp] 2697\n");
        thisTable->Clear(dead_id);
    }
    NEXT_BITSET_IN_SPARSEBV;

    if (!deadSymsBv.IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 2703\n");
        if (this->func->IsJitInDebugMode())
        {LOGMEIN("GlobOpt.cpp] 2705\n");
            // Do not remove non-temp local vars from liveVarSyms (i.e. do not let them become dead).
            // We will need to restore all initialized/used so far non-temp local during bail out.
            // (See BackwardPass::ProcessBailOutInfo)
            Assert(this->func->m_nonTempLocalVars);
            BVSparse<JitArenaAllocator> tempBv(this->tempAlloc);
            tempBv.Minus(&deadSymsBv, this->func->m_nonTempLocalVars);
            this->blockData.liveVarSyms->Minus(&tempBv);
#if DBG
            tempBv.And(this->blockData.liveInt32Syms, this->func->m_nonTempLocalVars);
            AssertMsg(tempBv.IsEmpty(), "Type spec is disabled under debugger. How come did we get a non-temp local in liveInt32Syms?");
            tempBv.And(this->blockData.liveLossyInt32Syms, this->func->m_nonTempLocalVars);
            AssertMsg(tempBv.IsEmpty(), "Type spec is disabled under debugger. How come did we get a non-temp local in liveLossyInt32Syms?");
            tempBv.And(this->blockData.liveFloat64Syms, this->func->m_nonTempLocalVars);
            AssertMsg(tempBv.IsEmpty(), "Type spec is disabled under debugger. How come did we get a non-temp local in liveFloat64Syms?");
#endif
        }
        else
        {
            this->blockData.liveVarSyms->Minus(&deadSymsBv);
        }

        this->blockData.liveInt32Syms->Minus(&deadSymsBv);
        this->blockData.liveLossyInt32Syms->Minus(&deadSymsBv);
        this->blockData.liveFloat64Syms->Minus(&deadSymsBv);
    }

    JitAdelete(this->alloc, upwardExposedUses);
    this->currentBlock->upwardExposedUses = nullptr;
    JitAdelete(this->alloc, upwardExposedFields);
    this->currentBlock->upwardExposedFields = nullptr;
    if (this->currentBlock->cloneStrCandidates)
    {LOGMEIN("GlobOpt.cpp] 2737\n");
        JitAdelete(this->alloc, this->currentBlock->cloneStrCandidates);
        this->currentBlock->cloneStrCandidates = nullptr;
    }
}

PRECandidatesList * GlobOpt::FindBackEdgePRECandidates(BasicBlock *block, JitArenaAllocator *alloc)
{LOGMEIN("GlobOpt.cpp] 2744\n");
    // Iterate over the value table looking for propertySyms which are candidates to
    // pre-load in the landing pad for field PRE

    GlobHashTable *valueTable = block->globOptData.symToValueMap;
    Loop *loop = block->loop;
    PRECandidatesList *candidates = nullptr;

    for (uint i = 0; i < valueTable->tableSize; i++)
    {
        FOREACH_SLISTBASE_ENTRY(GlobHashBucket, bucket, &valueTable->table[i])
        {LOGMEIN("GlobOpt.cpp] 2755\n");
            Sym *sym = bucket.value;

            if (!sym->IsPropertySym())
            {LOGMEIN("GlobOpt.cpp] 2759\n");
                continue;
            }

            PropertySym *propertySym = sym->AsPropertySym();

            // Field should be live on the back-edge
            if (!block->globOptData.liveFields->Test(propertySym->m_id))
            {LOGMEIN("GlobOpt.cpp] 2767\n");
                continue;
            }

            // Field should be live in the landing pad as well
            if (!loop->landingPad->globOptData.liveFields->Test(propertySym->m_id))
            {LOGMEIN("GlobOpt.cpp] 2773\n");
                continue;
            }

            Value *value = bucket.element;
            Sym *symStore = value->GetValueInfo()->GetSymStore();

            if (!symStore || !symStore->IsStackSym())
            {LOGMEIN("GlobOpt.cpp] 2781\n");
                continue;
            }

            // Check upwardExposed in case of:
            //  s1 = 0;
            // loop:
            //        = o.x;
            //        foo();
            //    o.x = s1;
            // Can't thrash s1 in loop top.
            if (!symStore->AsStackSym()->IsSingleDef() || loop->GetHeadBlock()->upwardExposedUses->Test(symStore->m_id))
            {LOGMEIN("GlobOpt.cpp] 2793\n");
                // If symStore isn't singleDef, we need to make sure it still has the same value.
                // This usually fails if we are not aggressive at transferring values in the prepass.
                Value **pSymStoreFromValue = valueTable->Get(symStore->m_id);

                // Consider: We should be fine if symStore isn't live in landing pad...
                if (!pSymStoreFromValue || (*pSymStoreFromValue)->GetValueNumber() != value->GetValueNumber())
                {LOGMEIN("GlobOpt.cpp] 2800\n");
                    continue;
                }
            }

            BasicBlock *landingPad = loop->landingPad;
            Value *landingPadValue = this->FindValue(landingPad->globOptData.symToValueMap, propertySym);

            if (!landingPadValue)
            {LOGMEIN("GlobOpt.cpp] 2809\n");
                // Value should be added as initial value or already be there.
                return nullptr;
            }

            IR::Instr * ldInstr = this->prePassInstrMap->Lookup(propertySym->m_id, nullptr);

            if (!ldInstr)
            {LOGMEIN("GlobOpt.cpp] 2817\n");
                continue;
            }

            if (!candidates)
            {LOGMEIN("GlobOpt.cpp] 2822\n");
                candidates = Anew(alloc, PRECandidatesList, alloc);
            }

            candidates->Prepend(&bucket);

        } NEXT_SLISTBASE_ENTRY;
    }

    return candidates;
}

PRECandidatesList * GlobOpt::RemoveUnavailableCandidates(BasicBlock *block, PRECandidatesList *candidates, JitArenaAllocator *alloc)
{
    // In case of multiple back-edges to the loop, make sure the candidates are still valid.
    FOREACH_SLIST_ENTRY_EDITING(GlobHashBucket*, candidate, (SList<GlobHashBucket*>*)candidates, iter)
    {LOGMEIN("GlobOpt.cpp] 2838\n");
        Value *candidateValue = candidate->element;
        PropertySym *candidatePropertySym = candidate->value->AsPropertySym();
        ValueNumber valueNumber = candidateValue->GetValueNumber();
        Sym *symStore = candidateValue->GetValueInfo()->GetSymStore();

        Value *blockValue = this->FindValue(block->globOptData.symToValueMap, candidatePropertySym);

        if (blockValue && blockValue->GetValueNumber() == valueNumber
            && blockValue->GetValueInfo()->GetSymStore() == symStore)
        {LOGMEIN("GlobOpt.cpp] 2848\n");
            Value *symStoreValue = this->FindValue(block->globOptData.symToValueMap, symStore);

            if (symStoreValue && symStoreValue->GetValueNumber() == valueNumber)
            {LOGMEIN("GlobOpt.cpp] 2852\n");
                continue;
            }
        }

        iter.RemoveCurrent();
    } NEXT_SLIST_ENTRY_EDITING;

    return candidates;
}

PRECandidatesList * GlobOpt::FindPossiblePRECandidates(Loop *loop, JitArenaAllocator *alloc)
{LOGMEIN("GlobOpt.cpp] 2864\n");
    // Find the set of PRE candidates
    BasicBlock *loopHeader = loop->GetHeadBlock();
    PRECandidatesList *candidates = nullptr;
    bool firstBackEdge = true;

    FOREACH_PREDECESSOR_BLOCK(blockPred, loopHeader)
    {LOGMEIN("GlobOpt.cpp] 2871\n");
        if (!loop->IsDescendentOrSelf(blockPred->loop))
        {LOGMEIN("GlobOpt.cpp] 2873\n");
            // Not a loop back-edge
            continue;
        }
        if (firstBackEdge)
        {LOGMEIN("GlobOpt.cpp] 2878\n");
            candidates = this->FindBackEdgePRECandidates(blockPred, alloc);
        }
        else
        {
            candidates = this->RemoveUnavailableCandidates(blockPred, candidates, alloc);
        }
    } NEXT_PREDECESSOR_BLOCK;

    return candidates;
}

BOOL GlobOpt::PreloadPRECandidate(Loop *loop, GlobHashBucket* candidate)
{LOGMEIN("GlobOpt.cpp] 2891\n");
    // Insert a load for each field PRE candidate.
    PropertySym *propertySym = candidate->value->AsPropertySym();
    StackSym *objPtrSym = propertySym->m_stackSym;

    // If objPtr isn't live, we'll retry later.
    // Another PRE candidate may insert a load for it.
    if (!this->IsLive(objPtrSym, loop->landingPad))
    {LOGMEIN("GlobOpt.cpp] 2899\n");
        return false;
    }
    BasicBlock *landingPad = loop->landingPad;
    Value *value = candidate->element;
    Sym *symStore = value->GetValueInfo()->GetSymStore();

    // The symStore can't be live into the loop
    // The symStore needs to still have the same value
    Assert(symStore && symStore->IsStackSym());

    if (this->IsLive(symStore, loop->landingPad))
    {LOGMEIN("GlobOpt.cpp] 2911\n");
        // May have already been hoisted:
        //  o.x = t1;
        //  o.y = t1;
        return false;
    }
    Value *landingPadValue = this->FindValue(landingPad->globOptData.symToValueMap, propertySym);

    // Value should be added as initial value or already be there.
    Assert(landingPadValue);

    IR::Instr * ldInstr = this->prePassInstrMap->Lookup(propertySym->m_id, nullptr);
    Assert(ldInstr);

    // Create instr to put in landing pad for compensation
    Assert(IsPREInstrCandidateLoad(ldInstr->m_opcode));
    IR::SymOpnd *ldSrc = ldInstr->GetSrc1()->AsSymOpnd();

    if (ldSrc->m_sym != propertySym)
    {LOGMEIN("GlobOpt.cpp] 2930\n");
        // It's possible that the propertySym but have equivalent objPtrs.  Verify their values.
        Value *val1 = this->FindValue(ldSrc->m_sym->AsPropertySym()->m_stackSym);
        Value *val2 = this->FindValue(propertySym->m_stackSym);
        if (!val1 || !val2 || val1->GetValueNumber() != val2->GetValueNumber())
        {LOGMEIN("GlobOpt.cpp] 2935\n");
            return false;
        }
    }

    ldInstr = ldInstr->Copy();

    // Consider: Shouldn't be necessary once we have copy-prop in prepass...
    ldInstr->GetSrc1()->AsSymOpnd()->m_sym = propertySym;
    ldSrc = ldInstr->GetSrc1()->AsSymOpnd();

    if (ldSrc->IsPropertySymOpnd())
    {LOGMEIN("GlobOpt.cpp] 2947\n");
        IR::PropertySymOpnd *propSymOpnd = ldSrc->AsPropertySymOpnd();
        IR::PropertySymOpnd *newPropSymOpnd;

        newPropSymOpnd = propSymOpnd->AsPropertySymOpnd()->CopyWithoutFlowSensitiveInfo(this->func);
        ldInstr->ReplaceSrc1(newPropSymOpnd);
    }

    if (ldInstr->GetDst()->AsRegOpnd()->m_sym != symStore)
    {LOGMEIN("GlobOpt.cpp] 2956\n");
        ldInstr->ReplaceDst(IR::RegOpnd::New(symStore->AsStackSym(), TyVar, this->func));
    }

    ldInstr->GetSrc1()->SetIsJITOptimizedReg(true);
    ldInstr->GetDst()->SetIsJITOptimizedReg(true);

    landingPad->globOptData.liveVarSyms->Set(symStore->m_id);
    loop->fieldPRESymStore->Set(symStore->m_id);

    ValueType valueType(ValueType::Uninitialized);
    Value *initialValue;

    if (loop->initialValueFieldMap.TryGetValue(propertySym, &initialValue))
    {LOGMEIN("GlobOpt.cpp] 2970\n");
        if (ldInstr->IsProfiledInstr())
        {LOGMEIN("GlobOpt.cpp] 2972\n");
            if (initialValue->GetValueNumber() == value->GetValueNumber())
            {LOGMEIN("GlobOpt.cpp] 2974\n");
                if (value->GetValueInfo()->IsUninitialized())
                {LOGMEIN("GlobOpt.cpp] 2976\n");
                    valueType = ldInstr->AsProfiledInstr()->u.FldInfo().valueType;
                }
                else
                {
                    valueType = value->GetValueInfo()->Type();
                }
            }
            else
            {
                valueType = ValueType::Uninitialized;
            }
            ldInstr->AsProfiledInstr()->u.FldInfo().valueType = valueType;
        }
    }
    else
    {
        valueType = landingPadValue->GetValueInfo()->Type();
    }

    loop->symsUsedBeforeDefined->Set(symStore->m_id);

    if (valueType.IsLikelyNumber())
    {LOGMEIN("GlobOpt.cpp] 2999\n");
        loop->likelyNumberSymsUsedBeforeDefined->Set(symStore->m_id);
        if (DoAggressiveIntTypeSpec() ? valueType.IsLikelyInt() : valueType.IsInt())
        {LOGMEIN("GlobOpt.cpp] 3002\n");
            // Can only force int conversions in the landing pad based on likely-int values if aggressive int type
            // specialization is enabled
            loop->likelyIntSymsUsedBeforeDefined->Set(symStore->m_id);
        }
    }

    // Insert in landing pad
    if (ldInstr->HasAnyImplicitCalls())
    {LOGMEIN("GlobOpt.cpp] 3011\n");
        IR::Instr * bailInstr = EnsureDisableImplicitCallRegion(loop);

        bailInstr->InsertBefore(ldInstr);
    }
    else if (loop->endDisableImplicitCall)
    {LOGMEIN("GlobOpt.cpp] 3017\n");
        loop->endDisableImplicitCall->InsertBefore(ldInstr);
    }
    else
    {
        loop->landingPad->InsertAfter(ldInstr);
    }

    ldInstr->ClearByteCodeOffset();
    ldInstr->SetByteCodeOffset(landingPad->GetFirstInstr());

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldPREPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
    {LOGMEIN("GlobOpt.cpp] 3030\n");
        Output::Print(_u("** TRACE: Field PRE: field pre-loaded in landing pad of loop head #%-3d: "), loop->GetHeadBlock()->GetBlockNum());
        ldInstr->Dump();
        Output::Print(_u("\n"));
    }
#endif

    return true;
}

void GlobOpt::PreloadPRECandidates(Loop *loop, PRECandidatesList *candidates)
{LOGMEIN("GlobOpt.cpp] 3041\n");
    // Insert loads in landing pad for field PRE candidates. Iterate while(changed)
    // for the o.x.y cases.
    BOOL changed = true;

    if (!candidates)
    {LOGMEIN("GlobOpt.cpp] 3047\n");
        return;
    }

    Assert(loop->landingPad->GetFirstInstr() == loop->landingPad->GetLastInstr());

    while (changed)
    {LOGMEIN("GlobOpt.cpp] 3054\n");
        changed = false;
        FOREACH_SLIST_ENTRY_EDITING(GlobHashBucket*, candidate, (SList<GlobHashBucket*>*)candidates, iter)
        {LOGMEIN("GlobOpt.cpp] 3057\n");
            if (this->PreloadPRECandidate(loop, candidate))
            {LOGMEIN("GlobOpt.cpp] 3059\n");
                changed = true;
                iter.RemoveCurrent();
            }
        } NEXT_SLIST_ENTRY_EDITING;
    }
}

void GlobOpt::FieldPRE(Loop *loop)
{LOGMEIN("GlobOpt.cpp] 3068\n");
    if (!DoFieldPRE(loop))
    {LOGMEIN("GlobOpt.cpp] 3070\n");
        return;
    }

    PRECandidatesList *candidates;
    JitArenaAllocator *alloc = this->tempAlloc;

    candidates = this->FindPossiblePRECandidates(loop, alloc);
    this->PreloadPRECandidates(loop, candidates);
}

void GlobOpt::InsertCloneStrs(BasicBlock *toBlock, GlobOptBlockData *toData, GlobOptBlockData *fromData)
{LOGMEIN("GlobOpt.cpp] 3082\n");
    if (toBlock->isLoopHeader   // isLoopBackEdge
        && toBlock->cloneStrCandidates
        && !IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 3086\n");
        Loop *loop = toBlock->loop;
        BasicBlock *landingPad = loop->landingPad;
        const SymTable *const symTable = func->m_symTable;
        Assert(tempBv->IsEmpty());
        tempBv->And(toBlock->cloneStrCandidates, fromData->isTempSrc);
        FOREACH_BITSET_IN_SPARSEBV(id, tempBv)
        {LOGMEIN("GlobOpt.cpp] 3093\n");
            StackSym *const sym = (StackSym *)symTable->Find(id);
            Assert(sym);

            if (!landingPad->globOptData.liveVarSyms->Test(id)
                || !fromData->liveVarSyms->Test(id))
            {LOGMEIN("GlobOpt.cpp] 3099\n");
                continue;
            }

            Value * landingPadValue = FindValue(landingPad->globOptData.symToValueMap, sym);
            if (landingPadValue == nullptr)
            {LOGMEIN("GlobOpt.cpp] 3105\n");
                continue;
            }

            Value * loopValue = FindValue(fromData->symToValueMap, sym);
            if (loopValue == nullptr)
            {LOGMEIN("GlobOpt.cpp] 3111\n");
                continue;
            }

            ValueInfo *landingPadValueInfo = landingPadValue->GetValueInfo();
            ValueInfo *loopValueInfo = loopValue->GetValueInfo();

            if (landingPadValueInfo->IsLikelyString()
                && loopValueInfo->IsLikelyString())
            {LOGMEIN("GlobOpt.cpp] 3120\n");
                IR::Instr *cloneStr = IR::Instr::New(Js::OpCode::CloneStr, this->func);
                IR::RegOpnd *opnd = IR::RegOpnd::New(sym, IRType::TyVar, this->func);
                cloneStr->SetDst(opnd);
                cloneStr->SetSrc1(opnd);
                if (loop->bailOutInfo->bailOutInstr)
                {LOGMEIN("GlobOpt.cpp] 3126\n");
                    loop->bailOutInfo->bailOutInstr->InsertBefore(cloneStr);
                }
                else
                {
                    landingPad->InsertAfter(cloneStr);
                }
                toData->isTempSrc->Set(id);
            }
        }
        NEXT_BITSET_IN_SPARSEBV;
        tempBv->ClearAll();
    }
}

void
GlobOpt::MergeValueMaps(
    GlobOptBlockData *toData,
    BasicBlock *toBlock,
    BasicBlock *fromBlock,
    BVSparse<JitArenaAllocator> *const symsRequiringCompensation,
    BVSparse<JitArenaAllocator> *const symsCreatedForMerge)
{LOGMEIN("GlobOpt.cpp] 3148\n");
    GlobOptBlockData *fromData = &(fromBlock->globOptData);
    bool isLoopBackEdge = toBlock->isLoopHeader;
    Loop *loop = toBlock->loop;
    bool isLoopPrepass = (loop && this->prePassLoop == loop);

    Assert(valuesCreatedForMerge->Count() == 0);
    DebugOnly(ValueSetByValueNumber mergedValues(tempAlloc, 64));

    BVSparse<JitArenaAllocator> *const mergedValueTypesTrackedForKills = tempBv;
    Assert(mergedValueTypesTrackedForKills->IsEmpty());
    toData->valuesToKillOnCalls->Clear(); // the tracking will be reevaluated based on merged value types

    GlobHashTable *thisTable = toData->symToValueMap;
    GlobHashTable *otherTable = fromData->symToValueMap;
    for (uint i = 0; i < thisTable->tableSize; i++)
    {LOGMEIN("GlobOpt.cpp] 3164\n");
        SListBase<GlobHashBucket>::Iterator iter2(&otherTable->table[i]);
        iter2.Next();
        FOREACH_SLISTBASE_ENTRY_EDITING(GlobHashBucket, bucket, &thisTable->table[i], iter)
        {LOGMEIN("GlobOpt.cpp] 3168\n");
            while (iter2.IsValid() && bucket.value->m_id < iter2.Data().value->m_id)
            {LOGMEIN("GlobOpt.cpp] 3170\n");
                iter2.Next();
            }
            Value *newValue = nullptr;

            if (iter2.IsValid() && bucket.value->m_id == iter2.Data().value->m_id)
            {LOGMEIN("GlobOpt.cpp] 3176\n");
                newValue =
                    MergeValues(
                        bucket.element,
                        iter2.Data().element,
                        iter2.Data().value,
                        toData,
                        fromData,
                        isLoopBackEdge,
                        symsRequiringCompensation,
                        symsCreatedForMerge);
            }
            if (newValue == nullptr)
            {LOGMEIN("GlobOpt.cpp] 3189\n");
                iter.RemoveCurrent(thisTable->alloc);
                continue;
            }
            else
            {
#if DBG
                // Ensure that only one value per value number is produced by merge. Byte-code constant values are reused in
                // multiple blocks without cloning, so exclude those value numbers.
                {
                    Value *const previouslyMergedValue = mergedValues.Lookup(newValue->GetValueNumber());
                    if (previouslyMergedValue)
                    {LOGMEIN("GlobOpt.cpp] 3201\n");
                        if (!byteCodeConstantValueNumbersBv->Test(newValue->GetValueNumber()))
                        {LOGMEIN("GlobOpt.cpp] 3203\n");
                            Assert(newValue == previouslyMergedValue);
                        }
                    }
                    else
                    {
                        mergedValues.Add(newValue);
                    }
                }
#endif

                TrackMergedValueForKills(newValue, toData, mergedValueTypesTrackedForKills);
                bucket.element = newValue;
            }
            iter2.Next();
        } NEXT_SLISTBASE_ENTRY_EDITING;

        if (isLoopPrepass && !this->rootLoopPrePass->allFieldsKilled)
        {LOGMEIN("GlobOpt.cpp] 3221\n");
            while (iter2.IsValid())
            {LOGMEIN("GlobOpt.cpp] 3223\n");
                iter2.Next();
            }
        }
    }

    valuesCreatedForMerge->Clear();
    DebugOnly(mergedValues.Reset());
    mergedValueTypesTrackedForKills->ClearAll();

    toData->exprToValueMap->And(fromData->exprToValueMap);

    ProcessValueKills(toBlock, toData);

    bool isLastLoopBackEdge = false;

    if (isLoopBackEdge)
    {
        ProcessValueKillsForLoopHeaderAfterBackEdgeMerge(toBlock, toData);

        BasicBlock *lastBlock = nullptr;
        FOREACH_PREDECESSOR_BLOCK(pred, toBlock)
        {LOGMEIN("GlobOpt.cpp] 3245\n");
            Assert(!lastBlock || pred->GetBlockNum() > lastBlock->GetBlockNum());
            lastBlock = pred;
        }NEXT_PREDECESSOR_BLOCK;
        isLastLoopBackEdge = (lastBlock == fromBlock);
    }
}

Value *
GlobOpt::MergeValues(
    Value *toDataValue,
    Value *fromDataValue,
    Sym *fromDataSym,
    GlobOptBlockData *toData,
    GlobOptBlockData *fromData,
    bool isLoopBackEdge,
    BVSparse<JitArenaAllocator> *const symsRequiringCompensation,
    BVSparse<JitArenaAllocator> *const symsCreatedForMerge)
{LOGMEIN("GlobOpt.cpp] 3263\n");
    // Same map
    if (toDataValue == fromDataValue)
    {LOGMEIN("GlobOpt.cpp] 3266\n");
        return toDataValue;
    }

    const ValueNumberPair sourceValueNumberPair(toDataValue->GetValueNumber(), fromDataValue->GetValueNumber());
    const bool sameValueNumber = sourceValueNumberPair.First() == sourceValueNumberPair.Second();

    ValueInfo *newValueInfo =
        this->MergeValueInfo(
            toDataValue,
            fromDataValue,
            fromDataSym,
            fromData,
            isLoopBackEdge,
            sameValueNumber,
            symsRequiringCompensation,
            symsCreatedForMerge);

    if (newValueInfo == nullptr)
    {LOGMEIN("GlobOpt.cpp] 3285\n");
        return nullptr;
    }

    if (sameValueNumber && newValueInfo == toDataValue->GetValueInfo())
    {LOGMEIN("GlobOpt.cpp] 3290\n");
        return toDataValue;
    }

    // There may be other syms in toData that haven't been merged yet, referring to the current toData value for this sym. If
    // the merge produced a new value info, don't corrupt the value info for the other sym by changing the same value. Instead,
    // create one value per source value number pair per merge and reuse that for new value infos.
    Value *newValue = valuesCreatedForMerge->Lookup(sourceValueNumberPair, nullptr);
    if(newValue)
    {LOGMEIN("GlobOpt.cpp] 3299\n");
        Assert(sameValueNumber == (newValue->GetValueNumber() == toDataValue->GetValueNumber()));

        // This is an exception where Value::SetValueInfo is called directly instead of GlobOpt::ChangeValueInfo, because we're
        // actually generating new value info through merges.
        newValue->SetValueInfo(newValueInfo);
    }
    else
    {
        newValue = NewValue(sameValueNumber ? sourceValueNumberPair.First() : NewValueNumber(), newValueInfo);
        valuesCreatedForMerge->Add(sourceValueNumberPair, newValue);
    }

    // Set symStore if same on both paths.
    if (toDataValue->GetValueInfo()->GetSymStore() == fromDataValue->GetValueInfo()->GetSymStore())
    {LOGMEIN("GlobOpt.cpp] 3314\n");
        this->SetSymStoreDirect(newValueInfo, toDataValue->GetValueInfo()->GetSymStore());
    }

    return newValue;
}

ValueInfo *
GlobOpt::MergeValueInfo(
    Value *toDataVal,
    Value *fromDataVal,
    Sym *fromDataSym,
    GlobOptBlockData *fromData,
    bool isLoopBackEdge,
    bool sameValueNumber,
    BVSparse<JitArenaAllocator> *const symsRequiringCompensation,
    BVSparse<JitArenaAllocator> *const symsCreatedForMerge)
{LOGMEIN("GlobOpt.cpp] 3331\n");
    ValueInfo *const toDataValueInfo = toDataVal->GetValueInfo();
    ValueInfo *const fromDataValueInfo = fromDataVal->GetValueInfo();

    // Same value
    if (toDataValueInfo == fromDataValueInfo)
    {LOGMEIN("GlobOpt.cpp] 3337\n");
        return toDataValueInfo;
    }

    if (toDataValueInfo->IsJsType() || fromDataValueInfo->IsJsType())
    {LOGMEIN("GlobOpt.cpp] 3342\n");
        Assert(toDataValueInfo->IsJsType() && fromDataValueInfo->IsJsType());
        return MergeJsTypeValueInfo(toDataValueInfo->AsJsType(), fromDataValueInfo->AsJsType(), isLoopBackEdge, sameValueNumber);
    }

    ValueType newValueType(toDataValueInfo->Type().Merge(fromDataValueInfo->Type()));
    if (newValueType.IsLikelyInt())
    {LOGMEIN("GlobOpt.cpp] 3349\n");
        return MergeLikelyIntValueInfo(toDataVal, fromDataVal, newValueType);
    }
    if(newValueType.IsLikelyAnyOptimizedArray())
    {LOGMEIN("GlobOpt.cpp] 3353\n");
        if(newValueType.IsLikelyArrayOrObjectWithArray() &&
            toDataValueInfo->IsLikelyArrayOrObjectWithArray() &&
            fromDataValueInfo->IsLikelyArrayOrObjectWithArray())
        {LOGMEIN("GlobOpt.cpp] 3357\n");
            // Value type merge for missing values is aggressive by default (for profile data) - if either side likely has no
            // missing values, then the merged value type also likely has no missing values. This is because arrays often start
            // off having missing values but are eventually filled up. In GlobOpt however, we need to be conservative because
            // the existence of a value type that likely has missing values indicates that it is more likely for it to have
            // missing values than not. Also, StElems that are likely to create missing values are tracked in profile data and
            // will update value types to say they are now likely to have missing values, and that needs to be propagated
            // conservatively.
            newValueType =
                newValueType.SetHasNoMissingValues(
                    toDataValueInfo->HasNoMissingValues() && fromDataValueInfo->HasNoMissingValues());

            if(toDataValueInfo->HasIntElements() != fromDataValueInfo->HasIntElements() ||
                toDataValueInfo->HasFloatElements() != fromDataValueInfo->HasFloatElements())
            {LOGMEIN("GlobOpt.cpp] 3371\n");
                // When merging arrays with different native storage types, make the merged value type a likely version to force
                // array checks to be done again and cause a conversion and/or bailout as necessary
                newValueType = newValueType.ToLikely();
            }
        }

        if(!(newValueType.IsObject() && toDataValueInfo->IsArrayValueInfo() && fromDataValueInfo->IsArrayValueInfo()))
        {LOGMEIN("GlobOpt.cpp] 3379\n");
            return ValueInfo::New(alloc, newValueType);
        }

        return
            MergeArrayValueInfo(
                newValueType,
                toDataValueInfo->AsArrayValueInfo(),
                fromDataValueInfo->AsArrayValueInfo(),
                fromDataSym,
                symsRequiringCompensation,
                symsCreatedForMerge);
    }

    // Consider: If both values are VarConstantValueInfo with the same value, we could
    // merge them preserving the value.
    return ValueInfo::New(this->alloc, newValueType);
}

ValueInfo *
GlobOpt::MergeLikelyIntValueInfo(Value *toDataVal, Value *fromDataVal, ValueType const newValueType)
{LOGMEIN("GlobOpt.cpp] 3400\n");
    Assert(newValueType.IsLikelyInt());

    ValueInfo *const toDataValueInfo = toDataVal->GetValueInfo();
    ValueInfo *const fromDataValueInfo = fromDataVal->GetValueInfo();
    Assert(toDataValueInfo != fromDataValueInfo);

    bool wasNegativeZeroPreventedByBailout;
    if(newValueType.IsInt())
    {LOGMEIN("GlobOpt.cpp] 3409\n");
        int32 toDataIntConstantValue, fromDataIntConstantValue;
        if (toDataValueInfo->TryGetIntConstantValue(&toDataIntConstantValue) &&
            fromDataValueInfo->TryGetIntConstantValue(&fromDataIntConstantValue) &&
            toDataIntConstantValue == fromDataIntConstantValue)
        {LOGMEIN("GlobOpt.cpp] 3414\n");
            // A new value number must be created to register the fact that the value has changed. Otherwise, if the value
            // changed inside a loop, the sym may look invariant on the loop back-edge (and hence not turned into a number
            // value), and its constant value from the first iteration may be incorrectly propagated after the loop.
            return IntConstantValueInfo::New(this->alloc, toDataIntConstantValue);
        }

        wasNegativeZeroPreventedByBailout =
            toDataValueInfo->WasNegativeZeroPreventedByBailout() ||
            fromDataValueInfo->WasNegativeZeroPreventedByBailout();
    }
    else
    {
        wasNegativeZeroPreventedByBailout = false;
    }

    const IntBounds *const toDataValBounds =
        toDataValueInfo->IsIntBounded() ? toDataValueInfo->AsIntBounded()->Bounds() : nullptr;
    const IntBounds *const fromDataValBounds =
        fromDataValueInfo->IsIntBounded() ? fromDataValueInfo->AsIntBounded()->Bounds() : nullptr;
    if(toDataValBounds || fromDataValBounds)
    {LOGMEIN("GlobOpt.cpp] 3435\n");
        const IntBounds *mergedBounds;
        if(toDataValBounds && fromDataValBounds)
        {LOGMEIN("GlobOpt.cpp] 3438\n");
            mergedBounds = IntBounds::Merge(toDataVal, toDataValBounds, fromDataVal, fromDataValBounds);
        }
        else
        {
            IntConstantBounds constantBounds;
            if(toDataValBounds)
            {LOGMEIN("GlobOpt.cpp] 3445\n");
                mergedBounds =
                    fromDataValueInfo->TryGetIntConstantBounds(&constantBounds, true)
                        ? IntBounds::Merge(toDataVal, toDataValBounds, fromDataVal, constantBounds)
                        : nullptr;
            }
            else
            {
                Assert(fromDataValBounds);
                mergedBounds =
                    toDataValueInfo->TryGetIntConstantBounds(&constantBounds, true)
                        ? IntBounds::Merge(fromDataVal, fromDataValBounds, toDataVal, constantBounds)
                        : nullptr;
            }
        }

        if(mergedBounds)
        {LOGMEIN("GlobOpt.cpp] 3462\n");
            if(mergedBounds->RequiresIntBoundedValueInfo(newValueType))
            {LOGMEIN("GlobOpt.cpp] 3464\n");
                return IntBoundedValueInfo::New(newValueType, mergedBounds, wasNegativeZeroPreventedByBailout, alloc);
            }
            mergedBounds->Delete();
        }
    }

    if(newValueType.IsInt())
    {LOGMEIN("GlobOpt.cpp] 3472\n");
        int32 min1, max1, min2, max2;
        toDataValueInfo->GetIntValMinMax(&min1, &max1, false);
        fromDataValueInfo->GetIntValMinMax(&min2, &max2, false);
        return NewIntRangeValueInfo(min(min1, min2), max(max1, max2), wasNegativeZeroPreventedByBailout);
    }

    return ValueInfo::New(alloc, newValueType);
}

JsTypeValueInfo* GlobOpt::MergeJsTypeValueInfo(JsTypeValueInfo * toValueInfo, JsTypeValueInfo * fromValueInfo, bool isLoopBackEdge, bool sameValueNumber)
{LOGMEIN("GlobOpt.cpp] 3483\n");
    Assert(toValueInfo != fromValueInfo);

    // On loop back edges we must be conservative and only consider type values which are invariant throughout the loop.
    // That's because in dead store pass we can't correctly track object pointer assignments (o = p), and we may not
    // be able to register correct type checks for the right properties upstream. If we ever figure out how to enhance
    // the dead store pass to track this info we could go more aggressively, as below.
    if (isLoopBackEdge && !sameValueNumber)
    {LOGMEIN("GlobOpt.cpp] 3491\n");
        return nullptr;
    }

    const JITTypeHolder toType = toValueInfo->GetJsType();
    const JITTypeHolder fromType = fromValueInfo->GetJsType();
    const JITTypeHolder mergedType = toType == fromType ? toType : JITTypeHolder(nullptr);

    Js::EquivalentTypeSet* toTypeSet = toValueInfo->GetJsTypeSet();
    Js::EquivalentTypeSet* fromTypeSet = fromValueInfo->GetJsTypeSet();
    Js::EquivalentTypeSet* mergedTypeSet = (toTypeSet != nullptr && fromTypeSet != nullptr && AreTypeSetsIdentical(toTypeSet, fromTypeSet)) ? toTypeSet : nullptr;

#if DBG_DUMP
    if (PHASE_TRACE(Js::ObjTypeSpecPhase, this->func) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 3505\n");
        Output::Print(_u("ObjTypeSpec: Merging type value info:\n"));
        Output::Print(_u("    from (shared %d): "), fromValueInfo->GetIsShared());
        fromValueInfo->Dump();
        Output::Print(_u("\n    to (shared %d): "), toValueInfo->GetIsShared());
        toValueInfo->Dump();
    }
#endif

    if (mergedType == toType && mergedTypeSet == toTypeSet)
    {LOGMEIN("GlobOpt.cpp] 3515\n");
#if DBG_DUMP
        if (PHASE_TRACE(Js::ObjTypeSpecPhase, this->func) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 3518\n");
            Output::Print(_u("\n    result (shared %d): "), toValueInfo->GetIsShared());
            toValueInfo->Dump();
            Output::Print(_u("\n"));
        }
#endif
        return toValueInfo;
    }

    if (mergedType == nullptr && mergedTypeSet == nullptr)
    {LOGMEIN("GlobOpt.cpp] 3528\n");
        // No info, so don't bother making a value.
        return nullptr;
    }

    if (toValueInfo->GetIsShared())
    {LOGMEIN("GlobOpt.cpp] 3534\n");
        JsTypeValueInfo* mergedValueInfo = JsTypeValueInfo::New(this->alloc, mergedType, mergedTypeSet);
#if DBG_DUMP
        if (PHASE_TRACE(Js::ObjTypeSpecPhase, this->func) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 3538\n");
            Output::Print(_u("\n    result (shared %d): "), mergedValueInfo->GetIsShared());
            mergedValueInfo->Dump();
            Output::Print(_u("\n"));
        }
#endif
        return mergedValueInfo;
    }
    else
    {
        toValueInfo->SetJsType(mergedType);
        toValueInfo->SetJsTypeSet(mergedTypeSet);
#if DBG_DUMP
        if (PHASE_TRACE(Js::ObjTypeSpecPhase, this->func) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 3552\n");
            Output::Print(_u("\n    result (shared %d): "), toValueInfo->GetIsShared());
            toValueInfo->Dump();
            Output::Print(_u("\n"));
        }
#endif
        return toValueInfo;
    }
}

ValueInfo *GlobOpt::MergeArrayValueInfo(
    const ValueType mergedValueType,
    const ArrayValueInfo *const toDataValueInfo,
    const ArrayValueInfo *const fromDataValueInfo,
    Sym *const arraySym,
    BVSparse<JitArenaAllocator> *const symsRequiringCompensation,
    BVSparse<JitArenaAllocator> *const symsCreatedForMerge)
{LOGMEIN("GlobOpt.cpp] 3569\n");
    Assert(mergedValueType.IsAnyOptimizedArray());
    Assert(toDataValueInfo);
    Assert(fromDataValueInfo);
    Assert(toDataValueInfo != fromDataValueInfo);
    Assert(arraySym);
    Assert(!symsRequiringCompensation == IsLoopPrePass());
    Assert(!symsCreatedForMerge == IsLoopPrePass());

    // Merge the segment and segment length syms. If we have the segment and/or the segment length syms available on both sides
    // but in different syms, create a new sym and record that the array sym requires compensation. Compensation will be
    // inserted later to initialize this new sym from all predecessors of the merged block.

    StackSym *newHeadSegmentSym;
    if(toDataValueInfo->HeadSegmentSym() && fromDataValueInfo->HeadSegmentSym())
    {LOGMEIN("GlobOpt.cpp] 3584\n");
        if(toDataValueInfo->HeadSegmentSym() == fromDataValueInfo->HeadSegmentSym())
        {LOGMEIN("GlobOpt.cpp] 3586\n");
            newHeadSegmentSym = toDataValueInfo->HeadSegmentSym();
        }
        else
        {
            Assert(!IsLoopPrePass());
            Assert(symsRequiringCompensation);
            symsRequiringCompensation->Set(arraySym->m_id);
            Assert(symsCreatedForMerge);
            if(symsCreatedForMerge->Test(toDataValueInfo->HeadSegmentSym()->m_id))
            {LOGMEIN("GlobOpt.cpp] 3596\n");
                newHeadSegmentSym = toDataValueInfo->HeadSegmentSym();
            }
            else
            {
                newHeadSegmentSym = StackSym::New(TyMachPtr, func);
                symsCreatedForMerge->Set(newHeadSegmentSym->m_id);
            }
        }
    }
    else
    {
        newHeadSegmentSym = nullptr;
    }

    StackSym *newHeadSegmentLengthSym;
    if(toDataValueInfo->HeadSegmentLengthSym() && fromDataValueInfo->HeadSegmentLengthSym())
    {LOGMEIN("GlobOpt.cpp] 3613\n");
        if(toDataValueInfo->HeadSegmentLengthSym() == fromDataValueInfo->HeadSegmentLengthSym())
        {LOGMEIN("GlobOpt.cpp] 3615\n");
            newHeadSegmentLengthSym = toDataValueInfo->HeadSegmentLengthSym();
        }
        else
        {
            Assert(!IsLoopPrePass());
            Assert(symsRequiringCompensation);
            symsRequiringCompensation->Set(arraySym->m_id);
            Assert(symsCreatedForMerge);
            if(symsCreatedForMerge->Test(toDataValueInfo->HeadSegmentLengthSym()->m_id))
            {LOGMEIN("GlobOpt.cpp] 3625\n");
                newHeadSegmentLengthSym = toDataValueInfo->HeadSegmentLengthSym();
            }
            else
            {
                newHeadSegmentLengthSym = StackSym::New(TyUint32, func);
                symsCreatedForMerge->Set(newHeadSegmentLengthSym->m_id);
            }
        }
    }
    else
    {
        newHeadSegmentLengthSym = nullptr;
    }

    StackSym *newLengthSym;
    if(toDataValueInfo->LengthSym() && fromDataValueInfo->LengthSym())
    {LOGMEIN("GlobOpt.cpp] 3642\n");
        if(toDataValueInfo->LengthSym() == fromDataValueInfo->LengthSym())
        {LOGMEIN("GlobOpt.cpp] 3644\n");
            newLengthSym = toDataValueInfo->LengthSym();
        }
        else
        {
            Assert(!IsLoopPrePass());
            Assert(symsRequiringCompensation);
            symsRequiringCompensation->Set(arraySym->m_id);
            Assert(symsCreatedForMerge);
            if(symsCreatedForMerge->Test(toDataValueInfo->LengthSym()->m_id))
            {LOGMEIN("GlobOpt.cpp] 3654\n");
                newLengthSym = toDataValueInfo->LengthSym();
            }
            else
            {
                newLengthSym = StackSym::New(TyUint32, func);
                symsCreatedForMerge->Set(newLengthSym->m_id);
            }
        }
    }
    else
    {
        newLengthSym = nullptr;
    }

    if(newHeadSegmentSym || newHeadSegmentLengthSym || newLengthSym)
    {LOGMEIN("GlobOpt.cpp] 3670\n");
        return ArrayValueInfo::New(alloc, mergedValueType, newHeadSegmentSym, newHeadSegmentLengthSym, newLengthSym);
    }

    if(symsRequiringCompensation)
    {LOGMEIN("GlobOpt.cpp] 3675\n");
        symsRequiringCompensation->Clear(arraySym->m_id);
    }
    return ValueInfo::New(alloc, mergedValueType);
}

void GlobOpt::InsertValueCompensation(
    BasicBlock *const predecessor,
    const SymToValueInfoMap &symsRequiringCompensationToMergedValueInfoMap)
{LOGMEIN("GlobOpt.cpp] 3684\n");
    Assert(predecessor);
    Assert(symsRequiringCompensationToMergedValueInfoMap.Count() != 0);

    IR::Instr *insertBeforeInstr = predecessor->GetLastInstr();
    Func *const func = insertBeforeInstr->m_func;
    bool setLastInstrInPredecessor;
    if(insertBeforeInstr->IsBranchInstr() || insertBeforeInstr->m_opcode == Js::OpCode::BailTarget)
    {LOGMEIN("GlobOpt.cpp] 3692\n");
        // Don't insert code between the branch and the corresponding ByteCodeUses instructions
        while(insertBeforeInstr->m_prev->m_opcode == Js::OpCode::ByteCodeUses)
        {LOGMEIN("GlobOpt.cpp] 3695\n");
            insertBeforeInstr = insertBeforeInstr->m_prev;
        }
        setLastInstrInPredecessor = false;
    }
    else
    {
        // Insert at the end of the block and set the last instruction
        Assert(insertBeforeInstr->m_next);
        insertBeforeInstr = insertBeforeInstr->m_next; // Instruction after the last instruction in the predecessor
        setLastInstrInPredecessor = true;
    }

    GlobOptBlockData &predecessorBlockData = predecessor->globOptData;
    GlobHashTable *const predecessorSymToValueMap = predecessor->globOptData.symToValueMap;
    GlobOptBlockData &successorBlockData = blockData;
    GlobHashTable *const successorSymToValueMap = blockData.symToValueMap;
    for(auto it = symsRequiringCompensationToMergedValueInfoMap.GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOpt.cpp] 3713\n");
        const auto &entry = it.Current();
        Sym *const sym = entry.Key();
        Value *const predecessorValue = FindValue(predecessorSymToValueMap, sym);
        Assert(predecessorValue);
        ValueInfo *const predecessorValueInfo = predecessorValue->GetValueInfo();

        // Currently, array value infos are the only ones that require compensation based on values
        Assert(predecessorValueInfo->IsAnyOptimizedArray());
        const ArrayValueInfo *const predecessorArrayValueInfo = predecessorValueInfo->AsArrayValueInfo();
        StackSym *const predecessorHeadSegmentSym = predecessorArrayValueInfo->HeadSegmentSym();
        StackSym *const predecessorHeadSegmentLengthSym = predecessorArrayValueInfo->HeadSegmentLengthSym();
        StackSym *const predecessorLengthSym = predecessorArrayValueInfo->LengthSym();
        ValueInfo *const mergedValueInfo = entry.Value();
        const ArrayValueInfo *const mergedArrayValueInfo = mergedValueInfo->AsArrayValueInfo();
        StackSym *const mergedHeadSegmentSym = mergedArrayValueInfo->HeadSegmentSym();
        StackSym *const mergedHeadSegmentLengthSym = mergedArrayValueInfo->HeadSegmentLengthSym();
        StackSym *const mergedLengthSym = mergedArrayValueInfo->LengthSym();
        Assert(!mergedHeadSegmentSym || predecessorHeadSegmentSym);
        Assert(!mergedHeadSegmentLengthSym || predecessorHeadSegmentLengthSym);
        Assert(!mergedLengthSym || predecessorLengthSym);

        bool compensated = false;
        if(mergedHeadSegmentSym && predecessorHeadSegmentSym != mergedHeadSegmentSym)
        {LOGMEIN("GlobOpt.cpp] 3737\n");
            IR::Instr *const newInstr =
                IR::Instr::New(
                    Js::OpCode::Ld_A,
                    IR::RegOpnd::New(mergedHeadSegmentSym, mergedHeadSegmentSym->GetType(), func),
                    IR::RegOpnd::New(predecessorHeadSegmentSym, predecessorHeadSegmentSym->GetType(), func),
                    func);
            newInstr->GetDst()->SetIsJITOptimizedReg(true);
            newInstr->GetSrc1()->SetIsJITOptimizedReg(true);
            newInstr->SetByteCodeOffset(insertBeforeInstr);
            insertBeforeInstr->InsertBefore(newInstr);
            compensated = true;
        }

        if(mergedHeadSegmentLengthSym && predecessorHeadSegmentLengthSym != mergedHeadSegmentLengthSym)
        {LOGMEIN("GlobOpt.cpp] 3752\n");
            IR::Instr *const newInstr =
                IR::Instr::New(
                    Js::OpCode::Ld_I4,
                    IR::RegOpnd::New(mergedHeadSegmentLengthSym, mergedHeadSegmentLengthSym->GetType(), func),
                    IR::RegOpnd::New(predecessorHeadSegmentLengthSym, predecessorHeadSegmentLengthSym->GetType(), func),
                    func);
            newInstr->GetDst()->SetIsJITOptimizedReg(true);
            newInstr->GetSrc1()->SetIsJITOptimizedReg(true);
            newInstr->SetByteCodeOffset(insertBeforeInstr);
            insertBeforeInstr->InsertBefore(newInstr);
            compensated = true;

            // Merge the head segment length value
            Assert(predecessorBlockData.liveVarSyms->Test(predecessorHeadSegmentLengthSym->m_id));
            predecessorBlockData.liveVarSyms->Set(mergedHeadSegmentLengthSym->m_id);
            successorBlockData.liveVarSyms->Set(mergedHeadSegmentLengthSym->m_id);
            Value *const predecessorHeadSegmentLengthValue =
                FindValue(predecessorSymToValueMap, predecessorHeadSegmentLengthSym);
            Assert(predecessorHeadSegmentLengthValue);
            SetValue(&predecessorBlockData, predecessorHeadSegmentLengthValue, mergedHeadSegmentLengthSym);
            Value *const mergedHeadSegmentLengthValue = FindValue(successorSymToValueMap, mergedHeadSegmentLengthSym);
            if(mergedHeadSegmentLengthValue)
            {LOGMEIN("GlobOpt.cpp] 3775\n");
                Assert(mergedHeadSegmentLengthValue->GetValueNumber() != predecessorHeadSegmentLengthValue->GetValueNumber());
                if(predecessorHeadSegmentLengthValue->GetValueInfo() != mergedHeadSegmentLengthValue->GetValueInfo())
                {LOGMEIN("GlobOpt.cpp] 3778\n");
                    mergedHeadSegmentLengthValue->SetValueInfo(
                        MergeLikelyIntValueInfo(
                            mergedHeadSegmentLengthValue,
                            predecessorHeadSegmentLengthValue,
                            mergedHeadSegmentLengthValue->GetValueInfo()->Type()
                                .Merge(predecessorHeadSegmentLengthValue->GetValueInfo()->Type())));
                }
            }
            else
            {
                SetValue(&successorBlockData, CopyValue(predecessorHeadSegmentLengthValue), mergedHeadSegmentLengthSym);
            }
        }

        if(mergedLengthSym && predecessorLengthSym != mergedLengthSym)
        {LOGMEIN("GlobOpt.cpp] 3794\n");
            IR::Instr *const newInstr =
                IR::Instr::New(
                    Js::OpCode::Ld_I4,
                    IR::RegOpnd::New(mergedLengthSym, mergedLengthSym->GetType(), func),
                    IR::RegOpnd::New(predecessorLengthSym, predecessorLengthSym->GetType(), func),
                    func);
            newInstr->GetDst()->SetIsJITOptimizedReg(true);
            newInstr->GetSrc1()->SetIsJITOptimizedReg(true);
            newInstr->SetByteCodeOffset(insertBeforeInstr);
            insertBeforeInstr->InsertBefore(newInstr);
            compensated = true;

            // Merge the length value
            Assert(predecessorBlockData.liveVarSyms->Test(predecessorLengthSym->m_id));
            predecessorBlockData.liveVarSyms->Set(mergedLengthSym->m_id);
            successorBlockData.liveVarSyms->Set(mergedLengthSym->m_id);
            Value *const predecessorLengthValue = FindValue(predecessorSymToValueMap, predecessorLengthSym);
            Assert(predecessorLengthValue);
            SetValue(&predecessorBlockData, predecessorLengthValue, mergedLengthSym);
            Value *const mergedLengthValue = FindValue(successorSymToValueMap, mergedLengthSym);
            if(mergedLengthValue)
            {LOGMEIN("GlobOpt.cpp] 3816\n");
                Assert(mergedLengthValue->GetValueNumber() != predecessorLengthValue->GetValueNumber());
                if(predecessorLengthValue->GetValueInfo() != mergedLengthValue->GetValueInfo())
                {LOGMEIN("GlobOpt.cpp] 3819\n");
                    mergedLengthValue->SetValueInfo(
                        MergeLikelyIntValueInfo(
                            mergedLengthValue,
                            predecessorLengthValue,
                            mergedLengthValue->GetValueInfo()->Type().Merge(predecessorLengthValue->GetValueInfo()->Type())));
                }
            }
            else
            {
                SetValue(&successorBlockData, CopyValue(predecessorLengthValue), mergedLengthSym);
            }
        }

        if(compensated)
        {
            ChangeValueInfo(
                predecessor,
                predecessorValue,
                ArrayValueInfo::New(
                    alloc,
                    predecessorValueInfo->Type(),
                    mergedHeadSegmentSym ? mergedHeadSegmentSym : predecessorHeadSegmentSym,
                    mergedHeadSegmentLengthSym ? mergedHeadSegmentLengthSym : predecessorHeadSegmentLengthSym,
                    mergedLengthSym ? mergedLengthSym : predecessorLengthSym,
                    predecessorValueInfo->GetSymStore()),
                false /*allowIncompatibleType*/,
                compensated);
        }
    }

    if(setLastInstrInPredecessor)
    {LOGMEIN("GlobOpt.cpp] 3851\n");
        predecessor->SetLastInstr(insertBeforeInstr->m_prev);
    }
}

BOOLEAN
GlobOpt::IsArgumentsSymID(SymID id, const GlobOptBlockData& blockData)
{LOGMEIN("GlobOpt.cpp] 3858\n");
    return blockData.argObjSyms->Test(id);
}

BOOLEAN
GlobOpt::IsArgumentsOpnd(IR::Opnd* opnd)
{LOGMEIN("GlobOpt.cpp] 3864\n");
    SymID id = 0;
    if (opnd->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 3867\n");
        id = opnd->AsRegOpnd()->m_sym->m_id;
        return IsArgumentsSymID(id, this->blockData);
    }
    else if (opnd->IsSymOpnd())
    {LOGMEIN("GlobOpt.cpp] 3872\n");
        Sym *sym = opnd->AsSymOpnd()->m_sym;
        if (sym && sym->IsPropertySym())
        {LOGMEIN("GlobOpt.cpp] 3875\n");
            PropertySym *propertySym = sym->AsPropertySym();
            id = propertySym->m_stackSym->m_id;
            return IsArgumentsSymID(id, this->blockData);
        }
        return false;
    }
    else if (opnd->IsIndirOpnd())
    {LOGMEIN("GlobOpt.cpp] 3883\n");
        IR::RegOpnd *indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
        IR::RegOpnd *baseOpnd = opnd->AsIndirOpnd()->GetBaseOpnd();
        return IsArgumentsSymID(baseOpnd->m_sym->m_id, this->blockData) || (indexOpnd && IsArgumentsSymID(indexOpnd->m_sym->m_id, this->blockData));
    }
    AssertMsg(false, "Unknown type");
    return false;
}

void
GlobOpt::TrackArgumentsSym(IR::RegOpnd* opnd)
{LOGMEIN("GlobOpt.cpp] 3894\n");
    if(!blockData.curFunc->argObjSyms)
    {LOGMEIN("GlobOpt.cpp] 3896\n");
        blockData.curFunc->argObjSyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
    }
    blockData.curFunc->argObjSyms->Set(opnd->m_sym->m_id);
    blockData.argObjSyms->Set(opnd->m_sym->m_id);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (PHASE_TESTTRACE(Js::StackArgOptPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 3904\n");
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(_u("Created a new alias s%d for arguments object in function %s(%s) topFunc %s(%s)\n"),
            opnd->m_sym->m_id,
            blockData.curFunc->GetJITFunctionBody()->GetDisplayName(),
            blockData.curFunc->GetDebugNumberSet(debugStringBuffer),
            this->func->GetJITFunctionBody()->GetDisplayName(),
            this->func->GetDebugNumberSet(debugStringBuffer2)
            );
        Output::Flush();
    }
#endif
}

void
GlobOpt::ClearArgumentsSym(IR::RegOpnd* opnd)
{LOGMEIN("GlobOpt.cpp] 3921\n");
    // We blindly clear so need to check func has argObjSyms
    if (blockData.curFunc->argObjSyms)
    {LOGMEIN("GlobOpt.cpp] 3924\n");
        blockData.curFunc->argObjSyms->Clear(opnd->m_sym->m_id);
    }
    blockData.argObjSyms->Clear(opnd->m_sym->m_id);
}

bool
GlobOpt::AreFromSameBytecodeFunc(IR::RegOpnd* src1, IR::RegOpnd* dst)
{LOGMEIN("GlobOpt.cpp] 3932\n");
    Assert(this->func->m_symTable->FindStackSym(src1->m_sym->m_id) == src1->m_sym);
    Assert(this->func->m_symTable->FindStackSym(dst->m_sym->m_id) == dst->m_sym);
    if (dst->m_sym->HasByteCodeRegSlot() && src1->m_sym->HasByteCodeRegSlot())
    {LOGMEIN("GlobOpt.cpp] 3936\n");
        return src1->m_sym->GetByteCodeFunc() == dst->m_sym->GetByteCodeFunc();
    }
    return false;
}

BOOLEAN
GlobOpt::TestAnyArgumentsSym()
{LOGMEIN("GlobOpt.cpp] 3944\n");
    return blockData.argObjSyms->TestEmpty();
}

/*
*   This is for scope object removal along with Heap Arguments optimization.
*   We track several instructions to facilitate the removal of scope object.
*   - LdSlotArr - This instr is tracked to keep track of the formals array (the dest)
*   - InlineeStart - To keep track of the stack syms for the formals of the inlinee.
*/
void
GlobOpt::TrackInstrsForScopeObjectRemoval(IR::Instr * instr)
{LOGMEIN("GlobOpt.cpp] 3956\n");
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src1 = instr->GetSrc1();

    if (instr->m_opcode == Js::OpCode::Ld_A && src1->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 3961\n");
        AssertMsg(!instr->m_func->IsStackArgsEnabled() || !src1->IsScopeObjOpnd(instr->m_func), "There can be no aliasing for scope object.");
    }

    // The following is to track formals array for Stack Arguments optimization with Formals
    if (instr->m_func->IsStackArgsEnabled() && !this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 3967\n");
        if (instr->m_opcode == Js::OpCode::LdSlotArr)
        {LOGMEIN("GlobOpt.cpp] 3969\n");
            if (instr->GetSrc1()->IsScopeObjOpnd(instr->m_func))
            {LOGMEIN("GlobOpt.cpp] 3971\n");
                AssertMsg(!instr->m_func->GetJITFunctionBody()->HasImplicitArgIns(), "No mapping is required in this case. So it should already be generating ArgIns.");
                instr->m_func->TrackFormalsArraySym(dst->GetStackSym()->m_id);
            }
        }
        else if (instr->m_opcode == Js::OpCode::InlineeStart)
        {LOGMEIN("GlobOpt.cpp] 3977\n");
            Assert(instr->m_func->IsInlined());
            Js::ArgSlot actualsCount = instr->m_func->actualCount - 1;
            Js::ArgSlot formalsCount = instr->m_func->GetJITFunctionBody()->GetInParamsCount() - 1;

            Func * func = instr->m_func;
            Func * inlinerFunc = func->GetParentFunc(); //Inliner's func

            IR::Instr * argOutInstr = instr->GetSrc2()->GetStackSym()->GetInstrDef();

            //The argout immediately before the InlineeStart will be the ArgOut for NewScObject
            //So we don't want to track the stack sym for this argout.- Skipping it here.
            if (instr->m_func->IsInlinedConstructor())
            {LOGMEIN("GlobOpt.cpp] 3990\n");
                //PRE might introduce a second defintion for the Src1. So assert for the opcode only when it has single definition.
                Assert(argOutInstr->GetSrc1()->GetStackSym()->GetInstrDef() == nullptr ||
                    argOutInstr->GetSrc1()->GetStackSym()->GetInstrDef()->m_opcode == Js::OpCode::NewScObjectNoCtor);
                argOutInstr = argOutInstr->GetSrc2()->GetStackSym()->GetInstrDef();
            }
            if (formalsCount < actualsCount)
            {LOGMEIN("GlobOpt.cpp] 3997\n");
                Js::ArgSlot extraActuals = actualsCount - formalsCount;

                //Skipping extra actuals passed
                for (Js::ArgSlot i = 0; i < extraActuals; i++)
                {LOGMEIN("GlobOpt.cpp] 4002\n");
                    argOutInstr = argOutInstr->GetSrc2()->GetStackSym()->GetInstrDef();
                }
            }

            StackSym * undefinedSym = nullptr;

            for (Js::ArgSlot param = formalsCount; param > 0; param--)
            {LOGMEIN("GlobOpt.cpp] 4010\n");
                StackSym * argOutSym = nullptr;

                if (argOutInstr->GetSrc1())
                {LOGMEIN("GlobOpt.cpp] 4014\n");
                    if (argOutInstr->GetSrc1()->IsRegOpnd())
                    {LOGMEIN("GlobOpt.cpp] 4016\n");
                        argOutSym = argOutInstr->GetSrc1()->GetStackSym();
                    }
                    else
                    {
                        // We will always have ArgOut instr - so the source operand will not be removed.
                        argOutSym = StackSym::New(inlinerFunc);
                        IR::Opnd * srcOpnd = argOutInstr->GetSrc1();
                        IR::Opnd * dstOpnd = IR::RegOpnd::New(argOutSym, TyVar, inlinerFunc);
                        IR::Instr * assignInstr = IR::Instr::New(Js::OpCode::Ld_A, dstOpnd, srcOpnd, inlinerFunc);
                        instr->InsertBefore(assignInstr);
                    }
                }

                Assert(!func->HasStackSymForFormal(param - 1));

                if (param <= actualsCount)
                {LOGMEIN("GlobOpt.cpp] 4033\n");
                    Assert(argOutSym);
                    func->TrackStackSymForFormalIndex(param - 1, argOutSym);
                    argOutInstr = argOutInstr->GetSrc2()->GetStackSym()->GetInstrDef();
                }
                else
                {
                    /*When param is out of range of actuals count, load undefined*/
                    // TODO: saravind: This will insert undefined for each of the param not having an actual. - Clean up this by having a sym for undefined on func ?
                    Assert(formalsCount > actualsCount);
                    if (undefinedSym == nullptr)
                    {LOGMEIN("GlobOpt.cpp] 4044\n");
                        undefinedSym = StackSym::New(inlinerFunc);
                        IR::Opnd * srcOpnd = IR::AddrOpnd::New(inlinerFunc->GetScriptContextInfo()->GetUndefinedAddr(), IR::AddrOpndKindDynamicMisc, inlinerFunc);
                        IR::Opnd * dstOpnd = IR::RegOpnd::New(undefinedSym, TyVar, inlinerFunc);
                        IR::Instr * assignUndefined = IR::Instr::New(Js::OpCode::Ld_A, dstOpnd, srcOpnd, inlinerFunc);
                        instr->InsertBefore(assignUndefined);
                    }
                    func->TrackStackSymForFormalIndex(param - 1, undefinedSym);
                }
            }
        }
    }
}

void
GlobOpt::OptArguments(IR::Instr *instr)
{LOGMEIN("GlobOpt.cpp] 4060\n");
    IR::Opnd* dst = instr->GetDst();
    IR::Opnd* src1 = instr->GetSrc1();
    IR::Opnd* src2 = instr->GetSrc2();

    TrackInstrsForScopeObjectRemoval(instr);

    if (!TrackArgumentsObject())
    {LOGMEIN("GlobOpt.cpp] 4068\n");
        return;
    }

    if (instr->HasAnyLoadHeapArgsOpCode())
    {LOGMEIN("GlobOpt.cpp] 4073\n");
        if (instr->m_func->IsStackArgsEnabled())
        {LOGMEIN("GlobOpt.cpp] 4075\n");
            if (instr->GetSrc1()->IsRegOpnd() && instr->m_func->GetJITFunctionBody()->GetInParamsCount() > 1)
            {LOGMEIN("GlobOpt.cpp] 4077\n");
                StackSym * scopeObjSym = instr->GetSrc1()->GetStackSym();
                Assert(scopeObjSym);
                Assert(scopeObjSym->GetInstrDef()->m_opcode == Js::OpCode::InitCachedScope || scopeObjSym->GetInstrDef()->m_opcode == Js::OpCode::NewScopeObject);
                Assert(instr->m_func->GetScopeObjSym() == scopeObjSym);

                if (PHASE_VERBOSE_TRACE1(Js::StackArgFormalsOptPhase))
                {LOGMEIN("GlobOpt.cpp] 4084\n");
                    Output::Print(_u("StackArgFormals : %s (%d) :Setting scopeObjSym in forward pass. \n"), instr->m_func->GetJITFunctionBody()->GetDisplayName(), instr->m_func->GetJITFunctionBody()->GetFunctionNumber());
                    Output::Flush();
                }
            }
        }

        if (instr->m_func->GetJITFunctionBody()->GetInParamsCount() != 1 && !instr->m_func->IsStackArgsEnabled())
        {LOGMEIN("GlobOpt.cpp] 4092\n");
            CannotAllocateArgumentsObjectOnStack();
        }
        else
        {
            TrackArgumentsSym(dst->AsRegOpnd());
        }
        return;
    }
    // Keep track of arguments objects and its aliases
    // LdHeapArguments loads the arguments object and Ld_A tracks the aliases.
    if ((instr->m_opcode == Js::OpCode::Ld_A || instr->m_opcode == Js::OpCode::BytecodeArgOutCapture) && (src1->IsRegOpnd() && IsArgumentsOpnd(src1)))
    {LOGMEIN("GlobOpt.cpp] 4104\n");
        // In the debug mode, we don't want to optimize away the aliases. Since we may have to show them on the inspection.
        if (((!AreFromSameBytecodeFunc(src1->AsRegOpnd(), dst->AsRegOpnd()) || this->currentBlock->loop) && instr->m_opcode != Js::OpCode::BytecodeArgOutCapture) || this->func->IsJitInDebugMode())
        {LOGMEIN("GlobOpt.cpp] 4107\n");
            CannotAllocateArgumentsObjectOnStack();
            return;
        }
        if(!dst->AsRegOpnd()->GetStackSym()->m_nonEscapingArgObjAlias)
        {LOGMEIN("GlobOpt.cpp] 4112\n");
            TrackArgumentsSym(dst->AsRegOpnd());
        }
        return;
    }

    if (!TestAnyArgumentsSym())
    {LOGMEIN("GlobOpt.cpp] 4119\n");
        // There are no syms to track yet, don't start tracking arguments sym.
        return;
    }

    // Avoid loop prepass
    if (this->currentBlock->loop && this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 4126\n");
        return;
    }

    SymID id = 0;

    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 4133\n");
    case Js::OpCode::LdElemI_A:
    case Js::OpCode::TypeofElem:
    {LOGMEIN("GlobOpt.cpp] 4136\n");
        Assert(src1->IsIndirOpnd());
        IR::RegOpnd *indexOpnd = src1->AsIndirOpnd()->GetIndexOpnd();
        if (indexOpnd && IsArgumentsSymID(indexOpnd->m_sym->m_id, this->blockData))
        {LOGMEIN("GlobOpt.cpp] 4140\n");
            // Pathological test cases such as a[arguments]
            CannotAllocateArgumentsObjectOnStack();
            return;
        }

        IR::RegOpnd *baseOpnd = src1->AsIndirOpnd()->GetBaseOpnd();
        id = baseOpnd->m_sym->m_id;
        if (IsArgumentsSymID(id, this->blockData))
        {LOGMEIN("GlobOpt.cpp] 4149\n");
            instr->usesStackArgumentsObject = true;
        }

        break;
    }
    case Js::OpCode::LdLen_A:
    {LOGMEIN("GlobOpt.cpp] 4156\n");
        Assert(src1->IsRegOpnd());
        if(IsArgumentsOpnd(src1))
        {LOGMEIN("GlobOpt.cpp] 4159\n");
            instr->usesStackArgumentsObject = true;
        }
        break;
    }
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
    {LOGMEIN("GlobOpt.cpp] 4165\n");
        if (IsArgumentsOpnd(src1))
        {LOGMEIN("GlobOpt.cpp] 4167\n");
            instr->usesStackArgumentsObject = true;
        }

        if (IsArgumentsOpnd(src1) &&
            src1->AsRegOpnd()->m_sym->GetInstrDef()->m_opcode == Js::OpCode::BytecodeArgOutCapture)
        {LOGMEIN("GlobOpt.cpp] 4173\n");
            // Apply inlining results in such usage - this is to ignore this sym that is def'd by ByteCodeArgOutCapture
            // It's needed because we do not have block level merging of arguments object and this def due to inlining can turn off stack args opt.
            IR::Instr* builtinStart = instr->GetNextRealInstr();
            if (builtinStart->m_opcode == Js::OpCode::InlineBuiltInStart)
            {LOGMEIN("GlobOpt.cpp] 4178\n");
                IR::Opnd* builtinOpnd = builtinStart->GetSrc1();
                if (builtinStart->GetSrc1()->IsAddrOpnd())
                {LOGMEIN("GlobOpt.cpp] 4181\n");
                    Assert(builtinOpnd->AsAddrOpnd()->m_isFunction);

                    Js::BuiltinFunction builtinFunction = Js::JavascriptLibrary::GetBuiltInForFuncInfo(((JITTimeFixedField*)builtinOpnd->AsAddrOpnd()->m_metadata)->GetFuncInfoAddr(), func->GetThreadContextInfo());
                    if (builtinFunction == Js::BuiltinFunction::JavascriptFunction_Apply)
                    {LOGMEIN("GlobOpt.cpp] 4186\n");
                        ClearArgumentsSym(src1->AsRegOpnd());
                    }
                }
                else if (builtinOpnd->IsRegOpnd())
                {LOGMEIN("GlobOpt.cpp] 4191\n");
                    if (builtinOpnd->AsRegOpnd()->m_sym->m_builtInIndex == Js::BuiltinFunction::JavascriptFunction_Apply)
                    {LOGMEIN("GlobOpt.cpp] 4193\n");
                        ClearArgumentsSym(src1->AsRegOpnd());
                    }
                }
            }
        }
        break;
    }
    case Js::OpCode::BailOnNotStackArgs:
    case Js::OpCode::ArgOut_A_FromStackArgs:
    case Js::OpCode::BytecodeArgOutUse:
    {LOGMEIN("GlobOpt.cpp] 4204\n");
        if (src1 && IsArgumentsOpnd(src1))
        {LOGMEIN("GlobOpt.cpp] 4206\n");
            instr->usesStackArgumentsObject = true;
        }

        break;
    }

    default:
        {
            // Super conservative here, if we see the arguments or any of its alias being used in any
            // other opcode just don't do this optimization. Revisit this to optimize further if we see any common
            // case is missed.

            if (src1)
            {LOGMEIN("GlobOpt.cpp] 4220\n");
                if (src1->IsRegOpnd() || src1->IsSymOpnd() || src1->IsIndirOpnd())
                {LOGMEIN("GlobOpt.cpp] 4222\n");
                    if (IsArgumentsOpnd(src1))
                    {LOGMEIN("GlobOpt.cpp] 4224\n");
#ifdef PERF_HINT
                        if (PHASE_TRACE1(Js::PerfHintPhase))
                        {LOGMEIN("GlobOpt.cpp] 4227\n");
                            WritePerfHint(PerfHints::HeapArgumentsCreated, instr->m_func, instr->GetByteCodeOffset());
                        }
#endif
                        CannotAllocateArgumentsObjectOnStack();
                        return;
                    }
                }
            }

            if (src2)
            {LOGMEIN("GlobOpt.cpp] 4238\n");
                if (src2->IsRegOpnd() || src2->IsSymOpnd() || src2->IsIndirOpnd())
                {LOGMEIN("GlobOpt.cpp] 4240\n");
                    if (IsArgumentsOpnd(src2))
                    {LOGMEIN("GlobOpt.cpp] 4242\n");
#ifdef PERF_HINT
                        if (PHASE_TRACE1(Js::PerfHintPhase))
                        {LOGMEIN("GlobOpt.cpp] 4245\n");
                            WritePerfHint(PerfHints::HeapArgumentsCreated, instr->m_func, instr->GetByteCodeOffset());
                        }
#endif
                        CannotAllocateArgumentsObjectOnStack();
                        return;
                    }
                }
            }

            // We should look at dst last to correctly handle cases where it's the same as one of the src operands.
            if (dst)
            {LOGMEIN("GlobOpt.cpp] 4257\n");
                if (dst->IsIndirOpnd() || dst->IsSymOpnd())
                {LOGMEIN("GlobOpt.cpp] 4259\n");
                    if (IsArgumentsOpnd(dst))
                    {LOGMEIN("GlobOpt.cpp] 4261\n");
#ifdef PERF_HINT
                        if (PHASE_TRACE1(Js::PerfHintPhase))
                        {LOGMEIN("GlobOpt.cpp] 4264\n");
                            WritePerfHint(PerfHints::HeapArgumentsModification, instr->m_func, instr->GetByteCodeOffset());
                        }
#endif
                        CannotAllocateArgumentsObjectOnStack();
                        return;
                    }
                }
                else if (dst->IsRegOpnd())
                {LOGMEIN("GlobOpt.cpp] 4273\n");
                    if (this->currentBlock->loop && IsArgumentsOpnd(dst))
                    {LOGMEIN("GlobOpt.cpp] 4275\n");
#ifdef PERF_HINT
                        if (PHASE_TRACE1(Js::PerfHintPhase))
                        {LOGMEIN("GlobOpt.cpp] 4278\n");
                            WritePerfHint(PerfHints::HeapArgumentsModification, instr->m_func, instr->GetByteCodeOffset());
                        }
#endif
                        CannotAllocateArgumentsObjectOnStack();
                        return;
                    }
                    ClearArgumentsSym(dst->AsRegOpnd());
                }
            }
        }
        break;
    }
    return;
}

void
GlobOpt::MarkArgumentsUsedForBranch(IR::Instr * instr)
{LOGMEIN("GlobOpt.cpp] 4296\n");
    // If it's a conditional branch instruction and the operand used for branching is one of the arguments
    // to the function, tag the m_argUsedForBranch of the functionBody so that it can be used later for inlining decisions.
    if (instr->IsBranchInstr() && !instr->AsBranchInstr()->IsUnconditional())
    {LOGMEIN("GlobOpt.cpp] 4300\n");
        IR::BranchInstr * bInstr = instr->AsBranchInstr();
        IR::Opnd *src1 = bInstr->GetSrc1();
        IR::Opnd *src2 = bInstr->GetSrc2();
        // These are used because we don't want to rely on src1 or src2 to always be the register/constant
        IR::RegOpnd *regOpnd = nullptr;
        if (!src2 && (instr->m_opcode == Js::OpCode::BrFalse_A || instr->m_opcode == Js::OpCode::BrTrue_A) && src1->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 4307\n");
            regOpnd = src1->AsRegOpnd();
        }
        // We need to check for (0===arg) and (arg===0); this is especially important since some minifiers
        // change all instances of one to the other.
        else if (src2 && src2->IsConstOpnd() && src1->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 4313\n");
            regOpnd = src1->AsRegOpnd();
        }
        else if (src2 && src2->IsRegOpnd() && src1->IsConstOpnd())
        {LOGMEIN("GlobOpt.cpp] 4317\n");
            regOpnd = src2->AsRegOpnd();
        }
        if (regOpnd != nullptr)
        {LOGMEIN("GlobOpt.cpp] 4321\n");
            if (regOpnd->m_sym->IsSingleDef())
            {LOGMEIN("GlobOpt.cpp] 4323\n");
                IR::Instr * defInst = regOpnd->m_sym->GetInstrDef();
                IR::Opnd *defSym = defInst->GetSrc1();
                if (defSym && defSym->IsSymOpnd() && defSym->AsSymOpnd()->m_sym->IsStackSym()
                    && defSym->AsSymOpnd()->m_sym->AsStackSym()->IsParamSlotSym())
                {LOGMEIN("GlobOpt.cpp] 4328\n");
                    uint16 param = defSym->AsSymOpnd()->m_sym->AsStackSym()->GetParamSlotNum();

                    // We only support functions with 13 arguments to ensure optimal size of callSiteInfo
                    if (param < Js::Constants::MaximumArgumentCountForConstantArgumentInlining)
                    {LOGMEIN("GlobOpt.cpp] 4333\n");
                        this->func->GetJITOutput()->SetArgUsedForBranch((uint8)param);
                    }
                }
            }
        }
    }
}

const InductionVariable*
GlobOpt::GetInductionVariable(SymID sym, Loop *loop)
{LOGMEIN("GlobOpt.cpp] 4344\n");
    if (loop->inductionVariables)
    {LOGMEIN("GlobOpt.cpp] 4346\n");
        for (auto it = loop->inductionVariables->GetIterator(); it.IsValid(); it.MoveNext())
        {LOGMEIN("GlobOpt.cpp] 4348\n");
            InductionVariable* iv = &it.CurrentValueReference();
            if (!iv->IsChangeDeterminate() || !iv->IsChangeUnidirectional())
            {LOGMEIN("GlobOpt.cpp] 4351\n");
                continue;
            }
            if (iv->Sym()->m_id == sym)
            {LOGMEIN("GlobOpt.cpp] 4355\n");
                return iv;
            }
        }
    }
    return nullptr;
}

bool
GlobOpt::IsSymIDInductionVariable(SymID sym, Loop *loop)
{LOGMEIN("GlobOpt.cpp] 4365\n");
    return GetInductionVariable(sym, loop) != nullptr;
}

SymID
GlobOpt::GetVarSymID(StackSym *sym)
{LOGMEIN("GlobOpt.cpp] 4371\n");
    if (sym && sym->m_type != TyVar)
    {LOGMEIN("GlobOpt.cpp] 4373\n");
        sym = sym->GetVarEquivSym(nullptr);
    }
    if (!sym)
    {LOGMEIN("GlobOpt.cpp] 4377\n");
        return Js::Constants::InvalidSymID;
    }
    return sym->m_id;
}

bool
GlobOpt::IsAllowedForMemOpt(IR::Instr* instr, bool isMemset, IR::RegOpnd *baseOpnd, IR::Opnd *indexOpnd)
{LOGMEIN("GlobOpt.cpp] 4385\n");
    Assert(instr);
    if (!baseOpnd || !indexOpnd)
    {LOGMEIN("GlobOpt.cpp] 4388\n");
        return false;
    }
    Loop* loop = this->currentBlock->loop;

    const ValueType baseValueType(baseOpnd->GetValueType());
    const ValueType indexValueType(indexOpnd->GetValueType());

    // Validate the array and index types
    if (
        !indexValueType.IsInt() ||
            !(
                baseValueType.IsTypedIntOrFloatArray() ||
                baseValueType.IsArray()
            )
        )
    {LOGMEIN("GlobOpt.cpp] 4404\n");
#if DBG_DUMP
        wchar indexValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
        indexValueType.ToString(indexValueTypeStr);
        wchar baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
        baseValueType.ToString(baseValueTypeStr);
        TRACE_MEMOP_VERBOSE(loop, instr, _u("Index[%s] or Array[%s] value type is invalid"), indexValueTypeStr, baseValueTypeStr);
#endif
        return false;
    }

    // The following is conservative and works around a bug in induction variable analysis.
    if (baseOpnd->IsArrayRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 4417\n");
        IR::ArrayRegOpnd *baseArrayOp = baseOpnd->AsArrayRegOpnd();
        bool hasBoundChecksRemoved = (
            baseArrayOp->EliminatedLowerBoundCheck() &&
            baseArrayOp->EliminatedUpperBoundCheck() &&
            !instr->extractedUpperBoundCheckWithoutHoisting &&
            !instr->loadedArrayHeadSegment &&
            !instr->loadedArrayHeadSegmentLength
            );
        if (!hasBoundChecksRemoved)
        {
            TRACE_MEMOP_VERBOSE(loop, instr, _u("Missing bounds check optimization"));
            return false;
        }
    }

    if (!baseValueType.IsTypedArray())
    {LOGMEIN("GlobOpt.cpp] 4434\n");
        // Check if the instr can kill the value type of the array
        JsArrayKills arrayKills = CheckJsArrayKills(instr);
        if (arrayKills.KillsValueType(baseValueType))
        {
            TRACE_MEMOP_VERBOSE(loop, instr, _u("The array (s%d) can lose its value type"), GetVarSymID(baseOpnd->GetStackSym()));
            return false;
        }
    }

    // Process the Index Operand
    if (!this->OptIsInvariant(baseOpnd, this->currentBlock, loop, this->FindValue(baseOpnd->m_sym), false, true))
    {
        TRACE_MEMOP_VERBOSE(loop, instr, _u("Base (s%d) is not invariant"), GetVarSymID(baseOpnd->GetStackSym()));
        return false;
    }

    // Validate the index
    Assert(indexOpnd->GetStackSym());
    SymID indexSymID = GetVarSymID(indexOpnd->GetStackSym());
    const InductionVariable* iv = GetInductionVariable(indexSymID, loop);
    if (!iv)
    {
        // If the index is not an induction variable return
        TRACE_MEMOP_VERBOSE(loop, instr, _u("Index (s%d) is not an induction variable"), indexSymID);
        return false;
    }

    Assert(iv->IsChangeDeterminate() && iv->IsChangeUnidirectional());
    const IntConstantBounds & bounds = iv->ChangeBounds();

    if (loop->memOpInfo)
    {LOGMEIN("GlobOpt.cpp] 4466\n");
        // Only accept induction variables that increments by 1
        Loop::InductionVariableChangeInfo inductionVariableChangeInfo = { 0, 0 };
        inductionVariableChangeInfo = loop->memOpInfo->inductionVariableChangeInfoMap->Lookup(indexSymID, inductionVariableChangeInfo);

        if (
            (bounds.LowerBound() != 1 && bounds.LowerBound() != -1) ||
            (bounds.UpperBound() != bounds.LowerBound()) ||
            inductionVariableChangeInfo.unroll > 1 // Must be 0 (not seen yet) or 1 (already seen)
        )
        {
            TRACE_MEMOP_VERBOSE(loop, instr, _u("The index does not change by 1: %d><%d, unroll=%d"), bounds.LowerBound(), bounds.UpperBound(), inductionVariableChangeInfo.unroll);
            return false;
        }

        // Check if the index is the same in all MemOp optimization in this loop
        if (!loop->memOpInfo->candidates->Empty())
        {LOGMEIN("GlobOpt.cpp] 4483\n");
            Loop::MemOpCandidate* previousCandidate = loop->memOpInfo->candidates->Head();

            // All MemOp operations within the same loop must use the same index
            if (previousCandidate->index != indexSymID)
            {
                TRACE_MEMOP_VERBOSE(loop, instr, _u("The index is not the same as other MemOp in the loop"));
                return false;
            }
        }
    }

    return true;
}

bool
GlobOpt::CollectMemcopyLdElementI(IR::Instr *instr, Loop *loop)
{LOGMEIN("GlobOpt.cpp] 4500\n");
    Assert(instr->GetSrc1()->IsIndirOpnd());

    IR::IndirOpnd *src1 = instr->GetSrc1()->AsIndirOpnd();
    IR::Opnd *indexOpnd = src1->GetIndexOpnd();
    IR::RegOpnd *baseOpnd = src1->GetBaseOpnd()->AsRegOpnd();
    SymID baseSymID = GetVarSymID(baseOpnd->GetStackSym());

    if (!IsAllowedForMemOpt(instr, false, baseOpnd, indexOpnd))
    {LOGMEIN("GlobOpt.cpp] 4509\n");
        return false;
    }

    SymID inductionSymID = GetVarSymID(indexOpnd->GetStackSym());
    Assert(IsSymIDInductionVariable(inductionSymID, loop));

    loop->EnsureMemOpVariablesInitialized();
    bool isIndexPreIncr = loop->memOpInfo->inductionVariableChangeInfoMap->ContainsKey(inductionSymID);

    IR::Opnd * dst = instr->GetDst();
    if (!dst->IsRegOpnd() || !dst->AsRegOpnd()->GetStackSym()->IsSingleDef())
    {LOGMEIN("GlobOpt.cpp] 4521\n");
        return false;
    }

    Loop::MemCopyCandidate* memcopyInfo = memcopyInfo = JitAnewStruct(this->func->GetTopFunc()->m_fg->alloc, Loop::MemCopyCandidate);
    memcopyInfo->ldBase = baseSymID;
    memcopyInfo->ldCount = 1;
    memcopyInfo->count = 0;
    memcopyInfo->bIndexAlreadyChanged = isIndexPreIncr;
    memcopyInfo->base = Js::Constants::InvalidSymID; //need to find the stElem first
    memcopyInfo->index = inductionSymID;
    memcopyInfo->transferSym = dst->AsRegOpnd()->GetStackSym();
    loop->memOpInfo->candidates->Prepend(memcopyInfo);
    return true;
}

bool
GlobOpt::CollectMemsetStElementI(IR::Instr *instr, Loop *loop)
{LOGMEIN("GlobOpt.cpp] 4539\n");
    Assert(instr->GetDst()->IsIndirOpnd());
    IR::IndirOpnd *dst = instr->GetDst()->AsIndirOpnd();
    IR::Opnd *indexOp = dst->GetIndexOpnd();
    IR::RegOpnd *baseOp = dst->GetBaseOpnd()->AsRegOpnd();

    if (!IsAllowedForMemOpt(instr, true, baseOp, indexOp))
    {LOGMEIN("GlobOpt.cpp] 4546\n");
        return false;
    }

    SymID baseSymID = GetVarSymID(baseOp->GetStackSym());

    IR::Opnd *srcDef = instr->GetSrc1();
    StackSym *srcSym = nullptr;
    if (srcDef->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 4555\n");
        IR::RegOpnd* opnd = srcDef->AsRegOpnd();
        if (this->OptIsInvariant(opnd, this->currentBlock, loop, this->FindValue(opnd->m_sym), true, true))
        {LOGMEIN("GlobOpt.cpp] 4558\n");
            srcSym = opnd->GetStackSym();
        }
    }

    BailoutConstantValue constant = {TyIllegal, 0};
    if (srcDef->IsFloatConstOpnd())
    {LOGMEIN("GlobOpt.cpp] 4565\n");
        constant.InitFloatConstValue(srcDef->AsFloatConstOpnd()->m_value);
    }
    else if (srcDef->IsIntConstOpnd())
    {LOGMEIN("GlobOpt.cpp] 4569\n");
        constant.InitIntConstValue(srcDef->AsIntConstOpnd()->GetValue(), srcDef->AsIntConstOpnd()->GetType());
    }
    else if (srcDef->IsAddrOpnd())
    {LOGMEIN("GlobOpt.cpp] 4573\n");
        constant.InitVarConstValue(srcDef->AsAddrOpnd()->m_address);
    }
    else if(!srcSym)
    {
        TRACE_MEMOP_PHASE_VERBOSE(MemSet, loop, instr, _u("Source is not an invariant"));
        return false;
    }

    // Process the Index Operand
    Assert(indexOp->GetStackSym());
    SymID inductionSymID = GetVarSymID(indexOp->GetStackSym());
    Assert(IsSymIDInductionVariable(inductionSymID, loop));

    loop->EnsureMemOpVariablesInitialized();
    bool isIndexPreIncr = loop->memOpInfo->inductionVariableChangeInfoMap->ContainsKey(inductionSymID);

    Loop::MemSetCandidate* memsetInfo = JitAnewStruct(this->func->GetTopFunc()->m_fg->alloc, Loop::MemSetCandidate);
    memsetInfo->base = baseSymID;
    memsetInfo->index = inductionSymID;
    memsetInfo->constant = constant;
    memsetInfo->srcSym = srcSym;
    memsetInfo->count = 1;
    memsetInfo->bIndexAlreadyChanged = isIndexPreIncr;
    loop->memOpInfo->candidates->Prepend(memsetInfo);
    return true;
}

bool GlobOpt::CollectMemcopyStElementI(IR::Instr *instr, Loop *loop)
{LOGMEIN("GlobOpt.cpp] 4602\n");
    if (!loop->memOpInfo || loop->memOpInfo->candidates->Empty())
    {LOGMEIN("GlobOpt.cpp] 4604\n");
        // There is no ldElem matching this stElem
        return false;
    }

    Assert(instr->GetDst()->IsIndirOpnd());
    IR::IndirOpnd *dst = instr->GetDst()->AsIndirOpnd();
    IR::Opnd *indexOp = dst->GetIndexOpnd();
    IR::RegOpnd *baseOp = dst->GetBaseOpnd()->AsRegOpnd();
    SymID baseSymID = GetVarSymID(baseOp->GetStackSym());

    if (!instr->GetSrc1()->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 4616\n");
        return false;
    }
    IR::RegOpnd* src1 = instr->GetSrc1()->AsRegOpnd();

    if (!src1->GetIsDead())
    {
        // This must be the last use of the register.
        // It will invalidate `var m = a[i]; b[i] = m;` but this is not a very interesting case.
        TRACE_MEMOP_PHASE_VERBOSE(MemCopy, loop, instr, _u("Source (s%d) is still alive after StElemI"), baseSymID);
        return false;
    }

    if (!IsAllowedForMemOpt(instr, false, baseOp, indexOp))
    {LOGMEIN("GlobOpt.cpp] 4630\n");
        return false;
    }

    SymID srcSymID = GetVarSymID(src1->GetStackSym());

    // Prepare the memcopyCandidate entry
    Loop::MemOpCandidate* previousCandidate = loop->memOpInfo->candidates->Head();
    if (!previousCandidate->IsMemCopy())
    {LOGMEIN("GlobOpt.cpp] 4639\n");
        return false;
    }
    Loop::MemCopyCandidate* memcopyInfo = previousCandidate->AsMemCopy();

    // The previous candidate has to have been created by the matching ldElem
    if (
        memcopyInfo->base != Js::Constants::InvalidSymID ||
        GetVarSymID(memcopyInfo->transferSym) != srcSymID
    )
    {
        TRACE_MEMOP_PHASE_VERBOSE(MemCopy, loop, instr, _u("No matching LdElem found (s%d)"), baseSymID);
        return false;
    }

    Assert(indexOp->GetStackSym());
    SymID inductionSymID = GetVarSymID(indexOp->GetStackSym());
    Assert(IsSymIDInductionVariable(inductionSymID, loop));
    bool isIndexPreIncr = loop->memOpInfo->inductionVariableChangeInfoMap->ContainsKey(inductionSymID);
    if (isIndexPreIncr != memcopyInfo->bIndexAlreadyChanged)
    {
        // The index changed between the load and the store
        TRACE_MEMOP_PHASE_VERBOSE(MemCopy, loop, instr, _u("Index value changed between ldElem and stElem"));
        return false;
    }

    // Consider: Can we remove the count field?
    memcopyInfo->count++;
    memcopyInfo->base = baseSymID;

    return true;
}

bool
GlobOpt::CollectMemOpLdElementI(IR::Instr *instr, Loop *loop)
{LOGMEIN("GlobOpt.cpp] 4674\n");
    Assert(instr->m_opcode == Js::OpCode::LdElemI_A);
    return (!PHASE_OFF(Js::MemCopyPhase, this->func) && CollectMemcopyLdElementI(instr, loop));
}

bool
GlobOpt::CollectMemOpStElementI(IR::Instr *instr, Loop *loop)
{LOGMEIN("GlobOpt.cpp] 4681\n");
    Assert(instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict);
    Assert(instr->GetSrc1());
    return (!PHASE_OFF(Js::MemSetPhase, this->func) && CollectMemsetStElementI(instr, loop)) ||
        (!PHASE_OFF(Js::MemCopyPhase, this->func) && CollectMemcopyStElementI(instr, loop));
}

bool
GlobOpt::CollectMemOpInfo(IR::Instr *instr, Value *src1Val, Value *src2Val)
{LOGMEIN("GlobOpt.cpp] 4690\n");
    Assert(this->currentBlock->loop);

    Loop *loop = this->currentBlock->loop;

    if (!loop->blockList.HasTwo())
    {LOGMEIN("GlobOpt.cpp] 4696\n");
        //  We support memcopy and memset for loops which have only two blocks.
        return false;
    }

    if (loop->GetLoopFlags().isInterpreted && !loop->GetLoopFlags().memopMinCountReached)
    {
        TRACE_MEMOP_VERBOSE(loop, instr, _u("minimum loop count not reached"))
        loop->doMemOp = false;
        return false;
    }
    Assert(loop->doMemOp);

    bool isIncr = true, isChangedByOne = false;
    switch (instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 4711\n");
    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
        if (!CollectMemOpStElementI(instr, loop))
        {LOGMEIN("GlobOpt.cpp] 4715\n");
            loop->doMemOp = false;
            return false;
        }
        break;
    case Js::OpCode::LdElemI_A:
        if (!CollectMemOpLdElementI(instr, loop))
        {LOGMEIN("GlobOpt.cpp] 4722\n");
            loop->doMemOp = false;
            return false;
        }
        break;
    case Js::OpCode::Decr_A:
        isIncr = false;
    case Js::OpCode::Incr_A:
        isChangedByOne = true;
        goto MemOpCheckInductionVariable;
    case Js::OpCode::Sub_I4:
    case Js::OpCode::Sub_A:
        isIncr = false;
    case Js::OpCode::Add_A:
    case Js::OpCode::Add_I4:
    {LOGMEIN("GlobOpt.cpp] 4737\n");
MemOpCheckInductionVariable:
        StackSym *sym = instr->GetSrc1()->GetStackSym();
        if (!sym)
        {LOGMEIN("GlobOpt.cpp] 4741\n");
            sym = instr->GetSrc2()->GetStackSym();
        }

        SymID inductionSymID = GetVarSymID(sym);

        if (IsSymIDInductionVariable(inductionSymID, this->currentBlock->loop))
        {LOGMEIN("GlobOpt.cpp] 4748\n");
            if (!isChangedByOne)
            {LOGMEIN("GlobOpt.cpp] 4750\n");
                IR::Opnd *src1, *src2;
                src1 = instr->GetSrc1();
                src2 = instr->GetSrc2();

                if (src2->IsRegOpnd())
                {LOGMEIN("GlobOpt.cpp] 4756\n");
                    Value *val = this->FindValue(src2->AsRegOpnd()->m_sym);
                    if (val)
                    {LOGMEIN("GlobOpt.cpp] 4759\n");
                        ValueInfo *vi = val->GetValueInfo();
                        int constValue;
                        if (vi && vi->TryGetIntConstantValue(&constValue))
                        {LOGMEIN("GlobOpt.cpp] 4763\n");
                            if (constValue == 1)
                            {LOGMEIN("GlobOpt.cpp] 4765\n");
                                isChangedByOne = true;
                            }
                        }
                    }
                }
                else if (src2->IsIntConstOpnd())
                {LOGMEIN("GlobOpt.cpp] 4772\n");
                    if (src2->AsIntConstOpnd()->GetValue() == 1)
                    {LOGMEIN("GlobOpt.cpp] 4774\n");
                        isChangedByOne = true;
                    }
                }
            }

            loop->EnsureMemOpVariablesInitialized();
            if (!isChangedByOne)
            {LOGMEIN("GlobOpt.cpp] 4782\n");
                Loop::InductionVariableChangeInfo inductionVariableChangeInfo = { Js::Constants::InvalidLoopUnrollFactor, 0 };

                if (!loop->memOpInfo->inductionVariableChangeInfoMap->ContainsKey(inductionSymID))
                {LOGMEIN("GlobOpt.cpp] 4786\n");
                    loop->memOpInfo->inductionVariableChangeInfoMap->Add(inductionSymID, inductionVariableChangeInfo);
                }
                else
                {
                    loop->memOpInfo->inductionVariableChangeInfoMap->Item(inductionSymID, inductionVariableChangeInfo);
                }
            }
            else
            {
                if (!loop->memOpInfo->inductionVariableChangeInfoMap->ContainsKey(inductionSymID))
                {LOGMEIN("GlobOpt.cpp] 4797\n");
                    Loop::InductionVariableChangeInfo inductionVariableChangeInfo = { 1, isIncr };
                    loop->memOpInfo->inductionVariableChangeInfoMap->Add(inductionSymID, inductionVariableChangeInfo);
                }
                else
                {
                    Loop::InductionVariableChangeInfo inductionVariableChangeInfo = { 0, 0 };
                    inductionVariableChangeInfo = loop->memOpInfo->inductionVariableChangeInfoMap->Lookup(inductionSymID, inductionVariableChangeInfo);
                    inductionVariableChangeInfo.unroll++;
                    inductionVariableChangeInfo.isIncremental = isIncr;
                    loop->memOpInfo->inductionVariableChangeInfoMap->Item(inductionSymID, inductionVariableChangeInfo);
                }
            }
            break;
        }
        // Fallthrough if not an induction variable
    }
    default:
        if (IsInstrInvalidForMemOp(instr, loop, src1Val, src2Val))
        {LOGMEIN("GlobOpt.cpp] 4816\n");
            loop->doMemOp = false;
            return false;
        }

        // Make sure this instruction doesn't use the memcopy transfer sym before it is checked by StElemI
        if (loop->memOpInfo && !loop->memOpInfo->candidates->Empty())
        {LOGMEIN("GlobOpt.cpp] 4823\n");
            Loop::MemOpCandidate* prevCandidate = loop->memOpInfo->candidates->Head();
            if (prevCandidate->IsMemCopy())
            {LOGMEIN("GlobOpt.cpp] 4826\n");
                Loop::MemCopyCandidate* memcopyCandidate = prevCandidate->AsMemCopy();
                if (memcopyCandidate->base == Js::Constants::InvalidSymID)
                {LOGMEIN("GlobOpt.cpp] 4829\n");
                    if (instr->FindRegUse(memcopyCandidate->transferSym))
                    {LOGMEIN("GlobOpt.cpp] 4831\n");
                        loop->doMemOp = false;
                        TRACE_MEMOP_PHASE_VERBOSE(MemCopy, loop, instr, _u("Found illegal use of LdElemI value(s%d)"), GetVarSymID(memcopyCandidate->transferSym));
                        return false;
                    }
                }
            }
        }
    }

    return true;
}

bool
GlobOpt::IsInstrInvalidForMemOp(IR::Instr *instr, Loop *loop, Value *src1Val, Value *src2Val)
{LOGMEIN("GlobOpt.cpp] 4846\n");
    // List of instruction that are valid with memop (ie: instr that gets removed if memop is emitted)
    if (
        this->currentBlock != loop->GetHeadBlock() &&
        !instr->IsLabelInstr() &&
        instr->IsRealInstr() &&
        instr->m_opcode != Js::OpCode::IncrLoopBodyCount &&
        instr->m_opcode != Js::OpCode::StLoopBodyCount &&
        instr->m_opcode != Js::OpCode::Ld_A &&
        instr->m_opcode != Js::OpCode::Ld_I4 &&
        !(instr->IsBranchInstr() && instr->AsBranchInstr()->IsUnconditional())
    )
    {
        TRACE_MEMOP_VERBOSE(loop, instr, _u("Instruction not accepted for memop"));
        return true;
    }

    // Check prev instr because it could have been added by an optimization and we won't see it here.
    if (OpCodeAttr::FastFldInstr(instr->m_opcode) || (instr->m_prev && OpCodeAttr::FastFldInstr(instr->m_prev->m_opcode)))
    {
        // Refuse any operations interacting with Fields
        TRACE_MEMOP_VERBOSE(loop, instr, _u("Field interaction detected"));
        return true;
    }

    if (Js::OpCodeUtil::GetOpCodeLayout(instr->m_opcode) == Js::OpLayoutType::ElementSlot)
    {
        // Refuse any operations interacting with slots
        TRACE_MEMOP_VERBOSE(loop, instr, _u("Slot interaction detected"));
        return true;
    }

    if (this->MayNeedBailOnImplicitCall(instr, src1Val, src2Val))
    {
        TRACE_MEMOP_VERBOSE(loop, instr, _u("Implicit call bailout detected"));
        return true;
    }
    return false;
}

void
GlobOpt::TryReplaceLdLen(IR::Instr *& instr)
{LOGMEIN("GlobOpt.cpp] 4888\n");
    // Change LdFld on arrays, strings, and 'arguments' to LdLen when we're accessing the .length field
    if ((instr->GetSrc1() && instr->GetSrc1()->IsSymOpnd() && instr->m_opcode == Js::OpCode::ProfiledLdFld) || instr->m_opcode == Js::OpCode::LdFld || instr->m_opcode == Js::OpCode::ScopedLdFld)
    {LOGMEIN("GlobOpt.cpp] 4891\n");
        IR::SymOpnd * opnd = instr->GetSrc1()->AsSymOpnd();
        Sym *sym = opnd->m_sym;
        if (sym->IsPropertySym())
        {LOGMEIN("GlobOpt.cpp] 4895\n");
            PropertySym *originalPropertySym = sym->AsPropertySym();
            // only on .length
            if (this->lengthEquivBv != nullptr && this->lengthEquivBv->Test(originalPropertySym->m_id))
            {LOGMEIN("GlobOpt.cpp] 4899\n");
                IR::RegOpnd* newopnd = IR::RegOpnd::New(originalPropertySym->m_stackSym, IRType::TyVar, instr->m_func);
                ValueInfo *const objectValueInfo = FindValue(originalPropertySym->m_stackSym)->GetValueInfo();
                // Only for things we'd emit a fast path for
                if (
                    objectValueInfo->IsLikelyAnyArray() ||
                    objectValueInfo->HasHadStringTag() ||
                    objectValueInfo->IsLikelyString() ||
                    newopnd->IsArgumentsObject() ||
                    (this->blockData.argObjSyms && IsArgumentsOpnd(newopnd))
                   )
                {LOGMEIN("GlobOpt.cpp] 4910\n");
                    // We need to properly transfer over the information from the old operand, which is
                    // a SymOpnd, to the new one, which is a RegOpnd. Unfortunately, the types mean the
                    // normal copy methods won't work here, so we're going to directly copy data.
                    newopnd->SetIsJITOptimizedReg(opnd->GetIsJITOptimizedReg());
                    newopnd->SetValueType(objectValueInfo->Type());
                    newopnd->SetIsDead(opnd->GetIsDead());

                    // Now that we have the operand we need, we can go ahead and make the new instr.
                    IR::Instr *newinstr = IR::Instr::New(Js::OpCode::LdLen_A, instr->m_func);
                    instr->TransferTo(newinstr);
                    newinstr->UnlinkSrc1();
                    newinstr->SetSrc1(newopnd);
                    instr->InsertAfter(newinstr);
                    instr->Remove();
                    instr = newinstr;
                }
            }
        }
    }
}

IR::Instr *
GlobOpt::OptInstr(IR::Instr *&instr, bool* isInstrRemoved)
{LOGMEIN("GlobOpt.cpp] 4934\n");
    Assert(instr->m_func->IsTopFunc() || instr->m_func->isGetterSetter || instr->m_func->callSiteIdInParentFunc != UINT16_MAX);

    IR::Opnd *src1, *src2;
    Value *src1Val = nullptr, *src2Val = nullptr, *dstVal = nullptr;
    Value *src1IndirIndexVal = nullptr, *dstIndirIndexVal = nullptr;
    IR::Instr *instrPrev = instr->m_prev;
    IR::Instr *instrNext = instr->m_next;

    if (instr->IsLabelInstr() && this->func->HasTry() && this->func->DoOptimizeTryCatch())
    {LOGMEIN("GlobOpt.cpp] 4944\n");
        this->currentRegion = instr->AsLabelInstr()->GetRegion();
        Assert(this->currentRegion);
    }

    if(PrepareForIgnoringIntOverflow(instr))
    {LOGMEIN("GlobOpt.cpp] 4950\n");
        if(!IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 4952\n");
            *isInstrRemoved = true;
            currentBlock->RemoveInstr(instr);
        }
        return instrNext;
    }

    if (!instr->IsRealInstr() || instr->IsByteCodeUsesInstr() || instr->m_opcode == Js::OpCode::Conv_Bool)
    {LOGMEIN("GlobOpt.cpp] 4960\n");
        return instrNext;
    }

    if (instr->m_opcode == Js::OpCode::Yield)
    {LOGMEIN("GlobOpt.cpp] 4965\n");
        // TODO[generators][ianhall]: Can this and the FillBailOutInfo call below be moved to after Src1 and Src2 so that Yield can be optimized right up to the actual yield?
        this->KillStateForGeneratorYield();
    }

    // Change LdFld on arrays, strings, and 'arguments' to LdLen when we're accessing the .length field
    this->TryReplaceLdLen(instr);

    // Consider: Do we ever get post-op bailout here, and if so is the FillBailOutInfo call in the right place?
    if (instr->HasBailOutInfo() && !this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 4975\n");
        this->FillBailOutInfo(this->currentBlock, instr->GetBailOutInfo());
    }

    this->instrCountSinceLastCleanUp++;

    instr = this->PreOptPeep(instr);

    this->OptArguments(instr);

    //StackArguments Optimization - We bail out if the index is out of range of actuals.
    if ((instr->m_opcode == Js::OpCode::LdElemI_A || instr->m_opcode == Js::OpCode::TypeofElem) &&
        instr->DoStackArgsOpt(this->func) && !this->IsLoopPrePass())
    {
        GenerateBailAtOperation(&instr, IR::BailOnStackArgsOutOfActualsRange);
    }

#if DBG
    PropertySym *propertySymUseBefore = nullptr;
    Assert(this->byteCodeUses == nullptr);
    this->byteCodeUsesBeforeOpt->ClearAll();
    GlobOpt::TrackByteCodeSymUsed(instr, this->byteCodeUsesBeforeOpt, &propertySymUseBefore);
    Assert(noImplicitCallUsesToInsert->Count() == 0);
#endif

    this->ignoredIntOverflowForCurrentInstr = false;
    this->ignoredNegativeZeroForCurrentInstr = false;

    src1 = instr->GetSrc1();
    src2 = instr->GetSrc2();

    if (src1)
    {LOGMEIN("GlobOpt.cpp] 5007\n");
        src1Val = this->OptSrc(src1, &instr, &src1IndirIndexVal);

        instr = this->SetTypeCheckBailOut(instr->GetSrc1(), instr, nullptr);

        if (src2)
        {LOGMEIN("GlobOpt.cpp] 5013\n");
            src2Val = this->OptSrc(src2, &instr);
        }
    }
    if(instr->GetDst() && instr->GetDst()->IsIndirOpnd())
    {LOGMEIN("GlobOpt.cpp] 5018\n");
        this->OptSrc(instr->GetDst(), &instr, &dstIndirIndexVal);
    }

    MarkArgumentsUsedForBranch(instr);
    CSEOptimize(this->currentBlock, &instr, &src1Val, &src2Val, &src1IndirIndexVal);
    OptimizeChecks(instr, src1Val, src2Val);
    OptArraySrc(&instr);
    OptNewScObject(&instr, src1Val);


    instr = this->OptPeep(instr, src1Val, src2Val);

    if (instr->m_opcode == Js::OpCode::Nop ||
        (instr->m_opcode == Js::OpCode::CheckThis &&
        instr->GetSrc1()->IsRegOpnd() &&
        instr->GetSrc1()->AsRegOpnd()->m_sym->m_isSafeThis))
    {LOGMEIN("GlobOpt.cpp] 5035\n");
        instrNext = instr->m_next;
        InsertNoImplicitCallUses(instr);
        if (this->byteCodeUses)
        {LOGMEIN("GlobOpt.cpp] 5039\n");
            this->InsertByteCodeUses(instr);
        }
        *isInstrRemoved = true;
        this->currentBlock->RemoveInstr(instr);
        return instrNext;
    }
    else if (instr->m_opcode == Js::OpCode::GetNewScObject && !this->IsLoopPrePass() && src1Val->GetValueInfo()->IsPrimitive())
    {LOGMEIN("GlobOpt.cpp] 5047\n");
        // Constructor returned (src1) a primitive value, so fold this into "dst = Ld_A src2", where src2 is the new object that
        // was passed into the constructor as its 'this' parameter
        instr->FreeSrc1();
        instr->SetSrc1(instr->UnlinkSrc2());
        instr->m_opcode = Js::OpCode::Ld_A;
        src1Val = src2Val;
        src2Val = nullptr;
    }
    else if (instr->m_opcode == Js::OpCode::TryCatch && this->func->DoOptimizeTryCatch())
    {LOGMEIN("GlobOpt.cpp] 5057\n");
        ProcessTryCatch(instr);
    }
    else if (instr->m_opcode == Js::OpCode::BrOnException)
    {LOGMEIN("GlobOpt.cpp] 5061\n");
        // BrOnException was added to model flow from try region to the catch region to assist
        // the backward pass in propagating bytecode upward exposed info from the catch block
        // to the try, and to handle break blocks. Removing it here as it has served its purpose
        // and keeping it around might also have unintended effects while merging block data for
        // the catch block's predecessors.
        // Note that the Deadstore pass will still be able to propagate bytecode upward exposed info
        // because it doesn't skip dead blocks for that.
        this->RemoveFlowEdgeToCatchBlock(instr);
        *isInstrRemoved = true;
        this->currentBlock->RemoveInstr(instr);
        return instrNext;
    }
    else if (instr->m_opcode == Js::OpCode::BrOnNoException)
    {LOGMEIN("GlobOpt.cpp] 5075\n");
        this->RemoveFlowEdgeToCatchBlock(instr);
    }

    bool isAlreadyTypeSpecialized = false;
    if (!IsLoopPrePass() && instr->HasBailOutInfo())
    {LOGMEIN("GlobOpt.cpp] 5081\n");
        if (instr->GetBailOutKind() == IR::BailOutExpectingInteger)
        {LOGMEIN("GlobOpt.cpp] 5083\n");
            isAlreadyTypeSpecialized = TypeSpecializeBailoutExpectedInteger(instr, src1Val, &dstVal);
        }
        else if (instr->GetBailOutKind() == IR::BailOutExpectingString)
        {LOGMEIN("GlobOpt.cpp] 5087\n");
            if (instr->GetSrc1()->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 5089\n");
                if (!src1Val || !src1Val->GetValueInfo()->IsLikelyString())
                {LOGMEIN("GlobOpt.cpp] 5091\n");
                    // Disable SwitchOpt if the source is definitely not a string - This may be realized only in Globopt
                    Assert(IsSwitchOptEnabled());
                    throw Js::RejitException(RejitReason::DisableSwitchOptExpectingString);
                }
            }
        }
    }

    bool forceInvariantHoisting = false;
    const bool ignoreIntOverflowInRangeForInstr = instr->ignoreIntOverflowInRange; // Save it since the instr can change
    if (!isAlreadyTypeSpecialized)
    {LOGMEIN("GlobOpt.cpp] 5103\n");
        bool redoTypeSpec;
        instr = this->TypeSpecialization(instr, &src1Val, &src2Val, &dstVal, &redoTypeSpec, &forceInvariantHoisting);

        if(redoTypeSpec && instr->m_opcode != Js::OpCode::Nop)
        {LOGMEIN("GlobOpt.cpp] 5108\n");
            forceInvariantHoisting = false;
            instr = this->TypeSpecialization(instr, &src1Val, &src2Val, &dstVal, &redoTypeSpec, &forceInvariantHoisting);
            Assert(!redoTypeSpec);
        }
        if (instr->m_opcode == Js::OpCode::Nop)
        {LOGMEIN("GlobOpt.cpp] 5114\n");
            InsertNoImplicitCallUses(instr);
            if (this->byteCodeUses)
            {LOGMEIN("GlobOpt.cpp] 5117\n");
                this->InsertByteCodeUses(instr);
            }
            instrNext = instr->m_next;
            *isInstrRemoved = true;
            this->currentBlock->RemoveInstr(instr);
            return instrNext;
        }
    }

    if (ignoreIntOverflowInRangeForInstr)
    {LOGMEIN("GlobOpt.cpp] 5128\n");
        VerifyIntSpecForIgnoringIntOverflow(instr);
    }

    // Track calls after any pre-op bailouts have been inserted before the call, because they will need to restore out params.
    // We don't inline in asmjs and hence we don't need to track calls in asmjs too, skipping this step for asmjs.
    if (!GetIsAsmJSFunc())
    {LOGMEIN("GlobOpt.cpp] 5135\n");
        this->TrackCalls(instr);
    }

    if (instr->GetSrc1())
    {LOGMEIN("GlobOpt.cpp] 5140\n");
        this->UpdateObjPtrValueType(instr->GetSrc1(), instr);
    }
    IR::Opnd *dst = instr->GetDst();

    if (dst)
    {
        // Copy prop dst uses and mark live/available type syms before tracking kills.
        CopyPropDstUses(dst, instr, src1Val);
    }

    // Track mark temp object before we process the dst so we can generate pre-op bailout
    instr = this->TrackMarkTempObject(instrPrev->m_next, instr);

    bool removed = OptTagChecks(instr);
    if (removed)
    {LOGMEIN("GlobOpt.cpp] 5156\n");
        *isInstrRemoved = true;
        return instrNext;
    }

    dstVal = this->OptDst(&instr, dstVal, src1Val, src2Val, dstIndirIndexVal, src1IndirIndexVal);
    dst = instr->GetDst();

    instrNext = instr->m_next;
    if (dst)
    {LOGMEIN("GlobOpt.cpp] 5166\n");
        if (this->func->HasTry() && this->func->DoOptimizeTryCatch())
        {LOGMEIN("GlobOpt.cpp] 5168\n");
            this->InsertToVarAtDefInTryRegion(instr, dst);
        }
        instr = this->SetTypeCheckBailOut(dst, instr, nullptr);
        this->UpdateObjPtrValueType(dst, instr);
    }

    BVSparse<JitArenaAllocator> instrByteCodeStackSymUsedAfter(this->alloc);
    PropertySym *propertySymUseAfter = nullptr;
    if (this->byteCodeUses != nullptr)
    {LOGMEIN("GlobOpt.cpp] 5178\n");
        GlobOpt::TrackByteCodeSymUsed(instr, &instrByteCodeStackSymUsedAfter, &propertySymUseAfter);
    }
#if DBG
    else
    {
        GlobOpt::TrackByteCodeSymUsed(instr, &instrByteCodeStackSymUsedAfter, &propertySymUseAfter);
        instrByteCodeStackSymUsedAfter.Equal(this->byteCodeUsesBeforeOpt);
        Assert(propertySymUseAfter == propertySymUseBefore);
    }
#endif

    bool isHoisted = false;
    if (this->currentBlock->loop && !this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 5192\n");
        isHoisted = this->TryHoistInvariant(instr, this->currentBlock, dstVal, src1Val, src2Val, true, false, forceInvariantHoisting);
    }

    src1 = instr->GetSrc1();
    if (!this->IsLoopPrePass() && src1)
    {LOGMEIN("GlobOpt.cpp] 5198\n");
        // instr  const, nonConst   =>  canonicalize by swapping operands
        // This simplifies lowering. (somewhat machine dependent)
        // Note that because of Var overflows, src1 may not have been constant prop'd to an IntConst
        this->PreLowerCanonicalize(instr, &src1Val, &src2Val);
    }

    if (!PHASE_OFF(Js::MemOpPhase, this->func) &&
        !isHoisted &&
        !(instr->IsJitProfilingInstr()) &&
        this->currentBlock->loop && !IsLoopPrePass() &&
        !func->IsJitInDebugMode() &&
        (func->HasProfileInfo() && !func->GetReadOnlyProfileInfo()->IsMemOpDisabled()) &&
        this->currentBlock->loop->doMemOp)
    {
        CollectMemOpInfo(instr, src1Val, src2Val);
    }

    InsertNoImplicitCallUses(instr);
    if (this->byteCodeUses != nullptr)
    {LOGMEIN("GlobOpt.cpp] 5218\n");
        // Optimization removed some uses from the instruction.
        // Need to insert fake uses so we can get the correct live register to restore in bailout.
        this->byteCodeUses->Minus(&instrByteCodeStackSymUsedAfter);
        if (this->propertySymUse == propertySymUseAfter)
        {LOGMEIN("GlobOpt.cpp] 5223\n");
            this->propertySymUse = nullptr;
        }
        this->InsertByteCodeUses(instr);
    }

    if (!this->IsLoopPrePass() && !isHoisted && this->IsImplicitCallBailOutCurrentlyNeeded(instr, src1Val, src2Val))
    {LOGMEIN("GlobOpt.cpp] 5230\n");
        IR::BailOutKind kind = IR::BailOutOnImplicitCalls;
        if(instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 5233\n");
            Assert(instr->GetBailOutInfo()->bailOutOffset == instr->GetByteCodeOffset());
            const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
            if((bailOutKind & ~IR::BailOutKindBits) != IR::BailOutOnImplicitCallsPreOp)
            {LOGMEIN("GlobOpt.cpp] 5237\n");
                Assert(!(bailOutKind & ~IR::BailOutKindBits));
                instr->SetBailOutKind(bailOutKind + IR::BailOutOnImplicitCallsPreOp);
            }
        }
        else if (instr->forcePreOpBailOutIfNeeded || this->isRecursiveCallOnLandingPad)
        {LOGMEIN("GlobOpt.cpp] 5243\n");
            // We can't have a byte code reg slot as dst to generate a
            // pre-op implicit call after we have processed the dst.

            // Consider: This might miss an opportunity to use a copy prop sym to restore
            // some other byte code reg if the dst is that copy prop that we already killed.
            Assert(!instr->GetDst()
                || !instr->GetDst()->IsRegOpnd()
                || instr->GetDst()->AsRegOpnd()->GetIsJITOptimizedReg()
                || !instr->GetDst()->AsRegOpnd()->m_sym->HasByteCodeRegSlot());

            this->GenerateBailAtOperation(&instr, IR::BailOutOnImplicitCallsPreOp);
        }
        else
        {
            // Capture value of the bailout after the operation is done.
            this->GenerateBailAfterOperation(&instr, kind);
        }
    }

    if (instr->HasBailOutInfo() && !this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 5264\n");
        GlobOptBlockData * globOptData = &this->currentBlock->globOptData;
        globOptData->changedSyms->ClearAll();

        if (!this->changedSymsAfterIncBailoutCandidate->IsEmpty())
        {LOGMEIN("GlobOpt.cpp] 5269\n");
            //
            // some symbols are changed after the values for current bailout have been
            // captured (GlobOpt::CapturedValues), need to restore such symbols as changed
            // for following incremental bailout construction, or we will miss capturing
            // values for later bailout
            //

            // swap changedSyms and changedSymsAfterIncBailoutCandidate
            // because both are from this->alloc
            BVSparse<JitArenaAllocator> * tempBvSwap = globOptData->changedSyms;
            globOptData->changedSyms = this->changedSymsAfterIncBailoutCandidate;
            this->changedSymsAfterIncBailoutCandidate = tempBvSwap;
        }

        globOptData->capturedValues = globOptData->capturedValuesCandidate;

        // null out capturedValuesCandicate to stop tracking symbols change for it
        globOptData->capturedValuesCandidate = nullptr;
    }

    return instrNext;
}

bool
GlobOpt::OptTagChecks(IR::Instr *instr)
{LOGMEIN("GlobOpt.cpp] 5295\n");
    if (PHASE_OFF(Js::OptTagChecksPhase, this->func) || !this->DoTagChecks())
    {LOGMEIN("GlobOpt.cpp] 5297\n");
        return false;
    }

    StackSym *stackSym = nullptr;
    IR::SymOpnd *symOpnd = nullptr;
    IR::RegOpnd *regOpnd = nullptr;

    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 5306\n");
    case Js::OpCode::LdFld:
    case Js::OpCode::LdMethodFld:
    case Js::OpCode::CheckFixedFld:
    case Js::OpCode::CheckPropertyGuardAndLoadType:
        symOpnd = instr->GetSrc1()->AsSymOpnd();
        stackSym = symOpnd->m_sym->AsPropertySym()->m_stackSym;
        break;

    case Js::OpCode::BailOnNotObject:
    case Js::OpCode::BailOnNotArray:
        if (instr->GetSrc1()->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 5318\n");
            regOpnd = instr->GetSrc1()->AsRegOpnd();
            stackSym = regOpnd->m_sym;
        }
        break;

    case Js::OpCode::StFld:
        symOpnd = instr->GetDst()->AsSymOpnd();
        stackSym = symOpnd->m_sym->AsPropertySym()->m_stackSym;
        break;
    }

    if (stackSym)
    {LOGMEIN("GlobOpt.cpp] 5331\n");
        Value *value = FindValue(blockData.symToValueMap, stackSym);
        if (value)
        {LOGMEIN("GlobOpt.cpp] 5334\n");
            ValueType valueType = value->GetValueInfo()->Type();
            if (instr->m_opcode == Js::OpCode::BailOnNotObject)
            {LOGMEIN("GlobOpt.cpp] 5337\n");
                if (valueType.CanBeTaggedValue())
                {
                    ChangeValueType(nullptr, value, valueType.SetCanBeTaggedValue(false), false);
                    return false;
                }
                if (this->byteCodeUses)
                {LOGMEIN("GlobOpt.cpp] 5344\n");
                    this->InsertByteCodeUses(instr);
                }
                this->currentBlock->RemoveInstr(instr);
                return true;
            }

            if (valueType.CanBeTaggedValue() &&
                !valueType.HasBeenNumber() &&
                (this->IsLoopPrePass() || !this->currentBlock->loop))
            {LOGMEIN("GlobOpt.cpp] 5354\n");
                ValueType newValueType = valueType.SetCanBeTaggedValue(false);

                // Split out the tag check as a separate instruction.
                IR::Instr *bailOutInstr;
                bailOutInstr = IR::BailOutInstr::New(Js::OpCode::BailOnNotObject, IR::BailOutOnTaggedValue, instr, instr->m_func);
                if (!this->IsLoopPrePass())
                {LOGMEIN("GlobOpt.cpp] 5361\n");
                    FillBailOutInfo(this->currentBlock, bailOutInstr->GetBailOutInfo());
                }
                IR::RegOpnd *srcOpnd = regOpnd;
                if (!srcOpnd)
                {LOGMEIN("GlobOpt.cpp] 5366\n");
                    srcOpnd = IR::RegOpnd::New(stackSym, stackSym->GetType(), instr->m_func);
                    AnalysisAssert(symOpnd);
                    if (symOpnd->GetIsJITOptimizedReg())
                    {LOGMEIN("GlobOpt.cpp] 5370\n");
                        srcOpnd->SetIsJITOptimizedReg(true);
                    }
                }
                bailOutInstr->SetSrc1(srcOpnd);
                bailOutInstr->GetSrc1()->SetValueType(valueType);
                instr->InsertBefore(bailOutInstr);

                if (symOpnd)
                {LOGMEIN("GlobOpt.cpp] 5379\n");
                    symOpnd->SetPropertyOwnerValueType(newValueType);
                }
                else
                {
                    regOpnd->SetValueType(newValueType);
                }
                ChangeValueType(nullptr, value, newValueType, false);
            }
        }
    }

    return false;
}

bool
GlobOpt::TypeSpecializeBailoutExpectedInteger(IR::Instr* instr, Value* src1Val, Value** dstVal)
{LOGMEIN("GlobOpt.cpp] 5396\n");
    bool isAlreadyTypeSpecialized = false;

    if(instr->GetSrc1()->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 5400\n");
        if (!src1Val || !src1Val->GetValueInfo()->IsLikelyInt() || instr->GetSrc1()->AsRegOpnd()->m_sym->m_isNotInt)
        {LOGMEIN("GlobOpt.cpp] 5402\n");
            Assert(IsSwitchOptEnabled());
            throw Js::RejitException(RejitReason::DisableSwitchOptExpectingInteger);
        }

        // Attach the BailOutExpectingInteger to FromVar and Remove the bail out info on the Ld_A (Begin Switch) instr.
        this->ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, src1Val, nullptr, TyInt32, IR::BailOutExpectingInteger, false, instr);

        //TypeSpecialize the dst of Ld_A
        TypeSpecializeIntDst(instr, instr->m_opcode, src1Val, src1Val, nullptr, IR::BailOutInvalid, INT32_MIN, INT32_MAX, dstVal);
        isAlreadyTypeSpecialized = true;
    }

    instr->ClearBailOutInfo();
    return isAlreadyTypeSpecialized;
}

Value*
GlobOpt::OptDst(
    IR::Instr ** pInstr,
    Value *dstVal,
    Value *src1Val,
    Value *src2Val,
    Value *dstIndirIndexVal,
    Value *src1IndirIndexVal)
{LOGMEIN("GlobOpt.cpp] 5427\n");
    IR::Instr *&instr = *pInstr;
    IR::Opnd *opnd = instr->GetDst();

    if (opnd)
    {LOGMEIN("GlobOpt.cpp] 5432\n");
        if (opnd->IsSymOpnd() && opnd->AsSymOpnd()->IsPropertySymOpnd())
        {LOGMEIN("GlobOpt.cpp] 5434\n");
            this->FinishOptPropOp(instr, opnd->AsPropertySymOpnd());
        }
        else if (instr->m_opcode == Js::OpCode::StElemI_A ||
                 instr->m_opcode == Js::OpCode::StElemI_A_Strict ||
                 instr->m_opcode == Js::OpCode::InitComputedProperty)
        {LOGMEIN("GlobOpt.cpp] 5440\n");
            this->KillObjectHeaderInlinedTypeSyms(this->currentBlock, false);
        }

        if (opnd->IsIndirOpnd() && !this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 5445\n");
            IR::RegOpnd *baseOpnd = opnd->AsIndirOpnd()->GetBaseOpnd();
            const ValueType baseValueType(baseOpnd->GetValueType());
            if ((
                    baseValueType.IsLikelyNativeArray() ||
                #ifdef _M_IX86
                    (
                        !AutoSystemInfo::Data.SSE2Available() &&
                        baseValueType.IsLikelyObject() &&
                        (
                            baseValueType.GetObjectType() == ObjectType::Float32Array ||
                            baseValueType.GetObjectType() == ObjectType::Float64Array
                        )
                    )
                #else
                    false
                #endif
                ) &&
                instr->GetSrc1()->IsVar())
            {LOGMEIN("GlobOpt.cpp] 5464\n");
                if(instr->m_opcode == Js::OpCode::StElemC)
                {LOGMEIN("GlobOpt.cpp] 5466\n");
                    // StElemC has different code that handles native array conversion or missing value stores. Add a bailout
                    // for those cases.
                    Assert(baseValueType.IsLikelyNativeArray());
                    Assert(!instr->HasBailOutInfo());
                    GenerateBailAtOperation(&instr, IR::BailOutConventionalNativeArrayAccessOnly);
                }
                else if(instr->HasBailOutInfo())
                {LOGMEIN("GlobOpt.cpp] 5474\n");
                    // The lowerer is not going to generate a fast path for this case. Remove any bailouts that require the fast
                    // path. Note that the removed bailouts should not be necessary for correctness. Bailout on native array
                    // conversion will be handled automatically as normal.
                    IR::BailOutKind bailOutKind = instr->GetBailOutKind();
                    if(bailOutKind & IR::BailOutOnArrayAccessHelperCall)
                    {LOGMEIN("GlobOpt.cpp] 5480\n");
                        bailOutKind -= IR::BailOutOnArrayAccessHelperCall;
                    }
                    if(bailOutKind == IR::BailOutOnImplicitCallsPreOp)
                    {LOGMEIN("GlobOpt.cpp] 5484\n");
                        bailOutKind -= IR::BailOutOnImplicitCallsPreOp;
                    }
                    if(bailOutKind)
                    {LOGMEIN("GlobOpt.cpp] 5488\n");
                        instr->SetBailOutKind(bailOutKind);
                    }
                    else
                    {
                        instr->ClearBailOutInfo();
                    }
                }
            }
        }
    }

    this->ProcessKills(instr);

    if (opnd)
    {LOGMEIN("GlobOpt.cpp] 5503\n");
        if (dstVal == nullptr)
        {LOGMEIN("GlobOpt.cpp] 5505\n");
            dstVal = ValueNumberDst(pInstr, src1Val, src2Val);
        }
        if (this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 5509\n");
            // Keep track of symbols defined in the loop.
            if (opnd->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 5512\n");
                StackSym *symDst = opnd->AsRegOpnd()->m_sym;
                rootLoopPrePass->symsDefInLoop->Set(symDst->m_id);
            }
        }
        else if (dstVal)
        {LOGMEIN("GlobOpt.cpp] 5518\n");
            opnd->SetValueType(dstVal->GetValueInfo()->Type());

            if(currentBlock->loop &&
                !IsLoopPrePass() &&
                (instr->m_opcode == Js::OpCode::Ld_A || instr->m_opcode == Js::OpCode::Ld_I4) &&
                instr->GetSrc1()->IsRegOpnd() &&
                !func->IsJitInDebugMode() &&
                func->DoGlobOptsForGeneratorFunc())
            {LOGMEIN("GlobOpt.cpp] 5527\n");
                // Look for the following patterns:
                //
                // Pattern 1:
                //     s1[liveOnBackEdge] = s3[dead]
                //
                // Pattern 2:
                //     s3 = operation(s1[liveOnBackEdge], s2)
                //     s1[liveOnBackEdge] = s3
                //
                // In both patterns, s1 and s3 have the same value by the end. Prefer to use s1 as the sym store instead of s3
                // since s1 is live on back-edge, as otherwise, their lifetimes overlap, requiring two registers to hold the
                // value instead of one.
                do
                {LOGMEIN("GlobOpt.cpp] 5541\n");
                    IR::RegOpnd *const src = instr->GetSrc1()->AsRegOpnd();
                    StackSym *srcVarSym = src->m_sym;
                    if(srcVarSym->IsTypeSpec())
                    {LOGMEIN("GlobOpt.cpp] 5545\n");
                        srcVarSym = srcVarSym->GetVarEquivSym(nullptr);
                        Assert(srcVarSym);
                    }
                    if(dstVal->GetValueInfo()->GetSymStore() != srcVarSym)
                    {LOGMEIN("GlobOpt.cpp] 5550\n");
                        break;
                    }

                    IR::RegOpnd *const dst = opnd->AsRegOpnd();
                    StackSym *dstVarSym = dst->m_sym;
                    if(dstVarSym->IsTypeSpec())
                    {LOGMEIN("GlobOpt.cpp] 5557\n");
                        dstVarSym = dstVarSym->GetVarEquivSym(nullptr);
                        Assert(dstVarSym);
                    }
                    if(!currentBlock->loop->regAlloc.liveOnBackEdgeSyms->Test(dstVarSym->m_id))
                    {LOGMEIN("GlobOpt.cpp] 5562\n");
                        break;
                    }

                    Value *const srcValue = FindValue(srcVarSym);
                    if(srcValue->GetValueNumber() != dstVal->GetValueNumber())
                    {LOGMEIN("GlobOpt.cpp] 5568\n");
                        break;
                    }

                    if(!src->GetIsDead())
                    {LOGMEIN("GlobOpt.cpp] 5573\n");
                        IR::Instr *const prevInstr = instr->GetPrevRealInstrOrLabel();
                        IR::Opnd *const prevDst = prevInstr->GetDst();
                        if(!prevDst ||
                            !src->IsEqualInternal(prevDst) ||
                            !(
                                (prevInstr->GetSrc1() && dst->IsEqual(prevInstr->GetSrc1())) ||
                                (prevInstr->GetSrc2() && dst->IsEqual(prevInstr->GetSrc2()))
                            ))
                        {LOGMEIN("GlobOpt.cpp] 5582\n");
                            break;
                        }
                    }

                    this->SetSymStoreDirect(dstVal->GetValueInfo(), dstVarSym);
                } while(false);
            }
        }

        this->ValueNumberObjectType(opnd, instr);
    }

    this->CSEAddInstr(this->currentBlock, *pInstr, dstVal, src1Val, src2Val, dstIndirIndexVal, src1IndirIndexVal);

    return dstVal;
}

void
GlobOpt::CopyPropDstUses(IR::Opnd *opnd, IR::Instr *instr, Value *src1Val)
{LOGMEIN("GlobOpt.cpp] 5602\n");
    if (opnd->IsSymOpnd())
    {LOGMEIN("GlobOpt.cpp] 5604\n");
        IR::SymOpnd *symOpnd = opnd->AsSymOpnd();

        if (symOpnd->m_sym->IsPropertySym())
        {LOGMEIN("GlobOpt.cpp] 5608\n");
            PropertySym * originalPropertySym = symOpnd->m_sym->AsPropertySym();

            Value *const objectValue = FindValue(originalPropertySym->m_stackSym);
            symOpnd->SetPropertyOwnerValueType(objectValue ? objectValue->GetValueInfo()->Type() : ValueType::Uninitialized);

            this->FieldHoistOptDst(instr, originalPropertySym, src1Val);
            PropertySym * sym = this->CopyPropPropertySymObj(symOpnd, instr);
            if (sym != originalPropertySym && !this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 5617\n");
                // Consider: This doesn't detect hoistability of a property sym after object pointer copy prop
                // on loop prepass. But if it so happened that the property sym is hoisted, we might as well do so.
                this->FieldHoistOptDst(instr, sym, src1Val);
            }

        }
    }
}

void
GlobOpt::SetLoopFieldInitialValue(Loop *loop, IR::Instr *instr, PropertySym *propertySym, PropertySym *originalPropertySym)
{LOGMEIN("GlobOpt.cpp] 5629\n");
    Value *initialValue;
    StackSym *symStore;

    if (loop->allFieldsKilled || loop->fieldKilled->Test(originalPropertySym->m_id))
    {LOGMEIN("GlobOpt.cpp] 5634\n");
        return;
    }
    Assert(!loop->fieldKilled->Test(propertySym->m_id));

    // Value already exists
    if (this->FindValue(propertySym))
    {LOGMEIN("GlobOpt.cpp] 5641\n");
        return;
    }

    // If this initial value was already added, we would find in the current value table.
    Assert(!loop->initialValueFieldMap.TryGetValue(propertySym, &initialValue));

    // If propertySym is live in landingPad, we don't need an initial value.
    if (loop->landingPad->globOptData.liveFields->Test(propertySym->m_id))
    {LOGMEIN("GlobOpt.cpp] 5650\n");
        return;
    }

    Value *landingPadObjPtrVal, *currentObjPtrVal;
    landingPadObjPtrVal = this->FindValue(loop->landingPad->globOptData.symToValueMap, propertySym->m_stackSym);
    currentObjPtrVal = this->FindValue(propertySym->m_stackSym);
    if (!currentObjPtrVal || !landingPadObjPtrVal || currentObjPtrVal->GetValueNumber() != landingPadObjPtrVal->GetValueNumber())
    {LOGMEIN("GlobOpt.cpp] 5658\n");
        // objPtr has a different value in the landing pad.
        return;
    }

    // The opnd's value type has not yet been initialized. Since the property sym doesn't have a value, it effectively has an
    // Uninitialized value type. Use the profiled value type from the instruction.
    const ValueType profiledValueType =
        instr->IsProfiledInstr() ? instr->AsProfiledInstr()->u.FldInfo().valueType : ValueType::Uninitialized;
    Assert(!profiledValueType.IsDefinite()); // Hence the values created here don't need to be tracked for kills
    initialValue = this->NewGenericValue(profiledValueType, propertySym);
    symStore = StackSym::New(this->func);

    initialValue->GetValueInfo()->SetSymStore(symStore);
    loop->initialValueFieldMap.Add(propertySym, initialValue->Copy(this->alloc, initialValue->GetValueNumber()));

    // Copy the initial value into the landing pad, but without a symStore
    Value *landingPadInitialValue = Value::New(this->alloc, initialValue->GetValueNumber(),
        ValueInfo::New(this->alloc, initialValue->GetValueInfo()->Type()));
    this->SetValue(&(loop->landingPad->globOptData), landingPadInitialValue, propertySym);
    loop->landingPad->globOptData.liveFields->Set(propertySym->m_id);

#if DBG_DUMP
    if (PHASE_TRACE(Js::FieldPREPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 5682\n");
        Output::Print(_u("** TRACE:  Field PRE initial value for loop head #%d. Val:%d symStore:"),
            loop->GetHeadBlock()->GetBlockNum(), initialValue->GetValueNumber());
        symStore->Dump();
        Output::Print(_u("\n    Instr: "));
        instr->Dump();
    }
#endif

    // Add initial value to all the previous blocks in the loop.
    FOREACH_BLOCK_BACKWARD_IN_RANGE(block, this->currentBlock->GetPrev(), loop->GetHeadBlock())
    {LOGMEIN("GlobOpt.cpp] 5693\n");
        if (block->GetDataUseCount() == 0)
        {LOGMEIN("GlobOpt.cpp] 5695\n");
            // All successor blocks have been processed, no point in adding the value.
            continue;
        }
        Value *newValue = initialValue->Copy(this->alloc, initialValue->GetValueNumber());
        this->SetValue(&(block->globOptData), newValue, propertySym);
        block->globOptData.liveFields->Set(propertySym->m_id);
        this->SetValue(&(block->globOptData), newValue, symStore);
        block->globOptData.liveVarSyms->Set(symStore->m_id);
    } NEXT_BLOCK_BACKWARD_IN_RANGE;

    this->SetValue(&(this->currentBlock->globOptData), initialValue, symStore);
    this->currentBlock->globOptData.liveVarSyms->Set(symStore->m_id);
    this->blockData.liveFields->Set(propertySym->m_id);
}

// Examine src, apply copy prop and value number it
Value*
GlobOpt::OptSrc(IR::Opnd *opnd, IR::Instr * *pInstr, Value **indirIndexValRef, IR::IndirOpnd *parentIndirOpnd)
{LOGMEIN("GlobOpt.cpp] 5714\n");
    IR::Instr * &instr = *pInstr;
    Assert(!indirIndexValRef || !*indirIndexValRef);
    Assert(
        parentIndirOpnd
            ? opnd == parentIndirOpnd->GetBaseOpnd() || opnd == parentIndirOpnd->GetIndexOpnd()
            : opnd == instr->GetSrc1() || opnd == instr->GetSrc2() || opnd == instr->GetDst() && opnd->IsIndirOpnd());

    Sym *sym;
    Value *val;
    PropertySym *originalPropertySym = nullptr;

    switch(opnd->GetKind())
    {LOGMEIN("GlobOpt.cpp] 5727\n");
    case IR::OpndKindIntConst:
        val = this->GetIntConstantValue(opnd->AsIntConstOpnd()->AsInt32(), instr);
        opnd->SetValueType(val->GetValueInfo()->Type());
        return val;

    case IR::OpndKindInt64Const:
        return nullptr;

    case IR::OpndKindFloatConst:
    {LOGMEIN("GlobOpt.cpp] 5737\n");
        const FloatConstType floatValue = opnd->AsFloatConstOpnd()->m_value;
        int32 int32Value;
        if(Js::JavascriptNumber::TryGetInt32Value(floatValue, &int32Value))
        {LOGMEIN("GlobOpt.cpp] 5741\n");
            val = GetIntConstantValue(int32Value, instr);
        }
        else
        {
            val = NewFloatConstantValue(floatValue);
        }
        opnd->SetValueType(val->GetValueInfo()->Type());
        return val;
    }

    case IR::OpndKindAddr:
        {LOGMEIN("GlobOpt.cpp] 5753\n");
            IR::AddrOpnd *addrOpnd = opnd->AsAddrOpnd();
            if (addrOpnd->m_isFunction)
            {LOGMEIN("GlobOpt.cpp] 5756\n");
                AssertMsg(!PHASE_OFF(Js::FixedMethodsPhase, instr->m_func), "Fixed function address operand with fixed method calls phase disabled?");
                val = NewFixedFunctionValue((Js::JavascriptFunction *)addrOpnd->m_address, addrOpnd);
                opnd->SetValueType(val->GetValueInfo()->Type());
                return val;
            }
            else if (addrOpnd->IsVar() && Js::TaggedInt::Is(addrOpnd->m_address))
            {LOGMEIN("GlobOpt.cpp] 5763\n");
                val = this->GetIntConstantValue(Js::TaggedInt::ToInt32(addrOpnd->m_address), instr);
                opnd->SetValueType(val->GetValueInfo()->Type());
                return val;
            }
            val = this->GetVarConstantValue(addrOpnd);
            return val;
        }

    case IR::OpndKindSym:
    {LOGMEIN("GlobOpt.cpp] 5773\n");
        // Clear the opnd's value type up-front, so that this code cannot accidentally use the value type set from a previous
        // OptSrc on the same instruction (for instance, from an earlier loop prepass). The value type will be set from the
        // value if available, before returning from this function.
        opnd->SetValueType(ValueType::Uninitialized);

        sym = opnd->AsSymOpnd()->m_sym;

        // Don't create a new value for ArgSlots and don't copy prop them away.
        if (sym->IsStackSym() && sym->AsStackSym()->IsArgSlotSym())
        {LOGMEIN("GlobOpt.cpp] 5783\n");
            return nullptr;
        }

        // Unless we have profile info, don't create a new value for ArgSlots and don't copy prop them away.
        if (sym->IsStackSym() && sym->AsStackSym()->IsParamSlotSym())
        {LOGMEIN("GlobOpt.cpp] 5789\n");
            if (!instr->m_func->IsLoopBody() && instr->m_func->HasProfileInfo())
            {LOGMEIN("GlobOpt.cpp] 5791\n");
                // Skip "this" pointer.
                int paramSlotNum = sym->AsStackSym()->GetParamSlotNum() - 2;
                if (paramSlotNum >= 0)
                {LOGMEIN("GlobOpt.cpp] 5795\n");
                    const auto parameterType = instr->m_func->GetReadOnlyProfileInfo()->GetParameterInfo(static_cast<Js::ArgSlot>(paramSlotNum));
                    val = NewGenericValue(parameterType);
                    opnd->SetValueType(val->GetValueInfo()->Type());
                    return val;
                }
            }
            return nullptr;
        }

        if (!sym->IsPropertySym())
        {LOGMEIN("GlobOpt.cpp] 5806\n");
            break;
        }
        originalPropertySym = sym->AsPropertySym();

        Value *const objectValue = FindValue(originalPropertySym->m_stackSym);
        opnd->AsSymOpnd()->SetPropertyOwnerValueType(
            objectValue ? objectValue->GetValueInfo()->Type() : ValueType::Uninitialized);

        if (!FieldHoistOptSrc(opnd->AsSymOpnd(), instr, originalPropertySym))
        {LOGMEIN("GlobOpt.cpp] 5816\n");
            sym = this->CopyPropPropertySymObj(opnd->AsSymOpnd(), instr);

            // Consider: This doesn't detect hoistability of a property sym after object pointer copy prop
            // on loop prepass. But if it so happened that the property sym is hoisted, we might as well do so.
            if (originalPropertySym == sym || this->IsLoopPrePass() ||
                !FieldHoistOptSrc(opnd->AsSymOpnd(), instr, sym->AsPropertySym()))
            {LOGMEIN("GlobOpt.cpp] 5823\n");
                if (!DoFieldCopyProp())
                {LOGMEIN("GlobOpt.cpp] 5825\n");
                    if (opnd->AsSymOpnd()->IsPropertySymOpnd())
                    {LOGMEIN("GlobOpt.cpp] 5827\n");
                        this->FinishOptPropOp(instr, opnd->AsPropertySymOpnd());
                    }
                    return nullptr;
                }
                switch (instr->m_opcode)
                {LOGMEIN("GlobOpt.cpp] 5833\n");
                    // These need the symbolic reference to the field, don't copy prop the value of the field
                case Js::OpCode::DeleteFld:
                case Js::OpCode::DeleteRootFld:
                case Js::OpCode::DeleteFldStrict:
                case Js::OpCode::DeleteRootFldStrict:
                case Js::OpCode::ScopedDeleteFld:
                case Js::OpCode::ScopedDeleteFldStrict:
                case Js::OpCode::LdMethodFromFlags:
                case Js::OpCode::BrOnNoProperty:
                case Js::OpCode::BrOnHasProperty:
                case Js::OpCode::LdMethodFldPolyInlineMiss:
                case Js::OpCode::StSlotChkUndecl:
                    return nullptr;
                };

                if (instr->CallsGetter())
                {LOGMEIN("GlobOpt.cpp] 5850\n");
                    return nullptr;
                }

                if (this->IsLoopPrePass() && this->DoFieldPRE(this->rootLoopPrePass))
                {LOGMEIN("GlobOpt.cpp] 5855\n");
                    if (!this->prePassLoop->allFieldsKilled && !this->prePassLoop->fieldKilled->Test(sym->m_id))
                    {LOGMEIN("GlobOpt.cpp] 5857\n");
                        this->SetLoopFieldInitialValue(this->rootLoopPrePass, instr, sym->AsPropertySym(), originalPropertySym);
                    }
                    if (this->IsPREInstrCandidateLoad(instr->m_opcode))
                    {LOGMEIN("GlobOpt.cpp] 5861\n");
                        // Foreach property sym, remember the first instruction that loads it.
                        // Can this be done in one call?
                        if (!this->prePassInstrMap->ContainsKey(sym->m_id))
                        {LOGMEIN("GlobOpt.cpp] 5865\n");
                            this->prePassInstrMap->AddNew(sym->m_id, instr);
                        }
                    }
                }
                break;
            }
        }

        // We field hoisted, we can continue as a reg.
        opnd = instr->GetSrc1();
    }
    case IR::OpndKindReg:
        // Clear the opnd's value type up-front, so that this code cannot accidentally use the value type set from a previous
        // OptSrc on the same instruction (for instance, from an earlier loop prepass). The value type will be set from the
        // value if available, before returning from this function.
        opnd->SetValueType(ValueType::Uninitialized);

        sym = opnd->AsRegOpnd()->m_sym;
        this->MarkTempLastUse(instr, opnd->AsRegOpnd());
        if (sym->AsStackSym()->IsTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 5886\n");
            sym = sym->AsStackSym()->GetVarEquivSym(this->func);
        }
        break;

    case IR::OpndKindIndir:
        this->OptimizeIndirUses(opnd->AsIndirOpnd(), &instr, indirIndexValRef);
        return nullptr;

    default:
        return nullptr;
    }

    val = this->FindValue(sym);

    if (val)
    {LOGMEIN("GlobOpt.cpp] 5902\n");
        Assert(GlobOpt::IsLive(sym, this->currentBlock) || (sym->IsPropertySym()));
        if (instr)
        {LOGMEIN("GlobOpt.cpp] 5905\n");
            opnd = this->CopyProp(opnd, instr, val, parentIndirOpnd);
        }

        // Check if we freed the operand.
        if (opnd == nullptr)
        {LOGMEIN("GlobOpt.cpp] 5911\n");
            return nullptr;
        }

        // In a loop prepass, determine stack syms that are used before they are defined in the root loop for which the prepass
        // is being done. This information is used to do type specialization conversions in the landing pad where appropriate.
        if(IsLoopPrePass() &&
            sym->IsStackSym() &&
            !rootLoopPrePass->symsUsedBeforeDefined->Test(sym->m_id) &&
            IsLive(sym, &rootLoopPrePass->landingPad->globOptData) && !isAsmJSFunc) // no typespec in asmjs and hence skipping this
        {LOGMEIN("GlobOpt.cpp] 5921\n");
            Value *const landingPadValue = FindValue(rootLoopPrePass->landingPad->globOptData.symToValueMap, sym);
            if(landingPadValue && val->GetValueNumber() == landingPadValue->GetValueNumber())
            {LOGMEIN("GlobOpt.cpp] 5924\n");
                rootLoopPrePass->symsUsedBeforeDefined->Set(sym->m_id);
                ValueInfo *landingPadValueInfo = landingPadValue->GetValueInfo();
                if(landingPadValueInfo->IsLikelyNumber())
                {LOGMEIN("GlobOpt.cpp] 5928\n");
                    rootLoopPrePass->likelyNumberSymsUsedBeforeDefined->Set(sym->m_id);
                    if(DoAggressiveIntTypeSpec() ? landingPadValueInfo->IsLikelyInt() : landingPadValueInfo->IsInt())
                    {LOGMEIN("GlobOpt.cpp] 5931\n");
                        // Can only force int conversions in the landing pad based on likely-int values if aggressive int type
                        // specialization is enabled.
                        rootLoopPrePass->likelyIntSymsUsedBeforeDefined->Set(sym->m_id);
                    }
                }

#ifdef ENABLE_SIMDJS
                // SIMD_JS
                // For uses before defs, we set likelySimd128*SymsUsedBeforeDefined bits for syms that have landing pad value info that allow type-spec to happen in the loop body.
                // The BV will be added to loop header if the backedge has a live matching type-spec value. We then compensate in the loop header to unbox the value.
                // This allows type-spec in the landing pad instead of boxing/unboxing on each iteration.
                if (Js::IsSimd128Opcode(instr->m_opcode))
                {LOGMEIN("GlobOpt.cpp] 5944\n");
                    // Simd ops are strongly typed. We type-spec only if the type is likely/Definitely the expected type or if we have object which can come from merging different Simd types.
                    // Simd value must be initialized properly on all paths before the loop entry. Cannot be merged with Undefined/Null.
                    ThreadContext::SimdFuncSignature funcSignature;
                    instr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(instr->m_opcode, funcSignature);
                    Assert(funcSignature.valid);
                    ValueType expectedType = funcSignature.args[opnd == instr->GetSrc1() ? 0 : 1];

                    if (expectedType.IsSimd128Float32x4())
                    {LOGMEIN("GlobOpt.cpp] 5953\n");
                        if  (
                                (landingPadValueInfo->IsLikelySimd128Float32x4() || (landingPadValueInfo->IsLikelyObject() && landingPadValueInfo->GetObjectType() == ObjectType::Object))
                                &&
                                !landingPadValueInfo->HasBeenUndefined() && !landingPadValueInfo->HasBeenNull()
                            )
                        {LOGMEIN("GlobOpt.cpp] 5959\n");
                            rootLoopPrePass->likelySimd128F4SymsUsedBeforeDefined->Set(sym->m_id);
                        }
                    }
                    else if (expectedType.IsSimd128Int32x4())
                    {LOGMEIN("GlobOpt.cpp] 5964\n");
                        if (
                            (landingPadValueInfo->IsLikelySimd128Int32x4() || (landingPadValueInfo->IsLikelyObject() && landingPadValueInfo->GetObjectType() == ObjectType::Object))
                            &&
                            !landingPadValueInfo->HasBeenUndefined() && !landingPadValueInfo->HasBeenNull()
                          )
                        {LOGMEIN("GlobOpt.cpp] 5970\n");
                            rootLoopPrePass->likelySimd128I4SymsUsedBeforeDefined->Set(sym->m_id);
                        }
                    }
                }
                else if (instr->m_opcode == Js::OpCode::ExtendArg_A && opnd == instr->GetSrc1() && instr->GetDst()->GetValueType().IsSimd128())
                {LOGMEIN("GlobOpt.cpp] 5976\n");
                    // Extended_Args for Simd ops are annotated with the expected type by the inliner. Use this info to find out if type-spec is supposed to happen.
                    ValueType expectedType = instr->GetDst()->GetValueType();

                    if ((landingPadValueInfo->IsLikelySimd128Float32x4() || (landingPadValueInfo->IsLikelyObject() && landingPadValueInfo->GetObjectType() == ObjectType::Object))
                        && expectedType.IsSimd128Float32x4())
                    {LOGMEIN("GlobOpt.cpp] 5982\n");
                        rootLoopPrePass->likelySimd128F4SymsUsedBeforeDefined->Set(sym->m_id);
                    }
                    else if ((landingPadValueInfo->IsLikelySimd128Int32x4() || (landingPadValueInfo->IsLikelyObject() && landingPadValueInfo->GetObjectType() == ObjectType::Object))
                        && expectedType.IsSimd128Int32x4())
                    {LOGMEIN("GlobOpt.cpp] 5987\n");
                        rootLoopPrePass->likelySimd128I4SymsUsedBeforeDefined->Set(sym->m_id);
                    }
                }
#endif
            }
        }
    }
    else if ((GlobOpt::TransferSrcValue(instr) || OpCodeAttr::CanCSE(instr->m_opcode)) && (opnd == instr->GetSrc1() || opnd == instr->GetSrc2()))
    {LOGMEIN("GlobOpt.cpp] 5996\n");
        if (sym->IsPropertySym())
        {LOGMEIN("GlobOpt.cpp] 5998\n");
            val = this->CreateFieldSrcValue(sym->AsPropertySym(), originalPropertySym, &opnd, instr);
        }
        else
        {
            val = this->NewGenericValue(ValueType::Uninitialized, opnd);
        }
    }

    if (opnd->IsSymOpnd() && opnd->AsSymOpnd()->IsPropertySymOpnd())
    {LOGMEIN("GlobOpt.cpp] 6008\n");
        TryOptimizeInstrWithFixedDataProperty(&instr);
        this->FinishOptPropOp(instr, opnd->AsPropertySymOpnd());
    }

    if (val)
    {LOGMEIN("GlobOpt.cpp] 6014\n");
        ValueType valueType(val->GetValueInfo()->Type());
        if (valueType.IsLikelyNativeArray() && !valueType.IsObject() && instr->IsProfiledInstr())
        {LOGMEIN("GlobOpt.cpp] 6017\n");
            // See if we have profile data for the array type
            IR::ProfiledInstr *const profiledInstr = instr->AsProfiledInstr();
            ValueType profiledArrayType;
            switch(instr->m_opcode)
            {LOGMEIN("GlobOpt.cpp] 6022\n");
                case Js::OpCode::LdElemI_A:
                    if(instr->GetSrc1()->IsIndirOpnd() && opnd == instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd())
                    {LOGMEIN("GlobOpt.cpp] 6025\n");
                        profiledArrayType = profiledInstr->u.ldElemInfo->GetArrayType();
                    }
                    break;

                case Js::OpCode::StElemI_A:
                case Js::OpCode::StElemI_A_Strict:
                case Js::OpCode::StElemC:
                    if(instr->GetDst()->IsIndirOpnd() && opnd == instr->GetDst()->AsIndirOpnd()->GetBaseOpnd())
                    {LOGMEIN("GlobOpt.cpp] 6034\n");
                        profiledArrayType = profiledInstr->u.stElemInfo->GetArrayType();
                    }
                    break;

                case Js::OpCode::LdLen_A:
                    if(instr->GetSrc1()->IsRegOpnd() && opnd == instr->GetSrc1())
                    {LOGMEIN("GlobOpt.cpp] 6041\n");
                        profiledArrayType = profiledInstr->u.ldElemInfo->GetArrayType();
                    }
                    break;
            }
            if(profiledArrayType.IsLikelyObject() &&
                profiledArrayType.GetObjectType() == valueType.GetObjectType() &&
                (profiledArrayType.HasVarElements() || (valueType.HasIntElements() && profiledArrayType.HasFloatElements())))
            {LOGMEIN("GlobOpt.cpp] 6049\n");
                // Merge array type we pulled from profile with type propagated by dataflow.
                valueType = valueType.Merge(profiledArrayType).SetHasNoMissingValues(valueType.HasNoMissingValues());
                ChangeValueType(currentBlock, FindValue(blockData.symToValueMap, opnd->AsRegOpnd()->m_sym), valueType, false);
            }
        }
        opnd->SetValueType(valueType);

        if(!IsLoopPrePass() && opnd->IsSymOpnd() && valueType.IsDefinite())
        {LOGMEIN("GlobOpt.cpp] 6058\n");
            if (opnd->AsSymOpnd()->m_sym->IsPropertySym())
            {
                // A property sym can only be guaranteed to have a definite value type when implicit calls are disabled from the
                // point where the sym was defined with the definite value type. Insert an instruction to indicate to the
                // dead-store pass that implicit calls need to be kept disabled until after this instruction.
                Assert(DoFieldCopyProp());
                CaptureNoImplicitCallUses(opnd, false, instr);
            }
        }
    }
    else
    {
        opnd->SetValueType(ValueType::Uninitialized);
    }

    return val;
}

/*
*   GlobOpt::TryOptimizeInstrWithFixedDataProperty
*       Converts Ld[Root]Fld instr to
*            * CheckFixedFld
*            * Dst = Ld_A <int Constant value>
*   This API assumes that the source operand is a Sym/PropertySym kind.
*/
void
GlobOpt::TryOptimizeInstrWithFixedDataProperty(IR::Instr ** const pInstr)
{LOGMEIN("GlobOpt.cpp] 6086\n");
    Assert(pInstr);
    IR::Instr * &instr = *pInstr;
    IR::Opnd * src1 = instr->GetSrc1();
    Assert(src1 && src1->IsSymOpnd() && src1->AsSymOpnd()->IsPropertySymOpnd());

    if(PHASE_OFF(Js::UseFixedDataPropsPhase, instr->m_func))
    {LOGMEIN("GlobOpt.cpp] 6093\n");
        return;
    }

    if (!this->IsLoopPrePass() && !this->isRecursiveCallOnLandingPad &&
        OpCodeAttr::CanLoadFixedFields(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 6099\n");
        instr->TryOptimizeInstrWithFixedDataProperty(&instr, this);
    }
}

bool
GlobOpt::TransferSrcValue(IR::Instr * instr)
{LOGMEIN("GlobOpt.cpp] 6106\n");
    // Return whether the instruction transfers a value to the destination.
    // This is used to determine whether we should generate a value for the src so that it will
    // match with the dst for copy prop.

    // No point creating an unknown value for the src of a binary instr, as the dst will just be a different
    // Don't create value for instruction without dst as well. The value doesn't go anywhere.

    // if (src2 == nullptr) Disable copy prop for ScopedLdFld/ScopeStFld, etc., consider enabling that in the future
    // Consider: Add opcode attribute to indicate whether the opcode would use the value or not

    return instr->GetDst() != nullptr && instr->GetSrc2() == nullptr && !OpCodeAttr::DoNotTransfer(instr->m_opcode) && !instr->CallsAccessor();
}

Value*
GlobOpt::FindValue(Sym *sym)
{LOGMEIN("GlobOpt.cpp] 6122\n");
    return FindValue(this->blockData.symToValueMap, sym);
}

Value*
GlobOpt::FindValue(GlobHashTable *valueNumberMap, Sym *sym)
{LOGMEIN("GlobOpt.cpp] 6128\n");
    Assert(valueNumberMap);

    if (sym->IsStackSym() && sym->AsStackSym()->IsTypeSpec())
    {LOGMEIN("GlobOpt.cpp] 6132\n");
        sym = sym->AsStackSym()->GetVarEquivSym(this->func);
    }
    else if (sym->IsPropertySym())
    {LOGMEIN("GlobOpt.cpp] 6136\n");
        return FindPropertyValue(valueNumberMap, sym->m_id);
    }

    if (sym->IsStackSym() && sym->AsStackSym()->IsFromByteCodeConstantTable())
    {LOGMEIN("GlobOpt.cpp] 6141\n");
        return this->byteCodeConstantValueArray->Get(sym->m_id);
    }
    else
    {
        return FindValueFromHashTable(valueNumberMap, sym->m_id);
    }
}

ValueNumber
GlobOpt::FindValueNumber(GlobHashTable *valueNumberMap, Sym *sym)
{LOGMEIN("GlobOpt.cpp] 6152\n");
    Value *val = FindValue(valueNumberMap, sym);

    return val->GetValueNumber();
}

Value *
GlobOpt::FindPropertyValue(GlobHashTable *valueNumberMap, SymID symId)
{LOGMEIN("GlobOpt.cpp] 6160\n");
    Assert(this->func->m_symTable->Find(symId)->IsPropertySym());
    if (!this->blockData.liveFields->Test(symId))
    {LOGMEIN("GlobOpt.cpp] 6163\n");
        Assert(!IsHoistablePropertySym(symId));
        return nullptr;
    }
    return FindValueFromHashTable(valueNumberMap, symId);
}

ValueNumber
GlobOpt::FindPropertyValueNumber(GlobHashTable *valueNumberMap, SymID symId)
{LOGMEIN("GlobOpt.cpp] 6172\n");
    Value *val = FindPropertyValue(valueNumberMap, symId);

    return val->GetValueNumber();
}

Value *
GlobOpt::FindObjectTypeValue(StackSym* typeSym)
{LOGMEIN("GlobOpt.cpp] 6180\n");
    return FindObjectTypeValue(typeSym, this->blockData.symToValueMap);
}

Value *
GlobOpt::FindObjectTypeValue(StackSym* typeSym, BasicBlock* block)
{LOGMEIN("GlobOpt.cpp] 6186\n");
    return FindObjectTypeValue(typeSym->m_id, block);
}

Value *
GlobOpt::FindObjectTypeValue(SymID typeSymId, BasicBlock* block)
{LOGMEIN("GlobOpt.cpp] 6192\n");
    return FindObjectTypeValue(typeSymId, block->globOptData.symToValueMap, block->globOptData.liveFields);
}

Value *
GlobOpt::FindObjectTypeValue(StackSym* typeSym, GlobHashTable *valueNumberMap)
{LOGMEIN("GlobOpt.cpp] 6198\n");
    return FindObjectTypeValue(typeSym->m_id, valueNumberMap);
}

Value *
GlobOpt::FindObjectTypeValue(SymID typeSymId, GlobHashTable *valueNumberMap)
{LOGMEIN("GlobOpt.cpp] 6204\n");
    return FindObjectTypeValue(typeSymId, valueNumberMap, this->blockData.liveFields);
}

Value *
GlobOpt::FindObjectTypeValue(StackSym* typeSym, GlobHashTable *valueNumberMap, BVSparse<JitArenaAllocator>* liveFields)
{LOGMEIN("GlobOpt.cpp] 6210\n");
    return FindObjectTypeValue(typeSym->m_id, valueNumberMap, liveFields);
}

Value *
GlobOpt::FindObjectTypeValue(SymID typeSymId, GlobHashTable *valueNumberMap, BVSparse<JitArenaAllocator>* liveFields)
{LOGMEIN("GlobOpt.cpp] 6216\n");
    Assert(this->func->m_symTable->Find(typeSymId)->IsStackSym());
    if (!liveFields->Test(typeSymId))
    {LOGMEIN("GlobOpt.cpp] 6219\n");
        return nullptr;
    }
    Value* value = FindValueFromHashTable(valueNumberMap, typeSymId);
    Assert(value == nullptr || value->GetValueInfo()->IsJsType());
    return value;
}

Value *
GlobOpt::FindFuturePropertyValue(PropertySym *const propertySym)
{LOGMEIN("GlobOpt.cpp] 6229\n");
    Assert(propertySym);

    // Try a direct lookup based on this sym
    Value *const value = FindValue(propertySym);
    if(value)
    {LOGMEIN("GlobOpt.cpp] 6235\n");
        return value;
    }

    if(PHASE_OFF(Js::CopyPropPhase, func))
    {LOGMEIN("GlobOpt.cpp] 6240\n");
        // Need to use copy-prop info to backtrack
        return nullptr;
    }

    // Try to get the property object's value
    StackSym *const objectSym = propertySym->m_stackSym;
    Value *objectValue = FindValue(objectSym);
    if(!objectValue)
    {LOGMEIN("GlobOpt.cpp] 6249\n");
        if(!objectSym->IsSingleDef())
        {LOGMEIN("GlobOpt.cpp] 6251\n");
            return nullptr;
        }

        switch(objectSym->m_instrDef->m_opcode)
        {LOGMEIN("GlobOpt.cpp] 6256\n");
        case Js::OpCode::Ld_A:
        case Js::OpCode::LdSlotArr:
        case Js::OpCode::LdSlot:
            // Allow only these op-codes for tracking the object sym's value transfer backwards. Other transfer op-codes
            // could be included here if this function is used in scenarios that need them.
            break;

        default:
            return nullptr;
        }

        // Try to get the property object's value from the src of the definition
        IR::Opnd *const objectTransferSrc = objectSym->m_instrDef->GetSrc1();
        if(!objectTransferSrc)
        {LOGMEIN("GlobOpt.cpp] 6271\n");
            return nullptr;
        }
        if(objectTransferSrc->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 6275\n");
            objectValue = FindValue(objectTransferSrc->AsRegOpnd()->m_sym);
        }
        else if(objectTransferSrc->IsSymOpnd())
        {LOGMEIN("GlobOpt.cpp] 6279\n");
            Sym *const objectTransferSrcSym = objectTransferSrc->AsSymOpnd()->m_sym;
            if(objectTransferSrcSym->IsStackSym())
            {LOGMEIN("GlobOpt.cpp] 6282\n");
                objectValue = FindValue(objectTransferSrcSym);
            }
            else
            {
                // About to make a recursive call, so when jitting in the foreground, probe the stack
                if(!func->IsBackgroundJIT())
                {LOGMEIN("GlobOpt.cpp] 6289\n");
                    PROBE_STACK(func->GetScriptContext(), Js::Constants::MinStackDefault);
                }
                objectValue = FindFuturePropertyValue(objectTransferSrcSym->AsPropertySym());
            }
        }
        else
        {
            return nullptr;
        }
        if(!objectValue)
        {LOGMEIN("GlobOpt.cpp] 6300\n");
            return nullptr;
        }
    }

    // Try to use the property object's copy-prop sym and the property ID to find a mapped property sym, and get its value
    StackSym *const objectCopyPropSym = GetCopyPropSym(nullptr, objectValue);
    if(!objectCopyPropSym)
    {LOGMEIN("GlobOpt.cpp] 6308\n");
        return nullptr;
    }
    PropertySym *const propertyCopyPropSym = PropertySym::Find(objectCopyPropSym->m_id, propertySym->m_propertyId, func);
    if(!propertyCopyPropSym)
    {LOGMEIN("GlobOpt.cpp] 6313\n");
        return nullptr;
    }
    return FindValue(propertyCopyPropSym);
}

Value *
GlobOpt::FindValueFromHashTable(GlobHashTable *valueNumberMap, SymID symId)
{LOGMEIN("GlobOpt.cpp] 6321\n");
    Value ** valuePtr = valueNumberMap->Get(symId);

    if (valuePtr == nullptr)
    {LOGMEIN("GlobOpt.cpp] 6325\n");
        return 0;
    }

    return (*valuePtr);
}

StackSym *
GlobOpt::GetCopyPropSym(Sym * sym, Value * value)
{LOGMEIN("GlobOpt.cpp] 6334\n");
    return GetCopyPropSym(this->currentBlock, sym, value);
}

StackSym *
GlobOpt::GetCopyPropSym(BasicBlock * block, Sym * sym, Value * value)
{LOGMEIN("GlobOpt.cpp] 6340\n");
    ValueInfo *valueInfo = value->GetValueInfo();
    Sym * copySym = valueInfo->GetSymStore();

    if (!copySym)
    {LOGMEIN("GlobOpt.cpp] 6345\n");
        return nullptr;
    }

    // Only copy prop stackSym, as a propertySym wouldn't improve anything.
    // SingleDef info isn't flow sensitive, so make sure the symbol is actually live.
    if (copySym->IsStackSym() && copySym != sym)
    {LOGMEIN("GlobOpt.cpp] 6352\n");
        Assert(!copySym->AsStackSym()->IsTypeSpec());
        Value *copySymVal = this->FindValue(block->globOptData.symToValueMap, valueInfo->GetSymStore());
        if (copySymVal && copySymVal->GetValueNumber() == value->GetValueNumber())
        {LOGMEIN("GlobOpt.cpp] 6356\n");
            if (valueInfo->IsVarConstant() && !GlobOpt::IsLive(copySym, block))
            {LOGMEIN("GlobOpt.cpp] 6358\n");
                // Because the addrConstantToValueMap isn't flow-based, the symStore of
                // varConstants may not be live.
                return nullptr;
            }
            return copySym->AsStackSym();
        }
    }
    return nullptr;

}

// Constant prop if possible, otherwise if this value already resides in another
// symbol, reuse this previous symbol. This should help register allocation.
IR::Opnd *
GlobOpt::CopyProp(IR::Opnd *opnd, IR::Instr *instr, Value *val, IR::IndirOpnd *parentIndirOpnd)
{LOGMEIN("GlobOpt.cpp] 6374\n");
    Assert(
        parentIndirOpnd
            ? opnd == parentIndirOpnd->GetBaseOpnd() || opnd == parentIndirOpnd->GetIndexOpnd()
            : opnd == instr->GetSrc1() || opnd == instr->GetSrc2() || opnd == instr->GetDst() && opnd->IsIndirOpnd());

    if (this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 6381\n");
        // Transformations are not legal in prepass...
        return opnd;
    }

    if (!this->func->DoGlobOptsForGeneratorFunc())
    {LOGMEIN("GlobOpt.cpp] 6387\n");
        // Don't copy prop in generator functions because non-bytecode temps that span a yield
        // cannot be saved and restored by the current bail-out mechanics utilized by generator
        // yield/resume.
        // TODO[generators][ianhall]: Enable copy-prop at least for in between yields.
        return opnd;
    }

    if (instr->m_opcode == Js::OpCode::CheckFixedFld || instr->m_opcode == Js::OpCode::CheckPropertyGuardAndLoadType)
    {LOGMEIN("GlobOpt.cpp] 6396\n");
        // Don't copy prop into CheckFixedFld or CheckPropertyGuardAndLoadType
        return opnd;
    }

    // Don't copy-prop link operands of ExtendedArgs
    if (instr->m_opcode == Js::OpCode::ExtendArg_A && opnd == instr->GetSrc2())
    {LOGMEIN("GlobOpt.cpp] 6403\n");
        return opnd;
    }

    // Don't copy-prop operand of SIMD instr with ExtendedArg operands. Each instr should have its exclusive EA sequence.
    if (
            Js::IsSimd128Opcode(instr->m_opcode) &&
            instr->GetSrc1() != nullptr &&
            instr->GetSrc1()->IsRegOpnd() &&
            instr->GetSrc2() == nullptr
       )
    {LOGMEIN("GlobOpt.cpp] 6414\n");
        StackSym *sym = instr->GetSrc1()->GetStackSym();
        if (sym && sym->IsSingleDef() && sym->GetInstrDef()->m_opcode == Js::OpCode::ExtendArg_A)
        {LOGMEIN("GlobOpt.cpp] 6417\n");
                return opnd;
        }
    }

    ValueInfo *valueInfo = val->GetValueInfo();

    // Constant prop?
    int32 intConstantValue;
    if (valueInfo->TryGetIntConstantValue(&intConstantValue))
    {LOGMEIN("GlobOpt.cpp] 6427\n");
        if (PHASE_OFF(Js::ConstPropPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 6429\n");
            return opnd;
        }

        if ((
                instr->m_opcode == Js::OpCode::StElemI_A ||
                instr->m_opcode == Js::OpCode::StElemI_A_Strict ||
                instr->m_opcode == Js::OpCode::StElemC
            ) && instr->GetSrc1() == opnd)
        {LOGMEIN("GlobOpt.cpp] 6438\n");
            // Disabling prop to src of native array store, because we were losing the chance to type specialize.
            // Is it possible to type specialize this src if we allow constants, etc., to be prop'd here?
            if (instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetValueType().IsLikelyNativeArray())
            {LOGMEIN("GlobOpt.cpp] 6442\n");
                return opnd;
            }
        }

        if(opnd != instr->GetSrc1() && opnd != instr->GetSrc2())
        {LOGMEIN("GlobOpt.cpp] 6448\n");
            if(PHASE_OFF(Js::IndirCopyPropPhase, instr->m_func))
            {LOGMEIN("GlobOpt.cpp] 6450\n");
                return opnd;
            }

            // Const-prop an indir opnd's constant index into its offset
            IR::Opnd *srcs[] = { instr->GetSrc1(), instr->GetSrc2(), instr->GetDst() };
            for(int i = 0; i < sizeof(srcs) / sizeof(srcs[0]); ++i)
            {LOGMEIN("GlobOpt.cpp] 6457\n");
                const auto src = srcs[i];
                if(!src || !src->IsIndirOpnd())
                {LOGMEIN("GlobOpt.cpp] 6460\n");
                    continue;
                }

                const auto indir = src->AsIndirOpnd();
                if(opnd == indir->GetIndexOpnd())
                {LOGMEIN("GlobOpt.cpp] 6466\n");
                    Assert(indir->GetScale() == 0);
                    GOPT_TRACE_OPND(opnd, _u("Constant prop indir index into offset (value: %d)\n"), intConstantValue);
                    this->CaptureByteCodeSymUses(instr);
                    indir->SetOffset(intConstantValue);
                    indir->SetIndexOpnd(nullptr);
                }
            }

            return opnd;
        }

        if (Js::TaggedInt::IsOverflow(intConstantValue))
        {LOGMEIN("GlobOpt.cpp] 6479\n");
            return opnd;
        }

        IR::Opnd *constOpnd;

        if (opnd->IsVar())
        {LOGMEIN("GlobOpt.cpp] 6486\n");
            IR::AddrOpnd *addrOpnd = IR::AddrOpnd::New(Js::TaggedInt::ToVarUnchecked((int)intConstantValue), IR::AddrOpndKindConstantVar, instr->m_func);

            GOPT_TRACE_OPND(opnd, _u("Constant prop %d (value:%d)\n"), addrOpnd->m_address, intConstantValue);
            constOpnd = addrOpnd;
        }
        else
        {
            // Note: Jit loop body generates some i32 operands...
            Assert(opnd->IsInt32() || opnd->IsInt64() || opnd->IsUInt32());
            IRType opndType;
            IntConstType constVal;
            if (opnd->IsUInt32())
            {LOGMEIN("GlobOpt.cpp] 6499\n");
                // avoid sign extension
                constVal = (uint32)intConstantValue;
                opndType = TyUint32;
            }
            else
            {
                constVal = intConstantValue;
                opndType = TyInt32;
            }
            IR::IntConstOpnd *intOpnd = IR::IntConstOpnd::New(constVal, opndType, instr->m_func);

            GOPT_TRACE_OPND(opnd, _u("Constant prop %d (value:%d)\n"), intOpnd->GetImmediateValue(instr->m_func), intConstantValue);
            constOpnd = intOpnd;
        }

#if ENABLE_DEBUG_CONFIG_OPTIONS
        //Need to update DumpFieldCopyPropTestTrace for every new opcode that is added for fieldcopyprop
        if(Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::FieldCopyPropPhase))
        {LOGMEIN("GlobOpt.cpp] 6518\n");
            instr->DumpFieldCopyPropTestTrace();
        }
#endif

        this->CaptureByteCodeSymUses(instr);
        opnd = instr->ReplaceSrc(opnd, constOpnd);

        switch (instr->m_opcode)
        {LOGMEIN("GlobOpt.cpp] 6527\n");
        case Js::OpCode::LdSlot:
        case Js::OpCode::LdSlotArr:
        case Js::OpCode::LdFld:
        case Js::OpCode::LdFldForTypeOf:
        case Js::OpCode::LdRootFldForTypeOf:
        case Js::OpCode::LdFldForCallApplyTarget:
        case Js::OpCode::LdRootFld:
        case Js::OpCode::LdMethodFld:
        case Js::OpCode::LdRootMethodFld:
        case Js::OpCode::LdMethodFromFlags:
        case Js::OpCode::ScopedLdMethodFld:
            instr->m_opcode = Js::OpCode::Ld_A;
        case Js::OpCode::Ld_A:
            {LOGMEIN("GlobOpt.cpp] 6541\n");
                IR::Opnd * dst = instr->GetDst();
                if (dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym->IsSingleDef())
                {LOGMEIN("GlobOpt.cpp] 6544\n");
                    dst->AsRegOpnd()->m_sym->SetIsIntConst((int)intConstantValue);
                }
                break;
            }
        case Js::OpCode::ArgOut_A:
        case Js::OpCode::ArgOut_A_Inline:
        case Js::OpCode::ArgOut_A_FixupForStackArgs:
        case Js::OpCode::ArgOut_A_InlineBuiltIn:

            if (instr->GetDst()->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 6555\n");
                Assert(instr->GetDst()->AsRegOpnd()->m_sym->m_isSingleDef);
                instr->GetDst()->AsRegOpnd()->m_sym->AsStackSym()->SetIsIntConst((int)intConstantValue);
            }
            else
            {
                instr->GetDst()->AsSymOpnd()->m_sym->AsStackSym()->SetIsIntConst((int)intConstantValue);
            }
            break;

        case Js::OpCode::TypeofElem:
            instr->m_opcode = Js::OpCode::Typeof;
            break;

        case Js::OpCode::StSlotChkUndecl:
            if (instr->GetSrc2() == opnd)
            {LOGMEIN("GlobOpt.cpp] 6571\n");
                // Src2 here should refer to the same location as the Dst operand, which we need to keep live
                // due to the implicit read for ChkUndecl.
                instr->m_opcode = Js::OpCode::StSlot;
                instr->FreeSrc2();
                opnd = nullptr;
            }
            break;
        }
        return opnd;
    }

    Sym *opndSym = nullptr;
    if (opnd->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 6585\n");
        IR::RegOpnd *regOpnd = opnd->AsRegOpnd();
        opndSym = regOpnd->m_sym;
    }
    else if (opnd->IsSymOpnd())
    {LOGMEIN("GlobOpt.cpp] 6590\n");
        IR::SymOpnd *symOpnd = opnd->AsSymOpnd();
        opndSym = symOpnd->m_sym;
    }
    if (!opndSym)
    {LOGMEIN("GlobOpt.cpp] 6595\n");
        return opnd;
    }

    if (PHASE_OFF(Js::CopyPropPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 6600\n");
        this->SetSymStoreDirect(valueInfo, opndSym);
        return opnd;
    }

    // We should have dealt with field hoist already
    Assert(!GlobOpt::TransferSrcValue(instr) || !opndSym->IsPropertySym() ||
        !this->IsHoistedPropertySym(opndSym->AsPropertySym()));

    StackSym *copySym = this->GetCopyPropSym(opndSym, val);
    if (copySym != nullptr)
    {LOGMEIN("GlobOpt.cpp] 6611\n");
        // Copy prop.
        return CopyPropReplaceOpnd(instr, opnd, copySym, parentIndirOpnd);
    }
    else
    {
        if (valueInfo->GetSymStore() && instr->m_opcode == Js::OpCode::Ld_A && instr->GetDst()->IsRegOpnd()
            && valueInfo->GetSymStore() == instr->GetDst()->AsRegOpnd()->m_sym)
        {LOGMEIN("GlobOpt.cpp] 6619\n");
            // Avoid resetting symStore after fieldHoisting:
            //  t1 = LdFld field            <- set symStore to fieldHoistSym
            //   fieldHoistSym = Ld_A t1    <- we're looking at t1 now, but want to copy-prop fieldHoistSym forward
            return opnd;
        }
        this->SetSymStoreDirect(valueInfo, opndSym);
    }
    return opnd;
}

IR::Opnd *
GlobOpt::CopyPropReplaceOpnd(IR::Instr * instr, IR::Opnd * opnd, StackSym * copySym, IR::IndirOpnd *parentIndirOpnd)
{LOGMEIN("GlobOpt.cpp] 6632\n");
    Assert(
        parentIndirOpnd
            ? opnd == parentIndirOpnd->GetBaseOpnd() || opnd == parentIndirOpnd->GetIndexOpnd()
            : opnd == instr->GetSrc1() || opnd == instr->GetSrc2() || opnd == instr->GetDst() && opnd->IsIndirOpnd());
    Assert(GlobOpt::IsLive(copySym, this->currentBlock));

    IR::RegOpnd *regOpnd;
    StackSym *newSym = copySym;

    GOPT_TRACE_OPND(opnd, _u("Copy prop s%d\n"), newSym->m_id);
#if ENABLE_DEBUG_CONFIG_OPTIONS
    //Need to update DumpFieldCopyPropTestTrace for every new opcode that is added for fieldcopyprop
    if(Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::FieldCopyPropPhase))
    {LOGMEIN("GlobOpt.cpp] 6646\n");
        instr->DumpFieldCopyPropTestTrace();
    }
#endif

    this->CaptureByteCodeSymUses(instr);
    if (opnd->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 6653\n");
        regOpnd = opnd->AsRegOpnd();
        regOpnd->m_sym = newSym;
        regOpnd->SetIsJITOptimizedReg(true);

        // The dead bit on the opnd is specific to the sym it is referencing. Since we replaced the sym, the bit is reset.
        regOpnd->SetIsDead(false);

        if(parentIndirOpnd)
        {LOGMEIN("GlobOpt.cpp] 6662\n");
            return regOpnd;
        }
    }
    else
    {
        // If this is an object type specialized field load inside a loop, and it produces a type value which wasn't live
        // before, make sure the type check is left in the loop, because it may be the last type check in the loop protecting
        // other fields which are not hoistable and are lexically upstream in the loop.  If the check is not ultimately
        // needed, the dead store pass will remove it.
        if (this->currentBlock->loop != nullptr && opnd->IsSymOpnd() && opnd->AsSymOpnd()->IsPropertySymOpnd())
        {LOGMEIN("GlobOpt.cpp] 6673\n");
            IR::PropertySymOpnd* propertySymOpnd = opnd->AsPropertySymOpnd();
            if (CheckIfPropOpEmitsTypeCheck(instr, propertySymOpnd))
            {LOGMEIN("GlobOpt.cpp] 6676\n");
                // We only set guarded properties in the dead store pass, so they shouldn't be set here yet. If they were
                // we would need to move them from this operand to the operand which is being copy propagated.
                Assert(propertySymOpnd->GetGuardedPropOps() == nullptr);

                // We're creating a copy of this operand to be reused in the same spot in the flow, so we can copy all
                // flow sensitive fields.  However, we will do only a type check here (no property access) and only for
                // the sake of downstream instructions, so the flags pertaining to this property access are irrelevant.
                IR::PropertySymOpnd* checkObjTypeOpnd = CreateOpndForTypeCheckOnly(propertySymOpnd, instr->m_func);
                IR::Instr* checkObjTypeInstr = IR::Instr::New(Js::OpCode::CheckObjType, instr->m_func);
                checkObjTypeInstr->SetSrc1(checkObjTypeOpnd);
                checkObjTypeInstr->SetByteCodeOffset(instr);
                instr->InsertBefore(checkObjTypeInstr);

                // Since we inserted this instruction before the one that is being processed in natural flow, we must process
                // it for object type spec explicitly here.
                FinishOptPropOp(checkObjTypeInstr, checkObjTypeOpnd);
                Assert(!propertySymOpnd->IsTypeChecked());
                checkObjTypeInstr = this->SetTypeCheckBailOut(checkObjTypeOpnd, checkObjTypeInstr, nullptr);
                Assert(checkObjTypeInstr->HasBailOutInfo());

                if (this->currentBlock->loop && !this->IsLoopPrePass())
                {LOGMEIN("GlobOpt.cpp] 6698\n");
                    // Try hoisting this checkObjType.
                    // But since this isn't the current instr being optimized, we need to play tricks with
                    // the byteCodeUse fields...
                    BVSparse<JitArenaAllocator> *currentBytecodeUses = this->byteCodeUses;
                    PropertySym * currentPropertySymUse = this->propertySymUse;
                    PropertySym * tempPropertySymUse = NULL;
                    this->byteCodeUses = NULL;
                    BVSparse<JitArenaAllocator> *tempByteCodeUse = JitAnew(this->tempAlloc, BVSparse<JitArenaAllocator>, this->tempAlloc);
#if DBG
                    BVSparse<JitArenaAllocator> *currentBytecodeUsesBeforeOpt = this->byteCodeUsesBeforeOpt;
                    this->byteCodeUsesBeforeOpt = tempByteCodeUse;
#endif
                    this->propertySymUse = NULL;
                    GlobOpt::TrackByteCodeSymUsed(checkObjTypeInstr, tempByteCodeUse, &tempPropertySymUse);

                    TryHoistInvariant(checkObjTypeInstr, this->currentBlock, NULL, this->FindValue(copySym), NULL, true);

                    this->byteCodeUses = currentBytecodeUses;
                    this->propertySymUse = currentPropertySymUse;
#if DBG
                    this->byteCodeUsesBeforeOpt = currentBytecodeUsesBeforeOpt;
#endif
                }
            }
        }

        if (opnd->IsSymOpnd() && opnd->GetIsDead())
        {LOGMEIN("GlobOpt.cpp] 6726\n");
            // Take the property sym out of the live fields set
            this->EndFieldLifetime(opnd->AsSymOpnd());
        }
        regOpnd = IR::RegOpnd::New(newSym, opnd->GetType(), instr->m_func);
        regOpnd->SetIsJITOptimizedReg(true);
        instr->ReplaceSrc(opnd, regOpnd);
    }

    switch (instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 6736\n");
    case Js::OpCode::Ld_A:
        if (instr->GetDst()->IsRegOpnd() && instr->GetSrc1()->IsRegOpnd() &&
            instr->GetDst()->AsRegOpnd()->GetStackSym() == instr->GetSrc1()->AsRegOpnd()->GetStackSym())
        {LOGMEIN("GlobOpt.cpp] 6740\n");
            this->InsertByteCodeUses(instr, true);
            instr->m_opcode = Js::OpCode::Nop;
        }
        break;

    case Js::OpCode::LdSlot:
    case Js::OpCode::LdSlotArr:
        if (instr->GetDst()->IsRegOpnd() && instr->GetSrc1()->IsRegOpnd() &&
            instr->GetDst()->AsRegOpnd()->GetStackSym() == instr->GetSrc1()->AsRegOpnd()->GetStackSym())
        {LOGMEIN("GlobOpt.cpp] 6750\n");
            this->InsertByteCodeUses(instr, true);
            instr->m_opcode = Js::OpCode::Nop;
        }
        else
        {
            instr->m_opcode = Js::OpCode::Ld_A;
        }
        break;

    case Js::OpCode::StSlotChkUndecl:
        if (instr->GetSrc2()->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 6762\n");
            // Src2 here should refer to the same location as the Dst operand, which we need to keep live
            // due to the implicit read for ChkUndecl.
            instr->m_opcode = Js::OpCode::StSlot;
            instr->FreeSrc2();
            return nullptr;
        }
        break;

    case Js::OpCode::LdFld:
    case Js::OpCode::LdFldForTypeOf:
    case Js::OpCode::LdRootFldForTypeOf:
    case Js::OpCode::LdFldForCallApplyTarget:
    case Js::OpCode::LdRootFld:
    case Js::OpCode::LdMethodFld:
    case Js::OpCode::LdRootMethodFld:
    case Js::OpCode::ScopedLdMethodFld:
        instr->m_opcode = Js::OpCode::Ld_A;
        break;

    case Js::OpCode::LdMethodFromFlags:
        // The bailout is checked on the loop top and we don't need to check bailout again in loop.
        instr->m_opcode = Js::OpCode::Ld_A;
        instr->ClearBailOutInfo();
        break;

    case Js::OpCode::TypeofElem:
        instr->m_opcode = Js::OpCode::Typeof;
        break;
    }
    this->MarkTempLastUse(instr, regOpnd);

    return regOpnd;
}

void
GlobOpt::MarkTempLastUse(IR::Instr *instr, IR::RegOpnd *regOpnd)
{LOGMEIN("GlobOpt.cpp] 6799\n");
    if (OpCodeAttr::NonTempNumberSources(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 6801\n");
        // Turn off bit if opcode could cause the src to be aliased.
        this->blockData.isTempSrc->Clear(regOpnd->m_sym->m_id);
    }
    else if (this->blockData.isTempSrc->Test(regOpnd->m_sym->m_id))
    {LOGMEIN("GlobOpt.cpp] 6806\n");
        // We just mark things that are temp in the globopt phase.
        // The backwards phase will turn this off if it is not the last use.
        // The isTempSrc is freed at the end of each block, which is why the backwards phase can't
        // just use it.
        if (!PHASE_OFF(Js::BackwardPhase, this->func) && !this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 6812\n");
            regOpnd->m_isTempLastUse = true;
        }
    }
}

ValueNumber
GlobOpt::NewValueNumber()
{LOGMEIN("GlobOpt.cpp] 6820\n");
    ValueNumber valueNumber = this->currentValue++;

    if (valueNumber == 0)
    {LOGMEIN("GlobOpt.cpp] 6824\n");
        Js::Throw::OutOfMemory();
    }
    return valueNumber;
}

Value *GlobOpt::NewValue(ValueInfo *const valueInfo)
{LOGMEIN("GlobOpt.cpp] 6831\n");
    return NewValue(NewValueNumber(), valueInfo);
}

Value *GlobOpt::NewValue(const ValueNumber valueNumber, ValueInfo *const valueInfo)
{LOGMEIN("GlobOpt.cpp] 6836\n");
    Assert(valueInfo);

    return Value::New(alloc, valueNumber, valueInfo);
}

Value *GlobOpt::CopyValue(Value *const value)
{LOGMEIN("GlobOpt.cpp] 6843\n");
    return CopyValue(value, NewValueNumber());
}

Value *GlobOpt::CopyValue(Value *const value, const ValueNumber valueNumber)
{LOGMEIN("GlobOpt.cpp] 6848\n");
    Assert(value);

    return value->Copy(alloc, valueNumber);
}

Value *
GlobOpt::NewGenericValue(const ValueType valueType)
{LOGMEIN("GlobOpt.cpp] 6856\n");
    return NewGenericValue(valueType, static_cast<IR::Opnd *>(nullptr));
}

Value *
GlobOpt::NewGenericValue(const ValueType valueType, IR::Opnd *const opnd)
{LOGMEIN("GlobOpt.cpp] 6862\n");
    // Shouldn't assign a likely-int value to something that is definitely not an int
    Assert(!(valueType.IsLikelyInt() && opnd && opnd->IsRegOpnd() && opnd->AsRegOpnd()->m_sym->m_isNotInt));

    ValueInfo *valueInfo = ValueInfo::New(this->alloc, valueType);
    Value *val = NewValue(valueInfo);
    TrackNewValueForKills(val);

    this->InsertNewValue(val, opnd);
    return val;
}

Value *
GlobOpt::NewGenericValue(const ValueType valueType, Sym *const sym)
{LOGMEIN("GlobOpt.cpp] 6876\n");
    ValueInfo *valueInfo = ValueInfo::New(this->alloc, valueType);
    Value *val = NewValue(valueInfo);
    TrackNewValueForKills(val);

    this->SetValue(&this->blockData, val, sym);
    return val;
}

Value *
GlobOpt::GetIntConstantValue(const int32 intConst, IR::Instr * instr, IR::Opnd *const opnd)
{LOGMEIN("GlobOpt.cpp] 6887\n");
    Value *value = nullptr;
    Value *const cachedValue = this->intConstantToValueMap->Lookup(intConst, nullptr);

    if(cachedValue)
    {LOGMEIN("GlobOpt.cpp] 6892\n");
        // The cached value could be from a different block since this is a global (as opposed to a per-block) cache. Since
        // values are cloned for each block, we can't use the same value object. We also can't have two values with the same
        // number in one block, so we can't simply copy the cached value either. And finally, there is no deterministic and fast
        // way to determine if a value with the same value number exists for this block. So the best we can do with a global
        // cache is to check the sym-store's value in the current block to see if it has a value with the same number.
        // Otherwise, we have to create a new value with a new value number.
        Sym *const symStore = cachedValue->GetValueInfo()->GetSymStore();
        if (symStore && IsLive(symStore, &blockData))
        {LOGMEIN("GlobOpt.cpp] 6901\n");

            Value *const symStoreValue = FindValue(symStore);
            int32 symStoreIntConstantValue;
            if (symStoreValue &&
                symStoreValue->GetValueNumber() == cachedValue->GetValueNumber() &&
                symStoreValue->GetValueInfo()->TryGetIntConstantValue(&symStoreIntConstantValue) &&
                symStoreIntConstantValue == intConst)
            {LOGMEIN("GlobOpt.cpp] 6909\n");
                value = symStoreValue;
            }
        }
    }

    if (!value)
    {LOGMEIN("GlobOpt.cpp] 6916\n");
        value = NewIntConstantValue(intConst, instr, !Js::TaggedInt::IsOverflow(intConst));
    }

    return this->InsertNewValue(value, opnd);
}

Value *
GlobOpt::NewIntConstantValue(const int32 intConst, IR::Instr * instr, bool isTaggable)
{LOGMEIN("GlobOpt.cpp] 6925\n");
    Value * value = NewValue(IntConstantValueInfo::New(this->alloc, intConst));
    this->intConstantToValueMap->Item(intConst, value);

    if (isTaggable &&
        !PHASE_OFF(Js::HoistConstIntPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 6931\n");
        // When creating a new int constant value, make sure it gets a symstore. If the int const doesn't have a symstore,
        // any downstream instruction using the same int will have to create a new value (object) for the int.
        // This gets in the way of CSE.
        value = HoistConstantLoadAndPropagateValueBackward(Js::TaggedInt::ToVarUnchecked(intConst), instr, value);
        if (!value->GetValueInfo()->GetSymStore() &&
            (instr->m_opcode == Js::OpCode::LdC_A_I4 || instr->m_opcode == Js::OpCode::Ld_I4))
        {LOGMEIN("GlobOpt.cpp] 6938\n");
            StackSym * sym = instr->GetDst()->GetStackSym();
            Assert(sym);
            if (sym->IsTypeSpec())
            {LOGMEIN("GlobOpt.cpp] 6942\n");
                Assert(sym->IsInt32());
                StackSym * varSym = sym->GetVarEquivSym(instr->m_func);
                SetValue(&this->currentBlock->globOptData, value, varSym);
                this->currentBlock->globOptData.liveInt32Syms->Set(varSym->m_id);
            }
            else
            {
                SetValue(&this->currentBlock->globOptData, value, sym);
                this->currentBlock->globOptData.liveVarSyms->Set(sym->m_id);
            }
        }
    }
    return value;
}

ValueInfo *
GlobOpt::NewIntRangeValueInfo(const int32 min, const int32 max, const bool wasNegativeZeroPreventedByBailout)
{LOGMEIN("GlobOpt.cpp] 6960\n");
    if (min == max)
    {LOGMEIN("GlobOpt.cpp] 6962\n");
        // Since int constant values are const-propped, negative zero tracking does not track them, and so it's okay to ignore
        // 'wasNegativeZeroPreventedByBailout'
        return IntConstantValueInfo::New(this->alloc, max);
    }

    return IntRangeValueInfo::New(this->alloc, min, max, wasNegativeZeroPreventedByBailout);
}

ValueInfo *GlobOpt::NewIntRangeValueInfo(
    const ValueInfo *const originalValueInfo,
    const int32 min,
    const int32 max) const
{LOGMEIN("GlobOpt.cpp] 6975\n");
    Assert(originalValueInfo);

    ValueInfo *valueInfo;
    if(min == max)
    {LOGMEIN("GlobOpt.cpp] 6980\n");
        // Since int constant values are const-propped, negative zero tracking does not track them, and so it's okay to ignore
        // 'wasNegativeZeroPreventedByBailout'
        valueInfo = IntConstantValueInfo::New(alloc, min);
    }
    else
    {
        valueInfo =
            IntRangeValueInfo::New(
                alloc,
                min,
                max,
                min <= 0 && max >= 0 && originalValueInfo->WasNegativeZeroPreventedByBailout());
    }
    valueInfo->SetSymStore(originalValueInfo->GetSymStore());
    return valueInfo;
}

Value *
GlobOpt::NewIntRangeValue(
    const int32 min,
    const int32 max,
    const bool wasNegativeZeroPreventedByBailout,
    IR::Opnd *const opnd)
{LOGMEIN("GlobOpt.cpp] 7004\n");
    ValueInfo *valueInfo = this->NewIntRangeValueInfo(min, max, wasNegativeZeroPreventedByBailout);
    Value *val = NewValue(valueInfo);

    if (opnd)
    {
        GOPT_TRACE_OPND(opnd, _u("Range %d (0x%X) to %d (0x%X)\n"), min, min, max, max);
    }
    this->InsertNewValue(val, opnd);
    return val;
}

IntBoundedValueInfo *GlobOpt::NewIntBoundedValueInfo(
    const ValueInfo *const originalValueInfo,
    const IntBounds *const bounds) const
{LOGMEIN("GlobOpt.cpp] 7019\n");
    Assert(originalValueInfo);
    bounds->Verify();

    IntBoundedValueInfo *const valueInfo =
        IntBoundedValueInfo::New(
            originalValueInfo->Type(),
            bounds,
            (
                bounds->ConstantLowerBound() <= 0 &&
                bounds->ConstantUpperBound() >= 0 &&
                originalValueInfo->WasNegativeZeroPreventedByBailout()
            ),
            alloc);
    valueInfo->SetSymStore(originalValueInfo->GetSymStore());
    return valueInfo;
}

Value *GlobOpt::NewIntBoundedValue(
    const ValueType valueType,
    const IntBounds *const bounds,
    const bool wasNegativeZeroPreventedByBailout,
    IR::Opnd *const opnd)
{LOGMEIN("GlobOpt.cpp] 7042\n");
    Value *const value = NewValue(IntBoundedValueInfo::New(valueType, bounds, wasNegativeZeroPreventedByBailout, alloc));
    InsertNewValue(value, opnd);
    return value;
}

Value *
GlobOpt::NewFloatConstantValue(const FloatConstType floatValue, IR::Opnd *const opnd)
{LOGMEIN("GlobOpt.cpp] 7050\n");
    FloatConstantValueInfo *valueInfo = FloatConstantValueInfo::New(this->alloc, floatValue);
    Value *val = NewValue(valueInfo);

    this->InsertNewValue(val, opnd);
    return val;
}

Value *
GlobOpt::GetVarConstantValue(IR::AddrOpnd *addrOpnd)
{LOGMEIN("GlobOpt.cpp] 7060\n");
    bool isVar = addrOpnd->IsVar();
    bool isString = isVar && addrOpnd->m_localAddress && JITJavascriptString::Is(addrOpnd->m_localAddress);
    Value *val = nullptr;
    Value *cachedValue;
    if(this->addrConstantToValueMap->TryGetValue(addrOpnd->m_address, &cachedValue))
    {LOGMEIN("GlobOpt.cpp] 7066\n");
        // The cached value could be from a different block since this is a global (as opposed to a per-block) cache. Since
        // values are cloned for each block, we can't use the same value object. We also can't have two values with the same
        // number in one block, so we can't simply copy the cached value either. And finally, there is no deterministic and fast
        // way to determine if a value with the same value number exists for this block. So the best we can do with a global
        // cache is to check the sym-store's value in the current block to see if it has a value with the same number.
        // Otherwise, we have to create a new value with a new value number.
        Sym *symStore = cachedValue->GetValueInfo()->GetSymStore();
        if(symStore && IsLive(symStore, &blockData))
        {LOGMEIN("GlobOpt.cpp] 7075\n");
            Value *const symStoreValue = FindValue(symStore);
            if(symStoreValue && symStoreValue->GetValueNumber() == cachedValue->GetValueNumber())
            {LOGMEIN("GlobOpt.cpp] 7078\n");
                ValueInfo *const symStoreValueInfo = symStoreValue->GetValueInfo();
                if(symStoreValueInfo->IsVarConstant() && symStoreValueInfo->AsVarConstant()->VarValue() == addrOpnd->m_address)
                {LOGMEIN("GlobOpt.cpp] 7081\n");
                    val = symStoreValue;
                }
            }
        }
    }
    else if (isString)
    {LOGMEIN("GlobOpt.cpp] 7088\n");
        JITJavascriptString* jsString = JITJavascriptString::FromVar(addrOpnd->m_localAddress);
        Js::InternalString internalString(jsString->GetString(), jsString->GetLength());
        if (this->stringConstantToValueMap->TryGetValue(internalString, &cachedValue))
        {LOGMEIN("GlobOpt.cpp] 7092\n");
            Sym *symStore = cachedValue->GetValueInfo()->GetSymStore();
            if (symStore && IsLive(symStore, &blockData))
            {LOGMEIN("GlobOpt.cpp] 7095\n");
                Value *const symStoreValue = FindValue(symStore);
                if (symStoreValue && symStoreValue->GetValueNumber() == cachedValue->GetValueNumber())
                {LOGMEIN("GlobOpt.cpp] 7098\n");
                    ValueInfo *const symStoreValueInfo = symStoreValue->GetValueInfo();
                    if (symStoreValueInfo->IsVarConstant())
                    {LOGMEIN("GlobOpt.cpp] 7101\n");
                        JITJavascriptString * cachedString = JITJavascriptString::FromVar(symStoreValue->GetValueInfo()->AsVarConstant()->VarValue(true));
                        Js::InternalString cachedInternalString(cachedString->GetString(), cachedString->GetLength());
                        if (Js::InternalStringComparer::Equals(internalString, cachedInternalString))
                        {LOGMEIN("GlobOpt.cpp] 7105\n");
                            val = symStoreValue;
                        }
                    }
                }
            }
        }
    }

    if(!val)
    {LOGMEIN("GlobOpt.cpp] 7115\n");
        val = NewVarConstantValue(addrOpnd, isString);
    }

    addrOpnd->SetValueType(val->GetValueInfo()->Type());
    return val;
}

Value *
GlobOpt::NewVarConstantValue(IR::AddrOpnd *addrOpnd, bool isString)
{LOGMEIN("GlobOpt.cpp] 7125\n");
    VarConstantValueInfo *valueInfo = VarConstantValueInfo::New(this->alloc, addrOpnd->m_address, addrOpnd->GetValueType(), false, addrOpnd->m_localAddress);
    Value * value = NewValue(valueInfo);
    this->addrConstantToValueMap->Item(addrOpnd->m_address, value);
    if (isString)
    {LOGMEIN("GlobOpt.cpp] 7130\n");
        JITJavascriptString* jsString = JITJavascriptString::FromVar(addrOpnd->m_localAddress);
        Js::InternalString internalString(jsString->GetString(), jsString->GetLength());
        this->stringConstantToValueMap->Item(internalString, value);
    }
    return value;
}

Value *
GlobOpt::HoistConstantLoadAndPropagateValueBackward(Js::Var varConst, IR::Instr * origInstr, Value * value)
{LOGMEIN("GlobOpt.cpp] 7140\n");
    if (this->IsLoopPrePass() ||
        ((this->currentBlock == this->func->m_fg->blockList) &&
        TransferSrcValue(origInstr)))
    {LOGMEIN("GlobOpt.cpp] 7144\n");
        return value;
    }

    // Only hoisting taggable int const loads for now. Could be extended to other constants (floats, strings, addr opnds) if we see some benefit.
    Assert(Js::TaggedInt::Is(varConst));

    // Insert a load of the constant at the top of the function
    StackSym *    dstSym = StackSym::New(this->func);
    IR::RegOpnd * constRegOpnd = IR::RegOpnd::New(dstSym, TyVar, this->func);
    IR::Instr *   loadInstr = IR::Instr::NewConstantLoad(constRegOpnd, (intptr_t)varConst, ValueType::GetInt(true), this->func);
    this->func->m_fg->blockList->GetFirstInstr()->InsertAfter(loadInstr);

    // Type-spec the load (Support for floats needs to be added when we start hoisting float constants).
    bool typeSpecedToInt = false;
    if (Js::TaggedInt::Is(varConst) && !IsTypeSpecPhaseOff(this->func))
    {LOGMEIN("GlobOpt.cpp] 7160\n");
        typeSpecedToInt = true;
        loadInstr->m_opcode = Js::OpCode::Ld_I4;
        ToInt32Dst(loadInstr, loadInstr->GetDst()->AsRegOpnd(), this->currentBlock);
        loadInstr->GetDst()->GetStackSym()->SetIsConst();
    }
    else
    {
        this->currentBlock->globOptData.liveVarSyms->Set(dstSym->m_id);
    }

    // Add the value (object) to the current block's symToValueMap and propagate the value backward to all relevant blocks so it is available on merges.
    value = this->InsertNewValue(value, constRegOpnd);

    BVSparse<JitArenaAllocator>* GlobOptBlockData::*bv;
    bv = typeSpecedToInt ? &GlobOptBlockData::liveInt32Syms : &GlobOptBlockData::liveVarSyms; // Will need to be expanded when we start hoisting float constants.
    if (this->currentBlock != this->func->m_fg->blockList)
    {LOGMEIN("GlobOpt.cpp] 7177\n");
        for (InvariantBlockBackwardIterator it(this, this->currentBlock, this->func->m_fg->blockList, nullptr);
             it.IsValid();
             it.MoveNext())
        {LOGMEIN("GlobOpt.cpp] 7181\n");
            BasicBlock * block = it.Block();
            (block->globOptData.*bv)->Set(dstSym->m_id);
            Assert(!FindValue(block->globOptData.symToValueMap, dstSym));
            Value *const valueCopy = CopyValue(value, value->GetValueNumber());
            SetValue(&block->globOptData, valueCopy, dstSym);
        }
    }

    return value;
}

Value *
GlobOpt::NewFixedFunctionValue(Js::JavascriptFunction *function, IR::AddrOpnd *addrOpnd)
{LOGMEIN("GlobOpt.cpp] 7195\n");
    Assert(function != nullptr);

    Value *val = nullptr;
    Value *cachedValue;
    if(this->addrConstantToValueMap->TryGetValue(addrOpnd->m_address, &cachedValue))
    {LOGMEIN("GlobOpt.cpp] 7201\n");
        // The cached value could be from a different block since this is a global (as opposed to a per-block) cache. Since
        // values are cloned for each block, we can't use the same value object. We also can't have two values with the same
        // number in one block, so we can't simply copy the cached value either. And finally, there is no deterministic and fast
        // way to determine if a value with the same value number exists for this block. So the best we can do with a global
        // cache is to check the sym-store's value in the current block to see if it has a value with the same number.
        // Otherwise, we have to create a new value with a new value number.
        Sym *symStore = cachedValue->GetValueInfo()->GetSymStore();
        if(symStore && IsLive(symStore, &blockData))
        {LOGMEIN("GlobOpt.cpp] 7210\n");
            Value *const symStoreValue = FindValue(symStore);
            if(symStoreValue && symStoreValue->GetValueNumber() == cachedValue->GetValueNumber())
            {LOGMEIN("GlobOpt.cpp] 7213\n");
                ValueInfo *const symStoreValueInfo = symStoreValue->GetValueInfo();
                if(symStoreValueInfo->IsVarConstant())
                {LOGMEIN("GlobOpt.cpp] 7216\n");
                    VarConstantValueInfo *const symStoreVarConstantValueInfo = symStoreValueInfo->AsVarConstant();
                    if(symStoreVarConstantValueInfo->VarValue() == addrOpnd->m_address &&
                        symStoreVarConstantValueInfo->IsFunction())
                    {LOGMEIN("GlobOpt.cpp] 7220\n");
                        val = symStoreValue;
                    }
                }
            }
        }
    }

    if(!val)
    {LOGMEIN("GlobOpt.cpp] 7229\n");
        VarConstantValueInfo *valueInfo = VarConstantValueInfo::New(this->alloc, function, addrOpnd->GetValueType(), true, addrOpnd->m_localAddress);
        val = NewValue(valueInfo);
        this->addrConstantToValueMap->AddNew(addrOpnd->m_address, val);
    }

    this->InsertNewValue(val, addrOpnd);
    return val;
}

Value *
GlobOpt::InsertNewValue(Value *val, IR::Opnd *opnd)
{LOGMEIN("GlobOpt.cpp] 7241\n");
    return this->InsertNewValue(&this->blockData, val, opnd);
}

Value *
GlobOpt::InsertNewValue(GlobOptBlockData *blockData, Value *val, IR::Opnd *opnd)
{LOGMEIN("GlobOpt.cpp] 7247\n");
    return this->SetValue(blockData, val, opnd);
}

void
GlobOpt::SetValueToHashTable(GlobHashTable *valueNumberMap, Value *val, Sym *sym)
{LOGMEIN("GlobOpt.cpp] 7253\n");
    Value **pValue = valueNumberMap->FindOrInsertNew(sym);
    *pValue = val;
}

StackSym *GlobOpt::GetTaggedIntConstantStackSym(const int32 intConstantValue) const
{LOGMEIN("GlobOpt.cpp] 7259\n");
    Assert(!Js::TaggedInt::IsOverflow(intConstantValue));

    return intConstantToStackSymMap->Lookup(intConstantValue, nullptr);
}

StackSym *GlobOpt::GetOrCreateTaggedIntConstantStackSym(const int32 intConstantValue) const
{LOGMEIN("GlobOpt.cpp] 7266\n");
    StackSym *stackSym = GetTaggedIntConstantStackSym(intConstantValue);
    if(stackSym)
    {LOGMEIN("GlobOpt.cpp] 7269\n");
        return stackSym;
    }

    stackSym = StackSym::New(TyVar,func);
    intConstantToStackSymMap->Add(intConstantValue, stackSym);
    return stackSym;
}

Sym *
GlobOpt::SetSymStore(ValueInfo *valueInfo, Sym *sym)
{LOGMEIN("GlobOpt.cpp] 7280\n");
    if (sym->IsStackSym())
    {LOGMEIN("GlobOpt.cpp] 7282\n");
        StackSym *stackSym = sym->AsStackSym();

        if (stackSym->IsTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 7286\n");
            stackSym = stackSym->GetVarEquivSym(this->func);
            sym = stackSym;
        }
    }
    if (valueInfo->GetSymStore() == nullptr || valueInfo->GetSymStore()->IsPropertySym())
    {
        SetSymStoreDirect(valueInfo, sym);
    }

    return sym;
}

void
GlobOpt::SetSymStoreDirect(ValueInfo * valueInfo, Sym * sym)
{LOGMEIN("GlobOpt.cpp] 7301\n");
    Sym * prevSymStore = valueInfo->GetSymStore();
    if (prevSymStore && prevSymStore->IsStackSym() &&
        prevSymStore->AsStackSym()->HasByteCodeRegSlot())
    {LOGMEIN("GlobOpt.cpp] 7305\n");
        this->SetChangedSym(prevSymStore->m_id);
    }
    valueInfo->SetSymStore(sym);
}

void
GlobOpt::SetChangedSym(SymID symId)
{LOGMEIN("GlobOpt.cpp] 7313\n");
    // this->currentBlock might not be the one which contain the changing symId,
    // like hoisting invariant, but more changed symId is overly conservative and safe.
    // symId in the hoisted to block is marked as JITOptimizedReg so it does't affect bailout.
    GlobOptBlockData * globOptData = &this->currentBlock->globOptData;
    if (globOptData->changedSyms)
    {LOGMEIN("GlobOpt.cpp] 7319\n");
        globOptData = &this->currentBlock->globOptData;

        globOptData->changedSyms->Set(symId);
        if (globOptData->capturedValuesCandidate != nullptr)
        {LOGMEIN("GlobOpt.cpp] 7324\n");
            this->changedSymsAfterIncBailoutCandidate->Set(symId);
        }
    }
    // else could be hit only in MergeValues and it is handled by MergeCapturedValues
}

void
GlobOpt::SetValue(GlobOptBlockData *blockData, Value *val, Sym * sym)
{LOGMEIN("GlobOpt.cpp] 7333\n");
    ValueInfo *valueInfo = val->GetValueInfo();

    sym = this->SetSymStore(valueInfo, sym);
    bool isStackSym = sym->IsStackSym();

    if (isStackSym && sym->AsStackSym()->IsFromByteCodeConstantTable())
    {LOGMEIN("GlobOpt.cpp] 7340\n");
        // Put the constants in a global array. This will minimize the per-block info.
        this->byteCodeConstantValueArray->Set(sym->m_id, val);
        this->byteCodeConstantValueNumbersBv->Set(val->GetValueNumber());
    }
    else
    {
        SetValueToHashTable(blockData->symToValueMap, val, sym);
        if (isStackSym && sym->AsStackSym()->HasByteCodeRegSlot())
        {LOGMEIN("GlobOpt.cpp] 7349\n");
            this->SetChangedSym(sym->m_id);
        }
    }
}

Value *
GlobOpt::SetValue(GlobOptBlockData *blockData, Value *val, IR::Opnd *opnd)
{LOGMEIN("GlobOpt.cpp] 7357\n");
    if (opnd)
    {LOGMEIN("GlobOpt.cpp] 7359\n");
        Sym *sym;
        switch (opnd->GetKind())
        {LOGMEIN("GlobOpt.cpp] 7362\n");
        case IR::OpndKindSym:
            sym = opnd->AsSymOpnd()->m_sym;
            break;

        case IR::OpndKindReg:
            sym = opnd->AsRegOpnd()->m_sym;
            break;

        default:
            sym = nullptr;
        }
        if (sym)
        {
            SetValue(blockData, val, sym);
        }
    }

    return val;
}

// Figure out the Value of this dst.
Value *
GlobOpt::ValueNumberDst(IR::Instr **pInstr, Value *src1Val, Value *src2Val)
{LOGMEIN("GlobOpt.cpp] 7386\n");
    IR::Instr *&instr = *pInstr;
    IR::Opnd *dst = instr->GetDst();
    Value *dstVal = nullptr;
    Sym *sym;

    if (instr->CallsSetter())
    {LOGMEIN("GlobOpt.cpp] 7393\n");
        return nullptr;
    }
    if (dst == nullptr)
    {LOGMEIN("GlobOpt.cpp] 7397\n");
        return nullptr;
    }

    switch (dst->GetKind())
    {LOGMEIN("GlobOpt.cpp] 7402\n");
    case IR::OpndKindSym:
        sym = dst->AsSymOpnd()->m_sym;
        break;

    case IR::OpndKindReg:
        sym = dst->AsRegOpnd()->m_sym;

        if (OpCodeAttr::TempNumberProducing(instr->m_opcode))
        {LOGMEIN("GlobOpt.cpp] 7411\n");
            this->blockData.isTempSrc->Set(sym->m_id);
        }
        else if (OpCodeAttr::TempNumberTransfer(instr->m_opcode))
        {LOGMEIN("GlobOpt.cpp] 7415\n");
            IR::Opnd *src1 = instr->GetSrc1();

            if (src1->IsRegOpnd() && this->blockData.isTempSrc->Test(src1->AsRegOpnd()->m_sym->m_id))
            {LOGMEIN("GlobOpt.cpp] 7419\n");
                StackSym *src1Sym = src1->AsRegOpnd()->m_sym;
                // isTempSrc is used for marking isTempLastUse, which is used to generate AddLeftDead()
                // calls instead of the normal Add helpers. It tells the runtime that concats can use string
                // builders.
                // We need to be careful in the case where src1 points to a string builder and is getting aliased.
                // Clear the bit on src and dst of the transfer instr in this case, unless we can prove src1
                // isn't pointing at a string builder, like if it is single def and the def instr is not an Add,
                // but TempProducing.
                if (src1Sym->IsSingleDef() && src1Sym->m_instrDef->m_opcode != Js::OpCode::Add_A
                    && OpCodeAttr::TempNumberProducing(src1Sym->m_instrDef->m_opcode))
                {LOGMEIN("GlobOpt.cpp] 7430\n");
                    this->blockData.isTempSrc->Set(sym->m_id);
                }
                else
                {
                    this->blockData.isTempSrc->Clear(src1->AsRegOpnd()->m_sym->m_id);
                    this->blockData.isTempSrc->Clear(sym->m_id);
                }
            }
            else
            {
                this->blockData.isTempSrc->Clear(sym->m_id);
            }
        }
        else
        {
            this->blockData.isTempSrc->Clear(sym->m_id);
        }
        break;

    case IR::OpndKindIndir:
        return nullptr;

    default:
        return nullptr;
    }

    int32 min1, max1, min2, max2, newMin, newMax;
    ValueInfo *src1ValueInfo = (src1Val ? src1Val->GetValueInfo() : nullptr);
    ValueInfo *src2ValueInfo = (src2Val ? src2Val->GetValueInfo() : nullptr);

    switch (instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 7462\n");
    case Js::OpCode::Conv_PrimStr:
        AssertMsg(instr->GetDst()->GetValueType().IsString(),
            "Creator of this instruction should have set the type");
        if (this->IsLoopPrePass() || src1ValueInfo == nullptr || !src1ValueInfo->IsPrimitive())
        {LOGMEIN("GlobOpt.cpp] 7467\n");
            break;
        }
        instr->m_opcode = Js::OpCode::Conv_Str;
        // fall-through
    case Js::OpCode::Conv_Str:
    // This opcode is commented out since we don't track regex information in GlobOpt now.
    //case Js::OpCode::Coerce_Regex:
    case Js::OpCode::Coerce_Str:
        AssertMsg(instr->GetDst()->GetValueType().IsString(),
            "Creator of this instruction should have set the type");
        // fall-through
    case Js::OpCode::Coerce_StrOrRegex:
        // We don't set the ValueType of src1 for Coerce_StrOrRegex, hence skip the ASSERT
        if (this->IsLoopPrePass() || src1ValueInfo == nullptr || !src1ValueInfo->IsString())
        {LOGMEIN("GlobOpt.cpp] 7482\n");
            break;
        }
        instr->m_opcode = Js::OpCode::Ld_A;
        // fall-through

    case Js::OpCode::BytecodeArgOutCapture:
    case Js::OpCode::InitConst:
    case Js::OpCode::LdAsmJsFunc:
    case Js::OpCode::Ld_A:
    case Js::OpCode::Ld_I4:

        // Propagate sym attributes across the reg copy.
        if (!this->IsLoopPrePass() && instr->GetSrc1()->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 7496\n");
            if (dst->AsRegOpnd()->m_sym->IsSingleDef())
            {LOGMEIN("GlobOpt.cpp] 7498\n");
                dst->AsRegOpnd()->m_sym->CopySymAttrs(instr->GetSrc1()->AsRegOpnd()->m_sym);
            }
        }

        if (instr->IsProfiledInstr())
        {LOGMEIN("GlobOpt.cpp] 7504\n");
            const ValueType profiledValueType(instr->AsProfiledInstr()->u.FldInfo().valueType);
            if(!(
                    profiledValueType.IsLikelyInt() &&
                    (
                        (dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym->m_isNotInt) ||
                        (instr->GetSrc1()->IsRegOpnd() && instr->GetSrc1()->AsRegOpnd()->m_sym->m_isNotInt)
                    )
                ))
            {LOGMEIN("GlobOpt.cpp] 7513\n");
                if(!src1ValueInfo)
                {LOGMEIN("GlobOpt.cpp] 7515\n");
                    dstVal = this->NewGenericValue(profiledValueType, dst);
                }
                else if(src1ValueInfo->IsUninitialized())
                {LOGMEIN("GlobOpt.cpp] 7519\n");
                    if(IsLoopPrePass())
                    {LOGMEIN("GlobOpt.cpp] 7521\n");
                        dstVal = this->NewGenericValue(profiledValueType, dst);
                    }
                    else
                    {
                        // Assuming the profile data gives more precise value types based on the path it took at runtime, we
                        // can improve the original value type.
                        src1ValueInfo->Type() = profiledValueType;
                        instr->GetSrc1()->SetValueType(profiledValueType);
                    }
                }
            }
        }

        if (dstVal == nullptr)
        {LOGMEIN("GlobOpt.cpp] 7536\n");
            // Ld_A is just transferring the value
            dstVal = this->ValueNumberTransferDst(instr, src1Val);
        }

        break;

    case Js::OpCode::ExtendArg_A:
    {LOGMEIN("GlobOpt.cpp] 7544\n");
        // SIMD_JS
        // We avoid transforming EAs to Lds to keep the IR shape consistent and avoid CSEing of EAs.
        // CSEOptimize only assigns a Value to the EA dst, and doesn't turn it to a Ld. If this happened, we shouldn't assign a new Value here.
        if (DoCSE())
        {LOGMEIN("GlobOpt.cpp] 7549\n");
            IR::Opnd * currDst = instr->GetDst();
            Value * currDstVal = this->FindValue(currDst->GetStackSym());
            if (currDstVal != nullptr)
            {LOGMEIN("GlobOpt.cpp] 7553\n");
                return currDstVal;
            }
        }
        break;
    }

    case Js::OpCode::CheckFixedFld:
        AssertMsg(false, "CheckFixedFld doesn't have a dst, so we should never get here");
        break;

    case Js::OpCode::LdSlot:
    case Js::OpCode::LdSlotArr:
    case Js::OpCode::LdFld:
    case Js::OpCode::LdFldForTypeOf:
    case Js::OpCode::LdFldForCallApplyTarget:
    // Do not transfer value type on ldFldForTypeOf to prevent copy-prop to LdRootFld in case the field doesn't exist since LdRootFldForTypeOf does not throw
    //case Js::OpCode::LdRootFldForTypeOf:
    case Js::OpCode::LdRootFld:
    case Js::OpCode::LdMethodFld:
    case Js::OpCode::LdRootMethodFld:
    case Js::OpCode::ScopedLdMethodFld:
    case Js::OpCode::LdMethodFromFlags:
        if (instr->IsProfiledInstr())
        {LOGMEIN("GlobOpt.cpp] 7577\n");
            ValueType profiledValueType(instr->AsProfiledInstr()->u.FldInfo().valueType);
            if(!(profiledValueType.IsLikelyInt() && dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym->m_isNotInt))
            {LOGMEIN("GlobOpt.cpp] 7580\n");
                if(!src1ValueInfo)
                {LOGMEIN("GlobOpt.cpp] 7582\n");
                    dstVal = this->NewGenericValue(profiledValueType, dst);
                }
                else if(src1ValueInfo->IsUninitialized())
                {LOGMEIN("GlobOpt.cpp] 7586\n");
                    if(IsLoopPrePass() && (!dst->IsRegOpnd() || !dst->AsRegOpnd()->m_sym->IsSingleDef() || DoFieldHoisting()))
                    {LOGMEIN("GlobOpt.cpp] 7588\n");
                        dstVal = this->NewGenericValue(profiledValueType, dst);
                    }
                    else
                    {
                        // Assuming the profile data gives more precise value types based on the path it took at runtime, we
                        // can improve the original value type.
                        src1ValueInfo->Type() = profiledValueType;
                        instr->GetSrc1()->SetValueType(profiledValueType);
                    }
                }
            }
        }
        if (dstVal == nullptr)
        {LOGMEIN("GlobOpt.cpp] 7602\n");
            dstVal = this->ValueNumberTransferDst(instr, src1Val);
        }

        if(!this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 7607\n");
            // We cannot transfer value if the field hasn't been copy prop'd because we don't generate
            // an implicit call bailout between those values if we don't have "live fields" unless, we are hoisting the field.
            PropertySym *propertySym = instr->GetSrc1()->AsSymOpnd()->m_sym->AsPropertySym();
            StackSym * fieldHoistSym;
            Loop * loop = this->FindFieldHoistStackSym(this->currentBlock->loop, propertySym->m_id, &fieldHoistSym, instr);
            ValueInfo *dstValueInfo = (dstVal ? dstVal->GetValueInfo() : nullptr);

            // Update symStore for field hoisting
            if (loop != nullptr && (dstValueInfo != nullptr))
            {LOGMEIN("GlobOpt.cpp] 7617\n");
                this->SetSymStoreDirect(dstValueInfo, fieldHoistSym);
            }
            // Update symStore if it isn't a stackSym
            if (dstVal && (!dstValueInfo->GetSymStore() || !dstValueInfo->GetSymStore()->IsStackSym()))
            {LOGMEIN("GlobOpt.cpp] 7622\n");
                Assert(dst->IsRegOpnd());
                this->SetSymStoreDirect(dstValueInfo, dst->AsRegOpnd()->m_sym);
            }
            if (src1Val != dstVal)
            {LOGMEIN("GlobOpt.cpp] 7627\n");
                this->SetValue(&this->blockData, dstVal, instr->GetSrc1());
            }
        }
        break;

    case Js::OpCode::LdC_A_R8:
    case Js::OpCode::LdC_A_I4:
    case Js::OpCode::ArgIn_A:
        dstVal = src1Val;
        break;

    case Js::OpCode::LdStr:
        if (src1Val == nullptr)
        {LOGMEIN("GlobOpt.cpp] 7641\n");
            src1Val = NewGenericValue(ValueType::String, dst);
        }
        dstVal = src1Val;
        break;

    // LdElemUndef only assign undef if the field doesn't exist.
    // So we don't actually know what the value is, so we can't really copy prop it.
    //case Js::OpCode::LdElemUndef:
    case Js::OpCode::StSlot:
    case Js::OpCode::StSlotChkUndecl:
    case Js::OpCode::StFld:
    case Js::OpCode::StRootFld:
    case Js::OpCode::StFldStrict:
    case Js::OpCode::StRootFldStrict:
        if (DoFieldCopyProp())
        {LOGMEIN("GlobOpt.cpp] 7657\n");
            if (src1Val == nullptr)
            {LOGMEIN("GlobOpt.cpp] 7659\n");
                // src1 may have no value if it's not a valid var, e.g., NULL for let/const initialization.
                // Consider creating generic values for such things.
                return nullptr;
            }
            AssertMsg(!src2Val, "Bad src Values...");

            Assert(sym->IsPropertySym());
            SymID symId = sym->m_id;
            Assert(instr->m_opcode == Js::OpCode::StSlot || instr->m_opcode == Js::OpCode::StSlotChkUndecl || !this->blockData.liveFields->Test(symId));
            if (IsHoistablePropertySym(symId))
            {LOGMEIN("GlobOpt.cpp] 7670\n");
                // We have changed the value of a hoistable field, load afterwards shouldn't get hoisted,
                // but we will still copy prop the pre-assign sym to it if we have a live value.
                Assert((instr->m_opcode == Js::OpCode::StSlot || instr->m_opcode == Js::OpCode::StSlotChkUndecl) && this->blockData.liveFields->Test(symId));
                this->blockData.hoistableFields->Clear(symId);
            }
            this->blockData.liveFields->Set(symId);
            if (!this->IsLoopPrePass() && dst->GetIsDead())
            {LOGMEIN("GlobOpt.cpp] 7678\n");
                // Take the property sym out of the live fields set (with special handling for loops).
                this->EndFieldLifetime(dst->AsSymOpnd());
            }

            dstVal = this->ValueNumberTransferDst(instr, src1Val);
        }
        else
        {
            return nullptr;
        }
        break;

    case Js::OpCode::Conv_Num:
        if(src1ValueInfo->IsNumber())
        {LOGMEIN("GlobOpt.cpp] 7693\n");
            dstVal = ValueNumberTransferDst(instr, src1Val);
        }
        else
        {
            return NewGenericValue(src1ValueInfo->Type().ToDefiniteAnyNumber(), dst);
        }
        break;

    case Js::OpCode::Not_A:
    {LOGMEIN("GlobOpt.cpp] 7703\n");
        if (!src1Val || !src1ValueInfo->GetIntValMinMax(&min1, &max1, this->DoAggressiveIntTypeSpec()))
        {LOGMEIN("GlobOpt.cpp] 7705\n");
            min1 = INT32_MIN;
            max1 = INT32_MAX;
        }

        this->PropagateIntRangeForNot(min1, max1, &newMin, &newMax);
        return CreateDstUntransferredIntValue(newMin, newMax, instr, src1Val, src2Val);
    }

    case Js::OpCode::Xor_A:
    case Js::OpCode::Or_A:
    case Js::OpCode::And_A:
    case Js::OpCode::Shl_A:
    case Js::OpCode::Shr_A:
    case Js::OpCode::ShrU_A:
    {LOGMEIN("GlobOpt.cpp] 7720\n");
        if (!src1Val || !src1ValueInfo->GetIntValMinMax(&min1, &max1, this->DoAggressiveIntTypeSpec()))
        {LOGMEIN("GlobOpt.cpp] 7722\n");
            min1 = INT32_MIN;
            max1 = INT32_MAX;
        }
        if (!src2Val || !src2ValueInfo->GetIntValMinMax(&min2, &max2, this->DoAggressiveIntTypeSpec()))
        {LOGMEIN("GlobOpt.cpp] 7727\n");
            min2 = INT32_MIN;
            max2 = INT32_MAX;
        }

        if (instr->m_opcode == Js::OpCode::ShrU_A &&
            min1 < 0 &&
            IntConstantBounds(min2, max2).And_0x1f().Contains(0))
        {LOGMEIN("GlobOpt.cpp] 7735\n");
            // Src1 may be too large to represent as a signed int32, and src2 may be zero.
            // Since the result can therefore be too large to represent as a signed int32,
            // include Number in the value type.
            return CreateDstUntransferredValue(
                ValueType::AnyNumber.SetCanBeTaggedValue(true), instr, src1Val, src2Val);
        }

        this->PropagateIntRangeBinary(instr, min1, max1, min2, max2, &newMin, &newMax);
        return CreateDstUntransferredIntValue(newMin, newMax, instr, src1Val, src2Val);
    }

    case Js::OpCode::Incr_A:
    case Js::OpCode::Decr_A:
    {LOGMEIN("GlobOpt.cpp] 7749\n");
        ValueType valueType;
        if(src1Val)
        {LOGMEIN("GlobOpt.cpp] 7752\n");
            valueType = src1Val->GetValueInfo()->Type().ToDefiniteAnyNumber();
        }
        else
        {
            valueType = ValueType::Number;
        }
        return CreateDstUntransferredValue(valueType, instr, src1Val, src2Val);
    }

    case Js::OpCode::Add_A:
    {LOGMEIN("GlobOpt.cpp] 7763\n");
        ValueType valueType;
        if (src1Val && src1ValueInfo->IsLikelyNumber() && src2Val && src2ValueInfo->IsLikelyNumber())
        {LOGMEIN("GlobOpt.cpp] 7766\n");
            if(src1ValueInfo->IsLikelyInt() && src2ValueInfo->IsLikelyInt())
            {LOGMEIN("GlobOpt.cpp] 7768\n");
                // When doing aggressiveIntType, just assume the result is likely going to be int
                // if both input is int.
                const bool isLikelyTagged = src1ValueInfo->IsLikelyTaggedInt() && src2ValueInfo->IsLikelyTaggedInt();
                if(src1ValueInfo->IsNumber() && src2ValueInfo->IsNumber())
                {LOGMEIN("GlobOpt.cpp] 7773\n");
                    // If both of them are numbers then we can definitely say that the result is a number.
                    valueType = ValueType::GetNumberAndLikelyInt(isLikelyTagged);
                }
                else
                {
                    // This is only likely going to be int but can be a string as well.
                    valueType = ValueType::GetInt(isLikelyTagged).ToLikely();
                }
            }
            else
            {
                // We can only be certain of any thing if both of them are numbers.
                // Otherwise, the result could be string.
                if (src1ValueInfo->IsNumber() && src2ValueInfo->IsNumber())
                {LOGMEIN("GlobOpt.cpp] 7788\n");
                    if (src1ValueInfo->IsFloat() || src2ValueInfo->IsFloat())
                    {LOGMEIN("GlobOpt.cpp] 7790\n");
                        // If one of them is a float, the result probably is a float instead of just int
                        // but should always be a number.
                        valueType = ValueType::Float;
                    }
                    else
                    {
                        // Could be int, could be number
                        valueType = ValueType::Number;
                    }
                }
                else if (src1ValueInfo->IsLikelyFloat() || src2ValueInfo->IsLikelyFloat())
                {LOGMEIN("GlobOpt.cpp] 7802\n");
                    // Result is likely a float (but can be anything)
                    valueType = ValueType::Float.ToLikely();
                }
                else
                {
                    // Otherwise it is a likely int or float (but can be anything)
                    valueType = ValueType::Number.ToLikely();
                }
            }
        }
        else if((src1Val && src1ValueInfo->IsString()) || (src2Val && src2ValueInfo->IsString()))
        {LOGMEIN("GlobOpt.cpp] 7814\n");
            // String + anything should always result in a string
            valueType = ValueType::String;
        }
        else if((src1Val && src1ValueInfo->IsNotString() && src1ValueInfo->IsPrimitive())
            && (src2Val && src2ValueInfo->IsNotString() && src2ValueInfo->IsPrimitive()))
        {LOGMEIN("GlobOpt.cpp] 7820\n");
            // If src1 and src2 are not strings and primitive, add should yield a number.
            valueType = ValueType::Number;
        }
        else if((src1Val && src1ValueInfo->IsLikelyString()) || (src2Val && src2ValueInfo->IsLikelyString()))
        {LOGMEIN("GlobOpt.cpp] 7825\n");
            // likelystring + anything should always result in a likelystring
            valueType = ValueType::String.ToLikely();
        }
        else
        {
            // Number or string. Could make the value a merge of Number and String, but Uninitialized is more useful at the moment.
            Assert(valueType.IsUninitialized());
        }

        return CreateDstUntransferredValue(valueType, instr, src1Val, src2Val);
    }

    case Js::OpCode::Div_A:
        {LOGMEIN("GlobOpt.cpp] 7839\n");
            ValueType divValueType = GetDivValueType(instr, src1Val, src2Val, false);
            if (divValueType.IsLikelyInt() || divValueType.IsFloat())
            {LOGMEIN("GlobOpt.cpp] 7842\n");
                return CreateDstUntransferredValue(divValueType, instr, src1Val, src2Val);
            }
        }
        // fall-through

    case Js::OpCode::Sub_A:
    case Js::OpCode::Mul_A:
    case Js::OpCode::Rem_A:
    {LOGMEIN("GlobOpt.cpp] 7851\n");
        ValueType valueType;
        if( src1Val &&
            src1ValueInfo->IsLikelyInt() &&
            src2Val &&
            src2ValueInfo->IsLikelyInt() &&
            instr->m_opcode != Js::OpCode::Div_A)
        {LOGMEIN("GlobOpt.cpp] 7858\n");
            const bool isLikelyTagged =
                src1ValueInfo->IsLikelyTaggedInt() && (src2ValueInfo->IsLikelyTaggedInt() || instr->m_opcode == Js::OpCode::Rem_A);
            if(src1ValueInfo->IsNumber() && src2ValueInfo->IsNumber())
            {LOGMEIN("GlobOpt.cpp] 7862\n");
                valueType = ValueType::GetNumberAndLikelyInt(isLikelyTagged);
            }
            else
            {
                valueType = ValueType::GetInt(isLikelyTagged).ToLikely();
            }
        }
        else if ((src1Val && src1ValueInfo->IsLikelyFloat()) || (src2Val && src2ValueInfo->IsLikelyFloat()))
        {LOGMEIN("GlobOpt.cpp] 7871\n");
            // This should ideally be NewNumberAndLikelyFloatValue since we know the result is a number but not sure if it will
            // be a float value. However, that Number/LikelyFloat value type doesn't exist currently and all the necessary
            // checks are done for float values (tagged int checks, etc.) so it's sufficient to just create a float value here.
            valueType = ValueType::Float;
        }
        else
        {
            valueType = ValueType::Number;
        }

        return CreateDstUntransferredValue(valueType, instr, src1Val, src2Val);
    }

    case Js::OpCode::CallI:
        Assert(dst->IsRegOpnd());
        return NewGenericValue(dst->AsRegOpnd()->GetValueType(), dst);

    case Js::OpCode::LdElemI_A:
    {LOGMEIN("GlobOpt.cpp] 7890\n");
        dstVal = ValueNumberLdElemDst(pInstr, src1Val);
        const ValueType baseValueType(instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetValueType());
        if( (
                baseValueType.IsLikelyNativeArray() ||
            #ifdef _M_IX86
                (
                    !AutoSystemInfo::Data.SSE2Available() &&
                    baseValueType.IsLikelyObject() &&
                    (
                        baseValueType.GetObjectType() == ObjectType::Float32Array ||
                        baseValueType.GetObjectType() == ObjectType::Float64Array
                    )
                )
            #else
                false
            #endif
            ) &&
            instr->GetDst()->IsVar() &&
            instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 7910\n");
            // The lowerer is not going to generate a fast path for this case. Remove any bailouts that require the fast
            // path. Note that the removed bailouts should not be necessary for correctness.
            IR::BailOutKind bailOutKind = instr->GetBailOutKind();
            if(bailOutKind & IR::BailOutOnArrayAccessHelperCall)
            {LOGMEIN("GlobOpt.cpp] 7915\n");
                bailOutKind -= IR::BailOutOnArrayAccessHelperCall;
            }
            if(bailOutKind == IR::BailOutOnImplicitCallsPreOp)
            {LOGMEIN("GlobOpt.cpp] 7919\n");
                bailOutKind -= IR::BailOutOnImplicitCallsPreOp;
            }
            if(bailOutKind)
            {LOGMEIN("GlobOpt.cpp] 7923\n");
                instr->SetBailOutKind(bailOutKind);
            }
            else
            {
                instr->ClearBailOutInfo();
            }
        }
        return dstVal;
    }

    case Js::OpCode::LdMethodElem:
        // Not worth profiling this, just assume it's likely object (should be likely function but ValueType does not track
        // functions currently, so using ObjectType::Object instead)
        dstVal = NewGenericValue(ValueType::GetObject(ObjectType::Object).ToLikely(), dst);
        if(instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetValueType().IsLikelyNativeArray() && instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 7939\n");
            // The lowerer is not going to generate a fast path for this case. Remove any bailouts that require the fast
            // path. Note that the removed bailouts should not be necessary for correctness.
            IR::BailOutKind bailOutKind = instr->GetBailOutKind();
            if(bailOutKind & IR::BailOutOnArrayAccessHelperCall)
            {LOGMEIN("GlobOpt.cpp] 7944\n");
                bailOutKind -= IR::BailOutOnArrayAccessHelperCall;
            }
            if(bailOutKind == IR::BailOutOnImplicitCallsPreOp)
            {LOGMEIN("GlobOpt.cpp] 7948\n");
                bailOutKind -= IR::BailOutOnImplicitCallsPreOp;
            }
            if(bailOutKind)
            {LOGMEIN("GlobOpt.cpp] 7952\n");
                instr->SetBailOutKind(bailOutKind);
            }
            else
            {
                instr->ClearBailOutInfo();
            }
        }
        return dstVal;

    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
        dstVal = this->ValueNumberTransferDst(instr, src1Val);
        break;

    case Js::OpCode::LdLen_A:
        if (instr->IsProfiledInstr())
        {LOGMEIN("GlobOpt.cpp] 7969\n");
            const ValueType profiledValueType(instr->AsProfiledInstr()->u.ldElemInfo->GetElementType());
            if(!(profiledValueType.IsLikelyInt() && dst->AsRegOpnd()->m_sym->m_isNotInt))
            {LOGMEIN("GlobOpt.cpp] 7972\n");
                return this->NewGenericValue(profiledValueType, dst);
            }
        }
        break;

    case Js::OpCode::BrOnEmpty:
    case Js::OpCode::BrOnNotEmpty:
        Assert(dst->IsRegOpnd());
        Assert(dst->GetValueType().IsString());
        return this->NewGenericValue(ValueType::String, dst);

    case Js::OpCode::IsInst:
    case Js::OpCode::LdTrue:
    case Js::OpCode::LdFalse:
        return this->NewGenericValue(ValueType::Boolean, dst);

    case Js::OpCode::LdUndef:
        return this->NewGenericValue(ValueType::Undefined, dst);

    case Js::OpCode::LdC_A_Null:
        return this->NewGenericValue(ValueType::Null, dst);

    case Js::OpCode::LdThis:
        if (!PHASE_OFF(Js::OptTagChecksPhase, this->func) &&
            (src1ValueInfo == nullptr || src1ValueInfo->IsUninitialized()))
        {LOGMEIN("GlobOpt.cpp] 7998\n");
            return this->NewGenericValue(ValueType::GetObject(ObjectType::Object), dst);
        }
        break;

    case Js::OpCode::Typeof:
        return this->NewGenericValue(ValueType::String, dst);
        break;
    }

#ifdef ENABLE_SIMDJS
    // SIMD_JS
    if (Js::IsSimd128Opcode(instr->m_opcode) && !func->GetJITFunctionBody()->IsAsmJsMode())
    {LOGMEIN("GlobOpt.cpp] 8011\n");
        ThreadContext::SimdFuncSignature simdFuncSignature;
        instr->m_func->GetScriptContext()->GetThreadContext()->GetSimdFuncSignatureFromOpcode(instr->m_opcode, simdFuncSignature);
        return this->NewGenericValue(simdFuncSignature.returnType, dst);
    }
#endif

    if (dstVal == nullptr)
    {LOGMEIN("GlobOpt.cpp] 8019\n");
        return this->NewGenericValue(dst->GetValueType(), dst);
    }

    return this->SetValue(&this->blockData, dstVal, dst);
}

Value *
GlobOpt::ValueNumberLdElemDst(IR::Instr **pInstr, Value *srcVal)
{LOGMEIN("GlobOpt.cpp] 8028\n");
    IR::Instr *&instr = *pInstr;
    IR::Opnd *dst = instr->GetDst();
    Value *dstVal = nullptr;
    int32 newMin, newMax;
    ValueInfo *srcValueInfo = (srcVal ? srcVal->GetValueInfo() : nullptr);

    ValueType profiledElementType;
    if (instr->IsProfiledInstr())
    {LOGMEIN("GlobOpt.cpp] 8037\n");
        profiledElementType = instr->AsProfiledInstr()->u.ldElemInfo->GetElementType();
        if(!(profiledElementType.IsLikelyInt() && dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym->m_isNotInt) &&
            srcVal &&
            srcValueInfo->IsUninitialized())
        {LOGMEIN("GlobOpt.cpp] 8042\n");
            if(IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 8044\n");
                dstVal = NewGenericValue(profiledElementType, dst);
            }
            else
            {
                // Assuming the profile data gives more precise value types based on the path it took at runtime, we
                // can improve the original value type.
                srcValueInfo->Type() = profiledElementType;
                instr->GetSrc1()->SetValueType(profiledElementType);
            }
        }
    }

    IR::IndirOpnd *src = instr->GetSrc1()->AsIndirOpnd();
    const ValueType baseValueType(src->GetBaseOpnd()->GetValueType());
    if (instr->DoStackArgsOpt(this->func) ||
        !(
            baseValueType.IsLikelyOptimizedTypedArray() ||
            (baseValueType.IsLikelyNativeArray() && instr->IsProfiledInstr()) // Specialized native array lowering for LdElem requires that it is profiled.
        ) ||
        (!this->DoTypedArrayTypeSpec() && baseValueType.IsLikelyOptimizedTypedArray()) ||

        // Don't do type spec on native array with a history of accessing gaps, as this is a bailout
        (!this->DoNativeArrayTypeSpec() && baseValueType.IsLikelyNativeArray()) ||
        !ShouldExpectConventionalArrayIndexValue(src))
    {LOGMEIN("GlobOpt.cpp] 8069\n");
        if(DoTypedArrayTypeSpec() && !IsLoopPrePass())
        {
            GOPT_TRACE_INSTR(instr, _u("Didn't specialize array access.\n"));
            if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->func))
            {LOGMEIN("GlobOpt.cpp] 8074\n");
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
                baseValueType.ToString(baseValueTypeStr);
                Output::Print(_u("Typed Array Optimization:  function: %s (%s): instr: %s, base value type: %S, did not type specialize, because %s.\n"),
                    this->func->GetJITFunctionBody()->GetDisplayName(),
                    this->func->GetDebugNumberSet(debugStringBuffer),
                    Js::OpCodeUtil::GetOpCodeName(instr->m_opcode),
                    baseValueTypeStr,
                    instr->DoStackArgsOpt(this->func) ? _u("instruction uses the arguments object") :
                    baseValueType.IsLikelyOptimizedTypedArray() ? _u("index is negative or likely not int") : _u("of array type"));
                Output::Flush();
            }
        }

        if(!dstVal)
        {LOGMEIN("GlobOpt.cpp] 8090\n");
            if(srcVal)
            {LOGMEIN("GlobOpt.cpp] 8092\n");
                dstVal = this->ValueNumberTransferDst(instr, srcVal);
            }
            else
            {
                dstVal = NewGenericValue(profiledElementType, dst);
            }
        }
        return dstVal;
    }

    Assert(instr->GetSrc1()->IsIndirOpnd());

    IRType toType = TyVar;
    IR::BailOutKind bailOutKind = IR::BailOutConventionalTypedArrayAccessOnly;

    switch(baseValueType.GetObjectType())
    {LOGMEIN("GlobOpt.cpp] 8109\n");
    case ObjectType::Int8Array:
    case ObjectType::Int8VirtualArray:
    case ObjectType::Int8MixedArray:
        newMin = Int8ConstMin;
        newMax = Int8ConstMax;
        goto IntArrayCommon;

    case ObjectType::Uint8Array:
    case ObjectType::Uint8VirtualArray:
    case ObjectType::Uint8MixedArray:
    case ObjectType::Uint8ClampedArray:
    case ObjectType::Uint8ClampedVirtualArray:
    case ObjectType::Uint8ClampedMixedArray:
        newMin = Uint8ConstMin;
        newMax = Uint8ConstMax;
        goto IntArrayCommon;

    case ObjectType::Int16Array:
    case ObjectType::Int16VirtualArray:
    case ObjectType::Int16MixedArray:
        newMin = Int16ConstMin;
        newMax = Int16ConstMax;
        goto IntArrayCommon;

    case ObjectType::Uint16Array:
    case ObjectType::Uint16VirtualArray:
    case ObjectType::Uint16MixedArray:
        newMin = Uint16ConstMin;
        newMax = Uint16ConstMax;
        goto IntArrayCommon;

    case ObjectType::Int32Array:
    case ObjectType::Int32VirtualArray:
    case ObjectType::Int32MixedArray:
    case ObjectType::Uint32Array: // int-specialized loads from uint32 arrays will bail out on values that don't fit in an int32
    case ObjectType::Uint32VirtualArray:
    case ObjectType::Uint32MixedArray:
    Int32Array:
        newMin = Int32ConstMin;
        newMax = Int32ConstMax;
        goto IntArrayCommon;

    IntArrayCommon:
        Assert(dst->IsRegOpnd());

        // If int type spec is disabled, it is ok to load int values as they can help float type spec, and merging int32 with float64 => float64.
        // But if float type spec is also disabled, we'll have problems because float64 merged with var => float64...
        if (!this->DoAggressiveIntTypeSpec() && !this->DoFloatTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 8158\n");
            if (!dstVal)
            {LOGMEIN("GlobOpt.cpp] 8160\n");
                if (srcVal)
                {LOGMEIN("GlobOpt.cpp] 8162\n");
                    dstVal = this->ValueNumberTransferDst(instr, srcVal);
                }
                else
                {
                    dstVal = NewGenericValue(profiledElementType, dst);
                }
            }
            return dstVal;
        }
        TypeSpecializeIntDst(instr, instr->m_opcode, nullptr, nullptr, nullptr, bailOutKind, newMin, newMax, &dstVal);
        toType = TyInt32;
        break;

    case ObjectType::Float32Array:
    case ObjectType::Float32VirtualArray:
    case ObjectType::Float32MixedArray:
    case ObjectType::Float64Array:
    case ObjectType::Float64VirtualArray:
    case ObjectType::Float64MixedArray:
    Float64Array:
        Assert(dst->IsRegOpnd());
        
        // If float type spec is disabled, don't load float64 values
        if (!this->DoFloatTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 8187\n");
            if (!dstVal)
            {LOGMEIN("GlobOpt.cpp] 8189\n");
                if (srcVal)
                {LOGMEIN("GlobOpt.cpp] 8191\n");
                    dstVal = this->ValueNumberTransferDst(instr, srcVal);
                }
                else
                {
                    dstVal = NewGenericValue(profiledElementType, dst);
                }
            }
            return dstVal;
        }
        TypeSpecializeFloatDst(instr, nullptr, nullptr, nullptr, &dstVal);
        toType = TyFloat64;
        break;

    default:
        Assert(baseValueType.IsLikelyNativeArray());
        bailOutKind = IR::BailOutConventionalNativeArrayAccessOnly;
        if(baseValueType.HasIntElements())
        {LOGMEIN("GlobOpt.cpp] 8209\n");
            goto Int32Array;
        }
        Assert(baseValueType.HasFloatElements());
        goto Float64Array;
    }

    if(!dstVal)
    {LOGMEIN("GlobOpt.cpp] 8217\n");
        dstVal = NewGenericValue(profiledElementType, dst);
    }

    Assert(toType != TyVar);

    GOPT_TRACE_INSTR(instr, _u("Type specialized array access.\n"));
    if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 8225\n");
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
        baseValueType.ToString(baseValueTypeStr);
        char dstValTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
        dstVal->GetValueInfo()->Type().ToString(dstValTypeStr);
        Output::Print(_u("Typed Array Optimization:  function: %s (%s): instr: %s, base value type: %S, type specialized to %s producing %S"),
            this->func->GetJITFunctionBody()->GetDisplayName(),
            this->func->GetDebugNumberSet(debugStringBuffer),
            Js::OpCodeUtil::GetOpCodeName(instr->m_opcode),
            baseValueTypeStr,
            toType == TyInt32 ? _u("int32") : _u("float64"),
            dstValTypeStr);
#if DBG_DUMP
        Output::Print(_u(" ("));
        dstVal->Dump();
        Output::Print(_u(").\n"));
#else
        Output::Print(_u(".\n"));
#endif
        Output::Flush();
    }

    if(!this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 8249\n");
        if(instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 8251\n");
            const IR::BailOutKind oldBailOutKind = instr->GetBailOutKind();
            Assert(
                (
                    !(oldBailOutKind & ~IR::BailOutKindBits) ||
                    (oldBailOutKind & ~IR::BailOutKindBits) == IR::BailOutOnImplicitCallsPreOp
                ) &&
                !(oldBailOutKind & IR::BailOutKindBits & ~(IR::BailOutOnArrayAccessHelperCall | IR::BailOutMarkTempObject)));
            if(bailOutKind == IR::BailOutConventionalTypedArrayAccessOnly)
            {LOGMEIN("GlobOpt.cpp] 8260\n");
                // BailOutConventionalTypedArrayAccessOnly also bails out if the array access is outside the head
                // segment bounds, and guarantees no implicit calls. Override the bailout kind so that the instruction
                // bails out for the right reason.
                instr->SetBailOutKind(
                    bailOutKind | (oldBailOutKind & (IR::BailOutKindBits - IR::BailOutOnArrayAccessHelperCall)));
            }
            else
            {
                // BailOutConventionalNativeArrayAccessOnly by itself may generate a helper call, and may cause implicit
                // calls to occur, so it must be merged in to eliminate generating the helper call
                Assert(bailOutKind == IR::BailOutConventionalNativeArrayAccessOnly);
                instr->SetBailOutKind(oldBailOutKind | bailOutKind);
            }
        }
        else
        {
            GenerateBailAtOperation(&instr, bailOutKind);
        }
    }

    return dstVal;
}

ValueType
GlobOpt::GetPrepassValueTypeForDst(
    const ValueType desiredValueType,
    IR::Instr *const instr,
    Value *const src1Value,
    Value *const src2Value,
    bool *const isValueInfoPreciseRef) const
{LOGMEIN("GlobOpt.cpp] 8291\n");
    // Values with definite types can be created in the loop prepass only when it is guaranteed that the value type will be the
    // same on any iteration of the loop. The heuristics currently used are:
    //     - If the source sym is not live on the back-edge, then it acquires a new value for each iteration of the loop, so
    //       that value type can be definite
    //         - Consider: A better solution for this is to track values that originate in this loop, which can have definite value
    //           types. That catches more cases, should look into that in the future.
    //     - If the source sym has a constant value that doesn't change for the duration of the function
    //     - The operation always results in a definite value type. For instance, signed bitwise operations always result in an
    //       int32, conv_num and ++ always result in a number, etc.
    //         - For operations that always result in an int32, the resulting int range is precise only if the source syms pass
    //           the above heuristics. Otherwise, the range must be expanded to the full int32 range.

    Assert(IsLoopPrePass());
    Assert(instr);

    if(isValueInfoPreciseRef)
    {LOGMEIN("GlobOpt.cpp] 8308\n");
        *isValueInfoPreciseRef = false;
    }

    if(!desiredValueType.IsDefinite())
    {LOGMEIN("GlobOpt.cpp] 8313\n");
        return desiredValueType;
    }

    if((instr->GetSrc1() && !IsPrepassSrcValueInfoPrecise(instr->GetSrc1(), src1Value)) ||
       (instr->GetSrc2() && !IsPrepassSrcValueInfoPrecise(instr->GetSrc2(), src2Value)))
    {LOGMEIN("GlobOpt.cpp] 8319\n");
        // If the desired value type is not precise, the value type of the destination is derived from the value types of the
        // sources. Since the value type of a source sym is not definite, the destination value type also cannot be definite.
        if(desiredValueType.IsInt() && OpCodeAttr::IsInt32(instr->m_opcode))
        {LOGMEIN("GlobOpt.cpp] 8323\n");
            // The op always produces an int32, but not always a tagged int
            return ValueType::GetInt(desiredValueType.IsLikelyTaggedInt());
        }
        if(desiredValueType.IsNumber() && OpCodeAttr::ProducesNumber(instr->m_opcode))
        {LOGMEIN("GlobOpt.cpp] 8328\n");
            // The op always produces a number, but not always an int
            return desiredValueType.ToDefiniteAnyNumber();
        }
        return desiredValueType.ToLikely();
    }

    if(isValueInfoPreciseRef)
    {LOGMEIN("GlobOpt.cpp] 8336\n");
        // The produced value info is derived from the sources, which have precise value infos
        *isValueInfoPreciseRef = true;
    }
    return desiredValueType;
}

bool
GlobOpt::IsPrepassSrcValueInfoPrecise(IR::Opnd *const src, Value *const srcValue) const
{LOGMEIN("GlobOpt.cpp] 8345\n");
    Assert(IsLoopPrePass());
    Assert(src);

    if(!src->IsRegOpnd() || !srcValue)
    {LOGMEIN("GlobOpt.cpp] 8350\n");
        return false;
    }

    ValueInfo *const srcValueInfo = srcValue->GetValueInfo();
    if(!srcValueInfo->IsDefinite())
    {LOGMEIN("GlobOpt.cpp] 8356\n");
        return false;
    }

    StackSym *srcSym = src->AsRegOpnd()->m_sym;
    Assert(!srcSym->IsTypeSpec());
    int32 intConstantValue;
    return
        srcSym->IsFromByteCodeConstantTable() ||
        (
            srcValueInfo->TryGetIntConstantValue(&intConstantValue) &&
            !Js::TaggedInt::IsOverflow(intConstantValue) &&
            GetTaggedIntConstantStackSym(intConstantValue) == srcSym
        ) ||
        !currentBlock->loop->regAlloc.liveOnBackEdgeSyms->Test(srcSym->m_id);
}

Value *GlobOpt::CreateDstUntransferredIntValue(
    const int32 min,
    const int32 max,
    IR::Instr *const instr,
    Value *const src1Value,
    Value *const src2Value)
{LOGMEIN("GlobOpt.cpp] 8379\n");
    Assert(instr);
    Assert(instr->GetDst());

    Assert(OpCodeAttr::ProducesNumber(instr->m_opcode)
        || (instr->m_opcode == Js::OpCode::Add_A && src1Value->GetValueInfo()->IsNumber()
        && src2Value->GetValueInfo()->IsNumber()));

    ValueType valueType(ValueType::GetInt(IntConstantBounds(min, max).IsLikelyTaggable()));
    Assert(valueType.IsInt());
    bool isValueInfoPrecise;
    if(IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 8391\n");
        valueType = GetPrepassValueTypeForDst(valueType, instr, src1Value, src2Value, &isValueInfoPrecise);
    }
    else
    {
        isValueInfoPrecise = true;
    }

    IR::Opnd *const dst = instr->GetDst();
    if(isValueInfoPrecise)
    {LOGMEIN("GlobOpt.cpp] 8401\n");
        Assert(valueType == ValueType::GetInt(IntConstantBounds(min, max).IsLikelyTaggable()));
        Assert(!(dst->IsRegOpnd() && dst->AsRegOpnd()->m_sym->IsTypeSpec()));
        return NewIntRangeValue(min, max, false, dst);
    }
    return NewGenericValue(valueType, dst);
}

Value *
GlobOpt::CreateDstUntransferredValue(
    const ValueType desiredValueType,
    IR::Instr *const instr,
    Value *const src1Value,
    Value *const src2Value)
{LOGMEIN("GlobOpt.cpp] 8415\n");
    Assert(instr);
    Assert(instr->GetDst());
    Assert(!desiredValueType.IsInt()); // use CreateDstUntransferredIntValue instead

    ValueType valueType(desiredValueType);
    if(IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 8422\n");
        valueType = GetPrepassValueTypeForDst(valueType, instr, src1Value, src2Value);
    }
    return NewGenericValue(valueType, instr->GetDst());
}

Value *
GlobOpt::ValueNumberTransferDst(IR::Instr *const instr, Value * src1Val)
{LOGMEIN("GlobOpt.cpp] 8430\n");
    Value *dstVal = this->IsLoopPrePass() ? this->ValueNumberTransferDstInPrepass(instr, src1Val) : src1Val;

    // Don't copy-prop a temp over a user symbol.  This is likely to extend the temp's lifetime, as the user symbol
    // is more likely to already have later references.
    // REVIEW: Enabling this does cause perf issues...
#if 0
    if (dstVal != src1Val)
    {LOGMEIN("GlobOpt.cpp] 8438\n");
        return dstVal;
    }

    Sym *dstSym = dst->GetStackSym();

    if (dstVal && dstSym && dstSym->IsStackSym() && !dstSym->AsStackSym()->m_isBytecodeTmp)
    {LOGMEIN("GlobOpt.cpp] 8445\n");
        Sym *dstValSym = dstVal->GetValueInfo()->GetSymStore();
        if (dstValSym && dstValSym->AsStackSym()->m_isBytecodeTmp /* src->GetIsDead()*/)
        {LOGMEIN("GlobOpt.cpp] 8448\n");
            dstVal->GetValueInfo()->SetSymStore(dstSym);
        }
    }
#endif

    return dstVal;
}

bool
GlobOpt::IsSafeToTransferInPrePass(IR::Opnd *src, Value *srcValue)
{LOGMEIN("GlobOpt.cpp] 8459\n");
    if (this->DoFieldHoisting())
    {LOGMEIN("GlobOpt.cpp] 8461\n");
        return false;
    }

    if (src->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 8466\n");
        StackSym *srcSym = src->AsRegOpnd()->m_sym;
        if (srcSym->IsFromByteCodeConstantTable())
        {LOGMEIN("GlobOpt.cpp] 8469\n");
            return true;
        }

        ValueInfo *srcValueInfo = srcValue->GetValueInfo();

        int32 srcIntConstantValue;
        if (srcValueInfo->TryGetIntConstantValue(&srcIntConstantValue) && !Js::TaggedInt::IsOverflow(srcIntConstantValue)
            && GetTaggedIntConstantStackSym(srcIntConstantValue) == srcSym)
        {LOGMEIN("GlobOpt.cpp] 8478\n");
            return true;
        }
    }

    return false;
}

Value *
GlobOpt::ValueNumberTransferDstInPrepass(IR::Instr *const instr, Value *const src1Val)
{LOGMEIN("GlobOpt.cpp] 8488\n");
    Value *dstVal = nullptr;

    if (!src1Val)
    {LOGMEIN("GlobOpt.cpp] 8492\n");
        return nullptr;
    }

    bool isValueInfoPrecise;
    ValueInfo *const src1ValueInfo = src1Val->GetValueInfo();

    // TODO: This conflicts with new values created by the type specialization code
    //       We should re-enable if we change that code to avoid the new values.
#if 0
    if (this->IsSafeToTransferInPrePass(instr->GetSrc1(), src1Val))
    {LOGMEIN("GlobOpt.cpp] 8503\n");
        return src1Val;
    }


    if (this->IsPREInstrCandidateLoad(instr->m_opcode) && instr->GetDst())
    {LOGMEIN("GlobOpt.cpp] 8509\n");
        StackSym *dstSym = instr->GetDst()->AsRegOpnd()->m_sym;

        for (Loop *curLoop = this->currentBlock->loop; curLoop; curLoop = curLoop->parent)
        {LOGMEIN("GlobOpt.cpp] 8513\n");
            if (curLoop->fieldPRESymStore->Test(dstSym->m_id))
            {LOGMEIN("GlobOpt.cpp] 8515\n");
                return src1Val;
            }
        }
    }

    if (!this->DoFieldHoisting())
    {LOGMEIN("GlobOpt.cpp] 8522\n");
        if (instr->GetDst()->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 8524\n");
            StackSym *stackSym = instr->GetDst()->AsRegOpnd()->m_sym;

            if (stackSym->IsSingleDef() || this->IsLive(stackSym, this->prePassLoop->landingPad))
            {LOGMEIN("GlobOpt.cpp] 8528\n");
                IntConstantBounds src1IntConstantBounds;
                if (src1ValueInfo->TryGetIntConstantBounds(&src1IntConstantBounds) &&
                    !(
                        src1IntConstantBounds.LowerBound() == INT32_MIN &&
                        src1IntConstantBounds.UpperBound() == INT32_MAX
                        ))
                {LOGMEIN("GlobOpt.cpp] 8535\n");
                    const ValueType valueType(
                        GetPrepassValueTypeForDst(src1ValueInfo->Type(), instr, src1Val, nullptr, &isValueInfoPrecise));
                    if (isValueInfoPrecise)
                    {LOGMEIN("GlobOpt.cpp] 8539\n");
                        return src1Val;
                    }
                }
                else
                {
                    return src1Val;
                }
            }
        }
    }
#endif

    // Src1's value could change later in the loop, so the value wouldn't be the same for each
    // iteration.  Since we don't iterate over loops "while (!changed)", go conservative on the
    // first pass when transferring a value that is live on the back-edge.

    // In prepass we are going to copy the value but with a different value number
    // for aggressive int type spec.
    const ValueType valueType(GetPrepassValueTypeForDst(src1ValueInfo->Type(), instr, src1Val, nullptr, &isValueInfoPrecise));
    if(isValueInfoPrecise || (valueType == src1ValueInfo->Type() && src1ValueInfo->IsGeneric()))
    {LOGMEIN("GlobOpt.cpp] 8560\n");
        Assert(valueType == src1ValueInfo->Type());
        dstVal = CopyValue(src1Val);
        TrackCopiedValueForKills(dstVal);
    }
    else
    {
        dstVal = NewGenericValue(valueType);
        dstVal->GetValueInfo()->SetSymStore(src1ValueInfo->GetSymStore());
    }

    return dstVal;
}

void
GlobOpt::PropagateIntRangeForNot(int32 minimum, int32 maximum, int32 *pNewMin, int32* pNewMax)
{LOGMEIN("GlobOpt.cpp] 8576\n");
    int32 tmp;
    Int32Math::Not(minimum, pNewMin);
    *pNewMax = *pNewMin;
    Int32Math::Not(maximum, &tmp);
    *pNewMin = min(*pNewMin, tmp);
    *pNewMax = max(*pNewMax, tmp);
}

void
GlobOpt::PropagateIntRangeBinary(IR::Instr *instr, int32 min1, int32 max1,
    int32 min2, int32 max2, int32 *pNewMin, int32* pNewMax)
{LOGMEIN("GlobOpt.cpp] 8588\n");
    int32 min, max, tmp, tmp2;

    min = INT32_MIN;
    max = INT32_MAX;

    switch (instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 8595\n");
    case Js::OpCode::Xor_A:
    case Js::OpCode::Or_A:
        // Find range with highest high order bit
        tmp = ::max((uint32)min1, (uint32)max1);
        tmp2 = ::max((uint32)min2, (uint32)max2);

        if ((uint32)tmp > (uint32)tmp2)
        {LOGMEIN("GlobOpt.cpp] 8603\n");
            max = tmp;
        }
        else
        {
            max = tmp2;
        }

        if (max < 0)
        {LOGMEIN("GlobOpt.cpp] 8612\n");
            min = INT32_MIN;  // REVIEW: conservative...
            max = INT32_MAX;
        }
        else
        {
            // Turn values like 0x1010 into 0x1111
            max = 1 << Math::Log2(max);
            max = (uint32)(max << 1) - 1;
            min = 0;
        }

        break;

    case Js::OpCode::And_A:

        if (min1 == INT32_MIN && min2 == INT32_MIN)
        {LOGMEIN("GlobOpt.cpp] 8629\n");
            // Shortcut
            break;
        }

        // Find range with lowest higher bit
        tmp = ::max((uint32)min1, (uint32)max1);
        tmp2 = ::max((uint32)min2, (uint32)max2);

        if ((uint32)tmp < (uint32)tmp2)
        {LOGMEIN("GlobOpt.cpp] 8639\n");
            min = min1;
            max = max1;
        }
        else
        {
            min = min2;
            max = max2;
        }

        // To compute max, look if min has higher high bit
        if ((uint32)min > (uint32)max)
        {LOGMEIN("GlobOpt.cpp] 8651\n");
            max = min;
        }

        // If max is negative, max let's assume it could be -1, so result in MAX_INT
        if (max < 0)
        {LOGMEIN("GlobOpt.cpp] 8657\n");
            max = INT32_MAX;
        }

        // If min is positive, the resulting min is zero
        if (min >= 0)
        {LOGMEIN("GlobOpt.cpp] 8663\n");
            min = 0;
        }
        else
        {
            min = INT32_MIN;
        }
        break;

    case Js::OpCode::Shl_A:
        {LOGMEIN("GlobOpt.cpp] 8673\n");
            // Shift count
            if (min2 != max2 && ((uint32)min2 > 0x1F || (uint32)max2 > 0x1F))
            {LOGMEIN("GlobOpt.cpp] 8676\n");
                min2 = 0;
                max2 = 0x1F;
            }
            else
            {
                min2 &= 0x1F;
                max2 &= 0x1F;
            }

            int32 min1FreeTopBitCount = min1 ? (sizeof(int32) * 8) - (Math::Log2(min1) + 1) : (sizeof(int32) * 8);
            int32 max1FreeTopBitCount = max1 ? (sizeof(int32) * 8) - (Math::Log2(max1) + 1) : (sizeof(int32) * 8);
            if (min1FreeTopBitCount <= max2 || max1FreeTopBitCount <= max2)
            {LOGMEIN("GlobOpt.cpp] 8689\n");
                // If the shift is going to touch the sign bit return the max range
                min = INT32_MIN;
                max = INT32_MAX;
            }
            else
            {
                // Compute max
                // Turn values like 0x1010 into 0x1111
                if (min1)
                {LOGMEIN("GlobOpt.cpp] 8699\n");
                    min1 = 1 << Math::Log2(min1);
                    min1 = (min1 << 1) - 1;
                }
                if (max1)
                {LOGMEIN("GlobOpt.cpp] 8704\n");
                    max1 = 1 << Math::Log2(max1);
                    max1 = (uint32)(max1 << 1) - 1;
                }

                if (max1 > 0)
                {LOGMEIN("GlobOpt.cpp] 8710\n");
                    int32 nrTopBits = (sizeof(int32) * 8) - Math::Log2(max1);
                    if (nrTopBits < ::min(max2, 30))
                        max = INT32_MAX;
                    else
                        max = ::max((max1 << ::min(max2, 30)) & ~0x80000000, (min1 << min2) & ~0x80000000);
                }
                else
                {
                    max = (max1 << min2) & ~0x80000000;
                }
                // Compute min

                if (min1 < 0)
                {LOGMEIN("GlobOpt.cpp] 8724\n");
                    min = ::min(min1 << max2, max1 << max2);
                }
                else
                {
                    min = ::min(min1 << min2, max1 << max2);
                }
                // Turn values like 0x1110 into 0x1000
                if (min)
                {LOGMEIN("GlobOpt.cpp] 8733\n");
                    min = 1 << Math::Log2(min);
                }
            }
        }
        break;

    case Js::OpCode::Shr_A:
        // Shift count
        if (min2 != max2 && ((uint32)min2 > 0x1F || (uint32)max2 > 0x1F))
        {LOGMEIN("GlobOpt.cpp] 8743\n");
            min2 = 0;
            max2 = 0x1F;
        }
        else
        {
            min2 &= 0x1F;
            max2 &= 0x1F;
        }

        // Compute max

        if (max1 < 0)
        {LOGMEIN("GlobOpt.cpp] 8756\n");
            max = max1 >> max2;
        }
        else
        {
            max = max1 >> min2;
        }

        // Compute min

        if (min1 < 0)
        {LOGMEIN("GlobOpt.cpp] 8767\n");
            min = min1 >> min2;
        }
        else
        {
            min = min1 >> max2;
        }
        break;

    case Js::OpCode::ShrU_A:

        // shift count is constant zero
        if ((min2 == max2) && (max2 & 0x1f) == 0)
        {LOGMEIN("GlobOpt.cpp] 8780\n");
            // We can't encode uint32 result, so it has to be used as int32 only or the original value is positive.
            Assert(instr->ignoreIntOverflow || min1 >= 0);
            // We can transfer the signed int32 range.
            min = min1;
            max = max1;
            break;
        }

        const IntConstantBounds src2NewBounds = IntConstantBounds(min2, max2).And_0x1f();
        // Zero is only allowed if result is always a signed int32 or always used as a signed int32
        Assert(min1 >= 0 || instr->ignoreIntOverflow || !src2NewBounds.Contains(0));
        min2 = src2NewBounds.LowerBound();
        max2 = src2NewBounds.UpperBound();

        Assert(min2 <= max2);
        // zero shift count is only allowed if result is used as int32 and/or value is positive
        Assert(min2 > 0 || instr->ignoreIntOverflow || min1 >= 0);

        uint32 umin1 = (uint32)min1;
        uint32 umax1 = (uint32)max1;

        if (umin1 > umax1)
        {LOGMEIN("GlobOpt.cpp] 8803\n");
            uint32 temp = umax1;
            umax1 = umin1;
            umin1 = temp;
        }

        Assert(min2 >= 0 && max2 < 32);

        // Compute max

        if (min1 < 0)
        {LOGMEIN("GlobOpt.cpp] 8814\n");
            umax1 = UINT32_MAX;
        }
        max = umax1 >> min2;

        // Compute min

        if (min1 <= 0 && max1 >=0)
        {LOGMEIN("GlobOpt.cpp] 8822\n");
            min = 0;
        }
        else
        {
            min = umin1 >> max2;
        }

        // We should be able to fit uint32 range as int32
        Assert(instr->ignoreIntOverflow || (min >= 0 && max >= 0) );
        if (min > max)
        {LOGMEIN("GlobOpt.cpp] 8833\n");
            // can only happen if shift count can be zero
            Assert(min2 == 0 && (instr->ignoreIntOverflow || min1 >= 0));
            min = Int32ConstMin;
            max = Int32ConstMax;
        }

        break;
    }

    *pNewMin = min;
    *pNewMax = max;
}

IR::Instr *
GlobOpt::TypeSpecialization(
    IR::Instr *instr,
    Value **pSrc1Val,
    Value **pSrc2Val,
    Value **pDstVal,
    bool *redoTypeSpecRef,
    bool *const forceInvariantHoistingRef)
{LOGMEIN("GlobOpt.cpp] 8855\n");
    Value *&src1Val = *pSrc1Val;
    Value *&src2Val = *pSrc2Val;
    *redoTypeSpecRef = false;
    Assert(!*forceInvariantHoistingRef);

    this->ignoredIntOverflowForCurrentInstr = false;
    this->ignoredNegativeZeroForCurrentInstr = false;

    // - Int32 values that can't be tagged are created as float constant values instead because a JavascriptNumber var is needed
    //   for that value at runtime. For the purposes of type specialization, recover the int32 values so that they will be
    //   treated as ints.
    // - If int overflow does not matter for the instruction, we can additionally treat uint32 values as int32 values because
    //   the value resulting from the operation will eventually be converted to int32 anyway
    Value *const src1OriginalVal = src1Val;
    Value *const src2OriginalVal = src2Val;

#ifdef ENABLE_SIMDJS
    // SIMD_JS
    if (TypeSpecializeSimd128(instr, pSrc1Val, pSrc2Val, pDstVal))
    {LOGMEIN("GlobOpt.cpp] 8875\n");
        return instr;
    }
#endif

    if(!instr->ShouldCheckForIntOverflow())
    {LOGMEIN("GlobOpt.cpp] 8881\n");
        if(src1Val && src1Val->GetValueInfo()->IsFloatConstant())
        {LOGMEIN("GlobOpt.cpp] 8883\n");
            int32 int32Value;
            bool isInt32;
            if(Js::JavascriptNumber::TryGetInt32OrUInt32Value(
                    src1Val->GetValueInfo()->AsFloatConstant()->FloatValue(),
                    &int32Value,
                    &isInt32))
            {LOGMEIN("GlobOpt.cpp] 8890\n");
                src1Val = GetIntConstantValue(int32Value, instr);
                if(!isInt32)
                {LOGMEIN("GlobOpt.cpp] 8893\n");
                    this->ignoredIntOverflowForCurrentInstr = true;
                }
            }
        }
        if(src2Val && src2Val->GetValueInfo()->IsFloatConstant())
        {LOGMEIN("GlobOpt.cpp] 8899\n");
            int32 int32Value;
            bool isInt32;
            if(Js::JavascriptNumber::TryGetInt32OrUInt32Value(
                    src2Val->GetValueInfo()->AsFloatConstant()->FloatValue(),
                    &int32Value,
                    &isInt32))
            {LOGMEIN("GlobOpt.cpp] 8906\n");
                src2Val = GetIntConstantValue(int32Value, instr);
                if(!isInt32)
                {LOGMEIN("GlobOpt.cpp] 8909\n");
                    this->ignoredIntOverflowForCurrentInstr = true;
                }
            }
        }
    }
    const AutoRestoreVal autoRestoreSrc1Val(src1OriginalVal, &src1Val);
    const AutoRestoreVal autoRestoreSrc2Val(src2OriginalVal, &src2Val);

    if (src1Val && instr->GetSrc2() == nullptr)
    {LOGMEIN("GlobOpt.cpp] 8919\n");
        // Unary
        // Note make sure that native array StElemI gets to TypeSpecializeStElem. Do this for typed arrays, too?
        int32 intConstantValue;
        if (!this->IsLoopPrePass() &&
            !instr->IsBranchInstr() &&
            src1Val->GetValueInfo()->TryGetIntConstantValue(&intConstantValue) &&
            !(
                // Nothing to fold for element stores. Go into type specialization to see if they can at least be specialized.
                instr->m_opcode == Js::OpCode::StElemI_A ||
                instr->m_opcode == Js::OpCode::StElemI_A_Strict ||
                instr->m_opcode == Js::OpCode::StElemC ||
                instr->m_opcode == Js::OpCode::MultiBr ||
                instr->m_opcode == Js::OpCode::InlineArrayPop
            ))
        {
            if (OptConstFoldUnary(&instr, intConstantValue, src1Val == src1OriginalVal, pDstVal))
            {LOGMEIN("GlobOpt.cpp] 8936\n");
                return instr;
            }
        }
        else if (this->TypeSpecializeUnary(
                    &instr,
                    &src1Val,
                    pDstVal,
                    src1OriginalVal,
                    redoTypeSpecRef,
                    forceInvariantHoistingRef))
        {LOGMEIN("GlobOpt.cpp] 8947\n");
            return instr;
        }
        else if(*redoTypeSpecRef)
        {LOGMEIN("GlobOpt.cpp] 8951\n");
            return instr;
        }
    }
    else if (instr->GetSrc2() && !instr->IsBranchInstr())
    {LOGMEIN("GlobOpt.cpp] 8956\n");
        // Binary
        if (!this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 8959\n");
            // OptConstFoldBinary doesn't do type spec, so only deal with things we are sure are int (IntConstant and IntRange)
            // and not just likely ints  TypeSpecializeBinary will deal with type specializing them and fold them again
            IntConstantBounds src1IntConstantBounds, src2IntConstantBounds;
            if (src1Val && src1Val->GetValueInfo()->TryGetIntConstantBounds(&src1IntConstantBounds))
            {LOGMEIN("GlobOpt.cpp] 8964\n");
                if (src2Val && src2Val->GetValueInfo()->TryGetIntConstantBounds(&src2IntConstantBounds))
                {LOGMEIN("GlobOpt.cpp] 8966\n");
                    if (this->OptConstFoldBinary(&instr, src1IntConstantBounds, src2IntConstantBounds, pDstVal))
                    {LOGMEIN("GlobOpt.cpp] 8968\n");
                        return instr;
                    }
                }
            }
        }
    }
    if (instr->GetSrc2() && this->TypeSpecializeBinary(&instr, pSrc1Val, pSrc2Val, pDstVal, src1OriginalVal, src2OriginalVal, redoTypeSpecRef))
    {LOGMEIN("GlobOpt.cpp] 8976\n");
        if (!this->IsLoopPrePass() &&
            instr->m_opcode != Js::OpCode::Nop &&
            instr->m_opcode != Js::OpCode::Br &&    // We may have const fold a branch

            // Cannot const-peep if the result of the operation is required for a bailout check
            !(instr->HasBailOutInfo() && instr->GetBailOutKind() & IR::BailOutOnResultConditions))
        {LOGMEIN("GlobOpt.cpp] 8983\n");
            if (src1Val && src1Val->GetValueInfo()->HasIntConstantValue())
            {LOGMEIN("GlobOpt.cpp] 8985\n");
                if (this->OptConstPeep(instr, instr->GetSrc1(), pDstVal, src1Val->GetValueInfo()))
                {LOGMEIN("GlobOpt.cpp] 8987\n");
                    return instr;
                }
            }
            else if (src2Val && src2Val->GetValueInfo()->HasIntConstantValue())
            {LOGMEIN("GlobOpt.cpp] 8992\n");
                if (this->OptConstPeep(instr, instr->GetSrc2(), pDstVal, src2Val->GetValueInfo()))
                {LOGMEIN("GlobOpt.cpp] 8994\n");
                    return instr;
                }
            }
        }
        return instr;
    }
    else if(*redoTypeSpecRef)
    {LOGMEIN("GlobOpt.cpp] 9002\n");
        return instr;
    }

    if (instr->IsBranchInstr() && !this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 9007\n");
        if (this->OptConstFoldBranch(instr, src1Val, src2Val, pDstVal))
        {LOGMEIN("GlobOpt.cpp] 9009\n");
            return instr;
        }
    }
    // We didn't type specialize, make sure the srcs are unspecialized
    IR::Opnd *src1 = instr->GetSrc1();
    if (src1)
    {LOGMEIN("GlobOpt.cpp] 9016\n");
        instr = this->ToVarUses(instr, src1, false, src1Val);

        IR::Opnd *src2 = instr->GetSrc2();
        if (src2)
        {LOGMEIN("GlobOpt.cpp] 9021\n");
            instr = this->ToVarUses(instr, src2, false, src2Val);
        }
    }

    IR::Opnd *dst = instr->GetDst();
    if (dst)
    {LOGMEIN("GlobOpt.cpp] 9028\n");
        instr = this->ToVarUses(instr, dst, true, nullptr);

        // Handling for instructions other than built-ins that may require only dst type specialization
        // should be added here.
        if(OpCodeAttr::IsInlineBuiltIn(instr->m_opcode) && !GetIsAsmJSFunc()) // don't need to do typespec for asmjs
        {LOGMEIN("GlobOpt.cpp] 9034\n");
            this->TypeSpecializeInlineBuiltInDst(&instr, pDstVal);
            return instr;
        }

        // Clear the int specialized bit on the dst.
        if (dst->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 9041\n");
            IR::RegOpnd *dstRegOpnd = dst->AsRegOpnd();
            if (!dstRegOpnd->m_sym->IsTypeSpec())
            {LOGMEIN("GlobOpt.cpp] 9044\n");
                this->ToVarRegOpnd(dstRegOpnd, this->currentBlock);
            }
            else if (dstRegOpnd->m_sym->IsInt32())
            {LOGMEIN("GlobOpt.cpp] 9048\n");
                this->ToInt32Dst(instr, dstRegOpnd, this->currentBlock);
            }
            else if (dstRegOpnd->m_sym->IsUInt32() && GetIsAsmJSFunc())
            {LOGMEIN("GlobOpt.cpp] 9052\n");
                this->ToUInt32Dst(instr, dstRegOpnd, this->currentBlock);
            }
            else if (dstRegOpnd->m_sym->IsFloat64())
            {LOGMEIN("GlobOpt.cpp] 9056\n");
                this->ToFloat64Dst(instr, dstRegOpnd, this->currentBlock);
            }
        }
        else if (dst->IsSymOpnd() && dst->AsSymOpnd()->m_sym->IsStackSym())
        {LOGMEIN("GlobOpt.cpp] 9061\n");
            this->ToVarStackSym(dst->AsSymOpnd()->m_sym->AsStackSym(), this->currentBlock);
        }
    }

    return instr;
}

bool
GlobOpt::OptConstPeep(IR::Instr *instr, IR::Opnd *constSrc, Value **pDstVal, ValueInfo *valuInfo)
{LOGMEIN("GlobOpt.cpp] 9071\n");
    int32 value;
    IR::Opnd *src;
    IR::Opnd *nonConstSrc = (constSrc == instr->GetSrc1() ? instr->GetSrc2() : instr->GetSrc1());

    // Try to find the value from value info first
    if (valuInfo->TryGetIntConstantValue(&value))
    {LOGMEIN("GlobOpt.cpp] 9078\n");
    }
    else if (constSrc->IsAddrOpnd())
    {LOGMEIN("GlobOpt.cpp] 9081\n");
        IR::AddrOpnd *addrOpnd = constSrc->AsAddrOpnd();
#ifdef _M_X64
        Assert(addrOpnd->IsVar() || Math::FitsInDWord((size_t)addrOpnd->m_address));
#else
        Assert(sizeof(value) == sizeof(addrOpnd->m_address));
#endif

        if (addrOpnd->IsVar())
        {LOGMEIN("GlobOpt.cpp] 9090\n");
            value = Js::TaggedInt::ToInt32(addrOpnd->m_address);
        }
        else
        {
            // We asserted that the address will fit in a DWORD above
            value = ::Math::PointerCastToIntegral<int32>(constSrc->AsAddrOpnd()->m_address);
        }
    }
    else if (constSrc->IsIntConstOpnd())
    {LOGMEIN("GlobOpt.cpp] 9100\n");
        value = constSrc->AsIntConstOpnd()->AsInt32();
    }
    else
    {
        return false;
    }

    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 9109\n");
        // Can't do all Add_A because of string concats.
        // Sub_A cannot be transformed to a NEG_A because 0 - 0 != -0
    case Js::OpCode::Add_A:
        src = nonConstSrc;

        if (!src->GetValueType().IsInt())
        {LOGMEIN("GlobOpt.cpp] 9116\n");
            // 0 + -0  != -0
            // "Foo" + 0 != "Foo
            return false;
        }
        // fall-through

    case Js::OpCode::Add_Ptr:
    case Js::OpCode::Add_I4:
        if (value != 0)
        {LOGMEIN("GlobOpt.cpp] 9126\n");
            return false;
        }
        if (constSrc == instr->GetSrc1())
        {LOGMEIN("GlobOpt.cpp] 9130\n");
            src = instr->GetSrc2();
        }
        else
        {
            src = instr->GetSrc1();
        }
        break;

    case Js::OpCode::Mul_A:
    case Js::OpCode::Mul_I4:
        if (value == 0)
        {LOGMEIN("GlobOpt.cpp] 9142\n");
            // -0 * 0 != 0
            return false;
        }
        else if (value == 1)
        {LOGMEIN("GlobOpt.cpp] 9147\n");
            src = nonConstSrc;
        }
        else
        {
            return false;
        }
        break;

    case Js::OpCode::Div_A:
        if (value == 1 && constSrc == instr->GetSrc2())
        {LOGMEIN("GlobOpt.cpp] 9158\n");
            src = instr->GetSrc1();
        }
        else
        {
            return false;
        }
        break;

    case Js::OpCode::Or_I4:
        if (value == -1)
        {LOGMEIN("GlobOpt.cpp] 9169\n");
            src = constSrc;
        }
        else if (value == 0)
        {LOGMEIN("GlobOpt.cpp] 9173\n");
            src = nonConstSrc;
        }
        else
        {
            return false;
        }
        break;

    case Js::OpCode::And_I4:
        if (value == -1)
        {LOGMEIN("GlobOpt.cpp] 9184\n");
            src = nonConstSrc;
        }
        else if (value == 0)
        {LOGMEIN("GlobOpt.cpp] 9188\n");
            src = constSrc;
        }
        else
        {
            return false;
        }
        break;

    case Js::OpCode::Shl_I4:
    case Js::OpCode::ShrU_I4:
    case Js::OpCode::Shr_I4:
        if (value != 0 || constSrc != instr->GetSrc2())
        {LOGMEIN("GlobOpt.cpp] 9201\n");
            return false;
        }
        src = instr->GetSrc1();
        break;

    default:
        return false;
    }

    this->CaptureByteCodeSymUses(instr);

    if (src == instr->GetSrc1())
    {LOGMEIN("GlobOpt.cpp] 9214\n");
        instr->FreeSrc2();
    }
    else
    {
        Assert(src == instr->GetSrc2());
        instr->ReplaceSrc1(instr->UnlinkSrc2());
    }

    instr->m_opcode = Js::OpCode::Ld_A;

    return true;
}

Js::Var // TODO: michhol OOP JIT, shouldn't play with Vars
GlobOpt::GetConstantVar(IR::Opnd *opnd, Value *val)
{LOGMEIN("GlobOpt.cpp] 9230\n");
    ValueInfo *valueInfo = val->GetValueInfo();

    if (valueInfo->IsVarConstant() && valueInfo->IsPrimitive())
    {LOGMEIN("GlobOpt.cpp] 9234\n");
        return valueInfo->AsVarConstant()->VarValue();
    }
    if (opnd->IsAddrOpnd())
    {LOGMEIN("GlobOpt.cpp] 9238\n");
        IR::AddrOpnd *addrOpnd = opnd->AsAddrOpnd();
        if (addrOpnd->IsVar())
        {LOGMEIN("GlobOpt.cpp] 9241\n");
            return addrOpnd->m_address;
        }
    }
    else if (opnd->IsIntConstOpnd())
    {LOGMEIN("GlobOpt.cpp] 9246\n");
        if (!Js::TaggedInt::IsOverflow(opnd->AsIntConstOpnd()->AsInt32()))
        {LOGMEIN("GlobOpt.cpp] 9248\n");
            return Js::TaggedInt::ToVarUnchecked(opnd->AsIntConstOpnd()->AsInt32());
        }
    }
    else if (opnd->IsRegOpnd() && opnd->AsRegOpnd()->m_sym->IsSingleDef())
    {LOGMEIN("GlobOpt.cpp] 9253\n");
        if (valueInfo->IsBoolean())
        {LOGMEIN("GlobOpt.cpp] 9255\n");
            IR::Instr * defInstr = opnd->AsRegOpnd()->m_sym->GetInstrDef();

            if (defInstr->m_opcode != Js::OpCode::Ld_A || !defInstr->GetSrc1()->IsAddrOpnd())
            {LOGMEIN("GlobOpt.cpp] 9259\n");
                return nullptr;
            }
            Assert(defInstr->GetSrc1()->AsAddrOpnd()->IsVar());
            return defInstr->GetSrc1()->AsAddrOpnd()->m_address;
        }
        else if (valueInfo->IsUndefined())
        {LOGMEIN("GlobOpt.cpp] 9266\n");
            return (Js::Var)this->func->GetScriptContextInfo()->GetUndefinedAddr();
        }
        else if (valueInfo->IsNull())
        {LOGMEIN("GlobOpt.cpp] 9270\n");
            return (Js::Var)this->func->GetScriptContextInfo()->GetNullAddr();
        }
    }

    return nullptr;
}

bool BoolAndIntStaticAndTypeMismatch(Value* src1Val, Value* src2Val, Js::Var src1Var, Js::Var src2Var)
{LOGMEIN("GlobOpt.cpp] 9279\n");
    ValueInfo *src1ValInfo = src1Val->GetValueInfo();
    ValueInfo *src2ValInfo = src2Val->GetValueInfo();
    return (src1ValInfo->IsNumber() && src1Var && src2ValInfo->IsBoolean() && src1Var != Js::TaggedInt::ToVarUnchecked(0) && src1Var != Js::TaggedInt::ToVarUnchecked(1)) ||
        (src2ValInfo->IsNumber() && src2Var && src1ValInfo->IsBoolean() && src2Var != Js::TaggedInt::ToVarUnchecked(0) && src2Var != Js::TaggedInt::ToVarUnchecked(1));
}

bool
GlobOpt::OptConstFoldBranch(IR::Instr *instr, Value *src1Val, Value*src2Val, Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 9288\n");
    if (!src1Val)
    {LOGMEIN("GlobOpt.cpp] 9290\n");
        return false;
    }

    Js::Var src1Var = this->GetConstantVar(instr->GetSrc1(), src1Val);

    Js::Var src2Var = nullptr;

    if (instr->GetSrc2())
    {LOGMEIN("GlobOpt.cpp] 9299\n");
        if (!src2Val)
        {LOGMEIN("GlobOpt.cpp] 9301\n");
            return false;
        }

        src2Var = this->GetConstantVar(instr->GetSrc2(), src2Val);
    }

    // Make sure GetConstantVar only returns primitives.
    // TODO: OOP JIT, enabled these asserts
    //Assert(!src1Var || !Js::JavascriptOperators::IsObject(src1Var));
    //Assert(!src2Var || !Js::JavascriptOperators::IsObject(src2Var));

    BOOL result;
    int32 constVal;
    switch (instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 9316\n");
    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrNotNeq_A:
        if (!src1Var || !src2Var)
        {
            if (BoolAndIntStaticAndTypeMismatch(src1Val, src2Val, src1Var, src2Var))
            {LOGMEIN("GlobOpt.cpp] 9322\n");
                    result = false;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (func->IsOOPJIT() || !CONFIG_FLAG(OOPJITMissingOpts))
            {LOGMEIN("GlobOpt.cpp] 9333\n");
                // TODO: OOP JIT, const folding
                return false;
            }
            result = Js::JavascriptOperators::Equal(src1Var, src2Var, this->func->GetScriptContext());
        }
        break;
    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrNotEq_A:
        if (!src1Var || !src2Var)
        {
            if (BoolAndIntStaticAndTypeMismatch(src1Val, src2Val, src1Var, src2Var))
            {LOGMEIN("GlobOpt.cpp] 9345\n");
                result = true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (func->IsOOPJIT() || !CONFIG_FLAG(OOPJITMissingOpts))
            {LOGMEIN("GlobOpt.cpp] 9356\n");
                // TODO: OOP JIT, const folding
                return false;
            }
            result = Js::JavascriptOperators::NotEqual(src1Var, src2Var, this->func->GetScriptContext());
        }
        break;
    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrSrNotNeq_A:
        if (!src1Var || !src2Var)
        {LOGMEIN("GlobOpt.cpp] 9366\n");
            ValueInfo *src1ValInfo = src1Val->GetValueInfo();
            ValueInfo *src2ValInfo = src2Val->GetValueInfo();
            if (
                (src1ValInfo->IsUndefined() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenUndefined()) ||
                (src1ValInfo->IsNull() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenNull()) ||
                (src1ValInfo->IsBoolean() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenBoolean()) ||
                (src1ValInfo->IsNumber() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenNumber()) ||
                (src1ValInfo->IsString() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenString()) ||

                (src2ValInfo->IsUndefined() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenUndefined()) ||
                (src2ValInfo->IsNull() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenNull()) ||
                (src2ValInfo->IsBoolean() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenBoolean()) ||
                (src2ValInfo->IsNumber() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenNumber()) ||
                (src2ValInfo->IsString() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenString())
               )
            {LOGMEIN("GlobOpt.cpp] 9382\n");
                result = false;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (func->IsOOPJIT() || !CONFIG_FLAG(OOPJITMissingOpts))
            {LOGMEIN("GlobOpt.cpp] 9393\n");
                // TODO: OOP JIT, const folding
                return false;
            }
            result = Js::JavascriptOperators::StrictEqual(src1Var, src2Var, this->func->GetScriptContext());
        }
        break;

    case Js::OpCode::BrSrNeq_A:
    case Js::OpCode::BrSrNotEq_A:
        if (!src1Var || !src2Var)
        {LOGMEIN("GlobOpt.cpp] 9404\n");
            ValueInfo *src1ValInfo = src1Val->GetValueInfo();
            ValueInfo *src2ValInfo = src2Val->GetValueInfo();
            if (
                (src1ValInfo->IsUndefined() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenUndefined()) ||
                (src1ValInfo->IsNull() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenNull()) ||
                (src1ValInfo->IsBoolean() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenBoolean()) ||
                (src1ValInfo->IsNumber() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenNumber()) ||
                (src1ValInfo->IsString() && src2ValInfo->IsDefinite() && !src2ValInfo->HasBeenString()) ||

                (src2ValInfo->IsUndefined() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenUndefined()) ||
                (src2ValInfo->IsNull() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenNull()) ||
                (src2ValInfo->IsBoolean() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenBoolean()) ||
                (src2ValInfo->IsNumber() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenNumber()) ||
                (src2ValInfo->IsString() && src1ValInfo->IsDefinite() && !src1ValInfo->HasBeenString())
               )
            {LOGMEIN("GlobOpt.cpp] 9420\n");
                result = true;
            }
            else
            {
                return false;
            }
        }
        else
        {
            if (func->IsOOPJIT() || !CONFIG_FLAG(OOPJITMissingOpts))
            {LOGMEIN("GlobOpt.cpp] 9431\n");
                // TODO: OOP JIT, const folding
                return false;
            }
            result = Js::JavascriptOperators::NotStrictEqual(src1Var, src2Var, this->func->GetScriptContext());
        }
        break;

    case Js::OpCode::BrFalse_A:
    case Js::OpCode::BrTrue_A:
    {LOGMEIN("GlobOpt.cpp] 9441\n");
        ValueInfo *const src1ValueInfo = src1Val->GetValueInfo();
        if(src1ValueInfo->IsNull() || src1ValueInfo->IsUndefined())
        {LOGMEIN("GlobOpt.cpp] 9444\n");
            result = instr->m_opcode == Js::OpCode::BrFalse_A;
            break;
        }
        if(src1ValueInfo->IsObject() && src1ValueInfo->GetObjectType() > ObjectType::Object)
        {LOGMEIN("GlobOpt.cpp] 9449\n");
            // Specific object types that are tracked are equivalent to 'true'
            result = instr->m_opcode == Js::OpCode::BrTrue_A;
            break;
        }

        if (func->IsOOPJIT() || !CONFIG_FLAG(OOPJITMissingOpts))
        {LOGMEIN("GlobOpt.cpp] 9456\n");
            // TODO: OOP JIT, const folding
            return false;
        }
        if (!src1Var)
        {LOGMEIN("GlobOpt.cpp] 9461\n");
            return false;
        }
        result = Js::JavascriptConversion::ToBoolean(src1Var, this->func->GetScriptContext());
        if(instr->m_opcode == Js::OpCode::BrFalse_A)
        {LOGMEIN("GlobOpt.cpp] 9466\n");
            result = !result;
        }
        break;
    }
    case Js::OpCode::BrFalse_I4:
        // this path would probably work outside of asm.js, but we should verify that if we ever hit this scenario
        Assert(GetIsAsmJSFunc());
        constVal = 0;
        if (src1Val->GetValueInfo()->TryGetIntConstantValue(&constVal) && constVal != 0)
        {LOGMEIN("GlobOpt.cpp] 9476\n");
            instr->FreeSrc1();
            if (instr->GetSrc2())
            {LOGMEIN("GlobOpt.cpp] 9479\n");
                instr->FreeSrc2();
            }
            instr->m_opcode = Js::OpCode::Nop;
            return true;
        }
        return false;

    default:
        return false;
    }

    this->OptConstFoldBr(!!result, instr);

    return true;
}

bool
GlobOpt::OptConstFoldUnary(
    IR::Instr * *pInstr,
    const int32 intConstantValue,
    const bool isUsingOriginalSrc1Value,
    Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 9502\n");
    IR::Instr * &instr = *pInstr;
    int32 value = 0;
    IR::Opnd *constOpnd;
    bool isInt = true;
    bool doSetDstVal = true;
    FloatConstType fValue = 0.0;

    if (!DoConstFold())
    {LOGMEIN("GlobOpt.cpp] 9511\n");
        return false;
    }

    if (instr->GetDst() && !instr->GetDst()->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 9516\n");
        return false;
    }

    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 9521\n");
    case Js::OpCode::Neg_A:
        if (intConstantValue == 0)
        {LOGMEIN("GlobOpt.cpp] 9524\n");
            // Could fold to -0.0
            return false;
        }

        if (Int32Math::Neg(intConstantValue, &value))
        {LOGMEIN("GlobOpt.cpp] 9530\n");
            return false;
        }
        break;

    case Js::OpCode::Not_A:
        Int32Math::Not(intConstantValue, &value);
        break;

    case Js::OpCode::Ld_A:
        if (instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 9541\n");
            //The profile data for switch expr can be string and in GlobOpt we realize it is an int.
            if(instr->GetBailOutKind() == IR::BailOutExpectingString)
            {LOGMEIN("GlobOpt.cpp] 9544\n");
                throw Js::RejitException(RejitReason::DisableSwitchOptExpectingString);
            }
            Assert(instr->GetBailOutKind() == IR::BailOutExpectingInteger);
            instr->ClearBailOutInfo();
        }
        value = intConstantValue;
        if(isUsingOriginalSrc1Value)
        {LOGMEIN("GlobOpt.cpp] 9552\n");
            doSetDstVal = false;  // Let OptDst do it by copying src1Val
        }
        break;

    case Js::OpCode::Conv_Num:
    case Js::OpCode::LdC_A_I4:
        value = intConstantValue;
        if(isUsingOriginalSrc1Value)
        {LOGMEIN("GlobOpt.cpp] 9561\n");
            doSetDstVal = false;  // Let OptDst do it by copying src1Val
        }
        break;

    case Js::OpCode::Incr_A:
        if (Int32Math::Inc(intConstantValue, &value))
        {LOGMEIN("GlobOpt.cpp] 9568\n");
            return false;
        }
        break;

    case Js::OpCode::Decr_A:
        if (Int32Math::Dec(intConstantValue, &value))
        {LOGMEIN("GlobOpt.cpp] 9575\n");
            return false;
        }
        break;

    case Js::OpCode::InlineMathAcos:
        fValue = Js::Math::Acos((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathAsin:
        fValue = Js::Math::Asin((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathAtan:
        fValue = Js::Math::Atan((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathCos:
        fValue = Js::Math::Cos((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathExp:
        fValue = Js::Math::Exp((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathLog:
        fValue = Js::Math::Log((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathSin:
        fValue = Js::Math::Sin((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathSqrt:
        fValue = ::sqrt((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathTan:
        fValue = ::tan((double)intConstantValue);
        isInt = false;
        break;

    case Js::OpCode::InlineMathFround:
        fValue = (double) (float) intConstantValue;
        isInt = false;
        break;

    case Js::OpCode::InlineMathAbs:
        if (intConstantValue == INT32_MIN)
        {LOGMEIN("GlobOpt.cpp] 9632\n");
            if (instr->GetDst()->IsInt32())
            {LOGMEIN("GlobOpt.cpp] 9634\n");
                // if dst is an int (e.g. in asm.js), we should coerce it, not convert to float
                value = static_cast<int32>(2147483648U);
            }
            else
            {
                // Rejit with AggressiveIntTypeSpecDisabled for Math.abs(INT32_MIN) because it causes dst
                // to be float type which could be different with previous type spec result in LoopPrePass
                throw Js::RejitException(RejitReason::AggressiveIntTypeSpecDisabled);
            }
        }
        else
        {
            value = ::abs(intConstantValue);
        }
        break;

    case Js::OpCode::InlineMathClz:
        DWORD clz;
        if (_BitScanReverse(&clz, intConstantValue))
        {LOGMEIN("GlobOpt.cpp] 9654\n");
            value = 31 - clz;
        }
        else
        {
            value = 32;
        }
        instr->ClearBailOutInfo();
        break;

    case Js::OpCode::Ctz:
        Assert(func->GetJITFunctionBody()->IsWasmFunction());
        Assert(!instr->HasBailOutInfo());
        DWORD ctz;
        if (_BitScanForward(&ctz, intConstantValue))
        {LOGMEIN("GlobOpt.cpp] 9669\n");
            value = ctz;
        }
        else
        {
            value = 32;
        }
        break;

    case Js::OpCode::InlineMathFloor:
        value = intConstantValue;
        instr->ClearBailOutInfo();
        break;

    case Js::OpCode::InlineMathCeil:
        value = intConstantValue;
        instr->ClearBailOutInfo();
        break;

    case Js::OpCode::InlineMathRound:
        value = intConstantValue;
        instr->ClearBailOutInfo();
        break;
    case Js::OpCode::ToVar:
        if (Js::TaggedInt::IsOverflow(intConstantValue))
        {LOGMEIN("GlobOpt.cpp] 9694\n");
            return false;
        }
        else
        {
            value = intConstantValue;
            instr->ClearBailOutInfo();
            break;
        }
    default:
        return false;
    }

    this->CaptureByteCodeSymUses(instr);

    Assert(!instr->HasBailOutInfo()); // If we are, in fact, successful in constant folding the instruction, there is no point in having the bailoutinfo around anymore.
                                      // Make sure that it is cleared if it was initially present.
    if (!isInt)
    {LOGMEIN("GlobOpt.cpp] 9712\n");
        value = (int32)fValue;
        if (fValue == (double)value)
        {LOGMEIN("GlobOpt.cpp] 9715\n");
            isInt = true;
        }
    }
    if (isInt)
    {LOGMEIN("GlobOpt.cpp] 9720\n");
        constOpnd = IR::IntConstOpnd::New(value, TyInt32, instr->m_func);
        GOPT_TRACE(_u("Constant folding to %d\n"), value);
    }
    else
    {
        constOpnd = IR::FloatConstOpnd::New(fValue, TyFloat64, instr->m_func);
        GOPT_TRACE(_u("Constant folding to %f\n"), fValue);
    }
    instr->ReplaceSrc1(constOpnd);

    this->OptSrc(constOpnd, &instr);

    IR::Opnd *dst = instr->GetDst();
    Assert(dst->IsRegOpnd());

    StackSym *dstSym = dst->AsRegOpnd()->m_sym;

    if (isInt)
    {LOGMEIN("GlobOpt.cpp] 9739\n");
        if (dstSym->IsSingleDef())
        {LOGMEIN("GlobOpt.cpp] 9741\n");
            dstSym->SetIsIntConst(value);
        }

        if (doSetDstVal)
        {LOGMEIN("GlobOpt.cpp] 9746\n");
            *pDstVal = GetIntConstantValue(value, instr, dst);
        }

        if (IsTypeSpecPhaseOff(this->func))
        {LOGMEIN("GlobOpt.cpp] 9751\n");
            instr->m_opcode = Js::OpCode::LdC_A_I4;
            this->ToVarRegOpnd(dst->AsRegOpnd(), this->currentBlock);
        }
        else
        {
            instr->m_opcode = Js::OpCode::Ld_I4;
            this->ToInt32Dst(instr, dst->AsRegOpnd(), this->currentBlock);

            StackSym * currDstSym = instr->GetDst()->AsRegOpnd()->m_sym;
            if (currDstSym->IsSingleDef())
            {LOGMEIN("GlobOpt.cpp] 9762\n");
                currDstSym->SetIsIntConst(value);
            }
        }
    }
    else
    {
        *pDstVal = NewFloatConstantValue(fValue, dst);

        if (IsTypeSpecPhaseOff(this->func))
        {LOGMEIN("GlobOpt.cpp] 9772\n");
            instr->m_opcode = Js::OpCode::LdC_A_R8;
            this->ToVarRegOpnd(dst->AsRegOpnd(), this->currentBlock);
        }
        else
        {
            instr->m_opcode = Js::OpCode::LdC_F8_R8;
            this->ToFloat64Dst(instr, dst->AsRegOpnd(), this->currentBlock);
        }
    }
    return true;
}

//------------------------------------------------------------------------------------------------------
// Type specialization
//------------------------------------------------------------------------------------------------------
bool
GlobOpt::IsWorthSpecializingToInt32DueToSrc(IR::Opnd *const src, Value *const val)
{LOGMEIN("GlobOpt.cpp] 9790\n");
    Assert(src);
    Assert(val);
    ValueInfo *valueInfo = val->GetValueInfo();
    Assert(valueInfo->IsLikelyInt());

    // If it is not known that the operand is definitely an int, the operand is not already type-specialized, and it's not live
    // in the loop landing pad (if we're in a loop), it's probably not worth type-specializing this instruction. The common case
    // where type-specializing this would be bad is where the operations are entirely on properties or array elements, where the
    // ratio of FromVars and ToVars to the number of actual operations is high, and the conversions would dominate the time
    // spent. On the other hand, if we're using a function formal parameter more than once, it would probably be worth
    // type-specializing it, hence the IsDead check on the operands.
    return
        valueInfo->IsInt() ||
        valueInfo->HasIntConstantValue(true) ||
        !src->GetIsDead() ||
        !src->IsRegOpnd() ||
        this->IsInt32TypeSpecialized(src->AsRegOpnd()->m_sym, this->currentBlock) ||
        (this->currentBlock->loop && this->IsLive(src->AsRegOpnd()->m_sym, this->currentBlock->loop->landingPad));
}

bool
GlobOpt::IsWorthSpecializingToInt32DueToDst(IR::Opnd *const dst)
{LOGMEIN("GlobOpt.cpp] 9813\n");
    Assert(dst);

    const auto sym = dst->AsRegOpnd()->m_sym;
    return
        this->IsInt32TypeSpecialized(sym, this->currentBlock) ||
        (this->currentBlock->loop && this->IsLive(sym, this->currentBlock->loop->landingPad));
}

bool
GlobOpt::IsWorthSpecializingToInt32(IR::Instr *const instr, Value *const src1Val, Value *const src2Val)
{LOGMEIN("GlobOpt.cpp] 9824\n");
    Assert(instr);

    const auto src1 = instr->GetSrc1();
    const auto src2 = instr->GetSrc2();

    // In addition to checking each operand and the destination, if for any reason we only have to do a maximum of two
    // conversions instead of the worst-case 3 conversions, it's probably worth specializing.
    if (IsWorthSpecializingToInt32DueToSrc(src1, src1Val) ||
        (src2Val && IsWorthSpecializingToInt32DueToSrc(src2, src2Val)))
    {LOGMEIN("GlobOpt.cpp] 9834\n");
        return true;
    }

    IR::Opnd *dst = instr->GetDst();
    if (!dst || IsWorthSpecializingToInt32DueToDst(dst))
    {LOGMEIN("GlobOpt.cpp] 9840\n");
        return true;
    }

    if (dst->IsEqual(src1) || (src2Val && (dst->IsEqual(src2) || src1->IsEqual(src2))))
    {LOGMEIN("GlobOpt.cpp] 9845\n");
        return true;
    }

    IR::Instr *instrNext = instr->GetNextRealInstrOrLabel();

    // Skip useless Ld_A's
    do
    {LOGMEIN("GlobOpt.cpp] 9853\n");
        switch (instrNext->m_opcode)
        {LOGMEIN("GlobOpt.cpp] 9855\n");
        case Js::OpCode::Ld_A:
            if (!dst->IsEqual(instrNext->GetSrc1()))
            {LOGMEIN("GlobOpt.cpp] 9858\n");
                goto done;
            }
            dst = instrNext->GetDst();
            break;
        case Js::OpCode::LdFld:
        case Js::OpCode::LdRootFld:
        case Js::OpCode::LdRootFldForTypeOf:
        case Js::OpCode::LdFldForTypeOf:
        case Js::OpCode::LdElemI_A:
        case Js::OpCode::ByteCodeUses:
            break;
        default:
            goto done;
        }

        instrNext = instrNext->GetNextRealInstrOrLabel();
    } while (true);
done:

    // If the next instr could also be type specialized, then it is probably worth it.
    if ((instrNext->GetSrc1() && dst->IsEqual(instrNext->GetSrc1())) || (instrNext->GetSrc2() && dst->IsEqual(instrNext->GetSrc2())))
    {LOGMEIN("GlobOpt.cpp] 9880\n");
        switch (instrNext->m_opcode)
        {LOGMEIN("GlobOpt.cpp] 9882\n");
        case Js::OpCode::Add_A:
        case Js::OpCode::Sub_A:
        case Js::OpCode::Mul_A:
        case Js::OpCode::Div_A:
        case Js::OpCode::Rem_A:
        case Js::OpCode::Xor_A:
        case Js::OpCode::And_A:
        case Js::OpCode::Or_A:
        case Js::OpCode::Shl_A:
        case Js::OpCode::Shr_A:
        case Js::OpCode::Incr_A:
        case Js::OpCode::Decr_A:
        case Js::OpCode::Neg_A:
        case Js::OpCode::Not_A:
        case Js::OpCode::Conv_Num:
        case Js::OpCode::BrEq_I4:
        case Js::OpCode::BrTrue_I4:
        case Js::OpCode::BrFalse_I4:
        case Js::OpCode::BrGe_I4:
        case Js::OpCode::BrGt_I4:
        case Js::OpCode::BrLt_I4:
        case Js::OpCode::BrLe_I4:
        case Js::OpCode::BrNeq_I4:
            return true;
        }
    }

    return false;
}

bool
GlobOpt::TypeSpecializeNumberUnary(IR::Instr *instr, Value *src1Val, Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 9915\n");
    Assert(src1Val->GetValueInfo()->IsNumber());

    if (this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 9919\n");
        return false;
    }

    switch (instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 9924\n");
    case Js::OpCode::Conv_Num:
        // Optimize Conv_Num away since we know this is a number
        instr->m_opcode = Js::OpCode::Ld_A;
        return false;
    }

    return false;
}

bool
GlobOpt::TypeSpecializeUnary(
    IR::Instr **pInstr,
    Value **pSrc1Val,
    Value **pDstVal,
    Value *const src1OriginalVal,
    bool *redoTypeSpecRef,
    bool *const forceInvariantHoistingRef)
{LOGMEIN("GlobOpt.cpp] 9942\n");
    Assert(pSrc1Val);
    Value *&src1Val = *pSrc1Val;
    Assert(src1Val);

    // We don't need to do typespec for asmjs
    if (IsTypeSpecPhaseOff(this->func) || GetIsAsmJSFunc())
    {LOGMEIN("GlobOpt.cpp] 9949\n");
        return false;
    }

    IR::Instr *&instr = *pInstr;
    int32 min, max;

    // Inline built-ins explicitly specify how srcs/dst must be specialized.
    if (OpCodeAttr::IsInlineBuiltIn(instr->m_opcode))
    {
        TypeSpecializeInlineBuiltInUnary(pInstr, &src1Val, pDstVal, src1OriginalVal, redoTypeSpecRef);
        return true;
    }

    // Consider: If type spec wasn't completely done, make sure that we don't type-spec the dst 2nd time.
    if(instr->m_opcode == Js::OpCode::LdLen_A && TypeSpecializeLdLen(&instr, &src1Val, pDstVal, forceInvariantHoistingRef))
    {LOGMEIN("GlobOpt.cpp] 9965\n");
        return true;
    }

    if (!src1Val->GetValueInfo()->GetIntValMinMax(&min, &max, this->DoAggressiveIntTypeSpec()))
    {LOGMEIN("GlobOpt.cpp] 9970\n");
        src1Val = src1OriginalVal;
        if (src1Val->GetValueInfo()->IsLikelyFloat())
        {LOGMEIN("GlobOpt.cpp] 9973\n");
            // Try to type specialize to float
            return this->TypeSpecializeFloatUnary(pInstr, src1Val, pDstVal);
        }
        else if (src1Val->GetValueInfo()->IsNumber())
        {LOGMEIN("GlobOpt.cpp] 9978\n");
            return TypeSpecializeNumberUnary(instr, src1Val, pDstVal);
        }
        return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
    }

    return this->TypeSpecializeIntUnary(pInstr, &src1Val, pDstVal, min, max, src1OriginalVal, redoTypeSpecRef);
}

// Returns true if the built-in requested type specialization, and no further action needed,
// otherwise returns false.
void
GlobOpt::TypeSpecializeInlineBuiltInUnary(IR::Instr **pInstr, Value **pSrc1Val, Value **pDstVal, Value *const src1OriginalVal, bool *redoTypeSpecRef)
{LOGMEIN("GlobOpt.cpp] 9991\n");
    IR::Instr *&instr = *pInstr;

    Assert(pSrc1Val);
    Value *&src1Val = *pSrc1Val;

    Assert(OpCodeAttr::IsInlineBuiltIn(instr->m_opcode));

    Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInInlineCandidateId(instr->m_opcode);   // From actual instr, not profile based.
    Assert(builtInId != Js::BuiltinFunction::None);

    // Consider using different bailout for float/int FromVars, so that when the arg cannot be converted to number we don't disable
    //       type spec for other parts of the big function but rather just don't inline that built-in instr.
    //       E.g. could do that if the value is not likelyInt/likelyFloat.

    Js::BuiltInFlags builtInFlags = Js::JavascriptLibrary::GetFlagsForBuiltIn(builtInId);
    bool areAllArgsAlwaysFloat = (builtInFlags & Js::BuiltInFlags::BIF_Args) == Js::BuiltInFlags::BIF_TypeSpecUnaryToFloat;
    if (areAllArgsAlwaysFloat)
    {LOGMEIN("GlobOpt.cpp] 10009\n");
        // InlineMathAcos, InlineMathAsin, InlineMathAtan, InlineMathCos, InlineMathExp, InlineMathLog, InlineMathSin, InlineMathSqrt, InlineMathTan.
        Assert(this->DoFloatTypeSpec());

        // Type-spec the src.
        src1Val = src1OriginalVal;
        bool retVal = this->TypeSpecializeFloatUnary(pInstr, src1Val, pDstVal, /* skipDst = */ true);
        AssertMsg(retVal, "For inline built-ins the args have to be type-specialized to float, but something failed during the process.");

        // Type-spec the dst.
        this->TypeSpecializeFloatDst(instr, nullptr, src1Val, nullptr, pDstVal);
    }
    else if (instr->m_opcode == Js::OpCode::InlineMathAbs)
    {LOGMEIN("GlobOpt.cpp] 10022\n");
        // Consider the case when the value is unknown - because of bailout in abs we may disable type spec for the whole function which is too much.
        // First, try int.
        int minVal, maxVal;
        bool shouldTypeSpecToInt = src1Val->GetValueInfo()->GetIntValMinMax(&minVal, &maxVal, /* doAggressiveIntTypeSpec = */ true);
        if (shouldTypeSpecToInt)
        {LOGMEIN("GlobOpt.cpp] 10028\n");
            Assert(this->DoAggressiveIntTypeSpec());
            bool retVal = this->TypeSpecializeIntUnary(pInstr, &src1Val, pDstVal, minVal, maxVal, src1OriginalVal, redoTypeSpecRef, true);
            AssertMsg(retVal, "For inline built-ins the args have to be type-specialized (int), but something failed during the process.");

            if (!this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 10034\n");
                // Create bailout for INT_MIN which does not have corresponding int value on the positive side.
                // Check int range: if we know the range is out of overflow, we do not need the bail out at all.
                if (minVal == INT32_MIN)
                {
                    GenerateBailAtOperation(&instr, IR::BailOnIntMin);
                }
            }

            // Account for ::abs(INT_MIN) == INT_MIN (which is less than 0).
            maxVal = ::max(
                ::abs(Int32Math::NearestInRangeTo(minVal, INT_MIN + 1, INT_MAX)),
                ::abs(Int32Math::NearestInRangeTo(maxVal, INT_MIN + 1, INT_MAX)));
            minVal = minVal >= 0 ? minVal : 0;
            this->TypeSpecializeIntDst(instr, instr->m_opcode, nullptr, src1Val, nullptr, IR::BailOutInvalid, minVal, maxVal, pDstVal);
        }
        else
        {
            // If we couldn't do int, do float.
            Assert(this->DoFloatTypeSpec());
            src1Val = src1OriginalVal;
            bool retVal = this->TypeSpecializeFloatUnary(pInstr, src1Val, pDstVal, true);
            AssertMsg(retVal, "For inline built-ins the args have to be type-specialized (float), but something failed during the process.");

            this->TypeSpecializeFloatDst(instr, nullptr, src1Val, nullptr, pDstVal);
        }
    }
    else if (instr->m_opcode == Js::OpCode::InlineMathFloor || instr->m_opcode == Js::OpCode::InlineMathCeil || instr->m_opcode == Js::OpCode::InlineMathRound)
    {LOGMEIN("GlobOpt.cpp] 10062\n");
        // Type specialize src to float
        src1Val = src1OriginalVal;
        bool retVal = this->TypeSpecializeFloatUnary(pInstr, src1Val, pDstVal, /* skipDst = */ true);
        AssertMsg(retVal, "For inline Math.floor and Math.ceil the src has to be type-specialized to float, but something failed during the process.");

        // Type specialize dst to int
        this->TypeSpecializeIntDst(
            instr,
            instr->m_opcode,
            nullptr,
            src1Val,
            nullptr,
            IR::BailOutInvalid,
            INT32_MIN,
            INT32_MAX,
            pDstVal);
    }
    else if(instr->m_opcode == Js::OpCode::InlineArrayPop)
    {LOGMEIN("GlobOpt.cpp] 10081\n");
        IR::Opnd *const thisOpnd = instr->GetSrc1();

        Assert(thisOpnd);

        // Ensure src1 (Array) is a var
        this->ToVarUses(instr, thisOpnd, false, src1Val);

        if(!this->IsLoopPrePass() && thisOpnd->GetValueType().IsLikelyNativeArray())
        {
            // We bail out, if there is illegal access or a mismatch in the Native array type that is optimized for, during the run time.
            GenerateBailAtOperation(&instr, IR::BailOutConventionalNativeArrayAccessOnly);
        }

        if(!instr->GetDst())
        {LOGMEIN("GlobOpt.cpp] 10096\n");
            return;
        }

        // Try Type Specializing the element (return item from Pop) based on the array's profile data.
        if(thisOpnd->GetValueType().IsLikelyNativeIntArray())
        {LOGMEIN("GlobOpt.cpp] 10102\n");
            this->TypeSpecializeIntDst(instr, instr->m_opcode, nullptr, nullptr, nullptr, IR::BailOutInvalid, INT32_MIN, INT32_MAX, pDstVal);
        }
        else if(thisOpnd->GetValueType().IsLikelyNativeFloatArray())
        {LOGMEIN("GlobOpt.cpp] 10106\n");
            this->TypeSpecializeFloatDst(instr, nullptr, nullptr, nullptr, pDstVal);
        }
        else
        {
            // We reached here so the Element is not yet type specialized. Ensure element is a var
            if(instr->GetDst()->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 10113\n");
                this->ToVarRegOpnd(instr->GetDst()->AsRegOpnd(), currentBlock);
            }
        }
    }
    else if (instr->m_opcode == Js::OpCode::InlineMathClz)
    {LOGMEIN("GlobOpt.cpp] 10119\n");
        Assert(this->DoAggressiveIntTypeSpec());
        Assert(this->DoLossyIntTypeSpec());
        //Type specialize to int
        bool retVal = this->TypeSpecializeIntUnary(pInstr, &src1Val, pDstVal, INT32_MIN, INT32_MAX, src1OriginalVal, redoTypeSpecRef);
        AssertMsg(retVal, "For clz32, the arg has to be type-specialized to int.");
    }
    else
    {
        AssertMsg(FALSE, "Unsupported built-in!");
    }
}

void
GlobOpt::TypeSpecializeInlineBuiltInBinary(IR::Instr **pInstr, Value *src1Val, Value* src2Val, Value **pDstVal, Value *const src1OriginalVal, Value *const src2OriginalVal)
{LOGMEIN("GlobOpt.cpp] 10134\n");
    IR::Instr *&instr = *pInstr;
    Assert(OpCodeAttr::IsInlineBuiltIn(instr->m_opcode));

    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 10139\n");
        case Js::OpCode::InlineMathAtan2:
        {LOGMEIN("GlobOpt.cpp] 10141\n");
            Js::BuiltinFunction builtInId = Js::JavascriptLibrary::GetBuiltInInlineCandidateId(instr->m_opcode);   // From actual instr, not profile based.
            Js::BuiltInFlags builtInFlags = Js::JavascriptLibrary::GetFlagsForBuiltIn(builtInId);

            bool areAllArgsAlwaysFloat = (builtInFlags & Js::BuiltInFlags::BIF_TypeSpecAllToFloat) != 0;
            Assert(areAllArgsAlwaysFloat);
            Assert(this->DoFloatTypeSpec());

            // Type-spec the src1, src2 and dst.
            src1Val = src1OriginalVal;
            src2Val = src2OriginalVal;
            bool retVal = this->TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal);
            AssertMsg(retVal, "For pow and atnan2 the args have to be type-specialized to float, but something failed during the process.");

            break;
        }

        case Js::OpCode::InlineMathPow:
        {LOGMEIN("GlobOpt.cpp] 10159\n");
#ifndef _M_ARM
            if (src2Val->GetValueInfo()->IsLikelyInt())
            {LOGMEIN("GlobOpt.cpp] 10162\n");
                bool lossy = false;

                this->ToInt32(instr, instr->GetSrc2(), this->currentBlock, src2Val, nullptr, lossy);

                IR::Opnd* src1 = instr->GetSrc1();
                int32 valueMin, valueMax;
                if (src1Val->GetValueInfo()->IsLikelyInt() &&
                    this->DoPowIntIntTypeSpec() &&
                    src2Val->GetValueInfo()->GetIntValMinMax(&valueMin, &valueMax, this->DoAggressiveIntTypeSpec()) &&
                    valueMin >= 0)

                {LOGMEIN("GlobOpt.cpp] 10174\n");
                    this->ToInt32(instr, src1, this->currentBlock, src1Val, nullptr, lossy);
                    this->TypeSpecializeIntDst(instr, instr->m_opcode, nullptr, src1Val, src2Val, IR::BailOutInvalid, INT32_MIN, INT32_MAX, pDstVal);

                    if(!this->IsLoopPrePass())
                    {
                        GenerateBailAtOperation(&instr, IR::BailOutOnPowIntIntOverflow);
                    }
                }
                else
                {
                    this->ToFloat64(instr, src1, this->currentBlock, src1Val, nullptr, IR::BailOutPrimitiveButString);
                    TypeSpecializeFloatDst(instr, nullptr, src1Val, src2Val, pDstVal);
                }
            }
            else
            {
#endif
                this->TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal);
#ifndef _M_ARM
            }
#endif
            break;
        }

        case Js::OpCode::InlineMathImul:
        {LOGMEIN("GlobOpt.cpp] 10200\n");
            Assert(this->DoAggressiveIntTypeSpec());
            Assert(this->DoLossyIntTypeSpec());

            //Type specialize to int
            bool retVal = this->TypeSpecializeIntBinary(pInstr, src1Val, src2Val, pDstVal, INT32_MIN, INT32_MAX, false /* skipDst */);

            AssertMsg(retVal, "For imul, the args have to be type-specialized to int but something failed during the process.");
            break;
        }

        case Js::OpCode::InlineMathMin:
        case Js::OpCode::InlineMathMax:
        {LOGMEIN("GlobOpt.cpp] 10213\n");
            if(src1Val->GetValueInfo()->IsLikelyInt() && src2Val->GetValueInfo()->IsLikelyInt())
            {LOGMEIN("GlobOpt.cpp] 10215\n");
                // Compute resulting range info
                int32 min1, max1, min2, max2, newMin, newMax;

                Assert(this->DoAggressiveIntTypeSpec());
                src1Val->GetValueInfo()->GetIntValMinMax(&min1, &max1, this->DoAggressiveIntTypeSpec());
                src2Val->GetValueInfo()->GetIntValMinMax(&min2, &max2, this->DoAggressiveIntTypeSpec());
                if (instr->m_opcode == Js::OpCode::InlineMathMin)
                {LOGMEIN("GlobOpt.cpp] 10223\n");
                    newMin = min(min1, min2);
                    newMax = min(max1, max2);
                }
                else
                {
                    Assert(instr->m_opcode == Js::OpCode::InlineMathMax);
                    newMin = max(min1, min2);
                    newMax = max(max1, max2);
                }
                // Type specialize to int
                bool retVal = this->TypeSpecializeIntBinary(pInstr, src1Val, src2Val, pDstVal, newMin, newMax, false /* skipDst */);
                AssertMsg(retVal, "For min and max, the args have to be type-specialized to int if any one of the sources is an int, but something failed during the process.");
            }

            // Couldn't type specialize to int, type specialize to float
            else
            {
                Assert(this->DoFloatTypeSpec());
                src1Val = src1OriginalVal;
                src2Val = src2OriginalVal;
                bool retVal = this->TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal);
                AssertMsg(retVal, "For min and max, the args have to be type-specialized to float if any one of the sources is a float, but something failed during the process.");
            }
            break;
        }
        case Js::OpCode::InlineArrayPush:
        {LOGMEIN("GlobOpt.cpp] 10250\n");
            IR::Opnd *const thisOpnd = instr->GetSrc1();

            Assert(thisOpnd);

            if(instr->GetDst() && instr->GetDst()->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 10256\n");
                // Set the dst as live here, as the built-ins return early from the TypeSpecialization functions - before the dst is marked as live.
                // Also, we are not specializing the dst separately and we are skipping the dst to be handled when we specialize the instruction above.
                this->ToVarRegOpnd(instr->GetDst()->AsRegOpnd(), currentBlock);
            }

            // Ensure src1 (Array) is a var
            this->ToVarUses(instr, thisOpnd, false, src1Val);

            if(!this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 10266\n");
                if(thisOpnd->GetValueType().IsLikelyNativeArray())
                {
                    // We bail out, if there is illegal access or a mismatch in the Native array type that is optimized for, during run time.
                    GenerateBailAtOperation(&instr, IR::BailOutConventionalNativeArrayAccessOnly);
                }
                else
                {
                    GenerateBailAtOperation(&instr, IR::BailOutOnImplicitCallsPreOp);
                }
            }

            // Try Type Specializing the element based on the array's profile data.
            if(thisOpnd->GetValueType().IsLikelyNativeFloatArray())
            {LOGMEIN("GlobOpt.cpp] 10280\n");
                src1Val = src1OriginalVal;
                src2Val = src2OriginalVal;
            }
            if((thisOpnd->GetValueType().IsLikelyNativeIntArray() && this->TypeSpecializeIntBinary(pInstr, src1Val, src2Val, pDstVal, INT32_MIN, INT32_MAX, true))
                || (thisOpnd->GetValueType().IsLikelyNativeFloatArray() && this->TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal)))
            {LOGMEIN("GlobOpt.cpp] 10286\n");
                break;
            }

            // The Element is not yet type specialized. Ensure element is a var
            this->ToVarUses(instr, instr->GetSrc2(), false, src2Val);
            break;
        }
    }
}

void
GlobOpt::TypeSpecializeInlineBuiltInDst(IR::Instr **pInstr, Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 10299\n");
    IR::Instr *&instr = *pInstr;
    Assert(OpCodeAttr::IsInlineBuiltIn(instr->m_opcode));

    if (instr->m_opcode == Js::OpCode::InlineMathRandom)
    {LOGMEIN("GlobOpt.cpp] 10304\n");
        Assert(this->DoFloatTypeSpec());
        // Type specialize dst to float
        this->TypeSpecializeFloatDst(instr, nullptr, nullptr, nullptr, pDstVal);
    }
}

bool
GlobOpt::TryTypeSpecializeUnaryToFloatHelper(IR::Instr** pInstr, Value** pSrc1Val, Value* const src1OriginalVal, Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 10313\n");
    // It has been determined that this instruction cannot be int-specialized. We need to determine whether to attempt to
    // float-specialize the instruction, or leave it unspecialized.
#if !INT32VAR
    Value*& src1Val = *pSrc1Val;
    if(src1Val->GetValueInfo()->IsLikelyUntaggedInt())
    {LOGMEIN("GlobOpt.cpp] 10319\n");
        // An input range is completely outside the range of an int31. Even if the operation may overflow, it is
        // unlikely to overflow on these operations, so we leave it unspecialized on 64-bit platforms. However, on
        // 32-bit platforms, the value is untaggable and will be a JavascriptNumber, which is significantly slower to
        // use in an unspecialized operation compared to a tagged int. So, try to float-specialize the instruction.
        src1Val = src1OriginalVal;
        return this->TypeSpecializeFloatUnary(pInstr, src1Val, pDstVal);
    }
#endif
    return false;
}

bool
GlobOpt::TypeSpecializeIntBinary(IR::Instr **pInstr, Value *src1Val, Value *src2Val, Value **pDstVal, int32 min, int32 max, bool skipDst /* = false */)
{LOGMEIN("GlobOpt.cpp] 10333\n");
    // Consider moving the code for int type spec-ing binary functions here.
    IR::Instr *&instr = *pInstr;
    bool lossy = false;

    if(OpCodeAttr::IsInlineBuiltIn(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 10339\n");
        if(instr->m_opcode == Js::OpCode::InlineArrayPush)
        {LOGMEIN("GlobOpt.cpp] 10341\n");
            int32 intConstantValue;
            bool isIntConstMissingItem = src2Val->GetValueInfo()->TryGetIntConstantValue(&intConstantValue);

            if(isIntConstMissingItem)
            {LOGMEIN("GlobOpt.cpp] 10346\n");
                isIntConstMissingItem = Js::SparseArraySegment<int>::IsMissingItem(&intConstantValue);
            }

            // Don't specialize if the element is not likelyInt or an IntConst which is a missing item value.
            if(!(src2Val->GetValueInfo()->IsLikelyInt()) || isIntConstMissingItem)
            {LOGMEIN("GlobOpt.cpp] 10352\n");
                return false;
            }
            // We don't want to specialize both the source operands, though it is a binary instr.
            IR::Opnd * elementOpnd = instr->GetSrc2();
            this->ToInt32(instr, elementOpnd, this->currentBlock, src2Val, nullptr, lossy);
        }
        else
        {
            IR::Opnd *src1 = instr->GetSrc1();
            this->ToInt32(instr, src1, this->currentBlock, src1Val, nullptr, lossy);

            IR::Opnd *src2 = instr->GetSrc2();
            this->ToInt32(instr, src2, this->currentBlock, src2Val, nullptr, lossy);
        }

        if(!skipDst)
        {LOGMEIN("GlobOpt.cpp] 10369\n");
            IR::Opnd *dst = instr->GetDst();
            if (dst)
            {
                TypeSpecializeIntDst(instr, instr->m_opcode, nullptr, src1Val, src2Val, IR::BailOutInvalid, min, max, pDstVal);
            }
        }
        return true;
    }
    else
    {
        AssertMsg(false, "Yet to move code for other binary functions here");
        return false;
    }
}

bool
GlobOpt::TypeSpecializeIntUnary(
    IR::Instr **pInstr,
    Value **pSrc1Val,
    Value **pDstVal,
    int32 min,
    int32 max,
    Value *const src1OriginalVal,
    bool *redoTypeSpecRef,
    bool skipDst /* = false */)
{LOGMEIN("GlobOpt.cpp] 10395\n");
    IR::Instr *&instr = *pInstr;

    Assert(pSrc1Val);
    Value *&src1Val = *pSrc1Val;

    bool isTransfer = false;
    Js::OpCode opcode;
    int32 newMin, newMax;
    bool lossy = false;
    IR::BailOutKind bailOutKind = IR::BailOutInvalid;
    bool ignoredIntOverflow = this->ignoredIntOverflowForCurrentInstr;
    bool ignoredNegativeZero = false;
    bool checkTypeSpecWorth = false;

    if(instr->GetSrc1()->IsRegOpnd() && instr->GetSrc1()->AsRegOpnd()->m_sym->m_isNotInt)
    {LOGMEIN("GlobOpt.cpp] 10411\n");
        return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
    }

    AddSubConstantInfo addSubConstantInfo;

    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 10418\n");
    case Js::OpCode::Ld_A:
        if (instr->GetSrc1()->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 10421\n");
            StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
            if (this->IsInt32TypeSpecialized(sym, this->currentBlock) == false)
            {LOGMEIN("GlobOpt.cpp] 10424\n");
                // Type specializing an Ld_A isn't worth it, unless the src
                // is already type specialized.
                return false;
            }
        }
        newMin = min;
        newMax = max;
        opcode = Js::OpCode::Ld_I4;
        isTransfer = true;
        break;

    case Js::OpCode::Conv_Num:
        newMin = min;
        newMax = max;
        opcode = Js::OpCode::Ld_I4;
        isTransfer = true;
        break;

    case Js::OpCode::LdC_A_I4:
        newMin = newMax = instr->GetSrc1()->AsIntConstOpnd()->AsInt32();
        opcode = Js::OpCode::Ld_I4;
        break;

    case Js::OpCode::Neg_A:
        if (min <= 0 && max >= 0)
        {LOGMEIN("GlobOpt.cpp] 10450\n");
            if(instr->ShouldCheckForNegativeZero())
            {LOGMEIN("GlobOpt.cpp] 10452\n");
                // -0 matters since the sym is not a local, or is used in a way in which -0 would differ from +0
                if(!DoAggressiveIntTypeSpec())
                {LOGMEIN("GlobOpt.cpp] 10455\n");
                    // May result in -0
                    // Consider adding a dynamic check for src1 == 0
                    return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                }
                if(min == 0 && max == 0)
                {LOGMEIN("GlobOpt.cpp] 10461\n");
                    // Always results in -0
                    return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                }
                bailOutKind |= IR::BailOutOnNegativeZero;
            }
            else
            {
                ignoredNegativeZero = true;
            }
        }
        if (Int32Math::Neg(min, &newMax))
        {LOGMEIN("GlobOpt.cpp] 10473\n");
            if(instr->ShouldCheckForIntOverflow())
            {LOGMEIN("GlobOpt.cpp] 10475\n");
                if(!DoAggressiveIntTypeSpec())
                {LOGMEIN("GlobOpt.cpp] 10477\n");
                    // May overflow
                    return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                }
                if(min == max)
                {LOGMEIN("GlobOpt.cpp] 10482\n");
                    // Always overflows
                    return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                }
                bailOutKind |= IR::BailOutOnOverflow;
                newMax = INT32_MAX;
            }
            else
            {
                ignoredIntOverflow = true;
            }
        }
        if (Int32Math::Neg(max, &newMin))
        {LOGMEIN("GlobOpt.cpp] 10495\n");
            if(instr->ShouldCheckForIntOverflow())
            {LOGMEIN("GlobOpt.cpp] 10497\n");
                if(!DoAggressiveIntTypeSpec())
                {LOGMEIN("GlobOpt.cpp] 10499\n");
                    // May overflow
                    return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                }
                bailOutKind |= IR::BailOutOnOverflow;
                newMin = INT32_MAX;
            }
            else
            {
                ignoredIntOverflow = true;
            }
        }
        if(!instr->ShouldCheckForIntOverflow() && newMin > newMax)
        {LOGMEIN("GlobOpt.cpp] 10512\n");
            // When ignoring overflow, the range needs to account for overflow. Since MIN_INT is the only int32 value that
            // overflows on Neg, and the value resulting from overflow is also MIN_INT, if calculating only the new min or new
            // max overflowed but not both, then the new min will be greater than the new max. In that case we need to consider
            // the full range of int32s as possible resulting values.
            newMin = INT32_MIN;
            newMax = INT32_MAX;
        }
        opcode = Js::OpCode::Neg_I4;
        checkTypeSpecWorth = true;
        break;

    case Js::OpCode::Not_A:
        if(!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 10526\n");
            return false;
        }
        this->PropagateIntRangeForNot(min, max, &newMin, &newMax);
        opcode = Js::OpCode::Not_I4;
        lossy = true;
        break;

    case Js::OpCode::Incr_A:
        do // while(false)
        {LOGMEIN("GlobOpt.cpp] 10536\n");
            const auto CannotOverflowBasedOnRelativeBounds = [&]()
            {
                const ValueInfo *const src1ValueInfo = src1Val->GetValueInfo();
                return
                    (src1ValueInfo->IsInt() || DoAggressiveIntTypeSpec()) &&
                    src1ValueInfo->IsIntBounded() &&
                    src1ValueInfo->AsIntBounded()->Bounds()->AddCannotOverflowBasedOnRelativeBounds(1);
            };

            if (Int32Math::Inc(min, &newMin))
            {LOGMEIN("GlobOpt.cpp] 10547\n");
                if(CannotOverflowBasedOnRelativeBounds())
                {LOGMEIN("GlobOpt.cpp] 10549\n");
                    newMin = INT32_MAX;
                }
                else if(instr->ShouldCheckForIntOverflow())
                {LOGMEIN("GlobOpt.cpp] 10553\n");
                    // Always overflows
                    return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                }
                else
                {
                    // When ignoring overflow, the range needs to account for overflow. For any Add or Sub, since overflow
                    // causes the value to wrap around, and we don't have a way to specify a lower and upper range of ints,
                    // we use the full range of int32s.
                    ignoredIntOverflow = true;
                    newMin = INT32_MIN;
                    newMax = INT32_MAX;
                    break;
                }
            }
            if (Int32Math::Inc(max, &newMax))
            {LOGMEIN("GlobOpt.cpp] 10569\n");
                if(CannotOverflowBasedOnRelativeBounds())
                {LOGMEIN("GlobOpt.cpp] 10571\n");
                    newMax = INT32_MAX;
                }
                else if(instr->ShouldCheckForIntOverflow())
                {LOGMEIN("GlobOpt.cpp] 10575\n");
                    if(!DoAggressiveIntTypeSpec())
                    {LOGMEIN("GlobOpt.cpp] 10577\n");
                        // May overflow
                        return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                    }
                    bailOutKind |= IR::BailOutOnOverflow;
                    newMax = INT32_MAX;
                }
                else
                {
                    // See comment about ignoring overflow above
                    ignoredIntOverflow = true;
                    newMin = INT32_MIN;
                    newMax = INT32_MAX;
                    break;
                }
            }
        } while(false);

        if(!ignoredIntOverflow && instr->GetSrc1()->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 10596\n");
            addSubConstantInfo.Set(instr->GetSrc1()->AsRegOpnd()->m_sym, src1Val, min == max, 1);
        }

        opcode = Js::OpCode::Add_I4;
        if (!this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 10602\n");
            instr->SetSrc2(IR::IntConstOpnd::New(1, TyInt32, instr->m_func));
        }
        checkTypeSpecWorth = true;
        break;

    case Js::OpCode::Decr_A:
        do // while(false)
        {LOGMEIN("GlobOpt.cpp] 10610\n");
            const auto CannotOverflowBasedOnRelativeBounds = [&]()
            {
                const ValueInfo *const src1ValueInfo = src1Val->GetValueInfo();
                return
                    (src1ValueInfo->IsInt() || DoAggressiveIntTypeSpec()) &&
                    src1ValueInfo->IsIntBounded() &&
                    src1ValueInfo->AsIntBounded()->Bounds()->SubCannotOverflowBasedOnRelativeBounds(1);
            };

            if (Int32Math::Dec(max, &newMax))
            {LOGMEIN("GlobOpt.cpp] 10621\n");
                if(CannotOverflowBasedOnRelativeBounds())
                {LOGMEIN("GlobOpt.cpp] 10623\n");
                    newMax = INT32_MIN;
                }
                else if(instr->ShouldCheckForIntOverflow())
                {LOGMEIN("GlobOpt.cpp] 10627\n");
                    // Always overflows
                    return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                }
                else
                {
                    // When ignoring overflow, the range needs to account for overflow. For any Add or Sub, since overflow
                    // causes the value to wrap around, and we don't have a way to specify a lower and upper range of ints, we
                    // use the full range of int32s.
                    ignoredIntOverflow = true;
                    newMin = INT32_MIN;
                    newMax = INT32_MAX;
                    break;
                }
            }
            if (Int32Math::Dec(min, &newMin))
            {LOGMEIN("GlobOpt.cpp] 10643\n");
                if(CannotOverflowBasedOnRelativeBounds())
                {LOGMEIN("GlobOpt.cpp] 10645\n");
                    newMin = INT32_MIN;
                }
                else if(instr->ShouldCheckForIntOverflow())
                {LOGMEIN("GlobOpt.cpp] 10649\n");
                    if(!DoAggressiveIntTypeSpec())
                    {LOGMEIN("GlobOpt.cpp] 10651\n");
                        // May overflow
                        return TryTypeSpecializeUnaryToFloatHelper(pInstr, &src1Val, src1OriginalVal, pDstVal);
                    }
                    bailOutKind |= IR::BailOutOnOverflow;
                    newMin = INT32_MIN;
                }
                else
                {
                    // See comment about ignoring overflow above
                    ignoredIntOverflow = true;
                    newMin = INT32_MIN;
                    newMax = INT32_MAX;
                    break;
                }
            }
        } while(false);

        if(!ignoredIntOverflow && instr->GetSrc1()->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 10670\n");
            addSubConstantInfo.Set(instr->GetSrc1()->AsRegOpnd()->m_sym, src1Val, min == max, -1);
        }

        opcode = Js::OpCode::Sub_I4;
        if (!this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 10676\n");
            instr->SetSrc2(IR::IntConstOpnd::New(1, TyInt32, instr->m_func));
        }
        checkTypeSpecWorth = true;
        break;

    case Js::OpCode::BrFalse_A:
    case Js::OpCode::BrTrue_A:
    {
        if(DoConstFold() && !IsLoopPrePass() && TryOptConstFoldBrFalse(instr, src1Val, min, max))
        {LOGMEIN("GlobOpt.cpp] 10686\n");
            return true;
        }

        bool specialize = true;
        if (!src1Val->GetValueInfo()->HasIntConstantValue() && instr->GetSrc1()->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 10692\n");
            StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
            if (this->IsInt32TypeSpecialized(sym, this->currentBlock) == false)
            {LOGMEIN("GlobOpt.cpp] 10695\n");
                // Type specializing a BrTrue_A/BrFalse_A isn't worth it, unless the src
                // is already type specialized
                specialize = false;
            }
        }
        if(instr->m_opcode == Js::OpCode::BrTrue_A)
        {
            UpdateIntBoundsForNotEqualBranch(src1Val, nullptr, 0);
            opcode = Js::OpCode::BrTrue_I4;
        }
        else
        {
            UpdateIntBoundsForEqualBranch(src1Val, nullptr, 0);
            opcode = Js::OpCode::BrFalse_I4;
        }
        if(!specialize)
        {LOGMEIN("GlobOpt.cpp] 10712\n");
            return false;
        }

        newMin = 2; newMax = 1;  // We'll assert if we make a range where min > max
        break;
    }

    case Js::OpCode::MultiBr:
        newMin = min;
        newMax = max;
        opcode = instr->m_opcode;
        break;

    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
    case Js::OpCode::StElemC:
        if(instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetValueType().IsLikelyAnyArrayWithNativeFloatValues())
        {LOGMEIN("GlobOpt.cpp] 10730\n");
            src1Val = src1OriginalVal;
        }
        return TypeSpecializeStElem(pInstr, src1Val, pDstVal);

    case Js::OpCode::NewScArray:
    case Js::OpCode::NewScArrayWithMissingValues:
    case Js::OpCode::InitFld:
    case Js::OpCode::InitRootFld:
    case Js::OpCode::StSlot:
    case Js::OpCode::StSlotChkUndecl:
#if !FLOATVAR
    case Js::OpCode::StSlotBoxTemp:
#endif
    case Js::OpCode::StFld:
    case Js::OpCode::StRootFld:
    case Js::OpCode::StFldStrict:
    case Js::OpCode::StRootFldStrict:
    case Js::OpCode::ArgOut_A:
    case Js::OpCode::ArgOut_A_Inline:
    case Js::OpCode::ArgOut_A_FixupForStackArgs:
    case Js::OpCode::ArgOut_A_Dynamic:
    case Js::OpCode::ArgOut_A_FromStackArgs:
    case Js::OpCode::ArgOut_A_SpreadArg:
    // For this one we need to implement type specialization
    //case Js::OpCode::ArgOut_A_InlineBuiltIn:
    case Js::OpCode::Ret:
    case Js::OpCode::LdElemUndef:
    case Js::OpCode::LdElemUndefScoped:
        return false;

    default:
        if (OpCodeAttr::IsInlineBuiltIn(instr->m_opcode))
        {LOGMEIN("GlobOpt.cpp] 10763\n");
            newMin = min;
            newMax = max;
            opcode = instr->m_opcode;
            break; // Note: we must keep checkTypeSpecWorth = false to make sure we never return false from this function.
        }
        return false;
    }

    // If this instruction is in a range of instructions where int overflow does not matter, we will still specialize it (won't
    // leave it unspecialized based on heuristics), since it is most likely worth specializing, and the dst value needs to be
    // guaranteed to be an int
    if(checkTypeSpecWorth &&
        !ignoredIntOverflow &&
        !ignoredNegativeZero &&
        instr->ShouldCheckForIntOverflow() &&
        !IsWorthSpecializingToInt32(instr, src1Val))
    {LOGMEIN("GlobOpt.cpp] 10780\n");
        // Even though type specialization is being skipped since it may not be worth it, the proper value should still be
        // maintained so that the result may be type specialized later. An int value is not created for the dst in any of
        // the following cases.
        // - A bailout check is necessary to specialize this instruction. The bailout check is what guarantees the result to be
        //   an int, but since we're not going to specialize this instruction, there won't be a bailout check.
        // - Aggressive int type specialization is disabled and we're in a loop prepass. We're conservative on dst values in
        //   that case, especially if the dst sym is live on the back-edge.
        if(bailOutKind == IR::BailOutInvalid &&
            instr->GetDst() &&
            (DoAggressiveIntTypeSpec() || !this->IsLoopPrePass()))
        {LOGMEIN("GlobOpt.cpp] 10791\n");
            *pDstVal = CreateDstUntransferredIntValue(newMin, newMax, instr, src1Val, nullptr);
        }

        if(instr->GetSrc2())
        {LOGMEIN("GlobOpt.cpp] 10796\n");
            instr->FreeSrc2();
        }
        return false;
    }

    this->ignoredIntOverflowForCurrentInstr = ignoredIntOverflow;
    this->ignoredNegativeZeroForCurrentInstr = ignoredNegativeZero;

    {
        // Try CSE again before modifying the IR, in case some attributes are required for successful CSE
        Value *src1IndirIndexVal = nullptr;
        Value *src2Val = nullptr;
        if(CSEOptimize(currentBlock, &instr, &src1Val, &src2Val, &src1IndirIndexVal, true /* intMathExprOnly */))
        {LOGMEIN("GlobOpt.cpp] 10810\n");
            *redoTypeSpecRef = true;
            return false;
        }
    }

    const Js::OpCode originalOpCode = instr->m_opcode;
    if (!this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 10818\n");
        // No re-write on prepass
        instr->m_opcode = opcode;
    }

    Value *src1ValueToSpecialize = src1Val;
    if(lossy)
    {LOGMEIN("GlobOpt.cpp] 10825\n");
        // Lossy conversions to int32 must be done based on the original source values. For instance, if one of the values is a
        // float constant with a value that fits in a uint32 but not an int32, and the instruction can ignore int overflow, the
        // source value for the purposes of int specialization would have been changed to an int constant value by ignoring
        // overflow. If we were to specialize the sym using the int constant value, it would be treated as a lossless
        // conversion, but since there may be subsequent uses of the same float constant value that may not ignore overflow,
        // this must be treated as a lossy conversion by specializing the sym using the original float constant value.
        src1ValueToSpecialize = src1OriginalVal;
    }

    // Make sure the srcs are specialized
    IR::Opnd *src1 = instr->GetSrc1();
    this->ToInt32(instr, src1, this->currentBlock, src1ValueToSpecialize, nullptr, lossy);

    if(bailOutKind != IR::BailOutInvalid && !this->IsLoopPrePass())
    {
        GenerateBailAtOperation(&instr, bailOutKind);
    }

    if (!skipDst)
    {LOGMEIN("GlobOpt.cpp] 10845\n");
        IR::Opnd *dst = instr->GetDst();
        if (dst)
        {LOGMEIN("GlobOpt.cpp] 10848\n");
            AssertMsg(!(isTransfer && !this->IsLoopPrePass()) || min == newMin && max == newMax, "If this is just a copy, old/new min/max should be the same");
            TypeSpecializeIntDst(
                instr,
                originalOpCode,
                isTransfer ? src1Val : nullptr,
                src1Val,
                nullptr,
                bailOutKind,
                newMin,
                newMax,
                pDstVal,
                addSubConstantInfo.HasInfo() ? &addSubConstantInfo : nullptr);
        }
    }

    if(bailOutKind == IR::BailOutInvalid)
    {LOGMEIN("GlobOpt.cpp] 10865\n");
        GOPT_TRACE(_u("Type specialized to INT\n"));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::AggressiveIntTypeSpecPhase))
        {LOGMEIN("GlobOpt.cpp] 10869\n");
            Output::Print(_u("Type specialized to INT: "));
            Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
        }
#endif
    }
    else
    {
        GOPT_TRACE(_u("Type specialized to INT with bailout on:\n"));
        if(bailOutKind & IR::BailOutOnOverflow)
        {LOGMEIN("GlobOpt.cpp] 10879\n");
            GOPT_TRACE(_u("    Overflow\n"));
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::AggressiveIntTypeSpecPhase))
            {LOGMEIN("GlobOpt.cpp] 10883\n");
                Output::Print(_u("Type specialized to INT with bailout (%S): "), "Overflow");
                Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
            }
#endif
        }
        if(bailOutKind & IR::BailOutOnNegativeZero)
        {LOGMEIN("GlobOpt.cpp] 10890\n");
            GOPT_TRACE(_u("    Zero\n"));
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::AggressiveIntTypeSpecPhase))
            {LOGMEIN("GlobOpt.cpp] 10894\n");
                Output::Print(_u("Type specialized to INT with bailout (%S): "), "Zero");
                Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
            }
#endif
        }
    }

    return true;
}

void
GlobOpt::TypeSpecializeIntDst(IR::Instr* instr, Js::OpCode originalOpCode, Value* valToTransfer, Value *const src1Value, Value *const src2Value, const IR::BailOutKind bailOutKind, int32 newMin, int32 newMax, Value** pDstVal, const AddSubConstantInfo *const addSubConstantInfo)
{LOGMEIN("GlobOpt.cpp] 10907\n");
    this->TypeSpecializeIntDst(instr, originalOpCode, valToTransfer, src1Value, src2Value, bailOutKind, ValueType::GetInt(IntConstantBounds(newMin, newMax).IsLikelyTaggable()), newMin, newMax, pDstVal, addSubConstantInfo);
}

void
GlobOpt::TypeSpecializeIntDst(IR::Instr* instr, Js::OpCode originalOpCode, Value* valToTransfer, Value *const src1Value, Value *const src2Value, const IR::BailOutKind bailOutKind, ValueType valueType, Value** pDstVal, const AddSubConstantInfo *const addSubConstantInfo)
{LOGMEIN("GlobOpt.cpp] 10913\n");
    this->TypeSpecializeIntDst(instr, originalOpCode, valToTransfer, src1Value, src2Value, bailOutKind, valueType, 0, 0, pDstVal, addSubConstantInfo);
}

void
GlobOpt::TypeSpecializeIntDst(IR::Instr* instr, Js::OpCode originalOpCode, Value* valToTransfer, Value *const src1Value, Value *const src2Value, const IR::BailOutKind bailOutKind, ValueType valueType, int32 newMin, int32 newMax, Value** pDstVal, const AddSubConstantInfo *const addSubConstantInfo)
{LOGMEIN("GlobOpt.cpp] 10919\n");
    Assert(valueType.IsInt() || (valueType.IsNumber() && valueType.IsLikelyInt() && newMin == 0 && newMax == 0));
    Assert(!valToTransfer || valToTransfer == src1Value);
    Assert(!addSubConstantInfo || addSubConstantInfo->HasInfo());

    IR::Opnd *dst = instr->GetDst();
    Assert(dst);

    bool isValueInfoPrecise;
    if(IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 10929\n");
        valueType = GetPrepassValueTypeForDst(valueType, instr, src1Value, src2Value, &isValueInfoPrecise);
    }
    else
    {
        isValueInfoPrecise = true;
    }

    // If dst has a circular reference in a loop, it probably won't get specialized. Don't mark the dst as type-specialized on
    // the pre-pass. With aggressive int spec though, it will take care of bailing out if necessary so there's no need to assume
    // that the dst will be a var even if it's live on the back-edge. Also if the op always produces an int32, then there's no
    // ambiguity in the dst's value type even in the prepass.
    if (!DoAggressiveIntTypeSpec() && this->IsLoopPrePass() && !valueType.IsInt())
    {LOGMEIN("GlobOpt.cpp] 10942\n");
        if (dst->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 10944\n");
            this->ToVarRegOpnd(dst->AsRegOpnd(), this->currentBlock);
        }
        return;
    }

    const IntBounds *dstBounds = nullptr;
    if(addSubConstantInfo && !addSubConstantInfo->SrcValueIsLikelyConstant() && DoTrackRelativeIntBounds())
    {LOGMEIN("GlobOpt.cpp] 10952\n");
        Assert(!ignoredIntOverflowForCurrentInstr);

        // Track bounds for add or sub with a constant. For instance, consider (b = a + 2). The value of 'b' should track that
        // it is equal to (the value of 'a') + 2. Additionally, the value of 'b' should inherit the bounds of 'a', offset by
        // the constant value.
        if(!valueType.IsInt() || !isValueInfoPrecise)
        {LOGMEIN("GlobOpt.cpp] 10959\n");
            newMin = INT32_MIN;
            newMax = INT32_MAX;
        }
        dstBounds =
            IntBounds::Add(
                addSubConstantInfo->SrcValue(),
                addSubConstantInfo->Offset(),
                isValueInfoPrecise,
                IntConstantBounds(newMin, newMax),
                alloc);
    }

    // Src1's value could change later in the loop, so the value wouldn't be the same for each
    // iteration.  Since we don't iterate over loops "while (!changed)", go conservative on the
    // pre-pass.
    if (valToTransfer)
    {LOGMEIN("GlobOpt.cpp] 10976\n");
        // If this is just a copy, no need for creating a new value.
        Assert(!addSubConstantInfo);
        *pDstVal = this->ValueNumberTransferDst(instr, valToTransfer);
        this->InsertNewValue(*pDstVal, dst);
    }
    else if (valueType.IsInt() && isValueInfoPrecise)
    {LOGMEIN("GlobOpt.cpp] 10983\n");
        bool wasNegativeZeroPreventedByBailout = false;
        if(newMin <= 0 && newMax >= 0)
        {LOGMEIN("GlobOpt.cpp] 10986\n");
            switch(originalOpCode)
            {LOGMEIN("GlobOpt.cpp] 10988\n");
                case Js::OpCode::Add_A:
                    // -0 + -0 == -0
                    Assert(src1Value);
                    Assert(src2Value);
                    wasNegativeZeroPreventedByBailout =
                        src1Value->GetValueInfo()->WasNegativeZeroPreventedByBailout() &&
                        src2Value->GetValueInfo()->WasNegativeZeroPreventedByBailout();
                    break;

                case Js::OpCode::Sub_A:
                    // -0 - 0 == -0
                    Assert(src1Value);
                    wasNegativeZeroPreventedByBailout = src1Value->GetValueInfo()->WasNegativeZeroPreventedByBailout();
                    break;

                case Js::OpCode::Neg_A:
                case Js::OpCode::Mul_A:
                case Js::OpCode::Div_A:
                case Js::OpCode::Rem_A:
                    wasNegativeZeroPreventedByBailout = !!(bailOutKind & IR::BailOutOnNegativeZero);
                    break;
            }
        }

        *pDstVal =
            dstBounds
                ? NewIntBoundedValue(valueType, dstBounds, wasNegativeZeroPreventedByBailout, nullptr)
                : NewIntRangeValue(newMin, newMax, wasNegativeZeroPreventedByBailout, nullptr);
    }
    else
    {
        *pDstVal = dstBounds ? NewIntBoundedValue(valueType, dstBounds, false, nullptr) : NewGenericValue(valueType);
    }

    if(addSubConstantInfo || updateInductionVariableValueNumber)
    {
        TrackIntSpecializedAddSubConstant(instr, addSubConstantInfo, *pDstVal, !!dstBounds);
    }

    SetValue(&blockData, *pDstVal, dst);

    AssertMsg(dst->IsRegOpnd(), "What else?");
    this->ToInt32Dst(instr, dst->AsRegOpnd(), this->currentBlock);
}

bool
GlobOpt::TypeSpecializeBinary(IR::Instr **pInstr, Value **pSrc1Val, Value **pSrc2Val, Value **pDstVal, Value *const src1OriginalVal, Value *const src2OriginalVal, bool *redoTypeSpecRef)
{LOGMEIN("GlobOpt.cpp] 11036\n");
    IR::Instr *&instr = *pInstr;
    int32 min1 = INT32_MIN, max1 = INT32_MAX, min2 = INT32_MIN, max2 = INT32_MAX, newMin, newMax, tmp;
    Js::OpCode opcode;
    IR::Opnd *src1, *src2;
    Value *&src1Val = *pSrc1Val;
    Value *&src2Val = *pSrc2Val;

    // We don't need to do typespec for asmjs
    if (IsTypeSpecPhaseOff(this->func) || GetIsAsmJSFunc())
    {LOGMEIN("GlobOpt.cpp] 11046\n");
        return false;
    }

    if (OpCodeAttr::IsInlineBuiltIn(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 11051\n");
        this->TypeSpecializeInlineBuiltInBinary(pInstr, src1Val, src2Val, pDstVal, src1OriginalVal, src2OriginalVal);
        return true;
    }

    if (src1Val)
    {LOGMEIN("GlobOpt.cpp] 11057\n");
        src1Val->GetValueInfo()->GetIntValMinMax(&min1, &max1, this->DoAggressiveIntTypeSpec());
    }
    if (src2Val)
    {LOGMEIN("GlobOpt.cpp] 11061\n");
        src2Val->GetValueInfo()->GetIntValMinMax(&min2, &max2, this->DoAggressiveIntTypeSpec());
    }

    // Type specialize binary operators to int32

    bool src1Lossy = true;
    bool src2Lossy = true;
    IR::BailOutKind bailOutKind = IR::BailOutInvalid;
    bool ignoredIntOverflow = this->ignoredIntOverflowForCurrentInstr;
    bool ignoredNegativeZero = false;
    bool skipSrc2 = false;
    bool skipDst = false;
    bool needsBoolConv = false;
    AddSubConstantInfo addSubConstantInfo;

    switch (instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 11078\n");
    case Js::OpCode::Or_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11081\n");
            return false;
        }
        this->PropagateIntRangeBinary(instr, min1, max1, min2, max2, &newMin, &newMax);
        opcode = Js::OpCode::Or_I4;
        break;

    case Js::OpCode::And_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11090\n");
            return false;
        }
        this->PropagateIntRangeBinary(instr, min1, max1, min2, max2, &newMin, &newMax);
        opcode = Js::OpCode::And_I4;
        break;

    case Js::OpCode::Xor_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11099\n");
            return false;
        }
        this->PropagateIntRangeBinary(instr, min1, max1, min2, max2, &newMin, &newMax);
        opcode = Js::OpCode::Xor_I4;
        break;

    case Js::OpCode::Shl_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11108\n");
            return false;
        }
        this->PropagateIntRangeBinary(instr, min1, max1, min2, max2, &newMin, &newMax);
        opcode = Js::OpCode::Shl_I4;
        break;

    case Js::OpCode::Shr_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11117\n");
            return false;
        }
        this->PropagateIntRangeBinary(instr, min1, max1, min2, max2, &newMin, &newMax);
        opcode = Js::OpCode::Shr_I4;
        break;

    case Js::OpCode::ShrU_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11126\n");
            return false;
        }
        if (min1 < 0 && IntConstantBounds(min2, max2).And_0x1f().Contains(0))
        {LOGMEIN("GlobOpt.cpp] 11130\n");
            // Src1 may be too large to represent as a signed int32, and src2 may be zero. Unless the resulting value is only
            // used as a signed int32 (hence allowing us to ignore the result's sign), don't specialize the instruction.
            if (!instr->ignoreIntOverflow)
                return false;
            ignoredIntOverflow = true;
        }
        this->PropagateIntRangeBinary(instr, min1, max1, min2, max2, &newMin, &newMax);
        opcode = Js::OpCode::ShrU_I4;
        break;

    case Js::OpCode::BrUnLe_A:
        // Folding the branch based on bounds will attempt a lossless int32 conversion of the sources if they are not definitely
        // int already, so require that both sources are likely int for folding.
        if (DoConstFold() &&
            !IsLoopPrePass() &&
            TryOptConstFoldBrUnsignedGreaterThan(instr, false, src1Val, min1, max1, src2Val, min2, max2))
        {LOGMEIN("GlobOpt.cpp] 11147\n");
            return true;
        }

        if (min1 >= 0 && min2 >= 0)
        {LOGMEIN("GlobOpt.cpp] 11152\n");
            // Only handle positive values since this is unsigned...

            // Bounds are tracked only for likely int values. Only likely int values may have bounds that are not the defaults
            // (INT32_MIN, INT32_MAX), so we're good.
            Assert(src1Val);
            Assert(src1Val->GetValueInfo()->IsLikelyInt());
            Assert(src2Val);
            Assert(src2Val->GetValueInfo()->IsLikelyInt());

            UpdateIntBoundsForLessThanOrEqualBranch(src1Val, src2Val);
        }

        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11166\n");
            return false;
        }
        newMin = newMax = 0;
        opcode = Js::OpCode::BrUnLe_I4;
        break;

    case Js::OpCode::BrUnLt_A:
        // Folding the branch based on bounds will attempt a lossless int32 conversion of the sources if they are not definitely
        // int already, so require that both sources are likely int for folding.
        if (DoConstFold() &&
            !IsLoopPrePass() &&
            TryOptConstFoldBrUnsignedLessThan(instr, true, src1Val, min1, max1, src2Val, min2, max2))
        {LOGMEIN("GlobOpt.cpp] 11179\n");
            return true;
        }

        if (min1 >= 0 && min2 >= 0)
        {LOGMEIN("GlobOpt.cpp] 11184\n");
            // Only handle positive values since this is unsigned...

            // Bounds are tracked only for likely int values. Only likely int values may have bounds that are not the defaults
            // (INT32_MIN, INT32_MAX), so we're good.
            Assert(src1Val);
            Assert(src1Val->GetValueInfo()->IsLikelyInt());
            Assert(src2Val);
            Assert(src2Val->GetValueInfo()->IsLikelyInt());

            UpdateIntBoundsForLessThanBranch(src1Val, src2Val);
        }

        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11198\n");
            return false;
        }
        newMin = newMax = 0;
        opcode = Js::OpCode::BrUnLt_I4;
        break;

    case Js::OpCode::BrUnGe_A:
        // Folding the branch based on bounds will attempt a lossless int32 conversion of the sources if they are not definitely
        // int already, so require that both sources are likely int for folding.
        if (DoConstFold() &&
            !IsLoopPrePass() &&
            TryOptConstFoldBrUnsignedLessThan(instr, false, src1Val, min1, max1, src2Val, min2, max2))
        {LOGMEIN("GlobOpt.cpp] 11211\n");
            return true;
        }

        if (min1 >= 0 && min2 >= 0)
        {LOGMEIN("GlobOpt.cpp] 11216\n");
            // Only handle positive values since this is unsigned...

            // Bounds are tracked only for likely int values. Only likely int values may have bounds that are not the defaults
            // (INT32_MIN, INT32_MAX), so we're good.
            Assert(src1Val);
            Assert(src1Val->GetValueInfo()->IsLikelyInt());
            Assert(src2Val);
            Assert(src2Val->GetValueInfo()->IsLikelyInt());

            UpdateIntBoundsForGreaterThanOrEqualBranch(src1Val, src2Val);
        }

        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11230\n");
            return false;
        }
        newMin = newMax = 0;
        opcode = Js::OpCode::BrUnGe_I4;
        break;

    case Js::OpCode::BrUnGt_A:
        // Folding the branch based on bounds will attempt a lossless int32 conversion of the sources if they are not definitely
        // int already, so require that both sources are likely int for folding.
        if (DoConstFold() &&
            !IsLoopPrePass() &&
            TryOptConstFoldBrUnsignedGreaterThan(instr, true, src1Val, min1, max1, src2Val, min2, max2))
        {LOGMEIN("GlobOpt.cpp] 11243\n");
            return true;
        }

        if (min1 >= 0 && min2 >= 0)
        {LOGMEIN("GlobOpt.cpp] 11248\n");
            // Only handle positive values since this is unsigned...

            // Bounds are tracked only for likely int values. Only likely int values may have bounds that are not the defaults
            // (INT32_MIN, INT32_MAX), so we're good.
            Assert(src1Val);
            Assert(src1Val->GetValueInfo()->IsLikelyInt());
            Assert(src2Val);
            Assert(src2Val->GetValueInfo()->IsLikelyInt());

            UpdateIntBoundsForGreaterThanBranch(src1Val, src2Val);
        }

        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11262\n");
            return false;
        }
        newMin = newMax = 0;
        opcode = Js::OpCode::BrUnGt_I4;
        break;

    case Js::OpCode::CmUnLe_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11271\n");
            return false;
        }
        newMin = 0;
        newMax = 1;
        opcode = Js::OpCode::CmUnLe_I4;
        needsBoolConv = true;
        break;

    case Js::OpCode::CmUnLt_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11282\n");
            return false;
        }
        newMin = 0;
        newMax = 1;
        opcode = Js::OpCode::CmUnLt_I4;
        needsBoolConv = true;
        break;

    case Js::OpCode::CmUnGe_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11293\n");
            return false;
        }
        newMin = 0;
        newMax = 1;
        opcode = Js::OpCode::CmUnGe_I4;
        needsBoolConv = true;
        break;

    case Js::OpCode::CmUnGt_A:
        if (!DoLossyIntTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 11304\n");
            return false;
        }
        newMin = 0;
        newMax = 1;
        opcode = Js::OpCode::CmUnGt_I4;
        needsBoolConv = true;
        break;

    case Js::OpCode::Expo_A:
        {LOGMEIN("GlobOpt.cpp] 11314\n");
            src1Val = src1OriginalVal;
            src2Val = src2OriginalVal;
            return this->TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal);
        }

    case Js::OpCode::Div_A:
        {LOGMEIN("GlobOpt.cpp] 11321\n");
            ValueType specializedValueType = GetDivValueType(instr, src1Val, src2Val, true);
            if (specializedValueType.IsFloat())
            {LOGMEIN("GlobOpt.cpp] 11324\n");
                // Either result is float or 1/x  or cst1/cst2 where cst1%cst2 != 0
                // Note: We should really constant fold cst1%cst2...
                src1Val = src1OriginalVal;
                src2Val = src2OriginalVal;
                return this->TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal);
            }
#ifdef _M_ARM
            if (!AutoSystemInfo::Data.ArmDivAvailable())
            {LOGMEIN("GlobOpt.cpp] 11333\n");
                return false;
            }
#endif
            if (specializedValueType.IsInt())
            {LOGMEIN("GlobOpt.cpp] 11338\n");
                if (max2 == 0x80000000 || (min2 == 0 && max2 == 00))
                {LOGMEIN("GlobOpt.cpp] 11340\n");
                    return false;
                }

                if (min1 == 0x80000000 && min2 <= -1 && max2 >= -1)
                {LOGMEIN("GlobOpt.cpp] 11345\n");
                    // Prevent integer overflow, as div by zero or MIN_INT / -1 will throw an exception
                    // Or we know we are dividing by zero (which is weird to have because the profile data
                    // say we got an int)
                    bailOutKind = IR::BailOutOnDivOfMinInt;
                }

                src1Lossy = false;          // Detect -0 on the sources
                src2Lossy = false;

                opcode = Js::OpCode::Div_I4;
                bailOutKind |= IR::BailOnDivResultNotInt;
                if (max2 >= 0 && min2 <= 0)
                {LOGMEIN("GlobOpt.cpp] 11358\n");
                    // Need to check for divide by zero if the denominator range includes 0
                    bailOutKind |= IR::BailOutOnDivByZero;
                }


                if (max1 >= 0 && min1 <= 0)
                {LOGMEIN("GlobOpt.cpp] 11365\n");
                    // Numerator contains 0 so the result contains 0
                    newMin = 0;
                    newMax = 0;

                    if (min2 < 0)
                    {LOGMEIN("GlobOpt.cpp] 11371\n");
                        // Denominator may be negative, so the result could be negative 0
                        if (instr->ShouldCheckForNegativeZero())
                        {LOGMEIN("GlobOpt.cpp] 11374\n");
                            bailOutKind |= IR::BailOutOnNegativeZero;
                        }
                        else
                        {
                            ignoredNegativeZero = true;
                        }
                    }
                }
                else
                {
                    // Initialize to invalid value, one of the condition below will update it correctly
                    newMin = INT_MAX;
                    newMax = INT_MIN;
                }

                // Deal with the positive and negative range separately for both the numerator and the denominator,
                // and integrate to the overall min and max.

                // If the result is positive (positive/positive or negative/negative):
                //      The min should be the  smallest magnitude numerator   (positive_Min1 | negative_Max1)
                //                divided by   ---------------------------------------------------------------
                //                             largest magnitude denominator  (positive_Max2 | negative_Min2)
                //
                //      The max should be the  largest magnitude numerator    (positive_Max1 | negative_Max1)
                //                divided by   ---------------------------------------------------------------
                //                             smallest magnitude denominator (positive_Min2 | negative_Max2)

                // If the result is negative (positive/negative or positive/negative):
                //      The min should be the  largest magnitude numerator    (positive_Max1 | negative_Min1)
                //                divided by   ---------------------------------------------------------------
                //                             smallest magnitude denominator (negative_Max2 | positive_Min2)
                //
                //      The max should be the  smallest magnitude numerator   (positive_Min1 | negative_Max1)
                //                divided by   ---------------------------------------------------------------
                //                             largest magnitude denominator  (negative_Min2 | positive_Max2)

                // Consider: The range can be slightly more precise if we take care of the rounding

                if (max1 > 0)
                {LOGMEIN("GlobOpt.cpp] 11414\n");
                    // Take only the positive numerator range
                    int32 positive_Min1 = max(1, min1);
                    int32 positive_Max1 = max1;
                    if (max2 > 0)
                    {LOGMEIN("GlobOpt.cpp] 11419\n");
                        // Take only the positive denominator range
                        int32 positive_Min2 = max(1, min2);
                        int32 positive_Max2 = max2;

                        // Positive / Positive
                        int32 quadrant1_Min = positive_Min1 <= positive_Max2? 1 : positive_Min1 / positive_Max2;
                        int32 quadrant1_Max = positive_Max1 <= positive_Min2? 1 : positive_Max1 / positive_Min2;

                        Assert(1 <= quadrant1_Min && quadrant1_Min <= quadrant1_Max);

                        // The result should positive
                        newMin = min(newMin, quadrant1_Min);
                        newMax = max(newMax, quadrant1_Max);
                    }

                    if (min2 < 0)
                    {LOGMEIN("GlobOpt.cpp] 11436\n");
                        // Take only the negative denominator range
                        int32 negative_Min2 = min2;
                        int32 negative_Max2 = min(-1, max2);

                        // Positive / Negative
                        int32 quadrant2_Min = -positive_Max1 >= negative_Max2? -1 : positive_Max1 / negative_Max2;
                        int32 quadrant2_Max = -positive_Min1 >= negative_Min2? -1 : positive_Min1 / negative_Min2;

                        // The result should negative
                        Assert(quadrant2_Min <= quadrant2_Max && quadrant2_Max <= -1);

                        newMin = min(newMin, quadrant2_Min);
                        newMax = max(newMax, quadrant2_Max);
                    }
                }
                if (min1 < 0)
                {LOGMEIN("GlobOpt.cpp] 11453\n");
                    // Take only the native numerator range
                    int32 negative_Min1 = min1;
                    int32 negative_Max1 = min(-1, max1);

                    if (max2 > 0)
                    {LOGMEIN("GlobOpt.cpp] 11459\n");
                        // Take only the positive denominator range
                        int32 positive_Min2 = max(1, min2);
                        int32 positive_Max2 = max2;

                        // Negative / Positive
                        int32 quadrant4_Min = negative_Min1 >= -positive_Min2? -1 : negative_Min1 / positive_Min2;
                        int32 quadrant4_Max = negative_Max1 >= -positive_Max2? -1 : negative_Max1 / positive_Max2;

                        // The result should negative
                        Assert(quadrant4_Min <= quadrant4_Max && quadrant4_Max <= -1);

                        newMin = min(newMin, quadrant4_Min);
                        newMax = max(newMax, quadrant4_Max);
                    }

                    if (min2 < 0)
                    {LOGMEIN("GlobOpt.cpp] 11476\n");

                        // Take only the negative denominator range
                        int32 negative_Min2 = min2;
                        int32 negative_Max2 = min(-1, max2);

                        int32 quadrant3_Min;
                        int32 quadrant3_Max;
                        // Negative / Negative
                        if (negative_Max1 == 0x80000000 && negative_Min2 == -1)
                        {LOGMEIN("GlobOpt.cpp] 11486\n");
                            quadrant3_Min = negative_Max1 >= negative_Min2? 1 : (negative_Max1+1) / negative_Min2;
                        }
                        else
                        {
                            quadrant3_Min = negative_Max1 >= negative_Min2? 1 : negative_Max1 / negative_Min2;
                        }
                        if (negative_Min1 == 0x80000000 && negative_Max2 == -1)
                        {LOGMEIN("GlobOpt.cpp] 11494\n");
                            quadrant3_Max = negative_Min1 >= negative_Max2? 1 : (negative_Min1+1) / negative_Max2;
                        }
                        else
                        {
                            quadrant3_Max = negative_Min1 >= negative_Max2? 1 : negative_Min1 / negative_Max2;
                        }

                        // The result should positive
                        Assert(1 <= quadrant3_Min && quadrant3_Min <= quadrant3_Max);

                        newMin = min(newMin, quadrant3_Min);
                        newMax = max(newMax, quadrant3_Max);
                    }
                }
                Assert(newMin <= newMax);
                // Continue to int type spec
                break;
            }
        }
        // fall-through
    default:
        {
            const bool involesLargeInt32 =
                (src1Val && src1Val->GetValueInfo()->IsLikelyUntaggedInt()) ||
                (src2Val && src2Val->GetValueInfo()->IsLikelyUntaggedInt());
            const auto trySpecializeToFloat =
                [&](const bool mayOverflow) -> bool
            {
                // It has been determined that this instruction cannot be int-specialized. Need to determine whether to attempt
                // to float-specialize the instruction, or leave it unspecialized.
                if((involesLargeInt32
#if INT32VAR
                    && mayOverflow
#endif
                   ) || (instr->m_opcode == Js::OpCode::Mul_A && !this->DoAggressiveMulIntTypeSpec())
                  )
                {LOGMEIN("GlobOpt.cpp] 11531\n");
                    // An input range is completely outside the range of an int31 and the operation is likely to overflow.
                    // Additionally, on 32-bit platforms, the value is untaggable and will be a JavascriptNumber, which is
                    // significantly slower to use in an unspecialized operation compared to a tagged int. So, try to
                    // float-specialize the instruction.
                    src1Val = src1OriginalVal;
                    src2Val = src2OriginalVal;
                    return TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal);
                }
                return false;
            };

            if (instr->m_opcode != Js::OpCode::ArgOut_A_InlineBuiltIn)
            {LOGMEIN("GlobOpt.cpp] 11544\n");
                if ((src1Val && src1Val->GetValueInfo()->IsLikelyFloat()) || (src2Val && src2Val->GetValueInfo()->IsLikelyFloat()))
                {LOGMEIN("GlobOpt.cpp] 11546\n");
                    // Try to type specialize to float
                    src1Val = src1OriginalVal;
                    src2Val = src2OriginalVal;
                    return this->TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal);
                }

                if (src1Val == nullptr ||
                    src2Val == nullptr ||
                    !src1Val->GetValueInfo()->IsLikelyInt() ||
                    !src2Val->GetValueInfo()->IsLikelyInt() ||
                    (
                        !DoAggressiveIntTypeSpec() &&
                        (
                            !(src1Val->GetValueInfo()->IsInt() || IsSwitchInt32TypeSpecialized(instr, currentBlock)) ||
                            !src2Val->GetValueInfo()->IsInt()
                        )
                    ) ||
                    (instr->GetSrc1()->IsRegOpnd() && instr->GetSrc1()->AsRegOpnd()->m_sym->m_isNotInt) ||
                    (instr->GetSrc2()->IsRegOpnd() && instr->GetSrc2()->AsRegOpnd()->m_sym->m_isNotInt))
                {LOGMEIN("GlobOpt.cpp] 11566\n");
                    return trySpecializeToFloat(true);
                }
            }

            // Try to type specialize to int32

            // If one of the values is a float constant with a value that fits in a uint32 but not an int32,
            // and the instruction can ignore int overflow, the source value for the purposes of int specialization
            // would have been changed to an int constant value by ignoring overflow. But, the conversion is still lossy.
            if (!(src1OriginalVal && src1OriginalVal->GetValueInfo()->IsFloatConstant() && src1Val && src1Val->GetValueInfo()->HasIntConstantValue()))
            {LOGMEIN("GlobOpt.cpp] 11577\n");
                src1Lossy = false;
            }
            if (!(src2OriginalVal && src2OriginalVal->GetValueInfo()->IsFloatConstant() && src2Val && src2Val->GetValueInfo()->HasIntConstantValue()))
            {LOGMEIN("GlobOpt.cpp] 11581\n");
                src2Lossy = false;
            }

            switch(instr->m_opcode)
            {LOGMEIN("GlobOpt.cpp] 11586\n");
            case Js::OpCode::ArgOut_A_InlineBuiltIn:
                // If the src is already type-specialized, if we don't type-specialize ArgOut_A_InlineBuiltIn instr, we'll get additional ToVar.
                //   So, to avoid that, type-specialize the ArgOut_A_InlineBuiltIn instr.
                // Else we don't need to type-specialize the instr, we are fine with src being Var.
                if (instr->GetSrc1()->IsRegOpnd())
                {LOGMEIN("GlobOpt.cpp] 11592\n");
                    StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
                    if (this->IsInt32TypeSpecialized(sym, this->currentBlock))
                    {LOGMEIN("GlobOpt.cpp] 11595\n");
                        opcode = instr->m_opcode;
                        skipDst = true;                 // We should keep dst as is, otherwise the link opnd for next ArgOut/InlineBuiltInStart would be broken.
                        skipSrc2 = true;                // src2 is linkOpnd. We don't need to type-specialize it.
                        newMin = min1; newMax = max1;   // Values don't matter, these are unused.
                        goto LOutsideSwitch;            // Continue to int-type-specialize.
                    }
                    else if (this->IsFloat64TypeSpecialized(sym, this->currentBlock))
                    {LOGMEIN("GlobOpt.cpp] 11603\n");
                        src1Val = src1OriginalVal;
                        src2Val = src2OriginalVal;
                        return this->TypeSpecializeFloatBinary(instr, src1Val, src2Val, pDstVal);
                    }
                    else if (this->IsSimd128F4TypeSpecialized(sym, this->currentBlock))
                    {LOGMEIN("GlobOpt.cpp] 11609\n");
                        // SIMD_JS
                        // We should be already using the SIMD type-spec sym. See TypeSpecializeSimd128.
                        Assert(IRType_IsSimd128(instr->GetSrc1()->GetType()));
                    }

                }
                return false;

            case Js::OpCode::Add_A:
                do // while(false)
                {LOGMEIN("GlobOpt.cpp] 11620\n");
                    const auto CannotOverflowBasedOnRelativeBounds = [&](int32 *const constantValueRef)
                    {LOGMEIN("GlobOpt.cpp] 11622\n");
                        Assert(constantValueRef);

                        if(min2 == max2 &&
                            src1Val->GetValueInfo()->IsIntBounded() &&
                            src1Val->GetValueInfo()->AsIntBounded()->Bounds()->AddCannotOverflowBasedOnRelativeBounds(min2))
                        {LOGMEIN("GlobOpt.cpp] 11628\n");
                            *constantValueRef = min2;
                            return true;
                        }
                        else if(
                            min1 == max1 &&
                            src2Val->GetValueInfo()->IsIntBounded() &&
                            src2Val->GetValueInfo()->AsIntBounded()->Bounds()->AddCannotOverflowBasedOnRelativeBounds(min1))
                        {LOGMEIN("GlobOpt.cpp] 11636\n");
                            *constantValueRef = min1;
                            return true;
                        }
                        return false;
                    };

                    if (Int32Math::Add(min1, min2, &newMin))
                    {LOGMEIN("GlobOpt.cpp] 11644\n");
                        int32 constantSrcValue;
                        if(CannotOverflowBasedOnRelativeBounds(&constantSrcValue))
                        {LOGMEIN("GlobOpt.cpp] 11647\n");
                            newMin = constantSrcValue >= 0 ? INT32_MAX : INT32_MIN;
                        }
                        else if(instr->ShouldCheckForIntOverflow())
                        {LOGMEIN("GlobOpt.cpp] 11651\n");
                            if(involesLargeInt32 || !DoAggressiveIntTypeSpec())
                            {LOGMEIN("GlobOpt.cpp] 11653\n");
                                // May overflow
                                return trySpecializeToFloat(true);
                            }
                            bailOutKind |= IR::BailOutOnOverflow;
                            newMin = min1 < 0 ? INT32_MIN : INT32_MAX;
                        }
                        else
                        {
                            // When ignoring overflow, the range needs to account for overflow. For any Add or Sub, since
                            // overflow causes the value to wrap around, and we don't have a way to specify a lower and upper
                            // range of ints, we use the full range of int32s.
                            ignoredIntOverflow = true;
                            newMin = INT32_MIN;
                            newMax = INT32_MAX;
                            break;
                        }
                    }
                    if (Int32Math::Add(max1, max2, &newMax))
                    {LOGMEIN("GlobOpt.cpp] 11672\n");
                        int32 constantSrcValue;
                        if(CannotOverflowBasedOnRelativeBounds(&constantSrcValue))
                        {LOGMEIN("GlobOpt.cpp] 11675\n");
                            newMax = constantSrcValue >= 0 ? INT32_MAX : INT32_MIN;
                        }
                        else if(instr->ShouldCheckForIntOverflow())
                        {LOGMEIN("GlobOpt.cpp] 11679\n");
                            if(involesLargeInt32 || !DoAggressiveIntTypeSpec())
                            {LOGMEIN("GlobOpt.cpp] 11681\n");
                                // May overflow
                                return trySpecializeToFloat(true);
                            }
                            bailOutKind |= IR::BailOutOnOverflow;
                            newMax = max1 < 0 ? INT32_MIN : INT32_MAX;
                        }
                        else
                        {
                            // See comment about ignoring overflow above
                            ignoredIntOverflow = true;
                            newMin = INT32_MIN;
                            newMax = INT32_MAX;
                            break;
                        }
                    }
                    if(bailOutKind & IR::BailOutOnOverflow)
                    {LOGMEIN("GlobOpt.cpp] 11698\n");
                        Assert(bailOutKind == IR::BailOutOnOverflow);
                        Assert(instr->ShouldCheckForIntOverflow());
                        int32 temp;
                        if(Int32Math::Add(
                            Int32Math::NearestInRangeTo(0, min1, max1),
                            Int32Math::NearestInRangeTo(0, min2, max2),
                            &temp))
                        {LOGMEIN("GlobOpt.cpp] 11706\n");
                            // Always overflows
                            return trySpecializeToFloat(true);
                        }
                    }
                } while(false);

                if (!this->IsLoopPrePass() && newMin == newMax && bailOutKind == IR::BailOutInvalid)
                {LOGMEIN("GlobOpt.cpp] 11714\n");
                    // Take care of Add with zero here, since we know we're dealing with 2 numbers.
                    this->CaptureByteCodeSymUses(instr);
                    IR::Opnd *src;
                    bool isAddZero = true;
                    int32 intConstantValue;
                    if (src1Val->GetValueInfo()->TryGetIntConstantValue(&intConstantValue) && intConstantValue == 0)
                    {LOGMEIN("GlobOpt.cpp] 11721\n");
                        src = instr->UnlinkSrc2();
                        instr->FreeSrc1();
                    }
                    else if (src2Val->GetValueInfo()->TryGetIntConstantValue(&intConstantValue) && intConstantValue == 0)
                    {LOGMEIN("GlobOpt.cpp] 11726\n");
                        src = instr->UnlinkSrc1();
                        instr->FreeSrc2();
                    }
                    else
                    {
                        // This should have been handled by const folding, unless:
                        // - A source's value was substituted with a different value here, which is after const folding happened
                        // - A value is not definitely int, but once converted to definite int, it would be zero due to a
                        //   condition in the source code such as if(a === 0). Ideally, we would specialize the sources and
                        //   remove the add, but doesn't seem too important for now.
                        Assert(
                            !DoConstFold() ||
                            src1Val != src1OriginalVal ||
                            src2Val != src2OriginalVal ||
                            !src1Val->GetValueInfo()->IsInt() ||
                            !src2Val->GetValueInfo()->IsInt());
                        isAddZero = false;
                        src = nullptr;
                    }
                    if (isAddZero)
                    {LOGMEIN("GlobOpt.cpp] 11747\n");
                        IR::Instr *newInstr = IR::Instr::New(Js::OpCode::Ld_A, instr->UnlinkDst(), src, instr->m_func);
                        newInstr->SetByteCodeOffset(instr);

                        instr->m_opcode = Js::OpCode::Nop;
                        this->currentBlock->InsertInstrAfter(newInstr, instr);
                        return true;
                    }
                }

                if(!ignoredIntOverflow)
                {LOGMEIN("GlobOpt.cpp] 11758\n");
                    if(min2 == max2 &&
                        (!IsLoopPrePass() || IsPrepassSrcValueInfoPrecise(instr->GetSrc2(), src2Val)) &&
                        instr->GetSrc1()->IsRegOpnd())
                    {LOGMEIN("GlobOpt.cpp] 11762\n");
                        addSubConstantInfo.Set(instr->GetSrc1()->AsRegOpnd()->m_sym, src1Val, min1 == max1, min2);
                    }
                    else if(
                        min1 == max1 &&
                        (!IsLoopPrePass() || IsPrepassSrcValueInfoPrecise(instr->GetSrc1(), src1Val)) &&
                        instr->GetSrc2()->IsRegOpnd())
                    {LOGMEIN("GlobOpt.cpp] 11769\n");
                        addSubConstantInfo.Set(instr->GetSrc2()->AsRegOpnd()->m_sym, src2Val, min2 == max2, min1);
                    }
                }

                opcode = Js::OpCode::Add_I4;
                break;

            case Js::OpCode::Sub_A:
                do // while(false)
                {LOGMEIN("GlobOpt.cpp] 11779\n");
                    const auto CannotOverflowBasedOnRelativeBounds = [&]()
                    {
                        return
                            min2 == max2 &&
                            src1Val->GetValueInfo()->IsIntBounded() &&
                            src1Val->GetValueInfo()->AsIntBounded()->Bounds()->SubCannotOverflowBasedOnRelativeBounds(min2);
                    };

                    if (Int32Math::Sub(min1, max2, &newMin))
                    {LOGMEIN("GlobOpt.cpp] 11789\n");
                        if(CannotOverflowBasedOnRelativeBounds())
                        {LOGMEIN("GlobOpt.cpp] 11791\n");
                            Assert(min2 == max2);
                            newMin = min2 >= 0 ? INT32_MIN : INT32_MAX;
                        }
                        else if(instr->ShouldCheckForIntOverflow())
                        {LOGMEIN("GlobOpt.cpp] 11796\n");
                            if(involesLargeInt32 || !DoAggressiveIntTypeSpec())
                            {LOGMEIN("GlobOpt.cpp] 11798\n");
                                // May overflow
                                return trySpecializeToFloat(true);
                            }
                            bailOutKind |= IR::BailOutOnOverflow;
                            newMin = min1 < 0 ? INT32_MIN : INT32_MAX;
                        }
                        else
                        {
                            // When ignoring overflow, the range needs to account for overflow. For any Add or Sub, since overflow
                            // causes the value to wrap around, and we don't have a way to specify a lower and upper range of ints,
                            // we use the full range of int32s.
                            ignoredIntOverflow = true;
                            newMin = INT32_MIN;
                            newMax = INT32_MAX;
                            break;
                        }
                    }
                    if (Int32Math::Sub(max1, min2, &newMax))
                    {LOGMEIN("GlobOpt.cpp] 11817\n");
                        if(CannotOverflowBasedOnRelativeBounds())
                        {LOGMEIN("GlobOpt.cpp] 11819\n");
                            Assert(min2 == max2);
                            newMax = min2 >= 0 ? INT32_MIN: INT32_MAX;
                        }
                        else if(instr->ShouldCheckForIntOverflow())
                        {LOGMEIN("GlobOpt.cpp] 11824\n");
                            if(involesLargeInt32 || !DoAggressiveIntTypeSpec())
                            {LOGMEIN("GlobOpt.cpp] 11826\n");
                                // May overflow
                                return trySpecializeToFloat(true);
                            }
                            bailOutKind |= IR::BailOutOnOverflow;
                            newMax = max1 < 0 ? INT32_MIN : INT32_MAX;
                        }
                        else
                        {
                            // See comment about ignoring overflow above
                            ignoredIntOverflow = true;
                            newMin = INT32_MIN;
                            newMax = INT32_MAX;
                            break;
                        }
                    }
                    if(bailOutKind & IR::BailOutOnOverflow)
                    {LOGMEIN("GlobOpt.cpp] 11843\n");
                        Assert(bailOutKind == IR::BailOutOnOverflow);
                        Assert(instr->ShouldCheckForIntOverflow());
                        int32 temp;
                        if(Int32Math::Sub(
                            Int32Math::NearestInRangeTo(-1, min1, max1),
                            Int32Math::NearestInRangeTo(0, min2, max2),
                            &temp))
                        {LOGMEIN("GlobOpt.cpp] 11851\n");
                            // Always overflows
                            return trySpecializeToFloat(true);
                        }
                    }
                } while(false);

                if(!ignoredIntOverflow &&
                    min2 == max2 &&
                    min2 != INT32_MIN &&
                    (!IsLoopPrePass() || IsPrepassSrcValueInfoPrecise(instr->GetSrc2(), src2Val)) &&
                    instr->GetSrc1()->IsRegOpnd())
                {LOGMEIN("GlobOpt.cpp] 11863\n");
                    addSubConstantInfo.Set(instr->GetSrc1()->AsRegOpnd()->m_sym, src1Val, min1 == max1, -min2);
                }

                opcode = Js::OpCode::Sub_I4;
                break;

            case Js::OpCode::Mul_A:
            {LOGMEIN("GlobOpt.cpp] 11871\n");
                if (Int32Math::Mul(min1, min2, &newMin))
                {LOGMEIN("GlobOpt.cpp] 11873\n");
                    if (involesLargeInt32 || !DoAggressiveMulIntTypeSpec() || !DoAggressiveIntTypeSpec())
                    {LOGMEIN("GlobOpt.cpp] 11875\n");
                        // May overflow
                        return trySpecializeToFloat(true);
                    }
                    bailOutKind |= IR::BailOutOnMulOverflow;
                    newMin = (min1 < 0) ^ (min2 < 0) ? INT32_MIN : INT32_MAX;
                }
                newMax = newMin;
                if (Int32Math::Mul(max1, max2, &tmp))
                {LOGMEIN("GlobOpt.cpp] 11884\n");
                    if (involesLargeInt32 || !DoAggressiveMulIntTypeSpec() || !DoAggressiveIntTypeSpec())
                    {LOGMEIN("GlobOpt.cpp] 11886\n");
                        // May overflow
                        return trySpecializeToFloat(true);
                    }
                    bailOutKind |= IR::BailOutOnMulOverflow;
                    tmp = (max1 < 0) ^ (max2 < 0) ? INT32_MIN : INT32_MAX;
                }
                newMin = min(newMin, tmp);
                newMax = max(newMax, tmp);
                if (Int32Math::Mul(min1, max2, &tmp))
                {LOGMEIN("GlobOpt.cpp] 11896\n");
                    if (involesLargeInt32 || !DoAggressiveMulIntTypeSpec() || !DoAggressiveIntTypeSpec())
                    {LOGMEIN("GlobOpt.cpp] 11898\n");
                        // May overflow
                        return trySpecializeToFloat(true);
                    }
                    bailOutKind |= IR::BailOutOnMulOverflow;
                    tmp = (min1 < 0) ^ (max2 < 0) ? INT32_MIN : INT32_MAX;
                }
                newMin = min(newMin, tmp);
                newMax = max(newMax, tmp);
                if (Int32Math::Mul(max1, min2, &tmp))
                {LOGMEIN("GlobOpt.cpp] 11908\n");
                    if (involesLargeInt32 || !DoAggressiveMulIntTypeSpec() || !DoAggressiveIntTypeSpec())
                    {LOGMEIN("GlobOpt.cpp] 11910\n");
                        // May overflow
                        return trySpecializeToFloat(true);
                    }
                    bailOutKind |= IR::BailOutOnMulOverflow;
                    tmp = (max1 < 0) ^ (min2 < 0) ? INT32_MIN : INT32_MAX;
                }
                newMin = min(newMin, tmp);
                newMax = max(newMax, tmp);
                if (bailOutKind & IR::BailOutOnMulOverflow)
                {LOGMEIN("GlobOpt.cpp] 11920\n");
                    // CSE only if two MULs have the same overflow check behavior.
                    // Currently this is set to be ignore int32 overflow, but not 53-bit, or int32 overflow matters.
                    if (!instr->ShouldCheckFor32BitOverflow() && instr->ShouldCheckForNon32BitOverflow())
                    {LOGMEIN("GlobOpt.cpp] 11924\n");
                        // If we allow int to overflow then there can be anything in the resulting int
                        newMin = IntConstMin;
                        newMax = IntConstMax;
                        ignoredIntOverflow = true;
                    }

                    int32 temp, overflowValue;
                    if (Int32Math::Mul(
                        Int32Math::NearestInRangeTo(0, min1, max1),
                        Int32Math::NearestInRangeTo(0, min2, max2),
                        &temp,
                        &overflowValue))
                    {LOGMEIN("GlobOpt.cpp] 11937\n");
                        Assert(instr->ignoreOverflowBitCount >= 32);
                        int overflowMatters = 64 - instr->ignoreOverflowBitCount;
                        if (!ignoredIntOverflow ||
                            // Use shift to check high bits in case its negative
                            ((overflowValue << overflowMatters) >> overflowMatters) != overflowValue
                            )
                        {LOGMEIN("GlobOpt.cpp] 11944\n");
                            // Always overflows
                            return trySpecializeToFloat(true);
                        }
                    }

                }

                if (newMin <= 0 && newMax >= 0 &&                   // New range crosses zero
                    (min1 < 0 || min2 < 0) &&                       // An operand's range contains a negative integer
                    !(min1 > 0 || min2 > 0) &&                      // Neither operand's range contains only positive integers
                    !instr->GetSrc1()->IsEqual(instr->GetSrc2()))   // The operands don't have the same value
                {LOGMEIN("GlobOpt.cpp] 11956\n");
                    if (instr->ShouldCheckForNegativeZero())
                    {LOGMEIN("GlobOpt.cpp] 11958\n");
                        // -0 matters since the sym is not a local, or is used in a way in which -0 would differ from +0
                        if (!DoAggressiveIntTypeSpec())
                        {LOGMEIN("GlobOpt.cpp] 11961\n");
                            // May result in -0
                            return trySpecializeToFloat(false);
                        }
                        if (((min1 == 0 && max1 == 0) || (min2 == 0 && max2 == 0)) && (max1 < 0 || max2 < 0))
                        {LOGMEIN("GlobOpt.cpp] 11966\n");
                            // Always results in -0
                            return trySpecializeToFloat(false);
                        }
                        bailOutKind |= IR::BailOutOnNegativeZero;
                    }
                    else
                    {
                        ignoredNegativeZero = true;
                    }
                }
                opcode = Js::OpCode::Mul_I4;
                break;
            }
            case Js::OpCode::Rem_A:
            {LOGMEIN("GlobOpt.cpp] 11981\n");
                src2 = instr->GetSrc2();
                if (!this->IsLoopPrePass() && min2 == max2 && min1 >= 0)
                {LOGMEIN("GlobOpt.cpp] 11984\n");
                    int32 value = min2;

                    if (value == (1 << Math::Log2(value)) && src2->IsAddrOpnd())
                    {LOGMEIN("GlobOpt.cpp] 11988\n");
                        Assert(src2->AsAddrOpnd()->IsVar());
                        instr->m_opcode = Js::OpCode::And_A;
                        src2->AsAddrOpnd()->SetAddress(Js::TaggedInt::ToVarUnchecked(value - 1),
                            IR::AddrOpndKindConstantVar);
                        *pSrc2Val = GetIntConstantValue(value - 1, instr);
                        src2Val = *pSrc2Val;
                        return this->TypeSpecializeBinary(&instr, pSrc1Val, pSrc2Val, pDstVal, src1OriginalVal, src2Val, redoTypeSpecRef);
                    }
                }
#ifdef _M_ARM
                if (!AutoSystemInfo::Data.ArmDivAvailable())
                {LOGMEIN("GlobOpt.cpp] 12000\n");
                    return false;
                }
#endif
                if (min1 < 0)
                {LOGMEIN("GlobOpt.cpp] 12005\n");
                    // The most negative it can be is min1, unless limited by min2/max2
                    int32 negMaxAbs2;
                    if (min2 == INT32_MIN)
                    {LOGMEIN("GlobOpt.cpp] 12009\n");
                        negMaxAbs2 = INT32_MIN;
                    }
                    else
                    {
                        negMaxAbs2 = -max(abs(min2), abs(max2)) + 1;
                    }
                    newMin = max(min1, negMaxAbs2);
                }
                else
                {
                    newMin = 0;
                }

                bool isModByPowerOf2 = (instr->IsProfiledInstr() && instr->m_func->HasProfileInfo() &&
                    instr->m_func->GetReadOnlyProfileInfo()->IsModulusOpByPowerOf2(static_cast<Js::ProfileId>(instr->AsProfiledInstr()->u.profileId)));

                if(isModByPowerOf2)
                {LOGMEIN("GlobOpt.cpp] 12027\n");
                    Assert(bailOutKind == IR::BailOutInvalid);
                    bailOutKind = IR::BailOnModByPowerOf2;
                    newMin = 0;
                }
                else
                {
                    if (min2 <= 0 && max2 >= 0)
                    {LOGMEIN("GlobOpt.cpp] 12035\n");
                        // Consider: We could handle the zero case with a check and bailout...
                        return false;
                    }

                    if (min1 == 0x80000000 && (min2 <= -1 && max2 >= -1))
                    {LOGMEIN("GlobOpt.cpp] 12041\n");
                        // Prevent integer overflow, as div by zero or MIN_INT / -1 will throw an exception
                        return false;
                    }
                    if (min1 < 0)
                    {LOGMEIN("GlobOpt.cpp] 12046\n");
                        if(instr->ShouldCheckForNegativeZero())
                        {LOGMEIN("GlobOpt.cpp] 12048\n");
                            if (!DoAggressiveIntTypeSpec())
                            {LOGMEIN("GlobOpt.cpp] 12050\n");
                                return false;
                            }
                            bailOutKind |= IR::BailOutOnNegativeZero;
                        }
                        else
                        {
                            ignoredNegativeZero = true;
                        }
                    }
                }
                {
                    int32 absMax2;
                    if (min2 == INT32_MIN)
                    {LOGMEIN("GlobOpt.cpp] 12064\n");
                        // abs(INT32_MIN) == INT32_MAX because of overflow
                        absMax2 = INT32_MAX;
                    }
                    else
                    {
                        absMax2 = max(abs(min2), abs(max2)) - 1;
                    }
                    newMax = min(absMax2, max(max1, 0));
                    newMax = max(newMin, newMax);
                }
                opcode = Js::OpCode::Rem_I4;
                break;
            }

            case Js::OpCode::CmEq_A:
            case Js::OpCode::CmSrEq_A:
                if (!IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val))
                {LOGMEIN("GlobOpt.cpp] 12082\n");
                    return false;
                }
                newMin = 0;
                newMax = 1;
                opcode = Js::OpCode::CmEq_I4;
                needsBoolConv = true;
                break;

            case Js::OpCode::CmNeq_A:
            case Js::OpCode::CmSrNeq_A:
                if (!IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val))
                {LOGMEIN("GlobOpt.cpp] 12094\n");
                    return false;
                }
                newMin = 0;
                newMax = 1;
                opcode = Js::OpCode::CmNeq_I4;
                needsBoolConv = true;
                break;

            case Js::OpCode::CmLe_A:
                if (!IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val))
                {LOGMEIN("GlobOpt.cpp] 12105\n");
                    return false;
                }
                newMin = 0;
                newMax = 1;
                opcode = Js::OpCode::CmLe_I4;
                needsBoolConv = true;
                break;

            case Js::OpCode::CmLt_A:
                if (!IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val))
                {LOGMEIN("GlobOpt.cpp] 12116\n");
                    return false;
                }
                newMin = 0;
                newMax = 1;
                opcode = Js::OpCode::CmLt_I4;
                needsBoolConv = true;
                break;

            case Js::OpCode::CmGe_A:
                if (!IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val))
                {LOGMEIN("GlobOpt.cpp] 12127\n");
                    return false;
                }
                newMin = 0;
                newMax = 1;
                opcode = Js::OpCode::CmGe_I4;
                needsBoolConv = true;
                break;

            case Js::OpCode::CmGt_A:
                if (!IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val))
                {LOGMEIN("GlobOpt.cpp] 12138\n");
                    return false;
                }
                newMin = 0;
                newMax = 1;
                opcode = Js::OpCode::CmGt_I4;
                needsBoolConv = true;
                break;

            case Js::OpCode::BrSrEq_A:
            case Js::OpCode::BrEq_A:
            case Js::OpCode::BrNotNeq_A:
            case Js::OpCode::BrSrNotNeq_A:
            {
                if(DoConstFold() &&
                    !IsLoopPrePass() &&
                    TryOptConstFoldBrEqual(instr, true, src1Val, min1, max1, src2Val, min2, max2))
                {LOGMEIN("GlobOpt.cpp] 12155\n");
                    return true;
                }

                const bool specialize = IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val);
                UpdateIntBoundsForEqualBranch(src1Val, src2Val);
                if(!specialize)
                {LOGMEIN("GlobOpt.cpp] 12162\n");
                    return false;
                }

                opcode = Js::OpCode::BrEq_I4;
                // We'll get a warning if we don't assign a value to these...
                // We'll assert if we use them and make a range where min > max
                newMin = 2; newMax = 1;
                break;
            }

            case Js::OpCode::BrSrNeq_A:
            case Js::OpCode::BrNeq_A:
            case Js::OpCode::BrSrNotEq_A:
            case Js::OpCode::BrNotEq_A:
            {
                if(DoConstFold() &&
                    !IsLoopPrePass() &&
                    TryOptConstFoldBrEqual(instr, false, src1Val, min1, max1, src2Val, min2, max2))
                {LOGMEIN("GlobOpt.cpp] 12181\n");
                    return true;
                }

                const bool specialize = IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val);
                UpdateIntBoundsForNotEqualBranch(src1Val, src2Val);
                if(!specialize)
                {LOGMEIN("GlobOpt.cpp] 12188\n");
                    return false;
                }

                opcode = Js::OpCode::BrNeq_I4;
                // We'll get a warning if we don't assign a value to these...
                // We'll assert if we use them and make a range where min > max
                newMin = 2; newMax = 1;
                break;
            }

            case Js::OpCode::BrGt_A:
            case Js::OpCode::BrNotLe_A:
            {
                if(DoConstFold() &&
                    !IsLoopPrePass() &&
                    TryOptConstFoldBrGreaterThan(instr, true, src1Val, min1, max1, src2Val, min2, max2))
                {LOGMEIN("GlobOpt.cpp] 12205\n");
                    return true;
                }

                const bool specialize = IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val);
                UpdateIntBoundsForGreaterThanBranch(src1Val, src2Val);
                if(!specialize)
                {LOGMEIN("GlobOpt.cpp] 12212\n");
                    return false;
                }

                opcode = Js::OpCode::BrGt_I4;
                // We'll get a warning if we don't assign a value to these...
                // We'll assert if we use them and make a range where min > max
                newMin = 2; newMax = 1;
                break;
            }

            case Js::OpCode::BrGe_A:
            case Js::OpCode::BrNotLt_A:
            {
                if(DoConstFold() &&
                    !IsLoopPrePass() &&
                    TryOptConstFoldBrGreaterThanOrEqual(instr, true, src1Val, min1, max1, src2Val, min2, max2))
                {LOGMEIN("GlobOpt.cpp] 12229\n");
                    return true;
                }

                const bool specialize = IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val);
                UpdateIntBoundsForGreaterThanOrEqualBranch(src1Val, src2Val);
                if(!specialize)
                {LOGMEIN("GlobOpt.cpp] 12236\n");
                    return false;
                }

                opcode = Js::OpCode::BrGe_I4;
                // We'll get a warning if we don't assign a value to these...
                // We'll assert if we use them and make a range where min > max
                newMin = 2; newMax = 1;
                break;
            }

            case Js::OpCode::BrLt_A:
            case Js::OpCode::BrNotGe_A:
            {
                if(DoConstFold() &&
                    !IsLoopPrePass() &&
                    TryOptConstFoldBrGreaterThanOrEqual(instr, false, src1Val, min1, max1, src2Val, min2, max2))
                {LOGMEIN("GlobOpt.cpp] 12253\n");
                    return true;
                }

                const bool specialize = IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val);
                UpdateIntBoundsForLessThanBranch(src1Val, src2Val);
                if(!specialize)
                {LOGMEIN("GlobOpt.cpp] 12260\n");
                    return false;
                }

                opcode = Js::OpCode::BrLt_I4;
                // We'll get a warning if we don't assign a value to these...
                // We'll assert if we use them and make a range where min > max
                newMin = 2; newMax = 1;
                break;
            }

            case Js::OpCode::BrLe_A:
            case Js::OpCode::BrNotGt_A:
            {
                if(DoConstFold() &&
                    !IsLoopPrePass() &&
                    TryOptConstFoldBrGreaterThan(instr, false, src1Val, min1, max1, src2Val, min2, max2))
                {LOGMEIN("GlobOpt.cpp] 12277\n");
                    return true;
                }

                const bool specialize = IsWorthSpecializingToInt32Branch(instr, src1Val, src2Val);
                UpdateIntBoundsForLessThanOrEqualBranch(src1Val, src2Val);
                if(!specialize)
                {LOGMEIN("GlobOpt.cpp] 12284\n");
                    return false;
                }

                opcode = Js::OpCode::BrLe_I4;
                // We'll get a warning if we don't assign a value to these...
                // We'll assert if we use them and make a range where min > max
                newMin = 2; newMax = 1;
                break;
            }

            default:
                return false;
            }

            // If this instruction is in a range of instructions where int overflow does not matter, we will still specialize it
            // (won't leave it unspecialized based on heuristics), since it is most likely worth specializing, and the dst value
            // needs to be guaranteed to be an int
            if(!ignoredIntOverflow &&
                !ignoredNegativeZero &&
                !needsBoolConv &&
                instr->ShouldCheckForIntOverflow() &&
                !IsWorthSpecializingToInt32(instr, src1Val, src2Val))
            {LOGMEIN("GlobOpt.cpp] 12307\n");
                // Even though type specialization is being skipped since it may not be worth it, the proper value should still be
                // maintained so that the result may be type specialized later. An int value is not created for the dst in any of
                // the following cases.
                // - A bailout check is necessary to specialize this instruction. The bailout check is what guarantees the result to
                //   be an int, but since we're not going to specialize this instruction, there won't be a bailout check.
                // - Aggressive int type specialization is disabled and we're in a loop prepass. We're conservative on dst values in
                //   that case, especially if the dst sym is live on the back-edge.
                if(bailOutKind == IR::BailOutInvalid &&
                    instr->GetDst() &&
                    src1Val->GetValueInfo()->IsInt() &&
                    src2Val->GetValueInfo()->IsInt() &&
                    (DoAggressiveIntTypeSpec() || !this->IsLoopPrePass()))
                {LOGMEIN("GlobOpt.cpp] 12320\n");
                    *pDstVal = CreateDstUntransferredIntValue(newMin, newMax, instr, src1Val, src2Val);
                }
                return false;
            }
        } // case default
    } // switch

LOutsideSwitch:
    this->ignoredIntOverflowForCurrentInstr = ignoredIntOverflow;
    this->ignoredNegativeZeroForCurrentInstr = ignoredNegativeZero;

    {
        // Try CSE again before modifying the IR, in case some attributes are required for successful CSE
        Value *src1IndirIndexVal = nullptr;
        if(CSEOptimize(currentBlock, &instr, &src1Val, &src2Val, &src1IndirIndexVal, true /* intMathExprOnly */))
        {LOGMEIN("GlobOpt.cpp] 12336\n");
            *redoTypeSpecRef = true;
            return false;
        }
    }

    const Js::OpCode originalOpCode = instr->m_opcode;
    if (!this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 12344\n");
        // No re-write on prepass
        instr->m_opcode = opcode;
    }

    Value *src1ValueToSpecialize = src1Val, *src2ValueToSpecialize = src2Val;
    // Lossy conversions to int32 must be done based on the original source values. For instance, if one of the values is a
    // float constant with a value that fits in a uint32 but not an int32, and the instruction can ignore int overflow, the
    // source value for the purposes of int specialization would have been changed to an int constant value by ignoring
    // overflow. If we were to specialize the sym using the int constant value, it would be treated as a lossless
    // conversion, but since there may be subsequent uses of the same float constant value that may not ignore overflow,
    // this must be treated as a lossy conversion by specializing the sym using the original float constant value.
    if(src1Lossy)
    {LOGMEIN("GlobOpt.cpp] 12357\n");
        src1ValueToSpecialize = src1OriginalVal;
    }
    if (src2Lossy)
    {LOGMEIN("GlobOpt.cpp] 12361\n");
        src2ValueToSpecialize = src2OriginalVal;
    }

    // Make sure the srcs are specialized
    src1 = instr->GetSrc1();
    this->ToInt32(instr, src1, this->currentBlock, src1ValueToSpecialize, nullptr, src1Lossy);

    if (!skipSrc2)
    {LOGMEIN("GlobOpt.cpp] 12370\n");
        src2 = instr->GetSrc2();
        this->ToInt32(instr, src2, this->currentBlock, src2ValueToSpecialize, nullptr, src2Lossy);
    }

    if(bailOutKind != IR::BailOutInvalid && !this->IsLoopPrePass())
    {
        GenerateBailAtOperation(&instr, bailOutKind);
    }

    if (!skipDst && instr->GetDst())
    {LOGMEIN("GlobOpt.cpp] 12381\n");
        if (needsBoolConv)
        {LOGMEIN("GlobOpt.cpp] 12383\n");
            IR::RegOpnd *varDst;
            if (this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 12386\n");
                varDst = instr->GetDst()->AsRegOpnd();
                this->ToVarRegOpnd(varDst, this->currentBlock);
            }
            else
            {
                // Generate:
                // t1.i = CmCC t2.i, t3.i
                // t1.v = Conv_bool t1.i
                //
                // If the only uses of t1 are ints, the conv_bool will get dead-stored

                TypeSpecializeIntDst(instr, originalOpCode, nullptr, src1Val, src2Val, bailOutKind, newMin, newMax, pDstVal);
                IR::RegOpnd *intDst = instr->GetDst()->AsRegOpnd();
                intDst->SetIsJITOptimizedReg(true);

                varDst = IR::RegOpnd::New(intDst->m_sym->GetVarEquivSym(this->func), TyVar, this->func);
                IR::Instr *convBoolInstr = IR::Instr::New(Js::OpCode::Conv_Bool, varDst, intDst, this->func);
                // In some cases (e.g. unsigned compare peep code), a comparison will use variables
                // other than the ones initially intended for it, if we can determine that we would
                // arrive at the same result. This means that we get a ByteCodeUses operation after
                // the actual comparison. Since Inserting the Conv_bool just after the compare, and
                // just before the ByteCodeUses, would cause issues later on with register lifetime
                // calculation, we want to insert the Conv_bool after the whole compare instruction
                // block.
                IR::Instr *putAfter = instr;
                while (putAfter->m_next && putAfter->m_next->m_opcode == Js::OpCode::ByteCodeUses)
                {LOGMEIN("GlobOpt.cpp] 12413\n");
                    putAfter = putAfter->m_next;
                }
                putAfter->InsertAfter(convBoolInstr);
                convBoolInstr->SetByteCodeOffset(instr);

                this->ToVarRegOpnd(varDst, this->currentBlock);
                this->blockData.liveInt32Syms->Set(varDst->m_sym->m_id);
                this->blockData.liveLossyInt32Syms->Set(varDst->m_sym->m_id);
            }
            *pDstVal = this->NewGenericValue(ValueType::Boolean, varDst);
        }
        else
        {
            TypeSpecializeIntDst(
                instr,
                originalOpCode,
                nullptr,
                src1Val,
                src2Val,
                bailOutKind,
                newMin,
                newMax,
                pDstVal,
                addSubConstantInfo.HasInfo() ? &addSubConstantInfo : nullptr);
        }
    }

    if(bailOutKind == IR::BailOutInvalid)
    {LOGMEIN("GlobOpt.cpp] 12442\n");
        GOPT_TRACE(_u("Type specialized to INT\n"));
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::AggressiveIntTypeSpecPhase))
        {LOGMEIN("GlobOpt.cpp] 12446\n");
            Output::Print(_u("Type specialized to INT: "));
            Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
        }
#endif
    }
    else
    {
        GOPT_TRACE(_u("Type specialized to INT with bailout on:\n"));
        if(bailOutKind & (IR::BailOutOnOverflow | IR::BailOutOnMulOverflow) )
        {LOGMEIN("GlobOpt.cpp] 12456\n");
            GOPT_TRACE(_u("    Overflow\n"));
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::AggressiveIntTypeSpecPhase))
            {LOGMEIN("GlobOpt.cpp] 12460\n");
                Output::Print(_u("Type specialized to INT with bailout (%S): "), "Overflow");
                Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
            }
#endif
        }
        if(bailOutKind & IR::BailOutOnNegativeZero)
        {LOGMEIN("GlobOpt.cpp] 12467\n");
            GOPT_TRACE(_u("    Zero\n"));
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::AggressiveIntTypeSpecPhase))
            {LOGMEIN("GlobOpt.cpp] 12471\n");
                Output::Print(_u("Type specialized to INT with bailout (%S): "), "Zero");
                Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
            }
#endif
        }
    }

    return true;
}

bool
GlobOpt::IsWorthSpecializingToInt32Branch(IR::Instr * instr, Value * src1Val, Value * src2Val)
{LOGMEIN("GlobOpt.cpp] 12484\n");
    if (!src1Val->GetValueInfo()->HasIntConstantValue() && instr->GetSrc1()->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 12486\n");
        StackSym *sym1 = instr->GetSrc1()->AsRegOpnd()->m_sym;
        if (this->IsInt32TypeSpecialized(sym1, this->currentBlock) == false)
        {LOGMEIN("GlobOpt.cpp] 12489\n");
            if (!src2Val->GetValueInfo()->HasIntConstantValue() && instr->GetSrc2()->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 12491\n");
                StackSym *sym2 = instr->GetSrc2()->AsRegOpnd()->m_sym;
                if (this->IsInt32TypeSpecialized(sym2, this->currentBlock) == false)
                {LOGMEIN("GlobOpt.cpp] 12494\n");
                    // Type specializing a Br itself isn't worth it, unless one src
                    // is already type specialized
                    return false;
                }
            }
        }
    }
    return true;
}

bool
GlobOpt::TryOptConstFoldBrFalse(
    IR::Instr *const instr,
    Value *const srcValue,
    const int32 min,
    const int32 max)
{LOGMEIN("GlobOpt.cpp] 12511\n");
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::BrFalse_A || instr->m_opcode == Js::OpCode::BrTrue_A);
    Assert(srcValue);

    if(!(DoAggressiveIntTypeSpec() ? srcValue->GetValueInfo()->IsLikelyInt() : srcValue->GetValueInfo()->IsInt()))
    {LOGMEIN("GlobOpt.cpp] 12517\n");
        return false;
    }
    if(ValueInfo::IsEqualTo(srcValue, min, max, nullptr, 0, 0))
    {LOGMEIN("GlobOpt.cpp] 12521\n");
        OptConstFoldBr(instr->m_opcode == Js::OpCode::BrFalse_A, instr, srcValue);
        return true;
    }
    if(ValueInfo::IsNotEqualTo(srcValue, min, max, nullptr, 0, 0))
    {LOGMEIN("GlobOpt.cpp] 12526\n");
        OptConstFoldBr(instr->m_opcode == Js::OpCode::BrTrue_A, instr, srcValue);
        return true;
    }
    return false;
}

bool
GlobOpt::TryOptConstFoldBrEqual(
    IR::Instr *const instr,
    const bool branchOnEqual,
    Value *const src1Value,
    const int32 min1,
    const int32 max1,
    Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOpt.cpp] 12543\n");
    Assert(instr);
    Assert(src1Value);
    Assert(DoAggressiveIntTypeSpec() ? src1Value->GetValueInfo()->IsLikelyInt() : src1Value->GetValueInfo()->IsInt());
    Assert(src2Value);
    Assert(DoAggressiveIntTypeSpec() ? src2Value->GetValueInfo()->IsLikelyInt() : src2Value->GetValueInfo()->IsInt());

    if(ValueInfo::IsEqualTo(src1Value, min1, max1, src2Value, min2, max2))
    {
        OptConstFoldBr(branchOnEqual, instr, src1Value, src2Value);
        return true;
    }
    if(ValueInfo::IsNotEqualTo(src1Value, min1, max1, src2Value, min2, max2))
    {
        OptConstFoldBr(!branchOnEqual, instr, src1Value, src2Value);
        return true;
    }
    return false;
}

bool
GlobOpt::TryOptConstFoldBrGreaterThan(
    IR::Instr *const instr,
    const bool branchOnGreaterThan,
    Value *const src1Value,
    const int32 min1,
    const int32 max1,
    Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOpt.cpp] 12573\n");
    Assert(instr);
    Assert(src1Value);
    Assert(DoAggressiveIntTypeSpec() ? src1Value->GetValueInfo()->IsLikelyInt() : src1Value->GetValueInfo()->IsInt());
    Assert(src2Value);
    Assert(DoAggressiveIntTypeSpec() ? src2Value->GetValueInfo()->IsLikelyInt() : src2Value->GetValueInfo()->IsInt());

    if(ValueInfo::IsGreaterThan(src1Value, min1, max1, src2Value, min2, max2))
    {
        OptConstFoldBr(branchOnGreaterThan, instr, src1Value, src2Value);
        return true;
    }
    if(ValueInfo::IsLessThanOrEqualTo(src1Value, min1, max1, src2Value, min2, max2))
    {
        OptConstFoldBr(!branchOnGreaterThan, instr, src1Value, src2Value);
        return true;
    }
    return false;
}

bool
GlobOpt::TryOptConstFoldBrGreaterThanOrEqual(
    IR::Instr *const instr,
    const bool branchOnGreaterThanOrEqual,
    Value *const src1Value,
    const int32 min1,
    const int32 max1,
    Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOpt.cpp] 12603\n");
    Assert(instr);
    Assert(src1Value);
    Assert(DoAggressiveIntTypeSpec() ? src1Value->GetValueInfo()->IsLikelyInt() : src1Value->GetValueInfo()->IsInt());
    Assert(src2Value);
    Assert(DoAggressiveIntTypeSpec() ? src2Value->GetValueInfo()->IsLikelyInt() : src2Value->GetValueInfo()->IsInt());

    if(ValueInfo::IsGreaterThanOrEqualTo(src1Value, min1, max1, src2Value, min2, max2))
    {
        OptConstFoldBr(branchOnGreaterThanOrEqual, instr, src1Value, src2Value);
        return true;
    }
    if(ValueInfo::IsLessThan(src1Value, min1, max1, src2Value, min2, max2))
    {
        OptConstFoldBr(!branchOnGreaterThanOrEqual, instr, src1Value, src2Value);
        return true;
    }
    return false;
}

bool
GlobOpt::TryOptConstFoldBrUnsignedLessThan(
    IR::Instr *const instr,
    const bool branchOnLessThan,
    Value *const src1Value,
    const int32 min1,
    const int32 max1,
    Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOpt.cpp] 12633\n");
    Assert(DoConstFold());
    Assert(!IsLoopPrePass());

    if(!src1Value ||
        !src2Value ||
        !(
            DoAggressiveIntTypeSpec()
                ? src1Value->GetValueInfo()->IsLikelyInt() && src2Value->GetValueInfo()->IsLikelyInt()
                : src1Value->GetValueInfo()->IsInt() && src2Value->GetValueInfo()->IsInt()
        ))
    {LOGMEIN("GlobOpt.cpp] 12644\n");
        return false;
    }

    uint uMin1 = (min1 < 0 ? (max1 < 0 ? min((uint)min1, (uint)max1) : 0) : min1);
    uint uMax1 = max((uint)min1, (uint)max1);
    uint uMin2 = (min2 < 0 ? (max2 < 0 ? min((uint)min2, (uint)max2) : 0) : min2);
    uint uMax2 = max((uint)min2, (uint)max2);

    if (uMax1 < uMin2)
    {
        // Range 1 is always lesser than Range 2
        OptConstFoldBr(branchOnLessThan, instr, src1Value, src2Value);
        return true;
    }
    if (uMin1 >= uMax2)
    {
        // Range 2 is always lesser than Range 1
        OptConstFoldBr(!branchOnLessThan, instr, src1Value, src2Value);
        return true;
    }
    return false;
}

bool
GlobOpt::TryOptConstFoldBrUnsignedGreaterThan(
    IR::Instr *const instr,
    const bool branchOnGreaterThan,
    Value *const src1Value,
    const int32 min1,
    const int32 max1,
    Value *const src2Value,
    const int32 min2,
    const int32 max2)
{LOGMEIN("GlobOpt.cpp] 12678\n");
    Assert(DoConstFold());
    Assert(!IsLoopPrePass());

    if(!src1Value ||
        !src2Value ||
        !(
            DoAggressiveIntTypeSpec()
                ? src1Value->GetValueInfo()->IsLikelyInt() && src2Value->GetValueInfo()->IsLikelyInt()
                : src1Value->GetValueInfo()->IsInt() && src2Value->GetValueInfo()->IsInt()
        ))
    {LOGMEIN("GlobOpt.cpp] 12689\n");
        return false;
    }

    uint uMin1 = (min1 < 0 ? (max1 < 0 ? min((uint)min1, (uint)max1) : 0) : min1);
    uint uMax1 = max((uint)min1, (uint)max1);
    uint uMin2 = (min2 < 0 ? (max2 < 0 ? min((uint)min2, (uint)max2) : 0) : min2);
    uint uMax2 = max((uint)min2, (uint)max2);

    if (uMin1 > uMax2)
    {
        // Range 1 is always greater than Range 2
        OptConstFoldBr(branchOnGreaterThan, instr, src1Value, src2Value);
        return true;
    }
    if (uMax1 <= uMin2)
    {
        // Range 2 is always greater than Range 1
        OptConstFoldBr(!branchOnGreaterThan, instr, src1Value, src2Value);
        return true;
    }
    return false;
}

void
GlobOpt::SetPathDependentInfo(const bool conditionToBranch, const PathDependentInfo &info)
{LOGMEIN("GlobOpt.cpp] 12715\n");
    Assert(this->currentBlock->GetSuccList()->Count() == 2);
    IR::Instr * fallthrough = this->currentBlock->GetNext()->GetFirstInstr();
    FOREACH_SLISTBASECOUNTED_ENTRY(FlowEdge*, edge, this->currentBlock->GetSuccList())
    {LOGMEIN("GlobOpt.cpp] 12719\n");
        if (conditionToBranch == (edge->GetSucc()->GetFirstInstr() != fallthrough))
        {LOGMEIN("GlobOpt.cpp] 12721\n");
            edge->SetPathDependentInfo(info, alloc);
            return;
        }
    }
    NEXT_SLISTBASECOUNTED_ENTRY;

    Assert(false);
}

PathDependentInfoToRestore
GlobOpt::UpdatePathDependentInfo(PathDependentInfo *const info)
{LOGMEIN("GlobOpt.cpp] 12733\n");
    Assert(info);

    if(!info->HasInfo())
    {LOGMEIN("GlobOpt.cpp] 12737\n");
        return PathDependentInfoToRestore();
    }

    decltype(&GlobOpt::UpdateIntBoundsForEqual) UpdateIntBoundsForLeftValue, UpdateIntBoundsForRightValue;
    switch(info->Relationship())
    {LOGMEIN("GlobOpt.cpp] 12743\n");
        case PathDependentRelationship::Equal:
            UpdateIntBoundsForLeftValue = &GlobOpt::UpdateIntBoundsForEqual;
            UpdateIntBoundsForRightValue = &GlobOpt::UpdateIntBoundsForEqual;
            break;

        case PathDependentRelationship::NotEqual:
            UpdateIntBoundsForLeftValue = &GlobOpt::UpdateIntBoundsForNotEqual;
            UpdateIntBoundsForRightValue = &GlobOpt::UpdateIntBoundsForNotEqual;
            break;

        case PathDependentRelationship::GreaterThanOrEqual:
            UpdateIntBoundsForLeftValue = &GlobOpt::UpdateIntBoundsForGreaterThanOrEqual;
            UpdateIntBoundsForRightValue = &GlobOpt::UpdateIntBoundsForLessThanOrEqual;
            break;

        case PathDependentRelationship::GreaterThan:
            UpdateIntBoundsForLeftValue = &GlobOpt::UpdateIntBoundsForGreaterThan;
            UpdateIntBoundsForRightValue = &GlobOpt::UpdateIntBoundsForLessThan;
            break;

        case PathDependentRelationship::LessThanOrEqual:
            UpdateIntBoundsForLeftValue = &GlobOpt::UpdateIntBoundsForLessThanOrEqual;
            UpdateIntBoundsForRightValue = &GlobOpt::UpdateIntBoundsForGreaterThanOrEqual;
            break;

        case PathDependentRelationship::LessThan:
            UpdateIntBoundsForLeftValue = &GlobOpt::UpdateIntBoundsForLessThan;
            UpdateIntBoundsForRightValue = &GlobOpt::UpdateIntBoundsForGreaterThan;
            break;

        default:
            Assert(false);
            __assume(false);
    }

    ValueInfo *leftValueInfo = info->LeftValue()->GetValueInfo();
    IntConstantBounds leftConstantBounds;
    AssertVerify(leftValueInfo->TryGetIntConstantBounds(&leftConstantBounds, true));

    ValueInfo *rightValueInfo;
    IntConstantBounds rightConstantBounds;
    if(info->RightValue())
    {LOGMEIN("GlobOpt.cpp] 12786\n");
        rightValueInfo = info->RightValue()->GetValueInfo();
        AssertVerify(rightValueInfo->TryGetIntConstantBounds(&rightConstantBounds, true));
    }
    else
    {
        rightValueInfo = nullptr;
        rightConstantBounds = IntConstantBounds(info->RightConstantValue(), info->RightConstantValue());
    }

    ValueInfo *const newLeftValueInfo =
        (this->*UpdateIntBoundsForLeftValue)(
            info->LeftValue(),
            leftConstantBounds,
            info->RightValue(),
            rightConstantBounds,
            true);
    if(newLeftValueInfo)
    {
        ChangeValueInfo(nullptr, info->LeftValue(), newLeftValueInfo);
        AssertVerify(newLeftValueInfo->TryGetIntConstantBounds(&leftConstantBounds, true));
    }
    else
    {
        leftValueInfo = nullptr;
    }

    ValueInfo *const newRightValueInfo =
        (this->*UpdateIntBoundsForRightValue)(
            info->RightValue(),
            rightConstantBounds,
            info->LeftValue(),
            leftConstantBounds,
            true);
    if(newRightValueInfo)
    {
        ChangeValueInfo(nullptr, info->RightValue(), newRightValueInfo);
    }
    else
    {
        rightValueInfo = nullptr;
    }

    return PathDependentInfoToRestore(leftValueInfo, rightValueInfo);
}

void
GlobOpt::RestorePathDependentInfo(PathDependentInfo *const info, const PathDependentInfoToRestore infoToRestore)
{LOGMEIN("GlobOpt.cpp] 12834\n");
    Assert(info);

    if(infoToRestore.LeftValueInfo())
    {LOGMEIN("GlobOpt.cpp] 12838\n");
        Assert(info->LeftValue());
        ChangeValueInfo(nullptr, info->LeftValue(), infoToRestore.LeftValueInfo());
    }

    if(infoToRestore.RightValueInfo())
    {LOGMEIN("GlobOpt.cpp] 12844\n");
        Assert(info->RightValue());
        ChangeValueInfo(nullptr, info->RightValue(), infoToRestore.RightValueInfo());
    }
}

bool
GlobOpt::TypeSpecializeFloatUnary(IR::Instr **pInstr, Value *src1Val, Value **pDstVal, bool skipDst /* = false */)
{LOGMEIN("GlobOpt.cpp] 12852\n");
    IR::Instr *&instr = *pInstr;
    IR::Opnd *src1;
    IR::Opnd *dst;
    Js::OpCode opcode = instr->m_opcode;
    Value *valueToTransfer = nullptr;

    Assert(src1Val && src1Val->GetValueInfo()->IsLikelyNumber() || OpCodeAttr::IsInlineBuiltIn(instr->m_opcode));

    if (!this->DoFloatTypeSpec())
    {LOGMEIN("GlobOpt.cpp] 12862\n");
        return false;
    }

    // For inline built-ins we need to do type specialization. Check upfront to avoid duplicating same case labels.
    if (!OpCodeAttr::IsInlineBuiltIn(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 12868\n");
        switch (opcode)
        {LOGMEIN("GlobOpt.cpp] 12870\n");
        case Js::OpCode::ArgOut_A_InlineBuiltIn:
            skipDst = true;
            // fall-through

        case Js::OpCode::Ld_A:
        case Js::OpCode::BrTrue_A:
        case Js::OpCode::BrFalse_A:
            if (instr->GetSrc1()->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 12879\n");
                StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
                if (this->IsFloat64TypeSpecialized(sym, this->currentBlock) == false)
                {LOGMEIN("GlobOpt.cpp] 12882\n");
                    // Type specializing an Ld_A isn't worth it, unless the src
                    // is already type specialized
                    return false;
                }
            }
            if (instr->m_opcode == Js::OpCode::Ld_A)
            {LOGMEIN("GlobOpt.cpp] 12889\n");
                valueToTransfer = src1Val;
            }
            break;
        case Js::OpCode::Neg_A:
            break;

        case Js::OpCode::Conv_Num:
            Assert(src1Val);
            opcode = Js::OpCode::Ld_A;
            valueToTransfer = src1Val;
            if (!src1Val->GetValueInfo()->IsNumber())
            {LOGMEIN("GlobOpt.cpp] 12901\n");
                StackSym *sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
                valueToTransfer = NewGenericValue(ValueType::Float, instr->GetDst()->GetStackSym());
                if (this->IsFloat64TypeSpecialized(sym, this->currentBlock) == false)
                {LOGMEIN("GlobOpt.cpp] 12905\n");
                    // Set the dst as a nonDeadStore. We want to keep the Ld_A to prevent the FromVar from
                    // being dead-stored, as it could cause implicit calls.
                    dst = instr->GetDst();
                    dst->AsRegOpnd()->m_dontDeadStore = true;
                }
            }
            break;

        case Js::OpCode::StElemI_A:
        case Js::OpCode::StElemI_A_Strict:
        case Js::OpCode::StElemC:
            return TypeSpecializeStElem(pInstr, src1Val, pDstVal);

        default:
            return false;
        }
    }

    // Make sure the srcs are specialized
    src1 = instr->GetSrc1();

    // Use original val when calling toFloat64 as this is what we'll use to try hoisting the fromVar if we're in a loop.
    this->ToFloat64(instr, src1, this->currentBlock, src1Val, nullptr, IR::BailOutPrimitiveButString);

    if (!skipDst)
    {LOGMEIN("GlobOpt.cpp] 12931\n");
        dst = instr->GetDst();
        if (dst)
        {LOGMEIN("GlobOpt.cpp] 12934\n");
            this->TypeSpecializeFloatDst(instr, valueToTransfer, src1Val, nullptr, pDstVal);
            if (!this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 12937\n");
                instr->m_opcode = opcode;
            }
        }
    }

    GOPT_TRACE_INSTR(instr, _u("Type specialized to FLOAT: "));

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::FloatTypeSpecPhase))
    {LOGMEIN("GlobOpt.cpp] 12947\n");
        Output::Print(_u("Type specialized to FLOAT: "));
        Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
    }
#endif

    return true;
}

// Unconditionally type-spec dst to float.
void
GlobOpt::TypeSpecializeFloatDst(IR::Instr *instr, Value *valToTransfer, Value *const src1Value, Value *const src2Value, Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 12959\n");
    IR::Opnd* dst = instr->GetDst();
    Assert(dst);

    AssertMsg(dst->IsRegOpnd(), "What else?");
    this->ToFloat64Dst(instr, dst->AsRegOpnd(), this->currentBlock);

    if(valToTransfer)
    {LOGMEIN("GlobOpt.cpp] 12967\n");
        *pDstVal = this->ValueNumberTransferDst(instr, valToTransfer);
        InsertNewValue(*pDstVal, dst);
    }
    else
    {
        *pDstVal = CreateDstUntransferredValue(ValueType::Float, instr, src1Value, src2Value);
    }
}

void
GlobOpt::TypeSpecializeSimd128Dst(IRType type, IR::Instr *instr, Value *valToTransfer, Value *const src1Value, Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 12979\n");
    IR::Opnd* dst = instr->GetDst();
    Assert(dst);

    AssertMsg(dst->IsRegOpnd(), "What else?");
    this->ToSimd128Dst(type, instr, dst->AsRegOpnd(), this->currentBlock);

    if (valToTransfer)
    {LOGMEIN("GlobOpt.cpp] 12987\n");
        *pDstVal = this->ValueNumberTransferDst(instr, valToTransfer);
        InsertNewValue(*pDstVal, dst);
    }
    else
    {
        *pDstVal = NewGenericValue(GetValueTypeFromIRType(type), instr->GetDst());
    }
}

bool
GlobOpt::TypeSpecializeLdLen(
    IR::Instr * *const instrRef,
    Value * *const src1ValueRef,
    Value * *const dstValueRef,
    bool *const forceInvariantHoistingRef)
{LOGMEIN("GlobOpt.cpp] 13003\n");
    Assert(instrRef);
    IR::Instr *&instr = *instrRef;
    Assert(instr);
    Assert(instr->m_opcode == Js::OpCode::LdLen_A);
    Assert(src1ValueRef);
    Value *&src1Value = *src1ValueRef;
    Assert(dstValueRef);
    Value *&dstValue = *dstValueRef;
    Assert(forceInvariantHoistingRef);
    bool &forceInvariantHoisting = *forceInvariantHoistingRef;

    if(!DoLdLenIntSpec(instr, instr->GetSrc1()->GetValueType()))
    {LOGMEIN("GlobOpt.cpp] 13016\n");
        return false;
    }

    IR::BailOutKind bailOutKind = IR::BailOutOnIrregularLength;
    if(!IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 13022\n");
        IR::RegOpnd *const baseOpnd = instr->GetSrc1()->AsRegOpnd();
        if(baseOpnd->IsArrayRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 13025\n");
            StackSym *const lengthSym = baseOpnd->AsArrayRegOpnd()->LengthSym();
            if(lengthSym)
            {LOGMEIN("GlobOpt.cpp] 13028\n");
                CaptureByteCodeSymUses(instr);
                instr->m_opcode = Js::OpCode::Ld_I4;
                instr->ReplaceSrc1(IR::RegOpnd::New(lengthSym, lengthSym->GetType(), func));
                instr->ClearBailOutInfo();

                // Find the hoisted length value
                Value *const lengthValue = FindValue(lengthSym);
                Assert(lengthValue);
                src1Value = lengthValue;
                ValueInfo *const lengthValueInfo = lengthValue->GetValueInfo();
                Assert(lengthValueInfo->GetSymStore() != lengthSym);
                IntConstantBounds lengthConstantBounds;
                AssertVerify(lengthValueInfo->TryGetIntConstantBounds(&lengthConstantBounds));
                Assert(lengthConstantBounds.LowerBound() >= 0);

                // Int-specialize, and transfer the value to the dst
                TypeSpecializeIntDst(
                    instr,
                    Js::OpCode::LdLen_A,
                    src1Value,
                    src1Value,
                    nullptr,
                    bailOutKind,
                    lengthConstantBounds.LowerBound(),
                    lengthConstantBounds.UpperBound(),
                    &dstValue);

                // Try to force hoisting the Ld_I4 so that the length will have an invariant sym store that can be
                // copy-propped. Invariant hoisting does not automatically hoist Ld_I4.
                forceInvariantHoisting = true;
                return true;
            }
        }

        if (instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 13064\n");
            Assert(instr->GetBailOutKind() == IR::BailOutMarkTempObject);
            bailOutKind = IR::BailOutOnIrregularLength | IR::BailOutMarkTempObject;
            instr->SetBailOutKind(bailOutKind);
        }
        else
        {
            Assert(bailOutKind == IR::BailOutOnIrregularLength);
            GenerateBailAtOperation(&instr, bailOutKind);
        }
    }

    TypeSpecializeIntDst(
        instr,
        Js::OpCode::LdLen_A,
        nullptr,
        nullptr,
        nullptr,
        bailOutKind,
        0,
        INT32_MAX,
        &dstValue);
    return true;
}

bool
GlobOpt::TypeSpecializeFloatBinary(IR::Instr *instr, Value *src1Val, Value *src2Val, Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 13091\n");
    IR::Opnd *src1;
    IR::Opnd *src2;
    IR::Opnd *dst;
    bool allowUndefinedOrNullSrc1 = true;
    bool allowUndefinedOrNullSrc2 = true;

    bool skipSrc1 = false;
    bool skipSrc2 = false;
    bool skipDst = false;

    if (!this->DoFloatTypeSpec())
    {LOGMEIN("GlobOpt.cpp] 13103\n");
        return false;
    }

    // For inline built-ins we need to do type specialization. Check upfront to avoid duplicating same case labels.
    if (!OpCodeAttr::IsInlineBuiltIn(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 13109\n");
        switch (instr->m_opcode)
        {LOGMEIN("GlobOpt.cpp] 13111\n");
        case Js::OpCode::Sub_A:
        case Js::OpCode::Mul_A:
        case Js::OpCode::Div_A:
        case Js::OpCode::Expo_A:
            // Avoid if one source is known not to be a number.
            if (src1Val->GetValueInfo()->IsNotNumber() || src2Val->GetValueInfo()->IsNotNumber())
            {LOGMEIN("GlobOpt.cpp] 13118\n");
                return false;
            }
            break;

        case Js::OpCode::BrSrEq_A:
        case Js::OpCode::BrSrNeq_A:
        case Js::OpCode::BrEq_A:
        case Js::OpCode::BrNeq_A:
        case Js::OpCode::BrSrNotEq_A:
        case Js::OpCode::BrNotEq_A:
        case Js::OpCode::BrSrNotNeq_A:
        case Js::OpCode::BrNotNeq_A:
            // Avoid if one source is known not to be a number.
            if (src1Val->GetValueInfo()->IsNotNumber() || src2Val->GetValueInfo()->IsNotNumber())
            {LOGMEIN("GlobOpt.cpp] 13133\n");
                return false;
            }
            // Undef == Undef, but +Undef != +Undef
            // 0.0 != null, but 0.0 == +null
            //
            // So Bailout on anything but numbers for both src1 and src2
            allowUndefinedOrNullSrc1 = false;
            allowUndefinedOrNullSrc2 = false;
            break;

        case Js::OpCode::BrGt_A:
        case Js::OpCode::BrGe_A:
        case Js::OpCode::BrLt_A:
        case Js::OpCode::BrLe_A:
        case Js::OpCode::BrNotGt_A:
        case Js::OpCode::BrNotGe_A:
        case Js::OpCode::BrNotLt_A:
        case Js::OpCode::BrNotLe_A:
            // Avoid if one source is known not to be a number.
            if (src1Val->GetValueInfo()->IsNotNumber() || src2Val->GetValueInfo()->IsNotNumber())
            {LOGMEIN("GlobOpt.cpp] 13154\n");
                return false;
            }
            break;

        case Js::OpCode::Add_A:
            // For Add, we need both sources to be Numbers, otherwise it could be a string concat
            if (!src1Val || !src2Val || !(src1Val->GetValueInfo()->IsLikelyNumber() && src2Val->GetValueInfo()->IsLikelyNumber()))
            {LOGMEIN("GlobOpt.cpp] 13162\n");
                return false;
            }
            break;

        case Js::OpCode::ArgOut_A_InlineBuiltIn:
            skipSrc2 = true;
            skipDst = true;
            break;

        default:
            return false;
        }
    }
    else
    {
        switch (instr->m_opcode)
        {LOGMEIN("GlobOpt.cpp] 13179\n");
            case Js::OpCode::InlineArrayPush:
                bool isFloatConstMissingItem = src2Val->GetValueInfo()->IsFloatConstant();

                if(isFloatConstMissingItem)
                {LOGMEIN("GlobOpt.cpp] 13184\n");
                    FloatConstType floatValue = src2Val->GetValueInfo()->AsFloatConstant()->FloatValue();
                    isFloatConstMissingItem = Js::SparseArraySegment<double>::IsMissingItem(&floatValue);
                }
                // Don't specialize if the element is not likelyNumber - we will surely bailout
                if(!(src2Val->GetValueInfo()->IsLikelyNumber()) || isFloatConstMissingItem)
                {LOGMEIN("GlobOpt.cpp] 13190\n");
                    return false;
                }
                // Only specialize the Second source - element
                skipSrc1 = true;
                skipDst = true;
                allowUndefinedOrNullSrc2 = false;
                break;
        }
    }

    // Make sure the srcs are specialized
    if(!skipSrc1)
    {LOGMEIN("GlobOpt.cpp] 13203\n");
        src1 = instr->GetSrc1();
        this->ToFloat64(instr, src1, this->currentBlock, src1Val, nullptr, (allowUndefinedOrNullSrc1 ? IR::BailOutPrimitiveButString : IR::BailOutNumberOnly));
    }

    if (!skipSrc2)
    {LOGMEIN("GlobOpt.cpp] 13209\n");
        src2 = instr->GetSrc2();
        this->ToFloat64(instr, src2, this->currentBlock, src2Val, nullptr, (allowUndefinedOrNullSrc2 ? IR::BailOutPrimitiveButString : IR::BailOutNumberOnly));
    }

    if (!skipDst)
    {LOGMEIN("GlobOpt.cpp] 13215\n");
        dst = instr->GetDst();

        if (dst)
        {LOGMEIN("GlobOpt.cpp] 13219\n");
            *pDstVal = CreateDstUntransferredValue(ValueType::Float, instr, src1Val, src2Val);

            AssertMsg(dst->IsRegOpnd(), "What else?");
            this->ToFloat64Dst(instr, dst->AsRegOpnd(), this->currentBlock);
        }
    }

    GOPT_TRACE_INSTR(instr, _u("Type specialized to FLOAT: "));

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::FloatTypeSpecPhase))
    {LOGMEIN("GlobOpt.cpp] 13231\n");
        Output::Print(_u("Type specialized to FLOAT: "));
        Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
    }
#endif

    return true;
}

bool
GlobOpt::TypeSpecializeStElem(IR::Instr ** pInstr, Value *src1Val, Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 13242\n");
    IR::Instr *&instr = *pInstr;

    IR::RegOpnd *baseOpnd = instr->GetDst()->AsIndirOpnd()->GetBaseOpnd();
    ValueType baseValueType(baseOpnd->GetValueType());
    if (instr->DoStackArgsOpt(this->func) ||
        (!this->DoTypedArrayTypeSpec() && baseValueType.IsLikelyOptimizedTypedArray()) ||
        (!this->DoNativeArrayTypeSpec() && baseValueType.IsLikelyNativeArray()) ||
        !(baseValueType.IsLikelyOptimizedTypedArray() || baseValueType.IsLikelyNativeArray()))
    {
        GOPT_TRACE_INSTR(instr, _u("Didn't type specialize array access, because typed array type specialization is disabled, or base is not an optimized typed array.\n"));
        if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 13254\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            baseValueType.ToString(baseValueTypeStr);
            Output::Print(_u("Typed Array Optimization:  function: %s (%s): instr: %s, base value type: %S, did not specialize because %s.\n"),
                this->func->GetJITFunctionBody()->GetDisplayName(),
                this->func->GetDebugNumberSet(debugStringBuffer),
                Js::OpCodeUtil::GetOpCodeName(instr->m_opcode),
                baseValueTypeStr,
                instr->DoStackArgsOpt(this->func) ?
                    _u("instruction uses the arguments object") :
                    _u("typed array type specialization is disabled, or base is not an optimized typed array"));
            Output::Flush();
        }
        return false;
    }

    Assert(instr->GetSrc1()->IsRegOpnd() || (src1Val && src1Val->GetValueInfo()->HasIntConstantValue()));

    StackSym *sym = instr->GetSrc1()->IsRegOpnd() ? instr->GetSrc1()->AsRegOpnd()->m_sym : nullptr;

    // Only type specialize the source of store element if the source symbol is already type specialized to int or float.
    if (sym)
    {LOGMEIN("GlobOpt.cpp] 13277\n");
        if (baseValueType.IsLikelyNativeArray())
        {LOGMEIN("GlobOpt.cpp] 13279\n");
            // Gently coerce these src's into native if it seems likely to work.
            // Otherwise we can't use the fast path to store.
            // But don't try to put a float-specialized number into an int array this way.
            if (!(
                    this->IsInt32TypeSpecialized(sym, this->currentBlock) ||
                    (
                        src1Val &&
                        (
                            DoAggressiveIntTypeSpec()
                                ? src1Val->GetValueInfo()->IsLikelyInt()
                                : src1Val->GetValueInfo()->IsInt()
                        )
                    )
                ))
            {LOGMEIN("GlobOpt.cpp] 13294\n");
                if (!(
                        this->IsFloat64TypeSpecialized(sym, this->currentBlock) ||
                        (src1Val && src1Val->GetValueInfo()->IsLikelyNumber())
                    ) ||
                    baseValueType.HasIntElements())
                {LOGMEIN("GlobOpt.cpp] 13300\n");
                    return false;
                }
            }
        }
        else if (!this->IsInt32TypeSpecialized(sym, this->currentBlock) && !this->IsFloat64TypeSpecialized(sym, this->currentBlock))
        {
            GOPT_TRACE_INSTR(instr, _u("Didn't specialize array access, because src is not type specialized.\n"));
            if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->func))
            {LOGMEIN("GlobOpt.cpp] 13309\n");
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
                baseValueType.ToString(baseValueTypeStr);
                Output::Print(_u("Typed Array Optimization:  function: %s (%s): instr: %s, base value type: %S, did not specialize because src is not specialized.\n"),
                              this->func->GetJITFunctionBody()->GetDisplayName(),
                              this->func->GetDebugNumberSet(debugStringBuffer),
                              Js::OpCodeUtil::GetOpCodeName(instr->m_opcode),
                              baseValueTypeStr);
                Output::Flush();
            }

            return false;
        }
    }

    int32 src1IntConstantValue;
    if(baseValueType.IsLikelyNativeIntArray() && src1Val && src1Val->GetValueInfo()->TryGetIntConstantValue(&src1IntConstantValue))
    {LOGMEIN("GlobOpt.cpp] 13327\n");
        if(Js::SparseArraySegment<int32>::IsMissingItem(&src1IntConstantValue))
        {LOGMEIN("GlobOpt.cpp] 13329\n");
            return false;
        }
    }

    // Note: doing ToVarUses to make sure we do get the int32 version of the index before trying to access its value in
    // ShouldExpectConventionalArrayIndexValue. Not sure why that never gave us a problem before.
    Assert(instr->GetDst()->IsIndirOpnd());
    IR::IndirOpnd *dst = instr->GetDst()->AsIndirOpnd();

    // Make sure we use the int32 version of the index operand symbol, if available.  Otherwise, ensure the var symbol is live (by
    // potentially inserting a ToVar).
    this->ToVarUses(instr, dst, /* isDst = */ true, nullptr);

    if (!ShouldExpectConventionalArrayIndexValue(dst))
    {
        GOPT_TRACE_INSTR(instr, _u("Didn't specialize array access, because index is negative or likely not int.\n"));
        if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 13347\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            baseValueType.ToString(baseValueTypeStr);
            Output::Print(_u("Typed Array Optimization:  function: %s (%s): instr: %s, base value type: %S, did not specialize because index is negative or likely not int.\n"),
                this->func->GetJITFunctionBody()->GetDisplayName(),
                this->func->GetDebugNumberSet(debugStringBuffer),
                Js::OpCodeUtil::GetOpCodeName(instr->m_opcode),
                baseValueTypeStr);
            Output::Flush();
        }
        return false;
    }

    IRType          toType = TyVar;
    bool            isLossyAllowed = true;
    IR::BailOutKind arrayBailOutKind = IR::BailOutConventionalTypedArrayAccessOnly;

    switch(baseValueType.GetObjectType())
    {LOGMEIN("GlobOpt.cpp] 13366\n");
    case ObjectType::Int8Array:
    case ObjectType::Uint8Array:
    case ObjectType::Int16Array:
    case ObjectType::Uint16Array:
    case ObjectType::Int32Array:
    case ObjectType::Int8VirtualArray:
    case ObjectType::Uint8VirtualArray:
    case ObjectType::Int16VirtualArray:
    case ObjectType::Uint16VirtualArray:
    case ObjectType::Int32VirtualArray:
    case ObjectType::Int8MixedArray:
    case ObjectType::Uint8MixedArray:
    case ObjectType::Int16MixedArray:
    case ObjectType::Uint16MixedArray:
    case ObjectType::Int32MixedArray:
    Int32Array:
        toType = TyInt32;
        break;

    case ObjectType::Uint32Array:
    case ObjectType::Uint32VirtualArray:
    case ObjectType::Uint32MixedArray:
        // Uint32Arrays may store values that overflow int32.  If the value being stored comes from a symbol that's
        // already losslessly type specialized to int32, we'll use it.  Otherwise, if we only have a float64 specialized
        // value, we don't want to force bailout if it doesn't fit in int32.  Instead, we'll emit conversion in the
        // lowerer, and handle overflow, if necessary.
        if (!sym || this->IsInt32TypeSpecialized(sym, this->currentBlock))
        {LOGMEIN("GlobOpt.cpp] 13394\n");
            toType = TyInt32;
        }
        else if (this->IsFloat64TypeSpecialized(sym, this->currentBlock))
        {LOGMEIN("GlobOpt.cpp] 13398\n");
            toType = TyFloat64;
        }
        break;

    case ObjectType::Float32Array:
    case ObjectType::Float64Array:
    case ObjectType::Float32VirtualArray:
    case ObjectType::Float32MixedArray:
    case ObjectType::Float64VirtualArray:
    case ObjectType::Float64MixedArray:
    Float64Array:
        toType = TyFloat64;
        break;

    case ObjectType::Uint8ClampedArray:
    case ObjectType::Uint8ClampedVirtualArray:
    case ObjectType::Uint8ClampedMixedArray:
        // Uint8ClampedArray requires rounding (as opposed to truncation) of floating point values. If source symbol is
        // float type specialized, type specialize this instruction to float as well, and handle rounding in the
        // lowerer.
            if (!sym || this->IsInt32TypeSpecialized(sym, this->currentBlock))
            {LOGMEIN("GlobOpt.cpp] 13420\n");
                toType = TyInt32;
                isLossyAllowed = false;
            }
            else if (this->IsFloat64TypeSpecialized(sym, this->currentBlock))
            {LOGMEIN("GlobOpt.cpp] 13425\n");
                toType = TyFloat64;
            }
        break;

    default:
        Assert(baseValueType.IsLikelyNativeArray());
        isLossyAllowed = false;
        arrayBailOutKind = IR::BailOutConventionalNativeArrayAccessOnly;
        if(baseValueType.HasIntElements())
        {LOGMEIN("GlobOpt.cpp] 13435\n");
            goto Int32Array;
        }
        Assert(baseValueType.HasFloatElements());
        goto Float64Array;
    }

    if (toType != TyVar)
    {
        GOPT_TRACE_INSTR(instr, _u("Type specialized array access.\n"));
        if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 13446\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            baseValueType.ToString(baseValueTypeStr);
            Output::Print(_u("Typed Array Optimization:  function: %s (%s): instr: %s, base value type: %S, type specialized to %s.\n"),
                this->func->GetJITFunctionBody()->GetDisplayName(),
                this->func->GetDebugNumberSet(debugStringBuffer),
                Js::OpCodeUtil::GetOpCodeName(instr->m_opcode),
                baseValueTypeStr,
                toType == TyInt32 ? _u("int32") : _u("float64"));
            Output::Flush();
        }

        IR::BailOutKind bailOutKind = ((toType == TyInt32) ? IR::BailOutIntOnly : IR::BailOutNumberOnly);
        this->ToTypeSpecUse(instr, instr->GetSrc1(), this->currentBlock, src1Val, nullptr, toType, bailOutKind, /* lossy = */ isLossyAllowed);

        if (!this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 13463\n");
            bool bConvertToBailoutInstr = true;
            // Definite StElemC doesn't need bailout, because it can't fail or cause conversion.
            if (instr->m_opcode == Js::OpCode::StElemC && baseValueType.IsObject())
            {LOGMEIN("GlobOpt.cpp] 13467\n");
                if (baseValueType.HasIntElements())
                {LOGMEIN("GlobOpt.cpp] 13469\n");
                    //Native int array requires a missing element check & bailout
                    int32 min = INT32_MIN;
                    int32 max = INT32_MAX;

                    if (src1Val->GetValueInfo()->GetIntValMinMax(&min, &max, false))
                    {LOGMEIN("GlobOpt.cpp] 13475\n");
                        bConvertToBailoutInstr = ((min <= Js::JavascriptNativeIntArray::MissingItem) && (max >= Js::JavascriptNativeIntArray::MissingItem));
                    }
                }
                else
                {
                    bConvertToBailoutInstr = false;
                }
            }

            if (bConvertToBailoutInstr)
            {LOGMEIN("GlobOpt.cpp] 13486\n");
                if(instr->HasBailOutInfo())
                {LOGMEIN("GlobOpt.cpp] 13488\n");
                    const IR::BailOutKind oldBailOutKind = instr->GetBailOutKind();
                    Assert(
                        (
                            !(oldBailOutKind & ~IR::BailOutKindBits) ||
                            (oldBailOutKind & ~IR::BailOutKindBits) == IR::BailOutOnImplicitCallsPreOp
                        ) &&
                        !(oldBailOutKind & IR::BailOutKindBits & ~(IR::BailOutOnArrayAccessHelperCall | IR::BailOutMarkTempObject)));
                    if(arrayBailOutKind == IR::BailOutConventionalTypedArrayAccessOnly)
                    {LOGMEIN("GlobOpt.cpp] 13497\n");
                        // BailOutConventionalTypedArrayAccessOnly also bails out if the array access is outside the head
                        // segment bounds, and guarantees no implicit calls. Override the bailout kind so that the instruction
                        // bails out for the right reason.
                        instr->SetBailOutKind(
                            arrayBailOutKind | (oldBailOutKind & (IR::BailOutKindBits - IR::BailOutOnArrayAccessHelperCall)));
                    }
                    else
                    {
                        // BailOutConventionalNativeArrayAccessOnly by itself may generate a helper call, and may cause implicit
                        // calls to occur, so it must be merged in to eliminate generating the helper call.
                        Assert(arrayBailOutKind == IR::BailOutConventionalNativeArrayAccessOnly);
                        instr->SetBailOutKind(oldBailOutKind | arrayBailOutKind);
                    }
                }
                else
                {
                    GenerateBailAtOperation(&instr, arrayBailOutKind);
                }
            }
        }
    }
    else
    {
        GOPT_TRACE_INSTR(instr, _u("Didn't specialize array access, because the source was not already specialized.\n"));
        if (PHASE_TRACE(Js::TypedArrayTypeSpecPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 13523\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
            char baseValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
            baseValueType.ToString(baseValueTypeStr);
            Output::Print(_u("Typed Array Optimization:  function: %s (%s): instr: %s, base value type: %S, did not type specialize, because of array type.\n"),
                this->func->GetJITFunctionBody()->GetDisplayName(),
                this->func->GetDebugNumberSet(debugStringBuffer),
                Js::OpCodeUtil::GetOpCodeName(instr->m_opcode),
                baseValueTypeStr);
            Output::Flush();
        }
    }

    return toType != TyVar;
}

IR::Instr *
GlobOpt::ToVarUses(IR::Instr *instr, IR::Opnd *opnd, bool isDst, Value *val)
{LOGMEIN("GlobOpt.cpp] 13541\n");
    Sym *sym;

    switch (opnd->GetKind())
    {LOGMEIN("GlobOpt.cpp] 13545\n");
    case IR::OpndKindReg:
        if (!isDst && !this->blockData.liveVarSyms->Test(opnd->AsRegOpnd()->m_sym->m_id))
        {LOGMEIN("GlobOpt.cpp] 13548\n");
            instr = this->ToVar(instr, opnd->AsRegOpnd(), this->currentBlock, val, true);
        }
        break;

    case IR::OpndKindSym:
        sym = opnd->AsSymOpnd()->m_sym;

        if (sym->IsPropertySym() && !this->blockData.liveVarSyms->Test(sym->AsPropertySym()->m_stackSym->m_id)
            && sym->AsPropertySym()->m_stackSym->IsVar())
        {LOGMEIN("GlobOpt.cpp] 13558\n");
            StackSym *propertyBase = sym->AsPropertySym()->m_stackSym;
            IR::RegOpnd *newOpnd = IR::RegOpnd::New(propertyBase, TyVar, instr->m_func);
            instr = this->ToVar(instr, newOpnd, this->currentBlock, this->FindValue(propertyBase), true);
        }
        break;

    case IR::OpndKindIndir:
        IR::RegOpnd *baseOpnd = opnd->AsIndirOpnd()->GetBaseOpnd();
        if (!this->blockData.liveVarSyms->Test(baseOpnd->m_sym->m_id))
        {LOGMEIN("GlobOpt.cpp] 13568\n");
            instr = this->ToVar(instr, baseOpnd, this->currentBlock, this->FindValue(baseOpnd->m_sym), true);
        }
        IR::RegOpnd *indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
        if (indexOpnd && !indexOpnd->m_sym->IsTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 13573\n");
            if((indexOpnd->GetValueType().IsInt()
                    ? !IsTypeSpecPhaseOff(func)
                    : indexOpnd->GetValueType().IsLikelyInt() && DoAggressiveIntTypeSpec()) && !GetIsAsmJSFunc()) // typespec is disabled for asmjs
            {LOGMEIN("GlobOpt.cpp] 13577\n");
                StackSym *const indexVarSym = indexOpnd->m_sym;
                Value *const indexValue = FindValue(indexVarSym);
                Assert(indexValue);
                Assert(indexValue->GetValueInfo()->IsLikelyInt());

                ToInt32(instr, indexOpnd, currentBlock, indexValue, opnd->AsIndirOpnd(), false);
                Assert(indexValue->GetValueInfo()->IsInt());

                if(!IsLoopPrePass())
                {LOGMEIN("GlobOpt.cpp] 13587\n");
                    indexOpnd = opnd->AsIndirOpnd()->GetIndexOpnd();
                    if(indexOpnd)
                    {LOGMEIN("GlobOpt.cpp] 13590\n");
                        Assert(indexOpnd->m_sym->IsTypeSpec());
                        IntConstantBounds indexConstantBounds;
                        AssertVerify(indexValue->GetValueInfo()->TryGetIntConstantBounds(&indexConstantBounds));
                        if(ValueInfo::IsGreaterThanOrEqualTo(
                                indexValue,
                                indexConstantBounds.LowerBound(),
                                indexConstantBounds.UpperBound(),
                                nullptr,
                                0,
                                0))
                        {LOGMEIN("GlobOpt.cpp] 13601\n");
                            indexOpnd->SetType(TyUint32);
                        }
                    }
                }
            }
            else if (!this->blockData.liveVarSyms->Test(indexOpnd->m_sym->m_id))
            {LOGMEIN("GlobOpt.cpp] 13608\n");
                instr = this->ToVar(instr, indexOpnd, this->currentBlock, this->FindValue(indexOpnd->m_sym), true);
            }
        }
        break;
    }

    return instr;
}

IR::Instr *
GlobOpt::ToVar(IR::Instr *instr, IR::RegOpnd *regOpnd, BasicBlock *block, Value *value, bool needsUpdate)
{LOGMEIN("GlobOpt.cpp] 13620\n");
    IR::Instr *newInstr;
    StackSym *varSym = regOpnd->m_sym;

    if (IsTypeSpecPhaseOff(this->func))
    {LOGMEIN("GlobOpt.cpp] 13625\n");
        return instr;
    }

    if (this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 13630\n");
        block->globOptData.liveVarSyms->Set(varSym->m_id);
        return instr;
    }

    if (block->globOptData.liveVarSyms->Test(varSym->m_id))
    {LOGMEIN("GlobOpt.cpp] 13636\n");
        // Already live, nothing to do
        return instr;
    }

    if (!varSym->IsVar())
    {LOGMEIN("GlobOpt.cpp] 13642\n");
        Assert(!varSym->IsTypeSpec());
        // Leave non-vars alone.
        return instr;
    }

    Assert(this->IsTypeSpecialized(varSym, block));

    if (!value)
    {LOGMEIN("GlobOpt.cpp] 13651\n");
        value = this->FindValue(block->globOptData.symToValueMap, varSym);
    }

    ValueInfo *valueInfo = value ? value->GetValueInfo() : nullptr;
    if(valueInfo && valueInfo->IsInt())
    {LOGMEIN("GlobOpt.cpp] 13657\n");
        // If two syms have the same value, one is lossy-int-specialized, and then the other is int-specialized, the value
        // would have been updated to definitely int. Upon using the lossy-int-specialized sym later, it would be flagged as
        // lossy while the value is definitely int. Since the bit-vectors are based on the sym and not the value, update the
        // lossy state.
        block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id);
    }

    IRType fromType;
    StackSym *typeSpecSym;

    if (block->globOptData.liveInt32Syms->Test(varSym->m_id) && !block->globOptData.liveLossyInt32Syms->Test(varSym->m_id))
    {LOGMEIN("GlobOpt.cpp] 13669\n");
        fromType = TyInt32;
        typeSpecSym = varSym->GetInt32EquivSym(this->func);
        Assert(valueInfo);
        Assert(valueInfo->IsInt());
    }
    else if (block->globOptData.liveFloat64Syms->Test(varSym->m_id))
    {LOGMEIN("GlobOpt.cpp] 13676\n");

        fromType = TyFloat64;
        typeSpecSym = varSym->GetFloat64EquivSym(this->func);

        // Ensure that all bailout FromVars that generate a value for this type-specialized sym will bail out on any non-number
        // value, even ones that have already been generated before. Float-specialized non-number values cannot be converted
        // back to Var since they will not go back to the original non-number value. The dead-store pass will update the bailout
        // kind on already-generated FromVars based on this bit.
        typeSpecSym->m_requiresBailOnNotNumber = true;

        // A previous float conversion may have used BailOutPrimitiveButString, which does not change the value type to say
        // definitely float, since it can also be a non-string primitive. The convert back to Var though, will cause that
        // bailout kind to be changed to BailOutNumberOnly in the dead-store phase, so from the point of the initial conversion
        // to float, that the value is definitely number. Since we don't know where the FromVar is, change the value type here.
        if(valueInfo)
        {LOGMEIN("GlobOpt.cpp] 13692\n");
            if(!valueInfo->IsNumber())
            {LOGMEIN("GlobOpt.cpp] 13694\n");
                valueInfo = valueInfo->SpecializeToFloat64(alloc);
                ChangeValueInfo(block, value, valueInfo);
                regOpnd->SetValueType(valueInfo->Type());
            }
        }
        else
        {
            value = NewGenericValue(ValueType::Float);
            valueInfo = value->GetValueInfo();
            SetValue(&block->globOptData, value, varSym);
            regOpnd->SetValueType(valueInfo->Type());
        }
    }
    else
    {
        // SIMD_JS
        Assert(IsLiveAsSimd128(varSym, &block->globOptData));
        if (IsLiveAsSimd128F4(varSym, &block->globOptData))
        {LOGMEIN("GlobOpt.cpp] 13713\n");
            fromType = TySimd128F4;
        }
        else
        {
            Assert(IsLiveAsSimd128I4(varSym, &block->globOptData));
            fromType = TySimd128I4;
        }

        if (valueInfo)
        {LOGMEIN("GlobOpt.cpp] 13723\n");
            if (fromType == TySimd128F4 && !valueInfo->Type().IsSimd128Float32x4())
            {LOGMEIN("GlobOpt.cpp] 13725\n");
                valueInfo = valueInfo->SpecializeToSimd128F4(alloc);
                ChangeValueInfo(block, value, valueInfo);
                regOpnd->SetValueType(valueInfo->Type());
            }
            else if (fromType == TySimd128I4 && !valueInfo->Type().IsSimd128Int32x4())
            {LOGMEIN("GlobOpt.cpp] 13731\n");
                if (!valueInfo->Type().IsSimd128Int32x4())
                {LOGMEIN("GlobOpt.cpp] 13733\n");
                    valueInfo = valueInfo->SpecializeToSimd128I4(alloc);
                    ChangeValueInfo(block, value, valueInfo);
                    regOpnd->SetValueType(valueInfo->Type());
                }
            }
        }
        else
        {
            ValueType valueType = fromType == TySimd128F4 ? ValueType::GetSimd128(ObjectType::Simd128Float32x4) : ValueType::GetSimd128(ObjectType::Simd128Int32x4);
            value = NewGenericValue(valueType);
            valueInfo = value->GetValueInfo();
            SetValue(&block->globOptData, value, varSym);
            regOpnd->SetValueType(valueInfo->Type());
        }

        ValueType valueType = valueInfo->Type();

        // Should be definite if type-specialized
        Assert(valueType.IsSimd128());

        typeSpecSym = varSym->GetSimd128EquivSym(fromType, this->func);
    }
    Assert(valueInfo);

    int32 intConstantValue;
    if (valueInfo->TryGetIntConstantValue(&intConstantValue))
    {LOGMEIN("GlobOpt.cpp] 13760\n");
        // Lower will tag or create a number directly
        newInstr = IR::Instr::New(Js::OpCode::LdC_A_I4, regOpnd,
            IR::IntConstOpnd::New(intConstantValue, TyInt32, instr->m_func), instr->m_func);
    }
    else
    {
        IR::RegOpnd * regNew = IR::RegOpnd::New(typeSpecSym, fromType, instr->m_func);
        Js::OpCode opcode = Js::OpCode::ToVar;
        regNew->SetIsJITOptimizedReg(true);

        newInstr = IR::Instr::New(opcode, regOpnd, regNew, instr->m_func);
    }
    newInstr->SetByteCodeOffset(instr);
    newInstr->GetDst()->AsRegOpnd()->SetIsJITOptimizedReg(true);

    ValueType valueType = valueInfo->Type();
    if(fromType == TyInt32)
    {LOGMEIN("GlobOpt.cpp] 13778\n");
    #if !INT32VAR // All 32-bit ints are taggable on 64-bit architectures
        IntConstantBounds constantBounds;
        AssertVerify(valueInfo->TryGetIntConstantBounds(&constantBounds));
        if(constantBounds.IsTaggable())
    #endif
        {LOGMEIN("GlobOpt.cpp] 13784\n");
            // The value is within the taggable range, so set the opnd value types to TaggedInt to avoid the overflow check
            valueType = ValueType::GetTaggedInt();
        }
    }
    newInstr->GetDst()->SetValueType(valueType);
    newInstr->GetSrc1()->SetValueType(valueType);

    IR::Instr *insertAfterInstr = instr->m_prev;
    if (instr == block->GetLastInstr() &&
        (instr->IsBranchInstr() || instr->m_opcode == Js::OpCode::BailTarget))
    {LOGMEIN("GlobOpt.cpp] 13795\n");
        // Don't insert code between the branch and the preceding ByteCodeUses instrs...
        while(insertAfterInstr->m_opcode == Js::OpCode::ByteCodeUses)
        {LOGMEIN("GlobOpt.cpp] 13798\n");
            insertAfterInstr = insertAfterInstr->m_prev;
        }
    }
    block->InsertInstrAfter(newInstr, insertAfterInstr);

    block->globOptData.liveVarSyms->Set(varSym->m_id);

    GOPT_TRACE_OPND(regOpnd, _u("Converting to var\n"));

    if (block->loop)
    {LOGMEIN("GlobOpt.cpp] 13809\n");
        Assert(!this->IsLoopPrePass());
        this->TryHoistInvariant(newInstr, block, value, value, nullptr, false);
    }

    if (needsUpdate)
    {LOGMEIN("GlobOpt.cpp] 13815\n");
        // Make sure that the kill effect of the ToVar instruction is tracked and that the kill of a property
        // type is reflected in the current instruction.
        this->ProcessKills(newInstr);
        this->ValueNumberObjectType(newInstr->GetDst(), newInstr);

        if (instr->GetSrc1() && instr->GetSrc1()->IsSymOpnd() && instr->GetSrc1()->AsSymOpnd()->IsPropertySymOpnd())
        {LOGMEIN("GlobOpt.cpp] 13822\n");
            // Reprocess the load source. We need to reset the PropertySymOpnd fields first.
            IR::PropertySymOpnd *propertySymOpnd = instr->GetSrc1()->AsPropertySymOpnd();
            if (propertySymOpnd->IsTypeCheckSeqCandidate())
            {LOGMEIN("GlobOpt.cpp] 13826\n");
                propertySymOpnd->SetTypeChecked(false);
                propertySymOpnd->SetTypeAvailable(false);
                propertySymOpnd->SetWriteGuardChecked(false);
            }

            this->FinishOptPropOp(instr, propertySymOpnd);
            instr = this->SetTypeCheckBailOut(instr->GetSrc1(), instr, nullptr);
        }
    }

    return instr;
}

IR::Instr *
GlobOpt::ToInt32(IR::Instr *instr, IR::Opnd *opnd, BasicBlock *block, Value *val, IR::IndirOpnd *indir, bool lossy)
{LOGMEIN("GlobOpt.cpp] 13842\n");
    return this->ToTypeSpecUse(instr, opnd, block, val, indir, TyInt32, IR::BailOutIntOnly, lossy);
}

IR::Instr *
GlobOpt::ToFloat64(IR::Instr *instr, IR::Opnd *opnd, BasicBlock *block, Value *val, IR::IndirOpnd *indir, IR::BailOutKind bailOutKind)
{LOGMEIN("GlobOpt.cpp] 13848\n");
    return this->ToTypeSpecUse(instr, opnd, block, val, indir, TyFloat64, bailOutKind);
}

IR::Instr *
GlobOpt::ToTypeSpecUse(IR::Instr *instr, IR::Opnd *opnd, BasicBlock *block, Value *val, IR::IndirOpnd *indir, IRType toType, IR::BailOutKind bailOutKind, bool lossy, IR::Instr *insertBeforeInstr)
{LOGMEIN("GlobOpt.cpp] 13854\n");
    Assert(bailOutKind != IR::BailOutInvalid);
    IR::Instr *newInstr;

    if (!val && opnd->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 13859\n");
        val = this->FindValue(block->globOptData.symToValueMap, opnd->AsRegOpnd()->m_sym);
    }

    ValueInfo *valueInfo = val ? val->GetValueInfo() : nullptr;
    bool needReplaceSrc = false;
    bool updateBlockLastInstr = false;

    if (instr)
    {LOGMEIN("GlobOpt.cpp] 13868\n");
        needReplaceSrc = true;
        if (!insertBeforeInstr)
        {LOGMEIN("GlobOpt.cpp] 13871\n");
            insertBeforeInstr = instr;
        }
    }
    else if (!insertBeforeInstr)
    {LOGMEIN("GlobOpt.cpp] 13876\n");
        // Insert it at the end of the block
        insertBeforeInstr = block->GetLastInstr();
        if (insertBeforeInstr->IsBranchInstr() || insertBeforeInstr->m_opcode == Js::OpCode::BailTarget)
        {LOGMEIN("GlobOpt.cpp] 13880\n");
            // Don't insert code between the branch and the preceding ByteCodeUses instrs...
            while(insertBeforeInstr->m_prev->m_opcode == Js::OpCode::ByteCodeUses)
            {LOGMEIN("GlobOpt.cpp] 13883\n");
                insertBeforeInstr = insertBeforeInstr->m_prev;
            }
        }
        else
        {
            insertBeforeInstr = insertBeforeInstr->m_next;
            updateBlockLastInstr = true;
        }
    }

    // Int constant values will be propagated into the instruction. For ArgOut_A_InlineBuiltIn, there's no benefit from
    // const-propping, so those are excluded.
    if (opnd->IsRegOpnd() &&
        !(
            valueInfo &&
            (valueInfo->HasIntConstantValue() || valueInfo->IsFloatConstant()) &&
            (!instr || instr->m_opcode != Js::OpCode::ArgOut_A_InlineBuiltIn)
        ))
    {LOGMEIN("GlobOpt.cpp] 13902\n");
        IR::RegOpnd *regSrc = opnd->AsRegOpnd();
        StackSym *varSym = regSrc->m_sym;
        Js::OpCode opcode = Js::OpCode::FromVar;

        if (varSym->IsTypeSpec() || !block->globOptData.liveVarSyms->Test(varSym->m_id))
        {LOGMEIN("GlobOpt.cpp] 13908\n");
            // Conversion between int32 and float64
            if (varSym->IsTypeSpec())
            {LOGMEIN("GlobOpt.cpp] 13911\n");
                varSym = varSym->GetVarEquivSym(this->func);
            }
            opcode = Js::OpCode::Conv_Prim;
        }

        Assert(block->globOptData.liveVarSyms->Test(varSym->m_id) || this->IsTypeSpecialized(varSym, block));

        StackSym *typeSpecSym;
        BOOL isLive;
        BVSparse<JitArenaAllocator> *livenessBv;

        if(valueInfo && valueInfo->IsInt())
        {LOGMEIN("GlobOpt.cpp] 13924\n");
            // If two syms have the same value, one is lossy-int-specialized, and then the other is int-specialized, the value
            // would have been updated to definitely int. Upon using the lossy-int-specialized sym later, it would be flagged as
            // lossy while the value is definitely int. Since the bit-vectors are based on the sym and not the value, update the
            // lossy state.
            block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id);
        }

        if (toType == TyInt32)
        {LOGMEIN("GlobOpt.cpp] 13933\n");
            // Need to determine whether the conversion is actually lossy or lossless. If the value is an int, then it's a
            // lossless conversion despite the type of conversion requested. The liveness of the converted int32 sym needs to be
            // set to reflect the actual type of conversion done. Also, a lossless conversion needs the value to determine
            // whether the conversion may need to bail out.
            Assert(valueInfo);
            if(valueInfo->IsInt())
            {LOGMEIN("GlobOpt.cpp] 13940\n");
                lossy = false;
            }
            else
            {
                Assert(IsLoopPrePass() || !IsInt32TypeSpecialized(varSym, block));
            }

            livenessBv = block->globOptData.liveInt32Syms;
            isLive = livenessBv->Test(varSym->m_id) && (lossy || !block->globOptData.liveLossyInt32Syms->Test(varSym->m_id));
            if (this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 13951\n");
                if(!isLive)
                {LOGMEIN("GlobOpt.cpp] 13953\n");
                    livenessBv->Set(varSym->m_id);
                    if(lossy)
                    {LOGMEIN("GlobOpt.cpp] 13956\n");
                        block->globOptData.liveLossyInt32Syms->Set(varSym->m_id);
                    }
                    else
                    {
                        block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id);
                    }
                }
                if(!lossy)
                {LOGMEIN("GlobOpt.cpp] 13965\n");
                    Assert(bailOutKind == IR::BailOutIntOnly || bailOutKind == IR::BailOutExpectingInteger);
                    valueInfo = valueInfo->SpecializeToInt32(alloc);
                    ChangeValueInfo(nullptr, val, valueInfo);
                    if(needReplaceSrc)
                    {LOGMEIN("GlobOpt.cpp] 13970\n");
                        opnd->SetValueType(valueInfo->Type());
                    }
                }
                return instr;
            }

            typeSpecSym = varSym->GetInt32EquivSym(this->func);

            if (!isLive)
            {LOGMEIN("GlobOpt.cpp] 13980\n");
                if (!opnd->IsVar() ||
                    !block->globOptData.liveVarSyms->Test(varSym->m_id) ||
                    (block->globOptData.liveFloat64Syms->Test(varSym->m_id) && valueInfo && valueInfo->IsLikelyFloat()))
                {LOGMEIN("GlobOpt.cpp] 13984\n");
                    Assert(block->globOptData.liveFloat64Syms->Test(varSym->m_id));

                    if(!lossy && !valueInfo->IsInt())
                    {LOGMEIN("GlobOpt.cpp] 13988\n");
                        // Shouldn't try to do a lossless conversion from float64 to int32 when the value is not known to be an
                        // int. There are cases where we need more than two passes over loops to flush out all dependencies.
                        // It's possible for the loop prepass to think that a sym s1 remains an int because it acquires the
                        // value of another sym s2 that is an int in the prepass at that time. However, s2 can become a float
                        // later in the loop body, in which case s1 would become a float on the second iteration of the loop. By
                        // that time, we would have already committed to having s1 live as a lossless int on entry into the
                        // loop, and we end up having to compensate by doing a lossless conversion from float to int, which will
                        // need a bailout and will most likely bail out.
                        //
                        // If s2 becomes a var instead of a float, then the compensation is legal although not ideal. After
                        // enough bailouts, rejit would be triggered with aggressive int type spec turned off. For the
                        // float-to-int conversion though, there's no point in emitting a bailout because we already know that
                        // the value is a float and has high probability of bailing out (whereas a var has a chance to be a
                        // tagged int), and so currently lossless conversion from float to int with bailout is not supported.
                        //
                        // So, treating this case as a compile-time bailout. The exception will trigger the jit work item to be
                        // restarted with aggressive int type specialization disabled.
                        if(bailOutKind == IR::BailOutExpectingInteger)
                        {LOGMEIN("GlobOpt.cpp] 14007\n");
                            Assert(IsSwitchOptEnabled());
                            throw Js::RejitException(RejitReason::DisableSwitchOptExpectingInteger);
                        }
                        else
                        {
                            Assert(DoAggressiveIntTypeSpec());
                            if(PHASE_TRACE(Js::BailOutPhase, this->func))
                            {LOGMEIN("GlobOpt.cpp] 14015\n");
                                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                                Output::Print(
                                    _u("BailOut (compile-time): function: %s (%s) varSym: "),
                                    this->func->GetJITFunctionBody()->GetDisplayName(),
                                    this->func->GetDebugNumberSet(debugStringBuffer),
                                    varSym->m_id);
    #if DBG_DUMP
                                varSym->Dump();
    #else
                                Output::Print(_u("s%u"), varSym->m_id);
    #endif
                                if(varSym->HasByteCodeRegSlot())
                                {LOGMEIN("GlobOpt.cpp] 14028\n");
                                    Output::Print(_u(" byteCodeReg: R%u"), varSym->GetByteCodeRegSlot());
                                }
                                Output::Print(_u(" (lossless conversion from float64 to int32)\n"));
                                Output::Flush();
                            }

                            if(!DoAggressiveIntTypeSpec())
                            {LOGMEIN("GlobOpt.cpp] 14036\n");
                                // Aggressive int type specialization is already off for some reason. Prevent trying to rejit again
                                // because it won't help and the same thing will happen again. Just abort jitting this function.
                                if(PHASE_TRACE(Js::BailOutPhase, this->func))
                                {LOGMEIN("GlobOpt.cpp] 14040\n");
                                    Output::Print(_u("    Aborting JIT because AggressiveIntTypeSpec is already off\n"));
                                    Output::Flush();
                                }
                                throw Js::OperationAbortedException();
                            }

                            throw Js::RejitException(RejitReason::AggressiveIntTypeSpecDisabled);
                        }
                    }

                    if(opnd->IsVar())
                    {LOGMEIN("GlobOpt.cpp] 14052\n");
                        regSrc->SetType(TyFloat64);
                        regSrc->m_sym = varSym->GetFloat64EquivSym(this->func);
                        opcode = Js::OpCode::Conv_Prim;
                    }
                    else
                    {
                        Assert(regSrc->IsFloat64());
                        Assert(regSrc->m_sym->IsFloat64());
                        Assert(opcode == Js::OpCode::Conv_Prim);
                    }
                }
            }
            GOPT_TRACE_OPND(regSrc, _u("Converting to int32\n"));
        }
        else if (toType == TyFloat64)
        {LOGMEIN("GlobOpt.cpp] 14068\n");
            // float64
            typeSpecSym = varSym->GetFloat64EquivSym(this->func);
            if(!IsLoopPrePass() && typeSpecSym->m_requiresBailOnNotNumber && IsFloat64TypeSpecialized(varSym, block))
            {LOGMEIN("GlobOpt.cpp] 14072\n");
                // This conversion is already protected by a BailOutNumberOnly bailout (or at least it will be after the
                // dead-store phase). Since 'requiresBailOnNotNumber' is not flow-based, change the value to definitely float.
                if(valueInfo)
                {LOGMEIN("GlobOpt.cpp] 14076\n");
                    if(!valueInfo->IsNumber())
                    {LOGMEIN("GlobOpt.cpp] 14078\n");
                        valueInfo = valueInfo->SpecializeToFloat64(alloc);
                        ChangeValueInfo(block, val, valueInfo);
                        opnd->SetValueType(valueInfo->Type());
                    }
                }
                else
                {
                    val = NewGenericValue(ValueType::Float);
                    valueInfo = val->GetValueInfo();
                    SetValue(&block->globOptData, val, varSym);
                    opnd->SetValueType(valueInfo->Type());
                }
            }

            if(bailOutKind == IR::BailOutNumberOnly)
            {LOGMEIN("GlobOpt.cpp] 14094\n");
                if(!IsLoopPrePass())
                {LOGMEIN("GlobOpt.cpp] 14096\n");
                    // Ensure that all bailout FromVars that generate a value for this type-specialized sym will bail out on any
                    // non-number value, even ones that have already been generated before. The dead-store pass will update the
                    // bailout kind on already-generated FromVars based on this bit.
                    typeSpecSym->m_requiresBailOnNotNumber = true;
                }
            }
            else if(typeSpecSym->m_requiresBailOnNotNumber)
            {LOGMEIN("GlobOpt.cpp] 14104\n");
                Assert(bailOutKind == IR::BailOutPrimitiveButString);
                bailOutKind = IR::BailOutNumberOnly;
            }

            livenessBv = block->globOptData.liveFloat64Syms;
            isLive = livenessBv->Test(varSym->m_id);
            if (this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 14112\n");
                if(!isLive)
                {LOGMEIN("GlobOpt.cpp] 14114\n");
                    livenessBv->Set(varSym->m_id);
                }

                if (this->OptIsInvariant(opnd, block, this->prePassLoop, val, false, true))
                {LOGMEIN("GlobOpt.cpp] 14119\n");
                    this->prePassLoop->forceFloat64SymsOnEntry->Set(varSym->m_id);
                }
                else
                {
                    Sym *symStore = (valueInfo ? valueInfo->GetSymStore() : NULL);
                    if (symStore && symStore != varSym
                        && this->OptIsInvariant(symStore, block, this->prePassLoop, this->FindValue(block->globOptData.symToValueMap, symStore), false, true))
                    {LOGMEIN("GlobOpt.cpp] 14127\n");
                        // If symStore is assigned to sym and we want sym to be type-specialized, for symStore to be specialized
                        // outside the loop.
                        this->prePassLoop->forceFloat64SymsOnEntry->Set(symStore->m_id);
                    }
                }

                if(bailOutKind == IR::BailOutNumberOnly)
                {LOGMEIN("GlobOpt.cpp] 14135\n");
                    if(valueInfo)
                    {LOGMEIN("GlobOpt.cpp] 14137\n");
                        valueInfo = valueInfo->SpecializeToFloat64(alloc);
                        ChangeValueInfo(block, val, valueInfo);
                    }
                    else
                    {
                        val = NewGenericValue(ValueType::Float);
                        valueInfo = val->GetValueInfo();
                        SetValue(&block->globOptData, val, varSym);
                    }
                    if(needReplaceSrc)
                    {LOGMEIN("GlobOpt.cpp] 14148\n");
                        opnd->SetValueType(valueInfo->Type());
                    }
                }
                return instr;
            }

            if (!isLive && regSrc->IsVar())
            {LOGMEIN("GlobOpt.cpp] 14156\n");
                if (!block->globOptData.liveVarSyms->Test(varSym->m_id) ||
                    (
                        block->globOptData.liveInt32Syms->Test(varSym->m_id) &&
                        !block->globOptData.liveLossyInt32Syms->Test(varSym->m_id) &&
                        valueInfo &&
                        valueInfo->IsLikelyInt()
                    ))
                {LOGMEIN("GlobOpt.cpp] 14164\n");
                    Assert(block->globOptData.liveInt32Syms->Test(varSym->m_id));
                    Assert(!block->globOptData.liveLossyInt32Syms->Test(varSym->m_id)); // Shouldn't try to convert a lossy int32 to anything
                    regSrc->SetType(TyInt32);
                    regSrc->m_sym = varSym->GetInt32EquivSym(this->func);
                    opcode = Js::OpCode::Conv_Prim;
                }
            }
            GOPT_TRACE_OPND(regSrc, _u("Converting to float64\n"));
        }
        else
        {
            // SIMD_JS
            Assert(IRType_IsSimd128(toType));

            // Get or create type-spec sym
            typeSpecSym = varSym->GetSimd128EquivSym(toType, this->func);

            if (!IsLoopPrePass() && IsSimd128TypeSpecialized(toType, varSym, block))
            {LOGMEIN("GlobOpt.cpp] 14183\n");
                // Consider: Is this needed ? Shouldn't this have been done at previous FromVar since the simd128 sym is alive ?
                if (valueInfo)
                {LOGMEIN("GlobOpt.cpp] 14186\n");
                    if (!valueInfo->IsSimd128(toType))
                    {LOGMEIN("GlobOpt.cpp] 14188\n");
                        valueInfo = valueInfo->SpecializeToSimd128(toType, alloc);
                        ChangeValueInfo(block, val, valueInfo);
                        opnd->SetValueType(valueInfo->Type());
                    }
                }
                else
                {
                    val = NewGenericValue(GetValueTypeFromIRType(toType));
                    valueInfo = val->GetValueInfo();
                    SetValue(&block->globOptData, val, varSym);
                    opnd->SetValueType(valueInfo->Type());
                }
            }

            livenessBv = block->globOptData.GetSimd128LivenessBV(toType);
            isLive = livenessBv->Test(varSym->m_id);
            if (this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 14206\n");
                // FromVar Hoisting
                BVSparse<Memory::JitArenaAllocator> * forceSimd128SymsOnEntry;
                forceSimd128SymsOnEntry = \
                    toType == TySimd128F4 ? this->prePassLoop->forceSimd128F4SymsOnEntry : this->prePassLoop->forceSimd128I4SymsOnEntry;
                if (!isLive)
                {LOGMEIN("GlobOpt.cpp] 14212\n");
                    livenessBv->Set(varSym->m_id);
                }

                // Be aggressive with hoisting only if value is always initialized to SIMD type before entering loop.
                // This reduces the chance that the FromVar gets executed while the specialized instruction in the loop is not. Leading to unnecessary excessive bailouts.
                if (val && !val->GetValueInfo()->HasBeenUndefined() &&  !val->GetValueInfo()->HasBeenNull() &&
                    this->OptIsInvariant(opnd, block, this->prePassLoop, val, false, true))
                {LOGMEIN("GlobOpt.cpp] 14220\n");
                    forceSimd128SymsOnEntry->Set(varSym->m_id);
                }
                else
                {
                    Sym *symStore = (valueInfo ? valueInfo->GetSymStore() : NULL);
                    Value * value = symStore ? this->FindValue(block->globOptData.symToValueMap, symStore) : nullptr;

                    if (symStore && symStore != varSym
                        && value
                        && !value->GetValueInfo()->HasBeenUndefined() && !value->GetValueInfo()->HasBeenNull()
                        && this->OptIsInvariant(symStore, block, this->prePassLoop, value, true, true))
                    {LOGMEIN("GlobOpt.cpp] 14232\n");
                        // If symStore is assigned to sym and we want sym to be type-specialized, for symStore to be specialized
                        // outside the loop.
                        forceSimd128SymsOnEntry->Set(symStore->m_id);
                    }
                }

                Assert(bailOutKind == IR::BailOutSimd128F4Only || bailOutKind == IR::BailOutSimd128I4Only);

                // We are in loop prepass, we haven't propagated the value info to the src. Do it now.
                if (valueInfo)
                {LOGMEIN("GlobOpt.cpp] 14243\n");
                    valueInfo = valueInfo->SpecializeToSimd128(toType, alloc);
                    ChangeValueInfo(block, val, valueInfo);
                }
                else
                {
                    val = NewGenericValue(GetValueTypeFromIRType(toType));
                    valueInfo = val->GetValueInfo();
                    SetValue(&block->globOptData, val, varSym);
                }
                if (needReplaceSrc)
                {LOGMEIN("GlobOpt.cpp] 14254\n");
                    opnd->SetValueType(valueInfo->Type());
                }
                return instr;
            }
            GOPT_TRACE_OPND(regSrc, _u("Converting to Simd128\n"));
        }
        bool needLoad = false;

        if (needReplaceSrc)
        {LOGMEIN("GlobOpt.cpp] 14264\n");
            bool wasDead = regSrc->GetIsDead();
            // needReplaceSrc means we are type specializing a use, and need to replace the src on the instr
            if (!isLive)
            {LOGMEIN("GlobOpt.cpp] 14268\n");
                needLoad = true;
                // ReplaceSrc will delete it.
                regSrc = regSrc->Copy(instr->m_func)->AsRegOpnd();
            }
            IR::RegOpnd * regNew = IR::RegOpnd::New(typeSpecSym, toType, instr->m_func);
            if(valueInfo)
            {LOGMEIN("GlobOpt.cpp] 14275\n");
                regNew->SetValueType(valueInfo->Type());
                regNew->m_wasNegativeZeroPreventedByBailout = valueInfo->WasNegativeZeroPreventedByBailout();
            }
            regNew->SetIsDead(wasDead);
            regNew->SetIsJITOptimizedReg(true);

            this->CaptureByteCodeSymUses(instr);
            if (indir == nullptr)
            {LOGMEIN("GlobOpt.cpp] 14284\n");
                instr->ReplaceSrc(opnd, regNew);
            }
            else
            {
                indir->ReplaceIndexOpnd(regNew);
            }
            opnd = regNew;

            if (!needLoad)
            {LOGMEIN("GlobOpt.cpp] 14294\n");
                Assert(isLive);
                return instr;
            }
        }
        else
        {
            // We just need to insert a load of a type spec sym
            if(isLive)
            {LOGMEIN("GlobOpt.cpp] 14303\n");
                return instr;
            }

            // Insert it before the specified instruction
            instr = insertBeforeInstr;
        }


        IR::RegOpnd *regDst = IR::RegOpnd::New(typeSpecSym, toType, instr->m_func);
        bool isBailout = false;
        bool isHoisted = false;
        bool isInLandingPad = (block->next && !block->next->isDeleted && block->next->isLoopHeader);

        if (isInLandingPad)
        {LOGMEIN("GlobOpt.cpp] 14318\n");
            Loop *loop = block->next->loop;
            Assert(loop && loop->landingPad == block);
            Assert(loop->bailOutInfo);
        }

        if (opcode == Js::OpCode::FromVar)
        {LOGMEIN("GlobOpt.cpp] 14325\n");

            if (toType == TyInt32)
            {LOGMEIN("GlobOpt.cpp] 14328\n");
                Assert(valueInfo);
                if (lossy)
                {LOGMEIN("GlobOpt.cpp] 14331\n");
                    if (!valueInfo->IsPrimitive() && !IsTypeSpecialized(varSym, block))
                    {LOGMEIN("GlobOpt.cpp] 14333\n");
                        // Lossy conversions to int32 on non-primitive values may have implicit calls to toString or valueOf, which
                        // may be overridden to have a side effect. The side effect needs to happen every time the conversion is
                        // supposed to happen, so the resulting lossy int32 value cannot be reused. Bail out on implicit calls.
                        Assert(DoLossyIntTypeSpec());

                        bailOutKind = IR::BailOutOnNotPrimitive;
                        isBailout = true;
                    }
                }
                else if (!valueInfo->IsInt())
                {LOGMEIN("GlobOpt.cpp] 14344\n");
                    // The operand is likely an int (hence the request to convert to int), so bail out if it's not an int. Only
                    // bail out if a lossless conversion to int is requested. Lossy conversions to int such as in (a | 0) don't
                    // need to bail out.
                    if (bailOutKind == IR::BailOutExpectingInteger)
                    {LOGMEIN("GlobOpt.cpp] 14349\n");
                        Assert(IsSwitchOptEnabled());
                    }
                    else
                    {
                        Assert(DoAggressiveIntTypeSpec());
                    }

                    isBailout = true;
                }
            }
            else if (toType == TyFloat64 &&
                (!valueInfo || !valueInfo->IsNumber()))
            {LOGMEIN("GlobOpt.cpp] 14362\n");
                // Bailout if converting vars to float if we can't prove they are floats:
                //      x = str + float;  -> need to bailout if str is a string
                //
                //      x = obj * 0.1;
                //      y = obj * 0.2;  -> if obj has valueof, we'll only call valueof once on the FromVar conversion...
                Assert(bailOutKind != IR::BailOutInvalid);
                isBailout = true;
            }
            else if (IRType_IsSimd128(toType) &&
                (!valueInfo || !valueInfo->IsSimd128(toType)))
            {LOGMEIN("GlobOpt.cpp] 14373\n");
                Assert(toType == TySimd128F4 && bailOutKind == IR::BailOutSimd128F4Only
                    || toType == TySimd128I4 && bailOutKind == IR::BailOutSimd128I4Only);
                isBailout = true;
            }
        }

        if (isBailout)
        {LOGMEIN("GlobOpt.cpp] 14381\n");
            if (isInLandingPad)
            {LOGMEIN("GlobOpt.cpp] 14383\n");
                Loop *loop = block->next->loop;
                this->EnsureBailTarget(loop);
                instr = loop->bailOutInfo->bailOutInstr;
                updateBlockLastInstr = false;
                newInstr = IR::BailOutInstr::New(opcode, bailOutKind, loop->bailOutInfo, instr->m_func);
                newInstr->SetDst(regDst);
                newInstr->SetSrc1(regSrc);
            }
            else
            {
                newInstr = IR::BailOutInstr::New(opcode, regDst, regSrc, bailOutKind, instr, instr->m_func);
            }
        }
        else
        {
            newInstr = IR::Instr::New(opcode, regDst, regSrc, instr->m_func);
        }

        newInstr->SetByteCodeOffset(instr);
        instr->InsertBefore(newInstr);
        if (updateBlockLastInstr)
        {LOGMEIN("GlobOpt.cpp] 14405\n");
            block->SetLastInstr(newInstr);
        }
        regDst->SetIsJITOptimizedReg(true);
        newInstr->GetSrc1()->AsRegOpnd()->SetIsJITOptimizedReg(true);

        ValueInfo *const oldValueInfo = valueInfo;
        if(valueInfo)
        {LOGMEIN("GlobOpt.cpp] 14413\n");
            newInstr->GetSrc1()->SetValueType(valueInfo->Type());
        }
        if(isBailout)
        {LOGMEIN("GlobOpt.cpp] 14417\n");
            Assert(opcode == Js::OpCode::FromVar);
            if(toType == TyInt32)
            {LOGMEIN("GlobOpt.cpp] 14420\n");
                Assert(valueInfo);
                if(!lossy)
                {LOGMEIN("GlobOpt.cpp] 14423\n");
                    Assert(bailOutKind == IR::BailOutIntOnly || bailOutKind == IR::BailOutExpectingInteger);
                    valueInfo = valueInfo->SpecializeToInt32(alloc, isPerformingLoopBackEdgeCompensation);
                    ChangeValueInfo(nullptr, val, valueInfo);

                    int32 intConstantValue;
                    if(indir && needReplaceSrc && valueInfo->TryGetIntConstantValue(&intConstantValue))
                    {LOGMEIN("GlobOpt.cpp] 14430\n");
                        // A likely-int value can have constant bounds due to conditional branches narrowing its range. Now that
                        // the sym has been proven to be an int, the likely-int value, after specialization, will be constant.
                        // Replace the index opnd in the indir with an offset.
                        Assert(opnd == indir->GetIndexOpnd());
                        Assert(indir->GetScale() == 0);
                        indir->UnlinkIndexOpnd()->Free(instr->m_func);
                        opnd = nullptr;
                        indir->SetOffset(intConstantValue);
                    }
                }
            }
            else if (toType == TyFloat64)
            {LOGMEIN("GlobOpt.cpp] 14443\n");
                if(bailOutKind == IR::BailOutNumberOnly)
                {LOGMEIN("GlobOpt.cpp] 14445\n");
                    if(valueInfo)
                    {LOGMEIN("GlobOpt.cpp] 14447\n");
                        valueInfo = valueInfo->SpecializeToFloat64(alloc);
                        ChangeValueInfo(block, val, valueInfo);
                    }
                    else
                    {
                        val = NewGenericValue(ValueType::Float);
                        valueInfo = val->GetValueInfo();
                        SetValue(&block->globOptData, val, varSym);
                    }
                }
            }
            else
            {
                Assert(IRType_IsSimd128(toType));
                if (valueInfo)
                {LOGMEIN("GlobOpt.cpp] 14463\n");
                    valueInfo = valueInfo->SpecializeToSimd128(toType, alloc);
                    ChangeValueInfo(block, val, valueInfo);
                }
                else
                {
                    val = NewGenericValue(GetValueTypeFromIRType(toType));
                    valueInfo = val->GetValueInfo();
                    SetValue(&block->globOptData, val, varSym);
                }
            }
        }

        if(valueInfo)
        {LOGMEIN("GlobOpt.cpp] 14477\n");
            newInstr->GetDst()->SetValueType(valueInfo->Type());
            if(needReplaceSrc && opnd)
            {LOGMEIN("GlobOpt.cpp] 14480\n");
                opnd->SetValueType(valueInfo->Type());
            }
        }

        if (block->loop)
        {LOGMEIN("GlobOpt.cpp] 14486\n");
            Assert(!this->IsLoopPrePass());
            isHoisted = this->TryHoistInvariant(newInstr, block, val, val, nullptr, false, lossy, false, bailOutKind);
        }

        if (isBailout)
        {LOGMEIN("GlobOpt.cpp] 14492\n");
            if (!isHoisted && !isInLandingPad)
            {LOGMEIN("GlobOpt.cpp] 14494\n");
                if(valueInfo)
                {LOGMEIN("GlobOpt.cpp] 14496\n");
                    // Since this is a pre-op bailout, the old value info should be used for the purposes of bailout. For
                    // instance, the value info could be LikelyInt but with a constant range. Once specialized to int, the value
                    // info would be an int constant. However, the int constant is only guaranteed if the value is actually an
                    // int, which this conversion is verifying, so bailout cannot assume the constant value.
                    if(oldValueInfo)
                    {LOGMEIN("GlobOpt.cpp] 14502\n");
                        val->SetValueInfo(oldValueInfo);
                    }
                    else
                    {
                        block->globOptData.symToValueMap->Clear(varSym->m_id);
                    }
                }

                // Fill in bail out info if the FromVar is a bailout instr, and it wasn't hoisted as invariant.
                // If it was hoisted, the invariant code will fill out the bailout info with the loop landing pad bailout info.
                this->FillBailOutInfo(block, newInstr->GetBailOutInfo());

                if(valueInfo)
                {LOGMEIN("GlobOpt.cpp] 14516\n");
                    // Restore the new value info after filling the bailout info
                    if(oldValueInfo)
                    {LOGMEIN("GlobOpt.cpp] 14519\n");
                        val->SetValueInfo(valueInfo);
                    }
                    else
                    {
                        SetValue(&block->globOptData, val, varSym);
                    }
                }
            }
        }

        // Now that we've captured the liveness in the bailout info, we can mark this as live.
        // This type specialized sym isn't live if the FromVar bails out.
        livenessBv->Set(varSym->m_id);
        if(toType == TyInt32)
        {LOGMEIN("GlobOpt.cpp] 14534\n");
            if(lossy)
            {LOGMEIN("GlobOpt.cpp] 14536\n");
                block->globOptData.liveLossyInt32Syms->Set(varSym->m_id);
            }
            else
            {
                block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id);
            }
        }
    }
    else
    {
        Assert(valueInfo);
        if(opnd->IsRegOpnd() && valueInfo->IsInt())
        {LOGMEIN("GlobOpt.cpp] 14549\n");
            // If two syms have the same value, one is lossy-int-specialized, and then the other is int-specialized, the value
            // would have been updated to definitely int. Upon using the lossy-int-specialized sym later, it would be flagged as
            // lossy while the value is definitely int. Since the bit-vectors are based on the sym and not the value, update the
            // lossy state.
            block->globOptData.liveLossyInt32Syms->Clear(opnd->AsRegOpnd()->m_sym->m_id);
            if(toType == TyInt32)
            {LOGMEIN("GlobOpt.cpp] 14556\n");
                lossy = false;
            }
        }

        if (this->IsLoopPrePass())
        {LOGMEIN("GlobOpt.cpp] 14562\n");
            if(opnd->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 14564\n");
                StackSym *const sym = opnd->AsRegOpnd()->m_sym;
                if(toType == TyInt32)
                {LOGMEIN("GlobOpt.cpp] 14567\n");
                    Assert(!sym->IsTypeSpec());
                    block->globOptData.liveInt32Syms->Set(sym->m_id);
                    if(lossy)
                    {LOGMEIN("GlobOpt.cpp] 14571\n");
                        block->globOptData.liveLossyInt32Syms->Set(sym->m_id);
                    }
                    else
                    {
                        block->globOptData.liveLossyInt32Syms->Clear(sym->m_id);
                    }
                }
                else
                {
                    Assert(toType == TyFloat64);
                    AnalysisAssert(instr);
                    StackSym *const varSym = sym->IsTypeSpec() ? sym->GetVarEquivSym(instr->m_func) : sym;
                    block->globOptData.liveFloat64Syms->Set(varSym->m_id);
                }
            }
            return instr;
        }

        if (!needReplaceSrc)
        {LOGMEIN("GlobOpt.cpp] 14591\n");
            instr = insertBeforeInstr;
        }

        IR::Opnd *constOpnd;
        int32 intConstantValue;
        if(valueInfo->TryGetIntConstantValue(&intConstantValue))
        {LOGMEIN("GlobOpt.cpp] 14598\n");
            if(toType == TyInt32)
            {LOGMEIN("GlobOpt.cpp] 14600\n");
                constOpnd = IR::IntConstOpnd::New(intConstantValue, TyInt32, instr->m_func);
            }
            else
            {
                Assert(toType == TyFloat64);
                constOpnd = IR::FloatConstOpnd::New(static_cast<FloatConstType>(intConstantValue), TyFloat64, instr->m_func);
            }
        }
        else if(valueInfo->IsFloatConstant())
        {LOGMEIN("GlobOpt.cpp] 14610\n");
            const FloatConstType floatValue = valueInfo->AsFloatConstant()->FloatValue();
            if(toType == TyInt32)
            {LOGMEIN("GlobOpt.cpp] 14613\n");
                Assert(lossy);
                constOpnd =
                    IR::IntConstOpnd::New(
                        Js::JavascriptMath::ToInt32(floatValue),
                        TyInt32,
                        instr->m_func);
            }
            else
            {
                Assert(toType == TyFloat64);
                constOpnd = IR::FloatConstOpnd::New(floatValue, TyFloat64, instr->m_func);
            }
        }
        else
        {
            Assert(opnd->IsVar());
            Assert(opnd->IsAddrOpnd());
            AssertMsg(opnd->AsAddrOpnd()->IsVar(), "We only expect to see addr that are var before lower.");

            // Don't need to capture uses, we are only replacing an addr opnd
            if(toType == TyInt32)
            {LOGMEIN("GlobOpt.cpp] 14635\n");
                constOpnd = IR::IntConstOpnd::New(Js::TaggedInt::ToInt32(opnd->AsAddrOpnd()->m_address), TyInt32, instr->m_func);
            }
            else
            {
                Assert(toType == TyFloat64);
                constOpnd = IR::FloatConstOpnd::New(Js::TaggedInt::ToDouble(opnd->AsAddrOpnd()->m_address), TyFloat64, instr->m_func);
            }
        }

        if (toType == TyInt32)
        {LOGMEIN("GlobOpt.cpp] 14646\n");
            if (needReplaceSrc)
            {LOGMEIN("GlobOpt.cpp] 14648\n");
                CaptureByteCodeSymUses(instr);
                if(indir)
                {LOGMEIN("GlobOpt.cpp] 14651\n");
                    Assert(opnd == indir->GetIndexOpnd());
                    Assert(indir->GetScale() == 0);
                    indir->UnlinkIndexOpnd()->Free(instr->m_func);
                    indir->SetOffset(constOpnd->AsIntConstOpnd()->AsInt32());
                }
                else
                {
                    instr->ReplaceSrc(opnd, constOpnd);
                }
            }
            else
            {
                StackSym *varSym = opnd->AsRegOpnd()->m_sym;
                if(varSym->IsTypeSpec())
                {LOGMEIN("GlobOpt.cpp] 14666\n");
                    varSym = varSym->GetVarEquivSym(nullptr);
                    Assert(varSym);
                }
                if(block->globOptData.liveInt32Syms->TestAndSet(varSym->m_id))
                {LOGMEIN("GlobOpt.cpp] 14671\n");
                    Assert(!!block->globOptData.liveLossyInt32Syms->Test(varSym->m_id) == lossy);
                }
                else
                {
                    if(lossy)
                    {LOGMEIN("GlobOpt.cpp] 14677\n");
                        block->globOptData.liveLossyInt32Syms->Set(varSym->m_id);
                    }

                    StackSym *int32Sym = varSym->GetInt32EquivSym(instr->m_func);
                    IR::RegOpnd *int32Reg = IR::RegOpnd::New(int32Sym, TyInt32, instr->m_func);
                    int32Reg->SetIsJITOptimizedReg(true);
                    newInstr = IR::Instr::New(Js::OpCode::Ld_I4, int32Reg, constOpnd, instr->m_func);
                    newInstr->SetByteCodeOffset(instr);
                    instr->InsertBefore(newInstr);
                    if (updateBlockLastInstr)
                    {LOGMEIN("GlobOpt.cpp] 14688\n");
                        block->SetLastInstr(newInstr);
                    }
                }
            }
        }
        else
        {
            StackSym *floatSym;

            bool newFloatSym = false;
            StackSym* varSym;
            if (opnd->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 14701\n");
                varSym = opnd->AsRegOpnd()->m_sym;
                if (varSym->IsTypeSpec())
                {LOGMEIN("GlobOpt.cpp] 14704\n");
                    varSym = varSym->GetVarEquivSym(nullptr);
                    Assert(varSym);
                }
                floatSym = varSym->GetFloat64EquivSym(instr->m_func);
            }
            else
            {
                varSym = GetCopyPropSym(block, nullptr, val);
                // If there is no float 64 type specialized sym for this - create a new sym.
                if(!varSym || !IsFloat64TypeSpecialized(varSym, block))
                {LOGMEIN("GlobOpt.cpp] 14715\n");
                    // Clear the symstore to ensure it's set below to this new symbol
                    this->SetSymStoreDirect(val->GetValueInfo(), nullptr);
                    varSym = StackSym::New(TyVar, instr->m_func);
                    newFloatSym = true;
                }
                floatSym = varSym->GetFloat64EquivSym(instr->m_func);
            }

            IR::RegOpnd *floatReg = IR::RegOpnd::New(floatSym, TyFloat64, instr->m_func);
            floatReg->SetIsJITOptimizedReg(true);

            // If the value is not live - let's load it.
            if(!block->globOptData.liveFloat64Syms->TestAndSet(varSym->m_id))
            {LOGMEIN("GlobOpt.cpp] 14729\n");
                newInstr = IR::Instr::New(Js::OpCode::LdC_F8_R8, floatReg, constOpnd, instr->m_func);
                newInstr->SetByteCodeOffset(instr);
                instr->InsertBefore(newInstr);
                if (updateBlockLastInstr)
                {LOGMEIN("GlobOpt.cpp] 14734\n");
                    block->SetLastInstr(newInstr);
                }
                if(newFloatSym)
                {LOGMEIN("GlobOpt.cpp] 14738\n");
                    this->SetValue(&block->globOptData, val, varSym);
                }

                // Src is always invariant, but check if the dst is, and then hoist.
                if (block->loop &&
                    (
                        (newFloatSym && block->loop->CanHoistInvariants()) ||
                        this->OptIsInvariant(floatReg, block, block->loop, val, false, false)
                    ))
                {LOGMEIN("GlobOpt.cpp] 14748\n");
                    Assert(!this->IsLoopPrePass());
                    this->OptHoistInvariant(newInstr, block, block->loop, val, val, false);
                }
            }

            if (needReplaceSrc)
            {LOGMEIN("GlobOpt.cpp] 14755\n");
                CaptureByteCodeSymUses(instr);
                instr->ReplaceSrc(opnd, floatReg);
            }

        }

        return instr;
    }

    return newInstr;
}

void
GlobOpt::ToVarRegOpnd(IR::RegOpnd *dst, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14770\n");
    ToVarStackSym(dst->m_sym, block);
}

void
GlobOpt::ToVarStackSym(StackSym *varSym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14776\n");
    //added another check for sym , in case of asmjs there is mostly no var syms and hence added a new check to see if it is the primary sym
    Assert(!varSym->IsTypeSpec());

    block->globOptData.liveVarSyms->Set(varSym->m_id);
    block->globOptData.liveInt32Syms->Clear(varSym->m_id);
    block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id);
    block->globOptData.liveFloat64Syms->Clear(varSym->m_id);

    // SIMD_JS
    block->globOptData.liveSimd128F4Syms->Clear(varSym->m_id);
    block->globOptData.liveSimd128I4Syms->Clear(varSym->m_id);

}

void
GlobOpt::ToInt32Dst(IR::Instr *instr, IR::RegOpnd *dst, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14793\n");
    StackSym *varSym = dst->m_sym;
    Assert(!varSym->IsTypeSpec());

    if (!this->IsLoopPrePass() && varSym->IsVar())
    {LOGMEIN("GlobOpt.cpp] 14798\n");
        StackSym *int32Sym = varSym->GetInt32EquivSym(instr->m_func);

        // Use UnlinkDst / SetDst to make sure isSingleDef is tracked properly,
        // since we'll just be hammering the symbol.
        dst = instr->UnlinkDst()->AsRegOpnd();
        dst->m_sym = int32Sym;
        dst->SetType(TyInt32);
        instr->SetDst(dst);
    }

    block->globOptData.liveInt32Syms->Set(varSym->m_id);
    block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id); // The store makes it lossless
    block->globOptData.liveVarSyms->Clear(varSym->m_id);
    block->globOptData.liveFloat64Syms->Clear(varSym->m_id);

    // SIMD_JS
    block->globOptData.liveSimd128F4Syms->Clear(varSym->m_id);
    block->globOptData.liveSimd128I4Syms->Clear(varSym->m_id);
}

void
GlobOpt::ToUInt32Dst(IR::Instr *instr, IR::RegOpnd *dst, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14821\n");
    // We should be calling only for asmjs function
    Assert(GetIsAsmJSFunc());
    StackSym *varSym = dst->m_sym;
    Assert(!varSym->IsTypeSpec());
    block->globOptData.liveInt32Syms->Set(varSym->m_id);
    block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id); // The store makes it lossless
    block->globOptData.liveVarSyms->Clear(varSym->m_id);
    block->globOptData.liveFloat64Syms->Clear(varSym->m_id);

    // SIMD_JS
    block->globOptData.liveSimd128F4Syms->Clear(varSym->m_id);
    block->globOptData.liveSimd128I4Syms->Clear(varSym->m_id);
}

void
GlobOpt::ToFloat64Dst(IR::Instr *instr, IR::RegOpnd *dst, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14838\n");
    StackSym *varSym = dst->m_sym;
    Assert(!varSym->IsTypeSpec());

    if (!this->IsLoopPrePass() && varSym->IsVar())
    {LOGMEIN("GlobOpt.cpp] 14843\n");
        StackSym *float64Sym = varSym->GetFloat64EquivSym(this->func);

        // Use UnlinkDst / SetDst to make sure isSingleDef is tracked properly,
        // since we'll just be hammering the symbol.
        dst = instr->UnlinkDst()->AsRegOpnd();
        dst->m_sym = float64Sym;
        dst->SetType(TyFloat64);
        instr->SetDst(dst);
    }

    block->globOptData.liveFloat64Syms->Set(varSym->m_id);
    block->globOptData.liveVarSyms->Clear(varSym->m_id);
    block->globOptData.liveInt32Syms->Clear(varSym->m_id);
    block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id);

    // SIMD_JS
    block->globOptData.liveSimd128F4Syms->Clear(varSym->m_id);
    block->globOptData.liveSimd128I4Syms->Clear(varSym->m_id);
}

// SIMD_JS
void
GlobOpt::ToSimd128Dst(IRType toType, IR::Instr *instr, IR::RegOpnd *dst, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14867\n");
    StackSym *varSym = dst->m_sym;
    Assert(!varSym->IsTypeSpec());
    BVSparse<JitArenaAllocator> * livenessBV = block->globOptData.GetSimd128LivenessBV(toType);

    Assert(livenessBV);

    if (!this->IsLoopPrePass() && varSym->IsVar())
    {LOGMEIN("GlobOpt.cpp] 14875\n");
        StackSym *simd128Sym = varSym->GetSimd128EquivSym(toType, this->func);

        // Use UnlinkDst / SetDst to make sure isSingleDef is tracked properly,
        // since we'll just be hammering the symbol.
        dst = instr->UnlinkDst()->AsRegOpnd();
        dst->m_sym = simd128Sym;
        dst->SetType(toType);
        instr->SetDst(dst);
    }

    block->globOptData.liveFloat64Syms->Clear(varSym->m_id);
    block->globOptData.liveVarSyms->Clear(varSym->m_id);
    block->globOptData.liveInt32Syms->Clear(varSym->m_id);
    block->globOptData.liveLossyInt32Syms->Clear(varSym->m_id);

    // SIMD_JS
    block->globOptData.liveSimd128F4Syms->Clear(varSym->m_id);
    block->globOptData.liveSimd128I4Syms->Clear(varSym->m_id);

    livenessBV->Set(varSym->m_id);
}

BOOL
GlobOpt::IsInt32TypeSpecialized(Sym *sym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14900\n");
    return IsInt32TypeSpecialized(sym, &block->globOptData);
}

BOOL
GlobOpt::IsSwitchInt32TypeSpecialized(IR::Instr * instr, BasicBlock * block)
{LOGMEIN("GlobOpt.cpp] 14906\n");
    return IsSwitchOptEnabled(instr->m_func->GetTopFunc()) && instr->GetSrc1()->IsRegOpnd() &&
                                    IsInt32TypeSpecialized(instr->GetSrc1()->AsRegOpnd()->m_sym, block);
}

BOOL
GlobOpt::IsInt32TypeSpecialized(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 14913\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return sym && data->liveInt32Syms->Test(sym->m_id) && !data->liveLossyInt32Syms->Test(sym->m_id);
}

BOOL
GlobOpt::IsFloat64TypeSpecialized(Sym *sym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14920\n");
    return IsFloat64TypeSpecialized(sym, &block->globOptData);
}

BOOL
GlobOpt::IsFloat64TypeSpecialized(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 14926\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return sym && data->liveFloat64Syms->Test(sym->m_id);
}

// SIMD_JS
BOOL
GlobOpt::IsSimd128TypeSpecialized(Sym *sym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14934\n");
    return IsSimd128TypeSpecialized(sym, &block->globOptData);
}

BOOL
GlobOpt::IsSimd128TypeSpecialized(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 14940\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return sym && (data->liveSimd128F4Syms->Test(sym->m_id) || data->liveSimd128I4Syms->Test(sym->m_id));
}

BOOL
GlobOpt::IsSimd128TypeSpecialized(IRType type, Sym *sym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14947\n");
    return IsSimd128TypeSpecialized(type, sym, &block->globOptData);
}

BOOL
GlobOpt::IsSimd128TypeSpecialized(IRType type, Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 14953\n");
    switch (type)
    {LOGMEIN("GlobOpt.cpp] 14955\n");
    case TySimd128F4:
        return IsSimd128F4TypeSpecialized(sym, data);
    case TySimd128I4:
        return IsSimd128I4TypeSpecialized(sym, data);
    default:
        Assert(UNREACHED);
        return false;
    }
}

BOOL
GlobOpt::IsSimd128F4TypeSpecialized(Sym *sym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14968\n");
    return IsSimd128F4TypeSpecialized(sym, &block->globOptData);
}

BOOL
GlobOpt::IsSimd128F4TypeSpecialized(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 14974\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return sym && (data->liveSimd128F4Syms->Test(sym->m_id));
}

BOOL
GlobOpt::IsSimd128I4TypeSpecialized(Sym *sym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 14981\n");
    return IsSimd128I4TypeSpecialized(sym, &block->globOptData);
}

BOOL
GlobOpt::IsSimd128I4TypeSpecialized(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 14987\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return sym && (data->liveSimd128I4Syms->Test(sym->m_id));
}

BOOL
GlobOpt::IsLiveAsSimd128(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 14994\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return
        sym &&
        (
        data->liveSimd128F4Syms->Test(sym->m_id) ||
        data->liveSimd128I4Syms->Test(sym->m_id)
        );
}

BOOL
GlobOpt::IsLiveAsSimd128F4(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 15006\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return sym && data->liveSimd128F4Syms->Test(sym->m_id);
}

BOOL
GlobOpt::IsLiveAsSimd128I4(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 15013\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return sym && data->liveSimd128I4Syms->Test(sym->m_id);
}

BOOL
GlobOpt::IsTypeSpecialized(Sym *sym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 15020\n");
    return IsTypeSpecialized(sym, &block->globOptData);
}

BOOL
GlobOpt::IsTypeSpecialized(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 15026\n");
    return IsInt32TypeSpecialized(sym, data) || IsFloat64TypeSpecialized(sym, data) || IsSimd128TypeSpecialized(sym, data);
}

BOOL
GlobOpt::IsLive(Sym *sym, BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 15032\n");
    return IsLive(sym, &block->globOptData);
}

BOOL
GlobOpt::IsLive(Sym *sym, GlobOptBlockData *data)
{LOGMEIN("GlobOpt.cpp] 15038\n");
    sym = StackSym::GetVarEquivStackSym_NoCreate(sym);
    return
        sym &&
        (
        data->liveVarSyms->Test(sym->m_id) ||
        data->liveInt32Syms->Test(sym->m_id) ||
        data->liveFloat64Syms->Test(sym->m_id) ||
        data->liveSimd128F4Syms->Test(sym->m_id) ||
        data->liveSimd128I4Syms->Test(sym->m_id)
        );
}

void
GlobOpt::MakeLive(StackSym *const sym, GlobOptBlockData *const blockData, const bool lossy) const
{LOGMEIN("GlobOpt.cpp] 15053\n");
    Assert(sym);
    Assert(blockData);

    if(sym->IsTypeSpec())
    {LOGMEIN("GlobOpt.cpp] 15058\n");
        const SymID varSymId = sym->GetVarEquivSym(func)->m_id;
        if(sym->IsInt32())
        {LOGMEIN("GlobOpt.cpp] 15061\n");
            blockData->liveInt32Syms->Set(varSymId);
            if(lossy)
            {LOGMEIN("GlobOpt.cpp] 15064\n");
                blockData->liveLossyInt32Syms->Set(varSymId);
            }
            else
            {
                blockData->liveLossyInt32Syms->Clear(varSymId);
            }
            return;
        }

        if (sym->IsFloat64())
        {LOGMEIN("GlobOpt.cpp] 15075\n");
            blockData->liveFloat64Syms->Set(varSymId);
            return;
        }

        // SIMD_JS
        if (sym->IsSimd128F4())
        {LOGMEIN("GlobOpt.cpp] 15082\n");
            blockData->liveSimd128F4Syms->Set(varSymId);
            return;
        }

        if (sym->IsSimd128I4())
        {LOGMEIN("GlobOpt.cpp] 15088\n");
            blockData->liveSimd128I4Syms->Set(varSymId);
            return;
        }

    }

    blockData->liveVarSyms->Set(sym->m_id);
}

bool
GlobOpt::OptConstFoldBinary(
    IR::Instr * *pInstr,
    const IntConstantBounds &src1IntConstantBounds,
    const IntConstantBounds &src2IntConstantBounds,
    Value **pDstVal)
{LOGMEIN("GlobOpt.cpp] 15104\n");
    IR::Instr * &instr = *pInstr;
    int32 value;
    IR::IntConstOpnd *constOpnd;

    if (!DoConstFold())
    {LOGMEIN("GlobOpt.cpp] 15110\n");
        return false;
    }

    int32 src1IntConstantValue = -1;
    int32 src2IntConstantValue = -1;

    int32 src1MaxIntConstantValue = -1;
    int32 src2MaxIntConstantValue = -1;
    int32 src1MinIntConstantValue = -1;
    int32 src2MinIntConstantValue = -1;

    if (instr->IsBranchInstr())
    {LOGMEIN("GlobOpt.cpp] 15123\n");
        src1MinIntConstantValue = src1IntConstantBounds.LowerBound();
        src1MaxIntConstantValue = src1IntConstantBounds.UpperBound();
        src2MinIntConstantValue = src2IntConstantBounds.LowerBound();
        src2MaxIntConstantValue = src2IntConstantBounds.UpperBound();
    }
    else if (src1IntConstantBounds.IsConstant() && src2IntConstantBounds.IsConstant())
    {LOGMEIN("GlobOpt.cpp] 15130\n");
        src1IntConstantValue = src1IntConstantBounds.LowerBound();
        src2IntConstantValue = src2IntConstantBounds.LowerBound();
    }
    else
    {
        return false;
    }

    IntConstType tmpValueOut;
    if (!instr->BinaryCalculator(src1IntConstantValue, src2IntConstantValue, &tmpValueOut)
        || !Math::FitsInDWord(tmpValueOut))
    {LOGMEIN("GlobOpt.cpp] 15142\n");
        return false;
    }

    value = (int32)tmpValueOut;

    this->CaptureByteCodeSymUses(instr);
    constOpnd = IR::IntConstOpnd::New(value, TyInt32, instr->m_func);
    instr->ReplaceSrc1(constOpnd);
    instr->FreeSrc2();

    this->OptSrc(constOpnd, &instr);

    IR::Opnd *dst = instr->GetDst();
    Assert(dst->IsRegOpnd());

    StackSym *dstSym = dst->AsRegOpnd()->m_sym;

    if (dstSym->IsSingleDef())
    {LOGMEIN("GlobOpt.cpp] 15161\n");
        dstSym->SetIsIntConst(value);
    }

    GOPT_TRACE_INSTR(instr, _u("Constant folding to %d: \n"), value);

    *pDstVal = GetIntConstantValue(value, instr, dst);

    if (IsTypeSpecPhaseOff(this->func))
    {LOGMEIN("GlobOpt.cpp] 15170\n");
        instr->m_opcode = Js::OpCode::LdC_A_I4;
        this->ToVarRegOpnd(dst->AsRegOpnd(), this->currentBlock);
    }
    else
    {
        instr->m_opcode = Js::OpCode::Ld_I4;
        this->ToInt32Dst(instr, dst->AsRegOpnd(), this->currentBlock);
    }

    return true;
}

void
GlobOpt::OptConstFoldBr(bool test, IR::Instr *instr, Value * src1Val, Value * src2Val)
{
    GOPT_TRACE_INSTR(instr, _u("Constant folding to branch: "));
    BasicBlock *deadBlock;

    if (src1Val)
    {LOGMEIN("GlobOpt.cpp] 15190\n");
        this->ToInt32(instr, instr->GetSrc1(), this->currentBlock, src1Val, nullptr, false);
    }

    if (src2Val)
    {LOGMEIN("GlobOpt.cpp] 15195\n");
        this->ToInt32(instr, instr->GetSrc2(), this->currentBlock, src2Val, nullptr, false);
    }

    this->CaptureByteCodeSymUses(instr);

    if (test)
    {LOGMEIN("GlobOpt.cpp] 15202\n");
        instr->m_opcode = Js::OpCode::Br;

        instr->FreeSrc1();
        if(instr->GetSrc2())
        {LOGMEIN("GlobOpt.cpp] 15207\n");
            instr->FreeSrc2();
        }
        deadBlock = instr->m_next->AsLabelInstr()->GetBasicBlock();
    }
    else
    {
        AssertMsg(instr->m_next->IsLabelInstr(), "Next instr of branch should be a label...");
        if(instr->AsBranchInstr()->IsMultiBranch())
        {LOGMEIN("GlobOpt.cpp] 15216\n");
            return;
        }
        deadBlock = instr->AsBranchInstr()->GetTarget()->GetBasicBlock();
        instr->FreeSrc1();
        if(instr->GetSrc2())
        {LOGMEIN("GlobOpt.cpp] 15222\n");
            instr->FreeSrc2();
        }
        instr->m_opcode = Js::OpCode::Nop;
    }

    // Loop back edge: we would have already decremented data use count for the tail block when we processed the loop header.
    if (!(this->currentBlock->loop && this->currentBlock->loop->GetHeadBlock() == deadBlock))
    {LOGMEIN("GlobOpt.cpp] 15230\n");
        this->currentBlock->DecrementDataUseCount();
    }

    this->currentBlock->RemoveDeadSucc(deadBlock, this->func->m_fg);

    if (deadBlock->GetPredList()->Count() == 0)
    {LOGMEIN("GlobOpt.cpp] 15237\n");
        deadBlock->SetDataUseCount(0);
    }
}

void
GlobOpt::ChangeValueType(
    BasicBlock *const block,
    Value *const value,
    const ValueType newValueType,
    const bool preserveSubclassInfo,
    const bool allowIncompatibleType) const
{LOGMEIN("GlobOpt.cpp] 15249\n");
    Assert(value);
    // Why are we trying to change the value type of the type sym value? Asserting here to make sure we don't deep copy the type sym's value info.
    Assert(!value->GetValueInfo()->IsJsType());

    ValueInfo *const valueInfo = value->GetValueInfo();
    const ValueType valueType(valueInfo->Type());
    if(valueType == newValueType && (preserveSubclassInfo || valueInfo->IsGeneric()))
    {LOGMEIN("GlobOpt.cpp] 15257\n");
        return;
    }

    // ArrayValueInfo has information specific to the array type, so make sure that doesn't change
    Assert(
        !preserveSubclassInfo ||
        !valueInfo->IsArrayValueInfo() ||
        newValueType.IsObject() && newValueType.GetObjectType() == valueInfo->GetObjectType());

    ValueInfo *const newValueInfo =
        preserveSubclassInfo
            ? valueInfo->Copy(alloc)
            : valueInfo->CopyWithGenericStructureKind(alloc);
    newValueInfo->Type() = newValueType;
    ChangeValueInfo(block, value, newValueInfo, allowIncompatibleType);
}

void
GlobOpt::ChangeValueInfo(BasicBlock *const block, Value *const value, ValueInfo *const newValueInfo, const bool allowIncompatibleType, const bool compensated) const
{LOGMEIN("GlobOpt.cpp] 15277\n");
    Assert(value);
    Assert(newValueInfo);

    // The value type must be changed to something more specific or something more generic. For instance, it would be changed to
    // something more specific if the current value type is LikelyArray and checks have been done to ensure that it's an array,
    // and it would be changed to something more generic if a call kills the Array value type and it must be treated as
    // LikelyArray going forward.

    // There are cases where we change the type because of different profile information, and because of rejit, these profile information
    // may conflict. Need to allow incompatible type in those cause. However, the old type should be indefinite.
    Assert((allowIncompatibleType && !value->GetValueInfo()->IsDefinite()) ||
        AreValueInfosCompatible(newValueInfo, value->GetValueInfo()));

    // ArrayValueInfo has information specific to the array type, so make sure that doesn't change
    Assert(
        !value->GetValueInfo()->IsArrayValueInfo() ||
        !newValueInfo->IsArrayValueInfo() ||
        newValueInfo->GetObjectType() == value->GetValueInfo()->GetObjectType());

    if(block)
    {
        TrackValueInfoChangeForKills(block, value, newValueInfo, compensated);
    }
    value->SetValueInfo(newValueInfo);
}

bool
GlobOpt::AreValueInfosCompatible(const ValueInfo *const v0, const ValueInfo *const v1) const
{LOGMEIN("GlobOpt.cpp] 15306\n");
    Assert(v0);
    Assert(v1);

    if(v0->IsUninitialized() || v1->IsUninitialized())
    {LOGMEIN("GlobOpt.cpp] 15311\n");
        return true;
    }

    const bool doAggressiveIntTypeSpec = DoAggressiveIntTypeSpec();
    if(doAggressiveIntTypeSpec && (v0->IsInt() || v1->IsInt()))
    {LOGMEIN("GlobOpt.cpp] 15317\n");
        // Int specialization in some uncommon loop cases involving dependencies, needs to allow specializing values of
        // arbitrary types, even values that are definitely not int, to compensate for aggressive assumptions made by a loop
        // prepass
        return true;
    }
    if ((v0->Type()).IsMixedTypedArrayPair(v1->Type()) || (v1->Type()).IsMixedTypedArrayPair(v0->Type()))
    {LOGMEIN("GlobOpt.cpp] 15324\n");
        return true;
    }
    const bool doFloatTypeSpec = DoFloatTypeSpec();
    if(doFloatTypeSpec && (v0->IsFloat() || v1->IsFloat()))
    {LOGMEIN("GlobOpt.cpp] 15329\n");
        // Float specialization allows specializing values of arbitrary types, even values that are definitely not float
        return true;
    }

    // SIMD_JS
    if (SIMD128_TYPE_SPEC_FLAG && v0->Type().IsSimd128())
    {LOGMEIN("GlobOpt.cpp] 15336\n");
        // We only type-spec Undefined values, Objects (possibly merged SIMD values), or actual SIMD values.

        if (v1->Type().IsLikelyUndefined() || v1->Type().IsLikelyNull())
        {LOGMEIN("GlobOpt.cpp] 15340\n");
            return true;
        }

        if (v1->Type().IsLikelyObject() && v1->Type().GetObjectType() == ObjectType::Object)
        {LOGMEIN("GlobOpt.cpp] 15345\n");
            return true;
        }

        if (v1->Type().IsSimd128())
        {LOGMEIN("GlobOpt.cpp] 15350\n");
            return v0->Type().GetObjectType() == v1->Type().GetObjectType();
        }
    }

    const bool doArrayMissingValueCheckHoist = DoArrayMissingValueCheckHoist();
    const bool doNativeArrayTypeSpec = DoNativeArrayTypeSpec();
    const auto AreValueTypesCompatible = [=](const ValueType t0, const ValueType t1)
    {LOGMEIN("GlobOpt.cpp] 15358\n");
        return
            t0.IsSubsetOf(t1, doAggressiveIntTypeSpec, doFloatTypeSpec, doArrayMissingValueCheckHoist, doNativeArrayTypeSpec) ||
            t1.IsSubsetOf(t0, doAggressiveIntTypeSpec, doFloatTypeSpec, doArrayMissingValueCheckHoist, doNativeArrayTypeSpec);
    };

    const ValueType t0(v0->Type().ToDefinite()), t1(v1->Type().ToDefinite());
    if(t0.IsLikelyObject() && t1.IsLikelyObject())
    {LOGMEIN("GlobOpt.cpp] 15366\n");
        // Check compatibility for the primitive portions and the object portions of the value types separately
        if(AreValueTypesCompatible(t0.ToDefiniteObject(), t1.ToDefiniteObject()) &&
            (
                !t0.HasBeenPrimitive() ||
                !t1.HasBeenPrimitive() ||
                AreValueTypesCompatible(t0.ToDefinitePrimitiveSubset(), t1.ToDefinitePrimitiveSubset())
            ))
        {LOGMEIN("GlobOpt.cpp] 15374\n");
            return true;
        }
    }
    else if(AreValueTypesCompatible(t0, t1))
    {LOGMEIN("GlobOpt.cpp] 15379\n");
        return true;
    }

    const FloatConstantValueInfo *floatConstantValueInfo;
    const ValueInfo *likelyIntValueinfo;
    if(v0->IsFloatConstant() && v1->IsLikelyInt())
    {LOGMEIN("GlobOpt.cpp] 15386\n");
        floatConstantValueInfo = v0->AsFloatConstant();
        likelyIntValueinfo = v1;
    }
    else if(v0->IsLikelyInt() && v1->IsFloatConstant())
    {LOGMEIN("GlobOpt.cpp] 15391\n");
        floatConstantValueInfo = v1->AsFloatConstant();
        likelyIntValueinfo = v0;
    }
    else
    {
        return false;
    }

    // A float constant value with a value that is actually an int is a subset of a likely-int value.
    // Ideally, we should create an int constant value for this up front, such that IsInt() also returns true. There
    // were other issues with that, should see if that can be done.
    int32 int32Value;
    return
        Js::JavascriptNumber::TryGetInt32Value(floatConstantValueInfo->FloatValue(), &int32Value) &&
        (!likelyIntValueinfo->IsLikelyTaggedInt() || !Js::TaggedInt::IsOverflow(int32Value));
}

#if DBG
void
GlobOpt::VerifyArrayValueInfoForTracking(
    const ValueInfo *const valueInfo,
    const bool isJsArray,
    const BasicBlock *const block,
    const bool ignoreKnownImplicitCalls) const
{LOGMEIN("GlobOpt.cpp] 15416\n");
    Assert(valueInfo);
    Assert(valueInfo->IsAnyOptimizedArray());
    Assert(isJsArray == valueInfo->IsArrayOrObjectWithArray());
    Assert(!isJsArray == valueInfo->IsOptimizedTypedArray());
    Assert(block);

    Loop *implicitCallsLoop;
    if(block->next && !block->next->isDeleted && block->next->isLoopHeader)
    {LOGMEIN("GlobOpt.cpp] 15425\n");
        // Since a loop's landing pad does not have user code, determine whether disabling implicit calls is allowed in the
        // landing pad based on the loop for which this block is the landing pad.
        implicitCallsLoop = block->next->loop;
        Assert(implicitCallsLoop);
        Assert(implicitCallsLoop->landingPad == block);
    }
    else
    {
        implicitCallsLoop = block->loop;
    }

    Assert(
        !isJsArray ||
        DoArrayCheckHoist(valueInfo->Type(), implicitCallsLoop) ||
        (
            ignoreKnownImplicitCalls &&
            !(implicitCallsLoop ? ImplicitCallFlagsAllowOpts(implicitCallsLoop) : ImplicitCallFlagsAllowOpts(func))
        ));
    Assert(!(isJsArray && valueInfo->HasNoMissingValues() && !DoArrayMissingValueCheckHoist()));
    Assert(
        !(
            valueInfo->IsArrayValueInfo() &&
            (
                valueInfo->AsArrayValueInfo()->HeadSegmentSym() ||
                valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym()
            ) &&
            !DoArraySegmentHoist(valueInfo->Type())
        ));
#if 0
    // We can't assert here that there is only a head segment length sym if hoisting is allowed in the current block,
    // because we may have propagated the sym forward out of a loop, and hoisting may be allowed inside but not
    // outside the loop.
    Assert(
        isJsArray ||
        !valueInfo->IsArrayValueInfo() ||
        !valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym() ||
        DoTypedArraySegmentLengthHoist(implicitCallsLoop) ||
        ignoreKnownImplicitCalls ||
        (implicitCallsLoop ? ImplicitCallFlagsAllowOpts(implicitCallsLoop) : ImplicitCallFlagsAllowOpts(func))
        );
#endif
    Assert(
        !(
            isJsArray &&
            valueInfo->IsArrayValueInfo() &&
            valueInfo->AsArrayValueInfo()->LengthSym() &&
            !DoArrayLengthHoist()
        ));
}
#endif

void
GlobOpt::TrackNewValueForKills(Value *const value)
{LOGMEIN("GlobOpt.cpp] 15479\n");
    Assert(value);

    if(!value->GetValueInfo()->IsAnyOptimizedArray())
    {LOGMEIN("GlobOpt.cpp] 15483\n");
        return;
    }

    DoTrackNewValueForKills(value);
}

void
GlobOpt::DoTrackNewValueForKills(Value *const value)
{LOGMEIN("GlobOpt.cpp] 15492\n");
    Assert(value);

    ValueInfo *const valueInfo = value->GetValueInfo();
    Assert(valueInfo->IsAnyOptimizedArray());
    Assert(!valueInfo->IsArrayValueInfo());

    // The value and value info here are new, so it's okay to modify the value info in-place
    Assert(!valueInfo->GetSymStore());

    const bool isJsArray = valueInfo->IsArrayOrObjectWithArray();
    Assert(!isJsArray == valueInfo->IsOptimizedTypedArray());

    Loop *implicitCallsLoop;
    if(currentBlock->next && !currentBlock->next->isDeleted && currentBlock->next->isLoopHeader)
    {LOGMEIN("GlobOpt.cpp] 15507\n");
        // Since a loop's landing pad does not have user code, determine whether disabling implicit calls is allowed in the
        // landing pad based on the loop for which this block is the landing pad.
        implicitCallsLoop = currentBlock->next->loop;
        Assert(implicitCallsLoop);
        Assert(implicitCallsLoop->landingPad == currentBlock);
    }
    else
    {
        implicitCallsLoop = currentBlock->loop;
    }

    if(isJsArray)
    {LOGMEIN("GlobOpt.cpp] 15520\n");
        if(!DoArrayCheckHoist(valueInfo->Type(), implicitCallsLoop))
        {LOGMEIN("GlobOpt.cpp] 15522\n");
            // Array opts are disabled for this value type, so treat it as an indefinite value type going forward
            valueInfo->Type() = valueInfo->Type().ToLikely();
            return;
        }

        if(valueInfo->HasNoMissingValues() && !DoArrayMissingValueCheckHoist())
        {LOGMEIN("GlobOpt.cpp] 15529\n");
            valueInfo->Type() = valueInfo->Type().SetHasNoMissingValues(false);
        }
    }

#if DBG
    VerifyArrayValueInfoForTracking(valueInfo, isJsArray, currentBlock);
#endif

    if(!isJsArray)
    {LOGMEIN("GlobOpt.cpp] 15539\n");
        return;
    }

    // Can't assume going forward that it will definitely be an array without disabling implicit calls, because the
    // array may be transformed into an ES5 array. Since array opts are enabled, implicit calls can be disabled, and we can
    // treat it as a definite value type going forward, but the value needs to be tracked so that something like a call can
    // revert the value type to a likely version.
    blockData.valuesToKillOnCalls->Add(value);
}

void
GlobOpt::TrackCopiedValueForKills(Value *const value)
{LOGMEIN("GlobOpt.cpp] 15552\n");
    Assert(value);

    if(!value->GetValueInfo()->IsAnyOptimizedArray())
    {LOGMEIN("GlobOpt.cpp] 15556\n");
        return;
    }

    DoTrackCopiedValueForKills(value);
}

void
GlobOpt::DoTrackCopiedValueForKills(Value *const value)
{LOGMEIN("GlobOpt.cpp] 15565\n");
    Assert(value);

    ValueInfo *const valueInfo = value->GetValueInfo();
    Assert(valueInfo->IsAnyOptimizedArray());

    const bool isJsArray = valueInfo->IsArrayOrObjectWithArray();
    Assert(!isJsArray == valueInfo->IsOptimizedTypedArray());

#if DBG
    VerifyArrayValueInfoForTracking(valueInfo, isJsArray, currentBlock);
#endif

    if(!isJsArray && !(valueInfo->IsArrayValueInfo() && valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym()))
    {LOGMEIN("GlobOpt.cpp] 15579\n");
        return;
    }

    // Can't assume going forward that it will definitely be an array without disabling implicit calls, because the
    // array may be transformed into an ES5 array. Since array opts are enabled, implicit calls can be disabled, and we can
    // treat it as a definite value type going forward, but the value needs to be tracked so that something like a call can
    // revert the value type to a likely version.
    blockData.valuesToKillOnCalls->Add(value);
}

void
GlobOpt::TrackMergedValueForKills(
    Value *const value,
    GlobOptBlockData *const blockData,
    BVSparse<JitArenaAllocator> *const mergedValueTypesTrackedForKills) const
{LOGMEIN("GlobOpt.cpp] 15595\n");
    Assert(value);

    if(!value->GetValueInfo()->IsAnyOptimizedArray())
    {LOGMEIN("GlobOpt.cpp] 15599\n");
        return;
    }

    DoTrackMergedValueForKills(value, blockData, mergedValueTypesTrackedForKills);
}

void
GlobOpt::DoTrackMergedValueForKills(
    Value *const value,
    GlobOptBlockData *const blockData,
    BVSparse<JitArenaAllocator> *const mergedValueTypesTrackedForKills) const
{LOGMEIN("GlobOpt.cpp] 15611\n");
    Assert(value);
    Assert(blockData);

    ValueInfo *valueInfo = value->GetValueInfo();
    Assert(valueInfo->IsAnyOptimizedArray());

    const bool isJsArray = valueInfo->IsArrayOrObjectWithArray();
    Assert(!isJsArray == valueInfo->IsOptimizedTypedArray());

#if DBG
    VerifyArrayValueInfoForTracking(valueInfo, isJsArray, currentBlock, true);
#endif

    if(!isJsArray && !(valueInfo->IsArrayValueInfo() && valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym()))
    {LOGMEIN("GlobOpt.cpp] 15626\n");
        return;
    }

    // Can't assume going forward that it will definitely be an array without disabling implicit calls, because the
    // array may be transformed into an ES5 array. Since array opts are enabled, implicit calls can be disabled, and we can
    // treat it as a definite value type going forward, but the value needs to be tracked so that something like a call can
    // revert the value type to a likely version.
    if(!mergedValueTypesTrackedForKills || !mergedValueTypesTrackedForKills->TestAndSet(value->GetValueNumber()))
    {LOGMEIN("GlobOpt.cpp] 15635\n");
        blockData->valuesToKillOnCalls->Add(value);
    }
}

void
GlobOpt::TrackValueInfoChangeForKills(BasicBlock *const block, Value *const value, ValueInfo *const newValueInfo, const bool compensated) const
{LOGMEIN("GlobOpt.cpp] 15642\n");
    Assert(block);
    Assert(value);
    Assert(newValueInfo);

    ValueInfo *const oldValueInfo = value->GetValueInfo();

#if DBG
    if(oldValueInfo->IsAnyOptimizedArray())
    {
        VerifyArrayValueInfoForTracking(oldValueInfo, oldValueInfo->IsArrayOrObjectWithArray(), block, compensated);
    }
#endif

    const bool trackOldValueInfo =
        oldValueInfo->IsArrayOrObjectWithArray() ||
        (
            oldValueInfo->IsOptimizedTypedArray() &&
            oldValueInfo->IsArrayValueInfo() &&
            oldValueInfo->AsArrayValueInfo()->HeadSegmentLengthSym()
        );
    Assert(trackOldValueInfo == block->globOptData.valuesToKillOnCalls->ContainsKey(value));

#if DBG
    if(newValueInfo->IsAnyOptimizedArray())
    {
        VerifyArrayValueInfoForTracking(newValueInfo, newValueInfo->IsArrayOrObjectWithArray(), block, compensated);
    }
#endif

    const bool trackNewValueInfo =
        newValueInfo->IsArrayOrObjectWithArray() ||
        (
            newValueInfo->IsOptimizedTypedArray() &&
            newValueInfo->IsArrayValueInfo() &&
            newValueInfo->AsArrayValueInfo()->HeadSegmentLengthSym()
        );

    if(trackOldValueInfo == trackNewValueInfo)
    {LOGMEIN("GlobOpt.cpp] 15681\n");
        return;
    }

    if(trackNewValueInfo)
    {LOGMEIN("GlobOpt.cpp] 15686\n");
        block->globOptData.valuesToKillOnCalls->Add(value);
    }
    else
    {
        block->globOptData.valuesToKillOnCalls->Remove(value);
    }
}

void
GlobOpt::ProcessValueKills(IR::Instr *const instr)
{LOGMEIN("GlobOpt.cpp] 15697\n");
    Assert(instr);

    ValueSet *const valuesToKillOnCalls = blockData.valuesToKillOnCalls;
    if(!IsLoopPrePass() && valuesToKillOnCalls->Count() == 0)
    {LOGMEIN("GlobOpt.cpp] 15702\n");
        return;
    }

    const JsArrayKills kills = CheckJsArrayKills(instr);
    Assert(!kills.KillsArrayHeadSegments() || kills.KillsArrayHeadSegmentLengths());

    if(IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 15710\n");
        rootLoopPrePass->jsArrayKills = rootLoopPrePass->jsArrayKills.Merge(kills);
        Assert(
            !rootLoopPrePass->parent ||
            rootLoopPrePass->jsArrayKills.AreSubsetOf(rootLoopPrePass->parent->jsArrayKills));
        if(kills.KillsAllArrays())
        {LOGMEIN("GlobOpt.cpp] 15716\n");
            rootLoopPrePass->needImplicitCallBailoutChecksForJsArrayCheckHoist = false;
        }

        if(valuesToKillOnCalls->Count() == 0)
        {LOGMEIN("GlobOpt.cpp] 15721\n");
            return;
        }
    }

    if(kills.KillsAllArrays())
    {LOGMEIN("GlobOpt.cpp] 15727\n");
        Assert(kills.KillsTypedArrayHeadSegmentLengths());

        // - Calls need to kill the value types of values in the following list. For instance, calls can transform a JS array
        //   into an ES5 array, so any definitely-array value types need to be killed. Update the value types.
        // - Calls also need to kill typed array head segment lengths. A typed array's array buffer may be transferred to a web
        //   worker, in which case the typed array's length is set to zero.
        for(auto it = valuesToKillOnCalls->GetIterator(); it.IsValid(); it.MoveNext())
        {LOGMEIN("GlobOpt.cpp] 15735\n");
            Value *const value = it.CurrentValue();
            ValueInfo *const valueInfo = value->GetValueInfo();
            Assert(
                valueInfo->IsArrayOrObjectWithArray() ||
                valueInfo->IsOptimizedTypedArray() && valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym());
            if(valueInfo->IsArrayOrObjectWithArray())
            {
                ChangeValueType(nullptr, value, valueInfo->Type().ToLikely(), false);
                continue;
            }
            ChangeValueInfo(
                nullptr,
                value,
                valueInfo->AsArrayValueInfo()->Copy(alloc, true, false /* copyHeadSegmentLength */, true));
        }
        valuesToKillOnCalls->Clear();
        return;
    }

    if(kills.KillsArraysWithNoMissingValues())
    {LOGMEIN("GlobOpt.cpp] 15756\n");
        // Some operations may kill arrays with no missing values in unlikely circumstances. Convert their value types to likely
        // versions so that the checks have to be redone.
        for(auto it = valuesToKillOnCalls->GetIteratorWithRemovalSupport(); it.IsValid(); it.MoveNext())
        {LOGMEIN("GlobOpt.cpp] 15760\n");
            Value *const value = it.CurrentValue();
            ValueInfo *const valueInfo = value->GetValueInfo();
            Assert(
                valueInfo->IsArrayOrObjectWithArray() ||
                valueInfo->IsOptimizedTypedArray() && valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym());
            if(!valueInfo->IsArrayOrObjectWithArray() || !valueInfo->HasNoMissingValues())
            {LOGMEIN("GlobOpt.cpp] 15767\n");
                continue;
            }
            ChangeValueType(nullptr, value, valueInfo->Type().ToLikely(), false);
            it.RemoveCurrent();
        }
    }

    if(kills.KillsNativeArrays())
    {LOGMEIN("GlobOpt.cpp] 15776\n");
        // Some operations may kill native arrays in (what should be) unlikely circumstances. Convert their value types to
        // likely versions so that the checks have to be redone.
        for(auto it = valuesToKillOnCalls->GetIteratorWithRemovalSupport(); it.IsValid(); it.MoveNext())
        {LOGMEIN("GlobOpt.cpp] 15780\n");
            Value *const value = it.CurrentValue();
            ValueInfo *const valueInfo = value->GetValueInfo();
            Assert(
                valueInfo->IsArrayOrObjectWithArray() ||
                valueInfo->IsOptimizedTypedArray() && valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym());
            if(!valueInfo->IsArrayOrObjectWithArray() || valueInfo->HasVarElements())
            {LOGMEIN("GlobOpt.cpp] 15787\n");
                continue;
            }
            ChangeValueType(nullptr, value, valueInfo->Type().ToLikely(), false);
            it.RemoveCurrent();
        }
    }

    const bool likelyKillsJsArraysWithNoMissingValues = IsOperationThatLikelyKillsJsArraysWithNoMissingValues(instr);
    if(!kills.KillsArrayHeadSegmentLengths())
    {LOGMEIN("GlobOpt.cpp] 15797\n");
        Assert(!kills.KillsArrayHeadSegments());
        if(!likelyKillsJsArraysWithNoMissingValues && !kills.KillsArrayLengths())
        {LOGMEIN("GlobOpt.cpp] 15800\n");
            return;
        }
    }

    for(auto it = valuesToKillOnCalls->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOpt.cpp] 15806\n");
        Value *const value = it.CurrentValue();
        ValueInfo *valueInfo = value->GetValueInfo();
        Assert(
            valueInfo->IsArrayOrObjectWithArray() ||
            valueInfo->IsOptimizedTypedArray() && valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym());
        if(!valueInfo->IsArrayOrObjectWithArray())
        {LOGMEIN("GlobOpt.cpp] 15813\n");
            continue;
        }

        if(likelyKillsJsArraysWithNoMissingValues && valueInfo->HasNoMissingValues())
        {
            ChangeValueType(nullptr, value, valueInfo->Type().SetHasNoMissingValues(false), true);
            valueInfo = value->GetValueInfo();
        }

        if(!valueInfo->IsArrayValueInfo())
        {LOGMEIN("GlobOpt.cpp] 15824\n");
            continue;
        }

        ArrayValueInfo *const arrayValueInfo = valueInfo->AsArrayValueInfo();
        const bool removeHeadSegment = kills.KillsArrayHeadSegments() && arrayValueInfo->HeadSegmentSym();
        const bool removeHeadSegmentLength = kills.KillsArrayHeadSegmentLengths() && arrayValueInfo->HeadSegmentLengthSym();
        const bool removeLength = kills.KillsArrayLengths() && arrayValueInfo->LengthSym();
        if(removeHeadSegment || removeHeadSegmentLength || removeLength)
        {
            ChangeValueInfo(
                nullptr,
                value,
                arrayValueInfo->Copy(alloc, !removeHeadSegment, !removeHeadSegmentLength, !removeLength));
            valueInfo = value->GetValueInfo();
        }
    }
}

void
GlobOpt::ProcessValueKills(BasicBlock *const block, GlobOptBlockData *const blockData)
{LOGMEIN("GlobOpt.cpp] 15845\n");
    Assert(block);
    Assert(blockData);

    ValueSet *const valuesToKillOnCalls = blockData->valuesToKillOnCalls;
    if(!IsLoopPrePass() && valuesToKillOnCalls->Count() == 0)
    {LOGMEIN("GlobOpt.cpp] 15851\n");
        return;
    }

    // If the current block or loop has implicit calls, kill all definitely-array value types, as using that info will cause
    // implicit calls to be disabled, resulting in unnecessary bailouts
    const bool killValuesOnImplicitCalls =
        (block->loop ? !this->ImplicitCallFlagsAllowOpts(block->loop) : !this->ImplicitCallFlagsAllowOpts(func));
    if (!killValuesOnImplicitCalls)
    {LOGMEIN("GlobOpt.cpp] 15860\n");
        return;
    }

    if(IsLoopPrePass() && block->loop == rootLoopPrePass)
    {LOGMEIN("GlobOpt.cpp] 15865\n");
        AnalysisAssert(rootLoopPrePass);

        for (Loop * loop = rootLoopPrePass; loop != nullptr; loop = loop->parent)
        {LOGMEIN("GlobOpt.cpp] 15869\n");
            loop->jsArrayKills.SetKillsAllArrays();
        }
        Assert(!rootLoopPrePass->parent || rootLoopPrePass->jsArrayKills.AreSubsetOf(rootLoopPrePass->parent->jsArrayKills));

        if(valuesToKillOnCalls->Count() == 0)
        {LOGMEIN("GlobOpt.cpp] 15875\n");
            return;
        }
    }

    for(auto it = valuesToKillOnCalls->GetIterator(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOpt.cpp] 15881\n");
        Value *const value = it.CurrentValue();
        ValueInfo *const valueInfo = value->GetValueInfo();
        Assert(
            valueInfo->IsArrayOrObjectWithArray() ||
            valueInfo->IsOptimizedTypedArray() && valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym());
        if(valueInfo->IsArrayOrObjectWithArray())
        {
            ChangeValueType(nullptr, value, valueInfo->Type().ToLikely(), false);
            continue;
        }
        ChangeValueInfo(
            nullptr,
            value,
            valueInfo->AsArrayValueInfo()->Copy(alloc, true, false /* copyHeadSegmentLength */, true));
    }
    valuesToKillOnCalls->Clear();
}

void
GlobOpt::ProcessValueKillsForLoopHeaderAfterBackEdgeMerge(BasicBlock *const block, GlobOptBlockData *const blockData)
{LOGMEIN("GlobOpt.cpp] 15902\n");
    Assert(block);
    Assert(block->isLoopHeader);
    Assert(blockData);

    ValueSet *const valuesToKillOnCalls = blockData->valuesToKillOnCalls;
    if(valuesToKillOnCalls->Count() == 0)
    {LOGMEIN("GlobOpt.cpp] 15909\n");
        return;
    }

    const JsArrayKills loopKills(block->loop->jsArrayKills);
    for(auto it = valuesToKillOnCalls->GetIteratorWithRemovalSupport(); it.IsValid(); it.MoveNext())
    {LOGMEIN("GlobOpt.cpp] 15915\n");
        Value *const value = it.CurrentValue();
        ValueInfo *valueInfo = value->GetValueInfo();
        Assert(
            valueInfo->IsArrayOrObjectWithArray() ||
            valueInfo->IsOptimizedTypedArray() && valueInfo->AsArrayValueInfo()->HeadSegmentLengthSym());

        const bool isJsArray = valueInfo->IsArrayOrObjectWithArray();
        Assert(!isJsArray == valueInfo->IsOptimizedTypedArray());

        if(isJsArray ? loopKills.KillsValueType(valueInfo->Type()) : loopKills.KillsTypedArrayHeadSegmentLengths())
        {LOGMEIN("GlobOpt.cpp] 15926\n");
            // Hoisting array checks and other related things for this type is disabled for the loop due to the kill, as
            // compensation code is currently not added on back-edges. When merging values from a back-edge, the array value
            // type cannot be definite, as that may require adding compensation code on the back-edge if the optimization pass
            // chooses to not optimize the array.
            if(isJsArray)
            {
                ChangeValueType(nullptr, value, valueInfo->Type().ToLikely(), false);
            }
            else
            {
                ChangeValueInfo(
                    nullptr,
                    value,
                    valueInfo->AsArrayValueInfo()->Copy(alloc, true, false /* copyHeadSegmentLength */, true));
            }
            it.RemoveCurrent();
            continue;
        }

        if(!isJsArray || !valueInfo->IsArrayValueInfo())
        {LOGMEIN("GlobOpt.cpp] 15947\n");
            continue;
        }

        // Similarly, if the loop contains an operation that kills JS array segments, don't make the segment or other related
        // syms available initially inside the loop
        ArrayValueInfo *const arrayValueInfo = valueInfo->AsArrayValueInfo();
        const bool removeHeadSegment = loopKills.KillsArrayHeadSegments() && arrayValueInfo->HeadSegmentSym();
        const bool removeHeadSegmentLength = loopKills.KillsArrayHeadSegmentLengths() && arrayValueInfo->HeadSegmentLengthSym();
        const bool removeLength = loopKills.KillsArrayLengths() && arrayValueInfo->LengthSym();
        if(removeHeadSegment || removeHeadSegmentLength || removeLength)
        {
            ChangeValueInfo(
                nullptr,
                value,
                arrayValueInfo->Copy(alloc, !removeHeadSegment, !removeHeadSegmentLength, !removeLength));
            valueInfo = value->GetValueInfo();
        }
    }
}

bool
GlobOpt::NeedBailOnImplicitCallForLiveValues(BasicBlock *const block, const bool isForwardPass) const
{LOGMEIN("GlobOpt.cpp] 15970\n");
    if(isForwardPass)
    {LOGMEIN("GlobOpt.cpp] 15972\n");
        return block->globOptData.valuesToKillOnCalls->Count() != 0;
    }

    if(block->noImplicitCallUses->IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 15977\n");
        Assert(block->noImplicitCallNoMissingValuesUses->IsEmpty());
        Assert(block->noImplicitCallNativeArrayUses->IsEmpty());
        Assert(block->noImplicitCallJsArrayHeadSegmentSymUses->IsEmpty());
        Assert(block->noImplicitCallArrayLengthSymUses->IsEmpty());
        return false;
    }

    return true;
}

IR::Instr*
GlobOpt::CreateBoundsCheckInstr(IR::Opnd* lowerBound, IR::Opnd* upperBound, int offset, Func* func)
{LOGMEIN("GlobOpt.cpp] 15990\n");
    IR::Instr* instr = IR::Instr::New(Js::OpCode::BoundCheck, func);
    return AttachBoundsCheckData(instr, lowerBound, upperBound, offset);
}

IR::Instr*
GlobOpt::CreateBoundsCheckInstr(IR::Opnd* lowerBound, IR::Opnd* upperBound, int offset, IR::BailOutKind bailoutkind, BailOutInfo* bailoutInfo, Func * func)
{LOGMEIN("GlobOpt.cpp] 15997\n");
    IR::Instr* instr = IR::BailOutInstr::New(Js::OpCode::BoundCheck, bailoutkind, bailoutInfo, func);
    return AttachBoundsCheckData(instr, lowerBound, upperBound, offset);
}

IR::Instr*
GlobOpt::AttachBoundsCheckData(IR::Instr* instr, IR::Opnd* lowerBound, IR::Opnd* upperBound, int offset)
{LOGMEIN("GlobOpt.cpp] 16004\n");
    instr->SetSrc1(lowerBound);
    instr->SetSrc2(upperBound);
    if (offset != 0)
    {LOGMEIN("GlobOpt.cpp] 16008\n");
        instr->SetDst(IR::IntConstOpnd::New(offset, TyInt32, instr->m_func, true));
    }
    return instr;
}

void
GlobOpt::OptArraySrc(IR::Instr * *const instrRef)
{LOGMEIN("GlobOpt.cpp] 16016\n");
    Assert(instrRef);
    IR::Instr *&instr = *instrRef;
    Assert(instr);

    IR::Instr *baseOwnerInstr;
    IR::IndirOpnd *baseOwnerIndir;
    IR::RegOpnd *baseOpnd;
    bool isProfilableLdElem, isProfilableStElem;
    bool isLoad, isStore;
    bool needsHeadSegment, needsHeadSegmentLength, needsLength, needsBoundChecks;
    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 16028\n");
        // SIMD_JS
        case Js::OpCode::Simd128_LdArr_F4:
        case Js::OpCode::Simd128_LdArr_I4:
            // no type-spec for Asm.js
            if (this->GetIsAsmJSFunc())
            {LOGMEIN("GlobOpt.cpp] 16034\n");
                return;
            }
            // fall through
        case Js::OpCode::LdElemI_A:
        case Js::OpCode::LdMethodElem:
            if(!instr->GetSrc1()->IsIndirOpnd())
            {LOGMEIN("GlobOpt.cpp] 16041\n");
                return;
            }
            baseOwnerInstr = nullptr;
            baseOwnerIndir = instr->GetSrc1()->AsIndirOpnd();
            baseOpnd = baseOwnerIndir->GetBaseOpnd();
            isProfilableLdElem = instr->m_opcode == Js::OpCode::LdElemI_A; // LdMethodElem is currently not profiled
            isProfilableLdElem |= Js::IsSimd128Load(instr->m_opcode);
            needsBoundChecks = needsHeadSegmentLength = needsHeadSegment = isLoad = true;
            needsLength = isStore = isProfilableStElem = false;
            break;

        // SIMD_JS
        case Js::OpCode::Simd128_StArr_F4:
        case Js::OpCode::Simd128_StArr_I4:
            if (this->GetIsAsmJSFunc())
            {LOGMEIN("GlobOpt.cpp] 16057\n");
                return;
            }
            // fall through
        case Js::OpCode::StElemI_A:
        case Js::OpCode::StElemI_A_Strict:
        case Js::OpCode::StElemC:
            if(!instr->GetDst()->IsIndirOpnd())
            {LOGMEIN("GlobOpt.cpp] 16065\n");
                return;
            }
            baseOwnerInstr = nullptr;
            baseOwnerIndir = instr->GetDst()->AsIndirOpnd();
            baseOpnd = baseOwnerIndir->GetBaseOpnd();
            needsBoundChecks = isProfilableStElem = instr->m_opcode != Js::OpCode::StElemC;
            isProfilableStElem |= Js::IsSimd128Store(instr->m_opcode);
            needsHeadSegmentLength = needsHeadSegment = isStore = true;
            needsLength = isLoad = isProfilableLdElem = false;
            break;

        case Js::OpCode::InlineArrayPush:
        case Js::OpCode::InlineArrayPop:
        {LOGMEIN("GlobOpt.cpp] 16079\n");
            baseOwnerInstr = instr;
            baseOwnerIndir = nullptr;
            IR::Opnd * thisOpnd = instr->GetSrc1();

            // Return if it not a LikelyArray or Object with Array - No point in doing array check elimination.
            if(!thisOpnd->IsRegOpnd() || !thisOpnd->GetValueType().IsLikelyArrayOrObjectWithArray())
            {LOGMEIN("GlobOpt.cpp] 16086\n");
                return;
            }
            baseOpnd = thisOpnd->AsRegOpnd();

            isLoad = instr->m_opcode == Js::OpCode::InlineArrayPop;
            isStore = instr->m_opcode == Js::OpCode::InlineArrayPush;
            needsLength = needsHeadSegmentLength = needsHeadSegment = true;
            needsBoundChecks = isProfilableLdElem = isProfilableStElem = false;
            break;
        }

        case Js::OpCode::LdLen_A:
            if(!instr->GetSrc1()->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 16100\n");
                return;
            }
            baseOwnerInstr = instr;
            baseOwnerIndir = nullptr;
            baseOpnd = instr->GetSrc1()->AsRegOpnd();
            if(baseOpnd->GetValueType().IsLikelyObject() &&
                baseOpnd->GetValueType().GetObjectType() == ObjectType::ObjectWithArray)
            {LOGMEIN("GlobOpt.cpp] 16108\n");
                return;
            }
            needsLength = true;
            needsBoundChecks =
                needsHeadSegmentLength =
                needsHeadSegment =
                isStore =
                isLoad =
                isProfilableStElem =
                isProfilableLdElem = false;
            break;

        default:
            return;
    }
    Assert(!(baseOwnerInstr && baseOwnerIndir));
    Assert(!needsHeadSegmentLength || needsHeadSegment);

    if(baseOwnerIndir && !IsLoopPrePass())
    {
        // Since this happens before type specialization, make sure that any necessary conversions are done, and that the index
        // is int-specialized if possible such that the const flags are correct.
        ToVarUses(instr, baseOwnerIndir, baseOwnerIndir == instr->GetDst(), nullptr);
    }

    if(isProfilableStElem && !IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 16135\n");
        // If the dead-store pass decides to add the bailout kind IR::BailOutInvalidatedArrayHeadSegment, and the fast path is
        // generated, it may bail out before the operation is done, so this would need to be a pre-op bailout.
        if(instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 16139\n");
            Assert(
                instr->GetByteCodeOffset() != Js::Constants::NoByteCodeOffset &&
                instr->GetBailOutInfo()->bailOutOffset <= instr->GetByteCodeOffset());

            const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
            Assert(
                !(bailOutKind & ~IR::BailOutKindBits) ||
                (bailOutKind & ~IR::BailOutKindBits) == IR::BailOutOnImplicitCallsPreOp);
            if(!(bailOutKind & ~IR::BailOutKindBits))
            {LOGMEIN("GlobOpt.cpp] 16149\n");
                instr->SetBailOutKind(bailOutKind + IR::BailOutOnImplicitCallsPreOp);
            }
        }
        else
        {
            GenerateBailAtOperation(&instr, IR::BailOutOnImplicitCallsPreOp);
        }
    }

    Value *const baseValue = FindValue(baseOpnd->m_sym);
    if(!baseValue)
    {LOGMEIN("GlobOpt.cpp] 16161\n");
        return;
    }
    ValueInfo *baseValueInfo = baseValue->GetValueInfo();
    ValueType baseValueType(baseValueInfo->Type());

    baseOpnd->SetValueType(baseValueType);
    if(!baseValueType.IsLikelyAnyOptimizedArray() ||
        !DoArrayCheckHoist(baseValueType, currentBlock->loop, instr) ||
        (baseOwnerIndir && !ShouldExpectConventionalArrayIndexValue(baseOwnerIndir)))
    {LOGMEIN("GlobOpt.cpp] 16171\n");
        return;
    }

    const bool isLikelyJsArray = !baseValueType.IsLikelyTypedArray();
    Assert(isLikelyJsArray == baseValueType.IsLikelyArrayOrObjectWithArray());
    Assert(!isLikelyJsArray == baseValueType.IsLikelyOptimizedTypedArray());
    if(!isLikelyJsArray && instr->m_opcode == Js::OpCode::LdMethodElem)
    {LOGMEIN("GlobOpt.cpp] 16179\n");
        // Fast path is not generated in this case since the subsequent call will throw
        return;
    }

    ValueType newBaseValueType(baseValueType.ToDefiniteObject());
    if(isLikelyJsArray && newBaseValueType.HasNoMissingValues() && !DoArrayMissingValueCheckHoist())
    {LOGMEIN("GlobOpt.cpp] 16186\n");
        newBaseValueType = newBaseValueType.SetHasNoMissingValues(false);
    }
    Assert((newBaseValueType == baseValueType) == baseValueType.IsObject());

    ArrayValueInfo *baseArrayValueInfo = nullptr;
    const auto UpdateValue = [&](StackSym *newHeadSegmentSym, StackSym *newHeadSegmentLengthSym, StackSym *newLengthSym)
    {LOGMEIN("GlobOpt.cpp] 16193\n");
        Assert(baseValueType.GetObjectType() == newBaseValueType.GetObjectType());
        Assert(newBaseValueType.IsObject());
        Assert(baseValueType.IsLikelyArray() || !newLengthSym);

        if(!(newHeadSegmentSym || newHeadSegmentLengthSym || newLengthSym))
        {
            // We're not adding new information to the value other than changing the value type. Preserve any existing
            // information and just change the value type.
            ChangeValueType(currentBlock, baseValue, newBaseValueType, true);
            return;
        }

        // Merge the new syms into the value while preserving any existing information, and change the value type
        if(baseArrayValueInfo)
        {LOGMEIN("GlobOpt.cpp] 16208\n");
            if(!newHeadSegmentSym)
            {LOGMEIN("GlobOpt.cpp] 16210\n");
                newHeadSegmentSym = baseArrayValueInfo->HeadSegmentSym();
            }
            if(!newHeadSegmentLengthSym)
            {LOGMEIN("GlobOpt.cpp] 16214\n");
                newHeadSegmentLengthSym = baseArrayValueInfo->HeadSegmentLengthSym();
            }
            if(!newLengthSym)
            {LOGMEIN("GlobOpt.cpp] 16218\n");
                newLengthSym = baseArrayValueInfo->LengthSym();
            }
            Assert(
                !baseArrayValueInfo->HeadSegmentSym() ||
                newHeadSegmentSym == baseArrayValueInfo->HeadSegmentSym());
            Assert(
                !baseArrayValueInfo->HeadSegmentLengthSym() ||
                newHeadSegmentLengthSym == baseArrayValueInfo->HeadSegmentLengthSym());
            Assert(!baseArrayValueInfo->LengthSym() || newLengthSym == baseArrayValueInfo->LengthSym());
        }
        ArrayValueInfo *const newBaseArrayValueInfo =
            ArrayValueInfo::New(
                alloc,
                newBaseValueType,
                newHeadSegmentSym,
                newHeadSegmentLengthSym,
                newLengthSym,
                baseValueInfo->GetSymStore());
        ChangeValueInfo(currentBlock, baseValue, newBaseArrayValueInfo);
    };

    if(IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 16241\n");
        if(newBaseValueType != baseValueType)
        {
            UpdateValue(nullptr, nullptr, nullptr);
        }

        // For javascript arrays and objects with javascript arrays:
        //   - Implicit calls need to be disabled and calls cannot be allowed in the loop since the array vtable may be changed
        //     into an ES5 array.
        // For typed arrays:
        //   - A typed array's array buffer may be transferred to a web worker as part of an implicit call, in which case the
        //     typed array's length is set to zero. Implicit calls need to be disabled if the typed array's head segment length
        //     is going to be loaded and used later.
        // Since we don't know if the loop has kills after this instruction, the kill information may not be complete. If a kill
        // is found later, this information will be updated to not require disabling implicit calls.
        if(!(
                isLikelyJsArray
                    ? rootLoopPrePass->jsArrayKills.KillsValueType(newBaseValueType)
                    : rootLoopPrePass->jsArrayKills.KillsTypedArrayHeadSegmentLengths()
            ))
        {LOGMEIN("GlobOpt.cpp] 16261\n");
            rootLoopPrePass->needImplicitCallBailoutChecksForJsArrayCheckHoist = true;
        }
        return;
    }

    if(baseValueInfo->IsArrayValueInfo())
    {LOGMEIN("GlobOpt.cpp] 16268\n");
        baseArrayValueInfo = baseValueInfo->AsArrayValueInfo();
    }

    const bool doArrayChecks = !baseValueType.IsObject();
    const bool doArraySegmentHoist = DoArraySegmentHoist(baseValueType) && instr->m_opcode != Js::OpCode::StElemC;
    const bool headSegmentIsAvailable = baseArrayValueInfo && baseArrayValueInfo->HeadSegmentSym();
    const bool doHeadSegmentLoad = doArraySegmentHoist && needsHeadSegment && !headSegmentIsAvailable;
    const bool doArraySegmentLengthHoist =
        doArraySegmentHoist && (isLikelyJsArray || DoTypedArraySegmentLengthHoist(currentBlock->loop));
    const bool headSegmentLengthIsAvailable = baseArrayValueInfo && baseArrayValueInfo->HeadSegmentLengthSym();
    const bool doHeadSegmentLengthLoad =
        doArraySegmentLengthHoist &&
        (needsHeadSegmentLength || (!isLikelyJsArray && needsLength)) &&
        !headSegmentLengthIsAvailable;
    const bool lengthIsAvailable = baseArrayValueInfo && baseArrayValueInfo->LengthSym();
    const bool doLengthLoad =
        DoArrayLengthHoist() &&
        needsLength &&
        !lengthIsAvailable &&
        baseValueType.IsLikelyArray() &&
        DoLdLenIntSpec(instr->m_opcode == Js::OpCode::LdLen_A ? instr : nullptr, baseValueType);
    StackSym *const newHeadSegmentSym = doHeadSegmentLoad ? StackSym::New(TyMachPtr, instr->m_func) : nullptr;
    StackSym *const newHeadSegmentLengthSym = doHeadSegmentLengthLoad ? StackSym::New(TyUint32, instr->m_func) : nullptr;
    StackSym *const newLengthSym = doLengthLoad ? StackSym::New(TyUint32, instr->m_func) : nullptr;

    bool canBailOutOnArrayAccessHelperCall;

    if (Js::IsSimd128LoadStore(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 16297\n");
        // SIMD_JS
        // simd load/store never call helper
        canBailOutOnArrayAccessHelperCall = true;
    }
    else
    {
        canBailOutOnArrayAccessHelperCall = (isProfilableLdElem || isProfilableStElem) &&
        DoEliminateArrayAccessHelperCall() &&
        !(
            instr->IsProfiledInstr() &&
            (
                isProfilableLdElem
                    ? instr->AsProfiledInstr()->u.ldElemInfo->LikelyNeedsHelperCall()
                    : instr->AsProfiledInstr()->u.stElemInfo->LikelyNeedsHelperCall()
            )
         );
    }

    bool doExtractBoundChecks = false, eliminatedLowerBoundCheck = false, eliminatedUpperBoundCheck = false;
    StackSym *indexVarSym = nullptr;
    Value *indexValue = nullptr;
    IntConstantBounds indexConstantBounds;
    Value *headSegmentLengthValue = nullptr;
    IntConstantBounds headSegmentLengthConstantBounds;

    if (baseValueType.IsLikelyOptimizedVirtualTypedArray() && !Js::IsSimd128LoadStore(instr->m_opcode) /*Always extract bounds for SIMD */)
    {LOGMEIN("GlobOpt.cpp] 16324\n");
        if (isProfilableStElem ||
            !instr->IsDstNotAlwaysConvertedToInt32() ||
            ( (baseValueType.GetObjectType() == ObjectType::Float32VirtualArray ||
              baseValueType.GetObjectType() == ObjectType::Float64VirtualArray) &&
              !instr->IsDstNotAlwaysConvertedToNumber()
            )
           )
        {LOGMEIN("GlobOpt.cpp] 16332\n");
            eliminatedLowerBoundCheck = true;
            eliminatedUpperBoundCheck = true;
            canBailOutOnArrayAccessHelperCall = false;
        }
    }

    if(needsBoundChecks && DoBoundCheckElimination())
    {LOGMEIN("GlobOpt.cpp] 16340\n");
        AnalysisAssert(baseOwnerIndir);
        Assert(needsHeadSegmentLength);

        // Bound checks can be separated from the instruction only if it can bail out instead of making a helper call when a
        // bound check fails. And only if it would bail out, can we use a bound check to eliminate redundant bound checks later
        // on that path.
        doExtractBoundChecks = (headSegmentLengthIsAvailable || doHeadSegmentLengthLoad) && canBailOutOnArrayAccessHelperCall;

        do
        {LOGMEIN("GlobOpt.cpp] 16350\n");
            // Get the index value
            IR::RegOpnd *const indexOpnd = baseOwnerIndir->GetIndexOpnd();
            if(indexOpnd)
            {LOGMEIN("GlobOpt.cpp] 16354\n");
                StackSym *const indexSym = indexOpnd->m_sym;
                if(indexSym->IsTypeSpec())
                {LOGMEIN("GlobOpt.cpp] 16357\n");
                    Assert(indexSym->IsInt32());
                    indexVarSym = indexSym->GetVarEquivSym(nullptr);
                    Assert(indexVarSym);
                    indexValue = FindValue(indexVarSym);
                    Assert(indexValue);
                    AssertVerify(indexValue->GetValueInfo()->TryGetIntConstantBounds(&indexConstantBounds));
                    Assert(indexOpnd->GetType() == TyInt32 || indexOpnd->GetType() == TyUint32);
                    Assert(
                        (indexOpnd->GetType() == TyUint32) ==
                        ValueInfo::IsGreaterThanOrEqualTo(
                            indexValue,
                            indexConstantBounds.LowerBound(),
                            indexConstantBounds.UpperBound(),
                            nullptr,
                            0,
                            0));
                    if(indexOpnd->GetType() == TyUint32)
                    {LOGMEIN("GlobOpt.cpp] 16375\n");
                        eliminatedLowerBoundCheck = true;
                    }
                }
                else
                {
                    doExtractBoundChecks = false; // Bound check instruction operates only on int-specialized operands
                    indexValue = FindValue(indexSym);
                    if(!indexValue || !indexValue->GetValueInfo()->TryGetIntConstantBounds(&indexConstantBounds))
                    {LOGMEIN("GlobOpt.cpp] 16384\n");
                        break;
                    }
                    if(ValueInfo::IsGreaterThanOrEqualTo(
                            indexValue,
                            indexConstantBounds.LowerBound(),
                            indexConstantBounds.UpperBound(),
                            nullptr,
                            0,
                            0))
                    {LOGMEIN("GlobOpt.cpp] 16394\n");
                        eliminatedLowerBoundCheck = true;
                    }
                }
                if(!eliminatedLowerBoundCheck &&
                    ValueInfo::IsLessThan(
                        indexValue,
                        indexConstantBounds.LowerBound(),
                        indexConstantBounds.UpperBound(),
                        nullptr,
                        0,
                        0))
                {LOGMEIN("GlobOpt.cpp] 16406\n");
                    eliminatedUpperBoundCheck = true;
                    doExtractBoundChecks = false;
                    break;
                }
            }
            else
            {
                const int32 indexConstantValue = baseOwnerIndir->GetOffset();
                if(indexConstantValue < 0)
                {LOGMEIN("GlobOpt.cpp] 16416\n");
                    eliminatedUpperBoundCheck = true;
                    doExtractBoundChecks = false;
                    break;
                }
                if(indexConstantValue == INT32_MAX)
                {LOGMEIN("GlobOpt.cpp] 16422\n");
                    eliminatedLowerBoundCheck = true;
                    doExtractBoundChecks = false;
                    break;
                }
                indexConstantBounds = IntConstantBounds(indexConstantValue, indexConstantValue);
                eliminatedLowerBoundCheck = true;
            }

            if(!headSegmentLengthIsAvailable)
            {LOGMEIN("GlobOpt.cpp] 16432\n");
                break;
            }
            headSegmentLengthValue = FindValue(baseArrayValueInfo->HeadSegmentLengthSym());
            if(!headSegmentLengthValue)
            {LOGMEIN("GlobOpt.cpp] 16437\n");
                if(doExtractBoundChecks)
                {LOGMEIN("GlobOpt.cpp] 16439\n");
                    headSegmentLengthConstantBounds = IntConstantBounds(0, Js::SparseArraySegmentBase::MaxLength);
                }
                break;
            }
            AssertVerify(headSegmentLengthValue->GetValueInfo()->TryGetIntConstantBounds(&headSegmentLengthConstantBounds));

            if (ValueInfo::IsLessThanOrEqualTo(
                    indexValue,
                    indexConstantBounds.LowerBound(),
                    indexConstantBounds.UpperBound(),
                    headSegmentLengthValue,
                    headSegmentLengthConstantBounds.LowerBound(),
                    headSegmentLengthConstantBounds.UpperBound(),
                    GetBoundCheckOffsetForSimd(newBaseValueType, instr, -1)
                    ))
            {LOGMEIN("GlobOpt.cpp] 16455\n");
                eliminatedUpperBoundCheck = true;
                if(eliminatedLowerBoundCheck)
                {LOGMEIN("GlobOpt.cpp] 16458\n");
                    doExtractBoundChecks = false;
                }
            }
        } while(false);
    }

    if(doArrayChecks || doHeadSegmentLoad || doHeadSegmentLengthLoad || doLengthLoad || doExtractBoundChecks)
    {LOGMEIN("GlobOpt.cpp] 16466\n");
        // Find the loops out of which array checks and head segment loads need to be hoisted
        Loop *hoistChecksOutOfLoop = nullptr;
        Loop *hoistHeadSegmentLoadOutOfLoop = nullptr;
        Loop *hoistHeadSegmentLengthLoadOutOfLoop = nullptr;
        Loop *hoistLengthLoadOutOfLoop = nullptr;
        if(doArrayChecks || doHeadSegmentLoad || doHeadSegmentLengthLoad || doLengthLoad)
        {LOGMEIN("GlobOpt.cpp] 16473\n");
            for(Loop *loop = currentBlock->loop; loop; loop = loop->parent)
            {LOGMEIN("GlobOpt.cpp] 16475\n");
                const JsArrayKills loopKills(loop->jsArrayKills);
                Value *baseValueInLoopLandingPad;
                if((isLikelyJsArray && loopKills.KillsValueType(newBaseValueType)) ||
                    !OptIsInvariant(baseOpnd->m_sym, currentBlock, loop, baseValue, true, true, &baseValueInLoopLandingPad) ||
                    !(doArrayChecks || baseValueInLoopLandingPad->GetValueInfo()->IsObject()))
                {LOGMEIN("GlobOpt.cpp] 16481\n");
                    break;
                }

                // The value types should be the same, except:
                //     - The value type in the landing pad is a type that can merge to a specific object type. Typically, these
                //       cases will use BailOnNoProfile, but that can be disabled due to excessive bailouts. Those value types
                //       merge aggressively to the other side's object type, so the value type may have started off as
                //       Uninitialized, [Likely]Undefined|Null, [Likely]UninitializedObject, etc., and changed in the loop to an
                //       array type during a prepass.
                //     - StElems in the loop can kill the no-missing-values info.
                //     - The native array type may be made more conservative based on profile data by an instruction in the loop.
                Assert(
                    baseValueInLoopLandingPad->GetValueInfo()->CanMergeToSpecificObjectType() ||
                    baseValueInLoopLandingPad->GetValueInfo()->Type().SetCanBeTaggedValue(false) ==
                        baseValueType.SetCanBeTaggedValue(false) ||
                    baseValueInLoopLandingPad->GetValueInfo()->Type().SetHasNoMissingValues(false).SetCanBeTaggedValue(false) ==
                        baseValueType.SetHasNoMissingValues(false).SetCanBeTaggedValue(false) ||
                    baseValueInLoopLandingPad->GetValueInfo()->Type().SetHasNoMissingValues(false).ToLikely().SetCanBeTaggedValue(false) ==
                        baseValueType.SetHasNoMissingValues(false).SetCanBeTaggedValue(false) ||
                    (
                        baseValueInLoopLandingPad->GetValueInfo()->Type().IsLikelyNativeArray() &&
                        baseValueInLoopLandingPad->GetValueInfo()->Type().Merge(baseValueType).SetHasNoMissingValues(false).SetCanBeTaggedValue(false) ==
                            baseValueType.SetHasNoMissingValues(false).SetCanBeTaggedValue(false)
                    ));

                if(doArrayChecks)
                {LOGMEIN("GlobOpt.cpp] 16508\n");
                    hoistChecksOutOfLoop = loop;
                }

                if(isLikelyJsArray && loopKills.KillsArrayHeadSegments())
                {LOGMEIN("GlobOpt.cpp] 16513\n");
                    Assert(loopKills.KillsArrayHeadSegmentLengths());
                    if(!(doArrayChecks || doLengthLoad))
                    {LOGMEIN("GlobOpt.cpp] 16516\n");
                        break;
                    }
                }
                else
                {
                    if(doHeadSegmentLoad || headSegmentIsAvailable)
                    {LOGMEIN("GlobOpt.cpp] 16523\n");
                        // If the head segment is already available, we may need to rehoist the value including other
                        // information. So, need to track the loop out of which the head segment length can be hoisted even if
                        // the head segment length is not being loaded here.
                        hoistHeadSegmentLoadOutOfLoop = loop;
                    }

                    if(isLikelyJsArray
                            ? loopKills.KillsArrayHeadSegmentLengths()
                            : loopKills.KillsTypedArrayHeadSegmentLengths())
                    {LOGMEIN("GlobOpt.cpp] 16533\n");
                        if(!(doArrayChecks || doHeadSegmentLoad || doLengthLoad))
                        {LOGMEIN("GlobOpt.cpp] 16535\n");
                            break;
                        }
                    }
                    else if(doHeadSegmentLengthLoad || headSegmentLengthIsAvailable)
                    {LOGMEIN("GlobOpt.cpp] 16540\n");
                        // If the head segment length is already available, we may need to rehoist the value including other
                        // information. So, need to track the loop out of which the head segment length can be hoisted even if
                        // the head segment length is not being loaded here.
                        hoistHeadSegmentLengthLoadOutOfLoop = loop;
                    }
                }

                if(isLikelyJsArray && loopKills.KillsArrayLengths())
                {LOGMEIN("GlobOpt.cpp] 16549\n");
                    if(!(doArrayChecks || doHeadSegmentLoad || doHeadSegmentLengthLoad))
                    {LOGMEIN("GlobOpt.cpp] 16551\n");
                        break;
                    }
                }
                else if(doLengthLoad || lengthIsAvailable)
                {LOGMEIN("GlobOpt.cpp] 16556\n");
                    // If the length is already available, we may need to rehoist the value including other information. So,
                    // need to track the loop out of which the head segment length can be hoisted even if the length is not
                    // being loaded here.
                    hoistLengthLoadOutOfLoop = loop;
                }
            }
        }

        IR::Instr *insertBeforeInstr = instr->GetInsertBeforeByteCodeUsesInstr();
        const auto InsertInstrInLandingPad = [&](IR::Instr *const instr, Loop *const hoistOutOfLoop)
        {LOGMEIN("GlobOpt.cpp] 16567\n");
            if(hoistOutOfLoop->bailOutInfo->bailOutInstr)
            {LOGMEIN("GlobOpt.cpp] 16569\n");
                instr->SetByteCodeOffset(hoistOutOfLoop->bailOutInfo->bailOutInstr);
                hoistOutOfLoop->bailOutInfo->bailOutInstr->InsertBefore(instr);
            }
            else
            {
                instr->SetByteCodeOffset(hoistOutOfLoop->landingPad->GetLastInstr());
                hoistOutOfLoop->landingPad->InsertAfter(instr);
            }
        };

        BailOutInfo *shareableBailOutInfo = nullptr;
        IR::Instr *shareableBailOutInfoOriginalOwner = nullptr;
        const auto ShareBailOut = [&]()
        {
            Assert(shareableBailOutInfo);
            if(shareableBailOutInfo->bailOutInstr != shareableBailOutInfoOriginalOwner)
            {LOGMEIN("GlobOpt.cpp] 16586\n");
                return;
            }

            Assert(shareableBailOutInfoOriginalOwner->GetBailOutInfo() == shareableBailOutInfo);
            IR::Instr *const sharedBailOut = shareableBailOutInfoOriginalOwner->ShareBailOut();
            Assert(sharedBailOut->GetBailOutInfo() == shareableBailOutInfo);
            shareableBailOutInfoOriginalOwner = nullptr;
            sharedBailOut->Unlink();
            insertBeforeInstr->InsertBefore(sharedBailOut);
            insertBeforeInstr = sharedBailOut;
        };

        if(doArrayChecks)
        {LOGMEIN("GlobOpt.cpp] 16600\n");
            TRACE_TESTTRACE_PHASE_INSTR(Js::ArrayCheckHoistPhase, instr, _u("Separating array checks with bailout\n"));

            IR::Instr *bailOnNotArray = IR::Instr::New(Js::OpCode::BailOnNotArray, instr->m_func);
            bailOnNotArray->SetSrc1(baseOpnd);
            bailOnNotArray->GetSrc1()->SetIsJITOptimizedReg(true);
            const IR::BailOutKind bailOutKind =
                newBaseValueType.IsLikelyNativeArray() ? IR::BailOutOnNotNativeArray : IR::BailOutOnNotArray;
            if(hoistChecksOutOfLoop)
            {LOGMEIN("GlobOpt.cpp] 16609\n");
                Assert(!(isLikelyJsArray && hoistChecksOutOfLoop->jsArrayKills.KillsValueType(newBaseValueType)));

                TRACE_PHASE_INSTR(
                    Js::ArrayCheckHoistPhase,
                    instr,
                    _u("Hoisting array checks with bailout out of loop %u to landing pad block %u\n"),
                    hoistChecksOutOfLoop->GetLoopNumber(),
                    hoistChecksOutOfLoop->landingPad->GetBlockNum());
                TESTTRACE_PHASE_INSTR(Js::ArrayCheckHoistPhase, instr, _u("Hoisting array checks with bailout out of loop\n"));

                Assert(hoistChecksOutOfLoop->bailOutInfo);
                EnsureBailTarget(hoistChecksOutOfLoop);
                InsertInstrInLandingPad(bailOnNotArray, hoistChecksOutOfLoop);
                bailOnNotArray = bailOnNotArray->ConvertToBailOutInstr(hoistChecksOutOfLoop->bailOutInfo, bailOutKind);
            }
            else
            {
                bailOnNotArray->SetByteCodeOffset(instr);
                insertBeforeInstr->InsertBefore(bailOnNotArray);
                GenerateBailAtOperation(&bailOnNotArray, bailOutKind);
                shareableBailOutInfo = bailOnNotArray->GetBailOutInfo();
                shareableBailOutInfoOriginalOwner = bailOnNotArray;
            }

            baseValueType = newBaseValueType;
            baseOpnd->SetValueType(newBaseValueType);
        }

        if(doLengthLoad)
        {LOGMEIN("GlobOpt.cpp] 16639\n");
            Assert(baseValueType.IsArray());
            Assert(newLengthSym);

            TRACE_TESTTRACE_PHASE_INSTR(Js::Phase::ArrayLengthHoistPhase, instr, _u("Separating array length load\n"));

            // Create an initial value for the length
            blockData.liveVarSyms->Set(newLengthSym->m_id);
            Value *const lengthValue = NewIntRangeValue(0, INT32_MAX, false);
            SetValue(&blockData, lengthValue, newLengthSym);

            // SetValue above would have set the sym store to newLengthSym. This sym won't be used for copy-prop though, so
            // remove it as the sym store.
            this->SetSymStoreDirect(lengthValue->GetValueInfo(), nullptr);

            // length = [array + offsetOf(length)]
            IR::Instr *const loadLength =
                IR::Instr::New(
                    Js::OpCode::LdIndir,
                    IR::RegOpnd::New(newLengthSym, newLengthSym->GetType(), instr->m_func),
                    IR::IndirOpnd::New(
                        baseOpnd,
                        Js::JavascriptArray::GetOffsetOfLength(),
                        newLengthSym->GetType(),
                        instr->m_func),
                    instr->m_func);
            loadLength->GetDst()->SetIsJITOptimizedReg(true);
            loadLength->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->SetIsJITOptimizedReg(true);

            // BailOnNegative length (BailOutOnIrregularLength)
            IR::Instr *bailOnIrregularLength = IR::Instr::New(Js::OpCode::BailOnNegative, instr->m_func);
            bailOnIrregularLength->SetSrc1(loadLength->GetDst());

            const IR::BailOutKind bailOutKind = IR::BailOutOnIrregularLength;
            if(hoistLengthLoadOutOfLoop)
            {LOGMEIN("GlobOpt.cpp] 16674\n");
                Assert(!hoistLengthLoadOutOfLoop->jsArrayKills.KillsArrayLengths());

                TRACE_PHASE_INSTR(
                    Js::Phase::ArrayLengthHoistPhase,
                    instr,
                    _u("Hoisting array length load out of loop %u to landing pad block %u\n"),
                    hoistLengthLoadOutOfLoop->GetLoopNumber(),
                    hoistLengthLoadOutOfLoop->landingPad->GetBlockNum());
                TESTTRACE_PHASE_INSTR(Js::Phase::ArrayLengthHoistPhase, instr, _u("Hoisting array length load out of loop\n"));

                Assert(hoistLengthLoadOutOfLoop->bailOutInfo);
                EnsureBailTarget(hoistLengthLoadOutOfLoop);
                InsertInstrInLandingPad(loadLength, hoistLengthLoadOutOfLoop);
                InsertInstrInLandingPad(bailOnIrregularLength, hoistLengthLoadOutOfLoop);
                bailOnIrregularLength =
                    bailOnIrregularLength->ConvertToBailOutInstr(hoistLengthLoadOutOfLoop->bailOutInfo, bailOutKind);

                // Hoist the length value
                for(InvariantBlockBackwardIterator it(
                        this,
                        currentBlock,
                        hoistLengthLoadOutOfLoop->landingPad,
                        baseOpnd->m_sym,
                        baseValue->GetValueNumber());
                    it.IsValid();
                    it.MoveNext())
                {LOGMEIN("GlobOpt.cpp] 16701\n");
                    BasicBlock *const block = it.Block();
                    block->globOptData.liveVarSyms->Set(newLengthSym->m_id);
                    Assert(!FindValue(block->globOptData.symToValueMap, newLengthSym));
                    Value *const lengthValueCopy = CopyValue(lengthValue, lengthValue->GetValueNumber());
                    SetValue(&block->globOptData, lengthValueCopy, newLengthSym);
                    this->SetSymStoreDirect(lengthValueCopy->GetValueInfo(), nullptr);
                }
            }
            else
            {
                loadLength->SetByteCodeOffset(instr);
                insertBeforeInstr->InsertBefore(loadLength);
                bailOnIrregularLength->SetByteCodeOffset(instr);
                insertBeforeInstr->InsertBefore(bailOnIrregularLength);
                if(shareableBailOutInfo)
                {LOGMEIN("GlobOpt.cpp] 16717\n");
                    ShareBailOut();
                    bailOnIrregularLength = bailOnIrregularLength->ConvertToBailOutInstr(shareableBailOutInfo, bailOutKind);
                }
                else
                {
                    GenerateBailAtOperation(&bailOnIrregularLength, bailOutKind);
                    shareableBailOutInfo = bailOnIrregularLength->GetBailOutInfo();
                    shareableBailOutInfoOriginalOwner = bailOnIrregularLength;
                }
            }
        }

        const auto InsertHeadSegmentLoad = [&]()
        {
            TRACE_TESTTRACE_PHASE_INSTR(Js::ArraySegmentHoistPhase, instr, _u("Separating array segment load\n"));

            Assert(newHeadSegmentSym);
            IR::RegOpnd *const headSegmentOpnd =
                IR::RegOpnd::New(newHeadSegmentSym, newHeadSegmentSym->GetType(), instr->m_func);
            headSegmentOpnd->SetIsJITOptimizedReg(true);
            IR::RegOpnd *const jitOptimizedBaseOpnd = baseOpnd->Copy(instr->m_func)->AsRegOpnd();
            jitOptimizedBaseOpnd->SetIsJITOptimizedReg(true);
            IR::Instr *loadObjectArray;
            if(baseValueType.GetObjectType() == ObjectType::ObjectWithArray)
            {LOGMEIN("GlobOpt.cpp] 16742\n");
                loadObjectArray =
                    IR::Instr::New(
                        Js::OpCode::LdIndir,
                        headSegmentOpnd,
                        IR::IndirOpnd::New(
                            jitOptimizedBaseOpnd,
                            Js::DynamicObject::GetOffsetOfObjectArray(),
                            jitOptimizedBaseOpnd->GetType(),
                            instr->m_func),
                        instr->m_func);
            }
            else
            {
                loadObjectArray = nullptr;
            }
            IR::Instr *const loadHeadSegment =
                IR::Instr::New(
                    Js::OpCode::LdIndir,
                    headSegmentOpnd,
                    IR::IndirOpnd::New(
                        loadObjectArray ? headSegmentOpnd : jitOptimizedBaseOpnd,
                        Lowerer::GetArrayOffsetOfHeadSegment(baseValueType),
                        headSegmentOpnd->GetType(),
                        instr->m_func),
                    instr->m_func);
            if(hoistHeadSegmentLoadOutOfLoop)
            {LOGMEIN("GlobOpt.cpp] 16769\n");
                Assert(!(isLikelyJsArray && hoistHeadSegmentLoadOutOfLoop->jsArrayKills.KillsArrayHeadSegments()));

                TRACE_PHASE_INSTR(
                    Js::ArraySegmentHoistPhase,
                    instr,
                    _u("Hoisting array segment load out of loop %u to landing pad block %u\n"),
                    hoistHeadSegmentLoadOutOfLoop->GetLoopNumber(),
                    hoistHeadSegmentLoadOutOfLoop->landingPad->GetBlockNum());
                TESTTRACE_PHASE_INSTR(Js::ArraySegmentHoistPhase, instr, _u("Hoisting array segment load out of loop\n"));

                if(loadObjectArray)
                {
                    InsertInstrInLandingPad(loadObjectArray, hoistHeadSegmentLoadOutOfLoop);
                }
                InsertInstrInLandingPad(loadHeadSegment, hoistHeadSegmentLoadOutOfLoop);
            }
            else
            {
                if(loadObjectArray)
                {LOGMEIN("GlobOpt.cpp] 16789\n");
                    loadObjectArray->SetByteCodeOffset(instr);
                    insertBeforeInstr->InsertBefore(loadObjectArray);
                }
                loadHeadSegment->SetByteCodeOffset(instr);
                insertBeforeInstr->InsertBefore(loadHeadSegment);
                instr->loadedArrayHeadSegment = true;
            }
        };

        if(doHeadSegmentLoad && isLikelyJsArray)
        {LOGMEIN("GlobOpt.cpp] 16800\n");
            // For javascript arrays, the head segment is required to load the head segment length
            InsertHeadSegmentLoad();
        }

        if(doHeadSegmentLengthLoad)
        {LOGMEIN("GlobOpt.cpp] 16806\n");
            Assert(!isLikelyJsArray || newHeadSegmentSym || baseArrayValueInfo && baseArrayValueInfo->HeadSegmentSym());
            Assert(newHeadSegmentLengthSym);
            Assert(!headSegmentLengthValue);

            TRACE_TESTTRACE_PHASE_INSTR(Js::ArraySegmentHoistPhase, instr, _u("Separating array segment length load\n"));

            // Create an initial value for the head segment length
            blockData.liveVarSyms->Set(newHeadSegmentLengthSym->m_id);
            headSegmentLengthValue = NewIntRangeValue(0, Js::SparseArraySegmentBase::MaxLength, false);
            headSegmentLengthConstantBounds = IntConstantBounds(0, Js::SparseArraySegmentBase::MaxLength);
            SetValue(&blockData, headSegmentLengthValue, newHeadSegmentLengthSym);

            // SetValue above would have set the sym store to newHeadSegmentLengthSym. This sym won't be used for copy-prop
            // though, so remove it as the sym store.
            this->SetSymStoreDirect(headSegmentLengthValue->GetValueInfo(), nullptr);

            StackSym *const headSegmentSym =
                isLikelyJsArray
                    ? newHeadSegmentSym ? newHeadSegmentSym : baseArrayValueInfo->HeadSegmentSym()
                    : nullptr;
            IR::Instr *const loadHeadSegmentLength =
                IR::Instr::New(
                    Js::OpCode::LdIndir,
                    IR::RegOpnd::New(newHeadSegmentLengthSym, newHeadSegmentLengthSym->GetType(), instr->m_func),
                    IR::IndirOpnd::New(
                        isLikelyJsArray ? IR::RegOpnd::New(headSegmentSym, headSegmentSym->GetType(), instr->m_func) : baseOpnd,
                        isLikelyJsArray
                            ? Js::SparseArraySegmentBase::GetOffsetOfLength()
                            : Lowerer::GetArrayOffsetOfLength(baseValueType),
                        newHeadSegmentLengthSym->GetType(),
                        instr->m_func),
                    instr->m_func);
            loadHeadSegmentLength->GetDst()->SetIsJITOptimizedReg(true);
            loadHeadSegmentLength->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->SetIsJITOptimizedReg(true);

            // We don't check the head segment length for negative (very large uint32) values. For JS arrays, the bound checks
            // cover that. For typed arrays, we currently don't allocate array buffers with more than 1 GB elements.
            if(hoistHeadSegmentLengthLoadOutOfLoop)
            {LOGMEIN("GlobOpt.cpp] 16845\n");
                Assert(
                    !(
                        isLikelyJsArray
                            ? hoistHeadSegmentLengthLoadOutOfLoop->jsArrayKills.KillsArrayHeadSegmentLengths()
                            : hoistHeadSegmentLengthLoadOutOfLoop->jsArrayKills.KillsTypedArrayHeadSegmentLengths()
                    ));

                TRACE_PHASE_INSTR(
                    Js::ArraySegmentHoistPhase,
                    instr,
                    _u("Hoisting array segment length load out of loop %u to landing pad block %u\n"),
                    hoistHeadSegmentLengthLoadOutOfLoop->GetLoopNumber(),
                    hoistHeadSegmentLengthLoadOutOfLoop->landingPad->GetBlockNum());
                TESTTRACE_PHASE_INSTR(Js::ArraySegmentHoistPhase, instr, _u("Hoisting array segment length load out of loop\n"));

                InsertInstrInLandingPad(loadHeadSegmentLength, hoistHeadSegmentLengthLoadOutOfLoop);

                // Hoist the head segment length value
                for(InvariantBlockBackwardIterator it(
                        this,
                        currentBlock,
                        hoistHeadSegmentLengthLoadOutOfLoop->landingPad,
                        baseOpnd->m_sym,
                        baseValue->GetValueNumber());
                    it.IsValid();
                    it.MoveNext())
                {LOGMEIN("GlobOpt.cpp] 16872\n");
                    BasicBlock *const block = it.Block();
                    block->globOptData.liveVarSyms->Set(newHeadSegmentLengthSym->m_id);
                    Assert(!FindValue(block->globOptData.symToValueMap, newHeadSegmentLengthSym));
                    Value *const headSegmentLengthValueCopy =
                        CopyValue(headSegmentLengthValue, headSegmentLengthValue->GetValueNumber());
                    SetValue(&block->globOptData, headSegmentLengthValueCopy, newHeadSegmentLengthSym);
                    this->SetSymStoreDirect(headSegmentLengthValueCopy->GetValueInfo(), nullptr);
                }
            }
            else
            {
                loadHeadSegmentLength->SetByteCodeOffset(instr);
                insertBeforeInstr->InsertBefore(loadHeadSegmentLength);
                instr->loadedArrayHeadSegmentLength = true;
            }
        }

        if(doExtractBoundChecks)
        {LOGMEIN("GlobOpt.cpp] 16891\n");
            Assert(!(eliminatedLowerBoundCheck && eliminatedUpperBoundCheck));
            Assert(baseOwnerIndir);
            Assert(!baseOwnerIndir->GetIndexOpnd() || baseOwnerIndir->GetIndexOpnd()->m_sym->IsTypeSpec());
            Assert(doHeadSegmentLengthLoad || headSegmentLengthIsAvailable);
            Assert(canBailOutOnArrayAccessHelperCall);
            Assert(!isStore || instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict || Js::IsSimd128LoadStore(instr->m_opcode));

            StackSym *const headSegmentLengthSym =
                headSegmentLengthIsAvailable ? baseArrayValueInfo->HeadSegmentLengthSym() : newHeadSegmentLengthSym;
            Assert(headSegmentLengthSym);
            Assert(headSegmentLengthValue);

            ArrayLowerBoundCheckHoistInfo lowerBoundCheckHoistInfo;
            ArrayUpperBoundCheckHoistInfo upperBoundCheckHoistInfo;
            bool failedToUpdateCompatibleLowerBoundCheck = false, failedToUpdateCompatibleUpperBoundCheck = false;
            if(DoBoundCheckHoist())
            {LOGMEIN("GlobOpt.cpp] 16908\n");
                if(indexVarSym)
                {LOGMEIN("GlobOpt.cpp] 16910\n");
                    TRACE_PHASE_INSTR_VERBOSE(
                        Js::Phase::BoundCheckHoistPhase,
                        instr,
                        _u("Determining array bound check hoistability for index s%u\n"),
                        indexVarSym->m_id);
                }
                else
                {
                    TRACE_PHASE_INSTR_VERBOSE(
                        Js::Phase::BoundCheckHoistPhase,
                        instr,
                        _u("Determining array bound check hoistability for index %d\n"),
                        indexConstantBounds.LowerBound());
                }

                DetermineArrayBoundCheckHoistability(
                    !eliminatedLowerBoundCheck,
                    !eliminatedUpperBoundCheck,
                    lowerBoundCheckHoistInfo,
                    upperBoundCheckHoistInfo,
                    isLikelyJsArray,
                    indexVarSym,
                    indexValue,
                    indexConstantBounds,
                    headSegmentLengthSym,
                    headSegmentLengthValue,
                    headSegmentLengthConstantBounds,
                    hoistHeadSegmentLengthLoadOutOfLoop,
                    failedToUpdateCompatibleLowerBoundCheck,
                    failedToUpdateCompatibleUpperBoundCheck);

#ifdef ENABLE_SIMDJS
                // SIMD_JS
                UpdateBoundCheckHoistInfoForSimd(upperBoundCheckHoistInfo, newBaseValueType, instr);
#endif
            }

            if(!eliminatedLowerBoundCheck)
            {LOGMEIN("GlobOpt.cpp] 16949\n");
                eliminatedLowerBoundCheck = true;

                Assert(indexVarSym);
                Assert(baseOwnerIndir->GetIndexOpnd());
                Assert(indexValue);

                ArrayLowerBoundCheckHoistInfo &hoistInfo = lowerBoundCheckHoistInfo;
                if(hoistInfo.HasAnyInfo())
                {LOGMEIN("GlobOpt.cpp] 16958\n");
                    BasicBlock *hoistBlock;
                    if(hoistInfo.CompatibleBoundCheckBlock())
                    {LOGMEIN("GlobOpt.cpp] 16961\n");
                        hoistBlock = hoistInfo.CompatibleBoundCheckBlock();

                        TRACE_PHASE_INSTR(
                            Js::Phase::BoundCheckHoistPhase,
                            instr,
                            _u("Hoisting array lower bound check into existing bound check instruction in block %u\n"),
                            hoistBlock->GetBlockNum());
                        TESTTRACE_PHASE_INSTR(
                            Js::Phase::BoundCheckHoistPhase,
                            instr,
                            _u("Hoisting array lower bound check into existing bound check instruction\n"));
                    }
                    else
                    {
                        Assert(hoistInfo.Loop());
                        BasicBlock *const landingPad = hoistInfo.Loop()->landingPad;
                        hoistBlock = landingPad;

                        StackSym *indexIntSym;
                        if(hoistInfo.IndexSym() && hoistInfo.IndexSym()->IsVar())
                        {LOGMEIN("GlobOpt.cpp] 16982\n");
                            if(!IsInt32TypeSpecialized(hoistInfo.IndexSym(), landingPad))
                            {LOGMEIN("GlobOpt.cpp] 16984\n");
                                // Int-specialize the index sym, as the BoundCheck instruction requires int operands. Specialize
                                // it in this block if it is invariant, as the conversion will be hoisted along with value
                                // updates.
                                BasicBlock *specializationBlock = hoistInfo.Loop()->landingPad;
                                IR::Instr *specializeBeforeInstr = nullptr;
                                if(!IsInt32TypeSpecialized(hoistInfo.IndexSym(), &blockData) &&
                                    OptIsInvariant(
                                        hoistInfo.IndexSym(),
                                        currentBlock,
                                        hoistInfo.Loop(),
                                        FindValue(hoistInfo.IndexSym()),
                                        false,
                                        true))
                                {LOGMEIN("GlobOpt.cpp] 16998\n");
                                    specializationBlock = currentBlock;
                                    specializeBeforeInstr = insertBeforeInstr;
                                }
                                Assert(tempBv->IsEmpty());
                                tempBv->Set(hoistInfo.IndexSym()->m_id);
                                ToInt32(tempBv, specializationBlock, false, specializeBeforeInstr);
                                tempBv->ClearAll();
                                Assert(IsInt32TypeSpecialized(hoistInfo.IndexSym(), landingPad));
                            }
                            indexIntSym = hoistInfo.IndexSym()->GetInt32EquivSym(nullptr);
                            Assert(indexIntSym);
                        }
                        else
                        {
                            indexIntSym = hoistInfo.IndexSym();
                            Assert(!indexIntSym || indexIntSym->GetType() == TyInt32 || indexIntSym->GetType() == TyUint32);
                        }

                        // The info in the landing pad may be better than the info in the current block due to changes made to
                        // the index sym inside the loop. Check if the bound check we intend to hoist is unnecessary in the
                        // landing pad.
                        if(!ValueInfo::IsLessThanOrEqualTo(
                                nullptr,
                                0,
                                0,
                                hoistInfo.IndexValue(),
                                hoistInfo.IndexConstantBounds().LowerBound(),
                                hoistInfo.IndexConstantBounds().UpperBound(),
                                hoistInfo.Offset()))
                        {LOGMEIN("GlobOpt.cpp] 17028\n");
                            Assert(hoistInfo.IndexSym());
                            Assert(hoistInfo.Loop()->bailOutInfo);
                            EnsureBailTarget(hoistInfo.Loop());

                            if(hoistInfo.LoopCount())
                            {LOGMEIN("GlobOpt.cpp] 17034\n");
                                // Generate the loop count and loop count based bound that will be used for the bound check
                                if(!hoistInfo.LoopCount()->HasBeenGenerated())
                                {LOGMEIN("GlobOpt.cpp] 17037\n");
                                    GenerateLoopCount(hoistInfo.Loop(), hoistInfo.LoopCount());
                                }
                                GenerateSecondaryInductionVariableBound(
                                    hoistInfo.Loop(),
                                    indexVarSym->GetInt32EquivSym(nullptr),
                                    hoistInfo.LoopCount(),
                                    hoistInfo.MaxMagnitudeChange(),
                                    hoistInfo.IndexSym());
                            }

                            IR::Opnd* lowerBound = IR::IntConstOpnd::New(0, TyInt32, instr->m_func, true);
                            IR::Opnd* upperBound = IR::RegOpnd::New(indexIntSym, TyInt32, instr->m_func);
                            upperBound->SetIsJITOptimizedReg(true);

                            // 0 <= indexSym + offset (src1 <= src2 + dst)
                            IR::Instr *const boundCheck = CreateBoundsCheckInstr(
                                lowerBound,
                                upperBound,
                                hoistInfo.Offset(),
                                hoistInfo.IsLoopCountBasedBound()
                                    ? IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck
                                    : IR::BailOutOnFailedHoistedBoundCheck,
                                hoistInfo.Loop()->bailOutInfo,
                                hoistInfo.Loop()->bailOutInfo->bailOutFunc);
                            InsertInstrInLandingPad(boundCheck, hoistInfo.Loop());

                            TRACE_PHASE_INSTR(
                                Js::Phase::BoundCheckHoistPhase,
                                instr,
                                _u("Hoisting array lower bound check out of loop %u to landing pad block %u, as (0 <= s%u + %d)\n"),
                                hoistInfo.Loop()->GetLoopNumber(),
                                landingPad->GetBlockNum(),
                                hoistInfo.IndexSym()->m_id,
                                hoistInfo.Offset());
                            TESTTRACE_PHASE_INSTR(
                                Js::Phase::BoundCheckHoistPhase,
                                instr,
                                _u("Hoisting array lower bound check out of loop\n"));

                            // Record the bound check instruction as available
                            const IntBoundCheck boundCheckInfo(
                                ZeroValueNumber,
                                hoistInfo.IndexValueNumber(),
                                boundCheck,
                                landingPad);
                            {LOGMEIN("GlobOpt.cpp] 17083\n");
                                const bool added = blockData.availableIntBoundChecks->AddNew(boundCheckInfo) >= 0;
                                Assert(added || failedToUpdateCompatibleLowerBoundCheck);
                            }
                            for(InvariantBlockBackwardIterator it(this, currentBlock, landingPad, nullptr);
                                it.IsValid();
                                it.MoveNext())
                            {LOGMEIN("GlobOpt.cpp] 17090\n");
                                const bool added = it.Block()->globOptData.availableIntBoundChecks->AddNew(boundCheckInfo) >= 0;
                                Assert(added || failedToUpdateCompatibleLowerBoundCheck);
                            }
                        }
                    }

                    // Update values of the syms involved in the bound check to reflect the bound check
                    if(hoistBlock != currentBlock && hoistInfo.IndexSym() && hoistInfo.Offset() != INT32_MIN)
                    {LOGMEIN("GlobOpt.cpp] 17099\n");
                        for(InvariantBlockBackwardIterator it(
                                this,
                                currentBlock->next,
                                hoistBlock,
                                hoistInfo.IndexSym(),
                                hoistInfo.IndexValueNumber());
                            it.IsValid();
                            it.MoveNext())
                        {LOGMEIN("GlobOpt.cpp] 17108\n");
                            Value *const value = it.InvariantSymValue();
                            IntConstantBounds constantBounds;
                            AssertVerify(value->GetValueInfo()->TryGetIntConstantBounds(&constantBounds, true));

                            ValueInfo *const newValueInfo =
                                UpdateIntBoundsForGreaterThanOrEqual(
                                    value,
                                    constantBounds,
                                    nullptr,
                                    IntConstantBounds(-hoistInfo.Offset(), -hoistInfo.Offset()),
                                    false);
                            if(newValueInfo)
                            {
                                ChangeValueInfo(nullptr, value, newValueInfo);
                                if(it.Block() == currentBlock && value == indexValue)
                                {LOGMEIN("GlobOpt.cpp] 17124\n");
                                    AssertVerify(newValueInfo->TryGetIntConstantBounds(&indexConstantBounds));
                                }
                            }
                        }
                    }
                }
                else
                {
                    IR::Opnd* lowerBound = IR::IntConstOpnd::New(0, TyInt32, instr->m_func, true);
                    IR::Opnd* upperBound = baseOwnerIndir->GetIndexOpnd();
                    upperBound->SetIsJITOptimizedReg(true);
                    const int offset = 0;

                    IR::Instr *boundCheck;
                    if(shareableBailOutInfo)
                    {LOGMEIN("GlobOpt.cpp] 17140\n");
                        ShareBailOut();
                        boundCheck = CreateBoundsCheckInstr(
                            lowerBound,
                            upperBound,
                            offset,
                            IR::BailOutOnArrayAccessHelperCall,
                            shareableBailOutInfo,
                            shareableBailOutInfo->bailOutFunc);
                    }
                    else
                    {
                        boundCheck = CreateBoundsCheckInstr(
                            lowerBound,
                            upperBound,
                            offset,
                            instr->m_func);
                    }


                    boundCheck->SetByteCodeOffset(instr);
                    insertBeforeInstr->InsertBefore(boundCheck);
                    if(!shareableBailOutInfo)
                    {
                        GenerateBailAtOperation(&boundCheck, IR::BailOutOnArrayAccessHelperCall);
                        shareableBailOutInfo = boundCheck->GetBailOutInfo();
                        shareableBailOutInfoOriginalOwner = boundCheck;
                    }

                    TRACE_PHASE_INSTR(
                        Js::Phase::BoundCheckEliminationPhase,
                        instr,
                        _u("Separating array lower bound check, as (0 <= s%u)\n"),
                        indexVarSym->m_id);
                    TESTTRACE_PHASE_INSTR(
                        Js::Phase::BoundCheckEliminationPhase,
                        instr,
                        _u("Separating array lower bound check\n"));

                    if(DoBoundCheckHoist())
                    {LOGMEIN("GlobOpt.cpp] 17180\n");
                        // Record the bound check instruction as available
                        const bool added =
                            blockData.availableIntBoundChecks->AddNew(
                                IntBoundCheck(ZeroValueNumber, indexValue->GetValueNumber(), boundCheck, currentBlock)) >= 0;
                        Assert(added || failedToUpdateCompatibleLowerBoundCheck);
                    }
                }

                // Update the index value to reflect the bound check
                ValueInfo *const newValueInfo =
                    UpdateIntBoundsForGreaterThanOrEqual(
                        indexValue,
                        indexConstantBounds,
                        nullptr,
                        IntConstantBounds(0, 0),
                        false);
                if(newValueInfo)
                {
                    ChangeValueInfo(nullptr, indexValue, newValueInfo);
                    AssertVerify(newValueInfo->TryGetIntConstantBounds(&indexConstantBounds));
                }
            }

            if(!eliminatedUpperBoundCheck)
            {LOGMEIN("GlobOpt.cpp] 17205\n");
                eliminatedUpperBoundCheck = true;

                ArrayUpperBoundCheckHoistInfo &hoistInfo = upperBoundCheckHoistInfo;
                if(hoistInfo.HasAnyInfo())
                {LOGMEIN("GlobOpt.cpp] 17210\n");
                    BasicBlock *hoistBlock;
                    if(hoistInfo.CompatibleBoundCheckBlock())
                    {LOGMEIN("GlobOpt.cpp] 17213\n");
                        hoistBlock = hoistInfo.CompatibleBoundCheckBlock();

                        TRACE_PHASE_INSTR(
                            Js::Phase::BoundCheckHoistPhase,
                            instr,
                            _u("Hoisting array upper bound check into existing bound check instruction in block %u\n"),
                            hoistBlock->GetBlockNum());
                        TESTTRACE_PHASE_INSTR(
                            Js::Phase::BoundCheckHoistPhase,
                            instr,
                            _u("Hoisting array upper bound check into existing bound check instruction\n"));
                    }
                    else
                    {
                        Assert(hoistInfo.Loop());
                        BasicBlock *const landingPad = hoistInfo.Loop()->landingPad;
                        hoistBlock = landingPad;

                        StackSym *indexIntSym;
                        if(hoistInfo.IndexSym() && hoistInfo.IndexSym()->IsVar())
                        {LOGMEIN("GlobOpt.cpp] 17234\n");
                            if(!IsInt32TypeSpecialized(hoistInfo.IndexSym(), landingPad))
                            {LOGMEIN("GlobOpt.cpp] 17236\n");
                                // Int-specialize the index sym, as the BoundCheck instruction requires int operands. Specialize it
                                // in this block if it is invariant, as the conversion will be hoisted along with value updates.
                                BasicBlock *specializationBlock = hoistInfo.Loop()->landingPad;
                                IR::Instr *specializeBeforeInstr = nullptr;
                                if(!IsInt32TypeSpecialized(hoistInfo.IndexSym(), &blockData) &&
                                    OptIsInvariant(
                                        hoistInfo.IndexSym(),
                                        currentBlock,
                                        hoistInfo.Loop(),
                                        FindValue(hoistInfo.IndexSym()),
                                        false,
                                        true))
                                {LOGMEIN("GlobOpt.cpp] 17249\n");
                                    specializationBlock = currentBlock;
                                    specializeBeforeInstr = insertBeforeInstr;
                                }
                                Assert(tempBv->IsEmpty());
                                tempBv->Set(hoistInfo.IndexSym()->m_id);
                                ToInt32(tempBv, specializationBlock, false, specializeBeforeInstr);
                                tempBv->ClearAll();
                                Assert(IsInt32TypeSpecialized(hoistInfo.IndexSym(), landingPad));
                            }
                            indexIntSym = hoistInfo.IndexSym()->GetInt32EquivSym(nullptr);
                            Assert(indexIntSym);
                        }
                        else
                        {
                            indexIntSym = hoistInfo.IndexSym();
                            Assert(!indexIntSym || indexIntSym->GetType() == TyInt32 || indexIntSym->GetType() == TyUint32);
                        }

                        // The info in the landing pad may be better than the info in the current block due to changes made to the
                        // index sym inside the loop. Check if the bound check we intend to hoist is unnecessary in the landing pad.
                        if(!ValueInfo::IsLessThanOrEqualTo(
                                hoistInfo.IndexValue(),
                                hoistInfo.IndexConstantBounds().LowerBound(),
                                hoistInfo.IndexConstantBounds().UpperBound(),
                                hoistInfo.HeadSegmentLengthValue(),
                                hoistInfo.HeadSegmentLengthConstantBounds().LowerBound(),
                                hoistInfo.HeadSegmentLengthConstantBounds().UpperBound(),
                                hoistInfo.Offset()))
                        {LOGMEIN("GlobOpt.cpp] 17278\n");
                            Assert(hoistInfo.Loop()->bailOutInfo);
                            EnsureBailTarget(hoistInfo.Loop());

                            if(hoistInfo.LoopCount())
                            {LOGMEIN("GlobOpt.cpp] 17283\n");
                                // Generate the loop count and loop count based bound that will be used for the bound check
                                if(!hoistInfo.LoopCount()->HasBeenGenerated())
                                {LOGMEIN("GlobOpt.cpp] 17286\n");
                                    GenerateLoopCount(hoistInfo.Loop(), hoistInfo.LoopCount());
                                }
                                GenerateSecondaryInductionVariableBound(
                                    hoistInfo.Loop(),
                                    indexVarSym->GetInt32EquivSym(nullptr),
                                    hoistInfo.LoopCount(),
                                    hoistInfo.MaxMagnitudeChange(),
                                    hoistInfo.IndexSym());
                            }

                            IR::Opnd* lowerBound = indexIntSym
                                ? static_cast<IR::Opnd *>(IR::RegOpnd::New(indexIntSym, TyInt32, instr->m_func))
                                : IR::IntConstOpnd::New(
                                    hoistInfo.IndexConstantBounds().LowerBound(),
                                    TyInt32,
                                    instr->m_func,
                                    true);
                            lowerBound->SetIsJITOptimizedReg(true);
                            IR::Opnd* upperBound = IR::RegOpnd::New(headSegmentLengthSym, headSegmentLengthSym->GetType(), instr->m_func);
                            upperBound->SetIsJITOptimizedReg(true);

                            // indexSym <= headSegmentLength + offset (src1 <= src2 + dst)
                            IR::Instr *const boundCheck = CreateBoundsCheckInstr(
                                lowerBound,
                                upperBound,
                                hoistInfo.Offset(),
                                hoistInfo.IsLoopCountBasedBound()
                                    ? IR::BailOutOnFailedHoistedLoopCountBasedBoundCheck
                                    : IR::BailOutOnFailedHoistedBoundCheck,
                                hoistInfo.Loop()->bailOutInfo,
                                hoistInfo.Loop()->bailOutInfo->bailOutFunc);

                            InsertInstrInLandingPad(boundCheck, hoistInfo.Loop());

                            if(indexIntSym)
                            {LOGMEIN("GlobOpt.cpp] 17322\n");
                                TRACE_PHASE_INSTR(
                                    Js::Phase::BoundCheckHoistPhase,
                                    instr,
                                    _u("Hoisting array upper bound check out of loop %u to landing pad block %u, as (s%u <= s%u + %d)\n"),
                                    hoistInfo.Loop()->GetLoopNumber(),
                                    landingPad->GetBlockNum(),
                                    hoistInfo.IndexSym()->m_id,
                                    headSegmentLengthSym->m_id,
                                    hoistInfo.Offset());
                            }
                            else
                            {
                                TRACE_PHASE_INSTR(
                                    Js::Phase::BoundCheckHoistPhase,
                                    instr,
                                    _u("Hoisting array upper bound check out of loop %u to landing pad block %u, as (%d <= s%u + %d)\n"),
                                    hoistInfo.Loop()->GetLoopNumber(),
                                    landingPad->GetBlockNum(),
                                    hoistInfo.IndexConstantBounds().LowerBound(),
                                    headSegmentLengthSym->m_id,
                                    hoistInfo.Offset());
                            }
                            TESTTRACE_PHASE_INSTR(
                                Js::Phase::BoundCheckHoistPhase,
                                instr,
                                _u("Hoisting array upper bound check out of loop\n"));

                            // Record the bound check instruction as available
                            const IntBoundCheck boundCheckInfo(
                                hoistInfo.IndexValue() ? hoistInfo.IndexValueNumber() : ZeroValueNumber,
                                hoistInfo.HeadSegmentLengthValue()->GetValueNumber(),
                                boundCheck,
                                landingPad);
                            {LOGMEIN("GlobOpt.cpp] 17356\n");
                                const bool added = blockData.availableIntBoundChecks->AddNew(boundCheckInfo) >= 0;
                                Assert(added || failedToUpdateCompatibleUpperBoundCheck);
                            }
                            for(InvariantBlockBackwardIterator it(this, currentBlock, landingPad, nullptr);
                                it.IsValid();
                                it.MoveNext())
                            {LOGMEIN("GlobOpt.cpp] 17363\n");
                                const bool added = it.Block()->globOptData.availableIntBoundChecks->AddNew(boundCheckInfo) >= 0;
                                Assert(added || failedToUpdateCompatibleUpperBoundCheck);
                            }
                        }
                    }

                    // Update values of the syms involved in the bound check to reflect the bound check
                    Assert(!hoistInfo.Loop() || hoistBlock != currentBlock);
                    if(hoistBlock != currentBlock)
                    {LOGMEIN("GlobOpt.cpp] 17373\n");
                        for(InvariantBlockBackwardIterator it(this, currentBlock->next, hoistBlock, nullptr);
                            it.IsValid();
                            it.MoveNext())
                        {LOGMEIN("GlobOpt.cpp] 17377\n");
                            BasicBlock *const block = it.Block();

                            Value *leftValue;
                            IntConstantBounds leftConstantBounds;
                            if(hoistInfo.IndexSym())
                            {LOGMEIN("GlobOpt.cpp] 17383\n");
                                leftValue = FindValue(block->globOptData.symToValueMap, hoistInfo.IndexSym());
                                if(!leftValue || leftValue->GetValueNumber() != hoistInfo.IndexValueNumber())
                                {LOGMEIN("GlobOpt.cpp] 17386\n");
                                    continue;
                                }
                                AssertVerify(leftValue->GetValueInfo()->TryGetIntConstantBounds(&leftConstantBounds, true));
                            }
                            else
                            {
                                leftValue = nullptr;
                                leftConstantBounds = hoistInfo.IndexConstantBounds();
                            }

                            Value *const rightValue = FindValue(block->globOptData.symToValueMap, headSegmentLengthSym);
                            if(!rightValue)
                            {LOGMEIN("GlobOpt.cpp] 17399\n");
                                continue;
                            }
                            Assert(rightValue->GetValueNumber() ==  headSegmentLengthValue->GetValueNumber());
                            IntConstantBounds rightConstantBounds;
                            AssertVerify(rightValue->GetValueInfo()->TryGetIntConstantBounds(&rightConstantBounds));

                            ValueInfo *const newValueInfoForLessThanOrEqual =
                                UpdateIntBoundsForLessThanOrEqual(
                                    leftValue,
                                    leftConstantBounds,
                                    rightValue,
                                    rightConstantBounds,
                                    hoistInfo.Offset(),
                                    false);
                            if (newValueInfoForLessThanOrEqual)
                            {
                                ChangeValueInfo(nullptr, leftValue, newValueInfoForLessThanOrEqual);
                                AssertVerify(newValueInfoForLessThanOrEqual->TryGetIntConstantBounds(&leftConstantBounds, true));
                                if(block == currentBlock && leftValue == indexValue)
                                {LOGMEIN("GlobOpt.cpp] 17419\n");
                                    Assert(newValueInfoForLessThanOrEqual->IsInt());
                                    indexConstantBounds = leftConstantBounds;
                                }
                            }
                            if(hoistInfo.Offset() != INT32_MIN)
                            {LOGMEIN("GlobOpt.cpp] 17425\n");
                                ValueInfo *const newValueInfoForGreaterThanOrEqual =
                                    UpdateIntBoundsForGreaterThanOrEqual(
                                        rightValue,
                                        rightConstantBounds,
                                        leftValue,
                                        leftConstantBounds,
                                        -hoistInfo.Offset(),
                                        false);
                                if (newValueInfoForGreaterThanOrEqual)
                                {
                                    ChangeValueInfo(nullptr, rightValue, newValueInfoForGreaterThanOrEqual);
                                    if(block == currentBlock)
                                    {LOGMEIN("GlobOpt.cpp] 17438\n");
                                        Assert(rightValue == headSegmentLengthValue);
                                        AssertVerify(newValueInfoForGreaterThanOrEqual->TryGetIntConstantBounds(&headSegmentLengthConstantBounds));
                                    }
                                }
                            }
                        }
                    }
                }
                else
                {
                    IR::Opnd* lowerBound = baseOwnerIndir->GetIndexOpnd()
                        ? static_cast<IR::Opnd *>(baseOwnerIndir->GetIndexOpnd())
                        : IR::IntConstOpnd::New(baseOwnerIndir->GetOffset(), TyInt32, instr->m_func, true);
                    lowerBound->SetIsJITOptimizedReg(true);
                    IR::Opnd* upperBound = IR::RegOpnd::New(headSegmentLengthSym, headSegmentLengthSym->GetType(), instr->m_func);
                    upperBound->SetIsJITOptimizedReg(true);
                    const int offset = GetBoundCheckOffsetForSimd(newBaseValueType, instr, -1);
                    IR::Instr *boundCheck;

                    // index <= headSegmentLength - 1 (src1 <= src2 + dst)
                    if (shareableBailOutInfo)
                    {LOGMEIN("GlobOpt.cpp] 17460\n");
                        ShareBailOut();
                        boundCheck = CreateBoundsCheckInstr(
                            lowerBound,
                            upperBound,
                            offset,
                            IR::BailOutOnArrayAccessHelperCall,
                            shareableBailOutInfo,
                            shareableBailOutInfo->bailOutFunc);
                    }
                    else
                    {
                        boundCheck = CreateBoundsCheckInstr(
                            lowerBound,
                            upperBound,
                            offset,
                            instr->m_func);
                    }

                    boundCheck->SetByteCodeOffset(instr);
                    insertBeforeInstr->InsertBefore(boundCheck);
                    if(!shareableBailOutInfo)
                    {
                        GenerateBailAtOperation(&boundCheck, IR::BailOutOnArrayAccessHelperCall);
                        shareableBailOutInfo = boundCheck->GetBailOutInfo();
                        shareableBailOutInfoOriginalOwner = boundCheck;
                    }
                    instr->extractedUpperBoundCheckWithoutHoisting = true;

                    if(baseOwnerIndir->GetIndexOpnd())
                    {LOGMEIN("GlobOpt.cpp] 17490\n");
                        TRACE_PHASE_INSTR(
                            Js::Phase::BoundCheckEliminationPhase,
                            instr,
                            _u("Separating array upper bound check, as (s%u < s%u)\n"),
                            indexVarSym->m_id,
                            headSegmentLengthSym->m_id);
                    }
                    else
                    {
                        TRACE_PHASE_INSTR(
                            Js::Phase::BoundCheckEliminationPhase,
                            instr,
                            _u("Separating array upper bound check, as (%d < s%u)\n"),
                            baseOwnerIndir->GetOffset(),
                            headSegmentLengthSym->m_id);
                    }
                    TESTTRACE_PHASE_INSTR(
                        Js::Phase::BoundCheckEliminationPhase,
                        instr,
                        _u("Separating array upper bound check\n"));

                    if(DoBoundCheckHoist())
                    {LOGMEIN("GlobOpt.cpp] 17513\n");
                        // Record the bound check instruction as available
                        const bool added =
                            blockData.availableIntBoundChecks->AddNew(
                                IntBoundCheck(
                                    indexValue ? indexValue->GetValueNumber() : ZeroValueNumber,
                                    headSegmentLengthValue->GetValueNumber(),
                                    boundCheck,
                                    currentBlock)) >= 0;
                        Assert(added || failedToUpdateCompatibleUpperBoundCheck);
                    }
                }

                // Update the index and head segment length values to reflect the bound check
                ValueInfo *newValueInfo =
                    UpdateIntBoundsForLessThan(
                        indexValue,
                        indexConstantBounds,
                        headSegmentLengthValue,
                        headSegmentLengthConstantBounds,
                        false);
                if(newValueInfo)
                {
                    ChangeValueInfo(nullptr, indexValue, newValueInfo);
                    AssertVerify(newValueInfo->TryGetIntConstantBounds(&indexConstantBounds));
                }
                newValueInfo =
                    UpdateIntBoundsForGreaterThan(
                        headSegmentLengthValue,
                        headSegmentLengthConstantBounds,
                        indexValue,
                        indexConstantBounds,
                        false);
                if(newValueInfo)
                {
                    ChangeValueInfo(nullptr, headSegmentLengthValue, newValueInfo);
                }
            }
        }

        if(doHeadSegmentLoad && !isLikelyJsArray)
        {LOGMEIN("GlobOpt.cpp] 17554\n");
            // For typed arrays, load the length first, followed by the bound checks, and then load the head segment. This
            // allows the length sym to become dead by the time of the head segment load, freeing up the register for use by the
            // head segment sym.
            InsertHeadSegmentLoad();
        }

        if(doArrayChecks || doHeadSegmentLoad || doHeadSegmentLengthLoad || doLengthLoad)
        {
            UpdateValue(newHeadSegmentSym, newHeadSegmentLengthSym, newLengthSym);
            baseValueInfo = baseValue->GetValueInfo();
            baseArrayValueInfo = baseValueInfo->IsArrayValueInfo() ? baseValueInfo->AsArrayValueInfo() : nullptr;

            // Iterate up to the root loop's landing pad until all necessary value info is updated
            uint hoistItemCount =
                static_cast<uint>(!!hoistChecksOutOfLoop) +
                !!hoistHeadSegmentLoadOutOfLoop +
                !!hoistHeadSegmentLengthLoadOutOfLoop +
                !!hoistLengthLoadOutOfLoop;
            if(hoistItemCount != 0)
            {LOGMEIN("GlobOpt.cpp] 17574\n");
                Loop *rootLoop = nullptr;
                for(Loop *loop = currentBlock->loop; loop; loop = loop->parent)
                {LOGMEIN("GlobOpt.cpp] 17577\n");
                    rootLoop = loop;
                }
                Assert(rootLoop);

                ValueInfo *valueInfoToHoist = baseValueInfo;
                bool removeHeadSegment, removeHeadSegmentLength, removeLength;
                if(baseArrayValueInfo)
                {LOGMEIN("GlobOpt.cpp] 17585\n");
                    removeHeadSegment = baseArrayValueInfo->HeadSegmentSym() && !hoistHeadSegmentLoadOutOfLoop;
                    removeHeadSegmentLength =
                        baseArrayValueInfo->HeadSegmentLengthSym() && !hoistHeadSegmentLengthLoadOutOfLoop;
                    removeLength = baseArrayValueInfo->LengthSym() && !hoistLengthLoadOutOfLoop;
                }
                else
                {
                    removeLength = removeHeadSegmentLength = removeHeadSegment = false;
                }
                for(InvariantBlockBackwardIterator it(
                        this,
                        currentBlock,
                        rootLoop->landingPad,
                        baseOpnd->m_sym,
                        baseValue->GetValueNumber());
                    it.IsValid();
                    it.MoveNext())
                {LOGMEIN("GlobOpt.cpp] 17603\n");
                    if(removeHeadSegment || removeHeadSegmentLength || removeLength)
                    {LOGMEIN("GlobOpt.cpp] 17605\n");
                        // Remove information that shouldn't be there anymore, from the value info
                        valueInfoToHoist =
                            valueInfoToHoist->AsArrayValueInfo()->Copy(
                                alloc,
                                !removeHeadSegment,
                                !removeHeadSegmentLength,
                                !removeLength);
                        removeLength = removeHeadSegmentLength = removeHeadSegment = false;
                    }

                    BasicBlock *const block = it.Block();
                    Value *const blockBaseValue = it.InvariantSymValue();
                    HoistInvariantValueInfo(valueInfoToHoist, blockBaseValue, block);

                    // See if we have completed hoisting value info for one of the items
                    if(hoistChecksOutOfLoop && block == hoistChecksOutOfLoop->landingPad)
                    {LOGMEIN("GlobOpt.cpp] 17622\n");
                        // All other items depend on array checks, so we can just stop here
                        hoistChecksOutOfLoop = nullptr;
                        break;
                    }
                    if(hoistHeadSegmentLoadOutOfLoop && block == hoistHeadSegmentLoadOutOfLoop->landingPad)
                    {LOGMEIN("GlobOpt.cpp] 17628\n");
                        hoistHeadSegmentLoadOutOfLoop = nullptr;
                        if(--hoistItemCount == 0)
                            break;
                        if(valueInfoToHoist->IsArrayValueInfo() && valueInfoToHoist->AsArrayValueInfo()->HeadSegmentSym())
                            removeHeadSegment = true;
                    }
                    if(hoistHeadSegmentLengthLoadOutOfLoop && block == hoistHeadSegmentLengthLoadOutOfLoop->landingPad)
                    {LOGMEIN("GlobOpt.cpp] 17636\n");
                        hoistHeadSegmentLengthLoadOutOfLoop = nullptr;
                        if(--hoistItemCount == 0)
                            break;
                        if(valueInfoToHoist->IsArrayValueInfo() && valueInfoToHoist->AsArrayValueInfo()->HeadSegmentLengthSym())
                            removeHeadSegmentLength = true;
                    }
                    if(hoistLengthLoadOutOfLoop && block == hoistLengthLoadOutOfLoop->landingPad)
                    {LOGMEIN("GlobOpt.cpp] 17644\n");
                        hoistLengthLoadOutOfLoop = nullptr;
                        if(--hoistItemCount == 0)
                            break;
                        if(valueInfoToHoist->IsArrayValueInfo() && valueInfoToHoist->AsArrayValueInfo()->LengthSym())
                            removeLength = true;
                    }
                }
            }
        }
    }

    IR::ArrayRegOpnd *baseArrayOpnd;
    if(baseArrayValueInfo)
    {LOGMEIN("GlobOpt.cpp] 17658\n");
        // Update the opnd to include the associated syms
        baseArrayOpnd =
            baseArrayValueInfo->CreateOpnd(
                baseOpnd,
                needsHeadSegment,
                needsHeadSegmentLength || (!isLikelyJsArray && needsLength),
                needsLength,
                eliminatedLowerBoundCheck,
                eliminatedUpperBoundCheck,
                instr->m_func);
        if(baseOwnerInstr)
        {LOGMEIN("GlobOpt.cpp] 17670\n");
            Assert(baseOwnerInstr->GetSrc1() == baseOpnd);
            baseOwnerInstr->ReplaceSrc1(baseArrayOpnd);
        }
        else
        {
            Assert(baseOwnerIndir);
            Assert(baseOwnerIndir->GetBaseOpnd() == baseOpnd);
            baseOwnerIndir->ReplaceBaseOpnd(baseArrayOpnd);
        }
        baseOpnd = baseArrayOpnd;
    }
    else
    {
        baseArrayOpnd = nullptr;
    }

    if(isLikelyJsArray)
    {
        // Insert an instruction to indicate to the dead-store pass that implicit calls need to be kept disabled until this
        // instruction. Operations other than LdElem and StElem don't benefit much from arrays having no missing values, so
        // no need to ensure that the array still has no missing values. For a particular array, if none of the accesses
        // benefit much from the no-missing-values information, it may be beneficial to avoid checking for no missing
        // values, especially in the case for a single array access, where the cost of the check could be relatively
        // significant. An StElem has to do additional checks in the common path if the array may have missing values, and
        // a StElem that operates on an array that has no missing values is more likely to keep the no-missing-values info
        // on the array more precise, so it still benefits a little from the no-missing-values info.
        CaptureNoImplicitCallUses(baseOpnd, isLoad || isStore);
    }
    else if(baseArrayOpnd && baseArrayOpnd->HeadSegmentLengthSym())
    {LOGMEIN("GlobOpt.cpp] 17700\n");
        // A typed array's array buffer may be transferred to a web worker as part of an implicit call, in which case the typed
        // array's length is set to zero. Insert an instruction to indicate to the dead-store pass that implicit calls need to
        // be disabled until this instruction.
        IR::RegOpnd *const headSegmentLengthOpnd =
            IR::RegOpnd::New(
                baseArrayOpnd->HeadSegmentLengthSym(),
                baseArrayOpnd->HeadSegmentLengthSym()->GetType(),
                instr->m_func);
        const IR::AutoReuseOpnd autoReuseHeadSegmentLengthOpnd(headSegmentLengthOpnd, instr->m_func);
        CaptureNoImplicitCallUses(headSegmentLengthOpnd, false);
    }

    const auto OnEliminated = [&](const Js::Phase phase, const char *const eliminatedLoad)
    {
        TRACE_TESTTRACE_PHASE_INSTR(phase, instr, _u("Eliminating array %S\n"), eliminatedLoad);
    };

    OnEliminated(Js::Phase::ArrayCheckHoistPhase, "checks");
    if(baseArrayOpnd)
    {LOGMEIN("GlobOpt.cpp] 17720\n");
        if(baseArrayOpnd->HeadSegmentSym())
        {LOGMEIN("GlobOpt.cpp] 17722\n");
            OnEliminated(Js::Phase::ArraySegmentHoistPhase, "head segment load");
        }
        if(baseArrayOpnd->HeadSegmentLengthSym())
        {LOGMEIN("GlobOpt.cpp] 17726\n");
            OnEliminated(Js::Phase::ArraySegmentHoistPhase, "head segment length load");
        }
        if(baseArrayOpnd->LengthSym())
        {LOGMEIN("GlobOpt.cpp] 17730\n");
            OnEliminated(Js::Phase::ArrayLengthHoistPhase, "length load");
        }
        if(baseArrayOpnd->EliminatedLowerBoundCheck())
        {LOGMEIN("GlobOpt.cpp] 17734\n");
            OnEliminated(Js::Phase::BoundCheckEliminationPhase, "lower bound check");
        }
        if(baseArrayOpnd->EliminatedUpperBoundCheck())
        {LOGMEIN("GlobOpt.cpp] 17738\n");
            OnEliminated(Js::Phase::BoundCheckEliminationPhase, "upper bound check");
        }
    }

    if(!canBailOutOnArrayAccessHelperCall)
    {LOGMEIN("GlobOpt.cpp] 17744\n");
        return;
    }

    // Bail out instead of generating a helper call. This helps to remove the array reference when the head segment and head
    // segment length are available, reduces code size, and allows bound checks to be separated.
    if(instr->HasBailOutInfo())
    {LOGMEIN("GlobOpt.cpp] 17751\n");
        const IR::BailOutKind bailOutKind = instr->GetBailOutKind();
        Assert(
            !(bailOutKind & ~IR::BailOutKindBits) ||
            (bailOutKind & ~IR::BailOutKindBits) == IR::BailOutOnImplicitCallsPreOp);
        instr->SetBailOutKind(bailOutKind & IR::BailOutKindBits | IR::BailOutOnArrayAccessHelperCall);
    }
    else
    {
        GenerateBailAtOperation(&instr, IR::BailOutOnArrayAccessHelperCall);
    }
}

void
GlobOpt::CaptureNoImplicitCallUses(
    IR::Opnd *opnd,
    const bool usesNoMissingValuesInfo,
    IR::Instr *const includeCurrentInstr)
{LOGMEIN("GlobOpt.cpp] 17769\n");
    Assert(!IsLoopPrePass());
    Assert(noImplicitCallUsesToInsert);
    Assert(opnd);

    // The opnd may be deleted later, so make a copy to ensure it is alive for inserting NoImplicitCallUses later
    opnd = opnd->Copy(func);

    if(!usesNoMissingValuesInfo)
    {LOGMEIN("GlobOpt.cpp] 17778\n");
        const ValueType valueType(opnd->GetValueType());
        if(valueType.IsArrayOrObjectWithArray() && valueType.HasNoMissingValues())
        {LOGMEIN("GlobOpt.cpp] 17781\n");
            // Inserting NoImplicitCallUses for an opnd with a definitely-array-with-no-missing-values value type means that the
            // instruction following it uses the information that the array has no missing values in some way, for instance, it
            // may omit missing value checks. Based on that, the dead-store phase in turn ensures that the necessary bailouts
            // are inserted to ensure that the array still has no missing values until the following instruction. Since
            // 'usesNoMissingValuesInfo' is false, change the value type to indicate to the dead-store phase that the following
            // instruction does not use the no-missing-values information.
            opnd->SetValueType(valueType.SetHasNoMissingValues(false));
        }
    }

    if(includeCurrentInstr)
    {LOGMEIN("GlobOpt.cpp] 17793\n");
        IR::Instr *const noImplicitCallUses =
            IR::PragmaInstr::New(Js::OpCode::NoImplicitCallUses, 0, includeCurrentInstr->m_func);
        noImplicitCallUses->SetSrc1(opnd);
        noImplicitCallUses->GetSrc1()->SetIsJITOptimizedReg(true);
        includeCurrentInstr->InsertAfter(noImplicitCallUses);
        return;
    }

    noImplicitCallUsesToInsert->Add(opnd);
}

void
GlobOpt::InsertNoImplicitCallUses(IR::Instr *const instr)
{LOGMEIN("GlobOpt.cpp] 17807\n");
    Assert(noImplicitCallUsesToInsert);

    const int n = noImplicitCallUsesToInsert->Count();
    if(n == 0)
    {LOGMEIN("GlobOpt.cpp] 17812\n");
        return;
    }

    IR::Instr *const insertBeforeInstr = instr->GetInsertBeforeByteCodeUsesInstr();
    for(int i = 0; i < n;)
    {LOGMEIN("GlobOpt.cpp] 17818\n");
        IR::Instr *const noImplicitCallUses = IR::PragmaInstr::New(Js::OpCode::NoImplicitCallUses, 0, instr->m_func);
        noImplicitCallUses->SetSrc1(noImplicitCallUsesToInsert->Item(i));
        noImplicitCallUses->GetSrc1()->SetIsJITOptimizedReg(true);
        ++i;
        if(i < n)
        {LOGMEIN("GlobOpt.cpp] 17824\n");
            noImplicitCallUses->SetSrc2(noImplicitCallUsesToInsert->Item(i));
            noImplicitCallUses->GetSrc2()->SetIsJITOptimizedReg(true);
            ++i;
        }
        noImplicitCallUses->SetByteCodeOffset(instr);
        insertBeforeInstr->InsertBefore(noImplicitCallUses);
    }
    noImplicitCallUsesToInsert->Clear();
}

void
GlobOpt::PrepareLoopArrayCheckHoist()
{LOGMEIN("GlobOpt.cpp] 17837\n");
    if(IsLoopPrePass() || !currentBlock->loop || !currentBlock->isLoopHeader || !currentBlock->loop->parent)
    {LOGMEIN("GlobOpt.cpp] 17839\n");
        return;
    }

    if(currentBlock->loop->parent->needImplicitCallBailoutChecksForJsArrayCheckHoist)
    {LOGMEIN("GlobOpt.cpp] 17844\n");
        // If the parent loop is an array check elimination candidate, so is the current loop. Even though the current loop may
        // not have array accesses, if the parent loop hoists array checks, the current loop also needs implicit call checks.
        currentBlock->loop->needImplicitCallBailoutChecksForJsArrayCheckHoist = true;
    }
}

JsArrayKills
GlobOpt::CheckJsArrayKills(IR::Instr *const instr)
{LOGMEIN("GlobOpt.cpp] 17853\n");
    Assert(instr);

    JsArrayKills kills;
    if(instr->UsesAllFields())
    {LOGMEIN("GlobOpt.cpp] 17858\n");
        // Calls can (but are unlikely to) change a javascript array into an ES5 array, which may have different behavior for
        // index properties.
        kills.SetKillsAllArrays();
        return kills;
    }

    const bool doArrayMissingValueCheckHoist = DoArrayMissingValueCheckHoist();
    const bool doNativeArrayTypeSpec = DoNativeArrayTypeSpec();
    const bool doArraySegmentHoist = DoArraySegmentHoist(ValueType::GetObject(ObjectType::Array));
    Assert(doArraySegmentHoist == DoArraySegmentHoist(ValueType::GetObject(ObjectType::ObjectWithArray)));
    const bool doArrayLengthHoist = DoArrayLengthHoist();
    if(!doArrayMissingValueCheckHoist && !doNativeArrayTypeSpec && !doArraySegmentHoist && !doArrayLengthHoist)
    {LOGMEIN("GlobOpt.cpp] 17871\n");
        return kills;
    }

    // The following operations may create missing values in an array in an unlikely circumstance. Even though they don't kill
    // the fact that the 'this' parameter is an array (when implicit calls are disabled), we don't have a way to say the value
    // type is definitely array but it likely has no missing values. So, these will kill the definite value type as well, making
    // it likely array, such that the array checks will have to be redone.
    const bool useValueTypes = !IsLoopPrePass(); // Source value types are not guaranteed to be correct in a loop prepass
    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 17881\n");
        case Js::OpCode::StElemI_A:
        case Js::OpCode::StElemI_A_Strict:
        {LOGMEIN("GlobOpt.cpp] 17884\n");
            Assert(instr->GetDst());
            if(!instr->GetDst()->IsIndirOpnd())
            {LOGMEIN("GlobOpt.cpp] 17887\n");
                break;
            }
            const ValueType baseValueType =
                useValueTypes ? instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetValueType() : ValueType::Uninitialized;
            if(useValueTypes && baseValueType.IsNotArrayOrObjectWithArray())
            {LOGMEIN("GlobOpt.cpp] 17893\n");
                break;
            }
            if(instr->IsProfiledInstr())
            {LOGMEIN("GlobOpt.cpp] 17897\n");
                const Js::StElemInfo *const stElemInfo = instr->AsProfiledInstr()->u.stElemInfo;
                if(doArraySegmentHoist && stElemInfo->LikelyStoresOutsideHeadSegmentBounds())
                {LOGMEIN("GlobOpt.cpp] 17900\n");
                    kills.SetKillsArrayHeadSegments();
                    kills.SetKillsArrayHeadSegmentLengths();
                }
                if(doArrayLengthHoist &&
                    !(useValueTypes && baseValueType.IsNotArray()) &&
                    stElemInfo->LikelyStoresOutsideArrayBounds())
                {LOGMEIN("GlobOpt.cpp] 17907\n");
                    kills.SetKillsArrayLengths();
                }
            }
            break;
        }

        case Js::OpCode::DeleteElemI_A:
        case Js::OpCode::DeleteElemIStrict_A:
            Assert(instr->GetSrc1());
            if(!instr->GetSrc1()->IsIndirOpnd() ||
                (useValueTypes && instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetValueType().IsNotArrayOrObjectWithArray()))
            {LOGMEIN("GlobOpt.cpp] 17919\n");
                break;
            }
            if(doArrayMissingValueCheckHoist)
            {LOGMEIN("GlobOpt.cpp] 17923\n");
                kills.SetKillsArraysWithNoMissingValues();
            }
            if(doArraySegmentHoist)
            {LOGMEIN("GlobOpt.cpp] 17927\n");
                kills.SetKillsArrayHeadSegmentLengths();
            }
            break;

        case Js::OpCode::StFld:
        case Js::OpCode::StFldStrict:
        {LOGMEIN("GlobOpt.cpp] 17934\n");
            Assert(instr->GetDst());

            if(!doArraySegmentHoist && !doArrayLengthHoist)
            {LOGMEIN("GlobOpt.cpp] 17938\n");
                break;
            }

            IR::SymOpnd *const symDst = instr->GetDst()->AsSymOpnd();
            if(!symDst->IsPropertySymOpnd())
            {LOGMEIN("GlobOpt.cpp] 17944\n");
                break;
            }

            IR::PropertySymOpnd *const dst = symDst->AsPropertySymOpnd();
            if(dst->m_sym->AsPropertySym()->m_propertyId != Js::PropertyIds::length)
            {LOGMEIN("GlobOpt.cpp] 17950\n");
                break;
            }

            if(useValueTypes && dst->GetPropertyOwnerValueType().IsNotArray())
            {LOGMEIN("GlobOpt.cpp] 17955\n");
                // Setting the 'length' property of an object that is not an array, even if it has an internal array, does
                // not kill the head segment or head segment length of any arrays.
                break;
            }

            if(doArraySegmentHoist)
            {LOGMEIN("GlobOpt.cpp] 17962\n");
                kills.SetKillsArrayHeadSegmentLengths();
            }
            if(doArrayLengthHoist)
            {LOGMEIN("GlobOpt.cpp] 17966\n");
                kills.SetKillsArrayLengths();
            }
            break;
        }

        case Js::OpCode::InlineArrayPush:
        {LOGMEIN("GlobOpt.cpp] 17973\n");
            Assert(instr->GetSrc2());
            IR::Opnd *const arrayOpnd = instr->GetSrc1();
            Assert(arrayOpnd);

            const ValueType arrayValueType(arrayOpnd->GetValueType());

            if(!arrayOpnd->IsRegOpnd() || (useValueTypes && arrayValueType.IsNotArrayOrObjectWithArray()))
            {LOGMEIN("GlobOpt.cpp] 17981\n");
                break;
            }

            if(doArrayMissingValueCheckHoist)
            {LOGMEIN("GlobOpt.cpp] 17986\n");
                kills.SetKillsArraysWithNoMissingValues();
            }

            if(doArraySegmentHoist)
            {LOGMEIN("GlobOpt.cpp] 17991\n");
                kills.SetKillsArrayHeadSegments();
                kills.SetKillsArrayHeadSegmentLengths();
            }

            if(doArrayLengthHoist && !(useValueTypes && arrayValueType.IsNotArray()))
            {LOGMEIN("GlobOpt.cpp] 17997\n");
                kills.SetKillsArrayLengths();
            }

            // Don't kill NativeArray, if there is no mismatch between array's type and element's type.
            if(doNativeArrayTypeSpec &&
               !(useValueTypes && arrayValueType.IsNativeArray() &&
                    ((arrayValueType.IsLikelyNativeIntArray() && instr->GetSrc2()->IsInt32()) ||
                     (arrayValueType.IsLikelyNativeFloatArray() && instr->GetSrc2()->IsFloat()))
                ) &&
               !(useValueTypes && arrayValueType.IsNotNativeArray()))
            {LOGMEIN("GlobOpt.cpp] 18008\n");
                kills.SetKillsNativeArrays();
            }

            break;
        }

        case Js::OpCode::InlineArrayPop:
        {LOGMEIN("GlobOpt.cpp] 18016\n");
            IR::Opnd *const arrayOpnd = instr->GetSrc1();
            Assert(arrayOpnd);

            const ValueType arrayValueType(arrayOpnd->GetValueType());
            if(!arrayOpnd->IsRegOpnd() || (useValueTypes && arrayValueType.IsNotArrayOrObjectWithArray()))
            {LOGMEIN("GlobOpt.cpp] 18022\n");
                break;
            }

            if(doArraySegmentHoist)
            {LOGMEIN("GlobOpt.cpp] 18027\n");
                kills.SetKillsArrayHeadSegmentLengths();
            }

            if(doArrayLengthHoist && !(useValueTypes && arrayValueType.IsNotArray()))
            {LOGMEIN("GlobOpt.cpp] 18032\n");
                kills.SetKillsArrayLengths();
            }
            break;
        }

        case Js::OpCode::CallDirect:
        {LOGMEIN("GlobOpt.cpp] 18039\n");
            Assert(instr->GetSrc1());

            // Find the 'this' parameter and check if it's possible for it to be an array
            IR::Opnd *const arrayOpnd = instr->FindCallArgumentOpnd(1);
            Assert(arrayOpnd);
            const ValueType arrayValueType(arrayOpnd->GetValueType());
            if(!arrayOpnd->IsRegOpnd() || (useValueTypes && arrayValueType.IsNotArrayOrObjectWithArray()))
            {LOGMEIN("GlobOpt.cpp] 18047\n");
                break;
            }

            const IR::JnHelperMethod helperMethod = instr->GetSrc1()->AsHelperCallOpnd()->m_fnHelper;
            if(doArrayMissingValueCheckHoist)
            {LOGMEIN("GlobOpt.cpp] 18053\n");
                switch(helperMethod)
                {LOGMEIN("GlobOpt.cpp] 18055\n");
                    case IR::HelperArray_Reverse:
                    case IR::HelperArray_Shift:
                    case IR::HelperArray_Splice:
                    case IR::HelperArray_Unshift:
                        kills.SetKillsArraysWithNoMissingValues();
                        break;
                }
            }

            if(doArraySegmentHoist)
            {LOGMEIN("GlobOpt.cpp] 18066\n");
                switch(helperMethod)
                {LOGMEIN("GlobOpt.cpp] 18068\n");
                    case IR::HelperArray_Reverse:
                    case IR::HelperArray_Shift:
                    case IR::HelperArray_Splice:
                    case IR::HelperArray_Unshift:
                        kills.SetKillsArrayHeadSegments();
                        kills.SetKillsArrayHeadSegmentLengths();
                        break;
                }
            }

            if(doArrayLengthHoist && !(useValueTypes && arrayValueType.IsNotArray()))
            {LOGMEIN("GlobOpt.cpp] 18080\n");
                switch(helperMethod)
                {LOGMEIN("GlobOpt.cpp] 18082\n");
                    case IR::HelperArray_Shift:
                    case IR::HelperArray_Splice:
                    case IR::HelperArray_Unshift:
                        kills.SetKillsArrayLengths();
                        break;
                }
            }

            if(doNativeArrayTypeSpec && !(useValueTypes && arrayValueType.IsNotNativeArray()))
            {LOGMEIN("GlobOpt.cpp] 18092\n");
                switch(helperMethod)
                {LOGMEIN("GlobOpt.cpp] 18094\n");
                    case IR::HelperArray_Reverse:
                    case IR::HelperArray_Shift:
                    case IR::HelperArray_Slice:
                    // Currently not inlined.
                    //case IR::HelperArray_Sort:
                    case IR::HelperArray_Splice:
                    case IR::HelperArray_Unshift:
                        kills.SetKillsNativeArrays();
                        break;
                }
            }
            break;
        }
    }

    return kills;
}

bool
GlobOpt::IsOperationThatLikelyKillsJsArraysWithNoMissingValues(IR::Instr *const instr)
{LOGMEIN("GlobOpt.cpp] 18115\n");
    // StElem is profiled with information indicating whether it will likely create a missing value in the array. In that case,
    // we prefer to kill the no-missing-values information in the value so that we don't bail out in a likely circumstance.
    return
        (instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict) &&
        DoArrayMissingValueCheckHoist() &&
        instr->IsProfiledInstr() &&
        instr->AsProfiledInstr()->u.stElemInfo->LikelyCreatesMissingValue();
}

bool
GlobOpt::NeedBailOnImplicitCallForArrayCheckHoist(BasicBlock *const block, const bool isForwardPass) const
{LOGMEIN("GlobOpt.cpp] 18127\n");
    Assert(block);
    return isForwardPass && block->loop && block->loop->needImplicitCallBailoutChecksForJsArrayCheckHoist;
}

bool
GlobOpt::PrepareForIgnoringIntOverflow(IR::Instr *const instr)
{LOGMEIN("GlobOpt.cpp] 18134\n");
    Assert(instr);

    const bool isBoundary = instr->m_opcode == Js::OpCode::NoIntOverflowBoundary;

    // Update the instruction's "int overflow matters" flag based on whether we are currently allowing ignoring int overflows.
    // Some operations convert their srcs to int32s, those can still ignore int overflow.
    if(instr->ignoreIntOverflowInRange)
    {LOGMEIN("GlobOpt.cpp] 18142\n");
        instr->ignoreIntOverflowInRange = !intOverflowCurrentlyMattersInRange || OpCodeAttr::IsInt32(instr->m_opcode);
    }

    if(!intOverflowDoesNotMatterRange)
    {LOGMEIN("GlobOpt.cpp] 18147\n");
        Assert(intOverflowCurrentlyMattersInRange);

        // There are no more ranges of instructions where int overflow does not matter, in this block.
        return isBoundary;
    }

    if(instr == intOverflowDoesNotMatterRange->LastInstr())
    {LOGMEIN("GlobOpt.cpp] 18155\n");
        Assert(isBoundary);

        // Reached the last instruction in the range
        intOverflowCurrentlyMattersInRange = true;
        intOverflowDoesNotMatterRange = intOverflowDoesNotMatterRange->Next();
        return isBoundary;
    }

    if(!intOverflowCurrentlyMattersInRange)
    {LOGMEIN("GlobOpt.cpp] 18165\n");
        return isBoundary;
    }

    if(instr != intOverflowDoesNotMatterRange->FirstInstr())
    {LOGMEIN("GlobOpt.cpp] 18170\n");
        // Have not reached the next range
        return isBoundary;
    }

    Assert(isBoundary);

    // This is the first instruction in a range of instructions where int overflow does not matter. There can be many inputs to
    // instructions in the range, some of which are inputs to the range itself (that is, the values are not defined in the
    // range). Ignoring int overflow is only valid for int operations, so we need to ensure that all inputs to the range are
    // int (not "likely int") before ignoring any overflows in the range. Ensuring that a sym with a "likely int" value is an
    // int requires a bail-out. These bail-out check need to happen before any overflows are ignored, otherwise it's too late.
    // The backward pass tracked all inputs into the range. Iterate over them and verify the values, and insert lossless
    // conversions to int as necessary, before the first instruction in the range. If for any reason all values cannot be
    // guaranteed to be ints, the optimization will be disabled for this range.
    intOverflowCurrentlyMattersInRange = false;
    {
        BVSparse<JitArenaAllocator> tempBv1(tempAlloc);
        BVSparse<JitArenaAllocator> tempBv2(tempAlloc);

        {LOGMEIN("GlobOpt.cpp] 18190\n");
            // Just renaming the temp BVs for this section to indicate how they're used so that it makes sense
            BVSparse<JitArenaAllocator> &symsToExclude = tempBv1;
            BVSparse<JitArenaAllocator> &symsToInclude = tempBv2;
#if DBG_DUMP
            SymID couldNotConvertSymId = 0;
#endif
            FOREACH_BITSET_IN_SPARSEBV(id, intOverflowDoesNotMatterRange->SymsRequiredToBeInt())
            {LOGMEIN("GlobOpt.cpp] 18198\n");
                Sym *const sym = func->m_symTable->Find(id);
                Assert(sym);

                // Some instructions with property syms are also tracked by the backward pass, and may be included in the range
                // (LdSlot for instance). These property syms don't get their values until either copy-prop resolves a value for
                // them, or a new value is created once the use of the property sym is reached. In either case, we're not that
                // far yet, so we need to find the future value of the property sym by evaluating copy-prop in reverse.
                Value *const value = sym->IsStackSym() ? FindValue(sym) : FindFuturePropertyValue(sym->AsPropertySym());
                if(!value)
                {LOGMEIN("GlobOpt.cpp] 18208\n");
#if DBG_DUMP
                    couldNotConvertSymId = id;
#endif
                    intOverflowCurrentlyMattersInRange = true;
                    BREAK_BITSET_IN_SPARSEBV;
                }

                const bool isInt32OrUInt32Float =
                    value->GetValueInfo()->IsFloatConstant() &&
                    Js::JavascriptNumber::IsInt32OrUInt32(value->GetValueInfo()->AsFloatConstant()->FloatValue());
                if(value->GetValueInfo()->IsInt() || isInt32OrUInt32Float)
                {LOGMEIN("GlobOpt.cpp] 18220\n");
                    if(!IsLoopPrePass())
                    {LOGMEIN("GlobOpt.cpp] 18222\n");
                        // Input values that are already int can be excluded from int-specialization. We can treat unsigned
                        // int32 values as int32 values (ignoring the overflow), since the values will only be used inside the
                        // range where overflow does not matter.
                        symsToExclude.Set(sym->m_id);
                    }
                    continue;
                }

                if(!DoAggressiveIntTypeSpec() || !value->GetValueInfo()->IsLikelyInt())
                {LOGMEIN("GlobOpt.cpp] 18232\n");
                    // When aggressive int specialization is off, syms with "likely int" values cannot be forced to int since
                    // int bail-out checks are not allowed in that mode. Similarly, with aggressive int specialization on, it
                    // wouldn't make sense to force non-"likely int" values to int since it would almost guarantee a bail-out at
                    // runtime. In both cases, just disable ignoring overflow for this range.
#if DBG_DUMP
                    couldNotConvertSymId = id;
#endif
                    intOverflowCurrentlyMattersInRange = true;
                    BREAK_BITSET_IN_SPARSEBV;
                }

                if(IsLoopPrePass())
                {LOGMEIN("GlobOpt.cpp] 18245\n");
                    // The loop prepass does not modify bit-vectors. Since it doesn't add bail-out checks, it also does not need
                    // to specialize anything up-front. It only needs to be consistent in how it determines whether to allow
                    // ignoring overflow for a range, based on the values of inputs into the range.
                    continue;
                }

                // Since input syms are tracked in the backward pass, where there is no value tracking, it will not be aware of
                // copy-prop. If a copy-prop sym is available, it will be used instead, so exclude the original sym and include
                // the copy-prop sym for specialization.
                StackSym *const copyPropSym = GetCopyPropSym(sym, value);
                if(copyPropSym)
                {LOGMEIN("GlobOpt.cpp] 18257\n");
                    symsToExclude.Set(sym->m_id);
                    Assert(!symsToExclude.Test(copyPropSym->m_id));

                    const bool needsToBeLossless =
                        !intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Test(sym->m_id);
                    if(intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Test(copyPropSym->m_id) ||
                        symsToInclude.TestAndSet(copyPropSym->m_id))
                    {LOGMEIN("GlobOpt.cpp] 18265\n");
                        // The copy-prop sym is already included
                        if(needsToBeLossless)
                        {LOGMEIN("GlobOpt.cpp] 18268\n");
                            // The original sym needs to be lossless, so make the copy-prop sym lossless as well.
                            intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Clear(copyPropSym->m_id);
                        }
                    }
                    else if(!needsToBeLossless)
                    {LOGMEIN("GlobOpt.cpp] 18274\n");
                        // The copy-prop sym was not included before, and the original sym can be lossy, so make it lossy.
                        intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Set(copyPropSym->m_id);
                    }
                }
                else if(!sym->IsStackSym())
                {LOGMEIN("GlobOpt.cpp] 18280\n");
                    // Only stack syms can be converted to int, and copy-prop syms are stack syms. If a copy-prop sym was not
                    // found for the property sym, we can't ignore overflows in this range.
#if DBG_DUMP
                    couldNotConvertSymId = id;
#endif
                    intOverflowCurrentlyMattersInRange = true;
                    BREAK_BITSET_IN_SPARSEBV;
                }
            } NEXT_BITSET_IN_SPARSEBV;

            if(intOverflowCurrentlyMattersInRange)
            {LOGMEIN("GlobOpt.cpp] 18292\n");
#if DBG_DUMP
                if(PHASE_TRACE(Js::TrackCompoundedIntOverflowPhase, func) && !IsLoopPrePass())
                {LOGMEIN("GlobOpt.cpp] 18295\n");
                    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Output::Print(
                        _u("TrackCompoundedIntOverflow - Top function: %s (%s), Phase: %s, Block: %u, Disabled ignoring overflows\n"),
                        func->GetJITFunctionBody()->GetDisplayName(),
                        func->GetDebugNumberSet(debugStringBuffer),
                        Js::PhaseNames[Js::ForwardPhase],
                        currentBlock->GetBlockNum());
                    Output::Print(_u("    Input sym could not be turned into an int:   %u\n"), couldNotConvertSymId);
                    Output::Print(_u("    First instr: "));
                    instr->m_next->Dump();
                    Output::Flush();
                }
#endif
                intOverflowDoesNotMatterRange = intOverflowDoesNotMatterRange->Next();
                return isBoundary;
            }

            if(IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 18314\n");
                return isBoundary;
            }

            // Update the syms to specialize after enumeration
            intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Minus(&symsToExclude);
            intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Minus(&symsToExclude);
            intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Or(&symsToInclude);
        }

        {LOGMEIN("GlobOpt.cpp] 18324\n");
            // Exclude syms that are already live as lossless int32, and exclude lossy conversions of syms that are already live
            // as lossy int32.
            //     symsToExclude = liveInt32Syms - liveLossyInt32Syms                   // syms live as lossless int
            //     lossySymsToExclude = symsRequiredToBeLossyInt & liveLossyInt32Syms;  // syms we want as lossy int that are already live as lossy int
            //     symsToExclude |= lossySymsToExclude
            //     symsRequiredToBeInt -= symsToExclude
            //     symsRequiredToBeLossyInt -= symsToExclude
            BVSparse<JitArenaAllocator> &symsToExclude = tempBv1;
            BVSparse<JitArenaAllocator> &lossySymsToExclude = tempBv2;
            symsToExclude.Minus(currentBlock->globOptData.liveInt32Syms, currentBlock->globOptData.liveLossyInt32Syms);
            lossySymsToExclude.And(
                intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt(),
                currentBlock->globOptData.liveLossyInt32Syms);
            symsToExclude.Or(&lossySymsToExclude);
            intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Minus(&symsToExclude);
            intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Minus(&symsToExclude);
        }

#if DBG
        {
            // Verify that the syms to be converted are live
            //     liveSyms = liveInt32Syms | liveFloat64Syms | liveVarSyms
            //     deadSymsRequiredToBeInt = symsRequiredToBeInt - liveSyms
            BVSparse<JitArenaAllocator> &liveSyms = tempBv1;
            BVSparse<JitArenaAllocator> &deadSymsRequiredToBeInt = tempBv2;
            liveSyms.Or(currentBlock->globOptData.liveInt32Syms, currentBlock->globOptData.liveFloat64Syms);
            liveSyms.Or(currentBlock->globOptData.liveVarSyms);
            deadSymsRequiredToBeInt.Minus(intOverflowDoesNotMatterRange->SymsRequiredToBeInt(), &liveSyms);
            Assert(deadSymsRequiredToBeInt.IsEmpty());
        }
#endif
    }

    // Int-specialize the syms before the first instruction of the range (the current instruction)
    intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Minus(intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt());

#if DBG_DUMP
    if(PHASE_TRACE(Js::TrackCompoundedIntOverflowPhase, func))
    {LOGMEIN("GlobOpt.cpp] 18363\n");
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(
            _u("TrackCompoundedIntOverflow - Top function: %s (%s), Phase: %s, Block: %u\n"),
            func->GetJITFunctionBody()->GetDisplayName(),
            func->GetDebugNumberSet(debugStringBuffer),
            Js::PhaseNames[Js::ForwardPhase],
            currentBlock->GetBlockNum());
        Output::Print(_u("    Input syms to be int-specialized (lossless): "));
        intOverflowDoesNotMatterRange->SymsRequiredToBeInt()->Dump();
        Output::Print(_u("    Input syms to be converted to int (lossy):   "));
        intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt()->Dump();
        Output::Print(_u("    First instr: "));
        instr->m_next->Dump();
        Output::Flush();
    }
#endif

    ToInt32(intOverflowDoesNotMatterRange->SymsRequiredToBeInt(), currentBlock, false /* lossy */, instr);
    ToInt32(intOverflowDoesNotMatterRange->SymsRequiredToBeLossyInt(), currentBlock, true /* lossy */, instr);
    return isBoundary;
}

void
GlobOpt::VerifyIntSpecForIgnoringIntOverflow(IR::Instr *const instr)
{LOGMEIN("GlobOpt.cpp] 18388\n");
    if(intOverflowCurrentlyMattersInRange || IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 18390\n");
        return;
    }

    Assert(instr->m_opcode != Js::OpCode::Mul_I4 ||
        (instr->m_opcode == Js::OpCode::Mul_I4 && !instr->ShouldCheckFor32BitOverflow() && instr->ShouldCheckForNon32BitOverflow() ));

    // Instructions that are marked as "overflow doesn't matter" in the range must guarantee that they operate on int values and
    // result in int values, for ignoring overflow to be valid. So, int-specialization is required for such instructions in the
    // range. Ld_A is an exception because it only specializes if the src sym is available as a required specialized sym, and it
    // doesn't generate bailouts or cause ignoring int overflow to be invalid.
    // MULs are allowed to start a region and have BailOutInfo since they will bailout on non-32 bit overflow.
    if(instr->m_opcode == Js::OpCode::Ld_A ||
       ((!instr->HasBailOutInfo() || instr->m_opcode == Js::OpCode::Mul_I4) &&
        (!instr->GetDst() || instr->GetDst()->IsInt32()) &&
        (!instr->GetSrc1() || instr->GetSrc1()->IsInt32()) &&
        (!instr->GetSrc2() || instr->GetSrc2()->IsInt32())))
    {LOGMEIN("GlobOpt.cpp] 18407\n");
        return;
    }

    if (!instr->HasBailOutInfo() && !instr->HasAnySideEffects())
    {LOGMEIN("GlobOpt.cpp] 18412\n");
        return;
    }

    // This can happen for Neg_A if it needs to bail out on negative zero, and perhaps other cases as well. It's too late to fix
    // the problem (overflows may already be ignored), so handle it by bailing out at compile-time and disabling tracking int
    // overflow.
    Assert(!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsTrackCompoundedIntOverflowDisabled());

    if(PHASE_TRACE(Js::BailOutPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 18422\n");
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(
            _u("BailOut (compile-time): function: %s (%s) instr: "),
            func->GetJITFunctionBody()->GetDisplayName(),
            func->GetDebugNumberSet(debugStringBuffer));
#if DBG_DUMP
        instr->Dump();
#else
        Output::Print(_u("%s "), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
#endif
        Output::Print(_u("(overflow does not matter but could not int-spec or needed bailout)\n"));
        Output::Flush();
    }

    if(func->HasProfileInfo() && func->GetReadOnlyProfileInfo()->IsTrackCompoundedIntOverflowDisabled())
    {LOGMEIN("GlobOpt.cpp] 18438\n");
        // Tracking int overflows is already off for some reason. Prevent trying to rejit again because it won't help and the
        // same thing will happen again and cause an infinite loop. Just abort jitting this function.
        if(PHASE_TRACE(Js::BailOutPhase, this->func))
        {LOGMEIN("GlobOpt.cpp] 18442\n");
            Output::Print(_u("    Aborting JIT because TrackIntOverflow is already off\n"));
            Output::Flush();
        }
        throw Js::OperationAbortedException();
    }

    throw Js::RejitException(RejitReason::TrackIntOverflowDisabled);
}

// It makes lowering easier if it can assume that the first src is never a constant,
// at least for commutative operators. For non-commutative, just hoist the constant.
void
GlobOpt::PreLowerCanonicalize(IR::Instr *instr, Value **pSrc1Val, Value **pSrc2Val)
{LOGMEIN("GlobOpt.cpp] 18456\n");
    IR::Opnd *dst = instr->GetDst();
    IR::Opnd *src1 = instr->GetSrc1();
    IR::Opnd *src2 = instr->GetSrc2();

    if (src1->IsImmediateOpnd())
    {LOGMEIN("GlobOpt.cpp] 18462\n");
        // Swap for dst, src
    }
    else if (src2 && dst && src2->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 18466\n");
        if (src2->GetIsDead() && !src1->GetIsDead() && !src1->IsEqual(dst))
        {LOGMEIN("GlobOpt.cpp] 18468\n");
            // Swap if src2 is dead, as the reg can be reuse for the dst for opEqs like on x86 (ADD r1, r2)
        }
        else if (src2->IsEqual(dst))
        {LOGMEIN("GlobOpt.cpp] 18472\n");
            // Helps lowering of opEqs
        }
        else
        {
            return;
        }
        // Make sure we don't swap 2 srcs with valueOf calls.
        if (OpCodeAttr::OpndHasImplicitCall(instr->m_opcode))
        {LOGMEIN("GlobOpt.cpp] 18481\n");
            if (instr->IsBranchInstr())
            {LOGMEIN("GlobOpt.cpp] 18483\n");
                if (!src1->GetValueType().IsPrimitive() || !src2->GetValueType().IsPrimitive())
                {LOGMEIN("GlobOpt.cpp] 18485\n");
                    return;
                }
            }
            else if (!src1->GetValueType().IsPrimitive() && !src2->GetValueType().IsPrimitive())
            {LOGMEIN("GlobOpt.cpp] 18490\n");
                return;
            }
        }
    }
    else
    {
        return;
    }
    Js::OpCode opcode = instr->m_opcode;
    switch (opcode)
    {LOGMEIN("GlobOpt.cpp] 18501\n");
    case Js::OpCode::And_A:
    case Js::OpCode::Mul_A:
    case Js::OpCode::Or_A:
    case Js::OpCode::Xor_A:
    case Js::OpCode::And_I4:
    case Js::OpCode::Mul_I4:
    case Js::OpCode::Or_I4:
    case Js::OpCode::Xor_I4:
    case Js::OpCode::Add_I4:
    case Js::OpCode::Add_Ptr:
swap_srcs:
        if (!instr->GetSrc2()->IsImmediateOpnd())
        {LOGMEIN("GlobOpt.cpp] 18514\n");
            instr->m_opcode = opcode;
            src1 = instr->UnlinkSrc1();
            src2 = instr->UnlinkSrc2();
            instr->SetSrc1(src2);
            instr->SetSrc2(src1);

            Value *tempVal = *pSrc1Val;
            *pSrc1Val = *pSrc2Val;
            *pSrc2Val = tempVal;
            return;
        }
        break;

    case Js::OpCode::BrSrEq_A:
    case Js::OpCode::BrSrNotNeq_A:
    case Js::OpCode::BrEq_I4:
        goto swap_srcs;

    case Js::OpCode::BrSrNeq_A:
    case Js::OpCode::BrNeq_A:
    case Js::OpCode::BrSrNotEq_A:
    case Js::OpCode::BrNotEq_A:
    case Js::OpCode::BrNeq_I4:
        goto swap_srcs;

    case Js::OpCode::BrGe_A:
        opcode = Js::OpCode::BrLe_A;
        goto swap_srcs;

    case Js::OpCode::BrNotGe_A:
        opcode = Js::OpCode::BrNotLe_A;
        goto swap_srcs;

    case Js::OpCode::BrGe_I4:
        opcode = Js::OpCode::BrLe_I4;
        goto swap_srcs;

    case Js::OpCode::BrGt_A:
        opcode = Js::OpCode::BrLt_A;
        goto swap_srcs;

    case Js::OpCode::BrNotGt_A:
        opcode = Js::OpCode::BrNotLt_A;
        goto swap_srcs;

    case Js::OpCode::BrGt_I4:
        opcode = Js::OpCode::BrLt_I4;
        goto swap_srcs;

    case Js::OpCode::BrLe_A:
        opcode = Js::OpCode::BrGe_A;
        goto swap_srcs;

    case Js::OpCode::BrNotLe_A:
        opcode = Js::OpCode::BrNotGe_A;
        goto swap_srcs;

    case Js::OpCode::BrLe_I4:
        opcode = Js::OpCode::BrGe_I4;
        goto swap_srcs;

    case Js::OpCode::BrLt_A:
        opcode = Js::OpCode::BrGt_A;
        goto swap_srcs;

    case Js::OpCode::BrNotLt_A:
        opcode = Js::OpCode::BrNotGt_A;
        goto swap_srcs;

    case Js::OpCode::BrLt_I4:
        opcode = Js::OpCode::BrGt_I4;
        goto swap_srcs;

    case Js::OpCode::BrEq_A:
    case Js::OpCode::BrNotNeq_A:
    case Js::OpCode::CmEq_A:
    case Js::OpCode::CmNeq_A:
        // this == "" not the same as "" == this...
        if (!src1->IsImmediateOpnd() && (!src1->GetValueType().IsPrimitive() || !src2->GetValueType().IsPrimitive()))
        {LOGMEIN("GlobOpt.cpp] 18594\n");
            return;
        }
        goto swap_srcs;

    case Js::OpCode::CmGe_A:
        if (!src1->IsImmediateOpnd() && (!src1->GetValueType().IsPrimitive() || !src2->GetValueType().IsPrimitive()))
        {LOGMEIN("GlobOpt.cpp] 18601\n");
            return;
        }
        opcode = Js::OpCode::CmLe_A;
        goto swap_srcs;

    case Js::OpCode::CmGt_A:
        if (!src1->IsImmediateOpnd() && (!src1->GetValueType().IsPrimitive() || !src2->GetValueType().IsPrimitive()))
        {LOGMEIN("GlobOpt.cpp] 18609\n");
            return;
        }
        opcode = Js::OpCode::CmLt_A;
        goto swap_srcs;

    case Js::OpCode::CmLe_A:
        if (!src1->IsImmediateOpnd() && (!src1->GetValueType().IsPrimitive() || !src2->GetValueType().IsPrimitive()))
        {LOGMEIN("GlobOpt.cpp] 18617\n");
            return;
        }
        opcode = Js::OpCode::CmGe_A;
        goto swap_srcs;

    case Js::OpCode::CmLt_A:
        if (!src1->IsImmediateOpnd() && (!src1->GetValueType().IsPrimitive() || !src2->GetValueType().IsPrimitive()))
        {LOGMEIN("GlobOpt.cpp] 18625\n");
            return;
        }
        opcode = Js::OpCode::CmGt_A;
        goto swap_srcs;

    case Js::OpCode::CallI:
    case Js::OpCode::CallIFixed:
    case Js::OpCode::NewScObject:
    case Js::OpCode::NewScObjectSpread:
    case Js::OpCode::NewScObjArray:
    case Js::OpCode::NewScObjArraySpread:
    case Js::OpCode::NewScObjectNoCtor:
        // Don't insert load to register if the function operand is a fixed function.
        if (instr->HasFixedFunctionAddressTarget())
        {LOGMEIN("GlobOpt.cpp] 18640\n");
            return;
        }
        break;

        // Can't do add because <32 + "Hello"> isn't equal to <"Hello" + 32>
        // Lower can do the swap. Other op-codes listed below don't need immediate source hoisting, as the fast paths handle it,
        // or the lowering handles the hoisting.
    case Js::OpCode::Add_A:
        if (src1->IsFloat())
        {LOGMEIN("GlobOpt.cpp] 18650\n");
            goto swap_srcs;
        }
        return;

    case Js::OpCode::Sub_I4:
    case Js::OpCode::Neg_I4:
    case Js::OpCode::Not_I4:
    case Js::OpCode::NewScFunc:
    case Js::OpCode::NewScGenFunc:
    case Js::OpCode::NewScArray:
    case Js::OpCode::NewScIntArray:
    case Js::OpCode::NewScFltArray:
    case Js::OpCode::NewScArrayWithMissingValues:
    case Js::OpCode::NewRegEx:
    case Js::OpCode::Ld_A:
    case Js::OpCode::Ld_I4:
    case Js::OpCode::FromVar:
    case Js::OpCode::Conv_Prim:
    case Js::OpCode::LdC_A_I4:
    case Js::OpCode::LdStr:
    case Js::OpCode::InitFld:
    case Js::OpCode::InitRootFld:
    case Js::OpCode::StartCall:
    case Js::OpCode::ArgOut_A:
    case Js::OpCode::ArgOut_A_Inline:
    case Js::OpCode::ArgOut_A_Dynamic:
    case Js::OpCode::ArgOut_A_FromStackArgs:
    case Js::OpCode::ArgOut_A_InlineBuiltIn:
    case Js::OpCode::ArgOut_A_InlineSpecialized:
    case Js::OpCode::ArgOut_A_SpreadArg:
    case Js::OpCode::InlineeEnd:
    case Js::OpCode::EndCallForPolymorphicInlinee:
    case Js::OpCode::InlineeMetaArg:
    case Js::OpCode::InlineBuiltInEnd:
    case Js::OpCode::InlineNonTrackingBuiltInEnd:
    case Js::OpCode::CallHelper:
    case Js::OpCode::LdElemUndef:
    case Js::OpCode::LdElemUndefScoped:
    case Js::OpCode::RuntimeTypeError:
    case Js::OpCode::RuntimeReferenceError:
    case Js::OpCode::Ret:
    case Js::OpCode::NewScObjectSimple:
    case Js::OpCode::NewScObjectLiteral:
    case Js::OpCode::StFld:
    case Js::OpCode::StRootFld:
    case Js::OpCode::StSlot:
    case Js::OpCode::StSlotChkUndecl:
    case Js::OpCode::StElemC:
    case Js::OpCode::StArrSegElemC:
    case Js::OpCode::StElemI_A:
    case Js::OpCode::StElemI_A_Strict:
    case Js::OpCode::CallDirect:
    case Js::OpCode::BrNotHasSideEffects:
    case Js::OpCode::NewConcatStrMulti:
    case Js::OpCode::NewConcatStrMultiBE:
    case Js::OpCode::ExtendArg_A:
#ifdef ENABLE_DOM_FAST_PATH
    case Js::OpCode::DOMFastPathGetter:
    case Js::OpCode::DOMFastPathSetter:
#endif
    case Js::OpCode::NewScopeSlots:
    case Js::OpCode::NewScopeSlotsWithoutPropIds:
    case Js::OpCode::NewStackScopeSlots:
    case Js::OpCode::IsInst:
    case Js::OpCode::BailOnEqual:
    case Js::OpCode::BailOnNotEqual:
    case Js::OpCode::StArrViewElem:
        return;
    }

    if (!src1->IsImmediateOpnd())
    {LOGMEIN("GlobOpt.cpp] 18722\n");
        return;
    }

    // The fast paths or lowering of the remaining instructions may not support handling immediate opnds for the first src. The
    // immediate src1 is hoisted here into a separate instruction.
    if (src1->IsIntConstOpnd())
    {LOGMEIN("GlobOpt.cpp] 18729\n");
        IR::Instr *newInstr = instr->HoistSrc1(Js::OpCode::Ld_I4);
        ToInt32Dst(newInstr, newInstr->GetDst()->AsRegOpnd(), this->currentBlock);
    }
    else
    {
        instr->HoistSrc1(Js::OpCode::Ld_A);
    }
    src1 = instr->GetSrc1();
    src1->AsRegOpnd()->m_sym->SetIsConst();
}

// Clear the ValueMap pf the values invalidated by this instr.
void
GlobOpt::ProcessKills(IR::Instr *instr)
{LOGMEIN("GlobOpt.cpp] 18744\n");
    this->ProcessFieldKills(instr);
    this->ProcessValueKills(instr);
    this->ProcessArrayValueKills(instr);
}

bool
GlobOpt::OptIsInvariant(IR::Opnd *src, BasicBlock *block, Loop *loop, Value *srcVal, bool isNotTypeSpecConv, bool allowNonPrimitives)
{LOGMEIN("GlobOpt.cpp] 18752\n");
    if(!loop->CanHoistInvariants())
    {LOGMEIN("GlobOpt.cpp] 18754\n");
        return false;
    }

    Sym *sym;

    switch(src->GetKind())
    {LOGMEIN("GlobOpt.cpp] 18761\n");
    case IR::OpndKindAddr:
    case IR::OpndKindFloatConst:
    case IR::OpndKindIntConst:
        return true;

    case IR::OpndKindReg:
        sym = src->AsRegOpnd()->m_sym;
        break;

    case IR::OpndKindSym:
        sym = src->AsSymOpnd()->m_sym;
        if (src->AsSymOpnd()->IsPropertySymOpnd())
        {LOGMEIN("GlobOpt.cpp] 18774\n");
            if (src->AsSymOpnd()->AsPropertySymOpnd()->IsTypeChecked())
            {LOGMEIN("GlobOpt.cpp] 18776\n");
                // We do not handle hoisting these yet.  We might be hoisting this across the instr with the type check protecting this one.
                // And somehow, the dead-store pass now removes the type check on that instr later on...
                // For CheckFixedFld, there is no benefit hoisting these if they don't have a type check as they won't generate code.
                return false;
            }
        }

        break;

    case IR::OpndKindHelperCall:
        // Helper calls, like the private slot getter, can be invariant.
        // Consider moving more math builtin to invariant?
        return HelperMethodAttributes::IsInVariant(src->AsHelperCallOpnd()->m_fnHelper);

    default:
        return false;
    }
    return OptIsInvariant(sym, block, loop, srcVal, isNotTypeSpecConv, allowNonPrimitives);
}

bool
GlobOpt::OptIsInvariant(Sym *sym, BasicBlock *block, Loop *loop, Value *srcVal, bool isNotTypeSpecConv, bool allowNonPrimitives, Value **loopHeadValRef)
{LOGMEIN("GlobOpt.cpp] 18799\n");
    Value *localLoopHeadVal;
    if(!loopHeadValRef)
    {LOGMEIN("GlobOpt.cpp] 18802\n");
        loopHeadValRef = &localLoopHeadVal;
    }
    Value *&loopHeadVal = *loopHeadValRef;
    loopHeadVal = nullptr;

    if(!loop->CanHoistInvariants())
    {LOGMEIN("GlobOpt.cpp] 18809\n");
        return false;
    }

    if (sym->IsStackSym())
    {LOGMEIN("GlobOpt.cpp] 18814\n");
        if (sym->AsStackSym()->IsTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 18816\n");
            StackSym *varSym = sym->AsStackSym()->GetVarEquivSym(this->func);
            // Make sure the int32/float64 version of this is available.
            // Note: We could handle this by converting the src, but usually the
            // conversion is hoistable if this is hoistable anyway.
            // In some weird cases it may not be however, so we'll bail out.
            if (sym->AsStackSym()->IsInt32())
            {LOGMEIN("GlobOpt.cpp] 18823\n");
                Assert(block->globOptData.liveInt32Syms->Test(varSym->m_id));
                if (!loop->landingPad->globOptData.liveInt32Syms->Test(varSym->m_id) ||
                    (loop->landingPad->globOptData.liveLossyInt32Syms->Test(varSym->m_id) &&
                    !block->globOptData.liveLossyInt32Syms->Test(varSym->m_id)))
                {LOGMEIN("GlobOpt.cpp] 18828\n");
                    // Either the int32 sym is not live in the landing pad, or it's lossy in the landing pad and the
                    // instruction's block is using the lossless version. In either case, the instruction cannot be hoisted
                    // without doing a conversion of this operand.
                    return false;
                }
            }
            else if (sym->AsStackSym()->IsFloat64())
            {LOGMEIN("GlobOpt.cpp] 18836\n");
                if (!loop->landingPad->globOptData.liveFloat64Syms->Test(varSym->m_id))
                {LOGMEIN("GlobOpt.cpp] 18838\n");
                    return false;
                }
            }
            else
            {
                Assert(sym->AsStackSym()->IsSimd128());
                if (!loop->landingPad->globOptData.liveSimd128F4Syms->Test(varSym->m_id) && !loop->landingPad->globOptData.liveSimd128I4Syms->Test(varSym->m_id))
                {LOGMEIN("GlobOpt.cpp] 18846\n");
                    return false;
                }
            }

            sym = sym->AsStackSym()->GetVarEquivSym(this->func);
        }
        else
        {
            // Make sure the var version of this is available.
            // Note: We could handle this by converting the src, but usually the
            // conversion is hoistable if this is hoistable anyway.
            // In some weird cases it may not be however, so we'll bail out.
            if (!loop->landingPad->globOptData.liveVarSyms->Test(sym->m_id))
            {LOGMEIN("GlobOpt.cpp] 18860\n");
                return false;
            }
        }
    }
    else if (sym->IsPropertySym())
    {LOGMEIN("GlobOpt.cpp] 18866\n");
        if (!loop->landingPad->globOptData.liveFields->Test(sym->m_id))
        {LOGMEIN("GlobOpt.cpp] 18868\n");
            return false;
        }
    }
    else
    {
        return false;
    }

    // We rely on having a value.
    if (srcVal == NULL)
    {LOGMEIN("GlobOpt.cpp] 18879\n");
        return false;
    }

    // Can't hoist non-primitives, unless we have safeguards against valueof/tostring.
    if (!allowNonPrimitives && !srcVal->GetValueInfo()->IsPrimitive() && !this->IsTypeSpecialized(sym, loop->landingPad))
    {LOGMEIN("GlobOpt.cpp] 18885\n");
        return false;
    }

    if(!isNotTypeSpecConv && loop->symsDefInLoop->Test(sym->m_id))
    {LOGMEIN("GlobOpt.cpp] 18890\n");
        // Typically, a sym is considered invariant if it has the same value in the current block and in the loop landing pad.
        // The sym may have had a different value earlier in the loop or on the back-edge, but as long as it's reassigned to its
        // value outside the loop, it would be considered invariant in this block. Consider that case:
        //     s1 = s2[invariant]
        //     <loop start>
        //         s1 = s2[invariant]
        //                              // s1 now has the same value as in the landing pad, and is considered invariant
        //         s1 += s3
        //                              // s1 is not invariant here, or on the back-edge
        //         ++s3                 // s3 is not invariant, so the add above cannot be hoisted
        //     <loop end>
        //
        // A problem occurs at the point of (s1 += s3) when:
        //     - At (s1 = s2) inside the loop, s1 was made to be the sym store of that value. This by itself is legal, because
        //       after that transfer, s1 and s2 have the same value.
        //     - (s1 += s3) is type-specialized but s1 is not specialized in the loop header. This happens when s1 is not
        //       specialized entering the loop, and since s1 is not used before it's defined in the loop, it's not specialized
        //       on back-edges.
        //
        // With that, at (s1 += s3), the conversion of s1 to the type-specialized version would be hoisted because s1 is
        // invariant just before that instruction. Since this add is specialized, the specialized version of the sym is modified
        // in the loop without a reassignment at (s1 = s2) inside the loop, and (s1 += s3) would then use an incorrect value of
        // s1 (it would use the value of s1 from the previous loop iteration, instead of using the value of s2).
        //
        // The problem here, is that we cannot hoist the conversion of s1 into its specialized version across the assignment
        // (s1 = s2) inside the loop. So for the purposes of type specialization, don't consider a sym invariant if it has a def
        // inside the loop.
        return false;
    }

    // A symbol is invariant if it's current value is the same as it was upon entering the loop.
    loopHeadVal = this->FindValue(loop->landingPad->globOptData.symToValueMap, sym);

    if (loopHeadVal == NULL || loopHeadVal->GetValueNumber() != srcVal->GetValueNumber())
    {LOGMEIN("GlobOpt.cpp] 18925\n");
        return false;
    }

    // For values with an int range, require additionally that the range is the same as in the landing pad, as the range may
    // have been changed on this path based on branches, and int specialization and invariant hoisting may rely on the range
    // being the same. For type spec conversions, only require that if the value is an int constant in the current block, that
    // it is also an int constant with the same value in the landing pad. Other range differences don't matter for type spec.
    IntConstantBounds srcIntConstantBounds, loopHeadIntConstantBounds;
    if(srcVal->GetValueInfo()->TryGetIntConstantBounds(&srcIntConstantBounds) &&
        (isNotTypeSpecConv || srcIntConstantBounds.IsConstant()) &&
        (
            !loopHeadVal->GetValueInfo()->TryGetIntConstantBounds(&loopHeadIntConstantBounds) ||
            loopHeadIntConstantBounds.LowerBound() != srcIntConstantBounds.LowerBound() ||
            loopHeadIntConstantBounds.UpperBound() != srcIntConstantBounds.UpperBound()
        ))
    {LOGMEIN("GlobOpt.cpp] 18941\n");
        return false;
    }

    return true;
}

bool
GlobOpt::OptIsInvariant(
    IR::Instr *instr,
    BasicBlock *block,
    Loop *loop,
    Value *src1Val,
    Value *src2Val,
    bool isNotTypeSpecConv,
    const bool forceInvariantHoisting)
{LOGMEIN("GlobOpt.cpp] 18957\n");
    if (!loop->CanHoistInvariants())
    {LOGMEIN("GlobOpt.cpp] 18959\n");
        return false;
    }
    if (!OpCodeAttr::CanCSE(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 18963\n");
        return false;
    }

    bool allowNonPrimitives = !OpCodeAttr::OpndHasImplicitCall(instr->m_opcode);

    switch(instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 18970\n");
        // Can't legally hoist these
    case Js::OpCode::LdLen_A:
        return false;

        //Can't Hoist BailOnNotStackArgs, as it is necessary as InlineArgsOptimization relies on this opcode
        //to decide whether to throw rejit exception or not.
    case Js::OpCode::BailOnNotStackArgs:
        return false;

        // Usually not worth hoisting these
    case Js::OpCode::LdStr:
    case Js::OpCode::Ld_A:
    case Js::OpCode::Ld_I4:
    case Js::OpCode::LdC_A_I4:
        if(!forceInvariantHoisting)
        {LOGMEIN("GlobOpt.cpp] 18986\n");
            return false;
        }
        break;

        // Can't hoist these outside the function it's for. The LdArgumentsFromFrame for an inlinee depends on the inlinee meta arg
        // that holds the arguments object, which is only initialized at the start of the inlinee. So, can't hoist this outside the
        // inlinee.
    case Js::OpCode::LdArgumentsFromFrame:
        if(instr->m_func != loop->GetFunc())
        {LOGMEIN("GlobOpt.cpp] 18996\n");
            return false;
        }
        break;

    case Js::OpCode::FromVar:
        if (instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 19003\n");
            allowNonPrimitives = true;
        }
        break;
    }

    IR::Opnd *dst = instr->GetDst();

    if (dst && !dst->IsRegOpnd())
    {LOGMEIN("GlobOpt.cpp] 19012\n");
        return false;
    }

    IR::Opnd *src1 = instr->GetSrc1();

    if (src1)
    {LOGMEIN("GlobOpt.cpp] 19019\n");
        if (!this->OptIsInvariant(src1, block, loop, src1Val, isNotTypeSpecConv, allowNonPrimitives))
        {LOGMEIN("GlobOpt.cpp] 19021\n");
            return false;
        }

        IR::Opnd *src2 = instr->GetSrc2();

        if (src2)
        {LOGMEIN("GlobOpt.cpp] 19028\n");
            if (!this->OptIsInvariant(src2, block, loop, src2Val, isNotTypeSpecConv, allowNonPrimitives))
            {LOGMEIN("GlobOpt.cpp] 19030\n");
                return false;
            }
        }
    }

    return true;
}

bool
GlobOpt::OptDstIsInvariant(IR::RegOpnd *dst)
{LOGMEIN("GlobOpt.cpp] 19041\n");
    StackSym *dstSym = dst->m_sym;
    if (dstSym->IsTypeSpec())
    {LOGMEIN("GlobOpt.cpp] 19044\n");
        // The type-specialized sym may be single def, but not the original...
        dstSym = dstSym->GetVarEquivSym(this->func);
    }

    return (dstSym->m_isSingleDef);
}

void
GlobOpt::OptHoistToLandingPadUpdateValueType(
    BasicBlock* landingPad,
    IR::Instr* instr,
    IR::Opnd* opnd,
    Value* opndVal)
{LOGMEIN("GlobOpt.cpp] 19058\n");
    if (instr->m_opcode == Js::OpCode::FromVar)
    {LOGMEIN("GlobOpt.cpp] 19060\n");
        return;
    }

    Sym* opndSym = opnd->GetSym();;

    if (opndSym)
    {LOGMEIN("GlobOpt.cpp] 19067\n");
        if (opndVal == nullptr)
        {LOGMEIN("GlobOpt.cpp] 19069\n");
            opndVal = FindValue(opndSym);
        }

        Value* opndValueInLandingPad = FindValue(landingPad->globOptData.symToValueMap, opndSym);
        Assert(opndVal->GetValueNumber() == opndValueInLandingPad->GetValueNumber());

        opnd->SetValueType(opndValueInLandingPad->GetValueInfo()->Type());

        if (opndSym->IsPropertySym())
        {LOGMEIN("GlobOpt.cpp] 19079\n");
            // Also fix valueInfo on objPtr
            StackSym* opndObjPtrSym = opndSym->AsPropertySym()->m_stackSym;
            Value* opndObjPtrSymValInLandingPad = FindValue(landingPad->globOptData.symToValueMap, opndObjPtrSym);
            ValueInfo* opndObjPtrSymValueInfoInLandingPad = opndObjPtrSymValInLandingPad->GetValueInfo();

            opnd->AsSymOpnd()->SetPropertyOwnerValueType(opndObjPtrSymValueInfoInLandingPad->Type());
        }
    }
}

void
GlobOpt::OptHoistInvariant(
    IR::Instr *instr,
    BasicBlock *block,
    Loop *loop,
    Value *dstVal,
    Value *const src1Val,
    bool isNotTypeSpecConv,
    bool lossy,
    IR::BailOutKind bailoutKind)
{LOGMEIN("GlobOpt.cpp] 19100\n");
    BasicBlock *landingPad = loop->landingPad;

    IR::Opnd* src1 = instr->GetSrc1();
    if (src1)
    {
        // We are hoisting this instruction possibly past other uses, which might invalidate the last use info. Clear it.
        OptHoistToLandingPadUpdateValueType(landingPad, instr, src1, src1Val);

        if (src1->IsRegOpnd())
        {LOGMEIN("GlobOpt.cpp] 19110\n");
            src1->AsRegOpnd()->m_isTempLastUse = false;
        }

        IR::Opnd* src2 = instr->GetSrc2();
        if (src2)
        {
            OptHoistToLandingPadUpdateValueType(landingPad, instr, src2, nullptr);

            if (src2->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 19120\n");
                src2->AsRegOpnd()->m_isTempLastUse = false;
            }
        }
    }

    IR::RegOpnd *dst = instr->GetDst() ? instr->GetDst()->AsRegOpnd() : nullptr;
    if(dst)
    {LOGMEIN("GlobOpt.cpp] 19128\n");
        switch (instr->m_opcode)
        {LOGMEIN("GlobOpt.cpp] 19130\n");
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
            // These operations are a special case. They generate a lossy int value, and the var sym is initialized using
            // Conv_Bool. A sym cannot be live only as a lossy int sym, the var needs to be live as well since the lossy int
            // sym cannot be used to convert to var. We don't know however, whether the Conv_Bool will be hoisted. The idea
            // currently is that the sym is only used on the path in which it is initialized inside the loop. So, don't
            // hoist any liveness info for the dst.
            if (!this->GetIsAsmJSFunc())
            {LOGMEIN("GlobOpt.cpp] 19147\n");
                lossy = true;
            }
            break;

        case Js::OpCode::FromVar:
        {LOGMEIN("GlobOpt.cpp] 19153\n");
            StackSym* src1StackSym = IR::RegOpnd::TryGetStackSym(instr->GetSrc1());

            if (instr->HasBailOutInfo())
            {LOGMEIN("GlobOpt.cpp] 19157\n");
                IR::BailOutKind instrBailoutKind = instr->GetBailOutKind();
                Assert(instrBailoutKind == IR::BailOutIntOnly ||
                    instrBailoutKind == IR::BailOutExpectingInteger ||
                    instrBailoutKind == IR::BailOutOnNotPrimitive ||
                    instrBailoutKind == IR::BailOutNumberOnly ||
                    instrBailoutKind == IR::BailOutPrimitiveButString ||
                    instrBailoutKind == IR::BailOutSimd128F4Only ||
                    instrBailoutKind == IR::BailOutSimd128I4Only);
            }
            else if (src1StackSym && bailoutKind != IR::BailOutInvalid)
            {LOGMEIN("GlobOpt.cpp] 19168\n");
                // We may be hoisting FromVar from a region where it didn't need a bailout (src1 had a definite value type) to a region
                // where it would. In such cases, the FromVar needs a bailout based on the value type of src1 in its new position.
                Assert(!src1StackSym->IsTypeSpec());
                Value* landingPadSrc1val = FindValue(landingPad->globOptData.symToValueMap, src1StackSym);
                Assert(src1Val->GetValueNumber() == landingPadSrc1val->GetValueNumber());

                ValueInfo *src1ValueInfo = src1Val->GetValueInfo();
                ValueInfo *landingPadSrc1ValueInfo = landingPadSrc1val->GetValueInfo();
                IRType dstType = dst->GetType();

                const auto AddBailOutToFromVar = [&]()
                {
                    instr->GetSrc1()->SetValueType(landingPadSrc1val->GetValueInfo()->Type());
                    EnsureBailTarget(loop);
                    if (block->IsLandingPad())
                    {LOGMEIN("GlobOpt.cpp] 19184\n");
                        instr = instr->ConvertToBailOutInstr(instr, bailoutKind, loop->bailOutInfo->bailOutOffset);
                    }
                    else
                    {
                        instr = instr->ConvertToBailOutInstr(instr, bailoutKind);
                    }
                };

                // A definite type in the source position and not a definite type in the destination (landing pad)
                // and no bailout on the instruction; we should put a bailout on the hoisted instruction.
                if (dstType == TyInt32)
                {LOGMEIN("GlobOpt.cpp] 19196\n");
                    if (lossy)
                    {LOGMEIN("GlobOpt.cpp] 19198\n");
                        if ((src1ValueInfo->IsPrimitive() || IsTypeSpecialized(src1StackSym, block)) &&                // didn't need a lossy type spec bailout in the source block
                            (!landingPadSrc1ValueInfo->IsPrimitive() && !IsTypeSpecialized(src1StackSym, landingPad))) // needs a lossy type spec bailout in the landing pad
                        {LOGMEIN("GlobOpt.cpp] 19201\n");
                            bailoutKind = IR::BailOutOnNotPrimitive;
                            AddBailOutToFromVar();
                        }
                    }
                    else if (src1ValueInfo->IsInt() && !landingPadSrc1ValueInfo->IsInt())
                    {LOGMEIN("GlobOpt.cpp] 19207\n");
                        AddBailOutToFromVar();
                    }
                }
                else if ((dstType == TyFloat64 && src1ValueInfo->IsNumber() && !landingPadSrc1ValueInfo->IsNumber()) ||
                    (IRType_IsSimd128(dstType) && src1ValueInfo->IsSimd128() && !landingPadSrc1ValueInfo->IsSimd128()))
                {LOGMEIN("GlobOpt.cpp] 19213\n");
                    AddBailOutToFromVar();
                }
            }

            break;
        }
        }

        if (dstVal == NULL)
        {LOGMEIN("GlobOpt.cpp] 19223\n");
            dstVal = this->NewGenericValue(ValueType::Uninitialized, dst);
        }

        // ToVar/FromVar don't need a new dst because it has to be invariant if their src is invariant.
        bool dstDoesntNeedLoad = (!isNotTypeSpecConv && instr->m_opcode != Js::OpCode::LdC_A_I4);

        StackSym *varSym = dst->m_sym;

        if (varSym->IsTypeSpec())
        {LOGMEIN("GlobOpt.cpp] 19233\n");
            varSym = varSym->GetVarEquivSym(this->func);
        }

        Value *const landingPadDstVal = FindValue(loop->landingPad->globOptData.symToValueMap, varSym);
        if(landingPadDstVal
                ? dstVal->GetValueNumber() != landingPadDstVal->GetValueNumber()
                : loop->symsDefInLoop->Test(varSym->m_id))
        {LOGMEIN("GlobOpt.cpp] 19241\n");
            // We need a temp for FromVar/ToVar if dst changes in the loop.
            dstDoesntNeedLoad = false;
        }

        if (!dstDoesntNeedLoad && this->OptDstIsInvariant(dst) == false)
        {LOGMEIN("GlobOpt.cpp] 19247\n");
            // Keep dst in place, hoist instr using a new dst.
            instr->UnlinkDst();

            // Set type specialization info correctly for this new sym
            StackSym *copyVarSym;
            IR::RegOpnd *copyReg;
            if (dst->m_sym->IsTypeSpec())
            {LOGMEIN("GlobOpt.cpp] 19255\n");
                copyVarSym = StackSym::New(TyVar, instr->m_func);
                StackSym *copySym = copyVarSym;
                if (dst->m_sym->IsInt32())
                {LOGMEIN("GlobOpt.cpp] 19259\n");
                    if(lossy)
                    {LOGMEIN("GlobOpt.cpp] 19261\n");
                        // The new sym would only be live as a lossy int since we're only hoisting the store to the int version
                        // of the sym, and cannot be converted to var. It is not legal to have a sym only live as a lossy int,
                        // so don't update liveness info for this sym.
                    }
                    else
                    {
                        block->globOptData.liveInt32Syms->Set(copyVarSym->m_id);
                    }
                    copySym = copySym->GetInt32EquivSym(instr->m_func);
                }
                else if (dst->m_sym->IsFloat64())
                {LOGMEIN("GlobOpt.cpp] 19273\n");
                    block->globOptData.liveFloat64Syms->Set(copyVarSym->m_id);
                    copySym = copySym->GetFloat64EquivSym(instr->m_func);
                }
                else if (dst->IsSimd128())
                {LOGMEIN("GlobOpt.cpp] 19278\n");
                    // SIMD_JS
                    if (dst->IsSimd128F4())
                    {LOGMEIN("GlobOpt.cpp] 19281\n");
                        block->globOptData.liveSimd128F4Syms->Set(copyVarSym->m_id);
                        copySym = copySym->GetSimd128F4EquivSym(instr->m_func);
                    }
                    else
                    {
                        Assert(dst->IsSimd128I4());
                        block->globOptData.liveSimd128I4Syms->Set(copyVarSym->m_id);
                        copySym = copySym->GetSimd128I4EquivSym(instr->m_func);
                    }

                }
                copyReg = IR::RegOpnd::New(copySym, copySym->GetType(), instr->m_func);
            }
            else
            {
                copyReg = IR::RegOpnd::New(dst->GetType(), instr->m_func);
                copyVarSym = copyReg->m_sym;
                block->globOptData.liveVarSyms->Set(copyVarSym->m_id);
            }

            copyReg->SetValueType(dst->GetValueType());
            IR::Instr *copyInstr = IR::Instr::New(Js::OpCode::Ld_A, dst, copyReg, instr->m_func);
            copyInstr->SetByteCodeOffset(instr);
            instr->SetDst(copyReg);
            instr->InsertBefore(copyInstr);

            dst->m_sym->m_mayNotBeTempLastUse = true;

            if (instr->GetSrc1() && instr->GetSrc1()->IsImmediateOpnd())
            {LOGMEIN("GlobOpt.cpp] 19311\n");
                // Propagate IsIntConst if appropriate
                switch(instr->m_opcode)
                {LOGMEIN("GlobOpt.cpp] 19314\n");
                case Js::OpCode::Ld_A:
                case Js::OpCode::Ld_I4:
                case Js::OpCode::LdC_A_I4:
                    copyReg->m_sym->SetIsConst();
                    break;
                }
            }

            ValueInfo *dstValueInfo = dstVal->GetValueInfo();
            if((!dstValueInfo->GetSymStore() || dstValueInfo->GetSymStore() == varSym) && !lossy)
            {LOGMEIN("GlobOpt.cpp] 19325\n");
                // The destination's value may have been transferred from one of the invariant sources, in which case we should
                // keep the sym store intact, as that sym will likely have a better lifetime than this new copy sym. For
                // instance, if we're inside a conditioned block, because we don't make the copy sym live and set its value in
                // all preceding blocks, this sym would not be live after exiting this block, causing this value to not
                // participate in copy-prop after this block.
                this->SetSymStoreDirect(dstValueInfo, copyVarSym);
            }

            this->InsertNewValue(&block->globOptData, dstVal, copyReg);
            dst = copyReg;
        }
    }

    // Move to landing pad
    block->UnlinkInstr(instr);

    if (loop->bailOutInfo->bailOutInstr)
    {LOGMEIN("GlobOpt.cpp] 19343\n");
        loop->bailOutInfo->bailOutInstr->InsertBefore(instr);
    }
    else
    {
        landingPad->InsertAfter(instr);
    }

    GlobOpt::MarkNonByteCodeUsed(instr);

    if (instr->HasBailOutInfo() || instr->HasAuxBailOut())
    {LOGMEIN("GlobOpt.cpp] 19354\n");
        Assert(loop->bailOutInfo);
        EnsureBailTarget(loop);

        // Copy bailout info of loop top.
        if (instr->ReplaceBailOutInfo(loop->bailOutInfo))
        {LOGMEIN("GlobOpt.cpp] 19360\n");
            // if the old bailout is deleted, reset capturedvalues cached in block
            block->globOptData.capturedValues = nullptr;
            block->globOptData.capturedValuesCandidate = nullptr;
        }
    }

    if(!dst)
    {LOGMEIN("GlobOpt.cpp] 19368\n");
        return;
    }

    // The bailout info's liveness for the dst sym is not updated in loop landing pads because bailout instructions previously
    // hoisted into the loop's landing pad may bail out before the current type of the dst sym became live (perhaps due to this
    // instruction). Since the landing pad will have a shared bailout point, the bailout info cannot assume that the current
    // type of the dst sym was live during every bailout hoisted into the landing pad.

    StackSym *const dstSym = dst->m_sym;
    StackSym *const dstVarSym = dstSym->IsTypeSpec() ? dstSym->GetVarEquivSym(nullptr) : dstSym;
    Assert(dstVarSym);
    if(isNotTypeSpecConv || !IsLive(dstVarSym, loop->landingPad))
    {LOGMEIN("GlobOpt.cpp] 19381\n");
        // A new dst is being hoisted, or the same single-def dst that would not be live before this block. So, make it live and
        // update the value info with the same value info in this block.

        if(lossy)
        {LOGMEIN("GlobOpt.cpp] 19386\n");
            // This is a lossy conversion to int. The instruction was given a new dst specifically for hoisting, so this new dst
            // will not be live as a var before this block. A sym cannot be live only as a lossy int sym, the var needs to be
            // live as well since the lossy int sym cannot be used to convert to var. Since the var version of the sym is not
            // going to be initialized, don't hoist any liveness info for the dst. The sym is only going to be used on the path
            // in which it is initialized inside the loop.
            Assert(dstSym->IsTypeSpec());
            Assert(dstSym->IsInt32());
            return;
        }

        // Check if the dst value was transferred from the src. If so, the value transfer needs to be replicated.
        bool isTransfer = dstVal == src1Val;

        StackSym *transferValueOfSym = nullptr;
        if(isTransfer)
        {LOGMEIN("GlobOpt.cpp] 19402\n");
            Assert(instr->GetSrc1());
            if(instr->GetSrc1()->IsRegOpnd())
            {LOGMEIN("GlobOpt.cpp] 19405\n");
                StackSym *src1Sym = instr->GetSrc1()->AsRegOpnd()->m_sym;
                if(src1Sym->IsTypeSpec())
                {LOGMEIN("GlobOpt.cpp] 19408\n");
                    src1Sym = src1Sym->GetVarEquivSym(nullptr);
                    Assert(src1Sym);
                }
                if(dstVal == FindValue(block->globOptData.symToValueMap, src1Sym))
                {LOGMEIN("GlobOpt.cpp] 19413\n");
                    transferValueOfSym = src1Sym;
                }
            }
        }

        // SIMD_JS
        if (instr->m_opcode == Js::OpCode::ExtendArg_A)
        {LOGMEIN("GlobOpt.cpp] 19421\n");
            // Check if we should have CSE'ed this EA
            Assert(instr->GetSrc1());

            // If the dstVal symstore is not the dst itself, then we copied the Value from another expression.
            if (dstVal->GetValueInfo()->GetSymStore() != instr->GetDst()->GetStackSym())
            {LOGMEIN("GlobOpt.cpp] 19427\n");
                isTransfer = true;
                transferValueOfSym = dstVal->GetValueInfo()->GetSymStore()->AsStackSym();
            }
        }

        const ValueNumber dstValueNumber = dstVal->GetValueNumber();
        ValueNumber dstNewValueNumber = InvalidValueNumber;
        for(InvariantBlockBackwardIterator it(this, block, loop->landingPad, nullptr); it.IsValid(); it.MoveNext())
        {LOGMEIN("GlobOpt.cpp] 19436\n");
            BasicBlock *const hoistBlock = it.Block();
            GlobOptBlockData &hoistBlockData = hoistBlock->globOptData;

            Assert(!IsLive(dstVarSym, &hoistBlockData));
            MakeLive(dstSym, &hoistBlockData, lossy);

            Value *newDstValue;
            do
            {LOGMEIN("GlobOpt.cpp] 19445\n");
                if(isTransfer)
                {LOGMEIN("GlobOpt.cpp] 19447\n");
                    if(transferValueOfSym)
                    {LOGMEIN("GlobOpt.cpp] 19449\n");
                        newDstValue = FindValue(hoistBlockData.symToValueMap, transferValueOfSym);
                        if(newDstValue && newDstValue->GetValueNumber() == dstValueNumber)
                        {LOGMEIN("GlobOpt.cpp] 19452\n");
                            break;
                        }
                    }

                    // It's a transfer, but we don't have a sym whose value number matches in the target block. Use a new value
                    // number since we don't know if there is already a value with the current number for the target block.
                    if(dstNewValueNumber == InvalidValueNumber)
                    {LOGMEIN("GlobOpt.cpp] 19460\n");
                        dstNewValueNumber = NewValueNumber();
                    }
                    newDstValue = CopyValue(dstVal, dstNewValueNumber);
                    break;
                }

                newDstValue = CopyValue(dstVal, dstValueNumber);
            } while(false);

            SetValue(&hoistBlockData, newDstValue, dstVarSym);
        }
        return;
    }

#if DBG
    if(instr->GetSrc1()->IsRegOpnd()) // Type spec conversion may load a constant into a dst sym
    {LOGMEIN("GlobOpt.cpp] 19477\n");
        StackSym *const srcSym = instr->GetSrc1()->AsRegOpnd()->m_sym;
        Assert(srcSym != dstSym); // Type spec conversion must be changing the type, so the syms must be different
        StackSym *const srcVarSym = srcSym->IsTypeSpec() ? srcSym->GetVarEquivSym(nullptr) : srcSym;
        Assert(srcVarSym == dstVarSym); // Type spec conversion must be between variants of the same var sym
    }
#endif

    bool changeValueType = false, changeValueTypeToInt = false;
    if(dstSym->IsTypeSpec())
    {LOGMEIN("GlobOpt.cpp] 19487\n");
        if(dst->IsInt32())
        {LOGMEIN("GlobOpt.cpp] 19489\n");
            if(!lossy)
            {LOGMEIN("GlobOpt.cpp] 19491\n");
                Assert(
                    !instr->HasBailOutInfo() ||
                    instr->GetBailOutKind() == IR::BailOutIntOnly ||
                    instr->GetBailOutKind() == IR::BailOutExpectingInteger);
                changeValueType = changeValueTypeToInt = true;
            }
        }
        else if (dst->IsFloat64())
        {LOGMEIN("GlobOpt.cpp] 19500\n");
            if(instr->HasBailOutInfo() && instr->GetBailOutKind() == IR::BailOutNumberOnly)
            {LOGMEIN("GlobOpt.cpp] 19502\n");
                changeValueType = true;
            }
        }
        else
        {
            // SIMD_JS
            Assert(dst->IsSimd128());
            if (instr->HasBailOutInfo() &&
                (instr->GetBailOutKind() == IR::BailOutSimd128F4Only || instr->GetBailOutKind() == IR::BailOutSimd128I4Only))
            {LOGMEIN("GlobOpt.cpp] 19512\n");
                changeValueType = true;
            }
        }
    }

    ValueInfo *previousValueInfoBeforeUpdate = nullptr, *previousValueInfoAfterUpdate = nullptr;
    for(InvariantBlockBackwardIterator it(
            this,
            block,
            loop->landingPad,
            dstVarSym,
            dstVal->GetValueNumber());
        it.IsValid();
        it.MoveNext())
    {LOGMEIN("GlobOpt.cpp] 19527\n");
        BasicBlock *const hoistBlock = it.Block();
        GlobOptBlockData &hoistBlockData = hoistBlock->globOptData;

    #if DBG
        // TODO: There are some odd cases with field hoisting where the sym is invariant in only part of the loop and the info
        // does not flow through all blocks. Un-comment the verification below after PRE replaces field hoisting.

        //// Verify that the src sym is live as the required type, and that the conversion is valid
        //Assert(IsLive(dstVarSym, &hoistBlockData));
        //if(instr->GetSrc1()->IsRegOpnd())
        //{
        //    IR::RegOpnd *const src = instr->GetSrc1()->AsRegOpnd();
        //    StackSym *const srcSym = instr->GetSrc1()->AsRegOpnd()->m_sym;
        //    if(srcSym->IsTypeSpec())
        //    {
        //        if(src->IsInt32())
        //        {
        //            Assert(hoistBlockData.liveInt32Syms->Test(dstVarSym->m_id));
        //            Assert(!hoistBlockData.liveLossyInt32Syms->Test(dstVarSym->m_id)); // shouldn't try to convert a lossy int32 to anything
        //        }
        //        else
        //        {
        //            Assert(src->IsFloat64());
        //            Assert(hoistBlockData.liveFloat64Syms->Test(dstVarSym->m_id));
        //            if(dstSym->IsTypeSpec() && dst->IsInt32())
        //            {
        //                Assert(lossy); // shouldn't try to do a lossless conversion from float64 to int32
        //            }
        //        }
        //    }
        //    else
        //    {
        //        Assert(hoistBlockData.liveVarSyms->Test(dstVarSym->m_id));
        //    }
        //}
        //if(dstSym->IsTypeSpec() && dst->IsInt32())
        //{
        //    // If the sym is already specialized as required in the block to which we are attempting to hoist the conversion,
        //    // that info should have flowed into this block
        //    if(lossy)
        //    {
        //        Assert(!hoistBlockData.liveInt32Syms->Test(dstVarSym->m_id));
        //    }
        //    else
        //    {
        //        Assert(!IsInt32TypeSpecialized(dstVarSym, hoistBlock));
        //    }
        //}
    #endif

        MakeLive(dstSym, &hoistBlockData, lossy);

        if(!changeValueType)
        {LOGMEIN("GlobOpt.cpp] 19581\n");
            continue;
        }

        Value *const hoistBlockValue = it.InvariantSymValue();
        ValueInfo *const hoistBlockValueInfo = hoistBlockValue->GetValueInfo();
        if(hoistBlockValueInfo == previousValueInfoBeforeUpdate)
        {LOGMEIN("GlobOpt.cpp] 19588\n");
            if(hoistBlockValueInfo != previousValueInfoAfterUpdate)
            {
                HoistInvariantValueInfo(previousValueInfoAfterUpdate, hoistBlockValue, hoistBlock);
            }
        }
        else
        {
            previousValueInfoBeforeUpdate = hoistBlockValueInfo;
            ValueInfo *const newValueInfo =
                changeValueTypeToInt
                    ? hoistBlockValueInfo->SpecializeToInt32(alloc)
                    : hoistBlockValueInfo->SpecializeToFloat64(alloc);
            previousValueInfoAfterUpdate = newValueInfo;
            ChangeValueInfo(changeValueTypeToInt ? nullptr : hoistBlock, hoistBlockValue, newValueInfo);
        }
    }
}

bool
GlobOpt::TryHoistInvariant(
    IR::Instr *instr,
    BasicBlock *block,
    Value *dstVal,
    Value *src1Val,
    Value *src2Val,
    bool isNotTypeSpecConv,
    const bool lossy,
    const bool forceInvariantHoisting,
    IR::BailOutKind bailoutKind)
{LOGMEIN("GlobOpt.cpp] 19618\n");
    Assert(!this->IsLoopPrePass());

    if (OptIsInvariant(instr, block, block->loop, src1Val, src2Val, isNotTypeSpecConv, forceInvariantHoisting))
    {LOGMEIN("GlobOpt.cpp] 19622\n");
#if DBG
        if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::InvariantsPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId()))
        {LOGMEIN("GlobOpt.cpp] 19625\n");
            Output::Print(_u(" **** INVARIANT  ***   "));
            instr->Dump();
        }
#endif
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::InvariantsPhase))
        {LOGMEIN("GlobOpt.cpp] 19632\n");
            Output::Print(_u(" **** INVARIANT  ***   "));
            Output::Print(_u("%s \n"), Js::OpCodeUtil::GetOpCodeName(instr->m_opcode));
        }
#endif
        Loop *loop = block->loop;

        // Try hoisting from to outer most loop
        while (loop->parent && OptIsInvariant(instr, block, loop->parent, src1Val, src2Val, isNotTypeSpecConv, forceInvariantHoisting))
        {LOGMEIN("GlobOpt.cpp] 19641\n");
            loop = loop->parent;
        }

        // Record the byte code use here since we are going to move this instruction up
        if (isNotTypeSpecConv)
        {LOGMEIN("GlobOpt.cpp] 19647\n");
            InsertNoImplicitCallUses(instr);
            this->CaptureByteCodeSymUses(instr);
            this->InsertByteCodeUses(instr, true);
        }
#if DBG
        else
        {
            PropertySym *propertySymUse = NULL;
            NoRecoverMemoryJitArenaAllocator tempAllocator(_u("BE-GlobOpt-Temp"), this->alloc->GetPageAllocator(), Js::Throw::OutOfMemory);
            BVSparse<JitArenaAllocator> * tempByteCodeUse = JitAnew(&tempAllocator, BVSparse<JitArenaAllocator>, &tempAllocator);
            GlobOpt::TrackByteCodeSymUsed(instr, tempByteCodeUse, &propertySymUse);
            Assert(tempByteCodeUse->Count() == 0 && propertySymUse == NULL);
        }
#endif
        OptHoistInvariant(instr, block, loop, dstVal, src1Val, isNotTypeSpecConv, lossy, bailoutKind);
        return true;
    }

    return false;
}

InvariantBlockBackwardIterator::InvariantBlockBackwardIterator(
    GlobOpt *const globOpt,
    BasicBlock *const exclusiveBeginBlock,
    BasicBlock *const inclusiveEndBlock,
    StackSym *const invariantSym,
    const ValueNumber invariantSymValueNumber)
    : globOpt(globOpt),
    exclusiveEndBlock(inclusiveEndBlock->prev),
    invariantSym(invariantSym),
    invariantSymValueNumber(invariantSymValueNumber),
    block(exclusiveBeginBlock)
#if DBG
    ,
    inclusiveEndBlock(inclusiveEndBlock)
#endif
{
    Assert(exclusiveBeginBlock);
    Assert(inclusiveEndBlock);
    Assert(!inclusiveEndBlock->isDeleted);
    Assert(exclusiveBeginBlock != inclusiveEndBlock);
    Assert(!invariantSym == (invariantSymValueNumber == InvalidValueNumber));

    MoveNext();
}

bool
InvariantBlockBackwardIterator::IsValid() const
{LOGMEIN("GlobOpt.cpp] 19696\n");
    return block != exclusiveEndBlock;
}

void
InvariantBlockBackwardIterator::MoveNext()
{LOGMEIN("GlobOpt.cpp] 19702\n");
    Assert(IsValid());

    while(true)
    {LOGMEIN("GlobOpt.cpp] 19706\n");
    #if DBG
        BasicBlock *const previouslyIteratedBlock = block;
    #endif
        block = block->prev;
        if(!IsValid())
        {LOGMEIN("GlobOpt.cpp] 19712\n");
            Assert(previouslyIteratedBlock == inclusiveEndBlock);
            break;
        }

        if(block->isDeleted)
        {LOGMEIN("GlobOpt.cpp] 19718\n");
            continue;
        }

        if(!block->globOptData.HasData())
        {LOGMEIN("GlobOpt.cpp] 19723\n");
            // This block's info has already been merged with all of its successors
            continue;
        }

        if(!invariantSym)
        {LOGMEIN("GlobOpt.cpp] 19729\n");
            break;
        }

        invariantSymValue = globOpt->FindValue(block->globOptData.symToValueMap, invariantSym);
        if(!invariantSymValue || invariantSymValue->GetValueNumber() != invariantSymValueNumber)
        {LOGMEIN("GlobOpt.cpp] 19735\n");
            // BailOnNoProfile and throw blocks are not moved outside loops. A sym table cleanup on these paths may delete the
            // values. Field hoisting also has some odd cases where the hoisted stack sym is invariant in only part of the loop.
            continue;
        }
        break;
    }
}

BasicBlock *
InvariantBlockBackwardIterator::Block() const
{LOGMEIN("GlobOpt.cpp] 19746\n");
    Assert(IsValid());
    return block;
}

Value *
InvariantBlockBackwardIterator::InvariantSymValue() const
{LOGMEIN("GlobOpt.cpp] 19753\n");
    Assert(IsValid());
    Assert(invariantSym);

    return invariantSymValue;
}

void
GlobOpt::HoistInvariantValueInfo(
    ValueInfo *const invariantValueInfoToHoist,
    Value *const valueToUpdate,
    BasicBlock *const targetBlock)
{LOGMEIN("GlobOpt.cpp] 19765\n");
    Assert(invariantValueInfoToHoist);
    Assert(valueToUpdate);
    Assert(targetBlock);

    // Why are we trying to change the value type of the type sym value? Asserting here to make sure we don't deep copy the type sym's value info.
    Assert(!invariantValueInfoToHoist->IsJsType());

    Sym *const symStore = valueToUpdate->GetValueInfo()->GetSymStore();
    ValueInfo *newValueInfo;
    if(invariantValueInfoToHoist->GetSymStore() == symStore)
    {LOGMEIN("GlobOpt.cpp] 19776\n");
        newValueInfo = invariantValueInfoToHoist;
    }
    else
    {
        newValueInfo = invariantValueInfoToHoist->Copy(alloc);
        this->SetSymStoreDirect(newValueInfo, symStore);
    }
    ChangeValueInfo(targetBlock, valueToUpdate, newValueInfo);
}

// static
bool
GlobOpt::DoInlineArgsOpt(Func* func)
{LOGMEIN("GlobOpt.cpp] 19790\n");
    Func* topFunc = func->GetTopFunc();
    Assert(topFunc != func);
    bool doInlineArgsOpt =
        !PHASE_OFF(Js::InlineArgsOptPhase, topFunc) &&
        !func->GetHasCalls() &&
        !func->GetHasUnoptimizedArgumentsAcccess() &&
        func->m_canDoInlineArgsOpt;
    return doInlineArgsOpt;
}

bool
GlobOpt::IsSwitchOptEnabled(Func* func)
{LOGMEIN("GlobOpt.cpp] 19803\n");
    Assert(func->IsTopFunc());
    return !PHASE_OFF(Js::SwitchOptPhase, func) && (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsSwitchOptDisabled()) && !IsTypeSpecPhaseOff(func)
        && func->DoGlobOpt() && !func->HasTry();
}

bool
GlobOpt::DoConstFold() const
{LOGMEIN("GlobOpt.cpp] 19811\n");
    return !PHASE_OFF(Js::ConstFoldPhase, func);
}

bool
GlobOpt::IsTypeSpecPhaseOff(Func *func)
{LOGMEIN("GlobOpt.cpp] 19817\n");
    return PHASE_OFF(Js::TypeSpecPhase, func) || func->IsJitInDebugMode() || !func->DoGlobOptsForGeneratorFunc();
}

bool
GlobOpt::DoTypeSpec() const
{LOGMEIN("GlobOpt.cpp] 19823\n");
    return doTypeSpec;
}

bool
GlobOpt::DoAggressiveIntTypeSpec(Func* func)
{LOGMEIN("GlobOpt.cpp] 19829\n");
    return
        !PHASE_OFF(Js::AggressiveIntTypeSpecPhase, func) &&
        !IsTypeSpecPhaseOff(func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsAggressiveIntTypeSpecDisabled(func->IsLoopBody()));
}

bool
GlobOpt::DoAggressiveIntTypeSpec() const
{LOGMEIN("GlobOpt.cpp] 19838\n");
    return doAggressiveIntTypeSpec;
}

bool
GlobOpt::DoAggressiveMulIntTypeSpec() const
{LOGMEIN("GlobOpt.cpp] 19844\n");
    return doAggressiveMulIntTypeSpec;
}

bool
GlobOpt::DoDivIntTypeSpec() const
{LOGMEIN("GlobOpt.cpp] 19850\n");
    return doDivIntTypeSpec;
}

// static
bool
GlobOpt::DoLossyIntTypeSpec(Func* func)
{LOGMEIN("GlobOpt.cpp] 19857\n");
    return
        !PHASE_OFF(Js::LossyIntTypeSpecPhase, func) &&
        !IsTypeSpecPhaseOff(func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsLossyIntTypeSpecDisabled());
}

bool
GlobOpt::DoLossyIntTypeSpec() const
{LOGMEIN("GlobOpt.cpp] 19866\n");
    return doLossyIntTypeSpec;
}

// static
bool
GlobOpt::DoFloatTypeSpec(Func* func)
{LOGMEIN("GlobOpt.cpp] 19873\n");
    return
        !PHASE_OFF(Js::FloatTypeSpecPhase, func) &&
        !IsTypeSpecPhaseOff(func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsFloatTypeSpecDisabled()) &&
        AutoSystemInfo::Data.SSE2Available();
}

bool
GlobOpt::DoFloatTypeSpec() const
{LOGMEIN("GlobOpt.cpp] 19883\n");
    return doFloatTypeSpec;
}

bool
GlobOpt::DoStringTypeSpec(Func* func)
{LOGMEIN("GlobOpt.cpp] 19889\n");
    return !PHASE_OFF(Js::StringTypeSpecPhase, func) && !IsTypeSpecPhaseOff(func);
}

// static
bool
GlobOpt::DoTypedArrayTypeSpec(Func* func)
{LOGMEIN("GlobOpt.cpp] 19896\n");
    return !PHASE_OFF(Js::TypedArrayTypeSpecPhase, func) &&
        !IsTypeSpecPhaseOff(func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsTypedArrayTypeSpecDisabled(func->IsLoopBody()))
#if defined(_M_IX86)
        && AutoSystemInfo::Data.SSE2Available()
#endif
        ;
}

// static
bool
GlobOpt::DoNativeArrayTypeSpec(Func* func)
{LOGMEIN("GlobOpt.cpp] 19909\n");
    return !PHASE_OFF(Js::NativeArrayPhase, func) &&
        !IsTypeSpecPhaseOff(func)
#if defined(_M_IX86)
        && AutoSystemInfo::Data.SSE2Available()
#endif
        ;
}

bool
GlobOpt::DoArrayCheckHoist(Func *const func)
{LOGMEIN("GlobOpt.cpp] 19920\n");
    Assert(func->IsTopFunc());
    return
        !PHASE_OFF(Js::ArrayCheckHoistPhase, func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsArrayCheckHoistDisabled(func->IsLoopBody())) &&
        !func->IsJitInDebugMode() && // StElemI fast path is not allowed when in debug mode, so it cannot have bailout
        func->DoGlobOptsForGeneratorFunc();
}

bool
GlobOpt::DoArrayCheckHoist() const
{LOGMEIN("GlobOpt.cpp] 19931\n");
    return doArrayCheckHoist;
}

bool
GlobOpt::DoArrayCheckHoist(const ValueType baseValueType, Loop* loop, IR::Instr *const instr) const
{LOGMEIN("GlobOpt.cpp] 19937\n");
    if(!DoArrayCheckHoist() || (instr && !IsLoopPrePass() && instr->DoStackArgsOpt(func)))
    {LOGMEIN("GlobOpt.cpp] 19939\n");
        return false;
    }

    if(!baseValueType.IsLikelyArrayOrObjectWithArray() ||
        (loop ? ImplicitCallFlagsAllowOpts(loop) : ImplicitCallFlagsAllowOpts(func)))
    {LOGMEIN("GlobOpt.cpp] 19945\n");
        return true;
    }

    // The function or loop does not allow disabling implicit calls, which is required to eliminate redundant JS array checks
#if DBG_DUMP
    if((((loop ? loop->GetImplicitCallFlags() : func->m_fg->implicitCallFlags) & ~Js::ImplicitCall_External) == 0) &&
        Js::Configuration::Global.flags.Trace.IsEnabled(Js::HostOptPhase))
    {LOGMEIN("GlobOpt.cpp] 19953\n");
        Output::Print(_u("DoArrayCheckHoist disabled for JS arrays because of external: "));
        func->DumpFullFunctionName();
        Output::Print(_u("\n"));
        Output::Flush();
    }
#endif
    return false;
}

bool
GlobOpt::DoArrayMissingValueCheckHoist(Func *const func)
{LOGMEIN("GlobOpt.cpp] 19965\n");
    return
        DoArrayCheckHoist(func) &&
        !PHASE_OFF(Js::ArrayMissingValueCheckHoistPhase, func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsArrayMissingValueCheckHoistDisabled(func->IsLoopBody()));
}

bool
GlobOpt::DoArrayMissingValueCheckHoist() const
{LOGMEIN("GlobOpt.cpp] 19974\n");
    return doArrayMissingValueCheckHoist;
}

bool
GlobOpt::DoArraySegmentHoist(const ValueType baseValueType, Func *const func)
{LOGMEIN("GlobOpt.cpp] 19980\n");
    Assert(baseValueType.IsLikelyAnyOptimizedArray());

    if(!DoArrayCheckHoist(func) || PHASE_OFF(Js::ArraySegmentHoistPhase, func))
    {LOGMEIN("GlobOpt.cpp] 19984\n");
        return false;
    }

    if(!baseValueType.IsLikelyArrayOrObjectWithArray())
    {LOGMEIN("GlobOpt.cpp] 19989\n");
        return true;
    }

    return
        !PHASE_OFF(Js::JsArraySegmentHoistPhase, func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsJsArraySegmentHoistDisabled(func->IsLoopBody()));
}

bool
GlobOpt::DoArraySegmentHoist(const ValueType baseValueType) const
{LOGMEIN("GlobOpt.cpp] 20000\n");
    Assert(baseValueType.IsLikelyAnyOptimizedArray());
    return baseValueType.IsLikelyArrayOrObjectWithArray() ? doJsArraySegmentHoist : doArraySegmentHoist;
}

bool
GlobOpt::DoTypedArraySegmentLengthHoist(Loop *const loop) const
{LOGMEIN("GlobOpt.cpp] 20007\n");
    if(!DoArraySegmentHoist(ValueType::GetObject(ObjectType::Int32Array)))
    {LOGMEIN("GlobOpt.cpp] 20009\n");
        return false;
    }

    if(loop ? ImplicitCallFlagsAllowOpts(loop) : ImplicitCallFlagsAllowOpts(func))
    {LOGMEIN("GlobOpt.cpp] 20014\n");
        return true;
    }

    // The function or loop does not allow disabling implicit calls, which is required to eliminate redundant typed array
    // segment length loads.
#if DBG_DUMP
    if((((loop ? loop->GetImplicitCallFlags() : func->m_fg->implicitCallFlags) & ~Js::ImplicitCall_External) == 0) &&
        Js::Configuration::Global.flags.Trace.IsEnabled(Js::HostOptPhase))
    {LOGMEIN("GlobOpt.cpp] 20023\n");
        Output::Print(_u("DoArraySegmentLengthHoist disabled for typed arrays because of external: "));
        func->DumpFullFunctionName();
        Output::Print(_u("\n"));
        Output::Flush();
    }
#endif
    return false;
}

bool
GlobOpt::DoArrayLengthHoist(Func *const func)
{LOGMEIN("GlobOpt.cpp] 20035\n");
    return
        DoArrayCheckHoist(func) &&
        !PHASE_OFF(Js::Phase::ArrayLengthHoistPhase, func) &&
        (!func->HasProfileInfo() || !func->GetReadOnlyProfileInfo()->IsArrayLengthHoistDisabled(func->IsLoopBody()));
}

bool
GlobOpt::DoArrayLengthHoist() const
{LOGMEIN("GlobOpt.cpp] 20044\n");
    return doArrayLengthHoist;
}

bool
GlobOpt::DoEliminateArrayAccessHelperCall() const
{LOGMEIN("GlobOpt.cpp] 20050\n");
    return doEliminateArrayAccessHelperCall;
}

bool
GlobOpt::DoLdLenIntSpec(IR::Instr *const instr, const ValueType baseValueType) const
{LOGMEIN("GlobOpt.cpp] 20056\n");
    Assert(!instr || instr->m_opcode == Js::OpCode::LdLen_A);
    Assert(!instr || instr->GetDst());
    Assert(!instr || instr->GetSrc1());

    if(PHASE_OFF(Js::LdLenIntSpecPhase, func) ||
        IsTypeSpecPhaseOff(func) ||
        (func->HasProfileInfo() && func->GetReadOnlyProfileInfo()->IsLdLenIntSpecDisabled()) ||
        (instr && !IsLoopPrePass() && instr->DoStackArgsOpt(func)))
    {LOGMEIN("GlobOpt.cpp] 20065\n");
        return false;
    }

    if(instr &&
        instr->IsProfiledInstr() &&
        (
            !instr->AsProfiledInstr()->u.ldElemInfo->GetElementType().IsLikelyInt() ||
            instr->GetDst()->AsRegOpnd()->m_sym->m_isNotInt
        ))
    {LOGMEIN("GlobOpt.cpp] 20075\n");
        return false;
    }

    Assert(!instr || baseValueType == instr->GetSrc1()->GetValueType());
    return
        baseValueType.HasBeenString() ||
        (baseValueType.IsLikelyAnyOptimizedArray() && baseValueType.GetObjectType() != ObjectType::ObjectWithArray);
}

bool
GlobOpt::DoPathDependentValues() const
{LOGMEIN("GlobOpt.cpp] 20087\n");
    return !PHASE_OFF(Js::Phase::PathDependentValuesPhase, func);
}

bool
GlobOpt::DoTrackRelativeIntBounds() const
{LOGMEIN("GlobOpt.cpp] 20093\n");
    return doTrackRelativeIntBounds;
}

bool
GlobOpt::DoBoundCheckElimination() const
{LOGMEIN("GlobOpt.cpp] 20099\n");
    return doBoundCheckElimination;
}

bool
GlobOpt::DoBoundCheckHoist() const
{LOGMEIN("GlobOpt.cpp] 20105\n");
    return doBoundCheckHoist;
}

bool
GlobOpt::DoLoopCountBasedBoundCheckHoist() const
{LOGMEIN("GlobOpt.cpp] 20111\n");
    return doLoopCountBasedBoundCheckHoist;
}

bool
GlobOpt::DoPowIntIntTypeSpec() const
{LOGMEIN("GlobOpt.cpp] 20117\n");
    return doPowIntIntTypeSpec;
}

bool
GlobOpt::DoTagChecks() const
{LOGMEIN("GlobOpt.cpp] 20123\n");
    return doTagChecks;
}

bool
GlobOpt::TrackArgumentsObject()
{LOGMEIN("GlobOpt.cpp] 20129\n");
    if (PHASE_OFF(Js::StackArgOptPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 20131\n");
        this->CannotAllocateArgumentsObjectOnStack();
        return false;
    }

    return func->GetHasStackArgs();
}

void
GlobOpt::CannotAllocateArgumentsObjectOnStack()
{LOGMEIN("GlobOpt.cpp] 20141\n");
    func->SetHasStackArgs(false);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (PHASE_TESTTRACE(Js::StackArgOptPhase, this->func))
    {LOGMEIN("GlobOpt.cpp] 20146\n");
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(_u("Stack args disabled for function %s(%s)\n"), func->GetJITFunctionBody()->GetDisplayName(), func->GetDebugNumberSet(debugStringBuffer));
        Output::Flush();
    }
#endif
}

IR::Instr *
GlobOpt::PreOptPeep(IR::Instr *instr)
{LOGMEIN("GlobOpt.cpp] 20156\n");
    if (OpCodeAttr::HasDeadFallThrough(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 20158\n");
        switch (instr->m_opcode)
        {LOGMEIN("GlobOpt.cpp] 20160\n");
            case Js::OpCode::BailOnNoProfile:
            {LOGMEIN("GlobOpt.cpp] 20162\n");
                // Handle BailOnNoProfile
                if (instr->HasBailOutInfo())
                {LOGMEIN("GlobOpt.cpp] 20165\n");
                    if (!this->prePassLoop)
                    {LOGMEIN("GlobOpt.cpp] 20167\n");
                        FillBailOutInfo(this->currentBlock, instr->GetBailOutInfo());
                    }
                    // Already processed.
                    return instr;
                }

                // Convert to bailout instr
                IR::Instr *nextBytecodeOffsetInstr = instr->GetNextRealInstrOrLabel();
                while(nextBytecodeOffsetInstr->GetByteCodeOffset() == Js::Constants::NoByteCodeOffset)
                {LOGMEIN("GlobOpt.cpp] 20177\n");
                    nextBytecodeOffsetInstr = nextBytecodeOffsetInstr->GetNextRealInstrOrLabel();
                    Assert(!nextBytecodeOffsetInstr->IsLabelInstr());
                }
                instr = instr->ConvertToBailOutInstr(nextBytecodeOffsetInstr, IR::BailOutOnNoProfile);
                instr->ClearByteCodeOffset();
                instr->SetByteCodeOffset(nextBytecodeOffsetInstr);

                if (!this->currentBlock->loop)
                {LOGMEIN("GlobOpt.cpp] 20186\n");
                    FillBailOutInfo(this->currentBlock, instr->GetBailOutInfo());
                }
                else
                {
                    Assert(this->prePassLoop);
                }
                break;
            }
            case Js::OpCode::BailOnException:
            {LOGMEIN("GlobOpt.cpp] 20196\n");
                Assert(this->func->HasTry() && this->func->DoOptimizeTryCatch() &&
                    instr->m_prev->m_opcode == Js::OpCode::Catch &&
                    instr->m_prev->m_prev->IsLabelInstr() &&
                    instr->m_prev->m_prev->AsLabelInstr()->GetRegion()->GetType() == RegionType::RegionTypeCatch); // Should also handle RegionTypeFinally

                break;
            }
            default:
            {
                if(this->currentBlock->loop && !this->IsLoopPrePass())
                {LOGMEIN("GlobOpt.cpp] 20207\n");
                    return instr;
                }
                break;
            }
        }
        RemoveCodeAfterNoFallthroughInstr(instr);
    }

    return instr;
}

void
GlobOpt::RemoveCodeAfterNoFallthroughInstr(IR::Instr *instr)
{LOGMEIN("GlobOpt.cpp] 20221\n");
    if (instr != this->currentBlock->GetLastInstr())
    {LOGMEIN("GlobOpt.cpp] 20223\n");
        // Remove dead code after bailout
        IR::Instr *instrDead = instr->m_next;
        IR::Instr *instrNext;

        for (; instrDead != this->currentBlock->GetLastInstr(); instrDead = instrNext)
        {LOGMEIN("GlobOpt.cpp] 20229\n");
            instrNext = instrDead->m_next;
            if (instrNext->m_opcode == Js::OpCode::FunctionExit)
            {LOGMEIN("GlobOpt.cpp] 20232\n");
                break;
            }
            this->func->m_fg->RemoveInstr(instrDead, this);
        }
        IR::Instr *instrNextBlock = instrDead->m_next;
        this->func->m_fg->RemoveInstr(instrDead, this);

        this->currentBlock->SetLastInstr(instrNextBlock->m_prev);
    }

    // Cleanup dead successors
    FOREACH_SUCCESSOR_BLOCK_EDITING(deadBlock, this->currentBlock, iter)
    {LOGMEIN("GlobOpt.cpp] 20245\n");
        this->currentBlock->RemoveDeadSucc(deadBlock, this->func->m_fg);
        if (this->currentBlock->GetDataUseCount() > 0)
        {LOGMEIN("GlobOpt.cpp] 20248\n");
            this->currentBlock->DecrementDataUseCount();
        }
    } NEXT_SUCCESSOR_BLOCK_EDITING;
}

void
GlobOpt::ProcessTryCatch(IR::Instr* instr)
{LOGMEIN("GlobOpt.cpp] 20256\n");
    Assert(instr->m_next->IsLabelInstr() && instr->m_next->AsLabelInstr()->GetRegion()->GetType() == RegionType::RegionTypeTry);

    Region* tryRegion = instr->m_next->AsLabelInstr()->GetRegion();
    BVSparse<JitArenaAllocator> * writeThroughSymbolsSet = tryRegion->writeThroughSymbolsSet;

    ToVar(writeThroughSymbolsSet, this->currentBlock);
}

void
GlobOpt::InsertToVarAtDefInTryRegion(IR::Instr * instr, IR::Opnd * dstOpnd)
{LOGMEIN("GlobOpt.cpp] 20267\n");
    if (this->currentRegion->GetType() == RegionTypeTry && dstOpnd->IsRegOpnd() && dstOpnd->AsRegOpnd()->m_sym->HasByteCodeRegSlot())
    {LOGMEIN("GlobOpt.cpp] 20269\n");
        StackSym * sym = dstOpnd->AsRegOpnd()->m_sym;
        if (sym->IsVar())
        {LOGMEIN("GlobOpt.cpp] 20272\n");
            return;
        }

        StackSym * varSym = sym->GetVarEquivSym(nullptr);
        if (this->currentRegion->writeThroughSymbolsSet->Test(varSym->m_id))
        {LOGMEIN("GlobOpt.cpp] 20278\n");
            IR::RegOpnd * regOpnd = IR::RegOpnd::New(varSym, IRType::TyVar, instr->m_func);
            this->ToVar(instr->m_next, regOpnd, this->currentBlock, NULL, false);
        }
    }
}

void
GlobOpt::RemoveFlowEdgeToCatchBlock(IR::Instr * instr)
{LOGMEIN("GlobOpt.cpp] 20287\n");
    Assert(instr->IsBranchInstr());

    BasicBlock * catchBlock = nullptr;
    BasicBlock * predBlock = nullptr;
    if (instr->m_opcode == Js::OpCode::BrOnException)
    {LOGMEIN("GlobOpt.cpp] 20293\n");
        catchBlock = instr->AsBranchInstr()->GetTarget()->GetBasicBlock();
        predBlock = this->currentBlock;
    }
    else if (instr->m_opcode == Js::OpCode::BrOnNoException)
    {LOGMEIN("GlobOpt.cpp] 20298\n");
        IR::Instr * nextInstr = instr->GetNextRealInstrOrLabel();
        Assert(nextInstr->IsLabelInstr());
        IR::LabelInstr * nextLabel = nextInstr->AsLabelInstr();

        if (nextLabel->GetRegion() && nextLabel->GetRegion()->GetType() == RegionTypeCatch)
        {LOGMEIN("GlobOpt.cpp] 20304\n");
            catchBlock = nextLabel->GetBasicBlock();
            predBlock = this->currentBlock;
        }
        else
        {
            Assert(nextLabel->m_next->IsBranchInstr() && nextLabel->m_next->AsBranchInstr()->IsUnconditional());
            BasicBlock * nextBlock = nextLabel->GetBasicBlock();
            IR::BranchInstr * branchToCatchBlock = nextLabel->m_next->AsBranchInstr();
            IR::LabelInstr * catchBlockLabel = branchToCatchBlock->GetTarget();
            Assert(catchBlockLabel->GetRegion()->GetType() == RegionTypeCatch);
            catchBlock = catchBlockLabel->GetBasicBlock();
            predBlock = nextBlock;
        }
    }

    Assert(catchBlock);
    Assert(predBlock);
    if (this->func->m_fg->FindEdge(predBlock, catchBlock))
    {LOGMEIN("GlobOpt.cpp] 20323\n");
        predBlock->RemoveDeadSucc(catchBlock, this->func->m_fg);
        if (predBlock == this->currentBlock)
        {LOGMEIN("GlobOpt.cpp] 20326\n");
            predBlock->DecrementDataUseCount();
        }
    }
}

IR::Instr *
GlobOpt::OptPeep(IR::Instr *instr, Value *src1Val, Value *src2Val)
{LOGMEIN("GlobOpt.cpp] 20334\n");
    IR::Opnd *dst, *src1, *src2;

    if (this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 20338\n");
        return instr;
    }

    switch (instr->m_opcode)
    {LOGMEIN("GlobOpt.cpp] 20343\n");
        case Js::OpCode::DeadBrEqual:
        case Js::OpCode::DeadBrRelational:
        case Js::OpCode::DeadBrSrEqual:
            src1 = instr->GetSrc1();
            src2 = instr->GetSrc2();

            // These branches were turned into dead branches because they were unnecessary (branch to next, ...).
            // The DeadBr are necessary in case the evaluation of the sources have side-effects.
            // If we know for sure the srcs are primitive or have been type specialized, we don't need these instructions
            if (((src1Val && src1Val->GetValueInfo()->IsPrimitive()) || (src1->IsRegOpnd() && this->IsTypeSpecialized(src1->AsRegOpnd()->m_sym, this->currentBlock))) &&
                ((src2Val && src2Val->GetValueInfo()->IsPrimitive()) || (src2->IsRegOpnd() && this->IsTypeSpecialized(src2->AsRegOpnd()->m_sym, this->currentBlock))))
            {LOGMEIN("GlobOpt.cpp] 20355\n");
                this->CaptureByteCodeSymUses(instr);
                instr->m_opcode = Js::OpCode::Nop;
            }
            break;
        case Js::OpCode::DeadBrOnHasProperty:
            src1 = instr->GetSrc1();

            if (((src1Val && src1Val->GetValueInfo()->IsPrimitive()) || (src1->IsRegOpnd() && this->IsTypeSpecialized(src1->AsRegOpnd()->m_sym, this->currentBlock))))
            {LOGMEIN("GlobOpt.cpp] 20364\n");
                this->CaptureByteCodeSymUses(instr);
                instr->m_opcode = Js::OpCode::Nop;
            }
            break;
        case Js::OpCode::Ld_A:
        case Js::OpCode::Ld_I4:
            src1 = instr->GetSrc1();
            dst = instr->GetDst();

            if (dst->IsRegOpnd() && dst->IsEqual(src1))
            {LOGMEIN("GlobOpt.cpp] 20375\n");
                dst = instr->UnlinkDst();
                if (!dst->GetIsJITOptimizedReg())
                {LOGMEIN("GlobOpt.cpp] 20378\n");
                    IR::ByteCodeUsesInstr *bytecodeUse = IR::ByteCodeUsesInstr::New(instr);
                    bytecodeUse->SetDst(dst);
                    instr->InsertAfter(bytecodeUse);
                }
                instr->FreeSrc1();
                instr->m_opcode = Js::OpCode::Nop;
            }
            break;
    }
    return instr;
}

void
GlobOpt::OptimizeIndirUses(IR::IndirOpnd *indirOpnd, IR::Instr * *pInstr, Value **indirIndexValRef)
{LOGMEIN("GlobOpt.cpp] 20393\n");
    IR::Instr * &instr = *pInstr;
    Assert(!indirIndexValRef || !*indirIndexValRef);

    // Update value types and copy-prop the base
    OptSrc(indirOpnd->GetBaseOpnd(), &instr, nullptr, indirOpnd);

    IR::RegOpnd *indexOpnd = indirOpnd->GetIndexOpnd();
    if (!indexOpnd)
    {LOGMEIN("GlobOpt.cpp] 20402\n");
        return;
    }

    // Update value types and copy-prop the index
    Value *indexVal = OptSrc(indexOpnd, &instr, nullptr, indirOpnd);
    if(indirIndexValRef)
    {LOGMEIN("GlobOpt.cpp] 20409\n");
        *indirIndexValRef = indexVal;
    }
}

bool
ValueInfo::IsGeneric() const
{LOGMEIN("GlobOpt.cpp] 20416\n");
    return structureKind == ValueStructureKind::Generic;
}

bool
ValueInfo::IsIntConstant() const
{LOGMEIN("GlobOpt.cpp] 20422\n");
    return IsInt() && structureKind == ValueStructureKind::IntConstant;
}

const IntConstantValueInfo *
ValueInfo::AsIntConstant() const
{LOGMEIN("GlobOpt.cpp] 20428\n");
    Assert(IsIntConstant());
    return static_cast<const IntConstantValueInfo *>(this);
}

bool
ValueInfo::IsIntRange() const
{LOGMEIN("GlobOpt.cpp] 20435\n");
    return IsInt() && structureKind == ValueStructureKind::IntRange;
}

const IntRangeValueInfo *
ValueInfo::AsIntRange() const
{LOGMEIN("GlobOpt.cpp] 20441\n");
    Assert(IsIntRange());
    return static_cast<const IntRangeValueInfo *>(this);
}

bool
ValueInfo::IsIntBounded() const
{LOGMEIN("GlobOpt.cpp] 20448\n");
    const bool isIntBounded = IsLikelyInt() && structureKind == ValueStructureKind::IntBounded;

    // Bounds for definitely int values should have relative bounds, otherwise those values should use one of the other value
    // infos
    Assert(!isIntBounded || static_cast<const IntBoundedValueInfo *>(this)->Bounds()->RequiresIntBoundedValueInfo(Type()));

    return isIntBounded;
}

const IntBoundedValueInfo *
ValueInfo::AsIntBounded() const
{LOGMEIN("GlobOpt.cpp] 20460\n");
    Assert(IsIntBounded());
    return static_cast<const IntBoundedValueInfo *>(this);
}

bool
ValueInfo::IsFloatConstant() const
{LOGMEIN("GlobOpt.cpp] 20467\n");
    return IsFloat() && structureKind == ValueStructureKind::FloatConstant;
}

FloatConstantValueInfo *
ValueInfo::AsFloatConstant()
{LOGMEIN("GlobOpt.cpp] 20473\n");
    Assert(IsFloatConstant());
    return static_cast<FloatConstantValueInfo *>(this);
}

const FloatConstantValueInfo *
ValueInfo::AsFloatConstant() const
{LOGMEIN("GlobOpt.cpp] 20480\n");
    Assert(IsFloatConstant());
    return static_cast<const FloatConstantValueInfo *>(this);
}

bool
ValueInfo::IsVarConstant() const
{LOGMEIN("GlobOpt.cpp] 20487\n");
    return structureKind == ValueStructureKind::VarConstant;
}

VarConstantValueInfo *
ValueInfo::AsVarConstant()
{LOGMEIN("GlobOpt.cpp] 20493\n");
    Assert(IsVarConstant());
    return static_cast<VarConstantValueInfo *>(this);
}

bool
ValueInfo::IsJsType() const
{LOGMEIN("GlobOpt.cpp] 20500\n");
    Assert(!(structureKind == ValueStructureKind::JsType && !IsUninitialized()));
    return structureKind == ValueStructureKind::JsType;
}

JsTypeValueInfo *
ValueInfo::AsJsType()
{LOGMEIN("GlobOpt.cpp] 20507\n");
    Assert(IsJsType());
    return static_cast<JsTypeValueInfo *>(this);
}

const JsTypeValueInfo *
ValueInfo::AsJsType() const
{LOGMEIN("GlobOpt.cpp] 20514\n");
    Assert(IsJsType());
    return static_cast<const JsTypeValueInfo *>(this);
}

bool
ValueInfo::IsArrayValueInfo() const
{LOGMEIN("GlobOpt.cpp] 20521\n");
    return IsAnyOptimizedArray() && structureKind == ValueStructureKind::Array;
}

const
ArrayValueInfo *ValueInfo::AsArrayValueInfo() const
{LOGMEIN("GlobOpt.cpp] 20527\n");
    Assert(IsArrayValueInfo());
    return static_cast<const ArrayValueInfo *>(this);
}

ArrayValueInfo *
ValueInfo::AsArrayValueInfo()
{LOGMEIN("GlobOpt.cpp] 20534\n");
    Assert(IsArrayValueInfo());
    return static_cast<ArrayValueInfo *>(this);
}

ValueInfo *
ValueInfo::SpecializeToInt32(JitArenaAllocator *const allocator, const bool isForLoopBackEdgeCompensation)
{LOGMEIN("GlobOpt.cpp] 20541\n");
    // Int specialization in some uncommon loop cases involving dependencies, needs to allow specializing values of arbitrary
    // types, even values that are definitely not int, to compensate for aggressive assumptions made by a loop prepass. In all
    // other cases, only values that are likely int may be int-specialized.
    Assert(IsUninitialized() || IsLikelyInt() || isForLoopBackEdgeCompensation);

    if(IsInt())
    {LOGMEIN("GlobOpt.cpp] 20548\n");
        return this;
    }

    if(!IsIntBounded())
    {LOGMEIN("GlobOpt.cpp] 20553\n");
        ValueInfo *const newValueInfo = CopyWithGenericStructureKind(allocator);
        newValueInfo->Type() = ValueType::GetInt(true);
        return newValueInfo;
    }

    const IntBoundedValueInfo *const boundedValueInfo = AsIntBounded();
    const IntBounds *const bounds = boundedValueInfo->Bounds();
    const IntConstantBounds constantBounds = bounds->ConstantBounds();
    if(bounds->RequiresIntBoundedValueInfo())
    {LOGMEIN("GlobOpt.cpp] 20563\n");
        IntBoundedValueInfo *const newValueInfo = boundedValueInfo->Copy(allocator);
        newValueInfo->Type() = constantBounds.GetValueType();
        return newValueInfo;
    }

    ValueInfo *const newValueInfo =
        constantBounds.IsConstant()
            ? static_cast<ValueInfo *>(IntConstantValueInfo::New(allocator, constantBounds.LowerBound()))
            : IntRangeValueInfo::New(allocator, constantBounds.LowerBound(), constantBounds.UpperBound(), false);
    newValueInfo->SetSymStore(GetSymStore());
    return newValueInfo;
}

ValueInfo *
ValueInfo::SpecializeToFloat64(JitArenaAllocator *const allocator)
{LOGMEIN("GlobOpt.cpp] 20579\n");
    if(IsNumber())
    {LOGMEIN("GlobOpt.cpp] 20581\n");
        return this;
    }

    ValueInfo *const newValueInfo = CopyWithGenericStructureKind(allocator);

    // If the value type was likely int, after float-specializing, it's preferable to use Int_Number rather than Float, as the
    // former is also likely int and allows int specialization later.
    newValueInfo->Type() = IsLikelyInt() ? Type().ToDefiniteAnyNumber() : Type().ToDefiniteAnyFloat();

    return newValueInfo;
}

// SIMD_JS
ValueInfo *
ValueInfo::SpecializeToSimd128(IRType type, JitArenaAllocator *const allocator)
{LOGMEIN("GlobOpt.cpp] 20597\n");
    switch (type)
    {LOGMEIN("GlobOpt.cpp] 20599\n");
    case TySimd128F4:
        return SpecializeToSimd128F4(allocator);
    case TySimd128I4:
        return SpecializeToSimd128I4(allocator);
    default:
        Assert(UNREACHED);
        return nullptr;
    }

}

ValueInfo *
ValueInfo::SpecializeToSimd128F4(JitArenaAllocator *const allocator)
{LOGMEIN("GlobOpt.cpp] 20613\n");
    if (IsSimd128Float32x4())
    {LOGMEIN("GlobOpt.cpp] 20615\n");
        return this;
    }

    ValueInfo *const newValueInfo = CopyWithGenericStructureKind(allocator);

    newValueInfo->Type() = ValueType::GetSimd128(ObjectType::Simd128Float32x4);

    return newValueInfo;
}

ValueInfo *
ValueInfo::SpecializeToSimd128I4(JitArenaAllocator *const allocator)
{LOGMEIN("GlobOpt.cpp] 20628\n");
    if (IsSimd128Int32x4())
    {LOGMEIN("GlobOpt.cpp] 20630\n");
        return this;
    }

    ValueInfo *const newValueInfo = CopyWithGenericStructureKind(allocator);

    newValueInfo->Type() = ValueType::GetSimd128(ObjectType::Simd128Int32x4);

    return newValueInfo;
}

bool
ValueInfo::GetIsShared() const
{LOGMEIN("GlobOpt.cpp] 20643\n");
    return IsJsType() ? AsJsType()->GetIsShared() : false;
}

void
ValueInfo::SetIsShared()
{LOGMEIN("GlobOpt.cpp] 20649\n");
    if (IsJsType()) AsJsType()->SetIsShared();
}

ValueInfo *
ValueInfo::Copy(JitArenaAllocator * allocator)
{LOGMEIN("GlobOpt.cpp] 20655\n");
    if(IsIntConstant())
    {LOGMEIN("GlobOpt.cpp] 20657\n");
        return AsIntConstant()->Copy(allocator);
    }
    if(IsIntRange())
    {LOGMEIN("GlobOpt.cpp] 20661\n");
        return AsIntRange()->Copy(allocator);
    }
    if(IsIntBounded())
    {LOGMEIN("GlobOpt.cpp] 20665\n");
        return AsIntBounded()->Copy(allocator);
    }
    if(IsFloatConstant())
    {LOGMEIN("GlobOpt.cpp] 20669\n");
        return AsFloatConstant()->Copy(allocator);
    }
    if(IsJsType())
    {LOGMEIN("GlobOpt.cpp] 20673\n");
        return AsJsType()->Copy(allocator);
    }
    if(IsArrayValueInfo())
    {LOGMEIN("GlobOpt.cpp] 20677\n");
        return AsArrayValueInfo()->Copy(allocator);
    }
    return CopyWithGenericStructureKind(allocator);
}

bool
ValueInfo::GetIntValMinMax(int *pMin, int *pMax, bool doAggressiveIntTypeSpec)
{LOGMEIN("GlobOpt.cpp] 20685\n");
    IntConstantBounds intConstantBounds;
    if (TryGetIntConstantBounds(&intConstantBounds, doAggressiveIntTypeSpec))
    {LOGMEIN("GlobOpt.cpp] 20688\n");
        *pMin = intConstantBounds.LowerBound();
        *pMax = intConstantBounds.UpperBound();
        return true;
    }

    Assert(!IsInt());
    Assert(!doAggressiveIntTypeSpec || !IsLikelyInt());
    return false;
}

bool
GlobOpt::IsPREInstrCandidateLoad(Js::OpCode opcode)
{LOGMEIN("GlobOpt.cpp] 20701\n");
    switch (opcode)
    {LOGMEIN("GlobOpt.cpp] 20703\n");
    case Js::OpCode::LdFld:
    case Js::OpCode::LdFldForTypeOf:
    case Js::OpCode::LdRootFld:
    case Js::OpCode::LdRootFldForTypeOf:
    case Js::OpCode::LdMethodFld:
    case Js::OpCode::LdRootMethodFld:
    case Js::OpCode::LdSlot:
    case Js::OpCode::LdSlotArr:
        return true;
    }

    return false;
}

bool
GlobOpt::IsPREInstrCandidateStore(Js::OpCode opcode)
{LOGMEIN("GlobOpt.cpp] 20720\n");
    switch (opcode)
    {LOGMEIN("GlobOpt.cpp] 20722\n");
    case Js::OpCode::StFld:
    case Js::OpCode::StRootFld:
    case Js::OpCode::StSlot:
        return true;
    }

    return false;
}

bool
GlobOpt::ImplicitCallFlagsAllowOpts(Loop *loop)
{LOGMEIN("GlobOpt.cpp] 20734\n");
    return loop->GetImplicitCallFlags() != Js::ImplicitCall_HasNoInfo &&
        (((loop->GetImplicitCallFlags() & ~Js::ImplicitCall_Accessor) | Js::ImplicitCall_None) == Js::ImplicitCall_None);
}

bool
GlobOpt::ImplicitCallFlagsAllowOpts(Func *func)
{LOGMEIN("GlobOpt.cpp] 20741\n");
    return func->m_fg->implicitCallFlags != Js::ImplicitCall_HasNoInfo &&
        (((func->m_fg->implicitCallFlags & ~Js::ImplicitCall_Accessor) | Js::ImplicitCall_None) == Js::ImplicitCall_None);
}

#if DBG_DUMP
void ValueInfo::Dump()
{LOGMEIN("GlobOpt.cpp] 20748\n");
    if(!IsJsType()) // The value type is uninitialized for a type value
    {LOGMEIN("GlobOpt.cpp] 20750\n");
        char typeStr[VALUE_TYPE_MAX_STRING_SIZE];
        Type().ToString(typeStr);
        Output::Print(_u("%S"), typeStr);
    }

    IntConstantBounds intConstantBounds;
    if(TryGetIntConstantBounds(&intConstantBounds))
    {LOGMEIN("GlobOpt.cpp] 20758\n");
        if(intConstantBounds.IsConstant())
        {LOGMEIN("GlobOpt.cpp] 20760\n");
            Output::Print(_u(" constant:%d"), intConstantBounds.LowerBound());
            return;
        }
        Output::Print(_u(" range:%d - %d"), intConstantBounds.LowerBound(), intConstantBounds.UpperBound());
    }
    else if(IsFloatConstant())
    {LOGMEIN("GlobOpt.cpp] 20767\n");
        Output::Print(_u(" constant:%g"), AsFloatConstant()->FloatValue());
    }
    else if(IsJsType())
    {LOGMEIN("GlobOpt.cpp] 20771\n");
        const JITTypeHolder type(AsJsType()->GetJsType());
        type != nullptr ? Output::Print(_u("type: 0x%p, "), type->GetAddr()) : Output::Print(_u("type: null, "));
        Output::Print(_u("type Set: "));
        Js::EquivalentTypeSet* typeSet = AsJsType()->GetJsTypeSet();
        if (typeSet != nullptr)
        {LOGMEIN("GlobOpt.cpp] 20777\n");
            uint16 typeCount = typeSet->GetCount();
            for (uint16 ti = 0; ti < typeCount - 1; ti++)
            {LOGMEIN("GlobOpt.cpp] 20780\n");
                Output::Print(_u("0x%p, "), typeSet->GetType(ti));
            }
            Output::Print(_u("0x%p"), typeSet->GetType(typeCount - 1));
        }
        else
        {
            Output::Print(_u("null"));
        }
    }
    else if(IsArrayValueInfo())
    {LOGMEIN("GlobOpt.cpp] 20791\n");
        const ArrayValueInfo *const arrayValueInfo = AsArrayValueInfo();
        if(arrayValueInfo->HeadSegmentSym())
        {LOGMEIN("GlobOpt.cpp] 20794\n");
            Output::Print(_u(" seg: "));
            arrayValueInfo->HeadSegmentSym()->Dump();
        }
        if(arrayValueInfo->HeadSegmentLengthSym())
        {LOGMEIN("GlobOpt.cpp] 20799\n");
            Output::Print(_u(" segLen: "));
            arrayValueInfo->HeadSegmentLengthSym()->Dump();
        }
        if(arrayValueInfo->LengthSym())
        {LOGMEIN("GlobOpt.cpp] 20804\n");
            Output::Print(_u(" len: "));
            arrayValueInfo->LengthSym()->Dump();
        }
    }

    if (this->GetSymStore())
    {LOGMEIN("GlobOpt.cpp] 20811\n");
        Output::Print(_u("\t\tsym:"));
        this->GetSymStore()->Dump();
    }
}

void
GlobOpt::Dump()
{LOGMEIN("GlobOpt.cpp] 20819\n");
    this->DumpSymToValueMap();
}

void
GlobOpt::DumpSymToValueMap(GlobHashTable* symToValueMap)
{LOGMEIN("GlobOpt.cpp] 20825\n");
    if (symToValueMap != nullptr)
    {LOGMEIN("GlobOpt.cpp] 20827\n");
        symToValueMap->Dump(GlobOpt::DumpSym);
    }
}

void
GlobOpt::DumpSymToValueMap(BasicBlock *block)
{LOGMEIN("GlobOpt.cpp] 20834\n");
    Output::Print(_u("\n*** SymToValueMap ***\n"));
    DumpSymToValueMap(block->globOptData.symToValueMap);
}

void
GlobOpt::DumpSymToValueMap()
{LOGMEIN("GlobOpt.cpp] 20841\n");
    DumpSymToValueMap(this->currentBlock);
}

void
GlobOpt::DumpSym(Sym *sym)
{LOGMEIN("GlobOpt.cpp] 20847\n");
    sym->Dump();
}

void
GlobOpt::DumpSymVal(int index)
{LOGMEIN("GlobOpt.cpp] 20853\n");
    SymID id = index;
    extern Func *CurrentFunc;
    Sym *sym = this->func->m_symTable->Find(id);

    AssertMsg(sym, "Sym not found!!!");

    Output::Print(_u("Sym: "));
    sym->Dump();

    Output::Print(_u("\t\tValueNumber: "));
    Value ** pValue = this->blockData.symToValueMap->Get(sym->m_id);
    (*pValue)->Dump();

    Output::Print(_u("\n"));
}

void
GlobOpt::Trace(BasicBlock * block, bool before)
{LOGMEIN("GlobOpt.cpp] 20872\n");
    bool globOptTrace = Js::Configuration::Global.flags.Trace.IsEnabled(Js::GlobOptPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId());
    bool typeSpecTrace = Js::Configuration::Global.flags.Trace.IsEnabled(Js::TypeSpecPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId());
    bool floatTypeSpecTrace = Js::Configuration::Global.flags.Trace.IsEnabled(Js::FloatTypeSpecPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId());
    bool fieldHoistTrace = Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldHoistPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId());
    bool fieldCopyPropTrace = fieldHoistTrace || Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldCopyPropPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId());
    bool objTypeSpecTrace = Js::Configuration::Global.flags.Trace.IsEnabled(Js::ObjTypeSpecPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId());
    bool valueTableTrace = Js::Configuration::Global.flags.Trace.IsEnabled(Js::ValueTablePhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId());
    bool fieldPRETrace = Js::Configuration::Global.flags.Trace.IsEnabled(Js::FieldPREPhase, this->func->GetSourceContextId(), this->func->GetLocalFunctionId());

    bool anyTrace = globOptTrace || typeSpecTrace || floatTypeSpecTrace || fieldCopyPropTrace || fieldHoistTrace || objTypeSpecTrace || valueTableTrace || fieldPRETrace;

    if (!anyTrace)
    {LOGMEIN("GlobOpt.cpp] 20885\n");
        return;
    }

    if (fieldPRETrace && this->IsLoopPrePass())
    {LOGMEIN("GlobOpt.cpp] 20890\n");
        if (block->isLoopHeader && before)
        {LOGMEIN("GlobOpt.cpp] 20892\n");
            Output::Print(_u("====  Loop Prepass block header #%-3d,  Visiting Loop block head #%-3d\n"),
                this->prePassLoop->GetHeadBlock()->GetBlockNum(), block->GetBlockNum());
        }
    }

    if (!typeSpecTrace && !floatTypeSpecTrace && !valueTableTrace && !Js::Configuration::Global.flags.Verbose)
    {LOGMEIN("GlobOpt.cpp] 20899\n");
        return;
    }

    if (before)
    {LOGMEIN("GlobOpt.cpp] 20904\n");
        Output::Print(_u("========================================================================\n"));
        Output::Print(_u("Begin OptBlock: Block #%-3d"), block->GetBlockNum());
        if (block->loop)
        {LOGMEIN("GlobOpt.cpp] 20908\n");
            Output::Print(_u("     Loop block header:%-3d  currentLoop block head:%-3d   %s"),
                block->loop->GetHeadBlock()->GetBlockNum(),
                this->prePassLoop ? this->prePassLoop->GetHeadBlock()->GetBlockNum() : 0,
                this->IsLoopPrePass() ? _u("PrePass") : _u(""));
        }
        Output::Print(_u("\n"));
    }
    else
    {
        Output::Print(_u("-----------------------------------------------------------------------\n"));
        Output::Print(_u("After OptBlock: Block #%-3d\n"), block->GetBlockNum());
    }
    if ((typeSpecTrace || floatTypeSpecTrace) && !block->globOptData.liveVarSyms->IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 20922\n");
        Output::Print(_u("    Live var syms: "));
        block->globOptData.liveVarSyms->Dump();
    }
    if (typeSpecTrace && !block->globOptData.liveInt32Syms->IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 20927\n");
        Assert(this->tempBv->IsEmpty());
        this->tempBv->Minus(block->globOptData.liveInt32Syms, block->globOptData.liveLossyInt32Syms);
        if(!this->tempBv->IsEmpty())
        {LOGMEIN("GlobOpt.cpp] 20931\n");
            Output::Print(_u("    Int32 type specialized (lossless) syms: "));
            this->tempBv->Dump();
        }
        this->tempBv->ClearAll();
        if(!block->globOptData.liveLossyInt32Syms->IsEmpty())
        {LOGMEIN("GlobOpt.cpp] 20937\n");
            Output::Print(_u("    Int32 converted (lossy) syms: "));
            block->globOptData.liveLossyInt32Syms->Dump();
        }
    }
    if (floatTypeSpecTrace && !block->globOptData.liveFloat64Syms->IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 20943\n");
        Output::Print(_u("    Float64 type specialized syms: "));
        block->globOptData.liveFloat64Syms->Dump();
    }
    if ((fieldCopyPropTrace || objTypeSpecTrace) && this->DoFieldCopyProp(block->loop) && !block->globOptData.liveFields->IsEmpty())
    {LOGMEIN("GlobOpt.cpp] 20948\n");
        Output::Print(_u("    Live field syms: "));
        block->globOptData.liveFields->Dump();
    }
    if ((fieldHoistTrace || objTypeSpecTrace) && this->DoFieldHoisting(block->loop) && HasHoistableFields(block))
    {LOGMEIN("GlobOpt.cpp] 20953\n");
        Output::Print(_u("    Hoistable field sym: "));
        block->globOptData.hoistableFields->Dump();
    }
    if (objTypeSpecTrace || valueTableTrace)
    {LOGMEIN("GlobOpt.cpp] 20958\n");
        Output::Print(_u("    Value table:\n"));
        DumpSymToValueMap(block->globOptData.symToValueMap);
    }

    if (before)
    {LOGMEIN("GlobOpt.cpp] 20964\n");
        Output::Print(_u("-----------------------------------------------------------------------\n")); \
    }

    Output::Flush();
}

void
GlobOpt::TraceSettings()
{LOGMEIN("GlobOpt.cpp] 20973\n");
    Output::Print(_u("GlobOpt Settings:\r\n"));
    Output::Print(_u("    FloatTypeSpec: %s\r\n"), this->DoFloatTypeSpec() ? _u("enabled") : _u("disabled"));
    Output::Print(_u("    AggressiveIntTypeSpec: %s\r\n"), this->DoAggressiveIntTypeSpec() ? _u("enabled") : _u("disabled"));
    Output::Print(_u("    LossyIntTypeSpec: %s\r\n"), this->DoLossyIntTypeSpec() ? _u("enabled") : _u("disabled"));
    Output::Print(_u("    ArrayCheckHoist: %s\r\n"),  (this->func->HasProfileInfo() && this->func->GetReadOnlyProfileInfo()->IsArrayCheckHoistDisabled(func->IsLoopBody())) ? _u("disabled") : _u("enabled"));
    Output::Print(_u("    ImplicitCallFlags: %s\r\n"), Js::DynamicProfileInfo::GetImplicitCallFlagsString(this->func->m_fg->implicitCallFlags));
    for (Loop * loop = this->func->m_fg->loopList; loop != NULL; loop = loop->next)
    {LOGMEIN("GlobOpt.cpp] 20981\n");
        Output::Print(_u("        loop: %d, ImplicitCallFlags: %s\r\n"), loop->GetLoopNumber(),
            Js::DynamicProfileInfo::GetImplicitCallFlagsString(loop->GetImplicitCallFlags()));
    }

    Output::Flush();
}
#endif  // DBG_DUMP

IR::Instr *
GlobOpt::TrackMarkTempObject(IR::Instr * instrStart, IR::Instr * instrLast)
{LOGMEIN("GlobOpt.cpp] 20992\n");
    if (!this->func->GetHasMarkTempObjects())
    {LOGMEIN("GlobOpt.cpp] 20994\n");
        return instrLast;
    }
    IR::Instr * instr = instrStart;
    IR::Instr * instrEnd = instrLast->m_next;
    IR::Instr * lastInstr = nullptr;
    GlobOptBlockData& globOptData = this->currentBlock->globOptData;
    do
    {LOGMEIN("GlobOpt.cpp] 21002\n");
        bool mayNeedBailOnImplicitCallsPreOp = !this->IsLoopPrePass()
            && instr->HasAnyImplicitCalls()
            && globOptData.maybeTempObjectSyms != nullptr;
        if (mayNeedBailOnImplicitCallsPreOp)
        {LOGMEIN("GlobOpt.cpp] 21007\n");
            IR::Opnd * src1 = instr->GetSrc1();
            if (src1)
            {LOGMEIN("GlobOpt.cpp] 21010\n");
                instr = GenerateBailOutMarkTempObjectIfNeeded(instr, src1, false);
                IR::Opnd * src2 = instr->GetSrc2();
                if (src2)
                {LOGMEIN("GlobOpt.cpp] 21014\n");
                    instr = GenerateBailOutMarkTempObjectIfNeeded(instr, src2, false);
                }
            }
        }

        IR::Opnd *dst = instr->GetDst();
        if (dst)
        {LOGMEIN("GlobOpt.cpp] 21022\n");
            if (dst->IsRegOpnd())
            {
                TrackTempObjectSyms(instr, dst->AsRegOpnd());
            }
            else if (mayNeedBailOnImplicitCallsPreOp)
            {LOGMEIN("GlobOpt.cpp] 21028\n");
                instr = GenerateBailOutMarkTempObjectIfNeeded(instr, dst, true);
            }
        }

        lastInstr = instr;
        instr = instr->m_next;
    }
    while (instr != instrEnd);
    return lastInstr;
}

void
GlobOpt::TrackTempObjectSyms(IR::Instr * instr, IR::RegOpnd * opnd)
{LOGMEIN("GlobOpt.cpp] 21042\n");
    // If it is marked as dstIsTempObject, we should have mark temped it, or type specialized it to Ld_I4.
    Assert(!instr->dstIsTempObject || ObjectTempVerify::CanMarkTemp(instr, nullptr));
    GlobOptBlockData& globOptData = this->currentBlock->globOptData;
    bool canStoreTemp = false;
    bool maybeTemp = false;
    if (OpCodeAttr::TempObjectProducing(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 21049\n");
        maybeTemp = instr->dstIsTempObject;

        // We have to make sure that lower will always generate code to do stack allocation
        // before we can store any other stack instance onto it. Otherwise, we would not
        // walk object to box the stack property.
        canStoreTemp = instr->dstIsTempObject && ObjectTemp::CanStoreTemp(instr);
    }
    else if (OpCodeAttr::TempObjectTransfer(instr->m_opcode))
    {LOGMEIN("GlobOpt.cpp] 21058\n");
        // Need to check both sources, GetNewScObject has two srcs for transfer.
        // No need to get var equiv sym here as transfer of type spec value does not transfer a mark temp object.
        maybeTemp = globOptData.maybeTempObjectSyms && (
            (instr->GetSrc1()->IsRegOpnd() && globOptData.maybeTempObjectSyms->Test(instr->GetSrc1()->AsRegOpnd()->m_sym->m_id))
            || (instr->GetSrc2() && instr->GetSrc2()->IsRegOpnd() && globOptData.maybeTempObjectSyms->Test(instr->GetSrc2()->AsRegOpnd()->m_sym->m_id)));

        canStoreTemp = globOptData.canStoreTempObjectSyms && (
            (instr->GetSrc1()->IsRegOpnd() && globOptData.canStoreTempObjectSyms->Test(instr->GetSrc1()->AsRegOpnd()->m_sym->m_id))
            && (!instr->GetSrc2() || (instr->GetSrc2()->IsRegOpnd() && globOptData.canStoreTempObjectSyms->Test(instr->GetSrc2()->AsRegOpnd()->m_sym->m_id))));

        Assert(!canStoreTemp || instr->dstIsTempObject);
        Assert(!maybeTemp || instr->dstIsTempObject);
    }

    // Need to get the var equiv sym as assignment of type specialized sym kill the var sym value anyway.
    StackSym * sym = opnd->m_sym;
    if (!sym->IsVar())
    {LOGMEIN("GlobOpt.cpp] 21076\n");
        sym = sym->GetVarEquivSym(nullptr);
        if (sym == nullptr)
        {LOGMEIN("GlobOpt.cpp] 21079\n");
            return;
        }
    }

    SymID symId = sym->m_id;
    if (maybeTemp)
    {LOGMEIN("GlobOpt.cpp] 21086\n");
        // Only var sym should be temp objects
        Assert(opnd->m_sym == sym);

        if (globOptData.maybeTempObjectSyms == nullptr)
        {LOGMEIN("GlobOpt.cpp] 21091\n");
            globOptData.maybeTempObjectSyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
        }
        globOptData.maybeTempObjectSyms->Set(symId);

        if (canStoreTemp)
        {LOGMEIN("GlobOpt.cpp] 21097\n");
            if (instr->m_opcode == Js::OpCode::NewScObjectLiteral && !this->IsLoopPrePass())
            {LOGMEIN("GlobOpt.cpp] 21099\n");
                // For object literal, we install the final type up front.
                // If there are bailout before we finish initializing all the fields, we need to
                // zero out the rest if we stack allocate the literal, so that the boxing would not
                // try to box trash pointer in the properties.

                // Although object Literal initialization can be done lexically, BailOnNoProfile may cause some path
                // to disappear. Do it is flow base make it easier to stop propagate those entries.

                IR::IntConstOpnd * propertyArrayIdOpnd = instr->GetSrc1()->AsIntConstOpnd();
                const Js::PropertyIdArray * propIds = instr->m_func->GetJITFunctionBody()->ReadPropertyIdArrayFromAuxData(propertyArrayIdOpnd->AsUint32());

                // Duplicates are removed by parser
                Assert(!propIds->hadDuplicates);

                if (globOptData.stackLiteralInitFldDataMap == nullptr)
                {LOGMEIN("GlobOpt.cpp] 21115\n");
                    globOptData.stackLiteralInitFldDataMap = JitAnew(alloc, StackLiteralInitFldDataMap, alloc);
                }
                else
                {
                    Assert(!globOptData.stackLiteralInitFldDataMap->ContainsKey(sym));
                }
                StackLiteralInitFldData data = { propIds, 0};
                globOptData.stackLiteralInitFldDataMap->AddNew(sym, data);
            }

            if (globOptData.canStoreTempObjectSyms == nullptr)
            {LOGMEIN("GlobOpt.cpp] 21127\n");
                globOptData.canStoreTempObjectSyms = JitAnew(this->alloc, BVSparse<JitArenaAllocator>, this->alloc);
            }
            globOptData.canStoreTempObjectSyms->Set(symId);
        }
        else if (globOptData.canStoreTempObjectSyms)
        {LOGMEIN("GlobOpt.cpp] 21133\n");
            globOptData.canStoreTempObjectSyms->Clear(symId);
        }
    }
    else
    {
        Assert(!canStoreTemp);
        if (globOptData.maybeTempObjectSyms)
        {LOGMEIN("GlobOpt.cpp] 21141\n");
            if (globOptData.canStoreTempObjectSyms)
            {LOGMEIN("GlobOpt.cpp] 21143\n");
                globOptData.canStoreTempObjectSyms->Clear(symId);
            }
            globOptData.maybeTempObjectSyms->Clear(symId);
        }
        else
        {
            Assert(!globOptData.canStoreTempObjectSyms);
        }

        // The symbol is being assigned to, the sym shouldn't still be in the stackLiteralInitFldDataMap
        Assert(this->IsLoopPrePass() ||
            globOptData.stackLiteralInitFldDataMap == nullptr
            || globOptData.stackLiteralInitFldDataMap->Count() == 0
            || !globOptData.stackLiteralInitFldDataMap->ContainsKey(sym));
    }
}

IR::Instr *
GlobOpt::GenerateBailOutMarkTempObjectIfNeeded(IR::Instr * instr, IR::Opnd * opnd, bool isDst)
{LOGMEIN("GlobOpt.cpp] 21163\n");
    Assert(opnd);
    Assert(isDst == (opnd == instr->GetDst()));
    Assert(opnd != instr->GetDst() || !opnd->IsRegOpnd());
    Assert(!this->IsLoopPrePass());
    Assert(instr->HasAnyImplicitCalls());

    // Only dst reg opnd opcode or ArgOut_A should have dstIsTempObject marked
    Assert(!isDst || !instr->dstIsTempObject || instr->m_opcode == Js::OpCode::ArgOut_A);

    // Post-op implicit call shouldn't have installed yet
    Assert(!instr->HasBailOutInfo() || (instr->GetBailOutKind() & IR::BailOutKindBits) != IR::BailOutOnImplicitCalls);

    GlobOptBlockData& globOptData = this->currentBlock->globOptData;
    Assert(globOptData.maybeTempObjectSyms != nullptr);

    IR::PropertySymOpnd * propertySymOpnd = nullptr;
    StackSym * stackSym = ObjectTemp::GetStackSym(opnd, &propertySymOpnd);

    // It is okay to not get the var equiv sym here, as use of a type specialized sym is not use of the temp object
    // so no need to add mark temp bailout.
    // TempObjectSysm doesn't contain any type spec sym, so we will get false here for all type spec sym.
    if (stackSym && globOptData.maybeTempObjectSyms->Test(stackSym->m_id))
    {LOGMEIN("GlobOpt.cpp] 21186\n");
        if (instr->HasBailOutInfo())
        {LOGMEIN("GlobOpt.cpp] 21188\n");
            instr->SetBailOutKind(instr->GetBailOutKind() | IR::BailOutMarkTempObject);
        }
        else
        {
            // On insert the pre op bailout if it is not Direct field access do nothing, don't check the dst yet.
            // SetTypeCheckBailout will clear this out if it is direct field access.
            if (isDst
                || (instr->m_opcode == Js::OpCode::FromVar && !opnd->GetValueType().IsPrimitive())
                || propertySymOpnd == nullptr
                || !propertySymOpnd->IsTypeCheckProtected())
            {LOGMEIN("GlobOpt.cpp] 21199\n");
                this->GenerateBailAtOperation(&instr, IR::BailOutMarkTempObject);
            }
        }

        if (!opnd->IsRegOpnd() && (!isDst || (globOptData.canStoreTempObjectSyms && globOptData.canStoreTempObjectSyms->Test(stackSym->m_id))))
        {LOGMEIN("GlobOpt.cpp] 21205\n");
            // If this opnd is a dst, that means that the object pointer is a stack object,
            // and we can store temp object/number on it.
            // If the opnd is a src, that means that the object pointer may be a stack object
            // so the load may be a temp object/number and we need to track its use.

            // Don't mark start of indir as can store temp, because we don't actually know
            // what it is assigning to.
            if (!isDst || !opnd->IsIndirOpnd())
            {LOGMEIN("GlobOpt.cpp] 21214\n");
                opnd->SetCanStoreTemp();
            }

            if (propertySymOpnd)
            {LOGMEIN("GlobOpt.cpp] 21219\n");
                // Track initfld of stack literals
                if (isDst && instr->m_opcode == Js::OpCode::InitFld)
                {LOGMEIN("GlobOpt.cpp] 21222\n");
                    const Js::PropertyId propertyId = propertySymOpnd->m_sym->AsPropertySym()->m_propertyId;

                    // We don't need to track numeric properties init
                    if (!this->func->GetThreadContextInfo()->IsNumericProperty(propertyId))
                    {LOGMEIN("GlobOpt.cpp] 21227\n");
                        DebugOnly(bool found = false);
                        globOptData.stackLiteralInitFldDataMap->RemoveIf(stackSym,
                            [&](StackSym * key, StackLiteralInitFldData & data)
                        {
                            DebugOnly(found = true);
                            Assert(key == stackSym);
                            Assert(data.currentInitFldCount < data.propIds->count);

                            if (data.propIds->elements[data.currentInitFldCount] != propertyId)
                            {LOGMEIN("GlobOpt.cpp] 21237\n");
#if DBG
                                bool duplicate = false;
                                for (uint i = 0; i < data.currentInitFldCount; i++)
                                {LOGMEIN("GlobOpt.cpp] 21241\n");
                                    if (data.propIds->elements[i] == propertyId)
                                    {LOGMEIN("GlobOpt.cpp] 21243\n");
                                        duplicate = true;
                                        break;
                                    }
                                }
                                Assert(duplicate);
#endif
                                // duplicate initialization
                                return false;
                            }
                            bool finished = (++data.currentInitFldCount == data.propIds->count);
#if DBG
                            if (finished)
                            {LOGMEIN("GlobOpt.cpp] 21256\n");
                                // We can still track the finished stack literal InitFld lexically.
                                this->finishedStackLiteralInitFld->Set(stackSym->m_id);
                            }
#endif
                            return finished;
                        });
                        // We might still see InitFld even we have finished with all the property Id because
                        // of duplicate entries at the end
                        Assert(found || finishedStackLiteralInitFld->Test(stackSym->m_id));
                    }
                }
            }
        }
    }
    return instr;
}

void
GlobOpt::KillStateForGeneratorYield()
{LOGMEIN("GlobOpt.cpp] 21276\n");
    GlobOptBlockData* globOptData = &this->currentBlock->globOptData;

    /*
    TODO[generators][ianhall]: Do a ToVar on any typespec'd syms before the bailout so that we can enable typespec in generators without bailin having to restore typespec'd values
    FOREACH_BITSET_IN_SPARSEBV(symId, globOptData->liveInt32Syms)
    {
        this->ToVar(instr, , this->currentBlock, , );
    }
    NEXT_BITSET_IN_SPARSEBV;

    FOREACH_BITSET_IN_SPARSEBV(symId, globOptData->liveInt32Syms)
    {
        this->ToVar(instr, , this->currentBlock, , );
    }
    NEXT_BITSET_IN_SPARSEBV;
    */

    FOREACH_GLOBHASHTABLE_ENTRY(bucket, globOptData->symToValueMap)
    {LOGMEIN("GlobOpt.cpp] 21295\n");
        ValueType type = bucket.element->GetValueInfo()->Type().ToLikely();
        bucket.element = this->NewGenericValue(type);
    }
    NEXT_GLOBHASHTABLE_ENTRY;

    globOptData->exprToValueMap->ClearAll();
    globOptData->liveFields->ClearAll();
    globOptData->liveArrayValues->ClearAll();
    if (globOptData->maybeWrittenTypeSyms)
    {LOGMEIN("GlobOpt.cpp] 21305\n");
        globOptData->maybeWrittenTypeSyms->ClearAll();
    }
    globOptData->isTempSrc->ClearAll();
    globOptData->liveInt32Syms->ClearAll();
    globOptData->liveLossyInt32Syms->ClearAll();
    globOptData->liveFloat64Syms->ClearAll();

    // SIMD_JS
    globOptData->liveSimd128F4Syms->ClearAll();
    globOptData->liveSimd128I4Syms->ClearAll();

    if (globOptData->hoistableFields)
    {LOGMEIN("GlobOpt.cpp] 21318\n");
        globOptData->hoistableFields->ClearAll();
    }
    // Keep globOptData->liveVarSyms as is
    // Keep globOptData->argObjSyms as is

    // MarkTemp should be disabled for generator functions for now
    Assert(globOptData->maybeTempObjectSyms == nullptr || globOptData->maybeTempObjectSyms->IsEmpty());
    Assert(globOptData->canStoreTempObjectSyms == nullptr || globOptData->canStoreTempObjectSyms->IsEmpty());

    globOptData->valuesToKillOnCalls->Clear();
    if (globOptData->inductionVariables)
    {LOGMEIN("GlobOpt.cpp] 21330\n");
        globOptData->inductionVariables->Clear();
    }
    if (globOptData->availableIntBoundChecks)
    {LOGMEIN("GlobOpt.cpp] 21334\n");
        globOptData->availableIntBoundChecks->Clear();
    }

    // Keep bailout data as is
    globOptData->hasCSECandidates = false;
}

LoopCount *
GlobOpt::GetOrGenerateLoopCountForMemOp(Loop *loop)
{LOGMEIN("GlobOpt.cpp] 21344\n");
    LoopCount *loopCount = loop->loopCount;

    if (loopCount && !loopCount->HasGeneratedLoopCountSym())
    {LOGMEIN("GlobOpt.cpp] 21348\n");
        Assert(loop->bailOutInfo);
        EnsureBailTarget(loop);
        GenerateLoopCountPlusOne(loop, loopCount);
    }

    return loopCount;
}

IR::Opnd *
GlobOpt::GenerateInductionVariableChangeForMemOp(Loop *loop, byte unroll, IR::Instr *insertBeforeInstr)
{LOGMEIN("GlobOpt.cpp] 21359\n");
    LoopCount *loopCount = loop->loopCount;
    IR::Opnd *sizeOpnd = nullptr;
    Assert(loopCount);
    Assert(loop->memOpInfo->inductionVariableOpndPerUnrollMap);
    if (loop->memOpInfo->inductionVariableOpndPerUnrollMap->TryGetValue(unroll, &sizeOpnd))
    {LOGMEIN("GlobOpt.cpp] 21365\n");
        return sizeOpnd;
    }

    Func *localFunc = loop->GetFunc();

    const auto InsertInstr = [&](IR::Instr *instr)
    {LOGMEIN("GlobOpt.cpp] 21372\n");
        if (insertBeforeInstr == nullptr)
        {LOGMEIN("GlobOpt.cpp] 21374\n");
            loop->landingPad->InsertAfter(instr);
        }
        else
        {
            insertBeforeInstr->InsertBefore(instr);
        }
    };

    if (loopCount->LoopCountMinusOneSym())
    {LOGMEIN("GlobOpt.cpp] 21384\n");
        IRType type = loopCount->LoopCountSym()->GetType();

        // Loop count is off by one, so add one
        IR::RegOpnd *loopCountOpnd = IR::RegOpnd::New(loopCount->LoopCountSym(), type, localFunc);
        sizeOpnd = loopCountOpnd;

        if (unroll != 1)
        {LOGMEIN("GlobOpt.cpp] 21392\n");
            sizeOpnd = IR::RegOpnd::New(TyUint32, this->func);

            IR::Opnd *unrollOpnd = IR::IntConstOpnd::New(unroll, type, localFunc, true);

            InsertInstr(IR::Instr::New(Js::OpCode::Mul_I4,
                sizeOpnd,
                loopCountOpnd,
                unrollOpnd,
                localFunc));

        }
    }
    else
    {
        uint size = (loopCount->LoopCountMinusOneConstantValue() + 1)  * unroll;
        sizeOpnd = IR::IntConstOpnd::New(size, IRType::TyUint32, localFunc, true);
    }
    loop->memOpInfo->inductionVariableOpndPerUnrollMap->Add(unroll, sizeOpnd);
    return sizeOpnd;
}

IR::RegOpnd*
GlobOpt::GenerateStartIndexOpndForMemop(Loop *loop, IR::Opnd *indexOpnd, IR::Opnd *sizeOpnd, bool isInductionVariableChangeIncremental, bool bIndexAlreadyChanged, IR::Instr *insertBeforeInstr)
{LOGMEIN("GlobOpt.cpp] 21416\n");
    IR::RegOpnd *startIndexOpnd = nullptr;
    Func *localFunc = loop->GetFunc();
    IRType type = indexOpnd->GetType();

    const int cacheIndex = ((int)isInductionVariableChangeIncremental << 1) | (int)bIndexAlreadyChanged;
    if (loop->memOpInfo->startIndexOpndCache[cacheIndex])
    {LOGMEIN("GlobOpt.cpp] 21423\n");
        return loop->memOpInfo->startIndexOpndCache[cacheIndex];
    }
    const auto InsertInstr = [&](IR::Instr *instr)
    {LOGMEIN("GlobOpt.cpp] 21427\n");
        if (insertBeforeInstr == nullptr)
        {LOGMEIN("GlobOpt.cpp] 21429\n");
            loop->landingPad->InsertAfter(instr);
        }
        else
        {
            insertBeforeInstr->InsertBefore(instr);
        }
    };

    startIndexOpnd = IR::RegOpnd::New(type, localFunc);

    // If the 2 are different we can simply use indexOpnd
    if (isInductionVariableChangeIncremental != bIndexAlreadyChanged)
    {LOGMEIN("GlobOpt.cpp] 21442\n");
        InsertInstr(IR::Instr::New(Js::OpCode::Ld_A,
                                   startIndexOpnd,
                                   indexOpnd,
                                   localFunc));
    }
    else
    {
        // Otherwise add 1 to it
        InsertInstr(IR::Instr::New(Js::OpCode::Add_I4,
                                   startIndexOpnd,
                                   indexOpnd,
                                   IR::IntConstOpnd::New(1, type, localFunc, true),
                                   localFunc));
    }

    if (!isInductionVariableChangeIncremental)
    {LOGMEIN("GlobOpt.cpp] 21459\n");
        InsertInstr(IR::Instr::New(Js::OpCode::Sub_I4,
                                   startIndexOpnd,
                                   startIndexOpnd,
                                   sizeOpnd,
                                   localFunc));
    }

    loop->memOpInfo->startIndexOpndCache[cacheIndex] = startIndexOpnd;
    return startIndexOpnd;
}

IR::Instr*
GlobOpt::FindUpperBoundsCheckInstr(IR::Instr* fromInstr)
{LOGMEIN("GlobOpt.cpp] 21473\n");
    IR::Instr *upperBoundCheck = fromInstr;
    do
    {LOGMEIN("GlobOpt.cpp] 21476\n");
        upperBoundCheck = upperBoundCheck->m_prev;
        Assert(upperBoundCheck);
        Assert(!upperBoundCheck->IsLabelInstr());
    } while (upperBoundCheck->m_opcode != Js::OpCode::BoundCheck);
    return upperBoundCheck;
}

IR::Instr*
GlobOpt::FindArraySegmentLoadInstr(IR::Instr* fromInstr)
{LOGMEIN("GlobOpt.cpp] 21486\n");
    IR::Instr *headSegmentLengthLoad = fromInstr;
    do
    {LOGMEIN("GlobOpt.cpp] 21489\n");
        headSegmentLengthLoad = headSegmentLengthLoad->m_prev;
        Assert(headSegmentLengthLoad);
        Assert(!headSegmentLengthLoad->IsLabelInstr());
    } while (headSegmentLengthLoad->m_opcode != Js::OpCode::LdIndir);
    return headSegmentLengthLoad;
}

void
GlobOpt::RemoveMemOpSrcInstr(IR::Instr* memopInstr, IR::Instr* srcInstr, BasicBlock* block)
{LOGMEIN("GlobOpt.cpp] 21499\n");
    Assert(srcInstr && (srcInstr->m_opcode == Js::OpCode::LdElemI_A || srcInstr->m_opcode == Js::OpCode::StElemI_A || srcInstr->m_opcode == Js::OpCode::StElemI_A_Strict));
    Assert(memopInstr && (memopInstr->m_opcode == Js::OpCode::Memcopy || memopInstr->m_opcode == Js::OpCode::Memset));
    Assert(block);
    const bool isDst = srcInstr->m_opcode == Js::OpCode::StElemI_A || srcInstr->m_opcode == Js::OpCode::StElemI_A_Strict;
    IR::RegOpnd* opnd = (isDst ? memopInstr->GetDst() : memopInstr->GetSrc1())->AsIndirOpnd()->GetBaseOpnd();
    IR::ArrayRegOpnd* arrayOpnd = opnd->IsArrayRegOpnd() ? opnd->AsArrayRegOpnd() : nullptr;

    IR::Instr* topInstr = srcInstr;
    if (srcInstr->extractedUpperBoundCheckWithoutHoisting)
    {LOGMEIN("GlobOpt.cpp] 21509\n");
        IR::Instr *upperBoundCheck = FindUpperBoundsCheckInstr(srcInstr);
        Assert(upperBoundCheck && upperBoundCheck != srcInstr);
        topInstr = upperBoundCheck;
    }
    if (srcInstr->loadedArrayHeadSegmentLength && arrayOpnd && arrayOpnd->HeadSegmentLengthSym())
    {LOGMEIN("GlobOpt.cpp] 21515\n");
        IR::Instr *arrayLoadSegmentHeadLength = FindArraySegmentLoadInstr(topInstr);
        Assert(arrayLoadSegmentHeadLength);
        topInstr = arrayLoadSegmentHeadLength;
        arrayOpnd->RemoveHeadSegmentLengthSym();
    }
    if (srcInstr->loadedArrayHeadSegment && arrayOpnd && arrayOpnd->HeadSegmentSym())
    {LOGMEIN("GlobOpt.cpp] 21522\n");
        IR::Instr *arrayLoadSegmentHead = FindArraySegmentLoadInstr(topInstr);
        Assert(arrayLoadSegmentHead);
        topInstr = arrayLoadSegmentHead;
        arrayOpnd->RemoveHeadSegmentSym();
    }

    // If no bounds check are present, simply look up for instruction added for instrumentation
    if(topInstr == srcInstr)
    {LOGMEIN("GlobOpt.cpp] 21531\n");
        bool checkPrev = true;
        while (checkPrev)
        {LOGMEIN("GlobOpt.cpp] 21534\n");
            switch (topInstr->m_prev->m_opcode)
            {LOGMEIN("GlobOpt.cpp] 21536\n");
            case Js::OpCode::BailOnNotArray:
            case Js::OpCode::NoImplicitCallUses:
            case Js::OpCode::ByteCodeUses:
                topInstr = topInstr->m_prev;
                checkPrev = !!topInstr->m_prev;
                break;
            default:
                checkPrev = false;
                break;
            }
        }
    }

    while (topInstr != srcInstr)
    {LOGMEIN("GlobOpt.cpp] 21551\n");
        IR::Instr* removeInstr = topInstr;
        topInstr = topInstr->m_next;
        Assert(
            removeInstr->m_opcode == Js::OpCode::BailOnNotArray ||
            removeInstr->m_opcode == Js::OpCode::NoImplicitCallUses ||
            removeInstr->m_opcode == Js::OpCode::ByteCodeUses ||
            removeInstr->m_opcode == Js::OpCode::LdIndir ||
            removeInstr->m_opcode == Js::OpCode::BoundCheck
        );
        if (removeInstr->m_opcode != Js::OpCode::ByteCodeUses)
        {LOGMEIN("GlobOpt.cpp] 21562\n");
            block->RemoveInstr(removeInstr);
        }
    }
    this->ConvertToByteCodeUses(srcInstr);
}

void
GlobOpt::GetMemOpSrcInfo(Loop* loop, IR::Instr* instr, IR::RegOpnd*& base, IR::RegOpnd*& index, IRType& arrayType)
{LOGMEIN("GlobOpt.cpp] 21571\n");
    Assert(instr && (instr->m_opcode == Js::OpCode::LdElemI_A || instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict));
    IR::Opnd* arrayOpnd = instr->m_opcode == Js::OpCode::LdElemI_A ? instr->GetSrc1() : instr->GetDst();
    Assert(arrayOpnd->IsIndirOpnd());

    IR::IndirOpnd* indirArrayOpnd = arrayOpnd->AsIndirOpnd();
    IR::RegOpnd* baseOpnd = (IR::RegOpnd*)indirArrayOpnd->GetBaseOpnd();
    IR::RegOpnd* indexOpnd = (IR::RegOpnd*)indirArrayOpnd->GetIndexOpnd();
    Assert(baseOpnd);
    Assert(indexOpnd);

    // Process Out Params
    base = baseOpnd;
    index = indexOpnd;
    arrayType = indirArrayOpnd->GetType();
}

void
GlobOpt::EmitMemop(Loop * loop, LoopCount *loopCount, const MemOpEmitData* emitData)
{LOGMEIN("GlobOpt.cpp] 21590\n");
    Assert(emitData);
    Assert(emitData->candidate);
    Assert(emitData->stElemInstr);
    Assert(emitData->stElemInstr->m_opcode == Js::OpCode::StElemI_A || emitData->stElemInstr->m_opcode == Js::OpCode::StElemI_A_Strict);
    IR::BailOutKind bailOutKind = emitData->bailOutKind;

    const byte unroll = emitData->inductionVar.unroll;
    Assert(unroll == 1);
    const bool isInductionVariableChangeIncremental = emitData->inductionVar.isIncremental;
    const bool bIndexAlreadyChanged = emitData->candidate->bIndexAlreadyChanged;

    IR::RegOpnd *baseOpnd = nullptr;
    IR::RegOpnd *indexOpnd = nullptr;
    IRType dstType;
    GetMemOpSrcInfo(loop, emitData->stElemInstr, baseOpnd, indexOpnd, dstType);

    Func *localFunc = loop->GetFunc();

    // Handle bailout info
    EnsureBailTarget(loop);
    Assert(bailOutKind != IR::BailOutInvalid);

    // Keep only Array bits bailOuts. Consider handling these bailouts instead of simply ignoring them
    bailOutKind &= IR::BailOutForArrayBits;

    // Add our custom bailout to handle Op_MemCopy return value.
    bailOutKind |= IR::BailOutOnMemOpError;
    BailOutInfo *const bailOutInfo = loop->bailOutInfo;
    Assert(bailOutInfo);

    IR::Instr *insertBeforeInstr = bailOutInfo->bailOutInstr;
    Assert(insertBeforeInstr);
    IR::Opnd *sizeOpnd = GenerateInductionVariableChangeForMemOp(loop, unroll, insertBeforeInstr);
    IR::RegOpnd *startIndexOpnd = GenerateStartIndexOpndForMemop(loop, indexOpnd, sizeOpnd, isInductionVariableChangeIncremental, bIndexAlreadyChanged, insertBeforeInstr);
    IR::IndirOpnd* dstOpnd = IR::IndirOpnd::New(baseOpnd, startIndexOpnd, dstType, localFunc);

    IR::Opnd *src1;
    const bool isMemset = emitData->candidate->IsMemSet();

    // Get the source according to the memop type
    if (isMemset)
    {LOGMEIN("GlobOpt.cpp] 21632\n");
        MemSetEmitData* data = (MemSetEmitData*)emitData;
        const Loop::MemSetCandidate* candidate = data->candidate->AsMemSet();
        if (candidate->srcSym)
        {LOGMEIN("GlobOpt.cpp] 21636\n");
            IR::RegOpnd* regSrc = IR::RegOpnd::New(candidate->srcSym, candidate->srcSym->GetType(), func);
            regSrc->SetIsJITOptimizedReg(true);
            src1 = regSrc;
        }
        else
        {
            src1 = IR::AddrOpnd::New(candidate->constant.ToVar(localFunc), IR::AddrOpndKindConstantAddress, localFunc);
        }
    }
    else
    {
        Assert(emitData->candidate->IsMemCopy());

        MemCopyEmitData* data = (MemCopyEmitData*)emitData;
        Assert(data->ldElemInstr);
        Assert(data->ldElemInstr->m_opcode == Js::OpCode::LdElemI_A);

        IR::RegOpnd *srcBaseOpnd = nullptr;
        IR::RegOpnd *srcIndexOpnd = nullptr;
        IRType srcType;
        GetMemOpSrcInfo(loop, data->ldElemInstr, srcBaseOpnd, srcIndexOpnd, srcType);
        Assert(GetVarSymID(srcIndexOpnd->GetStackSym()) == GetVarSymID(indexOpnd->GetStackSym()));

        src1 = IR::IndirOpnd::New(srcBaseOpnd, startIndexOpnd, srcType, localFunc);
    }

    // Generate memcopy
    IR::Instr* memopInstr = IR::BailOutInstr::New(isMemset ? Js::OpCode::Memset : Js::OpCode::Memcopy, bailOutKind, bailOutInfo, localFunc);
    memopInstr->SetDst(dstOpnd);
    memopInstr->SetSrc1(src1);
    memopInstr->SetSrc2(sizeOpnd);
    insertBeforeInstr->InsertBefore(memopInstr);

#if DBG_DUMP
    if (DO_MEMOP_TRACE())
    {LOGMEIN("GlobOpt.cpp] 21672\n");
        char valueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
        baseOpnd->GetValueType().ToString(valueTypeStr);
        const int loopCountBufSize = 16;
        char16 loopCountBuf[loopCountBufSize];
        if (loopCount->LoopCountMinusOneSym())
        {
            swprintf_s(loopCountBuf, _u("s%u"), loopCount->LoopCountMinusOneSym()->m_id);
        }
        else
        {
            swprintf_s(loopCountBuf, _u("%u"), loopCount->LoopCountMinusOneConstantValue() + 1);
        }
        if (isMemset)
        {LOGMEIN("GlobOpt.cpp] 21686\n");
            const Loop::MemSetCandidate* candidate = emitData->candidate->AsMemSet();
            const int constBufSize = 32;
            char16 constBuf[constBufSize];
            if (candidate->srcSym)
            {
                swprintf_s(constBuf, _u("s%u"), candidate->srcSym->m_id);
            }
            else
            {
                switch (candidate->constant.type)
                {LOGMEIN("GlobOpt.cpp] 21697\n");
                case TyInt8:
                case TyInt16:
                case TyInt32:
                case TyInt64:
                    swprintf_s(constBuf, sizeof(IntConstType) == 8 ? _u("%lld") : _u("%d"), candidate->constant.u.intConst.value);
                    break;
                case TyFloat32:
                case TyFloat64:
                    swprintf_s(constBuf, _u("%.4f"), candidate->constant.u.floatConst.value);
                    break;
                case TyVar:
                    swprintf_s(constBuf, sizeof(Js::Var) == 8 ? _u("0x%.16llX") : _u("0x%.8X"), candidate->constant.u.varConst.value);
                    break;
                default:
                    AssertMsg(false, "Unsupported constant type");
                    swprintf_s(constBuf, _u("Unknown"));
                    break;
                }
            }
            TRACE_MEMOP_PHASE(MemSet, loop, emitData->stElemInstr,
                              _u("ValueType: %S, Base: s%u, Index: s%u, Constant: %s, LoopCount: %s, IsIndexChangedBeforeUse: %d"),
                              valueTypeStr,
                              candidate->base,
                              candidate->index,
                              constBuf,
                              loopCountBuf,
                              bIndexAlreadyChanged);
        }
        else
        {
            const Loop::MemCopyCandidate* candidate = emitData->candidate->AsMemCopy();
            TRACE_MEMOP_PHASE(MemCopy, loop, emitData->stElemInstr,
                              _u("ValueType: %S, StBase: s%u, Index: s%u, LdBase: s%u, LoopCount: %s, IsIndexChangedBeforeUse: %d"),
                              valueTypeStr,
                              candidate->base,
                              candidate->index,
                              candidate->ldBase,
                              loopCountBuf,
                              bIndexAlreadyChanged);
        }

    }
#endif

    RemoveMemOpSrcInstr(memopInstr, emitData->stElemInstr, emitData->block);
    if (!isMemset)
    {
        RemoveMemOpSrcInstr(memopInstr, ((MemCopyEmitData*)emitData)->ldElemInstr, emitData->block);
    }
}

bool
GlobOpt::InspectInstrForMemSetCandidate(Loop* loop, IR::Instr* instr, MemSetEmitData* emitData, bool& errorInInstr)
{LOGMEIN("GlobOpt.cpp] 21751\n");
    Assert(emitData && emitData->candidate && emitData->candidate->IsMemSet());
    Loop::MemSetCandidate* candidate = (Loop::MemSetCandidate*)emitData->candidate;
    if (instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict)
    {LOGMEIN("GlobOpt.cpp] 21755\n");
        if (instr->GetDst()->IsIndirOpnd()
            && (GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym()) == candidate->base)
            && (GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym()) == candidate->index)
            )
        {LOGMEIN("GlobOpt.cpp] 21760\n");
            Assert(instr->IsProfiledInstr());
            emitData->stElemInstr = instr;
            emitData->bailOutKind = instr->GetBailOutKind();
            return true;
        }
        TRACE_MEMOP_PHASE_VERBOSE(MemSet, loop, instr, _u("Orphan StElemI_A detected"));
        errorInInstr = true;
    }
    else if (instr->m_opcode == Js::OpCode::LdElemI_A)
    {
        TRACE_MEMOP_PHASE_VERBOSE(MemSet, loop, instr, _u("Orphan LdElemI_A detected"));
        errorInInstr = true;
    }
    return false;
}

bool
GlobOpt::InspectInstrForMemCopyCandidate(Loop* loop, IR::Instr* instr, MemCopyEmitData* emitData, bool& errorInInstr)
{LOGMEIN("GlobOpt.cpp] 21779\n");
    Assert(emitData && emitData->candidate && emitData->candidate->IsMemCopy());
    Loop::MemCopyCandidate* candidate = (Loop::MemCopyCandidate*)emitData->candidate;
    if (instr->m_opcode == Js::OpCode::StElemI_A || instr->m_opcode == Js::OpCode::StElemI_A_Strict)
    {LOGMEIN("GlobOpt.cpp] 21783\n");
        if (
            instr->GetDst()->IsIndirOpnd() &&
            (GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym()) == candidate->base) &&
            (GetVarSymID(instr->GetDst()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym()) == candidate->index)
            )
        {LOGMEIN("GlobOpt.cpp] 21789\n");
            Assert(instr->IsProfiledInstr());
            emitData->stElemInstr = instr;
            emitData->bailOutKind = instr->GetBailOutKind();
            // Still need to find the LdElem
            return false;
        }
        TRACE_MEMOP_PHASE_VERBOSE(MemCopy, loop, instr, _u("Orphan StElemI_A detected"));
        errorInInstr = true;
    }
    else if (instr->m_opcode == Js::OpCode::LdElemI_A)
    {LOGMEIN("GlobOpt.cpp] 21800\n");
        if (
            emitData->stElemInstr &&
            instr->GetSrc1()->IsIndirOpnd() &&
            (GetVarSymID(instr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetStackSym()) == candidate->ldBase) &&
            (GetVarSymID(instr->GetSrc1()->AsIndirOpnd()->GetIndexOpnd()->GetStackSym()) == candidate->index)
            )
        {LOGMEIN("GlobOpt.cpp] 21807\n");
            Assert(instr->IsProfiledInstr());
            emitData->ldElemInstr = instr;
            ValueType stValueType = emitData->stElemInstr->GetDst()->AsIndirOpnd()->GetBaseOpnd()->GetValueType();
            ValueType ldValueType = emitData->ldElemInstr->GetSrc1()->AsIndirOpnd()->GetBaseOpnd()->GetValueType();
            if (stValueType != ldValueType)
            {LOGMEIN("GlobOpt.cpp] 21813\n");
#if DBG_DUMP
                char16 stValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
                stValueType.ToString(stValueTypeStr);
                char16 ldValueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
                ldValueType.ToString(ldValueTypeStr);
                TRACE_MEMOP_PHASE_VERBOSE(MemCopy, loop, instr, _u("for mismatch in Load(%s) and Store(%s) value type"), ldValueTypeStr, stValueTypeStr);
#endif
                errorInInstr = true;
                return false;
            }
            // We found both instruction for this candidate
            return true;
        }
        TRACE_MEMOP_PHASE_VERBOSE(MemCopy, loop, instr, _u("Orphan LdElemI_A detected"));
        errorInInstr = true;
    }
    return false;
}

// The caller is responsible to free the memory allocated between inOrderEmitData[iEmitData -> end]
bool
GlobOpt::ValidateMemOpCandidates(Loop * loop, _Out_writes_(iEmitData) MemOpEmitData** inOrderEmitData, int& iEmitData)
{LOGMEIN("GlobOpt.cpp] 21836\n");
    AnalysisAssert(iEmitData == (int)loop->memOpInfo->candidates->Count());
    // We iterate over the second block of the loop only. MemOp Works only if the loop has exactly 2 blocks
    Assert(loop->blockList.HasTwo());

    Loop::MemOpList::Iterator iter(loop->memOpInfo->candidates);
    BasicBlock* bblock = loop->blockList.Head()->next;
    Loop::MemOpCandidate* candidate = nullptr;
    MemOpEmitData* emitData = nullptr;

    // Iterate backward because the list of candidate is reversed
    FOREACH_INSTR_BACKWARD_IN_BLOCK(instr, bblock)
    {LOGMEIN("GlobOpt.cpp] 21848\n");
        if (!candidate)
        {LOGMEIN("GlobOpt.cpp] 21850\n");
            // Time to check next candidate
            if (!iter.Next())
            {LOGMEIN("GlobOpt.cpp] 21853\n");
                // We have been through the whole list of candidates, finish
                break;
            }
            candidate = iter.Data();
            if (!candidate)
            {LOGMEIN("GlobOpt.cpp] 21859\n");
                continue;
            }

            // Common check for memset and memcopy
            Loop::InductionVariableChangeInfo inductionVariableChangeInfo = { 0, 0 };

            // Get the inductionVariable changeInfo
            if (!loop->memOpInfo->inductionVariableChangeInfoMap->TryGetValue(candidate->index, &inductionVariableChangeInfo))
            {
                TRACE_MEMOP_VERBOSE(loop, nullptr, _u("MemOp skipped (s%d): no induction variable"), candidate->base);
                return false;
            }

            if (inductionVariableChangeInfo.unroll != candidate->count)
            {
                TRACE_MEMOP_VERBOSE(loop, nullptr, _u("MemOp skipped (s%d): not matching unroll count"), candidate->base);
                return false;
            }

            if (candidate->IsMemSet())
            {LOGMEIN("GlobOpt.cpp] 21880\n");
                Assert(!PHASE_OFF(Js::MemSetPhase, this->func));
                emitData = JitAnew(this->alloc, MemSetEmitData);
            }
            else
            {
                Assert(!PHASE_OFF(Js::MemCopyPhase, this->func));
                // Specific check for memcopy
                Assert(candidate->IsMemCopy());
                Loop::MemCopyCandidate* memcopyCandidate = candidate->AsMemCopy();

                if (memcopyCandidate->base == Js::Constants::InvalidSymID
                    || memcopyCandidate->ldBase == Js::Constants::InvalidSymID
                    || (memcopyCandidate->ldCount != memcopyCandidate->count))
                {
                    TRACE_MEMOP_PHASE(MemCopy, loop, nullptr, _u("(s%d): not matching ldElem and stElem"), candidate->base);
                    return false;
                }
                emitData = JitAnew(this->alloc, MemCopyEmitData);
            }
            Assert(emitData);
            emitData->block = bblock;
            emitData->inductionVar = inductionVariableChangeInfo;
            emitData->candidate = candidate;
        }
        bool errorInInstr = false;
        bool candidateFound = candidate->IsMemSet() ?
            InspectInstrForMemSetCandidate(loop, instr, (MemSetEmitData*)emitData, errorInInstr)
            : InspectInstrForMemCopyCandidate(loop, instr, (MemCopyEmitData*)emitData, errorInInstr);
        if (errorInInstr)
        {LOGMEIN("GlobOpt.cpp] 21910\n");
            JitAdelete(this->alloc, emitData);
            return false;
        }
        if (candidateFound)
        {LOGMEIN("GlobOpt.cpp] 21915\n");
            AnalysisAssert(iEmitData > 0);
            if (iEmitData == 0)
            {LOGMEIN("GlobOpt.cpp] 21918\n");
                // Explicit for OACR
                break;
            }
            inOrderEmitData[--iEmitData] = emitData;
            candidate = nullptr;
            emitData = nullptr;

        }
    } NEXT_INSTR_BACKWARD_IN_BLOCK;

    if (iter.IsValid())
    {
        TRACE_MEMOP(loop, nullptr, _u("Candidates not found in loop while validating"));
        return false;
    }
    return true;
}

void
GlobOpt::ProcessMemOp()
{
    FOREACH_LOOP_IN_FUNC_EDITING(loop, this->func)
    {LOGMEIN("GlobOpt.cpp] 21941\n");
        if (HasMemOp(loop))
        {LOGMEIN("GlobOpt.cpp] 21943\n");
            const int candidateCount = loop->memOpInfo->candidates->Count();
            Assert(candidateCount > 0);

            LoopCount * loopCount = GetOrGenerateLoopCountForMemOp(loop);

            // If loopCount is not available we can not continue with memop
            if (!loopCount || !(loopCount->LoopCountMinusOneSym() || loopCount->LoopCountMinusOneConstantValue()))
            {
                TRACE_MEMOP(loop, nullptr, _u("MemOp skipped for no loop count"));
                loop->doMemOp = false;
                loop->memOpInfo->candidates->Clear();
                continue;
            }

            // The list is reversed, check them and place them in order in the following array
            MemOpEmitData** inOrderCandidates = JitAnewArray(this->alloc, MemOpEmitData*, candidateCount);
            int i = candidateCount;
            if (ValidateMemOpCandidates(loop, inOrderCandidates, i))
            {LOGMEIN("GlobOpt.cpp] 21962\n");
                Assert(i == 0);

                // Process the valid MemOp candidate in order.
                for (; i < candidateCount; ++i)
                {
                    // Emit
                    EmitMemop(loop, loopCount, inOrderCandidates[i]);
                    JitAdelete(this->alloc, inOrderCandidates[i]);
                }
            }
            else
            {
                Assert(i != 0);
                for (; i < candidateCount; ++i)
                {LOGMEIN("GlobOpt.cpp] 21977\n");
                    JitAdelete(this->alloc, inOrderCandidates[i]);
                }

                // One of the memop candidates did not validate. Do not emit for this loop.
                loop->doMemOp = false;
                loop->memOpInfo->candidates->Clear();
            }

            // Free memory
            JitAdeleteArray(this->alloc, candidateCount, inOrderCandidates);
        }
    } NEXT_LOOP_EDITING;
}

template<>
ValueNumber JsUtil::ValueToKey<ValueNumber, Value *>::ToKey(Value *const &value)
{LOGMEIN("GlobOpt.cpp] 21994\n");
    Assert(value);
    return value->GetValueNumber();
}
