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
    {TRACE_IT(45622);
        if (name == m.GetParser()->names()->arguments || name == m.GetParser()->names()->eval)
        {TRACE_IT(45623);
            return m.FailName(usepn, _u("'%s' is not an allowed identifier"), name);
        }
        return true;
    }

    bool
    AsmJSCompiler::CheckModuleLevelName(AsmJsModuleCompiler &m, ParseNode *usepn, PropertyName name)
    {
        if (!CheckIdentifier(m, usepn, name))
        {TRACE_IT(45624);
            return false;
        }
        if (name == m.GetModuleFunctionName())
        {TRACE_IT(45625);
            return m.FailName(usepn, _u("duplicate name '%s' not allowed"), name);
        }
        //Check for all the duplicates here.
        return true;
    }


    bool
    AsmJSCompiler::CheckFunctionHead(AsmJsModuleCompiler &m, ParseNode *fn, bool isGlobal /*= true*/)
    {TRACE_IT(45626);
        PnFnc fnc = fn->sxFnc;

        if (fnc.HasNonSimpleParameterList())
        {TRACE_IT(45627);
            return m.Fail(fn, _u("default & rest args not allowed"));
        }

        if (fnc.IsStaticMember())
        {TRACE_IT(45628);
            return m.Fail(fn, _u("static functions are not allowed"));
        }

        if (fnc.IsGenerator())
        {TRACE_IT(45629);
            return m.Fail(fn, _u("generator functions are not allowed"));
        }

        if (fnc.IsAsync())
        {TRACE_IT(45630);
            return m.Fail(fn, _u("async functions are not allowed"));
        }

        if (fnc.IsLambda())
        {TRACE_IT(45631);
            return m.Fail(fn, _u("lambda functions are not allowed"));
        }

        if (!isGlobal && fnc.nestedCount != 0)
        {TRACE_IT(45632);
            return m.Fail(fn, _u("closure functions are not allowed"));
        }

        return true;
    }

    bool AsmJSCompiler::CheckTypeAnnotation( AsmJsModuleCompiler &m, ParseNode *coercionNode, AsmJSCoercion *coercion,
        ParseNode **coercedExpr /*= nullptr */)
    {TRACE_IT(45633);
        switch( coercionNode->nop )
        {
        case knopRsh:
        case knopLsh:
        case knopXor:
        case knopAnd:
        case knopOr: {TRACE_IT(45634);
            ParseNode *rhs = ParserWrapper::GetBinaryRight( coercionNode );
            *coercion = AsmJS_ToInt32;
            if( coercedExpr )
            {TRACE_IT(45635);

                if( rhs->nop == knopInt && rhs->sxInt.lw == 0 )
                {TRACE_IT(45636);
                    if( rhs->nop == knopAnd )
                    {TRACE_IT(45637);
                        // X & 0 == 0;
                        *coercedExpr = rhs;
                    }
                    else
                    {TRACE_IT(45638);
                        // (X|0) == (X^0) == (X<<0) == (X>>0) == X
                        *coercedExpr = ParserWrapper::GetBinaryLeft( coercionNode );
                    }
                }
                else
                {TRACE_IT(45639);
                    *coercedExpr = coercionNode;
                }
            }
            return true;
        }
        case knopPos: {TRACE_IT(45640);
            *coercion = AsmJS_ToNumber;
            if( coercedExpr )
            {TRACE_IT(45641);
                *coercedExpr = ParserWrapper::GetUnaryNode( coercionNode );
            }
            return true;
        }
        case knopCall: {TRACE_IT(45642);
            ParseNode* target;
            AsmJsFunctionDeclaration* sym;
            AsmJsMathFunction* mathSym;
            AsmJsSIMDFunction* simdSym;

            target = coercionNode->sxCall.pnodeTarget;

            if (!target || target->nop != knopName)
            {TRACE_IT(45643);
                return m.Fail(coercionNode, _u("Call must be of the form id(...)"));
            }

            simdSym = m.LookupSimdTypeCheck(target->name());
            // var x = f4.check(ffi.field)
            if (simdSym)
            {TRACE_IT(45644);
                if (coercionNode->sxCall.argCount == simdSym->GetArgCount())
                {TRACE_IT(45645);
                    switch (simdSym->GetSimdBuiltInFunction())
                    {
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
                    {TRACE_IT(45646);
                        *coercedExpr = coercionNode->sxCall.pnodeArgs;
                    }

                    return true;
                }
                else
                {TRACE_IT(45647);
                    return m.Fail(coercionNode, _u("Invalid SIMD coercion"));
                }

            }
            // not a SIMD coercion, fall through

            *coercion = AsmJS_FRound;
            sym = m.LookupFunction(target->name());
            mathSym = (AsmJsMathFunction*)sym;

            if (!(mathSym && mathSym->GetMathBuiltInFunction() == AsmJSMathBuiltin_fround))
            {TRACE_IT(45648);
                return m.Fail( coercionNode, _u("call must be to fround coercion") );
            }
            if( coercedExpr )
            {TRACE_IT(45649);
                *coercedExpr = coercionNode->sxCall.pnodeArgs;
            }
            return true;
        }
        case knopInt:{TRACE_IT(45650);
            *coercion = AsmJS_ToInt32;
            if( coercedExpr )
            {TRACE_IT(45651);
                *coercedExpr = coercionNode;
            }
            return true;
        }
        case knopFlt:{TRACE_IT(45652);
            if (ParserWrapper::IsMinInt(coercionNode))
            {TRACE_IT(45653);
                *coercion = AsmJS_ToInt32;
            }
            else if (coercionNode->sxFlt.maybeInt)
            {TRACE_IT(45654);
                return m.Fail(coercionNode, _u("Integer literal in return must be in range [-2^31, 2^31)"));
            }
            else
            {TRACE_IT(45655);
                *coercion = AsmJS_ToNumber;
            }
            if( coercedExpr )
            {TRACE_IT(45656);
                *coercedExpr = coercionNode ;
            }
            return true;
        }
        case knopName:{TRACE_IT(45657);

            // in this case we are returning a constant var from the global scope
            AsmJsSymbol * constSymSource = m.LookupIdentifier(coercionNode->name());

            if (!constSymSource)
            {TRACE_IT(45658);
                return m.Fail(coercionNode, _u("Identifier not globally declared"));
            }

            AsmJsVar * constSrc = constSymSource->Cast<AsmJsVar>();

            if (constSymSource->GetSymbolType() != AsmJsSymbol::Variable || constSrc->isMutable())
            {TRACE_IT(45659);
                return m.Fail(coercionNode, _u("Unannotated variables must be constant"));
            }

            if (constSrc->GetType().isSigned())
            {TRACE_IT(45660);
                *coercion = AsmJS_ToInt32;
            }
            else if (constSrc->GetType().isDouble())
            {TRACE_IT(45661);
                *coercion = AsmJS_ToNumber;
            }
            else
            {TRACE_IT(45662);
                Assert(constSrc->GetType().isFloat());
                *coercion = AsmJS_FRound;
            }
            if (coercedExpr)
            {TRACE_IT(45663);
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
    {TRACE_IT(45664);
        if (!ParserWrapper::IsDefinition(arg))
        {TRACE_IT(45665);
            return m.Fail(arg, _u("duplicate argument name not allowed"));
        }

        if (!CheckIdentifier(m, arg, arg->name()))
        {TRACE_IT(45666);
            return false;
        }
        *name = arg->name();

        m.GetByteCodeGenerator()->AssignPropertyId(*name);

        AsmJsModuleArg * moduleArg = Anew(m.GetAllocator(), AsmJsModuleArg, *name, type);

        if (!m.DefineIdentifier(*name, moduleArg))
        {TRACE_IT(45667);
            return m.Fail(arg, _u("duplicate argument name not allowed"));
        }

        if (!CheckModuleLevelName(m, arg, *name))
        {TRACE_IT(45668);
            return false;
        }

        return true;
    }

    bool
    AsmJSCompiler::CheckModuleArguments(AsmJsModuleCompiler &m, ParseNode *fn)
    {TRACE_IT(45669);
        ArgSlot numFormals = 0;

        ParseNode *arg1 = ParserWrapper::FunctionArgsList( fn, numFormals );
        ParseNode *arg2 = arg1 ? ParserWrapper::NextVar( arg1 ) : nullptr;
        ParseNode *arg3 = arg2 ? ParserWrapper::NextVar( arg2 ) : nullptr;

        if (numFormals > 3)
        {TRACE_IT(45670);
            return m.Fail(fn, _u("asm.js modules takes at most 3 argument"));
        }

        PropertyName arg1Name = nullptr;
        if (numFormals >= 1 && !CheckModuleArgument(m, arg1, &arg1Name, AsmJsModuleArg::ArgType::StdLib))
        {TRACE_IT(45671);
            return false;
        }

        m.InitStdLibArgName(arg1Name);

        PropertyName arg2Name = nullptr;
        if (numFormals >= 2 && !CheckModuleArgument(m, arg2, &arg2Name, AsmJsModuleArg::ArgType::Import))
        {TRACE_IT(45672);
            return false;
        }
        m.InitForeignArgName(arg2Name);

        PropertyName arg3Name = nullptr;
        if (numFormals >= 3 && !CheckModuleArgument(m, arg3, &arg3Name, AsmJsModuleArg::ArgType::Heap))
        {TRACE_IT(45673);
            return false;
        }
        m.InitBufferArgName(arg3Name);

        return true;
    }

    bool AsmJSCompiler::CheckGlobalVariableImportExpr( AsmJsModuleCompiler &m, PropertyName varName, AsmJSCoercion coercion, ParseNode *coercedExpr )
    {TRACE_IT(45674);
        if( !ParserWrapper::IsDotMember(coercedExpr) )
        {TRACE_IT(45675);
            return m.FailName( coercedExpr, _u("invalid import expression for global '%s'"), varName );
        }
        ParseNode *base = ParserWrapper::DotBase(coercedExpr);
        PropertyName field = ParserWrapper::DotMember(coercedExpr);

        PropertyName importName = m.GetForeignArgName();
        if (!importName || !field)
        {TRACE_IT(45676);
            return m.Fail(coercedExpr, _u("cannot import without an asm.js foreign parameter"));
        }
        m.GetByteCodeGenerator()->AssignPropertyId(field);
        if ((base->name() != importName))
        {TRACE_IT(45677);
            return m.FailName(coercedExpr, _u("base of import expression must be '%s'"), importName);
        }
        return m.AddGlobalVarImport(varName, field, coercion);
    }

    bool AsmJSCompiler::CheckGlobalVariableInitImport( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *initNode, bool isMutable /*= true*/)
    {TRACE_IT(45678);
        AsmJSCoercion coercion;
        ParseNode *coercedExpr;
        if( !CheckTypeAnnotation( m, initNode, &coercion, &coercedExpr ) )
        {TRACE_IT(45679);
            return false;
        }
        if ((ParserWrapper::IsFroundNumericLiteral(coercedExpr)) && coercion == AsmJSCoercion::AsmJS_FRound)
        {TRACE_IT(45680);
            return m.AddNumericVar(varName, coercedExpr, true, isMutable);
        }
        return CheckGlobalVariableImportExpr( m, varName, coercion, coercedExpr );
    }

    bool AsmJSCompiler::CheckNewArrayView( AsmJsModuleCompiler &m, PropertyName varName, ParseNode *newExpr )
    {TRACE_IT(45681);
        Assert( newExpr->nop == knopNew );
        ParseNode *ctorExpr = newExpr->sxCall.pnodeTarget;
        ArrayBufferView::ViewType type;
        if( ParserWrapper::IsDotMember(ctorExpr) )
        {TRACE_IT(45682);
            ParseNode *base = ParserWrapper::DotBase(ctorExpr);

            PropertyName globalName = m.GetStdLibArgName();
            if (!globalName)
            {TRACE_IT(45683);
                return m.Fail(base, _u("cannot create array view without an asm.js global parameter"));
            }

            if (!ParserWrapper::IsNameDeclaration(base) || base->name() != globalName)
            {TRACE_IT(45684);
                return m.FailName(base, _u("expecting '%s.*Array"), globalName);
            }
            PropertyName fieldName = ParserWrapper::DotMember(ctorExpr);
            if (!fieldName)
            {TRACE_IT(45685);
                return m.FailName(ctorExpr, _u("Failed to define array view to var %s"), varName);
            }
            PropertyId field = fieldName->GetPropertyId();

            switch (field)
            {
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
        {TRACE_IT(45686);
            AsmJsSymbol * buffFunc = m.LookupIdentifier(ctorExpr->name());

            if (!buffFunc || buffFunc->GetSymbolType() != AsmJsSymbol::TypedArrayBuiltinFunction)
            {TRACE_IT(45687);
                return m.Fail(ctorExpr, _u("invalid 'new' import"));
            }
            type = buffFunc->Cast<AsmJsTypedArrayFunction>()->GetViewType();
            if (type == ArrayBufferView::TYPE_COUNT)
            {TRACE_IT(45688);
                return m.Fail(ctorExpr, _u("could not match typed array name"));
            }
        }
        else
        {TRACE_IT(45689);
            return m.Fail(newExpr, _u("invalid 'new' import"));
        }

        ParseNode *bufArg = newExpr->sxCall.pnodeArgs;
        if( !bufArg || !ParserWrapper::IsNameDeclaration( bufArg ) )
        {TRACE_IT(45690);
            return m.Fail( ctorExpr, _u("array view constructor takes exactly one argument") );
        }

        PropertyName bufferName = m.GetBufferArgName();
        if( !bufferName )
        {TRACE_IT(45691);
            return m.Fail( bufArg, _u("cannot create array view without an asm.js heap parameter") );
        }

        if( bufferName != bufArg->name() )
        {TRACE_IT(45692);
            return m.FailName( bufArg, _u("argument to array view constructor must be '%s'"), bufferName );
        }


        if( !m.AddArrayView( varName, type ) )
        {TRACE_IT(45693);
            return m.FailName( ctorExpr, _u("Failed to define array view to var %s"), varName );
        }
        return true;
    }

    bool
    AsmJSCompiler::CheckGlobalDotImport(AsmJsModuleCompiler &m, PropertyName varName, ParseNode *initNode)
    {TRACE_IT(45694);
        ParseNode *base = ParserWrapper::DotBase(initNode);
        PropertyName field = ParserWrapper::DotMember(initNode);
        if( !field )
        {TRACE_IT(45695);
            return m.Fail( initNode, _u("Global import must be in the form c.x where c is stdlib or foreign and x is a string literal") );
        }
        m.GetByteCodeGenerator()->AssignPropertyId(field);
        PropertyName lib = nullptr;
        if (ParserWrapper::IsDotMember(base))
        {TRACE_IT(45696);
            lib = ParserWrapper::DotMember(base);
            base = ParserWrapper::DotBase(base);

            if (m.GetScriptContext()->GetConfig()->IsSimdjsEnabled())
            {TRACE_IT(45697);
                if (!lib || (lib->GetPropertyId() != PropertyIds::Math && lib->GetPropertyId() != PropertyIds::SIMD))
                {TRACE_IT(45698);
                    return m.FailName(initNode, _u("'%s' should be Math or SIMD, as in global.Math.xxxx"), field);
                }
            }
            else
            {TRACE_IT(45699);
                if (!lib || lib->GetPropertyId() != PropertyIds::Math)
                {TRACE_IT(45700);
                    return m.FailName(initNode, _u("'%s' should be Math, as in global.Math.xxxx"), field);
                }
            }
        }

        if( ParserWrapper::IsNameDeclaration(base) && base->name() == m.GetStdLibArgName() )
        {TRACE_IT(45701);

            if (m.GetScriptContext()->GetConfig()->IsSimdjsEnabled())
            {TRACE_IT(45702);
                if (lib && lib->GetPropertyId() == PropertyIds::SIMD)
                {TRACE_IT(45703);
                    // global.SIMD.xxx
                    AsmJsSIMDFunction *simdFunc;

                    if (!m.LookupStdLibSIMDName(field->GetPropertyId(), field, &simdFunc))
                    {TRACE_IT(45704);
                        return m.FailName(initNode, _u("'%s' is not standard SIMD builtin"), varName);
                    }

                    if (simdFunc->GetName() != nullptr)
                    {TRACE_IT(45705);
                        OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, _u("Warning: SIMD Builtin already defined for var %s"), simdFunc->GetName()->Psz());
                    }
                    simdFunc->SetName(varName);
                    if (!m.DefineIdentifier(varName, simdFunc))
                    {TRACE_IT(45706);
                        return m.FailName(initNode, _u("Failed to define SIMD builtin function to var %s"), varName);
                    }
                    m.AddSimdBuiltinUse(simdFunc->GetSimdBuiltInFunction());
                    return true;
                }
            }

            // global.Math.xxx
            MathBuiltin mathBuiltin;
            if (m.LookupStandardLibraryMathName(field, &mathBuiltin))
            {TRACE_IT(45707);
                switch (mathBuiltin.kind)
                {
                case MathBuiltin::Function:{TRACE_IT(45708);
                    auto func = mathBuiltin.u.func;
                    if (func->GetName() != nullptr)
                    {TRACE_IT(45709);
                        OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, _u("Warning: Math Builtin already defined for var %s"), func->GetName()->Psz());
                    }
                    func->SetName(varName);
                    if (!m.DefineIdentifier(varName, func))
                    {TRACE_IT(45710);
                        return m.FailName(initNode, _u("Failed to define math builtin function to var %s"), varName);
                    }
                    m.AddMathBuiltinUse(func->GetMathBuiltInFunction());
                }
                break;
                case MathBuiltin::Constant:
                    if (!m.AddNumericConst(varName, mathBuiltin.u.cst))
                    {TRACE_IT(45711);
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
            {TRACE_IT(45712);
                if (arrayBuiltin.mFunc->GetName() != nullptr)
                {TRACE_IT(45713);
                    OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, _u("Warning: Typed array builtin already defined for var %s"), arrayBuiltin.mFunc->GetName()->Psz());
                }
                arrayBuiltin.mFunc->SetName(varName);
                if (!m.DefineIdentifier(varName, arrayBuiltin.mFunc))
                {TRACE_IT(45714);
                    return m.FailName(initNode, _u("Failed to define typed array builtin function to var %s"), varName);
                }
                m.AddArrayBuiltinUse(arrayBuiltin.mFunc->GetArrayBuiltInFunction());
                return true;
            }

            return m.FailName(initNode, _u("'%s' is not a standard Math builtin"), field);
        }
        else if( ParserWrapper::IsNameDeclaration(base) && base->name() == m.GetForeignArgName() )
        {TRACE_IT(45715);
            // foreign import
            return m.AddModuleFunctionImport( varName, field );
        }
        else if (ParserWrapper::IsNameDeclaration(base))
        {TRACE_IT(45716);
            // Check if SIMD function import
            // e.g. var x = f4.add
            AsmJsSIMDFunction *simdFunc, *operation;

            simdFunc = m.LookupSimdConstructor(base->name());
            if (simdFunc == nullptr || !m.LookupStdLibSIMDName(simdFunc->GetSimdBuiltInFunction(), field, &operation))
            {TRACE_IT(45717);
                return m.FailName(initNode, _u("Invalid dot expression import. %s is not a standard SIMD operation"), varName);
            }

            if (operation->GetName() != nullptr)
            {TRACE_IT(45718);
                OutputMessage(m.GetScriptContext(), DEIT_ASMJS_FAILED, _u("Warning: SIMD Builtin already defined for var %s"), operation->GetName()->Psz());
            }

            // bind operation to var
            operation->SetName(varName);
            if (!m.DefineIdentifier(varName, operation))
            {TRACE_IT(45719);
                return m.FailName(initNode, _u("Failed to define SIMD builtin function to var %s"), varName);
            }

            m.AddSimdBuiltinUse(operation->GetSimdBuiltInFunction());
            return true;
        }

        return m.Fail(initNode, _u("expecting c.y where c is either the global or foreign parameter"));
    }

    bool
    AsmJSCompiler::CheckModuleGlobal(AsmJsModuleCompiler &m, ParseNode *var)
    {TRACE_IT(45720);
        Assert(var->nop == knopVarDecl || var->nop == knopConstDecl);

        bool isMutable = var->nop != knopConstDecl;
        PropertyName name = var->name();

        m.GetByteCodeGenerator()->AssignPropertyId(name);
        if (m.LookupIdentifier(name))
        {TRACE_IT(45721);
            return m.FailName(var, _u("import variable %s names must be unique"), name);
        }

        if (!CheckModuleLevelName(m, var, name))
        {TRACE_IT(45722);
            return false;
        }

        if (!var->sxVar.pnodeInit)
        {TRACE_IT(45723);
            return m.Fail(var, _u("module import needs initializer"));
        }

        ParseNode *initNode = var->sxVar.pnodeInit;


        if( ParserWrapper::IsNumericLiteral( initNode ) )
        {TRACE_IT(45724);
            if (m.AddNumericVar(name, initNode, false, isMutable))
            {TRACE_IT(45725);
                return true;
            }
            else
            {TRACE_IT(45726);
                return m.FailName(var, _u("Failed to declare numeric var %s"), name);
            }
        }


        if (initNode->nop == knopOr || initNode->nop == knopPos || initNode->nop == knopCall)
        {TRACE_IT(45727);
            // SIMD_JS
            // e.g. var x = f4(1.0, 2.0, 3.0, 4.0)
            if (initNode->nop == knopCall)
            {TRACE_IT(45728);
                AsmJsSIMDFunction* simdSym;
                // also checks if simd constructor
                simdSym = m.LookupSimdConstructor(initNode->sxCall.pnodeTarget->name());
                // call to simd constructor
                if (simdSym)
                {TRACE_IT(45729);
                    // validate args and define a SIMD symbol
                    return m.AddSimdValueVar(name, initNode, simdSym);
                }
                // else it is FFI import: var x = f4check(FFI.field), handled in CheckGlobalVariableInitImport
            }

           return CheckGlobalVariableInitImport(m, name, initNode, isMutable );
        }

        if( initNode->nop == knopNew )
        {TRACE_IT(45730);
           return CheckNewArrayView(m, var->name(), initNode);
        }

        if (ParserWrapper::IsDotMember(initNode))
        {TRACE_IT(45731);
            return CheckGlobalDotImport(m, name, initNode);
        }


        return m.Fail( initNode, _u("Failed to recognize global variable") );
    }

    bool
    AsmJSCompiler::CheckModuleGlobals(AsmJsModuleCompiler &m)
    {TRACE_IT(45732);
        ParseNode *varStmts;
        if( !ParserWrapper::ParseVarOrConstStatement( m.GetCurrentParserNode(), &varStmts ) )
        {TRACE_IT(45733);
            return false;
        }

        if (!varStmts)
        {TRACE_IT(45734);
            return true;
        }
        while (varStmts->nop == knopList)
        {TRACE_IT(45735);
            ParseNode * pnode = ParserWrapper::GetBinaryLeft(varStmts);
            while (pnode && pnode->nop != knopEndCode)
            {TRACE_IT(45736);
                ParseNode * decl;
                if (pnode->nop == knopList)
                {TRACE_IT(45737);
                    decl = ParserWrapper::GetBinaryLeft(pnode);
                    pnode = ParserWrapper::GetBinaryRight(pnode);
                }
                else
                {TRACE_IT(45738);
                    decl = pnode;
                    pnode = nullptr;
                }

                if (decl->nop == knopFncDecl)
                {TRACE_IT(45739);
                    goto varDeclEnd;
                }
                else if (decl->nop != knopConstDecl && decl->nop != knopVarDecl)
                {TRACE_IT(45740);
                    break;
                }

                if (decl->sxVar.pnodeInit && decl->sxVar.pnodeInit->nop == knopArray)
                {TRACE_IT(45741);
                    // Assume we reached func tables
                    goto varDeclEnd;
                }

                if (!CheckModuleGlobal(m, decl))
                {TRACE_IT(45742);
                    return false;
                }
            }

            if (ParserWrapper::GetBinaryRight(varStmts)->nop == knopEndCode)
            {TRACE_IT(45743);
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
        {TRACE_IT(45744);
            fnNodes = ParserWrapper::GetBinaryRight(fnNodes);
        }

        if (fnNodes->nop == knopEndCode)
        {TRACE_IT(45745);
            // if there are no function tables, we can just initialize count to 0
            m.SetFuncPtrTableCount(0);
        }
        else
        {TRACE_IT(45746);
            m.SetCurrentParseNode(fnNodes);
            if (!CheckFunctionTables(m))
            {TRACE_IT(45747);
                return false;
            }
        }
        // this will move us back to the beginning of the function declarations
        m.SetCurrentParseNode(varStmts);
        return true;
    }


    bool AsmJSCompiler::CheckFunction( AsmJsModuleCompiler &m, ParseNode* fncNode )
    {TRACE_IT(45748);
        Assert( fncNode->nop == knopFncDecl );

        if( PHASE_TRACE1( Js::ByteCodePhase ) )
        {TRACE_IT(45749);
            Output::Print( _u("  Checking Asm function: %s\n"), fncNode->sxFnc.funcInfo->name);
        }

        if( !CheckFunctionHead( m, fncNode, false ) )
        {TRACE_IT(45750);
            return false;
        }

        AsmJsFunc* func = m.CreateNewFunctionEntry(fncNode);
        if (!func)
        {TRACE_IT(45751);
            return m.Fail(fncNode, _u("      Error creating function entry"));
        }
        return true;
    }

    bool AsmJSCompiler::CheckFunctionsSequential( AsmJsModuleCompiler &m )
    {TRACE_IT(45752);
        AsmJSParser& list = m.GetCurrentParserNode();
        Assert( list->nop == knopList );


        ParseNode* pnode = ParserWrapper::GetBinaryLeft(list);

        while (pnode->nop == knopFncDecl)
        {
            if( !CheckFunction( m, pnode ) )
            {TRACE_IT(45753);
                return false;
            }

            if(ParserWrapper::GetBinaryRight(list)->nop == knopEndCode)
            {TRACE_IT(45754);
                break;
            }
            list = ParserWrapper::GetBinaryRight(list);
            pnode = ParserWrapper::GetBinaryLeft(list);
        }

        m.SetCurrentParseNode( list );

        return true;
    }

    bool AsmJSCompiler::CheckFunctionTables(AsmJsModuleCompiler &m)
    {TRACE_IT(45755);
        AsmJSParser& list = m.GetCurrentParserNode();
        Assert(list->nop == knopList);

        int32 funcPtrTableCount = 0;
        while (list->nop != knopEndCode)
        {TRACE_IT(45756);
            ParseNode * varStmt = ParserWrapper::GetBinaryLeft(list);
            if (varStmt->nop != knopConstDecl && varStmt->nop != knopVarDecl)
            {TRACE_IT(45757);
                break;
            }
            if (!varStmt->sxVar.pnodeInit || varStmt->sxVar.pnodeInit->nop != knopArray)
            {TRACE_IT(45758);
                break;
            }
            const uint tableSize = varStmt->sxVar.pnodeInit->sxArrLit.count;
            if (!::Math::IsPow2(tableSize))
            {TRACE_IT(45759);
                return m.FailName(varStmt, _u("Function table [%s] size must be a power of 2"), varStmt->name());
            }
            if (!m.AddFunctionTable(varStmt->name(), tableSize))
            {TRACE_IT(45760);
                return m.FailName(varStmt, _u("Unable to create new function table %s"), varStmt->name());
            }

            AsmJsFunctionTable* ftable = (AsmJsFunctionTable*)m.LookupIdentifier(varStmt->name());
            Assert(ftable);
            ParseNode* pnode = varStmt->sxVar.pnodeInit->sxArrLit.pnode1;
            if (pnode->nop == knopList)
            {TRACE_IT(45761);
                pnode = ParserWrapper::GetBinaryLeft(pnode);
            }
            if (!ParserWrapper::IsNameDeclaration(pnode))
            {TRACE_IT(45762);
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
    {TRACE_IT(45763);
        ParseNode* endStmt = m.GetCurrentParserNode();

        if (endStmt->nop != knopList)
        {TRACE_IT(45764);
            return m.Fail(endStmt, _u("Module must have a return"));
        }

        ParseNode* node = ParserWrapper::GetBinaryLeft( endStmt );
        ParseNode* endNode = ParserWrapper::GetBinaryRight( endStmt );

        if( node->nop != knopReturn || endNode->nop != knopEndCode )
        {TRACE_IT(45765);
            return m.Fail( node, _u("Only expression after table functions must be a return") );
        }

        ParseNode* objNode = node->sxReturn.pnodeExpr;
        if ( !objNode )
        {TRACE_IT(45766);
            return m.Fail( node, _u( "Module return must be an object or 1 function" ) );
        }

        if( objNode->nop != knopObject )
        {TRACE_IT(45767);
            if( ParserWrapper::IsNameDeclaration( objNode ) )
            {TRACE_IT(45768);
                PropertyName name = objNode->name();
                AsmJsSymbol* sym = m.LookupIdentifier( name );
                if( !sym )
                {TRACE_IT(45769);
                    return m.FailName( node, _u("Symbol %s not recognized inside module"), name );
                }

                if( sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                {TRACE_IT(45770);
                    return m.FailName( node, _u("Symbol %s can only be a function of the module"), name );
                }

                AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                if( !m.SetExportFunc( func ) )
                {TRACE_IT(45771);
                    return m.FailName( node, _u("Error adding return Symbol %s"), name );
                }
                return true;
            }
            return m.Fail( node, _u("Module return must be an object or 1 function") );
        }

        ParseNode* objectElement = ParserWrapper::GetUnaryNode(objNode);
        if (!objectElement)
        {TRACE_IT(45772);
            return m.Fail(node, _u("Return object must not be empty"));
        }
        while( objectElement )
        {TRACE_IT(45773);
            ParseNode* member = nullptr;
            if( objectElement->nop == knopList )
            {TRACE_IT(45774);
                member = ParserWrapper::GetBinaryLeft( objectElement );
                objectElement = ParserWrapper::GetBinaryRight( objectElement );
            }
            else if( objectElement->nop == knopMember )
            {TRACE_IT(45775);
                member = objectElement;
                objectElement = nullptr;
            }
            else
            {TRACE_IT(45776);
                return m.Fail( node, _u("Return object must only contain members") );
            }

            if( member )
            {TRACE_IT(45777);
                ParseNode* field = ParserWrapper::GetBinaryLeft( member );
                ParseNode* value = ParserWrapper::GetBinaryRight( member );
                if( !ParserWrapper::IsNameDeclaration( field ) || !ParserWrapper::IsNameDeclaration( value ) )
                {TRACE_IT(45778);
                    return m.Fail( node, _u("Return object member must be fields") );
                }

                AsmJsSymbol* sym = m.LookupIdentifier( value->name() );
                if( !sym )
                {TRACE_IT(45779);
                    return m.FailName( node, _u("Symbol %s not recognized inside module"), value->name() );
                }

                if( sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                {TRACE_IT(45780);
                    return m.FailName( node, _u("Symbol %s can only be a function of the module"), value->name() );
                }

                AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                if( !m.AddExport( field->name(), func->GetFunctionIndex() ) )
                {TRACE_IT(45781);
                    return m.FailName( node, _u("Error adding return Symbol %s"), value->name() );
                }
            }
        }

        return true;
    }

    bool AsmJSCompiler::CheckFuncPtrTables( AsmJsModuleCompiler &m )
    {TRACE_IT(45782);
        ParseNode *list = m.GetCurrentParserNode();
        if (!list)
        {TRACE_IT(45783);
            return true;
        }
        while (list->nop != knopEndCode)
        {TRACE_IT(45784);
            ParseNode * varStmt = ParserWrapper::GetBinaryLeft(list);
            if (varStmt->nop != knopConstDecl && varStmt->nop != knopVarDecl)
            {TRACE_IT(45785);
                break;
            }

            ParseNode* nodeInit = varStmt->sxVar.pnodeInit;
            if( !nodeInit || nodeInit->nop != knopArray )
            {TRACE_IT(45786);
                return m.Fail( varStmt, _u("Invalid variable after function declaration") );
            }

            PropertyName tableName = varStmt->name();

            AsmJsSymbol* symFunctionTable = m.LookupIdentifier(tableName);
            if( !symFunctionTable)
            {TRACE_IT(45787);
                // func table not used in functions disregard it
            }
            else
            {TRACE_IT(45788);
                //Check name
                if(symFunctionTable->GetSymbolType() != AsmJsSymbol::FuncPtrTable )
                {TRACE_IT(45789);
                    return m.FailName( varStmt, _u("Variable %s is already defined"), tableName );
                }

                AsmJsFunctionTable* table = symFunctionTable->Cast<AsmJsFunctionTable>();
                if( table->IsDefined() )
                {TRACE_IT(45790);
                    return m.FailName( varStmt, _u("Multiple declaration of function table %s"), tableName );
                }

                // Check content of the array
                uint count = nodeInit->sxArrLit.count;
                if( table->GetSize() != count )
                {TRACE_IT(45791);
                    return m.FailName( varStmt, _u("Invalid size of function table %s"), tableName );
                }

                // Set the content of the array in the table
                ParseNode* node = nodeInit->sxArrLit.pnode1;
                uint i = 0;
                while( node )
                {TRACE_IT(45792);
                    ParseNode* funcNameNode = nullptr;
                    if( node->nop == knopList )
                    {TRACE_IT(45793);
                        funcNameNode = ParserWrapper::GetBinaryLeft( node );
                        node = ParserWrapper::GetBinaryRight( node );
                    }
                    else
                    {TRACE_IT(45794);
                        Assert( i + 1 == count );
                        funcNameNode = node;
                        node = nullptr;
                    }

                    if( ParserWrapper::IsNameDeclaration( funcNameNode ) )
                    {TRACE_IT(45795);
                        AsmJsSymbol* sym = m.LookupIdentifier( funcNameNode->name() );
                        if( !sym || sym->GetSymbolType() != AsmJsSymbol::ModuleFunction )
                        {TRACE_IT(45796);
                            return m.FailName( varStmt, _u("Element in function table %s is not a function"), tableName );
                        }
                        AsmJsFunc* func = sym->Cast<AsmJsFunc>();
                        AsmJsRetType retType;
                        if (!table->SupportsArgCall(func->GetArgCount(), func->GetArgTypeArray(), retType))
                        {TRACE_IT(45797);
                            return m.FailName(funcNameNode, _u("Function signatures in table %s do not match"), tableName);
                        }
                        if (!table->CheckAndSetReturnType(func->GetReturnType()))
                        {TRACE_IT(45798);
                            return m.FailName(funcNameNode, _u("Function return types in table %s do not match"), tableName);
                        }
                        table->SetModuleFunctionIndex( func->GetFunctionIndex(), i );
                        ++i;
                    }
                    else
                    {TRACE_IT(45799);
                        return m.FailName(funcNameNode, _u("Element in function table %s is not a function name"), tableName);
                    }
                }

                table->Define();
            }

            list = ParserWrapper::GetBinaryRight(list);
        }

        if( !m.AreAllFuncTableDefined() )
        {TRACE_IT(45800);
            return m.Fail(list, _u("Some function table were used but not defined"));
        }

        m.SetCurrentParseNode(list);
        return true;
    }

    bool AsmJSCompiler::CheckModule( ExclusiveContext *cx, AsmJSParser &parser, ParseNode *stmtList )
    {TRACE_IT(45801);
        AsmJsModuleCompiler m( cx, parser );
        if( !m.Init() )
        {TRACE_IT(45802);
            return false;
        }
        if( PropertyName moduleFunctionName = ParserWrapper::FunctionName( m.GetModuleFunctionNode() ) )
        {
            if( !CheckModuleLevelName( m, m.GetModuleFunctionNode(), moduleFunctionName ) )
            {TRACE_IT(45803);
                return false;
            }
            m.InitModuleName( moduleFunctionName );

            if( PHASE_TRACE1( Js::ByteCodePhase ) )
            {TRACE_IT(45804);
                Output::Print( _u("Asm.Js Module [%s] detected, trying to compile\n"), moduleFunctionName->Psz() );
            }
        }

        m.AccumulateCompileTime(AsmJsCompilation::Module);

        if( !CheckFunctionHead( m, m.GetModuleFunctionNode() ) )
        {TRACE_IT(45805);
            goto AsmJsCompilationError;
        }

        if (!CheckModuleArguments(m, m.GetModuleFunctionNode()))
        {TRACE_IT(45806);
            goto AsmJsCompilationError;
        }

        if (!CheckModuleGlobals(m))
        {TRACE_IT(45807);
            goto AsmJsCompilationError;
        }

        m.AccumulateCompileTime(AsmJsCompilation::Module);

        if (!CheckFunctionsSequential(m))
        {TRACE_IT(45808);
            goto AsmJsCompilationError;
        }

        m.AccumulateCompileTime();
        m.InitMemoryOffsets();

        if( !m.CompileAllFunctions() )
        {TRACE_IT(45809);
            return false;
        }

        m.AccumulateCompileTime(AsmJsCompilation::ByteCode);

        if (!CheckFuncPtrTables(m))
        {TRACE_IT(45810);
            m.RevertAllFunctions();
            return false;
        }

        m.AccumulateCompileTime();

        if (!CheckModuleReturn(m))
        {TRACE_IT(45811);
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
        {TRACE_IT(45812);
            FunctionBody* body = moduleNode->sxFnc.funcInfo->GetParsedFunctionBody();
            body->ResetByteCodeGenState();
        }

        cx->byteCodeGenerator->Writer()->Reset();
        return false;
    }

    bool AsmJSCompiler::Compile(ExclusiveContext *cx, AsmJSParser parser, ParseNode *stmtList)
    {
        if (!CheckModule(cx, parser, stmtList))
        {TRACE_IT(45813);
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
    {TRACE_IT(45814);
        char16 buf[2048];
        size_t size;

        size = _vsnwprintf_s(buf, _countof(buf), _TRUNCATE, message, argptr);
        if (size == -1)
        {TRACE_IT(45815);
            size = 2048;
        }
        scriptContext->RaiseMessageToDebugger(messageType, buf, scriptContext->GetUrl());
        if (PHASE_TRACE1(AsmjsPhase) || PHASE_TESTTRACE1(AsmjsPhase))
        {TRACE_IT(45816);
            Output::PrintBuffer(buf, size);
            Output::Print(_u("\n"));
            Output::Flush();
        }
    }
}

#endif
