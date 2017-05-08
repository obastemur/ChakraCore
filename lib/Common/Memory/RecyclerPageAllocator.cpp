//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

RecyclerPageAllocator::RecyclerPageAllocator(Recycler* recycler, AllocationPolicyManager * policyManager,
#ifndef JD_PRIVATE
    Js::ConfigFlagsTable& flagTable,
#endif
    uint maxFreePageCount, uint maxAllocPageCount, bool enableWriteBarrier)
    : IdleDecommitPageAllocator(policyManager,
        PageAllocatorType_Recycler,
#ifndef JD_PRIVATE
        flagTable,
#endif
        0, maxFreePageCount,
        true,
#if ENABLE_BACKGROUND_PAGE_ZEROING
        &zeroPageQueue,
#endif
        maxAllocPageCount,
        enableWriteBarrier
        )
{TRACE_IT(26377);
    this->recycler = recycler;
}

bool RecyclerPageAllocator::IsMemProtectMode()
{TRACE_IT(26378);
    return recycler->IsMemProtectMode();
}

#if ENABLE_CONCURRENT_GC
#ifdef RECYCLER_WRITE_WATCH
void
RecyclerPageAllocator::EnableWriteWatch()
{TRACE_IT(26379);
    Assert(segments.Empty());
    Assert(fullSegments.Empty());
    Assert(emptySegments.Empty());
    Assert(decommitSegments.Empty());
    Assert(largeSegments.Empty());

    allocFlags = MEM_WRITE_WATCH;
}

bool
RecyclerPageAllocator::ResetWriteWatch()
{TRACE_IT(26380);
    if (!IsWriteWatchEnabled())
    {TRACE_IT(26381);
        return false;
    }

    GCETW(GC_RESETWRITEWATCH_START, (this));

    SuspendIdleDecommit();

    bool success = true;
    // Only reset write watch on allocated pages
    if (!ResetWriteWatch(&segments) ||
        !ResetWriteWatch(&decommitSegments) ||
        !ResetAllWriteWatch(&fullSegments) ||
        !ResetAllWriteWatch(&largeSegments))
    {TRACE_IT(26382);
        allocFlags = 0;
        success = false;
    }

    ResumeIdleDecommit();

    GCETW(GC_RESETWRITEWATCH_STOP, (this));

    return success;
}

bool
RecyclerPageAllocator::ResetWriteWatch(DListBase<PageSegment> * segmentList)
{TRACE_IT(26383);
    DListBase<PageSegment>::Iterator i(segmentList);
    while (i.Next())
    {TRACE_IT(26384);
        PageSegment& segment = i.Data();
        size_t pageCount = segment.GetAvailablePageCount();
        Assert(pageCount <= MAXUINT32);
        PageSegment::PageBitVector unallocPages = segment.GetUnAllocatedPages();
        for (uint index = 0u; index < pageCount; index++)
        {TRACE_IT(26385);
            if (unallocPages.Test(index))
            {TRACE_IT(26386);
                continue;
            }
            char * address = segment.GetAddress() + index * AutoSystemInfo::PageSize;
            if (::ResetWriteWatch(address, AutoSystemInfo::PageSize) != 0)

            {TRACE_IT(26387);
#if DBG_DUMP
                Output::Print(_u("ResetWriteWatch failed for %p\n"), address);
                Output::Flush();
#endif
                // shouldn't happen
                Assert(false);
                return false;
            }
        }
    }
    return true;
}

template <typename T>
bool
RecyclerPageAllocator::ResetAllWriteWatch(DListBase<T> * segmentList)
{TRACE_IT(26388);
    typename DListBase<T>::Iterator i(segmentList);
    while (i.Next())
    {TRACE_IT(26389);
        T& segment = i.Data();
        if (::ResetWriteWatch(segment.GetAddress(),  segment.GetPageCount() * AutoSystemInfo::PageSize ) != 0)
        {TRACE_IT(26390);
#if DBG_DUMP
            Output::Print(_u("ResetWriteWatch failed for %p\n"), segment.GetAddress());
            Output::Flush();
#endif
            // shouldn't happen
            Assert(false);
            return false;
        }
    }
    return true;
}
#endif

#ifdef RECYCLER_WRITE_WATCH
#if DBG
size_t
RecyclerPageAllocator::GetWriteWatchPageCount()
{TRACE_IT(26391);
    if (allocFlags != MEM_WRITE_WATCH)
    {TRACE_IT(26392);
        return 0;
    }

    SuspendIdleDecommit();

    // Only reset write watch on allocated pages
    size_t count = GetWriteWatchPageCount(&segments)
        + GetWriteWatchPageCount(&decommitSegments)
        + GetAllWriteWatchPageCount(&fullSegments)
        + GetAllWriteWatchPageCount(&largeSegments);

    ResumeIdleDecommit();

    return count;
}


size_t
RecyclerPageAllocator::GetWriteWatchPageCount(DListBase<PageSegment> * segmentList)
{TRACE_IT(26393);
    size_t totalCount = 0;
    DListBase<PageSegment>::Iterator i(segmentList);
    while (i.Next())
    {TRACE_IT(26394);
        PageSegment& segment = i.Data();
        size_t pageCount = segment.GetAvailablePageCount();
        Assert(pageCount <= MAXUINT32);
        PageSegment::PageBitVector unallocPages = segment.GetUnAllocatedPages();
        for (uint index = 0u; index < pageCount; index++)
        {TRACE_IT(26395);
            if (unallocPages.Test(index))
            {TRACE_IT(26396);
                continue;
            }
            char * address = segment.GetAddress() + index * AutoSystemInfo::PageSize;
            void * written;
            ULONG_PTR count = 0;
            DWORD pageSize = AutoSystemInfo::PageSize;
            if (::GetWriteWatch(0, address, AutoSystemInfo::PageSize, &written, &count, &pageSize) == 0)
            {TRACE_IT(26397);
#if DBG_DUMP
                Output::Print(_u("GetWriteWatch failed for %p\n"), segment.GetAddress());
                Output::Flush();
#endif
                // shouldn't happen
                Assert(false);
            }
            else
            {TRACE_IT(26398);
                Assert(count <= 1);
                Assert(pageSize == AutoSystemInfo::PageSize);
                Assert(count == 0 || written == address);
                totalCount += count;
            }
        }
    }
    return totalCount;
}

template <typename T>
size_t
RecyclerPageAllocator::GetAllWriteWatchPageCount(DListBase<T> * segmentList)
{TRACE_IT(26399);
    size_t totalCount = 0;
    _TYPENAME DListBase<T>::Iterator it(segmentList);
    while (it.Next())
    {TRACE_IT(26400);
        T& segment = it.Data();
        for (uint i = 0; i < segment.GetPageCount(); i++)
        {TRACE_IT(26401);
            void * address = segment.GetAddress() + i * AutoSystemInfo::PageSize;
            void * written;
            ULONG_PTR count = 0;
            DWORD pageSize = AutoSystemInfo::PageSize;
            if (::GetWriteWatch(0, address, AutoSystemInfo::PageSize, &written, &count, &pageSize) == 0)
            {TRACE_IT(26402);
#if DBG_DUMP
                Output::Print(_u("GetWriteWatch failed for %p\n"), segment.GetAddress());
                Output::Flush();
#endif
                // shouldn't happen
                Assert(false);
            }
            else
            {TRACE_IT(26403);
                Assert(count <= 1);
                Assert(pageSize == AutoSystemInfo::PageSize);
                Assert(count == 0 || written == address);
                totalCount += count;
            }
        }
    }
    return totalCount;
}
#endif
#endif
#endif
