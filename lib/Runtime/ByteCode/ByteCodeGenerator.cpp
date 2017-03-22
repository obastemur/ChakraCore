//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeByteCodePch.h"
#include "FormalsUtil.h"
#include "Library/StackScriptFunction.h"

void PreVisitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator);
void PostVisitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator);

bool IsCallOfConstants(ParseNode *pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 12\n");
    return pnode->sxCall.callOfConstants && pnode->sxCall.argCount > ByteCodeGenerator::MinArgumentsForCallOptimization;
}

template <class PrefixFn, class PostfixFn>
void Visit(ParseNode *pnode, ByteCodeGenerator* byteCodeGenerator, PrefixFn prefix, PostfixFn postfix, ParseNode * pnodeParent = nullptr);

//the only point of this type (as opposed to using a lambda) is to provide a named type in code coverage
template <typename TContext> class ParseNodeVisitor
{
    TContext* m_context;
    void(*m_fn)(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, TContext* context);
public:

    ParseNodeVisitor(TContext* ctx, void(*prefixParam)(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, TContext* context)) :
        m_context(ctx), m_fn(prefixParam)
    {LOGMEIN("ByteCodeGenerator.cpp] 28\n");
    }

    void operator () (ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator)
    {LOGMEIN("ByteCodeGenerator.cpp] 32\n");
        if (m_fn)
        {
            m_fn(pnode, byteCodeGenerator, m_context);
        }
    }
};

template<class TContext>
void VisitIndirect(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, TContext* context,
    void (*prefix)(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, TContext* context),
    void (*postfix)(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, TContext* context))
{LOGMEIN("ByteCodeGenerator.cpp] 44\n");
    ParseNodeVisitor<TContext> prefixHelper(context, prefix);
    ParseNodeVisitor<TContext> postfixHelper(context, postfix);

    Visit(pnode, byteCodeGenerator, prefixHelper, postfixHelper, nullptr);
}

template <class PrefixFn, class PostfixFn>
void VisitList(ParseNode *pnode, ByteCodeGenerator* byteCodeGenerator, PrefixFn prefix, PostfixFn postfix)
{LOGMEIN("ByteCodeGenerator.cpp] 53\n");
    Assert(pnode != nullptr);
    Assert(pnode->nop == knopList);

    do
    {LOGMEIN("ByteCodeGenerator.cpp] 58\n");
        ParseNode * pnode1 = pnode->sxBin.pnode1;
        Visit(pnode1, byteCodeGenerator, prefix, postfix);
        pnode = pnode->sxBin.pnode2;
    }
    while (pnode->nop == knopList);
    Visit(pnode, byteCodeGenerator, prefix, postfix);
}

template <class PrefixFn, class PostfixFn>
void VisitWithStmt(ParseNode *pnode, Js::RegSlot loc, ByteCodeGenerator* byteCodeGenerator, PrefixFn prefix, PostfixFn postfix, ParseNode *pnodeParent)
{LOGMEIN("ByteCodeGenerator.cpp] 69\n");
    // Note the fact that we're visiting the body of a with statement. This allows us to optimize register assignment
    // in the normal case of calls not requiring that their "this" objects be found dynamically.
    Scope *scope = pnode->sxWith.scope;

    byteCodeGenerator->PushScope(scope);
    Visit(pnode->sxWith.pnodeBody, byteCodeGenerator, prefix, postfix, pnodeParent);

    scope->SetIsObject();
    scope->SetMustInstantiate(true);

    byteCodeGenerator->PopScope();
}

bool BlockHasOwnScope(ParseNode* pnodeBlock, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 84\n");
    Assert(pnodeBlock->nop == knopBlock);
    return pnodeBlock->sxBlock.scope != nullptr &&
        (!(pnodeBlock->grfpn & fpnSyntheticNode) ||
            (pnodeBlock->sxBlock.blockType == PnodeBlockType::Global && byteCodeGenerator->IsEvalWithNoParentScopeInfo()));
}

void BeginVisitBlock(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{
    if (BlockHasOwnScope(pnode, byteCodeGenerator))
    {LOGMEIN("ByteCodeGenerator.cpp] 94\n");
        Scope *scope = pnode->sxBlock.scope;
        FuncInfo *func = scope->GetFunc();

        if (scope->IsInnerScope())
        {LOGMEIN("ByteCodeGenerator.cpp] 99\n");
            // Give this scope an index so its slots can be accessed via the index in the byte code,
            // not a register.
            scope->SetInnerScopeIndex(func->AcquireInnerScopeIndex());
        }

        byteCodeGenerator->PushBlock(pnode);
        byteCodeGenerator->PushScope(pnode->sxBlock.scope);
    }
}

void EndVisitBlock(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{
    if (BlockHasOwnScope(pnode, byteCodeGenerator))
    {LOGMEIN("ByteCodeGenerator.cpp] 113\n");
        Scope *scope = pnode->sxBlock.scope;
        FuncInfo *func = scope->GetFunc();

        if (!byteCodeGenerator->IsInDebugMode() &&
            scope->HasInnerScopeIndex())
        {LOGMEIN("ByteCodeGenerator.cpp] 119\n");
            // In debug mode, don't release the current index, as we're giving each scope a unique index, regardless
            // of nesting.
            Assert(scope->GetInnerScopeIndex() == func->CurrentInnerScopeIndex());
            func->ReleaseInnerScopeIndex();
        }

        Assert(byteCodeGenerator->GetCurrentScope() == scope);
        byteCodeGenerator->PopScope();
        byteCodeGenerator->PopBlock();
    }
}

void BeginVisitCatch(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 133\n");
    Scope *scope = pnode->sxCatch.scope;
    FuncInfo *func = scope->GetFunc();

    if (func->GetCallsEval() || func->GetChildCallsEval() ||
        (byteCodeGenerator->GetFlags() & (fscrEval | fscrImplicitThis | fscrImplicitParents)))
    {LOGMEIN("ByteCodeGenerator.cpp] 139\n");
        scope->SetIsObject();
    }

    // Give this scope an index so its slots can be accessed via the index in the byte code,
    // not a register.
    scope->SetInnerScopeIndex(func->AcquireInnerScopeIndex());

    byteCodeGenerator->PushScope(pnode->sxCatch.scope);
}

void EndVisitCatch(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 151\n");
    Scope *scope = pnode->sxCatch.scope;

    if (scope->HasInnerScopeIndex() && !byteCodeGenerator->IsInDebugMode())
    {LOGMEIN("ByteCodeGenerator.cpp] 155\n");
        // In debug mode, don't release the current index, as we're giving each scope a unique index,
        // regardless of nesting.
        FuncInfo *func = scope->GetFunc();

        Assert(scope->GetInnerScopeIndex() == func->CurrentInnerScopeIndex());
        func->ReleaseInnerScopeIndex();
    }

    byteCodeGenerator->PopScope();
}

bool CreateNativeArrays(ByteCodeGenerator *byteCodeGenerator, FuncInfo *funcInfo)
{LOGMEIN("ByteCodeGenerator.cpp] 168\n");
#if ENABLE_PROFILE_INFO
    Js::FunctionBody *functionBody = funcInfo ? funcInfo->GetParsedFunctionBody() : nullptr;

    return
        !PHASE_OFF_OPTFUNC(Js::NativeArrayPhase, functionBody) &&
        !byteCodeGenerator->IsInDebugMode() &&
        (
            functionBody
                ? Js::DynamicProfileInfo::IsEnabled(Js::NativeArrayPhase, functionBody)
                : Js::DynamicProfileInfo::IsEnabledForAtLeastOneFunction(
                    Js::NativeArrayPhase,
                    byteCodeGenerator->GetScriptContext())
        );
#else
    return false;
#endif
}

bool EmitAsConstantArray(ParseNode *pnodeArr, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 188\n");
    Assert(pnodeArr && pnodeArr->nop == knopArray);

    // TODO: We shouldn't have to handle an empty funcinfo stack here, but there seem to be issues
    // with the stack involved nested deferral. Remove this null check when those are resolved.
    if (CreateNativeArrays(byteCodeGenerator, byteCodeGenerator->TopFuncInfo()))
    {LOGMEIN("ByteCodeGenerator.cpp] 194\n");
        return pnodeArr->sxArrLit.arrayOfNumbers;
    }

    return pnodeArr->sxArrLit.arrayOfTaggedInts && pnodeArr->sxArrLit.count > 1;
}

void PropagateFlags(ParseNode *pnodeChild, ParseNode *pnodeParent);

template<class PrefixFn, class PostfixFn>
void Visit(ParseNode *pnode, ByteCodeGenerator* byteCodeGenerator, PrefixFn prefix, PostfixFn postfix, ParseNode *pnodeParent)
{LOGMEIN("ByteCodeGenerator.cpp] 205\n");
    if (pnode == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 207\n");
        return;
    }

    ThreadContext::ProbeCurrentStackNoDispose(Js::Constants::MinStackByteCodeVisitor, byteCodeGenerator->GetScriptContext());

    prefix(pnode, byteCodeGenerator);
    switch (pnode->nop)
    {LOGMEIN("ByteCodeGenerator.cpp] 215\n");
    default:
    {
        uint flags = ParseNode::Grfnop(pnode->nop);
        if (flags&fnopUni)
        {LOGMEIN("ByteCodeGenerator.cpp] 220\n");
            Visit(pnode->sxUni.pnode1, byteCodeGenerator, prefix, postfix);
        }
        else if (flags&fnopBin)
        {LOGMEIN("ByteCodeGenerator.cpp] 224\n");
            Visit(pnode->sxBin.pnode1, byteCodeGenerator, prefix, postfix);
            Visit(pnode->sxBin.pnode2, byteCodeGenerator, prefix, postfix);
        }

        break;
    }

    case knopParamPattern:
        Visit(pnode->sxParamPattern.pnode1, byteCodeGenerator, prefix, postfix);
        break;

    case knopArrayPattern:
        if (!byteCodeGenerator->InDestructuredPattern())
        {LOGMEIN("ByteCodeGenerator.cpp] 238\n");
            byteCodeGenerator->SetInDestructuredPattern(true);
            Visit(pnode->sxUni.pnode1, byteCodeGenerator, prefix, postfix);
            byteCodeGenerator->SetInDestructuredPattern(false);
        }
        else
        {
            Visit(pnode->sxUni.pnode1, byteCodeGenerator, prefix, postfix);
        }
        break;

    case knopCall:
        Visit(pnode->sxCall.pnodeTarget, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxCall.pnodeArgs, byteCodeGenerator, prefix, postfix);
        break;

    case knopNew:
    {LOGMEIN("ByteCodeGenerator.cpp] 255\n");
        Visit(pnode->sxCall.pnodeTarget, byteCodeGenerator, prefix, postfix);
        if (!IsCallOfConstants(pnode))
        {LOGMEIN("ByteCodeGenerator.cpp] 258\n");
            Visit(pnode->sxCall.pnodeArgs, byteCodeGenerator, prefix, postfix);
        }
        break;
    }

    case knopQmark:
        Visit(pnode->sxTri.pnode1, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxTri.pnode2, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxTri.pnode3, byteCodeGenerator, prefix, postfix);
        break;
    case knopList:
        VisitList(pnode, byteCodeGenerator, prefix, postfix);
        break;
    // PTNODE(knopVarDecl    , "varDcl"    ,None    ,Var  ,fnopNone)
    case knopVarDecl:
    case knopConstDecl:
    case knopLetDecl:
        if (pnode->sxVar.pnodeInit != nullptr)
            Visit(pnode->sxVar.pnodeInit, byteCodeGenerator, prefix, postfix);
        break;
    // PTNODE(knopFncDecl    , "fncDcl"    ,None    ,Fnc  ,fnopLeaf)
    case knopFncDecl:
    {LOGMEIN("ByteCodeGenerator.cpp] 281\n");
        // Inner function declarations are visited before anything else in the scope.
        // (See VisitFunctionsInScope.)
        break;
    }
    case knopClassDecl:
    {LOGMEIN("ByteCodeGenerator.cpp] 287\n");
        Visit(pnode->sxClass.pnodeDeclName, byteCodeGenerator, prefix, postfix);
        // Now visit the class name and methods.
        BeginVisitBlock(pnode->sxClass.pnodeBlock, byteCodeGenerator);
        // The extends clause is bound to the scope which contains the class name
        // (and the class name identifier is in a TDZ when the extends clause is evaluated).
        // See ES 2017 14.5.13 Runtime Semantics: ClassDefinitionEvaluation.
        Visit(pnode->sxClass.pnodeExtends, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxClass.pnodeName, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxClass.pnodeStaticMembers, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxClass.pnodeConstructor, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxClass.pnodeMembers, byteCodeGenerator, prefix, postfix);
        EndVisitBlock(pnode->sxClass.pnodeBlock, byteCodeGenerator);
        break;
    }
    case knopStrTemplate:
    {LOGMEIN("ByteCodeGenerator.cpp] 303\n");
        // Visit the string node lists only if we do not have a tagged template.
        // We never need to visit the raw strings as they are not used in non-tagged templates and
        // tagged templates will register them as part of the callsite constant object.
        if (!pnode->sxStrTemplate.isTaggedTemplate)
        {LOGMEIN("ByteCodeGenerator.cpp] 308\n");
            Visit(pnode->sxStrTemplate.pnodeStringLiterals, byteCodeGenerator, prefix, postfix);
        }
        Visit(pnode->sxStrTemplate.pnodeSubstitutionExpressions, byteCodeGenerator, prefix, postfix);
        break;
    }
    case knopExportDefault:
        Visit(pnode->sxExportDefault.pnodeExpr, byteCodeGenerator, prefix, postfix);
        break;
    // PTNODE(knopProg       , "program"    ,None    ,Fnc  ,fnopNone)
    case knopProg:
    {LOGMEIN("ByteCodeGenerator.cpp] 319\n");
        // We expect that the global statements have been generated (meaning that the pnodeFncs
        // field is a real pointer, not an enumeration).
        Assert(pnode->sxFnc.pnodeBody);

        uint i = 0;
        VisitNestedScopes(pnode->sxFnc.pnodeScopes, pnode, byteCodeGenerator, prefix, postfix, &i);
        // Visiting global code: track the last value statement.
        BeginVisitBlock(pnode->sxFnc.pnodeScopes, byteCodeGenerator);
        pnode->sxProg.pnodeLastValStmt = VisitBlock(pnode->sxFnc.pnodeBody, byteCodeGenerator, prefix, postfix);
        EndVisitBlock(pnode->sxFnc.pnodeScopes, byteCodeGenerator);

        break;
    }
    case knopFor:
        BeginVisitBlock(pnode->sxFor.pnodeBlock, byteCodeGenerator);
        Visit(pnode->sxFor.pnodeInit, byteCodeGenerator, prefix, postfix);
        byteCodeGenerator->EnterLoop();
        Visit(pnode->sxFor.pnodeCond, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxFor.pnodeIncr, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxFor.pnodeBody, byteCodeGenerator, prefix, postfix, pnode);
        byteCodeGenerator->ExitLoop();
        EndVisitBlock(pnode->sxFor.pnodeBlock, byteCodeGenerator);
        break;
    // PTNODE(knopIf         , "if"        ,None    ,If   ,fnopNone)
    case knopIf:
        Visit(pnode->sxIf.pnodeCond, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxIf.pnodeTrue, byteCodeGenerator, prefix, postfix, pnode);
        if (pnode->sxIf.pnodeFalse != nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 348\n");
            Visit(pnode->sxIf.pnodeFalse, byteCodeGenerator, prefix, postfix, pnode);
        }
        break;
    // PTNODE(knopWhile      , "while"        ,None    ,While,fnopBreak|fnopContinue)
    // PTNODE(knopDoWhile    , "do-while"    ,None    ,While,fnopBreak|fnopContinue)
    case knopDoWhile:
    case knopWhile:
        byteCodeGenerator->EnterLoop();
        Visit(pnode->sxWhile.pnodeCond, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxWhile.pnodeBody, byteCodeGenerator, prefix, postfix, pnode);
        byteCodeGenerator->ExitLoop();
        break;
    // PTNODE(knopForIn      , "for in"    ,None    ,ForIn,fnopBreak|fnopContinue|fnopCleanup)
    case knopForIn:
    case knopForOf:
        BeginVisitBlock(pnode->sxForInOrForOf.pnodeBlock, byteCodeGenerator);
        Visit(pnode->sxForInOrForOf.pnodeLval, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxForInOrForOf.pnodeObj, byteCodeGenerator, prefix, postfix);
        byteCodeGenerator->EnterLoop();
        Visit(pnode->sxForInOrForOf.pnodeBody, byteCodeGenerator, prefix, postfix, pnode);
        byteCodeGenerator->ExitLoop();
        EndVisitBlock(pnode->sxForInOrForOf.pnodeBlock, byteCodeGenerator);
        break;
    // PTNODE(knopReturn     , "return"    ,None    ,Uni  ,fnopNone)
    case knopReturn:
        if (pnode->sxReturn.pnodeExpr != nullptr)
            Visit(pnode->sxReturn.pnodeExpr, byteCodeGenerator, prefix, postfix);
        break;
    // PTNODE(knopBlock      , "{}"        ,None    ,Block,fnopNone)
    case knopBlock:
    {LOGMEIN("ByteCodeGenerator.cpp] 379\n");
        if (pnode->sxBlock.pnodeStmt != nullptr)
        {
            BeginVisitBlock(pnode, byteCodeGenerator);
            pnode->sxBlock.pnodeLastValStmt = VisitBlock(pnode->sxBlock.pnodeStmt, byteCodeGenerator, prefix, postfix, pnode);
            EndVisitBlock(pnode, byteCodeGenerator);
        }
        else
        {
            pnode->sxBlock.pnodeLastValStmt = nullptr;
        }
        break;
    }
    // PTNODE(knopWith       , "with"        ,None    ,With ,fnopCleanup)
    case knopWith:
        Visit(pnode->sxWith.pnodeObj, byteCodeGenerator, prefix, postfix);
        VisitWithStmt(pnode, pnode->sxWith.pnodeObj->location, byteCodeGenerator, prefix, postfix, pnode);
        break;
    // PTNODE(knopBreak      , "break"        ,None    ,Jump ,fnopNone)
    case knopBreak:
        // TODO: some representation of target
        break;
    // PTNODE(knopContinue   , "continue"    ,None    ,Jump ,fnopNone)
    case knopContinue:
        // TODO: some representation of target
        break;
    // PTNODE(knopLabel      , "label"        ,None    ,Label,fnopNone)
    case knopLabel:
        // TODO: print labeled statement
        break;
    // PTNODE(knopSwitch     , "switch"    ,None    ,Switch,fnopBreak)
    case knopSwitch:
        Visit(pnode->sxSwitch.pnodeVal, byteCodeGenerator, prefix, postfix);
        BeginVisitBlock(pnode->sxSwitch.pnodeBlock, byteCodeGenerator);
        for (ParseNode *pnodeT = pnode->sxSwitch.pnodeCases; nullptr != pnodeT; pnodeT = pnodeT->sxCase.pnodeNext)
        {
            Visit(pnodeT, byteCodeGenerator, prefix, postfix, pnode);
        }
        Visit(pnode->sxSwitch.pnodeBlock, byteCodeGenerator, prefix, postfix);
        EndVisitBlock(pnode->sxSwitch.pnodeBlock, byteCodeGenerator);
        break;
    // PTNODE(knopCase       , "case"        ,None    ,Case ,fnopNone)
    case knopCase:
        Visit(pnode->sxCase.pnodeExpr, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxCase.pnodeBody, byteCodeGenerator, prefix, postfix, pnode);
        break;
    case knopTypeof:
        Visit(pnode->sxUni.pnode1, byteCodeGenerator, prefix, postfix);
        break;
    // PTNODE(knopTryCatchFinally,"try-catch-finally",None,TryCatchFinally,fnopCleanup)
    case knopTryFinally:
        Visit(pnode->sxTryFinally.pnodeTry, byteCodeGenerator, prefix, postfix, pnode);
        Visit(pnode->sxTryFinally.pnodeFinally, byteCodeGenerator, prefix, postfix, pnode);
        break;
    // PTNODE(knopTryCatch      , "try-catch" ,None    ,TryCatch  ,fnopCleanup)
    case knopTryCatch:
        Visit(pnode->sxTryCatch.pnodeTry, byteCodeGenerator, prefix, postfix, pnode);
        Visit(pnode->sxTryCatch.pnodeCatch, byteCodeGenerator, prefix, postfix, pnode);
        break;
    // PTNODE(knopTry        , "try"       ,None    ,Try  ,fnopCleanup)
    case knopTry:
        Visit(pnode->sxTry.pnodeBody, byteCodeGenerator, prefix, postfix, pnode);
        break;
    case knopCatch:
        BeginVisitCatch(pnode, byteCodeGenerator);
        Visit(pnode->sxCatch.pnodeParam, byteCodeGenerator, prefix, postfix);
        Visit(pnode->sxCatch.pnodeBody, byteCodeGenerator, prefix, postfix, pnode);
        EndVisitCatch(pnode, byteCodeGenerator);
        break;
    case knopFinally:
        Visit(pnode->sxFinally.pnodeBody, byteCodeGenerator, prefix, postfix, pnode);
        break;
    // PTNODE(knopThrow      , "throw"     ,None    ,Uni  ,fnopNone)
    case knopThrow:
        Visit(pnode->sxUni.pnode1, byteCodeGenerator, prefix, postfix);
        break;
    case knopArray:
    {LOGMEIN("ByteCodeGenerator.cpp] 456\n");
        bool arrayLitOpt = EmitAsConstantArray(pnode, byteCodeGenerator);
        if (!arrayLitOpt)
        {LOGMEIN("ByteCodeGenerator.cpp] 459\n");
            Visit(pnode->sxUni.pnode1, byteCodeGenerator, prefix, postfix);
        }
        break;
    }
    case knopComma:
    {LOGMEIN("ByteCodeGenerator.cpp] 465\n");
        ParseNode *pnode1 = pnode->sxBin.pnode1;
        if (pnode1->nop == knopComma)
        {LOGMEIN("ByteCodeGenerator.cpp] 468\n");
            // Spot-fix to avoid recursion on very large comma expressions.
            ArenaAllocator *alloc = byteCodeGenerator->GetAllocator();
            SList<ParseNode*> rhsStack(alloc);
            do
            {LOGMEIN("ByteCodeGenerator.cpp] 473\n");
                rhsStack.Push(pnode1->sxBin.pnode2);
                pnode1 = pnode1->sxBin.pnode1;
            }
            while (pnode1->nop == knopComma);

            Visit(pnode1, byteCodeGenerator, prefix, postfix);
            while (!rhsStack.Empty())
            {LOGMEIN("ByteCodeGenerator.cpp] 481\n");
                ParseNode *pnodeRhs = rhsStack.Pop();
                Visit(pnodeRhs, byteCodeGenerator, prefix, postfix);
            }
        }
        else
        {
            Visit(pnode1, byteCodeGenerator, prefix, postfix);
        }
        Visit(pnode->sxBin.pnode2, byteCodeGenerator, prefix, postfix);
    }
        break;
    }
    if (pnodeParent)
    {
        PropagateFlags(pnode, pnodeParent);
    }
    postfix(pnode, byteCodeGenerator);
}

bool IsJump(ParseNode *pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 502\n");
    switch (pnode->nop)
    {LOGMEIN("ByteCodeGenerator.cpp] 504\n");
    case knopBreak:
    case knopContinue:
    case knopThrow:
    case knopReturn:
        return true;

    case knopBlock:
    case knopDoWhile:
    case knopWhile:
    case knopWith:
    case knopIf:
    case knopForIn:
    case knopForOf:
    case knopFor:
    case knopSwitch:
    case knopCase:
    case knopTryFinally:
    case knopTryCatch:
    case knopTry:
    case knopCatch:
    case knopFinally:
        return (pnode->sxStmt.grfnop & fnopJump) != 0;

    default:
        return false;
    }
}

void PropagateFlags(ParseNode *pnodeChild, ParseNode *pnodeParent)
{LOGMEIN("ByteCodeGenerator.cpp] 534\n");
    if (IsJump(pnodeChild))
    {LOGMEIN("ByteCodeGenerator.cpp] 536\n");
        pnodeParent->sxStmt.grfnop |= fnopJump;
    }
}

void Bind(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator);
void BindReference(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator);
void AssignRegisters(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator);

// TODO[ianhall]: This should be in a shared AST Utility header or source file
bool IsExpressionStatement(ParseNode* stmt, const Js::ScriptContext *const scriptContext)
{LOGMEIN("ByteCodeGenerator.cpp] 547\n");
    if (stmt->nop == knopFncDecl)
    {LOGMEIN("ByteCodeGenerator.cpp] 549\n");
        // 'knopFncDecl' is used for both function declarations and function expressions. In a program, a function expression
        // produces the function object that is created for the function expression as its value for the program. A function
        // declaration does not produce a value for the program.
        return !stmt->sxFnc.IsDeclaration();
    }
    if ((stmt->nop >= 0) && (stmt->nop<knopLim))
    {LOGMEIN("ByteCodeGenerator.cpp] 556\n");
        return (ParseNode::Grfnop(stmt->nop) & fnopNotExprStmt) == 0;
    }
    return false;
}

bool MustProduceValue(ParseNode *pnode, const Js::ScriptContext *const scriptContext)
{
    // Determine whether the current statement is guaranteed to produce a value.

    if (IsExpressionStatement(pnode, scriptContext))
    {LOGMEIN("ByteCodeGenerator.cpp] 567\n");
        // These are trivially true.
        return true;
    }

    for (;;)
    {LOGMEIN("ByteCodeGenerator.cpp] 573\n");
        switch (pnode->nop)
        {LOGMEIN("ByteCodeGenerator.cpp] 575\n");
        case knopFor:
            // Check the common "for (;;)" case.
            if (pnode->sxFor.pnodeCond != nullptr ||
                pnode->sxFor.pnodeBody == nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 580\n");
                return false;
            }
            // Loop body is always executed. Look at the loop body next.
            pnode = pnode->sxFor.pnodeBody;
            break;

        case knopIf:
            // True only if both "if" and "else" exist, and both produce values.
            if (pnode->sxIf.pnodeTrue == nullptr ||
                pnode->sxIf.pnodeFalse == nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 591\n");
                return false;
            }
            if (!MustProduceValue(pnode->sxIf.pnodeFalse, scriptContext))
            {LOGMEIN("ByteCodeGenerator.cpp] 595\n");
                return false;
            }
            pnode = pnode->sxIf.pnodeTrue;
            break;

        case knopWhile:
            // Check the common "while (1)" case.
            if (pnode->sxWhile.pnodeBody == nullptr ||
                (pnode->sxWhile.pnodeCond &&
                (pnode->sxWhile.pnodeCond->nop != knopInt ||
                pnode->sxWhile.pnodeCond->sxInt.lw == 0)))
            {LOGMEIN("ByteCodeGenerator.cpp] 607\n");
                return false;
            }
            // Loop body is always executed. Look at the loop body next.
            pnode = pnode->sxWhile.pnodeBody;
            break;

        case knopDoWhile:
            if (pnode->sxWhile.pnodeBody == nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 616\n");
                return false;
            }
            // Loop body is always executed. Look at the loop body next.
            pnode = pnode->sxWhile.pnodeBody;
            break;

        case knopBlock:
            return pnode->sxBlock.pnodeLastValStmt != nullptr;

        case knopWith:
            if (pnode->sxWith.pnodeBody == nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 628\n");
                return false;
            }
            pnode = pnode->sxWith.pnodeBody;
            break;

        case knopSwitch:
            {LOGMEIN("ByteCodeGenerator.cpp] 635\n");
                // This is potentially the most inefficient case. We could consider adding a flag to the PnSwitch
                // struct and computing it when we visit the switch, but:
                // a. switch statements at global scope shouldn't be that common;
                // b. switch statements with many arms shouldn't be that common;
                // c. switches without default cases can be trivially skipped.
                if (pnode->sxSwitch.pnodeDefault == nullptr)
                {LOGMEIN("ByteCodeGenerator.cpp] 642\n");
                    // Can't guarantee that any code is executed.
                return false;
                }
                ParseNode *pnodeCase;
                for (pnodeCase = pnode->sxSwitch.pnodeCases; pnodeCase; pnodeCase = pnodeCase->sxCase.pnodeNext)
                {LOGMEIN("ByteCodeGenerator.cpp] 648\n");
                    if (pnodeCase->sxCase.pnodeBody == nullptr)
                    {LOGMEIN("ByteCodeGenerator.cpp] 650\n");
                        if (pnodeCase->sxCase.pnodeNext == nullptr)
                        {LOGMEIN("ByteCodeGenerator.cpp] 652\n");
                            // Last case has no code to execute.
                        return false;
                        }
                        // Fall through to the next case.
                    }
                    else
                    {
                        if (!MustProduceValue(pnodeCase->sxCase.pnodeBody, scriptContext))
                        {LOGMEIN("ByteCodeGenerator.cpp] 661\n");
                        return false;
                        }
                    }
                }
            return true;
            }

        case knopTryCatch:
            // True only if both try and catch produce a value.
            if (pnode->sxTryCatch.pnodeTry->sxTry.pnodeBody == nullptr ||
                pnode->sxTryCatch.pnodeCatch->sxCatch.pnodeBody == nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 673\n");
                return false;
            }
            if (!MustProduceValue(pnode->sxTryCatch.pnodeCatch->sxCatch.pnodeBody, scriptContext))
            {LOGMEIN("ByteCodeGenerator.cpp] 677\n");
                return false;
            }
            pnode = pnode->sxTryCatch.pnodeTry->sxTry.pnodeBody;
            break;

        case knopTryFinally:
            if (pnode->sxTryFinally.pnodeFinally->sxFinally.pnodeBody == nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 685\n");
                // No finally body: look at the try body.
                if (pnode->sxTryFinally.pnodeTry->sxTry.pnodeBody == nullptr)
                {LOGMEIN("ByteCodeGenerator.cpp] 688\n");
                    return false;
                }
                pnode = pnode->sxTryFinally.pnodeTry->sxTry.pnodeBody;
                break;
            }
            // Skip the try body, since the finally body will always follow it.
            pnode = pnode->sxTryFinally.pnodeFinally->sxFinally.pnodeBody;
            break;

        default:
            return false;
        }
    }
}

ByteCodeGenerator::ByteCodeGenerator(Js::ScriptContext* scriptContext, Js::ScopeInfo* parentScopeInfo) :
    alloc(nullptr),
    scriptContext(scriptContext),
    flags(0),
    funcInfoStack(nullptr),
    pRootFunc(nullptr),
    pCurrentFunction(nullptr),
    globalScope(nullptr),
    currentScope(nullptr),
    parentScopeInfo(parentScopeInfo),
    dynamicScopeCount(0),
    isBinding(false),
    propertyRecords(nullptr),
    inDestructuredPattern(false)
{LOGMEIN("ByteCodeGenerator.cpp] 718\n");
    m_writer.Create();
}

/* static */
bool ByteCodeGenerator::IsFalse(ParseNode* node)
{LOGMEIN("ByteCodeGenerator.cpp] 724\n");
    return (node->nop == knopInt && node->sxInt.lw == 0) || node->nop == knopFalse;
}

bool ByteCodeGenerator::IsES6DestructuringEnabled() const
{LOGMEIN("ByteCodeGenerator.cpp] 729\n");
    return scriptContext->GetConfig()->IsES6DestructuringEnabled();
}

bool ByteCodeGenerator::IsES6ForLoopSemanticsEnabled() const
{LOGMEIN("ByteCodeGenerator.cpp] 734\n");
    return scriptContext->GetConfig()->IsES6ForLoopSemanticsEnabled();
}

// ByteCodeGenerator debug mode means we are generating debug mode user-code. Library code is always in non-debug mode.
bool ByteCodeGenerator::IsInDebugMode() const
{LOGMEIN("ByteCodeGenerator.cpp] 740\n");
    return m_utf8SourceInfo->IsInDebugMode();
}

// ByteCodeGenerator non-debug mode means we are not debugging, or we are generating library code which is always in non-debug mode.
bool ByteCodeGenerator::IsInNonDebugMode() const
{LOGMEIN("ByteCodeGenerator.cpp] 746\n");
    return scriptContext->IsScriptContextInNonDebugMode() || m_utf8SourceInfo->GetIsLibraryCode();
}

bool ByteCodeGenerator::ShouldTrackDebuggerMetadata() const
{LOGMEIN("ByteCodeGenerator.cpp] 751\n");
    return (IsInDebugMode())
#if DBG_DUMP
        || (Js::Configuration::Global.flags.Debug)
#endif
        ;
}

void ByteCodeGenerator::SetRootFuncInfo(FuncInfo* func)
{LOGMEIN("ByteCodeGenerator.cpp] 760\n");
    Assert(pRootFunc == nullptr || pRootFunc == func->byteCodeFunction || !IsInNonDebugMode());

    if ((this->flags & (fscrImplicitThis | fscrImplicitParents)) && !this->HasParentScopeInfo())
    {LOGMEIN("ByteCodeGenerator.cpp] 764\n");
        // Mark a top-level event handler, since it will need to construct the "this" pointer's
        // namespace hierarchy to access globals.
        Assert(!func->IsGlobalFunction());
        func->SetIsTopLevelEventHandler(true);
    }

    if (pRootFunc)
    {LOGMEIN("ByteCodeGenerator.cpp] 772\n");
        return;
    }

    this->pRootFunc = func->byteCodeFunction->GetParseableFunctionInfo();
    this->m_utf8SourceInfo->AddTopLevelFunctionInfo(func->byteCodeFunction->GetFunctionInfo(), scriptContext->GetRecycler());
}

Js::RegSlot ByteCodeGenerator::NextVarRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 781\n");
    return funcInfoStack->Top()->NextVarRegister();
}

Js::RegSlot ByteCodeGenerator::NextConstRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 786\n");
    return funcInfoStack->Top()->NextConstRegister();
}

