//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

//=====================================================================================================
// Initialization
//=====================================================================================================
LargeHeapBucket::~LargeHeapBucket()
{TRACE_IT(24544);
    Recycler* recycler = this->heapInfo->recycler;
    HeapInfo* autoHeap = this->heapInfo;

    ForEachEditingLargeHeapBlock([recycler, autoHeap](LargeHeapBlock * heapBlock)
    {
        heapBlock->ReleasePagesShutdown(recycler);
        LargeHeapBlock::Delete(heapBlock);
        RECYCLER_SLOW_CHECK(autoHeap->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]--);
    });
}

void
LargeHeapBucket::Initialize(HeapInfo * heapInfo, uint sizeCat, bool supportFreeList)
{TRACE_IT(24545);
    this->heapInfo = heapInfo;
    this->sizeCat = sizeCat;
#ifdef RECYCLER_PAGE_HEAP
    this->isPageHeapEnabled = heapInfo->IsPageHeapEnabledForBlock<LargeAllocationBlockAttributes>(sizeCat);
#endif
    this->supportFreeList = supportFreeList;
}

//=====================================================================================================
// Allocation
//=====================================================================================================
char *
LargeHeapBucket::TryAllocFromNewHeapBlock(Recycler * recycler, size_t sizeCat, size_t size, ObjectInfoBits attributes, bool nothrow)
{TRACE_IT(24546);
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

#ifdef RECYCLER_PAGE_HEAP
    if (IsPageHeapEnabled(attributes))
    {TRACE_IT(24547);
        return this->PageHeapAlloc(recycler, sizeCat, size, attributes, this->heapInfo->pageHeapMode, true);
    }
#endif

    LargeHeapBlock * heapBlock = AddLargeHeapBlock(sizeCat, nothrow);
    if (heapBlock == nullptr)
    {TRACE_IT(24548);
        return nullptr;
    }
    char * memBlock = heapBlock->Alloc(sizeCat, attributes);
    Assert(memBlock != nullptr);
    return memBlock;
}

char *
LargeHeapBucket::SnailAlloc(Recycler * recycler, size_t sizeCat, size_t size, ObjectInfoBits attributes, bool nothrow)
{TRACE_IT(24549);
    char * memBlock;

    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    // No free memory, try to collect with allocated bytes and time heuristic, and concurrently
#if ENABLE_CONCURRENT_GC
    BOOL collected = recycler->disableCollectOnAllocationHeuristics ? recycler->FinishConcurrent<FinishConcurrentOnAllocation>() :
        recycler->CollectNow<CollectOnAllocation>();
#else
    BOOL collected = recycler->disableCollectOnAllocationHeuristics ? FALSE : recycler->CollectNow<CollectOnAllocation>();
#endif

    if (!collected)
    {TRACE_IT(24550);
        memBlock = TryAllocFromNewHeapBlock(recycler, sizeCat, size, attributes, nothrow);
        if (memBlock != nullptr)
        {TRACE_IT(24551);
            return memBlock;
        }
        // Can't even allocate a new block, we need force a collection and
        // allocate some free memory, add a new heap block again, or throw out of memory
        AllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), _u("LargeHeapBucket::AddLargeHeapBlock failed, forcing in-thread collection\n"));
        recycler->CollectNow<CollectNowForceInThread>();
    }

    memBlock = TryAlloc(recycler, sizeCat, attributes);
    if (memBlock != nullptr)
    {TRACE_IT(24552);
        return memBlock;
    }

    memBlock = TryAllocFromNewHeapBlock(recycler, sizeCat, size, attributes, nothrow);
    if (memBlock != nullptr)
    {TRACE_IT(24553);
        return memBlock;
    }

    if (nothrow == false)
    {TRACE_IT(24554);
        // Can't add a heap block, we are out of memory
        // Since nothrow is false, we can throw right here
        recycler->OutOfMemory();
    }

    return nullptr;
}

