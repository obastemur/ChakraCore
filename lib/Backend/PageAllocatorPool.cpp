//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

#if ENABLE_OOP_NATIVE_CODEGEN
#include "JITServer/JITServer.h"
#include "PageAllocatorPool.h"

CriticalSection PageAllocatorPool::cs;
PageAllocatorPool* PageAllocatorPool::Instance = nullptr;

PageAllocatorPool::PageAllocatorPool()
    :pageAllocators(&NoThrowHeapAllocator::Instance),
    activePageAllocatorCount(0)
{LOGMEIN("PageAllocatorPool.cpp] 17\n");
    idleCleanupTimer = CreateWaitableTimerEx(NULL, L"JITServerIdle", 0/*auto reset*/, TIMER_ALL_ACCESS);
}

PageAllocatorPool::~PageAllocatorPool()
{LOGMEIN("PageAllocatorPool.cpp] 22\n");
    AutoCriticalSection autoCS(&cs);
    RemoveAll();
}

void PageAllocatorPool::Initialize()
{LOGMEIN("PageAllocatorPool.cpp] 28\n");
    Instance = HeapNewNoThrow(PageAllocatorPool);
    if (Instance == nullptr)
    {LOGMEIN("PageAllocatorPool.cpp] 31\n");
        Js::Throw::FatalInternalError();
    }
}
void PageAllocatorPool::Shutdown()
{LOGMEIN("PageAllocatorPool.cpp] 36\n");
    AutoCriticalSection autoCS(&cs);
    Assert(Instance);
    if (Instance)
    {LOGMEIN("PageAllocatorPool.cpp] 40\n");
        PageAllocatorPool* localInstance = Instance;
        Instance = nullptr;
        if (localInstance->idleCleanupTimer)
        {LOGMEIN("PageAllocatorPool.cpp] 44\n");
            CloseHandle(localInstance->idleCleanupTimer);
        }
        HeapDelete(localInstance);
    }
}
void PageAllocatorPool::RemoveAll()
{LOGMEIN("PageAllocatorPool.cpp] 51\n");
    Assert(cs.IsLocked());
    while (!pageAllocators.Empty())
    {LOGMEIN("PageAllocatorPool.cpp] 54\n");
        HeapDelete(pageAllocators.Pop());
    }
}

unsigned int PageAllocatorPool::GetInactivePageAllocatorCount()
{LOGMEIN("PageAllocatorPool.cpp] 60\n");
    AutoCriticalSection autoCS(&cs);
    return pageAllocators.Count();
}

PageAllocator* PageAllocatorPool::GetPageAllocator()
{LOGMEIN("PageAllocatorPool.cpp] 66\n");
    AutoCriticalSection autoCS(&cs);
    PageAllocator* pageAllocator = nullptr;
    if (pageAllocators.Count() > 0)
    {LOGMEIN("PageAllocatorPool.cpp] 70\n");
        // TODO: OOP JIT, select the page allocator with right count of free pages
        // base on some heuristic
        pageAllocator = this->pageAllocators.Pop();
    }
    else
    {
        pageAllocator = HeapNew(PageAllocator, nullptr, Js::Configuration::Global.flags, PageAllocatorType_BGJIT,
            AutoSystemInfo::Data.IsLowMemoryProcess() ? PageAllocator::DefaultLowMaxFreePageCount : PageAllocator::DefaultMaxFreePageCount);
    }

    activePageAllocatorCount++;
    return pageAllocator;

}
void PageAllocatorPool::ReturnPageAllocator(PageAllocator* pageAllocator)
{LOGMEIN("PageAllocatorPool.cpp] 86\n");
    AutoCriticalSection autoCS(&cs);
    if (!this->pageAllocators.PrependNoThrow(&NoThrowHeapAllocator::Instance, pageAllocator))
    {LOGMEIN("PageAllocatorPool.cpp] 89\n");
        HeapDelete(pageAllocator);
    }

    activePageAllocatorCount--;
    if (activePageAllocatorCount == 0 || GetInactivePageAllocatorCount() > (uint)Js::Configuration::Global.flags.JITServerMaxInactivePageAllocatorCount)
    {LOGMEIN("PageAllocatorPool.cpp] 95\n");
        PageAllocatorPool::IdleCleanup();
    }
}

void PageAllocatorPool::IdleCleanup()
{LOGMEIN("PageAllocatorPool.cpp] 101\n");
    AutoCriticalSection autoCS(&cs);
    if (Instance)
    {LOGMEIN("PageAllocatorPool.cpp] 104\n");
        LARGE_INTEGER liDueTime;
        liDueTime.QuadPart = Js::Configuration::Global.flags.JITServerIdleTimeout * -10000000LL; // wait for 10 seconds to do the cleanup

        // If the timer is already active when you call SetWaitableTimer, the timer is stopped, then it is reactivated.
        if (!SetWaitableTimer(Instance->idleCleanupTimer, &liDueTime, 0, IdleCleanupRoutine, NULL, 0))
        {LOGMEIN("PageAllocatorPool.cpp] 110\n");
            Instance->RemoveAll();
        }
    }
}

VOID CALLBACK PageAllocatorPool::IdleCleanupRoutine(
    _In_opt_ LPVOID lpArgToCompletionRoutine,
    _In_     DWORD  dwTimerLowValue,
    _In_     DWORD  dwTimerHighValue)
{LOGMEIN("PageAllocatorPool.cpp] 120\n");
    AutoCriticalSection autoCS(&cs);
    if (Instance)
    {LOGMEIN("PageAllocatorPool.cpp] 123\n");
        // TODO: OOP JIT, use better stragtegy to do the cleanup, like do not remove all,
        // instead keep couple inactivate page allocator for next calls
        Instance->RemoveAll();
    }
}
#endif
