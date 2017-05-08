//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"

#ifdef PROFILE_MEM
#include "Memory/AutoPtr.h"
#include "Core/ProfileMemory.h"

THREAD_LOCAL MemoryProfiler * MemoryProfiler::Instance = nullptr;

CriticalSection MemoryProfiler::s_cs;
AutoPtr<MemoryProfiler, NoCheckHeapAllocator> MemoryProfiler::profilers(nullptr);

MemoryProfiler::MemoryProfiler() :
    pageAllocator(nullptr, Js::Configuration::Global.flags,
    PageAllocatorType_Max, 0, false
#if ENABLE_BACKGROUND_PAGE_FREEING
        , nullptr
#endif
        ),
    alloc(_u("MemoryProfiler"), &pageAllocator, Js::Throw::OutOfMemory),
    arenaDataMap(&alloc, 10)
{TRACE_IT(20441);
    threadId = ::GetCurrentThreadId();
    memset(&pageMemoryData, 0, sizeof(pageMemoryData));
    memset(&recyclerMemoryData, 0, sizeof(recyclerMemoryData));
}

MemoryProfiler::~MemoryProfiler()
{TRACE_IT(20442);
#if DBG
    pageAllocator.SetDisableThreadAccessCheck();
#endif
    if (next != nullptr)
    {TRACE_IT(20443);
        NoCheckHeapDelete(next);
    }
}

MemoryProfiler *
MemoryProfiler::EnsureMemoryProfiler()
{TRACE_IT(20444);
    MemoryProfiler * memoryProfiler = MemoryProfiler::Instance;

    if (memoryProfiler == nullptr)
    {TRACE_IT(20445);
        memoryProfiler = NoCheckHeapNew(MemoryProfiler);

        {TRACE_IT(20446);
            AutoCriticalSection autocs(&s_cs);
            memoryProfiler->next = MemoryProfiler::profilers.Detach();
            MemoryProfiler::profilers = memoryProfiler;
        }

        MemoryProfiler::Instance = memoryProfiler;
    }
    return memoryProfiler;
}

PageMemoryData *
MemoryProfiler::GetPageMemoryData(PageAllocatorType type)
{TRACE_IT(20447);
    if (!Js::Configuration::Global.flags.IsEnabled(Js::TraceMemoryFlag))
    {TRACE_IT(20448);
        return nullptr;
    }
    if (type == PageAllocatorType_Max)
    {TRACE_IT(20449);
        return nullptr;
    }
    MemoryProfiler * memoryProfiler = EnsureMemoryProfiler();
    return &memoryProfiler->pageMemoryData[type];
}

RecyclerMemoryData *
MemoryProfiler::GetRecyclerMemoryData()
{TRACE_IT(20450);
    if (!Js::Configuration::Global.flags.IsEnabled(Js::TraceMemoryFlag))
    {TRACE_IT(20451);
        return nullptr;
    }
    MemoryProfiler * memoryProfiler = EnsureMemoryProfiler();
    return &memoryProfiler->recyclerMemoryData;
}

ArenaMemoryData *
MemoryProfiler::Begin(LPCWSTR name)
{TRACE_IT(20452);
    if (!Js::Configuration::Global.flags.IsEnabled(Js::TraceMemoryFlag))
    {TRACE_IT(20453);
        return nullptr;
    }
    Assert(name != nullptr);
    if (wcscmp(name, _u("MemoryProfiler")) == 0)
    {TRACE_IT(20454);
        // Don't profile memory profiler itself
        return nullptr;
    }

    // This is debug only code, we don't care if we catch the right exception
    AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
    MemoryProfiler * memoryProfiler = EnsureMemoryProfiler();
    ArenaMemoryDataSummary * arenaTotalMemoryData;
    if (!memoryProfiler->arenaDataMap.TryGetValue((LPWSTR)name, &arenaTotalMemoryData))
    {TRACE_IT(20455);
        arenaTotalMemoryData = AnewStructZ(&memoryProfiler->alloc, ArenaMemoryDataSummary);
        memoryProfiler->arenaDataMap.Add((LPWSTR)name, arenaTotalMemoryData);
    }
    arenaTotalMemoryData->arenaCount++;

    ArenaMemoryData * memoryData = AnewStructZ(&memoryProfiler->alloc, ArenaMemoryData);
    if (arenaTotalMemoryData->data == nullptr)
    {TRACE_IT(20456);
        arenaTotalMemoryData->data = memoryData;
    }
    else
    {TRACE_IT(20457);
        memoryData->next = arenaTotalMemoryData->data;
        arenaTotalMemoryData->data->prev = memoryData;
        arenaTotalMemoryData->data = memoryData;
    }
    memoryData->profiler = memoryProfiler;
    return memoryData;
}

