//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Common.h"
#include "ChakraPlatform.h"
#include <sys/sysinfo.h>
#include <sys/resource.h>

namespace PlatformAgnostic
{
    SystemInfo::PlatformData SystemInfo::data;

    SystemInfo::PlatformData::PlatformData()
    {TRACE_IT(65038);
        struct sysinfo systemInfo;
        if (sysinfo(&systemInfo) == -1)
        {TRACE_IT(65039);
            totalRam = 0;
        }
        else
        {TRACE_IT(65040);
            totalRam = systemInfo.totalram;
        }
    }

    bool SystemInfo::GetMaxVirtualMemory(size_t *totalAS)
    {TRACE_IT(65041);
        struct rlimit limit;
        if (getrlimit(RLIMIT_AS, &limit) != 0)
        {TRACE_IT(65042);
            return false;
        }
        *totalAS = limit.rlim_cur;
        return true;
    }
}
