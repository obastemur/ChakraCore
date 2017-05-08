//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

enum SymbolType : byte
{
    STFunction,
    STVariable,
    STMemberName,
    STFormal,
    STUnknown
};

typedef JsUtil::CharacterBuffer<WCHAR> SymbolName;
class Symbol
{
private:
    const SymbolName name;
    IdentPtr pid;
    ParseNode *decl;
    Scope *scope;                   // scope defining this symbol
    Js::PropertyId position;        // argument position in function declaration
    Js::RegSlot location;           // register in which the symbol resides
    Js::PropertyId scopeSlot;
    Js::PropertyId moduleIndex;
    Symbol *next;

    SymbolType symbolType;
    BYTE defCount;
    BYTE needDeclaration : 1;
    BYTE isBlockVar : 1;
    BYTE isGlobal : 1;
    BYTE isEval : 1;
    BYTE hasNonLocalReference : 1;  // if true, then this symbol needs to be heap-allocated
    BYTE isFuncExpr : 1;              // if true, then this symbol is allocated on it's on activation object
    BYTE isCatch : 1;               // if true then this a catch identifier
    BYTE hasInit : 1;
    BYTE isUsed : 1;
    BYTE isGlobalCatch : 1;
    BYTE isCommittedToSlot : 1;
    BYTE hasNonCommittedReference : 1;
    BYTE hasRealBlockVarRef : 1;
    BYTE hasBlockFncVarRedecl : 1;
    BYTE hasVisitedCapturingFunc : 1;
    BYTE isTrackedForDebugger : 1; // Whether the sym is tracked for debugger scope. This is fine because a sym can only be added to (not more than) one scope.
    BYTE isModuleExportStorage : 1; // If true, this symbol should be stored in the global scope export storage array.
    BYTE isModuleImport : 1; // If true, this symbol is the local name of a module import statement

    // These are get and set a lot, don't put it in bit fields, we are exceeding the number of bits anyway
    bool hasFuncAssignment;
    bool hasMaybeEscapedUse;
    bool isNonSimpleParameter;

    AssignmentState assignmentState;

public:
    Symbol(SymbolName const& name, ParseNode *decl, SymbolType symbolType) :
        name(name),
        decl(decl),
        next(nullptr),
        location(Js::Constants::NoRegister),
        needDeclaration(false),
        isBlockVar(false),
        isGlobal(false),
        hasNonLocalReference(false),
        isFuncExpr(false),
        isCatch(false),
        hasInit(false),
        isUsed(false),
        defCount(0),
        position(Js::Constants::NoProperty),
        scopeSlot(Js::Constants::NoProperty),
        isGlobalCatch(false),
        isCommittedToSlot(false),
        hasNonCommittedReference(false),
        hasRealBlockVarRef(false),
        hasBlockFncVarRedecl(false),
        hasVisitedCapturingFunc(false),
        isTrackedForDebugger(false),
        isNonSimpleParameter(false),
        assignmentState(NotAssigned),
        isModuleExportStorage(false),
        isModuleImport(false),
        moduleIndex(Js::Constants::NoProperty)
    {TRACE_IT(42027);
        SetSymbolType(symbolType);

        // Set it so we don't have to check it explicitly
        isEval = MatchName(_u("eval"), 4);

        if (PHASE_TESTTRACE1(Js::StackFuncPhase) && hasFuncAssignment)
        {TRACE_IT(42028);
            Output::Print(_u("HasFuncDecl: %s\n"), this->GetName().GetBuffer());
            Output::Flush();
        }
    }

    bool MatchName(const char16 *key, int length)
    {TRACE_IT(42029);
        return name == SymbolName(key, length);
    }

    void SetScope(Scope *scope)
    {TRACE_IT(42030);
        this->scope = scope;
    }
    Scope * GetScope() const {TRACE_IT(42031); return scope; }

    void SetDecl(ParseNode *pnodeDecl) {TRACE_IT(42032); decl = pnodeDecl; }
    ParseNode* GetDecl() const {TRACE_IT(42033); return decl; }

    void SetScopeSlot(Js::PropertyId slot)
    {TRACE_IT(42034);
        this->scopeSlot = slot;
    }

    Symbol *GetNext() const
    {TRACE_IT(42035);
        return next;
    }

    void SetNext(Symbol *sym)
    {TRACE_IT(42036);
        next = sym;
    }

