//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

//----------------------------------------------------------------------------
// EmitBufferManager::EmitBufferManager
//      Constructor
//----------------------------------------------------------------------------
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::EmitBufferManager(ArenaAllocator * allocator, CustomHeap::CodePageAllocators<TAlloc, TPreReservedAlloc> * codePageAllocators,
    Js::ScriptContext * scriptContext, LPCWSTR name, HANDLE processHandle) :
    allocationHeap(allocator, codePageAllocators, processHandle),
    allocator(allocator),
    allocations(nullptr),
    scriptContext(scriptContext),
    processHandle(processHandle)
{TRACE_IT(1706);
#if DBG_DUMP
    this->totalBytesCode = 0;
    this->totalBytesLoopBody = 0;
    this->totalBytesAlignment = 0;
    this->totalBytesCommitted = 0;
    this->totalBytesReserved = 0;
    this->name = name;
#endif
}

//----------------------------------------------------------------------------
// EmitBufferManager::~EmitBufferManager()
//      Free up all the VirtualAlloced memory
//----------------------------------------------------------------------------
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::~EmitBufferManager()
{TRACE_IT(1707);
    Clear();
}

template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
void
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::Decommit()
{TRACE_IT(1708);
    FreeAllocations(false);
}
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
void
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::Clear()
{TRACE_IT(1709);
    FreeAllocations(true);
}

template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
void
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::FreeAllocations(bool release)
{TRACE_IT(1710);
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

#if DBG_DUMP
    if (!release && PHASE_STATS1(Js::EmitterPhase))
    {TRACE_IT(1711);
        this->DumpAndResetStats(Js::Configuration::Global.flags.Filename);
    }
#endif

    TEmitBufferAllocation * allocation = this->allocations;
    while (allocation != nullptr)
    {TRACE_IT(1712);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if(CONFIG_FLAG(CheckEmitBufferPermissions))
        {TRACE_IT(1713);
            CheckBufferPermissions(allocation);
        }
#endif
        if (release)
        {TRACE_IT(1714);
            this->allocationHeap.Free(allocation->allocation);
        }
        else if ((scriptContext != nullptr) && allocation->recorded)
        {TRACE_IT(1715);
            // In case of ThunkEmitter the script context would be null and we don't want to track that as code size.
            this->scriptContext->GetThreadContext()->SubCodeSize(allocation->bytesCommitted);
            allocation->recorded = false;
        }

        allocation = allocation->nextAllocation;
    }
    if (release)
    {TRACE_IT(1716);
        this->allocations = nullptr;
    }
    else
    {TRACE_IT(1717);
        this->allocationHeap.DecommitAll();
    }
}

template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
bool EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::IsInHeap(__in void* address)
{TRACE_IT(1718);
    AutoRealOrFakeCriticalSection<SyncObject> autocs(&this->criticalSection);
    return this->allocationHeap.IsInHeap(address);
}

template <typename TAlloc, typename TPreReservedAlloc>
class AutoCustomHeapPointer
{
public:
    AutoCustomHeapPointer(CustomHeap::Heap<TAlloc, TPreReservedAlloc> * allocationHeap, CustomHeap::Allocation* heapAllocation) :
        _allocationHeap(allocationHeap),
        _heapAllocation(heapAllocation)
    {TRACE_IT(1719);
    }

    ~AutoCustomHeapPointer()
    {TRACE_IT(1720);
        if (_heapAllocation)
        {TRACE_IT(1721);
            _allocationHeap->Free(_heapAllocation);
        }
    }

    CustomHeap::Allocation* Detach()
    {TRACE_IT(1722);
        CustomHeap::Allocation* allocation = _heapAllocation;
        Assert(allocation != nullptr);
        _heapAllocation = nullptr;
        return allocation;
    }

private:
    CustomHeap::Allocation* _heapAllocation;
    CustomHeap::Heap<TAlloc, TPreReservedAlloc>* _allocationHeap;
};