void
MemoryProfiler::Reset(LPCWSTR name, ArenaMemoryData * memoryData)
{TRACE_IT(20458);
    MemoryProfiler * memoryProfiler = memoryData->profiler;
    ArenaMemoryDataSummary * arenaMemoryDataSummary;
    bool hasItem = memoryProfiler->arenaDataMap.TryGetValue((LPWSTR)name, &arenaMemoryDataSummary);
    Assert(hasItem);


    AccumulateData(arenaMemoryDataSummary, memoryData, true);
    memoryData->allocatedBytes = 0;
    memoryData->alignmentBytes = 0;
    memoryData->requestBytes = 0;
    memoryData->requestCount = 0;
    memoryData->reuseBytes = 0;
    memoryData->reuseCount = 0;
    memoryData->freelistBytes = 0;
    memoryData->freelistCount = 0;
    memoryData->resetCount++;
}

void
MemoryProfiler::End(LPCWSTR name, ArenaMemoryData * memoryData)
{TRACE_IT(20459);
    MemoryProfiler * memoryProfiler = memoryData->profiler;
    ArenaMemoryDataSummary * arenaMemoryDataSummary;
    bool hasItem = memoryProfiler->arenaDataMap.TryGetValue((LPWSTR)name, &arenaMemoryDataSummary);
    Assert(hasItem);

    if (memoryData->next != nullptr)
    {TRACE_IT(20460);
        memoryData->next->prev = memoryData->prev;
    }

    if (memoryData->prev != nullptr)
    {TRACE_IT(20461);
        memoryData->prev->next = memoryData->next;
    }
    else
    {TRACE_IT(20462);
        Assert(arenaMemoryDataSummary->data == memoryData);
        arenaMemoryDataSummary->data = memoryData->next;
    }
    AccumulateData(arenaMemoryDataSummary, memoryData);
}

void
MemoryProfiler::AccumulateData(ArenaMemoryDataSummary * arenaMemoryDataSummary,  ArenaMemoryData * memoryData, bool reset)
{TRACE_IT(20463);
    arenaMemoryDataSummary->total.alignmentBytes += memoryData->alignmentBytes;
    arenaMemoryDataSummary->total.allocatedBytes += memoryData->allocatedBytes;
    arenaMemoryDataSummary->total.freelistBytes += memoryData->freelistBytes;
    arenaMemoryDataSummary->total.freelistCount += memoryData->freelistCount;
    arenaMemoryDataSummary->total.requestBytes += memoryData->requestBytes;
    arenaMemoryDataSummary->total.requestCount += memoryData->requestCount;
    arenaMemoryDataSummary->total.reuseCount += memoryData->reuseCount;
    arenaMemoryDataSummary->total.reuseBytes += memoryData->reuseBytes;
    if (!reset)
    {TRACE_IT(20464);
        arenaMemoryDataSummary->total.resetCount += memoryData->resetCount;
    }

    arenaMemoryDataSummary->max.alignmentBytes = max(arenaMemoryDataSummary->max.alignmentBytes, memoryData->alignmentBytes);
    arenaMemoryDataSummary->max.allocatedBytes = max(arenaMemoryDataSummary->max.allocatedBytes, memoryData->allocatedBytes);
    arenaMemoryDataSummary->max.freelistBytes = max(arenaMemoryDataSummary->max.freelistBytes, memoryData->freelistBytes);
    arenaMemoryDataSummary->max.freelistCount = max(arenaMemoryDataSummary->max.freelistCount, memoryData->freelistCount);
    arenaMemoryDataSummary->max.requestBytes = max(arenaMemoryDataSummary->max.requestBytes, memoryData->requestBytes);
    arenaMemoryDataSummary->max.requestCount = max(arenaMemoryDataSummary->max.requestCount, memoryData->requestCount);
    arenaMemoryDataSummary->max.reuseCount = max(arenaMemoryDataSummary->max.reuseCount, memoryData->reuseCount);
    arenaMemoryDataSummary->max.reuseBytes = max(arenaMemoryDataSummary->max.reuseBytes, memoryData->reuseBytes);
    if (!reset)
    {TRACE_IT(20465);
        arenaMemoryDataSummary->max.resetCount = max(arenaMemoryDataSummary->max.resetCount, memoryData->resetCount);
    }
}

