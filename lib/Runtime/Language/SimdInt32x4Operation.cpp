//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)

namespace Js
{
    SIMDValue SIMDInt32x4Operation::OpInt32x4(int x, int y, int z, int w)
    {LOGMEIN("SimdInt32x4Operation.cpp] 12\n");
        SIMDValue result;

        result.i32[SIMD_X] = x;
        result.i32[SIMD_Y] = y;
        result.i32[SIMD_Z] = z;
        result.i32[SIMD_W] = w;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpSplat(int x)
    {LOGMEIN("SimdInt32x4Operation.cpp] 24\n");
        SIMDValue result;

        result.i32[SIMD_X] = result.i32[SIMD_Y] = result.i32[SIMD_Z] = result.i32[SIMD_W] = x;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpBool(int x, int y, int z, int w)
    {LOGMEIN("SimdInt32x4Operation.cpp] 33\n");
        SIMDValue result;

        int nX = x ? -1 : 0x0;
        int nY = y ? -1 : 0x0;
        int nZ = z ? -1 : 0x0;
        int nW = w ? -1 : 0x0;

        result.i32[SIMD_X] = nX;
        result.i32[SIMD_Y] = nY;
        result.i32[SIMD_Z] = nZ;
        result.i32[SIMD_W] = nW;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpBool(const SIMDValue& v)
    {LOGMEIN("SimdInt32x4Operation.cpp] 50\n");
        SIMDValue result;

        // incoming 4 signed integers has to be 0 or -1
        Assert(v.i32[SIMD_X] == 0 || v.i32[SIMD_X] == -1);
        Assert(v.i32[SIMD_Y] == 0 || v.i32[SIMD_Y] == -1);
        Assert(v.i32[SIMD_Z] == 0 || v.i32[SIMD_Z] == -1);
        Assert(v.i32[SIMD_W] == 0 || v.i32[SIMD_W] == -1);
        result = v;
        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpFromFloat32x4(const SIMDValue& v, bool &throws)
    {LOGMEIN("SimdInt32x4Operation.cpp] 63\n");
        SIMDValue result = { 0 };
        const int MIN_INT = 0x80000000, MAX_INT = 0x7FFFFFFF;

        for (uint i = 0; i < 4; i++)
        {LOGMEIN("SimdInt32x4Operation.cpp] 68\n");
            if (v.f32[i] >= MIN_INT && v.f32[i] <= MAX_INT)
            {LOGMEIN("SimdInt32x4Operation.cpp] 70\n");
                result.u32[i] = (int)(v.f32[i]);
            }
            else
            {
                // out of range. Caller should throw.
                throws = true;
                return result;
            }
        }
        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpFromFloat64x2(const SIMDValue& v)
    {LOGMEIN("SimdInt32x4Operation.cpp] 84\n");
        SIMDValue result;

        result.i32[SIMD_X] = (int)(v.f64[SIMD_X]);
        result.i32[SIMD_Y] = (int)(v.f64[SIMD_Y]);
        result.i32[SIMD_Z] = result.i32[SIMD_W] = 0;

        return result;
    }


    // Unary Ops
    SIMDValue SIMDInt32x4Operation::OpAbs(const SIMDValue& value)
    {LOGMEIN("SimdInt32x4Operation.cpp] 97\n");
        SIMDValue result;

        result.i32[SIMD_X] = (value.i32[SIMD_X] < 0) ? -1 * value.i32[SIMD_X] : value.i32[SIMD_X];
        result.i32[SIMD_Y] = (value.i32[SIMD_Y] < 0) ? -1 * value.i32[SIMD_Y] : value.i32[SIMD_Y];
        result.i32[SIMD_Z] = (value.i32[SIMD_Z] < 0) ? -1 * value.i32[SIMD_Z] : value.i32[SIMD_Z];
        result.i32[SIMD_W] = (value.i32[SIMD_W] < 0) ? -1 * value.i32[SIMD_W] : value.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpNeg(const SIMDValue& value)
    {LOGMEIN("SimdInt32x4Operation.cpp] 109\n");
        SIMDValue result;

        result.i32[SIMD_X] = -1 * value.i32[SIMD_X];
        result.i32[SIMD_Y] = -1 * value.i32[SIMD_Y];
        result.i32[SIMD_Z] = -1 * value.i32[SIMD_Z];
        result.i32[SIMD_W] = -1 * value.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpNot(const SIMDValue& value)
    {LOGMEIN("SimdInt32x4Operation.cpp] 121\n");
        SIMDValue result;

        result.i32[SIMD_X] = ~(value.i32[SIMD_X]);
        result.i32[SIMD_Y] = ~(value.i32[SIMD_Y]);
        result.i32[SIMD_Z] = ~(value.i32[SIMD_Z]);
        result.i32[SIMD_W] = ~(value.i32[SIMD_W]);

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 133\n");
        SIMDValue result;

        result.i32[SIMD_X] = aValue.i32[SIMD_X] + bValue.i32[SIMD_X];
        result.i32[SIMD_Y] = aValue.i32[SIMD_Y] + bValue.i32[SIMD_Y];
        result.i32[SIMD_Z] = aValue.i32[SIMD_Z] + bValue.i32[SIMD_Z];
        result.i32[SIMD_W] = aValue.i32[SIMD_W] + bValue.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 145\n");
        SIMDValue result;

        result.i32[SIMD_X] = aValue.i32[SIMD_X] - bValue.i32[SIMD_X];
        result.i32[SIMD_Y] = aValue.i32[SIMD_Y] - bValue.i32[SIMD_Y];
        result.i32[SIMD_Z] = aValue.i32[SIMD_Z] - bValue.i32[SIMD_Z];
        result.i32[SIMD_W] = aValue.i32[SIMD_W] - bValue.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 157\n");
        SIMDValue result;

        result.i32[SIMD_X] = aValue.i32[SIMD_X] * bValue.i32[SIMD_X];
        result.i32[SIMD_Y] = aValue.i32[SIMD_Y] * bValue.i32[SIMD_Y];
        result.i32[SIMD_Z] = aValue.i32[SIMD_Z] * bValue.i32[SIMD_Z];
        result.i32[SIMD_W] = aValue.i32[SIMD_W] * bValue.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpAnd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 169\n");
        SIMDValue result;

        result.i32[SIMD_X] = aValue.i32[SIMD_X] & bValue.i32[SIMD_X];
        result.i32[SIMD_Y] = aValue.i32[SIMD_Y] & bValue.i32[SIMD_Y];
        result.i32[SIMD_Z] = aValue.i32[SIMD_Z] & bValue.i32[SIMD_Z];
        result.i32[SIMD_W] = aValue.i32[SIMD_W] & bValue.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpOr(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 181\n");
        SIMDValue result;

        result.i32[SIMD_X] = aValue.i32[SIMD_X] | bValue.i32[SIMD_X];
        result.i32[SIMD_Y] = aValue.i32[SIMD_Y] | bValue.i32[SIMD_Y];
        result.i32[SIMD_Z] = aValue.i32[SIMD_Z] | bValue.i32[SIMD_Z];
        result.i32[SIMD_W] = aValue.i32[SIMD_W] | bValue.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpXor(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 193\n");
        SIMDValue result;

        result.i32[SIMD_X] = aValue.i32[SIMD_X] ^ bValue.i32[SIMD_X];
        result.i32[SIMD_Y] = aValue.i32[SIMD_Y] ^ bValue.i32[SIMD_Y];
        result.i32[SIMD_Z] = aValue.i32[SIMD_Z] ^ bValue.i32[SIMD_Z];
        result.i32[SIMD_W] = aValue.i32[SIMD_W] ^ bValue.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 205\n");
        SIMDValue result;

        result.i32[SIMD_X] = (aValue.i32[SIMD_X] < bValue.i32[SIMD_X]) ? aValue.i32[SIMD_X] : bValue.i32[SIMD_X];
        result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] < bValue.i32[SIMD_Y]) ? aValue.i32[SIMD_Y] : bValue.i32[SIMD_Y];
        result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] < bValue.i32[SIMD_Z]) ? aValue.i32[SIMD_Z] : bValue.i32[SIMD_Z];
        result.i32[SIMD_W] = (aValue.i32[SIMD_W] < bValue.i32[SIMD_W]) ? aValue.i32[SIMD_W] : bValue.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 217\n");
        SIMDValue result;

        result.i32[SIMD_X] = (aValue.i32[SIMD_X] > bValue.i32[SIMD_X]) ? aValue.i32[SIMD_X] : bValue.i32[SIMD_X];
        result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] > bValue.i32[SIMD_Y]) ? aValue.i32[SIMD_Y] : bValue.i32[SIMD_Y];
        result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] > bValue.i32[SIMD_Z]) ? aValue.i32[SIMD_Z] : bValue.i32[SIMD_Z];
        result.i32[SIMD_W] = (aValue.i32[SIMD_W] > bValue.i32[SIMD_W]) ? aValue.i32[SIMD_W] : bValue.i32[SIMD_W];

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 229\n");
        SIMDValue result;

        result.i32[SIMD_X] = (aValue.i32[SIMD_X] < bValue.i32[SIMD_X]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] < bValue.i32[SIMD_Y]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] < bValue.i32[SIMD_Z]) ? 0xffffffff : 0x0;
        result.i32[SIMD_W] = (aValue.i32[SIMD_W] < bValue.i32[SIMD_W]) ? 0xffffffff : 0x0;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 241\n");
        SIMDValue result;

