//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"
#include "Base/EtwTrace.h"
#include "Base/ScriptContextProfiler.h"
#ifdef VTUNE_PROFILING
#include "Base/VTuneChakraProfile.h"
#endif

#include "Library/ForInObjectEnumerator.h"

Func::Func(JitArenaAllocator *alloc, JITTimeWorkItem * workItem,
    ThreadContextInfo * threadContextInfo,
    ScriptContextInfo * scriptContextInfo,
    JITOutputIDL * outputData,
    Js::EntryPointInfo* epInfo,
    const FunctionJITRuntimeInfo *const runtimeInfo,
    JITTimePolymorphicInlineCacheInfo * const polymorphicInlineCacheInfo, void * const codeGenAllocators,
#if !FLOATVAR
    CodeGenNumberAllocator * numberAllocator,
#endif
    Js::ScriptContextProfiler *const codeGenProfiler, const bool isBackgroundJIT, Func * parentFunc,
    uint postCallByteCodeOffset, Js::RegSlot returnValueRegSlot, const bool isInlinedConstructor,
    Js::ProfileId callSiteIdInParentFunc, bool isGetterSetter) :
    m_alloc(alloc),
    m_workItem(workItem),
    m_output(outputData),
    m_entryPointInfo(epInfo),
    m_threadContextInfo(threadContextInfo),
    m_scriptContextInfo(scriptContextInfo),
    m_runtimeInfo(runtimeInfo),
    m_polymorphicInlineCacheInfo(polymorphicInlineCacheInfo),
    m_codeGenAllocators(codeGenAllocators),
    m_inlineeId(0),
    pinnedTypeRefs(nullptr),
    singleTypeGuards(nullptr),
    equivalentTypeGuards(nullptr),
    propertyGuardsByPropertyId(nullptr),
    ctorCachesByPropertyId(nullptr),
    callSiteToArgumentsOffsetFixupMap(nullptr),
    indexedPropertyGuardCount(0),
    propertiesWrittenTo(nullptr),
    lazyBailoutProperties(alloc),
    anyPropertyMayBeWrittenTo(false),
#ifdef PROFILE_EXEC
    m_codeGenProfiler(codeGenProfiler),
#endif
    m_isBackgroundJIT(isBackgroundJIT),
    m_cloner(nullptr),
    m_cloneMap(nullptr),
    m_loopParamSym(nullptr),
    m_funcObjSym(nullptr),
    m_localClosureSym(nullptr),
    m_paramClosureSym(nullptr),
    m_localFrameDisplaySym(nullptr),
    m_bailoutReturnValueSym(nullptr),
    m_hasBailedOutSym(nullptr),
    m_inlineeFrameStartSym(nullptr),
    m_regsUsed(0),
    m_fg(nullptr),
    m_labelCount(0),
    m_argSlotsForFunctionsCalled(0),
    m_isLeaf(false),
    m_hasCalls(false),
    m_hasInlineArgsOpt(false),
    m_canDoInlineArgsOpt(true),
    m_doFastPaths(false),
    hasBailout(false),
    hasBailoutInEHRegion(false),
    hasInstrNumber(false),
    maintainByteCodeOffset(true),
    frameSize(0),
    parentFunc(parentFunc),
    argObjSyms(nullptr),
    m_nonTempLocalVars(nullptr),
    hasAnyStackNestedFunc(false),
    hasMarkTempObjects(false),
    postCallByteCodeOffset(postCallByteCodeOffset),
    maxInlineeArgOutCount(0),
    returnValueRegSlot(returnValueRegSlot),
    firstActualStackOffset(-1),
    m_localVarSlotsOffset(Js::Constants::InvalidOffset),
    m_hasLocalVarChangedOffset(Js::Constants::InvalidOffset),
    actualCount((Js::ArgSlot) - 1),
    tryCatchNestingLevel(0),
    m_localStackHeight(0),
    tempSymDouble(nullptr),
    tempSymBool(nullptr),
    hasInlinee(false),
    thisOrParentInlinerHasArguments(false),
    hasStackArgs(false),
    hasImplicitParamLoad(false),
    hasThrow(false),
    hasNonSimpleParams(false),
    hasUnoptimizedArgumentsAcccess(false),
    hasApplyTargetInlining(false),
    hasImplicitCalls(false),
    hasTempObjectProducingInstr(false),
    isInlinedConstructor(isInlinedConstructor),
#if !FLOATVAR
    numberAllocator(numberAllocator),
#endif
    loopCount(0),
    callSiteIdInParentFunc(callSiteIdInParentFunc),
    isGetterSetter(isGetterSetter),
    frameInfo(nullptr),
    isTJLoopBody(false),
    m_nativeCodeDataSym(nullptr),
    isFlowGraphValid(false),
#if DBG
    m_callSiteCount(0),
#endif
    stackNestedFunc(false),
    stackClosure(false)
#if defined(_M_ARM32_OR_ARM64)
    , m_ArgumentsOffset(0)
    , m_epilogLabel(nullptr)
#endif
    , m_funcStartLabel(nullptr)
    , m_funcEndLabel(nullptr)
#if DBG
    , hasCalledSetDoFastPaths(false)
    , allowRemoveBailOutArgInstr(false)
    , currentPhases(alloc)
    , isPostLower(false)
    , isPostRegAlloc(false)
    , isPostPeeps(false)
    , isPostLayout(false)
    , isPostFinalLower(false)
    , vtableMap(nullptr)
#endif
    , m_yieldOffsetResumeLabelList(nullptr)
    , m_bailOutNoSaveLabel(nullptr)
    , constantAddressRegOpnd(alloc)
    , lastConstantAddressRegLoadInstr(nullptr)
    , m_totalJumpTableSizeInBytesForSwitchStatements(0)
    , slotArrayCheckTable(nullptr)
    , frameDisplayCheckTable(nullptr)
    , stackArgWithFormalsTracker(nullptr)
    , m_forInLoopBaseDepth(0)
    , m_forInEnumeratorArrayOffset(-1)
    , argInsCount(0)
    , m_globalObjTypeSpecFldInfoArray(nullptr)
#ifdef RECYCLER_WRITE_BARRIER_JIT
    , m_lowerer(nullptr)
#endif
{

    Assert(this->IsInlined() == !!runtimeInfo);

    if (this->IsTopFunc())
    {TRACE_IT(2440);
        outputData->hasJittedStackClosure = false;
        outputData->localVarSlotsOffset = m_localVarSlotsOffset;
        outputData->localVarChangedOffset = m_hasLocalVarChangedOffset;
    }

    if (this->IsInlined())
    {TRACE_IT(2441);
        m_inlineeId = ++(GetTopFunc()->m_inlineeId);
    }
    bool doStackNestedFunc = GetJITFunctionBody()->DoStackNestedFunc();

    bool doStackClosure = GetJITFunctionBody()->DoStackClosure() && !PHASE_OFF(Js::FrameDisplayFastPathPhase, this) && !PHASE_OFF(Js::StackClosurePhase, this);
    Assert(!doStackClosure || doStackNestedFunc);
    this->stackClosure = doStackClosure && this->IsTopFunc();
    if (this->stackClosure)
    {TRACE_IT(2442);
        // TODO: calculate on runtime side?
        m_output.SetHasJITStackClosure();
    }

    if (m_workItem->Type() == JsFunctionType &&
        GetJITFunctionBody()->DoBackendArgumentsOptimization() &&
        !GetJITFunctionBody()->HasTry())
    {TRACE_IT(2443);
        // doBackendArgumentsOptimization bit is set when there is no eval inside a function
        // as determined by the bytecode generator.
        SetHasStackArgs(true);
    }
    if (doStackNestedFunc && GetJITFunctionBody()->GetNestedCount() != 0 &&
        (this->IsTopFunc() || this->GetTopFunc()->m_workItem->Type() != JsLoopBodyWorkItemType)) // make sure none of the functions inlined in a jitted loop body allocate nested functions on the stack
    {TRACE_IT(2444);
        Assert(!(this->IsJitInDebugMode() && !GetJITFunctionBody()->IsLibraryCode()));
        stackNestedFunc = true;
        this->GetTopFunc()->hasAnyStackNestedFunc = true;
    }

    if (GetJITFunctionBody()->HasOrParentHasArguments() || (parentFunc && parentFunc->thisOrParentInlinerHasArguments))
    {TRACE_IT(2445);
        thisOrParentInlinerHasArguments = true;
    }

    if (parentFunc == nullptr)
    {TRACE_IT(2446);
        inlineDepth = 0;
        m_symTable = JitAnew(alloc, SymTable);
        m_symTable->Init(this);
        m_symTable->SetStartingID(static_cast<SymID>(workItem->GetJITFunctionBody()->GetLocalsCount() + 1));

        Assert(Js::Constants::NoByteCodeOffset == postCallByteCodeOffset);
        Assert(Js::Constants::NoRegister == returnValueRegSlot);

#if defined(_M_IX86) ||  defined(_M_X64)
        if (HasArgumentSlot())
        {TRACE_IT(2447);
            // Pre-allocate the single argument slot we'll reserve for the arguments object.
            // For ARM, the argument slot is not part of the local but part of the register saves
            m_localStackHeight = MachArgsSlotOffset;
        }
#endif
    }
    else
    {TRACE_IT(2448);
        inlineDepth = parentFunc->inlineDepth + 1;
        Assert(Js::Constants::NoByteCodeOffset != postCallByteCodeOffset);
    }

    this->constructorCacheCount = 0;
    this->constructorCaches = AnewArrayZ(this->m_alloc, JITTimeConstructorCache*, GetJITFunctionBody()->GetProfiledCallSiteCount());

#if DBG_DUMP
    m_codeSize = -1;
#endif

#if defined(_M_X64)
    m_spillSize = -1;
    m_argsSize = -1;
    m_savedRegSize = -1;
#endif

    if (this->IsJitInDebugMode())
    {TRACE_IT(2449);
        m_nonTempLocalVars = Anew(this->m_alloc, BVSparse<JitArenaAllocator>, this->m_alloc);
    }

    if (GetJITFunctionBody()->IsCoroutine())
    {TRACE_IT(2450);
        m_yieldOffsetResumeLabelList = YieldOffsetResumeLabelList::New(this->m_alloc);
    }

    if (this->IsTopFunc())
    {TRACE_IT(2451);
        m_globalObjTypeSpecFldInfoArray = JitAnewArrayZ(this->m_alloc, JITObjTypeSpecFldInfo*, GetWorkItem()->GetJITTimeInfo()->GetGlobalObjTypeSpecFldInfoCount());
    }

    for (uint i = 0; i < GetJITFunctionBody()->GetInlineCacheCount(); ++i)
    {TRACE_IT(2452);
        JITObjTypeSpecFldInfo * info = GetWorkItem()->GetJITTimeInfo()->GetObjTypeSpecFldInfo(i);
        if (info != nullptr)
        {TRACE_IT(2453);
            Assert(info->GetObjTypeSpecFldId() < GetTopFunc()->GetWorkItem()->GetJITTimeInfo()->GetGlobalObjTypeSpecFldInfoCount());
            GetTopFunc()->m_globalObjTypeSpecFldInfoArray[info->GetObjTypeSpecFldId()] = info;
        }
    }

    canHoistConstantAddressLoad = !PHASE_OFF(Js::HoistConstAddrPhase, this);

    m_forInLoopMaxDepth = this->GetJITFunctionBody()->GetForInLoopDepth();
}

