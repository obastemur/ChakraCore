//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeBasePch.h"
#include "BackendApi.h"
#include "ThreadServiceWrapper.h"
#include "Types/TypePropertyCache.h"
#include "Debug/DebuggingFlags.h"
#include "Debug/DiagProbe.h"
#include "Debug/DebugManager.h"
#include "Chars.h"
#include "CaseInsensitive.h"
#include "CharSet.h"
#include "CharMap.h"
#include "StandardChars.h"
#include "Base/ThreadContextTlsEntry.h"
#include "Base/ThreadBoundThreadContextManager.h"
#include "Language/SourceDynamicProfileManager.h"
#include "Language/CodeGenRecyclableData.h"
#include "Language/InterpreterStackFrame.h"
#include "Language/JavascriptStackWalker.h"
#include "Base/ScriptMemoryDumper.h"

// SIMD_JS
#include "Library/SimdLib.h"

#if DBG
#include "Memory/StressTest.h"
#endif

#ifdef DYNAMIC_PROFILE_MUTATOR
#include "Language/DynamicProfileMutator.h"
#endif


#ifdef ENABLE_BASIC_TELEMETRY
#include "Telemetry.h"
#endif

const int TotalNumberOfBuiltInProperties = Js::PropertyIds::_countJSOnlyProperty;

/*
 * When we aren't adding any additional properties
 */
void DefaultInitializeAdditionalProperties(ThreadContext *threadContext)
{TRACE_IT(36941);

}

/*
 *
 */
void (*InitializeAdditionalProperties)(ThreadContext *threadContext) = DefaultInitializeAdditionalProperties;

// To make sure the marker function doesn't get inlined, optimized away, or merged with other functions we disable optimization.
// If this method ends up causing a perf problem in the future, we should replace it with asm versions which should be lighter.
#pragma optimize("g", off)
_NOINLINE extern "C" void* MarkerForExternalDebugStep()
{TRACE_IT(36942);
    // We need to return something here to prevent this function from being merged with other empty functions by the linker.
    static int __dummy;
    return &__dummy;
}
#pragma optimize("", on)

CriticalSection ThreadContext::s_csThreadContext;
size_t ThreadContext::processNativeCodeSize = 0;
ThreadContext * ThreadContext::globalListFirst = nullptr;
ThreadContext * ThreadContext::globalListLast = nullptr;
THREAD_LOCAL uint ThreadContext::activeScriptSiteCount = 0;

const Js::PropertyRecord * const ThreadContext::builtInPropertyRecords[] =
{
    Js::BuiltInPropertyRecords::EMPTY,
#define ENTRY_INTERNAL_SYMBOL(n) Js::BuiltInPropertyRecords::n,
#define ENTRY_SYMBOL(n, d) Js::BuiltInPropertyRecords::n,
#define ENTRY(n) Js::BuiltInPropertyRecords::n,
#define ENTRY2(n, s) ENTRY(n)
#include "Base/JnDirectFields.h"
};

ThreadContext::RecyclableData::RecyclableData(Recycler *const recycler) :
    soErrorObject(nullptr, nullptr, nullptr, true),
    oomErrorObject(nullptr, nullptr, nullptr, true),
    terminatedErrorObject(nullptr, nullptr, nullptr),
    typesWithProtoPropertyCache(recycler),
    propertyGuards(recycler, 128),
    oldEntryPointInfo(nullptr),
    returnedValueList(nullptr),
    constructorCacheInvalidationCount(0)
{TRACE_IT(36943);
}

ThreadContext::ThreadContext(AllocationPolicyManager * allocationPolicyManager, JsUtil::ThreadService::ThreadServiceCallback threadServiceCallback, bool enableExperimentalFeatures) :
    currentThreadId(::GetCurrentThreadId()),
    stackLimitForCurrentThread(0),
    stackProber(nullptr),
    isThreadBound(false),
    hasThrownPendingException(false),
    noScriptScope(false),
    heapEnum(nullptr),
    threadContextFlags(ThreadContextFlagNoFlag),
    JsUtil::DoublyLinkedListElement<ThreadContext>(),
    allocationPolicyManager(allocationPolicyManager),
    threadService(threadServiceCallback),
    isOptimizedForManyInstances(Js::Configuration::Global.flags.OptimizeForManyInstances),
    bgJit(Js::Configuration::Global.flags.BgJit),
    pageAllocator(allocationPolicyManager, PageAllocatorType_Thread, Js::Configuration::Global.flags, 0, PageAllocator::DefaultMaxFreePageCount,
        false
#if ENABLE_BACKGROUND_PAGE_FREEING
        , &backgroundPageQueue
#endif
        ),
    recycler(nullptr),
    hasCollectionCallBack(false),
    callDispose(true),
#if ENABLE_NATIVE_CODEGEN
    jobProcessor(nullptr),
#endif
    interruptPoller(nullptr),
    expirableCollectModeGcCount(-1),
    expirableObjectList(nullptr),
    expirableObjectDisposeList(nullptr),
    numExpirableObjects(0),
    disableExpiration(false),
    callRootLevel(0),
    nextTypeId((Js::TypeId)Js::Constants::ReservedTypeIds),
    entryExitRecord(nullptr),
    leafInterpreterFrame(nullptr),
    threadServiceWrapper(nullptr),
    temporaryArenaAllocatorCount(0),
    temporaryGuestArenaAllocatorCount(0),
    crefSContextForDiag(0),
    m_prereservedRegionAddr(0),
    scriptContextList(nullptr),
    scriptContextEverRegistered(false),
#if DBG_DUMP || defined(PROFILE_EXEC)
    topLevelScriptSite(nullptr),
#endif
    polymorphicCacheState(0),
    stackProbeCount(0),
#ifdef BAILOUT_INJECTION
    bailOutByteCodeLocationCount(0),
#endif
    sourceCodeSize(0),
    nativeCodeSize(0),
    threadAlloc(_u("TC"), GetPageAllocator(), Js::Throw::OutOfMemory),
    inlineCacheThreadInfoAllocator(_u("TC-InlineCacheInfo"), GetPageAllocator(), Js::Throw::OutOfMemory),
    isInstInlineCacheThreadInfoAllocator(_u("TC-IsInstInlineCacheInfo"), GetPageAllocator(), Js::Throw::OutOfMemory),
    equivalentTypeCacheInfoAllocator(_u("TC-EquivalentTypeCacheInfo"), GetPageAllocator(), Js::Throw::OutOfMemory),
    preReservedVirtualAllocator(),
    protoInlineCacheByPropId(&inlineCacheThreadInfoAllocator, 512),
    storeFieldInlineCacheByPropId(&inlineCacheThreadInfoAllocator, 256),
    isInstInlineCacheByFunction(&isInstInlineCacheThreadInfoAllocator, 128),
    registeredInlineCacheCount(0),
    unregisteredInlineCacheCount(0),
    prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator(_u("TC-ProtoWritableProp"), GetPageAllocator(), Js::Throw::OutOfMemory),
    standardUTF8Chars(0),
    standardUnicodeChars(0),
    hasUnhandledException(FALSE),
    hasCatchHandler(FALSE),
    disableImplicitFlags(DisableImplicitNoFlag),
    hasCatchHandlerToUserCode(false),
    caseInvariantPropertySet(nullptr),
    entryPointToBuiltInOperationIdCache(&threadAlloc, 0),
#if ENABLE_NATIVE_CODEGEN
#if !FLOATVAR
    codeGenNumberThreadAllocator(nullptr),
    xProcNumberPageSegmentManager(nullptr),
#endif
    m_jitNumericProperties(nullptr),
    m_jitNeedsPropertyUpdate(false),
#if DYNAMIC_INTERPRETER_THUNK || defined(ASMJS_PLAT)
    thunkPageAllocators(allocationPolicyManager, /* allocXData */ false, /* virtualAllocator */ nullptr, GetCurrentProcess()),
#endif
    codePageAllocators(allocationPolicyManager, ALLOC_XDATA, GetPreReservedVirtualAllocator(), GetCurrentProcess()),
#endif
    dynamicObjectEnumeratorCacheMap(&HeapAllocator::Instance, 16),
    //threadContextFlags(ThreadContextFlagNoFlag),
#ifdef NTBUILD
    telemetryBlock(&localTelemetryBlock),
#endif
    configuration(enableExperimentalFeatures),
    jsrtRuntime(nullptr),
    propertyMap(nullptr),
    rootPendingClose(nullptr),
    exceptionCode(0),
    isProfilingUserCode(true),
    loopDepth(0),
    redeferralState(InitialRedeferralState),
    gcSinceLastRedeferral(0),
    gcSinceCallCountsCollected(0),
    tridentLoadAddress(nullptr),
    m_remoteThreadContextInfo(nullptr),
    debugManager(nullptr)
#if ENABLE_TTD
    , TTDContext(nullptr)
    , TTDLog(nullptr)
    , TTDRootNestingCount(0)
#endif
#ifdef ENABLE_DIRECTCALL_TELEMETRY
    , directCallTelemetry(this)
#endif
{
    pendingProjectionContextCloseList = JsUtil::List<IProjectionContext*, ArenaAllocator>::New(GetThreadAlloc());
    hostScriptContextStack = Anew(GetThreadAlloc(), JsUtil::Stack<HostScriptContext*>, GetThreadAlloc());

    functionCount = 0;
    sourceInfoCount = 0;
#if DBG || defined(RUNTIME_DATA_COLLECTION)
    scriptContextCount = 0;
#endif
    isScriptActive = false;

#ifdef ENABLE_CUSTOM_ENTROPY
    entropy.Initialize();
#endif

#if ENABLE_NATIVE_CODEGEN
    this->bailOutRegisterSaveSpace = AnewArrayZ(this->GetThreadAlloc(), Js::Var, GetBailOutRegisterSaveSlotCount());
#endif

#if defined(ENABLE_SIMDJS) && ENABLE_NATIVE_CODEGEN
    simdFuncInfoToOpcodeMap = Anew(this->GetThreadAlloc(), FuncInfoToOpcodeMap, this->GetThreadAlloc());
    simdOpcodeToSignatureMap = AnewArrayZ(this->GetThreadAlloc(), SimdFuncSignature, Js::Simd128OpcodeCount());
    {TRACE_IT(36944);
#define MACRO_SIMD_WMS(op, LayoutAsmJs, OpCodeAttrAsmJs, OpCodeAttr, ...) \
    AddSimdFuncToMaps(Js::OpCode::##op, __VA_ARGS__);

#define MACRO_SIMD_EXTEND_WMS(op, LayoutAsmJs, OpCodeAttrAsmJs, OpCodeAttr, ...) MACRO_SIMD_WMS(op, LayoutAsmJs, OpCodeAttrAsmJs, OpCodeAttr, __VA_ARGS__)

#include "ByteCode/OpCodesSimd.h"
    }
#endif // defined(ENABLE_SIMDJS) && ENABLE_NATIVE_CODEGEN

#if DBG_DUMP
    scriptSiteCount = 0;
    pageAllocator.debugName = _u("Thread");
#endif
#ifdef DYNAMIC_PROFILE_MUTATOR
    this->dynamicProfileMutator = DynamicProfileMutator::GetMutator();
#endif

    PERF_COUNTER_INC(Basic, ThreadContext);

#ifdef LEAK_REPORT
    this->rootTrackerScriptContext = nullptr;
    this->threadId = ::GetCurrentThreadId();
#endif

#ifdef NTBUILD
    memset(&localTelemetryBlock, 0, sizeof(localTelemetryBlock));
#endif

    AutoCriticalSection autocs(ThreadContext::GetCriticalSection());
    ThreadContext::LinkToBeginning(this, &ThreadContext::globalListFirst, &ThreadContext::globalListLast);
#if DBG
    // Since we created our page allocator while we were constructing this thread context
    // it will pick up the thread context id that is current on the thread. We need to update
    // that now.
    pageAllocator.UpdateThreadContextHandle((ThreadContextId)this);
#endif

#ifdef ENABLE_PROJECTION
#if DBG_DUMP
    this->projectionMemoryInformation = nullptr;
#endif
#endif

    this->InitAvailableCommit();
}

void ThreadContext::InitAvailableCommit()
{TRACE_IT(36945);
    // Once per process: get the available commit for the process from the OS and push it to the AutoSystemInfo.
    // (This must be done lazily, outside DllMain. And it must be done from the Runtime, since the common lib
    // doesn't have access to the DelayLoadLibrary stuff.)
    ULONG64 commit;
    BOOL success = AutoSystemInfo::Data.GetAvailableCommit(&commit);
    if (!success)
    {TRACE_IT(36946);
        commit = (ULONG64)-1;
#ifdef NTBUILD
        APP_MEMORY_INFORMATION AppMemInfo;
        success = GetWinCoreProcessThreads()->GetProcessInformation(
            GetCurrentProcess(),
            ProcessAppMemoryInfo,
            &AppMemInfo,
            sizeof(AppMemInfo));
        if (success)
        {TRACE_IT(36947);
            commit = AppMemInfo.AvailableCommit;
        }
#endif
        AutoSystemInfo::Data.SetAvailableCommit(commit);
    }
}

void ThreadContext::SetStackProber(StackProber * stackProber)
{TRACE_IT(36948);
    this->stackProber = stackProber;

    if (stackProber != NULL && this->stackLimitForCurrentThread != Js::Constants::StackLimitForScriptInterrupt)
    {TRACE_IT(36949);
        this->stackLimitForCurrentThread = stackProber->GetScriptStackLimit();
    }
}

size_t ThreadContext::GetScriptStackLimit() const
{TRACE_IT(36950);
    return stackProber->GetScriptStackLimit();
}

HANDLE
ThreadContext::GetProcessHandle() const
{TRACE_IT(36951);
    return GetCurrentProcess();
}

intptr_t
ThreadContext::GetThreadStackLimitAddr() const
{TRACE_IT(36952);
    return (intptr_t)GetAddressOfStackLimitForCurrentThread();
}

#if ENABLE_NATIVE_CODEGEN && defined(ENABLE_SIMDJS) && (defined(_M_IX86) || defined(_M_X64))
intptr_t
ThreadContext::GetSimdTempAreaAddr(uint8 tempIndex) const
{TRACE_IT(36953);
    return (intptr_t)&X86_TEMP_SIMD[tempIndex];
}
#endif

intptr_t
ThreadContext::GetDisableImplicitFlagsAddr() const
{TRACE_IT(36954);
    return (intptr_t)&disableImplicitFlags;
}

intptr_t
ThreadContext::GetImplicitCallFlagsAddr() const
{TRACE_IT(36955);
    return (intptr_t)&implicitCallFlags;
}

ptrdiff_t
ThreadContext::GetChakraBaseAddressDifference() const
{TRACE_IT(36956);
    return 0;
}

ptrdiff_t
ThreadContext::GetCRTBaseAddressDifference() const
{TRACE_IT(36957);
    return 0;
}

IActiveScriptProfilerHeapEnum* ThreadContext::GetHeapEnum()
{TRACE_IT(36958);
    return heapEnum;
}

void ThreadContext::SetHeapEnum(IActiveScriptProfilerHeapEnum* newHeapEnum)
{TRACE_IT(36959);
    Assert((newHeapEnum != nullptr && heapEnum == nullptr) || (newHeapEnum == nullptr && heapEnum != nullptr));
    heapEnum = newHeapEnum;
}

void ThreadContext::ClearHeapEnum()
{TRACE_IT(36960);
    Assert(heapEnum != nullptr);
    heapEnum = nullptr;
}

void ThreadContext::GlobalInitialize()
{TRACE_IT(36961);
    for (int i = 0; i < _countof(builtInPropertyRecords); i++)
    {TRACE_IT(36962);
        builtInPropertyRecords[i]->SetHash(JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(builtInPropertyRecords[i]->GetBuffer(), builtInPropertyRecords[i]->GetLength()));
    }
}

ThreadContext::~ThreadContext()
{TRACE_IT(36963);
    {TRACE_IT(36964);
        AutoCriticalSection autocs(ThreadContext::GetCriticalSection());
        ThreadContext::Unlink(this, &ThreadContext::globalListFirst, &ThreadContext::globalListLast);
    }

#if ENABLE_TTD
    if(this->TTDContext != nullptr)
    {TRACE_IT(36965);
        TT_HEAP_DELETE(TTD::ThreadContextTTD, this->TTDContext);
        this->TTDContext = nullptr;
    }

    if(this->TTDLog != nullptr)
    {TRACE_IT(36966);
        TT_HEAP_DELETE(TTD::EventLog, this->TTDLog);
        this->TTDLog = nullptr;
    }
#endif

#ifdef LEAK_REPORT
    if (Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag))
    {TRACE_IT(36967);
        AUTO_LEAK_REPORT_SECTION(Js::Configuration::Global.flags, _u("Thread Context (%p): %s (TID: %d)"), this,
            this->GetRecycler()->IsInDllCanUnloadNow()? _u("DllCanUnloadNow") :
            this->GetRecycler()->IsInDetachProcess()? _u("DetachProcess") : _u("Destructor"), this->threadId);
        LeakReport::DumpUrl(this->threadId);
    }
#endif
    if (interruptPoller)
    {TRACE_IT(36968);
        HeapDelete(interruptPoller);
        interruptPoller = nullptr;
    }

#if DBG
    // ThreadContext dtor may be running on a different thread.
    // Recycler may call finalizer that free temp Arenas, which will free pages back to
    // the page Allocator, which will try to suspend idle on a different thread.
    // So we need to disable idle decommit asserts.
    pageAllocator.ShutdownIdleDecommit();
#endif

    // Allocating memory during the shutdown codepath is not preferred
    // so we'll close the page allocator before we release the GC
    // If any dispose is allocating memory during shutdown, that is a bug
    pageAllocator.Close();

    // The recycler need to delete before the background code gen thread
    // because that might run finalizer which need access to the background code gen thread.
    if (recycler != nullptr)
    {TRACE_IT(36969);
        for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext; scriptContext = scriptContext->next)
        {TRACE_IT(36970);
            if (!scriptContext->IsActuallyClosed())
            {TRACE_IT(36971);
                // We close ScriptContext here because anyhow HeapDelete(recycler) when disposing the
                // JavaScriptLibrary will close ScriptContext. Explicit close gives us chance to clear
                // other things to which ScriptContext holds reference to
                AssertMsg(!IsInScript(), "Can we be in script here?");
                scriptContext->MarkForClose();
            }
        }

        // If all scriptContext's have been closed, then the sourceProfileManagersByUrl
        // should have been released
        AssertMsg(this->recyclableData->sourceProfileManagersByUrl == nullptr ||
            this->recyclableData->sourceProfileManagersByUrl->Count() == 0, "There seems to have been a refcounting imbalance.");

        this->recyclableData->sourceProfileManagersByUrl = nullptr;
        this->recyclableData->oldEntryPointInfo = nullptr;

        if (this->recyclableData->symbolRegistrationMap != nullptr)
        {TRACE_IT(36972);
            this->recyclableData->symbolRegistrationMap->Clear();
            this->recyclableData->symbolRegistrationMap = nullptr;
        }

        if (this->recyclableData->returnedValueList != nullptr)
        {TRACE_IT(36973);
            this->recyclableData->returnedValueList->Clear();
            this->recyclableData->returnedValueList = nullptr;
        }

        if (this->propertyMap != nullptr)
        {TRACE_IT(36974);
            HeapDelete(this->propertyMap);
            this->propertyMap = nullptr;
        }

#if ENABLE_NATIVE_CODEGEN
        if (this->m_jitNumericProperties != nullptr)
        {TRACE_IT(36975);
            HeapDelete(this->m_jitNumericProperties);
            this->m_jitNumericProperties = nullptr;
        }
#endif
        // Unpin the memory for leak report so we don't report this as a leak.
        recyclableData.Unroot(recycler);

#if defined(LEAK_REPORT) || defined(CHECK_MEMORY_LEAK)
        for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext; scriptContext = scriptContext->next)
        {TRACE_IT(36976);
            scriptContext->ClearSourceContextInfoMaps();
            scriptContext->ShutdownClearSourceLists();
        }

#ifdef LEAK_REPORT
        // heuristically figure out which one is the root tracker script engine
        // and force close on it
        if (this->rootTrackerScriptContext != nullptr)
        {TRACE_IT(36977);
            this->rootTrackerScriptContext->Close(false);
        }
#endif
#endif
#if ENABLE_NATIVE_CODEGEN
#if !FLOATVAR
        if (this->codeGenNumberThreadAllocator)
        {TRACE_IT(36978);
            HeapDelete(this->codeGenNumberThreadAllocator);
            this->codeGenNumberThreadAllocator = nullptr;
        }
        if (this->xProcNumberPageSegmentManager)
        {TRACE_IT(36979);
            HeapDelete(this->xProcNumberPageSegmentManager);
            this->xProcNumberPageSegmentManager = nullptr;
        }
#endif
#endif

        Assert(this->debugManager == nullptr);

        HeapDelete(recycler);
    }

#if ENABLE_NATIVE_CODEGEN
    if(jobProcessor)
    {TRACE_IT(36980);
        if(this->bgJit)
        {TRACE_IT(36981);
            HeapDelete(static_cast<JsUtil::BackgroundJobProcessor *>(jobProcessor));
        }
        else
        {TRACE_IT(36982);
            HeapDelete(static_cast<JsUtil::ForegroundJobProcessor *>(jobProcessor));
        }
        jobProcessor = nullptr;
    }
#endif

    // Do not require all GC callbacks to be revoked, because Trident may not revoke if there
    // is a leak, and we don't want the leak to be masked by an assert

#ifdef ENABLE_PROJECTION
    externalWeakReferenceCacheList.Clear(&HeapAllocator::Instance);
#endif

    this->collectCallBackList.Clear(&HeapAllocator::Instance);
    this->protoInlineCacheByPropId.Reset();
    this->storeFieldInlineCacheByPropId.Reset();
    this->isInstInlineCacheByFunction.Reset();
    this->equivalentTypeCacheEntryPoints.Reset();
    this->prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext.Reset();

    this->registeredInlineCacheCount = 0;
    this->unregisteredInlineCacheCount = 0;

    AssertMsg(this->GetHeapEnum() == nullptr, "Heap enumeration should have been cleared/closed by the ScriptSite.");
    if (this->GetHeapEnum() != nullptr)
    {TRACE_IT(36983);
        this->ClearHeapEnum();
    }

#ifdef BAILOUT_INJECTION
    if (Js::Configuration::Global.flags.IsEnabled(Js::BailOutByteCodeFlag)
        && Js::Configuration::Global.flags.BailOutByteCode.Empty())
    {TRACE_IT(36984);
        Output::Print(_u("Bail out byte code location count: %d"), this->bailOutByteCodeLocationCount);
    }
#endif

    Assert(processNativeCodeSize >= nativeCodeSize);
    ::InterlockedExchangeSubtract(&processNativeCodeSize, nativeCodeSize);

    PERF_COUNTER_DEC(Basic, ThreadContext);

#ifdef DYNAMIC_PROFILE_MUTATOR
    if (this->dynamicProfileMutator != nullptr)
    {TRACE_IT(36985);
        this->dynamicProfileMutator->Delete();
    }
#endif

