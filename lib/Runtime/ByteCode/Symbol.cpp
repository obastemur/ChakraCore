//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

#if DBG_DUMP
static const char16 * const SymbolTypeNames[] = { _u("Function"), _u("Variable"), _u("MemberName"), _u("Formal"), _u("Unknown") };
#endif

bool Symbol::GetIsArguments() const
{LOGMEIN("Symbol.cpp] 11\n");
    return decl != nullptr && (decl->grfpn & PNodeFlags::fpnArguments);
}

Js::PropertyId Symbol::EnsurePosition(ByteCodeGenerator* byteCodeGenerator)
{LOGMEIN("Symbol.cpp] 16\n");
    // Guarantee that a symbol's name has a property ID mapping.
    if (this->position == Js::Constants::NoProperty)
    {LOGMEIN("Symbol.cpp] 19\n");
        this->position = this->EnsurePositionNoCheck(byteCodeGenerator->TopFuncInfo());
    }
    return this->position;
}

Js::PropertyId Symbol::EnsurePosition(FuncInfo *funcInfo)
{LOGMEIN("Symbol.cpp] 26\n");
    // Guarantee that a symbol's name has a property ID mapping.
    if (this->position == Js::Constants::NoProperty)
    {LOGMEIN("Symbol.cpp] 29\n");
        this->position = this->EnsurePositionNoCheck(funcInfo);
    }
    return this->position;
}

Js::PropertyId Symbol::EnsurePositionNoCheck(FuncInfo *funcInfo)
{LOGMEIN("Symbol.cpp] 36\n");
    return funcInfo->byteCodeFunction->GetOrAddPropertyIdTracked(this->GetName());
}

void Symbol::SaveToPropIdArray(Symbol *sym, Js::PropertyIdArray *propIds, ByteCodeGenerator *byteCodeGenerator, Js::PropertyId *pFirstSlot /* = null */)
{LOGMEIN("Symbol.cpp] 41\n");
    if (sym)
    {LOGMEIN("Symbol.cpp] 43\n");
        Js::PropertyId slot = sym->scopeSlot;
        if (slot != Js::Constants::NoProperty)
        {LOGMEIN("Symbol.cpp] 46\n");
            Assert((uint32)slot < propIds->count);
            propIds->elements[slot] = sym->EnsurePosition(byteCodeGenerator);
            if (pFirstSlot && !sym->GetIsArguments())
            {LOGMEIN("Symbol.cpp] 50\n");
                if (*pFirstSlot == Js::Constants::NoProperty ||
                    *pFirstSlot > slot)
                {LOGMEIN("Symbol.cpp] 53\n");
                    *pFirstSlot = slot;
                }
            }
        }
    }
}

bool Symbol::NeedsSlotAlloc(FuncInfo *funcInfo)
{LOGMEIN("Symbol.cpp] 62\n");
    return IsInSlot(funcInfo, true);
}

bool Symbol::IsInSlot(FuncInfo *funcInfo, bool ensureSlotAlloc)
{LOGMEIN("Symbol.cpp] 67\n");
    if (this->GetIsGlobal() || this->GetIsModuleExportStorage())
    {LOGMEIN("Symbol.cpp] 69\n");
        return false;
    }
    if (funcInfo->GetHasHeapArguments() && this->GetIsFormal() && ByteCodeGenerator::NeedScopeObjectForArguments(funcInfo, funcInfo->root))
    {LOGMEIN("Symbol.cpp] 73\n");
        return true;
    }
    if (this->GetIsGlobalCatch())
    {LOGMEIN("Symbol.cpp] 77\n");
        return true;
    }
    if (this->scope->GetCapturesAll())
    {LOGMEIN("Symbol.cpp] 81\n");
        return true;
    }
    // If body and param scopes are not merged then an inner scope slot is used
    if (this->scope->GetScopeType() == ScopeType_Parameter && !this->scope->GetCanMergeWithBodyScope())
    {LOGMEIN("Symbol.cpp] 86\n");
        return true;
    }

    return this->GetHasNonLocalReference() && (ensureSlotAlloc || this->GetIsCommittedToSlot());
}

bool Symbol::GetIsCommittedToSlot() const
{LOGMEIN("Symbol.cpp] 94\n");
    if (!PHASE_ON1(Js::DelayCapturePhase))
    {LOGMEIN("Symbol.cpp] 96\n");
        return true;
    }
    return isCommittedToSlot || this->scope->GetFunc()->GetCallsEval() || this->scope->GetFunc()->GetChildCallsEval();
}

Js::PropertyId Symbol::EnsureScopeSlot(FuncInfo *funcInfo)
{LOGMEIN("Symbol.cpp] 103\n");
    if (this->NeedsSlotAlloc(funcInfo) && this->scopeSlot == Js::Constants::NoProperty)
    {LOGMEIN("Symbol.cpp] 105\n");
        this->scopeSlot = this->scope->AddScopeSlot();
    }
    return this->scopeSlot;
}

