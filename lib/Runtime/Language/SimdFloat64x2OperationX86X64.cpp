//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if _M_IX86 || _M_AMD64

namespace Js
{
    SIMDValue SIMDFloat64x2Operation::OpFloat64x2(double x, double y)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 12\n");
        X86SIMDValue x86Result;

        // Sets the lower double-precision, floating-point value to x
        // and sets the upper double-precision, floating-point value to y.
        x86Result.m128d_value = _mm_set_pd(y, x);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpSplat(double x)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 23\n");
        X86SIMDValue x86Result;
        // Sets the 2 double-precision, floating-point values to x
        x86Result.m128d_value = _mm_set1_pd(x);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    // Conversions
    SIMDValue SIMDFloat64x2Operation::OpFromFloat32x4(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 33\n");
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // Converts the lower 2 single-precision, floating-point values
        // to two double-precision, floating-point values
        x86Result.m128d_value = _mm_cvtps_pd(v.m128_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpFromInt32x4(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 45\n");
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        // Converts the lower 2 signed 32-bit integer values of
        // to double-precision, floating-point values
        x86Result.m128d_value = _mm_cvtepi32_pd(v.m128i_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    // Unary Ops
    SIMDValue SIMDFloat64x2Operation::OpAbs(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 58\n");
        X86SIMDValue x86Result, SIGNMASK;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        SIGNMASK.m128d_value = _mm_castsi128_pd(_mm_set_epi32(0x7fffffff, 0xffffffff, 0x7fffffff, 0xffffffff));
        x86Result.m128d_value = _mm_and_pd(v.m128d_value, SIGNMASK.m128d_value); // v & SIGNMASK

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpNeg(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 69\n");
        X86SIMDValue x86Result;

        X86SIMDValue SIGNMASK;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        SIGNMASK.m128d_value = _mm_castsi128_pd(_mm_set_epi32(0x80000000, 0x0, 0x80000000, 0x0));
        x86Result.m128d_value = _mm_xor_pd(v.m128d_value, SIGNMASK.m128d_value); // v ^ mask

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpNot(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 82\n");
        X86SIMDValue x86Result;

        X86SIMDValue negativeOnes = { { -1, -1, -1, -1 } };
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        x86Result.m128d_value = _mm_xor_pd(v.m128d_value, negativeOnes.m128d_value); // v ^ -1

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpReciprocal(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 94\n");
        X86SIMDValue x86Result;

        X86SIMDValue doubleOnes;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        doubleOnes.m128d_value = _mm_set_pd(1.0, 1.0);
        x86Result.m128d_value = _mm_div_pd(doubleOnes.m128d_value, v.m128d_value); // result = 1.0/value

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpReciprocalSqrt(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 107\n");
        X86SIMDValue x86Result;

        X86SIMDValue doubleOnes, temp;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        doubleOnes.m128d_value = _mm_set_pd(1.0, 1.0);
        temp.m128d_value = _mm_div_pd(doubleOnes.m128d_value, v.m128d_value); // temp = 1.0/value
        x86Result.m128d_value = _mm_sqrt_pd(temp.m128d_value); // result = sqrt(1.0/value)

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpSqrt(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 121\n");
        X86SIMDValue x86Result;

        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(value);

        x86Result.m128d_value = _mm_sqrt_pd(v.m128d_value); // result = sqrt(value)

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    // Binary Ops
    SIMDValue SIMDFloat64x2Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 133\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128d_value = _mm_add_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a + b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 144\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128d_value = _mm_sub_pd(tmpaValue.m128d_value, tmpbValue.m128d_value);  // a - b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 155\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128d_value = _mm_mul_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a * b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpDiv(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 166\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128d_value = _mm_div_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a / b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 177\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128d_value = _mm_and_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a & b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 188\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128d_value = _mm_or_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a | b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 199\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        x86Result.m128d_value = _mm_xor_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a ^ b

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 210\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        // choose the smaller value of the two parameters a and b
        x86Result.m128d_value = _mm_min_pd(tmpaValue.m128d_value, tmpbValue.m128d_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 222\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);

        // choose the larger value of the two parameters a and b
        x86Result.m128d_value = _mm_max_pd(tmpaValue.m128d_value, tmpbValue.m128d_value);

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpScale(const SIMDValue& Value, double scaleValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 234\n");
        X86SIMDValue x86Result;
        X86SIMDValue v = X86SIMDValue::ToX86SIMDValue(Value);

        X86SIMDValue scaleVector;
        scaleVector.m128d_value = _mm_set1_pd(scaleValue);
        x86Result.m128d_value = _mm_mul_pd(v.m128d_value, scaleVector.m128d_value); // v * scale

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 246\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128d_value = _mm_cmplt_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a < b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 256\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128d_value = _mm_cmple_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a <= b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 266\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128d_value = _mm_cmpeq_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a == b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 276\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128d_value = _mm_cmpneq_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a != b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 286\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128d_value = _mm_cmpgt_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a > b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 296\n");
        X86SIMDValue x86Result;
        X86SIMDValue tmpaValue = X86SIMDValue::ToX86SIMDValue(aValue);
        X86SIMDValue tmpbValue = X86SIMDValue::ToX86SIMDValue(bValue);
        x86Result.m128d_value = _mm_cmpge_pd(tmpaValue.m128d_value, tmpbValue.m128d_value); // a >= b?

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDFloat64x2Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {LOGMEIN("SimdFloat64x2OperationX86X64.cpp] 306\n");
        X86SIMDValue x86Result;
        X86SIMDValue maskValue  = X86SIMDValue::ToX86SIMDValue(mV);
        X86SIMDValue trueValue  = X86SIMDValue::ToX86SIMDValue(tV);
        X86SIMDValue falseValue = X86SIMDValue::ToX86SIMDValue(fV);

        X86SIMDValue tempTrue, tempFalse;
        tempTrue.m128d_value = _mm_and_pd(maskValue.m128d_value, trueValue.m128d_value);      // mask & True
        tempFalse.m128d_value = _mm_andnot_pd(maskValue.m128d_value, falseValue.m128d_value); // !mask & False
        x86Result.m128d_value = _mm_or_pd(tempTrue.m128d_value, tempFalse.m128d_value); // tempTrue | tempFalse

        return X86SIMDValue::ToSIMDValue(x86Result);
    }

}

#endif
