//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

bool Scope::IsGlobalEvalBlockScope() const
{TRACE_IT(41811);
    return this->scopeType == ScopeType_GlobalEvalBlock;
}

bool Scope::IsBlockScope(FuncInfo *funcInfo)
{TRACE_IT(41812);
    return this != funcInfo->GetBodyScope() && this != funcInfo->GetParamScope();
}

int Scope::AddScopeSlot()
{TRACE_IT(41813);
    int slot = scopeSlotCount++;
    if (scopeSlotCount == Js::ScopeSlots::MaxEncodedSlotCount)
    {TRACE_IT(41814);
        this->GetEnclosingFunc()->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("TooManySlots")));
    }
    return slot;
}

void Scope::ForceAllSymbolNonLocalReference(ByteCodeGenerator *byteCodeGenerator)
{TRACE_IT(41815);
    this->ForEachSymbol([this, byteCodeGenerator](Symbol *const sym)
    {
        if (!sym->GetIsArguments())
        {TRACE_IT(41816);
            sym->SetHasNonLocalReference();
            byteCodeGenerator->ProcessCapturedSym(sym);
            this->GetFunc()->SetHasLocalInClosure(true);
        }
    });
}

bool Scope::IsEmpty() const
{TRACE_IT(41817);
    if (GetFunc()->bodyScope == this || (GetFunc()->IsGlobalFunction() && this->IsGlobalEvalBlockScope()))
    {TRACE_IT(41818);
        return Count() == 0 && !GetFunc()->isThisLexicallyCaptured;
    }
    else
    {TRACE_IT(41819);
        return Count() == 0;
    }
}

void Scope::SetIsObject()
{TRACE_IT(41820);
    if (this->isObject)
    {TRACE_IT(41821);
        return;
    }

    this->isObject = true;

    // We might set the scope to be object after we have process the symbol
    // (e.g. "With" scope referencing a symbol in an outer scope).
    // If we have func assignment, we need to mark the function to not do stack nested function
    // as these are now assigned to a scope object.
    FuncInfo * funcInfo = this->GetFunc();
    if (funcInfo && !funcInfo->HasMaybeEscapedNestedFunc())
    {TRACE_IT(41822);
        this->ForEachSymbolUntil([funcInfo](Symbol * const sym)
        {
            if (sym->GetHasFuncAssignment())
            {TRACE_IT(41823);
                funcInfo->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("DelayedObjectScopeAssignment")));
                return true;
            }
            return false;
        });
    }

    if (this->GetScopeType() == ScopeType_FunctionBody && funcInfo && !funcInfo->IsBodyAndParamScopeMerged()
         && funcInfo->paramScope && !funcInfo->paramScope->GetIsObject())
    {TRACE_IT(41824);
        // If this is split scope then mark the param scope also as an object
        funcInfo->paramScope->SetIsObject();
    }
}

void Scope::MergeParamAndBodyScopes(ParseNode *pnodeScope)
{TRACE_IT(41825);
    Assert(pnodeScope->sxFnc.funcInfo);
    Scope *paramScope = pnodeScope->sxFnc.pnodeScopes->sxBlock.scope;
    Scope *bodyScope = pnodeScope->sxFnc.pnodeBodyScope->sxBlock.scope;

    if (paramScope->Count() == 0)
    {TRACE_IT(41826);
        return;
    }

    bodyScope->scopeSlotCount = paramScope->scopeSlotCount;
    paramScope->ForEachSymbol([&](Symbol * sym)
    {
        bodyScope->AddNewSymbol(sym);
    });

    if (paramScope->GetIsObject())
    {TRACE_IT(41827);
        bodyScope->SetIsObject();
    }
    if (paramScope->GetMustInstantiate())
    {TRACE_IT(41828);
        bodyScope->SetMustInstantiate(true);
    }
    if (paramScope->GetHasOwnLocalInClosure())
    {TRACE_IT(41829);
        bodyScope->SetHasOwnLocalInClosure(true);
    }
}

void Scope::RemoveParamScope(ParseNode *pnodeScope)
{TRACE_IT(41830);
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
