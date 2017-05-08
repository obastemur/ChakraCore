//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
template <typename T>
class PageStack
{
private:
    struct Chunk : public PagePoolPage
    {
        Chunk * nextChunk;
        T entries[];
    };

    static const size_t EntriesPerChunk = (AutoSystemInfo::PageSize - sizeof(Chunk)) / sizeof(T);

public:
    PageStack(PagePool * pagePool);
    ~PageStack();

    void Init(uint reservedPageCount = 0);
    void Clear();

    bool Pop(T * item);
    bool Push(T item);

    uint Split(uint targetCount, __in_ecount(targetCount) PageStack<T> ** targetStacks);

    void Abort();
    void Release();

    bool IsEmpty() const;
#if DBG
    bool HasChunk() const
    {TRACE_IT(21931);
        return this->currentChunk != nullptr;
    }
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void SetMaxPageCount(size_t maxPageCount)
    {TRACE_IT(21932);
        this->maxPageCount = maxPageCount > 1 ? maxPageCount : 1;
    }
#endif

    static const uint MaxSplitTargets = 3;     // Not counting original stack, so this supports 4-way parallel

private:
    Chunk * CreateChunk();
    void FreeChunk(Chunk * chunk);

private:
    T * nextEntry;
    T * chunkStart;
    T * chunkEnd;
    Chunk * currentChunk;
    PagePool * pagePool;
    bool usesReservedPages;

#if DBG
    size_t count;
#endif
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    size_t pageCount;
    size_t maxPageCount;
#endif
};


template <typename T>
inline
bool PageStack<T>::Pop(T * item)
{TRACE_IT(21933);
    Assert(currentChunk != nullptr);

    if (nextEntry == chunkStart)
    {TRACE_IT(21934);
        // We're at the beginning of the chunk.  Move to the previous chunk, if any
        if (currentChunk->nextChunk == nullptr)
        {TRACE_IT(21935);
            // All done
            Assert(count == 0);
            return false;
        }

        Chunk * temp = currentChunk;
        currentChunk = currentChunk->nextChunk;
        FreeChunk(temp);

        chunkStart = currentChunk->entries;
        chunkEnd = &currentChunk->entries[EntriesPerChunk];
        nextEntry = chunkEnd;
    }

    Assert(nextEntry > chunkStart && nextEntry <= chunkEnd);

    nextEntry--;
    *item = *nextEntry;

#if DBG
    count--;
    Assert(count == (nextEntry - chunkStart) + (pageCount - 1) * EntriesPerChunk);
#endif

    return true;
}

template <typename T>
inline
bool PageStack<T>::Push(T item)
{TRACE_IT(21936);
    if (nextEntry == chunkEnd)
    {TRACE_IT(21937);
        Chunk * newChunk = CreateChunk();
        if (newChunk == nullptr)
        {TRACE_IT(21938);
            return false;
        }

        newChunk->nextChunk = currentChunk;
        currentChunk = newChunk;

        chunkStart = currentChunk->entries;
        chunkEnd = &currentChunk->entries[EntriesPerChunk];
        nextEntry = chunkStart;
    }

    Assert(nextEntry >= chunkStart && nextEntry < chunkEnd);

    *nextEntry = item;
    nextEntry++;

#if DBG
    count++;
    Assert(count == (nextEntry - chunkStart) + (pageCount - 1) * EntriesPerChunk);
#endif

    return true;
}


template <typename T>
PageStack<T>::PageStack(PagePool * pagePool) :
    pagePool(pagePool),
    currentChunk(nullptr),
    nextEntry(nullptr),
    chunkStart(nullptr),
    chunkEnd(nullptr),
    usesReservedPages(false)
{TRACE_IT(21939);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    pageCount = 0;
    maxPageCount = (size_t)-1;  // Default to no limit
#endif

#if DBG
    count = 0;
#endif
}


template <typename T>
PageStack<T>::~PageStack()
{TRACE_IT(21940);
    Assert(currentChunk == nullptr);
    Assert(nextEntry == nullptr);
    Assert(count == 0);
    Assert(pageCount == 0);
}


template <typename T>
void PageStack<T>::Init(uint reservedPageCount)
{TRACE_IT(21941);
    if (reservedPageCount > 0)
    {TRACE_IT(21942);
        this->usesReservedPages = true;
        this->pagePool->ReservePages(reservedPageCount);
    }

    // Preallocate one chunk.
    Assert(currentChunk == nullptr);
    currentChunk = CreateChunk();
    if (currentChunk == nullptr)
    {TRACE_IT(21943);
        Js::Throw::OutOfMemory();
    }
    currentChunk->nextChunk = nullptr;
    chunkStart = currentChunk->entries;
    chunkEnd = &currentChunk->entries[EntriesPerChunk];
    nextEntry = chunkStart;
}


template <typename T>
void PageStack<T>::Clear()
{TRACE_IT(21944);
    currentChunk = nullptr;
    nextEntry = nullptr;
#if DBG
    count = 0;
    pageCount = 0;
#endif
}