    void SetIsGlobal(bool b)
    {TRACE_IT(42037);
        isGlobal = b;
    }

    void SetHasNonLocalReference();

    bool GetHasNonLocalReference() const
    {TRACE_IT(42038);
        return hasNonLocalReference;
    }

    void SetIsFuncExpr(bool b)
    {TRACE_IT(42039);
        isFuncExpr = b;
    }

    void SetIsBlockVar(bool is)
    {TRACE_IT(42040);
        isBlockVar = is;
    }

    bool GetIsBlockVar() const
    {TRACE_IT(42041);
        return isBlockVar;
    }

    void SetIsModuleExportStorage(bool is)
    {TRACE_IT(42042);
        isModuleExportStorage = is;
    }

    bool GetIsModuleExportStorage() const
    {TRACE_IT(42043);
        return isModuleExportStorage;
    }

    void SetIsModuleImport(bool is)
    {TRACE_IT(42044);
        isModuleImport = is;
    }

    bool GetIsModuleImport() const
    {TRACE_IT(42045);
        return isModuleImport;
    }

    void SetModuleIndex(Js::PropertyId index)
    {TRACE_IT(42046);
        moduleIndex = index;
    }

    Js::PropertyId GetModuleIndex()
    {TRACE_IT(42047);
        return moduleIndex;
    }

    void SetIsGlobalCatch(bool is)
    {TRACE_IT(42048);
        isGlobalCatch = is;
    }

    bool GetIsGlobalCatch() const
    {TRACE_IT(42049);
        return isGlobalCatch;
    }

    void SetIsCommittedToSlot()
    {TRACE_IT(42050);
        this->isCommittedToSlot = true;
    }

    bool GetIsCommittedToSlot() const;

    void SetHasVisitedCapturingFunc()
    {TRACE_IT(42051);
        this->hasVisitedCapturingFunc = true;
    }

    bool HasVisitedCapturingFunc() const
    {TRACE_IT(42052);
        return hasVisitedCapturingFunc;
    }

    void SetHasNonCommittedReference(bool has)
    {TRACE_IT(42053);
        this->hasNonCommittedReference = has;
    }

    bool GetHasNonCommittedReference() const
    {TRACE_IT(42054);
        return hasNonCommittedReference;
    }

    void SetIsTrackedForDebugger(bool is)
    {TRACE_IT(42055);
        isTrackedForDebugger = is;
    }

    bool GetIsTrackedForDebugger() const
    {TRACE_IT(42056);
        return isTrackedForDebugger;
    }

    void SetNeedDeclaration(bool need)
    {TRACE_IT(42057);
        needDeclaration = need;
    }

    bool GetNeedDeclaration() const
    {TRACE_IT(42058);
        return needDeclaration;
    }

    bool GetIsFuncExpr() const
    {TRACE_IT(42059);
        return isFuncExpr;
    }

    bool GetIsGlobal() const
    {
        return isGlobal;
    }

    bool GetIsMember() const
    {TRACE_IT(42061);
        return symbolType == STMemberName;
    }

    bool GetIsFormal() const
    {TRACE_IT(42062);
        return symbolType == STFormal;
    }

    bool GetIsEval() const
    {TRACE_IT(42063);
        return isEval;
    }

    bool GetIsCatch() const
    {TRACE_IT(42064);
        return isCatch;
    }

    void SetIsCatch(bool b)
    {TRACE_IT(42065);
        isCatch = b;
    }

    bool GetHasInit() const
    {TRACE_IT(42066);
        return hasInit;
    }

    void RecordDef()
    {TRACE_IT(42067);
        defCount++;
    }

    bool SingleDef() const
    {TRACE_IT(42068);
        return defCount == 1;
    }

    void SetHasInit(bool has)
    {TRACE_IT(42069);
        hasInit = has;
    }

    bool GetIsUsed() const
    {TRACE_IT(42070);
        return isUsed;
    }

    void SetIsUsed(bool is)
    {TRACE_IT(42071);
        isUsed = is;
    }

    bool HasRealBlockVarRef() const
    {TRACE_IT(42072);
        return hasRealBlockVarRef;
    }

    void SetHasRealBlockVarRef(bool has = true)
    {TRACE_IT(42073);
        hasRealBlockVarRef = has;
    }

    bool HasBlockFncVarRedecl() const
    {TRACE_IT(42074);
        return hasBlockFncVarRedecl;
    }