bool
Func::IsLoopBodyInTry() const
{TRACE_IT(2454);
    return IsLoopBody() && m_workItem->GetLoopHeader()->isInTry;
}

/* static */
void
Func::Codegen(JitArenaAllocator *alloc, JITTimeWorkItem * workItem,
    ThreadContextInfo * threadContextInfo,
    ScriptContextInfo * scriptContextInfo,
    JITOutputIDL * outputData,
    Js::EntryPointInfo* epInfo, // for in-proc jit only
    const FunctionJITRuntimeInfo *const runtimeInfo,
    JITTimePolymorphicInlineCacheInfo * const polymorphicInlineCacheInfo, void * const codeGenAllocators,
#if !FLOATVAR
    CodeGenNumberAllocator * numberAllocator,
#endif
    Js::ScriptContextProfiler *const codeGenProfiler, const bool isBackgroundJIT)
{TRACE_IT(2455);
    bool rejit;
    do
    {TRACE_IT(2456);
        Func func(alloc, workItem, threadContextInfo,
            scriptContextInfo, outputData, epInfo, runtimeInfo,
            polymorphicInlineCacheInfo, codeGenAllocators, 
#if !FLOATVAR
            numberAllocator,
#endif
            codeGenProfiler, isBackgroundJIT);
        try
        {TRACE_IT(2457);
            func.TryCodegen();
            rejit = false;
        }
        catch (Js::RejitException ex)
        {TRACE_IT(2458);
            // The work item needs to be rejitted, likely due to some optimization that was too aggressive
            if (ex.Reason() == RejitReason::AggressiveIntTypeSpecDisabled)
            {TRACE_IT(2459);
                workItem->GetJITFunctionBody()->GetProfileInfo()->DisableAggressiveIntTypeSpec(func.IsLoopBody());
                outputData->disableAggressiveIntTypeSpec = TRUE;
            }
            else if (ex.Reason() == RejitReason::InlineApplyDisabled)
            {TRACE_IT(2460);
                workItem->GetJITFunctionBody()->DisableInlineApply();
                outputData->disableInlineApply = TRUE;
            }
            else if (ex.Reason() == RejitReason::InlineSpreadDisabled)
            {TRACE_IT(2461);
                workItem->GetJITFunctionBody()->DisableInlineSpread();
                outputData->disableInlineSpread = TRUE;
            }
            else if (ex.Reason() == RejitReason::DisableStackArgOpt)
            {TRACE_IT(2462);
                workItem->GetJITFunctionBody()->GetProfileInfo()->DisableStackArgOpt();
                outputData->disableStackArgOpt = TRUE;
            }
            else if (ex.Reason() == RejitReason::DisableSwitchOptExpectingInteger ||
                ex.Reason() == RejitReason::DisableSwitchOptExpectingString)
            {TRACE_IT(2463);
                workItem->GetJITFunctionBody()->GetProfileInfo()->DisableSwitchOpt();
                outputData->disableSwitchOpt = TRUE;
            }
            else
            {TRACE_IT(2464);
                Assert(ex.Reason() == RejitReason::TrackIntOverflowDisabled);
                workItem->GetJITFunctionBody()->GetProfileInfo()->DisableTrackCompoundedIntOverflow();
                outputData->disableTrackCompoundedIntOverflow = TRUE;
            }

            if (PHASE_TRACE(Js::ReJITPhase, &func))
            {TRACE_IT(2465);
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(
                    _u("Rejit (compile-time): function: %s (%s) reason: %S\n"),
                    workItem->GetJITFunctionBody()->GetDisplayName(),
                    workItem->GetJITTimeInfo()->GetDebugNumberSet(debugStringBuffer),
                    ex.ReasonName());
            }

            rejit = true;
        }
        // Either the entry point has a reference to the number now, or we failed to code gen and we
        // don't need to numbers, we can flush the completed page now.
        //
        // If the number allocator is NULL then we are shutting down the thread context and so too the
        // code generator. The number allocator must be freed before the recycler (and thus before the
        // code generator) so we can't and don't need to flush it.

        // TODO: OOP JIT, allocator cleanup
    } while (rejit);
}

