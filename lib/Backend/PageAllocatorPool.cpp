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
{TRACE_IT(14871);
    idleCleanupTimer = CreateWaitableTimerEx(NULL, L"JITServerIdle", 0/*auto reset*/, TIMER_ALL_ACCESS);
}

PageAllocatorPool::~PageAllocatorPool()
{TRACE_IT(14872);
    AutoCriticalSection autoCS(&cs);
    RemoveAll();
}

void PageAllocatorPool::Initialize()
{TRACE_IT(14873);
    Instance = HeapNewNoThrow(PageAllocatorPool);
    if (Instance == nullptr)
    {TRACE_IT(14874);
        Js::Throw::FatalInternalError();
    }
}
void PageAllocatorPool::Shutdown()
{TRACE_IT(14875);
    AutoCriticalSection autoCS(&cs);
    Assert(Instance);
    if (Instance)
    {TRACE_IT(14876);
        PageAllocatorPool* localInstance = Instance;
        Instance = nullptr;
        if (localInstance->idleCleanupTimer)
        {TRACE_IT(14877);
            CloseHandle(localInstance->idleCleanupTimer);
        }
        HeapDelete(localInstance);
    }
}
void PageAllocatorPool::RemoveAll()
{TRACE_IT(14878);
    Assert(cs.IsLocked());
    while (!pageAllocators.Empty())
    {TRACE_IT(14879);
        HeapDelete(pageAllocators.Pop());
    }
}

unsigned int PageAllocatorPool::GetInactivePageAllocatorCount()
{TRACE_IT(14880);
    AutoCriticalSection autoCS(&cs);
    return pageAllocators.Count();
}

PageAllocator* PageAllocatorPool::GetPageAllocator()
{TRACE_IT(14881);
    AutoCriticalSection autoCS(&cs);
    PageAllocator* pageAllocator = nullptr;
    if (pageAllocators.Count() > 0)
    {TRACE_IT(14882);
        // TODO: OOP JIT, select the page allocator with right count of free pages
        // base on some heuristic
        pageAllocator = this->pageAllocators.Pop();
    }
    else
    {TRACE_IT(14883);
        pageAllocator = HeapNew(PageAllocator, nullptr, Js::Configuration::Global.flags, PageAllocatorType_BGJIT,
            AutoSystemInfo::Data.IsLowMemoryProcess() ? PageAllocator::DefaultLowMaxFreePageCount : PageAllocator::DefaultMaxFreePageCount);
    }

    activePageAllocatorCount++;
    return pageAllocator;

}
void PageAllocatorPool::ReturnPageAllocator(PageAllocator* pageAllocator)
{TRACE_IT(14884);
    AutoCriticalSection autoCS(&cs);
    if (!this->pageAllocators.PrependNoThrow(&NoThrowHeapAllocator::Instance, pageAllocator))
    {TRACE_IT(14885);
        HeapDelete(pageAllocator);
    }

    activePageAllocatorCount--;
    if (activePageAllocatorCount == 0 || GetInactivePageAllocatorCount() > (uint)Js::Configuration::Global.flags.JITServerMaxInactivePageAllocatorCount)
    {TRACE_IT(14886);
        PageAllocatorPool::IdleCleanup();
    }
}

void PageAllocatorPool::IdleCleanup()
{TRACE_IT(14887);
    AutoCriticalSection autoCS(&cs);
    if (Instance)
    {TRACE_IT(14888);
        LARGE_INTEGER liDueTime;
        liDueTime.QuadPart = Js::Configuration::Global.flags.JITServerIdleTimeout * -10000000LL; // wait for 10 seconds to do the cleanup

        // If the timer is already active when you call SetWaitableTimer, the timer is stopped, then it is reactivated.
        if (!SetWaitableTimer(Instance->idleCleanupTimer, &liDueTime, 0, IdleCleanupRoutine, NULL, 0))
        {TRACE_IT(14889);
            Instance->RemoveAll();
        }
    }
}

VOID CALLBACK PageAllocatorPool::IdleCleanupRoutine(
    _In_opt_ LPVOID lpArgToCompletionRoutine,
    _In_     DWORD  dwTimerLowValue,
    _In_     DWORD  dwTimerHighValue)
{TRACE_IT(14890);
    AutoCriticalSection autoCS(&cs);
    if (Instance)
    {TRACE_IT(14891);
        // TODO: OOP JIT, use better stragtegy to do the cleanup, like do not remove all,
        // instead keep couple inactivate page allocator for next calls
        Instance->RemoveAll();
    }
}
#endif
