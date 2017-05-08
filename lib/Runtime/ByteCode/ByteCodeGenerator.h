//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#if defined(_M_ARM32_OR_ARM64) || defined(_M_X64)
const int32 AstBytecodeRatioEstimate = 4;
#else
const int32 AstBytecodeRatioEstimate = 5;
#endif

class ByteCodeGenerator
{
private:
    Js::ScriptContext* scriptContext;
    ArenaAllocator *alloc;
    uint32 flags;
    Js::PropertyRecordList* propertyRecords;
    SList<FuncInfo*> *funcInfoStack;
    ParseNode *currentBlock;
    ParseNode *currentTopStatement;
    Scope *currentScope;
    Scope *globalScope; // the global members will be in this scope
    Js::ScopeInfo* parentScopeInfo;
    Js::ByteCodeWriter  m_writer;

    // pointer to the root function wrapper that will be invoked by the caller
    Js::ParseableFunctionInfo * pRootFunc;

    int32 maxAstSize;
    uint16 envDepth;
    uint sourceIndex;
    uint dynamicScopeCount;
    uint loopDepth;
    uint16 m_callSiteId;
    bool isBinding;
    bool trackEnvDepth;
    bool funcEscapes;
    bool inPrologue;
    bool inDestructuredPattern;
    Parser* parser; // currently active parser (used for AST transformation)

    Js::Utf8SourceInfo *m_utf8SourceInfo;

    // The stack walker won't be able to find the current function being defer parse, pass in
    // The address so we can patch it up if it is a stack function and we need to box it.
    Js::ScriptFunction ** functionRef;
public:
    // This points to the current function body which can be reused when parsing a subtree (called due to deferred parsing logic).
    Js::FunctionBody * pCurrentFunction;

    bool InDestructuredPattern() const {TRACE_IT(40692); return inDestructuredPattern; }
    void SetInDestructuredPattern(bool in) {TRACE_IT(40693); inDestructuredPattern = in; }

    bool InPrologue() const {TRACE_IT(40694); return inPrologue; }
    void SetInPrologue(bool val) {TRACE_IT(40695); inPrologue = val; }
    Parser* GetParser() {TRACE_IT(40696); return parser; }
    Js::ParseableFunctionInfo * GetRootFunc(){TRACE_IT(40697);return pRootFunc;}
    void SetRootFuncInfo(FuncInfo* funcInfo);
    // Treat the return value register like a constant register so that the byte code writer maps it to the bottom
    // of the register range.
    static const Js::RegSlot ReturnRegister = REGSLOT_TO_CONSTREG(Js::FunctionBody::ReturnValueRegSlot);
    static const Js::RegSlot RootObjectRegister = REGSLOT_TO_CONSTREG(Js::FunctionBody::RootObjectRegSlot);
    static const unsigned int DefaultArraySize = 0;  // This __must__ be '0' so that "(new Array()).length == 0"
    static const unsigned int MinArgumentsForCallOptimization = 16;
    bool forceNoNative;

    ByteCodeGenerator(Js::ScriptContext* scriptContext, Js::ScopeInfo* parentScopeInfo);

#if DBG_DUMP
    bool Trace() const
    {TRACE_IT(40698);
        return Js::Configuration::Global.flags.Trace.IsEnabled(Js::ByteCodePhase);
    }
#else
    bool Trace() const
    {TRACE_IT(40699);
        return false;
    }
#endif

    Js::ScriptContext* GetScriptContext() {TRACE_IT(40700); return scriptContext; }

    Scope *GetCurrentScope() const {TRACE_IT(40701); return currentScope; }

    void SetCurrentBlock(ParseNode *pnode) {TRACE_IT(40702); currentBlock = pnode; }
    ParseNode *GetCurrentBlock() const {TRACE_IT(40703); return currentBlock; }

    void SetCurrentTopStatement(ParseNode *pnode) {TRACE_IT(40704); currentTopStatement = pnode; }
    ParseNode *GetCurrentTopStatement() const {TRACE_IT(40705); return currentTopStatement; }

    Js::ModuleID GetModuleID() const
    {TRACE_IT(40706);
        return m_utf8SourceInfo->GetSrcInfo()->moduleID;
    }

