//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
// Portions of this file are copyright 2014 Mozilla Foundation, available under the Apache 2.0 license.
//-------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------
// Copyright 2014 Mozilla Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http ://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"
#ifdef ASMJS_PLAT
#include "ByteCode/Symbol.h"
#include "ByteCode/FuncInfo.h"
#include "ByteCode/ByteCodeWriter.h"
#include "ByteCode/ByteCodeGenerator.h"

namespace Js
{
    bool
    AsmJSCompiler::CheckIdentifier(AsmJsModuleCompiler &m, ParseNode *usepn, PropertyName name)
    {LOGMEIN("AsmJs.cpp] 33\n");
        if (name == m.GetParser()->names()->arguments || name == m.GetParser()->names()->eval)
        {LOGMEIN("AsmJs.cpp] 35\n");
            return m.FailName(usepn, _u("'%s' is not an allowed identifier"), name);
        }
        return true;
    }

    bool
    AsmJSCompiler::CheckModuleLevelName(AsmJsModuleCompiler &m, ParseNode *usepn, PropertyName name)
    {
        if (!CheckIdentifier(m, usepn, name))
        {LOGMEIN("AsmJs.cpp] 45\n");
            return false;
        }
        if (name == m.GetModuleFunctionName())
        {LOGMEIN("AsmJs.cpp] 49\n");
            return m.FailName(usepn, _u("duplicate name '%s' not allowed"), name);
        }
        //Check for all the duplicates here.
        return true;
    }


    bool
    AsmJSCompiler::CheckFunctionHead(AsmJsModuleCompiler &m, ParseNode *fn, bool isGlobal /*= true*/)
    {LOGMEIN("AsmJs.cpp] 59\n");
        PnFnc fnc = fn->sxFnc;

        if (fnc.HasNonSimpleParameterList())
        {LOGMEIN("AsmJs.cpp] 63\n");
            return m.Fail(fn, _u("default & rest args not allowed"));
        }

        if (fnc.IsStaticMember())
        {LOGMEIN("AsmJs.cpp] 68\n");
            return m.Fail(fn, _u("static functions are not allowed"));
        }

        if (fnc.IsGenerator())
        {LOGMEIN("AsmJs.cpp] 73\n");
            return m.Fail(fn, _u("generator functions are not allowed"));
        }

        if (fnc.IsAsync())
        {LOGMEIN("AsmJs.cpp] 78\n");
            return m.Fail(fn, _u("async functions are not allowed"));
        }

        if (fnc.IsLambda())
        {LOGMEIN("AsmJs.cpp] 83\n");
            return m.Fail(fn, _u("lambda functions are not allowed"));
        }

        if (!isGlobal && fnc.nestedCount != 0)
        {LOGMEIN("AsmJs.cpp] 88\n");
            return m.Fail(fn, _u("closure functions are not allowed"));
        }

        return true;
    }

