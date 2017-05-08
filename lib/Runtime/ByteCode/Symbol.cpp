//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

#if DBG_DUMP
static const char16 * const SymbolTypeNames[] = { _u("Function"), _u("Variable"), _u("MemberName"), _u("Formal"), _u("Unknown") };
#endif

bool Symbol::GetIsArguments() const
{TRACE_IT(41987);
    return decl != nullptr && (decl->grfpn & PNodeFlags::fpnArguments);
}

Js::PropertyId Symbol::EnsurePosition(ByteCodeGenerator* byteCodeGenerator)
{TRACE_IT(41988);
    // Guarantee that a symbol's name has a property ID mapping.
    if (this->position == Js::Constants::NoProperty)
    {TRACE_IT(41989);
        this->position = this->EnsurePositionNoCheck(byteCodeGenerator->TopFuncInfo());
    }
    return this->position;
}

Js::PropertyId Symbol::EnsurePosition(FuncInfo *funcInfo)
{TRACE_IT(41990);
    // Guarantee that a symbol's name has a property ID mapping.
    if (this->position == Js::Constants::NoProperty)
    {TRACE_IT(41991);
        this->position = this->EnsurePositionNoCheck(funcInfo);
    }
    return this->position;
}

Js::PropertyId Symbol::EnsurePositionNoCheck(FuncInfo *funcInfo)
{TRACE_IT(41992);
    return funcInfo->byteCodeFunction->GetOrAddPropertyIdTracked(this->GetName());
}

void Symbol::SaveToPropIdArray(Symbol *sym, Js::PropertyIdArray *propIds, ByteCodeGenerator *byteCodeGenerator, Js::PropertyId *pFirstSlot /* = null */)
{TRACE_IT(41993);
    if (sym)
    {TRACE_IT(41994);
        Js::PropertyId slot = sym->scopeSlot;
        if (slot != Js::Constants::NoProperty)
        {TRACE_IT(41995);
            Assert((uint32)slot < propIds->count);
            propIds->elements[slot] = sym->EnsurePosition(byteCodeGenerator);
            if (pFirstSlot && !sym->GetIsArguments())
            {TRACE_IT(41996);
                if (*pFirstSlot == Js::Constants::NoProperty ||
                    *pFirstSlot > slot)
                {TRACE_IT(41997);
                    *pFirstSlot = slot;
                }
            }
        }
    }
}

bool Symbol::NeedsSlotAlloc(FuncInfo *funcInfo)
{TRACE_IT(41998);
    return IsInSlot(funcInfo, true);
}

bool Symbol::IsInSlot(FuncInfo *funcInfo, bool ensureSlotAlloc)
{TRACE_IT(41999);
    if (this->GetIsGlobal() || this->GetIsModuleExportStorage())
    {TRACE_IT(42000);
        return false;
    }
    if (funcInfo->GetHasHeapArguments() && this->GetIsFormal() && ByteCodeGenerator::NeedScopeObjectForArguments(funcInfo, funcInfo->root))
    {TRACE_IT(42001);
        return true;
    }
    if (this->GetIsGlobalCatch())
    {TRACE_IT(42002);
        return true;
    }
    if (this->scope->GetCapturesAll())
    {TRACE_IT(42003);
        return true;
    }

    return this->GetHasNonLocalReference() && (ensureSlotAlloc || this->GetIsCommittedToSlot());
}

bool Symbol::GetIsCommittedToSlot() const
{TRACE_IT(42004);
    if (!PHASE_ON1(Js::DelayCapturePhase))
    {TRACE_IT(42005);
        return true;
    }
    return isCommittedToSlot || this->scope->GetFunc()->GetCallsEval() || this->scope->GetFunc()->GetChildCallsEval();
}

Js::PropertyId Symbol::EnsureScopeSlot(FuncInfo *funcInfo)
{TRACE_IT(42006);
    if (this->NeedsSlotAlloc(funcInfo) && this->scopeSlot == Js::Constants::NoProperty)
    {TRACE_IT(42007);
        this->scopeSlot = this->scope->AddScopeSlot();
    }
    return this->scopeSlot;
}