//----------------------------------------------------------------------------
// EmitBufferManager::NewAllocation
//      Create a new allocation
//----------------------------------------------------------------------------
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
EmitBufferAllocation<TAlloc, TPreReservedAlloc> *
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::NewAllocation(size_t bytes, ushort pdataCount, ushort xdataSize, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode)
{TRACE_IT(1723);
    FAULTINJECT_MEMORY_THROW(_u("JIT"), bytes);

    Assert(this->criticalSection.IsLocked());

    bool isAllJITCodeInPreReservedRegion = true;
    CustomHeap::Allocation* heapAllocation = this->allocationHeap.Alloc(bytes, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, &isAllJITCodeInPreReservedRegion);

    if (heapAllocation  == nullptr)
    {TRACE_IT(1724);
        if (!JITManager::GetJITManager()->IsJITServer())
        {TRACE_IT(1725);
            // This is used in interpreter scenario, thus we need to try to recover memory, if possible.
            // Can't simply throw as in JIT scenario, for which throw is what we want in order to give more mem to interpreter.
            JsUtil::ExternalApi::RecoverUnusedMemory();
            heapAllocation = this->allocationHeap.Alloc(bytes, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, &isAllJITCodeInPreReservedRegion);
        }
    }

    if (heapAllocation  == nullptr)
    {TRACE_IT(1726);
        Js::Throw::OutOfMemory();
    }

#if DBG
    heapAllocation->isAllocationUsed = true;
#endif

    AutoCustomHeapPointer<TAlloc, TPreReservedAlloc> allocatedMemory(&this->allocationHeap, heapAllocation);
    VerboseHeapTrace(_u("New allocation: 0x%p, size: %p\n"), heapAllocation->address, heapAllocation->size);
    TEmitBufferAllocation * allocation = AnewStruct(this->allocator, TEmitBufferAllocation);

    allocation->bytesCommitted = heapAllocation->size;
    allocation->allocation = allocatedMemory.Detach();
    allocation->bytesUsed = 0;
    allocation->nextAllocation = this->allocations;
    allocation->recorded = false;
    allocation->inPrereservedRegion = isAllJITCodeInPreReservedRegion;

    this->allocations = allocation;

#if DBG_DUMP
    this->totalBytesCommitted += heapAllocation->size;
#endif

    return allocation;
}

template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
bool
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::FreeAllocation(void* address)
{TRACE_IT(1727);
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

    TEmitBufferAllocation* previous = nullptr;
    TEmitBufferAllocation* allocation = allocations;
    while(allocation != nullptr)
    {TRACE_IT(1728);
        if (address >= allocation->allocation->address && address < (allocation->allocation->address + allocation->bytesUsed))
        {TRACE_IT(1729);
            if (previous == nullptr)
            {TRACE_IT(1730);
                this->allocations = allocation->nextAllocation;
            }
            else
            {TRACE_IT(1731);
                previous->nextAllocation = allocation->nextAllocation;
            }

            if ((scriptContext != nullptr) && allocation->recorded)
            {TRACE_IT(1732);
                this->scriptContext->GetThreadContext()->SubCodeSize(allocation->bytesCommitted);
            }

            VerboseHeapTrace(_u("Freeing 0x%p, allocation: 0x%p\n"), address, allocation->allocation->address);

            this->allocationHeap.Free(allocation->allocation);
            this->allocator->Free(allocation, sizeof(TEmitBufferAllocation));

            return true;
        }
        previous = allocation;
        allocation = allocation->nextAllocation;
    }
    return false;
}

//----------------------------------------------------------------------------
// EmitBufferManager::FinalizeAllocation
//      Fill the rest of the page with debugger breakpoint.
//----------------------------------------------------------------------------
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
bool EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::FinalizeAllocation(TEmitBufferAllocation *allocation, BYTE * dstBuffer)
{TRACE_IT(1733);
    Assert(this->criticalSection.IsLocked());

    DWORD bytes = allocation->BytesFree();
    if(bytes > 0)
    {TRACE_IT(1734);
        BYTE* buffer = nullptr;
        this->GetBuffer(allocation, bytes, &buffer);
        if (!this->CommitBuffer(allocation, dstBuffer, 0, /*sourceBuffer=*/ nullptr, /*alignPad=*/ bytes))
        {TRACE_IT(1735);
            return false;
        }

#if DBG_DUMP
        this->totalBytesCode -= bytes;
#endif
    }

    return true;
}

