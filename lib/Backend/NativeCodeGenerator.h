//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

struct CodeGenWorkItem;
struct JsFunctionCodeGen;
struct JsLoopBodyCodeGen;
class InliningDecider;
namespace Js
{
    class ObjTypeSpecFldInfo;
    class FunctionCodeGenJitTimeData;
    class RemoteScriptContext;
};

class NativeCodeGenerator sealed : public JsUtil::WaitableJobManager
{
#if ENABLE_DEBUG_CONFIG_OPTIONS
    static volatile UINT_PTR CodegenFailureSeed;
#endif

    friend JsUtil::ForegroundJobProcessor;
    friend JsUtil::BackgroundJobProcessor;
    friend Js::RemoteScriptContext;

public:
    NativeCodeGenerator(Js::ScriptContext * scriptContext);
    ~NativeCodeGenerator();
    void Close();

    JsFunctionCodeGen * NewFunctionCodeGen(Js::FunctionBody *functionBody, Js::EntryPointInfo* info);
    JsLoopBodyCodeGen * NewLoopBodyCodeGen(Js::FunctionBody *functionBody, Js::EntryPointInfo* info, Js::LoopHeader * loopHeader);

    bool GenerateFunction(Js::FunctionBody * fn, Js::ScriptFunction * function = nullptr);
    void GenerateLoopBody(Js::FunctionBody * functionBody, Js::LoopHeader * loopHeader, Js::EntryPointInfo* info = nullptr, uint localCount = 0, Js::Var localSlots[] = nullptr);
    static bool IsValidVar(const Js::Var var, Recycler *const recycler);

#ifdef ENABLE_PREJIT
    void GenerateAllFunctions(Js::FunctionBody * fn);
    bool DoBackEnd(Js::FunctionBody * fn);
#endif

#ifdef IR_VIEWER
    Js::Var RejitIRViewerFunction(Js::FunctionBody *fn, Js::ScriptContext *scriptContext);
#endif
void SetProfileMode(BOOL fSet);
public:
    static Js::Var CheckCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...);

#ifdef ASMJS_PLAT
    static Js::Var CheckAsmJsCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...);
#endif
    static bool IsThunk(Js::JavascriptMethod codeAddress);
    static bool IsAsmJsCodeGenThunk(Js::JavascriptMethod codeAddress);
    static CheckCodeGenFunction GetCheckCodeGenFunction(Js::JavascriptMethod codeAddress);
    static Js::JavascriptMethod CheckCodeGen(Js::ScriptFunction * function);
    static Js::Var CheckAsmJsCodeGen(Js::ScriptFunction * function);

public:
    static void Jit_TransitionFromSimpleJit(void *const framePointer);
private:
    static void TransitionFromSimpleJit(Js::ScriptFunction *const function);

private:
    static Js::JavascriptMethod CheckCodeGenDone(Js::FunctionBody *const functionBody, Js::FunctionEntryPointInfo *const entryPointInfo, Js::ScriptFunction * function);
    CodeGenWorkItem *GetJob(Js::EntryPointInfo *const entryPoint) const;
    bool WasAddedToJobProcessor(JsUtil::Job *const job) const;
    bool ShouldProcessInForeground(const bool willWaitForJob, const unsigned int numJobsInQueue) const;
    void Prioritize(JsUtil::Job *const job, const bool forceAddJobToProcessor = false, void* function = nullptr);
    void PrioritizedButNotYetProcessed(JsUtil::Job *const job);
    void BeforeWaitForJob(Js::EntryPointInfo *const entryPoint) const;
    void AfterWaitForJob(Js::EntryPointInfo *const entryPoint) const;
    static bool WorkItemExceedsJITLimits(CodeGenWorkItem *const codeGenWork);
    virtual bool Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData) override;
    virtual void JobProcessed(JsUtil::Job *const job, const bool succeeded) override;
    JsUtil::Job *GetJobToProcessProactively();
    void AddToJitQueue(CodeGenWorkItem *const codeGenWorkItem, bool prioritize, bool lock, void* function = nullptr);
    void RemoveProactiveJobs();
    void UpdateJITState();
    static void LogCodeGenStart(CodeGenWorkItem * workItem, LARGE_INTEGER * start_time);
    static void LogCodeGenDone(CodeGenWorkItem * workItem, LARGE_INTEGER * start_time);
    typedef SListCounted<Js::ObjTypeSpecFldInfo*, ArenaAllocator> ObjTypeSpecFldInfoList;

    template<bool IsInlinee> void GatherCodeGenData(
        Recycler *const recycler,
        Js::FunctionBody *const topFunctionBody,
        Js::FunctionBody *const functionBody,
        Js::EntryPointInfo *const entryPoint,
        InliningDecider &inliningDecider,
        ObjTypeSpecFldInfoList *objTypeSpecFldInfoList,
        Js::FunctionCodeGenJitTimeData *const jitTimeData,
        Js::FunctionCodeGenRuntimeData *const runtimeData,
        Js::JavascriptFunction* function = nullptr,
        bool isJitTimeDataComputed = false,
        uint32 recursiveInlineDepth = 0);
    Js::CodeGenRecyclableData *GatherCodeGenData(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const functionBody, Js::EntryPointInfo *const entryPoint, CodeGenWorkItem* workItem, void* function = nullptr);

