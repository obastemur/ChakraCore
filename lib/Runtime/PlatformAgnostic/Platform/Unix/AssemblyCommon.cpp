//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Common.h"
#include "PlatformAgnostic/AssemblyCommon.h"

#ifndef DISABLE_JIT
extern void mac_fde_wrapper(const char *dataStart, mac_fde_reg_op reg_op)
{LOGMEIN("AssemblyCommon.cpp] 10\n");
    const char *head = dataStart;
    do
    {LOGMEIN("AssemblyCommon.cpp] 13\n");
        const char *op = head;
        // Get Length of record [ Read 4 bytes ]
        const uint32_t* length = (const uint32_t*)op;
        if(*length == 0)
        {LOGMEIN("AssemblyCommon.cpp] 18\n");
            break;
        }
        // if it's not 0x0,
        // there we have the length of the CIE or FDE record.
        head += 4; // get next [should be non-zero]
        if(*(const uint32_t*)head != 0)
        {LOGMEIN("AssemblyCommon.cpp] 25\n");
            reg_op(op);
        }
        head += *length;
    } while(1);
}
#endif