template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
EmitBufferAllocation<TAlloc, TPreReservedAlloc>*
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::GetBuffer(TEmitBufferAllocation *allocation, __in size_t bytes, __deref_bcount(bytes) BYTE** ppBuffer)
{TRACE_IT(1736);
    Assert(this->criticalSection.IsLocked());

    Assert(allocation->BytesFree() >= bytes);

    // In case of ThunkEmitter the script context would be null and we don't want to track that as code size.
    if (scriptContext && !allocation->recorded)
    {TRACE_IT(1737);
        this->scriptContext->GetThreadContext()->AddCodeSize(allocation->bytesCommitted);
        allocation->recorded = true;
    }

    // The codegen buffer is beyond the alignment section - hence, we pass this pointer.
    *ppBuffer = allocation->GetUnused();
    return allocation;
}

//----------------------------------------------------------------------------
// EmitBufferManager::Allocate
//      Allocates an executable buffer with a certain alignment
//      NOTE: This buffer is not readable or writable. Use CommitBuffer
//      to modify this buffer one page at a time.
//----------------------------------------------------------------------------
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
EmitBufferAllocation<TAlloc, TPreReservedAlloc>*
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::AllocateBuffer(__in size_t bytes, __deref_bcount(bytes) BYTE** ppBuffer, ushort pdataCount /*=0*/, ushort xdataSize  /*=0*/, bool canAllocInPreReservedHeapPageSegment /*=false*/,
    bool isAnyJittedCode /* = false*/)
{TRACE_IT(1738);
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

    Assert(ppBuffer != nullptr);

    TEmitBufferAllocation * allocation = this->NewAllocation(bytes, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, isAnyJittedCode);

    GetBuffer(allocation, bytes, ppBuffer);

#if DBG
    MEMORY_BASIC_INFORMATION memBasicInfo;
    size_t resultBytes = VirtualQueryEx(this->processHandle, allocation->allocation->address, &memBasicInfo, sizeof(memBasicInfo));
    if (resultBytes == 0) 
    {TRACE_IT(1739);
        MemoryOperationLastError::RecordLastError();
        if (this->processHandle != GetCurrentProcess())
        {TRACE_IT(1740);            
            return nullptr;
        }
    }
    Assert(resultBytes != 0 && memBasicInfo.Protect == PAGE_EXECUTE);
#endif

    return allocation;
}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
bool EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::CheckCommitFaultInjection()
{TRACE_IT(1741);
    if (Js::Configuration::Global.flags.ForceOOMOnEBCommit == 0)
    {TRACE_IT(1742);
        return false;
    }

    commitCount++;

    if (Js::Configuration::Global.flags.ForceOOMOnEBCommit == -1)
    {TRACE_IT(1743);
        Output::Print(_u("Commit count: %d\n"), commitCount);
    }
    else if (commitCount == Js::Configuration::Global.flags.ForceOOMOnEBCommit)
    {TRACE_IT(1744);
        return true;
    }

    return false;
}

#endif

#if DBG
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
bool EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::IsBufferExecuteReadOnly(TEmitBufferAllocation * allocation)
{TRACE_IT(1745);
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);
    MEMORY_BASIC_INFORMATION memBasicInfo;
    size_t resultBytes = VirtualQuery(allocation->allocation->address, &memBasicInfo, sizeof(memBasicInfo));
    return resultBytes != 0 && memBasicInfo.Protect == PAGE_EXECUTE;
}
#endif

template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
bool EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::ProtectBufferWithExecuteReadWriteForInterpreter(TEmitBufferAllocation* allocation)
{TRACE_IT(1746);
    Assert(this->criticalSection.IsLocked());
    Assert(allocation != nullptr);
    return (this->allocationHeap.ProtectAllocationWithExecuteReadWrite(allocation->allocation) == TRUE);
}

// Returns true if we successfully commit the buffer
// Returns false if we OOM
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
bool EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::CommitBufferForInterpreter(TEmitBufferAllocation* allocation, _In_reads_bytes_(bufferSize) BYTE* pBuffer, _In_ size_t bufferSize)
{TRACE_IT(1747);
    Assert(this->criticalSection.IsLocked());

    Assert(allocation != nullptr);
    allocation->bytesUsed += bufferSize;
#ifdef DEBUG
    this->totalBytesCode += bufferSize;
#endif

    VerboseHeapTrace(_u("Setting execute permissions on 0x%p, allocation: 0x%p\n"), pBuffer, allocation->allocation->address);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (CheckCommitFaultInjection())
    {TRACE_IT(1748);
        return false;
    }
#endif

    if (!JITManager::GetJITManager()->IsJITServer() && !this->allocationHeap.ProtectAllocationWithExecuteReadOnly(allocation->allocation))
    {TRACE_IT(1749);
        return false;
    }

    FlushInstructionCache(this->processHandle, pBuffer, bufferSize);

    return true;
}

