//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimePlatformAgnosticPch.h"
#include "Common.h"
#include "ChakraPlatform.h"

namespace PlatformAgnostic
{
    SystemInfo::PlatformData SystemInfo::data;

    SystemInfo::PlatformData::PlatformData()
    {LOGMEIN("SystemInfo.cpp] 14\n");
        ULONGLONG ram;
        if (GetPhysicallyInstalledSystemMemory(&ram) == TRUE)
        {LOGMEIN("SystemInfo.cpp] 17\n");
            totalRam = static_cast<size_t>(ram) * 1024;
        }
    }

    bool SystemInfo::GetMaxVirtualMemory(size_t *totalAS)
    {LOGMEIN("SystemInfo.cpp] 23\n");
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        *totalAS = (size_t) info.lpMaximumApplicationAddress;
        return true;
    }

}
