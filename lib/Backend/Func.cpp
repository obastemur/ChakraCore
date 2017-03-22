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
    {LOGMEIN("Func.cpp] 153\n");
        outputData->hasJittedStackClosure = false;
        outputData->localVarSlotsOffset = m_localVarSlotsOffset;
        outputData->localVarChangedOffset = m_hasLocalVarChangedOffset;
    }

    if (this->IsInlined())
    {LOGMEIN("Func.cpp] 160\n");
        m_inlineeId = ++(GetTopFunc()->m_inlineeId);
    }
    bool doStackNestedFunc = GetJITFunctionBody()->DoStackNestedFunc();

    bool doStackClosure = GetJITFunctionBody()->DoStackClosure() && !PHASE_OFF(Js::FrameDisplayFastPathPhase, this) && !PHASE_OFF(Js::StackClosurePhase, this);
    Assert(!doStackClosure || doStackNestedFunc);
    this->stackClosure = doStackClosure && this->IsTopFunc();
    if (this->stackClosure)
    {LOGMEIN("Func.cpp] 169\n");
        // TODO: calculate on runtime side?
        m_output.SetHasJITStackClosure();
    }

    if (m_workItem->Type() == JsFunctionType &&
        GetJITFunctionBody()->DoBackendArgumentsOptimization() &&
        !GetJITFunctionBody()->HasTry())
    {LOGMEIN("Func.cpp] 177\n");
        // doBackendArgumentsOptimization bit is set when there is no eval inside a function
        // as determined by the bytecode generator.
        SetHasStackArgs(true);
    }
    if (doStackNestedFunc && GetJITFunctionBody()->GetNestedCount() != 0 &&
        (this->IsTopFunc() || this->GetTopFunc()->m_workItem->Type() != JsLoopBodyWorkItemType)) // make sure none of the functions inlined in a jitted loop body allocate nested functions on the stack
    {LOGMEIN("Func.cpp] 184\n");
        Assert(!(this->IsJitInDebugMode() && !GetJITFunctionBody()->IsLibraryCode()));
        stackNestedFunc = true;
        this->GetTopFunc()->hasAnyStackNestedFunc = true;
    }

    if (GetJITFunctionBody()->HasOrParentHasArguments() || (parentFunc && parentFunc->thisOrParentInlinerHasArguments))
    {LOGMEIN("Func.cpp] 191\n");
        thisOrParentInlinerHasArguments = true;
    }

    if (parentFunc == nullptr)
    {LOGMEIN("Func.cpp] 196\n");
        inlineDepth = 0;
        m_symTable = JitAnew(alloc, SymTable);
        m_symTable->Init(this);
        m_symTable->SetStartingID(static_cast<SymID>(workItem->GetJITFunctionBody()->GetLocalsCount() + 1));

        Assert(Js::Constants::NoByteCodeOffset == postCallByteCodeOffset);
        Assert(Js::Constants::NoRegister == returnValueRegSlot);

#if defined(_M_IX86) ||  defined(_M_X64)
        if (HasArgumentSlot())
        {LOGMEIN("Func.cpp] 207\n");
            // Pre-allocate the single argument slot we'll reserve for the arguments object.
            // For ARM, the argument slot is not part of the local but part of the register saves
            m_localStackHeight = MachArgsSlotOffset;
        }
#endif
    }
    else
    {
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
    {LOGMEIN("Func.cpp] 234\n");
        m_nonTempLocalVars = Anew(this->m_alloc, BVSparse<JitArenaAllocator>, this->m_alloc);
    }

    if (GetJITFunctionBody()->IsCoroutine())
    {LOGMEIN("Func.cpp] 239\n");
        m_yieldOffsetResumeLabelList = YieldOffsetResumeLabelList::New(this->m_alloc);
    }

    if (this->IsTopFunc())
    {LOGMEIN("Func.cpp] 244\n");
        m_globalObjTypeSpecFldInfoArray = JitAnewArrayZ(this->m_alloc, JITObjTypeSpecFldInfo*, GetWorkItem()->GetJITTimeInfo()->GetGlobalObjTypeSpecFldInfoCount());
    }

    for (uint i = 0; i < GetJITFunctionBody()->GetInlineCacheCount(); ++i)
    {LOGMEIN("Func.cpp] 249\n");
        JITObjTypeSpecFldInfo * info = GetWorkItem()->GetJITTimeInfo()->GetObjTypeSpecFldInfo(i);
        if (info != nullptr)
        {LOGMEIN("Func.cpp] 252\n");
            Assert(info->GetObjTypeSpecFldId() < GetTopFunc()->GetWorkItem()->GetJITTimeInfo()->GetGlobalObjTypeSpecFldInfoCount());
            GetTopFunc()->m_globalObjTypeSpecFldInfoArray[info->GetObjTypeSpecFldId()] = info;
        }
    }

    canHoistConstantAddressLoad = !PHASE_OFF(Js::HoistConstAddrPhase, this);

    m_forInLoopMaxDepth = this->GetJITFunctionBody()->GetForInLoopDepth();
}

