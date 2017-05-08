//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "ByteCode/ByteCodeApi.h"
#include "ByteCode/ByteCodeDumper.h"
#include "Language/AsmJsTypes.h"
#include "Language/AsmJsModule.h"
#include "ByteCode/ByteCodeSerializer.h"
#include "Language/FunctionCodeGenRuntimeData.h"

#include "ByteCode/ScopeInfo.h"
#include "Base/EtwTrace.h"
#ifdef VTUNE_PROFILING
#include "Base/VTuneChakraProfile.h"
#endif

#ifdef DYNAMIC_PROFILE_MUTATOR
#include "Language/DynamicProfileMutator.h"
#endif
#include "Language/SourceDynamicProfileManager.h"

#include "Debug/ProbeContainer.h"
#include "Debug/DebugContext.h"

#include "Parser.h"
#include "RegexCommon.h"
#include "RegexPattern.h"
#include "Library/RegexHelper.h"

#include "Language/InterpreterStackFrame.h"
#include "Library/ModuleRoot.h"
#include "Types/PathTypeHandler.h"
#include "Common/MathUtil.h"

namespace Js
{
    // The VS2013 linker treats this as a redefinition of an already
    // defined constant and complains. So skip the declaration if we're compiling
    // with VS2013 or below.
#if !defined(_MSC_VER) || _MSC_VER >= 1900
    uint const ScopeSlots::MaxEncodedSlotCount;
#endif

    CriticalSection FunctionProxy::GlobalLock;

#ifdef FIELD_ACCESS_STATS
    void FieldAccessStats::Add(FieldAccessStats* other)
    {TRACE_IT(33838);
        Assert(other != nullptr);
        this->totalInlineCacheCount += other->totalInlineCacheCount;
        this->noInfoInlineCacheCount += other->noInfoInlineCacheCount;
        this->monoInlineCacheCount += other->monoInlineCacheCount;
        this->emptyMonoInlineCacheCount += other->emptyMonoInlineCacheCount;
        this->polyInlineCacheCount += other->polyInlineCacheCount;
        this->nullPolyInlineCacheCount += other->nullPolyInlineCacheCount;
        this->emptyPolyInlineCacheCount += other->emptyPolyInlineCacheCount;
        this->ignoredPolyInlineCacheCount += other->ignoredPolyInlineCacheCount;
        this->highUtilPolyInlineCacheCount += other->highUtilPolyInlineCacheCount;
        this->lowUtilPolyInlineCacheCount += other->lowUtilPolyInlineCacheCount;
        this->equivPolyInlineCacheCount += other->equivPolyInlineCacheCount;
        this->nonEquivPolyInlineCacheCount += other->nonEquivPolyInlineCacheCount;
        this->disabledPolyInlineCacheCount += other->disabledPolyInlineCacheCount;
        this->clonedMonoInlineCacheCount += other->clonedMonoInlineCacheCount;
        this->clonedPolyInlineCacheCount += other->clonedPolyInlineCacheCount;
    }
#endif

    // FunctionProxy methods
    FunctionProxy::FunctionProxy(ScriptContext* scriptContext, Utf8SourceInfo* utf8SourceInfo, uint functionNumber):
        m_isTopLevel(false),
        m_isPublicLibraryCode(false),
        m_scriptContext(scriptContext),
        m_utf8SourceInfo(utf8SourceInfo),
        m_functionNumber(functionNumber),
        m_defaultEntryPointInfo(nullptr),
        m_displayNameIsRecyclerAllocated(false),
        m_tag11(true)
    {
        PERF_COUNTER_INC(Code, TotalFunction);
    }

    bool FunctionProxy::IsWasmFunction() const
    {TRACE_IT(33839);
        return GetFunctionInfo()->HasParseableInfo() &&
            GetFunctionInfo()->GetFunctionBody()->IsWasmFunction();
    }

    Recycler* FunctionProxy::GetRecycler() const
    {TRACE_IT(33840);
        return m_scriptContext->GetRecycler();
    }

    void* FunctionProxy::GetAuxPtr(AuxPointerType e) const
    {TRACE_IT(33841);
        if (this->auxPtrs == nullptr)
        {TRACE_IT(33842);
            return nullptr;
        }

        // On process detach this can be called from another thread but the ThreadContext should be locked
        Assert(ThreadContext::GetContextForCurrentThread() || ThreadContext::GetCriticalSection()->IsLocked());
        return AuxPtrsT::GetAuxPtr(this, e);
    }

    void* FunctionProxy::GetAuxPtrWithLock(AuxPointerType e) const
    {TRACE_IT(33843);
        if (this->auxPtrs == nullptr)
        {TRACE_IT(33844);
            return nullptr;
        }
#if DBG && ENABLE_NATIVE_CODEGEN
        // the lock for work item queue should not be locked while accessing AuxPtrs in background thread
        auto jobProcessorCS = this->GetScriptContext()->GetThreadContext()->GetJobProcessor()->GetCriticalSection();
        Assert(!jobProcessorCS || !jobProcessorCS->IsLocked());
#endif
        AutoCriticalSection autoCS(&GlobalLock);
        return AuxPtrsT::GetAuxPtr(this, e);
    }

    void FunctionProxy::SetAuxPtr(AuxPointerType e, void* ptr)
    {TRACE_IT(33845);
        // On process detach this can be called from another thread but the ThreadContext should be locked
        Assert(ThreadContext::GetContextForCurrentThread() || ThreadContext::GetCriticalSection()->IsLocked());

        if (ptr == nullptr && GetAuxPtr(e) == nullptr)
        {TRACE_IT(33846);
            return;
        }

        // when setting ptr to null we never need to promote
        AutoCriticalSection aucoCS(&GlobalLock);
        AuxPtrsT::SetAuxPtr(this, e, ptr);
    }

    uint FunctionProxy::GetSourceContextId() const
    {TRACE_IT(33847);
        return this->GetUtf8SourceInfo()->GetSrcInfo()->sourceContextInfo->sourceContextId;
    }

    char16* FunctionProxy::GetDebugNumberSet(wchar(&bufferToWriteTo)[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE]) const
    {TRACE_IT(33848);
        // (#%u.%u), #%u --> (source file Id . function Id) , function Number
        int len = swprintf_s(bufferToWriteTo, MAX_FUNCTION_BODY_DEBUG_STRING_SIZE, _u(" (#%d.%u), #%u"),
            (int)this->GetSourceContextId(), this->GetLocalFunctionId(), this->GetFunctionNumber());
        Assert(len > 8);
        return bufferToWriteTo;
    }

    bool
    FunctionProxy::IsFunctionBody() const
    {TRACE_IT(33849);
        return !IsDeferredDeserializeFunction() && GetParseableFunctionInfo()->IsFunctionParsed();
    }

    uint
    ParseableFunctionInfo::GetSourceIndex() const
    {TRACE_IT(33850);
        return this->m_sourceIndex;
    }

    LPCUTF8
    ParseableFunctionInfo::GetSource(const char16* reason) const
    {TRACE_IT(33851);
        return this->GetUtf8SourceInfo()->GetSource(reason == nullptr ? _u("ParseableFunctionInfo::GetSource") : reason) + this->StartOffset();
    }

    LPCUTF8
    ParseableFunctionInfo::GetStartOfDocument(const char16* reason) const
    {TRACE_IT(33852);
        return this->GetUtf8SourceInfo()->GetSource(reason == nullptr ? _u("ParseableFunctionInfo::GetStartOfDocument") : reason);
    }

    bool
    ParseableFunctionInfo::IsDynamicFunction() const
    {TRACE_IT(33853);
        return this->m_isDynamicFunction;
    }

    bool
    ParseableFunctionInfo::IsDynamicScript() const
    {TRACE_IT(33854);
        return this->GetSourceContextInfo()->IsDynamic();
    }

    charcount_t
    ParseableFunctionInfo::StartInDocument() const
    {TRACE_IT(33855);
        return this->m_cchStartOffset;
    }

    uint
    ParseableFunctionInfo::StartOffset() const
    {TRACE_IT(33856);
        return this->m_cbStartOffset;
    }

    void ParseableFunctionInfo::RegisterFuncToDiag(ScriptContext * scriptContext, char16 const * pszTitle)
    {TRACE_IT(33857);
        // Register the function to the PDM as eval code (the debugger app will show file as 'eval code')
        scriptContext->GetDebugContext()->RegisterFunction(this, pszTitle);
    }

    // Given an offset into the source buffer, determine if the end of this SourceInfo
    // lies after the given offset.
    bool
    ParseableFunctionInfo::EndsAfter(size_t offset) const
    {TRACE_IT(33858);
        return offset < this->StartOffset() + this->LengthInBytes();
    }

    void
    FunctionBody::RecordStatementMap(StatementMap* pStatementMap)
    {TRACE_IT(33859);
        Assert(!this->m_sourceInfo.pSpanSequence);
        Recycler* recycler = this->m_scriptContext->GetRecycler();
        StatementMapList * statementMaps = this->GetStatementMaps();
        if (!statementMaps)
        {TRACE_IT(33860);
            statementMaps = RecyclerNew(recycler, StatementMapList, recycler);
            this->SetStatementMaps(statementMaps);
        }

        statementMaps->Add(pStatementMap);
    }

    void
    FunctionBody::RecordStatementMap(SmallSpanSequenceIter &iter, StatementData * data)
    {TRACE_IT(33861);
        Assert(!this->GetStatementMaps());

        if (!this->m_sourceInfo.pSpanSequence)
        {TRACE_IT(33862);
            this->m_sourceInfo.pSpanSequence = HeapNew(SmallSpanSequence);
        }

        this->m_sourceInfo.pSpanSequence->RecordARange(iter, data);
    }

    void
    FunctionBody::RecordStatementAdjustment(uint offset, StatementAdjustmentType adjType)
    {TRACE_IT(33863);
        this->EnsureAuxStatementData();

        Recycler* recycler = this->m_scriptContext->GetRecycler();
        if (this->GetStatementAdjustmentRecords() == nullptr)
        {TRACE_IT(33864);
            m_sourceInfo.m_auxStatementData->m_statementAdjustmentRecords = RecyclerNew(recycler, StatementAdjustmentRecordList, recycler);
        }

        StatementAdjustmentRecord record(adjType, offset);
        this->GetStatementAdjustmentRecords()->Add(record); // Will copy stack value and put the copy into the container.
    }

    BOOL
    FunctionBody::GetBranchOffsetWithin(uint start, uint end, StatementAdjustmentRecord* record)
    {TRACE_IT(33865);
        Assert(start < end);

        if (!this->GetStatementAdjustmentRecords())
        {TRACE_IT(33866);
            // No Offset
            return FALSE;
        }

        int count = this->GetStatementAdjustmentRecords()->Count();
        for (int i = 0; i < count; i++)
        {TRACE_IT(33867);
            StatementAdjustmentRecord item = this->GetStatementAdjustmentRecords()->Item(i);
            if (item.GetByteCodeOffset() > start && item.GetByteCodeOffset() < end)
            {TRACE_IT(33868);
                *record = item;
                return TRUE;
            }
        }

        // No offset found in the range.
        return FALSE;
    }

    ScriptContext* EntryPointInfo::GetScriptContext()
    {TRACE_IT(33869);
        Assert(!IsCleanedUp());
        return this->library->GetScriptContext();
    }

#if DBG_DUMP | defined(VTUNE_PROFILING)
    void
    EntryPointInfo::RecordNativeMap(uint32 nativeOffset, uint32 statementIndex)
    {TRACE_IT(33870);
        int count = nativeOffsetMaps.Count();
        if (count)
        {TRACE_IT(33871);
            NativeOffsetMap* previous = &nativeOffsetMaps.Item(count-1);
            // Check if the range is still not finished.
            if (previous->nativeOffsetSpan.begin == previous->nativeOffsetSpan.end)
            {TRACE_IT(33872);
                if (previous->statementIndex == statementIndex)
                {TRACE_IT(33873);
                    // If the statement index is the same, we can continue with the previous range
                    return;
                }

                // If the range is empty, replace the previous range.
                if ((uint32)previous->nativeOffsetSpan.begin == nativeOffset)
                {TRACE_IT(33874);
                    if (statementIndex == Js::Constants::NoStatementIndex)
                    {TRACE_IT(33875);
                        nativeOffsetMaps.RemoveAtEnd();
                    }
                    else
                    {TRACE_IT(33876);
                        previous->statementIndex = statementIndex;
                    }
                    return;
                }

                // Close the previous range
                previous->nativeOffsetSpan.end = nativeOffset;
            }
        }

        if (statementIndex == Js::Constants::NoStatementIndex)
        {TRACE_IT(33877);
            // We do not explicitly record the offsets that do not map to user code.
            return;
        }

        NativeOffsetMap map;
        map.statementIndex = statementIndex;
        map.nativeOffsetSpan.begin = nativeOffset;
        map.nativeOffsetSpan.end = nativeOffset;

        nativeOffsetMaps.Add(map);
    }

#endif

    void
    FunctionBody::CopySourceInfo(ParseableFunctionInfo* originalFunctionInfo)
    {TRACE_IT(33878);
        this->FinishSourceInfo();
    }

    // When sourceInfo is complete, register this functionBody to utf8SourceInfo. This ensures we never
    // put incomplete functionBody into utf8SourceInfo map. (Previously we do it in FunctionBody constructor.
    // If an error occurs thereafter before SetSourceInfo, e.g. OOM, we'll have an incomplete functionBody
    // in utf8SourceInfo map whose source range is unknown and can't be reparsed.)
    void FunctionBody::FinishSourceInfo()
    {TRACE_IT(33879);
        this->GetUtf8SourceInfo()->SetFunctionBody(this);
    }

    RegSlot FunctionBody::GetFrameDisplayRegister() const
    {TRACE_IT(33880);
        return this->m_sourceInfo.frameDisplayRegister;
    }

    void FunctionBody::SetFrameDisplayRegister(RegSlot frameDisplayRegister)
    {TRACE_IT(33881);
        this->m_sourceInfo.frameDisplayRegister = frameDisplayRegister;
    }

    RegSlot FunctionBody::GetObjectRegister() const
    {TRACE_IT(33882);
        return this->m_sourceInfo.objectRegister;
    }

    void FunctionBody::SetObjectRegister(RegSlot objectRegister)
    {TRACE_IT(33883);
        this->m_sourceInfo.objectRegister = objectRegister;
    }

    ScopeObjectChain *FunctionBody::GetScopeObjectChain() const
    {TRACE_IT(33884);
        return this->m_sourceInfo.pScopeObjectChain;
    }

    void FunctionBody::SetScopeObjectChain(ScopeObjectChain *pScopeObjectChain)
    {TRACE_IT(33885);
        this->m_sourceInfo.pScopeObjectChain = pScopeObjectChain;
    }

    ByteBlock *FunctionBody::GetProbeBackingBlock()
    {TRACE_IT(33886);
        return this->m_sourceInfo.m_probeBackingBlock;
    }

    void FunctionBody::SetProbeBackingBlock(ByteBlock* probeBackingBlock)
    {TRACE_IT(33887);
        this->m_sourceInfo.m_probeBackingBlock = probeBackingBlock;
    }

    FunctionBody * FunctionBody::NewFromRecycler(ScriptContext * scriptContext, const char16 * displayName, uint displayNameLength, uint displayShortNameOffset, uint nestedCount,
        Utf8SourceInfo* sourceInfo, uint uScriptId, Js::LocalFunctionId functionId, Js::PropertyRecordList* boundPropertyRecords, FunctionInfo::Attributes attributes, FunctionBodyFlags flags
#ifdef PERF_COUNTERS
            , bool isDeserializedFunction
#endif
            )
    {TRACE_IT(33888);
            return FunctionBody::NewFromRecycler(scriptContext, displayName, displayNameLength, displayShortNameOffset, nestedCount, sourceInfo,
            scriptContext->GetThreadContext()->NewFunctionNumber(), uScriptId, functionId, boundPropertyRecords, attributes, flags
#ifdef PERF_COUNTERS
            , isDeserializedFunction
#endif
            );
    }

    FunctionBody * FunctionBody::NewFromRecycler(ScriptContext * scriptContext, const char16 * displayName, uint displayNameLength, uint displayShortNameOffset, uint nestedCount,
        Utf8SourceInfo* sourceInfo, uint uFunctionNumber, uint uScriptId, Js::LocalFunctionId  functionId, Js::PropertyRecordList* boundPropertyRecords, FunctionInfo::Attributes attributes, FunctionBodyFlags flags
#ifdef PERF_COUNTERS
            , bool isDeserializedFunction
#endif
            )
    {TRACE_IT(33889);
#ifdef PERF_COUNTERS
            return RecyclerNewWithBarrierFinalized(scriptContext->GetRecycler(), FunctionBody, scriptContext, displayName, displayNameLength, displayShortNameOffset, nestedCount, sourceInfo, uFunctionNumber, uScriptId, functionId, boundPropertyRecords, attributes, flags, isDeserializedFunction);
#else
            return RecyclerNewWithBarrierFinalized(scriptContext->GetRecycler(), FunctionBody, scriptContext, displayName, displayNameLength, displayShortNameOffset, nestedCount, sourceInfo, uFunctionNumber, uScriptId, functionId, boundPropertyRecords, attributes, flags);
#endif
    }

    FunctionBody *
    FunctionBody::NewFromParseableFunctionInfo(ParseableFunctionInfo * parseableFunctionInfo, PropertyRecordList * boundPropertyRecords)
    {TRACE_IT(33890);
        ScriptContext * scriptContext = parseableFunctionInfo->GetScriptContext();
        uint nestedCount = parseableFunctionInfo->GetNestedCount();

        FunctionBody * functionBody = RecyclerNewWithBarrierFinalized(scriptContext->GetRecycler(),
            FunctionBody,
            parseableFunctionInfo);
        if (!functionBody->GetBoundPropertyRecords())
        {TRACE_IT(33891);
            functionBody->SetBoundPropertyRecords(boundPropertyRecords);
        }

        // Initialize nested function array, update back pointers
        for (uint i = 0; i < nestedCount; i++)
        {TRACE_IT(33892);
            FunctionInfo * nestedInfo = parseableFunctionInfo->GetNestedFunc(i);
            functionBody->SetNestedFunc(nestedInfo, i, 0);
        }

        return functionBody;
    }

    FunctionBody::FunctionBody(ScriptContext* scriptContext, const char16* displayName, uint displayNameLength, uint displayShortNameOffset, uint nestedCount,
        Utf8SourceInfo* utf8SourceInfo, uint uFunctionNumber, uint uScriptId,
        Js::LocalFunctionId  functionId, Js::PropertyRecordList* boundPropertyRecords, FunctionInfo::Attributes attributes, FunctionBodyFlags flags
#ifdef PERF_COUNTERS
        , bool isDeserializedFunction
#endif
        ) :
        ParseableFunctionInfo(scriptContext->CurrentThunk, nestedCount, functionId, utf8SourceInfo, scriptContext, uFunctionNumber, displayName, displayNameLength, displayShortNameOffset, attributes, boundPropertyRecords, flags),
        counters(this),
        m_uScriptId(uScriptId),
        cleanedUp(false),
        sourceInfoCleanedUp(false),
        profiledLdElemCount(0),
        profiledStElemCount(0),
        profiledCallSiteCount(0),
        profiledArrayCallSiteCount(0),
        profiledDivOrRemCount(0),
        profiledSwitchCount(0),
        profiledReturnTypeCount(0),
        profiledSlotCount(0),
        m_isFuncRegistered(false),
        m_isFuncRegisteredToDiag(false),
        m_hasBailoutInstrInJittedCode(false),
        m_depth(0),
        inlineDepth(0),
        m_pendingLoopHeaderRelease(false),
        hasCachedScopePropIds(false),
        m_argUsedForBranch(0),
        m_envDepth((uint16)-1),
        interpretedCount(0),
        lastInterpretedCount(0),
        loopInterpreterLimit(CONFIG_FLAG(LoopInterpretCount)),
        savedPolymorphicCacheState(0),
        debuggerScopeIndex(0),
        m_hasFinally(false),
#if ENABLE_PROFILE_INFO
        dynamicProfileInfo(nullptr),
#endif
        savedInlinerVersion(0),
#if ENABLE_NATIVE_CODEGEN
        savedImplicitCallsFlags(ImplicitCall_HasNoInfo),
#endif
        hasExecutionDynamicProfileInfo(false),
        m_hasAllNonLocalReferenced(false),
        m_hasSetIsObject(false),
        m_hasFunExprNameReference(false),
        m_CallsEval(false),
        m_ChildCallsEval(false),
        m_hasReferenceableBuiltInArguments(false),
        m_isParamAndBodyScopeMerged(true),
        m_firstFunctionObject(true),
        m_inlineCachesOnFunctionObject(false),
        m_hasDoneAllNonLocalReferenced(false),
        m_hasFunctionCompiledSent(false),
        byteCodeCache(nullptr),
        m_hasLocalClosureRegister(false),
        m_hasParamClosureRegister(false),
        m_hasLocalFrameDisplayRegister(false),
        m_hasEnvRegister(false),
        m_hasThisRegisterForEventHandler(false),
        m_hasFirstInnerScopeRegister(false),
        m_hasFuncExprScopeRegister(false),
        m_hasFirstTmpRegister(false),
        m_hasActiveReference(false),
        m_tag31(true),
        m_tag32(true),
        m_tag33(true),
        m_nativeEntryPointUsed(FALSE),
        bailOnMisingProfileCount(0),
        bailOnMisingProfileRejitCount(0),
        byteCodeBlock(nullptr),
        entryPoints(nullptr),
        m_constTable(nullptr),
        inlineCaches(nullptr),
        cacheIdToPropertyIdMap(nullptr),
        executionMode(ExecutionMode::Interpreter),
        interpreterLimit(0),
        autoProfilingInterpreter0Limit(0),
        profilingInterpreter0Limit(0),
        autoProfilingInterpreter1Limit(0),
        simpleJitLimit(0),
        profilingInterpreter1Limit(0),
        fullJitThreshold(0),
        fullJitRequeueThreshold(0),
        committedProfiledIterations(0),
        wasCalledFromLoop(false),
        hasScopeObject(false),
        hasNestedLoop(false),
        recentlyBailedOutOfJittedLoopBody(false),
        m_isAsmJsScheduledForFullJIT(false),
        m_asmJsTotalLoopCount(0)
        //
        // Even if the function does not require any locals, we must always have "R0" to propagate
        // a return value.  By enabling this here, we avoid unnecessary conditionals during execution.
        //
#ifdef IR_VIEWER
        ,m_isIRDumpEnabled(false)
        ,m_irDumpBaseObject(nullptr)
#endif /* IR_VIEWER */
        , m_isFromNativeCodeModule(false)
        , hasHotLoop(false)
        , m_isPartialDeserializedFunction(false)
#if DBG
        , m_isSerialized(false)
#endif
#ifdef PERF_COUNTERS
        , m_isDeserializedFunction(isDeserializedFunction)
#endif
#if DBG
        , m_DEBUG_executionCount(0)
        , m_nativeEntryPointIsInterpreterThunk(false)
        , m_canDoStackNestedFunc(false)
        , m_inlineCacheTypes(nullptr)
        , m_iProfileSession(-1)
        , initializedExecutionModeAndLimits(false)
#endif
#if ENABLE_DEBUG_CONFIG_OPTIONS
        , regAllocLoadCount(0)
        , regAllocStoreCount(0)
        , callCountStats(0)
#endif
    {
        SetCountField(CounterFields::ConstantCount, 1);

        this->SetDefaultFunctionEntryPointInfo((FunctionEntryPointInfo*) this->GetDefaultEntryPointInfo(), DefaultEntryThunk);
        this->m_hasBeenParsed = true;

#ifdef PERF_COUNTERS
        if (isDeserializedFunction)
        {
            PERF_COUNTER_INC(Code, DeserializedFunctionBody);
        }
#endif
        Assert(!utf8SourceInfo || m_uScriptId == utf8SourceInfo->GetSrcInfo()->sourceContextInfo->sourceContextId);

        // Sync entryPoints changes to etw rundown lock
        CriticalSection* syncObj = scriptContext->GetThreadContext()->GetEtwRundownCriticalSection();
        this->entryPoints = RecyclerNew(this->m_scriptContext->GetRecycler(), FunctionEntryPointList, this->m_scriptContext->GetRecycler(), syncObj);

        this->AddEntryPointToEntryPointList(this->GetDefaultFunctionEntryPointInfo());

        Assert(this->GetDefaultEntryPointInfo()->jsMethod != nullptr);

        InitDisableInlineApply();
        InitDisableInlineSpread();
    }

    FunctionBody::FunctionBody(ParseableFunctionInfo * proxy) :
        ParseableFunctionInfo(proxy),
        counters(this),
        m_uScriptId(proxy->GetUtf8SourceInfo()->GetSrcInfo()->sourceContextInfo->sourceContextId),
        cleanedUp(false),
        sourceInfoCleanedUp(false),
        profiledLdElemCount(0),
        profiledStElemCount(0),
        profiledCallSiteCount(0),
        profiledArrayCallSiteCount(0),
        profiledDivOrRemCount(0),
        profiledSwitchCount(0),
        profiledReturnTypeCount(0),
        profiledSlotCount(0),
        m_isFuncRegistered(false),
        m_isFuncRegisteredToDiag(false),
        m_hasBailoutInstrInJittedCode(false),
        m_depth(0),
        inlineDepth(0),
        m_pendingLoopHeaderRelease(false),
        hasCachedScopePropIds(false),
        m_argUsedForBranch(0),
        m_envDepth((uint16)-1),
        interpretedCount(0),
        lastInterpretedCount(0),
        loopInterpreterLimit(CONFIG_FLAG(LoopInterpretCount)),
        savedPolymorphicCacheState(0),
        debuggerScopeIndex(0),
        m_hasFinally(false),
#if ENABLE_PROFILE_INFO
        dynamicProfileInfo(nullptr),
#endif
        savedInlinerVersion(0),
#if ENABLE_NATIVE_CODEGEN
        savedImplicitCallsFlags(ImplicitCall_HasNoInfo),
#endif
        hasExecutionDynamicProfileInfo(false),
        m_hasAllNonLocalReferenced(false),
        m_hasSetIsObject(false),
        m_hasFunExprNameReference(false),
        m_CallsEval(false),
        m_ChildCallsEval(false),
        m_hasReferenceableBuiltInArguments(false),
        m_isParamAndBodyScopeMerged(true),
        m_firstFunctionObject(true),
        m_inlineCachesOnFunctionObject(false),
        m_hasDoneAllNonLocalReferenced(false),
        m_hasFunctionCompiledSent(false),
        byteCodeCache(nullptr),
        m_hasLocalClosureRegister(false),
        m_hasParamClosureRegister(false),
        m_hasLocalFrameDisplayRegister(false),
        m_hasEnvRegister(false),
        m_hasThisRegisterForEventHandler(false),
        m_hasFirstInnerScopeRegister(false),
        m_hasFuncExprScopeRegister(false),
        m_hasFirstTmpRegister(false),
        m_hasActiveReference(false),
        m_tag31(true),
        m_tag32(true),
        m_tag33(true),
        m_nativeEntryPointUsed(false),
        bailOnMisingProfileCount(0),
        bailOnMisingProfileRejitCount(0),
        byteCodeBlock(nullptr),
        entryPoints(nullptr),
        m_constTable(nullptr),
        inlineCaches(nullptr),
        cacheIdToPropertyIdMap(nullptr),
        executionMode(ExecutionMode::Interpreter),
        interpreterLimit(0),
        autoProfilingInterpreter0Limit(0),
        profilingInterpreter0Limit(0),
        autoProfilingInterpreter1Limit(0),
        simpleJitLimit(0),
        profilingInterpreter1Limit(0),
        fullJitThreshold(0),
        fullJitRequeueThreshold(0),
        committedProfiledIterations(0),
        wasCalledFromLoop(false),
        hasScopeObject(false),
        hasNestedLoop(false),
        recentlyBailedOutOfJittedLoopBody(false),
        m_isAsmJsScheduledForFullJIT(false),
        m_asmJsTotalLoopCount(0)
        //
        // Even if the function does not require any locals, we must always have "R0" to propagate
        // a return value.  By enabling this here, we avoid unnecessary conditionals during execution.
        //
#ifdef IR_VIEWER
        ,m_isIRDumpEnabled(false)
        ,m_irDumpBaseObject(nullptr)
#endif /* IR_VIEWER */
        , m_isFromNativeCodeModule(false)
        , hasHotLoop(false)
        , m_isPartialDeserializedFunction(false)
#if DBG
        , m_isSerialized(false)
#endif
#ifdef PERF_COUNTERS
        , m_isDeserializedFunction(false)
#endif
#if DBG
        , m_DEBUG_executionCount(0)
        , m_nativeEntryPointIsInterpreterThunk(false)
        , m_canDoStackNestedFunc(false)
        , m_inlineCacheTypes(nullptr)
        , m_iProfileSession(-1)
        , initializedExecutionModeAndLimits(false)
#endif
#if ENABLE_DEBUG_CONFIG_OPTIONS
        , regAllocLoadCount(0)
        , regAllocStoreCount(0)
        , callCountStats(0)
#endif
    {
        ScriptContext * scriptContext = proxy->GetScriptContext();

        SetCountField(CounterFields::ConstantCount, 1);

        proxy->UpdateFunctionBodyImpl(this);
        this->SetDeferredStubs(proxy->GetDeferredStubs());

        void* validationCookie = nullptr;

#if ENABLE_NATIVE_CODEGEN
        validationCookie = (void*)scriptContext->GetNativeCodeGenerator();
#endif

        this->m_defaultEntryPointInfo = RecyclerNewFinalized(scriptContext->GetRecycler(),
            FunctionEntryPointInfo, this, scriptContext->CurrentThunk, scriptContext->GetThreadContext(), validationCookie);

        this->SetDefaultFunctionEntryPointInfo((FunctionEntryPointInfo*) this->GetDefaultEntryPointInfo(), DefaultEntryThunk);
        this->m_hasBeenParsed = true;

        Assert(!proxy->GetUtf8SourceInfo() || m_uScriptId == proxy->GetUtf8SourceInfo()->GetSrcInfo()->sourceContextInfo->sourceContextId);

        // Sync entryPoints changes to etw rundown lock
        CriticalSection* syncObj = scriptContext->GetThreadContext()->GetEtwRundownCriticalSection();
        this->entryPoints = RecyclerNew(scriptContext->GetRecycler(), FunctionEntryPointList, scriptContext->GetRecycler(), syncObj);

        this->AddEntryPointToEntryPointList(this->GetDefaultFunctionEntryPointInfo());

        Assert(this->GetDefaultEntryPointInfo()->jsMethod != nullptr);

        InitDisableInlineApply();
        InitDisableInlineSpread();
    }

    bool FunctionBody::InterpretedSinceCallCountCollection() const
    {TRACE_IT(33893);
        return this->interpretedCount != this->lastInterpretedCount;
    }

    void FunctionBody::CollectInterpretedCounts()
    {TRACE_IT(33894);
        this->lastInterpretedCount = this->interpretedCount;
    }

    void FunctionBody::IncrInactiveCount(uint increment)
    {TRACE_IT(33895);
        this->inactiveCount = UInt32Math::Add(this->inactiveCount, increment);
    }

    bool FunctionBody::IsActiveFunction(ActiveFunctionSet * pActiveFuncs) const
    {TRACE_IT(33896);
        return !!pActiveFuncs->Test(this->GetFunctionNumber());
    }

    bool FunctionBody::TestAndUpdateActiveFunctions(ActiveFunctionSet * pActiveFuncs) const
    {TRACE_IT(33897);
        return !!pActiveFuncs->TestAndSet(this->GetFunctionNumber());
    }

    void FunctionBody::UpdateActiveFunctionsForOneDataSet(ActiveFunctionSet *pActiveFuncs, FunctionCodeGenRuntimeData *parentData, Field(FunctionCodeGenRuntimeData*)* dataSet, uint count) const
    {TRACE_IT(33898);
        FunctionCodeGenRuntimeData *inlineeData;
        for (uint i = 0; i < count; i++)
        {TRACE_IT(33899);
            for (inlineeData = dataSet[i]; inlineeData; inlineeData = inlineeData->GetNext())
            {TRACE_IT(33900);
                // inlineeData == parentData indicates a cycle in the structure. We've already processed parentData, so don't descend.
                if (inlineeData != parentData)
                {TRACE_IT(33901);
                    inlineeData->GetFunctionBody()->UpdateActiveFunctionSet(pActiveFuncs, inlineeData);
                }
            }
        }
    }

    void FunctionBody::UpdateActiveFunctionSet(ActiveFunctionSet *pActiveFuncs, FunctionCodeGenRuntimeData *callSiteData) const
    {TRACE_IT(33902);
        // Always walk the inlinee and ldFldInlinee data (if we have them), as they are different at each call site.
        if (callSiteData)
        {TRACE_IT(33903);
            if (callSiteData->GetInlinees())
            {TRACE_IT(33904);
                this->UpdateActiveFunctionsForOneDataSet(pActiveFuncs, callSiteData, callSiteData->GetInlinees(), this->GetProfiledCallSiteCount());
            }
            if (callSiteData->GetLdFldInlinees())
            {TRACE_IT(33905);
                this->UpdateActiveFunctionsForOneDataSet(pActiveFuncs, callSiteData, callSiteData->GetLdFldInlinees(), this->GetInlineCacheCount());
            }
        }

        // Now walk the top-level data, but only do it once, since it's always the same.

        if (this->TestAndUpdateActiveFunctions(pActiveFuncs))
        {TRACE_IT(33906);
            return;
        }
        {
            Field(FunctionCodeGenRuntimeData*)* data = this->GetCodeGenRuntimeData();
            if (data != nullptr)
            {TRACE_IT(33907);
                this->UpdateActiveFunctionsForOneDataSet(pActiveFuncs, nullptr, data, this->GetProfiledCallSiteCount());
            }
        }
        {TRACE_IT(33908);
            Field(FunctionCodeGenRuntimeData*)* data = this->GetCodeGenGetSetRuntimeData();
            if (data != nullptr)
            {TRACE_IT(33909);
                this->UpdateActiveFunctionsForOneDataSet(pActiveFuncs, nullptr, data, this->GetInlineCacheCount());
            }
        }
    }

    bool FunctionBody::DoRedeferFunction(uint inactiveThreshold) const
    {TRACE_IT(33910);
        if (!(this->GetFunctionInfo()->GetFunctionProxy() == this &&
              this->CanBeDeferred() &&
              this->GetByteCode() &&
              this->GetCanDefer()))
        {TRACE_IT(33911);
            return false;
        }

        if (!PHASE_FORCE(Js::RedeferralPhase, this) && !PHASE_STRESS(Js::RedeferralPhase, this))
        {TRACE_IT(33912);
            uint tmpThreshold;
            auto fn = [&](){ tmpThreshold = 0xFFFFFFFF; };
            tmpThreshold = UInt32Math::Mul(inactiveThreshold, this->GetCompileCount(), fn);
            if (this->GetInactiveCount() < tmpThreshold)
            {TRACE_IT(33913);
                return false;
            }
        }

        // Make sure the function won't be jitted
        bool isJitModeFunction = !this->IsInterpreterExecutionMode();
        bool isJitCandidate = false;
        isJitCandidate = MapEntryPointsUntil([=](int index, FunctionEntryPointInfo *entryPointInfo)
        {
            if ((entryPointInfo->IsCodeGenPending() && isJitModeFunction) || entryPointInfo->IsCodeGenQueued() || entryPointInfo->IsCodeGenRecorded() || (entryPointInfo->IsCodeGenDone() && !entryPointInfo->nativeEntryPointProcessed))
            {TRACE_IT(33914);
                return true;
            }
            return false;
        });

        if (!isJitCandidate)
        {TRACE_IT(33915);
            // Now check loop body entry points
            isJitCandidate = MapLoopHeadersUntil([=](uint loopNumber, LoopHeader* header)
            {
                return header->MapEntryPointsUntil([&](int index, LoopEntryPointInfo* entryPointInfo)
                {
                    if (entryPointInfo->IsCodeGenPending() || entryPointInfo->IsCodeGenQueued() || entryPointInfo->IsCodeGenRecorded() || (entryPointInfo->IsCodeGenDone() && !entryPointInfo->nativeEntryPointProcessed))
                    {TRACE_IT(33916);
                        return true;
                    }
                    return false;
                });
            });
        }

        return !isJitCandidate;
    }

    void FunctionBody::RedeferFunction()
    {TRACE_IT(33917);
        Assert(this->CanBeDeferred());

#if DBG
        if (PHASE_STATS(RedeferralPhase, this))
        {TRACE_IT(33918);
            ThreadContext * threadContext = this->GetScriptContext()->GetThreadContext();
            threadContext->redeferredFunctions++;
            threadContext->recoveredBytes += sizeof(*this) + this->GetInlineCacheCount() * sizeof(InlineCache);
            if (this->byteCodeBlock)
            {TRACE_IT(33919);
                threadContext->recoveredBytes += this->byteCodeBlock->GetLength();
                if (this->GetAuxiliaryData())
                {TRACE_IT(33920);
                    threadContext->recoveredBytes += this->GetAuxiliaryData()->GetLength();
                }
            }
            this->MapEntryPoints([&](int index, FunctionEntryPointInfo * info) {
                threadContext->recoveredBytes += sizeof(info);
            });

            // TODO: Get size of polymorphic caches, jitted code, etc.
        }

        // We can't get here if the function is being jitted. Jitting was either completed or not begun.
        this->counters.bgThreadCallStarted = false;
#endif

        PHASE_PRINT_TRACE(Js::RedeferralPhase, this, _u("Redeferring function %d.%d: %s\n"),
                          GetSourceContextId(), GetLocalFunctionId(),
                          GetDisplayName() ? GetDisplayName() : _u("Anonymous function)"));

        ParseableFunctionInfo * parseableFunctionInfo =
            Js::ParseableFunctionInfo::NewDeferredFunctionFromFunctionBody(this);
        FunctionInfo * functionInfo = this->GetFunctionInfo();

        this->MapFunctionObjectTypes([&](DynamicType* type)
        {
            Assert(type->GetTypeId() == TypeIds_Function);

            ScriptFunctionType* functionType = (ScriptFunctionType*)type;
            if (!CrossSite::IsThunk(functionType->GetEntryPoint()))
            {TRACE_IT(33921);
                functionType->SetEntryPoint(GetScriptContext()->DeferredParsingThunk);
            }
            if (!CrossSite::IsThunk(functionType->GetEntryPointInfo()->jsMethod))
            {TRACE_IT(33922);
                functionType->GetEntryPointInfo()->jsMethod = GetScriptContext()->DeferredParsingThunk;
            }
        });

        this->Cleanup(false);
        if (GetIsFuncRegistered())
        {TRACE_IT(33923);
            this->GetUtf8SourceInfo()->RemoveFunctionBody(this);
        }

        // New allocation is done at this point, so update existing structures
        // Adjust functionInfo attributes, point to new proxy
        functionInfo->SetAttributes((FunctionInfo::Attributes)(functionInfo->GetAttributes() | FunctionInfo::Attributes::DeferredParse));
        functionInfo->SetFunctionProxy(parseableFunctionInfo);
        functionInfo->SetOriginalEntryPoint(DefaultEntryThunk);
    }

    void FunctionBody::SetDefaultFunctionEntryPointInfo(FunctionEntryPointInfo* entryPointInfo, const JavascriptMethod originalEntryPoint)
    {TRACE_IT(33924);
        Assert(entryPointInfo);

        // Need to set twice since ProxyEntryPointInfo cast points to an interior pointer
        this->m_defaultEntryPointInfo = (ProxyEntryPointInfo*) entryPointInfo;
        this->defaultFunctionEntryPointInfo = entryPointInfo;
        SetOriginalEntryPoint(originalEntryPoint);
    }

    Var
    FunctionBody::GetFormalsPropIdArrayOrNullObj()
    {TRACE_IT(33925);
        Var formalsPropIdArray = this->GetAuxPtrWithLock(AuxPointerType::FormalsPropIdArray);
        if (formalsPropIdArray == nullptr)
        {TRACE_IT(33926);
            return GetScriptContext()->GetLibrary()->GetNull();
        }
        return formalsPropIdArray;
    }

    PropertyIdArray*
    FunctionBody::GetFormalsPropIdArray(bool checkForNull)
    {TRACE_IT(33927);
        if (checkForNull)
        {TRACE_IT(33928);
            Assert(this->GetAuxPtrWithLock(AuxPointerType::FormalsPropIdArray));
        }
        return static_cast<PropertyIdArray*>(this->GetAuxPtrWithLock(AuxPointerType::FormalsPropIdArray));
    }

    void
    FunctionBody::SetFormalsPropIdArray(PropertyIdArray * propIdArray)
    {TRACE_IT(33929);
        AssertMsg(propIdArray == nullptr || this->GetAuxPtrWithLock(AuxPointerType::FormalsPropIdArray) == nullptr, "Already set?");
        this->SetAuxPtr(AuxPointerType::FormalsPropIdArray, propIdArray);
    }

    ByteBlock*
    FunctionBody::GetByteCode() const
    {TRACE_IT(33930);
        return this->byteCodeBlock;
    }

    // Returns original bytecode without probes (such as BPs).
    ByteBlock*
    FunctionBody::GetOriginalByteCode()
    {TRACE_IT(33931);
        if (m_sourceInfo.m_probeBackingBlock)
        {TRACE_IT(33932);
            return m_sourceInfo.m_probeBackingBlock;
        }
        else
        {TRACE_IT(33933);
            return this->GetByteCode();
        }
    }

    const char16* ParseableFunctionInfo::GetExternalDisplayName() const
    {TRACE_IT(33934);
        return GetExternalDisplayName(this);
    }

    RegSlot
    FunctionBody::GetLocalsCount()
    {TRACE_IT(33935);
        return GetConstantCount() + GetVarCount();
    }

    RegSlot
    FunctionBody::GetVarCount()
    {TRACE_IT(33936);
        return this->GetCountField(CounterFields::VarCount);
    }

    // Returns the number of non-temp local vars.
    uint32
    FunctionBody::GetNonTempLocalVarCount()
    {TRACE_IT(33937);
        Assert(this->GetEndNonTempLocalIndex() >= this->GetFirstNonTempLocalIndex());
        return this->GetEndNonTempLocalIndex() - this->GetFirstNonTempLocalIndex();
    }

    uint32
    FunctionBody::GetFirstNonTempLocalIndex()
    {TRACE_IT(33938);
        // First local var starts when the const vars end.
        return GetConstantCount();
    }

    uint32
    FunctionBody::GetEndNonTempLocalIndex()
    {TRACE_IT(33939);
        // It will give the index on which current non temp locals ends, which is a first temp reg.
        RegSlot firstTmpReg = GetFirstTmpRegister();
        return firstTmpReg != Constants::NoRegister ? firstTmpReg : GetLocalsCount();
    }

    bool
    FunctionBody::IsNonTempLocalVar(uint32 varIndex)
    {TRACE_IT(33940);
        return GetFirstNonTempLocalIndex() <= varIndex && varIndex < GetEndNonTempLocalIndex();
    }

    bool
    FunctionBody::GetSlotOffset(RegSlot slotId, int32 * slotOffset, bool allowTemp)
    {TRACE_IT(33941);
        if (IsNonTempLocalVar(slotId) || allowTemp)
        {TRACE_IT(33942);
            *slotOffset = (slotId - GetFirstNonTempLocalIndex()) * DIAGLOCALSLOTSIZE;
            return true;
        }
        return false;
    }

    void
    FunctionBody::CheckAndSetConstantCount(RegSlot cNewConstants) // New register count
    {TRACE_IT(33943);
        CheckNotExecuting();
        AssertMsg(GetConstantCount() <= cNewConstants, "Cannot shrink register usage");

        this->SetConstantCount(cNewConstants);
    }
    void
    FunctionBody::SetConstantCount(RegSlot cNewConstants) // New register count
    {TRACE_IT(33944);
        this->SetCountField(CounterFields::ConstantCount, cNewConstants);
    }
    void
    FunctionBody::CheckAndSetVarCount(RegSlot cNewVars)
    {TRACE_IT(33945);
        CheckNotExecuting();
        AssertMsg(this->GetVarCount() <= cNewVars, "Cannot shrink register usage");
        this->SetVarCount(cNewVars);
    }
    void
    FunctionBody::SetVarCount(RegSlot cNewVars) // New register count
    {TRACE_IT(33946);
        this->SetCountField(FunctionBody::CounterFields::VarCount, cNewVars);
    }

    RegSlot
    FunctionBody::GetYieldRegister()
    {TRACE_IT(33947);
        return GetEndNonTempLocalIndex() - 1;
    }

    RegSlot
    FunctionBody::GetFirstTmpReg()
    {TRACE_IT(33948);
        AssertMsg(GetFirstTmpRegister() != Constants::NoRegister, "First temp hasn't been set yet");
        return GetFirstTmpRegister();
    }

    void
    FunctionBody::SetFirstTmpReg(
        RegSlot firstTmpReg)
    {TRACE_IT(33949);
        CheckNotExecuting();
        AssertMsg(GetFirstTmpRegister() == Constants::NoRegister, "Should not be resetting the first temp");

        SetFirstTmpRegister(firstTmpReg);
    }

    RegSlot
    FunctionBody::GetTempCount()
    {TRACE_IT(33950);
        return GetLocalsCount() - GetFirstTmpRegister();
    }

    void
    FunctionBody::SetOutParamMaxDepth(RegSlot cOutParamsDepth)
    {TRACE_IT(33951);
        SetCountField(CounterFields::OutParamMaxDepth, cOutParamsDepth);
    }

    void
    FunctionBody::CheckAndSetOutParamMaxDepth(RegSlot cOutParamsDepth)
    {TRACE_IT(33952);
        CheckNotExecuting();
        SetOutParamMaxDepth(cOutParamsDepth);
    }

    RegSlot
    FunctionBody::GetOutParamMaxDepth()
    {TRACE_IT(33953);
        return GetCountField(CounterFields::OutParamMaxDepth);
    }

    ModuleID
    FunctionBody::GetModuleID() const
    {TRACE_IT(33954);
        return this->GetHostSrcInfo()->moduleID;
    }

    ///----------------------------------------------------------------------------
    ///
    /// FunctionBody::BeginExecution
    ///
    /// BeginExecution() is called by InterpreterStackFrame when a function begins execution.
    /// - Once started execution, the function may not be modified, as it would
    ///   change the stack-frame layout:
    /// - This is a debug-only check because of the runtime cost.  At release time,
    ///   a stack-walk will be performed by GC to determine which functions are
    ///   executing.
    ///
    ///----------------------------------------------------------------------------

    void
    FunctionBody::BeginExecution()
    {TRACE_IT(33955);
#if DBG
        m_DEBUG_executionCount++;
#endif
        // Don't allow loop headers to be released while the function is executing
        ::InterlockedIncrement(&this->m_depth);
    }


    ///----------------------------------------------------------------------------
    ///
    /// FunctionBody::CheckEmpty
    ///
    /// CheckEmpty() validates that the given instance has not been given an
    /// implementation yet.
    ///
    ///----------------------------------------------------------------------------

    void
    FunctionBody::CheckEmpty()
    {TRACE_IT(33956);
        AssertMsg((this->byteCodeBlock == nullptr) && (this->GetAuxiliaryData() == nullptr) && (this->GetAuxiliaryContextData() == nullptr), "Function body may only be set once");
    }


    ///----------------------------------------------------------------------------
    ///
    /// FunctionBody::CheckNotExecuting
    ///
    /// CheckNotExecuting() checks that function is not currently executing when it
    /// is being modified.  See BeginExecution() for details.
    ///
    ///----------------------------------------------------------------------------

    void
    FunctionBody::CheckNotExecuting()
    {TRACE_IT(33957);
        AssertMsg(m_DEBUG_executionCount == 0, "Function cannot be executing when modified");
    }

    ///----------------------------------------------------------------------------
    ///
    /// FunctionBody::EndExecution
    ///
    /// EndExecution() is called by InterpreterStackFrame when a function ends execution.
    /// See BeginExecution() for details.
    ///
    ///----------------------------------------------------------------------------

    void
    FunctionBody::EndExecution()
    {TRACE_IT(33958);
#if DBG
        AssertMsg(m_DEBUG_executionCount > 0, "Must have a previous execution to end");

        m_DEBUG_executionCount--;
#endif
        uint depth = ::InterlockedDecrement(&this->m_depth);

        // If loop headers were determined to be no longer needed
        // during the execution of the function, we release them now
        if (depth == 0 && this->m_pendingLoopHeaderRelease)
        {TRACE_IT(33959);
            this->m_pendingLoopHeaderRelease = false;
            ReleaseLoopHeaders();
        }
    }

    void FunctionBody::AddEntryPointToEntryPointList(FunctionEntryPointInfo* entryPointInfo)
    {TRACE_IT(33960);
        ThreadContext::AutoDisableExpiration disableExpiration(this->m_scriptContext->GetThreadContext());

        Recycler* recycler = this->m_scriptContext->GetRecycler();
        entryPointInfo->entryPointIndex = this->entryPoints->Add(recycler->CreateWeakReferenceHandle(entryPointInfo));
    }

#if DBG
    BOOL FunctionBody::IsInterpreterThunk() const
    {TRACE_IT(33961);
        bool isInterpreterThunk = this->GetOriginalEntryPoint_Unchecked() == DefaultEntryThunk;
#if DYNAMIC_INTERPRETER_THUNK
        isInterpreterThunk = isInterpreterThunk || IsDynamicInterpreterThunk();
#endif
        return isInterpreterThunk;
    }

    BOOL FunctionBody::IsDynamicInterpreterThunk() const
    {TRACE_IT(33962);
#if DYNAMIC_INTERPRETER_THUNK
        return this->GetScriptContext()->IsDynamicInterpreterThunk(this->GetOriginalEntryPoint_Unchecked());
#else
        return FALSE;
#endif
    }
#endif

    FunctionEntryPointInfo * FunctionBody::TryGetEntryPointInfo(int index) const
    {TRACE_IT(33963);
        // If we've already freed the recyclable data, we're shutting down the script context so skip clean up
        if (this->entryPoints == nullptr) return 0;

        Assert(index < this->entryPoints->Count());
        FunctionEntryPointInfo* entryPoint = this->entryPoints->Item(index)->Get();

        return entryPoint;
    }

    FunctionEntryPointInfo * FunctionBody::GetEntryPointInfo(int index) const
    {TRACE_IT(33964);
        FunctionEntryPointInfo* entryPoint = TryGetEntryPointInfo(index);
        Assert(entryPoint);

        return entryPoint;
    }

    uint32 FunctionBody::GetFrameHeight(EntryPointInfo* entryPointInfo) const
    {TRACE_IT(33965);
        return entryPointInfo->frameHeight;
    }

    void FunctionBody::SetFrameHeight(EntryPointInfo* entryPointInfo, uint32 frameHeight)
    {TRACE_IT(33966);
        entryPointInfo->frameHeight = frameHeight;
    }

#if ENABLE_NATIVE_CODEGEN
    void
    FunctionBody::SetNativeThrowSpanSequence(SmallSpanSequence *seq, uint loopNum, LoopEntryPointInfo* entryPoint)
    {TRACE_IT(33967);
        Assert(loopNum != LoopHeader::NoLoop);
        LoopHeader *loopHeader = this->GetLoopHeaderWithLock(loopNum);
        Assert(loopHeader);
        Assert(entryPoint->loopHeader == loopHeader);

        entryPoint->SetNativeThrowSpanSequence(seq);
    }

    void
    FunctionBody::RecordNativeThrowMap(SmallSpanSequenceIter& iter, uint32 nativeOffset, uint32 statementIndex, EntryPointInfo* entryPoint, uint loopNum)
    {TRACE_IT(33968);
        SmallSpanSequence *pSpanSequence;

        pSpanSequence = entryPoint->GetNativeThrowSpanSequence();

        if (!pSpanSequence)
        {TRACE_IT(33969);
            if (statementIndex == -1)
            {TRACE_IT(33970);
                return; // No need to initialize native throw map for non-user code
            }

            pSpanSequence = HeapNew(SmallSpanSequence);
            if (loopNum == LoopHeader::NoLoop)
            {TRACE_IT(33971);
                ((FunctionEntryPointInfo*) entryPoint)->SetNativeThrowSpanSequence(pSpanSequence);
            }
            else
            {TRACE_IT(33972);
                this->SetNativeThrowSpanSequence(pSpanSequence, loopNum, (LoopEntryPointInfo*) entryPoint);
            }
        }
        else if (iter.accumulatedSourceBegin == static_cast<int>(statementIndex))
        {TRACE_IT(33973);
            return; // Compress adjacent spans which share the same statementIndex
        }

        StatementData data;
        data.sourceBegin = static_cast<int>(statementIndex); // sourceBegin represents statementIndex here
        data.bytecodeBegin = static_cast<int>(nativeOffset); // bytecodeBegin represents nativeOffset here

        pSpanSequence->RecordARange(iter, &data);
    }
#endif

    bool
    ParseableFunctionInfo::IsTrackedPropertyId(PropertyId pid)
    {TRACE_IT(33974);
        Assert(this->GetBoundPropertyRecords() != nullptr);

        PropertyRecordList* trackedProperties = this->GetBoundPropertyRecords();
        const PropertyRecord* prop = nullptr;
        if (trackedProperties->TryGetValue(pid, &prop))
        {TRACE_IT(33975);
            Assert(prop != nullptr);

            return true;
        }

        return this->m_scriptContext->IsTrackedPropertyId(pid);
    }

    PropertyId
    ParseableFunctionInfo::GetOrAddPropertyIdTracked(JsUtil::CharacterBuffer<WCHAR> const& propName)
    {TRACE_IT(33976);
        Assert(this->GetBoundPropertyRecords() != nullptr);

        const Js::PropertyRecord* propRecord = nullptr;

        this->m_scriptContext->GetOrAddPropertyRecord(propName, &propRecord);

        PropertyId pid = propRecord->GetPropertyId();
        this->GetBoundPropertyRecords()->Item(pid, propRecord);

        return pid;
    }

    SmallSpanSequence::SmallSpanSequence()
        : pStatementBuffer(nullptr),
        pActualOffsetList(nullptr),
        baseValue(0)
    {TRACE_IT(33977);
    }

    BOOL SmallSpanSequence::RecordARange(SmallSpanSequenceIter &iter, StatementData * data)
    {TRACE_IT(33978);
        Assert(data);

        if (!this->pStatementBuffer)
        {TRACE_IT(33979);
            this->pStatementBuffer = JsUtil::GrowingUint32HeapArray::Create(4);
            baseValue = data->sourceBegin;
            Reset(iter);
        }

        SmallSpan span(0);

        span.sourceBegin = GetDiff(data->sourceBegin, iter.accumulatedSourceBegin);
        span.bytecodeBegin = GetDiff(data->bytecodeBegin, iter.accumulatedBytecodeBegin);

        this->pStatementBuffer->Add((uint32)span);

        // Update iterator for the next set

        iter.accumulatedSourceBegin = data->sourceBegin;
        iter.accumulatedBytecodeBegin = data->bytecodeBegin;

        return TRUE;
    }

    // FunctionProxy methods
    ScriptContext*
    FunctionProxy::GetScriptContext() const
    {
        return m_scriptContext;
    }

    void FunctionProxy::Copy(FunctionProxy* other)
    {TRACE_IT(33981);
        Assert(other);

        other->SetIsTopLevel(this->m_isTopLevel);

        if (this->IsPublicLibraryCode())
        {TRACE_IT(33982);
            other->SetIsPublicLibraryCode();
        }
    }

    void ParseableFunctionInfo::Copy(ParseableFunctionInfo * other)
    {TRACE_IT(33983);
#define CopyDeferParseField(field) other->field = this->field;
        CopyDeferParseField(flags);
        CopyDeferParseField(m_isDeclaration);
        CopyDeferParseField(m_isAccessor);
        CopyDeferParseField(m_isStrictMode);
        CopyDeferParseField(m_isGlobalFunc);
        CopyDeferParseField(m_doBackendArgumentsOptimization);
        CopyDeferParseField(m_doScopeObjectCreation);
        CopyDeferParseField(m_usesArgumentsObject);
        CopyDeferParseField(m_isEval);
        CopyDeferParseField(m_isDynamicFunction);
        CopyDeferParseField(m_hasImplicitArgIns);
        CopyDeferParseField(m_dontInline);
        CopyDeferParseField(m_inParamCount);
        CopyDeferParseField(m_grfscr);
        other->SetScopeInfo(this->GetScopeInfo());
        CopyDeferParseField(m_utf8SourceHasBeenSet);
#if DBG
        CopyDeferParseField(deferredParseNextFunctionId);
        CopyDeferParseField(scopeObjectSize);
#endif
        CopyDeferParseField(scopeSlotArraySize);
        CopyDeferParseField(paramScopeSlotArraySize);
        other->SetCachedSourceString(this->GetCachedSourceString());
        CopyDeferParseField(m_isAsmjsMode);
        CopyDeferParseField(m_isAsmJsFunction);

        other->SetFunctionObjectTypeList(this->GetFunctionObjectTypeList());

        CopyDeferParseField(m_sourceIndex);
        CopyDeferParseField(m_cchStartOffset);
        CopyDeferParseField(m_cchLength);
        CopyDeferParseField(m_lineNumber);
        CopyDeferParseField(m_columnNumber);
        CopyDeferParseField(m_cbStartOffset);
        CopyDeferParseField(m_cbLength);

        this->CopyNestedArray(other);
#undef CopyDeferParseField
   }

    void ParseableFunctionInfo::Copy(FunctionBody* other)
    {TRACE_IT(33984);
        this->Copy(static_cast<ParseableFunctionInfo*>(other));
        other->CopySourceInfo(this);
    }

    void ParseableFunctionInfo::CopyNestedArray(ParseableFunctionInfo * other)
    {TRACE_IT(33985);
        NestedArray * thisNestedArray = this->GetNestedArray();
        NestedArray * otherNestedArray = other->GetNestedArray();
        if (thisNestedArray == nullptr)
        {TRACE_IT(33986);
            Assert(otherNestedArray == nullptr);
            return;
        }
        Assert(otherNestedArray->nestedCount == thisNestedArray->nestedCount);

        for (uint i = 0; i < thisNestedArray->nestedCount; i++)
        {TRACE_IT(33987);
            otherNestedArray->functionInfoArray[i] = thisNestedArray->functionInfoArray[i];
        }
    }

    // DeferDeserializeFunctionInfo methods

    DeferDeserializeFunctionInfo::DeferDeserializeFunctionInfo(int nestedCount, LocalFunctionId functionId, ByteCodeCache* byteCodeCache, const byte* serializedFunction, Utf8SourceInfo* sourceInfo, ScriptContext* scriptContext, uint functionNumber, const char16* displayName, uint displayNameLength, uint displayShortNameOffset, NativeModule *nativeModule, FunctionInfo::Attributes attributes) :
        FunctionProxy(scriptContext, sourceInfo, functionNumber),
        m_cache(byteCodeCache),
        m_functionBytes(serializedFunction),
        m_displayName(nullptr),
        m_displayNameLength(0),
        m_nativeModule(nativeModule)
    {TRACE_IT(33988);
        this->functionInfo = RecyclerNew(scriptContext->GetRecycler(), FunctionInfo, DefaultDeferredDeserializeThunk, (FunctionInfo::Attributes)(attributes | FunctionInfo::Attributes::DeferredDeserialize), functionId, this);
        this->m_defaultEntryPointInfo = RecyclerNew(scriptContext->GetRecycler(), ProxyEntryPointInfo, DefaultDeferredDeserializeThunk);
        PERF_COUNTER_INC(Code, DeferDeserializeFunctionProxy);

        SetDisplayName(displayName, displayNameLength, displayShortNameOffset, FunctionProxy::SetDisplayNameFlagsDontCopy);
    }

    DeferDeserializeFunctionInfo* DeferDeserializeFunctionInfo::New(ScriptContext* scriptContext, int nestedCount, LocalFunctionId functionId, ByteCodeCache* byteCodeCache, const byte* serializedFunction, Utf8SourceInfo* sourceInfo, const char16* displayName, uint displayNameLength, uint displayShortNameOffset, NativeModule *nativeModule, FunctionInfo::Attributes attributes)
    {TRACE_IT(33989);
        return RecyclerNewFinalized(scriptContext->GetRecycler(),
            DeferDeserializeFunctionInfo,
            nestedCount,
            functionId,
            byteCodeCache,
            serializedFunction,
            sourceInfo,
            scriptContext,
            scriptContext->GetThreadContext()->NewFunctionNumber(),
            displayName,
            displayNameLength,
            displayShortNameOffset,
            nativeModule,
            attributes);
    }

    const char16*
    DeferDeserializeFunctionInfo::GetDisplayName() const
    {TRACE_IT(33990);
        return this->m_displayName;
    }

    // ParseableFunctionInfo methods
    ParseableFunctionInfo::ParseableFunctionInfo(JavascriptMethod entryPoint, int nestedCount,
        LocalFunctionId functionId, Utf8SourceInfo* sourceInfo, ScriptContext* scriptContext, uint functionNumber,
        const char16* displayName, uint displayNameLength, uint displayShortNameOffset, FunctionInfo::Attributes attributes, Js::PropertyRecordList* propertyRecords, FunctionBodyFlags flags) :
      FunctionProxy(scriptContext, sourceInfo, functionNumber),
#if DYNAMIC_INTERPRETER_THUNK
      m_dynamicInterpreterThunk(nullptr),
#endif
      flags(flags),
      m_hasBeenParsed(false),
      m_isGlobalFunc(false),
      m_isDeclaration(false),
      m_isNamedFunctionExpression(false),
      m_isNameIdentifierRef (true),
      m_isStaticNameFunction(false),
      m_doBackendArgumentsOptimization(true),
      m_doScopeObjectCreation(true),
      m_usesArgumentsObject(false),
      m_isStrictMode(false),
      m_isAsmjsMode(false),
      m_dontInline(false),
      m_hasImplicitArgIns(true),
      m_grfscr(0),
      m_inParamCount(0),
      m_reportedInParamCount(0),
      m_sourceIndex(Js::Constants::InvalidSourceIndex),
      m_utf8SourceHasBeenSet(false),
      m_cchLength(0),
      m_cbLength(0),
      m_cchStartOffset(0),
      m_cbStartOffset(0),
      m_lineNumber(0),
      m_columnNumber(0),
      m_isEval(false),
      m_isDynamicFunction(false),
      m_displayName(nullptr),
      m_displayNameLength(0),
      m_displayShortNameOffset(0),
      scopeSlotArraySize(0),
      paramScopeSlotArraySize(0),
      m_reparsed(false),
      m_isAsmJsFunction(false),
      m_tag21(true)
#if DBG
      ,m_wasEverAsmjsMode(false)
      ,scopeObjectSize(0)
#endif
    {
        this->functionInfo = RecyclerNew(scriptContext->GetRecycler(), FunctionInfo, entryPoint, attributes, functionId, this);

        if (nestedCount > 0)
        {TRACE_IT(33991);
            nestedArray = RecyclerNewPlusZ(m_scriptContext->GetRecycler(),
                nestedCount*sizeof(FunctionProxy*), NestedArray, nestedCount);
        }
        else
        {TRACE_IT(33992);
            nestedArray = nullptr;
        }

        SetBoundPropertyRecords(propertyRecords);
        if ((attributes & Js::FunctionInfo::DeferredParse) == 0)
        {TRACE_IT(33993);
            void* validationCookie = nullptr;

#if ENABLE_NATIVE_CODEGEN
            validationCookie = (void*)scriptContext->GetNativeCodeGenerator();
#endif

            this->m_defaultEntryPointInfo = RecyclerNewFinalized(scriptContext->GetRecycler(),
                FunctionEntryPointInfo, this, entryPoint, scriptContext->GetThreadContext(), validationCookie);
        }
        else
        {TRACE_IT(33994);
            this->m_defaultEntryPointInfo = RecyclerNew(scriptContext->GetRecycler(), ProxyEntryPointInfo, entryPoint);
        }

        SetDisplayName(displayName, displayNameLength, displayShortNameOffset);
        this->SetOriginalEntryPoint(DefaultEntryThunk);
    }

    ParseableFunctionInfo::ParseableFunctionInfo(ParseableFunctionInfo * proxy) :
      FunctionProxy(proxy->GetScriptContext(), proxy->GetUtf8SourceInfo(), proxy->GetFunctionNumber()),
#if DYNAMIC_INTERPRETER_THUNK
      m_dynamicInterpreterThunk(nullptr),
#endif
      m_hasBeenParsed(false),
      m_isNamedFunctionExpression(proxy->GetIsNamedFunctionExpression()),
      m_isNameIdentifierRef (proxy->GetIsNameIdentifierRef()),
      m_isStaticNameFunction(proxy->GetIsStaticNameFunction()),
      m_reportedInParamCount(proxy->GetReportedInParamsCount()),
      m_reparsed(proxy->IsReparsed()),
      m_tag21(true)
#if DBG
      ,m_wasEverAsmjsMode(proxy->m_wasEverAsmjsMode)
#endif
    {
        FunctionInfo * functionInfo = proxy->GetFunctionInfo();
        this->functionInfo = functionInfo;

        uint nestedCount = proxy->GetNestedCount();
        if (nestedCount > 0)
        {TRACE_IT(33995);
            nestedArray = RecyclerNewPlusZ(m_scriptContext->GetRecycler(),
                nestedCount*sizeof(FunctionProxy*), NestedArray, nestedCount);
        }
        else
        {TRACE_IT(33996);
            nestedArray = nullptr;
        }

        proxy->Copy(this);

        SetBoundPropertyRecords(proxy->GetBoundPropertyRecords());
        SetDisplayName(proxy->GetDisplayName(), proxy->GetDisplayNameLength(), proxy->GetShortDisplayNameOffset());
    }

    ParseableFunctionInfo* ParseableFunctionInfo::New(ScriptContext* scriptContext, int nestedCount,
        LocalFunctionId functionId, Utf8SourceInfo* sourceInfo, const char16* displayName, uint displayNameLength, uint displayShortNameOffset, Js::PropertyRecordList* propertyRecords, FunctionInfo::Attributes attributes, FunctionBodyFlags flags)
    {TRACE_IT(33997);
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
        Assert(
            scriptContext->DeferredParsingThunk == ProfileDeferredParsingThunk ||
            scriptContext->DeferredParsingThunk == DefaultDeferredParsingThunk);
#else
        Assert(scriptContext->DeferredParsingThunk == DefaultDeferredParsingThunk);
#endif

#ifdef PERF_COUNTERS
        PERF_COUNTER_INC(Code, DeferredFunction);
#endif
        uint newFunctionNumber = scriptContext->GetThreadContext()->NewFunctionNumber();
        if (!sourceInfo->GetSourceContextInfo()->IsDynamic())
        {TRACE_IT(33998);
            PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("Function was deferred from parsing - ID: %d; Display Name: %s; Utf8SourceInfo ID: %d; Source Length: %d; Source Url:%s\n"), newFunctionNumber, displayName, sourceInfo->GetSourceInfoId(), sourceInfo->GetCchLength(), sourceInfo->GetSourceContextInfo()->url);
        }
        else
        {TRACE_IT(33999);
            PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("Function was deferred from parsing - ID: %d; Display Name: %s; Utf8SourceInfo ID: %d; Source Length: %d;\n"), newFunctionNumber, displayName, sourceInfo->GetSourceInfoId(), sourceInfo->GetCchLength());
        }

        // When generating a new defer parse function, we always use a new function number
        return RecyclerNewWithBarrierFinalized(scriptContext->GetRecycler(),
            ParseableFunctionInfo,
            scriptContext->DeferredParsingThunk,
            nestedCount,
            functionId,
            sourceInfo,
            scriptContext,
            newFunctionNumber,
            displayName,
            displayNameLength,
            displayShortNameOffset,
            (FunctionInfo::Attributes)(attributes | FunctionInfo::Attributes::DeferredParse),
            propertyRecords,
            flags);
    }

    ParseableFunctionInfo *
    ParseableFunctionInfo::NewDeferredFunctionFromFunctionBody(FunctionBody * functionBody)
    {TRACE_IT(34000);
        ScriptContext * scriptContext = functionBody->GetScriptContext();
        uint nestedCount = functionBody->GetNestedCount();

        ParseableFunctionInfo * info = RecyclerNewWithBarrierFinalized(scriptContext->GetRecycler(),
            ParseableFunctionInfo,
            functionBody);

        // Create new entry point info
        info->m_defaultEntryPointInfo = RecyclerNew(scriptContext->GetRecycler(), ProxyEntryPointInfo, scriptContext->DeferredParsingThunk);

        // Initialize nested function array, update back pointers
        for (uint i = 0; i < nestedCount; i++)
        {TRACE_IT(34001);
            FunctionInfo * nestedInfo = functionBody->GetNestedFunc(i);
            info->SetNestedFunc(nestedInfo, i, 0);
        }

        // Update function objects

        return info;
    }

    DWORD_PTR FunctionProxy::GetSecondaryHostSourceContext() const
    {TRACE_IT(34002);
        return this->GetUtf8SourceInfo()->GetSecondaryHostSourceContext();
    }

    DWORD_PTR FunctionProxy::GetHostSourceContext() const
    {TRACE_IT(34003);
        return this->GetSourceContextInfo()->dwHostSourceContext;
    }

    SourceContextInfo * FunctionProxy::GetSourceContextInfo() const
    {TRACE_IT(34004);
        return this->GetHostSrcInfo()->sourceContextInfo;
    }

    SRCINFO const * FunctionProxy::GetHostSrcInfo() const
    {TRACE_IT(34005);
        return this->GetUtf8SourceInfo()->GetSrcInfo();
    }

    //
    // Returns the start line for the script buffer (code buffer for the entire script tag) of this current function.
    // We subtract the lnMinHost because it is the number of lines we have added to augment scriptlets passed through
    // ParseProcedureText to have a function name.
    //
    ULONG FunctionProxy::GetHostStartLine() const
    {TRACE_IT(34006);
        return this->GetHostSrcInfo()->dlnHost - this->GetHostSrcInfo()->lnMinHost;
    }

    //
    // Returns the start column of the first line for the script buffer of this current function.
    //
    ULONG FunctionProxy::GetHostStartColumn() const
    {TRACE_IT(34007);
        return this->GetHostSrcInfo()->ulColumnHost;
    }

    //
    // Returns line number in unmodified host buffer (i.e. without extra scriptlet code added by ParseProcedureText --
    // when e.g. we add extra code for event handlers, such as "function onclick(event)\n{\n").
    //
    ULONG FunctionProxy::GetLineNumberInHostBuffer(ULONG relativeLineNumber) const
    {TRACE_IT(34008);
        ULONG lineNumber = relativeLineNumber;
        if (lineNumber >= this->GetHostSrcInfo()->lnMinHost)
        {TRACE_IT(34009);
            lineNumber -= this->GetHostSrcInfo()->lnMinHost;
        }
        // Note that '<' is still a valid case -- that would be the case for onclick scriptlet function itself (lineNumber == 0).

        return lineNumber;
    }

    ULONG FunctionProxy::ComputeAbsoluteLineNumber(ULONG relativeLineNumber) const
    {TRACE_IT(34010);
        // We add 1 because the line numbers start from 0.
        return this->GetHostSrcInfo()->dlnHost + GetLineNumberInHostBuffer(relativeLineNumber) + 1;
    }

    ULONG FunctionProxy::ComputeAbsoluteColumnNumber(ULONG relativeLineNumber, ULONG relativeColumnNumber) const
    {TRACE_IT(34011);
        if (this->GetLineNumberInHostBuffer(relativeLineNumber) == 0)
        {TRACE_IT(34012);
            // The host column matters only for the first line.
            return this->GetHostStartColumn() + relativeColumnNumber + 1;
        }

        // We add 1 because the column numbers start from 0.
        return relativeColumnNumber + 1;
    }

    //
    // Returns the line number of the function declaration in the source file.
    //
    ULONG
    ParseableFunctionInfo::GetLineNumber() const
    {TRACE_IT(34013);
        return this->ComputeAbsoluteLineNumber(this->m_lineNumber);

    }

    //
    // Returns the column number of the function declaration in the source file.
    //
    ULONG
    ParseableFunctionInfo::GetColumnNumber() const
    {TRACE_IT(34014);
        return ComputeAbsoluteColumnNumber(this->m_lineNumber, m_columnNumber);
    }

    LPCWSTR
    ParseableFunctionInfo::GetSourceName() const
    {TRACE_IT(34015);
        return GetSourceName(this->GetSourceContextInfo());
    }

    void
    ParseableFunctionInfo::SetGrfscr(uint32 grfscr)
    {TRACE_IT(34016);
        this->m_grfscr = grfscr;
    }

    uint32
    ParseableFunctionInfo::GetGrfscr() const
    {TRACE_IT(34017);
        return this->m_grfscr;
    }

    ProxyEntryPointInfo*
    FunctionProxy::GetDefaultEntryPointInfo() const
    {TRACE_IT(34018);
        return this->m_defaultEntryPointInfo;
    }

    FunctionEntryPointInfo*
    FunctionBody::GetDefaultFunctionEntryPointInfo() const
    {TRACE_IT(34019);
        Assert(((ProxyEntryPointInfo*) this->defaultFunctionEntryPointInfo) == this->m_defaultEntryPointInfo);
        return this->defaultFunctionEntryPointInfo;
    }

    void
    ParseableFunctionInfo::SetInParamsCount(ArgSlot newInParamCount)
    {TRACE_IT(34020);
        AssertMsg(m_inParamCount <= newInParamCount, "Cannot shrink register usage");

        m_inParamCount = newInParamCount;

        if (newInParamCount <= 1)
        {TRACE_IT(34021);
            SetHasImplicitArgIns(false);
        }
    }

    ArgSlot
    ParseableFunctionInfo::GetReportedInParamsCount() const
    {TRACE_IT(34022);
        return m_reportedInParamCount;
    }

    void
    ParseableFunctionInfo::SetReportedInParamsCount(ArgSlot newInParamCount)
    {TRACE_IT(34023);
        AssertMsg(m_reportedInParamCount <= newInParamCount, "Cannot shrink register usage");

        m_reportedInParamCount = newInParamCount;
    }

    void
    ParseableFunctionInfo::ResetInParams()
    {TRACE_IT(34024);
        m_inParamCount = 0;
        m_reportedInParamCount = 0;
    }

    const char16*
    ParseableFunctionInfo::GetDisplayName() const
    {TRACE_IT(34025);
        return this->m_displayName;
    }

    void ParseableFunctionInfo::BuildDeferredStubs(ParseNode *pnodeFnc)
    {TRACE_IT(34026);
        Assert(pnodeFnc->nop == knopFncDecl);

        Recycler *recycler = GetScriptContext()->GetRecycler();
        this->SetDeferredStubs(BuildDeferredStubTree(pnodeFnc, recycler));
    }

    FunctionInfoArray ParseableFunctionInfo::GetNestedFuncArray()
    {TRACE_IT(34027);
        Assert(GetNestedArray() != nullptr);
        return GetNestedArray()->functionInfoArray;
    }

    void ParseableFunctionInfo::SetNestedFunc(FunctionInfo* nestedFunc, uint index, uint32 flags)
    {TRACE_IT(34028);
        AssertMsg(index < this->GetNestedCount(), "Trying to write past the nested func array");

        FunctionInfoArray nested = this->GetNestedFuncArray();
        nested[index] = nestedFunc;

        if (nestedFunc)
        {TRACE_IT(34029);
            if (!this->GetSourceContextInfo()->IsDynamic() && nestedFunc->IsDeferredParseFunction() && nestedFunc->GetParseableFunctionInfo()->GetIsDeclaration() && this->GetIsTopLevel() && !(flags & fscrEvalCode))
            {TRACE_IT(34030);
                this->GetUtf8SourceInfo()->TrackDeferredFunction(nestedFunc->GetLocalFunctionId(), nestedFunc->GetParseableFunctionInfo());
            }
        }

    }

    FunctionInfo* ParseableFunctionInfo::GetNestedFunc(uint index)
    {TRACE_IT(34031);
        return *(GetNestedFuncReference(index));
    }

    FunctionProxy* ParseableFunctionInfo::GetNestedFunctionProxy(uint index)
    {TRACE_IT(34032);
        FunctionInfo *info = GetNestedFunc(index);
        return info ? info->GetFunctionProxy() : nullptr;
    }

    FunctionInfoPtrPtr ParseableFunctionInfo::GetNestedFuncReference(uint index)
    {TRACE_IT(34033);
        AssertMsg(index < this->GetNestedCount(), "Trying to write past the nested func array");

        FunctionInfoArray nested = this->GetNestedFuncArray();
        return &nested[index];
    }

    ParseableFunctionInfo* ParseableFunctionInfo::GetNestedFunctionForExecution(uint index)
    {TRACE_IT(34034);
        FunctionInfo* currentNestedFunction = this->GetNestedFunc(index);
        Assert(currentNestedFunction);
        if (currentNestedFunction->IsDeferredDeserializeFunction())
        {TRACE_IT(34035);
            currentNestedFunction->GetFunctionProxy()->EnsureDeserialized();
        }

        return currentNestedFunction->GetParseableFunctionInfo();
    }

    void
    FunctionProxy::UpdateFunctionBodyImpl(FunctionBody * body)
    {TRACE_IT(34036);
        FunctionInfo *functionInfo = this->GetFunctionInfo();
        Assert(functionInfo->GetFunctionProxy() == this);
        Assert(!this->IsFunctionBody() || body == this);
        functionInfo->SetFunctionProxy(body);
        body->SetFunctionInfo(functionInfo);
        body->SetAttributes((FunctionInfo::Attributes)(functionInfo->GetAttributes() & ~(FunctionInfo::Attributes::DeferredParse | FunctionInfo::Attributes::DeferredDeserialize)));
    }

    //
    // This method gets a function body for the purposes of execution
    // It has an if within it to avoid making it a virtual- it's called from the interpreter
    // It will cause the function info to get deserialized if it hasn't been deserialized
    // already
    //
    ParseableFunctionInfo * FunctionProxy::EnsureDeserialized()
    {TRACE_IT(34037);
        Assert(this == this->GetFunctionInfo()->GetFunctionProxy());
        FunctionProxy * executionFunctionBody = this;

        if (IsDeferredDeserializeFunction())
        {TRACE_IT(34038);
            // No need to deserialize function body if scriptContext closed because we can't execute it.
            // Bigger problem is the script engine might have released bytecode file mapping and we can't deserialize.
            Assert(!m_scriptContext->IsClosed());

            executionFunctionBody = ((DeferDeserializeFunctionInfo*) this)->Deserialize();
            this->GetFunctionInfo()->SetFunctionProxy(executionFunctionBody);
            Assert(executionFunctionBody->GetFunctionInfo()->HasBody());
            Assert(executionFunctionBody != this);
        }

        return (ParseableFunctionInfo *)executionFunctionBody;
    }

    ScriptFunctionType * FunctionProxy::GetDeferredPrototypeType() const
    {TRACE_IT(34039);
        return deferredPrototypeType;
    }

    ScriptFunctionType * FunctionProxy::EnsureDeferredPrototypeType()
    {TRACE_IT(34040);
        Assert(this->GetFunctionInfo()->GetFunctionProxy() == this);
        return deferredPrototypeType != nullptr ?
            static_cast<ScriptFunctionType*>(deferredPrototypeType) : AllocDeferredPrototypeType();
    }

    ScriptFunctionType * FunctionProxy::AllocDeferredPrototypeType()
    {TRACE_IT(34041);
        Assert(deferredPrototypeType == nullptr);
        ScriptFunctionType * type = ScriptFunctionType::New(this, true);
        deferredPrototypeType = type;
        return type;
    }

    JavascriptMethod FunctionProxy::GetDirectEntryPoint(ProxyEntryPointInfo* entryPoint) const
    {TRACE_IT(34042);
        Assert(entryPoint->jsMethod != nullptr);
        return entryPoint->jsMethod;
    }

    // Function object type list methods
    FunctionProxy::FunctionTypeWeakRefList* FunctionProxy::GetFunctionObjectTypeList() const
    {TRACE_IT(34043);
        return static_cast<FunctionTypeWeakRefList*>(this->GetAuxPtr(AuxPointerType::FunctionObjectTypeList));
    }

    void FunctionProxy::SetFunctionObjectTypeList(FunctionProxy::FunctionTypeWeakRefList* list)
    {TRACE_IT(34044);
        this->SetAuxPtr(AuxPointerType::FunctionObjectTypeList, list);
    }

    template <typename Fn>
    void FunctionProxy::MapFunctionObjectTypes(Fn func)
    {TRACE_IT(34045);
        FunctionTypeWeakRefList* functionObjectTypeList = this->GetFunctionObjectTypeList();
        if (functionObjectTypeList != nullptr)
        {TRACE_IT(34046);
            functionObjectTypeList->Map([&](int, FunctionTypeWeakRef* typeWeakRef)
            {
                if (typeWeakRef)
                {TRACE_IT(34047);
                    DynamicType* type = typeWeakRef->Get();
                    if (type)
                    {TRACE_IT(34048);
                        func(type);
                    }
                }
            });
        }

        if (this->deferredPrototypeType)
        {TRACE_IT(34049);
            func(this->deferredPrototypeType);
        }
    }

    FunctionProxy::FunctionTypeWeakRefList* FunctionProxy::EnsureFunctionObjectTypeList()
    {TRACE_IT(34050);
        FunctionTypeWeakRefList* functionObjectTypeList = this->GetFunctionObjectTypeList();
        if (functionObjectTypeList == nullptr)
        {TRACE_IT(34051);
            Recycler* recycler = this->GetScriptContext()->GetRecycler();
            functionObjectTypeList = RecyclerNew(recycler, FunctionTypeWeakRefList, recycler);
            this->SetFunctionObjectTypeList(functionObjectTypeList);
        }

        return functionObjectTypeList;
    }

    void FunctionProxy::RegisterFunctionObjectType(DynamicType* functionType)
    {TRACE_IT(34052);
        FunctionTypeWeakRefList* typeList = EnsureFunctionObjectTypeList();

        Assert(functionType != deferredPrototypeType);
        Recycler * recycler = this->GetScriptContext()->GetRecycler();
        FunctionTypeWeakRef* weakRef = recycler->CreateWeakReferenceHandle(functionType);
        typeList->SetAtFirstFreeSpot(weakRef);
        OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Registered type 0x%p on function body %p, count = %d\n"), functionType, this, typeList->Count());
    }

    void DeferDeserializeFunctionInfo::SetDisplayName(const char16* displayName)
    {TRACE_IT(34053);
        size_t len = wcslen(displayName);
        if (len > UINT_MAX)
        {TRACE_IT(34054);
            // Can't support display name that big
            Js::Throw::OutOfMemory();
        }
        SetDisplayName(displayName, (uint)len, 0);
    }

    void DeferDeserializeFunctionInfo::SetDisplayName(const char16* pszDisplayName, uint displayNameLength, uint displayShortNameOffset, SetDisplayNameFlags flags /* default to None */)
    {TRACE_IT(34055);
        this->m_displayNameLength = displayNameLength;
        this->m_displayShortNameOffset = displayShortNameOffset;
        this->m_displayNameIsRecyclerAllocated = FunctionProxy::SetDisplayName(pszDisplayName, &this->m_displayName, displayNameLength, m_scriptContext, flags);
    }

    LPCWSTR DeferDeserializeFunctionInfo::GetSourceInfo(int& lineNumber, int& columnNumber) const
    {TRACE_IT(34056);
        // Read all the necessary information from the serialized byte code
        int lineNumberField, columnNumberField;
        bool m_isEval, m_isDynamicFunction;
        ByteCodeSerializer::ReadSourceInfo(this, lineNumberField, columnNumberField, m_isEval, m_isDynamicFunction);

        // Decode them
        lineNumber = ComputeAbsoluteLineNumber(lineNumberField);
        columnNumber = ComputeAbsoluteColumnNumber(lineNumberField, columnNumberField);
        return Js::ParseableFunctionInfo::GetSourceName<SourceContextInfo*>(this->GetSourceContextInfo(), m_isEval, m_isDynamicFunction);
    }

    void DeferDeserializeFunctionInfo::Finalize(bool isShutdown)
    {TRACE_IT(34057);
        __super::Finalize(isShutdown);
        PERF_COUNTER_DEC(Code, DeferDeserializeFunctionProxy);
    }

    FunctionBody* DeferDeserializeFunctionInfo::Deserialize()
    {TRACE_IT(34058);
        Assert(this->GetFunctionInfo()->GetFunctionProxy() == this);

        FunctionBody * body = ByteCodeSerializer::DeserializeFunction(this->m_scriptContext, this);
        this->SetLocalFunctionId(body->GetLocalFunctionId());
        this->SetOriginalEntryPoint(body->GetOriginalEntryPoint());
        this->Copy(body);
        this->UpdateFunctionBodyImpl(body);

        Assert(body->GetFunctionBody() == body);
        return body;
    }

    //
    // hrParse can be one of the following from deferred re-parse (check CompileScriptException::ProcessError):
    //      E_OUTOFMEMORY
    //      E_UNEXPECTED
    //      SCRIPT_E_RECORDED,
    //          with ei.scode: ERRnoMemory, VBSERR_OutOfStack, E_OUTOFMEMORY, E_FAIL
    //          Any other ei.scode shouldn't appear in deferred re-parse.
    //
    // Map errors like OOM/SOE, return it and clean hrParse. Any other error remaining in hrParse is an internal error.
    //
    HRESULT ParseableFunctionInfo::MapDeferredReparseError(HRESULT& hrParse, const CompileScriptException& se)
    {TRACE_IT(34059);
        HRESULT hrMapped = NO_ERROR;

        switch (hrParse)
        {
        case E_OUTOFMEMORY:
            hrMapped = E_OUTOFMEMORY;
            break;

        case SCRIPT_E_RECORDED:
            switch (se.ei.scode)
            {
            case ERRnoMemory:
            case E_OUTOFMEMORY:
            case VBSERR_OutOfMemory:
                hrMapped = E_OUTOFMEMORY;
                break;

            case VBSERR_OutOfStack:
                hrMapped = VBSERR_OutOfStack;
                break;
            }
        }

        if (FAILED(hrMapped))
        {TRACE_IT(34060);
            // If we have mapped error, clear hrParse. We'll throw error from hrMapped.
            hrParse = NO_ERROR;
        }

        return hrMapped;
    }

    FunctionBody* ParseableFunctionInfo::Parse(ScriptFunction ** functionRef, bool isByteCodeDeserialization)
    {TRACE_IT(34061);
        Assert(this == this->GetFunctionInfo()->GetFunctionProxy());
        if (!IsDeferredParseFunction())
        {TRACE_IT(34062);
            // If not deferredparsed, the functionBodyImpl and this will be the same, just return the current functionBody.
            Assert(GetFunctionBody()->IsFunctionParsed());
            return GetFunctionBody();
        }

        bool asmjsParseFailed = false;
        BOOL fParsed = FALSE;
        FunctionBody* returnFunctionBody = nullptr;
        ENTER_PINNED_SCOPE(Js::PropertyRecordList, propertyRecordList);
        Recycler* recycler = this->m_scriptContext->GetRecycler();
        propertyRecordList = RecyclerNew(recycler, Js::PropertyRecordList, recycler);

        bool isDebugOrAsmJsReparse = false;
        FunctionBody* funcBody = nullptr;

        {
            AutoRestoreFunctionInfo autoRestoreFunctionInfo(this, DefaultEntryThunk);


            // If m_hasBeenParsed = true, one of the following things happened things happened:
            // - We had multiple function objects which were all defer-parsed, but with the same function body and one of them
            //   got the body to be parsed before another was called
            // - We are in debug mode and had our thunks switched to DeferParseThunk
            // - This is an already parsed asm.js module, which has been invalidated at link time and must be reparsed as a non-asm.js function
            if (!this->m_hasBeenParsed)
            {TRACE_IT(34063);
            this->GetUtf8SourceInfo()->StopTrackingDeferredFunction(this->GetLocalFunctionId());
            funcBody = FunctionBody::NewFromParseableFunctionInfo(this, propertyRecordList);
                autoRestoreFunctionInfo.funcBody = funcBody;

                PERF_COUNTER_DEC(Code, DeferredFunction);

                if (!this->GetSourceContextInfo()->IsDynamic())
                {TRACE_IT(34064);
                    PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("TestTrace: Deferred function parsed - ID: %d; Display Name: %s; Length: %d; Nested Function Count: %d; Utf8SourceInfo: %d; Source Length: %d; Is Top Level: %s; Source Url: %s\n"), m_functionNumber, this->GetDisplayName(), this->m_cchLength, this->GetNestedCount(), this->m_utf8SourceInfo->GetSourceInfoId(), this->m_utf8SourceInfo->GetCchLength(), this->GetIsTopLevel() ? _u("True") : _u("False"), this->GetSourceContextInfo()->url);
                }
                else
                {TRACE_IT(34065);
                    PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("TestTrace: Deferred function parsed - ID: %d; Display Name: %s; Length: %d; Nested Function Count: %d; Utf8SourceInfo: %d; Source Length: %d\n; Is Top Level: %s;"), m_functionNumber, this->GetDisplayName(), this->m_cchLength, this->GetNestedCount(),  this->m_utf8SourceInfo->GetSourceInfoId(), this->m_utf8SourceInfo->GetCchLength(), this->GetIsTopLevel() ? _u("True") : _u("False"));
                }

                if (!this->GetIsTopLevel() &&
                    !this->GetSourceContextInfo()->IsDynamic() &&
                    this->m_scriptContext->DoUndeferGlobalFunctions())
                {TRACE_IT(34066);
                    this->GetUtf8SourceInfo()->UndeferGlobalFunctions([this](const Utf8SourceInfo::DeferredFunctionsDictionary::EntryType& func)
                    {
                        Js::ParseableFunctionInfo *nextFunc = func.Value();
                        JavascriptExceptionObject* pExceptionObject = nullptr;

                        if (nextFunc != nullptr && this != nextFunc)
                        {TRACE_IT(34067);
                            try
                            {TRACE_IT(34068);
                                nextFunc->Parse();
                            }
                            catch (OutOfMemoryException) {TRACE_IT(34069);}
                            catch (StackOverflowException) {TRACE_IT(34070);}
                            catch (const Js::JavascriptException& err)
                            {TRACE_IT(34071);
                                pExceptionObject = err.GetAndClear();
                            }

                            // Do not do anything with an OOM or SOE, returning true is fine, it will then be undeferred (or attempted to again when called)
                            if (pExceptionObject)
                            {TRACE_IT(34072);
                                if (pExceptionObject != ThreadContext::GetContextForCurrentThread()->GetPendingOOMErrorObject() &&
                                    pExceptionObject != ThreadContext::GetContextForCurrentThread()->GetPendingSOErrorObject())
                                {TRACE_IT(34073);
                                    JavascriptExceptionOperators::DoThrow(pExceptionObject, /*scriptContext*/nullptr);
                                }
                            }
                        }

                        return true;
                    });
                }
            }
            else
            {TRACE_IT(34074);
                bool isDebugReparse = m_scriptContext->IsScriptContextInSourceRundownOrDebugMode() && !this->GetUtf8SourceInfo()->GetIsLibraryCode();
                bool isAsmJsReparse = m_isAsmjsMode && !isDebugReparse;

                isDebugOrAsmJsReparse = isAsmJsReparse || isDebugReparse;

                funcBody = this->GetFunctionBody();

                if (isDebugOrAsmJsReparse)
                {TRACE_IT(34075);
#if ENABLE_DEBUG_CONFIG_OPTIONS
                    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
#if DBG
                    Assert(
                        funcBody->IsReparsed()
                        || m_scriptContext->IsScriptContextInSourceRundownOrDebugMode()
                        || m_isAsmjsMode);
#endif
                    OUTPUT_TRACE(Js::DebuggerPhase, _u("Full nested reparse of function: %s (%s)\n"), funcBody->GetDisplayName(), funcBody->GetDebugNumberSet(debugStringBuffer));

                    if (funcBody->GetByteCode())
                    {TRACE_IT(34076);
                        // The current function needs to be cleaned up before getting generated in the debug mode.
                        funcBody->CleanupToReparse();
                    }

                }
            }

            // Note that we may be trying to re-gen an already-completed function. (This can happen, for instance,
            // in the case of named function expressions inside "with" statements in compat mode.)
            // In such a case, there's no work to do.
            if (funcBody->GetByteCode() == nullptr)
            {TRACE_IT(34077);
#if ENABLE_PROFILE_INFO
                Assert(!funcBody->HasExecutionDynamicProfileInfo());
#endif
                // In debug or asm.js mode, the scriptlet will be asked to recompile again.
                AssertMsg(isDebugOrAsmJsReparse || funcBody->GetGrfscr() & fscrGlobalCode || CONFIG_FLAG(DeferNested), "Deferred parsing of non-global procedure?");

                HRESULT hr = NO_ERROR;
                HRESULT hrParser = NO_ERROR;
                HRESULT hrParseCodeGen = NO_ERROR;

                BEGIN_LEAVE_SCRIPT_INTERNAL(m_scriptContext)
                {TRACE_IT(34078);
                    bool isCesu8 = m_scriptContext->GetSource(funcBody->GetSourceIndex())->IsCesu8();

                    size_t offset = this->StartOffset();
                    charcount_t charOffset = this->StartInDocument();
                    size_t length = this->LengthInBytes();

                    LPCUTF8 pszStart = this->GetStartOfDocument();

                    uint32 grfscr = funcBody->GetGrfscr() | fscrDeferredFnc;

                    // For the global function we want to re-use the glo functionbody which is already created in the non-debug mode
                    if (!funcBody->GetIsGlobalFunc())
                    {TRACE_IT(34079);
                        grfscr &= ~fscrGlobalCode;
                    }

                    if (!funcBody->GetIsDeclaration() && !funcBody->GetIsGlobalFunc()) // No refresh may reparse global function (e.g. eval code)
                    {TRACE_IT(34080);
                        // Notify the parser that the top-level function was defined in an expression,
                        // (not a function declaration statement).
                        grfscr |= fscrDeferredFncExpression;
                    }
                    if (!CONFIG_FLAG(DeferNested) || isDebugOrAsmJsReparse)
                    {TRACE_IT(34081);
                        grfscr &= ~fscrDeferFncParse; // Disable deferred parsing if not DeferNested, or doing a debug/asm.js re-parse
                    }

                    if (isDebugOrAsmJsReparse)
                    {TRACE_IT(34082);
                        grfscr |= fscrNoAsmJs; // Disable asm.js when debugging or if linking failed
                    }

                    BEGIN_TRANSLATE_EXCEPTION_TO_HRESULT
                    {
                        CompileScriptException se;
                        Parser ps(m_scriptContext, funcBody->GetIsStrictMode() ? TRUE : FALSE);
                        ParseNodePtr parseTree;

                        uint nextFunctionId = funcBody->GetLocalFunctionId();
                        hrParser = ps.ParseSourceWithOffset(&parseTree, pszStart, offset, length, charOffset, isCesu8, grfscr, &se,
                            &nextFunctionId, funcBody->GetRelativeLineNumber(), funcBody->GetSourceContextInfo(),
                            funcBody);
                        // Assert(FAILED(hrParser) || nextFunctionId == funcBody->deferredParseNextFunctionId || isDebugOrAsmJsReparse || isByteCodeDeserialization);

                        if (FAILED(hrParser))
                        {TRACE_IT(34083);
                            hrParseCodeGen = MapDeferredReparseError(hrParser, se); // Map certain errors like OOM/SOE
                            AssertMsg(FAILED(hrParseCodeGen) && SUCCEEDED(hrParser), "Syntax errors should never be detected on deferred re-parse");
                        }
                        else
                        {TRACE_IT(34084);
                            TRACE_BYTECODE(_u("\nDeferred parse %s\n"), funcBody->GetDisplayName());
                            Js::AutoDynamicCodeReference dynamicFunctionReference(m_scriptContext);

                            bool forceNoNative = isDebugOrAsmJsReparse ? this->GetScriptContext()->IsInterpreted() : false;

                            ParseableFunctionInfo* rootFunc = funcBody->GetParseableFunctionInfo();
                            hrParseCodeGen = GenerateByteCode(parseTree, grfscr, m_scriptContext,
                                &rootFunc, funcBody->GetSourceIndex(),
                                forceNoNative, &ps, &se, funcBody->GetScopeInfo(), functionRef);
                            funcBody->SetParseableFunctionInfo(rootFunc);

                            if (SUCCEEDED(hrParseCodeGen))
                            {TRACE_IT(34085);
                                fParsed = TRUE;
                            }
                            else
                            {TRACE_IT(34086);
                                Assert(hrParseCodeGen == SCRIPT_E_RECORDED);
                                hrParseCodeGen = se.ei.scode;
                            }
                        }
                    }
                    END_TRANSLATE_EXCEPTION_TO_HRESULT(hr);
                }
                END_LEAVE_SCRIPT_INTERNAL(m_scriptContext);

                THROW_KNOWN_HRESULT_EXCEPTIONS(hr, m_scriptContext);

                Assert(hr == NO_ERROR);

                if (!SUCCEEDED(hrParser))
                {TRACE_IT(34087);
                    JavascriptError::ThrowError(m_scriptContext, VBSERR_InternalError);
                }
                else if (!SUCCEEDED(hrParseCodeGen))
                {TRACE_IT(34088);
                    /*
                     * VBSERR_OutOfStack is of type kjstError but we throw a (more specific) StackOverflowError when a hard stack
                     * overflow occurs. To keep the behavior consistent I'm special casing it here.
                     */
                    if (hrParseCodeGen == VBSERR_OutOfStack)
                    {TRACE_IT(34089);
                        JavascriptError::ThrowStackOverflowError(m_scriptContext);
                    }
                    else if (hrParseCodeGen == JSERR_AsmJsCompileError)
                    {TRACE_IT(34090);
                        asmjsParseFailed = true;
                    }
                    else
                    {TRACE_IT(34091);
                        JavascriptError::MapAndThrowError(m_scriptContext, hrParseCodeGen);
                    }
                }
            }
            else
            {TRACE_IT(34092);
                fParsed = FALSE;
            }

            if (!asmjsParseFailed)
            {TRACE_IT(34093);
                autoRestoreFunctionInfo.Clear();
            }
        }

        if (fParsed == TRUE)
        {TRACE_IT(34094);
            // Restore if the function has nameIdentifier reference, as that name on the left side will not be parsed again while deferparse.
            funcBody->SetIsNameIdentifierRef(this->GetIsNameIdentifierRef());

            this->m_hasBeenParsed = true;
            returnFunctionBody = funcBody;
        }
        else if(!asmjsParseFailed)
        {TRACE_IT(34095);
            returnFunctionBody = this->GetFunctionBody();
        }

        LEAVE_PINNED_SCOPE();

        if (asmjsParseFailed)
        {TRACE_IT(34096);
            // disable asm.js and reparse on failure
            m_grfscr |= fscrNoAsmJs;
            return Parse(functionRef, isByteCodeDeserialization);
        }

        return returnFunctionBody;
    }

#ifdef ASMJS_PLAT
    FunctionBody* ParseableFunctionInfo::ParseAsmJs(Parser * ps, __out CompileScriptException * se, __out ParseNodePtr * parseTree)
    {TRACE_IT(34097);
        Assert(IsDeferredParseFunction());
        Assert(m_isAsmjsMode);

        FunctionBody* returnFunctionBody = nullptr;
        ENTER_PINNED_SCOPE(Js::PropertyRecordList, propertyRecordList);
        Recycler* recycler = this->m_scriptContext->GetRecycler();
        propertyRecordList = RecyclerNew(recycler, Js::PropertyRecordList, recycler);

        FunctionBody* funcBody = nullptr;

        funcBody = FunctionBody::NewFromRecycler(
            this->m_scriptContext,
            this->m_displayName,
            this->m_displayNameLength,
            this->m_displayShortNameOffset,
            this->GetNestedCount(),
            this->GetUtf8SourceInfo(),
            this->m_functionNumber,
            this->GetUtf8SourceInfo()->GetSrcInfo()->sourceContextInfo->sourceContextId,
            this->GetLocalFunctionId(),
            propertyRecordList,
            (FunctionInfo::Attributes)(this->GetAttributes() & ~(FunctionInfo::Attributes::DeferredDeserialize | FunctionInfo::Attributes::DeferredParse)),
            Js::FunctionBody::FunctionBodyFlags::Flags_HasNoExplicitReturnValue
#ifdef PERF_COUNTERS
            , false /* is function from deferred deserialized proxy */
#endif
            );

        this->Copy(funcBody);
        PERF_COUNTER_DEC(Code, DeferredFunction);

        if (!this->GetSourceContextInfo()->IsDynamic())
        {TRACE_IT(34098);
            PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("TestTrace: Deferred function parsed - ID: %d; Display Name: %s; Length: %d; Nested Function Count: %d; Utf8SourceInfo: %d; Source Length: %d; Is Top Level: %s; Source Url: %s\n"), m_functionNumber, this->GetDisplayName(), this->m_cchLength, this->GetNestedCount(), this->m_utf8SourceInfo->GetSourceInfoId(), this->m_utf8SourceInfo->GetCchLength(), this->GetIsTopLevel() ? _u("True") : _u("False"), this->GetSourceContextInfo()->url);
        }
        else
        {TRACE_IT(34099);
            PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("TestTrace: Deferred function parsed - ID: %d; Display Name: %s; Length: %d; Nested Function Count: %d; Utf8SourceInfo: %d; Source Length: %d\n; Is Top Level: %s;"), m_functionNumber, this->GetDisplayName(), this->m_cchLength, this->GetNestedCount(), this->m_utf8SourceInfo->GetSourceInfoId(), this->m_utf8SourceInfo->GetCchLength(), this->GetIsTopLevel() ? _u("True") : _u("False"));
        }

#if ENABLE_PROFILE_INFO
        Assert(!funcBody->HasExecutionDynamicProfileInfo());
#endif

        HRESULT hrParser = NO_ERROR;
        HRESULT hrParseCodeGen = NO_ERROR;

        bool isCesu8 = m_scriptContext->GetSource(funcBody->GetSourceIndex())->IsCesu8();

        size_t offset = this->StartOffset();
        charcount_t charOffset = this->StartInDocument();
        size_t length = this->LengthInBytes();

        LPCUTF8 pszStart = this->GetStartOfDocument();

        uint32 grfscr = funcBody->GetGrfscr() | fscrDeferredFnc | fscrDeferredFncExpression;

        uint nextFunctionId = funcBody->GetLocalFunctionId();

        // if parser throws, it will be caught by function trying to bytecode gen the asm.js module, so don't need to catch/rethrow here
        hrParser = ps->ParseSourceWithOffset(parseTree, pszStart, offset, length, charOffset, isCesu8, grfscr, se,
                    &nextFunctionId, funcBody->GetRelativeLineNumber(), funcBody->GetSourceContextInfo(),
                    funcBody);

        Assert(FAILED(hrParser) || funcBody->deferredParseNextFunctionId == nextFunctionId);
        if (FAILED(hrParser))
        {TRACE_IT(34100);
            hrParseCodeGen = MapDeferredReparseError(hrParser, *se); // Map certain errors like OOM/SOE
            AssertMsg(FAILED(hrParseCodeGen) && SUCCEEDED(hrParser), "Syntax errors should never be detected on deferred re-parse");
        }

        if (!SUCCEEDED(hrParser))
        {TRACE_IT(34101);
            Throw::InternalError();
        }
        else if (!SUCCEEDED(hrParseCodeGen))
        {TRACE_IT(34102);
            if (hrParseCodeGen == VBSERR_OutOfStack)
            {TRACE_IT(34103);
                Throw::StackOverflow(m_scriptContext, nullptr);
            }
            else
            {TRACE_IT(34104);
                Assert(hrParseCodeGen == E_OUTOFMEMORY);
                Throw::OutOfMemory();
            }
        }

        UpdateFunctionBodyImpl(funcBody);
        m_hasBeenParsed = true;

        Assert(funcBody->GetFunctionBody() == funcBody);

        returnFunctionBody = funcBody;

        LEAVE_PINNED_SCOPE();

        return returnFunctionBody;
    }
#endif

    void ParseableFunctionInfo::Finalize(bool isShutdown)
    {TRACE_IT(34105);
        __super::Finalize(isShutdown);
        if (this->GetFunctionInfo())
        {TRACE_IT(34106);
            // (If function info was never set, then initialization didn't finish, so there's nothing to remove from the dictionary.)
            this->GetUtf8SourceInfo()->StopTrackingDeferredFunction(this->GetLocalFunctionId());
        }
        if (!this->m_hasBeenParsed)
        {
            PERF_COUNTER_DEC(Code, DeferredFunction);
        }
    }

    bool ParseableFunctionInfo::IsFakeGlobalFunc(uint32 flags) const
    {TRACE_IT(34107);
        return GetIsGlobalFunc() && !(flags & fscrGlobalCode);
    }

    bool ParseableFunctionInfo::GetExternalDisplaySourceName(BSTR* sourceName)
    {TRACE_IT(34108);
        Assert(sourceName);

        if (IsDynamicScript() && GetUtf8SourceInfo()->GetDebugDocumentName(sourceName))
        {TRACE_IT(34109);
            return true;
        }

        *sourceName = ::SysAllocString(GetSourceName());
        return *sourceName != nullptr;
    }

    const char16* FunctionProxy::WrapWithBrackets(const char16* name, charcount_t sz, ScriptContext* scriptContext)
    {TRACE_IT(34110);
        char16 * wrappedName = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), char16, sz + 3); //[]\0
        wrappedName[0] = _u('[');
        char16 *next = wrappedName;
        js_wmemcpy_s(++next, sz, name, sz);
        wrappedName[sz + 1] = _u(']');
        wrappedName[sz + 2] = _u('\0');
        return wrappedName;

    }

    const char16* FunctionProxy::GetShortDisplayName(charcount_t * shortNameLength)
    {TRACE_IT(34111);
        const char16* name = this->GetDisplayName();
        uint nameLength = this->GetDisplayNameLength();

        if (name == nullptr)
        {TRACE_IT(34112);
            *shortNameLength = 0;
            return Constants::Empty;
        }

        if (IsConstantFunctionName(name))
        {TRACE_IT(34113);
            *shortNameLength = nameLength;
            return name;
        }
        uint shortNameOffset = this->GetShortDisplayNameOffset();
        const char16 * shortName = name + shortNameOffset;
        bool isBracketCase = shortNameOffset != 0 && name[shortNameOffset-1] == '[';
        Assert(nameLength >= shortNameOffset);
        *shortNameLength = nameLength - shortNameOffset;

        if (!isBracketCase)
        {TRACE_IT(34114);
            return shortName;
        }

        Assert(name[nameLength - 1] == ']');
        char16 * finalshorterName = RecyclerNewArrayLeaf(this->GetScriptContext()->GetRecycler(), char16, *shortNameLength);
        js_wmemcpy_s(finalshorterName, *shortNameLength, shortName, *shortNameLength - 1); // we don't want the last character in shorterName
        finalshorterName[*shortNameLength - 1] = _u('\0');
        *shortNameLength = *shortNameLength - 1;
        return finalshorterName;
    }

    /*static*/
    bool FunctionProxy::IsConstantFunctionName(const char16* srcName)
    {TRACE_IT(34115);
        if (srcName == Js::Constants::GlobalFunction ||
            srcName == Js::Constants::AnonymousFunction ||
            srcName == Js::Constants::GlobalCode ||
            srcName == Js::Constants::Anonymous ||
            srcName == Js::Constants::UnknownScriptCode ||
            srcName == Js::Constants::FunctionCode)
        {TRACE_IT(34116);
            return true;
        }
        return false;
    }

    /*static */
    /*Return value: Whether the target value is a recycler pointer or not*/
    bool FunctionProxy::SetDisplayName(const char16* srcName, const char16** destName, uint displayNameLength,  ScriptContext * scriptContext, SetDisplayNameFlags flags /* default to None */)
    {TRACE_IT(34117);
        Assert(destName);
        Assert(scriptContext);

        if (srcName == nullptr)
        {TRACE_IT(34118);
            *destName = (_u(""));
            return false;
        }
        else if (IsConstantFunctionName(srcName) || (flags & SetDisplayNameFlagsDontCopy) != 0)
        {TRACE_IT(34119);
            *destName = srcName;
            return (flags & SetDisplayNameFlagsRecyclerAllocated) != 0; // Return true if array is recycler allocated
        }
        else
        {TRACE_IT(34120);
            uint  numCharacters =  displayNameLength + 1;
            Assert((flags & SetDisplayNameFlagsDontCopy) == 0);

            *destName = RecyclerNewArrayLeaf(scriptContext->GetRecycler(), char16, numCharacters);
            js_wmemcpy_s((char16 *)*destName, numCharacters, srcName, numCharacters);
            ((char16 *)(*destName))[numCharacters - 1] = _u('\0');

            return true;
        }
    }

    bool FunctionProxy::SetDisplayName(const char16* srcName, WriteBarrierPtr<const char16>* destName, uint displayNameLength, ScriptContext * scriptContext, SetDisplayNameFlags flags /* default to None */)
    {TRACE_IT(34121);
        const char16* dest = nullptr;
        bool targetIsRecyclerMemory = SetDisplayName(srcName, &dest, displayNameLength, scriptContext, flags);

        if (targetIsRecyclerMemory)
        {TRACE_IT(34122);
            *destName = dest;
        }
        else
        {TRACE_IT(34123);
            destName->NoWriteBarrierSet(dest);
        }
        return targetIsRecyclerMemory;
    }
    void ParseableFunctionInfo::SetDisplayName(const char16* pszDisplayName)
    {TRACE_IT(34124);
        size_t len = wcslen(pszDisplayName);
        if (len > UINT_MAX)
        {TRACE_IT(34125);
            // Can't support display name that big
            Js::Throw::OutOfMemory();
        }
        SetDisplayName(pszDisplayName, (uint)len, 0);
    }
    void ParseableFunctionInfo::SetDisplayName(const char16* pszDisplayName, uint displayNameLength, uint displayShortNameOffset, SetDisplayNameFlags flags /* default to None */)
    {TRACE_IT(34126);
        this->m_displayNameLength = displayNameLength;
        this->m_displayShortNameOffset = displayShortNameOffset;

        this->m_displayNameIsRecyclerAllocated = FunctionProxy::SetDisplayName(pszDisplayName, &this->m_displayName, displayNameLength, m_scriptContext, flags);
    }

    // SourceInfo methods

    /* static */
    template <typename TStatementMapList>
    FunctionBody::StatementMap * FunctionBody::GetNextNonSubexpressionStatementMap(TStatementMapList *statementMapList, int & startingAtIndex)
    {TRACE_IT(34127);
        AssertMsg(statementMapList != nullptr, "Must have valid statementMapList to execute");

        FunctionBody::StatementMap *map = statementMapList->Item(startingAtIndex);
        while (map->isSubexpression && startingAtIndex < statementMapList->Count() - 1)
        {TRACE_IT(34128);
            map = statementMapList->Item(++startingAtIndex);
        }
        if (map->isSubexpression)   // Didn't find any non inner maps
        {TRACE_IT(34129);
            return nullptr;
        }
        return map;
    }
    // explicitly instantiate template
    template FunctionBody::StatementMap *
    FunctionBody::GetNextNonSubexpressionStatementMap<FunctionBody::ArenaStatementMapList>(FunctionBody::ArenaStatementMapList *statementMapList, int & startingAtIndex);
    template FunctionBody::StatementMap *
    FunctionBody::GetNextNonSubexpressionStatementMap<FunctionBody::StatementMapList>(FunctionBody::StatementMapList *statementMapList, int & startingAtIndex);

    /* static */ FunctionBody::StatementMap * FunctionBody::GetPrevNonSubexpressionStatementMap(StatementMapList *statementMapList, int & startingAtIndex)
    {TRACE_IT(34130);
        AssertMsg(statementMapList != nullptr, "Must have valid statementMapList to execute");

        StatementMap *map = statementMapList->Item(startingAtIndex);
        while (startingAtIndex && map->isSubexpression)
        {TRACE_IT(34131);
            map = statementMapList->Item(--startingAtIndex);
        }
        if (map->isSubexpression)   // Didn't find any non inner maps
        {TRACE_IT(34132);
            return nullptr;
        }
        return map;
    }

    void ParseableFunctionInfo::SetSourceInfo(uint sourceIndex, ParseNodePtr node, bool isEval, bool isDynamicFunction)
    {TRACE_IT(34133);
        if (!m_utf8SourceHasBeenSet)
        {TRACE_IT(34134);
            this->m_sourceIndex = sourceIndex;
            this->m_cchStartOffset = node->ichMin;
            this->m_cchLength = node->LengthInCodepoints();
            this->m_lineNumber = node->sxFnc.lineNumber;
            this->m_columnNumber = node->sxFnc.columnNumber;
            this->m_isEval = isEval;
            this->m_isDynamicFunction = isDynamicFunction;

            // It would have been better if we detect and reject large source buffer earlier before parsing
            size_t cbMin = node->sxFnc.cbMin;
            size_t lengthInBytes = node->sxFnc.LengthInBytes();
            if (cbMin > UINT_MAX || lengthInBytes > UINT_MAX)
            {TRACE_IT(34135);
                Js::Throw::OutOfMemory();
            }
            this->m_cbStartOffset = (uint)cbMin;
            this->m_cbLength = (uint)lengthInBytes;

            Assert(this->m_utf8SourceInfo != nullptr);
            this->m_utf8SourceHasBeenSet = true;

            if (this->IsFunctionBody())
            {TRACE_IT(34136);
                this->GetFunctionBody()->FinishSourceInfo();
            }
        }
#if DBG
        else
        {TRACE_IT(34137);
            AssertMsg(this->m_sourceIndex == sourceIndex, "Mismatched source index");
            if (!this->GetIsGlobalFunc())
            {TRACE_IT(34138);
                // In the global function case with a @cc_on, we modify some of these values so it might
                // not match on reparse (see ParseableFunctionInfo::Parse()).
                AssertMsg(this->StartOffset() == node->sxFnc.cbMin, "Mismatched source start offset");
                AssertMsg(this->m_cchStartOffset == node->ichMin, "Mismatched source character start offset");
                AssertMsg(this->m_cchLength == node->LengthInCodepoints(), "Mismatched source length");
                AssertMsg(this->LengthInBytes() == node->sxFnc.LengthInBytes(), "Mismatched source encoded byte length");
            }

            AssertMsg(this->m_isEval == isEval, "Mismatched source type");
            AssertMsg(this->m_isDynamicFunction == isDynamicFunction, "Mismatch source type");
       }
#endif

#if DBG_DUMP
        if (PHASE_TRACE1(Js::FunctionSourceInfoParsePhase))
        {TRACE_IT(34139);
            Assert(this->GetFunctionInfo()->HasBody());
            if (this->IsFunctionBody())
            {TRACE_IT(34140);
                FunctionBody* functionBody = this->GetFunctionBody();
                Assert( functionBody != nullptr );

                functionBody->PrintStatementSourceLineFromStartOffset(functionBody->StartInDocument());
                Output::Flush();
            }
        }
#endif
    }

    void ParseableFunctionInfo::SetSourceInfo(uint sourceIndex)
    {TRACE_IT(34141);
        // TODO (michhol): how do we want to handle wasm source?
        if (!m_utf8SourceHasBeenSet)
        {TRACE_IT(34142);
            this->m_sourceIndex = sourceIndex;
            this->m_cchStartOffset = 0;
            this->m_cchLength = 0;
            this->m_lineNumber = 0;
            this->m_columnNumber = 0;

            this->m_cbStartOffset = 0;
            this->m_cbLength = 0;

            this->m_utf8SourceHasBeenSet = true;

            if (this->IsFunctionBody())
            {TRACE_IT(34143);
                this->GetFunctionBody()->FinishSourceInfo();
            }
        }
#if DBG
        else
        {TRACE_IT(34144);
            AssertMsg(this->m_sourceIndex == sourceIndex, "Mismatched source index");
        }
#endif
    }

    bool FunctionBody::Is(void* ptr)
    {TRACE_IT(34145);
        if(!ptr)
        {TRACE_IT(34146);
            return false;
        }
        return VirtualTableInfo<FunctionBody>::HasVirtualTable(ptr);
    }

    bool FunctionBody::HasLineBreak() const
    {TRACE_IT(34147);
        return this->HasLineBreak(this->StartOffset(), this->m_cchStartOffset + this->m_cchLength);
    }

    bool FunctionBody::HasLineBreak(charcount_t start, charcount_t end) const
    {TRACE_IT(34148);
        if (start > end) return false;
        charcount_t cchLength = end - start;
        if (start < this->m_cchStartOffset || cchLength > this->m_cchLength) return false;
        LPCUTF8 src = this->GetSource(_u("FunctionBody::HasLineBreak"));
        LPCUTF8 last = src + this->LengthInBytes();
        size_t offset = this->LengthInBytes() == this->m_cchLength ?
            start - this->m_cchStartOffset :
            utf8::CharacterIndexToByteIndex(src, this->LengthInBytes(), start - this->m_cchStartOffset, utf8::doAllowThreeByteSurrogates);
        src = src + offset;

        utf8::DecodeOptions options = utf8::doAllowThreeByteSurrogates;

        for (charcount_t cch = cchLength; cch > 0; --cch)
        {TRACE_IT(34149);
            switch (utf8::Decode(src, last, options))
            {
            case '\r':
            case '\n':
            case 0x2028:
            case 0x2029:
                return true;
            }
        }

        return false;
    }

    FunctionBody::StatementMap* FunctionBody::GetMatchingStatementMapFromByteCode(int byteCodeOffset, bool ignoreSubexpressions /* = false */)
    {TRACE_IT(34150);
        StatementMapList * pStatementMaps = this->GetStatementMaps();
        if (pStatementMaps)
        {TRACE_IT(34151);
            Assert(m_sourceInfo.pSpanSequence == nullptr);
            for (int index = 0; index < pStatementMaps->Count(); index++)
            {TRACE_IT(34152);
                FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);

                if (!(ignoreSubexpressions && pStatementMap->isSubexpression) &&  pStatementMap->byteCodeSpan.Includes(byteCodeOffset))
                {TRACE_IT(34153);
                    return pStatementMap;
                }
            }
        }
        return nullptr;
    }

    // Returns the StatementMap for the offset.
    // 1. Current statementMap if bytecodeoffset falls within bytecode's span
    // 2. Previous if the bytecodeoffset is in between previous's end to current's begin
    FunctionBody::StatementMap* FunctionBody::GetEnclosingStatementMapFromByteCode(int byteCodeOffset, bool ignoreSubexpressions /* = false */)
    {TRACE_IT(34154);
        int index = GetEnclosingStatementIndexFromByteCode(byteCodeOffset, ignoreSubexpressions);
        if (index != -1)
        {TRACE_IT(34155);
            return this->GetStatementMaps()->Item(index);
        }
        return nullptr;
    }

    // Returns the index of StatementMap for
    // 1. Current statementMap if bytecodeoffset falls within bytecode's span
    // 2. Previous if the bytecodeoffset is in between previous's end to current's begin
    // 3. -1 of the failures.
    int FunctionBody::GetEnclosingStatementIndexFromByteCode(int byteCodeOffset, bool ignoreSubexpressions /* = false */)
    {TRACE_IT(34156);
        StatementMapList * pStatementMaps = this->GetStatementMaps();
        if (pStatementMaps == nullptr)
        {TRACE_IT(34157);
            // e.g. internal library.
            return -1;
        }

        Assert(m_sourceInfo.pSpanSequence == nullptr);

        for (int index = 0; index < pStatementMaps->Count(); index++)
        {TRACE_IT(34158);
            FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);

            if (!(ignoreSubexpressions && pStatementMap->isSubexpression) && pStatementMap->byteCodeSpan.Includes(byteCodeOffset))
            {TRACE_IT(34159);
                return index;
            }
            else if (!pStatementMap->isSubexpression && byteCodeOffset < pStatementMap->byteCodeSpan.begin) // We always ignore sub expressions when checking if we went too far
            {TRACE_IT(34160);
                return index > 0 ? index - 1 : 0;
            }
        }

        return pStatementMaps->Count() - 1;
    }

    // In some cases in legacy mode, due to the state scriptContext->windowIdList, the parser might not detect an eval call in the first parse but do so in the reparse
    // This fixes up the state at the start of reparse
    void FunctionBody::SaveState(ParseNodePtr pnode)
    {TRACE_IT(34161);
        Assert(!this->IsReparsed());
        this->SetChildCallsEval(!!pnode->sxFnc.ChildCallsEval());
        this->SetCallsEval(!!pnode->sxFnc.CallsEval());
        this->SetHasReferenceableBuiltInArguments(!!pnode->sxFnc.HasReferenceableBuiltInArguments());
    }

    void FunctionBody::RestoreState(ParseNodePtr pnode)
    {TRACE_IT(34162);
        Assert(this->IsReparsed());
#if ENABLE_DEBUG_CONFIG_OPTIONS
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        if(!!pnode->sxFnc.ChildCallsEval() != this->GetChildCallsEval())
        {TRACE_IT(34163);
            OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, _u("Child calls eval is different on debug reparse: %s(%s)\n"), this->GetExternalDisplayName(), this->GetDebugNumberSet(debugStringBuffer));
        }
        if(!!pnode->sxFnc.CallsEval() != this->GetCallsEval())
        {TRACE_IT(34164);
            OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, _u("Calls eval is different on debug reparse: %s(%s)\n"), this->GetExternalDisplayName(), this->GetDebugNumberSet(debugStringBuffer));
        }
        if(!!pnode->sxFnc.HasReferenceableBuiltInArguments() != this->HasReferenceableBuiltInArguments())
        {TRACE_IT(34165);
            OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, _u("Referenceable Built in args is different on debug reparse: %s(%s)\n"), this->GetExternalDisplayName(), this->GetDebugNumberSet(debugStringBuffer));
        }

        pnode->sxFnc.SetChildCallsEval(this->GetChildCallsEval());
        pnode->sxFnc.SetCallsEval(this->GetCallsEval());
        pnode->sxFnc.SetHasReferenceableBuiltInArguments(this->HasReferenceableBuiltInArguments());
    }

    // Retrieves statement map for given byte code offset.
    // Parameters:
    // - sourceOffset: byte code offset to get map for.
    // - mapIndex: if not NULL, receives the index of found map.
    FunctionBody::StatementMap* FunctionBody::GetMatchingStatementMapFromSource(int sourceOffset, int* pMapIndex /* = nullptr */)
    {TRACE_IT(34166);
        StatementMapList * pStatementMaps = this->GetStatementMaps();
        if (pStatementMaps && pStatementMaps->Count() > 0)
        {TRACE_IT(34167);
            Assert(m_sourceInfo.pSpanSequence == nullptr);
            for (int index = pStatementMaps->Count() - 1; index >= 0; index--)
            {TRACE_IT(34168);
                FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);

                if (!pStatementMap->isSubexpression && pStatementMap->sourceSpan.Includes(sourceOffset))
                {TRACE_IT(34169);
                    if (pMapIndex)
                    {TRACE_IT(34170);
                        *pMapIndex = index;
                    }
                    return pStatementMap;
                }
            }
        }

        if (pMapIndex)
        {TRACE_IT(34171);
            *pMapIndex = 0;
        }
        return nullptr;
    }

    //
    // The function determine the line and column for a bytecode offset within the current script buffer.
    //
    bool FunctionBody::GetLineCharOffset(int byteCodeOffset, ULONG* _line, LONG* _charOffset, bool canAllocateLineCache /*= true*/)
    {TRACE_IT(34172);
        Assert(!this->GetUtf8SourceInfo()->GetIsLibraryCode());

        int startCharOfStatement = this->m_cchStartOffset; // Default to the start of this function

        if (m_sourceInfo.pSpanSequence)
        {TRACE_IT(34173);
            SmallSpanSequenceIter iter;
            m_sourceInfo.pSpanSequence->Reset(iter);

            StatementData data;

            if (m_sourceInfo.pSpanSequence->GetMatchingStatementFromBytecode(byteCodeOffset, iter, data)
                && EndsAfter(data.sourceBegin))
            {TRACE_IT(34174);
                startCharOfStatement = data.sourceBegin;
            }
        }
        else
        {TRACE_IT(34175);
            Js::FunctionBody::StatementMap* map = this->GetEnclosingStatementMapFromByteCode(byteCodeOffset, false);
            if (map && EndsAfter(map->sourceSpan.begin))
            {TRACE_IT(34176);
                startCharOfStatement = map->sourceSpan.begin;
            }
        }

        return this->GetLineCharOffsetFromStartChar(startCharOfStatement, _line, _charOffset, canAllocateLineCache);
    }

    bool FunctionBody::GetLineCharOffsetFromStartChar(int startCharOfStatement, ULONG* _line, LONG* _charOffset, bool canAllocateLineCache /*= true*/)
    {TRACE_IT(34177);
        Assert(!this->GetUtf8SourceInfo()->GetIsLibraryCode());

        // The following adjusts for where the script is within the document
        ULONG line = this->GetHostStartLine();
        charcount_t column = 0;
        ULONG lineCharOffset = 0;
        charcount_t lineByteOffset = 0;

        if (startCharOfStatement > 0)
        {TRACE_IT(34178);
            bool doSlowLookup = !canAllocateLineCache;
            if (canAllocateLineCache)
            {TRACE_IT(34179);
                HRESULT hr = this->GetUtf8SourceInfo()->EnsureLineOffsetCacheNoThrow();
                if (FAILED(hr))
                {TRACE_IT(34180);
                    if (hr != E_OUTOFMEMORY)
                    {TRACE_IT(34181);
                        Assert(hr == E_ABORT); // The only other possible error we know about is ScriptAbort from QueryContinue.
                        return false;
                    }

                    // Clear the cache so it is not used.
                    this->GetUtf8SourceInfo()->DeleteLineOffsetCache();

                    // We can try and do the slow lookup below
                    doSlowLookup = true;
                }
            }

            charcount_t cacheLine = 0;
            this->GetUtf8SourceInfo()->GetLineInfoForCharPosition(startCharOfStatement, &cacheLine, &column, &lineByteOffset, doSlowLookup);

            // Update the tracking variables to jump to the line position (only need to jump if not on the first line).
            if (cacheLine > 0)
            {TRACE_IT(34182);
                line += cacheLine;
                lineCharOffset = startCharOfStatement - column;
            }
        }

        if (this->GetSourceContextInfo()->IsDynamic() && this->m_isDynamicFunction)
        {TRACE_IT(34183);
            line -= JavascriptFunction::numberLinesPrependedToAnonymousFunction;
        }

        if(_line)
        {TRACE_IT(34184);
            *_line = line;
        }

        if(_charOffset)
        {TRACE_IT(34185);
            *_charOffset = column;

            // If we are at the beginning of the host code, adjust the offset based on the host provided offset
            if (this->GetHostSrcInfo()->dlnHost == line)
            {TRACE_IT(34186);
                *_charOffset += (LONG)this->GetHostStartColumn();
            }
        }

        return true;
    }

    bool FunctionBody::GetStatementIndexAndLengthAt(int byteCodeOffset, UINT32* statementIndex, UINT32* statementLength)
    {TRACE_IT(34187);
        Assert(statementIndex != nullptr);
        Assert(statementLength != nullptr);

        Assert(this->IsInDebugMode());

        StatementMap * statement = GetEnclosingStatementMapFromByteCode(byteCodeOffset, false);
        Assert(statement != nullptr);

        // Bailout if we are unable to find a statement.
        // We shouldn't be missing these when a debugger is attached but we don't want to AV on retail builds.
        if (statement == nullptr)
        {TRACE_IT(34188);
            return false;
        }

        Assert(m_utf8SourceInfo);
        const SRCINFO * srcInfo = GetUtf8SourceInfo()->GetSrcInfo();

        // Offset from the beginning of the document minus any host-supplied source characters.
        // Host supplied characters are inserted (for example) around onload:
        //      onload="foo('somestring', 0)" -> function onload(event).{.foo('somestring', 0).}
        ULONG offsetFromDocumentBegin = srcInfo ? srcInfo->ulCharOffset - srcInfo->ichMinHost : 0;

        *statementIndex = statement->sourceSpan.Begin() + offsetFromDocumentBegin;
        *statementLength = statement->sourceSpan.End() - statement->sourceSpan.Begin();
        return true;
    }

    void FunctionBody::RecordFrameDisplayRegister(RegSlot slot)
    {TRACE_IT(34189);
        AssertMsg(slot != 0, "The assumption that the Frame Display Register cannot be at the 0 slot is wrong.");
        SetFrameDisplayRegister(slot);
    }

    void FunctionBody::RecordObjectRegister(RegSlot slot)
    {TRACE_IT(34190);
        AssertMsg(slot != 0, "The assumption that the Object Register cannot be at the 0 slot is wrong.");
        SetObjectRegister(slot);
    }

    Js::RootObjectBase * FunctionBody::GetRootObject() const
    {TRACE_IT(34191);
        // Safe to be used by the JIT thread
        Assert(this->GetConstTable() != nullptr);
        return (Js::RootObjectBase *)PointerValue(this->GetConstTable()[Js::FunctionBody::RootObjectRegSlot - FunctionBody::FirstRegSlot]);
    }

    Js::RootObjectBase * FunctionBody::LoadRootObject() const
    {TRACE_IT(34192);
        if ((this->GetGrfscr() & fscrIsModuleCode) == fscrIsModuleCode || this->GetModuleID() == kmodGlobal)
        {TRACE_IT(34193);
            return JavascriptOperators::OP_LdRoot(this->GetScriptContext());
        }
        return JavascriptOperators::GetModuleRoot(this->GetModuleID(), this->GetScriptContext());
    }

#if ENABLE_NATIVE_CODEGEN
    FunctionEntryPointInfo * FunctionBody::GetEntryPointFromNativeAddress(DWORD_PTR codeAddress)
    {TRACE_IT(34194);
        FunctionEntryPointInfo * entryPoint = nullptr;
        this->MapEntryPoints([&entryPoint, &codeAddress](int index, FunctionEntryPointInfo * currentEntryPoint)
        {
            // We need to do a second check for IsNativeCode because the entry point could be in the process of
            // being recorded on the background thread
            if (currentEntryPoint->IsInNativeAddressRange(codeAddress))
            {TRACE_IT(34195);
                entryPoint = currentEntryPoint;
            }
        });

        return entryPoint;
    }

    LoopEntryPointInfo * FunctionBody::GetLoopEntryPointInfoFromNativeAddress(DWORD_PTR codeAddress, uint loopNum) const
    {TRACE_IT(34196);
        LoopEntryPointInfo * entryPoint = nullptr;

        LoopHeader * loopHeader = this->GetLoopHeader(loopNum);
        Assert(loopHeader);

        loopHeader->MapEntryPoints([&](int index, LoopEntryPointInfo * currentEntryPoint)
        {
            if (currentEntryPoint->IsCodeGenDone() &&
                codeAddress >= currentEntryPoint->GetNativeAddress() &&
                codeAddress < currentEntryPoint->GetNativeAddress() + currentEntryPoint->GetCodeSize())
            {TRACE_IT(34197);
                entryPoint = currentEntryPoint;
            }
        });

        return entryPoint;
    }

    int FunctionBody::GetStatementIndexFromNativeOffset(SmallSpanSequence *pThrowSpanSequence, uint32 nativeOffset)
    {TRACE_IT(34198);
        int statementIndex = -1;
        if (pThrowSpanSequence)
        {TRACE_IT(34199);
            SmallSpanSequenceIter iter;
            StatementData tmpData;
            if (pThrowSpanSequence->GetMatchingStatementFromBytecode(nativeOffset, iter, tmpData))
            {TRACE_IT(34200);
                statementIndex = tmpData.sourceBegin; // sourceBegin represents statementIndex here
            }
            else
            {TRACE_IT(34201);
                // If nativeOffset falls on the last span, GetMatchingStatement would miss it because SmallSpanSequence
                // does not know about the last span end. Since we checked that codeAddress is within our range,
                // we can safely consider it matches the last span.
                statementIndex = iter.accumulatedSourceBegin;
            }
        }

        return statementIndex;
    }

    int FunctionBody::GetStatementIndexFromNativeAddress(SmallSpanSequence *pThrowSpanSequence, DWORD_PTR codeAddress, DWORD_PTR nativeBaseAddress)
    {TRACE_IT(34202);
        uint32 nativeOffset = (uint32)(codeAddress - nativeBaseAddress);

        return GetStatementIndexFromNativeOffset(pThrowSpanSequence, nativeOffset);
    }
#endif

    BOOL FunctionBody::GetMatchingStatementMap(StatementData &data, int statementIndex, FunctionBody *inlinee)
    {TRACE_IT(34203);
        SourceInfo *si = &this->m_sourceInfo;
        if (inlinee)
        {TRACE_IT(34204);
            si = &inlinee->m_sourceInfo;
            Assert(si);
        }

        if (statementIndex >= 0)
        {TRACE_IT(34205);
            SmallSpanSequence *pSpanSequence = si->pSpanSequence;
            if (pSpanSequence)
            {TRACE_IT(34206);
                SmallSpanSequenceIter iter;
                pSpanSequence->Reset(iter);

                if (pSpanSequence->Item(statementIndex, iter, data))
                {TRACE_IT(34207);
                    return TRUE;
                }
            }
            else
            {TRACE_IT(34208);
                StatementMapList* pStatementMaps = GetStatementMaps();
                Assert(pStatementMaps);
                if (statementIndex >= pStatementMaps->Count())
                {TRACE_IT(34209);
                    return FALSE;
                }

                data.sourceBegin = pStatementMaps->Item(statementIndex)->sourceSpan.begin;
                data.bytecodeBegin = pStatementMaps->Item(statementIndex)->byteCodeSpan.begin;
                return TRUE;
            }
        }

        return FALSE;
    }

    void FunctionBody::FindClosestStatements(int32 characterOffset, StatementLocation *firstStatementLocation, StatementLocation *secondStatementLocation)
    {TRACE_IT(34210);
        auto statementMaps = this->GetStatementMaps();
        if (statementMaps)
        {TRACE_IT(34211);
            for(int i = 0; i < statementMaps->Count(); i++)
            {TRACE_IT(34212);
                regex::Interval* pSourceSpan = &(statementMaps->Item(i)->sourceSpan);
                if (FunctionBody::IsDummyGlobalRetStatement(pSourceSpan))
                {TRACE_IT(34213);
                    // Workaround for handling global return, which is an empty range.
                    continue;
                }

                if (pSourceSpan->begin < characterOffset
                    && (firstStatementLocation->function == nullptr || firstStatementLocation->statement.begin < pSourceSpan->begin))
                {TRACE_IT(34214);
                    firstStatementLocation->function = this;
                    firstStatementLocation->statement = *pSourceSpan;
                    firstStatementLocation->bytecodeSpan = statementMaps->Item(i)->byteCodeSpan;
                }
                else if (pSourceSpan->begin >= characterOffset
                    && (secondStatementLocation->function == nullptr || secondStatementLocation->statement.begin > pSourceSpan->begin))
                {TRACE_IT(34215);
                    secondStatementLocation->function = this;
                    secondStatementLocation->statement = *pSourceSpan;
                    secondStatementLocation->bytecodeSpan = statementMaps->Item(i)->byteCodeSpan;
                }
            }
        }
    }

#if ENABLE_NATIVE_CODEGEN
    BOOL FunctionBody::GetMatchingStatementMapFromNativeAddress(DWORD_PTR codeAddress, StatementData &data, uint loopNum, FunctionBody *inlinee /* = nullptr */)
    {TRACE_IT(34216);
        SmallSpanSequence * spanSequence = nullptr;
        DWORD_PTR nativeBaseAddress = NULL;

        EntryPointInfo * entryPoint;
        if (loopNum == -1)
        {TRACE_IT(34217);
            entryPoint = GetEntryPointFromNativeAddress(codeAddress);
        }
        else
        {TRACE_IT(34218);
            entryPoint = GetLoopEntryPointInfoFromNativeAddress(codeAddress, loopNum);
        }

        if (entryPoint != nullptr)
        {TRACE_IT(34219);
            spanSequence = entryPoint->GetNativeThrowSpanSequence();
            nativeBaseAddress = entryPoint->GetNativeAddress();
        }

        int statementIndex = GetStatementIndexFromNativeAddress(spanSequence, codeAddress, nativeBaseAddress);

        return GetMatchingStatementMap(data, statementIndex, inlinee);
    }

    BOOL FunctionBody::GetMatchingStatementMapFromNativeOffset(DWORD_PTR codeAddress, uint32 offset, StatementData &data, uint loopNum, FunctionBody *inlinee /* = nullptr */)
    {TRACE_IT(34220);
        EntryPointInfo * entryPoint;

        if (loopNum == -1)
        {TRACE_IT(34221);
            entryPoint = GetEntryPointFromNativeAddress(codeAddress);
        }
        else
        {TRACE_IT(34222);
            entryPoint = GetLoopEntryPointInfoFromNativeAddress(codeAddress, loopNum);
        }

        SmallSpanSequence *spanSequence = entryPoint ? entryPoint->GetNativeThrowSpanSequence() : nullptr;
        int statementIndex = GetStatementIndexFromNativeOffset(spanSequence, offset);

        return GetMatchingStatementMap(data, statementIndex, inlinee);
    }
#endif

#if ENABLE_PROFILE_INFO
    void FunctionBody::LoadDynamicProfileInfo()
    {TRACE_IT(34223);
        SourceDynamicProfileManager * sourceDynamicProfileManager = GetSourceContextInfo()->sourceDynamicProfileManager;
        if (sourceDynamicProfileManager != nullptr)
        {TRACE_IT(34224);
            this->dynamicProfileInfo = sourceDynamicProfileManager->GetDynamicProfileInfo(this);
#if DBG_DUMP
            if(this->dynamicProfileInfo)
            {TRACE_IT(34225);
                if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase, this->GetSourceContextId(), this->GetLocalFunctionId()))
                {TRACE_IT(34226);
                    Output::Print(_u("Loaded:"));
                    this->dynamicProfileInfo->Dump(this);
                }
            }
#endif
        }

#ifdef DYNAMIC_PROFILE_MUTATOR
        DynamicProfileMutator::Mutate(this);
#endif
    }

    bool FunctionBody::NeedEnsureDynamicProfileInfo() const
    {TRACE_IT(34227);
        // Only need to ensure dynamic profile if we don't already have link up the dynamic profile info
        // and dynamic profile collection is enabled
        return
            !this->m_isFromNativeCodeModule &&
            !this->m_isAsmJsFunction &&
#ifdef ASMJS_PLAT
            !this->GetAsmJsModuleInfo() &&
#endif
            !this->HasExecutionDynamicProfileInfo() &&
            DynamicProfileInfo::IsEnabled(this);
    }

    DynamicProfileInfo * FunctionBody::EnsureDynamicProfileInfo()
    {TRACE_IT(34228);
        if (this->NeedEnsureDynamicProfileInfo())
        {TRACE_IT(34229);
            m_scriptContext->AddDynamicProfileInfo(this, this->dynamicProfileInfo);
            Assert(!this->HasExecutionDynamicProfileInfo());
            this->hasExecutionDynamicProfileInfo = true;
        }

        return this->dynamicProfileInfo;
    }

    DynamicProfileInfo* FunctionBody::AllocateDynamicProfile()
    {TRACE_IT(34230);
        return DynamicProfileInfo::New(m_scriptContext->GetRecycler(), this);
    }
#endif

    BOOL FunctionBody::IsNativeOriginalEntryPoint() const
    {TRACE_IT(34231);
#if ENABLE_NATIVE_CODEGEN
        return this->GetScriptContext()->IsNativeAddress((void*)this->GetOriginalEntryPoint_Unchecked());
#else
        return false;
#endif
    }

    bool FunctionBody::IsSimpleJitOriginalEntryPoint() const
    {TRACE_IT(34232);
        const FunctionEntryPointInfo *const simpleJitEntryPointInfo = GetSimpleJitEntryPointInfo();
        return
            simpleJitEntryPointInfo &&
            reinterpret_cast<Js::JavascriptMethod>(simpleJitEntryPointInfo->GetNativeAddress()) == GetOriginalEntryPoint_Unchecked();
    }

    void FunctionProxy::Finalize(bool isShutdown)
    {TRACE_IT(34233);
        this->CleanupFunctionProxyCounters();
    }

#if DBG
    bool FunctionBody::HasValidSourceInfo()
    {TRACE_IT(34234);
        SourceContextInfo* sourceContextInfo;

        if (m_scriptContext->GetSourceContextInfoMap())
        {TRACE_IT(34235);
            if(m_scriptContext->GetSourceContextInfoMap()->TryGetValue(this->GetHostSourceContext(), &sourceContextInfo) &&
                sourceContextInfo == this->GetSourceContextInfo())
            {TRACE_IT(34236);
                return true;
            }
        }
        Assert(this->IsDynamicScript());

        if(m_scriptContext->GetDynamicSourceContextInfoMap())
        {TRACE_IT(34237);
            if(m_scriptContext->GetDynamicSourceContextInfoMap()->TryGetValue(this->GetSourceContextInfo()->hash, &sourceContextInfo) &&
                sourceContextInfo == this->GetSourceContextInfo())
            {TRACE_IT(34238);
                return true;
            }
        }

        // The SourceContextInfo will not be added to the dynamicSourceContextInfoMap, if they are host provided dynamic code. But they are valid source context info
        if (this->GetSourceContextInfo()->isHostDynamicDocument)
        {TRACE_IT(34239);
            return true;
        }
        return m_scriptContext->IsNoContextSourceContextInfo(this->GetSourceContextInfo());
    }

    // originalEntryPoint: DefaultDeferredParsingThunk, DefaultDeferredDeserializeThunk, DefaultEntryThunk, dynamic interpreter thunk or native entry point
    // directEntryPoint:
    //      if (!profiled) - DefaultDeferredParsingThunk, DefaultDeferredDeserializeThunk, DefaultEntryThunk, CheckCodeGenThunk,
    //                       dynamic interpreter thunk, native entry point
    //      if (profiling) - ProfileDeferredParsingThunk, ProfileDeferredDeserializeThunk, ProfileEntryThunk, CheckCodeGenThunk
    bool FunctionProxy::HasValidNonProfileEntryPoint() const
    {TRACE_IT(34240);
        JavascriptMethod directEntryPoint = this->GetDefaultEntryPointInfo()->jsMethod;
        JavascriptMethod originalEntryPoint = this->GetOriginalEntryPoint_Unchecked();

        // Check the direct entry point to see if it is codegen thunk
        // if it is not, the background codegen thread has updated both original entry point and direct entry point
        // and they should still match, same as cases other then code gen
        return IsIntermediateCodeGenThunk(directEntryPoint) || originalEntryPoint == directEntryPoint
#if ENABLE_PROFILE_INFO
            || (directEntryPoint == DynamicProfileInfo::EnsureDynamicProfileInfoThunk &&
            this->IsFunctionBody() && this->GetFunctionBody()->IsNativeOriginalEntryPoint())
#ifdef ENABLE_WASM
            || (GetFunctionBody()->IsWasmFunction() &&
                (directEntryPoint == WasmLibrary::WasmDeferredParseInternalThunk || directEntryPoint == WasmLibrary::WasmLazyTrapCallback))
#endif
#ifdef ASMJS_PLAT
            || (GetFunctionBody()->GetIsAsmJsFunction() && directEntryPoint == AsmJsDefaultEntryThunk)
            || IsAsmJsCodeGenThunk(directEntryPoint)
#endif
#endif
        ;
    }
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
    bool FunctionProxy::HasValidProfileEntryPoint() const
    {TRACE_IT(34241);
        JavascriptMethod directEntryPoint = this->GetDefaultEntryPointInfo()->jsMethod;
        JavascriptMethod originalEntryPoint = this->GetOriginalEntryPoint_Unchecked();

        if (originalEntryPoint == DefaultDeferredParsingThunk)
        {TRACE_IT(34242);
            return directEntryPoint == ProfileDeferredParsingThunk;
        }
        if (originalEntryPoint == DefaultDeferredDeserializeThunk)
        {TRACE_IT(34243);
            return directEntryPoint == ProfileDeferredDeserializeThunk;
        }
        if (!this->IsFunctionBody())
        {TRACE_IT(34244);
            return false;
        }

#if ENABLE_PROFILE_INFO
        FunctionBody * functionBody = this->GetFunctionBody();
        if (functionBody->IsInterpreterThunk() || functionBody->IsSimpleJitOriginalEntryPoint())
        {TRACE_IT(34245);
            return directEntryPoint == ProfileEntryThunk || IsIntermediateCodeGenThunk(directEntryPoint);
        }

#if ENABLE_NATIVE_CODEGEN
        // In the profiler mode, the EnsureDynamicProfileInfoThunk is valid as we would be assigning to appropriate thunk when that thunk called.
        return functionBody->IsNativeOriginalEntryPoint() &&
            (directEntryPoint == DynamicProfileInfo::EnsureDynamicProfileInfoThunk || directEntryPoint == ProfileEntryThunk);
#endif
#else
        return true;
#endif
    }
#endif

    bool FunctionProxy::HasValidEntryPoint() const
    {TRACE_IT(34246);
        if (this->IsWasmFunction() ||
            (!m_scriptContext->HadProfiled() &&
            !(this->m_scriptContext->IsScriptContextInDebugMode() && m_scriptContext->IsExceptionWrapperForBuiltInsEnabled())))
        {TRACE_IT(34247);
            return this->HasValidNonProfileEntryPoint();
        }
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
        if (m_scriptContext->IsProfiling())
        {TRACE_IT(34248);
            return this->HasValidProfileEntryPoint();
        }

        return this->HasValidNonProfileEntryPoint() || this->HasValidProfileEntryPoint();
#else
        return this->HasValidNonProfileEntryPoint();
#endif
    }

#endif
    void ParseableFunctionInfo::SetDeferredParsingEntryPoint()
    {TRACE_IT(34249);
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
        Assert(m_scriptContext->DeferredParsingThunk == ProfileDeferredParsingThunk
            || m_scriptContext->DeferredParsingThunk == DefaultDeferredParsingThunk);
#else
        Assert(m_scriptContext->DeferredParsingThunk == DefaultDeferredParsingThunk);
#endif

        this->SetEntryPoint(this->GetDefaultEntryPointInfo(), m_scriptContext->DeferredParsingThunk);
        this->SetOriginalEntryPoint(DefaultDeferredParsingThunk);
    }

    void ParseableFunctionInfo::SetInitialDefaultEntryPoint()
    {TRACE_IT(34250);
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
        Assert(m_scriptContext->CurrentThunk == ProfileEntryThunk || m_scriptContext->CurrentThunk == DefaultEntryThunk);
        Assert(this->GetOriginalEntryPoint_Unchecked() == DefaultDeferredParsingThunk ||
               this->GetOriginalEntryPoint_Unchecked() == ProfileDeferredParsingThunk ||
               this->GetOriginalEntryPoint_Unchecked() == DefaultDeferredDeserializeThunk ||
               this->GetOriginalEntryPoint_Unchecked() == ProfileDeferredDeserializeThunk ||
               this->GetOriginalEntryPoint_Unchecked() == DefaultEntryThunk ||
               this->GetOriginalEntryPoint_Unchecked() == ProfileEntryThunk);
#else
        Assert(m_scriptContext->CurrentThunk == DefaultEntryThunk);
        Assert(this->GetOriginalEntryPoint_Unchecked() == DefaultDeferredParsingThunk ||
               this->GetOriginalEntryPoint_Unchecked() == DefaultDeferredDeserializeThunk ||
               this->GetOriginalEntryPoint_Unchecked() == DefaultEntryThunk);
#endif
        Assert(this->m_defaultEntryPointInfo != nullptr);

        // CONSIDER: we can optimize this to generate the dynamic interpreter thunk up front
        // If we know that we are in the defer parsing thunk already
        this->SetEntryPoint(this->GetDefaultEntryPointInfo(), m_scriptContext->CurrentThunk);
        this->SetOriginalEntryPoint(DefaultEntryThunk);
    }

    void FunctionBody::SetCheckCodeGenEntryPoint(FunctionEntryPointInfo* entryPointInfo, JavascriptMethod entryPoint)
    {TRACE_IT(34251);
        Assert(IsIntermediateCodeGenThunk(entryPoint));
        Assert(
            this->GetEntryPoint(entryPointInfo) == m_scriptContext->CurrentThunk ||
            (entryPointInfo == this->m_defaultEntryPointInfo && this->IsInterpreterThunk()) ||
            (
                GetSimpleJitEntryPointInfo() &&
                GetEntryPoint(entryPointInfo) == reinterpret_cast<void *>(GetSimpleJitEntryPointInfo()->GetNativeAddress())
            ));
        this->SetEntryPoint(entryPointInfo, entryPoint);
    }

#if DYNAMIC_INTERPRETER_THUNK
    void FunctionBody::GenerateDynamicInterpreterThunk()
    {TRACE_IT(34252);
        if (this->m_dynamicInterpreterThunk == nullptr)
        {TRACE_IT(34253);
            // NOTE: Etw rundown thread may be reading this->dynamicInterpreterThunk concurrently. We don't need to synchronize
            // access as it is ok for etw rundown to get either null or updated new value.

            if (m_isAsmJsFunction)
            {TRACE_IT(34254);
                this->SetOriginalEntryPoint(this->m_scriptContext->GetNextDynamicAsmJsInterpreterThunk(&this->m_dynamicInterpreterThunk));
            }
            else
            {TRACE_IT(34255);
                this->SetOriginalEntryPoint(this->m_scriptContext->GetNextDynamicInterpreterThunk(&this->m_dynamicInterpreterThunk));
            }
            JS_ETW(EtwTrace::LogMethodInterpreterThunkLoadEvent(this));
        }
        else
        {TRACE_IT(34256);
            this->SetOriginalEntryPoint((JavascriptMethod)InterpreterThunkEmitter::ConvertToEntryPoint(this->m_dynamicInterpreterThunk));
        }
    }

    JavascriptMethod FunctionBody::EnsureDynamicInterpreterThunk(FunctionEntryPointInfo* entryPointInfo)
    {TRACE_IT(34257);
        // This may be first call to the function, make sure we have dynamic profile info
        //
        // We need to ensure dynamic profile info even if we didn't generate a dynamic interpreter thunk
        // This happens when we go through CheckCodeGen thunk, to DelayDynamicInterpreterThunk, to here
        // but the background codegen thread updated the entry point with the native entry point.

        this->EnsureDynamicProfileInfo();

        Assert(HasValidEntryPoint());
        if (InterpreterStackFrame::IsDelayDynamicInterpreterThunk(this->GetEntryPoint(entryPointInfo)))
        {TRACE_IT(34258);
            // We are not doing code gen on this function, just change the entry point directly
            Assert(InterpreterStackFrame::IsDelayDynamicInterpreterThunk(this->GetOriginalEntryPoint_Unchecked()));
            GenerateDynamicInterpreterThunk();
            this->SetEntryPoint(entryPointInfo, this->GetOriginalEntryPoint_Unchecked());
        }
        else if (this->GetEntryPoint(entryPointInfo) == ProfileEntryThunk)
        {TRACE_IT(34259);
            // We are not doing codegen on this function, just change the entry point directly
            // Don't replace the profile entry thunk
            Assert(InterpreterStackFrame::IsDelayDynamicInterpreterThunk(this->GetOriginalEntryPoint_Unchecked()));
            GenerateDynamicInterpreterThunk();
        }
        else if (InterpreterStackFrame::IsDelayDynamicInterpreterThunk(this->GetOriginalEntryPoint_Unchecked()))
        {TRACE_IT(34260);
            JsUtil::JobProcessor * jobProcessor = this->GetScriptContext()->GetThreadContext()->GetJobProcessor();
            if (jobProcessor->ProcessesInBackground())
            {TRACE_IT(34261);
                JsUtil::BackgroundJobProcessor * backgroundJobProcessor = static_cast<JsUtil::BackgroundJobProcessor *>(jobProcessor);
                AutoCriticalSection autocs(backgroundJobProcessor->GetCriticalSection());
                // Check again under lock
                if (InterpreterStackFrame::IsDelayDynamicInterpreterThunk(this->GetOriginalEntryPoint_Unchecked()))
                {TRACE_IT(34262);
                    // If the original entry point is DelayDynamicInterpreterThunk then there must be a version of this
                    // function being codegen'd.
                    Assert(IsIntermediateCodeGenThunk((JavascriptMethod)this->GetEntryPoint(this->GetDefaultEntryPointInfo())) || IsAsmJsCodeGenThunk((JavascriptMethod)this->GetEntryPoint(this->GetDefaultEntryPointInfo())));
                    GenerateDynamicInterpreterThunk();
                }
            }
            else
            {TRACE_IT(34263);
                // If the original entry point is DelayDynamicInterpreterThunk then there must be a version of this
                // function being codegen'd.
                Assert(IsIntermediateCodeGenThunk((JavascriptMethod)this->GetEntryPoint(this->GetDefaultEntryPointInfo())) || IsAsmJsCodeGenThunk((JavascriptMethod)this->GetEntryPoint(this->GetDefaultEntryPointInfo())));
                GenerateDynamicInterpreterThunk();
            }
        }
        return this->GetOriginalEntryPoint_Unchecked();
    }
#endif

#if ENABLE_NATIVE_CODEGEN
    void FunctionBody::SetNativeEntryPoint(FunctionEntryPointInfo* entryPointInfo, JavascriptMethod originalEntryPoint, JavascriptMethod directEntryPoint)
    {TRACE_IT(34264);
        if(entryPointInfo->nativeEntryPointProcessed)
        {TRACE_IT(34265);
            return;
        }
        bool isAsmJs = this->GetIsAsmjsMode();
        Assert(IsIntermediateCodeGenThunk(entryPointInfo->jsMethod) || CONFIG_FLAG(Prejit) || this->m_isFromNativeCodeModule || isAsmJs);
        entryPointInfo->EnsureIsReadyToCall();

        // keep originalEntryPoint updated with the latest known good native entry point
        if (entryPointInfo == this->GetDefaultEntryPointInfo())
        {TRACE_IT(34266);
            this->SetOriginalEntryPoint(originalEntryPoint);
        }

        if (entryPointInfo->entryPointIndex == 0 && this->NeedEnsureDynamicProfileInfo())
        {TRACE_IT(34267);
            entryPointInfo->jsMethod = DynamicProfileInfo::EnsureDynamicProfileInfoThunk;
        }
        else
        {TRACE_IT(34268);
            entryPointInfo->jsMethod = directEntryPoint;
        }
#ifdef ASMJS_PLAT
        if (isAsmJs)
        {TRACE_IT(34269);
            // release the old entrypointinfo if available
            FunctionEntryPointInfo* oldEntryPointInfo = entryPointInfo->GetOldFunctionEntryPointInfo();
            if (oldEntryPointInfo)
            {TRACE_IT(34270);
                this->GetScriptContext()->GetThreadContext()->QueueFreeOldEntryPointInfoIfInScript(oldEntryPointInfo);
                oldEntryPointInfo = nullptr;
            }
        }
#endif
        this->CaptureDynamicProfileState(entryPointInfo);

        if(entryPointInfo->GetJitMode() == ExecutionMode::SimpleJit)
        {TRACE_IT(34271);
            Assert(GetExecutionMode() == ExecutionMode::SimpleJit);
            SetSimpleJitEntryPointInfo(entryPointInfo);
            ResetSimpleJitCallCount();
        }
        else
        {TRACE_IT(34272);
            Assert(entryPointInfo->GetJitMode() == ExecutionMode::FullJit);
            Assert(isAsmJs || GetExecutionMode() == ExecutionMode::FullJit);
            entryPointInfo->callsCount =
                static_cast<uint8>(
                    min(
                        static_cast<uint>(static_cast<uint8>(CONFIG_FLAG(MinBailOutsBeforeRejit))) *
                            (Js::FunctionEntryPointInfo::GetDecrCallCountPerBailout() - 1),
                        0xffu));
        }
        TraceExecutionMode();

        JS_ETW(EtwTrace::LogMethodNativeLoadEvent(this, entryPointInfo));
#ifdef VTUNE_PROFILING
        VTuneChakraProfile::LogMethodNativeLoadEvent(this, entryPointInfo);
#endif

#ifdef _M_ARM
        // For ARM we need to make sure that pipeline is synchronized with memory/cache for newly jitted code.
        _InstructionSynchronizationBarrier();
#endif

        entryPointInfo->nativeEntryPointProcessed = true;
    }

    void FunctionBody::DefaultSetNativeEntryPoint(FunctionEntryPointInfo* entryPointInfo, FunctionBody * functionBody, JavascriptMethod entryPoint)
    {TRACE_IT(34273);
        Assert(functionBody->m_scriptContext->CurrentThunk == DefaultEntryThunk);
        functionBody->SetNativeEntryPoint(entryPointInfo, entryPoint, entryPoint);
    }


    void FunctionBody::ProfileSetNativeEntryPoint(FunctionEntryPointInfo* entryPointInfo, FunctionBody * functionBody, JavascriptMethod entryPoint)
    {TRACE_IT(34274);
#ifdef ENABLE_WASM
        // Do not profile WebAssembly functions
        if (functionBody->IsWasmFunction())
        {TRACE_IT(34275);
            functionBody->SetNativeEntryPoint(entryPointInfo, entryPoint, entryPoint);
            return;
        }
#endif
        Assert(functionBody->m_scriptContext->CurrentThunk == ProfileEntryThunk);
        functionBody->SetNativeEntryPoint(entryPointInfo, entryPoint, ProfileEntryThunk);
    }

    Js::JavascriptMethod FunctionBody::GetLoopBodyEntryPoint(Js::LoopHeader * loopHeader, int entryPointIndex)
    {TRACE_IT(34276);
#if DBG
        this->GetLoopNumber(loopHeader);
#endif
        return loopHeader->GetEntryPointInfo(entryPointIndex)->jsMethod;
    }

    void FunctionBody::SetLoopBodyEntryPoint(Js::LoopHeader * loopHeader, EntryPointInfo* entryPointInfo, Js::JavascriptMethod entryPoint, uint loopNum)
    {TRACE_IT(34277);
#if DBG_DUMP
        if (PHASE_TRACE1(Js::JITLoopBodyPhase))
        {TRACE_IT(34278);
            DumpFunctionId(true);
            Output::Print(_u(": %-20s LoopBody EntryPt  Loop: %2d Address : %x\n"), GetDisplayName(), loopNum, entryPoint);
            Output::Flush();
        }
#endif
        Assert(((LoopEntryPointInfo*)entryPointInfo)->loopHeader == loopHeader);
        Assert(reinterpret_cast<void*>(entryPointInfo->jsMethod) == nullptr);
        entryPointInfo->jsMethod = entryPoint;

        ((Js::LoopEntryPointInfo*)entryPointInfo)->totalJittedLoopIterations =
            static_cast<uint8>(
                min(
                    static_cast<uint>(static_cast<uint8>(CONFIG_FLAG(MinBailOutsBeforeRejitForLoops))) *
                    (Js::LoopEntryPointInfo::GetDecrLoopCountPerBailout() - 1),
                    0xffu));

        // reset the counter to 1 less than the threshold for TJLoopBody
        if (loopHeader->GetCurrentEntryPointInfo()->GetIsAsmJSFunction())
        {TRACE_IT(34279);
            loopHeader->interpretCount = entryPointInfo->GetFunctionBody()->GetLoopInterpretCount(loopHeader) - 1;
        }
        JS_ETW(EtwTrace::LogLoopBodyLoadEvent(this, loopHeader, ((LoopEntryPointInfo*)entryPointInfo), ((uint16)loopNum)));
#ifdef VTUNE_PROFILING
        VTuneChakraProfile::LogLoopBodyLoadEvent(this, loopHeader, ((LoopEntryPointInfo*)entryPointInfo), ((uint16)loopNum));
#endif
    }
#endif

    void FunctionBody::MarkScript(ByteBlock *byteCodeBlock, ByteBlock* auxBlock, ByteBlock* auxContextBlock,
        uint byteCodeCount, uint byteCodeInLoopCount, uint byteCodeWithoutLDACount)
    {TRACE_IT(34280);
        CheckNotExecuting();
        CheckEmpty();

#ifdef PERF_COUNTERS
        DWORD byteCodeSize = byteCodeBlock->GetLength()
            + (auxBlock? auxBlock->GetLength() : 0)
            + (auxContextBlock? auxContextBlock->GetLength() : 0);
        PERF_COUNTER_ADD(Code, DynamicByteCodeSize, byteCodeSize);
#endif

        SetByteCodeCount(byteCodeCount);
        SetByteCodeInLoopCount(byteCodeInLoopCount);
        SetByteCodeWithoutLDACount(byteCodeWithoutLDACount);

        InitializeExecutionModeAndLimits();

        this->SetAuxiliaryData(auxBlock);
        this->SetAuxiliaryContextData(auxContextBlock);

        // Memory barrier needed here to make sure the background codegen thread's inliner
        // gets all the assignment before it sees that the function has been parse
        MemoryBarrier();

        this->byteCodeBlock = byteCodeBlock;
        PERF_COUNTER_ADD(Code, TotalByteCodeSize, byteCodeSize);

        // If this is a defer parse function body, we would not have registered it
        // on the function bodies list so we should register it now
        if (!this->m_isFuncRegistered)
        {TRACE_IT(34281);
            this->GetUtf8SourceInfo()->SetFunctionBody(this);
        }
    }

    uint
    FunctionBody::GetLoopNumber(LoopHeader const * loopHeader) const
    {TRACE_IT(34282);
        LoopHeader* loopHeaderArray = this->GetLoopHeaderArray();
        Assert(loopHeader >= loopHeaderArray);
        uint loopNum = (uint)(loopHeader - loopHeaderArray);
        Assert(loopNum < GetLoopCount());
        return loopNum;
    }
    uint
    FunctionBody::GetLoopNumberWithLock(LoopHeader const * loopHeader) const
    {TRACE_IT(34283);
        LoopHeader* loopHeaderArray = this->GetLoopHeaderArrayWithLock();
        Assert(loopHeader >= loopHeaderArray);
        uint loopNum = (uint)(loopHeader - loopHeaderArray);
        Assert(loopNum < GetLoopCount());
        return loopNum;
    }

    bool FunctionBody::InstallProbe(int offset)
    {TRACE_IT(34284);
        if (offset < 0 || ((uint)offset + 1) >= byteCodeBlock->GetLength())
        {TRACE_IT(34285);
            return false;
        }

        byte* pbyteCodeBlockBuffer = this->byteCodeBlock->GetBuffer();

        if(!GetProbeBackingBlock())
        {TRACE_IT(34286);
            // The probe backing block is set on a different thread than the main thread
            // The recycler doesn't like allocations from a different thread, so we allocate
            // the backing byte code block in the arena
            ArenaAllocator *pArena = m_scriptContext->AllocatorForDiagnostics();
            AssertMem(pArena);
            ByteBlock* probeBackingBlock = ByteBlock::NewFromArena(pArena, pbyteCodeBlockBuffer, byteCodeBlock->GetLength());
            SetProbeBackingBlock(probeBackingBlock);
        }

        // Make sure Break opcode only need one byte
        Assert(OpCodeUtil::IsSmallEncodedOpcode(OpCode::Break));
#if ENABLE_NATIVE_CODEGEN
        Assert(!OpCodeAttr::HasMultiSizeLayout(OpCode::Break));
#endif
        *(byte *)(pbyteCodeBlockBuffer + offset) = (byte)OpCode::Break;

        ++m_sourceInfo.m_probeCount;

        return true;
    }

    bool FunctionBody::UninstallProbe(int offset)
    {TRACE_IT(34287);
        if (offset < 0 || ((uint)offset + 1) >= byteCodeBlock->GetLength())
        {TRACE_IT(34288);
            return false;
        }
        byte* pbyteCodeBlockBuffer = byteCodeBlock->GetBuffer();

        Js::OpCode originalOpCode = ByteCodeReader::PeekByteOp(GetProbeBackingBlock()->GetBuffer() + offset);
        *(pbyteCodeBlockBuffer + offset) = (byte)originalOpCode;

        --m_sourceInfo.m_probeCount;
        AssertMsg(m_sourceInfo.m_probeCount >= 0, "Probe (Break Point) count became negative!");

        return true;
    }

    bool FunctionBody::ProbeAtOffset(int offset, OpCode* pOriginalOpcode)
    {TRACE_IT(34289);
        if (!GetProbeBackingBlock())
        {TRACE_IT(34290);
            return false;
        }

        if (offset < 0 || ((uint)offset + 1) >= this->byteCodeBlock->GetLength())
        {
            AssertMsg(false, "ProbeAtOffset called with out of bounds offset");
            return false;
        }

        Js::OpCode runningOpCode = ByteCodeReader::PeekByteOp(this->byteCodeBlock->GetBuffer() + offset);
        Js::OpCode originalOpcode = ByteCodeReader::PeekByteOp(GetProbeBackingBlock()->GetBuffer() + offset);

        if ( runningOpCode != originalOpcode)
        {TRACE_IT(34291);
            *pOriginalOpcode = originalOpcode;
            return true;
        }
        else
        {TRACE_IT(34292);
            // e.g. inline break or a step hit and is checking for a bp
            return false;
        }
    }

    void FunctionBody::SetStackNestedFuncParent(FunctionInfo * parentFunctionInfo)
    {TRACE_IT(34293);
        FunctionBody * parentFunctionBody = parentFunctionInfo->GetFunctionBody();
        RecyclerWeakReference<FunctionInfo>* parent = this->GetStackNestedFuncParent();
        if (parent != nullptr)
        {TRACE_IT(34294);
            Assert(parent->Get() == parentFunctionInfo);
            return;
        }
//      Redeferral invalidates this assertion, as we may be recompiling with a different view of nested functions and
//      thus making different stack-nested-function decisions. I'm inclined to allow this, since things that have been
//      re-deferred will likely not be executed again, so it makes sense to exclude them from our analysis.
//        Assert(CanDoStackNestedFunc());
        Assert(parentFunctionBody->DoStackNestedFunc());

        this->SetAuxPtr(AuxPointerType::StackNestedFuncParent, this->GetScriptContext()->GetRecycler()->CreateWeakReferenceHandle(parentFunctionInfo));
    }

    FunctionInfo * FunctionBody::GetStackNestedFuncParentStrongRef()
    {TRACE_IT(34295);
        Assert(this->GetStackNestedFuncParent() != nullptr);
        return this->GetStackNestedFuncParent()->Get();
    }

    RecyclerWeakReference<FunctionInfo> * FunctionBody::GetStackNestedFuncParent()
    {TRACE_IT(34296);
        return static_cast<RecyclerWeakReference<FunctionInfo>*>(this->GetAuxPtr(AuxPointerType::StackNestedFuncParent));
    }

    FunctionInfo * FunctionBody::GetAndClearStackNestedFuncParent()
    {TRACE_IT(34297);
        if (this->GetAuxPtr(AuxPointerType::StackNestedFuncParent))
        {TRACE_IT(34298);
            FunctionInfo * parentFunctionInfo = GetStackNestedFuncParentStrongRef();
            ClearStackNestedFuncParent();
            return parentFunctionInfo;
        }
        return nullptr;
    }

    void FunctionBody::ClearStackNestedFuncParent()
    {TRACE_IT(34299);
        this->SetAuxPtr(AuxPointerType::StackNestedFuncParent, nullptr);
    }

    void FunctionBody::CreateCacheIdToPropertyIdMap(uint rootObjectLoadInlineCacheStart, uint rootObjectLoadMethodInlineCacheStart,
        uint rootObjectStoreInlineCacheStart,
        uint totalFieldAccessInlineCacheCount, uint isInstInlineCacheCount)
    {TRACE_IT(34300);
        Assert(this->GetRootObjectLoadInlineCacheStart() == 0);
        Assert(this->GetRootObjectLoadMethodInlineCacheStart() == 0);
        Assert(this->GetRootObjectStoreInlineCacheStart() == 0);
        Assert(this->GetInlineCacheCount() == 0);
        Assert(this->GetIsInstInlineCacheCount() == 0);

        this->SetRootObjectLoadInlineCacheStart(rootObjectLoadInlineCacheStart);
        this->SetRootObjectLoadMethodInlineCacheStart(rootObjectLoadMethodInlineCacheStart);
        this->SetRootObjectStoreInlineCacheStart(rootObjectStoreInlineCacheStart);
        this->SetInlineCacheCount(totalFieldAccessInlineCacheCount);
        this->SetIsInstInlineCacheCount(isInstInlineCacheCount);

        this->CreateCacheIdToPropertyIdMap();
    }

    void FunctionBody::CreateCacheIdToPropertyIdMap()
    {TRACE_IT(34301);
        Assert(this->cacheIdToPropertyIdMap == nullptr);
        Assert(this->inlineCaches == nullptr);
        uint count = this->GetInlineCacheCount() ;
        if (count!= 0)
        {TRACE_IT(34302);
            this->cacheIdToPropertyIdMap =
                RecyclerNewArrayLeaf(this->m_scriptContext->GetRecycler(), PropertyId, count);
#if DBG
            for (uint i = 0; i < count; i++)
            {TRACE_IT(34303);
                this->cacheIdToPropertyIdMap[i] = Js::Constants::NoProperty;
            }
#endif
        }

    }

#if DBG
    void FunctionBody::VerifyCacheIdToPropertyIdMap()
    {TRACE_IT(34304);
        uint count = this->GetInlineCacheCount();
        for (uint i = 0; i < count; i++)
        {TRACE_IT(34305);
            Assert(this->cacheIdToPropertyIdMap[i] != Js::Constants::NoProperty);
        }
    }
#endif

    void FunctionBody::SetPropertyIdForCacheId(uint cacheId, PropertyId propertyId)
    {TRACE_IT(34306);
        Assert(this->cacheIdToPropertyIdMap != nullptr);
        Assert(cacheId < this->GetInlineCacheCount());
        Assert(this->cacheIdToPropertyIdMap[cacheId] == Js::Constants::NoProperty);

        this->cacheIdToPropertyIdMap[cacheId] = propertyId;
    }

    void FunctionBody::CreateReferencedPropertyIdMap(uint referencedPropertyIdCount)
    {TRACE_IT(34307);
        this->SetReferencedPropertyIdCount(referencedPropertyIdCount);
        this->CreateReferencedPropertyIdMap();
    }

    void FunctionBody::CreateReferencedPropertyIdMap()
    {TRACE_IT(34308);
        Assert(this->GetReferencedPropertyIdMap() == nullptr);
        uint count = this->GetReferencedPropertyIdCount();
        if (count!= 0)
        {TRACE_IT(34309);
            this->SetReferencedPropertyIdMap(RecyclerNewArrayLeaf(this->m_scriptContext->GetRecycler(), PropertyId, count));
#if DBG
            for (uint i = 0; i < count; i++)
            {TRACE_IT(34310);
                this->GetReferencedPropertyIdMap()[i] = Js::Constants::NoProperty;
            }
#endif
        }
    }

#if DBG
    void FunctionBody::VerifyReferencedPropertyIdMap()
    {TRACE_IT(34311);
        uint count = this->GetReferencedPropertyIdCount();
        for (uint i = 0; i < count; i++)
        {TRACE_IT(34312);
            Assert(this->GetReferencedPropertyIdMap()[i] != Js::Constants::NoProperty);
        }
    }
#endif

    PropertyId FunctionBody::GetReferencedPropertyId(uint index)
    {TRACE_IT(34313);
        if (index < (uint)TotalNumberOfBuiltInProperties)
        {TRACE_IT(34314);
            return index;
        }
        uint mapIndex = index - TotalNumberOfBuiltInProperties;
        return GetReferencedPropertyIdWithMapIndex(mapIndex);
    }

    PropertyId FunctionBody::GetReferencedPropertyIdWithLock(uint index)
    {TRACE_IT(34315);
        if (index < (uint)TotalNumberOfBuiltInProperties)
        {TRACE_IT(34316);
            return index;
        }
        uint mapIndex = index - TotalNumberOfBuiltInProperties;
        return GetReferencedPropertyIdWithMapIndexWithLock(mapIndex);
    }

    PropertyId FunctionBody::GetReferencedPropertyIdWithMapIndex(uint mapIndex)
    {TRACE_IT(34317);
        Assert(this->GetReferencedPropertyIdMap());
        Assert(mapIndex < this->GetReferencedPropertyIdCount());
        return this->GetReferencedPropertyIdMap()[mapIndex];
    }

    PropertyId FunctionBody::GetReferencedPropertyIdWithMapIndexWithLock(uint mapIndex)
    {TRACE_IT(34318);
        Assert(this->GetReferencedPropertyIdMapWithLock());
        Assert(mapIndex < this->GetReferencedPropertyIdCount());
        return this->GetReferencedPropertyIdMapWithLock()[mapIndex];
    }

    void FunctionBody::SetReferencedPropertyIdWithMapIndex(uint mapIndex, PropertyId propertyId)
    {TRACE_IT(34319);
        Assert(propertyId >= TotalNumberOfBuiltInProperties);
        Assert(mapIndex < this->GetReferencedPropertyIdCount());
        Assert(this->GetReferencedPropertyIdMap() != nullptr);
        Assert(this->GetReferencedPropertyIdMap()[mapIndex] == Js::Constants::NoProperty);
        this->GetReferencedPropertyIdMap()[mapIndex] = propertyId;
    }

    void FunctionBody::CreateConstantTable()
    {TRACE_IT(34320);
        Assert(this->GetConstTable() == nullptr);
        Assert(GetConstantCount() > FirstRegSlot);

        this->SetConstTable(RecyclerNewArrayZ(this->m_scriptContext->GetRecycler(), Field(Var), GetConstantCount()));

        // Initialize with the root object, which will always be recorded here.
        Js::RootObjectBase * rootObject = this->LoadRootObject();
        if (rootObject)
        {TRACE_IT(34321);
            this->RecordConstant(RootObjectRegSlot, rootObject);
        }
        else
        {TRACE_IT(34322);
            Assert(false);
            this->RecordConstant(RootObjectRegSlot, this->m_scriptContext->GetLibrary()->GetUndefined());
        }

    }

    void FunctionBody::RecordConstant(RegSlot location, Var var)
    {TRACE_IT(34323);
        Assert(location < GetConstantCount());
        Assert(this->GetConstTable());
        Assert(var != nullptr);
        Assert(this->GetConstTable()[location - FunctionBody::FirstRegSlot] == nullptr);
        this->GetConstTable()[location - FunctionBody::FirstRegSlot] = var;
    }

    void FunctionBody::RecordNullObject(RegSlot location)
    {TRACE_IT(34324);
        ScriptContext *scriptContext = this->GetScriptContext();
        Var nullObject = JavascriptOperators::OP_LdNull(scriptContext);
        this->RecordConstant(location, nullObject);
    }

    void FunctionBody::RecordUndefinedObject(RegSlot location)
    {TRACE_IT(34325);
        ScriptContext *scriptContext = this->GetScriptContext();
        Var undefObject = JavascriptOperators::OP_LdUndef(scriptContext);
        this->RecordConstant(location, undefObject);
    }

    void FunctionBody::RecordTrueObject(RegSlot location)
    {TRACE_IT(34326);
        ScriptContext *scriptContext = this->GetScriptContext();
        Var trueObject = JavascriptBoolean::OP_LdTrue(scriptContext);
        this->RecordConstant(location, trueObject);
    }

    void FunctionBody::RecordFalseObject(RegSlot location)
    {TRACE_IT(34327);
        ScriptContext *scriptContext = this->GetScriptContext();
        Var falseObject = JavascriptBoolean::OP_LdFalse(scriptContext);
        this->RecordConstant(location, falseObject);
    }

    void FunctionBody::RecordIntConstant(RegSlot location, unsigned int val)
    {TRACE_IT(34328);
        ScriptContext *scriptContext = this->GetScriptContext();
        Var intConst = JavascriptNumber::ToVar((int32)val, scriptContext);
        this->RecordConstant(location, intConst);
    }

    void FunctionBody::RecordStrConstant(RegSlot location, LPCOLESTR psz, uint32 cch)
    {TRACE_IT(34329);
        ScriptContext *scriptContext = this->GetScriptContext();
        PropertyRecord const * propertyRecord;
        scriptContext->FindPropertyRecord(psz, cch, &propertyRecord);
        Var str;
        if (propertyRecord == nullptr)
        {TRACE_IT(34330);
            str = JavascriptString::NewCopyBuffer(psz, cch, scriptContext);
        }
        else
        {TRACE_IT(34331);
            // If a particular string constant already has a propertyId, just create a property string for it
            // as it might be likely that it is used for a property lookup
            str = scriptContext->GetPropertyString(propertyRecord->GetPropertyId());
        }
        this->RecordConstant(location, str);
    }

    void FunctionBody::RecordFloatConstant(RegSlot location, double d)
    {TRACE_IT(34332);
        ScriptContext *scriptContext = this->GetScriptContext();
        Var floatConst = JavascriptNumber::ToVarIntCheck(d, scriptContext);

        this->RecordConstant(location, floatConst);
    }

    void FunctionBody::RecordNullDisplayConstant(RegSlot location)
    {TRACE_IT(34333);
        this->RecordConstant(location, (Js::Var)&Js::NullFrameDisplay);
    }

    void FunctionBody::RecordStrictNullDisplayConstant(RegSlot location)
    {TRACE_IT(34334);
        this->RecordConstant(location, (Js::Var)&Js::StrictNullFrameDisplay);
    }

    void FunctionBody::InitConstantSlots(Var *dstSlots)
    {TRACE_IT(34335);
        // Initialize the given slots from the constant table.
        uint32 constCount = GetConstantCount();
        Assert(constCount > FunctionBody::FirstRegSlot);

        js_memcpy_s(dstSlots, (constCount - FunctionBody::FirstRegSlot) * sizeof(Var),
            this->GetConstTable(), (constCount - FunctionBody::FirstRegSlot) * sizeof(Var));
    }


    Var FunctionBody::GetConstantVar(RegSlot location)
    {TRACE_IT(34336);
        Assert(this->GetConstTable());
        Assert(location < GetConstantCount());
        Assert(location != 0);

        return this->GetConstTable()[location - FunctionBody::FirstRegSlot];
    }

#if DBG_DUMP
    void FunctionBody::Dump()
    {TRACE_IT(34337);
        Js::ByteCodeDumper::Dump(this);
    }

    void FunctionBody::DumpScopes()
    {TRACE_IT(34338);
        if(this->GetScopeObjectChain())
        {TRACE_IT(34339);
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

            Output::Print(_u("%s (%s) :\n"), this->GetDisplayName(), this->GetDebugNumberSet(debugStringBuffer));
            this->GetScopeObjectChain()->pScopeChain->Map( [=] (uint index, DebuggerScope* scope )
            {
                scope->Dump();
            });
        }
    }

#if ENABLE_NATIVE_CODEGEN
    void EntryPointInfo::DumpNativeOffsetMaps()
    {TRACE_IT(34340);
        // Native Offsets
        if (this->nativeOffsetMaps.Count() > 0)
        {TRACE_IT(34341);
            Output::Print(_u("Native Map: baseAddr: 0x%0Ix, size: 0x%0Ix\nstatementId, offset range, address range\n"),
                          this->GetNativeAddress(),
                          this->GetCodeSize());


            int count = this->nativeOffsetMaps.Count();
            for(int i = 0; i < count; i++)
            {TRACE_IT(34342);
                const NativeOffsetMap* map = &this->nativeOffsetMaps.Item(i);

                Output::Print(_u("S%4d, (%5d, %5d)  (0x%012Ix, 0x%012Ix)\n"), map->statementIndex,
                                                      map->nativeOffsetSpan.begin,
                                                      map->nativeOffsetSpan.end,
                                                      map->nativeOffsetSpan.begin + this->GetNativeAddress(),
                                                      map->nativeOffsetSpan.end + this->GetNativeAddress());
            }
        }
    }
#endif

    void FunctionBody::DumpStatementMaps()
    {TRACE_IT(34343);
        // Source Map to ByteCode
        StatementMapList * pStatementMaps = this->GetStatementMaps();
        if (pStatementMaps)
        {TRACE_IT(34344);
            Output::Print(_u("Statement Map:\nstatementId, SourceSpan, ByteCodeSpan\n"));
            int count = pStatementMaps->Count();
            for(int i = 0; i < count; i++)
            {TRACE_IT(34345);
                StatementMap* map = pStatementMaps->Item(i);

                Output::Print(_u("S%4d, (C%5d, C%5d)  (B%5d, B%5d) Inner=%d\n"), i,
                                                      map->sourceSpan.begin,
                                                      map->sourceSpan.end,
                                                      map->byteCodeSpan.begin,
                                                      map->byteCodeSpan.end,
                                                      map->isSubexpression);
            }
        }
    }

#if ENABLE_NATIVE_CODEGEN
    void EntryPointInfo::DumpNativeThrowSpanSequence()
    {TRACE_IT(34346);
        // Native Throw Map
        if (this->nativeThrowSpanSequence)
        {TRACE_IT(34347);
            Output::Print(_u("Native Throw Map: baseAddr: 0x%0Ix, size: 0x%Ix\nstatementId, offset range, address range\n"),
                          this->GetNativeAddress(),
                          this->GetCodeSize());

            int count = this->nativeThrowSpanSequence->Count();
            SmallSpanSequenceIter iter;
            for (int i = 0; i < count; i++)
            {TRACE_IT(34348);
                StatementData data;
                if (this->nativeThrowSpanSequence->Item(i, iter, data))
                {TRACE_IT(34349);
                    Output::Print(_u("S%4d, (%5d -----)  (0x%012Ix --------)\n"), data.sourceBegin, // statementIndex
                        data.bytecodeBegin, // nativeOffset
                        data.bytecodeBegin + this->GetNativeAddress());
                }
            }
        }
    }
#endif

    void FunctionBody::PrintStatementSourceLine(uint statementIndex)
    {TRACE_IT(34350);
        if (m_isWasmFunction)
        {TRACE_IT(34351);
            // currently no source view support for wasm
            return;
        }

        const uint startOffset = GetStatementStartOffset(statementIndex);

        // startOffset should only be 0 if statementIndex is 0, otherwise it is EOF and we should skip printing anything
        if (startOffset != 0 || statementIndex == 0)
        {TRACE_IT(34352);
            PrintStatementSourceLineFromStartOffset(startOffset);
        }
    }

    void FunctionBody::PrintStatementSourceLineFromStartOffset(uint cchStartOffset)
    {TRACE_IT(34353);
        ULONG line;
        LONG col;

        LPCUTF8 source = GetStartOfDocument(_u("FunctionBody::PrintStatementSourceLineFromStartOffset"));
        Utf8SourceInfo* sourceInfo = this->GetUtf8SourceInfo();
        Assert(sourceInfo != nullptr);
        LPCUTF8 sourceInfoSrc = sourceInfo->GetSource(_u("FunctionBody::PrintStatementSourceLineFromStartOffset"));
        if(!sourceInfoSrc)
        {TRACE_IT(34354);
            Assert(sourceInfo->GetIsLibraryCode());
            return;
        }
        if( source != sourceInfoSrc )
        {TRACE_IT(34355);
            Output::Print(_u("\nDETECTED MISMATCH:\n"));
            Output::Print(_u("GetUtf8SourceInfo()->GetSource(): 0x%08X: %.*s ...\n"), sourceInfo, 16, sourceInfo);
            Output::Print(_u("GetStartOfDocument():             0x%08X: %.*s ...\n"), source, 16, source);

            AssertMsg(false, "Non-matching start of document");
        }

        GetLineCharOffsetFromStartChar(cchStartOffset, &line, &col, false /*canAllocateLineCache*/);

        WORD color = 0;
        if (Js::Configuration::Global.flags.DumpLineNoInColor)
        {TRACE_IT(34356);
            color = Output::SetConsoleForeground(12);
        }
        Output::Print(_u("\n\n  Line %3d: "), line + 1);
        // Need to match up cchStartOffset to appropriate cbStartOffset given function's cbStartOffset and cchStartOffset
        size_t i = utf8::CharacterIndexToByteIndex(source, sourceInfo->GetCbLength(), cchStartOffset, this->m_cbStartOffset, this->m_cchStartOffset);

        size_t lastOffset = StartOffset() + LengthInBytes();
        for (;i < lastOffset && source[i] != '\n' && source[i] != '\r'; i++)
        {TRACE_IT(34357);
            Output::Print(_u("%C"), source[i]);
        }
        Output::Print(_u("\n"));
        Output::Print(_u("  Col %4d:%s^\n"), col + 1, ((col+1)<10000) ? _u(" ") : _u(""));

        if (color != 0)
        {TRACE_IT(34358);
            Output::SetConsoleForeground(color);
        }
    }
#endif // DBG_DUMP

    /**
     * Get the source code offset for the given <statementIndex>.
     */
    uint FunctionBody::GetStatementStartOffset(const uint statementIndex)
    {TRACE_IT(34359);
        uint startOffset = 0;

        if (statementIndex != Js::Constants::NoStatementIndex)
        {TRACE_IT(34360);
            const Js::FunctionBody::SourceInfo * sourceInfo = &(this->m_sourceInfo);
            if (sourceInfo->pSpanSequence != nullptr)
            {TRACE_IT(34361);
                Js::SmallSpanSequenceIter iter;
                sourceInfo->pSpanSequence->Reset(iter);
                Js::StatementData data;
                sourceInfo->pSpanSequence->Item(statementIndex, iter, data);
                startOffset = data.sourceBegin;
            }
            else
            {TRACE_IT(34362);
                int index = statementIndex;
                Js::FunctionBody::StatementMap * statementMap = GetNextNonSubexpressionStatementMap(GetStatementMaps(), index);
                startOffset = statementMap->sourceSpan.Begin();
            }
        }

        return startOffset;
    }

#ifdef IR_VIEWER
/* BEGIN potentially reusable code */

/*
    This code could be reused for locating source code in a debugger or to
    retrieve the text of source statements.

    Currently this code is used to retrieve the text of a source code statement
    in the IR_VIEWER feature.
*/

    /**
     * Given a statement's starting offset in the source code, calculate the beginning and end of a statement,
     * as well as the line and column number where the statement appears.
     *
     * @param startOffset (input) The offset into the source code where this statement begins.
     * @param sourceBegin (output) The beginning of the statement in the source string.
     * @param sourceEnd (output) The end of the statement in the source string.
     * @param line (output) The line number where the statement appeared in the source.
     * @param col (output) The column number where the statement appeared in the source.
     */
    void FunctionBody::GetSourceLineFromStartOffset(const uint startOffset, LPCUTF8 *sourceBegin, LPCUTF8 *sourceEnd,
                                                    ULONG * line, LONG * col)
    {TRACE_IT(34363);
        //
        // get source info
        //

        LPCUTF8 source = GetStartOfDocument(_u("IR Viewer FunctionBody::GetSourceLineFromStartOffset"));
        Utf8SourceInfo* sourceInfo = this->GetUtf8SourceInfo();
        Assert(sourceInfo != nullptr);
        LPCUTF8 sourceInfoSrc = sourceInfo->GetSource(_u("IR Viewer FunctionBody::GetSourceLineFromStartOffset"));
        if (!sourceInfoSrc)
        {TRACE_IT(34364);
            Assert(sourceInfo->GetIsLibraryCode());
            return;
        }
        if (source != sourceInfoSrc)
        {TRACE_IT(34365);
            Output::Print(_u("\nDETECTED MISMATCH:\n"));
            Output::Print(_u("GetUtf8SourceInfo()->GetSource(): 0x%08X: %.*s ...\n"), sourceInfo, 16, sourceInfo);
            Output::Print(_u("GetStartOfDocument():             0x%08X: %.*s ...\n"), source, 16, source);

            AssertMsg(false, "Non-matching start of document");
        }

        //
        // calculate source line info
        //

        size_t cbStartOffset = utf8::CharacterIndexToByteIndex(source, sourceInfo->GetCbLength(), (const charcount_t)startOffset, (size_t)this->m_cbStartOffset, (charcount_t)this->m_cchStartOffset);
        GetLineCharOffsetFromStartChar(startOffset, line, col);

        size_t lastOffset = StartOffset() + LengthInBytes();
        size_t i = 0;
        for (i = cbStartOffset; i < lastOffset && source[i] != '\n' && source[i] != '\r'; i++)
        {TRACE_IT(34366);
            // do nothing; scan until end of statement
        }
        size_t cbEndOffset = i;

        //
        // return
        //

        *sourceBegin = &source[cbStartOffset];
        *sourceEnd = &source[cbEndOffset];
    }

    /**
     * Given a statement index and output parameters, calculate the beginning and end of a statement,
     * as well as the line and column number where the statement appears.
     *
     * @param statementIndex (input) The statement's index (as used by the StatementBoundary pragma).
     * @param sourceBegin (output) The beginning of the statement in the source string.
     * @param sourceEnd (output) The end of the statement in the source string.
     * @param line (output) The line number where the statement appeared in the source.
     * @param col (output) The column number where the statement appeared in the source.
     */
    void FunctionBody::GetStatementSourceInfo(const uint statementIndex, LPCUTF8 *sourceBegin, LPCUTF8 *sourceEnd,
        ULONG * line, LONG * col)
    {TRACE_IT(34367);
        const size_t startOffset = GetStatementStartOffset(statementIndex);

        // startOffset should only be 0 if statementIndex is 0, otherwise it is EOF and we should return empty string
        if (startOffset != 0 || statementIndex == 0)
        {
            GetSourceLineFromStartOffset(startOffset, sourceBegin, sourceEnd, line, col);
        }
        else
        {TRACE_IT(34368);
            *sourceBegin = nullptr;
            *sourceEnd = nullptr;
            *line = 0;
            *col = 0;
            return;
        }
    }

/* END potentially reusable code */
#endif /* IR_VIEWER */

#if ENABLE_TTD
    void FunctionBody::GetSourceLineFromStartOffset_TTD(const uint startOffset, ULONG* line, LONG* col)
    {
        GetLineCharOffsetFromStartChar(startOffset, line, col);
    }
#endif

#ifdef IR_VIEWER
    Js::DynamicObject * FunctionBody::GetIRDumpBaseObject()
    {TRACE_IT(34369);
        if (!this->m_irDumpBaseObject)
        {TRACE_IT(34370);
            this->m_irDumpBaseObject = this->m_scriptContext->GetLibrary()->CreateObject();
        }
        return this->m_irDumpBaseObject;
    }
#endif /* IR_VIEWER */

#ifdef VTUNE_PROFILING
#include "jitprofiling.h"

    int EntryPointInfo::GetNativeOffsetMapCount() const
    {TRACE_IT(34371);
        return this->nativeOffsetMaps.Count();
    }

    uint EntryPointInfo::PopulateLineInfo(void* pInfo, FunctionBody* body)
    {TRACE_IT(34372);
        LineNumberInfo* pLineInfo = (LineNumberInfo*)pInfo;
        ULONG functionLineNumber = body->GetLineNumber();
        pLineInfo[0].Offset = 0;
        pLineInfo[0].LineNumber = functionLineNumber;

        int lineNumber = 0;
        int j = 1; // start with 1 since offset 0 has already been populated with function line number
        int count = this->nativeOffsetMaps.Count();
        for(int i = 0; i < count; i++)
        {TRACE_IT(34373);
            const NativeOffsetMap* map = &this->nativeOffsetMaps.Item(i);
            uint32 statementIndex = map->statementIndex;
            if (statementIndex == 0)
            {TRACE_IT(34374);
                // statementIndex is 0, first line in the function, populate with function line number
                pLineInfo[j].Offset = map->nativeOffsetSpan.begin;
                pLineInfo[j].LineNumber = functionLineNumber;
                j++;
            }

            lineNumber = body->GetSourceLineNumber(statementIndex);
            if (lineNumber != 0)
            {TRACE_IT(34375);
                pLineInfo[j].Offset = map->nativeOffsetSpan.end;
                pLineInfo[j].LineNumber = lineNumber;
                j++;
            }
        }

        return j;
    }

    ULONG FunctionBody::GetSourceLineNumber(uint statementIndex)
    {TRACE_IT(34376);
        ULONG line = 0;
        if (statementIndex != Js::Constants::NoStatementIndex)
        {TRACE_IT(34377);
            uint startOffset = GetStartOffset(statementIndex);

            if (startOffset != 0 || statementIndex == 0)
            {
                GetLineCharOffsetFromStartChar(startOffset, &line, nullptr, false /*canAllocateLineCache*/);
                line = line + 1;
            }
        }

        return line;
    }

    uint FunctionBody::GetStartOffset(uint statementIndex) const
    {TRACE_IT(34378);
        uint startOffset = 0;

        const Js::FunctionBody::SourceInfo * sourceInfo = &this->m_sourceInfo;
        if (sourceInfo->pSpanSequence != nullptr)
        {TRACE_IT(34379);
            Js::SmallSpanSequenceIter iter;
            sourceInfo->pSpanSequence->Reset(iter);
            Js::StatementData data;
            sourceInfo->pSpanSequence->Item(statementIndex, iter, data);
            startOffset = data.sourceBegin;
        }
        else
        {TRACE_IT(34380);
            int index = statementIndex;
            Js::FunctionBody::StatementMap * statementMap = GetNextNonSubexpressionStatementMap(GetStatementMaps(), index);
            startOffset = statementMap->sourceSpan.Begin();
        }

        return startOffset;
    }
#endif

    void ParseableFunctionInfo::SetIsNonUserCode(bool set)
    {
        // Mark current function as a non-user code, so that we can distinguish cases where exceptions are
        // caught in non-user code (see ProbeContainer::HasAllowedForException).
        SetFlags(set, Flags_NonUserCode);

        // Propagate setting for all functions in this scope (nested).
        this->ForEachNestedFunc([&](FunctionProxy* proxy, uint32 index)
        {
            ParseableFunctionInfo * pBody = proxy->GetParseableFunctionInfo();
            if (pBody != nullptr)
            {TRACE_IT(34381);
                pBody->SetIsNonUserCode(set);
            }
            return true;
        });
    }

    void FunctionBody::InsertSymbolToRegSlotList(JsUtil::CharacterBuffer<WCHAR> const& propName, RegSlot reg, RegSlot totalRegsCount)
    {TRACE_IT(34382);
        if (totalRegsCount > 0)
        {TRACE_IT(34383);
            PropertyId propertyId = GetOrAddPropertyIdTracked(propName);
            InsertSymbolToRegSlotList(reg, propertyId, totalRegsCount);
        }
    }

    void FunctionBody::InsertSymbolToRegSlotList(RegSlot reg, PropertyId propertyId, RegSlot totalRegsCount)
    {TRACE_IT(34384);
        if (totalRegsCount > 0)
        {TRACE_IT(34385);
            if (this->GetPropertyIdOnRegSlotsContainer() == nullptr)
            {TRACE_IT(34386);
                this->SetPropertyIdOnRegSlotsContainer(PropertyIdOnRegSlotsContainer::New(m_scriptContext->GetRecycler()));
            }

            if (this->GetPropertyIdOnRegSlotsContainer()->propertyIdsForRegSlots == nullptr)
            {TRACE_IT(34387);
                this->GetPropertyIdOnRegSlotsContainer()->CreateRegSlotsArray(m_scriptContext->GetRecycler(), totalRegsCount);
            }

            Assert(this->GetPropertyIdOnRegSlotsContainer() != nullptr);
            this->GetPropertyIdOnRegSlotsContainer()->Insert(reg, propertyId);
        }
    }

    void FunctionBody::SetPropertyIdsOfFormals(PropertyIdArray * formalArgs)
    {TRACE_IT(34388);
        Assert(formalArgs);
        if (this->GetPropertyIdOnRegSlotsContainer() == nullptr)
        {TRACE_IT(34389);
            this->SetPropertyIdOnRegSlotsContainer(PropertyIdOnRegSlotsContainer::New(m_scriptContext->GetRecycler()));
        }
        this->GetPropertyIdOnRegSlotsContainer()->SetFormalArgs(formalArgs);
    }

#ifdef ENABLE_SCRIPT_PROFILING
    HRESULT FunctionBody::RegisterFunction(BOOL fChangeMode, BOOL fOnlyCurrent)
    {TRACE_IT(34390);
        if (!this->IsFunctionParsed())
        {TRACE_IT(34391);
            return S_OK;
        }

        HRESULT hr = this->ReportFunctionCompiled();
        if (FAILED(hr))
        {TRACE_IT(34392);
            return hr;
        }

        if (fChangeMode)
        {TRACE_IT(34393);
            this->SetEntryToProfileMode();
        }

        if (!fOnlyCurrent)
        {TRACE_IT(34394);
            for (uint uIndex = 0; uIndex < this->GetNestedCount(); uIndex++)
            {TRACE_IT(34395);
                Js::ParseableFunctionInfo * pBody = this->GetNestedFunctionForExecution(uIndex);
                if (pBody == nullptr || !pBody->IsFunctionParsed())
                {TRACE_IT(34396);
                    continue;
                }

                hr = pBody->GetFunctionBody()->RegisterFunction(fChangeMode);
                if (FAILED(hr))
                {TRACE_IT(34397);
                    break;
                }
            }
        }
        return hr;
    }

    HRESULT FunctionBody::ReportScriptCompiled()
    {TRACE_IT(34398);
        AssertMsg(m_scriptContext != nullptr, "Script Context is null when reporting function information");

        PROFILER_SCRIPT_TYPE type = IsDynamicScript() ? PROFILER_SCRIPT_TYPE_DYNAMIC : PROFILER_SCRIPT_TYPE_USER;

        IDebugDocumentContext *pDebugDocumentContext = nullptr;
        this->m_scriptContext->GetDocumentContext(this->m_scriptContext, this, &pDebugDocumentContext);

        HRESULT hr = m_scriptContext->OnScriptCompiled((PROFILER_TOKEN) this->GetUtf8SourceInfo()->GetSourceInfoId(), type, pDebugDocumentContext);

        RELEASEPTR(pDebugDocumentContext);

        return hr;
    }

    HRESULT FunctionBody::ReportFunctionCompiled()
    {TRACE_IT(34399);
        // Some assumptions by Logger interface.
        // to send NULL as a name in case the name is anonymous and hint is anonymous code.
        const char16 *pwszName = GetExternalDisplayName();

        IDebugDocumentContext *pDebugDocumentContext = nullptr;
        this->m_scriptContext->GetDocumentContext(this->m_scriptContext, this, &pDebugDocumentContext);

        SetHasFunctionCompiledSent(true);

        HRESULT hr = m_scriptContext->OnFunctionCompiled(m_functionNumber, (PROFILER_TOKEN) this->GetUtf8SourceInfo()->GetSourceInfoId(), pwszName, nullptr, pDebugDocumentContext);
        RELEASEPTR(pDebugDocumentContext);

#if DBG
        if (m_iProfileSession >= m_scriptContext->GetProfileSession())
        {TRACE_IT(34400);
            OUTPUT_TRACE_DEBUGONLY(Js::ScriptProfilerPhase, _u("FunctionBody::ReportFunctionCompiled, Duplicate compile event (%d < %d) for FunctionNumber : %d\n"),
                m_iProfileSession, m_scriptContext->GetProfileSession(), m_functionNumber);
        }

        AssertMsg(m_iProfileSession < m_scriptContext->GetProfileSession(), "Duplicate compile event sent");
        m_iProfileSession = m_scriptContext->GetProfileSession();
#endif

        return hr;
    }

    void FunctionBody::SetEntryToProfileMode()
    {TRACE_IT(34401);
#if ENABLE_NATIVE_CODEGEN
        AssertMsg(this->m_scriptContext->CurrentThunk == ProfileEntryThunk, "ScriptContext not in profile mode");
#if DBG
        AssertMsg(m_iProfileSession == m_scriptContext->GetProfileSession(), "Changing mode to profile for function that didn't send compile event");
#endif
        // This is always done when bg thread is paused hence we don't need any kind of thread-synchronization at this point.

        // Change entry points to Profile Thunk
        //  If the entrypoint is CodeGenOnDemand or CodeGen - then we don't change the entry points
        ProxyEntryPointInfo* defaultEntryPointInfo = this->GetDefaultEntryPointInfo();

        if (!IsIntermediateCodeGenThunk(defaultEntryPointInfo->jsMethod)
            && defaultEntryPointInfo->jsMethod != DynamicProfileInfo::EnsureDynamicProfileInfoThunk)
        {TRACE_IT(34402);
            if (this->GetOriginalEntryPoint_Unchecked() == DefaultDeferredParsingThunk)
            {TRACE_IT(34403);
                defaultEntryPointInfo->jsMethod = ProfileDeferredParsingThunk;
            }
            else if (this->GetOriginalEntryPoint_Unchecked() == DefaultDeferredDeserializeThunk)
            {TRACE_IT(34404);
                defaultEntryPointInfo->jsMethod = ProfileDeferredDeserializeThunk;
            }
            else
            {TRACE_IT(34405);
                defaultEntryPointInfo->jsMethod = ProfileEntryThunk;
            }
        }

        // Update old entry points on the deferred prototype type so that they match current defaultEntryPointInfo.
        // to make sure that new JavascriptFunction instances use profile thunk.
        if (this->deferredPrototypeType)
        {TRACE_IT(34406);
            this->deferredPrototypeType->SetEntryPoint(this->GetDefaultEntryPointInfo()->jsMethod);
            this->deferredPrototypeType->SetEntryPointInfo(this->GetDefaultEntryPointInfo());
        }

#if DBG
        if (!this->HasValidEntryPoint())
        {TRACE_IT(34407);
            OUTPUT_TRACE_DEBUGONLY(Js::ScriptProfilerPhase, _u("FunctionBody::SetEntryToProfileMode, Assert due to HasValidEntryPoint(), directEntrypoint : 0x%0IX, originalentrypoint : 0x%0IX\n"),
                this->GetDefaultEntryPointInfo()->jsMethod, this->GetOriginalEntryPoint());

            AssertMsg(false, "Not a valid EntryPoint");
        }
#endif

#endif //ENABLE_NATIVE_CODEGEN
    }
#endif // ENABLE_SCRIPT_PROFILING

#if DBG
    void FunctionBody::MustBeInDebugMode()
    {TRACE_IT(34408);
        Assert(GetUtf8SourceInfo()->IsInDebugMode());
        Assert(m_sourceInfo.pSpanSequence == nullptr);
        Assert(this->GetStatementMaps() != nullptr);
    }
#endif

    void FunctionBody::CleanupToReparse()
    {TRACE_IT(34409);
#if DBG
        bool isCleaningUpOldValue = this->counters.isCleaningUp;
        this->counters.isCleaningUp = true;
#endif
        // The current function is already compiled. In order to prep this function to ready for debug mode, most of the previous information need to be thrown away.
        // Clean up the nested functions
        this->ForEachNestedFunc([&](FunctionProxy* proxy, uint32 index)
        {
            if (proxy && proxy->IsFunctionBody())
            {TRACE_IT(34410);
                proxy->GetFunctionBody()->CleanupToReparse();
            }
            return true;
        });

        CleanupRecyclerData(/* isShutdown */ false, true /* capture entry point cleanup stack trace */);

        this->entryPoints->ClearAndZero();

#if DYNAMIC_INTERPRETER_THUNK
        if (m_isAsmJsFunction && m_dynamicInterpreterThunk)
        {TRACE_IT(34411);
            m_scriptContext->ReleaseDynamicAsmJsInterpreterThunk((BYTE*)this->m_dynamicInterpreterThunk, true);
            this->m_dynamicInterpreterThunk = nullptr;
        }
#endif

        // Store the originalEntryPoint to restore it back immediately.
        JavascriptMethod originalEntryPoint = this->GetOriginalEntryPoint_Unchecked();
        this->CreateNewDefaultEntryPoint();
        this->SetOriginalEntryPoint(originalEntryPoint);
        if (this->m_defaultEntryPointInfo)
        {TRACE_IT(34412);
            this->GetDefaultFunctionEntryPointInfo()->entryPointIndex = 0;
        }

        this->SetAuxiliaryData(nullptr);
        this->SetAuxiliaryContextData(nullptr);
        this->byteCodeBlock = nullptr;
        this->SetLoopHeaderArray(nullptr);
        this->SetConstTable(nullptr);
        this->SetScopeInfo(nullptr);
        this->SetCodeGenRuntimeData(nullptr);
        this->cacheIdToPropertyIdMap = nullptr;
        this->SetFormalsPropIdArray(nullptr);
        this->SetReferencedPropertyIdMap(nullptr);
        this->SetLiteralRegexs(nullptr);
        this->SetPropertyIdsForScopeSlotArray(nullptr, 0);
        this->SetStatementMaps(nullptr);
        this->SetCodeGenGetSetRuntimeData(nullptr);
        this->SetPropertyIdOnRegSlotsContainer(nullptr);
        this->profiledLdElemCount = 0;
        this->profiledStElemCount = 0;
        this->profiledCallSiteCount = 0;
        this->profiledArrayCallSiteCount = 0;
        this->profiledDivOrRemCount = 0;
        this->profiledSwitchCount = 0;
        this->profiledReturnTypeCount = 0;
        this->profiledSlotCount = 0;
        this->SetLoopCount(0);

        this->m_envDepth = (uint16)-1;

        this->SetByteCodeCount(0);
        this->SetByteCodeWithoutLDACount(0);
        this->SetByteCodeInLoopCount(0);

#if ENABLE_PROFILE_INFO
        this->dynamicProfileInfo = nullptr;
#endif
        this->hasExecutionDynamicProfileInfo = false;

        this->SetFirstTmpRegister(Constants::NoRegister);
        this->SetVarCount(0);
        this->SetConstantCount(0);
        this->SetLocalClosureRegister(Constants::NoRegister);
        this->SetParamClosureRegister(Constants::NoRegister);
        this->SetLocalFrameDisplayRegister(Constants::NoRegister);
        this->SetEnvRegister(Constants::NoRegister);
        this->SetThisRegisterForEventHandler(Constants::NoRegister);
        this->SetFirstInnerScopeRegister(Constants::NoRegister);
        this->SetFuncExprScopeRegister(Constants::NoRegister);
        this->SetInnerScopeCount(0);
        this->hasCachedScopePropIds = false;

        this->ResetObjectLiteralTypes();

        this->SetInlineCacheCount(0);
        this->SetRootObjectLoadInlineCacheStart(0);
        this->SetRootObjectLoadMethodInlineCacheStart(0);
        this->SetRootObjectStoreInlineCacheStart(0);
        this->SetIsInstInlineCacheCount(0);
        this->m_inlineCachesOnFunctionObject = false;
        this->SetReferencedPropertyIdCount(0);
#if ENABLE_PROFILE_INFO
        this->SetPolymorphicCallSiteInfoHead(nullptr);
#endif

        this->SetInterpretedCount(0);

        this->m_hasDoneAllNonLocalReferenced = false;

        this->SetDebuggerScopeIndex(0);
        this->GetUtf8SourceInfo()->DeleteLineOffsetCache();

        // Reset to default.
        this->flags = this->IsClassConstructor() ? Flags_None : Flags_HasNoExplicitReturnValue;

        ResetInParams();

        this->m_isAsmjsMode = false;
        this->m_isAsmJsFunction = false;
        this->m_isAsmJsScheduledForFullJIT = false;
        this->m_asmJsTotalLoopCount = 0;

        recentlyBailedOutOfJittedLoopBody = false;

        SetLoopInterpreterLimit(CONFIG_FLAG(LoopInterpretCount));
        ReinitializeExecutionModeAndLimits();

        Assert(this->m_sourceInfo.m_probeCount == 0);
        this->m_sourceInfo.m_probeBackingBlock = nullptr;

#if DBG
        // This could be non-zero if the function threw exception before. Reset it.
        this->m_DEBUG_executionCount = 0;
#endif
        if (this->m_sourceInfo.pSpanSequence != nullptr)
        {TRACE_IT(34413);
            HeapDelete(this->m_sourceInfo.pSpanSequence);
            this->m_sourceInfo.pSpanSequence = nullptr;
        }

        if (this->m_sourceInfo.m_auxStatementData != nullptr)
        {TRACE_IT(34414);
            // This must be consistent with how we allocate the data for this and inner structures.
            // We are using recycler, thus it's enough just to set to NULL.
            Assert(m_scriptContext->GetRecycler()->IsValidObject(m_sourceInfo.m_auxStatementData));
            m_sourceInfo.m_auxStatementData = nullptr;
        }

#if DBG
        this->counters.isCleaningUp = isCleaningUpOldValue;
#endif
    }

    void FunctionBody::SetEntryToDeferParseForDebugger()
    {TRACE_IT(34415);
        ProxyEntryPointInfo* defaultEntryPointInfo = this->GetDefaultEntryPointInfo();
        if (defaultEntryPointInfo->jsMethod != DefaultDeferredParsingThunk
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
            && defaultEntryPointInfo->jsMethod != ProfileDeferredParsingThunk
#endif
            )
        {TRACE_IT(34416);
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
            // Just change the thunk, the cleanup will be done once the function gets called.
            if (this->m_scriptContext->CurrentThunk == ProfileEntryThunk)
            {TRACE_IT(34417);
                defaultEntryPointInfo->jsMethod = ProfileDeferredParsingThunk;
            }
            else
#endif
            {TRACE_IT(34418);
                defaultEntryPointInfo->jsMethod = DefaultDeferredParsingThunk;
            }

            this->SetOriginalEntryPoint(DefaultDeferredParsingThunk);

            // Abandon the shared type so a new function will get a new one
            this->deferredPrototypeType = nullptr;
            this->SetAttributes((FunctionInfo::Attributes) (this->GetAttributes() | FunctionInfo::Attributes::DeferredParse));
        }

        // Set other state back to before parse as well
        this->SetStackNestedFunc(false);
        this->SetAuxPtr(AuxPointerType::StackNestedFuncParent, nullptr);
        this->SetReparsed(true);
#if DBG
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, _u("Regenerate Due To Debug Mode: function %s (%s) from script context %p\n"),
            this->GetDisplayName(), this->GetDebugNumberSet(debugStringBuffer), m_scriptContext);

        this->counters.bgThreadCallStarted = false; // asuming background jit is stopped and allow the counter setters access again
#endif
    }

    void FunctionBody::ClearEntryPoints()
    {TRACE_IT(34419);
        if (this->entryPoints)
        {TRACE_IT(34420);
            this->MapEntryPoints([] (int index, FunctionEntryPointInfo* entryPoint)
            {
                if (nullptr != entryPoint)
                {TRACE_IT(34421);
                    // Finalize = Free up work item if it hasn't been released yet + entry point clean up
                    // isShutdown is false because cleanup is called only in the !isShutdown case
                    entryPoint->Finalize(/*isShutdown*/ false);
                }
            });

            this->MapLoopHeaders([] (uint loopNumber, LoopHeader* header)
            {
                header->MapEntryPoints([] (int index, LoopEntryPointInfo* entryPoint)
                {
                    entryPoint->Cleanup(/*isShutdown*/ false, true /* capture cleanup stack */);
                });
            });
        }

        this->entryPoints->ClearAndZero();
    }

    //
    // For library code all references to jitted entry points need to be removed
    //
    void FunctionBody::ResetEntryPoint()
    {TRACE_IT(34422);
        ClearEntryPoints();
        this->CreateNewDefaultEntryPoint();
        this->SetOriginalEntryPoint(DefaultEntryThunk);
        m_defaultEntryPointInfo->jsMethod = m_scriptContext->CurrentThunk;

        if (this->deferredPrototypeType)
        {TRACE_IT(34423);
            // Update old entry points on the deferred prototype type,
            // as they may point to old native code gen regions which age gone now.
            this->deferredPrototypeType->SetEntryPoint(this->GetDefaultEntryPointInfo()->jsMethod);
            this->deferredPrototypeType->SetEntryPointInfo(this->GetDefaultEntryPointInfo());
        }
        ReinitializeExecutionModeAndLimits();
    }

    void FunctionBody::AddDeferParseAttribute()
    {TRACE_IT(34424);
        this->SetAttributes((FunctionInfo::Attributes) (this->GetAttributes() | FunctionInfo::Attributes::DeferredParse));
    }

    void FunctionBody::RemoveDeferParseAttribute()
    {TRACE_IT(34425);
        this->SetAttributes((FunctionInfo::Attributes) (this->GetAttributes() & (~FunctionInfo::Attributes::DeferredParse)));
    }

    Js::DebuggerScope * FunctionBody::GetDiagCatchScopeObjectAt(int byteCodeOffset)
    {TRACE_IT(34426);
        if (GetScopeObjectChain())
        {TRACE_IT(34427);
            for (int i = 0; i < GetScopeObjectChain()->pScopeChain->Count(); i++)
            {TRACE_IT(34428);
                Js::DebuggerScope *debuggerScope = GetScopeObjectChain()->pScopeChain->Item(i);
                Assert(debuggerScope);

                if (debuggerScope->IsCatchScope() && debuggerScope->IsOffsetInScope(byteCodeOffset))
                {TRACE_IT(34429);
                    return debuggerScope;
                }
            }
        }
        return nullptr;
    }


    ushort SmallSpanSequence::GetDiff(int current, int prev)
    {TRACE_IT(34430);
        int diff = current - prev;

        if ((diff) < SHRT_MIN  || (diff) >= SHRT_MAX)
        {TRACE_IT(34431);
            diff = SHRT_MAX;

            if (!this->pActualOffsetList)
            {TRACE_IT(34432);
                this->pActualOffsetList = JsUtil::GrowingUint32HeapArray::Create(4);
            }

            this->pActualOffsetList->Add(current);
        }

        return (ushort)diff;
    }

    // Get Values of the beginning of the statement at particular index.
    BOOL SmallSpanSequence::GetRangeAt(int index, SmallSpanSequenceIter &iter, int * pCountOfMissed, StatementData & data)
    {TRACE_IT(34433);
        Assert((uint32)index < pStatementBuffer->Count());

        SmallSpan span(pStatementBuffer->ItemInBuffer((uint32)index));

        int countOfMissed = 0;

        if ((short)span.sourceBegin == SHRT_MAX)
        {TRACE_IT(34434);
            // Look in ActualOffset store
            Assert(this->pActualOffsetList);
            Assert(this->pActualOffsetList->Count() > 0);
            Assert(this->pActualOffsetList->Count() > (uint32)iter.indexOfActualOffset);

            data.sourceBegin = this->pActualOffsetList->ItemInBuffer((uint32)iter.indexOfActualOffset);
            countOfMissed++;
        }
        else
        {TRACE_IT(34435);
            data.sourceBegin = iter.accumulatedSourceBegin + (short)span.sourceBegin;
        }

        if (span.bytecodeBegin == SHRT_MAX)
        {TRACE_IT(34436);
            // Look in ActualOffset store
            Assert(this->pActualOffsetList);
            Assert(this->pActualOffsetList->Count() > 0);
            Assert(this->pActualOffsetList->Count() > (uint32)(iter.indexOfActualOffset + countOfMissed));

            data.bytecodeBegin = this->pActualOffsetList->ItemInBuffer((uint32)iter.indexOfActualOffset + countOfMissed);
            countOfMissed++;
        }
        else
        {TRACE_IT(34437);
            data.bytecodeBegin = iter.accumulatedBytecodeBegin + span.bytecodeBegin;
        }

        if (pCountOfMissed)
        {TRACE_IT(34438);
            *pCountOfMissed = countOfMissed;
        }

        return TRUE;
    }

    void SmallSpanSequence::Reset(SmallSpanSequenceIter &iter)
    {TRACE_IT(34439);
        iter.accumulatedIndex = 0;
        iter.accumulatedSourceBegin = baseValue;
        iter.accumulatedBytecodeBegin = 0;
        iter.indexOfActualOffset = 0;
    }

    BOOL SmallSpanSequence::GetMatchingStatementFromBytecode(int bytecode, SmallSpanSequenceIter &iter, StatementData & data)
    {TRACE_IT(34440);
        if (Count() > 0 && bytecode >= 0)
        {TRACE_IT(34441);
            // Support only in forward direction
            if (bytecode < iter.accumulatedBytecodeBegin
                || iter.accumulatedIndex <= 0 || (uint32)iter.accumulatedIndex >= Count())
            {TRACE_IT(34442);
                // re-initialize the accumulators
                Reset(iter);
            }

            while ((uint32)iter.accumulatedIndex < Count())
            {TRACE_IT(34443);
                int countOfMissed = 0;
                if (!GetRangeAt(iter.accumulatedIndex, iter, &countOfMissed, data))
                {TRACE_IT(34444);
                    Assert(FALSE);
                    break;
                }

                if (data.bytecodeBegin >= bytecode)
                {TRACE_IT(34445);
                    if (data.bytecodeBegin > bytecode)
                    {TRACE_IT(34446);
                        // Not exactly at the current bytecode, so it falls in between previous statement.
                        data.sourceBegin = iter.accumulatedSourceBegin;
                        data.bytecodeBegin = iter.accumulatedBytecodeBegin;
                    }

                    return TRUE;
                }

                // Look for the next
                iter.accumulatedSourceBegin = data.sourceBegin;
                iter.accumulatedBytecodeBegin = data.bytecodeBegin;
                iter.accumulatedIndex++;

                if (countOfMissed)
                {TRACE_IT(34447);
                    iter.indexOfActualOffset += countOfMissed;
                }
            }

            if (iter.accumulatedIndex != -1)
            {TRACE_IT(34448);
                // Give the last one.
                Assert(data.bytecodeBegin < bytecode);
                return TRUE;
            }
        }

        // Failed to give the correct one, init to default
        iter.accumulatedIndex = -1;
        return FALSE;
    }

    BOOL SmallSpanSequence::Item(int index, SmallSpanSequenceIter &iter, StatementData & data)
    {TRACE_IT(34449);
        if (!pStatementBuffer || (uint32)index >= pStatementBuffer->Count())
        {TRACE_IT(34450);
            return FALSE;
        }

        if (iter.accumulatedIndex <= 0 || iter.accumulatedIndex > index)
        {TRACE_IT(34451);
            Reset(iter);
        }

        while (iter.accumulatedIndex <= index)
        {TRACE_IT(34452);
            Assert((uint32)iter.accumulatedIndex < pStatementBuffer->Count());

            int countOfMissed = 0;
            if (!GetRangeAt(iter.accumulatedIndex, iter, &countOfMissed, data))
            {TRACE_IT(34453);
                Assert(FALSE);
                break;
            }

            // We store the next index
            iter.accumulatedSourceBegin = data.sourceBegin;
            iter.accumulatedBytecodeBegin = data.bytecodeBegin;

            iter.accumulatedIndex++;

            if (countOfMissed)
            {TRACE_IT(34454);
                iter.indexOfActualOffset += countOfMissed;
            }

            if ((iter.accumulatedIndex - 1) == index)
            {TRACE_IT(34455);
                return TRUE;
            }
        }

        return FALSE;
    }

    BOOL SmallSpanSequence::Seek(int index, StatementData & data)
    {TRACE_IT(34456);
        // This method will not alter any state of the variables, so this will just do plain search
        // from the beginning to look for that index.

        SmallSpanSequenceIter iter;
        Reset(iter);

        return Item(index, iter, data);
    }

    PropertyIdOnRegSlotsContainer * PropertyIdOnRegSlotsContainer::New(Recycler * recycler)
    {TRACE_IT(34457);
        return RecyclerNew(recycler, PropertyIdOnRegSlotsContainer);
    }

    PropertyIdOnRegSlotsContainer::PropertyIdOnRegSlotsContainer()
        :  propertyIdsForRegSlots(nullptr), length(0), propertyIdsForFormalArgs(nullptr), formalsUpperBound(Js::Constants::NoRegister)
    {TRACE_IT(34458);
    }

    void PropertyIdOnRegSlotsContainer::CreateRegSlotsArray(Recycler * recycler, uint _length)
    {TRACE_IT(34459);
        Assert(propertyIdsForRegSlots == nullptr);
        propertyIdsForRegSlots = RecyclerNewArrayLeafZ(recycler, PropertyId, _length);
        length = _length;
    }

    void PropertyIdOnRegSlotsContainer::SetFormalArgs(PropertyIdArray * formalArgs)
    {TRACE_IT(34460);
        propertyIdsForFormalArgs = formalArgs;
    }

    //
    // Helper methods for PropertyIdOnRegSlotsContainer

    void PropertyIdOnRegSlotsContainer::Insert(RegSlot reg, PropertyId propId)
    {TRACE_IT(34461);
        //
        // Reg is being used as an index;

        Assert(propertyIdsForRegSlots);
        Assert(reg < length);

        //
        // the current reg is unaccounted for const reg count. while fetching calculate the actual regslot value.

        Assert(propertyIdsForRegSlots[reg] == 0 || propertyIdsForRegSlots[reg] == propId);
        propertyIdsForRegSlots[reg] = propId;
    }

    void PropertyIdOnRegSlotsContainer::FetchItemAt(uint index, FunctionBody *pFuncBody, __out PropertyId *pPropId, __out RegSlot *pRegSlot)
    {TRACE_IT(34462);
        Assert(index < length);
        Assert(pPropId);
        Assert(pRegSlot);
        Assert(pFuncBody);

        *pPropId = propertyIdsForRegSlots[index];
        *pRegSlot = pFuncBody->MapRegSlot(index);
    }

    bool PropertyIdOnRegSlotsContainer::IsRegSlotFormal(RegSlot reg)
    {TRACE_IT(34463);
        if (propertyIdsForFormalArgs != nullptr && reg < length)
        {TRACE_IT(34464);
            PropertyId propId = propertyIdsForRegSlots[reg];
            for (uint32 i = 0; i < propertyIdsForFormalArgs->count; i++)
            {TRACE_IT(34465);
                if (propertyIdsForFormalArgs->elements[i] == propId)
                {TRACE_IT(34466);
                    return true;
                }
            }
        }

        return false;
    }

    ScopeType FrameDisplay::GetScopeType(void* scope)
    {TRACE_IT(34467);
        if(Js::ActivationObject::Is(scope))
        {TRACE_IT(34468);
            return ScopeType_ActivationObject;
        }
        if(Js::ScopeSlots::Is(scope))
        {TRACE_IT(34469);
            return ScopeType_SlotArray;
        }
        return ScopeType_WithScope;
    }

    // DebuggerScope

    // Get the sibling for the current debugger scope.
    DebuggerScope * DebuggerScope::GetSiblingScope(RegSlot location, FunctionBody *functionBody)
    {TRACE_IT(34470);
        bool isBlockSlotOrObject = scopeType == Js::DiagExtraScopesType::DiagBlockScopeInSlot || scopeType == Js::DiagExtraScopesType::DiagBlockScopeInObject;
        bool isCatchSlotOrObject = scopeType == Js::DiagExtraScopesType::DiagCatchScopeInSlot || scopeType == Js::DiagExtraScopesType::DiagCatchScopeInObject;

        // This is expected to be called only when the current scope is either slot or activation object.
        Assert(isBlockSlotOrObject || isCatchSlotOrObject);

        if (siblingScope == nullptr)
        {TRACE_IT(34471);
            // If the sibling isn't there, attempt to retrieve it if we're reparsing or create it anew if this is the first parse.
            siblingScope = functionBody->RecordStartScopeObject(isBlockSlotOrObject ? Js::DiagExtraScopesType::DiagBlockScopeDirect : Js::DiagExtraScopesType::DiagCatchScopeDirect, GetStart(), location);
        }

        return siblingScope;
    }

    // Adds a new property to be tracked in the debugger scope.
    // location     - The slot array index or register slot location of where the property is stored.
    // propertyId   - The property ID of the property.
    // flags        - Flags that help describe the property.
    void DebuggerScope::AddProperty(RegSlot location, Js::PropertyId propertyId, DebuggerScopePropertyFlags flags)
    {TRACE_IT(34472);
        DebuggerScopeProperty scopeProperty;

        scopeProperty.location = location;
        scopeProperty.propId = propertyId;

        // This offset is uninitialized until the property is initialized (with a ld opcode, for example).
        scopeProperty.byteCodeInitializationOffset = Constants::InvalidByteCodeOffset;
        scopeProperty.flags = flags;

        // Delay allocate the property list so we don't take up memory if there are no properties in this scope.
        // Scopes are created during non-debug mode as well so we want to keep them as small as possible.
        this->EnsurePropertyListIsAllocated();

        // The property doesn't exist yet, so add it.
        this->scopeProperties->Add(scopeProperty);
    }

    bool DebuggerScope::HasProperty(Js::PropertyId propertyId)
    {TRACE_IT(34473);
        int i = -1;
        return GetPropertyIndex(propertyId, i);
    }

    bool DebuggerScope::GetPropertyIndex(Js::PropertyId propertyId, int& index)
    {TRACE_IT(34474);
        if (!this->HasProperties())
        {TRACE_IT(34475);
            index = -1;
            return false;
        }

        bool found = this->scopeProperties->MapUntil( [&](int i, const DebuggerScopeProperty& scopeProperty) {
            if(scopeProperty.propId == propertyId)
            {TRACE_IT(34476);
                index = scopeProperty.location;
                return true;
            }
            return false;
        });

        if(!found)
        {TRACE_IT(34477);
            return false;
        }
        return true;
    }
#if DBG
    void DebuggerScope::Dump()
    {TRACE_IT(34478);
        int indent = (GetScopeDepth() - 1) * 4;

        Output::Print(indent, _u("Begin scope: Address: %p Type: %s Location: %d Sibling: %p Range: [%d, %d]\n "), this, GetDebuggerScopeTypeString(scopeType), scopeLocation, PointerValue(this->siblingScope), range.begin, range.end);
        if (this->HasProperties())
        {TRACE_IT(34479);
            this->scopeProperties->Map( [=] (int i, Js::DebuggerScopeProperty& scopeProperty) {
                Output::Print(indent, _u("%s(%d) Location: %d Const: %s Initialized: %d\n"), ThreadContext::GetContextForCurrentThread()->GetPropertyName(scopeProperty.propId)->GetBuffer(),
                    scopeProperty.propId, scopeProperty.location, scopeProperty.IsConst() ? _u("true"): _u("false"), scopeProperty.byteCodeInitializationOffset);
            });
        }

        Output::Print(_u("\n"));
    }

    // Returns the debugger scope type in string format.
    PCWSTR DebuggerScope::GetDebuggerScopeTypeString(DiagExtraScopesType scopeType)
    {TRACE_IT(34480);
        switch (scopeType)
        {
        case DiagExtraScopesType::DiagBlockScopeDirect:
            return _u("DiagBlockScopeDirect");
        case DiagExtraScopesType::DiagBlockScopeInObject:
            return _u("DiagBlockScopeInObject");
        case DiagExtraScopesType::DiagBlockScopeInSlot:
            return _u("DiagBlockScopeInSlot");
        case DiagExtraScopesType::DiagBlockScopeRangeEnd:
            return _u("DiagBlockScopeRangeEnd");
        case DiagExtraScopesType::DiagCatchScopeDirect:
            return _u("DiagCatchScopeDirect");
        case DiagExtraScopesType::DiagCatchScopeInObject:
            return _u("DiagCatchScopeInObject");
        case DiagExtraScopesType::DiagCatchScopeInSlot:
            return _u("DiagCatchScopeInSlot");
        case DiagExtraScopesType::DiagUnknownScope:
            return _u("DiagUnknownScope");
        case DiagExtraScopesType::DiagWithScope:
            return _u("DiagWithScope");
        case DiagExtraScopesType::DiagParamScope:
            return _u("DiagParamScope");
        case DiagExtraScopesType::DiagParamScopeInObject:
            return _u("DiagParamScopeInObject");
        default:
            AssertMsg(false, "Missing a debug scope type.");
            return _u("");
        }
    }
#endif

#if ENABLE_TTD
    Js::PropertyId DebuggerScope::GetPropertyIdForSlotIndex_TTD(uint32 slotIndex) const
    {TRACE_IT(34481);
        const Js::DebuggerScopeProperty& scopeProperty = this->scopeProperties->Item(slotIndex);
        return scopeProperty.propId;
    }
#endif

    // Updates the current offset of where the property is first initialized.  This is used to
    // detect whether or not a property is in a dead zone when broken in the debugger.
    // location                 - The slot array index or register slot location of where the property is stored.
    // propertyId               - The property ID of the property.
    // byteCodeOffset           - The offset to set the initialization point at.
    // isFunctionDeclaration    - Whether or not the property is a function declaration or not.  Used for verification.
    // <returns>        - True if the property was found and updated for the current scope, else false.
    bool DebuggerScope::UpdatePropertyInitializationOffset(
        RegSlot location,
        Js::PropertyId propertyId,
        int byteCodeOffset,
        bool isFunctionDeclaration /*= false*/)
    {
        if (UpdatePropertyInitializationOffsetInternal(location, propertyId, byteCodeOffset, isFunctionDeclaration))
        {TRACE_IT(34482);
            return true;
        }
        if (siblingScope != nullptr && siblingScope->UpdatePropertyInitializationOffsetInternal(location, propertyId, byteCodeOffset, isFunctionDeclaration))
        {TRACE_IT(34483);
            return true;
        }
        return false;
    }

    bool DebuggerScope::UpdatePropertyInitializationOffsetInternal(
        RegSlot location,
        Js::PropertyId propertyId,
        int byteCodeOffset,
        bool isFunctionDeclaration /*= false*/)
    {TRACE_IT(34484);
        if (scopeProperties == nullptr)
        {TRACE_IT(34485);
            return false;
        }

        for (int i = 0; i < scopeProperties->Count(); ++i)
        {TRACE_IT(34486);
            DebuggerScopeProperty propertyItem = scopeProperties->Item(i);
            if (propertyItem.propId == propertyId && propertyItem.location == location)
            {TRACE_IT(34487);
                if (propertyItem.byteCodeInitializationOffset == Constants::InvalidByteCodeOffset)
                {TRACE_IT(34488);
                    propertyItem.byteCodeInitializationOffset = byteCodeOffset;
                    scopeProperties->SetExistingItem(i, propertyItem);
                }
#if DBG
                else
                {
                    // If the bytecode initialization offset is not Constants::InvalidByteCodeOffset,
                    // it means we have two or more functions declared in the same scope with the same name
                    // and one has already been marked.  We track each location with a property entry
                    // on the debugging side (when calling DebuggerScope::AddProperty()) as opposed to scanning
                    // and checking if the property already exists each time we add in order to avoid duplicates.
                    AssertMsg(isFunctionDeclaration, "Only function declarations can be defined more than once in the same scope with the same name.");
                    AssertMsg(propertyItem.byteCodeInitializationOffset == byteCodeOffset, "The bytecode offset for all function declarations should be identical for this scope.");
                }
#endif // DBG

                return true;
            }
        }

        return false;
    }

    // Updates the debugger scopes fields due to a regeneration of bytecode (happens during debugger attach or detach, for
    // example).
    void DebuggerScope::UpdateDueToByteCodeRegeneration(DiagExtraScopesType scopeType, int start, RegSlot scopeLocation)
    {TRACE_IT(34489);
#if DBG
        if (this->scopeType != Js::DiagUnknownScope)
        {TRACE_IT(34490);
            // If the scope is unknown, it was deserialized without a scope type.  Otherwise, it should not have changed.
            // The scope type can change on a re-parse in certain scenarios related to eval detection in legacy mode -> Winblue: 272122
            AssertMsg(this->scopeType == scopeType, "The debugger scope type should not have changed when generating bytecode again.");
        }
#endif // DBG

        this->scopeType = scopeType;
        this->SetBegin(start);
        if(this->scopeProperties)
        {TRACE_IT(34491);
            this->scopeProperties->Clear();
        }

        // Reset the scope location as it may have changed during bytecode generation from the last run.
        this->SetScopeLocation(scopeLocation);

        if (siblingScope)
        {TRACE_IT(34492);
            // If we had a sibling scope during initial parsing, clear it now so that it will be reset
            // when it is retrieved during this bytecode generation pass, in GetSiblingScope().
            // GetSiblingScope() will ensure that the FunctionBody currentDebuggerScopeIndex value is
            // updated accordingly to account for future scopes coming after the sibling.
            // Calling of GetSiblingScope() will happen when register properties are added to this scope
            // via TrackRegisterPropertyForDebugger().
            siblingScope = nullptr;
        }
    }

    void DebuggerScope::UpdatePropertiesInForInOrOfCollectionScope()
    {TRACE_IT(34493);
        if (this->scopeProperties != nullptr)
        {TRACE_IT(34494);
            this->scopeProperties->All([&](Js::DebuggerScopeProperty& propertyItem)
            {
                propertyItem.flags |= DebuggerScopePropertyFlags_ForInOrOfCollection;
                return true;
            });
        }
    }

    void DebuggerScope::EnsurePropertyListIsAllocated()
    {TRACE_IT(34495);
        if (this->scopeProperties == nullptr)
        {TRACE_IT(34496);
            this->scopeProperties = RecyclerNew(this->recycler, DebuggerScopePropertyList, this->recycler);
        }
    }

    // Checks if the passed in ByteCodeGenerator offset is in this scope's being/end range.
    bool DebuggerScope::IsOffsetInScope(int offset) const
    {TRACE_IT(34497);
        Assert(this->range.end != -1);
        return this->range.Includes(offset);
    }

    // Determines if the DebuggerScope contains a property with the passed in ID and
    // location in the internal property list.
    // propertyId       - The ID of the property to search for.
    // location         - The slot array index or register to search for.
    // outScopeProperty - Optional parameter that will return the property, if found.
    bool DebuggerScope::Contains(Js::PropertyId propertyId, RegSlot location) const
    {TRACE_IT(34498);
        DebuggerScopeProperty tempProperty;
        return TryGetProperty(propertyId, location, &tempProperty);
    }

    // Gets whether or not the scope is a block scope (non-catch or with).
    bool DebuggerScope::IsBlockScope() const
    {TRACE_IT(34499);
        AssertMsg(this->scopeType != Js::DiagBlockScopeRangeEnd, "Debugger scope type should never be set to range end - only reserved for marking the end of a scope (not persisted).");
        return this->scopeType == Js::DiagBlockScopeDirect
            || this->scopeType == Js::DiagBlockScopeInObject
            || this->scopeType == Js::DiagBlockScopeInSlot
            || this->scopeType == Js::DiagBlockScopeRangeEnd;
    }

    // Gets whether or not the scope is a catch block scope.
    bool DebuggerScope::IsCatchScope() const
    {TRACE_IT(34500);
        return this->scopeType == Js::DiagCatchScopeDirect
            || this->scopeType == Js::DiagCatchScopeInObject
            || this->scopeType == Js::DiagCatchScopeInSlot;
    }

    // Gets whether or not the scope is a with block scope.
    bool DebuggerScope::IsWithScope() const
    {TRACE_IT(34501);
        return this->scopeType == Js::DiagWithScope;
    }

    // Gets whether or not the scope is a slot array scope.
    bool DebuggerScope::IsSlotScope() const
    {TRACE_IT(34502);
        return this->scopeType == Js::DiagBlockScopeInSlot
            || this->scopeType == Js::DiagCatchScopeInSlot;
    }

    bool DebuggerScope::IsParamScope() const
    {TRACE_IT(34503);
        return this->scopeType == Js::DiagParamScope
            || this->scopeType == Js::DiagParamScopeInObject;
    }

    // Gets whether or not the scope has any properties in it.
    bool DebuggerScope::HasProperties() const
    {TRACE_IT(34504);
        return this->scopeProperties && this->scopeProperties->Count() > 0;
    }

    // Checks if this scope is an ancestor of the passed in scope.
    bool DebuggerScope::IsAncestorOf(const DebuggerScope* potentialChildScope)
    {TRACE_IT(34505);
        if (potentialChildScope == nullptr)
        {TRACE_IT(34506);
            // If the child scope is null, it represents the global scope which
            // cannot be a child of anything.
            return false;
        }

        const DebuggerScope* currentScope = potentialChildScope;
        while (currentScope)
        {TRACE_IT(34507);
            if (currentScope->GetParentScope() == this)
            {TRACE_IT(34508);
                return true;
            }

            currentScope = currentScope->GetParentScope();
        }

        return false;
    }

    // Checks if all properties of the scope are currently in a dead zone given the specified offset.
    bool DebuggerScope::AreAllPropertiesInDeadZone(int byteCodeOffset) const
    {TRACE_IT(34509);
        if (!this->HasProperties())
        {TRACE_IT(34510);
            return false;
        }

        return this->scopeProperties->All([&](Js::DebuggerScopeProperty& propertyItem)
            {
                return propertyItem.IsInDeadZone(byteCodeOffset);
            });
    }

    // Attempts to get the specified property.  Returns true if the property was copied to the structure; false otherwise.
    bool DebuggerScope::TryGetProperty(Js::PropertyId propertyId, RegSlot location, DebuggerScopeProperty* outScopeProperty) const
    {TRACE_IT(34511);
        Assert(outScopeProperty);

        if (scopeProperties == nullptr)
        {TRACE_IT(34512);
            return false;
        }

        for (int i = 0; i < scopeProperties->Count(); ++i)
        {TRACE_IT(34513);
            DebuggerScopeProperty propertyItem = scopeProperties->Item(i);
            if (propertyItem.propId == propertyId && propertyItem.location == location)
            {TRACE_IT(34514);
                *outScopeProperty = propertyItem;
                return true;
            }
        }

        return false;
    }

    bool DebuggerScope::TryGetValidProperty(Js::PropertyId propertyId, RegSlot location, int offset, DebuggerScopeProperty* outScopeProperty, bool* isInDeadZone) const
    {
        if (TryGetProperty(propertyId, location, outScopeProperty))
        {TRACE_IT(34515);
            if (IsOffsetInScope(offset))
            {TRACE_IT(34516);
                if (isInDeadZone != nullptr)
                {TRACE_IT(34517);
                    *isInDeadZone = outScopeProperty->IsInDeadZone(offset);
                }

                return true;
            }
        }

        return false;
    }

    void DebuggerScope::SetBegin(int begin)
    {TRACE_IT(34518);
        range.begin = begin;
        if (siblingScope != nullptr)
        {TRACE_IT(34519);
            siblingScope->SetBegin(begin);
        }
    }

    void DebuggerScope::SetEnd(int end)
    {TRACE_IT(34520);
        range.end = end;
        if (siblingScope != nullptr)
        {TRACE_IT(34521);
            siblingScope->SetEnd(end);
        }
    }

    // Finds the common ancestor scope between this scope and the passed in scope.
    // Returns nullptr if the scopes are part of different trees.
    DebuggerScope* DebuggerScope::FindCommonAncestor(DebuggerScope* debuggerScope)
    {TRACE_IT(34522);
        AnalysisAssert(debuggerScope);

        if (this == debuggerScope)
        {TRACE_IT(34523);
            return debuggerScope;
        }

        if (this->IsAncestorOf(debuggerScope))
        {TRACE_IT(34524);
            return this;
        }

        if (debuggerScope->IsAncestorOf(this))
        {TRACE_IT(34525);
            return debuggerScope;
        }

        DebuggerScope* firstNode = this;
        DebuggerScope* secondNode = debuggerScope;

        int firstDepth = firstNode->GetScopeDepth();
        int secondDepth = secondNode->GetScopeDepth();

        // Calculate the depth difference in order to bring the deep node up to the sibling
        // level of the shorter node.
        int depthDifference = abs(firstDepth - secondDepth);

        DebuggerScope*& nodeToBringUp = firstDepth > secondDepth ? firstNode : secondNode;
        while (depthDifference > 0)
        {TRACE_IT(34526);
            AnalysisAssert(nodeToBringUp);
            nodeToBringUp = nodeToBringUp->GetParentScope();
            --depthDifference;
        }

        // Move up the tree and see where the nodes meet.
        while (firstNode && secondNode)
        {TRACE_IT(34527);
            if (firstNode == secondNode)
            {TRACE_IT(34528);
                return firstNode;
            }

            firstNode = firstNode->GetParentScope();
            secondNode = secondNode->GetParentScope();
        }

        // The nodes are not part of the same scope tree.
        return nullptr;
    }

    // Gets the depth of the scope in the parent link tree.
    int DebuggerScope::GetScopeDepth() const
    {TRACE_IT(34529);
        int depth = 0;
        const DebuggerScope* currentDebuggerScope = this;
        while (currentDebuggerScope)
        {TRACE_IT(34530);
            currentDebuggerScope = currentDebuggerScope->GetParentScope();
            ++depth;
        }

        return depth;
    }

    bool ScopeObjectChain::TryGetDebuggerScopePropertyInfo(PropertyId propertyId, RegSlot location, int offset, bool* isPropertyInDebuggerScope, bool *isConst, bool* isInDeadZone)
    {TRACE_IT(34531);
        Assert(pScopeChain);
        Assert(isPropertyInDebuggerScope);
        Assert(isConst);

        *isPropertyInDebuggerScope = false;
        *isConst = false;

        // Search through each block scope until we find the current scope.  If the register was found
        // in any of the scopes going down until we reach the scope of the debug break, then it's in scope.
        // if found but not in the scope, the out param will be updated (since it is actually a let or const), so that caller can make a call accordingly.
        for (int i = 0; i < pScopeChain->Count(); i++)
        {TRACE_IT(34532);
            Js::DebuggerScope *debuggerScope = pScopeChain->Item(i);
            DebuggerScopeProperty debuggerScopeProperty;
            if (!debuggerScope->IsParamScope() && debuggerScope->TryGetProperty(propertyId, location, &debuggerScopeProperty))
            {TRACE_IT(34533);
                bool isOffsetInScope = debuggerScope->IsOffsetInScope(offset);

                // For the Object scope, all the properties will have the same location (-1) so they can match. Use further check below to determine the propertyInDebuggerScope
                *isPropertyInDebuggerScope = isOffsetInScope || !debuggerScope->IsBlockObjectScope();

                if (isOffsetInScope)
                {TRACE_IT(34534);
                    if (isInDeadZone != nullptr)
                    {TRACE_IT(34535);
                        *isInDeadZone = debuggerScopeProperty.IsInDeadZone(offset);
                    }

                    *isConst = debuggerScopeProperty.IsConst();
                    return true;
                }
            }
        }

        return false;
    }

    void FunctionBody::AllocateForInCache()
    {TRACE_IT(34536);
        uint profiledForInLoopCount = this->GetProfiledForInLoopCount();
        if (profiledForInLoopCount == 0)
        {TRACE_IT(34537);
            return;
        }
        this->SetAuxPtr(AuxPointerType::ForInCacheArray, AllocatorNewArrayZ(CacheAllocator, this->GetScriptContext()->ForInCacheAllocator(), ForInCache, profiledForInLoopCount));
    }

    ForInCache * FunctionBody::GetForInCache(uint index)
    {TRACE_IT(34538);
        Assert(index < this->GetProfiledForInLoopCount());
        return &((ForInCache *)this->GetAuxPtr(AuxPointerType::ForInCacheArray))[index];
    }

    ForInCache * FunctionBody::GetForInCacheArray()
    {TRACE_IT(34539);
        return ((ForInCache *)this->GetAuxPtrWithLock(AuxPointerType::ForInCacheArray));
    }

    void FunctionBody::CleanUpForInCache(bool isShutdown)
    {TRACE_IT(34540);
        uint profiledForInLoopCount = this->GetProfiledForInLoopCount();
        if (profiledForInLoopCount == 0)
        {TRACE_IT(34541);
            return;
        }
        ForInCache * forInCacheArray = (ForInCache *)this->GetAuxPtr(AuxPointerType::ForInCacheArray);
        if (forInCacheArray)
        {TRACE_IT(34542);
            if (isShutdown)
            {
                memset(forInCacheArray, 0, sizeof(ForInCache) * profiledForInLoopCount);
            }
            else
            {
                AllocatorDeleteArray(CacheAllocator, this->GetScriptContext()->ForInCacheAllocator(), profiledForInLoopCount, forInCacheArray);
                this->SetAuxPtr(AuxPointerType::ForInCacheArray, nullptr);
            }
        }
    }

    void FunctionBody::AllocateInlineCache()
    {TRACE_IT(34543);
        Assert(this->inlineCaches == nullptr);
        uint isInstInlineCacheStart = this->GetInlineCacheCount();
        uint totalCacheCount = isInstInlineCacheStart + GetIsInstInlineCacheCount();

        if (totalCacheCount != 0)
        {TRACE_IT(34544);
            // Root object inline cache are not leaf
            void ** inlineCaches = RecyclerNewArrayZ(this->m_scriptContext->GetRecycler(),
                void*, totalCacheCount);
#if DBG
            this->m_inlineCacheTypes = RecyclerNewArrayLeafZ(this->m_scriptContext->GetRecycler(),
                byte, totalCacheCount);
#endif
            uint i = 0;
            uint plainInlineCacheEnd = GetRootObjectLoadInlineCacheStart();
            __analysis_assume(plainInlineCacheEnd <= totalCacheCount);
            for (; i < plainInlineCacheEnd; i++)
            {TRACE_IT(34545);
                inlineCaches[i] = AllocatorNewZ(InlineCacheAllocator,
                    this->m_scriptContext->GetInlineCacheAllocator(), InlineCache);
            }
            Js::RootObjectBase * rootObject = this->GetRootObject();
            ThreadContext * threadContext = this->GetScriptContext()->GetThreadContext();
            uint rootObjectLoadInlineCacheEnd = GetRootObjectLoadMethodInlineCacheStart();
            __analysis_assume(rootObjectLoadInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectLoadInlineCacheEnd; i++)
            {TRACE_IT(34546);
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(this->GetPropertyIdFromCacheId(i)), false, false);
            }
            uint rootObjectLoadMethodInlineCacheEnd = GetRootObjectStoreInlineCacheStart();
            __analysis_assume(rootObjectLoadMethodInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectLoadMethodInlineCacheEnd; i++)
            {TRACE_IT(34547);
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(this->GetPropertyIdFromCacheId(i)), true, false);
            }
            uint rootObjectStoreInlineCacheEnd = isInstInlineCacheStart;
            __analysis_assume(rootObjectStoreInlineCacheEnd <= totalCacheCount);
            for (; i < rootObjectStoreInlineCacheEnd; i++)
            {TRACE_IT(34548);
#pragma prefast(suppress:6386, "The analysis assume didn't help prefast figure out this is in range")
                inlineCaches[i] = rootObject->GetInlineCache(
                    threadContext->GetPropertyName(this->GetPropertyIdFromCacheId(i)), false, true);
            }
            for (; i < totalCacheCount; i++)
            {TRACE_IT(34549);
                inlineCaches[i] = AllocatorNewStructZ(CacheAllocator,
                    this->m_scriptContext->GetIsInstInlineCacheAllocator(), IsInstInlineCache);
            }
#if DBG
            this->m_inlineCacheTypes = RecyclerNewArrayLeafZ(this->m_scriptContext->GetRecycler(),
                byte, totalCacheCount);
#endif
            this->inlineCaches = inlineCaches;
        }
    }

    InlineCache *FunctionBody::GetInlineCache(uint index)
    {TRACE_IT(34550);
        Assert(this->inlineCaches != nullptr);
        Assert(index < this->GetInlineCacheCount());
#if DBG
        Assert(this->m_inlineCacheTypes[index] == InlineCacheTypeNone ||
            this->m_inlineCacheTypes[index] == InlineCacheTypeInlineCache);
        this->m_inlineCacheTypes[index] = InlineCacheTypeInlineCache;
#endif
        return reinterpret_cast<InlineCache *>(this->inlineCaches[index]);
    }

    bool FunctionBody::CanFunctionObjectHaveInlineCaches()
    {TRACE_IT(34551);
        if (this->DoStackNestedFunc() || this->IsCoroutine())
        {TRACE_IT(34552);
            return false;
        }

        uint totalCacheCount = this->GetInlineCacheCount() + this->GetIsInstInlineCacheCount();
        if (PHASE_FORCE(Js::ScriptFunctionWithInlineCachePhase, this) && totalCacheCount > 0)
        {TRACE_IT(34553);
            return true;
        }

        // Only have inline caches on function object for possible inlining candidates.
        // Since we don't know the size of the top function, check against the maximum possible inline threshold
        // Negative inline byte code size threshold will disable inline cache on function object.
        const int byteCodeSizeThreshold = CONFIG_FLAG(InlineThreshold) + CONFIG_FLAG(InlineThresholdAdjustCountInSmallFunction);
        if (byteCodeSizeThreshold < 0 || this->GetByteCodeWithoutLDACount() > (uint)byteCodeSizeThreshold)
        {TRACE_IT(34554);
            return false;
        }
        // Negative FuncObjectInlineCacheThreshold will disable inline cache on function object.
        if (CONFIG_FLAG(FuncObjectInlineCacheThreshold) < 0 || totalCacheCount > (uint)CONFIG_FLAG(FuncObjectInlineCacheThreshold) || totalCacheCount == 0)
        {TRACE_IT(34555);
            return false;
        }

        return true;
    }

    void** FunctionBody::GetInlineCaches()
    {TRACE_IT(34556);
        return this->inlineCaches;
    }

#if DBG
    byte* FunctionBody::GetInlineCacheTypes()
    {TRACE_IT(34557);
        return this->m_inlineCacheTypes;
    }
#endif

    IsInstInlineCache *FunctionBody::GetIsInstInlineCache(uint index)
    {TRACE_IT(34558);
        Assert(this->inlineCaches != nullptr);
        Assert(index < GetIsInstInlineCacheCount());
        index += this->GetInlineCacheCount();
#if DBG
        Assert(this->m_inlineCacheTypes[index] == InlineCacheTypeNone ||
            this->m_inlineCacheTypes[index] == InlineCacheTypeIsInst);
        this->m_inlineCacheTypes[index] = InlineCacheTypeIsInst;
#endif
        return reinterpret_cast<IsInstInlineCache *>(this->inlineCaches[index]);
    }

    PolymorphicInlineCache * FunctionBody::GetPolymorphicInlineCache(uint index)
    {TRACE_IT(34559);
        return this->polymorphicInlineCaches.GetInlineCache(this, index);
    }

    PolymorphicInlineCache * FunctionBody::CreateNewPolymorphicInlineCache(uint index, PropertyId propertyId, InlineCache * inlineCache)
    {TRACE_IT(34560);
        Assert(GetPolymorphicInlineCache(index) == nullptr);
        // Only create polymorphic inline caches for non-root inline cache indexes
        if (index < GetRootObjectLoadInlineCacheStart()
#if DBG
            && !PHASE_OFF1(Js::PolymorphicInlineCachePhase)
#endif
            )
        {TRACE_IT(34561);
            PolymorphicInlineCache * polymorphicInlineCache = CreatePolymorphicInlineCache(index, PolymorphicInlineCache::GetInitialSize());
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            if (PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
            {TRACE_IT(34562);
                this->DumpFullFunctionName();
                Output::Print(_u(": New PIC, index = %d, size = %d\n"), index, PolymorphicInlineCache::GetInitialSize());
            }

#endif
#if PHASE_PRINT_INTRUSIVE_TESTTRACE1
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
            PHASE_PRINT_INTRUSIVE_TESTTRACE1(
                Js::PolymorphicInlineCachePhase,
                _u("TestTrace PIC:  New, Function %s (%s), 0x%x, index = %d, size = %d\n"), this->GetDisplayName(), this->GetDebugNumberSet(debugStringBuffer), polymorphicInlineCache, index, PolymorphicInlineCache::GetInitialSize());

            uint indexInPolymorphicCache = polymorphicInlineCache->GetInlineCacheIndexForType(inlineCache->GetType());
            inlineCache->CopyTo(propertyId, m_scriptContext, &(polymorphicInlineCache->GetInlineCaches()[indexInPolymorphicCache]));
            polymorphicInlineCache->UpdateInlineCachesFillInfo(indexInPolymorphicCache, true /*set*/);

            return polymorphicInlineCache;
        }
        return nullptr;
    }

    PolymorphicInlineCache * FunctionBody::CreateBiggerPolymorphicInlineCache(uint index, PropertyId propertyId)
    {TRACE_IT(34563);
        PolymorphicInlineCache * polymorphicInlineCache = GetPolymorphicInlineCache(index);
        Assert(polymorphicInlineCache && polymorphicInlineCache->CanAllocateBigger());
        uint16 polymorphicInlineCacheSize = polymorphicInlineCache->GetSize();
        uint16 newPolymorphicInlineCacheSize = PolymorphicInlineCache::GetNextSize(polymorphicInlineCacheSize);
        Assert(newPolymorphicInlineCacheSize > polymorphicInlineCacheSize);
        PolymorphicInlineCache * newPolymorphicInlineCache = CreatePolymorphicInlineCache(index, newPolymorphicInlineCacheSize);
        polymorphicInlineCache->CopyTo(propertyId, m_scriptContext, newPolymorphicInlineCache);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
        {TRACE_IT(34564);
            this->DumpFullFunctionName();
            Output::Print(_u(": Bigger PIC, index = %d, oldSize = %d, newSize = %d\n"), index, polymorphicInlineCacheSize, newPolymorphicInlineCacheSize);
        }
#endif
#if PHASE_PRINT_INTRUSIVE_TESTTRACE1
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        PHASE_PRINT_INTRUSIVE_TESTTRACE1(
            Js::PolymorphicInlineCachePhase,
            _u("TestTrace PIC:  Bigger, Function %s (%s), 0x%x, index = %d, size = %d\n"), this->GetDisplayName(), this->GetDebugNumberSet(debugStringBuffer), newPolymorphicInlineCache, index, newPolymorphicInlineCacheSize);
        return newPolymorphicInlineCache;
    }

    void FunctionBody::ResetInlineCaches()
    {TRACE_IT(34565);
        SetInlineCacheCount(0);
        SetRootObjectLoadInlineCacheStart(0);
        SetRootObjectStoreInlineCacheStart(0);
        SetIsInstInlineCacheCount(0);
        this->inlineCaches = nullptr;
        this->polymorphicInlineCaches.Reset();
    }

    PolymorphicInlineCache * FunctionBody::CreatePolymorphicInlineCache(uint index, uint16 size)
    {TRACE_IT(34566);
        Recycler * recycler = this->m_scriptContext->GetRecycler();
        PolymorphicInlineCache * newPolymorphicInlineCache = PolymorphicInlineCache::New(size, this);
        this->polymorphicInlineCaches.SetInlineCache(recycler, this, index, newPolymorphicInlineCache);
        return newPolymorphicInlineCache;
    }

    uint FunctionBody::NewObjectLiteral()
    {TRACE_IT(34567);
        Assert(this->GetObjectLiteralTypes() == nullptr);
        return IncObjLiteralCount();
    }

    Field(DynamicType*)* FunctionBody::GetObjectLiteralTypeRef(uint index)
    {TRACE_IT(34568);
        Assert(index < GetObjLiteralCount());
        auto literalTypes = this->GetObjectLiteralTypes();
        Assert(literalTypes != nullptr);
        return literalTypes + index;
    }
    Field(DynamicType*)* FunctionBody::GetObjectLiteralTypeRefWithLock(uint index)
    {TRACE_IT(34569);
        Assert(index < GetObjLiteralCount());
        auto literalTypes = this->GetObjectLiteralTypesWithLock();
        Assert(literalTypes != nullptr);
        return literalTypes + index;
    }

    void FunctionBody::AllocateObjectLiteralTypeArray()
    {TRACE_IT(34570);
        Assert(this->GetObjectLiteralTypes() == nullptr);
        uint objLiteralCount = GetObjLiteralCount();
        if (objLiteralCount == 0)
        {TRACE_IT(34571);
            return;
        }

        this->SetObjectLiteralTypes(RecyclerNewArrayZ(this->GetScriptContext()->GetRecycler(), DynamicType *, objLiteralCount));
    }

    uint FunctionBody::NewLiteralRegex()
    {TRACE_IT(34572);
        if (this->GetLiteralRegexes() != nullptr)
        {TRACE_IT(34573);
            // This is a function nested in a redeferred function, so we won't regenerate byte code and won't make use of the index.
            // The regex count is already correct, so don't increment it.
            return 0;
        }
        return IncLiteralRegexCount();
    }


    void FunctionBody::AllocateLiteralRegexArray()
    {TRACE_IT(34574);
        Assert(!this->GetLiteralRegexes());
        uint32 literalRegexCount = GetLiteralRegexCount();
        if (literalRegexCount == 0)
        {TRACE_IT(34575);
            return;
        }

        this->SetLiteralRegexs(RecyclerNewArrayZ(m_scriptContext->GetRecycler(), UnifiedRegex::RegexPattern *, literalRegexCount));
    }

#ifdef ASMJS_PLAT
    AsmJsFunctionInfo* FunctionBody::AllocateAsmJsFunctionInfo()
    {TRACE_IT(34576);
        Assert( !this->GetAsmJsFunctionInfo() );
        this->SetAuxPtr(AuxPointerType::AsmJsFunctionInfo, RecyclerNew( m_scriptContext->GetRecycler(), AsmJsFunctionInfo));
        return this->GetAsmJsFunctionInfo();
    }

    AsmJsModuleInfo* FunctionBody::AllocateAsmJsModuleInfo()
    {TRACE_IT(34577);
        Assert( !this->GetAsmJsModuleInfo() );
        Recycler* rec = m_scriptContext->GetRecycler();
        this->SetAuxPtr(AuxPointerType::AsmJsModuleInfo, RecyclerNew(rec, AsmJsModuleInfo, rec));
        return this->GetAsmJsModuleInfo();
    }
#endif

    PropertyIdArray * FunctionBody::AllocatePropertyIdArrayForFormals(uint32 size, uint32 count, byte extraSlots)
    {TRACE_IT(34578);
        //TODO: saravind: Should the allocation be a Leaf Allocation?
        PropertyIdArray * formalsPropIdArray = RecyclerNewPlus(GetScriptContext()->GetRecycler(), size, Js::PropertyIdArray, count, extraSlots);
        SetFormalsPropIdArray(formalsPropIdArray);
        return formalsPropIdArray;
    }

    UnifiedRegex::RegexPattern *FunctionBody::GetLiteralRegex(const uint index)
    {TRACE_IT(34579);
        Assert(index < GetLiteralRegexCount());
        Assert(this->GetLiteralRegexes());

        return this->GetLiteralRegexes()[index];
    }

    UnifiedRegex::RegexPattern *FunctionBody::GetLiteralRegexWithLock(const uint index)
    {TRACE_IT(34580);
        Assert(index < GetLiteralRegexCount());
        Assert(this->GetLiteralRegexesWithLock());

        return this->GetLiteralRegexesWithLock()[index];
    }

    void FunctionBody::SetLiteralRegex(const uint index, UnifiedRegex::RegexPattern *const pattern)
    {TRACE_IT(34581);
        Assert(index < GetLiteralRegexCount());
        Assert(this->GetLiteralRegexes());

        auto literalRegexes = this->GetLiteralRegexes();
        if (literalRegexes[index] && literalRegexes[index] == pattern)
        {TRACE_IT(34582);
            return;
        }
        Assert(!literalRegexes[index]);

        literalRegexes[index] = pattern;
    }

    void FunctionBody::ResetObjectLiteralTypes()
    {TRACE_IT(34583);
        this->SetObjectLiteralTypes(nullptr);
        this->SetObjLiteralCount(0);
    }

    void FunctionBody::ResetLiteralRegexes()
    {TRACE_IT(34584);
        SetLiteralRegexCount(0);
        this->SetLiteralRegexs(nullptr);
    }

    void FunctionBody::ResetProfileIds()
    {TRACE_IT(34585);
#if ENABLE_PROFILE_INFO
        Assert(!HasDynamicProfileInfo()); // profile data relies on the profile ID counts; it should not have been created yet
        Assert(!this->GetCodeGenRuntimeData()); // relies on 'profiledCallSiteCount'

        profiledCallSiteCount = 0;
        profiledArrayCallSiteCount = 0;
        profiledReturnTypeCount = 0;
        profiledSlotCount = 0;
        profiledLdElemCount = 0;
        profiledStElemCount = 0;
#endif
    }

    void FunctionBody::ResetByteCodeGenState()
    {TRACE_IT(34586);
        // Byte code generation failed for this function. Revert any intermediate state being tracked in the function body, in
        // case byte code generation is attempted again for this function body.

        ResetInlineCaches();
        ResetObjectLiteralTypes();
        ResetLiteralRegexes();
        ResetLoops();
        ResetProfileIds();

        SetFirstTmpRegister(Constants::NoRegister);
        SetLocalClosureRegister(Constants::NoRegister);
        SetParamClosureRegister(Constants::NoRegister);
        SetLocalFrameDisplayRegister(Constants::NoRegister);
        SetEnvRegister(Constants::NoRegister);
        SetThisRegisterForEventHandler(Constants::NoRegister);
        SetFirstInnerScopeRegister(Constants::NoRegister);
        SetFuncExprScopeRegister(Constants::NoRegister);
        SetInnerScopeCount(0);
        hasCachedScopePropIds = false;
        this->SetConstantCount(0);
        this->SetConstTable(nullptr);
        this->byteCodeBlock = nullptr;

        // Also, remove the function body from the source info to prevent any further processing
        // of the function such as attempts to set breakpoints.
        if (GetIsFuncRegistered())
        {TRACE_IT(34587);
            this->GetUtf8SourceInfo()->RemoveFunctionBody(this);
        }

        // There is other state that is set by the byte code generator but the state should be the same each time byte code
        // generation is done for the function, so it doesn't need to be reverted
    }

    void FunctionBody::ResetByteCodeGenVisitState()
    {TRACE_IT(34588);
        // This function body is about to be visited by the byte code generator after defer-parsing it. Since the previous visit
        // pass may have failed, we need to restore state that is tracked on the function body by the visit pass.
        // Note: do not reset literal regexes if the function has already been compiled (e.g., is a parsed function enclosed by a
        // redeferred function) as we will not use the count of literals anyway, and the counters may be accessed by the background thread.
        if (this->byteCodeBlock == nullptr)
        {TRACE_IT(34589);
            ResetLiteralRegexes();
        }
    }

#if ENABLE_NATIVE_CODEGEN
    const FunctionCodeGenRuntimeData *FunctionBody::GetInlineeCodeGenRuntimeData(const ProfileId profiledCallSiteId) const
    {TRACE_IT(34590);
        Assert(profiledCallSiteId < profiledCallSiteCount);

        auto codeGenRuntimeData = this->GetCodeGenRuntimeDataWithLock();
        return codeGenRuntimeData ? codeGenRuntimeData[profiledCallSiteId] : nullptr;
    }

    const FunctionCodeGenRuntimeData *FunctionBody::GetInlineeCodeGenRuntimeDataForTargetInlinee(const ProfileId profiledCallSiteId, Js::FunctionBody *inlineeFuncBody) const
    {TRACE_IT(34591);
        Assert(profiledCallSiteId < profiledCallSiteCount);

        auto codeGenRuntimeData = this->GetCodeGenRuntimeDataWithLock();
        if (!codeGenRuntimeData)
        {TRACE_IT(34592);
            return nullptr;
        }
        const FunctionCodeGenRuntimeData *runtimeData = codeGenRuntimeData[profiledCallSiteId];
        while (runtimeData && runtimeData->GetFunctionBody() != inlineeFuncBody)
        {TRACE_IT(34593);
            runtimeData = runtimeData->GetNext();
        }
        return runtimeData;
    }

    FunctionCodeGenRuntimeData *FunctionBody::EnsureInlineeCodeGenRuntimeData(
        Recycler *const recycler,
        __in_range(0, profiledCallSiteCount - 1) const ProfileId profiledCallSiteId,
        FunctionBody *const inlinee)
    {TRACE_IT(34594);
        Assert(recycler);
        Assert(profiledCallSiteId < profiledCallSiteCount);
        Assert(inlinee);

        if(!this->GetCodeGenRuntimeData())
        {TRACE_IT(34595);
            const auto codeGenRuntimeData = RecyclerNewArrayZ(recycler, FunctionCodeGenRuntimeData *, profiledCallSiteCount);
            this->SetCodeGenRuntimeData(codeGenRuntimeData);
        }

        auto codeGenRuntimeData = this->GetCodeGenRuntimeData();
        const auto inlineeData = codeGenRuntimeData[profiledCallSiteId];

        if(!inlineeData)
        {TRACE_IT(34596);
            return codeGenRuntimeData[profiledCallSiteId] = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
        }

        // Find the right code gen runtime data
        FunctionCodeGenRuntimeData *next = inlineeData;

        while(next && (next->GetFunctionBody() != inlinee))
        {TRACE_IT(34597);
            next = next->GetNext();
        }

        if (next)
        {TRACE_IT(34598);
            return next;
        }

        FunctionCodeGenRuntimeData *runtimeData = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
        runtimeData->SetupRuntimeDataChain(inlineeData);
        return codeGenRuntimeData[profiledCallSiteId] = runtimeData;
    }

    const FunctionCodeGenRuntimeData *FunctionBody::GetLdFldInlineeCodeGenRuntimeData(const InlineCacheIndex inlineCacheIndex) const
    {TRACE_IT(34599);
        Assert(inlineCacheIndex < this->GetInlineCacheCount());

        auto data = this->GetCodeGenGetSetRuntimeDataWithLock();
        return (data != nullptr) ? data[inlineCacheIndex] : nullptr;
    }

    FunctionCodeGenRuntimeData *FunctionBody::EnsureLdFldInlineeCodeGenRuntimeData(
        Recycler *const recycler,
        const InlineCacheIndex inlineCacheIndex,
        FunctionBody *const inlinee)
    {TRACE_IT(34600);
        Assert(recycler);
        Assert(inlineCacheIndex < this->GetInlineCacheCount());
        Assert(inlinee);

        if (this->GetCodeGenGetSetRuntimeData() == nullptr)
        {TRACE_IT(34601);
            const auto codeGenRuntimeData = RecyclerNewArrayZ(recycler, FunctionCodeGenRuntimeData *, this->GetInlineCacheCount());
            this->SetCodeGenGetSetRuntimeData(codeGenRuntimeData);
        }

        auto codeGenGetSetRuntimeData = this->GetCodeGenGetSetRuntimeData();
        const auto inlineeData = codeGenGetSetRuntimeData[inlineCacheIndex];
        if (inlineeData)
        {TRACE_IT(34602);
            return inlineeData;
        }

        return codeGenGetSetRuntimeData[inlineCacheIndex] = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
    }
#endif

    void FunctionBody::AllocateLoopHeaders()
    {TRACE_IT(34603);
        Assert(this->GetLoopHeaderArray() == nullptr);

        uint loopCount = GetLoopCount();
        if (loopCount != 0)
        {TRACE_IT(34604);
            this->SetLoopHeaderArray(RecyclerNewArrayZ(this->m_scriptContext->GetRecycler(), LoopHeader, loopCount));
            auto loopHeaderArray = this->GetLoopHeaderArray();
            for (uint i = 0; i < loopCount; i++)
            {TRACE_IT(34605);
                loopHeaderArray[i].Init(this);
            }
        }
    }

    void FunctionBody::ReleaseLoopHeaders()
    {TRACE_IT(34606);
#if ENABLE_NATIVE_CODEGEN
        this->MapLoopHeaders([](uint loopNumber, LoopHeader * loopHeader)
        {
            loopHeader->ReleaseEntryPoints();
        });
#endif
    }

    void FunctionBody::ResetLoops()
    {TRACE_IT(34607);
        SetLoopCount(0);
        this->SetLoopHeaderArray(nullptr);
    }

    void FunctionBody::RestoreOldDefaultEntryPoint(FunctionEntryPointInfo* oldEntryPointInfo,
        JavascriptMethod oldOriginalEntryPoint,
        FunctionEntryPointInfo* newEntryPointInfo)
    {TRACE_IT(34608);
        Assert(newEntryPointInfo);

        this->SetDefaultFunctionEntryPointInfo(oldEntryPointInfo, oldOriginalEntryPoint);
        this->entryPoints->RemoveAt(newEntryPointInfo->entryPointIndex);
    }

    FunctionEntryPointInfo* FunctionBody::CreateNewDefaultEntryPoint()
    {TRACE_IT(34609);
        Recycler *const recycler = this->m_scriptContext->GetRecycler();
        const JavascriptMethod currentThunk = m_scriptContext->CurrentThunk;

        void* validationCookie = nullptr;
#if ENABLE_NATIVE_CODEGEN
        validationCookie = (void*)m_scriptContext->GetNativeCodeGenerator();
#endif

        FunctionEntryPointInfo *const entryPointInfo =
            RecyclerNewFinalized(
                recycler,
                FunctionEntryPointInfo,
                this,
                currentThunk,
                m_scriptContext->GetThreadContext(),
                validationCookie);

        AddEntryPointToEntryPointList(entryPointInfo);

        {TRACE_IT(34610);
            // Allocations in this region may trigger expiry and cause unexpected changes to state
            AUTO_NO_EXCEPTION_REGION;

            FunctionEntryPointInfo *const simpleJitEntryPointInfo = GetSimpleJitEntryPointInfo();
            Js::JavascriptMethod originalEntryPoint, directEntryPoint;
            if(simpleJitEntryPointInfo && GetExecutionMode() == ExecutionMode::FullJit)
            {TRACE_IT(34611);
                directEntryPoint =
                    originalEntryPoint = reinterpret_cast<Js::JavascriptMethod>(simpleJitEntryPointInfo->GetNativeAddress());
            }
            else
            {TRACE_IT(34612);
#if DYNAMIC_INTERPRETER_THUNK
                // If the dynamic interpreter thunk hasn't been created yet, then the entry point can be set to
                // the default entry point. Otherwise, since the new default entry point is being created to
                // move back to the interpreter, the original entry point is going to be the dynamic interpreter thunk
                originalEntryPoint =
                    m_dynamicInterpreterThunk
                        ? reinterpret_cast<JavascriptMethod>(InterpreterThunkEmitter::ConvertToEntryPoint(m_dynamicInterpreterThunk))
                        : DefaultEntryThunk;
#else
                originalEntryPoint = DefaultEntryThunk;
#endif

                directEntryPoint = currentThunk == DefaultEntryThunk ? originalEntryPoint : currentThunk;
            }

            entryPointInfo->jsMethod = directEntryPoint;
            SetDefaultFunctionEntryPointInfo(entryPointInfo, originalEntryPoint);
        }

        return entryPointInfo;
    }

    LoopHeader *FunctionBody::GetLoopHeader(uint index) const
    {TRACE_IT(34613);
        Assert(this->GetLoopHeaderArray() != nullptr);
        Assert(index < GetLoopCount());
        return &this->GetLoopHeaderArray()[index];
    }

    LoopHeader *FunctionBody::GetLoopHeaderWithLock(uint index) const
    {TRACE_IT(34614);
        Assert(this->GetLoopHeaderArrayWithLock() != nullptr);
        Assert(index < GetLoopCount());
        return &this->GetLoopHeaderArrayWithLock()[index];
    }
    FunctionEntryPointInfo *FunctionBody::GetSimpleJitEntryPointInfo() const
    {TRACE_IT(34615);
        return static_cast<FunctionEntryPointInfo *>(this->GetAuxPtr(AuxPointerType::SimpleJitEntryPointInfo));
    }

    void FunctionBody::SetSimpleJitEntryPointInfo(FunctionEntryPointInfo *const entryPointInfo)
    {TRACE_IT(34616);
        this->SetAuxPtr(AuxPointerType::SimpleJitEntryPointInfo, entryPointInfo);
    }

    void FunctionBody::VerifyExecutionMode(const ExecutionMode executionMode) const
    {TRACE_IT(34617);
#if DBG
        Assert(initializedExecutionModeAndLimits);
        Assert(executionMode < ExecutionMode::Count);

        switch(executionMode)
        {
            case ExecutionMode::Interpreter:
                Assert(!DoInterpreterProfile());
                break;

            case ExecutionMode::AutoProfilingInterpreter:
                Assert(DoInterpreterProfile());
                Assert(DoInterpreterAutoProfile());
                break;

            case ExecutionMode::ProfilingInterpreter:
                Assert(DoInterpreterProfile());
                break;

            case ExecutionMode::SimpleJit:
                Assert(DoSimpleJit());
                break;

            case ExecutionMode::FullJit:
                Assert(!PHASE_OFF(FullJitPhase, this));
                break;

            default:
                Assert(false);
                __assume(false);
        }
#endif
    }

    ExecutionMode FunctionBody::GetDefaultInterpreterExecutionMode() const
    {TRACE_IT(34618);
        if(!DoInterpreterProfile())
        {TRACE_IT(34619);
            VerifyExecutionMode(ExecutionMode::Interpreter);
            return ExecutionMode::Interpreter;
        }
        if(DoInterpreterAutoProfile())
        {TRACE_IT(34620);
            VerifyExecutionMode(ExecutionMode::AutoProfilingInterpreter);
            return ExecutionMode::AutoProfilingInterpreter;
        }
        VerifyExecutionMode(ExecutionMode::ProfilingInterpreter);
        return ExecutionMode::ProfilingInterpreter;
    }

    ExecutionMode FunctionBody::GetExecutionMode() const
    {TRACE_IT(34621);
        VerifyExecutionMode(executionMode);
        return executionMode;
    }

    ExecutionMode FunctionBody::GetInterpreterExecutionMode(const bool isPostBailout)
    {TRACE_IT(34622);
        Assert(initializedExecutionModeAndLimits);

        if(isPostBailout && DoInterpreterProfile())
        {TRACE_IT(34623);
            return ExecutionMode::ProfilingInterpreter;
        }

        switch(GetExecutionMode())
        {
            case ExecutionMode::Interpreter:
            case ExecutionMode::AutoProfilingInterpreter:
            case ExecutionMode::ProfilingInterpreter:
                return GetExecutionMode();

            case ExecutionMode::SimpleJit:
                if(CONFIG_FLAG(NewSimpleJit))
                {TRACE_IT(34624);
                    return GetDefaultInterpreterExecutionMode();
                }
                // fall through

            case ExecutionMode::FullJit:
            {TRACE_IT(34625);
                const ExecutionMode executionMode =
                    DoInterpreterProfile() ? ExecutionMode::ProfilingInterpreter : ExecutionMode::Interpreter;
                VerifyExecutionMode(executionMode);
                return executionMode;
            }

            default:
                Assert(false);
                __assume(false);
        }
    }

    void FunctionBody::SetExecutionMode(const ExecutionMode executionMode)
    {TRACE_IT(34626);
        VerifyExecutionMode(executionMode);
        this->executionMode = executionMode;
    }

    bool FunctionBody::IsInterpreterExecutionMode() const
    {TRACE_IT(34627);
        return GetExecutionMode() <= ExecutionMode::ProfilingInterpreter;
    }

    bool FunctionBody::TryTransitionToNextExecutionMode()
    {TRACE_IT(34628);
        Assert(initializedExecutionModeAndLimits);

        switch(GetExecutionMode())
        {
            case ExecutionMode::Interpreter:
                if(GetInterpretedCount() < interpreterLimit)
                {TRACE_IT(34629);
                    VerifyExecutionMode(GetExecutionMode());
                    return false;
                }
                CommitExecutedIterations(interpreterLimit, interpreterLimit);
                goto TransitionToFullJit;

            TransitionToAutoProfilingInterpreter:
                if(autoProfilingInterpreter0Limit != 0 || autoProfilingInterpreter1Limit != 0)
                {TRACE_IT(34630);
                    SetExecutionMode(ExecutionMode::AutoProfilingInterpreter);
                    SetInterpretedCount(0);
                    return true;
                }
                goto TransitionFromAutoProfilingInterpreter;

            case ExecutionMode::AutoProfilingInterpreter:
            {TRACE_IT(34631);
                uint16 &autoProfilingInterpreterLimit =
                    autoProfilingInterpreter0Limit == 0 && profilingInterpreter0Limit == 0
                        ? autoProfilingInterpreter1Limit
                        : autoProfilingInterpreter0Limit;
                if(GetInterpretedCount() < autoProfilingInterpreterLimit)
                {TRACE_IT(34632);
                    VerifyExecutionMode(GetExecutionMode());
                    return false;
                }
                CommitExecutedIterations(autoProfilingInterpreterLimit, autoProfilingInterpreterLimit);
                // fall through
            }

            TransitionFromAutoProfilingInterpreter:
                Assert(autoProfilingInterpreter0Limit == 0 || autoProfilingInterpreter1Limit == 0);
                if(profilingInterpreter0Limit == 0 && autoProfilingInterpreter1Limit == 0)
                {TRACE_IT(34633);
                    goto TransitionToSimpleJit;
                }
                // fall through

            TransitionToProfilingInterpreter:
                if(profilingInterpreter0Limit != 0 || profilingInterpreter1Limit != 0)
                {TRACE_IT(34634);
                    SetExecutionMode(ExecutionMode::ProfilingInterpreter);
                    SetInterpretedCount(0);
                    return true;
                }
                goto TransitionFromProfilingInterpreter;

            case ExecutionMode::ProfilingInterpreter:
            {TRACE_IT(34635);
                uint16 &profilingInterpreterLimit =
                    profilingInterpreter0Limit == 0 && autoProfilingInterpreter1Limit == 0 && simpleJitLimit == 0
                        ? profilingInterpreter1Limit
                        : profilingInterpreter0Limit;
                if(GetInterpretedCount() < profilingInterpreterLimit)
                {TRACE_IT(34636);
                    VerifyExecutionMode(GetExecutionMode());
                    return false;
                }
                CommitExecutedIterations(profilingInterpreterLimit, profilingInterpreterLimit);
                // fall through
            }

            TransitionFromProfilingInterpreter:
                Assert(profilingInterpreter0Limit == 0 || profilingInterpreter1Limit == 0);
                if(autoProfilingInterpreter1Limit == 0 && simpleJitLimit == 0 && profilingInterpreter1Limit == 0)
                {TRACE_IT(34637);
                    goto TransitionToFullJit;
                }
                goto TransitionToAutoProfilingInterpreter;

            TransitionToSimpleJit:
                if(simpleJitLimit != 0)
                {TRACE_IT(34638);
                    SetExecutionMode(ExecutionMode::SimpleJit);

                    // Zero the interpreted count here too, so that we can determine how many interpreter iterations ran
                    // while waiting for simple JIT
                    SetInterpretedCount(0);
                    return true;
                }
                goto TransitionToProfilingInterpreter;

            case ExecutionMode::SimpleJit:
            {TRACE_IT(34639);
                FunctionEntryPointInfo *const simpleJitEntryPointInfo = GetSimpleJitEntryPointInfo();
                if(!simpleJitEntryPointInfo || simpleJitEntryPointInfo->callsCount != 0)
                {TRACE_IT(34640);
                    VerifyExecutionMode(GetExecutionMode());
                    return false;
                }
                CommitExecutedIterations(simpleJitLimit, simpleJitLimit);
                goto TransitionToProfilingInterpreter;
            }

            TransitionToFullJit:
                if(!PHASE_OFF(FullJitPhase, this))
                {TRACE_IT(34641);
                    SetExecutionMode(ExecutionMode::FullJit);
                    return true;
                }
                // fall through

            case ExecutionMode::FullJit:
                VerifyExecutionMode(GetExecutionMode());
                return false;

            default:
                Assert(false);
                __assume(false);
        }
    }

    void FunctionBody::TryTransitionToNextInterpreterExecutionMode()
    {TRACE_IT(34642);
        Assert(IsInterpreterExecutionMode());

        TryTransitionToNextExecutionMode();
        SetExecutionMode(GetInterpreterExecutionMode(false));
    }

    void FunctionBody::SetIsSpeculativeJitCandidate()
    {TRACE_IT(34643);
        // This function is a candidate for speculative JIT. Ensure that it is profiled immediately by transitioning out of the
        // auto-profiling interpreter mode.
        if(GetExecutionMode() != ExecutionMode::AutoProfilingInterpreter || GetProfiledIterations() != 0)
        {TRACE_IT(34644);
            return;
        }

        TraceExecutionMode("IsSpeculativeJitCandidate (before)");

        if(autoProfilingInterpreter0Limit != 0)
        {TRACE_IT(34645);
            (profilingInterpreter0Limit == 0 ? profilingInterpreter0Limit : autoProfilingInterpreter1Limit) +=
                autoProfilingInterpreter0Limit;
            autoProfilingInterpreter0Limit = 0;
        }
        else if(profilingInterpreter0Limit == 0)
        {TRACE_IT(34646);
            profilingInterpreter0Limit += autoProfilingInterpreter1Limit;
            autoProfilingInterpreter1Limit = 0;
        }

        TraceExecutionMode("IsSpeculativeJitCandidate");
        TryTransitionToNextInterpreterExecutionMode();
    }

    bool FunctionBody::TryTransitionToJitExecutionMode()
    {TRACE_IT(34647);
        const ExecutionMode previousExecutionMode = GetExecutionMode();

        TryTransitionToNextExecutionMode();
        switch(GetExecutionMode())
        {
            case ExecutionMode::SimpleJit:
                break;

            case ExecutionMode::FullJit:
                if(fullJitRequeueThreshold == 0)
                {TRACE_IT(34648);
                    break;
                }
                --fullJitRequeueThreshold;
                return false;

            default:
                return false;
        }

        if(GetExecutionMode() != previousExecutionMode)
        {TRACE_IT(34649);
            TraceExecutionMode();
        }
        return true;
    }

    void FunctionBody::TransitionToSimpleJitExecutionMode()
    {TRACE_IT(34650);
        CommitExecutedIterations();

        interpreterLimit = 0;
        autoProfilingInterpreter0Limit = 0;
        profilingInterpreter0Limit = 0;
        autoProfilingInterpreter1Limit = 0;
        fullJitThreshold = simpleJitLimit + profilingInterpreter1Limit;

        VerifyExecutionModeLimits();
        SetExecutionMode(ExecutionMode::SimpleJit);
    }

    void FunctionBody::TransitionToFullJitExecutionMode()
    {TRACE_IT(34651);
        CommitExecutedIterations();

        interpreterLimit = 0;
        autoProfilingInterpreter0Limit = 0;
        profilingInterpreter0Limit = 0;
        autoProfilingInterpreter1Limit = 0;
        simpleJitLimit = 0;
        profilingInterpreter1Limit = 0;
        fullJitThreshold = 0;

        VerifyExecutionModeLimits();
        SetExecutionMode(ExecutionMode::FullJit);
    }

    void FunctionBody::VerifyExecutionModeLimits()
    {TRACE_IT(34652);
        Assert(initializedExecutionModeAndLimits);
        Assert(
            (
                interpreterLimit +
                autoProfilingInterpreter0Limit +
                profilingInterpreter0Limit +
                autoProfilingInterpreter1Limit +
                simpleJitLimit +
                profilingInterpreter1Limit
            ) == fullJitThreshold);
    }

    void FunctionBody::InitializeExecutionModeAndLimits()
    {TRACE_IT(34653);
        DebugOnly(initializedExecutionModeAndLimits = true);

        const ConfigFlagsTable &configFlags = Configuration::Global.flags;

        interpreterLimit = 0;
        autoProfilingInterpreter0Limit = static_cast<uint16>(configFlags.AutoProfilingInterpreter0Limit);
        profilingInterpreter0Limit = static_cast<uint16>(configFlags.ProfilingInterpreter0Limit);
        autoProfilingInterpreter1Limit = static_cast<uint16>(configFlags.AutoProfilingInterpreter1Limit);
        simpleJitLimit = static_cast<uint16>(configFlags.SimpleJitLimit);
        profilingInterpreter1Limit = static_cast<uint16>(configFlags.ProfilingInterpreter1Limit);

        // Based on which execution modes are disabled, calculate the number of additional iterations that need to be covered by
        // the execution mode that will scale with the full JIT threshold
        uint16 scale = 0;
        const bool doInterpreterProfile = DoInterpreterProfile();
        if(!doInterpreterProfile)
        {TRACE_IT(34654);
            scale +=
                autoProfilingInterpreter0Limit +
                profilingInterpreter0Limit +
                autoProfilingInterpreter1Limit +
                profilingInterpreter1Limit;
            autoProfilingInterpreter0Limit = 0;
            profilingInterpreter0Limit = 0;
            autoProfilingInterpreter1Limit = 0;
            profilingInterpreter1Limit = 0;
        }
        else if(!DoInterpreterAutoProfile())
        {TRACE_IT(34655);
            scale += autoProfilingInterpreter0Limit + autoProfilingInterpreter1Limit;
            autoProfilingInterpreter0Limit = 0;
            autoProfilingInterpreter1Limit = 0;
            if(!CONFIG_FLAG(NewSimpleJit))
            {TRACE_IT(34656);
                simpleJitLimit += profilingInterpreter0Limit;
                profilingInterpreter0Limit = 0;
            }
        }
        if(!DoSimpleJit())
        {TRACE_IT(34657);
            if(!CONFIG_FLAG(NewSimpleJit) && doInterpreterProfile)
            {TRACE_IT(34658);
                // The old simple JIT is off, but since it does profiling, it will be replaced with the profiling interpreter
                profilingInterpreter1Limit += simpleJitLimit;
            }
            else
            {TRACE_IT(34659);
                scale += simpleJitLimit;
            }
            simpleJitLimit = 0;
        }
        if(PHASE_OFF(FullJitPhase, this))
        {TRACE_IT(34660);
            scale += profilingInterpreter1Limit;
            profilingInterpreter1Limit = 0;
        }

        uint16 fullJitThreshold =
            static_cast<uint16>(
                configFlags.AutoProfilingInterpreter0Limit +
                configFlags.ProfilingInterpreter0Limit +
                configFlags.AutoProfilingInterpreter1Limit +
                configFlags.SimpleJitLimit +
                configFlags.ProfilingInterpreter1Limit);
        if(!configFlags.EnforceExecutionModeLimits)
        {TRACE_IT(34661);
            /*
            Scale the full JIT threshold based on some heuristics:
                - If the % of code in loops is > 50, scale by 1
                - Byte-code size of code outside loops
                    - If the size is < 50, scale by 1.2
                    - If the size is < 100, scale by 1.4
                    - If the size is >= 100, scale by 1.6
            */
            const uint loopPercentage = GetByteCodeInLoopCount() * 100 / max(1u, GetByteCodeCount());
            const int byteCodeSizeThresholdForInlineCandidate = CONFIG_FLAG(LoopInlineThreshold);
            bool delayFullJITThisFunc =
                (CONFIG_FLAG(DelayFullJITSmallFunc) > 0) && (this->GetByteCodeWithoutLDACount() <= (uint)byteCodeSizeThresholdForInlineCandidate);

            if(loopPercentage <= 50 || delayFullJITThisFunc)
            {TRACE_IT(34662);
                const uint straightLineSize = GetByteCodeCount() - GetByteCodeInLoopCount();
                double fullJitDelayMultiplier;
                if (delayFullJITThisFunc)
                {TRACE_IT(34663);
                    fullJitDelayMultiplier = CONFIG_FLAG(DelayFullJITSmallFunc) / 10.0;
                }
                else if(straightLineSize < 50)
                {TRACE_IT(34664);
                    fullJitDelayMultiplier = 1.2;
                }
                else if(straightLineSize < 100)
                {TRACE_IT(34665);
                    fullJitDelayMultiplier = 1.4;
                }
                else
                {TRACE_IT(34666);
                    fullJitDelayMultiplier = 1.6;
                }

                const uint16 newFullJitThreshold = static_cast<uint16>(fullJitThreshold * fullJitDelayMultiplier);
                scale += newFullJitThreshold - fullJitThreshold;
                fullJitThreshold = newFullJitThreshold;
            }
        }

        Assert(fullJitThreshold >= scale);
        this->fullJitThreshold = fullJitThreshold - scale;
        SetInterpretedCount(0);
        SetExecutionMode(GetDefaultInterpreterExecutionMode());
        SetFullJitThreshold(fullJitThreshold);
        TryTransitionToNextInterpreterExecutionMode();
    }

    void FunctionBody::ReinitializeExecutionModeAndLimits()
    {TRACE_IT(34667);
        wasCalledFromLoop = false;
        fullJitRequeueThreshold = 0;
        committedProfiledIterations = 0;
        InitializeExecutionModeAndLimits();
    }

    void FunctionBody::SetFullJitThreshold(const uint16 newFullJitThreshold, const bool skipSimpleJit)
    {TRACE_IT(34668);
        Assert(initializedExecutionModeAndLimits);
        Assert(GetExecutionMode() != ExecutionMode::FullJit);

        int scale = newFullJitThreshold - fullJitThreshold;
        if(scale == 0)
        {TRACE_IT(34669);
            VerifyExecutionModeLimits();
            return;
        }
        fullJitThreshold = newFullJitThreshold;

        const auto ScaleLimit = [&](uint16 &limit) -> bool
        {
            Assert(scale != 0);
            const int limitScale = max(-static_cast<int>(limit), scale);
            const int newLimit = limit + limitScale;
            Assert(static_cast<int>(static_cast<uint16>(newLimit)) == newLimit);
            limit = static_cast<uint16>(newLimit);
            scale -= limitScale;
            Assert(limit == 0 || scale == 0);

            if(&limit == &simpleJitLimit)
            {TRACE_IT(34670);
                FunctionEntryPointInfo *const simpleJitEntryPointInfo = GetSimpleJitEntryPointInfo();
                if(GetDefaultFunctionEntryPointInfo() == simpleJitEntryPointInfo)
                {TRACE_IT(34671);
                    Assert(GetExecutionMode() == ExecutionMode::SimpleJit);
                    const int newSimpleJitCallCount = max(0, (int)simpleJitEntryPointInfo->callsCount + limitScale);
                    Assert(static_cast<int>(static_cast<uint16>(newSimpleJitCallCount)) == newSimpleJitCallCount);
                    SetSimpleJitCallCount(static_cast<uint16>(newSimpleJitCallCount));
                }
            }

            return scale == 0;
        };

        /*
        Determine which execution mode's limit scales with the full JIT threshold, in order of preference:
            - New simple JIT
            - Auto-profiling interpreter 1
            - Auto-profiling interpreter 0
            - Interpreter
            - Profiling interpreter 0 (when using old simple JIT)
            - Old simple JIT
            - Profiling interpreter 1
            - Profiling interpreter 0 (when using new simple JIT)
        */
        const bool doSimpleJit = DoSimpleJit();
        const bool doInterpreterProfile = DoInterpreterProfile();
        const bool fullyScaled =
            (CONFIG_FLAG(NewSimpleJit) && doSimpleJit && ScaleLimit(simpleJitLimit)) ||
            (
                doInterpreterProfile
                    ?   DoInterpreterAutoProfile() &&
                        (ScaleLimit(autoProfilingInterpreter1Limit) || ScaleLimit(autoProfilingInterpreter0Limit))
                    :   ScaleLimit(interpreterLimit)
            ) ||
            (
                CONFIG_FLAG(NewSimpleJit)
                    ?   doInterpreterProfile &&
                        (ScaleLimit(profilingInterpreter1Limit) || ScaleLimit(profilingInterpreter0Limit))
                    :   (doInterpreterProfile && ScaleLimit(profilingInterpreter0Limit)) ||
                        (doSimpleJit && ScaleLimit(simpleJitLimit)) ||
                        (doInterpreterProfile && ScaleLimit(profilingInterpreter1Limit))
            );
        Assert(fullyScaled);
        Assert(scale == 0);

        if(GetExecutionMode() != ExecutionMode::SimpleJit)
        {TRACE_IT(34672);
            Assert(IsInterpreterExecutionMode());
            if(simpleJitLimit != 0 &&
                (skipSimpleJit || simpleJitLimit < DEFAULT_CONFIG_MinSimpleJitIterations) &&
                !PHASE_FORCE(Phase::SimpleJitPhase, this))
            {TRACE_IT(34673);
                // Simple JIT code has not yet been generated, and was either requested to be skipped, or the limit was scaled
                // down too much. Skip simple JIT by moving any remaining iterations to an equivalent interpreter execution
                // mode.
                (CONFIG_FLAG(NewSimpleJit) ? autoProfilingInterpreter1Limit : profilingInterpreter1Limit) += simpleJitLimit;
                simpleJitLimit = 0;
                TryTransitionToNextInterpreterExecutionMode();
            }
        }

        VerifyExecutionModeLimits();
    }

    void FunctionBody::CommitExecutedIterations()
    {TRACE_IT(34674);
        Assert(initializedExecutionModeAndLimits);

        switch(GetExecutionMode())
        {
            case ExecutionMode::Interpreter:
                CommitExecutedIterations(interpreterLimit, GetInterpretedCount());
                break;

            case ExecutionMode::AutoProfilingInterpreter:
                CommitExecutedIterations(
                    autoProfilingInterpreter0Limit == 0 && profilingInterpreter0Limit == 0
                        ? autoProfilingInterpreter1Limit
                        : autoProfilingInterpreter0Limit,
                    GetInterpretedCount());
                break;

            case ExecutionMode::ProfilingInterpreter:
                CommitExecutedIterations(
                    GetSimpleJitEntryPointInfo()
                        ? profilingInterpreter1Limit
                        : profilingInterpreter0Limit,
                    GetInterpretedCount());
                break;

            case ExecutionMode::SimpleJit:
                CommitExecutedIterations(simpleJitLimit, GetSimpleJitExecutedIterations());
                break;

            case ExecutionMode::FullJit:
                break;

            default:
                Assert(false);
                __assume(false);
        }
    }

    void FunctionBody::CommitExecutedIterations(uint16 &limit, const uint executedIterations)
    {TRACE_IT(34675);
        Assert(initializedExecutionModeAndLimits);
        Assert(
            &limit == &interpreterLimit ||
            &limit == &autoProfilingInterpreter0Limit ||
            &limit == &profilingInterpreter0Limit ||
            &limit == &autoProfilingInterpreter1Limit ||
            &limit == &simpleJitLimit ||
            &limit == &profilingInterpreter1Limit);

        const uint16 clampedExecutedIterations = executedIterations >= limit ? limit : static_cast<uint16>(executedIterations);
        Assert(fullJitThreshold >= clampedExecutedIterations);
        fullJitThreshold -= clampedExecutedIterations;
        limit -= clampedExecutedIterations;
        VerifyExecutionModeLimits();

        if(&limit == &profilingInterpreter0Limit ||
            (!CONFIG_FLAG(NewSimpleJit) && &limit == &simpleJitLimit) ||
            &limit == &profilingInterpreter1Limit)
        {TRACE_IT(34676);
            const uint16 newCommittedProfiledIterations = committedProfiledIterations + clampedExecutedIterations;
            committedProfiledIterations =
                newCommittedProfiledIterations >= committedProfiledIterations ? newCommittedProfiledIterations : UINT16_MAX;
        }
    }

    uint16 FunctionBody::GetSimpleJitExecutedIterations() const
    {TRACE_IT(34677);
        Assert(initializedExecutionModeAndLimits);
        Assert(GetExecutionMode() == ExecutionMode::SimpleJit);

        FunctionEntryPointInfo *const simpleJitEntryPointInfo = GetSimpleJitEntryPointInfo();
        if(!simpleJitEntryPointInfo)
        {TRACE_IT(34678);
            return 0;
        }

        // Simple JIT counts down and transitions on overflow
        const uint32 callCount = simpleJitEntryPointInfo->callsCount;
        Assert(simpleJitLimit == 0 ? callCount == 0 : simpleJitLimit > callCount);
        return callCount == 0 ?
            static_cast<uint16>(simpleJitLimit) :
            static_cast<uint16>(simpleJitLimit) - static_cast<uint16>(callCount) - 1;
    }

    void FunctionBody::ResetSimpleJitLimitAndCallCount()
    {TRACE_IT(34679);
        Assert(initializedExecutionModeAndLimits);
        Assert(GetExecutionMode() == ExecutionMode::SimpleJit);
        Assert(GetDefaultFunctionEntryPointInfo() == GetSimpleJitEntryPointInfo());

        const uint16 simpleJitNewLimit = static_cast<uint8>(Configuration::Global.flags.SimpleJitLimit);
        Assert(simpleJitNewLimit == Configuration::Global.flags.SimpleJitLimit);
        if(simpleJitLimit < simpleJitNewLimit)
        {TRACE_IT(34680);
            fullJitThreshold += simpleJitNewLimit - simpleJitLimit;
            simpleJitLimit = simpleJitNewLimit;
        }

        SetInterpretedCount(0);
        ResetSimpleJitCallCount();
    }

    void FunctionBody::SetSimpleJitCallCount(const uint16 simpleJitLimit) const
    {TRACE_IT(34681);
        Assert(GetExecutionMode() == ExecutionMode::SimpleJit);
        Assert(GetDefaultFunctionEntryPointInfo() == GetSimpleJitEntryPointInfo());

        // Simple JIT counts down and transitions on overflow
        const uint8 limit = static_cast<uint8>(min(0xffui16, simpleJitLimit));
        GetSimpleJitEntryPointInfo()->callsCount = limit == 0 ? 0 : limit - 1;
    }

    void FunctionBody::ResetSimpleJitCallCount()
    {TRACE_IT(34682);
        uint32 interpretedCount = GetInterpretedCount();
        SetSimpleJitCallCount(
            simpleJitLimit > interpretedCount
                ? simpleJitLimit - static_cast<uint16>(interpretedCount)
                : 0ui16);
    }

    uint16 FunctionBody::GetProfiledIterations() const
    {TRACE_IT(34683);
        Assert(initializedExecutionModeAndLimits);

        uint16 profiledIterations = committedProfiledIterations;
        switch(GetExecutionMode())
        {
            case ExecutionMode::ProfilingInterpreter:
            {TRACE_IT(34684);
                uint32 interpretedCount = GetInterpretedCount();
                const uint16 clampedInterpretedCount =
                    interpretedCount <= UINT16_MAX
                        ? static_cast<uint16>(interpretedCount)
                        : UINT16_MAX;
                const uint16 newProfiledIterations = profiledIterations + clampedInterpretedCount;
                profiledIterations = newProfiledIterations >= profiledIterations ? newProfiledIterations : UINT16_MAX;
                break;
            }

            case ExecutionMode::SimpleJit:
                if(!CONFIG_FLAG(NewSimpleJit))
                {TRACE_IT(34685);
                    const uint16 newProfiledIterations = profiledIterations + GetSimpleJitExecutedIterations();
                    profiledIterations = newProfiledIterations >= profiledIterations ? newProfiledIterations : UINT16_MAX;
                }
                break;
        }
        return profiledIterations;
    }

    void FunctionBody::OnFullJitDequeued(const FunctionEntryPointInfo *const entryPointInfo)
    {TRACE_IT(34686);
        Assert(initializedExecutionModeAndLimits);
        Assert(GetExecutionMode() == ExecutionMode::FullJit);
        Assert(entryPointInfo);

        if(entryPointInfo != GetDefaultFunctionEntryPointInfo())
        {TRACE_IT(34687);
            return;
        }

        // Re-queue the full JIT work item after this many iterations
        fullJitRequeueThreshold = static_cast<uint16>(DEFAULT_CONFIG_FullJitRequeueThreshold);
    }

    void FunctionBody::TraceExecutionMode(const char *const eventDescription) const
    {TRACE_IT(34688);
        Assert(initializedExecutionModeAndLimits);

        if(PHASE_TRACE(Phase::ExecutionModePhase, this))
        {TRACE_IT(34689);
            DoTraceExecutionMode(eventDescription);
        }
    }

    void FunctionBody::TraceInterpreterExecutionMode() const
    {TRACE_IT(34690);
        Assert(initializedExecutionModeAndLimits);

        if(!PHASE_TRACE(Phase::ExecutionModePhase, this))
        {TRACE_IT(34691);
            return;
        }

        switch(GetExecutionMode())
        {
            case ExecutionMode::Interpreter:
            case ExecutionMode::AutoProfilingInterpreter:
            case ExecutionMode::ProfilingInterpreter:
                DoTraceExecutionMode(nullptr);
                break;
        }
    }

    void FunctionBody::DoTraceExecutionMode(const char *const eventDescription) const
    {TRACE_IT(34692);
        Assert(PHASE_TRACE(Phase::ExecutionModePhase, this));
        Assert(initializedExecutionModeAndLimits);

        char16 functionIdString[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        Output::Print(
            _u("ExecutionMode - ")
                _u("function: %s (%s), ")
                _u("mode: %S, ")
                _u("size: %u, ")
                _u("limits: %hu.%hu.%hu.%hu.%hu = %hu"),
            GetDisplayName(),
                GetDebugNumberSet(functionIdString),
            ExecutionModeName(executionMode),
            GetByteCodeCount(),
            interpreterLimit + autoProfilingInterpreter0Limit,
                profilingInterpreter0Limit,
                autoProfilingInterpreter1Limit,
                simpleJitLimit,
                profilingInterpreter1Limit,
                fullJitThreshold);

        if(eventDescription)
        {TRACE_IT(34693);
            Output::Print(_u(", event: %S"), eventDescription);
        }

        Output::Print(_u("\n"));
        Output::Flush();
    }

    bool FunctionBody::DoSimpleJit() const
    {TRACE_IT(34694);
        return
            !PHASE_OFF(Js::SimpleJitPhase, this) &&
            !GetScriptContext()->GetConfig()->IsNoNative() &&
            !GetScriptContext()->IsScriptContextInDebugMode() &&
            DoInterpreterProfile() &&
#pragma warning(suppress: 6235) // (<non-zero constant> || <expression>) is always a non-zero constant.
            (!CONFIG_FLAG(NewSimpleJit) || DoInterpreterAutoProfile()) &&
            !IsCoroutine(); // Generator JIT requires bailout which SimpleJit cannot do since it skips GlobOpt
    }

    bool FunctionBody::DoSimpleJitWithLock() const
    {TRACE_IT(34695);
        return
            !PHASE_OFF(Js::SimpleJitPhase, this) &&
            !GetScriptContext()->GetConfig()->IsNoNative() &&
            !this->IsInDebugMode() &&
            DoInterpreterProfileWithLock() &&
#pragma warning(suppress: 6235) // (<non-zero constant> || <expression>) is always a non-zero constant.
            (!CONFIG_FLAG(NewSimpleJit) || DoInterpreterAutoProfile()) &&
            !IsCoroutine(); // Generator JIT requires bailout which SimpleJit cannot do since it skips GlobOpt
    }

    bool FunctionBody::DoSimpleJitDynamicProfile() const
    {TRACE_IT(34696);
        Assert(DoSimpleJitWithLock());

        return !PHASE_OFF(Js::SimpleJitDynamicProfilePhase, this) && !CONFIG_FLAG(NewSimpleJit);
    }

    bool FunctionBody::DoInterpreterProfile() const
    {TRACE_IT(34697);
#if ENABLE_PROFILE_INFO
#ifdef ASMJS_PLAT
        // Switch off profiling is asmJsFunction
        if (this->GetIsAsmJsFunction() || this->GetAsmJsModuleInfo())
        {TRACE_IT(34698);
            return false;
        }
        else
#endif
        {TRACE_IT(34699);
            return !PHASE_OFF(InterpreterProfilePhase, this) && DynamicProfileInfo::IsEnabled(this);
        }
#else
        return false;
#endif
    }

    bool FunctionBody::DoInterpreterProfileWithLock() const
    {TRACE_IT(34700);
#if ENABLE_PROFILE_INFO
#ifdef ASMJS_PLAT
        // Switch off profiling is asmJsFunction
        if (this->GetIsAsmJsFunction() || this->GetAsmJsModuleInfoWithLock())
        {TRACE_IT(34701);
            return false;
        }
        else
#endif
        {TRACE_IT(34702);
            return !PHASE_OFF(InterpreterProfilePhase, this) && DynamicProfileInfo::IsEnabled(this);
        }
#else
        return false;
#endif
    }

    bool FunctionBody::DoInterpreterAutoProfile() const
    {TRACE_IT(34703);
        Assert(DoInterpreterProfile());

        return !PHASE_OFF(InterpreterAutoProfilePhase, this) && !this->IsInDebugMode();
    }

    bool FunctionBody::WasCalledFromLoop() const
    {TRACE_IT(34704);
        return wasCalledFromLoop;
    }

    void FunctionBody::SetWasCalledFromLoop()
    {TRACE_IT(34705);
        if(wasCalledFromLoop)
        {TRACE_IT(34706);
            return;
        }
        wasCalledFromLoop = true;

        if(Configuration::Global.flags.EnforceExecutionModeLimits)
        {TRACE_IT(34707);
            if(PHASE_TRACE(Phase::ExecutionModePhase, this))
            {TRACE_IT(34708);
                CommitExecutedIterations();
                TraceExecutionMode("WasCalledFromLoop (before)");
            }
        }
        else
        {TRACE_IT(34709);
            // This function is likely going to be called frequently since it's called from a loop. Reduce the full JIT
            // threshold to realize the full JIT perf benefit sooner.
            CommitExecutedIterations();
            TraceExecutionMode("WasCalledFromLoop (before)");
            if(fullJitThreshold > 1)
            {TRACE_IT(34710);
                SetFullJitThreshold(fullJitThreshold / 2, !CONFIG_FLAG(NewSimpleJit));
            }
        }

        {TRACE_IT(34711);
            // Reduce the loop interpreter limit too, for the same reasons as above
            const uint oldLoopInterpreterLimit = GetLoopInterpreterLimit();
            const uint newLoopInterpreterLimit = GetReducedLoopInterpretCount();
            Assert(newLoopInterpreterLimit <= oldLoopInterpreterLimit);
            SetLoopInterpreterLimit(newLoopInterpreterLimit);

            // Adjust loop headers' interpret counts to ensure that loops will still be profiled a number of times before
            // loop bodies are jitted
            const uint oldLoopProfileThreshold = GetLoopProfileThreshold(oldLoopInterpreterLimit);
            const uint newLoopProfileThreshold = GetLoopProfileThreshold(newLoopInterpreterLimit);
            MapLoopHeaders([=](const uint index, LoopHeader *const loopHeader)
            {
                const uint interpretedCount = loopHeader->interpretCount;
                if(interpretedCount <= newLoopProfileThreshold || interpretedCount >= oldLoopInterpreterLimit)
                {TRACE_IT(34712);
                    // The loop hasn't been profiled yet and wouldn't have started profiling even with the new profile
                    // threshold, or it has already been profiled the necessary minimum number of times based on the old limit
                    return;
                }

                if(interpretedCount <= oldLoopProfileThreshold)
                {TRACE_IT(34713);
                    // The loop hasn't been profiled yet, but would have started profiling with the new profile threshold. Start
                    // profiling on the next iteration.
                    loopHeader->interpretCount = newLoopProfileThreshold;
                    return;
                }

                // The loop has been profiled some already. Preserve the number of profiled iterations.
                loopHeader->interpretCount = newLoopProfileThreshold + (interpretedCount - oldLoopProfileThreshold);
            });
        }

        TraceExecutionMode("WasCalledFromLoop");
    }

    bool FunctionBody::RecentlyBailedOutOfJittedLoopBody() const
    {TRACE_IT(34714);
        return recentlyBailedOutOfJittedLoopBody;
    }

    void FunctionBody::SetRecentlyBailedOutOfJittedLoopBody(const bool value)
    {TRACE_IT(34715);
        recentlyBailedOutOfJittedLoopBody = value;
    }

    uint16 FunctionBody::GetMinProfileIterations()
    {TRACE_IT(34716);
        return
            static_cast<uint>(
                CONFIG_FLAG(NewSimpleJit)
                    ? DEFAULT_CONFIG_MinProfileIterations
                    : DEFAULT_CONFIG_MinProfileIterations_OldSimpleJit);
    }

    uint16 FunctionBody::GetMinFunctionProfileIterations()
    {TRACE_IT(34717);
        return GetMinProfileIterations();
    }

    uint FunctionBody::GetMinLoopProfileIterations(const uint loopInterpreterLimit)
    {TRACE_IT(34718);
        return min(static_cast<uint>(GetMinProfileIterations()), loopInterpreterLimit);
    }

    uint FunctionBody::GetLoopProfileThreshold(const uint loopInterpreterLimit) const
    {TRACE_IT(34719);
        return
            DoInterpreterProfile()
                ? DoInterpreterAutoProfile()
                    ? loopInterpreterLimit - GetMinLoopProfileIterations(loopInterpreterLimit)
                    : 0
                : static_cast<uint>(-1);
    }

    uint FunctionBody::GetReducedLoopInterpretCount()
    {TRACE_IT(34720);
        const uint loopInterpretCount = CONFIG_FLAG(LoopInterpretCount);
        if(CONFIG_ISENABLED(LoopInterpretCountFlag))
        {TRACE_IT(34721);
            return loopInterpretCount;
        }
        return max(loopInterpretCount / 3, GetMinLoopProfileIterations(loopInterpretCount));
    }

    uint FunctionBody::GetLoopInterpretCount(LoopHeader* loopHeader) const
    {TRACE_IT(34722);
        if(loopHeader->isNested)
        {TRACE_IT(34723);
            Assert(GetLoopInterpreterLimit() >= GetReducedLoopInterpretCount());
            return GetReducedLoopInterpretCount();
        }
        return GetLoopInterpreterLimit();
    }

    bool FunctionBody::DoObjectHeaderInlining()
    {TRACE_IT(34724);
        return !PHASE_OFF1(ObjectHeaderInliningPhase);
    }

    bool FunctionBody::DoObjectHeaderInliningForConstructors()
    {TRACE_IT(34725);
        return !PHASE_OFF1(ObjectHeaderInliningForConstructorsPhase) && DoObjectHeaderInlining();
    }

    bool FunctionBody::DoObjectHeaderInliningForConstructor(const uint32 inlineSlotCapacity)
    {TRACE_IT(34726);
        return inlineSlotCapacity == 0 ? DoObjectHeaderInliningForEmptyObjects() : DoObjectHeaderInliningForConstructors();
    }

    bool FunctionBody::DoObjectHeaderInliningForObjectLiterals()
    {TRACE_IT(34727);
        return !PHASE_OFF1(ObjectHeaderInliningForObjectLiteralsPhase) && DoObjectHeaderInlining();
    }

    bool FunctionBody::DoObjectHeaderInliningForObjectLiteral(const uint32 inlineSlotCapacity)
    {TRACE_IT(34728);
        return
            inlineSlotCapacity == 0
                ?   DoObjectHeaderInliningForEmptyObjects()
                :   DoObjectHeaderInliningForObjectLiterals() &&
                    inlineSlotCapacity <= static_cast<uint32>(MaxPreInitializedObjectHeaderInlinedTypeInlineSlotCount);
    }

    bool FunctionBody::DoObjectHeaderInliningForObjectLiteral(
        const PropertyIdArray *const propIds)
    {TRACE_IT(34729);
        Assert(propIds);

        return
            DoObjectHeaderInliningForObjectLiteral(propIds->count) &&
            PathTypeHandlerBase::UsePathTypeHandlerForObjectLiteral(propIds);
    }

    bool FunctionBody::DoObjectHeaderInliningForEmptyObjects()
    {TRACE_IT(34730);
        #pragma prefast(suppress:6237, "(<zero> && <expression>) is always zero. <expression> is never evaluated and might have side effects.")
        return PHASE_ON1(ObjectHeaderInliningForEmptyObjectsPhase) && DoObjectHeaderInlining();
    }

    void FunctionBody::Finalize(bool isShutdown)
    {TRACE_IT(34731);
#if ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.Instrument.IsEnabled(Js::LinearScanPhase, this->GetSourceContextId(), this->GetLocalFunctionId()))
        {TRACE_IT(34732);
            this->DumpRegStats(this);
        }
#endif
        this->Cleanup(isShutdown);
        this->CleanupSourceInfo(isShutdown);
        this->CleanupFunctionProxyCounters();
    }

    void FunctionBody::OnMark()
    {TRACE_IT(34733);
        this->m_hasActiveReference = true;
    }

    void FunctionBody::CleanupSourceInfo(bool isScriptContextClosing)
    {TRACE_IT(34734);
        Assert(this->cleanedUp);

        if (!sourceInfoCleanedUp)
        {TRACE_IT(34735);
            if (GetIsFuncRegistered() && !isScriptContextClosing)
            {TRACE_IT(34736);
                // If our function is registered, then there must
                // be a Utf8SourceInfo pinned by it.
                Assert(this->m_utf8SourceInfo);

                this->GetUtf8SourceInfo()->RemoveFunctionBody(this);
            }

            if (this->m_sourceInfo.pSpanSequence != nullptr)
            {TRACE_IT(34737);
                HeapDelete(this->m_sourceInfo.pSpanSequence);
                this->m_sourceInfo.pSpanSequence = nullptr;
            }

            sourceInfoCleanedUp = true;
        }
    }

    template<bool IsScriptContextShutdown>
    void FunctionBody::CleanUpInlineCaches()
    {TRACE_IT(34738);
        uint unregisteredInlineCacheCount = 0;

        if (nullptr != this->inlineCaches)
        {TRACE_IT(34739);
            // Inline caches are in this order
            //      plain inline cache
            //      root object load inline cache
            //      root object store inline cache
            //      isInst inline cache
            // The inlineCacheCount includes all but isInst inline cache

            uint i = 0;
            uint plainInlineCacheEnd = GetRootObjectLoadInlineCacheStart();
            for (; i < plainInlineCacheEnd; i++)
            {TRACE_IT(34740);
                if (nullptr != this->inlineCaches[i])
                {TRACE_IT(34741);
                    InlineCache* inlineCache = (InlineCache*)this->inlineCaches[i];
                    if (IsScriptContextShutdown)
                    {
                        memset(inlineCache, 0, sizeof(InlineCache));
                    }
                    else
                    {TRACE_IT(34742);
                        if (inlineCache->RemoveFromInvalidationList())
                        {TRACE_IT(34743);
                            unregisteredInlineCacheCount++;
                        }
                        AllocatorDelete(InlineCacheAllocator, this->m_scriptContext->GetInlineCacheAllocator(), inlineCache);
                    }
                }
            }

            RootObjectBase * rootObjectBase = this->GetRootObject();
            uint rootObjectLoadInlineCacheEnd = GetRootObjectLoadMethodInlineCacheStart();
            for (; i < rootObjectLoadInlineCacheEnd; i++)
            {TRACE_IT(34744);
                if (nullptr != this->inlineCaches[i])
                {TRACE_IT(34745);
                    InlineCache* inlineCache = (InlineCache*)this->inlineCaches[i];
                    if (IsScriptContextShutdown)
                    {
                        memset(inlineCache, 0, sizeof(InlineCache));
                    }
                    else
                    {TRACE_IT(34746);
                        // A single root object inline caches for a given property is shared by all functions.  It is ref counted
                        // and doesn't get released to the allocator until there are no more outstanding references.  Thus we don't need
                        // to (and, in fact, cannot) remove it from the invalidation list here.  Instead, we'll do it in ReleaseInlineCache
                        // when there are no more outstanding references.
                        rootObjectBase->ReleaseInlineCache(this->GetPropertyIdFromCacheId(i), false, false, IsScriptContextShutdown);
                    }
                }
            }

            uint rootObjectLoadMethodInlineCacheEnd = GetRootObjectStoreInlineCacheStart();
            for (; i < rootObjectLoadMethodInlineCacheEnd; i++)
            {TRACE_IT(34747);
                if (nullptr != this->inlineCaches[i])
                {TRACE_IT(34748);
                    InlineCache* inlineCache = (InlineCache*)this->inlineCaches[i];
                    if (IsScriptContextShutdown)
                    {
                        memset(inlineCache, 0, sizeof(InlineCache));
                    }
                    else
                    {TRACE_IT(34749);
                        // A single root object inline caches for a given property is shared by all functions.  It is ref counted
                        // and doesn't get released to the allocator until there are no more outstanding references.  Thus we don't need
                        // to (and, in fact, cannot) remove it from the invalidation list here.  Instead, we'll do it in ReleaseInlineCache
                        // when there are no more outstanding references.
                        rootObjectBase->ReleaseInlineCache(this->GetPropertyIdFromCacheId(i), true, false, IsScriptContextShutdown);
                    }
                }
            }

            uint rootObjectStoreInlineCacheEnd = this->GetInlineCacheCount();
            for (; i < rootObjectStoreInlineCacheEnd; i++)
            {TRACE_IT(34750);
                if (nullptr != this->inlineCaches[i])
                {TRACE_IT(34751);
                    InlineCache* inlineCache = (InlineCache*)this->inlineCaches[i];
                    if (IsScriptContextShutdown)
                    {
                        memset(inlineCache, 0, sizeof(InlineCache));
                    }
                    else
                    {TRACE_IT(34752);
                        // A single root object inline caches for a given property is shared by all functions.  It is ref counted
                        // and doesn't get released to the allocator until there are no more outstanding references.  Thus we don't need
                        // to (and, in fact, cannot) remove it from the invalidation list here.  Instead, we'll do it in ReleaseInlineCache
                        // when there are no more outstanding references.
                        rootObjectBase->ReleaseInlineCache(this->GetPropertyIdFromCacheId(i), false, true, IsScriptContextShutdown);
                    }
                }
            }

            uint totalCacheCount = GetInlineCacheCount() + GetIsInstInlineCacheCount();
            for (; i < totalCacheCount; i++)
            {TRACE_IT(34753);
                if (nullptr != this->inlineCaches[i])
                {TRACE_IT(34754);
                    IsInstInlineCache* inlineCache = (IsInstInlineCache*)this->inlineCaches[i];
                    if (IsScriptContextShutdown)
                    {
                        memset(inlineCache, 0, sizeof(IsInstInlineCache));
                    }
                    else
                    {TRACE_IT(34755);
                        inlineCache->Unregister(this->m_scriptContext);
                        AllocatorDelete(CacheAllocator, this->m_scriptContext->GetIsInstInlineCacheAllocator(), inlineCache);
                    }
                }
            }

            this->inlineCaches = nullptr;

        }

        auto codeGenRuntimeData = this->GetCodeGenRuntimeData();
        if (nullptr != codeGenRuntimeData)
        {TRACE_IT(34756);
            for (ProfileId i = 0; i < this->profiledCallSiteCount; i++)
            {TRACE_IT(34757);
                const FunctionCodeGenRuntimeData* runtimeData = codeGenRuntimeData[i];
                if (nullptr != runtimeData)
                {TRACE_IT(34758);
                    runtimeData->MapInlineCaches([&](InlineCache* inlineCache)
                    {
                        if (nullptr != inlineCache)
                        {TRACE_IT(34759);
                            if (IsScriptContextShutdown)
                            {
                                memset(inlineCache, 0, sizeof(InlineCache));
                            }
                            else
                            {TRACE_IT(34760);
                                if (inlineCache->RemoveFromInvalidationList())
                                {TRACE_IT(34761);
                                    unregisteredInlineCacheCount++;
                                }
                                AllocatorDelete(InlineCacheAllocator, this->m_scriptContext->GetInlineCacheAllocator(), inlineCache);
                            }
                        }
                    });
                }
            }
        }

        auto codeGenGetSetRuntimeData = this->GetCodeGenGetSetRuntimeData();
        if (codeGenGetSetRuntimeData != nullptr)
        {TRACE_IT(34762);
            for (uint i = 0; i < this->GetInlineCacheCount(); i++)
            {TRACE_IT(34763);
                auto runtimeData = codeGenGetSetRuntimeData[i];
                if (nullptr != runtimeData)
                {TRACE_IT(34764);
                    runtimeData->MapInlineCaches([&](InlineCache* inlineCache)
                    {
                        if (nullptr != inlineCache)
                        {TRACE_IT(34765);
                            if (IsScriptContextShutdown)
                            {
                                memset(inlineCache, 0, sizeof(InlineCache));
                            }
                            else
                            {TRACE_IT(34766);
                                if (inlineCache->RemoveFromInvalidationList())
                                {TRACE_IT(34767);
                                    unregisteredInlineCacheCount++;
                                }
                                AllocatorDelete(InlineCacheAllocator, this->m_scriptContext->GetInlineCacheAllocator(), inlineCache);
                            }
                        }
                    });
                }
            }
        }

        if (unregisteredInlineCacheCount > 0)
        {
            AssertMsg(!IsScriptContextShutdown, "Unregistration of inlineCache should only be done if this is not scriptContext shutdown.");
            ThreadContext* threadContext = this->m_scriptContext->GetThreadContext();
            threadContext->NotifyInlineCacheBatchUnregistered(unregisteredInlineCacheCount);
        }

        while (this->GetPolymorphicInlineCachesHead())
        {TRACE_IT(34768);
            this->GetPolymorphicInlineCachesHead()->Finalize(IsScriptContextShutdown);
        }
        polymorphicInlineCaches.Reset();
    }

    void FunctionBody::CleanupRecyclerData(bool isShutdown, bool doEntryPointCleanupCaptureStack)
    {TRACE_IT(34769);
        // If we're not shutting down (i.e closing the script context), we need to remove our inline caches from
        // thread context's invalidation lists, and release memory back to the arena.  During script context shutdown,
        // we leave everything in place, because the inline cache arena will stay alive until script context is destroyed
        // (i.e it's destructor has been called) and thus the invalidation lists are safe to keep references to caches from this
        // script context.  We will, however, zero all inline caches so that we don't have to process them on subsequent
        // collections, which may still happen from other script contexts.

        if (isShutdown)
        {TRACE_IT(34770);
            CleanUpInlineCaches<true>();
        }
        else
        {TRACE_IT(34771);
            CleanUpInlineCaches<false>();
        }

        if (this->entryPoints)
        {TRACE_IT(34772);
#if defined(ENABLE_DEBUG_CONFIG_OPTIONS) && !(DBG)
            // On fretest builds, capture the stack only if the FreTestDiagMode switch is on
            doEntryPointCleanupCaptureStack = doEntryPointCleanupCaptureStack && Js::Configuration::Global.flags.FreTestDiagMode;
#endif

            this->MapEntryPoints([=](int index, FunctionEntryPointInfo* entryPoint)
            {
                if (nullptr != entryPoint)
                {TRACE_IT(34773);
                    // Finalize = Free up work item if it hasn't been released yet + entry point clean up
                    // isShutdown is false because cleanup is called only in the !isShutdown case
                    entryPoint->Finalize(isShutdown);

#if ENABLE_DEBUG_STACK_BACK_TRACE
                    // Do this separately since calling EntryPoint::Finalize doesn't capture the stack trace
                    // and in some calls to CleanupRecyclerData, we do want the stack trace captured.

                    if (doEntryPointCleanupCaptureStack)
                    {TRACE_IT(34774);
                        entryPoint->CaptureCleanupStackTrace();
                    }
#endif
                }
            });

            this->MapLoopHeaders([=](uint loopNumber, LoopHeader* header)
            {
                bool shuttingDown = isShutdown;
                header->MapEntryPoints([=](int index, LoopEntryPointInfo* entryPoint)
                {
                    entryPoint->Cleanup(shuttingDown, doEntryPointCleanupCaptureStack);
                });
            });
        }

#ifdef PERF_COUNTERS
        this->CleanupPerfCounter();
#endif
    }

    //
    // Removes all references of the function body and causes clean up of entry points.
    // If the cleanup has already occurred before this would be a no-op.
    //
    void FunctionBody::Cleanup(bool isScriptContextClosing)
    {TRACE_IT(34775);
        if (cleanedUp)
        {TRACE_IT(34776);
            return;
        }
#if DBG
        this->counters.isCleaningUp = true;
#endif

        CleanupRecyclerData(isScriptContextClosing, false /* capture entry point cleanup stack trace */);
        CleanUpForInCache(isScriptContextClosing);

        this->ResetObjectLiteralTypes();

        // Manually clear these values to break any circular references
        // that might prevent the script context from being disposed
        this->SetAuxiliaryData(nullptr);
        this->SetAuxiliaryContextData(nullptr);
        this->byteCodeBlock = nullptr;
        this->entryPoints = nullptr;
        this->SetLoopHeaderArray(nullptr);
        this->SetConstTable(nullptr);
        this->SetCodeGenRuntimeData(nullptr);
        this->SetCodeGenGetSetRuntimeData(nullptr);
        this->SetPropertyIdOnRegSlotsContainer(nullptr);
        this->inlineCaches = nullptr;
        this->polymorphicInlineCaches.Reset();
        this->SetPolymorphicInlineCachesHead(nullptr);
        this->cacheIdToPropertyIdMap = nullptr;
        this->SetFormalsPropIdArray(nullptr);
        this->SetReferencedPropertyIdMap(nullptr);
        this->SetLiteralRegexs(nullptr);
        this->SetPropertyIdsForScopeSlotArray(nullptr, 0);

#if DYNAMIC_INTERPRETER_THUNK
        if (this->HasInterpreterThunkGenerated())
        {TRACE_IT(34777);
            JS_ETW(EtwTrace::LogMethodInterpreterThunkUnloadEvent(this));

            if (!isScriptContextClosing)
            {TRACE_IT(34778);
                if (m_isAsmJsFunction)
                {TRACE_IT(34779);
                    m_scriptContext->ReleaseDynamicAsmJsInterpreterThunk((BYTE*)this->m_dynamicInterpreterThunk, /*addtoFreeList*/!isScriptContextClosing);
                }
                else
                {TRACE_IT(34780);
                    m_scriptContext->ReleaseDynamicInterpreterThunk((BYTE*)this->m_dynamicInterpreterThunk, /*addtoFreeList*/!isScriptContextClosing);
                }
            }
        }
#endif

#if ENABLE_PROFILE_INFO
        this->SetPolymorphicCallSiteInfoHead(nullptr);
#endif

        this->cleanedUp = true;
    }


#ifdef PERF_COUNTERS
    void FunctionBody::CleanupPerfCounter()
    {TRACE_IT(34781);
        // We might not have the byte code block yet if we defer parsed.
        DWORD byteCodeSize = (this->byteCodeBlock? this->byteCodeBlock->GetLength() : 0)
            + (this->GetAuxiliaryData() ? this->GetAuxiliaryData()->GetLength() : 0)
            + (this->GetAuxiliaryContextData() ? this->GetAuxiliaryContextData()->GetLength() : 0);
        PERF_COUNTER_SUB(Code, DynamicByteCodeSize, byteCodeSize);

        if (this->m_isDeserializedFunction)
        {
            PERF_COUNTER_DEC(Code, DeserializedFunctionBody);
        }

        PERF_COUNTER_SUB(Code, TotalByteCodeSize, byteCodeSize);
    }
#endif

    void FunctionBody::CaptureDynamicProfileState(FunctionEntryPointInfo* entryPointInfo)
    {TRACE_IT(34782);
        // DisableJIT-TODO: Move this to be under if DYNAMIC_PROFILE
#if ENABLE_NATIVE_CODEGEN
        // (See also the FunctionBody member written in CaptureDynamicProfileState.)
        this->SetSavedPolymorphicCacheState(entryPointInfo->GetPendingPolymorphicCacheState());
        this->savedInlinerVersion = entryPointInfo->GetPendingInlinerVersion();
        this->savedImplicitCallsFlags = entryPointInfo->GetPendingImplicitCallFlags();
#endif
    }

#if ENABLE_NATIVE_CODEGEN
    BYTE FunctionBody::GetSavedInlinerVersion() const
    {TRACE_IT(34783);
        Assert(this->dynamicProfileInfo != nullptr);
        return this->savedInlinerVersion;
    }

    uint32 FunctionBody::GetSavedPolymorphicCacheState() const
    {TRACE_IT(34784);
        Assert(this->dynamicProfileInfo != nullptr);
        return this->savedPolymorphicCacheState;
    }
    void FunctionBody::SetSavedPolymorphicCacheState(uint32 state)
    {TRACE_IT(34785);
        this->savedPolymorphicCacheState = state;
    }
#endif

    void FunctionBody::SetHasHotLoop()
    {TRACE_IT(34786);
        if(hasHotLoop)
        {TRACE_IT(34787);
            return;
        }
        hasHotLoop = true;

        if(Configuration::Global.flags.EnforceExecutionModeLimits)
        {TRACE_IT(34788);
            return;
        }

        CommitExecutedIterations();
        TraceExecutionMode("HasHotLoop (before)");
        if(fullJitThreshold > 1)
        {
            SetFullJitThreshold(1, true);
        }
        TraceExecutionMode("HasHotLoop");
    }

    bool FunctionBody::IsInlineApplyDisabled()
    {TRACE_IT(34789);
        return this->disableInlineApply;
    }

    void FunctionBody::SetDisableInlineApply(bool set)
    {TRACE_IT(34790);
        this->disableInlineApply = set;
    }

    void FunctionBody::InitDisableInlineApply()
    {TRACE_IT(34791);
        SetDisableInlineApply(
            (this->GetLocalFunctionId() != Js::Constants::NoFunctionId && PHASE_OFF(Js::InlinePhase, this)) ||
            PHASE_OFF(Js::InlineApplyPhase, this));
    }

    bool FunctionBody::CheckCalleeContextForInlining(FunctionProxy* calleeFunctionProxy)
    {TRACE_IT(34792);
        if (this->GetScriptContext() == calleeFunctionProxy->GetScriptContext())
        {TRACE_IT(34793);
            if (this->GetHostSourceContext() == calleeFunctionProxy->GetHostSourceContext() &&
                this->GetSecondaryHostSourceContext() == calleeFunctionProxy->GetSecondaryHostSourceContext())
            {TRACE_IT(34794);
                return true;
            }
        }
        return false;
    }

#if ENABLE_NATIVE_CODEGEN
    ImplicitCallFlags FunctionBody::GetSavedImplicitCallsFlags() const
    {TRACE_IT(34795);
        Assert(this->dynamicProfileInfo != nullptr);
        return this->savedImplicitCallsFlags;
    }

    bool FunctionBody::HasNonBuiltInCallee()
    {TRACE_IT(34796);
        for (ProfileId i = 0; i < profiledCallSiteCount; i++)
        {TRACE_IT(34797);
            Assert(HasDynamicProfileInfo());
            bool ctor;
            bool isPolymorphic;
            FunctionInfo *info = dynamicProfileInfo->GetCallSiteInfo(this, i, &ctor, &isPolymorphic);
            if (info == nullptr || info->HasBody())
            {TRACE_IT(34798);
                return true;
            }
        }
        return false;
    }
#endif

    void FunctionBody::CheckAndRegisterFuncToDiag(ScriptContext *scriptContext)
    {TRACE_IT(34799);
        // We will register function if, this is not host managed and it was not registered before.
        if (GetHostSourceContext() == Js::Constants::NoHostSourceContext
            && !m_isFuncRegisteredToDiag
            && !scriptContext->GetDebugContext()->GetProbeContainer()->IsContextRegistered(GetSecondaryHostSourceContext()))
        {TRACE_IT(34800);
            FunctionBody *pFunc = scriptContext->GetDebugContext()->GetProbeContainer()->GetGlobalFunc(scriptContext, GetSecondaryHostSourceContext());
            if (pFunc)
            {TRACE_IT(34801);
                // Existing behavior here is to ignore the OOM and since RegisterFuncToDiag
                // can throw now, we simply ignore the OOM here
                try
                {TRACE_IT(34802);
                    // Register the function to the PDM as eval code (the debugger app will show file as 'eval code')
                    pFunc->RegisterFuncToDiag(scriptContext, Constants::EvalCode);
                }
                catch (Js::OutOfMemoryException)
                {TRACE_IT(34803);
                }

                scriptContext->GetDebugContext()->GetProbeContainer()->RegisterContextToDiag(GetSecondaryHostSourceContext(), scriptContext->AllocatorForDiagnostics());

                m_isFuncRegisteredToDiag = true;
            }
        }
        else
        {TRACE_IT(34804);
            m_isFuncRegisteredToDiag = true;
        }

    }

    DebuggerScope* FunctionBody::RecordStartScopeObject(DiagExtraScopesType scopeType, int start, RegSlot scopeLocation, int* index)
    {TRACE_IT(34805);
        Recycler* recycler = m_scriptContext->GetRecycler();

        if (!GetScopeObjectChain())
        {
            SetScopeObjectChain(RecyclerNew(recycler, ScopeObjectChain, recycler));
        }

        // Check if we need to create the scope object or if it already exists from a previous bytecode
        // generator pass.
        DebuggerScope* debuggerScope = nullptr;
        int currentDebuggerScopeIndex = this->GetNextDebuggerScopeIndex();
        if (!this->TryGetDebuggerScopeAt(currentDebuggerScopeIndex, debuggerScope))
        {TRACE_IT(34806);
            // Create a new debugger scope.
            debuggerScope = AddScopeObject(scopeType, start, scopeLocation);
        }
        else
        {TRACE_IT(34807);
            debuggerScope->UpdateDueToByteCodeRegeneration(scopeType, start, scopeLocation);
        }

        if(index)
        {TRACE_IT(34808);
            *index = currentDebuggerScopeIndex;
        }

        return debuggerScope;
    }

    void FunctionBody::RecordEndScopeObject(DebuggerScope* currentScope, int end)
    {
        AssertMsg(currentScope, "No current debugger scope passed in.");
        currentScope->SetEnd(end);
    }

    DebuggerScope * FunctionBody::AddScopeObject(DiagExtraScopesType scopeType, int start, RegSlot scopeLocation)
    {TRACE_IT(34809);
        Assert(GetScopeObjectChain());

        DebuggerScope *scopeObject = RecyclerNew(m_scriptContext->GetRecycler(), DebuggerScope, m_scriptContext->GetRecycler(), scopeType, scopeLocation, start);
        GetScopeObjectChain()->pScopeChain->Add(scopeObject);

        return scopeObject;
    }

    // Tries to retrieve the debugger scope at the specified index.  If the index is out of range, nullptr
    // is returned.
    bool FunctionBody::TryGetDebuggerScopeAt(int index, DebuggerScope*& debuggerScope)
    {TRACE_IT(34810);
        AssertMsg(this->GetScopeObjectChain(), "TryGetDebuggerScopeAt should only be called with a valid scope chain in place.");
        Assert(index >= 0);

        const Js::ScopeObjectChain::ScopeObjectChainList* scopeChain = this->GetScopeObjectChain()->pScopeChain;
        if (index < scopeChain->Count())
        {TRACE_IT(34811);
            debuggerScope = scopeChain->Item(index);
            return true;
        }

        return false;
    }

#if DYNAMIC_INTERPRETER_THUNK
    DWORD FunctionBody::GetDynamicInterpreterThunkSize() const
    {TRACE_IT(34812);
        return InterpreterThunkEmitter::ThunkSize;
    }
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void
    FunctionBody::DumpFullFunctionName()
    {TRACE_IT(34813);
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

        Output::Print(_u("Function %s (%s)"), this->GetDisplayName(), this->GetDebugNumberSet(debugStringBuffer));
    }

    void FunctionBody::DumpFunctionId(bool pad)
    {TRACE_IT(34814);
        uint sourceContextId = this->GetSourceContextInfo()->sourceContextId;
        if (sourceContextId == Js::Constants::NoSourceContext)
        {TRACE_IT(34815);
            if (this->IsDynamicScript())
            {TRACE_IT(34816);
                Output::Print(pad? _u("Dy.%-3d") : _u("Dyn#%d"), this->GetLocalFunctionId());
            }
            else
            {TRACE_IT(34817);
                // Function from LoadFile
                Output::Print(pad? _u("%-5d") : _u("#%d"), this->GetLocalFunctionId());
            }
        }
        else
        {TRACE_IT(34818);
            Output::Print(pad? _u("%2d.%-3d") : _u("#%d.%d"), sourceContextId, this->GetLocalFunctionId());
        }
    }

#endif

    void FunctionBody::EnsureAuxStatementData()
    {TRACE_IT(34819);
        if (m_sourceInfo.m_auxStatementData == nullptr)
        {TRACE_IT(34820);
            Recycler* recycler = m_scriptContext->GetRecycler();

            // Note: allocating must be consistent with clean up in CleanupToReparse.
            m_sourceInfo.m_auxStatementData = RecyclerNew(recycler, AuxStatementData);
        }
    }

    /*static*/
    void FunctionBody::GetShortNameFromUrl(__in LPCWSTR pchUrl, _Out_writes_z_(cchBuffer) LPWSTR pchShortName, __in size_t cchBuffer)
    {TRACE_IT(34821);
        LPCWSTR pchFile = wcsrchr(pchUrl, _u('/'));
        if (pchFile == nullptr)
        {TRACE_IT(34822);
            pchFile = wcsrchr(pchUrl, _u('\\'));
        }

        LPCWSTR pchToCopy = pchUrl;

        if (pchFile != nullptr)
        {TRACE_IT(34823);
            pchToCopy = pchFile + 1;
        }

        wcscpy_s(pchShortName, cchBuffer, pchToCopy);
    }

    FunctionBody::StatementAdjustmentRecordList* FunctionBody::GetStatementAdjustmentRecords()
    {TRACE_IT(34824);
        if (m_sourceInfo.m_auxStatementData)
        {TRACE_IT(34825);
            return m_sourceInfo.m_auxStatementData->m_statementAdjustmentRecords;
        }
        return nullptr;
    }

    FunctionBody::CrossFrameEntryExitRecordList* FunctionBody::GetCrossFrameEntryExitRecords()
    {TRACE_IT(34826);
        if (m_sourceInfo.m_auxStatementData)
        {TRACE_IT(34827);
            return m_sourceInfo.m_auxStatementData->m_crossFrameBlockEntryExisRecords;
        }
        return nullptr;
    }

    void FunctionBody::RecordCrossFrameEntryExitRecord(uint byteCodeOffset, bool isEnterBlock)
    {TRACE_IT(34828);
        this->EnsureAuxStatementData();

        Recycler* recycler = this->m_scriptContext->GetRecycler();
        if (this->GetCrossFrameEntryExitRecords() == nullptr)
        {TRACE_IT(34829);
            m_sourceInfo.m_auxStatementData->m_crossFrameBlockEntryExisRecords = RecyclerNew(recycler, CrossFrameEntryExitRecordList, recycler);
        }
        Assert(this->GetCrossFrameEntryExitRecords());

        CrossFrameEntryExitRecord record(byteCodeOffset, isEnterBlock);
        this->GetCrossFrameEntryExitRecords()->Add(record); // Will copy stack value and put the copy into the container.
    }

    FunctionBody::AuxStatementData::AuxStatementData() : m_statementAdjustmentRecords(nullptr), m_crossFrameBlockEntryExisRecords(nullptr)
    {TRACE_IT(34830);
    }

    FunctionBody::StatementAdjustmentRecord::StatementAdjustmentRecord() :
        m_byteCodeOffset((uint)Constants::InvalidOffset), m_adjustmentType(SAT_None)
    {TRACE_IT(34831);
    }

    FunctionBody::StatementAdjustmentRecord::StatementAdjustmentRecord(StatementAdjustmentType type, int byteCodeOffset) :
        m_adjustmentType(type), m_byteCodeOffset(byteCodeOffset)
    {TRACE_IT(34832);
        Assert(SAT_None <= type && type <= SAT_All);
    }

    FunctionBody::StatementAdjustmentRecord::StatementAdjustmentRecord(const StatementAdjustmentRecord& other) :
        m_byteCodeOffset(other.m_byteCodeOffset), m_adjustmentType(other.m_adjustmentType)
    {TRACE_IT(34833);
    }

    uint FunctionBody::StatementAdjustmentRecord::GetByteCodeOffset()
    {TRACE_IT(34834);
        Assert(m_byteCodeOffset != Constants::InvalidOffset);
        return m_byteCodeOffset;
    }

    FunctionBody::StatementAdjustmentType FunctionBody::StatementAdjustmentRecord::GetAdjustmentType()
    {TRACE_IT(34835);
        Assert(this->m_adjustmentType != SAT_None);
        return m_adjustmentType;
    }

    FunctionBody::CrossFrameEntryExitRecord::CrossFrameEntryExitRecord() :
        m_byteCodeOffset((uint)Constants::InvalidOffset), m_isEnterBlock(false)
    {TRACE_IT(34836);
    }

    FunctionBody::CrossFrameEntryExitRecord::CrossFrameEntryExitRecord(uint byteCodeOffset, bool isEnterBlock) :
        m_byteCodeOffset(byteCodeOffset), m_isEnterBlock(isEnterBlock)
    {TRACE_IT(34837);
    }

    FunctionBody::CrossFrameEntryExitRecord::CrossFrameEntryExitRecord(const CrossFrameEntryExitRecord& other) :
        m_byteCodeOffset(other.m_byteCodeOffset), m_isEnterBlock(other.m_isEnterBlock)
    {TRACE_IT(34838);
    }

    uint FunctionBody::CrossFrameEntryExitRecord::GetByteCodeOffset() const
    {TRACE_IT(34839);
        Assert(m_byteCodeOffset != Constants::InvalidOffset);
        return m_byteCodeOffset;
    }

    bool FunctionBody::CrossFrameEntryExitRecord::GetIsEnterBlock()
    {TRACE_IT(34840);
        return m_isEnterBlock;
    }

    EntryPointPolymorphicInlineCacheInfo::EntryPointPolymorphicInlineCacheInfo(FunctionBody * functionBody) :
        selfInfo(functionBody),
        inlineeInfo(functionBody->GetRecycler())
    {TRACE_IT(34841);
    }

    PolymorphicInlineCacheInfo * EntryPointPolymorphicInlineCacheInfo::GetInlineeInfo(FunctionBody * inlineeFunctionBody)
    {
        SListCounted<PolymorphicInlineCacheInfo*, Recycler>::Iterator iter(&inlineeInfo);
        while (iter.Next())
        {TRACE_IT(34842);
            PolymorphicInlineCacheInfo * info = iter.Data();
            if (info->GetFunctionBody() == inlineeFunctionBody)
            {TRACE_IT(34843);
                return info;
            }
        }

        return nullptr;
    }

    PolymorphicInlineCacheInfo * EntryPointPolymorphicInlineCacheInfo::EnsureInlineeInfo(Recycler * recycler, FunctionBody * inlineeFunctionBody)
    {TRACE_IT(34844);
        PolymorphicInlineCacheInfo * info = GetInlineeInfo(inlineeFunctionBody);
        if (!info)
        {TRACE_IT(34845);
            info = RecyclerNew(recycler, PolymorphicInlineCacheInfo, inlineeFunctionBody);
            inlineeInfo.Prepend(info);
        }
        return info;
    }

    void EntryPointPolymorphicInlineCacheInfo::SetPolymorphicInlineCache(FunctionBody * functionBody, uint index, PolymorphicInlineCache * polymorphicInlineCache, bool isInlinee, byte polyCacheUtil)
    {TRACE_IT(34846);
        if (!isInlinee)
        {
            SetPolymorphicInlineCache(&selfInfo, functionBody, index, polymorphicInlineCache, polyCacheUtil);
            Assert(functionBody == selfInfo.GetFunctionBody());
        }
        else
        {TRACE_IT(34847);
            SetPolymorphicInlineCache(EnsureInlineeInfo(functionBody->GetScriptContext()->GetRecycler(), functionBody), functionBody, index, polymorphicInlineCache, polyCacheUtil);
            Assert(functionBody == GetInlineeInfo(functionBody)->GetFunctionBody());
        }
    }

    void EntryPointPolymorphicInlineCacheInfo::SetPolymorphicInlineCache(PolymorphicInlineCacheInfo * polymorphicInlineCacheInfo, FunctionBody * functionBody, uint index, PolymorphicInlineCache * polymorphicInlineCache, byte polyCacheUtil)
    {TRACE_IT(34848);
        polymorphicInlineCacheInfo->GetPolymorphicInlineCaches()->SetInlineCache(functionBody->GetScriptContext()->GetRecycler(), functionBody, index, polymorphicInlineCache);
        polymorphicInlineCacheInfo->GetUtilArray()->SetUtil(functionBody, index, polyCacheUtil);
    }

    void PolymorphicCacheUtilizationArray::SetUtil(Js::FunctionBody* functionBody, uint index, byte util)
    {TRACE_IT(34849);
        Assert(functionBody);
        Assert(index < functionBody->GetInlineCacheCount());

        EnsureUtilArray(functionBody->GetScriptContext()->GetRecycler(), functionBody);
        this->utilArray[index] = util;
    }

    byte PolymorphicCacheUtilizationArray::GetUtil(Js::FunctionBody* functionBody, uint index)
    {TRACE_IT(34850);
        Assert(index < functionBody->GetInlineCacheCount());
        return this->utilArray[index];
    }

    void PolymorphicCacheUtilizationArray::EnsureUtilArray(Recycler * const recycler, Js::FunctionBody * functionBody)
    {TRACE_IT(34851);
        Assert(recycler);
        Assert(functionBody);
        Assert(functionBody->GetInlineCacheCount() != 0);

        if(this->utilArray)
        {TRACE_IT(34852);
            return;
        }

        this->utilArray = RecyclerNewArrayLeafZ(recycler, byte, functionBody->GetInlineCacheCount());
    }

#if ENABLE_NATIVE_CODEGEN
    void EntryPointInfo::AddWeakFuncRef(RecyclerWeakReference<FunctionBody> *weakFuncRef, Recycler *recycler)
    {TRACE_IT(34853);
        Assert(this->state == CodeGenPending);

        this->weakFuncRefSet = this->EnsureWeakFuncRefSet(recycler);
        this->weakFuncRefSet->AddNew(weakFuncRef);
    }

    EntryPointInfo::WeakFuncRefSet *
    EntryPointInfo::EnsureWeakFuncRefSet(Recycler *recycler)
    {TRACE_IT(34854);
        if (this->weakFuncRefSet == nullptr)
        {TRACE_IT(34855);
            this->weakFuncRefSet = RecyclerNew(recycler, WeakFuncRefSet, recycler);
        }

        return this->weakFuncRefSet;
    }

    void EntryPointInfo::EnsureIsReadyToCall()
    {TRACE_IT(34856);
        ProcessJitTransferData();

#if !FLOATVAR
        if (this->numberPageSegments)
        {TRACE_IT(34857);
            auto numberArray = this->GetScriptContext()->GetThreadContext()
                ->GetXProcNumberPageSegmentManager()->RegisterSegments(this->numberPageSegments);
            this->SetNumberArray(numberArray);
            this->numberPageSegments = nullptr;
        }
#endif
    }

    void EntryPointInfo::ProcessJitTransferData()
    {TRACE_IT(34858);
        Assert(!IsCleanedUp());

        auto jitTransferData = GetJitTransferData();
        if (jitTransferData == nullptr)
        {TRACE_IT(34859);
            return;
        }

        class AutoCleanup
        {
            EntryPointInfo *entryPointInfo;
        public:
            AutoCleanup(EntryPointInfo *entryPointInfo) : entryPointInfo(entryPointInfo)
            {TRACE_IT(34860);
            }

            void Done()
            {TRACE_IT(34861);
                entryPointInfo = nullptr;
            }
            ~AutoCleanup()
            {TRACE_IT(34862);
                if (entryPointInfo)
                {TRACE_IT(34863);
                    entryPointInfo->OnNativeCodeInstallFailure();
                }
            }
        } autoCleanup(this);


        ScriptContext* scriptContext = GetScriptContext();

        if (jitTransferData->GetIsReady())
        {TRACE_IT(34864);
            PinTypeRefs(scriptContext);
            InstallGuards(scriptContext);
            FreeJitTransferData();
        }

        autoCleanup.Done();
    }

    EntryPointInfo::JitTransferData* EntryPointInfo::EnsureJitTransferData(Recycler* recycler)
    {TRACE_IT(34865);
        if (this->jitTransferData == nullptr)
        {TRACE_IT(34866);
            this->jitTransferData = RecyclerNew(recycler, EntryPointInfo::JitTransferData);
        }
        return this->jitTransferData;
    }

    void EntryPointInfo::OnNativeCodeInstallFailure()
    {TRACE_IT(34867);
        // If more data is transferred from the background thread to the main thread in ProcessJitTransferData,
        // corresponding fields on the entryPointInfo should be rolled back here.
        this->runtimeTypeRefs = nullptr;
        this->FreePropertyGuards();
        this->equivalentTypeCacheCount = 0;
        this->equivalentTypeCaches = nullptr;
        this->UnregisterEquivalentTypeCaches();

        this->ResetOnNativeCodeInstallFailure();
    }

#ifdef FIELD_ACCESS_STATS
    FieldAccessStats* EntryPointInfo::EnsureFieldAccessStats(Recycler* recycler)
    {TRACE_IT(34868);
        if (this->fieldAccessStats == nullptr)
        {TRACE_IT(34869);
            this->fieldAccessStats = RecyclerNew(recycler, FieldAccessStats);
        }
        return this->fieldAccessStats;
    }
#endif

    void EntryPointInfo::JitTransferData::AddJitTimeTypeRef(void* typeRef, Recycler* recycler)
    {TRACE_IT(34870);
        Assert(typeRef != nullptr);
        EnsureJitTimeTypeRefs(recycler);
        this->jitTimeTypeRefs->AddNew(typeRef);
    }

    void EntryPointInfo::JitTransferData::EnsureJitTimeTypeRefs(Recycler* recycler)
    {TRACE_IT(34871);
        if (this->jitTimeTypeRefs == nullptr)
        {TRACE_IT(34872);
            this->jitTimeTypeRefs = RecyclerNew(recycler, TypeRefSet, recycler);
        }
    }

    void EntryPointInfo::PinTypeRefs(ScriptContext* scriptContext)
    {TRACE_IT(34873);
        Assert(this->jitTransferData != nullptr && this->jitTransferData->GetIsReady());

        Recycler* recycler = scriptContext->GetRecycler();
        if (this->jitTransferData->GetRuntimeTypeRefs() != nullptr)
        {TRACE_IT(34874);
            // Copy pinned types from a heap allocated array created on the background thread
            // to a recycler allocated array which will live as long as this EntryPointInfo.
            // The original heap allocated array will be freed at the end of NativeCodeGenerator::CheckCodeGenDone
            void** jitPinnedTypeRefs = this->jitTransferData->GetRuntimeTypeRefs();
            size_t jitPinnedTypeRefCount = this->jitTransferData->GetRuntimeTypeRefCount();
            this->runtimeTypeRefs = RecyclerNewArray(recycler, Field(void*), jitPinnedTypeRefCount + 1);
            //js_memcpy_s(this->runtimeTypeRefs, jitPinnedTypeRefCount * sizeof(void*), jitPinnedTypeRefs, jitPinnedTypeRefCount * sizeof(void*));
            for (size_t i = 0; i < jitPinnedTypeRefCount; i++)
            {TRACE_IT(34875);
                this->runtimeTypeRefs[i] = jitPinnedTypeRefs[i];
            }
            this->runtimeTypeRefs[jitPinnedTypeRefCount] = nullptr;
        }
    }

    void EntryPointInfo::InstallGuards(ScriptContext* scriptContext)
    {TRACE_IT(34876);
        Assert(this->jitTransferData != nullptr && this->jitTransferData->GetIsReady());
        Assert(this->equivalentTypeCacheCount == 0 && this->equivalentTypeCaches == nullptr);
        Assert(this->propertyGuardCount == 0 && this->propertyGuardWeakRefs == nullptr);

        for (int i = 0; i < this->jitTransferData->lazyBailoutPropertyCount; i++)
        {TRACE_IT(34877);
            Assert(this->jitTransferData->lazyBailoutProperties != nullptr);

            Js::PropertyId propertyId = this->jitTransferData->lazyBailoutProperties[i];
            Js::PropertyGuard* sharedPropertyGuard;
            bool hasSharedPropertyGuard = TryGetSharedPropertyGuard(propertyId, sharedPropertyGuard);
            Assert(hasSharedPropertyGuard);
            bool isValid = hasSharedPropertyGuard ? sharedPropertyGuard->IsValid() : false;
            if (isValid)
            {TRACE_IT(34878);
                scriptContext->GetThreadContext()->RegisterLazyBailout(propertyId, this);
            }
            else
            {TRACE_IT(34879);
                OUTPUT_TRACE2(Js::LazyBailoutPhase, this->GetFunctionBody(), _u("Lazy bailout - Invalidation due to property: %s \n"), scriptContext->GetPropertyName(propertyId)->GetBuffer());
                this->Invalidate(true);
                return;
            }
        }


        // in-proc JIT
        if (this->jitTransferData->equivalentTypeGuardCount > 0)
        {TRACE_IT(34880);
            Assert(jitTransferData->equivalentTypeGuardOffsets == nullptr);
            Assert(this->jitTransferData->equivalentTypeGuards != nullptr);

            Recycler* recycler = scriptContext->GetRecycler();

            int guardCount = this->jitTransferData->equivalentTypeGuardCount;
            JitEquivalentTypeGuard** guards = this->jitTransferData->equivalentTypeGuards;

            // Create an array of equivalent type caches on the entry point info to ensure they are kept
            // alive for the lifetime of the entry point.
            this->equivalentTypeCacheCount = guardCount;

            // No need to zero-initialize, since we will populate all data slots.
            // We used to let the recycler scan the types in the cache, but we no longer do. See
            // ThreadContext::ClearEquivalentTypeCaches for an explanation.
            this->equivalentTypeCaches = RecyclerNewArrayLeafZ(recycler, EquivalentTypeCache, guardCount);

            this->RegisterEquivalentTypeCaches();

            EquivalentTypeCache* cache = this->equivalentTypeCaches;

            for (JitEquivalentTypeGuard** guard = guards; guard < guards + guardCount; guard++)
            {TRACE_IT(34881);
                EquivalentTypeCache* oldCache = (*guard)->GetCache();
                // Copy the contents of the heap-allocated cache to the recycler-allocated version to make sure the types are
                // kept alive. Allow the properties pointer to refer to the heap-allocated arrays. It will stay alive as long
                // as the entry point is alive, and property entries contain no pointers to other recycler allocated objects.
                (*cache) = (*oldCache);
                // Set the recycler-allocated cache on the (heap-allocated) guard.
                (*guard)->SetCache(cache);

                for(uint i = 0; i < EQUIVALENT_TYPE_CACHE_SIZE; i++)
                {TRACE_IT(34882);
                    if((*cache).types[i] != nullptr)
                    {TRACE_IT(34883);
                        (*cache).types[i]->SetHasBeenCached();
                    }
                }
                cache++;
            }
        }

        if (jitTransferData->equivalentTypeGuardOffsets)
        {TRACE_IT(34884);
            Recycler* recycler = scriptContext->GetRecycler();

            // InstallGuards
            int guardCount = jitTransferData->equivalentTypeGuardOffsets->count;

            // Create an array of equivalent type caches on the entry point info to ensure they are kept
            // alive for the lifetime of the entry point.
            this->equivalentTypeCacheCount = guardCount;

            // No need to zero-initialize, since we will populate all data slots.
            // We used to let the recycler scan the types in the cache, but we no longer do. See
            // ThreadContext::ClearEquivalentTypeCaches for an explanation.
            this->equivalentTypeCaches = RecyclerNewArrayLeafZ(recycler, EquivalentTypeCache, guardCount);

            this->RegisterEquivalentTypeCaches();
            EquivalentTypeCache* cache = this->equivalentTypeCaches;

            for (int i = 0; i < guardCount; i++)
            {TRACE_IT(34885);
                auto& cacheIDL = jitTransferData->equivalentTypeGuardOffsets->guards[i].cache;
                auto guardOffset = jitTransferData->equivalentTypeGuardOffsets->guards[i].offset;
                JitEquivalentTypeGuard* guard = (JitEquivalentTypeGuard*)(this->GetNativeDataBuffer() + guardOffset);
                cache[i].guard = guard;
                cache[i].hasFixedValue = cacheIDL.hasFixedValue != 0;
                cache[i].isLoadedFromProto = cacheIDL.isLoadedFromProto != 0;
                cache[i].nextEvictionVictim = cacheIDL.nextEvictionVictim;
                cache[i].record.propertyCount = cacheIDL.record.propertyCount;
                cache[i].record.properties = (EquivalentPropertyEntry*)(this->GetNativeDataBuffer() + cacheIDL.record.propertyOffset);
                for (int j = 0; j < EQUIVALENT_TYPE_CACHE_SIZE; j++)
                {TRACE_IT(34886);
                    cache[i].types[j] = (Js::Type*)cacheIDL.types[j];
                }
                guard->SetCache(&cache[i]);
            }
        }

        // OOP JIT
        if (jitTransferData->typeGuardTransferData.entries != nullptr)
        {TRACE_IT(34887);
            this->propertyGuardCount = jitTransferData->typeGuardTransferData.propertyGuardCount;
            this->propertyGuardWeakRefs = RecyclerNewArrayZ(scriptContext->GetRecycler(), Field(FakePropertyGuardWeakReference*), this->propertyGuardCount);
            ThreadContext* threadContext = scriptContext->GetThreadContext();
            auto next = &jitTransferData->typeGuardTransferData.entries;
            while (*next)
            {TRACE_IT(34888);
                Js::PropertyId propertyId = (*next)->propId;
                Js::PropertyGuard* sharedPropertyGuard;

                // We use the shared guard created during work item creation to ensure that the condition we assumed didn't change while
                // we were JIT-ing. If we don't have a shared property guard for this property then we must not need to protect it,
                // because it exists on the instance.  Unfortunately, this means that if we have a bug and fail to create a shared
                // guard for some property during work item creation, we won't find out about it here.
                bool isNeeded = TryGetSharedPropertyGuard(propertyId, sharedPropertyGuard);
                bool isValid = isNeeded ? sharedPropertyGuard->IsValid() : false;
                if (isNeeded)
                {TRACE_IT(34889);
                    for (unsigned int i = 0; i < (*next)->guardsCount; i++)
                    {TRACE_IT(34890);
                        Js::JitIndexedPropertyGuard* guard = (Js::JitIndexedPropertyGuard*)(this->nativeDataBuffer + (*next)->guardOffsets[i]);
                        int guardIndex = guard->GetIndex();
                        Assert(guardIndex >= 0 && guardIndex < this->propertyGuardCount);
                        // We use the shared guard here to make sure the conditions we assumed didn't change while we were JIT-ing.
                        // If they did, we proactively invalidate the guard here, so that we bail out if we try to call this code.
                        if (isValid)
                        {TRACE_IT(34891);
                            auto propertyGuardWeakRef = this->propertyGuardWeakRefs[guardIndex];
                            if (propertyGuardWeakRef == nullptr)
                            {TRACE_IT(34892);
                                propertyGuardWeakRef = Js::FakePropertyGuardWeakReference::New(scriptContext->GetRecycler(), guard);
                                this->propertyGuardWeakRefs[guardIndex] = propertyGuardWeakRef;
                            }
                            Assert(propertyGuardWeakRef->Get() == guard);
                            threadContext->RegisterUniquePropertyGuard(propertyId, propertyGuardWeakRef);
                        }
                        else
                        {TRACE_IT(34893);
                            guard->Invalidate();
                        }
                    }
                }
                *next = (*next)->next;
            }
        }

        // in-proc JIT
        // The propertyGuardsByPropertyId structure is temporary and serves only to register the type guards for the correct
        // properties.  If we've done code gen for this EntryPointInfo, typePropertyGuardsByPropertyId will have been used and nulled out.
        if (this->jitTransferData->propertyGuardsByPropertyId != nullptr)
        {TRACE_IT(34894);
            this->propertyGuardCount = this->jitTransferData->propertyGuardCount;
            this->propertyGuardWeakRefs = RecyclerNewArrayZ(scriptContext->GetRecycler(), Field(FakePropertyGuardWeakReference*), this->propertyGuardCount);

            ThreadContext* threadContext = scriptContext->GetThreadContext();

            Js::TypeGuardTransferEntry* entry = this->jitTransferData->propertyGuardsByPropertyId;
            while (entry->propertyId != Js::Constants::NoProperty)
            {TRACE_IT(34895);
                Js::PropertyId propertyId = entry->propertyId;
                Js::PropertyGuard* sharedPropertyGuard;

                // We use the shared guard created during work item creation to ensure that the condition we assumed didn't change while
                // we were JIT-ing. If we don't have a shared property guard for this property then we must not need to protect it,
                // because it exists on the instance.  Unfortunately, this means that if we have a bug and fail to create a shared
                // guard for some property during work item creation, we won't find out about it here.
                bool isNeeded = TryGetSharedPropertyGuard(propertyId, sharedPropertyGuard);
                bool isValid = isNeeded ? sharedPropertyGuard->IsValid() : false;
                int entryGuardIndex = 0;
                while (entry->guards[entryGuardIndex] != nullptr)
                {TRACE_IT(34896);
                    if (isNeeded)
                    {TRACE_IT(34897);
                        Js::JitIndexedPropertyGuard* guard = entry->guards[entryGuardIndex];
                        int guardIndex = guard->GetIndex();
                        Assert(guardIndex >= 0 && guardIndex < this->propertyGuardCount);
                        // We use the shared guard here to make sure the conditions we assumed didn't change while we were JIT-ing.
                        // If they did, we proactively invalidate the guard here, so that we bail out if we try to call this code.
                        if (isValid)
                        {TRACE_IT(34898);
                            auto propertyGuardWeakRef = this->propertyGuardWeakRefs[guardIndex];
                            if (propertyGuardWeakRef == nullptr)
                            {TRACE_IT(34899);
                                propertyGuardWeakRef = Js::FakePropertyGuardWeakReference::New(scriptContext->GetRecycler(), guard);
                                this->propertyGuardWeakRefs[guardIndex] = propertyGuardWeakRef;
                            }
                            Assert(propertyGuardWeakRef->Get() == guard);
                            threadContext->RegisterUniquePropertyGuard(propertyId, propertyGuardWeakRef);
                        }
                        else
                        {TRACE_IT(34900);
                            guard->Invalidate();
                        }
                    }
                    entryGuardIndex++;
                }
                entry = reinterpret_cast<Js::TypeGuardTransferEntry*>(&entry->guards[++entryGuardIndex]);
            }
        }


        // The ctorCacheGuardsByPropertyId structure is temporary and serves only to register the constructor cache guards for the correct
        // properties.  If we've done code gen for this EntryPointInfo, ctorCacheGuardsByPropertyId will have been used and nulled out.
        // Unlike type property guards, constructor cache guards use the live constructor caches associated with function objects. These are
        // recycler allocated and are kept alive by the constructorCaches field, where they were inserted during work item creation.

        // OOP JIT
        if (jitTransferData->ctorCacheTransferData.entries != nullptr)
        {TRACE_IT(34901);
            ThreadContext* threadContext = scriptContext->GetThreadContext();

            CtorCacheTransferEntryIDL ** entries = this->jitTransferData->ctorCacheTransferData.entries;
            for (uint i = 0; i < this->jitTransferData->ctorCacheTransferData.ctorCachesCount; ++i)
            {TRACE_IT(34902);
                Js::PropertyId propertyId = entries[i]->propId;
                Js::PropertyGuard* sharedPropertyGuard;

                // We use the shared guard created during work item creation to ensure that the condition we assumed didn't change while
                // we were JIT-ing. If we don't have a shared property guard for this property then we must not need to protect it,
                // because it exists on the instance.  Unfortunately, this means that if we have a bug and fail to create a shared
                // guard for some property during work item creation, we won't find out about it here.
                bool isNeeded = TryGetSharedPropertyGuard(propertyId, sharedPropertyGuard);
                bool isValid = isNeeded ? sharedPropertyGuard->IsValid() : false;

                if (isNeeded)
                {TRACE_IT(34903);
                    for (uint j = 0; j < entries[i]->cacheCount; ++j)
                    {TRACE_IT(34904);
                        Js::ConstructorCache* cache = (Js::ConstructorCache*)(entries[i]->caches[j]);
                        // We use the shared cache here to make sure the conditions we assumed didn't change while we were JIT-ing.
                        // If they did, we proactively invalidate the cache here, so that we bail out if we try to call this code.
                        if (isValid)
                        {TRACE_IT(34905);
                            threadContext->RegisterConstructorCache(propertyId, cache);
                        }
                        else
                        {TRACE_IT(34906);
                            cache->InvalidateAsGuard();
                        }
                    }
                }
            }
        }

        if (this->jitTransferData->ctorCacheGuardsByPropertyId != nullptr)
        {TRACE_IT(34907);
            ThreadContext* threadContext = scriptContext->GetThreadContext();

            Js::CtorCacheGuardTransferEntry* entry = this->jitTransferData->ctorCacheGuardsByPropertyId;
            while (entry->propertyId != Js::Constants::NoProperty)
            {TRACE_IT(34908);
                Js::PropertyId propertyId = entry->propertyId;
                Js::PropertyGuard* sharedPropertyGuard;

                // We use the shared guard created during work item creation to ensure that the condition we assumed didn't change while
                // we were JIT-ing. If we don't have a shared property guard for this property then we must not need to protect it,
                // because it exists on the instance.  Unfortunately, this means that if we have a bug and fail to create a shared
                // guard for some property during work item creation, we won't find out about it here.
                bool isNeeded = TryGetSharedPropertyGuard(propertyId, sharedPropertyGuard);
                bool isValid = isNeeded ? sharedPropertyGuard->IsValid() : false;
                int entryCacheIndex = 0;
                while (entry->caches[entryCacheIndex] != 0)
                {TRACE_IT(34909);
                    if (isNeeded)
                    {TRACE_IT(34910);
                        Js::ConstructorCache* cache = (Js::ConstructorCache*)(entry->caches[entryCacheIndex]);
                        // We use the shared cache here to make sure the conditions we assumed didn't change while we were JIT-ing.
                        // If they did, we proactively invalidate the cache here, so that we bail out if we try to call this code.
                        if (isValid)
                        {TRACE_IT(34911);
                            threadContext->RegisterConstructorCache(propertyId, cache);
                        }
                        else
                        {TRACE_IT(34912);
                            cache->InvalidateAsGuard();
                        }
                    }
                    entryCacheIndex++;
                }
                entry = reinterpret_cast<Js::CtorCacheGuardTransferEntry*>(&entry->caches[++entryCacheIndex]);
            }
        }

        if (PHASE_ON(Js::FailNativeCodeInstallPhase, this->GetFunctionBody()))
        {TRACE_IT(34913);
            Js::Throw::OutOfMemory();
        }
    }

    PropertyGuard* EntryPointInfo::RegisterSharedPropertyGuard(Js::PropertyId propertyId, ScriptContext* scriptContext)
    {TRACE_IT(34914);
        if (this->sharedPropertyGuards == nullptr)
        {TRACE_IT(34915);
            Recycler* recycler = scriptContext->GetRecycler();
            this->sharedPropertyGuards = RecyclerNew(recycler, SharedPropertyGuardDictionary, recycler);
        }

        PropertyGuard* guard = nullptr;
        if (!this->sharedPropertyGuards->TryGetValue(propertyId, &guard))
        {TRACE_IT(34916);
            ThreadContext* threadContext = scriptContext->GetThreadContext();
            guard = threadContext->RegisterSharedPropertyGuard(propertyId);
            this->sharedPropertyGuards->Add(propertyId, guard);
        }
        return guard;
    }

    Js::PropertyId* EntryPointInfo::GetSharedPropertyGuards(_Out_ unsigned int& count)
    {TRACE_IT(34917);
        Js::PropertyId* sharedPropertyGuards = nullptr;
        unsigned int guardCount = 0;

        if (this->sharedPropertyGuards != nullptr)
        {TRACE_IT(34918);
            const unsigned int sharedPropertyGuardsCount = (unsigned int)this->sharedPropertyGuards->Count();
            Js::PropertyId* guards = RecyclerNewArray(this->GetScriptContext()->GetRecycler(), Js::PropertyId, sharedPropertyGuardsCount);
            auto sharedGuardIter = this->sharedPropertyGuards->GetIterator();

            while (sharedGuardIter.IsValid())
            {TRACE_IT(34919);
                AnalysisAssert(guardCount < sharedPropertyGuardsCount);
                guards[guardCount] = sharedGuardIter.CurrentKey();
                sharedGuardIter.MoveNext();
                ++guardCount;
            }
            AnalysisAssert(guardCount == sharedPropertyGuardsCount);

            sharedPropertyGuards = guards;
        }

        count = guardCount;
        return sharedPropertyGuards;
    }

    bool EntryPointInfo::TryGetSharedPropertyGuard(Js::PropertyId propertyId, Js::PropertyGuard*& guard)
    {TRACE_IT(34920);
        return this->sharedPropertyGuards != nullptr ? this->sharedPropertyGuards->TryGetValue(propertyId, &guard) : false;
    }

    void EntryPointInfo::RecordTypeGuards(int typeGuardCount, TypeGuardTransferEntry* typeGuardTransferRecord, size_t typeGuardTransferPlusSize)
    {TRACE_IT(34921);
        Assert(this->jitTransferData != nullptr);

        this->jitTransferData->propertyGuardCount = typeGuardCount;
        this->jitTransferData->propertyGuardsByPropertyId = typeGuardTransferRecord;
        this->jitTransferData->propertyGuardsByPropertyIdPlusSize = typeGuardTransferPlusSize;
    }

    void EntryPointInfo::RecordCtorCacheGuards(CtorCacheGuardTransferEntry* ctorCacheTransferRecord, size_t ctorCacheTransferPlusSize)
    {TRACE_IT(34922);
        Assert(this->jitTransferData != nullptr);

        this->jitTransferData->ctorCacheGuardsByPropertyId = ctorCacheTransferRecord;
        this->jitTransferData->ctorCacheGuardsByPropertyIdPlusSize = ctorCacheTransferPlusSize;
    }

    void EntryPointInfo::FreePropertyGuards()
    {TRACE_IT(34923);
        // While typePropertyGuardWeakRefs are allocated via NativeCodeData::Allocator and will be automatically freed to the heap,
        // we must zero out the fake weak references so that property guard invalidation doesn't access freed memory.
        if (this->propertyGuardWeakRefs != nullptr)
        {TRACE_IT(34924);
            for (int i = 0; i < this->propertyGuardCount; i++)
            {TRACE_IT(34925);
                if (this->propertyGuardWeakRefs[i] != nullptr)
                {TRACE_IT(34926);
                    this->propertyGuardWeakRefs[i]->Zero();
                }
            }
            this->propertyGuardCount = 0;
            this->propertyGuardWeakRefs = nullptr;
        }
    }

    void EntryPointInfo::RecordBailOutMap(JsUtil::List<LazyBailOutRecord, ArenaAllocator>* bailoutMap)
    {TRACE_IT(34927);
        Assert(this->bailoutRecordMap == nullptr);
        this->bailoutRecordMap = HeapNew(BailOutRecordMap, &HeapAllocator::Instance);
        this->bailoutRecordMap->Copy(bailoutMap);
    }

    void EntryPointInfo::RecordInlineeFrameMap(JsUtil::List<NativeOffsetInlineeFramePair, ArenaAllocator>* tempInlineeFrameMap)
    {TRACE_IT(34928);
        Assert(this->inlineeFrameMap == nullptr);
        if (tempInlineeFrameMap->Count() > 0)
        {TRACE_IT(34929);
            this->inlineeFrameMap = HeapNew(InlineeFrameMap, &HeapAllocator::Instance);
            this->inlineeFrameMap->Copy(tempInlineeFrameMap);
        }
    }
    void EntryPointInfo::RecordInlineeFrameOffsetsInfo(unsigned int offsetsArrayOffset, unsigned int offsetsArrayCount)
    {TRACE_IT(34930);
        this->inlineeFrameOffsetArrayOffset = offsetsArrayOffset;
        this->inlineeFrameOffsetArrayCount = offsetsArrayCount;
    }

    InlineeFrameRecord* EntryPointInfo::FindInlineeFrame(void* returnAddress)
    {TRACE_IT(34931);
        if (this->nativeDataBuffer == nullptr) // in-proc JIT
        {TRACE_IT(34932);
            if (this->inlineeFrameMap == nullptr)
            {TRACE_IT(34933);
                return nullptr;
            }

            size_t offset = (size_t)((BYTE*)returnAddress - (BYTE*)this->GetNativeAddress());
            int index = this->inlineeFrameMap->BinarySearch([=](const NativeOffsetInlineeFramePair& pair, int index) {
                if (pair.offset >= offset)
                {TRACE_IT(34934);
                    if (index == 0 || (index > 0 && this->inlineeFrameMap->Item(index - 1).offset < offset))
                    {TRACE_IT(34935);
                        return 0;
                    }
                    else
                    {TRACE_IT(34936);
                        return 1;
                    }
                }
                return -1;
            });

            if (index == -1)
            {TRACE_IT(34937);
                return nullptr;
            }
            return this->inlineeFrameMap->Item(index).record;
        }
        else // OOP JIT
        {TRACE_IT(34938);
            NativeOffsetInlineeFrameRecordOffset* offsets = (NativeOffsetInlineeFrameRecordOffset*)(this->nativeDataBuffer + this->inlineeFrameOffsetArrayOffset);
            size_t offset = (size_t)((BYTE*)returnAddress - (BYTE*)this->GetNativeAddress());

            if (this->inlineeFrameOffsetArrayCount == 0)
            {TRACE_IT(34939);
                return nullptr;
            }

            uint fromIndex = 0;
            uint toIndex = this->inlineeFrameOffsetArrayCount - 1;
            while (fromIndex <= toIndex)
            {TRACE_IT(34940);
                uint midIndex = fromIndex + (toIndex - fromIndex) / 2;
                auto item = offsets[midIndex];

                if (item.offset >= offset)
                {TRACE_IT(34941);
                    if (midIndex == 0 || (midIndex > 0 && offsets[midIndex - 1].offset < offset))
                    {TRACE_IT(34942);
                        if (offsets[midIndex].recordOffset == NativeOffsetInlineeFrameRecordOffset::InvalidRecordOffset)
                        {TRACE_IT(34943);
                            return nullptr;
                        }
                        else
                        {TRACE_IT(34944);
                            return (InlineeFrameRecord*)(this->nativeDataBuffer + offsets[midIndex].recordOffset);
                        }
                    }
                    else
                    {TRACE_IT(34945);
                        toIndex = midIndex - 1;
                    }
                }
                else
                {TRACE_IT(34946);
                    fromIndex = midIndex + 1;
                }
            }
            return nullptr;
        }
    }

    void EntryPointInfo::DoLazyBailout(BYTE** addressOfInstructionPointer, Js::FunctionBody* functionBody, const PropertyRecord* propertyRecord)
    {TRACE_IT(34947);
        BYTE* instructionPointer = *addressOfInstructionPointer;
        Assert(instructionPointer > (BYTE*)this->nativeAddress && instructionPointer < ((BYTE*)this->nativeAddress + this->codeSize));
        size_t offset = instructionPointer - (BYTE*)this->nativeAddress;
        int found = this->bailoutRecordMap->BinarySearch([=](const LazyBailOutRecord& record, int index)
        {
            // find the closest entry which is greater than the current offset.
            if (record.offset >= offset)
            {TRACE_IT(34948);
                if (index == 0 || (index > 0 && this->bailoutRecordMap->Item(index - 1).offset < offset))
                {TRACE_IT(34949);
                    return 0;
                }
                else
                {TRACE_IT(34950);
                    return 1;
                }
            }
            return -1;
        });
        if (found != -1)
        {TRACE_IT(34951);
            LazyBailOutRecord& record = this->bailoutRecordMap->Item(found);
            *addressOfInstructionPointer = record.instructionPointer;
            record.SetBailOutKind();
            if (PHASE_TRACE1(Js::LazyBailoutPhase))
            {TRACE_IT(34952);
                Output::Print(_u("On stack lazy bailout. Property: %s Old IP: 0x%x New IP: 0x%x "), propertyRecord->GetBuffer(), instructionPointer, record.instructionPointer);
#if DBG
                record.Dump(functionBody);
#endif
                Output::Print(_u("\n"));
            }
        }
        else
        {
            AssertMsg(false, "Lazy Bailout address mapping missing");
        }
    }

    void EntryPointInfo::FreeJitTransferData()
    {TRACE_IT(34953);
        JitTransferData* jitTransferData = this->jitTransferData;
        this->jitTransferData = nullptr;

        if (jitTransferData != nullptr)
        {TRACE_IT(34954);
            // This dictionary is recycler allocated so it doesn't need to be explicitly freed.
            jitTransferData->jitTimeTypeRefs = nullptr;

            if (jitTransferData->lazyBailoutProperties != nullptr)
            {TRACE_IT(34955);
                HeapDeleteArray(jitTransferData->lazyBailoutPropertyCount, jitTransferData->lazyBailoutProperties);
                jitTransferData->lazyBailoutProperties = nullptr;
            }

            // All structures below are heap allocated and need to be freed explicitly.
            if (jitTransferData->runtimeTypeRefs != nullptr)
            {TRACE_IT(34956);
                if (jitTransferData->runtimeTypeRefs->isOOPJIT)
                {TRACE_IT(34957);
                    midl_user_free(jitTransferData->runtimeTypeRefs);
                }
                else
                {
                    HeapDeletePlus(offsetof(PinnedTypeRefsIDL, typeRefs) + sizeof(void*)*jitTransferData->runtimeTypeRefs->count - sizeof(PinnedTypeRefsIDL),
                        PointerValue(jitTransferData->runtimeTypeRefs));
                }
                jitTransferData->runtimeTypeRefs = nullptr;
            }

            if (jitTransferData->propertyGuardsByPropertyId != nullptr)
            {TRACE_IT(34958);
                HeapDeletePlus(jitTransferData->propertyGuardsByPropertyIdPlusSize, jitTransferData->propertyGuardsByPropertyId);
                jitTransferData->propertyGuardsByPropertyId = nullptr;
            }
            jitTransferData->propertyGuardCount = 0;
            jitTransferData->propertyGuardsByPropertyIdPlusSize = 0;

            if (jitTransferData->ctorCacheGuardsByPropertyId != nullptr)
            {TRACE_IT(34959);
                HeapDeletePlus(jitTransferData->ctorCacheGuardsByPropertyIdPlusSize, jitTransferData->ctorCacheGuardsByPropertyId);
                jitTransferData->ctorCacheGuardsByPropertyId = nullptr;
            }
            jitTransferData->ctorCacheGuardsByPropertyIdPlusSize = 0;

            if (jitTransferData->equivalentTypeGuards != nullptr)
            {TRACE_IT(34960);
                HeapDeleteArray(jitTransferData->equivalentTypeGuardCount, jitTransferData->equivalentTypeGuards);
                jitTransferData->equivalentTypeGuards = nullptr;
            }
            jitTransferData->equivalentTypeGuardCount = 0;

            if (jitTransferData->jitTransferRawData != nullptr)
            {TRACE_IT(34961);
                HeapDelete(jitTransferData->jitTransferRawData);
                jitTransferData->jitTransferRawData = nullptr;
            }

            if (jitTransferData->equivalentTypeGuardOffsets)
            {TRACE_IT(34962);
                midl_user_free(jitTransferData->equivalentTypeGuardOffsets);
            }

            if (jitTransferData->typeGuardTransferData.entries != nullptr)
            {TRACE_IT(34963);
                auto next = &jitTransferData->typeGuardTransferData.entries;
                while (*next)
                {TRACE_IT(34964);
                    auto current = (*next);
                    *next = (*next)->next;
                    midl_user_free(current);
                }
            }

            if (jitTransferData->ctorCacheTransferData.entries != nullptr)
            {TRACE_IT(34965);
                CtorCacheTransferEntryIDL ** entries = jitTransferData->ctorCacheTransferData.entries;
                for (uint i = 0; i < jitTransferData->ctorCacheTransferData.ctorCachesCount; ++i)
                {TRACE_IT(34966);
                    midl_user_free(entries[i]);
                }
                midl_user_free(entries);
            }

            jitTransferData = nullptr;
        }
    }

    void EntryPointInfo::RegisterEquivalentTypeCaches()
    {TRACE_IT(34967);
        Assert(this->registeredEquivalentTypeCacheRef == nullptr);
        this->registeredEquivalentTypeCacheRef =
            GetScriptContext()->GetThreadContext()->RegisterEquivalentTypeCacheEntryPoint(this);
    }

    void EntryPointInfo::UnregisterEquivalentTypeCaches()
    {TRACE_IT(34968);
        if (this->registeredEquivalentTypeCacheRef != nullptr)
        {TRACE_IT(34969);
            ScriptContext *scriptContext = GetScriptContext();
            if (scriptContext != nullptr)
            {TRACE_IT(34970);
                scriptContext->GetThreadContext()->UnregisterEquivalentTypeCacheEntryPoint(
                    this->registeredEquivalentTypeCacheRef);
            }
            this->registeredEquivalentTypeCacheRef = nullptr;
        }
    }

    bool EntryPointInfo::ClearEquivalentTypeCaches()
    {TRACE_IT(34971);
        Assert(this->equivalentTypeCaches != nullptr);
        Assert(this->equivalentTypeCacheCount > 0);

        bool isAnyCacheLive = false;
        Recycler *recycler = GetScriptContext()->GetRecycler();
        for (EquivalentTypeCache *cache = this->equivalentTypeCaches;
             cache < this->equivalentTypeCaches + this->equivalentTypeCacheCount;
             cache++)
        {TRACE_IT(34972);
            bool isCacheLive = cache->ClearUnusedTypes(recycler);
            if (isCacheLive)
            {TRACE_IT(34973);
                isAnyCacheLive = true;
            }
        }

        if (!isAnyCacheLive)
        {TRACE_IT(34974);
            // The caller must take care of unregistering this entry point. We may be in the middle of
            // walking the list of registered entry points.
            this->equivalentTypeCaches = nullptr;
            this->equivalentTypeCacheCount = 0;
            this->registeredEquivalentTypeCacheRef = nullptr;
        }

        return isAnyCacheLive;
    }

    bool EquivalentTypeCache::ClearUnusedTypes(Recycler *recycler)
    {TRACE_IT(34975);
        bool isAnyTypeLive = false;

        Assert(this->guard);
        if (this->guard->IsValid())
        {TRACE_IT(34976);
            Type *type = reinterpret_cast<Type*>(this->guard->GetValue());
            if (!recycler->IsObjectMarked(type))
            {TRACE_IT(34977);
                this->guard->InvalidateDuringSweep();
            }
            else
            {TRACE_IT(34978);
                isAnyTypeLive = true;
            }
        }
        uint16 nonNullIndex = 0;
#if DBG
        bool isGuardValuePresent = false;
#endif
        for (int i = 0; i < EQUIVALENT_TYPE_CACHE_SIZE; i++)
        {TRACE_IT(34979);
            Type *type = this->types[i];
            if (type != nullptr)
            {TRACE_IT(34980);
                this->types[i] = nullptr;
                if (recycler->IsObjectMarked(type))
                {TRACE_IT(34981);
                    // compact the types array by moving non-null types
                    // at the beginning.
                    this->types[nonNullIndex++] = type;
#if DBG
                    isGuardValuePresent = this->guard->GetValue() == reinterpret_cast<intptr_t>(type) ? true : isGuardValuePresent;
#endif
                }
            }
        }

        if (nonNullIndex > 0)
        {TRACE_IT(34982);
            isAnyTypeLive = true;
        }
        else
        {TRACE_IT(34983);
#if DBG
            isGuardValuePresent = true; // never went into loop. (noNullIndex == 0)
#endif
            if (guard->IsInvalidatedDuringSweep())
            {TRACE_IT(34984);
                // just mark this as actual invalidated since there are no types
                // present
                guard->Invalidate();
            }
        }

        // verify if guard value is valid, it is present in one of the types
        AssertMsg(!this->guard->IsValid() || isGuardValuePresent, "After ClearUnusedTypes, valid guard value should be one of the cached equivalent types.");
        return isAnyTypeLive;
    }

    void EntryPointInfo::RegisterConstructorCache(Js::ConstructorCache* constructorCache, Recycler* recycler)
    {TRACE_IT(34985);
        Assert(constructorCache != nullptr);

        if (!this->constructorCaches)
        {TRACE_IT(34986);
            this->constructorCaches = RecyclerNew(recycler, ConstructorCacheList, recycler);
        }

        this->constructorCaches->Prepend(constructorCache);
    }
#endif

#if ENABLE_DEBUG_STACK_BACK_TRACE
    void EntryPointInfo::CaptureCleanupStackTrace()
    {TRACE_IT(34987);
        if (this->cleanupStack != nullptr)
        {TRACE_IT(34988);
            this->cleanupStack->Delete(&NoCheckHeapAllocator::Instance);
            this->cleanupStack = nullptr;
        }

        this->cleanupStack = StackBackTrace::Capture(&NoCheckHeapAllocator::Instance);
    }
#endif

    void EntryPointInfo::Finalize(bool isShutdown)
    {TRACE_IT(34989);
        __super::Finalize(isShutdown);

        if (!isShutdown)
        {TRACE_IT(34990);
            ReleasePendingWorkItem();
        }

#if ENABLE_DEBUG_CONFIG_OPTIONS
        this->SetCleanupReason(CleanupReason::CleanUpForFinalize);
#endif

        this->Cleanup(isShutdown, false);

#if ENABLE_DEBUG_STACK_BACK_TRACE
        if (this->cleanupStack != nullptr)
        {TRACE_IT(34991);
            this->cleanupStack->Delete(&NoCheckHeapAllocator::Instance);
            this->cleanupStack = nullptr;
        }
#endif

        this->library = nullptr;
    }

#if ENABLE_NATIVE_CODEGEN
    EntryPointPolymorphicInlineCacheInfo * EntryPointInfo::EnsurePolymorphicInlineCacheInfo(Recycler * recycler, FunctionBody * functionBody)
    {TRACE_IT(34992);
        if (!polymorphicInlineCacheInfo)
        {TRACE_IT(34993);
            polymorphicInlineCacheInfo = RecyclerNew(recycler, EntryPointPolymorphicInlineCacheInfo, functionBody);
        }
        return polymorphicInlineCacheInfo;
    }
#endif

    void EntryPointInfo::Cleanup(bool isShutdown, bool captureCleanupStack)
    {TRACE_IT(34994);
        if (this->GetState() != CleanedUp)
        {TRACE_IT(34995);
            // Unregister xdataInfo before OnCleanup() which may release xdataInfo->address
#if ENABLE_NATIVE_CODEGEN
#if defined(_M_X64)
            if (this->xdataInfo != nullptr)
            {TRACE_IT(34996);
                XDataAllocator::Unregister(this->xdataInfo);
                HeapDelete(this->xdataInfo);
                this->xdataInfo = nullptr;
            }
#elif defined(_M_ARM32_OR_ARM64)
            if (this->xdataInfo != nullptr)
            {TRACE_IT(34997);
                XDataAllocator::Unregister(this->xdataInfo);
                if (JITManager::GetJITManager()->IsOOPJITEnabled())
                {TRACE_IT(34998);
                    HeapDelete(this->xdataInfo);
                }
                this->xdataInfo = nullptr;
            }
#endif
#endif

            this->OnCleanup(isShutdown);

#if ENABLE_NATIVE_CODEGEN
            FreeJitTransferData();

            if (this->bailoutRecordMap != nullptr)
            {TRACE_IT(34999);
                HeapDelete(this->bailoutRecordMap);
                bailoutRecordMap = nullptr;
            }

            if (this->sharedPropertyGuards != nullptr)
            {TRACE_IT(35000);
                sharedPropertyGuards->Clear();
                sharedPropertyGuards = nullptr;
            }

            FreePropertyGuards();

            if (this->equivalentTypeCaches != nullptr)
            {TRACE_IT(35001);
                this->UnregisterEquivalentTypeCaches();
                this->equivalentTypeCacheCount = 0;
                this->equivalentTypeCaches = nullptr;
            }

            if (this->constructorCaches != nullptr)
            {TRACE_IT(35002);
                this->constructorCaches->Clear();
            }
#endif

            // This is how we set the CleanedUp state
            this->workItem = nullptr;
            this->nativeAddress = nullptr;
#if ENABLE_NATIVE_CODEGEN
            this->weakFuncRefSet = nullptr;
            this->runtimeTypeRefs = nullptr;
#endif
            this->codeSize = -1;
            this->library = nullptr;

#if ENABLE_NATIVE_CODEGEN
            DeleteNativeCodeData(this->inProcJITNaticeCodedata);
            this->inProcJITNaticeCodedata = nullptr;
            this->numberChunks = nullptr;

            if (this->nativeDataBuffer)
            {TRACE_IT(35003);
                NativeDataBuffer* buffer = (NativeDataBuffer*)(this->nativeDataBuffer - offsetof(NativeDataBuffer, data));
                midl_user_free(buffer);
            }
#endif

            this->state = CleanedUp;
#if ENABLE_DEBUG_CONFIG_OPTIONS
#if !DBG
            captureCleanupStack = captureCleanupStack && Js::Configuration::Global.flags.FreTestDiagMode;
#endif
#if ENABLE_DEBUG_STACK_BACK_TRACE
            if (captureCleanupStack)
            {TRACE_IT(35004);
                this->CaptureCleanupStackTrace();
            }
#endif
#endif

#if ENABLE_NATIVE_CODEGEN
            if (nullptr != this->nativeThrowSpanSequence)
            {TRACE_IT(35005);
                HeapDelete(this->nativeThrowSpanSequence);
                this->nativeThrowSpanSequence = nullptr;
            }

            this->polymorphicInlineCacheInfo = nullptr;
#endif

#if DBG_DUMP | defined(VTUNE_PROFILING)
            this->nativeOffsetMaps.Reset();
#endif
        }
    }

    void EntryPointInfo::Reset(bool resetStateToNotScheduled)
    {TRACE_IT(35006);
        Assert(this->GetState() != CleanedUp);
        this->nativeAddress = nullptr;
        this->workItem = nullptr;
#if ENABLE_NATIVE_CODEGEN
        if (nullptr != this->nativeThrowSpanSequence)
        {TRACE_IT(35007);
            HeapDelete(this->nativeThrowSpanSequence);
            this->nativeThrowSpanSequence = nullptr;
        }
#endif
        this->codeSize = 0;
#if ENABLE_NATIVE_CODEGEN
        this->weakFuncRefSet = nullptr;
        this->sharedPropertyGuards = nullptr;
        FreePropertyGuards();
        FreeJitTransferData();
        if (this->inProcJITNaticeCodedata != nullptr)
        {TRACE_IT(35008);
            DeleteNativeCodeData(this->inProcJITNaticeCodedata);
            this->inProcJITNaticeCodedata = nullptr;
        }
#endif
        // Set the state to NotScheduled only if the call to Reset is not because of JIT cap being reached
        if (resetStateToNotScheduled)
        {TRACE_IT(35009);
            this->state = NotScheduled;
        }
    }

#if ENABLE_NATIVE_CODEGEN
    // This function needs review when we enable lazy bailouts-
    // Is calling Reset enough? Does Reset sufficiently resets the state of the entryPointInfo?
    void EntryPointInfo::ResetOnLazyBailoutFailure()
    {TRACE_IT(35010);
        Assert(PHASE_ON1(Js::LazyBailoutPhase));

        // Reset the entry point upon a lazy bailout.
        this->Reset(true);
        Assert(this->jsMethod != nullptr);
        FreeNativeCodeGenAllocation(GetScriptContext(), this->jsMethod);
        this->jsMethod = nullptr;
    }
#endif

#ifdef PERF_COUNTERS
    void FunctionEntryPointInfo::OnRecorded()
    {
        PERF_COUNTER_ADD(Code, TotalNativeCodeSize, GetCodeSize());
        PERF_COUNTER_ADD(Code, FunctionNativeCodeSize, GetCodeSize());
        PERF_COUNTER_ADD(Code, DynamicNativeCodeSize, GetCodeSize());
    }
#endif

    FunctionEntryPointInfo::FunctionEntryPointInfo(FunctionProxy * functionProxy, Js::JavascriptMethod method, ThreadContext* context, void* cookie) :
        EntryPointInfo(method, functionProxy->GetScriptContext()->GetLibrary(), cookie, context),
        localVarSlotsOffset(Js::Constants::InvalidOffset),
        localVarChangedOffset(Js::Constants::InvalidOffset),
        callsCount(0),
        jitMode(ExecutionMode::Interpreter),
        functionProxy(functionProxy),
        nextEntryPoint(nullptr),
        mIsTemplatizedJitMode(false)
    {TRACE_IT(35011);
    }

#ifdef ASMJS_PLAT
    void FunctionEntryPointInfo::SetOldFunctionEntryPointInfo(FunctionEntryPointInfo* entrypointInfo)
    {TRACE_IT(35012);
        Assert(this->GetIsAsmJSFunction());
        Assert(entrypointInfo);
        mOldFunctionEntryPointInfo = entrypointInfo;
    };

    FunctionEntryPointInfo* FunctionEntryPointInfo::GetOldFunctionEntryPointInfo()const
    {TRACE_IT(35013);
        Assert(this->GetIsAsmJSFunction());
        return mOldFunctionEntryPointInfo;
    };
    void FunctionEntryPointInfo::SetIsTJMode(bool value)
    {TRACE_IT(35014);
        Assert(this->GetIsAsmJSFunction());
        mIsTemplatizedJitMode = value;
    }

    bool FunctionEntryPointInfo::GetIsTJMode()const
    {TRACE_IT(35015);
        return mIsTemplatizedJitMode;
    };
#endif
    //End AsmJS Support

#if ENABLE_NATIVE_CODEGEN
    ExecutionMode FunctionEntryPointInfo::GetJitMode() const
    {TRACE_IT(35016);
        return jitMode;
    }

    void FunctionEntryPointInfo::SetJitMode(const ExecutionMode jitMode)
    {TRACE_IT(35017);
        Assert(jitMode == ExecutionMode::SimpleJit || jitMode == ExecutionMode::FullJit);

        this->jitMode = jitMode;
    }
#endif

    bool FunctionEntryPointInfo::ExecutedSinceCallCountCollection() const
    {TRACE_IT(35018);
        return this->callsCount != this->lastCallsCount;
    }

    void FunctionEntryPointInfo::CollectCallCounts()
    {TRACE_IT(35019);
        this->lastCallsCount = this->callsCount;
    }

    void FunctionEntryPointInfo::ReleasePendingWorkItem()
    {TRACE_IT(35020);
        // Do this outside of Cleanup since cleanup can be called from the background thread
        // We remove any work items corresponding to the function body being reclaimed
        // so that the background thread doesn't try to use them. ScriptContext != null => this
        // is a function entry point
        // In general this is not needed for loop bodies since loop bodies aren't in the low priority
        // queue, they should be jitted before the entry point is finalized
        if (!this->IsNotScheduled() && !this->IsCleanedUp())
        {TRACE_IT(35021);
#if defined(_M_ARM32_OR_ARM64)
            // On ARM machines, order of writes is not guaranteed while reading data from another processor
            // So we need to have a memory barrier here in order to make sure that the work item is consistent
            MemoryBarrier();
#endif
            CodeGenWorkItem* workItem = this->GetWorkItem();
            if (workItem != nullptr)
            {TRACE_IT(35022);
                Assert(this->library != nullptr);
#if ENABLE_NATIVE_CODEGEN
                TryReleaseNonHiPriWorkItem(this->library->GetScriptContext(), workItem);
#endif
                }
        }
    }

    FunctionBody *FunctionEntryPointInfo::GetFunctionBody() const
    {TRACE_IT(35023);
        return functionProxy->GetFunctionBody();
    }

    void FunctionEntryPointInfo::OnCleanup(bool isShutdown)
    {TRACE_IT(35024);
        if (this->IsCodeGenDone())
        {TRACE_IT(35025);
            Assert(this->functionProxy->GetFunctionInfo()->HasBody());
#if ENABLE_NATIVE_CODEGEN
            if (nullptr != this->inlineeFrameMap)
            {TRACE_IT(35026);
                HeapDelete(this->inlineeFrameMap);
                this->inlineeFrameMap = nullptr;
            }
#if PDATA_ENABLED
            if (this->xdataInfo != nullptr)
            {TRACE_IT(35027);
                XDataAllocator::Unregister(this->xdataInfo);
#if defined(_M_ARM32_OR_ARM64)
                if (JITManager::GetJITManager()->IsOOPJITEnabled())
#endif
                {TRACE_IT(35028);
                    HeapDelete(this->xdataInfo);
                }
                this->xdataInfo = nullptr;
            }
#endif
#endif

            if(nativeEntryPointProcessed)
            {TRACE_IT(35029);
                JS_ETW(EtwTrace::LogMethodNativeUnloadEvent(this->functionProxy->GetFunctionBody(), this));
            }

            FunctionBody* functionBody = this->functionProxy->GetFunctionBody();
#ifdef ASMJS_PLAT
            if (this->GetIsTJMode())
            {TRACE_IT(35030);
                // release LoopHeaders here if the entrypointInfo is TJ
                this->GetFunctionBody()->ReleaseLoopHeaders();
            }
#endif
            if(functionBody->GetSimpleJitEntryPointInfo() == this)
            {TRACE_IT(35031);
                functionBody->SetSimpleJitEntryPointInfo(nullptr);
            }
            // If we're shutting down, the script context might be gone
            if (!isShutdown)
            {TRACE_IT(35032);
                ScriptContext* scriptContext = this->functionProxy->GetScriptContext();

                void* currentCookie = nullptr;

#if ENABLE_NATIVE_CODEGEN
                // In the debugger case, we might call cleanup after the native code gen that
                // allocated this entry point has already shutdown. In that case, the validation
                // check below should fail and we should not try to free this entry point
                // since it's already been freed
                NativeCodeGenerator* currentNativeCodegen = scriptContext->GetNativeCodeGenerator();
                Assert(this->validationCookie != nullptr);
                currentCookie = (void*)currentNativeCodegen;
#endif

                if (this->jsMethod == reinterpret_cast<Js::JavascriptMethod>(this->GetNativeAddress()))
                {TRACE_IT(35033);
#if DBG
                    // tag the jsMethod in case the native address is reused in recycler and create a false positive
                    // not checking validationCookie because this can happen while debugger attaching, native address
                    // are batch freed through deleting NativeCodeGenerator
                    this->jsMethod = (Js::JavascriptMethod)((intptr_t)this->jsMethod | 1);
#else
                    this->jsMethod = nullptr;
#endif
                }

                if (validationCookie == currentCookie)
                {TRACE_IT(35034);
                    scriptContext->FreeFunctionEntryPoint((Js::JavascriptMethod)this->GetNativeAddress());
                }
            }

#ifdef PERF_COUNTERS
            PERF_COUNTER_SUB(Code, TotalNativeCodeSize, GetCodeSize());
            PERF_COUNTER_SUB(Code, FunctionNativeCodeSize, GetCodeSize());
            PERF_COUNTER_SUB(Code, DynamicNativeCodeSize, GetCodeSize());
#endif
        }

        this->functionProxy = nullptr;
    }

#if ENABLE_NATIVE_CODEGEN
    void FunctionEntryPointInfo::ResetOnNativeCodeInstallFailure()
    {TRACE_IT(35035);
        this->functionProxy->MapFunctionObjectTypes([&](DynamicType* type)
        {
            Assert(type->GetTypeId() == TypeIds_Function);

            ScriptFunctionType* functionType = (ScriptFunctionType*)type;
            if (functionType->GetEntryPointInfo() == this)
            {TRACE_IT(35036);
                if (!this->GetIsAsmJSFunction())
                {TRACE_IT(35037);
                    functionType->SetEntryPoint(GetCheckCodeGenThunk());
                }
#ifdef ASMJS_PLAT
                else
                {TRACE_IT(35038);
                    functionType->SetEntryPoint(GetCheckAsmJsCodeGenThunk());
                }
#endif
            }
        });
    }

    void FunctionEntryPointInfo::EnterExpirableCollectMode()
    {TRACE_IT(35039);
        this->lastCallsCount = this->callsCount;
        // For code that is not jitted yet we don't want to expire since there is nothing to free here
        if (this->IsCodeGenPending())
        {TRACE_IT(35040);
            this->SetIsObjectUsed();
        }

    }

    void FunctionEntryPointInfo::Invalidate(bool prolongEntryPoint)
    {TRACE_IT(35041);
        Assert(!this->functionProxy->IsDeferred());
        FunctionBody* functionBody = this->functionProxy->GetFunctionBody();
        Assert(this != functionBody->GetSimpleJitEntryPointInfo());

        // We may have got here following OOM in ProcessJitTransferData. Free any data we have
        // to reduce the chance of another OOM below.
        this->FreeJitTransferData();
        FunctionEntryPointInfo* entryPoint = functionBody->GetDefaultFunctionEntryPointInfo();
        if (entryPoint->IsCodeGenPending())
        {TRACE_IT(35042);
            OUTPUT_TRACE(Js::LazyBailoutPhase, _u("Skipping creating new entrypoint as one is already pending\n"));
        }
        else
        {TRACE_IT(35043);
            class AutoCleanup
            {
                EntryPointInfo *entryPointInfo;
            public:
                AutoCleanup(EntryPointInfo *entryPointInfo) : entryPointInfo(entryPointInfo)
                {TRACE_IT(35044);
                }

                void Done()
                {TRACE_IT(35045);
                    entryPointInfo = nullptr;
                }
                ~AutoCleanup()
                {TRACE_IT(35046);
                    if (entryPointInfo)
                    {TRACE_IT(35047);
                        entryPointInfo->ResetOnLazyBailoutFailure();
                    }
                }
            } autoCleanup(this);

            entryPoint = functionBody->CreateNewDefaultEntryPoint();

            GenerateFunction(functionBody->GetScriptContext()->GetNativeCodeGenerator(), functionBody, /*function*/ nullptr);
            autoCleanup.Done();

        }
        this->functionProxy->MapFunctionObjectTypes([&](DynamicType* type)
        {
            Assert(type->GetTypeId() == TypeIds_Function);

            ScriptFunctionType* functionType = (ScriptFunctionType*)type;
            if (functionType->GetEntryPointInfo() == this)
            {TRACE_IT(35048);
                functionType->SetEntryPointInfo(entryPoint);
                functionType->SetEntryPoint(this->functionProxy->GetDirectEntryPoint(entryPoint));
            }
        });
        if (!prolongEntryPoint)
        {TRACE_IT(35049);
            ThreadContext* threadContext = this->functionProxy->GetScriptContext()->GetThreadContext();
            threadContext->QueueFreeOldEntryPointInfoIfInScript(this);
        }
    }

    void FunctionEntryPointInfo::Expire()
    {TRACE_IT(35050);
        if (this->lastCallsCount != this->callsCount || !this->nativeEntryPointProcessed || this->IsCleanedUp())
        {TRACE_IT(35051);
            return;
        }

        ThreadContext* threadContext = this->functionProxy->GetScriptContext()->GetThreadContext();

        Assert(!this->functionProxy->IsDeferred());
        FunctionBody* functionBody = this->functionProxy->GetFunctionBody();

        FunctionEntryPointInfo *simpleJitEntryPointInfo = functionBody->GetSimpleJitEntryPointInfo();
        const bool expiringSimpleJitEntryPointInfo = simpleJitEntryPointInfo == this;
        if(expiringSimpleJitEntryPointInfo)
        {TRACE_IT(35052);
            if(functionBody->GetExecutionMode() != ExecutionMode::FullJit)
            {TRACE_IT(35053);
                // Don't expire simple JIT code until the transition to full JIT
                return;
            }
            simpleJitEntryPointInfo = nullptr;
            functionBody->SetSimpleJitEntryPointInfo(nullptr);
        }

        try
        {TRACE_IT(35054);
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);

            FunctionEntryPointInfo* newEntryPoint = nullptr;
            FunctionEntryPointInfo *const defaultEntryPointInfo = functionBody->GetDefaultFunctionEntryPointInfo();
            if(this == defaultEntryPointInfo)
            {TRACE_IT(35055);
                if(simpleJitEntryPointInfo)
                {TRACE_IT(35056);
                    newEntryPoint = simpleJitEntryPointInfo;
                    functionBody->SetDefaultFunctionEntryPointInfo(
                        simpleJitEntryPointInfo,
                        reinterpret_cast<JavascriptMethod>(newEntryPoint->GetNativeAddress()));
                    functionBody->SetExecutionMode(ExecutionMode::SimpleJit);
                    functionBody->ResetSimpleJitLimitAndCallCount();
                }
#ifdef ASMJS_PLAT
                else if (functionBody->GetIsAsmJsFunction())
                {TRACE_IT(35057);
                    // the new entrypoint will be set to interpreter
                    newEntryPoint = functionBody->CreateNewDefaultEntryPoint();
                    newEntryPoint->SetIsAsmJSFunction(true);
                    newEntryPoint->jsMethod = AsmJsDefaultEntryThunk;
                    functionBody->SetIsAsmJsFullJitScheduled(false);
                    functionBody->SetExecutionMode(functionBody->GetDefaultInterpreterExecutionMode());
                    this->functionProxy->SetOriginalEntryPoint(AsmJsDefaultEntryThunk);
                }
#endif
                else
                {TRACE_IT(35058);
                    newEntryPoint = functionBody->CreateNewDefaultEntryPoint();
                    functionBody->SetExecutionMode(functionBody->GetDefaultInterpreterExecutionMode());
                }
                functionBody->TraceExecutionMode("JitCodeExpired");
            }
            else
            {TRACE_IT(35059);
                newEntryPoint = defaultEntryPointInfo;
            }

            OUTPUT_TRACE(Js::ExpirableCollectPhase,  _u("Expiring 0x%p\n"), this);
            this->functionProxy->MapFunctionObjectTypes([&] (DynamicType* type)
            {
                Assert(type->GetTypeId() == TypeIds_Function);

                ScriptFunctionType* functionType = (ScriptFunctionType*) type;
                if (functionType->GetEntryPointInfo() == this)
                {TRACE_IT(35060);
                    OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Type 0x%p uses this entry point- switching to default entry point\n"), this);
                    functionType->SetEntryPointInfo(newEntryPoint);
                    // we are allowed to replace the entry point on the type only if it's
                    // directly using the jitted code or a type is referencing this entry point
                    // but the entry point hasn't been called since the codegen thunk was installed on it
                    if (functionType->GetEntryPoint() == functionProxy->GetDirectEntryPoint(this) || IsIntermediateCodeGenThunk(functionType->GetEntryPoint()))
                    {TRACE_IT(35061);
                        functionType->SetEntryPoint(this->functionProxy->GetDirectEntryPoint(newEntryPoint));
                    }
                }
                else
                {TRACE_IT(35062);
                    Assert(!functionType->GetEntryPointInfo()->IsFunctionEntryPointInfo() ||
                        ((FunctionEntryPointInfo*)functionType->GetEntryPointInfo())->IsCleanedUp()
                        || (DWORD_PTR)functionType->GetEntryPoint() != this->GetNativeAddress());
                }
            });

            if(expiringSimpleJitEntryPointInfo)
            {TRACE_IT(35063);
                // We could have just created a new entry point info that is using the simple JIT code. An allocation may have
                // triggered shortly after, resulting in expiring the simple JIT entry point info. Update any entry point infos
                // that are using the simple JIT code, and update the original entry point as necessary as well.
                const JavascriptMethod newOriginalEntryPoint =
                    functionBody->GetDynamicInterpreterEntryPoint()
                        ?   reinterpret_cast<JavascriptMethod>(
                                InterpreterThunkEmitter::ConvertToEntryPoint(functionBody->GetDynamicInterpreterEntryPoint()))
                        :   DefaultEntryThunk;
                const JavascriptMethod currentThunk = functionBody->GetScriptContext()->CurrentThunk;
                const JavascriptMethod newDirectEntryPoint =
                    currentThunk == DefaultEntryThunk ? newOriginalEntryPoint : currentThunk;
                const JavascriptMethod simpleJitNativeAddress = reinterpret_cast<JavascriptMethod>(GetNativeAddress());
                functionBody->MapEntryPoints([&](const int entryPointIndex, FunctionEntryPointInfo *const entryPointInfo)
                {
                    if(entryPointInfo != this && entryPointInfo->jsMethod == simpleJitNativeAddress)
                    {TRACE_IT(35064);
                        entryPointInfo->jsMethod = newDirectEntryPoint;
                    }
                });
                if(functionBody->GetOriginalEntryPoint_Unchecked() == simpleJitNativeAddress)
                {TRACE_IT(35065);
                    functionBody->SetOriginalEntryPoint(newOriginalEntryPoint);
                    functionBody->VerifyOriginalEntryPoint();
                }
            }

            threadContext->QueueFreeOldEntryPointInfoIfInScript(this);
        }
        catch (Js::OutOfMemoryException)
        {TRACE_IT(35066);
            // If we can't allocate a new entry point, skip expiring this object
            if(expiringSimpleJitEntryPointInfo)
            {TRACE_IT(35067);
                simpleJitEntryPointInfo = this;
                functionBody->SetSimpleJitEntryPointInfo(this);
            }
        }
    }
#endif

#ifdef PERF_COUNTERS
    void LoopEntryPointInfo::OnRecorded()
    {
        PERF_COUNTER_ADD(Code, TotalNativeCodeSize, GetCodeSize());
        PERF_COUNTER_ADD(Code, LoopNativeCodeSize, GetCodeSize());
        PERF_COUNTER_ADD(Code, DynamicNativeCodeSize, GetCodeSize());
    }
#endif

    FunctionBody *LoopEntryPointInfo::GetFunctionBody() const
    {TRACE_IT(35068);
        return loopHeader->functionBody;
    }

    //End AsmJs Support

    void LoopEntryPointInfo::OnCleanup(bool isShutdown)
    {TRACE_IT(35069);
#ifdef ASMJS_PLAT
        if (this->IsCodeGenDone() && !this->GetIsTJMode())
#else
        if (this->IsCodeGenDone())
#endif
        {TRACE_IT(35070);
            JS_ETW(EtwTrace::LogLoopBodyUnloadEvent(this->loopHeader->functionBody, this->loopHeader, this));

#if ENABLE_NATIVE_CODEGEN
            if (nullptr != this->inlineeFrameMap)
            {TRACE_IT(35071);
                HeapDelete(this->inlineeFrameMap);
                this->inlineeFrameMap = nullptr;
            }
#if PDATA_ENABLED
            if (this->xdataInfo != nullptr)
            {TRACE_IT(35072);
                XDataAllocator::Unregister(this->xdataInfo);
#if defined(_M_ARM32_OR_ARM64)
                if (JITManager::GetJITManager()->IsOOPJITEnabled())
#endif
                {TRACE_IT(35073);
                    HeapDelete(this->xdataInfo);
                }
                this->xdataInfo = nullptr;
            }
#endif
#endif

            if (!isShutdown)
            {TRACE_IT(35074);
                void* currentCookie = nullptr;
                ScriptContext* scriptContext = this->loopHeader->functionBody->GetScriptContext();

#if ENABLE_NATIVE_CODEGEN
                // In the debugger case, we might call cleanup after the native code gen that
                // allocated this entry point has already shutdown. In that case, the validation
                // check below should fail and we should not try to free this entry point
                // since it's already been freed
                NativeCodeGenerator* currentNativeCodegen = scriptContext->GetNativeCodeGenerator();
                Assert(this->validationCookie != nullptr);
                currentCookie = (void*)currentNativeCodegen;
#endif

                if (this->jsMethod == reinterpret_cast<Js::JavascriptMethod>(this->GetNativeAddress()))
                {TRACE_IT(35075);
#if DBG
                    // tag the jsMethod in case the native address is reused in recycler and create a false positive
                    // not checking validationCookie because this can happen while debugger attaching, native address
                    // are batch freed through deleting NativeCodeGenerator
                    this->jsMethod = (Js::JavascriptMethod)((intptr_t)this->jsMethod | 1);
#else
                    this->jsMethod = nullptr;
#endif
                }

                if (validationCookie == currentCookie)
                {TRACE_IT(35076);
                    scriptContext->FreeFunctionEntryPoint(reinterpret_cast<Js::JavascriptMethod>(this->GetNativeAddress()));
                }
            }

#ifdef PERF_COUNTERS
            PERF_COUNTER_SUB(Code, TotalNativeCodeSize, GetCodeSize());
            PERF_COUNTER_SUB(Code, LoopNativeCodeSize, GetCodeSize());
            PERF_COUNTER_SUB(Code, DynamicNativeCodeSize, GetCodeSize());
#endif
        }
    }

#if ENABLE_NATIVE_CODEGEN
    void LoopEntryPointInfo::ResetOnNativeCodeInstallFailure()
    {TRACE_IT(35077);
        // Since we call the address on the entryPointInfo for loop bodies, all we need to do is to roll back
        // the fields on the entryPointInfo related to transferring data from jit thread to main thread (already
        // being done in EntryPointInfo::OnNativeCodeInstallFailure). On the next loop iteration, the interpreter
        // will call EntryPointInfo::EnsureIsReadyToCall and we'll try to process jit transfer data again.
    }
#endif

    void LoopHeader::Init( FunctionBody * functionBody )
    {TRACE_IT(35078);
        // DisableJIT-TODO: Should this entire class be ifdefed out?
#if ENABLE_NATIVE_CODEGEN
        this->functionBody = functionBody;
        Recycler* recycler = functionBody->GetScriptContext()->GetRecycler();

        // Sync entryPoints changes to etw rundown lock
        auto syncObj = functionBody->GetScriptContext()->GetThreadContext()->GetEtwRundownCriticalSection();
        this->entryPoints = RecyclerNew(recycler, LoopEntryPointList, recycler, syncObj);

        this->CreateEntryPoint();
#endif
    }

#if ENABLE_NATIVE_CODEGEN
    int LoopHeader::CreateEntryPoint()
    {TRACE_IT(35079);
        ScriptContext* scriptContext = this->functionBody->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();
        LoopEntryPointInfo* entryPoint = RecyclerNew(recycler, LoopEntryPointInfo, this, scriptContext->GetLibrary(), scriptContext->GetNativeCodeGenerator());
        return this->entryPoints->Add(entryPoint);
    }

    void LoopHeader::ReleaseEntryPoints()
    {TRACE_IT(35080);
        for (int iEntryPoint = 0; iEntryPoint < this->entryPoints->Count(); iEntryPoint++)
        {TRACE_IT(35081);
            LoopEntryPointInfo * entryPoint = this->entryPoints->Item(iEntryPoint);

            if (entryPoint != nullptr && entryPoint->IsCodeGenDone())
            {TRACE_IT(35082);
                // ReleaseEntryPoints is not called during recycler shutdown scenarios
                // We also don't capture the cleanup stack since we've not seen cleanup bugs affect
                // loop entry points so far. We can pass true here if this is no longer the case.
                entryPoint->Cleanup(false /* isShutdown */, false /* capture cleanup stack */);
                this->entryPoints->Item(iEntryPoint, nullptr);
            }
        }
    }
#endif

#if ENABLE_DEBUG_CONFIG_OPTIONS
    void FunctionBody::DumpRegStats(FunctionBody *funcBody)
    {TRACE_IT(35083);
        if (funcBody->callCountStats == 0)
        {TRACE_IT(35084);
            return;
        }
        uint loads = funcBody->regAllocLoadCount;
        uint stores = funcBody->regAllocStoreCount;

        if (Js::Configuration::Global.flags.NormalizeStats)
        {TRACE_IT(35085);
            loads /= this->callCountStats;
            stores /= this->callCountStats;
        }
        funcBody->DumpFullFunctionName();
        Output::SkipToColumn(55);
        Output::Print(_u("Calls:%6d  Loads:%9d  Stores:%9d  Total refs:%9d\n"), this->callCountStats,
            loads, stores, loads + stores);
    }
#endif

    Js::RegSlot FunctionBody::GetRestParamRegSlot()
    {TRACE_IT(35086);
        Js::RegSlot dstRegSlot = GetConstantCount();
        if (GetHasImplicitArgIns())
        {TRACE_IT(35087);
            dstRegSlot += GetInParamsCount() - 1;
        }
        return dstRegSlot;
    }
    uint FunctionBody::GetNumberOfRecursiveCallSites()
    {TRACE_IT(35088);
        uint recursiveInlineSpan = 0;
        uint recursiveCallSiteInlineInfo = 0;
#if ENABLE_PROFILE_INFO
        if (this->HasDynamicProfileInfo())
        {TRACE_IT(35089);
            recursiveCallSiteInlineInfo = this->dynamicProfileInfo->GetRecursiveInlineInfo();
        }
#endif

        while (recursiveCallSiteInlineInfo)
        {TRACE_IT(35090);
            recursiveInlineSpan += (recursiveCallSiteInlineInfo & 1);
            recursiveCallSiteInlineInfo >>= 1;
        }
        return recursiveInlineSpan;
    }

    bool FunctionBody::CanInlineRecursively(uint depth, bool tryAggressive)
    {TRACE_IT(35091);
        uint recursiveInlineSpan = this->GetNumberOfRecursiveCallSites();

        uint minRecursiveInlineDepth = (uint)CONFIG_FLAG(RecursiveInlineDepthMin);

        if (recursiveInlineSpan != this->GetProfiledCallSiteCount() || tryAggressive == false)
        {TRACE_IT(35092);
            return depth < minRecursiveInlineDepth;
        }

        uint maxRecursiveInlineDepth = (uint)CONFIG_FLAG(RecursiveInlineDepthMax);
        uint maxRecursiveBytecodeBudget = (uint)CONFIG_FLAG(RecursiveInlineThreshold);
        uint numberOfAllowedFuncs = maxRecursiveBytecodeBudget / this->GetByteCodeWithoutLDACount();
        uint maxDepth;

        if (recursiveInlineSpan == 1)
        {TRACE_IT(35093);
            maxDepth = numberOfAllowedFuncs;
        }
        else
        {TRACE_IT(35094);
            maxDepth = (uint)ceil(log((double)((double)numberOfAllowedFuncs) / log((double)recursiveInlineSpan)));
        }
        maxDepth = maxDepth < minRecursiveInlineDepth ? minRecursiveInlineDepth : maxDepth;
        maxDepth = maxDepth < maxRecursiveInlineDepth ? maxDepth : maxRecursiveInlineDepth;
        return depth < maxDepth;
    }


    static const char16 LoopWStr[] = _u("Loop");
    size_t FunctionBody::GetLoopBodyName(uint loopNumber, _Out_writes_opt_z_(sizeInChars) WCHAR* displayName, _In_ size_t sizeInChars)
    {TRACE_IT(35095);
        const char16* functionName = this->GetExternalDisplayName();
        size_t length = wcslen(functionName) + /*length of largest int32*/ 10 + _countof(LoopWStr) + /*null*/ 1;
        if (sizeInChars < length || displayName == nullptr)
        {TRACE_IT(35096);
            return length;
        }
        int charsWritten = swprintf_s(displayName, length, _u("%s%s%u"), functionName, LoopWStr, loopNumber + 1);
        Assert(charsWritten != -1);
        return charsWritten + /*nullptr*/ 1;
    }

    void FunctionBody::MapAndSetEnvRegister(RegSlot reg)
    {TRACE_IT(35097);
        Assert(!m_hasEnvRegister);
        SetEnvRegister(this->MapRegSlot(reg));
    }
    void FunctionBody::SetEnvRegister(RegSlot reg)
    {TRACE_IT(35098);
        if (reg == Constants::NoRegister)
        {TRACE_IT(35099);
            m_hasEnvRegister = false;
        }
        else
        {TRACE_IT(35100);
            m_hasEnvRegister = true;
            SetCountField(CounterFields::EnvRegister, reg);
        }
    }
    RegSlot FunctionBody::GetEnvRegister() const
    {TRACE_IT(35101);
        return m_hasEnvRegister ? GetCountField(CounterFields::EnvRegister) : Constants::NoRegister;
    }
    void FunctionBody::MapAndSetThisRegisterForEventHandler(RegSlot reg)
    {TRACE_IT(35102);
        Assert(!m_hasThisRegisterForEventHandler);
        SetThisRegisterForEventHandler(this->MapRegSlot(reg));
    }
    void FunctionBody::SetThisRegisterForEventHandler(RegSlot reg)
    {TRACE_IT(35103);
        if (reg == Constants::NoRegister)
        {TRACE_IT(35104);
            m_hasThisRegisterForEventHandler = false;
        }
        else
        {TRACE_IT(35105);
            m_hasThisRegisterForEventHandler = true;
            SetCountField(CounterFields::ThisRegisterForEventHandler, reg);
        }
    }
    RegSlot FunctionBody::GetThisRegisterForEventHandler() const
    {TRACE_IT(35106);
        return m_hasThisRegisterForEventHandler ? GetCountField(CounterFields::ThisRegisterForEventHandler) : Constants::NoRegister;
    }
    void FunctionBody::SetLocalClosureRegister(RegSlot reg)
    {TRACE_IT(35107);
        if (reg == Constants::NoRegister)
        {TRACE_IT(35108);
            m_hasLocalClosureRegister = false;
        }
        else
        {TRACE_IT(35109);
            m_hasLocalClosureRegister = true;
            SetCountField(CounterFields::LocalClosureRegister, reg);
        }
    }
    void FunctionBody::MapAndSetLocalClosureRegister(RegSlot reg)
    {TRACE_IT(35110);
        Assert(!m_hasLocalClosureRegister);
        SetLocalClosureRegister(this->MapRegSlot(reg));
    }
    RegSlot FunctionBody::GetLocalClosureRegister() const
    {TRACE_IT(35111);
        return m_hasLocalClosureRegister ? GetCountField(CounterFields::LocalClosureRegister) : Constants::NoRegister;
    }
    void FunctionBody::SetParamClosureRegister(RegSlot reg)
    {TRACE_IT(35112);
        if (reg == Constants::NoRegister)
        {TRACE_IT(35113);
            m_hasParamClosureRegister = false;
        }
        else
        {TRACE_IT(35114);
            m_hasParamClosureRegister = true;
            SetCountField(CounterFields::ParamClosureRegister, reg);
        }
    }
    void FunctionBody::MapAndSetParamClosureRegister(RegSlot reg)
    {TRACE_IT(35115);
        Assert(!m_hasParamClosureRegister);
        SetParamClosureRegister(this->MapRegSlot(reg));
    }
    RegSlot FunctionBody::GetParamClosureRegister() const
    {TRACE_IT(35116);
        return m_hasParamClosureRegister ? GetCountField(CounterFields::ParamClosureRegister) : Constants::NoRegister;
    }
    void FunctionBody::MapAndSetLocalFrameDisplayRegister(RegSlot reg)
    {TRACE_IT(35117);
        Assert(!m_hasLocalFrameDisplayRegister);
        SetLocalFrameDisplayRegister(this->MapRegSlot(reg));
    }
    void FunctionBody::SetLocalFrameDisplayRegister(RegSlot reg)
    {TRACE_IT(35118);
        if (reg == Constants::NoRegister)
        {TRACE_IT(35119);
            m_hasLocalFrameDisplayRegister = false;
        }
        else
        {TRACE_IT(35120);
            m_hasLocalFrameDisplayRegister = true;
            SetCountField(CounterFields::LocalFrameDisplayRegister, reg);
        }
    }
    RegSlot FunctionBody::GetLocalFrameDisplayRegister() const
    {TRACE_IT(35121);
        return m_hasLocalFrameDisplayRegister ? GetCountField(CounterFields::LocalFrameDisplayRegister) : Constants::NoRegister;
    }
    void FunctionBody::MapAndSetFirstInnerScopeRegister(RegSlot reg)
    {TRACE_IT(35122);
        Assert(!m_hasFirstInnerScopeRegister);
        SetFirstInnerScopeRegister(this->MapRegSlot(reg));
    }
    void FunctionBody::SetFirstInnerScopeRegister(RegSlot reg)
    {TRACE_IT(35123);
        if (reg == Constants::NoRegister)
        {TRACE_IT(35124);
            m_hasFirstInnerScopeRegister = false;
        }
        else
        {TRACE_IT(35125);
            m_hasFirstInnerScopeRegister = true;
            SetCountField(CounterFields::FirstInnerScopeRegister, reg);
        }
    }
    RegSlot FunctionBody::GetFirstInnerScopeRegister() const
    {TRACE_IT(35126);
        return m_hasFirstInnerScopeRegister ? GetCountField(CounterFields::FirstInnerScopeRegister) : Constants::NoRegister;
    }
    void FunctionBody::MapAndSetFuncExprScopeRegister(RegSlot reg)
    {TRACE_IT(35127);
        Assert(!m_hasFuncExprScopeRegister);
        SetFuncExprScopeRegister(this->MapRegSlot(reg));
    }
    void FunctionBody::SetFuncExprScopeRegister(RegSlot reg)
    {TRACE_IT(35128);
        if (reg == Constants::NoRegister)
        {TRACE_IT(35129);
            m_hasFuncExprScopeRegister = false;
        }
        else
        {TRACE_IT(35130);
            m_hasFuncExprScopeRegister = true;
            SetCountField(CounterFields::FuncExprScopeRegister, reg);
        }
    }
    RegSlot FunctionBody::GetFuncExprScopeRegister() const
    {TRACE_IT(35131);
        return m_hasFuncExprScopeRegister ? GetCountField(CounterFields::FuncExprScopeRegister) : Constants::NoRegister;
    }

    void FunctionBody::SetFirstTmpRegister(RegSlot reg)
    {TRACE_IT(35132);
        if (reg == Constants::NoRegister)
        {TRACE_IT(35133);
            m_hasFirstTmpRegister = false;
        }
        else
        {TRACE_IT(35134);
            m_hasFirstTmpRegister = true;
            SetCountField(CounterFields::FirstTmpRegister, reg);
        }
    }
    RegSlot FunctionBody::GetFirstTmpRegister() const
    {TRACE_IT(35135);
        return m_hasFirstTmpRegister ? this->GetCountField(CounterFields::FirstTmpRegister) : Constants::NoRegister;
    }
}
