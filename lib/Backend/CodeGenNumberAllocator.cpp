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
{LOGMEIN("CodeGenNumberAllocator.cpp] 13\n");
}

CodeGenNumberThreadAllocator::~CodeGenNumberThreadAllocator()
{LOGMEIN("CodeGenNumberAllocator.cpp] 17\n");
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 29\n");
#ifdef RECYCLER_MEMORY_VERIFY
    if (recycler->VerifyEnabled())
    {LOGMEIN("CodeGenNumberAllocator.cpp] 32\n");
        return HeapInfo::GetAlignedSize(AllocSizeMath::Add(sizeof(Js::JavascriptNumber) + sizeof(size_t), recycler->verifyPad));
    }
#endif
    return HeapInfo::GetAlignedSizeNoCheck(sizeof(Js::JavascriptNumber));
}


size_t
CodeGenNumberThreadAllocator::GetChunkAllocSize()
{LOGMEIN("CodeGenNumberAllocator.cpp] 42\n");
#ifdef RECYCLER_MEMORY_VERIFY
    if (recycler->VerifyEnabled())
    {LOGMEIN("CodeGenNumberAllocator.cpp] 45\n");
        return HeapInfo::GetAlignedSize(AllocSizeMath::Add(sizeof(CodeGenNumberChunk) + sizeof(size_t), recycler->verifyPad));
    }
#endif
    return HeapInfo::GetAlignedSizeNoCheck(sizeof(CodeGenNumberChunk));
}

Js::JavascriptNumber *
CodeGenNumberThreadAllocator::AllocNumber()
{LOGMEIN("CodeGenNumberAllocator.cpp] 54\n");
    AutoCriticalSection autocs(&cs);
    size_t sizeCat = GetNumberAllocSize();
    if (nextNumber + sizeCat > currentNumberBlockEnd)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 58\n");
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 72\n");
    AutoCriticalSection autocs(&cs);
    size_t sizeCat = GetChunkAllocSize();
    if (nextChunk + sizeCat > currentChunkBlockEnd)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 76\n");
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 91\n");
    Assert(cs.IsLocked());
    Assert(nextNumber + GetNumberAllocSize() > currentNumberBlockEnd);
    if (hasNewNumberBlock)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 95\n");
        if (!pendingReferenceNumberBlock.PrependNode(&NoThrowHeapAllocator::Instance,
            currentNumberBlockEnd - BlockSize, currentNumberSegment))
        {LOGMEIN("CodeGenNumberAllocator.cpp] 98\n");
            Js::Throw::OutOfMemory();
        }
        hasNewNumberBlock = false;
    }

    if (currentNumberBlockEnd == numberSegmentEnd)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 105\n");
        Assert(cs.IsLocked());
        // Reserve the segment, but not committing it
        currentNumberSegment = PageAllocator::AllocPageSegment(pendingIntegrationNumberSegment, this->recycler->GetRecyclerLeafPageAllocator(), false, true, false);
        if (currentNumberSegment == nullptr)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 110\n");
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
    {LOGMEIN("CodeGenNumberAllocator.cpp] 124\n");
        Js::Throw::OutOfMemory();
    }
    nextNumber = currentNumberBlockEnd;
    currentNumberBlockEnd += BlockSize;
    hasNewNumberBlock = true;
    this->recycler->GetRecyclerLeafPageAllocator()->FillAllocPages(nextNumber, 1);
}