#ifdef ENABLE_PROJECTION
#if DBG_DUMP
    if (this->projectionMemoryInformation)
    {TRACE_IT(36986);
        this->projectionMemoryInformation->Release();
        this->projectionMemoryInformation = nullptr;
    }
#endif
#endif
}

void
ThreadContext::SetJSRTRuntime(void* runtime)
{TRACE_IT(36987);
    Assert(jsrtRuntime == nullptr);
    jsrtRuntime = runtime;
#ifdef ENABLE_BASIC_TELEMETRY
    Telemetry::EnsureInitializeForJSRT();
#endif
}

void ThreadContext::CloseForJSRT()
{TRACE_IT(36988);
    // This is used for JSRT APIs only.
    Assert(this->jsrtRuntime);
#ifdef ENABLE_BASIC_TELEMETRY
    // log any relevant telemetry before disposing the current thread for cases which are properly shutdown
    Telemetry::OnJSRTThreadContextClose();
#endif
    ShutdownThreads();
}


ThreadContext* ThreadContext::GetContextForCurrentThread()
{TRACE_IT(36989);
    ThreadContextTLSEntry * tlsEntry = ThreadContextTLSEntry::GetEntryForCurrentThread();
    if (tlsEntry != nullptr)
    {TRACE_IT(36990);
        return static_cast<ThreadContext *>(tlsEntry->GetThreadContext());
    }
    return nullptr;
}

void ThreadContext::ValidateThreadContext()
    {TRACE_IT(36991);
#if DBG
    // verify the runtime pointer is valid.
        {
            BOOL found = FALSE;
            AutoCriticalSection autocs(ThreadContext::GetCriticalSection());
            ThreadContext* currentThreadContext = GetThreadContextList();
            while (currentThreadContext)
            {TRACE_IT(36992);
                if (currentThreadContext == this)
                {TRACE_IT(36993);
                    return;
                }
                currentThreadContext = currentThreadContext->Next();
            }
            AssertMsg(found, "invalid thread context");
        }

#endif
}

#if ENABLE_NATIVE_CODEGEN && defined(ENABLE_SIMDJS)
void ThreadContext::AddSimdFuncToMaps(Js::OpCode op, ...)
{
    Assert(simdFuncInfoToOpcodeMap != nullptr);
    Assert(simdOpcodeToSignatureMap != nullptr);

    va_list arguments;
    va_start(arguments, op);

    int argumentsCount = va_arg(arguments, int);
    AssertMsg(argumentsCount >= 0 && argumentsCount <= 20, "Invalid arguments count for SIMD opcode");
    if (argumentsCount == 0)
    {TRACE_IT(36994);
        // no info to add
        return;
    }
    Js::FunctionInfo *funcInfo = va_arg(arguments, Js::FunctionInfo*);
    AddSimdFuncInfo(op, funcInfo);

    SimdFuncSignature simdFuncSignature;
    simdFuncSignature.valid = true;
    simdFuncSignature.argCount = argumentsCount - 2; // arg count to Simd func = argumentsCount - FuncInfo and return Type fields.
    simdFuncSignature.returnType = va_arg(arguments, ValueType);
    simdFuncSignature.args = AnewArrayZ(this->GetThreadAlloc(), ValueType, simdFuncSignature.argCount);
    for (uint iArg = 0; iArg < simdFuncSignature.argCount; iArg++)
    {TRACE_IT(36995);
        simdFuncSignature.args[iArg] = va_arg(arguments, ValueType);
    }

    simdOpcodeToSignatureMap[Js::SIMDUtils::SimdOpcodeAsIndex(op)] = simdFuncSignature;

    va_end(arguments);
}

void ThreadContext::AddSimdFuncInfo(Js::OpCode op, Js::FunctionInfo *funcInfo)
{TRACE_IT(36996);
    // primary funcInfo
    simdFuncInfoToOpcodeMap->AddNew(funcInfo, op);
    // Entry points of SIMD loads/stores of non-full width all map to the same opcode. This is not captured in the opcode table, so add additional entry points here.
    switch (op)
    {
    case Js::OpCode::Simd128_LdArr_F4:
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Load1, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Load2, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Load3, op);
        break;
    case Js::OpCode::Simd128_StArr_F4:
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Store1, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Store2, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDFloat32x4Lib::EntryInfo::Store3, op);
        break;
    case Js::OpCode::Simd128_LdArr_I4:
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Load1, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Load2, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Load3, op);
        break;
    case Js::OpCode::Simd128_StArr_I4:
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Store1, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Store2, op);
        simdFuncInfoToOpcodeMap->AddNew(&Js::SIMDInt32x4Lib::EntryInfo::Store3, op);
        break;
    }
}

Js::OpCode ThreadContext::GetSimdOpcodeFromFuncInfo(Js::FunctionInfo * funcInfo)
{TRACE_IT(36997);
    Assert(simdFuncInfoToOpcodeMap != nullptr);
    if (simdFuncInfoToOpcodeMap->ContainsKey(funcInfo))
    {TRACE_IT(36998);
        return simdFuncInfoToOpcodeMap->Item(funcInfo);

    }
    return (Js::OpCode) 0;
}

void ThreadContext::GetSimdFuncSignatureFromOpcode(Js::OpCode op, SimdFuncSignature &funcSignature)
{TRACE_IT(36999);
    Assert(simdOpcodeToSignatureMap != nullptr);
    funcSignature = simdOpcodeToSignatureMap[Js::SIMDUtils::SimdOpcodeAsIndex(op)];
}
#endif

class AutoRecyclerPtr : public AutoPtr<Recycler>
{
public:
    AutoRecyclerPtr(Recycler * ptr) : AutoPtr<Recycler>(ptr) {TRACE_IT(37000);}
    ~AutoRecyclerPtr()
    {TRACE_IT(37001);
#if ENABLE_CONCURRENT_GC
        if (ptr != nullptr)
        {TRACE_IT(37002);
            ptr->ShutdownThread();
        }
#endif
    }
};

Recycler* ThreadContext::EnsureRecycler()
{TRACE_IT(37003);
    if (recycler == NULL)
    {TRACE_IT(37004);
        AutoRecyclerPtr newRecycler(HeapNew(Recycler, GetAllocationPolicyManager(), &pageAllocator, Js::Throw::OutOfMemory, Js::Configuration::Global.flags));
        newRecycler->Initialize(isOptimizedForManyInstances, &threadService); // use in-thread GC when optimizing for many instances
        newRecycler->SetCollectionWrapper(this);

#if ENABLE_NATIVE_CODEGEN
        // This may throw, so it needs to be after the recycler is initialized,
        // otherwise, the recycler dtor may encounter problems
#if !FLOATVAR
        // TODO: we only need one of the following, one for OOP jit and one for in-proc BG JIT
        AutoPtr<CodeGenNumberThreadAllocator> localCodeGenNumberThreadAllocator(
            HeapNew(CodeGenNumberThreadAllocator, newRecycler));
        AutoPtr<XProcNumberPageSegmentManager> localXProcNumberPageSegmentManager(
            HeapNew(XProcNumberPageSegmentManager, newRecycler));
#endif
#endif

        this->recyclableData.Root(RecyclerNewZ(newRecycler, RecyclableData, newRecycler), newRecycler);

        if (this->IsThreadBound())
        {TRACE_IT(37005);
            newRecycler->SetIsThreadBound();
        }

        // Assign the recycler to the ThreadContext after everything is initialized, because an OOM during initialization would
        // result in only partial initialization, so the 'recycler' member variable should remain null to cause full
        // reinitialization when requested later. Anything that happens after the Detach must have special cleanup code.
        this->recycler = newRecycler.Detach();

        try
        {TRACE_IT(37006);
#ifdef RECYCLER_WRITE_BARRIER
#ifdef _M_X64_OR_ARM64
            if (!RecyclerWriteBarrierManager::OnThreadInit())
            {TRACE_IT(37007);
                Js::Throw::OutOfMemory();
            }
#endif
#endif

            this->expirableObjectList = Anew(&this->threadAlloc, ExpirableObjectList, &this->threadAlloc);
            this->expirableObjectDisposeList = Anew(&this->threadAlloc, ExpirableObjectList, &this->threadAlloc);

            InitializePropertyMaps(); // has many dependencies on the recycler and other members of the thread context
#if ENABLE_NATIVE_CODEGEN
#if !FLOATVAR
            this->codeGenNumberThreadAllocator = localCodeGenNumberThreadAllocator.Detach();
            this->xProcNumberPageSegmentManager = localXProcNumberPageSegmentManager.Detach();
#endif
#endif
        }
        catch(...)
        {TRACE_IT(37008);
            // Initialization failed, undo what was done above. Callees that throw must clean up after themselves.
            if (this->recyclableData != nullptr)
            {TRACE_IT(37009);
                this->recyclableData.Unroot(this->recycler);
            }

            {TRACE_IT(37010);
                // AutoRecyclerPtr's destructor takes care of shutting down the background thread and deleting the recycler
                AutoRecyclerPtr recyclerToDelete(this->recycler);
                this->recycler = nullptr;
            }

            throw;
        }

        JS_ETW(EventWriteJSCRIPT_GC_INIT(this->recycler, this->GetHiResTimer()->Now()));
    }

#if DBG
    if (CONFIG_FLAG(RecyclerTest))
    {TRACE_IT(37011);
        StressTester test(recycler);
        test.Run();
    }
#endif

    return recycler;
}

Js::PropertyRecord const *
ThreadContext::GetPropertyName(Js::PropertyId propertyId)
{TRACE_IT(37012);
    // This API should only be use on the main thread
    Assert(GetCurrentThreadContextId() == (ThreadContextId)this);
    return this->GetPropertyNameImpl<false>(propertyId);
}

Js::PropertyRecord const *
ThreadContext::GetPropertyNameLocked(Js::PropertyId propertyId)
{TRACE_IT(37013);
    return GetPropertyNameImpl<true>(propertyId);
}

template <bool locked>
Js::PropertyRecord const *
ThreadContext::GetPropertyNameImpl(Js::PropertyId propertyId)
{TRACE_IT(37014);
    //TODO: Remove this when completely transformed to use PropertyRecord*. Currently this is only partially done,
    // and there are calls to GetPropertyName with InternalPropertyId.

    if (propertyId >= 0 && Js::IsInternalPropertyId(propertyId))
    {TRACE_IT(37015);
        return Js::InternalPropertyRecords::GetInternalPropertyName(propertyId);
    }

    int propertyIndex = propertyId - Js::PropertyIds::_none;

    if (propertyIndex < 0 || propertyIndex > propertyMap->GetLastIndex())
    {TRACE_IT(37016);
        propertyIndex = 0;
    }

    const Js::PropertyRecord * propertyRecord = nullptr;
    if (locked) {TRACE_IT(37017); propertyMap->LockResize(); }
    bool found = propertyMap->TryGetValueAt(propertyIndex, &propertyRecord);
    if (locked) {TRACE_IT(37018); propertyMap->UnlockResize(); }

    AssertMsg(found && propertyRecord != nullptr, "using invalid propertyid");
    return propertyRecord;
}

void
ThreadContext::FindPropertyRecord(Js::JavascriptString *pstName, Js::PropertyRecord const ** propertyRecord)
{TRACE_IT(37019);
    LPCWSTR psz = pstName->GetSz();
    FindPropertyRecord(psz, pstName->GetLength(), propertyRecord);
}

void
ThreadContext::FindPropertyRecord(__in LPCWSTR propertyName, __in int propertyNameLength, Js::PropertyRecord const ** propertyRecord)
{TRACE_IT(37020);
    EnterPinnedScope((volatile void **)propertyRecord);
    *propertyRecord = FindPropertyRecord(propertyName, propertyNameLength);
    LeavePinnedScope();
}

Js::PropertyRecord const *
ThreadContext::GetPropertyRecord(Js::PropertyId propertyId)
{TRACE_IT(37021);
    return GetPropertyNameLocked(propertyId);
}

bool
ThreadContext::IsNumericProperty(Js::PropertyId propertyId)
{TRACE_IT(37022);
    return GetPropertyRecord(propertyId)->IsNumeric();
}

const Js::PropertyRecord *
ThreadContext::FindPropertyRecord(const char16 * propertyName, int propertyNameLength)
{TRACE_IT(37023);
    Js::PropertyRecord const * propertyRecord = nullptr;

    if (IsDirectPropertyName(propertyName, propertyNameLength))
    {TRACE_IT(37024);
        propertyRecord = propertyNamesDirect[propertyName[0]];
        Assert(propertyRecord == propertyMap->LookupWithKey(Js::HashedCharacterBuffer<char16>(propertyName, propertyNameLength)));
    }
    else
    {TRACE_IT(37025);
        propertyRecord = propertyMap->LookupWithKey(Js::HashedCharacterBuffer<char16>(propertyName, propertyNameLength));
    }

    return propertyRecord;
}

Js::PropertyRecord const *
ThreadContext::UncheckedAddPropertyId(__in LPCWSTR propertyName, __in int propertyNameLength, bool bind, bool isSymbol)
{TRACE_IT(37026);
    return UncheckedAddPropertyId(JsUtil::CharacterBuffer<WCHAR>(propertyName, propertyNameLength), bind, isSymbol);
}

void ThreadContext::InitializePropertyMaps()
{TRACE_IT(37027);
    Assert(this->recycler != nullptr);
    Assert(this->recyclableData != nullptr);
    Assert(this->propertyMap == nullptr);
    Assert(this->caseInvariantPropertySet == nullptr);

    try
    {TRACE_IT(37028);
        this->propertyMap = HeapNew(PropertyMap, &HeapAllocator::Instance, TotalNumberOfBuiltInProperties + 700);
        this->recyclableData->boundPropertyStrings = RecyclerNew(this->recycler, JsUtil::List<Js::PropertyRecord const*>, this->recycler);

        memset(propertyNamesDirect, 0, 128*sizeof(Js::PropertyRecord *));

        Js::JavascriptLibrary::InitializeProperties(this);
        InitializeAdditionalProperties(this);

        //Js::JavascriptLibrary::InitializeDOMProperties(this);
    }
    catch(...)
    {TRACE_IT(37029);
        // Initialization failed, undo what was done above. Callees that throw must clean up after themselves. The recycler will
        // be trashed, so clear members that point to recyclable memory. Stuff in 'recyclableData' will be taken care of by the
        // recycler, and the 'recyclableData' instance will be trashed as well.
        if (this->propertyMap != nullptr)
        {TRACE_IT(37030);
            HeapDelete(this->propertyMap);
        }
        this->propertyMap = nullptr;

        this->caseInvariantPropertySet = nullptr;
        memset(propertyNamesDirect, 0, 128*sizeof(Js::PropertyRecord *));
        throw;
    }
}

void ThreadContext::UncheckedAddBuiltInPropertyId()
{TRACE_IT(37031);
    for (int i = 0; i < _countof(builtInPropertyRecords); i++)
    {TRACE_IT(37032);
        AddPropertyRecordInternal(builtInPropertyRecords[i]);
    }
}

bool
ThreadContext::IsDirectPropertyName(const char16 * propertyName, int propertyNameLength)
{TRACE_IT(37033);
    return ((propertyNameLength == 1) && ((propertyName[0] & 0xFF80) == 0));
}

RecyclerWeakReference<const Js::PropertyRecord> *
ThreadContext::CreatePropertyRecordWeakRef(const Js::PropertyRecord * propertyRecord)
{TRACE_IT(37034);
    RecyclerWeakReference<const Js::PropertyRecord> * propertyRecordWeakRef;

    if (propertyRecord->IsBound())
    {TRACE_IT(37035);
        // Create a fake weak ref
        propertyRecordWeakRef = RecyclerNewLeaf(this->recycler, StaticPropertyRecordReference, propertyRecord);
    }
    else
    {TRACE_IT(37036);
        propertyRecordWeakRef = recycler->CreateWeakReferenceHandle(propertyRecord);
    }

    return propertyRecordWeakRef;
}

Js::PropertyRecord const *
ThreadContext::UncheckedAddPropertyId(JsUtil::CharacterBuffer<WCHAR> const& propertyName, bool bind, bool isSymbol)
{TRACE_IT(37037);
#if ENABLE_TTD
    if(isSymbol & this->IsRuntimeInTTDMode())
    {TRACE_IT(37038);
        if(this->TTDContext->GetActiveScriptContext() != nullptr && this->TTDContext->GetActiveScriptContext()->ShouldPerformReplayAction())
        {TRACE_IT(37039);
            //We reload all properties that occour in the trace so they only way we get here in TTD mode is:
            //(1) if the program is creating a new symbol (which always gets a fresh id) and we should recreate it or
            //(2) if it is forcing arguments in debug parse mode (instead of regular which we recorded in)
            Js::PropertyId propertyId = Js::Constants::NoProperty;
            this->TTDLog->ReplaySymbolCreationEvent(&propertyId);

            //Don't recreate the symbol below, instead return the known symbol by looking up on the pid
            const Js::PropertyRecord* res = this->GetPropertyName(propertyId);
            AssertMsg(res != nullptr, "This should never happen!!!");

            return res;
        }
    }
#endif

    this->propertyMap->EnsureCapacity();

    // Automatically bind direct (single-character) property names, so that they can be
    // stored in the direct property table
    if (IsDirectPropertyName(propertyName.GetBuffer(), propertyName.GetLength()))
    {TRACE_IT(37040);
        bind = true;
    }

    // Create the PropertyRecord

    int length = propertyName.GetLength();
    uint bytelength = sizeof(char16) * length;

    uint32 indexVal = 0;

    // Symbol properties cannot be numeric since their description is not to be used!
    bool isNumeric = !isSymbol && Js::PropertyRecord::IsPropertyNameNumeric(propertyName.GetBuffer(), propertyName.GetLength(), &indexVal);

    uint hash = JsUtil::CharacterBuffer<WCHAR>::StaticGetHashCode(propertyName.GetBuffer(), propertyName.GetLength());

    size_t allocLength = bytelength + sizeof(char16) + (isNumeric ? sizeof(uint32) : 0);

    // If it's bound, create it in the thread arena, along with a fake weak ref
    Js::PropertyRecord * propertyRecord;
    if (bind)
    {TRACE_IT(37041);
        propertyRecord = AnewPlus(GetThreadAlloc(), allocLength, Js::PropertyRecord, bytelength, isNumeric, hash, isSymbol);
        propertyRecord->isBound = true;
    }
    else
    {TRACE_IT(37042);
        propertyRecord = RecyclerNewFinalizedLeafPlus(recycler, allocLength, Js::PropertyRecord, bytelength, isNumeric, hash, isSymbol);
    }

    // Copy string and numeric info
    char16* buffer = (char16 *)(propertyRecord + 1);
    js_memcpy_s(buffer, bytelength, propertyName.GetBuffer(), bytelength);
    buffer[length] = _u('\0');

    if (isNumeric)
    {TRACE_IT(37043);
        *(uint32 *)(buffer + length + 1) = indexVal;
        Assert(propertyRecord->GetNumericValue() == indexVal);
    }

    Js::PropertyId propertyId = this->GetNextPropertyId();

#if ENABLE_TTD
    if(isSymbol & this->IsRuntimeInTTDMode())
    {TRACE_IT(37044);
        if(this->TTDContext->GetActiveScriptContext() != nullptr && this->TTDContext->GetActiveScriptContext()->ShouldPerformRecordAction())
        {TRACE_IT(37045);
            this->TTDLog->RecordSymbolCreationEvent(propertyId);
        }
    }
#endif

    propertyRecord->pid = propertyId;

    AddPropertyRecordInternal(propertyRecord);

    return propertyRecord;
}

void
ThreadContext::AddPropertyRecordInternal(const Js::PropertyRecord * propertyRecord)
{TRACE_IT(37046);
    // At this point the PropertyRecord is constructed but not added to the map.

    const char16 * propertyName = propertyRecord->GetBuffer();
    int propertyNameLength = propertyRecord->GetLength();
    Js::PropertyId propertyId = propertyRecord->GetPropertyId();

    Assert(propertyId == GetNextPropertyId());
    Assert(!IsActivePropertyId(propertyId));

#if DBG
    // Only Assert we can't find the property if we are not adding a symbol.
    // For a symbol, the propertyName is not used and may collide with something in the map already.
    if (!propertyRecord->IsSymbol())
    {
        Assert(FindPropertyRecord(propertyName, propertyNameLength) == nullptr);
    }
#endif

#if ENABLE_TTD
    if(this->IsRuntimeInTTDMode())
    {TRACE_IT(37047);
        this->TTDLog->AddPropertyRecord(propertyRecord);
    }
#endif

    // Add to the map
    propertyMap->Add(propertyRecord);

#if ENABLE_NATIVE_CODEGEN
    if (m_jitNumericProperties)
    {TRACE_IT(37048);
        if (propertyRecord->IsNumeric())
        {TRACE_IT(37049);
            m_jitNumericProperties->Set(propertyRecord->GetPropertyId());
            m_jitNeedsPropertyUpdate = true;
        }
    }
#endif

    PropertyRecordTrace(_u("Added property '%s' at 0x%08x, pid = %d\n"), propertyName, propertyRecord, propertyId);

    // Do not store the pid for symbols in the direct property name table.
    // We don't want property ids for symbols to be searchable anyway.
    if (!propertyRecord->IsSymbol() && IsDirectPropertyName(propertyName, propertyNameLength))
    {TRACE_IT(37050);
        // Store the pids for single character properties in the propertyNamesDirect array.
        // This property record should have been created as bound by the caller.
        Assert(propertyRecord->IsBound());
        Assert(propertyNamesDirect[propertyName[0]] == nullptr);
        propertyNamesDirect[propertyName[0]] = propertyRecord;
    }

    if (caseInvariantPropertySet)
    {TRACE_IT(37051);
        AddCaseInvariantPropertyRecord(propertyRecord);
    }

    // Check that everything was added correctly
#if DBG
    // Only Assert we can find the property if we are not adding a symbol.
    // For a symbol, the propertyName is not used and we won't be able to look the pid up via name.
    if (!propertyRecord->IsSymbol())
    {
        Assert(FindPropertyRecord(propertyName, propertyNameLength) == propertyRecord);
    }
    // We will still be able to lookup the symbol property by the property id, so go ahead and check that.
    Assert(GetPropertyName(propertyRecord->GetPropertyId()) == propertyRecord);
#endif
    JS_ETW_INTERNAL(EventWriteJSCRIPT_HOSTING_PROPERTYID_LIST(propertyRecord, propertyRecord->GetBuffer()));
}

void
ThreadContext::AddCaseInvariantPropertyRecord(const Js::PropertyRecord * propertyRecord)
{TRACE_IT(37052);
    Assert(this->caseInvariantPropertySet != nullptr);

    // Create a weak reference to the property record here (since we no longer use weak refs in the property map)
    RecyclerWeakReference<const Js::PropertyRecord> * propertyRecordWeakRef = CreatePropertyRecordWeakRef(propertyRecord);

    JsUtil::CharacterBuffer<WCHAR> newPropertyName(propertyRecord->GetBuffer(), propertyRecord->GetLength());
    Js::CaseInvariantPropertyListWithHashCode* list;
    if (!FindExistingPropertyRecord(newPropertyName, &list))
    {TRACE_IT(37053);
        // This binds all the property string that is key in this map with no hope of reclaiming them
        // TODO: do better
        list = RecyclerNew(recycler, Js::CaseInvariantPropertyListWithHashCode, recycler, 1);
        // Do the add first so that the list is non-empty and we can calculate its hashcode correctly
        list->Add(propertyRecordWeakRef);

        // This will calculate the hashcode
        caseInvariantPropertySet->Add(list);
    }
    else
    {TRACE_IT(37054);
        list->Add(propertyRecordWeakRef);
    }
}