void
MemoryProfiler::PrintPageMemoryData(PageMemoryData const& pageMemoryData, char const * title)
{TRACE_IT(20466);
    if (pageMemoryData.allocSegmentCount != 0)
    {TRACE_IT(20467);
        Output::Print(_u("%-10S:%9d %10d | %4d %10d | %4d %10d | %10d | %10d | %10d | %10d\n"), title,
            pageMemoryData.currentCommittedPageCount * AutoSystemInfo::PageSize, pageMemoryData.peakCommittedPageCount * AutoSystemInfo::PageSize,
            pageMemoryData.allocSegmentCount, pageMemoryData.allocSegmentBytes,
            pageMemoryData.releaseSegmentCount, pageMemoryData.releaseSegmentBytes,
            pageMemoryData.allocPageCount * AutoSystemInfo::PageSize,
            pageMemoryData.releasePageCount * AutoSystemInfo::PageSize,
            pageMemoryData.decommitPageCount * AutoSystemInfo::PageSize,
            pageMemoryData.recommitPageCount * AutoSystemInfo::PageSize);
    }
}

void
MemoryProfiler::Print()
{TRACE_IT(20468);
    Output::Print(_u("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n"));
    Output::Print(_u("Allocation for thread 0x%08X\n"), threadId);
    Output::Print(_u("-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=\n"));

    bool hasData = false;
    for (int i = 0; i < PageAllocatorType_Max; i++)
    {TRACE_IT(20469);
        if (pageMemoryData[i].allocSegmentCount != 0)
        {TRACE_IT(20470);
            hasData = true;
            break;
        }
    }
    if (hasData)
    {TRACE_IT(20471);
        Output::Print(_u("%-10s:%-20s | %-15s | %-15s | %10s | %10s | %11s | %11s\n"), _u(""), _u("         Current"), _u("   Alloc Seg"), _u("    Free Seg"),
            _u("Request"), _u("Released"), _u("Decommitted"), _u("Recommitted"));
        Output::Print(_u("%-10s:%9s %10s | %4s %10s | %4s %10s | %10s | %10s | %10s | %10s\n"), _u(""), _u("Bytes"), _u("Peak"), _u("#"), _u("Bytes"), _u("#"), _u("Bytes"),
             _u("Bytes"), _u("Bytes"), _u("Bytes"),  _u("Bytes"));
        Output::Print(_u("------------------------------------------------------------------------------------------------------------------\n"));

#define PAGEALLOCATOR_PRINT(i) PrintPageMemoryData(pageMemoryData[PageAllocatorType_ ## i], STRINGIZE(i));
        PAGE_ALLOCATOR_TYPE(PAGEALLOCATOR_PRINT);
        Output::Print(_u("------------------------------------------------------------------------------------------------------------------\n"));
    }

    if (recyclerMemoryData.requestCount != 0)
    {TRACE_IT(20472);
        Output::Print(_u("%-10s:%7s %10s %10s %10s\n"),
                _u("Recycler"),
                _u("#Alloc"),
                _u("AllocBytes"),
                _u("ReqBytes"),
                _u("AlignByte"));
        Output::Print(_u("--------------------------------------------------------------------------------------------------------\n"));
        Output::Print(_u("%-10s:%7d %10d %10d %10d\n"),
            _u(""),
            recyclerMemoryData.requestCount,
            recyclerMemoryData.requestBytes + recyclerMemoryData.alignmentBytes,
            recyclerMemoryData.requestBytes,
            recyclerMemoryData.alignmentBytes);
        Output::Print(_u("--------------------------------------------------------------------------------------------------------\n"));
    }

    if (Js::Configuration::Global.flags.TraceMemory.IsEnabled(Js::AllPhase))
    {TRACE_IT(20473);
        PrintArena(false);
    }
    PrintArena(true);
}

