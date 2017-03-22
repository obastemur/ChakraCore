//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

namespace Js
{
    //
    // Persist one symbol info into ScopeInfo.
    //
    void ScopeInfo::SaveSymbolInfo(Symbol* sym, MapSymbolData* mapSymbolData)
    {LOGMEIN("ScopeInfo.cpp] 12\n");
        // We don't need to create slot for or save "arguments"
        bool needScopeSlot = !sym->GetIsArguments() && sym->GetHasNonLocalReference()
            && (!mapSymbolData->func->IsInnerArgumentsSymbol(sym) || mapSymbolData->func->GetHasArguments());
        Js::PropertyId scopeSlot = Constants::NoSlot;

        if (sym->GetIsModuleExportStorage())
        {LOGMEIN("ScopeInfo.cpp] 19\n");
            // Export symbols aren't in slots but we need to persist the fact that they are export storage
            scopeSlot = sym->GetScope()->GetScopeSlotCount() + mapSymbolData->nonScopeSymbolCount++;
        }
        else if (needScopeSlot)
        {LOGMEIN("ScopeInfo.cpp] 24\n");
            // Any symbol may have non-local ref from deferred child. Allocate slot for it.
            scopeSlot = sym->EnsureScopeSlot(mapSymbolData->func);
        }

        if (needScopeSlot || sym->GetIsModuleExportStorage())
        {LOGMEIN("ScopeInfo.cpp] 30\n");
            Js::PropertyId propertyId = sym->EnsurePosition(mapSymbolData->func);
            this->SetSymbolId(scopeSlot, propertyId);
            this->SetSymbolType(scopeSlot, sym->GetSymbolType());
            this->SetHasFuncAssignment(scopeSlot, sym->GetHasFuncAssignment());
            this->SetIsBlockVariable(scopeSlot, sym->GetIsBlockVar());
            this->SetIsFuncExpr(scopeSlot, sym->GetIsFuncExpr());
            this->SetIsModuleExportStorage(scopeSlot, sym->GetIsModuleExportStorage());
            this->SetIsModuleImport(scopeSlot, sym->GetIsModuleImport());
        }

        TRACE_BYTECODE(_u("%12s %d\n"), sym->GetName().GetBuffer(), sym->GetScopeSlot());
    }

    //
    // Create scope info for a deferred child to refer to its parent ParseableFunctionInfo.
    //
    ScopeInfo* ScopeInfo::FromParent(FunctionBody* parent)
    {LOGMEIN("ScopeInfo.cpp] 48\n");
        return RecyclerNew(parent->GetScriptContext()->GetRecycler(), // Alloc with ParseableFunctionInfo
            ScopeInfo, parent->GetFunctionInfo(), 0);
    }

    inline void AddSlotCount(int& count, int addCount)
    {LOGMEIN("ScopeInfo.cpp] 54\n");
        if (addCount != 0 && Int32Math::Add(count, addCount, &count))
        {LOGMEIN("ScopeInfo.cpp] 56\n");
            ::Math::DefaultOverflowPolicy();
        }
    }

    //
    // Create scope info for an outer scope.
    //
    ScopeInfo* ScopeInfo::FromScope(ByteCodeGenerator* byteCodeGenerator, ParseableFunctionInfo* parent, Scope* scope, ScriptContext *scriptContext)
    {LOGMEIN("ScopeInfo.cpp] 65\n");
        int count = scope->Count();

        // Add argsPlaceHolder which includes same name args and destructuring patterns on parameters
        AddSlotCount(count, scope->GetFunc()->argsPlaceHolderSlotCount);
        AddSlotCount(count, scope->GetFunc()->thisScopeSlot != Js::Constants::NoRegister ? 1 : 0);
        AddSlotCount(count, scope->GetFunc()->superScopeSlot != Js::Constants::NoRegister ? 1 : 0);
        AddSlotCount(count, scope->GetFunc()->newTargetScopeSlot != Js::Constants::NoRegister ? 1 : 0);

        ScopeInfo* scopeInfo = RecyclerNewPlusZ(scriptContext->GetRecycler(),
            count * sizeof(SymbolInfo),
            ScopeInfo, parent ? parent->GetFunctionInfo() : nullptr, count);
        scopeInfo->isDynamic = scope->GetIsDynamic();
        scopeInfo->isObject = scope->GetIsObject();
        scopeInfo->mustInstantiate = scope->GetMustInstantiate();
        scopeInfo->isCached = (scope->GetFunc()->GetBodyScope() == scope) && scope->GetFunc()->GetHasCachedScope();
        scopeInfo->isGlobalEval = scope->GetScopeType() == ScopeType_GlobalEvalBlock;
        scopeInfo->canMergeWithBodyScope = scope->GetCanMergeWithBodyScope();
        scopeInfo->hasLocalInClosure = scope->GetHasOwnLocalInClosure();

        TRACE_BYTECODE(_u("\nSave ScopeInfo: %s parent: %s #symbols: %d %s\n"),
            scope->GetFunc()->name, parent ? parent->GetDisplayName() : _u(""), count,
            scopeInfo->isObject ? _u("isObject") : _u(""));

        MapSymbolData mapSymbolData = { byteCodeGenerator, scope->GetFunc(), 0 };
        scope->ForEachSymbol([&mapSymbolData, scopeInfo, scope](Symbol * sym)
        {
            Assert(scope == sym->GetScope());
            scopeInfo->SaveSymbolInfo(sym, &mapSymbolData);
        });

        return scopeInfo;
    }