FuncInfo * ByteCodeGenerator::TopFuncInfo() const
{LOGMEIN("ByteCodeGenerator.cpp] 791\n");
    return funcInfoStack->Empty() ? nullptr : funcInfoStack->Top();
}

void ByteCodeGenerator::EnterLoop()
{LOGMEIN("ByteCodeGenerator.cpp] 796\n");
    if (this->TopFuncInfo())
    {LOGMEIN("ByteCodeGenerator.cpp] 798\n");
        this->TopFuncInfo()->hasLoop = true;
    }
    loopDepth++;
}

void ByteCodeGenerator::SetHasTry(bool has)
{LOGMEIN("ByteCodeGenerator.cpp] 805\n");
    TopFuncInfo()->GetParsedFunctionBody()->SetHasTry(has);
}

void ByteCodeGenerator::SetHasFinally(bool has)
{LOGMEIN("ByteCodeGenerator.cpp] 810\n");
    TopFuncInfo()->GetParsedFunctionBody()->SetHasFinally(has);
}

// TODO: per-function register assignment for env and global symbols
void ByteCodeGenerator::AssignRegister(Symbol *sym)
    {LOGMEIN("ByteCodeGenerator.cpp] 816\n");
    AssertMsg(sym->GetDecl() == nullptr || sym->GetDecl()->nop != knopConstDecl || sym->GetDecl()->nop != knopLetDecl,
        "const and let should get only temporary register, assigned during emit stage");
    if (sym->GetLocation() == Js::Constants::NoRegister)
    {LOGMEIN("ByteCodeGenerator.cpp] 820\n");
        sym->SetLocation(NextVarRegister());
    }
}

void ByteCodeGenerator::AddTargetStmt(ParseNode *pnodeStmt)
{LOGMEIN("ByteCodeGenerator.cpp] 826\n");
    FuncInfo *top = funcInfoStack->Top();
    top->AddTargetStmt(pnodeStmt);
}

Js::RegSlot ByteCodeGenerator::AssignNullConstRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 832\n");
    FuncInfo *top = funcInfoStack->Top();
    return top->AssignNullConstRegister();
}

Js::RegSlot ByteCodeGenerator::AssignUndefinedConstRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 838\n");
    FuncInfo *top = funcInfoStack->Top();
    return top->AssignUndefinedConstRegister();
}

Js::RegSlot ByteCodeGenerator::AssignTrueConstRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 844\n");
    FuncInfo *top = funcInfoStack->Top();
    return top->AssignTrueConstRegister();
}

Js::RegSlot ByteCodeGenerator::AssignFalseConstRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 850\n");
    FuncInfo *top = funcInfoStack->Top();
    return top->AssignFalseConstRegister();
}

Js::RegSlot ByteCodeGenerator::AssignThisRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 856\n");
    FuncInfo *top = funcInfoStack->Top();
    return top->AssignThisRegister();
}

Js::RegSlot ByteCodeGenerator::AssignNewTargetRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 862\n");
    FuncInfo *top = funcInfoStack->Top();
    return top->AssignNewTargetRegister();
}

void ByteCodeGenerator::SetNeedEnvRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 868\n");
    FuncInfo *top = funcInfoStack->Top();
    top->SetNeedEnvRegister();
}

void ByteCodeGenerator::AssignFrameObjRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 874\n");
    FuncInfo* top = funcInfoStack->Top();
    if (top->frameObjRegister == Js::Constants::NoRegister)
    {LOGMEIN("ByteCodeGenerator.cpp] 877\n");
        top->frameObjRegister = top->NextVarRegister();
    }
}

void ByteCodeGenerator::AssignFrameDisplayRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 883\n");
    FuncInfo* top = funcInfoStack->Top();
    if (top->frameDisplayRegister == Js::Constants::NoRegister)
    {LOGMEIN("ByteCodeGenerator.cpp] 886\n");
        top->frameDisplayRegister = top->NextVarRegister();
    }
}

void ByteCodeGenerator::AssignFrameSlotsRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 892\n");
    FuncInfo* top = funcInfoStack->Top();
    if (top->frameSlotsRegister == Js::Constants::NoRegister)
    {LOGMEIN("ByteCodeGenerator.cpp] 895\n");
        top->frameSlotsRegister = NextVarRegister();
    }
}

void ByteCodeGenerator::AssignParamSlotsRegister()
{LOGMEIN("ByteCodeGenerator.cpp] 901\n");
    FuncInfo* top = funcInfoStack->Top();
    Assert(top->paramSlotsRegister == Js::Constants::NoRegister);
    top->paramSlotsRegister = NextVarRegister();
}

void ByteCodeGenerator::SetNumberOfInArgs(Js::ArgSlot argCount)
{LOGMEIN("ByteCodeGenerator.cpp] 908\n");
    FuncInfo *top = funcInfoStack->Top();
    top->inArgsCount = argCount;
}

Js::RegSlot ByteCodeGenerator::EnregisterConstant(unsigned int constant)
{LOGMEIN("ByteCodeGenerator.cpp] 914\n");
    Js::RegSlot loc = Js::Constants::NoRegister;
    FuncInfo *top = funcInfoStack->Top();
    if (!top->constantToRegister.TryGetValue(constant, &loc))
    {LOGMEIN("ByteCodeGenerator.cpp] 918\n");
        loc = NextConstRegister();
        top->constantToRegister.Add(constant, loc);
    }
    return loc;
}

Js::RegSlot ByteCodeGenerator::EnregisterStringConstant(IdentPtr pid)
{LOGMEIN("ByteCodeGenerator.cpp] 926\n");
    Js::RegSlot loc = Js::Constants::NoRegister;
    FuncInfo *top = funcInfoStack->Top();
    if (!top->stringToRegister.TryGetValue(pid, &loc))
    {LOGMEIN("ByteCodeGenerator.cpp] 930\n");
        loc = NextConstRegister();
        top->stringToRegister.Add(pid, loc);
    }
    return loc;
}

Js::RegSlot ByteCodeGenerator::EnregisterDoubleConstant(double d)
{LOGMEIN("ByteCodeGenerator.cpp] 938\n");
    Js::RegSlot loc = Js::Constants::NoRegister;
    FuncInfo *top = funcInfoStack->Top();
    if (!top->TryGetDoubleLoc(d, &loc))
    {LOGMEIN("ByteCodeGenerator.cpp] 942\n");
        loc = NextConstRegister();
        top->AddDoubleConstant(d, loc);
    }
    return loc;
}

Js::RegSlot ByteCodeGenerator::EnregisterStringTemplateCallsiteConstant(ParseNode* pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 950\n");
    Assert(pnode->nop == knopStrTemplate);
    Assert(pnode->sxStrTemplate.isTaggedTemplate);

    Js::RegSlot loc = Js::Constants::NoRegister;
    FuncInfo* top = funcInfoStack->Top();

    if (!top->stringTemplateCallsiteRegisterMap.TryGetValue(pnode, &loc))
    {LOGMEIN("ByteCodeGenerator.cpp] 958\n");
        loc = NextConstRegister();

        top->stringTemplateCallsiteRegisterMap.Add(pnode, loc);
    }

    return loc;
}

//
// Restore all outer func scope info when reparsing a deferred func.
//
void ByteCodeGenerator::RestoreScopeInfo(Js::ParseableFunctionInfo* functionBody)
{LOGMEIN("ByteCodeGenerator.cpp] 971\n");
    if (functionBody && functionBody->GetScopeInfo())
    {
        PROBE_STACK(scriptContext, Js::Constants::MinStackByteCodeVisitor);

        Js::ScopeInfo* scopeInfo = functionBody->GetScopeInfo();
        RestoreScopeInfo(scopeInfo->GetParent()); // Recursively restore outer func scope info

        Js::ScopeInfo* paramScopeInfo = scopeInfo->GetParamScopeInfo();
        Scope* paramScope = nullptr;
        if (paramScopeInfo != nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 982\n");
            paramScope = paramScopeInfo->GetScope();
            Assert(paramScope);
            if (!paramScopeInfo->GetCanMergeWithBodyScope())
            {LOGMEIN("ByteCodeGenerator.cpp] 986\n");
                paramScope->SetCannotMergeWithBodyScope();
            }
            // We need the funcInfo before continuing the restoration of the param scope, so wait for the funcInfo to be created.
        }

        Scope* bodyScope = scopeInfo->GetScope();

        Assert(bodyScope);
        bodyScope->SetHasOwnLocalInClosure(scopeInfo->GetHasOwnLocalInClosure());

        FuncInfo* func = Anew(alloc, FuncInfo, functionBody->GetDisplayName(), alloc, paramScope, bodyScope, nullptr, functionBody);

        if (bodyScope->GetScopeType() == ScopeType_GlobalEvalBlock)
        {LOGMEIN("ByteCodeGenerator.cpp] 1000\n");
            func->bodyScope = this->currentScope;
        }
        PushFuncInfo(_u("RestoreScopeInfo"), func);

        if (!functionBody->DoStackNestedFunc())
        {LOGMEIN("ByteCodeGenerator.cpp] 1006\n");
            func->hasEscapedUseNestedFunc = true;
        }

        Js::ScopeInfo* funcExprScopeInfo = scopeInfo->GetFuncExprScopeInfo();
        if (funcExprScopeInfo)
        {LOGMEIN("ByteCodeGenerator.cpp] 1012\n");
            Scope* funcExprScope = funcExprScopeInfo->GetScope();
            Assert(funcExprScope);
            funcExprScope->SetFunc(func);
            func->SetFuncExprScope(funcExprScope);
            funcExprScopeInfo->GetScopeInfo(nullptr, this, func, funcExprScope);
        }

        // Restore the param scope after the function expression scope
        if (paramScope != nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 1022\n");
            paramScope->SetFunc(func);
            paramScopeInfo->GetScopeInfo(nullptr, this, func, paramScope);
        }
        scopeInfo->GetScopeInfo(nullptr, this, func, bodyScope);
    }
    else
    {
        Assert(this->TopFuncInfo() == nullptr);
        // funcBody is glo
        currentScope = Anew(alloc, Scope, alloc, ScopeType_Global);
        globalScope = currentScope;

        FuncInfo *func = Anew(alloc, FuncInfo, Js::Constants::GlobalFunction,
            alloc, nullptr, currentScope, nullptr, functionBody);
        PushFuncInfo(_u("RestoreScopeInfo"), func);
    }
}

FuncInfo * ByteCodeGenerator::StartBindGlobalStatements(ParseNode *pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 1042\n");
    if (parentScopeInfo && parentScopeInfo->GetParent() && (!parentScopeInfo->GetParent()->GetIsGlobalFunc() || parentScopeInfo->GetParent()->IsEval()))
    {LOGMEIN("ByteCodeGenerator.cpp] 1044\n");
        Assert(CONFIG_FLAG(DeferNested));
        trackEnvDepth = true;
        RestoreScopeInfo(parentScopeInfo->GetParent());
        trackEnvDepth = false;
        // "currentScope" is the parentFunc scope. This ensures the deferred func declaration
        // symbol will bind to the func declaration symbol already available in parentFunc scope.
    }
    else
    {
        currentScope = pnode->sxProg.scope;
        Assert(currentScope);
        globalScope = currentScope;
    }

    Js::FunctionBody * byteCodeFunction;

    if (!IsInNonDebugMode() && this->pCurrentFunction != nullptr && this->pCurrentFunction->GetIsGlobalFunc() && !this->pCurrentFunction->IsFakeGlobalFunc(flags))
    {LOGMEIN("ByteCodeGenerator.cpp] 1062\n");
        // we will re-use the global FunctionBody which was created before deferred parse.
        byteCodeFunction = this->pCurrentFunction;
        byteCodeFunction->RemoveDeferParseAttribute();
        byteCodeFunction->ResetByteCodeGenVisitState();
        if (byteCodeFunction->GetBoundPropertyRecords() == nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 1068\n");
            // This happens when we try to re-use the function body which was created due to serialized bytecode.
            byteCodeFunction->SetBoundPropertyRecords(EnsurePropertyRecordList());
        }
    }
    else if ((this->flags & fscrDeferredFnc))
    {LOGMEIN("ByteCodeGenerator.cpp] 1074\n");
        byteCodeFunction = this->EnsureFakeGlobalFuncForUndefer(pnode);
    }
    else
    {
        byteCodeFunction = this->MakeGlobalFunctionBody(pnode);

        // Mark this global function to required for register script event
        byteCodeFunction->SetIsTopLevel(true);

        if (pnode->sxFnc.GetStrictMode() != 0)
        {LOGMEIN("ByteCodeGenerator.cpp] 1085\n");
            byteCodeFunction->SetIsStrictMode();
        }
    }
    if (byteCodeFunction->IsReparsed())
    {LOGMEIN("ByteCodeGenerator.cpp] 1090\n");
        byteCodeFunction->RestoreState(pnode);
    }
    else
    {
        byteCodeFunction->SaveState(pnode);
    }

    FuncInfo *funcInfo = Anew(alloc, FuncInfo, Js::Constants::GlobalFunction,
        alloc, nullptr, globalScope, pnode, byteCodeFunction);

    int32 currentAstSize = pnode->sxFnc.astSize;
    if (currentAstSize > this->maxAstSize)
    {LOGMEIN("ByteCodeGenerator.cpp] 1103\n");
        this->maxAstSize = currentAstSize;
    }
    PushFuncInfo(_u("StartBindGlobalStatements"), funcInfo);

    return funcInfo;
}

void ByteCodeGenerator::AssignPropertyId(IdentPtr pid)
{LOGMEIN("ByteCodeGenerator.cpp] 1112\n");
    if (pid->GetPropertyId() == Js::Constants::NoProperty)
    {LOGMEIN("ByteCodeGenerator.cpp] 1114\n");
        Js::PropertyId id = TopFuncInfo()->byteCodeFunction->GetOrAddPropertyIdTracked(SymbolName(pid->Psz(), pid->Cch()));
        pid->SetPropertyId(id);
    }
}

void ByteCodeGenerator::AssignPropertyId(Symbol *sym, Js::ParseableFunctionInfo* functionInfo)
{LOGMEIN("ByteCodeGenerator.cpp] 1121\n");
    sym->SetPosition(functionInfo->GetOrAddPropertyIdTracked(sym->GetName()));
}

template <class PrefixFn, class PostfixFn>
ParseNode* VisitBlock(ParseNode *pnode, ByteCodeGenerator* byteCodeGenerator, PrefixFn prefix, PostfixFn postfix, ParseNode *pnodeParent = nullptr)
{LOGMEIN("ByteCodeGenerator.cpp] 1127\n");
    ParseNode *pnodeLastVal = nullptr;
    if (pnode != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 1130\n");
        bool fTrackVal = byteCodeGenerator->IsBinding() &&
            (byteCodeGenerator->GetFlags() & fscrReturnExpression) &&
            byteCodeGenerator->TopFuncInfo()->IsGlobalFunction();
        while (pnode->nop == knopList)
        {LOGMEIN("ByteCodeGenerator.cpp] 1135\n");
            Visit(pnode->sxBin.pnode1, byteCodeGenerator, prefix, postfix, pnodeParent);
            if (fTrackVal)
            {LOGMEIN("ByteCodeGenerator.cpp] 1138\n");
                // If we're tracking values, find the last statement (if any) in the block that is
                // guaranteed to produce a value.
                if (MustProduceValue(pnode->sxBin.pnode1, byteCodeGenerator->GetScriptContext()))
                {LOGMEIN("ByteCodeGenerator.cpp] 1142\n");
                    pnodeLastVal = pnode->sxBin.pnode1;
                }
                if (IsJump(pnode->sxBin.pnode1))
                {LOGMEIN("ByteCodeGenerator.cpp] 1146\n");
                    // This is a jump out of the current block. The remaining instructions (if any)
                    // will not be executed, so stop tracking them.
                    fTrackVal = false;
                }
            }
            pnode = pnode->sxBin.pnode2;
        }
        Visit(pnode, byteCodeGenerator, prefix, postfix, pnodeParent);
        if (fTrackVal)
        {
            if (MustProduceValue(pnode, byteCodeGenerator->GetScriptContext()))
            {LOGMEIN("ByteCodeGenerator.cpp] 1158\n");
                pnodeLastVal = pnode;
            }
        }
    }
    return pnodeLastVal;
}

FuncInfo * ByteCodeGenerator::StartBindFunction(const char16 *name, uint nameLength, uint shortNameOffset, bool* pfuncExprWithName, ParseNode *pnode, Js::ParseableFunctionInfo * reuseNestedFunc)
{LOGMEIN("ByteCodeGenerator.cpp] 1167\n");
    bool funcExprWithName;
    Js::ParseableFunctionInfo* parseableFunctionInfo = nullptr;

    Js::AutoRestoreFunctionInfo autoRestoreFunctionInfo(reuseNestedFunc, reuseNestedFunc ? reuseNestedFunc->GetOriginalEntryPoint() : nullptr);

    if (this->pCurrentFunction &&
        this->pCurrentFunction->IsFunctionParsed())
    {LOGMEIN("ByteCodeGenerator.cpp] 1175\n");
        Assert(this->pCurrentFunction->StartInDocument() == pnode->ichMin);
        Assert(this->pCurrentFunction->LengthInChars() == pnode->LengthInCodepoints());

        // This is the root function for the current AST subtree, and it already has a FunctionBody
        // (created by a deferred parse) which we're now filling in.
        Js::FunctionBody * parsedFunctionBody = this->pCurrentFunction;
        parsedFunctionBody->RemoveDeferParseAttribute();

        if (parsedFunctionBody->GetBoundPropertyRecords() == nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 1185\n");
            // This happens when we try to re-use the function body which was created due to serialized bytecode.
            parsedFunctionBody->SetBoundPropertyRecords(EnsurePropertyRecordList());
        }

        Assert(!parsedFunctionBody->IsDeferredParseFunction() || parsedFunctionBody->IsReparsed());

        pnode->sxFnc.SetDeclaration(parsedFunctionBody->GetIsDeclaration());
        if (!pnode->sxFnc.CanBeDeferred())
        {LOGMEIN("ByteCodeGenerator.cpp] 1194\n");
            parsedFunctionBody->SetAttributes(
                (Js::FunctionInfo::Attributes)(parsedFunctionBody->GetAttributes() & ~Js::FunctionInfo::Attributes::CanDefer));
        }
        funcExprWithName =
            !(parsedFunctionBody->GetIsDeclaration() || pnode->sxFnc.IsMethod()) &&
            pnode->sxFnc.pnodeName != nullptr &&
            pnode->sxFnc.pnodeName->nop == knopVarDecl;
        *pfuncExprWithName = funcExprWithName;

        Assert(parsedFunctionBody->GetLocalFunctionId() == pnode->sxFnc.functionId || !IsInNonDebugMode());

        // Some state may be tracked on the function body during the visit pass. Since the previous visit pass may have failed,
        // we need to reset the state on the function body.
        parsedFunctionBody->ResetByteCodeGenVisitState();

        if (parsedFunctionBody->GetScopeInfo())
        {LOGMEIN("ByteCodeGenerator.cpp] 1211\n");
            // Propagate flags from the (real) parent function.
            Js::ParseableFunctionInfo *parent = parsedFunctionBody->GetScopeInfo()->GetParent();
            if (parent)
            {LOGMEIN("ByteCodeGenerator.cpp] 1215\n");
                if (parent->GetHasOrParentHasArguments())
                {LOGMEIN("ByteCodeGenerator.cpp] 1217\n");
                    parsedFunctionBody->SetHasOrParentHasArguments(true);
                }
            }
        }

        parseableFunctionInfo = parsedFunctionBody;
    }
    else
    {
        funcExprWithName = *pfuncExprWithName;
        Js::LocalFunctionId functionId = pnode->sxFnc.functionId;

        // Create a function body if:
        //  1. The parse node is not defer parsed
        //  2. Or creating function proxies is disallowed
        bool createFunctionBody = (pnode->sxFnc.pnodeBody != nullptr);
        if (!CONFIG_FLAG(CreateFunctionProxy)) createFunctionBody = true;

        Js::FunctionInfo::Attributes attributes = Js::FunctionInfo::Attributes::None;
        if (pnode->sxFnc.IsAsync())
        {LOGMEIN("ByteCodeGenerator.cpp] 1238\n");
            attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::ErrorOnNew | Js::FunctionInfo::Attributes::Async);
        }
        if (pnode->sxFnc.IsLambda())
        {LOGMEIN("ByteCodeGenerator.cpp] 1242\n");
            attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::ErrorOnNew | Js::FunctionInfo::Attributes::Lambda);
        }
        if (pnode->sxFnc.HasSuperReference())
        {LOGMEIN("ByteCodeGenerator.cpp] 1246\n");
            attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::SuperReference);
        }
        if (pnode->sxFnc.IsClassMember())
        {LOGMEIN("ByteCodeGenerator.cpp] 1250\n");
            if (pnode->sxFnc.IsClassConstructor())
            {LOGMEIN("ByteCodeGenerator.cpp] 1252\n");
                attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::ClassConstructor);
            }
            else
            {
                attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::ErrorOnNew | Js::FunctionInfo::Attributes::ClassMethod);
            }
        }
        if (pnode->sxFnc.IsGenerator())
        {LOGMEIN("ByteCodeGenerator.cpp] 1261\n");
            attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::Generator);
        }
        if (pnode->sxFnc.IsAccessor())
        {LOGMEIN("ByteCodeGenerator.cpp] 1265\n");
            attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::ErrorOnNew);
        }
        if (pnode->sxFnc.IsModule())
        {LOGMEIN("ByteCodeGenerator.cpp] 1269\n");
            attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::Module);
        }
        if (pnode->sxFnc.CanBeDeferred())
        {LOGMEIN("ByteCodeGenerator.cpp] 1273\n");
            attributes = (Js::FunctionInfo::Attributes)(attributes | Js::FunctionInfo::Attributes::CanDefer);
        }

        if (createFunctionBody)
        {LOGMEIN("ByteCodeGenerator.cpp] 1278\n");
            ENTER_PINNED_SCOPE(Js::PropertyRecordList, propertyRecordList);
            propertyRecordList = EnsurePropertyRecordList();
            if (reuseNestedFunc)
            {LOGMEIN("ByteCodeGenerator.cpp] 1282\n");
                if (!reuseNestedFunc->IsFunctionBody())
                {LOGMEIN("ByteCodeGenerator.cpp] 1284\n");
                    reuseNestedFunc->GetUtf8SourceInfo()->StopTrackingDeferredFunction(reuseNestedFunc->GetLocalFunctionId());
                    Js::FunctionBody * parsedFunctionBody =
                        Js::FunctionBody::NewFromParseableFunctionInfo(reuseNestedFunc->GetParseableFunctionInfo(), propertyRecordList);
                    autoRestoreFunctionInfo.funcBody = parsedFunctionBody;
                    parseableFunctionInfo = parsedFunctionBody;
                }
                else
                {
                    parseableFunctionInfo = reuseNestedFunc->GetFunctionBody();
                }
            }
            else
            {
                parseableFunctionInfo = Js::FunctionBody::NewFromRecycler(scriptContext, name, nameLength, shortNameOffset, pnode->sxFnc.nestedCount, m_utf8SourceInfo,
                    m_utf8SourceInfo->GetSrcInfo()->sourceContextInfo->sourceContextId, functionId, propertyRecordList
                    , attributes
                    , pnode->sxFnc.IsClassConstructor() ?
                        Js::FunctionBody::FunctionBodyFlags::Flags_None :
                        Js::FunctionBody::FunctionBodyFlags::Flags_HasNoExplicitReturnValue
#ifdef PERF_COUNTERS
                    , false /* is function from deferred deserialized proxy */
#endif
                );
            }
            LEAVE_PINNED_SCOPE();
        }
        else
        {
            ENTER_PINNED_SCOPE(Js::PropertyRecordList, propertyRecordList);
            propertyRecordList = nullptr;

            if (funcExprWithName)
            {LOGMEIN("ByteCodeGenerator.cpp] 1317\n");
                propertyRecordList = EnsurePropertyRecordList();
            }

            if (reuseNestedFunc)
            {LOGMEIN("ByteCodeGenerator.cpp] 1322\n");
                Assert(!reuseNestedFunc->IsFunctionBody() || reuseNestedFunc->GetFunctionBody()->GetByteCode() != nullptr);
                Assert(pnode->sxFnc.pnodeBody == nullptr);
                parseableFunctionInfo = reuseNestedFunc;
            }
            else
            {
                parseableFunctionInfo = Js::ParseableFunctionInfo::New(scriptContext, pnode->sxFnc.nestedCount, functionId, m_utf8SourceInfo, name, nameLength, shortNameOffset, propertyRecordList, attributes,
                                        pnode->sxFnc.IsClassConstructor() ?
                                            Js::FunctionBody::FunctionBodyFlags::Flags_None :
                                            Js::FunctionBody::FunctionBodyFlags::Flags_HasNoExplicitReturnValue);
            }
            LEAVE_PINNED_SCOPE();
        }

        // In either case register the function reference
        scriptContext->GetLibrary()->RegisterDynamicFunctionReference(parseableFunctionInfo);

#if DBG
        parseableFunctionInfo->deferredParseNextFunctionId = pnode->sxFnc.deferredParseNextFunctionId;
#endif
        parseableFunctionInfo->SetIsDeclaration(pnode->sxFnc.IsDeclaration() != 0);
        parseableFunctionInfo->SetIsAccessor(pnode->sxFnc.IsAccessor() != 0);
        if (pnode->sxFnc.IsAccessor())
        {LOGMEIN("ByteCodeGenerator.cpp] 1346\n");
            scriptContext->optimizationOverrides.SetSideEffects(Js::SideEffects_Accessor);
        }
    }

    Scope *funcExprScope = nullptr;
    if (funcExprWithName)
    {LOGMEIN("ByteCodeGenerator.cpp] 1353\n");
        funcExprScope = pnode->sxFnc.scope;
        Assert(funcExprScope);
        PushScope(funcExprScope);
        Symbol *sym = AddSymbolToScope(funcExprScope, name, nameLength, pnode->sxFnc.pnodeName, STFunction);

        sym->SetIsFuncExpr(true);

        sym->SetPosition(parseableFunctionInfo->GetOrAddPropertyIdTracked(sym->GetName()));

        pnode->sxFnc.SetFuncSymbol(sym);
    }

    Scope *paramScope = pnode->sxFnc.pnodeScopes ? pnode->sxFnc.pnodeScopes->sxBlock.scope : nullptr;
    Scope *bodyScope = pnode->sxFnc.pnodeBodyScope ? pnode->sxFnc.pnodeBodyScope->sxBlock.scope : nullptr;
    Assert(paramScope != nullptr || !pnode->sxFnc.pnodeScopes);
    if (paramScope == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 1370\n");
        paramScope = Anew(alloc, Scope, alloc, ScopeType_Parameter, true);
        if (pnode->sxFnc.pnodeScopes)
        {LOGMEIN("ByteCodeGenerator.cpp] 1373\n");
            pnode->sxFnc.pnodeScopes->sxBlock.scope = paramScope;
        }
    }
    if (bodyScope == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 1378\n");
        bodyScope = Anew(alloc, Scope, alloc, ScopeType_FunctionBody, true);
        if (pnode->sxFnc.pnodeBodyScope)
        {LOGMEIN("ByteCodeGenerator.cpp] 1381\n");
            pnode->sxFnc.pnodeBodyScope->sxBlock.scope = bodyScope;
        }
    }

    AssertMsg(pnode->nop == knopFncDecl, "Non-function declaration trying to create function body");

    parseableFunctionInfo->SetIsGlobalFunc(false);
    if (pnode->sxFnc.GetStrictMode() != 0)
    {LOGMEIN("ByteCodeGenerator.cpp] 1390\n");
        parseableFunctionInfo->SetIsStrictMode();
    }

    FuncInfo *funcInfo = Anew(alloc, FuncInfo, name, alloc, paramScope, bodyScope, pnode, parseableFunctionInfo);

#if DBG
    funcInfo->isReused = (reuseNestedFunc != nullptr);
