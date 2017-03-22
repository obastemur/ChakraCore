//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if defined(_M_ARM32_OR_ARM64)
namespace Js
{
    SIMDValue SIMDBool8x16Operation::OpBool8x16(bool b[])
    {LOGMEIN("SimdBool8x16Operation.cpp] 10\n");
        SIMDValue result;
        for (uint i = 0; i < 16; i++)
        {LOGMEIN("SimdBool8x16Operation.cpp] 13\n");
            result.i8[i] = b[i] ? -1 : 0;
        }

        return result;
    }

    SIMDValue SIMDBool8x16Operation::OpBool8x16(const SIMDValue& v)
    {LOGMEIN("SimdBool8x16Operation.cpp] 21\n");
        // overload function with input parameter as SIMDValue for completeness
        SIMDValue result;
        result = v;
        return result;
    }
}
#endif
