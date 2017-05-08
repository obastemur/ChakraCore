//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

template <class TBlockAttributes>
void
SmallHeapBlockT<TBlockAttributes>::SetAttributes(void * address, unsigned char attributes)
{TRACE_IT(23636);
    Assert(this->address != nullptr);
    Assert(this->segment != nullptr);
    ushort index = GetAddressIndex(address);
    Assert(this->ObjectInfo(index) == 0);
    Assert(index != SmallHeapBlockT<TBlockAttributes>::InvalidAddressBit);
    ObjectInfo(index) = attributes;
}

inline
IdleDecommitPageAllocator*
HeapBlock::GetPageAllocator(Recycler* recycler)
{TRACE_IT(23637);
    switch (this->GetHeapBlockType())
    {
    case SmallLeafBlockType:
    case MediumLeafBlockType:
        return recycler->GetRecyclerLeafPageAllocator();
    case LargeBlockType:
        return recycler->GetRecyclerLargeBlockPageAllocator();
#ifdef RECYCLER_WRITE_BARRIER
    case SmallNormalBlockWithBarrierType:
    case SmallFinalizableBlockWithBarrierType:
    case MediumNormalBlockWithBarrierType:
    case MediumFinalizableBlockWithBarrierType:
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_THREAD_PAGE
        return recycler->GetRecyclerLeafPageAllocator();
#elif defined(RECYCLER_WRITE_BARRIER_ALLOC_SEPARATE_PAGE)
        return recycler->GetRecyclerWithBarrierPageAllocator();
#endif
#endif

    default:
        return recycler->GetRecyclerPageAllocator();
    };
}

template <class TBlockAttributes>
template <class Fn>
void SmallHeapBlockT<TBlockAttributes>::ForEachAllocatedObject(Fn fn)
{TRACE_IT(23638);
    uint const objectBitDelta = this->GetObjectBitDelta();
    SmallHeapBlockBitVector * free = this->EnsureFreeBitVector();
    char * address = this->GetAddress();
    uint objectSize = this->GetObjectSize();
    for (uint i = 0; i < objectCount; i++)
    {TRACE_IT(23639);
        if (!free->Test(i * objectBitDelta))
        {
            fn(i, address + i * objectSize);
        }
    }
}

template <class TBlockAttributes>
template <typename Fn>
void SmallHeapBlockT<TBlockAttributes>::ForEachAllocatedObject(ObjectInfoBits attributes, Fn fn)
{TRACE_IT(23640);
    ForEachAllocatedObject([=](uint index, void * objectAddress)
    {
        if ((ObjectInfo(index) & attributes) != 0)
        {
            fn(index, objectAddress);
        }
    });
};

template <class TBlockAttributes>
template <typename Fn>
void SmallHeapBlockT<TBlockAttributes>::ScanNewImplicitRootsBase(Fn fn)
{TRACE_IT(23641);
    uint const localObjectCount = this->objectCount;

    // NOTE: we no longer track the mark count as we mark.  So this value
    // is basically the mark count we set during the initial implicit root scan
    // plus any subsequent new implicit root scan.
    uint localMarkCount = this->markCount;
    if (localMarkCount == localObjectCount)
    {TRACE_IT(23642);
        // The block is full when we first do the initial implicit root scan
        // So there can't be any new implicit roots
        return;
    }

#if DBG
    HeapBlockMap& map = this->GetRecycler()->heapBlockMap;
    ushort newlyMarkedCountForPage[TBlockAttributes::PageCount];
    for (uint i = 0; i < TBlockAttributes::PageCount; i++)
    {TRACE_IT(23643);
        newlyMarkedCountForPage[i] = 0;
    }
#endif

    uint const localObjectBitDelta = this->GetObjectBitDelta();
    uint const localObjectSize = this->GetObjectSize();
    Assert(localObjectSize <= HeapConstants::MaxMediumObjectSize);
    SmallHeapBlockBitVector * mark = this->GetMarkedBitVector();
    char * address = this->GetAddress();

    for (uint i = 0; i < localObjectCount; i++)
    {TRACE_IT(23644);
        if ((this->ObjectInfo(i) & ImplicitRootBit) != 0
            && !mark->TestAndSet(i * localObjectBitDelta))
        {TRACE_IT(23645);
            uint objectOffset = i * localObjectSize;
            localMarkCount++;
#if DBG
            uint pageNumber = objectOffset / AutoSystemInfo::PageSize;
            Assert(pageNumber < TBlockAttributes::PageCount);
            newlyMarkedCountForPage[pageNumber]++;
#endif

            fn(address + objectOffset, localObjectSize);
        }
    }
    Assert(localMarkCount <= USHRT_MAX);
#if DBG
    // Add newly marked count
    for (uint i = 0; i < TBlockAttributes::PageCount; i++)
    {TRACE_IT(23646);
        char* pageAddress = address + (AutoSystemInfo::PageSize * i);
        ushort oldPageMarkCount = map.GetPageMarkCount(pageAddress);
        map.SetPageMarkCount(pageAddress, oldPageMarkCount + newlyMarkedCountForPage[i]);
    }

#endif
    this->markCount = (ushort)localMarkCount;
}