    //
    // Ensure the pids referenced by this scope are tracked.
    //
    void ScopeInfo::EnsurePidTracking(ScriptContext* scriptContext)
    {LOGMEIN("ScopeInfo.cpp] 103\n");
        for (int i = 0; i < symbolCount; i++)
        {LOGMEIN("ScopeInfo.cpp] 105\n");
            auto propertyName = scriptContext->GetPropertyName(symbols[i].propertyId);
            scriptContext->TrackPid(propertyName);
        }
        if (funcExprScopeInfo)
        {LOGMEIN("ScopeInfo.cpp] 110\n");
            funcExprScopeInfo->EnsurePidTracking(scriptContext);
        }
        if (paramScopeInfo)
        {LOGMEIN("ScopeInfo.cpp] 114\n");
            paramScopeInfo->EnsurePidTracking(scriptContext);
        }
    }

    //
    // Save needed scope info for a deferred child func. The scope info is empty and only links to parent.
    //
    void ScopeInfo::SaveParentScopeInfo(FuncInfo* parentFunc, FuncInfo* func)
    {LOGMEIN("ScopeInfo.cpp] 123\n");
        Assert(func->IsDeferred() || func->byteCodeFunction->CanBeDeferred());

        // Parent must be parsed
        FunctionBody* parent = parentFunc->byteCodeFunction->GetFunctionBody();
        ParseableFunctionInfo* funcBody = func->byteCodeFunction;

        TRACE_BYTECODE(_u("\nSave ScopeInfo: %s parent: %s\n\n"),
            funcBody->GetDisplayName(), parent->GetDisplayName());

        ScopeInfo *info = FromParent(parent);
        info->parentOnly = true;
        funcBody->SetScopeInfo(info);
    }

    //
    // Save scope info for an outer func which has deferred child.
    //
    void ScopeInfo::SaveScopeInfo(ByteCodeGenerator* byteCodeGenerator, FuncInfo* parentFunc, FuncInfo* func)
    {LOGMEIN("ScopeInfo.cpp] 142\n");
        ParseableFunctionInfo* funcBody = func->byteCodeFunction;

        Assert((!func->IsGlobalFunction() || byteCodeGenerator->GetFlags() & fscrEvalCode) &&
            (func->HasDeferredChild() || func->HasRedeferrableChild() || funcBody->IsReparsed()));

        // If we are reparsing a deferred function, we already have correct "parent" info in
        // funcBody->scopeInfo. parentFunc is the knopProg shell and should not be used in this
        // case. We should use existing parent if available.
        ParseableFunctionInfo * parent = funcBody->GetScopeInfo() ?
            funcBody->GetScopeInfo()->GetParent() :
            parentFunc ? parentFunc->byteCodeFunction : nullptr;

        ScopeInfo* funcExprScopeInfo = nullptr;
        Scope* funcExprScope = func->GetFuncExprScope();
        if (funcExprScope && funcExprScope->GetMustInstantiate())
        {LOGMEIN("ScopeInfo.cpp] 158\n");
            funcExprScopeInfo = FromScope(byteCodeGenerator, parent, funcExprScope, funcBody->GetScriptContext());
        }

        Scope* bodyScope = func->IsGlobalFunction() ? func->GetGlobalEvalBlockScope() : func->GetBodyScope();
        ScopeInfo* paramScopeInfo = nullptr;
        Scope* paramScope = func->GetParamScope();
        if (paramScope && !paramScope->GetCanMergeWithBodyScope())
        {LOGMEIN("ScopeInfo.cpp] 166\n");
            paramScopeInfo = FromScope(byteCodeGenerator, parent, paramScope, funcBody->GetScriptContext());
        }

        ScopeInfo* scopeInfo = FromScope(byteCodeGenerator, parent, bodyScope, funcBody->GetScriptContext());
        scopeInfo->SetFuncExprScopeInfo(funcExprScopeInfo);
        scopeInfo->SetParamScopeInfo(paramScopeInfo);

        funcBody->SetScopeInfo(scopeInfo);
    }

