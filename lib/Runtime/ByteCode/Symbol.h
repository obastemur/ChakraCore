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
    {LOGMEIN("Symbol.h] 85\n");
        SetSymbolType(symbolType);

        // Set it so we don't have to check it explicitly
        isEval = MatchName(_u("eval"), 4);

        if (PHASE_TESTTRACE1(Js::StackFuncPhase) && hasFuncAssignment)
        {LOGMEIN("Symbol.h] 92\n");
            Output::Print(_u("HasFuncDecl: %s\n"), this->GetName().GetBuffer());
            Output::Flush();
        }
    }

    bool MatchName(const char16 *key, int length)
    {LOGMEIN("Symbol.h] 99\n");
        return name == SymbolName(key, length);
    }

    void SetScope(Scope *scope)
    {LOGMEIN("Symbol.h] 104\n");
        this->scope = scope;
    }
    Scope * GetScope() const {LOGMEIN("Symbol.h] 107\n"); return scope; }

    void SetDecl(ParseNode *pnodeDecl) {LOGMEIN("Symbol.h] 109\n"); decl = pnodeDecl; }
    ParseNode* GetDecl() const {LOGMEIN("Symbol.h] 110\n"); return decl; }

    void SetScopeSlot(Js::PropertyId slot)
    {LOGMEIN("Symbol.h] 113\n");
        this->scopeSlot = slot;
    }

    Symbol *GetNext() const
    {LOGMEIN("Symbol.h] 118\n");
        return next;
    }

    void SetNext(Symbol *sym)
    {LOGMEIN("Symbol.h] 123\n");
        next = sym;
    }

    void SetIsGlobal(bool b)
    {LOGMEIN("Symbol.h] 128\n");
        isGlobal = b;
    }

    void SetHasNonLocalReference();

    bool GetHasNonLocalReference() const
    {LOGMEIN("Symbol.h] 135\n");
        return hasNonLocalReference;
    }

    void SetIsFuncExpr(bool b)
    {LOGMEIN("Symbol.h] 140\n");
        isFuncExpr = b;
    }

    void SetIsBlockVar(bool is)
    {LOGMEIN("Symbol.h] 145\n");
        isBlockVar = is;
    }

    bool GetIsBlockVar() const
    {LOGMEIN("Symbol.h] 150\n");
        return isBlockVar;
    }

    void SetIsModuleExportStorage(bool is)
    {LOGMEIN("Symbol.h] 155\n");
        isModuleExportStorage = is;
    }

    bool GetIsModuleExportStorage() const
    {LOGMEIN("Symbol.h] 160\n");
        return isModuleExportStorage;
    }

    void SetIsModuleImport(bool is)
    {LOGMEIN("Symbol.h] 165\n");
        isModuleImport = is;
    }

    bool GetIsModuleImport() const
    {LOGMEIN("Symbol.h] 170\n");
        return isModuleImport;
    }

    void SetModuleIndex(Js::PropertyId index)
    {LOGMEIN("Symbol.h] 175\n");
        moduleIndex = index;
    }

    Js::PropertyId GetModuleIndex()
    {LOGMEIN("Symbol.h] 180\n");
        return moduleIndex;
    }

    void SetIsGlobalCatch(bool is)
    {LOGMEIN("Symbol.h] 185\n");
        isGlobalCatch = is;
    }

    bool GetIsGlobalCatch() const
    {LOGMEIN("Symbol.h] 190\n");
        return isGlobalCatch;
    }

    void SetIsCommittedToSlot()
    {LOGMEIN("Symbol.h] 195\n");
        this->isCommittedToSlot = true;
    }

    bool GetIsCommittedToSlot() const;

    void SetHasVisitedCapturingFunc()
    {LOGMEIN("Symbol.h] 202\n");
        this->hasVisitedCapturingFunc = true;
    }

    bool HasVisitedCapturingFunc() const
    {LOGMEIN("Symbol.h] 207\n");
        return hasVisitedCapturingFunc;
    }

    void SetHasNonCommittedReference(bool has)
    {LOGMEIN("Symbol.h] 212\n");
        this->hasNonCommittedReference = has;
    }

    bool GetHasNonCommittedReference() const
    {LOGMEIN("Symbol.h] 217\n");
        return hasNonCommittedReference;
    }

    void SetIsTrackedForDebugger(bool is)
    {LOGMEIN("Symbol.h] 222\n");
        isTrackedForDebugger = is;
    }

    bool GetIsTrackedForDebugger() const
    {LOGMEIN("Symbol.h] 227\n");
        return isTrackedForDebugger;
    }

    void SetNeedDeclaration(bool need)
    {LOGMEIN("Symbol.h] 232\n");
        needDeclaration = need;
    }

    bool GetNeedDeclaration() const
    {LOGMEIN("Symbol.h] 237\n");
        return needDeclaration;
    }

    bool GetIsFuncExpr() const
    {LOGMEIN("Symbol.h] 242\n");
        return isFuncExpr;
    }

    bool GetIsGlobal() const
    {LOGMEIN("Symbol.h] 247\n");
        return isGlobal;
    }

    bool GetIsMember() const
    {LOGMEIN("Symbol.h] 252\n");
        return symbolType == STMemberName;
    }

    bool GetIsFormal() const
    {LOGMEIN("Symbol.h] 257\n");
        return symbolType == STFormal;
    }

    bool GetIsEval() const
    {LOGMEIN("Symbol.h] 262\n");
        return isEval;
    }

    bool GetIsCatch() const
    {LOGMEIN("Symbol.h] 267\n");
        return isCatch;
    }

    void SetIsCatch(bool b)
    {LOGMEIN("Symbol.h] 272\n");
        isCatch = b;
    }

    bool GetHasInit() const
    {LOGMEIN("Symbol.h] 277\n");
        return hasInit;
    }

    void RecordDef()
    {LOGMEIN("Symbol.h] 282\n");
        defCount++;
    }

    bool SingleDef() const
    {LOGMEIN("Symbol.h] 287\n");
        return defCount == 1;
    }

    void SetHasInit(bool has)
    {LOGMEIN("Symbol.h] 292\n");
        hasInit = has;
    }

    bool GetIsUsed() const
    {LOGMEIN("Symbol.h] 297\n");
        return isUsed;
    }

    void SetIsUsed(bool is)
    {LOGMEIN("Symbol.h] 302\n");
        isUsed = is;
    }

    bool HasRealBlockVarRef() const
    {LOGMEIN("Symbol.h] 307\n");
        return hasRealBlockVarRef;
    }

    void SetHasRealBlockVarRef(bool has = true)
    {LOGMEIN("Symbol.h] 312\n");
        hasRealBlockVarRef = has;
    }

    bool HasBlockFncVarRedecl() const
    {LOGMEIN("Symbol.h] 317\n");
        return hasBlockFncVarRedecl;
    }

    void SetHasBlockFncVarRedecl(bool has = true)
    {LOGMEIN("Symbol.h] 322\n");
        hasBlockFncVarRedecl = has;
    }

    AssignmentState GetAssignmentState() const
    {LOGMEIN("Symbol.h] 327\n");
        return assignmentState;
    }

    void PromoteAssignmentState()
    {LOGMEIN("Symbol.h] 332\n");
        if (assignmentState == NotAssigned)
        {LOGMEIN("Symbol.h] 334\n");
            assignmentState = AssignedOnce;
        }
        else if (assignmentState == AssignedOnce)
        {LOGMEIN("Symbol.h] 338\n");
            assignmentState = AssignedMultipleTimes;
        }
    }

    bool IsAssignedOnce()
    {LOGMEIN("Symbol.h] 344\n");
        return assignmentState == AssignedOnce;
    }

    // For stack nested function escape analysis
    bool GetHasMaybeEscapedUse() const {LOGMEIN("Symbol.h] 349\n"); return hasMaybeEscapedUse; }
    void SetHasMaybeEscapedUse(ByteCodeGenerator * byteCodeGenerator);
    bool GetHasFuncAssignment() const {LOGMEIN("Symbol.h] 351\n"); return hasFuncAssignment; }
    void SetHasFuncAssignment(ByteCodeGenerator * byteCodeGenerator);
    void RestoreHasFuncAssignment();

    bool GetIsNonSimpleParameter() const
    {LOGMEIN("Symbol.h] 356\n");
        return isNonSimpleParameter;
    }

    void SetIsNonSimpleParameter(bool is)
    {LOGMEIN("Symbol.h] 361\n");
        isNonSimpleParameter = is;
    }

    bool GetIsArguments() const;

    void SetPosition(Js::PropertyId pos)
    {LOGMEIN("Symbol.h] 368\n");
        position = pos;
    }

    Js::PropertyId GetPosition()
    {LOGMEIN("Symbol.h] 373\n");
        return position;
    }

    Js::PropertyId EnsurePosition(ByteCodeGenerator* byteCodeGenerator);
    Js::PropertyId EnsurePosition(FuncInfo *funcInfo);
    Js::PropertyId EnsurePositionNoCheck(FuncInfo *funcInfo);

    void SetLocation(Js::RegSlot location)
    {LOGMEIN("Symbol.h] 382\n");
        this->location = location;
    }

    Js::RegSlot GetLocation()
    {LOGMEIN("Symbol.h] 387\n");
        return location;
    }

    Js::PropertyId GetScopeSlot() const {LOGMEIN("Symbol.h] 391\n"); return scopeSlot; }
    bool HasScopeSlot() const {LOGMEIN("Symbol.h] 392\n"); return scopeSlot != Js::Constants::NoProperty; }

    SymbolType GetSymbolType()
    {LOGMEIN("Symbol.h] 395\n");
        return symbolType;
    }

    void SetSymbolType(SymbolType symbolType)
    {LOGMEIN("Symbol.h] 400\n");
        this->symbolType = symbolType;
        this->hasMaybeEscapedUse = GetIsFormal();
        this->hasFuncAssignment = (symbolType == STFunction);
    }

