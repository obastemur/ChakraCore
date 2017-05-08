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
namespace Js
{

    bool ParserWrapper::ParseVarOrConstStatement( AsmJSParser &parser, ParseNode **var )
    {TRACE_IT(47254);
        Assert( parser );
        *var = nullptr;
        ParseNode *body = parser->sxFnc.pnodeBody;
        if( body )
        {TRACE_IT(47255);
            ParseNode* lhs = GetBinaryLeft( body );
            ParseNode* rhs = GetBinaryRight( body );
            if( rhs && rhs->nop == knopList )
            {TRACE_IT(47256);
                AssertMsg( lhs->nop == knopStr, "this should be use asm" );
                *var = rhs;
                return true;
            }
        }
        return false;
    }

    bool ParserWrapper::IsDefinition( ParseNode *arg )
    {TRACE_IT(47257);
        //TODO, eliminate duplicates
        return true;
    }



    ParseNode* ParserWrapper::NextInList( ParseNode *node )
    {TRACE_IT(47258);
        Assert( node->nop == knopList );
        return node->sxBin.pnode2;
    }

    ParseNode* ParserWrapper::NextVar( ParseNode *node )
    {TRACE_IT(47259);
        return node->sxVar.pnodeNext;
    }

    ParseNode* ParserWrapper::FunctionArgsList( ParseNode *node, ArgSlot &numformals )
    {TRACE_IT(47260);
        Assert( node->nop == knopFncDecl );
        PnFnc func = node->sxFnc;
        ParseNode* first = func.pnodeParams;
        // throws OOM on uint16 overflow
        for( ParseNode* pnode = first; pnode; pnode = pnode->sxVar.pnodeNext, UInt16Math::Inc(numformals));
        return first;
    }

    PropertyName ParserWrapper::VariableName( ParseNode *node )
    {TRACE_IT(47261);
        return node->name();
    }

    PropertyName ParserWrapper::FunctionName( ParseNode *node )
    {TRACE_IT(47262);
        if( node->nop == knopFncDecl )
        {TRACE_IT(47263);
            PnFnc function = node->sxFnc;
            if( function.pnodeName && function.pnodeName->nop == knopVarDecl )
            {TRACE_IT(47264);
                return function.pnodeName->sxVar.pid;
            }
        }
        return nullptr;
    }

    ParseNode * ParserWrapper::GetVarDeclList( ParseNode * pnode )
    {TRACE_IT(47265);
        ParseNode* varNode = pnode;
        while (varNode->nop == knopList)
        {TRACE_IT(47266);
            ParseNode * var = GetBinaryLeft(varNode);
            if (var->nop == knopVarDecl)
            {TRACE_IT(47267);
                return var;
            }
            else if (var->nop == knopList)
            {TRACE_IT(47268);
                var = GetBinaryLeft(var);
                if (var->nop == knopVarDecl)
                {TRACE_IT(47269);
                    return var;
                }
            }
            varNode = GetBinaryRight(varNode);
        }
        return nullptr;
    }

    void ParserWrapper::ReachEndVarDeclList( ParseNode** outNode )
    {TRACE_IT(47270);
        ParseNode* pnode = *outNode;
        // moving down to the last var declaration
        while( pnode->nop == knopList )
        {TRACE_IT(47271);
            ParseNode* var = GetBinaryLeft( pnode );
            if (var->nop == knopVarDecl)
            {TRACE_IT(47272);
                pnode = GetBinaryRight( pnode );
                continue;
            }
            else if (var->nop == knopList)
            {TRACE_IT(47273);
                var = GetBinaryLeft( var );
                if (var->nop == knopVarDecl)
                {TRACE_IT(47274);
                    pnode = GetBinaryRight( pnode );
                    continue;
                }
            }
            break;
        }
        *outNode = pnode;
    }

    AsmJsCompilationException::AsmJsCompilationException( const char16* _msg, ... )
    {
        va_list arglist;
        va_start( arglist, _msg );
        vswprintf_s( msg_, _msg, arglist );
    }