    void SetFlags(uint32 grfscr)
    {TRACE_IT(40707);
        flags = grfscr;
    }

    uint32 GetFlags(void)
    {TRACE_IT(40708);
        return flags;
    }

    bool IsConsoleScopeEval(void)
    {TRACE_IT(40709);
        return (flags & fscrConsoleScopeEval) == fscrConsoleScopeEval;
    }

    bool IsModuleCode()
    {TRACE_IT(40710);
        return (flags & fscrIsModuleCode) == fscrIsModuleCode;
    }

    bool IsBinding() const {TRACE_IT(40711);
        return isBinding;
    }

    Js::ByteCodeWriter *Writer() {TRACE_IT(40712);
        return &m_writer;
    }

    ArenaAllocator *GetAllocator() {TRACE_IT(40713);
        return alloc;
    }

    Js::PropertyRecordList* EnsurePropertyRecordList()
    {TRACE_IT(40714);
        if (this->propertyRecords == nullptr)
        {TRACE_IT(40715);
            Recycler* recycler = this->scriptContext->GetRecycler();
            this->propertyRecords = RecyclerNew(recycler, Js::PropertyRecordList, recycler);
        }

        return this->propertyRecords;
    }

    bool IsEvalWithNoParentScopeInfo()
    {TRACE_IT(40716);
        return (flags & fscrEvalCode) && !HasParentScopeInfo();
    }

    Js::ProfileId GetNextCallSiteId(Js::OpCode op)
    {TRACE_IT(40717);
        if (m_writer.ShouldIncrementCallSiteId(op))
        {TRACE_IT(40718);
            if (m_callSiteId != Js::Constants::NoProfileId)
            {TRACE_IT(40719);
                return m_callSiteId++;
            }
        }
        return m_callSiteId;
    }

    Js::RegSlot NextVarRegister();
    Js::RegSlot NextConstRegister();
    FuncInfo *TopFuncInfo() const;

    void EnterLoop();
    void ExitLoop() {TRACE_IT(40720); loopDepth--; }
    BOOL IsInLoop() const {TRACE_IT(40721); return loopDepth > 0; }
    // TODO: per-function register assignment for env and global symbols
    void AssignRegister(Symbol *sym);
    void AddTargetStmt(ParseNode *pnodeStmt);
    Js::RegSlot AssignNullConstRegister();
    Js::RegSlot AssignUndefinedConstRegister();
    Js::RegSlot AssignTrueConstRegister();
    Js::RegSlot AssignFalseConstRegister();
    Js::RegSlot AssignThisRegister();
    Js::RegSlot AssignNewTargetRegister();
    void SetNeedEnvRegister();
    void AssignFrameObjRegister();
    void AssignFrameSlotsRegister();
    void AssignParamSlotsRegister();
    void AssignFrameDisplayRegister();

    void ProcessCapturedSym(Symbol *sym);
    void ProcessScopeWithCapturedSym(Scope *scope);
    void InitScopeSlotArray(FuncInfo * funcInfo);
    void FinalizeRegisters(FuncInfo * funcInfo, Js::FunctionBody * byteCodeFunction);
    void SetClosureRegisters(FuncInfo * funcInfo, Js::FunctionBody * byteCodeFunction);
    void EnsureSpecialScopeSlots(FuncInfo* funcInfo, Scope* scope);
    void SetHasTry(bool has);
    void SetHasFinally(bool has);
    void SetNumberOfInArgs(Js::ArgSlot argCount);
    Js::RegSlot EnregisterConstant(unsigned int constant);
    Js::RegSlot EnregisterStringConstant(IdentPtr pid);
    Js::RegSlot EnregisterDoubleConstant(double d);
    Js::RegSlot EnregisterStringTemplateCallsiteConstant(ParseNode* pnode);

    static Js::JavascriptArray* BuildArrayFromStringList(ParseNode* stringNodeList, uint arrayLength, Js::ScriptContext* scriptContext);

    bool HasParentScopeInfo() const
    {TRACE_IT(40722);
        return this->parentScopeInfo != nullptr;
    }

