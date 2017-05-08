//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"

FuncInfo::FuncInfo(
    const char16 *name,
    ArenaAllocator *alloc,
    Scope *paramScope,
    Scope *bodyScope,
    ParseNode *pnode,
    Js::ParseableFunctionInfo* byteCodeFunction)
    : alloc(alloc),
    varRegsCount(0),
    constRegsCount(InitialConstRegsCount),
    inArgsCount(0),
    innerScopeCount(0),
    currentInnerScopeIndex((uint)-1),
    firstTmpReg(Js::Constants::NoRegister),
    curTmpReg(Js::Constants::NoRegister),
    outArgsMaxDepth(0),
    outArgsCurrentExpr(0),
#if DBG
    outArgsDepth(0),
#endif
    name(name),
    nullConstantRegister(Js::Constants::NoRegister),
    undefinedConstantRegister(Js::Constants::NoRegister),
    trueConstantRegister(Js::Constants::NoRegister),
    falseConstantRegister(Js::Constants::NoRegister),
    thisPointerRegister(Js::Constants::NoRegister),
    superRegister(Js::Constants::NoRegister),
    superCtorRegister(Js::Constants::NoRegister),
    newTargetRegister(Js::Constants::NoRegister),
    envRegister(Js::Constants::NoRegister),
    frameObjRegister(Js::Constants::NoRegister),
    frameSlotsRegister(Js::Constants::NoRegister),
    paramSlotsRegister(Js::Constants::NoRegister),
    frameDisplayRegister(Js::Constants::NoRegister),
    funcObjRegister(Js::Constants::NoRegister),
    localClosureReg(Js::Constants::NoRegister),
    yieldRegister(Js::Constants::NoRegister),
    paramScope(paramScope),
    bodyScope(bodyScope),
    funcExprScope(nullptr),
    root(pnode),
    capturedSyms(nullptr),
    capturedSymMap(nullptr),
    currentChildFunction(nullptr),
    currentChildScope(nullptr),
    callsEval(false),
    childCallsEval(false),
    hasArguments(false),
    hasHeapArguments(false),
    isTopLevelEventHandler(false),
    hasLocalInClosure(false),
    hasClosureReference(false),
    hasGlobalReference(false),
    hasCachedScope(false),
    funcExprNameReference(false),
    applyEnclosesArgs(false),
    escapes(false),
    hasDeferredChild(false),
    hasRedeferrableChild(false),
    childHasWith(false),
    hasLoop(false),
    hasEscapedUseNestedFunc(false),
    needEnvRegister(false),
    hasCapturedThis(false),
    isBodyAndParamScopeMerged(true),
#if DBG
    isReused(false),
#endif
    staticFuncId(-1),
    inlineCacheMap(nullptr),
    slotProfileIdMap(alloc),
    argsPlaceHolderSlotCount(0),
    thisScopeSlot(Js::Constants::NoProperty),
    superScopeSlot(Js::Constants::NoProperty),
    superCtorScopeSlot(Js::Constants::NoProperty),
    newTargetScopeSlot(Js::Constants::NoProperty),
    isThisLexicallyCaptured(false),
    isSuperLexicallyCaptured(false),
    isSuperCtorLexicallyCaptured(false),
    isNewTargetLexicallyCaptured(false),
    inlineCacheCount(0),
    rootObjectLoadInlineCacheCount(0),
    rootObjectLoadMethodInlineCacheCount(0),
    rootObjectStoreInlineCacheCount(0),
    isInstInlineCacheCount(0),
    referencedPropertyIdCount(0),
    argumentsSymbol(nullptr),
    nonUserNonTempRegistersToInitialize(alloc),
    constantToRegister(alloc, 17),
    stringToRegister(alloc, 17),
    doubleConstantToRegister(alloc, 17),
    stringTemplateCallsiteRegisterMap(alloc, 17),
    targetStatements(alloc),
    nextForInLoopLevel(0),
    maxForInLoopLevel(0)
{TRACE_IT(41498);
    this->byteCodeFunction = byteCodeFunction;
    bodyScope->SetFunc(this);
    if (paramScope != nullptr)
    {TRACE_IT(41499);
        paramScope->SetFunc(this);
    }
    if (pnode && pnode->sxFnc.NestedFuncEscapes())
    {TRACE_IT(41500);
        this->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("Child")));
    }
}

