//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

enum ScopeType: int
{
    ScopeType_Unknown,
    ScopeType_Global,
    ScopeType_GlobalEvalBlock,
    ScopeType_FunctionBody,
    ScopeType_FuncExpr,
    ScopeType_Block,
    ScopeType_Catch,
    ScopeType_CatchParamPattern,
    ScopeType_With,
    ScopeType_Parameter
};

class Scope
{
private:
    Scope *enclosingScope;
    Js::RegSlot location;
    FuncInfo *func;
    Symbol *m_symList;
    int m_count;
    ArenaAllocator *alloc;
    uint scopeSlotCount; // count of slots in the local scope
    uint innerScopeIndex;
    ScopeType const scopeType;
    BYTE isDynamic : 1;
    BYTE isObject : 1;
    BYTE canMerge : 1;
    BYTE capturesAll : 1;
    BYTE mustInstantiate : 1;
    BYTE hasCrossScopeFuncAssignment : 1;
    BYTE hasDuplicateFormals : 1;
    BYTE canMergeWithBodyScope : 1;
    BYTE hasLocalInClosure : 1;
    BYTE isBlockInLoop : 1;
public:
#if DBG
    BYTE isRestored : 1;
#endif
    Scope(ArenaAllocator *alloc, ScopeType scopeType, int capacity = 0) :
        alloc(alloc),
        func(nullptr),
        enclosingScope(nullptr),
        isDynamic(false),
        isObject(false),
        canMerge(true),
        capturesAll(false),
        mustInstantiate(false),
        hasCrossScopeFuncAssignment(false),
        hasDuplicateFormals(false),
        canMergeWithBodyScope(true),
        hasLocalInClosure(false),
        isBlockInLoop(false),
        location(Js::Constants::NoRegister),
        m_symList(nullptr),
        m_count(0),
        scopeSlotCount(0),
        innerScopeIndex((uint)-1),
        scopeType(scopeType)
#if DBG
        , isRestored(false)
#endif
    {
    }

    Symbol *FindLocalSymbol(SymbolName const& key)
    {LOGMEIN("Scope.h] 73\n");
        Symbol *sym;
        for (sym = m_symList; sym; sym = sym->GetNext())
        {LOGMEIN("Scope.h] 76\n");
            if (sym->GetName() == key)
            {LOGMEIN("Scope.h] 78\n");
                break;
            }
        }
        return sym;
    }

    template<class Fn>
    void ForEachSymbol(Fn fn)
    {LOGMEIN("Scope.h] 87\n");
        for (Symbol *sym = m_symList; sym;)
        {LOGMEIN("Scope.h] 89\n");
            Symbol *next = sym->GetNext();
            fn(sym);
            sym = next;
        }
    }

    template<class Fn>
    void ForEachSymbolUntil(Fn fn)
    {LOGMEIN("Scope.h] 98\n");
        for (Symbol *sym = m_symList; sym;)
        {LOGMEIN("Scope.h] 100\n");
            Symbol *next = sym->GetNext();
            if (fn(sym))
            {LOGMEIN("Scope.h] 103\n");
                return;
            }
            sym = next;
        }
    }

    // For JScript, this should not return NULL because
    // there is always an enclosing global scope.
    Symbol *FindSymbol(SymbolName const& name, SymbolType symbolType, bool fCreate = true)
    {LOGMEIN("Scope.h] 113\n");
        Symbol *sym = FindLocalSymbol(name);
        if (sym == nullptr)
        {LOGMEIN("Scope.h] 116\n");
            if (enclosingScope != nullptr)
            {LOGMEIN("Scope.h] 118\n");
                sym = enclosingScope->FindSymbol(name, symbolType);
            }
            else if (fCreate)
            {LOGMEIN("Scope.h] 122\n");
                sym = Anew(alloc, Symbol, name, nullptr, symbolType);
                AddNewSymbol(sym);
            }
        }
        return sym;
    }

    void AddSymbol(Symbol *sym)
    {LOGMEIN("Scope.h] 131\n");
        if (enclosingScope == nullptr)
        {LOGMEIN("Scope.h] 133\n");
            sym->SetIsGlobal(true);
        }
        sym->SetScope(this);
        for (Symbol *symInList = m_symList; symInList; symInList = symInList->GetNext())
        {LOGMEIN("Scope.h] 138\n");
            if (symInList->GetName() == sym->GetName())
            {LOGMEIN("Scope.h] 140\n");
                return;
            }
        }
        sym->SetNext(m_symList);
        m_symList = sym;
        m_count++;
    }

    void AddNewSymbol(Symbol *sym)
    {LOGMEIN("Scope.h] 150\n");
        if (scopeType == ScopeType_Global)
        {LOGMEIN("Scope.h] 152\n");
            sym->SetIsGlobal(true);
        }
        sym->SetScope(this);
        sym->SetNext(m_symList);
        m_symList = sym;
        m_count++;
    }

    bool HasStaticPathToAncestor(Scope const * target) const
    {LOGMEIN("Scope.h] 162\n");
        return target == this || (!isDynamic && enclosingScope != nullptr && enclosingScope->HasStaticPathToAncestor(target));
    }

