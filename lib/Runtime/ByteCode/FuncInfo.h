//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
struct InlineCacheUnit
{
    InlineCacheUnit() : loadCacheId((uint)-1), loadMethodCacheId((uint)-1), storeCacheId((uint)-1) {TRACE_IT(41572);}

    union {
        struct {
            uint loadCacheId;
            uint loadMethodCacheId;
            uint storeCacheId;
        };
        struct {
            uint cacheId;
        };
    };
};

typedef JsUtil::BaseDictionary<ParseNode*, SList<Symbol*>*, ArenaAllocator, PowerOf2SizePolicy> CapturedSymMap;

class FuncInfo
{
private:
    struct SlotKey
    {
        Scope* scope;
        Js::PropertyId slot;
    };

    template<class TSlotKey>
    class SlotKeyComparer
    {
    public:
        static bool Equals(TSlotKey key1, TSlotKey key2)
        {TRACE_IT(41573);
            return (key1.scope == key2.scope && key1.slot == key2.slot);
        }

        static int GetHashCode(TSlotKey key)
        {TRACE_IT(41574);
            return ::Math::PointerCastToIntegralTruncate<int>(key.scope) | key.slot & ArenaAllocator::ObjectAlignmentMask;
        }
    };

    uint inlineCacheCount;
    uint rootObjectLoadInlineCacheCount;
    uint rootObjectLoadMethodInlineCacheCount;
    uint rootObjectStoreInlineCacheCount;
    uint isInstInlineCacheCount;
    uint referencedPropertyIdCount;
    uint NewInlineCache()
    {TRACE_IT(41575);
        AssertMsg(this->inlineCacheCount < (uint)-2, "Inline cache index wrapped around?");
        return inlineCacheCount++;
    }
    uint NewRootObjectLoadInlineCache()
    {TRACE_IT(41576);
        AssertMsg(this->rootObjectLoadInlineCacheCount < (uint)-2, "Inline cache index wrapped around?");
        return rootObjectLoadInlineCacheCount++;
    }
    uint NewRootObjectLoadMethodInlineCache()
    {TRACE_IT(41577);
        AssertMsg(this->rootObjectLoadMethodInlineCacheCount < (uint)-2, "Inline cache index wrapped around?");
        return rootObjectLoadMethodInlineCacheCount++;
    }
    uint NewRootObjectStoreInlineCache()
    {TRACE_IT(41578);
        AssertMsg(this->rootObjectStoreInlineCacheCount < (uint)-2, "Inline cache index wrapped around?");
        return rootObjectStoreInlineCacheCount++;
    }
    uint NewReferencedPropertyId()
    {TRACE_IT(41579);
        AssertMsg(this->referencedPropertyIdCount < (uint)-2, "Referenced Property Id index wrapped around?");
        return referencedPropertyIdCount++;
    }

    FuncInfo *currentChildFunction;
    Scope *currentChildScope;
    SymbolTable *capturedSyms;
    CapturedSymMap *capturedSymMap;
    uint        nextForInLoopLevel;
    uint        maxForInLoopLevel;
public:
    static const Js::RegSlot InitialConstRegsCount = 2; // constRegsCount is set to 2 because R0 is the return register, and R1 is the root object