bool FuncInfo::IsGlobalFunction() const
{TRACE_IT(41501);
    return root && root->nop == knopProg;
}

bool FuncInfo::IsDeferred() const
{TRACE_IT(41502);
    return root && root->sxFnc.pnodeBody == nullptr;
}

bool FuncInfo::IsRedeferrable() const
{TRACE_IT(41503);
    return byteCodeFunction && byteCodeFunction->CanBeDeferred();
}

BOOL FuncInfo::HasSuperReference() const
{TRACE_IT(41504);
    return root->sxFnc.HasSuperReference();
}

BOOL FuncInfo::HasDirectSuper() const
{TRACE_IT(41505);
    return root->sxFnc.HasDirectSuper();
}

BOOL FuncInfo::IsClassMember() const
{TRACE_IT(41506);
    return root->sxFnc.IsClassMember();
}

BOOL FuncInfo::IsLambda() const
{TRACE_IT(41507);
    return root->sxFnc.IsLambda();
}

BOOL FuncInfo::IsClassConstructor() const
{TRACE_IT(41508);
    return root->sxFnc.IsClassConstructor();
}

BOOL FuncInfo::IsBaseClassConstructor() const
{TRACE_IT(41509);
    return root->sxFnc.IsBaseClassConstructor();
}

void FuncInfo::EnsureThisScopeSlot()
{TRACE_IT(41510);
    if (this->thisScopeSlot == Js::Constants::NoProperty)
    {TRACE_IT(41511);
        // In case of split scope param and body has separate closures. So we have to use different scope slots for them.
        Scope* scope = this->IsBodyAndParamScopeMerged() ? this->bodyScope : this->paramScope;
        Scope* currentScope = scope->IsGlobalEvalBlockScope() ? this->GetGlobalEvalBlockScope() : scope;

        this->thisScopeSlot = currentScope->AddScopeSlot();
    }
}

void FuncInfo::EnsureSuperScopeSlot()
{TRACE_IT(41512);
    if (this->superScopeSlot == Js::Constants::NoProperty)
    {TRACE_IT(41513);
        // In case of split scope param and body has separate closures. So we have to use different scope slots for them.
        Scope* scope = this->IsBodyAndParamScopeMerged() ? this->bodyScope : this->paramScope;

        this->superScopeSlot = scope->AddScopeSlot();
    }
}

void FuncInfo::EnsureSuperCtorScopeSlot()
{TRACE_IT(41514);
    if (this->superCtorScopeSlot == Js::Constants::NoProperty)
    {TRACE_IT(41515);
        // In case of split scope param and body has separate closures. So we have to use different scope slots for them.
        Scope* scope = this->IsBodyAndParamScopeMerged() ? this->bodyScope : this->paramScope;

        this->superCtorScopeSlot = scope->AddScopeSlot();
    }
}

void FuncInfo::EnsureNewTargetScopeSlot()
{TRACE_IT(41516);
    if (this->newTargetScopeSlot == Js::Constants::NoProperty)
    {TRACE_IT(41517);
        // In case of split scope param and body has separate closures. So we have to use different scope slots for them.
        Scope* scope = this->IsBodyAndParamScopeMerged() ? this->bodyScope : this->paramScope;

        this->newTargetScopeSlot = scope->AddScopeSlot();
    }
}

Scope *
FuncInfo::GetGlobalBlockScope() const
{TRACE_IT(41518);
    Assert(this->IsGlobalFunction());
    Scope * scope = this->root->sxFnc.pnodeScopes->sxBlock.scope;
    Assert(scope == nullptr || scope == this->GetBodyScope() || scope->GetEnclosingScope() == this->GetBodyScope());
    return scope;
}

Scope * FuncInfo::GetGlobalEvalBlockScope() const
{TRACE_IT(41519);
    Scope * globalEvalBlockScope = this->GetGlobalBlockScope();
    Assert(globalEvalBlockScope->GetEnclosingScope() == this->GetBodyScope());
    Assert(globalEvalBlockScope->GetScopeType() == ScopeType_GlobalEvalBlock);
    return globalEvalBlockScope;
}