#endif

    if (pnode->sxFnc.GetArgumentsObjectEscapes())
    {LOGMEIN("ByteCodeGenerator.cpp] 1401\n");
        // If the parser detected that the arguments object escapes, then the function scope escapes
        // and cannot be cached.
        this->FuncEscapes(bodyScope);
        funcInfo->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("ArgumentsObjectEscapes")));
    }

    if (parseableFunctionInfo->IsFunctionBody())
    {LOGMEIN("ByteCodeGenerator.cpp] 1409\n");
        Js::FunctionBody * parsedFunctionBody = parseableFunctionInfo->GetFunctionBody();
        if (parsedFunctionBody->IsReparsed())
        {LOGMEIN("ByteCodeGenerator.cpp] 1412\n");
            parsedFunctionBody->RestoreState(pnode);
        }
        else
        {
            parsedFunctionBody->SaveState(pnode);
        }
    }

    funcInfo->SetChildCallsEval(!!pnode->sxFnc.ChildCallsEval());

    if (pnode->sxFnc.CallsEval())
    {LOGMEIN("ByteCodeGenerator.cpp] 1424\n");
        funcInfo->SetCallsEval(true);

        bodyScope->SetIsDynamic(true);
        bodyScope->SetIsObject();
        bodyScope->SetCapturesAll(true);
        bodyScope->SetMustInstantiate(true);

        // Do not mark param scope as dynamic as it does not leak declarations
        paramScope->SetIsObject();
        paramScope->SetCapturesAll(true);
        paramScope->SetMustInstantiate(true);
    }

    PushFuncInfo(_u("StartBindFunction"), funcInfo);

    if (funcExprScope)
    {LOGMEIN("ByteCodeGenerator.cpp] 1441\n");
        funcExprScope->SetFunc(funcInfo);
        funcInfo->funcExprScope = funcExprScope;
    }

    int32 currentAstSize = pnode->sxFnc.astSize;
    if (currentAstSize > this->maxAstSize)
    {LOGMEIN("ByteCodeGenerator.cpp] 1448\n");
        this->maxAstSize = currentAstSize;
    }

    autoRestoreFunctionInfo.Clear();

    return funcInfo;
}

void ByteCodeGenerator::EndBindFunction(bool funcExprWithName)
{LOGMEIN("ByteCodeGenerator.cpp] 1458\n");
    bool isGlobalScope = currentScope->GetScopeType() == ScopeType_Global;

    Assert(currentScope->GetScopeType() == ScopeType_FunctionBody || isGlobalScope);
    PopScope(); // function body

    if (isGlobalScope)
    {LOGMEIN("ByteCodeGenerator.cpp] 1465\n");
        Assert(currentScope == nullptr);
    }
    else
    {
        Assert(currentScope->GetScopeType() == ScopeType_Parameter);
        PopScope(); // parameter scope
    }

    if (funcExprWithName)
    {LOGMEIN("ByteCodeGenerator.cpp] 1475\n");
        Assert(currentScope->GetScopeType() == ScopeType_FuncExpr);
        PopScope();
    }

    funcInfoStack->Pop();
}

void ByteCodeGenerator::StartBindCatch(ParseNode *pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 1484\n");
    Scope *scope = pnode->sxCatch.scope;
    Assert(scope);
    Assert(currentScope);
    scope->SetFunc(currentScope->GetFunc());
    PushScope(scope);
}

void ByteCodeGenerator::EndBindCatch()
{LOGMEIN("ByteCodeGenerator.cpp] 1493\n");
    PopScope();
}

void ByteCodeGenerator::PushScope(Scope *innerScope)
{LOGMEIN("ByteCodeGenerator.cpp] 1498\n");
    Assert(innerScope != nullptr);

    innerScope->SetEnclosingScope(currentScope);

    currentScope = innerScope;

    if (currentScope->GetIsDynamic())
    {LOGMEIN("ByteCodeGenerator.cpp] 1506\n");
        this->dynamicScopeCount++;
    }

    if (this->trackEnvDepth && currentScope->GetMustInstantiate())
    {LOGMEIN("ByteCodeGenerator.cpp] 1511\n");
        this->envDepth++;
        if (this->envDepth == 0)
        {LOGMEIN("ByteCodeGenerator.cpp] 1514\n");
            Js::Throw::OutOfMemory();
        }
    }
}

void ByteCodeGenerator::PopScope()
{LOGMEIN("ByteCodeGenerator.cpp] 1521\n");
    Assert(currentScope != nullptr);
    if (this->trackEnvDepth && currentScope->GetMustInstantiate())
    {LOGMEIN("ByteCodeGenerator.cpp] 1524\n");
        this->envDepth--;
        Assert(this->envDepth != (uint16)-1);
    }
    if (currentScope->GetIsDynamic())
    {LOGMEIN("ByteCodeGenerator.cpp] 1529\n");
        this->dynamicScopeCount--;
    }
    currentScope = currentScope->GetEnclosingScope();
}

void ByteCodeGenerator::PushBlock(ParseNode *pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 1536\n");
    pnode->sxBlock.SetEnclosingBlock(currentBlock);
    currentBlock = pnode;
}

void ByteCodeGenerator::PopBlock()
{LOGMEIN("ByteCodeGenerator.cpp] 1542\n");
    currentBlock = currentBlock->sxBlock.GetEnclosingBlock();
}

void ByteCodeGenerator::PushFuncInfo(char16 const * location, FuncInfo* funcInfo)
{LOGMEIN("ByteCodeGenerator.cpp] 1547\n");
    // We might have multiple global scope for deferparse.
    // Assert(!funcInfo->IsGlobalFunction() || this->TopFuncInfo() == nullptr || this->TopFuncInfo()->IsGlobalFunction());
    if (PHASE_TRACE1(Js::ByteCodePhase))
    {LOGMEIN("ByteCodeGenerator.cpp] 1551\n");
        Output::Print(_u("%s: PushFuncInfo: %s"), location, funcInfo->name);
        if (this->TopFuncInfo())
        {LOGMEIN("ByteCodeGenerator.cpp] 1554\n");
            Output::Print(_u(" Top: %s"), this->TopFuncInfo()->name);
        }
        Output::Print(_u("\n"));
        Output::Flush();
    }
    funcInfoStack->Push(funcInfo);
}

void ByteCodeGenerator::PopFuncInfo(char16 const * location)
{LOGMEIN("ByteCodeGenerator.cpp] 1564\n");
    FuncInfo * funcInfo = funcInfoStack->Pop();
    // Assert(!funcInfo->IsGlobalFunction() || this->TopFuncInfo() == nullptr || this->TopFuncInfo()->IsGlobalFunction());
    if (PHASE_TRACE1(Js::ByteCodePhase))
    {LOGMEIN("ByteCodeGenerator.cpp] 1568\n");
        Output::Print(_u("%s: PopFuncInfo: %s"), location, funcInfo->name);
        if (this->TopFuncInfo())
        {LOGMEIN("ByteCodeGenerator.cpp] 1571\n");
            Output::Print(_u(" Top: %s"), this->TopFuncInfo()->name);
        }
        Output::Print(_u("\n"));
        Output::Flush();
    }
}

Symbol * ByteCodeGenerator::FindSymbol(Symbol **symRef, IdentPtr pid, bool forReference)
{LOGMEIN("ByteCodeGenerator.cpp] 1580\n");
    const char16 *key = nullptr;

    Symbol *sym = nullptr;
    Assert(symRef);
    if (*symRef)
    {LOGMEIN("ByteCodeGenerator.cpp] 1586\n");
        sym = *symRef;
    }
    else
    {
        this->AssignPropertyId(pid);
        return nullptr;
    }
    key = reinterpret_cast<const char16*>(sym->GetPid()->Psz());

    Scope *symScope = sym->GetScope();
    Assert(symScope);

#if DBG_DUMP
    if (this->Trace())
    {LOGMEIN("ByteCodeGenerator.cpp] 1601\n");
        if (sym != nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 1603\n");
            Output::Print(_u("resolved %s to symbol of type %s: \n"), key, sym->GetSymbolTypeName());
        }
        else
        {
            Output::Print(_u("did not resolve %s\n"), key);
        }
    }
#endif

    if (!sym->GetIsGlobal() && !sym->GetIsModuleExportStorage())
    {LOGMEIN("ByteCodeGenerator.cpp] 1614\n");
        FuncInfo *top = funcInfoStack->Top();

        bool nonLocalRef = symScope->GetFunc() != top;
        Scope *scope = nullptr;
        if (forReference)
        {LOGMEIN("ByteCodeGenerator.cpp] 1620\n");
            Js::PropertyId i;
            scope = FindScopeForSym(symScope, nullptr, &i, top);
            // If we have a reference to a local within a with, we want to generate a closure represented by an object.
            if (scope != symScope && scope->GetIsDynamic())
            {LOGMEIN("ByteCodeGenerator.cpp] 1625\n");
                nonLocalRef = true;
                sym->SetHasNonLocalReference();
                symScope->SetIsObject();
            }
        }

        bool didTransferToFncVarSym = false;

        if (!PHASE_OFF(Js::OptimizeBlockScopePhase, top->byteCodeFunction) &&
            sym->GetIsBlockVar() &&
            !sym->GetScope()->IsBlockInLoop() &&
            sym->GetSymbolType() == STFunction)
        {LOGMEIN("ByteCodeGenerator.cpp] 1638\n");
            // Try to use the var-scoped function binding in place of the lexically scoped one.
            // This can be done if neither binding is explicitly assigned to, if there's no ambiguity in the binding
            // (with/eval), and if the function is not declared in a loop. (Loops are problematic, because as the loop
            // iterates different instances can be captured. If we always capture the var-scoped binding, then we
            // always get the latest instance, when we should get the instance belonging to the iteration that captured it.)
            if (sym->GetHasNonLocalReference())
            {LOGMEIN("ByteCodeGenerator.cpp] 1645\n");
                if (!scope)
                {LOGMEIN("ByteCodeGenerator.cpp] 1647\n");
                    Js::PropertyId i;
                    scope = FindScopeForSym(symScope, nullptr, &i, top);
                }
                if (scope == symScope && !scope->GetIsObject())
                {LOGMEIN("ByteCodeGenerator.cpp] 1652\n");
                    Symbol *fncVarSym = sym->GetFuncScopeVarSym();
                    if (fncVarSym &&
                        !fncVarSym->HasBlockFncVarRedecl() &&
                        sym->GetAssignmentState() == NotAssigned &&
                        fncVarSym->GetAssignmentState() == NotAssigned)
                    {LOGMEIN("ByteCodeGenerator.cpp] 1658\n");
                        // Make sure no dynamic scope intrudes between the two bindings.
                        bool foundDynamicScope = false;
                        for (Scope *tmpScope = symScope->GetEnclosingScope(); tmpScope != fncVarSym->GetScope(); tmpScope = tmpScope->GetEnclosingScope())
                        {LOGMEIN("ByteCodeGenerator.cpp] 1662\n");
                            Assert(tmpScope);
                            if (tmpScope->GetIsDynamic())
                            {LOGMEIN("ByteCodeGenerator.cpp] 1665\n");
                                foundDynamicScope = true;
                                break;
                            }
                        }
                        if (!foundDynamicScope)
                        {LOGMEIN("ByteCodeGenerator.cpp] 1671\n");
                            didTransferToFncVarSym = true;
                            sym = fncVarSym;
                            symScope = sym->GetScope();
                            if (nonLocalRef)
                            {LOGMEIN("ByteCodeGenerator.cpp] 1676\n");
                                sym->SetHasNonLocalReference();
                            }
                        }
                    }
                }
            }
        }
        if (!didTransferToFncVarSym)
        {LOGMEIN("ByteCodeGenerator.cpp] 1685\n");
            sym->SetHasRealBlockVarRef();
        }

        // This may not be a non-local reference, but the symbol may still be accessed non-locally. ('with', e.g.)
        // In that case, make sure we still process the symbol and its scope for closure capture.
        if (nonLocalRef || sym->GetHasNonLocalReference())
        {LOGMEIN("ByteCodeGenerator.cpp] 1692\n");
            // Symbol referenced through a closure. Mark it as such and give it a property ID.
            this->ProcessCapturedSym(sym);
            sym->SetPosition(top->byteCodeFunction->GetOrAddPropertyIdTracked(sym->GetName()));
            // If this is var is local to a function (meaning that it belongs to the function's scope
            // *or* to scope that need not be instantiated, like a function expression scope, which we'll
            // merge with the function scope, then indicate that fact.
            this->ProcessScopeWithCapturedSym(symScope);
            if (symScope->GetFunc()->GetHasArguments() && sym->GetIsFormal())
            {LOGMEIN("ByteCodeGenerator.cpp] 1701\n");
                // A formal is referenced non-locally. We need to allocate it on the heap, so
                // do the same for the whole arguments object.

                // Formal is referenced. So count of formals to function > 0.
                // So no need to check for inParams here.

                symScope->GetFunc()->SetHasHeapArguments(true);
            }
            if (symScope->GetFunc() != top)
            {LOGMEIN("ByteCodeGenerator.cpp] 1711\n");
                top->SetHasClosureReference(true);
            }
        }
        else if (!nonLocalRef && sym->GetHasNonLocalReference() && !sym->GetIsCommittedToSlot() && !sym->HasVisitedCapturingFunc())
        {LOGMEIN("ByteCodeGenerator.cpp] 1716\n");
            sym->SetHasNonCommittedReference(true);
        }

        if (sym->GetIsFuncExpr())
        {LOGMEIN("ByteCodeGenerator.cpp] 1721\n");
            symScope->GetFunc()->SetFuncExprNameReference(true);
        }
    }

    return sym;
}

Symbol * ByteCodeGenerator::AddSymbolToScope(Scope *scope, const char16 *key, int keyLength, ParseNode *varDecl, SymbolType symbolType)
{LOGMEIN("ByteCodeGenerator.cpp] 1730\n");
    Symbol *sym = nullptr;

    switch (varDecl->nop)
    {LOGMEIN("ByteCodeGenerator.cpp] 1734\n");
    case knopConstDecl:
    case knopLetDecl:
    case knopVarDecl:
        sym = varDecl->sxVar.sym;
        break;
    case knopName:
        AnalysisAssert(varDecl->sxPid.symRef);
        sym = *varDecl->sxPid.symRef;
        break;
    default:
        AnalysisAssert(0);
        sym = nullptr;
        break;
    }

    if (sym->GetScope() != scope && sym->GetScope()->GetScopeType() != ScopeType_Parameter)
    {LOGMEIN("ByteCodeGenerator.cpp] 1751\n");
        // This can happen when we have a function declared at global eval scope, and it has
        // references in deferred function bodies inside the eval. The BCG creates a new global scope
        // on such compiles, so we essentially have to migrate the symbol to the new scope.
        // We check fscrEvalCode, not fscrEval, because the same thing can happen in indirect eval,
        // when fscrEval is not set.
        Assert(scope->GetScopeType() == ScopeType_Global);
        scope->AddNewSymbol(sym);
    }

    Assert(sym && sym->GetScope() && (sym->GetScope() == scope || sym->GetScope()->GetScopeType() == ScopeType_Parameter));

    return sym;
}

Symbol * ByteCodeGenerator::AddSymbolToFunctionScope(const char16 *key, int keyLength, ParseNode *varDecl, SymbolType symbolType)
{LOGMEIN("ByteCodeGenerator.cpp] 1767\n");
    Scope* scope = currentScope->GetFunc()->GetBodyScope();
    return this->AddSymbolToScope(scope, key, keyLength, varDecl, symbolType);
}

FuncInfo *ByteCodeGenerator::FindEnclosingNonLambda()
{LOGMEIN("ByteCodeGenerator.cpp] 1773\n");
    for (Scope *scope = GetCurrentScope(); scope; scope = scope->GetEnclosingScope())
    {LOGMEIN("ByteCodeGenerator.cpp] 1775\n");
        if (!scope->GetFunc()->IsLambda())
        {LOGMEIN("ByteCodeGenerator.cpp] 1777\n");
            return scope->GetFunc();
        }
    }
    Assert(0);
    return nullptr;
}

FuncInfo* GetParentFuncInfo(FuncInfo* child)
{LOGMEIN("ByteCodeGenerator.cpp] 1786\n");
    for (Scope* scope = child->GetBodyScope(); scope; scope = scope->GetEnclosingScope())
    {LOGMEIN("ByteCodeGenerator.cpp] 1788\n");
        if (scope->GetFunc() != child)
        {LOGMEIN("ByteCodeGenerator.cpp] 1790\n");
            return scope->GetFunc();
        }
    }
    Assert(0);
    return nullptr;
}

bool ByteCodeGenerator::CanStackNestedFunc(FuncInfo * funcInfo, bool trace)
{LOGMEIN("ByteCodeGenerator.cpp] 1799\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
    Assert(!funcInfo->IsGlobalFunction());
    bool const doStackNestedFunc = !funcInfo->HasMaybeEscapedNestedFunc() && !IsInDebugMode()
        && !funcInfo->byteCodeFunction->IsCoroutine()
        && !funcInfo->byteCodeFunction->IsModule();
    if (!doStackNestedFunc)
    {LOGMEIN("ByteCodeGenerator.cpp] 1808\n");
        return false;
    }

    bool callsEval = funcInfo->GetCallsEval() || funcInfo->GetChildCallsEval();
    if (callsEval)
    {LOGMEIN("ByteCodeGenerator.cpp] 1814\n");
        if (trace)
        {LOGMEIN("ByteCodeGenerator.cpp] 1816\n");
            PHASE_PRINT_TESTTRACE(Js::StackFuncPhase, funcInfo->byteCodeFunction,
                _u("HasMaybeEscapedNestedFunc (Eval): %s (function %s)\n"),
                funcInfo->byteCodeFunction->GetDisplayName(),
                funcInfo->byteCodeFunction->GetDebugNumberSet(debugStringBuffer));
        }
        return false;
    }

    if (funcInfo->GetBodyScope()->GetIsObject() || funcInfo->GetParamScope()->GetIsObject())
    {LOGMEIN("ByteCodeGenerator.cpp] 1826\n");
        if (trace)
        {LOGMEIN("ByteCodeGenerator.cpp] 1828\n");
            PHASE_PRINT_TESTTRACE(Js::StackFuncPhase, funcInfo->byteCodeFunction,
                _u("HasMaybeEscapedNestedFunc (ObjectScope): %s (function %s)\n"),
                funcInfo->byteCodeFunction->GetDisplayName(),
                funcInfo->byteCodeFunction->GetDebugNumberSet(debugStringBuffer));
        }
        return false;
    }

    if (funcInfo->paramScope && !funcInfo->paramScope->GetCanMergeWithBodyScope())
    {LOGMEIN("ByteCodeGenerator.cpp] 1838\n");
        if (trace)
        {LOGMEIN("ByteCodeGenerator.cpp] 1840\n");
            PHASE_PRINT_TESTTRACE(Js::StackFuncPhase, funcInfo->byteCodeFunction,
                _u("CanStackNestedFunc: %s (Split Scope)\n"),
                funcInfo->byteCodeFunction->GetDisplayName());
        }
        return false;
    }

    if (trace && funcInfo->byteCodeFunction->GetNestedCount())
    {LOGMEIN("ByteCodeGenerator.cpp] 1849\n");
        // Only print functions that actually have nested functions, although we will still mark
        // functions that don't have nested child functions as DoStackNestedFunc.
        PHASE_PRINT_TESTTRACE(Js::StackFuncPhase, funcInfo->byteCodeFunction,
            _u("DoStackNestedFunc: %s (function %s)\n"),
            funcInfo->byteCodeFunction->GetDisplayName(),
            funcInfo->byteCodeFunction->GetDebugNumberSet(debugStringBuffer));
    }

    return !PHASE_OFF(Js::StackFuncPhase, funcInfo->byteCodeFunction);
}

bool ByteCodeGenerator::NeedObjectAsFunctionScope(FuncInfo * funcInfo, ParseNode * pnodeFnc) const
{LOGMEIN("ByteCodeGenerator.cpp] 1862\n");
    return funcInfo->GetCallsEval()
        || funcInfo->GetChildCallsEval()
        || NeedScopeObjectForArguments(funcInfo, pnodeFnc)
        || (this->flags & (fscrEval | fscrImplicitThis | fscrImplicitParents));
}

Scope * ByteCodeGenerator::FindScopeForSym(Scope *symScope, Scope *scope, Js::PropertyId *envIndex, FuncInfo *funcInfo) const
{LOGMEIN("ByteCodeGenerator.cpp] 1870\n");
    for (scope = scope ? scope->GetEnclosingScope() : currentScope; scope; scope = scope->GetEnclosingScope())
    {LOGMEIN("ByteCodeGenerator.cpp] 1872\n");
        if (scope->GetFunc() != funcInfo
            && scope->GetMustInstantiate()
            && scope != this->globalScope)
        {LOGMEIN("ByteCodeGenerator.cpp] 1876\n");
            (*envIndex)++;
        }
        if (scope == symScope || scope->GetIsDynamic())
        {LOGMEIN("ByteCodeGenerator.cpp] 1880\n");
            break;
        }
    }

    Assert(scope);
    return scope;
}

/* static */
Js::OpCode ByteCodeGenerator::GetStFldOpCode(FuncInfo* funcInfo, bool isRoot, bool isLetDecl, bool isConstDecl, bool isClassMemberInit)
{LOGMEIN("ByteCodeGenerator.cpp] 1891\n");
    return GetStFldOpCode(funcInfo->GetIsStrictMode(), isRoot, isLetDecl, isConstDecl, isClassMemberInit);
}

/* static */
Js::OpCode ByteCodeGenerator::GetScopedStFldOpCode(FuncInfo* funcInfo, bool isConsoleScopeLetConst)
{LOGMEIN("ByteCodeGenerator.cpp] 1897\n");
    if (isConsoleScopeLetConst)
    {LOGMEIN("ByteCodeGenerator.cpp] 1899\n");
        return Js::OpCode::ConsoleScopedStFld;
    }
    return GetScopedStFldOpCode(funcInfo->GetIsStrictMode());
}

/* static */
Js::OpCode ByteCodeGenerator::GetStElemIOpCode(FuncInfo* funcInfo)
{LOGMEIN("ByteCodeGenerator.cpp] 1907\n");
    return GetStElemIOpCode(funcInfo->GetIsStrictMode());
}

bool ByteCodeGenerator::DoJitLoopBodies(FuncInfo *funcInfo) const
{LOGMEIN("ByteCodeGenerator.cpp] 1912\n");
    // Never JIT loop bodies in a function with a try.
    // Otherwise, always JIT loop bodies under /forcejitloopbody.
    // Otherwise, JIT loop bodies unless we're in eval/"new Function" or feature is disabled.

    Assert(funcInfo->byteCodeFunction->IsFunctionParsed());
    Js::FunctionBody* functionBody = funcInfo->byteCodeFunction->GetFunctionBody();

    return functionBody->ForceJITLoopBody() || funcInfo->byteCodeFunction->IsJitLoopBodyPhaseEnabled();
}

void ByteCodeGenerator::Generate(__in ParseNode *pnode, uint32 grfscr, __in ByteCodeGenerator* byteCodeGenerator,
    __inout Js::ParseableFunctionInfo ** ppRootFunc, __in uint sourceIndex,
    __in bool forceNoNative, __in Parser* parser, Js::ScriptFunction **functionRef)
{LOGMEIN("ByteCodeGenerator.cpp] 1926\n");
    Js::ScriptContext * scriptContext = byteCodeGenerator->scriptContext;

#ifdef PROFILE_EXEC
    scriptContext->ProfileBegin(Js::ByteCodePhase);
#endif
    JS_ETW_INTERNAL(EventWriteJSCRIPT_BYTECODEGEN_START(scriptContext, 0));

    ThreadContext * threadContext = scriptContext->GetThreadContext();
    Js::Utf8SourceInfo * utf8SourceInfo = scriptContext->GetSource(sourceIndex);
    byteCodeGenerator->m_utf8SourceInfo = utf8SourceInfo;

    // For dynamic code, just provide a small number since that source info should have very few functions
    // For static code, the nextLocalFunctionId is a good guess of the initial size of the array to minimize reallocs
    SourceContextInfo * sourceContextInfo = utf8SourceInfo->GetSrcInfo()->sourceContextInfo;
    utf8SourceInfo->EnsureInitialized((grfscr & fscrDynamicCode) ? 4 : (sourceContextInfo->nextLocalFunctionId - pnode->sxFnc.functionId));
    sourceContextInfo->EnsureInitialized();

    ArenaAllocator localAlloc(_u("ByteCode"), threadContext->GetPageAllocator(), Js::Throw::OutOfMemory);
    byteCodeGenerator->parser = parser;
    byteCodeGenerator->SetCurrentSourceIndex(sourceIndex);
    byteCodeGenerator->Begin(&localAlloc, grfscr, *ppRootFunc);
    byteCodeGenerator->functionRef = functionRef;
    Visit(pnode, byteCodeGenerator, Bind, AssignRegisters);

    byteCodeGenerator->forceNoNative = forceNoNative;
    byteCodeGenerator->EmitProgram(pnode);

    if (byteCodeGenerator->flags & fscrEval)
    {LOGMEIN("ByteCodeGenerator.cpp] 1955\n");
        // The eval caller's frame always escapes if eval refers to the caller's arguments.
        byteCodeGenerator->GetRootFunc()->GetFunctionBody()->SetFuncEscapes(
            byteCodeGenerator->funcEscapes || pnode->sxProg.m_UsesArgumentsAtGlobal);
    }

#ifdef IR_VIEWER
    if (grfscr & fscrIrDumpEnable)
    {LOGMEIN("ByteCodeGenerator.cpp] 1963\n");
        byteCodeGenerator->GetRootFunc()->GetFunctionBody()->SetIRDumpEnabled(true);
    }
#endif /* IR_VIEWER */

    byteCodeGenerator->CheckDeferParseHasMaybeEscapedNestedFunc();

#ifdef PROFILE_EXEC
    scriptContext->ProfileEnd(Js::ByteCodePhase);
#endif
    JS_ETW_INTERNAL(EventWriteJSCRIPT_BYTECODEGEN_STOP(scriptContext, 0));

#if ENABLE_NATIVE_CODEGEN && defined(ENABLE_PREJIT)
    if (!byteCodeGenerator->forceNoNative && !scriptContext->GetConfig()->IsNoNative()
        && Js::Configuration::Global.flags.Prejit
        && (grfscr & fscrNoPreJit) == 0)
    {LOGMEIN("ByteCodeGenerator.cpp] 1979\n");
        GenerateAllFunctions(scriptContext->GetNativeCodeGenerator(), byteCodeGenerator->GetRootFunc()->GetFunctionBody());
    }
#endif

    if (ppRootFunc)
    {LOGMEIN("ByteCodeGenerator.cpp] 1985\n");
        *ppRootFunc = byteCodeGenerator->GetRootFunc();
    }

#ifdef PERF_COUNTERS
    PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("TestTrace: deferparse - # of func: %d # deferparsed: %d\n"),
        PerfCounter::CodeCounterSet::GetTotalFunctionCounter().GetValue(), PerfCounter::CodeCounterSet::GetDeferredFunctionCounter().GetValue());
#endif
}

void ByteCodeGenerator::CheckDeferParseHasMaybeEscapedNestedFunc()
{LOGMEIN("ByteCodeGenerator.cpp] 1996\n");
    if (!this->parentScopeInfo || (this->parentScopeInfo->GetParent() && this->parentScopeInfo->GetParent()->GetIsGlobalFunc()))
    {LOGMEIN("ByteCodeGenerator.cpp] 1998\n");
        return;
    }

    Assert(CONFIG_FLAG(DeferNested));

    Assert(this->funcInfoStack && !this->funcInfoStack->Empty());

    // Box the stack nested function if we detected new may be escaped use function.
    SList<FuncInfo *>::Iterator i(this->funcInfoStack);
    bool succeed = i.Next();
    Assert(succeed);
    Assert(i.Data()->IsGlobalFunction()); // We always leave a glo on type when defer parsing.
    Assert(!i.Data()->IsRestored());
    succeed = i.Next();
    FuncInfo * top = i.Data();

    Assert(!top->IsGlobalFunction());
    Assert(top->IsRestored());
    Js::FunctionBody * rootFuncBody = this->GetRootFunc()->GetFunctionBody();
    if (!rootFuncBody->DoStackNestedFunc())
    {LOGMEIN("ByteCodeGenerator.cpp] 2019\n");
        top->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("DeferredChild")));
    }
    else
    {
        // We have to wait until it is parsed before we populate the stack nested func parent.
        FuncInfo * parentFunc = top->GetBodyScope()->GetEnclosingFunc();
        if (!parentFunc->IsGlobalFunction())
        {LOGMEIN("ByteCodeGenerator.cpp] 2027\n");
            Assert(parentFunc->byteCodeFunction != rootFuncBody);
            Js::ParseableFunctionInfo * parentFunctionInfo = parentFunc->byteCodeFunction;
            if (parentFunctionInfo->DoStackNestedFunc())
            {LOGMEIN("ByteCodeGenerator.cpp] 2031\n");
                rootFuncBody->SetStackNestedFuncParent(parentFunctionInfo->GetFunctionInfo());
            }
        }
    }

    do
    {LOGMEIN("ByteCodeGenerator.cpp] 2038\n");
        FuncInfo * funcInfo = i.Data();
        Assert(funcInfo->IsRestored());
        Js::ParseableFunctionInfo * parseableFunctionInfo = funcInfo->byteCodeFunction;
        bool didStackNestedFunc = parseableFunctionInfo->DoStackNestedFunc();
        if (!didStackNestedFunc)
        {LOGMEIN("ByteCodeGenerator.cpp] 2044\n");
            return;
        }
        if (!parseableFunctionInfo->IsFunctionBody())
        {LOGMEIN("ByteCodeGenerator.cpp] 2048\n");
            continue;
        }
        Js::FunctionBody * functionBody = funcInfo->GetParsedFunctionBody();
        if (funcInfo->HasMaybeEscapedNestedFunc())
        {LOGMEIN("ByteCodeGenerator.cpp] 2053\n");
            // This should box the rest of the parent functions.
            if (PHASE_TESTTRACE(Js::StackFuncPhase, this->pCurrentFunction))
            {LOGMEIN("ByteCodeGenerator.cpp] 2056\n");
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                Output::Print(_u("DeferParse: box and disable stack function: %s (function %s)\n"),
                    functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer));
                Output::Flush();
            }

            // During the box workflow we reset all the parents of all nested functions and up. If a fault occurs when the stack function
            // is created this will cause further issues when trying to use the function object again. So failing faster seems to make more sense.
            try
            {LOGMEIN("ByteCodeGenerator.cpp] 2067\n");
                Js::StackScriptFunction::Box(functionBody, functionRef);
            }
            catch (Js::OutOfMemoryException)
            {LOGMEIN("ByteCodeGenerator.cpp] 2071\n");
                FailedToBox_OOM_fatal_error((ULONG_PTR)functionBody);
            }

            return;
        }
    }
    while (i.Next());
}

void ByteCodeGenerator::Begin(
    __in ArenaAllocator *alloc,
    __in uint32 grfscr,
    __in Js::ParseableFunctionInfo* pRootFunc)
{LOGMEIN("ByteCodeGenerator.cpp] 2085\n");
    this->alloc = alloc;
    this->flags = grfscr;
    this->pRootFunc = pRootFunc;
    this->pCurrentFunction = pRootFunc ? pRootFunc->GetFunctionBody() : nullptr;
    if (this->pCurrentFunction && this->pCurrentFunction->GetIsGlobalFunc() && IsInNonDebugMode())
    {LOGMEIN("ByteCodeGenerator.cpp] 2091\n");
        // This is the deferred parse case (not due to debug mode), in which case the global function will not be marked to compiled again.
        this->pCurrentFunction = nullptr;
    }

    this->globalScope = nullptr;
    this->currentScope = nullptr;
    this->currentBlock = nullptr;
    this->isBinding = true;
    this->inPrologue = false;
    this->funcEscapes = false;
    this->maxAstSize = 0;
    this->loopDepth = 0;
    this->envDepth = 0;
    this->trackEnvDepth = false;

    this->funcInfoStack = Anew(alloc, SList<FuncInfo*>, alloc);

    // If pRootFunc is not null, this is a deferred parse function
    // so reuse the property record list bound there since some of the symbols could have
    // been bound. If it's null, we need to create a new property record list
    if (pRootFunc != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 2113\n");
        this->propertyRecords = pRootFunc->GetBoundPropertyRecords();
    }
    else
    {
        this->propertyRecords = nullptr;
    }

    Js::FunctionBody *fakeGlobalFunc = scriptContext->GetLibrary()->GetFakeGlobalFuncForUndefer();
    if (fakeGlobalFunc)
    {LOGMEIN("ByteCodeGenerator.cpp] 2123\n");
        fakeGlobalFunc->ClearBoundPropertyRecords();
    }
}

