//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct CodeGenWorkItem;
class Lowerer;
class Inline;
class FlowGraph;

#if defined(_M_ARM32_OR_ARM64)
#include "UnwindInfoManager.h"
#endif

struct Cloner
{
    Cloner(Lowerer *lowerer, JitArenaAllocator *alloc) :
        alloc(alloc),
        symMap(nullptr),
        labelMap(nullptr),
        lowerer(lowerer),
        instrFirst(nullptr),
        instrLast(nullptr),
        fRetargetClonedBranch(FALSE)
    {TRACE_IT(2667);
    }

    ~Cloner()
    {TRACE_IT(2668);
        if (symMap)
        {
            Adelete(alloc, symMap);
        }
        if (labelMap)
        {
            Adelete(alloc, labelMap);
        }
    }

    void AddInstr(IR::Instr * instrOrig, IR::Instr * instrClone);
    void Finish();
    void RetargetClonedBranches();

    HashTable<StackSym*> *symMap;
    HashTable<IR::LabelInstr*> *labelMap;
    Lowerer * lowerer;
    IR::Instr * instrFirst;
    IR::Instr * instrLast;
    BOOL fRetargetClonedBranch;
    JitArenaAllocator *alloc;
    bool clonedInstrGetOrigArgSlotSym;
};

/*
* This class keeps track of various information required for Stack Arguments optimization with formals.
*/
class StackArgWithFormalsTracker
{
private:
    BVSparse<JitArenaAllocator> *  formalsArraySyms;    //Tracks Formal parameter Array - Is this Bv required explicitly?
    StackSym**                     formalsIndexToStackSymMap; //Tracks the stack sym for each formal
    StackSym*                      m_scopeObjSym;   // Tracks the stack sym for the scope object that is created.
    JitArenaAllocator*             alloc;

public:
    StackArgWithFormalsTracker(JitArenaAllocator *alloc):
        formalsArraySyms(nullptr),
        formalsIndexToStackSymMap(nullptr),
        m_scopeObjSym(nullptr),
        alloc(alloc)
    {TRACE_IT(2669);
    }

    BVSparse<JitArenaAllocator> * GetFormalsArraySyms();
    void SetFormalsArraySyms(SymID symId);

    StackSym ** GetFormalsIndexToStackSymMap();
    void SetStackSymInFormalsIndexMap(StackSym * sym, Js::ArgSlot formalsIndex, Js::ArgSlot formalsCount);

    void SetScopeObjSym(StackSym * sym);
    StackSym * GetScopeObjSym();
};

typedef JsUtil::Pair<uint32, IR::LabelInstr*> YieldOffsetResumeLabel;
typedef JsUtil::List<YieldOffsetResumeLabel, JitArenaAllocator> YieldOffsetResumeLabelList;
typedef HashTable<uint32, JitArenaAllocator> SlotArrayCheckTable;
struct FrameDisplayCheckRecord
{
    SlotArrayCheckTable *table;
    uint32               slotId;

    FrameDisplayCheckRecord() : table(nullptr), slotId((uint32)-1) {TRACE_IT(2670);}
};
typedef HashTable<FrameDisplayCheckRecord*, JitArenaAllocator> FrameDisplayCheckTable;

class Func
{
public:
    Func(JitArenaAllocator *alloc, JITTimeWorkItem * workItem,
        ThreadContextInfo * threadContextInfo,
        ScriptContextInfo * scriptContextInfo,
        JITOutputIDL * outputData,
        Js::EntryPointInfo* epInfo,
        const FunctionJITRuntimeInfo *const runtimeInfo,
        JITTimePolymorphicInlineCacheInfo * const polymorphicInlineCacheInfo, void * const codeGenAllocators,
#if !FLOATVAR
        CodeGenNumberAllocator * numberAllocator,
#endif
        Js::ScriptContextProfiler *const codeGenProfiler, const bool isBackgroundJIT, Func * parentFunc = nullptr,
        uint postCallByteCodeOffset = Js::Constants::NoByteCodeOffset,
        Js::RegSlot returnValueRegSlot = Js::Constants::NoRegister, const bool isInlinedConstructor = false,
        Js::ProfileId callSiteIdInParentFunc = UINT16_MAX, bool isGetterSetter = false);
public:
    void * const GetCodeGenAllocators()
    {TRACE_IT(2671);
        return this->GetTopFunc()->m_codeGenAllocators;
    }
    InProcCodeGenAllocators * const GetInProcCodeGenAllocators()
    {TRACE_IT(2672);
        Assert(!JITManager::GetJITManager()->IsJITServer());
        return reinterpret_cast<InProcCodeGenAllocators*>(this->GetTopFunc()->m_codeGenAllocators);
    }
#if ENABLE_OOP_NATIVE_CODEGEN
    OOPCodeGenAllocators * const GetOOPCodeGenAllocators()
    {TRACE_IT(2673);
        Assert(JITManager::GetJITManager()->IsJITServer());
        return reinterpret_cast<OOPCodeGenAllocators*>(this->GetTopFunc()->m_codeGenAllocators);
    }
#endif
    NativeCodeData::Allocator *GetNativeCodeDataAllocator()
    {TRACE_IT(2674);
        return &this->GetTopFunc()->nativeCodeDataAllocator;
    }
    NativeCodeData::Allocator *GetTransferDataAllocator()
    {TRACE_IT(2675);
        return &this->GetTopFunc()->transferDataAllocator;
    }
#if !FLOATVAR
    CodeGenNumberAllocator * GetNumberAllocator()
    {TRACE_IT(2676);
        return this->numberAllocator;
    }
#endif

#if !FLOATVAR
    XProcNumberPageSegmentImpl* GetXProcNumberAllocator()
    {TRACE_IT(2677);
        if (this->GetJITOutput()->GetOutputData()->numberPageSegments == nullptr)
        {TRACE_IT(2678);
            XProcNumberPageSegmentImpl* seg = (XProcNumberPageSegmentImpl*)midl_user_allocate(sizeof(XProcNumberPageSegment));
            if (seg == nullptr)
            {TRACE_IT(2679);
                Js::Throw::OutOfMemory();
            }
            this->GetJITOutput()->GetOutputData()->numberPageSegments = new (seg) XProcNumberPageSegmentImpl();
        }
        return (XProcNumberPageSegmentImpl*)this->GetJITOutput()->GetOutputData()->numberPageSegments;
    }
#endif