uint FuncInfo::FindOrAddReferencedPropertyId(Js::PropertyId propertyId)
{TRACE_IT(41520);
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(referencedPropertyIdToMapIndex != nullptr);
    if (propertyId < TotalNumberOfBuiltInProperties)
    {TRACE_IT(41521);
        return propertyId;
    }
    uint index;
    if (!referencedPropertyIdToMapIndex->TryGetValue(propertyId, &index))
    {TRACE_IT(41522);
        index = this->NewReferencedPropertyId();
        referencedPropertyIdToMapIndex->Add(propertyId, index);
    }
    return index + TotalNumberOfBuiltInProperties;
}

uint FuncInfo::FindOrAddRootObjectInlineCacheId(Js::PropertyId propertyId, bool isLoadMethod, bool isStore)
{TRACE_IT(41523);
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(!isLoadMethod || !isStore);
    uint cacheId;
    RootObjectInlineCacheIdMap * idMap = isStore ? rootObjectStoreInlineCacheMap : isLoadMethod ? rootObjectLoadMethodInlineCacheMap : rootObjectLoadInlineCacheMap;
    if (!idMap->TryGetValue(propertyId, &cacheId))
    {TRACE_IT(41524);
        cacheId = isStore ? this->NewRootObjectStoreInlineCache() : isLoadMethod ? this->NewRootObjectLoadMethodInlineCache() : this->NewRootObjectLoadInlineCache();
        idMap->Add(propertyId, cacheId);
    }
    return cacheId;
}

#if DBG_DUMP
void FuncInfo::Dump()
{TRACE_IT(41525);
    Output::Print(_u("FuncInfo: CallsEval:%s ChildCallsEval:%s HasArguments:%s HasHeapArguments:%s\n"),
        IsTrueOrFalse(this->GetCallsEval()),
        IsTrueOrFalse(this->GetChildCallsEval()),
        IsTrueOrFalse(this->GetHasArguments()),
        IsTrueOrFalse(this->GetHasHeapArguments()));
}
#endif

Js::RegSlot FuncInfo::AcquireLoc(ParseNode *pnode)
{TRACE_IT(41526);
    // Assign a new temp pseudo-register to this expression.
    if (pnode->location == Js::Constants::NoRegister)
    {TRACE_IT(41527);
        pnode->location = this->AcquireTmpRegister();
    }
    return pnode->location;
}

Js::RegSlot FuncInfo::AcquireTmpRegister()
{TRACE_IT(41528);
    Assert(this->firstTmpReg != Js::Constants::NoRegister);
    // Allocate a new temp pseudo-register, increasing the locals count if necessary.
    Assert(this->curTmpReg <= this->varRegsCount && this->curTmpReg >= this->firstTmpReg);
    Js::RegSlot tmpReg = this->curTmpReg;
    UInt32Math::Inc(this->curTmpReg);
    if (this->curTmpReg > this->varRegsCount)
    {TRACE_IT(41529);
        this->varRegsCount = this->curTmpReg;
    }
    return tmpReg;
}

void FuncInfo::ReleaseLoc(ParseNode *pnode)
{TRACE_IT(41530);
    // Release the temp assigned to this expression so it can be re-used.
    if (pnode && pnode->location != Js::Constants::NoRegister)
    {TRACE_IT(41531);
        this->ReleaseTmpRegister(pnode->location);
    }
}

void FuncInfo::ReleaseLoad(ParseNode *pnode)
{TRACE_IT(41532);
    // Release any temp register(s) acquired by an EmitLoad.
    switch (pnode->nop)
    {
    case knopDot:
    case knopIndex:
    case knopCall:
        this->ReleaseReference(pnode);
        break;
    }
    this->ReleaseLoc(pnode);
}

