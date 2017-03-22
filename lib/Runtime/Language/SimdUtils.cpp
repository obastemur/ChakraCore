//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

namespace Js
{
    bool SIMDUtils::IsSimdType(const Var aVar) //Needs SIMD Type Id's to be contiguous. 
    {LOGMEIN("SimdUtils.cpp] 10\n");
        Assert(aVar);
        TypeId tid = JavascriptOperators::GetTypeId(aVar);
        return (TypeIds_SIMDFloat32x4 <= tid && tid <= TypeIds_SIMDBool8x16) ? true : false;
    }
    uint32 SIMDUtils::GetSIMDLaneCount(const Var aVar)
    {LOGMEIN("SimdUtils.cpp] 16\n");
        Assert(IsSimdType(aVar));
        TypeId tid = JavascriptOperators::GetTypeId(aVar);
        switch(tid)
        {LOGMEIN("SimdUtils.cpp] 20\n");
        case TypeIds_SIMDFloat32x4:
        case TypeIds_SIMDInt32x4:
        case TypeIds_SIMDUint32x4:
        case TypeIds_SIMDBool32x4:
            return 4;
        case TypeIds_SIMDInt16x8:
        case TypeIds_SIMDUint16x8:
        case TypeIds_SIMDBool16x8:
            return 8;
        case TypeIds_SIMDInt8x16:
        case TypeIds_SIMDUint8x16:
        case TypeIds_SIMDBool8x16:
            return 16;
        default:
            Assert(UNREACHED);
        }
        return 0;
    }

    uint32 SIMDUtils::SIMDCheckTypedArrayIndex(ScriptContext* scriptContext, const Var index)
    {LOGMEIN("SimdUtils.cpp] 41\n");
        uint32 uint32Value;
        Assert(index != NULL);

        uint32Value = SIMDCheckUint32Number<true>(scriptContext, index);
        return uint32Value;
    }