    Js::ScriptContextProfiler *GetCodeGenProfiler() const
    {TRACE_IT(2680);
#ifdef PROFILE_EXEC
        return m_codeGenProfiler;
#else
        return nullptr;
#endif
    }

    bool IsOOPJIT() const {TRACE_IT(2681); return JITManager::GetJITManager()->IsOOPJITEnabled(); }

    void InitLocalClosureSyms();

    bool HasAnyStackNestedFunc() const {TRACE_IT(2682); return this->hasAnyStackNestedFunc; }
    bool DoStackNestedFunc() const {TRACE_IT(2683); return this->stackNestedFunc; }
    bool DoStackFrameDisplay() const {TRACE_IT(2684); return this->stackClosure; }
    bool DoStackScopeSlots() const {TRACE_IT(2685); return this->stackClosure; }
    bool IsBackgroundJIT() const {TRACE_IT(2686); return this->m_isBackgroundJIT; }
    bool HasArgumentSlot() const {TRACE_IT(2687); return this->GetInParamsCount() != 0 && !this->IsLoopBody(); }
    bool IsLoopBody() const {TRACE_IT(2688); return m_workItem->IsLoopBody(); }
    bool IsLoopBodyInTry() const;
    bool CanAllocInPreReservedHeapPageSegment();
    void SetDoFastPaths();
    bool DoFastPaths() const {TRACE_IT(2689); Assert(this->hasCalledSetDoFastPaths); return this->m_doFastPaths; }

    bool DoLoopFastPaths() const
    {TRACE_IT(2690);
        return
            (!IsSimpleJit() || CONFIG_FLAG(NewSimpleJit)) &&
            !PHASE_OFF(Js::FastPathPhase, this) &&
            !PHASE_OFF(Js::LoopFastPathPhase, this);
    }

    bool DoGlobOpt() const
    {TRACE_IT(2691);
        return
            !PHASE_OFF(Js::GlobOptPhase, this) && !IsSimpleJit() &&
            (!GetTopFunc()->HasTry() || GetTopFunc()->CanOptimizeTryCatch());
    }

    bool DoInline() const
    {TRACE_IT(2692);
        return DoGlobOpt() && !GetTopFunc()->HasTry();
    }

    bool DoOptimizeTryCatch() const
    {TRACE_IT(2693);
        Assert(IsTopFunc());
        return DoGlobOpt();
    }

    bool CanOptimizeTryCatch() const
    {TRACE_IT(2694);
        return !this->HasFinally() && !this->m_workItem->IsLoopBody() && !PHASE_OFF(Js::OptimizeTryCatchPhase, this);
    }

    bool DoSimpleJitDynamicProfile() const;
    bool IsSimpleJit() const {TRACE_IT(2695); return m_workItem->GetJitMode() == ExecutionMode::SimpleJit; }

    JITTimeWorkItem * GetWorkItem() const
    {TRACE_IT(2696);
        return m_workItem;
    }

    ThreadContext * GetInProcThreadContext() const
    {TRACE_IT(2697);
        Assert(!IsOOPJIT());
        return (ThreadContext*)m_threadContextInfo;
    }

    ServerThreadContext* GetOOPThreadContext() const
    {TRACE_IT(2698);
        Assert(IsOOPJIT());
        return (ServerThreadContext*)m_threadContextInfo;
    }

    ThreadContextInfo * GetThreadContextInfo() const
    {TRACE_IT(2699);
        return m_threadContextInfo;
    }

    ScriptContextInfo * GetScriptContextInfo() const
    {TRACE_IT(2700);
        return m_scriptContextInfo;
    }

    JITOutput* GetJITOutput()
    {TRACE_IT(2701);
        return &m_output;
    }

    const JITOutput* GetJITOutput() const
    {TRACE_IT(2702);
        return &m_output;
    }

    const JITTimeFunctionBody * const GetJITFunctionBody() const
    {TRACE_IT(2703);
        return m_workItem->GetJITFunctionBody();
    }

    Js::EntryPointInfo* GetInProcJITEntryPointInfo() const
    {TRACE_IT(2704);
        Assert(!IsOOPJIT());
        return m_entryPointInfo;
    }

    char16* GetDebugNumberSet(wchar(&bufferToWriteTo)[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE]) const
    {TRACE_IT(2705);
        return m_workItem->GetJITTimeInfo()->GetDebugNumberSet(bufferToWriteTo);
    }

    void TryCodegen();
    static void Codegen(JitArenaAllocator *alloc, JITTimeWorkItem * workItem,
        ThreadContextInfo * threadContextInfo,
        ScriptContextInfo * scriptContextInfo,
        JITOutputIDL * outputData,
        Js::EntryPointInfo* epInfo, // for in-proc jit only
        const FunctionJITRuntimeInfo *const runtimeInfo,
        JITTimePolymorphicInlineCacheInfo * const polymorphicInlineCacheInfo, void * const codeGenAllocators,
#if !FLOATVAR
        CodeGenNumberAllocator * numberAllocator,
#endif
        Js::ScriptContextProfiler *const codeGenProfiler, const bool isBackgroundJIT);

    int32 StackAllocate(int size);
    int32 StackAllocate(StackSym *stackSym, int size);
    void SetArgOffset(StackSym *stackSym, int32 offset);