    ArenaAllocator *alloc;
    // set in Bind/Assign pass
    Js::RegSlot varRegsCount; // number of registers used for non-constants
    Js::RegSlot constRegsCount; // number of registers used for constants
    Js::ArgSlot inArgsCount; // number of in args (including 'this')
    Js::RegSlot outArgsMaxDepth; // max depth of out args stack
    Js::RegSlot outArgsCurrentExpr; // max number of out args accumulated in the current nested expression
    uint        innerScopeCount;
    uint        currentInnerScopeIndex;
#if DBG
    uint32 outArgsDepth; // number of calls nested in an expression
#endif
    const char16 *name; // name of the function
    Js::RegSlot nullConstantRegister; // location, if any, of enregistered null constant
    Js::RegSlot undefinedConstantRegister; // location, if any, of enregistered undefined constant
    Js::RegSlot trueConstantRegister; // location, if any, of enregistered true constant
    Js::RegSlot falseConstantRegister; // location, if any, of enregistered false constant
    Js::RegSlot thisPointerRegister; // location, if any, of this pointer
    Js::RegSlot superRegister; // location, if any, of the super reference
    Js::RegSlot superCtorRegister; // location, if any, of the superCtor reference
    Js::RegSlot newTargetRegister; // location, if any, of the new.target reference
private:
    Js::RegSlot envRegister; // location, if any, of the closure environment
public:
    Js::RegSlot frameObjRegister; // location, if any, of the heap-allocated local frame
    Js::RegSlot frameSlotsRegister; // location, if any, of the heap-allocated local frame
    Js::RegSlot paramSlotsRegister; // location, if any, of the heap allocated local frame for param scope
    Js::RegSlot frameDisplayRegister; // location, if any, of the display of nested frames
    Js::RegSlot funcObjRegister;
    Js::RegSlot localClosureReg;
    Js::RegSlot yieldRegister;
    Js::RegSlot firstTmpReg;
    Js::RegSlot curTmpReg;
    int argsPlaceHolderSlotCount;   // count of place holder slots for same name args and destructuring patterns
    Js::RegSlot firstThunkArgReg;
    short thunkArgCount;
    short staticFuncId;

    uint callsEval : 1;
    uint childCallsEval : 1;
    uint hasArguments : 1;
    uint hasHeapArguments : 1;
    uint isTopLevelEventHandler : 1;
    uint hasLocalInClosure : 1;
    uint hasClosureReference : 1;
    uint hasGlobalReference : 1;
    uint hasCachedScope : 1;
    uint funcExprNameReference : 1;
    uint applyEnclosesArgs : 1;
    uint escapes : 1;
    uint hasDeferredChild : 1; // switch for DeferNested to persist outer scopes
    uint hasRedeferrableChild : 1;
    uint childHasWith : 1; // deferNested needs to know if child has with
    uint hasLoop : 1;
    uint hasEscapedUseNestedFunc : 1;
    uint needEnvRegister : 1;
    uint hasCapturedThis : 1;
    uint isBodyAndParamScopeMerged : 1;
#if DBG
    // FunctionBody was reused on recompile of a redeferred enclosing function.
    uint isReused:1;
#endif

    typedef JsUtil::BaseDictionary<uint, Js::RegSlot, ArenaAllocator, PrimeSizePolicy> ConstantRegisterMap;
    ConstantRegisterMap constantToRegister; // maps uint constant to register
    typedef JsUtil::BaseDictionary<IdentPtr, Js::RegSlot, ArenaAllocator> PidRegisterMap;
    PidRegisterMap stringToRegister; // maps string constant to register
    typedef JsUtil::BaseDictionary<double,Js::RegSlot, ArenaAllocator, PrimeSizePolicy> DoubleRegisterMap;
    DoubleRegisterMap doubleConstantToRegister; // maps double constant to register

    typedef JsUtil::BaseDictionary<ParseNodePtr, Js::RegSlot, ArenaAllocator, PowerOf2SizePolicy, Js::StringTemplateCallsiteObjectComparer> StringTemplateCallsiteRegisterMap;
    StringTemplateCallsiteRegisterMap stringTemplateCallsiteRegisterMap; // maps string template callsite constant to register

