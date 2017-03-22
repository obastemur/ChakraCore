//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)

namespace Js
{
    SIMDValue SIMDInt8x16Operation::OpInt8x16(int8 values[])
    {LOGMEIN("SimdInt8x16Operation.cpp] 12\n");
        SIMDValue result = {0, 0, 0, 0};

        for (uint8 i = 0; i < 16; i++)
        {LOGMEIN("SimdInt8x16Operation.cpp] 16\n");
            result.i8[i] = values[i];
        }
        
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpSplat(int8 x)
    {LOGMEIN("SimdInt8x16Operation.cpp] 24\n");
        SIMDValue result;

        result.i8[0] = result.i8[1] = result.i8[2] = result.i8[3] = result.i8[4] = result.i8[5] = result.i8[6]= result.i8[7] = result.i8[8] = result.i8[9]= result.i8[10] = result.i8[11] = result.i8[12]= result.i8[13] = result.i8[14] = result.i8[15] = x;

        return result;
    }

    //// Unary Ops
    SIMDValue SIMDInt8x16Operation::OpNeg(const SIMDValue& value)
    {LOGMEIN("SimdInt8x16Operation.cpp] 34\n");
        SIMDValue result;

        result.i8[0]  = -1 * value.i8[0];
        result.i8[1]  = -1 * value.i8[1];
        result.i8[2]  = -1 * value.i8[2];
        result.i8[3]  = -1 * value.i8[3];
        result.i8[4]  = -1 * value.i8[4];
        result.i8[5]  = -1 * value.i8[5];
        result.i8[6]  = -1 * value.i8[6];
        result.i8[7]  = -1 * value.i8[7];
        result.i8[8]  = -1 * value.i8[8];
        result.i8[9]  = -1 * value.i8[9];
        result.i8[10] = -1 * value.i8[10];
        result.i8[11] = -1 * value.i8[11];
        result.i8[12] = -1 * value.i8[12];
        result.i8[13] = -1 * value.i8[13];
        result.i8[14] = -1 * value.i8[14];
        result.i8[15] = -1 * value.i8[15];

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpAdd(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 58\n");
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 62\n");
            result.i8[idx] = aValue.i8[idx] + bValue.i8[idx];
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 69\n");
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 73\n");
            result.i8[idx] = aValue.i8[idx] - bValue.i8[idx];
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 81\n");
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 85\n");
            result.i8[idx] = aValue.i8[idx] * bValue.i8[idx];
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpAddSaturate(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 93\n");
        SIMDValue result;
        int mask = 0x80;
        for (uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 97\n");
            int8 val1 = aValue.i8[idx];
            int8 val2 = bValue.i8[idx];
            int8 sum = val1 + val2;

            result.i8[idx] = sum;
            if (val1 > 0 && val2 > 0 && sum < 0)
            {LOGMEIN("SimdInt8x16Operation.cpp] 104\n");
                result.i8[idx] = 0x7F;
            }
            else if (val1 < 0 && val2 < 0 && sum > 0)
            {LOGMEIN("SimdInt8x16Operation.cpp] 108\n");
                result.i8[idx] = static_cast<int8>(mask);
            }
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpSubSaturate(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 116\n");
        SIMDValue result;
        int mask = 0x80;
        for (uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 120\n");
            int8 val1 = aValue.i8[idx];
            int8 val2 = bValue.i8[idx];
            int16 diff = val1 + val2;

            result.i8[idx] = static_cast<int8>(diff);
            if (diff > 0x7F)
            {LOGMEIN("SimdInt8x16Operation.cpp] 127\n");
                result.i8[idx] = 0x7F;
            }
            else if (diff < 0x80)
            {LOGMEIN("SimdInt8x16Operation.cpp] 131\n");
                result.i8[idx] = static_cast<int8>(mask);
            }
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 139\n");
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 143\n");
            result.i8[idx] = (aValue.i8[idx] < bValue.i8[idx]) ? aValue.i8[idx] : bValue.i8[idx];
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 151\n");
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 155\n");
            result.i8[idx] = (aValue.i8[idx] > bValue.i8[idx]) ? aValue.i8[idx] : bValue.i8[idx];
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 163\n");
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 167\n");
            result.i8[idx] = (aValue.i8[idx] < bValue.i8[idx]) ? 0xff : 0x0;
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 174\n");
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 178\n");
            result.i8[idx] = (aValue.i8[idx] <= bValue.i8[idx]) ? 0xff : 0x0;
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 185\n");
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 189\n");
            result.i8[idx] = (aValue.i8[idx] == bValue.i8[idx]) ? 0xff : 0x0;
        }

        return result;
    }


    SIMDValue SIMDInt8x16Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 198\n");
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 202\n");
            result.i8[idx] = (aValue.i8[idx] != bValue.i8[idx]) ? 0xff : 0x0;
        }

        return result;
    }


    SIMDValue SIMDInt8x16Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 211\n");
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 215\n");
            result.i8[idx] = (aValue.i8[idx] > bValue.i8[idx]) ? 0xff : 0x0;
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdInt8x16Operation.cpp] 223\n");
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 227\n");
            result.i8[idx] = (aValue.i8[idx] >= bValue.i8[idx]) ? 0xff : 0x0;
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpShiftLeftByScalar(const SIMDValue& value, int count)
    {LOGMEIN("SimdInt8x16Operation.cpp] 235\n");
        SIMDValue result;

        count = count & SIMDUtils::SIMDGetShiftAmountMask(1);

        for(uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 241\n");
            result.i8[idx] = value.i8[idx] << count;
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpShiftRightByScalar(const SIMDValue& value, int count)
    {LOGMEIN("SimdInt8x16Operation.cpp] 249\n");
        SIMDValue result;

        count = count & SIMDUtils::SIMDGetShiftAmountMask(1);

        for(uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 255\n");
            result.i8[idx] = value.i8[idx] >> count;
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {LOGMEIN("SimdInt8x16Operation.cpp] 263\n");
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {LOGMEIN("SimdInt8x16Operation.cpp] 267\n");
            result.i8[idx] = (mV.i8[idx] == 1) ? tV.i8[idx] : fV.i8[idx];
        }
        return result;
    }

}
#endif