//----------------------------------------------------------------------------
// EmitBufferManager::CommitBuffer
//      Aligns the buffer with DEBUG instructions.
//      Copies contents of source buffer to the destination buffer - at max of one page at a time.
//      This ensures that only 1 page is writable at any point of time.
//      Commit a buffer from the last AllocateBuffer call that is filled.
//----------------------------------------------------------------------------
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
bool
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::CommitBuffer(TEmitBufferAllocation* allocation, __out_bcount(bytes) BYTE* destBuffer, __in size_t bytes, __in_bcount(bytes) const BYTE* sourceBuffer, __in DWORD alignPad)
{TRACE_IT(1750);
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

    Assert(destBuffer != nullptr);
    Assert(allocation != nullptr);

    BYTE *currentDestBuffer = destBuffer + allocation->GetBytesUsed();
    char *bufferToFlush = allocation->allocation->address + allocation->GetBytesUsed();
    Assert(allocation->BytesFree() >= bytes + alignPad);

    size_t bytesLeft = bytes + alignPad;
    size_t sizeToFlush = bytesLeft;

    // Copy the contents and set the alignment pad
    while(bytesLeft != 0)
    {TRACE_IT(1751);
        DWORD spaceInCurrentPage = AutoSystemInfo::PageSize - ((size_t)currentDestBuffer & (AutoSystemInfo::PageSize - 1));
        size_t bytesToChange = bytesLeft > spaceInCurrentPage ? spaceInCurrentPage : bytesLeft;


        // Buffer and the bytes that are marked RWX - these will eventually be marked as 'EXCEUTE' only.
        BYTE* readWriteBuffer = currentDestBuffer;
        size_t readWriteBytes = bytesToChange;

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (CheckCommitFaultInjection())
        {TRACE_IT(1752);
            return false;
        }
#endif
        if (!JITManager::GetJITManager()->IsJITServer() && !this->allocationHeap.ProtectAllocationWithExecuteReadWrite(allocation->allocation, (char*)readWriteBuffer))
        {TRACE_IT(1753);
            return false;
        }

        if (alignPad != 0)
        {TRACE_IT(1754);
            DWORD alignBytes = alignPad < spaceInCurrentPage ? alignPad : spaceInCurrentPage;
            CustomHeap::FillDebugBreak(currentDestBuffer, alignBytes);

            alignPad -= alignBytes;
            currentDestBuffer += alignBytes;
            allocation->bytesUsed += alignBytes;
            bytesLeft -= alignBytes;
            bytesToChange -= alignBytes;

#if DBG_DUMP
            this->totalBytesAlignment += alignBytes;
#endif
        }

        // If there are bytes still left to be copied then we should do the copy.
        if(bytesToChange > 0)
        {TRACE_IT(1755);
            AssertMsg(alignPad == 0, "If we are copying right now - we should be done with setting alignment.");

            memcpy_s(currentDestBuffer, allocation->BytesFree(), sourceBuffer, bytesToChange);

            currentDestBuffer += bytesToChange;
            sourceBuffer += bytesToChange;
            allocation->bytesUsed += bytesToChange;
            bytesLeft -= bytesToChange;
        }

        Assert(readWriteBuffer + readWriteBytes == currentDestBuffer);

        if (!JITManager::GetJITManager()->IsJITServer() && !this->allocationHeap.ProtectAllocationWithExecuteReadOnly(allocation->allocation, (char*)readWriteBuffer))
        {TRACE_IT(1756);
            return false;
        }
    }

    FlushInstructionCache(this->processHandle, bufferToFlush, sizeToFlush);
#if DBG_DUMP
    this->totalBytesCode += bytes;
#endif

    //Finish the current EmitBufferAllocation
    return FinalizeAllocation(allocation, destBuffer);
}

