//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Common.h"
#include "ChakraPlatform.h"
#include <sys/sysctl.h>

namespace PlatformAgnostic
{
    SystemInfo::PlatformData SystemInfo::data;

    SystemInfo::PlatformData::PlatformData()
    {TRACE_IT(65113);
        // Get Total Ram
        int totalRamHW [] = { CTL_HW, HW_MEMSIZE };

        size_t length = sizeof(size_t);
        if(sysctl(totalRamHW, 2, &totalRam, &length, NULL, 0) == -1)
        {TRACE_IT(65114);
            totalRam = 0;
        }
    }

    bool SystemInfo::GetMaxVirtualMemory(size_t *totalAS)
    {TRACE_IT(65115);
        struct rlimit limit;
        if (getrlimit(RLIMIT_AS, &limit) != 0)
        {TRACE_IT(65116);
            return false;
        }

        *totalAS = limit.rlim_cur;
        return true;
    }
}
