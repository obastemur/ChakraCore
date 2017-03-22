//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class Func;

struct CodeGenWorkItem : public JsUtil::Job
{
protected:
    CodeGenWorkItem(
        JsUtil::JobManager *const manager,
        Js::FunctionBody *const functionBody,
        Js::EntryPointInfo* entryPointInfo,
        bool isJitInDebugMode,
        CodeGenWorkItemType type);
    ~CodeGenWorkItem();

    CodeGenWorkItemIDL jitData;

    Js::FunctionBody *const functionBody;
    size_t codeAddress;
    ptrdiff_t codeSize;

public:
    virtual uint GetByteCodeCount() const = 0;
    virtual size_t GetDisplayName(_Out_writes_opt_z_(sizeInChars) WCHAR* displayName, _In_ size_t sizeInChars) = 0;
    virtual void GetEntryPointAddress(void** entrypoint, ptrdiff_t *size) = 0;
    virtual uint GetInterpretedCount() const = 0;
    virtual void Delete() = 0;
#if DBG_DUMP | defined(VTUNE_PROFILING)
    virtual void RecordNativeMap(uint32 nativeOffset, uint32 statementIndex) = 0;
#endif
#if DBG_DUMP
    virtual void DumpNativeOffsetMaps() = 0;
    virtual void DumpNativeThrowSpanSequence() = 0;
#endif

    uint GetFunctionNumber() const
    {LOGMEIN("CodeGenWorkItem.h] 40\n");
        return this->functionBody->GetFunctionNumber();
    }

    ExecutionMode GetJitMode() const
    {LOGMEIN("CodeGenWorkItem.h] 45\n");
        return static_cast<ExecutionMode>(this->jitData.jitMode);
    }

    CodeGenWorkItemIDL * GetJITData()
    {LOGMEIN("CodeGenWorkItem.h] 50\n");
        return &this->jitData;
    }

    CodeGenWorkItemType Type() const {LOGMEIN("CodeGenWorkItem.h] 54\n"); return static_cast<CodeGenWorkItemType>(this->jitData.type); }

    Js::ScriptContext* GetScriptContext()
    {LOGMEIN("CodeGenWorkItem.h] 57\n");
        return functionBody->GetScriptContext();
    }

    Js::FunctionBody* GetFunctionBody() const
    {LOGMEIN("CodeGenWorkItem.h] 62\n");
        return functionBody;
    }

    void SetCodeAddress(size_t codeAddress) {LOGMEIN("CodeGenWorkItem.h] 66\n"); this->codeAddress = codeAddress; }
    size_t GetCodeAddress() {LOGMEIN("CodeGenWorkItem.h] 67\n"); return codeAddress; }

    void SetCodeSize(ptrdiff_t codeSize) {LOGMEIN("CodeGenWorkItem.h] 69\n"); this->codeSize = codeSize; }
    ptrdiff_t GetCodeSize() {LOGMEIN("CodeGenWorkItem.h] 70\n"); return codeSize; }

protected:
    virtual uint GetLoopNumber() const
    {LOGMEIN("CodeGenWorkItem.h] 74\n");
        return Js::LoopHeader::NoLoop;
    }

protected:
    // This reference does not keep the entry point alive, and it's not expected to
    // The entry point is kept alive only if it's in the JIT queue, in which case recyclableData will be allocated and will keep the entry point alive
    // If the entry point is getting collected, it'll actually remove itself from the work item list so this work item will get deleted when the EntryPointInfo goes away
    Js::EntryPointInfo* entryPointInfo;
    Js::CodeGenRecyclableData *recyclableData;

private:
    bool isInJitQueue;                  // indicates if the work item has been added to the global jit queue
    bool isAllocationCommitted;         // Whether the EmitBuffer allocation has been committed

    QueuedFullJitWorkItem *queuedFullJitWorkItem;
    EmitBufferAllocation<VirtualAllocWrapper, PreReservedVirtualAllocWrapper> *allocation;

#ifdef IR_VIEWER
public:
    bool isRejitIRViewerFunction;               // re-JIT function for IRViewer object generation
    Js::DynamicObject *irViewerOutput;          // hold results of IRViewer APIs
    Js::ScriptContext *irViewerRequestContext;  // keep track of the request context

    Js::DynamicObject * GetIRViewerOutput(Js::ScriptContext *scriptContext)
    {LOGMEIN("CodeGenWorkItem.h] 99\n");
        if (!irViewerOutput)
        {LOGMEIN("CodeGenWorkItem.h] 101\n");
            irViewerOutput = scriptContext->GetLibrary()->CreateObject();
        }

        return irViewerOutput;
    }