template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
void
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::CompletePreviousAllocation(TEmitBufferAllocation* allocation)
{TRACE_IT(1757);
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);
    if (allocation != nullptr)
    {TRACE_IT(1758);
        allocation->bytesUsed = allocation->bytesCommitted;
    }
}

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
void
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::CheckBufferPermissions(TEmitBufferAllocation *allocation)
{TRACE_IT(1759);
    AutoRealOrFakeCriticalSection<SyncObject> autoCs(&this->criticalSection);

    if(allocation->bytesCommitted == 0)
        return;

    MEMORY_BASIC_INFORMATION memInfo;

    BYTE *buffer = (BYTE*) allocation->allocation->address;
    SIZE_T size = allocation->bytesCommitted;

    while(1)
    {TRACE_IT(1760);
        SIZE_T result = VirtualQuery(buffer, &memInfo, sizeof(memInfo));
        if(result == 0)
        {TRACE_IT(1761);
            // VirtualQuery failed.  This is not an expected condition, but it would be benign for the purposes of this check.  Seems
            // to occur occasionally on process shutdown.
            break;
        }
        else if(memInfo.Protect == PAGE_EXECUTE_READWRITE)
        {TRACE_IT(1762);
            Output::Print(_u("ERROR: Found PAGE_EXECUTE_READWRITE page!\n"));
#ifdef DEBUG
            AssertMsg(FALSE, "Page was marked PAGE_EXECUTE_READWRITE");
#else
            Fatal();
#endif
        }

        // Figure out if we need to continue the query.  The returned size might be larger than the size we requested,
        // for instance if more pages were allocated directly afterward, with the same permissions.
        if(memInfo.RegionSize >= size)
        {TRACE_IT(1763);
            break;
        }

        // recalculate size for next iteration
        buffer += memInfo.RegionSize;
        size -= memInfo.RegionSize;

        if(size <= 0)
        {
            AssertMsg(FALSE, "Last VirtualQuery left us with unmatched regions");
            break;
        }
    }
}
#endif

#if DBG_DUMP
template <typename TAlloc, typename TPreReservedAlloc, class SyncObject>
void
EmitBufferManager<TAlloc, TPreReservedAlloc, SyncObject>::DumpAndResetStats(char16 const * filename)
{TRACE_IT(1764);
    if (this->totalBytesCommitted != 0)
    {TRACE_IT(1765);
        size_t wasted = this->totalBytesCommitted - this->totalBytesCode - this->totalBytesAlignment;
        Output::Print(_u("Stats for %s: %s \n"), name, filename);
        Output::Print(_u("  Total code size      : %10d (%6.2f%% of committed)\n"), this->totalBytesCode,
            (float)this->totalBytesCode * 100 / this->totalBytesCommitted);
        Output::Print(_u("  Total LoopBody code  : %10d\n"), this->totalBytesLoopBody);
        Output::Print(_u("  Total alignment size : %10d (%6.2f%% of committed)\n"), this->totalBytesAlignment,
            (float)this->totalBytesAlignment * 100 / this->totalBytesCommitted);
        Output::Print(_u("  Total wasted size    : %10d (%6.2f%% of committed)\n"), wasted,
            (float)wasted * 100 / this->totalBytesCommitted);
        Output::Print(_u("  Total committed size : %10d (%6.2f%% of reserved)\n"), this->totalBytesCommitted,
            (float)this->totalBytesCommitted * 100 / this->totalBytesReserved);
        Output::Print(_u("  Total reserved size  : %10d\n"), this->totalBytesReserved);
    }
    this->totalBytesCode = 0;
    this->totalBytesLoopBody = 0;
    this->totalBytesAlignment = 0;
    this->totalBytesCommitted = 0;
    this->totalBytesReserved = 0;
}
#endif

template class EmitBufferManager<VirtualAllocWrapper, PreReservedVirtualAllocWrapper, FakeCriticalSection>;
template class EmitBufferManager<VirtualAllocWrapper, PreReservedVirtualAllocWrapper, CriticalSection>;
#if ENABLE_OOP_NATIVE_CODEGEN
template class EmitBufferManager<SectionAllocWrapper, PreReservedSectionAllocWrapper, FakeCriticalSection>;
template class EmitBufferManager<SectionAllocWrapper, PreReservedSectionAllocWrapper, CriticalSection>;
#endif