HRESULT GenerateByteCode(__in ParseNode *pnode, __in uint32 grfscr, __in Js::ScriptContext* scriptContext, __inout Js::ParseableFunctionInfo ** ppRootFunc,
                         __in uint sourceIndex, __in bool forceNoNative, __in Parser* parser, __in CompileScriptException *pse, Js::ScopeInfo* parentScopeInfo,
                        Js::ScriptFunction ** functionRef)
{LOGMEIN("ByteCodeGenerator.cpp] 2131\n");
    HRESULT hr = S_OK;
    ByteCodeGenerator byteCodeGenerator(scriptContext, parentScopeInfo);
    BEGIN_TRANSLATE_EXCEPTION_TO_HRESULT_NESTED
    {
        // Main code.
        ByteCodeGenerator::Generate(pnode, grfscr, &byteCodeGenerator, ppRootFunc, sourceIndex, forceNoNative, parser, functionRef);
    }
    END_TRANSLATE_EXCEPTION_TO_HRESULT(hr);

    if (FAILED(hr))
    {LOGMEIN("ByteCodeGenerator.cpp] 2142\n");
        hr = pse->ProcessError(nullptr, hr, nullptr);
    }

    return hr;
}

void BindInstAndMember(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 2150\n");
    Assert(pnode->nop == knopDot);

    BindReference(pnode, byteCodeGenerator);

    ParseNode *right = pnode->sxBin.pnode2;
    Assert(right->nop == knopName);
    byteCodeGenerator->AssignPropertyId(right->sxPid.pid);
    right->sxPid.sym = nullptr;
    right->sxPid.symRef = nullptr;
    right->grfpn |= fpnMemberReference;
}

void BindReference(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 2164\n");
    // Do special reference-op binding so that we can, for instance, handle call from inside "with"
    // where the "this" instance must be found dynamically.

    bool isCallNode = false;
    bool funcEscapes = false;
    switch (pnode->nop)
    {LOGMEIN("ByteCodeGenerator.cpp] 2171\n");
    case knopCall:
        isCallNode = true;
        pnode = pnode->sxCall.pnodeTarget;
        break;
    case knopDelete:
    case knopTypeof:
        pnode = pnode->sxUni.pnode1;
        break;
    case knopDot:
    case knopIndex:
        funcEscapes = true;
        // fall through
    case knopAsg:
        pnode = pnode->sxBin.pnode1;
        break;
    default:
        AssertMsg(0, "Unexpected opcode in BindReference");
        return;
    }

    if (pnode->nop == knopName)
    {LOGMEIN("ByteCodeGenerator.cpp] 2193\n");
        pnode->sxPid.sym = byteCodeGenerator->FindSymbol(pnode->sxPid.symRef, pnode->sxPid.pid, isCallNode);

        if (funcEscapes &&
            pnode->sxPid.sym &&
            pnode->sxPid.sym->GetSymbolType() == STFunction &&
            (!pnode->sxPid.sym->GetIsGlobal() || (byteCodeGenerator->GetFlags() & fscrEval)))
        {LOGMEIN("ByteCodeGenerator.cpp] 2200\n");
            // Dot, index, and scope ops can cause a local function on the LHS to escape.
            // Make sure scopes are not cached in this case.
            byteCodeGenerator->FuncEscapes(pnode->sxPid.sym->GetScope());
        }
    }
}

void MarkFormal(ByteCodeGenerator *byteCodeGenerator, Symbol *formal, bool assignLocation, bool needDeclaration)
{LOGMEIN("ByteCodeGenerator.cpp] 2209\n");
    if (assignLocation)
    {LOGMEIN("ByteCodeGenerator.cpp] 2211\n");
        formal->SetLocation(byteCodeGenerator->NextVarRegister());
    }
    if (needDeclaration)
    {LOGMEIN("ByteCodeGenerator.cpp] 2215\n");
        formal->SetNeedDeclaration(true);
    }
}

void AddArgsToScope(ParseNodePtr pnode, ByteCodeGenerator *byteCodeGenerator, bool assignLocation)
{LOGMEIN("ByteCodeGenerator.cpp] 2221\n");
    Assert(byteCodeGenerator->TopFuncInfo()->varRegsCount == 0);
    Js::ArgSlot pos = 1;
    bool isNonSimpleParameterList = pnode->sxFnc.HasNonSimpleParameterList();

    auto addArgToScope = [&](ParseNode *arg)
    {LOGMEIN("ByteCodeGenerator.cpp] 2227\n");
        if (arg->IsVarLetOrConst())
        {LOGMEIN("ByteCodeGenerator.cpp] 2229\n");
            Symbol *formal = byteCodeGenerator->AddSymbolToScope(byteCodeGenerator->TopFuncInfo()->GetParamScope(),
                reinterpret_cast<const char16*>(arg->sxVar.pid->Psz()),
                arg->sxVar.pid->Cch(),
                arg,
                STFormal);
#if DBG_DUMP
            if (byteCodeGenerator->Trace())
            {LOGMEIN("ByteCodeGenerator.cpp] 2237\n");
                Output::Print(_u("current context has declared arg %s of type %s at position %d\n"), arg->sxVar.pid->Psz(), formal->GetSymbolTypeName(), pos);
            }
#endif

            if (isNonSimpleParameterList)
            {LOGMEIN("ByteCodeGenerator.cpp] 2243\n");
                formal->SetIsNonSimpleParameter(true);
            }

            arg->sxVar.sym = formal;
            MarkFormal(byteCodeGenerator, formal, assignLocation || isNonSimpleParameterList, isNonSimpleParameterList);
        }
        else if (arg->nop == knopParamPattern)
        {LOGMEIN("ByteCodeGenerator.cpp] 2251\n");
            arg->sxParamPattern.location = byteCodeGenerator->NextVarRegister();
        }
        else
        {
            Assert(false);
        }
        UInt16Math::Inc(pos);
    };

    // We process rest separately because the number of in args needs to exclude rest.
    MapFormalsWithoutRest(pnode, addArgToScope);
    byteCodeGenerator->SetNumberOfInArgs(pos);

    if (pnode->sxFnc.pnodeRest != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 2266\n");
        // The rest parameter will always be in a register, regardless of whether it is in a scope slot.
        // We save the assignLocation value for the assert condition below.
        bool assignLocationSave = assignLocation;
        assignLocation = true;

        addArgToScope(pnode->sxFnc.pnodeRest);

        assignLocation = assignLocationSave;
    }

    MapFormalsFromPattern(pnode, addArgToScope);

    Assert(!assignLocation || byteCodeGenerator->TopFuncInfo()->varRegsCount + 1 == pos);
}

void AddVarsToScope(ParseNode *vars, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 2283\n");
    while (vars != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 2285\n");
        Symbol *sym = byteCodeGenerator->AddSymbolToFunctionScope(reinterpret_cast<const char16*>(vars->sxVar.pid->Psz()), vars->sxVar.pid->Cch(), vars, STVariable);

#if DBG_DUMP
        if (sym->GetSymbolType() == STVariable && byteCodeGenerator->Trace())
        {LOGMEIN("ByteCodeGenerator.cpp] 2290\n");
            Output::Print(_u("current context has declared var %s of type %s\n"),
                vars->sxVar.pid->Psz(), sym->GetSymbolTypeName());
        }
#endif

        if (sym->GetIsArguments() || vars->sxVar.pnodeInit == nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 2297\n");
            // LHS's of var decls are usually bound to symbols later, during the Visit/Bind pass,
            // so that things like catch scopes can be taken into account.
            // The exception is "arguments", which always binds to the local scope.
            // We can also bind to the function scope symbol now if there's no init value
            // to assign.
            vars->sxVar.sym = sym;
            if (sym->GetIsArguments())
            {LOGMEIN("ByteCodeGenerator.cpp] 2305\n");
                FuncInfo* funcInfo = byteCodeGenerator->TopFuncInfo();
                funcInfo->SetArgumentsSymbol(sym);

                if (funcInfo->paramScope && !funcInfo->paramScope->GetCanMergeWithBodyScope())
                {LOGMEIN("ByteCodeGenerator.cpp] 2310\n");
                    Symbol* innerArgSym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
                    funcInfo->SetInnerArgumentsSymbol(innerArgSym);
                    byteCodeGenerator->AssignRegister(innerArgSym);
                }
            }

        }
        else
        {
            vars->sxVar.sym = nullptr;
        }
        vars = vars->sxVar.pnodeNext;
    }
}

template <class Fn>
void VisitFncDecls(ParseNode *fns, Fn action)
{LOGMEIN("ByteCodeGenerator.cpp] 2328\n");
    while (fns != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 2330\n");
        switch (fns->nop)
        {LOGMEIN("ByteCodeGenerator.cpp] 2332\n");
        case knopFncDecl:
            action(fns);
            fns = fns->sxFnc.pnodeNext;
            break;

        case knopBlock:
            fns = fns->sxBlock.pnodeNext;
            break;

        case knopCatch:
            fns = fns->sxCatch.pnodeNext;
            break;

        case knopWith:
            fns = fns->sxWith.pnodeNext;
            break;

        default:
            AssertMsg(false, "Unexpected opcode in tree of scopes");
            return;
        }
    }
}

FuncInfo* PreVisitFunction(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, Js::ParseableFunctionInfo *reuseNestedFunc)
{LOGMEIN("ByteCodeGenerator.cpp] 2358\n");
    // Do binding of function name(s), initialize function scope, propagate function-wide properties from
    // the parent (if any).
    FuncInfo* parentFunc = byteCodeGenerator->TopFuncInfo();

    // fIsRoot indicates that this is the root function to be returned to a ParseProcedureText/AddScriptLet/etc. call.
    // In such cases, the global function is just a wrapper around the root function's declaration.
    // We used to assert that this was the only top-level function body, but it's possible to trick
    // "new Function" into compiling more than one function (see WOOB 1121759).
    bool fIsRoot = (!(byteCodeGenerator->GetFlags() & fscrGlobalCode) &&
        parentFunc->IsGlobalFunction() &&
        parentFunc->root->sxFnc.GetTopLevelScope() == pnode);

    const char16 *funcName = Js::Constants::AnonymousFunction;
    uint funcNameLength = Js::Constants::AnonymousFunctionLength;
    uint functionNameOffset = 0;
    bool funcExprWithName = false;

    if (pnode->sxFnc.hint != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 2377\n");
        funcName = reinterpret_cast<const char16*>(pnode->sxFnc.hint);
        funcNameLength = pnode->sxFnc.hintLength;
        functionNameOffset = pnode->sxFnc.hintOffset;
        Assert(funcNameLength != 0 || funcNameLength == (int)wcslen(funcName));
    }
    if (pnode->sxFnc.IsDeclaration() || pnode->sxFnc.IsMethod())
    {LOGMEIN("ByteCodeGenerator.cpp] 2384\n");
        // Class members have the fully qualified name stored in 'hint', no need to replace it.
        if (pnode->sxFnc.pid && !pnode->sxFnc.IsClassMember())
        {LOGMEIN("ByteCodeGenerator.cpp] 2387\n");
            funcName = reinterpret_cast<const char16*>(pnode->sxFnc.pid->Psz());
            funcNameLength = pnode->sxFnc.pid->Cch();
            functionNameOffset = 0;
        }
    }
    else if ((pnode->sxFnc.pnodeName != nullptr) &&
        (pnode->sxFnc.pnodeName->nop == knopVarDecl))
    {LOGMEIN("ByteCodeGenerator.cpp] 2395\n");
        funcName = reinterpret_cast<const char16*>(pnode->sxFnc.pnodeName->sxVar.pid->Psz());
        funcNameLength = pnode->sxFnc.pnodeName->sxVar.pid->Cch();
        functionNameOffset = 0;
        //
        // create the new scope for Function expression only in ES5 mode
        //
        funcExprWithName = true;
    }

    if (byteCodeGenerator->Trace())
    {LOGMEIN("ByteCodeGenerator.cpp] 2406\n");
        Output::Print(_u("function start %s\n"), funcName);
    }

    Assert(pnode->sxFnc.funcInfo == nullptr);
    FuncInfo* funcInfo = pnode->sxFnc.funcInfo = byteCodeGenerator->StartBindFunction(funcName, funcNameLength, functionNameOffset, &funcExprWithName, pnode, reuseNestedFunc);
    funcInfo->byteCodeFunction->SetIsNamedFunctionExpression(funcExprWithName);
    funcInfo->byteCodeFunction->SetIsNameIdentifierRef(pnode->sxFnc.isNameIdentifierRef);
    if (fIsRoot)
    {LOGMEIN("ByteCodeGenerator.cpp] 2415\n");
        byteCodeGenerator->SetRootFuncInfo(funcInfo);
    }

    if (pnode->sxFnc.pnodeBody == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 2420\n");
        // This is a deferred byte code gen, so we're done.
        // Process the formal arguments, even if there's no AST for the body, to support Function.length.
        Js::ArgSlot pos = 1;
        // We skip the rest parameter here because it is not counted towards the in arg count.
        MapFormalsWithoutRest(pnode, [&](ParseNode *pnode) { UInt16Math::Inc(pos); });
        byteCodeGenerator->SetNumberOfInArgs(pos);
        return funcInfo;
    }

    if (pnode->sxFnc.HasReferenceableBuiltInArguments())
    {LOGMEIN("ByteCodeGenerator.cpp] 2431\n");
        // The parser identified that there is a way to reference the built-in 'arguments' variable from this function. So, we
        // need to determine whether we need to create the variable or not. We need to create the variable iff:
        if (pnode->sxFnc.CallsEval())
        {LOGMEIN("ByteCodeGenerator.cpp] 2435\n");
            // 1. eval is called.
            // 2. when the debugging is enabled, since user can seek arguments during breakpoint.
            funcInfo->SetHasArguments(true);
            funcInfo->SetHasHeapArguments(true);
            if (funcInfo->inArgsCount == 0)
            {LOGMEIN("ByteCodeGenerator.cpp] 2441\n");
                // If no formals to function, no need to create the propertyid array
                byteCodeGenerator->AssignNullConstRegister();
            }
        }
        else if (pnode->sxFnc.UsesArguments())
        {LOGMEIN("ByteCodeGenerator.cpp] 2447\n");
            // 3. the function directly references an 'arguments' identifier
            funcInfo->SetHasArguments(true);
            funcInfo->GetParsedFunctionBody()->SetUsesArgumentsObject(true);
            if (pnode->sxFnc.HasHeapArguments())
            {LOGMEIN("ByteCodeGenerator.cpp] 2452\n");
                bool doStackArgsOpt = (!pnode->sxFnc.HasAnyWriteToFormals() || funcInfo->GetIsStrictMode());
#ifdef PERF_HINT
                if (PHASE_TRACE1(Js::PerfHintPhase) && !doStackArgsOpt)
                {LOGMEIN("ByteCodeGenerator.cpp] 2456\n");
                    WritePerfHint(PerfHints::HeapArgumentsDueToWriteToFormals, funcInfo->GetParsedFunctionBody(), 0);
                }
#endif

                //With statements - need scope object to be present.
                if ((doStackArgsOpt && pnode->sxFnc.funcInfo->GetParamScope()->Count() > 1) && (pnode->sxFnc.funcInfo->HasDeferredChild() || (byteCodeGenerator->GetFlags() & fscrEval) ||
                    pnode->sxFnc.HasWithStmt() || byteCodeGenerator->IsInDebugMode() || PHASE_OFF1(Js::StackArgFormalsOptPhase) || PHASE_OFF1(Js::StackArgOptPhase)))
                {LOGMEIN("ByteCodeGenerator.cpp] 2464\n");
                    doStackArgsOpt = false;
#ifdef PERF_HINT
                    if (PHASE_TRACE1(Js::PerfHintPhase))
                    {LOGMEIN("ByteCodeGenerator.cpp] 2468\n");
                        if (pnode->sxFnc.HasWithStmt())
                        {LOGMEIN("ByteCodeGenerator.cpp] 2470\n");
                            WritePerfHint(PerfHints::HasWithBlock, funcInfo->GetParsedFunctionBody(), 0);
                        }

                        if(byteCodeGenerator->GetFlags() & fscrEval)
                        {LOGMEIN("ByteCodeGenerator.cpp] 2475\n");
                            WritePerfHint(PerfHints::SrcIsEval, funcInfo->GetParsedFunctionBody(), 0);
                        }
                    }
#endif
                }
                funcInfo->SetHasHeapArguments(true, !pnode->sxFnc.IsCoroutine() && doStackArgsOpt /*= Optimize arguments in backend*/);
                if (funcInfo->inArgsCount == 0)
                {LOGMEIN("ByteCodeGenerator.cpp] 2483\n");
                    // If no formals to function, no need to create the propertyid array
                    byteCodeGenerator->AssignNullConstRegister();
                }
            }
        }
    }

    Js::FunctionBody* parentFunctionBody = parentFunc->GetParsedFunctionBody();
    if (funcInfo->GetHasArguments() ||
        parentFunctionBody->GetHasOrParentHasArguments())
    {LOGMEIN("ByteCodeGenerator.cpp] 2494\n");
        // The JIT uses this info, for instance, to narrow kills of array operations
        funcInfo->GetParsedFunctionBody()->SetHasOrParentHasArguments(true);
    }
    PreVisitBlock(pnode->sxFnc.pnodeScopes, byteCodeGenerator);
    // If we have arguments, we are going to need locations if the function is in strict mode or we have a non-simple parameter list. This is because we will not create a scope object.
    bool assignLocationForFormals = !(funcInfo->GetHasHeapArguments() && ByteCodeGenerator::NeedScopeObjectForArguments(funcInfo, funcInfo->root));
    AddArgsToScope(pnode, byteCodeGenerator, assignLocationForFormals);

    return funcInfo;
}

void AssignFuncSymRegister(ParseNode * pnode, ByteCodeGenerator * byteCodeGenerator, FuncInfo * callee)
{LOGMEIN("ByteCodeGenerator.cpp] 2507\n");
    // register to hold the allocated function (in enclosing sequence of global statements)
    // TODO: Make the parser identify uses of function decls as RHS's of expressions.
    // Currently they're all marked as used, so they all get permanent (non-temp) registers.
    if (pnode->sxFnc.pnodeName == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 2512\n");
        return;
    }
    Assert(pnode->sxFnc.pnodeName->nop == knopVarDecl);
    Symbol *sym = pnode->sxFnc.pnodeName->sxVar.sym;
    if (sym)
    {LOGMEIN("ByteCodeGenerator.cpp] 2518\n");
        if (!sym->GetIsGlobal() && !(callee->funcExprScope && callee->funcExprScope->GetIsObject()))
        {LOGMEIN("ByteCodeGenerator.cpp] 2520\n");
            // If the func decl is used, we have to give the expression a register to protect against:
            // x.x = function f() {...};
            // x.y = function f() {...};
            // If we let the value reside in the local slot for f, then both assignments will get the
            // second definition.
            if (!pnode->sxFnc.IsDeclaration())
            {LOGMEIN("ByteCodeGenerator.cpp] 2527\n");
                // A named function expression's name belongs to the enclosing scope.
                // In ES5 mode, it is visible only inside the inner function.
                // Allocate a register for the 'name' symbol from an appropriate register namespace.
                if (callee->GetFuncExprNameReference())
                {LOGMEIN("ByteCodeGenerator.cpp] 2532\n");
                    // This is a function expression with a name, but probably doesn't have a use within
                    // the function. If that is the case then allocate a register for LdFuncExpr inside the function
                    // we just finished post-visiting.
                    if (sym->GetLocation() == Js::Constants::NoRegister)
                    {LOGMEIN("ByteCodeGenerator.cpp] 2537\n");
                        sym->SetLocation(callee->NextVarRegister());
                    }
                }
            }
            else
            {
                // Function declaration
                byteCodeGenerator->AssignRegister(sym);
                pnode->location = sym->GetLocation();

                Assert(byteCodeGenerator->GetCurrentScope()->GetFunc() == sym->GetScope()->GetFunc());
                if (byteCodeGenerator->GetCurrentScope()->GetFunc() != sym->GetScope()->GetFunc())
                {LOGMEIN("ByteCodeGenerator.cpp] 2550\n");
                    Assert(GetParentFuncInfo(byteCodeGenerator->GetCurrentScope()->GetFunc()) == sym->GetScope()->GetFunc());
                    sym->GetScope()->SetMustInstantiate(true);
                    byteCodeGenerator->ProcessCapturedSym(sym);
                    sym->GetScope()->GetFunc()->SetHasLocalInClosure(true);
                }

                Symbol * functionScopeVarSym = sym->GetFuncScopeVarSym();
                if (functionScopeVarSym &&
                    !functionScopeVarSym->GetIsGlobal() &&
                    !functionScopeVarSym->IsInSlot(sym->GetScope()->GetFunc()))
                {LOGMEIN("ByteCodeGenerator.cpp] 2561\n");
                    byteCodeGenerator->AssignRegister(functionScopeVarSym);
                }
            }
        }
        else if (!pnode->sxFnc.IsDeclaration())
        {LOGMEIN("ByteCodeGenerator.cpp] 2567\n");
            if (sym->GetLocation() == Js::Constants::NoRegister)
            {LOGMEIN("ByteCodeGenerator.cpp] 2569\n");
                // Here, we are assigning a register for the LdFuncExpr instruction inside the function we just finished
                // post-visiting. The symbol is given a register from the register pool for the function we just finished
                // post-visiting, rather than from the parent function's register pool.
                sym->SetLocation(callee->NextVarRegister());
            }
        }
    }
}

bool FuncAllowsDirectSuper(FuncInfo *funcInfo, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 2580\n");
    if (!funcInfo->IsBaseClassConstructor() && funcInfo->IsClassConstructor())
    {LOGMEIN("ByteCodeGenerator.cpp] 2582\n");
        return true;
    }

    if (funcInfo->IsGlobalFunction() && ((byteCodeGenerator->GetFlags() & fscrEval) != 0))
    {LOGMEIN("ByteCodeGenerator.cpp] 2587\n");
        Js::JavascriptFunction *caller = nullptr;
        if (Js::JavascriptStackWalker::GetCaller(&caller, byteCodeGenerator->GetScriptContext()) && caller->GetFunctionInfo()->GetAllowDirectSuper())
        {LOGMEIN("ByteCodeGenerator.cpp] 2590\n");
            return true;
        }
    }

    return false;
}