void
CodeGenNumberThreadAllocator::AllocNewChunkBlock()
{LOGMEIN("CodeGenNumberAllocator.cpp] 135\n");
    Assert(cs.IsLocked());
    Assert(nextChunk + GetChunkAllocSize() > currentChunkBlockEnd);
    if (hasNewChunkBlock)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 139\n");
        if (!pendingFlushChunkBlock.PrependNode(&NoThrowHeapAllocator::Instance,
            currentChunkBlockEnd - BlockSize, currentChunkSegment))
        {LOGMEIN("CodeGenNumberAllocator.cpp] 142\n");
            Js::Throw::OutOfMemory();
        }
        // All integrated pages' object are all live initially, so don't need to rescan them
        // todo: SWB: need to allocate number with write barrier pages
        ::ResetWriteWatch(currentChunkBlockEnd - BlockSize, BlockSize);
        pendingReferenceNumberBlock.MoveTo(&pendingFlushNumberBlock);
        hasNewChunkBlock = false;
    }

    if (currentChunkBlockEnd == chunkSegmentEnd)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 153\n");
        Assert(cs.IsLocked());
        // Reserve the segment, but not committing it
        currentChunkSegment = PageAllocator::AllocPageSegment(pendingIntegrationChunkSegment, this->recycler->GetRecyclerPageAllocator(), false, true, false);
        if (currentChunkSegment == nullptr)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 158\n");
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
    {LOGMEIN("CodeGenNumberAllocator.cpp] 172\n");
        Js::Throw::OutOfMemory();
    }

    nextChunk = currentChunkBlockEnd;
    currentChunkBlockEnd += BlockSize;
    hasNewChunkBlock = true;
    this->recycler->GetRecyclerLeafPageAllocator()->FillAllocPages(nextChunk, 1);
}