///----------------------------------------------------------------------------
///
/// Func::TryCodegen
///
///     Attempt to Codegen this function.
///
///----------------------------------------------------------------------------
void
Func::TryCodegen()
{
    Assert(!IsJitInDebugMode() || !GetJITFunctionBody()->HasTry());

    BEGIN_CODEGEN_PHASE(this, Js::BackEndPhase);
    {
        // IRBuilder

        BEGIN_CODEGEN_PHASE(this, Js::IRBuilderPhase);

#ifdef ASMJS_PLAT
        if (GetJITFunctionBody()->IsAsmJsMode())
        {TRACE_IT(2466);
            IRBuilderAsmJs asmIrBuilder(this);
            asmIrBuilder.Build();
        }
        else
#endif
        {TRACE_IT(2467);
            IRBuilder irBuilder(this);
            irBuilder.Build();
        }

        END_CODEGEN_PHASE(this, Js::IRBuilderPhase);

#ifdef IR_VIEWER
        IRtoJSObjectBuilder::DumpIRtoGlobalObject(this, Js::IRBuilderPhase);
#endif /* IR_VIEWER */

        BEGIN_CODEGEN_PHASE(this, Js::InlinePhase);

        InliningHeuristics heuristics(GetWorkItem()->GetJITTimeInfo(), this->IsLoopBody());
        Inline inliner(this, heuristics);
        inliner.Optimize();

        END_CODEGEN_PHASE(this, Js::InlinePhase);

        ThrowIfScriptClosed();

        // FlowGraph
        {TRACE_IT(2468);
            // Scope for FlowGraph arena
            NoRecoverMemoryJitArenaAllocator fgAlloc(_u("BE-FlowGraph"), m_alloc->GetPageAllocator(), Js::Throw::OutOfMemory);

            BEGIN_CODEGEN_PHASE(this, Js::FGBuildPhase);

            this->m_fg = FlowGraph::New(this, &fgAlloc);
            this->m_fg->Build();

            END_CODEGEN_PHASE(this, Js::FGBuildPhase);

            // Global Optimization and Type Specialization
            BEGIN_CODEGEN_PHASE(this, Js::GlobOptPhase);

            GlobOpt globOpt(this);
            globOpt.Optimize();

            END_CODEGEN_PHASE(this, Js::GlobOptPhase);

            // Delete flowGraph now
            this->m_fg->Destroy();
            this->m_fg = nullptr;
        }

#ifdef IR_VIEWER
        IRtoJSObjectBuilder::DumpIRtoGlobalObject(this, Js::GlobOptPhase);
#endif /* IR_VIEWER */

        ThrowIfScriptClosed();

        // Lowering
        Lowerer lowerer(this);
        BEGIN_CODEGEN_PHASE(this, Js::LowererPhase);
        lowerer.Lower();
        END_CODEGEN_PHASE(this, Js::LowererPhase);

#ifdef IR_VIEWER
        IRtoJSObjectBuilder::DumpIRtoGlobalObject(this, Js::LowererPhase);
#endif /* IR_VIEWER */

        // Encode constants

        Security security(this);

        BEGIN_CODEGEN_PHASE(this, Js::EncodeConstantsPhase)
        security.EncodeLargeConstants();
        END_CODEGEN_PHASE(this, Js::EncodeConstantsPhase);
        if (GetJITFunctionBody()->DoInterruptProbe())
        {
            BEGIN_CODEGEN_PHASE(this, Js::InterruptProbePhase)
            lowerer.DoInterruptProbes();
            END_CODEGEN_PHASE(this, Js::InterruptProbePhase)
        }

        // Register Allocation

        BEGIN_CODEGEN_PHASE(this, Js::RegAllocPhase);

        LinearScan linearScan(this);
        linearScan.RegAlloc();

        END_CODEGEN_PHASE(this, Js::RegAllocPhase);

#ifdef IR_VIEWER
        IRtoJSObjectBuilder::DumpIRtoGlobalObject(this, Js::RegAllocPhase);
#endif /* IR_VIEWER */

        ThrowIfScriptClosed();

        // Peephole optimizations

        BEGIN_CODEGEN_PHASE(this, Js::PeepsPhase);

        Peeps peeps(this);
        peeps.PeepFunc();

        END_CODEGEN_PHASE(this, Js::PeepsPhase);

        // Layout

        BEGIN_CODEGEN_PHASE(this, Js::LayoutPhase);

        SimpleLayout layout(this);
        layout.Layout();

        END_CODEGEN_PHASE(this, Js::LayoutPhase);

        if (this->HasTry() && this->hasBailoutInEHRegion)
        {
            BEGIN_CODEGEN_PHASE(this, Js::EHBailoutPatchUpPhase);
            lowerer.EHBailoutPatchUp();
            END_CODEGEN_PHASE(this, Js::EHBailoutPatchUpPhase);
        }

        // Insert NOPs (moving this before prolog/epilog for AMD64 and possibly ARM).
        BEGIN_CODEGEN_PHASE(this, Js::InsertNOPsPhase);
        security.InsertNOPs();
        END_CODEGEN_PHASE(this, Js::InsertNOPsPhase);

        // Prolog/Epilog
        BEGIN_CODEGEN_PHASE(this, Js::PrologEpilogPhase);
        if (GetJITFunctionBody()->IsAsmJsMode())
        {TRACE_IT(2469);
            lowerer.LowerPrologEpilogAsmJs();
        }
        else
        {TRACE_IT(2470);
            lowerer.LowerPrologEpilog();
        }
        END_CODEGEN_PHASE(this, Js::PrologEpilogPhase);

        BEGIN_CODEGEN_PHASE(this, Js::FinalLowerPhase);
        lowerer.FinalLower();
        END_CODEGEN_PHASE(this, Js::FinalLowerPhase);

        // Encoder
        BEGIN_CODEGEN_PHASE(this, Js::EncoderPhase);

        Encoder encoder(this);
        encoder.Encode();

        END_CODEGEN_PHASE_NO_DUMP(this, Js::EncoderPhase);

#ifdef IR_VIEWER
        IRtoJSObjectBuilder::DumpIRtoGlobalObject(this, Js::EncoderPhase);
#endif /* IR_VIEWER */

    }

#if DBG_DUMP
    if (Js::Configuration::Global.flags.IsEnabled(Js::AsmDumpModeFlag))
    {TRACE_IT(2471);
        FILE * oldFile = 0;
        FILE * asmFile = GetScriptContext()->GetNativeCodeGenerator()->asmFile;
        if (asmFile)
        {TRACE_IT(2472);
            oldFile = Output::SetFile(asmFile);
        }

        this->Dump(IRDumpFlags_AsmDumpMode);

        Output::Flush();

        if (asmFile)
        {TRACE_IT(2473);
            FILE *openedFile = Output::SetFile(oldFile);
            Assert(openedFile == asmFile);
        }
    }
#endif
    if (this->IsOOPJIT())
    {
        BEGIN_CODEGEN_PHASE(this, Js::NativeCodeDataPhase);

        auto dataAllocator = this->GetNativeCodeDataAllocator();
        if (dataAllocator->allocCount > 0)
        {TRACE_IT(2474);
            NativeCodeData::DataChunk *chunk = (NativeCodeData::DataChunk*)dataAllocator->chunkList;
            NativeCodeData::DataChunk *next1 = chunk;
            while (next1)
            {TRACE_IT(2475);
                if (next1->fixupFunc)
                {TRACE_IT(2476);
                    next1->fixupFunc(next1->data, chunk);
                }
#if DBG
                // Scan memory to see if there's missing pointer needs to be fixed up
                // This can hit false positive if some data field happens to have value 
                // falls into the NativeCodeData memory range.
                NativeCodeData::DataChunk *next2 = chunk;
                while (next2)
                {TRACE_IT(2477);
                    for (unsigned int i = 0; i < next1->len / sizeof(void*); i++)
                    {TRACE_IT(2478);
                        if (((void**)next1->data)[i] == (void*)next2->data)
                        {TRACE_IT(2479);
                            NativeCodeData::VerifyExistFixupEntry((void*)next2->data, &((void**)next1->data)[i], next1->data);
                        }
                    }
                    next2 = next2->next;
                }
#endif
                next1 = next1->next;
            }

            JITOutputIDL* jitOutputData = m_output.GetOutputData();
            size_t allocSize = offsetof(NativeDataFixupTable, fixupRecords) + sizeof(NativeDataFixupRecord)* (dataAllocator->allocCount);
            jitOutputData->nativeDataFixupTable = (NativeDataFixupTable*)midl_user_allocate(allocSize);
            if (!jitOutputData->nativeDataFixupTable)
            {TRACE_IT(2480);
                Js::Throw::OutOfMemory();
            }
            __analysis_assume(jitOutputData->nativeDataFixupTable);
            jitOutputData->nativeDataFixupTable->count = dataAllocator->allocCount;

            jitOutputData->buffer = (NativeDataBuffer*)midl_user_allocate(offsetof(NativeDataBuffer, data) + dataAllocator->totalSize);
            if (!jitOutputData->buffer)
            {TRACE_IT(2481);
                Js::Throw::OutOfMemory();
            }
            __analysis_assume(jitOutputData->buffer);

            jitOutputData->buffer->len = dataAllocator->totalSize;

            unsigned int len = 0;
            unsigned int count = 0;
            next1 = chunk;
            while (next1)
            {TRACE_IT(2482);
                memcpy(jitOutputData->buffer->data + len, next1->data, next1->len);
                len += next1->len;

                jitOutputData->nativeDataFixupTable->fixupRecords[count].index = next1->allocIndex;
                jitOutputData->nativeDataFixupTable->fixupRecords[count].length = next1->len;
                jitOutputData->nativeDataFixupTable->fixupRecords[count].startOffset = next1->offset;
                jitOutputData->nativeDataFixupTable->fixupRecords[count].updateList = next1->fixupList;

                count++;
                next1 = next1->next;
            }

#if DBG
            if (PHASE_TRACE1(Js::NativeCodeDataPhase))
            {TRACE_IT(2483);
                Output::Print(_u("NativeCodeData Server Buffer: %p, len: %x, chunk head: %p\n"), jitOutputData->buffer->data, jitOutputData->buffer->len, chunk);
            }
#endif
        }
        END_CODEGEN_PHASE(this, Js::NativeCodeDataPhase);
    }

    END_CODEGEN_PHASE(this, Js::BackEndPhase);
}

///----------------------------------------------------------------------------
/// Func::StackAllocate
///     Allocate stack space of given size.
///----------------------------------------------------------------------------
int32
Func::StackAllocate(int size)
{TRACE_IT(2484);
    Assert(this->IsTopFunc());

    int32 offset;

#ifdef MD_GROW_LOCALS_AREA_UP
    // Locals have positive offsets and are allocated from bottom to top.
    m_localStackHeight = Math::Align(m_localStackHeight, min(size, MachStackAlignment));

    offset = m_localStackHeight;
    m_localStackHeight += size;
#else
    // Locals have negative offsets and are allocated from top to bottom.
    m_localStackHeight += size;
    m_localStackHeight = Math::Align(m_localStackHeight, min(size, MachStackAlignment));

    offset = -m_localStackHeight;
#endif

    return offset;
}

///----------------------------------------------------------------------------
///
/// Func::StackAllocate
///
///     Allocate stack space for this symbol.
///
///----------------------------------------------------------------------------

int32
Func::StackAllocate(StackSym *stackSym, int size)
{TRACE_IT(2485);
    Assert(size > 0);
    if (stackSym->IsArgSlotSym() || stackSym->IsParamSlotSym() || stackSym->IsAllocated())
    {TRACE_IT(2486);
        return stackSym->m_offset;
    }
    Assert(stackSym->m_offset == 0);
    stackSym->m_allocated = true;
    stackSym->m_offset = StackAllocate(size);

    return stackSym->m_offset;
}