void
MemoryProfiler::PrintArenaHeader(char16 const * title)
{TRACE_IT(20474);
    Output::Print(_u("--------------------------------------------------------------------------------------------------------\n"));

    Output::Print(_u("%-20s:%7s %9s %9s %9s %6s %9s %6s %9s %5s | %5s\n"),
            title,
            _u("#Alloc"),
            _u("AllocByte"),
            _u("ReqBytes"),
            _u("AlignByte"),
            _u("#Reuse"),
            _u("ReuseByte"),
            _u("#Free"),
            _u("FreeBytes"),
            _u("Reset"),
            _u("Count"));

    Output::Print(_u("--------------------------------------------------------------------------------------------------------\n"));
}

int MemoryProfiler::CreateArenaUsageSummary(ArenaAllocator * alloc, bool liveOnly,
    _Outptr_result_buffer_(return) LPWSTR ** name_ptr, _Outptr_result_buffer_(return) ArenaMemoryDataSummary *** summaries_ptr)
{TRACE_IT(20475);
    Assert(alloc);

    LPWSTR *& name = *name_ptr;
    ArenaMemoryDataSummary **& summaries = *summaries_ptr;

    int count = arenaDataMap.Count();
    name = AnewArray(alloc, LPWSTR, count);
    int i = 0;
    arenaDataMap.Map([&i, name](LPWSTR key, ArenaMemoryDataSummary*)
    {
        name[i++] = key;
    });

    qsort_s(name, count, sizeof(LPWSTR), [](void*, const void* a, const void* b) { return DefaultComparer<LPWSTR>::Compare(*(LPWSTR*)a, *(LPWSTR*)b); }, nullptr);

    summaries = AnewArray(alloc, ArenaMemoryDataSummary *, count);

    for (int j = 0; j < count; j++)
    {TRACE_IT(20476);
        ArenaMemoryDataSummary * summary = arenaDataMap.Item(name[j]);
        ArenaMemoryData * data = summary->data;

        ArenaMemoryDataSummary * localSummary;
        if (liveOnly)
        {TRACE_IT(20477);
            if (data == nullptr)
            {TRACE_IT(20478);
                summaries[j] = nullptr;
                continue;
            }
            localSummary = AnewStructZ(alloc, ArenaMemoryDataSummary);
        }
        else
        {TRACE_IT(20479);
            localSummary = Anew(alloc, ArenaMemoryDataSummary, *summary);
        }

        while (data != nullptr)
        {TRACE_IT(20480);
            localSummary->outstandingCount++;
            AccumulateData(localSummary, data);
            data = data->next;
        }

        if (liveOnly)
        {TRACE_IT(20481);
            localSummary->arenaCount = localSummary->outstandingCount;
        }
        summaries[j] = localSummary;
    }

    return count;
}

