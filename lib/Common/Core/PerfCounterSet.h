//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#ifdef PERF_COUNTERS

namespace PerfCounter
{
    template <typename TCounter>
    class DefaultCounterSetInstance : public InstanceBase
    {
    public:
        DefaultCounterSetInstance() : InstanceBase(TCounter::GetProvider(), TCounter::GetGuid())
        {TRACE_IT(20341);
            if (Initialize())
            {TRACE_IT(20342);
                data = defaultData;
            }
            else
            {TRACE_IT(20343);
                // if for any reason perf counter failed to initialize, try to create
                // shared memory instead. This will happen for sure if running under
                // Win8 AppContainer because they don't support v2 perf counters.
                // See comments in WWAHostJSCounterProvider for details.
                data = __super::InitializeSharedMemory(TCounter::MaxCounter, handle);
                if (data == nullptr)
                {TRACE_IT(20344);
                    data = defaultData;
                }
            }
            for (uint i = 0; i < TCounter::MaxCounter; i++)
            {TRACE_IT(20345);
                data[i] = 0;
                counters[i].Initialize(*this, i, &data[i]);
            }
        }
        ~DefaultCounterSetInstance()
        {TRACE_IT(20346);
            for (uint i = 0; i < TCounter::MaxCounter; i++)
            {TRACE_IT(20347);
                counters[i].Uninitialize(*this, i);
            }

            if (data != defaultData)
            {TRACE_IT(20348);
                __super::UninitializeSharedMemory(data, handle);
            }
        }
        Counter& GetCounter(uint id) {TRACE_IT(20349); Assert(id < TCounter::MaxCounter); return counters[id]; }

        bool Initialize()
        {TRACE_IT(20350);
            if (IsProviderInitialized())
            {TRACE_IT(20351);
                char16 wszModuleName[_MAX_PATH];
                if (!GetModuleFileName(NULL, wszModuleName, _MAX_PATH))
                {TRACE_IT(20352);
                    return false;
                }
                char16 wszFilename[_MAX_FNAME];
                _wsplitpath_s(wszModuleName, NULL, 0, NULL, 0, wszFilename, _MAX_FNAME, NULL, 0);

                return __super::Initialize(wszFilename, GetCurrentProcessId());
            }
            return false;
        }
    private:
        DWORD defaultData[TCounter::MaxCounter];
        DWORD * data;
        HANDLE handle;
        Counter counters[TCounter::MaxCounter];
    };

    class PageAllocatorCounterSet
    {
    public:
        static Counter& GetReservedSizeCounter(PageAllocatorType type)
            {TRACE_IT(20353); return instance.GetCounter(PageAllocatorCounterSetDefinition::GetReservedCounterId(type)); }
        static Counter& GetTotalReservedSizeCounter();
        static Counter& GetCommittedSizeCounter(PageAllocatorType type)
            {TRACE_IT(20354); return instance.GetCounter(PageAllocatorCounterSetDefinition::GetCommittedCounterId(type)); }
        static Counter& GetTotalCommittedSizeCounter();
        static Counter& GetUsedSizeCounter(PageAllocatorType type)
            {TRACE_IT(20355); return instance.GetCounter(PageAllocatorCounterSetDefinition::GetUsedCounterId(type)); }
        static Counter& GetTotalUsedSizeCounter();
    private:
        static DefaultCounterSetInstance<PageAllocatorCounterSetDefinition> instance;
    };

    class BasicCounterSet
    {
    public:
        static Counter& GetThreadContextCounter() {TRACE_IT(20356); return instance.GetCounter(0); }
        static Counter& GetScriptContextCounter() {TRACE_IT(20357); return instance.GetCounter(1); }
        static Counter& GetScriptContextActiveCounter() {TRACE_IT(20358); return instance.GetCounter(2); }
        static Counter& GetScriptCodeBufferCountCounter() {TRACE_IT(20359); return instance.GetCounter(3); }
    private:
        static DefaultCounterSetInstance<BasicCounterSetDefinition> instance;
    };


