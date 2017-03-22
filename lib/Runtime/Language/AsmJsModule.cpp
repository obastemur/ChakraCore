//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#ifdef ASMJS_PLAT
#include "ByteCode/Symbol.h"
#include "ByteCode/FuncInfo.h"
#include "ByteCode/ByteCodeApi.h"
#include "ByteCode/ByteCodeWriter.h"
#include "ByteCode/ByteCodeGenerator.h"
#include "ByteCode/AsmJsByteCodeWriter.h"
#include "Language/AsmJsByteCodeGenerator.h"

#if DBG_DUMP
#include "ByteCode/ByteCodeDumper.h"
#include "ByteCode/AsmJsByteCodeDumper.h"
#endif

namespace Js
{

    bool AsmJsModuleCompiler::CompileAllFunctions()
    {LOGMEIN("AsmJsModule.cpp] 25\n");
        const int size = mFunctionArray.Count();

        for (int i = 0; i < size; i++)
        {LOGMEIN("AsmJsModule.cpp] 29\n");
            AsmJsFunc* func = mFunctionArray.Item(i);

            if (!CompileFunction(func, i))
            {LOGMEIN("AsmJsModule.cpp] 33\n");
                // an error occurred in the function, revert state on all asm.js functions
                for (int j = 0; j <= i; j++)
                {LOGMEIN("AsmJsModule.cpp] 36\n");
                    RevertFunction(j);
                }
                return false;
            }
            func->Finish();
        }
        return true;
    }


    void AsmJsModuleCompiler::RevertFunction(int funcIndex)
    {LOGMEIN("AsmJsModule.cpp] 48\n");
        AsmJsFunc* func = mFunctionArray.Item(funcIndex);
        FunctionBody * funcBody = func->GetFuncBody();
        funcBody->ResetByteCodeGenState();
        funcBody->AddDeferParseAttribute();
        funcBody->SetFunctionParsed(false);
        funcBody->ResetEntryPoint();
        funcBody->SetEntryPoint(funcBody->GetDefaultEntryPointInfo(), GetScriptContext()->DeferredParsingThunk);
        funcBody->SetIsAsmjsMode(false);
        funcBody->SetIsAsmJsFunction(false);
        func->GetFncNode()->sxFnc.funcInfo->byteCodeFunction = func->GetFuncBody();
    }

    void AsmJsModuleCompiler::RevertAllFunctions()
    {LOGMEIN("AsmJsModule.cpp] 62\n");
        for (int i = 0; i < mFunctionArray.Count(); i++)
        {LOGMEIN("AsmJsModule.cpp] 64\n");
            RevertFunction(i);
        }
    }


    bool AsmJsModuleCompiler::CommitFunctions()
    {LOGMEIN("AsmJsModule.cpp] 71\n");
        const int size = mFunctionArray.Count();
        // if changeHeap is defined, it must be first function, so we should skip it
        for (int i = 0; i < size; i++)
        {LOGMEIN("AsmJsModule.cpp] 75\n");
            AsmJsFunc* func = mFunctionArray.Item(i);
            FunctionBody* functionBody = func->GetFuncBody();
            AsmJsFunctionInfo* asmInfo = functionBody->AllocateAsmJsFunctionInfo();

            if (i == 0 && mUsesChangeHeap)
            {LOGMEIN("AsmJsModule.cpp] 81\n");
                continue;
            }

            if (!asmInfo->Init(func))
            {LOGMEIN("AsmJsModule.cpp] 86\n");
                return false;
            }
            asmInfo->SetIsHeapBufferConst(!mUsesChangeHeap);
            asmInfo->SetUsesHeapBuffer(mUsesHeapBuffer);

            functionBody->CheckAndSetOutParamMaxDepth(func->GetMaxArgOutDepth());
            // should be set in EmitOneFunction
            Assert(functionBody->GetIsAsmjsMode());
            Assert(functionBody->GetIsAsmJsFunction());
            ((EntryPointInfo*)functionBody->GetDefaultEntryPointInfo())->SetIsAsmJSFunction(true);

#if DBG_DUMP && defined(ASMJS_PLAT)
            if(PHASE_DUMP(ByteCodePhase, functionBody))
            {LOGMEIN("AsmJsModule.cpp] 100\n");
                AsmJsByteCodeDumper::Dump(functionBody, nullptr, func);
            }
#endif
#if _M_IX86
            if (PHASE_ON1(AsmJsJITTemplatePhase) && !Configuration::Global.flags.NoNative)
            {LOGMEIN("AsmJsModule.cpp] 106\n");
                AsmJsCodeGenerator* generator = GetScriptContext()->GetAsmJsCodeGenerator();
                AccumulateCompileTime();
                if (!generator)
                {LOGMEIN("AsmJsModule.cpp] 110\n");
                    generator = GetScriptContext()->InitAsmJsCodeGenerator();
                }
                Assert( generator );
                generator->CodeGen(functionBody);
                AccumulateCompileTime(AsmJsCompilation::TemplateJIT);
            }
#endif
        }

        return true;
    }

    bool AsmJsModuleCompiler::CommitModule()
    {LOGMEIN("AsmJsModule.cpp] 124\n");
        FuncInfo* funcInfo = GetModuleFunctionNode()->sxFnc.funcInfo;
        FunctionBody* functionBody = funcInfo->GetParsedFunctionBody();
        AsmJsModuleInfo* asmInfo = functionBody->AllocateAsmJsModuleInfo();

        if (funcInfo->byteCodeFunction->GetIsNamedFunctionExpression())
        {LOGMEIN("AsmJsModule.cpp] 130\n");
            Assert(GetModuleFunctionNode()->sxFnc.pnodeName);
            if (GetModuleFunctionNode()->sxFnc.pnodeName->sxVar.sym->IsInSlot(funcInfo))
            {LOGMEIN("AsmJsModule.cpp] 133\n");
                ParseNodePtr nameNode = GetModuleFunctionNode()->sxFnc.pnodeName;
                GetByteCodeGenerator()->AssignPropertyId(nameNode->name());
                // if module is a named function expression, we may need to restore this for debugger
                AsmJsFunctionDeclaration* closure = Anew(&mAllocator, AsmJsFunctionDeclaration, nameNode->sxVar.pid, AsmJsSymbol::ClosureFunction, &mAllocator);
                DefineIdentifier(nameNode->sxVar.pid, closure);
            }
        }

        int argCount = 0;
        if (mBufferArgName)
        {LOGMEIN("AsmJsModule.cpp] 144\n");
            argCount = 3;
        }
        else if (mForeignArgName)
        {LOGMEIN("AsmJsModule.cpp] 148\n");
            argCount = 2;
        }
        else if (mStdLibArgName)
        {LOGMEIN("AsmJsModule.cpp] 152\n");
            argCount = 1;
        }

        const int functionCount = mFunctionArray.Count();
        const int functionTableCount = mFunctionTableArray.Count();
        const int importFunctionCount = mImportFunctions.GetTotalVarCount();
        asmInfo->SetFunctionCount(functionCount);
        asmInfo->SetFunctionTableCount(functionTableCount);
        asmInfo->SetFunctionImportCount(importFunctionCount);
        asmInfo->SetVarCount(mVarCount);
        asmInfo->SetVarImportCount(mVarImportCount);
        asmInfo->SetArgInCount(argCount);
        asmInfo->SetModuleMemory(mModuleMemory);
        asmInfo->SetAsmMathBuiltinUsed(mAsmMathBuiltinUsedBV);
        asmInfo->SetAsmArrayBuiltinUsed(mAsmArrayBuiltinUsedBV);
        asmInfo->SetUsesChangeHeap(mUsesChangeHeap);
        asmInfo->SetMaxHeapAccess(mMaxHeapAccess);

        if (IsSimdjsEnabled())
        {LOGMEIN("AsmJsModule.cpp] 172\n");
            asmInfo->SetAsmSimdBuiltinUsed(mAsmSimdBuiltinUsedBV);
            asmInfo->SetSimdRegCount(mSimdVarSpace.GetTotalVarCount());
        }

        int varCount = 3; // 3 possible arguments

        functionBody->SetInParamsCount(4); // Always set 4 inParams so the memory space is the same (globalEnv,stdlib,foreign,buffer)
        functionBody->SetReportedInParamsCount(4);
        functionBody->CheckAndSetConstantCount(2); // Return register + Root
        functionBody->CreateConstantTable();
        functionBody->CheckAndSetVarCount(varCount);
        functionBody->SetIsAsmjsMode(true);
        functionBody->NewObjectLiteral(); // allocate one object literal for the export object

        AsmJSByteCodeGenerator::EmitEmptyByteCode(funcInfo, GetByteCodeGenerator(), GetModuleFunctionNode());

        // Create export module proxy
        asmInfo->SetExportFunctionIndex(mExportFuncIndex);
        asmInfo->SetExportsCount(mExports.Count());
        for (int i = 0; i < mExports.Count(); i++)
        {LOGMEIN("AsmJsModule.cpp] 193\n");
            AsmJsModuleExport& exMod = mExports.Item(i);
            auto ex = asmInfo->GetExport(i);
            *ex.id = exMod.id;
            *ex.location = exMod.location;
        }

        int iVar = 0, iVarImp = 0, iFunc = 0, iFuncImp = 0;
        const int moduleEnvCount = mModuleEnvironment.Count();
        asmInfo->InitializeSlotMap(moduleEnvCount);
        auto slotMap = asmInfo->GetAsmJsSlotMap();
        for (int i = 0; i < moduleEnvCount; i++)
        {LOGMEIN("AsmJsModule.cpp] 205\n");
            AsmJsSymbol* sym = mModuleEnvironment.GetValueAt(i);
            if (sym)
            {LOGMEIN("AsmJsModule.cpp] 208\n");
                AsmJsSlot * slot = RecyclerNewLeaf(GetScriptContext()->GetRecycler(), AsmJsSlot);
                slot->symType = sym->GetSymbolType();
                slotMap->AddNew(sym->GetName()->GetPropertyId(), slot);
                switch (sym->GetSymbolType())
                {LOGMEIN("AsmJsModule.cpp] 213\n");
                case AsmJsSymbol::Variable:{LOGMEIN("AsmJsModule.cpp] 214\n");
                    AsmJsVar* var = sym->Cast<AsmJsVar>();
                    auto& modVar = asmInfo->GetVar(iVar++);
                    modVar.location = var->GetLocation();
                    modVar.type = var->GetVarType().which();
                    if (var->GetVarType().isInt())
                    {LOGMEIN("AsmJsModule.cpp] 220\n");
                        modVar.initialiser.intInit = var->GetIntInitialiser();
                    }
                    else if (var->GetVarType().isFloat())
                    {LOGMEIN("AsmJsModule.cpp] 224\n");
                        modVar.initialiser.floatInit = var->GetFloatInitialiser();
                    }
                    else if (var->GetVarType().isDouble())
                    {LOGMEIN("AsmJsModule.cpp] 228\n");
                        modVar.initialiser.doubleInit = var->GetDoubleInitialiser();
                    }
                    else if (IsSimdjsEnabled() && var->GetVarType().isSIMD())
                    {LOGMEIN("AsmJsModule.cpp] 232\n");
                        modVar.initialiser.simdInit = var->GetSimdConstInitialiser();
                    }
                    else
                    {
                        Assert(UNREACHED);
                    }

                    modVar.isMutable = var->isMutable();

                    slot->location = modVar.location;
                    slot->varType = var->GetVarType().which();
                    slot->isConstVar = !modVar.isMutable;
                    break;
                }
                case AsmJsSymbol::ConstantImport:{LOGMEIN("AsmJsModule.cpp] 247\n");
                    AsmJsConstantImport* var = sym->Cast<AsmJsConstantImport>();
                    auto& modVar = asmInfo->GetVarImport(iVarImp++);
                    modVar.location = var->GetLocation();
                    modVar.field = var->GetField()->GetPropertyId();
                    modVar.type = var->GetVarType().which();

                    slot->location = modVar.location;
                    slot->varType = modVar.type;
                    break;
                }
                case AsmJsSymbol::ImportFunction:{LOGMEIN("AsmJsModule.cpp] 258\n");
                    AsmJsImportFunction* func = sym->Cast<AsmJsImportFunction>();
                    auto& modVar = asmInfo->GetFunctionImport(iFuncImp++);
                    modVar.location = func->GetFunctionIndex();
                    modVar.field = func->GetField()->GetPropertyId();

                    slot->location = modVar.location;
                    break;
                }
                case AsmJsSymbol::FuncPtrTable:{LOGMEIN("AsmJsModule.cpp] 267\n");
                    AsmJsFunctionTable* funcTable = sym->Cast<AsmJsFunctionTable>();
                    const uint size = funcTable->GetSize();
                    const RegSlot index = funcTable->GetFunctionIndex();
                    asmInfo->SetFunctionTableSize(index, size);
                    auto& modTable = asmInfo->GetFunctionTable(index);
                    for (uint j = 0; j < size; j++)
                    {LOGMEIN("AsmJsModule.cpp] 274\n");
                        modTable.moduleFunctionIndex[j] = funcTable->GetModuleFunctionIndex(j);
                    }
                    slot->funcTableSize = size;
                    slot->location = index;

                    break;
                }
                case AsmJsSymbol::ModuleFunction:{LOGMEIN("AsmJsModule.cpp] 282\n");
                    AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                    auto& modVar = asmInfo->GetFunction(iFunc++);
                    modVar.location = func->GetFunctionIndex();
                    slot->location = modVar.location;
                    break;
                }
                case AsmJsSymbol::ArrayView:
                {LOGMEIN("AsmJsModule.cpp] 290\n");
                    AsmJsArrayView * var = sym->Cast<AsmJsArrayView>();
                    slot->viewType = var->GetViewType();
                    break;
                }
                case AsmJsSymbol::ModuleArgument:
                {LOGMEIN("AsmJsModule.cpp] 296\n");
                    AsmJsModuleArg * arg = sym->Cast<AsmJsModuleArg>();
                    slot->argType = arg->GetArgType();
                    break;
                }
                // used only for module validation
                case AsmJsSymbol::MathConstant:
                {LOGMEIN("AsmJsModule.cpp] 303\n");
                    AsmJsMathConst * constVar = sym->Cast<AsmJsMathConst>();
                    slot->mathConstVal = *constVar->GetVal();
                    break;
                }
                case AsmJsSymbol::MathBuiltinFunction:
                {LOGMEIN("AsmJsModule.cpp] 309\n");
                    AsmJsMathFunction * mathFunc = sym->Cast<AsmJsMathFunction>();
                    slot->builtinMathFunc = mathFunc->GetMathBuiltInFunction();
                    break;
                }
                case AsmJsSymbol::TypedArrayBuiltinFunction:
                {LOGMEIN("AsmJsModule.cpp] 315\n");
                    AsmJsTypedArrayFunction * mathFunc = sym->Cast<AsmJsTypedArrayFunction>();
                    slot->builtinArrayFunc = mathFunc->GetArrayBuiltInFunction();
                    break;
                }
                case AsmJsSymbol::SIMDBuiltinFunction:
                {LOGMEIN("AsmJsModule.cpp] 321\n");
                    AsmJsSIMDFunction * mathFunc = sym->Cast<AsmJsSIMDFunction>();
                    slot->builtinSIMDFunc = mathFunc->GetSimdBuiltInFunction();
                    break;
                }
                case AsmJsSymbol::ClosureFunction:
                    // we don't need to store any additional info in this case
                    break;
                default:
                    Assume(UNREACHED);
                }
            }
        }
        return true;
    }

    void AsmJsModuleCompiler::ASTPrepass(ParseNodePtr pnode, AsmJsFunc * func)
    {LOGMEIN("AsmJsModule.cpp] 338\n");
        ThreadContext::ProbeCurrentStackNoDispose(Js::Constants::MinStackByteCodeVisitor, GetByteCodeGenerator()->GetScriptContext());

        if (pnode == NULL)
        {LOGMEIN("AsmJsModule.cpp] 342\n");
            return;
        }

        switch (pnode->nop) {LOGMEIN("AsmJsModule.cpp] 346\n");
        // these first cases do the interesting work
        case knopBreak:
        case knopContinue:
            GetByteCodeGenerator()->AddTargetStmt(pnode->sxJump.pnodeTarget);
            break;

        case knopInt:
            func->AddConst<int>(pnode->sxInt.lw);
            break;
        case knopFlt:
        {LOGMEIN("AsmJsModule.cpp] 357\n");
            const double d = pnode->sxFlt.dbl;
            if (ParserWrapper::IsMinInt(pnode))
            {LOGMEIN("AsmJsModule.cpp] 360\n");
                func->AddConst<int>((int)d);
            }
            else if (ParserWrapper::IsUnsigned(pnode))
            {LOGMEIN("AsmJsModule.cpp] 364\n");
                func->AddConst<int>((int)(uint32)d);
            }
            else
            {
                func->AddConst<double>(d);
            }
            break;
        }
        case knopName:
        {LOGMEIN("AsmJsModule.cpp] 374\n");
            GetByteCodeGenerator()->AssignPropertyId(pnode->name());
            AsmJsSymbol * declSym = LookupIdentifier(pnode->name());
            if (declSym)
            {LOGMEIN("AsmJsModule.cpp] 378\n");
                if (declSym->GetSymbolType() == AsmJsSymbol::MathConstant)
                {LOGMEIN("AsmJsModule.cpp] 380\n");
                    AsmJsMathConst * definition = declSym->Cast<AsmJsMathConst>();
                    Assert(definition->GetType().isDouble());
                    func->AddConst<double>(*definition->GetVal());
                }
                else if (declSym->GetSymbolType() == AsmJsSymbol::Variable && !declSym->isMutable())
                {LOGMEIN("AsmJsModule.cpp] 386\n");
                    AsmJsVar * definition = declSym->Cast<AsmJsVar>();
                    switch (definition->GetVarType().which())
                    {LOGMEIN("AsmJsModule.cpp] 389\n");
                    case AsmJsVarType::Double:
                        func->AddConst<double>(definition->GetDoubleInitialiser());
                        break;
                    case AsmJsVarType::Float:
                        func->AddConst<float>(definition->GetFloatInitialiser());
                        break;
                    case AsmJsVarType::Int:
                        func->AddConst<int>(definition->GetIntInitialiser());
                        break;
                    default:
                        Assume(UNREACHED);
                    }
                }
            }
            break;
        }
        case knopCall:
        {LOGMEIN("AsmJsModule.cpp] 407\n");
            ASTPrepass(pnode->sxCall.pnodeTarget, func);
            bool evalArgs = true;
            if (pnode->sxCall.pnodeTarget->nop == knopName)
            {LOGMEIN("AsmJsModule.cpp] 411\n");
                AsmJsFunctionDeclaration* funcDecl = this->LookupFunction(pnode->sxCall.pnodeTarget->name());
                if (funcDecl && funcDecl->GetSymbolType() == AsmJsSymbol::MathBuiltinFunction)
                {LOGMEIN("AsmJsModule.cpp] 414\n");
                    AsmJsMathFunction* mathFunc = funcDecl->Cast<AsmJsMathFunction>();
                    if (mathFunc->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround)
                    {LOGMEIN("AsmJsModule.cpp] 417\n");
                        switch (pnode->sxCall.pnodeArgs->nop)
                        {LOGMEIN("AsmJsModule.cpp] 419\n");
                        case knopFlt:
                            func->AddConst<float>((float)pnode->sxCall.pnodeArgs->sxFlt.dbl);
                            evalArgs = false;
                            break;
                        case knopInt:
                            func->AddConst<float>((float)pnode->sxCall.pnodeArgs->sxInt.lw);
                            evalArgs = false;
                            break;
                        case knopNeg:
                            if (pnode->sxCall.pnodeArgs->sxUni.pnode1->nop == knopInt && pnode->sxCall.pnodeArgs->sxUni.pnode1->sxInt.lw == 0)
                            {LOGMEIN("AsmJsModule.cpp] 430\n");
                                func->AddConst<float>(-0.0f);
                                evalArgs = false;
                                break;
                            }
                        }
                    }
                }
                else if (IsSimdjsEnabled())
                {LOGMEIN("AsmJsModule.cpp] 439\n");
                    /*
                    Float32x4 operations work on Float reg space.
                    If any of the args is a literal (DoubleLit), we need to have a copy of it in the Float reg space.
                    Note that we may end up with redundant copies in the Double reg space, since we ASTPrepass the args (Fix later ?)
                    */
                    if (funcDecl && funcDecl->GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction)
                    {LOGMEIN("AsmJsModule.cpp] 446\n");
                        AsmJsSIMDFunction* simdFunc = funcDecl->Cast<AsmJsSIMDFunction>();
                        if (simdFunc->IsFloat32x4Func())
                        {LOGMEIN("AsmJsModule.cpp] 449\n");
                            ParseNode *argNode, *arg;
                            argNode = arg = pnode->sxCall.pnodeArgs;
                            do
                            {LOGMEIN("AsmJsModule.cpp] 453\n");
                                if (argNode->nop == knopList)
                                {LOGMEIN("AsmJsModule.cpp] 455\n");
                                    arg = ParserWrapper::GetBinaryLeft(argNode);
                                    argNode = ParserWrapper::GetBinaryRight(argNode);
                                }
                                if (arg->nop == knopFlt)
                                {LOGMEIN("AsmJsModule.cpp] 460\n");
                                    func->AddConst<float>((float)arg->sxFlt.dbl);
                                }
                                if (argNode != arg && argNode->nop == knopFlt)
                                {LOGMEIN("AsmJsModule.cpp] 464\n"); // last arg
                                    func->AddConst<float>((float)argNode->sxFlt.dbl);
                                }
                            } while (argNode->nop == knopList);
                        }
                    }
                }

            }
            if (evalArgs)
            {LOGMEIN("AsmJsModule.cpp] 474\n");
                ASTPrepass(pnode->sxCall.pnodeArgs, func);
            }
            break;
        }
        case knopVarDecl:
            GetByteCodeGenerator()->AssignPropertyId(pnode->name());
            ASTPrepass(pnode->sxVar.pnodeInit, func);
            break;
        // all the rest of the cases simply walk the AST
        case knopQmark:
            ASTPrepass(pnode->sxTri.pnode1, func);
            ASTPrepass(pnode->sxTri.pnode2, func);
            ASTPrepass(pnode->sxTri.pnode3, func);
            break;
        case knopList:
            do
            {LOGMEIN("AsmJsModule.cpp] 491\n");
                ParseNode * pnode1 = pnode->sxBin.pnode1;
                ASTPrepass(pnode1, func);
                pnode = pnode->sxBin.pnode2;
            } while (pnode->nop == knopList);
            ASTPrepass(pnode, func);
            break;
        case knopFor:
            ASTPrepass(pnode->sxFor.pnodeInit, func);
            ASTPrepass(pnode->sxFor.pnodeCond, func);
            ASTPrepass(pnode->sxFor.pnodeIncr, func);
            ASTPrepass(pnode->sxFor.pnodeBody, func);
            break;
        case knopIf:
            ASTPrepass(pnode->sxIf.pnodeCond, func);
            ASTPrepass(pnode->sxIf.pnodeTrue, func);
            ASTPrepass(pnode->sxIf.pnodeFalse, func);
            break;
        case knopDoWhile:
        case knopWhile:
            ASTPrepass(pnode->sxWhile.pnodeCond, func);
            ASTPrepass(pnode->sxWhile.pnodeBody, func);
            break;
        case knopReturn:
            ASTPrepass(pnode->sxReturn.pnodeExpr, func);
            break;
        case knopBlock:
            ASTPrepass(pnode->sxBlock.pnodeStmt, func);
            break;
        case knopSwitch:
            ASTPrepass(pnode->sxSwitch.pnodeVal, func);
            for (ParseNode *pnodeT = pnode->sxSwitch.pnodeCases; NULL != pnodeT; pnodeT = pnodeT->sxCase.pnodeNext)
            {
                ASTPrepass(pnodeT, func);
            }
            ASTPrepass(pnode->sxSwitch.pnodeBlock, func);
            break;
        case knopCase:
            ASTPrepass(pnode->sxCase.pnodeExpr, func);
            ASTPrepass(pnode->sxCase.pnodeBody, func);
            break;
        case knopComma:
        {LOGMEIN("AsmJsModule.cpp] 533\n");
            ParseNode *pnode1 = pnode->sxBin.pnode1;
            if (pnode1->nop == knopComma)
            {LOGMEIN("AsmJsModule.cpp] 536\n");
                // avoid recursion on very large comma expressions.
                ArenaAllocator *alloc = GetByteCodeGenerator()->GetAllocator();
                SList<ParseNode*> *rhsStack = Anew(alloc, SList<ParseNode*>, alloc);
                do {LOGMEIN("AsmJsModule.cpp] 540\n");
                    rhsStack->Push(pnode1->sxBin.pnode2);
                    pnode1 = pnode1->sxBin.pnode1;
                } while (pnode1->nop == knopComma);
                ASTPrepass(pnode1, func);
                while (!rhsStack->Empty())
                {LOGMEIN("AsmJsModule.cpp] 546\n");
                    ParseNode *pnodeRhs = rhsStack->Pop();
                    ASTPrepass(pnodeRhs, func);
                }
                Adelete(alloc, rhsStack);
            }
            else
            {
                ASTPrepass(pnode1, func);
            }
            ASTPrepass(pnode->sxBin.pnode2, func);
            break;
        }
        default:
        {
            uint flags = ParseNode::Grfnop(pnode->nop);
            if (flags&fnopUni)
            {LOGMEIN("AsmJsModule.cpp] 563\n");
                ASTPrepass(pnode->sxUni.pnode1, func);
            }
            else if (flags&fnopBin)
            {LOGMEIN("AsmJsModule.cpp] 567\n");
                ASTPrepass(pnode->sxBin.pnode1, func);
                ASTPrepass(pnode->sxBin.pnode2, func);
            }
            break;
        }
        }
    }