#ifdef RECYCLER_PAGE_HEAP
char*
LargeHeapBucket::PageHeapAlloc(Recycler * recycler, size_t sizeCat, size_t size, ObjectInfoBits attributes, PageHeapMode mode, bool nothrow)
{TRACE_IT(24555);
    Segment * segment;
    size_t pageCount = LargeHeapBlock::GetPagesNeeded(size, false);
    if (pageCount == 0)
    {TRACE_IT(24556);
        if (nothrow == false)
        {TRACE_IT(24557);
            // overflow
            // Since nothrow is false here, it's okay to throw
            recycler->OutOfMemory();
        }

        return nullptr;
    }

    if(size<sizeof(void*))
    {TRACE_IT(24558);
        attributes = (ObjectInfoBits)(attributes | LeafBit);
    }


    size_t actualPageCount = pageCount + 1; // 1 for guard page
    auto pageAllocator = recycler->GetRecyclerLargeBlockPageAllocator();
    char * baseAddress = pageAllocator->Alloc(&actualPageCount, &segment);
    if (baseAddress == nullptr)
    {TRACE_IT(24559);
        return nullptr;
    }

    size_t guardPageCount = actualPageCount - pageCount; // pageAllocator can return more than asked pages

    char* address = nullptr;
    char* guardPageAddress = nullptr;

    if (heapInfo->pageHeapMode == PageHeapMode::PageHeapModeBlockStart)
    {TRACE_IT(24560);
        address = baseAddress + AutoSystemInfo::PageSize * guardPageCount;
        guardPageAddress = baseAddress;
    }
    else if (heapInfo->pageHeapMode == PageHeapMode::PageHeapModeBlockEnd)
    {TRACE_IT(24561);
        address = baseAddress;
        guardPageAddress = baseAddress + pageCount * AutoSystemInfo::PageSize;
    }
    else
    {TRACE_IT(24562);
        AnalysisAssert(false);
    }



    LargeHeapBlock * heapBlock = LargeHeapBlock::New(address, pageCount, segment, 1, this);
    if (!heapBlock)
    {TRACE_IT(24563);
        pageAllocator->SuspendIdleDecommit();
        pageAllocator->Release(baseAddress, actualPageCount, segment);
        pageAllocator->ResumeIdleDecommit();
        return nullptr;
    }

    heapBlock->heapInfo = this->heapInfo;
    heapBlock->actualPageCount = actualPageCount;
    heapBlock->guardPageAddress = guardPageAddress;
    
    DWORD oldProtect;
    BOOL ret = ::VirtualProtect(guardPageAddress, AutoSystemInfo::PageSize * guardPageCount, PAGE_NOACCESS, &oldProtect);
    Assert(ret && oldProtect == PAGE_READWRITE);
    
    // fill pattern before set pageHeapMode, so background scan stack may verify the pattern
    size_t usedSpace = sizeof(LargeObjectHeader) + size;
    memset(address + usedSpace, 0xF0, pageCount * AutoSystemInfo::PageSize - usedSpace);
    heapBlock->pageHeapMode = heapInfo->pageHeapMode;

    if (!recycler->heapBlockMap.SetHeapBlock(address, pageCount, heapBlock, HeapBlock::HeapBlockType::LargeBlockType, 0))
    {TRACE_IT(24564);
        pageAllocator->SuspendIdleDecommit();
        heapBlock->ReleasePages(recycler);
        pageAllocator->ResumeIdleDecommit();
        LargeHeapBlock::Delete(heapBlock);
        return nullptr;
    }

    heapBlock->ResetMarks(ResetMarkFlags_None, recycler);

    char * memBlock = heapBlock->Alloc(size, attributes);
    Assert(memBlock != nullptr);

    heapBlock->SetNextBlock(this->largePageHeapBlockList);
    this->largePageHeapBlockList = heapBlock;

#if ENABLE_PARTIAL_GC
    recycler->autoHeap.uncollectedNewPageCount += pageCount;
#endif

    RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]++);
    RECYCLER_PERF_COUNTER_ADD(FreeObjectSize, heapBlock->GetPageCount() * AutoSystemInfo::PageSize);


    if (recycler->ShouldCapturePageHeapAllocStack())
    {TRACE_IT(24565);
#ifdef STACK_BACK_TRACE
        heapBlock->CapturePageHeapAllocStack();
#endif
    }

    return memBlock;
}
#endif

LargeHeapBlock*
LargeHeapBucket::AddLargeHeapBlock(size_t size, bool nothrow)
{TRACE_IT(24566);
    Recycler* recycler = this->heapInfo->recycler;
    Segment * segment;
    size_t pageCount = LargeHeapBlock::GetPagesNeeded(size, this->supportFreeList);
    if (pageCount == 0)
    {TRACE_IT(24567);
        if (nothrow == false)
        {TRACE_IT(24568);
            // overflow
            // Since nothrow is false here, it's okay to throw
            recycler->OutOfMemory();
        }

        return nullptr;
    }

    char * address = nullptr;

    size_t realPageCount = pageCount;
    address = recycler->GetRecyclerLargeBlockPageAllocator()->Alloc(&realPageCount, &segment);
    pageCount = realPageCount;

    if (address == nullptr)
    {TRACE_IT(24569);
        return nullptr;
    }
#ifdef RECYCLER_ZERO_MEM_CHECK
    recycler->VerifyZeroFill(address, pageCount * AutoSystemInfo::PageSize);
#endif
    uint objectCount = LargeHeapBlock::GetMaxLargeObjectCount(pageCount, size);
    LargeHeapBlock * heapBlock = LargeHeapBlock::New(address, pageCount, segment, objectCount, this);
#if DBG
    LargeAllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), _u("Allocated new large heap block 0x%p for sizeCat 0x%x\n"), heapBlock, sizeCat);
#endif