    Var AsmJsChangeHeapBuffer(RecyclableObject * function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count < 1 || !ArrayBuffer::Is(args[1]))
        {TRACE_IT(47275);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedArrayBufferObject);
        }


        ArrayBuffer* newArrayBuffer = ArrayBuffer::FromVar(args[1]);
        if (newArrayBuffer->IsDetached() || newArrayBuffer->GetByteLength() & 0xffffff || newArrayBuffer->GetByteLength() <= 0xffffff || newArrayBuffer->GetByteLength() > 0x80000000)
        {TRACE_IT(47276);
            return JavascriptBoolean::ToVar(FALSE, scriptContext);
        }
        FrameDisplay* frame = ((ScriptFunction*)function)->GetEnvironment();
        Field(Var)* moduleArrayBuffer = (Field(Var)*)frame->GetItem(0) + AsmJsModuleMemory::MemoryTableBeginOffset;
        *moduleArrayBuffer = newArrayBuffer;
        return JavascriptBoolean::ToVar(TRUE, scriptContext);
    }

#if ENABLE_DEBUG_CONFIG_OPTIONS
    int64 ConvertStringToInt64(Var string, ScriptContext* scriptContext)
    {TRACE_IT(47277);
        JavascriptString* str = JavascriptString::FromVar(string);
        charcount_t length = str->GetLength();
        const char16* buf = str->GetString();
        int radix = 10;
        if (length >= 2 && buf[0] == '0' && buf[1] == 'x')
        {TRACE_IT(47278);
            radix = 16;
        }
        return (int64)_wcstoui64(buf, nullptr, radix);
    }

    Var CreateI64ReturnObject(int64 val, ScriptContext* scriptContext)
    {TRACE_IT(47279);
        Js::Var i64Object = JavascriptOperators::NewJavascriptObjectNoArg(scriptContext);
        Var low = JavascriptNumber::ToVar((uint)val, scriptContext);
        Var high = JavascriptNumber::ToVar(val >> 32, scriptContext);

        PropertyRecord const * lowPropRecord = nullptr;
        PropertyRecord const * highPropRecord = nullptr;
        scriptContext->GetOrAddPropertyRecord(_u("low"), (int)wcslen(_u("low")), &lowPropRecord);
        scriptContext->GetOrAddPropertyRecord(_u("high"), (int)wcslen(_u("high")), &highPropRecord);
        JavascriptOperators::OP_SetProperty(i64Object, lowPropRecord->GetPropertyId(), low, scriptContext);
        JavascriptOperators::OP_SetProperty(i64Object, highPropRecord->GetPropertyId(), high, scriptContext);
        return i64Object;
    }