void FuncInfo::ReleaseReference(ParseNode *pnode)
{TRACE_IT(41533);
    // Release any temp(s) assigned to this reference expression so they can be re-used.
    switch (pnode->nop)
    {
    case knopDot:
        this->ReleaseLoc(pnode->sxBin.pnode1);
        break;

    case knopIndex:
        this->ReleaseLoc(pnode->sxBin.pnode2);
        this->ReleaseLoc(pnode->sxBin.pnode1);
        break;

    case knopName:
        // Do nothing (see EmitReference)
        break;

    case knopCall:
    case knopNew:
        // For call/new, we have to release the ArgOut register(s) in reverse order,
        // but we have the args in a singly linked list.
        // Fortunately, we know that the set we have to release is sequential.
        // So find the endpoints of the list and release them in descending order.
        if (pnode->sxCall.pnodeArgs)
        {TRACE_IT(41534);
            ParseNode *pnodeArg = pnode->sxCall.pnodeArgs;
            Js::RegSlot firstArg = Js::Constants::NoRegister;
            Js::RegSlot lastArg = Js::Constants::NoRegister;
            if (pnodeArg->nop == knopList)
            {TRACE_IT(41535);
                do
                {TRACE_IT(41536);
                    if (this->IsTmpReg(pnodeArg->sxBin.pnode1->location))
                    {TRACE_IT(41537);
                        lastArg = pnodeArg->sxBin.pnode1->location;
                        if (firstArg == Js::Constants::NoRegister)
                        {TRACE_IT(41538);
                            firstArg = lastArg;
                        }
                    }
                    pnodeArg = pnodeArg->sxBin.pnode2;
                }
                while (pnodeArg->nop == knopList);
            }
            if (this->IsTmpReg(pnodeArg->location))
            {TRACE_IT(41539);
                lastArg = pnodeArg->location;
                if (firstArg == Js::Constants::NoRegister)
                {TRACE_IT(41540);
                    // Just one: first and last point to the same node.
                    firstArg = lastArg;
                }
            }
            if (lastArg != Js::Constants::NoRegister)
            {TRACE_IT(41541);
                Assert(firstArg != Js::Constants::NoRegister);
                Assert(lastArg >= firstArg);
                do
                {TRACE_IT(41542);
                    // Walk down from last to first.
                    this->ReleaseTmpRegister(lastArg);
                } while (lastArg-- > firstArg); // these are unsigned, so (--lastArg >= firstArg) will cause an infinite loop if firstArg is 0 (although that shouldn't happen)
            }
        }
        // Now release the call target.
        switch (pnode->sxCall.pnodeTarget->nop)
        {
        case knopDot:
        case knopIndex:
            this->ReleaseReference(pnode->sxCall.pnodeTarget);
            this->ReleaseLoc(pnode->sxCall.pnodeTarget);
            break;
        default:
            this->ReleaseLoad(pnode->sxCall.pnodeTarget);
            break;
        }
        break;
    default:
        this->ReleaseLoc(pnode);
        break;
    }
}

void FuncInfo::ReleaseTmpRegister(Js::RegSlot tmpReg)
{TRACE_IT(41543);
    // Put this reg back on top of the temp stack (if it's a temp).
    Assert(tmpReg != Js::Constants::NoRegister);
    if (this->IsTmpReg(tmpReg))
    {TRACE_IT(41544);
        Assert(tmpReg == this->curTmpReg - 1);
        this->curTmpReg--;
    }
}

Js::RegSlot FuncInfo::InnerScopeToRegSlot(Scope *scope) const
{TRACE_IT(41545);
    Js::RegSlot reg = FirstInnerScopeReg();
    Assert(reg != Js::Constants::NoRegister);

    uint32 index = scope->GetInnerScopeIndex();

    return reg + index;
}

Js::RegSlot FuncInfo::FirstInnerScopeReg() const
{TRACE_IT(41546);
    // FunctionBody stores this as a mapped reg. Callers of this function want the pre-mapped value.

    Js::RegSlot reg = this->GetParsedFunctionBody()->GetFirstInnerScopeRegister();
    Assert(reg != Js::Constants::NoRegister);

    return reg - this->constRegsCount;
}

void FuncInfo::SetFirstInnerScopeReg(Js::RegSlot reg)
{TRACE_IT(41547);
    // Just forward to the FunctionBody.
    this->GetParsedFunctionBody()->MapAndSetFirstInnerScopeRegister(reg);
}

void FuncInfo::AddCapturedSym(Symbol *sym)
{TRACE_IT(41548);
    if (this->capturedSyms == nullptr)
    {TRACE_IT(41549);
        this->capturedSyms = Anew(alloc, SymbolTable, alloc);
    }
    this->capturedSyms->AddNew(sym);
}

void FuncInfo::OnStartVisitFunction(ParseNode *pnodeFnc)
{TRACE_IT(41550);
    Assert(pnodeFnc->nop == knopFncDecl);
    Assert(this->GetCurrentChildFunction() == nullptr);

    this->SetCurrentChildFunction(pnodeFnc->sxFnc.funcInfo);
}

