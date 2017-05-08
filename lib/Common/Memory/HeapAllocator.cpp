//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonMemoryPch.h"

#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
// Not enabled in ChakraCore
#include "MemProtectHeap.h"
#endif

// Initialization order
//  AB AutoSystemInfo
//  AD PerfCounter
//  AE PerfCounterSet
//  AM Output/Configuration
//  AN MemProtectHeap
//  AP DbgHelpSymbolManager
//  AQ CFGLogger
//  AR LeakReport
//  AS JavascriptDispatch/RecyclerObjectDumper
//  AT HeapAllocator/RecyclerHeuristic
//  AU RecyclerWriteBarrierManager
#pragma warning(disable:4075)       // initializers put in unrecognized initialization area on purpose
#pragma init_seg(".CRT$XCAT")

#ifdef HEAP_TRACK_ALLOC
CriticalSection HeapAllocator::cs;
#endif

#ifdef CHECK_MEMORY_LEAK
MemoryLeakCheck MemoryLeakCheck::leakCheck;
#endif

HeapAllocator HeapAllocator::Instance;
NoThrowHeapAllocator NoThrowHeapAllocator::Instance;
NoCheckHeapAllocator NoCheckHeapAllocator::Instance;
HANDLE NoCheckHeapAllocator::processHeap = nullptr;

template <bool noThrow>
char * HeapAllocator::AllocT(size_t byteSize)
{TRACE_IT(23241);
#ifdef HEAP_TRACK_ALLOC
    size_t requestedBytes = byteSize;
    byteSize = AllocSizeMath::Add(requestedBytes, ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT));
    TrackAllocData allocData;
    ClearTrackAllocInfo(&allocData);
#elif defined(HEAP_PERF_COUNTERS)
    size_t requestedBytes = byteSize;
    byteSize = AllocSizeMath::Add(requestedBytes, ::Math::Align<size_t>(sizeof(size_t), MEMORY_ALLOCATION_ALIGNMENT));
#endif

    if (noThrow)
    {TRACE_IT(23242);
        FAULTINJECT_MEMORY_NOTHROW(_u("Heap"), byteSize);
    }
    else
    {TRACE_IT(23243);
        FAULTINJECT_MEMORY_THROW(_u("Heap"), byteSize);
    }

    char * buffer;
#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    if (DoUseMemProtectHeap())
    {TRACE_IT(23244);
        void * memory = MemProtectHeapRootAlloc(memProtectHeapHandle, byteSize);
        if (memory == nullptr)
        {TRACE_IT(23245);
            if (noThrow)
            {TRACE_IT(23246);
                return nullptr;
            }
            Js::Throw::OutOfMemory();
        }
        buffer = (char *)memory;
    }
    else
#endif
    {TRACE_IT(23247);
        if (CONFIG_FLAG(PrivateHeap))
        {TRACE_IT(23248);
            buffer = (char *)HeapAlloc(GetPrivateHeap(), 0, byteSize);
        }
        else
        {TRACE_IT(23249);
            buffer = (char *)malloc(byteSize);
        }
    }

    if (!noThrow && buffer == nullptr)
    {TRACE_IT(23250);
        Js::Throw::OutOfMemory();
    }

#if defined(HEAP_TRACK_ALLOC) || defined(HEAP_PERF_COUNTERS)
    if (!noThrow || buffer != nullptr)
    {TRACE_IT(23251);
#ifdef HEAP_TRACK_ALLOC
        cs.Enter();
        data.LogAlloc((HeapAllocRecord *)buffer, requestedBytes, allocData);
        cs.Leave();
        buffer += ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT);
#else
        *(size_t *)buffer = requestedBytes;
        buffer += ::Math::Align<size_t>(sizeof(size_t), MEMORY_ALLOCATION_ALIGNMENT);

#endif
        HEAP_PERF_COUNTER_INC(LiveObject);
        HEAP_PERF_COUNTER_ADD(LiveObjectSize, requestedBytes);
    }
#endif
    return buffer;
}

template char * HeapAllocator::AllocT<true>(size_t byteSize);
template char * HeapAllocator::AllocT<false>(size_t byteSize);