#ifdef ENABLE_JS_ETW
#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (segment->GetPageCount() > recycler->GetRecyclerLargeBlockPageAllocator()->GetMaxAllocPageCount())
    {TRACE_IT(24570);
        JS_ETW_INTERNAL(EventWriteJSCRIPT_INTERNAL_RECYCLER_EXTRALARGE_OBJECT_ALLOC(size));
    }
#endif
#endif
    if (!heapBlock)
    {TRACE_IT(24571);
        recycler->GetRecyclerLargeBlockPageAllocator()->SuspendIdleDecommit();
        recycler->GetRecyclerLargeBlockPageAllocator()->Release(address, pageCount, segment);
        recycler->GetRecyclerLargeBlockPageAllocator()->ResumeIdleDecommit();
        return nullptr;
    }
#if ENABLE_PARTIAL_GC
    recycler->autoHeap.uncollectedNewPageCount += pageCount;
#endif

    RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]++);

    heapBlock->heapInfo = this->heapInfo;

    heapBlock->lastCollectAllocCount = 0;

    Assert(recycler->collectionState != CollectionStateMark);

    if (!recycler->heapBlockMap.SetHeapBlock(address, pageCount, heapBlock, HeapBlock::HeapBlockType::LargeBlockType, 0))
    {TRACE_IT(24572);
        recycler->GetRecyclerLargeBlockPageAllocator()->SuspendIdleDecommit();
        heapBlock->ReleasePages(recycler);
        recycler->GetRecyclerLargeBlockPageAllocator()->ResumeIdleDecommit();
        LargeHeapBlock::Delete(heapBlock);
        RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]--);
        return nullptr;
    }

    heapBlock->SetNextBlock(this->largeBlockList);
    this->largeBlockList = heapBlock;

    RECYCLER_PERF_COUNTER_ADD(FreeObjectSize, heapBlock->GetPageCount() * AutoSystemInfo::PageSize);
    return heapBlock;
}

char *
LargeHeapBucket::TryAllocFromFreeList(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes)
{TRACE_IT(24573);
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    LargeHeapBlockFreeList* freeListEntry = this->freeList;

    // Walk through the free list, find the first entry that can fit our desired size
    while (freeListEntry)
    {TRACE_IT(24574);
        LargeHeapBlock* heapBlock = freeListEntry->heapBlock;

        char * memBlock = heapBlock->TryAllocFromFreeList(sizeCat, attributes);
        if (memBlock)
        {TRACE_IT(24575);
            // Don't need to verify zero fill here since we will do it in LargeHeapBucket::Alloc
            return memBlock;
        }
        else
        {TRACE_IT(24576);
#if DBG
            LargeAllocationVerboseTrace(recycler->GetRecyclerFlagsTable(), _u("Unable to allocate object of size 0x%x from freelist\n"), sizeCat);
#endif
        }

        freeListEntry = freeListEntry->next;
    }
    return nullptr;
}

char *
LargeHeapBucket::TryAllocFromExplicitFreeList(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes)
{TRACE_IT(24577);
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    FreeObject * currFreeObject = this->explicitFreeList;
    FreeObject * prevFreeObject = nullptr;
    while (currFreeObject != nullptr)
    {TRACE_IT(24578);
        char * memBlock = (char *)currFreeObject;
        LargeObjectHeader * header = LargeHeapBlock::GetHeaderFromAddress(memBlock);
        Assert(header->isExplicitFreed);
        Assert(HeapInfo::GetMediumObjectAlignedSizeNoCheck(header->objectSize) == this->sizeCat);
        if (header->objectSize < sizeCat)
        {TRACE_IT(24579);
            prevFreeObject = currFreeObject;
            currFreeObject = currFreeObject->GetNext();
            continue;
        }

        DebugOnly(header->isExplicitFreed = false);
        if (prevFreeObject)
        {TRACE_IT(24580);
            prevFreeObject->SetNext(currFreeObject->GetNext());
        }
        else
        {TRACE_IT(24581);
            this->explicitFreeList = currFreeObject->GetNext();
        }

#ifdef RECYCLER_MEMORY_VERIFY
        HeapBlock* heapBlockVerify = recycler->FindHeapBlock(memBlock);
        Assert(heapBlockVerify != nullptr);
        Assert(heapBlockVerify->IsLargeHeapBlock());
        LargeHeapBlock * largeHeapBlock = (LargeHeapBlock *)heapBlockVerify;
        LargeObjectHeader * dbgHeader;
        Assert(largeHeapBlock->GetObjectHeader(memBlock, &dbgHeader));
        Assert(dbgHeader == header);

        ((FreeObject *)memBlock)->DebugFillNext();
#endif
#ifdef RECYCLER_ZERO_MEM_CHECK
        // TODO: large heap block doesn't separate leaf object on to different page allocator.
        // so all the memory should still be zeroed.
        memset(memBlock, 0, sizeof(FreeObject));
#endif
        header->SetAttributes(recycler->Cookie, (attributes & StoredObjectInfoBitMask));

        if ((attributes & ObjectInfoBits::FinalizeBit) != 0)
        {TRACE_IT(24582);
            LargeHeapBlock* heapBlock = (LargeHeapBlock *)recycler->FindHeapBlock(memBlock);
            heapBlock->finalizeCount++;
#ifdef RECYCLER_FINALIZE_CHECK
            heapInfo->liveFinalizableObjectCount++;
            heapInfo->newFinalizableObjectCount++;
#endif
        }
        return memBlock;
    }

    return nullptr;
}
//=====================================================================================================
// Free
//=====================================================================================================
void
LargeHeapBucket::ExplicitFree(void * object, size_t sizeCat)
{TRACE_IT(24583);
    Assert(HeapInfo::GetMediumObjectAlignedSizeNoCheck(sizeCat) == this->sizeCat);
    LargeObjectHeader * header = LargeHeapBlock::GetHeaderFromAddress(object);
    Assert(header->GetAttributes(this->heapInfo->recycler->Cookie) == ObjectInfoBits::NoBit || header->GetAttributes(this->heapInfo->recycler->Cookie) == ObjectInfoBits::LeafBit);
    Assert(!header->isExplicitFreed);
    DebugOnly(header->isExplicitFreed = true);
    Assert(header->objectSize >= sizeCat);

#if DBG
    HeapBlock* heapBlock = this->GetRecycler()->FindHeapBlock(object);
    Assert(heapBlock != nullptr);
    Assert(heapBlock->IsLargeHeapBlock());

    LargeHeapBlock * largeHeapBlock = (LargeHeapBlock *)heapBlock;
    LargeObjectHeader * dbgHeader;
    Assert(largeHeapBlock->GetObjectHeader(object, &dbgHeader));
    Assert(dbgHeader == header);
#endif

    FreeObject * freeObject = (FreeObject *)object;
    freeObject->SetNext(this->explicitFreeList);
    this->explicitFreeList = freeObject;
    header->SetAttributes(this->heapInfo->recycler->Cookie, ObjectInfoBits::LeafBit);       // We can stop scanning it now.


}