void
ThreadContext::BindPropertyRecord(const Js::PropertyRecord * propertyRecord)
{TRACE_IT(37055);
    if (!propertyRecord->IsBound())
    {TRACE_IT(37056);
        Assert(!this->recyclableData->boundPropertyStrings->Contains(propertyRecord));

        this->recyclableData->boundPropertyStrings->Add(propertyRecord);

        // Cast around constness to set propertyRecord as bound
        const_cast<Js::PropertyRecord *>(propertyRecord)->isBound = true;
    }
}

void ThreadContext::GetOrAddPropertyId(__in LPCWSTR propertyName, __in int propertyNameLength, Js::PropertyRecord const ** propertyRecord)
{TRACE_IT(37057);
    GetOrAddPropertyId(JsUtil::CharacterBuffer<WCHAR>(propertyName, propertyNameLength), propertyRecord);
}

void ThreadContext::GetOrAddPropertyId(JsUtil::CharacterBuffer<WCHAR> const& propertyName, Js::PropertyRecord const ** propRecord)
{TRACE_IT(37058);
    EnterPinnedScope((volatile void **)propRecord);
    *propRecord = GetOrAddPropertyRecord(propertyName);
    LeavePinnedScope();
}

const Js::PropertyRecord *
ThreadContext::GetOrAddPropertyRecordImpl(JsUtil::CharacterBuffer<char16> propertyName, bool bind)
{TRACE_IT(37059);
    // Make sure the recycler is around so that we can take weak references to the property strings
    EnsureRecycler();

    const Js::PropertyRecord * propertyRecord;
    FindPropertyRecord(propertyName.GetBuffer(), propertyName.GetLength(), &propertyRecord);

    if (propertyRecord == nullptr)
    {TRACE_IT(37060);
        propertyRecord = UncheckedAddPropertyId(propertyName, bind);
    }
    else
    {TRACE_IT(37061);
        // PropertyRecord exists, but may not be bound.  Bind now if requested.
        if (bind)
        {TRACE_IT(37062);
            BindPropertyRecord(propertyRecord);
        }
    }

    Assert(propertyRecord != nullptr);
    Assert(!bind || propertyRecord->IsBound());

    return propertyRecord;
}

void ThreadContext::AddBuiltInPropertyRecord(const Js::PropertyRecord *propertyRecord)
{TRACE_IT(37063);
    this->AddPropertyRecordInternal(propertyRecord);
}

BOOL ThreadContext::IsNumericPropertyId(Js::PropertyId propertyId, uint32* value)
{TRACE_IT(37064);
    Js::PropertyRecord const * propertyRecord = this->GetPropertyName(propertyId);
    Assert(propertyRecord != nullptr);
    if (propertyRecord == nullptr || !propertyRecord->IsNumeric())
    {TRACE_IT(37065);
        return false;
    }
    *value = propertyRecord->GetNumericValue();
    return true;
}

bool ThreadContext::IsActivePropertyId(Js::PropertyId pid)
{TRACE_IT(37066);
    Assert(pid != Js::Constants::NoProperty);
    if (Js::IsInternalPropertyId(pid))
    {TRACE_IT(37067);
        return true;
    }

    int propertyIndex = pid - Js::PropertyIds::_none;

    const Js::PropertyRecord * propertyRecord;
    if (propertyMap->TryGetValueAt(propertyIndex, &propertyRecord) && propertyRecord != nullptr)
    {TRACE_IT(37068);
        return true;
    }

    return false;
}

void ThreadContext::InvalidatePropertyRecord(const Js::PropertyRecord * propertyRecord)
{TRACE_IT(37069);
    InternalInvalidateProtoTypePropertyCaches(propertyRecord->GetPropertyId());     // use the internal version so we don't check for active property id
#if ENABLE_NATIVE_CODEGEN
    if (propertyRecord->IsNumeric() && m_jitNumericProperties)
    {TRACE_IT(37070);
        m_jitNumericProperties->Clear(propertyRecord->GetPropertyId());
        m_jitNeedsPropertyUpdate = true;
    }
#endif
    this->propertyMap->Remove(propertyRecord);
    PropertyRecordTrace(_u("Reclaimed property '%s' at 0x%08x, pid = %d\n"),
        propertyRecord->GetBuffer(), propertyRecord, propertyRecord->GetPropertyId());
}

Js::PropertyId ThreadContext::GetNextPropertyId()
{TRACE_IT(37071);
    return this->propertyMap->GetNextIndex() + Js::PropertyIds::_none;
}

Js::PropertyId ThreadContext::GetMaxPropertyId()
{TRACE_IT(37072);
    auto maxPropertyId = this->propertyMap->Count() + Js::InternalPropertyIds::Count;
    return maxPropertyId;
}

void ThreadContext::CreateNoCasePropertyMap()
{TRACE_IT(37073);
    Assert(caseInvariantPropertySet == nullptr);
    caseInvariantPropertySet = RecyclerNew(recycler, PropertyNoCaseSetType, recycler, 173);

    // Prevent the set from being reclaimed
    // Individual items in the set can be reclaimed though since they're lists of weak references
    // The lists themselves can be reclaimed when all the weak references in them are cleared
    this->recyclableData->caseInvariantPropertySet = caseInvariantPropertySet;

    // Note that we are allocating from the recycler below, so we may cause a GC at any time, which
    // could cause PropertyRecords to be collected and removed from the propertyMap.
    // Thus, don't use BaseDictionary::Map here, as it cannot tolerate changes while mapping.
    // Instead, walk the PropertyRecord entries in index order.  This will work even if a GC occurs.

    for (int propertyIndex = 0; propertyIndex <= this->propertyMap->GetLastIndex(); propertyIndex++)
    {TRACE_IT(37074);
        const Js::PropertyRecord * propertyRecord;
        if (this->propertyMap->TryGetValueAt(propertyIndex, &propertyRecord) && propertyRecord != nullptr)
        {TRACE_IT(37075);
            AddCaseInvariantPropertyRecord(propertyRecord);
        }
    }
}

JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>*
ThreadContext::FindPropertyIdNoCase(Js::ScriptContext * scriptContext, LPCWSTR propertyName, int propertyNameLength)
{TRACE_IT(37076);
    return ThreadContext::FindPropertyIdNoCase(scriptContext, JsUtil::CharacterBuffer<WCHAR>(propertyName,  propertyNameLength));
}

JsUtil::List<const RecyclerWeakReference<Js::PropertyRecord const>*>*
ThreadContext::FindPropertyIdNoCase(Js::ScriptContext * scriptContext, JsUtil::CharacterBuffer<WCHAR> const& propertyName)
{TRACE_IT(37077);
    if (caseInvariantPropertySet == nullptr)
    {TRACE_IT(37078);
        this->CreateNoCasePropertyMap();
    }
    Js::CaseInvariantPropertyListWithHashCode* list;
    if (FindExistingPropertyRecord(propertyName, &list))
    {TRACE_IT(37079);
        return list;
    }
    return nullptr;
}

bool
ThreadContext::FindExistingPropertyRecord(_In_ JsUtil::CharacterBuffer<WCHAR> const& propertyName, Js::CaseInvariantPropertyListWithHashCode** list)
{TRACE_IT(37080);
    Js::CaseInvariantPropertyListWithHashCode* l = this->caseInvariantPropertySet->LookupWithKey(propertyName);

    (*list) = l;

    return (l != nullptr);
}

void ThreadContext::CleanNoCasePropertyMap()
{TRACE_IT(37081);
    if (this->caseInvariantPropertySet != nullptr)
    {TRACE_IT(37082);
        this->caseInvariantPropertySet->MapAndRemoveIf([](Js::CaseInvariantPropertyListWithHashCode* value) -> bool {
            if (value && value->Count() == 0)
            {TRACE_IT(37083);
                // Remove entry
                return true;
            }

            // Keep entry
            return false;
        });
    }
}

void
ThreadContext::ForceCleanPropertyMap()
{TRACE_IT(37084);
    // No-op now that we no longer use weak refs
}

#if ENABLE_NATIVE_CODEGEN
JsUtil::JobProcessor *
ThreadContext::GetJobProcessor()
{TRACE_IT(37085);
    if(bgJit && isOptimizedForManyInstances)
    {TRACE_IT(37086);
        return ThreadBoundThreadContextManager::GetSharedJobProcessor();
    }

    if (!jobProcessor)
    {TRACE_IT(37087);
        if(bgJit && !isOptimizedForManyInstances)
        {TRACE_IT(37088);
            jobProcessor = HeapNew(JsUtil::BackgroundJobProcessor, GetAllocationPolicyManager(), &threadService, false /*disableParallelThreads*/);
        }
        else
        {TRACE_IT(37089);
            jobProcessor = HeapNew(JsUtil::ForegroundJobProcessor);
        }
    }
    return jobProcessor;
}
#endif

void
ThreadContext::RegisterCodeGenRecyclableData(Js::CodeGenRecyclableData *const codeGenRecyclableData)
{TRACE_IT(37090);
    Assert(codeGenRecyclableData);
    Assert(recyclableData);

    // Linking must not be done concurrently with unlinking (caller must use lock)
    recyclableData->codeGenRecyclableDatas.LinkToEnd(codeGenRecyclableData);
}

void
ThreadContext::UnregisterCodeGenRecyclableData(Js::CodeGenRecyclableData *const codeGenRecyclableData)
{TRACE_IT(37091);
    Assert(codeGenRecyclableData);

    if(!recyclableData)
    {TRACE_IT(37092);
        // The thread context's recyclable data may have already been released to the recycler if we're shutting down
        return;
    }

    // Unlinking may be done from a background thread, but not concurrently with linking (caller must use lock).  Partial unlink
    // does not zero the previous and next links for the unlinked node so that the recycler can scan through the node from the
    // main thread.
    recyclableData->codeGenRecyclableDatas.UnlinkPartial(codeGenRecyclableData);
}



uint
ThreadContext::EnterScriptStart(Js::ScriptEntryExitRecord * record, bool doCleanup)
{TRACE_IT(37093);
    Recycler * recycler = this->GetRecycler();
    Assert(recycler->IsReentrantState());
    JS_ETW_INTERNAL(EventWriteJSCRIPT_RUN_START(this,0));

    // Increment the callRootLevel early so that Dispose ran during FinishConcurrent will not close the current scriptContext
    uint oldCallRootLevel = this->callRootLevel++;

    if (oldCallRootLevel == 0)
    {TRACE_IT(37094);
        Assert(!this->hasThrownPendingException);
        RECORD_TIMESTAMP(lastScriptStartTime);
        InterruptPoller *poller = this->interruptPoller;
        if (poller)
        {TRACE_IT(37095);
            poller->StartScript();
        }

        recycler->SetIsInScript(true);
        if (doCleanup)
        {TRACE_IT(37096);
            recycler->EnterIdleDecommit();
#if ENABLE_CONCURRENT_GC
            recycler->FinishConcurrent<FinishConcurrentOnEnterScript>();
#endif
            if (threadServiceWrapper == NULL)
            {TRACE_IT(37097);
                // Reschedule the next collection at the start of the script.
                recycler->ScheduleNextCollection();
            }
        }
    }

    this->PushEntryExitRecord(record);

    AssertMsg(!this->IsScriptActive(),
              "Missing EnterScriptEnd or LeaveScriptStart");
    this->isScriptActive = true;
    recycler->SetIsScriptActive(true);

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::RunPhase))
    {TRACE_IT(37098);
        Output::Trace(Js::RunPhase, _u("%p> EnterScriptStart(%p): Level %d\n"), ::GetCurrentThreadId(), this, this->callRootLevel);
        Output::Flush();
    }
#endif

    return oldCallRootLevel;
}


void
ThreadContext::EnterScriptEnd(Js::ScriptEntryExitRecord * record, bool doCleanup)
{TRACE_IT(37099);
#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::RunPhase))
    {TRACE_IT(37100);
        Output::Trace(Js::RunPhase, _u("%p> EnterScriptEnd  (%p): Level %d\n"), ::GetCurrentThreadId(), this, this->callRootLevel);
        Output::Flush();
    }
#endif
    this->PopEntryExitRecord(record);
    AssertMsg(this->IsScriptActive(),
              "Missing EnterScriptStart or LeaveScriptEnd");
    this->isScriptActive = false;
    this->GetRecycler()->SetIsScriptActive(false);
    this->callRootLevel--;
#ifdef EXCEPTION_CHECK
    ExceptionCheck::SetHandledExceptionType(record->handledExceptionType);
#endif
#ifdef RECYCLER_MEMORY_VERIFY
    recycler->Verify(Js::RunPhase);
#endif

    if (this->callRootLevel == 0)
    {TRACE_IT(37101);
        RECORD_TIMESTAMP(lastScriptEndTime);
        this->GetRecycler()->SetIsInScript(false);
        InterruptPoller *poller = this->interruptPoller;
        if (poller)
        {TRACE_IT(37102);
            poller->EndScript();
        }
        ClosePendingProjectionContexts();
        ClosePendingScriptContexts();
        Assert(rootPendingClose == nullptr);

        if (this->hasThrownPendingException)
        {TRACE_IT(37103);
            // We have some cases where the thread instant of JavascriptExceptionObject
            // are ignored and not clear. To avoid leaks, clear it here when
            // we are not in script, where no one should be using these JavascriptExceptionObject
            this->ClearPendingOOMError();
            this->ClearPendingSOError();
            this->hasThrownPendingException = false;
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.FreeRejittedCode)
#endif
        {TRACE_IT(37104);
            // Since we're no longer in script, old entry points can now be collected
            Js::FunctionEntryPointInfo* current = this->recyclableData->oldEntryPointInfo;
            this->recyclableData->oldEntryPointInfo = nullptr;

            // Clear out the next pointers so older entry points wont be held on
            // as a false positive

            while (current != nullptr)
            {TRACE_IT(37105);
                Js::FunctionEntryPointInfo* next = current->nextEntryPoint;
                current->nextEntryPoint = nullptr;
                current = next;
            }
        }

        if (doCleanup)
        {TRACE_IT(37106);
            ThreadServiceWrapper* threadServiceWrapper = GetThreadServiceWrapper();
            if (!threadServiceWrapper || !threadServiceWrapper->ScheduleNextCollectOnExit())
            {TRACE_IT(37107);
                // Do the idle GC now if we fail schedule one.
                recycler->CollectNow<CollectOnScriptExit>();
            }
            recycler->LeaveIdleDecommit();
        }
    }

    if (doCleanup)
    {TRACE_IT(37108);
        PHASE_PRINT_TRACE1(Js::DisposePhase, _u("[Dispose] NeedDispose in EnterScriptEnd: %d\n"), this->recycler->NeedDispose());

        if (this->recycler->NeedDispose())
        {TRACE_IT(37109);
            this->recycler->FinishDisposeObjectsNow<FinishDispose>();
        }
    }

    JS_ETW_INTERNAL(EventWriteJSCRIPT_RUN_STOP(this,0));
}

void
ThreadContext::SetForceOneIdleCollection()
{TRACE_IT(37110);
    ThreadServiceWrapper* threadServiceWrapper = GetThreadServiceWrapper();
    if (threadServiceWrapper)
    {TRACE_IT(37111);
        threadServiceWrapper->SetForceOneIdleCollection();
    }

}

BOOLEAN
ThreadContext::IsOnStack(void const *ptr)
{TRACE_IT(37112);
#if defined(_M_IX86) && defined(_MSC_VER)
    return ptr < (void*)__readfsdword(0x4) && ptr >= (void*)__readfsdword(0xE0C);
#elif defined(_M_AMD64) && defined(_MSC_VER)
    return ptr < (void*)__readgsqword(0x8) && ptr >= (void*)__readgsqword(0x1478);
#elif defined(_M_ARM)
    ULONG lowLimit, highLimit;
    ::GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    bool isOnStack = (void*)lowLimit <= ptr && ptr < (void*)highLimit;
    return isOnStack;
#elif defined(_M_ARM64)
    ULONG64 lowLimit, highLimit;
    ::GetCurrentThreadStackLimits(&lowLimit, &highLimit);
    bool isOnStack = (void*)lowLimit <= ptr && ptr < (void*)highLimit;
    return isOnStack;
#elif !defined(_MSC_VER)
    return ::IsAddressOnStack((ULONG_PTR) ptr);
#else
    AssertMsg(FALSE, "IsOnStack -- not implemented yet case");
    Js::Throw::NotImplemented();
    return false;
#endif
}

 size_t
 ThreadContext::GetStackLimitForCurrentThread() const
{TRACE_IT(37113);
    FAULTINJECT_SCRIPT_TERMINATION;
    size_t limit = this->stackLimitForCurrentThread;
    Assert(limit == Js::Constants::StackLimitForScriptInterrupt
        || !this->GetStackProber()
        || limit == this->GetStackProber()->GetScriptStackLimit());
    return limit;
}
void
ThreadContext::SetStackLimitForCurrentThread(size_t limit)
{TRACE_IT(37114);
    this->stackLimitForCurrentThread = limit;
}

_NOINLINE //Win8 947081: might use wrong _AddressOfReturnAddress() if this and caller are inlined
bool
ThreadContext::IsStackAvailable(size_t size)
{
    bool stackAvailable = true;
    FAULTINJECT_STACK_PROBE

    if (size == 0 && stackAvailable)
    {
        return true;
    }

    TRACE_IT(37115);
    size_t sp = (size_t)_AddressOfReturnAddress();
    size_t stackLimit = this->GetStackLimitForCurrentThread();

    // Verify that JIT'd frames didn't mess up the ABI stack alignment
    Assert(((uintptr_t)sp & (AutoSystemInfo::StackAlign - 1)) == (sizeof(void*) & (AutoSystemInfo::StackAlign - 1)));

#if DBG
    this->GetStackProber()->AdjustKnownStackLimit(sp, size);
#endif

    if (stackAvailable)
    {TRACE_IT(37116);
        if (sp > size && (sp - size) > stackLimit)
        {
            return true;
        }
    }

    if (sp <= stackLimit)
    {TRACE_IT(37117);
        if (stackLimit == Js::Constants::StackLimitForScriptInterrupt)
        {TRACE_IT(37118);
            if (sp <= this->GetStackProber()->GetScriptStackLimit())
            {TRACE_IT(37119);
                // Take down the process if we cant recover from the stack overflow
                Js::Throw::FatalInternalError();
            }
        }
    }

    return false;
}

_NOINLINE //Win8 947081: might use wrong _AddressOfReturnAddress() if this and caller are inlined
bool
ThreadContext::IsStackAvailableNoThrow(size_t size)
{TRACE_IT(37120);
    size_t sp = (size_t)_AddressOfReturnAddress();
    size_t stackLimit = this->GetStackLimitForCurrentThread();
    bool stackAvailable = (sp > stackLimit) && (sp > size) && ((sp - size) > stackLimit);

    FAULTINJECT_STACK_PROBE

    return stackAvailable;
}

/* static */ bool
ThreadContext::IsCurrentStackAvailable(size_t size)
{TRACE_IT(37121);
    ThreadContext *currentContext = GetContextForCurrentThread();
    Assert(currentContext);

    return currentContext->IsStackAvailable(size);
}

/*
    returnAddress will be passed in the stackprobe call at the beginning of interpreter frame.
    We need to probe the stack before we link up the InterpreterFrame structure in threadcontext,
    and if we throw there, the stack walker might get confused when trying to identify a frame
    is interpreter frame by comparing the current ebp in ebp chain with return address specified
    in the last InterpreterFrame linked in threadcontext. We need to pass in the return address
    of the probing frame to skip the right one (we need to skip first match in a->a->a recursion,
    but not in a->b->a recursion).
*/
void
ThreadContext::ProbeStackNoDispose(size_t size, Js::ScriptContext *scriptContext, PVOID returnAddress)
{TRACE_IT(37122);
    AssertCanHandleStackOverflow();
    if (!this->IsStackAvailable(size))
    {TRACE_IT(37123);
        if (this->IsExecutionDisabled())
        {TRACE_IT(37124);
            // The probe failed because we hammered the stack limit to trigger script interrupt.
            Assert(this->DoInterruptProbe());
            throw Js::ScriptAbortException();
        }

        Js::Throw::StackOverflow(scriptContext, returnAddress);
    }

    // Use every Nth stack probe as a QC trigger.
    if (AutoSystemInfo::ShouldQCMoreFrequently() && this->HasInterruptPoller() && this->IsScriptActive())
    {TRACE_IT(37125);
        ++(this->stackProbeCount);
        if (this->stackProbeCount > ThreadContext::StackProbePollThreshold)
        {TRACE_IT(37126);
            this->stackProbeCount = 0;
            this->CheckInterruptPoll();
        }
    }
}

void
ThreadContext::ProbeStack(size_t size, Js::ScriptContext *scriptContext, PVOID returnAddress)
{TRACE_IT(37127);
    this->ProbeStackNoDispose(size, scriptContext, returnAddress);

    // BACKGROUND-GC TODO: If we're stuck purely in JITted code, we should have the
    // background GC thread modify the threads stack limit to trigger the runtime stack probe
    if (this->callDispose && this->recycler->NeedDispose())
    {TRACE_IT(37128);
        PHASE_PRINT_TRACE1(Js::DisposePhase, _u("[Dispose] NeedDispose in ProbeStack: %d\n"), this->recycler->NeedDispose());
        this->recycler->FinishDisposeObjectsNow<FinishDisposeTimed>();
    }
}

void
ThreadContext::ProbeStack(size_t size, Js::RecyclableObject * obj, Js::ScriptContext *scriptContext)
{TRACE_IT(37129);
    AssertCanHandleStackOverflowCall(obj->IsExternal() ||
        (Js::JavascriptOperators::GetTypeId(obj) == Js::TypeIds_Function &&
        Js::JavascriptFunction::FromVar(obj)->IsExternalFunction()));
    if (!this->IsStackAvailable(size))
    {TRACE_IT(37130);
        if (this->IsExecutionDisabled())
        {TRACE_IT(37131);
            // The probe failed because we hammered the stack limit to trigger script interrupt.
            Assert(this->DoInterruptProbe());
            throw Js::ScriptAbortException();
        }

        if (obj->IsExternal() ||
            (Js::JavascriptOperators::GetTypeId(obj) == Js::TypeIds_Function &&
            Js::JavascriptFunction::FromVar(obj)->IsExternalFunction()))
        {TRACE_IT(37132);
            Js::JavascriptError::ThrowStackOverflowError(scriptContext);
        }
        Js::Throw::StackOverflow(scriptContext, NULL);
    }

}

void
ThreadContext::ProbeStack(size_t size)
{TRACE_IT(37133);
    Assert(this->IsScriptActive());

    Js::ScriptEntryExitRecord *entryExitRecord = this->GetScriptEntryExit();
    Assert(entryExitRecord);

    Js::ScriptContext *scriptContext = entryExitRecord->scriptContext;
    Assert(scriptContext);

    this->ProbeStack(size, scriptContext);
}