bool
Func::IsLoopBodyInTry() const
{LOGMEIN("Func.cpp] 265\n");
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
{LOGMEIN("Func.cpp] 282\n");
    bool rejit;
    do
    {LOGMEIN("Func.cpp] 285\n");
        Func func(alloc, workItem, threadContextInfo,
            scriptContextInfo, outputData, epInfo, runtimeInfo,
            polymorphicInlineCacheInfo, codeGenAllocators, 
#if !FLOATVAR
            numberAllocator,
#endif
            codeGenProfiler, isBackgroundJIT);
        try
        {LOGMEIN("Func.cpp] 294\n");
            func.TryCodegen();
            rejit = false;
        }
        catch (Js::RejitException ex)
        {LOGMEIN("Func.cpp] 299\n");
            // The work item needs to be rejitted, likely due to some optimization that was too aggressive
            if (ex.Reason() == RejitReason::AggressiveIntTypeSpecDisabled)
            {LOGMEIN("Func.cpp] 302\n");
                workItem->GetJITFunctionBody()->GetProfileInfo()->DisableAggressiveIntTypeSpec(func.IsLoopBody());
                outputData->disableAggressiveIntTypeSpec = TRUE;
            }
            else if (ex.Reason() == RejitReason::InlineApplyDisabled)
            {LOGMEIN("Func.cpp] 307\n");
                workItem->GetJITFunctionBody()->DisableInlineApply();
                outputData->disableInlineApply = TRUE;
            }
            else if (ex.Reason() == RejitReason::InlineSpreadDisabled)
            {LOGMEIN("Func.cpp] 312\n");
                workItem->GetJITFunctionBody()->DisableInlineSpread();
                outputData->disableInlineSpread = TRUE;
            }
            else if (ex.Reason() == RejitReason::DisableStackArgOpt)
            {LOGMEIN("Func.cpp] 317\n");
                workItem->GetJITFunctionBody()->GetProfileInfo()->DisableStackArgOpt();
                outputData->disableStackArgOpt = TRUE;
            }
            else if (ex.Reason() == RejitReason::DisableSwitchOptExpectingInteger ||
                ex.Reason() == RejitReason::DisableSwitchOptExpectingString)
            {LOGMEIN("Func.cpp] 323\n");
                workItem->GetJITFunctionBody()->GetProfileInfo()->DisableSwitchOpt();
                outputData->disableSwitchOpt = TRUE;
            }
            else
            {
                Assert(ex.Reason() == RejitReason::TrackIntOverflowDisabled);
                workItem->GetJITFunctionBody()->GetProfileInfo()->DisableTrackCompoundedIntOverflow();
                outputData->disableTrackCompoundedIntOverflow = TRUE;
            }

            if (PHASE_TRACE(Js::ReJITPhase, &func))
            {LOGMEIN("Func.cpp] 335\n");
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
        {LOGMEIN("Func.cpp] 377\n");
            IRBuilderAsmJs asmIrBuilder(this);
            asmIrBuilder.Build();
        }
        else
#endif
        {
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
        {LOGMEIN("Func.cpp] 405\n");
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
        {LOGMEIN("Func.cpp] 507\n");
            lowerer.LowerPrologEpilogAsmJs();
        }
        else
        {
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
    {LOGMEIN("Func.cpp] 536\n");
        FILE * oldFile = 0;
        FILE * asmFile = GetScriptContext()->GetNativeCodeGenerator()->asmFile;
        if (asmFile)
        {LOGMEIN("Func.cpp] 540\n");
            oldFile = Output::SetFile(asmFile);
        }

        this->Dump(IRDumpFlags_AsmDumpMode);

        Output::Flush();

        if (asmFile)
        {LOGMEIN("Func.cpp] 549\n");
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
        {LOGMEIN("Func.cpp] 561\n");
            NativeCodeData::DataChunk *chunk = (NativeCodeData::DataChunk*)dataAllocator->chunkList;
            NativeCodeData::DataChunk *next1 = chunk;
            while (next1)
            {LOGMEIN("Func.cpp] 565\n");
                if (next1->fixupFunc)
                {LOGMEIN("Func.cpp] 567\n");
                    next1->fixupFunc(next1->data, chunk);
                }
#if DBG
                // Scan memory to see if there's missing pointer needs to be fixed up
                // This can hit false positive if some data field happens to have value 
                // falls into the NativeCodeData memory range.
                NativeCodeData::DataChunk *next2 = chunk;
                while (next2)
                {LOGMEIN("Func.cpp] 576\n");
                    for (unsigned int i = 0; i < next1->len / sizeof(void*); i++)
                    {LOGMEIN("Func.cpp] 578\n");
                        if (((void**)next1->data)[i] == (void*)next2->data)
                        {LOGMEIN("Func.cpp] 580\n");
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
            {LOGMEIN("Func.cpp] 594\n");
                Js::Throw::OutOfMemory();
            }
            __analysis_assume(jitOutputData->nativeDataFixupTable);
            jitOutputData->nativeDataFixupTable->count = dataAllocator->allocCount;

            jitOutputData->buffer = (NativeDataBuffer*)midl_user_allocate(offsetof(NativeDataBuffer, data) + dataAllocator->totalSize);
            if (!jitOutputData->buffer)
            {LOGMEIN("Func.cpp] 602\n");
                Js::Throw::OutOfMemory();
            }
            __analysis_assume(jitOutputData->buffer);

            jitOutputData->buffer->len = dataAllocator->totalSize;

            unsigned int len = 0;
            unsigned int count = 0;
            next1 = chunk;
            while (next1)
            {LOGMEIN("Func.cpp] 613\n");
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
            {LOGMEIN("Func.cpp] 628\n");
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
{LOGMEIN("Func.cpp] 645\n");
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
{LOGMEIN("Func.cpp] 677\n");
    Assert(size > 0);
    if (stackSym->IsArgSlotSym() || stackSym->IsParamSlotSym() || stackSym->IsAllocated())
    {LOGMEIN("Func.cpp] 680\n");
        return stackSym->m_offset;
    }
    Assert(stackSym->m_offset == 0);
    stackSym->m_allocated = true;
    stackSym->m_offset = StackAllocate(size);

    return stackSym->m_offset;
}

void
Func::SetArgOffset(StackSym *stackSym, int32 offset)
{LOGMEIN("Func.cpp] 692\n");
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
{LOGMEIN("Func.cpp] 706\n");
    Assert(IsJitInDebugMode());

    if (!this->HasLocalVarSlotCreated())
    {LOGMEIN("Func.cpp] 710\n");
        uint32 localSlotCount = GetJITFunctionBody()->GetNonTempLocalVarCount();
        if (localSlotCount && m_localVarSlotsOffset == Js::Constants::InvalidOffset)
        {LOGMEIN("Func.cpp] 713\n");
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
{LOGMEIN("Func.cpp] 728\n");
    Assert(inlineeStart->m_func == this);
    Assert(!IsTopFunc());
    int32 lastOffset;

    IR::Instr* arg = inlineeStart->GetNextArg();
    const auto lastArgOutStackSym = arg->GetDst()->AsSymOpnd()->m_sym->AsStackSym();
    lastOffset = lastArgOutStackSym->m_offset;
    Assert(lastArgOutStackSym->m_isSingleDef);
    const auto secondLastArgOutOpnd = lastArgOutStackSym->m_instrDef->GetSrc2();
    if (secondLastArgOutOpnd->IsSymOpnd())
    {LOGMEIN("Func.cpp] 739\n");
        const auto secondLastOffset = secondLastArgOutOpnd->AsSymOpnd()->m_sym->AsStackSym()->m_offset;
        if (secondLastOffset > lastOffset)
        {LOGMEIN("Func.cpp] 742\n");
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
{LOGMEIN("Func.cpp] 754\n");
    this->EnsureLocalVarSlots();
    Assert(m_localVarSlotsOffset != Js::Constants::InvalidOffset);

    int32 slotOffset = slotId * GetDiagLocalSlotSize();

    return m_localVarSlotsOffset + slotOffset;
}

void Func::OnAddSym(Sym* sym)
{LOGMEIN("Func.cpp] 764\n");
    Assert(sym);
    if (this->IsJitInDebugMode() && this->IsNonTempLocalVar(sym->m_id))
    {LOGMEIN("Func.cpp] 767\n");
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
{LOGMEIN("Func.cpp] 779\n");
    this->EnsureLocalVarSlots();
    return m_hasLocalVarChangedOffset;
}

bool
Func::IsJitInDebugMode()
{LOGMEIN("Func.cpp] 786\n");
    return m_workItem->IsJitInDebugMode();
}

bool
Func::IsNonTempLocalVar(uint32 slotIndex)
{LOGMEIN("Func.cpp] 792\n");
    return GetJITFunctionBody()->IsNonTempLocalVar(slotIndex);
}

int32
Func::AdjustOffsetValue(int32 offset)
{LOGMEIN("Func.cpp] 798\n");
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
{LOGMEIN("Func.cpp] 811\n");
    if (GetJITFunctionBody()->GetNonTempLocalVarCount())
    {LOGMEIN("Func.cpp] 813\n");
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
{LOGMEIN("Func.cpp] 828\n");
    // Disable GlobOpt optimizations for generators initially. Will visit and enable each one by one.
    return !GetJITFunctionBody()->IsCoroutine();
}

bool
Func::DoSimpleJitDynamicProfile() const
{LOGMEIN("Func.cpp] 835\n");
    return IsSimpleJit() && !PHASE_OFF(Js::SimpleJitDynamicProfilePhase, GetTopFunc()) && !CONFIG_FLAG(NewSimpleJit);
}

void
Func::SetDoFastPaths()
{LOGMEIN("Func.cpp] 841\n");
    // Make sure we only call this once!
    Assert(!this->hasCalledSetDoFastPaths);

    bool doFastPaths = false;

    if(!PHASE_OFF(Js::FastPathPhase, this) && (!IsSimpleJit() || CONFIG_FLAG(NewSimpleJit)))
    {LOGMEIN("Func.cpp] 848\n");
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
{LOGMEIN("Func.cpp] 862\n");
#ifdef DBG
    if (Js::Configuration::Global.flags.IsEnabled(Js::ForceLocalsPtrFlag))
    {LOGMEIN("Func.cpp] 865\n");
        return ALT_LOCALS_PTR;
    }
#endif

    if (GetJITFunctionBody()->HasTry())
    {LOGMEIN("Func.cpp] 871\n");
        return ALT_LOCALS_PTR;
    }

    return RegSP;
}

#endif

void Func::AddSlotArrayCheck(IR::SymOpnd *fieldOpnd)
{LOGMEIN("Func.cpp] 881\n");
    if (PHASE_OFF(Js::ClosureRangeCheckPhase, this))
    {LOGMEIN("Func.cpp] 883\n");
        return;
    }

    Assert(IsTopFunc());
    if (this->slotArrayCheckTable == nullptr)
    {LOGMEIN("Func.cpp] 889\n");
        this->slotArrayCheckTable = SlotArrayCheckTable::New(m_alloc, 4);
    }

    PropertySym *propertySym = fieldOpnd->m_sym->AsPropertySym();
    uint32 slot = propertySym->m_propertyId;
    uint32 *pSlotId = this->slotArrayCheckTable->FindOrInsert(slot, propertySym->m_stackSym->m_id);

    if (pSlotId && (*pSlotId == (uint32)-1 || *pSlotId < slot))
    {LOGMEIN("Func.cpp] 898\n");
        *pSlotId = propertySym->m_propertyId;
    }
}

void Func::AddFrameDisplayCheck(IR::SymOpnd *fieldOpnd, uint32 slotId)
{LOGMEIN("Func.cpp] 904\n");
    if (PHASE_OFF(Js::ClosureRangeCheckPhase, this))
    {LOGMEIN("Func.cpp] 906\n");
        return;
    }

    Assert(IsTopFunc());
    if (this->frameDisplayCheckTable == nullptr)
    {LOGMEIN("Func.cpp] 912\n");
        this->frameDisplayCheckTable = FrameDisplayCheckTable::New(m_alloc, 4);
    }

    PropertySym *propertySym = fieldOpnd->m_sym->AsPropertySym();
    FrameDisplayCheckRecord **record = this->frameDisplayCheckTable->FindOrInsertNew(propertySym->m_stackSym->m_id);
    if (*record == nullptr)
    {LOGMEIN("Func.cpp] 919\n");
        *record = JitAnew(m_alloc, FrameDisplayCheckRecord);
    }

    uint32 frameDisplaySlot = propertySym->m_propertyId;
    if ((*record)->table == nullptr || (*record)->slotId < frameDisplaySlot)
    {LOGMEIN("Func.cpp] 925\n");
        (*record)->slotId = frameDisplaySlot;
    }

    if (slotId != (uint32)-1)
    {LOGMEIN("Func.cpp] 930\n");
        if ((*record)->table == nullptr)
        {LOGMEIN("Func.cpp] 932\n");
            (*record)->table = SlotArrayCheckTable::New(m_alloc, 4);
        }
        uint32 *pSlotId = (*record)->table->FindOrInsert(slotId, frameDisplaySlot);
        if (pSlotId && *pSlotId < slotId)
        {LOGMEIN("Func.cpp] 937\n");
            *pSlotId = slotId;
        }
    }
}

void Func::InitLocalClosureSyms()
{LOGMEIN("Func.cpp] 944\n");
    Assert(this->m_localClosureSym == nullptr);

    // Allocate stack space for closure pointers. Do this only if we're jitting for stack closures, and
    // tell bailout that these are not byte code symbols so that we don't try to encode them in the bailout record,
    // as they don't have normal lifetimes.
    Js::RegSlot regSlot = GetJITFunctionBody()->GetLocalClosureReg();
    if (regSlot != Js::Constants::NoRegister)
    {LOGMEIN("Func.cpp] 952\n");
        this->m_localClosureSym =
            StackSym::FindOrCreate(static_cast<SymID>(regSlot),
                                   this->DoStackFrameDisplay() ? (Js::RegSlot)-1 : regSlot,
                                   this);
    }

    regSlot = this->GetJITFunctionBody()->GetParamClosureReg();
    if (regSlot != Js::Constants::NoRegister)
    {LOGMEIN("Func.cpp] 961\n");
        Assert(this->GetParamClosureSym() == nullptr && !this->GetJITFunctionBody()->IsParamAndBodyScopeMerged());
        this->m_paramClosureSym =
            StackSym::FindOrCreate(static_cast<SymID>(regSlot),
                this->DoStackFrameDisplay() ? (Js::RegSlot) - 1 : regSlot,
                this);
    }

    regSlot = GetJITFunctionBody()->GetLocalFrameDisplayReg();
    if (regSlot != Js::Constants::NoRegister)
    {LOGMEIN("Func.cpp] 971\n");
        this->m_localFrameDisplaySym =
            StackSym::FindOrCreate(static_cast<SymID>(regSlot),
                                   this->DoStackFrameDisplay() ? (Js::RegSlot)-1 : regSlot,
                                   this);
    }
}

bool Func::CanAllocInPreReservedHeapPageSegment ()
{LOGMEIN("Func.cpp] 980\n");
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
{LOGMEIN("Func.cpp] 1016\n");
    uint instrCount = 0;

    FOREACH_INSTR_IN_FUNC(instr, this)
    {LOGMEIN("Func.cpp] 1020\n");
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
{LOGMEIN("Func.cpp] 1036\n");
#if DBG_DUMP
    Assert(this->IsTopFunc());
    Assert(!this->hasInstrNumber);
    this->hasInstrNumber = true;
#endif
    uint instrCount = 1;

    FOREACH_INSTR_IN_FUNC(instr, this)
    {LOGMEIN("Func.cpp] 1045\n");
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
{LOGMEIN("Func.cpp] 1061\n");
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
{LOGMEIN("Func.cpp] 1075\n");
#ifdef DBG
    this->GetTopFunc()->currentPhases.Push(tag);
#endif

#ifdef PROFILE_EXEC
    AssertMsg((this->m_codeGenProfiler != nullptr) == Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag),
        "Profiler tag is supplied but the profiler pointer is NULL");
    if (this->m_codeGenProfiler)
    {LOGMEIN("Func.cpp] 1084\n");
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
{LOGMEIN("Func.cpp] 1099\n");
#ifdef DBG
    Assert(this->GetTopFunc()->currentPhases.Count() > 0);
    Js::Phase popped = this->GetTopFunc()->currentPhases.Pop();
    Assert(tag == popped);
#endif

#ifdef PROFILE_EXEC
    AssertMsg((this->m_codeGenProfiler != nullptr) == Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag),
        "Profiler tag is supplied but the profiler pointer is NULL");
    if (this->m_codeGenProfiler)
    {LOGMEIN("Func.cpp] 1110\n");
        this->m_codeGenProfiler->ProfileEnd(tag);
    }
#endif
}

void
Func::EndPhase(Js::Phase tag, bool dump)
{LOGMEIN("Func.cpp] 1118\n");
    this->EndProfiler(tag);
#if DBG_DUMP
    if(dump && (PHASE_DUMP(tag, this)
        || PHASE_DUMP(Js::BackEndPhase, this)))
    {LOGMEIN("Func.cpp] 1123\n");
        Output::Print(_u("-----------------------------------------------------------------------------\n"));

        if (IsLoopBody())
        {LOGMEIN("Func.cpp] 1127\n");
            Output::Print(_u("************   IR after %s (%S) Loop %d ************\n"),
                Js::PhaseNames[tag],
                ExecutionModeName(m_workItem->GetJitMode()),
                m_workItem->GetLoopNumber());
        }
        else
        {
            Output::Print(_u("************   IR after %s (%S)  ************\n"),
                Js::PhaseNames[tag],
                ExecutionModeName(m_workItem->GetJitMode()));
        }
        this->Dump(Js::Configuration::Global.flags.AsmDiff? IRDumpFlags_AsmDumpMode : IRDumpFlags_None);
    }
#endif

#if DBG
    if (tag == Js::LowererPhase)
    {LOGMEIN("Func.cpp] 1145\n");
        Assert(!this->isPostLower);
        this->isPostLower = true;
    }
    else if (tag == Js::RegAllocPhase)
    {LOGMEIN("Func.cpp] 1150\n");
        Assert(!this->isPostRegAlloc);
        this->isPostRegAlloc = true;
    }
    else if (tag == Js::PeepsPhase)
    {LOGMEIN("Func.cpp] 1155\n");
        Assert(this->isPostLower && !this->isPostLayout);
        this->isPostPeeps = true;
    }
    else if (tag == Js::LayoutPhase)
    {LOGMEIN("Func.cpp] 1160\n");
        Assert(this->isPostPeeps && !this->isPostLayout);
        this->isPostLayout = true;
    }
    else if (tag == Js::FinalLowerPhase)
    {LOGMEIN("Func.cpp] 1165\n");
        Assert(this->isPostLayout && !this->isPostFinalLower);
        this->isPostFinalLower = true;
    }
    if (this->isPostLower)
    {LOGMEIN("Func.cpp] 1170\n");
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
{LOGMEIN("Func.cpp] 1183\n");
    Func const * func = this;
    while (!func->IsTopFunc())
    {LOGMEIN("Func.cpp] 1186\n");
        func = func->parentFunc;
    }
    return func;
}

Func *
Func::GetTopFunc()
{LOGMEIN("Func.cpp] 1194\n");
    Func * func = this;
    while (!func->IsTopFunc())
    {LOGMEIN("Func.cpp] 1197\n");
        func = func->parentFunc;
    }
    return func;
}

StackSym *
Func::EnsureLoopParamSym()
{LOGMEIN("Func.cpp] 1205\n");
    if (this->m_loopParamSym == nullptr)
    {LOGMEIN("Func.cpp] 1207\n");
        this->m_loopParamSym = StackSym::New(TyMachPtr, this);
    }
    return this->m_loopParamSym;
}

void
Func::UpdateMaxInlineeArgOutCount(uint inlineeArgOutCount)
{LOGMEIN("Func.cpp] 1215\n");
    if (maxInlineeArgOutCount < inlineeArgOutCount)
    {LOGMEIN("Func.cpp] 1217\n");
        maxInlineeArgOutCount = inlineeArgOutCount;
    }
}

void
Func::BeginClone(Lowerer * lowerer, JitArenaAllocator *alloc)
{LOGMEIN("Func.cpp] 1224\n");
    Assert(this->IsTopFunc());
    AssertMsg(m_cloner == nullptr, "Starting new clone while one is in progress");
    m_cloner = JitAnew(alloc, Cloner, lowerer, alloc);
    if (m_cloneMap == nullptr)
    {LOGMEIN("Func.cpp] 1229\n");
         m_cloneMap = JitAnew(alloc, InstrMap, alloc, 7);
    }
}

void
Func::EndClone()
{LOGMEIN("Func.cpp] 1236\n");
    Assert(this->IsTopFunc());
    if (m_cloner)
    {LOGMEIN("Func.cpp] 1239\n");
        m_cloner->Finish();
        JitAdelete(m_cloner->alloc, m_cloner);
        m_cloner = nullptr;
    }
}

IR::SymOpnd *
Func::GetInlineeOpndAtOffset(int32 offset)
{LOGMEIN("Func.cpp] 1248\n");
    Assert(IsInlinee());

    StackSym *stackSym = CreateInlineeStackSym();
    this->SetArgOffset(stackSym, stackSym->m_offset + offset);
    Assert(stackSym->m_offset >= 0);

    return IR::SymOpnd::New(stackSym, 0, TyMachReg, this);
}

StackSym *
Func::CreateInlineeStackSym()
{LOGMEIN("Func.cpp] 1260\n");
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
{LOGMEIN("Func.cpp] 1276\n");
    // this value can change while JITing, so or these together
    return GetJITFunctionBody()->GetArgUsedForBranch() | GetJITOutput()->GetArgUsedForBranch();
}

intptr_t
Func::GetJittedLoopIterationsSinceLastBailoutAddress() const
{LOGMEIN("Func.cpp] 1283\n");
    Assert(this->m_workItem->Type() == JsLoopBodyWorkItemType);

    return m_workItem->GetJittedLoopIterationsSinceLastBailoutAddr();
}

intptr_t
Func::GetWeakFuncRef() const
{LOGMEIN("Func.cpp] 1291\n");
    // TODO: OOP JIT figure out if this can be null

    return m_workItem->GetJITTimeInfo()->GetWeakFuncRef();
}

intptr_t
Func::GetRuntimeInlineCache(const uint index) const
{LOGMEIN("Func.cpp] 1299\n");
    if(m_runtimeInfo != nullptr && m_runtimeInfo->HasClonedInlineCaches())
    {LOGMEIN("Func.cpp] 1301\n");
        intptr_t inlineCache = m_runtimeInfo->GetClonedInlineCache(index);
        if(inlineCache)
        {LOGMEIN("Func.cpp] 1304\n");
            return inlineCache;
        }
    }

    return GetJITFunctionBody()->GetInlineCache(index);
}

JITTimePolymorphicInlineCache *
Func::GetRuntimePolymorphicInlineCache(const uint index) const
{LOGMEIN("Func.cpp] 1314\n");
    if (this->m_polymorphicInlineCacheInfo && this->m_polymorphicInlineCacheInfo->HasInlineCaches())
    {LOGMEIN("Func.cpp] 1316\n");
        return this->m_polymorphicInlineCacheInfo->GetInlineCache(index);
    }
    return nullptr;
}

byte
Func::GetPolyCacheUtilToInitialize(const uint index) const
{LOGMEIN("Func.cpp] 1324\n");
    return this->GetRuntimePolymorphicInlineCache(index) ? this->GetPolyCacheUtil(index) : PolymorphicInlineCacheUtilizationMinValue;
}

byte
Func::GetPolyCacheUtil(const uint index) const
{LOGMEIN("Func.cpp] 1330\n");
    return this->m_polymorphicInlineCacheInfo->GetUtil(index);
}

JITObjTypeSpecFldInfo*
Func::GetObjTypeSpecFldInfo(const uint index) const
{LOGMEIN("Func.cpp] 1336\n");
    if (GetJITFunctionBody()->GetInlineCacheCount() == 0)
    {LOGMEIN("Func.cpp] 1338\n");
        Assert(UNREACHED);
        return nullptr;
    }

    return GetWorkItem()->GetJITTimeInfo()->GetObjTypeSpecFldInfo(index);
}

JITObjTypeSpecFldInfo*
Func::GetGlobalObjTypeSpecFldInfo(uint propertyInfoId) const
{LOGMEIN("Func.cpp] 1348\n");
    Assert(propertyInfoId < GetTopFunc()->GetWorkItem()->GetJITTimeInfo()->GetGlobalObjTypeSpecFldInfoCount());
    return GetTopFunc()->m_globalObjTypeSpecFldInfoArray[propertyInfoId];
}

void
Func::EnsurePinnedTypeRefs()
{LOGMEIN("Func.cpp] 1355\n");
    if (this->pinnedTypeRefs == nullptr)
    {LOGMEIN("Func.cpp] 1357\n");
        this->pinnedTypeRefs = JitAnew(this->m_alloc, TypeRefSet, this->m_alloc);
    }
}

void
Func::PinTypeRef(void* typeRef)
{LOGMEIN("Func.cpp] 1364\n");
    EnsurePinnedTypeRefs();
    this->pinnedTypeRefs->AddNew(typeRef);
}

void
Func::EnsureSingleTypeGuards()
{LOGMEIN("Func.cpp] 1371\n");
    if (this->singleTypeGuards == nullptr)
    {LOGMEIN("Func.cpp] 1373\n");
        this->singleTypeGuards = JitAnew(this->m_alloc, TypePropertyGuardDictionary, this->m_alloc);
    }
}

Js::JitTypePropertyGuard*
Func::GetOrCreateSingleTypeGuard(intptr_t typeAddr)
{LOGMEIN("Func.cpp] 1380\n");
    EnsureSingleTypeGuards();

    Js::JitTypePropertyGuard* guard;
    if (!this->singleTypeGuards->TryGetValue(typeAddr, &guard))
    {LOGMEIN("Func.cpp] 1385\n");
        // Property guards are allocated by NativeCodeData::Allocator so that their lifetime extends as long as the EntryPointInfo is alive.
        guard = NativeCodeDataNewNoFixup(GetNativeCodeDataAllocator(), Js::JitTypePropertyGuard, typeAddr, this->indexedPropertyGuardCount++);
        this->singleTypeGuards->Add(typeAddr, guard);
    }
    else
    {
        Assert(guard->GetTypeAddr() == typeAddr);
    }

    return guard;
}

void
Func::EnsureEquivalentTypeGuards()
{LOGMEIN("Func.cpp] 1400\n");
    if (this->equivalentTypeGuards == nullptr)
    {LOGMEIN("Func.cpp] 1402\n");
        this->equivalentTypeGuards = JitAnew(this->m_alloc, EquivalentTypeGuardList, this->m_alloc);
    }
}

Js::JitEquivalentTypeGuard*
Func::CreateEquivalentTypeGuard(JITTypeHolder type, uint32 objTypeSpecFldId)
{LOGMEIN("Func.cpp] 1409\n");
    EnsureEquivalentTypeGuards();

    Js::JitEquivalentTypeGuard* guard = NativeCodeDataNewNoFixup(GetNativeCodeDataAllocator(), Js::JitEquivalentTypeGuard, type->GetAddr(), this->indexedPropertyGuardCount++, objTypeSpecFldId);

    // If we want to hard code the address of the cache, we will need to go back to allocating it from the native code data allocator.
    // We would then need to maintain consistency (double write) to both the recycler allocated cache and the one on the heap.
    Js::EquivalentTypeCache* cache = nullptr;
    if (this->IsOOPJIT())
    {LOGMEIN("Func.cpp] 1418\n");
        cache = JitAnewZ(this->m_alloc, Js::EquivalentTypeCache);
    }
    else
    {
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
{LOGMEIN("Func.cpp] 1436\n");
    if (this->propertyGuardsByPropertyId == nullptr)
    {LOGMEIN("Func.cpp] 1438\n");
        this->propertyGuardsByPropertyId = JitAnew(this->m_alloc, PropertyGuardByPropertyIdMap, this->m_alloc);
    }
}

void
Func::EnsureCtorCachesByPropertyId()
{LOGMEIN("Func.cpp] 1445\n");
    if (this->ctorCachesByPropertyId == nullptr)
    {LOGMEIN("Func.cpp] 1447\n");
        this->ctorCachesByPropertyId = JitAnew(this->m_alloc, CtorCachesByPropertyIdMap, this->m_alloc);
    }
}

void
Func::LinkGuardToPropertyId(Js::PropertyId propertyId, Js::JitIndexedPropertyGuard* guard)
{LOGMEIN("Func.cpp] 1454\n");
    Assert(guard != nullptr);
    Assert(guard->GetValue() != NULL);

    Assert(this->propertyGuardsByPropertyId != nullptr);

    IndexedPropertyGuardSet* set;
    if (!this->propertyGuardsByPropertyId->TryGetValue(propertyId, &set))
    {LOGMEIN("Func.cpp] 1462\n");
        set = JitAnew(this->m_alloc, IndexedPropertyGuardSet, this->m_alloc);
        this->propertyGuardsByPropertyId->Add(propertyId, set);
    }

    set->Item(guard);
}

void
Func::LinkCtorCacheToPropertyId(Js::PropertyId propertyId, JITTimeConstructorCache* cache)
{LOGMEIN("Func.cpp] 1472\n");
    Assert(cache != nullptr);
    Assert(this->ctorCachesByPropertyId != nullptr);

    CtorCacheSet* set;
    if (!this->ctorCachesByPropertyId->TryGetValue(propertyId, &set))
    {LOGMEIN("Func.cpp] 1478\n");
        set = JitAnew(this->m_alloc, CtorCacheSet, this->m_alloc);
        this->ctorCachesByPropertyId->Add(propertyId, set);
    }

    set->Item(cache->GetRuntimeCacheAddr());
}

JITTimeConstructorCache* Func::GetConstructorCache(const Js::ProfileId profiledCallSiteId)
{LOGMEIN("Func.cpp] 1487\n");
    Assert(profiledCallSiteId < GetJITFunctionBody()->GetProfiledCallSiteCount());
    Assert(this->constructorCaches != nullptr);
    return this->constructorCaches[profiledCallSiteId];
}

void Func::SetConstructorCache(const Js::ProfileId profiledCallSiteId, JITTimeConstructorCache* constructorCache)
{LOGMEIN("Func.cpp] 1494\n");
    Assert(profiledCallSiteId < GetJITFunctionBody()->GetProfiledCallSiteCount());
    Assert(constructorCache != nullptr);
    Assert(this->constructorCaches != nullptr);
    Assert(this->constructorCaches[profiledCallSiteId] == nullptr);
    this->constructorCacheCount++;
    this->constructorCaches[profiledCallSiteId] = constructorCache;
}

void Func::EnsurePropertiesWrittenTo()
{LOGMEIN("Func.cpp] 1504\n");
    if (this->propertiesWrittenTo == nullptr)
    {LOGMEIN("Func.cpp] 1506\n");
        this->propertiesWrittenTo = JitAnew(this->m_alloc, PropertyIdSet, this->m_alloc);
    }
}

void Func::EnsureCallSiteToArgumentsOffsetFixupMap()
{LOGMEIN("Func.cpp] 1512\n");
    if (this->callSiteToArgumentsOffsetFixupMap == nullptr)
    {LOGMEIN("Func.cpp] 1514\n");
        this->callSiteToArgumentsOffsetFixupMap = JitAnew(this->m_alloc, CallSiteToArgumentsOffsetFixupMap, this->m_alloc);
    }
}

IR::LabelInstr *
Func::GetFuncStartLabel()
{LOGMEIN("Func.cpp] 1521\n");
    return m_funcStartLabel;
}

IR::LabelInstr *
Func::EnsureFuncStartLabel()
{LOGMEIN("Func.cpp] 1527\n");
    if(m_funcStartLabel == nullptr)
    {LOGMEIN("Func.cpp] 1529\n");
        m_funcStartLabel = IR::LabelInstr::New( Js::OpCode::Label, this );
    }
    return m_funcStartLabel;
}

IR::LabelInstr *
Func::GetFuncEndLabel()
{LOGMEIN("Func.cpp] 1537\n");
    return m_funcEndLabel;
}

IR::LabelInstr *
Func::EnsureFuncEndLabel()
{LOGMEIN("Func.cpp] 1543\n");
    if(m_funcEndLabel == nullptr)
    {LOGMEIN("Func.cpp] 1545\n");
        m_funcEndLabel = IR::LabelInstr::New( Js::OpCode::Label, this );
    }
    return m_funcEndLabel;
}

void
Func::EnsureStackArgWithFormalsTracker()
{LOGMEIN("Func.cpp] 1553\n");
    if (stackArgWithFormalsTracker == nullptr)
    {LOGMEIN("Func.cpp] 1555\n");
        stackArgWithFormalsTracker = JitAnew(m_alloc, StackArgWithFormalsTracker, m_alloc);
    }
}

BOOL
Func::IsFormalsArraySym(SymID symId)
{LOGMEIN("Func.cpp] 1562\n");
    if (stackArgWithFormalsTracker == nullptr || stackArgWithFormalsTracker->GetFormalsArraySyms() == nullptr)
    {LOGMEIN("Func.cpp] 1564\n");
        return false;
    }
    return stackArgWithFormalsTracker->GetFormalsArraySyms()->Test(symId);
}

void
Func::TrackFormalsArraySym(SymID symId)
{LOGMEIN("Func.cpp] 1572\n");
    EnsureStackArgWithFormalsTracker();
    stackArgWithFormalsTracker->SetFormalsArraySyms(symId);
}

void
Func::TrackStackSymForFormalIndex(Js::ArgSlot formalsIndex, StackSym * sym)
{LOGMEIN("Func.cpp] 1579\n");
    EnsureStackArgWithFormalsTracker();
    Js::ArgSlot formalsCount = GetJITFunctionBody()->GetInParamsCount() - 1;
    stackArgWithFormalsTracker->SetStackSymInFormalsIndexMap(sym, formalsIndex, formalsCount);
}

StackSym *
Func::GetStackSymForFormal(Js::ArgSlot formalsIndex)
{LOGMEIN("Func.cpp] 1587\n");
    if (stackArgWithFormalsTracker == nullptr || stackArgWithFormalsTracker->GetFormalsIndexToStackSymMap() == nullptr)
    {LOGMEIN("Func.cpp] 1589\n");
        return nullptr;
    }

    Js::ArgSlot formalsCount = GetJITFunctionBody()->GetInParamsCount() - 1;
    StackSym ** formalsIndexToStackSymMap = stackArgWithFormalsTracker->GetFormalsIndexToStackSymMap();
    AssertMsg(formalsIndex < formalsCount, "OutOfRange ? ");
    return formalsIndexToStackSymMap[formalsIndex];
}

bool
Func::HasStackSymForFormal(Js::ArgSlot formalsIndex)
{LOGMEIN("Func.cpp] 1601\n");
    if (stackArgWithFormalsTracker == nullptr || stackArgWithFormalsTracker->GetFormalsIndexToStackSymMap() == nullptr)
    {LOGMEIN("Func.cpp] 1603\n");
        return false;
    }
    return GetStackSymForFormal(formalsIndex) != nullptr;
}

void
Func::SetScopeObjSym(StackSym * sym)
{LOGMEIN("Func.cpp] 1611\n");
    EnsureStackArgWithFormalsTracker();
    stackArgWithFormalsTracker->SetScopeObjSym(sym);
}

StackSym *
Func::GetNativeCodeDataSym() const
{LOGMEIN("Func.cpp] 1618\n");
    Assert(IsOOPJIT());
    return m_nativeCodeDataSym;
}

void
Func::SetNativeCodeDataSym(StackSym * opnd)
{LOGMEIN("Func.cpp] 1625\n");
    Assert(IsOOPJIT());
    m_nativeCodeDataSym = opnd;
}

StackSym*
Func::GetScopeObjSym()
{LOGMEIN("Func.cpp] 1632\n");
    if (stackArgWithFormalsTracker == nullptr)
    {LOGMEIN("Func.cpp] 1634\n");
        return nullptr;
    }
    return stackArgWithFormalsTracker->GetScopeObjSym();
}

BVSparse<JitArenaAllocator> *
StackArgWithFormalsTracker::GetFormalsArraySyms()
{LOGMEIN("Func.cpp] 1642\n");
    return formalsArraySyms;
}

void
StackArgWithFormalsTracker::SetFormalsArraySyms(SymID symId)
{LOGMEIN("Func.cpp] 1648\n");
    if (formalsArraySyms == nullptr)
    {LOGMEIN("Func.cpp] 1650\n");
        formalsArraySyms = JitAnew(alloc, BVSparse<JitArenaAllocator>, alloc);
    }
    formalsArraySyms->Set(symId);
}

StackSym **
StackArgWithFormalsTracker::GetFormalsIndexToStackSymMap()
{LOGMEIN("Func.cpp] 1658\n");
    return formalsIndexToStackSymMap;
}

void
StackArgWithFormalsTracker::SetStackSymInFormalsIndexMap(StackSym * sym, Js::ArgSlot formalsIndex, Js::ArgSlot formalsCount)
{LOGMEIN("Func.cpp] 1664\n");
    if(formalsIndexToStackSymMap == nullptr)
    {LOGMEIN("Func.cpp] 1666\n");
        formalsIndexToStackSymMap = JitAnewArrayZ(alloc, StackSym*, formalsCount);
    }
    AssertMsg(formalsIndex < formalsCount, "Out of range ?");
    formalsIndexToStackSymMap[formalsIndex] = sym;
}

void
StackArgWithFormalsTracker::SetScopeObjSym(StackSym * sym)
{LOGMEIN("Func.cpp] 1675\n");
    m_scopeObjSym = sym;
}

StackSym *
StackArgWithFormalsTracker::GetScopeObjSym()
{LOGMEIN("Func.cpp] 1681\n");
    return m_scopeObjSym;
}


void
Cloner::AddInstr(IR::Instr * instrOrig, IR::Instr * instrClone)
{LOGMEIN("Func.cpp] 1688\n");
    if (!this->instrFirst)
    {LOGMEIN("Func.cpp] 1690\n");
        this->instrFirst = instrClone;
    }
    this->instrLast = instrClone;
}

void
Cloner::Finish()
{LOGMEIN("Func.cpp] 1698\n");
    this->RetargetClonedBranches();
    if (this->lowerer)
    {LOGMEIN("Func.cpp] 1701\n");
        lowerer->LowerRange(this->instrFirst, this->instrLast, false, false);
    }
}

void
Cloner::RetargetClonedBranches()
{LOGMEIN("Func.cpp] 1708\n");
    if (!this->fRetargetClonedBranch)
    {LOGMEIN("Func.cpp] 1710\n");
        return;
    }

    FOREACH_INSTR_IN_RANGE(instr, this->instrFirst, this->instrLast)
    {LOGMEIN("Func.cpp] 1715\n");
        if (instr->IsBranchInstr())
        {LOGMEIN("Func.cpp] 1717\n");
            instr->AsBranchInstr()->RetargetClonedBranch();
        }
    }
    NEXT_INSTR_IN_RANGE;
}

void Func::ThrowIfScriptClosed()
{LOGMEIN("Func.cpp] 1725\n");
    if (GetScriptContextInfo()->IsClosed())
    {LOGMEIN("Func.cpp] 1727\n");
        // Should not be jitting something in the foreground when the script context is actually closed
        Assert(IsBackgroundJIT() || !GetScriptContext()->IsActuallyClosed());

        throw Js::OperationAbortedException();
    }
}

IR::IndirOpnd * Func::GetConstantAddressIndirOpnd(intptr_t address, IR::Opnd * largeConstOpnd, IR::AddrOpndKind kind, IRType type, Js::OpCode loadOpCode)
{LOGMEIN("Func.cpp] 1736\n");
    Assert(this->GetTopFunc() == this);
    if (!canHoistConstantAddressLoad)
    {LOGMEIN("Func.cpp] 1739\n");
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
        {LOGMEIN("Func.cpp] 1754\n");
            return false;
        }

        offset = (int)diff;
        return true;
    });

    IR::RegOpnd * addressRegOpnd;
    if (foundRegOpnd != nullptr)
    {LOGMEIN("Func.cpp] 1764\n");
        addressRegOpnd = *foundRegOpnd;
    }
    else
    {
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
        {LOGMEIN("Func.cpp] 1781\n");
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
{LOGMEIN("Func.cpp] 1796\n");
    Assert(this->GetTopFunc() == this);
    this->constantAddressRegOpnd.Iterate([bv](IR::RegOpnd * regOpnd)
    {
        bv->Set(regOpnd->m_sym->m_id);
    });
}

IR::Instr *
Func::GetFunctionEntryInsertionPoint()
{LOGMEIN("Func.cpp] 1806\n");
    Assert(this->GetTopFunc() == this);
    IR::Instr * insertInsert = this->lastConstantAddressRegLoadInstr;
    if (insertInsert != nullptr)
    {LOGMEIN("Func.cpp] 1810\n");
        return insertInsert->m_next;
    }

    insertInsert = this->m_headInstr;

    if (this->HasTry())
    {LOGMEIN("Func.cpp] 1817\n");
        // Insert it inside the root region
        insertInsert = insertInsert->m_next;
        Assert(insertInsert->IsLabelInstr() && insertInsert->AsLabelInstr()->GetRegion()->GetType() == RegionTypeRoot);
    }

    return insertInsert->m_next;
}

Js::Var
Func::AllocateNumber(double value)
{LOGMEIN("Func.cpp] 1828\n");
    Js::Var number = nullptr;
#if FLOATVAR
    number = Js::JavascriptNumber::NewCodeGenInstance((double)value, nullptr);
#else
    if (!IsOOPJIT()) // in-proc jit
    {LOGMEIN("Func.cpp] 1834\n");
        number = Js::JavascriptNumber::NewCodeGenInstance(GetNumberAllocator(), (double)value, GetScriptContext());
    }
    else // OOP JIT
    {
        number = GetXProcNumberAllocator()->AllocateNumber(this, value);
    }
#endif

    return number;
}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
void
Func::DumpFullFunctionName()
{LOGMEIN("Func.cpp] 1849\n");
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

    Output::Print(_u("Function %s (%s)"), GetJITFunctionBody()->GetDisplayName(), GetDebugNumberSet(debugStringBuffer));
}
#endif

void
Func::UpdateForInLoopMaxDepth(uint forInLoopMaxDepth)
{LOGMEIN("Func.cpp] 1858\n");
    Assert(this->IsTopFunc());
    this->m_forInLoopMaxDepth = max(this->m_forInLoopMaxDepth, forInLoopMaxDepth);
}

int
Func::GetForInEnumeratorArrayOffset() const
{LOGMEIN("Func.cpp] 1865\n");
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
{LOGMEIN("Func.cpp] 1880\n");
    Output::Print(_u("-----------------------------------------------------------------------------\n"));

    DumpFullFunctionName();

    Output::SkipToColumn(50);
    Output::Print(_u("Instr Count:%d"), GetInstrCount());

    if(m_codeSize > 0)
    {LOGMEIN("Func.cpp] 1889\n");
        Output::Print(_u("\t\tSize:%d\n\n"), m_codeSize);
    }
    else
    {
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
{LOGMEIN("Func.cpp] 1905\n");
    this->DumpHeader();

    FOREACH_INSTR_IN_FUNC(instr, this)
    {LOGMEIN("Func.cpp] 1909\n");
        instr->DumpGlobOptInstrString();
        instr->Dump(flags);
    }NEXT_INSTR_IN_FUNC;

    Output::Flush();
}

void
Func::Dump()
{LOGMEIN("Func.cpp] 1919\n");
    this->Dump(IRDumpFlags_None);
}
#endif

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
LPCSTR
Func::GetVtableName(INT_PTR address)
{LOGMEIN("Func.cpp] 1927\n");
#if DBG
    if (vtableMap == nullptr)
    {LOGMEIN("Func.cpp] 1930\n");
        vtableMap = VirtualTableRegistry::CreateVtableHashMap(this->m_alloc);
    };
    LPCSTR name = vtableMap->Lookup(address, nullptr);
    if (name)
    {
         if (strncmp(name, "class ", _countof("class ") - 1) == 0)
         {LOGMEIN("Func.cpp] 1937\n");
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
{LOGMEIN("Func.cpp] 1950\n");
#if defined(VTUNE_PROFILING)
    if (VTuneChakraProfile::isJitProfilingActive)
    {LOGMEIN("Func.cpp] 1953\n");
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
{LOGMEIN("Func.cpp] 1967\n");
    if (!func->IsOOPJIT())
    {
        WritePerfHint(hint, (Js::FunctionBody*)func->GetJITFunctionBody()->GetAddr(), byteCodeOffset);
    }
}
#endif
