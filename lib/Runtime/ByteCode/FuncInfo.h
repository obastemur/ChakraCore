//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
struct InlineCacheUnit
{
    InlineCacheUnit() : loadCacheId((uint)-1), loadMethodCacheId((uint)-1), storeCacheId((uint)-1) {LOGMEIN("FuncInfo.h] 6\n");}

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
        {LOGMEIN("FuncInfo.h] 36\n");
            return (key1.scope == key2.scope && key1.slot == key2.slot);
        }

        static int GetHashCode(TSlotKey key)
        {LOGMEIN("FuncInfo.h] 41\n");
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
    {LOGMEIN("FuncInfo.h] 53\n");
        AssertMsg(this->inlineCacheCount < (uint)-2, "Inline cache index wrapped around?");
        return inlineCacheCount++;
    }
    uint NewRootObjectLoadInlineCache()
    {LOGMEIN("FuncInfo.h] 58\n");
        AssertMsg(this->rootObjectLoadInlineCacheCount < (uint)-2, "Inline cache index wrapped around?");
        return rootObjectLoadInlineCacheCount++;
    }
    uint NewRootObjectLoadMethodInlineCache()
    {LOGMEIN("FuncInfo.h] 63\n");
        AssertMsg(this->rootObjectLoadMethodInlineCacheCount < (uint)-2, "Inline cache index wrapped around?");
        return rootObjectLoadMethodInlineCacheCount++;
    }
    uint NewRootObjectStoreInlineCache()
    {LOGMEIN("FuncInfo.h] 68\n");
        AssertMsg(this->rootObjectStoreInlineCacheCount < (uint)-2, "Inline cache index wrapped around?");
        return rootObjectStoreInlineCacheCount++;
    }
    uint NewReferencedPropertyId()
    {LOGMEIN("FuncInfo.h] 73\n");
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
    Js::PropertyId innerThisScopeSlot; // Used in case of split scope
    Js::PropertyId superScopeSlot;
    Js::PropertyId innerSuperScopeSlot; // Used in case of split scope
    Js::PropertyId superCtorScopeSlot;
    Js::PropertyId innerSuperCtorScopeSlot; // Used in case of split scope
    Js::PropertyId newTargetScopeSlot;
    Js::PropertyId innerNewTargetScopeSlot; // Used in case of split scope
    bool isThisLexicallyCaptured;
    bool isSuperLexicallyCaptured;
    bool isSuperCtorLexicallyCaptured;
    bool isNewTargetLexicallyCaptured;
    Symbol *argumentsSymbol;
    Symbol *innerArgumentsSymbol;
    JsUtil::List<Js::RegSlot, ArenaAllocator> nonUserNonTempRegistersToInitialize;

    // constRegsCount is set to 2 because R0 is the return register, and R1 is the root object.
    FuncInfo(
        const char16 *name,
        ArenaAllocator *alloc,
        Scope *paramScope,
        Scope *bodyScope,
        ParseNode *pnode,
        Js::ParseableFunctionInfo* byteCodeFunction);
    uint NewIsInstInlineCache() {LOGMEIN("FuncInfo.h] 202\n"); return isInstInlineCacheCount++; }
    uint GetInlineCacheCount() const {LOGMEIN("FuncInfo.h] 203\n"); return this->inlineCacheCount; }
    uint GetRootObjectLoadInlineCacheCount() const {LOGMEIN("FuncInfo.h] 204\n"); return this->rootObjectLoadInlineCacheCount; }
    uint GetRootObjectLoadMethodInlineCacheCount() const {LOGMEIN("FuncInfo.h] 205\n"); return this->rootObjectLoadMethodInlineCacheCount; }
    uint GetRootObjectStoreInlineCacheCount() const {LOGMEIN("FuncInfo.h] 206\n"); return this->rootObjectStoreInlineCacheCount; }
    uint GetIsInstInlineCacheCount() const {LOGMEIN("FuncInfo.h] 207\n"); return this->isInstInlineCacheCount; }
    uint GetReferencedPropertyIdCount() const {LOGMEIN("FuncInfo.h] 208\n"); return this->referencedPropertyIdCount; }
    void SetFirstTmpReg(Js::RegSlot tmpReg)
    {LOGMEIN("FuncInfo.h] 210\n");
        Assert(this->firstTmpReg == Js::Constants::NoRegister);
        Assert(this->curTmpReg == Js::Constants::NoRegister);
        this->firstTmpReg = tmpReg;
        this->curTmpReg = tmpReg;
    }

    bool IsTmpReg(Js::RegSlot tmpReg)
    {LOGMEIN("FuncInfo.h] 218\n");
        Assert(this->firstTmpReg != Js::Constants::NoRegister);
        return !RegIsConst(tmpReg) && tmpReg >= firstTmpReg;
    }

    bool RegIsConst(Js::RegSlot reg)
    {LOGMEIN("FuncInfo.h] 224\n");
        // varRegsCount includes the tmp regs, so if reg number is larger than that,
        // then it must be in the negative range for const.
        return reg >= varRegsCount;
    }

    Js::RegSlot NextVarRegister()
    {LOGMEIN("FuncInfo.h] 231\n");
        AssertMsg(this->firstTmpReg == Js::Constants::NoRegister, "Shouldn't assign var register after we start allocating temp reg");
        Js::RegSlot reg = varRegsCount;
        UInt32Math::Inc(varRegsCount);
        return REGSLOT_TO_VARREG(reg);
    }

    Js::RegSlot NextConstRegister()
    {LOGMEIN("FuncInfo.h] 239\n");
        AssertMsg(this->firstTmpReg == Js::Constants::NoRegister, "Shouldn't assign var register after we start allocating temp reg");
        Js::RegSlot reg = constRegsCount;
        UInt32Math::Inc(constRegsCount);
        return REGSLOT_TO_CONSTREG(reg);
    }

    Js::RegSlot RegCount() const
    {LOGMEIN("FuncInfo.h] 247\n");
        return constRegsCount + varRegsCount;
    }

    uint InnerScopeCount() const {LOGMEIN("FuncInfo.h] 251\n"); return innerScopeCount; }
    uint CurrentInnerScopeIndex() const {LOGMEIN("FuncInfo.h] 252\n"); return currentInnerScopeIndex; }
    uint AcquireInnerScopeIndex();
    void ReleaseInnerScopeIndex();

    bool GetApplyEnclosesArgs() const {LOGMEIN("FuncInfo.h] 256\n"); return applyEnclosesArgs; }
    void SetApplyEnclosesArgs(bool b) {LOGMEIN("FuncInfo.h] 257\n"); applyEnclosesArgs=b; }

    bool IsGlobalFunction() const;

    // Fake global ->
    //    1) new Function code's global code
    //    2) global code generated from the reparsing deferred parse function

    bool IsFakeGlobalFunction(uint32 flags) const {LOGMEIN("FuncInfo.h] 265\n");
        return IsGlobalFunction() && !(flags & fscrGlobalCode);
    }

    Scope *GetBodyScope() const {LOGMEIN("FuncInfo.h] 269\n");
        return bodyScope;
    }

    Scope *GetParamScope() const {LOGMEIN("FuncInfo.h] 273\n");
        return paramScope;
    }

    Scope *GetTopLevelScope() const {LOGMEIN("FuncInfo.h] 277\n");
        // Top level scope will be the same for knopProg and knopFncDecl.
        return paramScope;
    }

    Scope* GetFuncExprScope() const {LOGMEIN("FuncInfo.h] 282\n");
        return funcExprScope;
    }

    void SetFuncExprScope(Scope* funcExprScope) {LOGMEIN("FuncInfo.h] 286\n");
        this->funcExprScope = funcExprScope;
    }

    Symbol *GetArgumentsSymbol() const
    {LOGMEIN("FuncInfo.h] 291\n");
        return argumentsSymbol;
    }

    void SetArgumentsSymbol(Symbol *sym)
    {LOGMEIN("FuncInfo.h] 296\n");
        Assert(argumentsSymbol == nullptr || argumentsSymbol == sym);
        argumentsSymbol = sym;
    }

    Symbol *GetInnerArgumentsSymbol() const
    {LOGMEIN("FuncInfo.h] 302\n");
        return innerArgumentsSymbol;
    }

    void SetInnerArgumentsSymbol(Symbol *sym)
    {LOGMEIN("FuncInfo.h] 307\n");
        Assert(innerArgumentsSymbol == nullptr || innerArgumentsSymbol == sym);
        innerArgumentsSymbol = sym;
    }

    bool IsInnerArgumentsSymbol(Symbol* sym)
    {LOGMEIN("FuncInfo.h] 313\n");
        return innerArgumentsSymbol != nullptr && innerArgumentsSymbol == sym;
    }

    bool GetCallsEval() const {LOGMEIN("FuncInfo.h] 317\n");
        return callsEval;
    }

    void SetCallsEval(bool does) {LOGMEIN("FuncInfo.h] 321\n");
        callsEval = does;
    }

    bool GetHasArguments() const {LOGMEIN("FuncInfo.h] 325\n");
        return hasArguments;
    }

    void SetHasArguments(bool has) {LOGMEIN("FuncInfo.h] 329\n");
        hasArguments = has;
    }

    bool GetHasHeapArguments() const
    {LOGMEIN("FuncInfo.h] 334\n");
        return hasHeapArguments;
    }

    void SetHasHeapArguments(bool has, bool optArgInBackend = false)
    {LOGMEIN("FuncInfo.h] 339\n");
        hasHeapArguments = has;
        byteCodeFunction->SetDoBackendArgumentsOptimization(optArgInBackend);
    }

    bool GetIsTopLevelEventHandler() const {LOGMEIN("FuncInfo.h] 344\n");
        return isTopLevelEventHandler;
    }

    void SetIsTopLevelEventHandler(bool is) {LOGMEIN("FuncInfo.h] 348\n");
        isTopLevelEventHandler = is;
    }

    bool GetChildCallsEval() const {LOGMEIN("FuncInfo.h] 352\n");
        return childCallsEval;
    }

    void SetChildCallsEval(bool does) {LOGMEIN("FuncInfo.h] 356\n");
        childCallsEval = does;
    }

    bool GetHasLocalInClosure() const {LOGMEIN("FuncInfo.h] 360\n");
        return hasLocalInClosure;
    }

    void SetHasLocalInClosure(bool has) {LOGMEIN("FuncInfo.h] 364\n");
        hasLocalInClosure = has;
    }

    bool GetHasClosureReference() const {LOGMEIN("FuncInfo.h] 368\n");
        return hasClosureReference;
    }

    void SetHasCachedScope(bool has) {LOGMEIN("FuncInfo.h] 372\n");
        hasCachedScope = has;
    }

    bool GetHasCachedScope() const {LOGMEIN("FuncInfo.h] 376\n");
        return hasCachedScope;
    }

    void SetFuncExprNameReference(bool has) {LOGMEIN("FuncInfo.h] 380\n");
        funcExprNameReference = has;
    }

    bool GetFuncExprNameReference() const {LOGMEIN("FuncInfo.h] 384\n");
        return funcExprNameReference;
    }

    void SetHasClosureReference(bool has) {LOGMEIN("FuncInfo.h] 388\n");
        hasClosureReference = has;
    }

    bool GetHasGlobalRef() const {LOGMEIN("FuncInfo.h] 392\n");
        return hasGlobalReference;
    }

    void SetHasGlobalRef(bool has) {LOGMEIN("FuncInfo.h] 396\n");
        hasGlobalReference = has;
    }

    bool GetIsStrictMode() const {LOGMEIN("FuncInfo.h] 400\n");
        return this->byteCodeFunction->GetIsStrictMode();
    }

    bool Escapes() const {LOGMEIN("FuncInfo.h] 404\n");
        return escapes;
    }

    void SetEscapes(bool does) {LOGMEIN("FuncInfo.h] 408\n");
        escapes = does;
    }

    bool HasMaybeEscapedNestedFunc() const {LOGMEIN("FuncInfo.h] 412\n");
        return hasEscapedUseNestedFunc;
    }

    void SetHasMaybeEscapedNestedFunc(DebugOnly(char16 const * reason));

    bool IsDeferred() const;

    bool IsRestored()
    {LOGMEIN("FuncInfo.h] 421\n");
        // FuncInfo are from RestoredScopeInfo
        return root == nullptr;
    }

    bool HasDeferredChild() const {LOGMEIN("FuncInfo.h] 426\n");
        return hasDeferredChild;
    }

    void SetHasDeferredChild() {LOGMEIN("FuncInfo.h] 430\n");
        hasDeferredChild = true;
    }

    bool HasRedeferrableChild() const {LOGMEIN("FuncInfo.h] 434\n");
        return hasRedeferrableChild;
    }

    void SetHasRedeferrableChild() {LOGMEIN("FuncInfo.h] 438\n");
        hasRedeferrableChild = true;
    }

    bool IsRedeferrable() const;

    Js::FunctionBody* GetParsedFunctionBody() const
    {LOGMEIN("FuncInfo.h] 445\n");
        AssertMsg(this->byteCodeFunction->IsFunctionParsed(), "Function must be parsed in order to call this method");
        Assert(!IsDeferred() || this->byteCodeFunction->GetFunctionBody()->GetByteCode() != nullptr);

        return this->byteCodeFunction->GetFunctionBody();
    }

    bool ChildHasWith() const {LOGMEIN("FuncInfo.h] 452\n");
        return childHasWith;
    }

    void SetChildHasWith() {LOGMEIN("FuncInfo.h] 456\n");
        childHasWith = true;
    }

    bool HasCapturedThis() const {LOGMEIN("FuncInfo.h] 460\n");
        return hasCapturedThis;
    }

    void SetHasCapturedThis() {LOGMEIN("FuncInfo.h] 464\n");
        hasCapturedThis = true;
    }

    BOOL HasSuperReference() const;
    BOOL HasDirectSuper() const;
    BOOL IsClassMember() const;
    BOOL IsLambda() const;
    BOOL IsClassConstructor() const;
    BOOL IsBaseClassConstructor() const;

    void RemoveTargetStmt(ParseNode* pnodeStmt) {LOGMEIN("FuncInfo.h] 475\n");
        targetStatements.Remove(pnodeStmt);
    }

    void AddTargetStmt(ParseNode *pnodeStmt) {LOGMEIN("FuncInfo.h] 479\n");
        targetStatements.Prepend(pnodeStmt);
    }

    Js::RegSlot LookupDouble(double d) {LOGMEIN("FuncInfo.h] 483\n");
        return doubleConstantToRegister.Lookup(d,Js::Constants::NoRegister);
    }

    bool TryGetDoubleLoc(double d, Js::RegSlot *loc) {LOGMEIN("FuncInfo.h] 487\n");
        Js::RegSlot ret=LookupDouble(d);
        *loc=ret;
        return(ret!=Js::Constants::NoRegister);
    }

    void AddDoubleConstant(double d, Js::RegSlot location) {LOGMEIN("FuncInfo.h] 493\n");
        doubleConstantToRegister.Item(d,location);
    }

    bool NeedEnvRegister() const {LOGMEIN("FuncInfo.h] 497\n"); return this->needEnvRegister; }
    void SetNeedEnvRegister() {LOGMEIN("FuncInfo.h] 498\n"); this->needEnvRegister = true; };
    Js::RegSlot GetEnvRegister() const
    {LOGMEIN("FuncInfo.h] 500\n");
        Assert(this->envRegister != Js::Constants::NoRegister);
        return this->envRegister;
    }
    Js::RegSlot AssignEnvRegister(bool constReg)
    {LOGMEIN("FuncInfo.h] 505\n");
        Assert(needEnvRegister);
        Assert(this->envRegister == Js::Constants::NoRegister);
        Js::RegSlot reg = constReg? NextConstRegister() : NextVarRegister();
        this->envRegister = reg;
        return reg;
    }

    Js::RegSlot AssignThisRegister()
    {LOGMEIN("FuncInfo.h] 514\n");
        if (this->thisPointerRegister == Js::Constants::NoRegister)
        {LOGMEIN("FuncInfo.h] 516\n");
            this->thisPointerRegister = NextVarRegister();
        }
        return this->thisPointerRegister;
    }

    Js::RegSlot AssignSuperRegister()
    {LOGMEIN("FuncInfo.h] 523\n");
        if (this->superRegister == Js::Constants::NoRegister)
        {LOGMEIN("FuncInfo.h] 525\n");
            this->superRegister = NextVarRegister();
        }
        return this->superRegister;
    }

    Js::RegSlot AssignSuperCtorRegister()
    {LOGMEIN("FuncInfo.h] 532\n");
        if (this->superCtorRegister == Js::Constants::NoRegister)
        {LOGMEIN("FuncInfo.h] 534\n");
            this->superCtorRegister = NextVarRegister();
        }
        return this->superCtorRegister;
    }

    Js::RegSlot AssignNewTargetRegister()
    {LOGMEIN("FuncInfo.h] 541\n");
        if (this->newTargetRegister == Js::Constants::NoRegister)
        {LOGMEIN("FuncInfo.h] 543\n");
            this->newTargetRegister = NextVarRegister();
        }
        return this->newTargetRegister;
    }

    Js::RegSlot AssignNullConstRegister()
    {LOGMEIN("FuncInfo.h] 550\n");
        if (this->nullConstantRegister == Js::Constants::NoRegister)
        {LOGMEIN("FuncInfo.h] 552\n");
            this->nullConstantRegister = NextConstRegister();
        }
        return this->nullConstantRegister;
    }

    Js::RegSlot AssignUndefinedConstRegister()
    {LOGMEIN("FuncInfo.h] 559\n");
        if (this->undefinedConstantRegister == Js::Constants::NoRegister)
        {LOGMEIN("FuncInfo.h] 561\n");
            this->undefinedConstantRegister = NextConstRegister();
        }
        return this->undefinedConstantRegister;
    }

    Js::RegSlot AssignTrueConstRegister()
    {LOGMEIN("FuncInfo.h] 568\n");
        if (this->trueConstantRegister == Js::Constants::NoRegister)
        {LOGMEIN("FuncInfo.h] 570\n");
            this->trueConstantRegister = NextConstRegister();
        }
        return this->trueConstantRegister;
    }

    Js::RegSlot AssignFalseConstRegister()
    {LOGMEIN("FuncInfo.h] 577\n");
        if (this->falseConstantRegister == Js::Constants::NoRegister)
        {LOGMEIN("FuncInfo.h] 579\n");
            this->falseConstantRegister = NextConstRegister();
        }
        return this->falseConstantRegister;
    }

    Js::RegSlot AssignYieldRegister()
    {LOGMEIN("FuncInfo.h] 586\n");
        AssertMsg(this->yieldRegister == Js::Constants::NoRegister, "yield register should only be assigned once by FinalizeRegisters()");
        this->yieldRegister = NextVarRegister();
        return this->yieldRegister;
    }

    Js::RegSlot GetLocalScopeSlotsReg()
    {LOGMEIN("FuncInfo.h] 593\n");
        return this->localClosureReg;
    }

    Js::RegSlot GetLocalFrameDisplayReg()
    {LOGMEIN("FuncInfo.h] 598\n");
        return this->localClosureReg + 1;
    }

    Js::RegSlot InnerScopeToRegSlot(Scope *scope) const;
    Js::RegSlot FirstInnerScopeReg() const;
    void SetFirstInnerScopeReg(Js::RegSlot reg);

    void StartRecordingOutArgs(unsigned int argCount)
    {LOGMEIN("FuncInfo.h] 607\n");
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
        {LOGMEIN("FuncInfo.h] 620\n");
            Js::Throw::OutOfMemory();
        }
        outArgsMaxDepth = max(outArgsMaxDepth, outArgsCurrentExpr);
    }

    void EndRecordingOutArgs(Js::ArgSlot argCount)
    {LOGMEIN("FuncInfo.h] 627\n");
        AssertMsg(outArgsDepth > 0, "mismatched Start and End");
        Assert(outArgsCurrentExpr >= argCount);
#if DBG
        outArgsDepth--;
#endif
        // Add one to pop the space to save the m_outParams pointer
        outArgsCurrentExpr -= (argCount + 1);

        Assert(outArgsDepth != 0 || outArgsCurrentExpr == 0);
    }

    uint GetMaxForInLoopLevel() const {LOGMEIN("FuncInfo.h] 639\n"); return this->maxForInLoopLevel;  }
    uint AcquireForInLoopLevel()
    {LOGMEIN("FuncInfo.h] 641\n");
        uint forInLoopLevel = this->nextForInLoopLevel++;
        this->maxForInLoopLevel = max(this->maxForInLoopLevel, this->nextForInLoopLevel);
        return forInLoopLevel;
    }

    void ReleaseForInLoopLevel(uint forInLoopLevel)
    {LOGMEIN("FuncInfo.h] 648\n");
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
    {LOGMEIN("FuncInfo.h] 665\n");
        Assert(instanceSlot != Js::Constants::NoRegister);
        Assert(propertySlot != Js::Constants::NoProperty);
        Assert(!isLoadMethod || !isStore);

        InlineCacheIdMap *properties;
        uint cacheId;

        if (isStore)
        {LOGMEIN("FuncInfo.h] 674\n");
            // ... = foo.toString;
            // foo.toString = ...;

            // We need a new cache here to ensure SetProperty() is called, which will set the side-effect bit
            // on the scriptContext.
            switch (propertySlot)
            {LOGMEIN("FuncInfo.h] 681\n");
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
        {LOGMEIN("FuncInfo.h] 695\n");
            properties = Anew(alloc, InlineCacheIdMap, alloc, 17);
            inlineCacheMap->Add(instanceSlot, properties);
        }

        InlineCacheList* cacheList;
        if (!properties->TryGetValue(propertySlot, &cacheList))
        {LOGMEIN("FuncInfo.h] 702\n");
            cacheList = Anew(alloc, InlineCacheList, alloc);
            properties->Add(propertySlot, cacheList);
        }

        // If we share inline caches we should never have more than one entry in the list.
        Assert(Js::FunctionBody::ShouldShareInlineCaches() || cacheList->Count() <= 1);

        InlineCacheUnit cacheIdUnit;

        if (Js::FunctionBody::ShouldShareInlineCaches() && !cacheList->Empty())
        {LOGMEIN("FuncInfo.h] 713\n");
            cacheIdUnit = cacheList->Head();
            if (isLoadMethod)
            {LOGMEIN("FuncInfo.h] 716\n");
                if (cacheIdUnit.loadMethodCacheId == (uint)-1)
                {LOGMEIN("FuncInfo.h] 718\n");
                    cacheIdUnit.loadMethodCacheId = this->NewInlineCache();
                }
                cacheId = cacheIdUnit.loadMethodCacheId;
            }
            else if (isStore)
            {LOGMEIN("FuncInfo.h] 724\n");
                if (cacheIdUnit.storeCacheId == (uint)-1)
                {LOGMEIN("FuncInfo.h] 726\n");
                    cacheIdUnit.storeCacheId = this->NewInlineCache();
                }
                cacheId = cacheIdUnit.storeCacheId;
            }
            else
            {
                if (cacheIdUnit.loadCacheId == (uint)-1)
                {LOGMEIN("FuncInfo.h] 734\n");
                    cacheIdUnit.loadCacheId = this->NewInlineCache();
                }
                cacheId = cacheIdUnit.loadCacheId;
            }
            cacheList->Head() = cacheIdUnit;
        }
        else
        {
            cacheId = this->NewInlineCache();
            if (Js::FunctionBody::ShouldShareInlineCaches())
            {LOGMEIN("FuncInfo.h] 745\n");
                if (isLoadMethod)
                {LOGMEIN("FuncInfo.h] 747\n");
                    cacheIdUnit.loadCacheId = (uint)-1;
                    cacheIdUnit.loadMethodCacheId = cacheId;
                    cacheIdUnit.storeCacheId = (uint)-1;
                }
                else if (isStore)
                {LOGMEIN("FuncInfo.h] 753\n");
                    cacheIdUnit.loadCacheId = (uint)-1;
                    cacheIdUnit.loadMethodCacheId = (uint)-1;
                    cacheIdUnit.storeCacheId = cacheId;
                }
                else
                {
                    cacheIdUnit.loadCacheId = cacheId;
                    cacheIdUnit.loadMethodCacheId = (uint)-1;
                    cacheIdUnit.storeCacheId = (uint)-1;
                }
            }
            else
            {
                cacheIdUnit.cacheId = cacheId;
            }
            cacheList->Prepend(cacheIdUnit);
        }

        return cacheId;
    }

    Js::ProfileId FindOrAddSlotProfileId(Scope* scope, Js::PropertyId propertyId)
    {LOGMEIN("FuncInfo.h] 776\n");
        SlotKey key;

        key.scope = scope;
        key.slot = propertyId;
        Js::ProfileId profileId = Js::Constants::NoProfileId;

        if (!this->slotProfileIdMap.TryGetValue(key, &profileId))
        {LOGMEIN("FuncInfo.h] 784\n");
            Assert(this->byteCodeFunction->IsFunctionParsed());
            if (this->byteCodeFunction->GetFunctionBody()->AllocProfiledSlotId(&profileId))
            {LOGMEIN("FuncInfo.h] 787\n");
                this->slotProfileIdMap.Add(key, profileId);
            }
        }

        return profileId;
    }

    void EnsureThisScopeSlot();
    void EnsureSuperScopeSlot();
    void EnsureSuperCtorScopeSlot();
    void EnsureNewTargetScopeSlot();
    void UseInnerSpecialScopeSlots();

    void SetIsThisLexicallyCaptured()
    {LOGMEIN("FuncInfo.h] 802\n");
        this->isThisLexicallyCaptured = true;
    }

    void SetIsSuperLexicallyCaptured()
    {LOGMEIN("FuncInfo.h] 807\n");
        this->isSuperLexicallyCaptured = true;
    }

    void SetIsSuperCtorLexicallyCaptured()
    {LOGMEIN("FuncInfo.h] 812\n");
        this->isSuperCtorLexicallyCaptured = true;
    }

    void SetIsNewTargetLexicallyCaptured()
    {LOGMEIN("FuncInfo.h] 817\n");
        this->isNewTargetLexicallyCaptured = true;
    }

    Scope * GetGlobalBlockScope() const;
    Scope * GetGlobalEvalBlockScope() const;

    FuncInfo *GetCurrentChildFunction() const
    {LOGMEIN("FuncInfo.h] 825\n");
        return this->currentChildFunction;
    }

    void SetCurrentChildFunction(FuncInfo *funcInfo)
    {LOGMEIN("FuncInfo.h] 830\n");
        this->currentChildFunction = funcInfo;
    }

    Scope *GetCurrentChildScope() const
    {LOGMEIN("FuncInfo.h] 835\n");
        return this->currentChildScope;
    }

    void SetCurrentChildScope(Scope *scope)
    {LOGMEIN("FuncInfo.h] 840\n");
        this->currentChildScope = scope;
    }

    SymbolTable *GetCapturedSyms() const {LOGMEIN("FuncInfo.h] 844\n"); return capturedSyms; }

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