public:
    void UpdateQueueForDebugMode();
    bool IsBackgroundJIT() const;
    void EnterScriptStart();
    void FreeNativeCodeGenAllocation(void* address);
    bool TryReleaseNonHiPriWorkItem(CodeGenWorkItem* workItem);

    void QueueFreeNativeCodeGenAllocation(void* address);

    bool IsClosed() {LOGMEIN("NativeCodeGenerator.h] 108\n"); return isClosed; }
    void AddWorkItem(CodeGenWorkItem* workItem);
    InProcCodeGenAllocators* GetCodeGenAllocator(PageAllocator* pageallocator){LOGMEIN("NativeCodeGenerator.h] 110\n"); return EnsureForegroundAllocators(pageallocator); }

#if DBG_DUMP
    FILE * asmFile;
#endif

#ifdef PROFILE_EXEC
    void CreateProfiler(Js::ScriptContextProfiler * profiler);
    void SetProfilerFromNativeCodeGen(NativeCodeGenerator * nativeCodeGen);
    Js::ScriptContextProfiler *EnsureForegroundCodeGenProfiler();
    static void ProfileBegin(Js::ScriptContextProfiler *const profiler, Js::Phase);
    static void ProfileEnd(Js::ScriptContextProfiler *const profiler, Js::Phase);
    void ProfilePrint();
#endif

private:

    void CodeGen(PageAllocator * pageAllocator, CodeGenWorkItem* workItem, const bool foreground);

    InProcCodeGenAllocators *CreateAllocators(PageAllocator *const pageAllocator)
    {LOGMEIN("NativeCodeGenerator.h] 130\n");
        return HeapNew(InProcCodeGenAllocators, pageAllocator->GetAllocationPolicyManager(), scriptContext, scriptContext->GetThreadContext()->GetCodePageAllocators(), GetCurrentProcess());
    }

    InProcCodeGenAllocators *EnsureForegroundAllocators(PageAllocator * pageAllocator)
    {LOGMEIN("NativeCodeGenerator.h] 135\n");
        if (this->foregroundAllocators == nullptr)
        {LOGMEIN("NativeCodeGenerator.h] 137\n");
            this->foregroundAllocators = CreateAllocators(pageAllocator);

#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
            if (this->scriptContext->webWorkerId != Js::Constants::NonWebWorkerContextId)
            {LOGMEIN("NativeCodeGenerator.h] 142\n");
                this->foregroundAllocators->canCreatePreReservedSegment = true;
            }
#endif
        }

        return this->foregroundAllocators;
    }


    InProcCodeGenAllocators * GetBackgroundAllocator(PageAllocator *pageAllocator)
    {LOGMEIN("NativeCodeGenerator.h] 153\n");
        return this->backgroundAllocators;
    }

    Js::ScriptContextProfiler * GetBackgroundCodeGenProfiler(PageAllocator *allocator);

    void  AllocateBackgroundCodeGenProfiler(PageAllocator * pageAllocator);

    void AllocateBackgroundAllocators(PageAllocator * pageAllocator)
    {LOGMEIN("NativeCodeGenerator.h] 162\n");
        if (!this->backgroundAllocators)
        {LOGMEIN("NativeCodeGenerator.h] 164\n");
            this->backgroundAllocators = CreateAllocators(pageAllocator);
#if !_M_X64_OR_ARM64 && _CONTROL_FLOW_GUARD
            this->backgroundAllocators->canCreatePreReservedSegment = true;
#endif
        }

        AllocateBackgroundCodeGenProfiler(pageAllocator);
    }

    virtual void ProcessorThreadSpecificCallBack(PageAllocator * pageAllocator) override
    {
        AllocateBackgroundAllocators(pageAllocator);
    }

    static ExecutionMode PrejitJitMode(Js::FunctionBody *const functionBody);

    bool TryAggressiveInlining(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const functionBody, InliningDecider &inliningDecider, uint32& inlineeCount, uint recursiveInlineDepth);

