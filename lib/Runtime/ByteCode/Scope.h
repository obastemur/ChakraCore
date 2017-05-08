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
    {TRACE_IT(41831);
        Symbol *sym;
        for (sym = m_symList; sym; sym = sym->GetNext())
        {TRACE_IT(41832);
            if (sym->GetName() == key)
            {TRACE_IT(41833);
                break;
            }
        }
        return sym;
    }

    template<class Fn>
    void ForEachSymbol(Fn fn)
    {TRACE_IT(41834);
        for (Symbol *sym = m_symList; sym;)
        {TRACE_IT(41835);
            Symbol *next = sym->GetNext();
            fn(sym);
            sym = next;
        }
    }

    template<class Fn>
    void ForEachSymbolUntil(Fn fn)
    {TRACE_IT(41836);
        for (Symbol *sym = m_symList; sym;)
        {TRACE_IT(41837);
            Symbol *next = sym->GetNext();
            if (fn(sym))
            {TRACE_IT(41838);
                return;
            }
            sym = next;
        }
    }

    // For JScript, this should not return NULL because
    // there is always an enclosing global scope.
    Symbol *FindSymbol(SymbolName const& name, SymbolType symbolType, bool fCreate = true)
    {TRACE_IT(41839);
        Symbol *sym = FindLocalSymbol(name);
        if (sym == nullptr)
        {TRACE_IT(41840);
            if (enclosingScope != nullptr)
            {TRACE_IT(41841);
                sym = enclosingScope->FindSymbol(name, symbolType);
            }
            else if (fCreate)
            {TRACE_IT(41842);
                sym = Anew(alloc, Symbol, name, nullptr, symbolType);
                AddNewSymbol(sym);
            }
        }
        return sym;
    }

    void AddSymbol(Symbol *sym)
    {TRACE_IT(41843);
        if (enclosingScope == nullptr)
        {TRACE_IT(41844);
            sym->SetIsGlobal(true);
        }
        sym->SetScope(this);
        for (Symbol *symInList = m_symList; symInList; symInList = symInList->GetNext())
        {TRACE_IT(41845);
            if (symInList->GetName() == sym->GetName())
            {TRACE_IT(41846);
                return;
            }
        }
        sym->SetNext(m_symList);
        m_symList = sym;
        m_count++;
    }

    void AddNewSymbol(Symbol *sym)
    {TRACE_IT(41847);
        if (scopeType == ScopeType_Global)
        {TRACE_IT(41848);
            sym->SetIsGlobal(true);
        }
        sym->SetScope(this);
        sym->SetNext(m_symList);
        m_symList = sym;
        m_count++;
    }

    bool HasStaticPathToAncestor(Scope const * target) const
    {TRACE_IT(41849);
        return target == this || (!isDynamic && enclosingScope != nullptr && enclosingScope->HasStaticPathToAncestor(target));
    }

    void SetEnclosingScope(Scope *enclosingScope)
    {TRACE_IT(41850);
        // Check for scope cycles
        Assert(enclosingScope != this);
        Assert(enclosingScope == nullptr || this != enclosingScope->GetEnclosingScope());
        this->enclosingScope = enclosingScope;
    }

    Scope *GetEnclosingScope() const
    {TRACE_IT(41851);
        return enclosingScope;
    }

    ScopeType GetScopeType() const
    {TRACE_IT(41852);
        return this->scopeType;
    }

    bool IsInnerScope() const
    {TRACE_IT(41853);
        return scopeType == ScopeType_Block
            || scopeType == ScopeType_Catch
            || scopeType == ScopeType_CatchParamPattern
            || scopeType == ScopeType_GlobalEvalBlock;
    }

    int Count() const
    {TRACE_IT(41854);
        return m_count;
    }

    void SetFunc(FuncInfo *func)
    {TRACE_IT(41855);
        this->func = func;
    }

    FuncInfo *GetFunc() const
    {TRACE_IT(41856);
        return func;
    }

    FuncInfo *GetEnclosingFunc()
    {TRACE_IT(41857);
        Scope *scope = this;
        while (scope && scope->func == nullptr)
        {TRACE_IT(41858);
            scope = scope->GetEnclosingScope();
        }
        AnalysisAssert(scope);
        return scope->func;
    }

    void SetLocation(Js::RegSlot loc) {TRACE_IT(41859); location = loc; }
    Js::RegSlot GetLocation() const {TRACE_IT(41860); return location; }

    void SetIsDynamic(bool is) {TRACE_IT(41861); isDynamic = is; }
    bool GetIsDynamic() const {TRACE_IT(41862); return isDynamic; }

    bool IsEmpty() const;

    bool IsBlockScope(FuncInfo *funcInfo);

    void SetIsObject();
    bool GetIsObject() const {TRACE_IT(41863); return isObject; }

    void SetCapturesAll(bool does) {TRACE_IT(41864); capturesAll = does; }
    bool GetCapturesAll() const {TRACE_IT(41865); return capturesAll; }

    void SetMustInstantiate(bool must) {TRACE_IT(41866); mustInstantiate = must; }
    bool GetMustInstantiate() const {TRACE_IT(41867); return mustInstantiate; }

    void SetCanMerge(bool can) {TRACE_IT(41868); canMerge = can; }
    bool GetCanMerge() const {TRACE_IT(41869); return canMerge && !mustInstantiate && !isObject; }

    void SetScopeSlotCount(uint i) {TRACE_IT(41870); scopeSlotCount = i; }
    uint GetScopeSlotCount() const {TRACE_IT(41871); return scopeSlotCount; }

    void SetHasDuplicateFormals() {TRACE_IT(41872); hasDuplicateFormals = true; }
    bool GetHasDuplicateFormals() {TRACE_IT(41873); return hasDuplicateFormals; }

    void SetHasOwnLocalInClosure(bool has) {TRACE_IT(41874); hasLocalInClosure = has; }
    bool GetHasOwnLocalInClosure() const {TRACE_IT(41875); return hasLocalInClosure; }

    void SetIsBlockInLoop(bool is = true) {TRACE_IT(41876); isBlockInLoop = is; }
    bool IsBlockInLoop() const {TRACE_IT(41877); return isBlockInLoop; }

    bool HasInnerScopeIndex() const {TRACE_IT(41878); return innerScopeIndex != (uint)-1; }
    uint GetInnerScopeIndex() const {TRACE_IT(41879); return innerScopeIndex; }
    void SetInnerScopeIndex(uint index) {TRACE_IT(41880); innerScopeIndex = index; }

    int AddScopeSlot();

    void SetHasCrossScopeFuncAssignment() {TRACE_IT(41881); hasCrossScopeFuncAssignment = true; }
    bool HasCrossScopeFuncAssignment() const {TRACE_IT(41882); return hasCrossScopeFuncAssignment; }

    void ForceAllSymbolNonLocalReference(ByteCodeGenerator *byteCodeGenerator);

    bool IsGlobalEvalBlockScope() const;

    static void MergeParamAndBodyScopes(ParseNode *pnodeScope);
    static void RemoveParamScope(ParseNode *pnodeScope);
};
