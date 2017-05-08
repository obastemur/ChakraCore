//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
#include "Base/ScriptMemoryDumper.h"

ScriptMemoryDumper::ScriptMemoryDumper(Js::ScriptContext* scriptContext)
    :scriptContext(scriptContext)
{
    memset(&current, 0, sizeof(current));
    memset(&total, 0, sizeof(total));
    Init();
}


void ScriptMemoryDumper::Init()
{TRACE_IT(36861);
    pageCountId = scriptContext->GetOrAddPropertyIdTracked(_u("pageCount"));
    objectSizeId = scriptContext->GetOrAddPropertyIdTracked(_u("objectSize"));
    freeObjectCountId = scriptContext->GetOrAddPropertyIdTracked(_u("freeObjectCount"));
    activeObjectCountId = scriptContext->GetOrAddPropertyIdTracked(_u("activeObjectCount"));
    totalByteCountId = scriptContext->GetOrAddPropertyIdTracked(_u("totalByteCount"));
    finalizeCountId = scriptContext->GetOrAddPropertyIdTracked(_u("finalizeCount"));
    weakReferenceCountId = scriptContext->GetOrAddPropertyIdTracked(_u("weakReferenceCount"));
    largeObjectsId = scriptContext->GetOrAddPropertyIdTracked(_u("largeObjects"));
    activeObjectByteSizeId = scriptContext->GetOrAddPropertyIdTracked(_u("activeObjectByteSize"));
    summaryId = scriptContext->GetOrAddPropertyIdTracked(_u("summary"));
    dumpObject = scriptContext->GetLibrary()->CreateObject();
}

// Export script related memory to javascript object containing related information.
Js::Var ScriptMemoryDumper::Dump()
{TRACE_IT(36862);
    Recycler* recycler = scriptContext->GetRecycler();
    HeapInfo* heapInfo = recycler->GetAutoHeap();

    for (uint32 i = 0 ; i < HeapConstants::BucketCount; i++)
    {TRACE_IT(36863);
        ResetCurrentStats();
        size_t sizeCat = (i + 1) * HeapConstants::ObjectGranularity;
        DumpHeapBucket(i, &heapInfo->GetBucket<LeafBit>(sizeCat));
        DumpHeapBucket(i, &heapInfo->GetBucket<NoBit>(sizeCat));
        DumpHeapBucket(i, (SmallFinalizableHeapBucket *)&heapInfo->GetBucket<FinalizeBit>(sizeCat));
        SaveCurrentAtIndex(i);
        MergeCurrentStats();
    }

#ifdef BUCKETIZE_MEDIUM_ALLOCATIONS
    for (uint32 i = 0 ; i < HeapConstants::MediumBucketCount; i++)
    {TRACE_IT(36864);
        ResetCurrentStats();
        size_t sizeCat = HeapConstants::MaxSmallObjectSize + ((i + 1) * HeapConstants::ObjectGranularity);

#if SMALLBLOCK_MEDIUM_ALLOC
        DumpHeapBucket(i, &heapInfo->GetMediumBucket<LeafBit>(sizeCat));
        DumpHeapBucket(i, &heapInfo->GetMediumBucket<NoBit>(sizeCat));
        DumpHeapBucket(i, (MediumFinalizableHeapBucket *)&heapInfo->GetMediumBucket<FinalizeBit>(sizeCat));
#else
        DumpLargeBucket(&heapInfo->GetMediumBucket(sizeCat));
#endif
        SaveCurrentAtIndex(i + HeapConstants::BucketCount);
        MergeCurrentStats();
    }
#endif

    ResetCurrentStats();
    DumpLargeBucket(&heapInfo->largeObjectBucket);
    SaveCurrentAsLargeBlock();
    MergeCurrentStats();
    SaveSummary();
    return dumpObject;
}

