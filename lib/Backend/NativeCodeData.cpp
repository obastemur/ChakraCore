//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

NativeCodeData::NativeCodeData(DataChunk * chunkList)
    : chunkList(chunkList)
{TRACE_IT(13725);
#ifdef PERF_COUNTERS
    this->size = 0;
#endif
}

NativeCodeData::~NativeCodeData()
{TRACE_IT(13726);
    if (JITManager::GetJITManager()->IsJITServer())
    {TRACE_IT(13727);
        NativeCodeData::DeleteChunkList(this->chunkList);
    }
    else
    {TRACE_IT(13728);
        NativeCodeData::DeleteChunkList(this->noFixupChunkList);
    }
    PERF_COUNTER_SUB(Code, DynamicNativeCodeDataSize, this->size);
    PERF_COUNTER_SUB(Code, TotalNativeCodeDataSize, this->size);
}

void
NativeCodeData::AddFixupEntry(void* targetAddr, void* addrToFixup, void* startAddress, DataChunk * chunkList)
{TRACE_IT(13729);
    return NativeCodeData::AddFixupEntry(targetAddr, targetAddr, addrToFixup, startAddress, chunkList);
}

// targetAddr: target address
// targetStartAddr: target start address, some fied might reference to middle of another data chunk, like outParamOffsets
// startAddress: current data start address
// addrToFixup: address that currently pointing to dataAddr, which need to be updated
void
NativeCodeData::AddFixupEntry(void* targetAddr, void* targetStartAddr, void* addrToFixup, void* startAddress, DataChunk * chunkList)
{TRACE_IT(13730);
    Assert(addrToFixup >= startAddress);
    Assert(((__int64)addrToFixup) % sizeof(void*) == 0);

    if (targetAddr == nullptr)
    {TRACE_IT(13731);
        return;
    }

    Assert(targetStartAddr);

    unsigned int inDataOffset = (unsigned int)((char*)targetAddr - (char*)targetStartAddr);
    DataChunk* targetChunk = NativeCodeData::GetDataChunk(targetStartAddr);
    Assert(targetChunk->len >= inDataOffset);

#if DBG
    bool foundTargetChunk = false;
    while (chunkList)
    {TRACE_IT(13732);
        foundTargetChunk |= (chunkList == targetChunk);
        chunkList = chunkList->next;
    }
    AssertMsg(foundTargetChunk, "current pointer is not allocated with NativeCodeData allocator?"); // change to valid check instead of assertion?
#endif

    DataChunk* chunk = NativeCodeData::GetDataChunk(startAddress);

    NativeDataFixupEntry* entry = (NativeDataFixupEntry*)midl_user_allocate(sizeof(NativeDataFixupEntry));
    if (!entry)
    {TRACE_IT(13733);
        Js::Throw::OutOfMemory();
    }
    __analysis_assume(entry);
    entry->addrOffset = (unsigned int)((__int64)addrToFixup - (__int64)startAddress);
    Assert(entry->addrOffset <= chunk->len - sizeof(void*));

    entry->targetTotalOffset = targetChunk->offset + inDataOffset;
    entry->next = chunk->fixupList;
    chunk->fixupList = entry;

#if DBG
    if (PHASE_TRACE1(Js::NativeCodeDataPhase))
    {TRACE_IT(13734);
        Output::Print(_u("NativeCodeData Add Fixup: %p(%p+%d, chunk:%p)  -->  %p(chunk:%p)  %S\n"),
            addrToFixup, startAddress, entry->addrOffset, (void*)chunk, targetAddr, (void*)targetChunk, chunk->dataType);
    }
#endif
}