    void AsmJsModuleCompiler::BindArguments(ParseNode* argList)
    {LOGMEIN("AsmJsModule.cpp] 577\n");
        for (ParseNode* pnode = argList; pnode; pnode = pnode->sxVar.pnodeNext)
        {LOGMEIN("AsmJsModule.cpp] 579\n");
            GetByteCodeGenerator()->AssignPropertyId(pnode->name());
        }
    }

    bool AsmJsModuleCompiler::CompileFunction(AsmJsFunc * func, int funcIndex)
    {LOGMEIN("AsmJsModule.cpp] 585\n");
        ParseNodePtr fncNode = func->GetFncNode();
        ParseNodePtr pnodeBody = nullptr;

        Assert(fncNode->nop == knopFncDecl && fncNode->sxFnc.funcInfo && fncNode->sxFnc.funcInfo->IsDeferred() && fncNode->sxFnc.pnodeBody == NULL);

        Js::ParseableFunctionInfo* deferParseFunction = fncNode->sxFnc.funcInfo->byteCodeFunction;
        Utf8SourceInfo * utf8SourceInfo = deferParseFunction->GetUtf8SourceInfo();
        ULONG grfscr = utf8SourceInfo->GetParseFlags();
        grfscr = grfscr & (~fscrGlobalCode);
        func->SetOrigParseFlags(grfscr);
        deferParseFunction->SetGrfscr(grfscr | (grfscr & ~fscrDeferredFncExpression));
        deferParseFunction->SetSourceInfo(GetByteCodeGenerator()->GetCurrentSourceIndex(),
            fncNode,
            !!(grfscr & fscrEvalCode),
            ((grfscr & fscrDynamicCode) && !(grfscr & fscrEvalCode)));

        deferParseFunction->SetInParamsCount(fncNode->sxFnc.funcInfo->inArgsCount);
        deferParseFunction->SetReportedInParamsCount(fncNode->sxFnc.funcInfo->inArgsCount);

        if (fncNode->sxFnc.pnodeBody == NULL)
        {LOGMEIN("AsmJsModule.cpp] 606\n");
            if (!PHASE_OFF1(Js::SkipNestedDeferredPhase))
            {LOGMEIN("AsmJsModule.cpp] 608\n");
                deferParseFunction->BuildDeferredStubs(fncNode);
            }
        }
        deferParseFunction->SetIsAsmjsMode(true);
        PageAllocator tempPageAlloc(NULL, Js::Configuration::Global.flags);
        Parser ps(GetScriptContext(), FALSE, &tempPageAlloc);
        FunctionBody * funcBody;
        ParseNodePtr parseTree;

        CompileScriptException se;
        funcBody = deferParseFunction->ParseAsmJs(&ps, &se, &parseTree);
        fncNode->sxFnc.funcInfo->byteCodeFunction = funcBody;

        TRACE_BYTECODE(_u("\nDeferred parse %s\n"), funcBody->GetDisplayName());
        if (parseTree && parseTree->nop == knopProg)
        {LOGMEIN("AsmJsModule.cpp] 624\n");
            auto body = parseTree->sxProg.pnodeBody;
            if (body && body->nop == knopList)
            {LOGMEIN("AsmJsModule.cpp] 627\n");
                auto fncDecl = body->sxBin.pnode1;
                if (fncDecl && fncDecl->nop == knopFncDecl)
                {LOGMEIN("AsmJsModule.cpp] 630\n");
                    pnodeBody = fncDecl->sxFnc.pnodeBody;
                    func->SetFuncBody(funcBody);
                }
            }
        }
        GetByteCodeGenerator()->PushFuncInfo(_u("Start asm.js AST prepass"), fncNode->sxFnc.funcInfo);
        BindArguments(fncNode->sxFnc.pnodeParams);
        ASTPrepass(pnodeBody, func);
        GetByteCodeGenerator()->PopFuncInfo(_u("End asm.js AST prepass"));

        fncNode->sxFnc.pnodeBody = pnodeBody;

        if (!pnodeBody)
        {LOGMEIN("AsmJsModule.cpp] 644\n");
            // body should never be null if parsing succeeded
            Assert(UNREACHED);
            return Fail(fncNode, _u("Function should always have parse nodes"));
        }

        // Check if this function requires a bigger Ast
        UpdateMaxAstSize(fncNode->sxFnc.astSize);

        if (funcIndex == 0 && CheckChangeHeap(func))
        {LOGMEIN("AsmJsModule.cpp] 654\n");
            fncNode->sxFnc.pnodeBody = NULL;
            return true;
        }

        if (!SetupFunctionArguments(func, pnodeBody))
        {LOGMEIN("AsmJsModule.cpp] 660\n");
            // failure message will be printed by SetupFunctionArguments
            fncNode->sxFnc.pnodeBody = NULL;
            return false;
        }

        if (!SetupLocalVariables(func))
        {LOGMEIN("AsmJsModule.cpp] 667\n");
            // failure message will be printed by SetupLocalVariables
            fncNode->sxFnc.pnodeBody = NULL;
            return false;
        }

        // now that we have setup the function, we can generate bytecode for it
        AsmJSByteCodeGenerator gen(func, this);
        bool wasEmit = gen.EmitOneFunction();
        fncNode->sxFnc.pnodeBody = NULL;
        return wasEmit;
    }


    bool AsmJsModuleCompiler::SetupFunctionArguments(AsmJsFunc * func, ParseNodePtr pnode)
    {LOGMEIN("AsmJsModule.cpp] 682\n");
        // Check arguments
        ArgSlot numArguments = 0;
        ParseNode * fncNode = func->GetFncNode();
        ParseNode* argNode = ParserWrapper::FunctionArgsList(fncNode, numArguments);

        if (!func->EnsureArgCount(numArguments))
        {LOGMEIN("AsmJsModule.cpp] 689\n");
            return Fail(argNode, _u("Cannot have variable number of arguments"));
        }

        ArgSlot index = 0;
        while (argNode)
        {LOGMEIN("AsmJsModule.cpp] 695\n");
            if (pnode->nop != knopList)
            {LOGMEIN("AsmJsModule.cpp] 697\n");
                return Fail(pnode, _u("Missing assignment statement for argument"));
            }


            if (!ParserWrapper::IsDefinition(argNode))
            {LOGMEIN("AsmJsModule.cpp] 703\n");
                return Fail(argNode, _u("duplicate argument name not allowed"));
            }

            PropertyName argName = argNode->name();
            if (!AsmJSCompiler::CheckIdentifier(*this, argNode, argName))
            {LOGMEIN("AsmJsModule.cpp] 709\n");
                return false;
            }

            // creates the variable
            AsmJsVarBase* var = func->DefineVar(argName, true);
            if (!var)
            {LOGMEIN("AsmJsModule.cpp] 716\n");
                return Fail(argNode, _u("Failed to define var"));
            }

            ParseNode* argDefinition = ParserWrapper::GetBinaryLeft(pnode);
            if (argDefinition->nop != knopAsg)
            {LOGMEIN("AsmJsModule.cpp] 722\n");
                return Fail(argDefinition, _u("Expecting an assignment"));
            }

            ParseNode* lhs = ParserWrapper::GetBinaryLeft(argDefinition);
            ParseNode* rhs = ParserWrapper::GetBinaryRight(argDefinition);

#define NodeDefineThisArgument(n,var) (n->nop == knopName && ParserWrapper::VariableName(n)->GetPropertyId() == var->GetName()->GetPropertyId())

            if (!NodeDefineThisArgument(lhs, var))
            {LOGMEIN("AsmJsModule.cpp] 732\n");
                return Fail(lhs, _u("Defining wrong argument"));
            }

            if (rhs->nop == knopPos)
            {LOGMEIN("AsmJsModule.cpp] 737\n");
                // unary + => double
                var->SetVarType(AsmJsVarType::Double);
                var->SetLocation(func->AcquireRegister<double>());
                // validate stmt
                ParseNode* argSym = ParserWrapper::GetUnaryNode(rhs);

                if (!NodeDefineThisArgument(argSym, var))
                {LOGMEIN("AsmJsModule.cpp] 745\n");
                    return Fail(lhs, _u("Defining wrong argument"));
                }
            }
            else if (rhs->nop == knopOr)
            {LOGMEIN("AsmJsModule.cpp] 750\n");
                var->SetVarType(AsmJsVarType::Int);
                var->SetLocation(func->AcquireRegister<int>());

                ParseNode* argSym = ParserWrapper::GetBinaryLeft(rhs);
                ParseNode* intSym = ParserWrapper::GetBinaryRight(rhs);
                // validate stmt
                if (!NodeDefineThisArgument(argSym, var))
                {LOGMEIN("AsmJsModule.cpp] 758\n");
                    return Fail(lhs, _u("Defining wrong argument"));
                }
                if (intSym->nop != knopInt || intSym->sxInt.lw != 0)
                {LOGMEIN("AsmJsModule.cpp] 762\n");
                    return Fail(lhs, _u("Or value must be 0 when defining arguments"));
                }
            }
            else if (rhs->nop == knopCall)
            {LOGMEIN("AsmJsModule.cpp] 767\n");
                if (rhs->sxCall.pnodeTarget->nop != knopName)
                {LOGMEIN("AsmJsModule.cpp] 769\n");
                    return Fail(rhs, _u("call should be for fround"));
                }
                AsmJsFunctionDeclaration* funcDecl = this->LookupFunction(rhs->sxCall.pnodeTarget->name());

                if (!funcDecl)
                    return Fail(rhs, _u("Cannot resolve function for argument definition, or wrong function"));

                if (funcDecl->GetSymbolType() == AsmJsSymbol::MathBuiltinFunction)
                {LOGMEIN("AsmJsModule.cpp] 778\n");
                    AsmJsMathFunction* mathFunc = funcDecl->Cast<AsmJsMathFunction>();
                    if (!(mathFunc && mathFunc->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround))
                    {LOGMEIN("AsmJsModule.cpp] 781\n");
                        return Fail(rhs, _u("call should be for fround"));
                    }
                    var->SetVarType(AsmJsVarType::Float);
                    var->SetLocation(func->AcquireRegister<float>());
                }
                else if (IsSimdjsEnabled() && funcDecl->GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction)
                {LOGMEIN("AsmJsModule.cpp] 788\n");
                    AsmJsSIMDFunction* simdFunc = funcDecl->Cast<AsmJsSIMDFunction>();
                    // x = f4check(x)
                    if (!simdFunc->IsTypeCheck())
                    {LOGMEIN("AsmJsModule.cpp] 792\n");
                       return Fail(rhs, _u("Invalid SIMD argument type check. E.g. expected x = f4check(x)"));
                    }
                    if (simdFunc->IsUnsignedTypeCheck())
                    {LOGMEIN("AsmJsModule.cpp] 796\n");
                        return Fail(rhs, _u("Invalid SIMD argument type. Expecting Signed arguments."));
                    }
                    var->SetVarType(simdFunc->GetTypeCheckVarType());
                    // We don't set SIMD args reg location here. We defer that after all function locals are processed.
                    // This allows us to capture all SIMD constants from locals initializations, add them to the register space before we assign registers to args and locals.
                    func->GetSimdVarsList().Add(var);
                }
                else
                {
                    return Fail(rhs, _u("Wrong function used for argument definition"));
                }

                if (!NodeDefineThisArgument(rhs->sxCall.pnodeArgs, var))
                {LOGMEIN("AsmJsModule.cpp] 810\n");
                    return Fail(lhs, _u("Defining wrong argument"));
                }
            }
            else
            {
                return Fail(rhs, _u("arguments are not casted as valid Asm.js type"));
            }

            if (PHASE_TRACE1(ByteCodePhase))
            {LOGMEIN("AsmJsModule.cpp] 820\n");
                Output::Print(_u("    Argument [%s] Valid"), argName->Psz());
            }

            if (!func->EnsureArgType(var, index++))
            {LOGMEIN("AsmJsModule.cpp] 825\n");
                return Fail(rhs, _u("Unexpected argument type"));
            }

            argNode = ParserWrapper::NextVar(argNode);
            pnode = ParserWrapper::GetBinaryRight(pnode);
        }

        func->SetBodyNode(pnode);
        return true;
    }

    bool AsmJsModuleCompiler::SetupLocalVariables(AsmJsFunc * func)
    {LOGMEIN("AsmJsModule.cpp] 838\n");
        ParseNodePtr pnode = func->GetBodyNode();
        MathBuiltin mathBuiltin;
        AsmJsMathFunction* mathFunc = nullptr;
        AsmJsSIMDFunction* simdFunc = nullptr;
        AsmJsSIMDValue simdValue;
        simdValue.Zero();
        // define all variables
        while (pnode->nop == knopList)
        {LOGMEIN("AsmJsModule.cpp] 847\n");
            ParseNode * varNode = ParserWrapper::GetBinaryLeft(pnode);
            while (varNode && varNode->nop != knopEndCode)
            {LOGMEIN("AsmJsModule.cpp] 850\n");
                ParseNode * decl;
                if (varNode->nop == knopList)
                {LOGMEIN("AsmJsModule.cpp] 853\n");
                    decl = ParserWrapper::GetBinaryLeft(varNode);
                    varNode = ParserWrapper::GetBinaryRight(varNode);
                }
                else
                {
                    decl = varNode;
                    varNode = nullptr;
                }
                // if we have hit a non-declaration, we are done processing the function header
                if (decl->nop != knopVarDecl)
                {LOGMEIN("AsmJsModule.cpp] 864\n");
                    goto varDeclEnd;
                }
                ParseNode* pnodeInit = decl->sxVar.pnodeInit;
                AsmJsSymbol * declSym = nullptr;

                mathFunc = nullptr;
                simdFunc = nullptr;

                if (!pnodeInit)
                {LOGMEIN("AsmJsModule.cpp] 874\n");
                    return Fail(decl, _u("The righthand side of a var declaration missing an initialization (empty)"));
                }

                if (pnodeInit->nop == knopName)
                {LOGMEIN("AsmJsModule.cpp] 879\n");
                    declSym = LookupIdentifier(pnodeInit->name(), func);
                    if (!declSym || declSym->isMutable() || (declSym->GetSymbolType() != AsmJsSymbol::Variable && declSym->GetSymbolType() != AsmJsSymbol::MathConstant))
                    {LOGMEIN("AsmJsModule.cpp] 882\n");
                        return Fail(decl, _u("Var declaration with non-constant"));
                    }
                }
                else if (pnodeInit->nop == knopCall)
                {LOGMEIN("AsmJsModule.cpp] 887\n");
                    if (pnodeInit->sxCall.pnodeTarget->nop != knopName)
                    {LOGMEIN("AsmJsModule.cpp] 889\n");
                        return Fail(decl, _u("Var declaration with something else than a literal value|fround call"));
                    }
                    AsmJsFunctionDeclaration* funcDecl = this->LookupFunction(pnodeInit->sxCall.pnodeTarget->name());

                    if (!funcDecl)
                        return Fail(pnodeInit, _u("Cannot resolve function name"));

                    if (funcDecl->GetSymbolType() == AsmJsSymbol::MathBuiltinFunction)
                    {LOGMEIN("AsmJsModule.cpp] 898\n");
                        mathFunc = funcDecl->Cast<AsmJsMathFunction>();
                        if (!(mathFunc && mathFunc->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround))
                        {LOGMEIN("AsmJsModule.cpp] 901\n");
                            return Fail(decl, _u("Var declaration with something else than a literal value|fround call"));
                        }
                        if (!ParserWrapper::IsFroundNumericLiteral(pnodeInit->sxCall.pnodeArgs))
                        {LOGMEIN("AsmJsModule.cpp] 905\n");
                            return Fail(decl, _u("Var declaration with something else than a literal value|fround call"));
                        }
                    }
                    else if (IsSimdjsEnabled() && funcDecl->GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction)
                    {LOGMEIN("AsmJsModule.cpp] 910\n");
                        // var x = f4(1.0, 2.0, 3.0, 4.0);
                        simdFunc = funcDecl->Cast<AsmJsSIMDFunction>();
                        if (!ValidateSimdConstructor(pnodeInit, simdFunc, simdValue))
                        {LOGMEIN("AsmJsModule.cpp] 914\n");
                            return Fail(varNode, _u("Invalid SIMD local declaration"));
                        }
                    }
                }
                else if (pnodeInit->nop != knopInt && pnodeInit->nop != knopFlt)
                {LOGMEIN("AsmJsModule.cpp] 920\n");
                    return Fail(decl, _u("Var declaration with something else than a literal value|fround call"));
                }
                if (!AsmJSCompiler::CheckIdentifier(*this, decl, decl->name()))
                {LOGMEIN("AsmJsModule.cpp] 924\n");
                    // CheckIdentifier will print failure message
                    return false;
                }

                AsmJsVar* var = (AsmJsVar*)func->DefineVar(decl->name(), false);
                if (!var)
                {LOGMEIN("AsmJsModule.cpp] 931\n");
                    return Fail(decl, _u("Failed to define var"));
                }
                RegSlot loc = Constants::NoRegister;
                if (pnodeInit->nop == knopInt)
                {LOGMEIN("AsmJsModule.cpp] 936\n");
                    var->SetVarType(AsmJsVarType::Int);
                    var->SetLocation(func->AcquireRegister<int>());
                    var->SetConstInitialiser(pnodeInit->sxInt.lw);
                    loc = func->GetConstRegister<int>(pnodeInit->sxInt.lw);
                }
                else if (ParserWrapper::IsMinInt(pnodeInit))
                {LOGMEIN("AsmJsModule.cpp] 943\n");
                    var->SetVarType(AsmJsVarType::Int);
                    var->SetLocation(func->AcquireRegister<int>());
                    var->SetConstInitialiser(INT_MIN);
                    loc = func->GetConstRegister<int>(INT_MIN);
                }
                else if (ParserWrapper::IsUnsigned(pnodeInit))
                {LOGMEIN("AsmJsModule.cpp] 950\n");
                    var->SetVarType(AsmJsVarType::Int);
                    var->SetLocation(func->AcquireRegister<int>());
                    var->SetConstInitialiser((int)((uint32)pnodeInit->sxFlt.dbl));
                    loc = func->GetConstRegister<int>((uint32)pnodeInit->sxFlt.dbl);
                }
                else if (pnodeInit->nop == knopFlt)
                {LOGMEIN("AsmJsModule.cpp] 957\n");
                    if (pnodeInit->sxFlt.maybeInt)
                    {LOGMEIN("AsmJsModule.cpp] 959\n");
                        return Fail(decl, _u("Var declaration with integer literal outside range [-2^31, 2^32)"));
                    }
                    var->SetVarType(AsmJsVarType::Double);
                    var->SetLocation(func->AcquireRegister<double>());
                    loc = func->GetConstRegister<double>(pnodeInit->sxFlt.dbl);
                    var->SetConstInitialiser(pnodeInit->sxFlt.dbl);
                }
                else if (pnodeInit->nop == knopName)
                {LOGMEIN("AsmJsModule.cpp] 968\n");
                    if (declSym->GetSymbolType() == AsmJsSymbol::Variable)
                    {LOGMEIN("AsmJsModule.cpp] 970\n");
                        AsmJsVar * definition = declSym->Cast<AsmJsVar>();
                        switch (definition->GetVarType().which())
                        {LOGMEIN("AsmJsModule.cpp] 973\n");
                        case AsmJsVarType::Double:
                            var->SetVarType(AsmJsVarType::Double);
                            var->SetLocation(func->AcquireRegister<double>());
                            var->SetConstInitialiser(definition->GetDoubleInitialiser());
                            break;

                        case AsmJsVarType::Float:
                            var->SetVarType(AsmJsVarType::Float);
                            var->SetLocation(func->AcquireRegister<float>());
                            var->SetConstInitialiser(definition->GetFloatInitialiser());
                            break;

                        case AsmJsVarType::Int:
                            var->SetVarType(AsmJsVarType::Int);
                            var->SetLocation(func->AcquireRegister<int>());
                            var->SetConstInitialiser(definition->GetIntInitialiser());
                            break;

                        default:
                            Assume(UNREACHED);
                        }
                    }
                    else
                    {
                        Assert(declSym->GetSymbolType() == AsmJsSymbol::MathConstant);
                        Assert(declSym->GetType() == AsmJsType::Double);

                        AsmJsMathConst * definition = declSym->Cast<AsmJsMathConst>();

                        var->SetVarType(AsmJsVarType::Double);
                        var->SetLocation(func->AcquireRegister<double>());
                        var->SetConstInitialiser(*definition->GetVal());
                    }
                }
                else if (pnodeInit->nop == knopCall)
                {LOGMEIN("AsmJsModule.cpp] 1009\n");
                    if (mathFunc)
                    {LOGMEIN("AsmJsModule.cpp] 1011\n");
                        var->SetVarType(AsmJsVarType::Float);
                        var->SetLocation(func->AcquireRegister<float>());
                        if (pnodeInit->sxCall.pnodeArgs->nop == knopInt)
                        {LOGMEIN("AsmJsModule.cpp] 1015\n");
                            int iVal = pnodeInit->sxCall.pnodeArgs->sxInt.lw;
                            var->SetConstInitialiser((float)iVal);
                            loc = func->GetConstRegister<float>((float)iVal);
                        }
                        else if (ParserWrapper::IsNegativeZero(pnodeInit->sxCall.pnodeArgs))
                        {LOGMEIN("AsmJsModule.cpp] 1021\n");
                            var->SetConstInitialiser(-0.0f);
                            loc = func->GetConstRegister<float>(-0.0f);
                        }
                        else
                        {
                            // note: fround((-)NumericLiteral) is explicitly allowed for any range, so we do not need to check for maybeInt
                            Assert(pnodeInit->sxCall.pnodeArgs->nop == knopFlt);
                            float fVal = (float)pnodeInit->sxCall.pnodeArgs->sxFlt.dbl;
                            var->SetConstInitialiser((float)fVal);
                            loc = func->GetConstRegister<float>(fVal);
                        }
                    }
                    else if (IsSimdjsEnabled() && simdFunc)
                    {LOGMEIN("AsmJsModule.cpp] 1035\n");
                        // simd constructor call
                        // en-register the simdvalue constant first
                        func->AddConst<AsmJsSIMDValue>(simdValue);
                        loc = func->GetConstRegister<AsmJsSIMDValue>(simdValue);
                        var->SetConstInitialiser(simdValue);
                        var->SetVarType(simdFunc->GetConstructorVarType());
                        // add to list. assign register after all constants.
                        func->GetSimdVarsList().Add(var);
                    }
                    else
                    {
                        Assert(UNREACHED);
                    }
                }

                if (loc == Constants::NoRegister && pnodeInit->nop != knopName)
                {LOGMEIN("AsmJsModule.cpp] 1052\n");
                    return Fail(decl, _u("Cannot find Register constant for var"));
                }
            }

            if (ParserWrapper::GetBinaryRight(pnode)->nop == knopEndCode)
            {LOGMEIN("AsmJsModule.cpp] 1058\n");
                break;
            }
            pnode = ParserWrapper::GetBinaryRight(pnode);
        }

        varDeclEnd:
        // this code has to be on all exit-path from the function
        if (IsSimdjsEnabled())
        {LOGMEIN("AsmJsModule.cpp] 1067\n");
            // Now, assign registers to all SIMD vars after all constants are en-registered.
            for (int i = 0; i < func->GetSimdVarsList().Count(); i++)
            {LOGMEIN("AsmJsModule.cpp] 1070\n");
                AsmJsVarBase *var = func->GetSimdVarsList().Item(i);
                var->SetLocation(func->AcquireRegister<AsmJsSIMDValue>());
            }
            func->GetSimdVarsList().Reset(); // list not needed anymore
        }
        return true;
    }