    void SetIRViewerOutput(Js::DynamicObject *output)
    {LOGMEIN("CodeGenWorkItem.h] 109\n");
        irViewerOutput = output;
    }
#endif
private:
    // REVIEW: can we delete this?
    EmitBufferAllocation<VirtualAllocWrapper, PreReservedVirtualAllocWrapper> *GetAllocation() {LOGMEIN("CodeGenWorkItem.h] 115\n"); return allocation; }

public:
    Js::EntryPointInfo* GetEntryPoint() const
    {LOGMEIN("CodeGenWorkItem.h] 119\n");
        return this->entryPointInfo;
    }

    Js::CodeGenRecyclableData *RecyclableData() const
    {LOGMEIN("CodeGenWorkItem.h] 124\n");
        return recyclableData;
    }

    void SetRecyclableData(Js::CodeGenRecyclableData *const recyclableData)
    {LOGMEIN("CodeGenWorkItem.h] 129\n");
        Assert(recyclableData);
        Assert(!this->recyclableData);

        this->recyclableData = recyclableData;
    }

    void SetEntryPointInfo(Js::EntryPointInfo* entryPointInfo)
    {LOGMEIN("CodeGenWorkItem.h] 137\n");
        this->entryPointInfo = entryPointInfo;
    }

public:
    void ResetJitMode()
    {LOGMEIN("CodeGenWorkItem.h] 143\n");
        this->jitData.jitMode = static_cast<uint8>(ExecutionMode::Interpreter);
    }

    void SetJitMode(const ExecutionMode jitMode)
    {LOGMEIN("CodeGenWorkItem.h] 148\n");
        this->jitData.jitMode = static_cast<uint8>(jitMode);
        VerifyJitMode();
    }

    void VerifyJitMode() const
    {LOGMEIN("CodeGenWorkItem.h] 154\n");
        Assert(GetJitMode() == ExecutionMode::SimpleJit || GetJitMode() == ExecutionMode::FullJit);
        Assert(GetJitMode() != ExecutionMode::SimpleJit || GetFunctionBody()->DoSimpleJit());
        Assert(GetJitMode() != ExecutionMode::FullJit || !PHASE_OFF(Js::FullJitPhase, GetFunctionBody()));
    }

    void OnAddToJitQueue();
    void OnRemoveFromJitQueue(NativeCodeGenerator* generator);

public:
    bool ShouldSpeculativelyJit(uint byteCodeSizeGenerated) const;
private:
    bool ShouldSpeculativelyJitBasedOnProfile() const;

public:
    bool IsInJitQueue() const
    {LOGMEIN("CodeGenWorkItem.h] 170\n");
        return isInJitQueue;
    }

    bool IsJitInDebugMode() const
    {LOGMEIN("CodeGenWorkItem.h] 175\n");
        return jitData.isJitInDebugMode != 0;
    }

    void OnWorkItemProcessFail(NativeCodeGenerator *codeGen);

    void RecordNativeThrowMap(Js::SmallSpanSequenceIter& iter, uint32 nativeOffset, uint32 statementIndex)
    {LOGMEIN("CodeGenWorkItem.h] 182\n");
        this->functionBody->RecordNativeThrowMap(iter, nativeOffset, statementIndex, this->GetEntryPoint(), GetLoopNumber());
    }

    QueuedFullJitWorkItem *GetQueuedFullJitWorkItem() const;
    QueuedFullJitWorkItem *EnsureQueuedFullJitWorkItem();

private:
    bool ShouldSpeculativelyJit() const;
};

struct JsFunctionCodeGen sealed : public CodeGenWorkItem
{
    JsFunctionCodeGen(
        JsUtil::JobManager *const manager,
        Js::FunctionBody *const functionBody,
        Js::EntryPointInfo* entryPointInfo,
        bool isJitInDebugMode)
        : CodeGenWorkItem(manager, functionBody, entryPointInfo, isJitInDebugMode, JsFunctionType)
    {LOGMEIN("CodeGenWorkItem.h] 201\n");
        this->jitData.loopNumber = GetLoopNumber();
    }

public:
    uint GetByteCodeCount() const override
    {
        return functionBody->GetByteCodeCount() +  functionBody->GetConstantCount();
    }