/* static */ void
ThreadContext::ProbeCurrentStack(size_t size, Js::ScriptContext *scriptContext)
{TRACE_IT(37134);
    Assert(scriptContext != nullptr);
    Assert(scriptContext->GetThreadContext() == GetContextForCurrentThread());
    scriptContext->GetThreadContext()->ProbeStack(size, scriptContext);
}

/* static */ void
ThreadContext::ProbeCurrentStackNoDispose(size_t size, Js::ScriptContext *scriptContext)
{TRACE_IT(37135);
    Assert(scriptContext != nullptr);
    Assert(scriptContext->GetThreadContext() == GetContextForCurrentThread());
    scriptContext->GetThreadContext()->ProbeStackNoDispose(size, scriptContext);
}

template <bool leaveForHost>
void
ThreadContext::LeaveScriptStart(void * frameAddress)
{TRACE_IT(37136);
    Assert(this->IsScriptActive());

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::RunPhase))
    {TRACE_IT(37137);
        Output::Trace(Js::RunPhase, _u("%p> LeaveScriptStart(%p): Level %d\n"), ::GetCurrentThreadId(), this, this->callRootLevel);
        Output::Flush();
    }
#endif

    Js::ScriptEntryExitRecord * entryExitRecord = this->GetScriptEntryExit();

    AssertMsg(entryExitRecord && entryExitRecord->frameIdOfScriptExitFunction == nullptr,
              "Missing LeaveScriptEnd or EnterScriptStart");

    entryExitRecord->frameIdOfScriptExitFunction = frameAddress;

    this->isScriptActive = false;
    this->GetRecycler()->SetIsScriptActive(false);

    AssertMsg(!(leaveForHost && this->IsDisableImplicitCall()),
        "Disable implicit call should have been caught before leaving script for host");

    // Save the implicit call flags
    entryExitRecord->savedImplicitCallFlags = this->GetImplicitCallFlags();

    // clear the hasReentered to detect if we have reentered into script
    entryExitRecord->hasReentered = false;
#if DBG || defined(PROFILE_EXEC)
    entryExitRecord->leaveForHost = leaveForHost;
#endif
#if DBG
    entryExitRecord->leaveForAsyncHostOperation = false;
#endif

#ifdef PROFILE_EXEC
    if (leaveForHost)
    {TRACE_IT(37138);
        entryExitRecord->scriptContext->ProfileEnd(Js::RunPhase);
    }
#endif
}

void ThreadContext::DisposeOnLeaveScript()
{TRACE_IT(37139);
    PHASE_PRINT_TRACE1(Js::DisposePhase, _u("[Dispose] NeedDispose in LeaveScriptStart: %d\n"), this->recycler->NeedDispose());

    if (this->callDispose && this->recycler->NeedDispose())
    {TRACE_IT(37140);
        this->recycler->FinishDisposeObjectsNow<FinishDispose>();
    }
}


template <bool leaveForHost>
void
ThreadContext::LeaveScriptEnd(void * frameAddress)
{TRACE_IT(37141);
    Assert(!this->IsScriptActive());

#if DBG_DUMP
    if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::RunPhase))
    {TRACE_IT(37142);
        Output::Trace(Js::RunPhase, _u("%p> LeaveScriptEnd(%p): Level %d\n"), ::GetCurrentThreadId(), this, this->callRootLevel);
        Output::Flush();
    }
#endif

    Js::ScriptEntryExitRecord * entryExitRecord = this->GetScriptEntryExit();

    AssertMsg(entryExitRecord && entryExitRecord->frameIdOfScriptExitFunction,
              "LeaveScriptEnd without LeaveScriptStart");
    AssertMsg(frameAddress == nullptr || frameAddress == entryExitRecord->frameIdOfScriptExitFunction,
              "Mismatched script exit frames");
    Assert(!!entryExitRecord->leaveForHost == leaveForHost);

    entryExitRecord->frameIdOfScriptExitFunction = nullptr;

    AssertMsg(!this->IsScriptActive(), "Missing LeaveScriptStart or LeaveScriptStart");
    this->isScriptActive = true;
    this->GetRecycler()->SetIsScriptActive(true);

    Js::ImplicitCallFlags savedImplicitCallFlags = entryExitRecord->savedImplicitCallFlags;
    if (leaveForHost)
    {TRACE_IT(37143);
        savedImplicitCallFlags = (Js::ImplicitCallFlags)(savedImplicitCallFlags | Js::ImplicitCall_External);
    }
    else if (entryExitRecord->hasReentered)
    {TRACE_IT(37144);
        savedImplicitCallFlags = (Js::ImplicitCallFlags)(savedImplicitCallFlags | Js::ImplicitCall_AsyncHostOperation);
    }
    // Restore the implicit call flags
    this->SetImplicitCallFlags(savedImplicitCallFlags);

#ifdef PROFILE_EXEC
    if (leaveForHost)
    {TRACE_IT(37145);
        entryExitRecord->scriptContext->ProfileBegin(Js::RunPhase);
    }
#endif
}

// explicit instantiations
template void ThreadContext::LeaveScriptStart<true>(void * frameAddress);
template void ThreadContext::LeaveScriptStart<false>(void * frameAddress);
template void ThreadContext::LeaveScriptEnd<true>(void * frameAddress);
template void ThreadContext::LeaveScriptEnd<false>(void * frameAddress);

void
ThreadContext::PushInterpreterFrame(Js::InterpreterStackFrame *interpreterFrame)
{TRACE_IT(37146);
    interpreterFrame->SetPreviousFrame(this->leafInterpreterFrame);
    this->leafInterpreterFrame = interpreterFrame;
}

Js::InterpreterStackFrame *
ThreadContext::PopInterpreterFrame()
{TRACE_IT(37147);
    Js::InterpreterStackFrame *interpreterFrame = this->leafInterpreterFrame;
    Assert(interpreterFrame);
    this->leafInterpreterFrame = interpreterFrame->GetPreviousFrame();
    return interpreterFrame;
}

BOOL
ThreadContext::ExecuteRecyclerCollectionFunctionCommon(Recycler * recycler, CollectionFunction function, CollectionFlags flags)
{TRACE_IT(37148);
    return  __super::ExecuteRecyclerCollectionFunction(recycler, function, flags);
}

#if DBG
bool
ThreadContext::IsInAsyncHostOperation() const
{TRACE_IT(37149);
    if (!this->IsScriptActive())
    {TRACE_IT(37150);
        Js::ScriptEntryExitRecord * lastRecord = this->entryExitRecord;
        if (lastRecord != NULL)
        {TRACE_IT(37151);
            return !!lastRecord->leaveForAsyncHostOperation;
        }
    }
    return false;
}
#endif