void HeapAllocator::Free(void * buffer, size_t byteSize)
{TRACE_IT(23252);
#ifdef HEAP_TRACK_ALLOC
    if (buffer != nullptr)
    {TRACE_IT(23253);
        HeapAllocRecord * record = (HeapAllocRecord *)(((char *)buffer) - ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT));
        Assert(byteSize == (size_t)-1 || record->size == byteSize);

        HEAP_PERF_COUNTER_DEC(LiveObject);
        HEAP_PERF_COUNTER_SUB(LiveObjectSize, record->size);

        cs.Enter();
        data.LogFree(record);
        cs.Leave();

        buffer = record;
#if DBG
        memset(buffer, DbgMemFill, record->size + ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT));
#endif
    }
#elif defined(HEAP_PERF_COUNTERS)
    if (buffer != nullptr)
    {TRACE_IT(23254);
        HEAP_PERF_COUNTER_DEC(LiveObject);
        size_t * allocSize = (size_t *)(((char *)buffer) - ::Math::Align<size_t>(sizeof(size_t), MEMORY_ALLOCATION_ALIGNMENT));
        HEAP_PERF_COUNTER_SUB(LiveObjectSize, *allocSize);
        buffer = allocSize;
    }
#endif
#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    if (DoUseMemProtectHeap())
    {TRACE_IT(23255);
        HRESULT hr = MemProtectHeapUnrootAndZero(memProtectHeapHandle, buffer);
        Assert(SUCCEEDED(hr));
        return;
    }
#endif

    if (CONFIG_FLAG(PrivateHeap))
    {TRACE_IT(23256);
        HeapFree(GetPrivateHeap(), 0, buffer);
    }
    else
    {TRACE_IT(23257);
        free(buffer);
    }
}

void HeapAllocator::InitPrivateHeap()
{TRACE_IT(23258);
    if (this->m_privateHeap == nullptr)
    {TRACE_IT(23259);
        this->m_privateHeap = HeapCreate(0, 0, 0); // no options, default initial size, no max size
    }
}

void HeapAllocator::DestroyPrivateHeap()
{TRACE_IT(23260);
    if (this->m_privateHeap != nullptr)
    {TRACE_IT(23261);
        // xplat-todo: PAL no HeapDestroy?
#ifdef _WIN32
        BOOL success = HeapDestroy(this->m_privateHeap);
        Assert(success);
#endif
        this->m_privateHeap = nullptr;
    }
}

HANDLE HeapAllocator::GetPrivateHeap()
{TRACE_IT(23262);
    InitPrivateHeap(); // will initialize PrivateHeap if not already initialized
    return this->m_privateHeap;
}

#ifdef TRACK_ALLOC
#ifdef HEAP_TRACK_ALLOC
THREAD_LOCAL TrackAllocData HeapAllocator::nextAllocData;
#endif

HeapAllocator * HeapAllocator::TrackAllocInfo(TrackAllocData const& data)
{TRACE_IT(23263);
#ifdef HEAP_TRACK_ALLOC
    Assert(nextAllocData.IsEmpty());
    nextAllocData = data;
#endif
    return this;
}

void HeapAllocator::ClearTrackAllocInfo(TrackAllocData* data/* = NULL*/)
{TRACE_IT(23264);
#ifdef HEAP_TRACK_ALLOC
    Assert(!nextAllocData.IsEmpty());
    if (data)
    {TRACE_IT(23265);
        *data = nextAllocData;
    }
    nextAllocData.Clear();
#endif
}
#endif

#ifdef HEAP_TRACK_ALLOC
//static
bool HeapAllocator::CheckLeaks()
{TRACE_IT(23266);
    return Instance.data.CheckLeaks();
}
#endif // HEAP_TRACK_ALLOC

char * NoThrowHeapAllocator::AllocZero(size_t byteSize)
{TRACE_IT(23267);
    return HeapAllocator::Instance.NoThrowAllocZero(byteSize);
}

char * NoThrowHeapAllocator::Alloc(size_t byteSize)
{TRACE_IT(23268);
    return HeapAllocator::Instance.NoThrowAlloc(byteSize);
}

void NoThrowHeapAllocator::Free(void * buffer, size_t byteSize)
{TRACE_IT(23269);
    HeapAllocator::Instance.Free(buffer, byteSize);
}

#ifdef TRACK_ALLOC
NoThrowHeapAllocator * NoThrowHeapAllocator::TrackAllocInfo(TrackAllocData const& data)
{TRACE_IT(23270);
    HeapAllocator::Instance.TrackAllocInfo(data);
    return this;
}
#endif // TRACK_ALLOC