void
Func::SetArgOffset(StackSym *stackSym, int32 offset)
{TRACE_IT(2487);
    AssertMsg(offset >= 0, "Why is the offset, negative?");
    stackSym->m_offset = offset;
    stackSym->m_allocated = true;
}

///
/// Ensures that local var slots are created, if the function has locals.
///     Allocate stack space for locals used for debugging
///     (for local non-temp vars we write-through memory so that locals inspection can make use of that.).
//      On stack, after local slots we allocate space for metadata (in particular, whether any the locals was changed in debugger).
///
void
Func::EnsureLocalVarSlots()
{TRACE_IT(2488);
    Assert(IsJitInDebugMode());

    if (!this->HasLocalVarSlotCreated())
    {TRACE_IT(2489);
        uint32 localSlotCount = GetJITFunctionBody()->GetNonTempLocalVarCount();
        if (localSlotCount && m_localVarSlotsOffset == Js::Constants::InvalidOffset)
        {TRACE_IT(2490);
            // Allocate the slots.
            int32 size = localSlotCount * GetDiagLocalSlotSize();
            m_localVarSlotsOffset = StackAllocate(size);
            m_hasLocalVarChangedOffset = StackAllocate(max(1, MachStackAlignment)); // Can't alloc less than StackAlignment bytes.

            Assert(m_workItem->Type() == JsFunctionType);

            m_output.SetVarSlotsOffset(AdjustOffsetValue(m_localVarSlotsOffset));
            m_output.SetVarChangedOffset(AdjustOffsetValue(m_hasLocalVarChangedOffset));
        }
    }
}

void Func::SetFirstArgOffset(IR::Instr* inlineeStart)
{TRACE_IT(2491);
    Assert(inlineeStart->m_func == this);
    Assert(!IsTopFunc());
    int32 lastOffset;

    IR::Instr* arg = inlineeStart->GetNextArg();
    const auto lastArgOutStackSym = arg->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
    lastOffset = lastArgOutStackSym->m_offset;
    Assert(lastArgOutStackSym->m_isSingleDef);
    const auto secondLastArgOutOpnd = lastArgOutStackSym->m_instrDef->GetSrc2();
    if (secondLastArgOutOpnd->IsSymOpnd())
    {TRACE_IT(2492);
        const auto secondLastOffset = secondLastArgOutOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_offset;
        if (secondLastOffset > lastOffset)
        {TRACE_IT(2493);
            lastOffset = secondLastOffset;
        }
    }
    lastOffset += MachPtr;
    int32 firstActualStackOffset = lastOffset - ((this->actualCount + Js::Constants::InlineeMetaArgCount) * MachPtr);
    Assert((this->firstActualStackOffset == -1) || (this->firstActualStackOffset == firstActualStackOffset));
    this->firstActualStackOffset = firstActualStackOffset;
}

int32
Func::GetLocalVarSlotOffset(int32 slotId)
{TRACE_IT(2494);
    this->EnsureLocalVarSlots();
    Assert(m_localVarSlotsOffset != Js::Constants::InvalidOffset);

    int32 slotOffset = slotId * GetDiagLocalSlotSize();

    return m_localVarSlotsOffset + slotOffset;
}

void Func::OnAddSym(Sym* sym)
{TRACE_IT(2495);
    Assert(sym);
    if (this->IsJitInDebugMode() && this->IsNonTempLocalVar(sym->m_id))
    {TRACE_IT(2496);
        Assert(m_nonTempLocalVars);
        m_nonTempLocalVars->Set(sym->m_id);
    }
}

///
/// Returns offset of the flag (1 byte) whether any local was changed (in debugger).
/// If the function does not have any locals, returns -1.
///
int32
Func::GetHasLocalVarChangedOffset()
{TRACE_IT(2497);
    this->EnsureLocalVarSlots();
    return m_hasLocalVarChangedOffset;
}

bool
Func::IsJitInDebugMode()
{TRACE_IT(2498);
    return m_workItem->IsJitInDebugMode();
}

bool
Func::IsNonTempLocalVar(uint32 slotIndex)
{TRACE_IT(2499);
    return GetJITFunctionBody()->IsNonTempLocalVar(slotIndex);
}

int32
Func::AdjustOffsetValue(int32 offset)
{TRACE_IT(2500);
#ifdef MD_GROW_LOCALS_AREA_UP
        return -(offset + BailOutInfo::StackSymBias);
#else
        // Stack offset are negative, includes the PUSH EBP and return address
        return offset - (2 * MachPtr);
#endif
}

#ifdef MD_GROW_LOCALS_AREA_UP
// Note: this is called during jit-compile when we finalize bail out record.
void
Func::AjustLocalVarSlotOffset()
{TRACE_IT(2501);
    if (GetJITFunctionBody()->GetNonTempLocalVarCount())
    {TRACE_IT(2502);
        // Turn positive SP-relative base locals offset into negative frame-pointer-relative offset
        // This is changing value for restoring the locals when read due to locals inspection.

        int localsOffset = m_localVarSlotsOffset - (m_localStackHeight + m_ArgumentsOffset);
        int valueChangeOffset = m_hasLocalVarChangedOffset - (m_localStackHeight + m_ArgumentsOffset);

        m_output.SetVarSlotsOffset(localsOffset);
        m_output.SetVarChangedOffset(valueChangeOffset);
    }
}
#endif

bool
Func::DoGlobOptsForGeneratorFunc() const
{TRACE_IT(2503);
    // Disable GlobOpt optimizations for generators initially. Will visit and enable each one by one.
    return !GetJITFunctionBody()->IsCoroutine();
}

bool
Func::DoSimpleJitDynamicProfile() const
{TRACE_IT(2504);
    return IsSimpleJit() && !PHASE_OFF(Js::SimpleJitDynamicProfilePhase, GetTopFunc()) && !CONFIG_FLAG(NewSimpleJit);
}

void
Func::SetDoFastPaths()
{TRACE_IT(2505);
    // Make sure we only call this once!
    Assert(!this->hasCalledSetDoFastPaths);

    bool doFastPaths = false;

    if(!PHASE_OFF(Js::FastPathPhase, this) && (!IsSimpleJit() || CONFIG_FLAG(NewSimpleJit)))
    {TRACE_IT(2506);
        doFastPaths = true;
    }

    this->m_doFastPaths = doFastPaths;
#ifdef DBG
    this->hasCalledSetDoFastPaths = true;
#endif
}

#ifdef _M_ARM

RegNum
Func::GetLocalsPointer() const
{TRACE_IT(2507);
#ifdef DBG
    if (Js::Configuration::Global.flags.IsEnabled(Js::ForceLocalsPtrFlag))
    {TRACE_IT(2508);
        return ALT_LOCALS_PTR;
    }
#endif

    if (GetJITFunctionBody()->HasTry())
    {TRACE_IT(2509);
        return ALT_LOCALS_PTR;
    }

    return RegSP;
}

#endif

void Func::AddSlotArrayCheck(IR::SymOpnd *fieldOpnd)
{TRACE_IT(2510);
    if (PHASE_OFF(Js::ClosureRangeCheckPhase, this))
    {TRACE_IT(2511);
        return;
    }

    Assert(IsTopFunc());
    if (this->slotArrayCheckTable == nullptr)
    {TRACE_IT(2512);
        this->slotArrayCheckTable = SlotArrayCheckTable::New(m_alloc, 4);
    }

    PropertySym *propertySym = fieldOpnd->m_sym->AsPropertySym();
    uint32 slot = propertySym->m_propertyId;
    uint32 *pSlotId = this->slotArrayCheckTable->FindOrInsert(slot, propertySym->m_stackSym->m_id);

    if (pSlotId && (*pSlotId == (uint32)-1 || *pSlotId < slot))
    {TRACE_IT(2513);
        *pSlotId = propertySym->m_propertyId;
    }
}

void Func::AddFrameDisplayCheck(IR::SymOpnd *fieldOpnd, uint32 slotId)
{TRACE_IT(2514);
    if (PHASE_OFF(Js::ClosureRangeCheckPhase, this))
    {TRACE_IT(2515);
        return;
    }

    Assert(IsTopFunc());
    if (this->frameDisplayCheckTable == nullptr)
    {TRACE_IT(2516);
        this->frameDisplayCheckTable = FrameDisplayCheckTable::New(m_alloc, 4);
    }

    PropertySym *propertySym = fieldOpnd->m_sym->AsPropertySym();
    FrameDisplayCheckRecord **record = this->frameDisplayCheckTable->FindOrInsertNew(propertySym->m_stackSym->m_id);
    if (*record == nullptr)
    {TRACE_IT(2517);
        *record = JitAnew(m_alloc, FrameDisplayCheckRecord);
    }

    uint32 frameDisplaySlot = propertySym->m_propertyId;
    if ((*record)->table == nullptr || (*record)->slotId < frameDisplaySlot)
    {TRACE_IT(2518);
        (*record)->slotId = frameDisplaySlot;
    }

    if (slotId != (uint32)-1)
    {TRACE_IT(2519);
        if ((*record)->table == nullptr)
        {TRACE_IT(2520);
            (*record)->table = SlotArrayCheckTable::New(m_alloc, 4);
        }
        uint32 *pSlotId = (*record)->table->FindOrInsert(slotId, frameDisplaySlot);
        if (pSlotId && *pSlotId < slotId)
        {TRACE_IT(2521);
            *pSlotId = slotId;
        }
    }
}

