//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

#if !FLOATVAR
CodeGenNumberThreadAllocator::CodeGenNumberThreadAllocator(Recycler * recycler)
    : recycler(recycler), currentNumberSegment(nullptr), currentChunkSegment(nullptr),
    numberSegmentEnd(nullptr), currentNumberBlockEnd(nullptr), nextNumber(nullptr), chunkSegmentEnd(nullptr),
    currentChunkBlockEnd(nullptr), nextChunk(nullptr), hasNewNumberBlock(nullptr), hasNewChunkBlock(nullptr),
    pendingIntegrationNumberSegmentCount(0), pendingIntegrationChunkSegmentCount(0),
    pendingIntegrationNumberSegmentPageCount(0), pendingIntegrationChunkSegmentPageCount(0)
{TRACE_IT(1490);
}

CodeGenNumberThreadAllocator::~CodeGenNumberThreadAllocator()
{TRACE_IT(1491);
    pendingIntegrationNumberSegment.Clear(&NoThrowNoMemProtectHeapAllocator::Instance);
    pendingIntegrationChunkSegment.Clear(&NoThrowNoMemProtectHeapAllocator::Instance);
    pendingIntegrationNumberBlock.Clear(&NoThrowHeapAllocator::Instance);
    pendingIntegrationChunkBlock.Clear(&NoThrowHeapAllocator::Instance);
    pendingFlushNumberBlock.Clear(&NoThrowHeapAllocator::Instance);
    pendingFlushChunkBlock.Clear(&NoThrowHeapAllocator::Instance);
    pendingReferenceNumberBlock.Clear(&NoThrowHeapAllocator::Instance);
}

size_t
CodeGenNumberThreadAllocator::GetNumberAllocSize()
{TRACE_IT(1492);
#ifdef RECYCLER_MEMORY_VERIFY
    if (recycler->VerifyEnabled())
    {TRACE_IT(1493);
        return HeapInfo::GetAlignedSize(AllocSizeMath::Add(sizeof(Js::JavascriptNumber) + sizeof(size_t), recycler->verifyPad));
    }
#endif
    return HeapInfo::GetAlignedSizeNoCheck(sizeof(Js::JavascriptNumber));
}


size_t
CodeGenNumberThreadAllocator::GetChunkAllocSize()
{TRACE_IT(1494);
#ifdef RECYCLER_MEMORY_VERIFY
    if (recycler->VerifyEnabled())
    {TRACE_IT(1495);
        return HeapInfo::GetAlignedSize(AllocSizeMath::Add(sizeof(CodeGenNumberChunk) + sizeof(size_t), recycler->verifyPad));
    }
#endif
    return HeapInfo::GetAlignedSizeNoCheck(sizeof(CodeGenNumberChunk));
}

Js::JavascriptNumber *
CodeGenNumberThreadAllocator::AllocNumber()
{TRACE_IT(1496);
    AutoCriticalSection autocs(&cs);
    size_t sizeCat = GetNumberAllocSize();
    if (nextNumber + sizeCat > currentNumberBlockEnd)
    {TRACE_IT(1497);
        AllocNewNumberBlock();
    }
    Js::JavascriptNumber * newNumber = (Js::JavascriptNumber *)nextNumber;
#ifdef RECYCLER_MEMORY_VERIFY
    recycler->FillCheckPad(newNumber, sizeof(Js::JavascriptNumber), sizeCat);
#endif

    nextNumber += sizeCat;
    return newNumber;
}

CodeGenNumberChunk *
CodeGenNumberThreadAllocator::AllocChunk()
{TRACE_IT(1498);
    AutoCriticalSection autocs(&cs);
    size_t sizeCat = GetChunkAllocSize();
    if (nextChunk + sizeCat > currentChunkBlockEnd)
    {TRACE_IT(1499);
        AllocNewChunkBlock();
    }
    CodeGenNumberChunk * newChunk = (CodeGenNumberChunk *)nextChunk;
#ifdef RECYCLER_MEMORY_VERIFY
    recycler->FillCheckPad(nextChunk, sizeof(CodeGenNumberChunk), sizeCat);
#endif

    memset(newChunk, 0, sizeof(CodeGenNumberChunk));
    nextChunk += sizeCat;
    return newChunk;
}