#ifdef TRACK_ALLOC
void NoThrowHeapAllocator::ClearTrackAllocInfo(TrackAllocData* data /*= NULL*/)
{TRACE_IT(23271);
    HeapAllocator::Instance.ClearTrackAllocInfo(data);
}
#endif // TRACK_ALLOC

HeapAllocator * HeapAllocator::GetNoMemProtectInstance()
{TRACE_IT(23272);
#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    // Used only in Chakra, no need to use CUSTOM_CONFIG_FLAG
    if (CONFIG_FLAG(MemProtectHeap))
    {TRACE_IT(23273);
        return &NoMemProtectInstance;
    }
#endif
    return &Instance;
}

#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
HeapAllocator HeapAllocator::NoMemProtectInstance(false);

bool HeapAllocator::DoUseMemProtectHeap()
{TRACE_IT(23274);
    if (!allocMemProtect)
    {TRACE_IT(23275);
        return false;
    }

    if (memProtectHeapHandle != nullptr)
    {TRACE_IT(23276);
        return true;
    }

    DebugOnly(bool wasUsed = isUsed);
    isUsed = true;

    // Flag is used only in Chakra, no need to use CUSTOM_CONFIG_FLAG
    if (CONFIG_FLAG(MemProtectHeap))
    {TRACE_IT(23277);
        Assert(!wasUsed);
        if (FAILED(MemProtectHeapCreate(&memProtectHeapHandle, MemProtectHeapCreateFlags_ProtectCurrentStack)))
        {TRACE_IT(23278);
            Assert(false);
        }
        return true;
    }

    return false;
}

void HeapAllocator::FinishMemProtectHeapCollect()
{TRACE_IT(23279);
    if (memProtectHeapHandle)
    {
        MemProtectHeapCollect(memProtectHeapHandle, MemProtectHeap_ForceFinishCollect);
        DebugOnly(MemProtectHeapSetDisableConcurrentThreadExitedCheck(memProtectHeapHandle));
    }
}

NoThrowNoMemProtectHeapAllocator NoThrowNoMemProtectHeapAllocator::Instance;

char * NoThrowNoMemProtectHeapAllocator::AllocZero(size_t byteSize)
{TRACE_IT(23280);
    return HeapAllocator::GetNoMemProtectInstance()->NoThrowAllocZero(byteSize);
}

char * NoThrowNoMemProtectHeapAllocator::Alloc(size_t byteSize)
{TRACE_IT(23281);
    return HeapAllocator::GetNoMemProtectInstance()->NoThrowAlloc(byteSize);
}

void NoThrowNoMemProtectHeapAllocator::Free(void * buffer, size_t byteSize)
{TRACE_IT(23282);
    HeapAllocator::GetNoMemProtectInstance()->Free(buffer, byteSize);
}

#ifdef TRACK_ALLOC
NoThrowNoMemProtectHeapAllocator * NoThrowNoMemProtectHeapAllocator::TrackAllocInfo(TrackAllocData const& data)
{TRACE_IT(23283);
    HeapAllocator::GetNoMemProtectInstance()->TrackAllocInfo(data);
    return this;
}
#endif // TRACK_ALLOC

#ifdef TRACK_ALLOC
void NoThrowNoMemProtectHeapAllocator::ClearTrackAllocInfo(TrackAllocData* data /*= NULL*/)
{TRACE_IT(23284);
    HeapAllocator::GetNoMemProtectInstance()->ClearTrackAllocInfo(data);
}
#endif // TRACK_ALLOC
#endif

HeapAllocator::HeapAllocator(bool useAllocMemProtect)
    : m_privateHeap(nullptr)
#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    , isUsed(false)
    , memProtectHeapHandle(nullptr)
    , allocMemProtect(useAllocMemProtect)
#endif
{
    if (CONFIG_FLAG(PrivateHeap))
    {TRACE_IT(23285);
        this->InitPrivateHeap();
    }
}