template <typename TBlockType>
void ScriptMemoryDumper::DumpHeapBucket(uint index, HeapBucketT<TBlockType>* heapBucket)
{TRACE_IT(36865);
    SmallHeapBlockAllocator<TBlockType> * currentAllocator = heapBucket->GetAllocator();
    do
    {TRACE_IT(36866);
        DumpSmallHeapBlock(currentAllocator->GetHeapBlock());
        currentAllocator = currentAllocator->GetNext();
    }
    while (currentAllocator != heapBucket->GetAllocator());

    DumpSmallHeapBlockList(heapBucket->fullBlockList);
    DumpSmallHeapBlockList(heapBucket->heapBlockList);
}

template <typename TBlockType, typename TBlockAttributes>
void ScriptMemoryDumper::DumpHeapBucket(uint index, SmallNormalHeapBucketBase<TBlockType>* heapBucket)
{
    DumpHeapBucket(index, (HeapBucketT<TBlockType> *)heapBucket);
    DumpSmallHeapBlockList(((SmallNormalHeapBucketBase<TBlockType, TBlockAttributes> *)heapBucket)->partialHeapBlockList);
    DumpSmallHeapBlockList(((SmallNormalHeapBucketBase<TBlockType, TBlockAttributes> *)heapBucket)->partialSweptHeapBlockList);
}

template <typename TBlockAttributes>
void ScriptMemoryDumper::DumpHeapBucket(uint index, SmallFinalizableHeapBucketT<TBlockAttributes>* heapBucket)
{
    DumpHeapBucket(index, (SmallNormalHeapBucketBase<SmallFinalizableHeapBlockT<TBlockAttributes>> *)heapBucket);
    DumpSmallHeapBlockList(heapBucket->pendingDisposeList);
}

template <class TBlockAttributes>
void ScriptMemoryDumper::DumpSmallHeapBlockList(SmallHeapBlockT<TBlockAttributes>* heapBlockHead)
{TRACE_IT(36867);
    HeapBlockList::ForEach(heapBlockHead, [this](SmallHeapBlockT<TBlockAttributes> * heapBlock)
    {
        DumpSmallHeapBlock(heapBlock);
    });
}

template <class TBlockAttributes>
void ScriptMemoryDumper::DumpSmallHeapBlock(SmallHeapBlockT<TBlockAttributes>* heapBlock)
{TRACE_IT(36868);
    if (heapBlock == nullptr)
        return;

    if (current.objectSize == 0)
    {TRACE_IT(36869);
        current.objectSize = heapBlock->objectSize;
    }
    Assert(current.objectSize == heapBlock->GetObjectSize());
    current.freeObjectCount = heapBlock->freeCount;
    current.activeObjectCount += heapBlock->objectCount - heapBlock->freeCount;
    if (heapBlock->IsAnyFinalizableBlock())
    {TRACE_IT(36870);
        current.finalizeCount += heapBlock->template AsFinalizableBlock<TBlockAttributes>()->finalizeCount;
    }
    current.pageCount += heapBlock->GetPageCount();
    current.totalByteCount += heapBlock->GetPageCount() * AutoSystemInfo::PageSize;
    current.activeObjectByteSize += (heapBlock->objectCount - heapBlock->freeCount)* current.objectSize;
}

void ScriptMemoryDumper::DumpLargeHeapBlockList(LargeHeapBlock* heapBlockHead)
{TRACE_IT(36871);
    HeapBlockList::ForEach(heapBlockHead, [this](LargeHeapBlock * heapBlock)
    {
        DumpLargeHeapBlock(heapBlock);
    });
}

void ScriptMemoryDumper::DumpLargeBucket(LargeHeapBucket* heapBucket)
{TRACE_IT(36872);
    DumpLargeHeapBlockList(heapBucket->fullLargeBlockList);
    DumpLargeHeapBlockList(heapBucket->largeBlockList);
#ifdef RECYCLER_PAGE_HEAP
    DumpLargeHeapBlockList(heapBucket->largePageHeapBlockList);
#endif
    DumpLargeHeapBlockList(heapBucket->pendingDisposeLargeBlockList);
#if ENABLE_CONCURRENT_GC
    DumpLargeHeapBlockList(heapBucket->pendingSweepLargeBlockList);
#if ENABLE_PARTIAL_GC
    DumpLargeHeapBlockList(heapBucket->partialSweptLargeBlockList);
#endif
#endif
}