    int32 GetLocalVarSlotOffset(int32 slotId);
    int32 GetHasLocalVarChangedOffset();
    bool IsJitInDebugMode();
    bool IsNonTempLocalVar(uint32 slotIndex);
    void OnAddSym(Sym* sym);

    uint GetLocalFunctionId() const
    {TRACE_IT(2706);
        return m_workItem->GetJITTimeInfo()->GetLocalFunctionId();
    }

    uint GetSourceContextId() const
    {TRACE_IT(2707);
        return m_workItem->GetJITFunctionBody()->GetSourceContextId();
    }


#ifdef MD_GROW_LOCALS_AREA_UP
    void AjustLocalVarSlotOffset();
#endif

    bool DoGlobOptsForGeneratorFunc() const;

    static int32 AdjustOffsetValue(int32 offset);

    static inline uint32 GetDiagLocalSlotSize()
    {TRACE_IT(2708);
        // For the debug purpose we will have fixed stack slot size
        // We will allocated the 8 bytes for each variable.
        return MachDouble;
    }

#ifdef DBG
    // The pattern used to pre-fill locals for CHK builds.
    // When we restore bailout values we check for this pattern, this is how we assert for non-initialized variables/garbage.

static const uint32 c_debugFillPattern4 = 0xcececece;
static const unsigned __int64 c_debugFillPattern8 = 0xcececececececece;

#if defined(_M_IX86) || defined (_M_ARM)
    static const uint32 c_debugFillPattern = c_debugFillPattern4;
#elif defined(_M_X64) || defined(_M_ARM64)
    static const unsigned __int64 c_debugFillPattern = c_debugFillPattern8;
#else
#error unsupported platform
#endif

#endif

    bool IsSIMDEnabled() const
    {TRACE_IT(2709);
        return GetScriptContextInfo()->IsSIMDEnabled();
    }
    uint32 GetInstrCount();
    inline Js::ScriptContext* GetScriptContext() const
    {TRACE_IT(2710);
        Assert(!IsOOPJIT());

        return static_cast<Js::ScriptContext*>(this->GetScriptContextInfo());
    }
    void NumberInstrs();
    bool IsTopFunc() const {TRACE_IT(2711); return this->parentFunc == nullptr; }
    Func const * GetTopFunc() const;
    Func * GetTopFunc();

    void SetFirstArgOffset(IR::Instr* inlineeStart);

    uint GetFunctionNumber() const
    {TRACE_IT(2712);
        return m_workItem->GetJITFunctionBody()->GetFunctionNumber();
    }

    BOOL HasTry() const
    {TRACE_IT(2713);
        Assert(this->IsTopFunc());
        return this->GetJITFunctionBody()->HasTry();
    }
    bool HasFinally() const
    {TRACE_IT(2714);
        Assert(this->IsTopFunc());
        return this->GetJITFunctionBody()->HasFinally();
    }
    bool HasThis() const
    {TRACE_IT(2715);
        Assert(this->IsTopFunc());
        Assert(this->GetJITFunctionBody());     // For now we always have a function body
        return this->GetJITFunctionBody()->HasThis();
    }
    Js::ArgSlot GetInParamsCount() const
    {TRACE_IT(2716);
        Assert(this->IsTopFunc());
        return this->GetJITFunctionBody()->GetInParamsCount();
    }
    bool IsGlobalFunc() const
    {TRACE_IT(2717);
        Assert(this->IsTopFunc());
        return this->GetJITFunctionBody()->IsGlobalFunc();
    }
    uint16 GetArgUsedForBranch() const;

    intptr_t GetWeakFuncRef() const;

    const FunctionJITRuntimeInfo * GetRuntimeInfo() const {TRACE_IT(2718); return m_runtimeInfo; }
    bool IsLambda() const
    {TRACE_IT(2719);
        Assert(this->IsTopFunc());
        Assert(this->GetJITFunctionBody());     // For now we always have a function body
        return this->GetJITFunctionBody()->IsLambda();
    }
    bool IsTrueLeaf() const
    {TRACE_IT(2720);
        return !GetHasCalls() && !GetHasImplicitCalls();
    }

    StackSym *EnsureLoopParamSym();

    void UpdateForInLoopMaxDepth(uint forInLoopMaxDepth);
    int GetForInEnumeratorArrayOffset() const;

    StackSym *GetFuncObjSym() const {TRACE_IT(2721); return m_funcObjSym; }
    void SetFuncObjSym(StackSym *sym) {TRACE_IT(2722); m_funcObjSym = sym; }

    StackSym *GetJavascriptLibrarySym() const {TRACE_IT(2723); return m_javascriptLibrarySym; }
    void SetJavascriptLibrarySym(StackSym *sym) {TRACE_IT(2724); m_javascriptLibrarySym = sym; }

    StackSym *GetScriptContextSym() const {TRACE_IT(2725); return m_scriptContextSym; }
    void SetScriptContextSym(StackSym *sym) {TRACE_IT(2726); m_scriptContextSym = sym; }

    StackSym *GetFunctionBodySym() const {TRACE_IT(2727); return m_functionBodySym; }
    void SetFunctionBodySym(StackSym *sym) {TRACE_IT(2728); m_functionBodySym = sym; }

    StackSym *GetLocalClosureSym() const {TRACE_IT(2729); return m_localClosureSym; }
    void SetLocalClosureSym(StackSym *sym) {TRACE_IT(2730); m_localClosureSym = sym; }

    StackSym *GetParamClosureSym() const {TRACE_IT(2731); return m_paramClosureSym; }
    void SetParamClosureSym(StackSym *sym) {TRACE_IT(2732); m_paramClosureSym = sym; }

    StackSym *GetLocalFrameDisplaySym() const {TRACE_IT(2733); return m_localFrameDisplaySym; }
    void SetLocalFrameDisplaySym(StackSym *sym) {TRACE_IT(2734); m_localFrameDisplaySym = sym; }

