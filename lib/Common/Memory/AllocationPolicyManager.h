//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
//
// AllocationPolicyManager allows a caller/host to disallow new page allocations
// and to track current page usage.
//

// NOTE: For now, we are only tracking reserved page count.
// Consider whether we should also (or maybe only) track committed page count.

class AllocationPolicyManager
{
public:
    enum MemoryAllocateEvent
    {
        MemoryAllocate = 0,
        MemoryFree = 1,
        MemoryFailure = 2,
        MemoryMax = 2,
    };
typedef bool (__stdcall * PageAllocatorMemoryAllocationCallback)(__in LPVOID context,
    __in AllocationPolicyManager::MemoryAllocateEvent allocationEvent,
    __in size_t allocationSize);


private:
    size_t memoryLimit;
    size_t currentMemory;
    bool supportConcurrency;
    CriticalSection cs;
    void * context;
    PageAllocatorMemoryAllocationCallback memoryAllocationCallback;

public:
    AllocationPolicyManager(bool needConcurrencySupport) :
        memoryLimit((size_t)-1),
        currentMemory(0),
        supportConcurrency(needConcurrencySupport),
        context(NULL),
        memoryAllocationCallback(NULL)
    {TRACE_IT(22613);
    }

    ~AllocationPolicyManager()
    {TRACE_IT(22614);
        Assert(currentMemory == 0);
    }

    size_t GetUsage()
    {TRACE_IT(22615);
        return currentMemory;
    }

    size_t GetLimit()
    {TRACE_IT(22616);
        return memoryLimit;
    }

    void SetLimit(size_t newLimit)
    {TRACE_IT(22617);
        memoryLimit = newLimit;
    }

    bool RequestAlloc(DECLSPEC_GUARD_OVERFLOW size_t byteCount, bool externalAlloc = false)
    {TRACE_IT(22618);
        if (supportConcurrency)
        {TRACE_IT(22619);
            AutoCriticalSection auto_cs(&cs);
            return RequestAllocImpl(byteCount, externalAlloc);
        }
        else
        {TRACE_IT(22620);
            return RequestAllocImpl(byteCount, externalAlloc);
        }
    }


    void ReportFailure(size_t byteCount)
    {TRACE_IT(22621);
        if (supportConcurrency)
        {TRACE_IT(22622);
            AutoCriticalSection auto_cs(&cs);
            ReportFreeImpl(MemoryAllocateEvent::MemoryFailure, byteCount);
        }
        else
        {TRACE_IT(22623);
            ReportFreeImpl(MemoryAllocateEvent::MemoryFailure, byteCount);
        }

    }

    void ReportFree(size_t byteCount)
    {TRACE_IT(22624);
        if (supportConcurrency)
        {TRACE_IT(22625);
            AutoCriticalSection auto_cs(&cs);
            ReportFreeImpl(MemoryAllocateEvent::MemoryFree, byteCount);
        }
        else
        {TRACE_IT(22626);
            ReportFreeImpl(MemoryAllocateEvent::MemoryFree, byteCount);
        }
    }

    void SetMemoryAllocationCallback(LPVOID newContext, PageAllocatorMemoryAllocationCallback callback)
    {TRACE_IT(22627);
        this->memoryAllocationCallback = callback;

        if (callback == NULL)
        {TRACE_IT(22628);
            // doesn't make sense to have non-null context when the callback is NULL.
            this->context = NULL;
        }
        else
        {TRACE_IT(22629);
            this->context = newContext;
        }
    }

private:
    inline bool RequestAllocImpl(size_t byteCount, bool externalAlloc = false)
    {TRACE_IT(22630);
        size_t newCurrentMemory = currentMemory + byteCount;

        if (newCurrentMemory < currentMemory ||
            newCurrentMemory > memoryLimit ||
            (memoryAllocationCallback != NULL && !memoryAllocationCallback(context, MemoryAllocateEvent::MemoryAllocate, byteCount)))
        {TRACE_IT(22631);
            if (memoryAllocationCallback != NULL)
            {
                memoryAllocationCallback(context, MemoryAllocateEvent::MemoryFailure, byteCount);
            }
            
            // oopjit number allocator allocated pages, we can't stop it from allocating so just increase the usage number
            if (externalAlloc)
            {TRACE_IT(22632);
                currentMemory = newCurrentMemory;
            }

            return false;
        }
        else
        {TRACE_IT(22633);
            currentMemory = newCurrentMemory;
            return true;
        }
    }

    inline void ReportFreeImpl(MemoryAllocateEvent allocationEvent, size_t byteCount)
    {TRACE_IT(22634);
        Assert(currentMemory >= byteCount);

        currentMemory = currentMemory - byteCount;

        if (memoryAllocationCallback != NULL)
        {
            // The callback should be minimal, with no possibility of calling back to us.
            // Note that this can be called both in script or out of script.
            memoryAllocationCallback(context, allocationEvent, byteCount);
        }
    }
};