    Js::RegSlot EmitLdObjProto(Js::OpCode op, Js::RegSlot objReg, FuncInfo *funcInfo)
    {TRACE_IT(40723);
        // LdHomeObjProto protoReg, objReg
        // LdFuncObjProto protoReg, objReg
        Js::RegSlot protoReg = funcInfo->AcquireTmpRegister();
        this->Writer()->Reg2(op, protoReg, objReg);
        funcInfo->ReleaseTmpRegister(protoReg);
        return protoReg;
    }

    void RestoreScopeInfo(Js::ParseableFunctionInfo* funcInfo);
    FuncInfo *StartBindGlobalStatements(ParseNode *pnode);
    void AssignPropertyId(Symbol *sym, Js::ParseableFunctionInfo* functionInfo);
    void AssignPropertyId(IdentPtr pid);

    void ProcessCapturedSyms(ParseNode *pnodeFnc);

    void RecordAllIntConstants(FuncInfo * funcInfo);
    void RecordAllStrConstants(FuncInfo * funcInfo);
    void RecordAllStringTemplateCallsiteConstants(FuncInfo* funcInfo);

    // For now, this just assigns field ids for the current script.
    // Later, we will combine this information with the global field ID map.
    // This temporary code will not work if a global member is accessed both with and without a LHS.
    void AssignPropertyIds(Js::ParseableFunctionInfo* functionInfo);
    void MapCacheIdsToPropertyIds(FuncInfo *funcInfo);
    void MapReferencedPropertyIds(FuncInfo *funcInfo);
    FuncInfo *StartBindFunction(const char16 *name, uint nameLength, uint shortNameOffset, bool* pfuncExprWithName, ParseNode *pnode, Js::ParseableFunctionInfo * reuseNestedFunc);
    void EndBindFunction(bool funcExprWithName);
    void StartBindCatch(ParseNode *pnode);

    // Block scopes related functions
    template<class Fn> void IterateBlockScopedVariables(ParseNode *pnodeBlock, Fn fn);
    void InitBlockScopedContent(ParseNode *pnodeBlock, Js::DebuggerScope *debuggerScope, FuncInfo *funcInfo);

    Js::DebuggerScope* RecordStartScopeObject(ParseNode *pnodeBlock, Js::DiagExtraScopesType scopeType, Js::RegSlot scopeLocation = Js::Constants::NoRegister, int* index = nullptr);
    void RecordEndScopeObject(ParseNode *pnodeBlock);

    void EndBindCatch();
    void StartEmitFunction(ParseNode *pnodeFnc);
    void EndEmitFunction(ParseNode *pnodeFnc);
    void StartEmitBlock(ParseNode *pnodeBlock);
    void EndEmitBlock(ParseNode *pnodeBlock);
    void StartEmitCatch(ParseNode *pnodeCatch);
    void EndEmitCatch(ParseNode *pnodeCatch);
    void StartEmitWith(ParseNode *pnodeWith);
    void EndEmitWith(ParseNode *pnodeWith);
    void EnsureFncScopeSlots(ParseNode *pnode, FuncInfo *funcInfo);
    void EnsureLetConstScopeSlots(ParseNode *pnodeBlock, FuncInfo *funcInfo);
    bool EnsureSymbolModuleSlots(Symbol* sym, FuncInfo* funcInfo);
    void EmitAssignmentToDefaultModuleExport(ParseNode* pnode, FuncInfo* funcInfo);
    void EmitModuleExportAccess(Symbol* sym, Js::OpCode opcode, Js::RegSlot location, FuncInfo* funcInfo);

    void PushScope(Scope *innerScope);
    void PopScope();
    void PushBlock(ParseNode *pnode);
    void PopBlock();

    void PushFuncInfo(char16 const * location, FuncInfo* funcInfo);
    void PopFuncInfo(char16 const * location);

