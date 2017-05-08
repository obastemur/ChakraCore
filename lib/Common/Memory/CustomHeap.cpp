//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"
#ifdef _M_X64
#include "Memory/amd64/XDataAllocator.h"
#elif defined(_M_ARM)
#include "Memory/arm/XDataAllocator.h"
#include <wchar.h>
#elif defined(_M_ARM64)
#include "Memory/arm64/XDataAllocator.h"
#endif
#include "CustomHeap.h"

namespace Memory
{
namespace CustomHeap
{

#pragma region "Constructor and Destructor"

template<typename TAlloc, typename TPreReservedAlloc>
Heap<TAlloc, TPreReservedAlloc>::Heap(ArenaAllocator * alloc, CodePageAllocators<TAlloc, TPreReservedAlloc> * codePageAllocators, HANDLE processHandle):
    auxiliaryAllocator(alloc),
    codePageAllocators(codePageAllocators),
    lastSecondaryAllocStateChangedCount(0),
    processHandle(processHandle)
#if DBG_DUMP
    , freeObjectSize(0)
    , totalAllocationSize(0)
    , allocationsSinceLastCompact(0)
    , freesSinceLastCompact(0)
#endif
#if DBG
    , inDtor(false)
#endif
{
    for (int i = 0; i < NumBuckets; i++)
    {TRACE_IT(23004);
        this->buckets[i].Reset();
    }
}

template<typename TAlloc, typename TPreReservedAlloc>
Heap<TAlloc, TPreReservedAlloc>::~Heap()
{TRACE_IT(23005);
#if DBG
    inDtor = true;
#endif
    this->FreeAll();
}

#pragma endregion

#pragma region "Public routines"
template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeAll()
{TRACE_IT(23006);
    AutoCriticalSection autoLock(&this->codePageAllocators->cs);
    FreeBuckets(false);

    FreeLargeObjects();

    FreeDecommittedBuckets();
    FreeDecommittedLargeObjects();
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::Free(__in Allocation* object)
{TRACE_IT(23007);
    Assert(object != nullptr);

    if (object == nullptr)
    {TRACE_IT(23008);
        return;
    }

    BucketId bucket = (BucketId) GetBucketForSize(object->size);

    if (bucket == BucketId::LargeObjectList)
    {TRACE_IT(23009);
#if PDATA_ENABLED
        if(!object->xdata.IsFreed())
        {TRACE_IT(23010);
            FreeXdata(&object->xdata, object->largeObjectAllocation.segment);
        }
#endif
        if (!object->largeObjectAllocation.isDecommitted)
        {TRACE_IT(23011);
            FreeLargeObject(object);
        }
        return;
    }

#if PDATA_ENABLED
    if(!object->xdata.IsFreed())
    {TRACE_IT(23012);
        FreeXdata(&object->xdata, object->page->segment);
    }
#endif

    if (!object->page->isDecommitted)
    {TRACE_IT(23013);
        FreeAllocation(object);
    }
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::DecommitAll()
{TRACE_IT(23014);
    // This function doesn't really touch the page allocator data structure.
    // DecommitPages is merely a wrapper for VirtualFree
    // So no need to take the critical section to synchronize

    DListBase<Allocation>::EditingIterator i(&this->largeObjectAllocations);
    while (i.Next())
    {TRACE_IT(23015); 
        Allocation& allocation = i.Data();
        Assert(!allocation.largeObjectAllocation.isDecommitted);

        this->codePageAllocators->DecommitPages(allocation.address, allocation.GetPageCount(), allocation.largeObjectAllocation.segment);
        i.MoveCurrentTo(&this->decommittedLargeObjects);
        allocation.largeObjectAllocation.isDecommitted = true;
    }

    for (int bucket = 0; bucket < BucketId::NumBuckets; bucket++)
    {
        FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, &(this->fullPages[bucket]), bucketIter1)
        {TRACE_IT(23016);
            Assert(page.inFullList);
            this->codePageAllocators->DecommitPages(page.address, 1 /* pageCount */, page.segment);
            bucketIter1.MoveCurrentTo(&(this->decommittedPages));
            page.isDecommitted = true;
        }
        NEXT_DLISTBASE_ENTRY_EDITING;

        FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, &(this->buckets[bucket]), bucketIter2)
        {TRACE_IT(23017);
            Assert(!page.inFullList);
            this->codePageAllocators->DecommitPages(page.address, 1 /* pageCount */, page.segment);
            bucketIter2.MoveCurrentTo(&(this->decommittedPages));
            page.isDecommitted = true;
        }
        NEXT_DLISTBASE_ENTRY_EDITING;
    }
}

template<typename TAlloc, typename TPreReservedAlloc>
bool Heap<TAlloc, TPreReservedAlloc>::IsInHeap(DListBase<Page> const& bucket, __in void * address)
{TRACE_IT(23018);
    DListBase<Page>::Iterator i(&bucket);
    while (i.Next())
    {TRACE_IT(23019);
        Page& page = i.Data();
        if (page.address <= address && address < page.address + AutoSystemInfo::PageSize)
        {TRACE_IT(23020);
            return true;
        }
    }
    return false;
}

template<typename TAlloc, typename TPreReservedAlloc>
bool Heap<TAlloc, TPreReservedAlloc>::IsInHeap(DListBase<Page> const buckets[NumBuckets], __in void * address)
{TRACE_IT(23021);
    for (uint i = 0; i < NumBuckets; i++)
    {TRACE_IT(23022);
        if (this->IsInHeap(buckets[i], address))
        {TRACE_IT(23023);
            return true;
        }
    }
    return false;
}

template<typename TAlloc, typename TPreReservedAlloc>
bool Heap<TAlloc, TPreReservedAlloc>::IsInHeap(DListBase<Allocation> const& allocations, __in void *address)
{TRACE_IT(23024);
    DListBase<Allocation>::Iterator i(&allocations);
    while (i.Next())
    {TRACE_IT(23025);
        Allocation& allocation = i.Data();
        if (allocation.address <= address && address < allocation.address + allocation.size)
        {TRACE_IT(23026);
            return true;
        }
    }
    return false;
}

template<typename TAlloc, typename TPreReservedAlloc>
bool Heap<TAlloc, TPreReservedAlloc>::IsInHeap(__in void* address)
{TRACE_IT(23027);
    return IsInHeap(buckets, address) || IsInHeap(fullPages, address) || IsInHeap(largeObjectAllocations, address);
}

template<typename TAlloc, typename TPreReservedAlloc>
Page * Heap<TAlloc, TPreReservedAlloc>::GetExistingPage(BucketId bucket, bool canAllocInPreReservedHeapPageSegment)
{TRACE_IT(23028);
    // TODO: this can get a non-prereserved page even if you want one
    if (!this->buckets[bucket].Empty())
    {TRACE_IT(23029);
        Assert(!this->buckets[bucket].Head().inFullList);
        return &this->buckets[bucket].Head();
    }

    return FindPageToSplit(bucket, canAllocInPreReservedHeapPageSegment);
}

/*
 * Algorithm:
 *   - Find bucket
 *   - Check bucket pages - if it has enough free space, allocate that chunk
 *   - Check pages in bigger buckets - if that has enough space, split that page and allocate from that chunk
 *   - Allocate new page
 */
template<typename TAlloc, typename TPreReservedAlloc>
Allocation* Heap<TAlloc, TPreReservedAlloc>::Alloc(size_t bytes, ushort pdataCount, ushort xdataSize, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode, _Inout_ bool* isAllJITCodeInPreReservedRegion)
{TRACE_IT(23030);
    Assert(bytes > 0);
    Assert((codePageAllocators->AllocXdata() || pdataCount == 0) && (!codePageAllocators->AllocXdata() || pdataCount > 0));
    Assert(pdataCount > 0 || (pdataCount == 0 && xdataSize == 0));

    // Round up to power of two to allocate, and figure out which bucket to allocate in
    size_t bytesToAllocate = PowerOf2Policy::GetSize(bytes);
    BucketId bucket = (BucketId) GetBucketForSize(bytesToAllocate);

    if (bucket == BucketId::LargeObjectList)
    {TRACE_IT(23031);
        return AllocLargeObject(bytes, pdataCount, xdataSize, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, isAllJITCodeInPreReservedRegion);
    }

    VerboseHeapTrace(_u("Bucket is %d\n"), bucket);
    VerboseHeapTrace(_u("Requested: %d bytes. Allocated: %d bytes\n"), bytes, bytesToAllocate);

    do
    {TRACE_IT(23032);
        Page* page = GetExistingPage(bucket, canAllocInPreReservedHeapPageSegment);
        if (page == nullptr && UpdateFullPages())
        {TRACE_IT(23033);
            page = GetExistingPage(bucket, canAllocInPreReservedHeapPageSegment);
        }

        if (page == nullptr)
        {TRACE_IT(23034);
            page = AllocNewPage(bucket, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, isAllJITCodeInPreReservedRegion);
        }
        else if (!canAllocInPreReservedHeapPageSegment && isAnyJittedCode)
        {TRACE_IT(23035);
            *isAllJITCodeInPreReservedRegion = false;
        }

        // Out of memory
        if (page == nullptr)
        {TRACE_IT(23036);
            return nullptr;
        }

#if defined(DBG)
        MEMORY_BASIC_INFORMATION memBasicInfo;
        size_t resultBytes = VirtualQueryEx(this->processHandle, page->address, &memBasicInfo, sizeof(memBasicInfo));
        if (resultBytes == 0)
        {TRACE_IT(23037);
            if (this->processHandle != GetCurrentProcess())
            {TRACE_IT(23038);
                MemoryOperationLastError::RecordLastErrorAndThrow();
            }
        }
        Assert(memBasicInfo.Protect == PAGE_EXECUTE);
#endif

        Allocation* allocation = nullptr;
        if (AllocInPage(page, bytesToAllocate, pdataCount, xdataSize, &allocation))
        {TRACE_IT(23039);
            return allocation;
        }
    } while (true);
}

template<typename TAlloc, typename TPreReservedAlloc>
BOOL Heap<TAlloc, TPreReservedAlloc>::ProtectAllocationWithExecuteReadWrite(Allocation *allocation, __in_opt char* addressInPage)
{TRACE_IT(23040);
    DWORD protectFlags = 0;

    if (AutoSystemInfo::Data.IsCFGEnabled())
    {TRACE_IT(23041);
        protectFlags = PAGE_EXECUTE_RW_TARGETS_NO_UPDATE;
    }
    else
    {TRACE_IT(23042);
        protectFlags = PAGE_EXECUTE_READWRITE;
    }
    return this->ProtectAllocation(allocation, protectFlags, PAGE_EXECUTE, addressInPage);
}

template<typename TAlloc, typename TPreReservedAlloc>
BOOL Heap<TAlloc, TPreReservedAlloc>::ProtectAllocationWithExecuteReadOnly(Allocation *allocation, __in_opt char* addressInPage)
{TRACE_IT(23043);
    DWORD protectFlags = 0;
    if (AutoSystemInfo::Data.IsCFGEnabled())
    {TRACE_IT(23044);
        protectFlags = PAGE_EXECUTE_RO_TARGETS_NO_UPDATE;
    }
    else
    {TRACE_IT(23045);
        protectFlags = PAGE_EXECUTE;
    }
    return this->ProtectAllocation(allocation, protectFlags, PAGE_EXECUTE_READWRITE, addressInPage);
}

template<typename TAlloc, typename TPreReservedAlloc>
BOOL Heap<TAlloc, TPreReservedAlloc>::ProtectAllocation(__in Allocation* allocation, DWORD dwVirtualProtectFlags, DWORD desiredOldProtectFlag, __in_opt char* addressInPage)
{TRACE_IT(23046);
    // Allocate at the page level so that our protections don't
    // transcend allocation page boundaries. Here, allocation->address is page
    // aligned if the object is a large object allocation. If it isn't, in the else
    // branch of the following if statement, we set it to the allocation's page's
    // address. This ensures that the address being protected is always page aligned

    Assert(allocation != nullptr);
    Assert(allocation->isAllocationUsed);
    Assert(addressInPage == nullptr || (addressInPage >= allocation->address && addressInPage < (allocation->address + allocation->size)));

    char* address = allocation->address;

    size_t pageCount;
    void * segment;
    if (allocation->IsLargeAllocation())
    {TRACE_IT(23047);
#if DBG_DUMP || defined(RECYCLER_TRACE)
        if (Js::Configuration::Global.flags.IsEnabled(Js::TraceProtectPagesFlag))
        {TRACE_IT(23048);
            Output::Print(_u("Protecting large allocation\n"));
        }
#endif
        segment = allocation->largeObjectAllocation.segment;

        if (addressInPage != nullptr)
        {TRACE_IT(23049);
            if (addressInPage >= allocation->address + AutoSystemInfo::PageSize)
            {TRACE_IT(23050);
                size_t page = (addressInPage - allocation->address) / AutoSystemInfo::PageSize;

                address = allocation->address + (page * AutoSystemInfo::PageSize);
            }
            pageCount = 1;
        }
        else
        {TRACE_IT(23051);
            pageCount = allocation->GetPageCount();
        }

        VerboseHeapTrace(_u("Protecting 0x%p with 0x%x\n"), address, dwVirtualProtectFlags);
        return this->codePageAllocators->ProtectPages(address, pageCount, segment, dwVirtualProtectFlags, desiredOldProtectFlag);
    }
    else
    {TRACE_IT(23052);
#if DBG_DUMP || defined(RECYCLER_TRACE)
        if (Js::Configuration::Global.flags.IsEnabled(Js::TraceProtectPagesFlag))
        {TRACE_IT(23053);
            Output::Print(_u("Protecting small allocation\n"));
        }
#endif
        segment = allocation->page->segment;
        address = allocation->page->address;
        pageCount = 1;

        VerboseHeapTrace(_u("Protecting 0x%p with 0x%x\n"), address, dwVirtualProtectFlags);
        return this->codePageAllocators->ProtectPages(address, pageCount, segment, dwVirtualProtectFlags, desiredOldProtectFlag);
    }
}

#pragma endregion

#pragma region "Large object methods"
template<typename TAlloc, typename TPreReservedAlloc>
Allocation* Heap<TAlloc, TPreReservedAlloc>::AllocLargeObject(size_t bytes, ushort pdataCount, ushort xdataSize, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode, _Inout_ bool* isAllJITCodeInPreReservedRegion)
{TRACE_IT(23054);
    size_t pages = GetNumPagesForSize(bytes);

    if (pages == 0)
    {TRACE_IT(23055);
        return nullptr;
    }

    void * segment = nullptr;
    char* address = nullptr;

#if PDATA_ENABLED
    XDataAllocation xdata;
#endif

    {
        AutoCriticalSection autoLock(&this->codePageAllocators->cs);
        address = this->codePageAllocators->Alloc(&pages, &segment, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, isAllJITCodeInPreReservedRegion);

        // Out of memory
        if (address == nullptr)
        {TRACE_IT(23056);
            return nullptr;
        }

        char* localAddr = this->codePageAllocators->AllocLocal(address, pages*AutoSystemInfo::PageSize, segment);
        if (!localAddr)
        {TRACE_IT(23057);
            return nullptr;
        }
        FillDebugBreak((BYTE*)localAddr, pages*AutoSystemInfo::PageSize);
        this->codePageAllocators->FreeLocal(localAddr, segment);

        if (this->processHandle == GetCurrentProcess())
        {TRACE_IT(23058);
            DWORD protectFlags = 0;
            if (AutoSystemInfo::Data.IsCFGEnabled())
            {TRACE_IT(23059);
                protectFlags = PAGE_EXECUTE_RO_TARGETS_NO_UPDATE;
            }
            else
            {TRACE_IT(23060);
                protectFlags = PAGE_EXECUTE;
            }
            this->codePageAllocators->ProtectPages(address, pages, segment, protectFlags /*dwVirtualProtectFlags*/, PAGE_READWRITE /*desiredOldProtectFlags*/);
        }
#if PDATA_ENABLED
        if(pdataCount > 0)
        {TRACE_IT(23061);
            if (!this->codePageAllocators->AllocSecondary(segment, (ULONG_PTR) address, bytes, pdataCount, xdataSize, &xdata))
            {TRACE_IT(23062);
                this->codePageAllocators->Release(address, pages, segment);
                return nullptr;
            }
        }
#endif
    }

#if defined(DBG)
    MEMORY_BASIC_INFORMATION memBasicInfo;
    size_t resultBytes = VirtualQueryEx(this->processHandle, address, &memBasicInfo, sizeof(memBasicInfo));
    if (resultBytes == 0)
    {TRACE_IT(23063);
        if (this->processHandle != GetCurrentProcess())
        {TRACE_IT(23064);
            MemoryOperationLastError::RecordLastErrorAndThrow();
        }
    }
    Assert(memBasicInfo.Protect == PAGE_EXECUTE);
#endif

    Allocation* allocation = this->largeObjectAllocations.PrependNode(this->auxiliaryAllocator);
    if (allocation == nullptr)
    {TRACE_IT(23065);
        AutoCriticalSection autoLock(&this->codePageAllocators->cs);
        this->codePageAllocators->Release(address, pages, segment);

#if PDATA_ENABLED
        if(pdataCount > 0)
        {TRACE_IT(23066);
            this->codePageAllocators->ReleaseSecondary(xdata, segment);
        }
#endif
        return nullptr;
    }

    allocation->address = address;
    allocation->largeObjectAllocation.segment = segment;
    allocation->largeObjectAllocation.isDecommitted = false;
    allocation->size = pages * AutoSystemInfo::PageSize;

#if PDATA_ENABLED
    allocation->xdata = xdata;
#endif
    return allocation;
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeDecommittedLargeObjects()
{TRACE_IT(23067);
    // CodePageAllocators is locked in FreeAll
    Assert(inDtor);
    FOREACH_DLISTBASE_ENTRY_EDITING(Allocation, allocation, &this->decommittedLargeObjects, largeObjectIter)
    {TRACE_IT(23068);
        VerboseHeapTrace(_u("Decommitting large object at address 0x%p of size %u\n"), allocation.address, allocation.size);

        this->codePageAllocators->ReleaseDecommitted(allocation.address, allocation.GetPageCount(), allocation.largeObjectAllocation.segment);

        largeObjectIter.RemoveCurrent(this->auxiliaryAllocator);
    }
    NEXT_DLISTBASE_ENTRY_EDITING;
}

//Called during Free (while shutting down)
template<typename TAlloc, typename TPreReservedAlloc>
DWORD Heap<TAlloc, TPreReservedAlloc>::EnsurePageWriteable(Page* page)
{TRACE_IT(23069);
    return EnsurePageReadWrite<PAGE_READWRITE>(page);
}

// this get called when freeing the whole page
template<typename TAlloc, typename TPreReservedAlloc>
DWORD Heap<TAlloc, TPreReservedAlloc>::EnsureAllocationWriteable(Allocation* allocation)
{TRACE_IT(23070);
    return EnsureAllocationReadWrite<PAGE_READWRITE>(allocation);
}

// this get called when only freeing a part in the page
template<typename TAlloc, typename TPreReservedAlloc>
DWORD Heap<TAlloc, TPreReservedAlloc>::EnsureAllocationExecuteWriteable(Allocation* allocation)
{TRACE_IT(23071);
    if (AutoSystemInfo::Data.IsCFGEnabled())
    {TRACE_IT(23072);
        return EnsureAllocationReadWrite<PAGE_EXECUTE_RW_TARGETS_NO_UPDATE>(allocation);
    }
    else
    {TRACE_IT(23073);
        return EnsureAllocationReadWrite<PAGE_EXECUTE_READWRITE>(allocation);
    }
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeLargeObjects()
{TRACE_IT(23074);
    AutoCriticalSection autoLock(&this->codePageAllocators->cs);
    FOREACH_DLISTBASE_ENTRY_EDITING(Allocation, allocation, &this->largeObjectAllocations, largeObjectIter)
    {TRACE_IT(23075);
        EnsureAllocationWriteable(&allocation);
#if PDATA_ENABLED
        Assert(allocation.xdata.IsFreed());
#endif
        this->codePageAllocators->Release(allocation.address, allocation.GetPageCount(), allocation.largeObjectAllocation.segment);

        largeObjectIter.RemoveCurrent(this->auxiliaryAllocator);
    }
    NEXT_DLISTBASE_ENTRY_EDITING;
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeLargeObject(Allocation* allocation)
{TRACE_IT(23076);
    AutoCriticalSection autoLock(&this->codePageAllocators->cs);

    EnsureAllocationWriteable(allocation);
#if PDATA_ENABLED
    Assert(allocation->xdata.IsFreed());
#endif
    this->codePageAllocators->Release(allocation->address, allocation->GetPageCount(), allocation->largeObjectAllocation.segment);

    this->largeObjectAllocations.RemoveElement(this->auxiliaryAllocator, allocation);
}

#pragma endregion

#pragma region "Page methods"

template<typename TAlloc, typename TPreReservedAlloc>
bool Heap<TAlloc, TPreReservedAlloc>::AllocInPage(Page* page, size_t bytes, ushort pdataCount, ushort xdataSize, Allocation ** allocationOut)
{TRACE_IT(23077);
    Allocation * allocation = AnewNoThrowStruct(this->auxiliaryAllocator, Allocation);
    if (allocation == nullptr)
    {TRACE_IT(23078);
        return true;
    }

    Assert(Math::IsPow2((int32)bytes));

    uint length = GetChunkSizeForBytes(bytes);
    BVIndex index = GetFreeIndexForPage(page, bytes);
    
    if (index == BVInvalidIndex)
    {TRACE_IT(23079);
        CustomHeap_BadPageState_fatal_error((ULONG_PTR)this);
        return false;
    }

    char* address = page->address + Page::Alignment * index;

#if PDATA_ENABLED
    XDataAllocation xdata;
    if(pdataCount > 0)
    {TRACE_IT(23080);
        AutoCriticalSection autoLock(&this->codePageAllocators->cs);
        if (this->ShouldBeInFullList(page))
        {TRACE_IT(23081);
            Adelete(this->auxiliaryAllocator, allocation);
            // If we run out of XData space with the segment, move the page to the full page list, and return false to try the next page.
            BucketId bucket = page->currentBucket;
            VerboseHeapTrace(_u("Moving page from bucket %d to full list\n"), bucket);

            Assert(!page->inFullList);
            this->buckets[bucket].MoveElementTo(page, &this->fullPages[bucket]);
            page->inFullList = true;
            return false;
        }

        if (!this->codePageAllocators->AllocSecondary(page->segment, (ULONG_PTR)address, bytes, pdataCount, xdataSize, &xdata))
        {TRACE_IT(23082);
            Adelete(this->auxiliaryAllocator, allocation);
            return true;
        }
    }
#endif

#if DBG
    allocation->isAllocationUsed = false;
    allocation->isNotExecutableBecauseOOM = false;
#endif

    allocation->page = page;
    allocation->size = bytes;
    allocation->address = address;

#if DBG_DUMP
    this->allocationsSinceLastCompact += bytes;
    this->freeObjectSize -= bytes;
#endif

    //Section of the Page should already be freed.
    if (!page->freeBitVector.TestRange(index, length))
    {TRACE_IT(23083);
        CustomHeap_BadPageState_fatal_error((ULONG_PTR)this);
        return false;
    }

    page->freeBitVector.ClearRange(index, length);
    VerboseHeapTrace(_u("ChunkSize: %d, Index: %d, Free bit vector in page: "), length, index);

#if VERBOSE_HEAP
    page->freeBitVector.DumpWord();
#endif
    VerboseHeapTrace(_u("\n"));

    if (this->ShouldBeInFullList(page))
    {TRACE_IT(23084);
        BucketId bucket = page->currentBucket;
        VerboseHeapTrace(_u("Moving page from bucket %d to full list\n"), bucket);

        Assert(!page->inFullList);
        this->buckets[bucket].MoveElementTo(page, &this->fullPages[bucket]);
        page->inFullList = true;
    }

#if PDATA_ENABLED
    allocation->xdata = xdata;
#endif

    *allocationOut = allocation;
    return true;
}

template<typename TAlloc, typename TPreReservedAlloc>
Page* Heap<TAlloc, TPreReservedAlloc>::AllocNewPage(BucketId bucket, bool canAllocInPreReservedHeapPageSegment, bool isAnyJittedCode, _Inout_ bool* isAllJITCodeInPreReservedRegion)
{TRACE_IT(23085);
    void* pageSegment = nullptr;

    char* address = nullptr;
    {
        AutoCriticalSection autoLock(&this->codePageAllocators->cs);
        address = this->codePageAllocators->AllocPages(1, &pageSegment, canAllocInPreReservedHeapPageSegment, isAnyJittedCode, isAllJITCodeInPreReservedRegion);

        if (address == nullptr)
        {TRACE_IT(23086);
            return nullptr;
        }
    }

    char* localAddr = this->codePageAllocators->AllocLocal(address, AutoSystemInfo::PageSize, pageSegment);
    if (!localAddr)
    {TRACE_IT(23087);
        return nullptr;
    }
    FillDebugBreak((BYTE*)localAddr, AutoSystemInfo::PageSize);
    this->codePageAllocators->FreeLocal(localAddr, pageSegment);

    DWORD protectFlags = 0;

    if (AutoSystemInfo::Data.IsCFGEnabled())
    {TRACE_IT(23088);
        protectFlags = PAGE_EXECUTE_RO_TARGETS_NO_UPDATE;
    }
    else
    {TRACE_IT(23089);
        protectFlags = PAGE_EXECUTE;
    }

    //Change the protection of the page to Read-Only Execute, before adding it to the bucket list.
    this->codePageAllocators->ProtectPages(address, 1, pageSegment, protectFlags, PAGE_READWRITE);

    // Switch to allocating on a list of pages so we can do leak tracking later
    VerboseHeapTrace(_u("Allocing new page in bucket %d\n"), bucket);
    Page* page = this->buckets[bucket].PrependNode(this->auxiliaryAllocator, address, pageSegment, bucket);

    if (page == nullptr)
    {TRACE_IT(23090);
        AutoCriticalSection autoLock(&this->codePageAllocators->cs);
        this->codePageAllocators->ReleasePages(address, 1, pageSegment);
        return nullptr;
    }

#if DBG_DUMP
    this->totalAllocationSize += AutoSystemInfo::PageSize;
    this->freeObjectSize += AutoSystemInfo::PageSize;
#endif

    return page;
}

template<typename TAlloc, typename TPreReservedAlloc>
Page* Heap<TAlloc, TPreReservedAlloc>::AddPageToBucket(Page* page, BucketId bucket, bool wasFull)
{TRACE_IT(23091);
    Assert(bucket > BucketId::InvalidBucket && bucket < BucketId::NumBuckets);

    BucketId oldBucket = page->currentBucket;

    page->currentBucket = bucket;
    if (wasFull)
    {TRACE_IT(23092);
        #pragma prefast(suppress: __WARNING_UNCHECKED_LOWER_BOUND_FOR_ENUMINDEX, "targetBucket is always in range >= SmallObjectList, but an __in_range doesn't fix the warning.");
        Assert(page->inFullList);
        this->fullPages[oldBucket].MoveElementTo(page, &this->buckets[bucket]);
        page->inFullList = false;
    }
    else
    {TRACE_IT(23093);
        Assert(!page->inFullList);
        #pragma prefast(suppress: __WARNING_UNCHECKED_LOWER_BOUND_FOR_ENUMINDEX, "targetBucket is always in range >= SmallObjectList, but an __in_range doesn't fix the warning.");
        this->buckets[oldBucket].MoveElementTo(page, &this->buckets[bucket]);
    }

    return page;
}

/*
 * This method goes through the buckets greater than the target bucket
 * and if the higher bucket has a page with enough free space to allocate
 * something in the smaller bucket, then we bring the page to the smaller
 * bucket.
 * Note that if we allocate something from a page in the given bucket,
 * and then that page is split into a lower bucket, freeing is still not
 * a problem since the larger allocation is a multiple of the smaller one.
 * This gets more complicated if we can coalesce buckets. In that case,
 * we need to make sure that if a page was coalesced, and an allocation
 * pre-coalescing was freed, the page would need to get split upon free
 * to ensure correctness. For now, we've skipped implementing coalescing.
 * findPreReservedHeapPages - true, if we need to find pages only belonging to PreReservedHeapSegment
 */
template<typename TAlloc, typename TPreReservedAlloc>
Page* Heap<TAlloc, TPreReservedAlloc>::FindPageToSplit(BucketId targetBucket, bool findPreReservedHeapPages)
{TRACE_IT(23094);
    for (BucketId b = (BucketId)(targetBucket + 1); b < BucketId::NumBuckets; b = (BucketId) (b + 1))
    {TRACE_IT(23095);
        #pragma prefast(suppress: __WARNING_UNCHECKED_LOWER_BOUND_FOR_ENUMINDEX, "targetBucket is always in range >= SmallObjectList, but an __in_range doesn't fix the warning.");
        FOREACH_DLISTBASE_ENTRY_EDITING(Page, pageInBucket, &this->buckets[b], bucketIter)
        {TRACE_IT(23096);
            Assert(!pageInBucket.inFullList);
            if (findPreReservedHeapPages && !this->codePageAllocators->IsPreReservedSegment(pageInBucket.segment))
            {TRACE_IT(23097);
                //Find only pages that are pre-reserved using preReservedHeapPageAllocator
                continue;
            }

            if (pageInBucket.CanAllocate(targetBucket))
            {TRACE_IT(23098);
                Page* page = &pageInBucket;
                if (findPreReservedHeapPages)
                {TRACE_IT(23099);
                    VerboseHeapTrace(_u("PRE-RESERVE: Found page for splitting in Pre Reserved Segment\n"));
                }
                VerboseHeapTrace(_u("Found page to split. Moving from bucket %d to %d\n"), b, targetBucket);
                return AddPageToBucket(page, targetBucket);
            }
        }
        NEXT_DLISTBASE_ENTRY_EDITING;
    }

    return nullptr;
}

template<typename TAlloc, typename TPreReservedAlloc>
BVIndex Heap<TAlloc, TPreReservedAlloc>::GetIndexInPage(__in Page* page, __in char* address)
{TRACE_IT(23100);
    Assert(page->address <= address && address < page->address + AutoSystemInfo::PageSize);

    return (BVIndex) ((address - page->address) / Page::Alignment);
}

#pragma endregion

/**
 * Free List methods
 */
#pragma region "Freeing methods"
template<typename TAlloc, typename TPreReservedAlloc>
bool Heap<TAlloc, TPreReservedAlloc>::FreeAllocation(Allocation* object)
{TRACE_IT(23101);
    Page* page = object->page;
    void* segment = page->segment;
    size_t pageSize =  AutoSystemInfo::PageSize;

    unsigned int length = GetChunkSizeForBytes(object->size);
    BVIndex index = GetIndexInPage(page, object->address);

    uint freeBitsCount = page->freeBitVector.Count();

    // Make sure that the section under interest or the whole page has not already been freed
    if (page->IsEmpty() || page->freeBitVector.TestAnyInRange(index, length))
    {TRACE_IT(23102);
        CustomHeap_BadPageState_fatal_error((ULONG_PTR)this);
        return false;
    }

    if (page->inFullList)
    {TRACE_IT(23103);
        VerboseHeapTrace(_u("Recycling page 0x%p because address 0x%p of size %d was freed\n"), page->address, object->address, object->size);
       
        // If the object being freed is equal to the page size, we're
        // going to remove it anyway so don't add it to a bucket
        if (object->size != pageSize)
        {
            AddPageToBucket(page, page->currentBucket, true);
        }
        else
        {TRACE_IT(23104);
            EnsureAllocationWriteable(object);

            // Fill the old buffer with debug breaks
            char* localAddr = this->codePageAllocators->AllocLocal(object->address, object->size, page->segment);
            if (!localAddr)
            {TRACE_IT(23105);
                MemoryOperationLastError::CheckProcessAndThrowFatalError(this->processHandle);
                return false;
            }
            FillDebugBreak((BYTE*)localAddr, object->size);
            this->codePageAllocators->FreeLocal(localAddr, page->segment);

            void* pageAddress = page->address;

            this->fullPages[page->currentBucket].RemoveElement(this->auxiliaryAllocator, page);            

            // The page is not in any bucket- just update the stats, free the allocation
            // and dump the page- we don't need to update free object size since the object
            // size is equal to the page size so they cancel each other out
#if DBG_DUMP
            this->totalAllocationSize -= pageSize;
#endif
            this->auxiliaryAllocator->Free(object, sizeof(Allocation));
            {TRACE_IT(23106);
                AutoCriticalSection autoLock(&this->codePageAllocators->cs);
                this->codePageAllocators->ReleasePages(pageAddress, 1, segment);
            }
            VerboseHeapTrace(_u("FastPath: freeing page-sized object directly\n"));
            return true;
        }
    }

    // If the page is about to become empty then we should not need
    // to set it to executable and we don't expect to restore the
    // previous protection settings.
    if (freeBitsCount == BVUnit::BitsPerWord - length)
    {TRACE_IT(23107);
        EnsureAllocationWriteable(object);
        FreeAllocationHelper(object, index, length);
        Assert(page->IsEmpty());

        this->buckets[page->currentBucket].RemoveElement(this->auxiliaryAllocator, page);
        return false;
    }
    else
    {TRACE_IT(23108);
        EnsureAllocationExecuteWriteable(object);

        FreeAllocationHelper(object, index, length);

        // after freeing part of the page, the page should be in PAGE_EXECUTE_READWRITE protection, and turning to PAGE_EXECUTE (always with TARGETS_NO_UPDATE state)

        DWORD protectFlags = 0;

        if (AutoSystemInfo::Data.IsCFGEnabled())
        {TRACE_IT(23109);
            protectFlags = PAGE_EXECUTE_RO_TARGETS_NO_UPDATE;
        }
        else
        {TRACE_IT(23110);
            protectFlags = PAGE_EXECUTE;
        }

        this->codePageAllocators->ProtectPages(page->address, 1, segment, protectFlags, PAGE_EXECUTE_READWRITE);
        
        return true;
    }
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeAllocationHelper(Allocation* object, BVIndex index, uint length)
{TRACE_IT(23111);
    Page* page = object->page;

    // Fill the old buffer with debug breaks
    char* localAddr = this->codePageAllocators->AllocLocal(object->address, object->size, page->segment);
    if (localAddr)
    {TRACE_IT(23112);
        FillDebugBreak((BYTE*)localAddr, object->size);
        this->codePageAllocators->FreeLocal(localAddr, page->segment);
    }
    else
    {TRACE_IT(23113);
        MemoryOperationLastError::CheckProcessAndThrowFatalError(this->processHandle);
    }
    VerboseHeapTrace(_u("Setting %d bits starting at bit %d, Free bit vector in page was "), length, index);
#if VERBOSE_HEAP
    page->freeBitVector.DumpWord();
#endif
    VerboseHeapTrace(_u("\n"));

    page->freeBitVector.SetRange(index, length);

    VerboseHeapTrace(_u("Free bit vector in page: "), length, index);
#if VERBOSE_HEAP
    page->freeBitVector.DumpWord();
#endif
    VerboseHeapTrace(_u("\n"));

#if DBG_DUMP
    this->freeObjectSize += object->size;
    this->freesSinceLastCompact += object->size;
#endif

    this->auxiliaryAllocator->Free(object, sizeof(Allocation));
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeDecommittedBuckets()
{TRACE_IT(23114);
    // CodePageAllocators is locked in FreeAll
    Assert(inDtor);
    FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, &this->decommittedPages, iter)
    {TRACE_IT(23115);
        this->codePageAllocators->TrackDecommittedPages(page.address, 1, page.segment);
        iter.RemoveCurrent(this->auxiliaryAllocator);
    }
    NEXT_DLISTBASE_ENTRY_EDITING;
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreePage(Page* page)
{TRACE_IT(23116);
    // CodePageAllocators is locked in FreeAll
    Assert(inDtor);
    DWORD pageSize = AutoSystemInfo::PageSize;
    EnsurePageWriteable(page);
    size_t freeSpace = page->freeBitVector.Count() * Page::Alignment;

    VerboseHeapTrace(_u("Removing page in bucket %d, freeSpace: %d\n"), page->currentBucket, freeSpace);
    this->codePageAllocators->ReleasePages(page->address, 1, page->segment);

#if DBG_DUMP
    this->freeObjectSize -= freeSpace;
    this->totalAllocationSize -= pageSize;
#endif
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeBucket(DListBase<Page>* bucket, bool freeOnlyEmptyPages)
{TRACE_IT(23117);
    // CodePageAllocators is locked in FreeAll
    Assert(inDtor);
    FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, bucket, pageIter)
    {TRACE_IT(23118);
        // Templatize this to remove branches/make code more compact?
        if (!freeOnlyEmptyPages || page.IsEmpty())
        {TRACE_IT(23119);
            FreePage(&page);
            pageIter.RemoveCurrent(this->auxiliaryAllocator);
        }
    }
    NEXT_DLISTBASE_ENTRY_EDITING;
}

template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeBuckets(bool freeOnlyEmptyPages)
{TRACE_IT(23120);
    // CodePageAllocators is locked in FreeAll
    Assert(inDtor);
    for (int i = 0; i < NumBuckets; i++)
    {TRACE_IT(23121);
        FreeBucket(&this->buckets[i], freeOnlyEmptyPages);
        FreeBucket(&this->fullPages[i], freeOnlyEmptyPages);
    }

#if DBG_DUMP
    this->allocationsSinceLastCompact = 0;
    this->freesSinceLastCompact = 0;
#endif
}

template<typename TAlloc, typename TPreReservedAlloc>
bool Heap<TAlloc, TPreReservedAlloc>::UpdateFullPages()
{TRACE_IT(23122);
    bool updated = false;
    if (this->codePageAllocators->HasSecondaryAllocStateChanged(&lastSecondaryAllocStateChangedCount))
    {TRACE_IT(23123);
        AutoCriticalSection autoLock(&this->codePageAllocators->cs);
        for (int bucket = 0; bucket < BucketId::NumBuckets; bucket++)
        {
            FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, &(this->fullPages[bucket]), bucketIter)
            {TRACE_IT(23124);
                Assert(page.inFullList);
                if (!this->ShouldBeInFullList(&page))
                {TRACE_IT(23125);
                    VerboseHeapTrace(_u("Recycling page 0x%p because XDATA was freed\n"), page.address);
                    bucketIter.MoveCurrentTo(&(this->buckets[bucket]));
                    page.inFullList = false;
                    updated = true;
                }
            }
            NEXT_DLISTBASE_ENTRY_EDITING;
        }
    }
    return updated;
}

#if PDATA_ENABLED
template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::FreeXdata(XDataAllocation* xdata, void* segment)
{TRACE_IT(23126);
    Assert(!xdata->IsFreed()); 
    {TRACE_IT(23127);
        AutoCriticalSection autoLock(&this->codePageAllocators->cs);
        this->codePageAllocators->ReleaseSecondary(*xdata, segment);
        xdata->Free();
    }
}
#endif

#if DBG_DUMP
template<typename TAlloc, typename TPreReservedAlloc>
void Heap<TAlloc, TPreReservedAlloc>::DumpStats()
{TRACE_IT(23128);
    HeapTrace(_u("Total allocation size: %d\n"), totalAllocationSize);
    HeapTrace(_u("Total free size: %d\n"), freeObjectSize);
    HeapTrace(_u("Total allocations since last compact: %d\n"), allocationsSinceLastCompact);
    HeapTrace(_u("Total frees since last compact: %d\n"), freesSinceLastCompact);
    HeapTrace(_u("Large object count: %d\n"), this->largeObjectAllocations.Count());

    HeapTrace(_u("Buckets: \n"));
    for (int i = 0; i < BucketId::NumBuckets; i++)
    {TRACE_IT(23129);
        printf("\t%d => %u [", (1 << (i + 7)), buckets[i].Count());

        FOREACH_DLISTBASE_ENTRY_EDITING(Page, page, &this->buckets[i], bucketIter)
        {TRACE_IT(23130);
            BVUnit usedBitVector = page.freeBitVector;
            usedBitVector.ComplimentAll(); // Get the actual used bit vector
            printf(" %u ", usedBitVector.Count() * Page::Alignment); // Print out the space used in this page
        }

        NEXT_DLISTBASE_ENTRY_EDITING
            printf("] {{%u}}\n", this->fullPages[i].Count());
    }
}
#endif

#pragma endregion

/**
 * Helper methods
 */
#pragma region "Helpers"
inline unsigned int log2(size_t number)
{TRACE_IT(23131);
    const unsigned int b[] = {0x2, 0xC, 0xF0, 0xFF00, 0xFFFF0000};
    const unsigned int S[] = {1, 2, 4, 8, 16};

    unsigned int result = 0;
    for (int i = 4; i >= 0; i--)
    {TRACE_IT(23132);
        if (number & b[i])
        {TRACE_IT(23133);
            number >>= S[i];
            result |= S[i];
        }
    }

    return result;
}

inline BucketId GetBucketForSize(size_t bytes)
{TRACE_IT(23134);
    if (bytes > Page::MaxAllocationSize)
    {TRACE_IT(23135);
        return BucketId::LargeObjectList;
    }

    BucketId bucket = (BucketId) (log2(bytes) - 7);

    // < 8 => 0
    // 8 => 1
    // 9 => 2 ...
    Assert(bucket < BucketId::LargeObjectList);

    if (bucket < BucketId::SmallObjectList)
    {TRACE_IT(23136);
        bucket = BucketId::SmallObjectList;
    }

    return bucket;
}

// Fills the specified buffer with "debug break" instruction encoding.
// If there is any space left after that due to alignment, fill it with 0.
// static
void FillDebugBreak(_Out_writes_bytes_all_(byteCount) BYTE* buffer, _In_ size_t byteCount)
{TRACE_IT(23137);
#if defined(_M_ARM)
    // On ARM there is breakpoint instruction (BKPT) which is 0xBEii, where ii (immediate 8) can be any value, 0xBE in particular.
    // While it could be easier to put 0xBE (same way as 0xCC on x86), BKPT is not recommended -- it may cause unexpected side effects.
    // So, use same sequence are C++ compiler uses (0xDEFE), this is recognized by debugger as __debugbreak.
    // This is 2 bytes, and in case there is a gap of 1 byte in the end, fill it with 0 (there is no 1 byte long THUMB instruction).
    CompileAssert(sizeof(char16) == 2);
    char16 pattern = 0xDEFE;

    BYTE * writeBuffer = buffer;
    wmemset((char16 *)writeBuffer, pattern, byteCount / 2);
    if (byteCount % 2)
    {TRACE_IT(23138);
        // Note: this is valid scenario: in JIT mode, we may not be 2-byte-aligned in the end of unwind info.
        *(writeBuffer + byteCount - 1) = 0;  // Fill last remaining byte.
    }

#elif defined(_M_ARM64)
    CompileAssert(sizeof(DWORD) == 4);
    DWORD pattern = 0xd4200000 | (0xf000 << 5);
    for (size_t i = 0; i < byteCount / 4; i++)
    {TRACE_IT(23139);
        reinterpret_cast<DWORD*>(buffer)[i] = pattern;
    }
    for (size_t i = (byteCount / 4) * 4; i < byteCount; i++)
    {TRACE_IT(23140);
        // Note: this is valid scenario: in JIT mode, we may not be 2-byte-aligned in the end of unwind info.
        buffer[i] = 0;  // Fill last remaining bytes.
    }
#else
    // On Intel just use "INT 3" instruction which is 0xCC.
    memset(buffer, 0xCC, byteCount);
#endif
}

template class Heap<VirtualAllocWrapper, PreReservedVirtualAllocWrapper>;
#if ENABLE_OOP_NATIVE_CODEGEN
template class Heap<SectionAllocWrapper, PreReservedSectionAllocWrapper>;
#endif

#pragma endregion

template<>
char *
CodePageAllocators<VirtualAllocWrapper, PreReservedVirtualAllocWrapper>::AllocLocal(char * remoteAddr, size_t size, void * segment)
{TRACE_IT(23141);
    return remoteAddr;
}

template<>
void
CodePageAllocators<VirtualAllocWrapper, PreReservedVirtualAllocWrapper>::FreeLocal(char * localAddr, void * segment)
{TRACE_IT(23142);
    // do nothing in case we are in proc
}

#if ENABLE_OOP_NATIVE_CODEGEN
template<>
char *
CodePageAllocators<SectionAllocWrapper, PreReservedSectionAllocWrapper>::AllocLocal(char * remoteAddr, size_t size, void * segment)
{TRACE_IT(23143);
    AutoCriticalSection autoLock(&this->cs);
    Assert(segment);
    LPVOID address = nullptr;
    if (IsPreReservedSegment(segment))
    {TRACE_IT(23144);
        address = ((SegmentBase<PreReservedSectionAllocWrapper>*)segment)->GetAllocator()->GetVirtualAllocator()->AllocLocal(remoteAddr, size);
    }
    else
    {TRACE_IT(23145);
        address = ((SegmentBase<SectionAllocWrapper>*)segment)->GetAllocator()->GetVirtualAllocator()->AllocLocal(remoteAddr, size);
    }
    return (char*)address;
}

template<>
void
CodePageAllocators<SectionAllocWrapper, PreReservedSectionAllocWrapper>::FreeLocal(char * localAddr, void * segment)
{TRACE_IT(23146);
    AutoCriticalSection autoLock(&this->cs);
    Assert(segment);
    if (IsPreReservedSegment(segment))
    {TRACE_IT(23147);
        ((SegmentBase<PreReservedSectionAllocWrapper>*)segment)->GetAllocator()->GetVirtualAllocator()->FreeLocal(localAddr);
    }
    else
    {TRACE_IT(23148);
        ((SegmentBase<SectionAllocWrapper>*)segment)->GetAllocator()->GetVirtualAllocator()->FreeLocal(localAddr);
    }
}
#endif



} // namespace CustomHeap

} // namespace Memory