    uint32 SIMDUtils::SIMDCheckLaneIndex(ScriptContext* scriptContext, const Var lane, const uint32 range)
    {LOGMEIN("SimdUtils.cpp] 50\n");
        Assert(lane != NULL);
        uint32 uint32Value = SIMDCheckUint32Number(scriptContext, lane);

        if (uint32Value >= range)
        {LOGMEIN("SimdUtils.cpp] 55\n");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_SimdLaneRangeError);
        }
        return uint32Value;
    }

    // Is Number with uint32 value.
    template<bool acceptNegZero>
    uint32 SIMDUtils::SIMDCheckUint32Number(ScriptContext* scriptContext, const Var value)
    {LOGMEIN("SimdUtils.cpp] 64\n");
        int32 int32Value;

        if (JavascriptNumber::Is(value))
        {LOGMEIN("SimdUtils.cpp] 68\n");
            if (!JavascriptNumber::TryGetInt32Value<acceptNegZero>(JavascriptNumber::GetValue(value), &int32Value))
            {LOGMEIN("SimdUtils.cpp] 70\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange);
            }
        }
        else if (TaggedInt::Is(value))
        {LOGMEIN("SimdUtils.cpp] 75\n");
            int32Value = TaggedInt::ToInt32(value);
        }
        else
        {
            if (!JavascriptNumber::TryGetInt32Value<acceptNegZero>(JavascriptConversion::ToNumber(value, scriptContext), &int32Value))
            {LOGMEIN("SimdUtils.cpp] 81\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange);
            }
        }
        return int32Value;
    }


#if _M_IX86 || _M_AMD64
    SIMDValue SIMDUtils::FromSimdBits(const SIMDValue value)
    {LOGMEIN("SimdUtils.cpp] 91\n");
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        _mm_store_ps(x86Result.f32, v.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }
#else
    SIMDValue SIMDUtils::FromSimdBits(const SIMDValue value)
    {LOGMEIN("SimdUtils.cpp] 101\n");
        SIMDValue result;
        result.i32[SIMD_X] = value.i32[SIMD_X];
        result.i32[SIMD_Y] = value.i32[SIMD_Y];
        result.i32[SIMD_Z] = value.i32[SIMD_Z];
        result.i32[SIMD_W] = value.i32[SIMD_W];
        return result;
    }
#endif

    SIMDValue SIMDUtils::SIMD128InnerShuffle(const SIMDValue src1, const SIMDValue src2, uint32 laneCount, const uint32* lanes)
    {LOGMEIN("SimdUtils.cpp] 112\n");
        SIMDValue result = { 0 };
        Assert(laneCount == 16 || laneCount == 8 || laneCount == 4);
        Assert(lanes != nullptr);
        switch (laneCount)
        {LOGMEIN("SimdUtils.cpp] 117\n");
        case 4:
            for (uint i = 0; i < laneCount; ++i)
            {LOGMEIN("SimdUtils.cpp] 120\n");
                result.i32[i] = lanes[i] < laneCount ? src1.i32[lanes[i]] : src2.i32[lanes[i] - laneCount];
            }
            break;
        case 8:
            for (uint i = 0; i < laneCount; ++i)
            {LOGMEIN("SimdUtils.cpp] 126\n");
                result.i16[i] = lanes[i] < laneCount ? src1.i16[lanes[i]] : src2.i16[lanes[i] - laneCount];
            }
            break;
        case 16:
            for (uint i = 0; i < laneCount; ++i)
            {LOGMEIN("SimdUtils.cpp] 132\n");
                result.i8[i] = lanes[i] < laneCount ? src1.i8[lanes[i]] : src2.i8[lanes[i] - laneCount];
            }
            break;
        default:
            Assert(UNREACHED);
        }
        return result;
    }

    template <class SIMDType>
    Var SIMDUtils::SIMD128SlowShuffle(Var src1, Var src2, Var* lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext)
    {LOGMEIN("SimdUtils.cpp] 144\n");
        SIMDType *a = SIMDType::FromVar(src1);
        SIMDType *b = SIMDType::FromVar(src2);
        Assert(a);
        Assert(b);

        SIMDValue src1Value = a->GetValue();
        SIMDValue src2Value = b->GetValue();
        SIMDValue result;

        uint32 laneValue[16] = { 0 };
        Assert(laneCount == 16 || laneCount == 8 || laneCount == 4);

        for (uint i = 0; i < laneCount; ++i)
        {LOGMEIN("SimdUtils.cpp] 158\n");
            laneValue[i] = SIMDUtils::SIMDCheckLaneIndex(scriptContext, lanes[i], range);
        }

        result = SIMD128InnerShuffle(src1Value, src2Value, laneCount, laneValue);

        return SIMDType::New(&result, scriptContext);
    }
    template Var SIMDUtils::SIMD128SlowShuffle<JavascriptSIMDInt32x4  >(Var src1, Var src2, Var *lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext);
    template Var SIMDUtils::SIMD128SlowShuffle<JavascriptSIMDFloat32x4>(Var src1, Var src2, Var *lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext);
    template Var SIMDUtils::SIMD128SlowShuffle<JavascriptSIMDFloat64x2>(Var src1, Var src2, Var *lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext);
    template Var SIMDUtils::SIMD128SlowShuffle<JavascriptSIMDUint32x4 >(Var src1, Var src2, Var *lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext);
    template Var SIMDUtils::SIMD128SlowShuffle<JavascriptSIMDInt8x16  >(Var src1, Var src2, Var *lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext);
    template Var SIMDUtils::SIMD128SlowShuffle<JavascriptSIMDInt16x8  >(Var src1, Var src2, Var *lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext);
    template Var SIMDUtils::SIMD128SlowShuffle<JavascriptSIMDUint8x16 >(Var src1, Var src2, Var *lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext);
    template Var SIMDUtils::SIMD128SlowShuffle<JavascriptSIMDUint16x8 >(Var src1, Var src2, Var *lanes, const uint32 laneCount, const uint32 range, ScriptContext* scriptContext);

    bool SIMDUtils::SIMDIsSupportedTypedArray(Var value)
    {LOGMEIN("SimdUtils.cpp] 176\n");
        return JavascriptOperators::GetTypeId(value) >= TypeIds_Int8Array && JavascriptOperators::GetTypeId(value) <= TypeIds_Float64Array;
    }

    /*
    Checks if:
    1. Array is supported typed array
    2. Lane index is a Number/TaggedInt and int32 value
    3. Lane index is within array bounds
    */

    SIMDValue* SIMDUtils::SIMDCheckTypedArrayAccess(Var arg1, Var arg2, TypedArrayBase **tarray, int32 *index, uint32 dataWidth, ScriptContext *scriptContext)
    {LOGMEIN("SimdUtils.cpp] 188\n");
        if (!SIMDIsSupportedTypedArray(arg1))
        {LOGMEIN("SimdUtils.cpp] 190\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_SimdInvalidArgType, _u("Simd typed array access"));
        }

        *index = SIMDCheckUint32Number<true>(scriptContext, arg2);

        // bound check
        *tarray = TypedArrayBase::FromVar(arg1);
        uint32 bpe = (*tarray)->GetBytesPerElement();
        int32 offset = (*index) * bpe;
        if (offset < 0 || (offset + dataWidth) >(int32)(*tarray)->GetByteLength())
        {LOGMEIN("SimdUtils.cpp] 201\n");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange, _u("Simd typed array access"));
        }
        return (SIMDValue*)((*tarray)->GetByteBuffer() + offset);
    }

    SIMDValue SIMDUtils::SIMDLdData(const SIMDValue *data, uint8 dataWidth)
    {LOGMEIN("SimdUtils.cpp] 208\n");
        SIMDValue result = { 0, 0, 0, 0 };
        // bitwise copy. Always use integer fields to avoid wrong copy of NaNs.
        switch (dataWidth)
        {LOGMEIN("SimdUtils.cpp] 212\n");
        case 16:
            result.i32[SIMD_W] = data->i32[SIMD_W];
            // fall through
        case 12:
            result.i32[SIMD_Z] = data->i32[SIMD_Z];
            // fall through
        case 8:
            result.i32[SIMD_Y] = data->i32[SIMD_Y];
            // fall through
        case 4:
            result.i32[SIMD_X] = data->i32[SIMD_X];
            break;
        default:
            Assert(UNREACHED);
        }
        return result;
    }

    void SIMDUtils::SIMDStData(SIMDValue *data, const SIMDValue simdValue, uint8 dataWidth)
    {LOGMEIN("SimdUtils.cpp] 232\n");
        // bitwise copy. Always use integer fields to avoid wrong copy of NaNs.
        switch (dataWidth)
        {LOGMEIN("SimdUtils.cpp] 235\n");
        case 16:
            data->i32[SIMD_W] = simdValue.i32[SIMD_W];
            // fall through
        case 12:
            data->i32[SIMD_Z] = simdValue.i32[SIMD_Z];
            // fall through
        case 8:
            data->i32[SIMD_Y] = simdValue.i32[SIMD_Y];
            // fall through
        case 4:
            data->i32[SIMD_X] = simdValue.i32[SIMD_X];
            break;
        default:
            Assert(UNREACHED);
        }
    }

#if ENABLE_NATIVE_CODEGEN
    // Maps Simd opcodes which are non-contiguous to a zero-based linear space. Used to index a table using a Simd opcode.
    uint32 SIMDUtils::SimdOpcodeAsIndex(Js::OpCode op)
    {LOGMEIN("SimdUtils.cpp] 256\n");
        if (op <= Js::OpCode::Simd128_End)
        {LOGMEIN("SimdUtils.cpp] 258\n");
            return (uint32)((Js::OpCode)op - Js::OpCode::Simd128_Start);
        }
        else
        {
            Assert(op >= Js::OpCode::Simd128_Start_Extend && op <= Js::OpCode::Simd128_End_Extend);
            return (uint32)((Js::OpCode)op - Js::OpCode::Simd128_Start_Extend) + (uint32)(Js::OpCode::Simd128_End - Js::OpCode::Simd128_Start) + 1;
        }
    }
#endif
}
