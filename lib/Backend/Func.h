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
    {LOGMEIN("Func.h] 25\n");
    }

    ~Cloner()
    {LOGMEIN("Func.h] 29\n");
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
    {LOGMEIN("Func.h] 71\n");
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

    FrameDisplayCheckRecord() : table(nullptr), slotId((uint32)-1) {LOGMEIN("Func.h] 92\n");}
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
    {LOGMEIN("Func.h] 115\n");
        return this->GetTopFunc()->m_codeGenAllocators;
    }
    InProcCodeGenAllocators * const GetInProcCodeGenAllocators()
    {LOGMEIN("Func.h] 119\n");
        Assert(!JITManager::GetJITManager()->IsJITServer());
        return reinterpret_cast<InProcCodeGenAllocators*>(this->GetTopFunc()->m_codeGenAllocators);
    }
#if ENABLE_OOP_NATIVE_CODEGEN
    OOPCodeGenAllocators * const GetOOPCodeGenAllocators()
    {LOGMEIN("Func.h] 125\n");
        Assert(JITManager::GetJITManager()->IsJITServer());
        return reinterpret_cast<OOPCodeGenAllocators*>(this->GetTopFunc()->m_codeGenAllocators);
    }
#endif
    NativeCodeData::Allocator *GetNativeCodeDataAllocator()
    {LOGMEIN("Func.h] 131\n");
        return &this->GetTopFunc()->nativeCodeDataAllocator;
    }
    NativeCodeData::Allocator *GetTransferDataAllocator()
    {LOGMEIN("Func.h] 135\n");
        return &this->GetTopFunc()->transferDataAllocator;
    }
#if !FLOATVAR
    CodeGenNumberAllocator * GetNumberAllocator()
    {LOGMEIN("Func.h] 140\n");
        return this->numberAllocator;
    }
#endif

#if !FLOATVAR
    XProcNumberPageSegmentImpl* GetXProcNumberAllocator()
    {LOGMEIN("Func.h] 147\n");
        if (this->GetJITOutput()->GetOutputData()->numberPageSegments == nullptr)
        {LOGMEIN("Func.h] 149\n");
            XProcNumberPageSegmentImpl* seg = (XProcNumberPageSegmentImpl*)midl_user_allocate(sizeof(XProcNumberPageSegment));
            if (seg == nullptr)
            {LOGMEIN("Func.h] 152\n");
                Js::Throw::OutOfMemory();
            }
            this->GetJITOutput()->GetOutputData()->numberPageSegments = new (seg) XProcNumberPageSegmentImpl();
        }
        return (XProcNumberPageSegmentImpl*)this->GetJITOutput()->GetOutputData()->numberPageSegments;
    }
#endif

    Js::ScriptContextProfiler *GetCodeGenProfiler() const
    {LOGMEIN("Func.h] 162\n");
#ifdef PROFILE_EXEC
        return m_codeGenProfiler;
#else
        return nullptr;
#endif
    }

    bool IsOOPJIT() const {LOGMEIN("Func.h] 170\n"); return JITManager::GetJITManager()->IsOOPJITEnabled(); }

    void InitLocalClosureSyms();

    bool HasAnyStackNestedFunc() const {LOGMEIN("Func.h] 174\n"); return this->hasAnyStackNestedFunc; }
    bool DoStackNestedFunc() const {LOGMEIN("Func.h] 175\n"); return this->stackNestedFunc; }
    bool DoStackFrameDisplay() const {LOGMEIN("Func.h] 176\n"); return this->stackClosure; }
    bool DoStackScopeSlots() const {LOGMEIN("Func.h] 177\n"); return this->stackClosure; }
    bool IsBackgroundJIT() const {LOGMEIN("Func.h] 178\n"); return this->m_isBackgroundJIT; }
    bool HasArgumentSlot() const {LOGMEIN("Func.h] 179\n"); return this->GetInParamsCount() != 0 && !this->IsLoopBody(); }
    bool IsLoopBody() const {LOGMEIN("Func.h] 180\n"); return m_workItem->IsLoopBody(); }
    bool IsLoopBodyInTry() const;
    bool CanAllocInPreReservedHeapPageSegment();
    void SetDoFastPaths();
    bool DoFastPaths() const {LOGMEIN("Func.h] 184\n"); Assert(this->hasCalledSetDoFastPaths); return this->m_doFastPaths; }

    bool DoLoopFastPaths() const
    {LOGMEIN("Func.h] 187\n");
        return
            (!IsSimpleJit() || CONFIG_FLAG(NewSimpleJit)) &&
            !PHASE_OFF(Js::FastPathPhase, this) &&
            !PHASE_OFF(Js::LoopFastPathPhase, this);
    }

    bool DoGlobOpt() const
    {LOGMEIN("Func.h] 195\n");
        return
            !PHASE_OFF(Js::GlobOptPhase, this) && !IsSimpleJit() &&
            (!GetTopFunc()->HasTry() || GetTopFunc()->CanOptimizeTryCatch());
    }

    bool DoInline() const
    {LOGMEIN("Func.h] 202\n");
        return DoGlobOpt() && !GetTopFunc()->HasTry();
    }

    bool DoOptimizeTryCatch() const
    {LOGMEIN("Func.h] 207\n");
        Assert(IsTopFunc());
        return DoGlobOpt();
    }

    bool CanOptimizeTryCatch() const
    {LOGMEIN("Func.h] 213\n");
        return !this->HasFinally() && !this->m_workItem->IsLoopBody() && !PHASE_OFF(Js::OptimizeTryCatchPhase, this);
    }

    bool DoSimpleJitDynamicProfile() const;
    bool IsSimpleJit() const {LOGMEIN("Func.h] 218\n"); return m_workItem->GetJitMode() == ExecutionMode::SimpleJit; }

    JITTimeWorkItem * GetWorkItem() const
    {LOGMEIN("Func.h] 221\n");
        return m_workItem;
    }

    ThreadContext * GetInProcThreadContext() const
    {LOGMEIN("Func.h] 226\n");
        Assert(!IsOOPJIT());
        return (ThreadContext*)m_threadContextInfo;
    }

    ServerThreadContext* GetOOPThreadContext() const
    {LOGMEIN("Func.h] 232\n");
        Assert(IsOOPJIT());
        return (ServerThreadContext*)m_threadContextInfo;
    }

    ThreadContextInfo * GetThreadContextInfo() const
    {LOGMEIN("Func.h] 238\n");
        return m_threadContextInfo;
    }

    ScriptContextInfo * GetScriptContextInfo() const
    {LOGMEIN("Func.h] 243\n");
        return m_scriptContextInfo;
    }

    JITOutput* GetJITOutput()
    {LOGMEIN("Func.h] 248\n");
        return &m_output;
    }

    const JITOutput* GetJITOutput() const
    {LOGMEIN("Func.h] 253\n");
        return &m_output;
    }

    const JITTimeFunctionBody * const GetJITFunctionBody() const
    {LOGMEIN("Func.h] 258\n");
        return m_workItem->GetJITFunctionBody();
    }

    Js::EntryPointInfo* GetInProcJITEntryPointInfo() const
    {LOGMEIN("Func.h] 263\n");
        Assert(!IsOOPJIT());
        return m_entryPointInfo;
    }

    char16* GetDebugNumberSet(wchar(&bufferToWriteTo)[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE]) const
    {LOGMEIN("Func.h] 269\n");
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
    {LOGMEIN("Func.h] 297\n");
        return m_workItem->GetJITTimeInfo()->GetLocalFunctionId();
    }

    uint GetSourceContextId() const
    {LOGMEIN("Func.h] 302\n");
        return m_workItem->GetJITFunctionBody()->GetSourceContextId();
    }


#ifdef MD_GROW_LOCALS_AREA_UP
    void AjustLocalVarSlotOffset();
#endif

    bool DoGlobOptsForGeneratorFunc() const;

    static int32 AdjustOffsetValue(int32 offset);

    static inline uint32 GetDiagLocalSlotSize()
    {LOGMEIN("Func.h] 316\n");
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
    {LOGMEIN("Func.h] 340\n");
        return GetScriptContextInfo()->IsSIMDEnabled();
    }
    uint32 GetInstrCount();
    inline Js::ScriptContext* GetScriptContext() const
    {LOGMEIN("Func.h] 345\n");
        Assert(!IsOOPJIT());

        return static_cast<Js::ScriptContext*>(this->GetScriptContextInfo());
    }
    void NumberInstrs();
    bool IsTopFunc() const {LOGMEIN("Func.h] 351\n"); return this->parentFunc == nullptr; }
    Func const * GetTopFunc() const;
    Func * GetTopFunc();

    void SetFirstArgOffset(IR::Instr* inlineeStart);

    uint GetFunctionNumber() const
    {LOGMEIN("Func.h] 358\n");
        return m_workItem->GetJITFunctionBody()->GetFunctionNumber();
    }

    BOOL HasTry() const
    {LOGMEIN("Func.h] 363\n");
        Assert(this->IsTopFunc());
        return this->GetJITFunctionBody()->HasTry();
    }
    bool HasFinally() const
    {LOGMEIN("Func.h] 368\n");
        Assert(this->IsTopFunc());
        return this->GetJITFunctionBody()->HasFinally();
    }
    bool HasThis() const
    {LOGMEIN("Func.h] 373\n");
        Assert(this->IsTopFunc());
        Assert(this->GetJITFunctionBody());     // For now we always have a function body
        return this->GetJITFunctionBody()->HasThis();
    }
    Js::ArgSlot GetInParamsCount() const
    {LOGMEIN("Func.h] 379\n");
        Assert(this->IsTopFunc());
        return this->GetJITFunctionBody()->GetInParamsCount();
    }
    bool IsGlobalFunc() const
    {LOGMEIN("Func.h] 384\n");
        Assert(this->IsTopFunc());
        return this->GetJITFunctionBody()->IsGlobalFunc();
    }
    uint16 GetArgUsedForBranch() const;

    intptr_t GetWeakFuncRef() const;

    const FunctionJITRuntimeInfo * GetRuntimeInfo() const {LOGMEIN("Func.h] 392\n"); return m_runtimeInfo; }
    bool IsLambda() const
    {LOGMEIN("Func.h] 394\n");
        Assert(this->IsTopFunc());
        Assert(this->GetJITFunctionBody());     // For now we always have a function body
        return this->GetJITFunctionBody()->IsLambda();
    }
    bool IsTrueLeaf() const
    {LOGMEIN("Func.h] 400\n");
        return !GetHasCalls() && !GetHasImplicitCalls();
    }

    StackSym *EnsureLoopParamSym();

    void UpdateForInLoopMaxDepth(uint forInLoopMaxDepth);
    int GetForInEnumeratorArrayOffset() const;

    StackSym *GetFuncObjSym() const {LOGMEIN("Func.h] 409\n"); return m_funcObjSym; }
    void SetFuncObjSym(StackSym *sym) {LOGMEIN("Func.h] 410\n"); m_funcObjSym = sym; }

    StackSym *GetJavascriptLibrarySym() const {LOGMEIN("Func.h] 412\n"); return m_javascriptLibrarySym; }
    void SetJavascriptLibrarySym(StackSym *sym) {LOGMEIN("Func.h] 413\n"); m_javascriptLibrarySym = sym; }

    StackSym *GetScriptContextSym() const {LOGMEIN("Func.h] 415\n"); return m_scriptContextSym; }
    void SetScriptContextSym(StackSym *sym) {LOGMEIN("Func.h] 416\n"); m_scriptContextSym = sym; }

    StackSym *GetFunctionBodySym() const {LOGMEIN("Func.h] 418\n"); return m_functionBodySym; }
    void SetFunctionBodySym(StackSym *sym) {LOGMEIN("Func.h] 419\n"); m_functionBodySym = sym; }

    StackSym *GetLocalClosureSym() const {LOGMEIN("Func.h] 421\n"); return m_localClosureSym; }
    void SetLocalClosureSym(StackSym *sym) {LOGMEIN("Func.h] 422\n"); m_localClosureSym = sym; }

    StackSym *GetParamClosureSym() const {LOGMEIN("Func.h] 424\n"); return m_paramClosureSym; }
    void SetParamClosureSym(StackSym *sym) {LOGMEIN("Func.h] 425\n"); m_paramClosureSym = sym; }

    StackSym *GetLocalFrameDisplaySym() const {LOGMEIN("Func.h] 427\n"); return m_localFrameDisplaySym; }
    void SetLocalFrameDisplaySym(StackSym *sym) {LOGMEIN("Func.h] 428\n"); m_localFrameDisplaySym = sym; }

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
    {LOGMEIN("Func.h] 461\n");
        m_spillSize = spillSize;
    }

    int32 GetSpillSize()
    {LOGMEIN("Func.h] 466\n");
        return m_spillSize;
    }

    void SetArgsSize(int32 argsSize)
    {LOGMEIN("Func.h] 471\n");
        m_argsSize = argsSize;
    }

    int32 GetArgsSize()
    {LOGMEIN("Func.h] 476\n");
        return m_argsSize;
    }

    void SetSavedRegSize(int32 savedRegSize)
    {LOGMEIN("Func.h] 481\n");
        m_savedRegSize = savedRegSize;
    }

    int32 GetSavedRegSize()
    {LOGMEIN("Func.h] 486\n");
        return m_savedRegSize;
    }
#endif

    bool IsInlinee() const
    {LOGMEIN("Func.h] 492\n");
        Assert(m_inlineeFrameStartSym ? (m_inlineeFrameStartSym->m_offset != -1) : true);
        return m_inlineeFrameStartSym != nullptr;
    }

    void SetInlineeFrameStartSym(StackSym *sym)
    {LOGMEIN("Func.h] 498\n");
        Assert(m_inlineeFrameStartSym == nullptr);
        m_inlineeFrameStartSym = sym;
    }

    IR::SymOpnd *GetInlineeArgCountSlotOpnd()
    {LOGMEIN("Func.h] 504\n");
        return GetInlineeOpndAtOffset(Js::Constants::InlineeMetaArgIndex_Argc * MachPtr);
    }

    IR::SymOpnd *GetNextInlineeFrameArgCountSlotOpnd()
    {LOGMEIN("Func.h] 509\n");
        Assert(!this->m_hasInlineArgsOpt);
        return GetInlineeOpndAtOffset((Js::Constants::InlineeMetaArgCount + actualCount) * MachPtr);
    }

    IR::SymOpnd *GetInlineeFunctionObjectSlotOpnd()
    {LOGMEIN("Func.h] 515\n");
        Assert(!this->m_hasInlineArgsOpt);
        return GetInlineeOpndAtOffset(Js::Constants::InlineeMetaArgIndex_FunctionObject * MachPtr);
    }

    IR::SymOpnd *GetInlineeArgumentsObjectSlotOpnd()
    {LOGMEIN("Func.h] 521\n");
        return GetInlineeOpndAtOffset(Js::Constants::InlineeMetaArgIndex_ArgumentsObject * MachPtr);
    }

    IR::SymOpnd *GetInlineeArgvSlotOpnd()
    {LOGMEIN("Func.h] 526\n");
        Assert(!this->m_hasInlineArgsOpt);
        return GetInlineeOpndAtOffset(Js::Constants::InlineeMetaArgIndex_Argv * MachPtr);
    }

    bool IsInlined() const
    {LOGMEIN("Func.h] 532\n");
        return this->parentFunc != nullptr;
    }

    bool IsInlinedConstructor() const
    {LOGMEIN("Func.h] 537\n");
        return this->isInlinedConstructor;
    }
    bool IsTJLoopBody()const {LOGMEIN("Func.h] 540\n");
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
    Cloner *            GetCloner() const {LOGMEIN("Func.h] 715\n"); return GetTopFunc()->m_cloner; }
    InstrMap *          GetCloneMap() const {LOGMEIN("Func.h] 716\n"); return GetTopFunc()->m_cloneMap; }
    void                ClearCloneMap() {LOGMEIN("Func.h] 717\n"); Assert(this->IsTopFunc()); this->m_cloneMap = nullptr; }

    bool                HasByteCodeOffset() const {LOGMEIN("Func.h] 719\n"); return !this->GetTopFunc()->hasInstrNumber; }
    bool                DoMaintainByteCodeOffset() const {LOGMEIN("Func.h] 720\n"); return this->HasByteCodeOffset() && this->GetTopFunc()->maintainByteCodeOffset; }
    void                StopMaintainByteCodeOffset() {LOGMEIN("Func.h] 721\n"); this->GetTopFunc()->maintainByteCodeOffset = false; }
    Func *              GetParentFunc() const {LOGMEIN("Func.h] 722\n"); return parentFunc; }
    uint                GetMaxInlineeArgOutCount() const {LOGMEIN("Func.h] 723\n"); return maxInlineeArgOutCount; }
    void                UpdateMaxInlineeArgOutCount(uint inlineeArgOutCount);
#if DBG_DUMP
    ptrdiff_t           m_codeSize;
#endif
    bool                GetHasCalls() const {LOGMEIN("Func.h] 728\n"); return this->m_hasCalls; }
    void                SetHasCalls() {LOGMEIN("Func.h] 729\n"); this->m_hasCalls = true; }
    void                SetHasCallsOnSelfAndParents()
    {LOGMEIN("Func.h] 731\n");
                        Func *curFunc = this;
                        while (curFunc)
                        {LOGMEIN("Func.h] 734\n");
                            curFunc->SetHasCalls();
                            curFunc = curFunc->GetParentFunc();
                        }
    }
    void                SetHasInstrNumber(bool has) {LOGMEIN("Func.h] 739\n"); this->GetTopFunc()->hasInstrNumber = has; }
    bool                HasInstrNumber() const {LOGMEIN("Func.h] 740\n"); return this->GetTopFunc()->hasInstrNumber; }
    bool                HasInlinee() const {LOGMEIN("Func.h] 741\n"); Assert(this->IsTopFunc()); return this->hasInlinee; }
    void                SetHasInlinee() {LOGMEIN("Func.h] 742\n"); Assert(this->IsTopFunc()); this->hasInlinee = true; }

    bool                GetThisOrParentInlinerHasArguments() const {LOGMEIN("Func.h] 744\n"); return thisOrParentInlinerHasArguments; }

    bool                GetHasStackArgs()
    {LOGMEIN("Func.h] 747\n");
                        bool isStackArgOptDisabled = false;
                        if (HasProfileInfo())
                        {LOGMEIN("Func.h] 750\n");
                            isStackArgOptDisabled = GetReadOnlyProfileInfo()->IsStackArgOptDisabled();
                        }
                        return this->hasStackArgs && !isStackArgOptDisabled && !PHASE_OFF1(Js::StackArgOptPhase);
    }
    void                SetHasStackArgs(bool has) {LOGMEIN("Func.h] 755\n"); this->hasStackArgs = has;}

    bool                IsStackArgsEnabled()
    {LOGMEIN("Func.h] 758\n");
                        Func* curFunc = this;
                        bool isStackArgsEnabled = GetJITFunctionBody()->UsesArgumentsObject() && curFunc->GetHasStackArgs();
                        Func * topFunc = curFunc->GetTopFunc();
                        if (topFunc != nullptr)
                        {LOGMEIN("Func.h] 763\n");
                            isStackArgsEnabled = isStackArgsEnabled && topFunc->GetHasStackArgs();
                        }
                        return isStackArgsEnabled;
    }

    bool                GetHasImplicitParamLoad() const {LOGMEIN("Func.h] 769\n"); return this->hasImplicitParamLoad; }
    void                SetHasImplicitParamLoad() {LOGMEIN("Func.h] 770\n"); this->hasImplicitParamLoad = true; }

    bool                GetHasThrow() const {LOGMEIN("Func.h] 772\n"); return this->hasThrow; }
    void                SetHasThrow() {LOGMEIN("Func.h] 773\n"); this->hasThrow = true; }

    bool                GetHasUnoptimizedArgumentsAcccess() const {LOGMEIN("Func.h] 775\n"); return this->hasUnoptimizedArgumentsAcccess; }
    void                SetHasUnoptimizedArgumentsAccess(bool args)
    {LOGMEIN("Func.h] 777\n");
                        // Once set to 'true' make sure this does not become false
                        if (!this->hasUnoptimizedArgumentsAcccess)
                        {LOGMEIN("Func.h] 780\n");
                            this->hasUnoptimizedArgumentsAcccess = args;
                        }

                        if (args)
                        {LOGMEIN("Func.h] 785\n");
                            Func *curFunc = this->GetParentFunc();
                            while (curFunc)
                            {LOGMEIN("Func.h] 788\n");
                                curFunc->hasUnoptimizedArgumentsAcccess = args;
                                curFunc = curFunc->GetParentFunc();
                            }
                        }
    }

    void               DisableCanDoInlineArgOpt()
    {LOGMEIN("Func.h] 796\n");
                        Func* curFunc = this;
                        while (curFunc)
                        {LOGMEIN("Func.h] 799\n");
                            curFunc->m_canDoInlineArgsOpt = false;
                            curFunc->m_hasInlineArgsOpt = false;
                            curFunc = curFunc->GetParentFunc();
                        }
    }

    bool                GetHasApplyTargetInlining() const {LOGMEIN("Func.h] 806\n"); return this->hasApplyTargetInlining;}
    void                SetHasApplyTargetInlining() {LOGMEIN("Func.h] 807\n"); this->hasApplyTargetInlining = true;}

    bool                GetHasMarkTempObjects() const {LOGMEIN("Func.h] 809\n"); return this->hasMarkTempObjects; }
    void                SetHasMarkTempObjects() {LOGMEIN("Func.h] 810\n"); this->hasMarkTempObjects = true; }

    bool                GetHasNonSimpleParams() const {LOGMEIN("Func.h] 812\n"); return this->hasNonSimpleParams; }
    void                SetHasNonSimpleParams() {LOGMEIN("Func.h] 813\n"); this->hasNonSimpleParams = true; }

    bool                GetHasImplicitCalls() const {LOGMEIN("Func.h] 815\n"); return this->hasImplicitCalls;}
    void                SetHasImplicitCalls(bool has) {LOGMEIN("Func.h] 816\n"); this->hasImplicitCalls = has;}
    void                SetHasImplicitCallsOnSelfAndParents()
                        {LOGMEIN("Func.h] 818\n");
                            this->SetHasImplicitCalls(true);
                            Func *curFunc = this->GetParentFunc();
                            while (curFunc && !curFunc->IsTopFunc())
                            {LOGMEIN("Func.h] 822\n");
                                curFunc->SetHasImplicitCalls(true);
                                curFunc = curFunc->GetParentFunc();
                            }
                        }

    bool                GetHasTempObjectProducingInstr() const {LOGMEIN("Func.h] 828\n"); return this->hasTempObjectProducingInstr; }
    void                SetHasTempObjectProducingInstr(bool has) {LOGMEIN("Func.h] 829\n"); this->hasTempObjectProducingInstr = has; }

    const JITTimeProfileInfo * GetReadOnlyProfileInfo() const {LOGMEIN("Func.h] 831\n"); return GetJITFunctionBody()->GetReadOnlyProfileInfo(); }
    bool                HasProfileInfo() const {LOGMEIN("Func.h] 832\n"); return GetJITFunctionBody()->HasProfileInfo(); }
    bool                HasArrayInfo()
    {LOGMEIN("Func.h] 834\n");
        const auto top = this->GetTopFunc();
        return this->HasProfileInfo() && this->GetWeakFuncRef() && !(top->HasTry() && !top->DoOptimizeTryCatch()) &&
            top->DoGlobOpt() && !PHASE_OFF(Js::LoopFastPathPhase, top);
    }

    static Js::BuiltinFunction GetBuiltInIndex(IR::Opnd* opnd)
    {LOGMEIN("Func.h] 841\n");
        Assert(opnd);
        Js::BuiltinFunction index;
        if (opnd->IsRegOpnd())
        {LOGMEIN("Func.h] 845\n");
            index = opnd->AsRegOpnd()->m_sym->m_builtInIndex;
        }
        else if (opnd->IsSymOpnd())
        {LOGMEIN("Func.h] 849\n");
            PropertySym *propertySym = opnd->AsSymOpnd()->m_sym->AsPropertySym();
            index = Js::JavascriptLibrary::GetBuiltinFunctionForPropId(propertySym->m_propertyId);
        }
        else
        {
            index = Js::BuiltinFunction::None;
        }
        return index;
    }

    static bool IsBuiltInInlinedInLowerer(IR::Opnd* opnd)
    {LOGMEIN("Func.h] 861\n");
        Assert(opnd);
        Js::BuiltinFunction index = Func::GetBuiltInIndex(opnd);
        switch (index)
        {LOGMEIN("Func.h] 865\n");
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
    {LOGMEIN("Func.h] 880\n");
        m_yieldOffsetResumeLabelList->Add(YieldOffsetResumeLabel(offset, label));
    }

    template <typename Fn>
    void MapYieldOffsetResumeLabels(Fn fn)
    {LOGMEIN("Func.h] 886\n");
        m_yieldOffsetResumeLabelList->Map(fn);
    }

    template <typename Fn>
    bool MapUntilYieldOffsetResumeLabels(Fn fn)
    {LOGMEIN("Func.h] 892\n");
        return m_yieldOffsetResumeLabelList->MapUntil(fn);
    }

    void RemoveYieldOffsetResumeLabel(const YieldOffsetResumeLabel& yorl)
    {LOGMEIN("Func.h] 897\n");
        m_yieldOffsetResumeLabelList->Remove(yorl);
    }

    void RemoveDeadYieldOffsetResumeLabel(IR::LabelInstr* label)
    {LOGMEIN("Func.h] 902\n");
        uint32 offset;
        bool found = m_yieldOffsetResumeLabelList->MapUntil([&offset, &label](int i, YieldOffsetResumeLabel& yorl)
        {
            if (yorl.Second() == label)
            {LOGMEIN("Func.h] 907\n");
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
    void DisableConstandAddressLoadHoist() {LOGMEIN("Func.h] 921\n"); canHoistConstantAddressLoad = false; }

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
    {LOGMEIN("Func.h] 944\n");
        int32 count = this->GetMaxInlineeArgOutCount();
        if (count)
        {LOGMEIN("Func.h] 947\n");
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
    bool HasLocalVarSlotCreated() const {LOGMEIN("Func.h] 1003\n"); return m_localVarSlotsOffset != Js::Constants::InvalidOffset; }
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
    {LOGMEIN("Func.h] 1022\n");
        func->BeginPhase(phase);
    }
    ~AutoCodeGenPhase()
    {LOGMEIN("Func.h] 1026\n");
        if(this->isPhaseComplete)
        {LOGMEIN("Func.h] 1028\n");
            func->EndPhase(phase, dump);
        }
        else
        {
            //End the profiler tag
            func->EndProfiler(phase);
        }
    }
    void EndPhase(Func * func, Js::Phase phase, bool dump, bool isPhaseComplete)
    {LOGMEIN("Func.h] 1038\n");
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
#define BEGIN_CODEGEN_PHASE(func, phase) {LOGMEIN("Func.h] 1050\n"); AutoCodeGenPhase __autoCodeGen(func, phase);
#define END_CODEGEN_PHASE(func, phase) __autoCodeGen.EndPhase(func, phase, true, true); }
#define END_CODEGEN_PHASE_NO_DUMP(func, phase) __autoCodeGen.EndPhase(func, phase, false, true); }

#ifdef PERF_HINT
void WritePerfHint(PerfHints hint, Func* func, uint byteCodeOffset = Js::Constants::NoByteCodeOffset);
#endif