    Js::RegSlot PrependLocalScopes(Js::RegSlot evalEnv, Js::RegSlot tempLoc, FuncInfo *funcInfo);
    Symbol *FindSymbol(Symbol **symRef, IdentPtr pid, bool forReference = false);
    Symbol *AddSymbolToScope(Scope *scope, const char16 *key, int keyLength, ParseNode *varDecl, SymbolType symbolType);
    Symbol *AddSymbolToFunctionScope(const char16 *key, int keyLength, ParseNode *varDecl, SymbolType symbolType);
    void FuncEscapes(Scope *scope);
    void EmitTopLevelStatement(ParseNode *stmt, FuncInfo *funcInfo, BOOL fReturnValue);
    void EmitInvertedLoop(ParseNode* outerLoop,ParseNode* invertedLoop,FuncInfo* funcInfo);
    void DefineFunctions(FuncInfo *funcInfoParent);
    Js::RegSlot DefineOneFunction(ParseNode *pnodeFnc, FuncInfo *funcInfoParent, bool generateAssignment=true, Js::RegSlot regEnv = Js::Constants::NoRegister, Js::RegSlot frameDisplayTemp = Js::Constants::NoRegister);
    void DefineCachedFunctions(FuncInfo *funcInfoParent);
    void DefineUncachedFunctions(FuncInfo *funcInfoParent);
    void DefineUserVars(FuncInfo *funcInfo);
    void InitBlockScopedNonTemps(ParseNode *pnode, FuncInfo *funcInfo);
    // temporarily load all constants and special registers in a single block
    void LoadAllConstants(FuncInfo *funcInfo);
    void LoadHeapArguments(FuncInfo *funcInfo);
    void LoadUncachedHeapArguments(FuncInfo *funcInfo);
    void LoadCachedHeapArguments(FuncInfo *funcInfo);
    void LoadThisObject(FuncInfo *funcInfo, bool thisLoadedFromParams = false);
    void EmitThis(FuncInfo *funcInfo, Js::RegSlot fromRegister);
    void LoadNewTargetObject(FuncInfo *funcInfo);
    void GetEnclosingNonLambdaScope(FuncInfo *funcInfo, Scope * &scope, Js::PropertyId &envIndex);
    void EmitInternalScopedSlotLoad(FuncInfo *funcInfo, Js::RegSlot slot, Js::RegSlot symbolRegister, bool chkUndecl = false);
    void EmitInternalScopedSlotLoad(FuncInfo *funcInfo, Scope *scope, Js::PropertyId envIndex, Js::RegSlot slot, Js::RegSlot symbolRegister, bool chkUndecl = false);
    void EmitInternalScopedSlotStore(FuncInfo *funcInfo, Js::RegSlot slot, Js::RegSlot symbolRegister);
    void EmitInternalScopeObjInit(FuncInfo *funcInfo, Scope *scope, Js::RegSlot valueLocation, Js::PropertyId propertyId);
    void EmitSuperCall(FuncInfo* funcInfo, ParseNode* pnode, BOOL fReturnValue);
    void EmitScopeSlotLoadThis(FuncInfo *funcInfo, Js::RegSlot regLoc, bool chkUndecl = true);
    void EmitScopeSlotStoreThis(FuncInfo *funcInfo, Js::RegSlot regLoc, bool chkUndecl = false);
    void EmitClassConstructorEndCode(FuncInfo *funcInfo);
    void EmitBaseClassConstructorThisObject(FuncInfo *funcInfo);

    // TODO: home the 'this' argument
    void EmitLoadFormalIntoRegister(ParseNode *pnodeFormal, Js::RegSlot pos, FuncInfo *funcInfo);
    void HomeArguments(FuncInfo *funcInfo);

    void EnsureNoRedeclarations(ParseNode *pnodeBlock, FuncInfo *funcInfo);

    void DefineLabels(FuncInfo *funcInfo);
    void EmitProgram(ParseNode *pnodeProg);
    void EmitScopeList(ParseNode *pnode, ParseNode *breakOnBodyScopeNode = nullptr);
    void EmitDefaultArgs(FuncInfo *funcInfo, ParseNode *pnode);
    void EmitOneFunction(ParseNode *pnode);
    void EmitGlobalFncDeclInit(Js::RegSlot rhsLocation, Js::PropertyId propertyId, FuncInfo * funcInfo);
    void EmitLocalPropInit(Js::RegSlot rhsLocation, Symbol *sym, FuncInfo *funcInfo);
    void EmitPropStore(Js::RegSlot rhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo, bool isLet = false, bool isConst = false, bool isFncDeclVar = false);
    void EmitPropLoad(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo);
    void EmitPropDelete(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo);
    void EmitPropTypeof(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo);
    void EmitTypeOfFld(FuncInfo * funcInfo, Js::PropertyId propertyId, Js::RegSlot value, Js::RegSlot instance, Js::OpCode op1);

