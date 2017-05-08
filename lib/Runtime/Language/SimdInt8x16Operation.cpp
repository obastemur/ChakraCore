//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)

namespace Js
{
    SIMDValue SIMDInt8x16Operation::OpInt8x16(int8 values[])
    {TRACE_IT(52425);
        SIMDValue result = {0, 0, 0, 0};

        for (uint8 i = 0; i < 16; i++)
        {TRACE_IT(52426);
            result.i8[i] = values[i];
        }
        
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpSplat(int8 x)
    {TRACE_IT(52427);
        SIMDValue result;

        result.i8[0] = result.i8[1] = result.i8[2] = result.i8[3] = result.i8[4] = result.i8[5] = result.i8[6]= result.i8[7] = result.i8[8] = result.i8[9]= result.i8[10] = result.i8[11] = result.i8[12]= result.i8[13] = result.i8[14] = result.i8[15] = x;

        return result;
    }

    //// Unary Ops
    SIMDValue SIMDInt8x16Operation::OpNeg(const SIMDValue& value)
    {TRACE_IT(52428);
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
    {TRACE_IT(52429);
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52430);
            result.i8[idx] = aValue.i8[idx] + bValue.i8[idx];
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpSub(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52431);
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52432);
            result.i8[idx] = aValue.i8[idx] - bValue.i8[idx];
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpMul(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52433);
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52434);
            result.i8[idx] = aValue.i8[idx] * bValue.i8[idx];
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpAddSaturate(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52435);
        SIMDValue result;
        int mask = 0x80;
        for (uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52436);
            int8 val1 = aValue.i8[idx];
            int8 val2 = bValue.i8[idx];
            int8 sum = val1 + val2;

            result.i8[idx] = sum;
            if (val1 > 0 && val2 > 0 && sum < 0)
            {TRACE_IT(52437);
                result.i8[idx] = 0x7F;
            }
            else if (val1 < 0 && val2 < 0 && sum > 0)
            {TRACE_IT(52438);
                result.i8[idx] = static_cast<int8>(mask);
            }
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpSubSaturate(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52439);
        SIMDValue result;
        int mask = 0x80;
        for (uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52440);
            int8 val1 = aValue.i8[idx];
            int8 val2 = bValue.i8[idx];
            int16 diff = val1 + val2;

            result.i8[idx] = static_cast<int8>(diff);
            if (diff > 0x7F)
            {TRACE_IT(52441);
                result.i8[idx] = 0x7F;
            }
            else if (diff < 0x80)
            {TRACE_IT(52442);
                result.i8[idx] = static_cast<int8>(mask);
            }
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52443);
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52444);
            result.i8[idx] = (aValue.i8[idx] < bValue.i8[idx]) ? aValue.i8[idx] : bValue.i8[idx];
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52445);
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52446);
            result.i8[idx] = (aValue.i8[idx] > bValue.i8[idx]) ? aValue.i8[idx] : bValue.i8[idx];
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52447);
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52448);
            result.i8[idx] = (aValue.i8[idx] < bValue.i8[idx]) ? 0xff : 0x0;
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52449);
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52450);
            result.i8[idx] = (aValue.i8[idx] <= bValue.i8[idx]) ? 0xff : 0x0;
        }
        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52451);
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52452);
            result.i8[idx] = (aValue.i8[idx] == bValue.i8[idx]) ? 0xff : 0x0;
        }

        return result;
    }


    SIMDValue SIMDInt8x16Operation::OpNotEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52453);
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52454);
            result.i8[idx] = (aValue.i8[idx] != bValue.i8[idx]) ? 0xff : 0x0;
        }

        return result;
    }


    SIMDValue SIMDInt8x16Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52455);
        SIMDValue result;

        for(uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52456);
            result.i8[idx] = (aValue.i8[idx] > bValue.i8[idx]) ? 0xff : 0x0;
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {TRACE_IT(52457);
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52458);
            result.i8[idx] = (aValue.i8[idx] >= bValue.i8[idx]) ? 0xff : 0x0;
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpShiftLeftByScalar(const SIMDValue& value, int count)
    {TRACE_IT(52459);
        SIMDValue result;

        count = count & SIMDUtils::SIMDGetShiftAmountMask(1);

        for(uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52460);
            result.i8[idx] = value.i8[idx] << count;
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpShiftRightByScalar(const SIMDValue& value, int count)
    {TRACE_IT(52461);
        SIMDValue result;

        count = count & SIMDUtils::SIMDGetShiftAmountMask(1);

        for(uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52462);
            result.i8[idx] = value.i8[idx] >> count;
        }

        return result;
    }

    SIMDValue SIMDInt8x16Operation::OpSelect(const SIMDValue& mV, const SIMDValue& tV, const SIMDValue& fV)
    {TRACE_IT(52463);
        SIMDValue result;

        for (uint idx = 0; idx < 16; ++idx)
        {TRACE_IT(52464);
            result.i8[idx] = (mV.i8[idx] == 1) ? tV.i8[idx] : fV.i8[idx];
        }
        return result;
    }

}
#endif