#if ENABLE_NATIVE_CODEGEN
void
ThreadContext::SetJITConnectionInfo(HANDLE processHandle, void* serverSecurityDescriptor, UUID connectionId)
{TRACE_IT(37152);
    Assert(JITManager::GetJITManager()->IsOOPJITEnabled());
    if (!JITManager::GetJITManager()->IsConnected())
    {TRACE_IT(37153);
        // TODO: return HRESULT
        JITManager::GetJITManager()->ConnectRpcServer(processHandle, serverSecurityDescriptor, connectionId);
    }
}
bool
ThreadContext::EnsureJITThreadContext(bool allowPrereserveAlloc)
{TRACE_IT(37154);
#if ENABLE_OOP_NATIVE_CODEGEN
    Assert(JITManager::GetJITManager()->IsOOPJITEnabled());
    if (!JITManager::GetJITManager()->IsConnected())
    {TRACE_IT(37155);
        return false;
    }

    if (m_remoteThreadContextInfo)
    {TRACE_IT(37156);
        return true;
    }

    ThreadContextDataIDL contextData;
    HANDLE serverHandle = JITManager::GetJITManager()->GetServerHandle();

    HANDLE jitTargetHandle = nullptr;
    if (!DuplicateHandle(GetCurrentProcess(), GetCurrentProcess(), serverHandle, &jitTargetHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {TRACE_IT(37157);
        return false;
    }

    contextData.processHandle = (intptr_t)jitTargetHandle;

    contextData.chakraBaseAddress = (intptr_t)AutoSystemInfo::Data.GetChakraBaseAddr();
    ucrtC99MathApis.Ensure();
    contextData.crtBaseAddress = (intptr_t)ucrtC99MathApis.GetHandle();
    contextData.threadStackLimitAddr = reinterpret_cast<intptr_t>(GetAddressOfStackLimitForCurrentThread());
    contextData.bailOutRegisterSaveSpaceAddr = (intptr_t)bailOutRegisterSaveSpace;
    contextData.disableImplicitFlagsAddr = (intptr_t)GetAddressOfDisableImplicitFlags();
    contextData.implicitCallFlagsAddr = (intptr_t)GetAddressOfImplicitCallFlags();
    contextData.scriptStackLimit = GetScriptStackLimit();
    contextData.isThreadBound = IsThreadBound();
    contextData.allowPrereserveAlloc = allowPrereserveAlloc;
#if defined(ENABLE_SIMDJS) && (_M_IX86 || _M_AMD64)
    contextData.simdTempAreaBaseAddr = (intptr_t)GetSimdTempArea();
#endif

    m_jitNumericProperties = HeapNew(BVSparse<HeapAllocator>, &HeapAllocator::Instance);

    for (auto iter = propertyMap->GetIterator(); iter.IsValid(); iter.MoveNext())
    {TRACE_IT(37158);
        if (iter.CurrentKey()->IsNumeric())
        {TRACE_IT(37159);
            m_jitNumericProperties->Set(iter.CurrentKey()->GetPropertyId());
            m_jitNeedsPropertyUpdate = true;
        }
    }

    HRESULT hr = JITManager::GetJITManager()->InitializeThreadContext(&contextData, &m_remoteThreadContextInfo, &m_prereservedRegionAddr);
    JITManager::HandleServerCallResult(hr, RemoteCallType::StateUpdate);

    return m_remoteThreadContextInfo != nullptr;
#endif
}
#endif

#if ENABLE_TTD
void ThreadContext::InitTimeTravel(ThreadContext* threadContext, void* runtimeHandle, uint32 snapInterval, uint32 snapHistoryLength)
{TRACE_IT(37160);
    TTDAssert(!this->IsRuntimeInTTDMode(), "We should only init once.");

    this->TTDContext = HeapNew(TTD::ThreadContextTTD, this, runtimeHandle, snapInterval, snapHistoryLength);
    this->TTDLog = HeapNew(TTD::EventLog, this);
}

void ThreadContext::InitHostFunctionsAndTTData(bool record, bool replay, bool debug, size_t optTTUriLength, const char* optTTUri,
    TTD::TTDOpenResourceStreamCallback openResourceStreamfp, TTD::TTDReadBytesFromStreamCallback readBytesFromStreamfp,
    TTD::TTDWriteBytesToStreamCallback writeBytesToStreamfp, TTD::TTDFlushAndCloseStreamCallback flushAndCloseStreamfp,
    TTD::TTDCreateExternalObjectCallback createExternalObjectfp,
    TTD::TTDCreateJsRTContextCallback createJsRTContextCallbackfp, TTD::TTDReleaseJsRTContextCallback releaseJsRTContextCallbackfp, TTD::TTDSetActiveJsRTContext setActiveJsRTContextfp)
{TRACE_IT(37161);
    AssertMsg(this->IsRuntimeInTTDMode(), "Need to call init first.");

    this->TTDContext->TTDataIOInfo = { openResourceStreamfp, readBytesFromStreamfp, writeBytesToStreamfp, flushAndCloseStreamfp, 0, nullptr };
    this->TTDContext->TTDExternalObjectFunctions = { createExternalObjectfp, createJsRTContextCallbackfp, releaseJsRTContextCallbackfp, setActiveJsRTContextfp };

    if(record)
    {TRACE_IT(37162);
        TTDAssert(optTTUri == nullptr, "No URI is needed in record mode (the host explicitly provides it when writing.");

        this->TTDLog->InitForTTDRecord();
    }
    else
    {TRACE_IT(37163);
        TTDAssert(optTTUri != nullptr, "We need a URI in replay mode so we can initialize the log from it");

        this->TTDLog->InitForTTDReplay(this->TTDContext->TTDataIOInfo, optTTUri, optTTUriLength, debug);
    }
}
#endif

BOOL
ThreadContext::ExecuteRecyclerCollectionFunction(Recycler * recycler, CollectionFunction function, CollectionFlags flags)
{TRACE_IT(37164);
    // If the thread context doesn't have an associated Recycler set, don't do anything
    if (this->recycler == nullptr)
    {TRACE_IT(37165);
        return FALSE;
    }

    // Take etw rundown lock on this thread context. We can't collect entryPoints if we are in etw rundown.
    AutoCriticalSection autocs(this->GetEtwRundownCriticalSection());

    // Disable calling dispose from leave script or the stack probe
    // while we're executing the recycler wrapper
    AutoRestoreValue<bool> callDispose(&this->callDispose, false);

    BOOL ret = FALSE;


#if ENABLE_TTD
    //
    //TODO: We lose any events that happen in the callbacks (such as JsRelease) which may be a problem in the future.
    //      It may be possible to defer the collection of these objects to an explicit collection at the yield loop (same for weak set/map).
    //      We already indirectly do this for ScriptContext collection.
    //
    if(this->IsRuntimeInTTDMode())
    {TRACE_IT(37166);
        this->TTDLog->PushMode(TTD::TTDMode::ExcludedExecutionTTAction);
    }
#endif

    if (!this->IsScriptActive())
    {TRACE_IT(37167);
        Assert(!this->IsDisableImplicitCall() || this->IsInAsyncHostOperation());
        ret = this->ExecuteRecyclerCollectionFunctionCommon(recycler, function, flags);

        // Make sure that we finish a collect that is activated outside of script, since
        // we won't have exit script to schedule it
        if (!this->IsInScript() && recycler->CollectionInProgress()
            && ((flags & CollectOverride_DisableIdleFinish) == 0) && threadServiceWrapper)
        {TRACE_IT(37168);
            threadServiceWrapper->ScheduleFinishConcurrent();
        }
    }
    else
    {TRACE_IT(37169);
        void * frameAddr = nullptr;
        GET_CURRENT_FRAME_ID(frameAddr);

        // We may need stack to call out from Dispose or QC
        if (!this->IsDisableImplicitCall()) // otherwise Dispose/QC disabled
        {TRACE_IT(37170);
            // If we don't have stack space to call out from Dispose or QC,
            // don't throw, simply return false. This gives SnailAlloc a better
            // chance of allocating in low stack-space situations (like allocating
            // a StackOverflowException object)
            if (!this->IsStackAvailableNoThrow(Js::Constants::MinStackCallout))
            {TRACE_IT(37171);
                return false;
            }
        }

        this->LeaveScriptStart<false>(frameAddr);
        ret = this->ExecuteRecyclerCollectionFunctionCommon(recycler, function, flags);
        this->LeaveScriptEnd<false>(frameAddr);

        if (this->callRootLevel != 0)
        {TRACE_IT(37172);
            this->CheckScriptInterrupt();
        }
    }

#if ENABLE_TTD
    if(this->IsRuntimeInTTDMode())
    {TRACE_IT(37173);
        this->TTDLog->PopMode(TTD::TTDMode::ExcludedExecutionTTAction);
    }
#endif

    return ret;
}

void
ThreadContext::DisposeObjects(Recycler * recycler)
{TRACE_IT(37174);
    if (this->IsDisableImplicitCall())
    {TRACE_IT(37175);
        // Don't dispose objects when implicit calls are disabled, since disposing may cause implicit calls. Objects will remain
        // in the dispose queue and will be disposed later when implicit calls are not disabled.
        return;
    }

    // We shouldn't DisposeObjects in NoScriptScope as this might lead to script execution.
    // Callers of DisposeObjects should ensure !IsNoScriptScope() before calling DisposeObjects.
    if (this->IsNoScriptScope())
    {TRACE_IT(37176);
        FromDOM_NoScriptScope_fatal_error();
    }

    if (!this->IsScriptActive())
    {TRACE_IT(37177);
        __super::DisposeObjects(recycler);
    }
    else
    {TRACE_IT(37178);
        void * frameAddr = nullptr;
        GET_CURRENT_FRAME_ID(frameAddr);

        // We may need stack to call out from Dispose
        this->ProbeStack(Js::Constants::MinStackCallout);

        this->LeaveScriptStart<false>(frameAddr);
        __super::DisposeObjects(recycler);
        this->LeaveScriptEnd<false>(frameAddr);
    }
}

void
ThreadContext::PushEntryExitRecord(Js::ScriptEntryExitRecord * record)
{
    AssertMsg(record, "Didn't provide a script entry record to push");
    Assert(record->next == nullptr);


    Js::ScriptEntryExitRecord * lastRecord = this->entryExitRecord;
    if (lastRecord != nullptr)
    {TRACE_IT(37179);
        // If we enter script again, we should have leave with leaveForHost or leave for dispose.
        Assert(lastRecord->leaveForHost || lastRecord->leaveForAsyncHostOperation);
        lastRecord->hasReentered = true;
        record->next = lastRecord;

        // these are on stack, which grows down. if this condition doesn't hold, then the list somehow got messed up
        if (!IsOnStack(lastRecord) || (uintptr_t)record >= (uintptr_t)lastRecord)
        {TRACE_IT(37180);
            EntryExitRecord_Corrupted_fatal_error();
        }
    }

    this->entryExitRecord = record;
}

void ThreadContext::PopEntryExitRecord(Js::ScriptEntryExitRecord * record)
{TRACE_IT(37181);
    AssertMsg(record && record == this->entryExitRecord, "Mismatch script entry/exit");

    // these are on stack, which grows down. if this condition doesn't hold, then the list somehow got messed up
    Js::ScriptEntryExitRecord * next = this->entryExitRecord->next;
    if (next && (!IsOnStack(next) || (uintptr_t)this->entryExitRecord >= (uintptr_t)next))
    {TRACE_IT(37182);
        EntryExitRecord_Corrupted_fatal_error();
    }

    this->entryExitRecord = next;
}

BOOL ThreadContext::ReserveStaticTypeIds(__in int first, __in int last)
{TRACE_IT(37183);
    if ( nextTypeId <= first )
    {TRACE_IT(37184);
        nextTypeId = (Js::TypeId) last;
        return TRUE;
    }
    else
    {TRACE_IT(37185);
        return FALSE;
    }
}

Js::TypeId ThreadContext::ReserveTypeIds(int count)
{TRACE_IT(37186);
    Js::TypeId firstTypeId = nextTypeId;
    nextTypeId = (Js::TypeId)(nextTypeId + count);
    return firstTypeId;
}

Js::TypeId ThreadContext::CreateTypeId()
{TRACE_IT(37187);
    return nextTypeId = (Js::TypeId)(nextTypeId + 1);
}

WellKnownHostType ThreadContext::GetWellKnownHostType(Js::TypeId typeId)
{TRACE_IT(37188);
    if (this->wellKnownHostTypeHTMLAllCollectionTypeId == typeId)
    {TRACE_IT(37189);
        return WellKnownHostType_HTMLAllCollection;
    }

    return WellKnownHostType_Invalid;
}

void ThreadContext::SetWellKnownHostTypeId(WellKnownHostType wellKnownType, Js::TypeId typeId)
{TRACE_IT(37190);
    AssertMsg(WellKnownHostType_HTMLAllCollection == wellKnownType, "ThreadContext::SetWellKnownHostTypeId called on type other than HTMLAllCollection");

    if (WellKnownHostType_HTMLAllCollection == wellKnownType)
    {TRACE_IT(37191);
        this->wellKnownHostTypeHTMLAllCollectionTypeId = typeId;
#if ENABLE_NATIVE_CODEGEN
        if (this->m_remoteThreadContextInfo)
        {TRACE_IT(37192);
            HRESULT hr = JITManager::GetJITManager()->SetWellKnownHostTypeId(this->m_remoteThreadContextInfo, (int)typeId);
            JITManager::HandleServerCallResult(hr, RemoteCallType::StateUpdate);
        }
#endif
    }
}

void ThreadContext::EnsureDebugManager()
{TRACE_IT(37193);
    if (this->debugManager == nullptr)
    {TRACE_IT(37194);
        this->debugManager = HeapNew(Js::DebugManager, this, this->GetAllocationPolicyManager());
    }
    InterlockedIncrement(&crefSContextForDiag);
    Assert(this->debugManager != nullptr);
}

void ThreadContext::ReleaseDebugManager()
{TRACE_IT(37195);
    Assert(crefSContextForDiag > 0);
    Assert(this->debugManager != nullptr);

    LONG lref = InterlockedDecrement(&crefSContextForDiag);

    if (lref == 0)
    {TRACE_IT(37196);
        if (this->recyclableData != nullptr)
        {TRACE_IT(37197);
            this->recyclableData->returnedValueList = nullptr;
        }

        if (this->debugManager != nullptr)
        {TRACE_IT(37198);
            this->debugManager->Close();
            HeapDelete(this->debugManager);
            this->debugManager = nullptr;
        }
    }
}



Js::TempArenaAllocatorObject *
ThreadContext::GetTemporaryAllocator(LPCWSTR name)
{TRACE_IT(37199);
    AssertCanHandleOutOfMemory();

    if (temporaryArenaAllocatorCount != 0)
    {TRACE_IT(37200);
        temporaryArenaAllocatorCount--;
        Js::TempArenaAllocatorObject * allocator = recyclableData->temporaryArenaAllocators[temporaryArenaAllocatorCount];
        recyclableData->temporaryArenaAllocators[temporaryArenaAllocatorCount] = nullptr;
        return allocator;
    }

    return Js::TempArenaAllocatorObject::Create(this);
}

void
ThreadContext::ReleaseTemporaryAllocator(Js::TempArenaAllocatorObject * tempAllocator)
{TRACE_IT(37201);
    if (temporaryArenaAllocatorCount < MaxTemporaryArenaAllocators)
    {TRACE_IT(37202);
        tempAllocator->GetAllocator()->Reset();
        recyclableData->temporaryArenaAllocators[temporaryArenaAllocatorCount] = tempAllocator;
        temporaryArenaAllocatorCount++;
        return;
    }

    tempAllocator->Dispose(false);
}


Js::TempGuestArenaAllocatorObject *
ThreadContext::GetTemporaryGuestAllocator(LPCWSTR name)
{TRACE_IT(37203);
    AssertCanHandleOutOfMemory();

    if (temporaryGuestArenaAllocatorCount != 0)
    {TRACE_IT(37204);
        temporaryGuestArenaAllocatorCount--;
        Js::TempGuestArenaAllocatorObject * allocator = recyclableData->temporaryGuestArenaAllocators[temporaryGuestArenaAllocatorCount];
        allocator->AdviseInUse();
        recyclableData->temporaryGuestArenaAllocators[temporaryGuestArenaAllocatorCount] = nullptr;
        return allocator;
    }

    return Js::TempGuestArenaAllocatorObject::Create(this);
}

void
ThreadContext::ReleaseTemporaryGuestAllocator(Js::TempGuestArenaAllocatorObject * tempGuestAllocator)
{TRACE_IT(37205);
    if (temporaryGuestArenaAllocatorCount < MaxTemporaryArenaAllocators)
    {TRACE_IT(37206);
        tempGuestAllocator->AdviseNotInUse();
        recyclableData->temporaryGuestArenaAllocators[temporaryGuestArenaAllocatorCount] = tempGuestAllocator;
        temporaryGuestArenaAllocatorCount++;
        return;
    }

    tempGuestAllocator->Dispose(false);
}

void
ThreadContext::AddToPendingScriptContextCloseList(Js::ScriptContext * scriptContext)
{TRACE_IT(37207);
    Assert(scriptContext != nullptr);

    if (rootPendingClose == nullptr)
    {TRACE_IT(37208);
        rootPendingClose = scriptContext;
        return;
    }

    // Prepend to the list.
    scriptContext->SetNextPendingClose(rootPendingClose);
    rootPendingClose = scriptContext;
}

void
ThreadContext::RemoveFromPendingClose(Js::ScriptContext * scriptContext)
{TRACE_IT(37209);
    Assert(scriptContext != nullptr);

    if (rootPendingClose == nullptr)
    {TRACE_IT(37210);
        // We already sent a close message, ignore the notification.
        return;
    }

    // Base case: The root is being removed. Move the root along.
    if (scriptContext == rootPendingClose)
    {TRACE_IT(37211);
        rootPendingClose = rootPendingClose->GetNextPendingClose();
        return;
    }

    Js::ScriptContext * currScriptContext = rootPendingClose;
    Js::ScriptContext * nextScriptContext = nullptr;
    while (currScriptContext)
    {TRACE_IT(37212);
        nextScriptContext = currScriptContext->GetNextPendingClose();
        if (!nextScriptContext)
        {TRACE_IT(37213);
            break;
        }

        if (nextScriptContext == scriptContext) {TRACE_IT(37214);
            // The next pending close ScriptContext is the one to be removed - set prev->next to next->next
            currScriptContext->SetNextPendingClose(nextScriptContext->GetNextPendingClose());
            return;
        }
        currScriptContext = nextScriptContext;
    }

    // We expect to find scriptContext in the pending close list.
    Assert(false);
}

void ThreadContext::ClosePendingScriptContexts()
{TRACE_IT(37215);
    Js::ScriptContext * scriptContext = rootPendingClose;
    if (scriptContext == nullptr)
    {TRACE_IT(37216);
        return;
    }
    Js::ScriptContext * nextScriptContext;
    do
    {TRACE_IT(37217);
        nextScriptContext = scriptContext->GetNextPendingClose();
        scriptContext->Close(false);
        scriptContext = nextScriptContext;
    }
    while (scriptContext);
    rootPendingClose = nullptr;
}

void
ThreadContext::AddToPendingProjectionContextCloseList(IProjectionContext *projectionContext)
{TRACE_IT(37218);
    pendingProjectionContextCloseList->Add(projectionContext);
}

void
ThreadContext::RemoveFromPendingClose(IProjectionContext* projectionContext)
{TRACE_IT(37219);
    pendingProjectionContextCloseList->Remove(projectionContext);
}

void ThreadContext::ClosePendingProjectionContexts()
{TRACE_IT(37220);
    IProjectionContext* projectionContext;
    for (int i = 0 ; i < pendingProjectionContextCloseList->Count(); i++)
    {TRACE_IT(37221);
        projectionContext = pendingProjectionContextCloseList->Item(i);
        projectionContext->Close();
    }
    pendingProjectionContextCloseList->Clear();

}

void
ThreadContext::RegisterScriptContext(Js::ScriptContext *scriptContext)
{TRACE_IT(37222);
    // NOTE: ETW rundown thread may be reading the scriptContextList concurrently. We don't need to
    // lock access because we only insert to the front here.

    scriptContext->next = this->scriptContextList;
    if (this->scriptContextList)
    {TRACE_IT(37223);
        Assert(this->scriptContextList->prev == NULL);
        this->scriptContextList->prev = scriptContext;
    }
    scriptContext->prev = NULL;
    this->scriptContextList = scriptContext;

    if(NoJIT())
    {TRACE_IT(37224);
        scriptContext->ForceNoNative();
    }
#if DBG || defined(RUNTIME_DATA_COLLECTION)
    scriptContextCount++;
#endif
    scriptContextEverRegistered = true;
}

void
ThreadContext::UnregisterScriptContext(Js::ScriptContext *scriptContext)
{TRACE_IT(37225);
    // NOTE: ETW rundown thread may be reading the scriptContextList concurrently. Since this function
    // is only called by ~ScriptContext() which already synchronized to ETW rundown, we are safe here.

    if (scriptContext == this->scriptContextList)
    {TRACE_IT(37226);
        Assert(scriptContext->prev == NULL);
        this->scriptContextList = scriptContext->next;
    }
    else
    {TRACE_IT(37227);
        scriptContext->prev->next = scriptContext->next;
    }

    if (scriptContext->next)
    {TRACE_IT(37228);
        scriptContext->next->prev = scriptContext->prev;
    }

    scriptContext->prev = nullptr;
    scriptContext->next = nullptr;

#if DBG || defined(RUNTIME_DATA_COLLECTION)
    scriptContextCount--;
#endif
}

ThreadContext::CollectCallBack *
ThreadContext::AddRecyclerCollectCallBack(RecyclerCollectCallBackFunction callback, void * context)
{TRACE_IT(37229);
    AutoCriticalSection autocs(&csCollectionCallBack);
    CollectCallBack * collectCallBack = this->collectCallBackList.PrependNode(&HeapAllocator::Instance);
    collectCallBack->callback = callback;
    collectCallBack->context = context;
    this->hasCollectionCallBack = true;
    return collectCallBack;
}

void
ThreadContext::RemoveRecyclerCollectCallBack(ThreadContext::CollectCallBack * collectCallBack)
{TRACE_IT(37230);
    AutoCriticalSection autocs(&csCollectionCallBack);
    this->collectCallBackList.RemoveElement(&HeapAllocator::Instance, collectCallBack);
    this->hasCollectionCallBack = !this->collectCallBackList.Empty();
}

void
ThreadContext::PreCollectionCallBack(CollectionFlags flags)
{TRACE_IT(37231);
#ifdef PERF_COUNTERS
    PHASE_PRINT_TESTTRACE1(Js::DeferParsePhase, _u("TestTrace: deferparse - # of func: %d # deferparsed: %d\n"), PerfCounter::CodeCounterSet::GetTotalFunctionCounter().GetValue(), PerfCounter::CodeCounterSet::GetDeferredFunctionCounter().GetValue());
#endif

    // This needs to be done before ClearInlineCaches since that method can empty the list of
    // script contexts with inline caches
    this->ClearScriptContextCaches();

    // Clear up references to types to avoid keep them alive
    this->ClearPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesCaches();

    // Clean up unused memory before we start collecting
    this->CleanNoCasePropertyMap();
    this->TryEnterExpirableCollectMode();

    const BOOL concurrent = flags & CollectMode_Concurrent;
    const BOOL partial = flags & CollectMode_Partial;

    if (!partial)
    {TRACE_IT(37232);
        // Integrate allocated pages from background JIT threads
#if ENABLE_NATIVE_CODEGEN
#if !FLOATVAR
        if (codeGenNumberThreadAllocator)
        {TRACE_IT(37233);
            codeGenNumberThreadAllocator->Integrate();
        }
        if (this->xProcNumberPageSegmentManager)
        {TRACE_IT(37234);
            this->xProcNumberPageSegmentManager->Integrate();
        }
#endif
#endif
    }

    RecyclerCollectCallBackFlags callBackFlags = (RecyclerCollectCallBackFlags)
        ((concurrent ? Collect_Begin_Concurrent : Collect_Begin) | (partial? Collect_Begin_Partial : Collect_Begin));
    CollectionCallBack(callBackFlags);
}

void
ThreadContext::PreSweepCallback()
{TRACE_IT(37235);
#ifdef PERSISTENT_INLINE_CACHES
    ClearInlineCachesWithDeadWeakRefs();
#else
    ClearInlineCaches();
#endif

    ClearIsInstInlineCaches();

    ClearEquivalentTypeCaches();

    ClearForInCaches();

    this->dynamicObjectEnumeratorCacheMap.Clear();
}

void
ThreadContext::CollectionCallBack(RecyclerCollectCallBackFlags flags)
{TRACE_IT(37236);
    DListBase<CollectCallBack>::Iterator i(&this->collectCallBackList);
    while (i.Next())
    {TRACE_IT(37237);
        i.Data().callback(i.Data().context, flags);
    }
}

void
ThreadContext::WaitCollectionCallBack()
{TRACE_IT(37238);
    // Avoid taking the lock if there are no call back
    if (hasCollectionCallBack)
    {TRACE_IT(37239);
        AutoCriticalSection autocs(&csCollectionCallBack);
        CollectionCallBack(Collect_Wait);
    }
}

void
ThreadContext::PostCollectionCallBack()
{TRACE_IT(37240);
    CollectionCallBack(Collect_End);

    TryExitExpirableCollectMode();

    // Recycler is null in the case where the ThreadContext is in the process of creating the recycler and
    // we have a GC triggered (say because the -recyclerStress flag is passed in)
    if (this->recycler != NULL)
    {TRACE_IT(37241);
        if (this->recycler->InCacheCleanupCollection())
        {TRACE_IT(37242);
            this->recycler->ClearCacheCleanupCollection();
            for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext; scriptContext = scriptContext->next)
            {TRACE_IT(37243);
                scriptContext->CleanupWeakReferenceDictionaries();
            }
        }
    }
}

void
ThreadContext::PostSweepRedeferralCallBack()
{TRACE_IT(37244);
    if (this->DoTryRedeferral())
    {TRACE_IT(37245);
        HRESULT hr = S_OK;
        BEGIN_TRANSLATE_OOM_TO_HRESULT
        {TRACE_IT(37246);
            this->TryRedeferral();
        }
        END_TRANSLATE_OOM_TO_HRESULT(hr);
    }

    this->UpdateRedeferralState();
}

bool
ThreadContext::DoTryRedeferral() const
{TRACE_IT(37247);
    if (PHASE_FORCE1(Js::RedeferralPhase) || PHASE_STRESS1(Js::RedeferralPhase))
    {TRACE_IT(37248);
        return true;
    }

    if (PHASE_OFF1(Js::RedeferralPhase))
    {TRACE_IT(37249);
        return false;
    }

    switch (this->redeferralState)
    {
        case InitialRedeferralState:
            return false;

        case StartupRedeferralState:
            return gcSinceCallCountsCollected >= StartupRedeferralInactiveThreshold;

        case MainRedeferralState:
            return gcSinceCallCountsCollected >= MainRedeferralInactiveThreshold;

        default:
            Assert(0);
            return false;
    };
}

bool
ThreadContext::DoRedeferFunctionBodies() const
{TRACE_IT(37250);
#if ENABLE_TTD
    if (this->IsRuntimeInTTDMode())
    {TRACE_IT(37251);
        return false;
    }
#endif

    if (PHASE_FORCE1(Js::RedeferralPhase) || PHASE_STRESS1(Js::RedeferralPhase))
    {TRACE_IT(37252);
        return true;
    }

    if (PHASE_OFF1(Js::RedeferralPhase))
    {TRACE_IT(37253);
        return false;
    }

    switch (this->redeferralState)
    {
        case InitialRedeferralState:
            return false;

        case StartupRedeferralState:
            return gcSinceLastRedeferral >= StartupRedeferralCheckInterval;

        case MainRedeferralState:
            return gcSinceLastRedeferral >= MainRedeferralCheckInterval;

        default:
            Assert(0);
            return false;
    };
}

uint
ThreadContext::GetRedeferralCollectionInterval() const
{TRACE_IT(37254);
    switch(this->redeferralState)
    {
        case InitialRedeferralState:
            return InitialRedeferralDelay;

        case StartupRedeferralState:
            return StartupRedeferralCheckInterval;

        case MainRedeferralState:
            return MainRedeferralCheckInterval;

        default:
            Assert(0);
            return (uint)-1;
    }
}

uint
ThreadContext::GetRedeferralInactiveThreshold() const
{TRACE_IT(37255);
    switch(this->redeferralState)
    {
        case InitialRedeferralState:
            return InitialRedeferralDelay;

        case StartupRedeferralState:
            return StartupRedeferralInactiveThreshold;

        case MainRedeferralState:
            return MainRedeferralInactiveThreshold;

        default:
            Assert(0);
            return (uint)-1;
    }
}

void
ThreadContext::TryRedeferral()
{TRACE_IT(37256);
    bool doRedefer = this->DoRedeferFunctionBodies();

    // Collect the set of active functions.
    ActiveFunctionSet *pActiveFuncs = nullptr;
    if (doRedefer)
    {TRACE_IT(37257);
        pActiveFuncs = Anew(this->GetThreadAlloc(), ActiveFunctionSet, this->GetThreadAlloc());
        this->GetActiveFunctions(pActiveFuncs);
#if DBG
        this->redeferredFunctions = 0;
        this->recoveredBytes = 0;
#endif
    }

    uint inactiveThreshold = this->GetRedeferralInactiveThreshold();
    Js::ScriptContext *scriptContext;
    for (scriptContext = GetScriptContextList(); scriptContext; scriptContext = scriptContext->next)
    {TRACE_IT(37258);
        if (scriptContext->IsClosed())
        {TRACE_IT(37259);
            continue;
        }
        scriptContext->RedeferFunctionBodies(pActiveFuncs, inactiveThreshold);
    }

    if (pActiveFuncs)
    {TRACE_IT(37260);
        Adelete(this->GetThreadAlloc(), pActiveFuncs);
#if DBG
        if (PHASE_STATS1(Js::RedeferralPhase) && this->redeferredFunctions)
        {TRACE_IT(37261);
            Output::Print(_u("Redeferred: %d, Bytes: 0x%x\n"), this->redeferredFunctions, this->recoveredBytes);
        }
#endif
    }
}

void
ThreadContext::GetActiveFunctions(ActiveFunctionSet * pActiveFuncs)
{TRACE_IT(37262);
    if (!this->IsInScript() || this->entryExitRecord == nullptr)
    {TRACE_IT(37263);
        return;
    }

    Js::JavascriptStackWalker walker(GetScriptContextList(), TRUE, NULL, true);
    Js::JavascriptFunction *function = nullptr;
    while (walker.GetCallerWithoutInlinedFrames(&function))
    {TRACE_IT(37264);
        if (function->GetFunctionInfo()->HasBody())
        {TRACE_IT(37265);
            Js::FunctionBody *body = function->GetFunctionInfo()->GetFunctionBody();
            body->UpdateActiveFunctionSet(pActiveFuncs, nullptr);
        }
    }
}

void
ThreadContext::UpdateRedeferralState()
{TRACE_IT(37266);
    uint inactiveThreshold = this->GetRedeferralInactiveThreshold();
    uint collectInterval = this->GetRedeferralCollectionInterval();

    if (this->gcSinceCallCountsCollected >= inactiveThreshold)
    {TRACE_IT(37267);
        this->gcSinceCallCountsCollected = 0;
        if (this->gcSinceLastRedeferral >= collectInterval)
        {TRACE_IT(37268);
            // Advance state
            switch (this->redeferralState)
            {
                case InitialRedeferralState:
                    this->redeferralState = StartupRedeferralState;
                    break;

                case StartupRedeferralState:
                    this->redeferralState = MainRedeferralState;
                    break;

                case MainRedeferralState:
                    break;

                default:
                    Assert(0);
                    break;
            }

            this->gcSinceLastRedeferral = 0;
        }
    }
    else
    {TRACE_IT(37269);
        this->gcSinceCallCountsCollected++;
        this->gcSinceLastRedeferral++;
    }
}

#ifdef FAULT_INJECTION
void
ThreadContext::DisposeScriptContextByFaultInjectionCallBack()
{TRACE_IT(37270);
    if (FAULTINJECT_SCRIPT_TERMINATION_ON_DISPOSE) {TRACE_IT(37271);
        int scriptContextToClose = -1;

        /* inject only if we have more than 1 script context*/
        uint totalScriptCount = GetScriptContextCount();
        if (totalScriptCount > 1) {TRACE_IT(37272);
            if (Js::Configuration::Global.flags.FaultInjectionScriptContextToTerminateCount > 0)
            {TRACE_IT(37273);
                scriptContextToClose = Js::Configuration::Global.flags.FaultInjectionScriptContextToTerminateCount % totalScriptCount;
                for (Js::ScriptContext *scriptContext = GetScriptContextList(); scriptContext; scriptContext = scriptContext->next)
                {TRACE_IT(37274);
                    if (scriptContextToClose-- == 0)
                    {TRACE_IT(37275);
                        scriptContext->DisposeScriptContextByFaultInjection();
                        break;
                    }
                }
            }
            else
            {
                fwprintf(stderr, _u("***FI: FaultInjectionScriptContextToTerminateCount Failed, Value should be > 0. \n"));
            }
        }
    }
}
#endif

#pragma region "Expirable Object Methods"
void
ThreadContext::TryExitExpirableCollectMode()
{TRACE_IT(37276);
    // If this feature is turned off or if we're already in profile collection mode, do nothing
    // We also do nothing if expiration is explicitly disabled by someone lower down the stack
    if (PHASE_OFF1(Js::ExpirableCollectPhase) || !InExpirableCollectMode() || this->disableExpiration)
    {TRACE_IT(37277);
        return;
    }

    if (InExpirableCollectMode())
    {TRACE_IT(37278);
        OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Checking to see whether to complete Expirable Object Collection: GC Count is %d\n"), this->expirableCollectModeGcCount);
        if (this->expirableCollectModeGcCount > 0)
        {TRACE_IT(37279);
            this->expirableCollectModeGcCount--;
        }

        if (this->expirableCollectModeGcCount == 0 &&
            (this->recycler->InCacheCleanupCollection() || CONFIG_FLAG(ForceExpireOnNonCacheCollect)))
        {TRACE_IT(37280);
            OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Completing Expirable Object Collection\n"));

            ExpirableObjectList::Iterator expirableObjectIterator(this->expirableObjectList);

            while (expirableObjectIterator.Next())
            {TRACE_IT(37281);
                ExpirableObject* object = expirableObjectIterator.Data();

                Assert(object);

                if (!object->IsObjectUsed())
                {TRACE_IT(37282);
                    object->Expire();
                }
            }

            // Leave expirable collection mode
            expirableCollectModeGcCount = -1;
        }
    }
}

bool
ThreadContext::InExpirableCollectMode()
{TRACE_IT(37283);
    // We're in expirable collect if we have expirable objects registered,
    // and expirableCollectModeGcCount is not negative
    // and when debugger is attaching, it might have set the function to deferredParse.
    return (expirableObjectList != nullptr &&
            numExpirableObjects > 0 &&
            expirableCollectModeGcCount >= 0 &&
            (this->GetDebugManager() != nullptr &&
            !this->GetDebugManager()->IsDebuggerAttaching()));
}

void
ThreadContext::TryEnterExpirableCollectMode()
{TRACE_IT(37284);
    // If this feature is turned off or if we're already in profile collection mode, do nothing
    if (PHASE_OFF1(Js::ExpirableCollectPhase) || InExpirableCollectMode())
    {TRACE_IT(37285);
        OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Not running Expirable Object Collection\n"));
        return;
    }

    double entryPointCollectionThreshold = Js::Configuration::Global.flags.ExpirableCollectionTriggerThreshold / 100.0;

    double currentThreadNativeCodeRatio = ((double) GetCodeSize()) / Js::Constants::MaxThreadJITCodeHeapSize;

    OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Current native code ratio: %f\n"), currentThreadNativeCodeRatio);
    if (currentThreadNativeCodeRatio > entryPointCollectionThreshold)
    {TRACE_IT(37286);
        OUTPUT_TRACE(Js::ExpirableCollectPhase, _u("Setting up Expirable Object Collection\n"));

        this->expirableCollectModeGcCount = Js::Configuration::Global.flags.ExpirableCollectionGCCount;

        ExpirableObjectList::Iterator expirableObjectIterator(this->expirableObjectList);

        while (expirableObjectIterator.Next())
        {TRACE_IT(37287);
            ExpirableObject* object = expirableObjectIterator.Data();

            Assert(object);
            object->EnterExpirableCollectMode();
        }

        if (this->entryExitRecord != nullptr)
        {TRACE_IT(37288);
            // If we're in script, we will do a stack walk, find the JavascriptFunction's on the stack
            // and mark their entry points as being used so that we don't prematurely expire them

            Js::ScriptContext* topScriptContext = this->entryExitRecord->scriptContext;
            Js::JavascriptStackWalker walker(topScriptContext, TRUE);

            Js::JavascriptFunction* javascriptFunction = nullptr;
            while (walker.GetCallerWithoutInlinedFrames(&javascriptFunction))
            {TRACE_IT(37289);
                if (javascriptFunction != nullptr && Js::ScriptFunction::Test(javascriptFunction))
                {TRACE_IT(37290);
                    Js::ScriptFunction* scriptFunction = (Js::ScriptFunction*) javascriptFunction;
                    Js::FunctionEntryPointInfo* entryPointInfo =  scriptFunction->GetFunctionEntryPointInfo();
                    entryPointInfo->SetIsObjectUsed();
                    scriptFunction->GetFunctionBody()->MapEntryPoints([](int index, Js::FunctionEntryPointInfo* entryPoint){
                        entryPoint->SetIsObjectUsed();
                    });
                }
            }

        }
    }
}

void
ThreadContext::RegisterExpirableObject(ExpirableObject* object)
{TRACE_IT(37291);
    Assert(this->expirableObjectList);
    Assert(object->registrationHandle == nullptr);

    ExpirableObject** registrationData = this->expirableObjectList->PrependNode();
    (*registrationData) = object;
    object->registrationHandle = (void*) registrationData;
    OUTPUT_VERBOSE_TRACE(Js::ExpirableCollectPhase, _u("Registered 0x%p\n"), object);

    numExpirableObjects++;
}

void
ThreadContext::UnregisterExpirableObject(ExpirableObject* object)
{TRACE_IT(37292);
    Assert(this->expirableObjectList);
    Assert(object->registrationHandle != nullptr);
    Assert(this->expirableObjectList->HasElement(
        (ExpirableObject* const *)PointerValue(object->registrationHandle)));

    ExpirableObject** registrationData = (ExpirableObject**)PointerValue(object->registrationHandle);
    Assert(*registrationData == object);

    this->expirableObjectList->MoveElementTo(registrationData, this->expirableObjectDisposeList);
    object->registrationHandle = nullptr;
    OUTPUT_VERBOSE_TRACE(Js::ExpirableCollectPhase, _u("Unregistered 0x%p\n"), object);
    numExpirableObjects--;
}

void
ThreadContext::DisposeExpirableObject(ExpirableObject* object)
{TRACE_IT(37293);
    Assert(this->expirableObjectDisposeList);
    Assert(object->registrationHandle == nullptr);

    this->expirableObjectDisposeList->Remove(object);

    OUTPUT_VERBOSE_TRACE(Js::ExpirableCollectPhase, _u("Disposed 0x%p\n"), object);
}
#pragma endregion

void
ThreadContext::ClearScriptContextCaches()
{TRACE_IT(37294);
    for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext != nullptr; scriptContext = scriptContext->next)
    {TRACE_IT(37295);
        scriptContext->ClearScriptContextCaches();
    }
}

