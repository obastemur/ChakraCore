//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
namespace Memory
{
template <typename TBlockType>
class SmallHeapBlockAllocator
{
public:
    typedef TBlockType BlockType;

    SmallHeapBlockAllocator();
    void Initialize();

    template <ObjectInfoBits attributes>
    inline char * InlinedAlloc(Recycler * recycler, DECLSPEC_GUARD_OVERFLOW size_t sizeCat);

    // Pass through template parameter to InlinedAllocImpl
    template <bool canFaultInject>
    inline char * SlowAlloc(Recycler * recycler, DECLSPEC_GUARD_OVERFLOW size_t sizeCat, ObjectInfoBits attributes);

    // There are paths where we simply can't OOM here, so we shouldn't fault inject as it creates a bit of a mess
    template <bool canFaultInject>
    inline char* InlinedAllocImpl(Recycler * recycler, DECLSPEC_GUARD_OVERFLOW size_t sizeCat, ObjectInfoBits attributes);

    TBlockType * GetHeapBlock() const {TRACE_IT(26948); return heapBlock; }
    SmallHeapBlockAllocator * GetNext() const {TRACE_IT(26949); return next; }

    void Set(TBlockType * heapBlock);
    void SetNew(TBlockType * heapBlock);
    void Clear();
    void UpdateHeapBlock();
    void SetExplicitFreeList(FreeObject* list);

    static uint32 GetEndAddressOffset() {TRACE_IT(26950); return offsetof(SmallHeapBlockAllocator, endAddress); }
    char *GetEndAddress() {TRACE_IT(26951); return endAddress; }
    static uint32 GetFreeObjectListOffset() {TRACE_IT(26952); return offsetof(SmallHeapBlockAllocator, freeObjectList); }
    FreeObject *GetFreeObjectList() {TRACE_IT(26953); return freeObjectList; }
    void SetFreeObjectList(FreeObject *freeObject) {TRACE_IT(26954); freeObjectList = freeObject; }

#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(ETW_MEMORY_TRACKING)
    void SetTrackNativeAllocatedObjectCallBack(void (*pfnCallBack)(Recycler *, void *, size_t))
    {TRACE_IT(26955);
        pfnTrackNativeAllocatedObjectCallBack = pfnCallBack;
    }
#endif
#if DBG
    FreeObject * GetExplicitFreeList() const
    {TRACE_IT(26956);
        Assert(IsExplicitFreeObjectListAllocMode());
        return this->freeObjectList;
    }
#endif

    bool IsBumpAllocMode() const
    {TRACE_IT(26957);
        return endAddress != nullptr;
    }
    bool IsExplicitFreeObjectListAllocMode() const
    {TRACE_IT(26958);
        return this->heapBlock == nullptr;
    }
    bool IsFreeListAllocMode() const
    {TRACE_IT(26959);
        return !IsBumpAllocMode() && !IsExplicitFreeObjectListAllocMode();
    }
private:
    static bool NeedSetAttributes(ObjectInfoBits attributes)
    {TRACE_IT(26960);
        return attributes != LeafBit && (attributes & InternalObjectInfoBitMask) != 0;
    }

    char * endAddress;
    FreeObject * freeObjectList;
    TBlockType * heapBlock;

    SmallHeapBlockAllocator * prev;
    SmallHeapBlockAllocator * next;