    Scope *paramScope; // top level scope for parameter default values
    Scope *bodyScope; // top level scope of the function body
    Scope *funcExprScope;
    ParseNode *root; // top-level AST for function
    Js::ParseableFunctionInfo* byteCodeFunction; // reference to generated bytecode function (could be defer parsed or actually parsed)
    SList<ParseNode*> targetStatements; // statements that are targets of jumps (break or continue)
    Js::ByteCodeLabel singleExit;
    typedef SList<InlineCacheUnit> InlineCacheList;
    typedef JsUtil::BaseDictionary<Js::PropertyId, InlineCacheList*, ArenaAllocator, PowerOf2SizePolicy> InlineCacheIdMap;
    typedef JsUtil::BaseDictionary<Js::RegSlot, InlineCacheIdMap*, ArenaAllocator, PowerOf2SizePolicy> InlineCacheMap;
    typedef JsUtil::BaseDictionary<Js::PropertyId, uint, ArenaAllocator, PowerOf2SizePolicy> RootObjectInlineCacheIdMap;
    typedef JsUtil::BaseDictionary<Js::PropertyId, uint, ArenaAllocator, PowerOf2SizePolicy> ReferencedPropertyIdMap;
    RootObjectInlineCacheIdMap * rootObjectLoadInlineCacheMap;
    RootObjectInlineCacheIdMap * rootObjectLoadMethodInlineCacheMap;
    RootObjectInlineCacheIdMap * rootObjectStoreInlineCacheMap;
    InlineCacheMap * inlineCacheMap;
    ReferencedPropertyIdMap * referencedPropertyIdToMapIndex;
    SListBase<uint> valueOfStoreCacheIds;
    SListBase<uint> toStringStoreCacheIds;
    typedef JsUtil::BaseDictionary<SlotKey, Js::ProfileId, ArenaAllocator, PowerOf2SizePolicy, SlotKeyComparer> SlotProfileIdMap;
    SlotProfileIdMap slotProfileIdMap;
    Js::PropertyId thisScopeSlot;
    Js::PropertyId superScopeSlot;
    Js::PropertyId superCtorScopeSlot;
    Js::PropertyId newTargetScopeSlot;
    bool isThisLexicallyCaptured;
    bool isSuperLexicallyCaptured;
    bool isSuperCtorLexicallyCaptured;
    bool isNewTargetLexicallyCaptured;
    Symbol *argumentsSymbol;
    JsUtil::List<Js::RegSlot, ArenaAllocator> nonUserNonTempRegistersToInitialize;

    FuncInfo(
        const char16 *name,
        ArenaAllocator *alloc,
        Scope *paramScope,
        Scope *bodyScope,
        ParseNode *pnode,
        Js::ParseableFunctionInfo* byteCodeFunction);
    uint NewIsInstInlineCache() {TRACE_IT(41580); return isInstInlineCacheCount++; }
    uint GetInlineCacheCount() const {TRACE_IT(41581); return this->inlineCacheCount; }
    uint GetRootObjectLoadInlineCacheCount() const {TRACE_IT(41582); return this->rootObjectLoadInlineCacheCount; }
    uint GetRootObjectLoadMethodInlineCacheCount() const {TRACE_IT(41583); return this->rootObjectLoadMethodInlineCacheCount; }
    uint GetRootObjectStoreInlineCacheCount() const {TRACE_IT(41584); return this->rootObjectStoreInlineCacheCount; }
    uint GetIsInstInlineCacheCount() const {TRACE_IT(41585); return this->isInstInlineCacheCount; }
    uint GetReferencedPropertyIdCount() const {TRACE_IT(41586); return this->referencedPropertyIdCount; }
    void SetFirstTmpReg(Js::RegSlot tmpReg)
    {TRACE_IT(41587);
        Assert(this->firstTmpReg == Js::Constants::NoRegister);
        Assert(this->curTmpReg == Js::Constants::NoRegister);
        this->firstTmpReg = tmpReg;
        this->curTmpReg = tmpReg;
    }

    bool IsTmpReg(Js::RegSlot tmpReg)
    {TRACE_IT(41588);
        Assert(this->firstTmpReg != Js::Constants::NoRegister);
        return !RegIsConst(tmpReg) && tmpReg >= firstTmpReg;
    }

    bool RegIsConst(Js::RegSlot reg)
    {TRACE_IT(41589);
        // varRegsCount includes the tmp regs, so if reg number is larger than that,
        // then it must be in the negative range for const.
        return reg >= varRegsCount;
    }

    Js::RegSlot NextVarRegister()
    {TRACE_IT(41590);
        AssertMsg(this->firstTmpReg == Js::Constants::NoRegister, "Shouldn't assign var register after we start allocating temp reg");
        Js::RegSlot reg = varRegsCount;
        UInt32Math::Inc(varRegsCount);
        return REGSLOT_TO_VARREG(reg);
    }

    Js::RegSlot NextConstRegister()
    {TRACE_IT(41591);
        AssertMsg(this->firstTmpReg == Js::Constants::NoRegister, "Shouldn't assign var register after we start allocating temp reg");
        Js::RegSlot reg = constRegsCount;
        UInt32Math::Inc(constRegsCount);
        return REGSLOT_TO_CONSTREG(reg);
    }

