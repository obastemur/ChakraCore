//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#ifdef PERF_COUNTERS
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
#pragma init_seg(".CRT$XCAD")

#pragma warning(push)
#pragma warning(disable:4838) // conversion from 'unsigned long' to 'LONG' requires a narrowing conversion
#include "Microsoft-Scripting-Jscript9.InternalCounters.h"
#pragma warning(pop)

namespace PerfCounter
{

class Provider
{
public:
    static Provider InternalCounter;
#ifdef ENABLE_COUNTER_NOTIFICATION_CALLBACK
    void SetNotificationCallBack(PERFLIBREQUEST pfn) {TRACE_IT(20283); pfnNotificationCallBack = pfn; }
#endif
private:
    Provider(HANDLE& handle);
    ~Provider();

    bool IsInitialized() const {TRACE_IT(20284); return isInitialized; }
    HANDLE GetHandler() {TRACE_IT(20285); return handle; }

#ifdef ENABLE_COUNTER_NOTIFICATION_CALLBACK
    PERFLIBREQUEST pfnNotificationCallBack;
    static ULONG WINAPI NotificationCallBack(ULONG RequestCode, PVOID Buffer, ULONG BufferSize);
#endif
    HANDLE& handle;
    bool isInitialized;
    friend class InstanceBase;
    friend class Counter;
};

Provider Provider::InternalCounter(JS9InternalCounterProvider);

#ifdef ENABLE_COUNTER_NOTIFICATION_CALLBACK
ULONG WINAPI
Provider::NotificationCallBack(ULONG RequestCode, PVOID Buffer, ULONG BufferSize)
{TRACE_IT(20286);
    if (Provider::InternalCounter.pfnNotificationCallBack != NULL)
    {TRACE_IT(20287);
        return Provider::InternalCounter.pfnNotificationCallBack(RequestCode, Buffer, BufferSize);
    }
    return ERROR_SUCCESS;
}
#endif
Provider::Provider(HANDLE& handle) :
    handle(handle), isInitialized(false)
{TRACE_IT(20288);
    PERFLIBREQUEST callback = NULL;
#ifdef ENABLE_COUNTER_NOTIFICATION_CALLBACK
    callback = &NotificationCallBack;
#endif
    if (ERROR_SUCCESS == CounterInitialize(callback, NULL, NULL, NULL))
    {TRACE_IT(20289);
        isInitialized = true;
    }
}

Provider::~Provider()
{TRACE_IT(20290);
    if (IsInitialized())
    {TRACE_IT(20291);
        CounterCleanup();
    }
}

InstanceBase::InstanceBase(Provider& provider, GUID const& guid) : provider(provider), guid(guid), instanceData(NULL)
{TRACE_IT(20292);
}

InstanceBase::~InstanceBase()
{TRACE_IT(20293);
    if (IsEnabled())
    {TRACE_IT(20294);
        ::PerfDeleteInstance(provider.GetHandler(), instanceData);
    }
}

bool
InstanceBase::IsProviderInitialized() const
{TRACE_IT(20295);
    return provider.IsInitialized();
}

bool
InstanceBase::IsEnabled() const
{TRACE_IT(20296);
    return instanceData != NULL;
}

static const size_t GUID_LEN = 37;   // includes null
static const char16 s_wszObjectNamePrefix[] = _u("jscript9_perf_counter_");
static const size_t OBJECT_NAME_LEN = GUID_LEN + _countof(s_wszObjectNamePrefix) + 11;

static
void GetSharedMemoryObjectName(__inout_ecount(OBJECT_NAME_LEN) char16 wszObjectName[OBJECT_NAME_LEN], DWORD pid, GUID const& guid)
{
    swprintf_s(wszObjectName, OBJECT_NAME_LEN, _u("%s%d_%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x"),
        s_wszObjectNamePrefix, pid,
        guid.Data1,
        guid.Data2,
        guid.Data3,
        guid.Data4[0], guid.Data4[1],
        guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
}

bool
InstanceBase::Initialize(char16 const * wszInstanceName, DWORD processId)
{TRACE_IT(20297);
    if (provider.IsInitialized())
    {TRACE_IT(20298);
        instanceData = PerfCreateInstance(provider.GetHandler(), &guid,
            wszInstanceName, processId);
        return instanceData != NULL;
    }
    return false;
}

DWORD *
InstanceBase::InitializeSharedMemory(DWORD numCounter, HANDLE& handle)
{TRACE_IT(20299);
    Assert(!IsEnabled());

    DWORD size = numCounter * sizeof(DWORD);
    char16 wszObjectName[OBJECT_NAME_LEN];
    GetSharedMemoryObjectName(wszObjectName, GetCurrentProcessId(), guid);
    handle = ::CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, size, wszObjectName);
    if (handle == NULL)
    {TRACE_IT(20300);
        return NULL;
    }
    DWORD * data = (DWORD *)MapViewOfFile(handle, FILE_MAP_WRITE, 0, 0, size);
    if (data == NULL)
    {TRACE_IT(20301);
        CloseHandle(handle);
        handle = NULL;
    }
    return data;
}

DWORD *
InstanceBase::OpenSharedMemory(__in_ecount(MAX_OBJECT_NAME_PREFIX) char16 const wszObjectNamePrefix[MAX_OBJECT_NAME_PREFIX],
    DWORD pid, DWORD numCounter, HANDLE& handle)
{TRACE_IT(20302);
    DWORD size = numCounter * sizeof(DWORD);
    char16 wszObjectName[OBJECT_NAME_LEN];
    GetSharedMemoryObjectName(wszObjectName, pid, guid);
    char16 wszObjectNameFull[MAX_OBJECT_NAME_PREFIX + OBJECT_NAME_LEN];
    swprintf_s(wszObjectNameFull, _u("%s\\%s"), wszObjectNamePrefix, wszObjectName);
    handle = ::OpenFileMapping(FILE_MAP_READ, FALSE, wszObjectNameFull);
    if (handle == NULL)
    {TRACE_IT(20303);
        return NULL;
    }
    DWORD * data = (DWORD *)MapViewOfFile(handle, FILE_MAP_READ, 0, 0, size);
    if (data == NULL)
    {TRACE_IT(20304);
        CloseHandle(handle);
        handle = NULL;
    }
    return data;
}

void
InstanceBase::UninitializeSharedMemory(DWORD * data, HANDLE handle)
{TRACE_IT(20305);
    UnmapViewOfFile(data);
    CloseHandle(handle);
}

void
Counter::Initialize(InstanceBase& instance, DWORD id, DWORD * count)
{TRACE_IT(20306);
    this->count = count;
    if (instance.IsEnabled())
    {TRACE_IT(20307);
        ::PerfSetCounterRefValue(instance.GetProvider().GetHandler(), instance.GetData(), id, count);
    }
}

void
Counter::Uninitialize(InstanceBase& instance, DWORD id)
{TRACE_IT(20308);
    if (instance.IsEnabled())
    {TRACE_IT(20309);
        ::PerfSetCounterRefValue(instance.GetProvider().GetHandler(), instance.GetData(), id, NULL);
    }
}

#define DEFINE_PAGE_ALLOCATOR_COUNTER_ID(type) JS9InternalCounter_PageAllocCounterSet_##type##ReservedSize,
static uint ReservedCounterId[PageAllocatorType_Max + 1] =
{
    PAGE_ALLOCATOR_TYPE(DEFINE_PAGE_ALLOCATOR_COUNTER_ID)
    JS9InternalCounter_PageAllocCounterSet_TotalReservedSize
};
#undef DEFINE_PAGE_ALLOCATOR_COUNTER_ID

#define DEFINE_PAGE_ALLOCATOR_COUNTER_ID(type) JS9InternalCounter_PageAllocCounterSet_##type##CommittedSize,
static uint CommittedCounterId[PageAllocatorType_Max + 1] =
{
    PAGE_ALLOCATOR_TYPE(DEFINE_PAGE_ALLOCATOR_COUNTER_ID)
    JS9InternalCounter_PageAllocCounterSet_TotalCommittedSize
};
#undef DEFINE_PAGE_ALLOCATOR_COUNTER_ID

#define DEFINE_PAGE_ALLOCATOR_COUNTER_ID(type) JS9InternalCounter_PageAllocCounterSet_##type##UsedSize,
static uint UsedCounterId[PageAllocatorType_Max + 1] =
{
    PAGE_ALLOCATOR_TYPE(DEFINE_PAGE_ALLOCATOR_COUNTER_ID)
    JS9InternalCounter_PageAllocCounterSet_TotalUsedSize
};
#undef DEFINE_PAGE_ALLOCATOR_COUNTER_ID
uint
PageAllocatorCounterSetDefinition::GetReservedCounterId(PageAllocatorType type)
{TRACE_IT(20310);
    return ReservedCounterId[type];
}
uint
PageAllocatorCounterSetDefinition::GetCommittedCounterId(PageAllocatorType type)
{TRACE_IT(20311);
    return CommittedCounterId[type];
}
uint
PageAllocatorCounterSetDefinition::GetUsedCounterId(PageAllocatorType type)
{TRACE_IT(20312);
    return UsedCounterId[type];
}


GUID const& PageAllocatorCounterSetDefinition::GetGuid() {TRACE_IT(20313); return JS9InternalCounter_PageAllocCounterSetGuid; }
Provider& PageAllocatorCounterSetDefinition::GetProvider() {TRACE_IT(20314); return Provider::InternalCounter; }

GUID const& BasicCounterSetDefinition::GetGuid() {TRACE_IT(20315); return JS9InternalCounter_BasicCounterSetGuid; }
Provider& BasicCounterSetDefinition::GetProvider() {TRACE_IT(20316); return Provider::InternalCounter; }

GUID const& CodeCounterSetDefinition::GetGuid() {TRACE_IT(20317); return JS9InternalCounter_CodeCounterSetGuid; }
Provider& CodeCounterSetDefinition::GetProvider() {TRACE_IT(20318); return Provider::InternalCounter; }

#ifdef HEAP_PERF_COUNTERS
GUID const& HeapCounterSetDefinition::GetGuid() {TRACE_IT(20319); return JS9InternalCounter_HeapCounterSetGuid; }
Provider& HeapCounterSetDefinition::GetProvider() {TRACE_IT(20320); return Provider::InternalCounter; }
#endif

#ifdef RECYCLER_PERF_COUNTERS
GUID const& RecyclerCounterSetDefinition::GetGuid() {TRACE_IT(20321); return JS9InternalCounter_RecyclerCounterSetGuid; }
Provider& RecyclerCounterSetDefinition::GetProvider() {TRACE_IT(20322); return Provider::InternalCounter; }
#endif

#ifdef PROFILE_RECYCLER_ALLOC
GUID const& RecyclerTrackerCounterSetDefinition::GetGuid() {TRACE_IT(20323); return JS9InternalCounter_RecyclerTrackerCounterSetGuid; }
Provider& RecyclerTrackerCounterSetDefinition::GetProvider() {TRACE_IT(20324); return Provider::InternalCounter; }

#define DEFINE_RECYCLER_TRACKER_PERF_COUNTER_INDEX(type) \
    uint const RecyclerTrackerCounterSetDefinition::##type##CounterIndex = JS9InternalCounter_RecyclerTrackerCounterSet_##type##Count; \
    uint const RecyclerTrackerCounterSetDefinition::##type##SizeCounterIndex = JS9InternalCounter_RecyclerTrackerCounterSet_##type##Size;

#define DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER_INDEX(type) \
    uint const RecyclerTrackerCounterSetDefinition::##type##ArrayCounterIndex = JS9InternalCounter_RecyclerTrackerCounterSet_##type##ArrayCount; \
    uint const RecyclerTrackerCounterSetDefinition::##type##ArraySizeCounterIndex = JS9InternalCounter_RecyclerTrackerCounterSet_##type##ArraySize;

#define DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER_INDEX(type) \
    uint const RecyclerTrackerCounterSetDefinition::##type##WeakRefCounterIndex = JS9InternalCounter_RecyclerTrackerCounterSet_##type##WeakRefCount;

RECYCLER_TRACKER_PERF_COUNTER_TYPE(DEFINE_RECYCLER_TRACKER_PERF_COUNTER_INDEX);
RECYCLER_TRACKER_ARRAY_PERF_COUNTER_TYPE(DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER_INDEX);
RECYCLER_TRACKER_WEAKREF_PERF_COUNTER_TYPE(DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER_INDEX);
#endif
};
#endif