//=====================================================================================================
// Collections
//=====================================================================================================
void
LargeHeapBucket::ResetMarks(ResetMarkFlags flags)
{TRACE_IT(24584);
    Recycler* recycler = this->heapInfo->recycler;

    HeapBlockList::ForEach(largeBlockList, [flags, recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ResetMarks(flags, recycler);
    });
#ifdef RECYCLER_PAGE_HEAP
    HeapBlockList::ForEach(largePageHeapBlockList, [flags, recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ResetMarks(flags, recycler);
    });
#endif
    HeapBlockList::ForEach(fullLargeBlockList, [flags, recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ResetMarks(flags, recycler);
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [flags, recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ResetMarks(flags, recycler);
    });
#if ENABLE_CONCURRENT_GC
    Assert(pendingSweepLargeBlockList == nullptr);
#endif
}

void
LargeHeapBucket::ScanInitialImplicitRoots(Recycler * recycler)
{TRACE_IT(24585);
    HeapBlockList::ForEach(largeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });
#ifdef RECYCLER_PAGE_HEAP
    HeapBlockList::ForEach(largePageHeapBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });
#endif
    HeapBlockList::ForEach(fullLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanInitialImplicitRoots(recycler);
    });
#if ENABLE_CONCURRENT_GC
    Assert(pendingSweepLargeBlockList == nullptr);
#endif
}

void
LargeHeapBucket::ScanNewImplicitRoots(Recycler * recycler)
{TRACE_IT(24586);
    HeapBlockList::ForEach(largeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanNewImplicitRoots(recycler);
    });
#ifdef RECYCLER_PAGE_HEAP
    HeapBlockList::ForEach(largePageHeapBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanNewImplicitRoots(recycler);
    });
#endif
    HeapBlockList::ForEach(fullLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanNewImplicitRoots(recycler);
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->ScanNewImplicitRoots(recycler);
    });
#if ENABLE_CONCURRENT_GC
    Assert(pendingSweepLargeBlockList == nullptr);
#endif
}

//=====================================================================================================
// Sweep
//=====================================================================================================
#pragma region Sweep

