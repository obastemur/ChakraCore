//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

template <typename TBlockType>
SmallHeapBlockAllocator<TBlockType>::SmallHeapBlockAllocator() :
    freeObjectList(nullptr),
    endAddress(nullptr),
    heapBlock(nullptr),
    prev(nullptr),
    next(nullptr)
{TRACE_IT(26927);
#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
    this->lastNonNativeBumpAllocatedBlock = nullptr;
#endif
}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::Initialize()
{TRACE_IT(26928);
    Assert(this->freeObjectList == nullptr);
    Assert(this->endAddress == nullptr);
    Assert(this->heapBlock == nullptr);

    this->prev = this;
    this->next = this;
}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::UpdateHeapBlock()
{TRACE_IT(26929);
    if (heapBlock != nullptr)
    {TRACE_IT(26930);
        if (this->endAddress == nullptr)
        {TRACE_IT(26931);
            heapBlock->freeObjectList = this->freeObjectList;
        }
        else
        {TRACE_IT(26932);
            Assert(heapBlock->freeObjectList == nullptr);
        }
    }
}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::Clear()
{TRACE_IT(26933);
    TBlockType * heapBlock = this->heapBlock;
    if (heapBlock != nullptr)
    {TRACE_IT(26934);
        Assert(heapBlock->isInAllocator);
        heapBlock->isInAllocator = false;
        FreeObject * remainingFreeObjectList = nullptr;
        if (this->endAddress != nullptr)
        {TRACE_IT(26935);
#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
            TrackNativeAllocatedObjects();
            lastNonNativeBumpAllocatedBlock = nullptr;
#endif
#ifdef PROFILE_RECYCLER_ALLOC
            // Need to tell the tracker
            this->bucket->heapInfo->recycler->TrackUnallocated((char *)this->freeObjectList, this->endAddress, this->bucket->sizeCat);
#endif
            RecyclerMemoryTracking::ReportUnallocated(this->heapBlock->heapBucket->heapInfo->recycler, (char *)this->freeObjectList, this->endAddress, heapBlock->heapBucket->sizeCat);
#ifdef RECYCLER_PERF_COUNTERS
            size_t unallocatedObjects = heapBlock->objectCount - ((char *)this->freeObjectList - heapBlock->address) / heapBlock->objectSize;
            size_t unallocatedObjectBytes = unallocatedObjects * heapBlock->GetObjectSize();
            RECYCLER_PERF_COUNTER_ADD(LiveObject, unallocatedObjects);
            RECYCLER_PERF_COUNTER_ADD(LiveObjectSize, unallocatedObjectBytes);
            RECYCLER_PERF_COUNTER_SUB(FreeObjectSize, unallocatedObjectBytes);
            RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObject, unallocatedObjects);
            RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObjectSize, unallocatedObjectBytes);
            RECYCLER_PERF_COUNTER_SUB(SmallHeapBlockFreeObjectSize, unallocatedObjectBytes);
#endif
            Assert(heapBlock->freeObjectList == nullptr);
            this->endAddress = nullptr;
        }
        else
        {TRACE_IT(26936);
            remainingFreeObjectList = this->freeObjectList;
            heapBlock->freeObjectList = remainingFreeObjectList;
        }
        this->freeObjectList = nullptr;

        // this->freeObjectList and this->lastFreeCount are accessed in SmallHeapBlock::ResetMarks
        // the order of access there is first we see if lastFreeCount = 0, and if it is, we assert
        // that freeObjectList = null. Because of ARM's memory model, we need to insert barriers
        // so that the two variables can be accessed correctly across threads. Here, after we write
        // to this->freeObjectList, we insert a write barrier so that if this->lastFreeCount is 0,
        // this->freeObjectList must have been set to null. On the other end, we stick a read barrier
        // We use the MemoryBarrier macro because of ARMs lack of a separate read barrier
#if defined(_M_ARM32_OR_ARM64)
#if DBG
        MemoryBarrier();
#endif
#endif

        if (remainingFreeObjectList == nullptr)
        {TRACE_IT(26937);
            uint lastFreeCount = heapBlock->GetAndClearLastFreeCount();
            heapBlock->heapBucket->heapInfo->uncollectedAllocBytes += lastFreeCount * heapBlock->GetObjectSize();
            Assert(heapBlock->lastUncollectedAllocBytes == 0);
            DebugOnly(heapBlock->lastUncollectedAllocBytes = lastFreeCount * heapBlock->GetObjectSize());
        }
        else
        {TRACE_IT(26938);
            DebugOnly(heapBlock->SetIsClearedFromAllocator(true));
        }
        this->heapBlock = nullptr;

        RECYCLER_SLOW_CHECK(heapBlock->CheckDebugFreeBitVector(false));
    }
    else if (this->freeObjectList != nullptr)
    {TRACE_IT(26939);
        // Explicit Free Object List
#ifdef RECYCLER_MEMORY_VERIFY
        FreeObject* freeObject = this->freeObjectList;

        while (freeObject)
        {TRACE_IT(26940);
            HeapBlock* heapBlockVerify = this->bucket->GetRecycler()->FindHeapBlock((void*) freeObject);
            Assert(heapBlockVerify != nullptr);
            Assert(!heapBlockVerify->IsLargeHeapBlock());
            TBlockType* smallBlock = (TBlockType*)heapBlockVerify;

            smallBlock->ClearExplicitFreeBitForObject((void*) freeObject);
            freeObject = freeObject->GetNext();
        }
#endif
        this->freeObjectList = nullptr;
    }

}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::SetNew(BlockType * heapBlock)
{TRACE_IT(26941);
    Assert(this->endAddress == nullptr);
    Assert(this->heapBlock == nullptr);
    Assert(this->freeObjectList == nullptr);

    Assert(heapBlock != nullptr);
    Assert(heapBlock->freeObjectList == nullptr);
    Assert(heapBlock->lastFreeCount != 0);

    Assert(!heapBlock->isInAllocator);
    heapBlock->isInAllocator = true;

    this->heapBlock = heapBlock;
    this->freeObjectList = (FreeObject *)heapBlock->GetAddress();
    this->endAddress = heapBlock->GetEndAddress();
}