    AsmJsFunc* AsmJsModuleCompiler::CreateNewFunctionEntry( ParseNode* pnodeFnc )
    {LOGMEIN("AsmJsModule.cpp] 1080\n");
        PropertyName name = ParserWrapper::FunctionName( pnodeFnc );
        if ( !name )
        {LOGMEIN("AsmJsModule.cpp] 1083\n");
            return nullptr;
        }

        GetByteCodeGenerator()->AssignPropertyId(name);
        AsmJsFunc* func = Anew( &mAllocator, AsmJsFunc, name, pnodeFnc, &mAllocator, mCx->scriptContext );
        if( func )
        {
            if( DefineIdentifier( name, func ) )
            {LOGMEIN("AsmJsModule.cpp] 1092\n");
                func->SetFunctionIndex( pnodeFnc->sxFnc.nestedIndex );
                // Add extra check to make sure all the slots between 0 - Count are filled with func;
                mFunctionArray.SetItem( func->GetFunctionIndex(), func );
                return func;
            }
            // Error adding function
            mAllocator.Free( func, sizeof( AsmJsFunc ) );
        }
        // Error allocating a new function
        return nullptr;
    }

    bool AsmJsModuleCompiler::CheckChangeHeap(AsmJsFunc * func)
    {LOGMEIN("AsmJsModule.cpp] 1106\n");
        ParseNode * fncNode = func->GetFncNode();
        ParseNode * pnodeBody = fncNode->sxFnc.pnodeBody;
        ParseNode * pnodeArgs = fncNode->sxFnc.pnodeParams;

        // match AST for changeHeap function.
        // it must be defined in the following format (names/whitespace can differ):
        //function changeHeap(newBuffer)
        //{
        //  if (byteLength(newBuffer) & 0xffffff ||
        //      byteLength(newBuffer) <= 0xffffff ||
        //      byteLength(newBuffer) >  0x80000000)
        //      return false;
        //  heap32 = new Int32Array(newBuffer);
        //  ...
        //  buffer = newBuffer;
        //  return true;
        //}

        // ensure function
        if (pnodeBody->nop != knopList || !pnodeArgs || pnodeArgs->nop != knopVarDecl)
        {LOGMEIN("AsmJsModule.cpp] 1127\n");
            return false;
        }

        // ensure if expression
        ParseNode * ifNode = pnodeBody->sxBin.pnode1;
        if (ifNode->nop != knopIf || ifNode->sxIf.pnodeFalse)
        {LOGMEIN("AsmJsModule.cpp] 1134\n");
            return false;
        }

        // validate "byteLength(newBuffer) >  0x80000000"
        ParseNode * orNode = ifNode->sxIf.pnodeCond;
        if (orNode->nop != knopLogOr || orNode->sxBin.pnode1->nop != knopLogOr)
        {LOGMEIN("AsmJsModule.cpp] 1141\n");
            return false;
        }
        ParseNode * cond = orNode->sxBin.pnode2;
        if (cond->nop != knopGt || !CheckByteLengthCall(cond->sxBin.pnode1, pnodeArgs) || cond->sxBin.pnode2->nop != knopFlt || cond->sxBin.pnode2->sxFlt.dbl != 2147483648.0 || !cond->sxBin.pnode2->sxFlt.maybeInt)
        {LOGMEIN("AsmJsModule.cpp] 1146\n");
            return false;
        }

        // validate "byteLength(newBuffer) <= 0xffffff"
        orNode = orNode->sxBin.pnode1;
        cond = orNode->sxBin.pnode2;
        if (cond->nop != knopLe || !CheckByteLengthCall(cond->sxBin.pnode1, pnodeArgs) || cond->sxBin.pnode2->nop != knopInt || cond->sxBin.pnode2->sxInt.lw != 0x00ffffff)
        {LOGMEIN("AsmJsModule.cpp] 1154\n");
            return false;
        }

        // validate "byteLength(newBuffer) & 0xffffff"
        cond = orNode->sxBin.pnode1;
        if (cond->nop != knopAnd || !CheckByteLengthCall(cond->sxBin.pnode1, pnodeArgs) || cond->sxBin.pnode2->nop != knopInt || cond->sxBin.pnode2->sxInt.lw != 0x00ffffff)
        {LOGMEIN("AsmJsModule.cpp] 1161\n");
            return false;
        }
        // validate "return false;"
        cond = ifNode->sxIf.pnodeTrue;
        if (!cond || cond->nop != knopReturn || cond->sxReturn.pnodeExpr->nop != knopFalse)
        {LOGMEIN("AsmJsModule.cpp] 1167\n");
            return false;
        }

        // validate heap32 = new Int32Array(newBuffer); etc.
        while (!mArrayViews.Empty())
        {LOGMEIN("AsmJsModule.cpp] 1173\n");
            // all views that were instantiated must be replaced in the order which they were instantiated
            AsmJsArrayView * requiredArrayView = mArrayViews.Dequeue();
            pnodeBody = pnodeBody->sxBin.pnode2;
            if (pnodeBody->nop != knopList)
            {LOGMEIN("AsmJsModule.cpp] 1178\n");
                return false;
            }
            ParseNode * assignNode = pnodeBody->sxBin.pnode1;
            if (assignNode->nop != knopAsg || assignNode->sxBin.pnode1->nop != knopName)
            {LOGMEIN("AsmJsModule.cpp] 1183\n");
                return false;
            }
            // validate left hand side
            AsmJsSymbol * actualArraySym = LookupIdentifier(assignNode->sxBin.pnode1->name());
            if (requiredArrayView != actualArraySym)
            {LOGMEIN("AsmJsModule.cpp] 1189\n");
                return false;
            }

            ParseNode * callNode = assignNode->sxBin.pnode2;
            // validate correct argument is passed
            if (callNode->nop != knopNew || !callNode->sxCall.pnodeArgs || callNode->sxCall.pnodeArgs->nop != knopName || callNode->sxCall.pnodeArgs->name()->GetPropertyId() != pnodeArgs->name()->GetPropertyId() || callNode->sxCall.pnodeTarget->nop != knopName)
            {LOGMEIN("AsmJsModule.cpp] 1196\n");
                return false;
            }
            // validate correct function is being called
            AsmJsSymbol * callTargetSym = LookupIdentifier(callNode->sxCall.pnodeTarget->name());
            if (!callTargetSym || callTargetSym->GetSymbolType() != AsmJsSymbol::TypedArrayBuiltinFunction)
            {LOGMEIN("AsmJsModule.cpp] 1202\n");
                return false;
            }
            if (requiredArrayView->GetViewType() != callTargetSym->Cast<AsmJsTypedArrayFunction>()->GetViewType())
            {LOGMEIN("AsmJsModule.cpp] 1206\n");
                return false;
            }
        }
        pnodeBody = pnodeBody->sxBin.pnode2;
        if (pnodeBody->nop != knopList)
        {LOGMEIN("AsmJsModule.cpp] 1212\n");
            return false;
        }

        // validate buffer = newBuffer;
        ParseNode * assign = pnodeBody->sxBin.pnode1;
        if (assign->nop != knopAsg || assign->sxBin.pnode1->nop != knopName || !mBufferArgName || mBufferArgName->GetPropertyId() != assign->sxBin.pnode1->name()->GetPropertyId() ||
            assign->sxBin.pnode2->nop != knopName || pnodeArgs->name()->GetPropertyId() != assign->sxBin.pnode2->name()->GetPropertyId())
        {LOGMEIN("AsmJsModule.cpp] 1220\n");
            return false;
        }
        // validate return true;
        pnodeBody = pnodeBody->sxBin.pnode2;
        if (pnodeBody->nop != knopList || pnodeBody->sxBin.pnode2->nop != knopEndCode ||
            pnodeBody->sxBin.pnode1->nop != knopReturn || !pnodeBody->sxBin.pnode1->sxReturn.pnodeExpr || pnodeBody->sxBin.pnode1->sxReturn.pnodeExpr->nop != knopTrue)
        {LOGMEIN("AsmJsModule.cpp] 1227\n");
            return false;
        }
        // now we should flag this module as containing changeHeap method
        mUsesChangeHeap = true;
        AsmJSByteCodeGenerator::EmitEmptyByteCode(func->GetFuncInfo(), GetByteCodeGenerator(), fncNode);
        return true;
    }

    bool AsmJsModuleCompiler::CheckByteLengthCall(ParseNode * callNode, ParseNode * bufferDecl)
    {LOGMEIN("AsmJsModule.cpp] 1237\n");
        if (callNode->nop != knopCall || callNode->sxCall.pnodeTarget->nop != knopName)
        {LOGMEIN("AsmJsModule.cpp] 1239\n");
            return false;
        }
        AsmJsSymbol* funcDecl = LookupIdentifier(callNode->sxCall.pnodeTarget->name());
        if (!funcDecl || funcDecl->GetSymbolType() != AsmJsSymbol::TypedArrayBuiltinFunction)
        {LOGMEIN("AsmJsModule.cpp] 1244\n");
            return false;
        }

        AsmJsTypedArrayFunction* arrayFunc = funcDecl->Cast<AsmJsTypedArrayFunction>();
        return callNode->sxCall.argCount == 1 &&
            !callNode->sxCall.isApplyCall &&
            !callNode->sxCall.isEvalCall &&
            callNode->sxCall.spreadArgCount == 0 &&
            arrayFunc->GetArrayBuiltInFunction() == AsmJSTypedArrayBuiltin_byteLength &&
            callNode->sxCall.pnodeArgs->nop == knopName &&
            callNode->sxCall.pnodeArgs->name()->GetPropertyId() == bufferDecl->name()->GetPropertyId();
    }

    bool AsmJsModuleCompiler::Fail( ParseNode* usepn, const wchar *error )
    {LOGMEIN("AsmJsModule.cpp] 1259\n");
        AsmJSCompiler::OutputError(GetScriptContext(), error);
        return false;
    }

    bool AsmJsModuleCompiler::FailName( ParseNode *usepn, const wchar *fmt, PropertyName name )
    {LOGMEIN("AsmJsModule.cpp] 1265\n");
        AsmJSCompiler::OutputError(GetScriptContext(), fmt, name->Psz());
        return false;
    }

    bool AsmJsModuleCompiler::LookupStandardLibraryMathName( PropertyName name, MathBuiltin *mathBuiltin ) const
    {LOGMEIN("AsmJsModule.cpp] 1271\n");
        return mStandardLibraryMathNames.TryGetValue( name->GetPropertyId(), mathBuiltin );
    }

    bool AsmJsModuleCompiler::LookupStandardLibraryArrayName(PropertyName name, TypedArrayBuiltin *builtin) const
    {LOGMEIN("AsmJsModule.cpp] 1276\n");
        return mStandardLibraryArrayNames.TryGetValue(name->GetPropertyId(), builtin);
    }

    void AsmJsModuleCompiler::InitBufferArgName( PropertyName n )
    {LOGMEIN("AsmJsModule.cpp] 1281\n");
#if DBG
        Assert( !mBufferArgNameInit );
        mBufferArgNameInit = true;
#endif
        mBufferArgName = n;
    }

    void AsmJsModuleCompiler::InitForeignArgName( PropertyName n )
    {LOGMEIN("AsmJsModule.cpp] 1290\n");
#if DBG
        Assert( !mForeignArgNameInit );
        mForeignArgNameInit = true;
#endif
        mForeignArgName = n;
    }

    void AsmJsModuleCompiler::InitStdLibArgName( PropertyName n )
    {LOGMEIN("AsmJsModule.cpp] 1299\n");
#if DBG
        Assert( !mStdLibArgNameInit );
        mStdLibArgNameInit = true;
#endif
        mStdLibArgName = n;
    }

    Js::PropertyName AsmJsModuleCompiler::GetStdLibArgName() const
    {LOGMEIN("AsmJsModule.cpp] 1308\n");
#if DBG
        Assert( mBufferArgNameInit );
#endif
        return mStdLibArgName;
    }

    Js::PropertyName AsmJsModuleCompiler::GetForeignArgName() const
    {LOGMEIN("AsmJsModule.cpp] 1316\n");
#if DBG
        Assert( mForeignArgNameInit );
#endif
        return mForeignArgName;
    }

    Js::PropertyName AsmJsModuleCompiler::GetBufferArgName() const
    {LOGMEIN("AsmJsModule.cpp] 1324\n");
#if DBG
        Assert( mStdLibArgNameInit );
#endif
        return mBufferArgName;
    }