template <class TBlockAttributes>
bool
SmallHeapBlockT<TBlockAttributes>::FindImplicitRootObject(void* candidate, Recycler* recycler, RecyclerHeapObjectInfo& heapObject)
{TRACE_IT(23647);
    ushort index = GetAddressIndex(candidate);

    if (index == InvalidAddressBit)
    {TRACE_IT(23648);
        return false;
    }

    byte& attributes = ObjectInfo(index);
    heapObject = RecyclerHeapObjectInfo(candidate, recycler, this, &attributes);
    return true;
}

template <bool doSpecialMark, typename Fn>
bool
HeapBlock::UpdateAttributesOfMarkedObjects(MarkContext * markContext, void * objectAddress, size_t objectSize, unsigned char attributes, Fn fn)
{TRACE_IT(23649);
    bool noOOMDuringMark = true;

    if (attributes & TrackBit)
    {TRACE_IT(23650);
        FinalizableObject * trackedObject = (FinalizableObject *)objectAddress;

#if ENABLE_PARTIAL_GC
        if (!markContext->GetRecycler()->inPartialCollectMode)
#endif
        {TRACE_IT(23651);
#if ENABLE_CONCURRENT_GC
            if (markContext->GetRecycler()->DoQueueTrackedObject())
            {TRACE_IT(23652);
                if (!markContext->AddTrackedObject(trackedObject))
                {TRACE_IT(23653);
                    noOOMDuringMark = false;
                }
            }
            else
#endif
            {TRACE_IT(23654);
                // Process the tracked object right now
                markContext->MarkTrackedObject(trackedObject);
            }
        }

        if (noOOMDuringMark)
        {TRACE_IT(23655);
            // Object has been successfully processed, so clear NewTrackBit
            attributes &= ~NewTrackBit;
        }
        else
        {TRACE_IT(23656);
            // Set the NewTrackBit, so that the main thread will redo tracking
            attributes |= NewTrackBit;
            noOOMDuringMark = false;
        }
        fn(attributes);
    }

    // only need to scan non-leaf objects
    if ((attributes & LeafBit) == 0)
    {TRACE_IT(23657);
        if (!markContext->AddMarkedObject(objectAddress, objectSize))
        {TRACE_IT(23658);
            noOOMDuringMark = false;
        }
    }

    // Special mark-time behavior for finalizable objects on certain GC's
    if (doSpecialMark)
    {TRACE_IT(23659);
        if (attributes & FinalizeBit)
        {TRACE_IT(23660);
            FinalizableObject * trackedObject = (FinalizableObject *)objectAddress;
            trackedObject->OnMark();
        }
    }        

#ifdef RECYCLER_STATS
    RECYCLER_STATS_INTERLOCKED_INC(markContext->GetRecycler(), markData.markCount);
    RECYCLER_STATS_INTERLOCKED_ADD(markContext->GetRecycler(), markData.markBytes, objectSize);

    // Don't count track or finalize it if we still have to process it in thread because of OOM
    if ((attributes & (TrackBit | NewTrackBit)) != (TrackBit | NewTrackBit))
    {TRACE_IT(23661);
        // Only count those we have queued, so we don't double count
        if (attributes & TrackBit)
        {TRACE_IT(23662);
            RECYCLER_STATS_INTERLOCKED_INC(markContext->GetRecycler(), trackCount);
        }
        if (attributes & FinalizeBit)
        {TRACE_IT(23663);
            // we counted the finalizable object here,
            // turn off the new bit so we don't count it again
            // on Rescan
            attributes &= ~NewFinalizeBit;
            fn(attributes);
            RECYCLER_STATS_INTERLOCKED_INC(markContext->GetRecycler(), finalizeCount);
        }
    }
#endif

    return noOOMDuringMark;
}
