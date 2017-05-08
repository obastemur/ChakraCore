//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#include "Memory/ForcedMemoryConstraints.h"

void
ForcedMemoryConstraint::Apply()
{TRACE_IT(23231);
    if (Js::Configuration::Global.flags.IsEnabled(Js::ForceFragmentAddressSpaceFlag))
    {TRACE_IT(23232);
        FragmentAddressSpace(Js::Configuration::Global.flags.ForceFragmentAddressSpace);
    }
}

#pragma prefast(suppress:6262, "Where this function is call should have ample of stack space")
void ForcedMemoryConstraint::FragmentAddressSpace(size_t usableSize)
{TRACE_IT(23233);
    // AMD64 address space is too big
#if !defined(_M_X64_OR_ARM64)
    uint const allocationGranularity = 64 * 1024;     // 64 KB
    Assert(allocationGranularity == AutoSystemInfo::Data.dwAllocationGranularity);
    uint64 const addressEnd = ((uint64)4) * 1024 * 1024 * 1024;

    uint const freeSpaceSize = Math::Align<size_t>(usableSize, allocationGranularity);
    void * address[addressEnd / allocationGranularity];

    // Reserve a contiguous usable space
    void * freeAddress = ::VirtualAlloc(NULL, freeSpaceSize, MEM_RESERVE, PAGE_NOACCESS);

    // Reserve the reset the address space
    for (uint i = 1; i < _countof(address); i++)
    {TRACE_IT(23234);
        address[i] = ::VirtualAlloc((LPVOID)(i * allocationGranularity), allocationGranularity, MEM_RESERVE, PAGE_NOACCESS);
    }

    // fragment
    int j = _countof(address) - 2;
    do
    {TRACE_IT(23235);
        if (address[j + 1] == nullptr)
        {TRACE_IT(23236);
             j--;
             continue;
        }

        ::VirtualFree(address[j + 1], 0, MEM_RELEASE);
        j -= 2;
    }
    while (j > 0);

    ::VirtualFree(freeAddress, 0, MEM_RELEASE);
#endif
}

#endif