    Js::RegSlot RegCount() const
    {TRACE_IT(41592);
        return constRegsCount + varRegsCount;
    }

    uint InnerScopeCount() const {TRACE_IT(41593); return innerScopeCount; }
    uint CurrentInnerScopeIndex() const {TRACE_IT(41594); return currentInnerScopeIndex; }
    uint AcquireInnerScopeIndex();
    void ReleaseInnerScopeIndex();

    bool GetApplyEnclosesArgs() const {TRACE_IT(41595); return applyEnclosesArgs; }
    void SetApplyEnclosesArgs(bool b) {TRACE_IT(41596); applyEnclosesArgs=b; }

    bool IsGlobalFunction() const;

    // Fake global ->
    //    1) new Function code's global code
    //    2) global code generated from the reparsing deferred parse function

    bool IsFakeGlobalFunction(uint32 flags) const {TRACE_IT(41597);
        return IsGlobalFunction() && !(flags & fscrGlobalCode);
    }

    Scope *GetBodyScope() const {TRACE_IT(41598);
        return bodyScope;
    }

    Scope *GetParamScope() const {TRACE_IT(41599);
        return paramScope;
    }

    Scope *GetTopLevelScope() const {TRACE_IT(41600);
        // Top level scope will be the same for knopProg and knopFncDecl.
        return paramScope;
    }

    Scope* GetFuncExprScope() const {TRACE_IT(41601);
        return funcExprScope;
    }

    void SetFuncExprScope(Scope* funcExprScope) {TRACE_IT(41602);
        this->funcExprScope = funcExprScope;
    }

    Symbol *GetArgumentsSymbol() const
    {TRACE_IT(41603);
        return argumentsSymbol;
    }

    void SetArgumentsSymbol(Symbol *sym)
    {TRACE_IT(41604);
        Assert(argumentsSymbol == nullptr || argumentsSymbol == sym);
        argumentsSymbol = sym;
    }

    bool GetCallsEval() const {TRACE_IT(41605);
        return callsEval;
    }

    void SetCallsEval(bool does) {TRACE_IT(41606);
        callsEval = does;
    }

    bool GetHasArguments() const {TRACE_IT(41607);
        return hasArguments;
    }

    void SetHasArguments(bool has) {TRACE_IT(41608);
        hasArguments = has;
    }

    bool GetHasHeapArguments() const
    {TRACE_IT(41609);
        return hasHeapArguments;
    }

    void SetHasHeapArguments(bool has, bool optArgInBackend = false)
    {TRACE_IT(41610);
        hasHeapArguments = has;
        byteCodeFunction->SetDoBackendArgumentsOptimization(optArgInBackend);
    }

    bool GetIsTopLevelEventHandler() const {TRACE_IT(41611);
        return isTopLevelEventHandler;
    }

    void SetIsTopLevelEventHandler(bool is) {TRACE_IT(41612);
        isTopLevelEventHandler = is;
    }

    bool GetChildCallsEval() const {TRACE_IT(41613);
        return childCallsEval;
    }

    void SetChildCallsEval(bool does) {TRACE_IT(41614);
        childCallsEval = does;
    }

    bool GetHasLocalInClosure() const {TRACE_IT(41615);
        return hasLocalInClosure;
    }

    void SetHasLocalInClosure(bool has) {TRACE_IT(41616);
        hasLocalInClosure = has;
    }

    bool GetHasClosureReference() const {TRACE_IT(41617);
        return hasClosureReference;
    }

    void SetHasCachedScope(bool has) {TRACE_IT(41618);
        hasCachedScope = has;
    }

    bool GetHasCachedScope() const {TRACE_IT(41619);
        return hasCachedScope;
    }

    void SetFuncExprNameReference(bool has) {TRACE_IT(41620);
        funcExprNameReference = has;
    }

    bool GetFuncExprNameReference() const {TRACE_IT(41621);
        return funcExprNameReference;
    }

    void SetHasClosureReference(bool has) {TRACE_IT(41622);
        hasClosureReference = has;
    }

    bool GetHasGlobalRef() const {TRACE_IT(41623);
        return hasGlobalReference;
    }