    bool AsmJsModuleCompiler::Init()
    {LOGMEIN("AsmJsModule.cpp] 1332\n");
        if( mInitialised )
        {LOGMEIN("AsmJsModule.cpp] 1334\n");
            return false;
        }
        mInitialised = true;

        struct MathFunc
        {
            MathFunc( PropertyId id_ = 0, AsmJsMathFunction* val_ = nullptr ) :
                id( id_ ), val( val_ )
            {LOGMEIN("AsmJsModule.cpp] 1343\n");
            }
            PropertyId id;
            AsmJsMathFunction* val;
        };
        MathFunc mathFunctions[AsmJSMathBuiltinFunction_COUNT];
        // we could move the mathBuiltinFuncname to MathFunc struct
        mathFunctions[AsmJSMathBuiltin_sin   ] = MathFunc(PropertyIds::sin   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_sin   , OpCodeAsmJs::Sin_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_cos   ] = MathFunc(PropertyIds::cos   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_cos   , OpCodeAsmJs::Cos_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_tan   ] = MathFunc(PropertyIds::tan   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_tan   , OpCodeAsmJs::Tan_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_asin  ] = MathFunc(PropertyIds::asin  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_asin  , OpCodeAsmJs::Asin_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_acos  ] = MathFunc(PropertyIds::acos  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_acos  , OpCodeAsmJs::Acos_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_atan  ] = MathFunc(PropertyIds::atan  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_atan  , OpCodeAsmJs::Atan_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_ceil  ] = MathFunc(PropertyIds::ceil  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_ceil  , OpCodeAsmJs::Ceil_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_floor ] = MathFunc(PropertyIds::floor , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_floor , OpCodeAsmJs::Floor_Db , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_exp   ] = MathFunc(PropertyIds::exp   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_exp   , OpCodeAsmJs::Exp_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_log   ] = MathFunc(PropertyIds::log   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_log   , OpCodeAsmJs::Log_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_pow   ] = MathFunc(PropertyIds::pow   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_pow   , OpCodeAsmJs::Pow_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble ));
        mathFunctions[AsmJSMathBuiltin_sqrt  ] = MathFunc(PropertyIds::sqrt  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_sqrt  , OpCodeAsmJs::Sqrt_Db  , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_abs   ] = MathFunc(PropertyIds::abs   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_abs   , OpCodeAsmJs::Abs_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble                      ));
        mathFunctions[AsmJSMathBuiltin_atan2 ] = MathFunc(PropertyIds::atan2 , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_atan2 , OpCodeAsmJs::Atan2_Db , AsmJsRetType::Double, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble ));
        mathFunctions[AsmJSMathBuiltin_imul  ] = MathFunc(PropertyIds::imul  , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_imul  , OpCodeAsmJs::Imul_Int , AsmJsRetType::Signed, AsmJsType::Intish     , AsmJsType::Intish      ));
        mathFunctions[AsmJSMathBuiltin_fround] = MathFunc(PropertyIds::fround, Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_fround, OpCodeAsmJs::Fround_Flt,AsmJsRetType::Float , AsmJsType::Floatish                            ));
        mathFunctions[AsmJSMathBuiltin_min   ] = MathFunc(PropertyIds::min   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_min   , OpCodeAsmJs::Min_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble));
        mathFunctions[AsmJSMathBuiltin_max   ] = MathFunc(PropertyIds::max   , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_max   , OpCodeAsmJs::Max_Db   , AsmJsRetType::Double, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble));
        mathFunctions[AsmJSMathBuiltin_clz32 ] = MathFunc(PropertyIds::clz32 , Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_clz32 , OpCodeAsmJs::Clz32_Int, AsmJsRetType::Fixnum, AsmJsType::Intish));

        mathFunctions[AsmJSMathBuiltin_abs].val->SetOverload(Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_abs, OpCodeAsmJs::Abs_Int, AsmJsRetType::Unsigned, AsmJsType::Signed));
        mathFunctions[AsmJSMathBuiltin_min].val->SetOverload(Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_min, OpCodeAsmJs::Min_Int, AsmJsRetType::Signed,  AsmJsType::Signed,  AsmJsType::Signed));
        mathFunctions[AsmJSMathBuiltin_max].val->SetOverload(Anew( &mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 2, AsmJSMathBuiltin_max, OpCodeAsmJs::Max_Int, AsmJsRetType::Signed,  AsmJsType::Signed,  AsmJsType::Signed));

        //Float Overloads
        mathFunctions[AsmJSMathBuiltin_fround].val->SetOverload(Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_fround, OpCodeAsmJs::Fround_Db,  AsmJsRetType::Float, AsmJsType::MaybeDouble));
        mathFunctions[AsmJSMathBuiltin_fround].val->SetOverload(Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_fround, OpCodeAsmJs::Fround_Int, AsmJsRetType::Float, AsmJsType::Int));// should we split this into signed and unsigned?
        mathFunctions[AsmJSMathBuiltin_abs].val->SetOverload(   Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_abs,    OpCodeAsmJs::Abs_Flt,    AsmJsRetType::Floatish, AsmJsType::MaybeFloat));
        mathFunctions[AsmJSMathBuiltin_ceil].val->SetOverload(  Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_ceil,   OpCodeAsmJs::Ceil_Flt,   AsmJsRetType::Floatish, AsmJsType::MaybeFloat));
        mathFunctions[AsmJSMathBuiltin_floor].val->SetOverload( Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_floor,  OpCodeAsmJs::Floor_Flt,  AsmJsRetType::Floatish, AsmJsType::MaybeFloat));
        mathFunctions[AsmJSMathBuiltin_sqrt].val->SetOverload(  Anew(&mAllocator, AsmJsMathFunction, nullptr, &mAllocator, 1, AsmJSMathBuiltin_sqrt,   OpCodeAsmJs::Sqrt_Flt,   AsmJsRetType::Floatish, AsmJsType::MaybeFloat));

        for (int i = 0; i < AsmJSMathBuiltinFunction_COUNT ; i++)
        {LOGMEIN("AsmJsModule.cpp] 1383\n");
            if( !AddStandardLibraryMathName( (PropertyId)mathFunctions[i].id, mathFunctions[i].val, mathFunctions[i].val->GetMathBuiltInFunction() ) )
            {LOGMEIN("AsmJsModule.cpp] 1385\n");
                return false;
            }
        }

        struct ConstMath
        {
            ConstMath( PropertyId id_, const double* val_, AsmJSMathBuiltinFunction mathLibConstName_):
                id(id_), val(val_), mathLibConstName(mathLibConstName_) {LOGMEIN("AsmJsModule.cpp] 1393\n"); }
            PropertyId id;
            AsmJSMathBuiltinFunction mathLibConstName;
            const double* val;
        };
        ConstMath constMath[] = {
            ConstMath( PropertyIds::E       , &Math::E                           , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_e ),
            ConstMath(PropertyIds::LN10     , &Math::LN10                        , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln10),
            ConstMath(PropertyIds::LN2      , &Math::LN2                         , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln2),
            ConstMath(PropertyIds::LOG2E    , &Math::LOG2E                       , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log2e),
            ConstMath(PropertyIds::LOG10E   , &Math::LOG10E                      , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log10e),
            ConstMath(PropertyIds::PI       , &Math::PI                          , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_pi),
            ConstMath(PropertyIds::SQRT1_2  , &Math::SQRT1_2                     , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt1_2),
            ConstMath(PropertyIds::SQRT2    , &Math::SQRT2                       , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt2),
            ConstMath(PropertyIds::Infinity , &NumberConstants::POSITIVE_INFINITY, AsmJSMathBuiltinFunction::AsmJSMathBuiltin_infinity),
            ConstMath(PropertyIds::NaN      , &NumberConstants::NaN              , AsmJSMathBuiltinFunction::AsmJSMathBuiltin_nan),
        };
        const int size = sizeof( constMath ) / sizeof( ConstMath );
        for (int i = 0; i < size ; i++)
        {LOGMEIN("AsmJsModule.cpp] 1412\n");
            if( !AddStandardLibraryMathName( constMath[i].id, constMath[i].val, constMath[i].mathLibConstName ) )
            {LOGMEIN("AsmJsModule.cpp] 1414\n");
                return false;
            }
        }


        struct ArrayFunc
        {
            ArrayFunc(PropertyId id_ = 0, AsmJsTypedArrayFunction* val_ = nullptr) :
                id(id_), val(val_)
            {LOGMEIN("AsmJsModule.cpp] 1424\n");
            }
            PropertyId id;
            AsmJsTypedArrayFunction* val;
        };

        ArrayFunc arrayFunctions[AsmJSMathBuiltinFunction_COUNT];
        arrayFunctions[AsmJSTypedArrayBuiltin_Int8Array   ] = ArrayFunc(PropertyIds::Int8Array,    Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Int8Array,    ArrayBufferView::TYPE_INT8));
        arrayFunctions[AsmJSTypedArrayBuiltin_Uint8Array  ] = ArrayFunc(PropertyIds::Uint8Array,   Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Uint8Array,   ArrayBufferView::TYPE_UINT8));
        arrayFunctions[AsmJSTypedArrayBuiltin_Int16Array  ] = ArrayFunc(PropertyIds::Int16Array,   Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Int16Array,   ArrayBufferView::TYPE_INT16));
        arrayFunctions[AsmJSTypedArrayBuiltin_Uint16Array ] = ArrayFunc(PropertyIds::Uint16Array,  Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Uint16Array,  ArrayBufferView::TYPE_UINT16));
        arrayFunctions[AsmJSTypedArrayBuiltin_Int32Array  ] = ArrayFunc(PropertyIds::Int32Array,   Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Int32Array,   ArrayBufferView::TYPE_INT32));
        arrayFunctions[AsmJSTypedArrayBuiltin_Uint32Array ] = ArrayFunc(PropertyIds::Uint32Array,  Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Uint32Array,  ArrayBufferView::TYPE_UINT32));
        arrayFunctions[AsmJSTypedArrayBuiltin_Float32Array] = ArrayFunc(PropertyIds::Float32Array, Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Float32Array, ArrayBufferView::TYPE_FLOAT32));
        arrayFunctions[AsmJSTypedArrayBuiltin_Float64Array] = ArrayFunc(PropertyIds::Float64Array, Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_Float64Array, ArrayBufferView::TYPE_FLOAT64));
        arrayFunctions[AsmJSTypedArrayBuiltin_byteLength  ] = ArrayFunc(PropertyIds::byteLength,   Anew(&mAllocator, AsmJsTypedArrayFunction, nullptr, &mAllocator, AsmJSTypedArrayBuiltin_byteLength,   ArrayBufferView::TYPE_COUNT));

        for (int i = 0; i < AsmJSTypedArrayBuiltin_COUNT; i++)
        {LOGMEIN("AsmJsModule.cpp] 1442\n");
            if (!AddStandardLibraryArrayName((PropertyId)arrayFunctions[i].id, arrayFunctions[i].val, arrayFunctions[i].val->GetArrayBuiltInFunction()))
            {LOGMEIN("AsmJsModule.cpp] 1444\n");
                return false;
            }
        }
        // similar to math functions maps initialization.
        if (IsSimdjsEnabled())
        {LOGMEIN("AsmJsModule.cpp] 1450\n");
            if (!InitSIMDBuiltins())
            {LOGMEIN("AsmJsModule.cpp] 1452\n");
                return false;
            }
        }
        return true;
    }

    bool AsmJsModuleCompiler::InitSIMDBuiltins()
    {LOGMEIN("AsmJsModule.cpp] 1460\n");
        struct SIMDFunc
        {
            SIMDFunc(PropertyId id_ = 0, AsmJsSIMDFunction* val_ = nullptr) :
            id(id_), val(val_)
            {LOGMEIN("AsmJsModule.cpp] 1465\n");
            }
            PropertyId id;
            AsmJsSIMDFunction* val;
        };

        SIMDFunc simdFunctions[AsmJsSIMDBuiltin_COUNT];

        // !! NOTE: Keep these grouped by SIMD type

        /* Int32x4 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Int32x4]                     = SIMDFunc(PropertyIds::Int32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_Int32x4, OpCodeAsmJs::Simd128_IntsToI4, AsmJsRetType::Int32x4, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_check]               = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_check, OpCodeAsmJs::Simd128_Ld_I4 /*no dynamic checks*/, AsmJsRetType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_splat]               = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_splat, OpCodeAsmJs::Simd128_Splat_I4, AsmJsRetType::Int32x4, AsmJsType::Intish));
#if 0
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromFloat64x2]       = SIMDFunc(PropertyIds::fromFloat64x2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromFloat64x2, OpCodeAsmJs::Simd128_FromFloat64x2_I4, AsmJsRetType::Int32x4, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromFloat64x2Bits]   = SIMDFunc(PropertyIds::fromFloat64x2Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromFloat64x2Bits, OpCodeAsmJs::Simd128_FromFloat64x2Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Float64x2));
#endif
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromFloat32x4]       = SIMDFunc(PropertyIds::fromFloat32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromFloat32x4, OpCodeAsmJs::Simd128_FromFloat32x4_I4, AsmJsRetType::Int32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromFloat32x4Bits]   = SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Float32x4));

        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromInt16x8Bits]     = SIMDFunc(PropertyIds::fromInt16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromInt16x8Bits, OpCodeAsmJs::Simd128_FromInt16x8Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromInt8x16Bits]     = SIMDFunc(PropertyIds::fromInt8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromInt8x16Bits, OpCodeAsmJs::Simd128_FromInt8x16Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Int8x16));

        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromUint32x4Bits]    = SIMDFunc(PropertyIds::fromUint32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromUint32x4Bits, OpCodeAsmJs::Simd128_FromUint32x4Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromUint16x8Bits]    = SIMDFunc(PropertyIds::fromUint16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromUint16x8Bits, OpCodeAsmJs::Simd128_FromUint16x8Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_fromUint8x16Bits]    = SIMDFunc(PropertyIds::fromUint8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_fromUint8x16Bits, OpCodeAsmJs::Simd128_FromUint8x16Bits_I4, AsmJsRetType::Int32x4, AsmJsType::Uint8x16));

        simdFunctions[AsmJsSIMDBuiltin_int32x4_neg]                 = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_neg, OpCodeAsmJs::Simd128_Neg_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_add]                 = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_add, OpCodeAsmJs::Simd128_Add_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_sub]                 = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_sub, OpCodeAsmJs::Simd128_Sub_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_mul]                 = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_mul, OpCodeAsmJs::Simd128_Mul_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));

        simdFunctions[AsmJsSIMDBuiltin_int32x4_swizzle]             = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 5, AsmJsSIMDBuiltin_int32x4_swizzle, OpCodeAsmJs::Simd128_Swizzle_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_shuffle]             = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 6, AsmJsSIMDBuiltin_int32x4_shuffle, OpCodeAsmJs::Simd128_Shuffle_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_extractLane]         = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_extractLane, OpCodeAsmJs::Simd128_ExtractLane_I4, AsmJsRetType::Signed, AsmJsType::Int32x4, AsmJsType::Int));

        simdFunctions[AsmJsSIMDBuiltin_int32x4_replaceLane]         = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_lessThan]            = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_lessThan, OpCodeAsmJs::Simd128_Lt_I4, AsmJsRetType::Bool32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_lessThanOrEqual]     = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_I4, AsmJsRetType::Bool32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_equal]               = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_equal, OpCodeAsmJs::Simd128_Eq_I4, AsmJsRetType::Bool32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_notEqual]            = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_notEqual, OpCodeAsmJs::Simd128_Neq_I4, AsmJsRetType::Bool32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_greaterThan]         = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_greaterThan, OpCodeAsmJs::Simd128_Gt_I4, AsmJsRetType::Bool32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_greaterThanOrEqual]  = SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_I4, AsmJsRetType::Bool32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_select]              = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_select, OpCodeAsmJs::Simd128_Select_I4, AsmJsRetType::Int32x4, AsmJsType::Bool32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_and]                 = SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_and, OpCodeAsmJs::Simd128_And_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_or]                  = SIMDFunc(PropertyIds::or_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_or, OpCodeAsmJs::Simd128_Or_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_xor]                 = SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_xor, OpCodeAsmJs::Simd128_Xor_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_not]                 = SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int32x4_not, OpCodeAsmJs::Simd128_Not_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4));

        simdFunctions[AsmJsSIMDBuiltin_int32x4_shiftLeftByScalar]   = SIMDFunc(PropertyIds::shiftLeftByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_shiftLeftByScalar, OpCodeAsmJs::Simd128_ShLtByScalar_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_shiftRightByScalar]  = SIMDFunc(PropertyIds::shiftRightByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_shiftRightByScalar, OpCodeAsmJs::Simd128_ShRtByScalar_I4, AsmJsRetType::Int32x4, AsmJsType::Int32x4, AsmJsType::Int));

        // Loads and Stores
        // We fill Void for the tarray type. This is ok since we special handle these ops.
        simdFunctions[AsmJsSIMDBuiltin_int32x4_load]                = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_load, OpCodeAsmJs::Simd128_LdArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_load1]               = SIMDFunc(PropertyIds::load1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_load1, OpCodeAsmJs::Simd128_LdArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_load2]               = SIMDFunc(PropertyIds::load2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_load2, OpCodeAsmJs::Simd128_LdArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_load3]               = SIMDFunc(PropertyIds::load3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int32x4_load3, OpCodeAsmJs::Simd128_LdArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_store]               = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_store, OpCodeAsmJs::Simd128_StArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_store1]              = SIMDFunc(PropertyIds::store1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_store1, OpCodeAsmJs::Simd128_StArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_store2]              = SIMDFunc(PropertyIds::store2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_store2, OpCodeAsmJs::Simd128_StArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int32x4_store3]              = SIMDFunc(PropertyIds::store3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int32x4_store3, OpCodeAsmJs::Simd128_StArr_I4, AsmJsRetType::Int32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int32x4));

        /* Float32x4 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Float32x4]                   = SIMDFunc(PropertyIds::Float32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_Float32x4, OpCodeAsmJs::Simd128_FloatsToF4, AsmJsRetType::Float32x4, AsmJsType::FloatishDoubleLit, AsmJsType::FloatishDoubleLit, AsmJsType::FloatishDoubleLit, AsmJsType::FloatishDoubleLit));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_check]             = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_check, OpCodeAsmJs::Simd128_Ld_F4 /*no dynamic checks*/, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_splat]             = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_splat, OpCodeAsmJs::Simd128_Splat_F4, AsmJsRetType::Float32x4, AsmJsType::FloatishDoubleLit));
#if 0
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromFloat64x2]     = SIMDFunc(PropertyIds::fromFloat64x2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromFloat64x2, OpCodeAsmJs::Simd128_FromFloat64x2_F4, AsmJsRetType::Float32x4, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromFloat64x2Bits] = SIMDFunc(PropertyIds::fromFloat64x2Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromFloat64x2Bits, OpCodeAsmJs::Simd128_FromFloat64x2Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Float64x2));
#endif
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromInt32x4]       = SIMDFunc(PropertyIds::fromInt32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromInt32x4, OpCodeAsmJs::Simd128_FromInt32x4_F4, AsmJsRetType::Float32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromUint32x4]      = SIMDFunc(PropertyIds::fromUint32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromUint32x4, OpCodeAsmJs::Simd128_FromUint32x4_F4, AsmJsRetType::Float32x4, AsmJsType::Uint32x4));

        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromInt32x4Bits]   = SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromInt16x8Bits]   = SIMDFunc(PropertyIds::fromInt16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromInt16x8Bits, OpCodeAsmJs::Simd128_FromInt16x8Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromInt8x16Bits]   = SIMDFunc(PropertyIds::fromInt8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromInt8x16Bits, OpCodeAsmJs::Simd128_FromInt8x16Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromUint32x4Bits]  = SIMDFunc(PropertyIds::fromUint32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromUint32x4Bits, OpCodeAsmJs::Simd128_FromUint32x4Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromUint16x8Bits]  = SIMDFunc(PropertyIds::fromUint16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromUint16x8Bits, OpCodeAsmJs::Simd128_FromUint16x8Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_fromUint8x16Bits]  = SIMDFunc(PropertyIds::fromUint8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_fromUint8x16Bits, OpCodeAsmJs::Simd128_FromUint8x16Bits_F4, AsmJsRetType::Float32x4, AsmJsType::Uint8x16));

        simdFunctions[AsmJsSIMDBuiltin_float32x4_abs]               = SIMDFunc(PropertyIds::abs, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_abs, OpCodeAsmJs::Simd128_Abs_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_neg]               = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_neg, OpCodeAsmJs::Simd128_Neg_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_add]               = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_add, OpCodeAsmJs::Simd128_Add_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_sub]               = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_sub, OpCodeAsmJs::Simd128_Sub_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_mul]               = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_mul, OpCodeAsmJs::Simd128_Mul_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_div]               = SIMDFunc(PropertyIds::div, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_div, OpCodeAsmJs::Simd128_Div_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_min]               = SIMDFunc(PropertyIds::min, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_min, OpCodeAsmJs::Simd128_Min_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_max]               = SIMDFunc(PropertyIds::max, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_max, OpCodeAsmJs::Simd128_Max_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_reciprocal]        = SIMDFunc(PropertyIds::reciprocalApproximation, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_reciprocal, OpCodeAsmJs::Simd128_Rcp_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_reciprocalSqrt]    = SIMDFunc(PropertyIds::reciprocalSqrtApproximation, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_reciprocalSqrt, OpCodeAsmJs::Simd128_RcpSqrt_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_sqrt]              = SIMDFunc(PropertyIds::sqrt, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float32x4_sqrt, OpCodeAsmJs::Simd128_Sqrt_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_swizzle]           = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 5, AsmJsSIMDBuiltin_float32x4_swizzle, OpCodeAsmJs::Simd128_Swizzle_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_shuffle]           = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 6, AsmJsSIMDBuiltin_float32x4_shuffle, OpCodeAsmJs::Simd128_Shuffle_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Float32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_extractLane]       = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_extractLane, OpCodeAsmJs::Simd128_ExtractLane_F4, AsmJsRetType::Float, AsmJsType::Float32x4, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_replaceLane]       = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_F4, AsmJsRetType::Float32x4, AsmJsType::Float32x4, AsmJsType::Int, AsmJsType::FloatishDoubleLit));

        simdFunctions[AsmJsSIMDBuiltin_float32x4_lessThan]          = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_lessThan, OpCodeAsmJs::Simd128_Lt_F4, AsmJsRetType::Bool32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_lessThanOrEqual]   = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_F4, AsmJsRetType::Bool32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_equal]             = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_equal, OpCodeAsmJs::Simd128_Eq_F4, AsmJsRetType::Bool32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_notEqual]          = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_notEqual, OpCodeAsmJs::Simd128_Neq_F4, AsmJsRetType::Bool32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_greaterThan]       = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_greaterThan, OpCodeAsmJs::Simd128_Gt_F4, AsmJsRetType::Bool32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_greaterThanOrEqual]= SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_F4, AsmJsRetType::Bool32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_select]            = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_select, OpCodeAsmJs::Simd128_Select_F4, AsmJsRetType::Float32x4, AsmJsType::Bool32x4, AsmJsType::Float32x4, AsmJsType::Float32x4));

        simdFunctions[AsmJsSIMDBuiltin_float32x4_load]              = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_load, OpCodeAsmJs::Simd128_LdArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_load1]             = SIMDFunc(PropertyIds::load1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_load1, OpCodeAsmJs::Simd128_LdArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_load2]             = SIMDFunc(PropertyIds::load2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_load2, OpCodeAsmJs::Simd128_LdArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_load3]             = SIMDFunc(PropertyIds::load3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float32x4_load3, OpCodeAsmJs::Simd128_LdArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_store]             = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_store, OpCodeAsmJs::Simd128_StArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_store1]            = SIMDFunc(PropertyIds::store1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_store1, OpCodeAsmJs::Simd128_StArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_store2]            = SIMDFunc(PropertyIds::store2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_store2, OpCodeAsmJs::Simd128_StArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float32x4_store3]            = SIMDFunc(PropertyIds::store3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float32x4_store3, OpCodeAsmJs::Simd128_StArr_F4, AsmJsRetType::Float32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float32x4));

        /* Float64x2 builtins*/
        //-------------------
#if 0
        simdFunctions[AsmJsSIMDBuiltin_Float64x2]                   = SIMDFunc(PropertyIds::Float64x2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_Float64x2, OpCodeAsmJs::Simd128_DoublesToD2, AsmJsRetType::Float64x2, AsmJsType::MaybeDouble, AsmJsType::MaybeDouble));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_check]             = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_check, OpCodeAsmJs::Simd128_Ld_D2 /*no dynamic checks*/, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_splat]             = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_splat, OpCodeAsmJs::Simd128_Splat_D2, AsmJsRetType::Float64x2, AsmJsType::Double));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_fromFloat32x4]     = SIMDFunc(PropertyIds::fromFloat32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_fromFloat32x4, OpCodeAsmJs::Simd128_FromFloat32x4_D2, AsmJsRetType::Float64x2, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_fromFloat32x4Bits] = SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_D2, AsmJsRetType::Float64x2, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_fromInt32x4]       = SIMDFunc(PropertyIds::fromInt32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_fromInt32x4, OpCodeAsmJs::Simd128_FromInt32x4_D2, AsmJsRetType::Float64x2, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_fromInt32x4Bits]   = SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_D2, AsmJsRetType::Float64x2, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_abs]               = SIMDFunc(PropertyIds::abs, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_abs, OpCodeAsmJs::Simd128_Abs_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_neg]               = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_neg, OpCodeAsmJs::Simd128_Neg_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_add]               = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_add, OpCodeAsmJs::Simd128_Add_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_sub]               = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_sub, OpCodeAsmJs::Simd128_Sub_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_mul]               = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_mul, OpCodeAsmJs::Simd128_Mul_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_div]               = SIMDFunc(PropertyIds::div, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_div, OpCodeAsmJs::Simd128_Div_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_min]               = SIMDFunc(PropertyIds::min, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_min, OpCodeAsmJs::Simd128_Min_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_max]               = SIMDFunc(PropertyIds::max, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_max, OpCodeAsmJs::Simd128_Max_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_reciprocal]        = SIMDFunc(PropertyIds::reciprocalApproximation, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1,  AsmJsSIMDBuiltin_float64x2_reciprocal, OpCodeAsmJs::Simd128_Rcp_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_reciprocalSqrt]    = SIMDFunc(PropertyIds::reciprocalSqrtApproximation, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_reciprocalSqrt, OpCodeAsmJs::Simd128_RcpSqrt_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_sqrt]              = SIMDFunc(PropertyIds::sqrt, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_float64x2_sqrt, OpCodeAsmJs::Simd128_Sqrt_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_swizzle]           = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_swizzle, OpCodeAsmJs::Simd128_Swizzle_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_shuffle]           = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_float64x2_shuffle, OpCodeAsmJs::Simd128_Shuffle_D2, AsmJsRetType::Float64x2, AsmJsType::Float64x2, AsmJsType::Float64x2, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_lessThan]          = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_lessThan, OpCodeAsmJs::Simd128_Lt_D2, AsmJsRetType::Bool32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_lessThanOrEqual]   = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_D2, AsmJsRetType::Bool32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_equal]             = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_equal, OpCodeAsmJs::Simd128_Eq_D2, AsmJsRetType::Bool32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_notEqual]          = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_notEqual, OpCodeAsmJs::Simd128_Neq_D2, AsmJsRetType::Bool32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_greaterThan]       = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_greaterThan, OpCodeAsmJs::Simd128_Gt_D2, AsmJsRetType::Bool32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_greaterThanOrEqual]= SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_D2, AsmJsRetType::Bool32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_select]            = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_select, OpCodeAsmJs::Simd128_Select_D2, AsmJsRetType::Float64x2, AsmJsType::Bool32x4, AsmJsType::Float64x2, AsmJsType::Float64x2));

        simdFunctions[AsmJsSIMDBuiltin_float64x2_load]              = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_load, OpCodeAsmJs::Simd128_LdArr_D2, AsmJsRetType::Float64x2, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_load1]             = SIMDFunc(PropertyIds::load1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_float64x2_load1, OpCodeAsmJs::Simd128_LdArr_D2, AsmJsRetType::Float64x2, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_store]             = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_store, OpCodeAsmJs::Simd128_StArr_D2, AsmJsRetType::Float64x2, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float64x2));
        simdFunctions[AsmJsSIMDBuiltin_float64x2_store1]            = SIMDFunc(PropertyIds::store1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_float64x2_store1, OpCodeAsmJs::Simd128_StArr_D2, AsmJsRetType::Float64x2, AsmJsType::Void, AsmJsType::Int, AsmJsType::Float64x2));
