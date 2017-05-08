//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

template <class TBlockType>
SmallFinalizableHeapBucketBaseT<TBlockType>::SmallFinalizableHeapBucketBaseT() :
    pendingDisposeList(nullptr)
{TRACE_IT(26896);
#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    tempPendingDisposeList = nullptr;
#endif
}

template <class TBlockType>
SmallFinalizableHeapBucketBaseT<TBlockType>::~SmallFinalizableHeapBucketBaseT()
{TRACE_IT(26897);
    Assert(this->AllocatorsAreEmpty());
    this->DeleteHeapBlockList(this->pendingDisposeList);
    Assert(this->tempPendingDisposeList == nullptr);
}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::FinalizeAllObjects()
{TRACE_IT(26898);
    // Finalize all objects on shutdown.

    // Clear allocators to update the information on the heapblock
    // Walk through the allocated object and call finalize and dispose on them
    this->ClearAllocators();
    FinalizeHeapBlockList(this->pendingDisposeList);
#if ENABLE_PARTIAL_GC
    FinalizeHeapBlockList(this->partialHeapBlockList);
#endif
    FinalizeHeapBlockList(this->heapBlockList);
    FinalizeHeapBlockList(this->fullBlockList);

#if ENABLE_PARTIAL_GC && ENABLE_CONCURRENT_GC
    FinalizeHeapBlockList(this->partialSweptHeapBlockList);
#endif
}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::FinalizeHeapBlockList(TBlockType * list)
{TRACE_IT(26899);
    HeapBlockList::ForEach(list, [](TBlockType * heapBlock)
    {
        heapBlock->FinalizeAllObjects();
    });
}

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
template <class TBlockType>
size_t
SmallFinalizableHeapBucketBaseT<TBlockType>::GetNonEmptyHeapBlockCount(bool checkCount) const
{TRACE_IT(26900);
    size_t currentHeapBlockCount =  __super::GetNonEmptyHeapBlockCount(false)
        + HeapBlockList::Count(pendingDisposeList)
        + HeapBlockList::Count(tempPendingDisposeList);
    RECYCLER_SLOW_CHECK(Assert(!checkCount || this->heapBlockCount == currentHeapBlockCount));
    return currentHeapBlockCount;
}
#endif

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::ResetMarks(ResetMarkFlags flags)
{TRACE_IT(26901);
    __super::ResetMarks(flags);

    if ((flags & ResetMarkFlags_ScanImplicitRoot) != 0)
    {TRACE_IT(26902);
        HeapBlockList::ForEach(this->pendingDisposeList, [flags](TBlockType * heapBlock)
        {
            heapBlock->MarkImplicitRoots();
        });
    }
}

#ifdef DUMP_FRAGMENTATION_STATS
template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::AggregateBucketStats(HeapBucketStats& stats)
{TRACE_IT(26903);
    __super::AggregateBucketStats(stats);

    HeapBlockList::ForEach(pendingDisposeList, [&stats](TBlockType* heapBlock) {
        heapBlock->AggregateBlockStats(stats);
    });
}
#endif