    void ScopeInfo::SaveScopeInfoForDeferParse(ByteCodeGenerator* byteCodeGenerator, FuncInfo* parentFunc, FuncInfo* funcInfo)
    {LOGMEIN("ScopeInfo.cpp] 178\n");
        // TODO: Not technically necessary, as we always do scope look up on eval if it is
        // not found in the scope chain, and block scopes are always objects in eval.
        // But if we save the global eval block scope for deferred child so that we can look up
        // let/const in that scope with slot index instead of doing a scope lookup.
        // We will have to implement encoding block scope info to enable, which will also
        // enable defer parsing function that are in block scopes.

        if (funcInfo->byteCodeFunction &&
            funcInfo->byteCodeFunction->GetScopeInfo() != nullptr &&
            !funcInfo->byteCodeFunction->GetScopeInfo()->IsParentInfoOnly())
        {LOGMEIN("ScopeInfo.cpp] 189\n");
            // No need to regenerate scope info if we re-compile an enclosing function
            return;
        }

        Scope* currentScope = byteCodeGenerator->GetCurrentScope();
        Assert(currentScope == funcInfo->GetBodyScope());
        if (funcInfo->HasDeferredChild() ||
            funcInfo->HasRedeferrableChild() ||
            (!funcInfo->IsGlobalFunction() &&
                funcInfo->byteCodeFunction &&
                funcInfo->byteCodeFunction->IsReparsed() &&
                funcInfo->byteCodeFunction->GetFunctionBody()->HasAllNonLocalReferenced()))
        {LOGMEIN("ScopeInfo.cpp] 202\n");
            // When we reparse due to attach, we would need to capture all of them, since they were captured before going to debug mode.

            Js::ScopeInfo::SaveScopeInfo(byteCodeGenerator, parentFunc, funcInfo);
        }
        else if (funcInfo->IsDeferred() || funcInfo->IsRedeferrable())
        {LOGMEIN("ScopeInfo.cpp] 208\n");
            // Don't need to remember the parent function if we have a global function
            if (!parentFunc->IsGlobalFunction() ||
                ((byteCodeGenerator->GetFlags() & fscrEvalCode) && (parentFunc->HasDeferredChild() || parentFunc->HasRedeferrableChild())))
            {LOGMEIN("ScopeInfo.cpp] 212\n");
                // TODO: currently we only support defer nested function that is in function scope (no block scope, no with scope, etc.)
#if DBG
                if (funcInfo->GetFuncExprScope() && funcInfo->GetFuncExprScope()->GetIsObject())
                {LOGMEIN("ScopeInfo.cpp] 216\n");
                    if (funcInfo->paramScope && !funcInfo->paramScope->GetCanMergeWithBodyScope())
                    {LOGMEIN("ScopeInfo.cpp] 218\n");
                        Assert(currentScope->GetEnclosingScope()->GetEnclosingScope() == funcInfo->GetFuncExprScope());
                    }
                    else
                    {
                        Assert(currentScope->GetEnclosingScope() == funcInfo->GetFuncExprScope() &&
                            currentScope->GetEnclosingScope()->GetEnclosingScope() ==
                            (parentFunc->IsGlobalFunction() && parentFunc->GetGlobalEvalBlockScope()->GetMustInstantiate() ?
                             parentFunc->GetGlobalEvalBlockScope() : parentFunc->GetBodyScope()));
                    }
                }
                else
                {
                    if (currentScope->GetEnclosingScope() == parentFunc->GetParamScope())
                    {LOGMEIN("ScopeInfo.cpp] 232\n");
                        Assert(!parentFunc->GetParamScope()->GetCanMergeWithBodyScope());
                        Assert(funcInfo->GetParamScope()->GetCanMergeWithBodyScope());
                    }
                    else if (currentScope->GetEnclosingScope() == funcInfo->GetParamScope())
                    {LOGMEIN("ScopeInfo.cpp] 237\n");
                        Assert(!funcInfo->GetParamScope()->GetCanMergeWithBodyScope());
                    }
#if 0
                    else
                    {
                        Assert(currentScope->GetEnclosingScope() ==
                            (parentFunc->IsGlobalFunction() && parentFunc->GetGlobalEvalBlockScope() && parentFunc->GetGlobalEvalBlockScope()->GetMustInstantiate() ? parentFunc->GetGlobalEvalBlockScope() : parentFunc->GetBodyScope()));
                    }
#endif
                }
#endif
                Js::ScopeInfo::SaveParentScopeInfo(parentFunc, funcInfo);
            }
        }
    }