FuncInfo* PostVisitFunction(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 2599\n");
    // Assign function-wide registers such as local frame object, closure environment, etc., based on
    // observed attributes. Propagate attributes to the parent function (if any).
    FuncInfo *top = byteCodeGenerator->TopFuncInfo();
    Symbol *sym = pnode->sxFnc.GetFuncSymbol();
    bool funcExprWithName = !top->IsGlobalFunction() && sym && sym->GetIsFuncExpr();

    if (top->IsLambda())
    {LOGMEIN("ByteCodeGenerator.cpp] 2607\n");
        FuncInfo *enclosingNonLambda = byteCodeGenerator->FindEnclosingNonLambda();

        if (enclosingNonLambda->isThisLexicallyCaptured)
        {LOGMEIN("ByteCodeGenerator.cpp] 2611\n");
            top->byteCodeFunction->SetCapturesThis();
        }

        if (enclosingNonLambda->IsGlobalFunction())
        {LOGMEIN("ByteCodeGenerator.cpp] 2616\n");
            top->byteCodeFunction->SetEnclosedByGlobalFunc();
        }

        if (FuncAllowsDirectSuper(enclosingNonLambda, byteCodeGenerator))
        {LOGMEIN("ByteCodeGenerator.cpp] 2621\n");
            top->byteCodeFunction->GetFunctionInfo()->SetAllowDirectSuper();
        }
    }
    else if (FuncAllowsDirectSuper(top, byteCodeGenerator))
    {LOGMEIN("ByteCodeGenerator.cpp] 2626\n");
        top->byteCodeFunction->GetFunctionInfo()->SetAllowDirectSuper();
    }

    // If this is a named function expression and has deferred child, mark has non-local reference.
    if (funcExprWithName)
    {LOGMEIN("ByteCodeGenerator.cpp] 2632\n");
        // If we are reparsing this function due to being in debug mode - we should restore the state of this from the earlier parse
        if (top->byteCodeFunction->IsFunctionParsed() && top->GetParsedFunctionBody()->HasFuncExprNameReference())
        {LOGMEIN("ByteCodeGenerator.cpp] 2635\n");
            top->SetFuncExprNameReference(true);
        }
        if (sym->GetHasNonLocalReference())
        {LOGMEIN("ByteCodeGenerator.cpp] 2639\n");
            // Before doing this, though, make sure there's no local symbol that hides the function name
            // from the nested functions. If a lookup starting at the current local scope finds some symbol
            // other than the func expr, then it's hidden. (See Win8 393618.)
            Assert(CONFIG_FLAG(DeferNested));
            byteCodeGenerator->ProcessCapturedSym(sym);

            if (!top->root->sxFnc.NameIsHidden())
            {LOGMEIN("ByteCodeGenerator.cpp] 2647\n");
                top->SetFuncExprNameReference(true);
                if (pnode->sxFnc.pnodeBody)
                {LOGMEIN("ByteCodeGenerator.cpp] 2650\n");
                    top->GetParsedFunctionBody()->SetFuncExprNameReference(true);
                }
                if (!sym->GetScope()->GetIsObject())
                {LOGMEIN("ByteCodeGenerator.cpp] 2654\n");
                    // The function expression symbol will be emitted in the param/body scope.
                    if (top->GetParamScope())
                    {LOGMEIN("ByteCodeGenerator.cpp] 2657\n");
                        top->GetParamScope()->SetHasOwnLocalInClosure(true);
                    }
                    else
                    {
                        top->GetBodyScope()->SetHasOwnLocalInClosure(true);
                    }
                    top->SetHasLocalInClosure(true);
                }
            }
        }
    }

    if (pnode->nop != knopProg
        && !top->bodyScope->GetIsObject()
        && byteCodeGenerator->NeedObjectAsFunctionScope(top, pnode))
    {LOGMEIN("ByteCodeGenerator.cpp] 2673\n");
        // Even if it wasn't determined during visiting this function that we need a scope object, we still have a few conditions that may require one.
        top->bodyScope->SetIsObject();
        if (!top->paramScope->GetCanMergeWithBodyScope())
        {LOGMEIN("ByteCodeGenerator.cpp] 2677\n");
            // If we have the function inside an eval then access to outer variables should go through scope object.
            // So we set the body scope as object and we need to set the param scope also as object in case of split scope.
            top->paramScope->SetIsObject();
        }
    }

    if (pnode->nop == knopProg
        && top->byteCodeFunction->GetIsStrictMode()
        && (byteCodeGenerator->GetFlags() & fscrEval))
    {LOGMEIN("ByteCodeGenerator.cpp] 2687\n");
        // At global scope inside a strict mode eval, vars will not leak out and require a scope object (along with its parent.)
        top->bodyScope->SetIsObject();
    }

    if (pnode->sxFnc.pnodeBody)
    {LOGMEIN("ByteCodeGenerator.cpp] 2693\n");
        if (!top->IsGlobalFunction())
        {LOGMEIN("ByteCodeGenerator.cpp] 2695\n");
            auto fnProcess =
                [byteCodeGenerator, top](Symbol *const sym)
                {LOGMEIN("ByteCodeGenerator.cpp] 2698\n");
                    if (sym->GetHasNonLocalReference() && !sym->GetIsModuleExportStorage())
                    {LOGMEIN("ByteCodeGenerator.cpp] 2700\n");
                        byteCodeGenerator->ProcessCapturedSym(sym);
                    }
                };

            Scope *bodyScope = top->bodyScope;
            Scope *paramScope = top->paramScope;
            if (paramScope != nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 2708\n");
                if (paramScope->GetHasOwnLocalInClosure())
                {LOGMEIN("ByteCodeGenerator.cpp] 2710\n");
                    paramScope->ForEachSymbol(fnProcess);
                    top->SetHasLocalInClosure(true);
                }
            }

            if (bodyScope->GetHasOwnLocalInClosure())
            {LOGMEIN("ByteCodeGenerator.cpp] 2717\n");
                bodyScope->ForEachSymbol(fnProcess);
                top->SetHasLocalInClosure(true);
            }

            PostVisitBlock(pnode->sxFnc.pnodeBodyScope, byteCodeGenerator);
            PostVisitBlock(pnode->sxFnc.pnodeScopes, byteCodeGenerator);
        }

        if ((byteCodeGenerator->GetFlags() & fscrEvalCode) && top->GetCallsEval())
        {LOGMEIN("ByteCodeGenerator.cpp] 2727\n");
            // Must establish "this" in case nested eval refers to it.
            top->GetParsedFunctionBody()->SetHasThis(true);
        }

        // This function refers to the closure environment if:
        // 1. it has a child function (we'll pass the environment to the constructor when the child is created -
        //      even if it's not needed, it's as cheap as loading "null" from the library);
        // 2. it calls eval (and will use the environment to construct the scope chain to pass to eval);
        // 3. it refers to a local defined in a parent function;
        // 4. it refers to a global and some parent calls eval (which might declare the "global" locally);
        // 5. it refers to a global and we're in an event handler;
        // 6. it refers to a global and the function was declared inside a "with";
        // 7. it refers to a global and we're in an eval expression.
        if (pnode->sxFnc.nestedCount != 0 ||
            top->GetCallsEval() ||
            top->GetHasClosureReference() ||
            ((top->GetHasGlobalRef() &&
            (byteCodeGenerator->InDynamicScope() ||
            (byteCodeGenerator->GetFlags() & (fscrImplicitThis | fscrImplicitParents | fscrEval))))))
        {LOGMEIN("ByteCodeGenerator.cpp] 2747\n");
            byteCodeGenerator->SetNeedEnvRegister();
            if (top->GetIsTopLevelEventHandler())
            {LOGMEIN("ByteCodeGenerator.cpp] 2750\n");
                byteCodeGenerator->AssignThisRegister();
            }
        }

        // This function needs to construct a local frame on the heap if it is not the global function (even in eval) and:
        // 1. it calls eval, which may refer to or declare any locals in this frame;
        // 2. a child calls eval (which may refer to locals through a closure);
        // 3. it uses non-strict mode "arguments", so the arguments have to be put in a closure;
        // 4. it defines a local that is used by a child function (read from a closure).
        // 5. it is a main function that's wrapped in a function expression scope but has locals used through
        //    a closure (used in forReference function call cases in a with for example).
        if (!top->IsGlobalFunction())
        {LOGMEIN("ByteCodeGenerator.cpp] 2763\n");
            if (top->GetCallsEval() ||
                top->GetChildCallsEval() ||
                (top->GetHasArguments() && ByteCodeGenerator::NeedScopeObjectForArguments(top, pnode) && pnode->sxFnc.pnodeParams != nullptr) ||
                top->GetHasLocalInClosure() ||
                (top->funcExprScope && top->funcExprScope->GetMustInstantiate()) ||
                // When we have split scope normally either eval will be present or the GetHasLocalInClosure will be true as one of the formal is
                // captured. But when we force split scope or split scope happens due to some other reasons we have to make sure we allocate frame
                // slot register here.
                (top->paramScope != nullptr && !top->paramScope->GetCanMergeWithBodyScope()))
            {LOGMEIN("ByteCodeGenerator.cpp] 2773\n");
                if (!top->GetCallsEval() && top->GetHasLocalInClosure())
                {LOGMEIN("ByteCodeGenerator.cpp] 2775\n");
                    byteCodeGenerator->AssignFrameSlotsRegister();
                }

                if (top->GetParamScope() && !top->GetParamScope()->GetCanMergeWithBodyScope())
                {LOGMEIN("ByteCodeGenerator.cpp] 2780\n");
                    byteCodeGenerator->AssignParamSlotsRegister();
                }

                if (byteCodeGenerator->NeedObjectAsFunctionScope(top, top->root)
                    || top->bodyScope->GetIsObject()
                    || top->paramScope->GetIsObject())
                {LOGMEIN("ByteCodeGenerator.cpp] 2787\n");
                    byteCodeGenerator->AssignFrameObjRegister();
                }

                // The function also needs to construct a frame display if:
                // 1. it calls eval;
                // 2. it has a child function.
                // 3. When has arguments and in debug mode. So that frame display be there along with frame object register.
                if (top->GetCallsEval() ||
                    pnode->sxFnc.nestedCount != 0
                    || (top->GetHasArguments()
                        && (pnode->sxFnc.pnodeParams != nullptr)
                        && byteCodeGenerator->IsInDebugMode()))
                {LOGMEIN("ByteCodeGenerator.cpp] 2800\n");
                    byteCodeGenerator->SetNeedEnvRegister(); // This to ensure that Env should be there when the FrameDisplay register is there.
                    byteCodeGenerator->AssignFrameDisplayRegister();

                    if (top->GetIsTopLevelEventHandler())
                    {LOGMEIN("ByteCodeGenerator.cpp] 2805\n");
                        byteCodeGenerator->AssignThisRegister();
                    }
                }
            }

            if (top->GetHasArguments())
            {LOGMEIN("ByteCodeGenerator.cpp] 2812\n");
                Symbol *argSym = top->GetArgumentsSymbol();
                Assert(argSym);
                if (argSym)
                {LOGMEIN("ByteCodeGenerator.cpp] 2816\n");
                    Assert(top->bodyScope->GetScopeSlotCount() == 0);
                    Assert(top->argsPlaceHolderSlotCount == 0);
                    byteCodeGenerator->AssignRegister(argSym);
                    uint i = 0;
                    auto setArgScopeSlot = [&](ParseNode *pnodeArg)
                    {LOGMEIN("ByteCodeGenerator.cpp] 2822\n");
                        if (pnodeArg->IsVarLetOrConst())
                        {LOGMEIN("ByteCodeGenerator.cpp] 2824\n");
                            Symbol* sym = pnodeArg->sxVar.sym;
                            if (sym->GetScopeSlot() != Js::Constants::NoProperty)
                            {LOGMEIN("ByteCodeGenerator.cpp] 2827\n");
                                top->argsPlaceHolderSlotCount++; // Same name args appeared before
                            }
                            sym->SetScopeSlot(i);
                        }
                        else if (pnodeArg->nop == knopParamPattern)
                        {LOGMEIN("ByteCodeGenerator.cpp] 2833\n");
                            top->argsPlaceHolderSlotCount++;
                        }
                        i++;
                    };

                    // We need to include the rest as well -as it will get slot assigned.
                    if (ByteCodeGenerator::NeedScopeObjectForArguments(top, pnode))
                    {
                        MapFormals(pnode, setArgScopeSlot);
                        if (argSym->NeedsSlotAlloc(top))
                        {LOGMEIN("ByteCodeGenerator.cpp] 2844\n");
                            Assert(argSym->GetScopeSlot() == Js::Constants::NoProperty);
                            argSym->SetScopeSlot(i++);
                        }
                        MapFormalsFromPattern(pnode, setArgScopeSlot);
                    }

                    top->paramScope->SetScopeSlotCount(i);

                    Assert(top->GetHasHeapArguments());
                    if (ByteCodeGenerator::NeedScopeObjectForArguments(top, pnode)
                        && !pnode->sxFnc.HasNonSimpleParameterList())
                    {LOGMEIN("ByteCodeGenerator.cpp] 2856\n");
                        top->byteCodeFunction->SetHasImplicitArgIns(false);
                    }
                }
            }
        }
        else
        {
            Assert(top->IsGlobalFunction() || pnode->sxFnc.IsModule());
            // eval is called in strict mode
            bool newScopeForEval = (top->byteCodeFunction->GetIsStrictMode() && (byteCodeGenerator->GetFlags() & fscrEval));

            if (newScopeForEval)
            {LOGMEIN("ByteCodeGenerator.cpp] 2869\n");
                byteCodeGenerator->SetNeedEnvRegister();
                byteCodeGenerator->AssignFrameObjRegister();
                byteCodeGenerator->AssignFrameDisplayRegister();
            }
        }

        Assert(!funcExprWithName || sym);
        if (funcExprWithName)
        {LOGMEIN("ByteCodeGenerator.cpp] 2878\n");
            Assert(top->funcExprScope);
            // If the func expr may be accessed via eval, force the func expr scope into an object.
            if (top->GetCallsEval() || top->GetChildCallsEval())
            {LOGMEIN("ByteCodeGenerator.cpp] 2882\n");
                top->funcExprScope->SetIsObject();
            }
            if (top->funcExprScope->GetIsObject())
            {LOGMEIN("ByteCodeGenerator.cpp] 2886\n");
                top->funcExprScope->SetLocation(byteCodeGenerator->NextVarRegister());
            }
        }
    }

    byteCodeGenerator->EndBindFunction(funcExprWithName);

    // If the "child" is the global function, we're done.
    if (top->IsGlobalFunction())
    {LOGMEIN("ByteCodeGenerator.cpp] 2896\n");
        return top;
    }

    if (top->paramScope && top->paramScope->GetCanMergeWithBodyScope())
    {LOGMEIN("ByteCodeGenerator.cpp] 2901\n");
        Scope::MergeParamAndBodyScopes(pnode);
        Scope::RemoveParamScope(pnode);
    }

    FuncInfo* const parentFunc = byteCodeGenerator->TopFuncInfo();

    Js::FunctionBody * parentFunctionBody = parentFunc->byteCodeFunction->GetFunctionBody();
    Assert(parentFunctionBody != nullptr);
    bool const hasAnyDeferredChild = top->HasDeferredChild() || top->IsDeferred();
    bool const hasAnyRedeferrableChild = top->HasRedeferrableChild() || top->IsRedeferrable();
    bool setHasNonLocalReference = parentFunctionBody->HasAllNonLocalReferenced();

    // If we have any deferred child, we need to instantiate the fake global block scope if it is not empty
    if (parentFunc->IsGlobalFunction())
    {LOGMEIN("ByteCodeGenerator.cpp] 2916\n");
        if (byteCodeGenerator->IsEvalWithNoParentScopeInfo())
        {LOGMEIN("ByteCodeGenerator.cpp] 2918\n");
            Scope * globalEvalBlockScope = parentFunc->GetGlobalEvalBlockScope();
            if (globalEvalBlockScope->GetHasOwnLocalInClosure())
            {LOGMEIN("ByteCodeGenerator.cpp] 2921\n");
                globalEvalBlockScope->SetMustInstantiate(true);
            }
            if (hasAnyDeferredChild)
            {LOGMEIN("ByteCodeGenerator.cpp] 2925\n");
                parentFunc->SetHasDeferredChild();
            }
            if (hasAnyRedeferrableChild)
            {LOGMEIN("ByteCodeGenerator.cpp] 2929\n");
                parentFunc->SetHasRedeferrableChild();
            }
        }
    }
    else
    {
        if (setHasNonLocalReference)
        {LOGMEIN("ByteCodeGenerator.cpp] 2937\n");
            // All locals are already marked as non-locals-referenced. Mark the parent as well.
            if (parentFunctionBody->HasSetIsObject())
            {LOGMEIN("ByteCodeGenerator.cpp] 2940\n");
                // Updated the current function, as per the previous stored info.
                parentFunc->GetBodyScope()->SetIsObject();
                parentFunc->GetParamScope()->SetIsObject();
            }
        }

        // Propagate "hasDeferredChild" attribute back to parent.
        if (hasAnyDeferredChild)
        {LOGMEIN("ByteCodeGenerator.cpp] 2949\n");
            Assert(CONFIG_FLAG(DeferNested));
            parentFunc->SetHasDeferredChild();
        }
        if (hasAnyRedeferrableChild)
        {LOGMEIN("ByteCodeGenerator.cpp] 2954\n");
            parentFunc->SetHasRedeferrableChild();
        }

        if (top->ChildHasWith() || pnode->sxFnc.HasWithStmt())
        {LOGMEIN("ByteCodeGenerator.cpp] 2959\n");
            // Parent scopes may contain symbols called inside the with.
            // Current implementation needs the symScope isObject.

            parentFunc->SetChildHasWith();

            if (parentFunc->GetBodyScope()->GetHasOwnLocalInClosure() ||
                (parentFunc->GetParamScope()->GetHasOwnLocalInClosure() &&
                 parentFunc->GetParamScope()->GetCanMergeWithBodyScope()))
            {LOGMEIN("ByteCodeGenerator.cpp] 2968\n");
                parentFunc->GetBodyScope()->SetIsObject();
                // Record this for future use in the no-refresh debugging.
                parentFunctionBody->SetHasSetIsObject(true);
            }

            if (parentFunc->GetParamScope()->GetHasOwnLocalInClosure() &&
                !parentFunc->GetParamScope()->GetCanMergeWithBodyScope())
            {LOGMEIN("ByteCodeGenerator.cpp] 2976\n");
                parentFunc->GetParamScope()->SetIsObject();
                // Record this for future use in the no-refresh debugging.
                parentFunctionBody->SetHasSetIsObject(true);
            }
        }

        // Propagate HasMaybeEscapedNestedFunc
        if (!byteCodeGenerator->CanStackNestedFunc(top, false) ||
            byteCodeGenerator->NeedObjectAsFunctionScope(top, pnode))
        {LOGMEIN("ByteCodeGenerator.cpp] 2986\n");
            parentFunc->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("Child")));
        }
    }

    if (top->GetCallsEval() || top->GetChildCallsEval())
    {LOGMEIN("ByteCodeGenerator.cpp] 2992\n");
        parentFunc->SetChildCallsEval(true);
        ParseNode *currentBlock = byteCodeGenerator->GetCurrentBlock();
        if (currentBlock)
        {LOGMEIN("ByteCodeGenerator.cpp] 2996\n");
            Assert(currentBlock->nop == knopBlock);
            currentBlock->sxBlock.SetChildCallsEval(true);
        }
        parentFunc->SetHasHeapArguments(true);
        setHasNonLocalReference = true;
        parentFunctionBody->SetAllNonLocalReferenced(true);

        Scope * const funcExprScope = top->funcExprScope;
        if (funcExprScope)
        {LOGMEIN("ByteCodeGenerator.cpp] 3006\n");
            // If we have the body scope as an object, the outer function expression scope also needs to be an object to propagate the name.
            funcExprScope->SetIsObject();
        }

        if (parentFunc->inArgsCount == 1)
        {LOGMEIN("ByteCodeGenerator.cpp] 3012\n");
            // If no formals to function, no need to create the propertyid array
            byteCodeGenerator->AssignNullConstRegister();
        }
    }

    if (setHasNonLocalReference && !parentFunctionBody->HasDoneAllNonLocalReferenced())
    {LOGMEIN("ByteCodeGenerator.cpp] 3019\n");
        parentFunc->GetBodyScope()->ForceAllSymbolNonLocalReference(byteCodeGenerator);
        if (!parentFunc->IsGlobalFunction())
        {LOGMEIN("ByteCodeGenerator.cpp] 3022\n");
            parentFunc->GetParamScope()->ForceAllSymbolNonLocalReference(byteCodeGenerator);
        }
        parentFunctionBody->SetHasDoneAllNonLocalReferenced(true);
    }

    if (top->HasSuperReference())
    {LOGMEIN("ByteCodeGenerator.cpp] 3029\n");
        top->AssignSuperRegister();
    }

    if (top->HasDirectSuper())
    {LOGMEIN("ByteCodeGenerator.cpp] 3034\n");
        top->AssignSuperCtorRegister();
    }

    if ((top->root->sxFnc.IsConstructor() && (top->isNewTargetLexicallyCaptured || top->GetCallsEval() || top->GetChildCallsEval())) || top->IsClassConstructor())
    {LOGMEIN("ByteCodeGenerator.cpp] 3039\n");
        if (top->IsBaseClassConstructor())
        {LOGMEIN("ByteCodeGenerator.cpp] 3041\n");
            // Base class constructor may not explicitly reference new.target but we always need to have it in order to construct the 'this' object.
            top->AssignNewTargetRegister();
            // Also must have a register to slot the 'this' object into.
            top->AssignThisRegister();
        }
        else
        {
            // Derived class constructors need to check undefined against explicit return statements.
            top->AssignUndefinedConstRegister();

            top->AssignNewTargetRegister();
            top->AssignThisRegister();

            if (top->GetCallsEval() || top->GetChildCallsEval())
            {LOGMEIN("ByteCodeGenerator.cpp] 3056\n");
                top->SetIsThisLexicallyCaptured();
                top->SetIsNewTargetLexicallyCaptured();
                top->SetIsSuperLexicallyCaptured();
                top->SetIsSuperCtorLexicallyCaptured();
                top->SetHasLocalInClosure(true);
                top->SetHasClosureReference(true);
                top->SetHasCapturedThis();
            }
        }
    }

    AssignFuncSymRegister(pnode, byteCodeGenerator, top);

    if (pnode->sxFnc.pnodeBody && pnode->sxFnc.HasReferenceableBuiltInArguments() && pnode->sxFnc.UsesArguments() &&
        pnode->sxFnc.HasHeapArguments())
    {LOGMEIN("ByteCodeGenerator.cpp] 3072\n");
        bool doStackArgsOpt = top->byteCodeFunction->GetDoBackendArgumentsOptimization();

        bool hasAnyParamInClosure = top->GetHasLocalInClosure() && top->GetParamScope()->GetHasOwnLocalInClosure();

        if ((doStackArgsOpt && top->inArgsCount > 1))
        {LOGMEIN("ByteCodeGenerator.cpp] 3078\n");
            if (doStackArgsOpt && hasAnyParamInClosure)
            {LOGMEIN("ByteCodeGenerator.cpp] 3080\n");
                top->SetHasHeapArguments(true, false /*= Optimize arguments in backend*/);
#ifdef PERF_HINT
                if (PHASE_TRACE1(Js::PerfHintPhase))
                {LOGMEIN("ByteCodeGenerator.cpp] 3084\n");
                    WritePerfHint(PerfHints::HeapArgumentsDueToNonLocalRef, top->GetParsedFunctionBody(), 0);
                }
#endif
            }
            else if (!top->GetHasLocalInClosure())
            {LOGMEIN("ByteCodeGenerator.cpp] 3090\n");
                //Scope object creation instr will be a MOV NULL instruction in the Lowerer - if we still decide to do StackArgs after Globopt phase.
                top->byteCodeFunction->SetDoScopeObjectCreation(false);
            }
        }
    }
    return top;
}

void ByteCodeGenerator::ProcessCapturedSym(Symbol *sym)
{LOGMEIN("ByteCodeGenerator.cpp] 3100\n");
    // The symbol's home function will tell us which child function we're currently processing.
    // This is the one that captures the symbol, from the declaring function's perspective.
    // So based on that information, note either that, (a.) the symbol is committed to the heap from its
    // inception, (b.) the symbol must be committed when the capturing function is instantiated.

    FuncInfo *funcHome = sym->GetScope()->GetFunc();
    FuncInfo *funcChild = funcHome->GetCurrentChildFunction();

    Assert(sym->NeedsSlotAlloc(funcHome) || sym->GetIsGlobal());

    // If this is not a local property, or not all its references can be tracked, or
    // it's not scoped to the function, or we're in debug mode, disable the delayed capture optimization.
    if (funcHome->IsGlobalFunction() ||
        funcHome->GetCallsEval() ||
        funcHome->GetChildCallsEval() ||
        funcChild == nullptr ||
        sym->GetScope() != funcHome->GetBodyScope() ||
        this->IsInDebugMode() ||
        PHASE_OFF(Js::DelayCapturePhase, funcHome->byteCodeFunction))
    {LOGMEIN("ByteCodeGenerator.cpp] 3120\n");
        sym->SetIsCommittedToSlot();
    }

    if (sym->GetIsCommittedToSlot())
    {LOGMEIN("ByteCodeGenerator.cpp] 3125\n");
        return;
    }

    AnalysisAssert(funcChild);
    ParseNode *pnodeChild = funcChild->root;

    Assert(pnodeChild && pnodeChild->nop == knopFncDecl);

    if (pnodeChild->sxFnc.IsDeclaration())
    {LOGMEIN("ByteCodeGenerator.cpp] 3135\n");
        // The capturing function is a declaration but may still be limited to an inner scope.
        Scope *scopeChild = funcHome->GetCurrentChildScope();
        if (scopeChild == sym->GetScope() || scopeChild->GetScopeType() == ScopeType_FunctionBody)
        {LOGMEIN("ByteCodeGenerator.cpp] 3139\n");
            // The symbol is captured on entry to the scope in which it's declared.
            // (Check the scope type separately so that we get the special parameter list and
            // named function expression cases as well.)
            sym->SetIsCommittedToSlot();
            return;
        }
    }

    // There is a chance we can limit the region in which the symbol lives on the heap.
    // Note which function captures the symbol.
    funcChild->AddCapturedSym(sym);
}

void ByteCodeGenerator::ProcessScopeWithCapturedSym(Scope *scope)
{LOGMEIN("ByteCodeGenerator.cpp] 3154\n");
    Assert(scope->GetHasOwnLocalInClosure());

    // (Note: if any catch var is closure-captured, we won't merge the catch scope with the function scope.
    // So don't mark the function scope "has local in closure".)
    FuncInfo *func = scope->GetFunc();
    bool notCatch = scope->GetScopeType() != ScopeType_Catch && scope->GetScopeType() != ScopeType_CatchParamPattern;
    if (scope == func->GetBodyScope() || scope == func->GetParamScope() || (scope->GetCanMerge() && notCatch))
    {LOGMEIN("ByteCodeGenerator.cpp] 3162\n");
        func->SetHasLocalInClosure(true);
    }
    else
    {
        if (scope->HasCrossScopeFuncAssignment())
        {LOGMEIN("ByteCodeGenerator.cpp] 3168\n");
            func->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("InstantiateScopeWithCrossScopeAssignment")));
        }
        scope->SetMustInstantiate(true);
    }
}

void MarkInit(ParseNode* pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 3176\n");
    if (pnode->nop == knopList)
    {LOGMEIN("ByteCodeGenerator.cpp] 3178\n");
        do
        {LOGMEIN("ByteCodeGenerator.cpp] 3180\n");
            MarkInit(pnode->sxBin.pnode1);
            pnode = pnode->sxBin.pnode2;
        }
        while (pnode->nop == knopList);
        MarkInit(pnode);
    }
    else
    {
        Symbol *sym = nullptr;
        ParseNode *pnodeInit = nullptr;
        if (pnode->nop == knopVarDecl)
        {LOGMEIN("ByteCodeGenerator.cpp] 3192\n");
            sym = pnode->sxVar.sym;
            pnodeInit = pnode->sxVar.pnodeInit;
        }
        else if (pnode->nop == knopAsg && pnode->sxBin.pnode1->nop == knopName)
        {LOGMEIN("ByteCodeGenerator.cpp] 3197\n");
            sym = pnode->sxBin.pnode1->sxPid.sym;
            pnodeInit = pnode->sxBin.pnode2;
        }

        if (sym && !sym->GetIsUsed() && pnodeInit)
        {LOGMEIN("ByteCodeGenerator.cpp] 3203\n");
            sym->SetHasInit(true);
            if (sym->HasVisitedCapturingFunc())
            {LOGMEIN("ByteCodeGenerator.cpp] 3206\n");
                sym->SetHasNonCommittedReference(false);
            }
        }
    }
}

void AddFunctionsToScope(ParseNodePtr scope, ByteCodeGenerator * byteCodeGenerator)
{
    VisitFncDecls(scope, [byteCodeGenerator](ParseNode *fn)
    {
        ParseNode *pnodeName = fn->sxFnc.pnodeName;
        if (pnodeName && pnodeName->nop == knopVarDecl && fn->sxFnc.IsDeclaration())
        {LOGMEIN("ByteCodeGenerator.cpp] 3219\n");
            const char16 *fnName = pnodeName->sxVar.pid->Psz();
            if (byteCodeGenerator->Trace())
            {LOGMEIN("ByteCodeGenerator.cpp] 3222\n");
                Output::Print(_u("current context has declared function %s\n"), fnName);
            }
            // In ES6, functions are scoped to the block, which will be the current scope.
            // Pre-ES6, function declarations are scoped to the function body, so get that scope.
            Symbol *sym;
            if (!byteCodeGenerator->GetCurrentScope()->IsGlobalEvalBlockScope())
            {LOGMEIN("ByteCodeGenerator.cpp] 3229\n");
                sym = byteCodeGenerator->AddSymbolToScope(byteCodeGenerator->GetCurrentScope(), fnName, pnodeName->sxVar.pid->Cch(), pnodeName, STFunction);
            }
            else
            {
                sym = byteCodeGenerator->AddSymbolToFunctionScope(fnName, pnodeName->sxVar.pid->Cch(), pnodeName, STFunction);
            }

            pnodeName->sxVar.sym = sym;

            if (sym->GetIsGlobal())
            {LOGMEIN("ByteCodeGenerator.cpp] 3240\n");
                FuncInfo* func = byteCodeGenerator->TopFuncInfo();
                func->SetHasGlobalRef(true);
            }

            if (sym->GetScope() != sym->GetScope()->GetFunc()->GetBodyScope() &&
                sym->GetScope() != sym->GetScope()->GetFunc()->GetParamScope())
            {LOGMEIN("ByteCodeGenerator.cpp] 3247\n");
                sym->SetIsBlockVar(true);
            }
        }
    });
}

template <class PrefixFn, class PostfixFn>
void VisitNestedScopes(ParseNode* pnodeScopeList, ParseNode* pnodeParent, ByteCodeGenerator* byteCodeGenerator,
    PrefixFn prefix, PostfixFn postfix, uint *pIndex, bool breakOnBodyScope = false)
{LOGMEIN("ByteCodeGenerator.cpp] 3257\n");
    // Visit all scopes nested in this scope before visiting this function's statements. This way we have all the
    // attributes of all the inner functions before we assign registers within this function.
    // All the attributes we need to propagate downward should already be recorded by the parser.
    // - call to "eval()"
    // - nested in "with"
    FuncInfo * parentFuncInfo = pnodeParent->sxFnc.funcInfo;
    Js::ParseableFunctionInfo* parentFunc = parentFuncInfo->byteCodeFunction;
    ParseNode* pnodeScope;
    uint i = 0;

    // Cache to restore it back once we come out of current function.
    Js::FunctionBody * pLastReuseFunc = byteCodeGenerator->pCurrentFunction;

    for (pnodeScope = pnodeScopeList; pnodeScope;)
    {LOGMEIN("ByteCodeGenerator.cpp] 3272\n");
        if (breakOnBodyScope && pnodeScope == pnodeParent->sxFnc.pnodeBodyScope)
        {LOGMEIN("ByteCodeGenerator.cpp] 3274\n");
            break;
        }

        switch (pnodeScope->nop)
        {LOGMEIN("ByteCodeGenerator.cpp] 3279\n");
        case knopFncDecl:
        {LOGMEIN("ByteCodeGenerator.cpp] 3281\n");
            if (pLastReuseFunc)
            {LOGMEIN("ByteCodeGenerator.cpp] 3283\n");
                if (!byteCodeGenerator->IsInNonDebugMode())
                {LOGMEIN("ByteCodeGenerator.cpp] 3285\n");
                    // Here we are trying to match the inner sub-tree as well with already created inner function.

                    if ((pLastReuseFunc->GetIsGlobalFunc() && parentFunc->GetIsGlobalFunc())
                        || (!pLastReuseFunc->GetIsGlobalFunc() && !parentFunc->GetIsGlobalFunc()))
                    {LOGMEIN("ByteCodeGenerator.cpp] 3290\n");
                        Assert(pLastReuseFunc->StartInDocument() == pnodeParent->ichMin);
                        Assert(pLastReuseFunc->LengthInChars() == pnodeParent->LengthInCodepoints());
                        Assert(pLastReuseFunc->GetNestedCount() == parentFunc->GetNestedCount());

                        // If the current function is not parsed yet, its function body is not generated yet.
                        // Reset pCurrentFunction to null so that it will not be able re-use anything.
                        Js::FunctionProxy* proxy = pLastReuseFunc->GetNestedFunctionProxy((*pIndex));
                        if (proxy && proxy->IsFunctionBody())
                        {LOGMEIN("ByteCodeGenerator.cpp] 3299\n");
                            byteCodeGenerator->pCurrentFunction = proxy->GetFunctionBody();
                        }
                        else
                        {
                            byteCodeGenerator->pCurrentFunction = nullptr;
                        }
                    }
                }
                else if (!parentFunc->GetIsGlobalFunc())
                {LOGMEIN("ByteCodeGenerator.cpp] 3309\n");
                    // In the deferred parsing mode, we will be reusing the only one function (which is asked when on ::Begin) all inner function will be created.
                    byteCodeGenerator->pCurrentFunction = nullptr;
                }
            }

            Js::ParseableFunctionInfo::NestedArray * parentNestedArray = parentFunc->GetNestedArray();
            Js::ParseableFunctionInfo* reuseNestedFunc = nullptr;
            if (parentNestedArray)
            {LOGMEIN("ByteCodeGenerator.cpp] 3318\n");
                Assert(*pIndex < parentNestedArray->nestedCount);
                Js::FunctionInfo * info = parentNestedArray->functionInfoArray[*pIndex];
                if (info && info->HasParseableInfo())
                {LOGMEIN("ByteCodeGenerator.cpp] 3322\n");
                    reuseNestedFunc = info->GetParseableFunctionInfo();

                    // If parentFunc was redeferred, try to set pCurrentFunction to this FunctionBody,
                    // and cleanup to reparse (as previous cleanup stops at redeferred parentFunc).
                    if (!byteCodeGenerator->IsInNonDebugMode()
                        && !byteCodeGenerator->pCurrentFunction
                        && reuseNestedFunc->IsFunctionBody())
                    {LOGMEIN("ByteCodeGenerator.cpp] 3330\n");
                        byteCodeGenerator->pCurrentFunction = reuseNestedFunc->GetFunctionBody();
                        byteCodeGenerator->pCurrentFunction->CleanupToReparse();
                    }
                }
            }
            PreVisitFunction(pnodeScope, byteCodeGenerator, reuseNestedFunc);
            FuncInfo *funcInfo = pnodeScope->sxFnc.funcInfo;

            parentFuncInfo->OnStartVisitFunction(pnodeScope);

            if (pnodeScope->sxFnc.pnodeBody)
            {LOGMEIN("ByteCodeGenerator.cpp] 3342\n");
                if (!byteCodeGenerator->IsInNonDebugMode() && pLastReuseFunc != nullptr && byteCodeGenerator->pCurrentFunction == nullptr)
                {LOGMEIN("ByteCodeGenerator.cpp] 3344\n");
                    // Patch current non-parsed function's FunctionBodyImpl with the new generated function body.
                    // So that the function object (pointing to the old function body) can able to get to the new one.

                    Js::FunctionProxy* proxy = pLastReuseFunc->GetNestedFunctionProxy((*pIndex));
                    if (proxy && !proxy->IsFunctionBody())
                    {LOGMEIN("ByteCodeGenerator.cpp] 3350\n");
                        proxy->UpdateFunctionBodyImpl(funcInfo->byteCodeFunction->GetFunctionBody());
                    }
                }

                Scope *paramScope = funcInfo->GetParamScope();
                Scope *bodyScope = funcInfo->GetBodyScope();

                BeginVisitBlock(pnodeScope->sxFnc.pnodeScopes, byteCodeGenerator);
                i = 0;
                ParseNodePtr containerScope = pnodeScope->sxFnc.pnodeScopes;

                // Push the param scope
                byteCodeGenerator->PushScope(paramScope);

                if (pnodeScope->sxFnc.HasNonSimpleParameterList() && !paramScope->GetCanMergeWithBodyScope())
                {LOGMEIN("ByteCodeGenerator.cpp] 3366\n");
                    // Set param scope as the current child scope.
                    funcInfo->SetCurrentChildScope(paramScope);
                    Assert(containerScope->nop == knopBlock && containerScope->sxBlock.blockType == Parameter);
                    VisitNestedScopes(containerScope->sxBlock.pnodeScopes, pnodeScope, byteCodeGenerator, prefix, postfix, &i, true);
                    MapFormals(pnodeScope, [&](ParseNode *argNode) { Visit(argNode, byteCodeGenerator, prefix, postfix); });
                }

                // Push the body scope
                byteCodeGenerator->PushScope(bodyScope);
                funcInfo->SetCurrentChildScope(bodyScope);

                PreVisitBlock(pnodeScope->sxFnc.pnodeBodyScope, byteCodeGenerator);
                AddVarsToScope(pnodeScope->sxFnc.pnodeVars, byteCodeGenerator);

                if (!pnodeScope->sxFnc.HasNonSimpleParameterList() || paramScope->GetCanMergeWithBodyScope())
                {
                    VisitNestedScopes(containerScope, pnodeScope, byteCodeGenerator, prefix, postfix, &i);
                    MapFormals(pnodeScope, [&](ParseNode *argNode) { Visit(argNode, byteCodeGenerator, prefix, postfix); });
                }

                if (pnodeScope->sxFnc.HasNonSimpleParameterList())
                {LOGMEIN("ByteCodeGenerator.cpp] 3388\n");
                    byteCodeGenerator->AssignUndefinedConstRegister();

                    if (!paramScope->GetCanMergeWithBodyScope())
                    {LOGMEIN("ByteCodeGenerator.cpp] 3392\n");
                        Assert(pnodeScope->sxFnc.pnodeBodyScope->sxBlock.scope);
                        VisitNestedScopes(pnodeScope->sxFnc.pnodeBodyScope->sxBlock.pnodeScopes, pnodeScope, byteCodeGenerator, prefix, postfix, &i);
                    }
                }

                BeginVisitBlock(pnodeScope->sxFnc.pnodeBodyScope, byteCodeGenerator);

                ParseNode* pnode = pnodeScope->sxFnc.pnodeBody;
                while (pnode->nop == knopList)
                {LOGMEIN("ByteCodeGenerator.cpp] 3402\n");
                    // Check to see whether initializations of locals to "undef" can be skipped.
                    // The logic to do this is cheap - omit the init if we see an init with a value
                    // on the RHS at the top statement level (i.e., not inside a block, try, loop, etc.)
                    // before we see a use. The motivation is to help identify single-def locals in the BE.
                    // Note that this can't be done for globals.
                    byteCodeGenerator->SetCurrentTopStatement(pnode->sxBin.pnode1);
                    Visit(pnode->sxBin.pnode1, byteCodeGenerator, prefix, postfix);
                    if (!funcInfo->GetCallsEval() && !funcInfo->GetChildCallsEval() &&
                        // So that it will not be marked as init thus it will be added to the diagnostics symbols container.
                        !(byteCodeGenerator->ShouldTrackDebuggerMetadata()))
                    {LOGMEIN("ByteCodeGenerator.cpp] 3413\n");
                        MarkInit(pnode->sxBin.pnode1);
                    }
                    pnode = pnode->sxBin.pnode2;
                }
                byteCodeGenerator->SetCurrentTopStatement(pnode);
                Visit(pnode, byteCodeGenerator, prefix, postfix);

                EndVisitBlock(pnodeScope->sxFnc.pnodeBodyScope, byteCodeGenerator);
                EndVisitBlock(pnodeScope->sxFnc.pnodeScopes, byteCodeGenerator);
            }

            if (!pnodeScope->sxFnc.pnodeBody)
            {LOGMEIN("ByteCodeGenerator.cpp] 3426\n");
                // For defer prase scenario push the scopes here
                byteCodeGenerator->PushScope(funcInfo->GetParamScope());
                byteCodeGenerator->PushScope(funcInfo->GetBodyScope());
            }

            if (!parentFuncInfo->IsFakeGlobalFunction(byteCodeGenerator->GetFlags()))
            {LOGMEIN("ByteCodeGenerator.cpp] 3433\n");
                pnodeScope->sxFnc.nestedIndex = *pIndex;
                parentFunc->SetNestedFunc(funcInfo->byteCodeFunction->GetFunctionInfo(), (*pIndex)++, byteCodeGenerator->GetFlags());
            }

            Assert(parentFunc);

            parentFuncInfo->OnEndVisitFunction(pnodeScope);

            PostVisitFunction(pnodeScope, byteCodeGenerator);

            pnodeScope = pnodeScope->sxFnc.pnodeNext;

            byteCodeGenerator->pCurrentFunction = pLastReuseFunc;
            break;
        }

        case knopBlock:
        {
            PreVisitBlock(pnodeScope, byteCodeGenerator);
            bool isMergedScope;
            parentFuncInfo->OnStartVisitScope(pnodeScope->sxBlock.scope, &isMergedScope);
            VisitNestedScopes(pnodeScope->sxBlock.pnodeScopes, pnodeParent, byteCodeGenerator, prefix, postfix, pIndex);
            parentFuncInfo->OnEndVisitScope(pnodeScope->sxBlock.scope, isMergedScope);
            PostVisitBlock(pnodeScope, byteCodeGenerator);

            pnodeScope = pnodeScope->sxBlock.pnodeNext;
            break;
        }

        case knopCatch:
        {
            PreVisitCatch(pnodeScope, byteCodeGenerator);

            if (pnodeScope->sxCatch.pnodeParam->nop == knopParamPattern)
            {LOGMEIN("ByteCodeGenerator.cpp] 3468\n");
                Parser::MapBindIdentifier(pnodeScope->sxCatch.pnodeParam->sxParamPattern.pnode1, [byteCodeGenerator](ParseNodePtr pnode)
                {
                    Assert(pnode->nop == knopLetDecl);
                    pnode->sxVar.sym->SetLocation(byteCodeGenerator->NextVarRegister());
                });

                if (pnodeScope->sxCatch.pnodeParam->sxParamPattern.location == Js::Constants::NoRegister)
                {LOGMEIN("ByteCodeGenerator.cpp] 3476\n");
                    pnodeScope->sxCatch.pnodeParam->sxParamPattern.location = byteCodeGenerator->NextVarRegister();
                }
            }
            else
            {
                Visit(pnodeScope->sxCatch.pnodeParam, byteCodeGenerator, prefix, postfix);
            }

            bool isMergedScope;
            parentFuncInfo->OnStartVisitScope(pnodeScope->sxCatch.scope, &isMergedScope);
            VisitNestedScopes(pnodeScope->sxCatch.pnodeScopes, pnodeParent, byteCodeGenerator, prefix, postfix, pIndex);

            parentFuncInfo->OnEndVisitScope(pnodeScope->sxCatch.scope, isMergedScope);
            PostVisitCatch(pnodeScope, byteCodeGenerator);

            pnodeScope = pnodeScope->sxCatch.pnodeNext;
            break;
        }

        case knopWith:
        {
            PreVisitWith(pnodeScope, byteCodeGenerator);
            bool isMergedScope;
            parentFuncInfo->OnStartVisitScope(pnodeScope->sxWith.scope, &isMergedScope);
            VisitNestedScopes(pnodeScope->sxWith.pnodeScopes, pnodeParent, byteCodeGenerator, prefix, postfix, pIndex);
            parentFuncInfo->OnEndVisitScope(pnodeScope->sxWith.scope, isMergedScope);
            PostVisitWith(pnodeScope, byteCodeGenerator);
            pnodeScope = pnodeScope->sxWith.pnodeNext;
            break;
        }

        default:
            AssertMsg(false, "Unexpected opcode in tree of scopes");
            return;
        }
    }
}