    void EmitLoadInstance(Symbol *sym, IdentPtr pid, Js::RegSlot *pThisLocation, Js::RegSlot *pTargetLocation, FuncInfo *funcInfo);
    void EmitGlobalBody(FuncInfo *funcInfo);
    void EmitFunctionBody(FuncInfo *funcInfo);
    void EmitAsmFunctionBody(FuncInfo *funcInfo);
    void EmitScopeObjectInit(FuncInfo *funcInfo);

    void EmitPatchableRootProperty(Js::OpCode opcode, Js::RegSlot regSlot, Js::PropertyId propertyId, bool isLoadMethod, bool isStore, FuncInfo *funcInfo);

    struct TryScopeRecord;
    JsUtil::DoublyLinkedList<TryScopeRecord> tryScopeRecordsList;
    void EmitLeaveOpCodesBeforeYield();
    void EmitTryBlockHeadersAfterYield();

    void InvalidateCachedOuterScopes(FuncInfo *funcInfo);

    bool InDynamicScope() const {TRACE_IT(40724); return dynamicScopeCount != 0; }

    Scope * FindScopeForSym(Scope *symScope, Scope *scope, Js::PropertyId *envIndex, FuncInfo *funcInfo) const;

    static Js::OpCode GetStFldOpCode(bool isStrictMode, bool isRoot, bool isLetDecl, bool isConstDecl, bool isClassMemberInit)
    {TRACE_IT(40725);
        return isClassMemberInit ? Js::OpCode::InitClassMember :
            isConstDecl ? (isRoot ? Js::OpCode::InitRootConstFld : Js::OpCode::InitConstFld) :
            isLetDecl ? (isRoot ? Js::OpCode::InitRootLetFld : Js::OpCode::InitLetFld) :
            isStrictMode ? (isRoot ? Js::OpCode::StRootFldStrict : Js::OpCode::StFldStrict) :
            isRoot ? Js::OpCode::StRootFld : Js::OpCode::StFld;
    }
    static Js::OpCode GetStFldOpCode(FuncInfo* funcInfo, bool isRoot, bool isLetDecl, bool isConstDecl, bool isClassMemberInit);
    static Js::OpCode GetScopedStFldOpCode(bool isStrictMode)
    {TRACE_IT(40726);
        return isStrictMode ? Js::OpCode::ScopedStFldStrict : Js::OpCode::ScopedStFld;
    }
    static Js::OpCode GetScopedStFldOpCode(FuncInfo* funcInfo, bool isConsoleScopeLetConst = false);
    static Js::OpCode GetStElemIOpCode(bool isStrictMode)
    {TRACE_IT(40727);
        return isStrictMode ? Js::OpCode::StElemI_A_Strict : Js::OpCode::StElemI_A;
    }
    static Js::OpCode GetStElemIOpCode(FuncInfo* funcInfo);

    bool DoJitLoopBodies(FuncInfo *funcInfo) const;

    static void Generate(__in ParseNode *pnode, uint32 grfscr, __in ByteCodeGenerator* byteCodeGenerator, __inout Js::ParseableFunctionInfo ** ppRootFunc, __in uint sourceIndex, __in bool forceNoNative, __in Parser* parser, Js::ScriptFunction ** functionRef);
    void Begin(
        __in ArenaAllocator *alloc,
        __in uint32 grfscr,
        __in Js::ParseableFunctionInfo* pRootFunc);

    void SetCurrentSourceIndex(uint sourceIndex) {TRACE_IT(40728); this->sourceIndex = sourceIndex; }
    uint GetCurrentSourceIndex() {TRACE_IT(40729); return sourceIndex; }