void FuncInfo::OnEndVisitFunction(ParseNode *pnodeFnc)
{TRACE_IT(41551);
    Assert(pnodeFnc->nop == knopFncDecl);
    Assert(this->GetCurrentChildFunction() == pnodeFnc->sxFnc.funcInfo);

    pnodeFnc->sxFnc.funcInfo->SetCurrentChildScope(nullptr);
    this->SetCurrentChildFunction(nullptr);
}

void FuncInfo::OnStartVisitScope(Scope *scope, bool *pisMergedScope)
{TRACE_IT(41552);
    *pisMergedScope = false;

    if (scope == nullptr)
    {TRACE_IT(41553);
        return;
    }

    Scope* childScope = this->GetCurrentChildScope();
    if (childScope)
    {TRACE_IT(41554);
        if (scope->GetScopeType() == ScopeType_Parameter)
        {TRACE_IT(41555);
            Assert(childScope->GetEnclosingScope() == scope);
        }
        else if (childScope->GetScopeType() == ScopeType_Parameter
                 && childScope->GetFunc()->IsBodyAndParamScopeMerged()
                 && scope->GetScopeType() == ScopeType_Block)
        {TRACE_IT(41556);
            // If param and body are merged then the class declaration in param scope will have body as the parent
            *pisMergedScope = true;
            Assert(childScope == scope->GetEnclosingScope()->GetEnclosingScope());
        }
        else
        {TRACE_IT(41557);
            Assert(childScope == scope->GetEnclosingScope());
        }
    }

    this->SetCurrentChildScope(scope);
    return;
}

void FuncInfo::OnEndVisitScope(Scope *scope, bool isMergedScope)
{TRACE_IT(41558);
    if (scope == nullptr)
    {TRACE_IT(41559);
        return;
    }
    Assert(this->GetCurrentChildScope() == scope || (scope->GetScopeType() == ScopeType_Parameter && this->GetParamScope() == scope));

    this->SetCurrentChildScope(isMergedScope ? scope->GetEnclosingScope()->GetEnclosingScope() : scope->GetEnclosingScope());
}

CapturedSymMap *FuncInfo::EnsureCapturedSymMap()
{TRACE_IT(41560);
    if (this->capturedSymMap == nullptr)
    {TRACE_IT(41561);
        this->capturedSymMap = Anew(alloc, CapturedSymMap, alloc);
    }
    return this->capturedSymMap;
}

void FuncInfo::SetHasMaybeEscapedNestedFunc(DebugOnly(char16 const * reason))
{TRACE_IT(41562);
    if (PHASE_TESTTRACE(Js::StackFuncPhase, this->byteCodeFunction) && !hasEscapedUseNestedFunc)
    {TRACE_IT(41563);
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        char16 const * r = _u("");

        DebugOnly(r = reason);
        Output::Print(_u("HasMaybeEscapedNestedFunc (%s): %s (function %s)\n"),
            r,
            this->byteCodeFunction->GetDisplayName(),
            this->byteCodeFunction->GetDebugNumberSet(debugStringBuffer));
        Output::Flush();
    }
    hasEscapedUseNestedFunc = true;
}

uint FuncInfo::AcquireInnerScopeIndex()
{TRACE_IT(41564);
    uint index = this->currentInnerScopeIndex;
    if (index == (uint)-1)
    {TRACE_IT(41565);
        index = 0;
    }
    else
    {TRACE_IT(41566);
        index++;
        if (index == (uint)-1)
        {TRACE_IT(41567);
            Js::Throw::OutOfMemory();
        }
    }
    if (index == this->innerScopeCount)
    {TRACE_IT(41568);
        this->innerScopeCount = index + 1;
    }
    this->currentInnerScopeIndex = index;
    return index;
}

void FuncInfo::ReleaseInnerScopeIndex()
{TRACE_IT(41569);
    uint index = this->currentInnerScopeIndex;
    Assert(index != (uint)-1);

    if (index == 0)
    {TRACE_IT(41570);
        index = (uint)-1;
    }
    else
    {TRACE_IT(41571);
        index--;
    }
    this->currentInnerScopeIndex = index;
}
