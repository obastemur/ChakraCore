//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

bool Scope::IsGlobalEvalBlockScope() const
{LOGMEIN("Scope.cpp] 7\n");
    return this->scopeType == ScopeType_GlobalEvalBlock;
}

bool Scope::IsBlockScope(FuncInfo *funcInfo)
{LOGMEIN("Scope.cpp] 12\n");
    return this != funcInfo->GetBodyScope() && this != funcInfo->GetParamScope();
}

int Scope::AddScopeSlot()
{LOGMEIN("Scope.cpp] 17\n");
    int slot = scopeSlotCount++;
    if (scopeSlotCount == Js::ScopeSlots::MaxEncodedSlotCount)
    {LOGMEIN("Scope.cpp] 20\n");
        this->GetEnclosingFunc()->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("TooManySlots")));
    }
    return slot;
}

void Scope::ForceAllSymbolNonLocalReference(ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("Scope.cpp] 27\n");
    this->ForEachSymbol([this, byteCodeGenerator](Symbol *const sym)
    {
        if (!sym->GetIsArguments())
        {LOGMEIN("Scope.cpp] 31\n");
            sym->SetHasNonLocalReference();
            byteCodeGenerator->ProcessCapturedSym(sym);
            this->GetFunc()->SetHasLocalInClosure(true);
        }
    });
}

bool Scope::IsEmpty() const
{LOGMEIN("Scope.cpp] 40\n");
    if (GetFunc()->bodyScope == this || (GetFunc()->IsGlobalFunction() && this->IsGlobalEvalBlockScope()))
    {LOGMEIN("Scope.cpp] 42\n");
        return Count() == 0 && !GetFunc()->isThisLexicallyCaptured;
    }
    else
    {
        return Count() == 0;
    }
}

void Scope::SetIsObject()
{LOGMEIN("Scope.cpp] 52\n");
    if (this->isObject)
    {LOGMEIN("Scope.cpp] 54\n");
        return;
    }

    this->isObject = true;

    // We might set the scope to be object after we have process the symbol
    // (e.g. "With" scope referencing a symbol in an outer scope).
    // If we have func assignment, we need to mark the function to not do stack nested function
    // as these are now assigned to a scope object.
    FuncInfo * funcInfo = this->GetFunc();
    if (funcInfo && !funcInfo->HasMaybeEscapedNestedFunc())
    {LOGMEIN("Scope.cpp] 66\n");
        this->ForEachSymbolUntil([funcInfo](Symbol * const sym)
        {
            if (sym->GetHasFuncAssignment())
            {LOGMEIN("Scope.cpp] 70\n");
                funcInfo->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("DelayedObjectScopeAssignment")));
                return true;
            }
            return false;
        });
    }

    if (this->GetScopeType() == ScopeType_FunctionBody && funcInfo && funcInfo->paramScope
        && !funcInfo->paramScope->GetIsObject() && !funcInfo->paramScope->GetCanMergeWithBodyScope())
    {LOGMEIN("Scope.cpp] 80\n");
        // If this is split scope then mark the param scope also as an object
        funcInfo->paramScope->SetIsObject();
    }
}

void Scope::MergeParamAndBodyScopes(ParseNode *pnodeScope)
{LOGMEIN("Scope.cpp] 87\n");
    Assert(pnodeScope->sxFnc.funcInfo);
    Scope *paramScope = pnodeScope->sxFnc.pnodeScopes->sxBlock.scope;
    Scope *bodyScope = pnodeScope->sxFnc.pnodeBodyScope->sxBlock.scope;

    if (paramScope->Count() == 0)
    {LOGMEIN("Scope.cpp] 93\n");
        return;
    }

    bodyScope->scopeSlotCount = paramScope->scopeSlotCount;
    paramScope->ForEachSymbol([&](Symbol * sym)
    {
        bodyScope->AddNewSymbol(sym);
    });

    if (paramScope->GetIsObject())
    {LOGMEIN("Scope.cpp] 104\n");
        bodyScope->SetIsObject();
    }
    if (paramScope->GetMustInstantiate())
    {LOGMEIN("Scope.cpp] 108\n");
        bodyScope->SetMustInstantiate(true);
    }
    if (paramScope->GetHasOwnLocalInClosure())
    {LOGMEIN("Scope.cpp] 112\n");
        bodyScope->SetHasOwnLocalInClosure(true);
    }
}

void Scope::RemoveParamScope(ParseNode *pnodeScope)
{LOGMEIN("Scope.cpp] 118\n");
    Assert(pnodeScope->sxFnc.funcInfo);
    Scope *paramScope = pnodeScope->sxFnc.pnodeScopes->sxBlock.scope;
    Scope *bodyScope = pnodeScope->sxFnc.pnodeBodyScope->sxBlock.scope;

    // Once the scopes are merged, there's no reason to instantiate the param scope.
    paramScope->SetMustInstantiate(false);

    paramScope->m_count = 0;
    paramScope->scopeSlotCount = 0;
    paramScope->m_symList = nullptr;
    // Remove the parameter scope from the scope chain.

    bodyScope->SetEnclosingScope(paramScope->GetEnclosingScope());
}