    intptr_t GetJittedLoopIterationsSinceLastBailoutAddress() const;
    void EnsurePinnedTypeRefs();
    void PinTypeRef(void* typeRef);

    void EnsureSingleTypeGuards();
    Js::JitTypePropertyGuard* GetOrCreateSingleTypeGuard(intptr_t typeAddr);

    void  EnsureEquivalentTypeGuards();
    Js::JitEquivalentTypeGuard * CreateEquivalentTypeGuard(JITTypeHolder type, uint32 objTypeSpecFldId);

    void ThrowIfScriptClosed();
    void EnsurePropertyGuardsByPropertyId();
    void EnsureCtorCachesByPropertyId();

    void LinkGuardToPropertyId(Js::PropertyId propertyId, Js::JitIndexedPropertyGuard* guard);
    void LinkCtorCacheToPropertyId(Js::PropertyId propertyId, JITTimeConstructorCache* cache);

    JITTimeConstructorCache * GetConstructorCache(const Js::ProfileId profiledCallSiteId);
    void SetConstructorCache(const Js::ProfileId profiledCallSiteId, JITTimeConstructorCache* constructorCache);

    void EnsurePropertiesWrittenTo();

    void EnsureCallSiteToArgumentsOffsetFixupMap();

    IR::LabelInstr * EnsureFuncStartLabel();
    IR::LabelInstr * GetFuncStartLabel();
    IR::LabelInstr * EnsureFuncEndLabel();
    IR::LabelInstr * GetFuncEndLabel();

#ifdef _M_X64
    void SetSpillSize(int32 spillSize)
    {TRACE_IT(2735);
        m_spillSize = spillSize;
    }

    int32 GetSpillSize()
    {TRACE_IT(2736);
        return m_spillSize;
    }

    void SetArgsSize(int32 argsSize)
    {TRACE_IT(2737);
        m_argsSize = argsSize;
    }

    int32 GetArgsSize()
    {TRACE_IT(2738);
        return m_argsSize;
    }

    void SetSavedRegSize(int32 savedRegSize)
    {TRACE_IT(2739);
        m_savedRegSize = savedRegSize;
    }

    int32 GetSavedRegSize()
    {TRACE_IT(2740);
        return m_savedRegSize;
    }
#endif

    bool IsInlinee() const
    {TRACE_IT(2741);
        Assert(m_inlineeFrameStartSym ? (m_inlineeFrameStartSym->m_offset != -1) : true);
        return m_inlineeFrameStartSym != nullptr;
    }

    void SetInlineeFrameStartSym(StackSym *sym)
    {TRACE_IT(2742);
        Assert(m_inlineeFrameStartSym == nullptr);
        m_inlineeFrameStartSym = sym;
    }

    IR::SymOpnd *GetInlineeArgCountSlotOpnd()
    {TRACE_IT(2743);
        return GetInlineeOpndAtOffset(Js::Constants::InlineeMetaArgIndex_Argc * MachPtr);
    }

    IR::SymOpnd *GetNextInlineeFrameArgCountSlotOpnd()
    {TRACE_IT(2744);
        Assert(!this->m_hasInlineArgsOpt);
        return GetInlineeOpndAtOffset((Js::Constants::InlineeMetaArgCount + actualCount) * MachPtr);
    }

    IR::SymOpnd *GetInlineeFunctionObjectSlotOpnd()
    {TRACE_IT(2745);
        Assert(!this->m_hasInlineArgsOpt);
        return GetInlineeOpndAtOffset(Js::Constants::InlineeMetaArgIndex_FunctionObject * MachPtr);
    }

    IR::SymOpnd *GetInlineeArgumentsObjectSlotOpnd()
    {TRACE_IT(2746);
        return GetInlineeOpndAtOffset(Js::Constants::InlineeMetaArgIndex_ArgumentsObject * MachPtr);
    }

    IR::SymOpnd *GetInlineeArgvSlotOpnd()
    {TRACE_IT(2747);
        Assert(!this->m_hasInlineArgsOpt);
        return GetInlineeOpndAtOffset(Js::Constants::InlineeMetaArgIndex_Argv * MachPtr);
    }

    bool IsInlined() const
    {TRACE_IT(2748);
        return this->parentFunc != nullptr;
    }

    bool IsInlinedConstructor() const
    {TRACE_IT(2749);
        return this->isInlinedConstructor;
    }
    bool IsTJLoopBody()const {TRACE_IT(2750);
        return this->isTJLoopBody;
    }

    Js::Var AllocateNumber(double value);

    JITObjTypeSpecFldInfo* GetObjTypeSpecFldInfo(const uint index) const;
    JITObjTypeSpecFldInfo* GetGlobalObjTypeSpecFldInfo(uint propertyInfoId) const;

    // Gets an inline cache pointer to use in jitted code. Cached data may not be stable while jitting. Does not return null.
    intptr_t GetRuntimeInlineCache(const uint index) const;
    JITTimePolymorphicInlineCache * GetRuntimePolymorphicInlineCache(const uint index) const;
    byte GetPolyCacheUtil(const uint index) const;
    byte GetPolyCacheUtilToInitialize(const uint index) const;

#if defined(_M_ARM32_OR_ARM64)
    RegNum GetLocalsPointer() const;
#endif

#if DBG_DUMP
    void                Dump(IRDumpFlags flags);
    void                Dump();
    void                DumpHeader();
#endif

#if DBG_DUMP || defined(ENABLE_IR_VIEWER)
    LPCSTR GetVtableName(INT_PTR address);
#endif
#if DBG_DUMP | defined(VTUNE_PROFILING)
    bool DoRecordNativeMap() const;
#endif


#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void DumpFullFunctionName();
#endif

public:
    JitArenaAllocator *    m_alloc;
    const FunctionJITRuntimeInfo *const m_runtimeInfo;
    ThreadContextInfo * m_threadContextInfo;
    ScriptContextInfo * m_scriptContextInfo;
    JITTimeWorkItem * m_workItem;
    JITTimePolymorphicInlineCacheInfo *const m_polymorphicInlineCacheInfo;