#endif
        /* Int16x8 */
        simdFunctions[AsmJsSIMDBuiltin_Int16x8]                     = SIMDFunc(PropertyIds::Int16x8, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 8, AsmJsSIMDBuiltin_Int16x8, OpCodeAsmJs::Simd128_IntsToI8, AsmJsRetType::Int16x8,
                                                                               AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_check]               = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_check, OpCodeAsmJs::Simd128_Ld_I8 /*no dynamic checks*/, AsmJsRetType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_extractLane]         = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_extractLane, OpCodeAsmJs::Simd128_ExtractLane_I8, AsmJsRetType::Signed, AsmJsType::Int16x8, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_swizzle]             = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 9, AsmJsSIMDBuiltin_int16x8_swizzle, OpCodeAsmJs::Simd128_Swizzle_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int,
                                                                              AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_shuffle]             = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 10, AsmJsSIMDBuiltin_int16x8_shuffle, OpCodeAsmJs::Simd128_Shuffle_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8,
                                                                               AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_splat]               = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_splat, OpCodeAsmJs::Simd128_Splat_I8, AsmJsRetType::Int16x8, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_replaceLane]         = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int16x8_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_and]                 = SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_and, OpCodeAsmJs::Simd128_And_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_or]                  = SIMDFunc(PropertyIds::or_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_or, OpCodeAsmJs::Simd128_Or_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_xor]                 = SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_xor, OpCodeAsmJs::Simd128_Xor_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_not]                 = SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_not, OpCodeAsmJs::Simd128_Not_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_add]                 = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_add, OpCodeAsmJs::Simd128_Add_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_sub]                 = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_sub, OpCodeAsmJs::Simd128_Sub_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_mul]                 = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_mul, OpCodeAsmJs::Simd128_Mul_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_neg]                 = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_neg, OpCodeAsmJs::Simd128_Neg_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_shiftLeftByScalar]   = SIMDFunc(PropertyIds::shiftLeftByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_shiftLeftByScalar, OpCodeAsmJs::Simd128_ShLtByScalar_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_shiftRightByScalar]  = SIMDFunc(PropertyIds::shiftRightByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_shiftRightByScalar, OpCodeAsmJs::Simd128_ShRtByScalar_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int));

        simdFunctions[AsmJsSIMDBuiltin_int16x8_lessThan]            = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_lessThan, OpCodeAsmJs::Simd128_Lt_I8, AsmJsRetType::Bool16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_lessThanOrEqual]     = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_I8, AsmJsRetType::Bool16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_equal]               = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_equal, OpCodeAsmJs::Simd128_Eq_I8, AsmJsRetType::Bool16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_notEqual]            = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_notEqual, OpCodeAsmJs::Simd128_Neq_I8, AsmJsRetType::Bool16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_greaterThan]         = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_greaterThan, OpCodeAsmJs::Simd128_Gt_I8, AsmJsRetType::Bool16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_greaterThanOrEqual]  = SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_I8, AsmJsRetType::Bool16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_select]              = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int16x8_select, OpCodeAsmJs::Simd128_Select_I8, AsmJsRetType::Int16x8, AsmJsType::Bool16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));

        simdFunctions[AsmJsSIMDBuiltin_int16x8_addSaturate]         = SIMDFunc(PropertyIds::addSaturate, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_addSaturate, OpCodeAsmJs::Simd128_AddSaturate_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_subSaturate]        = SIMDFunc(PropertyIds::subSaturate, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_subSaturate, OpCodeAsmJs::Simd128_SubSaturate_I8, AsmJsRetType::Int16x8, AsmJsType::Int16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_load]                = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int16x8_load, OpCodeAsmJs::Simd128_LdArr_I8, AsmJsRetType::Int16x8, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_store]               = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int16x8_store, OpCodeAsmJs::Simd128_StArr_I8, AsmJsRetType::Int16x8, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_fromFloat32x4Bits]   = SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_I8, AsmJsRetType::Int16x8, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_fromInt32x4Bits]     = SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_I8, AsmJsRetType::Int16x8, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_fromInt8x16Bits]     = SIMDFunc(PropertyIds::fromInt8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_fromInt8x16Bits, OpCodeAsmJs::Simd128_FromInt8x16Bits_I8, AsmJsRetType::Int16x8, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_fromUint32x4Bits]    = SIMDFunc(PropertyIds::fromUint32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_fromUint32x4Bits, OpCodeAsmJs::Simd128_FromUint32x4Bits_I8, AsmJsRetType::Int16x8, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_fromUint16x8Bits]    = SIMDFunc(PropertyIds::fromUint16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_fromUint16x8Bits, OpCodeAsmJs::Simd128_FromUint16x8Bits_I8, AsmJsRetType::Int16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_int16x8_fromUint8x16Bits]    = SIMDFunc(PropertyIds::fromUint8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int16x8_fromUint8x16Bits, OpCodeAsmJs::Simd128_FromUint8x16Bits_I8, AsmJsRetType::Int16x8, AsmJsType::Uint8x16));

        /* Int8x16 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Int8x16]                     = SIMDFunc(PropertyIds::Int8x16, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 16, AsmJsSIMDBuiltin_Int8x16, OpCodeAsmJs::Simd128_IntsToI16, AsmJsRetType::Int8x16, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_check]               = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_check, OpCodeAsmJs::Simd128_Ld_I16 /*no dynamic checks*/, AsmJsRetType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_splat]               = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_splat, OpCodeAsmJs::Simd128_Splat_I16, AsmJsRetType::Int8x16, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_neg]                 = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_neg, OpCodeAsmJs::Simd128_Neg_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_add]                 = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_add, OpCodeAsmJs::Simd128_Add_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_sub]                 = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_sub, OpCodeAsmJs::Simd128_Sub_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_mul]                 = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_mul, OpCodeAsmJs::Simd128_Mul_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));

        simdFunctions[AsmJsSIMDBuiltin_int8x16_and]                 = SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_and, OpCodeAsmJs::Simd128_And_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_or]                  = SIMDFunc(PropertyIds:: or_ , Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_or, OpCodeAsmJs::Simd128_Or_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_xor]                 = SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_xor, OpCodeAsmJs::Simd128_Xor_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_not]                 = SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_not, OpCodeAsmJs::Simd128_Not_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_shiftLeftByScalar]   = SIMDFunc(PropertyIds::shiftLeftByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_shiftLeftByScalar, OpCodeAsmJs::Simd128_ShLtByScalar_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_shiftRightByScalar]  = SIMDFunc(PropertyIds::shiftRightByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_shiftRightByScalar, OpCodeAsmJs::Simd128_ShRtByScalar_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int));

        simdFunctions[AsmJsSIMDBuiltin_int8x16_lessThan]            = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_lessThan, OpCodeAsmJs::Simd128_Lt_I16, AsmJsRetType::Bool8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_lessThanOrEqual]     = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_I16, AsmJsRetType::Bool8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_equal]               = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_equal, OpCodeAsmJs::Simd128_Eq_I16, AsmJsRetType::Bool8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_notEqual]            = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_notEqual, OpCodeAsmJs::Simd128_Neq_I16, AsmJsRetType::Bool8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_greaterThan]         = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_greaterThan, OpCodeAsmJs::Simd128_Gt_I16, AsmJsRetType::Bool8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_greaterThanOrEqual]  = SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_I16, AsmJsRetType::Bool8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_select]              = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int8x16_select, OpCodeAsmJs::Simd128_Select_I16, AsmJsRetType::Int8x16, AsmJsType::Bool8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));


        simdFunctions[AsmJsSIMDBuiltin_int8x16_addSaturate]         = SIMDFunc(PropertyIds::addSaturate, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_addSaturate, OpCodeAsmJs::Simd128_AddSaturate_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_subSaturate]        = SIMDFunc(PropertyIds::subSaturate, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_subSaturate, OpCodeAsmJs::Simd128_SubSaturate_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_load]                = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_load, OpCodeAsmJs::Simd128_LdArr_I16, AsmJsRetType::Int8x16, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_store]               = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int8x16_store, OpCodeAsmJs::Simd128_StArr_I16, AsmJsRetType::Int8x16, AsmJsType::Void, AsmJsType::Int, AsmJsType::Int8x16));

        simdFunctions[AsmJsSIMDBuiltin_int8x16_extractLane]         = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_int8x16_extractLane, OpCodeAsmJs::Simd128_ExtractLane_I16, AsmJsRetType::Signed, AsmJsType::Int8x16, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_replaceLane]         = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_int8x16_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_swizzle]             = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 17, AsmJsSIMDBuiltin_int8x16_swizzle, OpCodeAsmJs::Simd128_Swizzle_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16,
            AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int,
            AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_shuffle]             = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 18, AsmJsSIMDBuiltin_int8x16_shuffle, OpCodeAsmJs::Simd128_Shuffle_I16, AsmJsRetType::Int8x16, AsmJsType::Int8x16, AsmJsType::Int8x16,
            AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int,
            AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_fromFloat32x4Bits]   = SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_I16, AsmJsRetType::Int8x16, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_fromInt32x4Bits]     = SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_I16, AsmJsRetType::Int8x16, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_fromInt16x8Bits]     = SIMDFunc(PropertyIds::fromInt16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_fromInt16x8Bits, OpCodeAsmJs::Simd128_FromInt16x8Bits_I16, AsmJsRetType::Int8x16, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_fromUint32x4Bits]    = SIMDFunc(PropertyIds::fromUint32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_fromUint32x4Bits, OpCodeAsmJs::Simd128_FromUint32x4Bits_I16, AsmJsRetType::Int8x16, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_fromUint16x8Bits]    = SIMDFunc(PropertyIds::fromUint16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_fromUint16x8Bits, OpCodeAsmJs::Simd128_FromUint16x8Bits_I16, AsmJsRetType::Int8x16, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_int8x16_fromUint8x16Bits]    = SIMDFunc(PropertyIds::fromUint8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_int8x16_fromUint8x16Bits, OpCodeAsmJs::Simd128_FromUint8x16Bits_I16, AsmJsRetType::Int8x16, AsmJsType::Uint8x16));


        /* Uint32x4 */
        simdFunctions[AsmJsSIMDBuiltin_Uint32x4]                    = SIMDFunc(PropertyIds::Uint32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_Uint32x4, OpCodeAsmJs::Simd128_IntsToU4, AsmJsRetType::Uint32x4, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_check]               = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_check, OpCodeAsmJs::Simd128_Ld_U4 /*no dynamic checks*/, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_extractLane         ]= SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_extractLane, OpCodeAsmJs::Simd128_ExtractLane_U4, AsmJsRetType::Unsigned, AsmJsType::Uint32x4, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_swizzle             ]= SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 5, AsmJsSIMDBuiltin_uint32x4_swizzle, OpCodeAsmJs::Simd128_Swizzle_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_shuffle             ]= SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 6, AsmJsSIMDBuiltin_uint32x4_shuffle, OpCodeAsmJs::Simd128_Shuffle_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_splat               ]= SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_splat, OpCodeAsmJs::Simd128_Splat_U4, AsmJsRetType::Uint32x4, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_replaceLane         ]= SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint32x4_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_and                 ]= SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_and, OpCodeAsmJs::Simd128_And_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_or                  ]= SIMDFunc(PropertyIds::or_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_or, OpCodeAsmJs::Simd128_Or_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_xor                 ]= SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_xor, OpCodeAsmJs::Simd128_Xor_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_not                 ]= SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_not, OpCodeAsmJs::Simd128_Not_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_neg                 ]= SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_neg, OpCodeAsmJs::Simd128_Neg_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_add                 ]= SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_add, OpCodeAsmJs::Simd128_Add_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_sub                 ]= SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_sub, OpCodeAsmJs::Simd128_Sub_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_mul                 ]= SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_mul, OpCodeAsmJs::Simd128_Mul_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_shiftLeftByScalar   ]= SIMDFunc(PropertyIds::shiftLeftByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_shiftLeftByScalar, OpCodeAsmJs::Simd128_ShLtByScalar_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_shiftRightByScalar  ]= SIMDFunc(PropertyIds::shiftRightByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_shiftRightByScalar, OpCodeAsmJs::Simd128_ShRtByScalar_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint32x4, AsmJsType::Int));

        simdFunctions[AsmJsSIMDBuiltin_uint32x4_lessThan]            = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_lessThan, OpCodeAsmJs::Simd128_Lt_U4, AsmJsRetType::Bool32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_lessThanOrEqual]     = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_U4, AsmJsRetType::Bool32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_equal]               = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_equal, OpCodeAsmJs::Simd128_Eq_U4, AsmJsRetType::Bool32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_notEqual]            = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_notEqual, OpCodeAsmJs::Simd128_Neq_U4, AsmJsRetType::Bool32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_greaterThan]         = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_greaterThan, OpCodeAsmJs::Simd128_Gt_U4, AsmJsRetType::Bool32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_greaterThanOrEqual]  = SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_U4, AsmJsRetType::Bool32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_select]              = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint32x4_select, OpCodeAsmJs::Simd128_Select_U4, AsmJsRetType::Uint32x4, AsmJsType::Bool32x4, AsmJsType::Uint32x4, AsmJsType::Uint32x4));

        simdFunctions[AsmJsSIMDBuiltin_uint32x4_load                ]= SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_load, OpCodeAsmJs::Simd128_LdArr_U4, AsmJsRetType::Uint32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_load1               ]= SIMDFunc(PropertyIds::load1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_load1, OpCodeAsmJs::Simd128_LdArr_U4, AsmJsRetType::Uint32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_load2               ]= SIMDFunc(PropertyIds::load2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_load2, OpCodeAsmJs::Simd128_LdArr_U4, AsmJsRetType::Uint32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_load3               ]= SIMDFunc(PropertyIds::load3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint32x4_load3, OpCodeAsmJs::Simd128_LdArr_U4, AsmJsRetType::Uint32x4, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_store               ]= SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint32x4_store, OpCodeAsmJs::Simd128_StArr_U4, AsmJsRetType::Uint32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_store1              ]= SIMDFunc(PropertyIds::store1, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint32x4_store1, OpCodeAsmJs::Simd128_StArr_U4, AsmJsRetType::Uint32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_store2              ]= SIMDFunc(PropertyIds::store2, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint32x4_store2, OpCodeAsmJs::Simd128_StArr_U4, AsmJsRetType::Uint32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_store3              ]= SIMDFunc(PropertyIds::store3, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint32x4_store3, OpCodeAsmJs::Simd128_StArr_U4, AsmJsRetType::Uint32x4, AsmJsType::Void, AsmJsType::Int, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_fromFloat32x4       ]= SIMDFunc(PropertyIds::fromFloat32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_fromFloat32x4, OpCodeAsmJs::Simd128_FromFloat32x4_U4, AsmJsRetType::Uint32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_fromFloat32x4Bits   ]= SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_U4, AsmJsRetType::Uint32x4, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_fromInt32x4Bits     ]= SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_U4, AsmJsRetType::Uint32x4, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_fromInt16x8Bits     ]= SIMDFunc(PropertyIds::fromInt16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_fromInt16x8Bits, OpCodeAsmJs::Simd128_FromInt16x8Bits_U4, AsmJsRetType::Uint32x4, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_fromInt8x16Bits     ]= SIMDFunc(PropertyIds::fromInt8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_fromInt8x16Bits, OpCodeAsmJs::Simd128_FromInt8x16Bits_U4, AsmJsRetType::Uint32x4, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_fromUint16x8Bits    ]= SIMDFunc(PropertyIds::fromUint16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_fromUint16x8Bits, OpCodeAsmJs::Simd128_FromUint16x8Bits_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint32x4_fromUint8x16Bits    ]= SIMDFunc(PropertyIds::fromUint8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint32x4_fromUint8x16Bits, OpCodeAsmJs::Simd128_FromUint8x16Bits_U4, AsmJsRetType::Uint32x4, AsmJsType::Uint8x16));

        /* Uint16x8 */
        simdFunctions[AsmJsSIMDBuiltin_Uint16x8]                    = SIMDFunc(PropertyIds::Uint16x8, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 8, AsmJsSIMDBuiltin_Uint16x8, OpCodeAsmJs::Simd128_IntsToU8, AsmJsRetType::Uint16x8,
                                                                               AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_check]               = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_check, OpCodeAsmJs::Simd128_Ld_U8 /*no dynamic checks*/, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_extractLane         ]= SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_extractLane, OpCodeAsmJs::Simd128_ExtractLane_U8, AsmJsRetType::Unsigned, AsmJsType::Uint16x8, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_swizzle             ]= SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 9, AsmJsSIMDBuiltin_uint16x8_swizzle, OpCodeAsmJs::Simd128_Swizzle_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8,
                                                                                AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_shuffle             ]= SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 10, AsmJsSIMDBuiltin_uint16x8_shuffle, OpCodeAsmJs::Simd128_Shuffle_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8,
                                                                                AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_splat               ]= SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_splat, OpCodeAsmJs::Simd128_Splat_U8, AsmJsRetType::Uint16x8, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_replaceLane         ]= SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint16x8_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_and                 ]= SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_and, OpCodeAsmJs::Simd128_And_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_or                  ]= SIMDFunc(PropertyIds::or_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_or,   OpCodeAsmJs::Simd128_Or_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_xor                 ]= SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_xor, OpCodeAsmJs::Simd128_Xor_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_not                 ]= SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_not, OpCodeAsmJs::Simd128_Not_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_neg                 ]= SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_neg, OpCodeAsmJs::Simd128_Neg_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_add                 ]= SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_add, OpCodeAsmJs::Simd128_Add_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_sub                 ]= SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_sub, OpCodeAsmJs::Simd128_Sub_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_mul                 ]= SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_mul, OpCodeAsmJs::Simd128_Mul_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_shiftLeftByScalar   ]= SIMDFunc(PropertyIds::shiftLeftByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_shiftLeftByScalar, OpCodeAsmJs::Simd128_ShLtByScalar_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_shiftRightByScalar  ]= SIMDFunc(PropertyIds::shiftRightByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_shiftRightByScalar, OpCodeAsmJs::Simd128_ShRtByScalar_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Int));

        simdFunctions[AsmJsSIMDBuiltin_uint16x8_lessThan]            = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_lessThan, OpCodeAsmJs::Simd128_Lt_U8, AsmJsRetType::Bool16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_lessThanOrEqual]     = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_U8, AsmJsRetType::Bool16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_equal]               = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_equal, OpCodeAsmJs::Simd128_Eq_U8, AsmJsRetType::Bool16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_notEqual]            = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_notEqual, OpCodeAsmJs::Simd128_Neq_U8, AsmJsRetType::Bool16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_greaterThan]         = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_greaterThan, OpCodeAsmJs::Simd128_Gt_U8, AsmJsRetType::Bool16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_greaterThanOrEqual]  = SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_U8, AsmJsRetType::Bool16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_select]              = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint16x8_select, OpCodeAsmJs::Simd128_Select_U8, AsmJsRetType::Uint16x8, AsmJsType::Bool16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));

        simdFunctions[AsmJsSIMDBuiltin_uint16x8_addSaturate         ]= SIMDFunc(PropertyIds::addSaturate, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_addSaturate, OpCodeAsmJs::Simd128_AddSaturate_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_subSaturate         ]= SIMDFunc(PropertyIds::subSaturate, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_subSaturate, OpCodeAsmJs::Simd128_SubSaturate_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint16x8, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_load                ]= SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint16x8_load, OpCodeAsmJs::Simd128_LdArr_U8, AsmJsRetType::Uint16x8, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_store               ]= SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint16x8_store, OpCodeAsmJs::Simd128_StArr_U8, AsmJsRetType::Uint16x8, AsmJsType::Void, AsmJsType::Int, AsmJsType::Uint16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_fromFloat32x4Bits   ]= SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_U8, AsmJsRetType::Uint16x8, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_fromInt32x4Bits     ]= SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_U8, AsmJsRetType::Uint16x8, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_fromInt16x8Bits     ]= SIMDFunc(PropertyIds::fromInt16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_fromInt16x8Bits, OpCodeAsmJs::Simd128_FromInt16x8Bits_U8, AsmJsRetType::Uint16x8, AsmJsType::Int16x8));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_fromInt8x16Bits     ]= SIMDFunc(PropertyIds::fromInt8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_fromInt8x16Bits, OpCodeAsmJs::Simd128_FromInt8x16Bits_U8, AsmJsRetType::Uint16x8, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_fromUint32x4Bits    ]= SIMDFunc(PropertyIds::fromUint32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_fromUint32x4Bits, OpCodeAsmJs::Simd128_FromUint32x4Bits_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint16x8_fromUint8x16Bits    ]= SIMDFunc(PropertyIds::fromUint8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint16x8_fromUint8x16Bits, OpCodeAsmJs::Simd128_FromUint8x16Bits_U8, AsmJsRetType::Uint16x8, AsmJsType::Uint8x16));


        /* Uint8x16 */
        simdFunctions[AsmJsSIMDBuiltin_Uint8x16]                    = SIMDFunc(PropertyIds::Uint8x16, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 16, AsmJsSIMDBuiltin_Uint8x16, OpCodeAsmJs::Simd128_IntsToU16, AsmJsRetType::Uint8x16,
                                                                               AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish,
                                                                               AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_check]              = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_check, OpCodeAsmJs::Simd128_Ld_U16 /*no dynamic checks*/, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_extractLane       ] = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_extractLane, OpCodeAsmJs::Simd128_ExtractLane_U16, AsmJsRetType::Unsigned, AsmJsType::Uint8x16, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_swizzle           ] = SIMDFunc(PropertyIds::swizzle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 17, AsmJsSIMDBuiltin_uint8x16_swizzle, OpCodeAsmJs::Simd128_Swizzle_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16,
                                                                      AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int,
                                                                      AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_shuffle]            = SIMDFunc(PropertyIds::shuffle, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 18, AsmJsSIMDBuiltin_uint8x16_shuffle, OpCodeAsmJs::Simd128_Shuffle_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16,
                                                                      AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int,
                                                                      AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_splat             ] = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_splat, OpCodeAsmJs::Simd128_Splat_U16, AsmJsRetType::Uint8x16, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_replaceLane       ] = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint8x16_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_and               ] = SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_and, OpCodeAsmJs::Simd128_And_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_or                ] = SIMDFunc(PropertyIds::or_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_or,   OpCodeAsmJs::Simd128_Or_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_xor               ] = SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_xor, OpCodeAsmJs::Simd128_Xor_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_not               ] = SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_not, OpCodeAsmJs::Simd128_Not_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_neg               ] = SIMDFunc(PropertyIds::neg, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_neg, OpCodeAsmJs::Simd128_Neg_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_add               ] = SIMDFunc(PropertyIds::add, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_add, OpCodeAsmJs::Simd128_Add_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_sub               ] = SIMDFunc(PropertyIds::sub, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_sub, OpCodeAsmJs::Simd128_Sub_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_mul               ] = SIMDFunc(PropertyIds::mul, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_mul, OpCodeAsmJs::Simd128_Mul_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_shiftLeftByScalar ] = SIMDFunc(PropertyIds::shiftLeftByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_shiftLeftByScalar, OpCodeAsmJs::Simd128_ShLtByScalar_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_shiftRightByScalar] = SIMDFunc(PropertyIds::shiftRightByScalar, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_shiftRightByScalar, OpCodeAsmJs::Simd128_ShRtByScalar_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Int));

        simdFunctions[AsmJsSIMDBuiltin_uint8x16_lessThan]            = SIMDFunc(PropertyIds::lessThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_lessThan, OpCodeAsmJs::Simd128_Lt_U16, AsmJsRetType::Bool8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_lessThanOrEqual]     = SIMDFunc(PropertyIds::lessThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_lessThanOrEqual, OpCodeAsmJs::Simd128_LtEq_U16, AsmJsRetType::Bool8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_equal]               = SIMDFunc(PropertyIds::equal, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_equal, OpCodeAsmJs::Simd128_Eq_U16, AsmJsRetType::Bool8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_notEqual]            = SIMDFunc(PropertyIds::notEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_notEqual, OpCodeAsmJs::Simd128_Neq_U16, AsmJsRetType::Bool8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_greaterThan]         = SIMDFunc(PropertyIds::greaterThan, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_greaterThan, OpCodeAsmJs::Simd128_Gt_U16, AsmJsRetType::Bool8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_greaterThanOrEqual]  = SIMDFunc(PropertyIds::greaterThanOrEqual, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_greaterThanOrEqual, OpCodeAsmJs::Simd128_GtEq_U16, AsmJsRetType::Bool8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_select]              = SIMDFunc(PropertyIds::select, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint8x16_select, OpCodeAsmJs::Simd128_Select_U16, AsmJsRetType::Uint8x16, AsmJsType::Bool8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));


        simdFunctions[AsmJsSIMDBuiltin_uint8x16_addSaturate       ] = SIMDFunc(PropertyIds::addSaturate, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_addSaturate, OpCodeAsmJs::Simd128_AddSaturate_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_subSaturate       ] = SIMDFunc(PropertyIds::subSaturate, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_subSaturate, OpCodeAsmJs::Simd128_SubSaturate_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint8x16, AsmJsType::Uint8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_load              ] = SIMDFunc(PropertyIds::load, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_uint8x16_load, OpCodeAsmJs::Simd128_LdArr_U16, AsmJsRetType::Uint8x16, AsmJsType::Void, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_store             ] = SIMDFunc(PropertyIds::store, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_uint8x16_store, OpCodeAsmJs::Simd128_StArr_U16, AsmJsRetType::Uint8x16, AsmJsType::Void, AsmJsType::Int, AsmJsType::Uint8x16));

        simdFunctions[AsmJsSIMDBuiltin_uint8x16_fromFloat32x4Bits ] = SIMDFunc(PropertyIds::fromFloat32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_fromFloat32x4Bits, OpCodeAsmJs::Simd128_FromFloat32x4Bits_U16, AsmJsRetType::Uint8x16, AsmJsType::Float32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_fromInt32x4Bits   ] = SIMDFunc(PropertyIds::fromInt32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_fromInt32x4Bits, OpCodeAsmJs::Simd128_FromInt32x4Bits_U16, AsmJsRetType::Uint8x16, AsmJsType::Int32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_fromInt16x8Bits   ] = SIMDFunc(PropertyIds::fromInt16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_fromInt16x8Bits, OpCodeAsmJs::Simd128_FromInt16x8Bits_U16, AsmJsRetType::Uint8x16, AsmJsType::Int16x8));
         simdFunctions[AsmJsSIMDBuiltin_uint8x16_fromInt8x16Bits   ] = SIMDFunc(PropertyIds::fromInt8x16Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_fromInt8x16Bits, OpCodeAsmJs::Simd128_FromInt8x16Bits_U16, AsmJsRetType::Uint8x16, AsmJsType::Int8x16));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_fromUint32x4Bits  ] = SIMDFunc(PropertyIds::fromUint32x4Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_fromUint32x4Bits, OpCodeAsmJs::Simd128_FromUint32x4Bits_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint32x4));
        simdFunctions[AsmJsSIMDBuiltin_uint8x16_fromUint16x8Bits  ] = SIMDFunc(PropertyIds::fromUint16x8Bits, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_uint8x16_fromUint16x8Bits, OpCodeAsmJs::Simd128_FromUint16x8Bits_U16, AsmJsRetType::Uint8x16, AsmJsType::Uint16x8));

        /* Bool32x4 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Bool32x4]                    = SIMDFunc(PropertyIds::Bool32x4, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 4, AsmJsSIMDBuiltin_Bool32x4, OpCodeAsmJs::Simd128_IntsToB4, AsmJsRetType::Bool32x4, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_check]              = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool32x4_check, OpCodeAsmJs::Simd128_Ld_B4 /*no dynamic checks*/, AsmJsRetType::Bool32x4, AsmJsType::Bool32x4));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_splat]              = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool32x4_splat, OpCodeAsmJs::Simd128_Splat_B4, AsmJsRetType::Bool32x4, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_extractLane]        = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool32x4_extractLane, OpCodeAsmJs::Simd128_ExtractLane_B4, AsmJsRetType::Signed, AsmJsType::Bool32x4, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_replaceLane]        = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_bool32x4_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_B4, AsmJsRetType::Bool32x4, AsmJsType::Bool32x4, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_and]                = SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool32x4_and, OpCodeAsmJs::Simd128_And_B4, AsmJsRetType::Bool32x4, AsmJsType::Bool32x4, AsmJsType::Bool32x4));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_or]                 = SIMDFunc(PropertyIds::or_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool32x4_or, OpCodeAsmJs::Simd128_Or_B4, AsmJsRetType::Bool32x4, AsmJsType::Bool32x4, AsmJsType::Bool32x4));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_xor]                = SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool32x4_xor, OpCodeAsmJs::Simd128_Xor_B4, AsmJsRetType::Bool32x4, AsmJsType::Bool32x4, AsmJsType::Bool32x4));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_not]                = SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool32x4_not, OpCodeAsmJs::Simd128_Not_B4, AsmJsRetType::Bool32x4, AsmJsType::Bool32x4));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_anyTrue]            = SIMDFunc(PropertyIds::anyTrue, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool32x4_anyTrue, OpCodeAsmJs::Simd128_AnyTrue_B4, AsmJsRetType::Signed, AsmJsType::Bool32x4));
        simdFunctions[AsmJsSIMDBuiltin_bool32x4_allTrue]            = SIMDFunc(PropertyIds::allTrue, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool32x4_allTrue, OpCodeAsmJs::Simd128_AllTrue_B4, AsmJsRetType::Signed, AsmJsType::Bool32x4));

        /* Bool16x8 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Bool16x8]                    = SIMDFunc(PropertyIds::Bool16x8, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 8, AsmJsSIMDBuiltin_Bool16x8, OpCodeAsmJs::Simd128_IntsToB8, AsmJsRetType::Bool16x8, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_check]              = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool16x8_check, OpCodeAsmJs::Simd128_Ld_B8 /*no dynamic checks*/, AsmJsRetType::Bool16x8, AsmJsType::Bool16x8));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_splat]              = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool16x8_splat, OpCodeAsmJs::Simd128_Splat_B8, AsmJsRetType::Bool16x8, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_extractLane]        = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool16x8_extractLane, OpCodeAsmJs::Simd128_ExtractLane_B8, AsmJsRetType::Signed, AsmJsType::Bool16x8, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_replaceLane]        = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_bool16x8_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_B8, AsmJsRetType::Bool16x8, AsmJsType::Bool16x8, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_and]                = SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool16x8_and, OpCodeAsmJs::Simd128_And_B8, AsmJsRetType::Bool16x8, AsmJsType::Bool16x8, AsmJsType::Bool16x8));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_or]                 = SIMDFunc(PropertyIds::or_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool16x8_or, OpCodeAsmJs::Simd128_Or_B8, AsmJsRetType::Bool16x8, AsmJsType::Bool16x8, AsmJsType::Bool16x8));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_xor]                = SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool16x8_xor, OpCodeAsmJs::Simd128_Xor_B8, AsmJsRetType::Bool16x8, AsmJsType::Bool16x8, AsmJsType::Bool16x8));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_not]                = SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool16x8_not, OpCodeAsmJs::Simd128_Not_B8, AsmJsRetType::Bool16x8, AsmJsType::Bool16x8));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_anyTrue]            = SIMDFunc(PropertyIds::anyTrue, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool16x8_anyTrue, OpCodeAsmJs::Simd128_AnyTrue_B8, AsmJsRetType::Signed, AsmJsType::Bool16x8));
        simdFunctions[AsmJsSIMDBuiltin_bool16x8_allTrue]            = SIMDFunc(PropertyIds::allTrue, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool16x8_allTrue, OpCodeAsmJs::Simd128_AllTrue_B8, AsmJsRetType::Signed, AsmJsType::Bool16x8));

        /* Bool8x16 builtins*/
        //-------------------
        simdFunctions[AsmJsSIMDBuiltin_Bool8x16]                    = SIMDFunc(PropertyIds::Bool8x16, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 16, AsmJsSIMDBuiltin_Bool8x16, OpCodeAsmJs::Simd128_IntsToB16, AsmJsRetType::Bool8x16, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_check]              = SIMDFunc(PropertyIds::check, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool8x16_check, OpCodeAsmJs::Simd128_Ld_B16 /*no dynamic checks*/, AsmJsRetType::Bool8x16, AsmJsType::Bool8x16));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_splat]              = SIMDFunc(PropertyIds::splat, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool8x16_splat, OpCodeAsmJs::Simd128_Splat_B16, AsmJsRetType::Bool8x16, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_extractLane]        = SIMDFunc(PropertyIds::extractLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool8x16_extractLane, OpCodeAsmJs::Simd128_ExtractLane_B16, AsmJsRetType::Signed, AsmJsType::Bool8x16, AsmJsType::Int));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_replaceLane]        = SIMDFunc(PropertyIds::replaceLane, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 3, AsmJsSIMDBuiltin_bool8x16_replaceLane, OpCodeAsmJs::Simd128_ReplaceLane_B16, AsmJsRetType::Bool8x16, AsmJsType::Bool8x16, AsmJsType::Int, AsmJsType::Intish));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_and]                = SIMDFunc(PropertyIds::and_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool8x16_and, OpCodeAsmJs::Simd128_And_B16, AsmJsRetType::Bool8x16, AsmJsType::Bool8x16, AsmJsType::Bool8x16));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_or]                 = SIMDFunc(PropertyIds::or_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool8x16_or, OpCodeAsmJs::Simd128_Or_B16, AsmJsRetType::Bool8x16, AsmJsType::Bool8x16, AsmJsType::Bool8x16));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_xor]                = SIMDFunc(PropertyIds::xor_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 2, AsmJsSIMDBuiltin_bool8x16_xor, OpCodeAsmJs::Simd128_Xor_B16, AsmJsRetType::Bool8x16, AsmJsType::Bool8x16, AsmJsType::Bool8x16));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_not]                = SIMDFunc(PropertyIds::not_, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool8x16_not, OpCodeAsmJs::Simd128_Not_B16, AsmJsRetType::Bool8x16, AsmJsType::Bool8x16));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_anyTrue]            = SIMDFunc(PropertyIds::anyTrue, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool8x16_anyTrue, OpCodeAsmJs::Simd128_AnyTrue_B16, AsmJsRetType::Signed, AsmJsType::Bool8x16));
        simdFunctions[AsmJsSIMDBuiltin_bool8x16_allTrue]            = SIMDFunc(PropertyIds::allTrue, Anew(&mAllocator, AsmJsSIMDFunction, nullptr, &mAllocator, 1, AsmJsSIMDBuiltin_bool8x16_allTrue, OpCodeAsmJs::Simd128_AllTrue_B16, AsmJsRetType::Signed, AsmJsType::Bool8x16));


        {LOGMEIN("AsmJsModule.cpp] 1880\n");
            SIMDNameMap *map = &mStdLibSIMDInt32x4Map;
            for (int i = 0; i < AsmJsSIMDBuiltin_COUNT; i++)
            {LOGMEIN("AsmJsModule.cpp] 1883\n");
                switch (i)
                {LOGMEIN("AsmJsModule.cpp] 1885\n");
                case AsmJsSIMDBuiltin_Float32x4:
                    map = &mStdLibSIMDFloat32x4Map;
                    break;
                case AsmJsSIMDBuiltin_Float64x2:
                    map = &mStdLibSIMDFloat64x2Map;
                    break;
                case AsmJsSIMDBuiltin_Int16x8:
                    map = &mStdLibSIMDInt16x8Map;
                    break;
                case AsmJsSIMDBuiltin_Int8x16:
                    map = &mStdLibSIMDInt8x16Map;
                    break;
                case AsmJsSIMDBuiltin_Uint32x4:
                    map = &mStdLibSIMDUint32x4Map;
                    break;
                case AsmJsSIMDBuiltin_Uint16x8:
                    map = &mStdLibSIMDUint16x8Map;
                    break;
                case AsmJsSIMDBuiltin_Uint8x16:
                    map = &mStdLibSIMDUint8x16Map;
                    break;
                case AsmJsSIMDBuiltin_Bool32x4:
                    map = &mStdLibSIMDBool32x4Map;
                    break;
                case AsmJsSIMDBuiltin_Bool16x8:
                    map = &mStdLibSIMDBool16x8Map;
                    break;
                case AsmJsSIMDBuiltin_Bool8x16:
                    map = &mStdLibSIMDBool8x16Map;
                    break;
                }

                if (simdFunctions[i].id && simdFunctions[i].val)
                {LOGMEIN("AsmJsModule.cpp] 1919\n");
                    if (!AddStandardLibrarySIMDNameInMap(simdFunctions[i].id, simdFunctions[i].val, map))
                    {LOGMEIN("AsmJsModule.cpp] 1921\n");
                        AsmJSCompiler::OutputError(GetScriptContext(), _u("Cannot initialize SIMD library"));
                        return false;
                    }
                }
            }
        }
        return true;
    }

    AsmJsModuleCompiler::AsmJsModuleCompiler( ExclusiveContext *cx, AsmJSParser &parser ) :
        mCx( cx )
        , mCurrentParserNode( parser )
        , mAllocator( _u("Asmjs"), cx->scriptContext->GetThreadContext()->GetPageAllocator(), Throw::OutOfMemory )
        , mModuleFunctionName( nullptr )
        , mStandardLibraryMathNames(&mAllocator)
        , mStandardLibraryArrayNames(&mAllocator)
        , mFunctionArray( &mAllocator )
        , mModuleEnvironment( &mAllocator )
        , mFunctionTableArray( &mAllocator )
        , mInitialised(false)
        , mIntVarSpace( )
        , mDoubleVarSpace( )
        , mExports(&mAllocator)
        , mExportFuncIndex(Js::Constants::NoRegister)
        , mVarImportCount(0)
        , mVarCount(0)
        , mFuncPtrTableCount(0)
        , mCompileTime()
        , mCompileTimeLastTick(GetTick())
        , mMaxAstSize(0)
        , mArrayViews(&mAllocator)
        , mUsesChangeHeap(false)
        , mUsesHeapBuffer(false)
        , mMaxHeapAccess(0)