#ifdef PERSISTENT_INLINE_CACHES
void
ThreadContext::ClearInlineCachesWithDeadWeakRefs()
{TRACE_IT(37296);
    for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext != nullptr; scriptContext = scriptContext->next)
    {TRACE_IT(37297);
        scriptContext->ClearInlineCachesWithDeadWeakRefs();
    }

    if (PHASE_TRACE1(Js::InlineCachePhase))
    {TRACE_IT(37298);
        size_t size = 0;
        size_t freeListSize = 0;
        size_t polyInlineCacheSize = 0;
        uint scriptContextCount = 0;
        for (Js::ScriptContext *scriptContext = scriptContextList;
            scriptContext;
            scriptContext = scriptContext->next)
        {TRACE_IT(37299);
            scriptContextCount++;
            size += scriptContext->GetInlineCacheAllocator()->AllocatedSize();
            freeListSize += scriptContext->GetInlineCacheAllocator()->FreeListSize();
#ifdef POLY_INLINE_CACHE_SIZE_STATS
            polyInlineCacheSize += scriptContext->GetInlineCacheAllocator()->GetPolyInlineCacheSize();
#endif
        };
        printf("Inline cache arena: total = %5I64u KB, free list = %5I64u KB, poly caches = %5I64u KB, script contexts = %u\n",
            static_cast<uint64>(size / 1024), static_cast<uint64>(freeListSize / 1024), static_cast<uint64>(polyInlineCacheSize / 1024), scriptContextCount);
    }
}

void
ThreadContext::ClearInvalidatedUniqueGuards()
{TRACE_IT(37300);
    // If a propertyGuard was invalidated, make sure to remove it's entry from unique property guard table of other property records.
    PropertyGuardDictionary &guards = this->recyclableData->propertyGuards;

    guards.Map([this](Js::PropertyRecord const * propertyRecord, PropertyGuardEntry* entry, const RecyclerWeakReference<const Js::PropertyRecord>* weakRef)
    {
        entry->uniqueGuards.MapAndRemoveIf([=](RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef)
        {
            Js::PropertyGuard* guard = guardWeakRef->Get();
            bool shouldRemove = guard != nullptr && !guard->IsValid();
            if (shouldRemove)
            {TRACE_IT(37301);
                if (PHASE_TRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase))
                {TRACE_IT(37302);
                    Output::Print(_u("FixedFields: invalidating guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p\n"),
                                  propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue());
                    Output::Flush();
                }
                if (PHASE_TESTTRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))
                {TRACE_IT(37303);
                    Output::Print(_u("FixedFields: invalidating guard: name: %s, value: 0x%p\n"),
                                  propertyRecord->GetBuffer(), guard->GetValue());
                    Output::Flush();
                }
            }

            return shouldRemove;
        });
    });
}

void
ThreadContext::ClearInlineCaches()
{TRACE_IT(37304);
    if (PHASE_TRACE1(Js::InlineCachePhase))
    {TRACE_IT(37305);
        size_t size = 0;
        size_t freeListSize = 0;
        size_t polyInlineCacheSize = 0;
        uint scriptContextCount = 0;
        for (Js::ScriptContext *scriptContext = scriptContextList;
        scriptContext;
            scriptContext = scriptContext->next)
        {TRACE_IT(37306);
            scriptContextCount++;
            size += scriptContext->GetInlineCacheAllocator()->AllocatedSize();
            freeListSize += scriptContext->GetInlineCacheAllocator()->FreeListSize();
#ifdef POLY_INLINE_CACHE_SIZE_STATS
            polyInlineCacheSize += scriptContext->GetInlineCacheAllocator()->GetPolyInlineCacheSize();
#endif
        };
        printf("Inline cache arena: total = %5I64u KB, free list = %5I64u KB, poly caches = %5I64u KB, script contexts = %u\n",
            static_cast<uint64>(size / 1024), static_cast<uint64>(freeListSize / 1024), static_cast<uint64>(polyInlineCacheSize / 1024), scriptContextCount);
    }

    Js::ScriptContext *scriptContext = this->scriptContextList;
    while (scriptContext != nullptr)
    {TRACE_IT(37307);
        scriptContext->ClearInlineCaches();
        scriptContext = scriptContext->next;
    }

    inlineCacheThreadInfoAllocator.Reset();
    protoInlineCacheByPropId.ResetNoDelete();
    storeFieldInlineCacheByPropId.ResetNoDelete();

    registeredInlineCacheCount = 0;
    unregisteredInlineCacheCount = 0;
}
#endif //PERSISTENT_INLINE_CACHES

void
ThreadContext::ClearIsInstInlineCaches()
{TRACE_IT(37308);
    Js::ScriptContext *scriptContext = this->scriptContextList;
    while (scriptContext != nullptr)
    {TRACE_IT(37309);
        scriptContext->ClearIsInstInlineCaches();
        scriptContext = scriptContext->next;
    }

    isInstInlineCacheThreadInfoAllocator.Reset();
    isInstInlineCacheByFunction.ResetNoDelete();
}

void
ThreadContext::ClearForInCaches()
{TRACE_IT(37310);
    Js::ScriptContext *scriptContext = this->scriptContextList;
    while (scriptContext != nullptr)
    {TRACE_IT(37311);
        scriptContext->ClearForInCaches();
        scriptContext = scriptContext->next;
    }
}

void
ThreadContext::ClearEquivalentTypeCaches()
{TRACE_IT(37312);
#if ENABLE_NATIVE_CODEGEN
    // Called from PreSweepCallback to clear pointers to types that have no live object references left.
    // The EquivalentTypeCache used to keep these types alive, but this caused memory growth in cases where
    // entry points stayed around for a long time.
    // In future we may want to pin the reference/guard type to the entry point, but that choice will depend
    // on a use case where pinning the type helps us optimize. Lacking that, clearing the guard type is a
    // simpler short-term solution.
    // Note that clearing unmarked types from the cache and guard is needed for correctness if the cache doesn't keep
    // the types alive.
    FOREACH_DLISTBASE_ENTRY_EDITING(Js::EntryPointInfo *, entryPoint, &equivalentTypeCacheEntryPoints, iter)
    {TRACE_IT(37313);
        bool isLive = entryPoint->ClearEquivalentTypeCaches();
        if (!isLive)
        {TRACE_IT(37314);
            iter.RemoveCurrent(&equivalentTypeCacheInfoAllocator);
        }
    }
    NEXT_DLISTBASE_ENTRY_EDITING;

    // Note: Don't reset the list, because we're only clearing the dead types from these caches.
    // There may still be type references we need to keep an eye on.
#endif
}

Js::EntryPointInfo **
ThreadContext::RegisterEquivalentTypeCacheEntryPoint(Js::EntryPointInfo * entryPoint)
{TRACE_IT(37315);
    return equivalentTypeCacheEntryPoints.PrependNode(&equivalentTypeCacheInfoAllocator, entryPoint);
}

void
ThreadContext::UnregisterEquivalentTypeCacheEntryPoint(Js::EntryPointInfo ** entryPoint)
{TRACE_IT(37316);
    equivalentTypeCacheEntryPoints.RemoveElement(&equivalentTypeCacheInfoAllocator, entryPoint);
}

void
ThreadContext::RegisterProtoInlineCache(Js::InlineCache * inlineCache, Js::PropertyId propertyId)
{TRACE_IT(37317);
    if (PHASE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
    {TRACE_IT(37318);
        Output::Print(_u("InlineCacheInvalidation: registering proto cache 0x%p for property %s(%u)\n"),
            inlineCache, GetPropertyName(propertyId)->GetBuffer(), propertyId);
        Output::Flush();
    }

    RegisterInlineCache(protoInlineCacheByPropId, inlineCache, propertyId);
}

void
ThreadContext::RegisterStoreFieldInlineCache(Js::InlineCache * inlineCache, Js::PropertyId propertyId)
{TRACE_IT(37319);
    if (PHASE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
    {TRACE_IT(37320);
        Output::Print(_u("InlineCacheInvalidation: registering store field cache 0x%p for property %s(%u)\n"),
            inlineCache, GetPropertyName(propertyId)->GetBuffer(), propertyId);
        Output::Flush();
    }

    RegisterInlineCache(storeFieldInlineCacheByPropId, inlineCache, propertyId);
}

void
ThreadContext::RegisterInlineCache(InlineCacheListMapByPropertyId& inlineCacheMap, Js::InlineCache * inlineCache, Js::PropertyId propertyId)
{TRACE_IT(37321);
    InlineCacheList* inlineCacheList;
    if (!inlineCacheMap.TryGetValue(propertyId, &inlineCacheList))
    {TRACE_IT(37322);
        inlineCacheList = Anew(&this->inlineCacheThreadInfoAllocator, InlineCacheList, &this->inlineCacheThreadInfoAllocator);
        inlineCacheMap.AddNew(propertyId, inlineCacheList);
    }

    Js::InlineCache** inlineCacheRef = inlineCacheList->PrependNode();
    Assert(inlineCacheRef != nullptr);
    *inlineCacheRef = inlineCache;
    inlineCache->invalidationListSlotPtr = inlineCacheRef;
    this->registeredInlineCacheCount++;
}

void ThreadContext::NotifyInlineCacheBatchUnregistered(uint count)
{TRACE_IT(37323);
    this->unregisteredInlineCacheCount += count;
    // Negative or 0 InlineCacheInvalidationListCompactionThreshold forces compaction all the time.
    if (CONFIG_FLAG(InlineCacheInvalidationListCompactionThreshold) <= 0 ||
        this->registeredInlineCacheCount / this->unregisteredInlineCacheCount < (uint)CONFIG_FLAG(InlineCacheInvalidationListCompactionThreshold))
    {TRACE_IT(37324);
        CompactInlineCacheInvalidationLists();
    }
}

void
ThreadContext::InvalidateProtoInlineCaches(Js::PropertyId propertyId)
{TRACE_IT(37325);
    InlineCacheList* inlineCacheList;
    if (protoInlineCacheByPropId.TryGetValueAndRemove(propertyId, &inlineCacheList))
    {TRACE_IT(37326);
        if (PHASE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
        {TRACE_IT(37327);
            Output::Print(_u("InlineCacheInvalidation: invalidating proto caches for property %s(%u)\n"),
                GetPropertyName(propertyId)->GetBuffer(), propertyId);
            Output::Flush();
        }

        InvalidateAndDeleteInlineCacheList(inlineCacheList);
    }
}

void
ThreadContext::InvalidateStoreFieldInlineCaches(Js::PropertyId propertyId)
{TRACE_IT(37328);
    InlineCacheList* inlineCacheList;
    if (storeFieldInlineCacheByPropId.TryGetValueAndRemove(propertyId, &inlineCacheList))
    {TRACE_IT(37329);
        if (PHASE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
        {TRACE_IT(37330);
            Output::Print(_u("InlineCacheInvalidation: invalidating store field caches for property %s(%u)\n"),
                GetPropertyName(propertyId)->GetBuffer(), propertyId);
            Output::Flush();
        }

        InvalidateAndDeleteInlineCacheList(inlineCacheList);
    }
}

void
ThreadContext::InvalidateAndDeleteInlineCacheList(InlineCacheList* inlineCacheList)
{TRACE_IT(37331);
    Assert(inlineCacheList != nullptr);

    uint cacheCount = 0;
    uint nullCacheCount = 0;
    FOREACH_SLISTBASE_ENTRY(Js::InlineCache*, inlineCache, inlineCacheList)
    {TRACE_IT(37332);
        cacheCount++;
        if (inlineCache != nullptr)
        {TRACE_IT(37333);
            if (PHASE_VERBOSE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
            {TRACE_IT(37334);
                Output::Print(_u("InlineCacheInvalidation: invalidating cache 0x%p\n"), inlineCache);
                Output::Flush();
            }

            memset(inlineCache, 0, sizeof(Js::InlineCache));
        }
        else
        {TRACE_IT(37335);
            nullCacheCount++;
        }
    }
    NEXT_SLISTBASE_ENTRY;
    Adelete(&this->inlineCacheThreadInfoAllocator, inlineCacheList);
    this->registeredInlineCacheCount = this->registeredInlineCacheCount > cacheCount ? this->registeredInlineCacheCount - cacheCount : 0;
    this->unregisteredInlineCacheCount = this->unregisteredInlineCacheCount > nullCacheCount ? this->unregisteredInlineCacheCount - nullCacheCount : 0;
}

void
ThreadContext::CompactInlineCacheInvalidationLists()
{TRACE_IT(37336);
#if DBG
    uint countOfNodesToCompact = this->unregisteredInlineCacheCount;
    this->totalUnregisteredCacheCount = 0;
#endif
    Assert(this->unregisteredInlineCacheCount > 0);
    CompactProtoInlineCaches();

    if (this->unregisteredInlineCacheCount > 0)
    {TRACE_IT(37337);
        CompactStoreFieldInlineCaches();
    }
    Assert(countOfNodesToCompact == this->totalUnregisteredCacheCount);
}

void
ThreadContext::CompactProtoInlineCaches()
{TRACE_IT(37338);
    protoInlineCacheByPropId.MapUntil([this](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        CompactInlineCacheList(inlineCacheList);
        return this->unregisteredInlineCacheCount == 0;
    });
}

void
ThreadContext::CompactStoreFieldInlineCaches()
{TRACE_IT(37339);
    storeFieldInlineCacheByPropId.MapUntil([this](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        CompactInlineCacheList(inlineCacheList);
        return this->unregisteredInlineCacheCount == 0;
    });
}

void
ThreadContext::CompactInlineCacheList(InlineCacheList* inlineCacheList)
{TRACE_IT(37340);
    Assert(inlineCacheList != nullptr);
    uint cacheCount = 0;
    FOREACH_SLISTBASE_ENTRY_EDITING(Js::InlineCache*, inlineCache, inlineCacheList, iterator)
    {TRACE_IT(37341);
        if (inlineCache == nullptr)
        {TRACE_IT(37342);
            iterator.RemoveCurrent();
            cacheCount++;
        }
    }
    NEXT_SLISTBASE_ENTRY_EDITING;

#if DBG
    this->totalUnregisteredCacheCount += cacheCount;
#endif
    if (cacheCount > 0)
    {TRACE_IT(37343);
        AssertMsg(this->unregisteredInlineCacheCount >= cacheCount, "Some codepaths didn't unregistered the inlineCaches which might leak memory.");
        this->unregisteredInlineCacheCount = this->unregisteredInlineCacheCount > cacheCount ?
            this->unregisteredInlineCacheCount - cacheCount : 0;

        AssertMsg(this->registeredInlineCacheCount >= cacheCount, "Some codepaths didn't registered the inlineCaches which might leak memory.");
        this->registeredInlineCacheCount = this->registeredInlineCacheCount > cacheCount ?
            this->registeredInlineCacheCount - cacheCount : 0;
    }
}

#if DBG
bool
ThreadContext::IsProtoInlineCacheRegistered(const Js::InlineCache* inlineCache, Js::PropertyId propertyId)
{TRACE_IT(37344);
    return IsInlineCacheRegistered(protoInlineCacheByPropId, inlineCache, propertyId);
}

bool
ThreadContext::IsStoreFieldInlineCacheRegistered(const Js::InlineCache* inlineCache, Js::PropertyId propertyId)
{TRACE_IT(37345);
    return IsInlineCacheRegistered(storeFieldInlineCacheByPropId, inlineCache, propertyId);
}

bool
ThreadContext::IsInlineCacheRegistered(InlineCacheListMapByPropertyId& inlineCacheMap, const Js::InlineCache* inlineCache, Js::PropertyId propertyId)
{TRACE_IT(37346);
    InlineCacheList* inlineCacheList;
    if (inlineCacheMap.TryGetValue(propertyId, &inlineCacheList))
    {TRACE_IT(37347);
        return IsInlineCacheInList(inlineCache, inlineCacheList);
    }
    else
    {TRACE_IT(37348);
        return false;
    }
}

bool
ThreadContext::IsInlineCacheInList(const Js::InlineCache* inlineCache, const InlineCacheList* inlineCacheList)
{TRACE_IT(37349);
    Assert(inlineCache != nullptr);
    Assert(inlineCacheList != nullptr);

    FOREACH_SLISTBASE_ENTRY(Js::InlineCache*, curInlineCache, inlineCacheList)
    {TRACE_IT(37350);
        if (curInlineCache == inlineCache)
        {TRACE_IT(37351);
            return true;
        }
    }
    NEXT_SLISTBASE_ENTRY;

    return false;
}
#endif

#if ENABLE_NATIVE_CODEGEN
ThreadContext::PropertyGuardEntry*
ThreadContext::EnsurePropertyGuardEntry(const Js::PropertyRecord* propertyRecord, bool& foundExistingEntry)
{TRACE_IT(37352);
    PropertyGuardDictionary &guards = this->recyclableData->propertyGuards;
    PropertyGuardEntry* entry;

    foundExistingEntry = guards.TryGetValue(propertyRecord, &entry);
    if (!foundExistingEntry)
    {TRACE_IT(37353);
        entry = RecyclerNew(GetRecycler(), PropertyGuardEntry, GetRecycler());

        guards.UncheckedAdd(CreatePropertyRecordWeakRef(propertyRecord), entry);
    }

    return entry;
}

Js::PropertyGuard*
ThreadContext::RegisterSharedPropertyGuard(Js::PropertyId propertyId)
{TRACE_IT(37354);
    Assert(IsActivePropertyId(propertyId));

    const Js::PropertyRecord * propertyRecord = GetPropertyName(propertyId);

    bool foundExistingGuard;
    PropertyGuardEntry* entry = EnsurePropertyGuardEntry(propertyRecord, foundExistingGuard);

    if (entry->sharedGuard == nullptr)
    {TRACE_IT(37355);
        entry->sharedGuard = Js::PropertyGuard::New(GetRecycler());
    }

    Js::PropertyGuard* guard = entry->sharedGuard;

    PHASE_PRINT_VERBOSE_TRACE1(Js::FixedMethodsPhase, _u("FixedFields: registered shared guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p, %s\n"),
        propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue(), foundExistingGuard ? _u("existing") : _u("new"));
    PHASE_PRINT_TESTTRACE1(Js::FixedMethodsPhase, _u("FixedFields: registered shared guard: name: %s, value: 0x%p, %s\n"),
        propertyRecord->GetBuffer(), guard->GetValue(), foundExistingGuard ? _u("existing") : _u("new"));

    return guard;
}

void
ThreadContext::RegisterLazyBailout(Js::PropertyId propertyId, Js::EntryPointInfo* entryPoint)
{TRACE_IT(37356);
    const Js::PropertyRecord * propertyRecord = GetPropertyName(propertyId);

    bool foundExistingGuard;
    PropertyGuardEntry* entry = EnsurePropertyGuardEntry(propertyRecord, foundExistingGuard);
    if (!entry->entryPoints)
    {TRACE_IT(37357);
        entry->entryPoints = RecyclerNew(recycler, PropertyGuardEntry::EntryPointDictionary, recycler, /*capacity*/ 3);
    }
    entry->entryPoints->UncheckedAdd(entryPoint, NULL);
}

void
ThreadContext::RegisterUniquePropertyGuard(Js::PropertyId propertyId, Js::PropertyGuard* guard)
{TRACE_IT(37358);
    Assert(IsActivePropertyId(propertyId));
    Assert(guard != nullptr);

    RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef = this->recycler->CreateWeakReferenceHandle(guard);
    RegisterUniquePropertyGuard(propertyId, guardWeakRef);
}

void
ThreadContext::RegisterUniquePropertyGuard(Js::PropertyId propertyId, RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef)
{TRACE_IT(37359);
    Assert(IsActivePropertyId(propertyId));
    Assert(guardWeakRef != nullptr);

    Js::PropertyGuard* guard = guardWeakRef->Get();
    Assert(guard != nullptr);

    const Js::PropertyRecord * propertyRecord = GetPropertyName(propertyId);

    bool foundExistingGuard;


    PropertyGuardEntry* entry = EnsurePropertyGuardEntry(propertyRecord, foundExistingGuard);

    entry->uniqueGuards.Item(guardWeakRef);

    if (PHASE_TRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase))
    {TRACE_IT(37360);
        Output::Print(_u("FixedFields: registered unique guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p, %s entry\n"),
            propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue(), foundExistingGuard ? _u("existing") : _u("new"));
        Output::Flush();
    }

    if (PHASE_TESTTRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))
    {TRACE_IT(37361);
        Output::Print(_u("FixedFields: registered unique guard: name: %s, value: 0x%p, %s entry\n"),
            propertyRecord->GetBuffer(), guard->GetValue(), foundExistingGuard ? _u("existing") : _u("new"));
        Output::Flush();
    }
}

void
ThreadContext::RegisterConstructorCache(Js::PropertyId propertyId, Js::ConstructorCache* cache)
{TRACE_IT(37362);
    Assert(Js::ConstructorCache::GetOffsetOfGuardValue() == Js::PropertyGuard::GetOffsetOfValue());
    Assert(Js::ConstructorCache::GetSizeOfGuardValue() == Js::PropertyGuard::GetSizeOfValue());
    RegisterUniquePropertyGuard(propertyId, reinterpret_cast<Js::PropertyGuard*>(cache));
}

void
ThreadContext::InvalidatePropertyGuardEntry(const Js::PropertyRecord* propertyRecord, PropertyGuardEntry* entry, bool isAllPropertyGuardsInvalidation)
{TRACE_IT(37363);
    Assert(entry != nullptr);

    if (entry->sharedGuard != nullptr)
    {TRACE_IT(37364);
        Js::PropertyGuard* guard = entry->sharedGuard;

        if (PHASE_TRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase))
        {TRACE_IT(37365);
            Output::Print(_u("FixedFields: invalidating guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p\n"),
                propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue());
            Output::Flush();
        }

        if (PHASE_TESTTRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))
        {TRACE_IT(37366);
            Output::Print(_u("FixedFields: invalidating guard: name: %s, value: 0x%p\n"), propertyRecord->GetBuffer(), guard->GetValue());
            Output::Flush();
        }

        guard->Invalidate();
    }

    uint count = 0;
    entry->uniqueGuards.Map([&count, propertyRecord](RecyclerWeakReference<Js::PropertyGuard>* guardWeakRef)
    {
        Js::PropertyGuard* guard = guardWeakRef->Get();
        if (guard != nullptr)
        {TRACE_IT(37367);
            if (PHASE_TRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TRACE1(Js::FixedMethodsPhase))
            {TRACE_IT(37368);
                Output::Print(_u("FixedFields: invalidating guard: name: %s, address: 0x%p, value: 0x%p, value address: 0x%p\n"),
                    propertyRecord->GetBuffer(), guard, guard->GetValue(), guard->GetAddressOfValue());
                Output::Flush();
            }

            if (PHASE_TESTTRACE1(Js::TracePropertyGuardsPhase) || PHASE_VERBOSE_TESTTRACE1(Js::FixedMethodsPhase))
            {TRACE_IT(37369);
                Output::Print(_u("FixedFields: invalidating guard: name: %s, value: 0x%p\n"),
                    propertyRecord->GetBuffer(), guard->GetValue());
                Output::Flush();
            }

            guard->Invalidate();
            count++;
        }
    });

    entry->uniqueGuards.Clear();


    // Count no. of invalidations done so far. Exclude if this is all property guards invalidation in which case
    // the unique Guards will be cleared anyway.
    if (!isAllPropertyGuardsInvalidation)
    {TRACE_IT(37370);
        this->recyclableData->constructorCacheInvalidationCount += count;
        if (this->recyclableData->constructorCacheInvalidationCount > (uint)CONFIG_FLAG(ConstructorCacheInvalidationThreshold))
        {TRACE_IT(37371);
            // TODO: In future, we should compact the uniqueGuards dictionary so this function can be called from PreCollectionCallback
            // instead
            this->ClearInvalidatedUniqueGuards();
            this->recyclableData->constructorCacheInvalidationCount = 0;
        }
    }

    if (entry->entryPoints && entry->entryPoints->Count() > 0)
    {TRACE_IT(37372);
        Js::JavascriptStackWalker stackWalker(this->GetScriptContextList());
        Js::JavascriptFunction* caller;
        while (stackWalker.GetCaller(&caller, /*includeInlineFrames*/ false))
        {TRACE_IT(37373);
            // If the current frame is already from a bailout - we do not need to do on stack invalidation
            if (caller != nullptr && Js::ScriptFunction::Test(caller) && !stackWalker.GetCurrentFrameFromBailout())
            {TRACE_IT(37374);
                BYTE dummy;
                Js::FunctionEntryPointInfo* functionEntryPoint = caller->GetFunctionBody()->GetDefaultFunctionEntryPointInfo();
                if (functionEntryPoint->IsInNativeAddressRange((DWORD_PTR)stackWalker.GetInstructionPointer()))
                {TRACE_IT(37375);
                    if (entry->entryPoints->TryGetValue(functionEntryPoint, &dummy))
                    {TRACE_IT(37376);
                        functionEntryPoint->DoLazyBailout(stackWalker.GetCurrentAddressOfInstructionPointer(),
                            caller->GetFunctionBody(), propertyRecord);
                    }
                }
            }
        }
        entry->entryPoints->Map([=](Js::EntryPointInfo* info, BYTE& dummy, const RecyclerWeakReference<Js::EntryPointInfo>* infoWeakRef)
        {
            OUTPUT_TRACE2(Js::LazyBailoutPhase, info->GetFunctionBody(), _u("Lazy bailout - Invalidation due to property: %s \n"), propertyRecord->GetBuffer());
            info->Invalidate(true);
        });
        entry->entryPoints->Clear();
    }
}

void
ThreadContext::InvalidatePropertyGuards(Js::PropertyId propertyId)
{TRACE_IT(37377);
    const Js::PropertyRecord* propertyRecord = GetPropertyName(propertyId);
    PropertyGuardDictionary &guards = this->recyclableData->propertyGuards;
    PropertyGuardEntry* entry;
    if (guards.TryGetValueAndRemove(propertyRecord, &entry))
    {
        InvalidatePropertyGuardEntry(propertyRecord, entry, false);
    }
}

void
ThreadContext::InvalidateAllPropertyGuards()
{TRACE_IT(37378);
    PropertyGuardDictionary &guards = this->recyclableData->propertyGuards;
    if (guards.Count() > 0)
    {TRACE_IT(37379);
        guards.Map([this](Js::PropertyRecord const * propertyRecord, PropertyGuardEntry* entry, const RecyclerWeakReference<const Js::PropertyRecord>* weakRef)
        {
            InvalidatePropertyGuardEntry(propertyRecord, entry, true);
        });

        guards.Clear();
    }
}
#endif

void
ThreadContext::InvalidateAllProtoInlineCaches()
{TRACE_IT(37380);
    protoInlineCacheByPropId.Map([this](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        InvalidateAndDeleteInlineCacheList(inlineCacheList);
    });
    protoInlineCacheByPropId.Reset();
}

#if DBG

// Verifies if object is registered in any proto InlineCache
bool
ThreadContext::IsObjectRegisteredInProtoInlineCaches(Js::DynamicObject * object)
{TRACE_IT(37381);
    return protoInlineCacheByPropId.MapUntil([object](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        FOREACH_SLISTBASE_ENTRY(Js::InlineCache*, inlineCache, inlineCacheList)
        {
            if (inlineCache != nullptr && !inlineCache->IsEmpty())
            {TRACE_IT(37382);
                // Verify this object is not present in prototype chain of inlineCache's type
                bool isObjectPresentOnPrototypeChain =
                    Js::JavascriptOperators::MapObjectAndPrototypesUntil<true>(inlineCache->GetType()->GetPrototype(), [=](Js::RecyclableObject* prototype)
                {
                    return prototype == object;
                });
                if (isObjectPresentOnPrototypeChain) {TRACE_IT(37383);
                    return true;
                }
            }
        }
        NEXT_SLISTBASE_ENTRY;
        return false;
    });
}

// Verifies if object is registered in any storeField InlineCache
bool
ThreadContext::IsObjectRegisteredInStoreFieldInlineCaches(Js::DynamicObject * object)
{TRACE_IT(37384);
    return storeFieldInlineCacheByPropId.MapUntil([object](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        FOREACH_SLISTBASE_ENTRY(Js::InlineCache*, inlineCache, inlineCacheList)
        {
            if (inlineCache != nullptr && !inlineCache->IsEmpty())
            {TRACE_IT(37385);
                // Verify this object is not present in prototype chain of inlineCache's type
                bool isObjectPresentOnPrototypeChain =
                    Js::JavascriptOperators::MapObjectAndPrototypesUntil<true>(inlineCache->GetType()->GetPrototype(), [=](Js::RecyclableObject* prototype)
                {
                    return prototype == object;
                });
                if (isObjectPresentOnPrototypeChain) {TRACE_IT(37386);
                    return true;
                }
            }
        }
        NEXT_SLISTBASE_ENTRY;
        return false;
    });
}
#endif

bool
ThreadContext::AreAllProtoInlineCachesInvalidated()
{TRACE_IT(37387);
    return protoInlineCacheByPropId.Count() == 0;
}

void
ThreadContext::InvalidateAllStoreFieldInlineCaches()
{TRACE_IT(37388);
    storeFieldInlineCacheByPropId.Map([this](Js::PropertyId propertyId, InlineCacheList* inlineCacheList)
    {
        InvalidateAndDeleteInlineCacheList(inlineCacheList);
    });
    storeFieldInlineCacheByPropId.Reset();
}

bool
ThreadContext::AreAllStoreFieldInlineCachesInvalidated()
{TRACE_IT(37389);
    return storeFieldInlineCacheByPropId.Count() == 0;
}

#if DBG
bool
ThreadContext::IsIsInstInlineCacheRegistered(Js::IsInstInlineCache * inlineCache, Js::Var function)
{TRACE_IT(37390);
    Assert(inlineCache != nullptr);
    Assert(function != nullptr);
    Js::IsInstInlineCache* firstInlineCache;
    if (this->isInstInlineCacheByFunction.TryGetValue(function, &firstInlineCache))
    {TRACE_IT(37391);
        return IsIsInstInlineCacheInList(inlineCache, firstInlineCache);
    }
    else
    {TRACE_IT(37392);
        return false;
    }
}
#endif

void
ThreadContext::RegisterIsInstInlineCache(Js::IsInstInlineCache * inlineCache, Js::Var function)
{TRACE_IT(37393);
    Assert(function != nullptr);
    Assert(inlineCache != nullptr);
    // We should never cross-register or re-register a cache that is already on some invalidation list (for its function or some other function).
    // Every cache must be first cleared and unregistered before being registered again.
    AssertMsg(inlineCache->function == nullptr, "We should only register instance-of caches that have not yet been populated.");
    Js::IsInstInlineCache** inlineCacheRef = nullptr;

    if (this->isInstInlineCacheByFunction.TryGetReference(function, &inlineCacheRef))
    {
        AssertMsg(!IsIsInstInlineCacheInList(inlineCache, *inlineCacheRef), "Why are we registering a cache that is already registered?");
        inlineCache->next = *inlineCacheRef;
        *inlineCacheRef = inlineCache;
    }
    else
    {TRACE_IT(37394);
        inlineCache->next = nullptr;
        this->isInstInlineCacheByFunction.Add(function, inlineCache);
    }
}

void
ThreadContext::UnregisterIsInstInlineCache(Js::IsInstInlineCache * inlineCache, Js::Var function)
{TRACE_IT(37395);
    Assert(inlineCache != nullptr);
    Js::IsInstInlineCache** inlineCacheRef = nullptr;

    if (this->isInstInlineCacheByFunction.TryGetReference(function, &inlineCacheRef))
    {TRACE_IT(37396);
        Assert(*inlineCacheRef != nullptr);
        if (inlineCache == *inlineCacheRef)
        {TRACE_IT(37397);
            *inlineCacheRef = (*inlineCacheRef)->next;
            if (*inlineCacheRef == nullptr)
            {TRACE_IT(37398);
                this->isInstInlineCacheByFunction.Remove(function);
            }
        }
        else
        {TRACE_IT(37399);
            Js::IsInstInlineCache * prevInlineCache;
            Js::IsInstInlineCache * curInlineCache;
            for (prevInlineCache = *inlineCacheRef, curInlineCache = (*inlineCacheRef)->next; curInlineCache != nullptr;
                prevInlineCache = curInlineCache, curInlineCache = curInlineCache->next)
            {TRACE_IT(37400);
                if (curInlineCache == inlineCache)
                {TRACE_IT(37401);
                    prevInlineCache->next = curInlineCache->next;
                    return;
                }
            }
            AssertMsg(false, "Why are we unregistering a cache that is not registered?");
        }
    }
}

void
ThreadContext::InvalidateIsInstInlineCacheList(Js::IsInstInlineCache* inlineCacheList)
{TRACE_IT(37402);
    Assert(inlineCacheList != nullptr);
    Js::IsInstInlineCache* curInlineCache;
    Js::IsInstInlineCache* nextInlineCache;
    for (curInlineCache = inlineCacheList; curInlineCache != nullptr; curInlineCache = nextInlineCache)
    {TRACE_IT(37403);
        if (PHASE_VERBOSE_TRACE1(Js::TraceInlineCacheInvalidationPhase))
        {TRACE_IT(37404);
            Output::Print(_u("InlineCacheInvalidation: invalidating instanceof cache 0x%p\n"), curInlineCache);
            Output::Flush();
        }
        // Stash away the next cache before we zero out the current one (including its next pointer).
        nextInlineCache = curInlineCache->next;
        // Clear the current cache to invalidate it.
        memset(curInlineCache, 0, sizeof(Js::IsInstInlineCache));
    }
}

void
ThreadContext::InvalidateIsInstInlineCachesForFunction(Js::Var function)
{TRACE_IT(37405);
    Js::IsInstInlineCache* inlineCacheList;
    if (this->isInstInlineCacheByFunction.TryGetValueAndRemove(function, &inlineCacheList))
    {TRACE_IT(37406);
        InvalidateIsInstInlineCacheList(inlineCacheList);
    }
}

void
ThreadContext::InvalidateAllIsInstInlineCaches()
{TRACE_IT(37407);
    isInstInlineCacheByFunction.Map([this](const Js::Var function, Js::IsInstInlineCache* inlineCacheList)
    {
        InvalidateIsInstInlineCacheList(inlineCacheList);
    });
    isInstInlineCacheByFunction.Clear();
}

bool
ThreadContext::AreAllIsInstInlineCachesInvalidated() const
{TRACE_IT(37408);
    return isInstInlineCacheByFunction.Count() == 0;
}

#if DBG
bool
ThreadContext::IsIsInstInlineCacheInList(const Js::IsInstInlineCache* inlineCache, const Js::IsInstInlineCache* inlineCacheList)
{TRACE_IT(37409);
    Assert(inlineCache != nullptr);
    Assert(inlineCacheList != nullptr);

    for (const Js::IsInstInlineCache* curInlineCache = inlineCacheList; curInlineCache != nullptr; curInlineCache = curInlineCache->next)
    {TRACE_IT(37410);
        if (curInlineCache == inlineCache)
        {TRACE_IT(37411);
            return true;
        }
    }

    return false;
}
#endif

void ThreadContext::RegisterTypeWithProtoPropertyCache(const Js::PropertyId propertyId, Js::Type *const type)
{TRACE_IT(37412);
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(IsActivePropertyId(propertyId));
    Assert(type);

    PropertyIdToTypeHashSetDictionary &typesWithProtoPropertyCache = recyclableData->typesWithProtoPropertyCache;
    TypeHashSet *typeHashSet;
    if(!typesWithProtoPropertyCache.TryGetValue(propertyId, &typeHashSet))
    {TRACE_IT(37413);
        typeHashSet = RecyclerNew(recycler, TypeHashSet, recycler);
        typesWithProtoPropertyCache.Add(propertyId, typeHashSet);
    }

    typeHashSet->Item(type, false);
}

void ThreadContext::InvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId)
{TRACE_IT(37414);
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(IsActivePropertyId(propertyId));
    InternalInvalidateProtoTypePropertyCaches(propertyId);
}