    // This indicates how many constructor caches we inserted into the constructorCaches array, not the total size of the array.
    uint constructorCacheCount;

    // This array maps callsite ids to constructor caches. The size corresponds to the number of callsites in the function.
    JITTimeConstructorCache** constructorCaches;

    typedef JsUtil::BaseHashSet<void*, JitArenaAllocator, PowerOf2SizePolicy> TypeRefSet;
    TypeRefSet* pinnedTypeRefs;

    typedef JsUtil::BaseDictionary<intptr_t, Js::JitTypePropertyGuard*, JitArenaAllocator, PowerOf2SizePolicy> TypePropertyGuardDictionary;
    TypePropertyGuardDictionary* singleTypeGuards;

    typedef SListCounted<Js::JitEquivalentTypeGuard*> EquivalentTypeGuardList;
    EquivalentTypeGuardList* equivalentTypeGuards;

    typedef JsUtil::BaseHashSet<Js::JitIndexedPropertyGuard*, JitArenaAllocator, PowerOf2SizePolicy> IndexedPropertyGuardSet;
    typedef JsUtil::BaseDictionary<Js::PropertyId, IndexedPropertyGuardSet*, JitArenaAllocator, PowerOf2SizePolicy> PropertyGuardByPropertyIdMap;
    PropertyGuardByPropertyIdMap* propertyGuardsByPropertyId;

    typedef JsUtil::BaseHashSet<intptr_t, JitArenaAllocator, PowerOf2SizePolicy> CtorCacheSet;
    typedef JsUtil::BaseDictionary<Js::PropertyId, CtorCacheSet*, JitArenaAllocator, PowerOf2SizePolicy> CtorCachesByPropertyIdMap;
    CtorCachesByPropertyIdMap* ctorCachesByPropertyId;

    typedef JsUtil::BaseDictionary<Js::ProfileId, int32, JitArenaAllocator, PrimeSizePolicy> CallSiteToArgumentsOffsetFixupMap;
    CallSiteToArgumentsOffsetFixupMap* callSiteToArgumentsOffsetFixupMap;
    int indexedPropertyGuardCount;

    typedef JsUtil::BaseHashSet<Js::PropertyId, JitArenaAllocator> PropertyIdSet;
    PropertyIdSet* propertiesWrittenTo;
    PropertyIdSet lazyBailoutProperties;
    bool anyPropertyMayBeWrittenTo;

    SlotArrayCheckTable *slotArrayCheckTable;
    FrameDisplayCheckTable *frameDisplayCheckTable;

    IR::Instr *         m_headInstr;
    IR::Instr *         m_exitInstr;
    IR::Instr *         m_tailInstr;
#ifdef _M_X64
    int32               m_spillSize;
    int32               m_argsSize;
    int32               m_savedRegSize;
    PrologEncoder       m_prologEncoder;
#endif

    SymTable *          m_symTable;
    StackSym *          m_loopParamSym;
    StackSym *          m_funcObjSym;
    StackSym *          m_javascriptLibrarySym;
    StackSym *          m_scriptContextSym;
    StackSym *          m_functionBodySym;
    StackSym *          m_localClosureSym;
    StackSym *          m_paramClosureSym;
    StackSym *          m_localFrameDisplaySym;
    StackSym *          m_bailoutReturnValueSym;
    StackSym *          m_hasBailedOutSym;
    uint                m_forInLoopMaxDepth;
    uint                m_forInLoopBaseDepth;
    int32               m_forInEnumeratorArrayOffset;

    int32               m_localStackHeight;
    uint                frameSize;
    uint32              inlineDepth;
    uint32              postCallByteCodeOffset;
    Js::RegSlot         returnValueRegSlot;
    Js::ArgSlot         actualCount;
    int32               firstActualStackOffset;
    uint32              tryCatchNestingLevel;
    uint32              m_totalJumpTableSizeInBytesForSwitchStatements;
#if defined(_M_ARM32_OR_ARM64)
    //Offset to arguments from sp + m_localStackHeight;
    //For non leaf functions this is (callee saved register count + LR + R11) * MachRegInt
    //For leaf functions this is (saved registers) * MachRegInt
    int32               m_ArgumentsOffset;
    UnwindInfoManager   m_unwindInfo;
    IR::LabelInstr *    m_epilogLabel;
#endif
    IR::LabelInstr *    m_funcStartLabel;
    IR::LabelInstr *    m_funcEndLabel;

    // Keep track of the maximum number of args on the stack.
    uint32              m_argSlotsForFunctionsCalled;
#if DBG
    uint32              m_callSiteCount;
#endif
    FlowGraph *         m_fg;
    unsigned int        m_labelCount;
    BitVector           m_regsUsed;
    StackSym *          tempSymDouble;
    StackSym *          tempSymBool;
    uint32              loopCount;
    Js::ProfileId       callSiteIdInParentFunc;
    bool                m_isLeaf: 1;  // This is set in the IRBuilder and might be inaccurate after inlining
    bool                m_hasCalls: 1; // This is more accurate compared to m_isLeaf
    bool                m_hasInlineArgsOpt : 1;
    bool                m_doFastPaths : 1;
    bool                hasBailout: 1;
    bool                hasBailoutInEHRegion : 1;
    bool                hasStackArgs: 1;
    bool                hasImplicitParamLoad : 1; // True if there is a load of CallInfo, FunctionObject
    bool                hasThrow : 1;
    bool                hasUnoptimizedArgumentsAcccess : 1; // True if there are any arguments access beyond the simple case of this.apply pattern
    bool                m_canDoInlineArgsOpt : 1;
    bool                hasApplyTargetInlining:1;
    bool                isGetterSetter : 1;
    const bool          isInlinedConstructor: 1;
    bool                hasImplicitCalls: 1;
    bool                hasTempObjectProducingInstr:1; // At least one instruction which can produce temp object
    bool                isTJLoopBody : 1;
    bool                isFlowGraphValid : 1;
#if DBG
    bool                hasCalledSetDoFastPaths:1;
    bool                isPostLower:1;
    bool                isPostRegAlloc:1;
    bool                isPostPeeps:1;
    bool                isPostLayout:1;
    bool                isPostFinalLower:1;