void Func::InitLocalClosureSyms()
{TRACE_IT(2522);
    Assert(this->m_localClosureSym == nullptr);

    // Allocate stack space for closure pointers. Do this only if we're jitting for stack closures, and
    // tell bailout that these are not byte code symbols so that we don't try to encode them in the bailout record,
    // as they don't have normal lifetimes.
    Js::RegSlot regSlot = GetJITFunctionBody()->GetLocalClosureReg();
    if (regSlot != Js::Constants::NoRegister)
    {TRACE_IT(2523);
        this->m_localClosureSym =
            StackSym::FindOrCreate(static_cast<SymID>(regSlot),
                                   this->DoStackFrameDisplay() ? (Js::RegSlot)-1 : regSlot,
                                   this);
    }

    regSlot = this->GetJITFunctionBody()->GetParamClosureReg();
    if (regSlot != Js::Constants::NoRegister)
    {TRACE_IT(2524);
        Assert(this->GetParamClosureSym() == nullptr && !this->GetJITFunctionBody()->IsParamAndBodyScopeMerged());
        this->m_paramClosureSym =
            StackSym::FindOrCreate(static_cast<SymID>(regSlot),
                this->DoStackFrameDisplay() ? (Js::RegSlot) - 1 : regSlot,
                this);
    }

    regSlot = GetJITFunctionBody()->GetLocalFrameDisplayReg();
    if (regSlot != Js::Constants::NoRegister)
    {TRACE_IT(2525);
        this->m_localFrameDisplaySym =
            StackSym::FindOrCreate(static_cast<SymID>(regSlot),
                                   this->DoStackFrameDisplay() ? (Js::RegSlot)-1 : regSlot,
                                   this);
    }
}

bool Func::CanAllocInPreReservedHeapPageSegment ()
{TRACE_IT(2526);
#ifdef _CONTROL_FLOW_GUARD
    return PHASE_FORCE1(Js::PreReservedHeapAllocPhase) || (!PHASE_OFF1(Js::PreReservedHeapAllocPhase) &&
        !IsJitInDebugMode() && GetThreadContextInfo()->IsCFGEnabled()
        //&& !GetScriptContext()->IsScriptContextInDebugMode()
#if _M_IX86
        && m_workItem->GetJitMode() == ExecutionMode::FullJit

#if ENABLE_OOP_NATIVE_CODEGEN
        && (JITManager::GetJITManager()->IsJITServer()
            ? GetOOPCodeGenAllocators()->canCreatePreReservedSegment
            : GetInProcCodeGenAllocators()->canCreatePreReservedSegment)
#else
        && GetInProcCodeGenAllocators()->canCreatePreReservedSegment
#endif
        );
#elif _M_X64
        && true);
#else
        && false); //Not yet implemented for architectures other than x86 and amd64.
#endif  //_M_ARCH
#else
    return false;
#endif//_CONTROL_FLOW_GUARD
}

///----------------------------------------------------------------------------
///
/// Func::GetInstrCount
///
///     Returns the number of instrs.
///     Note: It counts all instrs for now, including labels, etc.
///
///----------------------------------------------------------------------------
uint32
Func::GetInstrCount()
{TRACE_IT(2527);
    uint instrCount = 0;

    FOREACH_INSTR_IN_FUNC(instr, this)
    {TRACE_IT(2528);
        instrCount++;
    }NEXT_INSTR_IN_FUNC;

    return instrCount;
}

///----------------------------------------------------------------------------
///
/// Func::NumberInstrs
///
///     Number each instruction in order of appearance in the function.
///
///----------------------------------------------------------------------------
void
Func::NumberInstrs()
{TRACE_IT(2529);
#if DBG_DUMP
    Assert(this->IsTopFunc());
    Assert(!this->hasInstrNumber);
    this->hasInstrNumber = true;
#endif
    uint instrCount = 1;

    FOREACH_INSTR_IN_FUNC(instr, this)
    {TRACE_IT(2530);
        instr->SetNumber(instrCount++);
    }
    NEXT_INSTR_IN_FUNC;
}

///----------------------------------------------------------------------------
///
/// Func::IsInPhase
///
/// Determines whether the function is currently in the provided phase
///
///----------------------------------------------------------------------------
#if DBG
bool
Func::IsInPhase(Js::Phase tag)
{TRACE_IT(2531);
    return this->GetTopFunc()->currentPhases.Contains(tag);
}
#endif

///----------------------------------------------------------------------------
///
/// Func::BeginPhase
///
/// Takes care of the profiler
///
///----------------------------------------------------------------------------
void
Func::BeginPhase(Js::Phase tag)
{TRACE_IT(2532);
#ifdef DBG
    this->GetTopFunc()->currentPhases.Push(tag);
#endif

#ifdef PROFILE_EXEC
    AssertMsg((this->m_codeGenProfiler != nullptr) == Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag),
        "Profiler tag is supplied but the profiler pointer is NULL");
    if (this->m_codeGenProfiler)
    {TRACE_IT(2533);
        this->m_codeGenProfiler->ProfileBegin(tag);
    }
#endif
}

///----------------------------------------------------------------------------
///
/// Func::EndPhase
///
/// Takes care of the profiler and dumper
///
///----------------------------------------------------------------------------
void
Func::EndProfiler(Js::Phase tag)
{TRACE_IT(2534);
#ifdef DBG
    Assert(this->GetTopFunc()->currentPhases.Count() > 0);
    Js::Phase popped = this->GetTopFunc()->currentPhases.Pop();
    Assert(tag == popped);
#endif

#ifdef PROFILE_EXEC
    AssertMsg((this->m_codeGenProfiler != nullptr) == Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag),
        "Profiler tag is supplied but the profiler pointer is NULL");
    if (this->m_codeGenProfiler)
    {TRACE_IT(2535);
        this->m_codeGenProfiler->ProfileEnd(tag);
    }
#endif
}

void
Func::EndPhase(Js::Phase tag, bool dump)
{TRACE_IT(2536);
    this->EndProfiler(tag);
#if DBG_DUMP
    if(dump && (PHASE_DUMP(tag, this)
        || PHASE_DUMP(Js::BackEndPhase, this)))
    {TRACE_IT(2537);
        Output::Print(_u("-----------------------------------------------------------------------------\n"));

        if (IsLoopBody())
        {TRACE_IT(2538);
            Output::Print(_u("************   IR after %s (%S) Loop %d ************\n"),
                Js::PhaseNames[tag],
                ExecutionModeName(m_workItem->GetJitMode()),
                m_workItem->GetLoopNumber());
        }
        else
        {TRACE_IT(2539);
            Output::Print(_u("************   IR after %s (%S)  ************\n"),
                Js::PhaseNames[tag],
                ExecutionModeName(m_workItem->GetJitMode()));
        }
        this->Dump(Js::Configuration::Global.flags.AsmDiff? IRDumpFlags_AsmDumpMode : IRDumpFlags_None);
    }
#endif

#if DBG
    if (tag == Js::LowererPhase)
    {TRACE_IT(2540);
        Assert(!this->isPostLower);
        this->isPostLower = true;
    }
    else if (tag == Js::RegAllocPhase)
    {TRACE_IT(2541);
        Assert(!this->isPostRegAlloc);
        this->isPostRegAlloc = true;
    }
    else if (tag == Js::PeepsPhase)
    {TRACE_IT(2542);
        Assert(this->isPostLower && !this->isPostLayout);
        this->isPostPeeps = true;
    }
    else if (tag == Js::LayoutPhase)
    {TRACE_IT(2543);
        Assert(this->isPostPeeps && !this->isPostLayout);
        this->isPostLayout = true;
    }
    else if (tag == Js::FinalLowerPhase)
    {TRACE_IT(2544);
        Assert(this->isPostLayout && !this->isPostFinalLower);
        this->isPostFinalLower = true;
    }
    if (this->isPostLower)
    {TRACE_IT(2545);
#ifndef _M_ARM    // Need to verify ARM is clean.
        DbCheckPostLower dbCheck(this);

        dbCheck.Check();
#endif
    }
    this->m_alloc->MergeDelayFreeList();
#endif
}

Func const *
Func::GetTopFunc() const
{TRACE_IT(2546);
    Func const * func = this;
    while (!func->IsTopFunc())
    {TRACE_IT(2547);
        func = func->parentFunc;
    }
    return func;
}

Func *
Func::GetTopFunc()
{TRACE_IT(2548);
    Func * func = this;
    while (!func->IsTopFunc())
    {TRACE_IT(2549);
        func = func->parentFunc;
    }
    return func;
}

StackSym *
Func::EnsureLoopParamSym()
{TRACE_IT(2550);
    if (this->m_loopParamSym == nullptr)
    {TRACE_IT(2551);
        this->m_loopParamSym = StackSym::New(TyMachPtr, this);
    }
    return this->m_loopParamSym;
}