void
CodeGenNumberThreadAllocator::Integrate()
{LOGMEIN("CodeGenNumberAllocator.cpp] 184\n");
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
        {LOGMEIN("CodeGenNumberAllocator.cpp] 205\n");
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
        {LOGMEIN("CodeGenNumberAllocator.cpp] 220\n");
            Js::Throw::OutOfMemory();
        }
#if DBG && GLOBAL_ENABLE_WRITE_BARRIER
        if (CONFIG_FLAG(ForceSoftwareWriteBarrier) && CONFIG_FLAG(RecyclerVerifyMark))
        {LOGMEIN("CodeGenNumberAllocator.cpp] 225\n");
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 239\n");
    AutoCriticalSection autocs(&cs);
    pendingFlushNumberBlock.MoveTo(&pendingIntegrationNumberBlock);
    pendingFlushChunkBlock.MoveTo(&pendingIntegrationChunkBlock);
}

CodeGenNumberAllocator::CodeGenNumberAllocator(CodeGenNumberThreadAllocator * threadAlloc, Recycler * recycler) :
    threadAlloc(threadAlloc), recycler(recycler), chunk(nullptr), chunkTail(nullptr), currentChunkNumberCount(CodeGenNumberChunk::MaxNumberCount)
{LOGMEIN("CodeGenNumberAllocator.cpp] 247\n");
#if DBG
    finalized = false;
#endif
}

// We should never call this function if we are using tagged float
Js::JavascriptNumber *
CodeGenNumberAllocator::Alloc()
{LOGMEIN("CodeGenNumberAllocator.cpp] 256\n");
    Assert(!finalized);
    if (currentChunkNumberCount == CodeGenNumberChunk::MaxNumberCount)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 259\n");
        CodeGenNumberChunk * newChunk = threadAlloc? threadAlloc->AllocChunk()
            : RecyclerNewStructZ(recycler, CodeGenNumberChunk);
        // Need to always put the new chunk last, as when we flush
        // pages, new chunk's page might not be full yet, and won't
        // be flushed, and we will have a broken link in the link list.
        newChunk->next = nullptr;
        if (this->chunkTail != nullptr)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 267\n");
            this->chunkTail->next = newChunk;
        }
        else
        {
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 286\n");
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 301\n");
    HANDLE hProcess = func->GetThreadContextInfo()->GetProcessHandle();

    XProcNumberPageSegmentImpl* tail = this;

    if (this->pageAddress != 0)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 307\n");
        while (tail->nextSegment)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 309\n");
            tail = (XProcNumberPageSegmentImpl*)tail->nextSegment;
        }

        if (tail->pageAddress + tail->committedEnd - tail->allocEndAddress >= sizeCat)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 314\n");
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
            {LOGMEIN("CodeGenNumberAllocator.cpp] 327\n");
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
            {LOGMEIN("CodeGenNumberAllocator.cpp] 342\n");
                MemoryOperationLastError::RecordLastErrorAndThrow();
            }

            return (Js::JavascriptNumber*) number;
        }

        // alloc blocks
        if (tail->GetCommitEndAddress() < tail->GetEndAddress())
        {LOGMEIN("CodeGenNumberAllocator.cpp] 351\n");
            Assert((unsigned int)((char*)tail->GetEndAddress() - (char*)tail->GetCommitEndAddress()) >= BlockSize);
            // TODO: implement guard pages (still necessary for OOP JIT?)
            LPVOID addr = ::VirtualAllocEx(hProcess, tail->GetCommitEndAddress(), BlockSize, MEM_COMMIT, PAGE_READWRITE);
            if (addr == nullptr)
            {LOGMEIN("CodeGenNumberAllocator.cpp] 356\n");
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
    {LOGMEIN("CodeGenNumberAllocator.cpp] 368\n");
        MemoryOperationLastError::RecordLastError();
        Js::Throw::OutOfMemory();
    }

    if (tail->pageAddress == 0)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 374\n");
        tail->pageAddress = (intptr_t)pages;
        tail->allocStartAddress = this->pageAddress;
        tail->allocEndAddress = this->pageAddress;
        tail->nextSegment = nullptr;
        return AllocateNumber(func, value);
    }
    else
    {
        XProcNumberPageSegmentImpl* seg = (XProcNumberPageSegmentImpl*)midl_user_allocate(sizeof(XProcNumberPageSegment));
        if (seg == nullptr)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 385\n");
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 401\n");
    uint allocSize = (uint)sizeof(Js::JavascriptNumber);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    allocSize += Js::Configuration::Global.flags.NumberAllocPlusSize;
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    // TODO: share same pad size with main process
    if (recyclerVerifyEnabled)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 409\n");
        uint padAllocSize = (uint)AllocSizeMath::Add(sizeof(Js::JavascriptNumber) + sizeof(size_t), recyclerVerifyPad);
        allocSize = padAllocSize < allocSize ? allocSize : padAllocSize;
    }
#endif

    allocSize = (uint)HeapInfo::GetAlignedSizeNoCheck(allocSize);

    if (BlockSize%allocSize != 0)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 418\n");
        // align allocation sizeCat to be 2^n to make integration easier
        allocSize = BlockSize / (1 << (Math::Log2((size_t)BlockSize / allocSize)));
    }

    sizeCat = allocSize;
}

Field(Js::JavascriptNumber*)* ::XProcNumberPageSegmentManager::RegisterSegments(XProcNumberPageSegment* segments)
{LOGMEIN("CodeGenNumberAllocator.cpp] 427\n");
    Assert(segments->pageAddress && segments->allocStartAddress && segments->allocEndAddress);
    XProcNumberPageSegmentImpl* segmentImpl = (XProcNumberPageSegmentImpl*)segments;

    XProcNumberPageSegmentImpl* temp = segmentImpl;
    size_t totalCount = 0;
    while (temp)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 434\n");
        totalCount += (temp->allocEndAddress - temp->allocStartAddress) / XProcNumberPageSegmentImpl::sizeCat;
        temp = (XProcNumberPageSegmentImpl*)temp->nextSegment;
    }

    Field(Js::JavascriptNumber*)* numbers = RecyclerNewArray(this->recycler, Field(Js::JavascriptNumber*), totalCount);

    temp = segmentImpl;
    int count = 0;
    while (temp)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 444\n");
        while (temp->allocStartAddress < temp->allocEndAddress)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 446\n");
            numbers[count] = (Js::JavascriptNumber*)temp->allocStartAddress;
            count++;
            temp->allocStartAddress += XProcNumberPageSegmentImpl::sizeCat;
        }
        temp = (XProcNumberPageSegmentImpl*)temp->nextSegment;
    }

    AutoCriticalSection autoCS(&cs);
    if (this->segmentsList == nullptr)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 456\n");
        this->segmentsList = segmentImpl;
    }
    else
    {
        temp = segmentsList;
        while (temp->nextSegment)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 463\n");
            temp = (XProcNumberPageSegmentImpl*)temp->nextSegment;
        }
        temp->nextSegment = segmentImpl;
    }

    return numbers;
}

