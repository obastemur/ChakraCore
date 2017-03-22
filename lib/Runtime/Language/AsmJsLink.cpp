//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"
#include "Library/BoundFunction.h"

#ifdef ASMJS_PLAT
namespace Js{
    bool ASMLink::CheckArrayBuffer(ScriptContext* scriptContext, Var bufferView, const AsmJsModuleInfo * info)
    {LOGMEIN("AsmJsLink.cpp] 11\n");
        if (!bufferView)
        {LOGMEIN("AsmJsLink.cpp] 13\n");
            return true;
        }

        if (!JavascriptArrayBuffer::Is(bufferView))
        {LOGMEIN("AsmJsLink.cpp] 18\n");
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Buffer parameter is not an Array buffer"));
            return false;
        }
        JavascriptArrayBuffer* buffer = (JavascriptArrayBuffer*)bufferView;
        if (buffer->GetByteLength() <= info->GetMaxHeapAccess())
        {LOGMEIN("AsmJsLink.cpp] 24\n");
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Buffer bytelength is smaller than constant accesses"));
            return false;
        }
        if (info->GetUsesChangeHeap())
        {LOGMEIN("AsmJsLink.cpp] 29\n");
            if (buffer->GetByteLength() < 0x1000000)
            {LOGMEIN("AsmJsLink.cpp] 31\n");
                Output::Print(_u("Asm.js Runtime Error : Buffer bytelength is not a valid size for asm.js\n"));
                return false;
            }
            if (info->GetMaxHeapAccess() >= 0x1000000)
            {LOGMEIN("AsmJsLink.cpp] 36\n");
                Output::Print(_u("Asm.js Runtime Error : Cannot have such large constant accesses\n"));
                return false;
            }
        }

        if (!buffer->IsValidAsmJsBufferLength(buffer->GetByteLength(), true))
        {LOGMEIN("AsmJsLink.cpp] 43\n");
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Buffer bytelength is not a valid size for asm.js"));
            return false;
        }

        return true;
    }

    bool ASMLink::CheckFFI(ScriptContext* scriptContext, AsmJsModuleInfo* info, const Var foreign)
    {LOGMEIN("AsmJsLink.cpp] 52\n");
        if (info->GetFunctionImportCount() == 0 && info->GetVarImportCount() == 0)
        {LOGMEIN("AsmJsLink.cpp] 54\n");
            return true;
        }
        Assert(foreign);
        if (!RecyclableObject::Is(foreign))
        {LOGMEIN("AsmJsLink.cpp] 59\n");
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : FFI is not an object"));
            return false;
        }
        TypeId foreignObjType = RecyclableObject::FromVar(foreign)->GetTypeId();
        if (StaticType::Is(foreignObjType) || TypeIds_Proxy == foreignObjType)
        {LOGMEIN("AsmJsLink.cpp] 65\n");
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : FFI is not an object"));
            return false;
        }
        return true;
    }

    bool ASMLink::CheckStdLib(ScriptContext* scriptContext, const AsmJsModuleInfo* info, const Var stdlib)
    {LOGMEIN("AsmJsLink.cpp] 73\n");
        BVStatic<ASMMATH_BUILTIN_SIZE> mathBuiltinUsed = info->GetAsmMathBuiltinUsed();
        BVStatic<ASMARRAY_BUILTIN_SIZE> arrayBuiltinUsed = info->GetAsmArrayBuiltinUsed();
        BVStatic<ASMSIMD_BUILTIN_SIZE> simdBuiltinUsed = info->GetAsmSimdBuiltinUsed();

        if (mathBuiltinUsed.IsAllClear() && arrayBuiltinUsed.IsAllClear() && simdBuiltinUsed.IsAllClear())
        {LOGMEIN("AsmJsLink.cpp] 79\n");
            return true;
        }
        Assert(stdlib);
        if (!RecyclableObject::Is(stdlib))
        {LOGMEIN("AsmJsLink.cpp] 84\n");
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : StdLib is not an object"));
            return false;
        }
        TypeId stdLibObjType = RecyclableObject::FromVar(stdlib)->GetTypeId();
        if (StaticType::Is(stdLibObjType) || TypeIds_Proxy == stdLibObjType)
        {LOGMEIN("AsmJsLink.cpp] 90\n");
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : StdLib is not an object"));
            return false;
        }

        Js::JavascriptLibrary* library = scriptContext->GetLibrary();
        if (mathBuiltinUsed.TestAndClear(AsmJSMathBuiltinFunction::AsmJSMathBuiltin_infinity))
        {LOGMEIN("AsmJsLink.cpp] 97\n");
            Var asmInfinityObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Infinity, scriptContext);
            if (!JavascriptConversion::SameValue(asmInfinityObj, library->GetPositiveInfinite()))
            {LOGMEIN("AsmJsLink.cpp] 100\n");
                AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Math constant Infinity is invalid"));
                return false;
            }
        }
        if (mathBuiltinUsed.TestAndClear(AsmJSMathBuiltinFunction::AsmJSMathBuiltin_nan))
        {LOGMEIN("AsmJsLink.cpp] 106\n");
            Var asmNaNObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::NaN, scriptContext);
            if (!JavascriptConversion::SameValue(asmNaNObj, library->GetNaN()))
            {LOGMEIN("AsmJsLink.cpp] 109\n");
                AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Math constant NaN is invalid"));
                return false;
            }
        }
        if (!mathBuiltinUsed.IsAllClear())
        {LOGMEIN("AsmJsLink.cpp] 115\n");
            Var asmMathObject = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Math, scriptContext);
            for (int i = 0; i < AsmJSMathBuiltinFunction::AsmJSMathBuiltin_COUNT; i++)
            {LOGMEIN("AsmJsLink.cpp] 118\n");
                //check if bit is set
                if (!mathBuiltinUsed.Test(i))
                {LOGMEIN("AsmJsLink.cpp] 121\n");
                    continue;
                }
                AsmJSMathBuiltinFunction mathBuiltinFunc = (AsmJSMathBuiltinFunction)i;
                if (!CheckMathLibraryMethod(scriptContext, asmMathObject, mathBuiltinFunc))
                {LOGMEIN("AsmJsLink.cpp] 126\n");
                    AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Math builtin function is invalid"));
                    return false;
                }
            }
        }
        for (int i = 0; i < AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_COUNT; i++)
        {LOGMEIN("AsmJsLink.cpp] 133\n");
            //check if bit is set
            if (!arrayBuiltinUsed.Test(i))
            {LOGMEIN("AsmJsLink.cpp] 136\n");
                continue;
            }
            AsmJSTypedArrayBuiltinFunction arrayBuiltinFunc = (AsmJSTypedArrayBuiltinFunction)i;
            if (!CheckArrayLibraryMethod(scriptContext, stdlib, arrayBuiltinFunc))
            {LOGMEIN("AsmJsLink.cpp] 141\n");
                AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Array builtin function is invalid"));
                return false;
            }
        }