    void SetHasBlockFncVarRedecl(bool has = true)
    {TRACE_IT(42075);
        hasBlockFncVarRedecl = has;
    }

    AssignmentState GetAssignmentState() const
    {TRACE_IT(42076);
        return assignmentState;
    }

    void PromoteAssignmentState()
    {TRACE_IT(42077);
        if (assignmentState == NotAssigned)
        {TRACE_IT(42078);
            assignmentState = AssignedOnce;
        }
        else if (assignmentState == AssignedOnce)
        {TRACE_IT(42079);
            assignmentState = AssignedMultipleTimes;
        }
    }

    bool IsAssignedOnce()
    {TRACE_IT(42080);
        return assignmentState == AssignedOnce;
    }

    // For stack nested function escape analysis
    bool GetHasMaybeEscapedUse() const {TRACE_IT(42081); return hasMaybeEscapedUse; }
    void SetHasMaybeEscapedUse(ByteCodeGenerator * byteCodeGenerator);
    bool GetHasFuncAssignment() const {TRACE_IT(42082); return hasFuncAssignment; }
    void SetHasFuncAssignment(ByteCodeGenerator * byteCodeGenerator);
    void RestoreHasFuncAssignment();

    bool GetIsNonSimpleParameter() const
    {TRACE_IT(42083);
        return isNonSimpleParameter;
    }

    void SetIsNonSimpleParameter(bool is)
    {TRACE_IT(42084);
        isNonSimpleParameter = is;
    }

    bool GetIsArguments() const;

    void SetPosition(Js::PropertyId pos)
    {TRACE_IT(42085);
        position = pos;
    }

    Js::PropertyId GetPosition()
    {TRACE_IT(42086);
        return position;
    }

    Js::PropertyId EnsurePosition(ByteCodeGenerator* byteCodeGenerator);
    Js::PropertyId EnsurePosition(FuncInfo *funcInfo);
    Js::PropertyId EnsurePositionNoCheck(FuncInfo *funcInfo);

    void SetLocation(Js::RegSlot location)
    {TRACE_IT(42087);
        this->location = location;
    }

    Js::RegSlot GetLocation()
    {TRACE_IT(42088);
        return location;
    }

    Js::PropertyId GetScopeSlot() const {TRACE_IT(42089); return scopeSlot; }
    bool HasScopeSlot() const {TRACE_IT(42090); return scopeSlot != Js::Constants::NoProperty; }

    SymbolType GetSymbolType()
    {TRACE_IT(42091);
        return symbolType;
    }

    void SetSymbolType(SymbolType symbolType)
    {TRACE_IT(42092);
        this->symbolType = symbolType;
        this->hasMaybeEscapedUse = GetIsFormal();
        this->hasFuncAssignment = (symbolType == STFunction);
    }

#if DBG_DUMP
    const char16 *GetSymbolTypeName();
#endif

    const JsUtil::CharacterBuffer<WCHAR>& GetName() const
    {TRACE_IT(42093);
        return this->name;
    }

    Js::PropertyId EnsureScopeSlot(FuncInfo *funcInfo);
    bool IsInSlot(FuncInfo *funcInfo, bool ensureSlotAlloc = false);
    bool NeedsSlotAlloc(FuncInfo *funcInfo);

    static void SaveToPropIdArray(Symbol *sym, Js::PropertyIdArray *propIds, ByteCodeGenerator *byteCodeGenerator, Js::PropertyId *pFirstSlot = nullptr);

    Symbol * GetFuncScopeVarSym() const;

    void SetPid(IdentPtr pid)
    {TRACE_IT(42094);
        this->pid = pid;
    }
    IdentPtr GetPid() const
    {TRACE_IT(42095);
        return pid;
    }

private:
    void SetHasMaybeEscapedUseInternal(ByteCodeGenerator * byteCodeGenerator);
    void SetHasFuncAssignmentInternal(ByteCodeGenerator * byteCodeGenerator);
};

// specialize toKey to use the name in the symbol as the key
template <>
inline SymbolName JsUtil::ValueToKey<SymbolName, Symbol *>::ToKey(Symbol * const& sym)
{TRACE_IT(42096);
    return sym->GetName();
}

typedef JsUtil::BaseHashSet<Symbol *, ArenaAllocator, PrimeSizePolicy, SymbolName, DefaultComparer, JsUtil::HashedEntry> SymbolTable;
