//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#ifdef RECYCLER_WRITE_BARRIER
template <class TBlockAttributes>
SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes>*
SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes>::New(HeapBucketT<SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes>> * bucket)
{TRACE_IT(26838);
    CompileAssert(TBlockAttributes::MaxObjectSize <= USHRT_MAX);
    Assert(bucket->sizeCat <= TBlockAttributes::MaxObjectSize);
    Assert((TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / bucket->sizeCat <= USHRT_MAX);

    ushort objectSize = (ushort)bucket->sizeCat;
    ushort objectCount = (ushort)(TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / objectSize;
    return NoMemProtectHeapNewNoThrowPlusPrefixZ(Base::GetAllocPlusSize(objectCount), SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes>, bucket, objectSize, objectCount);
}

template <class TBlockAttributes>
void
SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes>::Delete(SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes>* heapBlock)
{TRACE_IT(26839);
    Assert(heapBlock->IsAnyFinalizableBlock());
    Assert(heapBlock->IsWithBarrier());

    NoMemProtectHeapDeletePlusPrefix(Base::GetAllocPlusSize(heapBlock->objectCount), heapBlock);
}
#endif

template <class TBlockAttributes>
SmallFinalizableHeapBlockT<TBlockAttributes> *
SmallFinalizableHeapBlockT<TBlockAttributes>::New(HeapBucketT<SmallFinalizableHeapBlockT<TBlockAttributes>> * bucket)
{TRACE_IT(26840);
    CompileAssert(TBlockAttributes::MaxObjectSize <= USHRT_MAX);
    Assert(bucket->sizeCat <= TBlockAttributes::MaxObjectSize);
    Assert((TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / bucket->sizeCat <= USHRT_MAX);

    ushort objectSize = (ushort)bucket->sizeCat;
    ushort objectCount = (ushort)(TBlockAttributes::PageCount * AutoSystemInfo::PageSize) / objectSize;
    return NoMemProtectHeapNewNoThrowPlusPrefixZ(Base::GetAllocPlusSize(objectCount), SmallFinalizableHeapBlockT<TBlockAttributes>, bucket, objectSize, objectCount);
}

template <class TBlockAttributes>
void
SmallFinalizableHeapBlockT<TBlockAttributes>::Delete(SmallFinalizableHeapBlockT<TBlockAttributes> * heapBlock)
{TRACE_IT(26841);
    Assert(heapBlock->IsFinalizableBlock());
    NoMemProtectHeapDeletePlusPrefix(Base::GetAllocPlusSize(heapBlock->objectCount), heapBlock);
}

template <>
SmallFinalizableHeapBlockT<SmallAllocationBlockAttributes>::SmallFinalizableHeapBlockT(HeapBucketT<SmallFinalizableHeapBlockT<SmallAllocationBlockAttributes>> * bucket, ushort objectSize, ushort objectCount)
    : Base(bucket, objectSize, objectCount, HeapBlock::SmallFinalizableBlockType)
{TRACE_IT(26842);
    // We used AllocZ
    Assert(this->finalizeCount == 0);
    Assert(this->pendingDisposeCount == 0);
    Assert(this->disposedObjectList == nullptr);
    Assert(this->disposedObjectListTail == nullptr);
    Assert(!this->isPendingDispose);
}

template <>
SmallFinalizableHeapBlockT<MediumAllocationBlockAttributes>::SmallFinalizableHeapBlockT(HeapBucketT<SmallFinalizableHeapBlockT<MediumAllocationBlockAttributes>> * bucket, ushort objectSize, ushort objectCount)
    : Base(bucket, objectSize, objectCount, MediumFinalizableBlockType)
{TRACE_IT(26843);
    // We used AllocZ
    Assert(this->finalizeCount == 0);
    Assert(this->pendingDisposeCount == 0);
    Assert(this->disposedObjectList == nullptr);
    Assert(this->disposedObjectListTail == nullptr);
    Assert(!this->isPendingDispose);
}

#ifdef RECYCLER_WRITE_BARRIER
template <class TBlockAttributes>
SmallFinalizableHeapBlockT<TBlockAttributes>::SmallFinalizableHeapBlockT(HeapBucketT<SmallFinalizableWithBarrierHeapBlockT<TBlockAttributes>> * bucket, ushort objectSize, ushort objectCount, HeapBlockType blockType)
    : SmallNormalHeapBlockT<TBlockAttributes>(bucket, objectSize, objectCount, blockType)
{TRACE_IT(26844);
    // We used AllocZ
    Assert(this->finalizeCount == 0);
    Assert(this->pendingDisposeCount == 0);
    Assert(this->disposedObjectList == nullptr);
    Assert(this->disposedObjectListTail == nullptr);
    Assert(!this->isPendingDispose);
}
#endif

template <class TBlockAttributes>
void
SmallFinalizableHeapBlockT<TBlockAttributes>::SetAttributes(void * address, unsigned char attributes)
{TRACE_IT(26845);
    Assert((attributes & FinalizeBit) != 0);
    __super::SetAttributes(address, attributes);
    finalizeCount++;

#ifdef RECYCLER_FINALIZE_CHECK
    HeapInfo * heapInfo = this->heapBucket->heapInfo;
    heapInfo->liveFinalizableObjectCount++;
    heapInfo->newFinalizableObjectCount++;
#endif
}

template <class TBlockAttributes>
bool
SmallFinalizableHeapBlockT<TBlockAttributes>::TryGetAttributes(void* objectAddress, unsigned char * pAttr)
{TRACE_IT(26846);
    unsigned char * attributes = nullptr;
    if (this->TryGetAddressOfAttributes(objectAddress, &attributes))
    {TRACE_IT(26847);
        *pAttr = *attributes;
        return true;
    }
    return false;
}

template <class TBlockAttributes>
bool
SmallFinalizableHeapBlockT<TBlockAttributes>::TryGetAddressOfAttributes(void* objectAddress, unsigned char ** ppAttrs)
{TRACE_IT(26848);
    ushort objectIndex = this->GetAddressIndex(objectAddress);

    if (objectIndex == SmallHeapBlockT<TBlockAttributes>::InvalidAddressBit)
    {TRACE_IT(26849);
        // Not a valid offset within the block.  No further processing necessary.
        return false;
    }

    *ppAttrs = &this->ObjectInfo(objectIndex);
    return true;
}

template <class TBlockAttributes>
template <bool doSpecialMark>
_NOINLINE
void
SmallFinalizableHeapBlockT<TBlockAttributes>::ProcessMarkedObject(void* objectAddress, MarkContext * markContext)
{TRACE_IT(26850);
    unsigned char * attributes = nullptr;
    if (!this->TryGetAddressOfAttributes(objectAddress, &attributes))
    {TRACE_IT(26851);
        return;
    }

    if (!this->template UpdateAttributesOfMarkedObjects<doSpecialMark>(markContext, objectAddress, this->objectSize, *attributes,
        [&](unsigned char _attributes) { *attributes = _attributes; }))
    {
        // Couldn't mark children- bail out and come back later
        this->SetNeedOOMRescan(markContext->GetRecycler());
    }
}

#if ENABLE_PARTIAL_GC || ENABLE_CONCURRENT_GC
// static
template <class TBlockAttributes>
bool
SmallFinalizableHeapBlockT<TBlockAttributes>::CanRescanFullBlock()
{TRACE_IT(26852);
    // Finalizable block need to rescan object one at a time.
    return false;
}

// static
template <class TBlockAttributes>
bool
SmallFinalizableHeapBlockT<TBlockAttributes>::RescanObject(SmallFinalizableHeapBlockT<TBlockAttributes> * block, __in_ecount(localObjectSize) char * objectAddress, uint localObjectSize,
    uint objectIndex, Recycler * recycler)
{TRACE_IT(26853);
    unsigned char const attributes = block->ObjectInfo(objectIndex);
    Assert(block->IsAnyFinalizableBlock());

    if ((attributes & LeafBit) == 0)
    {TRACE_IT(26854);
        Assert(block->GetAddressIndex(objectAddress) != SmallHeapBlockT<TBlockAttributes>::InvalidAddressBit);

        if (!recycler->AddMark(objectAddress, localObjectSize))
        {TRACE_IT(26855);
            // Failed to add to the mark stack due to OOM.
            return false;
        }

        RECYCLER_STATS_INC(recycler, markData.rescanObjectCount);
        RECYCLER_STATS_ADD(recycler, markData.rescanObjectByteCount, localObjectSize);
    }

    // Since we mark through unallocated objects, we might have marked an object before it
    // is allocated as a tracked object. The object will not be queue up in the
    // tracked object list, and NewTrackBit will still be on. Queue it up now.

    // NewTrackBit will also be on for tracked object that we weren't able to queue
    // because of OOM.  In those case, the page is forced to be rescan, and we will
    // try to process those again here.
    if ((attributes & (TrackBit | NewTrackBit)) == (TrackBit | NewTrackBit))
    {TRACE_IT(26856);
        if (!block->RescanTrackedObject((FinalizableObject*) objectAddress, objectIndex, recycler))
        {TRACE_IT(26857);
            // Failed to add to the mark stack due to OOM.
            return false;
        }
    }
#ifdef RECYCLER_STATS
    else if (attributes & FinalizeBit)
    {TRACE_IT(26858);
        // Concurrent thread mark the object before the attribute is set and missed the finalize count
        // For finalized object, we will always write a dummy vtable before returning to the call,
        // so the page will always need to be rescanned, and we can count those here.

        // NewFinalizeBit is cleared if the background thread has already counted the object.
        // So if it is still set here, we need to count it

        RECYCLER_STATS_INC_IF(attributes & NewFinalizeBit, recycler, finalizeCount);
        block->ObjectInfo(objectIndex) &= ~NewFinalizeBit;
    }
#endif

    return true;
}

template <class TBlockAttributes>
bool
SmallFinalizableHeapBlockT<TBlockAttributes>::RescanTrackedObject(FinalizableObject * object, uint objectIndex, Recycler * recycler)
{TRACE_IT(26859);
    RecyclerVerboseTrace(recycler->GetRecyclerFlagsTable(), _u("Marking 0x%08x during rescan\n"), object);
#if ENABLE_CONCURRENT_GC
#if ENABLE_PARTIAL_GC
    if (recycler->inPartialCollectMode)
    {TRACE_IT(26860);
        Assert(!recycler->DoQueueTrackedObject());
    }
    else
#endif
    {TRACE_IT(26861);
        Assert(recycler->DoQueueTrackedObject());

        if (!recycler->QueueTrackedObject(object))
        {TRACE_IT(26862);
            // Failed to add to track stack due to OOM.
            return false;
        }
    }

    RECYCLER_STATS_INC(recycler, trackCount);
    RECYCLER_STATS_INC_IF(this->ObjectInfo(objectIndex) & FinalizeBit, recycler, finalizeCount);

    // We have processed this object as tracked, we can clear the NewTrackBit
    this->ObjectInfo(objectIndex) &= ~NewTrackBit;

    return true;
#else
    // REVIEW: Is this correct? Or should we remove the track bit always?
    return false;
#endif
}
#endif

template <class TBlockAttributes>
SweepState
SmallFinalizableHeapBlockT<TBlockAttributes>::Sweep(RecyclerSweep& recyclerSweep, bool queuePendingSweep, bool allocable)
{TRACE_IT(26863);
    Assert(!recyclerSweep.IsBackground());
    Assert(!queuePendingSweep);

    // If there are finalizable objects in this heap block, they need to be swept
    // in-thread and not in the concurrent thread, so don't queue pending sweep

    return SmallNormalHeapBlockT<TBlockAttributes>::Sweep(recyclerSweep, false, allocable, this->finalizeCount, HasAnyDisposeObjects());
}

template <class TBlockAttributes>
void
SmallFinalizableHeapBlockT<TBlockAttributes>::DisposeObjects()
{TRACE_IT(26864);
    Assert(this->isPendingDispose);
    Assert(HasAnyDisposeObjects());

    // PARTIALGC-CONSIDER: page with finalizable/disposable object will always be modified
    // because calling dispose probably will modify object itself, and it may call other
    // script that might touch the page as well.  We can't distinguish between these two kind
    // of write to the page.
    //
    // Possible mitigation include:
    //      - allocating finalizable/disposable object on separate pages
    //      - some of the object only need finalize, but not dispose.  mark them separately
    //
    // For now, we always touch the page by zeroing out disposed object which should be moved as well.

    ForEachPendingDisposeObject([&] (uint index) {
        void * objectAddress = this->address + (this->objectSize * index);

        // Dispose the object.
        // Note that Dispose can cause reentrancy, which can cause allocation, which can cause collection.
        // The object we're disposing is still considered PendingDispose until the Dispose call completes.
        // So in case we call CheckFreeBitVector or similar, we should still see correct state re this object.

        ((FinalizableObject *)objectAddress)->Dispose(false);

        Assert(finalizeCount != 0);
        finalizeCount--;
        Assert(pendingDisposeCount != 0);
        pendingDisposeCount--;

        // Properly enqueue the processed object
        // This will also clear the ObjectInfo bits so it's not marked as PendingDispose anymore
        this->EnqueueProcessedObject(&disposedObjectList, &disposedObjectListTail, objectAddress, index);

        RECYCLER_STATS_INC(this->heapBucket->heapInfo->recycler, finalizeSweepCount);
#ifdef RECYCLER_FINALIZE_CHECK
        this->heapBucket->heapInfo->liveFinalizableObjectCount--;
        this->heapBucket->heapInfo->pendingDisposableObjectCount--;
#endif
    });

    // Dispose could have re-entered and caused new pending dispose objects on this block.
    // If so, recycler->hasDisposableObject will have been set again, and we will do another
    // round of Dispose to actually dispose these objects.
    Assert(this->pendingDisposeCount == 0 || this->heapBucket->heapInfo->recycler->hasDisposableObject);
}

template <class TBlockAttributes>
void
SmallFinalizableHeapBlockT<TBlockAttributes>::TransferDisposedObjects()
{TRACE_IT(26865);
    // CONCURRENT-TODO: we don't allocate on pending disposed blocks during concurrent sweep or disable dispose
    // So the free bit vector must be valid
    Assert(this->IsFreeBitsValid());
    Assert(this->isPendingDispose);
    Assert(this->pendingDisposeCount == 0);

    DebugOnly(this->isPendingDispose = false);

    this->TransferProcessedObjects(this->disposedObjectList, this->disposedObjectListTail);
    this->disposedObjectList = nullptr;
    this->disposedObjectListTail = nullptr;

    // We already updated the bit vector on TransferSweptObjects
    // So just update the free object head.
    this->lastFreeObjectHead = this->freeObjectList;

    RECYCLER_SLOW_CHECK(this->CheckFreeBitVector(true));
}

template <class TBlockAttributes>
ushort
SmallFinalizableHeapBlockT<TBlockAttributes>::AddDisposedObjectFreeBitVector(SmallHeapBlockBitVector * free)
{TRACE_IT(26866);
    // all the finalized object are considered freed, but not allocable yet
    ushort freeCount = 0;
    FreeObject * freeObject = this->disposedObjectList;

    if (freeObject != nullptr)
    {TRACE_IT(26867);
        while (true)
        {TRACE_IT(26868);
            uint bitIndex = this->GetAddressBitIndex(freeObject);
            Assert(this->IsValidBitIndex(bitIndex));

            // not allocable yet
            Assert(!this->GetDebugFreeBitVector()->Test(bitIndex));

            // but in the free list to mark can skip scanning the object
            free->Set(bitIndex);
            freeCount++;

            if (freeObject == this->disposedObjectListTail)
            {TRACE_IT(26869);
                break;
            }
            freeObject = freeObject->GetNext();
        }
    }
    return freeCount;
}

template <class TBlockAttributes>
void
SmallFinalizableHeapBlockT<TBlockAttributes>::FinalizeAllObjects()
{TRACE_IT(26870);
    if (this->finalizeCount != 0)
    {TRACE_IT(26871);
        DebugOnly(uint processedCount = 0);
        this->ForEachAllocatedObject(FinalizeBit, [&](uint index, void * objectAddress)
        {
            FinalizableObject * finalizableObject = ((FinalizableObject *)objectAddress);

            finalizableObject->Finalize(true);
            finalizableObject->Dispose(true);
#ifdef RECYCLER_FINALIZE_CHECK
            this->heapBucket->heapInfo->liveFinalizableObjectCount --;
#endif
            DebugOnly(processedCount++);
        });

        this->ForEachPendingDisposeObject([&] (uint index) {
            void * objectAddress = this->address + (this->objectSize * index);
            ((FinalizableObject *)objectAddress)->Dispose(true);
#ifdef RECYCLER_FINALIZE_CHECK
            this->heapBucket->heapInfo->liveFinalizableObjectCount--;
            this->heapBucket->heapInfo->pendingDisposableObjectCount--;
#endif
            DebugOnly(processedCount++);
        });

        Assert(this->finalizeCount == processedCount);
    }
}

#if DBG
template <class TBlockAttributes>
void
SmallFinalizableHeapBlockT<TBlockAttributes>::Init(ushort objectSize, ushort objectCount)
{TRACE_IT(26872);
    Assert(this->disposedObjectList == nullptr);
    Assert(this->disposedObjectListTail == nullptr);
    Assert(this->finalizeCount == 0);
    Assert(this->pendingDisposeCount == 0);
    __super::Init(objectSize, objectCount);
}

#if ENABLE_PARTIAL_GC
template <class TBlockAttributes>
void
SmallFinalizableHeapBlockT<TBlockAttributes>::FinishPartialCollect()
{TRACE_IT(26873);
    Assert(this->disposedObjectList == nullptr);
    Assert(this->disposedObjectListTail == nullptr);
    __super::FinishPartialCollect();
}
#endif
#endif

#ifdef RECYCLER_SLOW_CHECK_ENABLED
template <class TBlockAttributes>
uint
SmallFinalizableHeapBlockT<TBlockAttributes>::CheckDisposedObjectFreeBitVector()
{TRACE_IT(26874);
    uint verifyFreeCount = 0;
    // all the finalized object are considered freed, but not allocable yet
    FreeObject *freeObject = this->disposedObjectList;
    if (freeObject != nullptr)
    {TRACE_IT(26875);
        SmallHeapBlockBitVector * free = this->GetFreeBitVector();
        while (true)
        {TRACE_IT(26876);
            uint bitIndex = this->GetAddressBitIndex(freeObject);
            Assert(this->IsValidBitIndex(bitIndex));
            Assert(!this->GetDebugFreeBitVector()->Test(bitIndex));
            Assert(free->Test(bitIndex));
            verifyFreeCount++;

            if (freeObject == this->disposedObjectListTail)
            {TRACE_IT(26877);
                break;
            }
            freeObject = freeObject->GetNext();
        }
    }
    return verifyFreeCount;
}

template <class TBlockAttributes>
bool
SmallFinalizableHeapBlockT<TBlockAttributes>::GetFreeObjectListOnAllocator(FreeObject ** freeObjectList)
{TRACE_IT(26878);
    return this->template GetFreeObjectListOnAllocatorImpl<SmallFinalizableHeapBlockT<TBlockAttributes>>(freeObjectList);
}

#endif

namespace Memory
{
    template class SmallFinalizableHeapBlockT<SmallAllocationBlockAttributes>;
    template void SmallFinalizableHeapBlockT<SmallAllocationBlockAttributes>::ProcessMarkedObject<true>(void* objectAddress, MarkContext * markContext);
    template void SmallFinalizableHeapBlockT<SmallAllocationBlockAttributes>::ProcessMarkedObject<false>(void* objectAddress, MarkContext * markContext);
    template class SmallFinalizableHeapBlockT<MediumAllocationBlockAttributes>;
    template void SmallFinalizableHeapBlockT<MediumAllocationBlockAttributes>::ProcessMarkedObject<true>(void* objectAddress, MarkContext * markContext);;
    template void SmallFinalizableHeapBlockT<MediumAllocationBlockAttributes>::ProcessMarkedObject<false>(void* objectAddress, MarkContext * markContext);;

#ifdef RECYCLER_WRITE_BARRIER
    template class SmallFinalizableWithBarrierHeapBlockT<SmallAllocationBlockAttributes>;
    template class SmallFinalizableWithBarrierHeapBlockT<MediumAllocationBlockAttributes>;
#endif
}