void
LargeHeapBucket::Sweep(RecyclerSweep& recyclerSweep)
{TRACE_IT(24587);
#if ENABLE_CONCURRENT_GC
    // CONCURRENT-TODO: large buckets are not swept in the background currently.
    Assert(!recyclerSweep.GetRecycler()->IsConcurrentExecutingState());
#endif

    LargeHeapBlock * currentLargeObjectBlocks = largeBlockList;
#ifdef RECYCLER_PAGE_HEAP
    LargeHeapBlock * currentLargePageHeapObjectBlocks = largePageHeapBlockList;
#endif
    LargeHeapBlock * currentFullLargeObjectBlocks = fullLargeBlockList;
    LargeHeapBlock * currentDisposeLargeBlockList = pendingDisposeLargeBlockList;
    this->largeBlockList = nullptr;
#ifdef RECYCLER_PAGE_HEAP
    this->largePageHeapBlockList = nullptr;
#endif
    this->fullLargeBlockList = nullptr;

    // Clear the free list before sweep
    // We'll reconstruct the free list during sweep
    if (this->supportFreeList)
    {TRACE_IT(24588);
#if DBG
        LargeAllocationVerboseTrace(recyclerSweep.GetRecycler()->GetRecyclerFlagsTable(), _u("Resetting free list for 0x%x bucket\n"), this->sizeCat);
#endif
        this->freeList = nullptr;
        this->explicitFreeList = nullptr;
    }

#if ENABLE_CONCURRENT_GC
    Assert(this->pendingSweepLargeBlockList == nullptr);
#endif
    SweepLargeHeapBlockList(recyclerSweep, currentLargeObjectBlocks);
#ifdef RECYCLER_PAGE_HEAP
    SweepLargeHeapBlockList(recyclerSweep, currentLargePageHeapObjectBlocks);
#endif
    SweepLargeHeapBlockList(recyclerSweep, currentFullLargeObjectBlocks);
    SweepLargeHeapBlockList(recyclerSweep, currentDisposeLargeBlockList);
}

void
LargeHeapBucket::SweepLargeHeapBlockList(RecyclerSweep& recyclerSweep, LargeHeapBlock * heapBlockList)
{TRACE_IT(24589);
    Recycler * recycler = recyclerSweep.GetRecycler();
    HeapBlockList::ForEachEditing(heapBlockList, [this, &recyclerSweep, recycler](LargeHeapBlock * heapBlock)
    {
        this->UnregisterFreeList(heapBlock->GetFreeList());

        // CONCURRENT-TODO: Allow large block to be sweep in the background
        SweepState state = heapBlock->Sweep(recyclerSweep, false);

        // If the block is already in the pending dispose list (re-entrant GC scenario), do nothing, leave it there
        if (heapBlock->IsInPendingDisposeList()) return;

        switch (state)
        {
        case SweepStateEmpty:
            heapBlock->ReleasePagesSweep(recycler);
            LargeHeapBlock::Delete(heapBlock);
            RECYCLER_SLOW_CHECK(this->heapInfo->heapBlockCount[HeapBlock::HeapBlockType::LargeBlockType]--);
            break;
        case SweepStateFull:
            heapBlock->SetNextBlock(this->fullLargeBlockList);
            this->fullLargeBlockList = heapBlock;
            break;
        case SweepStateSwept:
            if (supportFreeList)
            {TRACE_IT(24590);
                ConstructFreelist(heapBlock);
            }
            else
            {TRACE_IT(24591);
                ReinsertLargeHeapBlock(heapBlock);
            }

            break;
        case SweepStatePendingDispose:
            Assert(!recyclerSweep.IsBackground());
            Assert(!recycler->hasPendingTransferDisposedObjects);
            heapBlock->SetNextBlock(this->pendingDisposeLargeBlockList);
            this->pendingDisposeLargeBlockList = heapBlock;
            heapBlock->SetIsInPendingDisposeList(true);
#if DBG
            heapBlock->SetHasDisposeBeenCalled(false);
#endif
            recycler->hasDisposableObject = true;
            break;
#if ENABLE_CONCURRENT_GC
        case SweepStatePendingSweep:
            heapBlock->SetNextBlock(this->pendingSweepLargeBlockList);
            this->pendingSweepLargeBlockList = heapBlock;
            break;
#endif
        }
    });
}

void
LargeHeapBucket::ReinsertLargeHeapBlock(LargeHeapBlock * heapBlock)
{TRACE_IT(24592);
    Assert(!heapBlock->hasPartialFreeObjects);
    Assert(!heapBlock->IsInPendingDisposeList());

    if (this->largeBlockList != nullptr && heapBlock->GetFreeSize() > this->largeBlockList->GetFreeSize())
    {TRACE_IT(24593);
        heapBlock->SetNextBlock(this->largeBlockList->GetNextBlock());
        this->largeBlockList->SetNextBlock(this->fullLargeBlockList);
        this->fullLargeBlockList = this->largeBlockList;
        this->largeBlockList = heapBlock;
    }
    else
    {TRACE_IT(24594);
        heapBlock->SetNextBlock(this->fullLargeBlockList);
        this->fullLargeBlockList = heapBlock;
    }
}

void
LargeHeapBucket::RegisterFreeList(LargeHeapBlockFreeList* freeList)
{TRACE_IT(24595);
    Assert(freeList->next == nullptr);
    Assert(freeList->previous == nullptr);

    LargeHeapBlockFreeList* head = this->freeList;

    if (head)
    {TRACE_IT(24596);
        head->previous = freeList;
    }

    freeList->next = head;
    this->freeList = freeList;
}