        result.i32[SIMD_X] = (aValue.i32[SIMD_X] <= bValue.i32[SIMD_X]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] <= bValue.i32[SIMD_Y]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] <= bValue.i32[SIMD_Z]) ? 0xffffffff : 0x0;
        result.i32[SIMD_W] = (aValue.i32[SIMD_W] <= bValue.i32[SIMD_W]) ? 0xffffffff : 0x0;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 253\n");
        SIMDValue result;

        result.i32[SIMD_X] = (aValue.i32[SIMD_X] == bValue.i32[SIMD_X]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] == bValue.i32[SIMD_Y]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] == bValue.i32[SIMD_Z]) ? 0xffffffff : 0x0;
        result.i32[SIMD_W] = (aValue.i32[SIMD_W] == bValue.i32[SIMD_W]) ? 0xffffffff : 0x0;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 265\n");
        SIMDValue result;

        result.i32[SIMD_X] = (aValue.i32[SIMD_X] != bValue.i32[SIMD_X]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] != bValue.i32[SIMD_Y]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] != bValue.i32[SIMD_Z]) ? 0xffffffff : 0x0;
        result.i32[SIMD_W] = (aValue.i32[SIMD_W] != bValue.i32[SIMD_W]) ? 0xffffffff : 0x0;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 277\n");
        SIMDValue result;

        result.i32[SIMD_X] = (aValue.i32[SIMD_X] > bValue.i32[SIMD_X]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] > bValue.i32[SIMD_Y]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] > bValue.i32[SIMD_Z]) ? 0xffffffff : 0x0;
        result.i32[SIMD_W] = (aValue.i32[SIMD_W] > bValue.i32[SIMD_W]) ? 0xffffffff : 0x0;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt32x4Operation.cpp] 289\n");
        SIMDValue result;

        result.i32[SIMD_X] = (aValue.i32[SIMD_X] >= bValue.i32[SIMD_X]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Y] = (aValue.i32[SIMD_Y] >= bValue.i32[SIMD_Y]) ? 0xffffffff : 0x0;
        result.i32[SIMD_Z] = (aValue.i32[SIMD_Z] >= bValue.i32[SIMD_Z]) ? 0xffffffff : 0x0;
        result.i32[SIMD_W] = (aValue.i32[SIMD_W] >= bValue.i32[SIMD_W]) ? 0xffffffff : 0x0;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpShiftLeftByScalar(const SIMDValue& value, int count)
    {LOGMEIN("SimdInt32x4Operation.cpp] 301\n");
        SIMDValue result;

        count = count & SIMDUtils::SIMDGetShiftAmountMask(4);

        result.i32[SIMD_X] = value.i32[SIMD_X] << count;
        result.i32[SIMD_Y] = value.i32[SIMD_Y] << count;
        result.i32[SIMD_Z] = value.i32[SIMD_Z] << count;
        result.i32[SIMD_W] = value.i32[SIMD_W] << count;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpShiftRightByScalar(const SIMDValue& value, int count)
    {LOGMEIN("SimdInt32x4Operation.cpp] 315\n");
        SIMDValue result;

        count = count & SIMDUtils::SIMDGetShiftAmountMask(4);

        result.i32[SIMD_X] = value.i32[SIMD_X] >> count;
        result.i32[SIMD_Y] = value.i32[SIMD_Y] >> count;
        result.i32[SIMD_Z] = value.i32[SIMD_Z] >> count;
        result.i32[SIMD_W] = value.i32[SIMD_W] >> count;

        return result;
    }

    SIMDValue SIMDInt32x4Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {LOGMEIN("SimdInt32x4Operation.cpp] 329\n");
        SIMDValue result;

        SIMDValue trueResult  = SIMDInt32x4Operation::OpAnd(mV, tV);
        SIMDValue notValue    = SIMDInt32x4Operation::OpNot(mV);
        SIMDValue falseResult = SIMDInt32x4Operation::OpAnd(notValue, fV);

        result = SIMDInt32x4Operation::OpOr(trueResult, falseResult);

        return result;
    }
}
#endif