#if DBG
        , mStdLibArgNameInit(false)
        , mForeignArgNameInit(false)
        , mBufferArgNameInit(false)
#endif
        , mStdLibSIMDInt32x4Map(&mAllocator)
        , mStdLibSIMDInt16x8Map(&mAllocator)
        , mStdLibSIMDInt8x16Map(&mAllocator)
        , mStdLibSIMDUint32x4Map(&mAllocator)
        , mStdLibSIMDUint16x8Map(&mAllocator)
        , mStdLibSIMDUint8x16Map(&mAllocator)
        , mStdLibSIMDBool32x4Map(&mAllocator)
        , mStdLibSIMDBool16x8Map(&mAllocator)
        , mStdLibSIMDBool8x16Map(&mAllocator)
        , mStdLibSIMDFloat32x4Map(&mAllocator)
        , mStdLibSIMDFloat64x2Map(&mAllocator)

    {LOGMEIN("AsmJsModule.cpp] 1973\n");
        InitModuleNode( parser );
    }

    bool AsmJsModuleCompiler::AddStandardLibraryMathName( PropertyId id, const double* cstAddr, AsmJSMathBuiltinFunction mathLibFunctionName )
    {LOGMEIN("AsmJsModule.cpp] 1978\n");
        // make sure this name is unique
        if( mStandardLibraryMathNames.ContainsKey( id ) )
        {LOGMEIN("AsmJsModule.cpp] 1981\n");
            return false;
        }

        MathBuiltin mathBuiltin(mathLibFunctionName, cstAddr);
        int addResult = mStandardLibraryMathNames.AddNew( id, mathBuiltin );
        if( addResult == -1 )
        {LOGMEIN("AsmJsModule.cpp] 1988\n");
            // Error adding the function
            return false;
        }
        return true;
    }


    bool AsmJsModuleCompiler::AddStandardLibraryMathName(PropertyId id, AsmJsMathFunction* func, AsmJSMathBuiltinFunction mathLibFunctionName)
    {LOGMEIN("AsmJsModule.cpp] 1997\n");
        // make sure this name is unique
        if( mStandardLibraryMathNames.ContainsKey( id ) )
        {LOGMEIN("AsmJsModule.cpp] 2000\n");
            return false;
        }

        MathBuiltin mathBuiltin(mathLibFunctionName, func);
        int addResult = mStandardLibraryMathNames.AddNew( id, mathBuiltin );
        if( addResult == -1 )
        {LOGMEIN("AsmJsModule.cpp] 2007\n");
            // Error adding the function
            return false;
        }
        return true;
    }

    bool AsmJsModuleCompiler::AddStandardLibraryArrayName(PropertyId id, AsmJsTypedArrayFunction* func, AsmJSTypedArrayBuiltinFunction arrayLibFunctionName)
    {LOGMEIN("AsmJsModule.cpp] 2015\n");
        // make sure this name is unique
        if (mStandardLibraryArrayNames.ContainsKey(id))
        {LOGMEIN("AsmJsModule.cpp] 2018\n");
            return false;
        }

        TypedArrayBuiltin arrayBuiltin(arrayLibFunctionName, func);
        int addResult = mStandardLibraryArrayNames.AddNew(id, arrayBuiltin);
        if (addResult == -1)
        {LOGMEIN("AsmJsModule.cpp] 2025\n");
            // Error adding the function
            return false;
        }
        return true;
    }

    Parser * AsmJsModuleCompiler::GetParser() const
    {LOGMEIN("AsmJsModule.cpp] 2033\n");
        return mCx->byteCodeGenerator->GetParser();
    }

    ByteCodeGenerator* AsmJsModuleCompiler::GetByteCodeGenerator() const
    {LOGMEIN("AsmJsModule.cpp] 2038\n");
        return mCx->byteCodeGenerator;
    }

    ScriptContext * AsmJsModuleCompiler::GetScriptContext() const
    {LOGMEIN("AsmJsModule.cpp] 2043\n");
        return mCx->scriptContext;
    }

    AsmJsSymbol* AsmJsModuleCompiler::LookupIdentifier( PropertyName name, AsmJsFunc* func /*= nullptr */, AsmJsLookupSource::Source* lookupSource /*= nullptr*/ )
    {LOGMEIN("AsmJsModule.cpp] 2048\n");
        AsmJsSymbol* lookupResult = nullptr;
        if (name)
        {LOGMEIN("AsmJsModule.cpp] 2051\n");
            if (func)
            {LOGMEIN("AsmJsModule.cpp] 2053\n");
                lookupResult = func->LookupIdentifier(name, lookupSource);
                if (lookupResult)
                {LOGMEIN("AsmJsModule.cpp] 2056\n");
                    return lookupResult;
                }
            }

            lookupResult = mModuleEnvironment.LookupWithKey(name->GetPropertyId(), nullptr);
            if (lookupSource)
            {LOGMEIN("AsmJsModule.cpp] 2063\n");
                *lookupSource = AsmJsLookupSource::AsmJsModule;
            }
        }
        return lookupResult;
    }

    bool AsmJsModuleCompiler::DefineIdentifier( PropertyName name, AsmJsSymbol* symbol )
    {LOGMEIN("AsmJsModule.cpp] 2071\n");
        Assert( symbol );
        if( symbol )
        {LOGMEIN("AsmJsModule.cpp] 2074\n");
            // make sure this identifier is unique
            if(!LookupIdentifier( name ))
            {LOGMEIN("AsmJsModule.cpp] 2077\n");
                int addResult = mModuleEnvironment.AddNew(name->GetPropertyId(), symbol);
                return addResult != -1;
            }
        }
        return false;
    }

    bool AsmJsModuleCompiler::AddNumericVar( PropertyName name, ParseNode* pnode, bool isFloat, bool isMutable /*= true*/ )
    {LOGMEIN("AsmJsModule.cpp] 2086\n");
        Assert(ParserWrapper::IsNumericLiteral(pnode) || (isFloat && ParserWrapper::IsFroundNumericLiteral(pnode)));
        AsmJsVar* var = Anew( &mAllocator, AsmJsVar, name, isMutable );
        if( !var )
        {LOGMEIN("AsmJsModule.cpp] 2090\n");
            return false;
        }
        if( !DefineIdentifier( name, var ) )
        {LOGMEIN("AsmJsModule.cpp] 2094\n");
            return false;
        }

        ++mVarCount;

        if (isFloat)
        {LOGMEIN("AsmJsModule.cpp] 2101\n");
            var->SetVarType(AsmJsVarType::Float);
            var->SetLocation(mFloatVarSpace.AcquireRegister());
            if (pnode->nop == knopInt)
            {LOGMEIN("AsmJsModule.cpp] 2105\n");
                var->SetConstInitialiser((float)pnode->sxInt.lw);
            }
            else if (ParserWrapper::IsNegativeZero(pnode))
            {LOGMEIN("AsmJsModule.cpp] 2109\n");
                var->SetConstInitialiser(-0.0f);
            }
            else
            {
                var->SetConstInitialiser((float)pnode->sxFlt.dbl);
            }
        }
        else if (pnode->nop == knopInt)
        {LOGMEIN("AsmJsModule.cpp] 2118\n");
            var->SetVarType(AsmJsVarType::Int);
            var->SetLocation(mIntVarSpace.AcquireRegister());
            var->SetConstInitialiser(pnode->sxInt.lw);
        }
        else
        {
            if (ParserWrapper::IsMinInt(pnode))
            {LOGMEIN("AsmJsModule.cpp] 2126\n");
                var->SetVarType(AsmJsVarType::Int);
                var->SetLocation(mIntVarSpace.AcquireRegister());
                var->SetConstInitialiser(INT_MIN);
            }
            else if (ParserWrapper::IsUnsigned(pnode))
            {LOGMEIN("AsmJsModule.cpp] 2132\n");
                var->SetVarType(AsmJsVarType::Int);
                var->SetLocation(mIntVarSpace.AcquireRegister());
                var->SetConstInitialiser((int)((uint32)pnode->sxFlt.dbl));
            }
            else if (pnode->sxFlt.maybeInt)
            {LOGMEIN("AsmJsModule.cpp] 2138\n");
                // this means there was an int literal not in range [-2^31,3^32)
                return false;
            }
            else
            {
                var->SetVarType(AsmJsVarType::Double);
                var->SetLocation(mDoubleVarSpace.AcquireRegister());
                var->SetConstInitialiser(pnode->sxFlt.dbl);
            }
        }
        return true;
    }

    bool AsmJsModuleCompiler::AddGlobalVarImport( PropertyName name, PropertyName field, AsmJSCoercion coercion )
    {LOGMEIN("AsmJsModule.cpp] 2153\n");
        AsmJsConstantImport* var = Anew( &mAllocator, AsmJsConstantImport, name, field );
        if( !var )
        {LOGMEIN("AsmJsModule.cpp] 2156\n");
            return false;
        }
        if( !DefineIdentifier( name, var ) )
        {LOGMEIN("AsmJsModule.cpp] 2160\n");
            return false;
        }
        ++mVarImportCount;

        switch( coercion )
        {LOGMEIN("AsmJsModule.cpp] 2166\n");
        case Js::AsmJS_ToInt32:
            var->SetVarType( AsmJsVarType::Int );
            var->SetLocation( mIntVarSpace.AcquireRegister() );
            break;
        case Js::AsmJS_ToNumber:
            var->SetVarType( AsmJsVarType::Double );
            var->SetLocation( mDoubleVarSpace.AcquireRegister() );
            break;
        case Js::AsmJS_FRound:
            var->SetVarType( AsmJsVarType::Float );
            var->SetLocation(mFloatVarSpace.AcquireRegister());
            break;
        case Js::AsmJS_Int32x4:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2181\n");
                var->SetVarType(AsmJsVarType::Int32x4);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case Js::AsmJS_Bool32x4:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2189\n");
                var->SetVarType(AsmJsVarType::Bool32x4);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case Js::AsmJS_Bool16x8:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2197\n");
                var->SetVarType(AsmJsVarType::Bool16x8);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case Js::AsmJS_Bool8x16:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2205\n");
                var->SetVarType(AsmJsVarType::Bool8x16);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case AsmJS_Float32x4:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2213\n");
                var->SetVarType(AsmJsVarType::Float32x4);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case AsmJS_Float64x2:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2221\n");
                var->SetVarType(AsmJsVarType::Float64x2);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case Js::AsmJS_Int16x8:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2229\n");
                var->SetVarType(AsmJsVarType::Int16x8);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case Js::AsmJS_Int8x16:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2237\n");
                var->SetVarType(AsmJsVarType::Int8x16);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case Js::AsmJS_Uint32x4:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2245\n");
                var->SetVarType(AsmJsVarType::Uint32x4);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case Js::AsmJS_Uint16x8:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2253\n");
                var->SetVarType(AsmJsVarType::Uint16x8);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        case Js::AsmJS_Uint8x16:
            if (IsSimdjsEnabled())
            {LOGMEIN("AsmJsModule.cpp] 2261\n");
                var->SetVarType(AsmJsVarType::Uint8x16);
                var->SetLocation(mSimdVarSpace.AcquireRegister());
                break;
            }
            Assert(UNREACHED);
        default:
            break;
        }

        return true;
    }

    bool AsmJsModuleCompiler::AddModuleFunctionImport( PropertyName name, PropertyName field )
    {LOGMEIN("AsmJsModule.cpp] 2275\n");
        AsmJsImportFunction* var = Anew( &mAllocator, AsmJsImportFunction, name, field, &mAllocator );
        if( !var )
        {LOGMEIN("AsmJsModule.cpp] 2278\n");
            return false;
        }
        if( !DefineIdentifier( name, var ) )
        {LOGMEIN("AsmJsModule.cpp] 2282\n");
            return false;
        }
        var->SetFunctionIndex( mImportFunctions.AcquireRegister() );

        return true;
    }

    bool AsmJsModuleCompiler::AddNumericConst( PropertyName name, const double* cst )
    {LOGMEIN("AsmJsModule.cpp] 2291\n");
        AsmJsMathConst* var = Anew( &mAllocator, AsmJsMathConst, name, cst );
        if( !var )
        {LOGMEIN("AsmJsModule.cpp] 2294\n");
            return false;
        }
        if( !DefineIdentifier( name, var ) )
        {LOGMEIN("AsmJsModule.cpp] 2298\n");
            return false;
        }

        return true;
    }

    bool AsmJsModuleCompiler::AddArrayView( PropertyName name, ArrayBufferView::ViewType type )
    {LOGMEIN("AsmJsModule.cpp] 2306\n");
        AsmJsArrayView* view = Anew( &mAllocator, AsmJsArrayView, name, type );
        if( !view )
        {LOGMEIN("AsmJsModule.cpp] 2309\n");
            return false;
        }
        if( !DefineIdentifier( name, view ) )
        {LOGMEIN("AsmJsModule.cpp] 2313\n");
            return false;
        }
        mArrayViews.Enqueue(view);

        return true;
    }

    bool AsmJsModuleCompiler::AddFunctionTable( PropertyName name, const int size )
    {LOGMEIN("AsmJsModule.cpp] 2322\n");
        GetByteCodeGenerator()->AssignPropertyId(name);
        AsmJsFunctionTable* funcTable = Anew( &mAllocator, AsmJsFunctionTable, name, &mAllocator );
        if( !funcTable )
        {LOGMEIN("AsmJsModule.cpp] 2326\n");
            return false;
        }
        if( !DefineIdentifier( name, funcTable ) )
        {LOGMEIN("AsmJsModule.cpp] 2330\n");
            return false;
        }
        funcTable->SetSize( size );
        int pos = mFunctionTableArray.Add( funcTable );
        funcTable->SetFunctionIndex( pos );

        return true;
    }

    bool AsmJsModuleCompiler::AddExport( PropertyName name, RegSlot location )
    {LOGMEIN("AsmJsModule.cpp] 2341\n");
        AsmJsModuleExport ex;
        ex.id = name->GetPropertyId();
        ex.location = location;

        // return is < 0 if count overflowed 31bits
        return mExports.Add( ex ) >= 0;
    }

    bool AsmJsModuleCompiler::SetExportFunc( AsmJsFunc* func )
    {LOGMEIN("AsmJsModule.cpp] 2351\n");
        Assert( mExports.Count() == 0 && func);
        mExportFuncIndex = func->GetFunctionIndex();
        return mExports.Count() == 0 && (uint32)mExportFuncIndex < (uint32)mFunctionArray.Count();
    }

    AsmJsFunctionDeclaration* AsmJsModuleCompiler::LookupFunction( PropertyName name )
    {LOGMEIN("AsmJsModule.cpp] 2358\n");
        if (name)
        {LOGMEIN("AsmJsModule.cpp] 2360\n");
            AsmJsSymbol* sym = LookupIdentifier(name);
            if (sym)
            {LOGMEIN("AsmJsModule.cpp] 2363\n");
                switch (sym->GetSymbolType())
                {LOGMEIN("AsmJsModule.cpp] 2365\n");
                case AsmJsSymbol::SIMDBuiltinFunction:
                case AsmJsSymbol::MathBuiltinFunction:
                case AsmJsSymbol::ModuleFunction:
                case AsmJsSymbol::ImportFunction:
                case AsmJsSymbol::FuncPtrTable:
                    return sym->Cast<AsmJsFunctionDeclaration>();
                default:
                    break;
                }
            }
        }
        return nullptr;
    }

    bool AsmJsModuleCompiler::AreAllFuncTableDefined()
    {LOGMEIN("AsmJsModule.cpp] 2381\n");
        const int size = mFunctionTableArray.Count();
        for (int i = 0; i < size ; i++)
        {LOGMEIN("AsmJsModule.cpp] 2384\n");
            AsmJsFunctionTable* funcTable = mFunctionTableArray.Item( i );
            if( !funcTable->IsDefined() )
            {LOGMEIN("AsmJsModule.cpp] 2387\n");
                AsmJSCompiler::OutputError(GetScriptContext(), _u("Function table %s was used in a function but does not appear in the module"), funcTable->GetName()->Psz());
                return false;
            }
        }
        return true;
    }

    void AsmJsModuleCompiler::UpdateMaxHeapAccess(uint index)
    {LOGMEIN("AsmJsModule.cpp] 2396\n");
        if (mMaxHeapAccess < index)
        {LOGMEIN("AsmJsModule.cpp] 2398\n");
            mMaxHeapAccess = index;
        }
    }

    void AsmJsModuleCompiler::InitMemoryOffsets()
    {LOGMEIN("AsmJsModule.cpp] 2404\n");
        mModuleMemory.mArrayBufferOffset = AsmJsModuleMemory::MemoryTableBeginOffset;
        mModuleMemory.mStdLibOffset = mModuleMemory.mArrayBufferOffset + 1;
        mModuleMemory.mDoubleOffset = mModuleMemory.mStdLibOffset + 1;
        mModuleMemory.mFuncOffset = mModuleMemory.mDoubleOffset + (mDoubleVarSpace.GetTotalVarCount() * WAsmJs::DOUBLE_SLOTS_SPACE);
        mModuleMemory.mFFIOffset = mModuleMemory.mFuncOffset + mFunctionArray.Count();
        mModuleMemory.mFuncPtrOffset = mModuleMemory.mFFIOffset + mImportFunctions.GetTotalVarCount();
        mModuleMemory.mFloatOffset = mModuleMemory.mFuncPtrOffset + GetFuncPtrTableCount();
        mModuleMemory.mIntOffset = mModuleMemory.mFloatOffset + (int32)(mFloatVarSpace.GetTotalVarCount() * WAsmJs::FLOAT_SLOTS_SPACE + 0.5);
        mModuleMemory.mMemorySize    = mModuleMemory.mIntOffset + (int32)(mIntVarSpace.GetTotalVarCount() * WAsmJs::INT_SLOTS_SPACE + 0.5);

        if (IsSimdjsEnabled())
        {LOGMEIN("AsmJsModule.cpp] 2416\n");
            // mSimdOffset is in SIMDValues, hence aligned
            // mMemorySize is in Vars
            mModuleMemory.mSimdOffset = (int) ::ceil(mModuleMemory.mMemorySize / WAsmJs::SIMD_SLOTS_SPACE);
            if (mSimdVarSpace.GetTotalVarCount())
            {LOGMEIN("AsmJsModule.cpp] 2421\n");
                mModuleMemory.mMemorySize = (int)((mModuleMemory.mSimdOffset + mSimdVarSpace.GetTotalVarCount()) * WAsmJs::SIMD_SLOTS_SPACE);
            }

        }
    }

    void AsmJsModuleCompiler::AccumulateCompileTime()
    {LOGMEIN("AsmJsModule.cpp] 2429\n");
        Js::TickDelta td;
        AsmJsCompileTime curTime = GetTick();
        td = curTime - mCompileTimeLastTick;
        mCompileTime = mCompileTime+td;
        mCompileTimeLastTick = curTime;
    }

    void AsmJsModuleCompiler::AccumulateCompileTime(AsmJsCompilation::Phases phase)
    {LOGMEIN("AsmJsModule.cpp] 2438\n");
        Js::TickDelta td;
        AsmJsCompileTime curTime = GetTick();
        td = curTime - mCompileTimeLastTick;
        mCompileTime = mCompileTime+td;
        mCompileTimeLastTick = curTime;
        mPhaseCompileTime[phase] = mPhaseCompileTime[phase] + td;
    }

    Js::AsmJsCompileTime AsmJsModuleCompiler::GetTick()
    {LOGMEIN("AsmJsModule.cpp] 2448\n");
        return Js::Tick::Now();
    }

    uint64 AsmJsModuleCompiler::GetCompileTime() const
{LOGMEIN("AsmJsModule.cpp] 2453\n");
        return mCompileTime.ToMicroseconds();
    }

    static const char16* AsmPhaseNames[AsmJsCompilation::Phases_COUNT] = {
        _u("Module"),
        _u("ByteCode"),
        _u("TemplateJIT"),
    };

    void AsmJsModuleCompiler::PrintCompileTrace() const
    {LOGMEIN("AsmJsModule.cpp] 2464\n");
        // for testtrace, don't print time so that it can be used for baselines
        if (PHASE_TESTTRACE1(AsmjsPhase))
        {LOGMEIN("AsmJsModule.cpp] 2467\n");
            AsmJSCompiler::OutputMessage(GetScriptContext(), DEIT_ASMJS_SUCCEEDED, _u("Successfully compiled asm.js code"));
        }
        else
        {
            uint64 us = GetCompileTime();
            uint64 ms = us / 1000;
            us = us % 1000;
            AsmJSCompiler::OutputMessage(GetScriptContext(), DEIT_ASMJS_SUCCEEDED, _u("Successfully compiled asm.js code (total compilation time %llu.%llums)"), ms, us);
        }

        if (PHASE_TRACE1(AsmjsPhase))
        {LOGMEIN("AsmJsModule.cpp] 2479\n");
            for (int i = 0; i < AsmJsCompilation::Phases_COUNT; i++)
            {LOGMEIN("AsmJsModule.cpp] 2481\n");
                uint64 us = mPhaseCompileTime[i].ToMicroseconds();
                uint64 ms = us / 1000;
                us = us % 1000;
                Output::Print(_u("%20s : %llu.%llums\n"), AsmPhaseNames[i], ms, us);
            }
            Output::Flush();
        }
    }

    BVStatic<ASMMATH_BUILTIN_SIZE> AsmJsModuleCompiler::GetAsmMathBuiltinUsedBV()
    {LOGMEIN("AsmJsModule.cpp] 2492\n");
        return mAsmMathBuiltinUsedBV;
    }

    BVStatic<ASMARRAY_BUILTIN_SIZE> AsmJsModuleCompiler::GetAsmArrayBuiltinUsedBV()
    {LOGMEIN("AsmJsModule.cpp] 2497\n");
        return mAsmArrayBuiltinUsedBV;
    }

    void AsmJsModuleInfo::SetFunctionCount( int val )
    {LOGMEIN("AsmJsModule.cpp] 2502\n");
        Assert( mFunctions == nullptr );
        mFunctionCount = val;
        mFunctions = RecyclerNewArray( mRecycler, ModuleFunction, val );
    }

    void AsmJsModuleInfo::SetFunctionTableCount( int val )
    {LOGMEIN("AsmJsModule.cpp] 2509\n");
        Assert( mFunctionTables == nullptr );
        mFunctionTableCount = val;
        mFunctionTables = RecyclerNewArray( mRecycler, ModuleFunctionTable, val );
    }

    void AsmJsModuleInfo::SetFunctionImportCount( int val )
    {LOGMEIN("AsmJsModule.cpp] 2516\n");
        Assert( mFunctionImports == nullptr );
        mFunctionImportCount = val;
        mFunctionImports = RecyclerNewArray( mRecycler, ModuleFunctionImport, val );
    }

    void AsmJsModuleInfo::SetVarCount( int val )
    {LOGMEIN("AsmJsModule.cpp] 2523\n");
        Assert( mVars == nullptr );
        mVarCount = val;
        mVars = RecyclerNewArray( mRecycler, ModuleVar, val );
    }

    void AsmJsModuleInfo::SetVarImportCount( int val )
    {LOGMEIN("AsmJsModule.cpp] 2530\n");
        Assert( mVarImports == nullptr );
        mVarImportCount = val;
        mVarImports = RecyclerNewArray( mRecycler, ModuleVarImport, val );
    }

    void AsmJsModuleInfo::SetExportsCount( int count )
    {LOGMEIN("AsmJsModule.cpp] 2537\n");
        if( count )
        {LOGMEIN("AsmJsModule.cpp] 2539\n");
            mExports = RecyclerNewPlus( mRecycler, count * sizeof( PropertyId ), PropertyIdArray, count, 0);
            mExportsFunctionLocation = RecyclerNewArray( mRecycler, RegSlot, count );
        }
        mExportsCount = count;
    }

    void AsmJsModuleInfo::InitializeSlotMap(int val)
    {LOGMEIN("AsmJsModule.cpp] 2547\n");
        Assert(mSlotMap == nullptr);
        mSlotsCount = val;
        mSlotMap = RecyclerNew(mRecycler, AsmJsSlotMap, mRecycler);
    }

    void AsmJsModuleInfo::SetFunctionTableSize( int index, uint size )
    {LOGMEIN("AsmJsModule.cpp] 2554\n");
        Assert( mFunctionTables != nullptr );
        Assert( index < mFunctionTableCount );
        ModuleFunctionTable& table = mFunctionTables[index];
        table.size = size;
        table.moduleFunctionIndex = RecyclerNewArray( mRecycler, RegSlot, size );
    }

    void AsmJsModuleInfo::EnsureHeapAttached(ScriptFunction * func)
    {LOGMEIN("AsmJsModule.cpp] 2563\n");
        FrameDisplay* frame = func->GetEnvironment();
        ArrayBuffer* moduleArrayBuffer = nullptr;
#ifdef ENABLE_WASM
        if (func->GetFunctionBody()->IsWasmFunction())
        {LOGMEIN("AsmJsModule.cpp] 2568\n");
            WebAssemblyMemory * wasmMem = *(WebAssemblyMemory**)((Var*)frame->GetItem(0) + AsmJsModuleMemory::MemoryTableBeginOffset);
            if (wasmMem != nullptr)
            {LOGMEIN("AsmJsModule.cpp] 2571\n");
                moduleArrayBuffer = wasmMem->GetBuffer();
            }
        }
        else
#endif
        {
            moduleArrayBuffer = *(ArrayBuffer**)((Var*)frame->GetItem(0) + AsmJsModuleMemory::MemoryTableBeginOffset);
        }

        if (moduleArrayBuffer && moduleArrayBuffer->IsDetached())
        {LOGMEIN("AsmJsModule.cpp] 2582\n");
            Throw::OutOfMemory();
        }
    }

    void * AsmJsModuleInfo::ConvertFrameForJavascript(void * asmMemory, ScriptFunction* func)
    {LOGMEIN("AsmJsModule.cpp] 2588\n");
        FunctionBody * body = func->GetFunctionBody();
        AsmJsFunctionInfo * asmFuncInfo = body->GetAsmJsFunctionInfo();
        FunctionBody * moduleBody = asmFuncInfo->GetModuleFunctionBody();
        AsmJsModuleInfo * asmModuleInfo = moduleBody->GetAsmJsModuleInfo();
        Assert(asmModuleInfo);

        ScriptContext * scriptContext = func->GetScriptContext();
        // AsmJsModuleEnvironment is all laid out here
        Var * asmJsEnvironment = static_cast<Var*>(func->GetEnvironment()->GetItem(0));
        Var * asmBufferPtr = asmJsEnvironment + asmModuleInfo->GetModuleMemory().mArrayBufferOffset;
        ArrayBuffer * asmBuffer = *asmBufferPtr ? ArrayBuffer::FromVar(*asmBufferPtr) : nullptr;

        Var stdLibObj = *(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mStdLibOffset);
        Var asmMathObject = stdLibObj ? JavascriptOperators::OP_GetProperty(stdLibObj, PropertyIds::Math, scriptContext) : nullptr;

        Var * asmFFIs = asmJsEnvironment + asmModuleInfo->GetModuleMemory().mFFIOffset;
        Var * asmFuncs = asmJsEnvironment + asmModuleInfo->GetModuleMemory().mFuncOffset;
        Var ** asmFuncPtrs = reinterpret_cast<Var**>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mFuncPtrOffset);

        double * asmDoubleVars = reinterpret_cast<double*>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mDoubleOffset);
        int * asmIntVars = reinterpret_cast<int*>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mIntOffset);
        float * asmFloatVars = reinterpret_cast<float*>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mFloatOffset);

        AsmJsSIMDValue * asmSIMDVars = reinterpret_cast<AsmJsSIMDValue*>(asmJsEnvironment + asmModuleInfo->GetModuleMemory().mSimdOffset);