void
NativeCodeData::AddFixupEntryForPointerArray(void* startAddress, DataChunk * chunkList)
{TRACE_IT(13735);
    DataChunk* chunk = NativeCodeData::GetDataChunk(startAddress);
    Assert(chunk->len % sizeof(void*) == 0);
    for (unsigned int i = 0; i < chunk->len / sizeof(void*); i++)
    {TRACE_IT(13736);
        size_t offset = i * sizeof(void*);
        void* targetAddr = *(void**)((char*)startAddress + offset);

        if (targetAddr == nullptr)
        {TRACE_IT(13737);
            continue;
        }

        DataChunk* targetChunk = NativeCodeData::GetDataChunk(targetAddr);

#if DBG
        bool foundTargetChunk = false;
        DataChunk* chunk1 = chunkList;
        while (chunk1 && !foundTargetChunk)
        {TRACE_IT(13738);
            foundTargetChunk = (chunk1 == targetChunk);
            chunk1 = chunk1->next;
        }
        AssertMsg(foundTargetChunk, "current pointer is not allocated with NativeCodeData allocator?"); // change to valid check instead of assertion?
#endif

        NativeDataFixupEntry* entry = (NativeDataFixupEntry*)midl_user_allocate(sizeof(NativeDataFixupEntry));
        if (!entry)
        {TRACE_IT(13739);
            Js::Throw::OutOfMemory();
        }
        __analysis_assume(entry);
        entry->addrOffset = (unsigned int)offset;
        entry->targetTotalOffset = targetChunk->offset;
        entry->next = chunk->fixupList;
        chunk->fixupList = entry;

#if DBG
        if (PHASE_TRACE1(Js::NativeCodeDataPhase))
        {TRACE_IT(13740);
            Output::Print(_u("NativeCodeData Add Fixup: %p[%d](+%d, chunk:%p)  -->  %p(chunk:%p)  %S\n"),
                startAddress, i, entry->addrOffset, (void*)chunk, targetAddr, (void*)targetChunk, chunk->dataType);
        }
#endif
    }
}

char16*
NativeCodeData::GetDataDescription(void* data, JitArenaAllocator * alloc)
{TRACE_IT(13741);
    auto chunk = GetDataChunk(data);
    char16 buf[1024] = { 0 };
#if DBG
    swprintf_s(buf, _u("%hs, NativeCodeData: index: %x, len: %x, offset: +%x"), chunk->dataType, chunk->allocIndex, chunk->len, chunk->offset);
#else
    swprintf_s(buf, _u("NativeCodeData: index: %x, len: %x, offset: +%x"), chunk->allocIndex, chunk->len, chunk->offset);
#endif
    auto len = wcslen(buf) + 1;
    auto desc = JitAnewArray(alloc, char16, len);
    wcscpy_s(desc, len, buf);
    return desc;
}

void
NativeCodeData::VerifyExistFixupEntry(void* targetAddr, void* addrToFixup, void* startAddress)
{TRACE_IT(13742);
    DataChunk* chunk = NativeCodeData::GetDataChunk(startAddress);
    DataChunk* targetChunk = NativeCodeData::GetDataChunk(targetAddr);
    if (chunk->len == 0)
    {TRACE_IT(13743);
        return;
    }
    unsigned int offset = (unsigned int)((char*)addrToFixup - (char*)startAddress);
    Assert(offset <= chunk->len);

    NativeDataFixupEntry* entry = chunk->fixupList;
    while (entry)
    {TRACE_IT(13744);
        if (entry->addrOffset == offset)
        {TRACE_IT(13745);
            // The following assertions can be false positive in case a data field happen to
            // have value fall into NativeCodeData memory range
            AssertMsg(entry->targetTotalOffset == targetChunk->offset, "Missing fixup");
            return;
        }
        entry = entry->next;
    }
    AssertMsg(false, "Data chunk not found");
}

template<class DataChunkT>
void
NativeCodeData::DeleteChunkList(DataChunkT * chunkList)
{TRACE_IT(13746);
    DataChunkT * next = chunkList;
    while (next != nullptr)
    {TRACE_IT(13747);
        DataChunkT * current = next;
        next = next->next;

        // TODO: Should be HeapDeletePlus, but we don't know plusSize
        HeapDelete(current, AllocatorDeleteFlags::UnknownSize);
    }
}

NativeCodeData::Allocator::Allocator()
    : chunkList(nullptr),
    lastChunkList(nullptr),
    isOOPJIT(JITManager::GetJITManager()->IsJITServer())
{TRACE_IT(13748);
    this->totalSize = 0;
    this->allocCount = 0;
#if DBG
    this->finalized = false;
#endif
#ifdef PERF_COUNTERS
    this->size = 0;
#endif
}