void ThreadContext::InternalInvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId)
{TRACE_IT(37415);
    // Get the hash set of registered types associated with the property ID, invalidate each type in the hash set, and
    // remove the property ID and its hash set from the map
    PropertyIdToTypeHashSetDictionary &typesWithProtoPropertyCache = recyclableData->typesWithProtoPropertyCache;
    TypeHashSet *typeHashSet;
    if(typesWithProtoPropertyCache.Count() != 0 && typesWithProtoPropertyCache.TryGetValueAndRemove(propertyId, &typeHashSet))
    {
        DoInvalidateProtoTypePropertyCaches(propertyId, typeHashSet);
    }
}

void ThreadContext::InvalidateAllProtoTypePropertyCaches()
{TRACE_IT(37416);
    PropertyIdToTypeHashSetDictionary &typesWithProtoPropertyCache = recyclableData->typesWithProtoPropertyCache;
    if (typesWithProtoPropertyCache.Count() > 0)
    {TRACE_IT(37417);
        typesWithProtoPropertyCache.Map([this](Js::PropertyId propertyId, TypeHashSet * typeHashSet)
        {
            DoInvalidateProtoTypePropertyCaches(propertyId, typeHashSet);
        });
        typesWithProtoPropertyCache.Clear();
    }
}

void ThreadContext::DoInvalidateProtoTypePropertyCaches(const Js::PropertyId propertyId, TypeHashSet *const typeHashSet)
{TRACE_IT(37418);
    Assert(propertyId != Js::Constants::NoProperty);
    Assert(typeHashSet);

    typeHashSet->Map(
        [propertyId](Js::Type *const type, const bool unused, const RecyclerWeakReference<Js::Type>*)
        {
            type->GetPropertyCache()->ClearIfPropertyIsOnAPrototype(propertyId);
        });
}

Js::ScriptContext **
ThreadContext::RegisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext(Js::ScriptContext * scriptContext)
{TRACE_IT(37419);
    return prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext.PrependNode(&prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator, scriptContext);
}

void
ThreadContext::UnregisterPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext(Js::ScriptContext ** scriptContext)
{TRACE_IT(37420);
    prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext.RemoveElement(&prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator, scriptContext);
}

void
ThreadContext::ClearPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesCaches()
{TRACE_IT(37421);
    bool hasItem = false;
    FOREACH_DLISTBASE_ENTRY(Js::ScriptContext *, scriptContext, &prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext)
    {TRACE_IT(37422);
        scriptContext->ClearPrototypeChainEnsuredToHaveOnlyWritableDataPropertiesCaches();
        hasItem = true;
    }
    NEXT_DLISTBASE_ENTRY;

    if (!hasItem)
    {TRACE_IT(37423);
        return;
    }

    prototypeChainEnsuredToHaveOnlyWritableDataPropertiesScriptContext.Reset();
    prototypeChainEnsuredToHaveOnlyWritableDataPropertiesAllocator.Reset();
}

BOOL ThreadContext::HasPreviousHostScriptContext()
{TRACE_IT(37424);
    return hostScriptContextStack->Count() != 0;
}

HostScriptContext* ThreadContext::GetPreviousHostScriptContext()
{TRACE_IT(37425);
    return hostScriptContextStack->Peek();
}

void ThreadContext::PushHostScriptContext(HostScriptContext* topProvider)
{TRACE_IT(37426);
    // script engine can be created coming from GetDispID, so push/pop can be
    // happening after the first round of enterscript as well. we might need to
    // revisit the whole callRootLevel but probably not now.
    // Assert(HasPreviousHostScriptContext() || callRootLevel == 0);
    hostScriptContextStack->Push(topProvider);
}

HostScriptContext* ThreadContext::PopHostScriptContext()
{TRACE_IT(37427);
    return hostScriptContextStack->Pop();
    // script engine can be created coming from GetDispID, so push/pop can be
    // happening after the first round of enterscript as well. we might need to
    // revisit the whole callRootLevel but probably not now.
    // Assert(HasPreviousHostScriptContext() || callRootLevel == 0);
}

#if DBG || defined(PROFILE_EXEC)
bool
ThreadContext::AsyncHostOperationStart(void * suspendRecord)
{TRACE_IT(37428);
    bool wasInAsync = false;
    Assert(!this->IsScriptActive());
    Js::ScriptEntryExitRecord * lastRecord = this->entryExitRecord;
    if (lastRecord != NULL)
    {TRACE_IT(37429);
        if (!lastRecord->leaveForHost)
        {TRACE_IT(37430);
#if DBG
            wasInAsync = !!lastRecord->leaveForAsyncHostOperation;
            lastRecord->leaveForAsyncHostOperation = true;
#endif
#ifdef PROFILE_EXEC
            lastRecord->scriptContext->ProfileSuspend(Js::RunPhase, (Js::Profiler::SuspendRecord *)suspendRecord);
#endif
        }
        else
        {TRACE_IT(37431);
            Assert(!lastRecord->leaveForAsyncHostOperation);
        }
    }
    return wasInAsync;
}

void
ThreadContext::AsyncHostOperationEnd(bool wasInAsync, void * suspendRecord)
{TRACE_IT(37432);
    Assert(!this->IsScriptActive());
    Js::ScriptEntryExitRecord * lastRecord = this->entryExitRecord;
    if (lastRecord != NULL)
    {TRACE_IT(37433);
        if (!lastRecord->leaveForHost)
        {TRACE_IT(37434);
            Assert(lastRecord->leaveForAsyncHostOperation);
#if DBG
            lastRecord->leaveForAsyncHostOperation = wasInAsync;
#endif
#ifdef PROFILE_EXEC
            lastRecord->scriptContext->ProfileResume((Js::Profiler::SuspendRecord *)suspendRecord);
#endif
        }
        else
        {TRACE_IT(37435);
            Assert(!lastRecord->leaveForAsyncHostOperation);
            Assert(!wasInAsync);
        }
    }
}

#endif

#ifdef RECYCLER_DUMP_OBJECT_GRAPH
void DumpRecyclerObjectGraph()
{TRACE_IT(37436);
    ThreadContext * threadContext = ThreadContext::GetContextForCurrentThread();
    if (threadContext == nullptr)
    {TRACE_IT(37437);
        Output::Print(_u("No thread context"));
    }
    threadContext->GetRecycler()->DumpObjectGraph();
}
#endif

#if ENABLE_NATIVE_CODEGEN
bool ThreadContext::IsNativeAddressHelper(void * pCodeAddr, Js::ScriptContext* currentScriptContext)
{TRACE_IT(37438);
    bool isNativeAddr = false;
    if (currentScriptContext && currentScriptContext->GetJitFuncRangeCache() != nullptr)
    {TRACE_IT(37439);
        isNativeAddr = currentScriptContext->GetJitFuncRangeCache()->IsNativeAddr(pCodeAddr);
    }

    for (Js::ScriptContext *scriptContext = scriptContextList; scriptContext && !isNativeAddr; scriptContext = scriptContext->next)
    {TRACE_IT(37440);
        if (scriptContext == currentScriptContext || scriptContext->GetJitFuncRangeCache() == nullptr)
        {TRACE_IT(37441);
            continue;
        }
        isNativeAddr = scriptContext->GetJitFuncRangeCache()->IsNativeAddr(pCodeAddr);
    }
    return isNativeAddr;
}

BOOL ThreadContext::IsNativeAddress(void * pCodeAddr, Js::ScriptContext* currentScriptContext)
{TRACE_IT(37442);
#if ENABLE_OOP_NATIVE_CODEGEN
    if (JITManager::GetJITManager()->IsOOPJITEnabled())
    {TRACE_IT(37443);
        if (PreReservedVirtualAllocWrapper::IsInRange((void*)m_prereservedRegionAddr, pCodeAddr))
        {TRACE_IT(37444);
            return true;
        }
        if (IsAllJITCodeInPreReservedRegion())
        {TRACE_IT(37445);
            return false;
        }
        if (AutoSystemInfo::IsJscriptModulePointer(pCodeAddr))
        {TRACE_IT(37446);
            return false;
        }

#if DBG
        boolean result;
        HRESULT hr = JITManager::GetJITManager()->IsNativeAddr(this->m_remoteThreadContextInfo, (intptr_t)pCodeAddr, &result);
        JITManager::HandleServerCallResult(hr, RemoteCallType::HeapQuery);
#endif
        bool isNativeAddr = IsNativeAddressHelper(pCodeAddr, currentScriptContext);
#if DBG
        Assert(result == (isNativeAddr? TRUE:FALSE));
#endif
        return isNativeAddr;
    }
    else
#endif
    {TRACE_IT(37447);
        PreReservedVirtualAllocWrapper *preReservedVirtualAllocWrapper = this->GetPreReservedVirtualAllocator();
        if (preReservedVirtualAllocWrapper->IsInRange(pCodeAddr))
        {TRACE_IT(37448);
            return TRUE;
        }

        if (!this->IsAllJITCodeInPreReservedRegion())
        {TRACE_IT(37449);
#if DBG
            AutoCriticalSection autoLock(&this->codePageAllocators.cs);
#endif
            bool isNativeAddr = IsNativeAddressHelper(pCodeAddr, currentScriptContext);
#if DBG
            Assert(this->codePageAllocators.IsInNonPreReservedPageAllocator(pCodeAddr) == isNativeAddr);
#endif
            return isNativeAddr;
        }
        return FALSE;
    }
}
#endif

#if ENABLE_PROFILE_INFO
void ThreadContext::EnsureSourceProfileManagersByUrlMap()
{TRACE_IT(37450);
    if(this->recyclableData->sourceProfileManagersByUrl == nullptr)
    {TRACE_IT(37451);
        this->EnsureRecycler();
        this->recyclableData->sourceProfileManagersByUrl = RecyclerNew(GetRecycler(), SourceProfileManagersByUrlMap, GetRecycler());
    }
}