void
LargeHeapBucket::UnregisterFreeList(LargeHeapBlockFreeList* freeList)
{TRACE_IT(24597);
    LargeHeapBlockFreeList* next = freeList->next;
    LargeHeapBlockFreeList* previous = freeList->previous;

    if (previous)
    {TRACE_IT(24598);
        previous->next = next;
    }

    if (next)
    {TRACE_IT(24599);
        next->previous = previous;
    }

    freeList->next = nullptr;
    freeList->previous = nullptr;

    if (freeList == this->freeList)
    {TRACE_IT(24600);
        this->freeList = next;
    }
}

void
LargeHeapBucket::ConstructFreelist(LargeHeapBlock * heapBlock)
{TRACE_IT(24601);
    Assert(!heapBlock->hasPartialFreeObjects);
    Assert(!heapBlock->IsInPendingDisposeList());

    // The free list is the only way we reuse heap block entries
    // so if the heap block is allocated from directly, it'll not
    // invalidate the free list
    LargeHeapBlockFreeList* freeList = heapBlock->GetFreeList();
    Assert(freeList);

    if (freeList->entries)
    {TRACE_IT(24602);
        this->RegisterFreeList(freeList);

#if DBG
        LargeAllocationVerboseTrace(this->GetRecycler()->GetRecyclerFlagsTable(), _u("Free list created for 0x%x bucket\n"), this->sizeCat);
#endif
    }

    ReinsertLargeHeapBlock(heapBlock);
}

#pragma endregion

size_t
LargeHeapBucket::Rescan(LargeHeapBlock * list, Recycler * recycler, bool isPartialSwept, RescanFlags flags)
{TRACE_IT(24603);
    size_t scannedPageCount = 0;
    HeapBlockList::ForEach(list, [recycler, isPartialSwept, flags, &scannedPageCount](LargeHeapBlock * heapBlock)
    {
        scannedPageCount += heapBlock->Rescan(recycler, isPartialSwept, flags);
    });
    return scannedPageCount;
}

size_t
LargeHeapBucket::Rescan(RescanFlags flags)
{TRACE_IT(24604);
#if ENABLE_CONCURRENT_GC
    Assert(pendingSweepLargeBlockList == nullptr);
#endif

    size_t scannedPageCount = 0;
    Recycler* recycler = this->heapInfo->recycler;

    scannedPageCount += LargeHeapBucket::Rescan(largeBlockList, recycler, false, flags);
#ifdef RECYCLER_PAGE_HEAP
    scannedPageCount += LargeHeapBucket::Rescan(largePageHeapBlockList, recycler, false, flags);
#endif
    scannedPageCount += LargeHeapBucket::Rescan(fullLargeBlockList, recycler, false, flags);
    scannedPageCount += LargeHeapBucket::Rescan(pendingDisposeLargeBlockList, recycler, true, flags);

#if ENABLE_PARTIAL_GC && ENABLE_CONCURRENT_GC
    Assert(recycler->inPartialCollectMode || partialSweptLargeBlockList == nullptr);
    if (recycler->inPartialCollectMode)
    {TRACE_IT(24605);
        scannedPageCount += LargeHeapBucket::Rescan(partialSweptLargeBlockList, recycler, true, flags);
    }
#endif
    return scannedPageCount;
}

#if ENABLE_PARTIAL_GC || ENABLE_CONCURRENT_GC
void
LargeHeapBucket::SweepPendingObjects(RecyclerSweep& recyclerSweep)
{TRACE_IT(24606);
#if ENABLE_CONCURRENT_GC
    if (recyclerSweep.IsBackground())
    {TRACE_IT(24607);
        Recycler * recycler = recyclerSweep.GetRecycler();
#if ENABLE_PARTIAL_GC
        if (recycler->inPartialCollectMode)
        {TRACE_IT(24608);
            HeapBlockList::ForEach(this->pendingSweepLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
            {
                // Page heap blocks are never swept concurrently
                heapBlock->SweepObjects<SweepMode_ConcurrentPartial>(recycler);
            });
        }
        else
#endif
        {TRACE_IT(24609);
            HeapBlockList::ForEach(this->pendingSweepLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
            {
                // Page heap blocks are never swept concurrently
                heapBlock->SweepObjects<SweepMode_Concurrent>(recycler);
            });
        }
    }
    else
    {TRACE_IT(24610);
        Assert(this->pendingSweepLargeBlockList == nullptr);
    }
#endif
}

#if ENABLE_PARTIAL_GC
#if ENABLE_CONCURRENT_GC
void
LargeHeapBucket::ConcurrentPartialTransferSweptObjects(RecyclerSweep& recyclerSweep)
{TRACE_IT(24611);
    Assert(recyclerSweep.InPartialCollectMode());
    Assert(!recyclerSweep.IsBackground());

    RECYCLER_SLOW_CHECK(this->VerifyLargeHeapBlockCount());

    LargeHeapBlock * list = this->pendingSweepLargeBlockList;
    this->pendingSweepLargeBlockList = nullptr;
    HeapBlockList::ForEachEditing(list, [this](LargeHeapBlock * heapBlock)
    {
        // GC-REVIEW: We could maybe reuse the large objects
        heapBlock->PartialTransferSweptObjects();
        heapBlock->SetNextBlock(this->partialSweptLargeBlockList);
        this->partialSweptLargeBlockList = heapBlock;
    });

    RECYCLER_SLOW_CHECK(this->VerifyLargeHeapBlockCount());
}
#endif

