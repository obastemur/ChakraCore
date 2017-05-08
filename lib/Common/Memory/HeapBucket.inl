//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

template <typename TBlockType>
template <ObjectInfoBits attributes, bool nothrow>

inline char *
HeapBucketT<TBlockType>::RealAlloc(Recycler * recycler, size_t sizeCat, size_t size)
{TRACE_IT(24057);
    Assert(sizeCat == this->sizeCat);

    char * memBlock = allocatorHead.template InlinedAlloc<(ObjectInfoBits)(attributes & InternalObjectInfoBitMask)>(recycler, sizeCat);

    if (memBlock == nullptr)
    {TRACE_IT(24058);
        memBlock = SnailAlloc(recycler, &allocatorHead, sizeCat, size, attributes, nothrow);
        Assert(memBlock != nullptr || nothrow);
    }

    // If this API is called and throwing is not allowed,
    // check if we actually allocated a block before verifying
    // its zero fill state. If it is nullptr, return that here.
    if (nothrow)
    {TRACE_IT(24059);
        if (memBlock == nullptr)
        {TRACE_IT(24060);
            return nullptr;
        }
    }

#ifdef RECYCLER_ZERO_MEM_CHECK
    // Do the verify zero fill only if it's not a nothrow alloc
    if ((attributes & ObjectInfoBits::LeafBit) == 0
#ifdef RECYCLER_WRITE_BARRIER_ALLOC_THREAD_PAGE
        && ((attributes & ObjectInfoBits::WithBarrierBit) == 0)
#endif
        )
    {TRACE_IT(24061);
        if (this->IsPageHeapEnabled(attributes))
        {TRACE_IT(24062);
            // in page heap there's no free list, and using size instead of sizeCat
            recycler->VerifyZeroFill(memBlock, size);
        }
        else
        {TRACE_IT(24063);
            // Skip the first and the last pointer objects- the first may have next pointer for the free list
            // the last might have the old size of the object if this was allocated from an explicit free list
            recycler->VerifyZeroFill(memBlock + sizeof(FreeObject), sizeCat - (2 * sizeof(FreeObject)));
        }
    }
#endif

    return memBlock;
}

template <typename TBlockType>
void
HeapBucketT<TBlockType>::ExplicitFree(void* object, size_t sizeCat)
{TRACE_IT(24064);
    FreeObject* explicitFreeObject = (FreeObject*) object;
    if (lastExplicitFreeListAllocator->IsExplicitFreeObjectListAllocMode())
    {TRACE_IT(24065);
        explicitFreeObject->SetNext(lastExplicitFreeListAllocator->GetFreeObjectList());
        lastExplicitFreeListAllocator->SetFreeObjectList(explicitFreeObject);
    }
    else
    {TRACE_IT(24066);
        explicitFreeObject->SetNext(this->explicitFreeList);
        this->explicitFreeList = explicitFreeObject;
    }

    // Don't fill memory fill pattern here since we're still pretending like the object
    // is allocated to other parts of the GC.
}

#if DBG || defined(RECYCLER_SLOW_CHECK_ENABLED)
inline
Recycler *
HeapBucket::GetRecycler() const
{TRACE_IT(24067);
    return this->heapInfo->recycler;
}
#endif