void
Func::UpdateMaxInlineeArgOutCount(uint inlineeArgOutCount)
{TRACE_IT(2552);
    if (maxInlineeArgOutCount < inlineeArgOutCount)
    {TRACE_IT(2553);
        maxInlineeArgOutCount = inlineeArgOutCount;
    }
}

void
Func::BeginClone(Lowerer * lowerer, JitArenaAllocator *alloc)
{TRACE_IT(2554);
    Assert(this->IsTopFunc());
    AssertMsg(m_cloner == nullptr, "Starting new clone while one is in progress");
    m_cloner = JitAnew(alloc, Cloner, lowerer, alloc);
    if (m_cloneMap == nullptr)
    {TRACE_IT(2555);
         m_cloneMap = JitAnew(alloc, InstrMap, alloc, 7);
    }
}

void
Func::EndClone()
{TRACE_IT(2556);
    Assert(this->IsTopFunc());
    if (m_cloner)
    {TRACE_IT(2557);
        m_cloner->Finish();
        JitAdelete(m_cloner->alloc, m_cloner);
        m_cloner = nullptr;
    }
}

IR::SymOpnd *
Func::GetInlineeOpndAtOffset(int32 offset)
{TRACE_IT(2558);
    Assert(IsInlinee());

    StackSym *stackSym = CreateInlineeStackSym();
    this->SetArgOffset(stackSym, stackSym->m_offset + offset);
    Assert(stackSym->m_offset >= 0);

    return IR::SymOpnd::New(stackSym, 0, TyMachReg, this);
}

StackSym *
Func::CreateInlineeStackSym()
{TRACE_IT(2559);
    // Make sure this is an inlinee and that GlobOpt has initialized the offset
    // in the inlinee's frame.
    Assert(IsInlinee());
    Assert(m_inlineeFrameStartSym->m_offset != -1);

    StackSym *stackSym = m_symTable->GetArgSlotSym((Js::ArgSlot)-1);
    stackSym->m_isInlinedArgSlot = true;
    stackSym->m_offset = m_inlineeFrameStartSym->m_offset;
    stackSym->m_allocated = true;

    return stackSym;
}

uint16
Func::GetArgUsedForBranch() const
{TRACE_IT(2560);
    // this value can change while JITing, so or these together
    return GetJITFunctionBody()->GetArgUsedForBranch() | GetJITOutput()->GetArgUsedForBranch();
}

intptr_t
Func::GetJittedLoopIterationsSinceLastBailoutAddress() const
{TRACE_IT(2561);
    Assert(this->m_workItem->Type() == JsLoopBodyWorkItemType);

    return m_workItem->GetJittedLoopIterationsSinceLastBailoutAddr();
}

intptr_t
Func::GetWeakFuncRef() const
{TRACE_IT(2562);
    // TODO: OOP JIT figure out if this can be null

    return m_workItem->GetJITTimeInfo()->GetWeakFuncRef();
}

intptr_t
Func::GetRuntimeInlineCache(const uint index) const
{TRACE_IT(2563);
    if(m_runtimeInfo != nullptr && m_runtimeInfo->HasClonedInlineCaches())
    {TRACE_IT(2564);
        intptr_t inlineCache = m_runtimeInfo->GetClonedInlineCache(index);
        if(inlineCache)
        {TRACE_IT(2565);
            return inlineCache;
        }
    }

    return GetJITFunctionBody()->GetInlineCache(index);
}

JITTimePolymorphicInlineCache *
Func::GetRuntimePolymorphicInlineCache(const uint index) const
{TRACE_IT(2566);
    if (this->m_polymorphicInlineCacheInfo && this->m_polymorphicInlineCacheInfo->HasInlineCaches())
    {TRACE_IT(2567);
        return this->m_polymorphicInlineCacheInfo->GetInlineCache(index);
    }
    return nullptr;
}

byte
Func::GetPolyCacheUtilToInitialize(const uint index) const
{TRACE_IT(2568);
    return this->GetRuntimePolymorphicInlineCache(index) ? this->GetPolyCacheUtil(index) : PolymorphicInlineCacheUtilizationMinValue;
}

byte
Func::GetPolyCacheUtil(const uint index) const
{TRACE_IT(2569);
    return this->m_polymorphicInlineCacheInfo->GetUtil(index);
}

JITObjTypeSpecFldInfo*
Func::GetObjTypeSpecFldInfo(const uint index) const
{TRACE_IT(2570);
    if (GetJITFunctionBody()->GetInlineCacheCount() == 0)
    {TRACE_IT(2571);
        Assert(UNREACHED);
        return nullptr;
    }

    return GetWorkItem()->GetJITTimeInfo()->GetObjTypeSpecFldInfo(index);
}

JITObjTypeSpecFldInfo*
Func::GetGlobalObjTypeSpecFldInfo(uint propertyInfoId) const
{TRACE_IT(2572);
    Assert(propertyInfoId < GetTopFunc()->GetWorkItem()->GetJITTimeInfo()->GetGlobalObjTypeSpecFldInfoCount());
    return GetTopFunc()->m_globalObjTypeSpecFldInfoArray[propertyInfoId];
}

void
Func::EnsurePinnedTypeRefs()
{TRACE_IT(2573);
    if (this->pinnedTypeRefs == nullptr)
    {TRACE_IT(2574);
        this->pinnedTypeRefs = JitAnew(this->m_alloc, TypeRefSet, this->m_alloc);
    }
}

void
Func::PinTypeRef(void* typeRef)
{TRACE_IT(2575);
    EnsurePinnedTypeRefs();
    this->pinnedTypeRefs->AddNew(typeRef);
}

void
Func::EnsureSingleTypeGuards()
{TRACE_IT(2576);
    if (this->singleTypeGuards == nullptr)
    {TRACE_IT(2577);
        this->singleTypeGuards = JitAnew(this->m_alloc, TypePropertyGuardDictionary, this->m_alloc);
    }
}

Js::JitTypePropertyGuard*
Func::GetOrCreateSingleTypeGuard(intptr_t typeAddr)
{TRACE_IT(2578);
    EnsureSingleTypeGuards();

    Js::JitTypePropertyGuard* guard;
    if (!this->singleTypeGuards->TryGetValue(typeAddr, &guard))
    {TRACE_IT(2579);
        // Property guards are allocated by NativeCodeData::Allocator so that their lifetime extends as long as the EntryPointInfo is alive.
        guard = NativeCodeDataNewNoFixup(GetNativeCodeDataAllocator(), Js::JitTypePropertyGuard, typeAddr, this->indexedPropertyGuardCount++);
        this->singleTypeGuards->Add(typeAddr, guard);
    }
    else
    {TRACE_IT(2580);
        Assert(guard->GetTypeAddr() == typeAddr);
    }

    return guard;
}

void
Func::EnsureEquivalentTypeGuards()
{TRACE_IT(2581);
    if (this->equivalentTypeGuards == nullptr)
    {TRACE_IT(2582);
        this->equivalentTypeGuards = JitAnew(this->m_alloc, EquivalentTypeGuardList, this->m_alloc);
    }
}

Js::JitEquivalentTypeGuard*
Func::CreateEquivalentTypeGuard(JITTypeHolder type, uint32 objTypeSpecFldId)
{TRACE_IT(2583);
    EnsureEquivalentTypeGuards();

    Js::JitEquivalentTypeGuard* guard = NativeCodeDataNewNoFixup(GetNativeCodeDataAllocator(), Js::JitEquivalentTypeGuard, type->GetAddr(), this->indexedPropertyGuardCount++, objTypeSpecFldId);

    // If we want to hard code the address of the cache, we will need to go back to allocating it from the native code data allocator.
    // We would then need to maintain consistency (double write) to both the recycler allocated cache and the one on the heap.
    Js::EquivalentTypeCache* cache = nullptr;
    if (this->IsOOPJIT())
    {TRACE_IT(2584);
        cache = JitAnewZ(this->m_alloc, Js::EquivalentTypeCache);
    }
    else
    {TRACE_IT(2585);
        cache = NativeCodeDataNewZNoFixup(GetTransferDataAllocator(), Js::EquivalentTypeCache);
    }
    guard->SetCache(cache);

    // Give the cache a back-pointer to the guard so that the guard can be cleared at runtime if necessary.
    cache->SetGuard(guard);
    this->equivalentTypeGuards->Prepend(guard);

    return guard;
}

void
Func::EnsurePropertyGuardsByPropertyId()
{TRACE_IT(2586);
    if (this->propertyGuardsByPropertyId == nullptr)
    {TRACE_IT(2587);
        this->propertyGuardsByPropertyId = JitAnew(this->m_alloc, PropertyGuardByPropertyIdMap, this->m_alloc);
    }
}

void
Func::EnsureCtorCachesByPropertyId()
{TRACE_IT(2588);
    if (this->ctorCachesByPropertyId == nullptr)
    {TRACE_IT(2589);
        this->ctorCachesByPropertyId = JitAnew(this->m_alloc, CtorCachesByPropertyIdMap, this->m_alloc);
    }
}