    typedef JsUtil::Stack<Js::Phase> CurrentPhasesStack;
    CurrentPhasesStack  currentPhases;

    bool                IsInPhase(Js::Phase tag);
#endif

    void                BeginPhase(Js::Phase tag);
    void                EndPhase(Js::Phase tag, bool dump = true);
    void                EndProfiler(Js::Phase tag);

    void                BeginClone(Lowerer *lowerer, JitArenaAllocator *alloc);
    void                EndClone();
    Cloner *            GetCloner() const {TRACE_IT(2751); return GetTopFunc()->m_cloner; }
    InstrMap *          GetCloneMap() const {TRACE_IT(2752); return GetTopFunc()->m_cloneMap; }
    void                ClearCloneMap() {TRACE_IT(2753); Assert(this->IsTopFunc()); this->m_cloneMap = nullptr; }

    bool                HasByteCodeOffset() const {TRACE_IT(2754); return !this->GetTopFunc()->hasInstrNumber; }
    bool                DoMaintainByteCodeOffset() const {TRACE_IT(2755); return this->HasByteCodeOffset() && this->GetTopFunc()->maintainByteCodeOffset; }
    void                StopMaintainByteCodeOffset() {TRACE_IT(2756); this->GetTopFunc()->maintainByteCodeOffset = false; }
    Func *              GetParentFunc() const {TRACE_IT(2757); return parentFunc; }
    uint                GetMaxInlineeArgOutCount() const {TRACE_IT(2758); return maxInlineeArgOutCount; }
    void                UpdateMaxInlineeArgOutCount(uint inlineeArgOutCount);
#if DBG_DUMP
    ptrdiff_t           m_codeSize;
#endif
    bool                GetHasCalls() const {TRACE_IT(2759); return this->m_hasCalls; }
    void                SetHasCalls() {TRACE_IT(2760); this->m_hasCalls = true; }
    void                SetHasCallsOnSelfAndParents()
    {TRACE_IT(2761);
                        Func *curFunc = this;
                        while (curFunc)
                        {TRACE_IT(2762);
                            curFunc->SetHasCalls();
                            curFunc = curFunc->GetParentFunc();
                        }
    }
    void                SetHasInstrNumber(bool has) {TRACE_IT(2763); this->GetTopFunc()->hasInstrNumber = has; }
    bool                HasInstrNumber() const {TRACE_IT(2764); return this->GetTopFunc()->hasInstrNumber; }
    bool                HasInlinee() const {TRACE_IT(2765); Assert(this->IsTopFunc()); return this->hasInlinee; }
    void                SetHasInlinee() {TRACE_IT(2766); Assert(this->IsTopFunc()); this->hasInlinee = true; }

    bool                GetThisOrParentInlinerHasArguments() const {TRACE_IT(2767); return thisOrParentInlinerHasArguments; }

    bool                GetHasStackArgs()
    {TRACE_IT(2768);
                        bool isStackArgOptDisabled = false;
                        if (HasProfileInfo())
                        {TRACE_IT(2769);
                            isStackArgOptDisabled = GetReadOnlyProfileInfo()->IsStackArgOptDisabled();
                        }
                        return this->hasStackArgs && !isStackArgOptDisabled && !PHASE_OFF1(Js::StackArgOptPhase);
    }
    void                SetHasStackArgs(bool has) {TRACE_IT(2770); this->hasStackArgs = has;}

    bool                IsStackArgsEnabled()
    {TRACE_IT(2771);
                        Func* curFunc = this;
                        bool isStackArgsEnabled = GetJITFunctionBody()->UsesArgumentsObject() && curFunc->GetHasStackArgs();
                        Func * topFunc = curFunc->GetTopFunc();
                        if (topFunc != nullptr)
                        {TRACE_IT(2772);
                            isStackArgsEnabled = isStackArgsEnabled && topFunc->GetHasStackArgs();
                        }
                        return isStackArgsEnabled;
    }

    bool                GetHasImplicitParamLoad() const {TRACE_IT(2773); return this->hasImplicitParamLoad; }
    void                SetHasImplicitParamLoad() {TRACE_IT(2774); this->hasImplicitParamLoad = true; }

    bool                GetHasThrow() const {TRACE_IT(2775); return this->hasThrow; }
    void                SetHasThrow() {TRACE_IT(2776); this->hasThrow = true; }

    bool                GetHasUnoptimizedArgumentsAcccess() const {TRACE_IT(2777); return this->hasUnoptimizedArgumentsAcccess; }
    void                SetHasUnoptimizedArgumentsAccess(bool args)
    {TRACE_IT(2778);
                        // Once set to 'true' make sure this does not become false
                        if (!this->hasUnoptimizedArgumentsAcccess)
                        {TRACE_IT(2779);
                            this->hasUnoptimizedArgumentsAcccess = args;
                        }

                        if (args)
                        {TRACE_IT(2780);
                            Func *curFunc = this->GetParentFunc();
                            while (curFunc)
                            {TRACE_IT(2781);
                                curFunc->hasUnoptimizedArgumentsAcccess = args;
                                curFunc = curFunc->GetParentFunc();
                            }
                        }
    }