XProcNumberPageSegment * XProcNumberPageSegmentManager::GetFreeSegment(Memory::ArenaAllocator* alloc)
{LOGMEIN("CodeGenNumberAllocator.cpp] 473\n");
    AutoCriticalSection autoCS(&cs);

    auto temp = segmentsList;
    auto prev = &segmentsList;
    while (temp)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 479\n");
        if (temp->allocEndAddress != temp->pageAddress + (int)(XProcNumberPageSegmentImpl::PageCount*AutoSystemInfo::PageSize)) // not full
        {LOGMEIN("CodeGenNumberAllocator.cpp] 481\n");
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 499\n");
    AutoCriticalSection autoCS(&cs);

    auto temp = this->segmentsList;
    auto prev = &this->segmentsList;
    while (temp)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 505\n");
        if((uintptr_t)temp->allocEndAddress - (uintptr_t)temp->pageAddress > temp->blockIntegratedSize + XProcNumberPageSegmentImpl::BlockSize)
        {LOGMEIN("CodeGenNumberAllocator.cpp] 507\n");
            if (temp->pageSegment == 0)
            {LOGMEIN("CodeGenNumberAllocator.cpp] 509\n");
                auto leafPageAllocator = recycler->GetRecyclerLeafPageAllocator();
                DListBase<PageSegment> segmentList;
                temp->pageSegment = (intptr_t)leafPageAllocator->AllocPageSegment(segmentList, leafPageAllocator,
                    (void*)temp->pageAddress, XProcNumberPageSegmentImpl::PageCount, temp->committedEnd / AutoSystemInfo::PageSize, false);

                if (temp->pageSegment)
                {LOGMEIN("CodeGenNumberAllocator.cpp] 516\n");
                    leafPageAllocator->IntegrateSegments(segmentList, 1, XProcNumberPageSegmentImpl::PageCount);
                    this->integratedSegmentCount++;
                }
            }

            if (temp->pageSegment)
            {LOGMEIN("CodeGenNumberAllocator.cpp] 523\n");
                unsigned int minIntegrateSize = XProcNumberPageSegmentImpl::BlockSize;
                for (; temp->pageAddress + temp->blockIntegratedSize + minIntegrateSize < (unsigned int)temp->allocEndAddress;
                    temp->blockIntegratedSize += minIntegrateSize)
                {
                    TRACK_ALLOC_INFO(recycler, Js::JavascriptNumber, Recycler, 0, (size_t)-1);

                    if (!recycler->IntegrateBlock<LeafBit>((char*)temp->pageAddress + temp->blockIntegratedSize,
                        (PageSegment*)temp->pageSegment, XProcNumberPageSegmentImpl::sizeCat, sizeof(Js::JavascriptNumber)))
                    {LOGMEIN("CodeGenNumberAllocator.cpp] 532\n");
                        Js::Throw::OutOfMemory();
                    }
                }

                if ((uintptr_t)temp->allocEndAddress + XProcNumberPageSegmentImpl::sizeCat
                    > (uintptr_t)temp->pageAddress + XProcNumberPageSegmentImpl::PageCount*AutoSystemInfo::PageSize)
                {LOGMEIN("CodeGenNumberAllocator.cpp] 539\n");
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
{LOGMEIN("CodeGenNumberAllocator.cpp] 554\n");
#ifdef RECYCLER_MEMORY_VERIFY
    XProcNumberPageSegmentImpl::Initialize(recycler->VerifyEnabled() == TRUE, recycler->GetVerifyPad());
#else
    XProcNumberPageSegmentImpl::Initialize(false, 0);
#endif
}

XProcNumberPageSegmentManager::~XProcNumberPageSegmentManager()
{LOGMEIN("CodeGenNumberAllocator.cpp] 563\n");
    auto temp = segmentsList;
    while (temp)
    {LOGMEIN("CodeGenNumberAllocator.cpp] 566\n");
        auto next = temp->nextSegment;
        midl_user_free(temp);
        temp = (XProcNumberPageSegmentImpl*)next;
    }
}
#endif