void
CodeGenNumberThreadAllocator::AllocNewNumberBlock()
{TRACE_IT(1500);
    Assert(cs.IsLocked());
    Assert(nextNumber + GetNumberAllocSize() > currentNumberBlockEnd);
    if (hasNewNumberBlock)
    {TRACE_IT(1501);
        if (!pendingReferenceNumberBlock.PrependNode(&NoThrowHeapAllocator::Instance,
            currentNumberBlockEnd - BlockSize, currentNumberSegment))
        {TRACE_IT(1502);
            Js::Throw::OutOfMemory();
        }
        hasNewNumberBlock = false;
    }

    if (currentNumberBlockEnd == numberSegmentEnd)
    {TRACE_IT(1503);
        Assert(cs.IsLocked());
        // Reserve the segment, but not committing it
        currentNumberSegment = PageAllocator::AllocPageSegment(pendingIntegrationNumberSegment, this->recycler->GetRecyclerLeafPageAllocator(), false, true, false);
        if (currentNumberSegment == nullptr)
        {TRACE_IT(1504);
            currentNumberBlockEnd = nullptr;
            numberSegmentEnd = nullptr;
            nextNumber = nullptr;
            Js::Throw::OutOfMemory();
        }
        pendingIntegrationNumberSegmentCount++;
        pendingIntegrationNumberSegmentPageCount += currentNumberSegment->GetPageCount();
        currentNumberBlockEnd = currentNumberSegment->GetAddress();
        numberSegmentEnd = currentNumberSegment->GetEndAddress();
    }

    // Commit the page.
    if (!::VirtualAlloc(currentNumberBlockEnd, BlockSize, MEM_COMMIT, PAGE_READWRITE))
    {TRACE_IT(1505);
        Js::Throw::OutOfMemory();
    }
    nextNumber = currentNumberBlockEnd;
    currentNumberBlockEnd += BlockSize;
    hasNewNumberBlock = true;
    this->recycler->GetRecyclerLeafPageAllocator()->FillAllocPages(nextNumber, 1);
}

void
CodeGenNumberThreadAllocator::AllocNewChunkBlock()
{TRACE_IT(1506);
    Assert(cs.IsLocked());
    Assert(nextChunk + GetChunkAllocSize() > currentChunkBlockEnd);
    if (hasNewChunkBlock)
    {TRACE_IT(1507);
        if (!pendingFlushChunkBlock.PrependNode(&NoThrowHeapAllocator::Instance,
            currentChunkBlockEnd - BlockSize, currentChunkSegment))
        {TRACE_IT(1508);
            Js::Throw::OutOfMemory();
        }
        // All integrated pages' object are all live initially, so don't need to rescan them
        // todo: SWB: need to allocate number with write barrier pages
        ::ResetWriteWatch(currentChunkBlockEnd - BlockSize, BlockSize);
        pendingReferenceNumberBlock.MoveTo(&pendingFlushNumberBlock);
        hasNewChunkBlock = false;
    }

    if (currentChunkBlockEnd == chunkSegmentEnd)
    {TRACE_IT(1509);
        Assert(cs.IsLocked());
        // Reserve the segment, but not committing it
        currentChunkSegment = PageAllocator::AllocPageSegment(pendingIntegrationChunkSegment, this->recycler->GetRecyclerPageAllocator(), false, true, false);
        if (currentChunkSegment == nullptr)
        {TRACE_IT(1510);
            currentChunkBlockEnd = nullptr;
            chunkSegmentEnd = nullptr;
            nextChunk = nullptr;
            Js::Throw::OutOfMemory();
        }
        pendingIntegrationChunkSegmentCount++;
        pendingIntegrationChunkSegmentPageCount += currentChunkSegment->GetPageCount();
        currentChunkBlockEnd = currentChunkSegment->GetAddress();
        chunkSegmentEnd = currentChunkSegment->GetEndAddress();
    }

    // Commit the page.
    if (!::VirtualAlloc(currentChunkBlockEnd, BlockSize, MEM_COMMIT, PAGE_READWRITE))
    {TRACE_IT(1511);
        Js::Throw::OutOfMemory();
    }

    nextChunk = currentChunkBlockEnd;
    currentChunkBlockEnd += BlockSize;
    hasNewChunkBlock = true;
    this->recycler->GetRecyclerLeafPageAllocator()->FillAllocPages(nextChunk, 1);
}