    class CodeCounterSet
    {
    public:
        static Counter& GetTotalByteCodeSizeCounter() {TRACE_IT(20360); return instance.GetCounter(0); }
        static Counter& GetTotalNativeCodeSizeCounter() {TRACE_IT(20361); return instance.GetCounter(1); }
        static Counter& GetTotalNativeCodeDataSizeCounter() {TRACE_IT(20362); return instance.GetCounter(2); }
        static Counter& GetStaticByteCodeSizeCounter() {TRACE_IT(20363); return instance.GetCounter(3); }
        static Counter& GetStaticNativeCodeSizeCounter() {TRACE_IT(20364); return instance.GetCounter(4); }
        static Counter& GetStaticNativeCodeDataSizeCounter() {TRACE_IT(20365); return instance.GetCounter(5); }
        static Counter& GetDynamicByteCodeSizeCounter() {TRACE_IT(20366); return instance.GetCounter(6); }
        static Counter& GetDynamicNativeCodeSizeCounter() {TRACE_IT(20367); return instance.GetCounter(7); }
        static Counter& GetDynamicNativeCodeDataSizeCounter() {TRACE_IT(20368); return instance.GetCounter(8); }
        static Counter& GetTotalFunctionCounter() {TRACE_IT(20369); return instance.GetCounter(9); }
        static Counter& GetStaticFunctionCounter() {TRACE_IT(20370); return instance.GetCounter(10); }
        static Counter& GetDynamicFunctionCounter() {TRACE_IT(20371); return instance.GetCounter(11); }
        static Counter& GetLoopNativeCodeSizeCounter() {TRACE_IT(20372); return instance.GetCounter(12); }
        static Counter& GetFunctionNativeCodeSizeCounter() {TRACE_IT(20373); return instance.GetCounter(13); }
        static Counter& GetDeferDeserializeFunctionProxyCounter() {TRACE_IT(20374); return instance.GetCounter(14); }
        static Counter& GetDeserializedFunctionBodyCounter() {TRACE_IT(20375); return instance.GetCounter(15); }
        static Counter& GetDeferredFunctionCounter() {TRACE_IT(20376); return instance.GetCounter(16); }

    private:
        static DefaultCounterSetInstance<CodeCounterSetDefinition> instance;
    };

#ifdef HEAP_PERF_COUNTERS
    class HeapCounterSet
    {
    public:
        static Counter& GetLiveObjectCounter() {TRACE_IT(20377); return instance.GetCounter(0); }
        static Counter& GetLiveObjectSizeCounter() {TRACE_IT(20378); return instance.GetCounter(1); }
    private:
        static DefaultCounterSetInstance<HeapCounterSetDefinition> instance;
    };
#endif
#ifdef RECYCLER_PERF_COUNTERS
    class RecyclerCounterSet
    {
    public:
        static Counter& GetLiveObjectSizeCounter() {TRACE_IT(20379); return instance.GetCounter(0); }
        static Counter& GetLiveObjectCounter() {TRACE_IT(20380); return instance.GetCounter(1); }
        static Counter& GetFreeObjectSizeCounter() {TRACE_IT(20381); return instance.GetCounter(2); }
        static Counter& GetPinnedObjectCounter() {TRACE_IT(20382); return instance.GetCounter(3); }
        static Counter& GetBindReferenceCounter() {TRACE_IT(20383); return instance.GetCounter(4); }
        static Counter& GetPropertyRecordBindReferenceCounter() {TRACE_IT(20384); return instance.GetCounter(5); }
        static Counter& GetLargeHeapBlockLiveObjectSizeCounter() {TRACE_IT(20385); return instance.GetCounter(6); }
        static Counter& GetLargeHeapBlockLiveObjectCounter() {TRACE_IT(20386); return instance.GetCounter(7); }
        static Counter& GetLargeHeapBlockFreeObjectSizeCounter() {TRACE_IT(20387); return instance.GetCounter(8); }
        static Counter& GetSmallHeapBlockLiveObjectSizeCounter() {TRACE_IT(20388); return instance.GetCounter(9); }
        static Counter& GetSmallHeapBlockLiveObjectCounter() {TRACE_IT(20389); return instance.GetCounter(10); }
        static Counter& GetSmallHeapBlockFreeObjectSizeCounter() {TRACE_IT(20390); return instance.GetCounter(11); }
        static Counter& GetLargeHeapBlockCountCounter() {TRACE_IT(20391); return instance.GetCounter(12); }
        static Counter& GetLargeHeapBlockPageSizeCounter() {TRACE_IT(20392); return instance.GetCounter(13); }

    private:
        static DefaultCounterSetInstance<RecyclerCounterSetDefinition> instance;
    };
#endif

#ifdef PROFILE_RECYCLER_ALLOC
    class RecyclerTrackerCounterSet
    {
    public:
        static Counter& GetPerfCounter(type_info const * typeinfo, bool isArray);
        static Counter& GetPerfSizeCounter(type_info const * typeinfo, bool isArray);
        static Counter& GetWeakRefPerfCounter(type_info const * typeinfo);

        class Map
        {
        public:
            Map(type_info const * type, bool isArray, uint counterIndex, uint sizeCounterIndex);
            Map(type_info const * type, uint weakRefCounterIndex);
        };