template <typename T>
typename PageStack<T>::Chunk * PageStack<T>::CreateChunk()
{TRACE_IT(21945);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (pageCount >= maxPageCount)
    {TRACE_IT(21946);
        return nullptr;
    }
#endif
    Chunk * newChunk = (Chunk *)this->pagePool->GetPage(usesReservedPages);

    if (newChunk == nullptr)
    {TRACE_IT(21947);
        return nullptr;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    pageCount++;
#endif
    return newChunk;
}


template <typename T>
void PageStack<T>::FreeChunk(Chunk * chunk)
{TRACE_IT(21948);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    pageCount--;
#endif
    this->pagePool->FreePage(chunk);
}


template <typename T>
uint PageStack<T>::Split(uint targetCount, __in_ecount(targetCount) PageStack<T> ** targetStacks)
{TRACE_IT(21949);
    // Split the current stack up to [targetCount + 1] ways.
    // [targetStacks] contains the target stacks and must have [targetCount] elements.

    Assert(targetCount > 0 && targetCount <= MaxSplitTargets);
    Assert(targetStacks);
    __analysis_assume(targetCount <= MaxSplitTargets);

    Chunk * mainCurrent;
    Chunk * targetCurrents[MaxSplitTargets];

    // Do the initial split of first pages for each target stack.
    // During this, if we run out of pages, we will return a value < maxSplit to
    // indicate that the split was less than the maximum possible.

    Chunk * chunk = this->currentChunk;
    Assert(chunk != nullptr);

    // The first chunk is assigned to the main stack, and since it's already there,
    // we just advance to the next chunk and start assigning to each target stack.
    mainCurrent = chunk;
    chunk = chunk->nextChunk;

    uint targetIndex = 0;
    while (targetIndex < targetCount)
    {TRACE_IT(21950);
        if (chunk == nullptr)
        {TRACE_IT(21951);
            // No more pages.  Adjust targetCount down to what we were actually able to do.
            // We'll return this number below so the caller knows.
            targetCount = targetIndex;
            break;
        }

        // Target stack should be empty.
        // If it has a free page currently, release it.
        Assert(targetStacks[targetIndex]->IsEmpty());
        targetStacks[targetIndex]->Release();

        targetStacks[targetIndex]->currentChunk = chunk;
        targetStacks[targetIndex]->chunkStart = chunk->entries;
        targetStacks[targetIndex]->chunkEnd = &chunk->entries[EntriesPerChunk];
        targetStacks[targetIndex]->nextEntry = targetStacks[targetIndex]->chunkEnd;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        this->pageCount--;
        targetStacks[targetIndex]->pageCount = 1;
#endif
#if DBG
        this->count -= EntriesPerChunk;
        targetStacks[targetIndex]->count = EntriesPerChunk;
#endif

        targetCurrents[targetIndex] = chunk;

        chunk = chunk->nextChunk;
        targetIndex++;
    }

    // Loop through the remaining chunks (if any),
    // assigning each chunk to the main chunk and the target chunks in turn,
    // and linking each chunk to the end of the respective list.
    while (true)
    {TRACE_IT(21952);
        if (chunk == nullptr)
        {TRACE_IT(21953);
            break;
        }

        mainCurrent->nextChunk = chunk;
        mainCurrent = chunk;

        chunk = chunk->nextChunk;

        targetIndex = 0;
        while (targetIndex < targetCount)
        {TRACE_IT(21954);
            if (chunk == nullptr)
            {TRACE_IT(21955);
                break;
            }

            targetCurrents[targetIndex]->nextChunk = chunk;
            targetCurrents[targetIndex] = chunk;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
            this->pageCount--;
            targetStacks[targetIndex]->pageCount++;
#endif
#if DBG
            this->count -= EntriesPerChunk;
            targetStacks[targetIndex]->count += EntriesPerChunk;
#endif

            chunk = chunk->nextChunk;
            targetIndex++;
        }
    }

    // Terminate all the split chunk lists with null
    mainCurrent->nextChunk = nullptr;
    targetIndex = 0;
    while (targetIndex < targetCount)
    {TRACE_IT(21956);
        targetCurrents[targetIndex]->nextChunk = nullptr;
        targetIndex++;
    }

    // Return the actual split count we were able to do, which may have been lowered above.
    return targetCount;
}


template <typename T>
void PageStack<T>::Abort()
{TRACE_IT(21957);
    // Abandon the current entries in the stack and reset to initialized state.

    if (currentChunk == nullptr)
    {TRACE_IT(21958);
        Assert(count == 0);
        return;
    }

    // Free all the chunks except the first one
    while (currentChunk->nextChunk != nullptr)
    {TRACE_IT(21959);
        Chunk * temp = currentChunk;
        currentChunk = currentChunk->nextChunk;
        FreeChunk(temp);
    }

    chunkStart = currentChunk->entries;
    chunkEnd = &currentChunk->entries[EntriesPerChunk];
    nextEntry = chunkStart;

#if DBG
    count = 0;
#endif
}


template <typename T>
void PageStack<T>::Release()
{TRACE_IT(21960);
    Assert(IsEmpty());

    // We may have a preallocated chunk still held; if so release it.
    if (currentChunk != nullptr)
    {TRACE_IT(21961);
        Assert(currentChunk->nextChunk == nullptr);
        FreeChunk(currentChunk);
        currentChunk = nullptr;
    }

    nextEntry = nullptr;
    chunkStart = nullptr;
    chunkEnd = nullptr;
}


template <typename T>
bool PageStack<T>::IsEmpty() const
{TRACE_IT(21962);
    if (currentChunk == nullptr)
    {TRACE_IT(21963);
        Assert(count == 0);
        Assert(nextEntry == nullptr);
        return true;
    }

    if (nextEntry == chunkStart && currentChunk->nextChunk == nullptr)
    {TRACE_IT(21964);
        Assert(count == 0);
        return true;
    }

    Assert(count != 0);
    return false;
}