void
CodeGenNumberThreadAllocator::Integrate()
{TRACE_IT(1512);
    AutoCriticalSection autocs(&cs);
    PageAllocator * leafPageAllocator = this->recycler->GetRecyclerLeafPageAllocator();
    leafPageAllocator->IntegrateSegments(pendingIntegrationNumberSegment, pendingIntegrationNumberSegmentCount, pendingIntegrationNumberSegmentPageCount);
    PageAllocator * recyclerPageAllocator = this->recycler->GetRecyclerPageAllocator();
    recyclerPageAllocator->IntegrateSegments(pendingIntegrationChunkSegment, pendingIntegrationChunkSegmentCount, pendingIntegrationChunkSegmentPageCount);
    pendingIntegrationNumberSegmentCount = 0;
    pendingIntegrationChunkSegmentCount = 0;
    pendingIntegrationNumberSegmentPageCount = 0;
    pendingIntegrationChunkSegmentPageCount = 0;

#ifdef TRACK_ALLOC
    TrackAllocData oldAllocData = recycler->nextAllocData;
    recycler->nextAllocData.Clear();
#endif
    while (!pendingIntegrationNumberBlock.Empty())
    {
        TRACK_ALLOC_INFO(recycler, Js::JavascriptNumber, Recycler, 0, (size_t)-1);

        BlockRecord& record = pendingIntegrationNumberBlock.Head();
        if (!recycler->IntegrateBlock<LeafBit>(record.blockAddress, record.segment, GetNumberAllocSize(), sizeof(Js::JavascriptNumber)))
        {TRACE_IT(1513);
            Js::Throw::OutOfMemory();
        }
        pendingIntegrationNumberBlock.RemoveHead(&NoThrowHeapAllocator::Instance);
    }


    while (!pendingIntegrationChunkBlock.Empty())
    {
        // REVIEW: the above number block integration can be moved into this loop

        TRACK_ALLOC_INFO(recycler, CodeGenNumberChunk, Recycler, 0, (size_t)-1);

        BlockRecord& record = pendingIntegrationChunkBlock.Head();
        if (!recycler->IntegrateBlock<NoBit>(record.blockAddress, record.segment, GetChunkAllocSize(), sizeof(CodeGenNumberChunk)))
        {TRACE_IT(1514);
            Js::Throw::OutOfMemory();
        }
#if DBG && GLOBAL_ENABLE_WRITE_BARRIER
        if (CONFIG_FLAG(ForceSoftwareWriteBarrier) && CONFIG_FLAG(RecyclerVerifyMark))
        {TRACE_IT(1515);
            Recycler::WBSetBitRange(record.blockAddress, BlockSize / sizeof(void*));
        }
#endif
        pendingIntegrationChunkBlock.RemoveHead(&NoThrowHeapAllocator::Instance);
    }
#ifdef TRACK_ALLOC
    Assert(recycler->nextAllocData.IsEmpty());
    recycler->nextAllocData = oldAllocData;
#endif
}

void
CodeGenNumberThreadAllocator::FlushAllocations()
{TRACE_IT(1516);
    AutoCriticalSection autocs(&cs);
    pendingFlushNumberBlock.MoveTo(&pendingIntegrationNumberBlock);
    pendingFlushChunkBlock.MoveTo(&pendingIntegrationChunkBlock);
}

CodeGenNumberAllocator::CodeGenNumberAllocator(CodeGenNumberThreadAllocator * threadAlloc, Recycler * recycler) :
    threadAlloc(threadAlloc), recycler(recycler), chunk(nullptr), chunkTail(nullptr), currentChunkNumberCount(CodeGenNumberChunk::MaxNumberCount)
{TRACE_IT(1517);
#if DBG
    finalized = false;
#endif
}

// We should never call this function if we are using tagged float
Js::JavascriptNumber *
CodeGenNumberAllocator::Alloc()
{TRACE_IT(1518);
    Assert(!finalized);
    if (currentChunkNumberCount == CodeGenNumberChunk::MaxNumberCount)
    {TRACE_IT(1519);
        CodeGenNumberChunk * newChunk = threadAlloc? threadAlloc->AllocChunk()
            : RecyclerNewStructZ(recycler, CodeGenNumberChunk);
        // Need to always put the new chunk last, as when we flush
        // pages, new chunk's page might not be full yet, and won't
        // be flushed, and we will have a broken link in the link list.
        newChunk->next = nullptr;
        if (this->chunkTail != nullptr)
        {TRACE_IT(1520);
            this->chunkTail->next = newChunk;
        }
        else
        {TRACE_IT(1521);
            this->chunk = newChunk;
        }
        this->chunkTail = newChunk;
        this->currentChunkNumberCount = 0;
    }
    Js::JavascriptNumber * newNumber = threadAlloc? threadAlloc->AllocNumber()
        : Js::JavascriptNumber::NewUninitialized(recycler);
    this->chunkTail->numbers[this->currentChunkNumberCount++] = newNumber;
    return newNumber;
}