    void SetHasGlobalRef(bool has) {TRACE_IT(41624);
        hasGlobalReference = has;
    }

    bool GetIsStrictMode() const {TRACE_IT(41625);
        return this->byteCodeFunction->GetIsStrictMode();
    }

    bool Escapes() const {TRACE_IT(41626);
        return escapes;
    }

    void SetEscapes(bool does) {TRACE_IT(41627);
        escapes = does;
    }

    bool HasMaybeEscapedNestedFunc() const {TRACE_IT(41628);
        return hasEscapedUseNestedFunc;
    }

    void SetHasMaybeEscapedNestedFunc(DebugOnly(char16 const * reason));

    bool IsDeferred() const;

    bool IsRestored()
    {TRACE_IT(41629);
        // FuncInfo are from RestoredScopeInfo
        return root == nullptr;
    }

    bool HasDeferredChild() const {TRACE_IT(41630);
        return hasDeferredChild;
    }

    void SetHasDeferredChild() {TRACE_IT(41631);
        hasDeferredChild = true;
    }

    bool HasRedeferrableChild() const {TRACE_IT(41632);
        return hasRedeferrableChild;
    }

    void SetHasRedeferrableChild() {TRACE_IT(41633);
        hasRedeferrableChild = true;
    }

    bool IsRedeferrable() const;

    Js::FunctionBody* GetParsedFunctionBody() const
    {TRACE_IT(41634);
        AssertMsg(this->byteCodeFunction->IsFunctionParsed(), "Function must be parsed in order to call this method");
        Assert(!IsDeferred() || this->byteCodeFunction->GetFunctionBody()->GetByteCode() != nullptr);

        return this->byteCodeFunction->GetFunctionBody();
    }

    bool ChildHasWith() const {TRACE_IT(41635);
        return childHasWith;
    }

    void SetChildHasWith() {TRACE_IT(41636);
        childHasWith = true;
    }

    bool HasCapturedThis() const {TRACE_IT(41637);
        return hasCapturedThis;
    }

    void SetHasCapturedThis() {TRACE_IT(41638);
        hasCapturedThis = true;
    }

    bool IsBodyAndParamScopeMerged() const {TRACE_IT(41639);
        return isBodyAndParamScopeMerged;
    }

    void ResetBodyAndParamScopeMerged() {TRACE_IT(41640);
        isBodyAndParamScopeMerged = false;
    }

    BOOL HasSuperReference() const;
    BOOL HasDirectSuper() const;
    BOOL IsClassMember() const;
    BOOL IsLambda() const;
    BOOL IsClassConstructor() const;
    BOOL IsBaseClassConstructor() const;

    void RemoveTargetStmt(ParseNode* pnodeStmt) {TRACE_IT(41641);
        targetStatements.Remove(pnodeStmt);
    }

    void AddTargetStmt(ParseNode *pnodeStmt) {TRACE_IT(41642);
        targetStatements.Prepend(pnodeStmt);
    }

    Js::RegSlot LookupDouble(double d) {TRACE_IT(41643);
        return doubleConstantToRegister.Lookup(d,Js::Constants::NoRegister);
    }

    bool TryGetDoubleLoc(double d, Js::RegSlot *loc) {TRACE_IT(41644);
        Js::RegSlot ret=LookupDouble(d);
        *loc=ret;
        return(ret!=Js::Constants::NoRegister);
    }

    void AddDoubleConstant(double d, Js::RegSlot location) {TRACE_IT(41645);
        doubleConstantToRegister.Item(d,location);
    }

    bool NeedEnvRegister() const {TRACE_IT(41646); return this->needEnvRegister; }
    void SetNeedEnvRegister() {TRACE_IT(41647); this->needEnvRegister = true; };
    Js::RegSlot GetEnvRegister() const
    {TRACE_IT(41648);
        Assert(this->envRegister != Js::Constants::NoRegister);
        return this->envRegister;
    }
    Js::RegSlot AssignEnvRegister(bool constReg)
    {TRACE_IT(41649);
        Assert(needEnvRegister);
        Assert(this->envRegister == Js::Constants::NoRegister);
        Js::RegSlot reg = constReg? NextConstRegister() : NextVarRegister();
        this->envRegister = reg;
        return reg;
    }