void
LargeHeapBucket::FinishPartialCollect(RecyclerSweep * recyclerSweep)
{TRACE_IT(24612);
#if ENABLE_CONCURRENT_GC
    Recycler* recycler = this->heapInfo->recycler;

    if (recyclerSweep && recyclerSweep->IsBackground())
    {TRACE_IT(24613);
        // Leave it in the partialSweptLargeBlockList if we are processing it in the background
        // ConcurrentTransferSweptObjects will put it back.
        HeapBlockList::ForEachEditing(partialSweptLargeBlockList, [this, recycler](LargeHeapBlock * heapBlock)
        {
            heapBlock->FinishPartialCollect(recycler);
        });
    }
    else
    {TRACE_IT(24614);
        HeapBlockList::ForEachEditing(partialSweptLargeBlockList, [this, recycler](LargeHeapBlock * heapBlock)
        {
            heapBlock->FinishPartialCollect(recycler);
            this->ReinsertLargeHeapBlock(heapBlock);
        });
        this->partialSweptLargeBlockList = nullptr;
    }
#endif
}
#endif

#if ENABLE_CONCURRENT_GC
void
LargeHeapBucket::ConcurrentTransferSweptObjects(RecyclerSweep& recyclerSweep)
{TRACE_IT(24615);
#if ENABLE_PARTIAL_GC
    Assert(!recyclerSweep.InPartialCollectMode());
#endif
    Assert(!recyclerSweep.IsBackground());

    HeapBlockList::ForEachEditing(this->pendingSweepLargeBlockList, [this](LargeHeapBlock * heapBlock)
    {
        heapBlock->TransferSweptObjects();
        ReinsertLargeHeapBlock(heapBlock);
    });
    this->pendingSweepLargeBlockList = nullptr;

#if ENABLE_PARTIAL_GC
    // If we did a background finish partial collect, we have left the partialSweptLargeBlockList
    // there because can't reinsert the heap block in the background, do it here now.
    HeapBlockList::ForEachEditing(this->partialSweptLargeBlockList, [this](LargeHeapBlock * heapBlock)
    {
        ReinsertLargeHeapBlock(heapBlock);
    });
    this->partialSweptLargeBlockList = nullptr;
#endif
}

#endif

#endif

void
LargeHeapBucket::FinalizeAllObjects()
{TRACE_IT(24616);
    ForEachLargeHeapBlock([](LargeHeapBlock * heapBlock) { heapBlock->FinalizeAllObjects(); });
}


void
LargeHeapBucket::Finalize(Recycler * recycler, LargeHeapBlock * heapBlockList)
{TRACE_IT(24617);
    HeapBlockList::ForEachEditing(heapBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->FinalizeObjects(recycler);
    });
}

void
LargeHeapBucket::Finalize()
{TRACE_IT(24618);
    Recycler* recycler = this->heapInfo->recycler;

    // Finalize any free objects in the non-filled large heap blocks
    Finalize(recycler, largeBlockList);

#ifdef RECYCLER_PAGE_HEAP
    Finalize(recycler, largePageHeapBlockList);
#endif

    // Finalize any free objects in the filled large heap blocks
    Finalize(recycler, fullLargeBlockList);

    // Finalize any free objects in the large heap blocks which have objects pending dispose
    // This is to handle the case where if during dispose, a GC is triggered, we might have
    // found more objects to free. These objects might reside in a block that was moved to the
    // pendingDisposeLargeBlockList during the outer GC. So we need to walk through this list
    // again and finalize any objects that need to be finalized. If we don't, they would
    // not get finalized
    Finalize(recycler, pendingDisposeLargeBlockList);
}

void
LargeHeapBucket::DisposeObjects()
{TRACE_IT(24619);
    Recycler * recycler = this->heapInfo->recycler;
    HeapBlockList::ForEach(this->pendingDisposeLargeBlockList, [recycler](LargeHeapBlock * heapBlock)
    {
        heapBlock->DisposeObjects(recycler);
    });
}

void
LargeHeapBucket::TransferDisposedObjects()
{TRACE_IT(24620);
#if ENABLE_CONCURRENT_GC
    Recycler * recycler = this->heapInfo->recycler;
    Assert(!recycler->IsConcurrentExecutingState());
#endif

    HeapBlockList::ForEachEditing(this->pendingDisposeLargeBlockList, [this](LargeHeapBlock * heapBlock)
    {
        /* GC-TODO: large heap block doesn't support free list yet */
        heapBlock->SetIsInPendingDisposeList(false);
        ReinsertLargeHeapBlock(heapBlock);
    });

    this->pendingDisposeLargeBlockList = nullptr;
}