    void               DisableCanDoInlineArgOpt()
    {TRACE_IT(2782);
                        Func* curFunc = this;
                        while (curFunc)
                        {TRACE_IT(2783);
                            curFunc->m_canDoInlineArgsOpt = false;
                            curFunc->m_hasInlineArgsOpt = false;
                            curFunc = curFunc->GetParentFunc();
                        }
    }

    bool                GetHasApplyTargetInlining() const {TRACE_IT(2784); return this->hasApplyTargetInlining;}
    void                SetHasApplyTargetInlining() {TRACE_IT(2785); this->hasApplyTargetInlining = true;}

    bool                GetHasMarkTempObjects() const {TRACE_IT(2786); return this->hasMarkTempObjects; }
    void                SetHasMarkTempObjects() {TRACE_IT(2787); this->hasMarkTempObjects = true; }

    bool                GetHasNonSimpleParams() const {TRACE_IT(2788); return this->hasNonSimpleParams; }
    void                SetHasNonSimpleParams() {TRACE_IT(2789); this->hasNonSimpleParams = true; }

    bool                GetHasImplicitCalls() const {TRACE_IT(2790); return this->hasImplicitCalls;}
    void                SetHasImplicitCalls(bool has) {TRACE_IT(2791); this->hasImplicitCalls = has;}
    void                SetHasImplicitCallsOnSelfAndParents()
                        {TRACE_IT(2792);
                            this->SetHasImplicitCalls(true);
                            Func *curFunc = this->GetParentFunc();
                            while (curFunc && !curFunc->IsTopFunc())
                            {TRACE_IT(2793);
                                curFunc->SetHasImplicitCalls(true);
                                curFunc = curFunc->GetParentFunc();
                            }
                        }

    bool                GetHasTempObjectProducingInstr() const {TRACE_IT(2794); return this->hasTempObjectProducingInstr; }
    void                SetHasTempObjectProducingInstr(bool has) {TRACE_IT(2795); this->hasTempObjectProducingInstr = has; }

    const JITTimeProfileInfo * GetReadOnlyProfileInfo() const {TRACE_IT(2796); return GetJITFunctionBody()->GetReadOnlyProfileInfo(); }
    bool                HasProfileInfo() const {TRACE_IT(2797); return GetJITFunctionBody()->HasProfileInfo(); }
    bool                HasArrayInfo()
    {TRACE_IT(2798);
        const auto top = this->GetTopFunc();
        return this->HasProfileInfo() && this->GetWeakFuncRef() && !(top->HasTry() && !top->DoOptimizeTryCatch()) &&
            top->DoGlobOpt() && !PHASE_OFF(Js::LoopFastPathPhase, top);
    }

    static Js::BuiltinFunction GetBuiltInIndex(IR::Opnd* opnd)
    {TRACE_IT(2799);
        Assert(opnd);
        Js::BuiltinFunction index;
        if (opnd->IsRegOpnd())
        {TRACE_IT(2800);
            index = opnd->AsRegOpnd()->m_sym->m_builtInIndex;
        }
        else if (opnd->IsSymOpnd())
        {TRACE_IT(2801);
            PropertySym *propertySym = opnd->AsSymOpnd()->m_sym->AsPropertySym();
            index = Js::JavascriptLibrary::GetBuiltinFunctionForPropId(propertySym->m_propertyId);
        }
        else
        {TRACE_IT(2802);
            index = Js::BuiltinFunction::None;
        }
        return index;
    }

    static bool IsBuiltInInlinedInLowerer(IR::Opnd* opnd)
    {TRACE_IT(2803);
        Assert(opnd);
        Js::BuiltinFunction index = Func::GetBuiltInIndex(opnd);
        switch (index)
        {
        case Js::BuiltinFunction::JavascriptString_CharAt:
        case Js::BuiltinFunction::JavascriptString_CharCodeAt:
        case Js::BuiltinFunction::JavascriptString_CodePointAt:
        case Js::BuiltinFunction::Math_Abs:
        case Js::BuiltinFunction::JavascriptArray_Push:
        case Js::BuiltinFunction::JavascriptString_Replace:
            return true;

        default:
            return false;
        }
    }

    void AddYieldOffsetResumeLabel(uint32 offset, IR::LabelInstr* label)
    {TRACE_IT(2804);
        m_yieldOffsetResumeLabelList->Add(YieldOffsetResumeLabel(offset, label));
    }

    template <typename Fn>
    void MapYieldOffsetResumeLabels(Fn fn)
    {TRACE_IT(2805);
        m_yieldOffsetResumeLabelList->Map(fn);
    }

    template <typename Fn>
    bool MapUntilYieldOffsetResumeLabels(Fn fn)
    {TRACE_IT(2806);
        return m_yieldOffsetResumeLabelList->MapUntil(fn);
    }

    void RemoveYieldOffsetResumeLabel(const YieldOffsetResumeLabel& yorl)
    {TRACE_IT(2807);
        m_yieldOffsetResumeLabelList->Remove(yorl);
    }

    void RemoveDeadYieldOffsetResumeLabel(IR::LabelInstr* label)
    {TRACE_IT(2808);
        uint32 offset;
        bool found = m_yieldOffsetResumeLabelList->MapUntil([&offset, &label](int i, YieldOffsetResumeLabel& yorl)
        {
            if (yorl.Second() == label)
            {TRACE_IT(2809);
                offset = yorl.First();
                return true;
            }
            return false;
        });
        Assert(found);
        RemoveYieldOffsetResumeLabel(YieldOffsetResumeLabel(offset, label));
        AddYieldOffsetResumeLabel(offset, nullptr);
    }

    IR::Instr * GetFunctionEntryInsertionPoint();
    IR::IndirOpnd * GetConstantAddressIndirOpnd(intptr_t address, IR::Opnd *largeConstOpnd, IR::AddrOpndKind kind, IRType type, Js::OpCode loadOpCode);
    void MarkConstantAddressSyms(BVSparse<JitArenaAllocator> * bv);
    void DisableConstandAddressLoadHoist() {TRACE_IT(2810); canHoistConstantAddressLoad = false; }

