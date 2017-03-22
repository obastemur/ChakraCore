//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)

namespace Js
{
    SIMDValue SIMDUint32x4Operation::OpUint32x4(unsigned int x, unsigned int y, unsigned int z, unsigned int w)
    {LOGMEIN("SimdUint32x4Operation.cpp] 12\n");
        SIMDValue result;

        result.u32[SIMD_X] = x;
        result.u32[SIMD_Y] = y;
        result.u32[SIMD_Z] = z;
        result.u32[SIMD_W] = w;

        return result;
    }

    SIMDValue SIMDUint32x4Operation::OpSplat(unsigned int x)
    {LOGMEIN("SimdUint32x4Operation.cpp] 24\n");
        SIMDValue result;

        result.u32[SIMD_X] = result.u32[SIMD_Y] = result.u32[SIMD_Z] = result.u32[SIMD_W] = x;

        return result;
    }

    SIMDValue SIMDUint32x4Operation::OpShiftRightByScalar(const SIMDValue& value, int count)
    {LOGMEIN("SimdUint32x4Operation.cpp] 33\n");
        SIMDValue result;

        count = count & SIMDUtils::SIMDGetShiftAmountMask(4);

        result.u32[SIMD_X] = (value.u32[SIMD_X] >> count);
        result.u32[SIMD_Y] = (value.u32[SIMD_Y] >> count);
        result.u32[SIMD_Z] = (value.u32[SIMD_Z] >> count);
        result.u32[SIMD_W] = (value.u32[SIMD_W] >> count);

        return result;
    }

    SIMDValue SIMDUint32x4Operation::OpFromFloat32x4(const SIMDValue& v, bool &throws)
    {LOGMEIN("SimdUint32x4Operation.cpp] 47\n");
        SIMDValue result = {0};
        const int MIN_UINT = -1, MAX_UINT = 0xFFFFFFFF;

        for (int i = 0; i < 4; i++)
        {LOGMEIN("SimdUint32x4Operation.cpp] 52\n");
            if (v.f32[i] > MIN_UINT && v.f32[i] <= MAX_UINT)
            {LOGMEIN("SimdUint32x4Operation.cpp] 54\n");
                result.u32[i] = (unsigned int)(v.f32[i]);
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

    SIMDValue SIMDUint32x4Operation::OpMin(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdUint32x4Operation.cpp] 68\n");
        SIMDValue result;
        result.u32[SIMD_X] = (aValue.u32[SIMD_X] < bValue.u32[SIMD_X]) ? aValue.u32[SIMD_X] : bValue.u32[SIMD_X];
        result.u32[SIMD_Y] = (aValue.u32[SIMD_Y] < bValue.u32[SIMD_Y]) ? aValue.u32[SIMD_Y] : bValue.u32[SIMD_Y];
        result.u32[SIMD_Z] = (aValue.u32[SIMD_Z] < bValue.u32[SIMD_Z]) ? aValue.u32[SIMD_Z] : bValue.u32[SIMD_Z];
        result.u32[SIMD_W] = (aValue.u32[SIMD_W] < bValue.u32[SIMD_W]) ? aValue.u32[SIMD_W] : bValue.u32[SIMD_W];

        return result;
    }

    SIMDValue SIMDUint32x4Operation::OpMax(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdUint32x4Operation.cpp] 79\n");
        return OpMin(bValue, aValue); // swap args
    }

    SIMDValue SIMDUint32x4Operation::OpLessThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdUint32x4Operation.cpp] 84\n");
        SIMDValue result;

        result.u32[SIMD_X] = (aValue.u32[SIMD_X] < bValue.u32[SIMD_X]) ? 0xffffffff : 0x0;
        result.u32[SIMD_Y] = (aValue.u32[SIMD_Y] < bValue.u32[SIMD_Y]) ? 0xffffffff : 0x0;
        result.u32[SIMD_Z] = (aValue.u32[SIMD_Z] < bValue.u32[SIMD_Z]) ? 0xffffffff : 0x0;
        result.u32[SIMD_W] = (aValue.u32[SIMD_W] < bValue.u32[SIMD_W]) ? 0xffffffff : 0x0;

        return result;
    }

    SIMDValue SIMDUint32x4Operation::OpLessThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdUint32x4Operation.cpp] 96\n");
        SIMDValue result;

        result.u32[SIMD_X] = (aValue.u32[SIMD_X] <= bValue.u32[SIMD_X]) ? 0xffffffff : 0x0;
        result.u32[SIMD_Y] = (aValue.u32[SIMD_Y] <= bValue.u32[SIMD_Y]) ? 0xffffffff : 0x0;
        result.u32[SIMD_Z] = (aValue.u32[SIMD_Z] <= bValue.u32[SIMD_Z]) ? 0xffffffff : 0x0;
        result.u32[SIMD_W] = (aValue.u32[SIMD_W] <= bValue.u32[SIMD_W]) ? 0xffffffff : 0x0;

        return result;
    }

    SIMDValue SIMDUint32x4Operation::OpGreaterThanOrEqual(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdUint32x4Operation.cpp] 108\n");
        SIMDValue result;
        result = SIMDUint32x4Operation::OpLessThan(aValue, bValue);
        result = SIMDInt32x4Operation::OpNot(result);
        return result;
    }

    SIMDValue SIMDUint32x4Operation::OpGreaterThan(const SIMDValue& aValue, const SIMDValue& bValue)
    {LOGMEIN("SimdUint32x4Operation.cpp] 116\n");
        SIMDValue result;
        result = SIMDUint32x4Operation::OpLessThanOrEqual(aValue, bValue);
        result = SIMDInt32x4Operation::OpNot(result);
        return result;
    }

}

#endif
