//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"

#include "Option.h"
#include "ImmutableList.h"
#include "BufferBuilder.h"

namespace Js
{

#if DBG
void
BufferBuilder::TraceOutput(byte * buffer, uint32 size) const
{TRACE_IT(20869);
    if (PHASE_TRACE1(Js::ByteCodeSerializationPhase))
    {TRACE_IT(20870);
        Output::Print(_u("%08X: %-40s:"), this->offset, this->clue);
        for (uint i = 0; i < size; i ++)
        {TRACE_IT(20871);
            Output::Print(_u(" %02x"), buffer[this->offset + i]);
        }
        Output::Print(_u("\n"));
    }
}
#endif

};