void PreVisitBlock(ParseNode *pnodeBlock, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 3516\n");
    if (!pnodeBlock->sxBlock.scope &&
        !pnodeBlock->sxBlock.HasBlockScopedContent() &&
        !pnodeBlock->sxBlock.GetCallsEval())
    {LOGMEIN("ByteCodeGenerator.cpp] 3520\n");
        // Do nothing here if the block doesn't declare anything or call eval (which may declare something).
        return;
    }

    bool isGlobalEvalBlockScope = false;
    FuncInfo *func = byteCodeGenerator->TopFuncInfo();
    if (func->IsGlobalFunction() &&
        func->root->sxFnc.pnodeScopes == pnodeBlock &&
        byteCodeGenerator->IsEvalWithNoParentScopeInfo())
    {LOGMEIN("ByteCodeGenerator.cpp] 3530\n");
        isGlobalEvalBlockScope = true;
    }
    Assert(!pnodeBlock->sxBlock.scope ||
           isGlobalEvalBlockScope == (pnodeBlock->sxBlock.scope->GetScopeType() == ScopeType_GlobalEvalBlock));

    ArenaAllocator *alloc = byteCodeGenerator->GetAllocator();
    Scope *scope;

    if ((pnodeBlock->sxBlock.blockType == PnodeBlockType::Global && !byteCodeGenerator->IsEvalWithNoParentScopeInfo()) || pnodeBlock->sxBlock.blockType == PnodeBlockType::Function)
    {LOGMEIN("ByteCodeGenerator.cpp] 3540\n");
        scope = byteCodeGenerator->GetCurrentScope();

        if (pnodeBlock->sxBlock.blockType == PnodeBlockType::Function)
        {LOGMEIN("ByteCodeGenerator.cpp] 3544\n");
            AnalysisAssert(pnodeBlock->sxBlock.scope);
            if (pnodeBlock->sxBlock.scope->GetScopeType() == ScopeType_Parameter
                && scope->GetScopeType() == ScopeType_FunctionBody)
            {LOGMEIN("ByteCodeGenerator.cpp] 3548\n");
                scope = scope->GetEnclosingScope();
            }
        }

        pnodeBlock->sxBlock.scope = scope;
    }
    else if (!(pnodeBlock->grfpn & fpnSyntheticNode) || isGlobalEvalBlockScope)
    {LOGMEIN("ByteCodeGenerator.cpp] 3556\n");
        scope = pnodeBlock->sxBlock.scope;
        if (!scope)
        {LOGMEIN("ByteCodeGenerator.cpp] 3559\n");
            scope = Anew(alloc, Scope, alloc,
                         isGlobalEvalBlockScope? ScopeType_GlobalEvalBlock : ScopeType_Block, true);
            pnodeBlock->sxBlock.scope = scope;
        }
        scope->SetFunc(byteCodeGenerator->TopFuncInfo());
        // For now, prevent block scope from being merged with enclosing function scope.
        // Consider optimizing this.
        scope->SetCanMerge(false);

        if (isGlobalEvalBlockScope)
        {LOGMEIN("ByteCodeGenerator.cpp] 3570\n");
            scope->SetIsObject();
        }

        byteCodeGenerator->PushScope(scope);
        byteCodeGenerator->PushBlock(pnodeBlock);
    }
    else
    {
        return;
    }

    Assert(scope && scope == pnodeBlock->sxBlock.scope);

    bool isGlobalScope = (scope->GetEnclosingScope() == nullptr);
    Assert(!isGlobalScope || (pnodeBlock->grfpn & fpnSyntheticNode));

    // If it is the global eval block scope, we don't what function decl to be assigned in the block scope.
    // They should already declared in the global function's scope.
    if (!isGlobalEvalBlockScope && !isGlobalScope)
    {LOGMEIN("ByteCodeGenerator.cpp] 3590\n");
        AddFunctionsToScope(pnodeBlock->sxBlock.pnodeScopes, byteCodeGenerator);
    }

    // We can skip this check by not creating the GlobalEvalBlock above and in Parser::Parse for console eval but that seems to break couple of places
    // as we heavily depend on BlockHasOwnScope function. Once we clean up the creation of GlobalEvalBlock for evals we can clean this as well.
    if (byteCodeGenerator->IsConsoleScopeEval() && isGlobalEvalBlockScope && !isGlobalScope)
    {LOGMEIN("ByteCodeGenerator.cpp] 3597\n");
        AssertMsg(scope->GetEnclosingScope()->GetScopeType() == ScopeType_Global, "Additional scope between Global and GlobalEvalBlock?");
        scope = scope->GetEnclosingScope();
        isGlobalScope = true;
    }

    auto addSymbolToScope = [scope, byteCodeGenerator, isGlobalScope](ParseNode *pnode)
        {LOGMEIN("ByteCodeGenerator.cpp] 3604\n");
            Symbol *sym = byteCodeGenerator->AddSymbolToScope(scope, reinterpret_cast<const char16*>(pnode->sxVar.pid->Psz()), pnode->sxVar.pid->Cch(), pnode, STVariable);
#if DBG_DUMP
        if (sym->GetSymbolType() == STVariable && byteCodeGenerator->Trace())
        {LOGMEIN("ByteCodeGenerator.cpp] 3608\n");
            Output::Print(_u("current context has declared %s %s of type %s\n"),
                sym->GetDecl()->nop == knopLetDecl ? _u("let") : _u("const"),
                pnode->sxVar.pid->Psz(),
                sym->GetSymbolTypeName());
        }
#endif
            sym->SetIsGlobal(isGlobalScope);
            sym->SetIsBlockVar(true);
            sym->SetNeedDeclaration(true);
            pnode->sxVar.sym = sym;
        };

    byteCodeGenerator->IterateBlockScopedVariables(pnodeBlock, addSymbolToScope);
}