    private:
        static Counter& GetUnknownCounter() {TRACE_IT(20393); return instance.GetCounter(0); }
        static Counter& GetUnknownSizeCounter() {TRACE_IT(20394); return instance.GetCounter(1); }
        static Counter& GetUnknownArrayCounter() {TRACE_IT(20395); return instance.GetCounter(2); }
        static Counter& GetUnknownArraySizeCounter() {TRACE_IT(20396); return instance.GetCounter(3); }
        static Counter& GetUnknownWeakRefCounter() {TRACE_IT(20397); return instance.GetCounter(4); }
        static DefaultCounterSetInstance<RecyclerTrackerCounterSetDefinition> instance;

        static uint const NumUnknownCounters = 5;
        static type_info const * CountIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
        static type_info const * SizeIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
        static type_info const * ArrayCountIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
        static type_info const * ArraySizeIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
        static type_info const * WeakRefIndexTypeInfoMap[RecyclerTrackerCounterSetDefinition::MaxCounter - NumUnknownCounters];
    };

#define DEFINE_RECYCLER_TRACKER_PERF_COUNTER(type) \
    static PerfCounter::RecyclerTrackerCounterSet::Map RecyclerTrackerCounter##id(&typeid(type), false, \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##CounterIndex, \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##SizeCounterIndex)

#define DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER(type) \
    static PerfCounter::RecyclerTrackerCounterSet::Map RecyclerTrackerArrayCounter##id(&typeid(type), true, \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##ArrayCounterIndex, \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##ArraySizeCounterIndex)

#define DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(type) \
    static PerfCounter::RecyclerTrackerCounterSet::Map RecyclerTrackerWeakRefCounter##id(&typeid(type), \
        PerfCounter::RecyclerTrackerCounterSetDefinition::##type##WeakRefCounterIndex);

#else
#define DEFINE_RECYCLER_TRACKER_PERF_COUNTER(type)
#define DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER(type)
#define DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(type)
#endif
};

#define PERF_COUNTER_INC(CounterSetName, CounterName) ++PerfCounter::CounterSetName##CounterSet::Get##CounterName##Counter()
#define PERF_COUNTER_DEC(CounterSetName, CounterName) --PerfCounter::CounterSetName##CounterSet::Get##CounterName##Counter()

#define PERF_COUNTER_ADD(CounterSetName, CounterName, value) PerfCounter::CounterSetName##CounterSet::Get##CounterName##Counter() += value
#define PERF_COUNTER_SUB(CounterSetName, CounterName, value) PerfCounter::CounterSetName##CounterSet::Get##CounterName##Counter() -= value
#else
#define PERF_COUNTER_INC(CounterSetName, CounterName)
#define PERF_COUNTER_DEC(CounterSetName, CounterName)
#define PERF_COUNTER_ADD(CounterSetName, CounterName, value)
#define PERF_COUNTER_SUB(CounterSetName, CounterName, value)
#define DEFINE_RECYCLER_TRACKER_PERF_COUNTER(type)
#define DEFINE_RECYCLER_TRACKER_ARRAY_PERF_COUNTER(type)
#define DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(type)
#endif

#ifdef HEAP_PERF_COUNTERS
#define HEAP_PERF_COUNTER_INC(CounterName) PERF_COUNTER_INC(Heap, CounterName)
#define HEAP_PERF_COUNTER_DEC(CounterName) PERF_COUNTER_DEC(Heap, CounterName)
#define HEAP_PERF_COUNTER_ADD(CounterName, value) PERF_COUNTER_ADD(Heap, CounterName, value)
#define HEAP_PERF_COUNTER_SUB(CounterName, value) PERF_COUNTER_SUB(Heap, CounterName, value)
#else
#define HEAP_PERF_COUNTER_INC(CounterName)
#define HEAP_PERF_COUNTER_DEC(CounterName)
#define HEAP_PERF_COUNTER_ADD(CounterName, value)
#define HEAP_PERF_COUNTER_SUB(CounterName, value)
#endif

#ifdef RECYCLER_PERF_COUNTERS
#define RECYCLER_PERF_COUNTER_INC(CounterName) PERF_COUNTER_INC(Recycler, CounterName)
#define RECYCLER_PERF_COUNTER_DEC(CounterName) PERF_COUNTER_DEC(Recycler, CounterName)
#define RECYCLER_PERF_COUNTER_ADD(CounterName, value) PERF_COUNTER_ADD(Recycler, CounterName, value)
#define RECYCLER_PERF_COUNTER_SUB(CounterName, value) PERF_COUNTER_SUB(Recycler, CounterName, value)
#else
#define RECYCLER_PERF_COUNTER_INC(CounterName)
#define RECYCLER_PERF_COUNTER_DEC(CounterName)
#define RECYCLER_PERF_COUNTER_ADD(CounterName, value)
#define RECYCLER_PERF_COUNTER_SUB(CounterName, value)
#endif