    Js::RegSlot AssignThisRegister()
    {TRACE_IT(41650);
        if (this->thisPointerRegister == Js::Constants::NoRegister)
        {TRACE_IT(41651);
            this->thisPointerRegister = NextVarRegister();
        }
        return this->thisPointerRegister;
    }

    Js::RegSlot AssignSuperRegister()
    {TRACE_IT(41652);
        if (this->superRegister == Js::Constants::NoRegister)
        {TRACE_IT(41653);
            this->superRegister = NextVarRegister();
        }
        return this->superRegister;
    }

    Js::RegSlot AssignSuperCtorRegister()
    {TRACE_IT(41654);
        if (this->superCtorRegister == Js::Constants::NoRegister)
        {TRACE_IT(41655);
            this->superCtorRegister = NextVarRegister();
        }
        return this->superCtorRegister;
    }

    Js::RegSlot AssignNewTargetRegister()
    {TRACE_IT(41656);
        if (this->newTargetRegister == Js::Constants::NoRegister)
        {TRACE_IT(41657);
            this->newTargetRegister = NextVarRegister();
        }
        return this->newTargetRegister;
    }

    Js::RegSlot AssignNullConstRegister()
    {TRACE_IT(41658);
        if (this->nullConstantRegister == Js::Constants::NoRegister)
        {TRACE_IT(41659);
            this->nullConstantRegister = NextConstRegister();
        }
        return this->nullConstantRegister;
    }

    Js::RegSlot AssignUndefinedConstRegister()
    {TRACE_IT(41660);
        if (this->undefinedConstantRegister == Js::Constants::NoRegister)
        {TRACE_IT(41661);
            this->undefinedConstantRegister = NextConstRegister();
        }
        return this->undefinedConstantRegister;
    }

    Js::RegSlot AssignTrueConstRegister()
    {TRACE_IT(41662);
        if (this->trueConstantRegister == Js::Constants::NoRegister)
        {TRACE_IT(41663);
            this->trueConstantRegister = NextConstRegister();
        }
        return this->trueConstantRegister;
    }

    Js::RegSlot AssignFalseConstRegister()
    {TRACE_IT(41664);
        if (this->falseConstantRegister == Js::Constants::NoRegister)
        {TRACE_IT(41665);
            this->falseConstantRegister = NextConstRegister();
        }
        return this->falseConstantRegister;
    }

    Js::RegSlot AssignYieldRegister()
    {TRACE_IT(41666);
        AssertMsg(this->yieldRegister == Js::Constants::NoRegister, "yield register should only be assigned once by FinalizeRegisters()");
        this->yieldRegister = NextVarRegister();
        return this->yieldRegister;
    }

    Js::RegSlot GetLocalScopeSlotsReg()
    {TRACE_IT(41667);
        return this->localClosureReg;
    }

    Js::RegSlot GetLocalFrameDisplayReg()
    {TRACE_IT(41668);
        return this->localClosureReg + 1;
    }

    Js::RegSlot InnerScopeToRegSlot(Scope *scope) const;
    Js::RegSlot FirstInnerScopeReg() const;
    void SetFirstInnerScopeReg(Js::RegSlot reg);

    void StartRecordingOutArgs(unsigned int argCount)
    {TRACE_IT(41669);
#if DBG
        outArgsDepth++;
#endif
        // We should have checked for argCount overflow already
        Assert(argCount == (Js::ArgSlot)argCount);

        // Add one for the space to save the m_outParams pointer in InterpreterStackFrame::PushOut
        unsigned int outArgsCount = argCount + 1;
        outArgsCurrentExpr += (Js::ArgSlot)outArgsCount;

        // Check for overflow
        if ((Js::ArgSlot)outArgsCount != outArgsCount || outArgsCurrentExpr < outArgsCount)
        {TRACE_IT(41670);
            Js::Throw::OutOfMemory();
        }
        outArgsMaxDepth = max(outArgsMaxDepth, outArgsCurrentExpr);
    }