CodeGenNumberChunk *
CodeGenNumberAllocator::Finalize()
{TRACE_IT(1522);
    Assert(!finalized);
#if DBG
    finalized = true;
#endif
    CodeGenNumberChunk * finalizedChunk = this->chunk;
    this->chunk = nullptr;
    this->chunkTail = nullptr;
    this->currentChunkNumberCount = 0;
    return finalizedChunk;
}


uint XProcNumberPageSegmentImpl::sizeCat = sizeof(Js::JavascriptNumber);
Js::JavascriptNumber* XProcNumberPageSegmentImpl::AllocateNumber(Func* func, double value)
{TRACE_IT(1523);
    HANDLE hProcess = func->GetThreadContextInfo()->GetProcessHandle();

    XProcNumberPageSegmentImpl* tail = this;

    if (this->pageAddress != 0)
    {TRACE_IT(1524);
        while (tail->nextSegment)
        {TRACE_IT(1525);
            tail = (XProcNumberPageSegmentImpl*)tail->nextSegment;
        }

        if (tail->pageAddress + tail->committedEnd - tail->allocEndAddress >= sizeCat)
        {TRACE_IT(1526);
            auto number = tail->allocEndAddress;
            tail->allocEndAddress += sizeCat;

#if DBG
            Js::JavascriptNumber localNumber(value, (Js::StaticType*)func->GetScriptContextInfo()->GetNumberTypeStaticAddr(), true);
#else
            Js::JavascriptNumber localNumber(value, (Js::StaticType*)func->GetScriptContextInfo()->GetNumberTypeStaticAddr());
#endif
            Js::JavascriptNumber* pLocalNumber = &localNumber;

#ifdef RECYCLER_MEMORY_VERIFY
            if (func->GetScriptContextInfo()->IsRecyclerVerifyEnabled())
            {TRACE_IT(1527);
                pLocalNumber = (Js::JavascriptNumber*)alloca(sizeCat);
                memset(pLocalNumber, Recycler::VerifyMemFill, sizeCat);
                Recycler::FillPadNoCheck(pLocalNumber, sizeof(Js::JavascriptNumber), sizeCat, false);
                pLocalNumber = new (pLocalNumber) Js::JavascriptNumber(localNumber);
            }
#else
            Assert(sizeCat == sizeof(Js::JavascriptNumber));
            __analysis_assume(sizeCat == sizeof(Js::JavascriptNumber));
#endif
            // change vtable to the remote one
            *(void**)pLocalNumber = (void*)func->GetScriptContextInfo()->GetVTableAddress(VTableValue::VtableJavascriptNumber);

            // initialize number by WriteProcessMemory
            if (!WriteProcessMemory(hProcess, (void*)number, pLocalNumber, sizeCat, NULL))
            {TRACE_IT(1528);
                MemoryOperationLastError::RecordLastErrorAndThrow();
            }

            return (Js::JavascriptNumber*) number;
        }

        // alloc blocks
        if (tail->GetCommitEndAddress() < tail->GetEndAddress())
        {TRACE_IT(1529);
            Assert((unsigned int)((char*)tail->GetEndAddress() - (char*)tail->GetCommitEndAddress()) >= BlockSize);
            // TODO: implement guard pages (still necessary for OOP JIT?)
            LPVOID addr = ::VirtualAllocEx(hProcess, tail->GetCommitEndAddress(), BlockSize, MEM_COMMIT, PAGE_READWRITE);
            if (addr == nullptr)
            {TRACE_IT(1530);
                MemoryOperationLastError::RecordLastError();
                Js::Throw::OutOfMemory();
            }
            tail->committedEnd += BlockSize;
            return AllocateNumber(func, value);
        }
    }

    // alloc new segment
    void* pages = ::VirtualAllocEx(hProcess, nullptr, PageCount * AutoSystemInfo::PageSize, MEM_RESERVE, PAGE_READWRITE);
    if (pages == nullptr)
    {TRACE_IT(1531);
        MemoryOperationLastError::RecordLastError();
        Js::Throw::OutOfMemory();
    }

    if (tail->pageAddress == 0)
    {TRACE_IT(1532);
        tail->pageAddress = (intptr_t)pages;
        tail->allocStartAddress = this->pageAddress;
        tail->allocEndAddress = this->pageAddress;
        tail->nextSegment = nullptr;
        return AllocateNumber(func, value);
    }
    else
    {TRACE_IT(1533);
        XProcNumberPageSegmentImpl* seg = (XProcNumberPageSegmentImpl*)midl_user_allocate(sizeof(XProcNumberPageSegment));
        if (seg == nullptr)
        {TRACE_IT(1534);
            Js::Throw::OutOfMemory();
        }
        seg = new (seg) XProcNumberPageSegmentImpl();
        tail->nextSegment = seg;
        return seg->AllocateNumber(func, value);
    }
}