void Symbol::SetHasNonLocalReference()
{TRACE_IT(42008);
    this->hasNonLocalReference = true;
    this->scope->SetHasOwnLocalInClosure(true);
}

void Symbol::SetHasMaybeEscapedUse(ByteCodeGenerator * byteCodeGenerator)
{TRACE_IT(42009);
    Assert(!this->GetIsMember());
    if (!hasMaybeEscapedUse)
    {TRACE_IT(42010);
        SetHasMaybeEscapedUseInternal(byteCodeGenerator);
    }
}

void Symbol::SetHasMaybeEscapedUseInternal(ByteCodeGenerator * byteCodeGenerator)
{TRACE_IT(42011);
    Assert(!hasMaybeEscapedUse);
    Assert(!this->GetIsFormal());
    hasMaybeEscapedUse = true;
    if (PHASE_TESTTRACE(Js::StackFuncPhase, byteCodeGenerator->TopFuncInfo()->byteCodeFunction))
    {TRACE_IT(42012);
        Output::Print(_u("HasMaybeEscapedUse: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }
    if (this->GetHasFuncAssignment())
    {TRACE_IT(42013);
        this->GetScope()->GetFunc()->SetHasMaybeEscapedNestedFunc(
            DebugOnly(this->symbolType == STFunction ? _u("MaybeEscapedUseFuncDecl") : _u("MaybeEscapedUse")));
    }
}

void Symbol::SetHasFuncAssignment(ByteCodeGenerator * byteCodeGenerator)
{TRACE_IT(42014);
    Assert(!this->GetIsMember());
    if (!hasFuncAssignment)
    {TRACE_IT(42015);
        SetHasFuncAssignmentInternal(byteCodeGenerator);
    }
}

void Symbol::SetHasFuncAssignmentInternal(ByteCodeGenerator * byteCodeGenerator)
{TRACE_IT(42016);
    Assert(!hasFuncAssignment);
    hasFuncAssignment = true;
    FuncInfo * top = byteCodeGenerator->TopFuncInfo();
    if (PHASE_TESTTRACE(Js::StackFuncPhase, top->byteCodeFunction))
    {TRACE_IT(42017);
        Output::Print(_u("HasFuncAssignment: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }

    if (this->GetHasMaybeEscapedUse() || this->GetScope()->GetIsObject())
    {TRACE_IT(42018);
        byteCodeGenerator->TopFuncInfo()->SetHasMaybeEscapedNestedFunc(DebugOnly(
            this->GetIsFormal() ? _u("FormalAssignment") :
            this->GetScope()->GetIsObject() ? _u("ObjectScopeAssignment") :
            _u("MaybeEscapedUse")));
    }
}

void Symbol::RestoreHasFuncAssignment()
{TRACE_IT(42019);
    Assert(hasFuncAssignment == (this->symbolType == STFunction));
    Assert(this->GetIsFormal() || !this->GetHasMaybeEscapedUse());
    hasFuncAssignment = true;
    if (PHASE_TESTTRACE1(Js::StackFuncPhase))
    {TRACE_IT(42020);
        Output::Print(_u("RestoreHasFuncAssignment: %s\n"), this->GetName().GetBuffer());
        Output::Flush();
    }
}

Symbol * Symbol::GetFuncScopeVarSym() const
{TRACE_IT(42021);
    if (!this->GetIsBlockVar())
    {TRACE_IT(42022);
        return nullptr;
    }
    FuncInfo * parentFuncInfo = this->GetScope()->GetFunc();
    if (parentFuncInfo->GetIsStrictMode())
    {TRACE_IT(42023);
        return nullptr;
    }
    Symbol *fncScopeSym = parentFuncInfo->GetBodyScope()->FindLocalSymbol(this->GetName());
    if (fncScopeSym == nullptr && parentFuncInfo->GetParamScope() != nullptr)
    {TRACE_IT(42024);
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
    {TRACE_IT(42025);
        return nullptr;
    }
    return fncScopeSym;
}

#if DBG_DUMP
const char16 * Symbol::GetSymbolTypeName()
{TRACE_IT(42026);
    return SymbolTypeNames[symbolType];
}
#endif