    bool AsmJSCompiler::CheckTypeAnnotation( AsmJsModuleCompiler &m, ParseNode *coercionNode, AsmJSCoercion *coercion,
        ParseNode **coercedExpr /*= nullptr */)
    {LOGMEIN("AsmJs.cpp] 97\n");
        switch( coercionNode->nop )
        {LOGMEIN("AsmJs.cpp] 99\n");
        case knopRsh:
        case knopLsh:
        case knopXor:
        case knopAnd:
        case knopOr: {LOGMEIN("AsmJs.cpp] 104\n");
            ParseNode *rhs = ParserWrapper::GetBinaryRight( coercionNode );
            *coercion = AsmJS_ToInt32;
            if( coercedExpr )
            {LOGMEIN("AsmJs.cpp] 108\n");

                if( rhs->nop == knopInt && rhs->sxInt.lw == 0 )
                {LOGMEIN("AsmJs.cpp] 111\n");
                    if( rhs->nop == knopAnd )
                    {LOGMEIN("AsmJs.cpp] 113\n");
                        // X & 0 == 0;
                        *coercedExpr = rhs;
                    }
                    else
                    {
                        // (X|0) == (X^0) == (X<<0) == (X>>0) == X
                        *coercedExpr = ParserWrapper::GetBinaryLeft( coercionNode );
                    }
                }
                else
                {
                    *coercedExpr = coercionNode;
                }
            }
            return true;
        }
        case knopPos: {LOGMEIN("AsmJs.cpp] 130\n");
            *coercion = AsmJS_ToNumber;
            if( coercedExpr )
            {LOGMEIN("AsmJs.cpp] 133\n");
                *coercedExpr = ParserWrapper::GetUnaryNode( coercionNode );
            }
            return true;
        }
        case knopCall: {LOGMEIN("AsmJs.cpp] 138\n");
            ParseNode* target;
            AsmJsFunctionDeclaration* sym;
            AsmJsMathFunction* mathSym;
            AsmJsSIMDFunction* simdSym;

            target = coercionNode->sxCall.pnodeTarget;

            if (!target || target->nop != knopName)
            {LOGMEIN("AsmJs.cpp] 147\n");
                return m.Fail(coercionNode, _u("Call must be of the form id(...)"));
            }

            simdSym = m.LookupSimdTypeCheck(target->name());
            // var x = f4.check(ffi.field)
            if (simdSym)
            {LOGMEIN("AsmJs.cpp] 154\n");
                if (coercionNode->sxCall.argCount == simdSym->GetArgCount())
                {LOGMEIN("AsmJs.cpp] 156\n");
                    switch (simdSym->GetSimdBuiltInFunction())
                    {LOGMEIN("AsmJs.cpp] 158\n");
                    case AsmJsSIMDBuiltin_int32x4_check:
                        *coercion = AsmJS_Int32x4;
                        break;
                    case AsmJsSIMDBuiltin_bool32x4_check:
                        *coercion = AsmJS_Bool32x4;
                        break;
                    case AsmJsSIMDBuiltin_bool16x8_check:
                        *coercion = AsmJS_Bool16x8;
                        break;
                    case AsmJsSIMDBuiltin_bool8x16_check:
                        *coercion = AsmJS_Bool8x16;
                        break;
                    case AsmJsSIMDBuiltin_float32x4_check:
                        *coercion = AsmJS_Float32x4;
                        break;
                    case AsmJsSIMDBuiltin_float64x2_check:
                        *coercion = AsmJS_Float64x2;
                        break;
                    case AsmJsSIMDBuiltin_int16x8_check:
                        *coercion = AsmJS_Int16x8;
                        break;
                    case AsmJsSIMDBuiltin_int8x16_check:
                        *coercion = AsmJS_Int8x16;
                        break;
                    case AsmJsSIMDBuiltin_uint32x4_check:
                        *coercion = AsmJS_Uint32x4;
                        break;
                    case AsmJsSIMDBuiltin_uint16x8_check:
                        *coercion = AsmJS_Uint16x8;
                        break;
                    case AsmJsSIMDBuiltin_uint8x16_check:
                        *coercion = AsmJS_Uint8x16;
                        break;
                    default:
                        Assert(UNREACHED);
                    }
                    if (coercedExpr)
                    {LOGMEIN("AsmJs.cpp] 196\n");
                        *coercedExpr = coercionNode->sxCall.pnodeArgs;
                    }

                    return true;
                }
                else
                {
                    return m.Fail(coercionNode, _u("Invalid SIMD coercion"));
                }

            }
            // not a SIMD coercion, fall through

            *coercion = AsmJS_FRound;
            sym = m.LookupFunction(target->name());
            mathSym = (AsmJsMathFunction*)sym;

            if (!(mathSym && mathSym->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround))
            {LOGMEIN("AsmJs.cpp] 215\n");
                return m.Fail( coercionNode, _u("call must be to fround coercion") );
            }
            if( coercedExpr )
            {LOGMEIN("AsmJs.cpp] 219\n");
                *coercedExpr = coercionNode->sxCall.pnodeArgs;
            }
            return true;
        }
        case knopInt:{LOGMEIN("AsmJs.cpp] 224\n");
            *coercion = AsmJS_ToInt32;
            if( coercedExpr )
            {LOGMEIN("AsmJs.cpp] 227\n");
                *coercedExpr = coercionNode;
            }
            return true;
        }
        case knopFlt:{LOGMEIN("AsmJs.cpp] 232\n");
            if (ParserWrapper::IsMinInt(coercionNode))
            {LOGMEIN("AsmJs.cpp] 234\n");
                *coercion = AsmJS_ToInt32;
            }
            else if (coercionNode->sxFlt.maybeInt)
            {LOGMEIN("AsmJs.cpp] 238\n");
                return m.Fail(coercionNode, _u("Integer literal in return must be in range [-2^31, 2^31)"));
            }
            else
            {
                *coercion = AsmJS_ToNumber;
            }
            if( coercedExpr )
            {LOGMEIN("AsmJs.cpp] 246\n");
                *coercedExpr = coercionNode ;
            }
            return true;
        }
        case knopName:{LOGMEIN("AsmJs.cpp] 251\n");

            // in this case we are returning a constant var from the global scope
            AsmJsSymbol * constSymSource = m.LookupIdentifier(coercionNode->name());

            if (!constSymSource)
            {LOGMEIN("AsmJs.cpp] 257\n");
                return m.Fail(coercionNode, _u("Identifier not globally declared"));
            }

            AsmJsVar * constSrc = constSymSource->Cast<AsmJsVar>();

            if (constSymSource->GetSymbolType() != AsmJsSymbol::Variable || constSrc->isMutable())
            {LOGMEIN("AsmJs.cpp] 264\n");
                return m.Fail(coercionNode, _u("Unannotated variables must be constant"));
            }

            if (constSrc->GetType().isSigned())
            {LOGMEIN("AsmJs.cpp] 269\n");
                *coercion = AsmJS_ToInt32;
            }
            else if (constSrc->GetType().isDouble())
            {LOGMEIN("AsmJs.cpp] 273\n");
                *coercion = AsmJS_ToNumber;
            }
            else
            {
                Assert(constSrc->GetType().isFloat());
                *coercion = AsmJS_FRound;
            }
            if (coercedExpr)
            {LOGMEIN("AsmJs.cpp] 282\n");
                *coercedExpr = coercionNode;
            }
            return true;
        }
        default:;
        }

        return m.Fail( coercionNode, _u("must be of the form +x, fround(x) or x|0") );
    }

    bool
    AsmJSCompiler::CheckModuleArgument(AsmJsModuleCompiler &m, ParseNode *arg, PropertyName *name, AsmJsModuleArg::ArgType type)
    {LOGMEIN("AsmJs.cpp] 295\n");
        if (!ParserWrapper::IsDefinition(arg))
        {LOGMEIN("AsmJs.cpp] 297\n");
            return m.Fail(arg, _u("duplicate argument name not allowed"));
        }

        if (!CheckIdentifier(m, arg, arg->name()))
        {LOGMEIN("AsmJs.cpp] 302\n");
            return false;
        }
        *name = arg->name();

        m.GetByteCodeGenerator()->AssignPropertyId(*name);

        AsmJsModuleArg * moduleArg = Anew(m.GetAllocator(), AsmJsModuleArg, *name, type);

        if (!m.DefineIdentifier(*name, moduleArg))
        {LOGMEIN("AsmJs.cpp] 312\n");
            return m.Fail(arg, _u("duplicate argument name not allowed"));
        }

        if (!CheckModuleLevelName(m, arg, *name))
        {LOGMEIN("AsmJs.cpp] 317\n");
            return false;
        }

        return true;
    }

    bool
    AsmJSCompiler::CheckModuleArguments(AsmJsModuleCompiler &m, ParseNode *fn)
    {LOGMEIN("AsmJs.cpp] 326\n");
        ArgSlot numFormals = 0;

        ParseNode *arg1 = ParserWrapper::FunctionArgsList( fn, numFormals );
        ParseNode *arg2 = arg1 ? ParserWrapper::NextVar( arg1 ) : nullptr;
        ParseNode *arg3 = arg2 ? ParserWrapper::NextVar( arg2 ) : nullptr;

        if (numFormals > 3)
        {LOGMEIN("AsmJs.cpp] 334\n");
            return m.Fail(fn, _u("asm.js modules takes at most 3 argument"));
        }

        PropertyName arg1Name = nullptr;
        if (numFormals >= 1 && !CheckModuleArgument(m, arg1, &arg1Name, AsmJsModuleArg::ArgType::StdLib))
        {LOGMEIN("AsmJs.cpp] 340\n");
            return false;
        }

        m.InitStdLibArgName(arg1Name);

        PropertyName arg2Name = nullptr;
        if (numFormals >= 2 && !CheckModuleArgument(m, arg2, &arg2Name, AsmJsModuleArg::ArgType::Import))
        {LOGMEIN("AsmJs.cpp] 348\n");
            return false;
        }
        m.InitForeignArgName(arg2Name);

        PropertyName arg3Name = nullptr;
        if (numFormals >= 3 && !CheckModuleArgument(m, arg3, &arg3Name, AsmJsModuleArg::ArgType::Heap))
        {LOGMEIN("AsmJs.cpp] 355\n");
            return false;
        }
        m.InitBufferArgName(arg3Name);

        return true;
    }

