//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"
#include "Library/BoundFunction.h"

#ifdef ASMJS_PLAT
namespace Js{
    bool ASMLink::CheckArrayBuffer(ScriptContext* scriptContext, Var bufferView, const AsmJsModuleInfo * info)
    {TRACE_IT(46391);
        if (!bufferView)
        {TRACE_IT(46392);
            return true;
        }

        if (!JavascriptArrayBuffer::Is(bufferView))
        {TRACE_IT(46393);
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Buffer parameter is not an Array buffer"));
            return false;
        }
        JavascriptArrayBuffer* buffer = (JavascriptArrayBuffer*)bufferView;
        if (buffer->GetByteLength() <= info->GetMaxHeapAccess())
        {TRACE_IT(46394);
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Buffer bytelength is smaller than constant accesses"));
            return false;
        }
        if (info->GetUsesChangeHeap())
        {TRACE_IT(46395);
            if (buffer->GetByteLength() < 0x1000000)
            {TRACE_IT(46396);
                Output::Print(_u("Asm.js Runtime Error : Buffer bytelength is not a valid size for asm.js\n"));
                return false;
            }
            if (info->GetMaxHeapAccess() >= 0x1000000)
            {TRACE_IT(46397);
                Output::Print(_u("Asm.js Runtime Error : Cannot have such large constant accesses\n"));
                return false;
            }
        }

        if (!buffer->IsValidAsmJsBufferLength(buffer->GetByteLength(), true))
        {TRACE_IT(46398);
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Buffer bytelength is not a valid size for asm.js"));
            return false;
        }

        return true;
    }

    bool ASMLink::CheckFFI(ScriptContext* scriptContext, AsmJsModuleInfo* info, const Var foreign)
    {TRACE_IT(46399);
        if (info->GetFunctionImportCount() == 0 && info->GetVarImportCount() == 0)
        {TRACE_IT(46400);
            return true;
        }
        Assert(foreign);
        if (!RecyclableObject::Is(foreign))
        {TRACE_IT(46401);
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : FFI is not an object"));
            return false;
        }
        TypeId foreignObjType = RecyclableObject::FromVar(foreign)->GetTypeId();
        if (StaticType::Is(foreignObjType) || TypeIds_Proxy == foreignObjType)
        {TRACE_IT(46402);
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : FFI is not an object"));
            return false;
        }
        return true;
    }

    bool ASMLink::CheckStdLib(ScriptContext* scriptContext, const AsmJsModuleInfo* info, const Var stdlib)
    {TRACE_IT(46403);
        BVStatic<ASMMATH_BUILTIN_SIZE> mathBuiltinUsed = info->GetAsmMathBuiltinUsed();
        BVStatic<ASMARRAY_BUILTIN_SIZE> arrayBuiltinUsed = info->GetAsmArrayBuiltinUsed();
        BVStatic<ASMSIMD_BUILTIN_SIZE> simdBuiltinUsed = info->GetAsmSimdBuiltinUsed();

        if (mathBuiltinUsed.IsAllClear() && arrayBuiltinUsed.IsAllClear() && simdBuiltinUsed.IsAllClear())
        {TRACE_IT(46404);
            return true;
        }
        Assert(stdlib);
        if (!RecyclableObject::Is(stdlib))
        {TRACE_IT(46405);
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : StdLib is not an object"));
            return false;
        }
        TypeId stdLibObjType = RecyclableObject::FromVar(stdlib)->GetTypeId();
        if (StaticType::Is(stdLibObjType) || TypeIds_Proxy == stdLibObjType)
        {TRACE_IT(46406);
            AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : StdLib is not an object"));
            return false;
        }

        Js::JavascriptLibrary* library = scriptContext->GetLibrary();
        if (mathBuiltinUsed.TestAndClear(AsmJSMathBuiltinFunction::AsmJSMathBuiltin_infinity))
        {TRACE_IT(46407);
            Var asmInfinityObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Infinity, scriptContext);
            if (!JavascriptConversion::SameValue(asmInfinityObj, library->GetPositiveInfinite()))
            {TRACE_IT(46408);
                AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Math constant Infinity is invalid"));
                return false;
            }
        }
        if (mathBuiltinUsed.TestAndClear(AsmJSMathBuiltinFunction::AsmJSMathBuiltin_nan))
        {TRACE_IT(46409);
            Var asmNaNObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::NaN, scriptContext);
            if (!JavascriptConversion::SameValue(asmNaNObj, library->GetNaN()))
            {TRACE_IT(46410);
                AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Math constant NaN is invalid"));
                return false;
            }
        }
        if (!mathBuiltinUsed.IsAllClear())
        {TRACE_IT(46411);
            Var asmMathObject = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Math, scriptContext);
            for (int i = 0; i < AsmJSMathBuiltinFunction::AsmJSMathBuiltin_COUNT; i++)
            {TRACE_IT(46412);
                //check if bit is set
                if (!mathBuiltinUsed.Test(i))
                {TRACE_IT(46413);
                    continue;
                }
                AsmJSMathBuiltinFunction mathBuiltinFunc = (AsmJSMathBuiltinFunction)i;
                if (!CheckMathLibraryMethod(scriptContext, asmMathObject, mathBuiltinFunc))
                {TRACE_IT(46414);
                    AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Math builtin function is invalid"));
                    return false;
                }
            }
        }
        for (int i = 0; i < AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_COUNT; i++)
        {TRACE_IT(46415);
            //check if bit is set
            if (!arrayBuiltinUsed.Test(i))
            {TRACE_IT(46416);
                continue;
            }
            AsmJSTypedArrayBuiltinFunction arrayBuiltinFunc = (AsmJSTypedArrayBuiltinFunction)i;
            if (!CheckArrayLibraryMethod(scriptContext, stdlib, arrayBuiltinFunc))
            {TRACE_IT(46417);
                AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : Array builtin function is invalid"));
                return false;
            }
        }