#if DBG_DUMP
    const char16 *GetSymbolTypeName();
#endif

    const JsUtil::CharacterBuffer<WCHAR>& GetName() const
    {LOGMEIN("Symbol.h] 411\n");
        return this->name;
    }

    Js::PropertyId EnsureScopeSlot(FuncInfo *funcInfo);
    bool IsInSlot(FuncInfo *funcInfo, bool ensureSlotAlloc = false);
    bool NeedsSlotAlloc(FuncInfo *funcInfo);

    static void SaveToPropIdArray(Symbol *sym, Js::PropertyIdArray *propIds, ByteCodeGenerator *byteCodeGenerator, Js::PropertyId *pFirstSlot = nullptr);

    Symbol * GetFuncScopeVarSym() const;

    void SetPid(IdentPtr pid)
    {LOGMEIN("Symbol.h] 424\n");
        this->pid = pid;
    }
    IdentPtr GetPid() const
    {LOGMEIN("Symbol.h] 428\n");
        return pid;
    }

private:
    void SetHasMaybeEscapedUseInternal(ByteCodeGenerator * byteCodeGenerator);
    void SetHasFuncAssignmentInternal(ByteCodeGenerator * byteCodeGenerator);
};

// specialize toKey to use the name in the symbol as the key
template <>
inline SymbolName JsUtil::ValueToKey<SymbolName, Symbol *>::ToKey(Symbol * const& sym)
{LOGMEIN("Symbol.h] 440\n");
    return sym->GetName();
}

typedef JsUtil::BaseHashSet<Symbol *, ArenaAllocator, PrimeSizePolicy, SymbolName, DefaultComparer, JsUtil::HashedEntry> SymbolTable;