void PostVisitBlock(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{
    if (!BlockHasOwnScope(pnode, byteCodeGenerator))
    {LOGMEIN("ByteCodeGenerator.cpp] 3627\n");
        return;
    }

    if (pnode->sxBlock.GetCallsEval() || pnode->sxBlock.GetChildCallsEval() || (byteCodeGenerator->GetFlags() & (fscrEval | fscrImplicitThis | fscrImplicitParents)))
    {LOGMEIN("ByteCodeGenerator.cpp] 3632\n");
        Scope *scope = pnode->sxBlock.scope;
        bool scopeIsEmpty = scope->IsEmpty();
        scope->SetIsObject();
        scope->SetCapturesAll(true);
        scope->SetMustInstantiate(!scopeIsEmpty);
    }

    byteCodeGenerator->PopScope();
    byteCodeGenerator->PopBlock();

    ParseNode *currentBlock = byteCodeGenerator->GetCurrentBlock();
    if (currentBlock && (pnode->sxBlock.GetCallsEval() || pnode->sxBlock.GetChildCallsEval()))
    {LOGMEIN("ByteCodeGenerator.cpp] 3645\n");
        currentBlock->sxBlock.SetChildCallsEval(true);
    }
}

void PreVisitCatch(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 3651\n");
    // Push the catch scope and add the catch expression to it.
    byteCodeGenerator->StartBindCatch(pnode);
    if (pnode->sxCatch.pnodeParam->nop == knopParamPattern)
    {LOGMEIN("ByteCodeGenerator.cpp] 3655\n");
        Parser::MapBindIdentifier(pnode->sxCatch.pnodeParam->sxParamPattern.pnode1, [&](ParseNodePtr item)
        {
            Symbol *sym = item->sxVar.sym;
#if DBG_DUMP
            if (byteCodeGenerator->Trace())
            {LOGMEIN("ByteCodeGenerator.cpp] 3661\n");
                Output::Print(_u("current context has declared catch var %s of type %s\n"),
                    item->sxVar.pid->Psz(), sym->GetSymbolTypeName());
            }
#endif
        });
    }
    else
    {
        Symbol *sym = *pnode->sxCatch.pnodeParam->sxPid.symRef;
        Assert(sym->GetScope() == pnode->sxCatch.scope);
#if DBG_DUMP
        if (byteCodeGenerator->Trace())
        {LOGMEIN("ByteCodeGenerator.cpp] 3674\n");
            Output::Print(_u("current context has declared catch var %s of type %s\n"),
                pnode->sxCatch.pnodeParam->sxPid.pid->Psz(), sym->GetSymbolTypeName());
        }
#endif
        sym->SetIsCatch(true);
        pnode->sxCatch.pnodeParam->sxPid.sym = sym;
    }
    // This call will actually add the nested function symbols to the enclosing function scope (which is what we want).
    AddFunctionsToScope(pnode->sxCatch.pnodeScopes, byteCodeGenerator);
}

void PostVisitCatch(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 3687\n");
    byteCodeGenerator->EndBindCatch();
}

void PreVisitWith(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 3692\n");
    ArenaAllocator *alloc = byteCodeGenerator->GetAllocator();
    Scope *scope = Anew(alloc, Scope, alloc, ScopeType_With);
    scope->SetFunc(byteCodeGenerator->TopFuncInfo());
    scope->SetIsDynamic(true);
    pnode->sxWith.scope = scope;

    byteCodeGenerator->PushScope(scope);
}

void PostVisitWith(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 3703\n");
    byteCodeGenerator->PopScope();
}

void BindFuncSymbol(ParseNode *pnodeFnc, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 3708\n");
    if (pnodeFnc->sxFnc.pnodeName)
    {LOGMEIN("ByteCodeGenerator.cpp] 3710\n");
        Assert(pnodeFnc->sxFnc.pnodeName->nop == knopVarDecl);
        Symbol *sym = pnodeFnc->sxFnc.pnodeName->sxVar.sym;
        FuncInfo* func = byteCodeGenerator->TopFuncInfo();
        if (sym == nullptr || sym->GetIsGlobal())
        {LOGMEIN("ByteCodeGenerator.cpp] 3715\n");
            func->SetHasGlobalRef(true);
        }
    }
}

bool IsMathLibraryId(Js::PropertyId propertyId)
{LOGMEIN("ByteCodeGenerator.cpp] 3722\n");
    return (propertyId >= Js::PropertyIds::abs) && (propertyId <= Js::PropertyIds::fround);
}

bool IsLibraryFunction(ParseNode* expr, Js::ScriptContext* scriptContext)
{LOGMEIN("ByteCodeGenerator.cpp] 3727\n");
    if (expr && expr->nop == knopDot)
    {LOGMEIN("ByteCodeGenerator.cpp] 3729\n");
        ParseNode* lhs = expr->sxBin.pnode1;
        ParseNode* rhs = expr->sxBin.pnode2;
        if ((lhs != nullptr) && (rhs != nullptr) && (lhs->nop == knopName) && (rhs->nop == knopName))
        {LOGMEIN("ByteCodeGenerator.cpp] 3733\n");
            Symbol* lsym = lhs->sxPid.sym;
            if ((lsym == nullptr || lsym->GetIsGlobal()) && lhs->sxPid.PropertyIdFromNameNode() == Js::PropertyIds::Math)
            {LOGMEIN("ByteCodeGenerator.cpp] 3736\n");
                return IsMathLibraryId(rhs->sxPid.PropertyIdFromNameNode());
            }
        }
    }
    return false;
}

struct SymCheck
{
    static const int kMaxInvertedSyms = 8;
    Symbol* syms[kMaxInvertedSyms];
    Symbol* permittedSym;
    int symCount;
    bool result;
    bool cond;

    bool AddSymbol(Symbol* sym)
    {LOGMEIN("ByteCodeGenerator.cpp] 3754\n");
        if (symCount < kMaxInvertedSyms)
        {LOGMEIN("ByteCodeGenerator.cpp] 3756\n");
            syms[symCount++] = sym;
            return true;
        }
        else
        {
            return false;
        }
    }

    bool MatchSymbol(Symbol* sym)
    {LOGMEIN("ByteCodeGenerator.cpp] 3767\n");
        if (sym != permittedSym)
        {LOGMEIN("ByteCodeGenerator.cpp] 3769\n");
            for (int i = 0; i < symCount; i++)
            {LOGMEIN("ByteCodeGenerator.cpp] 3771\n");
                if (sym == syms[i])
                {LOGMEIN("ByteCodeGenerator.cpp] 3773\n");
                    return true;
                }
            }
        }
        return false;
    }

    void Init()
    {LOGMEIN("ByteCodeGenerator.cpp] 3782\n");
        symCount = 0;
        result = true;
    }
};

void CheckInvertableExpr(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, SymCheck* symCheck)
{LOGMEIN("ByteCodeGenerator.cpp] 3789\n");
    if (symCheck->result)
    {LOGMEIN("ByteCodeGenerator.cpp] 3791\n");
        switch (pnode->nop)
        {LOGMEIN("ByteCodeGenerator.cpp] 3793\n");
        case knopName:
            if (symCheck->MatchSymbol(pnode->sxPid.sym))
            {LOGMEIN("ByteCodeGenerator.cpp] 3796\n");
                symCheck->result = false;
            }
            break;
        case knopCall:
        {LOGMEIN("ByteCodeGenerator.cpp] 3801\n");
            ParseNode* callTarget = pnode->sxBin.pnode1;
            if (callTarget != nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 3804\n");
                if (callTarget->nop == knopName)
                {LOGMEIN("ByteCodeGenerator.cpp] 3806\n");
                    Symbol* sym = callTarget->sxPid.sym;
                    if (sym && sym->SingleDef())
                    {LOGMEIN("ByteCodeGenerator.cpp] 3809\n");
                        ParseNode* decl = sym->GetDecl();
                        if (decl == nullptr ||
                            decl->nop != knopVarDecl ||
                            !IsLibraryFunction(decl->sxVar.pnodeInit, byteCodeGenerator->GetScriptContext()))
                        {LOGMEIN("ByteCodeGenerator.cpp] 3814\n");
                            symCheck->result = false;
                        }
                        }
                    else
                    {
                        symCheck->result = false;
                    }
                    }
                else if (callTarget->nop == knopDot)
                {
                    if (!IsLibraryFunction(callTarget, byteCodeGenerator->GetScriptContext()))
                    {LOGMEIN("ByteCodeGenerator.cpp] 3826\n");
                        symCheck->result = false;
                }
                    }
                    }
            else
            {
                symCheck->result = false;
                }
            break;
                       }
        case knopDot:
            if (!IsLibraryFunction(pnode, byteCodeGenerator->GetScriptContext()))
            {LOGMEIN("ByteCodeGenerator.cpp] 3839\n");
                symCheck->result = false;
            }
            break;
        case knopTrue:
        case knopFalse:
        case knopAdd:
        case knopSub:
        case knopDiv:
        case knopMul:
        case knopExpo:
        case knopMod:
        case knopNeg:
        case knopInt:
        case knopFlt:
        case knopLt:
        case knopGt:
        case knopLe:
        case knopGe:
        case knopEq:
        case knopNe:
            break;
        default:
            symCheck->result = false;
            break;
        }
    }
}

bool InvertableExpr(SymCheck* symCheck, ParseNode* expr, ByteCodeGenerator* byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 3869\n");
    symCheck->result = true;
    symCheck->cond = false;
    symCheck->permittedSym = nullptr;
    VisitIndirect<SymCheck>(expr, byteCodeGenerator, symCheck, &CheckInvertableExpr, nullptr);
    return symCheck->result;
}

bool InvertableExprPlus(SymCheck* symCheck, ParseNode* expr, ByteCodeGenerator* byteCodeGenerator, Symbol* permittedSym)
{LOGMEIN("ByteCodeGenerator.cpp] 3878\n");
    symCheck->result = true;
    symCheck->cond = true;
    symCheck->permittedSym = permittedSym;
    VisitIndirect<SymCheck>(expr, byteCodeGenerator, symCheck, &CheckInvertableExpr, nullptr);
    return symCheck->result;
}

void CheckLocalVarDef(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 3887\n");
    Assert(pnode->nop == knopAsg);
    if (pnode->sxBin.pnode1 != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 3890\n");
        ParseNode *lhs = pnode->sxBin.pnode1;
        if (lhs->nop == knopName)
        {LOGMEIN("ByteCodeGenerator.cpp] 3893\n");
            Symbol *sym = lhs->sxPid.sym;
            if (sym != nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 3896\n");
                sym->RecordDef();
            }
        }
    }
}

ParseNode* ConstructInvertedStatement(ParseNode* stmt, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo,
    ParseNode** outerStmtRef)
{LOGMEIN("ByteCodeGenerator.cpp] 3905\n");
    if (stmt == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 3907\n");
            return nullptr;
        }

        ParseNode* cStmt;
    if ((stmt->nop == knopAsg) || (stmt->nop == knopVarDecl))
    {LOGMEIN("ByteCodeGenerator.cpp] 3913\n");
        ParseNode* rhs = nullptr;
        ParseNode* lhs = nullptr;

        if (stmt->nop == knopAsg)
        {LOGMEIN("ByteCodeGenerator.cpp] 3918\n");
            rhs = stmt->sxBin.pnode2;
            lhs = stmt->sxBin.pnode1;
            }
        else if (stmt->nop == knopVarDecl)
        {LOGMEIN("ByteCodeGenerator.cpp] 3923\n");
            rhs = stmt->sxVar.pnodeInit;
            }
        ArenaAllocator* alloc = byteCodeGenerator->GetAllocator();
        ParseNode* loopInvar = byteCodeGenerator->GetParser()->CreateTempNode(rhs);
        loopInvar->location = funcInfo->NextVarRegister();

            // Can't use a temp register here because the inversion happens at the parse tree level without generating
        // any bytecode yet. All local non-temp registers need to be initialized for jitted loop bodies, and since this is
            // not a user variable, track this register separately to have it be initialized at the top of the function.
            funcInfo->nonUserNonTempRegistersToInitialize.Add(loopInvar->location);

            // add temp node to list of initializers for new outer loop
        if ((*outerStmtRef)->sxBin.pnode1 == nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 3937\n");
            (*outerStmtRef)->sxBin.pnode1 = loopInvar;
            }
        else
        {
            ParseNode* listNode = Parser::StaticCreateBinNode(knopList, nullptr, nullptr, alloc);
            (*outerStmtRef)->sxBin.pnode2 = listNode;
            listNode->sxBin.pnode1 = loopInvar;
            *outerStmtRef = listNode;
            }

        ParseNode* tempName = byteCodeGenerator->GetParser()->CreateTempRef(loopInvar);

        if (lhs != nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 3951\n");
            cStmt = Parser::StaticCreateBinNode(knopAsg, lhs, tempName, alloc);
            }
        else
        {
                // Use AddVarDeclNode to add the var to the function.
                // Do not use CreateVarDeclNode which is meant to be used while parsing. It assumes that
                // parser's internal data structures (m_ppnodeVar in particular) is at the "current" location.
            cStmt = byteCodeGenerator->GetParser()->AddVarDeclNode(stmt->sxVar.pid, funcInfo->root);
            cStmt->sxVar.pnodeInit = tempName;
            cStmt->sxVar.sym = stmt->sxVar.sym;
            }
        }
    else
    {
        cStmt = byteCodeGenerator->GetParser()->CopyPnode(stmt);
        }

        return cStmt;
}

ParseNode* ConstructInvertedLoop(ParseNode* innerLoop, ParseNode* outerLoop, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{LOGMEIN("ByteCodeGenerator.cpp] 3973\n");
    ArenaAllocator* alloc = byteCodeGenerator->GetAllocator();
    ParseNode* outerLoopC = Parser::StaticCreateNodeT<knopFor>(alloc);
    outerLoopC->sxFor.pnodeInit = innerLoop->sxFor.pnodeInit;
    outerLoopC->sxFor.pnodeCond = innerLoop->sxFor.pnodeCond;
    outerLoopC->sxFor.pnodeIncr = innerLoop->sxFor.pnodeIncr;
    outerLoopC->sxFor.pnodeBlock = innerLoop->sxFor.pnodeBlock;
    outerLoopC->sxFor.pnodeInverted = nullptr;

    ParseNode* innerLoopC = Parser::StaticCreateNodeT<knopFor>(alloc);
    innerLoopC->sxFor.pnodeInit = outerLoop->sxFor.pnodeInit;
    innerLoopC->sxFor.pnodeCond = outerLoop->sxFor.pnodeCond;
    innerLoopC->sxFor.pnodeIncr = outerLoop->sxFor.pnodeIncr;
    innerLoopC->sxFor.pnodeBlock = outerLoop->sxFor.pnodeBlock;
    innerLoopC->sxFor.pnodeInverted = nullptr;

    ParseNode* innerBod = Parser::StaticCreateBlockNode(alloc);
    innerLoopC->sxFor.pnodeBody = innerBod;
    innerBod->sxBlock.scope = innerLoop->sxFor.pnodeBody->sxBlock.scope;

    ParseNode* outerBod = Parser::StaticCreateBlockNode(alloc);
    outerLoopC->sxFor.pnodeBody = outerBod;
    outerBod->sxBlock.scope = outerLoop->sxFor.pnodeBody->sxBlock.scope;

    ParseNode* listNode = Parser::StaticCreateBinNode(knopList, nullptr, nullptr, alloc);
    outerBod->sxBlock.pnodeStmt = listNode;

    ParseNode* innerBodOriginal = innerLoop->sxFor.pnodeBody;
    ParseNode* origStmt = innerBodOriginal->sxBlock.pnodeStmt;
    if (origStmt->nop == knopList)
    {LOGMEIN("ByteCodeGenerator.cpp] 4003\n");
        ParseNode* invertedStmt = nullptr;
        while (origStmt->nop == knopList)
        {LOGMEIN("ByteCodeGenerator.cpp] 4006\n");
            ParseNode* invertedItem = ConstructInvertedStatement(origStmt->sxBin.pnode1, byteCodeGenerator, funcInfo, &listNode);
            if (invertedStmt != nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 4009\n");
                invertedStmt = invertedStmt->sxBin.pnode2 = byteCodeGenerator->GetParser()->CreateBinNode(knopList, invertedItem, nullptr);
            }
            else
            {
                invertedStmt = innerBod->sxBlock.pnodeStmt = byteCodeGenerator->GetParser()->CreateBinNode(knopList, invertedItem, nullptr);
            }
            origStmt = origStmt->sxBin.pnode2;
        }
        Assert(invertedStmt != nullptr);
        invertedStmt->sxBin.pnode2 = ConstructInvertedStatement(origStmt, byteCodeGenerator, funcInfo, &listNode);
    }
    else
    {
        innerBod->sxBlock.pnodeStmt = ConstructInvertedStatement(origStmt, byteCodeGenerator, funcInfo, &listNode);
    }

    if (listNode->sxBin.pnode1 == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 4027\n");
        listNode->sxBin.pnode1 = byteCodeGenerator->GetParser()->CreateTempNode(nullptr);
    }

    listNode->sxBin.pnode2 = innerLoopC;
    return outerLoopC;
}

bool InvertableStmt(ParseNode* stmt, Symbol* outerVar, ParseNode* innerLoop, ParseNode* outerLoop, ByteCodeGenerator* byteCodeGenerator, SymCheck* symCheck)
{LOGMEIN("ByteCodeGenerator.cpp] 4036\n");
    if (stmt != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 4038\n");
        ParseNode* lhs = nullptr;
        ParseNode* rhs = nullptr;
        if (stmt->nop == knopAsg)
        {LOGMEIN("ByteCodeGenerator.cpp] 4042\n");
            lhs = stmt->sxBin.pnode1;
            rhs = stmt->sxBin.pnode2;
        }
        else if (stmt->nop == knopVarDecl)
        {LOGMEIN("ByteCodeGenerator.cpp] 4047\n");
            rhs = stmt->sxVar.pnodeInit;
        }

        if (lhs != nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 4052\n");
            if (lhs->nop == knopDot)
            {LOGMEIN("ByteCodeGenerator.cpp] 4054\n");
                return false;
            }

            if (lhs->nop == knopName)
            {LOGMEIN("ByteCodeGenerator.cpp] 4059\n");
                if ((lhs->sxPid.sym != nullptr) && (lhs->sxPid.sym->GetIsGlobal()))
                {LOGMEIN("ByteCodeGenerator.cpp] 4061\n");
                    return false;
                }
            }
            else if (lhs->nop == knopIndex)
            {LOGMEIN("ByteCodeGenerator.cpp] 4066\n");
                ParseNode* indexed = lhs->sxBin.pnode1;
                ParseNode* index = lhs->sxBin.pnode2;

                if ((index == nullptr) || (indexed == nullptr))
                {LOGMEIN("ByteCodeGenerator.cpp] 4071\n");
                    return false;
                }

                if ((indexed->nop != knopName) || (indexed->sxPid.sym == nullptr))
                {LOGMEIN("ByteCodeGenerator.cpp] 4076\n");
                    return false;
                }

                if (!InvertableExprPlus(symCheck, index, byteCodeGenerator, outerVar))
                {LOGMEIN("ByteCodeGenerator.cpp] 4081\n");
                    return false;
                }
            }
        }

        if (rhs != nullptr)
        {
            if (!InvertableExpr(symCheck, rhs, byteCodeGenerator))
            {LOGMEIN("ByteCodeGenerator.cpp] 4090\n");
                return false;
            }
        }
        else
        {
            if (!InvertableExpr(symCheck, stmt, byteCodeGenerator))
            {LOGMEIN("ByteCodeGenerator.cpp] 4097\n");
                return false;
            }
        }

        return true;
    }

    return false;
}

bool GatherInversionSyms(ParseNode* stmt, Symbol* outerVar, ParseNode* innerLoop, ByteCodeGenerator* byteCodeGenerator, SymCheck* symCheck)
{LOGMEIN("ByteCodeGenerator.cpp] 4109\n");
    if (stmt != nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 4111\n");
        ParseNode* lhs = nullptr;
        Symbol* auxSym = nullptr;

        if (stmt->nop == knopAsg)
        {LOGMEIN("ByteCodeGenerator.cpp] 4116\n");
            lhs = stmt->sxBin.pnode1;
        }
        else if (stmt->nop == knopVarDecl)
        {LOGMEIN("ByteCodeGenerator.cpp] 4120\n");
            auxSym = stmt->sxVar.sym;
        }

        if (lhs != nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 4125\n");
            if (lhs->nop == knopDot)
            {LOGMEIN("ByteCodeGenerator.cpp] 4127\n");
                return false;
            }

            if (lhs->nop == knopName)
            {LOGMEIN("ByteCodeGenerator.cpp] 4132\n");
                if ((lhs->sxPid.sym == nullptr) || (lhs->sxPid.sym->GetIsGlobal()))
                {LOGMEIN("ByteCodeGenerator.cpp] 4134\n");
                    return false;
                }
                else
                {
                    auxSym = lhs->sxPid.sym;
                }
            }
        }

        if (auxSym != nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 4145\n");
            return symCheck->AddSymbol(auxSym);
        }
    }

    return true;
}

bool InvertableBlock(ParseNode* block, Symbol* outerVar, ParseNode* innerLoop, ParseNode* outerLoop, ByteCodeGenerator* byteCodeGenerator,
    SymCheck* symCheck)
{LOGMEIN("ByteCodeGenerator.cpp] 4155\n");
    if (block == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 4157\n");
            return false;
        }

    if (!symCheck->AddSymbol(outerVar))
    {LOGMEIN("ByteCodeGenerator.cpp] 4162\n");
            return false;
        }

        if ((innerLoop->sxFor.pnodeBody->nop == knopBlock && innerLoop->sxFor.pnodeBody->sxBlock.HasBlockScopedContent())
            || (outerLoop->sxFor.pnodeBody->nop == knopBlock && outerLoop->sxFor.pnodeBody->sxBlock.HasBlockScopedContent()))
        {LOGMEIN("ByteCodeGenerator.cpp] 4168\n");
            // we can not invert loops if there are block scoped declarations inside
            return false;
        }

    if ((block != nullptr) && (block->nop == knopBlock))
    {LOGMEIN("ByteCodeGenerator.cpp] 4174\n");
        ParseNode* stmt = block->sxBlock.pnodeStmt;
        while ((stmt != nullptr) && (stmt->nop == knopList))
        {LOGMEIN("ByteCodeGenerator.cpp] 4177\n");
            if (!GatherInversionSyms(stmt->sxBin.pnode1, outerVar, innerLoop, byteCodeGenerator, symCheck))
            {LOGMEIN("ByteCodeGenerator.cpp] 4179\n");
                    return false;
                }
            stmt = stmt->sxBin.pnode2;
            }

        if (!GatherInversionSyms(stmt, outerVar, innerLoop, byteCodeGenerator, symCheck))
        {LOGMEIN("ByteCodeGenerator.cpp] 4186\n");
                return false;
            }

        stmt = block->sxBlock.pnodeStmt;
        while ((stmt != nullptr) && (stmt->nop == knopList))
        {LOGMEIN("ByteCodeGenerator.cpp] 4192\n");
            if (!InvertableStmt(stmt->sxBin.pnode1, outerVar, innerLoop, outerLoop, byteCodeGenerator, symCheck))
            {LOGMEIN("ByteCodeGenerator.cpp] 4194\n");
                    return false;
                }
            stmt = stmt->sxBin.pnode2;
            }

        if (!InvertableStmt(stmt, outerVar, innerLoop, outerLoop, byteCodeGenerator, symCheck))
        {LOGMEIN("ByteCodeGenerator.cpp] 4201\n");
                return false;
            }

        return (InvertableExprPlus(symCheck, innerLoop->sxFor.pnodeCond, byteCodeGenerator, nullptr) &&
            InvertableExprPlus(symCheck, outerLoop->sxFor.pnodeCond, byteCodeGenerator, outerVar));
        }
    else
    {
            return false;
        }
}

// Start of invert loop optimization.
// For now, find simple cases (only for loops around single assignment).
// Returns new AST for inverted loop; also returns in out param
// side effects level, if any that guards the new AST (old AST will be
// used if guard fails).
// Should only be called with loopNode representing top-level statement.
ParseNode* InvertLoop(ParseNode* outerLoop, ByteCodeGenerator* byteCodeGenerator, FuncInfo* funcInfo)
{LOGMEIN("ByteCodeGenerator.cpp] 4221\n");
    if (byteCodeGenerator->GetScriptContext()->optimizationOverrides.GetSideEffects() != Js::SideEffects_None)
    {LOGMEIN("ByteCodeGenerator.cpp] 4223\n");
        return nullptr;
    }

    SymCheck symCheck;
    symCheck.Init();

    if (outerLoop->nop == knopFor)
    {LOGMEIN("ByteCodeGenerator.cpp] 4231\n");
        ParseNode* innerLoop = outerLoop->sxFor.pnodeBody;
        if ((innerLoop == nullptr) || (innerLoop->nop != knopBlock))
        {LOGMEIN("ByteCodeGenerator.cpp] 4234\n");
            return nullptr;
        }
        else
        {
            innerLoop = innerLoop->sxBlock.pnodeStmt;
        }

        if ((innerLoop != nullptr) && (innerLoop->nop == knopFor))
        {LOGMEIN("ByteCodeGenerator.cpp] 4243\n");
            if ((outerLoop->sxFor.pnodeInit != nullptr) &&
                (outerLoop->sxFor.pnodeInit->nop == knopVarDecl) &&
                (outerLoop->sxFor.pnodeInit->sxVar.pnodeInit != nullptr) &&
                (outerLoop->sxFor.pnodeInit->sxVar.pnodeInit->nop == knopInt) &&
                (outerLoop->sxFor.pnodeIncr != nullptr) &&
                ((outerLoop->sxFor.pnodeIncr->nop == knopIncPre) || (outerLoop->sxFor.pnodeIncr->nop == knopIncPost)) &&
                (outerLoop->sxFor.pnodeIncr->sxUni.pnode1->nop == knopName) &&
                (outerLoop->sxFor.pnodeInit->sxVar.pid == outerLoop->sxFor.pnodeIncr->sxUni.pnode1->sxPid.pid) &&
                (innerLoop->sxFor.pnodeIncr != nullptr) &&
                ((innerLoop->sxFor.pnodeIncr->nop == knopIncPre) || (innerLoop->sxFor.pnodeIncr->nop == knopIncPost)) &&
                (innerLoop->sxFor.pnodeInit != nullptr) &&
                (innerLoop->sxFor.pnodeInit->nop == knopVarDecl) &&
                (innerLoop->sxFor.pnodeInit->sxVar.pnodeInit != nullptr) &&
                (innerLoop->sxFor.pnodeInit->sxVar.pnodeInit->nop == knopInt) &&
                (innerLoop->sxFor.pnodeIncr->sxUni.pnode1->nop == knopName) &&
                (innerLoop->sxFor.pnodeInit->sxVar.pid == innerLoop->sxFor.pnodeIncr->sxUni.pnode1->sxPid.pid))
            {LOGMEIN("ByteCodeGenerator.cpp] 4260\n");
                Symbol* outerVar = outerLoop->sxFor.pnodeInit->sxVar.sym;
                Symbol* innerVar = innerLoop->sxFor.pnodeInit->sxVar.sym;
                if ((outerVar != nullptr) && (innerVar != nullptr))
                {LOGMEIN("ByteCodeGenerator.cpp] 4264\n");
                    ParseNode* block = innerLoop->sxFor.pnodeBody;
                    if (InvertableBlock(block, outerVar, innerLoop, outerLoop, byteCodeGenerator, &symCheck))
                    {LOGMEIN("ByteCodeGenerator.cpp] 4267\n");
                        return ConstructInvertedLoop(innerLoop, outerLoop, byteCodeGenerator, funcInfo);
                        }
                    }
            }
        }
    }

    return nullptr;
}

void SetAdditionalBindInfoForVariables(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 4279\n");
    Symbol *sym = pnode->sxVar.sym;
    if (sym == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 4282\n");
        return;
    }

    FuncInfo* func = byteCodeGenerator->TopFuncInfo();
    if (func->IsGlobalFunction())
    {LOGMEIN("ByteCodeGenerator.cpp] 4288\n");
        func->SetHasGlobalRef(true);
    }

    if (!sym->GetIsGlobal() && !sym->GetIsArguments() &&
        (sym->GetScope() == func->GetBodyScope() || sym->GetScope() == func->GetParamScope() || sym->GetScope()->GetCanMerge()))
    {LOGMEIN("ByteCodeGenerator.cpp] 4294\n");
        if (func->GetChildCallsEval())
        {LOGMEIN("ByteCodeGenerator.cpp] 4296\n");
            func->SetHasLocalInClosure(true);
        }
        else
        {
            sym->RecordDef();
        }
    }

    // If this decl does an assignment inside a loop body, then there's a chance
    // that a jitted loop body will expect us to begin with a valid value in this var.
    // So mark the sym as used so that we guarantee the var will at least get "undefined".
    if (byteCodeGenerator->IsInLoop() &&
        pnode->sxVar.pnodeInit)
    {LOGMEIN("ByteCodeGenerator.cpp] 4310\n");
        sym->SetIsUsed(true);
    }
}

// bind references to definitions (prefix pass)
void Bind(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 4317\n");
    if (pnode == nullptr)
{LOGMEIN("ByteCodeGenerator.cpp] 4319\n");
        return;
    }

    switch (pnode->nop)
    {LOGMEIN("ByteCodeGenerator.cpp] 4324\n");
    case knopBreak:
    case knopContinue:
        byteCodeGenerator->AddTargetStmt(pnode->sxJump.pnodeTarget);
        break;
    case knopProg:
        {LOGMEIN("ByteCodeGenerator.cpp] 4330\n");
            FuncInfo* globFuncInfo = byteCodeGenerator->StartBindGlobalStatements(pnode);
            pnode->sxFnc.funcInfo = globFuncInfo;
            AddFunctionsToScope(pnode->sxFnc.GetTopLevelScope(), byteCodeGenerator);
            AddVarsToScope(pnode->sxFnc.pnodeVars, byteCodeGenerator);
            // There are no args to add, but "eval" gets a this pointer.
            byteCodeGenerator->SetNumberOfInArgs(!!(byteCodeGenerator->GetFlags() & fscrEvalCode));
            if (!globFuncInfo->IsFakeGlobalFunction(byteCodeGenerator->GetFlags()))
            {LOGMEIN("ByteCodeGenerator.cpp] 4338\n");
                // Global code: the root function is the global function.
                byteCodeGenerator->SetRootFuncInfo(globFuncInfo);
            }
            else if (globFuncInfo->byteCodeFunction)
            {LOGMEIN("ByteCodeGenerator.cpp] 4343\n");
            // If the current global code wasn't marked to be treated as global code (e.g. from deferred parsing),
            // we don't need to send a register script event for it.
                globFuncInfo->byteCodeFunction->SetIsTopLevel(false);
            }
            if (pnode->sxFnc.CallsEval())
            {LOGMEIN("ByteCodeGenerator.cpp] 4349\n");
                globFuncInfo->SetCallsEval(true);
            }
            break;
        }
    case knopFncDecl:
        // VisitFunctionsInScope has already done binding within the declared function. Here, just record the fact
        // that the parent function has a local/global declaration in it.
        BindFuncSymbol(pnode, byteCodeGenerator);
        if (pnode->sxFnc.IsCoroutine())
        {LOGMEIN("ByteCodeGenerator.cpp] 4359\n");
            // Always assume generator functions escape since tracking them requires tracking
            // the resulting generators in addition to the function.
            byteCodeGenerator->FuncEscapes(byteCodeGenerator->TopFuncInfo()->GetBodyScope());
        }
        if (!pnode->sxFnc.IsDeclaration())
        {LOGMEIN("ByteCodeGenerator.cpp] 4365\n");
            FuncInfo *funcInfo = byteCodeGenerator->TopFuncInfo();
            if (!funcInfo->IsGlobalFunction() || (byteCodeGenerator->GetFlags() & fscrEval))
            {LOGMEIN("ByteCodeGenerator.cpp] 4368\n");
                // In the case of a nested function expression, assumes that it escapes.
                // We could try to analyze what it touches to be more precise.
                byteCodeGenerator->FuncEscapes(funcInfo->GetBodyScope());
            }
            byteCodeGenerator->ProcessCapturedSyms(pnode);
        }
        else if (byteCodeGenerator->IsInLoop())
        {LOGMEIN("ByteCodeGenerator.cpp] 4376\n");
            Symbol *funcSym = pnode->sxFnc.GetFuncSymbol();
            if (funcSym)
            {LOGMEIN("ByteCodeGenerator.cpp] 4379\n");
                Symbol *funcVarSym = funcSym->GetFuncScopeVarSym();
                if (funcVarSym)
                {LOGMEIN("ByteCodeGenerator.cpp] 4382\n");
                    // We're going to write to the funcVarSym when we do the function instantiation,
                    // so treat the funcVarSym as used. That way, we know it will get undef-initialized at the
                    // top of the function, so a jitted loop body won't have any issue with boxing if
                    // the function instantiation isn't executed.
                    Assert(funcVarSym != funcSym);
                    funcVarSym->SetIsUsed(true);
                }
            }
        }
        break;
    case knopThis:
    case knopSuper:
    {LOGMEIN("ByteCodeGenerator.cpp] 4395\n");
        FuncInfo *top = byteCodeGenerator->TopFuncInfo();
        if (top->IsGlobalFunction() && !(byteCodeGenerator->GetFlags() & fscrEval))
        {LOGMEIN("ByteCodeGenerator.cpp] 4398\n");
            top->SetHasGlobalRef(true);
        }
        else if (top->IsLambda())
        {LOGMEIN("ByteCodeGenerator.cpp] 4402\n");
            byteCodeGenerator->MarkThisUsedInLambda();
        }

        // "this" should be loaded for both global and non-global functions
        byteCodeGenerator->TopFuncInfo()->GetParsedFunctionBody()->SetHasThis(true);
        break;
    }
    case knopName:
    {LOGMEIN("ByteCodeGenerator.cpp] 4411\n");
        if (pnode->sxPid.sym == nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 4413\n");
            if (pnode->grfpn & fpnMemberReference)
            {LOGMEIN("ByteCodeGenerator.cpp] 4415\n");
                // This is a member name. No binding.
                break;
            }

            Symbol *sym = byteCodeGenerator->FindSymbol(pnode->sxPid.symRef, pnode->sxPid.pid);
            if (sym)
            {LOGMEIN("ByteCodeGenerator.cpp] 4422\n");
                // This is a named load, not just a reference, so if it's a nested function note that all
                // the nested scopes escape.
                Assert(!sym->GetDecl() || (pnode->sxPid.symRef && *pnode->sxPid.symRef));
                Assert(!sym->GetDecl() || ((*pnode->sxPid.symRef)->GetDecl() == sym->GetDecl()) ||
                       ((*pnode->sxPid.symRef)->GetFuncScopeVarSym() == sym));

                pnode->sxPid.sym = sym;
                if (sym->GetSymbolType() == STFunction &&
                    (!sym->GetIsGlobal() || (byteCodeGenerator->GetFlags() & fscrEval)))
                {LOGMEIN("ByteCodeGenerator.cpp] 4432\n");
                    byteCodeGenerator->FuncEscapes(sym->GetScope());
                }
            }
        }

            FuncInfo *top = byteCodeGenerator->TopFuncInfo();
            if (pnode->sxPid.sym == nullptr || pnode->sxPid.sym->GetIsGlobal())
            {LOGMEIN("ByteCodeGenerator.cpp] 4440\n");
                top->SetHasGlobalRef(true);
            }

            if (pnode->sxPid.sym)
            {LOGMEIN("ByteCodeGenerator.cpp] 4445\n");
            pnode->sxPid.sym->SetIsUsed(true);
        }

        break;
    }
    case knopMember:
    case knopMemberShort:
    case knopObjectPatternMember:
        if (pnode->sxBin.pnode1->nop == knopComputedName)
        {LOGMEIN("ByteCodeGenerator.cpp] 4455\n");
            // Computed property name - cannot bind yet
            break;
        }
        // fall through
    case knopGetMember:
    case knopSetMember:
        {LOGMEIN("ByteCodeGenerator.cpp] 4462\n");
            // lhs is knopStr, rhs is expr
        ParseNode *id = pnode->sxBin.pnode1;
            if (id->nop == knopStr || id->nop == knopName)
            {LOGMEIN("ByteCodeGenerator.cpp] 4466\n");
                byteCodeGenerator->AssignPropertyId(id->sxPid.pid);
                id->sxPid.sym = nullptr;
                id->sxPid.symRef = nullptr;
                id->grfpn |= fpnMemberReference;
            }
            break;
        }
        // TODO: convert index over string to Get/Put Value
    case knopIndex:
        BindReference(pnode, byteCodeGenerator);
        break;
    case knopDot:
        BindInstAndMember(pnode, byteCodeGenerator);
        break;
    case knopTryFinally:
        byteCodeGenerator->SetHasFinally(true);
    case knopTryCatch:
        byteCodeGenerator->SetHasTry(true);
        byteCodeGenerator->TopFuncInfo()->byteCodeFunction->SetDontInline(true);
        byteCodeGenerator->AddTargetStmt(pnode);
        break;
    case knopAsg:
        BindReference(pnode, byteCodeGenerator);
        CheckLocalVarDef(pnode, byteCodeGenerator);
        break;
    case knopVarDecl:
        // "arguments" symbol or decl w/o RHS may have been bound already; otherwise, do the binding here.
        if (pnode->sxVar.sym == nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 4495\n");
            pnode->sxVar.sym = byteCodeGenerator->FindSymbol(pnode->sxVar.symRef, pnode->sxVar.pid);
        }
        SetAdditionalBindInfoForVariables(pnode, byteCodeGenerator);
        break;
    case knopConstDecl:
    case knopLetDecl:
        // "arguments" symbol or decl w/o RHS may have been bound already; otherwise, do the binding here.
        if (!pnode->sxVar.sym)
        {LOGMEIN("ByteCodeGenerator.cpp] 4504\n");
            AssertMsg(pnode->sxVar.symRef && *pnode->sxVar.symRef, "'const' and 'let' should be binded when we bind block");
            pnode->sxVar.sym = *pnode->sxVar.symRef;
        }
        SetAdditionalBindInfoForVariables(pnode, byteCodeGenerator);
        break;
    case knopCall:
        if (pnode->sxCall.isEvalCall && byteCodeGenerator->TopFuncInfo()->IsLambda())
        {LOGMEIN("ByteCodeGenerator.cpp] 4512\n");
            byteCodeGenerator->MarkThisUsedInLambda();
        }
        // fallthrough
    case knopTypeof:
    case knopDelete:
        BindReference(pnode, byteCodeGenerator);
        break;

    case knopRegExp:
        pnode->sxPid.regexPatternIndex = byteCodeGenerator->TopFuncInfo()->GetParsedFunctionBody()->NewLiteralRegex();
        break;

    case knopComma:
        pnode->sxBin.pnode1->SetNotEscapedUse();
        break;

    case knopBlock:
    {LOGMEIN("ByteCodeGenerator.cpp] 4530\n");
        for (ParseNode *pnodeScope = pnode->sxBlock.pnodeScopes; pnodeScope; /* no increment */)
        {LOGMEIN("ByteCodeGenerator.cpp] 4532\n");
            switch (pnodeScope->nop)
            {LOGMEIN("ByteCodeGenerator.cpp] 4534\n");
            case knopFncDecl:
                if (pnodeScope->sxFnc.IsDeclaration())
                {LOGMEIN("ByteCodeGenerator.cpp] 4537\n");
                    byteCodeGenerator->ProcessCapturedSyms(pnodeScope);
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
        break;
    }

    }
}

void ByteCodeGenerator::ProcessCapturedSyms(ParseNode *pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 4563\n");
    SymbolTable *capturedSyms = pnode->sxFnc.funcInfo->GetCapturedSyms();
    if (capturedSyms)
    {LOGMEIN("ByteCodeGenerator.cpp] 4566\n");
        FuncInfo *funcInfo = this->TopFuncInfo();
        CapturedSymMap *capturedSymMap = funcInfo->EnsureCapturedSymMap();
        ParseNode *pnodeStmt = this->GetCurrentTopStatement();

        SList<Symbol*> *capturedSymList;
        if (!pnodeStmt->CapturesSyms())
        {LOGMEIN("ByteCodeGenerator.cpp] 4573\n");
            capturedSymList = Anew(this->alloc, SList<Symbol*>, this->alloc);
            capturedSymMap->Add(pnodeStmt, capturedSymList);
            pnodeStmt->SetCapturesSyms();
        }
        else
        {
            capturedSymList = capturedSymMap->Item(pnodeStmt);
        }

        capturedSyms->Map([&](Symbol *sym)
        {
            if (!sym->GetIsCommittedToSlot() && !sym->HasVisitedCapturingFunc())
            {LOGMEIN("ByteCodeGenerator.cpp] 4586\n");
                capturedSymList->Prepend(sym);
                sym->SetHasVisitedCapturingFunc();
            }
        });
    }
}

void ByteCodeGenerator::MarkThisUsedInLambda()
{LOGMEIN("ByteCodeGenerator.cpp] 4595\n");
    // This is a lambda that refers to "this".
    // Find the enclosing "normal" function and indicate that the lambda captures the enclosing function's "this".
    FuncInfo *parent = this->FindEnclosingNonLambda();
    parent->GetParsedFunctionBody()->SetHasThis(true);
    if (!parent->IsGlobalFunction() || this->GetFlags() & fscrEval)
    {LOGMEIN("ByteCodeGenerator.cpp] 4601\n");
        // If the enclosing function is non-global or eval global, it will put "this" in a closure slot.
        parent->SetIsThisLexicallyCaptured();
        Scope* scope = parent->IsGlobalFunction() ? parent->GetGlobalEvalBlockScope() :
            (parent->GetParamScope() && !parent->GetParamScope()->GetCanMergeWithBodyScope()) ? parent->GetParamScope() :
            parent->GetBodyScope();
        scope->SetHasOwnLocalInClosure(true);
        this->ProcessScopeWithCapturedSym(scope);

        this->TopFuncInfo()->SetHasClosureReference(true);
    }

    this->TopFuncInfo()->SetHasCapturedThis();
}

void ByteCodeGenerator::FuncEscapes(Scope *scope)
{LOGMEIN("ByteCodeGenerator.cpp] 4617\n");
    while (scope)
    {LOGMEIN("ByteCodeGenerator.cpp] 4619\n");
        Assert(scope->GetFunc());
        scope->GetFunc()->SetEscapes(true);
        scope = scope->GetEnclosingScope();
    }

    if (this->flags & fscrEval)
    {LOGMEIN("ByteCodeGenerator.cpp] 4626\n");
        // If a function declared inside eval escapes, we'll need
        // to invalidate the caller's cached scope.
        this->funcEscapes = true;
    }
}

bool ByteCodeGenerator::HasInterleavingDynamicScope(Symbol * sym) const
{LOGMEIN("ByteCodeGenerator.cpp] 4634\n");
    Js::PropertyId unused;
    return this->InDynamicScope() &&
        sym->GetScope() != this->FindScopeForSym(sym->GetScope(), nullptr, &unused, this->TopFuncInfo());
}

void CheckMaybeEscapedUse(ParseNode * pnode, ByteCodeGenerator * byteCodeGenerator, bool isCall = false)
{LOGMEIN("ByteCodeGenerator.cpp] 4641\n");
    if (pnode == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 4643\n");
        return;
    }

    FuncInfo * topFunc = byteCodeGenerator->TopFuncInfo();
    if (topFunc->IsGlobalFunction())
    {LOGMEIN("ByteCodeGenerator.cpp] 4649\n");
        return;
    }

    switch (pnode->nop)
    {LOGMEIN("ByteCodeGenerator.cpp] 4654\n");
    case knopAsg:
        if (pnode->sxBin.pnode1->nop != knopName)
        {LOGMEIN("ByteCodeGenerator.cpp] 4657\n");
            break;
        }
        // use of an assignment (e.g. (y = function() {}) + "1"), just make y an escaped use.
        pnode = pnode->sxBin.pnode1;
        isCall = false;
        // fall-through
    case knopName:
        if (!isCall)
        {LOGMEIN("ByteCodeGenerator.cpp] 4666\n");
            // Mark the name has having escaped use
            if (pnode->sxPid.sym)
            {LOGMEIN("ByteCodeGenerator.cpp] 4669\n");
                pnode->sxPid.sym->SetHasMaybeEscapedUse(byteCodeGenerator);
            }
        }
        break;

    case knopFncDecl:
        // A function declaration has an unknown use (not assignment nor call),
        // mark the function as having child escaped
        topFunc->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("UnknownUse")));
        break;
    }
}

void CheckFuncAssignment(Symbol * sym, ParseNode * pnode2, ByteCodeGenerator * byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 4684\n");
    if (pnode2 == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 4686\n");
        return;
    }

    switch (pnode2->nop)
    {LOGMEIN("ByteCodeGenerator.cpp] 4691\n");
    default:
        CheckMaybeEscapedUse(pnode2, byteCodeGenerator);
        break;
    case knopFncDecl:
        {LOGMEIN("ByteCodeGenerator.cpp] 4696\n");
            FuncInfo * topFunc = byteCodeGenerator->TopFuncInfo();
            if (topFunc->IsGlobalFunction())
            {LOGMEIN("ByteCodeGenerator.cpp] 4699\n");
                return;
            }
            // Use not as an assignment or assignment to an outer function's sym, or assigned to a formal
        // or assigned to multiple names.

            if (sym == nullptr
                || sym->GetScope()->GetFunc() != topFunc)
            {LOGMEIN("ByteCodeGenerator.cpp] 4707\n");
                topFunc->SetHasMaybeEscapedNestedFunc(DebugOnly(
                sym == nullptr ? _u("UnknownAssignment") :
                (sym->GetScope()->GetFunc() != topFunc) ? _u("CrossFuncAssignment") :
                    _u("SomethingIsWrong!"))
                    );
            }
            else
            {
                // TODO-STACK-NESTED-FUNC: Since we only support single def functions, we can still put the
            // nested function on the stack and reuse even if the function goes out of the block scope.
                // However, we cannot allocate frame display or slots on the stack if the function is
            // declared in a loop, because there might be multiple functions referencing different
            // iterations of the scope.
                // For now, just disable everything.

                Scope * funcParentScope = pnode2->sxFnc.funcInfo->GetBodyScope()->GetEnclosingScope();
                while (sym->GetScope() != funcParentScope)
                {LOGMEIN("ByteCodeGenerator.cpp] 4725\n");
                    if (funcParentScope->GetMustInstantiate())
                    {LOGMEIN("ByteCodeGenerator.cpp] 4727\n");
                        topFunc->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("CrossScopeAssignment")));
                        break;
                    }
                    funcParentScope->SetHasCrossScopeFuncAssignment();
                    funcParentScope = funcParentScope->GetEnclosingScope();
                }

            // Need to always detect interleaving dynamic scope ('with') for assignments
            // as those may end up escaping into the 'with' scope.
                // TODO: the with scope is marked as MustInstantiate late during byte code emit
                // We could detect this using the loop above as well, by marking the with
            // scope as must instantiate early, this is just less risky of a fix for RTM.

                if (byteCodeGenerator->HasInterleavingDynamicScope(sym))
                {LOGMEIN("ByteCodeGenerator.cpp] 4742\n");
                     byteCodeGenerator->TopFuncInfo()->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("InterleavingDynamicScope")));
                }

                sym->SetHasFuncAssignment(byteCodeGenerator);
            }
        }
        break;
    };
}


inline bool ContainsSuperReference(ParseNodePtr pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 4755\n");
    return (pnode->sxCall.pnodeTarget->nop == knopDot && pnode->sxCall.pnodeTarget->sxBin.pnode1->nop == knopSuper) // super.prop()
           || (pnode->sxCall.pnodeTarget->nop == knopIndex && pnode->sxCall.pnodeTarget->sxBin.pnode1->nop == knopSuper); // super[prop]()
}

inline bool ContainsDirectSuper(ParseNodePtr pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 4761\n");
    return pnode->sxCall.pnodeTarget->nop == knopSuper; // super()
}