NativeCodeData::Allocator::~Allocator()
{TRACE_IT(13749);
    Assert(!finalized || this->chunkList == nullptr);
    if (JITManager::GetJITManager()->IsJITServer())
    {TRACE_IT(13750);
        NativeCodeData::DeleteChunkList(this->chunkList);
    }
    else
    {TRACE_IT(13751);
        NativeCodeData::DeleteChunkList(this->noFixupChunkList);
    }
    PERF_COUNTER_SUB(Code, DynamicNativeCodeDataSize, this->size);
    PERF_COUNTER_SUB(Code, TotalNativeCodeDataSize, this->size);
}

char *
NativeCodeData::Allocator::Alloc(DECLSPEC_GUARD_OVERFLOW size_t requestSize)
{TRACE_IT(13752);
    Assert(!finalized);
    char * data = nullptr;
    requestSize = Math::Align(requestSize, sizeof(void*));

    if (isOOPJIT)
    {TRACE_IT(13753);

#if DBG
        // Always zero out the data for chk build to reduce the chance of false
        // positive while verifying missing fixup entries
        // Allocation without zeroing out, and with bool field in the structure
        // will increase the chance of false positive because of reusing memory
        // without zeroing, and the bool field is set to false, makes the garbage
        // memory not changed, and the garbage memory might be just pointing to the
        // same range of NativeCodeData memory, the checking tool will report false
        // poisitive, see NativeCodeData::VerifyExistFixupEntry for more
        DataChunk * newChunk = HeapNewStructPlusZ(requestSize, DataChunk);
#else
        DataChunk * newChunk = HeapNewStructPlus(requestSize, DataChunk);
#endif

#if DBG
        newChunk->dataType = nullptr;
#endif

        newChunk->next = nullptr;
        newChunk->allocIndex = this->allocCount++;
        newChunk->len = (unsigned int)requestSize;
        newChunk->fixupList = nullptr;
        newChunk->fixupFunc = nullptr;
        newChunk->offset = this->totalSize;
        if (this->chunkList == nullptr)
        {TRACE_IT(13754);
            this->chunkList = newChunk;
            this->lastChunkList = newChunk;
        }
        else
        {TRACE_IT(13755);
            this->lastChunkList->next = newChunk;
            this->lastChunkList = newChunk;
        }
        this->totalSize += (unsigned int)requestSize;
        data = newChunk->data;
    }
    else
    {TRACE_IT(13756);
        DataChunkNoFixup * newChunk = HeapNewStructPlus(requestSize, DataChunkNoFixup);
        newChunk->next = this->noFixupChunkList;
        this->noFixupChunkList = newChunk;
        data = newChunk->data;
    }


#ifdef PERF_COUNTERS
    this->size += requestSize;
    PERF_COUNTER_ADD(Code, DynamicNativeCodeDataSize, requestSize);
#endif

    PERF_COUNTER_ADD(Code, TotalNativeCodeDataSize, requestSize);
    return data;
}

char *
NativeCodeData::Allocator::AllocLeaf(DECLSPEC_GUARD_OVERFLOW size_t requestSize)
{TRACE_IT(13757);
    return Alloc(requestSize);
}

char *
NativeCodeData::Allocator::AllocZero(DECLSPEC_GUARD_OVERFLOW size_t requestSize)
{TRACE_IT(13758);
    char * data = Alloc(requestSize);
#if !DBG
    // Allocated with HeapNewStructPlusZ for chk build
    memset(data, 0, requestSize);
#else
    if (!isOOPJIT)
    {
        memset(data, 0, requestSize);
    }
#endif
    return data;
}

NativeCodeData *
NativeCodeData::Allocator::Finalize()
{TRACE_IT(13759);
    NativeCodeData * data = nullptr;
    if (this->chunkList != nullptr)
    {TRACE_IT(13760);
        data = HeapNew(NativeCodeData, this->chunkList);
        this->chunkList = nullptr;
#ifdef PERF_COUNTERS
        data->size = this->size;
        this->size = 0;
#endif
    }
#if DBG
    this->finalized = true;
#endif
    return data;
}

//////////////////////////////////////////////////////////////////////////
//NativeCodeData::Allocator::Free
//This function should not be called at all because the life time is active during the run time
//This function is added to enable Dictionary(has calls to Free() Method - which will never be called as it will be
//allocated as a NativeAllocator to be allocated with NativeAllocator)
//////////////////////////////////////////////////////////////////////////
void
NativeCodeData::Allocator::Free(void * buffer, size_t byteSize)
{TRACE_IT(13761);
}
