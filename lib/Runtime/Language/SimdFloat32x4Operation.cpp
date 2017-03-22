//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)
namespace Js
{
    SIMDValue SIMDFloat32x4Operation::OpFloat32x4(float x, float y, float z, float w)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 11\n");
        SIMDValue result;

        result.f32[SIMD_X] = x;
        result.f32[SIMD_Y] = y;
        result.f32[SIMD_Z] = z;
        result.f32[SIMD_W] = w;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpSplat(float x)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 23\n");
        SIMDValue result;

        result.f32[SIMD_X] = result.f32[SIMD_Y] = result.f32[SIMD_Z] = result.f32[SIMD_W] = x;

        return result;
    }

    // Conversions
    SIMDValue SIMDFloat32x4Operation::OpFromFloat64x2(const SIMDValue& v)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 33\n");
        SIMDValue result;

        result.f32[SIMD_X] = (float)(v.f64[SIMD_X]);
        result.f32[SIMD_Y] = (float)(v.f64[SIMD_Y]);
        result.f32[SIMD_Z] = result.f32[SIMD_W] = 0;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpFromInt32x4(const SIMDValue& v)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 44\n");
        SIMDValue result;

        result.f32[SIMD_X] = (float)(v.i32[SIMD_X]);
        result.f32[SIMD_Y] = (float)(v.i32[SIMD_Y]);
        result.f32[SIMD_Z] = (float)(v.i32[SIMD_Z]);
        result.f32[SIMD_W] = (float)(v.i32[SIMD_W]);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpFromUint32x4(const SIMDValue& v)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 56\n");
        SIMDValue result;

        result.f32[SIMD_X] = (float)(v.u32[SIMD_X]);
        result.f32[SIMD_Y] = (float)(v.u32[SIMD_Y]);
        result.f32[SIMD_Z] = (float)(v.u32[SIMD_Z]);
        result.f32[SIMD_W] = (float)(v.u32[SIMD_W]);

        return result;
    }

    // Unary Ops
    SIMDValue SIMDFloat32x4Operation::OpAbs(const SIMDValue& value)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 69\n");
        SIMDValue result;

        result.f32[SIMD_X] = (value.f32[SIMD_X] < 0) ? -1 * value.f32[SIMD_X] : value.f32[SIMD_X];
        result.f32[SIMD_Y] = (value.f32[SIMD_Y] < 0) ? -1 * value.f32[SIMD_Y] : value.f32[SIMD_Y];
        result.f32[SIMD_Z] = (value.f32[SIMD_Z] < 0) ? -1 * value.f32[SIMD_Z] : value.f32[SIMD_Z];
        result.f32[SIMD_W] = (value.f32[SIMD_W] < 0) ? -1 * value.f32[SIMD_W] : value.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpNeg(const SIMDValue& value)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 81\n");
        SIMDValue result;

        result.f32[SIMD_X] = -1 * value.f32[SIMD_X];
        result.f32[SIMD_Y] = -1 * value.f32[SIMD_Y];
        result.f32[SIMD_Z] = -1 * value.f32[SIMD_Z];
        result.f32[SIMD_W] = -1 * value.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpNot(const SIMDValue& value)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 93\n");
        SIMDValue result;

        result = SIMDInt32x4Operation::OpNot(value);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpReciprocal(const SIMDValue& value)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 102\n");
        SIMDValue result;

        result.f32[SIMD_X] = (float)(1.0 / (value.f32[SIMD_X]));
        result.f32[SIMD_Y] = (float)(1.0 / (value.f32[SIMD_Y]));
        result.f32[SIMD_Z] = (float)(1.0 / (value.f32[SIMD_Z]));
        result.f32[SIMD_W] = (float)(1.0 / (value.f32[SIMD_W]));

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpReciprocalSqrt(const SIMDValue& value)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 114\n");
        SIMDValue result;

        result.f32[SIMD_X] = (float)sqrt(1.0 / (value.f32[SIMD_X]));
        result.f32[SIMD_Y] = (float)sqrt(1.0 / (value.f32[SIMD_Y]));
        result.f32[SIMD_Z] = (float)sqrt(1.0 / (value.f32[SIMD_Z]));
        result.f32[SIMD_W] = (float)sqrt(1.0 / (value.f32[SIMD_W]));

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpSqrt(const SIMDValue& value)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 126\n");
        SIMDValue result;

        result.f32[SIMD_X] = sqrtf(value.f32[SIMD_X]);
        result.f32[SIMD_Y] = sqrtf(value.f32[SIMD_Y]);
        result.f32[SIMD_Z] = sqrtf(value.f32[SIMD_Z]);
        result.f32[SIMD_W] = sqrtf(value.f32[SIMD_W]);

        return result;
    }

    // Binary Ops
    SIMDValue SIMDFloat32x4Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 139\n");
        SIMDValue result;

        result.f32[SIMD_X] = aValue.f32[SIMD_X] + bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = aValue.f32[SIMD_Y] + bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = aValue.f32[SIMD_Z] + bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = aValue.f32[SIMD_W] + bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 151\n");
        SIMDValue result;

        result.f32[SIMD_X] = aValue.f32[SIMD_X] - bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = aValue.f32[SIMD_Y] - bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = aValue.f32[SIMD_Z] - bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = aValue.f32[SIMD_W] - bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 163\n");
        SIMDValue result;

        result.f32[SIMD_X] = aValue.f32[SIMD_X] * bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = aValue.f32[SIMD_Y] * bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = aValue.f32[SIMD_Z] * bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = aValue.f32[SIMD_W] * bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpDiv(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 175\n");
        SIMDValue result;

        result.f32[SIMD_X] = aValue.f32[SIMD_X] / bValue.f32[SIMD_X];
        result.f32[SIMD_Y] = aValue.f32[SIMD_Y] / bValue.f32[SIMD_Y];
        result.f32[SIMD_Z] = aValue.f32[SIMD_Z] / bValue.f32[SIMD_Z];
        result.f32[SIMD_W] = aValue.f32[SIMD_W] / bValue.f32[SIMD_W];

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 187\n");
        SIMDValue result;

        result = SIMDInt32x4Operation::OpAnd(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 196\n");
        SIMDValue result;

        result = SIMDInt32x4Operation::OpOr(aValue, bValue);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 205\n");
        SIMDValue result;

        result = SIMDInt32x4Operation::OpXor(aValue, bValue);

        return result;
    }

    /*
    Min/Max(a, b) spec semantics:
    If any value is NaN, return NaN
    a < b ? a : b; where +0.0 > -0.0 (vice versa for Max)
    */
    SIMDValue SIMDFloat32x4Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 219\n");
        SIMDValue result;
        for (uint i = 0; i < 4; i++)
        {LOGMEIN("SimdFloat32x4Operation.cpp] 222\n");
            float a = aValue.f32[i];
            float b = bValue.f32[i];
            if (Js::NumberUtilities::IsNan(a))
            {LOGMEIN("SimdFloat32x4Operation.cpp] 226\n");
                result.f32[i] = a;
            }
            else if (Js::NumberUtilities::IsNan(b))
            {LOGMEIN("SimdFloat32x4Operation.cpp] 230\n");
                result.f32[i] = b;
            }
            else if (Js::NumberUtilities::IsFloat32NegZero(a) && b >= 0.0)
            {LOGMEIN("SimdFloat32x4Operation.cpp] 234\n");
                result.f32[i] = a;
            }
            else if (Js::NumberUtilities::IsFloat32NegZero(b) && a >= 0.0)
            {LOGMEIN("SimdFloat32x4Operation.cpp] 238\n");
                result.f32[i] = b;
            }
            else
            {
                result.f32[i] = a < b ? a : b;
            }
        }
        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 250\n");
        SIMDValue result;
        for (uint i = 0; i < 4; i++)
        {LOGMEIN("SimdFloat32x4Operation.cpp] 253\n");
            float a = aValue.f32[i];
            float b = bValue.f32[i];
            if (Js::NumberUtilities::IsNan(a))
            {LOGMEIN("SimdFloat32x4Operation.cpp] 257\n");
                result.f32[i] = a;
            }
            else if (Js::NumberUtilities::IsNan(b))
            {LOGMEIN("SimdFloat32x4Operation.cpp] 261\n");
                result.f32[i] = b;
            }
            else if (Js::NumberUtilities::IsFloat32NegZero(a) && b >= 0.0)
            {LOGMEIN("SimdFloat32x4Operation.cpp] 265\n");
                result.f32[i] = b;
            }
            else if (Js::NumberUtilities::IsFloat32NegZero(b) && a >= 0.0)
            {LOGMEIN("SimdFloat32x4Operation.cpp] 269\n");
                result.f32[i] = a;
            }
            else
            {
                result.f32[i] = a < b ? b : a;
            }
        }
        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpScale(const SIMDValue& Value, float scaleValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 281\n");
        SIMDValue result;

        result.f32[SIMD_X] = Value.f32[SIMD_X] * scaleValue;
        result.f32[SIMD_Y] = Value.f32[SIMD_Y] * scaleValue;
        result.f32[SIMD_Z] = Value.f32[SIMD_Z] * scaleValue;
        result.f32[SIMD_W] = Value.f32[SIMD_W] * scaleValue;

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 293\n");
        SIMDValue result;

        int x = aValue.f32[SIMD_X] < bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] < bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] < bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] < bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 307\n");
        SIMDValue result;

        int x = aValue.f32[SIMD_X] <= bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] <= bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] <= bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] <= bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 321\n");
        SIMDValue result;

        int x = aValue.f32[SIMD_X] == bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] == bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] == bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] == bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 335\n");
        SIMDValue result;

        int x = aValue.f32[SIMD_X] != bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] != bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] != bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] != bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 349\n");
        SIMDValue result;

        int x = aValue.f32[SIMD_X] > bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] > bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] > bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] > bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 363\n");
        SIMDValue result;

        int x = aValue.f32[SIMD_X] >= bValue.f32[SIMD_X];
        int y = aValue.f32[SIMD_Y] >= bValue.f32[SIMD_Y];
        int z = aValue.f32[SIMD_Z] >= bValue.f32[SIMD_Z];
        int w = aValue.f32[SIMD_W] >= bValue.f32[SIMD_W];

        result = SIMDInt32x4Operation::OpBool(x, y, z, w);

        return result;
    }

    SIMDValue SIMDFloat32x4Operation::OpClamp(const SIMDValue& value, const SIMDValue& lower, const SIMDValue& upper)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 377\n");
        SIMDValue result;

        // lower clamp
        result.f32[SIMD_X] = value.f32[SIMD_X] < lower.f32[SIMD_X] ? lower.f32[SIMD_X] : value.f32[SIMD_X];
        result.f32[SIMD_Y] = value.f32[SIMD_Y] < lower.f32[SIMD_Y] ? lower.f32[SIMD_Y] : value.f32[SIMD_Y];
        result.f32[SIMD_Z] = value.f32[SIMD_Z] < lower.f32[SIMD_Z] ? lower.f32[SIMD_Z] : value.f32[SIMD_Z];
        result.f32[SIMD_W] = value.f32[SIMD_W] < lower.f32[SIMD_W] ? lower.f32[SIMD_W] : value.f32[SIMD_W];

        // upper clamp
        result.f32[SIMD_X] = result.f32[SIMD_X] > upper.f32[SIMD_X] ? upper.f32[SIMD_X] : result.f32[SIMD_X];
        result.f32[SIMD_Y] = result.f32[SIMD_Y] > upper.f32[SIMD_Y] ? upper.f32[SIMD_Y] : result.f32[SIMD_Y];
        result.f32[SIMD_Z] = result.f32[SIMD_Z] > upper.f32[SIMD_Z] ? upper.f32[SIMD_Z] : result.f32[SIMD_Z];
        result.f32[SIMD_W] = result.f32[SIMD_W] > upper.f32[SIMD_W] ? upper.f32[SIMD_W] : result.f32[SIMD_W];

        return result;
    }


    SIMDValue SIMDFloat32x4Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {LOGMEIN("SimdFloat32x4Operation.cpp] 397\n");
        SIMDValue result;

        SIMDValue trueResult  = SIMDInt32x4Operation::OpAnd(mV, tV);
        SIMDValue notValue    = SIMDInt32x4Operation::OpNot(mV);
        SIMDValue falseResult = SIMDInt32x4Operation::OpAnd(notValue, fV);

        result = SIMDInt32x4Operation::OpOr(trueResult, falseResult);

        return result;
    }

}
#endif
