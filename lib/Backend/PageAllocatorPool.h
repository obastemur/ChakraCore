//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

class PageAllocatorPool
{
    friend class AutoReturnPageAllocator;
public:
    PageAllocatorPool();
    ~PageAllocatorPool();
    void RemoveAll();

    static void Initialize();
    static void Shutdown();
    static void IdleCleanup();
private:

    static VOID CALLBACK IdleCleanupRoutine(
        _In_opt_ LPVOID lpArgToCompletionRoutine,
        _In_     DWORD  dwTimerLowValue,
        _In_     DWORD  dwTimerHighValue);

    PageAllocator* GetPageAllocator();
    void ReturnPageAllocator(PageAllocator* pageAllocator);
    unsigned int GetInactivePageAllocatorCount();

    SList<PageAllocator*, NoThrowHeapAllocator, RealCount> pageAllocators;
    static CriticalSection cs;
    static PageAllocatorPool* Instance;
    HANDLE idleCleanupTimer;
    volatile unsigned long long activePageAllocatorCount;
};

class AutoReturnPageAllocator
{
public:
    AutoReturnPageAllocator() :pageAllocator(nullptr) {TRACE_IT(14892);}
    ~AutoReturnPageAllocator()
    {TRACE_IT(14893);
        if (pageAllocator)
        {TRACE_IT(14894);
            PageAllocatorPool::Instance->ReturnPageAllocator(pageAllocator);
        }
    }
    PageAllocator* GetPageAllocator()
    {TRACE_IT(14895);
        if (pageAllocator == nullptr)
        {TRACE_IT(14896);
            pageAllocator = PageAllocatorPool::Instance->GetPageAllocator();
        }

        return pageAllocator;
    }

private:
    PageAllocator* pageAllocator;
};
