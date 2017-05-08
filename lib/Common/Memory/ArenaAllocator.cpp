//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#define ASSERT_THREAD() AssertMsg(this->pageAllocator->ValidThreadAccess(), "Arena allocation should only be used by a single thread")

// The VS2013 linker treats this as a redefinition of an already
// defined constant and complains. So skip the declaration if we're compiling
// with VS2013 or below.
#if !defined(_MSC_VER) || _MSC_VER >= 1900
const uint Memory::StandAloneFreeListPolicy::MaxEntriesGrowth;
#endif

// We need this function to be inlined for perf
template _ALWAYSINLINE BVSparseNode<JitArenaAllocator> * BVSparse<JitArenaAllocator>::NodeFromIndex(BVIndex i, Field(BVSparseNode*, JitArenaAllocator)** prevNextFieldOut, bool create);

ArenaData::ArenaData(PageAllocator * pageAllocator) :
    pageAllocator(pageAllocator),
    bigBlocks(nullptr),
    mallocBlocks(nullptr),
    fullBlocks(nullptr),
    cacheBlockCurrent(nullptr),
    lockBlockList(false)
{TRACE_IT(22677);
}

void ArenaData::UpdateCacheBlock() const
{TRACE_IT(22678);
    if (bigBlocks != nullptr)
    {TRACE_IT(22679);
        size_t currentByte = (cacheBlockCurrent - bigBlocks->GetBytes());
        // Avoid writing to the page unnecessary, it might be write watched
        if (currentByte != bigBlocks->currentByte)
        {TRACE_IT(22680);
            bigBlocks->currentByte = currentByte;
        }
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
ArenaAllocatorBase(__in LPCWSTR name, PageAllocator * pageAllocator, void(*outOfMemoryFunc)(), void(*recoverMemoryFunc)()) :
    Allocator(outOfMemoryFunc, recoverMemoryFunc),
    ArenaData(pageAllocator),
#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
    freeListSize(0),
#endif
    freeList(nullptr),
    largestHole(0),
    cacheBlockEnd(nullptr),
    blockState(0)
{TRACE_IT(22681);
#ifdef PROFILE_MEM
    this->name = name;
    LogBegin();
#endif
#if DBG
    needsDelayFreeList = false;
#endif
    ArenaMemoryTracking::ArenaCreated(this, name);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
~ArenaAllocatorBase()
{TRACE_IT(22682);
    Assert(!lockBlockList);
    ArenaMemoryTracking::ReportFreeAll(this);
    ArenaMemoryTracking::ArenaDestroyed(this);

    if (!pageAllocator->IsClosed())
    {TRACE_IT(22683);
        ReleasePageMemory();
    }
    ReleaseHeapMemory();
    TFreeListPolicy::Release(this->freeList);
#ifdef PROFILE_MEM
    LogEnd();
#endif

}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Move(ArenaAllocatorBase *srcAllocator)
{TRACE_IT(22684);
    Assert(!lockBlockList);
    Assert(srcAllocator != nullptr);
    Allocator::Move(srcAllocator);

    Assert(this->pageAllocator == srcAllocator->pageAllocator);
    AllocatorFieldMove(this, srcAllocator, bigBlocks);
    AllocatorFieldMove(this, srcAllocator, largestHole);
    AllocatorFieldMove(this, srcAllocator, cacheBlockCurrent);
    AllocatorFieldMove(this, srcAllocator, cacheBlockEnd);
    AllocatorFieldMove(this, srcAllocator, mallocBlocks);
    AllocatorFieldMove(this, srcAllocator, fullBlocks);
    AllocatorFieldMove(this, srcAllocator, blockState);
    AllocatorFieldMove(this, srcAllocator, freeList);

#ifdef PROFILE_MEM
    this->name = srcAllocator->name;
    srcAllocator->name = nullptr;
    AllocatorFieldMove(this, srcAllocator, memoryData);
#endif
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
size_t
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AllocatedSize(ArenaMemoryBlock * blockList)
{TRACE_IT(22685);
    ArenaMemoryBlock * memoryBlock = blockList;
    size_t totalBytes = 0;
    while (memoryBlock != NULL)
    {TRACE_IT(22686);
        totalBytes += memoryBlock->nbytes;
        memoryBlock = memoryBlock->next;
    }
    return totalBytes;
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
size_t
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AllocatedSize()
{TRACE_IT(22687);
    UpdateCacheBlock();
    return AllocatedSize(this->fullBlocks) + AllocatedSize(this->bigBlocks) + AllocatedSize(this->mallocBlocks);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
size_t
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Size()
{TRACE_IT(22688);
    UpdateCacheBlock();
    return Size(this->fullBlocks) + Size(this->bigBlocks) + AllocatedSize(this->mallocBlocks);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
size_t
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Size(BigBlock * blockList)
{TRACE_IT(22689);
    BigBlock * memoryBlock = blockList;
    size_t totalBytes = 0;
    while (memoryBlock != NULL)
    {TRACE_IT(22690);
        totalBytes += memoryBlock->currentByte;
        memoryBlock = (BigBlock *)memoryBlock->next;
    }
    return totalBytes;
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
RealAlloc(size_t nbytes)
{TRACE_IT(22691);
    return RealAllocInlined(nbytes);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
RealAllocInlined(size_t nbytes)
{TRACE_IT(22692);
    Assert(nbytes != 0);

#ifdef ARENA_MEMORY_VERIFY
    if (Js::Configuration::Global.flags.ArenaUseHeapAlloc)
    {TRACE_IT(22693);
        return AllocFromHeap<true>(nbytes);
    }
#endif

    Assert(cacheBlockEnd >= cacheBlockCurrent);
    char * p = cacheBlockCurrent;
    if ((size_t)(cacheBlockEnd - p) >= nbytes)
    {TRACE_IT(22694);
        Assert(cacheBlockEnd == bigBlocks->GetBytes() + bigBlocks->nbytes);
        Assert(bigBlocks->GetBytes() <= cacheBlockCurrent && cacheBlockCurrent <= cacheBlockEnd);
        cacheBlockCurrent = p + nbytes;

        ArenaMemoryTracking::ReportAllocation(this, p, nbytes);

        return(p);
    }

    return SnailAlloc(nbytes);

}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
SetCacheBlock(BigBlock * newCacheBlock)
{TRACE_IT(22695);
    if (bigBlocks != nullptr)
    {TRACE_IT(22696);
        Assert(cacheBlockEnd == bigBlocks->GetBytes() + bigBlocks->nbytes);
        Assert(bigBlocks->GetBytes() <= cacheBlockCurrent && cacheBlockCurrent <= cacheBlockEnd);

        bigBlocks->currentByte = (cacheBlockCurrent - bigBlocks->GetBytes());
        uint cacheBlockRemainBytes = (uint)(cacheBlockEnd - cacheBlockCurrent);
        if (cacheBlockRemainBytes < ObjectAlignment && !lockBlockList)
        {TRACE_IT(22697);
            BigBlock * cacheBlock = bigBlocks;
            bigBlocks = bigBlocks->nextBigBlock;
            cacheBlock->next = fullBlocks;
            fullBlocks = cacheBlock;
        }
        else
        {TRACE_IT(22698);
            largestHole = max(largestHole, static_cast<size_t>(cacheBlockRemainBytes));
        }
    }
    cacheBlockCurrent = newCacheBlock->GetBytes() + newCacheBlock->currentByte;
    cacheBlockEnd = newCacheBlock->GetBytes() + newCacheBlock->nbytes;
    newCacheBlock->nextBigBlock = bigBlocks;
    bigBlocks = newCacheBlock;
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
SnailAlloc(size_t nbytes)
{TRACE_IT(22699);
    BigBlock* blockp = NULL;
    size_t currentLargestHole = 0;

    if (nbytes <= largestHole)
    {TRACE_IT(22700);
        Assert(bigBlocks != nullptr);
        Assert(cacheBlockEnd == bigBlocks->GetBytes() + bigBlocks->nbytes);
        Assert(bigBlocks->GetBytes() <= cacheBlockCurrent && cacheBlockCurrent <= cacheBlockEnd);

        BigBlock * cacheBlock = bigBlocks;
        BigBlock** pPrev= &(bigBlocks->nextBigBlock);
        blockp = bigBlocks->nextBigBlock;
        int giveUpAfter = 10;
        do
        {TRACE_IT(22701);
            size_t remainingBytes = blockp->nbytes - blockp->currentByte;
            if (remainingBytes >= nbytes)
            {TRACE_IT(22702);
                char *p = blockp->GetBytes() + blockp->currentByte;
                blockp->currentByte += nbytes;
                if (remainingBytes == largestHole || currentLargestHole > largestHole)
                {TRACE_IT(22703);
                    largestHole = currentLargestHole;
                }
                remainingBytes -= nbytes;
                if (remainingBytes > cacheBlock->nbytes - cacheBlock->currentByte)
                {TRACE_IT(22704);
                    *pPrev = blockp->nextBigBlock;
                    SetCacheBlock(blockp);
                }
                else if (remainingBytes < ObjectAlignment && !lockBlockList)
                {TRACE_IT(22705);
                    *pPrev = blockp->nextBigBlock;
                    blockp->nextBigBlock = fullBlocks;
                    fullBlocks = blockp;
                }

                ArenaMemoryTracking::ReportAllocation(this, p, nbytes);

                return(p);
            }
            currentLargestHole = max(currentLargestHole, remainingBytes);
            if (--giveUpAfter == 0)
            {TRACE_IT(22706);
                break;
            }
            pPrev = &(blockp->nextBigBlock);
            blockp = blockp->nextBigBlock;
        }
        while (blockp != nullptr);
    }

    blockp = AddBigBlock(nbytes);
    if (blockp == nullptr)
    {TRACE_IT(22707);
        return AllocFromHeap<false>(nbytes);    // Passing DoRecoverMemory=false as we already tried recovering memory in AddBigBlock, and it is costly.
    }

    this->blockState++;
    SetCacheBlock(blockp);
    char *p = cacheBlockCurrent;
    Assert(p + nbytes <= cacheBlockEnd);
    cacheBlockCurrent += nbytes;

    ArenaMemoryTracking::ReportAllocation(this, p, nbytes);

    return(p);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
template <bool DoRecoverMemory>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AllocFromHeap(size_t requestBytes)
{TRACE_IT(22708);
    size_t allocBytes = AllocSizeMath::Add(requestBytes, sizeof(ArenaMemoryBlock));

    ARENA_FAULTINJECT_MEMORY(this->name, requestBytes);

    char * buffer = HeapNewNoThrowArray(char, allocBytes);

    if (buffer == nullptr)
    {TRACE_IT(22709);
        if (DoRecoverMemory && recoverMemoryFunc)
        {TRACE_IT(22710);
            // Try to recover some memory and see if after that we can allocate.
            recoverMemoryFunc();
            buffer = HeapNewNoThrowArray(char, allocBytes);
        }

        if (buffer == nullptr)
        {TRACE_IT(22711);
            if (outOfMemoryFunc)
            {TRACE_IT(22712);
                outOfMemoryFunc();
            }
            return nullptr;
        }
    }

    ArenaMemoryBlock * memoryBlock = (ArenaMemoryBlock *)buffer;
    memoryBlock->nbytes = requestBytes;
    memoryBlock->next = this->mallocBlocks;
    this->mallocBlocks = memoryBlock;
    this->blockState = 2;                       // set the block state to 2 to disable the reset fast path.

    ArenaMemoryTracking::ReportAllocation(this, buffer + sizeof(ArenaMemoryBlock), requestBytes);

    return buffer + sizeof(ArenaMemoryBlock);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
BigBlock *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AddBigBlock(size_t requestBytes)
{TRACE_IT(22713);

    FAULTINJECT_MEMORY_NOTHROW(this->name, requestBytes);

    size_t allocBytes = AllocSizeMath::Add(requestBytes, sizeof(BigBlock));

    PageAllocation * allocation = this->GetPageAllocator()->AllocPagesForBytes(allocBytes);

    if (allocation == nullptr)
    {TRACE_IT(22714);
        // Try to recover some memory and see if after that we can allocate.
        if (recoverMemoryFunc)
        {TRACE_IT(22715);
            recoverMemoryFunc();
            allocation = this->GetPageAllocator()->AllocPagesForBytes(allocBytes);
        }
        if (allocation == nullptr)
        {TRACE_IT(22716);
            return nullptr;
        }
    }
    BigBlock * blockp = (BigBlock *)allocation->GetAddress();
    blockp->allocation = allocation;
    blockp->nbytes = allocation->GetSize() - sizeof(BigBlock);
    blockp->currentByte = 0;

#ifdef PROFILE_MEM
    LogRealAlloc(allocation->GetSize() + sizeof(PageAllocation));
#endif
    return(blockp);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
FullReset()
{TRACE_IT(22717);
    BigBlock * initBlock = this->bigBlocks;
    if (initBlock != nullptr)
    {TRACE_IT(22718);
        this->bigBlocks = initBlock->nextBigBlock;
    }
    Clear();
    if (initBlock != nullptr)
    {TRACE_IT(22719);
        this->blockState = 1;
        initBlock->currentByte = 0;
        SetCacheBlock(initBlock);
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
ReleaseMemory()
{TRACE_IT(22720);
    ReleasePageMemory();
    ReleaseHeapMemory();
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
ReleasePageMemory()
{TRACE_IT(22721);
    pageAllocator->SuspendIdleDecommit();
#ifdef ARENA_MEMORY_VERIFY
    bool reenableDisablePageReuse = false;
    if (Js::Configuration::Global.flags.ArenaNoPageReuse)
    {TRACE_IT(22722);
        reenableDisablePageReuse = !pageAllocator->DisablePageReuse();
    }
#endif
    BigBlock *blockp = bigBlocks;
    while (blockp != NULL)
    {TRACE_IT(22723);
        PageAllocation * allocation = blockp->allocation;
        blockp = blockp->nextBigBlock;
        GetPageAllocator()->ReleaseAllocationNoSuspend(allocation);
    }

    blockp = fullBlocks;
    while (blockp != NULL)
    {TRACE_IT(22724);
        PageAllocation * allocation = blockp->allocation;
        blockp = blockp->nextBigBlock;
        GetPageAllocator()->ReleaseAllocationNoSuspend(allocation);
    }

#ifdef ARENA_MEMORY_VERIFY
    if (reenableDisablePageReuse)
    {TRACE_IT(22725);
        pageAllocator->ReenablePageReuse();
    }
#endif

    pageAllocator->ResumeIdleDecommit();
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
ReleaseHeapMemory()
{TRACE_IT(22726);
    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22727);
        ArenaMemoryBlock * next = memoryBlock->next;
        HeapDeleteArray(memoryBlock->nbytes + sizeof(ArenaMemoryBlock), (char *)memoryBlock);
        memoryBlock = next;
    }
}

template _ALWAYSINLINE char *ArenaAllocatorBase<InPlaceFreeListPolicy, 0, 0, 0>::AllocInternal(size_t requestedBytes);

#if !(defined(__clang__) && defined(_M_IX86_OR_ARM32))
// otherwise duplicate instantination of AllocInternal Error
template _ALWAYSINLINE char *ArenaAllocatorBase<InPlaceFreeListPolicy, 3, 0, 0>::AllocInternal(size_t requestedBytes);
#endif

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
AllocInternal(size_t requestedBytes)
{TRACE_IT(22728);
    Assert(requestedBytes != 0);

    if (MaxObjectSize > 0)
    {TRACE_IT(22729);
        Assert(requestedBytes <= MaxObjectSize);
    }

    if (RequireObjectAlignment)
    {TRACE_IT(22730);
        Assert(requestedBytes % ObjectAlignment == 0);
    }

    // If out of memory function is set, that means that the caller is a throwing allocation
    // routine, so we can throw from here. Otherwise, we shouldn't throw.
    ARENA_FAULTINJECT_MEMORY(this->name, requestedBytes);

    ASSERT_THREAD();

    size_t nbytes;
    if (freeList != nullptr && requestedBytes > 0 && requestedBytes <= ArenaAllocatorBase::MaxSmallObjectSize)
    {TRACE_IT(22731);
        // We have checked the size requested, so no integer overflow check
        nbytes = Math::Align(requestedBytes, ArenaAllocator::ObjectAlignment);
        Assert(nbytes <= ArenaAllocator::MaxSmallObjectSize);
#ifdef PROFILE_MEM
        LogAlloc(requestedBytes, nbytes);
#endif
        void * freeObject = TFreeListPolicy::Allocate(this->freeList, nbytes);

        if (freeObject != nullptr)
        {TRACE_IT(22732);
#ifdef ARENA_MEMORY_VERIFY
            TFreeListPolicy::VerifyFreeObjectIsFreeMemFilled(freeObject, nbytes);
#endif

#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
            this->freeListSize -= nbytes;
#endif

#ifdef PROFILE_MEM
            LogReuse(nbytes);
#endif
            ArenaMemoryTracking::ReportAllocation(this, freeObject, nbytes);
            return (char *)freeObject;
        }
    }
    else
    {TRACE_IT(22733);
        nbytes = AllocSizeMath::Align(requestedBytes, ArenaAllocator::ObjectAlignment);
#ifdef PROFILE_MEM
        LogAlloc(requestedBytes, nbytes);
#endif
    }
    // TODO: Support large object free listing
    return ArenaAllocatorBase::RealAllocInlined(nbytes);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Free(void * buffer, size_t byteSize)
{TRACE_IT(22734);
    ASSERT_THREAD();
    Assert(byteSize != 0);

    if (MaxObjectSize > 0)
    {TRACE_IT(22735);
        Assert(byteSize <= MaxObjectSize);
    }

    if (RequireObjectAlignment)
    {TRACE_IT(22736);
        Assert(byteSize % ObjectAlignment == 0);
    }

    // Since we successfully allocated, we shouldn't have integer overflow here
    size_t size = Math::Align(byteSize, ArenaAllocator::ObjectAlignment);
    Assert(size >= byteSize);

    ArenaMemoryTracking::ReportFree(this, buffer, byteSize);

#ifdef ARENA_MEMORY_VERIFY
    if (Js::Configuration::Global.flags.ArenaNoFreeList)
    {TRACE_IT(22737);
        return;
    }
#endif
    if (buffer == cacheBlockCurrent - byteSize)
    {TRACE_IT(22738);
#ifdef PROFILE_MEM
        LogFree(byteSize);
#endif
        cacheBlockCurrent = (char *)buffer;
        return;
    }
    else if (this->pageAllocator->IsClosed())
    {TRACE_IT(22739);
        return;
    }
    else if (size <= ArenaAllocator::MaxSmallObjectSize)
    {TRACE_IT(22740);
        // If we plan to free-list this object, we must prepare (typically, debug pattern fill) its memory here, in case we fail to allocate the free list because we're out of memory (see below),
        // and we never get to call TFreeListPolicy::Free.
        TFreeListPolicy::PrepareFreeObject(buffer, size);

        if (freeList == nullptr)
        {TRACE_IT(22741);
            // Caution: TFreeListPolicy::New may fail silently if we're out of memory.
            freeList = TFreeListPolicy::New(this);

            if (freeList == nullptr)
            {TRACE_IT(22742);
                return;
            }
        }

        void **policy = &this->freeList;
#if DBG
        if (needsDelayFreeList)
        {TRACE_IT(22743);
            void *delayFreeList = reinterpret_cast<FreeObject **>(this->freeList) + (MaxSmallObjectSize >> ObjectAlignmentBitShift);
            policy = &delayFreeList;
        }
#endif
        *policy = TFreeListPolicy::Free(*policy, buffer, size);

#ifdef ARENA_ALLOCATOR_FREE_LIST_SIZE
        this->freeListSize += size;
#endif

#ifdef PROFILE_MEM
        LogFree(byteSize);
#endif
        return;
    }

    // TODO: Free list bigger objects
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
char *
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
Realloc(void* buffer, size_t existingBytes, size_t requestedBytes)
{TRACE_IT(22744);
    ASSERT_THREAD();

    if (existingBytes == 0)
    {TRACE_IT(22745);
        Assert(buffer == nullptr);
        return AllocInternal(requestedBytes);
    }

    if (MaxObjectSize > 0)
    {TRACE_IT(22746);
        Assert(requestedBytes <= MaxObjectSize);
    }

    if (RequireObjectAlignment)
    {TRACE_IT(22747);
        Assert(requestedBytes % ObjectAlignment == 0);
    }

    size_t nbytes = AllocSizeMath::Align(requestedBytes, ArenaAllocator::ObjectAlignment);

    // Since we successfully allocated, we shouldn't have integer overflow here
    size_t nbytesExisting = Math::Align(existingBytes, ArenaAllocator::ObjectAlignment);
    Assert(nbytesExisting >= existingBytes);

    if (nbytes == nbytesExisting)
    {TRACE_IT(22748);
        return (char *)buffer;
    }

    if (nbytes < nbytesExisting)
    {TRACE_IT(22749);
        ArenaMemoryTracking::ReportReallocation(this, buffer, nbytesExisting, nbytes);

        Free(((char *)buffer) + nbytes, nbytesExisting - nbytes);
        return (char *)buffer;
    }

    char* replacementBuf = nullptr;
    if (requestedBytes > 0)
    {TRACE_IT(22750);
        replacementBuf = AllocInternal(requestedBytes);
        if (replacementBuf != nullptr)
        {
            js_memcpy_s(replacementBuf, requestedBytes, buffer, existingBytes);
        }
    }

    if (nbytesExisting > 0)
    {
        Free(buffer, nbytesExisting);
    }

    return replacementBuf;
}

#if DBG
template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::MergeDelayFreeList()
{TRACE_IT(22751);
    Assert(needsDelayFreeList);
    TFreeListPolicy::MergeDelayFreeList(freeList);
}
#endif

#ifdef PROFILE_MEM
template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogBegin()
{TRACE_IT(22752);
    memoryData = MemoryProfiler::Begin(this->name);
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogReset()
{TRACE_IT(22753);
    if (memoryData)
    {TRACE_IT(22754);
        MemoryProfiler::Reset(this->name, memoryData);
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogEnd()
{TRACE_IT(22755);
    if (memoryData)
    {TRACE_IT(22756);
        MemoryProfiler::End(this->name, memoryData);
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogAlloc(size_t requestedBytes, size_t allocateBytes)
{TRACE_IT(22757);
    if (memoryData)
    {TRACE_IT(22758);
        memoryData->requestCount++;
        memoryData->requestBytes += requestedBytes;
        memoryData->alignmentBytes += allocateBytes - requestedBytes;
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogRealAlloc(size_t size)
{TRACE_IT(22759);
    if (memoryData)
    {TRACE_IT(22760);
        memoryData->allocatedBytes += size;
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogFree(size_t size)
{TRACE_IT(22761);
    if (memoryData)
    {TRACE_IT(22762);
        memoryData->freelistBytes += size;
        memoryData->freelistCount++;
    }
}

template <class TFreeListPolicy, size_t ObjectAlignmentBitShiftArg, bool RequireObjectAlignment, size_t MaxObjectSize>
void
ArenaAllocatorBase<TFreeListPolicy, ObjectAlignmentBitShiftArg, RequireObjectAlignment, MaxObjectSize>::
LogReuse(size_t size)
{TRACE_IT(22763);
    if (memoryData)
    {TRACE_IT(22764);
        memoryData->reuseCount++;
        memoryData->reuseBytes += size;
    }
}
#endif

void * InPlaceFreeListPolicy::New(ArenaAllocatorBase<InPlaceFreeListPolicy> * allocator)
{TRACE_IT(22765);
#if DBG
    // Allocate freeList followed by delayFreeList
    // A delayFreeList will enable us to detect use-after free scenarios in debug builds
    if (allocator->HasDelayFreeList())
    {TRACE_IT(22766);
        return AllocatorNewNoThrowNoRecoveryArrayZ(ArenaAllocator, allocator, FreeObject *, 2 * buckets);
    }
#endif
    return AllocatorNewNoThrowNoRecoveryArrayZ(ArenaAllocator, allocator, FreeObject *, buckets);
}

void * InPlaceFreeListPolicy::Allocate(void * policy, size_t size)
{TRACE_IT(22767);
    Assert(policy);

    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(policy);
    size_t index = (size >> ArenaAllocator::ObjectAlignmentBitShift) - 1;
    FreeObject * freeObject = freeObjectLists[index];

    if (NULL != freeObject)
    {TRACE_IT(22768);
        freeObjectLists[index] = freeObject->next;

#ifdef ARENA_MEMORY_VERIFY
#ifndef _MSC_VER
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wsizeof-pointer-memaccess"
#endif
        // Make sure the next pointer bytes are also DbgFreeMemFill-ed.
        memset(freeObject, DbgFreeMemFill, sizeof(freeObject->next));
#ifndef _MSC_VER
#pragma clang diagnostic pop
#endif
#endif
    }

    return freeObject;
}

void * InPlaceFreeListPolicy::Free(void * policy, void * object, size_t size)
{TRACE_IT(22769);
    Assert(policy);
    void * freeList = policy;
    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(freeList);
    FreeObject * freeObject = reinterpret_cast<FreeObject *>(object);
    size_t index = (size >> ArenaAllocator::ObjectAlignmentBitShift) - 1;

    freeObject->next = freeObjectLists[index];
    freeObjectLists[index] = freeObject;
    return policy;
}

void * InPlaceFreeListPolicy::Reset(void * policy)
{TRACE_IT(22770);
    return NULL;
}

#if DBG
void InPlaceFreeListPolicy::MergeDelayFreeList(void * freeList)
{TRACE_IT(22771);
    if (!freeList) return;

    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(freeList);
    FreeObject ** delayFreeObjectLists = freeObjectLists + buckets;

    for (int i = 0; i < buckets; i++)
    {TRACE_IT(22772);
        int size = (i + 1) << ArenaAllocator::ObjectAlignmentBitShift;
        FreeObject *delayObject = delayFreeObjectLists[i];
        FreeObject *lastDelayObject = nullptr;

        while (delayObject != nullptr)
        {TRACE_IT(22773);
            FreeObject *nextDelayObject = delayObject->next;
            // DebugPatternFill is required here, because we set isDeleted bit on freed Opnd
            PrepareFreeObject(delayObject, size);
            delayObject->next = nextDelayObject;
            lastDelayObject = delayObject;
            delayObject = nextDelayObject;
        }

        if (freeObjectLists[i] == nullptr)
        {TRACE_IT(22774);
            freeObjectLists[i] = delayFreeObjectLists[i];
            delayFreeObjectLists[i] = nullptr;
        }
        else if (lastDelayObject != nullptr) {TRACE_IT(22775);
            FreeObject * firstFreeObject = freeObjectLists[i];
            freeObjectLists[i] = delayFreeObjectLists[i];
            lastDelayObject->next = firstFreeObject;
            delayFreeObjectLists[i] = nullptr;
        }
    }
}
#endif

#ifdef ARENA_MEMORY_VERIFY
void InPlaceFreeListPolicy::VerifyFreeObjectIsFreeMemFilled(void * object, size_t size)
{TRACE_IT(22776);
    unsigned char * bytes = reinterpret_cast<unsigned char*>(object);
    for (size_t i = 0; i < size; i++)
    {TRACE_IT(22777);
        Assert(bytes[i] == InPlaceFreeListPolicy::DbgFreeMemFill);
    }
}
#endif

void * StandAloneFreeListPolicy::New(ArenaAllocatorBase<StandAloneFreeListPolicy> * /*allocator*/)
{TRACE_IT(22778);
    return NewInternal(InitialEntries);
}

void * StandAloneFreeListPolicy::Allocate(void * policy, size_t size)
{TRACE_IT(22779);
    Assert(policy);

    StandAloneFreeListPolicy * _this = reinterpret_cast<StandAloneFreeListPolicy *>(policy);
    size_t index = (size >> ArenaAllocator::ObjectAlignmentBitShift) - 1;
    void * object = NULL;

    uint * freeObjectList = &_this->freeObjectLists[index];
    if (0 != *freeObjectList)
    {TRACE_IT(22780);
        FreeObjectListEntry * entry = &_this->entries[*freeObjectList - 1];
        uint oldFreeList = _this->freeList;

        _this->freeList = *freeObjectList;
        *freeObjectList = entry->next;
        object = entry->object;
        Assert(object != NULL);
        entry->next = oldFreeList;
        entry->object = NULL;
    }

    return object;
}

void * StandAloneFreeListPolicy::Free(void * policy, void * object, size_t size)
{TRACE_IT(22781);
    Assert(policy);

    StandAloneFreeListPolicy * _this = reinterpret_cast<StandAloneFreeListPolicy *>(policy);
    size_t index = (size >> ArenaAllocator::ObjectAlignmentBitShift) - 1;

    if (TryEnsureFreeListEntry(_this))
    {TRACE_IT(22782);
        Assert(_this->freeList != 0);

        uint * freeObjectList = &_this->freeObjectLists[index];
        FreeObjectListEntry * entry = &_this->entries[_this->freeList - 1];
        uint oldFreeObjectList = *freeObjectList;

        *freeObjectList = _this->freeList;
        _this->freeList = entry->next;
        entry->object = object;
        entry->next = oldFreeObjectList;
    }

    return _this;
}

void * StandAloneFreeListPolicy::Reset(void * policy)
{TRACE_IT(22783);
    Assert(policy);

    StandAloneFreeListPolicy * _this = reinterpret_cast<StandAloneFreeListPolicy *>(policy);
    HeapDeletePlus(GetPlusSize(_this), _this);

    return NULL;
}

#if DBG
void StandAloneFreeListPolicy::MergeDelayFreeList(void * freeList)
{
    AssertMsg(0, "StandAlone Policy, mergelists not supported");
}
#endif

#ifdef ARENA_MEMORY_VERIFY
void StandAloneFreeListPolicy::VerifyFreeObjectIsFreeMemFilled(void * object, size_t size)
{TRACE_IT(22784);
    char * bytes = reinterpret_cast<char*>(object);
    for (size_t i = 0; i < size; i++)
    {TRACE_IT(22785);
        Assert(bytes[i] == StandAloneFreeListPolicy::DbgFreeMemFill);
    }
}
#endif

void StandAloneFreeListPolicy::Release(void * policy)
{TRACE_IT(22786);
    if (NULL != policy)
    {TRACE_IT(22787);
        Reset(policy);
    }
}

StandAloneFreeListPolicy * StandAloneFreeListPolicy::NewInternal(uint entries)
{TRACE_IT(22788);
    size_t plusSize = buckets * sizeof(uint) + entries * sizeof(FreeObjectListEntry);
    StandAloneFreeListPolicy * _this = HeapNewNoThrowPlusZ(plusSize, StandAloneFreeListPolicy);

    if (NULL != _this)
    {TRACE_IT(22789);
        _this->allocated = entries;
        _this->freeObjectLists = (uint *)(_this + 1);
        _this->entries = (FreeObjectListEntry *)(_this->freeObjectLists + buckets);
    }

    return _this;
}

bool StandAloneFreeListPolicy::TryEnsureFreeListEntry(StandAloneFreeListPolicy *& _this)
{TRACE_IT(22790);
    if (0 == _this->freeList)
    {TRACE_IT(22791);
        if (_this->used < _this->allocated)
        {TRACE_IT(22792);
            _this->used++;
            _this->freeList = _this->used;
        }
        else
        {TRACE_IT(22793);
            Assert(_this->used == _this->allocated);

            StandAloneFreeListPolicy * oldThis = _this;
            uint entries = oldThis->allocated + min(oldThis->allocated, MaxEntriesGrowth);
            StandAloneFreeListPolicy * newThis = NewInternal(entries);
            if (NULL != newThis)
            {TRACE_IT(22794);
                uint sizeInBytes = buckets * sizeof(uint);
                js_memcpy_s(newThis->freeObjectLists, sizeInBytes, oldThis->freeObjectLists, sizeInBytes);
                js_memcpy_s(newThis->entries, newThis->allocated * sizeof(FreeObjectListEntry), oldThis->entries, oldThis->used * sizeof(FreeObjectListEntry));
                newThis->used = oldThis->used + 1;
                newThis->freeList = newThis->used;
                _this = newThis;
                HeapDeletePlus(GetPlusSize(oldThis), oldThis);
            }
            else
            {TRACE_IT(22795);
                return false;
            }
        }
    }

    return true;
}

#ifdef PERSISTENT_INLINE_CACHES

void * InlineCacheFreeListPolicy::New(ArenaAllocatorBase<InlineCacheAllocatorTraits> * allocator)
{TRACE_IT(22796);
    return NewInternal();
}

InlineCacheFreeListPolicy * InlineCacheFreeListPolicy::NewInternal()
{TRACE_IT(22797);
    InlineCacheFreeListPolicy * _this = HeapNewNoThrowZ(InlineCacheFreeListPolicy);
    return _this;
}

InlineCacheFreeListPolicy::InlineCacheFreeListPolicy()
{TRACE_IT(22798);
    Assert(AreFreeListBucketsEmpty());
}

bool InlineCacheFreeListPolicy::AreFreeListBucketsEmpty()
{TRACE_IT(22799);
    for (int b = 0; b < bucketCount; b++)
    {TRACE_IT(22800);
        if (this->freeListBuckets[b] != 0) return false;
    }
    return true;
}

void * InlineCacheFreeListPolicy::Allocate(void * policy, size_t size)
{TRACE_IT(22801);
    Assert(policy);

    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(policy);
    size_t index = (size >> InlineCacheAllocatorInfo::ObjectAlignmentBitShift) - 1;
    FreeObject * freeObject = freeObjectLists[index];

    if (NULL != freeObject)
    {TRACE_IT(22802);
        freeObjectLists[index] = reinterpret_cast<FreeObject *>(reinterpret_cast<intptr_t>(freeObject->next) & ~InlineCacheFreeListTag);

#ifdef ARENA_MEMORY_VERIFY
        // Make sure the next pointer bytes are also DbgFreeMemFill-ed, before we give them out.
        memset(&freeObject->next, DbgFreeMemFill, sizeof(freeObject->next));
#endif
    }

    return freeObject;
}

void * InlineCacheFreeListPolicy::Free(void * policy, void * object, size_t size)
{TRACE_IT(22803);
    Assert(policy);

    FreeObject ** freeObjectLists = reinterpret_cast<FreeObject **>(policy);
    FreeObject * freeObject = reinterpret_cast<FreeObject *>(object);
    size_t index = (size >> InlineCacheAllocatorInfo::ObjectAlignmentBitShift) - 1;

    freeObject->next = reinterpret_cast<FreeObject *>(reinterpret_cast<intptr_t>(freeObjectLists[index]) | InlineCacheFreeListTag);
    freeObjectLists[index] = freeObject;
    return policy;
}

void * InlineCacheFreeListPolicy::Reset(void * policy)
{TRACE_IT(22804);
    Assert(policy);

    InlineCacheFreeListPolicy * _this = reinterpret_cast<InlineCacheFreeListPolicy *>(policy);
    HeapDelete(_this);

    return NULL;
}

#if DBG
void InlineCacheFreeListPolicy::MergeDelayFreeList(void * freeList)
{
    AssertMsg(0, "Inline policy, merge lists not supported");
}
#endif

#ifdef ARENA_MEMORY_VERIFY
void InlineCacheFreeListPolicy::VerifyFreeObjectIsFreeMemFilled(void * object, size_t size)
{TRACE_IT(22805);
    unsigned char * bytes = reinterpret_cast<unsigned char*>(object);
    for (size_t i = 0; i < size; i++)
    {TRACE_IT(22806);
        // We must allow for zero-filled free listed objects (at least their weakRefs/blankSlots bytes), because during garbage collection, we may zero out
        // some of the weakRefs (those that have become unreachable), and this is NOT a sign of "use after free" problem.  It would be nice if during collection
        // we could reliably distinguish free-listed objects from live caches, but that's not possible because caches can be allocated and freed in batches
        // (see more on that in comments inside InlineCacheFreeListPolicy::PrepareFreeObject).
        Assert(bytes[i] == NULL || bytes[i] == InlineCacheFreeListPolicy::DbgFreeMemFill);
    }
}
#endif

void InlineCacheFreeListPolicy::Release(void * policy)
{TRACE_IT(22807);
    if (NULL != policy)
    {TRACE_IT(22808);
        Reset(policy);
    }
}

#if DBG
bool InlineCacheAllocator::IsAllZero()
{TRACE_IT(22809);
    UpdateCacheBlock();

    // See InlineCacheAllocator::ZeroAll for why we ignore the strongRef slot of the CacheLayout.

    BigBlock *bigBlock = this->bigBlocks;
    while (bigBlock != NULL)
    {TRACE_IT(22810);
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {TRACE_IT(22811);
            unsigned char* weakRefBytes = (unsigned char *)cache->weakRefs;
            for (size_t i = 0; i < sizeof(cache->weakRefs); i++)
            {TRACE_IT(22812);
                // If we're verifying arena memory (in debug builds) caches on the free list
                // will be debug pattern filled (specifically, at least their weak reference slots).
                // All other caches must be zeroed out (again, at least their weak reference slots).
#ifdef ARENA_MEMORY_VERIFY
                if (weakRefBytes[i] != NULL && weakRefBytes[i] != InlineCacheFreeListPolicy::DbgFreeMemFill)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#else
                if (weakRefBytes[i] != NULL)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#endif
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    bigBlock = this->fullBlocks;
    while (bigBlock != NULL)
    {TRACE_IT(22813);
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {TRACE_IT(22814);
            char* weakRefBytes = (char *)cache->weakRefs;
            for (size_t i = 0; i < sizeof(cache->weakRefs); i++)
            {TRACE_IT(22815);
                // If we're verifying arena memory (in debug builds) caches on the free list
                // will be debug pattern filled (specifically, their weak reference slots).
                // All other caches must be zeroed out (again, their weak reference slots).
#ifdef ARENA_MEMORY_VERIFY
                if (weakRefBytes[i] != NULL && weakRefBytes[i] != InlineCacheFreeListPolicy::DbgFreeMemFill)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#else
                if (weakRefBytes[i] != NULL)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#endif
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22816);
        Assert(memoryBlock->nbytes % sizeof(CacheLayout) == 0);
        ArenaMemoryBlock * next = memoryBlock->next;
        CacheLayout* endPtr = (CacheLayout*)(memoryBlock->GetBytes() + memoryBlock->nbytes);
        for (CacheLayout* cache = (CacheLayout*)memoryBlock->GetBytes(); cache < endPtr; cache++)
        {TRACE_IT(22817);
            unsigned char* weakRefBytes = (unsigned char *)cache->weakRefs;
            for (size_t i = 0; i < sizeof(cache->weakRefs); i++)
            {TRACE_IT(22818);
#ifdef ARENA_MEMORY_VERIFY
                if (weakRefBytes[i] != NULL && weakRefBytes[i] != InlineCacheFreeListPolicy::DbgFreeMemFill)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#else
                if (weakRefBytes[i] != NULL)
                {
                    AssertMsg(false, "Inline cache arena is not zeroed!");
                    return false;
                }
#endif
            }
        }
        memoryBlock = next;
    }

    return true;
}
#endif

void InlineCacheAllocator::ZeroAll()
{TRACE_IT(22819);
    UpdateCacheBlock();

    // We zero the weakRefs part of each cache in the arena unconditionally.  The strongRef slot is zeroed only
    // if it isn't tagged with InlineCacheFreeListTag.  That's so we don't lose our free list, which is
    // formed by caches linked via their strongRef slot tagged with InlineCacheFreeListTag.  On the other hand,
    // inline caches that require invalidation use the same slot as a pointer (untagged) to the cache's address
    // in the invalidation list.  Hence, we must zero the strongRef slot when untagged to ensure the cache
    // doesn't appear registered for invalidation when it's actually blank (which would trigger asserts in InlineCache::VerifyRegistrationForInvalidation).

    BigBlock *bigBlock = this->bigBlocks;
    while (bigBlock != NULL)
    {TRACE_IT(22820);
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {TRACE_IT(22821);
            memset(cache->weakRefs, 0, sizeof(cache->weakRefs));
            // We want to preserve the free list, whose next pointers are tagged with InlineCacheFreeListTag.
            if ((cache->strongRef & InlineCacheFreeListTag) == 0) cache->strongRef = 0;

            if (cache->weakRefs[0] != NULL || cache->weakRefs[1] != NULL || cache->weakRefs[2] != NULL)
            {
                AssertMsg(false, "Inline cache arena is not zeroed!");
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    bigBlock = this->fullBlocks;
    while (bigBlock != NULL)
    {TRACE_IT(22822);
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {TRACE_IT(22823);
            memset(cache->weakRefs, 0, sizeof(cache->weakRefs));
            // We want to preserve the free list, whose next pointers are tagged with InlineCacheFreeListTag.
            if ((cache->strongRef & InlineCacheFreeListTag) == 0) cache->strongRef = 0;

            if (cache->weakRefs[0] != NULL || cache->weakRefs[1] != NULL || cache->weakRefs[2] != NULL)
            {
                AssertMsg(false, "Inline cache arena is not zeroed!");
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22824);
        Assert(memoryBlock->nbytes % sizeof(CacheLayout) == 0);
        ArenaMemoryBlock * next = memoryBlock->next;
        CacheLayout* endPtr = (CacheLayout*)(memoryBlock->GetBytes() + memoryBlock->nbytes);
        for (CacheLayout* cache = (CacheLayout*)memoryBlock->GetBytes(); cache < endPtr; cache++)
        {TRACE_IT(22825);
            memset(cache->weakRefs, 0, sizeof(cache->weakRefs));
            // We want to preserve the free list, whose next pointers are tagged with InlineCacheFreeListTag.
            if ((cache->strongRef & InlineCacheFreeListTag) == 0) cache->strongRef = 0;

            if (cache->weakRefs[0] != NULL || cache->weakRefs[1] != NULL || cache->weakRefs[2] != NULL)
            {
                AssertMsg(false, "Inline cache arena is not zeroed!");
            }
        }
        memoryBlock = next;
    }
}

bool InlineCacheAllocator::IsDeadWeakRef(Recycler* recycler, void* ptr)
{TRACE_IT(22826);
    return recycler->IsObjectMarked(ptr);
}

bool InlineCacheAllocator::CacheHasDeadWeakRefs(Recycler* recycler, CacheLayout* cache)
{TRACE_IT(22827);
    for (intptr_t* curWeakRefPtr = cache->weakRefs; curWeakRefPtr < &cache->strongRef; curWeakRefPtr++)
    {TRACE_IT(22828);
        intptr_t curWeakRef = *curWeakRefPtr;

        if (curWeakRef == 0)
        {TRACE_IT(22829);
            continue;
        }

        curWeakRef &= ~(intptr_t)InlineCacheAuxSlotTypeTag;

        if ((curWeakRef & (HeapConstants::ObjectGranularity - 1)) != 0)
        {TRACE_IT(22830);
            continue;
        }


        if (!recycler->IsObjectMarked((void*)curWeakRef))
        {TRACE_IT(22831);
            return true;
        }
    }

    return false;
}

bool InlineCacheAllocator::HasNoDeadWeakRefs(Recycler* recycler)
{TRACE_IT(22832);
    UpdateCacheBlock();

    BigBlock *bigBlock = this->bigBlocks;
    while (bigBlock != NULL)
    {TRACE_IT(22833);
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            if (CacheHasDeadWeakRefs(recycler, cache))
            {TRACE_IT(22834);
                return false;
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }
    bigBlock = this->fullBlocks;
    while (bigBlock != NULL)
    {TRACE_IT(22835);
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            if (CacheHasDeadWeakRefs(recycler, cache))
            {TRACE_IT(22836);
                return false;
            }
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22837);
        Assert(memoryBlock->nbytes % sizeof(CacheLayout) == 0);
        ArenaMemoryBlock * next = memoryBlock->next;
        CacheLayout* endPtr = (CacheLayout*)(memoryBlock->GetBytes() + memoryBlock->nbytes);
        for (CacheLayout* cache = (CacheLayout*)memoryBlock->GetBytes(); cache < endPtr; cache++)
        {
            if (CacheHasDeadWeakRefs(recycler, cache))
            {TRACE_IT(22838);
                return false;
            }
        }
        memoryBlock = next;
    }

    return true;
}

void InlineCacheAllocator::ClearCacheIfHasDeadWeakRefs(Recycler* recycler, CacheLayout* cache)
{TRACE_IT(22839);
    for (intptr_t* curWeakRefPtr = cache->weakRefs; curWeakRefPtr < &cache->strongRef; curWeakRefPtr++)
    {TRACE_IT(22840);
        intptr_t curWeakRef = *curWeakRefPtr;

        if (curWeakRef == 0)
        {TRACE_IT(22841);
            continue;
        }

        curWeakRef &= ~(intptr_t)InlineCacheAuxSlotTypeTag;

        if ((curWeakRef & (HeapConstants::ObjectGranularity - 1)) != 0)
        {TRACE_IT(22842);
            continue;
        }

        if (!recycler->IsObjectMarked((void*)curWeakRef))
        {TRACE_IT(22843);
            cache->weakRefs[0] = 0;
            cache->weakRefs[1] = 0;
            cache->weakRefs[2] = 0;
            break;
        }
    }
}

void InlineCacheAllocator::ClearCachesWithDeadWeakRefs(Recycler* recycler)
{TRACE_IT(22844);
    UpdateCacheBlock();

    BigBlock *bigBlock = this->bigBlocks;
    while (bigBlock != NULL)
    {TRACE_IT(22845);
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            ClearCacheIfHasDeadWeakRefs(recycler, cache);
        }
        bigBlock = bigBlock->nextBigBlock;
    }
    bigBlock = this->fullBlocks;
    while (bigBlock != NULL)
    {TRACE_IT(22846);
        Assert(bigBlock->currentByte % sizeof(CacheLayout) == 0);
        CacheLayout* endPtr = (CacheLayout*)(bigBlock->GetBytes() + bigBlock->currentByte);
        for (CacheLayout* cache = (CacheLayout*)bigBlock->GetBytes(); cache < endPtr; cache++)
        {
            ClearCacheIfHasDeadWeakRefs(recycler, cache);
        }
        bigBlock = bigBlock->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22847);
        Assert(memoryBlock->nbytes % sizeof(CacheLayout) == 0);
        ArenaMemoryBlock * next = memoryBlock->next;
        CacheLayout* endPtr = (CacheLayout*)(memoryBlock->GetBytes() + memoryBlock->nbytes);
        for (CacheLayout* cache = (CacheLayout*)memoryBlock->GetBytes(); cache < endPtr; cache++)
        {
            ClearCacheIfHasDeadWeakRefs(recycler, cache);
        }
        memoryBlock = next;
    }
}

#else

#if DBG
bool InlineCacheAllocator::IsAllZero()
{TRACE_IT(22848);
    UpdateCacheBlock();
    BigBlock *blockp = this->bigBlocks;
    while (blockp != NULL)
    {TRACE_IT(22849);
        for (size_t i = 0; i < blockp->currentByte; i++)
        {TRACE_IT(22850);
            if (blockp->GetBytes()[i] != 0)
            {TRACE_IT(22851);
                return false;
            }
        }

        blockp = blockp->nextBigBlock;
    }
    blockp = this->fullBlocks;
    while (blockp != NULL)
    {TRACE_IT(22852);
        for (size_t i = 0; i < blockp->currentByte; i++)
        {TRACE_IT(22853);
            if (blockp->GetBytes()[i] != 0)
            {TRACE_IT(22854);
                return false;
            }
        }
        blockp = blockp->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22855);
        ArenaMemoryBlock * next = memoryBlock->next;
        for (size_t i = 0; i < memoryBlock->nbytes; i++)
        {TRACE_IT(22856);
            if (memoryBlock->GetBytes()[i] != 0)
            {TRACE_IT(22857);
                return false;
            }
        }
        memoryBlock = next;
    }
    return true;
}
#endif

void InlineCacheAllocator::ZeroAll()
{TRACE_IT(22858);
    UpdateCacheBlock();
    BigBlock *blockp = this->bigBlocks;
    while (blockp != NULL)
    {TRACE_IT(22859);
        memset(blockp->GetBytes(), 0, blockp->currentByte);
        blockp = blockp->nextBigBlock;
    }
    blockp = this->fullBlocks;
    while (blockp != NULL)
    {TRACE_IT(22860);
        memset(blockp->GetBytes(), 0, blockp->currentByte);
        blockp = blockp->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22861);
        ArenaMemoryBlock * next = memoryBlock->next;
        memset(memoryBlock->GetBytes(), 0, memoryBlock->nbytes);
        memoryBlock = next;
    }
}

#endif

#if DBG
bool CacheAllocator::IsAllZero()
{TRACE_IT(22862);
    UpdateCacheBlock();
    BigBlock *blockp = this->bigBlocks;
    while (blockp != NULL)
    {TRACE_IT(22863);
        for (size_t i = 0; i < blockp->currentByte; i++)
        {TRACE_IT(22864);
            if (blockp->GetBytes()[i] != 0)
            {TRACE_IT(22865);
                return false;
            }
        }

        blockp = blockp->nextBigBlock;
    }
    blockp = this->fullBlocks;
    while (blockp != NULL)
    {TRACE_IT(22866);
        for (size_t i = 0; i < blockp->currentByte; i++)
        {TRACE_IT(22867);
            if (blockp->GetBytes()[i] != 0)
            {TRACE_IT(22868);
                return false;
            }
        }
        blockp = blockp->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22869);
        ArenaMemoryBlock * next = memoryBlock->next;
        for (size_t i = 0; i < memoryBlock->nbytes; i++)
        {TRACE_IT(22870);
            if (memoryBlock->GetBytes()[i] != 0)
            {TRACE_IT(22871);
                return false;
            }
        }
        memoryBlock = next;
    }
    return true;
}
#endif

void CacheAllocator::ZeroAll()
{TRACE_IT(22872);
    UpdateCacheBlock();
    BigBlock *blockp = this->bigBlocks;
    while (blockp != NULL)
    {TRACE_IT(22873);
        memset(blockp->GetBytes(), 0, blockp->currentByte);
        blockp = blockp->nextBigBlock;
    }
    blockp = this->fullBlocks;
    while (blockp != NULL)
    {TRACE_IT(22874);
        memset(blockp->GetBytes(), 0, blockp->currentByte);
        blockp = blockp->nextBigBlock;
    }

    ArenaMemoryBlock * memoryBlock = this->mallocBlocks;
    while (memoryBlock != nullptr)
    {TRACE_IT(22875);
        ArenaMemoryBlock * next = memoryBlock->next;
        memset(memoryBlock->GetBytes(), 0, memoryBlock->nbytes);
        memoryBlock = next;
    }
}

#undef ASSERT_THREAD

namespace Memory
{
    template class ArenaAllocatorBase<InPlaceFreeListPolicy>;
    template class ArenaAllocatorBase<StandAloneFreeListPolicy>;
    template class ArenaAllocatorBase<InlineCacheAllocatorTraits>;
}
