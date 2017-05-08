//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAT")

RecyclerHeuristic RecyclerHeuristic::Instance;

// static
RecyclerHeuristic::RecyclerHeuristic()
{TRACE_IT(26324);
    ::MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    BOOL isSuccess = ::GlobalMemoryStatusEx(&mem);
    Assert(isSuccess);

    DWORDLONG physicalMemoryBytes = mem.ullTotalPhys;
    uint baseFactor;

    // xplat-todo: Android sysconf is rather unreliable,
    // ullTotalPhys may not be the best source for a decision below
    if (isSuccess && AutoSystemInfo::IsLowMemoryDevice() && physicalMemoryBytes <= 512 MEGABYTES)
    {TRACE_IT(26325);
        // Low-end Apollo (512MB RAM) scenario.
        // Note that what's specific about Apollo is that IE runs in physical memory,
        //      that's one reason to distinguish 512MB Apollo from 512MB desktop.
        baseFactor = 16;
        this->DefaultMaxFreePageCount = 16 MEGABYTES_OF_PAGES;
        this->DefaultMaxAllocPageCount = 32;
    }
    else if (isSuccess && physicalMemoryBytes <= 1024 MEGABYTES)
    {TRACE_IT(26326);
        // Tablet/slate/high-end Apollo scenario, including 512MB non-Apollo.
        baseFactor = 64;
        this->DefaultMaxFreePageCount = 64 MEGABYTES_OF_PAGES;
        this->DefaultMaxAllocPageCount = 64;
    }
    else
    {TRACE_IT(26327);
        // Regular desktop scenario.
        baseFactor = 192;
        this->DefaultMaxFreePageCount = 512 MEGABYTES_OF_PAGES;
        this->DefaultMaxAllocPageCount = 256;
    }

    this->ConfigureBaseFactor(baseFactor);
}

void
RecyclerHeuristic::ConfigureBaseFactor(uint baseFactor)
{TRACE_IT(26328);
    this->MaxUncollectedAllocBytes = baseFactor MEGABYTES;
    this->UncollectedAllocBytesConcurrentPriorityBoost = baseFactor MEGABYTES;
    this->MaxPartialUncollectedNewPageCount = baseFactor MEGABYTES_OF_PAGES;
    this->MaxUncollectedAllocBytesOnExit = (baseFactor / 2) MEGABYTES;

    this->MaxUncollectedAllocBytesPartialCollect = this->MaxUncollectedAllocBytes - 1 MEGABYTES;
}

uint
RecyclerHeuristic::UncollectedAllocBytesCollection()
{TRACE_IT(26329);
    return DefaultUncollectedAllocBytesCollection;
}

#if ENABLE_CONCURRENT_GC
uint
RecyclerHeuristic::MaxBackgroundFinishMarkCount(Js::ConfigFlagsTable& flags)
{TRACE_IT(26330);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::MaxBackgroundFinishMarkCountFlag))
    {TRACE_IT(26331);
        return flags.MaxBackgroundFinishMarkCount;
    }
#endif
    return DefaultMaxBackgroundFinishMarkCount;
}

DWORD
RecyclerHeuristic::BackgroundFinishMarkWaitTime(bool backgroundFinishMarkWaitTime, Js::ConfigFlagsTable& flags)
{TRACE_IT(26332);
    if (RECYCLER_HEURISTIC_VERSION == 10)
    {TRACE_IT(26333);
        backgroundFinishMarkWaitTime = backgroundFinishMarkWaitTime && CUSTOM_PHASE_ON1(flags, Js::BackgroundFinishMarkPhase);
    }
    else
    {TRACE_IT(26334);
        backgroundFinishMarkWaitTime = backgroundFinishMarkWaitTime && !CUSTOM_PHASE_OFF1(flags, Js::BackgroundFinishMarkPhase);
    }
    if (!backgroundFinishMarkWaitTime && !CUSTOM_PHASE_FORCE1(flags, Js::BackgroundFinishMarkPhase))
    {TRACE_IT(26335);
        return INFINITE;
    }
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::BackgroundFinishMarkWaitTimeFlag))
    {TRACE_IT(26336);
        return flags.BackgroundFinishMarkWaitTime;
    }
#endif
    if (CUSTOM_PHASE_FORCE1(flags, Js::BackgroundFinishMarkPhase))
    {TRACE_IT(26337);
        return INFINITE;
    }
    return DefaultBackgroundFinishMarkWaitTime;
}

size_t
RecyclerHeuristic::MinBackgroundRepeatMarkRescanBytes(Js::ConfigFlagsTable& flags)
{TRACE_IT(26338);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::MinBackgroundRepeatMarkRescanBytesFlag))
    {TRACE_IT(26339);
        return flags.MinBackgroundRepeatMarkRescanBytes;
    }
#endif
    return DefaultMinBackgroundRepeatMarkRescanBytes;
}

DWORD
RecyclerHeuristic::FinishConcurrentCollectWaitTime(Js::ConfigFlagsTable& flags)
{TRACE_IT(26340);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::RecyclerThreadCollectTimeoutFlag))
    {TRACE_IT(26341);
        return flags.RecyclerThreadCollectTimeout;
    }
#endif
    return DefaultFinishConcurrentCollectWaitTime;
}


DWORD
RecyclerHeuristic::PriorityBoostTimeout(Js::ConfigFlagsTable& flags)
{TRACE_IT(26342);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (flags.IsEnabled(Js::RecyclerPriorityBoostTimeoutFlag))
    {TRACE_IT(26343);
        return flags.RecyclerPriorityBoostTimeout;
    }
#endif
    return TickCountConcurrentPriorityBoost;
}
#endif

#if ENABLE_PARTIAL_GC && ENABLE_CONCURRENT_GC
bool
RecyclerHeuristic::PartialConcurrentNextCollection(double ratio, Js::ConfigFlagsTable& flags)
{
    if (CUSTOM_PHASE_FORCE1(flags, Js::ConcurrentPartialCollectPhase))
    {TRACE_IT(26344);
        return true;
    }

    if (RECYCLER_HEURISTIC_VERSION == 10)
    {
        // Default off for version == 10
        if (!CUSTOM_PHASE_ON1(flags, Js::ConcurrentPartialCollectPhase))
        {TRACE_IT(26345);
            return false;
        }
    }
    else
    {
        // Default on
        if (CUSTOM_PHASE_OFF1(flags, Js::ConcurrentPartialCollectPhase))
        {TRACE_IT(26346);
            return false;
        }
    }
    return ratio >= 0.5;
}
#endif