    void EndRecordingOutArgs(Js::ArgSlot argCount)
    {TRACE_IT(41671);
        AssertMsg(outArgsDepth > 0, "mismatched Start and End");
        Assert(outArgsCurrentExpr >= argCount);
#if DBG
        outArgsDepth--;
#endif
        // Add one to pop the space to save the m_outParams pointer
        outArgsCurrentExpr -= (argCount + 1);

        Assert(outArgsDepth != 0 || outArgsCurrentExpr == 0);
    }

    uint GetMaxForInLoopLevel() const {TRACE_IT(41672); return this->maxForInLoopLevel;  }
    uint AcquireForInLoopLevel()
    {TRACE_IT(41673);
        uint forInLoopLevel = this->nextForInLoopLevel++;
        this->maxForInLoopLevel = max(this->maxForInLoopLevel, this->nextForInLoopLevel);
        return forInLoopLevel;
    }

    void ReleaseForInLoopLevel(uint forInLoopLevel)
    {TRACE_IT(41674);
        Assert(this->nextForInLoopLevel == forInLoopLevel + 1);
        this->nextForInLoopLevel = forInLoopLevel;
    }

    Js::RegSlot AcquireLoc(ParseNode *pnode);
    Js::RegSlot AcquireTmpRegister();
    void ReleaseLoc(ParseNode *pnode);
    void ReleaseReference(ParseNode *pnode);
    void ReleaseLoad(ParseNode *pnode);
    void ReleaseTmpRegister(Js::RegSlot tmpReg);

    uint FindOrAddReferencedPropertyId(Js::PropertyId propertyId);

    uint FindOrAddRootObjectInlineCacheId(Js::PropertyId propertyId, bool isLoadMethod, bool isStore);

    uint FindOrAddInlineCacheId(Js::RegSlot instanceSlot, Js::PropertyId propertySlot, bool isLoadMethod, bool isStore)
    {TRACE_IT(41675);
        Assert(instanceSlot != Js::Constants::NoRegister);
        Assert(propertySlot != Js::Constants::NoProperty);
        Assert(!isLoadMethod || !isStore);

        InlineCacheIdMap *properties;
        uint cacheId;

        if (isStore)
        {TRACE_IT(41676);
            // ... = foo.toString;
            // foo.toString = ...;

            // We need a new cache here to ensure SetProperty() is called, which will set the side-effect bit
            // on the scriptContext.
            switch (propertySlot)
            {
            case Js::PropertyIds::valueOf:
                cacheId = this->NewInlineCache();
                valueOfStoreCacheIds.Prepend(alloc, cacheId);
                return cacheId;

            case Js::PropertyIds::toString:
                cacheId = this->NewInlineCache();
                toStringStoreCacheIds.Prepend(alloc, cacheId);
                return cacheId;
            };
        }

        if (!inlineCacheMap->TryGetValue(instanceSlot, &properties))
        {TRACE_IT(41677);
            properties = Anew(alloc, InlineCacheIdMap, alloc, 17);
            inlineCacheMap->Add(instanceSlot, properties);
        }

        InlineCacheList* cacheList;
        if (!properties->TryGetValue(propertySlot, &cacheList))
        {TRACE_IT(41678);
            cacheList = Anew(alloc, InlineCacheList, alloc);
            properties->Add(propertySlot, cacheList);
        }

        // If we share inline caches we should never have more than one entry in the list.
        Assert(Js::FunctionBody::ShouldShareInlineCaches() || cacheList->Count() <= 1);

        InlineCacheUnit cacheIdUnit;

        if (Js::FunctionBody::ShouldShareInlineCaches() && !cacheList->Empty())
        {TRACE_IT(41679);
            cacheIdUnit = cacheList->Head();
            if (isLoadMethod)
            {TRACE_IT(41680);
                if (cacheIdUnit.loadMethodCacheId == (uint)-1)
                {TRACE_IT(41681);
                    cacheIdUnit.loadMethodCacheId = this->NewInlineCache();
                }
                cacheId = cacheIdUnit.loadMethodCacheId;
            }
            else if (isStore)
            {TRACE_IT(41682);
                if (cacheIdUnit.storeCacheId == (uint)-1)
                {TRACE_IT(41683);
                    cacheIdUnit.storeCacheId = this->NewInlineCache();
                }
                cacheId = cacheIdUnit.storeCacheId;
            }
            else
            {TRACE_IT(41684);
                if (cacheIdUnit.loadCacheId == (uint)-1)
                {TRACE_IT(41685);
                    cacheIdUnit.loadCacheId = this->NewInlineCache();
                }
                cacheId = cacheIdUnit.loadCacheId;
            }
            cacheList->Head() = cacheIdUnit;
        }
        else
        {TRACE_IT(41686);
            cacheId = this->NewInlineCache();
            if (Js::FunctionBody::ShouldShareInlineCaches())
            {TRACE_IT(41687);
                if (isLoadMethod)
                {TRACE_IT(41688);
                    cacheIdUnit.loadCacheId = (uint)-1;
                    cacheIdUnit.loadMethodCacheId = cacheId;
                    cacheIdUnit.storeCacheId = (uint)-1;
                }
                else if (isStore)
                {TRACE_IT(41689);
                    cacheIdUnit.loadCacheId = (uint)-1;
                    cacheIdUnit.loadMethodCacheId = (uint)-1;
                    cacheIdUnit.storeCacheId = cacheId;
                }
                else
                {TRACE_IT(41690);
                    cacheIdUnit.loadCacheId = cacheId;
                    cacheIdUnit.loadMethodCacheId = (uint)-1;
                    cacheIdUnit.storeCacheId = (uint)-1;
                }
            }
            else
            {TRACE_IT(41691);
                cacheIdUnit.cacheId = cacheId;
            }
            cacheList->Prepend(cacheIdUnit);
        }

        return cacheId;
    }