    static bool IsFalse(ParseNode* node);

    void StartStatement(ParseNode* node);
    void EndStatement(ParseNode* node);
    void StartSubexpression(ParseNode* node);
    void EndSubexpression(ParseNode* node);

    bool IsES6DestructuringEnabled() const;
    bool IsES6ForLoopSemanticsEnabled() const;

    // Debugger methods.
    bool IsInDebugMode() const;
    bool IsInNonDebugMode() const;
    bool ShouldTrackDebuggerMetadata() const;
    void TrackRegisterPropertyForDebugger(Js::DebuggerScope *debuggerScope, Symbol *symbol, FuncInfo *funcInfo, Js::DebuggerScopePropertyFlags flags = Js::DebuggerScopePropertyFlags_None, bool isFunctionDeclaration = false);
    void TrackActivationObjectPropertyForDebugger(Js::DebuggerScope *debuggerScope, Symbol *symbol, Js::DebuggerScopePropertyFlags flags = Js::DebuggerScopePropertyFlags_None, bool isFunctionDeclaration = false);
    void TrackSlotArrayPropertyForDebugger(Js::DebuggerScope *debuggerScope, Symbol* symbol, Js::PropertyId propertyId, Js::DebuggerScopePropertyFlags flags = Js::DebuggerScopePropertyFlags_None, bool isFunctionDeclaration = false);
    void TrackFunctionDeclarationPropertyForDebugger(Symbol *functionDeclarationSymbol, FuncInfo *funcInfoParent);
    void UpdateDebuggerPropertyInitializationOffset(Js::RegSlot location, Js::PropertyId propertyId, bool shouldConsumeRegister = true);

    void PopulateFormalsScope(uint beginOffset, FuncInfo *funcInfo, ParseNode *pnode);
    void InsertPropertyToDebuggerScope(FuncInfo* funcInfo, Js::DebuggerScope* debuggerScope, Symbol* sym);
    FuncInfo *FindEnclosingNonLambda();

    bool CanStackNestedFunc(FuncInfo * funcInfo, bool trace = false);
    void CheckDeferParseHasMaybeEscapedNestedFunc();
    bool NeedObjectAsFunctionScope(FuncInfo * funcInfo, ParseNode * pnodeFnc) const;
    bool HasInterleavingDynamicScope(Symbol * sym) const;

    void MarkThisUsedInLambda();

    void EmitInitCapturedThis(FuncInfo* funcInfo, Scope* scope);
    void EmitInitCapturedNewTarget(FuncInfo* funcInfo, Scope* scope);

    Js::FunctionBody *EnsureFakeGlobalFuncForUndefer(ParseNode *pnode);
    Js::FunctionBody *MakeGlobalFunctionBody(ParseNode *pnode);

    static bool NeedScopeObjectForArguments(FuncInfo *funcInfo, ParseNode *pnodeFnc);

    Js::OpCode GetStSlotOp(Scope *scope, int envIndex, Js::RegSlot scopeLocation, bool chkBlockVar, FuncInfo *funcInfo);
    Js::OpCode GetLdSlotOp(Scope *scope, int envIndex, Js::RegSlot scopeLocation, FuncInfo *funcInfo);
    Js::OpCode GetInitFldOp(Scope *scope, Js::RegSlot scopeLocation, FuncInfo *funcInfo, bool letDecl = false);

private:
    bool NeedCheckBlockVar(Symbol* sym, Scope* scope, FuncInfo* funcInfo) const;

    Js::OpCode ToChkUndeclOp(Js::OpCode op) const;
};

template<class Fn> void ByteCodeGenerator::IterateBlockScopedVariables(ParseNode *pnodeBlock, Fn fn)
{TRACE_IT(40730);
    Assert(pnodeBlock->nop == knopBlock);
    for (auto lexvar = pnodeBlock->sxBlock.pnodeLexVars; lexvar; lexvar = lexvar->sxVar.pnodeNext)
    {TRACE_IT(40731);
        fn(lexvar);
    }
}

struct ApplyCheck {
    bool matches;
    bool insideApplyCall;
    bool sawApply;
};