void
LargeHeapBucket::EnumerateObjects(ObjectInfoBits infoBits, void (*CallBackFunction)(void * address, size_t size))
{TRACE_IT(24621);
    HeapBucket::EnumerateObjects(largeBlockList, infoBits, CallBackFunction);
#ifdef RECYCLER_PAGE_HEAP
    HeapBucket::EnumerateObjects(largePageHeapBlockList, infoBits, CallBackFunction);
#endif
    HeapBucket::EnumerateObjects(fullLargeBlockList, infoBits, CallBackFunction);

    // Pending dispose large block list need not be null
    // When we enumerate over this list, anything that has been swept/finalized won't be
    // enumerated since it needs to have the object header for enumeration
    // and we set the header to null upon sweep/finalize
    HeapBucket::EnumerateObjects(pendingDisposeLargeBlockList, infoBits, CallBackFunction);
#if ENABLE_CONCURRENT_GC
    Assert(this->pendingSweepLargeBlockList == nullptr);
#if ENABLE_PARTIAL_GC
    HeapBucket::EnumerateObjects(partialSweptLargeBlockList, infoBits, CallBackFunction);
#endif
#endif
}

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)

size_t
LargeHeapBucket::GetLargeHeapBlockCount(bool checkCount) const
{TRACE_IT(24622);
    size_t currentLargeHeapBlockCount = HeapBlockList::Count(fullLargeBlockList);
    currentLargeHeapBlockCount += HeapBlockList::Count(largeBlockList);
#ifdef RECYCLER_PAGE_HEAP
    currentLargeHeapBlockCount += HeapBlockList::Count(largePageHeapBlockList);
#endif
    currentLargeHeapBlockCount += HeapBlockList::Count(pendingDisposeLargeBlockList);
#if ENABLE_CONCURRENT_GC
    currentLargeHeapBlockCount += HeapBlockList::Count(pendingSweepLargeBlockList);
#if ENABLE_PARTIAL_GC
    currentLargeHeapBlockCount += HeapBlockList::Count(partialSweptLargeBlockList);
#endif
#endif

    return currentLargeHeapBlockCount;
}
#endif

#ifdef RECYCLER_SLOW_CHECK_ENABLED
size_t
LargeHeapBucket::Check()
{TRACE_IT(24623);
    size_t currentLargeHeapBlockCount = Check(false, false, largeBlockList);
#ifdef RECYCLER_PAGE_HEAP
    currentLargeHeapBlockCount += Check(true, false, largePageHeapBlockList);
#endif
    currentLargeHeapBlockCount += Check(true, false, fullLargeBlockList);

#if ENABLE_CONCURRENT_GC
    Assert(pendingSweepLargeBlockList == nullptr);
#if ENABLE_PARTIAL_GC
    currentLargeHeapBlockCount += Check(false, false, partialSweptLargeBlockList);
#endif
#endif
    currentLargeHeapBlockCount += Check(false, true, pendingDisposeLargeBlockList);
    return currentLargeHeapBlockCount;
}

template <typename TBlockType>
size_t
LargeHeapBucket::Check(bool expectFull, bool expectPending, TBlockType * list, TBlockType * tail)
{TRACE_IT(24624);
    size_t heapBlockCount = 0;
    HeapBlockList::ForEach(list, tail, [&heapBlockCount, expectFull, expectPending](TBlockType * heapBlock)
    {
        heapBlock->Check(expectFull, expectPending);
        heapBlockCount++;
    });
    return heapBlockCount;
}

template size_t LargeHeapBucket::Check<LargeHeapBlock>(bool expectFull, bool expectPending, LargeHeapBlock * list, LargeHeapBlock * tail);

void
LargeHeapBucket::VerifyLargeHeapBlockCount()
{TRACE_IT(24625);
    GetLargeHeapBlockCount(true);
}
#endif

#ifdef RECYCLER_MEMORY_VERIFY
void
LargeHeapBucket::Verify()
{TRACE_IT(24626);
    Recycler * recycler = this->heapInfo->recycler;
    HeapBlockList::ForEach(largeBlockList, [recycler](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->Verify(recycler);
    });
#ifdef RECYCLER_PAGE_HEAP
    HeapBlockList::ForEach(largePageHeapBlockList, [recycler](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->Verify(recycler);
    });
#endif
    HeapBlockList::ForEach(fullLargeBlockList, [recycler](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->Verify(recycler);
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [recycler](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->Verify(recycler);
    });
}
#endif

#ifdef RECYCLER_VERIFY_MARK
void
LargeHeapBucket::VerifyMark()
{TRACE_IT(24627);
    HeapBlockList::ForEach(largeBlockList, [](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->VerifyMark();
    });
    HeapBlockList::ForEach(fullLargeBlockList, [](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->VerifyMark();
    });
    HeapBlockList::ForEach(pendingDisposeLargeBlockList, [](LargeHeapBlock * largeHeapBlock)
    {
        largeHeapBlock->VerifyMark();
    });
}
#endif