#ifdef ENABLE_SIMDJS
        if (!simdBuiltinUsed.IsAllClear())
        {TRACE_IT(46418);
            Var asmSimdObject = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::SIMD, scriptContext);
            for (int i = 0; i < AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_COUNT; i++)
            {TRACE_IT(46419);
                if (!simdBuiltinUsed.Test(i))
                {TRACE_IT(46420);
                    continue;
                }
                AsmJsSIMDBuiltinFunction simdBuiltinFunc = (AsmJsSIMDBuiltinFunction)i;
                if (!CheckSimdLibraryMethod(scriptContext, asmSimdObject, simdBuiltinFunc))
                {TRACE_IT(46421);
                    AsmJSCompiler::OutputError(scriptContext, _u("Asm.js Runtime Error : SIMD builtin function is invalid"));
                    return false;
                }
            }
        }
#endif

        return true;
    }

    bool ASMLink::CheckArrayLibraryMethod(ScriptContext* scriptContext, const Var stdlib, const AsmJSTypedArrayBuiltinFunction arrayLibMethod)
    {TRACE_IT(46422);
        Var arrayFuncObj;
        switch (arrayLibMethod)
        {
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_byteLength:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::byteLength, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {TRACE_IT(46423);
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                if (arrayLibFunc->IsBoundFunction())
                {TRACE_IT(46424);
                    BoundFunction* boundFunc = (BoundFunction*)arrayLibFunc;
                    RecyclableObject* thisObj = boundFunc->GetBoundThis();
                    if (JavascriptFunction::Is(thisObj))
                    {TRACE_IT(46425);
                        JavascriptFunction * thisFunc = (JavascriptFunction*)thisObj;
                        if (thisFunc->GetFunctionInfo()->GetOriginalEntryPoint() != (&ArrayBuffer::EntryInfo::GetterByteLength)->GetOriginalEntryPoint())
                        {TRACE_IT(46426);
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
            {TRACE_IT(46427);
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int8Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint8Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint8Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {TRACE_IT(46428);
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint8Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Int16Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Int16Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {TRACE_IT(46429);
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int16Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint16Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint16Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {TRACE_IT(46430);
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint16Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Int32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Int32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {TRACE_IT(46431);
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Int32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Uint32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Uint32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {TRACE_IT(46432);
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Uint32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Float32Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Float32Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {TRACE_IT(46433);
                JavascriptFunction* arrayLibFunc = (JavascriptFunction*)arrayFuncObj;
                return arrayLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Float32Array::EntryInfo::NewInstance)->GetOriginalEntryPoint();
            }
            break;
        case AsmJSTypedArrayBuiltinFunction::AsmJSTypedArrayBuiltin_Float64Array:
            arrayFuncObj = JavascriptOperators::OP_GetProperty(stdlib, PropertyIds::Float64Array, scriptContext);
            if (JavascriptFunction::Is(arrayFuncObj))
            {TRACE_IT(46434);
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
    {TRACE_IT(46435);
        Var mathFuncObj;
        switch (mathLibMethod)
        {
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sin:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::sin, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46436);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Sin)->GetOriginalEntryPoint())
                {TRACE_IT(46437);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_cos:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::cos, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46438);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Cos)->GetOriginalEntryPoint())
                {TRACE_IT(46439);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_tan:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::tan, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46440);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Tan)->GetOriginalEntryPoint())
                {TRACE_IT(46441);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_asin:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::asin, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46442);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Asin)->GetOriginalEntryPoint())
                {TRACE_IT(46443);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_acos:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::acos, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46444);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Acos)->GetOriginalEntryPoint())
                {TRACE_IT(46445);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_atan:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::atan, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46446);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Atan)->GetOriginalEntryPoint())
                {TRACE_IT(46447);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ceil:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::ceil, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46448);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Ceil)->GetOriginalEntryPoint())
                {TRACE_IT(46449);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_floor:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::floor, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46450);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Floor)->GetOriginalEntryPoint())
                {TRACE_IT(46451);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_exp:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::exp, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46452);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Exp)->GetOriginalEntryPoint())
                {TRACE_IT(46453);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::log, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46454);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Log)->GetOriginalEntryPoint())
                {TRACE_IT(46455);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_pow:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::pow, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46456);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Pow)->GetOriginalEntryPoint())
                {TRACE_IT(46457);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::sqrt, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46458);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Sqrt)->GetOriginalEntryPoint())
                {TRACE_IT(46459);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_abs:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::abs, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46460);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Abs)->GetOriginalEntryPoint())
                {TRACE_IT(46461);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_atan2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::atan2, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46462);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Atan2)->GetOriginalEntryPoint())
                {TRACE_IT(46463);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_imul:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::imul, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46464);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Imul)->GetOriginalEntryPoint())
                {TRACE_IT(46465);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_clz32:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::clz32, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46466);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Clz32)->GetOriginalEntryPoint())
                {TRACE_IT(46467);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_min:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::min, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46468);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Min)->GetOriginalEntryPoint())
                {TRACE_IT(46469);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_max:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::max, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46470);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Max)->GetOriginalEntryPoint())
                {TRACE_IT(46471);
                    return true;
                }
            }
            break;

        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_fround:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::fround, scriptContext);
            if (JavascriptFunction::Is(mathFuncObj))
            {TRACE_IT(46472);
                JavascriptFunction* mathLibFunc = (JavascriptFunction*)mathFuncObj;
                if (mathLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&Math::EntryInfo::Fround)->GetOriginalEntryPoint())
                {TRACE_IT(46473);
                    return true;
                }
            }
            break;

        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {TRACE_IT(46474);
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::E))
                {TRACE_IT(46475);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln10:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LN10, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {TRACE_IT(46476);
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LN10))
                {TRACE_IT(46477);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_ln2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LN2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {TRACE_IT(46478);
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LN2))
                {TRACE_IT(46479);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log2e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LOG2E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {TRACE_IT(46480);
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LOG2E))
                {TRACE_IT(46481);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_log10e:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::LOG10E, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {TRACE_IT(46482);
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::LOG10E))
                {TRACE_IT(46483);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_pi:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::PI, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {TRACE_IT(46484);
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::PI))
                {TRACE_IT(46485);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt1_2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::SQRT1_2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {TRACE_IT(46486);
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::SQRT1_2))
                {TRACE_IT(46487);
                    return true;
                }
            }
            break;
        case AsmJSMathBuiltinFunction::AsmJSMathBuiltin_sqrt2:
            mathFuncObj = JavascriptOperators::OP_GetProperty(asmMathObject, PropertyIds::SQRT2, scriptContext);
            if (JavascriptNumber::Is(mathFuncObj))
            {TRACE_IT(46488);
                JavascriptNumber* mathConstNumber = (JavascriptNumber*)mathFuncObj;
                if (JavascriptNumber::GetValue(mathConstNumber) == (Math::SQRT2))
                {TRACE_IT(46489);
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
    {TRACE_IT(46490);
        Var simdConstructorObj, simdFuncObj;

        switch (simdLibMethod)
        {
#define ASMJS_SIMD_C_NAMES(builtInId, propertyId, libName, entryPoint) \
        case  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_##builtInId: \
            simdFuncObj = JavascriptOperators::OP_GetProperty(asmSimdObject, PropertyIds::##libName, scriptContext); \
            if (JavascriptFunction::Is(simdFuncObj)) \
            {TRACE_IT(46491); \
                JavascriptFunction* simdLibFunc = (JavascriptFunction*)simdFuncObj; \
                if (simdLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&SIMD##libName##Lib::EntryInfo::##entryPoint)->GetOriginalEntryPoint()) \
                {TRACE_IT(46492); \
                    return true; \
                }\
            } \
            break;


#define ASMJS_SIMD_O_NAMES(builtInId, propertyId, libName, entryPoint) \
        case  AsmJsSIMDBuiltinFunction::AsmJsSIMDBuiltin_##builtInId: \
            simdConstructorObj = JavascriptOperators::OP_GetProperty(asmSimdObject, PropertyIds::##libName, scriptContext); \
            simdFuncObj = JavascriptOperators::OP_GetProperty(simdConstructorObj, PropertyIds::##propertyId, scriptContext); \
            if (JavascriptFunction::Is(simdFuncObj)) \
            {TRACE_IT(46493); \
                JavascriptFunction* simdLibFunc = (JavascriptFunction*)simdFuncObj; \
                if (simdLibFunc->GetFunctionInfo()->GetOriginalEntryPoint() == (&SIMD##libName##Lib::EntryInfo::##entryPoint)->GetOriginalEntryPoint()) \
                {TRACE_IT(46494); \
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
        {TRACE_IT(46495);
            return true;
        }
        Output::Flush();
        return false;
    }
}
#endif