HeapAllocator::~HeapAllocator()
{TRACE_IT(23286);
#ifdef HEAP_TRACK_ALLOC
    bool hasFakeHeapLeak = false;
    auto fakeHeapLeak = [&]()
    {
        // REVIEW: Okay to use global flags?
        if (Js::Configuration::Global.flags.ForceMemoryLeak && !hasFakeHeapLeak)
        {TRACE_IT(23287);
            AUTO_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
            struct FakeMemory { int f; };
            HeapNewStruct(FakeMemory);
            hasFakeHeapLeak = true;
        }
    };

#ifdef LEAK_REPORT
    // REVIEW: Okay to use global flags?
    if (Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag))
    {TRACE_IT(23288);
        fakeHeapLeak();
        LeakReport::StartSection(_u("Heap Leaks"));
        LeakReport::StartRedirectOutput();
        bool leaked = !HeapAllocator::CheckLeaks();
        LeakReport::EndRedirectOutput();
        LeakReport::EndSection();

        LeakReport::Print(_u("--------------------------------------------------------------------------------\n"));
        if (leaked)
        {TRACE_IT(23289);
            LeakReport::Print(_u("Heap Leaked Object: %d bytes (%d objects)\n"),
                data.outstandingBytes, data.allocCount - data.deleteCount);
        }
    }
#endif // LEAK_REPORT

#ifdef CHECK_MEMORY_LEAK
    // REVIEW: Okay to use global flags?
    if (Js::Configuration::Global.flags.CheckMemoryLeak)
    {TRACE_IT(23290);
        fakeHeapLeak();
        Output::CaptureStart();
        Output::Print(_u("-------------------------------------------------------------------------------------\n"));
        Output::Print(_u("Heap Leaks\n"));
        Output::Print(_u("-------------------------------------------------------------------------------------\n"));
        if (!HeapAllocator::CheckLeaks())
        {TRACE_IT(23291);
            Output::Print(_u("-------------------------------------------------------------------------------------\n"));
            Output::Print(_u("Heap Leaked Object: %d bytes (%d objects)\n"),
                data.outstandingBytes, data.allocCount - data.deleteCount);
            char16 * buffer = Output::CaptureEnd();
            MemoryLeakCheck::AddLeakDump(buffer, data.outstandingBytes, data.allocCount - data.deleteCount);
        }
        else
        {TRACE_IT(23292);
            free(Output::CaptureEnd());
        }
    }
#endif // CHECK_MEMORY_LEAK
#endif // HEAP_TRACK_ALLOC

    // destroy private heap after leak check
    if (CONFIG_FLAG(PrivateHeap))
    {TRACE_IT(23293);
        this->DestroyPrivateHeap();
    }

#ifdef INTERNAL_MEM_PROTECT_HEAP_ALLOC
    if (memProtectHeapHandle != nullptr)
    {TRACE_IT(23294);
        MemProtectHeapDestroy(memProtectHeapHandle);
    }
#endif // INTERNAL_MEM_PROTECT_HEAP_ALLOC
}

#ifdef HEAP_TRACK_ALLOC
void
HeapAllocatorData::LogAlloc(HeapAllocRecord * record, size_t requestedBytes, TrackAllocData const& data)
{TRACE_IT(23295);
    record->prev = nullptr;
    record->size = requestedBytes;

    record->data = this;
    record->next = head;
    record->allocId = allocCount;
    record->allocData = data;
    if (head != nullptr)
    {TRACE_IT(23296);
        head->prev = record;
    }
    head = record;
    outstandingBytes += requestedBytes;
    allocCount++;

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
#ifdef STACK_BACK_TRACE
    // REVIEW: Okay to use global flags?
    if (Js::Configuration::Global.flags.LeakStackTrace)
    {TRACE_IT(23297);
        // Allocation done before the flags is parse doesn't get a stack trace
        record->stacktrace = StackBackTrace::Capture(&NoCheckHeapAllocator::Instance, 1, StackTraceDepth);
    }
    else
    {TRACE_IT(23298);
        record->stacktrace = nullptr;
    }
#endif
#endif
}

void
HeapAllocatorData::LogFree(HeapAllocRecord * record)
{TRACE_IT(23299);
    Assert(record->data == this);

    // This is an expensive check for double free
#if 0
    HeapAllocRecord * curr = head;
    while (curr != nullptr)
    {TRACE_IT(23300);
        if (curr == record)
        {TRACE_IT(23301);
            break;
        }
        curr = curr->next;
    }
    Assert(curr != nullptr);
#endif
    if (record->next != nullptr)
    {TRACE_IT(23302);
        record->next->prev = record->prev;
    }
    if (record->prev == nullptr)
    {TRACE_IT(23303);
        head = record->next;
    }
    else
    {TRACE_IT(23304);
        record->prev->next = record->next;
    }

    deleteCount++;
    outstandingBytes -= record->size;
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
#ifdef STACK_BACK_TRACE
    if (record->stacktrace != nullptr)
    {TRACE_IT(23305);
        record->stacktrace->Delete(&NoCheckHeapAllocator::Instance);
    }
#endif
#endif
}

