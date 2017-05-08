//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"
#include "FormalsUtil.h"
#include "Language/AsmJs.h"

void EmitReference(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitAssignment(ParseNode *asgnNode, ParseNode *lhs, Js::RegSlot rhsLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitLoad(ParseNode *rhs, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitCall(ParseNode* pnode, Js::RegSlot rhsLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, BOOL fReturnValue, BOOL fEvaluateComponents, BOOL fHasNewTarget, Js::RegSlot overrideThisLocation = Js::Constants::NoRegister);
void EmitSuperFieldPatch(FuncInfo* funcInfo, ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator);

void EmitUseBeforeDeclaration(Symbol *sym, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitUseBeforeDeclarationRuntimeError(ByteCodeGenerator *byteCodeGenerator, Js::RegSlot location);
void VisitClearTmpRegs(ParseNode * pnode, ByteCodeGenerator * byteCodeGenerator, FuncInfo * funcInfo);

bool CallTargetIsArray(ParseNode *pnode)
{TRACE_IT(38547);
    return pnode->nop == knopName && pnode->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::Array;
}

#define STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode) \
if ((isTopLevel)) \
{TRACE_IT(38548); \
    byteCodeGenerator->StartStatement(pnode); \
}

#define ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode) \
if ((isTopLevel)) \
{TRACE_IT(38549); \
    byteCodeGenerator->EndStatement(pnode); \
}

BOOL MayHaveSideEffectOnNode(ParseNode *pnode, ParseNode *pnodeSE)
{TRACE_IT(38550);
    // Try to determine whether pnodeSE may kill the named var represented by pnode.

    if (pnode->nop == knopComputedName)
    {TRACE_IT(38551);
        pnode = pnode->sxUni.pnode1;
    }

    if (pnode->nop != knopName)
    {TRACE_IT(38552);
        // Only investigating named vars here.
        return false;
    }

    uint fnop = ParseNode::Grfnop(pnodeSE->nop);
    if (fnop & fnopLeaf)
    {TRACE_IT(38553);
        // pnodeSE is a leaf and can't kill anything.
        return false;
    }

    if (fnop & fnopAsg)
    {TRACE_IT(38554);
        // pnodeSE is an assignment (=, ++, +=, etc.)
        // Trying to examine the LHS of pnodeSE caused small perf regressions,
        // maybe because of code layout or some other subtle effect.
        return true;
    }

    if (fnop & fnopUni)
    {TRACE_IT(38555);
        // pnodeSE is a unary op, so recurse to the source (if present - e.g., [] may have no opnd).
        if (pnodeSE->nop == knopTempRef)
        {TRACE_IT(38556);
            return false;
        }
        else
        {TRACE_IT(38557);
            return pnodeSE->sxUni.pnode1 && MayHaveSideEffectOnNode(pnode, pnodeSE->sxUni.pnode1);
        }
    }
    else if (fnop & fnopBin)
    {TRACE_IT(38558);
        // pnodeSE is a binary (or ternary) op, so recurse to the sources (if present).
        if (pnodeSE->nop == knopQmark)
        {TRACE_IT(38559);
            return MayHaveSideEffectOnNode(pnode, pnodeSE->sxTri.pnode1) ||
                MayHaveSideEffectOnNode(pnode, pnodeSE->sxTri.pnode2) ||
                MayHaveSideEffectOnNode(pnode, pnodeSE->sxTri.pnode3);
        }
        else if (pnodeSE->nop == knopCall || pnodeSE->nop == knopNew)
        {TRACE_IT(38560);
            return MayHaveSideEffectOnNode(pnode, pnodeSE->sxCall.pnodeTarget) ||
                (pnodeSE->sxCall.pnodeArgs && MayHaveSideEffectOnNode(pnode, pnodeSE->sxCall.pnodeArgs));
        }
        else
        {TRACE_IT(38561);
            return MayHaveSideEffectOnNode(pnode, pnodeSE->sxBin.pnode1) ||
                (pnodeSE->sxBin.pnode2 && MayHaveSideEffectOnNode(pnode, pnodeSE->sxBin.pnode2));
        }
    }
    else if (pnodeSE->nop == knopList)
    {TRACE_IT(38562);
        return true;
    }

    return false;
}

bool IsCallOfConstants(ParseNode *pnode);
bool BlockHasOwnScope(ParseNode* pnodeBlock, ByteCodeGenerator *byteCodeGenerator);
bool CreateNativeArrays(ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);

bool IsArguments(ParseNode *pnode)
{TRACE_IT(38563);
    for (;;)
    {TRACE_IT(38564);
        switch (pnode->nop)
        {
        case knopName:
            return pnode->sxPid.sym && pnode->sxPid.sym->GetIsArguments();

        case knopCall:
        case knopNew:
            if (IsArguments(pnode->sxCall.pnodeTarget))
            {TRACE_IT(38565);
                return true;
            }

            if (pnode->sxCall.pnodeArgs)
            {TRACE_IT(38566);
                ParseNode *pnodeArg = pnode->sxCall.pnodeArgs;
                while (pnodeArg->nop == knopList)
                {TRACE_IT(38567);
                    if (IsArguments(pnodeArg->sxBin.pnode1))
                        return true;

                    pnodeArg = pnodeArg->sxBin.pnode2;
                }

                pnode = pnodeArg;
                break;
            }

            return false;

        case knopArray:
            if (pnode->sxArrLit.arrayOfNumbers || pnode->sxArrLit.count == 0)
            {TRACE_IT(38568);
                return false;
            }

            pnode = pnode->sxUni.pnode1;
            break;

        case knopQmark:
            if (IsArguments(pnode->sxTri.pnode1) || IsArguments(pnode->sxTri.pnode2))
            {TRACE_IT(38569);
                return true;
            }

            pnode = pnode->sxTri.pnode3;
            break;

            //
            // Cases where we don't check for "arguments" yet.
            // Assume that they might have it. Disable the optimization is such scenarios
            //
        case knopList:
        case knopObject:
        case knopVarDecl:
        case knopConstDecl:
        case knopLetDecl:
        case knopFncDecl:
        case knopClassDecl:
        case knopFor:
        case knopIf:
        case knopDoWhile:
        case knopWhile:
        case knopForIn:
        case knopForOf:
        case knopReturn:
        case knopBlock:
        case knopBreak:
        case knopContinue:
        case knopLabel:
        case knopTypeof:
        case knopThrow:
        case knopWith:
        case knopFinally:
        case knopTry:
        case knopTryCatch:
        case knopTryFinally:
        case knopArrayPattern:
        case knopObjectPattern:
        case knopParamPattern:
            return true;

        default:
        {TRACE_IT(38570);
            uint flags = ParseNode::Grfnop(pnode->nop);
            if (flags&fnopUni)
            {TRACE_IT(38571);
                Assert(pnode->sxUni.pnode1);

                pnode = pnode->sxUni.pnode1;
                break;
            }
            else if (flags&fnopBin)
            {TRACE_IT(38572);
                Assert(pnode->sxBin.pnode1 && pnode->sxBin.pnode2);

                if (IsArguments(pnode->sxBin.pnode1))
                {TRACE_IT(38573);
                    return true;
                }

                pnode = pnode->sxBin.pnode2;
                break;
            }

            return false;
        }

        }
    }
}

bool ApplyEnclosesArgs(ParseNode* fncDecl, ByteCodeGenerator* byteCodeGenerator);
void Emit(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fReturnValue, bool isConstructorCall = false, ParseNode *bindPnode = nullptr, bool isTopLevel = false);
void EmitComputedFunctionNameVar(ParseNode *nameNode, ParseNode *exprNode, ByteCodeGenerator *byteCodeGenerator);
void EmitBinaryOpnds(ParseNode *pnode1, ParseNode *pnode2, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
bool IsExpressionStatement(ParseNode* stmt, const Js::ScriptContext *const scriptContext);
void EmitInvoke(Js::RegSlot location, Js::RegSlot callObjLocation, Js::PropertyId propertyId, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitInvoke(Js::RegSlot location, Js::RegSlot callObjLocation, Js::PropertyId propertyId, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, Js::RegSlot arg1Location);

static const Js::OpCode nopToOp[knopLim] =
{
#define OP(x) Br##x##_A
#define PTNODE(nop,sn,pc,nk,grfnop,json) Js::OpCode::pc,
#include "ptlist.h"
};
static const Js::OpCode nopToCMOp[knopLim] =
{
#define OP(x) Cm##x##_A
#define PTNODE(nop,sn,pc,nk,grfnop,json) Js::OpCode::pc,
#include "ptlist.h"
};

Js::OpCode ByteCodeGenerator::ToChkUndeclOp(Js::OpCode op) const
{TRACE_IT(38574);
    switch (op)
    {
    case Js::OpCode::StLocalSlot:
        return Js::OpCode::StLocalSlotChkUndecl;

    case Js::OpCode::StParamSlot:
        return Js::OpCode::StParamSlotChkUndecl;

    case Js::OpCode::StInnerSlot:
        return Js::OpCode::StInnerSlotChkUndecl;

    case Js::OpCode::StEnvSlot:
        return Js::OpCode::StEnvSlotChkUndecl;

    case Js::OpCode::StObjSlot:
        return Js::OpCode::StObjSlotChkUndecl;

    case Js::OpCode::StLocalObjSlot:
        return Js::OpCode::StLocalObjSlotChkUndecl;

    case Js::OpCode::StParamObjSlot:
        return Js::OpCode::StParamObjSlotChkUndecl;

    case Js::OpCode::StInnerObjSlot:
        return Js::OpCode::StInnerObjSlotChkUndecl;

    case Js::OpCode::StEnvObjSlot:
        return Js::OpCode::StEnvObjSlotChkUndecl;

    default:
        AssertMsg(false, "Unknown opcode for chk undecl mapping");
        return Js::OpCode::InvalidOpCode;
    }
}

// Tracks a register slot let/const property for the passed in debugger block/catch scope.
// debuggerScope         - The scope to add the variable to.
// symbol                - The symbol that represents the register property.
// funcInfo              - The function info used to store the property into the tracked debugger register slot list.
// flags                 - The flags to assign to the property.
// isFunctionDeclaration - Whether or not the register is a function declaration, which requires that its byte code offset be updated immediately.
void ByteCodeGenerator::TrackRegisterPropertyForDebugger(
    Js::DebuggerScope *debuggerScope,
    Symbol *symbol,
    FuncInfo *funcInfo,
    Js::DebuggerScopePropertyFlags flags /*= Js::DebuggerScopePropertyFlags_None*/,
    bool isFunctionDeclaration /*= false*/)
{TRACE_IT(38575);
    Assert(debuggerScope);
    Assert(symbol);
    Assert(funcInfo);

    Js::RegSlot location = symbol->GetLocation();

    Js::DebuggerScope *correctDebuggerScope = debuggerScope;
    if (debuggerScope->scopeType != Js::DiagExtraScopesType::DiagBlockScopeDirect && debuggerScope->scopeType != Js::DiagExtraScopesType::DiagCatchScopeDirect)
    {TRACE_IT(38576);
        // We have to get the appropriate scope and add property over there.
        // Make sure the scope is created whether we're in debug mode or not, because we
        // need the empty scopes present during reparsing for debug mode.
        correctDebuggerScope = debuggerScope->GetSiblingScope(location, Writer()->GetFunctionWrite());
    }

    if (this->ShouldTrackDebuggerMetadata() && !symbol->GetIsTrackedForDebugger())
    {TRACE_IT(38577);
        // Only track the property if we're in debug mode since it's only needed by the debugger.
        Js::PropertyId propertyId = symbol->EnsurePosition(this);

        this->Writer()->AddPropertyToDebuggerScope(
            correctDebuggerScope,
            location,
            propertyId,
            /*shouldConsumeRegister*/ true,
            flags,
            isFunctionDeclaration);

        Js::FunctionBody *byteCodeFunction = funcInfo->GetParsedFunctionBody();
        byteCodeFunction->InsertSymbolToRegSlotList(location, propertyId, funcInfo->varRegsCount);

        symbol->SetIsTrackedForDebugger(true);
    }
}

void ByteCodeGenerator::TrackActivationObjectPropertyForDebugger(
    Js::DebuggerScope *debuggerScope,
    Symbol *symbol,
    Js::DebuggerScopePropertyFlags flags /*= Js::DebuggerScopePropertyFlags_None*/,
    bool isFunctionDeclaration /*= false*/)
{TRACE_IT(38578);
    Assert(debuggerScope);
    Assert(symbol);

    // Only need to track activation object properties in debug mode.
    if (ShouldTrackDebuggerMetadata() && !symbol->GetIsTrackedForDebugger())
    {TRACE_IT(38579);
        Js::RegSlot location = symbol->GetLocation();
        Js::PropertyId propertyId = symbol->EnsurePosition(this);

        this->Writer()->AddPropertyToDebuggerScope(
            debuggerScope,
            location,
            propertyId,
            /*shouldConsumeRegister*/ false,
            flags,
            isFunctionDeclaration);

        symbol->SetIsTrackedForDebugger(true);
    }
}

void ByteCodeGenerator::TrackSlotArrayPropertyForDebugger(
    Js::DebuggerScope *debuggerScope,
    Symbol* symbol,
    Js::PropertyId propertyId,
    Js::DebuggerScopePropertyFlags flags /*= Js::DebuggerScopePropertyFlags_None*/,
    bool isFunctionDeclaration /*= false*/)
{TRACE_IT(38580);
    // Note: Slot array properties are tracked even in non-debug mode in order to support slot array serialization
    // of let/const variables between non-debug and debug mode (for example, when a slot array var escapes and is retrieved
    // after a debugger attach or for WWA apps).  They are also needed for heap enumeration.
    Assert(debuggerScope);
    Assert(symbol);

    if (!symbol->GetIsTrackedForDebugger())
    {TRACE_IT(38581);
        Js::RegSlot location = symbol->GetScopeSlot();
        Assert(location != Js::Constants::NoRegister);
        Assert(propertyId != Js::Constants::NoProperty);

        this->Writer()->AddPropertyToDebuggerScope(
            debuggerScope,
            location,
            propertyId,
            /*shouldConsumeRegister*/ false,
            flags,
            isFunctionDeclaration);

        symbol->SetIsTrackedForDebugger(true);
    }
}

// Tracks a function declaration inside a block scope for the debugger metadata's current scope (let binding).
void ByteCodeGenerator::TrackFunctionDeclarationPropertyForDebugger(Symbol *functionDeclarationSymbol, FuncInfo *funcInfoParent)
{TRACE_IT(38582);
    Assert(functionDeclarationSymbol);
    Assert(funcInfoParent);
    AssertMsg(functionDeclarationSymbol->GetIsBlockVar(), "We should only track inner function let bindings for the debugger.");

    // Note: we don't have to check symbol->GetIsTrackedForDebugger, as we are not doing actual work here,
    //       which is done in other Track* functions that we call.

    if (functionDeclarationSymbol->IsInSlot(funcInfoParent))
    {TRACE_IT(38583);
        if (functionDeclarationSymbol->GetScope()->GetIsObject())
        {TRACE_IT(38584);
            this->TrackActivationObjectPropertyForDebugger(
                this->Writer()->GetCurrentDebuggerScope(),
                functionDeclarationSymbol,
                Js::DebuggerScopePropertyFlags_None,
                true /*isFunctionDeclaration*/);
        }
        else
        {TRACE_IT(38585);
            // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
            // Note that slot array inner function bindings are tracked even in non-debug mode in order
            // to keep the lifetime of the closure binding that could escape around for heap enumeration.
            functionDeclarationSymbol->EnsureScopeSlot(funcInfoParent);
            functionDeclarationSymbol->EnsurePosition(this);
            this->TrackSlotArrayPropertyForDebugger(
                this->Writer()->GetCurrentDebuggerScope(),
                functionDeclarationSymbol,
                functionDeclarationSymbol->GetPosition(),
                Js::DebuggerScopePropertyFlags_None,
                true /*isFunctionDeclaration*/);
        }
    }
    else
    {TRACE_IT(38586);
        this->TrackRegisterPropertyForDebugger(
            this->Writer()->GetCurrentDebuggerScope(),
            functionDeclarationSymbol,
            funcInfoParent,
            Js::DebuggerScopePropertyFlags_None,
            true /*isFunctionDeclaration*/);
    }
}

// Updates the byte code offset of the property with the passed in location and ID.
// Used to track let/const variables that are in the dead zone debugger side.
// location                 - The activation object, scope slot index, or register location for the property.
// propertyId               - The ID of the property to update.
// shouldConsumeRegister    - Whether or not the a register should be consumed (used for reg slot locations).
void ByteCodeGenerator::UpdateDebuggerPropertyInitializationOffset(Js::RegSlot location, Js::PropertyId propertyId, bool shouldConsumeRegister)
{TRACE_IT(38587);
    Assert(this->Writer());
    Js::DebuggerScope* currentDebuggerScope = this->Writer()->GetCurrentDebuggerScope();
    Assert(currentDebuggerScope);
    if (currentDebuggerScope != nullptr)
    {TRACE_IT(38588);
        this->Writer()->UpdateDebuggerPropertyInitializationOffset(
            currentDebuggerScope,
            location,
            propertyId,
            shouldConsumeRegister);
    }
}

void ByteCodeGenerator::LoadHeapArguments(FuncInfo *funcInfo)
{TRACE_IT(38589);
    if (funcInfo->GetHasCachedScope())
    {TRACE_IT(38590);
        this->LoadCachedHeapArguments(funcInfo);
    }
    else
    {TRACE_IT(38591);
        this->LoadUncachedHeapArguments(funcInfo);
    }
}

void GetFormalArgsArray(ByteCodeGenerator *byteCodeGenerator, FuncInfo * funcInfo, Js::PropertyIdArray *propIds)
{TRACE_IT(38592);
    Assert(funcInfo);
    Assert(propIds);
    Assert(byteCodeGenerator);

    bool hadDuplicates = false;
    Js::ArgSlot i = 0;

    auto processArg = [&](ParseNode *pnode)
    {TRACE_IT(38593);
        if (pnode->IsVarLetOrConst())
        {TRACE_IT(38594);
            Assert(i < propIds->count);
            Symbol *sym = pnode->sxVar.sym;
            Assert(sym);
            Js::PropertyId symPos = sym->EnsurePosition(byteCodeGenerator);

            //
            // Check if the function has any same name parameters
            // For the same name param, only the last one will be passed the correct propertyid
            // For remaining dup param names, pass Constants::NoProperty
            //
            for (Js::ArgSlot j = 0; j < i; j++)
            {TRACE_IT(38595);
                if (propIds->elements[j] == symPos)
                {TRACE_IT(38596);
                    // Found a dup parameter name
                    propIds->elements[j] = Js::Constants::NoProperty;
                    hadDuplicates = true;
                    break;
                }
            }
            propIds->elements[i] = symPos;
        }
        else
        {TRACE_IT(38597);
            propIds->elements[i] = Js::Constants::NoProperty;
        }
        ++i;
    };
    MapFormals(funcInfo->root, processArg);

    propIds->hadDuplicates = hadDuplicates;
}

void ByteCodeGenerator::LoadUncachedHeapArguments(FuncInfo *funcInfo)
{TRACE_IT(38598);
    Assert(funcInfo->GetHasHeapArguments());

    Scope *scope = funcInfo->GetBodyScope();
    Assert(scope);
    Symbol *argSym = funcInfo->GetArgumentsSymbol();
    Assert(argSym && argSym->GetIsArguments());
    Js::RegSlot argumentsLoc = argSym->GetLocation();


    Js::OpCode opcode = !funcInfo->root->sxFnc.HasNonSimpleParameterList() ? Js::OpCode::LdHeapArguments : Js::OpCode::LdLetHeapArguments;
    bool hasRest = funcInfo->root->sxFnc.pnodeRest != nullptr;
    uint count = funcInfo->inArgsCount + (hasRest ? 1 : 0) - 1;
    if (count == 0)
    {TRACE_IT(38599);
        // If no formals to function (only "this"), then no need to create the scope object.
        // Leave both the arguments location and the propertyIds location as null.
        Assert(funcInfo->root->sxFnc.pnodeParams == nullptr && !hasRest);
    }
    else if (!NeedScopeObjectForArguments(funcInfo, funcInfo->root))
    {TRACE_IT(38600);
        // We may not need a scope object for arguments, e.g. strict mode with no eval.
    }
    else if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
    {TRACE_IT(38601);
        // Pass the frame object and ID array to the runtime, and put the resulting Arguments object
        // at the expected location.

        Js::PropertyIdArray *propIds = funcInfo->GetParsedFunctionBody()->AllocatePropertyIdArrayForFormals(count * sizeof(Js::PropertyId), count, 0);
        GetFormalArgsArray(this, funcInfo, propIds);
    }

    this->m_writer.Reg1(opcode, argumentsLoc);
    EmitLocalPropInit(argSym->GetLocation(), argSym, funcInfo);
}

void ByteCodeGenerator::LoadCachedHeapArguments(FuncInfo *funcInfo)
{TRACE_IT(38602);
    Assert(funcInfo->GetHasHeapArguments());

    Scope *scope = funcInfo->GetBodyScope();
    Assert(scope);
    Symbol *argSym = funcInfo->GetArgumentsSymbol();
    Assert(argSym && argSym->GetIsArguments());
    Js::RegSlot argumentsLoc = argSym->GetLocation();

    Js::OpCode op = !funcInfo->root->sxFnc.HasNonSimpleParameterList() ? Js::OpCode::LdHeapArgsCached : Js::OpCode::LdLetHeapArgsCached;

    this->m_writer.Reg1(op, argumentsLoc);
    EmitLocalPropInit(argumentsLoc, argSym, funcInfo);
}

Js::JavascriptArray* ByteCodeGenerator::BuildArrayFromStringList(ParseNode* stringNodeList, uint arrayLength, Js::ScriptContext* scriptContext)
{TRACE_IT(38603);
    Assert(stringNodeList);

    uint index = 0;
    Js::Var str;
    IdentPtr pid;
    Js::JavascriptArray* pArr = scriptContext->GetLibrary()->CreateArray(arrayLength);

    while (stringNodeList->nop == knopList)
    {TRACE_IT(38604);
        Assert(stringNodeList->sxBin.pnode1->nop == knopStr);

        pid = stringNodeList->sxBin.pnode1->sxPid.pid;
        str = Js::JavascriptString::NewCopyBuffer(pid->Psz(), pid->Cch(), scriptContext);
        pArr->SetItemWithAttributes(index, str, PropertyEnumerable);

        stringNodeList = stringNodeList->sxBin.pnode2;
        index++;
    }

    Assert(stringNodeList->nop == knopStr);

    pid = stringNodeList->sxPid.pid;
    str = Js::JavascriptString::NewCopyBuffer(pid->Psz(), pid->Cch(), scriptContext);
    pArr->SetItemWithAttributes(index, str, PropertyEnumerable);

    return pArr;
}

// For now, this just assigns field ids for the current script.
// Later, we will combine this information with the global field id map.
// This temporary code will not work if a global member is accessed both with and without a LHS.
void ByteCodeGenerator::AssignPropertyIds(Js::ParseableFunctionInfo* functionInfo)
{TRACE_IT(38605);
    globalScope->ForEachSymbol([this, functionInfo](Symbol * sym)
    {
        this->AssignPropertyId(sym, functionInfo);
    });
}

void ByteCodeGenerator::InitBlockScopedContent(ParseNode *pnodeBlock, Js::DebuggerScope* debuggerScope, FuncInfo *funcInfo)
{TRACE_IT(38606);
    Assert(pnodeBlock->nop == knopBlock);

    auto genBlockInit = [this, debuggerScope, funcInfo](ParseNode *pnode)
    {
        // Only check if the scope is valid when let/const vars are in the scope.  If there are no let/const vars,
        // the debugger scope will not be created.
        AssertMsg(debuggerScope, "Missing a case of scope tracking in BeginEmitBlock.");

        FuncInfo *funcInfo = this->TopFuncInfo();
        Symbol *sym = pnode->sxVar.sym;
        Scope *scope = sym->GetScope();

        if (sym->GetIsGlobal())
        {TRACE_IT(38607);
            Js::PropertyId propertyId = sym->EnsurePosition(this);
            if (this->flags & fscrEval)
            {TRACE_IT(38608);
                AssertMsg(this->IsConsoleScopeEval(), "Let/Consts cannot be in global scope outside of console eval");
                Js::OpCode op = (sym->GetDecl()->nop == knopConstDecl) ? Js::OpCode::InitUndeclConsoleConstFld : Js::OpCode::InitUndeclConsoleLetFld;
                this->m_writer.ElementScopedU(op, funcInfo->FindOrAddReferencedPropertyId(propertyId));
            }
            else
            {TRACE_IT(38609);
                Js::OpCode op = (sym->GetDecl()->nop == knopConstDecl) ?
                    Js::OpCode::InitUndeclRootConstFld : Js::OpCode::InitUndeclRootLetFld;
                this->m_writer.ElementRootU(op, funcInfo->FindOrAddReferencedPropertyId(propertyId));
            }
        }
        else if (sym->IsInSlot(funcInfo) || (scope->GetIsObject() && sym->NeedsSlotAlloc(funcInfo)))
        {TRACE_IT(38610);
            if (scope->GetIsObject())
            {TRACE_IT(38611);
                Js::RegSlot scopeLocation = scope->GetLocation();
                Js::PropertyId propertyId = sym->EnsurePosition(this);

                if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
                {TRACE_IT(38612);
                    uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true);

                    Js::OpCode op = (sym->GetDecl()->nop == knopConstDecl) ?
                        Js::OpCode::InitUndeclLocalConstFld : Js::OpCode::InitUndeclLocalLetFld;

                    this->m_writer.ElementP(op, ByteCodeGenerator::ReturnRegister, cacheId);
                }
                else
                {TRACE_IT(38613);
                    uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->InnerScopeToRegSlot(scope), propertyId, false, true);

                    Js::OpCode op = (sym->GetDecl()->nop == knopConstDecl) ?
                        Js::OpCode::InitUndeclConstFld : Js::OpCode::InitUndeclLetFld;

                    this->m_writer.ElementPIndexed(op, ByteCodeGenerator::ReturnRegister, scope->GetInnerScopeIndex(), cacheId);
                }

                TrackActivationObjectPropertyForDebugger(debuggerScope, sym, pnode->nop == knopConstDecl ? Js::DebuggerScopePropertyFlags_Const : Js::DebuggerScopePropertyFlags_None);
            }
            else
            {TRACE_IT(38614);
                Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg1(Js::OpCode::InitUndecl, tmpReg);
                this->EmitLocalPropInit(tmpReg, sym, funcInfo);
                funcInfo->ReleaseTmpRegister(tmpReg);

                // Slot array properties are tracked in non-debug mode as well because they need to stay
                // around for heap enumeration and escaping during attach/detach.
                TrackSlotArrayPropertyForDebugger(debuggerScope, sym, sym->EnsurePosition(this), pnode->nop == knopConstDecl ? Js::DebuggerScopePropertyFlags_Const : Js::DebuggerScopePropertyFlags_None);
            }
        }
        else if (!sym->GetIsModuleExportStorage())
        {TRACE_IT(38615);
            if (sym->GetDecl()->sxVar.isSwitchStmtDecl)
            {TRACE_IT(38616);
                // let/const declared in a switch is the only case of a variable that must be checked for
                // use-before-declaration dynamically within its own function.
                this->m_writer.Reg1(Js::OpCode::InitUndecl, sym->GetLocation());
            }
            // Syms that begin in register may be delay-captured. In debugger mode, such syms
            // will live only in slots, so tell the debugger to find them there.
            if (sym->NeedsSlotAlloc(funcInfo))
            {
                TrackSlotArrayPropertyForDebugger(debuggerScope, sym, sym->EnsurePosition(this), pnode->nop == knopConstDecl ? Js::DebuggerScopePropertyFlags_Const : Js::DebuggerScopePropertyFlags_None);
            }
            else
            {
                TrackRegisterPropertyForDebugger(debuggerScope, sym, funcInfo, pnode->nop == knopConstDecl ? Js::DebuggerScopePropertyFlags_Const : Js::DebuggerScopePropertyFlags_None);
            }
        }
    };

    IterateBlockScopedVariables(pnodeBlock, genBlockInit);
}

// Records the start of a debugger scope if the passed in node has any let/const variables (or is not a block node).
// If it has no let/const variables, nullptr will be returned as no scope will be created.
Js::DebuggerScope* ByteCodeGenerator::RecordStartScopeObject(ParseNode *pnodeBlock, Js::DiagExtraScopesType scopeType, Js::RegSlot scopeLocation /*= Js::Constants::NoRegister*/, int* index /*= nullptr*/)
{TRACE_IT(38617);
    Assert(pnodeBlock);
    if (pnodeBlock->nop == knopBlock && !pnodeBlock->sxBlock.HasBlockScopedContent())
    {TRACE_IT(38618);
        // In order to reduce allocations now that we track debugger scopes in non-debug mode,
        // don't add a block to the chain if it has no let/const variables at all.
        return nullptr;
    }

    return this->Writer()->RecordStartScopeObject(scopeType, scopeLocation, index);
}

// Records the end of the current scope, but only if the current block has block scoped content.
// Otherwise, a scope would not have been added (see ByteCodeGenerator::RecordStartScopeObject()).
void ByteCodeGenerator::RecordEndScopeObject(ParseNode *pnodeBlock)
{TRACE_IT(38619);
    Assert(pnodeBlock);
    if (pnodeBlock->nop == knopBlock && !pnodeBlock->sxBlock.HasBlockScopedContent())
    {TRACE_IT(38620);
        return;
    }

    this->Writer()->RecordEndScopeObject();
}

void BeginEmitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(38621);
    Js::DebuggerScope* debuggerScope = nullptr;

    if (BlockHasOwnScope(pnodeBlock, byteCodeGenerator))
    {TRACE_IT(38622);
        Scope *scope = pnodeBlock->sxBlock.scope;
        byteCodeGenerator->PushScope(scope);

        Js::RegSlot scopeLocation = scope->GetLocation();
        if (scope->GetMustInstantiate())
        {TRACE_IT(38623);
            Assert(scopeLocation == Js::Constants::NoRegister);
            scopeLocation = funcInfo->FirstInnerScopeReg() + scope->GetInnerScopeIndex();

            if (scope->GetIsObject())
            {TRACE_IT(38624);
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeInObject, scopeLocation);

                byteCodeGenerator->Writer()->Unsigned1(Js::OpCode::NewBlockScope, scope->GetInnerScopeIndex());
            }
            else
            {TRACE_IT(38625);
                int scopeIndex = Js::DebuggerScope::InvalidScopeIndex;
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeInSlot, scopeLocation, &scopeIndex);

                // TODO: Handle heap enumeration
                int scopeSlotCount = scope->GetScopeSlotCount();
                byteCodeGenerator->Writer()->Num3(Js::OpCode::NewInnerScopeSlots, scope->GetInnerScopeIndex(), scopeSlotCount + Js::ScopeSlots::FirstSlotIndex, scopeIndex);
            }
        }
        else
        {TRACE_IT(38626);
            // In the direct register access case, there is no block scope emitted but we can still track
            // the start and end offset of the block.  The location registers for let/const variables will still be
            // captured along with this range in InitBlockScopedContent().
            debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeDirect);
        }

        bool const isGlobalEvalBlockScope = scope->IsGlobalEvalBlockScope();
        Js::RegSlot frameDisplayLoc = Js::Constants::NoRegister;
        Js::RegSlot tmpInnerEnvReg = Js::Constants::NoRegister;
        ParseNodePtr pnodeScope;
        for (pnodeScope = pnodeBlock->sxBlock.pnodeScopes; pnodeScope;)
        {TRACE_IT(38627);
            switch (pnodeScope->nop)
            {
            case knopFncDecl:
                if (pnodeScope->sxFnc.IsDeclaration())
                {TRACE_IT(38628);
                    // The frameDisplayLoc register's lifetime has to be controlled by this function. We can't let
                    // it be released by DefineOneFunction, because further iterations of this loop can allocate
                    // temps, and we can't let frameDisplayLoc be re-purposed until this loop completes.
                    // So we'll supply a temp that we allocate and release here.
                    if (frameDisplayLoc == Js::Constants::NoRegister)
                    {TRACE_IT(38629);
                        if (funcInfo->frameDisplayRegister != Js::Constants::NoRegister)
                        {TRACE_IT(38630);
                            frameDisplayLoc = funcInfo->frameDisplayRegister;
                        }
                        else
                        {TRACE_IT(38631);
                            frameDisplayLoc = funcInfo->GetEnvRegister();
                        }
                        tmpInnerEnvReg = funcInfo->AcquireTmpRegister();
                        frameDisplayLoc = byteCodeGenerator->PrependLocalScopes(frameDisplayLoc, tmpInnerEnvReg, funcInfo);
                    }
                    byteCodeGenerator->DefineOneFunction(pnodeScope, funcInfo, true, frameDisplayLoc);
                }

                // If this is the global eval block scope, the function is actually assigned to the global
                // so we don't need to keep the registers.
                if (isGlobalEvalBlockScope)
                {TRACE_IT(38632);
                    funcInfo->ReleaseLoc(pnodeScope);
                    pnodeScope->location = Js::Constants::NoRegister;
                }
                pnodeScope = pnodeScope->sxFnc.pnodeNext;
                break;

            case knopBlock:
                pnodeScope = pnodeScope->sxBlock.pnodeNext;
                break;

            case knopCatch:
                pnodeScope = pnodeScope->sxCatch.pnodeNext;
                break;

            case knopWith:
                pnodeScope = pnodeScope->sxWith.pnodeNext;
                break;
            }
        }

        if (tmpInnerEnvReg != Js::Constants::NoRegister)
        {TRACE_IT(38633);
            funcInfo->ReleaseTmpRegister(tmpInnerEnvReg);
        }

        if (pnodeBlock->sxBlock.scope->IsGlobalEvalBlockScope() && funcInfo->thisScopeSlot != Js::Constants::NoRegister)
        {TRACE_IT(38634);
            Scope* globalEvalBlockScope = funcInfo->GetGlobalEvalBlockScope();
            byteCodeGenerator->EmitInitCapturedThis(funcInfo, globalEvalBlockScope);
        }
    }
    else
    {TRACE_IT(38635);
        Scope *scope = pnodeBlock->sxBlock.scope;
        if (scope)
        {TRACE_IT(38636);
            if (scope->GetMustInstantiate())
            {TRACE_IT(38637);
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeInObject);
            }
            else
            {TRACE_IT(38638);
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeDirect);
            }
        }
        else
        {TRACE_IT(38639);
            debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnodeBlock, Js::DiagExtraScopesType::DiagBlockScopeInSlot);
        }
    }

    byteCodeGenerator->InitBlockScopedContent(pnodeBlock, debuggerScope, funcInfo);
}

void EndEmitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    if (BlockHasOwnScope(pnodeBlock, byteCodeGenerator))
    {TRACE_IT(38640);
        Scope *scope = pnodeBlock->sxBlock.scope;
        Assert(scope);
        Assert(scope == byteCodeGenerator->GetCurrentScope());
        byteCodeGenerator->PopScope();
    }

    byteCodeGenerator->RecordEndScopeObject(pnodeBlock);
}

void CloneEmitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    if (BlockHasOwnScope(pnodeBlock, byteCodeGenerator))
    {TRACE_IT(38641);
        // Only let variables have observable behavior when there are per iteration
        // bindings.  const variables do not since they are immutable.  Therefore,
        // (and the spec agrees), only create new scope clones if the loop variable
        // is a let declaration.
        bool isConst = false;
        pnodeBlock->sxBlock.scope->ForEachSymbolUntil([&isConst](Symbol * const sym) {
            // Exploit the fact that a for loop sxBlock can only have let and const
            // declarations, and can only have one or the other, regardless of how
            // many syms there might be.  Thus only check the first sym.
            isConst = sym->GetDecl()->nop == knopConstDecl;
            return true;
        });

        if (!isConst)
        {TRACE_IT(38642);
            Scope *scope = pnodeBlock->sxBlock.scope;
            Assert(scope == byteCodeGenerator->GetCurrentScope());

            if (scope->GetMustInstantiate())
            {TRACE_IT(38643);
                Js::OpCode op = scope->GetIsObject() ? Js::OpCode::CloneBlockScope : Js::OpCode::CloneInnerScopeSlots;

                byteCodeGenerator->Writer()->Unsigned1(op, scope->GetInnerScopeIndex());
            }
        }
    }
}

void EmitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fReturnValue)
{TRACE_IT(38644);
    Assert(pnodeBlock->nop == knopBlock);
    ParseNode *pnode = pnodeBlock->sxBlock.pnodeStmt;
    if (pnode == nullptr)
    {TRACE_IT(38645);
        return;
    }

    BeginEmitBlock(pnodeBlock, byteCodeGenerator, funcInfo);

    ParseNode *pnodeLastValStmt = pnodeBlock->sxBlock.pnodeLastValStmt;

    while (pnode->nop == knopList)
    {TRACE_IT(38646);
        ParseNode* stmt = pnode->sxBin.pnode1;
        if (stmt == pnodeLastValStmt)
        {TRACE_IT(38647);
            // This is the last guaranteed return value, so any potential return values have to be
            // copied to the return register from this point forward.
            pnodeLastValStmt = nullptr;
        }
        byteCodeGenerator->EmitTopLevelStatement(stmt, funcInfo, fReturnValue && (pnodeLastValStmt == nullptr));
        pnode = pnode->sxBin.pnode2;
    }

    if (pnode == pnodeLastValStmt)
    {TRACE_IT(38648);
        pnodeLastValStmt = nullptr;
    }
    byteCodeGenerator->EmitTopLevelStatement(pnode, funcInfo, fReturnValue && (pnodeLastValStmt == nullptr));

    EndEmitBlock(pnodeBlock, byteCodeGenerator, funcInfo);
}

void ClearTmpRegs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, FuncInfo* emitFunc)
{TRACE_IT(38649);
    if (emitFunc->IsTmpReg(pnode->location))
    {TRACE_IT(38650);
        pnode->location = Js::Constants::NoRegister;
    }
}

void ByteCodeGenerator::EmitTopLevelStatement(ParseNode *stmt, FuncInfo *funcInfo, BOOL fReturnValue)
{TRACE_IT(38651);
    if (stmt->nop == knopFncDecl && stmt->sxFnc.IsDeclaration())
    {TRACE_IT(38652);
        // Function declarations (not function-declaration RHS's) are already fully processed.
        // Skip them here so the temp registers don't get messed up.
        return;
    }

    if (stmt->nop == knopName || stmt->nop == knopDot)
    {TRACE_IT(38653);
        // Generating span for top level names are mostly useful in debugging mode, because user can debug it even though no side-effect expected.
        // But the name can have runtime error, e.g., foo.bar; // where foo is not defined.
        // At this time we need to throw proper line number and offset. so recording on all modes will be useful.
        StartStatement(stmt);
        Writer()->Empty(Js::OpCode::Nop);
        EndStatement(stmt);
    }

    Emit(stmt, this, funcInfo, fReturnValue, false/*isConstructorCall*/, nullptr/*bindPnode*/, true/*isTopLevel*/);
    if (funcInfo->IsTmpReg(stmt->location))
    {TRACE_IT(38654);
        if (!stmt->isUsed && !fReturnValue)
        {TRACE_IT(38655);
            m_writer.Reg1(Js::OpCode::Unused, stmt->location);
        }
        funcInfo->ReleaseLoc(stmt);
    }
}

// ByteCodeGenerator::DefineFunctions
//
// Emit byte code for scope-wide function definitions before any calls in the scope, regardless of lexical
// order. Note that stores to the closure array are not emitted until we see the knopFncDecl in the tree
// to make sure that sources of the stores have been defined.
void ByteCodeGenerator::DefineFunctions(FuncInfo *funcInfoParent)
{TRACE_IT(38656);
    // DefineCachedFunctions doesn't depend on whether the user vars are declared or not, so
    // we'll just overload this variable to mean that the functions getting called again and we don't need to do anything
    if (funcInfoParent->GetHasCachedScope())
    {TRACE_IT(38657);
        this->DefineCachedFunctions(funcInfoParent);
    }
    else
    {TRACE_IT(38658);
        this->DefineUncachedFunctions(funcInfoParent);
    }
}

// Iterate over all child functions in a function's parameter and body scopes.
template<typename Fn>
void MapContainerScopeFunctions(ParseNode* pnodeScope, Fn fn)
{TRACE_IT(38659);
    auto mapFncDeclsInScopeList = [&](ParseNode *pnodeHead)
    {TRACE_IT(38660);
        for (ParseNode *pnode = pnodeHead; pnode != nullptr;)
        {TRACE_IT(38661);
            switch (pnode->nop)
            {
            case knopFncDecl:
                fn(pnode);
                pnode = pnode->sxFnc.pnodeNext;
                break;

            case knopBlock:
                pnode = pnode->sxBlock.pnodeNext;
                break;

            case knopCatch:
                pnode = pnode->sxCatch.pnodeNext;
                break;

            case knopWith:
                pnode = pnode->sxWith.pnodeNext;
                break;

            default:
                AssertMsg(false, "Unexpected opcode in tree of scopes");
                return;
            }
        }
    };
    pnodeScope->sxFnc.MapContainerScopes(mapFncDeclsInScopeList);
}

void ByteCodeGenerator::DefineCachedFunctions(FuncInfo *funcInfoParent)
{TRACE_IT(38662);
    ParseNode *pnodeParent = funcInfoParent->root;
    uint slotCount = 0;

    auto countFncSlots = [&](ParseNode *pnodeFnc)
    {TRACE_IT(38663);
        if (pnodeFnc->sxFnc.GetFuncSymbol() != nullptr && pnodeFnc->sxFnc.IsDeclaration())
        {TRACE_IT(38664);
            slotCount++;
        }
    };
    MapContainerScopeFunctions(pnodeParent, countFncSlots);

    if (slotCount == 0)
    {TRACE_IT(38665);
        return;
    }

    size_t extraBytesActual = AllocSizeMath::Mul(slotCount, sizeof(Js::FuncInfoEntry));
    // Reg2Aux takes int for byteCount so we need to convert to int. OOM if we can't because it would truncate data.
    if (extraBytesActual > INT_MAX)
    {TRACE_IT(38666);
        Js::Throw::OutOfMemory();
    }
    int extraBytes = (int)extraBytesActual;

    Js::FuncInfoArray *info = AnewPlus(alloc, extraBytes, Js::FuncInfoArray, slotCount);

    slotCount = 0;

    auto fillEntries = [&](ParseNode *pnodeFnc)
    {TRACE_IT(38667);
        Symbol *sym = pnodeFnc->sxFnc.GetFuncSymbol();
        if (sym != nullptr && (pnodeFnc->sxFnc.IsDeclaration()))
        {TRACE_IT(38668);
            AssertMsg(!pnodeFnc->sxFnc.IsGenerator(), "Generator functions are not supported by InitCachedFuncs but since they always escape they should disable function caching");
            Js::FuncInfoEntry *entry = &info->elements[slotCount];
            entry->nestedIndex = pnodeFnc->sxFnc.nestedIndex;
            entry->scopeSlot = sym->GetScopeSlot();
            slotCount++;
        }
    };
    MapContainerScopeFunctions(pnodeParent, fillEntries);

    m_writer.AuxNoReg(Js::OpCode::InitCachedFuncs,
        info,
        sizeof(Js::FuncInfoArray) + extraBytes,
        sizeof(Js::FuncInfoArray) + extraBytes);

    slotCount = 0;
    auto defineOrGetCachedFunc = [&](ParseNode *pnodeFnc)
    {TRACE_IT(38669);
        Symbol *sym = pnodeFnc->sxFnc.GetFuncSymbol();
        if (pnodeFnc->sxFnc.IsDeclaration())
        {TRACE_IT(38670);
            // Do we need to define the function here (i.e., is it not one of our cached locals)?
            // Only happens if the sym is null (e.g., function x.y(){}).
            if (sym == nullptr)
            {TRACE_IT(38671);
                this->DefineOneFunction(pnodeFnc, funcInfoParent);
            }
            else if (!sym->IsInSlot(funcInfoParent) && sym->GetLocation() != Js::Constants::NoRegister)
            {TRACE_IT(38672);
                // If it was defined by InitCachedFuncs, do we need to put it in a register rather than a slot?
                m_writer.Reg1Unsigned1(Js::OpCode::GetCachedFunc, sym->GetLocation(), slotCount);
            }
            // The "x = function() {...}" case is being generated on the fly, during emission,
            // so the caller expects to be able to release this register.
            funcInfoParent->ReleaseLoc(pnodeFnc);
            pnodeFnc->location = Js::Constants::NoRegister;
            slotCount++;
        }
    };
    MapContainerScopeFunctions(pnodeParent, defineOrGetCachedFunc);

    AdeletePlus(alloc, extraBytes, info);
}

void ByteCodeGenerator::DefineUncachedFunctions(FuncInfo *funcInfoParent)
{TRACE_IT(38673);
    ParseNode *pnodeParent = funcInfoParent->root;
    auto defineCheck = [&](ParseNode *pnodeFnc)
    {TRACE_IT(38674);
        Assert(pnodeFnc->nop == knopFncDecl);

        //
        // Don't define the function upfront in following cases
        // 1. x = function() {...};
        //    Don't define the function for all modes.
        //    Such a function can only be accessed via the LHS, so we define it at the assignment point
        //    rather than the scope entry to save a register (and possibly save the whole definition).
        //
        // 2. x = function f() {...};
        //    f is not visible in the enclosing scope.
        //    Such function expressions should be emitted only at the assignment point, as can be used only
        //    after the assignment. Might save register.
        //

        if (pnodeFnc->sxFnc.IsDeclaration())
        {TRACE_IT(38675);
            this->DefineOneFunction(pnodeFnc, funcInfoParent);
            // The "x = function() {...}" case is being generated on the fly, during emission,
            // so the caller expects to be able to release this register.
            funcInfoParent->ReleaseLoc(pnodeFnc);
            pnodeFnc->location = Js::Constants::NoRegister;
        }
    };
    MapContainerScopeFunctions(pnodeParent, defineCheck);
}

void EmitAssignmentToFuncName(ParseNode *pnodeFnc, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfoParent)
{TRACE_IT(38676);
    // Assign the location holding the func object reference to the given name.
    Symbol *sym = pnodeFnc->sxFnc.pnodeName->sxVar.sym;

    if (sym != nullptr && !sym->GetIsFuncExpr())
    {TRACE_IT(38677);
        if (sym->GetIsModuleExportStorage())
        {TRACE_IT(38678);
            byteCodeGenerator->EmitPropStore(pnodeFnc->location, sym, nullptr, funcInfoParent);
        }
        else if (sym->GetIsGlobal())
        {TRACE_IT(38679);
            Js::PropertyId propertyId = sym->GetPosition();
            byteCodeGenerator->EmitGlobalFncDeclInit(pnodeFnc->location, propertyId, funcInfoParent);
            if (byteCodeGenerator->GetFlags() & fscrEval && !funcInfoParent->GetIsStrictMode())
            {TRACE_IT(38680);
                byteCodeGenerator->EmitPropStore(pnodeFnc->location, sym, nullptr, funcInfoParent);
            }
        }
        else
        {TRACE_IT(38681);
            if (sym->NeedsSlotAlloc(funcInfoParent))
            {TRACE_IT(38682);
                if (!sym->GetHasNonCommittedReference() ||
                    (funcInfoParent->GetParsedFunctionBody()->DoStackNestedFunc()))
                {TRACE_IT(38683);
                    // No point in trying to optimize if there are no references before we have to commit to slot.
                    // And not safe to delay putting a stack function in the slot, since we may miss boxing.
                    sym->SetIsCommittedToSlot();
                }
            }

            if (sym->GetScope()->GetFunc() != byteCodeGenerator->TopFuncInfo())
            {TRACE_IT(38684);
                byteCodeGenerator->EmitPropStore(pnodeFnc->location, sym, nullptr, funcInfoParent);
            }
            else if (!sym->GetIsBlockVar() || sym->HasRealBlockVarRef() || sym->GetScope()->GetIsObject())
            {TRACE_IT(38685);
                byteCodeGenerator->EmitLocalPropInit(pnodeFnc->location, sym, funcInfoParent);
            }

            Symbol * fncScopeSym = sym->GetFuncScopeVarSym();

            if (fncScopeSym)
            {TRACE_IT(38686);
                if (fncScopeSym->GetIsGlobal() && byteCodeGenerator->GetFlags() & fscrEval)
                {TRACE_IT(38687);
                    Js::PropertyId propertyId = fncScopeSym->GetPosition();
                    byteCodeGenerator->EmitGlobalFncDeclInit(pnodeFnc->location, propertyId, funcInfoParent);
                }
                else
                {TRACE_IT(38688);
                    byteCodeGenerator->EmitPropStore(pnodeFnc->location, fncScopeSym, nullptr, funcInfoParent, false, false, /* isFncDeclVar */true);
                }
            }
        }
    }
}

Js::RegSlot ByteCodeGenerator::DefineOneFunction(ParseNode *pnodeFnc, FuncInfo *funcInfoParent, bool generateAssignment, Js::RegSlot regEnv, Js::RegSlot frameDisplayTemp)
{TRACE_IT(38689);
    Assert(pnodeFnc->nop == knopFncDecl);

    funcInfoParent->AcquireLoc(pnodeFnc);

    if (regEnv == Js::Constants::NoRegister)
    {TRACE_IT(38690);
        // If the child needs a closure, find a heap-allocated frame to pass to it.
        if (frameDisplayTemp != Js::Constants::NoRegister)
        {TRACE_IT(38691);
            // We allocated a temp to hold a local frame display value. Use that.
            // It's likely that the FD is on the stack, and we used the temp to load it back.
            regEnv = frameDisplayTemp;
        }
        else if (funcInfoParent->frameDisplayRegister != Js::Constants::NoRegister)
        {TRACE_IT(38692);
            // This function has built a frame display, so pass it down.
            regEnv = funcInfoParent->frameDisplayRegister;
        }
        else
        {TRACE_IT(38693);
            // This function has no captured locals but inherits a closure environment, so pass it down.
            regEnv = funcInfoParent->GetEnvRegister();
        }

        regEnv = this->PrependLocalScopes(regEnv, Js::Constants::NoRegister, funcInfoParent);
    }

    // AssertMsg(funcInfo->nonLocalSymbols == 0 || regEnv != funcInfoParent->nullConstantRegister,
    // "We need a closure for the nested function");

    // If we are in a parameter scope and it is not merged with body scope then we have to create the child function as an inner function
    if (regEnv == funcInfoParent->frameDisplayRegister || regEnv == funcInfoParent->GetEnvRegister())
    {TRACE_IT(38694);
        m_writer.NewFunction(pnodeFnc->location, pnodeFnc->sxFnc.nestedIndex, pnodeFnc->sxFnc.IsCoroutine());
    }
    else
    {TRACE_IT(38695);
        m_writer.NewInnerFunction(pnodeFnc->location, pnodeFnc->sxFnc.nestedIndex, regEnv, pnodeFnc->sxFnc.IsCoroutine());
    }

    if (funcInfoParent->IsGlobalFunction() && (this->flags & fscrEval))
    {TRACE_IT(38696);
        // A function declared at global scope in eval is untrackable,
        // so make sure the caller's cached scope is invalidated.
        this->funcEscapes = true;
    }
    else
    {TRACE_IT(38697);
        if (pnodeFnc->sxFnc.IsDeclaration())
        {TRACE_IT(38698);
            Symbol * funcSymbol = pnodeFnc->sxFnc.GetFuncSymbol();
            if (funcSymbol)
            {TRACE_IT(38699);
                // In the case where a let/const declaration is the same symbol name
                // as the function declaration (shadowing case), the let/const var and
                // the function declaration symbol are the same and share the same flags
                // (particularly, sym->GetIsBlockVar() for this code path).
                //
                // For example:
                // let a = 0;       // <-- sym->GetIsBlockVar() = true
                // function b(){}   // <-- sym2->GetIsBlockVar() = false
                //
                // let x = 0;       // <-- sym3->GetIsBlockVar() = true
                // function x(){}   // <-- sym3->GetIsBlockVar() = true
                //
                // In order to tell if the function is actually part
                // of a block scope, we compare against the function scope here.
                // Note that having a function with the same name as a let/const declaration
                // is a redeclaration error, but we're pushing the fix for this out since it's
                // a bit involved.
                Assert(funcInfoParent->GetBodyScope() != nullptr && funcSymbol->GetScope() != nullptr);
                bool isFunctionDeclarationInBlock = funcSymbol->GetIsBlockVar();

                // Track all vars/lets/consts register slot function declarations.
                if (ShouldTrackDebuggerMetadata()
                    // If this is a let binding function declaration at global level, we want to
                    // be sure to track the register location as well.
                    && !(funcInfoParent->IsGlobalFunction() && !isFunctionDeclarationInBlock))
                {TRACE_IT(38700);
                    if (!funcSymbol->IsInSlot(funcInfoParent))
                    {TRACE_IT(38701);
                        funcInfoParent->byteCodeFunction->GetFunctionBody()->InsertSymbolToRegSlotList(funcSymbol->GetName(), pnodeFnc->location, funcInfoParent->varRegsCount);
                    }
                }

                if (isFunctionDeclarationInBlock)
                {TRACE_IT(38702);
                    // We only track inner let bindings for the debugger side.
                    this->TrackFunctionDeclarationPropertyForDebugger(funcSymbol, funcInfoParent);
                }
            }
        }
    }

    if (pnodeFnc->sxFnc.IsDefaultModuleExport())
    {TRACE_IT(38703);
        this->EmitAssignmentToDefaultModuleExport(pnodeFnc, funcInfoParent);
    }

    if (pnodeFnc->sxFnc.pnodeName == nullptr || !generateAssignment)
    {TRACE_IT(38704);
        return regEnv;
    }

    EmitAssignmentToFuncName(pnodeFnc, this, funcInfoParent);

    return regEnv;
}

void ByteCodeGenerator::DefineUserVars(FuncInfo *funcInfo)
{TRACE_IT(38705);
    // Initialize scope-wide variables on entry to the scope. TODO: optimize by detecting uses that are always reached
    // by an existing initialization.

    BOOL fGlobal = funcInfo->IsGlobalFunction();
    ParseNode *pnode;
    Js::FunctionBody *byteCodeFunction = funcInfo->GetParsedFunctionBody();
    // Global declarations need a temp register to hold the init value, but the node shouldn't get a register.
    // Just assign one on the fly and re-use it for all initializations.
    Js::RegSlot tmpReg = fGlobal ? funcInfo->AcquireTmpRegister() : Js::Constants::NoRegister;

    for (pnode = funcInfo->root->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
    {TRACE_IT(38706);
        Symbol* sym = pnode->sxVar.sym;

        if (sym != nullptr && !(pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar()))
        {TRACE_IT(38707);
            if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
            {TRACE_IT(38708);
                // The init node was bound to the catch object, because it's inside a catch and has the
                // same name as the catch object. But we want to define a user var at function scope,
                // so find the right symbol. (We'll still assign the RHS value to the catch object symbol.)
                // This also applies to a var declaration in the same scope as a let declaration.
#if DBG
                if (sym->GetIsArguments())
                {TRACE_IT(38709);
                    // There is a block scoped var named arguments
                    Assert(!funcInfo->GetHasArguments());
                    continue;
                }
                else if (!sym->GetIsCatch())
                {TRACE_IT(38710);
                    // Assert that catch cannot be at function scope and let and var at function scope is redeclaration error.
                    Assert(funcInfo->bodyScope != sym->GetScope());
                }
#endif
                sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
                Assert(sym && !sym->GetIsCatch() && !sym->GetIsBlockVar());
            }

            if (sym->GetSymbolType() == STVariable && !sym->GetIsModuleExportStorage())
            {TRACE_IT(38711);
                if (fGlobal)
                {TRACE_IT(38712);
                    Js::PropertyId propertyId = sym->EnsurePosition(this);
                    // We do need to initialize some globals to avoid JS errors on loading undefined variables.
                    // But we first need to make sure we're not trashing built-ins.

                    if (this->flags & fscrEval)
                    {TRACE_IT(38713);
                        if (funcInfo->byteCodeFunction->GetIsStrictMode())
                        {TRACE_IT(38714);
                            // Check/Init the property of the frame object
                            this->m_writer.ElementRootU(Js::OpCode::LdLocalElemUndef,
                                funcInfo->FindOrAddReferencedPropertyId(propertyId));
                        }
                        else
                        {TRACE_IT(38715);
                            // The check and the init involve the first element in the scope chain.
                            this->m_writer.ElementScopedU(
                                Js::OpCode::LdElemUndefScoped, funcInfo->FindOrAddReferencedPropertyId(propertyId));
                        }
                    }
                    else
                    {TRACE_IT(38716);
                        this->m_writer.ElementU(Js::OpCode::LdElemUndef, ByteCodeGenerator::RootObjectRegister,
                            funcInfo->FindOrAddReferencedPropertyId(propertyId));
                    }
                }
                else if (!sym->GetIsArguments())
                {TRACE_IT(38717);
                    if (sym->NeedsSlotAlloc(funcInfo))
                    {TRACE_IT(38718);
                        if (!sym->GetHasNonCommittedReference() ||
                            (sym->GetHasFuncAssignment() && funcInfo->GetParsedFunctionBody()->DoStackNestedFunc()))
                        {TRACE_IT(38719);
                            // No point in trying to optimize if there are no references before we have to commit to slot.
                            // And not safe to delay putting a stack function in the slot, since we may miss boxing.
                            sym->SetIsCommittedToSlot();
                        }
                    }

                    // Undef-initialize the home location if it is a register (not closure-captured, or else capture
                    // is delayed) or a property of an object.
                    if ((!sym->GetHasInit() && !sym->IsInSlot(funcInfo)) ||
                        (funcInfo->bodyScope->GetIsObject() && !funcInfo->GetHasCachedScope()))
                    {TRACE_IT(38720);
                        Js::RegSlot reg = sym->GetLocation();
                        if (reg == Js::Constants::NoRegister)
                        {TRACE_IT(38721);
                            Assert(sym->IsInSlot(funcInfo));
                            reg = funcInfo->AcquireTmpRegister();
                        }
                        this->m_writer.Reg1(Js::OpCode::LdUndef, reg);
                        this->EmitLocalPropInit(reg, sym, funcInfo);

                        if (ShouldTrackDebuggerMetadata() && !sym->GetHasInit() && !sym->IsInSlot(funcInfo))
                        {TRACE_IT(38722);
                            byteCodeFunction->InsertSymbolToRegSlotList(sym->GetName(), reg, funcInfo->varRegsCount);
                        }

                        funcInfo->ReleaseTmpRegister(reg);
                    }
                }
                else if (ShouldTrackDebuggerMetadata())
                {TRACE_IT(38723);
                    if (!sym->GetHasInit() && !sym->IsInSlot(funcInfo))
                    {TRACE_IT(38724);
                        Js::RegSlot reg = sym->GetLocation();
                        if (reg != Js::Constants::NoRegister)
                        {TRACE_IT(38725);
                            byteCodeFunction->InsertSymbolToRegSlotList(sym->GetName(), reg, funcInfo->varRegsCount);
                        }
                    }
                }  
                sym->SetHasInit(TRUE);
            }
        }

    }
    if (tmpReg != Js::Constants::NoRegister)
    {TRACE_IT(38726);
        funcInfo->ReleaseTmpRegister(tmpReg);
    }

    for (int i = 0; i < funcInfo->nonUserNonTempRegistersToInitialize.Count(); ++i)
    {TRACE_IT(38727);
        m_writer.Reg1(Js::OpCode::LdUndef, funcInfo->nonUserNonTempRegistersToInitialize.Item(i));
    }
}

void ByteCodeGenerator::InitBlockScopedNonTemps(ParseNode *pnode, FuncInfo *funcInfo)
{TRACE_IT(38728);
    // Initialize all non-temp register variables on entry to the enclosing func - in particular,
    // those with lifetimes that begin after the start of user code and may not be initialized normally.
    // This protects us from, for instance, trying to restore garbage on bailout.
    // It was originally done in debugger mode only, but we do it always to avoid issues with boxing
    // garbage on exit from jitted loop bodies.
    while (pnode)
    {TRACE_IT(38729);
        switch (pnode->nop)
        {
        case knopFncDecl:
        {TRACE_IT(38730);
            // If this is a block-scoped function, initialize it.
            ParseNode *pnodeName = pnode->sxFnc.pnodeName;
            if (!pnode->sxFnc.IsMethod() && pnodeName && pnodeName->nop == knopVarDecl)
            {TRACE_IT(38731);
                Symbol *sym = pnodeName->sxVar.sym;
                Assert(sym);
                if (sym->GetLocation() != Js::Constants::NoRegister &&
                    sym->GetScope()->IsBlockScope(funcInfo) &&
                    sym->GetScope()->GetFunc() == funcInfo)
                {TRACE_IT(38732);
                    this->m_writer.Reg1(Js::OpCode::LdUndef, sym->GetLocation());
                }
            }

            // No need to recurse to the nested scopes, as they belong to a nested function.
            pnode = pnode->sxFnc.pnodeNext;
            break;
        }

        case knopBlock:
        {TRACE_IT(38733);
            Scope *scope = pnode->sxBlock.scope;
            if (scope)
            {TRACE_IT(38734);
                if (scope->IsBlockScope(funcInfo))
                {TRACE_IT(38735);
                    Js::RegSlot scopeLoc = scope->GetLocation();
                    if (scopeLoc != Js::Constants::NoRegister && !funcInfo->IsTmpReg(scopeLoc))
                    {TRACE_IT(38736);
                        this->m_writer.Reg1(Js::OpCode::LdUndef, scopeLoc);
                    }
                }
                auto fnInit = [this, funcInfo](ParseNode *pnode)
                {TRACE_IT(38737);
                    Symbol *sym = pnode->sxVar.sym;
                    if (!sym->IsInSlot(funcInfo) && !sym->GetIsGlobal() && !sym->GetIsModuleImport())
                    {TRACE_IT(38738);
                        this->m_writer.Reg1(Js::OpCode::InitUndecl, pnode->sxVar.sym->GetLocation());
                    }
                };
                IterateBlockScopedVariables(pnode, fnInit);
            }
            InitBlockScopedNonTemps(pnode->sxBlock.pnodeScopes, funcInfo);
            pnode = pnode->sxBlock.pnodeNext;
            break;
        }
        case knopCatch:
            InitBlockScopedNonTemps(pnode->sxCatch.pnodeScopes, funcInfo);
            pnode = pnode->sxCatch.pnodeNext;
            break;

        case knopWith:
        {TRACE_IT(38739);
            Js::RegSlot withLoc = pnode->location;
            AssertMsg(withLoc != Js::Constants::NoRegister && !funcInfo->IsTmpReg(withLoc),
                "We should put with objects at known stack locations in debug mode");
            this->m_writer.Reg1(Js::OpCode::LdUndef, withLoc);
            InitBlockScopedNonTemps(pnode->sxWith.pnodeScopes, funcInfo);
            pnode = pnode->sxWith.pnodeNext;
            break;
        }

        default:
            Assert(false);
            return;
        }
    }
}

void ByteCodeGenerator::EmitScopeObjectInit(FuncInfo *funcInfo)
{TRACE_IT(38740);
    Assert(!funcInfo->byteCodeFunction->GetFunctionBody()->DoStackNestedFunc());

    if (!funcInfo->GetHasCachedScope() /* || forcing scope/inner func caching */)
    {TRACE_IT(38741);
        return;
    }

    Scope* currentScope = funcInfo->GetCurrentChildScope();
    uint slotCount = currentScope->GetScopeSlotCount();
    uint cachedFuncCount = 0;
    Js::PropertyId firstFuncSlot = Js::Constants::NoProperty;
    Js::PropertyId firstVarSlot = Js::Constants::NoProperty;
    uint extraAlloc = (slotCount + Js::ActivationObjectEx::ExtraSlotCount()) * sizeof(Js::PropertyId);

    // Create and fill the array of local property ID's.
    // They all have slots assigned to them already (if they need them): see StartEmitFunction.

    Js::PropertyIdArray *propIds = funcInfo->GetParsedFunctionBody()->AllocatePropertyIdArrayForFormals(extraAlloc, slotCount, Js::ActivationObjectEx::ExtraSlotCount());

    ParseNode *pnodeFnc = funcInfo->root;
    ParseNode *pnode;
    Symbol *sym;

    if (funcInfo->GetFuncExprNameReference() && pnodeFnc->sxFnc.GetFuncSymbol()->GetScope() == funcInfo->GetBodyScope())
    {TRACE_IT(38742);
        Symbol::SaveToPropIdArray(pnodeFnc->sxFnc.GetFuncSymbol(), propIds, this);
    }

    if (funcInfo->GetHasArguments())
    {TRACE_IT(38743);
        // Because the arguments object can access all instances of same-named formals ("function(x,x){...}"),
        // be sure we initialize any duplicate appearances of a formal parameter to "NoProperty".
        Js::PropertyId slot = 0;
        auto initArg = [&](ParseNode *pnode)
        {TRACE_IT(38744);
            if (pnode->IsVarLetOrConst())
            {TRACE_IT(38745);
                Symbol *sym = pnode->sxVar.sym;
                Assert(sym);
                if (sym->GetScopeSlot() == slot)
                {TRACE_IT(38746);
                    // This is the last appearance of the formal, so record the ID.
                    Symbol::SaveToPropIdArray(sym, propIds, this);
                }
                else
                {TRACE_IT(38747);
                    // This is an earlier duplicate appearance of the formal, so use NoProperty as a placeholder
                    // since this slot can't be accessed by name.
                    Assert(sym->GetScopeSlot() != Js::Constants::NoProperty && sym->GetScopeSlot() > slot);
                    propIds->elements[slot] = Js::Constants::NoProperty;
                }
            }
            else
            {TRACE_IT(38748);
                // This is for patterns
                propIds->elements[slot] = Js::Constants::NoProperty;
            }
            slot++;
        };
        MapFormalsWithoutRest(pnodeFnc, initArg);

        // If the rest is in the slot - we need to keep that slot.
        if (pnodeFnc->sxFnc.pnodeRest != nullptr && pnodeFnc->sxFnc.pnodeRest->sxVar.sym->IsInSlot(funcInfo))
        {TRACE_IT(38749);
            Symbol::SaveToPropIdArray(pnodeFnc->sxFnc.pnodeRest->sxVar.sym, propIds, this);
        }
    }
    else
    {
        MapFormals(pnodeFnc, [&](ParseNode *pnode)
        {
            if (pnode->IsVarLetOrConst())
            {TRACE_IT(38750);
                Symbol::SaveToPropIdArray(pnode->sxVar.sym, propIds, this);
            }
        });
    }

    auto saveFunctionVarsToPropIdArray = [&](ParseNode *pnodeFunction)
    {TRACE_IT(38751);
        if (pnodeFunction->sxFnc.IsDeclaration())
        {TRACE_IT(38752);
            ParseNode *pnodeName = pnodeFunction->sxFnc.pnodeName;
            if (pnodeName != nullptr)
            {TRACE_IT(38753);
                while (pnodeName->nop == knopList)
                {TRACE_IT(38754);
                    if (pnodeName->sxBin.pnode1->nop == knopVarDecl)
                    {TRACE_IT(38755);
                        sym = pnodeName->sxBin.pnode1->sxVar.sym;
                        if (sym)
                        {TRACE_IT(38756);
                            Symbol::SaveToPropIdArray(sym, propIds, this, &firstFuncSlot);
                        }
                    }
                    pnodeName = pnodeName->sxBin.pnode2;
                }
                if (pnodeName->nop == knopVarDecl)
                {TRACE_IT(38757);
                    sym = pnodeName->sxVar.sym;
                    if (sym)
                    {TRACE_IT(38758);
                        Symbol::SaveToPropIdArray(sym, propIds, this, &firstFuncSlot);
                        cachedFuncCount++;
                    }
                }
            }
        }
    };
    MapContainerScopeFunctions(pnodeFnc, saveFunctionVarsToPropIdArray);

    if (currentScope->GetScopeType() != ScopeType_Parameter)
    {TRACE_IT(38759);
        for (pnode = pnodeFnc->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
        {TRACE_IT(38760);
            sym = pnode->sxVar.sym;
            if (!(pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar()))
            {TRACE_IT(38761);
                if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
                {TRACE_IT(38762);
                    sym = currentScope->FindLocalSymbol(sym->GetName());
                }
                Symbol::SaveToPropIdArray(sym, propIds, this, &firstVarSlot);
            }
        }

        ParseNode *pnodeBlock = pnodeFnc->sxFnc.pnodeScopes;
        for (pnode = pnodeBlock->sxBlock.pnodeLexVars; pnode; pnode = pnode->sxVar.pnodeNext)
        {TRACE_IT(38763);
            sym = pnode->sxVar.sym;
            Symbol::SaveToPropIdArray(sym, propIds, this, &firstVarSlot);
        }

        pnodeBlock = pnodeFnc->sxFnc.pnodeBodyScope;
        for (pnode = pnodeBlock->sxBlock.pnodeLexVars; pnode; pnode = pnode->sxVar.pnodeNext)
        {TRACE_IT(38764);
            sym = pnode->sxVar.sym;
            Symbol::SaveToPropIdArray(sym, propIds, this, &firstVarSlot);
        }
    }
    else
    {TRACE_IT(38765);
        Assert(!funcInfo->IsBodyAndParamScopeMerged());
    }

    if (funcInfo->thisScopeSlot != Js::Constants::NoRegister)
    {TRACE_IT(38766);
        propIds->elements[funcInfo->thisScopeSlot] = Js::PropertyIds::_lexicalThisSlotSymbol;
    }
    if (funcInfo->newTargetScopeSlot != Js::Constants::NoRegister)
    {TRACE_IT(38767);
        propIds->elements[funcInfo->newTargetScopeSlot] = Js::PropertyIds::_lexicalNewTargetSymbol;
    }
    if (funcInfo->superScopeSlot != Js::Constants::NoRegister)
    {TRACE_IT(38768);
        propIds->elements[funcInfo->superScopeSlot] = Js::PropertyIds::_superReferenceSymbol;
    }
    if (funcInfo->superCtorScopeSlot != Js::Constants::NoRegister)
    {TRACE_IT(38769);
        propIds->elements[funcInfo->superCtorScopeSlot] = Js::PropertyIds::_superCtorReferenceSymbol;
    }

    // Write the first func slot and first var slot into the auxiliary data
    Js::PropertyId *slots = propIds->elements + slotCount;
    slots[0] = cachedFuncCount;
    slots[1] = firstFuncSlot;
    slots[2] = firstVarSlot;
    slots[3] = funcInfo->GetParsedFunctionBody()->NewObjectLiteral();

    propIds->hasNonSimpleParams = funcInfo->root->sxFnc.HasNonSimpleParameterList();

    funcInfo->GetParsedFunctionBody()->SetHasCachedScopePropIds(true);
}

void ByteCodeGenerator::SetClosureRegisters(FuncInfo* funcInfo, Js::FunctionBody* byteCodeFunction)
{TRACE_IT(38770);
    if (funcInfo->frameDisplayRegister != Js::Constants::NoRegister)
    {TRACE_IT(38771);
        byteCodeFunction->MapAndSetLocalFrameDisplayRegister(funcInfo->frameDisplayRegister);
    }

    if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
    {TRACE_IT(38772);
        byteCodeFunction->MapAndSetLocalClosureRegister(funcInfo->frameObjRegister);
        byteCodeFunction->SetHasScopeObject(true);
    }
    else if (funcInfo->frameSlotsRegister != Js::Constants::NoRegister)
    {TRACE_IT(38773);
        byteCodeFunction->MapAndSetLocalClosureRegister(funcInfo->frameSlotsRegister);
    }

    if (funcInfo->paramSlotsRegister != Js::Constants::NoRegister)
    {TRACE_IT(38774);
        byteCodeFunction->MapAndSetParamClosureRegister(funcInfo->paramSlotsRegister);
    }
}

void ByteCodeGenerator::FinalizeRegisters(FuncInfo * funcInfo, Js::FunctionBody * byteCodeFunction)
{TRACE_IT(38775);
    if (byteCodeFunction->IsCoroutine())
    {TRACE_IT(38776);
        // EmitYield uses 'false' to create the IteratorResult object
        funcInfo->AssignFalseConstRegister();
    }

    if (funcInfo->NeedEnvRegister())
    {TRACE_IT(38777);
        bool constReg = !funcInfo->GetIsTopLevelEventHandler() && funcInfo->IsGlobalFunction() && !(this->flags & fscrEval);
        funcInfo->AssignEnvRegister(constReg);
    }

    // Set the function body's constant count before emitting anything so that the byte code writer
    // can distinguish constants from variables.
    byteCodeFunction->CheckAndSetConstantCount(funcInfo->constRegsCount);

    this->SetClosureRegisters(funcInfo, byteCodeFunction);

    if (this->IsInDebugMode())
    {TRACE_IT(38778);
        // Give permanent registers to the inner scopes in debug mode.
        uint innerScopeCount = funcInfo->InnerScopeCount();
        byteCodeFunction->SetInnerScopeCount(innerScopeCount);
        if (innerScopeCount)
        {TRACE_IT(38779);
            funcInfo->SetFirstInnerScopeReg(funcInfo->NextVarRegister());
            for (uint i = 1; i < innerScopeCount; i++)
            {TRACE_IT(38780);
                funcInfo->NextVarRegister();
            }
        }
    }

    // NOTE: The FB expects the yield reg to be the final non-temp.
    if (byteCodeFunction->IsCoroutine())
    {TRACE_IT(38781);
        funcInfo->AssignYieldRegister();
    }

    Js::RegSlot firstTmpReg = funcInfo->varRegsCount;
    funcInfo->SetFirstTmpReg(firstTmpReg);
    byteCodeFunction->SetFirstTmpReg(funcInfo->RegCount());
}

void ByteCodeGenerator::InitScopeSlotArray(FuncInfo * funcInfo)
{TRACE_IT(38782);
    // Record slots info for ScopeSlots/ScopeObject.
    uint scopeSlotCount = funcInfo->bodyScope->GetScopeSlotCount();
    bool isSplitScope = !funcInfo->IsBodyAndParamScopeMerged();
    Assert(funcInfo->paramScope == nullptr || funcInfo->paramScope->GetScopeSlotCount() == 0 || isSplitScope);
    uint scopeSlotCountForParamScope = funcInfo->paramScope != nullptr ? funcInfo->paramScope->GetScopeSlotCount() : 0;

    if (scopeSlotCount == 0 && scopeSlotCountForParamScope == 0)
    {TRACE_IT(38783);
        return;
    }

    Js::FunctionBody *byteCodeFunction = funcInfo->GetParsedFunctionBody();
    if (scopeSlotCount > 0 || scopeSlotCountForParamScope > 0)
    {TRACE_IT(38784);
        byteCodeFunction->SetScopeSlotArraySizes(scopeSlotCount, scopeSlotCountForParamScope);
    }

    // TODO: Need to add property ids for the case when scopeSlotCountForParamSCope is non-zero
    if (scopeSlotCount)
    {TRACE_IT(38785);
        Js::PropertyId *propertyIdsForScopeSlotArray = RecyclerNewArrayLeafZ(scriptContext->GetRecycler(), Js::PropertyId, scopeSlotCount);
        byteCodeFunction->SetPropertyIdsForScopeSlotArray(propertyIdsForScopeSlotArray, scopeSlotCount, scopeSlotCountForParamScope);
        AssertMsg(!byteCodeFunction->IsReparsed() || byteCodeFunction->m_wasEverAsmjsMode || byteCodeFunction->scopeSlotArraySize == scopeSlotCount,
            "The slot array size is different between debug and non-debug mode");
#if DEBUG
        for (UINT i = 0; i < scopeSlotCount; i++)
        {TRACE_IT(38786);
            propertyIdsForScopeSlotArray[i] = Js::Constants::NoProperty;
        }
#endif
        auto setPropertyIdForScopeSlotArray =
            [scopeSlotCount, propertyIdsForScopeSlotArray]
            (Js::PropertyId slot, Js::PropertyId propId)
        {TRACE_IT(38787);
            if (slot < 0 || (uint)slot >= scopeSlotCount)
            {TRACE_IT(38788);
                Js::Throw::FatalInternalError();
            }
            propertyIdsForScopeSlotArray[slot] = propId;
        };

        auto setPropIdsForScopeSlotArray = [funcInfo, setPropertyIdForScopeSlotArray](Symbol *const sym)
        {TRACE_IT(38789);
            if (sym->NeedsSlotAlloc(funcInfo))
            {TRACE_IT(38790);
                    // All properties should get correct propertyId here.
                    Assert(sym->HasScopeSlot()); // We can't allocate scope slot now. Any symbol needing scope slot must have allocated it before this point.
                    setPropertyIdForScopeSlotArray(sym->GetScopeSlot(), sym->EnsurePosition(funcInfo));
            }
        };

        funcInfo->GetBodyScope()->ForEachSymbol(setPropIdsForScopeSlotArray);

        // In case of split scope these symbols belong to the param scope not body scope
        if (!isSplitScope)
        {TRACE_IT(38791);
            if (funcInfo->thisScopeSlot != Js::Constants::NoRegister)
            {TRACE_IT(38792);
                setPropertyIdForScopeSlotArray(funcInfo->thisScopeSlot, Js::PropertyIds::_lexicalThisSlotSymbol);
            }

            if (funcInfo->newTargetScopeSlot != Js::Constants::NoRegister)
            {TRACE_IT(38793);
                setPropertyIdForScopeSlotArray(funcInfo->newTargetScopeSlot, Js::PropertyIds::_lexicalNewTargetSymbol);
            }

            if (funcInfo->superScopeSlot != Js::Constants::NoRegister)
            {TRACE_IT(38794);
                setPropertyIdForScopeSlotArray(funcInfo->superScopeSlot, Js::PropertyIds::_superReferenceSymbol);
            }

            if (funcInfo->superCtorScopeSlot != Js::Constants::NoRegister)
            {TRACE_IT(38795);
                setPropertyIdForScopeSlotArray(funcInfo->superCtorScopeSlot, Js::PropertyIds::_superCtorReferenceSymbol);
            }
        }

#if DEBUG
        for (UINT i = 0; i < scopeSlotCount; i++)
        {TRACE_IT(38796);
            Assert(propertyIdsForScopeSlotArray[i] != Js::Constants::NoProperty
                || funcInfo->frameObjRegister != Js::Constants::NoRegister); // ScopeObject may have unassigned entries, e.g. for same-named parameters
        }
#endif
    }
}

// temporarily load all constants and special registers in a single block
void ByteCodeGenerator::LoadAllConstants(FuncInfo *funcInfo)
{TRACE_IT(38797);
    Symbol *sym;

    Js::FunctionBody *byteCodeFunction = funcInfo->GetParsedFunctionBody();
    byteCodeFunction->CreateConstantTable();

    if (funcInfo->nullConstantRegister != Js::Constants::NoRegister)
    {TRACE_IT(38798);
        byteCodeFunction->RecordNullObject(byteCodeFunction->MapRegSlot(funcInfo->nullConstantRegister));
    }

    if (funcInfo->undefinedConstantRegister != Js::Constants::NoRegister)
    {TRACE_IT(38799);
        byteCodeFunction->RecordUndefinedObject(byteCodeFunction->MapRegSlot(funcInfo->undefinedConstantRegister));
    }

    if (funcInfo->trueConstantRegister != Js::Constants::NoRegister)
    {TRACE_IT(38800);
        byteCodeFunction->RecordTrueObject(byteCodeFunction->MapRegSlot(funcInfo->trueConstantRegister));
    }

    if (funcInfo->falseConstantRegister != Js::Constants::NoRegister)
    {TRACE_IT(38801);
        byteCodeFunction->RecordFalseObject(byteCodeFunction->MapRegSlot(funcInfo->falseConstantRegister));
    }

    if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
    {TRACE_IT(38802);
        m_writer.RecordObjectRegister(funcInfo->frameObjRegister);
        if (!funcInfo->GetApplyEnclosesArgs())
        {TRACE_IT(38803);
            this->EmitScopeObjectInit(funcInfo);
        }

#if DBG
        uint count = 0;
        funcInfo->GetBodyScope()->ForEachSymbol([&](Symbol *const sym)
        {
            if (sym->NeedsSlotAlloc(funcInfo))
            {TRACE_IT(38804);
                // All properties should get correct propertyId here.
                count++;
            }
        });

        if (funcInfo->GetParamScope() != nullptr)
        {TRACE_IT(38805);
            funcInfo->GetParamScope()->ForEachSymbol([&](Symbol *const sym)
            {
                if (sym->NeedsSlotAlloc(funcInfo))
                {TRACE_IT(38806);
                    // All properties should get correct propertyId here.
                    count++;
                }
            });
        }

        // A reparse should result in the same size of the activation object.
        // Exclude functions which were created from the ByteCodeCache.
        AssertMsg(!byteCodeFunction->IsReparsed() || byteCodeFunction->HasGeneratedFromByteCodeCache() ||
            byteCodeFunction->scopeObjectSize == count || byteCodeFunction->m_wasEverAsmjsMode,
            "The activation object size is different between debug and non-debug mode");
        byteCodeFunction->scopeObjectSize = count;
#endif
    }
    else if (funcInfo->frameSlotsRegister != Js::Constants::NoRegister)
    {TRACE_IT(38807);
        int scopeSlotCount = funcInfo->bodyScope->GetScopeSlotCount();
        int paramSlotCount = funcInfo->paramScope->GetScopeSlotCount();
        if (scopeSlotCount == 0 && paramSlotCount == 0)
        {TRACE_IT(38808);
            AssertMsg(funcInfo->frameDisplayRegister != Js::Constants::NoRegister, "Why do we need scope slots?");
            m_writer.Reg1(Js::OpCode::LdC_A_Null, funcInfo->frameSlotsRegister);
        }
    }

    if (funcInfo->funcExprScope && funcInfo->funcExprScope->GetIsObject())
    {TRACE_IT(38809);
        byteCodeFunction->MapAndSetFuncExprScopeRegister(funcInfo->funcExprScope->GetLocation());
        byteCodeFunction->SetEnvDepth((uint16)-1);
    }

    bool thisLoadedFromParams = false;

    if (funcInfo->NeedEnvRegister())
    {TRACE_IT(38810);
        byteCodeFunction->MapAndSetEnvRegister(funcInfo->GetEnvRegister());
        if (funcInfo->GetIsTopLevelEventHandler())
        {TRACE_IT(38811);
            byteCodeFunction->MapAndSetThisRegisterForEventHandler(funcInfo->thisPointerRegister);
            // The environment is the namespace hierarchy starting with "this".
            Assert(!funcInfo->RegIsConst(funcInfo->GetEnvRegister()));
            thisLoadedFromParams = true;
            this->InvalidateCachedOuterScopes(funcInfo);
        }
        else if (funcInfo->IsGlobalFunction() && !(this->flags & fscrEval))
        {TRACE_IT(38812);
            Assert(funcInfo->RegIsConst(funcInfo->GetEnvRegister()));

            if (funcInfo->GetIsStrictMode())
            {TRACE_IT(38813);
                byteCodeFunction->RecordStrictNullDisplayConstant(byteCodeFunction->MapRegSlot(funcInfo->GetEnvRegister()));
            }
            else
            {TRACE_IT(38814);
                byteCodeFunction->RecordNullDisplayConstant(byteCodeFunction->MapRegSlot(funcInfo->GetEnvRegister()));
            }
        }
        else
        {TRACE_IT(38815);
            // environment may be required to load "this"
            Assert(!funcInfo->RegIsConst(funcInfo->GetEnvRegister()));
            this->InvalidateCachedOuterScopes(funcInfo);
        }
    }

    if (funcInfo->frameDisplayRegister != Js::Constants::NoRegister)
    {TRACE_IT(38816);
        m_writer.RecordFrameDisplayRegister(funcInfo->frameDisplayRegister);
    }

    // new.target may be used to construct the 'this' register so make sure to load it first
    if (funcInfo->newTargetRegister != Js::Constants::NoRegister)
    {TRACE_IT(38817);
        this->LoadNewTargetObject(funcInfo);
    }

    if (funcInfo->thisPointerRegister != Js::Constants::NoRegister)
    {TRACE_IT(38818);
        this->LoadThisObject(funcInfo, thisLoadedFromParams);
    }

    this->RecordAllIntConstants(funcInfo);
    this->RecordAllStrConstants(funcInfo);
    this->RecordAllStringTemplateCallsiteConstants(funcInfo);

    funcInfo->doubleConstantToRegister.Map([byteCodeFunction](double d, Js::RegSlot location)
    {
        byteCodeFunction->RecordFloatConstant(byteCodeFunction->MapRegSlot(location), d);
    });

    if (funcInfo->GetHasArguments())
    {TRACE_IT(38819);
        sym = funcInfo->GetArgumentsSymbol();
        Assert(sym);
        Assert(funcInfo->GetHasHeapArguments());

        if (funcInfo->GetCallsEval() || (!funcInfo->GetApplyEnclosesArgs()))
        {TRACE_IT(38820);
            this->LoadHeapArguments(funcInfo);
        }

    }
    else if (!funcInfo->IsGlobalFunction() && !IsInNonDebugMode())
    {TRACE_IT(38821);
        uint count = funcInfo->inArgsCount + (funcInfo->root->sxFnc.pnodeRest != nullptr ? 1 : 0) - 1;
        if (count != 0)
        {TRACE_IT(38822);
            Js::PropertyIdArray *propIds = RecyclerNewPlus(scriptContext->GetRecycler(), count * sizeof(Js::PropertyId), Js::PropertyIdArray, count, 0);

            GetFormalArgsArray(this, funcInfo, propIds);
            byteCodeFunction->SetPropertyIdsOfFormals(propIds);
        }
    }

    //
    // If the function is a function expression with a name,
    // load the function object at runtime to its activation object.
    //
    sym = funcInfo->root->sxFnc.GetFuncSymbol();
    bool funcExprWithName = !funcInfo->IsGlobalFunction() && sym && sym->GetIsFuncExpr();

    if (funcExprWithName)
    {TRACE_IT(38823);
        if (funcInfo->GetFuncExprNameReference() ||
            (funcInfo->funcExprScope && funcInfo->funcExprScope->GetIsObject()))
        {TRACE_IT(38824);
            //
            // x = function f(...) { ... }
            // A named function expression's name (Symbol:f) belongs to the enclosing scope.
            // Thus there are no uses of 'f' within the scope of the function (as references to 'f'
            // are looked up in the closure). So, we can't use f's register as it is from the enclosing
            // scope's register namespace. So use a tmp register.
            // In ES5 mode though 'f' is *not* a part of the enclosing scope. So we always assign 'f' a register
            // from it's register namespace, which LdFuncExpr can use.
            //
            Js::RegSlot ldFuncExprDst = sym->GetLocation();
            this->m_writer.Reg1(Js::OpCode::LdFuncExpr, ldFuncExprDst);

            if (sym->IsInSlot(funcInfo))
            {TRACE_IT(38825);
                Js::RegSlot scopeLocation;
                AnalysisAssert(funcInfo->funcExprScope);

                if (funcInfo->funcExprScope->GetIsObject())
                {TRACE_IT(38826);
                    scopeLocation = funcInfo->funcExprScope->GetLocation();
                    this->m_writer.Property(Js::OpCode::StFuncExpr, sym->GetLocation(), scopeLocation,
                        funcInfo->FindOrAddReferencedPropertyId(sym->GetPosition()));
                }
                else if (funcInfo->bodyScope->GetIsObject())
                {TRACE_IT(38827);
                    this->m_writer.ElementU(Js::OpCode::StLocalFuncExpr, sym->GetLocation(),
                        funcInfo->FindOrAddReferencedPropertyId(sym->GetPosition()));
                }
                else
                {TRACE_IT(38828);
                    Assert(sym->HasScopeSlot());
                    this->m_writer.SlotI1(Js::OpCode::StLocalSlot, sym->GetLocation(),
                                          sym->GetScopeSlot() + Js::ScopeSlots::FirstSlotIndex);
                }
            }
            else if (ShouldTrackDebuggerMetadata())
            {TRACE_IT(38829);
                funcInfo->byteCodeFunction->GetFunctionBody()->InsertSymbolToRegSlotList(sym->GetName(), sym->GetLocation(), funcInfo->varRegsCount);
            }
        }
    }
}

void ByteCodeGenerator::InvalidateCachedOuterScopes(FuncInfo *funcInfo)
{TRACE_IT(38830);
    Assert(funcInfo->GetEnvRegister() != Js::Constants::NoRegister);

    // Walk the scope stack, from funcInfo outward, looking for scopes that have been cached.

    Scope *scope = funcInfo->GetBodyScope()->GetEnclosingScope();
    uint32 envIndex = 0;

    while (scope && scope->GetFunc() == funcInfo)
    {TRACE_IT(38831);
        // Skip over FuncExpr Scope and parameter scope for current funcInfo to get to the first enclosing scope of the outer function.
        scope = scope->GetEnclosingScope();
    }

    for (; scope; scope = scope->GetEnclosingScope())
    {TRACE_IT(38832);
        FuncInfo *func = scope->GetFunc();
        if (scope == func->GetBodyScope())
        {TRACE_IT(38833);
            if (func->Escapes() && func->GetHasCachedScope())
            {TRACE_IT(38834);
                Assert(scope->GetIsObject());
                this->m_writer.Unsigned1(Js::OpCode::InvalCachedScope, envIndex);
            }
        }
        if (scope->GetMustInstantiate())
        {TRACE_IT(38835);
            envIndex++;
        }
    }
}

void ByteCodeGenerator::LoadThisObject(FuncInfo *funcInfo, bool thisLoadedFromParams)
{TRACE_IT(38836);
    if (this->scriptContext->GetConfig()->IsES6ClassAndExtendsEnabled() && funcInfo->IsClassConstructor())
    {TRACE_IT(38837);
        // Derived class constructors initialize 'this' to be Undecl except "extends null" cases
        //   - we'll check this value during a super call and during 'this' access
        //
        // Base class constructors or "extends null" cases initialize 'this' to a new object using new.target
        if (funcInfo->IsBaseClassConstructor())
        {TRACE_IT(38838);
            EmitBaseClassConstructorThisObject(funcInfo);
        }
        else
        {TRACE_IT(38839);
            Js::ByteCodeLabel thisLabel = this->Writer()->DefineLabel();
            Js::ByteCodeLabel skipLabel = this->Writer()->DefineLabel();

            Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
            this->Writer()->Reg1(Js::OpCode::LdFuncObj, tmpReg);
            this->Writer()->BrReg1(Js::OpCode::BrOnBaseConstructorKind, thisLabel, tmpReg);  // branch when [[ConstructorKind]]=="base"
            funcInfo->ReleaseTmpRegister(tmpReg);

            this->m_writer.Reg1(Js::OpCode::InitUndecl, funcInfo->thisPointerRegister);  // not "extends null" case
            this->Writer()->Br(Js::OpCode::Br, skipLabel);

            this->Writer()->MarkLabel(thisLabel);
            EmitBaseClassConstructorThisObject(funcInfo);  // "extends null" case

            this->Writer()->MarkLabel(skipLabel);
        }
    }
    else if (!funcInfo->IsGlobalFunction() || (this->flags & fscrEval))
    {TRACE_IT(38840);
        //
        // thisLoadedFromParams would be true for the event Handler case,
        // "this" would have been loaded from parameters to put in the environment
        //
        if (!thisLoadedFromParams && !funcInfo->IsLambda())
        {TRACE_IT(38841);
            m_writer.ArgIn0(funcInfo->thisPointerRegister);
        }
        if (!(this->flags & fscrEval) || !funcInfo->IsGlobalFunction())
        {
            // we don't want to emit 'this' for eval, because 'this' value in eval is equal to 'this' value of caller
            // and does not depend on "use strict" inside of eval.
            // so we pass 'this' directly in GlobalObject::EntryEval()
            EmitThis(funcInfo, funcInfo->thisPointerRegister);
        }
    }
    else
    {TRACE_IT(38842);
        Assert(funcInfo->IsGlobalFunction());
        Js::RegSlot root = funcInfo->nullConstantRegister;
        EmitThis(funcInfo, root);
    }
}

void ByteCodeGenerator::LoadNewTargetObject(FuncInfo *funcInfo)
{TRACE_IT(38843);
    if (funcInfo->IsClassConstructor())
    {TRACE_IT(38844);
        Assert(!funcInfo->IsLambda());

        m_writer.ArgIn0(funcInfo->newTargetRegister);
    }
    else if (funcInfo->IsLambda() && !(this->flags & fscrEval))
    {TRACE_IT(38845);
        Scope *scope;
        Js::PropertyId envIndex = -1;
        GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);

        if (scope->GetFunc()->IsGlobalFunction())
        {TRACE_IT(38846);
            m_writer.Reg1(Js::OpCode::LdUndef, funcInfo->newTargetRegister);
        }
        else
        {TRACE_IT(38847);
            Js::PropertyId slot = scope->GetFunc()->newTargetScopeSlot;
            EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, slot, funcInfo->newTargetRegister);
        }
    }
    else if ((funcInfo->IsGlobalFunction() || funcInfo->IsLambda()) && (this->flags & fscrEval))
    {TRACE_IT(38848);
        Js::RegSlot scopeLocation;

        if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
        {TRACE_IT(38849);
            scopeLocation = funcInfo->frameDisplayRegister;
        }
        else if (funcInfo->NeedEnvRegister())
        {TRACE_IT(38850);
            scopeLocation = funcInfo->GetEnvRegister();
        }
        else
        {TRACE_IT(38851);
            // If this eval doesn't have environment register or frame display register, we didn't capture anything from a class constructor.
            m_writer.Reg1(Js::OpCode::LdNewTarget, funcInfo->newTargetRegister);
            return;
        }

        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, Js::PropertyIds::_lexicalNewTargetSymbol, false, false);
        this->m_writer.ElementP(Js::OpCode::ScopedLdFld, funcInfo->newTargetRegister, cacheId);
    }
    else if (funcInfo->IsGlobalFunction())
    {TRACE_IT(38852);
        m_writer.Reg1(Js::OpCode::LdUndef, funcInfo->newTargetRegister);
    }
    else
    {TRACE_IT(38853);
        m_writer.Reg1(Js::OpCode::LdNewTarget, funcInfo->newTargetRegister);
    }
}

void ByteCodeGenerator::EmitScopeSlotLoadThis(FuncInfo *funcInfo, Js::RegSlot regLoc, bool chkUndecl)
{TRACE_IT(38854);
    FuncInfo* nonLambdaFunc = funcInfo;
    if (funcInfo->IsLambda())
    {TRACE_IT(38855);
        nonLambdaFunc = FindEnclosingNonLambda();
    }

    if (nonLambdaFunc->IsClassConstructor() && !nonLambdaFunc->IsBaseClassConstructor())
    {TRACE_IT(38856);
        // If we are in a derived class constructor and we have a scope slot for 'this',
        // we need to load 'this' from the scope slot. This is to support the case where
        // the call to initialize 'this' via super() is inside a lambda since the lambda
        // can't assign to the 'this' register of the parent constructor.
        if (nonLambdaFunc->thisScopeSlot != Js::Constants::NoRegister)
        {TRACE_IT(38857);
            Js::PropertyId slot = nonLambdaFunc->thisScopeSlot;

            EmitInternalScopedSlotLoad(funcInfo, slot, regLoc, chkUndecl);
        }
        else if (funcInfo->thisPointerRegister != Js::Constants::NoRegister && chkUndecl)
        {TRACE_IT(38858);
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, funcInfo->thisPointerRegister);
        }
        else if (chkUndecl)
        {
            // If we don't have a scope slot for 'this' we know that super could not have
            // been called inside a lambda so we can check to see if we called
            // super and assigned to the this register already. If not, this should trigger
            // a ReferenceError.
            EmitUseBeforeDeclarationRuntimeError(this, Js::Constants::NoRegister);
        }
    }
    else if (this->flags & fscrEval && (funcInfo->IsGlobalFunction() || (funcInfo->IsLambda() && nonLambdaFunc->IsGlobalFunction()))
        && funcInfo->GetBodyScope()->GetIsObject())
    {TRACE_IT(38859);
        Js::RegSlot scopeLocation;

        if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
        {TRACE_IT(38860);
            scopeLocation = funcInfo->frameDisplayRegister;
        }
        else if (funcInfo->NeedEnvRegister())
        {TRACE_IT(38861);
            scopeLocation = funcInfo->GetEnvRegister();
        }
        else
        {TRACE_IT(38862);
            // If this eval doesn't have environment register or frame display register, we didn't capture anything from a class constructor
            return;
        }

        // CONSIDER [tawoll] - Should we add a ByteCodeGenerator flag (fscrEvalWithClassConstructorParent) and avoid doing this runtime check?
        Js::ByteCodeLabel skipLabel = this->Writer()->DefineLabel();
        this->Writer()->BrReg1(Js::OpCode::BrNotUndecl_A, skipLabel, funcInfo->thisPointerRegister);

        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, Js::PropertyIds::_lexicalThisSlotSymbol, false, false);
        this->m_writer.ElementP(Js::OpCode::ScopedLdFld, funcInfo->thisPointerRegister, cacheId);
        if (chkUndecl)
        {TRACE_IT(38863);
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, funcInfo->thisPointerRegister);
        }

        this->Writer()->MarkLabel(skipLabel);
    }
}

void ByteCodeGenerator::EmitScopeSlotStoreThis(FuncInfo *funcInfo, Js::RegSlot regLoc, bool chkUndecl)
{TRACE_IT(38864);
    if (this->flags & fscrEval && (funcInfo->IsGlobalFunction() || (funcInfo->IsLambda() && FindEnclosingNonLambda()->IsGlobalFunction())))
    {TRACE_IT(38865);
        Js::RegSlot scopeLocation;

        if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
        {TRACE_IT(38866);
            scopeLocation = funcInfo->frameDisplayRegister;
        }
        else
        {TRACE_IT(38867);
            scopeLocation = funcInfo->GetEnvRegister();
        }

        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, Js::PropertyIds::_lexicalThisSlotSymbol, false, true);
        this->m_writer.ElementP(GetScopedStFldOpCode(funcInfo->byteCodeFunction->GetIsStrictMode()), funcInfo->thisPointerRegister, cacheId);
    }
    else if (regLoc != Js::Constants::NoRegister)
    {
        EmitInternalScopedSlotStore(funcInfo, regLoc, funcInfo->thisPointerRegister);
    }
}

void ByteCodeGenerator::EmitSuperCall(FuncInfo* funcInfo, ParseNode* pnode, BOOL fReturnValue)
{TRACE_IT(38868);
    Assert(pnode->sxCall.pnodeTarget->nop == knopSuper);

    FuncInfo* nonLambdaFunc = funcInfo;

    if (funcInfo->IsLambda())
    {TRACE_IT(38869);
        nonLambdaFunc = this->FindEnclosingNonLambda();
    }

    if (nonLambdaFunc->IsBaseClassConstructor())
    {TRACE_IT(38870);
        // super() is not allowed in base class constructors. If we detect this, emit a ReferenceError and skip making the call.
        this->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_ClassSuperInBaseClass));
        return;
    }
    else
    {
        EmitSuperFieldPatch(funcInfo, pnode, this);
        pnode->isUsed = true;
    }

    // We already know pnode->sxCall.pnodeTarget->nop is super but we can't use the super register in case
    // this is an eval and we will load super dynamically from the scope using ScopedLdHomeObj.
    // That means we'll have to rely on the location of the call target to be sure.
    // We have to make sure to allocate the location for the node now, before we try to branch on it.
    Emit(pnode->sxCall.pnodeTarget, this, funcInfo, false, /*isConstructorCall*/ true); // reuse isConstructorCall ("new super()" is illegal)

    //
    // if (super is class constructor) {
    //   _this = new.target;
    // } else {
    //   _this = NewScObjFull(new.target);
    // }
    //
    // temp = super.call(_this, new.target); // CallFlag_New | CallFlag_NewTarget | CallFlag_ExtraArg
    // if (temp is object) {
    //   _this = temp;
    // }
    //
    // if (UndeclBlockVar === this) {
    //   this = _this;
    // } else {
    //   throw ReferenceError;
    // }
    //
    funcInfo->AcquireLoc(pnode);
    Js::RegSlot thisForSuperCall = funcInfo->AcquireTmpRegister();
    Js::ByteCodeLabel useNewTargetForThisLabel = this->Writer()->DefineLabel();
    Js::ByteCodeLabel makeCallLabel = this->Writer()->DefineLabel();
    Js::ByteCodeLabel useSuperCallResultLabel = this->Writer()->DefineLabel();
    Js::ByteCodeLabel doneLabel = this->Writer()->DefineLabel();

    Js::RegSlot tmpReg = this->EmitLdObjProto(Js::OpCode::LdFuncObjProto, pnode->sxCall.pnodeTarget->location, funcInfo);
    this->Writer()->BrReg1(Js::OpCode::BrOnClassConstructor, useNewTargetForThisLabel, tmpReg);

    this->Writer()->Reg2(Js::OpCode::NewScObjectNoCtorFull, thisForSuperCall, funcInfo->newTargetRegister);
    this->Writer()->Br(Js::OpCode::Br, makeCallLabel);

    this->Writer()->MarkLabel(useNewTargetForThisLabel);
    this->Writer()->Reg2(Js::OpCode::Ld_A, thisForSuperCall, funcInfo->newTargetRegister);

    this->Writer()->MarkLabel(makeCallLabel);
    EmitCall(pnode, Js::Constants::NoRegister, this, funcInfo, fReturnValue, /*fEvaluateComponents*/ true, /*fHasNewTarget*/ true, thisForSuperCall);

    // We have to use another temp for the this value before assigning to this register.
    // This is because IRBuilder does not expect us to use the value of a temp after potentially assigning to that same temp.
    // Ex:
    // _this = new.target;
    // temp = super.call(_this);
    // if (temp is object) {
    //   _this = temp; // creates a new sym for _this as it was previously used
    // }
    // this = _this; // tries to loads a value from the old sym (which is dead)
    Js::RegSlot valueForThis = funcInfo->AcquireTmpRegister();

    this->Writer()->BrReg1(Js::OpCode::BrOnObject_A, useSuperCallResultLabel, pnode->location);
    this->Writer()->Reg2(Js::OpCode::Ld_A, valueForThis, thisForSuperCall);
    this->Writer()->Br(Js::OpCode::Br, doneLabel);
    this->Writer()->MarkLabel(useSuperCallResultLabel);
    this->Writer()->Reg2(Js::OpCode::Ld_A, valueForThis, pnode->location);
    this->Writer()->MarkLabel(doneLabel);

    // The call is done and we know what we will bind to 'this' so let's check to see if 'this' is already decl.
    // We may need to load 'this' from the scope slot.
    EmitScopeSlotLoadThis(funcInfo, funcInfo->thisPointerRegister, false);

    Js::ByteCodeLabel skipLabel = this->Writer()->DefineLabel();
    Js::RegSlot tmpUndeclReg = funcInfo->AcquireTmpRegister();
    this->Writer()->Reg1(Js::OpCode::InitUndecl, tmpUndeclReg);
    this->Writer()->BrReg2(Js::OpCode::BrSrEq_A, skipLabel, funcInfo->thisPointerRegister, tmpUndeclReg);
    funcInfo->ReleaseTmpRegister(tmpUndeclReg);

    this->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_ClassThisAlreadyAssigned));
    this->Writer()->MarkLabel(skipLabel);

    this->Writer()->Reg2(Js::OpCode::StrictLdThis, funcInfo->thisPointerRegister, valueForThis);
    funcInfo->ReleaseTmpRegister(valueForThis);
    funcInfo->ReleaseTmpRegister(thisForSuperCall);

    // We already assigned the result of super() to the 'this' register but we need to store it in the scope slot, too. If there is one.
    this->EmitScopeSlotStoreThis(funcInfo, nonLambdaFunc->thisScopeSlot);
}

void ByteCodeGenerator::EmitClassConstructorEndCode(FuncInfo *funcInfo)
{TRACE_IT(38871);
    if (funcInfo->thisPointerRegister != Js::Constants::NoRegister)
    {
        // We need to try and load 'this' from the scope slot, if there is one.
        EmitScopeSlotLoadThis(funcInfo, funcInfo->thisPointerRegister);
        this->Writer()->Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, funcInfo->thisPointerRegister);
    }
}

void ByteCodeGenerator::EmitBaseClassConstructorThisObject(FuncInfo *funcInfo)
{TRACE_IT(38872);
    this->Writer()->Reg2(Js::OpCode::NewScObjectNoCtorFull, funcInfo->thisPointerRegister, funcInfo->newTargetRegister);
}

void ByteCodeGenerator::EmitInternalScopedSlotLoad(FuncInfo *funcInfo, Js::RegSlot slot, Js::RegSlot symbolRegister, bool chkUndecl)
{TRACE_IT(38873);
    Scope* scope = nullptr;

    if (funcInfo->IsLambda())
    {TRACE_IT(38874);
        Js::PropertyId envIndex = -1;
        GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);

        EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, slot, symbolRegister, chkUndecl);
    }
    else
    {TRACE_IT(38875);
        scope = funcInfo->IsBodyAndParamScopeMerged() ? funcInfo->GetBodyScope() : funcInfo->GetParamScope();

        EmitInternalScopedSlotLoad(funcInfo, scope, -1, slot, symbolRegister, chkUndecl);
    }
}

void ByteCodeGenerator::EmitInternalScopedSlotLoad(FuncInfo *funcInfo, Scope *scope, Js::PropertyId envIndex, Js::RegSlot slot, Js::RegSlot symbolRegister, bool chkUndecl)
{TRACE_IT(38876);
    Assert(slot != Js::Constants::NoProperty);
    Js::ProfileId profileId = funcInfo->FindOrAddSlotProfileId(scope, symbolRegister);
    Js::OpCode opcode;

    // This method is used only for loading the special symbols. If this is already param scope then we don't have to do anything.
    // Else, for split scope, we have to get the special symbols from the param scope.
    if (scope->GetScopeType() == ScopeType_Parameter)
    {TRACE_IT(38877);
        Assert(!scope->GetFunc()->IsBodyAndParamScopeMerged());
    }
    else if (!scope->GetFunc()->IsBodyAndParamScopeMerged())
    {TRACE_IT(38878);
        scope = scope->GetFunc()->GetParamScope();
    }

    Js::RegSlot scopeLocation = scope->GetLocation();
    opcode = this->GetLdSlotOp(scope, envIndex, scopeLocation, funcInfo);
    slot += (scope->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

    if (envIndex != -1)
    {TRACE_IT(38879);
        this->m_writer.SlotI2(opcode, symbolRegister, envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var), slot, profileId);
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
        (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
    {TRACE_IT(38880);
        this->m_writer.SlotI1(opcode, symbolRegister, slot, profileId);
    }
    else
    {TRACE_IT(38881);
        this->m_writer.Slot(opcode, symbolRegister, scopeLocation, slot, profileId);
    }

    if (chkUndecl)
    {TRACE_IT(38882);
        this->m_writer.Reg1(Js::OpCode::ChkUndecl, symbolRegister);
    }
}

void ByteCodeGenerator::EmitInternalScopedSlotStore(FuncInfo *funcInfo, Js::RegSlot slot, Js::RegSlot symbolRegister)
{TRACE_IT(38883);
    Assert(slot != Js::Constants::NoProperty);

    Scope* scope = nullptr;
    Js::OpCode opcode;

    Js::PropertyId envIndex = -1;
    if (funcInfo->IsLambda())
    {
        GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);
    }
    else
    {TRACE_IT(38884);
        scope = funcInfo->GetBodyScope();
    }

    // This method is used only for storing values to the special symbols. If this is already param scope then we don't have to do anything.
    // Else, for split scope, we have to get the special symbols from the param scope.
    if (scope->GetScopeType() == ScopeType_Parameter)
    {TRACE_IT(38885);
        Assert(!scope->GetFunc()->IsBodyAndParamScopeMerged());
    }
    else if (!scope->GetFunc()->IsBodyAndParamScopeMerged())
    {TRACE_IT(38886);
        scope = scope->GetFunc()->GetParamScope();
    }

    Js::RegSlot scopeLocation = scope->GetLocation();
    opcode = this->GetStSlotOp(scope, envIndex, scopeLocation, false, funcInfo);
    slot += (scope->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

    if (envIndex != -1)
    {TRACE_IT(38887);
        this->m_writer.SlotI2(opcode, symbolRegister, envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var), slot);
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
        (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
    {TRACE_IT(38888);
        this->m_writer.SlotI1(opcode, symbolRegister, slot);
    }
    else if (scope->GetIsObject())
    {TRACE_IT(38889);
        this->m_writer.Slot(opcode, symbolRegister, scopeLocation, slot);
    }
    else
    {TRACE_IT(38890);
        this->m_writer.SlotI2(opcode, symbolRegister, scope->GetInnerScopeIndex(), slot);
    }
}

void ByteCodeGenerator::EmitInternalScopeObjInit(FuncInfo *funcInfo, Scope *scope, Js::RegSlot valueLocation, Js::PropertyId propertyId)
{TRACE_IT(38891);
    Js::RegSlot scopeLocation = scope->GetLocation();
    Js::OpCode opcode = this->GetInitFldOp(scope, scopeLocation, funcInfo);
    if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
    {TRACE_IT(38892);
        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true);
        this->m_writer.ElementP(opcode, valueLocation, cacheId);
    }
    else if (scope->HasInnerScopeIndex())
    {TRACE_IT(38893);
        uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->InnerScopeToRegSlot(scope), propertyId, false, true);
        this->m_writer.ElementPIndexed(opcode, valueLocation, scope->GetInnerScopeIndex(), cacheId);
    }
    else
    {TRACE_IT(38894);
        uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true);
        this->m_writer.PatchableProperty(opcode, valueLocation, scopeLocation, cacheId);
    }
}

void ByteCodeGenerator::GetEnclosingNonLambdaScope(FuncInfo *funcInfo, Scope * &scope, Js::PropertyId &envIndex)
{TRACE_IT(38895);
    Assert(funcInfo->IsLambda());
    envIndex = -1;
    for (scope = GetCurrentScope(); scope; scope = scope->GetEnclosingScope())
    {TRACE_IT(38896);
        FuncInfo* scopeFuncInfo = scope->GetFunc();
        if (scope->GetMustInstantiate() && scopeFuncInfo != funcInfo)
        {TRACE_IT(38897);
            envIndex++;
        }
        if (scope->IsGlobalEvalBlockScope())
        {TRACE_IT(38898);
            break;
        }
        else if (!scopeFuncInfo->IsLambda())
        {TRACE_IT(38899);
            // This method is used only for working with the special scope slots like this, super or new.target captured
            // from the parent function. In case of split scope all these symbols should be loaded from the param scope.
            if (scope == scopeFuncInfo->GetParamScope())
            {TRACE_IT(38900);
                Assert(!scopeFuncInfo->IsBodyAndParamScopeMerged());
                break;
            }
            else if (scope == scopeFuncInfo->GetBodyScope() && scopeFuncInfo->IsBodyAndParamScopeMerged())
            {TRACE_IT(38901);
                break;
            }
        }
    }
}

void ByteCodeGenerator::EmitThis(FuncInfo *funcInfo, Js::RegSlot fromRegister)
{TRACE_IT(38902);
    if (funcInfo->IsLambda())
    {TRACE_IT(38903);
        Scope *scope;
        Js::PropertyId envIndex = -1;
        GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);
        FuncInfo* parent = scope->GetFunc();

        if (parent->IsGlobalFunction())
        {TRACE_IT(38904);
            if (this->flags & fscrEval)
            {TRACE_IT(38905);
                scope = parent->GetGlobalEvalBlockScope();
                Js::PropertyId slot = parent->thisScopeSlot;
                EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, slot, funcInfo->thisPointerRegister, false);
            }
            else
            {TRACE_IT(38906);
                // Always load global object via LdThis of null to get the possibly protected via secureHostObject global object.
                this->m_writer.Reg2Int1(Js::OpCode::LdThis, funcInfo->thisPointerRegister, funcInfo->nullConstantRegister, this->GetModuleID());
            }
        }
        else if (!parent->IsClassConstructor() || parent->IsBaseClassConstructor())
        {TRACE_IT(38907);
            // In a lambda inside a derived class constructor, 'this' should be loaded from the scope slot whenever 'this' is accessed.
            // It's safe to load 'this' into the register for base class constructors because there is no complex assignment to 'this'
            // via super call chain.
            Js::PropertyId slot = parent->thisScopeSlot;
            EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, slot, funcInfo->thisPointerRegister, false);
        }
    }
    else if (funcInfo->byteCodeFunction->GetIsStrictMode() && (!funcInfo->IsGlobalFunction() || this->flags & fscrEval))
    {TRACE_IT(38908);
        m_writer.Reg2(Js::OpCode::StrictLdThis, funcInfo->thisPointerRegister, fromRegister);
    }
    else
    {TRACE_IT(38909);
        m_writer.Reg2Int1(Js::OpCode::LdThis, funcInfo->thisPointerRegister, fromRegister, this->GetModuleID());
    }
}

void ByteCodeGenerator::EmitLoadFormalIntoRegister(ParseNode *pnodeFormal, Js::RegSlot pos, FuncInfo *funcInfo)
{TRACE_IT(38910);
    if (pnodeFormal->IsVarLetOrConst())
    {TRACE_IT(38911);
        // Get the param from its argument position into its assigned register.
        // The position should match the location, otherwise, it has been shadowed by parameter with the same name
        Symbol *formal = pnodeFormal->sxVar.sym;
        if (formal->GetLocation() + 1 == pos)
        {TRACE_IT(38912);
            // Transfer to the frame object, etc., if necessary.
            this->EmitLocalPropInit(formal->GetLocation(), formal, funcInfo);
        }
    }
}

void ByteCodeGenerator::HomeArguments(FuncInfo *funcInfo)
{TRACE_IT(38913);
    if (ShouldTrackDebuggerMetadata())
    {TRACE_IT(38914);
        // Add formals to the debugger propertyidcontainer for reg slots
        auto addFormalsToPropertyIdContainer = [funcInfo](ParseNode *pnodeFormal)
        {TRACE_IT(38915);
            if (pnodeFormal->IsVarLetOrConst())
            {TRACE_IT(38916);
                Symbol* formal = pnodeFormal->sxVar.sym;
                if (!formal->IsInSlot(funcInfo))
                {TRACE_IT(38917);
                    Assert(!formal->GetHasInit());
                    funcInfo->GetParsedFunctionBody()->InsertSymbolToRegSlotList(formal->GetName(), formal->GetLocation(), funcInfo->varRegsCount);
                }
            }
        };

        MapFormals(funcInfo->root, addFormalsToPropertyIdContainer);
    }

    // Transfer formal parameters to their home locations on the local frame.
    if (funcInfo->GetHasArguments())
    {TRACE_IT(38918);
        if (funcInfo->root->sxFnc.pnodeRest != nullptr)
        {TRACE_IT(38919);
            // Since we don't have to iterate over arguments here, we'll trust the location to be correct.
            EmitLoadFormalIntoRegister(funcInfo->root->sxFnc.pnodeRest, funcInfo->root->sxFnc.pnodeRest->sxVar.sym->GetLocation() + 1, funcInfo);
        }

        // The arguments object creation helper does this work for us.
        return;
    }

    Js::ArgSlot pos = 1;
    auto loadFormal = [&](ParseNode *pnodeFormal)
    {
        EmitLoadFormalIntoRegister(pnodeFormal, pos, funcInfo);
        pos++;
    };
    MapFormals(funcInfo->root, loadFormal);
}

void ByteCodeGenerator::DefineLabels(FuncInfo *funcInfo)
{TRACE_IT(38920);
    funcInfo->singleExit = m_writer.DefineLabel();
    SList<ParseNode *>::Iterator iter(&funcInfo->targetStatements);
    while (iter.Next())
    {TRACE_IT(38921);
        ParseNode * node = iter.Data();
        node->sxStmt.breakLabel = m_writer.DefineLabel();
        node->sxStmt.continueLabel = m_writer.DefineLabel();
        node->emitLabels = true;
    }
}

void ByteCodeGenerator::EmitGlobalBody(FuncInfo *funcInfo)
{TRACE_IT(38922);
    // Emit global code (global scope or eval), fixing up the return register with the implicit
    // return value.
    ParseNode *pnode = funcInfo->root->sxFnc.pnodeBody;
    ParseNode *pnodeLastVal = funcInfo->root->sxProg.pnodeLastValStmt;
    if (pnodeLastVal == nullptr)
    {TRACE_IT(38923);
        // We're not guaranteed to compute any values, so fix up the return register at the top
        // in case.
        this->m_writer.Reg1(Js::OpCode::LdUndef, ReturnRegister);
    }

    while (pnode->nop == knopList)
    {TRACE_IT(38924);
        ParseNode *stmt = pnode->sxBin.pnode1;
        if (stmt == pnodeLastVal)
        {TRACE_IT(38925);
            pnodeLastVal = nullptr;
        }
        if (pnodeLastVal == nullptr && (this->flags & fscrReturnExpression))
        {
            EmitTopLevelStatement(stmt, funcInfo, true);
        }
        else
        {
            // Haven't hit the post-dominating return value yet,
            // so don't bother with the return register.
            EmitTopLevelStatement(stmt, funcInfo, false);
        }
        pnode = pnode->sxBin.pnode2;
    }
    EmitTopLevelStatement(pnode, funcInfo, false);
}

void ByteCodeGenerator::EmitFunctionBody(FuncInfo *funcInfo)
{TRACE_IT(38926);
    // Emit a function body. Only explicit returns and the implicit "undef" at the bottom
    // get copied to the return register.
    ParseNode *pnodeBody = funcInfo->root->sxFnc.pnodeBody;
    ParseNode *pnode = pnodeBody;
    while (pnode->nop == knopList)
    {TRACE_IT(38927);
        ParseNode *stmt = pnode->sxBin.pnode1;
        if (stmt->CapturesSyms())
        {TRACE_IT(38928);
            CapturedSymMap *map = funcInfo->EnsureCapturedSymMap();
            SList<Symbol*> *list = map->Item(stmt);
            FOREACH_SLIST_ENTRY(Symbol*, sym, list)
            {TRACE_IT(38929);
                if (!sym->GetIsCommittedToSlot())
                {TRACE_IT(38930);
                    Assert(sym->GetLocation() != Js::Constants::NoProperty);
                    sym->SetIsCommittedToSlot();
                    ParseNode *decl = sym->GetDecl();
                    Assert(decl);
                    if (PHASE_TRACE(Js::DelayCapturePhase, funcInfo->byteCodeFunction))
                    {TRACE_IT(38931);
                        Output::Print(_u("--- DelayCapture: Committed symbol '%s' to slot.\n"),
                            sym->GetName().GetBuffer());
                        Output::Flush();
                    }
                    // REVIEW[ianhall]: HACK to work around this causing an error due to sym not yet being initialized
                    // what is this doing? Why are we assigning sym to itself?
                    bool old = sym->GetNeedDeclaration();
                    sym->SetNeedDeclaration(false);
                    this->EmitPropStore(sym->GetLocation(), sym, sym->GetPid(), funcInfo, decl->nop == knopLetDecl, decl->nop == knopConstDecl);
                    sym->SetNeedDeclaration(old);
                }
            }
            NEXT_SLIST_ENTRY;
        }
        EmitTopLevelStatement(stmt, funcInfo, false);
        pnode = pnode->sxBin.pnode2;
    }
    Assert(!pnode->CapturesSyms());
    EmitTopLevelStatement(pnode, funcInfo, false);
}

void ByteCodeGenerator::EmitProgram(ParseNode *pnodeProg)
{TRACE_IT(38932);
    // Indicate that the binding phase is over.
    this->isBinding = false;
    this->trackEnvDepth = true;
    AssignPropertyIds(pnodeProg->sxFnc.funcInfo->byteCodeFunction);

    int32 initSize = this->maxAstSize / AstBytecodeRatioEstimate;

    // Use the temp allocator in bytecode write temp buffer.
    m_writer.InitData(this->alloc, initSize);

#ifdef LOG_BYTECODE_AST_RATIO
    // log the max Ast size
    Output::Print(_u("Max Ast size: %d"), initSize);
#endif

    Assert(pnodeProg && pnodeProg->nop == knopProg);

    if (this->parentScopeInfo)
    {TRACE_IT(38933);
        // Scope stack is already set up the way we want it, so don't visit the global scope.
        // Start emitting with the nested scope (i.e., the deferred function).
        this->EmitScopeList(pnodeProg->sxProg.pnodeScopes);
    }
    else
    {TRACE_IT(38934);
        this->EmitScopeList(pnodeProg);
    }
}

void ByteCodeGenerator::EmitInitCapturedThis(FuncInfo* funcInfo, Scope* scope)
{TRACE_IT(38935);
    if (scope->GetIsObject())
    {TRACE_IT(38936);
        // Ensure space for the this slot
        this->EmitInternalScopeObjInit(funcInfo, scope, funcInfo->thisPointerRegister, Js::PropertyIds::_lexicalThisSlotSymbol);
    }
    else
    {TRACE_IT(38937);
        this->EmitInternalScopedSlotStore(funcInfo, funcInfo->thisScopeSlot, funcInfo->thisPointerRegister);
    }
}

void ByteCodeGenerator::EmitInitCapturedNewTarget(FuncInfo* funcInfo, Scope* scope)
{TRACE_IT(38938);
    if (scope->GetIsObject())
    {TRACE_IT(38939);
        // Ensure space for the new.target slot
        this->EmitInternalScopeObjInit(funcInfo, scope, funcInfo->newTargetRegister, Js::PropertyIds::_lexicalNewTargetSymbol);
    }
    else
    {TRACE_IT(38940);
        this->EmitInternalScopedSlotStore(funcInfo, funcInfo->newTargetScopeSlot, funcInfo->newTargetRegister);
    }
}

void EmitDestructuredObject(ParseNode *lhs, Js::RegSlot rhsLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);
void EmitDestructuredValueOrInitializer(ParseNodePtr lhsElementNode, Js::RegSlot rhsLocation, ParseNodePtr initializer, bool isNonPatternAssignmentTarget, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo);

void ByteCodeGenerator::PopulateFormalsScope(uint beginOffset, FuncInfo *funcInfo, ParseNode *pnode)
{TRACE_IT(38941);
    Js::DebuggerScope *debuggerScope = nullptr;
    auto processArg = [&](ParseNode *pnodeArg) {TRACE_IT(38942);
        if (pnodeArg->IsVarLetOrConst())
        {TRACE_IT(38943);
            if (debuggerScope == nullptr)
            {TRACE_IT(38944);
                debuggerScope = RecordStartScopeObject(pnode, funcInfo->paramScope && funcInfo->paramScope->GetIsObject() ? Js::DiagParamScopeInObject : Js::DiagParamScope);
                debuggerScope->SetBegin(beginOffset);
            }

            InsertPropertyToDebuggerScope(funcInfo, debuggerScope, pnodeArg->sxVar.sym);
        }
    };

    MapFormals(pnode, processArg);
    MapFormalsFromPattern(pnode, processArg);

    if (debuggerScope != nullptr)
    {TRACE_IT(38945);
        if (!funcInfo->GetParsedFunctionBody()->IsParamAndBodyScopeMerged())
        {
            InsertPropertyToDebuggerScope(funcInfo, debuggerScope, funcInfo->GetArgumentsSymbol());
        }

        RecordEndScopeObject(pnode);
    }
}

void ByteCodeGenerator::InsertPropertyToDebuggerScope(FuncInfo* funcInfo, Js::DebuggerScope* debuggerScope, Symbol* sym)
{TRACE_IT(38946);
    if (sym)
    {TRACE_IT(38947);
        Js::FunctionBody* funcBody = funcInfo->GetParsedFunctionBody();
        Js::DebuggerScopePropertyFlags flag = Js::DebuggerScopePropertyFlags_None;
        Js::RegSlot location = sym->GetLocation();
        if (ShouldTrackDebuggerMetadata() && !funcInfo->IsBodyAndParamScopeMerged() && funcInfo->bodyScope->FindLocalSymbol(sym->GetName()) != nullptr)
        {TRACE_IT(38948);
            flag |= Js::DebuggerScopePropertyFlags_HasDuplicateInBody;
            location = funcBody->MapRegSlot(location);
        }

        debuggerScope->AddProperty(location, sym->EnsurePosition(funcInfo), flag);
    }
}

void ByteCodeGenerator::EmitDefaultArgs(FuncInfo *funcInfo, ParseNode *pnode)
{TRACE_IT(38949);
    uint beginOffset = m_writer.GetCurrentOffset();

    auto emitDefaultArg = [&](ParseNode *pnodeArg)
    {TRACE_IT(38950);
        if (pnodeArg->nop == knopParamPattern)
        {TRACE_IT(38951);
            this->StartStatement(pnodeArg);

            Assert(pnodeArg->sxParamPattern.location != Js::Constants::NoRegister);
            ParseNodePtr pnode1 = pnodeArg->sxParamPattern.pnode1;

            if (pnode1->IsPattern())
            {
                EmitAssignment(nullptr, pnode1, pnodeArg->sxParamPattern.location, this, funcInfo);
            }
            else
            {TRACE_IT(38952);
                Assert(pnode1->nop == knopAsg);
                Assert(pnode1->sxBin.pnode1->IsPattern());
                EmitDestructuredValueOrInitializer(pnode1->sxBin.pnode1,
                    pnodeArg->sxParamPattern.location,
                    pnode1->sxBin.pnode2,
                    false /*isNonPatternAssignmentTarget*/,
                    this,
                    funcInfo);
            }
            this->EndStatement(pnodeArg);
            return;
        }
        else if (pnodeArg->IsVarLetOrConst())
        {TRACE_IT(38953);
            Js::RegSlot location = pnodeArg->sxVar.sym->GetLocation();

            if (pnodeArg->sxVar.pnodeInit == nullptr)
            {TRACE_IT(38954);
                // Since the formal hasn't been initialized in LdLetHeapArguments, we'll initialize it here.
                pnodeArg->sxVar.sym->SetNeedDeclaration(false);
                EmitPropStore(location, pnodeArg->sxVar.sym, pnodeArg->sxVar.pid, funcInfo, true);

                return;
            }

            // Load the default argument if we got undefined, skip RHS evaluation otherwise.
            Js::ByteCodeLabel noDefaultLabel = this->m_writer.DefineLabel();
            Js::ByteCodeLabel endLabel = this->m_writer.DefineLabel();
            this->StartStatement(pnodeArg);
            // Let us use strict not equal to differentiate between null and undefined
            m_writer.BrReg2(Js::OpCode::BrSrNeq_A, noDefaultLabel, location, funcInfo->undefinedConstantRegister);

            Emit(pnodeArg->sxVar.pnodeInit, this, funcInfo, false);
            pnodeArg->sxVar.sym->SetNeedDeclaration(false); // After emit to prevent foo(a = a)

            if (funcInfo->GetHasArguments() && pnodeArg->sxVar.sym->IsInSlot(funcInfo))
            {TRACE_IT(38955);
                EmitPropStore(pnodeArg->sxVar.pnodeInit->location, pnodeArg->sxVar.sym, pnodeArg->sxVar.pid, funcInfo, true);

                m_writer.Br(endLabel);
            }
            else
            {
                EmitAssignment(nullptr, pnodeArg, pnodeArg->sxVar.pnodeInit->location, this, funcInfo);
            }

            funcInfo->ReleaseLoc(pnodeArg->sxVar.pnodeInit);

            m_writer.MarkLabel(noDefaultLabel);

            if (funcInfo->GetHasArguments() && pnodeArg->sxVar.sym->IsInSlot(funcInfo))
            {
                EmitPropStore(location, pnodeArg->sxVar.sym, pnodeArg->sxVar.pid, funcInfo, true);

                m_writer.MarkLabel(endLabel);
            }

            this->EndStatement(pnodeArg);
        }
    };

    // If the function is async, we wrap the default arguments in a try catch and reject a Promise in case of error.
    if (pnode->sxFnc.IsAsync())
    {TRACE_IT(38956);
        uint cacheId;
        Js::ByteCodeLabel catchLabel = m_writer.DefineLabel();
        Js::ByteCodeLabel doneLabel = m_writer.DefineLabel();
        Js::RegSlot catchArgLocation = funcInfo->AcquireTmpRegister();
        Js::RegSlot promiseLocation = funcInfo->AcquireTmpRegister();
        Js::RegSlot rejectLocation = funcInfo->AcquireTmpRegister();

        // try
        m_writer.RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ true);
        m_writer.Br(Js::OpCode::TryCatch, catchLabel);

        // Rest cannot have a default argument, so we ignore it.
        MapFormalsWithoutRest(pnode, emitDefaultArg);

        m_writer.RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ false);
        m_writer.Empty(Js::OpCode::Leave);
        m_writer.Br(doneLabel);

        // catch
        m_writer.MarkLabel(catchLabel);
        m_writer.Reg1(Js::OpCode::Catch, catchArgLocation);

        m_writer.RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ true);
        m_writer.Empty(Js::OpCode::Nop);

        // return Promise.reject(error);
        cacheId = funcInfo->FindOrAddRootObjectInlineCacheId(Js::PropertyIds::Promise, false, false);
        m_writer.PatchableRootProperty(Js::OpCode::LdRootFld, promiseLocation, cacheId, false, false);

        EmitInvoke(rejectLocation, promiseLocation, Js::PropertyIds::reject, this, funcInfo, catchArgLocation);

        m_writer.Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, rejectLocation);

        m_writer.RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ false);
        m_writer.Empty(Js::OpCode::Leave);
        m_writer.Br(funcInfo->singleExit);
        m_writer.Empty(Js::OpCode::Leave);

        m_writer.MarkLabel(doneLabel);

        this->SetHasTry(true);

        funcInfo->ReleaseTmpRegister(rejectLocation);
        funcInfo->ReleaseTmpRegister(promiseLocation);
        funcInfo->ReleaseTmpRegister(catchArgLocation);
    }
    else
    {
        // Rest cannot have a default argument, so we ignore it.
        MapFormalsWithoutRest(pnode, emitDefaultArg);
    }

    if (m_writer.GetCurrentOffset() > beginOffset)
    {
        PopulateFormalsScope(beginOffset, funcInfo, pnode);
    }
}

void ByteCodeGenerator::EmitOneFunction(ParseNode *pnode)
{TRACE_IT(38957);
    Assert(pnode && (pnode->nop == knopProg || pnode->nop == knopFncDecl));
    FuncInfo *funcInfo = pnode->sxFnc.funcInfo;
    Assert(funcInfo != nullptr);

    if (funcInfo->IsFakeGlobalFunction(this->flags))
    {TRACE_IT(38958);
        return;
    }

    Js::ParseableFunctionInfo* deferParseFunction = funcInfo->byteCodeFunction;
    deferParseFunction->SetGrfscr(deferParseFunction->GetGrfscr() | (this->flags & ~fscrDeferredFncExpression));
    deferParseFunction->SetSourceInfo(this->GetCurrentSourceIndex(),
        funcInfo->root,
        !!(this->flags & fscrEvalCode),
        ((this->flags & fscrDynamicCode) && !(this->flags & fscrEvalCode)));

    deferParseFunction->SetInParamsCount(funcInfo->inArgsCount);
    if (pnode->sxFnc.HasDefaultArguments())
    {TRACE_IT(38959);
        deferParseFunction->SetReportedInParamsCount(pnode->sxFnc.firstDefaultArg + 1);
    }
    else
    {TRACE_IT(38960);
        deferParseFunction->SetReportedInParamsCount(funcInfo->inArgsCount);
    }

    if (funcInfo->root->sxFnc.pnodeBody == nullptr)
    {TRACE_IT(38961);
        if (!PHASE_OFF1(Js::SkipNestedDeferredPhase))
        {TRACE_IT(38962);
            deferParseFunction->BuildDeferredStubs(funcInfo->root);
        }
        Assert(!deferParseFunction->IsFunctionBody() || deferParseFunction->GetFunctionBody()->GetByteCode() != nullptr);
        return;
    }

    Js::FunctionBody* byteCodeFunction = funcInfo->GetParsedFunctionBody();
    // We've now done a full parse of this function, so we no longer need to remember the extents
    // and attributes of the top-level nested functions. (The above code has run for all of those,
    // so they have pointers to the stub sub-trees they need.)
    byteCodeFunction->SetDeferredStubs(nullptr);

    try
    {TRACE_IT(38963);
        if (!funcInfo->IsGlobalFunction())
        {
            // Note: Do not set the stack nested func flag if the function has been redeferred and recompiled.
            // In that case the flag already has the value we want.
            if (CanStackNestedFunc(funcInfo, true) && byteCodeFunction->GetCompileCount() == 0)
            {TRACE_IT(38964);
#if DBG
                byteCodeFunction->SetCanDoStackNestedFunc();
#endif
                if (funcInfo->root->sxFnc.astSize <= PnFnc::MaxStackClosureAST)
                {TRACE_IT(38965);
                    byteCodeFunction->SetStackNestedFunc(true);
                }
            }
        }

        if (byteCodeFunction->DoStackNestedFunc())
        {TRACE_IT(38966);
            uint nestedCount = byteCodeFunction->GetNestedCount();
            for (uint i = 0; i < nestedCount; i++)
            {TRACE_IT(38967);
                Js::FunctionProxy * nested = byteCodeFunction->GetNestedFunctionProxy(i);
                if (nested->IsFunctionBody())
                {TRACE_IT(38968);
                    nested->GetFunctionBody()->SetStackNestedFuncParent(byteCodeFunction->GetFunctionInfo());
                }
            }
        }

        if (byteCodeFunction->GetByteCode() != nullptr)
        {TRACE_IT(38969);
            // Previously compiled function nested within a re-deferred and re-compiled function.
            return;
        }

        // Bug : 301517
        // In the debug mode the hasOnlyThis optimization needs to be disabled, since user can break in this function
        // and do operation on 'this' and its property, which may not be defined yet.
        if (funcInfo->root->sxFnc.HasOnlyThisStmts() && !IsInDebugMode())
        {TRACE_IT(38970);
            byteCodeFunction->SetHasOnlyThisStmts(true);
        }

        if (byteCodeFunction->IsInlineApplyDisabled() || this->scriptContext->GetConfig()->IsNoNative())
        {TRACE_IT(38971);
            if ((pnode->nop == knopFncDecl) && (funcInfo->GetHasHeapArguments()) && (!funcInfo->GetCallsEval()) && ApplyEnclosesArgs(pnode, this))
            {TRACE_IT(38972);
                bool applyEnclosesArgs = true;
                for (ParseNode* pnodeVar = funcInfo->root->sxFnc.pnodeVars; pnodeVar; pnodeVar = pnodeVar->sxVar.pnodeNext)
                {TRACE_IT(38973);
                    Symbol* sym = pnodeVar->sxVar.sym;
                    if (sym->GetSymbolType() == STVariable && !sym->GetIsArguments())
                    {TRACE_IT(38974);
                        applyEnclosesArgs = false;
                        break;
                    }
                }
                auto constAndLetCheck = [](ParseNode *pnodeBlock, bool *applyEnclosesArgs)
                {TRACE_IT(38975);
                    if (*applyEnclosesArgs)
                    {TRACE_IT(38976);
                        for (auto lexvar = pnodeBlock->sxBlock.pnodeLexVars; lexvar; lexvar = lexvar->sxVar.pnodeNext)
                        {TRACE_IT(38977);
                            Symbol* sym = lexvar->sxVar.sym;
                            if (sym->GetSymbolType() == STVariable && !sym->GetIsArguments())
                            {TRACE_IT(38978);
                                *applyEnclosesArgs = false;
                                break;
                            }
                        }
                    }
                };
                constAndLetCheck(funcInfo->root->sxFnc.pnodeScopes, &applyEnclosesArgs);
                constAndLetCheck(funcInfo->root->sxFnc.pnodeBodyScope, &applyEnclosesArgs);
                funcInfo->SetApplyEnclosesArgs(applyEnclosesArgs);
            }
        }

        InitScopeSlotArray(funcInfo);
        FinalizeRegisters(funcInfo, byteCodeFunction);
        DebugOnly(Js::RegSlot firstTmpReg = funcInfo->varRegsCount);

        // Reserve temp registers for the inner scopes. We prefer temps because the JIT will then renumber them
        // and see different lifetimes. (Note that debug mode requires permanent registers. See FinalizeRegisters.)
        uint innerScopeCount = funcInfo->InnerScopeCount();
        if (!this->IsInDebugMode())
        {TRACE_IT(38979);
            byteCodeFunction->SetInnerScopeCount(innerScopeCount);
            if (innerScopeCount)
            {TRACE_IT(38980);
                funcInfo->SetFirstInnerScopeReg(funcInfo->AcquireTmpRegister());
                for (uint i = 1; i < innerScopeCount; i++)
                {TRACE_IT(38981);
                    funcInfo->AcquireTmpRegister();
                }
            }
        }

        funcInfo->inlineCacheMap = Anew(alloc, FuncInfo::InlineCacheMap,
            alloc,
            funcInfo->RegCount() // Pass the actual register count. // TODO: Check if we can reduce this count
            );
        funcInfo->rootObjectLoadInlineCacheMap = Anew(alloc, FuncInfo::RootObjectInlineCacheIdMap,
            alloc,
            10);
        funcInfo->rootObjectLoadMethodInlineCacheMap = Anew(alloc, FuncInfo::RootObjectInlineCacheIdMap,
            alloc,
            10);
        funcInfo->rootObjectStoreInlineCacheMap = Anew(alloc, FuncInfo::RootObjectInlineCacheIdMap,
            alloc,
            10);
        funcInfo->referencedPropertyIdToMapIndex = Anew(alloc, FuncInfo::RootObjectInlineCacheIdMap,
            alloc,
            10);

        byteCodeFunction->AllocateLiteralRegexArray();
        m_callSiteId = 0;
        m_writer.Begin(byteCodeFunction, alloc, this->DoJitLoopBodies(funcInfo), funcInfo->hasLoop, this->IsInDebugMode());
        this->PushFuncInfo(_u("EmitOneFunction"), funcInfo);

        this->inPrologue = true;

        // Class constructors do not have a [[call]] slot but we don't implement a generic way to express this.
        // What we do is emit a check for the new flag here. If we don't have CallFlags_New set, the opcode will throw.
        // We need to do this before emitting 'this' since the base class constructor will try to construct a new object.
        if (funcInfo->IsClassConstructor())
        {TRACE_IT(38982);
            m_writer.Empty(Js::OpCode::ChkNewCallFlag);
        }

        Scope* paramScope = funcInfo->GetParamScope();
        Scope* bodyScope = funcInfo->GetBodyScope();

        // For now, emit all constant loads at top of function (should instead put in closest dominator of uses).
        LoadAllConstants(funcInfo);
        HomeArguments(funcInfo);

        if (!funcInfo->IsBodyAndParamScopeMerged())
        {TRACE_IT(38983);
            byteCodeFunction->SetParamAndBodyScopeNotMerged();

            // Pop the body scope before emitting the default args
            PopScope();
            Assert(this->GetCurrentScope() == paramScope);
        }

        if (funcInfo->root->sxFnc.pnodeRest != nullptr)
        {TRACE_IT(38984);
            byteCodeFunction->SetHasRestParameter();
        }

        if (funcInfo->IsGlobalFunction())
        {TRACE_IT(38985);
            EnsureNoRedeclarations(pnode->sxFnc.pnodeScopes, funcInfo);
        }

        ::BeginEmitBlock(pnode->sxFnc.pnodeScopes, this, funcInfo);

        if (funcInfo->thisScopeSlot != Js::Constants::NoRegister && !(funcInfo->IsLambda() || (funcInfo->IsGlobalFunction() && this->flags & fscrEval)))
        {
            EmitInitCapturedThis(funcInfo, funcInfo->bodyScope);
        }

        // Any function with a super reference or an eval call inside a method or a constructor needs to load super,
        if ((funcInfo->HasSuperReference() || (funcInfo->GetCallsEval() && (funcInfo->root->sxFnc.IsMethod() || funcInfo->root->sxFnc.IsConstructor())))
            // unless we are already inside the 'global' scope inside an eval (in which case 'ScopedLdHomeObj' is emitted at every 'super' reference).
            && !((GetFlags() & fscrEval) && funcInfo->IsGlobalFunction()))
        {TRACE_IT(38986);
            if (funcInfo->IsLambda())
            {TRACE_IT(38987);
                Scope *scope;
                Js::PropertyId envIndex = -1;
                GetEnclosingNonLambdaScope(funcInfo, scope, envIndex);

                FuncInfo* parent = scope->GetFunc();

                if (!parent->IsGlobalFunction())
                {
                    // lambda in non-global scope (eval and non-eval)
                    EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, parent->superScopeSlot, funcInfo->superRegister);
                    if (funcInfo->superCtorRegister != Js::Constants::NoRegister)
                    {
                        EmitInternalScopedSlotLoad(funcInfo, scope, envIndex, parent->superCtorScopeSlot, funcInfo->superCtorRegister);
                    }
                }
                else if (!(GetFlags() & fscrEval))
                {TRACE_IT(38988);
                    // lambda in non-eval global scope
                    m_writer.Reg1(Js::OpCode::LdUndef, funcInfo->superRegister);
                }
                // lambda in eval global scope: ScopedLdHomeObj will handle error throwing
            }
            else
            {TRACE_IT(38989);
                m_writer.Reg1(Js::OpCode::LdHomeObj, funcInfo->superRegister);

                if (funcInfo->superCtorRegister != Js::Constants::NoRegister) // super() is allowed only in derived class constructors
                {TRACE_IT(38990);
                    m_writer.Reg1(Js::OpCode::LdFuncObj, funcInfo->superCtorRegister);
                }

                if (!funcInfo->IsGlobalFunction())
                {TRACE_IT(38991);
                    if (bodyScope->GetIsObject() && bodyScope->GetLocation() != Js::Constants::NoRegister)
                    {TRACE_IT(38992);
                        // Stash the super reference in case something inside the eval or lambda references it.
                        uint cacheId = funcInfo->FindOrAddInlineCacheId(bodyScope->GetLocation(), Js::PropertyIds::_superReferenceSymbol, false, true);
                        m_writer.ElementP(Js::OpCode::InitLocalFld, funcInfo->superRegister, cacheId);
                        if (funcInfo->superCtorRegister != Js::Constants::NoRegister)
                        {TRACE_IT(38993);
                            cacheId = funcInfo->FindOrAddInlineCacheId(bodyScope->GetLocation(), Js::PropertyIds::_superCtorReferenceSymbol, false, true);
                            m_writer.ElementP(Js::OpCode::InitLocalFld, funcInfo->superCtorRegister, cacheId);
                        }
                    }
                    else if (funcInfo->superScopeSlot == Js::Constants::NoProperty || funcInfo->superCtorScopeSlot == Js::Constants::NoProperty)
                    {TRACE_IT(38994);
                        // While the diag locals walker will pick up super from scoped slots or an activation object,
                        // it will not pick it up when it is only in a register.
                        byteCodeFunction->InsertSymbolToRegSlotList(funcInfo->superRegister, Js::PropertyIds::_superReferenceSymbol, funcInfo->varRegsCount);
                        if (funcInfo->superCtorRegister != Js::Constants::NoRegister)
                        {TRACE_IT(38995);
                            byteCodeFunction->InsertSymbolToRegSlotList(funcInfo->superCtorRegister, Js::PropertyIds::_superCtorReferenceSymbol, funcInfo->varRegsCount);
                        }
                    }
                }
            }
        }

        if (funcInfo->newTargetScopeSlot != Js::Constants::NoRegister && !funcInfo->IsGlobalFunction())
        {
            EmitInitCapturedNewTarget(funcInfo, bodyScope);
        }

        // We don't want to load super if we are already in an eval. ScopedLdHomeObj will take care of loading super in that case.
        if (!(GetFlags() & fscrEval) && !bodyScope->GetIsObject())
        {TRACE_IT(38996);
            if (funcInfo->superScopeSlot != Js::Constants::NoRegister)
            {TRACE_IT(38997);
                this->EmitInternalScopedSlotStore(funcInfo, funcInfo->superScopeSlot, funcInfo->superRegister);
            }

            if (funcInfo->superCtorScopeSlot != Js::Constants::NoRegister)
            {TRACE_IT(38998);
                this->EmitInternalScopedSlotStore(funcInfo, funcInfo->superCtorScopeSlot, funcInfo->superCtorRegister);
            }
        }

        DefineLabels(funcInfo);

        if (pnode->sxFnc.HasNonSimpleParameterList())
        {TRACE_IT(38999);
            this->InitBlockScopedNonTemps(funcInfo->root->sxFnc.pnodeScopes, funcInfo);

            EmitDefaultArgs(funcInfo, pnode);

            if (!funcInfo->IsBodyAndParamScopeMerged())
            {TRACE_IT(39000);
                Assert(this->GetCurrentScope() == paramScope);
                // Push the body scope
                PushScope(bodyScope);

                funcInfo->SetCurrentChildScope(bodyScope);

                // Mark the beginning of the body scope so that new scope slots can be created.
                this->Writer()->Empty(Js::OpCode::BeginBodyScope);
            }
        }

        // Emit all scope-wide function definitions before emitting function bodies
        // so that calls may reference functions they precede lexically.
        // Note, global eval scope is a fake local scope and is handled as if it were
        // a lexical block instead of a true global scope, so do not define the functions
        // here. They will be defined during BeginEmitBlock.
        if (!(funcInfo->IsGlobalFunction() && this->IsEvalWithNoParentScopeInfo()))
        {TRACE_IT(39001);
            // This only handles function declarations, which param scope cannot have any.
            DefineFunctions(funcInfo);
        }

        DefineUserVars(funcInfo);

        if (pnode->sxFnc.HasNonSimpleParameterList())
        {TRACE_IT(39002);
            this->InitBlockScopedNonTemps(funcInfo->root->sxFnc.pnodeBodyScope, funcInfo);
        }
        else
        {TRACE_IT(39003);
            this->InitBlockScopedNonTemps(funcInfo->root->sxFnc.pnodeScopes, funcInfo);
        }

        if (!pnode->sxFnc.HasNonSimpleParameterList() && funcInfo->GetHasArguments() && !NeedScopeObjectForArguments(funcInfo, pnode))
        {
            // If we didn't create a scope object and didn't have default args, we still need to transfer the formals to their slots.
            MapFormalsWithoutRest(pnode, [&](ParseNode *pnodeArg) { EmitPropStore(pnodeArg->sxVar.sym->GetLocation(), pnodeArg->sxVar.sym, pnodeArg->sxVar.pid, funcInfo); });
        }

        // Rest needs to trigger use before declaration until all default args have been processed.
        if (pnode->sxFnc.pnodeRest != nullptr)
        {TRACE_IT(39004);
            pnode->sxFnc.pnodeRest->sxVar.sym->SetNeedDeclaration(false);
        }

        Js::RegSlot formalsUpperBound = Js::Constants::NoRegister; // Needed for tracking the last RegSlot in the param scope
        if (!funcInfo->IsBodyAndParamScopeMerged())
        {TRACE_IT(39005);
            // Emit bytecode to copy the initial values from param names to their corresponding body bindings.
            // We have to do this after the rest param is marked as false for need declaration.
            Symbol* funcSym = funcInfo->root->sxFnc.GetFuncSymbol();
            paramScope->ForEachSymbol([&](Symbol* param) {
                Symbol* varSym = funcInfo->GetBodyScope()->FindLocalSymbol(param->GetName());
                if (varSym)
                {TRACE_IT(39006);
                    if ((funcSym == nullptr || funcSym != param)    // Do not copy the symbol over to body as the function expression symbol
                                                                    // is expected to stay inside the function expression scope
                        && (varSym->GetSymbolType() == STVariable && (varSym->IsInSlot(funcInfo) || varSym->GetLocation() != Js::Constants::NoRegister)))
                    {TRACE_IT(39007);
                        if (!varSym->GetNeedDeclaration())
                        {TRACE_IT(39008);
                            if (param->IsInSlot(funcInfo))
                            {TRACE_IT(39009);
                                // Simulating EmitPropLoad here. We can't directly call the method as we have to use the param scope specifically.
                                // Walking the scope chain is not possible at this time.
                                Js::RegSlot tempReg = funcInfo->AcquireTmpRegister();
                                Js::PropertyId slot = param->EnsureScopeSlot(funcInfo);
                                Js::ProfileId profileId = funcInfo->FindOrAddSlotProfileId(paramScope, slot);
                                Js::OpCode op = paramScope->GetIsObject() ? Js::OpCode::LdParamObjSlot : Js::OpCode::LdParamSlot;
                                slot = slot + (paramScope->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

                                this->m_writer.SlotI1(op, tempReg, slot, profileId);

                                this->EmitPropStore(tempReg, varSym, varSym->GetPid(), funcInfo);
                                funcInfo->ReleaseTmpRegister(tempReg);
                            }
                            else if (param->GetLocation() != Js::Constants::NoRegister)
                            {TRACE_IT(39010);
                                this->EmitPropStore(param->GetLocation(), varSym, varSym->GetPid(), funcInfo);
                            }
                            else
                            {TRACE_IT(39011);
                                Assert(param->GetIsArguments() && !funcInfo->GetHasArguments());
                            }
                        }
                        else
                        {TRACE_IT(39012);
                            // There is a let redeclaration of arguments symbol. Any other var will cause a
                            // re-declaration error.
                            Assert(param->GetIsArguments());
                        }
                    }
                }

                if (ShouldTrackDebuggerMetadata() && param->GetLocation() != Js::Constants::NoRegister)
                {TRACE_IT(39013);
                    if (formalsUpperBound == Js::Constants::NoRegister || formalsUpperBound < param->GetLocation())
                    {TRACE_IT(39014);
                        formalsUpperBound = param->GetLocation();
                    }
                }
            });
        }
        if (ShouldTrackDebuggerMetadata() && byteCodeFunction->GetPropertyIdOnRegSlotsContainer())
        {TRACE_IT(39015);
            byteCodeFunction->GetPropertyIdOnRegSlotsContainer()->formalsUpperBound = formalsUpperBound;
        }

        if (pnode->sxFnc.pnodeBodyScope != nullptr)
        {TRACE_IT(39016);
            ::BeginEmitBlock(pnode->sxFnc.pnodeBodyScope, this, funcInfo);
        }

        this->inPrologue = false;

        if (funcInfo->IsGlobalFunction())
        {TRACE_IT(39017);
            EmitGlobalBody(funcInfo);
        }
        else
        {TRACE_IT(39018);
            EmitFunctionBody(funcInfo);
        }

        if (pnode->sxFnc.pnodeBodyScope != nullptr)
        {TRACE_IT(39019);
            ::EndEmitBlock(pnode->sxFnc.pnodeBodyScope, this, funcInfo);
        }
        ::EndEmitBlock(pnode->sxFnc.pnodeScopes, this, funcInfo);

        if (!this->IsInDebugMode())
        {TRACE_IT(39020);
            // Release the temp registers that we reserved for inner scopes above.
            if (innerScopeCount)
            {TRACE_IT(39021);
                Js::RegSlot tmpReg = funcInfo->FirstInnerScopeReg() + innerScopeCount - 1;
                for (uint i = 0; i < innerScopeCount; i++)
                {TRACE_IT(39022);
                    funcInfo->ReleaseTmpRegister(tmpReg);
                    tmpReg--;
                }
            }
        }

        Assert(funcInfo->firstTmpReg == firstTmpReg);
        Assert(funcInfo->curTmpReg == firstTmpReg);
        Assert(byteCodeFunction->GetFirstTmpReg() == firstTmpReg + byteCodeFunction->GetConstantCount());

        byteCodeFunction->CheckAndSetVarCount(funcInfo->varRegsCount);
        byteCodeFunction->CheckAndSetOutParamMaxDepth(funcInfo->outArgsMaxDepth);
        byteCodeFunction->SetForInLoopDepth(funcInfo->GetMaxForInLoopLevel());

        // Do a uint32 add just to verify that we haven't overflowed the reg slot type.
        UInt32Math::Add(funcInfo->varRegsCount, funcInfo->constRegsCount);

#if DBG_DUMP
        if (PHASE_STATS1(Js::ByteCodePhase))
        {TRACE_IT(39023);
            Output::Print(_u(" BCode: %-10d, Aux: %-10d, AuxC: %-10d Total: %-10d,  %s\n"),
                m_writer.ByteCodeDataSize(),
                m_writer.AuxiliaryDataSize(),
                m_writer.AuxiliaryContextDataSize(),
                m_writer.ByteCodeDataSize() + m_writer.AuxiliaryDataSize() + m_writer.AuxiliaryContextDataSize(),
                funcInfo->name);

            this->scriptContext->byteCodeDataSize += m_writer.ByteCodeDataSize();
            this->scriptContext->byteCodeAuxiliaryDataSize += m_writer.AuxiliaryDataSize();
            this->scriptContext->byteCodeAuxiliaryContextDataSize += m_writer.AuxiliaryContextDataSize();
        }
#endif

        this->MapCacheIdsToPropertyIds(funcInfo);
        this->MapReferencedPropertyIds(funcInfo);

        Assert(this->TopFuncInfo() == funcInfo);
        PopFuncInfo(_u("EmitOneFunction"));
        m_writer.SetCallSiteCount(m_callSiteId);
#ifdef LOG_BYTECODE_AST_RATIO
        m_writer.End(funcInfo->root->sxFnc.astSize, this->maxAstSize);
#else
        m_writer.End();
#endif
    }
    catch (...)
    {TRACE_IT(39024);
        // Failed to generate byte-code for this function body (likely OOM or stack overflow). Notify the function body so that
        // it can revert intermediate state changes that may have taken place during byte code generation before the failure.
        byteCodeFunction->ResetByteCodeGenState();
        m_writer.Reset();
        throw;
    }

#ifdef PERF_HINT
    if (PHASE_TRACE1(Js::PerfHintPhase) && !byteCodeFunction->GetIsGlobalFunc())
    {TRACE_IT(39025);
        if (byteCodeFunction->GetHasTry())
        {TRACE_IT(39026);
            WritePerfHint(PerfHints::HasTryBlock_Verbose, byteCodeFunction);
        }

        if (funcInfo->GetCallsEval())
        {TRACE_IT(39027);
            WritePerfHint(PerfHints::CallsEval_Verbose, byteCodeFunction);
        }
        else if (funcInfo->GetChildCallsEval())
        {TRACE_IT(39028);
            WritePerfHint(PerfHints::ChildCallsEval, byteCodeFunction);
        }
    }
#endif


    byteCodeFunction->SetInitialDefaultEntryPoint();
    byteCodeFunction->SetCompileCount(UInt32Math::Add(byteCodeFunction->GetCompileCount(), 1));

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (byteCodeFunction->IsInDebugMode() != scriptContext->IsScriptContextInDebugMode()) // debug mode mismatch
    {TRACE_IT(39029);
        if (m_utf8SourceInfo->GetIsLibraryCode())
        {TRACE_IT(39030);
            Assert(!byteCodeFunction->IsInDebugMode()); // Library script byteCode is never in debug mode
        }
        else
        {TRACE_IT(39031);
            Js::Throw::FatalInternalError();
        }
    }
#endif

#if DBG_DUMP
    if (PHASE_DUMP(Js::ByteCodePhase, funcInfo->byteCodeFunction) && Js::Configuration::Global.flags.Verbose)
    {TRACE_IT(39032);
        pnode->Dump();
    }
    if (this->Trace() || PHASE_DUMP(Js::ByteCodePhase, funcInfo->byteCodeFunction))
    {TRACE_IT(39033);
        Js::ByteCodeDumper::Dump(byteCodeFunction);
    }
    if (PHASE_DUMP(Js::DebuggerScopePhase, funcInfo->byteCodeFunction))
    {TRACE_IT(39034);
        byteCodeFunction->DumpScopes();
    }
#endif
#if ENABLE_NATIVE_CODEGEN
    if ((!PHASE_OFF(Js::BackEndPhase, funcInfo->byteCodeFunction))
        && !this->forceNoNative
        && !this->scriptContext->GetConfig()->IsNoNative())
    {TRACE_IT(39035);
        GenerateFunction(this->scriptContext->GetNativeCodeGenerator(), byteCodeFunction);
    }
#endif
}

void ByteCodeGenerator::MapCacheIdsToPropertyIds(FuncInfo *funcInfo)
{TRACE_IT(39036);
    Js::FunctionBody *functionBody = funcInfo->GetParsedFunctionBody();
    uint rootObjectLoadInlineCacheStart = funcInfo->GetInlineCacheCount();
    uint rootObjectLoadMethodInlineCacheStart = rootObjectLoadInlineCacheStart + funcInfo->GetRootObjectLoadInlineCacheCount();
    uint rootObjectStoreInlineCacheStart = rootObjectLoadMethodInlineCacheStart + funcInfo->GetRootObjectLoadMethodInlineCacheCount();
    uint totalFieldAccessInlineCacheCount = rootObjectStoreInlineCacheStart + funcInfo->GetRootObjectStoreInlineCacheCount();

    functionBody->CreateCacheIdToPropertyIdMap(rootObjectLoadInlineCacheStart, rootObjectLoadMethodInlineCacheStart,
        rootObjectStoreInlineCacheStart, totalFieldAccessInlineCacheCount, funcInfo->GetIsInstInlineCacheCount());

    if (totalFieldAccessInlineCacheCount == 0)
    {TRACE_IT(39037);
        return;
    }

    funcInfo->inlineCacheMap->Map([functionBody](Js::RegSlot regSlot, FuncInfo::InlineCacheIdMap *inlineCacheIdMap)
    {
        inlineCacheIdMap->Map([functionBody](Js::PropertyId propertyId, FuncInfo::InlineCacheList* inlineCacheList)
        {
            if (inlineCacheList)
            {TRACE_IT(39038);
                inlineCacheList->Iterate([functionBody, propertyId](InlineCacheUnit cacheUnit)
                {
                    CompileAssert(offsetof(InlineCacheUnit, cacheId) == offsetof(InlineCacheUnit, loadCacheId));
                    if (cacheUnit.loadCacheId != -1)
                    {TRACE_IT(39039);
                        functionBody->SetPropertyIdForCacheId(cacheUnit.loadCacheId, propertyId);
                    }
                    if (cacheUnit.loadMethodCacheId != -1)
                    {TRACE_IT(39040);
                        functionBody->SetPropertyIdForCacheId(cacheUnit.loadMethodCacheId, propertyId);
                    }
                    if (cacheUnit.storeCacheId != -1)
                    {TRACE_IT(39041);
                        functionBody->SetPropertyIdForCacheId(cacheUnit.storeCacheId, propertyId);
                    }
                });
            }
        });
    });

    funcInfo->rootObjectLoadInlineCacheMap->Map([functionBody, rootObjectLoadInlineCacheStart](Js::PropertyId propertyId, uint cacheId)
    {
        functionBody->SetPropertyIdForCacheId(cacheId + rootObjectLoadInlineCacheStart, propertyId);
    });
    funcInfo->rootObjectLoadMethodInlineCacheMap->Map([functionBody, rootObjectLoadMethodInlineCacheStart](Js::PropertyId propertyId, uint cacheId)
    {
        functionBody->SetPropertyIdForCacheId(cacheId + rootObjectLoadMethodInlineCacheStart, propertyId);
    });
    funcInfo->rootObjectStoreInlineCacheMap->Map([functionBody, rootObjectStoreInlineCacheStart](Js::PropertyId propertyId, uint cacheId)
    {
        functionBody->SetPropertyIdForCacheId(cacheId + rootObjectStoreInlineCacheStart, propertyId);
    });

    SListBase<uint>::Iterator valueOfIter(&funcInfo->valueOfStoreCacheIds);
    while (valueOfIter.Next())
    {TRACE_IT(39042);
        functionBody->SetPropertyIdForCacheId(valueOfIter.Data(), Js::PropertyIds::valueOf);
    }

    SListBase<uint>::Iterator toStringIter(&funcInfo->toStringStoreCacheIds);
    while (toStringIter.Next())
    {TRACE_IT(39043);
        functionBody->SetPropertyIdForCacheId(toStringIter.Data(), Js::PropertyIds::toString);
    }

#if DBG
    functionBody->VerifyCacheIdToPropertyIdMap();
#endif
}

void ByteCodeGenerator::MapReferencedPropertyIds(FuncInfo * funcInfo)
{TRACE_IT(39044);
    Js::FunctionBody *functionBody = funcInfo->GetParsedFunctionBody();
    uint referencedPropertyIdCount = funcInfo->GetReferencedPropertyIdCount();
    functionBody->CreateReferencedPropertyIdMap(referencedPropertyIdCount);

    funcInfo->referencedPropertyIdToMapIndex->Map([functionBody](Js::PropertyId propertyId, uint mapIndex)
    {
        functionBody->SetReferencedPropertyIdWithMapIndex(mapIndex, propertyId);
    });

#if DBG
    functionBody->VerifyReferencedPropertyIdMap();
#endif
}

void ByteCodeGenerator::EmitScopeList(ParseNode *pnode, ParseNode *breakOnBodyScopeNode)
{TRACE_IT(39045);
    while (pnode)
    {TRACE_IT(39046);
        if (breakOnBodyScopeNode != nullptr && breakOnBodyScopeNode == pnode)
        {TRACE_IT(39047);
            break;
        }

        switch (pnode->nop)
        {
        case knopFncDecl:
#ifdef ASMJS_PLAT
            if (pnode->sxFnc.GetAsmjsMode())
            {TRACE_IT(39048);
                Js::ExclusiveContext context(this, GetScriptContext());
                if (Js::AsmJSCompiler::Compile(&context, pnode, pnode->sxFnc.pnodeParams))
                {TRACE_IT(39049);
                    pnode = pnode->sxFnc.pnodeNext;
                    break;
                }
                else if (CONFIG_FLAG(AsmJsStopOnError))
                {TRACE_IT(39050);
                    exit(JSERR_AsmJsCompileError);
                }
                else
                {TRACE_IT(39051);
                    // if asm.js parse error happened, reparse with asm.js disabled.
                    throw Js::AsmJsParseException();
                }
            }
#endif
            // FALLTHROUGH
        case knopProg:
            if (pnode->sxFnc.funcInfo)
            {TRACE_IT(39052);
                FuncInfo* funcInfo = pnode->sxFnc.funcInfo;
                Scope* paramScope = funcInfo->GetParamScope();

                if (!funcInfo->IsBodyAndParamScopeMerged())
                {TRACE_IT(39053);
                    funcInfo->SetCurrentChildScope(paramScope);
                }
                else
                {TRACE_IT(39054);
                    funcInfo->SetCurrentChildScope(funcInfo->GetBodyScope());
                }
                this->StartEmitFunction(pnode);

                // Persist outer func scope info if nested func is deferred
                if (CONFIG_FLAG(DeferNested))
                {TRACE_IT(39055);
                    FuncInfo* parentFunc = TopFuncInfo();
                    Js::ScopeInfo::SaveScopeInfoForDeferParse(this, parentFunc, funcInfo);
                    PushFuncInfo(_u("StartEmitFunction"), funcInfo);
                }

                if (!funcInfo->IsBodyAndParamScopeMerged())
                {TRACE_IT(39056);
                    this->EmitScopeList(pnode->sxFnc.pnodeBodyScope->sxBlock.pnodeScopes);
                }
                else
                {TRACE_IT(39057);
                    this->EmitScopeList(pnode->sxFnc.pnodeScopes);
                }

                this->EmitOneFunction(pnode);
                this->EndEmitFunction(pnode);

                Assert(pnode->sxFnc.pnodeBody == nullptr || funcInfo->isReused || funcInfo->GetCurrentChildScope() == funcInfo->GetBodyScope());
                funcInfo->SetCurrentChildScope(nullptr);
            }
            pnode = pnode->sxFnc.pnodeNext;
            break;

        case knopBlock:
            this->StartEmitBlock(pnode);
            this->EmitScopeList(pnode->sxBlock.pnodeScopes);
            this->EndEmitBlock(pnode);
            pnode = pnode->sxBlock.pnodeNext;
            break;

        case knopCatch:
            this->StartEmitCatch(pnode);
            this->EmitScopeList(pnode->sxCatch.pnodeScopes);
            this->EndEmitCatch(pnode);
            pnode = pnode->sxCatch.pnodeNext;
            break;

        case knopWith:
            this->StartEmitWith(pnode);
            this->EmitScopeList(pnode->sxWith.pnodeScopes);
            this->EndEmitWith(pnode);
            pnode = pnode->sxWith.pnodeNext;
            break;

        default:
            AssertMsg(false, "Unexpected opcode in tree of scopes");
            break;
        }
    }
}

void EnsureFncDeclScopeSlot(ParseNode *pnodeFnc, FuncInfo *funcInfo)
{TRACE_IT(39058);
    if (pnodeFnc->sxFnc.pnodeName)
    {TRACE_IT(39059);
        Assert(pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl);
        Symbol *sym = pnodeFnc->sxFnc.pnodeName->sxVar.sym;
        // If this function is shadowing the arguments symbol in body then skip it.
        // We will allocate scope slot for the arguments symbol during EmitLocalPropInit.
        if (sym && !sym->GetIsArguments())
        {TRACE_IT(39060);
            sym->EnsureScopeSlot(funcInfo);
        }
    }
}

// Similar to EnsureFncScopeSlot visitor function, but verifies that a slot is needed before assigning it.
void CheckFncDeclScopeSlot(ParseNode *pnodeFnc, FuncInfo *funcInfo)
{TRACE_IT(39061);
    if (pnodeFnc->sxFnc.pnodeName && pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl)
    {TRACE_IT(39062);
        Assert(pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl);
        Symbol *sym = pnodeFnc->sxFnc.pnodeName->sxVar.sym;
        if (sym && sym->NeedsSlotAlloc(funcInfo))
        {TRACE_IT(39063);
            sym->EnsureScopeSlot(funcInfo);
        }
    }
}

void ByteCodeGenerator::EnsureSpecialScopeSlots(FuncInfo* funcInfo, Scope* scope)
{TRACE_IT(39064);
    if (scope->GetIsObject())
    {TRACE_IT(39065);
        if (funcInfo->isThisLexicallyCaptured)
        {TRACE_IT(39066);
            funcInfo->EnsureThisScopeSlot();
        }

        if (((!funcInfo->IsLambda() && funcInfo->GetCallsEval())
            || funcInfo->isSuperLexicallyCaptured))
        {TRACE_IT(39067);
            if (funcInfo->superRegister != Js::Constants::NoRegister)
            {TRACE_IT(39068);
                funcInfo->EnsureSuperScopeSlot();
            }

            if (funcInfo->superCtorRegister != Js::Constants::NoRegister)
            {TRACE_IT(39069);
                funcInfo->EnsureSuperCtorScopeSlot();
            }
        }

        if (funcInfo->isNewTargetLexicallyCaptured)
        {TRACE_IT(39070);
            funcInfo->EnsureNewTargetScopeSlot();
        }
    }
    else
    {TRACE_IT(39071);
        // Don't rely on the Emit() pass to assign scope slots where needed, because peeps/shortcuts
        // may cause some expressions not to be emitted. Assign the slots we need before we start
        // emitting the prolog.
        // TODO: Investigate moving detection of non-local references to Emit() so we don't assign
        // slots to symbols that are never referenced in emitted code.

        if (funcInfo->isThisLexicallyCaptured)
        {TRACE_IT(39072);
            funcInfo->EnsureThisScopeSlot();
        }

        if (funcInfo->isSuperLexicallyCaptured)
        {TRACE_IT(39073);
            funcInfo->EnsureSuperScopeSlot();
        }

        if (funcInfo->isSuperCtorLexicallyCaptured)
        {TRACE_IT(39074);
            funcInfo->EnsureSuperCtorScopeSlot();
        }

        if (funcInfo->isNewTargetLexicallyCaptured)
        {TRACE_IT(39075);
            funcInfo->EnsureNewTargetScopeSlot();
        }
    }
}

void ByteCodeGenerator::StartEmitFunction(ParseNode *pnodeFnc)
{TRACE_IT(39076);
    Assert(pnodeFnc->nop == knopFncDecl || pnodeFnc->nop == knopProg);

    FuncInfo *funcInfo = pnodeFnc->sxFnc.funcInfo;

    if (funcInfo->byteCodeFunction->IsFunctionParsed())
    {TRACE_IT(39077);
        if (!(flags & (fscrEval | fscrImplicitThis | fscrImplicitParents)))
        {TRACE_IT(39078);
            // Only set the environment depth if it's truly known (i.e., not in eval or event handler).
            funcInfo->GetParsedFunctionBody()->SetEnvDepth(this->envDepth);
        }

        if (pnodeFnc->sxFnc.FIBPreventsDeferral())
        {TRACE_IT(39079);
            for (Scope *scope = this->currentScope; scope; scope = scope->GetEnclosingScope())
            {TRACE_IT(39080);
                if (scope->GetScopeType() != ScopeType_FunctionBody && 
                    scope->GetScopeType() != ScopeType_Global &&
                    scope->GetScopeType() != ScopeType_GlobalEvalBlock &&
                    scope->GetMustInstantiate())
                {TRACE_IT(39081);
                    funcInfo->byteCodeFunction->SetAttributes((Js::FunctionInfo::Attributes)(funcInfo->byteCodeFunction->GetAttributes() & ~Js::FunctionInfo::Attributes::CanDefer));
                    break;
                }
            }
        }
    }

    if (funcInfo->GetCallsEval())
    {TRACE_IT(39082);
        funcInfo->byteCodeFunction->SetDontInline(true);
    }

    Scope * const funcExprScope = funcInfo->funcExprScope;
    if (funcExprScope)
    {TRACE_IT(39083);
        if (funcInfo->GetCallsEval())
        {TRACE_IT(39084);
            Assert(funcExprScope->GetIsObject());
        }

        if (funcExprScope->GetIsObject())
        {TRACE_IT(39085);
            funcExprScope->SetCapturesAll(true);
            funcExprScope->SetMustInstantiate(true);
            PushScope(funcExprScope);
        }
        else
        {TRACE_IT(39086);
            Symbol *sym = funcInfo->root->sxFnc.GetFuncSymbol();
            if (funcInfo->IsBodyAndParamScopeMerged())
            {TRACE_IT(39087);
                funcInfo->bodyScope->AddSymbol(sym);
            }
            else
            {TRACE_IT(39088);
                funcInfo->paramScope->AddSymbol(sym);
            }
            sym->EnsureScopeSlot(funcInfo);
        }
    }

    Scope * const bodyScope = funcInfo->GetBodyScope();
    Scope * const paramScope = funcInfo->GetParamScope();

    if (pnodeFnc->nop != knopProg)
    {TRACE_IT(39089);
        if (!bodyScope->GetIsObject() && NeedObjectAsFunctionScope(funcInfo, pnodeFnc))
        {TRACE_IT(39090);
            Assert(bodyScope->GetIsObject());
        }

        if (bodyScope->GetIsObject())
        {TRACE_IT(39091);
            bodyScope->SetLocation(funcInfo->frameObjRegister);
        }
        else
        {TRACE_IT(39092);
            bodyScope->SetLocation(funcInfo->frameSlotsRegister);
        }

        if (!funcInfo->IsBodyAndParamScopeMerged())
        {TRACE_IT(39093);
            if (paramScope->GetIsObject())
            {TRACE_IT(39094);
                paramScope->SetLocation(funcInfo->frameObjRegister);
            }
            else
            {TRACE_IT(39095);
                paramScope->SetLocation(funcInfo->frameSlotsRegister);
            }
        }

        if (bodyScope->GetIsObject())
        {TRACE_IT(39096);
            // Win8 908700: Disable under F12 debugger because there are too many cached scopes holding onto locals.
            funcInfo->SetHasCachedScope(
                !PHASE_OFF(Js::CachedScopePhase, funcInfo->byteCodeFunction) &&
                !funcInfo->Escapes() &&
                funcInfo->frameObjRegister != Js::Constants::NoRegister &&
                !ApplyEnclosesArgs(pnodeFnc, this) &&
                funcInfo->IsBodyAndParamScopeMerged() && // There is eval in the param scope
                (PHASE_FORCE(Js::CachedScopePhase, funcInfo->byteCodeFunction) || !IsInDebugMode())
#if ENABLE_TTD
                && !funcInfo->GetParsedFunctionBody()->GetScriptContext()->GetThreadContext()->IsRuntimeInTTDMode()
#endif
            );

            if (funcInfo->GetHasCachedScope())
            {TRACE_IT(39097);
                Assert(funcInfo->funcObjRegister == Js::Constants::NoRegister);
                Symbol *funcSym = funcInfo->root->sxFnc.GetFuncSymbol();
                if (funcSym && funcSym->GetIsFuncExpr())
                {TRACE_IT(39098);
                    if (funcSym->GetLocation() == Js::Constants::NoRegister)
                    {TRACE_IT(39099);
                        funcInfo->funcObjRegister = funcInfo->NextVarRegister();
                    }
                    else
                    {TRACE_IT(39100);
                        funcInfo->funcObjRegister = funcSym->GetLocation();
                    }
                }
                else
                {TRACE_IT(39101);
                    funcInfo->funcObjRegister = funcInfo->NextVarRegister();
                }
                Assert(funcInfo->funcObjRegister != Js::Constants::NoRegister);
            }

            ParseNode *pnode;
            Symbol *sym;

            if (funcInfo->GetHasArguments())
            {
                // Process function's formal parameters
                MapFormals(pnodeFnc, [&](ParseNode *pnode)
                {
                    if (pnode->IsVarLetOrConst())
                    {TRACE_IT(39102);
                        pnode->sxVar.sym->EnsureScopeSlot(funcInfo);
                    }
                });

                MapFormalsFromPattern(pnodeFnc, [&](ParseNode *pnode) { pnode->sxVar.sym->EnsureScopeSlot(funcInfo); });

                // Only allocate scope slot for "arguments" when really necessary. "hasDeferredChild"
                // doesn't require scope slot for "arguments" because inner functions can't access
                // outer function's arguments directly.
                sym = funcInfo->GetArgumentsSymbol();
                Assert(sym);
                if (sym->NeedsSlotAlloc(funcInfo))
                {TRACE_IT(39103);
                    sym->EnsureScopeSlot(funcInfo);
                }
            }

            sym = funcInfo->root->sxFnc.GetFuncSymbol();

            if (sym && sym->NeedsSlotAlloc(funcInfo))
            {TRACE_IT(39104);
                if (funcInfo->funcExprScope && funcInfo->funcExprScope->GetIsObject())
                {TRACE_IT(39105);
                    sym->SetScopeSlot(0);
                }
                else if (funcInfo->GetFuncExprNameReference())
                {TRACE_IT(39106);
                    sym->EnsureScopeSlot(funcInfo);
                }
            }

            if (!funcInfo->GetHasArguments())
            {TRACE_IT(39107);
                Symbol *formal;
                Js::ArgSlot pos = 1;
                auto moveArgToReg = [&](ParseNode *pnode)
                {TRACE_IT(39108);
                    if (pnode->IsVarLetOrConst())
                    {TRACE_IT(39109);
                        formal = pnode->sxVar.sym;
                        // Get the param from its argument position into its assigned register.
                        // The position should match the location; otherwise, it has been shadowed by parameter with the same name.
                        if (formal->GetLocation() + 1 == pos)
                        {TRACE_IT(39110);
                            pnode->sxVar.sym->EnsureScopeSlot(funcInfo);
                        }
                    }
                    pos++;
                };
                MapFormals(pnodeFnc, moveArgToReg);
                MapFormalsFromPattern(pnodeFnc, [&](ParseNode *pnode) { pnode->sxVar.sym->EnsureScopeSlot(funcInfo); });
            }

            this->EnsureSpecialScopeSlots(funcInfo, bodyScope);

            auto ensureFncDeclScopeSlots = [&](ParseNode *pnodeScope)
            {TRACE_IT(39111);
                for (pnode = pnodeScope; pnode;)
                {TRACE_IT(39112);
                    switch (pnode->nop)
                    {
                    case knopFncDecl:
                        if (pnode->sxFnc.IsDeclaration())
                        {
                            EnsureFncDeclScopeSlot(pnode, funcInfo);
                        }
                        pnode = pnode->sxFnc.pnodeNext;
                        break;
                    case knopBlock:
                        pnode = pnode->sxBlock.pnodeNext;
                        break;
                    case knopCatch:
                        pnode = pnode->sxCatch.pnodeNext;
                        break;
                    case knopWith:
                        pnode = pnode->sxWith.pnodeNext;
                        break;
                    }
                }
            };
            pnodeFnc->sxFnc.MapContainerScopes(ensureFncDeclScopeSlots);

            for (pnode = pnodeFnc->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
            {TRACE_IT(39113);
                sym = pnode->sxVar.sym;
                if (!(pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar()))
                {TRACE_IT(39114);
                    if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
                    {TRACE_IT(39115);
                        sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
                    }
                    if (sym->GetSymbolType() == STVariable && !sym->GetIsArguments())
                    {TRACE_IT(39116);
                        sym->EnsureScopeSlot(funcInfo);
                    }
                }
            }

            if (pnodeFnc->sxFnc.pnodeBody)
            {TRACE_IT(39117);
                Assert(pnodeFnc->sxFnc.pnodeScopes->nop == knopBlock);
                this->EnsureLetConstScopeSlots(pnodeFnc->sxFnc.pnodeBodyScope, funcInfo);
            }
        }
        else
        {TRACE_IT(39118);
            ParseNode *pnode;
            Symbol *sym;

            this->EnsureSpecialScopeSlots(funcInfo, bodyScope);

            pnodeFnc->sxFnc.MapContainerScopes([&](ParseNode *pnodeScope) { this->EnsureFncScopeSlots(pnodeScope, funcInfo); });

            for (pnode = pnodeFnc->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
            {TRACE_IT(39119);
                sym = pnode->sxVar.sym;
                if (!(pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar()))
                {TRACE_IT(39120);
                    if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
                    {TRACE_IT(39121);
                        sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
                    }
                    if (sym->GetSymbolType() == STVariable && sym->NeedsSlotAlloc(funcInfo) && !sym->GetIsArguments())
                    {TRACE_IT(39122);
                        sym->EnsureScopeSlot(funcInfo);
                    }
                }
            }

            auto ensureScopeSlot = [&](ParseNode *pnode)
            {TRACE_IT(39123);
                if (pnode->IsVarLetOrConst())
                {TRACE_IT(39124);
                    sym = pnode->sxVar.sym;
                    if (sym->GetSymbolType() == STFormal && sym->NeedsSlotAlloc(funcInfo))
                    {TRACE_IT(39125);
                        sym->EnsureScopeSlot(funcInfo);
                    }
                }
            };
            // Process function's formal parameters
            MapFormals(pnodeFnc, ensureScopeSlot);
            MapFormalsFromPattern(pnodeFnc, ensureScopeSlot);

            if (funcInfo->GetHasArguments())
            {TRACE_IT(39126);
                sym = funcInfo->GetArgumentsSymbol();
                Assert(sym);

                // There is no eval so the arguments may be captured in a lambda.
                // But we cannot relay on slots getting allocated while the lambda is emitted as the function body may be reparsed.
                sym->EnsureScopeSlot(funcInfo);
            }

            if (pnodeFnc->sxFnc.pnodeBody)
            {TRACE_IT(39127);
                this->EnsureLetConstScopeSlots(pnodeFnc->sxFnc.pnodeScopes, funcInfo);
                this->EnsureLetConstScopeSlots(pnodeFnc->sxFnc.pnodeBodyScope, funcInfo);
            }
        }

        // When we have split scope and body scope does not have any scope slots allocated, we don't have to mark the body scope as mustinstantiate.
        if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
        {TRACE_IT(39128);
            bodyScope->SetMustInstantiate(true);
        }
        else if (pnodeFnc->sxFnc.IsBodyAndParamScopeMerged() || bodyScope->GetScopeSlotCount() != 0)
        {TRACE_IT(39129);
            bodyScope->SetMustInstantiate(funcInfo->frameSlotsRegister != Js::Constants::NoRegister);
        }
        paramScope->SetMustInstantiate(!pnodeFnc->sxFnc.IsBodyAndParamScopeMerged());
    }
    else
    {TRACE_IT(39130);
        bool newScopeForEval = (funcInfo->byteCodeFunction->GetIsStrictMode() && (this->GetFlags() & fscrEval));

        if (newScopeForEval)
        {TRACE_IT(39131);
            Assert(bodyScope->GetIsObject());
        }
    }

    if (!funcInfo->IsBodyAndParamScopeMerged())
    {TRACE_IT(39132);
        ParseNodePtr paramBlock = pnodeFnc->sxFnc.pnodeScopes;
        Assert(paramBlock->nop == knopBlock && paramBlock->sxBlock.blockType == Parameter);

        PushScope(paramScope);

        // While emitting the functions we have to stop when we see the body scope block.
        // Otherwise functions defined in the body scope will not be able to get the right references.
        this->EmitScopeList(paramBlock->sxBlock.pnodeScopes, pnodeFnc->sxFnc.pnodeBodyScope);
        Assert(this->GetCurrentScope() == paramScope);
    }

    PushScope(bodyScope);
}

void ByteCodeGenerator::EmitModuleExportAccess(Symbol* sym, Js::OpCode opcode, Js::RegSlot location, FuncInfo* funcInfo)
{
    if (EnsureSymbolModuleSlots(sym, funcInfo))
    {TRACE_IT(39133);
        this->Writer()->SlotI2(opcode, location, sym->GetModuleIndex(), sym->GetScopeSlot());
    }
    else
    {TRACE_IT(39134);
        this->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(ERRInvalidExportName));

        if (opcode == Js::OpCode::LdModuleSlot)
        {TRACE_IT(39135);
            this->Writer()->Reg1(Js::OpCode::LdUndef, location);
        }
    }
}

bool ByteCodeGenerator::EnsureSymbolModuleSlots(Symbol* sym, FuncInfo* funcInfo)
{TRACE_IT(39136);
    Assert(sym->GetIsModuleExportStorage());

    if (sym->GetModuleIndex() != Js::Constants::NoProperty && sym->GetScopeSlot() != Js::Constants::NoProperty)
    {TRACE_IT(39137);
        return true;
    }

    Js::JavascriptLibrary* library = this->GetScriptContext()->GetLibrary();
    library->EnsureModuleRecordList();
    uint moduleIndex = this->GetModuleID();
    uint moduleSlotIndex;
    Js::SourceTextModuleRecord* moduleRecord = library->GetModuleRecord(moduleIndex);

    if (sym->GetIsModuleImport())
    {TRACE_IT(39138);
        Js::PropertyId localImportNameId = sym->EnsurePosition(funcInfo);
        Js::ModuleNameRecord* moduleNameRecord = nullptr;
        if (!moduleRecord->ResolveImport(localImportNameId, &moduleNameRecord))
        {TRACE_IT(39139);
            return false;
        }

        AnalysisAssert(moduleNameRecord != nullptr);
        Assert(moduleNameRecord->module->IsSourceTextModuleRecord());
        Js::SourceTextModuleRecord* resolvedModuleRecord =
            (Js::SourceTextModuleRecord*)PointerValue(moduleNameRecord->module);

        moduleIndex = resolvedModuleRecord->GetModuleId();
        moduleSlotIndex = resolvedModuleRecord->GetLocalExportSlotIndexByLocalName(moduleNameRecord->bindingName);
    }
    else
    {TRACE_IT(39140);
        Js::PropertyId exportNameId = sym->EnsurePosition(funcInfo);
        moduleSlotIndex = moduleRecord->GetLocalExportSlotIndexByLocalName(exportNameId);
    }

    sym->SetModuleIndex(moduleIndex);
    sym->SetScopeSlot(moduleSlotIndex);

    return true;
}

void ByteCodeGenerator::EmitAssignmentToDefaultModuleExport(ParseNode* pnode, FuncInfo* funcInfo)
{TRACE_IT(39141);
    // We are assigning pnode to the default export of the current module.
    uint moduleIndex = this->GetModuleID();

    Js::JavascriptLibrary* library = this->GetScriptContext()->GetLibrary();
    library->EnsureModuleRecordList();
    Js::SourceTextModuleRecord* moduleRecord = library->GetModuleRecord(moduleIndex);
    uint moduleSlotIndex = moduleRecord->GetLocalExportSlotIndexByExportName(Js::PropertyIds::default_);

    this->Writer()->SlotI2(Js::OpCode::StModuleSlot, pnode->location, moduleIndex, moduleSlotIndex);
}

void ByteCodeGenerator::EnsureLetConstScopeSlots(ParseNode *pnodeBlock, FuncInfo *funcInfo)
{TRACE_IT(39142);
    bool callsEval = pnodeBlock->sxBlock.GetCallsEval() || pnodeBlock->sxBlock.GetChildCallsEval();
    auto ensureLetConstSlots = ([this, funcInfo, callsEval](ParseNode *pnode)
    {
        Symbol *sym = pnode->sxVar.sym;
        if (callsEval || sym->NeedsSlotAlloc(funcInfo))
        {TRACE_IT(39143);
            sym->EnsureScopeSlot(funcInfo);
            this->ProcessCapturedSym(sym);
        }
    });
    IterateBlockScopedVariables(pnodeBlock, ensureLetConstSlots);
}

void ByteCodeGenerator::EnsureFncScopeSlots(ParseNode *pnode, FuncInfo *funcInfo)
{TRACE_IT(39144);
    while (pnode)
    {TRACE_IT(39145);
        switch (pnode->nop)
        {
        case knopFncDecl:
            if (pnode->sxFnc.IsDeclaration())
            {
                CheckFncDeclScopeSlot(pnode, funcInfo);
            }
            pnode = pnode->sxFnc.pnodeNext;
            break;
        case knopBlock:
            pnode = pnode->sxBlock.pnodeNext;
            break;
        case knopCatch:
            pnode = pnode->sxCatch.pnodeNext;
            break;
        case knopWith:
            pnode = pnode->sxWith.pnodeNext;
            break;
        }
    }
}

void ByteCodeGenerator::EndEmitFunction(ParseNode *pnodeFnc)
{TRACE_IT(39146);
    Assert(pnodeFnc->nop == knopFncDecl || pnodeFnc->nop == knopProg);
    Assert(pnodeFnc->nop == knopFncDecl && currentScope->GetEnclosingScope() != nullptr || pnodeFnc->nop == knopProg);

    PopScope(); // function body

    FuncInfo *funcInfo = pnodeFnc->sxFnc.funcInfo;

    Scope* paramScope = funcInfo->paramScope;
    if (!funcInfo->IsBodyAndParamScopeMerged())
    {TRACE_IT(39147);
        Assert(this->GetCurrentScope() == paramScope);
        PopScope(); // Pop the param scope
    }

    Scope *scope = funcInfo->funcExprScope;
    if (scope && scope->GetMustInstantiate())
    {TRACE_IT(39148);
        Assert(currentScope == scope);
        PopScope();
    }

    if (CONFIG_FLAG(DeferNested))
    {TRACE_IT(39149);
        Assert(funcInfo == this->TopFuncInfo());
        PopFuncInfo(_u("EndEmitFunction"));
    }
}

void ByteCodeGenerator::StartEmitCatch(ParseNode *pnodeCatch)
{TRACE_IT(39150);
    Assert(pnodeCatch->nop == knopCatch);

    Scope *scope = pnodeCatch->sxCatch.scope;
    FuncInfo *funcInfo = scope->GetFunc();

    // Catch scope is a dynamic object if it can be passed to a scoped lookup helper (i.e., eval is present or we're in an event handler).
    if (funcInfo->GetCallsEval() || funcInfo->GetChildCallsEval() || (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents)))
    {TRACE_IT(39151);
        scope->SetIsObject();
    }

    if (pnodeCatch->sxCatch.pnodeParam->nop == knopParamPattern)
    {TRACE_IT(39152);
        scope->SetCapturesAll(funcInfo->GetCallsEval() || funcInfo->GetChildCallsEval());
        scope->SetMustInstantiate(scope->Count() > 0 && (scope->GetMustInstantiate() || scope->GetCapturesAll() || funcInfo->IsGlobalFunction()));

        Parser::MapBindIdentifier(pnodeCatch->sxCatch.pnodeParam->sxParamPattern.pnode1, [&](ParseNodePtr item)
        {
            Symbol *sym = item->sxVar.sym;
            if (funcInfo->IsGlobalFunction())
            {TRACE_IT(39153);
                sym->SetIsGlobalCatch(true);
            }

            Assert(sym->GetScopeSlot() == Js::Constants::NoProperty);
            if (sym->NeedsSlotAlloc(funcInfo))
            {TRACE_IT(39154);
                sym->EnsureScopeSlot(funcInfo);
            }
        });

        // In the case of pattern we will always going to push the scope.
        PushScope(scope);
    }
    else
    {TRACE_IT(39155);
        Symbol *sym = pnodeCatch->sxCatch.pnodeParam->sxPid.sym;

        // Catch object is stored in the catch scope if there may be an ambiguous lookup or a var declaration that hides it.
        scope->SetCapturesAll(funcInfo->GetCallsEval() || funcInfo->GetChildCallsEval() || sym->GetHasNonLocalReference());
        scope->SetMustInstantiate(scope->GetCapturesAll() || funcInfo->IsGlobalFunction());

        if (funcInfo->IsGlobalFunction())
        {TRACE_IT(39156);
            sym->SetIsGlobalCatch(true);
        }

        if (scope->GetMustInstantiate())
        {TRACE_IT(39157);
            if (sym->IsInSlot(funcInfo))
            {TRACE_IT(39158);
                // Since there is only one symbol we are pushing to slot.
                // Also in order to make IsInSlot to return true - forcing the sym-has-non-local-reference.
                this->ProcessCapturedSym(sym);
                sym->EnsureScopeSlot(funcInfo);
            }
        }

        PushScope(scope);
    }
}

void ByteCodeGenerator::EndEmitCatch(ParseNode *pnodeCatch)
{TRACE_IT(39159);
    Assert(pnodeCatch->nop == knopCatch);
    Assert(currentScope == pnodeCatch->sxCatch.scope);
    PopScope();
}

void ByteCodeGenerator::StartEmitBlock(ParseNode *pnodeBlock)
{
    if (!BlockHasOwnScope(pnodeBlock, this))
    {TRACE_IT(39160);
        return;
    }

    Assert(pnodeBlock->nop == knopBlock);

    PushBlock(pnodeBlock);

    Scope *scope = pnodeBlock->sxBlock.scope;
    if (pnodeBlock->sxBlock.GetCallsEval() || pnodeBlock->sxBlock.GetChildCallsEval() || (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents)))
    {TRACE_IT(39161);
        Assert(scope->GetIsObject());
    }

    // TODO: Consider nested deferred parsing.
    if (scope->GetMustInstantiate())
    {TRACE_IT(39162);
        FuncInfo *funcInfo = scope->GetFunc();
        if (scope->IsGlobalEvalBlockScope() && funcInfo->isThisLexicallyCaptured)
        {TRACE_IT(39163);
            funcInfo->EnsureThisScopeSlot();
        }
        this->EnsureFncScopeSlots(pnodeBlock->sxBlock.pnodeScopes, funcInfo);
        this->EnsureLetConstScopeSlots(pnodeBlock, funcInfo);
        PushScope(scope);
    }
}

void ByteCodeGenerator::EndEmitBlock(ParseNode *pnodeBlock)
{
    if (!BlockHasOwnScope(pnodeBlock, this))
    {TRACE_IT(39164);
        return;
    }

    Assert(pnodeBlock->nop == knopBlock);

    Scope *scope = pnodeBlock->sxBlock.scope;
    if (scope && scope->GetMustInstantiate())
    {TRACE_IT(39165);
        Assert(currentScope == pnodeBlock->sxBlock.scope);
        PopScope();
    }

    PopBlock();
}

void ByteCodeGenerator::StartEmitWith(ParseNode *pnodeWith)
{TRACE_IT(39166);
    Assert(pnodeWith->nop == knopWith);

    Scope *scope = pnodeWith->sxWith.scope;

    Assert(scope->GetIsObject());

    PushScope(scope);
}

void ByteCodeGenerator::EndEmitWith(ParseNode *pnodeWith)
{TRACE_IT(39167);
    Assert(pnodeWith->nop == knopWith);
    Assert(currentScope == pnodeWith->sxWith.scope);

    PopScope();
}

Js::RegSlot ByteCodeGenerator::PrependLocalScopes(Js::RegSlot evalEnv, Js::RegSlot tempLoc, FuncInfo *funcInfo)
{TRACE_IT(39168);
    Scope *currScope = this->currentScope;
    Scope *funcScope = funcInfo->GetCurrentChildScope() ? funcInfo->GetCurrentChildScope() : funcInfo->GetBodyScope();

    if (currScope == funcScope)
    {TRACE_IT(39169);
        return evalEnv;
    }

    bool acquireTempLoc = tempLoc == Js::Constants::NoRegister;
    if (acquireTempLoc)
    {TRACE_IT(39170);
        tempLoc = funcInfo->AcquireTmpRegister();
    }

    // The with/catch objects must be prepended to the environment we pass to eval() or to a func declared inside with,
    // but the list must first be reversed so that innermost scopes appear first in the list.
    while (currScope != funcScope)
    {TRACE_IT(39171);
        Scope *innerScope;
        for (innerScope = currScope; innerScope->GetEnclosingScope() != funcScope; innerScope = innerScope->GetEnclosingScope())
            ;
        if (innerScope->GetMustInstantiate())
        {TRACE_IT(39172);
            if (!innerScope->HasInnerScopeIndex())
            {TRACE_IT(39173);
                if (evalEnv == funcInfo->GetEnvRegister() || evalEnv == funcInfo->frameDisplayRegister)
                {TRACE_IT(39174);
                    this->m_writer.Reg2(Js::OpCode::LdInnerFrameDisplayNoParent, tempLoc, innerScope->GetLocation());
                }
                else
                {TRACE_IT(39175);
                    this->m_writer.Reg3(Js::OpCode::LdInnerFrameDisplay, tempLoc, innerScope->GetLocation(), evalEnv);
                }
            }
            else
            {TRACE_IT(39176);
                if (evalEnv == funcInfo->GetEnvRegister() || evalEnv == funcInfo->frameDisplayRegister)
                {TRACE_IT(39177);
                    this->m_writer.Reg1Unsigned1(Js::OpCode::LdIndexedFrameDisplayNoParent, tempLoc, innerScope->GetInnerScopeIndex());
                }
                else
                {TRACE_IT(39178);
                    this->m_writer.Reg2Int1(Js::OpCode::LdIndexedFrameDisplay, tempLoc, evalEnv, innerScope->GetInnerScopeIndex());
                }
            }
            evalEnv = tempLoc;
        }
        funcScope = innerScope;
    }

    if (acquireTempLoc)
    {TRACE_IT(39179);
        funcInfo->ReleaseTmpRegister(tempLoc);
    }
    return evalEnv;
}

void ByteCodeGenerator::EmitLoadInstance(Symbol *sym, IdentPtr pid, Js::RegSlot *pThisLocation, Js::RegSlot *pInstLocation, FuncInfo *funcInfo)
{TRACE_IT(39180);
    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    Js::RegSlot thisLocation = *pThisLocation;
    Js::RegSlot instLocation = *pInstLocation;
    Js::PropertyId envIndex = -1;
    Scope *scope = nullptr;
    Scope *symScope = sym ? sym->GetScope() : this->globalScope;
    Assert(symScope);

    if (sym != nullptr && sym->GetIsModuleExportStorage())
    {TRACE_IT(39181);
        *pInstLocation = Js::Constants::NoRegister;
        return;
    }

    for (;;)
    {TRACE_IT(39182);
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {TRACE_IT(39183);
            break;
        }

        if (scope != symScope)
        {TRACE_IT(39184);
            // We're not sure where the function is (eval/with/etc).
            // So we're going to need registers to hold the instance where we (dynamically) find
            // the function, and possibly to hold the "this" pointer we will pass to it.
            // Assign them here so that they can't overlap with the scopeLocation assigned below.
            // Otherwise we wind up with temp lifetime confusion in the IRBuilder. (Win8 281689)
            if (instLocation == Js::Constants::NoRegister)
            {TRACE_IT(39185);
                instLocation = funcInfo->AcquireTmpRegister();
                // The "this" pointer will not be the same as the instance, so give it its own register.
                thisLocation = funcInfo->AcquireTmpRegister();
            }
        }

        if (envIndex == -1)
        {TRACE_IT(39186);
            Assert(funcInfo == scope->GetFunc());
            scopeLocation = scope->GetLocation();
        }

        if (scope == symScope)
        {TRACE_IT(39187);
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {TRACE_IT(39188);
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }

        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {TRACE_IT(39189);
            this->m_writer.BrEnvProperty(
                Js::OpCode::BrOnNoEnvProperty, nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();

            Assert(scope->GetIsObject());
            this->m_writer.SlotI1(Js::OpCode::LdEnvObj, tmpReg,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            Js::OpCode op = unwrapWithObj ? Js::OpCode::UnwrapWithObj : Js::OpCode::Ld_A;

            this->m_writer.Reg2(op, instLocation, tmpReg);
            if (thisLocation != Js::Constants::NoRegister)
            {TRACE_IT(39190);
                this->m_writer.Reg2(op, thisLocation, tmpReg);
            }

            funcInfo->ReleaseTmpRegister(tmpReg);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {TRACE_IT(39191);
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            Assert(scope->GetIsObject());
            this->m_writer.Reg1(Js::OpCode::LdLocalObj, instLocation);
            if (thisLocation != Js::Constants::NoRegister)
            {TRACE_IT(39192);
                this->m_writer.Reg1(Js::OpCode::LdLocalObj, thisLocation);
            }
        }
        else
        {TRACE_IT(39193);
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Js::OpCode op = unwrapWithObj ? Js::OpCode::UnwrapWithObj : Js::OpCode::Ld_A;
            this->m_writer.Reg2(op, instLocation, scopeLocation);
            if (thisLocation != Js::Constants::NoRegister)
            {TRACE_IT(39194);
                this->m_writer.Reg2(op, thisLocation, scopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    if (sym == nullptr || sym->GetIsGlobal())
    {TRACE_IT(39195);
        if (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents))
        {TRACE_IT(39196);
            // Load of a symbol with unknown scope from within eval.
            // Get it from the closure environment.
            if (instLocation == Js::Constants::NoRegister)
            {TRACE_IT(39197);
                instLocation = funcInfo->AcquireTmpRegister();
            }

            // TODO: It should be possible to avoid this double call to ScopedLdInst by having it return both
            // results at once. The reason for the uncertainty here is that we don't know whether the callee
            // belongs to a "with" object. If it does, we have to pass the "with" object as "this"; in all other
            // cases, we pass "undefined". For now, there are apparently no significant performance issues.
            Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

            if (thisLocation == Js::Constants::NoRegister)
            {TRACE_IT(39198);
                thisLocation = funcInfo->AcquireTmpRegister();
            }
            this->m_writer.ScopedProperty2(Js::OpCode::ScopedLdInst, instLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId), thisLocation);
        }
        else
        {TRACE_IT(39199);
            if (instLocation == Js::Constants::NoRegister)
            {TRACE_IT(39200);
                instLocation = ByteCodeGenerator::RootObjectRegister;
            }
            else
            {TRACE_IT(39201);
                this->m_writer.Reg2(Js::OpCode::Ld_A, instLocation, ByteCodeGenerator::RootObjectRegister);
            }

            if (thisLocation == Js::Constants::NoRegister)
            {TRACE_IT(39202);
                thisLocation = funcInfo->undefinedConstantRegister;
            }
            else
            {TRACE_IT(39203);
                this->m_writer.Reg2(Js::OpCode::Ld_A, thisLocation, funcInfo->undefinedConstantRegister);
            }
        }
    }
    else if (instLocation != Js::Constants::NoRegister)
    {TRACE_IT(39204);
        if (envIndex != -1)
        {TRACE_IT(39205);
            Assert(scope->GetIsObject());
            this->m_writer.SlotI1(Js::OpCode::LdEnvObj, instLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));
        }
        else if (scope->HasInnerScopeIndex())
        {TRACE_IT(39206);
            this->m_writer.Reg1Unsigned1(Js::OpCode::LdInnerScope, instLocation, scope->GetInnerScopeIndex());
        }
        else if (symScope == funcInfo->GetParamScope())
        {TRACE_IT(39207);
            Assert(funcInfo->frameObjRegister != Js::Constants::NoRegister && !funcInfo->IsBodyAndParamScopeMerged());
            this->m_writer.Reg1(Js::OpCode::LdParamObj, instLocation);
        }
        else if (symScope != funcInfo->GetBodyScope())
        {TRACE_IT(39208);
            this->m_writer.Reg2(Js::OpCode::Ld_A, instLocation, scopeLocation);
        }
        else
        {TRACE_IT(39209);
            Assert(funcInfo->frameObjRegister != Js::Constants::NoRegister);
            this->m_writer.Reg1(Js::OpCode::LdLocalObj, instLocation);
        }

        if (thisLocation != Js::Constants::NoRegister)
        {TRACE_IT(39210);
            this->m_writer.Reg2(Js::OpCode::Ld_A, thisLocation, funcInfo->undefinedConstantRegister);
        }
        else
        {TRACE_IT(39211);
            thisLocation = funcInfo->undefinedConstantRegister;
        }
    }

    *pThisLocation = thisLocation;
    *pInstLocation = instLocation;

    if (fLabelDefined)
    {TRACE_IT(39212);
        this->m_writer.MarkLabel(doneLabel);
    }
}

void ByteCodeGenerator::EmitGlobalFncDeclInit(Js::RegSlot rhsLocation, Js::PropertyId propertyId, FuncInfo * funcInfo)
{TRACE_IT(39213);
    // Note: declared variables and assignments in the global function go to the root object directly.
    if (this->flags & fscrEval)
    {TRACE_IT(39214);
        // Func decl's always get their init values before any use, so we don't pre-initialize the property to undef.
        // That means that we have to use ScopedInitFld so that we initialize the property on the right instance
        // even if the instance doesn't have the property yet (i.e., collapse the init-to-undef and the store
        // into one operation). See WOOB 1121763 and 1120973.
        this->m_writer.ScopedProperty(Js::OpCode::ScopedInitFunc, rhsLocation,
            funcInfo->FindOrAddReferencedPropertyId(propertyId));
    }
    else
    {TRACE_IT(39215);
        this->EmitPatchableRootProperty(Js::OpCode::InitRootFld, rhsLocation, propertyId, false, true, funcInfo);
    }
}

void
ByteCodeGenerator::EmitPatchableRootProperty(Js::OpCode opcode,
    Js::RegSlot regSlot, Js::PropertyId propertyId, bool isLoadMethod, bool isStore, FuncInfo * funcInfo)
{TRACE_IT(39216);
    uint cacheId = funcInfo->FindOrAddRootObjectInlineCacheId(propertyId, isLoadMethod, isStore);
    this->m_writer.PatchableRootProperty(opcode, regSlot, cacheId, isLoadMethod, isStore);
}

void ByteCodeGenerator::EmitLocalPropInit(Js::RegSlot rhsLocation, Symbol *sym, FuncInfo *funcInfo)
{TRACE_IT(39217);
    Scope *scope = sym->GetScope();

    // Check consistency of sym->IsInSlot.
    Assert(sym->NeedsSlotAlloc(funcInfo) || sym->GetScopeSlot() == Js::Constants::NoProperty);

    // Arrived at the scope in which the property was defined.
    if (sym->NeedsSlotAlloc(funcInfo))
    {TRACE_IT(39218);
        // The property is in memory rather than register. We'll have to load it from the slots.
        if (scope->GetIsObject())
        {TRACE_IT(39219);
            Assert(!this->TopFuncInfo()->GetParsedFunctionBody()->DoStackNestedFunc());
            Js::PropertyId propertyId = sym->EnsurePosition(this);
            Js::RegSlot objReg;
            if (scope->HasInnerScopeIndex())
            {TRACE_IT(39220);
                objReg = funcInfo->InnerScopeToRegSlot(scope);
            }
            else
            {TRACE_IT(39221);
                objReg = scope->GetLocation();
            }
            uint cacheId = funcInfo->FindOrAddInlineCacheId(objReg, propertyId, false, true);
            Js::OpCode op = this->GetInitFldOp(scope, objReg, funcInfo, sym->GetIsNonSimpleParameter());
            if (objReg != Js::Constants::NoRegister && objReg == funcInfo->frameObjRegister)
            {TRACE_IT(39222);
                this->m_writer.ElementP(op, rhsLocation, cacheId);
            }
            else if (scope->HasInnerScopeIndex())
            {TRACE_IT(39223);
                this->m_writer.ElementPIndexed(op, rhsLocation, scope->GetInnerScopeIndex(), cacheId);
            }
            else
            {TRACE_IT(39224);
                this->m_writer.PatchableProperty(op, rhsLocation, scope->GetLocation(), cacheId);
            }
        }
        else
        {TRACE_IT(39225);
            // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
            Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
            Js::RegSlot slotReg = scope->GetCanMerge() ? funcInfo->frameSlotsRegister : scope->GetLocation();
            // Now store the property to its slot.
            Js::OpCode op = this->GetStSlotOp(scope, -1, slotReg, false, funcInfo);

            if (slotReg != Js::Constants::NoRegister && slotReg == funcInfo->frameSlotsRegister)
            {TRACE_IT(39226);
                this->m_writer.SlotI1(op, rhsLocation, slot + Js::ScopeSlots::FirstSlotIndex);
            }
            else
            {TRACE_IT(39227);
                this->m_writer.SlotI2(op, rhsLocation, scope->GetInnerScopeIndex(), slot + Js::ScopeSlots::FirstSlotIndex);
            }
        }
    }
    if (sym->GetLocation() != Js::Constants::NoRegister && rhsLocation != sym->GetLocation())
    {TRACE_IT(39228);
        this->m_writer.Reg2(Js::OpCode::Ld_A, sym->GetLocation(), rhsLocation);
    }
}

Js::OpCode
ByteCodeGenerator::GetStSlotOp(Scope *scope, int envIndex, Js::RegSlot scopeLocation, bool chkBlockVar, FuncInfo *funcInfo)
{TRACE_IT(39229);
    Js::OpCode op;

    if (envIndex != -1)
    {TRACE_IT(39230);
        if (scope->GetIsObject())
        {TRACE_IT(39231);
            op = Js::OpCode::StEnvObjSlot;
        }
        else
        {TRACE_IT(39232);
            op = Js::OpCode::StEnvSlot;
        }
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
        scopeLocation == funcInfo->frameSlotsRegister)
    {TRACE_IT(39233);
        if (scope->GetScopeType() == ScopeType_Parameter && scope != scope->GetFunc()->GetCurrentChildScope())
        {TRACE_IT(39234);
            // Symbol is from the param scope of a split scope function and we are emitting the body.
            // We should use the param scope's bytecode now.
            Assert(!funcInfo->IsBodyAndParamScopeMerged());
            op = Js::OpCode::StParamSlot;
        }
        else
        {TRACE_IT(39235);
            op = Js::OpCode::StLocalSlot;
        }
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
        scopeLocation == funcInfo->frameObjRegister)
    {TRACE_IT(39236);
        if (scope->GetScopeType() == ScopeType_Parameter && scope != scope->GetFunc()->GetCurrentChildScope())
        {TRACE_IT(39237);
            // Symbol is from the param scope of a split scope function and we are emitting the body.
            // We should use the param scope's bytecode now.
            Assert(!funcInfo->IsBodyAndParamScopeMerged());
            op = Js::OpCode::StParamObjSlot;
        }
        else
        {TRACE_IT(39238);
            op = Js::OpCode::StLocalObjSlot;
        }
    }
    else
    {TRACE_IT(39239);
        Assert(scope->HasInnerScopeIndex());
        if (scope->GetIsObject())
        {TRACE_IT(39240);
            op = Js::OpCode::StInnerObjSlot;
        }
        else
        {TRACE_IT(39241);
            op = Js::OpCode::StInnerSlot;
        }
    }

    if (chkBlockVar)
    {TRACE_IT(39242);
        op = this->ToChkUndeclOp(op);
    }

    return op;
}

Js::OpCode
ByteCodeGenerator::GetInitFldOp(Scope *scope, Js::RegSlot scopeLocation, FuncInfo *funcInfo, bool letDecl)
{TRACE_IT(39243);
    Js::OpCode op;

    if (scopeLocation != Js::Constants::NoRegister &&
        scopeLocation == funcInfo->frameObjRegister)
    {TRACE_IT(39244);
        op = letDecl ? Js::OpCode::InitLocalLetFld : Js::OpCode::InitLocalFld;
    }
    else if (scope->HasInnerScopeIndex())
    {TRACE_IT(39245);
        op = letDecl ? Js::OpCode::InitInnerLetFld : Js::OpCode::InitInnerFld;
    }
    else
    {TRACE_IT(39246);
        op = letDecl ? Js::OpCode::InitLetFld : Js::OpCode::InitFld;
    }

    return op;
}

void ByteCodeGenerator::EmitPropStore(Js::RegSlot rhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo, bool isLetDecl, bool isConstDecl, bool isFncDeclVar)
{TRACE_IT(39247);
    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::PropertyId envIndex = -1;
    Scope *symScope = sym == nullptr || sym->GetIsGlobal() ? this->globalScope : sym->GetScope();
    Assert(symScope);
    // isFncDeclVar denotes that the symbol being stored to here is the var
    // binding of a function declaration and we know we want to store directly
    // to it, skipping over any dynamic scopes that may lie in between.
    Scope *scope = nullptr;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    bool scopeAcquired = false;
    Js::OpCode op;

    if (sym && sym->GetIsModuleExportStorage())
    {TRACE_IT(39248);
        if (!isConstDecl && sym->GetDecl() && sym->GetDecl()->nop == knopConstDecl)
        {TRACE_IT(39249);
            this->m_writer.W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(ERRAssignmentToConst));
        }

        EmitModuleExportAccess(sym, Js::OpCode::StModuleSlot, rhsLocation, funcInfo);
        return;
    }

    if (isFncDeclVar)
    {TRACE_IT(39250);
        // async functions allow for the fncDeclVar to be in the body or parameter scope
        // of the parent function, so we need to calculate envIndex in lieu of the while
        // loop below.
        do
        {TRACE_IT(39251);
            scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        } while (scope != symScope);
        Assert(scope == symScope);
        scopeLocation = scope->GetLocation();
    }

    while (!isFncDeclVar)
    {TRACE_IT(39252);
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {TRACE_IT(39253);
            break;
        }
        if (envIndex == -1)
        {TRACE_IT(39254);
            Assert(funcInfo == scope->GetFunc());
            scopeLocation = scope->GetLocation();
        }

        if (scope == symScope)
        {TRACE_IT(39255);
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {TRACE_IT(39256);
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }
        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

        Js::RegSlot unwrappedScopeLocation = scopeLocation;
        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {TRACE_IT(39257);
            this->m_writer.BrEnvProperty(
                Js::OpCode::BrOnNoEnvProperty,
                nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            Js::RegSlot instLocation = funcInfo->AcquireTmpRegister();

            Assert(scope->GetIsObject());
            this->m_writer.SlotI1(
                Js::OpCode::LdEnvObj,
                instLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            if (unwrapWithObj)
            {TRACE_IT(39258);
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, instLocation, instLocation);
            }

            this->m_writer.PatchableProperty(
                Js::OpCode::StFld,
                rhsLocation,
                instLocation,
                funcInfo->FindOrAddInlineCacheId(instLocation, propertyId, false, true));

            funcInfo->ReleaseTmpRegister(instLocation);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {TRACE_IT(39259);
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->m_writer.ElementP(Js::OpCode::StLocalFld, rhsLocation,
                funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true));
        }
        else
        {TRACE_IT(39260);
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            if (unwrapWithObj)
            {TRACE_IT(39261);
                unwrappedScopeLocation = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, unwrappedScopeLocation, scopeLocation);
                scopeLocation = unwrappedScopeLocation;
            }

            uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, true);
            this->m_writer.PatchableProperty(Js::OpCode::StFld, rhsLocation, scopeLocation, cacheId);

            if (unwrapWithObj)
            {TRACE_IT(39262);
                funcInfo->ReleaseTmpRegister(unwrappedScopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    // Arrived at the scope in which the property was defined.
    if (sym && sym->GetNeedDeclaration() && scope->GetFunc() == funcInfo)
    {
        EmitUseBeforeDeclarationRuntimeError(this, Js::Constants::NoRegister);
        // Intentionally continue on to do normal EmitPropStore behavior so
        // that the bytecode ends up well-formed for the backend.  This is
        // in contrast to EmitPropLoad and EmitPropTypeof where they both
        // tell EmitUseBeforeDeclarationRuntimeError to emit a LdUndef in place
        // of their load and then they skip emitting their own bytecode.
        // Potayto potahto.
    }

    if (sym == nullptr || sym->GetIsGlobal())
    {TRACE_IT(39263);
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        if (this->flags & fscrEval)
        {TRACE_IT(39264);
            if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
            {TRACE_IT(39265);
                uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->frameDisplayRegister, propertyId, false, true);
                this->m_writer.ElementP(GetScopedStFldOpCode(funcInfo), rhsLocation, cacheId);
            }
            else
            {TRACE_IT(39266);
                uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->GetEnvRegister(), propertyId, false, true);
                bool isConsoleScopeLetConst = this->IsConsoleScopeEval() && (isLetDecl || isConstDecl);
                // In "eval", store to a symbol with unknown scope goes through the closure environment.
                this->m_writer.ElementP(GetScopedStFldOpCode(funcInfo, isConsoleScopeLetConst), rhsLocation, cacheId);
            }
        }
        else if (this->flags & (fscrImplicitThis | fscrImplicitParents))
        {TRACE_IT(39267);
            uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->GetEnvRegister(), propertyId, false, true);

            // In "eval", store to a symbol with unknown scope goes through the closure environment.
            this->m_writer.ElementP(GetScopedStFldOpCode(funcInfo), rhsLocation, cacheId);
        }
        else
        {TRACE_IT(39268);
            this->EmitPatchableRootProperty(GetStFldOpCode(funcInfo, true, isLetDecl, isConstDecl, false), rhsLocation, propertyId, false, true, funcInfo);
        }
    }
    else if (sym->GetIsFuncExpr())
    {TRACE_IT(39269);
        // Store to function expr variable.

        // strict mode: we need to throw type error
        if (funcInfo->byteCodeFunction->GetIsStrictMode())
        {TRACE_IT(39270);
            // Note that in this case the sym's location belongs to the parent function, so we can't use it.
            // It doesn't matter which register we use, as long as it's valid for this function.
            this->m_writer.W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_CantAssignToReadOnly));
        }
    }
    else if (sym->IsInSlot(funcInfo) || envIndex != -1)
    {TRACE_IT(39271);
        if (!isConstDecl && sym->GetDecl() && sym->GetDecl()->nop == knopConstDecl)
        {TRACE_IT(39272);
            // This is a case where const reassignment can't be proven statically (e.g., eval, with) so
            // we have to catch it at runtime.
            this->m_writer.W1(
                Js::OpCode::RuntimeTypeError, SCODE_CODE(ERRAssignmentToConst));
        }
        // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
        Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
        bool chkBlockVar = !isLetDecl && !isConstDecl && NeedCheckBlockVar(sym, scope, funcInfo);

        // The property is in memory rather than register. We'll have to load it from the slots.
        op = this->GetStSlotOp(scope, envIndex, scopeLocation, chkBlockVar, funcInfo);

        if (envIndex != -1)
        {TRACE_IT(39273);
            this->m_writer.SlotI2(op, rhsLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var),
                slot + (sym->GetScope()->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex));
        }
        else if (scopeLocation != Js::Constants::NoRegister &&
            (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
        {TRACE_IT(39274);
            this->m_writer.SlotI1(op, rhsLocation,
                slot + (sym->GetScope()->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex));
        }
        else
        {TRACE_IT(39275);
            Assert(scope->HasInnerScopeIndex());
            this->m_writer.SlotI2(op, rhsLocation, scope->GetInnerScopeIndex(),
                slot + (sym->GetScope()->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex));
        }

        if (this->ShouldTrackDebuggerMetadata() && (isLetDecl || isConstDecl))
        {TRACE_IT(39276);
            Js::PropertyId location = scope->GetIsObject() ? sym->GetLocation() : slot;
            this->UpdateDebuggerPropertyInitializationOffset(location, sym->GetPosition(), false);
        }
    }
    else if (isConstDecl)
    {TRACE_IT(39277);
        this->m_writer.Reg2(Js::OpCode::InitConst, sym->GetLocation(), rhsLocation);

        if (this->ShouldTrackDebuggerMetadata())
        {TRACE_IT(39278);
            this->UpdateDebuggerPropertyInitializationOffset(sym->GetLocation(), sym->GetPosition());
        }
    }
    else
    {TRACE_IT(39279);
        if (!isConstDecl && sym->GetDecl() && sym->GetDecl()->nop == knopConstDecl)
        {TRACE_IT(39280);
            // This is a case where const reassignment can't be proven statically (e.g., eval, with) so
            // we have to catch it at runtime.
            this->m_writer.W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(ERRAssignmentToConst));
        }
        if (rhsLocation != sym->GetLocation())
        {TRACE_IT(39281);
            this->m_writer.Reg2(Js::OpCode::Ld_A, sym->GetLocation(), rhsLocation);

            if (this->ShouldTrackDebuggerMetadata() && isLetDecl)
            {TRACE_IT(39282);
                this->UpdateDebuggerPropertyInitializationOffset(sym->GetLocation(), sym->GetPosition());
            }
        }
    }
    if (fLabelDefined)
    {TRACE_IT(39283);
        this->m_writer.MarkLabel(doneLabel);
    }

    if (scopeAcquired)
    {TRACE_IT(39284);
        funcInfo->ReleaseTmpRegister(scopeLocation);
    }
}

Js::OpCode
ByteCodeGenerator::GetLdSlotOp(Scope *scope, int envIndex, Js::RegSlot scopeLocation, FuncInfo *funcInfo)
{TRACE_IT(39285);
    Js::OpCode op;

    if (envIndex != -1)
    {TRACE_IT(39286);
        if (scope->GetIsObject())
        {TRACE_IT(39287);
            op = Js::OpCode::LdEnvObjSlot;
        }
        else
        {TRACE_IT(39288);
            op = Js::OpCode::LdEnvSlot;
        }
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
        scopeLocation == funcInfo->frameSlotsRegister)
    {TRACE_IT(39289);
        if (scope->GetScopeType() == ScopeType_Parameter && scope != scope->GetFunc()->GetCurrentChildScope())
        {TRACE_IT(39290);
            // Symbol is from the param scope of a split scope function and we are emitting the body.
            // We should use the param scope's bytecode now.
            Assert(!funcInfo->IsBodyAndParamScopeMerged());
            op = Js::OpCode::LdParamSlot;
        }
        else
        {TRACE_IT(39291);
            op = Js::OpCode::LdLocalSlot;
        }
    }
    else if (scopeLocation != Js::Constants::NoRegister &&
        scopeLocation == funcInfo->frameObjRegister)
    {TRACE_IT(39292);
        if (scope->GetScopeType() == ScopeType_Parameter && scope != scope->GetFunc()->GetCurrentChildScope())
        {TRACE_IT(39293);
            // Symbol is from the param scope of a split scope function and we are emitting the body.
            // We should use the param scope's bytecode now.
            Assert(!funcInfo->IsBodyAndParamScopeMerged());
            op = Js::OpCode::LdParamObjSlot;
        }
        else
        {TRACE_IT(39294);
            op = Js::OpCode::LdLocalObjSlot;
        }
    }
    else if (scope->HasInnerScopeIndex())
    {TRACE_IT(39295);
        if (scope->GetIsObject())
        {TRACE_IT(39296);
            op = Js::OpCode::LdInnerObjSlot;
        }
        else
        {TRACE_IT(39297);
            op = Js::OpCode::LdInnerSlot;
        }
    }
    else
    {TRACE_IT(39298);
        Assert(scope->GetIsObject());
        op = Js::OpCode::LdObjSlot;
    }

    return op;
}

void ByteCodeGenerator::EmitPropLoad(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo)
{TRACE_IT(39299);
    // If sym belongs to a parent frame, get it from the closure environment.
    // If it belongs to this func, but there's a non-local reference, get it from the heap-allocated frame.
    // (TODO: optimize this by getting the sym from its normal location if there are no non-local defs.)
    // Otherwise, just copy the value to the lhsLocation.

    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    Js::PropertyId envIndex = -1;
    Scope *scope = nullptr;
    Scope *symScope = sym ? sym->GetScope() : this->globalScope;
    Assert(symScope);

    if (sym && sym->GetIsModuleExportStorage())
    {
        EmitModuleExportAccess(sym, Js::OpCode::LdModuleSlot, lhsLocation, funcInfo);
        return;
    }

    for (;;)
    {TRACE_IT(39300);
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {TRACE_IT(39301);
            break;
        }

        scopeLocation = scope->GetLocation();

        if (scope == symScope)
        {TRACE_IT(39302);
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {TRACE_IT(39303);
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }

        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

        Js::RegSlot unwrappedScopeLocation = Js::Constants::NoRegister;
        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {TRACE_IT(39304);
            this->m_writer.BrEnvProperty(
                Js::OpCode::BrOnNoEnvProperty,
                nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            Js::RegSlot instLocation = funcInfo->AcquireTmpRegister();

            Assert(scope->GetIsObject());
            this->m_writer.SlotI1(
                Js::OpCode::LdEnvObj,
                instLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            if (unwrapWithObj)
            {TRACE_IT(39305);
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, instLocation, instLocation);
            }

            this->m_writer.PatchableProperty(
                Js::OpCode::LdFld,
                lhsLocation,
                instLocation,
                funcInfo->FindOrAddInlineCacheId(instLocation, propertyId, false, false));

            funcInfo->ReleaseTmpRegister(instLocation);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {TRACE_IT(39306);
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->m_writer.ElementP(Js::OpCode::LdLocalFld, lhsLocation,
                funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, false));
        }
        else
        {TRACE_IT(39307);
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            if (unwrapWithObj)
            {TRACE_IT(39308);
                unwrappedScopeLocation = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, unwrappedScopeLocation, scopeLocation);
                scopeLocation = unwrappedScopeLocation;
            }

            uint cacheId = funcInfo->FindOrAddInlineCacheId(scopeLocation, propertyId, false, false);
            this->m_writer.PatchableProperty(Js::OpCode::LdFld, lhsLocation, scopeLocation, cacheId);

            if (unwrapWithObj)
            {TRACE_IT(39309);
                funcInfo->ReleaseTmpRegister(unwrappedScopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    // Arrived at the scope in which the property was defined.
    if (sym && sym->GetNeedDeclaration() && scope->GetFunc() == funcInfo)
    {TRACE_IT(39310);
        // Ensure this symbol has a slot if it needs one.
        if (sym->IsInSlot(funcInfo))
        {TRACE_IT(39311);
            Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
            funcInfo->FindOrAddSlotProfileId(scope, slot);
        }

        EmitUseBeforeDeclarationRuntimeError(this, lhsLocation);
    }
    else if (sym == nullptr || sym->GetIsGlobal())
    {TRACE_IT(39312);
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        if (this->flags & fscrEval)
        {TRACE_IT(39313);
            if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
            {TRACE_IT(39314);
                uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->frameDisplayRegister, propertyId, false, false);
                this->m_writer.ElementP(Js::OpCode::ScopedLdFld, lhsLocation, cacheId);
            }
            else
            {TRACE_IT(39315);
                uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->GetEnvRegister(), propertyId, false, false);

                // Load of a symbol with unknown scope from within eval
                // Get it from the closure environment.
                this->m_writer.ElementP(Js::OpCode::ScopedLdFld, lhsLocation, cacheId);
            }
        }
        else if (this->flags & (fscrImplicitThis | fscrImplicitParents))
        {TRACE_IT(39316);
            uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->GetEnvRegister(), propertyId, false, false);

            // Load of a symbol with unknown scope from within eval or event handler.
            // Get it from the closure environment.
            this->m_writer.ElementP(Js::OpCode::ScopedLdFld, lhsLocation, cacheId);
        }
        else
        {TRACE_IT(39317);
            // Special case non-writable built-ins
            // TODO: support non-writable global property in general by detecting what attribute the property have current?
            // But can't be done if we are byte code serialized, because the attribute might be different for use fields
            // next time we run. May want to catch that in the JIT.
            Js::OpCode opcode = Js::OpCode::LdRootFld;

            // These properties are non-writable
            switch (propertyId)
            {
            case Js::PropertyIds::NaN:
                opcode = Js::OpCode::LdNaN;
                break;
            case Js::PropertyIds::Infinity:
                opcode = Js::OpCode::LdInfinity;
                break;
            case Js::PropertyIds::undefined:
                opcode = Js::OpCode::LdUndef;
                break;
            }

            if (opcode == Js::OpCode::LdRootFld)
            {TRACE_IT(39318);
                this->EmitPatchableRootProperty(Js::OpCode::LdRootFld, lhsLocation, propertyId, false, false, funcInfo);
            }
            else
            {TRACE_IT(39319);
                this->Writer()->Reg1(opcode, lhsLocation);
            }
        }
    }
    else if (sym->IsInSlot(funcInfo) || envIndex != -1)
    {TRACE_IT(39320);
        // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
        Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
        Js::ProfileId profileId = funcInfo->FindOrAddSlotProfileId(scope, slot);
        bool chkBlockVar = NeedCheckBlockVar(sym, scope, funcInfo);
        Js::OpCode op;

        // Now get the property from its slot.
        op = this->GetLdSlotOp(scope, envIndex, scopeLocation, funcInfo);
        slot = slot + (sym->GetScope()->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

        if (envIndex != -1)
        {TRACE_IT(39321);
            this->m_writer.SlotI2(op, lhsLocation, envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var), slot, profileId);
        }
        else if (scopeLocation != Js::Constants::NoRegister &&
            (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
        {TRACE_IT(39322);
            this->m_writer.SlotI1(op, lhsLocation, slot, profileId);
        }
        else if (scope->HasInnerScopeIndex())
        {TRACE_IT(39323);
            this->m_writer.SlotI2(op, lhsLocation, scope->GetInnerScopeIndex(), slot, profileId);
        }
        else
        {TRACE_IT(39324);
            Assert(scope->GetIsObject());
            this->m_writer.Slot(op, lhsLocation, scopeLocation, slot, profileId);
        }

        if (chkBlockVar)
        {TRACE_IT(39325);
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, lhsLocation);
        }
    }
    else
    {TRACE_IT(39326);
        if (lhsLocation != sym->GetLocation())
        {TRACE_IT(39327);
            this->m_writer.Reg2(Js::OpCode::Ld_A, lhsLocation, sym->GetLocation());
        }
        if (sym->GetIsBlockVar() && ((sym->GetDecl()->nop == knopLetDecl || sym->GetDecl()->nop == knopConstDecl) && sym->GetDecl()->sxVar.isSwitchStmtDecl))
        {TRACE_IT(39328);
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, lhsLocation);
        }
    }

    if (fLabelDefined)
    {TRACE_IT(39329);
        this->m_writer.MarkLabel(doneLabel);
    }
}

bool ByteCodeGenerator::NeedCheckBlockVar(Symbol* sym, Scope* scope, FuncInfo* funcInfo) const
{TRACE_IT(39330);
    bool tdz = sym->GetIsBlockVar()
        && (scope->GetFunc() != funcInfo || ((sym->GetDecl()->nop == knopLetDecl || sym->GetDecl()->nop == knopConstDecl) && sym->GetDecl()->sxVar.isSwitchStmtDecl));

    return tdz || sym->GetIsNonSimpleParameter();
}

void ByteCodeGenerator::EmitPropDelete(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo)
{TRACE_IT(39331);
    // If sym belongs to a parent frame, delete it from the closure environment.
    // If it belongs to this func, but there's a non-local reference, get it from the heap-allocated frame.
    // (TODO: optimize this by getting the sym from its normal location if there are no non-local defs.)
    // Otherwise, just return false.

    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    Js::PropertyId envIndex = -1;
    Scope *scope = nullptr;
    Scope *symScope = sym ? sym->GetScope() : this->globalScope;
    Assert(symScope);

    for (;;)
    {TRACE_IT(39332);
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {TRACE_IT(39333);
            scopeLocation = ByteCodeGenerator::RootObjectRegister;
        }
        else if (envIndex == -1)
        {TRACE_IT(39334);
            Assert(funcInfo == scope->GetFunc());
            scopeLocation = scope->GetLocation();
        }

        if (scope == symScope)
        {TRACE_IT(39335);
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {TRACE_IT(39336);
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }

        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {TRACE_IT(39337);
            this->m_writer.BrEnvProperty(
                Js::OpCode::BrOnNoEnvProperty,
                nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            Js::RegSlot instLocation = funcInfo->AcquireTmpRegister();

            Assert(scope->GetIsObject());
            this->m_writer.SlotI1(
                Js::OpCode::LdEnvObj,
                instLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            if (unwrapWithObj)
            {TRACE_IT(39338);
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, instLocation, instLocation);
            }

            this->m_writer.Property(Js::OpCode::DeleteFld, lhsLocation, instLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            funcInfo->ReleaseTmpRegister(instLocation);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {TRACE_IT(39339);
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->m_writer.ElementU(Js::OpCode::DeleteLocalFld, lhsLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));
        }
        else
        {TRACE_IT(39340);
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Js::RegSlot unwrappedScopeLocation = Js::Constants::NoRegister;
            if (unwrapWithObj)
            {TRACE_IT(39341);
                unwrappedScopeLocation = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, unwrappedScopeLocation, scopeLocation);
                scopeLocation = unwrappedScopeLocation;
            }

            this->m_writer.Property(Js::OpCode::DeleteFld, lhsLocation, scopeLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            if (unwrapWithObj)
            {TRACE_IT(39342);
                funcInfo->ReleaseTmpRegister(unwrappedScopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    // Arrived at the scope in which the property was defined.
    if (sym == nullptr || sym->GetIsGlobal())
    {TRACE_IT(39343);
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        if (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents))
        {TRACE_IT(39344);
            this->m_writer.ScopedProperty(Js::OpCode::ScopedDeleteFld, lhsLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));
        }
        else
        {TRACE_IT(39345);
            this->m_writer.Property(Js::OpCode::DeleteRootFld, lhsLocation, ByteCodeGenerator::RootObjectRegister,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));
        }
    }
    else
    {TRACE_IT(39346);
        // The delete will look like a non-local reference, so make sure a slot is reserved.
        sym->EnsureScopeSlot(funcInfo);
        this->m_writer.Reg1(Js::OpCode::LdFalse, lhsLocation);
    }

    if (fLabelDefined)
    {TRACE_IT(39347);
        this->m_writer.MarkLabel(doneLabel);
    }
}

void ByteCodeGenerator::EmitTypeOfFld(FuncInfo * funcInfo, Js::PropertyId propertyId, Js::RegSlot value, Js::RegSlot instance, Js::OpCode ldFldOp)
{TRACE_IT(39348);

    uint cacheId;
    Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
    switch (ldFldOp)
    {
    case Js::OpCode::LdRootFldForTypeOf:
        cacheId = funcInfo->FindOrAddRootObjectInlineCacheId(propertyId, false, false);
        this->Writer()->PatchableRootProperty(ldFldOp, tmpReg, cacheId, false, false);
        break;

    case Js::OpCode::LdLocalFld:
    case Js::OpCode::ScopedLdFldForTypeOf:
        cacheId = funcInfo->FindOrAddInlineCacheId(instance, propertyId, false, false);
        this->Writer()->ElementP(ldFldOp, tmpReg, cacheId);
        break;

    default:
        cacheId = funcInfo->FindOrAddInlineCacheId(instance, propertyId, false, false);
        this->Writer()->PatchableProperty(ldFldOp, tmpReg, instance, cacheId);
        break;
    }

    this->Writer()->Reg2(Js::OpCode::Typeof, value, tmpReg);
    funcInfo->ReleaseTmpRegister(tmpReg);
}

void ByteCodeGenerator::EmitPropTypeof(Js::RegSlot lhsLocation, Symbol *sym, IdentPtr pid, FuncInfo *funcInfo)
{TRACE_IT(39349);
    // If sym belongs to a parent frame, delete it from the closure environment.
    // If it belongs to this func, but there's a non-local reference, get it from the heap-allocated frame.
    // (TODO: optimize this by getting the sym from its normal location if there are no non-local defs.)
    // Otherwise, just return false

    Js::ByteCodeLabel doneLabel = 0;
    bool fLabelDefined = false;
    Js::RegSlot scopeLocation = Js::Constants::NoRegister;
    Js::PropertyId envIndex = -1;
    Scope *scope = nullptr;
    Scope *symScope = sym ? sym->GetScope() : this->globalScope;
    Assert(symScope);

    if (sym && sym->GetIsModuleExportStorage())
    {TRACE_IT(39350);
        Js::RegSlot tmpLocation = funcInfo->AcquireTmpRegister();
        EmitModuleExportAccess(sym, Js::OpCode::LdModuleSlot, tmpLocation, funcInfo);
        this->m_writer.Reg2(Js::OpCode::Typeof, lhsLocation, tmpLocation);
        funcInfo->ReleaseTmpRegister(tmpLocation);
        return;
    }

    for (;;)
    {TRACE_IT(39351);
        scope = this->FindScopeForSym(symScope, scope, &envIndex, funcInfo);
        if (scope == this->globalScope)
        {TRACE_IT(39352);
            scopeLocation = ByteCodeGenerator::RootObjectRegister;
        }
        else if (envIndex == -1)
        {TRACE_IT(39353);
            Assert(funcInfo == scope->GetFunc());
            scopeLocation = scope->GetLocation();
        }

        if (scope == symScope)
        {TRACE_IT(39354);
            break;
        }

        // Found a scope to which the property may have been added.
        Assert(scope && scope->GetIsDynamic());

        if (!fLabelDefined)
        {TRACE_IT(39355);
            fLabelDefined = true;
            doneLabel = this->m_writer.DefineLabel();
        }

        Js::ByteCodeLabel nextLabel = this->m_writer.DefineLabel();
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();

        bool unwrapWithObj = scope->GetScopeType() == ScopeType_With && scriptContext->GetConfig()->IsES6UnscopablesEnabled();
        if (envIndex != -1)
        {TRACE_IT(39356);
            this->m_writer.BrEnvProperty(Js::OpCode::BrOnNoEnvProperty, nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId),
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            Js::RegSlot instLocation = funcInfo->AcquireTmpRegister();

            Assert(scope->GetIsObject());
            this->m_writer.SlotI1(Js::OpCode::LdEnvObj,
                instLocation,
                envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var));

            if (unwrapWithObj)
            {TRACE_IT(39357);
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, instLocation, instLocation);
            }

            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, instLocation, Js::OpCode::LdFldForTypeOf);

            funcInfo->ReleaseTmpRegister(instLocation);
        }
        else if (scopeLocation != Js::Constants::NoRegister && scopeLocation == funcInfo->frameObjRegister)
        {TRACE_IT(39358);
            this->m_writer.BrLocalProperty(Js::OpCode::BrOnNoLocalProperty, nextLabel,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Assert(!unwrapWithObj);
            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, scopeLocation, Js::OpCode::LdLocalFld);
        }
        else
        {TRACE_IT(39359);
            this->m_writer.BrProperty(Js::OpCode::BrOnNoProperty, nextLabel, scopeLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));

            Js::RegSlot unwrappedScopeLocation = Js::Constants::NoRegister;
            if (unwrapWithObj)
            {TRACE_IT(39360);
                unwrappedScopeLocation = funcInfo->AcquireTmpRegister();
                this->m_writer.Reg2(Js::OpCode::UnwrapWithObj, unwrappedScopeLocation, scopeLocation);
                scopeLocation = unwrappedScopeLocation;
            }

            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, scopeLocation, Js::OpCode::LdFldForTypeOf);

            if (unwrapWithObj)
            {TRACE_IT(39361);
                funcInfo->ReleaseTmpRegister(unwrappedScopeLocation);
            }
        }

        this->m_writer.Br(doneLabel);
        this->m_writer.MarkLabel(nextLabel);
    }

    // Arrived at the scope in which the property was defined.
    if (sym && sym->GetNeedDeclaration() && scope->GetFunc() == funcInfo)
    {TRACE_IT(39362);
        // Ensure this symbol has a slot if it needs one.
        if (sym->IsInSlot(funcInfo))
        {TRACE_IT(39363);
            Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
            funcInfo->FindOrAddSlotProfileId(scope, slot);
        }

        EmitUseBeforeDeclarationRuntimeError(this, lhsLocation);
    }
    else if (sym == nullptr || sym->GetIsGlobal())
    {TRACE_IT(39364);
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(this) : pid->GetPropertyId();
        if (this->flags & fscrEval)
        {TRACE_IT(39365);
            if (funcInfo->byteCodeFunction->GetIsStrictMode() && funcInfo->IsGlobalFunction())
            {TRACE_IT(39366);
                this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, funcInfo->frameDisplayRegister, Js::OpCode::ScopedLdFldForTypeOf);
            }
            else
            {TRACE_IT(39367);
                this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, funcInfo->GetEnvRegister(), Js::OpCode::ScopedLdFldForTypeOf);
            }
        }
        else if (this->flags & (fscrImplicitThis | fscrImplicitParents))
        {TRACE_IT(39368);
            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, funcInfo->GetEnvRegister(), Js::OpCode::ScopedLdFldForTypeOf);
        }
        else
        {TRACE_IT(39369);
            this->EmitTypeOfFld(funcInfo, propertyId, lhsLocation, ByteCodeGenerator::RootObjectRegister, Js::OpCode::LdRootFldForTypeOf);
        }
    }
    else if (sym->IsInSlot(funcInfo) || envIndex != -1)
    {TRACE_IT(39370);
        // Make sure the property has a slot. This will bump up the size of the slot array if necessary.
        Js::PropertyId slot = sym->EnsureScopeSlot(funcInfo);
        Js::ProfileId profileId = funcInfo->FindOrAddSlotProfileId(scope, slot);
        Js::RegSlot tmpLocation = funcInfo->AcquireTmpRegister();
        bool chkBlockVar = NeedCheckBlockVar(sym, scope, funcInfo);
        Js::OpCode op;

        op = this->GetLdSlotOp(scope, envIndex, scopeLocation, funcInfo);
        slot = slot + (sym->GetScope()->GetIsObject() ? 0 : Js::ScopeSlots::FirstSlotIndex);

        if (envIndex != -1)
        {TRACE_IT(39371);
            this->m_writer.SlotI2(op, tmpLocation, envIndex + Js::FrameDisplay::GetOffsetOfScopes() / sizeof(Js::Var), slot, profileId);
        }
        else if (scopeLocation != Js::Constants::NoRegister &&
            (scopeLocation == funcInfo->frameSlotsRegister || scopeLocation == funcInfo->frameObjRegister))
        {TRACE_IT(39372);
            this->m_writer.SlotI1(op, tmpLocation, slot, profileId);
        }
        else if (scope->HasInnerScopeIndex())
        {TRACE_IT(39373);
            this->m_writer.SlotI2(op, tmpLocation, scope->GetInnerScopeIndex(), slot, profileId);
        }
        else
        {TRACE_IT(39374);
            Assert(scope->GetIsObject());
            this->m_writer.Slot(op, tmpLocation, scopeLocation, slot, profileId);
        }

        if (chkBlockVar)
        {TRACE_IT(39375);
            this->m_writer.Reg1(Js::OpCode::ChkUndecl, tmpLocation);
        }

        this->m_writer.Reg2(Js::OpCode::Typeof, lhsLocation, tmpLocation);
        funcInfo->ReleaseTmpRegister(tmpLocation);
    }
    else
    {TRACE_IT(39376);
        this->m_writer.Reg2(Js::OpCode::Typeof, lhsLocation, sym->GetLocation());
    }

    if (fLabelDefined)
    {TRACE_IT(39377);
        this->m_writer.MarkLabel(doneLabel);
    }
}

void ByteCodeGenerator::EnsureNoRedeclarations(ParseNode *pnodeBlock, FuncInfo *funcInfo)
{TRACE_IT(39378);
    // Emit dynamic runtime checks for variable re-declarations. Only necessary for global functions (script or eval).
    // In eval only var declarations can cause redeclaration, and only in non-strict mode, because let/const variables
    // remain local to the eval code.

    Assert(pnodeBlock->nop == knopBlock);
    Assert(pnodeBlock->sxBlock.blockType == PnodeBlockType::Global || pnodeBlock->sxBlock.scope->GetScopeType() == ScopeType_GlobalEvalBlock);

    if (!(this->flags & fscrEvalCode))
    {
        IterateBlockScopedVariables(pnodeBlock, [this](ParseNode *pnode)
        {
            FuncInfo *funcInfo = this->TopFuncInfo();
            Symbol *sym = pnode->sxVar.sym;

            Assert(sym->GetIsGlobal());

            Js::PropertyId propertyId = sym->EnsurePosition(this);

            this->m_writer.ElementRootU(Js::OpCode::EnsureNoRootFld, funcInfo->FindOrAddReferencedPropertyId(propertyId));
        });
    }

    for (ParseNode *pnode = funcInfo->root->sxFnc.pnodeVars; pnode; pnode = pnode->sxVar.pnodeNext)
    {TRACE_IT(39379);
        Symbol* sym = pnode->sxVar.sym;

        if (sym == nullptr || pnode->sxVar.isBlockScopeFncDeclVar)
            continue;

        if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar()))
        {TRACE_IT(39380);
            // The init node was bound to the catch object, because it's inside a catch and has the
            // same name as the catch object. But we want to define a user var at function scope,
            // so find the right symbol. (We'll still assign the RHS value to the catch object symbol.)
            // This also applies to a var declaration in the same scope as a let declaration.

            // Assert that catch cannot be at function scope and let and var at function scope is redeclaration error.
            Assert(sym->GetIsCatch() || funcInfo->bodyScope != sym->GetScope());
            sym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
            Assert(sym && !sym->GetIsCatch() && !sym->GetIsBlockVar());
        }

        Assert(sym->GetIsGlobal());

        if (sym->GetSymbolType() == STVariable)
        {TRACE_IT(39381);
            Js::PropertyId propertyId = sym->EnsurePosition(this);

            if (this->flags & fscrEval)
            {TRACE_IT(39382);
                if (!funcInfo->byteCodeFunction->GetIsStrictMode())
                {TRACE_IT(39383);
                    this->m_writer.ScopedProperty(Js::OpCode::ScopedEnsureNoRedeclFld, ByteCodeGenerator::RootObjectRegister,
                        funcInfo->FindOrAddReferencedPropertyId(propertyId));
                }
            }
            else
            {TRACE_IT(39384);
                this->m_writer.ElementRootU(Js::OpCode::EnsureNoRootRedeclFld, funcInfo->FindOrAddReferencedPropertyId(propertyId));
            }
        }
    }
}

void ByteCodeGenerator::RecordAllIntConstants(FuncInfo * funcInfo)
{TRACE_IT(39385);
    Js::FunctionBody *byteCodeFunction = this->TopFuncInfo()->GetParsedFunctionBody();
    funcInfo->constantToRegister.Map([byteCodeFunction](unsigned int val, Js::RegSlot location)
    {
        byteCodeFunction->RecordIntConstant(byteCodeFunction->MapRegSlot(location), val);
    });
}

void ByteCodeGenerator::RecordAllStrConstants(FuncInfo * funcInfo)
{TRACE_IT(39386);
    Js::FunctionBody *byteCodeFunction = this->TopFuncInfo()->GetParsedFunctionBody();
    funcInfo->stringToRegister.Map([byteCodeFunction](IdentPtr pid, Js::RegSlot location)
    {
        byteCodeFunction->RecordStrConstant(byteCodeFunction->MapRegSlot(location), pid->Psz(), pid->Cch());
    });
}

void ByteCodeGenerator::RecordAllStringTemplateCallsiteConstants(FuncInfo* funcInfo)
{TRACE_IT(39387);
    Js::FunctionBody *byteCodeFunction = this->TopFuncInfo()->GetParsedFunctionBody();
    funcInfo->stringTemplateCallsiteRegisterMap.Map([byteCodeFunction](ParseNodePtr pnode, Js::RegSlot location)
    {
        Js::ScriptContext* scriptContext = byteCodeFunction->GetScriptContext();
        Js::JavascriptLibrary* library = scriptContext->GetLibrary();
        Js::RecyclableObject* callsiteObject = library->TryGetStringTemplateCallsiteObject(pnode);

        if (callsiteObject == nullptr)
        {TRACE_IT(39388);
            Js::RecyclableObject* rawArray = ByteCodeGenerator::BuildArrayFromStringList(pnode->sxStrTemplate.pnodeStringRawLiterals, pnode->sxStrTemplate.countStringLiterals, scriptContext);
            rawArray->Freeze();

            callsiteObject = ByteCodeGenerator::BuildArrayFromStringList(pnode->sxStrTemplate.pnodeStringLiterals, pnode->sxStrTemplate.countStringLiterals, scriptContext);
            callsiteObject->SetPropertyWithAttributes(Js::PropertyIds::raw, rawArray, PropertyNone, nullptr);
            callsiteObject->Freeze();

            library->AddStringTemplateCallsiteObject(callsiteObject);
        }

        byteCodeFunction->RecordConstant(byteCodeFunction->MapRegSlot(location), callsiteObject);
    });
}

bool IsApplyArgs(ParseNode* callNode)
{TRACE_IT(39389);
    ParseNode* target = callNode->sxCall.pnodeTarget;
    ParseNode* args = callNode->sxCall.pnodeArgs;
    if ((target != nullptr) && (target->nop == knopDot))
    {TRACE_IT(39390);
        ParseNode* lhsNode = target->sxBin.pnode1;
        if ((lhsNode != nullptr) && ((lhsNode->nop == knopDot) || (lhsNode->nop == knopName)) && !IsArguments(lhsNode))
        {TRACE_IT(39391);
            ParseNode* nameNode = target->sxBin.pnode2;
            if (nameNode != nullptr)
            {TRACE_IT(39392);
                bool nameIsApply = nameNode->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::apply;
                if (nameIsApply && args != nullptr && args->nop == knopList)
                {TRACE_IT(39393);
                    ParseNode* arg1 = args->sxBin.pnode1;
                    ParseNode* arg2 = args->sxBin.pnode2;
                    if ((arg1 != nullptr) && (arg1->nop == knopThis) && (arg2 != nullptr) && (arg2->nop == knopName) && (arg2->sxPid.sym != nullptr))
                    {TRACE_IT(39394);
                        return arg2->sxPid.sym->GetIsArguments();
                    }
                }
            }
        }
    }
    return false;
}

void PostCheckApplyEnclosesArgs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, ApplyCheck* applyCheck)
{TRACE_IT(39395);
    if ((pnode == nullptr) || (!applyCheck->matches))
    {TRACE_IT(39396);
        return;
    }

    if (pnode->nop == knopCall)
    {TRACE_IT(39397);
        if ((!pnode->isUsed) && IsApplyArgs(pnode))
        {TRACE_IT(39398);
            if (!applyCheck->insideApplyCall)
            {TRACE_IT(39399);
                applyCheck->matches = false;
            }
            applyCheck->insideApplyCall = false;
        }
    }
}

void CheckApplyEnclosesArgs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, ApplyCheck* applyCheck)
{TRACE_IT(39400);
    if ((pnode == nullptr) || (!applyCheck->matches))
    {TRACE_IT(39401);
        return;
    }

    switch (pnode->nop)
    {
    case knopName:
    {TRACE_IT(39402);
        Symbol* sym = pnode->sxPid.sym;
        if (sym != nullptr)
        {TRACE_IT(39403);
            if (sym->GetIsArguments())
            {TRACE_IT(39404);
                if (!applyCheck->insideApplyCall)
                {TRACE_IT(39405);
                    applyCheck->matches = false;
                }
            }
        }
        break;
    }

    case knopCall:
        if ((!pnode->isUsed) && IsApplyArgs(pnode))
        {TRACE_IT(39406);
            // no nested apply calls
            if (applyCheck->insideApplyCall)
            {TRACE_IT(39407);
                applyCheck->matches = false;
            }
            else
            {TRACE_IT(39408);
                applyCheck->insideApplyCall = true;
                applyCheck->sawApply = true;
                pnode->sxCall.isApplyCall = true;
            }
        }
        break;
    }
}

unsigned int CountArguments(ParseNode *pnode, BOOL *pSideEffect = nullptr)
{TRACE_IT(39409);
    // If the caller passed us a pSideEffect, it wants to know whether there are potential
    // side-effects in the argument list. We need to know this so that the call target
    // operands can be preserved if necessary.
    // For now, treat any non-leaf op as a potential side-effect. This causes no detectable slowdowns,
    // but we can be more precise if we need to be.
    if (pSideEffect)
    {TRACE_IT(39410);
        *pSideEffect = FALSE;
    }

    unsigned int argCount = 1;
    if (pnode != nullptr)
    {TRACE_IT(39411);
        while (pnode->nop == knopList)
        {TRACE_IT(39412);
            argCount++;
            if (pSideEffect && !(ParseNode::Grfnop(pnode->sxBin.pnode1->nop) & fnopLeaf))
            {TRACE_IT(39413);
                *pSideEffect = TRUE;
            }
            pnode = pnode->sxBin.pnode2;
        }
        argCount++;
        if (pSideEffect && !(ParseNode::Grfnop(pnode->nop) & fnopLeaf))
        {TRACE_IT(39414);
            *pSideEffect = TRUE;
        }
    }

    return argCount;
}

void SaveOpndValue(ParseNode *pnode, FuncInfo *funcInfo)
{TRACE_IT(39415);
    // Save a local name to a register other than its home location.
    // This guards against side-effects in cases like x.foo(x = bar()).
    Symbol *sym = nullptr;
    if (pnode->nop == knopName)
    {TRACE_IT(39416);
        sym = pnode->sxPid.sym;
    }
    else if (pnode->nop == knopComputedName)
    {TRACE_IT(39417);
        ParseNode *pnode1 = pnode->sxUni.pnode1;
        if (pnode1->nop == knopName)
        {TRACE_IT(39418);
            sym = pnode1->sxPid.sym;
        }
    }

    if (sym == nullptr)
    {TRACE_IT(39419);
        return;
    }

    // If the target is a local being kept in its home location,
    // protect the target's value in the event the home location is overwritten.
    if (pnode->location != Js::Constants::NoRegister &&
        sym->GetScope()->GetFunc() == funcInfo &&
        pnode->location == sym->GetLocation())
    {TRACE_IT(39420);
        pnode->location = funcInfo->AcquireTmpRegister();
    }
}

void ByteCodeGenerator::StartStatement(ParseNode* node)
{TRACE_IT(39421);
    Assert(TopFuncInfo() != nullptr);
    m_writer.StartStatement(node, TopFuncInfo()->curTmpReg - TopFuncInfo()->firstTmpReg);
}

void ByteCodeGenerator::EndStatement(ParseNode* node)
{TRACE_IT(39422);
    m_writer.EndStatement(node);
}

void ByteCodeGenerator::StartSubexpression(ParseNode* node)
{TRACE_IT(39423);
    Assert(TopFuncInfo() != nullptr);
    m_writer.StartSubexpression(node);
}

void ByteCodeGenerator::EndSubexpression(ParseNode* node)
{TRACE_IT(39424);
    m_writer.EndSubexpression(node);
}

void EmitReference(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39425);
    // Generate code for the LHS of an assignment.
    switch (pnode->nop)
    {
    case knopDot:
        Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        break;

    case knopIndex:
        Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        Emit(pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, false);
        break;

    case knopName:
        break;

    case knopArrayPattern:
    case knopObjectPattern:
        break;

    case knopCall:
    case knopNew:
        // Emit the operands of a call that will be used as a LHS.
        // These have to be emitted before the RHS, but they have to persist until
        // the end of the expression.
        // Emit the call target operands first.
        switch (pnode->sxCall.pnodeTarget->nop)
        {
        case knopDot:
        case knopIndex:
            funcInfo->AcquireLoc(pnode->sxCall.pnodeTarget);
            EmitReference(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo);
            break;

        case knopName:
        {TRACE_IT(39426);
            Symbol *sym = pnode->sxCall.pnodeTarget->sxPid.sym;
            if (!sym || sym->GetLocation() == Js::Constants::NoRegister)
            {TRACE_IT(39427);
                funcInfo->AcquireLoc(pnode->sxCall.pnodeTarget);
            }
            if (sym && (sym->IsInSlot(funcInfo) || sym->GetScope()->GetFunc() != funcInfo))
            {TRACE_IT(39428);
                // Can't get the value from the assigned register, so load it here.
                EmitLoad(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo);
            }
            else
            {TRACE_IT(39429);
                // EmitLoad will check for needsDeclaration and emit the Use Before Declaration error
                // bytecode op as necessary, but EmitReference does not check this (by design). So we
                // must manually check here.
                EmitUseBeforeDeclaration(pnode->sxCall.pnodeTarget->sxPid.sym, byteCodeGenerator, funcInfo);
                EmitReference(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo);
            }
            break;
        }
        default:
            EmitLoad(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo);
            break;
        }

        // Now the arg list. We evaluate everything now and emit the ArgOut's later.
        if (pnode->sxCall.pnodeArgs)
        {TRACE_IT(39430);
            ParseNode *pnodeArg = pnode->sxCall.pnodeArgs;
            while (pnodeArg->nop == knopList)
            {TRACE_IT(39431);
                Emit(pnodeArg->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
                pnodeArg = pnodeArg->sxBin.pnode2;
            }
            Emit(pnodeArg, byteCodeGenerator, funcInfo, false);
        }
        break;

    default:
        Emit(pnode, byteCodeGenerator, funcInfo, false);
        break;
    }
}

void EmitGetIterator(Js::RegSlot iteratorLocation, Js::RegSlot iterableLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitIteratorNext(Js::RegSlot itemLocation, Js::RegSlot iteratorLocation, Js::RegSlot nextInputLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitIteratorClose(Js::RegSlot iteratorLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitIteratorComplete(Js::RegSlot doneLocation, Js::RegSlot iteratorResultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);
void EmitIteratorValue(Js::RegSlot valueLocation, Js::RegSlot iteratorResultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo);

void EmitDestructuredElement(ParseNode *elem, Js::RegSlot sourceLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39432);
    switch (elem->nop)
    {
    case knopVarDecl:
    case knopLetDecl:
    case knopConstDecl:
        // We manually need to set NeedDeclaration since the node won't be visited.
        elem->sxVar.sym->SetNeedDeclaration(false);
        break;

    default:
        EmitReference(elem, byteCodeGenerator, funcInfo);
    }

    EmitAssignment(nullptr, elem, sourceLocation, byteCodeGenerator, funcInfo);
    funcInfo->ReleaseReference(elem);
}

void EmitDestructuredRestArray(ParseNode *elem,
    Js::RegSlot iteratorLocation,
    Js::RegSlot shouldCallReturnFunctionLocation,
    Js::RegSlot shouldCallReturnFunctionLocationFinally,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39433);
    Js::RegSlot restArrayLocation = funcInfo->AcquireTmpRegister();
    bool isAssignmentTarget = !(elem->sxUni.pnode1->IsPattern() || elem->sxUni.pnode1->IsVarLetOrConst());

    if (isAssignmentTarget)
    {TRACE_IT(39434);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocation);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocationFinally);
        EmitReference(elem->sxUni.pnode1, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocation);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocationFinally);
    }

    byteCodeGenerator->Writer()->Reg1Unsigned1(
        Js::OpCode::NewScArray,
        restArrayLocation,
        ByteCodeGenerator::DefaultArraySize);

    // BytecodeGen can't convey to IRBuilder that some of the temporaries used here are live. When we
    // have a rest parameter, a counter is used in a loop for the array index, but there is no way to
    // convey this is live on the back edge.
    // As a workaround, we have a persistent var reg that is used for the loop counter
    Js::RegSlot counterLocation = elem->location;
    // TODO[ianhall]: Is calling EnregisterConstant() during Emit phase allowed?
    Js::RegSlot zeroConstantReg = byteCodeGenerator->EnregisterConstant(0);
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, counterLocation, zeroConstantReg);

    // loopTop:
    Js::ByteCodeLabel loopTop = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->MarkLabel(loopTop);

    Js::RegSlot itemLocation = funcInfo->AcquireTmpRegister();

    EmitIteratorNext(itemLocation, iteratorLocation, Js::Constants::NoRegister, byteCodeGenerator, funcInfo);

    Js::RegSlot doneLocation = funcInfo->AcquireTmpRegister();
    EmitIteratorComplete(doneLocation, itemLocation, byteCodeGenerator, funcInfo);

    Js::ByteCodeLabel iteratorDone = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, iteratorDone, doneLocation);

    Js::RegSlot valueLocation = funcInfo->AcquireTmpRegister();
    EmitIteratorValue(valueLocation, itemLocation, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocation);
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocationFinally);

    byteCodeGenerator->Writer()->Element(
        ByteCodeGenerator::GetStElemIOpCode(funcInfo),
        valueLocation, restArrayLocation, counterLocation);
    funcInfo->ReleaseTmpRegister(valueLocation);
    funcInfo->ReleaseTmpRegister(doneLocation);
    funcInfo->ReleaseTmpRegister(itemLocation);

    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Incr_A, counterLocation, counterLocation);

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocation);
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocationFinally);

    byteCodeGenerator->Writer()->Br(loopTop);

    // iteratorDone:
    byteCodeGenerator->Writer()->MarkLabel(iteratorDone);

    ParseNode *restElem = elem->sxUni.pnode1;
    if (isAssignmentTarget)
    {
        EmitAssignment(nullptr, restElem, restArrayLocation, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseReference(restElem);
    }
    else
    {
        EmitDestructuredElement(restElem, restArrayLocation, byteCodeGenerator, funcInfo);
    }

    funcInfo->ReleaseTmpRegister(restArrayLocation);
}

void EmitDestructuredArray(
    ParseNode *lhs,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo);

void EmitIteratorCloseIfNotDone(Js::RegSlot iteratorLocation, Js::RegSlot doneLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{TRACE_IT(39435);
    Js::ByteCodeLabel skipCloseLabel = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, skipCloseLabel, doneLocation);

    EmitIteratorClose(iteratorLocation, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->MarkLabel(skipCloseLabel);
}

/*
  EmitDestructuredArray(lhsArray, rhs):
    iterator = rhs[@@iterator]

    if lhsArray empty
      return

    for each element in lhsArray except rest
      value = iterator.next()
      if element is a nested destructured array
        EmitDestructuredArray(element, value)
      else
        if value is undefined and there is an initializer
          evaluate initializer
          evaluate element reference
          element = initializer
        else
          element = value

    if lhsArray has a rest element
      rest = []
      while iterator is not done
        value = iterator.next()
        rest.append(value)
*/
void EmitDestructuredArrayCore(
    ParseNode *list,
    Js::RegSlot iteratorLocation,
    Js::RegSlot shouldCallReturnFunctionLocation,
    Js::RegSlot shouldCallReturnFunctionLocationFinally,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo
    )
{TRACE_IT(39436);
    Assert(list != nullptr);

    ParseNode *elem = nullptr;
    while (list != nullptr)
    {TRACE_IT(39437);
        ParseNode *init = nullptr;

        if (list->nop == knopList)
        {TRACE_IT(39438);
            elem = list->sxBin.pnode1;
        }
        else
        {TRACE_IT(39439);
            elem = list;
        }

        if (elem->nop == knopEllipsis)
        {TRACE_IT(39440);
            break;
        }

        switch (elem->nop)
        {
        case knopAsg:
            // An assignment node will always have an initializer
            init = elem->sxBin.pnode2;
            elem = elem->sxBin.pnode1;
            break;

        case knopVarDecl:
        case knopLetDecl:
        case knopConstDecl:
            init = elem->sxVar.pnodeInit;
            break;

        default:
            break;
        }

        byteCodeGenerator->StartStatement(elem);

        bool isAssignmentTarget = !(elem->IsPattern() || elem->IsVarLetOrConst());

        if (isAssignmentTarget)
        {TRACE_IT(39441);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocation);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocationFinally);
            EmitReference(elem, byteCodeGenerator, funcInfo);
        }

        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocation);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocationFinally);

        Js::RegSlot itemLocation = funcInfo->AcquireTmpRegister();
        EmitIteratorNext(itemLocation, iteratorLocation, Js::Constants::NoRegister, byteCodeGenerator, funcInfo);

        Js::RegSlot doneLocation = funcInfo->AcquireTmpRegister();
        EmitIteratorComplete(doneLocation, itemLocation, byteCodeGenerator, funcInfo);

        if (elem->nop == knopEmpty)
        {TRACE_IT(39442);
            if (list->nop == knopList)
            {TRACE_IT(39443);
                list = list->sxBin.pnode2;
                funcInfo->ReleaseTmpRegister(doneLocation);
                funcInfo->ReleaseTmpRegister(itemLocation);
                continue;
            }
            else
            {TRACE_IT(39444);
                Assert(list->nop == knopEmpty);
                EmitIteratorCloseIfNotDone(iteratorLocation, doneLocation, byteCodeGenerator, funcInfo);
                funcInfo->ReleaseTmpRegister(doneLocation);
                funcInfo->ReleaseTmpRegister(itemLocation);
                break;
            }
        }

        // If the iterator hasn't completed, skip assigning undefined.
        Js::ByteCodeLabel iteratorAlreadyDone = byteCodeGenerator->Writer()->DefineLabel();
        byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, iteratorAlreadyDone, doneLocation);

        // We're not done with the iterator, so assign the .next() value.
        Js::RegSlot valueLocation = funcInfo->AcquireTmpRegister();
        EmitIteratorValue(valueLocation, itemLocation, byteCodeGenerator, funcInfo);
        Js::ByteCodeLabel beforeDefaultAssign = byteCodeGenerator->Writer()->DefineLabel();

        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocation);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocationFinally);
        byteCodeGenerator->Writer()->Br(beforeDefaultAssign);

        // iteratorAlreadyDone:
        byteCodeGenerator->Writer()->MarkLabel(iteratorAlreadyDone);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, valueLocation, funcInfo->undefinedConstantRegister);

        // beforeDefaultAssign:
        byteCodeGenerator->Writer()->MarkLabel(beforeDefaultAssign);

        if (elem->IsPattern())
        {TRACE_IT(39445);
            // If we get an undefined value and have an initializer, use it in place of undefined.
            if (init != nullptr)
            {TRACE_IT(39446);
                /*
                the IR builder uses two symbols for a temp register in the if else path
                R9 <- R3
                if (...)
                R9 <- R2
                R10 = R9.<property>  // error -> IR creates a new lifetime for the if path, and the direct path dest is not referenced
                hence we have to create a new temp

                TEMP REG USED TO FIX THIS PRODUCES THIS
                R9 <- R3
                if (BrEq_A R9, R3)
                R10 <- R2               :
                else
                R10 <- R9               : skipdefault
                ...  = R10[@@iterator]  : loadIter
                */

                // Temp Register
                Js::RegSlot valueLocationTmp = funcInfo->AcquireTmpRegister();
                byteCodeGenerator->StartStatement(init);

                Js::ByteCodeLabel skipDefault = byteCodeGenerator->Writer()->DefineLabel();
                Js::ByteCodeLabel loadIter = byteCodeGenerator->Writer()->DefineLabel();

                // check value is undefined
                byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrSrNeq_A, skipDefault, valueLocation, funcInfo->undefinedConstantRegister);

                // Evaluate the default expression and assign it.
                Emit(init, byteCodeGenerator, funcInfo, false);
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, valueLocationTmp, init->location);
                funcInfo->ReleaseLoc(init);

                // jmp to loadIter
                byteCodeGenerator->Writer()->Br(loadIter);

                // skipDefault:
                byteCodeGenerator->Writer()->MarkLabel(skipDefault);
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, valueLocationTmp, valueLocation);

                // loadIter:
                // @@iterator
                byteCodeGenerator->Writer()->MarkLabel(loadIter);
                byteCodeGenerator->EndStatement(init);

                if (elem->nop == knopObjectPattern)
                {
                    EmitDestructuredObject(elem, valueLocationTmp, byteCodeGenerator, funcInfo);
                }
                else
                {
                    // Recursively emit a destructured array using the current .next() as the RHS.
                    EmitDestructuredArray(elem, valueLocationTmp, byteCodeGenerator, funcInfo);
                }

                funcInfo->ReleaseTmpRegister(valueLocationTmp);
            }
            else
            {TRACE_IT(39447);
                if (elem->nop == knopObjectPattern)
                {
                    EmitDestructuredObject(elem, valueLocation, byteCodeGenerator, funcInfo);
                }
                else
                {
                    // Recursively emit a destructured array using the current .next() as the RHS.
                    EmitDestructuredArray(elem, valueLocation, byteCodeGenerator, funcInfo);
                }
            }
        }
        else
        {
            EmitDestructuredValueOrInitializer(elem, valueLocation, init, isAssignmentTarget, byteCodeGenerator, funcInfo);
        }

        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocation);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocationFinally);

        if (list->nop != knopList)
        {
            EmitIteratorCloseIfNotDone(iteratorLocation, doneLocation, byteCodeGenerator, funcInfo);
        }

        funcInfo->ReleaseTmpRegister(valueLocation);
        funcInfo->ReleaseTmpRegister(doneLocation);
        funcInfo->ReleaseTmpRegister(itemLocation);

        if (isAssignmentTarget)
        {TRACE_IT(39448);
            funcInfo->ReleaseReference(elem);
        }

        byteCodeGenerator->EndStatement(elem);

        if (list->nop == knopList)
        {TRACE_IT(39449);
            list = list->sxBin.pnode2;
        }
        else
        {TRACE_IT(39450);
            break;
        }
    }

    // If we saw a rest element, emit the rest array.
    if (elem != nullptr && elem->nop == knopEllipsis)
    {
        EmitDestructuredRestArray(elem,
            iteratorLocation,
            shouldCallReturnFunctionLocation,
            shouldCallReturnFunctionLocationFinally,
            byteCodeGenerator,
            funcInfo);
    }
}

// Generating
// try {
//    CallIteratorClose
// } catch (e) {
//    do nothing
// }

void EmitTryCatchAroundClose(
    Js::RegSlot iteratorLocation,
    Js::ByteCodeLabel endLabel,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39451);
    Js::ByteCodeLabel catchLabel = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->Br(Js::OpCode::TryCatch, catchLabel);

    //
    // There is no need to add TryScopeRecord here as we are going to call 'return' function and there is not yield expression here.

    EmitIteratorClose(iteratorLocation, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
    byteCodeGenerator->Writer()->Br(endLabel);

    byteCodeGenerator->Writer()->MarkLabel(catchLabel);
    Js::RegSlot catchParamLocation = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::Catch, catchParamLocation);
    funcInfo->ReleaseTmpRegister(catchParamLocation);

    byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
}

struct ByteCodeGenerator::TryScopeRecord : public JsUtil::DoublyLinkedListElement<TryScopeRecord>
{
    Js::OpCode op;
    Js::ByteCodeLabel label;
    Js::RegSlot reg1;
    Js::RegSlot reg2;

    TryScopeRecord(Js::OpCode op, Js::ByteCodeLabel label) : op(op), label(label), reg1(Js::Constants::NoRegister), reg2(Js::Constants::NoRegister) {TRACE_IT(39452); }
    TryScopeRecord(Js::OpCode op, Js::ByteCodeLabel label, Js::RegSlot r1, Js::RegSlot r2) : op(op), label(label), reg1(r1), reg2(r2) {TRACE_IT(39453); }
};

// Generating
// catch(e) {
//      if (shouldCallReturn)
//          CallReturnWhichWrappedByTryCatch
//      throw e;
// }
void EmitTopLevelCatch(Js::ByteCodeLabel catchLabel,
    Js::RegSlot iteratorLocation,
    Js::RegSlot shouldCallReturnLocation,
    Js::RegSlot shouldCallReturnLocationFinally,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39454);
    Js::ByteCodeLabel afterCatchBlockLabel = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
    byteCodeGenerator->Writer()->Br(afterCatchBlockLabel);
    byteCodeGenerator->Writer()->MarkLabel(catchLabel);

    Js::RegSlot catchParamLocation = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::Catch, catchParamLocation);

    ByteCodeGenerator::TryScopeRecord tryRecForCatch(Js::OpCode::ResumeCatch, catchLabel);
    if (funcInfo->byteCodeFunction->IsCoroutine())
    {TRACE_IT(39455);
        byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForCatch);
    }

    Js::ByteCodeLabel skipCallCloseLabel = byteCodeGenerator->Writer()->DefineLabel();

    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrFalse_A, skipCallCloseLabel, shouldCallReturnLocation);
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnLocationFinally);
    EmitTryCatchAroundClose(iteratorLocation, skipCallCloseLabel, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->MarkLabel(skipCallCloseLabel);

    // Rethrow the exception.
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::Throw, catchParamLocation);

    funcInfo->ReleaseTmpRegister(catchParamLocation);

    if (funcInfo->byteCodeFunction->IsCoroutine())
    {TRACE_IT(39456);
        byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
    }

    byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
    byteCodeGenerator->Writer()->MarkLabel(afterCatchBlockLabel);
}

// Generating
// finally {
//      if (shouldCallReturn)
//          CallReturn
// }

void EmitTopLevelFinally(Js::ByteCodeLabel finallyLabel,
    Js::RegSlot iteratorLocation,
    Js::RegSlot shouldCallReturnLocation,
    Js::RegSlot yieldExceptionLocation,
    Js::RegSlot yieldOffsetLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39457);
    bool isCoroutine = funcInfo->byteCodeFunction->IsCoroutine();

    Js::ByteCodeLabel afterFinallyBlockLabel = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);

    byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(false);
    byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

    byteCodeGenerator->Writer()->Br(afterFinallyBlockLabel);
    byteCodeGenerator->Writer()->MarkLabel(finallyLabel);

    ByteCodeGenerator::TryScopeRecord tryRecForFinally(Js::OpCode::ResumeFinally, finallyLabel, yieldExceptionLocation, yieldOffsetLocation);
    if (isCoroutine)
    {TRACE_IT(39458);
        byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForFinally);
    }

    Js::ByteCodeLabel skipCallCloseLabel = byteCodeGenerator->Writer()->DefineLabel();

    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrFalse_A, skipCallCloseLabel, shouldCallReturnLocation);
    EmitIteratorClose(iteratorLocation, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->MarkLabel(skipCallCloseLabel);

    if (isCoroutine)
    {TRACE_IT(39459);
        byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
        funcInfo->ReleaseTmpRegister(yieldOffsetLocation);
        funcInfo->ReleaseTmpRegister(yieldExceptionLocation);
    }

    byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(false);
    byteCodeGenerator->Writer()->Empty(Js::OpCode::LeaveNull);
    byteCodeGenerator->Writer()->MarkLabel(afterFinallyBlockLabel);
}

void EmitCatchAndFinallyBlocks(Js::ByteCodeLabel catchLabel,
    Js::ByteCodeLabel finallyLabel,
    Js::RegSlot iteratorLocation,
    Js::RegSlot shouldCallReturnFunctionLocation,
    Js::RegSlot shouldCallReturnFunctionLocationFinally,
    Js::RegSlot yieldExceptionLocation,
    Js::RegSlot yieldOffsetLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo
    )
{TRACE_IT(39460);
    bool isCoroutine = funcInfo->byteCodeFunction->IsCoroutine();
    if (isCoroutine)
    {TRACE_IT(39461);
        byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
    }

    EmitTopLevelCatch(catchLabel,
        iteratorLocation,
        shouldCallReturnFunctionLocation,
        shouldCallReturnFunctionLocationFinally,
        byteCodeGenerator,
        funcInfo);

    if (isCoroutine)
    {TRACE_IT(39462);
        byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
    }

    EmitTopLevelFinally(finallyLabel,
        iteratorLocation,
        shouldCallReturnFunctionLocationFinally,
        yieldExceptionLocation,
        yieldOffsetLocation,
        byteCodeGenerator,
        funcInfo);

    funcInfo->ReleaseTmpRegister(shouldCallReturnFunctionLocationFinally);
    funcInfo->ReleaseTmpRegister(shouldCallReturnFunctionLocation);
}

// Emit a wrapper try..finaly block around the destructuring elements
void EmitDestructuredArray(
    ParseNode *lhs,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39463);
    byteCodeGenerator->StartStatement(lhs);
    Js::RegSlot iteratorLocation = funcInfo->AcquireTmpRegister();

    EmitGetIterator(iteratorLocation, rhsLocation, byteCodeGenerator, funcInfo);

    Assert(lhs->nop == knopArrayPattern);
    ParseNode *list = lhs->sxArrLit.pnode1;

    if (list == nullptr)
    { // Handline this case ([] = obj);
        EmitIteratorClose(iteratorLocation, byteCodeGenerator, funcInfo);

        // No elements to bind or assign.
        funcInfo->ReleaseTmpRegister(iteratorLocation);
        byteCodeGenerator->EndStatement(lhs);
        return;
    }

    // This variable facilitates on when to call the return function (which is Iterator close). When we are emitting bytecode for destructuring element
    // this variable will be set to true.
    Js::RegSlot shouldCallReturnFunctionLocation = funcInfo->AcquireTmpRegister();
    Js::RegSlot shouldCallReturnFunctionLocationFinally = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocation);
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocationFinally);

    byteCodeGenerator->SetHasFinally(true);
    byteCodeGenerator->SetHasTry(true);
    byteCodeGenerator->TopFuncInfo()->byteCodeFunction->SetDontInline(true);

    Js::RegSlot regException = Js::Constants::NoRegister;
    Js::RegSlot regOffset = Js::Constants::NoRegister;
    bool isCoroutine = funcInfo->byteCodeFunction->IsCoroutine();

    if (isCoroutine)
    {TRACE_IT(39464);
        regException = funcInfo->AcquireTmpRegister();
        regOffset = funcInfo->AcquireTmpRegister();
    }

    // Insert try node here
    Js::ByteCodeLabel finallyLabel = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel catchLabel = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

    ByteCodeGenerator::TryScopeRecord tryRecForTryFinally(Js::OpCode::TryFinallyWithYield, finallyLabel);

    if (isCoroutine)
    {TRACE_IT(39465);
        byteCodeGenerator->Writer()->BrReg2(Js::OpCode::TryFinallyWithYield, finallyLabel, regException, regOffset);
        tryRecForTryFinally.reg1 = regException;
        tryRecForTryFinally.reg2 = regOffset;
        byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForTryFinally);
    }
    else
    {TRACE_IT(39466);
        byteCodeGenerator->Writer()->Br(Js::OpCode::TryFinally, finallyLabel);
    }

    byteCodeGenerator->Writer()->Br(Js::OpCode::TryCatch, catchLabel);

    ByteCodeGenerator::TryScopeRecord tryRecForTry(Js::OpCode::TryCatch, catchLabel);
    if (isCoroutine)
    {TRACE_IT(39467);
        byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForTry);
    }

    EmitDestructuredArrayCore(list,
        iteratorLocation,
        shouldCallReturnFunctionLocation,
        shouldCallReturnFunctionLocationFinally,
        byteCodeGenerator,
        funcInfo);

    EmitCatchAndFinallyBlocks(catchLabel,
        finallyLabel,
        iteratorLocation,
        shouldCallReturnFunctionLocation,
        shouldCallReturnFunctionLocationFinally,
        regException,
        regOffset,
        byteCodeGenerator,
        funcInfo);

    funcInfo->ReleaseTmpRegister(iteratorLocation);

    byteCodeGenerator->EndStatement(lhs);
}

void EmitNameInvoke(Js::RegSlot lhsLocation,
    Js::RegSlot objectLocation,
    ParseNodePtr nameNode,
    ByteCodeGenerator* byteCodeGenerator,
    FuncInfo* funcInfo)
{TRACE_IT(39468);
    Assert(nameNode != nullptr);
    if (nameNode->nop == knopComputedName)
    {TRACE_IT(39469);
        ParseNodePtr pnode1 = nameNode->sxUni.pnode1;
        Emit(pnode1, byteCodeGenerator, funcInfo, false/*isConstructorCall*/);

        byteCodeGenerator->Writer()->Element(Js::OpCode::LdElemI_A, lhsLocation, objectLocation, pnode1->location);
        funcInfo->ReleaseLoc(pnode1);
    }
    else
    {TRACE_IT(39470);
        Assert(nameNode->nop == knopName || nameNode->nop == knopStr);
        Symbol *sym = nameNode->sxPid.sym;
        Js::PropertyId propertyId = sym ? sym->EnsurePosition(byteCodeGenerator) : nameNode->sxPid.pid->GetPropertyId();

        uint cacheId = funcInfo->FindOrAddInlineCacheId(objectLocation, propertyId, false/*isLoadMethod*/, false/*isStore*/);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, lhsLocation, objectLocation, cacheId);
    }
}

void EmitDestructuredValueOrInitializer(ParseNodePtr lhsElementNode,
    Js::RegSlot rhsLocation,
    ParseNodePtr initializer,
    bool isNonPatternAssignmentTarget,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39471);
    // If we have initializer we need to see if the destructured value is undefined or not - if it is undefined we need to assign initializer

    Js::ByteCodeLabel useDefault = -1;
    Js::ByteCodeLabel end = -1;
    Js::RegSlot rhsLocationTmp = rhsLocation;

    if (initializer != nullptr)
    {TRACE_IT(39472);
        rhsLocationTmp = funcInfo->AcquireTmpRegister();

        useDefault = byteCodeGenerator->Writer()->DefineLabel();
        end = byteCodeGenerator->Writer()->DefineLabel();

        byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrSrEq_A, useDefault, rhsLocation, funcInfo->undefinedConstantRegister);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, rhsLocationTmp, rhsLocation);

        byteCodeGenerator->Writer()->Br(end);
        byteCodeGenerator->Writer()->MarkLabel(useDefault);

        Emit(initializer, byteCodeGenerator, funcInfo, false/*isConstructorCall*/);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, rhsLocationTmp, initializer->location);
        funcInfo->ReleaseLoc(initializer);

        byteCodeGenerator->Writer()->MarkLabel(end);
    }

    if (lhsElementNode->nop == knopArrayPattern)
    {
        EmitDestructuredArray(lhsElementNode, rhsLocationTmp, byteCodeGenerator, funcInfo);
    }
    else if (lhsElementNode->nop == knopObjectPattern)
    {
        EmitDestructuredObject(lhsElementNode, rhsLocationTmp, byteCodeGenerator, funcInfo);
    }
    else if (isNonPatternAssignmentTarget)
    {
        EmitAssignment(nullptr, lhsElementNode, rhsLocationTmp, byteCodeGenerator, funcInfo);
    }
    else
    {
        EmitDestructuredElement(lhsElementNode, rhsLocationTmp, byteCodeGenerator, funcInfo);
    }

    if (initializer != nullptr)
    {TRACE_IT(39473);
        funcInfo->ReleaseTmpRegister(rhsLocationTmp);
    }
}

void EmitDestructuredObjectMember(ParseNodePtr memberNode,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39474);
    Assert(memberNode->nop == knopObjectPatternMember);

    Js::RegSlot nameLocation = funcInfo->AcquireTmpRegister();
    EmitNameInvoke(nameLocation, rhsLocation, memberNode->sxBin.pnode1, byteCodeGenerator, funcInfo);

    // Imagine we are transforming
    // {x:x1} = {} to x1 = {}.x  (here x1 is the second node of the member but that is our lhsnode)

    ParseNodePtr lhsElementNode = memberNode->sxBin.pnode2;
    ParseNodePtr init = nullptr;
    if (lhsElementNode->IsVarLetOrConst())
    {TRACE_IT(39475);
        init = lhsElementNode->sxVar.pnodeInit;
    }
    else if (lhsElementNode->nop == knopAsg)
    {TRACE_IT(39476);
        init = lhsElementNode->sxBin.pnode2;
        lhsElementNode = lhsElementNode->sxBin.pnode1;
    }

    EmitDestructuredValueOrInitializer(lhsElementNode, nameLocation, init, false /*isNonPatternAssignmentTarget*/, byteCodeGenerator, funcInfo);

    funcInfo->ReleaseTmpRegister(nameLocation);
}

void EmitDestructuredObject(ParseNode *lhs,
    Js::RegSlot rhsLocationOrig,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39477);
    Assert(lhs->nop == knopObjectPattern);
    ParseNodePtr pnode1 = lhs->sxUni.pnode1;

    byteCodeGenerator->StartStatement(lhs);

    Js::ByteCodeLabel skipThrow = byteCodeGenerator->Writer()->DefineLabel();
    Js::RegSlot rhsLocation = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, rhsLocation, rhsLocationOrig);
    byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrNeq_A, skipThrow, rhsLocation, funcInfo->undefinedConstantRegister);
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_ObjectCoercible));
    byteCodeGenerator->Writer()->MarkLabel(skipThrow);

    if (pnode1 != nullptr)
    {TRACE_IT(39478);
        Assert(pnode1->nop == knopList || pnode1->nop == knopObjectPatternMember);

        ParseNodePtr current = pnode1;
        while (current->nop == knopList)
        {TRACE_IT(39479);
            ParseNodePtr memberNode = current->sxBin.pnode1;
            EmitDestructuredObjectMember(memberNode, rhsLocation, byteCodeGenerator, funcInfo);
            current = current->sxBin.pnode2;
        }
        EmitDestructuredObjectMember(current, rhsLocation, byteCodeGenerator, funcInfo);
    }

    funcInfo->ReleaseTmpRegister(rhsLocation);
    byteCodeGenerator->EndStatement(lhs);
}

void EmitAssignment(
    ParseNode *asgnNode,
    ParseNode *lhs,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39480);
    switch (lhs->nop)
    {
        // assignment to a local or global variable
    case knopVarDecl:
    case knopLetDecl:
    case knopConstDecl:
    {TRACE_IT(39481);
        Symbol *sym = lhs->sxVar.sym;
        Assert(sym != nullptr);
        byteCodeGenerator->EmitPropStore(rhsLocation, sym, nullptr, funcInfo, lhs->nop == knopLetDecl, lhs->nop == knopConstDecl);
        break;
    }

    case knopName:
    {TRACE_IT(39482);
        byteCodeGenerator->EmitPropStore(rhsLocation, lhs->sxPid.sym, lhs->sxPid.pid, funcInfo);
        break;
    }

    // x.y =
    case knopDot:
    {TRACE_IT(39483);
        // PutValue(x, "y", rhs)
        Js::PropertyId propertyId = lhs->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

        uint cacheId = funcInfo->FindOrAddInlineCacheId(lhs->sxBin.pnode1->location, propertyId, false, true);
        if (lhs->sxBin.pnode1->nop == knopSuper)
        {TRACE_IT(39484);
            Js::RegSlot tmpReg = byteCodeGenerator->EmitLdObjProto(Js::OpCode::LdHomeObjProto, funcInfo->superRegister, funcInfo);
            byteCodeGenerator->Writer()->PatchablePropertyWithThisPtr(Js::OpCode::StSuperFld, rhsLocation, tmpReg, funcInfo->thisPointerRegister, cacheId);
        }
        else
        {TRACE_IT(39485);
            byteCodeGenerator->Writer()->PatchableProperty(
                ByteCodeGenerator::GetStFldOpCode(funcInfo, false, false, false, false), rhsLocation, lhs->sxBin.pnode1->location, cacheId);
        }

        break;
    }

    case knopIndex:
    {TRACE_IT(39486);
        byteCodeGenerator->Writer()->Element(
            ByteCodeGenerator::GetStElemIOpCode(funcInfo),
            rhsLocation, lhs->sxBin.pnode1->location, lhs->sxBin.pnode2->location);
        break;
    }

    case knopObjectPattern:
    {TRACE_IT(39487);
        Assert(byteCodeGenerator->IsES6DestructuringEnabled());
        // Copy the rhs value to be the result of the assignment if needed.
        if (asgnNode != nullptr)
        {TRACE_IT(39488);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, asgnNode->location, rhsLocation);
        }
        return EmitDestructuredObject(lhs, rhsLocation, byteCodeGenerator, funcInfo);
    }

    case knopArrayPattern:
    {TRACE_IT(39489);
        Assert(byteCodeGenerator->IsES6DestructuringEnabled());
        // Copy the rhs value to be the result of the assignment if needed.
        if (asgnNode != nullptr)
        {TRACE_IT(39490);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, asgnNode->location, rhsLocation);
        }
        return EmitDestructuredArray(lhs, rhsLocation, byteCodeGenerator, funcInfo);
    }

    case knopArray:
    case knopObject:
        // Assignment to array/object can get through to byte code gen when the parser fails to convert destructuring
        // assignment to pattern (because of structural mismatch between LHS & RHS?). Revisit when we nail
        // down early vs. runtime errors for destructuring.
        byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_CantAssignTo));
        break;

    default:
        Assert(!PHASE_ON1(Js::EarlyReferenceErrorsPhase));
        byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_CantAssignTo));
        break;
    }

    if (asgnNode != nullptr)
    {TRACE_IT(39491);
        // We leave it up to the caller to pass this node only if the assignment expression is used.
        if (asgnNode->location != rhsLocation)
        {TRACE_IT(39492);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, asgnNode->location, rhsLocation);
        }
    }
}

void EmitLoad(
    ParseNode *lhs,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39493);
    // Emit the instructions to load the value into the LHS location. Do not assign/free any temps
    // in the process.
    // We usually get here as part of an op-equiv expression: x.y += z;
    // In such a case, x has to be emitted first, then the value of x.y loaded (by this function), then z emitted.
    switch (lhs->nop)
    {

        // load of a local or global variable
    case knopName:
    {TRACE_IT(39494);
        funcInfo->AcquireLoc(lhs);
        byteCodeGenerator->EmitPropLoad(lhs->location, lhs->sxPid.sym, lhs->sxPid.pid, funcInfo);
        break;
    }

    // = x.y
    case knopDot:
    {TRACE_IT(39495);
        // get field id for "y"
        Js::PropertyId propertyId = lhs->sxBin.pnode2->sxPid.PropertyIdFromNameNode();
        funcInfo->AcquireLoc(lhs);
        EmitReference(lhs, byteCodeGenerator, funcInfo);
        uint cacheId = funcInfo->FindOrAddInlineCacheId(lhs->sxBin.pnode1->location, propertyId, false, false);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, lhs->location, lhs->sxBin.pnode1->location, cacheId);
        break;
    }

    case knopIndex:
        funcInfo->AcquireLoc(lhs);
        EmitReference(lhs, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->Element(
            Js::OpCode::LdElemI_A, lhs->location, lhs->sxBin.pnode1->location, lhs->sxBin.pnode2->location);
        break;

        // f(x) +=
    case knopCall:
        funcInfo->AcquireLoc(lhs);
        EmitReference(lhs, byteCodeGenerator, funcInfo);
        EmitCall(lhs, /*rhs=*/ Js::Constants::NoRegister, byteCodeGenerator, funcInfo, /*fReturnValue=*/ false, /*fEvaluateComponents=*/ false, /*fHasNewTarget=*/ false);
        break;

    default:
        funcInfo->AcquireLoc(lhs);
        Emit(lhs, byteCodeGenerator, funcInfo, false);
        break;
    }
}

void EmitList(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39496);
    if (pnode != nullptr)
    {TRACE_IT(39497);
        while (pnode->nop == knopList)
        {TRACE_IT(39498);
            byteCodeGenerator->EmitTopLevelStatement(pnode->sxBin.pnode1, funcInfo, false);
            pnode = pnode->sxBin.pnode2;
        }
        byteCodeGenerator->EmitTopLevelStatement(pnode, funcInfo, false);
    }
}

void EmitSpreadArgToListBytecodeInstr(ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, Js::RegSlot argLoc, Js::ProfileId callSiteId, Js::ArgSlot &argIndex)
{TRACE_IT(39499);
    Js::RegSlot regVal = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdCustomSpreadIteratorList, regVal, argLoc);
    byteCodeGenerator->Writer()->ArgOut<true>(++argIndex, regVal, callSiteId);
    funcInfo->ReleaseTmpRegister(regVal);
}

size_t EmitArgs(
    ParseNode *pnode,
    BOOL fAssignRegs,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    Js::AuxArray<uint32> *spreadIndices = nullptr
    )
{TRACE_IT(39500);
    Js::ArgSlot argIndex = 0;
    Js::ArgSlot spreadIndex = 0;

    if (pnode != nullptr)
    {TRACE_IT(39501);
        while (pnode->nop == knopList)
        {TRACE_IT(39502);
            // If this is a put, the arguments have already been evaluated (see EmitReference).
            // We just need to emit the ArgOut instructions.
            if (fAssignRegs)
            {TRACE_IT(39503);
                Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
            }

            if (pnode->sxBin.pnode1->nop == knopEllipsis)
            {TRACE_IT(39504);
                Assert(spreadIndices != nullptr);
                spreadIndices->elements[spreadIndex++] = argIndex + 1; // account for 'this'
                EmitSpreadArgToListBytecodeInstr(byteCodeGenerator, funcInfo, pnode->sxBin.pnode1->location, callSiteId, argIndex);
            }
            else
            {TRACE_IT(39505);
                byteCodeGenerator->Writer()->ArgOut<true>(++argIndex, pnode->sxBin.pnode1->location, callSiteId);
            }
            if (fAssignRegs)
            {TRACE_IT(39506);
                funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
            }

            pnode = pnode->sxBin.pnode2;
        }

        // If this is a put, the call target has already been evaluated (see EmitReference).
        if (fAssignRegs)
        {
            Emit(pnode, byteCodeGenerator, funcInfo, false);
        }

        if (pnode->nop == knopEllipsis)
        {TRACE_IT(39507);
            Assert(spreadIndices != nullptr);
            spreadIndices->elements[spreadIndex++] = argIndex + 1; // account for 'this'
            EmitSpreadArgToListBytecodeInstr(byteCodeGenerator, funcInfo, pnode->location, callSiteId, argIndex);
        }
        else
        {TRACE_IT(39508);
            byteCodeGenerator->Writer()->ArgOut<true>(++argIndex, pnode->location, callSiteId);
        }

        if (fAssignRegs)
        {TRACE_IT(39509);
            funcInfo->ReleaseLoc(pnode);
        }
    }

    return argIndex;
}

void EmitArgListStart(
    Js::RegSlot thisLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId)
{TRACE_IT(39510);
    if (thisLocation != Js::Constants::NoRegister)
    {TRACE_IT(39511);
        // Emit the "this" object.
        byteCodeGenerator->Writer()->ArgOut<true>(0, thisLocation, callSiteId);
    }
}

Js::ArgSlot EmitArgListEnd(
    ParseNode *pnode,
    Js::RegSlot rhsLocation,
    Js::RegSlot thisLocation,
    Js::RegSlot evalLocation,
    Js::RegSlot newTargetLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    size_t argIndex,
    Js::ProfileId callSiteId)
{TRACE_IT(39512);
    BOOL fEvalInModule = false;
    BOOL fIsPut = (rhsLocation != Js::Constants::NoRegister);
    BOOL fIsEval = (evalLocation != Js::Constants::NoRegister);
    BOOL fHasNewTarget = (newTargetLocation != Js::Constants::NoRegister);

    Js::ArgSlot argSlotIndex = (Js::ArgSlot) argIndex;
    static const Js::ArgSlot maxExtraArgSlot = 4;  // max(extraEvalArg, extraArg), where extraEvalArg==2 (moduleRoot,env), extraArg==4 (this, eval, evalInModule, newTarget)

    // check for integer overflow with margin for increments below to calculate argument count
    if ((size_t)argSlotIndex != argIndex || argSlotIndex + maxExtraArgSlot < argSlotIndex)
    {TRACE_IT(39513);
        Js::Throw::OutOfMemory();
    }

    Js::ArgSlot evalIndex;

    if (fIsPut)
    {TRACE_IT(39514);
        // Emit the assigned value as an additional operand. Note that the value has already been evaluated.
        // We just need to emit the ArgOut instruction.
        argSlotIndex++;
        byteCodeGenerator->Writer()->ArgOut<true>(argSlotIndex, rhsLocation, callSiteId);
    }

    if (fIsEval && argSlotIndex > 0)
    {TRACE_IT(39515);
        Assert(!fHasNewTarget);

        // Pass the frame display as an extra argument to "eval".
        // Do this only if eval is called with some args
        Js::RegSlot evalEnv;
        if (funcInfo->IsGlobalFunction() && !(funcInfo->GetIsStrictMode() && byteCodeGenerator->GetFlags() & fscrEval))
        {TRACE_IT(39516);
            // Use current environment as the environment for the function being called when:
            // - this is the root global function (not an eval's global function)
            // - this is an eval's global function that is not in strict mode (see else block)
            evalEnv = funcInfo->GetEnvRegister();
        }
        else
        {TRACE_IT(39517);
            // Use the frame display as the environment for the function being called when:
            // - this is not a global function and thus it will have its own scope
            // - this is an eval's global function that is in strict mode, since in strict mode the eval's global function
            //   has its own scope
            evalEnv = funcInfo->frameDisplayRegister;
        }

        evalEnv = byteCodeGenerator->PrependLocalScopes(evalEnv, evalLocation, funcInfo);

        Js::ModuleID moduleID = byteCodeGenerator->GetModuleID();
        if (moduleID != kmodGlobal)
        {TRACE_IT(39518);
            // Pass both the module root and the environment.
            fEvalInModule = true;
            byteCodeGenerator->Writer()->ArgOut<true>(argSlotIndex + 1, ByteCodeGenerator::RootObjectRegister, callSiteId);
            evalIndex = argSlotIndex + 2;
        }
        else
        {TRACE_IT(39519);
            // Just pass the environment.
            evalIndex = argSlotIndex + 1;
        }

        if (evalEnv == funcInfo->GetEnvRegister() || evalEnv == funcInfo->frameDisplayRegister)
        {TRACE_IT(39520);
            byteCodeGenerator->Writer()->ArgOutEnv(evalIndex);
        }
        else
        {TRACE_IT(39521);
            byteCodeGenerator->Writer()->ArgOut<false>(evalIndex, evalEnv, callSiteId);
        }
    }

    if (fHasNewTarget)
    {TRACE_IT(39522);
        Assert(!fIsEval);

        byteCodeGenerator->Writer()->ArgOut<true>(argSlotIndex + 1, newTargetLocation, callSiteId);
    }

    Js::ArgSlot argIntCount = argSlotIndex + 1 + (Js::ArgSlot)fIsEval + (Js::ArgSlot)fEvalInModule + (Js::ArgSlot)fHasNewTarget;

    // eval and no args passed, return 1 as argument count
    if (fIsEval && pnode == nullptr)
    {TRACE_IT(39523);
        return 1;
    }

    return argIntCount;
}

Js::ArgSlot EmitArgList(
    ParseNode *pnode,
    Js::RegSlot rhsLocation,
    Js::RegSlot thisLocation,
    Js::RegSlot newTargetLocation,
    BOOL fIsEval,
    BOOL fAssignRegs,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    uint16 spreadArgCount = 0,
    Js::AuxArray<uint32> **spreadIndices = nullptr)
{
    // This function emits the arguments for a call.
    // ArgOut's with uses immediately following defs.

    EmitArgListStart(thisLocation, byteCodeGenerator, funcInfo, callSiteId);

    Js::RegSlot evalLocation = Js::Constants::NoRegister;

    //
    // If Emitting arguments for eval and assigning registers, get a tmpLocation for eval.
    // This would be used while generating frameDisplay in EmitArgListEnd.
    //
    if (fIsEval)
    {TRACE_IT(39524);
        evalLocation = funcInfo->AcquireTmpRegister();
    }

    if (spreadArgCount > 0)
    {TRACE_IT(39525);
        const size_t extraAlloc = spreadArgCount * sizeof(uint32);
        Assert(spreadIndices != nullptr);
        *spreadIndices = AnewPlus(byteCodeGenerator->GetAllocator(), extraAlloc, Js::AuxArray<uint32>, spreadArgCount);
    }

    size_t argIndex = EmitArgs(pnode, fAssignRegs, byteCodeGenerator, funcInfo, callSiteId, spreadIndices == nullptr ? nullptr : *spreadIndices);

    Js::ArgSlot argumentsCount = EmitArgListEnd(pnode, rhsLocation, thisLocation, evalLocation, newTargetLocation, byteCodeGenerator, funcInfo, argIndex, callSiteId);

    if (fIsEval)
    {TRACE_IT(39526);
        funcInfo->ReleaseTmpRegister(evalLocation);
    }

    return argumentsCount;
}

void EmitConstantArgsToVarArray(ByteCodeGenerator *byteCodeGenerator, __out_ecount(argCount) Js::Var *vars, ParseNode *args, uint argCount)
{TRACE_IT(39527);
    uint index = 0;
    while (args->nop == knopList && index < argCount)
    {TRACE_IT(39528);
        if (args->sxBin.pnode1->nop == knopInt)
        {TRACE_IT(39529);
            int value = args->sxBin.pnode1->sxInt.lw;
            vars[index++] = Js::TaggedInt::ToVarUnchecked(value);
        }
        else if (args->sxBin.pnode1->nop == knopFlt)
        {TRACE_IT(39530);
            Js::Var number = Js::JavascriptNumber::New(args->sxBin.pnode1->sxFlt.dbl, byteCodeGenerator->GetScriptContext());
#if ! FLOATVAR
            byteCodeGenerator->GetScriptContext()->BindReference(number);
#endif
            vars[index++] = number;
        }
        else
        {TRACE_IT(39531);
            AnalysisAssert(false);
        }
        args = args->sxBin.pnode2;
    }

    if (index == argCount)
    {TRACE_IT(39532);
        Assert(false);
        Js::Throw::InternalError();
        return;
    }

    if (args->nop == knopInt)
    {TRACE_IT(39533);
        int value = args->sxInt.lw;
        vars[index++] = Js::TaggedInt::ToVarUnchecked(value);
    }
    else if (args->nop == knopFlt)
    {TRACE_IT(39534);
        Js::Var number = Js::JavascriptNumber::New(args->sxFlt.dbl, byteCodeGenerator->GetScriptContext());
#if ! FLOATVAR
        byteCodeGenerator->GetScriptContext()->BindReference(number);
#endif
        vars[index++] = number;
    }
    else
    {TRACE_IT(39535);
        AnalysisAssert(false);
    }
}

void EmitConstantArgsToIntArray(ByteCodeGenerator *byteCodeGenerator, __out_ecount(argCount) int32 *vars, ParseNode *args, uint argCount)
{TRACE_IT(39536);
    uint index = 0;
    while (args->nop == knopList && index < argCount)
    {TRACE_IT(39537);
        Assert(args->sxBin.pnode1->nop == knopInt);
        vars[index++] = args->sxBin.pnode1->sxInt.lw;
        args = args->sxBin.pnode2;
    }

    if (index == argCount)
    {TRACE_IT(39538);
        Assert(false);
        Js::Throw::InternalError();
        return;
    }

    Assert(args->nop == knopInt);
    vars[index++] = args->sxInt.lw;

    Assert(index == argCount);
}

void EmitConstantArgsToFltArray(ByteCodeGenerator *byteCodeGenerator, __out_ecount(argCount) double *vars, ParseNode *args, uint argCount)
{TRACE_IT(39539);
    uint index = 0;
    while (args->nop == knopList && index < argCount)
    {TRACE_IT(39540);
        OpCode nop = args->sxBin.pnode1->nop;
        if (nop == knopInt)
        {TRACE_IT(39541);
            vars[index++] = (double)args->sxBin.pnode1->sxInt.lw;
        }
        else
        {TRACE_IT(39542);
            Assert(nop == knopFlt);
            vars[index++] = args->sxBin.pnode1->sxFlt.dbl;
        }
        args = args->sxBin.pnode2;
    }

    if (index == argCount)
    {TRACE_IT(39543);
        Assert(false);
        Js::Throw::InternalError();
        return;
    }

    if (args->nop == knopInt)
    {TRACE_IT(39544);
        vars[index++] = (double)args->sxInt.lw;
    }
    else
    {TRACE_IT(39545);
        Assert(args->nop == knopFlt);
        vars[index++] = args->sxFlt.dbl;
    }

    Assert(index == argCount);
}

//
// Called when we have new Ctr(constant, constant...)
//
Js::ArgSlot EmitNewObjectOfConstants(
    ParseNode *pnode,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    unsigned int argCount)
{TRACE_IT(39546);
    EmitArgListStart(Js::Constants::NoRegister, byteCodeGenerator, funcInfo, Js::Constants::NoProfileId);

    // Create the vars array
    Js::VarArrayVarCount *vars = AnewPlus(byteCodeGenerator->GetAllocator(), (argCount - 1) * sizeof(Js::Var), Js::VarArrayVarCount, Js::TaggedInt::ToVarUnchecked(argCount - 1));

    // Emit all constants to the vars array
    EmitConstantArgsToVarArray(byteCodeGenerator, vars->elements, pnode->sxCall.pnodeArgs, argCount - 1);

    // Finish the arg list
    Js::ArgSlot actualArgCount = EmitArgListEnd(
        pnode->sxCall.pnodeArgs,
        Js::Constants::NoRegister,
        Js::Constants::NoRegister,
        Js::Constants::NoRegister,
        Js::Constants::NoRegister,
        byteCodeGenerator,
        funcInfo,
        argCount - 1,
        Js::Constants::NoProfileId);

    // Make sure the cacheId to regSlot map in the ByteCodeWriter is left in a consistent state after writing NewScObject_A
    byteCodeGenerator->Writer()->RemoveEntryForRegSlotFromCacheIdMap(pnode->sxCall.pnodeTarget->location);

    // Generate the opcode with vars
    byteCodeGenerator->Writer()->AuxiliaryContext(
        Js::OpCode::NewScObject_A,
        funcInfo->AcquireLoc(pnode),
        vars,
        sizeof(Js::VarArray) + (argCount - 1) * sizeof(Js::Var),
        pnode->sxCall.pnodeTarget->location);

    AdeletePlus(byteCodeGenerator->GetAllocator(), (argCount - 1) * sizeof(Js::VarArrayVarCount), vars);

    return actualArgCount;
}

void EmitMethodFld(bool isRoot, bool isScoped, Js::RegSlot location, Js::RegSlot callObjLocation, Js::PropertyId propertyId, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, bool registerCacheIdForCall = true)
{TRACE_IT(39547);
    Js::OpCode opcode;
    if (!isRoot)
    {TRACE_IT(39548);
        if (callObjLocation == funcInfo->frameObjRegister)
        {TRACE_IT(39549);
            opcode = Js::OpCode::LdLocalMethodFld;
        }
        else
        {TRACE_IT(39550);
            opcode = Js::OpCode::LdMethodFld;
        }
    }
    else if (isScoped)
    {TRACE_IT(39551);
        opcode = Js::OpCode::ScopedLdMethodFld;
    }
    else
    {TRACE_IT(39552);
        opcode = Js::OpCode::LdRootMethodFld;
    }

    if (isScoped || !isRoot)
    {TRACE_IT(39553);
        Assert(isScoped || !isRoot || callObjLocation == ByteCodeGenerator::RootObjectRegister);
        uint cacheId = funcInfo->FindOrAddInlineCacheId(callObjLocation, propertyId, true, false);
        if (callObjLocation == funcInfo->frameObjRegister)
        {TRACE_IT(39554);
            byteCodeGenerator->Writer()->ElementP(opcode, location, cacheId, false /*isCtor*/, registerCacheIdForCall);
        }
        else
        {TRACE_IT(39555);
            byteCodeGenerator->Writer()->PatchableProperty(opcode, location, callObjLocation, cacheId, false /*isCtor*/, registerCacheIdForCall);
        }
    }
    else
    {TRACE_IT(39556);
        uint cacheId = funcInfo->FindOrAddRootObjectInlineCacheId(propertyId, true, false);
        byteCodeGenerator->Writer()->PatchableRootProperty(opcode, location, cacheId, true, false, registerCacheIdForCall);
    }
}

void EmitMethodFld(ParseNode *pnode, Js::RegSlot callObjLocation, Js::PropertyId propertyId, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, bool registerCacheIdForCall = true)
{TRACE_IT(39557);
    // Load a call target of the form x.y(). (Call target may be a plain knopName if we're getting it from
    // the global object, etc.)
    bool isRoot = pnode->nop == knopName && (pnode->sxPid.sym == nullptr || pnode->sxPid.sym->GetIsGlobal());
    bool isScoped = (byteCodeGenerator->GetFlags() & fscrEval) != 0 ||
        (isRoot && callObjLocation != ByteCodeGenerator::RootObjectRegister);

    EmitMethodFld(isRoot, isScoped, pnode->location, callObjLocation, propertyId, byteCodeGenerator, funcInfo, registerCacheIdForCall);
}

// lhs.apply(this, arguments);
void EmitApplyCall(ParseNode* pnode, Js::RegSlot rhsLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo, BOOL fReturnValue)
{TRACE_IT(39558);
    ParseNode* applyNode = pnode->sxCall.pnodeTarget;
    ParseNode* thisNode = pnode->sxCall.pnodeArgs->sxBin.pnode1;
    Assert(applyNode->nop == knopDot);

    ParseNode* funcNode = applyNode->sxBin.pnode1;
    Js::ByteCodeLabel slowPath = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel afterSlowPath = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel argsAlreadyCreated = byteCodeGenerator->Writer()->DefineLabel();

    Assert(applyNode->nop == knopDot);

    Emit(funcNode, byteCodeGenerator, funcInfo, false);

    funcInfo->AcquireLoc(applyNode);
    Js::PropertyId propertyId = applyNode->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

    // As we won't be emitting a call instruction for apply, no need to register the cacheId for apply
    // load to be associated with the call. This is also required, as in the absence of a corresponding
    // call for apply, we won't remove the entry for "apply" cacheId from
    // ByteCodeWriter::callRegToLdFldCacheIndexMap, which is contrary to our assumption that we would
    // have removed an entry from a map upon seeing its corresponding call.
    EmitMethodFld(applyNode, funcNode->location, propertyId, byteCodeGenerator, funcInfo, false /*registerCacheIdForCall*/);

    Symbol *argSym = funcInfo->GetArgumentsSymbol();
    Assert(argSym && argSym->GetIsArguments());
    Js::RegSlot argumentsLoc = argSym->GetLocation();

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdArgumentsFromFrame, argumentsLoc);
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrNotNull_A, argsAlreadyCreated, argumentsLoc);

    // If apply is overridden, bail to slow path.
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrFncNeqApply, slowPath, applyNode->location);

    // Note: acquire and release a temp register for this stack arg pointer instead of trying to stash it
    // in funcInfo->stackArgReg. Otherwise, we'll needlessly load and store it in jitted loop bodies and
    // may crash if we try to unbox it on the store.
    Js::RegSlot stackArgReg = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdStackArgPtr, stackArgReg);

    Js::RegSlot argCountLocation = funcInfo->AcquireTmpRegister();

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdArgCnt, argCountLocation);
    byteCodeGenerator->Writer()->Reg5(Js::OpCode::ApplyArgs, funcNode->location, funcNode->location, thisNode->location, stackArgReg, argCountLocation);

    funcInfo->ReleaseTmpRegister(argCountLocation);
    funcInfo->ReleaseTmpRegister(stackArgReg);
    funcInfo->ReleaseLoc(applyNode);
    funcInfo->ReleaseLoc(funcNode);

    // Clear these nodes as they are going to be used to re-generate the slow path.
    VisitClearTmpRegs(applyNode, byteCodeGenerator, funcInfo);
    VisitClearTmpRegs(funcNode, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->Br(afterSlowPath);

    // slow path
    byteCodeGenerator->Writer()->MarkLabel(slowPath);
    if (funcInfo->frameObjRegister != Js::Constants::NoRegister)
    {TRACE_IT(39559);
        byteCodeGenerator->EmitScopeObjectInit(funcInfo);
    }
    byteCodeGenerator->LoadHeapArguments(funcInfo);

    byteCodeGenerator->Writer()->MarkLabel(argsAlreadyCreated);
    EmitCall(pnode, rhsLocation, byteCodeGenerator, funcInfo, fReturnValue, /*fEvaluateComponents*/true, /*fHasNewTarget*/false);
    byteCodeGenerator->Writer()->MarkLabel(afterSlowPath);
}

void EmitMethodElem(ParseNode *pnode, Js::RegSlot callObjLocation, Js::RegSlot indexLocation, ByteCodeGenerator *byteCodeGenerator)
{TRACE_IT(39560);
    // Load a call target of the form x[y]().
    byteCodeGenerator->Writer()->Element(Js::OpCode::LdMethodElem, pnode->location, callObjLocation, indexLocation);
}

void EmitCallTargetNoEvalComponents(
    ParseNode *pnodeTarget,
    BOOL fSideEffectArgs,
    Js::RegSlot *thisLocation,
    Js::RegSlot *callObjLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39561);
    // We first get a reference to the call target, then evaluate the arguments, then
    // evaluate the call target.

    // - emit reference to target
    //    - copy instance to scratch reg if necessary.
    //    - assign this
    //    - assign instance for dynamic/global name
    // - emit args
    // - do call (CallFld/Elem/I)

    switch (pnodeTarget->nop)
    {
    case knopDot:
        *thisLocation = pnodeTarget->sxBin.pnode1->location;
        *callObjLocation = pnodeTarget->sxBin.pnode1->location;
        break;

    case knopIndex:
        *thisLocation = pnodeTarget->sxBin.pnode1->location;
        *callObjLocation = pnodeTarget->sxBin.pnode1->location;
        break;

    case knopName:
        // If the call target is a name, do some extra work to get its instance and the "this" pointer.
        byteCodeGenerator->EmitLoadInstance(pnodeTarget->sxPid.sym, pnodeTarget->sxPid.pid, thisLocation, callObjLocation, funcInfo);
        if (*thisLocation == Js::Constants::NoRegister)
        {TRACE_IT(39562);
            *thisLocation = funcInfo->undefinedConstantRegister;
        }

        break;

    default:
        *thisLocation = funcInfo->undefinedConstantRegister;
        break;
    }
}

void EmitSuperMethodBegin(
    ParseNode *pnodeTarget,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39563);
    FuncInfo *parentFuncInfo = funcInfo;
    if (parentFuncInfo->IsLambda())
    {TRACE_IT(39564);
        parentFuncInfo = byteCodeGenerator->FindEnclosingNonLambda();
    }

    if (pnodeTarget->sxBin.pnode1->nop == knopSuper && parentFuncInfo->IsClassConstructor() && !parentFuncInfo->IsBaseClassConstructor())
    {TRACE_IT(39565);
        byteCodeGenerator->EmitScopeSlotLoadThis(funcInfo, funcInfo->thisPointerRegister, /*chkUndecl*/ true);
    }
}

void EmitCallTarget(
    ParseNode *pnodeTarget,
    BOOL fSideEffectArgs,
    Js::RegSlot *thisLocation,
    Js::RegSlot *callObjLocation,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39566);
    // - emit target
    //    - assign this
    // - emit args
    // - do call

    // The call target is fully evaluated before the argument list. Note that we're not handling
    // put-call cases here currently, as such cases only apply to host objects
    // and are very unlikely to behave differently depending on the order of evaluation.

    switch (pnodeTarget->nop)
    {
    case knopDot:
    {TRACE_IT(39567);
        funcInfo->AcquireLoc(pnodeTarget);
        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        if (fSideEffectArgs)
        {TRACE_IT(39568);
            // Though we're done with target evaluation after this point, still protect opnd1 from
            // arg side-effects as it's the "this" pointer.
            SaveOpndValue(pnodeTarget->sxBin.pnode1, funcInfo);
        }

        if ((pnodeTarget->sxBin.pnode2->nop == knopName) && ((pnodeTarget->sxBin.pnode2->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::apply) || (pnodeTarget->sxBin.pnode2->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::call)))
        {TRACE_IT(39569);
            pnodeTarget->sxBin.pnode1->SetIsCallApplyTargetLoad();
        }

        Emit(pnodeTarget->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        Js::PropertyId propertyId = pnodeTarget->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

        Js::RegSlot protoLocation =
            (pnodeTarget->sxBin.pnode1->nop == knopSuper) ?
            byteCodeGenerator->EmitLdObjProto(Js::OpCode::LdHomeObjProto, funcInfo->superRegister, funcInfo) :
            pnodeTarget->sxBin.pnode1->location;

        EmitSuperMethodBegin(pnodeTarget, byteCodeGenerator, funcInfo);
        EmitMethodFld(pnodeTarget, protoLocation, propertyId, byteCodeGenerator, funcInfo);

        // Function calls on the 'super' object should maintain current 'this' pointer
        *thisLocation = (pnodeTarget->sxBin.pnode1->nop == knopSuper) ? funcInfo->thisPointerRegister : pnodeTarget->sxBin.pnode1->location;
        break;
    }

    case knopIndex:
    {TRACE_IT(39570);
        funcInfo->AcquireLoc(pnodeTarget);
        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        if (fSideEffectArgs || !(ParseNode::Grfnop(pnodeTarget->sxBin.pnode2->nop) & fnopLeaf))
        {TRACE_IT(39571);
            // Though we're done with target evaluation after this point, still protect opnd1 from
            // arg or opnd2 side-effects as it's the "this" pointer.
            SaveOpndValue(pnodeTarget->sxBin.pnode1, funcInfo);
        }
        Emit(pnodeTarget->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        Emit(pnodeTarget->sxBin.pnode2, byteCodeGenerator, funcInfo, false);

        Js::RegSlot indexLocation = pnodeTarget->sxBin.pnode2->location;

        Js::RegSlot protoLocation =
            (pnodeTarget->sxBin.pnode1->nop == knopSuper) ?
            byteCodeGenerator->EmitLdObjProto(Js::OpCode::LdHomeObjProto, funcInfo->superRegister, funcInfo) :
            pnodeTarget->sxBin.pnode1->location;

        EmitSuperMethodBegin(pnodeTarget, byteCodeGenerator, funcInfo);
        EmitMethodElem(pnodeTarget, protoLocation, indexLocation, byteCodeGenerator);

        funcInfo->ReleaseLoc(pnodeTarget->sxBin.pnode2); // don't release indexLocation until after we use it.

        // Function calls on the 'super' object should maintain current 'this' pointer
        *thisLocation = (pnodeTarget->sxBin.pnode1->nop == knopSuper) ? funcInfo->thisPointerRegister : pnodeTarget->sxBin.pnode1->location;
        break;
    }

    case knopClassDecl:
    {
        Emit(pnodeTarget, byteCodeGenerator, funcInfo, false);
        // We won't always have an assigned this register (e.g. class expression calls.) We need undefined in this case.
        *thisLocation = funcInfo->thisPointerRegister == Js::Constants::NoRegister ? funcInfo->undefinedConstantRegister : funcInfo->thisPointerRegister;
        break;
    }

    case knopSuper:
    {
        Emit(pnodeTarget, byteCodeGenerator, funcInfo, false, /*isConstructorCall*/ true);  // reuse isConstructorCall ("new super()" is illegal)

        // Super calls should always use the new.target register unless we don't have one.
        // That could happen if we have an eval('super()') outside of a class constructor.
        if (funcInfo->newTargetRegister != Js::Constants::NoRegister)
        {TRACE_IT(39572);
            *thisLocation = funcInfo->newTargetRegister;
        }
        else
        {TRACE_IT(39573);
            *thisLocation = funcInfo->thisPointerRegister;
        }
        break;
    }

    case knopName:
    {TRACE_IT(39574);
        funcInfo->AcquireLoc(pnodeTarget);
        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        if (fSideEffectArgs)
        {
            SaveOpndValue(pnodeTarget, funcInfo);
        }
        byteCodeGenerator->EmitLoadInstance(pnodeTarget->sxPid.sym, pnodeTarget->sxPid.pid, thisLocation, callObjLocation, funcInfo);
        if (*callObjLocation != Js::Constants::NoRegister)
        {TRACE_IT(39575);
            // Load the call target as a property of the instance.
            Js::PropertyId propertyId = pnodeTarget->sxPid.PropertyIdFromNameNode();
            EmitMethodFld(pnodeTarget, *callObjLocation, propertyId, byteCodeGenerator, funcInfo);
            break;
        }

        // FALL THROUGH to evaluate call target.
    }

    default:
        // Assign the call target operand(s), putting them into expression temps if necessary to protect
        // them from side-effects.
        Emit(pnodeTarget, byteCodeGenerator, funcInfo, false);
        *thisLocation = funcInfo->undefinedConstantRegister;
        break;
    }

    // "This" pointer should have been assigned by the above.
    Assert(*thisLocation != Js::Constants::NoRegister);
}

void EmitCallI(
    ParseNode *pnode,
    BOOL fEvaluateComponents,
    BOOL fIsPut,
    BOOL fIsEval,
    BOOL fHasNewTarget,
    uint32 actualArgCount,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    Js::AuxArray<uint32> *spreadIndices = nullptr)
{TRACE_IT(39576);
    // Emit a call where the target is in a register, because it's either a local name or an expression we've
    // already evaluated.

    ParseNode *pnodeTarget = pnode->sxBin.pnode1;
    Js::OpCode op;
    Js::CallFlags callFlags = Js::CallFlags::CallFlags_None;
    uint spreadExtraAlloc = 0;

    Js::ArgSlot actualArgSlotCount = (Js::ArgSlot) actualArgCount;

    // check for integer overflow
    if ((size_t)actualArgSlotCount != actualArgCount)
    {TRACE_IT(39577);
        Js::Throw::OutOfMemory();
    }

    if (fIsPut)
    {TRACE_IT(39578);
        if (pnode->sxCall.spreadArgCount > 0)
        {TRACE_IT(39579);
            // TODO(tcare): We are disallowing spread with CallIPut for the moment. See DEVDIV2: 876387
            //              When CallIPut is migrated to the CallIExtended layout, this can be removed.
            byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_CantAsgCall));
        }
        // Grab a tmp register for the call result.
        Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
        byteCodeGenerator->Writer()->CallI(Js::OpCode::CallIFlags, tmpReg, pnodeTarget->location, actualArgSlotCount, callSiteId, Js::CallFlags::CallFlags_NewTarget);
        funcInfo->ReleaseTmpRegister(tmpReg);
    }
    else
    {TRACE_IT(39580);
        if (fEvaluateComponents)
        {TRACE_IT(39581);
            // Release the call target operands we assigned above. If we didn't assign them here,
            // we'll need them later, so we can't re-use them for the result of the call.
            funcInfo->ReleaseLoc(pnodeTarget);
        }
        // Grab a register for the call result.
        if (pnode->isUsed)
        {TRACE_IT(39582);
            funcInfo->AcquireLoc(pnode);
        }

        if (fIsEval)
        {TRACE_IT(39583);
            op = Js::OpCode::CallIExtendedFlags;
            callFlags = Js::CallFlags::CallFlags_ExtraArg;
        }
        else
        {TRACE_IT(39584);
            bool isSuperCall = pnodeTarget->nop == knopSuper;

            if (isSuperCall)
            {TRACE_IT(39585);
                callFlags = Js::CallFlags_New;
            }
            if (fHasNewTarget)
            {TRACE_IT(39586);
                callFlags = (Js::CallFlags) (callFlags | Js::CallFlags::CallFlags_ExtraArg | Js::CallFlags::CallFlags_NewTarget);
            }

            if (pnode->sxCall.spreadArgCount > 0)
            {TRACE_IT(39587);
                op = (isSuperCall || fHasNewTarget) ? Js::OpCode::CallIExtendedFlags : Js::OpCode::CallIExtended;
            }
            else
            {TRACE_IT(39588);
                op = (isSuperCall || fHasNewTarget) ? Js::OpCode::CallIFlags : Js::OpCode::CallI;
            }
        }

        if (op == Js::OpCode::CallI || op == Js::OpCode::CallIFlags)
        {TRACE_IT(39589);
            if (pnodeTarget->nop == knopSuper)
            {TRACE_IT(39590);
                Js::RegSlot tmpReg = byteCodeGenerator->EmitLdObjProto(Js::OpCode::LdFuncObjProto, pnodeTarget->location, funcInfo);
                byteCodeGenerator->Writer()->CallI(op, pnode->location, tmpReg, actualArgSlotCount, callSiteId, callFlags);
            }
            else
            {TRACE_IT(39591);
                byteCodeGenerator->Writer()->CallI(op, pnode->location, pnodeTarget->location, actualArgSlotCount, callSiteId, callFlags);
            }
        }
        else
        {TRACE_IT(39592);
            uint spreadIndicesSize = 0;
            Js::CallIExtendedOptions options = Js::CallIExtended_None;

            if (pnode->sxCall.spreadArgCount > 0)
            {TRACE_IT(39593);
                Assert(spreadIndices != nullptr);
                spreadExtraAlloc = spreadIndices->count * sizeof(uint32);
                spreadIndicesSize = sizeof(*spreadIndices) + spreadExtraAlloc;
                options = Js::CallIExtended_SpreadArgs;
            }

            if (pnodeTarget->nop == knopSuper)
            {TRACE_IT(39594);
                Js::RegSlot tmpReg = byteCodeGenerator->EmitLdObjProto(Js::OpCode::LdFuncObjProto, pnodeTarget->location, funcInfo);
                byteCodeGenerator->Writer()->CallIExtended(op, pnode->location, tmpReg, actualArgSlotCount, options, spreadIndices, spreadIndicesSize, callSiteId, callFlags);
            }
            else
            {TRACE_IT(39595);
                byteCodeGenerator->Writer()->CallIExtended(op, pnode->location, pnodeTarget->location, actualArgSlotCount, options, spreadIndices, spreadIndicesSize, callSiteId, callFlags);
            }
        }

        if (pnode->sxCall.spreadArgCount > 0)
        {TRACE_IT(39596);
            Assert(spreadExtraAlloc != 0);
            AdeletePlus(byteCodeGenerator->GetAllocator(), spreadExtraAlloc, spreadIndices);
        }
    }
}

void EmitCallInstrNoEvalComponents(
    ParseNode *pnode,
    BOOL fIsPut,
    BOOL fIsEval,
    Js::RegSlot thisLocation,
    Js::RegSlot callObjLocation,
    uint32 actualArgCount,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    Js::AuxArray<uint32> *spreadIndices = nullptr)
{TRACE_IT(39597);
    // Emit the call instruction. The call target is a reference at this point, and we evaluate
    // it as part of doing the actual call.
    // Note that we don't handle the (fEvaluateComponents == TRUE) case in this function.
    // (This function is only called on the !fEvaluateComponents branch in EmitCall.)

    ParseNode *pnodeTarget = pnode->sxBin.pnode1;

    switch (pnodeTarget->nop)
    {
    case knopDot:
    {TRACE_IT(39598);
        Assert(pnodeTarget->sxBin.pnode2->nop == knopName);
        Js::PropertyId propertyId = pnodeTarget->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

        EmitMethodFld(pnodeTarget, callObjLocation, propertyId, byteCodeGenerator, funcInfo);
        EmitCallI(pnode, /*fEvaluateComponents*/ FALSE, fIsPut, fIsEval, /*fHasNewTarget*/ FALSE, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
    }
    break;

    case knopIndex:
    {
        EmitMethodElem(pnodeTarget, pnodeTarget->sxBin.pnode1->location, pnodeTarget->sxBin.pnode2->location, byteCodeGenerator);
        EmitCallI(pnode, /*fEvaluateComponents*/ FALSE, fIsPut, fIsEval, /*fHasNewTarget*/ FALSE, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
    }
    break;

    case knopName:
    {TRACE_IT(39599);
        if (callObjLocation != Js::Constants::NoRegister)
        {TRACE_IT(39600);
            // We still have to get the property from its instance, so emit CallFld.
            if (thisLocation != callObjLocation)
            {TRACE_IT(39601);
                funcInfo->ReleaseTmpRegister(thisLocation);
            }
            funcInfo->ReleaseTmpRegister(callObjLocation);

            Js::PropertyId propertyId = pnodeTarget->sxPid.PropertyIdFromNameNode();
            EmitMethodFld(pnodeTarget, callObjLocation, propertyId, byteCodeGenerator, funcInfo);
            EmitCallI(pnode, /*fEvaluateComponents*/ FALSE, fIsPut, fIsEval, /*fHasNewTarget*/ FALSE, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
            break;
        }
    }
    // FALL THROUGH

    default:
        EmitCallI(pnode, /*fEvaluateComponents*/ FALSE, fIsPut, fIsEval, /*fHasNewTarget*/ FALSE, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
        break;
    }
}

void EmitCallInstr(
    ParseNode *pnode,
    BOOL fIsPut,
    BOOL fIsEval,
    BOOL fHasNewTarget,
    Js::RegSlot thisLocation,
    Js::RegSlot callObjLocation,
    uint32 actualArgCount,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    Js::ProfileId callSiteId,
    Js::AuxArray<uint32> *spreadIndices = nullptr)
{TRACE_IT(39602);
    // Emit a call instruction. The call target has been fully evaluated already, so we always
    // emit a CallI through the register that holds the target value.
    // Note that we don't handle !fEvaluateComponents cases at this point.
    // (This function is only called on the fEvaluateComponents branch in EmitCall.)

    if (thisLocation != Js::Constants::NoRegister)
    {TRACE_IT(39603);
        funcInfo->ReleaseTmpRegister(thisLocation);
    }

    if (callObjLocation != Js::Constants::NoRegister &&
        callObjLocation != thisLocation)
    {TRACE_IT(39604);
        funcInfo->ReleaseTmpRegister(callObjLocation);
    }

    EmitCallI(pnode, /*fEvaluateComponents*/ TRUE, fIsPut, fIsEval, fHasNewTarget, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
}

void EmitNew(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{TRACE_IT(39605);
    Js::ArgSlot argCount = pnode->sxCall.argCount;
    argCount++; // include "this"

    BOOL fSideEffectArgs = FALSE;
    unsigned int tmpCount = CountArguments(pnode->sxCall.pnodeArgs, &fSideEffectArgs);
    Assert(argCount == tmpCount);

    if (argCount != (Js::ArgSlot)argCount)
    {TRACE_IT(39606);
        Js::Throw::OutOfMemory();
    }

    byteCodeGenerator->StartStatement(pnode);

    // Start call, allocate out param space
    funcInfo->StartRecordingOutArgs(argCount);

    // Assign the call target operand(s), putting them into expression temps if necessary to protect
    // them from side-effects.
    if (fSideEffectArgs)
    {TRACE_IT(39607);
        SaveOpndValue(pnode->sxCall.pnodeTarget, funcInfo);
    }

    if (pnode->sxCall.pnodeTarget->nop == knopSuper)
    {
        EmitSuperFieldPatch(funcInfo, pnode, byteCodeGenerator);
    }

    Emit(pnode->sxCall.pnodeTarget, byteCodeGenerator, funcInfo, false, true);

    if (pnode->sxCall.pnodeArgs == nullptr)
    {TRACE_IT(39608);
        funcInfo->ReleaseLoc(pnode->sxCall.pnodeTarget);
        Js::OpCode op = (CreateNativeArrays(byteCodeGenerator, funcInfo)
            && CallTargetIsArray(pnode->sxCall.pnodeTarget))
            ? Js::OpCode::NewScObjArray : Js::OpCode::NewScObject;
        Assert(argCount == 1);

        Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(op);
        byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, argCount);
        byteCodeGenerator->Writer()->CallI(op, funcInfo->AcquireLoc(pnode),
            pnode->sxCall.pnodeTarget->location, argCount, callSiteId);
    }
    else
    {TRACE_IT(39609);
        byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, argCount);
        uint32 actualArgCount = 0;

        if (IsCallOfConstants(pnode))
        {TRACE_IT(39610);
            funcInfo->ReleaseLoc(pnode->sxCall.pnodeTarget);
            actualArgCount = EmitNewObjectOfConstants(pnode, byteCodeGenerator, funcInfo, argCount);
        }
        else
        {TRACE_IT(39611);
            Js::OpCode op;
            if ((CreateNativeArrays(byteCodeGenerator, funcInfo) && CallTargetIsArray(pnode->sxCall.pnodeTarget)))
            {TRACE_IT(39612);
                op = pnode->sxCall.spreadArgCount > 0 ? Js::OpCode::NewScObjArraySpread : Js::OpCode::NewScObjArray;
            }
            else
            {TRACE_IT(39613);
                op = pnode->sxCall.spreadArgCount > 0 ? Js::OpCode::NewScObjectSpread : Js::OpCode::NewScObject;
            }

            Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(op);


            Js::AuxArray<uint32> *spreadIndices = nullptr;
            actualArgCount = EmitArgList(pnode->sxCall.pnodeArgs, Js::Constants::NoRegister, Js::Constants::NoRegister, Js::Constants::NoRegister,
                false, true, byteCodeGenerator, funcInfo, callSiteId, pnode->sxCall.spreadArgCount, &spreadIndices);
            funcInfo->ReleaseLoc(pnode->sxCall.pnodeTarget);


            if (pnode->sxCall.spreadArgCount > 0)
            {TRACE_IT(39614);
                Assert(spreadIndices != nullptr);
                uint spreadExtraAlloc = spreadIndices->count * sizeof(uint32);
                uint spreadIndicesSize = sizeof(*spreadIndices) + spreadExtraAlloc;
                byteCodeGenerator->Writer()->CallIExtended(op, funcInfo->AcquireLoc(pnode), pnode->sxCall.pnodeTarget->location,
                    (uint16)actualArgCount, Js::CallIExtended_SpreadArgs,
                    spreadIndices, spreadIndicesSize, callSiteId);
            }
            else
            {TRACE_IT(39615);
                byteCodeGenerator->Writer()->CallI(op, funcInfo->AcquireLoc(pnode), pnode->sxCall.pnodeTarget->location,
                    (uint16)actualArgCount, callSiteId);
            }
        }

        Assert(argCount == actualArgCount);
    }

    // End call, pop param space
    funcInfo->EndRecordingOutArgs(argCount);
    return;
}

void EmitCall(
    ParseNode* pnode,
    Js::RegSlot rhsLocation,
    ByteCodeGenerator* byteCodeGenerator,
    FuncInfo* funcInfo,
    BOOL fReturnValue,
    BOOL fEvaluateComponents,
    BOOL fHasNewTarget,
    Js::RegSlot overrideThisLocation)
{TRACE_IT(39616);
    BOOL fIsPut = (rhsLocation != Js::Constants::NoRegister);
    // If the call returns a float, we'll note this in the byte code.
    Js::RegSlot thisLocation = Js::Constants::NoRegister;
    Js::RegSlot callObjLocation = Js::Constants::NoRegister;
    Js::RegSlot newTargetLocation = Js::Constants::NoRegister;
    BOOL fSideEffectArgs = FALSE;
    ParseNode *pnodeTarget = pnode->sxCall.pnodeTarget;
    ParseNode *pnodeArgs = pnode->sxCall.pnodeArgs;
    uint16 spreadArgCount = pnode->sxCall.spreadArgCount;

    if (CreateNativeArrays(byteCodeGenerator, funcInfo) && CallTargetIsArray(pnode->sxCall.pnodeTarget)) {TRACE_IT(39617);
        // some minifiers (potentially incorrectly) assume that "v = new Array()" and "v = Array()" are equivalent,
        // and replace the former with the latter to save 4 characters. What that means for us is that it, at least
        // initially, uses the "Call" path. We want to guess that it _is_ just "new Array()" and change over to the
        // "new" path, since then our native array handling can kick in.
        /*EmitNew(pnode, byteCodeGenerator, funcInfo);
        return;*/
    }

    unsigned int argCount = CountArguments(pnode->sxCall.pnodeArgs, &fSideEffectArgs) + (unsigned int)fIsPut;

    BOOL fIsEval = !fIsPut && pnode->sxCall.isEvalCall;

    if (fIsEval)
    {TRACE_IT(39618);
        Assert(!fHasNewTarget);

        //
        // "eval" takes the closure environment as an extra argument
        // Pass the closure env only if some argument is passed
        // For just eval(), don't pass the closure environment
        //
        if (argCount > 1)
        {TRACE_IT(39619);
            // Check the module ID as well. If it's not the global (default) module,
            // we need to pass the root to eval so it can do the right global lookups.
            // (Passing the module root is the least disruptive way to get the module ID
            // to the helper, given the current set of byte codes. Once we have a full set
            // of byte code ops taking immediate opnds, passing the ID is more intuitive.)
            Js::ModuleID moduleID = byteCodeGenerator->GetModuleID();
            if (moduleID == kmodGlobal)
            {TRACE_IT(39620);
                argCount++;
            }
            else
            {TRACE_IT(39621);
                // Module ID must be passed
                argCount += 2;
            }
        }
    }

    if (fHasNewTarget)
    {TRACE_IT(39622);
        Assert(!fIsEval);

        // When we need to pass new.target explicitly, it is passed as an extra argument.
        // This is similar to how eval passes an extra argument for the frame display and is
        // used to support cases where we need to pass both 'this' and new.target as part of
        // a function call.
        // OpCode::LdNewTarget knows how to look at the call flags and fetch this argument.
        argCount++;
        newTargetLocation = funcInfo->newTargetRegister;

        Assert(newTargetLocation != Js::Constants::NoRegister);
    }

    Js::ArgSlot argSlotCount = (Js::ArgSlot)argCount;

    if (argCount != (unsigned int)argSlotCount)
    {TRACE_IT(39623);
        Js::Throw::OutOfMemory();
    }

    if (fReturnValue)
    {TRACE_IT(39624);
        pnode->isUsed = true;
    }

    //
    // Set up the call.
    //

    if (!fEvaluateComponents)
    {
        EmitCallTargetNoEvalComponents(pnodeTarget, fSideEffectArgs, &thisLocation, &callObjLocation, byteCodeGenerator, funcInfo);
    }
    else
    {
        EmitCallTarget(pnodeTarget, fSideEffectArgs, &thisLocation, &callObjLocation, byteCodeGenerator, funcInfo);
    }

    bool releaseThisLocation = true;
    // If we are strictly overriding the this location, ignore what the call target set this location to.
    if (overrideThisLocation != Js::Constants::NoRegister)
    {TRACE_IT(39625);
        thisLocation = overrideThisLocation;
        releaseThisLocation = false;
    }

    // Evaluate the arguments (nothing mode-specific here).
    // Start call, allocate out param space
    funcInfo->StartRecordingOutArgs(argSlotCount);

    Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(Js::OpCode::CallI);

    byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, argSlotCount);
    Js::AuxArray<uint32> *spreadIndices;
    Js::ArgSlot actualArgCount = EmitArgList(pnodeArgs, rhsLocation, thisLocation, newTargetLocation, fIsEval, fEvaluateComponents, byteCodeGenerator, funcInfo, callSiteId, spreadArgCount, &spreadIndices);
    Assert(argSlotCount == actualArgCount);

    if (!fEvaluateComponents)
    {
        EmitCallInstrNoEvalComponents(pnode, fIsPut, fIsEval, thisLocation, callObjLocation, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
    }
    else
    {
        EmitCallInstr(pnode, fIsPut, fIsEval, fHasNewTarget, releaseThisLocation ? thisLocation : Js::Constants::NoRegister, callObjLocation, actualArgCount, byteCodeGenerator, funcInfo, callSiteId, spreadIndices);
    }

    // End call, pop param space
    funcInfo->EndRecordingOutArgs(argSlotCount);
}

void EmitInvoke(
    Js::RegSlot location,
    Js::RegSlot callObjLocation,
    Js::PropertyId propertyId,
    ByteCodeGenerator* byteCodeGenerator,
    FuncInfo* funcInfo)
{
    EmitMethodFld(false, false, location, callObjLocation, propertyId, byteCodeGenerator, funcInfo);

    funcInfo->StartRecordingOutArgs(1);

    Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(Js::OpCode::CallI);

    byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, 1);
    EmitArgListStart(callObjLocation, byteCodeGenerator, funcInfo, callSiteId);

    byteCodeGenerator->Writer()->CallI(Js::OpCode::CallI, location, location, 1, callSiteId);
}

void EmitInvoke(
    Js::RegSlot location,
    Js::RegSlot callObjLocation,
    Js::PropertyId propertyId,
    ByteCodeGenerator* byteCodeGenerator,
    FuncInfo* funcInfo,
    Js::RegSlot arg1Location)
{
    EmitMethodFld(false, false, location, callObjLocation, propertyId, byteCodeGenerator, funcInfo);

    funcInfo->StartRecordingOutArgs(2);

    Js::ProfileId callSiteId = byteCodeGenerator->GetNextCallSiteId(Js::OpCode::CallI);

    byteCodeGenerator->Writer()->StartCall(Js::OpCode::StartCall, 2);
    EmitArgListStart(callObjLocation, byteCodeGenerator, funcInfo, callSiteId);
    byteCodeGenerator->Writer()->ArgOut<true>(1, arg1Location, callSiteId);

    byteCodeGenerator->Writer()->CallI(Js::OpCode::CallI, location, location, 2, callSiteId);
}

void EmitComputedFunctionNameVar(ParseNode *nameNode, ParseNode *exprNode, ByteCodeGenerator *byteCodeGenerator)
{TRACE_IT(39626);
    AssertMsg(exprNode != nullptr, "callers of this function should pass in a valid expression Node");

    if (nameNode == nullptr)
    {TRACE_IT(39627);
        return;
    }

    if ((exprNode->nop == knopFncDecl && (exprNode->sxFnc.pnodeName == nullptr || exprNode->sxFnc.pnodeName->nop != knopVarDecl)))
    {TRACE_IT(39628);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::SetComputedNameVar, exprNode->location, nameNode->location);
    }
}

void EmitMemberNode(ParseNode *memberNode, Js::RegSlot objectLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, ParseNode* parentNode, bool useStore, bool* isObjectEmpty = nullptr)
{TRACE_IT(39629);
    ParseNode *nameNode = memberNode->sxBin.pnode1;
    ParseNode *exprNode = memberNode->sxBin.pnode2;

    bool isFncDecl = exprNode->nop == knopFncDecl;
    bool isClassMember = isFncDecl && exprNode->sxFnc.IsClassMember();

    // Moved SetComputedNameVar before LdFld of prototype because loading the prototype undefers the function TypeHandler
    // which makes this bytecode too late to influence the function.name.
    if (nameNode->nop == knopComputedName)
    {TRACE_IT(39630);
        // Computed property name
        // Transparently pass the name expr
        // The Emit will replace this with a temp register if necessary to preserve the value.
        nameNode->location = nameNode->sxUni.pnode1->location;
        EmitBinaryOpnds(nameNode, exprNode, byteCodeGenerator, funcInfo);
        if (isFncDecl && !exprNode->sxFnc.IsClassConstructor())
        {
            EmitComputedFunctionNameVar(nameNode, exprNode, byteCodeGenerator);
        }
    }

    // Classes allocates a RegSlot as part of Instance Methods EmitClassInitializers,
    // but if we don't have any members then we don't need to load the prototype.
    Assert(isClassMember == (isObjectEmpty != nullptr));
    if (isClassMember && *isObjectEmpty)
    {TRACE_IT(39631);
        *isObjectEmpty = false;
        int cacheId = funcInfo->FindOrAddInlineCacheId(parentNode->location, Js::PropertyIds::prototype, false, false);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, objectLocation, parentNode->location, cacheId);
    }

    if (nameNode->nop == knopComputedName)
    {TRACE_IT(39632);
        Assert(memberNode->nop == knopGetMember || memberNode->nop == knopSetMember || memberNode->nop == knopMember);

        Js::OpCode setOp = memberNode->nop == knopGetMember ?
            (isClassMember ? Js::OpCode::InitClassMemberGetComputedName : Js::OpCode::InitGetElemI) :
            memberNode->nop == knopSetMember ?
            (isClassMember ? Js::OpCode::InitClassMemberSetComputedName : Js::OpCode::InitSetElemI) :
            (isClassMember ? Js::OpCode::InitClassMemberComputedName : Js::OpCode::InitComputedProperty);

        byteCodeGenerator->Writer()->Element(setOp, exprNode->location, objectLocation, nameNode->location, true);

        // Class and object members need a reference back to the class.
        if (isFncDecl)
        {TRACE_IT(39633);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::SetHomeObj, exprNode->location, objectLocation);
        }

        funcInfo->ReleaseLoc(exprNode);
        funcInfo->ReleaseLoc(nameNode);

        return;
    }

    Js::OpCode stFldOpCode = (Js::OpCode)0;
    if (useStore)
    {TRACE_IT(39634);
        stFldOpCode = ByteCodeGenerator::GetStFldOpCode(funcInfo, false, false, false, isClassMember);
    }

    Emit(exprNode, byteCodeGenerator, funcInfo, false);
    Js::PropertyId propertyId = nameNode->sxPid.PropertyIdFromNameNode();

    if (Js::PropertyIds::name == propertyId
        && exprNode->nop == knopFncDecl
        && exprNode->sxFnc.IsStaticMember()
        && parentNode != nullptr && parentNode->nop == knopClassDecl
        && parentNode->sxClass.pnodeConstructor != nullptr)
    {TRACE_IT(39635);
        Js::ParseableFunctionInfo* nameFunc = parentNode->sxClass.pnodeConstructor->sxFnc.funcInfo->byteCodeFunction->GetParseableFunctionInfo();
        nameFunc->SetIsStaticNameFunction(true);
    }

    if (memberNode->nop == knopMember || memberNode->nop == knopMemberShort)
    {TRACE_IT(39636);
        // The internal prototype should be set only if the production is of the form PropertyDefinition : PropertyName : AssignmentExpression
        if (propertyId == Js::PropertyIds::__proto__ && memberNode->nop != knopMemberShort && (exprNode->nop != knopFncDecl || !exprNode->sxFnc.IsMethod()))
        {TRACE_IT(39637);
            byteCodeGenerator->Writer()->Property(Js::OpCode::InitProto, exprNode->location, objectLocation,
                funcInfo->FindOrAddReferencedPropertyId(propertyId));
        }
        else
        {TRACE_IT(39638);
            uint cacheId = funcInfo->FindOrAddInlineCacheId(objectLocation, propertyId, false, true);
            Js::OpCode patchablePropertyOpCode;

            if (useStore)
            {TRACE_IT(39639);
                patchablePropertyOpCode = stFldOpCode;
            }
            else if (isClassMember)
            {TRACE_IT(39640);
                patchablePropertyOpCode = Js::OpCode::InitClassMember;
            }
            else
            {TRACE_IT(39641);
                patchablePropertyOpCode = Js::OpCode::InitFld;
            }

            byteCodeGenerator->Writer()->PatchableProperty(patchablePropertyOpCode, exprNode->location, objectLocation, cacheId);
        }
    }
    else
    {TRACE_IT(39642);
        Assert(memberNode->nop == knopGetMember || memberNode->nop == knopSetMember);

        Js::OpCode setOp = memberNode->nop == knopGetMember ?
            (isClassMember ? Js::OpCode::InitClassMemberGet : Js::OpCode::InitGetFld) :
            (isClassMember ? Js::OpCode::InitClassMemberSet : Js::OpCode::InitSetFld);

        byteCodeGenerator->Writer()->Property(setOp, exprNode->location, objectLocation, funcInfo->FindOrAddReferencedPropertyId(propertyId));
    }

    // Class and object members need a reference back to the class.
    if (isFncDecl)
    {TRACE_IT(39643);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::SetHomeObj, exprNode->location, objectLocation);
    }

    funcInfo->ReleaseLoc(exprNode);

    if (propertyId == Js::PropertyIds::valueOf)
    {TRACE_IT(39644);
        byteCodeGenerator->GetScriptContext()->optimizationOverrides.SetSideEffects(Js::SideEffects_ValueOf);
    }
    else if (propertyId == Js::PropertyIds::toString)
    {TRACE_IT(39645);
        byteCodeGenerator->GetScriptContext()->optimizationOverrides.SetSideEffects(Js::SideEffects_ToString);
    }
}

void EmitClassInitializers(ParseNode *memberList, Js::RegSlot objectLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, ParseNode* parentNode, bool isObjectEmpty)
{TRACE_IT(39646);
    if (memberList != nullptr)
    {TRACE_IT(39647);
        while (memberList->nop == knopList)
        {TRACE_IT(39648);
            ParseNode *memberNode = memberList->sxBin.pnode1;
            EmitMemberNode(memberNode, objectLocation, byteCodeGenerator, funcInfo, parentNode, /*useStore*/ false, &isObjectEmpty);
            memberList = memberList->sxBin.pnode2;
        }
        EmitMemberNode(memberList, objectLocation, byteCodeGenerator, funcInfo, parentNode, /*useStore*/ false, &isObjectEmpty);
    }
}

void EmitObjectInitializers(ParseNode *memberList, Js::RegSlot objectLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39649);
    ParseNode *pmemberList = memberList;
    unsigned int argCount = 0;
    uint32  value;
    Js::PropertyId propertyId;

    //
    // 1. Add all non-int property ids to a dictionary propertyIds with value true
    // 2. Get the count of propertyIds
    // 3. Create a propertyId array of size count
    // 4. Put the propIds in the auxiliary area
    // 5. Get the objectLiteralCacheId
    // 6. Generate propId inits with values
    //

    // Handle propertyId collision
    typedef JsUtil::BaseHashSet<Js::PropertyId, ArenaAllocator, PowerOf2SizePolicy> PropertyIdSet;
    PropertyIdSet* propertyIds = Anew(byteCodeGenerator->GetAllocator(), PropertyIdSet, byteCodeGenerator->GetAllocator(), 17);

    bool hasComputedName = false;
    if (memberList != nullptr)
    {TRACE_IT(39650);
        while (memberList->nop == knopList)
        {TRACE_IT(39651);
            if (memberList->sxBin.pnode1->sxBin.pnode1->nop == knopComputedName)
            {TRACE_IT(39652);
                hasComputedName = true;
                break;
            }

            propertyId = memberList->sxBin.pnode1->sxBin.pnode1->sxPid.PropertyIdFromNameNode();
            if (!byteCodeGenerator->GetScriptContext()->IsNumericPropertyId(propertyId, &value))
            {TRACE_IT(39653);
                propertyIds->Item(propertyId);
            }

            memberList = memberList->sxBin.pnode2;
        }

        if (memberList->sxBin.pnode1->nop != knopComputedName && !hasComputedName)
        {TRACE_IT(39654);
            propertyId = memberList->sxBin.pnode1->sxPid.PropertyIdFromNameNode();
            if (!byteCodeGenerator->GetScriptContext()->IsNumericPropertyId(propertyId, &value))
            {TRACE_IT(39655);
                propertyIds->Item(propertyId);
            }
        }
    }

    argCount = propertyIds->Count();

    memberList = pmemberList;
    if ((memberList == nullptr) || (argCount == 0))
    {TRACE_IT(39656);
        // Empty literal or numeric property only object literal
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::NewScObjectSimple, objectLocation);
    }
    else
    {TRACE_IT(39657);
        Js::PropertyIdArray *propIds = AnewPlus(byteCodeGenerator->GetAllocator(), argCount * sizeof(Js::PropertyId), Js::PropertyIdArray, argCount, 0);

        if (propertyIds->ContainsKey(Js::PropertyIds::__proto__))
        {TRACE_IT(39658);
            // Always record whether the initializer contains __proto__ no matter if current environment has it enabled
            // or not, in case the bytecode is later run with __proto__ enabled.
            propIds->has__proto__ = true;
        }

        unsigned int argIndex = 0;
        while (memberList->nop == knopList)
        {TRACE_IT(39659);
            if (memberList->sxBin.pnode1->sxBin.pnode1->nop == knopComputedName)
            {TRACE_IT(39660);
                break;
            }
            propertyId = memberList->sxBin.pnode1->sxBin.pnode1->sxPid.PropertyIdFromNameNode();
            if (!byteCodeGenerator->GetScriptContext()->IsNumericPropertyId(propertyId, &value) && propertyIds->Remove(propertyId))
            {TRACE_IT(39661);
                propIds->elements[argIndex] = propertyId;
                argIndex++;
            }
            memberList = memberList->sxBin.pnode2;
        }

        if (memberList->sxBin.pnode1->nop != knopComputedName && !hasComputedName)
        {TRACE_IT(39662);
            propertyId = memberList->sxBin.pnode1->sxPid.PropertyIdFromNameNode();
            if (!byteCodeGenerator->GetScriptContext()->IsNumericPropertyId(propertyId, &value) && propertyIds->Remove(propertyId))
            {TRACE_IT(39663);
                propIds->elements[argIndex] = propertyId;
                argIndex++;
            }
        }

        uint32 literalObjectId = funcInfo->GetParsedFunctionBody()->NewObjectLiteral();

        // Generate the opcode with propIds and cacheId
        byteCodeGenerator->Writer()->Auxiliary(Js::OpCode::NewScObjectLiteral, objectLocation, propIds, sizeof(Js::PropertyIdArray) + argCount * sizeof(Js::PropertyId), literalObjectId);

        Adelete(byteCodeGenerator->GetAllocator(), propertyIds);

        AdeletePlus(byteCodeGenerator->GetAllocator(), argCount * sizeof(Js::PropertyId), propIds);
    }

    memberList = pmemberList;

    bool useStore = false;
    // Generate the actual assignment to those properties
    if (memberList != nullptr)
    {TRACE_IT(39664);
        while (memberList->nop == knopList)
        {TRACE_IT(39665);
            ParseNode *memberNode = memberList->sxBin.pnode1;

            if (memberNode->sxBin.pnode1->nop == knopComputedName)
            {TRACE_IT(39666);
                useStore = true;
            }

            byteCodeGenerator->StartSubexpression(memberNode);
            EmitMemberNode(memberNode, objectLocation, byteCodeGenerator, funcInfo, nullptr, useStore);
            byteCodeGenerator->EndSubexpression(memberNode);
            memberList = memberList->sxBin.pnode2;
        }

        byteCodeGenerator->StartSubexpression(memberList);
        EmitMemberNode(memberList, objectLocation, byteCodeGenerator, funcInfo, nullptr, useStore);
        byteCodeGenerator->EndSubexpression(memberList);
    }
}

void EmitStringTemplate(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39667);
    Assert(pnode->sxStrTemplate.pnodeStringLiterals);

    // For a tagged string template, we will create the callsite constant object as part of the FunctionBody constants table.
    // We only need to emit code for non-tagged string templates here.
    if (!pnode->sxStrTemplate.isTaggedTemplate)
    {TRACE_IT(39668);
        // If we have no substitutions and this is not a tagged template, we can emit just the single cooked string.
        if (pnode->sxStrTemplate.pnodeSubstitutionExpressions == nullptr)
        {TRACE_IT(39669);
            Assert(pnode->sxStrTemplate.pnodeStringLiterals->nop != knopList);

            funcInfo->AcquireLoc(pnode);
            Emit(pnode->sxStrTemplate.pnodeStringLiterals, byteCodeGenerator, funcInfo, false);

            Assert(pnode->location != pnode->sxStrTemplate.pnodeStringLiterals->location);

            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxStrTemplate.pnodeStringLiterals->location);
            funcInfo->ReleaseLoc(pnode->sxStrTemplate.pnodeStringLiterals);
        }
        else
        {TRACE_IT(39670);
            // If we have substitutions but no tag function, we can skip the callSite object construction (and also ignore raw strings).
            funcInfo->AcquireLoc(pnode);

            // First string must be a list node since we have substitutions.
            AssertMsg(pnode->sxStrTemplate.pnodeStringLiterals->nop == knopList, "First string in the list must be a knopList node.");

            ParseNode* stringNodeList = pnode->sxStrTemplate.pnodeStringLiterals;

            // Emit the first string and load that into the pnode location.
            Emit(stringNodeList->sxBin.pnode1, byteCodeGenerator, funcInfo, false);

            Assert(pnode->location != stringNodeList->sxBin.pnode1->location);

            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, stringNodeList->sxBin.pnode1->location);
            funcInfo->ReleaseLoc(stringNodeList->sxBin.pnode1);

            ParseNode* expressionNodeList = pnode->sxStrTemplate.pnodeSubstitutionExpressions;
            ParseNode* stringNode;
            ParseNode* expressionNode;

            // Now append the substitution expressions and remaining string constants via normal add operator
            // We will always have one more string constant than substitution expression
            // `strcon1 ${expr1} strcon2 ${expr2} strcon3` = strcon1 + expr1 + strcon2 + expr2 + strcon3
            //
            // strcon1 --- step 1 (above)
            // expr1   \__ step 2
            // strcon2 /
            // expr2   \__ step 3
            // strcon3 /
            while (stringNodeList->nop == knopList)
            {TRACE_IT(39671);
                // If the current head of the expression list is a list, fetch the node and walk the list.
                if (expressionNodeList->nop == knopList)
                {TRACE_IT(39672);
                    expressionNode = expressionNodeList->sxBin.pnode1;
                    expressionNodeList = expressionNodeList->sxBin.pnode2;
                }
                else
                {TRACE_IT(39673);
                    // This is the last element of the expression list.
                    expressionNode = expressionNodeList;
                }

                // Emit the expression and append it to the string we're building.
                Emit(expressionNode, byteCodeGenerator, funcInfo, false);

                Js::RegSlot toStringLocation = funcInfo->AcquireTmpRegister();
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Conv_Str, toStringLocation, expressionNode->location);
                byteCodeGenerator->Writer()->Reg3(Js::OpCode::Add_A, pnode->location, pnode->location, toStringLocation);
                funcInfo->ReleaseTmpRegister(toStringLocation);
                funcInfo->ReleaseLoc(expressionNode);

                // Move to the next string in the list - we already got ahead of the expressions in the first string literal above.
                stringNodeList = stringNodeList->sxBin.pnode2;

                // If the current head of the string literal list is also a list node, need to fetch the actual string literal node.
                if (stringNodeList->nop == knopList)
                {TRACE_IT(39674);
                    stringNode = stringNodeList->sxBin.pnode1;
                }
                else
                {TRACE_IT(39675);
                    // This is the last element of the string literal list.
                    stringNode = stringNodeList;
                }

                // Emit the string node following the previous expression and append it to the string.
                // This is either just some string in the list or it is the last string.
                Emit(stringNode, byteCodeGenerator, funcInfo, false);
                byteCodeGenerator->Writer()->Reg3(Js::OpCode::Add_A, pnode->location, pnode->location, stringNode->location);
                funcInfo->ReleaseLoc(stringNode);
            }
        }
    }
}

void SetNewArrayElements(ParseNode *pnode, Js::RegSlot arrayLocation, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39676);
    ParseNode *args = pnode->sxUni.pnode1;
    uint argCount = pnode->sxArrLit.count;
    uint spreadCount = pnode->sxArrLit.spreadCount;
    bool nativeArrays = CreateNativeArrays(byteCodeGenerator, funcInfo);

    bool arrayIntOpt = nativeArrays && pnode->sxArrLit.arrayOfInts;
    if (arrayIntOpt)
    {TRACE_IT(39677);
        int extraAlloc = argCount * sizeof(int32);
        Js::AuxArray<int> *ints = AnewPlus(byteCodeGenerator->GetAllocator(), extraAlloc, Js::AuxArray<int32>, argCount);
        EmitConstantArgsToIntArray(byteCodeGenerator, ints->elements, args, argCount);
        Assert(!pnode->sxArrLit.hasMissingValues);
        byteCodeGenerator->Writer()->Auxiliary(
            Js::OpCode::NewScIntArray,
            pnode->location,
            ints,
            sizeof(Js::AuxArray<int>) + extraAlloc,
            argCount);
        AdeletePlus(byteCodeGenerator->GetAllocator(), extraAlloc, ints);
        return;
    }

    bool arrayNumOpt = nativeArrays && pnode->sxArrLit.arrayOfNumbers;
    if (arrayNumOpt)
    {TRACE_IT(39678);
        int extraAlloc = argCount * sizeof(double);
        Js::AuxArray<double> *doubles = AnewPlus(byteCodeGenerator->GetAllocator(), extraAlloc, Js::AuxArray<double>, argCount);
        EmitConstantArgsToFltArray(byteCodeGenerator, doubles->elements, args, argCount);
        Assert(!pnode->sxArrLit.hasMissingValues);
        byteCodeGenerator->Writer()->Auxiliary(
            Js::OpCode::NewScFltArray,
            pnode->location,
            doubles,
            sizeof(Js::AuxArray<double>) + extraAlloc,
            argCount);
        AdeletePlus(byteCodeGenerator->GetAllocator(), extraAlloc, doubles);
        return;
    }

    bool arrayLitOpt = pnode->sxArrLit.arrayOfTaggedInts && pnode->sxArrLit.count > 1;
    Assert(!arrayLitOpt || !nativeArrays);

    Js::RegSlot spreadArrLoc = arrayLocation;
    Js::AuxArray<uint32> *spreadIndices = nullptr;
    const uint extraAlloc = spreadCount * sizeof(uint32);
    if (pnode->sxArrLit.spreadCount > 0)
    {TRACE_IT(39679);
        arrayLocation = funcInfo->AcquireTmpRegister();
        spreadIndices = AnewPlus(byteCodeGenerator->GetAllocator(), extraAlloc, Js::AuxArray<uint32>, spreadCount);
    }

    byteCodeGenerator->Writer()->Reg1Unsigned1(
        pnode->sxArrLit.hasMissingValues ? Js::OpCode::NewScArrayWithMissingValues : Js::OpCode::NewScArray,
        arrayLocation,
        argCount);

    if (args != nullptr)
    {TRACE_IT(39680);
        Js::OpCode opcode;
        Js::RegSlot arrLoc;
        if (argCount == 1 && !byteCodeGenerator->Writer()->DoProfileNewScArrayOp(Js::OpCode::NewScArray))
        {TRACE_IT(39681);
            opcode = Js::OpCode::StArrItemC_CI4;
            arrLoc = arrayLocation;
        }
        else if (arrayLitOpt)
        {TRACE_IT(39682);
            opcode = Js::OpCode::StArrSegItem_A;
            arrLoc = funcInfo->AcquireTmpRegister();
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdArrHead, arrLoc, arrayLocation);
        }
        else if (Js::JavascriptArray::HasInlineHeadSegment(argCount))
        {TRACE_IT(39683);
            // The head segment will be allocated inline as an interior pointer. To keep the array alive, the set operation
            // should be done relative to the array header to keep it alive (instead of the array segment).
            opcode = Js::OpCode::StArrInlineItem_CI4;
            arrLoc = arrayLocation;
        }
        else if (argCount <= Js::JavascriptArray::MaxInitialDenseLength)
        {TRACE_IT(39684);
            opcode = Js::OpCode::StArrSegItem_CI4;
            arrLoc = funcInfo->AcquireTmpRegister();
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdArrHead, arrLoc, arrayLocation);
        }
        else
        {TRACE_IT(39685);
            opcode = Js::OpCode::StArrItemI_CI4;
            arrLoc = arrayLocation;
        }

        if (arrayLitOpt)
        {TRACE_IT(39686);
            Js::VarArray *vars = AnewPlus(byteCodeGenerator->GetAllocator(), argCount * sizeof(Js::Var), Js::VarArray, argCount);

            EmitConstantArgsToVarArray(byteCodeGenerator, vars->elements, args, argCount);

            // Generate the opcode with vars
            byteCodeGenerator->Writer()->Auxiliary(Js::OpCode::StArrSegItem_A, arrLoc, vars, sizeof(Js::VarArray) + argCount * sizeof(Js::Var), argCount);

            AdeletePlus(byteCodeGenerator->GetAllocator(), argCount * sizeof(Js::Var), vars);
        }
        else
        {TRACE_IT(39687);
            uint i = 0;
            unsigned spreadIndex = 0;
            Js::RegSlot rhsLocation;
            while (args->nop == knopList)
            {TRACE_IT(39688);
                if (args->sxBin.pnode1->nop != knopEmpty)
                {TRACE_IT(39689);
                    Emit(args->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
                    rhsLocation = args->sxBin.pnode1->location;
                    Js::RegSlot regVal = rhsLocation;
                    if (args->sxBin.pnode1->nop == knopEllipsis)
                    {TRACE_IT(39690);
                        AnalysisAssert(spreadIndices);
                        regVal = funcInfo->AcquireTmpRegister();
                        byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdCustomSpreadIteratorList, regVal, rhsLocation);
                        spreadIndices->elements[spreadIndex++] = i;
                    }

                    byteCodeGenerator->Writer()->ElementUnsigned1(opcode, regVal, arrLoc, i);

                    if (args->sxBin.pnode1->nop == knopEllipsis)
                    {TRACE_IT(39691);
                        funcInfo->ReleaseTmpRegister(regVal);
                    }

                    funcInfo->ReleaseLoc(args->sxBin.pnode1);
                }

                args = args->sxBin.pnode2;
                i++;
            }

            if (args->nop != knopEmpty)
            {
                Emit(args, byteCodeGenerator, funcInfo, false);
                rhsLocation = args->location;
                Js::RegSlot regVal = rhsLocation;
                if (args->nop == knopEllipsis)
                {TRACE_IT(39692);
                    regVal = funcInfo->AcquireTmpRegister();
                    byteCodeGenerator->Writer()->Reg2(Js::OpCode::LdCustomSpreadIteratorList, regVal, rhsLocation);
                    AnalysisAssert(spreadIndices);
                    spreadIndices->elements[spreadIndex] = i;
                }

                byteCodeGenerator->Writer()->ElementUnsigned1(opcode, regVal, arrLoc, i);

                if (args->nop == knopEllipsis)
                {TRACE_IT(39693);
                    funcInfo->ReleaseTmpRegister(regVal);
                }

                funcInfo->ReleaseLoc(args);
                i++;
            }
            Assert(i <= argCount);
        }

        if (arrLoc != arrayLocation)
        {TRACE_IT(39694);
            funcInfo->ReleaseTmpRegister(arrLoc);
        }
    }

    if (pnode->sxArrLit.spreadCount > 0)
    {TRACE_IT(39695);
        byteCodeGenerator->Writer()->Reg2Aux(Js::OpCode::SpreadArrayLiteral, spreadArrLoc, arrayLocation, spreadIndices, sizeof(Js::AuxArray<uint32>) + extraAlloc, extraAlloc);
        AdeletePlus(byteCodeGenerator->GetAllocator(), extraAlloc, spreadIndices);
        funcInfo->ReleaseTmpRegister(arrayLocation);
    }
}

// FIX: TODO: mixed-mode expressions (arithmetic expressions mixed with boolean expressions); current solution
// will not short-circuit in some cases and is not complete (for example: var i=(x==y))
// This uses Aho and Ullman style double-branch generation (p. 494 ASU); we will need to peephole optimize or replace
// with special case for single-branch style.
void EmitBooleanExpression(ParseNode *expr, Js::ByteCodeLabel trueLabel, Js::ByteCodeLabel falseLabel, ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo)
{TRACE_IT(39696);
    byteCodeGenerator->StartStatement(expr);
    switch (expr->nop)
    {

    case knopLogOr:
    {TRACE_IT(39697);
        Js::ByteCodeLabel leftFalse = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(expr->sxBin.pnode1, trueLabel, leftFalse, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        byteCodeGenerator->Writer()->MarkLabel(leftFalse);
        EmitBooleanExpression(expr->sxBin.pnode2, trueLabel, falseLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        break;
    }

    case knopLogAnd:
    {TRACE_IT(39698);
        Js::ByteCodeLabel leftTrue = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(expr->sxBin.pnode1, leftTrue, falseLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        byteCodeGenerator->Writer()->MarkLabel(leftTrue);
        EmitBooleanExpression(expr->sxBin.pnode2, trueLabel, falseLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        break;
    }

    case knopLogNot:
        EmitBooleanExpression(expr->sxUni.pnode1, falseLabel, trueLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxUni.pnode1);
        break;

    case knopEq:
    case knopEqv:
    case knopNEqv:
    case knopNe:
    case knopLt:
    case knopLe:
    case knopGe:
    case knopGt:
        EmitBinaryOpnds(expr->sxBin.pnode1, expr->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        byteCodeGenerator->Writer()->BrReg2(nopToOp[expr->nop], trueLabel, expr->sxBin.pnode1->location,
            expr->sxBin.pnode2->location);
        byteCodeGenerator->Writer()->Br(falseLabel);
        break;
    case knopTrue:
        byteCodeGenerator->Writer()->Br(trueLabel);
        break;
    case knopFalse:
        byteCodeGenerator->Writer()->Br(falseLabel);
        break;
    default:
        // Note: we usually release the temp assigned to a node after we Emit it.
        // But in this case, EmitBooleanExpression is just a wrapper around a normal Emit call,
        // and the caller of EmitBooleanExpression expects to be able to release this register.

        Emit(expr, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
        byteCodeGenerator->Writer()->Br(falseLabel);
        break;
    }

    byteCodeGenerator->EndStatement(expr);
}

void EmitGeneratingBooleanExpression(ParseNode *expr, Js::ByteCodeLabel trueLabel, bool truefallthrough, Js::ByteCodeLabel falseLabel, bool falsefallthrough, Js::RegSlot writeto,
    ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39699);
    switch (expr->nop)
    {

    case knopLogOr:
    {TRACE_IT(39700);
        byteCodeGenerator->StartStatement(expr);
        Js::ByteCodeLabel leftFalse = byteCodeGenerator->Writer()->DefineLabel();
        EmitGeneratingBooleanExpression(expr->sxBin.pnode1, trueLabel, false, leftFalse, true, writeto, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        byteCodeGenerator->Writer()->MarkLabel(leftFalse);
        EmitGeneratingBooleanExpression(expr->sxBin.pnode2, trueLabel, truefallthrough, falseLabel, falsefallthrough, writeto, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        byteCodeGenerator->EndStatement(expr);
        break;
    }

    case knopLogAnd:
    {TRACE_IT(39701);
        byteCodeGenerator->StartStatement(expr);
        Js::ByteCodeLabel leftTrue = byteCodeGenerator->Writer()->DefineLabel();
        EmitGeneratingBooleanExpression(expr->sxBin.pnode1, leftTrue, true, falseLabel, false, writeto, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        byteCodeGenerator->Writer()->MarkLabel(leftTrue);
        EmitGeneratingBooleanExpression(expr->sxBin.pnode2, trueLabel, truefallthrough, falseLabel, falsefallthrough, writeto, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        byteCodeGenerator->EndStatement(expr);
        break;
    }

    case knopLogNot:
    {TRACE_IT(39702);
        byteCodeGenerator->StartStatement(expr);
        // this time we want a boolean expression, since Logical Not is nice and only returns true or false
        Js::ByteCodeLabel emitTrue = byteCodeGenerator->Writer()->DefineLabel();
        Js::ByteCodeLabel emitFalse = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(expr->sxUni.pnode1, emitFalse, emitTrue, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->MarkLabel(emitTrue);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, writeto);
        byteCodeGenerator->Writer()->Br(trueLabel);
        byteCodeGenerator->Writer()->MarkLabel(emitFalse);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, writeto);
        if (!falsefallthrough)
        {TRACE_IT(39703);
            byteCodeGenerator->Writer()->Br(falseLabel);
        }
        funcInfo->ReleaseLoc(expr->sxUni.pnode1);
        byteCodeGenerator->EndStatement(expr);
        break;
    }
    case knopEq:
    case knopEqv:
    case knopNEqv:
    case knopNe:
    case knopLt:
    case knopLe:
    case knopGe:
    case knopGt:
        byteCodeGenerator->StartStatement(expr);
        EmitBinaryOpnds(expr->sxBin.pnode1, expr->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(expr->sxBin.pnode2);
        funcInfo->ReleaseLoc(expr->sxBin.pnode1);
        funcInfo->AcquireLoc(expr);
        byteCodeGenerator->Writer()->Reg3(nopToCMOp[expr->nop], expr->location, expr->sxBin.pnode1->location,
            expr->sxBin.pnode2->location);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, writeto, expr->location);
        // The inliner likes small bytecode
        if (!(truefallthrough || falsefallthrough))
        {TRACE_IT(39704);
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
            byteCodeGenerator->Writer()->Br(falseLabel);
        }
        else if (truefallthrough && !falsefallthrough) {TRACE_IT(39705);
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrFalse_A, falseLabel, expr->location);
        }
        else if (falsefallthrough && !truefallthrough) {TRACE_IT(39706);
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
        }
        byteCodeGenerator->EndStatement(expr);
        break;
    case knopTrue:
        byteCodeGenerator->StartStatement(expr);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, writeto);
        if (!truefallthrough)
        {TRACE_IT(39707);
            byteCodeGenerator->Writer()->Br(trueLabel);
        }
        byteCodeGenerator->EndStatement(expr);
        break;
    case knopFalse:
        byteCodeGenerator->StartStatement(expr);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, writeto);
        if (!falsefallthrough)
        {TRACE_IT(39708);
            byteCodeGenerator->Writer()->Br(falseLabel);
        }
        byteCodeGenerator->EndStatement(expr);
        break;
    default:
        // Note: we usually release the temp assigned to a node after we Emit it.
        // But in this case, EmitBooleanExpression is just a wrapper around a normal Emit call,
        // and the caller of EmitBooleanExpression expects to be able to release this register.

        // For diagnostics purposes, register the name and dot to the statement list.
        if (expr->nop == knopName || expr->nop == knopDot)
        {TRACE_IT(39709);
            byteCodeGenerator->StartStatement(expr);
            Emit(expr, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, writeto, expr->location);
            // The inliner likes small bytecode
            if (!(truefallthrough || falsefallthrough))
            {TRACE_IT(39710);
                byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
                byteCodeGenerator->Writer()->Br(falseLabel);
            }
            else if (truefallthrough && !falsefallthrough) {TRACE_IT(39711);
                byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrFalse_A, falseLabel, expr->location);
            }
            else if (falsefallthrough && !truefallthrough) {TRACE_IT(39712);
                byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
            }
            byteCodeGenerator->EndStatement(expr);
        }
        else
        {
            Emit(expr, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, writeto, expr->location);
            // The inliner likes small bytecode
            if (!(truefallthrough || falsefallthrough))
            {TRACE_IT(39713);
                byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
                byteCodeGenerator->Writer()->Br(falseLabel);
            }
            else if (truefallthrough && !falsefallthrough) {TRACE_IT(39714);
                byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrFalse_A, falseLabel, expr->location);
            }
            else if (falsefallthrough && !truefallthrough) {TRACE_IT(39715);
                byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, trueLabel, expr->location);
            }
        }
        break;
    }
}

// used by while and for loops
void EmitLoop(
    ParseNode *loopNode,
    ParseNode *cond,
    ParseNode *body,
    ParseNode *incr,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    BOOL fReturnValue,
    BOOL doWhile = FALSE,
    ParseNode *forLoopBlock = nullptr)
{TRACE_IT(39716);
    // Need to increment loop count whether we are going to profile or not for HasLoop()

    Js::ByteCodeLabel loopEntrance = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel continuePastLoop = byteCodeGenerator->Writer()->DefineLabel();

    uint loopId = byteCodeGenerator->Writer()->EnterLoop(loopEntrance);
    loopNode->sxLoop.loopId = loopId;

    if (doWhile)
    {
        Emit(body, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(body);
        if (loopNode->emitLabels)
        {TRACE_IT(39717);
            byteCodeGenerator->Writer()->MarkLabel(loopNode->sxStmt.continueLabel);
        }
        if (!ByteCodeGenerator::IsFalse(cond) ||
            byteCodeGenerator->IsInDebugMode())
        {
            EmitBooleanExpression(cond, loopEntrance, continuePastLoop, byteCodeGenerator, funcInfo);
        }
        funcInfo->ReleaseLoc(cond);
    }
    else
    {TRACE_IT(39718);
        if (cond)
        {TRACE_IT(39719);
            if (!(cond->nop == knopInt &&
                cond->sxInt.lw != 0))
            {TRACE_IT(39720);
                Js::ByteCodeLabel trueLabel = byteCodeGenerator->Writer()->DefineLabel();
                EmitBooleanExpression(cond, trueLabel, continuePastLoop, byteCodeGenerator, funcInfo);
                byteCodeGenerator->Writer()->MarkLabel(trueLabel);
            }
            funcInfo->ReleaseLoc(cond);
        }
        Emit(body, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(body);

        if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled() &&
            forLoopBlock != nullptr)
        {
            CloneEmitBlock(forLoopBlock, byteCodeGenerator, funcInfo);
        }

        if (loopNode->emitLabels)
        {TRACE_IT(39721);
            byteCodeGenerator->Writer()->MarkLabel(loopNode->sxStmt.continueLabel);
        }

        if (incr != nullptr)
        {
            Emit(incr, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(incr);
        }

        byteCodeGenerator->Writer()->Br(loopEntrance);
    }

    byteCodeGenerator->Writer()->MarkLabel(continuePastLoop);
    if (loopNode->emitLabels)
    {TRACE_IT(39722);
        byteCodeGenerator->Writer()->MarkLabel(loopNode->sxStmt.breakLabel);
    }

    byteCodeGenerator->Writer()->ExitLoop(loopId);
}

void ByteCodeGenerator::EmitInvertedLoop(ParseNode* outerLoop, ParseNode* invertedLoop, FuncInfo* funcInfo)
{TRACE_IT(39723);
    Js::ByteCodeLabel invertedLoopLabel = this->m_writer.DefineLabel();
    Js::ByteCodeLabel afterInvertedLoop = this->m_writer.DefineLabel();

    // emit branch around original
    Emit(outerLoop->sxFor.pnodeInit, this, funcInfo, false);
    funcInfo->ReleaseLoc(outerLoop->sxFor.pnodeInit);
    this->m_writer.BrS(Js::OpCode::BrNotHasSideEffects, invertedLoopLabel, Js::SideEffects_Any);

    // emit original
    EmitLoop(outerLoop, outerLoop->sxFor.pnodeCond, outerLoop->sxFor.pnodeBody,
        outerLoop->sxFor.pnodeIncr, this, funcInfo, false);

    // clear temporary registers since inverted loop may share nodes with
    // emitted original loop
    VisitClearTmpRegs(outerLoop, this, funcInfo);

    // emit branch around inverted
    this->m_writer.Br(afterInvertedLoop);
    this->m_writer.MarkLabel(invertedLoopLabel);

    // Emit a zero trip test for the original outer-loop
    Js::ByteCodeLabel zeroTrip = this->m_writer.DefineLabel();
    ParseNode* testNode = this->GetParser()->CopyPnode(outerLoop->sxFor.pnodeCond);
    EmitBooleanExpression(testNode, zeroTrip, afterInvertedLoop, this, funcInfo);
    this->m_writer.MarkLabel(zeroTrip);
    funcInfo->ReleaseLoc(testNode);

    // emit inverted
    Emit(invertedLoop->sxFor.pnodeInit, this, funcInfo, false);
    funcInfo->ReleaseLoc(invertedLoop->sxFor.pnodeInit);
    EmitLoop(invertedLoop, invertedLoop->sxFor.pnodeCond, invertedLoop->sxFor.pnodeBody,
        invertedLoop->sxFor.pnodeIncr, this, funcInfo, false);
    this->m_writer.MarkLabel(afterInvertedLoop);
}

void EmitGetIterator(Js::RegSlot iteratorLocation, Js::RegSlot iterableLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{
    // get iterator object from the iterable
    EmitInvoke(iteratorLocation, iterableLocation, Js::PropertyIds::_symbolIterator, byteCodeGenerator, funcInfo);

    // throw TypeError if the result is not an object
    Js::ByteCodeLabel skipThrow = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrOnObject_A, skipThrow, iteratorLocation);
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_NeedObject));
    byteCodeGenerator->Writer()->MarkLabel(skipThrow);
}

void EmitIteratorNext(Js::RegSlot itemLocation, Js::RegSlot iteratorLocation, Js::RegSlot nextInputLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{TRACE_IT(39724);
    // invoke next() on the iterator
    if (nextInputLocation == Js::Constants::NoRegister)
    {
        EmitInvoke(itemLocation, iteratorLocation, Js::PropertyIds::next, byteCodeGenerator, funcInfo);
    }
    else
    {
        EmitInvoke(itemLocation, iteratorLocation, Js::PropertyIds::next, byteCodeGenerator, funcInfo, nextInputLocation);
    }

    // throw TypeError if the result is not an object
    Js::ByteCodeLabel skipThrow = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrOnObject_A, skipThrow, itemLocation);
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_NeedObject));
    byteCodeGenerator->Writer()->MarkLabel(skipThrow);
}

// Generating
// if (hasReturnFunction) {
//     value = Call Retrun;
//     if (value != Object)
//        throw TypeError;
// }

void EmitIteratorClose(Js::RegSlot iteratorLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{TRACE_IT(39725);
    Js::RegSlot returnLocation = funcInfo->AcquireTmpRegister();

    Js::ByteCodeLabel skipThrow = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel noReturn = byteCodeGenerator->Writer()->DefineLabel();

    uint cacheId = funcInfo->FindOrAddInlineCacheId(iteratorLocation, Js::PropertyIds::return_, false, false);
    byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, returnLocation, iteratorLocation, cacheId);

    byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrEq_A, noReturn, returnLocation, funcInfo->undefinedConstantRegister);

    EmitInvoke(returnLocation, iteratorLocation, Js::PropertyIds::return_, byteCodeGenerator, funcInfo);

    // throw TypeError if the result is not an Object
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrOnObject_A, skipThrow, returnLocation);
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_NeedObject));
    byteCodeGenerator->Writer()->MarkLabel(skipThrow);
    byteCodeGenerator->Writer()->MarkLabel(noReturn);

    funcInfo->ReleaseTmpRegister(returnLocation);
}

void EmitIteratorComplete(Js::RegSlot doneLocation, Js::RegSlot iteratorResultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{TRACE_IT(39726);
    // get the iterator result's "done" property
    uint cacheId = funcInfo->FindOrAddInlineCacheId(iteratorResultLocation, Js::PropertyIds::done, false, false);
    byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, doneLocation, iteratorResultLocation, cacheId);

    // Do not need to do ToBoolean explicitly with current uses of EmitIteratorComplete since BrTrue_A does this.
    // Add a ToBoolean controlled by template flag if needed for new uses later on.
}

void EmitIteratorValue(Js::RegSlot valueLocation, Js::RegSlot iteratorResultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{TRACE_IT(39727);
    // get the iterator result's "value" property
    uint cacheId = funcInfo->FindOrAddInlineCacheId(iteratorResultLocation, Js::PropertyIds::value, false, false);
    byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, valueLocation, iteratorResultLocation, cacheId);
}

void EmitForInOfLoopBody(ParseNode *loopNode,
    Js::ByteCodeLabel loopEntrance,
    Js::ByteCodeLabel continuePastLoop,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    BOOL fReturnValue)
{TRACE_IT(39728);
    if (loopNode->sxForInOrForOf.pnodeLval->nop != knopVarDecl &&
        loopNode->sxForInOrForOf.pnodeLval->nop != knopLetDecl &&
        loopNode->sxForInOrForOf.pnodeLval->nop != knopConstDecl)
    {TRACE_IT(39729);
        EmitReference(loopNode->sxForInOrForOf.pnodeLval, byteCodeGenerator, funcInfo);
    }
    else
    {TRACE_IT(39730);
        Symbol * sym = loopNode->sxForInOrForOf.pnodeLval->sxVar.sym;
        sym->SetNeedDeclaration(false);
    }

    if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {TRACE_IT(39731);
        BeginEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
    }

    EmitAssignment(nullptr, loopNode->sxForInOrForOf.pnodeLval, loopNode->sxForInOrForOf.itemLocation, byteCodeGenerator, funcInfo);

    // The StartStatement is already done in the caller of this function.
    byteCodeGenerator->EndStatement(loopNode->sxForInOrForOf.pnodeLval);

    funcInfo->ReleaseReference(loopNode->sxForInOrForOf.pnodeLval);

    Emit(loopNode->sxForInOrForOf.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
    funcInfo->ReleaseLoc(loopNode->sxForInOrForOf.pnodeBody);

    if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {TRACE_IT(39732);
        EndEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
    }

    funcInfo->ReleaseTmpRegister(loopNode->sxForInOrForOf.itemLocation);
    if (loopNode->emitLabels)
    {TRACE_IT(39733);
        byteCodeGenerator->Writer()->MarkLabel(loopNode->sxForInOrForOf.continueLabel);
    }
    byteCodeGenerator->Writer()->Br(loopEntrance);
    byteCodeGenerator->Writer()->MarkLabel(continuePastLoop);
    if (loopNode->emitLabels)
    {TRACE_IT(39734);
        byteCodeGenerator->Writer()->MarkLabel(loopNode->sxForInOrForOf.breakLabel);
    }
}

void EmitForIn(ParseNode *loopNode,
    Js::ByteCodeLabel loopEntrance,
    Js::ByteCodeLabel continuePastLoop,
    ByteCodeGenerator *byteCodeGenerator,
    FuncInfo *funcInfo,
    BOOL fReturnValue)
{TRACE_IT(39735);
    Assert(loopNode->nop == knopForIn);
    Assert(loopNode->location == Js::Constants::NoRegister);

    // Grab registers for the enumerator and for the current enumerated item.
    // The enumerator register will be released after this call returns.
    loopNode->sxForInOrForOf.itemLocation = funcInfo->AcquireTmpRegister();

    uint forInLoopLevel = funcInfo->AcquireForInLoopLevel();

    // get enumerator from the collection
    byteCodeGenerator->Writer()->Reg1Unsigned1(Js::OpCode::InitForInEnumerator, loopNode->sxForInOrForOf.pnodeObj->location, forInLoopLevel);

    // The StartStatement is already done in the caller of the current function, which is EmitForInOrForOf
    byteCodeGenerator->EndStatement(loopNode);

    // Need to increment loop count whether we are going into profile or not for HasLoop()
    uint loopId = byteCodeGenerator->Writer()->EnterLoop(loopEntrance);
    loopNode->sxForInOrForOf.loopId = loopId;

    // The EndStatement will happen in the EmitForInOfLoopBody function
    byteCodeGenerator->StartStatement(loopNode->sxForInOrForOf.pnodeLval);

    // branch past loop when MoveAndGetNext returns nullptr
    byteCodeGenerator->Writer()->BrReg1Unsigned1(Js::OpCode::BrOnEmpty, continuePastLoop, loopNode->sxForInOrForOf.itemLocation, forInLoopLevel);

    EmitForInOfLoopBody(loopNode, loopEntrance, continuePastLoop, byteCodeGenerator, funcInfo, fReturnValue);

    byteCodeGenerator->Writer()->ExitLoop(loopId);

    funcInfo->ReleaseForInLoopLevel(forInLoopLevel);

    if (!byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {TRACE_IT(39736);
        EndEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
    }
}

void EmitForInOrForOf(ParseNode *loopNode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fReturnValue)
{TRACE_IT(39737);
    bool isForIn = (loopNode->nop == knopForIn);
    Assert(isForIn || loopNode->nop == knopForOf);

    BeginEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);

    byteCodeGenerator->StartStatement(loopNode);
    if (!isForIn)
    {TRACE_IT(39738);
        funcInfo->AcquireLoc(loopNode);
    }

    // Record the branch bytecode offset.
    // This is used for "ignore exception" and "set next stmt" scenarios. See ProbeContainer::GetNextUserStatementOffsetForAdvance:
    // If there is a branch recorded between current offset and next stmt offset, we'll use offset of the branch recorded,
    // otherwise use offset of next stmt.
    // The idea here is that when we bail out after ignore exception, we need to bail out to the beginning of the ForIn,
    // but currently ForIn stmt starts at the condition part, which is needed for correct handling of break point on ForIn
    // (break every time on the loop back edge) and correct display of current statement under debugger.
    // See WinBlue 231880 for details.
    byteCodeGenerator->Writer()->RecordStatementAdjustment(Js::FunctionBody::SAT_All);
    if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled() &&
        loopNode->sxForInOrForOf.pnodeBlock->sxBlock.HasBlockScopedContent())
    {TRACE_IT(39739);
        byteCodeGenerator->Writer()->RecordForInOrOfCollectionScope();
    }
    Js::ByteCodeLabel loopEntrance = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel continuePastLoop = byteCodeGenerator->Writer()->DefineLabel();

    if (loopNode->sxForInOrForOf.pnodeLval->nop == knopVarDecl)
    {TRACE_IT(39740);
        EmitReference(loopNode->sxForInOrForOf.pnodeLval, byteCodeGenerator, funcInfo);
    }

    Emit(loopNode->sxForInOrForOf.pnodeObj, byteCodeGenerator, funcInfo, false); // evaluate collection expression
    funcInfo->ReleaseLoc(loopNode->sxForInOrForOf.pnodeObj);

    if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {TRACE_IT(39741);
        EndEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
        if (loopNode->sxForInOrForOf.pnodeBlock->sxBlock.scope != nullptr)
        {TRACE_IT(39742);
            loopNode->sxForInOrForOf.pnodeBlock->sxBlock.scope->ForEachSymbol([](Symbol *sym) {
                sym->SetIsTrackedForDebugger(false);
            });
        }
    }

    if (isForIn)
    {
        EmitForIn(loopNode, loopEntrance, continuePastLoop, byteCodeGenerator, funcInfo, fReturnValue);

        if (!byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
        {TRACE_IT(39743);
            EndEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
        }

        return;
    }

    Js::ByteCodeLabel skipThrow = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrNeq_A, skipThrow, loopNode->sxForInOrForOf.pnodeObj->location, funcInfo->undefinedConstantRegister);
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_ObjectCoercible));
    byteCodeGenerator->Writer()->MarkLabel(skipThrow);

    Js::RegSlot regException = Js::Constants::NoRegister;
    Js::RegSlot regOffset = Js::Constants::NoRegister;

    // These two temp variables store the information of return function to be called or not.
    // one variable is used for catch block and one is used for finally block. These variable will be set to true when we think that return function
    // to be called on abrupt loop break.
    // Why two variables? since these are temps and JIT does like not flow if single variable is used in multiple blocks.
    Js::RegSlot shouldCallReturnFunctionLocation = funcInfo->AcquireTmpRegister();
    Js::RegSlot shouldCallReturnFunctionLocationFinally = funcInfo->AcquireTmpRegister();

    bool isCoroutine = funcInfo->byteCodeFunction->IsCoroutine();

    if (isCoroutine)
    {TRACE_IT(39744);
        regException = funcInfo->AcquireTmpRegister();
        regOffset = funcInfo->AcquireTmpRegister();
    }

    // Grab registers for the enumerator and for the current enumerated item.
    // The enumerator register will be released after this call returns.
    loopNode->sxForInOrForOf.itemLocation = funcInfo->AcquireTmpRegister();

    // We want call profile information on the @@iterator call, so instead of adding a GetForOfIterator bytecode op
    // to do all the following work in a helper do it explicitly in bytecode so that the @@iterator call is exposed
    // to the profiler and JIT.

    byteCodeGenerator->SetHasFinally(true);
    byteCodeGenerator->SetHasTry(true);
    byteCodeGenerator->TopFuncInfo()->byteCodeFunction->SetDontInline(true);

    // do a ToObject on the collection
    Js::RegSlot tmpObj = funcInfo->AcquireTmpRegister();
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Conv_Obj, tmpObj, loopNode->sxForInOrForOf.pnodeObj->location);

    EmitGetIterator(loopNode->location, tmpObj, byteCodeGenerator, funcInfo);
    funcInfo->ReleaseTmpRegister(tmpObj);

    // The whole loop is surrounded with try..catch..finally - in order to capture the abrupt completion.
    Js::ByteCodeLabel finallyLabel = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel catchLabel = byteCodeGenerator->Writer()->DefineLabel();
    byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocation);
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocationFinally);

    ByteCodeGenerator::TryScopeRecord tryRecForTryFinally(Js::OpCode::TryFinallyWithYield, finallyLabel);

    if (isCoroutine)
    {TRACE_IT(39745);
        byteCodeGenerator->Writer()->BrReg2(Js::OpCode::TryFinallyWithYield, finallyLabel, regException, regOffset);
        tryRecForTryFinally.reg1 = regException;
        tryRecForTryFinally.reg2 = regOffset;
        byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForTryFinally);
    }
    else
    {TRACE_IT(39746);
        byteCodeGenerator->Writer()->Br(Js::OpCode::TryFinally, finallyLabel);
    }

    byteCodeGenerator->Writer()->Br(Js::OpCode::TryCatch, catchLabel);

    ByteCodeGenerator::TryScopeRecord tryRecForTry(Js::OpCode::TryCatch, catchLabel);
    if (isCoroutine)
    {TRACE_IT(39747);
        byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForTry);
    }

    byteCodeGenerator->EndStatement(loopNode);

    // Need to increment loop count whether we are going into profile or not for HasLoop()
    uint loopId = byteCodeGenerator->Writer()->EnterLoop(loopEntrance);
    loopNode->sxForInOrForOf.loopId = loopId;

    byteCodeGenerator->StartStatement(loopNode->sxForInOrForOf.pnodeLval);

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocation);
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, shouldCallReturnFunctionLocationFinally);

    EmitIteratorNext(loopNode->sxForInOrForOf.itemLocation, loopNode->location, Js::Constants::NoRegister, byteCodeGenerator, funcInfo);

    Js::RegSlot doneLocation = funcInfo->AcquireTmpRegister();
    EmitIteratorComplete(doneLocation, loopNode->sxForInOrForOf.itemLocation, byteCodeGenerator, funcInfo);

    // branch past loop if the result's done property is truthy
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, continuePastLoop, doneLocation);
    funcInfo->ReleaseTmpRegister(doneLocation);

    // otherwise put result's value property in itemLocation
    EmitIteratorValue(loopNode->sxForInOrForOf.itemLocation, loopNode->sxForInOrForOf.itemLocation, byteCodeGenerator, funcInfo);

    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocation);
    byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, shouldCallReturnFunctionLocationFinally);

    EmitForInOfLoopBody(loopNode, loopEntrance, continuePastLoop, byteCodeGenerator, funcInfo, fReturnValue);

    byteCodeGenerator->Writer()->ExitLoop(loopId);

    EmitCatchAndFinallyBlocks(catchLabel,
        finallyLabel,
        loopNode->location,
        shouldCallReturnFunctionLocation,
        shouldCallReturnFunctionLocationFinally,
        regException,
        regOffset,
        byteCodeGenerator,
        funcInfo);

    if (!byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
    {TRACE_IT(39748);
        EndEmitBlock(loopNode->sxForInOrForOf.pnodeBlock, byteCodeGenerator, funcInfo);
    }
}

void EmitArrayLiteral(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39749);
    funcInfo->AcquireLoc(pnode);
    ParseNode *args = pnode->sxUni.pnode1;
    if (args == nullptr)
    {TRACE_IT(39750);
        byteCodeGenerator->Writer()->Reg1Unsigned1(
            pnode->sxArrLit.hasMissingValues ? Js::OpCode::NewScArrayWithMissingValues : Js::OpCode::NewScArray,
            pnode->location,
            ByteCodeGenerator::DefaultArraySize);
    }
    else
    {
        SetNewArrayElements(pnode, pnode->location, byteCodeGenerator, funcInfo);
    }
}

void EmitJumpCleanup(ParseNode *pnode, ParseNode *pnodeTarget, ByteCodeGenerator *byteCodeGenerator, FuncInfo * funcInfo)
{TRACE_IT(39751);
    for (; pnode != pnodeTarget; pnode = pnode->sxStmt.pnodeOuter)
    {TRACE_IT(39752);
        switch (pnode->nop)
        {
        case knopTry:
        case knopCatch:
        case knopFinally:
            // We insert OpCode::Leave when there is a 'return' inside try/catch/finally.
            // This is for flow control and does not participate in identifying boundaries of try/catch blocks,
            // thus we shouldn't call RecordCrossFrameEntryExitRecord() here.
            byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
            break;

        case knopForOf:
#if ENABLE_PROFILE_INFO
            if (Js::DynamicProfileInfo::EnableImplicitCallFlags(funcInfo->GetParsedFunctionBody()))
            {TRACE_IT(39753);
                byteCodeGenerator->Writer()->Unsigned1(Js::OpCode::ProfiledLoopEnd, pnode->sxLoop.loopId);
            }
#endif
            // The ForOf loop code is wrapped around try..catch..finally - Forcing couple Leave bytecode over here
            byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
            byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
            break;

#if ENABLE_PROFILE_INFO
        case knopWhile:
        case knopDoWhile:
        case knopFor:
        case knopForIn:
            if (Js::DynamicProfileInfo::EnableImplicitCallFlags(funcInfo->GetParsedFunctionBody()))
            {TRACE_IT(39754);
                byteCodeGenerator->Writer()->Unsigned1(Js::OpCode::ProfiledLoopEnd, pnode->sxLoop.loopId);
            }
            break;
#endif

        }
    }
}

void EmitBinaryOpnds(ParseNode *pnode1, ParseNode *pnode2, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    // If opnd2 can overwrite opnd1, make sure the value of opnd1 is stashed away.
    if (MayHaveSideEffectOnNode(pnode1, pnode2))
    {
        SaveOpndValue(pnode1, funcInfo);
    }

    Emit(pnode1, byteCodeGenerator, funcInfo, false);

    if (pnode1->nop == knopComputedName && pnode2->nop == knopClassDecl &&
        (pnode2->sxClass.pnodeConstructor == nullptr || pnode2->sxClass.pnodeConstructor->nop != knopVarDecl))
    {
        Emit(pnode2, byteCodeGenerator, funcInfo, false, false, pnode1);
    }
    else
    {
        Emit(pnode2, byteCodeGenerator, funcInfo, false);
    }
}

void EmitBinaryReference(ParseNode *pnode1, ParseNode *pnode2, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fLoadLhs)
{TRACE_IT(39755);
    // Make sure that the RHS of an assignment doesn't kill the opnd's of the expression on the LHS.
    switch (pnode1->nop)
    {
    case knopName:
        if (fLoadLhs && MayHaveSideEffectOnNode(pnode1, pnode2))
        {
            // Given x op y, y may kill x, so stash x.
            // Note that this only matters if we're loading x prior to the op.
            SaveOpndValue(pnode1, funcInfo);
        }
        break;
    case knopDot:
        if (fLoadLhs)
        {TRACE_IT(39756);
            // We're loading the value of the LHS before the RHS, so make sure the LHS gets a register first.
            funcInfo->AcquireLoc(pnode1);
        }
        if (MayHaveSideEffectOnNode(pnode1->sxBin.pnode1, pnode2))
        {TRACE_IT(39757);
            // Given x.y op z, z may kill x, so stash x away.
            SaveOpndValue(pnode1->sxBin.pnode1, funcInfo);
        }
        break;
    case knopIndex:
        if (fLoadLhs)
        {TRACE_IT(39758);
            // We're loading the value of the LHS before the RHS, so make sure the LHS gets a register first.
            funcInfo->AcquireLoc(pnode1);
        }
        if (MayHaveSideEffectOnNode(pnode1->sxBin.pnode1, pnode2) ||
            MayHaveSideEffectOnNode(pnode1->sxBin.pnode1, pnode1->sxBin.pnode2))
        {TRACE_IT(39759);
            // Given x[y] op z, y or z may kill x, so stash x away.
            SaveOpndValue(pnode1->sxBin.pnode1, funcInfo);
        }
        if (MayHaveSideEffectOnNode(pnode1->sxBin.pnode2, pnode2))
        {TRACE_IT(39760);
            // Given x[y] op z, z may kill y, so stash y away.
            // But make sure that x gets a register before y.
            funcInfo->AcquireLoc(pnode1->sxBin.pnode1);
            SaveOpndValue(pnode1->sxBin.pnode2, funcInfo);
        }
        break;
    }

    if (fLoadLhs)
    {
        // Emit code to load the value of the LHS.
        EmitLoad(pnode1, byteCodeGenerator, funcInfo);
    }
    else
    {
        // Emit code to evaluate the LHS opnds, but don't load the LHS's value.
        EmitReference(pnode1, byteCodeGenerator, funcInfo);
    }

    // Evaluate the RHS.
    Emit(pnode2, byteCodeGenerator, funcInfo, false);
}

void EmitUseBeforeDeclarationRuntimeError(ByteCodeGenerator * byteCodeGenerator, Js::RegSlot location)
{TRACE_IT(39761);
    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_UseBeforeDeclaration));

    if (location != Js::Constants::NoRegister)
    {TRACE_IT(39762);
        // Optionally load something into register in order to do not confuse IRBuilder. This value will never be used.
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, location);
    }
}

void EmitUseBeforeDeclaration(Symbol *sym, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39763);
    // Don't emit static use-before-declaration error in a closure or dynamic scope case. We detect such cases with dynamic checks,
    // if necessary.
    if (sym != nullptr &&
        !sym->GetIsModuleExportStorage() &&
        sym->GetNeedDeclaration() &&
        byteCodeGenerator->GetCurrentScope()->HasStaticPathToAncestor(sym->GetScope()) &&
        sym->GetScope()->GetFunc() == funcInfo)
    {
        EmitUseBeforeDeclarationRuntimeError(byteCodeGenerator, Js::Constants::NoRegister);
    }
}

void EmitBinary(Js::OpCode opcode, ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39764);
    byteCodeGenerator->StartStatement(pnode);
    EmitBinaryOpnds(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo);
    funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
    funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
    funcInfo->AcquireLoc(pnode);
    byteCodeGenerator->Writer()->Reg3(opcode,
        pnode->location,
        pnode->sxBin.pnode1->location,
        pnode->sxBin.pnode2->location);
    byteCodeGenerator->EndStatement(pnode);
}

bool CollectConcat(ParseNode *pnodeAdd, DListCounted<ParseNode *, ArenaAllocator>& concatOpnds, ArenaAllocator *arenaAllocator)
{TRACE_IT(39765);
    Assert(pnodeAdd->nop == knopAdd);
    Assert(pnodeAdd->CanFlattenConcatExpr());

    bool doConcatString = false;
    DList<ParseNode*, ArenaAllocator> pnodeStack(arenaAllocator);
    pnodeStack.Prepend(pnodeAdd->sxBin.pnode2);
    ParseNode * pnode = pnodeAdd->sxBin.pnode1;
    while (true)
    {TRACE_IT(39766);
        if (!pnode->CanFlattenConcatExpr())
        {TRACE_IT(39767);
            concatOpnds.Append(pnode);
        }
        else if (pnode->nop == knopStr)
        {TRACE_IT(39768);
            concatOpnds.Append(pnode);

            // Detect if there are any string larger then the append size limit.
            // If there are, we can do concat; otherwise, still use add so we will not lose the AddLeftDead opportunities.
            doConcatString = doConcatString || !Js::CompoundString::ShouldAppendChars(pnode->sxPid.pid->Cch());
        }
        else
        {TRACE_IT(39769);
            Assert(pnode->nop == knopAdd);
            pnodeStack.Prepend(pnode->sxBin.pnode2);
            pnode = pnode->sxBin.pnode1;
            continue;
        }

        if (pnodeStack.Empty())
        {TRACE_IT(39770);
            break;
        }

        pnode = pnodeStack.Head();
        pnodeStack.RemoveHead();
    }

    return doConcatString;
}

void EmitConcat3(ParseNode *pnode, ParseNode *pnode1, ParseNode *pnode2, ParseNode *pnode3, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39771);
    byteCodeGenerator->StartStatement(pnode);
    if (MayHaveSideEffectOnNode(pnode1, pnode2) || MayHaveSideEffectOnNode(pnode1, pnode3))
    {
        SaveOpndValue(pnode1, funcInfo);
    }

    if (MayHaveSideEffectOnNode(pnode2, pnode3))
    {
        SaveOpndValue(pnode2, funcInfo);
    }

    Emit(pnode1, byteCodeGenerator, funcInfo, false);
    Emit(pnode2, byteCodeGenerator, funcInfo, false);
    Emit(pnode3, byteCodeGenerator, funcInfo, false);
    funcInfo->ReleaseLoc(pnode3);
    funcInfo->ReleaseLoc(pnode2);
    funcInfo->ReleaseLoc(pnode1);
    funcInfo->AcquireLoc(pnode);
    byteCodeGenerator->Writer()->Reg4(Js::OpCode::Concat3,
        pnode->location,
        pnode1->location,
        pnode2->location,
        pnode3->location);
    byteCodeGenerator->EndStatement(pnode);
}

void EmitNewConcatStrMulti(ParseNode *pnode, uint8 count, ParseNode *pnode1, ParseNode *pnode2, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{
    EmitBinaryOpnds(pnode1, pnode2, byteCodeGenerator, funcInfo);
    funcInfo->ReleaseLoc(pnode2);
    funcInfo->ReleaseLoc(pnode1);
    funcInfo->AcquireLoc(pnode);
    byteCodeGenerator->Writer()->Reg3B1(Js::OpCode::NewConcatStrMulti,
        pnode->location,
        pnode1->location,
        pnode2->location,
        count);
}

void EmitAdd(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{TRACE_IT(39772);
    Assert(pnode->nop == knopAdd);

    if (pnode->CanFlattenConcatExpr())
    {TRACE_IT(39773);
        // We should only have a string concat if the feature is on.
        Assert(!PHASE_OFF1(Js::ByteCodeConcatExprOptPhase));
        DListCounted<ParseNode*, ArenaAllocator> concatOpnds(byteCodeGenerator->GetAllocator());
        bool doConcatString = CollectConcat(pnode, concatOpnds, byteCodeGenerator->GetAllocator());
        if (doConcatString)
        {TRACE_IT(39774);
            uint concatCount = concatOpnds.Count();
            Assert(concatCount >= 2);

            // Don't do concatN if the number is too high
            // CONSIDER: although we could have done multiple ConcatNs
            if (concatCount > 2 && concatCount <= UINT8_MAX)
            {TRACE_IT(39775);
#if DBG
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
                ParseNode * pnode1 = concatOpnds.Head();
                concatOpnds.RemoveHead();
                ParseNode * pnode2 = concatOpnds.Head();
                concatOpnds.RemoveHead();
                if (concatCount == 3)
                {TRACE_IT(39776);
                    OUTPUT_TRACE_DEBUGONLY(Js::ByteCodeConcatExprOptPhase, _u("%s(%s) offset:#%d : Concat3\n"),
                        funcInfo->GetParsedFunctionBody()->GetDisplayName(),
                        funcInfo->GetParsedFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                        byteCodeGenerator->Writer()->ByteCodeDataSize());
                    EmitConcat3(pnode, pnode1, pnode2, concatOpnds.Head(), byteCodeGenerator, funcInfo);
                    return;
                }

                OUTPUT_TRACE_DEBUGONLY(Js::ByteCodeConcatExprOptPhase, _u("%s(%s) offset:#%d: ConcatMulti %d\n"),
                    funcInfo->GetParsedFunctionBody()->GetDisplayName(),
                    funcInfo->GetParsedFunctionBody()->GetDebugNumberSet(debugStringBuffer),
                    byteCodeGenerator->Writer()->ByteCodeDataSize(), concatCount);
                byteCodeGenerator->StartStatement(pnode);
                funcInfo->AcquireLoc(pnode);

                // CONSIDER: this may cause the backend not able CSE repeating pattern within the concat.
                EmitNewConcatStrMulti(pnode, (uint8)concatCount, pnode1, pnode2, byteCodeGenerator, funcInfo);

                uint i = 2;
                do
                {TRACE_IT(39777);
                    ParseNode * currNode = concatOpnds.Head();
                    concatOpnds.RemoveHead();
                    ParseNode * currNode2 = concatOpnds.Head();
                    concatOpnds.RemoveHead();

                    EmitBinaryOpnds(currNode, currNode2, byteCodeGenerator, funcInfo);
                    funcInfo->ReleaseLoc(currNode2);
                    funcInfo->ReleaseLoc(currNode);
                    byteCodeGenerator->Writer()->Reg3B1(
                        Js::OpCode::SetConcatStrMultiItem2, pnode->location, currNode->location, currNode2->location, (uint8)i);
                    i += 2;
                } while (concatOpnds.Count() > 1);

                if (!concatOpnds.Empty())
                {TRACE_IT(39778);
                    ParseNode * currNode = concatOpnds.Head();
                    Emit(currNode, byteCodeGenerator, funcInfo, false);
                    funcInfo->ReleaseLoc(currNode);
                    byteCodeGenerator->Writer()->Reg2B1(
                        Js::OpCode::SetConcatStrMultiItem, pnode->location, currNode->location, (uint8)i);
                    i++;
                }

                Assert(concatCount == i);
                byteCodeGenerator->EndStatement(pnode);
                return;
            }
        }

        // Since we collected all the node already, let's just emit them instead of doing it recursively.
        byteCodeGenerator->StartStatement(pnode);
        ParseNode * currNode = concatOpnds.Head();
        concatOpnds.RemoveHead();
        ParseNode * currNode2 = concatOpnds.Head();
        concatOpnds.RemoveHead();

        EmitBinaryOpnds(currNode, currNode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(currNode2);
        funcInfo->ReleaseLoc(currNode);
        Js::RegSlot dstReg = funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->Writer()->Reg3(
            Js::OpCode::Add_A, dstReg, currNode->location, currNode2->location);
        while (!concatOpnds.Empty())
        {TRACE_IT(39779);
            currNode = concatOpnds.Head();
            concatOpnds.RemoveHead();
            Emit(currNode, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(currNode);
            byteCodeGenerator->Writer()->Reg3(
                Js::OpCode::Add_A, dstReg, dstReg, currNode->location);
        }
        byteCodeGenerator->EndStatement(pnode);
    }
    else
    {TRACE_IT(39780);
        EmitBinary(Js::OpCode::Add_A, pnode, byteCodeGenerator, funcInfo);
    }
}

void EmitSuperFieldPatch(FuncInfo* funcInfo, ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator)
{TRACE_IT(39781);
    ParseNodePtr propFuncNode = funcInfo->root;

    if (byteCodeGenerator->GetFlags() & fscrEval)
    {TRACE_IT(39782);
        // If we are inside an eval, ScopedLdHomeObj will take care of the patch.
        return;
    }

    if (funcInfo->IsLambda())
    {TRACE_IT(39783);
        FuncInfo *parent = byteCodeGenerator->FindEnclosingNonLambda();
        propFuncNode = parent->root;
    }

    // No need to emit a LdFld for the constructor.
    if (propFuncNode->sxFnc.IsClassConstructor())
    {TRACE_IT(39784);
        return;
    }

    if (!propFuncNode->sxFnc.IsClassMember() || propFuncNode->sxFnc.pid == nullptr)
    {TRACE_IT(39785);
        // Non-methods will fail lookup.
        return;
    }
    if (propFuncNode->sxFnc.pid->GetPropertyId() == Js::Constants::NoProperty)
    {TRACE_IT(39786);
        byteCodeGenerator->AssignPropertyId(propFuncNode->sxFnc.pid);
    }

    // Load the current method's property ID from super instead of using super directly.
    Js::RegSlot superLoc = funcInfo->superRegister;
    pnode->sxCall.pnodeTarget->location = Js::Constants::NoRegister;
    Js::RegSlot superPropLoc = funcInfo->AcquireLoc(pnode->sxCall.pnodeTarget);
    Js::PropertyId propertyId = propFuncNode->sxFnc.pid->GetPropertyId();
    uint cacheId = funcInfo->FindOrAddInlineCacheId(superLoc, propertyId, true, false);
    byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdMethodFld, superPropLoc, superLoc, cacheId);

    propFuncNode->sxFnc.pnodeName = nullptr;
}



void ByteCodeGenerator::EmitLeaveOpCodesBeforeYield()
{TRACE_IT(39787);
    for (TryScopeRecord* node = this->tryScopeRecordsList.Tail(); node != nullptr; node = node->Previous())
    {TRACE_IT(39788);
        switch (node->op)
        {
        case Js::OpCode::TryFinallyWithYield:
            this->Writer()->Empty(Js::OpCode::LeaveNull);
            break;
        case Js::OpCode::TryCatch:
        case Js::OpCode::ResumeFinally:
        case Js::OpCode::ResumeCatch:
            this->Writer()->Empty(Js::OpCode::Leave);
            break;
        default:
            AssertMsg(false, "Unexpected OpCode before Yield in the Try-Catch-Finally cache for generator!");
            break;
        }
    }
}

void ByteCodeGenerator::EmitTryBlockHeadersAfterYield()
{TRACE_IT(39789);
    for (TryScopeRecord* node = this->tryScopeRecordsList.Head(); node != nullptr; node = node->Next())
    {TRACE_IT(39790);
        switch (node->op)
        {
        case Js::OpCode::TryCatch:
            this->Writer()->Br(node->op, node->label);
            break;
        case Js::OpCode::TryFinallyWithYield:
        case Js::OpCode::ResumeFinally:
            this->Writer()->BrReg2(node->op, node->label, node->reg1, node->reg2);
            break;
        case Js::OpCode::ResumeCatch:
            this->Writer()->Empty(node->op);
            break;
        default:
            AssertMsg(false, "Unexpected OpCode after yield in the Try-Catch-Finally cache for generator!");
            break;
        }
    }
}

void EmitYield(Js::RegSlot inputLocation, Js::RegSlot resultLocation, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo,
    Js::RegSlot yieldStarIterator = Js::Constants::NoRegister)
{TRACE_IT(39791);
    // If the bytecode emitted by this function is part of 'yield*', inputLocation is the object
    // returned by the iterable's next/return/throw method. Otherwise, it is the yielded value.
    if (yieldStarIterator == Js::Constants::NoRegister)
    {TRACE_IT(39792);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::NewScObjectSimple, funcInfo->yieldRegister);

        uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->yieldRegister, Js::PropertyIds::value, false, true);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::StFld, inputLocation, funcInfo->yieldRegister, cacheId);

        cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->yieldRegister, Js::PropertyIds::done, false, true);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::StFld, funcInfo->falseConstantRegister, funcInfo->yieldRegister, cacheId);
    }
    else
    {TRACE_IT(39793);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, funcInfo->yieldRegister, inputLocation);
    }

    byteCodeGenerator->EmitLeaveOpCodesBeforeYield();
    byteCodeGenerator->Writer()->Reg2(Js::OpCode::Yield, funcInfo->yieldRegister, funcInfo->yieldRegister);
    byteCodeGenerator->EmitTryBlockHeadersAfterYield();

    if (yieldStarIterator == Js::Constants::NoRegister)
    {TRACE_IT(39794);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::ResumeYield, resultLocation, funcInfo->yieldRegister);
    }
    else
    {TRACE_IT(39795);
        byteCodeGenerator->Writer()->Reg3(Js::OpCode::ResumeYieldStar, resultLocation, funcInfo->yieldRegister, yieldStarIterator);
    }
}

void EmitYieldStar(ParseNode* yieldStarNode, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{TRACE_IT(39796);
    funcInfo->AcquireLoc(yieldStarNode);

    Js::ByteCodeLabel loopEntrance = byteCodeGenerator->Writer()->DefineLabel();
    Js::ByteCodeLabel continuePastLoop = byteCodeGenerator->Writer()->DefineLabel();

    Js::RegSlot iteratorLocation = funcInfo->AcquireTmpRegister();

    // Evaluate operand
    Emit(yieldStarNode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
    funcInfo->ReleaseLoc(yieldStarNode->sxUni.pnode1);

    EmitGetIterator(iteratorLocation, yieldStarNode->sxUni.pnode1->location, byteCodeGenerator, funcInfo);

    // Call the iterator's next()
    EmitIteratorNext(yieldStarNode->location, iteratorLocation, funcInfo->undefinedConstantRegister, byteCodeGenerator, funcInfo);

    uint loopId = byteCodeGenerator->Writer()->EnterLoop(loopEntrance);
    // since a yield* doesn't have a user defined body, we cannot return from this loop
    // which means we don't need to support EmitJumpCleanup() and there do not need to
    // remember the loopId like the loop statements do.

    Js::RegSlot doneLocation = funcInfo->AcquireTmpRegister();
    EmitIteratorComplete(doneLocation, yieldStarNode->location, byteCodeGenerator, funcInfo);

    // branch past the loop if the done property is truthy
    byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, continuePastLoop, doneLocation);
    funcInfo->ReleaseTmpRegister(doneLocation);

    EmitYield(yieldStarNode->location, yieldStarNode->location, byteCodeGenerator, funcInfo, iteratorLocation);

    funcInfo->ReleaseTmpRegister(iteratorLocation);

    byteCodeGenerator->Writer()->Br(loopEntrance);
    byteCodeGenerator->Writer()->MarkLabel(continuePastLoop);
    byteCodeGenerator->Writer()->ExitLoop(loopId);

    // Put the iterator result's value in yieldStarNode->location.
    // It will be used as the result value of the yield* operator expression.
    EmitIteratorValue(yieldStarNode->location, yieldStarNode->location, byteCodeGenerator, funcInfo);
}

void TrackIntConstantsOnGlobalUserObject(ByteCodeGenerator *byteCodeGenerator, bool isSymGlobalAndSingleAssignment, Js::PropertyId propertyId)
{TRACE_IT(39797);
    if (isSymGlobalAndSingleAssignment)
    {TRACE_IT(39798);
        byteCodeGenerator->GetScriptContext()->TrackIntConstPropertyOnGlobalUserObject(propertyId);
    }
}

void TrackIntConstantsOnGlobalObject(ByteCodeGenerator *byteCodeGenerator, bool isSymGlobalAndSingleAssignment, Js::PropertyId propertyId)
{TRACE_IT(39799);
    if (isSymGlobalAndSingleAssignment)
    {TRACE_IT(39800);
        byteCodeGenerator->GetScriptContext()->TrackIntConstPropertyOnGlobalObject(propertyId);
    }
}

void TrackIntConstantsOnGlobalObject(ByteCodeGenerator *byteCodeGenerator, Symbol *sym)
{TRACE_IT(39801);
    if (sym && sym->GetIsGlobal() && sym->IsAssignedOnce())
    {TRACE_IT(39802);
        Js::PropertyId propertyId = sym->EnsurePosition(byteCodeGenerator);
        byteCodeGenerator->GetScriptContext()->TrackIntConstPropertyOnGlobalObject(propertyId);
    }
}

void TrackMemberNodesInObjectForIntConstants(ByteCodeGenerator *byteCodeGenerator, ParseNodePtr objNode)
{TRACE_IT(39803);
    Assert(objNode->nop == knopObject);
    Assert(ParseNode::Grfnop(objNode->nop) & fnopUni);

    ParseNodePtr memberList = objNode->sxUni.pnode1;

    while (memberList != nullptr)
    {TRACE_IT(39804);
        ParseNodePtr memberNode = memberList->nop == knopList ? memberList->sxBin.pnode1 : memberList;
        ParseNodePtr memberNameNode = memberNode->sxBin.pnode1;
        ParseNodePtr memberValNode = memberNode->sxBin.pnode2;

        if (memberNameNode->nop != knopComputedName && memberValNode->nop == knopInt)
        {TRACE_IT(39805);
            Js::PropertyId propertyId = memberNameNode->sxPid.PropertyIdFromNameNode();
            TrackIntConstantsOnGlobalUserObject(byteCodeGenerator, true, propertyId);
        }

        memberList = memberList->nop == knopList ? memberList->sxBin.pnode2 : nullptr;
    }
}

void TrackGlobalIntAssignmentsForknopDotProps(ParseNodePtr knopDotNode, ByteCodeGenerator * byteCodeGenerator)
{TRACE_IT(39806);
    Assert(knopDotNode->nop == knopDot);

    ParseNodePtr objectNode = knopDotNode->sxBin.pnode1;
    ParseNodePtr propertyNode = knopDotNode->sxBin.pnode2;
    bool isSymGlobalAndSingleAssignment = false;

    if (objectNode->nop == knopName)
    {TRACE_IT(39807);
        Symbol * sym = objectNode->sxVar.sym;
        isSymGlobalAndSingleAssignment = sym && sym->GetIsGlobal() && sym->IsAssignedOnce() && propertyNode->sxPid.pid->IsSingleAssignment();
        Js::PropertyId propertyId = propertyNode->sxPid.PropertyIdFromNameNode();
        TrackIntConstantsOnGlobalUserObject(byteCodeGenerator, isSymGlobalAndSingleAssignment, propertyId);
    }
    else if (objectNode->nop == knopThis)
    {TRACE_IT(39808);
        // Assume knopThis always refer to GlobalObject
        // Cases like "this.a = "
        isSymGlobalAndSingleAssignment = propertyNode->sxPid.pid->IsSingleAssignment();
        Js::PropertyId propertyId = propertyNode->sxPid.PropertyIdFromNameNode();
        TrackIntConstantsOnGlobalObject(byteCodeGenerator, isSymGlobalAndSingleAssignment, propertyId);
    }
}

void TrackGlobalIntAssignments(ParseNodePtr pnode, ByteCodeGenerator * byteCodeGenerator)
{TRACE_IT(39809);
    // Track the Global Int Constant properties' assignments here.
    uint nodeType = ParseNode::Grfnop(pnode->nop);
    if (nodeType & fnopAsg)
    {TRACE_IT(39810);
        if (nodeType & fnopBin)
        {TRACE_IT(39811);
            ParseNodePtr lhs = pnode->sxBin.pnode1;
            ParseNodePtr rhs = pnode->sxBin.pnode2;

            Assert(lhs && rhs);

            // Don't track other than integers and objects with member nodes.
            if (rhs->nop == knopObject && (ParseNode::Grfnop(rhs->nop) & fnopUni))
            {
                TrackMemberNodesInObjectForIntConstants(byteCodeGenerator, rhs);
            }
            else if (rhs->nop != knopInt &&
                ((rhs->nop != knopLsh && rhs->nop != knopRsh) || (rhs->sxBin.pnode1->nop != knopInt || rhs->sxBin.pnode2->nop != knopInt)))
            {TRACE_IT(39812);
                return;
            }

            if (lhs->nop == knopName)
            {TRACE_IT(39813);
                // Handle "a = <Integer>" cases here
                Symbol * sym = lhs->sxVar.sym;
                TrackIntConstantsOnGlobalObject(byteCodeGenerator, sym);
            }
            else if (lhs->nop == knopDot && lhs->sxBin.pnode2->nop == knopName)
            {
                // Cases like "obj.a = <Integer>"
                TrackGlobalIntAssignmentsForknopDotProps(lhs, byteCodeGenerator);
            }
        }
        else if (nodeType & fnopUni)
        {TRACE_IT(39814);
            ParseNodePtr lhs = pnode->sxUni.pnode1;

            if (lhs->nop == knopName)
            {TRACE_IT(39815);
                // Cases like "a++"
                Symbol * sym = lhs->sxVar.sym;
                TrackIntConstantsOnGlobalObject(byteCodeGenerator, sym);
            }
            else if (lhs->nop == knopDot && lhs->sxBin.pnode2->nop == knopName)
            {
                // Cases like "obj.a++"
                TrackGlobalIntAssignmentsForknopDotProps(lhs, byteCodeGenerator);
            }
        }
    }
}

void Emit(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo, BOOL fReturnValue, bool isConstructorCall, ParseNode * bindPnode, bool isTopLevel)
{TRACE_IT(39816);
    if (pnode == nullptr)
    {TRACE_IT(39817);
        return;
    }

    ThreadContext::ProbeCurrentStackNoDispose(Js::Constants::MinStackByteCodeVisitor, byteCodeGenerator->GetScriptContext());

    TrackGlobalIntAssignments(pnode, byteCodeGenerator);

    // printNop(pnode->nop);
    switch (pnode->nop)
    {
    case knopList:
        EmitList(pnode, byteCodeGenerator, funcInfo);
        break;
    case knopInt:
        // currently, these are loaded at the top
        break;
        // PTNODE(knopFlt        , "flt const"    ,None    ,Flt  ,fnopLeaf|fnopConst)
    case knopFlt:
        // currently, these are loaded at the top
        break;
        // PTNODE(knopStr        , "str const"    ,None    ,Pid  ,fnopLeaf|fnopConst)
    case knopStr:
        // TODO: protocol for combining string constants
        break;
        // PTNODE(knopRegExp     , "reg expr"    ,None    ,Pid  ,fnopLeaf|fnopConst)
    case knopRegExp:
        funcInfo->GetParsedFunctionBody()->SetLiteralRegex(pnode->sxPid.regexPatternIndex, pnode->sxPid.regexPattern);
        byteCodeGenerator->Writer()->Reg1Unsigned1(Js::OpCode::NewRegEx, funcInfo->AcquireLoc(pnode), pnode->sxPid.regexPatternIndex);
        break;          // PTNODE(knopThis       , "this"        ,None    ,None ,fnopLeaf)
    case knopThis:
        // enregistered
        // Try to load 'this' from a scope slot if we are in a derived class constructor with scope slots. Otherwise, this is a nop.
        byteCodeGenerator->EmitScopeSlotLoadThis(funcInfo, funcInfo->thisPointerRegister);
        break;
        // PTNODE(knopNewTarget      , "new.target"       ,None    , None        , fnopLeaf)
    case knopNewTarget:
        break;
        // PTNODE(knopSuper      , "super"       ,None    , None        , fnopLeaf)
    case knopSuper:
        if (!funcInfo->IsClassMember())
        {TRACE_IT(39818);
            FuncInfo* nonLambdaFunc = funcInfo;
            if (funcInfo->IsLambda())
            {TRACE_IT(39819);
                nonLambdaFunc = byteCodeGenerator->FindEnclosingNonLambda();
            }

            if (nonLambdaFunc->IsGlobalFunction())
            {TRACE_IT(39820);
                if ((byteCodeGenerator->GetFlags() & fscrEval))
                {TRACE_IT(39821);
                    byteCodeGenerator->Writer()->Reg1(isConstructorCall ? Js::OpCode::ScopedLdFuncObj : Js::OpCode::ScopedLdHomeObj, funcInfo->AcquireLoc(pnode));
                }
                else
                {TRACE_IT(39822);
                    byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_BadSuperReference));
                }
            }
        }
        break;
        // PTNODE(knopNull       , "null"        ,Null    ,None ,fnopLeaf)
    case knopNull:
        // enregistered
        break;
        // PTNODE(knopFalse      , "false"        ,False   ,None ,fnopLeaf)
    case knopFalse:
        // enregistered
        break;
        // PTNODE(knopTrue       , "true"        ,True    ,None ,fnopLeaf)
    case knopTrue:
        // enregistered
        break;
        // PTNODE(knopEmpty      , "empty"        ,Empty   ,None ,fnopLeaf)
    case knopEmpty:
        break;
        // Unary operators.
    // PTNODE(knopNot        , "~"            ,BitNot  ,Uni  ,fnopUni)
    case knopNot:
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->Writer()->Reg2(
            Js::OpCode::Not_A, funcInfo->AcquireLoc(pnode), pnode->sxUni.pnode1->location);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
        // PTNODE(knopNeg        , "unary -"    ,Neg     ,Uni  ,fnopUni)
    case knopNeg:
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->Writer()->Reg2(
            Js::OpCode::Neg_A, pnode->location, pnode->sxUni.pnode1->location);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
        // PTNODE(knopPos        , "unary +"    ,Pos     ,Uni  ,fnopUni)
    case knopPos:
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->Writer()->Reg2(
            Js::OpCode::Conv_Num, funcInfo->AcquireLoc(pnode), pnode->sxUni.pnode1->location);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
        // PTNODE(knopLogNot     , "!"            ,LogNot  ,Uni  ,fnopUni)
    case knopLogNot:
    {
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        Js::ByteCodeLabel doneLabel = byteCodeGenerator->Writer()->DefineLabel();
        // For boolean expressions that compute a result, we have to burn a register for the result
        // so that the back end can identify it cheaply as a single temp lifetime. Revisit this if we do
        // full-on renaming in the back end.
        funcInfo->AcquireLoc(pnode);
        if (pnode->sxUni.pnode1->nop == knopInt)
        {TRACE_IT(39823);
            int32 value = pnode->sxUni.pnode1->sxInt.lw;
            Js::OpCode op = value ? Js::OpCode::LdFalse : Js::OpCode::LdTrue;
            byteCodeGenerator->Writer()->Reg1(op, pnode->location);
        }
        else
        {TRACE_IT(39824);
            Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdFalse, pnode->location);
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrTrue_A, doneLabel, pnode->sxUni.pnode1->location);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, pnode->location);
            byteCodeGenerator->Writer()->MarkLabel(doneLabel);
        }
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
    }
    // PTNODE(knopEllipsis     , "..."       ,Spread  ,Uni         , fnopUni)
    case knopEllipsis:
    {TRACE_IT(39825);
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        // Transparently pass the location of the array.
        pnode->location = pnode->sxUni.pnode1->location;
        break;
    }
    // PTNODE(knopIncPost    , "post++"    ,Inc     ,Uni  ,fnopUni|fnopAsg)
    case knopIncPost:
    case knopDecPost:
        // FALL THROUGH to the faster pre-inc/dec case if the result of the expression is not needed.
        if (pnode->isUsed || fReturnValue)
        {TRACE_IT(39826);
            byteCodeGenerator->StartStatement(pnode);
            Js::OpCode op = Js::OpCode::Add_A;
            if (pnode->nop == knopDecPost)
            {TRACE_IT(39827);
                op = Js::OpCode::Sub_A;
            }
            // Grab a register for the expression result.
            funcInfo->AcquireLoc(pnode);

            // Load the initial value, convert it (this is the expression result), and increment it.
            EmitLoad(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Conv_Num, pnode->location, pnode->sxUni.pnode1->location);

            Js::RegSlot incDecResult = pnode->sxUni.pnode1->location;
            if (funcInfo->RegIsConst(incDecResult))
            {TRACE_IT(39828);
                // Avoid letting the add/sub overwrite a constant reg, as this may actually change the
                // contents of the constant table.
                incDecResult = funcInfo->AcquireTmpRegister();
            }

            Js::RegSlot oneReg = funcInfo->constantToRegister.LookupWithKey(1, Js::Constants::NoRegister);
            Assert(oneReg != Js::Constants::NoRegister);
            byteCodeGenerator->Writer()->Reg3(op, incDecResult, pnode->location, oneReg);

            // Store the incremented value.
            EmitAssignment(nullptr, pnode->sxUni.pnode1, incDecResult, byteCodeGenerator, funcInfo);

            // Release the incremented value and the l-value.
            if (incDecResult != pnode->sxUni.pnode1->location)
            {TRACE_IT(39829);
                funcInfo->ReleaseTmpRegister(incDecResult);
            }
            funcInfo->ReleaseLoad(pnode->sxUni.pnode1);
            byteCodeGenerator->EndStatement(pnode);

            break;
        }
        else
        {TRACE_IT(39830);
            pnode->nop = (pnode->nop == knopIncPost) ? knopIncPre : knopDecPre;
        }
        // FALL THROUGH to the fast pre-inc/dec case if the result of the expression is not needed.

    // PTNODE(knopIncPre     , "++ pre"    ,Inc     ,Uni  ,fnopUni|fnopAsg)
    case knopIncPre:
    case knopDecPre:
    {TRACE_IT(39831);
        byteCodeGenerator->StartStatement(pnode);
        Js::OpCode op = Js::OpCode::Incr_A;
        if (pnode->nop == knopDecPre)
        {TRACE_IT(39832);
            op = Js::OpCode::Decr_A;
        }

        // Assign a register for the result only if the result is used or the operand can't be assigned to
        // (i.e., is a constant).
        if (pnode->isUsed || fReturnValue)
        {TRACE_IT(39833);
            funcInfo->AcquireLoc(pnode);

            // Load the initial value and increment it (this is the expression result).
            EmitLoad(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo);
            byteCodeGenerator->Writer()->Reg2(op, pnode->location, pnode->sxUni.pnode1->location);

            // Store the incremented value and release the l-value.
            EmitAssignment(nullptr, pnode->sxUni.pnode1, pnode->location, byteCodeGenerator, funcInfo);
            funcInfo->ReleaseLoad(pnode->sxUni.pnode1);
        }
        else
        {TRACE_IT(39834);
            // Load the initial value and increment it (this is the expression result).
            EmitLoad(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo);

            Js::RegSlot incDecResult = pnode->sxUni.pnode1->location;
            if (funcInfo->RegIsConst(incDecResult))
            {TRACE_IT(39835);
                // Avoid letting the add/sub overwrite a constant reg, as this may actually change the
                // contents of the constant table.
                incDecResult = funcInfo->AcquireTmpRegister();
            }

            byteCodeGenerator->Writer()->Reg2(op, incDecResult, pnode->sxUni.pnode1->location);

            // Store the incremented value and release the l-value.
            EmitAssignment(nullptr, pnode->sxUni.pnode1, incDecResult, byteCodeGenerator, funcInfo);
            if (incDecResult != pnode->sxUni.pnode1->location)
            {TRACE_IT(39836);
                funcInfo->ReleaseTmpRegister(incDecResult);
            }
            funcInfo->ReleaseLoad(pnode->sxUni.pnode1);
        }

        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    // PTNODE(knopTypeof     , "typeof"    ,None    ,Uni  ,fnopUni)
    case knopTypeof:
    {
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        ParseNode* pnodeOpnd = pnode->sxUni.pnode1;
        switch (pnodeOpnd->nop)
        {
        case knopDot:
        {TRACE_IT(39837);
            Emit(pnodeOpnd->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
            Js::PropertyId propertyId = pnodeOpnd->sxBin.pnode2->sxPid.PropertyIdFromNameNode();
            Assert(pnodeOpnd->sxBin.pnode2->nop == knopName);
            funcInfo->ReleaseLoc(pnodeOpnd->sxBin.pnode1);
            funcInfo->AcquireLoc(pnode);

            byteCodeGenerator->EmitTypeOfFld(funcInfo, propertyId, pnode->location, pnodeOpnd->sxBin.pnode1->location, Js::OpCode::LdFldForTypeOf);
            break;
        }

        case knopIndex:
        {TRACE_IT(39838);
            EmitBinaryOpnds(pnodeOpnd->sxBin.pnode1, pnodeOpnd->sxBin.pnode2, byteCodeGenerator, funcInfo);
            funcInfo->ReleaseLoc(pnodeOpnd->sxBin.pnode2);
            funcInfo->ReleaseLoc(pnodeOpnd->sxBin.pnode1);
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->Writer()->Element(Js::OpCode::TypeofElem, pnode->location, pnodeOpnd->sxBin.pnode1->location, pnodeOpnd->sxBin.pnode2->location);
            break;
        }
        case knopName:
        {TRACE_IT(39839);
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->EmitPropTypeof(pnode->location, pnodeOpnd->sxPid.sym, pnodeOpnd->sxPid.pid, funcInfo);
            break;
        }

        default:
            Emit(pnodeOpnd, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(pnodeOpnd);
            byteCodeGenerator->Writer()->Reg2(
                Js::OpCode::Typeof, funcInfo->AcquireLoc(pnode), pnodeOpnd->location);
            break;
        }
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
    }
    // PTNODE(knopVoid       , "void"        ,Void    ,Uni  ,fnopUni)
    case knopVoid:
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, funcInfo->AcquireLoc(pnode));
        break;
        // PTNODE(knopArray      , "arr cnst"    ,None    ,Uni  ,fnopUni)
    case knopArray:
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        EmitArrayLiteral(pnode, byteCodeGenerator, funcInfo);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
        // PTNODE(knopObject     , "obj cnst"    ,None    ,Uni  ,fnopUni)
    case knopObject:
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        funcInfo->AcquireLoc(pnode);
        EmitObjectInitializers(pnode->sxUni.pnode1, pnode->location, byteCodeGenerator, funcInfo);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
        // PTNODE(knopComputedName, "[name]"      ,None    ,Uni  ,fnopUni)
    case knopComputedName:
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        if (pnode->location == Js::Constants::NoRegister)
        {TRACE_IT(39840);
            // The name is some expression with no home location. We can just re-use the register.
            pnode->location = pnode->sxUni.pnode1->location;
        }
        else if (pnode->location != pnode->sxUni.pnode1->location)
        {TRACE_IT(39841);
            // The name had to be protected from side-effects of the RHS.
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxUni.pnode1->location);
        }
        break;
        // Binary and Ternary Operators
    case knopAdd:
        EmitAdd(pnode, byteCodeGenerator, funcInfo);
        break;
    case knopSub:
    case knopMul:
    case knopExpo:
    case knopDiv:
    case knopMod:
    case knopOr:
    case knopXor:
    case knopAnd:
    case knopLsh:
    case knopRsh:
    case knopRs2:
    case knopIn:
        EmitBinary(nopToOp[pnode->nop], pnode, byteCodeGenerator, funcInfo);
        break;
    case knopInstOf:
    {
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        EmitBinaryOpnds(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
        funcInfo->AcquireLoc(pnode);
        uint cacheId = funcInfo->NewIsInstInlineCache();
        byteCodeGenerator->Writer()->Reg3C(nopToOp[pnode->nop], pnode->location, pnode->sxBin.pnode1->location,
            pnode->sxBin.pnode2->location, cacheId);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
    }
    break;
    case knopEq:
    case knopEqv:
    case knopNEqv:
    case knopNe:
    case knopLt:
    case knopLe:
    case knopGe:
    case knopGt:
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        EmitBinaryOpnds(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
        funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->Writer()->Reg3(nopToCMOp[pnode->nop], pnode->location, pnode->sxBin.pnode1->location,
            pnode->sxBin.pnode2->location);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
    case knopNew:
    {
        EmitNew(pnode, byteCodeGenerator, funcInfo);

        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    case knopDelete:
    {TRACE_IT(39842);
        ParseNode *pexpr = pnode->sxUni.pnode1;
        byteCodeGenerator->StartStatement(pnode);
        switch (pexpr->nop)
        {
        case knopName:
        {TRACE_IT(39843);
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->EmitPropDelete(pnode->location, pexpr->sxPid.sym, pexpr->sxPid.pid, funcInfo);
            break;
        }
        case knopDot:
        {TRACE_IT(39844);
            Emit(pexpr->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
            Js::PropertyId propertyId = pexpr->sxBin.pnode2->sxPid.PropertyIdFromNameNode();
            funcInfo->ReleaseLoc(pexpr->sxBin.pnode1);
            funcInfo->AcquireLoc(pnode);

            if (pexpr->sxBin.pnode1->nop == knopSuper)
            {TRACE_IT(39845);
                byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeReferenceError, SCODE_CODE(JSERR_DeletePropertyWithSuper));
            }
            else
            {TRACE_IT(39846);
                byteCodeGenerator->Writer()->Property(Js::OpCode::DeleteFld, pnode->location, pexpr->sxBin.pnode1->location,
                    funcInfo->FindOrAddReferencedPropertyId(propertyId));
            }

            break;
        }
        case knopIndex:
        {TRACE_IT(39847);
            EmitBinaryOpnds(pexpr->sxBin.pnode1, pexpr->sxBin.pnode2, byteCodeGenerator, funcInfo);
            funcInfo->ReleaseLoc(pexpr->sxBin.pnode2);
            funcInfo->ReleaseLoc(pexpr->sxBin.pnode1);
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->Writer()->Element(Js::OpCode::DeleteElemI_A, pnode->location, pexpr->sxBin.pnode1->location, pexpr->sxBin.pnode2->location);
            break;
        }
        case knopThis:
        {TRACE_IT(39848);
            funcInfo->AcquireLoc(pnode);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdTrue, pnode->location);
            break;
        }
        default:
        {
            Emit(pexpr, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(pexpr);
            byteCodeGenerator->Writer()->Reg2(
                Js::OpCode::Delete_A, funcInfo->AcquireLoc(pnode), pexpr->location);
            break;
        }
        }
        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    case knopCall:
    {TRACE_IT(39849);
        byteCodeGenerator->StartStatement(pnode);

        if (pnode->sxCall.pnodeTarget->nop == knopSuper)
        {TRACE_IT(39850);
            byteCodeGenerator->EmitSuperCall(funcInfo, pnode, fReturnValue);
        }
        else
        {TRACE_IT(39851);
            if (pnode->sxCall.isApplyCall && funcInfo->GetApplyEnclosesArgs())
            {
                // TODO[ianhall]: Can we remove the ApplyCall bytecode gen time optimization?
                EmitApplyCall(pnode, Js::Constants::NoRegister, byteCodeGenerator, funcInfo, fReturnValue);
            }
            else
            {
                EmitCall(pnode, Js::Constants::NoRegister, byteCodeGenerator, funcInfo, fReturnValue, /*fEvaluateComponents*/ true, /*fHasNewTarget*/ false);
            }
        }

        byteCodeGenerator->EndStatement(pnode);
        break;
    }
    case knopIndex:
    {
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        EmitBinaryOpnds(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
        funcInfo->AcquireLoc(pnode);

        Js::RegSlot callObjLocation = pnode->sxBin.pnode1->location;

        Js::RegSlot protoLocation =
            (pnode->sxBin.pnode1->nop == knopSuper) ?
            byteCodeGenerator->EmitLdObjProto(Js::OpCode::LdHomeObjProto, funcInfo->superRegister, funcInfo) :
            callObjLocation;

        EmitSuperMethodBegin(pnode, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->Element(
            Js::OpCode::LdElemI_A, pnode->location, protoLocation, pnode->sxBin.pnode2->location);

        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
    }
    // this is MemberExpression as rvalue
    case knopDot:
    {TRACE_IT(39852);
        Emit(pnode->sxBin.pnode1, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode1);
        funcInfo->AcquireLoc(pnode);
        Js::PropertyId propertyId = pnode->sxBin.pnode2->sxPid.PropertyIdFromNameNode();

        Js::RegSlot callObjLocation = pnode->sxBin.pnode1->location;
        Js::RegSlot protoLocation = callObjLocation;
        EmitSuperMethodBegin(pnode, byteCodeGenerator, funcInfo);

        uint cacheId = funcInfo->FindOrAddInlineCacheId(callObjLocation, propertyId, false, false);
        if (pnode->IsCallApplyTargetLoad())
        {TRACE_IT(39853);
            if (pnode->sxBin.pnode1->nop == knopSuper)
            {TRACE_IT(39854);
                Js::RegSlot tmpReg = byteCodeGenerator->EmitLdObjProto(Js::OpCode::LdHomeObjProto, funcInfo->superRegister, funcInfo);
                byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFldForCallApplyTarget, pnode->location, tmpReg, cacheId);
            }
            else
            {TRACE_IT(39855);
                byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFldForCallApplyTarget, pnode->location, protoLocation, cacheId);
            }
        }
        else
        {TRACE_IT(39856);
            if (pnode->sxBin.pnode1->nop == knopSuper)
            {TRACE_IT(39857);
                Js::RegSlot tmpReg = byteCodeGenerator->EmitLdObjProto(Js::OpCode::LdHomeObjProto, funcInfo->superRegister, funcInfo);
                byteCodeGenerator->Writer()->PatchablePropertyWithThisPtr(Js::OpCode::LdSuperFld, pnode->location, tmpReg, funcInfo->thisPointerRegister, cacheId, isConstructorCall);
            }
            else
            {TRACE_IT(39858);
                byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, pnode->location, callObjLocation, cacheId, isConstructorCall);
            }
        }

        break;
    }

    // PTNODE(knopAsg        , "="            ,None    ,Bin  ,fnopBin|fnopAsg)
    case knopAsg:
    {TRACE_IT(39859);
        ParseNode *lhs = pnode->sxBin.pnode1;
        ParseNode *rhs = pnode->sxBin.pnode2;
        byteCodeGenerator->StartStatement(pnode);
        if (pnode->isUsed || fReturnValue)
        {TRACE_IT(39860);
            // If the assignment result is used, grab a register to hold it and pass it to EmitAssignment,
            // which will copy the assigned value there.
            funcInfo->AcquireLoc(pnode);
            EmitBinaryReference(lhs, rhs, byteCodeGenerator, funcInfo, false);
            EmitAssignment(pnode, lhs, rhs->location, byteCodeGenerator, funcInfo);
        }
        else
        {
            EmitBinaryReference(lhs, rhs, byteCodeGenerator, funcInfo, false);
            EmitAssignment(nullptr, lhs, rhs->location, byteCodeGenerator, funcInfo);
        }
        funcInfo->ReleaseLoc(rhs);
        if (!(byteCodeGenerator->IsES6DestructuringEnabled() && (lhs->IsPattern())))
        {TRACE_IT(39861);
            funcInfo->ReleaseReference(lhs);
        }
        byteCodeGenerator->EndStatement(pnode);
        break;
    }

    case knopName:
        funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->EmitPropLoad(pnode->location, pnode->sxPid.sym, pnode->sxPid.pid, funcInfo);
        break;

    case knopComma:
    {
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        // The parser marks binary opnd pnodes as used, but value of the first opnd of a comma is not used.
        // Easier to correct this here than to check every binary op in the parser.
        ParseNode *pnode1 = pnode->sxBin.pnode1;
        pnode1->isUsed = false;
        if (pnode1->nop == knopComma)
        {TRACE_IT(39862);
            // Spot fix for giant comma expressions that send us into OOS if we use a simple recursive
            // algorithm. Instead of recursing on comma LHS's, iterate over them, pushing the RHS's onto
            // a stack. (This suggests a model for removing recursion from Emit altogether...)
            ArenaAllocator *alloc = byteCodeGenerator->GetAllocator();
            SList<ParseNode *> rhsStack(alloc);
            do
            {TRACE_IT(39863);
                rhsStack.Push(pnode1->sxBin.pnode2);
                pnode1 = pnode1->sxBin.pnode1;
                pnode1->isUsed = false;
            } while (pnode1->nop == knopComma);

            Emit(pnode1, byteCodeGenerator, funcInfo, false);
            if (funcInfo->IsTmpReg(pnode1->location))
            {TRACE_IT(39864);
                byteCodeGenerator->Writer()->Reg1(Js::OpCode::Unused, pnode1->location);
            }

            while (!rhsStack.Empty())
            {TRACE_IT(39865);
                ParseNode *pnodeRhs = rhsStack.Pop();
                pnodeRhs->isUsed = false;
                Emit(pnodeRhs, byteCodeGenerator, funcInfo, false);
                if (funcInfo->IsTmpReg(pnodeRhs->location))
                {TRACE_IT(39866);
                    byteCodeGenerator->Writer()->Reg1(Js::OpCode::Unused, pnodeRhs->location);
                }
                funcInfo->ReleaseLoc(pnodeRhs);
            }
        }
        else
        {
            Emit(pnode1, byteCodeGenerator, funcInfo, false);
            if (funcInfo->IsTmpReg(pnode1->location))
            {TRACE_IT(39867);
                byteCodeGenerator->Writer()->Reg1(Js::OpCode::Unused, pnode1->location);
            }
        }
        funcInfo->ReleaseLoc(pnode1);

        pnode->sxBin.pnode2->isUsed = pnode->isUsed || fReturnValue;
        Emit(pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, false);
        funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
        funcInfo->AcquireLoc(pnode);
        if (pnode->sxBin.pnode2->isUsed && pnode->location != pnode->sxBin.pnode2->location)
        {TRACE_IT(39868);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxBin.pnode2->location);
        }
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
    }
    break;

    // The binary logical ops && and || resolve to the value of the left-hand expression if its
    // boolean value short-circuits the operation, and to the value of the right-hand expression
    // otherwise. (In other words, the "truth" of the right-hand expression is never tested.)
    // PTNODE(knopLogOr      , "||"        ,None    ,Bin  ,fnopBin)
    case knopLogOr:
    {
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        Js::ByteCodeLabel doneLabel = byteCodeGenerator->Writer()->DefineLabel();
        // We use a single dest here for the whole generating boolean expr, because we were poorly
        // optimizing the previous version where we had a dest for each level
        funcInfo->AcquireLoc(pnode);
        EmitGeneratingBooleanExpression(pnode, doneLabel, true, doneLabel, true, pnode->location, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->MarkLabel(doneLabel);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
    }
    // PTNODE(knopLogAnd     , "&&"        ,None    ,Bin  ,fnopBin)
    case knopLogAnd:
    {
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        Js::ByteCodeLabel doneLabel = byteCodeGenerator->Writer()->DefineLabel();
        // We use a single dest here for the whole generating boolean expr, because we were poorly
        // optimizing the previous version where we had a dest for each level
        funcInfo->AcquireLoc(pnode);
        EmitGeneratingBooleanExpression(pnode, doneLabel, true, doneLabel, true, pnode->location, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->MarkLabel(doneLabel);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
    }
    // PTNODE(knopQmark      , "?"            ,None    ,Tri  ,fnopBin)
    case knopQmark:
    {TRACE_IT(39869);
        Js::ByteCodeLabel trueLabel = byteCodeGenerator->Writer()->DefineLabel();
        Js::ByteCodeLabel falseLabel = byteCodeGenerator->Writer()->DefineLabel();
        Js::ByteCodeLabel skipLabel = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(pnode->sxTri.pnode1, trueLabel, falseLabel, byteCodeGenerator, funcInfo);
        byteCodeGenerator->Writer()->MarkLabel(trueLabel);
        funcInfo->ReleaseLoc(pnode->sxTri.pnode1);

        // For boolean expressions that compute a result, we have to burn a register for the result
        // so that the back end can identify it cheaply as a single temp lifetime. Revisit this if we do
        // full-on renaming in the back end.
        funcInfo->AcquireLoc(pnode);

        Emit(pnode->sxTri.pnode2, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxTri.pnode2->location);
        funcInfo->ReleaseLoc(pnode->sxTri.pnode2);

        // Record the branch bytecode offset
        byteCodeGenerator->Writer()->RecordStatementAdjustment(Js::FunctionBody::SAT_FromCurrentToNext);

        byteCodeGenerator->Writer()->Br(skipLabel);

        byteCodeGenerator->Writer()->MarkLabel(falseLabel);
        Emit(pnode->sxTri.pnode3, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxTri.pnode3->location);
        funcInfo->ReleaseLoc(pnode->sxTri.pnode3);

        byteCodeGenerator->Writer()->MarkLabel(skipLabel);

        break;
    }

    case knopAsgAdd:
    case knopAsgSub:
    case knopAsgMul:
    case knopAsgDiv:
    case knopAsgExpo:
    case knopAsgMod:
    case knopAsgAnd:
    case knopAsgXor:
    case knopAsgOr:
    case knopAsgLsh:
    case knopAsgRsh:
    case knopAsgRs2:
        byteCodeGenerator->StartStatement(pnode);
        // Assign a register for the result only if the result is used or the LHS can't be assigned to
        // (i.e., is a constant).
        if (pnode->isUsed || fReturnValue || funcInfo->RegIsConst(pnode->sxBin.pnode1->location))
        {TRACE_IT(39870);
            // If the assign-op result is used, grab a register to hold it.
            funcInfo->AcquireLoc(pnode);

            // Grab a register for the initial value and load it.
            EmitBinaryReference(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, true);
            funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
            // Do the arithmetic, store the result, and release the l-value.
            byteCodeGenerator->Writer()->Reg3(nopToOp[pnode->nop], pnode->location, pnode->sxBin.pnode1->location,
                pnode->sxBin.pnode2->location);

            EmitAssignment(pnode, pnode->sxBin.pnode1, pnode->location, byteCodeGenerator, funcInfo);
        }
        else
        {TRACE_IT(39871);
            // Grab a register for the initial value and load it.
            EmitBinaryReference(pnode->sxBin.pnode1, pnode->sxBin.pnode2, byteCodeGenerator, funcInfo, true);
            funcInfo->ReleaseLoc(pnode->sxBin.pnode2);
            // Do the arithmetic, store the result, and release the l-value.
            byteCodeGenerator->Writer()->Reg3(nopToOp[pnode->nop], pnode->sxBin.pnode1->location, pnode->sxBin.pnode1->location,
                pnode->sxBin.pnode2->location);
            EmitAssignment(nullptr, pnode->sxBin.pnode1, pnode->sxBin.pnode1->location, byteCodeGenerator, funcInfo);
        }
        funcInfo->ReleaseLoad(pnode->sxBin.pnode1);

        byteCodeGenerator->EndStatement(pnode);
        break;

        // General nodes.
        // PTNODE(knopTempRef      , "temp ref"  ,None   ,Uni ,fnopUni)
    case knopTempRef:
        // TODO: check whether mov is necessary
        funcInfo->AcquireLoc(pnode);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxUni.pnode1->location);
        break;
        // PTNODE(knopTemp      , "temp"        ,None   ,None ,fnopLeaf)
    case knopTemp:
        // Emit initialization code
        if (pnode->sxVar.pnodeInit != nullptr)
        {TRACE_IT(39872);
            byteCodeGenerator->StartStatement(pnode);
            Emit(pnode->sxVar.pnodeInit, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, pnode->location, pnode->sxVar.pnodeInit->location);
            funcInfo->ReleaseLoc(pnode->sxVar.pnodeInit);
            byteCodeGenerator->EndStatement(pnode);
        }
        break;
        // PTNODE(knopVarDecl    , "varDcl"    ,None    ,Var  ,fnopNone)
    case knopVarDecl:
    case knopConstDecl:
    case knopLetDecl:
    {TRACE_IT(39873);
        // Emit initialization code
        ParseNodePtr initNode = pnode->sxVar.pnodeInit;
        AssertMsg(pnode->nop != knopConstDecl || initNode != nullptr, "knopConstDecl expected to have an initializer");

        if (initNode != nullptr || pnode->nop == knopLetDecl)
        {TRACE_IT(39874);
            Symbol *sym = pnode->sxVar.sym;
            Js::RegSlot rhsLocation;

            byteCodeGenerator->StartStatement(pnode);

            if (initNode != nullptr)
            {
                Emit(initNode, byteCodeGenerator, funcInfo, false);
                rhsLocation = initNode->location;

                if (initNode->nop == knopObject)
                {
                    TrackMemberNodesInObjectForIntConstants(byteCodeGenerator, initNode);
                }
                else if (initNode->nop == knopInt)
                {
                    TrackIntConstantsOnGlobalObject(byteCodeGenerator, sym);
                }
            }
            else
            {TRACE_IT(39875);
                Assert(pnode->nop == knopLetDecl);
                rhsLocation = funcInfo->AcquireTmpRegister();
                byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, rhsLocation);
            }

            if (pnode->nop != knopVarDecl)
            {TRACE_IT(39876);
                Assert(sym->GetDecl() == pnode || (sym->GetIsArguments() && !funcInfo->GetHasArguments()));
                sym->SetNeedDeclaration(false);
            }

            EmitAssignment(nullptr, pnode, rhsLocation, byteCodeGenerator, funcInfo);
            funcInfo->ReleaseTmpRegister(rhsLocation);

            byteCodeGenerator->EndStatement(pnode);
        }
        break;
    }
    // PTNODE(knopFncDecl    , "fncDcl"    ,None    ,Fnc  ,fnopLeaf)
    case knopFncDecl:
        // The "function declarations" were emitted in DefineFunctions()
        if (!pnode->sxFnc.IsDeclaration())
        {TRACE_IT(39877);
            byteCodeGenerator->DefineOneFunction(pnode, funcInfo, false);
        }
        break;
        // PTNODE(knopClassDecl, "class"    ,None    ,None ,fnopLeaf)
    case knopClassDecl:
    {TRACE_IT(39878);
        funcInfo->AcquireLoc(pnode);

        Assert(pnode->sxClass.pnodeConstructor);
        pnode->sxClass.pnodeConstructor->location = pnode->location;

        BeginEmitBlock(pnode->sxClass.pnodeBlock, byteCodeGenerator, funcInfo);

        // Extends
        if (pnode->sxClass.pnodeExtends)
        {TRACE_IT(39879);
            // We can't do StartStatement/EndStatement for pnodeExtends here because the load locations may differ between
            // defer and nondefer parse modes.
            Emit(pnode->sxClass.pnodeExtends, byteCodeGenerator, funcInfo, false);
        }

        // Constructor
        Emit(pnode->sxClass.pnodeConstructor, byteCodeGenerator, funcInfo, false);
        EmitComputedFunctionNameVar(bindPnode, pnode->sxClass.pnodeConstructor, byteCodeGenerator);
        if (pnode->sxClass.pnodeExtends)
        {TRACE_IT(39880);
            byteCodeGenerator->StartStatement(pnode->sxClass.pnodeExtends);
            byteCodeGenerator->Writer()->InitClass(pnode->location, pnode->sxClass.pnodeExtends->location);
            byteCodeGenerator->EndStatement(pnode->sxClass.pnodeExtends);
        }
        else
        {TRACE_IT(39881);
            byteCodeGenerator->Writer()->InitClass(pnode->location);
        }

        Js::RegSlot protoLoc = funcInfo->AcquireTmpRegister(); //register set if we have Instance Methods
        int cacheId = funcInfo->FindOrAddInlineCacheId(pnode->location, Js::PropertyIds::prototype, false, false);
        byteCodeGenerator->Writer()->PatchableProperty(Js::OpCode::LdFld, protoLoc, pnode->location, cacheId);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::SetHomeObj, pnode->location, protoLoc);

        // Static Methods
        EmitClassInitializers(pnode->sxClass.pnodeStaticMembers, pnode->location, byteCodeGenerator, funcInfo, pnode, /*isObjectEmpty*/ false);

        // Instance Methods
        EmitClassInitializers(pnode->sxClass.pnodeMembers, protoLoc, byteCodeGenerator, funcInfo, pnode, /*isObjectEmpty*/ true);
        funcInfo->ReleaseTmpRegister(protoLoc);

        // Emit name binding.
        if (pnode->sxClass.pnodeName)
        {TRACE_IT(39882);
            Symbol * sym = pnode->sxClass.pnodeName->sxVar.sym;
            sym->SetNeedDeclaration(false);
            byteCodeGenerator->EmitPropStore(pnode->location, sym, nullptr, funcInfo, false, true);
        }

        EndEmitBlock(pnode->sxClass.pnodeBlock, byteCodeGenerator, funcInfo);

        if (pnode->sxClass.pnodeExtends)
        {TRACE_IT(39883);
            funcInfo->ReleaseLoc(pnode->sxClass.pnodeExtends);
        }

        if (pnode->sxClass.pnodeDeclName)
        {TRACE_IT(39884);
            Symbol * sym = pnode->sxClass.pnodeDeclName->sxVar.sym;
            sym->SetNeedDeclaration(false);
            byteCodeGenerator->EmitPropStore(pnode->location, sym, nullptr, funcInfo, true, false);
        }

        if (pnode->sxClass.IsDefaultModuleExport())
        {TRACE_IT(39885);
            byteCodeGenerator->EmitAssignmentToDefaultModuleExport(pnode, funcInfo);
        }

        break;
    }
    case knopStrTemplate:
        STARTSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        EmitStringTemplate(pnode, byteCodeGenerator, funcInfo);
        ENDSTATEMENET_IFTOPLEVEL(isTopLevel, pnode);
        break;
    case knopEndCode:
        byteCodeGenerator->Writer()->RecordStatementAdjustment(Js::FunctionBody::SAT_All);

        // load undefined for the fallthrough case:
        if (!funcInfo->IsGlobalFunction())
        {TRACE_IT(39886);
            if (funcInfo->IsClassConstructor())
            {TRACE_IT(39887);
                // For class constructors, we need to explicitly load 'this' into the return register.
                byteCodeGenerator->EmitClassConstructorEndCode(funcInfo);
            }
            else
            {TRACE_IT(39888);
                // In the global function, implicit return values are copied to the return register, and if
                // necessary the return register is initialized at the top. Don't clobber the value here.
                byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, ByteCodeGenerator::ReturnRegister);
            }
        }

        // Label for non-fall-through return
        byteCodeGenerator->Writer()->MarkLabel(funcInfo->singleExit);

        if (funcInfo->GetHasCachedScope())
        {TRACE_IT(39889);
            byteCodeGenerator->Writer()->Empty(Js::OpCode::CommitScope);
        }
        byteCodeGenerator->StartStatement(pnode);
        byteCodeGenerator->Writer()->Empty(Js::OpCode::Ret);
        byteCodeGenerator->EndStatement(pnode);
        break;
        // PTNODE(knopDebugger   , "debugger"    ,None    ,None ,fnopNone)
    case knopDebugger:
        byteCodeGenerator->StartStatement(pnode);
        byteCodeGenerator->Writer()->Empty(Js::OpCode::Break);
        byteCodeGenerator->EndStatement(pnode);
        break;
        // PTNODE(knopFor        , "for"        ,None    ,For  ,fnopBreak|fnopContinue)
    case knopFor:
        if (pnode->sxFor.pnodeInverted != nullptr)
        {TRACE_IT(39890);
            byteCodeGenerator->EmitInvertedLoop(pnode, pnode->sxFor.pnodeInverted, funcInfo);
        }
        else
        {TRACE_IT(39891);
            BeginEmitBlock(pnode->sxFor.pnodeBlock, byteCodeGenerator, funcInfo);
            Emit(pnode->sxFor.pnodeInit, byteCodeGenerator, funcInfo, false);
            funcInfo->ReleaseLoc(pnode->sxFor.pnodeInit);
            if (byteCodeGenerator->IsES6ForLoopSemanticsEnabled())
            {TRACE_IT(39892);
                CloneEmitBlock(pnode->sxFor.pnodeBlock, byteCodeGenerator, funcInfo);
            }
            EmitLoop(pnode,
                pnode->sxFor.pnodeCond,
                pnode->sxFor.pnodeBody,
                pnode->sxFor.pnodeIncr,
                byteCodeGenerator,
                funcInfo,
                fReturnValue,
                FALSE,
                pnode->sxFor.pnodeBlock);
            EndEmitBlock(pnode->sxFor.pnodeBlock, byteCodeGenerator, funcInfo);
        }
        break;
        // PTNODE(knopIf         , "if"        ,None    ,If   ,fnopNone)
    case knopIf:
    {TRACE_IT(39893);
        byteCodeGenerator->StartStatement(pnode);

        Js::ByteCodeLabel trueLabel = byteCodeGenerator->Writer()->DefineLabel();
        Js::ByteCodeLabel falseLabel = byteCodeGenerator->Writer()->DefineLabel();
        EmitBooleanExpression(pnode->sxIf.pnodeCond, trueLabel, falseLabel, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxIf.pnodeCond);

        byteCodeGenerator->EndStatement(pnode);

        byteCodeGenerator->Writer()->MarkLabel(trueLabel);
        Emit(pnode->sxIf.pnodeTrue, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(pnode->sxIf.pnodeTrue);
        if (pnode->sxIf.pnodeFalse != nullptr)
        {TRACE_IT(39894);
            // has else clause
            Js::ByteCodeLabel skipLabel = byteCodeGenerator->Writer()->DefineLabel();

            // Record the branch bytecode offset
            byteCodeGenerator->Writer()->RecordStatementAdjustment(Js::FunctionBody::SAT_FromCurrentToNext);

            // then clause skips else clause
            byteCodeGenerator->Writer()->Br(skipLabel);
            // generate code for else clause
            byteCodeGenerator->Writer()->MarkLabel(falseLabel);
            Emit(pnode->sxIf.pnodeFalse, byteCodeGenerator, funcInfo, fReturnValue);
            funcInfo->ReleaseLoc(pnode->sxIf.pnodeFalse);
            byteCodeGenerator->Writer()->MarkLabel(skipLabel);
        }
        else
        {TRACE_IT(39895);
            byteCodeGenerator->Writer()->MarkLabel(falseLabel);
        }

        if (pnode->emitLabels)
        {TRACE_IT(39896);
            byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        }
        break;
    }
    case knopWhile:
        EmitLoop(pnode,
            pnode->sxWhile.pnodeCond,
            pnode->sxWhile.pnodeBody,
            nullptr,
            byteCodeGenerator,
            funcInfo,
            fReturnValue);
        break;
        // PTNODE(knopDoWhile    , "do-while"    ,None    ,While,fnopBreak|fnopContinue)
    case knopDoWhile:
        EmitLoop(pnode,
            pnode->sxWhile.pnodeCond,
            pnode->sxWhile.pnodeBody,
            nullptr,
            byteCodeGenerator,
            funcInfo,
            fReturnValue,
            true);
        break;
        // PTNODE(knopForIn      , "for in"    ,None    ,ForIn,fnopBreak|fnopContinue|fnopCleanup)
    case knopForIn:
        EmitForInOrForOf(pnode, byteCodeGenerator, funcInfo, fReturnValue);
        break;
    case knopForOf:
        EmitForInOrForOf(pnode, byteCodeGenerator, funcInfo, fReturnValue);
        break;
        // PTNODE(knopReturn     , "return"    ,None    ,Uni  ,fnopNone)
    case knopReturn:
        byteCodeGenerator->StartStatement(pnode);
        if (pnode->sxReturn.pnodeExpr != nullptr)
        {TRACE_IT(39897);
            if (pnode->sxReturn.pnodeExpr->location == Js::Constants::NoRegister)
            {TRACE_IT(39898);
                // No need to burn a register for the return value. If we need a temp, use R0 directly.
                pnode->sxReturn.pnodeExpr->location = ByteCodeGenerator::ReturnRegister;
            }
            Emit(pnode->sxReturn.pnodeExpr, byteCodeGenerator, funcInfo, fReturnValue);
            if (pnode->sxReturn.pnodeExpr->location != ByteCodeGenerator::ReturnRegister)
            {TRACE_IT(39899);
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, pnode->sxReturn.pnodeExpr->location);
            }
            funcInfo->GetParsedFunctionBody()->SetHasNoExplicitReturnValue(false);
        }
        else
        {TRACE_IT(39900);
            byteCodeGenerator->Writer()->Reg1(Js::OpCode::LdUndef, ByteCodeGenerator::ReturnRegister);
        }
        if (funcInfo->IsClassConstructor())
        {TRACE_IT(39901);
            // return expr; // becomes like below:
            //
            // if (IsObject(expr)) {
            //   return expr;
            // } else if (IsBaseClassConstructor) {
            //   return this;
            // } else if (!IsUndefined(expr)) {
            //   throw TypeError;
            // }

            Js::ByteCodeLabel returnExprLabel = byteCodeGenerator->Writer()->DefineLabel();
            byteCodeGenerator->Writer()->BrReg1(Js::OpCode::BrOnObject_A, returnExprLabel, ByteCodeGenerator::ReturnRegister);

            if (funcInfo->IsBaseClassConstructor())
            {TRACE_IT(39902);
                byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, funcInfo->thisPointerRegister);
            }
            else
            {TRACE_IT(39903);
                Js::ByteCodeLabel returnThisLabel = byteCodeGenerator->Writer()->DefineLabel();
                byteCodeGenerator->Writer()->BrReg2(Js::OpCode::BrSrEq_A, returnThisLabel, ByteCodeGenerator::ReturnRegister, funcInfo->undefinedConstantRegister);
                byteCodeGenerator->Writer()->W1(Js::OpCode::RuntimeTypeError, SCODE_CODE(JSERR_ClassDerivedConstructorInvalidReturnType));
                byteCodeGenerator->Writer()->MarkLabel(returnThisLabel);
                byteCodeGenerator->EmitClassConstructorEndCode(funcInfo);
            }

            byteCodeGenerator->Writer()->MarkLabel(returnExprLabel);
        }
        if (pnode->sxStmt.grfnop & fnopCleanup)
        {
            EmitJumpCleanup(pnode, nullptr, byteCodeGenerator, funcInfo);
        }

        byteCodeGenerator->Writer()->Br(funcInfo->singleExit);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopLabel:
        break;
        // PTNODE(knopBlock      , "{}"        ,None    ,Block,fnopNone)
    case knopBlock:
        if (pnode->sxBlock.pnodeStmt != nullptr)
        {
            EmitBlock(pnode, byteCodeGenerator, funcInfo, fReturnValue);
            if (pnode->emitLabels)
            {TRACE_IT(39904);
                byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
            }
        }
        break;
        // PTNODE(knopWith       , "with"        ,None    ,With ,fnopCleanup)
    case knopWith:
    {TRACE_IT(39905);
        Assert(pnode->sxWith.pnodeObj != nullptr);
        byteCodeGenerator->StartStatement(pnode);
        // Copy the with object to a temp register (the location assigned to pnode) so that if the with object
        // is overwritten in the body, the lookups are not affected.
        funcInfo->AcquireLoc(pnode);
        Emit(pnode->sxWith.pnodeObj, byteCodeGenerator, funcInfo, false);

        Js::RegSlot regVal = (byteCodeGenerator->GetScriptContext()->GetConfig()->IsES6UnscopablesEnabled()) ? funcInfo->AcquireTmpRegister() : pnode->location;
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Conv_Obj, regVal, pnode->sxWith.pnodeObj->location);
        if (byteCodeGenerator->GetScriptContext()->GetConfig()->IsES6UnscopablesEnabled())
        {TRACE_IT(39906);
            byteCodeGenerator->Writer()->Reg2(Js::OpCode::NewWithObject, pnode->location, regVal);
        }
        byteCodeGenerator->EndStatement(pnode);

#ifdef PERF_HINT
        if (PHASE_TRACE1(Js::PerfHintPhase))
        {TRACE_IT(39907);
            WritePerfHint(PerfHints::HasWithBlock, funcInfo->byteCodeFunction->GetFunctionBody(), byteCodeGenerator->Writer()->GetCurrentOffset() - 1);
        }
#endif
        if (pnode->sxWith.pnodeBody != nullptr)
        {TRACE_IT(39908);
            Scope *scope = pnode->sxWith.scope;
            scope->SetLocation(pnode->location);
            byteCodeGenerator->PushScope(scope);

            Js::DebuggerScope *debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnode, Js::DiagExtraScopesType::DiagWithScope, regVal);

            if (byteCodeGenerator->ShouldTrackDebuggerMetadata())
            {TRACE_IT(39909);
                byteCodeGenerator->Writer()->AddPropertyToDebuggerScope(debuggerScope, regVal, Js::Constants::NoProperty, /*shouldConsumeRegister*/ true, Js::DebuggerScopePropertyFlags_WithObject);
            }

            Emit(pnode->sxWith.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
            funcInfo->ReleaseLoc(pnode->sxWith.pnodeBody);
            byteCodeGenerator->PopScope();

            byteCodeGenerator->RecordEndScopeObject(pnode);
        }
        if (pnode->emitLabels)
        {TRACE_IT(39910);
            byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        }
        if (byteCodeGenerator->GetScriptContext()->GetConfig()->IsES6UnscopablesEnabled())
        {TRACE_IT(39911);
            funcInfo->ReleaseTmpRegister(regVal);
        }
        funcInfo->ReleaseLoc(pnode->sxWith.pnodeObj);
        break;
    }
    // PTNODE(knopBreak      , "break"        ,None    ,Jump ,fnopNone)
    case knopBreak:
        Assert(pnode->sxJump.pnodeTarget->emitLabels);
        byteCodeGenerator->StartStatement(pnode);
        if (pnode->sxStmt.grfnop & fnopCleanup)
        {
            EmitJumpCleanup(pnode, pnode->sxJump.pnodeTarget, byteCodeGenerator, funcInfo);
        }
        byteCodeGenerator->Writer()->Br(pnode->sxJump.pnodeTarget->sxStmt.breakLabel);
        if (pnode->emitLabels)
        {TRACE_IT(39912);
            byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        }
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopContinue:
        Assert(pnode->sxJump.pnodeTarget->emitLabels);
        byteCodeGenerator->StartStatement(pnode);
        if (pnode->sxStmt.grfnop & fnopCleanup)
        {
            EmitJumpCleanup(pnode, pnode->sxJump.pnodeTarget, byteCodeGenerator, funcInfo);
        }
        byteCodeGenerator->Writer()->Br(pnode->sxJump.pnodeTarget->sxStmt.continueLabel);
        byteCodeGenerator->EndStatement(pnode);
        break;
        // PTNODE(knopContinue   , "continue"    ,None    ,Jump ,fnopNone)
    case knopSwitch:
    {TRACE_IT(39913);
        BOOL fHasDefault = false;
        Assert(pnode->sxSwitch.pnodeVal != nullptr);
        byteCodeGenerator->StartStatement(pnode);
        Emit(pnode->sxSwitch.pnodeVal, byteCodeGenerator, funcInfo, false);

        Js::RegSlot regVal = funcInfo->AcquireTmpRegister();

        byteCodeGenerator->Writer()->Reg2(Js::OpCode::BeginSwitch, regVal, pnode->sxSwitch.pnodeVal->location);

        BeginEmitBlock(pnode->sxSwitch.pnodeBlock, byteCodeGenerator, funcInfo);

        byteCodeGenerator->EndStatement(pnode);

        // TODO: if all cases are compile-time constants, emit a switch statement in the byte
        // code so the BE can optimize it.

        ParseNode *pnodeCase;
        for (pnodeCase = pnode->sxSwitch.pnodeCases; pnodeCase; pnodeCase = pnodeCase->sxCase.pnodeNext)
        {TRACE_IT(39914);
            // Jump to the first case body if this one doesn't match. Make sure any side-effects of the case
            // expression take place regardless.
            pnodeCase->sxCase.labelCase = byteCodeGenerator->Writer()->DefineLabel();
            if (pnodeCase == pnode->sxSwitch.pnodeDefault)
            {TRACE_IT(39915);
                fHasDefault = true;
                continue;
            }
            Emit(pnodeCase->sxCase.pnodeExpr, byteCodeGenerator, funcInfo, false);
            byteCodeGenerator->Writer()->BrReg2(
                Js::OpCode::Case, pnodeCase->sxCase.labelCase, regVal, pnodeCase->sxCase.pnodeExpr->location);
            funcInfo->ReleaseLoc(pnodeCase->sxCase.pnodeExpr);
        }

        // No explicit case value matches. Jump to the default arm (if any) or break out altogether.
        if (fHasDefault)
        {TRACE_IT(39916);
            byteCodeGenerator->Writer()->Br(Js::OpCode::EndSwitch, pnode->sxSwitch.pnodeDefault->sxCase.labelCase);
        }
        else
        {TRACE_IT(39917);
            if (!pnode->emitLabels)
            {TRACE_IT(39918);
                pnode->sxStmt.breakLabel = byteCodeGenerator->Writer()->DefineLabel();
            }
            byteCodeGenerator->Writer()->Br(Js::OpCode::EndSwitch, pnode->sxStmt.breakLabel);
        }
        // Now emit the case arms to which we jump on matching a case value.
        for (pnodeCase = pnode->sxSwitch.pnodeCases; pnodeCase; pnodeCase = pnodeCase->sxCase.pnodeNext)
        {TRACE_IT(39919);
            byteCodeGenerator->Writer()->MarkLabel(pnodeCase->sxCase.labelCase);
            Emit(pnodeCase->sxCase.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
            funcInfo->ReleaseLoc(pnodeCase->sxCase.pnodeBody);
        }

        EndEmitBlock(pnode->sxSwitch.pnodeBlock, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseTmpRegister(regVal);
        funcInfo->ReleaseLoc(pnode->sxSwitch.pnodeVal);

        if (!fHasDefault || pnode->emitLabels)
        {TRACE_IT(39920);
            byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        }
        break;
    }

    case knopTryCatch:
    {TRACE_IT(39921);
        Js::ByteCodeLabel catchLabel = (Js::ByteCodeLabel) - 1;

        ParseNode *pnodeTry = pnode->sxTryCatch.pnodeTry;
        Assert(pnodeTry);
        ParseNode *pnodeCatch = pnode->sxTryCatch.pnodeCatch;
        Assert(pnodeCatch);

        catchLabel = byteCodeGenerator->Writer()->DefineLabel();

        // Note: try uses OpCode::Leave which causes a return to parent interpreter thunk,
        // same for catch block. Thus record cross interpreter frame entry/exit records for them.
        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ true);

        byteCodeGenerator->Writer()->Br(Js::OpCode::TryCatch, catchLabel);

        ByteCodeGenerator::TryScopeRecord tryRecForTry(Js::OpCode::TryCatch, catchLabel);
        if (funcInfo->byteCodeFunction->IsCoroutine())
        {TRACE_IT(39922);
            byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForTry);
        }

        Emit(pnodeTry->sxTry.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(pnodeTry->sxTry.pnodeBody);

        if (funcInfo->byteCodeFunction->IsCoroutine())
        {TRACE_IT(39923);
            byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
        }

        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(/* isEnterBlock = */ false);

        byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
        byteCodeGenerator->Writer()->Br(pnode->sxStmt.breakLabel);
        byteCodeGenerator->Writer()->MarkLabel(catchLabel);
        Assert(pnodeCatch->sxCatch.pnodeParam);
        ParseNode *pnodeObj = pnodeCatch->sxCatch.pnodeParam;
        Js::RegSlot location;

        bool acquiredTempLocation = false;

        Js::DebuggerScope *debuggerScope = nullptr;
        Js::DebuggerScopePropertyFlags debuggerPropertyFlags = Js::DebuggerScopePropertyFlags_CatchObject;

        bool isPattern = pnodeObj->nop == knopParamPattern;

        if (isPattern)
        {TRACE_IT(39924);
            location = pnodeObj->sxParamPattern.location;
        }
        else
        {TRACE_IT(39925);
            location = pnodeObj->sxPid.sym->GetLocation();
        }

        if (location == Js::Constants::NoRegister)
        {TRACE_IT(39926);
            location = funcInfo->AcquireLoc(pnodeObj);
            acquiredTempLocation = true;
        }
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::Catch, location);

        Scope *scope = pnodeCatch->sxCatch.scope;
        byteCodeGenerator->PushScope(scope);

        if (scope->GetMustInstantiate())
        {TRACE_IT(39927);
            Assert(scope->GetLocation() == Js::Constants::NoRegister);
            if (scope->GetIsObject())
            {TRACE_IT(39928);
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnode, Js::DiagCatchScopeInObject, funcInfo->InnerScopeToRegSlot(scope));
                byteCodeGenerator->Writer()->Unsigned1(Js::OpCode::NewPseudoScope, scope->GetInnerScopeIndex());
            }
            else
            {TRACE_IT(39929);

                int index = Js::DebuggerScope::InvalidScopeIndex;
                debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnode, Js::DiagCatchScopeInSlot, funcInfo->InnerScopeToRegSlot(scope), &index);
                byteCodeGenerator->Writer()->Num3(Js::OpCode::NewInnerScopeSlots, scope->GetInnerScopeIndex(), scope->GetScopeSlotCount() + Js::ScopeSlots::FirstSlotIndex, index);
            }
        }
        else
        {TRACE_IT(39930);
            debuggerScope = byteCodeGenerator->RecordStartScopeObject(pnode, Js::DiagCatchScopeDirect, location);
        }

        auto ParamTrackAndInitialization = [&](Symbol *sym, bool initializeParam, Js::RegSlot location)
        {TRACE_IT(39931);
            if (sym->IsInSlot(funcInfo))
            {TRACE_IT(39932);
                Assert(scope->GetMustInstantiate());
                if (scope->GetIsObject())
                {TRACE_IT(39933);
                    Js::OpCode op = (sym->GetDecl()->nop == knopLetDecl) ? Js::OpCode::InitUndeclLetFld :
                        byteCodeGenerator->GetInitFldOp(scope, scope->GetLocation(), funcInfo, false);

                    Js::PropertyId propertyId = sym->EnsurePosition(byteCodeGenerator);
                    uint cacheId = funcInfo->FindOrAddInlineCacheId(funcInfo->InnerScopeToRegSlot(scope), propertyId, false, true);
                    byteCodeGenerator->Writer()->ElementPIndexed(op, location, scope->GetInnerScopeIndex(), cacheId);

                    byteCodeGenerator->TrackActivationObjectPropertyForDebugger(debuggerScope, sym, debuggerPropertyFlags);
                }
                else
                {TRACE_IT(39934);
                    byteCodeGenerator->TrackSlotArrayPropertyForDebugger(debuggerScope, sym, sym->EnsurePosition(byteCodeGenerator), debuggerPropertyFlags);
                    if (initializeParam)
                    {TRACE_IT(39935);
                        byteCodeGenerator->EmitLocalPropInit(location, sym, funcInfo);
                    }
                    else
                    {TRACE_IT(39936);
                        Js::RegSlot tmpReg = funcInfo->AcquireTmpRegister();
                        byteCodeGenerator->Writer()->Reg1(Js::OpCode::InitUndecl, tmpReg);
                        byteCodeGenerator->EmitLocalPropInit(tmpReg, sym, funcInfo);
                        funcInfo->ReleaseTmpRegister(tmpReg);
                    }
                }
            }
            else
            {TRACE_IT(39937);
                byteCodeGenerator->TrackRegisterPropertyForDebugger(debuggerScope, sym, funcInfo, debuggerPropertyFlags);
                if (initializeParam)
                {TRACE_IT(39938);
                    byteCodeGenerator->EmitLocalPropInit(location, sym, funcInfo);
                }
                else
                {TRACE_IT(39939);
                    byteCodeGenerator->Writer()->Reg1(Js::OpCode::InitUndecl, location);
                }
            }
        };

        if (isPattern)
        {TRACE_IT(39940);
            Parser::MapBindIdentifier(pnodeObj->sxParamPattern.pnode1, [&](ParseNodePtr item)
            {
                Js::RegSlot itemLocation = item->sxVar.sym->GetLocation();
                if (itemLocation == Js::Constants::NoRegister)
                {TRACE_IT(39941);
                    // The var has no assigned register, meaning it's captured, so we have no reg to write to.
                    // Emit the designated return reg in the byte code to avoid asserting on bad register.
                    itemLocation = ByteCodeGenerator::ReturnRegister;
                }
                ParamTrackAndInitialization(item->sxVar.sym, false /*initializeParam*/, itemLocation);
            });
            byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

            // Now emitting bytecode for destructuring pattern
            byteCodeGenerator->StartStatement(pnodeCatch);
            ParseNodePtr pnode1 = pnodeObj->sxParamPattern.pnode1;
            Assert(pnode1->IsPattern());

            ByteCodeGenerator::TryScopeRecord tryRecForCatch(Js::OpCode::ResumeCatch, catchLabel);
            if (funcInfo->byteCodeFunction->IsCoroutine())
            {TRACE_IT(39942);
                byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForCatch);
            }

            EmitAssignment(nullptr, pnode1, location, byteCodeGenerator, funcInfo);
            byteCodeGenerator->EndStatement(pnodeCatch);
        }
        else
        {TRACE_IT(39943);
            ParamTrackAndInitialization(pnodeObj->sxPid.sym, true /*initializeParam*/, location);
            if (scope->GetMustInstantiate())
            {TRACE_IT(39944);
                pnodeObj->sxPid.sym->SetIsGlobalCatch(true);
            }
            byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

            // Allow a debugger to stop on the 'catch (e)'
            byteCodeGenerator->StartStatement(pnodeCatch);
            byteCodeGenerator->Writer()->Empty(Js::OpCode::Nop);
            byteCodeGenerator->EndStatement(pnodeCatch);

            ByteCodeGenerator::TryScopeRecord tryRecForCatch(Js::OpCode::ResumeCatch, catchLabel);
            if (funcInfo->byteCodeFunction->IsCoroutine())
            {TRACE_IT(39945);
                byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForCatch);
            }
        }

        Emit(pnodeCatch->sxCatch.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);

        if (funcInfo->byteCodeFunction->IsCoroutine())
        {TRACE_IT(39946);
            byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
        }

        byteCodeGenerator->PopScope();

        byteCodeGenerator->RecordEndScopeObject(pnode);

        funcInfo->ReleaseLoc(pnodeCatch->sxCatch.pnodeBody);

        if (acquiredTempLocation)
        {TRACE_IT(39947);
            funcInfo->ReleaseLoc(pnodeObj);
        }

        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(false);

        byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
        byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        break;
    }

    case knopTryFinally:
    {TRACE_IT(39948);
        Js::ByteCodeLabel finallyLabel = (Js::ByteCodeLabel) - 1;

        ParseNode *pnodeTry = pnode->sxTryFinally.pnodeTry;
        Assert(pnodeTry);
        ParseNode *pnodeFinally = pnode->sxTryFinally.pnodeFinally;
        Assert(pnodeFinally);

        // If we yield from the finally block after an exception, we have to store the exception object for the future next call.
        // When we yield from the Try-Finally the offset to the end of the Try block is needed for the branch instruction.
        Js::RegSlot regException = Js::Constants::NoRegister;
        Js::RegSlot regOffset = Js::Constants::NoRegister;

        finallyLabel = byteCodeGenerator->Writer()->DefineLabel();
        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

        // [CONSIDER][aneeshd] Ideally the TryFinallyWithYield opcode needs to be used only if there is a yield expression.
        // For now, if the function is generator we are using the TryFinallyWithYield.
        ByteCodeGenerator::TryScopeRecord tryRecForTry(Js::OpCode::TryFinallyWithYield, finallyLabel);
        if (funcInfo->byteCodeFunction->IsCoroutine())
        {TRACE_IT(39949);
            regException = funcInfo->AcquireTmpRegister();
            regOffset = funcInfo->AcquireTmpRegister();
            byteCodeGenerator->Writer()->BrReg2(Js::OpCode::TryFinallyWithYield, finallyLabel, regException, regOffset);
            tryRecForTry.reg1 = regException;
            tryRecForTry.reg2 = regOffset;
            byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForTry);
        }
        else
        {TRACE_IT(39950);
            byteCodeGenerator->Writer()->Br(Js::OpCode::TryFinally, finallyLabel);
        }

        // Increasing the stack as we will be storing the additional values when we enter try..finally.
        funcInfo->StartRecordingOutArgs(1);

        Emit(pnodeTry->sxTry.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(pnodeTry->sxTry.pnodeBody);

        if (funcInfo->byteCodeFunction->IsCoroutine())
        {TRACE_IT(39951);
            byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
        }

        byteCodeGenerator->Writer()->Empty(Js::OpCode::Leave);
        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(false);

        // Note: although we don't use OpCode::Leave for finally block,
        // OpCode::LeaveNull causes a return to parent interpreter thunk.
        // This has to be on offset prior to offset of 1st statement of finally.
        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(true);

        byteCodeGenerator->Writer()->Br(pnode->sxStmt.breakLabel);
        byteCodeGenerator->Writer()->MarkLabel(finallyLabel);

        ByteCodeGenerator::TryScopeRecord tryRecForFinally(Js::OpCode::ResumeFinally, finallyLabel, regException, regOffset);
        if (funcInfo->byteCodeFunction->IsCoroutine())
        {TRACE_IT(39952);
            byteCodeGenerator->tryScopeRecordsList.LinkToEnd(&tryRecForFinally);
        }

        Emit(pnodeFinally->sxFinally.pnodeBody, byteCodeGenerator, funcInfo, fReturnValue);
        funcInfo->ReleaseLoc(pnodeFinally->sxFinally.pnodeBody);

        if (funcInfo->byteCodeFunction->IsCoroutine())
        {TRACE_IT(39953);
            byteCodeGenerator->tryScopeRecordsList.UnlinkFromEnd();
            funcInfo->ReleaseTmpRegister(regOffset);
            funcInfo->ReleaseTmpRegister(regException);
        }

        funcInfo->EndRecordingOutArgs(1);

        byteCodeGenerator->Writer()->RecordCrossFrameEntryExitRecord(false);

        byteCodeGenerator->Writer()->Empty(Js::OpCode::LeaveNull);

        byteCodeGenerator->Writer()->MarkLabel(pnode->sxStmt.breakLabel);
        break;
    }
    case knopThrow:
        byteCodeGenerator->StartStatement(pnode);
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->Writer()->Reg1(Js::OpCode::Throw, pnode->sxUni.pnode1->location);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopYieldLeaf:
        byteCodeGenerator->StartStatement(pnode);
        funcInfo->AcquireLoc(pnode);
        EmitYield(funcInfo->undefinedConstantRegister, pnode->location, byteCodeGenerator, funcInfo);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopAwait:
    case knopYield:
        byteCodeGenerator->StartStatement(pnode);
        funcInfo->AcquireLoc(pnode);
        Emit(pnode->sxUni.pnode1, byteCodeGenerator, funcInfo, false);
        EmitYield(pnode->sxUni.pnode1->location, pnode->location, byteCodeGenerator, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxUni.pnode1);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopYieldStar:
        byteCodeGenerator->StartStatement(pnode);
        EmitYieldStar(pnode, byteCodeGenerator, funcInfo);
        byteCodeGenerator->EndStatement(pnode);
        break;
    case knopExportDefault:
        Emit(pnode->sxExportDefault.pnodeExpr, byteCodeGenerator, funcInfo, false);
        byteCodeGenerator->EmitAssignmentToDefaultModuleExport(pnode->sxExportDefault.pnodeExpr, funcInfo);
        funcInfo->ReleaseLoc(pnode->sxExportDefault.pnodeExpr);
        pnode = pnode->sxExportDefault.pnodeExpr;
        break;
    default:
        AssertMsg(0, "emit unhandled pnode op");
        break;
    }

    if (fReturnValue && IsExpressionStatement(pnode, byteCodeGenerator->GetScriptContext()))
    {TRACE_IT(39954);
        // If this statement may produce the global function's return value, copy its result to the return register.
        // fReturnValue implies global function, which implies that "return" is a parse error.
        Assert(funcInfo->IsGlobalFunction());
        Assert(pnode->nop != knopReturn);
        byteCodeGenerator->Writer()->Reg2(Js::OpCode::Ld_A, ByteCodeGenerator::ReturnRegister, pnode->location);
    }
}