#endif

    void * UnboxAsmJsArguments(ScriptFunction* func, Var * origArgs, char * argDst, CallInfo callInfo)
    {TRACE_IT(47280);
        void * address = reinterpret_cast<void*>(func->GetEntryPointInfo()->jsMethod);
        Assert(address);
        AsmJsFunctionInfo* info = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        ScriptContext* scriptContext = func->GetScriptContext();

#if ENABLE_DEBUG_CONFIG_OPTIONS
        bool allowTestInputs = CONFIG_FLAG(WasmI64);
#endif

        AsmJsModuleInfo::EnsureHeapAttached(func);

        ArgumentReader reader(&callInfo, origArgs);
        uint actualArgCount = reader.Info.Count - 1; // -1 for ScriptFunction
        argDst = argDst + MachPtr; // add one first so as to skip the ScriptFunction argument
        for (ArgSlot i = 0; i < info->GetArgCount(); i++)
        {TRACE_IT(47281);

            if (info->GetArgType(i).isInt())
            {TRACE_IT(47282);
                int32 intVal;
                if (i < actualArgCount)
                {TRACE_IT(47283);
#if ENABLE_DEBUG_CONFIG_OPTIONS
                    if (allowTestInputs && JavascriptString::Is(*origArgs))
                    {TRACE_IT(47284);
                        intVal = (int32)ConvertStringToInt64(*origArgs, scriptContext);
                    }
                    else
#endif
                        intVal = JavascriptMath::ToInt32(*origArgs, scriptContext);
                }
                else
                {TRACE_IT(47285);
                    intVal = 0;
                }

#if TARGET_64
                *(int64*)(argDst) = 0;
#endif
                *(int32*)argDst = intVal;
                argDst = argDst + MachPtr;
            }
            else if (info->GetArgType(i).isInt64())
            {TRACE_IT(47286);
#if ENABLE_DEBUG_CONFIG_OPTIONS
                if (!allowTestInputs)
#endif
                {TRACE_IT(47287);
                    JavascriptError::ThrowTypeError(scriptContext, WASMERR_InvalidTypeConversion);
                }

#if ENABLE_DEBUG_CONFIG_OPTIONS
                int64 val;
                if (i < actualArgCount)
                {TRACE_IT(47288);
                    if (JavascriptString::Is(*origArgs))
                    {TRACE_IT(47289);
                        val = ConvertStringToInt64(*origArgs, scriptContext);
                    }
                    else if (JavascriptObject::Is(*origArgs))
                    {TRACE_IT(47290);
                        RecyclableObject* object = RecyclableObject::FromVar(*origArgs);
                        PropertyRecord const * lowPropRecord = nullptr;
                        PropertyRecord const * highPropRecord = nullptr;
                        scriptContext->GetOrAddPropertyRecord(_u("low"), (int)wcslen(_u("low")), &lowPropRecord);
                        scriptContext->GetOrAddPropertyRecord(_u("high"), (int)wcslen(_u("high")), &highPropRecord);
                        Var low = JavascriptOperators::OP_GetProperty(object, lowPropRecord->GetPropertyId(), scriptContext);
                        Var high = JavascriptOperators::OP_GetProperty(object, highPropRecord->GetPropertyId(), scriptContext);

                        uint64 lowVal = JavascriptMath::ToInt32(low, scriptContext);
                        uint64 highVal = JavascriptMath::ToInt32(high, scriptContext);
                        val = (highVal << 32) | (lowVal & 0xFFFFFFFF);
                    }
                    else
                    {TRACE_IT(47291);
                        int32 intVal = JavascriptMath::ToInt32(*origArgs, scriptContext);
                        val = (int64)intVal;
                    }
                }
                else
                {TRACE_IT(47292);
                    val = 0;
                }

                *(int64*)(argDst) = val;
                argDst += sizeof(int64);
#endif
            }
            else if (info->GetArgType(i).isFloat())
            {TRACE_IT(47293);
                float floatVal;
                if (i < actualArgCount)
                {TRACE_IT(47294);
#if ENABLE_DEBUG_CONFIG_OPTIONS
                    if (allowTestInputs && JavascriptString::Is(*origArgs))
                    {TRACE_IT(47295);
                        int32 val = (int32)ConvertStringToInt64(*origArgs, scriptContext);
                        floatVal = *(float*)&val;
                    }
                    else
#endif
                        floatVal = (float)(JavascriptConversion::ToNumber(*origArgs, scriptContext));
                }
                else
                {TRACE_IT(47296);
                    floatVal = (float)(JavascriptNumber::NaN);
                }
#if TARGET_64
                *(int64*)(argDst) = 0;
#endif
                *(float*)argDst = floatVal;
                argDst = argDst + MachPtr;
            }
            else if (info->GetArgType(i).isDouble())
            {TRACE_IT(47297);
                double doubleVal;
                if (i < actualArgCount)
                {TRACE_IT(47298);
#if ENABLE_DEBUG_CONFIG_OPTIONS
                    if (allowTestInputs && JavascriptString::Is(*origArgs))
                    {TRACE_IT(47299);
                        int64 val = ConvertStringToInt64(*origArgs, scriptContext);
                        doubleVal = *(double*)&val;
                    }
                    else
#endif
                        doubleVal = JavascriptConversion::ToNumber(*origArgs, scriptContext);
                }
                else
                {TRACE_IT(47300);
                    doubleVal = JavascriptNumber::NaN;
                }

                *(double*)argDst = doubleVal;
                argDst = argDst + sizeof(double);
            }
            else if (info->GetArgType(i).isSIMD())
            {TRACE_IT(47301);
                AsmJsVarType argType = info->GetArgType(i);
                AsmJsSIMDValue simdVal = {0, 0, 0, 0};
                // SIMD values are copied unaligned.
                // SIMD values cannot be implicitly coerced from/to other types. If the SIMD parameter is missing (i.e. Undefined), we throw type error since there is not equivalent SIMD value to coerce to.
                switch (argType.which())
                {
                case AsmJsType::Int32x4:
                    if (!JavascriptSIMDInt32x4::Is(*origArgs))
                    {TRACE_IT(47302);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt32x4TypeMismatch, _u("Int32x4"));
                    }
                    simdVal = ((JavascriptSIMDInt32x4*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Bool32x4:
                    if (!JavascriptSIMDBool32x4::Is(*origArgs))
                    {TRACE_IT(47303);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdBool32x4TypeMismatch, _u("Bool32x4"));
                    }
                    simdVal = ((JavascriptSIMDBool32x4*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Bool16x8:
                    if (!JavascriptSIMDBool16x8::Is(*origArgs))
                    {TRACE_IT(47304);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdBool16x8TypeMismatch, _u("Bool16x8"));
                    }
                    simdVal = ((JavascriptSIMDBool16x8*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Bool8x16:
                    if (!JavascriptSIMDBool8x16::Is(*origArgs))
                    {TRACE_IT(47305);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdBool8x16TypeMismatch, _u("Bool8x16"));
                    }
                    simdVal = ((JavascriptSIMDBool8x16*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Float32x4:
                    if (!JavascriptSIMDFloat32x4::Is(*origArgs))
                    {TRACE_IT(47306);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdFloat32x4TypeMismatch, _u("Float32x4"));
                    }
                    simdVal = ((JavascriptSIMDFloat32x4*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Float64x2:
                    if (!JavascriptSIMDFloat64x2::Is(*origArgs))
                    {TRACE_IT(47307);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdFloat64x2TypeMismatch, _u("Float64x2"));
                    }
                    simdVal = ((JavascriptSIMDFloat64x2*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Int16x8:
                    if (!JavascriptSIMDInt16x8::Is(*origArgs))
                    {TRACE_IT(47308);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt16x8TypeMismatch, _u("Int16x8"));
                    }
                    simdVal = ((JavascriptSIMDInt16x8*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Int8x16:
                    if (!JavascriptSIMDInt8x16::Is(*origArgs))
                    {TRACE_IT(47309);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInt8x16TypeMismatch, _u("Int8x16"));
                    }
                    simdVal = ((JavascriptSIMDInt8x16*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Uint32x4:
                    if (!JavascriptSIMDUint32x4::Is(*origArgs))
                    {TRACE_IT(47310);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdUint32x4TypeMismatch, _u("Uint32x4"));
                    }
                    simdVal = ((JavascriptSIMDUint32x4*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Uint16x8:
                    if (!JavascriptSIMDUint16x8::Is(*origArgs))
                    {TRACE_IT(47311);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdUint16x8TypeMismatch, _u("Uint16x8"));
                    }
                    simdVal = ((JavascriptSIMDUint16x8*)(*origArgs))->GetValue();
                    break;
                case AsmJsType::Uint8x16:
                    if (!JavascriptSIMDUint8x16::Is(*origArgs))
                    {TRACE_IT(47312);
                        JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdUint8x16TypeMismatch, _u("Uint8x16"));
                    }
                    simdVal = ((JavascriptSIMDUint8x16*)(*origArgs))->GetValue();
                    break;
                default:
                    Assert(UNREACHED);
                }
                *(AsmJsSIMDValue*)argDst = simdVal;
                argDst = argDst + sizeof(AsmJsSIMDValue);
            }
            else
            {TRACE_IT(47313);
                Assert(UNREACHED);
            }
            ++origArgs;
        }
        // for convenience, lets take the opportunity to return the asm.js entrypoint address
        return address;
    }

#if _M_X64

    // returns an array containing the size of each argument
    uint *GetArgsSizesArray(ScriptFunction* func)
    {TRACE_IT(47314);
        AsmJsFunctionInfo* info = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        return info->GetArgsSizesArray();
    }

    int GetStackSizeForAsmJsUnboxing(ScriptFunction* func)
    {TRACE_IT(47315);
        AsmJsFunctionInfo* info = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        int argSize = MachPtr;
        for (ArgSlot i = 0; i < info->GetArgCount(); i++)
        {TRACE_IT(47316);
            if (info->GetArgType(i).isSIMD())
            {TRACE_IT(47317);
                argSize += sizeof(AsmJsSIMDValue);
            }
            else
            {TRACE_IT(47318);
                argSize += MachPtr;
            }
        }
        argSize = ::Math::Align<int32>(argSize, 16);

        if (argSize < 32)
        {TRACE_IT(47319);
            argSize = 32; // convention is to always allocate spill space for rcx,rdx,r8,r9
        }

        PROBE_STACK_CALL(func->GetScriptContext(), func, argSize);
        return argSize;
    }

    Var BoxAsmJsReturnValue(ScriptFunction* func, int64 intRetVal, double doubleRetVal, float floatRetVal, __m128 simdRetVal)
    {TRACE_IT(47320);
        // ExternalEntryPoint doesn't know the return value, so it will send garbage for everything except actual return type
        Var returnValue = nullptr;
        // make call and convert primitive type back to Var
        AsmJsFunctionInfo* info = func->GetFunctionBody()->GetAsmJsFunctionInfo();
        ScriptContext* scriptContext = func->GetScriptContext();
        switch (info->GetReturnType().which())
        {
        case AsmJsRetType::Void:
            returnValue = JavascriptOperators::OP_LdUndef(scriptContext);
            break;
        case AsmJsRetType::Signed:
        {TRACE_IT(47321);
            returnValue = JavascriptNumber::ToVar((int)intRetVal, scriptContext);
            break;
        }
        case AsmJsRetType::Int64:
        {TRACE_IT(47322);
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (CONFIG_FLAG(WasmI64))
            {TRACE_IT(47323);
                returnValue = CreateI64ReturnObject(intRetVal, scriptContext);
                break;
            }
#endif
            JavascriptError::ThrowTypeError(scriptContext, WASMERR_InvalidTypeConversion);
        }
        case AsmJsRetType::Double:
        {TRACE_IT(47324);
            returnValue = JavascriptNumber::NewWithCheck(doubleRetVal, scriptContext);
            break;
        }
        case AsmJsRetType::Float:
        {TRACE_IT(47325);
            returnValue = JavascriptNumber::NewWithCheck(floatRetVal, scriptContext);
            break;
        }
        case AsmJsRetType::Float32x4:
        {TRACE_IT(47326);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDFloat32x4::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Int32x4:
        {TRACE_IT(47327);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDInt32x4::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Bool32x4:
        {TRACE_IT(47328);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDBool32x4::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Bool16x8:
        {TRACE_IT(47329);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDBool16x8::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Bool8x16:
        {TRACE_IT(47330);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDBool8x16::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Float64x2:
        {TRACE_IT(47331);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDFloat64x2::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Int16x8:
        {TRACE_IT(47332);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDInt16x8::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Int8x16:
        {TRACE_IT(47333);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDInt8x16::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Uint32x4:
        {TRACE_IT(47334);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDUint32x4::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Uint16x8:
        {TRACE_IT(47335);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDUint16x8::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        case AsmJsRetType::Uint8x16:
        {TRACE_IT(47336);
            X86SIMDValue simdVal;
            simdVal.m128_value = simdRetVal;
            returnValue = JavascriptSIMDUint8x16::New(&X86SIMDValue::ToSIMDValue(simdVal), scriptContext);
            break;
        }
        default:
            Assume(UNREACHED);
        }

        return returnValue;
    }

#elif _M_IX86
    Var AsmJsExternalEntryPoint(RecyclableObject* entryObject, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);
        ScriptFunction* func = (ScriptFunction*)entryObject;
        FunctionBody* body = func->GetFunctionBody();
        AsmJsFunctionInfo* info = body->GetAsmJsFunctionInfo();
        int argSize = info->GetArgByteSize();
        char* dst;
        Var returnValue = 0;

        // TODO (michhol): wasm, heap should not ever be detached
        AsmJsModuleInfo::EnsureHeapAttached(func);

        argSize = ::Math::Align<int32>(argSize, 8);
        // Allocate stack space for args

        __asm
        {
            sub esp, argSize
            mov dst, esp
        };

        const void * asmJSEntryPoint = UnboxAsmJsArguments(func, args.Values + 1, dst - MachPtr, callInfo);

        // make call and convert primitive type back to Var
        switch (info->GetReturnType().which())
        {
        case AsmJsRetType::Void:
            __asm
            {TRACE_IT(47337);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
            }
            returnValue = JavascriptOperators::OP_LdUndef(func->GetScriptContext());
            break;
        case AsmJsRetType::Signed:{TRACE_IT(47338);
            int32 ival = 0;
            __asm
            {TRACE_IT(47339);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                mov ival, eax
            }
            returnValue = JavascriptNumber::ToVar(ival, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Int64:
        {TRACE_IT(47340);
            int32 iLow = 0, iHigh = 0;
            __asm
            {TRACE_IT(47341);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                mov iLow, eax;
                mov iHigh, edx;
            }
#if ENABLE_DEBUG_CONFIG_OPTIONS
            if (CONFIG_FLAG(WasmI64))
            {TRACE_IT(47342);
                returnValue = CreateI64ReturnObject((int64)iLow | ((int64)iHigh << 32), func->GetScriptContext());
                break;
            }
#endif
            JavascriptError::ThrowTypeError(func->GetScriptContext(), WASMERR_InvalidTypeConversion);
        }
        case AsmJsRetType::Double:{TRACE_IT(47343);
            double dval = 0;
            __asm
            {TRACE_IT(47344);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movsd dval, xmm0
            }
            returnValue = JavascriptNumber::NewWithCheck(dval, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Float:{TRACE_IT(47345);
            float fval = 0;
            __asm
            {TRACE_IT(47346);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movss fval, xmm0
            }
            returnValue = JavascriptNumber::NewWithCheck((double)fval, func->GetScriptContext());
            break;
        }
        case AsmJsRetType::Int32x4:
            AsmJsSIMDValue simdVal;
            simdVal.Zero();
            __asm
            {TRACE_IT(47347);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDInt32x4::New(&simdVal, func->GetScriptContext());
            break;
        case AsmJsRetType::Bool32x4:
            simdVal.Zero();
            __asm
            {TRACE_IT(47348);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                    push func
                    call ecx
                    movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDBool32x4::New(&simdVal, func->GetScriptContext());
            break;
        case AsmJsRetType::Bool16x8:
            simdVal.Zero();
            __asm
            {TRACE_IT(47349);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                    push func
                    call ecx
                    movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDBool16x8::New(&simdVal, func->GetScriptContext());
            break;
        case AsmJsRetType::Bool8x16:
            simdVal.Zero();
            __asm
            {TRACE_IT(47350);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                    push func
                    call ecx
                    movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDBool8x16::New(&simdVal, func->GetScriptContext());
            break;
        case AsmJsRetType::Float32x4:
            simdVal.Zero();
            __asm
            {TRACE_IT(47351);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movups simdVal, xmm0
            }
                returnValue = JavascriptSIMDFloat32x4::New(&simdVal, func->GetScriptContext());
                break;

        case AsmJsRetType::Float64x2:
            simdVal.Zero();
            __asm
            {TRACE_IT(47352);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                push func
                call ecx
                movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDFloat64x2::New(&simdVal, func->GetScriptContext());
            break;

        case AsmJsRetType::Int16x8:
            simdVal.Zero();
            __asm
            {TRACE_IT(47353);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                    push func
                    call ecx
                    movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDInt16x8::New(&simdVal, func->GetScriptContext());
            break;

        case AsmJsRetType::Int8x16:
            simdVal.Zero();
            __asm
            {TRACE_IT(47354);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                    push func
                    call ecx
                    movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDInt8x16::New(&simdVal, func->GetScriptContext());
            break;

        case AsmJsRetType::Uint32x4:
            simdVal.Zero();
            __asm
            {TRACE_IT(47355);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                    push func
                    call ecx
                    movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDUint32x4::New(&simdVal, func->GetScriptContext());
            break;

        case AsmJsRetType::Uint16x8:
            simdVal.Zero();
            __asm
            {TRACE_IT(47356);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                    push func
                    call ecx
                    movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDUint16x8::New(&simdVal, func->GetScriptContext());
            break;

        case AsmJsRetType::Uint8x16:
            simdVal.Zero();
            __asm
            {TRACE_IT(47357);
                mov  ecx, asmJSEntryPoint
#ifdef _CONTROL_FLOW_GUARD
                call[__guard_check_icall_fptr]
#endif
                    push func
                    call ecx
                    movups simdVal, xmm0
            }
            returnValue = JavascriptSIMDUint8x16::New(&simdVal, func->GetScriptContext());
            break;
        default:
            Assume(UNREACHED);
        }
        return returnValue;
    }
#endif

}
#endif