bool
HeapAllocatorData::CheckLeaks()
{TRACE_IT(23306);
    bool needPause = false;
    if (allocCount != deleteCount)
    {TRACE_IT(23307);
        needPause = true;

        HeapAllocRecord * current = head;
        while (current != nullptr)
        {TRACE_IT(23308);
            Output::Print(_u("%S%s"), current->allocData.GetTypeInfo()->name(),
                current->allocData.GetCount() == (size_t)-1? _u("") : _u("[]"));
            Output::SkipToColumn(50);
            Output::Print(_u("- %p - %10d bytes\n"),
                ((char*)current) + ::Math::Align<size_t>(sizeof(HeapAllocRecord), MEMORY_ALLOCATION_ALIGNMENT),
                current->size);
#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
#ifdef STACK_BACK_TRACE
            // REVIEW: Okay to use global flags?
            if (Js::Configuration::Global.flags.LeakStackTrace && current->stacktrace)
            {TRACE_IT(23309);
                // Allocation done before the flags is parse doesn't get a stack trace
                Output::Print(_u(" Allocation Stack:\n"));
                current->stacktrace->Print();
            }
#endif
#endif
            current = current->next;
        }
    }
    else if (outstandingBytes != 0)
    {TRACE_IT(23310);
        needPause = true;
        Output::Print(_u("Unbalanced new/delete size: %d\n"), outstandingBytes);
    }

    Output::Flush();

#if defined(ENABLE_DEBUG_CONFIG_OPTIONS) && !DBG
    // REVIEW: Okay to use global flags?
    if (needPause && Js::Configuration::Global.flags.Console)
    {TRACE_IT(23311);
        //This is not defined for WinCE
        HANDLE handle = GetStdHandle( STD_INPUT_HANDLE );

        FlushConsoleInputBuffer(handle);

        Output::Print(_u("Press any key to continue...\n"));
        Output::Flush();

        WaitForSingleObject(handle, INFINITE);

    }
#endif
    return allocCount == deleteCount && outstandingBytes == 0;
}

#endif


#ifdef CHECK_MEMORY_LEAK
MemoryLeakCheck::~MemoryLeakCheck()
{TRACE_IT(23312);
    if (head != nullptr)
    {TRACE_IT(23313);
        if (enableOutput)
        {TRACE_IT(23314);
            Output::Print(_u("FATAL ERROR: Memory Leak Detected\n"));
        }
        LeakRecord * current = head;
        do
        {TRACE_IT(23315);

            if (enableOutput)
            {TRACE_IT(23316);
                Output::PrintBuffer(current->dump, wcslen(current->dump));
            }
            LeakRecord * prev = current;
            current = current->next;
            free((void *)prev->dump);
            NoCheckHeapDelete(prev);
        }
        while (current != nullptr);
        if (enableOutput)
        {TRACE_IT(23317);
            Output::Print(_u("-------------------------------------------------------------------------------------\n"));
            Output::Print(_u("Total leaked: %d bytes (%d objects)\n"), leakedBytes, leakedCount);
            Output::Flush();
        }
#ifdef GENERATE_DUMP
        if (enableOutput)
        {TRACE_IT(23318);
            Js::Throw::GenerateDump(Js::Configuration::Global.flags.DumpOnCrash, true, true);
        }
#endif
    }
}

void
MemoryLeakCheck::AddLeakDump(char16 const * dump, size_t bytes, size_t count)
{TRACE_IT(23319);
    AutoCriticalSection autocs(&leakCheck.cs);
    LeakRecord * record = NoCheckHeapNewStruct(LeakRecord);
    record->dump = dump;
    record->next = nullptr;
    if (leakCheck.tail == nullptr)
    {TRACE_IT(23320);
        leakCheck.head = record;
        leakCheck.tail = record;
    }
    else
    {TRACE_IT(23321);
        leakCheck.tail->next = record;
        leakCheck.tail = record;
    }
    leakCheck.leakedBytes += bytes;
    leakCheck.leakedCount += count;
}
#endif