    friend class HeapBucketT<BlockType>;
#ifdef RECYCLER_SLOW_CHECK_ENABLED
    template <class TBlockAttributes>
    friend class SmallHeapBlockT;
#endif
#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY)
    HeapBucket * bucket;
#endif

#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
    char * lastNonNativeBumpAllocatedBlock;
    void TrackNativeAllocatedObjects();
#endif
#if defined(PROFILE_RECYCLER_ALLOC) || defined(RECYCLER_MEMORY_VERIFY) || defined(MEMSPECT_TRACKING) || defined(ETW_MEMORY_TRACKING)
    void (*pfnTrackNativeAllocatedObjectCallBack)(Recycler * recycler, void *, size_t sizeCat);
#endif
};

template <typename TBlockType>
template <bool canFaultInject>
inline char*
SmallHeapBlockAllocator<TBlockType>::InlinedAllocImpl(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes)
{TRACE_IT(26961);
    Assert((attributes & InternalObjectInfoBitMask) == attributes);
#ifdef RECYCLER_WRITE_BARRIER
    Assert(!CONFIG_FLAG(ForceSoftwareWriteBarrier) || (attributes & WithBarrierBit) || (attributes & LeafBit));
#endif

    AUTO_NO_EXCEPTION_REGION;
    if (canFaultInject)
    {TRACE_IT(26962);
        FAULTINJECT_MEMORY_NOTHROW(_u("InlinedAllocImpl"), sizeCat);
    }

    char * memBlock = (char *)freeObjectList;
    char * nextCurrentAddress = memBlock + sizeCat;
    char * endAddress = this->endAddress;

    if (nextCurrentAddress <= endAddress)
    {TRACE_IT(26963);
        // Bump Allocation
        Assert(this->IsBumpAllocMode());
#ifdef RECYCLER_TRACK_NATIVE_ALLOCATED_OBJECTS
        TrackNativeAllocatedObjects();
        lastNonNativeBumpAllocatedBlock = memBlock;
#endif
        freeObjectList = (FreeObject *)nextCurrentAddress;

        if (NeedSetAttributes(attributes))
        {TRACE_IT(26964);
            heapBlock->SetAttributes(memBlock, (attributes & StoredObjectInfoBitMask));
        }

        return memBlock;
    }

    if (memBlock != nullptr && endAddress == nullptr)
    {TRACE_IT(26965);
        // Free list allocation
        Assert(!this->IsBumpAllocMode());
        if (NeedSetAttributes(attributes))
        {TRACE_IT(26966);
            TBlockType * allocationHeapBlock = this->heapBlock;
            if (allocationHeapBlock == nullptr)
            {TRACE_IT(26967);
                Assert(this->IsExplicitFreeObjectListAllocMode());
                allocationHeapBlock = (TBlockType *)recycler->FindHeapBlock(memBlock);
                Assert(allocationHeapBlock != nullptr);
                Assert(!allocationHeapBlock->IsLargeHeapBlock());
            }
            allocationHeapBlock->SetAttributes(memBlock, (attributes & StoredObjectInfoBitMask));
        }
        freeObjectList = ((FreeObject *)memBlock)->GetNext();

#ifdef RECYCLER_MEMORY_VERIFY
        ((FreeObject *)memBlock)->DebugFillNext();

        if (this->IsExplicitFreeObjectListAllocMode())
        {TRACE_IT(26968);
            HeapBlock* heapBlock = recycler->FindHeapBlock(memBlock);
            Assert(heapBlock != nullptr);
            Assert(!heapBlock->IsLargeHeapBlock());
            TBlockType* smallBlock = (TBlockType*)heapBlock;
            smallBlock->ClearExplicitFreeBitForObject(memBlock);
        }
#endif

#if DBG || defined(RECYCLER_STATS)
        if (!IsExplicitFreeObjectListAllocMode())
        {TRACE_IT(26969);
            BOOL isSet = heapBlock->GetDebugFreeBitVector()->TestAndClear(heapBlock->GetAddressBitIndex(memBlock));
            Assert(isSet);
        }
#endif
        return memBlock;
    }

    return nullptr;
}


template <typename TBlockType>
template <ObjectInfoBits attributes>
inline char *
SmallHeapBlockAllocator<TBlockType>::InlinedAlloc(Recycler * recycler, size_t sizeCat)
{TRACE_IT(26970);
    return InlinedAllocImpl<true /* allow fault injection */>(recycler, sizeCat, attributes);
}

template <typename TBlockType>
template <bool canFaultInject>
inline
char *
SmallHeapBlockAllocator<TBlockType>::SlowAlloc(Recycler * recycler, size_t sizeCat, ObjectInfoBits attributes)
{TRACE_IT(26971);
    Assert((attributes & InternalObjectInfoBitMask) == attributes);

    return InlinedAllocImpl<canFaultInject>(recycler, sizeCat, attributes);
}
}