    void AddSlotArrayCheck(IR::SymOpnd *fieldOpnd);
    void AddFrameDisplayCheck(IR::SymOpnd *fieldOpnd, uint32 slotId = (uint32)-1);

    void EnsureStackArgWithFormalsTracker();

    BOOL IsFormalsArraySym(SymID symId);
    void TrackFormalsArraySym(SymID symId);

    void TrackStackSymForFormalIndex(Js::ArgSlot formalsIndex, StackSym * sym);
    StackSym* GetStackSymForFormal(Js::ArgSlot formalsIndex);
    bool HasStackSymForFormal(Js::ArgSlot formalsIndex);

    void SetScopeObjSym(StackSym * sym);
    StackSym * GetScopeObjSym();

#if DBG
    bool                allowRemoveBailOutArgInstr;
#endif

#if defined(_M_ARM32_OR_ARM64)
    int32               GetInlineeArgumentStackSize()
    {TRACE_IT(2811);
        int32 count = this->GetMaxInlineeArgOutCount();
        if (count)
        {TRACE_IT(2812);
            return ((count + 1) * MachPtr); // +1 for the dedicated zero out argc slot
        }
        return 0;
    }
#endif

public:
    BVSparse<JitArenaAllocator> *  argObjSyms;
    BVSparse<JitArenaAllocator> *  m_nonTempLocalVars;  // Only populated in debug mode as part of IRBuilder. Used in GlobOpt and BackwardPass.
    InlineeFrameInfo*              frameInfo;
    Js::ArgSlot argInsCount;        // This count doesn't include the ArgIn instr for "this".

    uint32 m_inlineeId;

    IR::LabelInstr *    m_bailOutNoSaveLabel;

    StackSym * GetNativeCodeDataSym() const;
    void SetNativeCodeDataSym(StackSym * sym);
private:

    Js::EntryPointInfo* m_entryPointInfo; // for in-proc JIT only

    JITOutput m_output;
#ifdef PROFILE_EXEC
    Js::ScriptContextProfiler *const m_codeGenProfiler;
#endif
    Func * const        parentFunc;
    StackSym *          m_inlineeFrameStartSym;
    uint                maxInlineeArgOutCount;
    const bool          m_isBackgroundJIT;
    bool                hasInstrNumber;
    bool                maintainByteCodeOffset;
    bool                hasInlinee;
    bool                thisOrParentInlinerHasArguments;
    bool                useRuntimeStats;
    bool                stackNestedFunc;
    bool                stackClosure;
    bool                hasAnyStackNestedFunc;
    bool                hasMarkTempObjects;
    bool                hasNonSimpleParams;
    Cloner *            m_cloner;
    InstrMap *          m_cloneMap;
    NativeCodeData::Allocator       nativeCodeDataAllocator;
    NativeCodeData::Allocator       transferDataAllocator;
#if !FLOATVAR
    CodeGenNumberAllocator *        numberAllocator;
#endif
    int32           m_localVarSlotsOffset;
    int32           m_hasLocalVarChangedOffset;    // Offset on stack of 1 byte which indicates if any local var has changed.
    void * const    m_codeGenAllocators;
    YieldOffsetResumeLabelList * m_yieldOffsetResumeLabelList;
    StackArgWithFormalsTracker * stackArgWithFormalsTracker;
    JITObjTypeSpecFldInfo ** m_globalObjTypeSpecFldInfoArray;
    StackSym *CreateInlineeStackSym();
    IR::SymOpnd *GetInlineeOpndAtOffset(int32 offset);
    bool HasLocalVarSlotCreated() const {TRACE_IT(2813); return m_localVarSlotsOffset != Js::Constants::InvalidOffset; }
    void EnsureLocalVarSlots();
    StackSym * m_nativeCodeDataSym;
    SList<IR::RegOpnd *> constantAddressRegOpnd;
    IR::Instr * lastConstantAddressRegLoadInstr;
    bool canHoistConstantAddressLoad;
#if DBG
    VtableHashMap * vtableMap;
#endif
#ifdef RECYCLER_WRITE_BARRIER_JIT
public:
    Lowerer* m_lowerer;
#endif
};

class AutoCodeGenPhase
{
public:
    AutoCodeGenPhase(Func * func, Js::Phase phase) : func(func), phase(phase), dump(false), isPhaseComplete(false)
    {TRACE_IT(2814);
        func->BeginPhase(phase);
    }
    ~AutoCodeGenPhase()
    {TRACE_IT(2815);
        if(this->isPhaseComplete)
        {TRACE_IT(2816);
            func->EndPhase(phase, dump);
        }
        else
        {TRACE_IT(2817);
            //End the profiler tag
            func->EndProfiler(phase);
        }
    }
    void EndPhase(Func * func, Js::Phase phase, bool dump, bool isPhaseComplete)
    {TRACE_IT(2818);
        Assert(this->func == func);
        Assert(this->phase == phase);
        this->dump = dump && (PHASE_DUMP(Js::SimpleJitPhase, func) || !func->IsSimpleJit());
        this->isPhaseComplete = isPhaseComplete;
    }
private:
    Func * func;
    Js::Phase phase;
    bool dump;
    bool isPhaseComplete;
};
#define BEGIN_CODEGEN_PHASE(func, phase) {TRACE_IT(2819); AutoCodeGenPhase __autoCodeGen(func, phase);
#define END_CODEGEN_PHASE(func, phase) __autoCodeGen.EndPhase(func, phase, true, true); }
#define END_CODEGEN_PHASE_NO_DUMP(func, phase) __autoCodeGen.EndPhase(func, phase, false, true); }

#ifdef PERF_HINT
void WritePerfHint(PerfHints hint, Func* func, uint byteCodeOffset = Js::Constants::NoByteCodeOffset);
#endif
