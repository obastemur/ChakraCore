//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)
namespace Js
{
    SIMDValue SIMDFloat64x2Operation::OpFloat64x2(double x, double y)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 11\n");
        SIMDValue result;

        result.f64[SIMD_X] = x;
        result.f64[SIMD_Y] = y;

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSplat(double x)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 21\n");
        SIMDValue result;

        result.f64[SIMD_X] = result.f64[SIMD_Y] = x;

        return result;
    }

    // Conversions
    SIMDValue SIMDFloat64x2Operation::OpFromFloat32x4(const SIMDValue& v)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 31\n");
        SIMDValue result;

        result.f64[SIMD_X] = (double)(v.f32[SIMD_X]);
        result.f64[SIMD_Y] = (double)(v.f32[SIMD_Y]);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpFromInt32x4(const SIMDValue& v)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 41\n");
        SIMDValue result;

        result.f64[SIMD_X] = (double)(v.i32[SIMD_X]);
        result.f64[SIMD_Y] = (double)(v.i32[SIMD_Y]);

        return result;
    }

    // Unary Ops
    SIMDValue SIMDFloat64x2Operation::OpAbs(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 52\n");
        SIMDValue result;

        result.f64[SIMD_X] = (value.f64[SIMD_X] < 0) ? -1 * value.f64[SIMD_X] : value.f64[SIMD_X];
        result.f64[SIMD_Y] = (value.f64[SIMD_Y] < 0) ? -1 * value.f64[SIMD_Y] : value.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpNeg(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 62\n");
        SIMDValue result;

        result.f64[SIMD_X] = -1 * value.f64[SIMD_X];
        result.f64[SIMD_Y] = -1 * value.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpNot(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 72\n");
        SIMDValue result;

        result = SIMDInt32x4Operation::OpNot(value);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpReciprocal(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 81\n");
        SIMDValue result;

        result.f64[SIMD_X] = 1.0/(value.f64[SIMD_X]);
        result.f64[SIMD_Y] = 1.0/(value.f64[SIMD_Y]);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpReciprocalSqrt(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 91\n");
        SIMDValue result;

        result.f64[SIMD_X] = sqrt(1.0 / (value.f64[SIMD_X]));
        result.f64[SIMD_Y] = sqrt(1.0 / (value.f64[SIMD_Y]));

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSqrt(const SIMDValue& value)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 101\n");
        SIMDValue result;

        result.f64[SIMD_X] = sqrt(value.f64[SIMD_X]);
        result.f64[SIMD_Y] = sqrt(value.f64[SIMD_Y]);

        return result;
    }

    // Binary Ops
    SIMDValue SIMDFloat64x2Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 112\n");
        SIMDValue result;

        result.f64[SIMD_X] = aValue.f64[SIMD_X] + bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = aValue.f64[SIMD_Y] + bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 122\n");
        SIMDValue result;

        result.f64[SIMD_X] = aValue.f64[SIMD_X] - bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = aValue.f64[SIMD_Y] - bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 132\n");
        SIMDValue result;

        result.f64[SIMD_X] = aValue.f64[SIMD_X] * bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = aValue.f64[SIMD_Y] * bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpDiv(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 142\n");
        SIMDValue result;

        result.f64[SIMD_X] = aValue.f64[SIMD_X] / bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = aValue.f64[SIMD_Y] / bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 152\n");
        SIMDValue result;

        result = SIMDInt32x4Operation::OpAnd(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 161\n");
        SIMDValue result;

        result = SIMDInt32x4Operation::OpOr(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 170\n");
        SIMDValue result;

        result = SIMDInt32x4Operation::OpXor(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 179\n");
        SIMDValue result;

        result.f64[SIMD_X] = (aValue.f64[SIMD_X] < bValue.f64[SIMD_X]) ? aValue.f64[SIMD_X] : bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = (aValue.f64[SIMD_Y] < bValue.f64[SIMD_Y]) ? aValue.f64[SIMD_Y] : bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 189\n");
        SIMDValue result;

        result.f64[SIMD_X] = (aValue.f64[SIMD_X] > bValue.f64[SIMD_X]) ? aValue.f64[SIMD_X] : bValue.f64[SIMD_X];
        result.f64[SIMD_Y] = (aValue.f64[SIMD_Y] > bValue.f64[SIMD_Y]) ? aValue.f64[SIMD_Y] : bValue.f64[SIMD_Y];

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpScale(const SIMDValue& Value, double scaleValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 199\n");
        SIMDValue result;

        result.f64[SIMD_X] = Value.f64[SIMD_X] * scaleValue;
        result.f64[SIMD_Y] = Value.f64[SIMD_Y] * scaleValue;

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 209\n");
        SIMDValue result;

        int x = aValue.f64[SIMD_X] < bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] < bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 221\n");
        SIMDValue result;

        int x = aValue.f64[SIMD_X] <= bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] <= bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 233\n");
        SIMDValue result;

        int x = aValue.f64[SIMD_X] == bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] == bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 245\n");
        SIMDValue result;

        int x = aValue.f64[SIMD_X] != bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] != bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }


    SIMDValue SIMDFloat64x2Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 258\n");
        SIMDValue result;

        int x = aValue.f64[SIMD_X] > bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] > bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 270\n");
        SIMDValue result;

        int x = aValue.f64[SIMD_X] >= bValue.f64[SIMD_X];
        int y = aValue.f64[SIMD_Y] >= bValue.f64[SIMD_Y];

        result = SIMDInt32x4Operation::OpBool(x, x, y, y);

        return result;
    }

    SIMDValue SIMDFloat64x2Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {LOGMEIN("SimdFloat64x2Operation.cpp] 282\n");
        SIMDValue result;

        SIMDValue trueResult  = SIMDInt32x4Operation::OpAnd(mV, tV);
        SIMDValue notValue    = SIMDInt32x4Operation::OpNot(mV);
        SIMDValue falseResult = SIMDInt32x4Operation::OpAnd(notValue, fV);

        result = SIMDInt32x4Operation::OpOr(trueResult, falseResult);

        return result;
    }
}
#endif