void
MemoryProfiler::PrintArena(bool liveOnly)
{
    WithArenaUsageSummary(liveOnly, [&] (int count, _In_reads_(count) LPWSTR * name, _In_reads_(count) ArenaMemoryDataSummary ** summaries)
    {
        int i = 0;

        if (liveOnly)
        {TRACE_IT(20482);
            Output::Print(_u("Arena usage summary (live)\n"));
        }
        else
        {TRACE_IT(20483);
            Output::Print(_u("Arena usage summary (all)\n"));
        }

        bool header = false;

        for (i = 0; i < count; i++)
        {TRACE_IT(20484);
            ArenaMemoryDataSummary * data = summaries[i];
            if (data == nullptr)
            {TRACE_IT(20485);
                continue;
            }
            if (!header)
            {TRACE_IT(20486);
                header = true;
                PrintArenaHeader(_u("Arena Size"));
            }

            Output::Print(_u("%-20s %7d %9d %9d %9d %6d %9d %6d %9d %5d | %5d\n"),
                name[i],
                data->total.requestCount,
                data->total.allocatedBytes,
                data->total.requestBytes,
                data->total.alignmentBytes,
                data->total.reuseCount,
                data->total.reuseBytes,
                data->total.freelistCount,
                data->total.freelistBytes,
                data->total.resetCount,
                data->arenaCount);
        }

        header = false;

        for (i = 0; i < count; i++)
        {TRACE_IT(20487);
            ArenaMemoryDataSummary * data = summaries[i];
            if (data == nullptr)
            {TRACE_IT(20488);
                continue;
            }
            if (!header)
            {TRACE_IT(20489);
                header = true;
                PrintArenaHeader(_u("Arena Max"));
            }
            Output::Print(_u("%-20s %7d %9d %9d %9d %6d %9d %6d %9d %5d | %5d\n"),
                name[i],
                data->max.requestCount,
                data->max.allocatedBytes,
                data->max.requestBytes,
                data->max.alignmentBytes,
                data->max.reuseCount,
                data->max.reuseBytes,
                data->max.freelistCount, data->max.freelistBytes,
                data->max.resetCount, data->outstandingCount);
        }

        header = false;
        for (i = 0; i < count; i++)
        {TRACE_IT(20490);
            ArenaMemoryDataSummary * data = summaries[i];
            if (data == nullptr)
            {TRACE_IT(20491);
                continue;
            }
            if (!header)
            {TRACE_IT(20492);
                header = true;
                PrintArenaHeader(_u("Arena Average"));
            }
            Output::Print(_u("%-20s %7d %9d %9d %9d %6d %9d %6d %9d %5d\n"), name[i],
                data->total.requestCount / data->arenaCount,
                data->total.allocatedBytes / data->arenaCount,
                data->total.requestBytes / data->arenaCount,
                data->total.alignmentBytes / data->arenaCount,
                data->total.reuseCount / data->arenaCount,
                data->total.reuseBytes / data->arenaCount,
                data->total.freelistCount / data->arenaCount,
                data->total.freelistBytes / data->arenaCount,
                data->total.resetCount / data->arenaCount);
        }

        Output::Print(_u("--------------------------------------------------------------------------------------------------------\n"));
    });
}

void
MemoryProfiler::PrintCurrentThread()
{TRACE_IT(20493);
    MemoryProfiler* instance = NULL;
    instance = MemoryProfiler::Instance;

    Output::Print(_u("========================================================================================================\n"));
    Output::Print(_u("Memory Profile (Current thread)\n"));
    if (instance != nullptr)
    {TRACE_IT(20494);
        instance->Print();
    }

    Output::Flush();
}

void
MemoryProfiler::PrintAll()
{TRACE_IT(20495);
    Output::Print(_u("========================================================================================================\n"));
    Output::Print(_u("Memory Profile (All threads)\n"));

    ForEachProfiler([] (MemoryProfiler * memoryProfiler)
    {
        memoryProfiler->Print();
    });

    Output::Flush();
}

bool
MemoryProfiler::IsTraceEnabled(bool isRecycler)
{TRACE_IT(20496);
    if (!Js::Configuration::Global.flags.IsEnabled(Js::TraceMemoryFlag))
    {TRACE_IT(20497);
        return false;
    }

    if (Js::Configuration::Global.flags.TraceMemory.IsEnabled(Js::AllPhase))
    {TRACE_IT(20498);
        return true;
    }

    if (!isRecycler)
    {TRACE_IT(20499);
        return (Js::Configuration::Global.flags.TraceMemory.IsEnabled(Js::RunPhase)
            || Js::Configuration::Global.flags.TraceMemory.GetFirstPhase() == Js::InvalidPhase);
    }

    return Js::Configuration::Global.flags.TraceMemory.IsEnabled(Js::RecyclerPhase);
}

bool
MemoryProfiler::IsEnabled()
{TRACE_IT(20500);
    return Js::Configuration::Global.flags.IsEnabled(Js::ProfileMemoryFlag);
}

bool
MemoryProfiler::DoTrackRecyclerAllocation()
{TRACE_IT(20501);
    return MemoryProfiler::IsEnabled() || MemoryProfiler::IsTraceEnabled(true) || MemoryProfiler::IsTraceEnabled(false);
}
#endif