void
Func::LinkGuardToPropertyId(Js::PropertyId propertyId, Js::JitIndexedPropertyGuard* guard)
{TRACE_IT(2590);
    Assert(guard != nullptr);
    Assert(guard->GetValue() != NULL);

    Assert(this->propertyGuardsByPropertyId != nullptr);

    IndexedPropertyGuardSet* set;
    if (!this->propertyGuardsByPropertyId->TryGetValue(propertyId, &set))
    {TRACE_IT(2591);
        set = JitAnew(this->m_alloc, IndexedPropertyGuardSet, this->m_alloc);
        this->propertyGuardsByPropertyId->Add(propertyId, set);
    }

    set->Item(guard);
}

void
Func::LinkCtorCacheToPropertyId(Js::PropertyId propertyId, JITTimeConstructorCache* cache)
{TRACE_IT(2592);
    Assert(cache != nullptr);
    Assert(this->ctorCachesByPropertyId != nullptr);

    CtorCacheSet* set;
    if (!this->ctorCachesByPropertyId->TryGetValue(propertyId, &set))
    {TRACE_IT(2593);
        set = JitAnew(this->m_alloc, CtorCacheSet, this->m_alloc);
        this->ctorCachesByPropertyId->Add(propertyId, set);
    }

    set->Item(cache->GetRuntimeCacheAddr());
}

JITTimeConstructorCache* Func::GetConstructorCache(const Js::ProfileId profiledCallSiteId)
{TRACE_IT(2594);
    Assert(profiledCallSiteId < GetJITFunctionBody()->GetProfiledCallSiteCount());
    Assert(this->constructorCaches != nullptr);
    return this->constructorCaches[profiledCallSiteId];
}

void Func::SetConstructorCache(const Js::ProfileId profiledCallSiteId, JITTimeConstructorCache* constructorCache)
{TRACE_IT(2595);
    Assert(profiledCallSiteId < GetJITFunctionBody()->GetProfiledCallSiteCount());
    Assert(constructorCache != nullptr);
    Assert(this->constructorCaches != nullptr);
    Assert(this->constructorCaches[profiledCallSiteId] == nullptr);
    this->constructorCacheCount++;
    this->constructorCaches[profiledCallSiteId] = constructorCache;
}

void Func::EnsurePropertiesWrittenTo()
{TRACE_IT(2596);
    if (this->propertiesWrittenTo == nullptr)
    {TRACE_IT(2597);
        this->propertiesWrittenTo = JitAnew(this->m_alloc, PropertyIdSet, this->m_alloc);
    }
}

void Func::EnsureCallSiteToArgumentsOffsetFixupMap()
{TRACE_IT(2598);
    if (this->callSiteToArgumentsOffsetFixupMap == nullptr)
    {TRACE_IT(2599);
        this->callSiteToArgumentsOffsetFixupMap = JitAnew(this->m_alloc, CallSiteToArgumentsOffsetFixupMap, this->m_alloc);
    }
}

IR::LabelInstr *
Func::GetFuncStartLabel()
{TRACE_IT(2600);
    return m_funcStartLabel;
}

IR::LabelInstr *
Func::EnsureFuncStartLabel()
{TRACE_IT(2601);
    if(m_funcStartLabel == nullptr)
    {TRACE_IT(2602);
        m_funcStartLabel = IR::LabelInstr::New( Js::OpCode::Label, this );
    }
    return m_funcStartLabel;
}

IR::LabelInstr *
Func::GetFuncEndLabel()
{TRACE_IT(2603);
    return m_funcEndLabel;
}

IR::LabelInstr *
Func::EnsureFuncEndLabel()
{TRACE_IT(2604);
    if(m_funcEndLabel == nullptr)
    {TRACE_IT(2605);
        m_funcEndLabel = IR::LabelInstr::New( Js::OpCode::Label, this );
    }
    return m_funcEndLabel;
}

void
Func::EnsureStackArgWithFormalsTracker()
{TRACE_IT(2606);
    if (stackArgWithFormalsTracker == nullptr)
    {TRACE_IT(2607);
        stackArgWithFormalsTracker = JitAnew(m_alloc, StackArgWithFormalsTracker, m_alloc);
    }
}

BOOL
Func::IsFormalsArraySym(SymID symId)
{TRACE_IT(2608);
    if (stackArgWithFormalsTracker == nullptr || stackArgWithFormalsTracker->GetFormalsArraySyms() == nullptr)
    {TRACE_IT(2609);
        return false;
    }
    return stackArgWithFormalsTracker->GetFormalsArraySyms()->Test(symId);
}

void
Func::TrackFormalsArraySym(SymID symId)
{TRACE_IT(2610);
    EnsureStackArgWithFormalsTracker();
    stackArgWithFormalsTracker->SetFormalsArraySyms(symId);
}

void
Func::TrackStackSymForFormalIndex(Js::ArgSlot formalsIndex, StackSym * sym)
{TRACE_IT(2611);
    EnsureStackArgWithFormalsTracker();
    Js::ArgSlot formalsCount = GetJITFunctionBody()->GetInParamsCount() - 1;
    stackArgWithFormalsTracker->SetStackSymInFormalsIndexMap(sym, formalsIndex, formalsCount);
}

StackSym *
Func::GetStackSymForFormal(Js::ArgSlot formalsIndex)
{TRACE_IT(2612);
    if (stackArgWithFormalsTracker == nullptr || stackArgWithFormalsTracker->GetFormalsIndexToStackSymMap() == nullptr)
    {TRACE_IT(2613);
        return nullptr;
    }

    Js::ArgSlot formalsCount = GetJITFunctionBody()->GetInParamsCount() - 1;
    StackSym ** formalsIndexToStackSymMap = stackArgWithFormalsTracker->GetFormalsIndexToStackSymMap();
    AssertMsg(formalsIndex < formalsCount, "OutOfRange ? ");
    return formalsIndexToStackSymMap[formalsIndex];
}

bool
Func::HasStackSymForFormal(Js::ArgSlot formalsIndex)
{TRACE_IT(2614);
    if (stackArgWithFormalsTracker == nullptr || stackArgWithFormalsTracker->GetFormalsIndexToStackSymMap() == nullptr)
    {TRACE_IT(2615);
        return false;
    }
    return GetStackSymForFormal(formalsIndex) != nullptr;
}

void
Func::SetScopeObjSym(StackSym * sym)
{TRACE_IT(2616);
    EnsureStackArgWithFormalsTracker();
    stackArgWithFormalsTracker->SetScopeObjSym(sym);
}

StackSym *
Func::GetNativeCodeDataSym() const
{TRACE_IT(2617);
    Assert(IsOOPJIT());
    return m_nativeCodeDataSym;
}

void
Func::SetNativeCodeDataSym(StackSym * opnd)
{TRACE_IT(2618);
    Assert(IsOOPJIT());
    m_nativeCodeDataSym = opnd;
}

StackSym*
Func::GetScopeObjSym()
{TRACE_IT(2619);
    if (stackArgWithFormalsTracker == nullptr)
    {TRACE_IT(2620);
        return nullptr;
    }
    return stackArgWithFormalsTracker->GetScopeObjSym();
}

BVSparse<JitArenaAllocator> *
StackArgWithFormalsTracker::GetFormalsArraySyms()
{TRACE_IT(2621);
    return formalsArraySyms;
}

void
StackArgWithFormalsTracker::SetFormalsArraySyms(SymID symId)
{TRACE_IT(2622);
    if (formalsArraySyms == nullptr)
    {TRACE_IT(2623);
        formalsArraySyms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    }
    formalsArraySyms->Set(symId);
}

StackSym **
StackArgWithFormalsTracker::GetFormalsIndexToStackSymMap()
{TRACE_IT(2624);
    return formalsIndexToStackSymMap;
}

void
StackArgWithFormalsTracker::SetStackSymInFormalsIndexMap(StackSym * sym, Js::ArgSlot formalsIndex, Js::ArgSlot formalsCount)
{TRACE_IT(2625);
    if(formalsIndexToStackSymMap == nullptr)
    {TRACE_IT(2626);
        formalsIndexToStackSymMap = JitAnewArrayZ(alloc, StackSym*, formalsCount);
    }
    AssertMsg(formalsIndex < formalsCount, "Out of range ?");
    formalsIndexToStackSymMap[formalsIndex] = sym;
}

void
StackArgWithFormalsTracker::SetScopeObjSym(StackSym * sym)
{TRACE_IT(2627);
    m_scopeObjSym = sym;
}

StackSym *
StackArgWithFormalsTracker::GetScopeObjSym()
{TRACE_IT(2628);
    return m_scopeObjSym;
}


void
Cloner::AddInstr(IR::Instr * instrOrig, IR::Instr * instrClone)
{TRACE_IT(2629);
    if (!this->instrFirst)
    {TRACE_IT(2630);
        this->instrFirst = instrClone;
    }
    this->instrLast = instrClone;
}

void
Cloner::Finish()
{TRACE_IT(2631);
    this->RetargetClonedBranches();
    if (this->lowerer)
    {TRACE_IT(2632);
        lowerer->LowerRange(this->instrFirst, this->instrLast, false, false);
    }
}