#ifdef ENABLE_SIMDJS
        if (!simdBuiltinUsed.IsAllClear())
        {LOGMEIN("AsmJsLink.cpp] 149\n");
            Var asmSimdObject = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::SIMD, scriptContext);
            for (int i = 0; i < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_COUNT; i++)
            {LOGMEIN("AsmJsLink.cpp] 152\n");
                if (!simdBuiltinUsed.Test(i))
                {LOGMEIN("AsmJsLink.cpp] 154\n");
                    continue;
                }
                AsmJsSIMDBuiltinFunction simdBuiltinFunc = (AsmJsSIMDBuiltinFunction)i;
                if (!CheckSimdLibraryMethod(scriptContext, asmSimdObject, simdBuiltinFunc))
                {LOGMEIN("AsmJsLink.cpp] 159\n");
                    AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : SIMD builtin function is invalid"));
                    return false;
                }
            }
        }
#endif

        return true;
    }

    bool ASMLink::CheckArrayLibraryMethod(ScriptContext* scriptContext, const Var stdlib, const AsmJSTypedArrayBuiltinFunction arrayLibMethod)
    {LOGMEIN("AsmJsLink.cpp] 171\n");
        Var arrayFuncObj;
        switch (arrayLibMethod)
        {LOGMEIN("AsmJsLink.cpp] 174\n");
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_byteLength:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::byteLength, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 178\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                if (arrayLibFunc->IsBoundFunction())
                {LOGMEIN("AsmJsLink.cpp] 181\n");
                    BoundFunction* boundFunc = (BoundFunction*)arrayLibFunc;
                    RecyclableObject* thisObj = boundFunc->GetBoundThis();
                    if (JavascriptFunction::Is(thisObj))
                    {LOGMEIN("AsmJsLink.cpp] 185\n");
                        JavascriptFunction * thisFunc = (JavascriptFunction*)thisObj;
                        if (thisFunc->GetFunctionInfo()->GetOriginalEntryPoint() != (&ArrayBuffer::EntryInfo::GetterByteLength)->GetOriginalEntryPoint())
                        {LOGMEIN("AsmJsLink.cpp] 188\n");
                            return false;
                        }
                    }
                    JavascriptFunction* targetFunc = boundFunc->GetTargetFunction();
                    return targetFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&JavascriptFunction::EntryInfo::Call)->GetOriginalEntryPoint();
                }
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Int8Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Int8Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 200\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int8Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint8Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint8Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 208\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint8Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Int16Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Int16Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 216\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int16Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint16Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint16Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 224\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint16Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Int32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Int32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 232\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 240\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Float32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Float32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 248\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Float32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Float64Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Float64Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 256\n");
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Float64Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        default:
            Assume(UNREACHED);
        }
        return false;
    }

    bool ASMLink::CheckMathLibraryMethod(ScriptContext* scriptContext, const Var asmMathObject, const AsmJSMathBuiltinFunction mathLibMethod)
    {LOGMEIN("AsmJsLink.cpp] 268\n");
        Var mathFuncObj;
        switch (mathLibMethod)
        {LOGMEIN("AsmJsLink.cpp] 271\n");
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sin:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::sin, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 275\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Sin)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 278\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_cos:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::cos, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 286\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Cos)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 289\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_tan:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::tan, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 297\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Tan)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 300\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_asin:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::asin, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 308\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Asin)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 311\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_acos:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::acos, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 319\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Acos)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 322\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_atan:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::atan, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 330\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Atan)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 333\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ceil:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::ceil, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 341\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Ceil)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 344\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_floor:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::floor, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 352\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Floor)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 355\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_exp:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::exp, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 363\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Exp)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 366\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::log, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 374\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Log)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 377\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_pow:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::pow, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 385\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Pow)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 388\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::sqrt, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 396\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Sqrt)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 399\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_abs:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::abs, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 407\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Abs)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 410\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_atan2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::atan2, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 418\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Atan2)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 421\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_imul:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::imul, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 429\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Imul)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 432\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_clz32:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::clz32, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 440\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Clz32)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 443\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_min:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::min, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 451\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Min)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 454\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_max:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::max, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 462\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Max)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 465\n");
                    return true;
                }
            }
            break;

        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_fround:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::fround, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 474\n");
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Fround)->GetOriginalEntryPoint())
                {LOGMEIN("AsmJsLink.cpp] 477\n");
                    return true;
                }
            }
            break;

        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 486\n");
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::E))
                {LOGMEIN("AsmJsLink.cpp] 489\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln10:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LN10, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 497\n");
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LN10))
                {LOGMEIN("AsmJsLink.cpp] 500\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LN2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 508\n");
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LN2))
                {LOGMEIN("AsmJsLink.cpp] 511\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log2e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LOG2E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 519\n");
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LOG2E))
                {LOGMEIN("AsmJsLink.cpp] 522\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log10e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LOG10E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 530\n");
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LOG10E))
                {LOGMEIN("AsmJsLink.cpp] 533\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_pi:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::PI, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 541\n");
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::PI))
                {LOGMEIN("AsmJsLink.cpp] 544\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt1_2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::SQRT1_2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 552\n");
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::SQRT1_2))
                {LOGMEIN("AsmJsLink.cpp] 555\n");
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::SQRT2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {LOGMEIN("AsmJsLink.cpp] 563\n");
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::SQRT2))
                {LOGMEIN("AsmJsLink.cpp] 566\n");
                    return true;
                }
            }
            break;
        default:
            Assume(UNREACHED);
        }
        return false;
    }