//
// Returns the cache profile manager for the URL and hash combination for a particular dynamic script. There is a ref count added for every script context
// that references the shared profile manager info.
//
Js::SourceDynamicProfileManager* ThreadContext::GetSourceDynamicProfileManager(_In_z_ const WCHAR* url, _In_ uint hash, _Inout_ bool* addRef)
{TRACE_IT(37452);
      EnsureSourceProfileManagersByUrlMap();
      Js::SourceDynamicProfileManager* profileManager = nullptr;
      SourceDynamicProfileManagerCache* managerCache;
      bool newCache = false;
      if(!this->recyclableData->sourceProfileManagersByUrl->TryGetValue(url, &managerCache))
      {TRACE_IT(37453);
          if(this->recyclableData->sourceProfileManagersByUrl->Count() >= INMEMORY_CACHE_MAX_URL)
          {TRACE_IT(37454);
              return nullptr;
          }
          managerCache = RecyclerNewZ(this->GetRecycler(), SourceDynamicProfileManagerCache);
          newCache = true;
      }
      bool createProfileManager = false;
      if(!managerCache->sourceProfileManagerMap)
      {TRACE_IT(37455);
          managerCache->sourceProfileManagerMap = RecyclerNew(this->GetRecycler(), SourceDynamicProfileManagerMap, this->GetRecycler());
          createProfileManager = true;
      }
      else
      {TRACE_IT(37456);
          createProfileManager = !managerCache->sourceProfileManagerMap->TryGetValue(hash, &profileManager);
      }
      if(createProfileManager)
      {TRACE_IT(37457);
          if(managerCache->sourceProfileManagerMap->Count() < INMEMORY_CACHE_MAX_PROFILE_MANAGER)
          {TRACE_IT(37458);
              profileManager = RecyclerNewZ(this->GetRecycler(), Js::SourceDynamicProfileManager, this->GetRecycler());
              managerCache->sourceProfileManagerMap->Add(hash, profileManager);
          }
      }
      else
      {TRACE_IT(37459);
          profileManager->Reuse();
      }

      if(!*addRef)
      {TRACE_IT(37460);
          managerCache->AddRef();
          *addRef = true;
          OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Addref dynamic source profile manger - Url: %s\n"), url);
      }

      if (newCache)
      {TRACE_IT(37461);
          // Let's make a copy of the URL because there is no guarantee this URL will remain alive in the future.
          size_t lengthInChars = wcslen(url) + 1;
          WCHAR* urlCopy = RecyclerNewArrayLeaf(GetRecycler(), WCHAR, lengthInChars);
          js_memcpy_s(urlCopy, lengthInChars * sizeof(WCHAR), url, lengthInChars * sizeof(WCHAR));
          this->recyclableData->sourceProfileManagersByUrl->Add(urlCopy, managerCache);
      }
      return profileManager;
}

//
// Decrement the ref count for this URL and cleanup the corresponding record if there are no other references to it.
//
uint ThreadContext::ReleaseSourceDynamicProfileManagers(const WCHAR* url)
{TRACE_IT(37462);
    // If we've already freed the recyclable data, we're shutting down the thread context so skip clean up
    if (this->recyclableData == nullptr) return 0;

    SourceDynamicProfileManagerCache* managerCache = this->recyclableData->sourceProfileManagersByUrl->Lookup(url, nullptr);
    uint refCount = 0;
    if(managerCache)  // manager cache may be null we exceeded -INMEMORY_CACHE_MAX_URL
    {TRACE_IT(37463);
        refCount = managerCache->Release();
        OUTPUT_VERBOSE_TRACE(Js::DynamicProfilePhase, _u("Release dynamic source profile manger %d Url: %s\n"), refCount, url);
        Output::Flush();
        if(refCount == 0)
        {TRACE_IT(37464);
            this->recyclableData->sourceProfileManagersByUrl->Remove(url);
        }
    }
    return refCount;
}
#endif

void ThreadContext::EnsureSymbolRegistrationMap()
{TRACE_IT(37465);
    if (this->recyclableData->symbolRegistrationMap == nullptr)
    {TRACE_IT(37466);
        this->EnsureRecycler();
        this->recyclableData->symbolRegistrationMap = RecyclerNew(GetRecycler(), SymbolRegistrationMap, GetRecycler());
    }
}

const Js::PropertyRecord* ThreadContext::GetSymbolFromRegistrationMap(const char16* stringKey)
{TRACE_IT(37467);
    this->EnsureSymbolRegistrationMap();

    return this->recyclableData->symbolRegistrationMap->Lookup(stringKey, nullptr);
}

const Js::PropertyRecord* ThreadContext::AddSymbolToRegistrationMap(const char16* stringKey, charcount_t stringLength)
{TRACE_IT(37468);
    this->EnsureSymbolRegistrationMap();

    const Js::PropertyRecord* propertyRecord = this->UncheckedAddPropertyId(stringKey, stringLength, /*bind*/false, /*isSymbol*/true);

    Assert(propertyRecord);

    // The key is the PropertyRecord's buffer (the PropertyRecord itself) which is being pinned as long as it's in this map.
    // If that's ever not the case, we'll need to duplicate the key here and put that in the map instead.
    this->recyclableData->symbolRegistrationMap->Add(propertyRecord->GetBuffer(), propertyRecord);

    return propertyRecord;
}

#if ENABLE_TTD
JsUtil::BaseDictionary<const char16*, const Js::PropertyRecord*, Recycler, PowerOf2SizePolicy>* ThreadContext::GetSymbolRegistrationMap_TTD()
{TRACE_IT(37469);
    //This adds a little memory but makes simplifies other logic -- maybe revise later
    this->EnsureSymbolRegistrationMap();

    return this->recyclableData->symbolRegistrationMap;
}
#endif

void ThreadContext::ClearImplicitCallFlags()
{TRACE_IT(37470);
    SetImplicitCallFlags(Js::ImplicitCall_None);
}

void ThreadContext::ClearImplicitCallFlags(Js::ImplicitCallFlags flags)
{TRACE_IT(37471);
    Assert((flags & Js::ImplicitCall_None) == 0);
    SetImplicitCallFlags((Js::ImplicitCallFlags)(implicitCallFlags & ~flags));
}

void ThreadContext::CheckAndResetImplicitCallAccessorFlag()
{TRACE_IT(37472);
    Js::ImplicitCallFlags accessorCallFlag = (Js::ImplicitCallFlags)(Js::ImplicitCall_Accessor & ~Js::ImplicitCall_None);
    if ((GetImplicitCallFlags() & accessorCallFlag) != 0)
    {TRACE_IT(37473);
        ClearImplicitCallFlags(accessorCallFlag);
        AddImplicitCallFlags(Js::ImplicitCall_NonProfiledAccessor);
    }
}

bool ThreadContext::HasNoSideEffect(Js::RecyclableObject * function) const
{TRACE_IT(37474);
    Js::FunctionInfo::Attributes attributes = Js::FunctionInfo::GetAttributes(function);

    return this->HasNoSideEffect(function, attributes);
}

bool ThreadContext::HasNoSideEffect(Js::RecyclableObject * function, Js::FunctionInfo::Attributes attributes) const
{TRACE_IT(37475);
    if (((attributes & Js::FunctionInfo::CanBeHoisted) != 0)
        || ((attributes & Js::FunctionInfo::HasNoSideEffect) != 0 && !IsDisableImplicitException()))
    {TRACE_IT(37476);
        Assert((attributes & Js::FunctionInfo::HasNoSideEffect) != 0);
        return true;
    }

    return false;
}

bool
ThreadContext::RecordImplicitException()
{TRACE_IT(37477);
    // Record the exception in the implicit call flag
    AddImplicitCallFlags(Js::ImplicitCall_Exception);
    if (IsDisableImplicitException())
    {TRACE_IT(37478);
        // Indicate that we shouldn't throw if ImplicitExceptions have been disabled
        return false;
    }
    // Disabling implicit exception when disabling implicit calls can result in valid exceptions not being thrown.
    // Instead we tell not to throw only if an implicit call happened and they are disabled. This is to cover the case
    // of an exception being thrown because an implicit call not executed left the execution in a bad state.
    // Since there is an implicit call, we expect to bailout and handle this operation in the interpreter instead.
    bool hasImplicitCallHappened = IsDisableImplicitCall() && (GetImplicitCallFlags() & ~Js::ImplicitCall_Exception);
    return !hasImplicitCallHappened;
}

void ThreadContext::SetThreadServiceWrapper(ThreadServiceWrapper* inThreadServiceWrapper)
{TRACE_IT(37479);
    AssertMsg(threadServiceWrapper == NULL || inThreadServiceWrapper == NULL, "double set ThreadServiceWrapper");
    threadServiceWrapper = inThreadServiceWrapper;
}

ThreadServiceWrapper* ThreadContext::GetThreadServiceWrapper()
{TRACE_IT(37480);
    return threadServiceWrapper;
}

uint ThreadContext::GetRandomNumber()
{TRACE_IT(37481);
#ifdef ENABLE_CUSTOM_ENTROPY
    return (uint)GetEntropy().GetRand();
#else
    uint randomNumber = 0;
    errno_t e = rand_s(&randomNumber);
    Assert(e == 0);
    return randomNumber;
#endif
}

#if defined(ENABLE_JS_ETW) && defined(NTBUILD)
void ThreadContext::EtwLogPropertyIdList()
{TRACE_IT(37482);
    propertyMap->Map([&](const Js::PropertyRecord* propertyRecord){
        EventWriteJSCRIPT_HOSTING_PROPERTYID_LIST(propertyRecord, propertyRecord->GetBuffer());
    });
}
#endif

#ifdef ENABLE_PROJECTION
void ThreadContext::AddExternalWeakReferenceCache(ExternalWeakReferenceCache *externalWeakReferenceCache)
{TRACE_IT(37483);
    this->externalWeakReferenceCacheList.Prepend(&HeapAllocator::Instance, externalWeakReferenceCache);
}

void ThreadContext::RemoveExternalWeakReferenceCache(ExternalWeakReferenceCache *externalWeakReferenceCache)
{TRACE_IT(37484);
    Assert(!externalWeakReferenceCacheList.Empty());
    this->externalWeakReferenceCacheList.Remove(&HeapAllocator::Instance, externalWeakReferenceCache);
}

void ThreadContext::MarkExternalWeakReferencedObjects(bool inPartialCollect)
{
    SListBase<ExternalWeakReferenceCache *, HeapAllocator>::Iterator iteratorWeakRefCache(&this->externalWeakReferenceCacheList);
    while (iteratorWeakRefCache.Next())
    {TRACE_IT(37485);
        iteratorWeakRefCache.Data()->MarkNow(recycler, inPartialCollect);
    }
}

void ThreadContext::ResolveExternalWeakReferencedObjects()
{
    SListBase<ExternalWeakReferenceCache *, HeapAllocator>::Iterator iteratorWeakRefCache(&this->externalWeakReferenceCacheList);
    while (iteratorWeakRefCache.Next())
    {TRACE_IT(37486);
        iteratorWeakRefCache.Data()->ResolveNow(recycler);
    }
}

#if DBG_DUMP
void ThreadContext::RegisterProjectionMemoryInformation(IProjectionContextMemoryInfo* projectionContextMemoryInfo)
{TRACE_IT(37487);
    Assert(this->projectionMemoryInformation == nullptr || this->projectionMemoryInformation == projectionContextMemoryInfo);

    this->projectionMemoryInformation = projectionContextMemoryInfo;
}

void ThreadContext::DumpProjectionContextMemoryStats(LPCWSTR headerMsg, bool forceDetailed)
{TRACE_IT(37488);
    if (this->projectionMemoryInformation)
    {TRACE_IT(37489);
        this->projectionMemoryInformation->DumpCurrentStats(headerMsg, forceDetailed);
    }
}

IProjectionContextMemoryInfo* ThreadContext::GetProjectionContextMemoryInformation()
{TRACE_IT(37490);
    return this->projectionMemoryInformation;
}
#endif
#endif

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
Js::Var ThreadContext::GetMemoryStat(Js::ScriptContext* scriptContext)
{TRACE_IT(37491);
    ScriptMemoryDumper dumper(scriptContext);
    return dumper.Dump();
}

void ThreadContext::SetAutoProxyName(LPCWSTR objectName)
{TRACE_IT(37492);
    recyclableData->autoProxyName = objectName;
}
#endif
//
// Regex helpers
//

UnifiedRegex::StandardChars<uint8>* ThreadContext::GetStandardChars(__inout_opt uint8* dummy)
{TRACE_IT(37493);
    if (standardUTF8Chars == 0)
    {TRACE_IT(37494);
        ArenaAllocator* allocator = GetThreadAlloc();
        standardUTF8Chars = Anew(allocator, UnifiedRegex::UTF8StandardChars, allocator);
    }
    return standardUTF8Chars;
}

UnifiedRegex::StandardChars<char16>* ThreadContext::GetStandardChars(__inout_opt char16* dummy)
{TRACE_IT(37495);
    if (standardUnicodeChars == 0)
    {TRACE_IT(37496);
        ArenaAllocator* allocator = GetThreadAlloc();
        standardUnicodeChars = Anew(allocator, UnifiedRegex::UnicodeStandardChars, allocator);
    }
    return standardUnicodeChars;
}

void ThreadContext::CheckScriptInterrupt()
{TRACE_IT(37497);
    if (TestThreadContextFlag(ThreadContextFlagCanDisableExecution))
    {TRACE_IT(37498);
        if (this->IsExecutionDisabled())
        {TRACE_IT(37499);
            Assert(this->DoInterruptProbe());
            throw Js::ScriptAbortException();
        }
    }
    else
    {TRACE_IT(37500);
        this->CheckInterruptPoll();
    }
}

void ThreadContext::CheckInterruptPoll()
{TRACE_IT(37501);
    // Disable QC when implicit calls are disabled since the host can do anything before returning back, like servicing the
    // message loop, which may in turn cause script code to be executed and implicit calls to be made
    if (!IsDisableImplicitCall())
    {TRACE_IT(37502);
        InterruptPoller *poller = this->interruptPoller;
        if (poller)
        {TRACE_IT(37503);
            poller->CheckInterruptPoll();
        }
    }
}

void *
ThreadContext::GetDynamicObjectEnumeratorCache(Js::DynamicType const * dynamicType)
{TRACE_IT(37504);
    void * data;
    return this->dynamicObjectEnumeratorCacheMap.TryGetValue(dynamicType, &data)? data : nullptr;
}

void
ThreadContext::AddDynamicObjectEnumeratorCache(Js::DynamicType const * dynamicType, void * cache)
{TRACE_IT(37505);
    this->dynamicObjectEnumeratorCacheMap.Item(dynamicType, cache);
}

InterruptPoller::InterruptPoller(ThreadContext *tc) :
    threadContext(tc),
    lastPollTick(0),
    lastResetTick(0),
    isDisabled(FALSE)
{TRACE_IT(37506);
    tc->SetInterruptPoller(this);
}

void InterruptPoller::CheckInterruptPoll()
{TRACE_IT(37507);
    if (!isDisabled)
    {TRACE_IT(37508);
        Js::ScriptEntryExitRecord *entryExitRecord = this->threadContext->GetScriptEntryExit();
        if (entryExitRecord)
        {TRACE_IT(37509);
            Js::ScriptContext *scriptContext = entryExitRecord->scriptContext;
            if (scriptContext)
            {TRACE_IT(37510);
                this->TryInterruptPoll(scriptContext);
            }
        }
    }
}


void InterruptPoller::GetStatementCount(ULONG *pluHi, ULONG *pluLo)
{TRACE_IT(37511);
    DWORD resetTick = this->lastResetTick;
    DWORD pollTick = this->lastPollTick;
    DWORD elapsed;

    elapsed = pollTick - resetTick;

    ULONGLONG statements = (ULONGLONG)elapsed * InterruptPoller::TicksToStatements;
    *pluLo = (ULONG)statements;
    *pluHi = (ULONG)(statements >> 32);
}

void ThreadContext::DisableExecution()
{TRACE_IT(37512);
    Assert(TestThreadContextFlag(ThreadContextFlagCanDisableExecution));
    // Hammer the stack limit with a value that will cause script abort on the next stack probe.
    this->SetStackLimitForCurrentThread(Js::Constants::StackLimitForScriptInterrupt);

    return;
}

void ThreadContext::EnableExecution()
{TRACE_IT(37513);
    Assert(this->GetStackProber());
    // Restore the normal stack limit.
    this->SetStackLimitForCurrentThread(this->GetStackProber()->GetScriptStackLimit());

    // It's possible that the host disabled execution after the script threw an exception
    // of it's own, so we shouldn't clear that. Only exceptions for script termination
    // should be cleared.
    if (GetRecordedException() == GetPendingTerminatedErrorObject())
    {TRACE_IT(37514);
        SetRecordedException(NULL);
    }
}

bool ThreadContext::TestThreadContextFlag(ThreadContextFlags contextFlag) const
{TRACE_IT(37515);
    return (this->threadContextFlags & contextFlag) != 0;
}

void ThreadContext::SetThreadContextFlag(ThreadContextFlags contextFlag)
{TRACE_IT(37516);
    this->threadContextFlags = (ThreadContextFlags)(this->threadContextFlags | contextFlag);
}

void ThreadContext::ClearThreadContextFlag(ThreadContextFlags contextFlag)
{TRACE_IT(37517);
    this->threadContextFlags = (ThreadContextFlags)(this->threadContextFlags & ~contextFlag);
}

#ifdef ENABLE_GLOBALIZATION
Js::DelayLoadWinRtString * ThreadContext::GetWinRTStringLibrary()
{TRACE_IT(37518);
    delayLoadWinRtString.EnsureFromSystemDirOnly();

    return &delayLoadWinRtString;
}

#ifdef ENABLE_PROJECTION
Js::DelayLoadWinRtError * ThreadContext::GetWinRTErrorLibrary()
{TRACE_IT(37519);
    delayLoadWinRtError.EnsureFromSystemDirOnly();

    return &delayLoadWinRtError;
}

Js::DelayLoadWinRtTypeResolution* ThreadContext::GetWinRTTypeResolutionLibrary()
{TRACE_IT(37520);
    delayLoadWinRtTypeResolution.EnsureFromSystemDirOnly();

    return &delayLoadWinRtTypeResolution;
}

Js::DelayLoadWinRtRoParameterizedIID* ThreadContext::GetWinRTRoParameterizedIIDLibrary()
{TRACE_IT(37521);
    delayLoadWinRtRoParameterizedIID.EnsureFromSystemDirOnly();

    return &delayLoadWinRtRoParameterizedIID;
}
#endif

#if defined(ENABLE_INTL_OBJECT) || defined(ENABLE_ES6_CHAR_CLASSIFIER)
Js::WindowsGlobalizationAdapter* ThreadContext::GetWindowsGlobalizationAdapter()
{TRACE_IT(37522);
    return &windowsGlobalizationAdapter;
}

Js::DelayLoadWindowsGlobalization* ThreadContext::GetWindowsGlobalizationLibrary()
{TRACE_IT(37523);
    delayLoadWindowsGlobalizationLibrary.Ensure(this->GetWinRTStringLibrary());

    return &delayLoadWindowsGlobalizationLibrary;
}
#endif

#ifdef ENABLE_FOUNDATION_OBJECT
Js::WindowsFoundationAdapter* ThreadContext::GetWindowsFoundationAdapter()
{TRACE_IT(37524);
    return &windowsFoundationAdapter;
}

Js::DelayLoadWinRtFoundation* ThreadContext::GetWinRtFoundationLibrary()
{TRACE_IT(37525);
    delayLoadWinRtFoundationLibrary.EnsureFromSystemDirOnly();

    return &delayLoadWinRtFoundationLibrary;
}
#endif
#endif // ENABLE_GLOBALIZATION

// Despite the name, callers expect this to return the highest propid + 1.

uint ThreadContext::GetHighestPropertyNameIndex() const
{TRACE_IT(37526);
    return propertyMap->GetLastIndex() + 1 + Js::InternalPropertyIds::Count;
}

#if defined(CHECK_MEMORY_LEAK) || defined(LEAK_REPORT)
void ThreadContext::ReportAndCheckLeaksOnProcessDetach()
{TRACE_IT(37527);
    bool needReportOrCheck = false;
#ifdef LEAK_REPORT
    needReportOrCheck = needReportOrCheck || Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag);
#endif
#ifdef CHECK_MEMORY_LEAK
    needReportOrCheck = needReportOrCheck ||
        (Js::Configuration::Global.flags.CheckMemoryLeak && MemoryLeakCheck::IsEnableOutput());
#endif

    if (!needReportOrCheck)
    {TRACE_IT(37528);
        return;
    }

    // Report leaks even if this is a force termination and we have not clean up the thread
    // This is call during process detach. No one should be creating new thread context.
    // So don't need to take the lock
    ThreadContext * current = GetThreadContextList();

    while (current)
    {TRACE_IT(37529);
#if DBG
        current->pageAllocator.ClearConcurrentThreadId();
#endif
        Recycler * recycler = current->GetRecycler();

#ifdef LEAK_REPORT
        if (Js::Configuration::Global.flags.IsEnabled(Js::LeakReportFlag))
        {TRACE_IT(37530);
            AUTO_LEAK_REPORT_SECTION(Js::Configuration::Global.flags, _u("Thread Context (%p): Process Termination (TID: %d)"), current, current->threadId);
            LeakReport::DumpUrl(current->threadId);

            // Heuristically figure out which one is the root tracker script engine
            // and force close on it
            if (current->rootTrackerScriptContext != nullptr)
            {TRACE_IT(37531);
                current->rootTrackerScriptContext->Close(false);
            }
            recycler->ReportLeaksOnProcessDetach();
        }
#endif
#ifdef CHECK_MEMORY_LEAK
        recycler->CheckLeaksOnProcessDetach(_u("Process Termination"));
#endif
        current = current->Next();
    }
}
#endif

#ifdef LEAK_REPORT
void
ThreadContext::SetRootTrackerScriptContext(Js::ScriptContext * scriptContext)
{TRACE_IT(37532);
    Assert(this->rootTrackerScriptContext == nullptr);
    this->rootTrackerScriptContext = scriptContext;
    scriptContext->isRootTrackerScriptContext = true;
}

void
ThreadContext::ClearRootTrackerScriptContext(Js::ScriptContext * scriptContext)
{TRACE_IT(37533);
    Assert(this->rootTrackerScriptContext == scriptContext);
    this->rootTrackerScriptContext->isRootTrackerScriptContext = false;
    this->rootTrackerScriptContext = nullptr;
}
#endif

AutoTagNativeLibraryEntry::AutoTagNativeLibraryEntry(Js::RecyclableObject* function, Js::CallInfo callInfo, PCWSTR name, void* addr)
{TRACE_IT(37534);
    // Save function/callInfo values (for StackWalker). Compiler may stackpack/optimize them for built-in native functions.
    entry.function = function;
    entry.callInfo = callInfo;
    entry.name = name;
    entry.addr = addr;

    ThreadContext* threadContext = function->GetScriptContext()->GetThreadContext();
    threadContext->PushNativeLibraryEntry(&entry);
}

AutoTagNativeLibraryEntry::~AutoTagNativeLibraryEntry()
{TRACE_IT(37535);
    ThreadContext* threadContext = entry.function->GetScriptContext()->GetThreadContext();
    Assert(threadContext->PeekNativeLibraryEntry() == &entry);
    threadContext->PopNativeLibraryEntry();
}