    size_t GetDisplayName(_Out_writes_opt_z_(sizeInChars) WCHAR* displayName, _In_ size_t sizeInChars) override
    {
        const WCHAR* name = functionBody->GetExternalDisplayName();
        size_t nameSizeInChars = wcslen(name) + 1;
        size_t sizeInBytes = nameSizeInChars * sizeof(WCHAR);
        if(displayName == NULL || sizeInChars < nameSizeInChars)
        {LOGMEIN("CodeGenWorkItem.h] 217\n");
           return nameSizeInChars;
        }
        js_memcpy_s(displayName, sizeInChars * sizeof(WCHAR), name, sizeInBytes);
        return nameSizeInChars;
    }

    void GetEntryPointAddress(void** entrypoint, ptrdiff_t *size) override
    {
         Assert(entrypoint);
         *entrypoint = (void*)this->GetEntryPoint()->jsMethod;
         *size = this->GetEntryPoint()->GetCodeSize();
    }

    uint GetInterpretedCount() const override
    {
        return this->functionBody->GetInterpretedCount();
    }

    void Delete() override
    {
        HeapDelete(this);
    }

#if DBG_DUMP | defined(VTUNE_PROFILING)
    void RecordNativeMap(uint32 nativeOffset, uint32 statementIndex) override
    {
        Js::FunctionEntryPointInfo* info = (Js::FunctionEntryPointInfo*) this->GetEntryPoint();

        info->RecordNativeMap(nativeOffset, statementIndex);
    }
#endif

#if DBG_DUMP
    virtual void DumpNativeOffsetMaps() override
    {
        this->GetEntryPoint()->DumpNativeOffsetMaps();
    }

    virtual void DumpNativeThrowSpanSequence() override
    {
        this->GetEntryPoint()->DumpNativeThrowSpanSequence();
    }
#endif
};

struct JsLoopBodyCodeGen sealed : public CodeGenWorkItem
{
    JsLoopBodyCodeGen(
        JsUtil::JobManager *const manager, Js::FunctionBody *const functionBody,
        Js::EntryPointInfo* entryPointInfo, bool isJitInDebugMode, Js::LoopHeader * loopHeader) :
        CodeGenWorkItem(manager, functionBody, entryPointInfo, isJitInDebugMode, JsLoopBodyWorkItemType),
        loopHeader(loopHeader)
    {LOGMEIN("CodeGenWorkItem.h] 270\n");
        this->jitData.loopNumber = GetLoopNumber();
    }

    Js::LoopHeader * loopHeader;

    uint GetLoopNumber() const override
    {
        return functionBody->GetLoopNumberWithLock(loopHeader);
    }

    uint GetByteCodeCount() const override
    {
        return (loopHeader->endOffset - loopHeader->startOffset) + functionBody->GetConstantCount();
    }

    size_t GetDisplayName(_Out_writes_opt_z_(sizeInChars) WCHAR* displayName, _In_ size_t sizeInChars) override
    {
        return this->functionBody->GetLoopBodyName(this->GetLoopNumber(), displayName, sizeInChars);
    }

    void GetEntryPointAddress(void** entrypoint, ptrdiff_t *size) override
    {
        Assert(entrypoint);
        Js::EntryPointInfo * entryPoint = this->GetEntryPoint();
        *entrypoint = reinterpret_cast<void*>(entryPoint->jsMethod);
        *size = entryPoint->GetCodeSize();
    }

    uint GetInterpretedCount() const override
    {
        return loopHeader->interpretCount;
    }

#if DBG_DUMP | defined(VTUNE_PROFILING)
    void RecordNativeMap(uint32 nativeOffset, uint32 statementIndex) override
    {
        this->GetEntryPoint()->RecordNativeMap(nativeOffset, statementIndex);
    }
#endif

#if DBG_DUMP
    virtual void DumpNativeOffsetMaps() override
    {
        this->GetEntryPoint()->DumpNativeOffsetMaps();
    }

    virtual void DumpNativeThrowSpanSequence() override
    {
        this->GetEntryPoint()->DumpNativeThrowSpanSequence();
    }
#endif

    void Delete() override
    {
        HeapDelete(this);
    }

    ~JsLoopBodyCodeGen()
    {LOGMEIN("CodeGenWorkItem.h] 329\n");
        if (this->jitData.symIdToValueTypeMap != nullptr)
        {LOGMEIN("CodeGenWorkItem.h] 331\n");
            HeapDeleteArray(this->jitData.symIdToValueTypeMapCount, this->jitData.symIdToValueTypeMap);
            this->jitData.symIdToValueTypeMap = nullptr;
        }
    }
};