    bool AsmJSCompiler::CheckGlobalVariableImportExpr( AsmJsModuleCompiler &m, PropertyName varName, AsmJSCoercion coercion, ParseNode *coercedExpr )
    {LOGMEIN("AsmJs.cpp] 364\n");
        if( !ParserWrapper::IsDotMember(coercedExpr) )
        {LOGMEIN("AsmJs.cpp] 366\n");
            return m.FailName( coercedExpr, _u("invalid import expression for global '%s'"), varName );
        }
        ParseNode *base = ParserWrapper::DotBase(coercedExpr);
        PropertyName field = ParserWrapper::DotMember(coercedExpr);

        PropertyName importName = m.GetForeignArgName();
        if (!importName || !field)
        {LOGMEIN("AsmJs.cpp] 374\n");
            return m.Fail(coercedExpr, _u("cannot import without an asm.js foreign parameter"));
        }
        m.GetByteCodeGenerator()->AssignPropertyId(field);
        if ((base->name() != importName))
        {LOGMEIN("AsmJs.cpp] 379\n");
            return m.FailName(coercedExpr, _u("base of import expression must be '%s'"), importName);
        }
        return m.AddGlobalVarImport(varName, field, coercion);
    }

    bool AsmJSCompiler::CheckGlobalVariableInitImport( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *initNode, bool isMutable /*= true*/)
    {LOGMEIN("AsmJs.cpp] 386\n");
        AsmJSCoercion coercion;
        ParseNode *coercedExpr;
        if( !CheckTypeAnnotation( m, initNode, &coercion, &coercedExpr ) )
        {LOGMEIN("AsmJs.cpp] 390\n");
            return false;
        }
        if ((ParserWrapper::IsFroundNumericLiteral(coercedExpr)) && coercion == AsmJSCoercion::AsmJS_FRound)
        {LOGMEIN("AsmJs.cpp] 394\n");
            return m.AddNumericVar(varName, coercedExpr, true, isMutable);
        }
        return CheckGlobalVariableImportExpr( m, varName, coercion, coercedExpr );
    }