template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::Set(BlockType * heapBlock)
{TRACE_IT(26942);
    Assert(this->endAddress == nullptr);
    Assert(this->heapBlock == nullptr);
    Assert(this->freeObjectList == nullptr);

    Assert(heapBlock != nullptr);
    Assert(heapBlock->freeObjectList != nullptr);
    Assert(heapBlock->lastFreeCount != 0);

    Assert(!heapBlock->isInAllocator);
    heapBlock->isInAllocator = true;

    this->heapBlock = heapBlock;
    RECYCLER_SLOW_CHECK(this->heapBlock->CheckDebugFreeBitVector(true));
    this->freeObjectList = this->heapBlock->freeObjectList;
}


template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::SetExplicitFreeList(FreeObject* list)
{TRACE_IT(26943);
    Assert(list != nullptr);
    Assert(this->heapBlock == nullptr);
    Assert(this->freeObjectList == nullptr);

    this->freeObjectList = list;
}

#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
template <typename TBlockType>
void
SmallHeapBlockAllocator<TBlockType>::TrackNativeAllocatedObjects()
{TRACE_IT(26944);
    Assert(this->freeObjectList != nullptr && endAddress != nullptr);
    Assert(this->heapBlock != nullptr);

#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(ETW_MEMORY_TRACKING)
    if (pfnTrackNativeAllocatedObjectCallBack == nullptr)
    {TRACE_IT(26945);
        return;
    }

    if (lastNonNativeBumpAllocatedBlock == nullptr)
    {TRACE_IT(26946);
        Assert((char *)this->freeObjectList == this->heapBlock->GetAddress());
        return;
    }

    Recycler * recycler = this->heapBlock->heapBucket->heapInfo->recycler;
    size_t sizeCat = this->heapBlock->heapBucket->sizeCat;
    char * curr = lastNonNativeBumpAllocatedBlock + sizeCat;
    Assert(curr <= (char *)this->freeObjectList);

#if DBG_DUMP
    AllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), _u("TrackNativeAllocatedObjects: recycler = 0x%p, sizeCat = %u, lastRuntimeAllocatedBlock = 0x%p, freeObjectList = 0x%p, nativeAllocatedObjectCount = %u\n"),
        recycler, sizeCat, this->lastNonNativeBumpAllocatedBlock, this->freeObjectList, ((char *)this->freeObjectList - curr) / sizeCat);
#endif

    while (curr < (char *)this->freeObjectList)
    {
        pfnTrackNativeAllocatedObjectCallBack(recycler, curr, sizeCat);
        curr += sizeCat;
    }
#elif defined(RECYCLER_PERF_COUNTERS)
    if (lastNonNativeBumpAllocatedBlock == nullptr)
    {TRACE_IT(26947);
        return;
    }

    size_t sizeCat = this->heapBlock->heapBucket->sizeCat;
    char * curr = lastNonNativeBumpAllocatedBlock + sizeCat;
    Assert(curr <= (char *)this->freeObjectList);
    size_t byteCount = ((char *)this->freeObjectList - curr);

#if DBG_DUMP
    AllocationVerboseTrace(_u("TrackNativeAllocatedObjects: recycler = 0x%p, sizeCat = %u, lastRuntimeAllocatedBlock = 0x%p, freeObjectList = 0x%p, nativeAllocatedObjectCount = %u\n"),
        recycler, sizeCat, this->lastNonNativeBumpAllocatedBlock, this->freeObjectList, ((char *)this->freeObjectList - curr) / sizeCat);
#endif

    RECYCLER_PERF_COUNTER_ADD(LiveObject, byteCount / sizeCat);
    RECYCLER_PERF_COUNTER_ADD(LiveObjectSize, byteCount);
    RECYCLER_PERF_COUNTER_SUB(FreeObjectSize, byteCount);
    RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObject, byteCount / sizeCat);
    RECYCLER_PERF_COUNTER_ADD(SmallHeapBlockLiveObjectSize, byteCount);
    RECYCLER_PERF_COUNTER_SUB(SmallHeapBlockFreeObjectSize, byteCount);
#else
#error Not implemented
#endif
}
#endif

namespace Memory
{
    EXPLICIT_INSTANTIATE_WITH_SMALL_HEAP_BLOCK_TYPE(SmallHeapBlockAllocator)

    template _ALWAYSINLINE char* SmallHeapBlockAllocator<SmallNormalHeapBlock>::InlinedAllocImpl</*canFaultInject*/true>(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes);
}