// Assign permanent (non-temp) registers for the function.
// These include constants (null, 3.7, this) and locals that use registers as their home locations.
// Assign the location fields of parse nodes whose values are constants/locals with permanent/known registers.
// Re-usable expression temps are assigned during the final Emit pass.
void AssignRegisters(ParseNode *pnode, ByteCodeGenerator *byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 4771\n");
    if (pnode == nullptr)
    {LOGMEIN("ByteCodeGenerator.cpp] 4773\n");
        return;
    }

    Symbol *sym;
    OpCode nop = pnode->nop;
    switch (nop)
    {LOGMEIN("ByteCodeGenerator.cpp] 4780\n");
    default:
        {
            uint flags = ParseNode::Grfnop(nop);
            if (flags & fnopUni)
            {LOGMEIN("ByteCodeGenerator.cpp] 4785\n");
                CheckMaybeEscapedUse(pnode->sxUni.pnode1, byteCodeGenerator);
            }
            else if (flags & fnopBin)
            {LOGMEIN("ByteCodeGenerator.cpp] 4789\n");
                CheckMaybeEscapedUse(pnode->sxBin.pnode1, byteCodeGenerator);
                CheckMaybeEscapedUse(pnode->sxBin.pnode2, byteCodeGenerator);
            }
        break;
    }

    case knopParamPattern:
        byteCodeGenerator->AssignUndefinedConstRegister();
        CheckMaybeEscapedUse(pnode->sxParamPattern.pnode1, byteCodeGenerator);
        break;

    case knopObjectPattern:
    case knopArrayPattern:
        byteCodeGenerator->AssignUndefinedConstRegister();
        CheckMaybeEscapedUse(pnode->sxUni.pnode1, byteCodeGenerator);
        break;

    case knopDot:
        CheckMaybeEscapedUse(pnode->sxBin.pnode1, byteCodeGenerator);
        break;
    case knopMember:
    case knopMemberShort:
    case knopGetMember:
    case knopSetMember:
        CheckMaybeEscapedUse(pnode->sxBin.pnode2, byteCodeGenerator);
        break;

    case knopAsg:
        {LOGMEIN("ByteCodeGenerator.cpp] 4818\n");
            Symbol * symName = pnode->sxBin.pnode1->nop == knopName ? pnode->sxBin.pnode1->sxPid.sym : nullptr;
            CheckFuncAssignment(symName, pnode->sxBin.pnode2, byteCodeGenerator);

            if (pnode->IsInList())
            {LOGMEIN("ByteCodeGenerator.cpp] 4823\n");
                // Assignment in array literal
                CheckMaybeEscapedUse(pnode->sxBin.pnode1, byteCodeGenerator);
            }

            if (byteCodeGenerator->IsES6DestructuringEnabled() && (pnode->sxBin.pnode1->nop == knopArrayPattern || pnode->sxBin.pnode1->nop == knopObjectPattern))
            {LOGMEIN("ByteCodeGenerator.cpp] 4829\n");
                // Destructured arrays may have default values and need undefined.
                byteCodeGenerator->AssignUndefinedConstRegister();

                // Any rest parameter in a destructured array will need a 0 constant.
                byteCodeGenerator->EnregisterConstant(0);
            }

        break;
    }

    case knopEllipsis:
        if (byteCodeGenerator->InDestructuredPattern())
        {LOGMEIN("ByteCodeGenerator.cpp] 4842\n");
            // Get a register for the rest array counter.
            pnode->location = byteCodeGenerator->NextVarRegister();

            // Any rest parameter in a destructured array will need a 0 constant.
            byteCodeGenerator->EnregisterConstant(0);
        }
        CheckMaybeEscapedUse(pnode->sxUni.pnode1, byteCodeGenerator);
        break;

    case knopQmark:
        CheckMaybeEscapedUse(pnode->sxTri.pnode1, byteCodeGenerator);
        CheckMaybeEscapedUse(pnode->sxTri.pnode2, byteCodeGenerator);
        CheckMaybeEscapedUse(pnode->sxTri.pnode3, byteCodeGenerator);
        break;
    case knopWith:
        pnode->location = byteCodeGenerator->NextVarRegister();
        CheckMaybeEscapedUse(pnode->sxWith.pnodeObj, byteCodeGenerator);
        break;
    case knopComma:
        if (!pnode->IsNotEscapedUse())
        {LOGMEIN("ByteCodeGenerator.cpp] 4863\n");
            // Only the last expr in comma expr escape. Mark it if it is escapable.
            CheckMaybeEscapedUse(pnode->sxBin.pnode2, byteCodeGenerator);
        }
        break;
    case knopFncDecl:
        if (!byteCodeGenerator->TopFuncInfo()->IsGlobalFunction())
        {LOGMEIN("ByteCodeGenerator.cpp] 4870\n");
            if (pnode->sxFnc.IsCoroutine())
            {LOGMEIN("ByteCodeGenerator.cpp] 4872\n");
                // Assume generators always escape; otherwise need to analyze if
                // the return value of calls to generator function, the generator
                // objects, escape.
                FuncInfo* funcInfo = byteCodeGenerator->TopFuncInfo();
                funcInfo->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("Generator")));
            }

            if (pnode->IsInList() && !pnode->IsNotEscapedUse())
            {LOGMEIN("ByteCodeGenerator.cpp] 4881\n");
                byteCodeGenerator->TopFuncInfo()->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("InList")));
            }

            ParseNodePtr pnodeName = pnode->sxFnc.pnodeName;
            if (pnodeName != nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 4887\n");
                // REVIEW: does this apply now that compat mode is gone?
                // There is a weird case in compat mode where we may not have a sym assigned to a fnc decl's
                // name node if it is a named function declare inside 'with' that also assigned to something else
                // as well. Instead, We generate two knopFncDecl node one for parent function and one for the assignment.
                // Only the top one gets a sym, not the inner one.  The assignment in the 'with' will be using the inner
                // one.  Also we will detect that the assignment to a variable is an escape inside a 'with'.
                // Since we need the sym in the fnc decl's name, we just detect the escape here as "WithScopeFuncName".

                if (pnodeName->nop == knopVarDecl && pnodeName->sxVar.sym != nullptr)
                {LOGMEIN("ByteCodeGenerator.cpp] 4897\n");
                    // Unlike in CheckFuncAssignment, we don't check for interleaving
                    // dynamic scope ('with') here, because we also generate direct assignment for
                    // function decl's names

                    pnodeName->sxVar.sym->SetHasFuncAssignment(byteCodeGenerator);

                    // Function declaration in block scope and non-strict mode has a
                    // corresponding var sym that we assign to as well.  Need to
                    // mark that symbol as has func assignment as well.
                    Symbol * functionScopeVarSym = pnodeName->sxVar.sym->GetFuncScopeVarSym();
                    if (functionScopeVarSym)
                    {LOGMEIN("ByteCodeGenerator.cpp] 4909\n");
                        functionScopeVarSym->SetHasFuncAssignment(byteCodeGenerator);
                    }
                }
                else
                {
                    // The function has multiple names, or assign to o.x or o::x
                    byteCodeGenerator->TopFuncInfo()->SetHasMaybeEscapedNestedFunc(DebugOnly(
                        pnodeName->nop == knopList ? _u("MultipleFuncName") :
                        pnodeName->nop == knopDot ? _u("PropFuncName") :
                        pnodeName->nop == knopVarDecl && pnodeName->sxVar.sym == nullptr ? _u("WithScopeFuncName") :
                        _u("WeirdFuncName")
                    ));
                }
            }
        }

        break;
    case knopNew:
        CheckMaybeEscapedUse(pnode->sxCall.pnodeTarget, byteCodeGenerator);
        CheckMaybeEscapedUse(pnode->sxCall.pnodeArgs, byteCodeGenerator);
        break;
    case knopThrow:
        CheckMaybeEscapedUse(pnode->sxUni.pnode1, byteCodeGenerator);
        break;

    // REVIEW: Technically, switch expr or case expr doesn't really escape as strict equal
    // doesn't cause the function to escape.
    case knopSwitch:
        CheckMaybeEscapedUse(pnode->sxSwitch.pnodeVal, byteCodeGenerator);
        break;
    case knopCase:
        CheckMaybeEscapedUse(pnode->sxCase.pnodeExpr, byteCodeGenerator);
        break;

    // REVIEW: Technically, the object for GetForInEnumerator doesn't escape, except when cached,
    // which we can make work.
    case knopForIn:
        CheckMaybeEscapedUse(pnode->sxForInOrForOf.pnodeObj, byteCodeGenerator);
        break;

    case knopForOf:
        byteCodeGenerator->AssignNullConstRegister();
        byteCodeGenerator->AssignUndefinedConstRegister();
        CheckMaybeEscapedUse(pnode->sxForInOrForOf.pnodeObj, byteCodeGenerator);
        break;

    case knopTrue:
        pnode->location = byteCodeGenerator->AssignTrueConstRegister();
        break;

    case knopFalse:
        pnode->location = byteCodeGenerator->AssignFalseConstRegister();
        break;

    case knopDecPost:
    case knopIncPost:
    case knopDecPre:
    case knopIncPre:
        byteCodeGenerator->EnregisterConstant(1);
        CheckMaybeEscapedUse(pnode->sxUni.pnode1, byteCodeGenerator);
        break;
    case knopObject:
        byteCodeGenerator->AssignNullConstRegister();
        break;
    case knopClassDecl:
        {LOGMEIN("ByteCodeGenerator.cpp] 4975\n");
            FuncInfo * topFunc = byteCodeGenerator->TopFuncInfo();
            topFunc->SetHasMaybeEscapedNestedFunc(DebugOnly(_u("Class")));

            // We may need undefined for the 'this', e.g. calling a class expression
            byteCodeGenerator->AssignUndefinedConstRegister();

        break;
        }
    case knopNull:
        pnode->location = byteCodeGenerator->AssignNullConstRegister();
        break;
    case knopThis:
        {LOGMEIN("ByteCodeGenerator.cpp] 4988\n");
            FuncInfo* func = byteCodeGenerator->TopFuncInfo();
            pnode->location = func->AssignThisRegister();
            if (func->IsLambda())
            {LOGMEIN("ByteCodeGenerator.cpp] 4992\n");
                func = byteCodeGenerator->FindEnclosingNonLambda();
                func->AssignThisRegister();

                if (func->IsGlobalFunction() && !(byteCodeGenerator->GetFlags() & fscrEval))
                {LOGMEIN("ByteCodeGenerator.cpp] 4997\n");
                    byteCodeGenerator->AssignNullConstRegister();
                }
            }
            // "this" should be loaded for both global and non global functions
            if (func->IsGlobalFunction() && !(byteCodeGenerator->GetFlags() & fscrEval))
            {LOGMEIN("ByteCodeGenerator.cpp] 5003\n");
                // We'll pass "null" to LdThis, to simulate "null" passed as "this" to the
                // global function.
                func->AssignNullConstRegister();
            }

            break;
        }
    case knopNewTarget:
    {LOGMEIN("ByteCodeGenerator.cpp] 5012\n");
        FuncInfo* func = byteCodeGenerator->TopFuncInfo();
        pnode->location = func->AssignNewTargetRegister();

        FuncInfo* nonLambdaFunc = func;

        if (func->IsLambda())
        {LOGMEIN("ByteCodeGenerator.cpp] 5019\n");
            nonLambdaFunc = byteCodeGenerator->FindEnclosingNonLambda();
        }

        if (nonLambdaFunc != func || (func->IsGlobalFunction() && (byteCodeGenerator->GetFlags() & fscrEval)))
        {LOGMEIN("ByteCodeGenerator.cpp] 5024\n");
            nonLambdaFunc->root->sxFnc.SetHasNewTargetReference();
            nonLambdaFunc->AssignNewTargetRegister();
            nonLambdaFunc->SetIsNewTargetLexicallyCaptured();
            nonLambdaFunc->GetBodyScope()->SetHasOwnLocalInClosure(true);
            byteCodeGenerator->ProcessScopeWithCapturedSym(nonLambdaFunc->GetBodyScope());

            func->SetHasClosureReference(true);
        }

        break;
    }
    case knopSuper:
    {LOGMEIN("ByteCodeGenerator.cpp] 5037\n");
        FuncInfo* func = byteCodeGenerator->TopFuncInfo();
        pnode->location = func->AssignSuperRegister();
        func->AssignThisRegister();

        FuncInfo* nonLambdaFunc = func;
        if (func->IsLambda())
        {LOGMEIN("ByteCodeGenerator.cpp] 5044\n");
            // If this is a lambda inside a class member, the class member will need to load super.
            nonLambdaFunc = byteCodeGenerator->FindEnclosingNonLambda();

            nonLambdaFunc->root->sxFnc.SetHasSuperReference();
            nonLambdaFunc->AssignSuperRegister();
            nonLambdaFunc->AssignThisRegister();
            nonLambdaFunc->SetIsSuperLexicallyCaptured();

            if (nonLambdaFunc->IsClassConstructor())
            {LOGMEIN("ByteCodeGenerator.cpp] 5054\n");
                func->AssignNewTargetRegister();

                nonLambdaFunc->root->sxFnc.SetHasNewTargetReference();
                nonLambdaFunc->AssignNewTargetRegister();
                nonLambdaFunc->SetIsNewTargetLexicallyCaptured();
                nonLambdaFunc->AssignUndefinedConstRegister();
            }

            nonLambdaFunc->GetBodyScope()->SetHasOwnLocalInClosure(true);
            byteCodeGenerator->ProcessScopeWithCapturedSym(nonLambdaFunc->GetBodyScope());
            func->SetHasClosureReference(true);
        }
        else
        {
            if (func->IsClassConstructor())
            {LOGMEIN("ByteCodeGenerator.cpp] 5070\n");
                func->AssignNewTargetRegister();
            }
        }

        if (nonLambdaFunc->IsGlobalFunction())
        {LOGMEIN("ByteCodeGenerator.cpp] 5076\n");
            if (!(byteCodeGenerator->GetFlags() & fscrEval))
            {LOGMEIN("ByteCodeGenerator.cpp] 5078\n");
                // Enable LdSuper for global function to support subsequent emission of call, dot, prop, etc., related to super.
                func->AssignNullConstRegister();
                nonLambdaFunc->AssignNullConstRegister();
            }
        }
        else if (!func->IsClassMember())
        {LOGMEIN("ByteCodeGenerator.cpp] 5085\n");
            func->AssignUndefinedConstRegister();
        }
        break;
    }
    case knopCall:
    {LOGMEIN("ByteCodeGenerator.cpp] 5091\n");
        if (pnode->sxCall.pnodeTarget->nop != knopIndex &&
            pnode->sxCall.pnodeTarget->nop != knopDot)
        {LOGMEIN("ByteCodeGenerator.cpp] 5094\n");
            byteCodeGenerator->AssignUndefinedConstRegister();
        }

        bool containsDirectSuper = ContainsDirectSuper(pnode);
        bool containsSuperReference = ContainsSuperReference(pnode);

        if (containsDirectSuper)
        {LOGMEIN("ByteCodeGenerator.cpp] 5102\n");
            pnode->sxCall.pnodeTarget->location = byteCodeGenerator->TopFuncInfo()->AssignSuperCtorRegister();
        }

        FuncInfo *funcInfo = byteCodeGenerator->TopFuncInfo();

        if (containsDirectSuper || containsSuperReference)
        {LOGMEIN("ByteCodeGenerator.cpp] 5109\n");
            // A super call requires 'this' to be available.
            byteCodeGenerator->SetNeedEnvRegister();
            byteCodeGenerator->AssignThisRegister();

            FuncInfo* parent = funcInfo;
            if (funcInfo->IsLambda())
            {LOGMEIN("ByteCodeGenerator.cpp] 5116\n");
                // If this is a lambda inside a method or a constructor, the enclosing function will need to load super.
                parent = byteCodeGenerator->FindEnclosingNonLambda();
                if (parent->root->sxFnc.IsMethod() || parent->root->sxFnc.IsConstructor())
                {LOGMEIN("ByteCodeGenerator.cpp] 5120\n");
                    // Set up super reference
                    if (containsSuperReference)
                    {LOGMEIN("ByteCodeGenerator.cpp] 5123\n");
                        parent->root->sxFnc.SetHasSuperReference();
                        parent->AssignSuperRegister();
                        parent->SetIsSuperLexicallyCaptured();
                    }
                    else if (containsDirectSuper)
                    {LOGMEIN("ByteCodeGenerator.cpp] 5129\n");
                        parent->root->sxFnc.SetHasDirectSuper();
                        parent->AssignSuperCtorRegister();
                        parent->SetIsSuperCtorLexicallyCaptured();
                    }

                    byteCodeGenerator->ProcessScopeWithCapturedSym(parent->GetBodyScope());
                    funcInfo->SetHasClosureReference(true);
                }

                parent->AssignThisRegister();
                byteCodeGenerator->MarkThisUsedInLambda();
            }

            // If this is a super call we need to have new.target
            if (pnode->sxCall.pnodeTarget->nop == knopSuper)
            {LOGMEIN("ByteCodeGenerator.cpp] 5145\n");
                byteCodeGenerator->AssignNewTargetRegister();
            }
        }

        if (pnode->sxCall.isEvalCall)
        {LOGMEIN("ByteCodeGenerator.cpp] 5151\n");
            if (!funcInfo->GetParsedFunctionBody()->IsReparsed())
            {LOGMEIN("ByteCodeGenerator.cpp] 5153\n");
                Assert(funcInfo->IsGlobalFunction() || funcInfo->GetCallsEval());
                funcInfo->SetCallsEval(true);
                funcInfo->GetParsedFunctionBody()->SetCallsEval(true);
            }
            else
            {
                // On reparsing, load the state from function Body, instead of using the state on the parse node,
                // as they might be different.
                pnode->sxCall.isEvalCall = funcInfo->GetParsedFunctionBody()->GetCallsEval();
            }

            if (funcInfo->IsLambda() && pnode->sxCall.isEvalCall)
            {LOGMEIN("ByteCodeGenerator.cpp] 5166\n");
                FuncInfo* nonLambdaParent = byteCodeGenerator->FindEnclosingNonLambda();
                if (!nonLambdaParent->IsGlobalFunction() || (byteCodeGenerator->GetFlags() & fscrEval))
                {LOGMEIN("ByteCodeGenerator.cpp] 5169\n");
                    nonLambdaParent->AssignThisRegister();
                }
            }

            // An eval call in a method or a constructor needs to load super.
            if (funcInfo->root->sxFnc.IsMethod() || funcInfo->root->sxFnc.IsConstructor())
            {LOGMEIN("ByteCodeGenerator.cpp] 5176\n");
                funcInfo->AssignSuperRegister();
                if (funcInfo->root->sxFnc.IsClassConstructor() && !funcInfo->root->sxFnc.IsBaseClassConstructor())
                {LOGMEIN("ByteCodeGenerator.cpp] 5179\n");
                    funcInfo->AssignSuperCtorRegister();
                }
            }
            else if (funcInfo->IsLambda())
            {LOGMEIN("ByteCodeGenerator.cpp] 5184\n");
                // If this is a lambda inside a class member, the class member will need to load super.
                FuncInfo *parent = byteCodeGenerator->FindEnclosingNonLambda();
                if (parent->root->sxFnc.IsClassMember())
                {LOGMEIN("ByteCodeGenerator.cpp] 5188\n");
                    parent->root->sxFnc.SetHasSuperReference();
                    parent->AssignSuperRegister();
                    if (parent->IsClassConstructor() && !parent->IsBaseClassConstructor())
                    {LOGMEIN("ByteCodeGenerator.cpp] 5192\n");
                        parent->AssignSuperCtorRegister();
                    }
                }
            }
        }
        // Don't need to check pnode->sxCall.pnodeTarget even if it is a knopFncDecl,
        // e.g. (function(){})();
        // It is only used as a call, so don't count as an escape.
        // Although not assigned to a slot, we will still able to box it by boxing
        // all the stack function on the interpreter frame or the stack function link list
        // on a jitted frame
        break;
    }

    case knopInt:
        pnode->location = byteCodeGenerator->EnregisterConstant(pnode->sxInt.lw);
        break;
    case knopFlt:
    {LOGMEIN("ByteCodeGenerator.cpp] 5211\n");
        pnode->location = byteCodeGenerator->EnregisterDoubleConstant(pnode->sxFlt.dbl);
        break;
    }
    case knopStr:
        pnode->location = byteCodeGenerator->EnregisterStringConstant(pnode->sxPid.pid);
        break;
    case knopVarDecl:
    case knopConstDecl:
    case knopLetDecl:
        {LOGMEIN("ByteCodeGenerator.cpp] 5221\n");
            sym = pnode->sxVar.sym;
            Assert(sym != nullptr);

            Assert(sym->GetScope()->GetEnclosingFunc() == byteCodeGenerator->TopFuncInfo());

            if (pnode->sxVar.isBlockScopeFncDeclVar && sym->GetIsBlockVar())
            {LOGMEIN("ByteCodeGenerator.cpp] 5228\n");
                break;
            }

            if (!sym->GetIsGlobal())
            {LOGMEIN("ByteCodeGenerator.cpp] 5233\n");
                FuncInfo *funcInfo = byteCodeGenerator->TopFuncInfo();

                // Check the function assignment for the sym that we have, even if we remap it to function level sym below
                // as we are going assign to the original sym
                CheckFuncAssignment(sym, pnode->sxVar.pnodeInit, byteCodeGenerator);

                if (sym->GetIsCatch() || (pnode->nop == knopVarDecl && sym->GetIsBlockVar() && !pnode->sxVar.isBlockScopeFncDeclVar))
                {LOGMEIN("ByteCodeGenerator.cpp] 5241\n");
                    // The LHS of the var decl really binds to the local symbol, not the catch or let symbol.
                    // But the assignment will go to the catch or let symbol. Just assign a register to the local
                    // so that it can get initialized to undefined.
#if DBG
                    if (!sym->GetIsCatch())
                    {LOGMEIN("ByteCodeGenerator.cpp] 5247\n");
                        // Catch cannot be at function scope and let and var at function scope is redeclaration error.
                        Assert(funcInfo->bodyScope != sym->GetScope());
                    }
#endif
                    auto symName = sym->GetName();
                    sym = funcInfo->bodyScope->FindLocalSymbol(symName);
                    if (sym == nullptr)
                    {LOGMEIN("ByteCodeGenerator.cpp] 5255\n");
                        sym = funcInfo->paramScope->FindLocalSymbol(symName);
                    }
                    Assert((sym && !sym->GetIsCatch() && !sym->GetIsBlockVar()));
                }
                // Don't give the declared var a register if it's in a closure, because the closure slot
                // is its true "home". (Need to check IsGlobal again as the sym may have changed above.)
                if (!sym->GetIsGlobal() && !sym->IsInSlot(funcInfo))
                {LOGMEIN("ByteCodeGenerator.cpp] 5263\n");
                    if (PHASE_TRACE(Js::DelayCapturePhase, funcInfo->byteCodeFunction))
                    {LOGMEIN("ByteCodeGenerator.cpp] 5265\n");
                        if (sym->NeedsSlotAlloc(byteCodeGenerator->TopFuncInfo()))
                        {LOGMEIN("ByteCodeGenerator.cpp] 5267\n");
                            Output::Print(_u("--- DelayCapture: Delayed capturing symbol '%s' during initialization.\n"),
                                sym->GetName().GetBuffer());
                            Output::Flush();
                        }
                    }
                    byteCodeGenerator->AssignRegister(sym);
                }
                if (sym->GetScope() == funcInfo->paramScope && !funcInfo->paramScope->GetCanMergeWithBodyScope())
                {LOGMEIN("ByteCodeGenerator.cpp] 5276\n");
                    // We created an equivalent symbol in the body, let us allocate a register for it if necessary,
                    // because it may not be referenced in the body at all.
                    Symbol* bodySym = funcInfo->bodyScope->FindLocalSymbol(sym->GetName());
                    if (!bodySym->IsInSlot(funcInfo))
                    {LOGMEIN("ByteCodeGenerator.cpp] 5281\n");
                        byteCodeGenerator->AssignRegister(bodySym);
                    }
                }
            }
            else
            {
                Assert(byteCodeGenerator->TopFuncInfo()->IsGlobalFunction());
            }

            break;
        }

    case knopFor:
        if ((pnode->sxFor.pnodeBody != nullptr) && (pnode->sxFor.pnodeBody->nop == knopBlock) &&
            (pnode->sxFor.pnodeBody->sxBlock.pnodeStmt != nullptr) &&
            (pnode->sxFor.pnodeBody->sxBlock.pnodeStmt->nop == knopFor) &&
            (!byteCodeGenerator->IsInDebugMode()))
        {LOGMEIN("ByteCodeGenerator.cpp] 5299\n");
                FuncInfo *funcInfo = byteCodeGenerator->TopFuncInfo();
            pnode->sxFor.pnodeInverted = InvertLoop(pnode, byteCodeGenerator, funcInfo);
        }
        else
        {
            pnode->sxFor.pnodeInverted = nullptr;
        }

        break;

    case knopName:
        sym = pnode->sxPid.sym;
        if (sym == nullptr)
        {LOGMEIN("ByteCodeGenerator.cpp] 5313\n");
            Assert(pnode->sxPid.pid->GetPropertyId() != Js::Constants::NoProperty);
        }
        else
        {
            // Note: don't give a register to a local if it's in a closure, because then the closure
            // is its true home.
            if (!sym->GetIsGlobal() &&
                !sym->GetIsMember() &&
                byteCodeGenerator->TopFuncInfo() == sym->GetScope()->GetEnclosingFunc() &&
                !sym->IsInSlot(byteCodeGenerator->TopFuncInfo()) &&
                !sym->HasVisitedCapturingFunc())
            {LOGMEIN("ByteCodeGenerator.cpp] 5325\n");
                if (PHASE_TRACE(Js::DelayCapturePhase, byteCodeGenerator->TopFuncInfo()->byteCodeFunction))
                {LOGMEIN("ByteCodeGenerator.cpp] 5327\n");
                    if (sym->NeedsSlotAlloc(byteCodeGenerator->TopFuncInfo()))
                    {LOGMEIN("ByteCodeGenerator.cpp] 5329\n");
                        Output::Print(_u("--- DelayCapture: Delayed capturing symbol '%s'.\n"),
                            sym->GetName().GetBuffer());
                        Output::Flush();
                    }
                }

                // Local symbol being accessed in its own frame. Even if "with" or event
                // handler semantics make the binding ambiguous, it has a home location,
                // so assign it.
                byteCodeGenerator->AssignRegister(sym);

                // If we're in something like a "with" we'll need a scratch register to hold
                // the multiple possible values of the property.
                if (!byteCodeGenerator->HasInterleavingDynamicScope(sym))
                {LOGMEIN("ByteCodeGenerator.cpp] 5344\n");
                    // We're not in a dynamic scope, or our home scope is nested within the dynamic scope, so we
                    // don't have to do dynamic binding. Just use the home location for this reference.
                    pnode->location = sym->GetLocation();
                }
            }
        }
        if (pnode->IsInList() && !pnode->IsNotEscapedUse())
        {
            // A node that is in a list is assumed to be escape, unless marked otherwise.
            // This includes array literal list/object literal list
            CheckMaybeEscapedUse(pnode, byteCodeGenerator);
        }
        break;

    case knopProg:
        if (!byteCodeGenerator->HasParentScopeInfo())
        {
            // If we're compiling a nested deferred function, don't pop the scope stack,
            // because we just want to leave it as-is for the emit pass.
            PostVisitFunction(pnode, byteCodeGenerator);
        }
        break;
    case knopReturn:
        {LOGMEIN("ByteCodeGenerator.cpp] 5368\n");
            ParseNode *pnodeExpr = pnode->sxReturn.pnodeExpr;
            CheckMaybeEscapedUse(pnodeExpr, byteCodeGenerator);
            break;
        }

    case knopStrTemplate:
        {LOGMEIN("ByteCodeGenerator.cpp] 5375\n");
            ParseNode* pnodeExprs = pnode->sxStrTemplate.pnodeSubstitutionExpressions;
            if (pnodeExprs != nullptr)
            {LOGMEIN("ByteCodeGenerator.cpp] 5378\n");
                while (pnodeExprs->nop == knopList)
                {LOGMEIN("ByteCodeGenerator.cpp] 5380\n");
                    Assert(pnodeExprs->sxBin.pnode1 != nullptr);
                    Assert(pnodeExprs->sxBin.pnode2 != nullptr);

                    CheckMaybeEscapedUse(pnodeExprs->sxBin.pnode1, byteCodeGenerator);
                    pnodeExprs = pnodeExprs->sxBin.pnode2;
                }

                // Also check the final element in the list
                CheckMaybeEscapedUse(pnodeExprs, byteCodeGenerator);
            }

            if (pnode->sxStrTemplate.isTaggedTemplate)
            {LOGMEIN("ByteCodeGenerator.cpp] 5393\n");
                pnode->location = byteCodeGenerator->EnregisterStringTemplateCallsiteConstant(pnode);
            }
            break;
        }
    case knopExportDefault:
        {LOGMEIN("ByteCodeGenerator.cpp] 5399\n");
            ParseNode* expr = pnode->sxExportDefault.pnodeExpr;

            if (expr != nullptr)
            {
                CheckMaybeEscapedUse(expr, byteCodeGenerator);
            }

            break;
        }
    case knopYieldLeaf:
        byteCodeGenerator->AssignUndefinedConstRegister();
        break;
    case knopYield:
        CheckMaybeEscapedUse(pnode->sxUni.pnode1, byteCodeGenerator);
        break;
    case knopYieldStar:
        byteCodeGenerator->AssignNullConstRegister();
        byteCodeGenerator->AssignUndefinedConstRegister();
        CheckMaybeEscapedUse(pnode->sxUni.pnode1, byteCodeGenerator);
        break;
    }
}

// TODO[ianhall]: ApplyEnclosesArgs should be in ByteCodeEmitter.cpp but that becomes complicated because it depends on VisitIndirect
void PostCheckApplyEnclosesArgs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, ApplyCheck* applyCheck);
void CheckApplyEnclosesArgs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, ApplyCheck* applyCheck);
bool ApplyEnclosesArgs(ParseNode* fncDecl, ByteCodeGenerator* byteCodeGenerator)
{LOGMEIN("ByteCodeGenerator.cpp] 5427\n");
    if (byteCodeGenerator->IsInDebugMode())
    {LOGMEIN("ByteCodeGenerator.cpp] 5429\n");
        // Inspection of the arguments object will be messed up if we do ApplyArgs.
        return false;
    }

    if (!fncDecl->HasVarArguments()
        && fncDecl->sxFnc.pnodeParams == nullptr
        && fncDecl->sxFnc.pnodeRest == nullptr
        && fncDecl->sxFnc.nestedCount == 0)
    {LOGMEIN("ByteCodeGenerator.cpp] 5438\n");
        ApplyCheck applyCheck;
        applyCheck.matches = true;
        applyCheck.sawApply = false;
        applyCheck.insideApplyCall = false;
        VisitIndirect<ApplyCheck>(fncDecl->sxFnc.pnodeBody, byteCodeGenerator, &applyCheck, &CheckApplyEnclosesArgs, &PostCheckApplyEnclosesArgs);
        return applyCheck.matches&&applyCheck.sawApply;
    }

    return false;
}

// TODO[ianhall]: VisitClearTmpRegs should be in ByteCodeEmitter.cpp but that becomes complicated because it depends on VisitIndirect
void ClearTmpRegs(ParseNode* pnode, ByteCodeGenerator* byteCodeGenerator, FuncInfo* emitFunc);
void VisitClearTmpRegs(ParseNode * pnode, ByteCodeGenerator * byteCodeGenerator, FuncInfo * funcInfo)
{
    VisitIndirect<FuncInfo>(pnode, byteCodeGenerator, funcInfo, &ClearTmpRegs, nullptr);
}

Js::FunctionBody * ByteCodeGenerator::MakeGlobalFunctionBody(ParseNode *pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 5458\n");
    Js::FunctionBody * func;

    ENTER_PINNED_SCOPE(Js::PropertyRecordList, propertyRecordList);
    propertyRecordList = EnsurePropertyRecordList();

    func =
        Js::FunctionBody::NewFromRecycler(
            scriptContext,
            Js::Constants::GlobalFunction,
            Js::Constants::GlobalFunctionLength,
            0,
            pnode->sxFnc.nestedCount,
            m_utf8SourceInfo,
            m_utf8SourceInfo->GetSrcInfo()->sourceContextInfo->sourceContextId,
            pnode->sxFnc.functionId,
            propertyRecordList,
            Js::FunctionInfo::Attributes::None,
            Js::FunctionBody::FunctionBodyFlags::Flags_HasNoExplicitReturnValue
#ifdef PERF_COUNTERS
            , false /* is function from deferred deserialized proxy */
#endif
            );

    func->SetIsGlobalFunc(true);
    scriptContext->GetLibrary()->RegisterDynamicFunctionReference(func);
    LEAVE_PINNED_SCOPE();

    return func;
}

/* static */
bool ByteCodeGenerator::NeedScopeObjectForArguments(FuncInfo *funcInfo, ParseNode *pnodeFnc)
{LOGMEIN("ByteCodeGenerator.cpp] 5491\n");
    // We can avoid creating a scope object with arguments present if:
    bool dontNeedScopeObject =
        // We have arguments, and
        funcInfo->GetHasHeapArguments()
        // Either we are in strict mode, or have strict mode formal semantics from a non-simple parameter list, and
        && (funcInfo->GetIsStrictMode()
            || pnodeFnc->sxFnc.HasNonSimpleParameterList())
        // Neither of the scopes are objects
        && !funcInfo->paramScope->GetIsObject()
        && !funcInfo->bodyScope->GetIsObject();

    return funcInfo->GetHasHeapArguments()
        // Regardless of the conditions above, we won't need a scope object if there aren't any formals.
        && (pnodeFnc->sxFnc.pnodeParams != nullptr || pnodeFnc->sxFnc.pnodeRest != nullptr)
        && !dontNeedScopeObject;
}

Js::FunctionBody *ByteCodeGenerator::EnsureFakeGlobalFuncForUndefer(ParseNode *pnode)
{LOGMEIN("ByteCodeGenerator.cpp] 5510\n");
    Js::FunctionBody *func = scriptContext->GetLibrary()->GetFakeGlobalFuncForUndefer();
    if (!func)
    {LOGMEIN("ByteCodeGenerator.cpp] 5513\n");
        func = this->MakeGlobalFunctionBody(pnode);
        scriptContext->GetLibrary()->SetFakeGlobalFuncForUndefer(func);
    }
    else
    {
        func->SetBoundPropertyRecords(EnsurePropertyRecordList());
    }
    if (pnode->sxFnc.GetStrictMode() != 0)
    {LOGMEIN("ByteCodeGenerator.cpp] 5522\n");
        func->SetIsStrictMode();
    }

    return func;
}