void Symbol::SetHasNonLocalReference()
{LOGMEIN("Symbol.cpp] 112\n");
    this->hasNonLocalReference = true;
    this->scope->SetHasOwnLocalInClosure(true);
}

void Symbol::SetHasMaybeEscapedUse(ByteCodeGenerator * byteCodeGenerator)
{LOGMEIN("Symbol.cpp] 118\n");
    Assert(!this->GetIsMember());
    if (!hasMaybeEscapedUse)
    {LOGMEIN("Symbol.cpp] 121\n");
        SetHasMaybeEscapedUseInternal(byteCodeGenerator);
    }
}

void Symbol::SetHasMaybeEscapedUseInternal(ByteCodeGenerator * byteCodeGenerator)
{LOGMEIN("Symbol.cpp] 127\n");
    Assert(!hasMaybeEscapedUse);
    Assert(!this->GetIsFormal());
    hasMaybeEscapedUse = true;
    if (PHASE_TESTTRACE(Js::StackFuncPhase, byteCodeGenerator->TopFuncInfo()->byteCodeFunction))
    {LOGMEIN("Symbol.cpp] 132\n");
        Output::Print(_u("HasMaybeEscapedUse: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }
    if (this->GetHasFuncAssignment())
    {LOGMEIN("Symbol.cpp] 137\n");
        this->GetScope()->GetFunc()->SetHasMaybeEscapedNestedFunc(
            DebugOnly(this->symbolType == STFunction ? _u("MaybeEscapedUseFuncDecl") : _u("MaybeEscapedUse")));
    }
}

void Symbol::SetHasFuncAssignment(ByteCodeGenerator * byteCodeGenerator)
{LOGMEIN("Symbol.cpp] 144\n");
    Assert(!this->GetIsMember());
    if (!hasFuncAssignment)
    {LOGMEIN("Symbol.cpp] 147\n");
        SetHasFuncAssignmentInternal(byteCodeGenerator);
    }
}

void Symbol::SetHasFuncAssignmentInternal(ByteCodeGenerator * byteCodeGenerator)
{LOGMEIN("Symbol.cpp] 153\n");
    Assert(!hasFuncAssignment);
    hasFuncAssignment = true;
    FuncInfo * top = byteCodeGenerator->TopFuncInfo();
    if (PHASE_TESTTRACE(Js::StackFuncPhase, top->byteCodeFunction))
    {LOGMEIN("Symbol.cpp] 158\n");
        Output::Print(_u("HasFuncAssignment: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }

    if (this->GetHasMaybeEscapedUse() || this->GetScope()->GetIsObject())
    {LOGMEIN("Symbol.cpp] 164\n");
        byteCodeGenerator->TopFuncInfo()->SetHasMaybeEscapedNestedFunc(DebugOnly(
            this->GetIsFormal() ? _u("FormalAssignment") :
            this->GetScope()->GetIsObject() ? _u("ObjectScopeAssignment") :
            _u("MaybeEscapedUse")));
    }
}

void Symbol::RestoreHasFuncAssignment()
{LOGMEIN("Symbol.cpp] 173\n");
    Assert(hasFuncAssignment == (this->symbolType == STFunction));
    Assert(this->GetIsFormal() || !this->GetHasMaybeEscapedUse());
    hasFuncAssignment = true;
    if (PHASE_TESTTRACE1(Js::StackFuncPhase))
    {LOGMEIN("Symbol.cpp] 178\n");
        Output::Print(_u("RestoreHasFuncAssignment: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }
}

Symbol * Symbol::GetFuncScopeVarSym() const
{LOGMEIN("Symbol.cpp] 185\n");
    if (!this->GetIsBlockVar())
    {LOGMEIN("Symbol.cpp] 187\n");
        return nullptr;
    }
    FuncInfo * parentFuncInfo = this->GetScope()->GetFunc();
    if (parentFuncInfo->GetIsStrictMode())
    {LOGMEIN("Symbol.cpp] 192\n");
        return nullptr;
    }
    Symbol *fncScopeSym = parentFuncInfo->GetBodyScope()->FindLocalSymbol(this->GetName());
    if (fncScopeSym == nullptr && parentFuncInfo->GetParamScope() != nullptr)
    {LOGMEIN("Symbol.cpp] 197\n");
        // We couldn't find the sym in the body scope, try finding it in the parameter scope.
        Scope* paramScope = parentFuncInfo->GetParamScope();
        fncScopeSym = paramScope->FindLocalSymbol(this->GetName());
    }
    Assert(fncScopeSym);
    // Parser should have added a fake var decl node for block scoped functions in non-strict mode
    // IsBlockVar() indicates a user let declared variable at function scope which
    // shadows the function's var binding, thus only emit the var binding init if
    // we do not have a block var symbol.
    if (!fncScopeSym || fncScopeSym->GetIsBlockVar())
    {LOGMEIN("Symbol.cpp] 208\n");
        return nullptr;
    }
    return fncScopeSym;
}

#if DBG_DUMP
const char16 * Symbol::GetSymbolTypeName()
{LOGMEIN("Symbol.cpp] 216\n");
    return SymbolTypeNames[symbolType];
}
#endif