struct LargeObjectHeader;
void ScriptMemoryDumper::DumpLargeHeapBlock(LargeHeapBlock* heapBlock)
{TRACE_IT(36873);
    if (heapBlock == nullptr)
        return;

    current.finalizeCount += heapBlock->finalizeCount;
    current.pageCount += heapBlock->GetPageCount();
    current.totalByteCount += heapBlock->GetPageCount() * AutoSystemInfo::PageSize;

    for (uint32 i = 0; i < heapBlock->allocCount; i++)
    {TRACE_IT(36874);
        Memory::LargeObjectHeader* heapHeader = heapBlock->GetHeader(i);
        if (heapHeader != nullptr)
        {TRACE_IT(36875);
            current.activeObjectCount++;
            current.activeObjectByteSize += heapHeader->objectSize;
        }
    }
}

inline void ScriptMemoryDumper::ResetCurrentStats()
{
    memset(&current, 0, sizeof(current));
}

inline void ScriptMemoryDumper::MergeCurrentStats()
{TRACE_IT(36876);
    total.pageCount += current.pageCount;
    total.activeObjectCount += current.activeObjectCount;
    total.activeObjectByteSize += current.activeObjectByteSize;
    total.finalizeCount += current.finalizeCount;
    total.totalByteCount += current.totalByteCount;
    total.freeObjectCount += current.freeObjectCount;
}

void ScriptMemoryDumper::SaveCurrentAtIndex(uint32 index)
{TRACE_IT(36877);
    Js::DynamicObject* currentBucket = scriptContext->GetLibrary()->CreateObject();
    FillObjectWithStats(currentBucket, current);
    dumpObject->SetItem(index, currentBucket, Js::PropertyOperation_None);
}

void ScriptMemoryDumper::SaveCurrentAsLargeBlock()
{TRACE_IT(36878);
    Js::DynamicObject* largeObjectStat = scriptContext->GetLibrary()->CreateObject();
    FillObjectWithStats(largeObjectStat, current);
    dumpObject->SetProperty(largeObjectsId, largeObjectStat, Js::PropertyOperation_None, NULL);
}

void ScriptMemoryDumper::SaveSummary()
{TRACE_IT(36879);
    Js::DynamicObject* summaryStat = scriptContext->GetLibrary()->CreateObject();
    FillObjectWithStats(summaryStat, total);
    dumpObject->SetProperty(summaryId, summaryStat, Js::PropertyOperation_None, NULL);
}

void ScriptMemoryDumper::FillObjectWithStats(Js::DynamicObject* dynamicObject, HeapStats stats)
{TRACE_IT(36880);
    dynamicObject->SetProperty(pageCountId, Js::JavascriptUInt64Number::ToVar(stats.pageCount, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(objectSizeId, Js::JavascriptNumber::New(stats.objectSize, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(freeObjectCountId, Js::JavascriptNumber::New(stats.freeObjectCount, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(activeObjectCountId, Js::JavascriptNumber::New(stats.activeObjectCount, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(activeObjectByteSizeId, Js::JavascriptUInt64Number::ToVar(stats.activeObjectByteSize, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(totalByteCountId, Js::JavascriptUInt64Number::ToVar(stats.totalByteCount, scriptContext),  Js::PropertyOperation_None, NULL);
    dynamicObject->SetProperty(finalizeCountId, Js::JavascriptNumber::New(stats.finalizeCount, scriptContext),  Js::PropertyOperation_None, NULL);
}

#endif // ENABLE_DEBUG_CONFIG_OPTIONS