    //
    // Load persisted scope info.
    //
    void ScopeInfo::GetScopeInfo(Parser *parser, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, Scope* scope)
    {LOGMEIN("ScopeInfo.cpp] 258\n");
        ScriptContext* scriptContext;
        ArenaAllocator* alloc;

        // Load scope attributes and push onto scope stack.
        scope->SetIsDynamic(this->isDynamic);
        if (this->isObject)
        {LOGMEIN("ScopeInfo.cpp] 265\n");
            scope->SetIsObject();
        }
        scope->SetMustInstantiate(this->mustInstantiate);
        if (!this->GetCanMergeWithBodyScope())
        {LOGMEIN("ScopeInfo.cpp] 270\n");
            scope->SetCannotMergeWithBodyScope();
        }
        scope->SetHasOwnLocalInClosure(this->hasLocalInClosure);
        if (parser)
        {LOGMEIN("ScopeInfo.cpp] 275\n");
            scriptContext = parser->GetScriptContext();
            alloc = parser->GetAllocator();
        }
        else
        {
            TRACE_BYTECODE(_u("\nRestore ScopeInfo: %s #symbols: %d %s\n"),
                funcInfo->name, symbolCount, isObject ? _u("isObject") : _u(""));

            Assert(!this->isCached || scope == funcInfo->GetBodyScope());
            funcInfo->SetHasCachedScope(this->isCached);
            byteCodeGenerator->PushScope(scope);

            // this->scope was created/saved during parsing and used by
            // ByteCodeGenerator::RestoreScopeInfo. We no longer need it by now.
            // Clear it to avoid GC false positive (arena memory later used by GC).
            Assert(this->scope == scope);
            this->scope = nullptr;

            // The scope is already populated, so we're done.
            return;
        }

        // Load scope symbols
        // On first access to the scopeinfo, replace the ID's with PropertyRecord*'s to save the dictionary lookup
        // on later accesses. Replace them all before allocating Symbol's to prevent inconsistency on OOM.
        if (!this->areNamesCached && !PHASE_OFF1(Js::CacheScopeInfoNamesPhase))
        {LOGMEIN("ScopeInfo.cpp] 302\n");
            for (int i = 0; i < symbolCount; i++)
            {LOGMEIN("ScopeInfo.cpp] 304\n");
                PropertyId propertyId = GetSymbolId(i);
                if (propertyId != 0) // There may be empty slots, e.g. "arguments" may have no slot
                {LOGMEIN("ScopeInfo.cpp] 307\n");
                    PropertyRecord const* name = scriptContext->GetPropertyName(propertyId);
                    this->SetPropertyName(i, name);
                }
            }
            this->areNamesCached = true;
        }

        for (int i = 0; i < symbolCount; i++)
        {LOGMEIN("ScopeInfo.cpp] 316\n");
            PropertyRecord const* name = nullptr;
            if (this->areNamesCached)
            {LOGMEIN("ScopeInfo.cpp] 319\n");
                name = this->GetPropertyName(i);
            }
            else
            {
                PropertyId propertyId = GetSymbolId(i);
                if (propertyId != 0) // There may be empty slots, e.g. "arguments" may have no slot
                {LOGMEIN("ScopeInfo.cpp] 326\n");
                    name = scriptContext->GetPropertyName(propertyId);
                }
            }

            if (name != nullptr)
            {LOGMEIN("ScopeInfo.cpp] 332\n");
                SymbolType symbolType = GetSymbolType(i);
                SymbolName symName(name->GetBuffer(), name->GetLength());
                Symbol *sym = Anew(alloc, Symbol, symName, nullptr, symbolType);

                sym->SetScopeSlot(static_cast<PropertyId>(i));
                sym->SetIsBlockVar(GetIsBlockVariable(i));
                sym->SetIsFuncExpr(GetIsFuncExpr(i));
                sym->SetIsModuleExportStorage(GetIsModuleExportStorage(i));
                sym->SetIsModuleImport(GetIsModuleImport(i));
                if (GetHasFuncAssignment(i))
                {LOGMEIN("ScopeInfo.cpp] 343\n");
                    sym->RestoreHasFuncAssignment();
                }
                scope->AddNewSymbol(sym);
                sym->SetHasNonLocalReference();
                Assert(parser);
                parser->RestorePidRefForSym(sym);

                TRACE_BYTECODE(_u("%12s %d\n"), sym->GetName().GetBuffer(), sym->GetScopeSlot());
            }
        }
        this->scope = scope;
        DebugOnly(scope->isRestored = true);
    }

    ScopeInfo::AutoCapturesAllScope::AutoCapturesAllScope(Scope* scope, bool turnOn)
        : scope(scope)
    {LOGMEIN("ScopeInfo.cpp] 360\n");
        oldCapturesAll = scope->GetCapturesAll();
        if (turnOn)
        {LOGMEIN("ScopeInfo.cpp] 363\n");
            scope->SetCapturesAll(true);
        }
    }

    ScopeInfo::AutoCapturesAllScope::~AutoCapturesAllScope()
    {LOGMEIN("ScopeInfo.cpp] 369\n");
        scope->SetCapturesAll(oldCapturesAll);
    }
} // namespace Js