#if DEBUG
        Field(Var) * slotArray = RecyclerNewArrayZ(scriptContext->GetRecycler(), Field(Var), moduleBody->scopeSlotArraySize + ScopeSlots::FirstSlotIndex);
#else
        Field(Var) * slotArray = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), moduleBody->scopeSlotArraySize + ScopeSlots::FirstSlotIndex);
#endif
        ScopeSlots scopeSlots((Js::Var*)slotArray);
        scopeSlots.SetCount(moduleBody->scopeSlotArraySize);
        scopeSlots.SetScopeMetadata(moduleBody->GetFunctionInfo());

        auto asmSlotMap = asmModuleInfo->GetAsmJsSlotMap();
        Assert((uint)asmModuleInfo->GetSlotsCount() >= moduleBody->scopeSlotArraySize);

        Js::ActivationObject* activeScopeObject = nullptr;
        if (moduleBody->GetObjectRegister() != 0)
        {LOGMEIN("AsmJsModule.cpp] 2629\n");
            activeScopeObject = static_cast<ActivationObject*>(scriptContext->GetLibrary()->CreateActivationObject());
        }

        PropertyId* propertyIdArray = moduleBody->GetPropertyIdsForScopeSlotArray();
        uint slotsCount = moduleBody->scopeSlotArraySize;
        for (uint i = 0; i < slotsCount; ++i)
        {LOGMEIN("AsmJsModule.cpp] 2636\n");
            AsmJsSlot * asmSlot;
            bool found = asmSlotMap->TryGetValue(propertyIdArray[i], &asmSlot);
            // we should have everything we need in the map
            Assert(found);
            Var value = nullptr;
            switch (asmSlot->symType)
            {LOGMEIN("AsmJsModule.cpp] 2643\n");
            case AsmJsSymbol::ConstantImport:
            case AsmJsSymbol::Variable:
            {LOGMEIN("AsmJsModule.cpp] 2646\n");
                switch (asmSlot->varType)
                {LOGMEIN("AsmJsModule.cpp] 2648\n");
                case AsmJsVarType::Double:
                    value = JavascriptNumber::NewWithCheck(asmDoubleVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Float:
                    value = JavascriptNumber::NewWithCheck(asmFloatVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Int:
                    value = JavascriptNumber::ToVar(asmIntVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Float32x4:
                    value = JavascriptSIMDFloat32x4::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Float64x2:
                    value = JavascriptSIMDFloat64x2::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Int32x4:
                    value = JavascriptSIMDInt32x4::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Int16x8:
                    value = JavascriptSIMDInt16x8::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Int8x16:
                    value = JavascriptSIMDInt8x16::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Uint32x4:
                    value = JavascriptSIMDUint32x4::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Uint16x8:
                    value = JavascriptSIMDUint16x8::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Uint8x16:
                    value = JavascriptSIMDUint8x16::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Bool32x4:
                    value = JavascriptSIMDBool32x4::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Bool16x8:
                    value = JavascriptSIMDBool16x8::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                case AsmJsVarType::Bool8x16:
                    value = JavascriptSIMDBool8x16::New(&asmSIMDVars[asmSlot->location], scriptContext);
                    break;
                default:
                    Assume(UNREACHED);
                }
                break;
            }
            case AsmJsSymbol::ModuleArgument:
            {LOGMEIN("AsmJsModule.cpp] 2697\n");
                switch (asmSlot->argType)
                {LOGMEIN("AsmJsModule.cpp] 2699\n");
                case AsmJsModuleArg::ArgType::StdLib:
                    value = stdLibObj;
                    break;
                case AsmJsModuleArg::ArgType::Import:
                    // we can't reference this inside functions (and don't hold onto it), but must set to something, so set it to be undefined
                    value = scriptContext->GetLibrary()->GetUndefined();
                    break;
                case AsmJsModuleArg::ArgType::Heap:
                    value = asmBuffer;
                    break;
                default:
                    Assume(UNREACHED);
                }
                break;
            }
            case AsmJsSymbol::ImportFunction:
                value = asmFFIs[asmSlot->location];
                break;
            case AsmJsSymbol::FuncPtrTable:
                value = JavascriptArray::OP_NewScArrayWithElements(asmSlot->funcTableSize, asmFuncPtrs[asmSlot->location], scriptContext);
                break;
            case AsmJsSymbol::ModuleFunction:
                value = asmFuncs[asmSlot->location];
                break;
            case AsmJsSymbol::MathConstant:
                value = JavascriptNumber::NewWithCheck(asmSlot->mathConstVal, scriptContext);
                break;
            case AsmJsSymbol::ClosureFunction:
                // we can't reference this inside functions but must set to something, so set it to be undefined
                value = scriptContext->GetLibrary()->GetUndefined();
                break;
            case AsmJsSymbol::ArrayView:
            {LOGMEIN("AsmJsModule.cpp] 2732\n");
                AnalysisAssert(asmBuffer);
#ifdef _M_X64
                const bool isOptimizedBuffer = true;
#elif _M_IX86
                const bool isOptimizedBuffer = false;
#else
                Assert(UNREACHED);
                const bool isOptimizedBuffer = false;
#endif
                Assert(isOptimizedBuffer == asmBuffer->IsValidVirtualBufferLength(asmBuffer->GetByteLength()));
                switch (asmSlot->viewType)
                {LOGMEIN("AsmJsModule.cpp] 2744\n");
                case ArrayBufferView::TYPE_FLOAT32:
                    value = TypedArray<float, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 2, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_FLOAT64:
                    value = TypedArray<double, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 3, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_INT8:
                    value = TypedArray<int8, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength(), scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_INT16:
                    value = TypedArray<int16, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 1, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_INT32:
                    value = TypedArray<int32, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 2, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_UINT8:
                    value = TypedArray<uint8, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength(), scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_UINT16:
                    value = TypedArray<uint16, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 1, scriptContext->GetLibrary());
                    break;
                case ArrayBufferView::TYPE_UINT32:
                    value = TypedArray<uint32, false, isOptimizedBuffer>::Create(asmBuffer, 0, asmBuffer->GetByteLength() >> 2, scriptContext->GetLibrary());
                    break;
                default:
                    Assume(UNREACHED);
                }
                break;
            }
            case AsmJsSymbol::MathBuiltinFunction:
            {LOGMEIN("AsmJsModule.cpp] 2775\n");
                switch (asmSlot->builtinMathFunc)
                {LOGMEIN("AsmJsModule.cpp] 2777\n");
#define ASMJS_MATH_FUNC_NAMES(name, propertyName) \
                        case AsmJSMathBuiltin_##name: \
                            value = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::##propertyName, scriptContext); \
                            break;
#include "AsmJsBuiltInNames.h"
                default:
                    Assume(UNREACHED);
                }
                break;
            }
            case AsmJsSymbol::TypedArrayBuiltinFunction:
                switch (asmSlot->builtinArrayFunc)
                {LOGMEIN("AsmJsModule.cpp] 2790\n");
#define ASMJS_ARRAY_NAMES(name, propertyName) \
                        case AsmJSTypedArrayBuiltin_##name: \
                            value = JavascriptOperators::OP_GetProperty(stdLibObj, PropertyIds::##propertyName, scriptContext); \
                            break;
#include "AsmJsBuiltInNames.h"
                default:
                    Assume(UNREACHED);
                }
                break;

            case AsmJsSymbol::SIMDBuiltinFunction:
                switch (asmSlot->builtinSIMDFunc)
                {LOGMEIN("AsmJsModule.cpp] 2803\n");
#define ASMJS_SIMD_NAMES(name, propertyName, libName, entryPoint) \
                        case AsmJsSIMDBuiltin_##name: \
                            value = JavascriptOperators::OP_GetProperty(stdLibObj, PropertyIds::##propertyName, scriptContext); \
                            break;
#include "AsmJsBuiltInNames.h"
                default:
                    Assume(UNREACHED);
                }
                break;

            default:
                Assume(UNREACHED);
            }
            if (activeScopeObject != nullptr)
            {LOGMEIN("AsmJsModule.cpp] 2818\n");
                activeScopeObject->SetPropertyWithAttributes(
                    propertyIdArray[i],
                    value,
                    asmSlot->isConstVar ? PropertyConstDefaults : PropertyDynamicTypeDefaults,
                    nullptr);
            }
            else
            {
                // ensure we aren't multiply writing to a slot
                Assert(scopeSlots.Get(i) == nullptr);
                scopeSlots.Set(i, value);
            }
        }

        if (activeScopeObject != nullptr)
        {LOGMEIN("AsmJsModule.cpp] 2834\n");
            return (void*)activeScopeObject;
        }
        else
        {
            return (void*)slotArray;
        }
    }

    bool AsmJsModuleCompiler::LookupStdLibSIMDNameInMap(PropertyName name, AsmJsSIMDFunction **simdFunc, SIMDNameMap* map) const
    {LOGMEIN("AsmJsModule.cpp] 2844\n");
        return map->TryGetValue(name->GetPropertyId(), simdFunc);
    }

    bool AsmJsModuleCompiler::AddStandardLibrarySIMDNameInMap(PropertyId id, AsmJsSIMDFunction *simdFunc, SIMDNameMap* map)
    {LOGMEIN("AsmJsModule.cpp] 2849\n");
        //SimdBuiltin simdBuiltin(simdFunc->GetSimdBuiltInFunction(), simdFunc);
        if (map->ContainsKey(id))
        {LOGMEIN("AsmJsModule.cpp] 2852\n");
            return nullptr;
        }

        return map->AddNew(id, simdFunc) == -1 ? false : true;
    }

    bool AsmJsModuleCompiler::LookupStdLibSIMDName(PropertyId baseId, PropertyName fieldName, AsmJsSIMDFunction **simdFunc)
    {LOGMEIN("AsmJsModule.cpp] 2860\n");
        switch (baseId)
        {LOGMEIN("AsmJsModule.cpp] 2862\n");
        case PropertyIds::Int32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDInt32x4Map);
        case PropertyIds::Bool32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDBool32x4Map);
        case PropertyIds::Bool16x8:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDBool16x8Map);
        case PropertyIds::Bool8x16:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDBool8x16Map);
        case PropertyIds::Float32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDFloat32x4Map);
        case PropertyIds::Float64x2:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDFloat64x2Map);
        case PropertyIds::Int16x8:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDInt16x8Map);
        case PropertyIds::Int8x16:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDInt8x16Map);
        case PropertyIds::Uint32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDUint32x4Map);
        case PropertyIds::Uint16x8:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDUint16x8Map);
        case PropertyIds::Uint8x16:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDUint8x16Map);

        default:
            AssertMsg(false, "Invalid SIMD type");
            return false;
        }
    }

    bool AsmJsModuleCompiler::LookupStdLibSIMDName(AsmJsSIMDBuiltinFunction baseId, PropertyName fieldName, AsmJsSIMDFunction **simdFunc)
    {LOGMEIN("AsmJsModule.cpp] 2893\n");
        switch (baseId)
        {LOGMEIN("AsmJsModule.cpp] 2895\n");
        case AsmJsSIMDBuiltin_Int32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDInt32x4Map);
        case AsmJsSIMDBuiltin_Bool32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDBool32x4Map);
        case AsmJsSIMDBuiltin_Bool16x8:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDBool16x8Map);
        case AsmJsSIMDBuiltin_Bool8x16:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDBool8x16Map);
        case AsmJsSIMDBuiltin_Float32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDFloat32x4Map);
        case AsmJsSIMDBuiltin_Float64x2:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDFloat64x2Map);
        case AsmJsSIMDBuiltin_Int16x8:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDInt16x8Map);
        case AsmJsSIMDBuiltin_Int8x16:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDInt8x16Map);
        case AsmJsSIMDBuiltin_Uint32x4:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDUint32x4Map);
        case AsmJsSIMDBuiltin_Uint16x8:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDUint16x8Map);
        case AsmJsSIMDBuiltin_Uint8x16:
            return LookupStdLibSIMDNameInMap(fieldName, simdFunc, &mStdLibSIMDUint8x16Map);
        default:
            AssertMsg(false, "Invalid SIMD type");
            return false;
        }
    }

    AsmJsSIMDFunction* AsmJsModuleCompiler::LookupSimdConstructor(PropertyName name)
    {LOGMEIN("AsmJsModule.cpp] 2925\n");
        AsmJsFunctionDeclaration *func = LookupFunction(name);
        if (func == nullptr || func->GetSymbolType() != AsmJsSymbol::SIMDBuiltinFunction)
        {LOGMEIN("AsmJsModule.cpp] 2928\n");
            return nullptr;
        }
        AsmJsSIMDFunction *simdFunc = func->Cast<AsmJsSIMDFunction>();
        if (simdFunc->IsConstructor())
        {LOGMEIN("AsmJsModule.cpp] 2933\n");
            return simdFunc;
        }
        return nullptr;
    }

    AsmJsSIMDFunction* AsmJsModuleCompiler::LookupSimdTypeCheck(PropertyName name)
    {LOGMEIN("AsmJsModule.cpp] 2940\n");
        AsmJsFunctionDeclaration *func = LookupFunction(name);
        if (func == nullptr || func->GetSymbolType() != AsmJsSymbol::SIMDBuiltinFunction)
        {LOGMEIN("AsmJsModule.cpp] 2943\n");
            return nullptr;
        }
        AsmJsSIMDFunction *simdFunc = func->Cast<AsmJsSIMDFunction>();
        if (simdFunc->IsTypeCheck())
        {LOGMEIN("AsmJsModule.cpp] 2948\n");
            return simdFunc;
        }
        return nullptr;
    }

    AsmJsSIMDFunction* AsmJsModuleCompiler::LookupSimdOperation(PropertyName name)
    {LOGMEIN("AsmJsModule.cpp] 2955\n");
        AsmJsFunctionDeclaration *func = LookupFunction(name);
        if (func == nullptr || func->GetSymbolType() != AsmJsSymbol::SIMDBuiltinFunction)
        {LOGMEIN("AsmJsModule.cpp] 2958\n");
            return nullptr;
        }
        AsmJsSIMDFunction *simdFunc = func->Cast<AsmJsSIMDFunction>();
        if (simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Int32x4 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Int16x8 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Int8x16 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Float32x4 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Float64x2 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Uint32x4 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Uint16x8 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Uint8x16 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Bool32x4 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Bool16x8 &&
            simdFunc->GetSimdBuiltInFunction() != AsmJsSIMDBuiltin_Bool8x16
            )
        {LOGMEIN("AsmJsModule.cpp] 2974\n");
            return simdFunc;
        }
        return nullptr;
    }


    bool AsmJsModuleCompiler::AddSimdValueVar(PropertyName name, ParseNode* pnode, AsmJsSIMDFunction* simdFunc)
    {LOGMEIN("AsmJsModule.cpp] 2982\n");
        AssertMsg(simdFunc->GetSymbolType() == AsmJsSymbol::SIMDBuiltinFunction, "Expecting SIMD builtin");
        AssertMsg(simdFunc->IsConstructor(), "Expecting constructor function");

        AsmJsSIMDValue value;
        AsmJsVarType type = simdFunc->GetConstructorVarType();

        // e.g. var g1 = f4(1.0, 2.0, 3.0, 4.0);
        if (!ValidateSimdConstructor(pnode, simdFunc, value))
        {LOGMEIN("AsmJsModule.cpp] 2991\n");
            return false;
        }

        AsmJsVar* var = Anew(&mAllocator, AsmJsVar, name);
        if (!var || !DefineIdentifier(name, var))
        {LOGMEIN("AsmJsModule.cpp] 2997\n");
            return false;
        }

        ++mVarCount;
        var->SetVarType(type);
        var->SetConstInitialiser(value);
        // acquire register
        var->SetLocation(mSimdVarSpace.AcquireRegister());
        return true;
    }

    bool AsmJsModuleCompiler::ValidateSimdConstructor(ParseNode* pnode, AsmJsSIMDFunction* simdFunc, AsmJsSIMDValue& value)
    {LOGMEIN("AsmJsModule.cpp] 3010\n");
        Assert(pnode->nop == knopCall);

        uint argCount = pnode->sxCall.argCount;
        ParseNode* argNode = pnode->sxCall.pnodeArgs;
        ParseNode *arg = argNode;
        uint nop = 0;
        AsmJsSIMDBuiltinFunction simdBuiltin = simdFunc->GetSimdBuiltInFunction();

        if (!simdFunc->IsConstructor(argCount))
        {LOGMEIN("AsmJsModule.cpp] 3020\n");
            return Fail(pnode, _u("Invalid SIMD constructor or wrong number of arguments."));
        }

        switch (simdBuiltin)
        {LOGMEIN("AsmJsModule.cpp] 3025\n");
        case AsmJsSIMDBuiltin_Float64x2:
        case AsmJsSIMDBuiltin_Float32x4:
            nop = (uint)knopFlt;
            break;
        case AsmJsSIMDBuiltin_Int32x4:
        case AsmJsSIMDBuiltin_Int16x8:
        case AsmJsSIMDBuiltin_Int8x16:
        case AsmJsSIMDBuiltin_Uint32x4:
        case AsmJsSIMDBuiltin_Uint16x8:
        case AsmJsSIMDBuiltin_Uint8x16:
        case AsmJsSIMDBuiltin_Bool32x4:
        case AsmJsSIMDBuiltin_Bool16x8:
        case AsmJsSIMDBuiltin_Bool8x16:
            nop = (uint)knopInt;
            break;
        default:
            Assert(UNREACHED);
        }

        if (simdFunc->GetArgCount() != argCount)
        {LOGMEIN("AsmJsModule.cpp] 3046\n");
            return Fail(pnode, _u("Invalid number of arguments to SIMD constructor."));
        }

        for (uint i = 0; i < argCount; i++)
        {LOGMEIN("AsmJsModule.cpp] 3051\n");
            arg = argNode;
            if (argNode->nop == knopList)
            {LOGMEIN("AsmJsModule.cpp] 3054\n");
                arg = ParserWrapper::GetBinaryLeft(argNode);
                argNode = ParserWrapper::GetBinaryRight(argNode);
            }
            Assert(arg);
            // store to SIMD Value
            if (arg->nop == nop)
            {LOGMEIN("AsmJsModule.cpp] 3061\n");
                if (nop == (uint)knopInt)
                {LOGMEIN("AsmJsModule.cpp] 3063\n");
                    switch (simdBuiltin)
                    {LOGMEIN("AsmJsModule.cpp] 3065\n");
                    case AsmJsSIMDBuiltin_Int32x4:
                        value.i32[i] = arg->sxInt.lw;
                        break;
                    case AsmJsSIMDBuiltin_Int16x8:
                        value.i16[i] = (int16)arg->sxInt.lw;
                        break;
                    case AsmJsSIMDBuiltin_Int8x16:
                        value.i8[i] = (int8)arg->sxInt.lw;
                        break;
                    case AsmJsSIMDBuiltin_Uint32x4:
                        value.u32[i] = (uint32)arg->sxInt.lw;
                        break;
                    case AsmJsSIMDBuiltin_Uint16x8:
                        value.u16[i] = (uint16)arg->sxInt.lw;
                        break;
                    case AsmJsSIMDBuiltin_Uint8x16:
                        value.u8[i] = (uint8)arg->sxInt.lw;
                        break;
                    case AsmJsSIMDBuiltin_Bool32x4:
                        value.i32[i] = (arg->sxInt.lw) ? -1 : 0;
                        break;
                    case AsmJsSIMDBuiltin_Bool16x8:
                        value.i16[i] = (arg->sxInt.lw) ? -1 : 0;
                        break;
                    case AsmJsSIMDBuiltin_Bool8x16:
                        value.i8[i] = (arg->sxInt.lw) ? -1 : 0;
                        break;
                    default:
                        Assert(UNREACHED);
                    }

                }
                else if (nop == (uint)knopFlt)
                {LOGMEIN("AsmJsModule.cpp] 3099\n");
                    if (simdBuiltin == AsmJsSIMDBuiltin_Float32x4)
                    {LOGMEIN("AsmJsModule.cpp] 3101\n");
                        value.f32[i] = (float)arg->sxFlt.dbl;
                    }
                    else // float64x2
                    {
                        value.f64[i] = arg->sxFlt.dbl;
                    }
                }
            }
            else
            {
                return Fail(pnode, _u("Invalid argument type to SIMD constructor."));
            }
        }
        return true;
    }
};
#endif