template<class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::Sweep(RecyclerSweep& recyclerSweep)
{TRACE_IT(26904);
    Assert(!recyclerSweep.IsBackground());

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
    Assert(this->tempPendingDisposeList == nullptr);
    this->tempPendingDisposeList = pendingDisposeList;
#endif

    TBlockType * currentDisposeList = pendingDisposeList;
    this->pendingDisposeList = nullptr;

    BaseT::SweepBucket(recyclerSweep, [=](RecyclerSweep& recyclerSweep)
    {
#if DBG
        if (TBlockType::HeapBlockAttributes::IsSmallBlock)
        {TRACE_IT(26905);
            recyclerSweep.SetupVerifyListConsistencyDataForSmallBlock(nullptr, false, true);
        }
        else if (TBlockType::HeapBlockAttributes::IsMediumBlock)
        {TRACE_IT(26906);
            recyclerSweep.SetupVerifyListConsistencyDataForMediumBlock(nullptr, false, true);
        }
        else
        {TRACE_IT(26907);
            Assert(false);
        }
#endif

        HeapBucketT<TBlockType>::SweepHeapBlockList(recyclerSweep, currentDisposeList, false);

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
        Assert(this->tempPendingDisposeList == currentDisposeList);
        this->tempPendingDisposeList = nullptr;
#endif
        RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(recyclerSweep.IsBackground()));
    });

}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::DisposeObjects()
{TRACE_IT(26908);
    HeapBlockList::ForEach(this->pendingDisposeList, [](TBlockType * heapBlock)
    {
        Assert(heapBlock->HasAnyDisposeObjects());
        heapBlock->DisposeObjects();
    });
}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::TransferDisposedObjects()
{TRACE_IT(26909);
    Assert(!this->IsAllocationStopped());
    TBlockType * currentPendingDisposeList = this->pendingDisposeList;
    if (currentPendingDisposeList != nullptr)
    {TRACE_IT(26910);
        this->pendingDisposeList = nullptr;

        HeapBlockList::ForEach(currentPendingDisposeList, [=](TBlockType * heapBlock)
        {
            heapBlock->TransferDisposedObjects();

            // in pageheap, we actually always have free object
            Assert(heapBlock->template HasFreeObject<false>());
        });

        // For partial collect, dispose will modify the object, and we
        // also touch the page by chaining the object through the free list
        // might as well reuse the block for partial collect
        this->AppendAllocableHeapBlockList(currentPendingDisposeList);
    }

    RECYCLER_SLOW_CHECK(this->VerifyHeapBlockCount(false));
}

template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::EnumerateObjects(ObjectInfoBits infoBits, void(*CallBackFunction)(void * address, size_t size))
{TRACE_IT(26911);
    __super::EnumerateObjects(infoBits, CallBackFunction);
    HeapBucket::EnumerateObjects(this->pendingDisposeList, infoBits, CallBackFunction);
}

#ifdef RECYCLER_SLOW_CHECK_ENABLED
template <class TBlockType>
size_t
SmallFinalizableHeapBucketBaseT<TBlockType>::Check()
{TRACE_IT(26912);
    size_t smallHeapBlockCount = __super::Check(false) + HeapInfo::Check(false, true, this->pendingDisposeList);
    Assert(this->heapBlockCount == smallHeapBlockCount);
    return smallHeapBlockCount;
}
#endif

#ifdef RECYCLER_MEMORY_VERIFY
template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::Verify()
{TRACE_IT(26913);
    BaseT::Verify();

#if DBG
    RecyclerVerifyListConsistencyData recyclerVerifyListConsistencyData;
    if (TBlockType::HeapBlockAttributes::IsSmallBlock)
    {TRACE_IT(26914);
        recyclerVerifyListConsistencyData.SetupVerifyListConsistencyDataForSmallBlock(nullptr, false, true);
    }
    else if (TBlockType::HeapBlockAttributes::IsMediumBlock)
    {TRACE_IT(26915);
        recyclerVerifyListConsistencyData.SetupVerifyListConsistencyDataForMediumBlock(nullptr, false, true);
    }
    else
    {TRACE_IT(26916);
        Assert(false);
    }

    HeapBlockList::ForEach(this->pendingDisposeList, [this, &recyclerVerifyListConsistencyData](TBlockType * heapBlock)
    {
        DebugOnly(this->VerifyBlockConsistencyInList(heapBlock, recyclerVerifyListConsistencyData));
        heapBlock->Verify(true);
    });
#endif

}
#endif

#ifdef RECYCLER_VERIFY_MARK
template <class TBlockType>
void
SmallFinalizableHeapBucketBaseT<TBlockType>::VerifyMark()
{TRACE_IT(26917);
    __super::VerifyMark();
    HeapBlockList::ForEach(this->pendingDisposeList, [](TBlockType * heapBlock)
    {
        Assert(heapBlock->HasAnyDisposeObjects());
        heapBlock->VerifyMark();
    });
}
#endif

namespace Memory
{
    template class SmallFinalizableHeapBucketBaseT<SmallFinalizableHeapBlock>;
#ifdef RECYCLER_WRITE_BARRIER
    template class SmallFinalizableHeapBucketBaseT<SmallFinalizableWithBarrierHeapBlock>;
#endif

    template class SmallFinalizableHeapBucketBaseT<MediumFinalizableHeapBlock>;
#ifdef RECYCLER_WRITE_BARRIER
    template class SmallFinalizableHeapBucketBaseT<MediumFinalizableWithBarrierHeapBlock>;
#endif
}