private:
    Js::ScriptContext * scriptContext;
    Js::FunctionBody::SetNativeEntryPointFuncType SetNativeEntryPoint;
    uint pendingCodeGenWorkItems;
    JsUtil::DoublyLinkedList<CodeGenWorkItem> workItems;
    JsUtil::DoublyLinkedList<QueuedFullJitWorkItem> queuedFullJitWorkItems;
    uint queuedFullJitWorkItemCount;
    uint byteCodeSizeGenerated;

    bool isOptimizedForManyInstances;
    bool isClosed;
    bool hasUpdatedQForDebugMode;

    class FreeLoopBodyJob: public JsUtil::Job
    {
    public:
        FreeLoopBodyJob(JsUtil::JobManager *const manager, void* address, bool isHeapAllocated = true):
          JsUtil::Job(manager),
          codeAddress(address),
          heapAllocated(isHeapAllocated)
        {LOGMEIN("NativeCodeGenerator.h] 203\n");
        }

        bool heapAllocated;
        void* codeAddress;
    };

    class FreeLoopBodyJobManager sealed: public WaitableJobManager
    {
    public:
        FreeLoopBodyJobManager(JsUtil::JobProcessor* processor)
            : JsUtil::WaitableJobManager(processor)
            , autoClose(true)
            , isClosed(false)
            , stackJobProcessed(false)
#if DBG
            , waitingForStackJob(false)
#endif
        {
            Processor()->AddManager(this);
        }

        virtual ~FreeLoopBodyJobManager()
        {LOGMEIN("NativeCodeGenerator.h] 226\n");
            if (autoClose && !isClosed)
            {LOGMEIN("NativeCodeGenerator.h] 228\n");
                Close();
            }
            Assert(this->isClosed);
        }

        void Close()
        {LOGMEIN("NativeCodeGenerator.h] 235\n");
            Assert(!this->isClosed);
            Processor()->RemoveManager(this);
            this->isClosed = true;
        }

        void SetAutoClose(bool autoClose)
        {LOGMEIN("NativeCodeGenerator.h] 242\n");
            this->autoClose = autoClose;
        }

        FreeLoopBodyJob* GetJob(FreeLoopBodyJob* job)
        {LOGMEIN("NativeCodeGenerator.h] 247\n");
            if (!job->heapAllocated)
            {LOGMEIN("NativeCodeGenerator.h] 249\n");
                return this->stackJobProcessed ? nullptr : job;
            }
            else
            {
                return job;
            }
        }

        bool WasAddedToJobProcessor(JsUtil::Job *const job) const
        {LOGMEIN("NativeCodeGenerator.h] 259\n");
            return true;
        }

        void SetNativeCodeGen(NativeCodeGenerator* nativeCodeGen)
        {LOGMEIN("NativeCodeGenerator.h] 264\n");
            this->nativeCodeGen = nativeCodeGen;
        }

        void BeforeWaitForJob(FreeLoopBodyJob*) const {LOGMEIN("NativeCodeGenerator.h] 268\n");}
        void AfterWaitForJob(FreeLoopBodyJob*) const {LOGMEIN("NativeCodeGenerator.h] 269\n");}

        virtual bool Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData) override
        {
            FreeLoopBodyJob* freeLoopBodyJob = static_cast<FreeLoopBodyJob*>(job);

            // Free Loop Body
            nativeCodeGen->FreeNativeCodeGenAllocation(freeLoopBodyJob->codeAddress);

            return true;
        }

        virtual void JobProcessed(JsUtil::Job *const job, const bool succeeded) override
        {
            FreeLoopBodyJob* freeLoopBodyJob = static_cast<FreeLoopBodyJob*>(job);

            if (freeLoopBodyJob->heapAllocated)
            {LOGMEIN("NativeCodeGenerator.h] 286\n");
                HeapDelete(freeLoopBodyJob);
            }
            else
            {
#if DBG
                Assert(this->waitingForStackJob);
                this->waitingForStackJob = false;
#endif
                this->stackJobProcessed = true;
            }
        }

        void QueueFreeLoopBodyJob(void* codeAddress);

    private:
        NativeCodeGenerator* nativeCodeGen;
        bool autoClose;
        bool isClosed;
        bool stackJobProcessed;
#if DBG
        bool waitingForStackJob;
#endif
    };

    FreeLoopBodyJobManager freeLoopBodyManager;

    InProcCodeGenAllocators * foregroundAllocators;
    InProcCodeGenAllocators * backgroundAllocators;
#ifdef PROFILE_EXEC
    Js::ScriptContextProfiler * foregroundCodeGenProfiler;
    Js::ScriptContextProfiler * backgroundCodeGenProfiler;
#endif

#if DBG
    ThreadContextId mainThreadId;
    friend void CheckIsExecutable(Js::RecyclableObject * function, Js::JavascriptMethod entrypoint);
#endif
};
