//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Language/CodeGenRecyclableData.h"

namespace Js
{
    CodeGenRecyclableData::CodeGenRecyclableData(const FunctionCodeGenJitTimeData *const jitTimeData) : jitTimeData(jitTimeData)
    {TRACE_IT(47476);
        Assert(jitTimeData);
    }

    const FunctionCodeGenJitTimeData *CodeGenRecyclableData::JitTimeData() const
    {TRACE_IT(47477);
        return jitTimeData;
    }
}