    bool AsmJSCompiler::CheckNewArrayView( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *newExpr )
    {LOGMEIN("AsmJs.cpp] 401\n");
        Assert( newExpr->nop == knopNew );
        ParseNode *ctorExpr = newExpr->sxCall.pnodeTarget;
        ArrayBufferView::ViewType type;
        if( ParserWrapper::IsDotMember(ctorExpr) )
        {LOGMEIN("AsmJs.cpp] 406\n");
            ParseNode *base = ParserWrapper::DotBase(ctorExpr);

            PropertyName globalName = m.GetStdLibArgName();
            if (!globalName)
            {LOGMEIN("AsmJs.cpp] 411\n");
                return m.Fail(base, _u("cannot create array view without an asm.js global parameter"));
            }

            if (!ParserWrapper::IsNameDeclaration(base) || base->name() != globalName)
            {LOGMEIN("AsmJs.cpp] 416\n");
                return m.FailName(base, _u("expecting '%s.*Array"), globalName);
            }
            PropertyName fieldName = ParserWrapper::DotMember(ctorExpr);
            if (!fieldName)
            {LOGMEIN("AsmJs.cpp] 421\n");
                return m.FailName(ctorExpr, _u("Failed to define array view to var %s"), varName);
            }
            PropertyId field = fieldName->GetPropertyId();

            switch (field)
            {LOGMEIN("AsmJs.cpp] 427\n");
            case PropertyIds::Int8Array:
                type = ArrayBufferView::TYPE_INT8;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Int8Array);
                break;
            case PropertyIds::Uint8Array:
                type = ArrayBufferView::TYPE_UINT8;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Uint8Array);
                break;
            case PropertyIds::Int16Array:
                type = ArrayBufferView::TYPE_INT16;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Int16Array);
                break;
            case PropertyIds::Uint16Array:
                type = ArrayBufferView::TYPE_UINT16;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Uint16Array);
                break;
            case PropertyIds::Int32Array:
                type = ArrayBufferView::TYPE_INT32;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Int32Array);
                break;
            case PropertyIds::Uint32Array:
                type = ArrayBufferView::TYPE_UINT32;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Uint32Array);
                break;
            case PropertyIds::Float32Array:
                type = ArrayBufferView::TYPE_FLOAT32;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Float32Array);
                break;
            case PropertyIds::Float64Array:
                type = ArrayBufferView::TYPE_FLOAT64;
                m.AddArrayBuiltinUse(AsmJSTypedArrayBuiltin_Float64Array);
                break;
            default:
                return m.Fail(ctorExpr, _u("could not match typed array name"));
                break;
            }
        }
        else if (ctorExpr->nop == knopName)
        {LOGMEIN("AsmJs.cpp] 466\n");
            AsmJsSymbol * buffFunc = m.LookupIdentifier(ctorExpr->name());

            if (!buffFunc || buffFunc->GetSymbolType() != AsmJsSymbol::TypedArrayBuiltinFunction)
            {LOGMEIN("AsmJs.cpp] 470\n");
                return m.Fail(ctorExpr, _u("invalid 'new' import"));
            }
            type = buffFunc->Cast<AsmJsTypedArrayFunction>()->GetViewType();
            if (type == ArrayBufferView::TYPE_COUNT)
            {LOGMEIN("AsmJs.cpp] 475\n");
                return m.Fail(ctorExpr, _u("could not match typed array name"));
            }
        }
        else
        {
            return m.Fail(newExpr, _u("invalid 'new' import"));
        }

        ParseNode *bufArg = newExpr->sxCall.pnodeArgs;
        if( !bufArg || !ParserWrapper::IsNameDeclaration( bufArg ) )
        {LOGMEIN("AsmJs.cpp] 486\n");
            return m.Fail( ctorExpr, _u("array view constructor takes exactly one argument") );
        }

        PropertyName bufferName = m.GetBufferArgName();
        if( !bufferName )
        {LOGMEIN("AsmJs.cpp] 492\n");
            return m.Fail( bufArg, _u("cannot create array view without an asm.js heap parameter") );
        }

        if( bufferName != bufArg->name() )
        {LOGMEIN("AsmJs.cpp] 497\n");
            return m.FailName( bufArg, _u("argument to array view constructor must be '%s'"), bufferName );
        }


        if( !m.AddArrayView( varName, type ) )
        {LOGMEIN("AsmJs.cpp] 503\n");
            return m.FailName( ctorExpr, _u("Failed to define array view to var %s"), varName );
        }
        return true;
    }

    bool
    AsmJSCompiler::CheckGlobalDotImport(AsmJsModuleCompiler &m, PropertyName varName, ParseNode *initNode)
    {LOGMEIN("AsmJs.cpp] 511\n");
        ParseNode *base = ParserWrapper::DotBase(initNode);
        PropertyName field = ParserWrapper::DotMember(initNode);
        if( !field )
        {LOGMEIN("AsmJs.cpp] 515\n");
            return m.Fail( initNode, _u("Global import must be in the form c.x where c is stdlib or foreign and x is a string literal") );
        }
        m.GetByteCodeGenerator()->AssignPropertyId(field);
        PropertyName lib = nullptr;
        if (ParserWrapper::IsDotMember(base))
        {LOGMEIN("AsmJs.cpp] 521\n");
            lib = ParserWrapper::DotMember(base);
            base = ParserWrapper::DotBase(base);

            if (m.GetScriptContext()->GetConfig()->IsSimdjsEnabled())
            {LOGMEIN("AsmJs.cpp] 526\n");
                if (!lib || (lib->GetPropertyId() != PropertyIds::Math && lib->GetPropertyId() != PropertyIds::SIMD))
                {LOGMEIN("AsmJs.cpp] 528\n");
                    return m.FailName(initNode, _u("'%s' should be Math or SIMD, as in global.Math.xxxx"), field);
                }
            }
            else
            {
                if (!lib || lib->GetPropertyId() != PropertyIds::Math)
                {LOGMEIN("AsmJs.cpp] 535\n");
                    return m.FailName(initNode, _u("'%s' should be Math, as in global.Math.xxxx"), field);
                }
            }
        }

        if( ParserWrapper::IsNameDeclaration(base) && base->name() == m.GetStdLibArgName() )
        {LOGMEIN("AsmJs.cpp] 542\n");

            if (m.GetScriptContext()->GetConfig()->IsSimdjsEnabled())
            {LOGMEIN("AsmJs.cpp] 545\n");
                if (lib && lib->GetPropertyId() == PropertyIds::SIMD)
                {LOGMEIN("AsmJs.cpp] 547\n");
                    // global.SIMD.xxx
                    AsmJsSIMDFunction *simdFunc;

                    if (!m.LookupStdLibSIMDName(field->GetPropertyId(), field, &simdFunc))
                    {LOGMEIN("AsmJs.cpp] 552\n");
                        return m.FailName(initNode, _u("'%s' is not standard SIMD builtin"), varName);
                    }

                    if (simdFunc->GetName() != nullptr)
                    {LOGMEIN("AsmJs.cpp] 557\n");
                        OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, _u("Warning: SIMD Builtin already defined for var %s"), simdFunc->GetName()->Psz());
                    }
                    simdFunc->SetName(varName);
                    if (!m.DefineIdentifier(varName, simdFunc))
                    {LOGMEIN("AsmJs.cpp] 562\n");
                        return m.FailName(initNode, _u("Failed to define SIMD builtin function to var %s"), varName);
                    }
                    m.AddSimdBuiltinUse(simdFunc->GetSimdBuiltInFunction());
                    return true;
                }
            }

            // global.Math.xxx
            MathBuiltin mathBuiltin;
            if (m.LookupStandardLibraryMathName(field, &mathBuiltin))
            {LOGMEIN("AsmJs.cpp] 573\n");
                switch (mathBuiltin.kind)
                {LOGMEIN("AsmJs.cpp] 575\n");
                case MathBuiltin::Function:{LOGMEIN("AsmJs.cpp] 576\n");
                    auto func = mathBuiltin.u.func;
                    if (func->GetName() != nullptr)
                    {LOGMEIN("AsmJs.cpp] 579\n");
                        OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, _u("Warning: Math Builtin already defined for var %s"), func->GetName()->Psz());
                    }
                    func->SetName(varName);
                    if (!m.DefineIdentifier(varName, func))
                    {LOGMEIN("AsmJs.cpp] 584\n");
                        return m.FailName(initNode, _u("Failed to define math builtin function to var %s"), varName);
                    }
                    m.AddMathBuiltinUse(func->GetMathBuiltInFunction());
                }
                break;
                case MathBuiltin::Constant:
                    if (!m.AddNumericConst(varName, mathBuiltin.u.cst))
                    {LOGMEIN("AsmJs.cpp] 592\n");
                        return m.FailName(initNode, _u("Failed to define math constant to var %s"), varName);
                    }
                    m.AddMathBuiltinUse(mathBuiltin.mathLibFunctionName);
                    break;
                default:
                    Assume(UNREACHED);
                }
                return true;
            }

            TypedArrayBuiltin arrayBuiltin;
            if (m.LookupStandardLibraryArrayName(field, &arrayBuiltin))
            {LOGMEIN("AsmJs.cpp] 605\n");
                if (arrayBuiltin.mFunc->GetName() != nullptr)
                {LOGMEIN("AsmJs.cpp] 607\n");
                    OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, _u("Warning: Typed array builtin already defined for var %s"), arrayBuiltin.mFunc->GetName()->Psz());
                }
                arrayBuiltin.mFunc->SetName(varName);
                if (!m.DefineIdentifier(varName, arrayBuiltin.mFunc))
                {LOGMEIN("AsmJs.cpp] 612\n");
                    return m.FailName(initNode, _u("Failed to define typed array builtin function to var %s"), varName);
                }
                m.AddArrayBuiltinUse(arrayBuiltin.mFunc->GetArrayBuiltInFunction());
                return true;
            }

            return m.FailName(initNode, _u("'%s' is not a standard Math builtin"), field);
        }
        else if( ParserWrapper::IsNameDeclaration(base) && base->name() == m.GetForeignArgName() )
        {LOGMEIN("AsmJs.cpp] 622\n");
            // foreign import
            return m.AddModuleFunctionImport( varName, field );
        }
        else if (ParserWrapper::IsNameDeclaration(base))
        {LOGMEIN("AsmJs.cpp] 627\n");
            // Check if SIMD function import
            // e.g. var x = f4.add
            AsmJsSIMDFunction *simdFunc, *operation;

            simdFunc = m.LookupSimdConstructor(base->name());
            if (simdFunc == nullptr || !m.LookupStdLibSIMDName(simdFunc->GetSimdBuiltInFunction(), field, &operation))
            {LOGMEIN("AsmJs.cpp] 634\n");
                return m.FailName(initNode, _u("Invalid dot expression import. %s is not a standard SIMD operation"), varName);
            }

            if (operation->GetName() != nullptr)
            {LOGMEIN("AsmJs.cpp] 639\n");
                OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, _u("Warning: SIMD Builtin already defined for var %s"), operation->GetName()->Psz());
            }

            // bind operation to var
            operation->SetName(varName);
            if (!m.DefineIdentifier(varName, operation))
            {LOGMEIN("AsmJs.cpp] 646\n");
                return m.FailName(initNode, _u("Failed to define SIMD builtin function to var %s"), varName);
            }

            m.AddSimdBuiltinUse(operation->GetSimdBuiltInFunction());
            return true;
        }

        return m.Fail(initNode, _u("expecting c.y where c is either the global or foreign parameter"));
    }

    bool
    AsmJSCompiler::CheckModuleGlobal(AsmJsModuleCompiler &m, ParseNode *var)
    {LOGMEIN("AsmJs.cpp] 659\n");
        Assert(var->nop == knopVarDecl || var->nop == knopConstDecl);

        bool isMutable = var->nop != knopConstDecl;
        PropertyName name = var->name();

        m.GetByteCodeGenerator()->AssignPropertyId(name);
        if (m.LookupIdentifier(name))
        {LOGMEIN("AsmJs.cpp] 667\n");
            return m.FailName(var, _u("import variable %s names must be unique"), name);
        }

        if (!CheckModuleLevelName(m, var, name))
        {LOGMEIN("AsmJs.cpp] 672\n");
            return false;
        }

        if (!var->sxVar.pnodeInit)
        {LOGMEIN("AsmJs.cpp] 677\n");
            return m.Fail(var, _u("module import needs initializer"));
        }

        ParseNode *initNode = var->sxVar.pnodeInit;


        if( ParserWrapper::IsNumericLiteral( initNode ) )
        {LOGMEIN("AsmJs.cpp] 685\n");
            if (m.AddNumericVar(name, initNode, false, isMutable))
            {LOGMEIN("AsmJs.cpp] 687\n");
                return true;
            }
            else
            {
                return m.FailName(var, _u("Failed to declare numeric var %s"), name);
            }
        }


        if (initNode->nop == knopOr || initNode->nop == knopPos || initNode->nop == knopCall)
        {LOGMEIN("AsmJs.cpp] 698\n");
            // SIMD_JS
            // e.g. var x = f4(1.0, 2.0, 3.0, 4.0)
            if (initNode->nop == knopCall)
            {LOGMEIN("AsmJs.cpp] 702\n");
                AsmJsSIMDFunction* simdSym;
                // also checks if simd constructor
                simdSym = m.LookupSimdConstructor(initNode->sxCall.pnodeTarget->name());
                // call to simd constructor
                if (simdSym)
                {LOGMEIN("AsmJs.cpp] 708\n");
                    // validate args and define a SIMD symbol
                    return m.AddSimdValueVar(name, initNode, simdSym);
                }
                // else it is FFI import: var x = f4check(FFI.field), handled in CheckGlobalVariableInitImport
            }

           return CheckGlobalVariableInitImport(m, name, initNode, isMutable );
        }

        if( initNode->nop == knopNew )
        {LOGMEIN("AsmJs.cpp] 719\n");
           return CheckNewArrayView(m, var->name(), initNode);
        }

        if (ParserWrapper::IsDotMember(initNode))
        {LOGMEIN("AsmJs.cpp] 724\n");
            return CheckGlobalDotImport(m, name, initNode);
        }


        return m.Fail( initNode, _u("Failed to recognize global variable") );
    }

    bool
    AsmJSCompiler::CheckModuleGlobals(AsmJsModuleCompiler &m)
    {LOGMEIN("AsmJs.cpp] 734\n");
        ParseNode *varStmts;
        if( !ParserWrapper::ParseVarOrConstStatement( m.GetCurrentParserNode(), &varStmts ) )
        {LOGMEIN("AsmJs.cpp] 737\n");
            return false;
        }

        if (!varStmts)
        {LOGMEIN("AsmJs.cpp] 742\n");
            return true;
        }
        while (varStmts->nop == knopList)
        {LOGMEIN("AsmJs.cpp] 746\n");
            ParseNode * pnode = ParserWrapper::GetBinaryLeft(varStmts);
            while (pnode && pnode->nop != knopEndCode)
            {LOGMEIN("AsmJs.cpp] 749\n");
                ParseNode * decl;
                if (pnode->nop == knopList)
                {LOGMEIN("AsmJs.cpp] 752\n");
                    decl = ParserWrapper::GetBinaryLeft(pnode);
                    pnode = ParserWrapper::GetBinaryRight(pnode);
                }
                else
                {
                    decl = pnode;
                    pnode = nullptr;
                }

                if (decl->nop == knopFncDecl)
                {LOGMEIN("AsmJs.cpp] 763\n");
                    goto varDeclEnd;
                }
                else if (decl->nop != knopConstDecl && decl->nop != knopVarDecl)
                {LOGMEIN("AsmJs.cpp] 767\n");
                    break;
                }

                if (decl->sxVar.pnodeInit && decl->sxVar.pnodeInit->nop == knopArray)
                {LOGMEIN("AsmJs.cpp] 772\n");
                    // Assume we reached func tables
                    goto varDeclEnd;
                }

                if (!CheckModuleGlobal(m, decl))
                {LOGMEIN("AsmJs.cpp] 778\n");
                    return false;
                }
            }

            if (ParserWrapper::GetBinaryRight(varStmts)->nop == knopEndCode)
            {LOGMEIN("AsmJs.cpp] 784\n");
                // this is an error condition, but CheckFunctionsSequential will figure it out
                goto varDeclEnd;
            }
            varStmts = ParserWrapper::GetBinaryRight(varStmts);
        }
varDeclEnd:
        // we will collect information on the function tables now and come back to the functions themselves afterwards,
        // because function table information is used when processing function bodies
        ParseNode * fnNodes = varStmts;

        while (fnNodes->nop != knopEndCode && ParserWrapper::GetBinaryLeft(fnNodes)->nop == knopFncDecl)
        {LOGMEIN("AsmJs.cpp] 796\n");
            fnNodes = ParserWrapper::GetBinaryRight(fnNodes);
        }

        if (fnNodes->nop == knopEndCode)
        {LOGMEIN("AsmJs.cpp] 801\n");
            // if there are no function tables, we can just initialize count to 0
            m.SetFuncPtrTableCount(0);
        }
        else
        {
            m.SetCurrentParseNode(fnNodes);
            if (!CheckFunctionTables(m))
            {LOGMEIN("AsmJs.cpp] 809\n");
                return false;
            }
        }
        // this will move us back to the beginning of the function declarations
        m.SetCurrentParseNode(varStmts);
        return true;
    }


    bool AsmJSCompiler::CheckFunction( AsmJsModuleCompiler &m, ParseNode* fncNode )
    {LOGMEIN("AsmJs.cpp] 820\n");
        Assert( fncNode->nop == knopFncDecl );

        if( PHASE_TRACE1( Js::ByteCodePhase ) )
        {LOGMEIN("AsmJs.cpp] 824\n");
            Output::Print( _u("  Checking Asm function: %s\n"), fncNode->sxFnc.funcInfo->name);
        }

        if( !CheckFunctionHead( m, fncNode, false ) )
        {LOGMEIN("AsmJs.cpp] 829\n");
            return false;
        }

        AsmJsFunc* func = m.CreateNewFunctionEntry(fncNode);
        if (!func)
        {LOGMEIN("AsmJs.cpp] 835\n");
            return m.Fail(fncNode, _u("      Error creating function entry"));
        }
        return true;
    }

    bool AsmJSCompiler::CheckFunctionsSequential( AsmJsModuleCompiler &m )
    {LOGMEIN("AsmJs.cpp] 842\n");
        AsmJSParser& list = m.GetCurrentParserNode();
        Assert( list->nop == knopList );


        ParseNode* pnode = ParserWrapper::GetBinaryLeft(list);

        while (pnode->nop == knopFncDecl)
        {
            if( !CheckFunction( m, pnode ) )
            {LOGMEIN("AsmJs.cpp] 852\n");
                return false;
            }

            if(ParserWrapper::GetBinaryRight(list)->nop == knopEndCode)
            {LOGMEIN("AsmJs.cpp] 857\n");
                break;
            }
            list = ParserWrapper::GetBinaryRight(list);
            pnode = ParserWrapper::GetBinaryLeft(list);
        }

        m.SetCurrentParseNode( list );

        return true;
    }

    bool AsmJSCompiler::CheckFunctionTables(AsmJsModuleCompiler &m)
    {LOGMEIN("AsmJs.cpp] 870\n");
        AsmJSParser& list = m.GetCurrentParserNode();
        Assert(list->nop == knopList);

        int32 funcPtrTableCount = 0;
        while (list->nop != knopEndCode)
        {LOGMEIN("AsmJs.cpp] 876\n");
            ParseNode * varStmt = ParserWrapper::GetBinaryLeft(list);
            if (varStmt->nop != knopConstDecl && varStmt->nop != knopVarDecl)
            {LOGMEIN("AsmJs.cpp] 879\n");
                break;
            }
            if (!varStmt->sxVar.pnodeInit || varStmt->sxVar.pnodeInit->nop != knopArray)
            {LOGMEIN("AsmJs.cpp] 883\n");
                break;
            }
            const uint tableSize = varStmt->sxVar.pnodeInit->sxArrLit.count;
            if (!::Math::IsPow2(tableSize))
            {LOGMEIN("AsmJs.cpp] 888\n");
                return m.FailName(varStmt, _u("Function table [%s] size must be a power of 2"), varStmt->name());
            }
            if (!m.AddFunctionTable(varStmt->name(), tableSize))
            {LOGMEIN("AsmJs.cpp] 892\n");
                return m.FailName(varStmt, _u("Unable to create new function table %s"), varStmt->name());
            }

            AsmJsFunctionTable* ftable = (AsmJsFunctionTable*)m.LookupIdentifier(varStmt->name());
            Assert(ftable);
            ParseNode* pnode = varStmt->sxVar.pnodeInit->sxArrLit.pnode1;
            if (pnode->nop == knopList)
            {LOGMEIN("AsmJs.cpp] 900\n");
                pnode = ParserWrapper::GetBinaryLeft(pnode);
            }
            if (!ParserWrapper::IsNameDeclaration(pnode))
            {LOGMEIN("AsmJs.cpp] 904\n");
                return m.FailName(pnode, _u("Invalid element in function table %s"), varStmt->name());
            }
            ++funcPtrTableCount;
            list = ParserWrapper::GetBinaryRight(list);
        }

        m.SetFuncPtrTableCount(funcPtrTableCount);

        m.SetCurrentParseNode(list);
        return true;
    }

    bool AsmJSCompiler::CheckModuleReturn( AsmJsModuleCompiler& m )
    {LOGMEIN("AsmJs.cpp] 918\n");
        ParseNode* endStmt = m.GetCurrentParserNode();

        Assert( endStmt->nop == knopList );
        ParseNode* node = ParserWrapper::GetBinaryLeft( endStmt );
        ParseNode* endNode = ParserWrapper::GetBinaryRight( endStmt );

        if( node->nop != knopReturn || endNode->nop != knopEndCode )
        {LOGMEIN("AsmJs.cpp] 926\n");
            return m.Fail( node, _u("Only expression after table functions must be a return") );
        }

        ParseNode* objNode = node->sxReturn.pnodeExpr;
        if ( !objNode )
        {LOGMEIN("AsmJs.cpp] 932\n");
            return m.Fail( node, _u( "Module return must be an object or 1 function" ) );
        }

        if( objNode->nop != knopObject )
        {LOGMEIN("AsmJs.cpp] 937\n");
            if( ParserWrapper::IsNameDeclaration( objNode ) )
            {LOGMEIN("AsmJs.cpp] 939\n");
                PropertyName name = objNode->name();
                AsmJsSymbol* sym = m.LookupIdentifier( name );
                if( !sym )
                {LOGMEIN("AsmJs.cpp] 943\n");
                    return m.FailName( node, _u("Symbol %s not recognized inside module"), name );
                }

                if( sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                {LOGMEIN("AsmJs.cpp] 948\n");
                    return m.FailName( node, _u("Symbol %s can only be a function of the module"), name );
                }

                AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                if( !m.SetExportFunc( func ) )
                {LOGMEIN("AsmJs.cpp] 954\n");
                    return m.FailName( node, _u("Error adding return Symbol %s"), name );
                }
                return true;
            }
            return m.Fail( node, _u("Module return must be an object or 1 function") );
        }

        ParseNode* objectElement = ParserWrapper::GetUnaryNode(objNode);
        while( objectElement )
        {LOGMEIN("AsmJs.cpp] 964\n");
            ParseNode* member = nullptr;
            if( objectElement->nop == knopList )
            {LOGMEIN("AsmJs.cpp] 967\n");
                member = ParserWrapper::GetBinaryLeft( objectElement );
                objectElement = ParserWrapper::GetBinaryRight( objectElement );
            }
            else if( objectElement->nop == knopMember )
            {LOGMEIN("AsmJs.cpp] 972\n");
                member = objectElement;
                objectElement = nullptr;
            }
            else
            {
                return m.Fail( node, _u("Return object must only contain members") );
            }

            if( member )
            {LOGMEIN("AsmJs.cpp] 982\n");
                ParseNode* field = ParserWrapper::GetBinaryLeft( member );
                ParseNode* value = ParserWrapper::GetBinaryRight( member );
                if( !ParserWrapper::IsNameDeclaration( field ) || !ParserWrapper::IsNameDeclaration( value ) )
                {LOGMEIN("AsmJs.cpp] 986\n");
                    return m.Fail( node, _u("Return object member must be fields") );
                }

                AsmJsSymbol* sym = m.LookupIdentifier( value->name() );
                if( !sym )
                {LOGMEIN("AsmJs.cpp] 992\n");
                    return m.FailName( node, _u("Symbol %s not recognized inside module"), value->name() );
                }

                if( sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                {LOGMEIN("AsmJs.cpp] 997\n");
                    return m.FailName( node, _u("Symbol %s can only be a function of the module"), value->name() );
                }

                AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                if( !m.AddExport( field->name(), func->GetFunctionIndex() ) )
                {LOGMEIN("AsmJs.cpp] 1003\n");
                    return m.FailName( node, _u("Error adding return Symbol %s"), value->name() );
                }
            }
        }

        return true;
    }

    bool AsmJSCompiler::CheckFuncPtrTables( AsmJsModuleCompiler &m )
    {LOGMEIN("AsmJs.cpp] 1013\n");
        ParseNode *list = m.GetCurrentParserNode();
        if (!list)
        {LOGMEIN("AsmJs.cpp] 1016\n");
            return true;
        }
        while (list->nop != knopEndCode)
        {LOGMEIN("AsmJs.cpp] 1020\n");
            ParseNode * varStmt = ParserWrapper::GetBinaryLeft(list);
            if (varStmt->nop != knopConstDecl && varStmt->nop != knopVarDecl)
            {LOGMEIN("AsmJs.cpp] 1023\n");
                break;
            }

            ParseNode* nodeInit = varStmt->sxVar.pnodeInit;
            if( !nodeInit || nodeInit->nop != knopArray )
            {LOGMEIN("AsmJs.cpp] 1029\n");
                return m.Fail( varStmt, _u("Invalid variable after function declaration") );
            }

            PropertyName tableName = varStmt->name();

            AsmJsSymbol* symFunctionTable = m.LookupIdentifier(tableName);
            if( !symFunctionTable)
            {LOGMEIN("AsmJs.cpp] 1037\n");
                // func table not used in functions disregard it
            }
            else
            {
                //Check name
                if(symFunctionTable->GetSymbolType() != AsmJsSymbol::FuncPtrTable )
                {LOGMEIN("AsmJs.cpp] 1044\n");
                    return m.FailName( varStmt, _u("Variable %s is already defined"), tableName );
                }

                AsmJsFunctionTable* table = symFunctionTable->Cast<AsmJsFunctionTable>();
                if( table->IsDefined() )
                {LOGMEIN("AsmJs.cpp] 1050\n");
                    return m.FailName( varStmt, _u("Multiple declaration of function table %s"), tableName );
                }

                // Check content of the array
                uint count = nodeInit->sxArrLit.count;
                if( table->GetSize() != count )
                {LOGMEIN("AsmJs.cpp] 1057\n");
                    return m.FailName( varStmt, _u("Invalid size of function table %s"), tableName );
                }

                // Set the content of the array in the table
                ParseNode* node = nodeInit->sxArrLit.pnode1;
                uint i = 0;
                while( node )
                {LOGMEIN("AsmJs.cpp] 1065\n");
                    ParseNode* funcNameNode = nullptr;
                    if( node->nop == knopList )
                    {LOGMEIN("AsmJs.cpp] 1068\n");
                        funcNameNode = ParserWrapper::GetBinaryLeft( node );
                        node = ParserWrapper::GetBinaryRight( node );
                    }
                    else
                    {
                        Assert( i + 1 == count );
                        funcNameNode = node;
                        node = nullptr;
                    }

                    if( ParserWrapper::IsNameDeclaration( funcNameNode ) )
                    {LOGMEIN("AsmJs.cpp] 1080\n");
                        AsmJsSymbol* sym = m.LookupIdentifier( funcNameNode->name() );
                        if( !sym || sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                        {LOGMEIN("AsmJs.cpp] 1083\n");
                            return m.FailName( varStmt, _u("Element in function table %s is not a function"), tableName );
                        }
                        AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                        AsmJsRetType retType;
                        if (!table->SupportsArgCall(func->GetArgCount(), func->GetArgTypeArray(), retType))
                        {LOGMEIN("AsmJs.cpp] 1089\n");
                            return m.FailName(funcNameNode, _u("Function signatures in table %s do not match"), tableName);
                        }
                        if (!table->CheckAndSetReturnType(func->GetReturnType()))
                        {LOGMEIN("AsmJs.cpp] 1093\n");
                            return m.FailName(funcNameNode, _u("Function return types in table %s do not match"), tableName);
                        }
                        table->SetModuleFunctionIndex( func->GetFunctionIndex(), i );
                        ++i;
                    }
                    else
                    {
                        return m.FailName(funcNameNode, _u("Element in function table %s is not a function name"), tableName);
                    }
                }

                table->Define();
            }

            list = ParserWrapper::GetBinaryRight(list);
        }

        if( !m.AreAllFuncTableDefined() )
        {LOGMEIN("AsmJs.cpp] 1112\n");
            return m.Fail(list, _u("Some function table were used but not defined"));
        }

        m.SetCurrentParseNode(list);
        return true;
    }

    bool AsmJSCompiler::CheckModule( ExclusiveContext *cx, AsmJSParser &parser, ParseNode *stmtList )
    {LOGMEIN("AsmJs.cpp] 1121\n");
        AsmJsModuleCompiler m( cx, parser );
        if( !m.Init() )
        {LOGMEIN("AsmJs.cpp] 1124\n");
            return false;
        }
        if( PropertyName moduleFunctionName = ParserWrapper::FunctionName( m.GetModuleFunctionNode() ) )
        {
            if( !CheckModuleLevelName( m, m.GetModuleFunctionNode(), moduleFunctionName ) )
            {LOGMEIN("AsmJs.cpp] 1130\n");
                return false;
            }
            m.InitModuleName( moduleFunctionName );

            if( PHASE_TRACE1( Js::ByteCodePhase ) )
            {LOGMEIN("AsmJs.cpp] 1136\n");
                Output::Print( _u("Asm.Js Module [%s] detected, trying to compile\n"), moduleFunctionName->Psz() );
            }
        }

        m.AccumulateCompileTime(AsmJsCompilation::Module);

        if( !CheckFunctionHead( m, m.GetModuleFunctionNode() ) )
        {LOGMEIN("AsmJs.cpp] 1144\n");
            goto AsmJsCompilationError;
        }

        if (!CheckModuleArguments(m, m.GetModuleFunctionNode()))
        {LOGMEIN("AsmJs.cpp] 1149\n");
            goto AsmJsCompilationError;
        }

        if (!CheckModuleGlobals(m))
        {LOGMEIN("AsmJs.cpp] 1154\n");
            goto AsmJsCompilationError;
        }

        m.AccumulateCompileTime(AsmJsCompilation::Module);

        if (!CheckFunctionsSequential(m))
        {LOGMEIN("AsmJs.cpp] 1161\n");
            goto AsmJsCompilationError;
        }

        m.AccumulateCompileTime();
        m.InitMemoryOffsets();

        if( !m.CompileAllFunctions() )
        {LOGMEIN("AsmJs.cpp] 1169\n");
            return false;
        }

        m.AccumulateCompileTime(AsmJsCompilation::ByteCode);

        if (!CheckFuncPtrTables(m))
        {LOGMEIN("AsmJs.cpp] 1176\n");
            m.RevertAllFunctions();
            return false;
        }

        m.AccumulateCompileTime();

        if (!CheckModuleReturn(m))
        {LOGMEIN("AsmJs.cpp] 1184\n");
            m.RevertAllFunctions();
            return false;
        }

        m.CommitFunctions();
        m.CommitModule();
        m.AccumulateCompileTime(AsmJsCompilation::Module);

        m.PrintCompileTrace();

        return true;

AsmJsCompilationError:
        ParseNode * moduleNode = m.GetModuleFunctionNode();
        if( moduleNode )
        {LOGMEIN("AsmJs.cpp] 1200\n");
            FunctionBody* body = moduleNode->sxFnc.funcInfo->GetParsedFunctionBody();
            body->ResetByteCodeGenState();
        }

        cx->byteCodeGenerator->Writer()->Reset();
        return false;
    }

    bool AsmJSCompiler::Compile(ExclusiveContext *cx, AsmJSParser parser, ParseNode *stmtList)
    {
        if (!CheckModule(cx, parser, stmtList))
        {LOGMEIN("AsmJs.cpp] 1212\n");
            OutputError(cx->scriptContext, _u("Asm.js compilation failed."));
            return false;
        }
        return true;
    }

    void AsmJSCompiler::OutputError(ScriptContext * scriptContext, const wchar * message, ...)
    {
        va_list argptr;
        va_start(argptr, message);
        VOutputMessage(scriptContext, DEIT_ASMJS_FAILED, message, argptr);
    }

    void AsmJSCompiler::OutputMessage(ScriptContext * scriptContext, const DEBUG_EVENT_INFO_TYPE messageType, const wchar * message, ...)
    {
        va_list argptr;
        va_start(argptr, message);
        VOutputMessage(scriptContext, messageType, message, argptr);
    }

    void AsmJSCompiler::VOutputMessage(ScriptContext * scriptContext, const DEBUG_EVENT_INFO_TYPE messageType, const wchar * message, va_list argptr)
    {LOGMEIN("AsmJs.cpp] 1234\n");
        char16 buf[2048];
        size_t size;

        size = _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, message, argptr);
        if (size == -1)
        {LOGMEIN("AsmJs.cpp] 1240\n");
            size = 2048;
        }
        scriptContext->RaiseMessageToDebugger(messageType, buf, scriptContext->GetUrl());
        if (PHASE_TRACE1(AsmjsPhase) || PHASE_TESTTRACE1(AsmjsPhase))
        {LOGMEIN("AsmJs.cpp] 1245\n");
            Output::PrintBuffer(buf, size);
            Output::Print(_u("\n"));
            Output::Flush();
        }
    }
}

#endif