XProcNumberPageSegmentImpl::XProcNumberPageSegmentImpl()
{
    memset(this, 0, sizeof(XProcNumberPageSegment));
}

void XProcNumberPageSegmentImpl::Initialize(bool recyclerVerifyEnabled, uint recyclerVerifyPad)
{TRACE_IT(1535);
    uint allocSize = (uint)sizeof(Js::JavascriptNumber);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    allocSize += Js::Configuration::Global.flags.NumberAllocPlusSize;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    // TODO: share same pad size with main process
    if (recyclerVerifyEnabled)
    {TRACE_IT(1536);
        uint padAllocSize = (uint)AllocSizeMath::Add(sizeof(Js::JavascriptNumber) + sizeof(size_t), recyclerVerifyPad);
        allocSize = padAllocSize < allocSize ? allocSize : padAllocSize;
    }
#endif

    allocSize = (uint)HeapInfo::GetAlignedSizeNoCheck(allocSize);

    if (BlockSize%allocSize != 0)
    {TRACE_IT(1537);
        // align allocation sizeCat to be 2^n to make integration easier
        allocSize = BlockSize / (1 << (Math::Log2((size_t)BlockSize / allocSize)));
    }

    sizeCat = allocSize;
}

Field(Js::JavascriptNumber*)* ::XProcNumberPageSegmentManager::RegisterSegments(XProcNumberPageSegment* segments)
{TRACE_IT(1538);
    Assert(segments->pageAddress && segments->allocStartAddress && segments->allocEndAddress);
    XProcNumberPageSegmentImpl* segmentImpl = (XProcNumberPageSegmentImpl*)segments;

    XProcNumberPageSegmentImpl* temp = segmentImpl;
    size_t totalCount = 0;
    while (temp)
    {TRACE_IT(1539);
        totalCount += (temp->allocEndAddress - temp->allocStartAddress) / XProcNumberPageSegmentImpl::sizeCat;
        temp = (XProcNumberPageSegmentImpl*)temp->nextSegment;
    }

    Field(Js::JavascriptNumber*)* numbers = RecyclerNewArray(this->recycler, Field(Js::JavascriptNumber*), totalCount);

    temp = segmentImpl;
    int count = 0;
    while (temp)
    {TRACE_IT(1540);
        while (temp->allocStartAddress < temp->allocEndAddress)
        {TRACE_IT(1541);
            numbers[count] = (Js::JavascriptNumber*)temp->allocStartAddress;
            count++;
            temp->allocStartAddress += XProcNumberPageSegmentImpl::sizeCat;
        }
        temp = (XProcNumberPageSegmentImpl*)temp->nextSegment;
    }

    AutoCriticalSection autoCS(&cs);
    if (this->segmentsList == nullptr)
    {TRACE_IT(1542);
        this->segmentsList = segmentImpl;
    }
    else
    {TRACE_IT(1543);
        temp = segmentsList;
        while (temp->nextSegment)
        {TRACE_IT(1544);
            temp = (XProcNumberPageSegmentImpl*)temp->nextSegment;
        }
        temp->nextSegment = segmentImpl;
    }

    return numbers;
}