    Js::ProfileId FindOrAddSlotProfileId(Scope* scope, Js::PropertyId propertyId)
    {TRACE_IT(41692);
        SlotKey key;

        key.scope = scope;
        key.slot = propertyId;
        Js::ProfileId profileId = Js::Constants::NoProfileId;

        if (!this->slotProfileIdMap.TryGetValue(key, &profileId))
        {TRACE_IT(41693);
            Assert(this->byteCodeFunction->IsFunctionParsed());
            if (this->byteCodeFunction->GetFunctionBody()->AllocProfiledSlotId(&profileId))
            {TRACE_IT(41694);
                this->slotProfileIdMap.Add(key, profileId);
            }
        }

        return profileId;
    }

    void EnsureThisScopeSlot();
    void EnsureSuperScopeSlot();
    void EnsureSuperCtorScopeSlot();
    void EnsureNewTargetScopeSlot();

    void SetIsThisLexicallyCaptured()
    {TRACE_IT(41695);
        this->isThisLexicallyCaptured = true;
    }

    void SetIsSuperLexicallyCaptured()
    {TRACE_IT(41696);
        this->isSuperLexicallyCaptured = true;
    }

    void SetIsSuperCtorLexicallyCaptured()
    {TRACE_IT(41697);
        this->isSuperCtorLexicallyCaptured = true;
    }

    void SetIsNewTargetLexicallyCaptured()
    {TRACE_IT(41698);
        this->isNewTargetLexicallyCaptured = true;
    }

    Scope * GetGlobalBlockScope() const;
    Scope * GetGlobalEvalBlockScope() const;

    FuncInfo *GetCurrentChildFunction() const
    {TRACE_IT(41699);
        return this->currentChildFunction;
    }

    void SetCurrentChildFunction(FuncInfo *funcInfo)
    {TRACE_IT(41700);
        this->currentChildFunction = funcInfo;
    }

    Scope *GetCurrentChildScope() const
    {TRACE_IT(41701);
        return this->currentChildScope;
    }

    void SetCurrentChildScope(Scope *scope)
    {TRACE_IT(41702);
        this->currentChildScope = scope;
    }

    SymbolTable *GetCapturedSyms() const {TRACE_IT(41703); return capturedSyms; }

    void OnStartVisitFunction(ParseNode *pnodeFnc);
    void OnEndVisitFunction(ParseNode *pnodeFnc);
    void OnStartVisitScope(Scope *scope, bool *pisMergedScope);
    void OnEndVisitScope(Scope *scope, bool isMergedScope = false);
    void AddCapturedSym(Symbol *sym);
    CapturedSymMap *EnsureCapturedSymMap();

#if DBG_DUMP
    void Dump();
#endif
};
