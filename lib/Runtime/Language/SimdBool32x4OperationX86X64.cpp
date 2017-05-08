//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if _M_IX86 || _M_AMD64

namespace Js
{
    SIMDValue SIMDBool32x4Operation::OpBool32x4(bool x, bool y, bool z, bool w)
    {TRACE_IT(52177);
        X86SIMDValue x86Result;
        x86Result.m128i_value = _mm_set_epi32(w * -1, z * -1, y * -1, x * -1);
        return X86SIMDValue::ToSIMDValue(x86Result);
    }

    SIMDValue SIMDBool32x4Operation::OpBool32x4(const SIMDValue& v)
    {TRACE_IT(52178);
        // overload function with input parameter as SIMDValue for completeness
        SIMDValue result;
        result = v;
        return result;
    }

    // Unary Ops
    bool SIMDBool32x4Operation::OpAnyTrue(const SIMDValue& simd)
    {TRACE_IT(52179);
        X86SIMDValue x86Simd = X86SIMDValue::ToX86SIMDValue(simd);
        int mask_8 = _mm_movemask_epi8(x86Simd.m128i_value); //latency 3, throughput 1
        return mask_8 != 0;
    }

    bool SIMDBool32x4Operation::OpAllTrue(const SIMDValue& simd)
    {TRACE_IT(52180);
        X86SIMDValue x86Simd = X86SIMDValue::ToX86SIMDValue(simd);
        int mask_8 = _mm_movemask_epi8(x86Simd.m128i_value); //latency 3, throughput 1
        return mask_8 == 0xFFFF;
    }
}


#endif