XProcNumberPageSegment * XProcNumberPageSegmentManager::GetFreeSegment(Memory::ArenaAllocator* alloc)
{TRACE_IT(1545);
    AutoCriticalSection autoCS(&cs);

    auto temp = segmentsList;
    auto prev = &segmentsList;
    while (temp)
    {TRACE_IT(1546);
        if (temp->allocEndAddress != temp->pageAddress + (int)(XProcNumberPageSegmentImpl::PageCount*AutoSystemInfo::PageSize)) // not full
        {TRACE_IT(1547);
            *prev = (XProcNumberPageSegmentImpl*)temp->nextSegment;

            // remove from the list
            XProcNumberPageSegment * seg = (XProcNumberPageSegment *)AnewStructZ(alloc, XProcNumberPageSegmentImpl);
            temp->nextSegment = 0;
            memcpy(seg, temp, sizeof(XProcNumberPageSegment));
            midl_user_free(temp);
            return seg;
        }
        prev = (XProcNumberPageSegmentImpl**)&temp->nextSegment;
        temp = (XProcNumberPageSegmentImpl*)temp->nextSegment;
    }

    return nullptr;
}

void XProcNumberPageSegmentManager::Integrate()
{TRACE_IT(1548);
    AutoCriticalSection autoCS(&cs);

    auto temp = this->segmentsList;
    auto prev = &this->segmentsList;
    while (temp)
    {TRACE_IT(1549);
        if((uintptr_t)temp->allocEndAddress - (uintptr_t)temp->pageAddress > temp->blockIntegratedSize + XProcNumberPageSegmentImpl::BlockSize)
        {TRACE_IT(1550);
            if (temp->pageSegment == 0)
            {TRACE_IT(1551);
                auto leafPageAllocator = recycler->GetRecyclerLeafPageAllocator();
                DListBase<PageSegment> segmentList;
                temp->pageSegment = (intptr_t)leafPageAllocator->AllocPageSegment(segmentList, leafPageAllocator,
                    (void*)temp->pageAddress, XProcNumberPageSegmentImpl::PageCount, temp->committedEnd / AutoSystemInfo::PageSize, false);

                if (temp->pageSegment)
                {TRACE_IT(1552);
                    leafPageAllocator->IntegrateSegments(segmentList, 1, XProcNumberPageSegmentImpl::PageCount);
                    this->integratedSegmentCount++;
                }
            }

            if (temp->pageSegment)
            {TRACE_IT(1553);
                unsigned int minIntegrateSize = XProcNumberPageSegmentImpl::BlockSize;
                for (; temp->pageAddress + temp->blockIntegratedSize + minIntegrateSize < (unsigned int)temp->allocEndAddress;
                    temp->blockIntegratedSize += minIntegrateSize)
                {
                    TRACK_ALLOC_INFO(recycler, Js::JavascriptNumber, Recycler, 0, (size_t)-1);

                    if (!recycler->IntegrateBlock<LeafBit>((char*)temp->pageAddress + temp->blockIntegratedSize,
                        (PageSegment*)temp->pageSegment, XProcNumberPageSegmentImpl::sizeCat, sizeof(Js::JavascriptNumber)))
                    {TRACE_IT(1554);
                        Js::Throw::OutOfMemory();
                    }
                }

                if ((uintptr_t)temp->allocEndAddress + XProcNumberPageSegmentImpl::sizeCat
                    > (uintptr_t)temp->pageAddress + XProcNumberPageSegmentImpl::PageCount*AutoSystemInfo::PageSize)
                {TRACE_IT(1555);
                    *prev = (XProcNumberPageSegmentImpl*)temp->nextSegment;
                    midl_user_free(temp);
                    temp = *prev;
                    continue;
                }
            }
        }

        temp = (XProcNumberPageSegmentImpl*)temp->nextSegment;
    }
}

XProcNumberPageSegmentManager::XProcNumberPageSegmentManager(Recycler* recycler)
    :segmentsList(nullptr), recycler(recycler), integratedSegmentCount(0)
{TRACE_IT(1556);
#ifdef RECYCLER_MEMORY_VERIFY
    XProcNumberPageSegmentImpl::Initialize(recycler->VerifyEnabled() == TRUE, recycler->GetVerifyPad());
#else
    XProcNumberPageSegmentImpl::Initialize(false, 0);
#endif
}

XProcNumberPageSegmentManager::~XProcNumberPageSegmentManager()
{TRACE_IT(1557);
    auto temp = segmentsList;
    while (temp)
    {TRACE_IT(1558);
        auto next = temp->nextSegment;
        midl_user_free(temp);
        temp = (XProcNumberPageSegmentImpl*)next;
    }
}
#endif