#ifdef ENABLE_SIMDJS
    bool ASMLink::CheckSimdLibraryMethod(ScriptContext* scriptContext, const Var asmSimdObject, const AsmJsSIMDBuiltinFunction simdLibMethod)
    {LOGMEIN("AsmJsLink.cpp] 579\n");
        Var simdConstructorObj, simdFuncObj;

        switch (simdLibMethod)
        {LOGMEIN("AsmJsLink.cpp] 583\n");
#define ASMJS_SIMD_C_NAMES(builtInId, propertyId, libName, entryPoint) \
        case  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_##builtInId: \
            simdFuncObj = JavascriptOperators::OP_GetProperty(asmSimdObject, PropertyIds::##libName, scriptContext); \
            if (JavascriptFunction::Is(simdFuncObj)) \
            {LOGMEIN("AsmJsLink.cpp] 588\n"); \
                JavascriptFunction* simdLibFunc = (JavascriptFunction*)simdFuncObj; \
                if (simdLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&SIMD##libName##Lib::EntryInfo::##entryPoint)->GetOriginalEntryPoint()) \
                {LOGMEIN("AsmJsLink.cpp] 591\n"); \
                    return true; \
                }\
            } \
            break;


#define ASMJS_SIMD_O_NAMES(builtInId, propertyId, libName, entryPoint) \
        case  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_##builtInId: \
            simdConstructorObj = JavascriptOperators::OP_GetProperty(asmSimdObject, PropertyIds::##libName, scriptContext); \
            simdFuncObj = JavascriptOperators::OP_GetProperty(simdConstructorObj, PropertyIds::##propertyId, scriptContext); \
            if (JavascriptFunction::Is(simdFuncObj)) \
            {LOGMEIN("AsmJsLink.cpp] 603\n"); \
                JavascriptFunction* simdLibFunc = (JavascriptFunction*)simdFuncObj; \
                if (simdLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&SIMD##libName##Lib::EntryInfo::##entryPoint)->GetOriginalEntryPoint()) \
                {LOGMEIN("AsmJsLink.cpp] 606\n"); \
                    return true; \
                }\
            } \
            break;
#include "AsmJsBuiltinNames.h"



        default:
            Assume(UNREACHED);
        }
        return false;
    }
#endif




    bool ASMLink::CheckParams(ScriptContext* scriptContext, AsmJsModuleInfo* info, const Var stdlib, const Var foreign, const Var bufferView)
    {
        if (CheckStdLib(scriptContext, info, stdlib) && CheckArrayBuffer(scriptContext, bufferView, info) && CheckFFI(scriptContext, info, stdlib))
        {LOGMEIN("AsmJsLink.cpp] 628\n");
            return true;
        }
        Output::Flush();
        return false;
    }
}
#endif