    void SetEnclosingScope(Scope *enclosingScope)
    {LOGMEIN("Scope.h] 167\n");
        // Check for scope cycles
        Assert(enclosingScope != this);
        Assert(enclosingScope == nullptr || this != enclosingScope->GetEnclosingScope());
        this->enclosingScope = enclosingScope;
    }

    Scope *GetEnclosingScope() const
    {LOGMEIN("Scope.h] 175\n");
        return enclosingScope;
    }

    ScopeType GetScopeType() const
    {LOGMEIN("Scope.h] 180\n");
        return this->scopeType;
    }

    bool IsInnerScope() const
    {LOGMEIN("Scope.h] 185\n");
        return scopeType == ScopeType_Block
            || scopeType == ScopeType_Catch
            || scopeType == ScopeType_CatchParamPattern
            || scopeType == ScopeType_GlobalEvalBlock;
    }

    int Count() const
    {LOGMEIN("Scope.h] 193\n");
        return m_count;
    }

    void SetFunc(FuncInfo *func)
    {LOGMEIN("Scope.h] 198\n");
        this->func = func;
    }

    FuncInfo *GetFunc() const
    {LOGMEIN("Scope.h] 203\n");
        return func;
    }

    FuncInfo *GetEnclosingFunc()
    {LOGMEIN("Scope.h] 208\n");
        Scope *scope = this;
        while (scope && scope->func == nullptr)
        {LOGMEIN("Scope.h] 211\n");
            scope = scope->GetEnclosingScope();
        }
        AnalysisAssert(scope);
        return scope->func;
    }

    void SetLocation(Js::RegSlot loc) {LOGMEIN("Scope.h] 218\n"); location = loc; }
    Js::RegSlot GetLocation() const {LOGMEIN("Scope.h] 219\n"); return location; }

    void SetIsDynamic(bool is) {LOGMEIN("Scope.h] 221\n"); isDynamic = is; }
    bool GetIsDynamic() const {LOGMEIN("Scope.h] 222\n"); return isDynamic; }

    bool IsEmpty() const;

    bool IsBlockScope(FuncInfo *funcInfo);

    void SetIsObject();
    bool GetIsObject() const {LOGMEIN("Scope.h] 229\n"); return isObject; }

    void SetCapturesAll(bool does) {LOGMEIN("Scope.h] 231\n"); capturesAll = does; }
    bool GetCapturesAll() const {LOGMEIN("Scope.h] 232\n"); return capturesAll; }

    void SetMustInstantiate(bool must) {LOGMEIN("Scope.h] 234\n"); mustInstantiate = must; }
    bool GetMustInstantiate() const {LOGMEIN("Scope.h] 235\n"); return mustInstantiate; }

    void SetCanMerge(bool can) {LOGMEIN("Scope.h] 237\n"); canMerge = can; }
    bool GetCanMerge() const {LOGMEIN("Scope.h] 238\n"); return canMerge && !mustInstantiate && !isObject; }

    void SetScopeSlotCount(uint i) {LOGMEIN("Scope.h] 240\n"); scopeSlotCount = i; }
    uint GetScopeSlotCount() const {LOGMEIN("Scope.h] 241\n"); return scopeSlotCount; }

    void SetHasDuplicateFormals() {LOGMEIN("Scope.h] 243\n"); hasDuplicateFormals = true; }
    bool GetHasDuplicateFormals() {LOGMEIN("Scope.h] 244\n"); return hasDuplicateFormals; }

    void SetCannotMergeWithBodyScope() {LOGMEIN("Scope.h] 246\n"); Assert(this->scopeType == ScopeType_Parameter); canMergeWithBodyScope = false; }
    bool GetCanMergeWithBodyScope() const {LOGMEIN("Scope.h] 247\n"); return canMergeWithBodyScope; }

    void SetHasOwnLocalInClosure(bool has) {LOGMEIN("Scope.h] 249\n"); hasLocalInClosure = has; }
    bool GetHasOwnLocalInClosure() const {LOGMEIN("Scope.h] 250\n"); return hasLocalInClosure; }

    void SetIsBlockInLoop(bool is = true) {LOGMEIN("Scope.h] 252\n"); isBlockInLoop = is; }
    bool IsBlockInLoop() const {LOGMEIN("Scope.h] 253\n"); return isBlockInLoop; }

    bool HasInnerScopeIndex() const {LOGMEIN("Scope.h] 255\n"); return innerScopeIndex != (uint)-1; }
    uint GetInnerScopeIndex() const {LOGMEIN("Scope.h] 256\n"); return innerScopeIndex; }
    void SetInnerScopeIndex(uint index) {LOGMEIN("Scope.h] 257\n"); innerScopeIndex = index; }

    int AddScopeSlot();

    void SetHasCrossScopeFuncAssignment() {LOGMEIN("Scope.h] 261\n"); hasCrossScopeFuncAssignment = true; }
    bool HasCrossScopeFuncAssignment() const {LOGMEIN("Scope.h] 262\n"); return hasCrossScopeFuncAssignment; }

    void ForceAllSymbolNonLocalReference(ByteCodeGenerator *byteCodeGenerator);

    bool IsGlobalEvalBlockScope() const;

    static void MergeParamAndBodyScopes(ParseNode *pnodeScope);
    static void RemoveParamScope(ParseNode *pnodeScope);
};