void
Cloner::RetargetClonedBranches()
{TRACE_IT(2633);
    if (!this->fRetargetClonedBranch)
    {TRACE_IT(2634);
        return;
    }

    FOREACH_INSTR_IN_RANGE(instr, this->instrFirst, this->instrLast)
    {TRACE_IT(2635);
        if (instr->IsBranchInstr())
        {TRACE_IT(2636);
            instr->AsBranchInstr()->RetargetClonedBranch();
        }
    }
    NEXT_INSTR_IN_RANGE;
}

void Func::ThrowIfScriptClosed()
{TRACE_IT(2637);
    if (GetScriptContextInfo()->IsClosed())
    {TRACE_IT(2638);
        // Should not be jitting something in the foreground when the script context is actually closed
        Assert(IsBackgroundJIT() || !GetScriptContext()->IsActuallyClosed());

        throw Js::OperationAbortedException();
    }
}

IR::IndirOpnd * Func::GetConstantAddressIndirOpnd(intptr_t address, IR::Opnd * largeConstOpnd, IR::AddrOpndKind kind, IRType type, Js::OpCode loadOpCode)
{TRACE_IT(2639);
    Assert(this->GetTopFunc() == this);
    if (!canHoistConstantAddressLoad)
    {TRACE_IT(2640);
        // We can't hoist constant address load after lower, as we can't mark the sym as
        // live on back edge
        return nullptr;
    }
    int offset = 0;
    IR::RegOpnd ** foundRegOpnd = this->constantAddressRegOpnd.Find([address, &offset](IR::RegOpnd * regOpnd)
    {
        Assert(regOpnd->m_sym->IsSingleDef());
        Assert(regOpnd->m_sym->m_instrDef->GetSrc1()->IsAddrOpnd() || regOpnd->m_sym->m_instrDef->GetSrc1()->IsIntConstOpnd());
        void * curr = regOpnd->m_sym->m_instrDef->GetSrc1()->IsAddrOpnd() ?
                      regOpnd->m_sym->m_instrDef->GetSrc1()->AsAddrOpnd()->m_address :
                      (void *)regOpnd->m_sym->m_instrDef->GetSrc1()->AsIntConstOpnd()->GetValue();
        ptrdiff_t diff = (uintptr_t)address - (uintptr_t)curr;
        if (!Math::FitsInDWord(diff))
        {TRACE_IT(2641);
            return false;
        }

        offset = (int)diff;
        return true;
    });

    IR::RegOpnd * addressRegOpnd;
    if (foundRegOpnd != nullptr)
    {TRACE_IT(2642);
        addressRegOpnd = *foundRegOpnd;
    }
    else
    {TRACE_IT(2643);
        Assert(offset == 0);
        addressRegOpnd = IR::RegOpnd::New(TyMachPtr, this);
        IR::Instr *const newInstr =
            IR::Instr::New(
            loadOpCode,
            addressRegOpnd,
            largeConstOpnd,
            this);
        this->constantAddressRegOpnd.Prepend(addressRegOpnd);

        IR::Instr * insertBeforeInstr = this->lastConstantAddressRegLoadInstr;
        if (insertBeforeInstr == nullptr)
        {TRACE_IT(2644);
            insertBeforeInstr = this->GetFunctionEntryInsertionPoint();
            this->lastConstantAddressRegLoadInstr = newInstr;
        }
        insertBeforeInstr->InsertBefore(newInstr);
    }
    IR::IndirOpnd * indirOpnd =  IR::IndirOpnd::New(addressRegOpnd, offset, type, this, true);
#if DBG_DUMP
    // TODO: michhol make intptr_t
    indirOpnd->SetAddrKind(kind, (void*)address);
#endif
    return indirOpnd;
}

void Func::MarkConstantAddressSyms(BVSparse<JitArenaAllocator> * bv)
{TRACE_IT(2645);
    Assert(this->GetTopFunc() == this);
    this->constantAddressRegOpnd.Iterate([bv](IR::RegOpnd * regOpnd)
    {
        bv->Set(regOpnd->m_sym->m_id);
    });
}

IR::Instr *
Func::GetFunctionEntryInsertionPoint()
{TRACE_IT(2646);
    Assert(this->GetTopFunc() == this);
    IR::Instr * insertInsert = this->lastConstantAddressRegLoadInstr;
    if (insertInsert != nullptr)
    {TRACE_IT(2647);
        return insertInsert->m_next;
    }

    insertInsert = this->m_headInstr;

    if (this->HasTry())
    {TRACE_IT(2648);
        // Insert it inside the root region
        insertInsert = insertInsert->m_next;
        Assert(insertInsert->IsLabelInstr() && insertInsert->AsLabelInstr()->GetRegion()->GetType() == RegionTypeRoot);
    }

    return insertInsert->m_next;
}

Js::Var
Func::AllocateNumber(double value)
{TRACE_IT(2649);
    Js::Var number = nullptr;
#if FLOATVAR
    number = Js::JavascriptNumber::NewCodeGenInstance((double)value, nullptr);
#else
    if (!IsOOPJIT()) // in-proc jit
    {TRACE_IT(2650);
        number = Js::JavascriptNumber::NewCodeGenInstance(GetNumberAllocator(), (double)value, GetScriptContext());
    }
    else // OOP JIT
    {TRACE_IT(2651);
        number = GetXProcNumberAllocator()->AllocateNumber(this, value);
    }
#endif

    return number;
}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
void
Func::DumpFullFunctionName()
{TRACE_IT(2652);
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

    Output::Print(_u("Function %s (%s)"), GetJITFunctionBody()->GetDisplayName(), GetDebugNumberSet(debugStringBuffer));
}
#endif

void
Func::UpdateForInLoopMaxDepth(uint forInLoopMaxDepth)
{TRACE_IT(2653);
    Assert(this->IsTopFunc());
    this->m_forInLoopMaxDepth = max(this->m_forInLoopMaxDepth, forInLoopMaxDepth);
}

int
Func::GetForInEnumeratorArrayOffset() const
{TRACE_IT(2654);
    Func const* topFunc = this->GetTopFunc();
    Assert(this->m_forInLoopBaseDepth + this->GetJITFunctionBody()->GetForInLoopDepth() <= topFunc->m_forInLoopMaxDepth);
    return topFunc->m_forInEnumeratorArrayOffset
        + this->m_forInLoopBaseDepth * sizeof(Js::ForInObjectEnumerator);
}

#if DBG_DUMP
///----------------------------------------------------------------------------
///
/// Func::DumpHeader
///
///----------------------------------------------------------------------------
void
Func::DumpHeader()
{TRACE_IT(2655);
    Output::Print(_u("-----------------------------------------------------------------------------\n"));

    DumpFullFunctionName();

    Output::SkipToColumn(50);
    Output::Print(_u("Instr Count:%d"), GetInstrCount());

    if(m_codeSize > 0)
    {TRACE_IT(2656);
        Output::Print(_u("\t\tSize:%d\n\n"), m_codeSize);
    }
    else
    {TRACE_IT(2657);
        Output::Print(_u("\n\n"));
    }
}

///----------------------------------------------------------------------------
///
/// Func::Dump
///
///----------------------------------------------------------------------------
void
Func::Dump(IRDumpFlags flags)
{TRACE_IT(2658);
    this->DumpHeader();

    FOREACH_INSTR_IN_FUNC(instr, this)
    {TRACE_IT(2659);
        instr->DumpGlobOptInstrString();
        instr->Dump(flags);
    }NEXT_INSTR_IN_FUNC;

    Output::Flush();
}

void
Func::Dump()
{TRACE_IT(2660);
    this->Dump(IRDumpFlags_None);
}
#endif

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
LPCSTR
Func::GetVtableName(INT_PTR address)
{TRACE_IT(2661);
#if DBG
    if (vtableMap == nullptr)
    {TRACE_IT(2662);
        vtableMap = VirtualTableRegistry::CreateVtableHashMap(this->m_alloc);
    };
    LPCSTR name = vtableMap->Lookup(address, nullptr);
    if (name)
    {
         if (strncmp(name, "class ", _countof("class ") - 1) == 0)
         {TRACE_IT(2663);
             name += _countof("class ") - 1;
         }
    }
    return name;
#else
    return "";
#endif
}
#endif

#if DBG_DUMP | defined(VTUNE_PROFILING)
bool Func::DoRecordNativeMap() const
{TRACE_IT(2664);
#if defined(VTUNE_PROFILING)
    if (VTuneChakraProfile::isJitProfilingActive)
    {TRACE_IT(2665);
        return true;
    }
#endif
#if DBG_DUMP
    return PHASE_DUMP(Js::EncoderPhase, this) && Js::Configuration::Global.flags.Verbose;
#else
    return false;
#endif
}
#endif

#ifdef PERF_HINT
void WritePerfHint(PerfHints hint, Func* func, uint byteCodeOffset /*= Js::Constants::NoByteCodeOffset*/)
{TRACE_IT(2666);
    if (!func->IsOOPJIT())
    {
        WritePerfHint(hint, (Js::FunctionBody*)func->GetJITFunctionBody()->GetAddr(), byteCodeOffset);
    }
}
#endif
