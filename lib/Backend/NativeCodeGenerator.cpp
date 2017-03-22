//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"
#include "Base/ScriptContextProfiler.h"

#if DBG
Js::JavascriptMethod checkCodeGenThunk;
#endif

#ifdef ENABLE_PREJIT
#define IS_PREJIT_ON() (Js::Configuration::Global.flags.Prejit)
#else
#define IS_PREJIT_ON() (DEFAULT_CONFIG_Prejit)
#endif

#define ASSERT_THREAD() AssertMsg(mainThreadId == GetCurrentThreadContextId(), \
    "Cannot use this member of native code generator from thread other than the creating context's current thread")

NativeCodeGenerator::NativeCodeGenerator(Js::ScriptContext * scriptContext)
:   JsUtil::WaitableJobManager(scriptContext->GetThreadContext()->GetJobProcessor()),
    scriptContext(scriptContext),
    pendingCodeGenWorkItems(0),
    queuedFullJitWorkItemCount(0),
    foregroundAllocators(nullptr),
    backgroundAllocators(nullptr),
    byteCodeSizeGenerated(0),
    isClosed(false),
    isOptimizedForManyInstances(scriptContext->GetThreadContext()->IsOptimizedForManyInstances()),
    SetNativeEntryPoint(Js::FunctionBody::DefaultSetNativeEntryPoint),
    freeLoopBodyManager(scriptContext->GetThreadContext()->GetJobProcessor()),
    hasUpdatedQForDebugMode(false)
#ifdef PROFILE_EXEC
    , foregroundCodeGenProfiler(nullptr)
    , backgroundCodeGenProfiler(nullptr)
#endif
{
    freeLoopBodyManager.SetNativeCodeGen(this);

#if DBG_DUMP
    if (Js::Configuration::Global.flags.IsEnabled(Js::AsmDumpModeFlag)
        && (Js::Configuration::Global.flags.AsmDumpMode != nullptr))
    {LOGMEIN("NativeCodeGenerator.cpp] 43\n");
        bool fileOpened = false;
        fileOpened = (0 == _wfopen_s(&this->asmFile, Js::Configuration::Global.flags.AsmDumpMode, _u("wt")));
        if (!fileOpened)
        {LOGMEIN("NativeCodeGenerator.cpp] 47\n");
            size_t len = wcslen(Js::Configuration::Global.flags.AsmDumpMode);
            if (len < _MAX_PATH - 5)
            {LOGMEIN("NativeCodeGenerator.cpp] 50\n");
                char16 filename[_MAX_PATH];
                wcscpy_s(filename, _MAX_PATH, Js::Configuration::Global.flags.AsmDumpMode);
                char16 * number = filename + len;
                for (int i = 0; i < 1000; i++)
                {
                    _itow_s(i, number, 5, 10);
                    fileOpened = (0 == _wfopen_s(&this->asmFile, filename, _u("wt")));
                    if (fileOpened)
                    {LOGMEIN("NativeCodeGenerator.cpp] 59\n");
                        break;
                    }
                }
            }

            if (!fileOpened)
            {LOGMEIN("NativeCodeGenerator.cpp] 66\n");
                this->asmFile = nullptr;
                AssertMsg(0, "Could not open file for AsmDump. The output will goto standard console");
            }
        }

    }
    else
    {
        this->asmFile = nullptr;
    }
#endif

#if DBG
    this->mainThreadId = GetCurrentThreadContextId();
#endif

    Processor()->AddManager(this);
    this->freeLoopBodyManager.SetAutoClose(false);
}

NativeCodeGenerator::~NativeCodeGenerator()
{LOGMEIN("NativeCodeGenerator.cpp] 88\n");
    Assert(this->IsClosed());

#ifdef PROFILE_EXEC
    if (this->foregroundCodeGenProfiler != nullptr)
    {LOGMEIN("NativeCodeGenerator.cpp] 93\n");
        this->foregroundCodeGenProfiler->Release();
    }
#endif

    if (scriptContext->GetJitFuncRangeCache() != nullptr)
    {LOGMEIN("NativeCodeGenerator.cpp] 99\n");
        scriptContext->GetJitFuncRangeCache()->ClearCache();
    }

    if(this->foregroundAllocators != nullptr)
    {LOGMEIN("NativeCodeGenerator.cpp] 104\n");
        HeapDelete(this->foregroundAllocators);
    }


    if (this->backgroundAllocators)
    {LOGMEIN("NativeCodeGenerator.cpp] 110\n");
#if DBG
        // PageAllocator is thread agile. This destructor can be called from background GC thread.
        // We have already removed this manager from the job queue and hence its fine to set the threadId to -1.
        // We can't DissociatePageAllocator here as its allocated ui thread.
        //this->Processor()->DissociatePageAllocator(allocator->GetPageAllocator());
        this->backgroundAllocators->ClearConcurrentThreadId();
#endif
        // The native code generator may be deleted after Close was called on the job processor. In that case, the
        // background thread is no longer running, so clean things up in the foreground.
        HeapDelete(this->backgroundAllocators);
    }

#ifdef PROFILE_EXEC
    if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
    {LOGMEIN("NativeCodeGenerator.cpp] 125\n");
        while (this->backgroundCodeGenProfiler)
        {LOGMEIN("NativeCodeGenerator.cpp] 127\n");
            Js::ScriptContextProfiler *codegenProfiler = this->backgroundCodeGenProfiler;
            this->backgroundCodeGenProfiler = this->backgroundCodeGenProfiler->next;
            // background codegen profiler is allocated in background thread,
            // clear the thead Id before release
#ifdef DBG
            if (codegenProfiler->pageAllocator != nullptr)
            {LOGMEIN("NativeCodeGenerator.cpp] 134\n");
                codegenProfiler->pageAllocator->SetDisableThreadAccessCheck();
            }
#endif
            codegenProfiler->Release();
        }
    }
    else
    {
        Assert(this->backgroundCodeGenProfiler == nullptr);
    }
#endif

}

void NativeCodeGenerator::Close()
{LOGMEIN("NativeCodeGenerator.cpp] 150\n");
    Assert(!this->IsClosed());

    // Close FreeLoopBodyJobManager first, as it depends on NativeCodeGenerator to be open before it's removed
    this->freeLoopBodyManager.Close();

    // Remove only if it is not updated in the debug mode (and which goes to interpreter mode).
    if (!hasUpdatedQForDebugMode || Js::Configuration::Global.EnableJitInDebugMode())
    {LOGMEIN("NativeCodeGenerator.cpp] 158\n");
        Processor()->RemoveManager(this);
    }

    this->isClosed = true;

    Assert(!queuedFullJitWorkItems.Head());
    Assert(queuedFullJitWorkItemCount == 0);

    for(JsUtil::Job *job = workItems.Head(); job;)
    {LOGMEIN("NativeCodeGenerator.cpp] 168\n");
        JsUtil::Job *const next = job->Next();
        JobProcessed(job, /*succeeded*/ false);
        job = next;
    }
    workItems.Clear();

    // Only decommit here instead of releasing the memory, so we retain control over these addresses
    // Mitigate against the case the entry point is called after the script site is closed
    if (this->backgroundAllocators)
    {LOGMEIN("NativeCodeGenerator.cpp] 178\n");
        this->backgroundAllocators->emitBufferManager.Decommit();
    }

    if (this->foregroundAllocators)
    {LOGMEIN("NativeCodeGenerator.cpp] 183\n");
        this->foregroundAllocators->emitBufferManager.Decommit();
    }

#if DBG_DUMP
    if (this->asmFile != nullptr)
    {LOGMEIN("NativeCodeGenerator.cpp] 189\n");
        if(0 != fclose(this->asmFile))
        {
            AssertMsg(0, "Could not close file for AsmDump. You may ignore this warning.");
        }
    }
#endif
}

#if DBG_DUMP
extern Func *CurrentFunc;
#endif

JsFunctionCodeGen *
NativeCodeGenerator::NewFunctionCodeGen(Js::FunctionBody *functionBody, Js::EntryPointInfo* info)
{LOGMEIN("NativeCodeGenerator.cpp] 204\n");
    return HeapNewNoThrow(JsFunctionCodeGen, this, functionBody, info, functionBody->IsInDebugMode());
}

JsLoopBodyCodeGen *
NativeCodeGenerator::NewLoopBodyCodeGen(Js::FunctionBody *functionBody, Js::EntryPointInfo* info, Js::LoopHeader * loopHeader)
{LOGMEIN("NativeCodeGenerator.cpp] 210\n");
    return HeapNewNoThrow(JsLoopBodyCodeGen, this, functionBody, info, functionBody->IsInDebugMode(), loopHeader);
}

#ifdef ENABLE_PREJIT
bool
NativeCodeGenerator::DoBackEnd(Js::FunctionBody *fn)
{LOGMEIN("NativeCodeGenerator.cpp] 217\n");
    return (
        !PHASE_OFF(Js::BackEndPhase, fn)
        && !fn->IsGeneratorAndJitIsDisabled()
#ifdef ASMJS_PLAT
        && !fn->IsAsmJSModule()
#endif
    );
}

void
NativeCodeGenerator::GenerateAllFunctions(Js::FunctionBody * fn)
{LOGMEIN("NativeCodeGenerator.cpp] 229\n");
    Assert(IS_PREJIT_ON());
    Assert(fn->GetDefaultFunctionEntryPointInfo()->entryPointIndex == 0);

    // Make sure this isn't a deferred function
    Assert(fn->GetFunctionBody() == fn);
    Assert(!fn->IsDeferred());

    if (DoBackEnd(fn))
    {LOGMEIN("NativeCodeGenerator.cpp] 238\n");
        if (fn->GetLoopCount() != 0 && fn->ForceJITLoopBody() && !fn->IsInDebugMode())
        {LOGMEIN("NativeCodeGenerator.cpp] 240\n");
            // Only jit the loop body with /force:JITLoopBody

            for (uint i = 0; i < fn->GetLoopCount(); i++)
            {LOGMEIN("NativeCodeGenerator.cpp] 244\n");
                Js::LoopHeader * loopHeader = fn->GetLoopHeader(i);
                Js::EntryPointInfo * entryPointInfo = loopHeader->GetCurrentEntryPointInfo();
                this->GenerateLoopBody(fn, loopHeader, entryPointInfo);
            }
        }
        else
        {
            // A JIT attempt should have already been made through GenerateFunction
            Assert(!fn->GetDefaultFunctionEntryPointInfo()->IsNotScheduled());
        }
    }
    for (uint i = 0; i < fn->GetNestedCount(); i++)
    {LOGMEIN("NativeCodeGenerator.cpp] 257\n");
        Js::FunctionBody* functionToJIT = fn->GetNestedFunctionForExecution(i)->GetFunctionBody();
        GenerateAllFunctions(functionToJIT);
    }
}
#endif

#if _M_ARM
USHORT ArmExtractThumbImmediate16(PUSHORT address)
{LOGMEIN("NativeCodeGenerator.cpp] 266\n");
    return ((address[0] << 12) & 0xf000) |  // bits[15:12] in OP0[3:0]
           ((address[0] <<  1) & 0x0800) |  // bits[11]    in OP0[10]
           ((address[1] >>  4) & 0x0700) |  // bits[10:8]  in OP1[14:12]
           ((address[1] >>  0) & 0x00ff);   // bits[7:0]   in OP1[7:0]
}

void ArmInsertThumbImmediate16(PUSHORT address, USHORT immediate)
{LOGMEIN("NativeCodeGenerator.cpp] 274\n");
    USHORT opcode0;
    USHORT opcode1;

    opcode0 = address[0];
    opcode1 = address[1];
    opcode0 &= ~((0xf000 >> 12) | (0x0800 >> 1));
    opcode1 &= ~((0x0700 <<  4) | (0x00ff << 0));
    opcode0 |= (immediate & 0xf000) >> 12;   // bits[15:12] in OP0[3:0]
    opcode0 |= (immediate & 0x0800) >>  1;   // bits[11]    in OP0[10]
    opcode1 |= (immediate & 0x0700) <<  4;   // bits[10:8]  in OP1[14:12]
    opcode1 |= (immediate & 0x00ff) <<  0;   // bits[7:0]   in OP1[7:0]
    address[0] = opcode0;
    address[1] = opcode1;
}
#endif

void DoFunctionRelocations(BYTE *function, DWORD functionOffset, DWORD functionSize, BYTE *module, size_t imageBase, IMAGE_SECTION_HEADER *textHeader, IMAGE_SECTION_HEADER *relocHeader)
{LOGMEIN("NativeCodeGenerator.cpp] 292\n");
    PIMAGE_BASE_RELOCATION relocationBlock = (PIMAGE_BASE_RELOCATION)(module + relocHeader->PointerToRawData);
    for (; relocationBlock->VirtualAddress > 0 && ((BYTE *)relocationBlock < (module + relocHeader->PointerToRawData + relocHeader->SizeOfRawData)); )
    {LOGMEIN("NativeCodeGenerator.cpp] 295\n");
        DWORD blockOffset = relocationBlock->VirtualAddress - textHeader->VirtualAddress;

        // Skip relocation blocks that are before the function
        if ((blockOffset + 0x1000) > functionOffset)
        {LOGMEIN("NativeCodeGenerator.cpp] 300\n");
            unsigned short *relocation = (unsigned short *)((unsigned char *)relocationBlock + sizeof(IMAGE_BASE_RELOCATION));
            for (uint index = 0; index < ((relocationBlock->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2); index++, relocation++)
            {LOGMEIN("NativeCodeGenerator.cpp] 303\n");
                int type = *relocation >> 12;
                int offset = *relocation & 0xfff;

                // If we are past the end of the function, we can stop.
                if ((blockOffset + offset) >= (functionOffset + functionSize))
                {LOGMEIN("NativeCodeGenerator.cpp] 309\n");
                    break;
                }

                if ((blockOffset + offset) < functionOffset)
                {LOGMEIN("NativeCodeGenerator.cpp] 314\n");
                    continue;
                }

                switch (type)
                {LOGMEIN("NativeCodeGenerator.cpp] 319\n");
                case IMAGE_REL_BASED_ABSOLUTE:
                    break;

#if _M_IX86
                case IMAGE_REL_BASED_HIGHLOW:
                    {LOGMEIN("NativeCodeGenerator.cpp] 325\n");
                        DWORD *patchAddrHL = (DWORD *) (function + blockOffset + offset - functionOffset);
                        DWORD patchAddrHLOffset = *patchAddrHL - imageBase - textHeader->VirtualAddress;
                        Assert((patchAddrHLOffset > functionOffset) && (patchAddrHLOffset < (functionOffset + functionSize)));
                        *patchAddrHL = patchAddrHLOffset - functionOffset + (DWORD)function;
                    }
                    break;

#elif defined(_M_X64_OR_ARM64)
                case IMAGE_REL_BASED_DIR64:
                    {LOGMEIN("NativeCodeGenerator.cpp] 335\n");
                        ULONGLONG *patchAddr64 = (ULONGLONG *) (function + blockOffset + offset - functionOffset);
                        ULONGLONG patchAddr64Offset = *patchAddr64 - imageBase - textHeader->VirtualAddress;
                        Assert((patchAddr64Offset > functionOffset) && (patchAddr64Offset < (functionOffset + functionSize)));
                        *patchAddr64 = patchAddr64Offset - functionOffset + (ULONGLONG)function;
                    }
                    break;
#else
                case IMAGE_REL_BASED_THUMB_MOV32:
                    {LOGMEIN("NativeCodeGenerator.cpp] 344\n");
                        USHORT *patchAddr = (USHORT *) (function + blockOffset + offset - functionOffset);
                        DWORD address = ArmExtractThumbImmediate16(patchAddr) | (ArmExtractThumbImmediate16(patchAddr + 2) << 16);
                        address = address - imageBase - textHeader->VirtualAddress - functionOffset + (DWORD)function;
                        ArmInsertThumbImmediate16(patchAddr, (USHORT)(address & 0xFFFF));
                        ArmInsertThumbImmediate16(patchAddr + 2, (USHORT)(address >> 16));
                    }
                    break;
#endif

                default:
                    Assert(false);
                    break;
                }
            }
        }

        relocationBlock = (PIMAGE_BASE_RELOCATION) (((BYTE *) relocationBlock) + relocationBlock->SizeOfBlock);
    }
}

class AutoRestoreDefaultEntryPoint
{
public:
    AutoRestoreDefaultEntryPoint(Js::FunctionBody* functionBody):
        functionBody(functionBody)
    {LOGMEIN("NativeCodeGenerator.cpp] 370\n");
        this->oldDefaultEntryPoint = functionBody->GetDefaultFunctionEntryPointInfo();
        this->oldOriginalEntryPoint = functionBody->GetOriginalEntryPoint();

        this->newEntryPoint = functionBody->CreateNewDefaultEntryPoint();
    }

    ~AutoRestoreDefaultEntryPoint()
    {LOGMEIN("NativeCodeGenerator.cpp] 378\n");
        if (newEntryPoint && !newEntryPoint->IsCodeGenDone())
        {LOGMEIN("NativeCodeGenerator.cpp] 380\n");
            functionBody->RestoreOldDefaultEntryPoint(oldDefaultEntryPoint, oldOriginalEntryPoint, newEntryPoint);
        }
    }

private:
    Js::FunctionBody* functionBody;
    Js::FunctionEntryPointInfo* oldDefaultEntryPoint;
    Js::JavascriptMethod oldOriginalEntryPoint;
    Js::FunctionEntryPointInfo* newEntryPoint;
};

//static
void NativeCodeGenerator::Jit_TransitionFromSimpleJit(void *const framePointer)
{LOGMEIN("NativeCodeGenerator.cpp] 394\n");
    TransitionFromSimpleJit(
        Js::ScriptFunction::FromVar(Js::JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject));
}

//static
void NativeCodeGenerator::TransitionFromSimpleJit(Js::ScriptFunction *const function)
{LOGMEIN("NativeCodeGenerator.cpp] 401\n");
    Assert(function);

    Js::FunctionBody *const functionBody = function->GetFunctionBody();
    Js::FunctionEntryPointInfo *const defaultEntryPointInfo = functionBody->GetDefaultFunctionEntryPointInfo();
    if(defaultEntryPointInfo == functionBody->GetSimpleJitEntryPointInfo())
    {LOGMEIN("NativeCodeGenerator.cpp] 407\n");
        Assert(functionBody->GetExecutionMode() == ExecutionMode::SimpleJit);
        Assert(function->GetFunctionEntryPointInfo() == defaultEntryPointInfo);

        // The latest entry point is the simple JIT, transition to the next execution mode and schedule a full JIT
        bool functionEntryPointUpdated = functionBody->GetScriptContext()->GetNativeCodeGenerator()->GenerateFunction(functionBody, function);

        if (functionEntryPointUpdated)
        {LOGMEIN("NativeCodeGenerator.cpp] 415\n");
            // Transition to the next execution mode after scheduling a full JIT, in case of OOM before the entry point is changed
            const bool transitioned = functionBody->TryTransitionToNextExecutionMode();
            Assert(transitioned);

            if (PHASE_TRACE(Js::SimpleJitPhase, functionBody))
            {LOGMEIN("NativeCodeGenerator.cpp] 421\n");
                char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                Output::Print(
                    _u("SimpleJit (TransitionFromSimpleJit): function: %s (%s)"),
                    functionBody->GetDisplayName(),
                    functionBody->GetDebugNumberSet(debugStringBuffer));
                Output::Flush();
            }
        }
        return;
    }

    if(function->GetFunctionEntryPointInfo() != defaultEntryPointInfo)
    {LOGMEIN("NativeCodeGenerator.cpp] 434\n");
        // A full JIT may have already been scheduled, or some entry point info got expired before the simple JIT entry point
        // was ready. In any case, the function's entry point info is not the latest, so update it.
        function->UpdateThunkEntryPoint(defaultEntryPointInfo, functionBody->GetDirectEntryPoint(defaultEntryPointInfo));
    }
}

#ifdef IR_VIEWER
Js::Var
NativeCodeGenerator::RejitIRViewerFunction(Js::FunctionBody *fn, Js::ScriptContext *requestContext)
{LOGMEIN("NativeCodeGenerator.cpp] 444\n");
    /* Note: adapted from NativeCodeGenerator::GenerateFunction (NativeCodeGenerator.cpp) */

    Js::ScriptContext *scriptContext = fn->GetScriptContext();
    PageAllocator *pageAllocator = scriptContext->GetThreadContext()->GetPageAllocator();
    NativeCodeGenerator *nativeCodeGenerator = scriptContext->GetNativeCodeGenerator();

    AutoRestoreDefaultEntryPoint autoRestore(fn);
    Js::FunctionEntryPointInfo * entryPoint = fn->GetDefaultFunctionEntryPointInfo();

    JsFunctionCodeGen workitem(this, fn, entryPoint, fn->IsInDebugMode());
    workitem.isRejitIRViewerFunction = true;
    workitem.irViewerRequestContext = scriptContext;

    workitem.SetJitMode(ExecutionMode::FullJit);

    entryPoint->SetCodeGenPendingWithStackAllocatedWorkItem();
    entryPoint->SetCodeGenQueued();
    const auto recyclableData = GatherCodeGenData(fn, fn, entryPoint, &workitem);
    workitem.SetRecyclableData(recyclableData);

    nativeCodeGenerator->CodeGen(pageAllocator, &workitem, true);

    return Js::CrossSite::MarshalVar(requestContext, workitem.GetIRViewerOutput(scriptContext));
}
#endif /* IR_VIEWER */

///----------------------------------------------------------------------------
///
/// NativeCodeGenerator::GenerateFunction
///
///     This is the main entry point for the runtime to call the native code
///     generator.
///
///----------------------------------------------------------------------------
bool
NativeCodeGenerator::GenerateFunction(Js::FunctionBody *fn, Js::ScriptFunction * function)
{LOGMEIN("NativeCodeGenerator.cpp] 481\n");
    ASSERT_THREAD();
    Assert(!fn->GetIsFromNativeCodeModule());
    Assert(fn->GetScriptContext()->GetNativeCodeGenerator() == this);
    Assert(fn->GetFunctionBody() == fn);
    Assert(!fn->IsDeferred());

#if !defined(_M_ARM64)

    if (fn->IsGeneratorAndJitIsDisabled())
    {LOGMEIN("NativeCodeGenerator.cpp] 491\n");
        // JITing generator functions is not complete nor stable yet so it is off by default.
        // Also try/catch JIT support in generator functions is not a goal for threshold
        // release so JITing generators containing try blocks is disabled for now.
        return false;
    }

    if (fn->IsInDebugMode() && fn->GetHasTry())
    {LOGMEIN("NativeCodeGenerator.cpp] 499\n");
        // Under debug mode disable JIT for functions that:
        // - have try
        return false;
    }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    if (Js::Configuration::Global.flags.Interpret &&
        fn->GetDisplayName() &&
        ::wcsstr(Js::Configuration::Global.flags.Interpret, fn->GetDisplayName()))
    {LOGMEIN("NativeCodeGenerator.cpp] 509\n");
        return false;
    }
#endif

    if (fn->GetLoopCount() != 0 && fn->ForceJITLoopBody() && !fn->IsInDebugMode())
    {LOGMEIN("NativeCodeGenerator.cpp] 515\n");
        // Don't code gen the function if the function has loop, ForceJITLoopBody is on,
        // unless we are in debug mode in which case JIT loop body is disabled, even if it's forced.
        return false;
    }

    // Create a work item with null entry point- we'll set it once its allocated
    AutoPtr<JsFunctionCodeGen> workItemAutoPtr(this->NewFunctionCodeGen(fn, nullptr));
    if ((JsFunctionCodeGen*) workItemAutoPtr == nullptr)
    {LOGMEIN("NativeCodeGenerator.cpp] 524\n");
        // OOM, just skip this work item and return.
        return false;
    }

    Js::FunctionEntryPointInfo* entryPointInfo = nullptr;

    if (function != nullptr)
    {LOGMEIN("NativeCodeGenerator.cpp] 532\n");
        entryPointInfo = fn->CreateNewDefaultEntryPoint();
    }
    else
    {
        entryPointInfo = fn->GetDefaultFunctionEntryPointInfo();
        Assert(fn->IsInterpreterThunk() || fn->IsSimpleJitOriginalEntryPoint());
    }
    bool doPreJit = IS_PREJIT_ON();
#ifdef ASMJS_PLAT
    if (fn->GetIsAsmjsMode())
    {LOGMEIN("NativeCodeGenerator.cpp] 543\n");
        AnalysisAssert(function != nullptr);
        Js::FunctionEntryPointInfo* oldFuncObjEntryPointInfo = (Js::FunctionEntryPointInfo*)function->GetEntryPointInfo();
        Assert(oldFuncObjEntryPointInfo->GetIsAsmJSFunction()); // should be asmjs entrypoint info

        // Set asmjs to be true in entrypoint
        entryPointInfo->SetIsAsmJSFunction(true);

        // Update the native address of the older entry point - this should be either the TJ entrypoint or the Interpreter Entry point
        entryPointInfo->SetNativeAddress(oldFuncObjEntryPointInfo->jsMethod);
        // have a reference to TJ entrypointInfo, this will be queued for collection in checkcodegen
        entryPointInfo->SetOldFunctionEntryPointInfo(oldFuncObjEntryPointInfo);
        Assert(PHASE_ON1(Js::AsmJsJITTemplatePhase) || (!oldFuncObjEntryPointInfo->GetIsTJMode() && !entryPointInfo->GetIsTJMode()));
        // this changes the address in the entrypointinfo to be the AsmJsCodgenThunk
        function->UpdateThunkEntryPoint(entryPointInfo, NativeCodeGenerator::CheckAsmJsCodeGenThunk);

        if (PHASE_TRACE1(Js::AsmjsEntryPointInfoPhase))
            Output::Print(_u("New Entrypoint is CheckAsmJsCodeGenThunk for function: %s\n"), fn->GetDisplayName());
        doPreJit |= CONFIG_FLAG(MaxAsmJsInterpreterRunCount) == 0 || CONFIG_ISENABLED(Js::ForceNativeFlag);
    }
    else
#endif
    {
        fn->SetCheckCodeGenEntryPoint(entryPointInfo, NativeCodeGenerator::CheckCodeGenThunk);
        if (function != nullptr)
        {LOGMEIN("NativeCodeGenerator.cpp] 568\n");
            function->UpdateThunkEntryPoint(entryPointInfo, NativeCodeGenerator::CheckCodeGenThunk);
        }
    }

    JsFunctionCodeGen * workitem = workItemAutoPtr.Detach();
    workitem->SetEntryPointInfo(entryPointInfo);

    entryPointInfo->SetCodeGenPending(workitem);
    InterlockedIncrement(&pendingCodeGenWorkItems);

    if(!doPreJit)
    {LOGMEIN("NativeCodeGenerator.cpp] 580\n");
        workItems.LinkToEnd(workitem);
        return true;
    }

    const ExecutionMode prejitJitMode = PrejitJitMode(fn);
    workitem->SetJitMode(prejitJitMode);
    try
    {
        AddToJitQueue(workitem, /*prioritize*/ true, /*lock*/ true, function);
    }
    catch (...)
    {LOGMEIN("NativeCodeGenerator.cpp] 592\n");
        // Add the item back to the list if AddToJitQueue throws. The position in the list is not important.
        workitem->ResetJitMode();
        workItems.LinkToEnd(workitem);
        throw;
    }
    fn->TraceExecutionMode("Prejit (before)");
    if(prejitJitMode == ExecutionMode::SimpleJit)
    {LOGMEIN("NativeCodeGenerator.cpp] 600\n");
        fn->TransitionToSimpleJitExecutionMode();
    }
    else
    {
        Assert(prejitJitMode == ExecutionMode::FullJit);
        fn->TransitionToFullJitExecutionMode();
    }
    fn->TraceExecutionMode("Prejit");
    Processor()->PrioritizeJobAndWait(this, entryPointInfo, function);
    CheckCodeGenDone(fn, entryPointInfo, function);
    return true;
#else
    return false;
#endif
}

void NativeCodeGenerator::GenerateLoopBody(Js::FunctionBody * fn, Js::LoopHeader * loopHeader, Js::EntryPointInfo* entryPoint, uint localCount, Js::Var localSlots[])
{LOGMEIN("NativeCodeGenerator.cpp] 618\n");
    ASSERT_THREAD();
    Assert(fn->GetScriptContext()->GetNativeCodeGenerator() == this);
    Assert(entryPoint->jsMethod == nullptr);

#if DBG_DUMP
    if (PHASE_TRACE1(Js::JITLoopBodyPhase))
    {LOGMEIN("NativeCodeGenerator.cpp] 625\n");
        fn->DumpFunctionId(true);
        Output::Print(_u(": %-20s LoopBody Start    Loop: %2d ByteCode: %4d (%4d,%4d)\n"), fn->GetDisplayName(), fn->GetLoopNumber(loopHeader),
            loopHeader->endOffset - loopHeader->startOffset, loopHeader->startOffset, loopHeader->endOffset);
        Output::Flush();
    }
#endif

    // If the parent function is JITted, no need to JIT this loop
    // CanReleaseLoopHeaders is a quick and dirty way of checking if the
    // function is currently being interpreted. If it is being interpreted,
    // We'd still like to jit the loop body.
    // We reset the interpretCount to 0 in case we switch back to the interpreter
    if (fn->GetNativeEntryPointUsed() && fn->GetCanReleaseLoopHeaders()
#ifdef ASMJS_PLAT
        && (!fn->GetIsAsmJsFunction() || !(loopHeader->GetCurrentEntryPointInfo()->GetIsTJMode()))
#endif
    )
    {LOGMEIN("NativeCodeGenerator.cpp] 643\n");
        loopHeader->ResetInterpreterCount();
        return;
    }
#ifdef ASMJS_PLAT
    if (fn->GetIsAsmJsFunction())
    {LOGMEIN("NativeCodeGenerator.cpp] 649\n");
        Js::LoopEntryPointInfo* loopEntryPointInfo = (Js::LoopEntryPointInfo*)entryPoint;
        loopEntryPointInfo->SetIsAsmJSFunction(true);
    }
#endif
    JsLoopBodyCodeGen * workitem = this->NewLoopBodyCodeGen(fn, entryPoint, loopHeader);
    if (!workitem)
    {LOGMEIN("NativeCodeGenerator.cpp] 656\n");
        // OOM, just skip this work item and return.
        return;
    }

    entryPoint->SetCodeGenPending(workitem);

    try
    {LOGMEIN("NativeCodeGenerator.cpp] 664\n");
        if (!fn->GetIsAsmJsFunction()) // not needed for asmjs as we don't profile in asm mode
        {LOGMEIN("NativeCodeGenerator.cpp] 666\n");
            const uint profiledRegBegin = fn->GetConstantCount();
            const uint profiledRegEnd = localCount;
            if (profiledRegBegin < profiledRegEnd)
            {LOGMEIN("NativeCodeGenerator.cpp] 670\n");
                workitem->GetJITData()->symIdToValueTypeMapCount = profiledRegEnd - profiledRegBegin;
                workitem->GetJITData()->symIdToValueTypeMap = (uint16*)HeapNewArrayZ(ValueType, workitem->GetJITData()->symIdToValueTypeMapCount);
                Recycler *recycler = fn->GetScriptContext()->GetRecycler();
                for (uint i = profiledRegBegin; i < profiledRegEnd; i++)
                {LOGMEIN("NativeCodeGenerator.cpp] 675\n");
                    if (localSlots[i] && IsValidVar(localSlots[i], recycler))
                    {LOGMEIN("NativeCodeGenerator.cpp] 677\n");
                        workitem->GetJITData()->symIdToValueTypeMap[i - profiledRegBegin] = ValueType::Uninitialized.Merge(localSlots[i]).GetRawData();
                    }
                }
            }
        }

        workitem->SetJitMode(ExecutionMode::FullJit);
        AddToJitQueue(workitem, /*prioritize*/ true, /*lock*/ true);
    }
    catch (...)
    {LOGMEIN("NativeCodeGenerator.cpp] 688\n");
        // If adding to the JIT queue fails we need to revert the state of the entry point
        // and delete the work item
        entryPoint->RevertToNotScheduled();
        workitem->Delete();

        throw;
    }

    if (!Processor()->ProcessesInBackground() || fn->ForceJITLoopBody())
    {
        Processor()->PrioritizeJobAndWait(this, entryPoint);
    }
}

bool
NativeCodeGenerator::IsValidVar(const Js::Var var, Recycler *const recycler)
{LOGMEIN("NativeCodeGenerator.cpp] 705\n");
    using namespace Js;
    Assert(var);
    Assert(recycler);

    // We may be handling uninitialized memory here, need to ensure that each recycler-allocated object is valid before it is
    // read. Virtual functions shouldn't be called because the type ID may match by coincidence but the vtable can still be
    // invalid, even if it is deemed to be a "valid" object, since that only validates that the memory is still owned by the
    // recycler. This function validates the memory that ValueType::Merge(Var) reads.

    if(TaggedInt::Is(var))
    {LOGMEIN("NativeCodeGenerator.cpp] 716\n");
        return true;
    }

#if FLOATVAR
    if(JavascriptNumber::Is_NoTaggedIntCheck(var))
    {LOGMEIN("NativeCodeGenerator.cpp] 722\n");
        return true;
    }
#endif

    RecyclableObject *const recyclableObject = RecyclableObject::FromVar(var);
    if(!recycler->IsValidObject(recyclableObject, sizeof(*recyclableObject)))
    {LOGMEIN("NativeCodeGenerator.cpp] 729\n");
        return false;
    }

    INT_PTR vtable = VirtualTableInfoBase::GetVirtualTable(var);
    if (vtable <= USHRT_MAX || (vtable & 1))
    {LOGMEIN("NativeCodeGenerator.cpp] 735\n");
        // Don't have a vtable, is it not a var, may be a frame display?
        return false;
    }

    Type *const type = recyclableObject->GetType();
    if(!recycler->IsValidObject(type, sizeof(*type)))
    {LOGMEIN("NativeCodeGenerator.cpp] 742\n");
        return false;
    }

#if !FLOATVAR
    if(JavascriptNumber::Is_NoTaggedIntCheck(var))
    {LOGMEIN("NativeCodeGenerator.cpp] 748\n");
        return true;
    }
#endif

    const TypeId typeId = type->GetTypeId();
    if(typeId < static_cast<TypeId>(0))
    {LOGMEIN("NativeCodeGenerator.cpp] 755\n");
        return false;
    }
    if(!DynamicType::Is(typeId))
    {LOGMEIN("NativeCodeGenerator.cpp] 759\n");
        return true;
    }

    DynamicType *const dynamicType = static_cast<DynamicType *>(type);
    if(!recycler->IsValidObject(dynamicType, sizeof(*dynamicType)))
    {LOGMEIN("NativeCodeGenerator.cpp] 765\n");
        return false;
    }

    DynamicTypeHandler *const typeHandler = dynamicType->GetTypeHandler();
    if(!recycler->IsValidObject(typeHandler, sizeof(*typeHandler)))
    {LOGMEIN("NativeCodeGenerator.cpp] 771\n");
        return false;
    }

    // Not using DynamicObject::FromVar since there's a virtual call in there
    DynamicObject *const object = static_cast<DynamicObject *>(recyclableObject);
    if(!recycler->IsValidObject(object, sizeof(*object)))
    {LOGMEIN("NativeCodeGenerator.cpp] 778\n");
        return false;
    }

    if(typeId != TypeIds_Array)
    {LOGMEIN("NativeCodeGenerator.cpp] 783\n");
        ArrayObject* const objectArray = object->GetObjectArrayUnchecked();
        return objectArray == nullptr || recycler->IsValidObject(objectArray, sizeof(*objectArray));
    }

    // Not using JavascriptArray::FromVar since there's a virtual call in there
    JavascriptArray *const array = static_cast<JavascriptArray *>(object);
    if(!recycler->IsValidObject(array, sizeof(*array)))
    {LOGMEIN("NativeCodeGenerator.cpp] 791\n");
        return false;
    }

    return true;
}

#if ENABLE_DEBUG_CONFIG_OPTIONS
volatile UINT_PTR NativeCodeGenerator::CodegenFailureSeed = 0;
#endif

void
NativeCodeGenerator::CodeGen(PageAllocator * pageAllocator, CodeGenWorkItem* workItem, const bool foreground)
{LOGMEIN("NativeCodeGenerator.cpp] 804\n");
    if(foreground)
    {
        // Func::Codegen has a lot of things on the stack, so probe the stack here instead
        PROBE_STACK(scriptContext, Js::Constants::MinStackJITCompile);
    }

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (!foreground && Js::Configuration::Global.flags.IsEnabled(Js::InduceCodeGenFailureFlag))
    {LOGMEIN("NativeCodeGenerator.cpp] 813\n");
        if (NativeCodeGenerator::CodegenFailureSeed == 0)
        {LOGMEIN("NativeCodeGenerator.cpp] 815\n");
            // Initialize the seed
            NativeCodeGenerator::CodegenFailureSeed = Js::Configuration::Global.flags.InduceCodeGenFailureSeed;
            if (NativeCodeGenerator::CodegenFailureSeed == 0)
            {LOGMEIN("NativeCodeGenerator.cpp] 819\n");
                LARGE_INTEGER ctr;
                ::QueryPerformanceCounter(&ctr);

                NativeCodeGenerator::CodegenFailureSeed = ctr.HighPart ^ ctr.LowPart;
                srand((uint)NativeCodeGenerator::CodegenFailureSeed);
            }
        }

        int v = Math::Rand() % 100;
        if (v < Js::Configuration::Global.flags.InduceCodeGenFailure)
        {LOGMEIN("NativeCodeGenerator.cpp] 830\n");
            switch (v % 3)
            {LOGMEIN("NativeCodeGenerator.cpp] 832\n");
            case 0: Js::Throw::OutOfMemory(); break;
            case 1: throw Js::StackOverflowException(); break;
            case 2: throw Js::OperationAbortedException(); break;
            default:
                Assert(false);
            }
        }
    }
#endif

    bool irviewerInstance = false;
#ifdef IR_VIEWER
    irviewerInstance = true;
#endif
    Assert(
        workItem->Type() != JsFunctionType ||
        irviewerInstance ||
        IsThunk(workItem->GetFunctionBody()->GetDirectEntryPoint(workItem->GetEntryPoint())) ||
        IsAsmJsCodeGenThunk(workItem->GetFunctionBody()->GetDirectEntryPoint(workItem->GetEntryPoint())));

    InterlockedExchangeAdd(&this->byteCodeSizeGenerated, workItem->GetByteCodeCount()); // must be interlocked because this data may be modified in the foreground and background thread concurrently
    Js::FunctionBody* body = workItem->GetFunctionBody();
    int nRegs = body->GetLocalsCount();
    AssertMsg((nRegs + 1) == (int)(SymID)(nRegs + 1), "SymID too small...");

    if (body->GetScriptContext()->IsClosed())
    {LOGMEIN("NativeCodeGenerator.cpp] 859\n");
        // Should not be jitting something in the foreground when the script context is actually closed
        Assert(IsBackgroundJIT() || !body->GetScriptContext()->IsActuallyClosed());

        throw Js::OperationAbortedException();
    }

    workItem->GetJITData()->nativeDataAddr = (__int3264)workItem->GetEntryPoint()->GetNativeDataBufferRef();

    // TODO: oop jit can we be more efficient here?
    ArenaAllocator alloc(_u("JitData"), pageAllocator, Js::Throw::OutOfMemory);

    auto& jitData = workItem->GetJITData()->jitData;
    jitData = AnewStructZ(&alloc, FunctionJITTimeDataIDL);
    auto codeGenData = workItem->RecyclableData()->JitTimeData();
    FunctionJITTimeInfo::BuildJITTimeData(&alloc, codeGenData, nullptr, workItem->GetJITData()->jitData, false, foreground);
    workItem->GetJITData()->profiledIterations = codeGenData->GetProfiledIterations();
    Js::EntryPointInfo * epInfo = workItem->GetEntryPoint();
    if (workItem->Type() == JsFunctionType)
    {LOGMEIN("NativeCodeGenerator.cpp] 878\n");
        auto funcEPInfo = (Js::FunctionEntryPointInfo*)epInfo;
        jitData->callsCountAddress = (uintptr_t)&funcEPInfo->callsCount;
    }
    else
    {
        workItem->GetJITData()->jittedLoopIterationsSinceLastBailoutAddr = (intptr_t)Js::FunctionBody::GetJittedLoopIterationsSinceLastBailoutAddress(epInfo);
    }

    jitData->sharedPropertyGuards = codeGenData->sharedPropertyGuards;
    jitData->sharedPropGuardCount = codeGenData->sharedPropertyGuardCount;

    JITOutputIDL jitWriteData = {0};

#if !FLOATVAR
    workItem->GetJITData()->xProcNumberPageSegment = scriptContext->GetThreadContext()->GetXProcNumberPageSegmentManager()->GetFreeSegment(&alloc);
#endif
    workItem->GetJITData()->globalThisAddr = (intptr_t)workItem->RecyclableData()->JitTimeData()->GetGlobalThisObject();

    LARGE_INTEGER start_time = { 0 };
    NativeCodeGenerator::LogCodeGenStart(workItem, &start_time);
    workItem->GetJITData()->startTime = (int64)start_time.QuadPart;
    if (JITManager::GetJITManager()->IsOOPJITEnabled())
    {LOGMEIN("NativeCodeGenerator.cpp] 901\n");
        if (!JITManager::GetJITManager()->IsConnected())
        {LOGMEIN("NativeCodeGenerator.cpp] 903\n");
            throw Js::OperationAbortedException();
        }
        HRESULT hr = JITManager::GetJITManager()->RemoteCodeGenCall(
            workItem->GetJITData(),
            scriptContext->GetRemoteScriptAddr(),
            &jitWriteData);
        if (hr == E_ACCESSDENIED && body->GetScriptContext()->IsClosed())
        {LOGMEIN("NativeCodeGenerator.cpp] 911\n");
            // script context may close after codegen call starts, consider this as aborted codegen
            hr = E_ABORT;
        }
        JITManager::HandleServerCallResult(hr, RemoteCallType::CodeGen);

        if (!PreReservedVirtualAllocWrapper::IsInRange((void*)this->scriptContext->GetThreadContext()->GetPreReservedRegionAddr(), (void*)jitWriteData.codeAddress))
        {LOGMEIN("NativeCodeGenerator.cpp] 918\n");
            this->scriptContext->GetJitFuncRangeCache()->AddFuncRange((void*)jitWriteData.codeAddress, jitWriteData.codeSize);
        }
    }
    else
    {
        InProcCodeGenAllocators *const allocators =
            foreground ? EnsureForegroundAllocators(pageAllocator) : GetBackgroundAllocator(pageAllocator); // okay to do outside lock since the respective function is called only from one thread
        NoRecoverMemoryJitArenaAllocator jitArena(_u("JITArena"), pageAllocator, Js::Throw::OutOfMemory);
#if DBG
        jitArena.SetNeedsDelayFreeList();
#endif
        JITTimeWorkItem * jitWorkItem = Anew(&jitArena, JITTimeWorkItem, workItem->GetJITData());

#if !FLOATVAR
        CodeGenNumberAllocator* pNumberAllocator = nullptr;

        // the number allocator needs to be on the stack so that if we are doing foreground JIT
        // the chunk allocated from the recycler will be stacked pinned
        CodeGenNumberAllocator numberAllocator(
            foreground ? nullptr : scriptContext->GetThreadContext()->GetCodeGenNumberThreadAllocator(),
            scriptContext->GetRecycler());
        pNumberAllocator = &numberAllocator;
#endif
        Js::ScriptContextProfiler *const codeGenProfiler =
#ifdef PROFILE_EXEC
            foreground ? EnsureForegroundCodeGenProfiler() : GetBackgroundCodeGenProfiler(pageAllocator); // okay to do outside lock since the respective function is called only from one thread
#else
            nullptr;
#endif

        Func::Codegen(&jitArena, jitWorkItem, scriptContext->GetThreadContext(),
            scriptContext, &jitWriteData, epInfo, nullptr, jitWorkItem->GetPolymorphicInlineCacheInfo(), allocators,
#if !FLOATVAR
            pNumberAllocator,
#endif
            codeGenProfiler, !foreground);
        
        if (!this->scriptContext->GetThreadContext()->GetPreReservedVirtualAllocator()->IsInRange((void*)jitWriteData.codeAddress))
        {LOGMEIN("NativeCodeGenerator.cpp] 957\n");
            this->scriptContext->GetJitFuncRangeCache()->AddFuncRange((void*)jitWriteData.codeAddress, jitWriteData.codeSize);
        }
    }
    
    if (JITManager::GetJITManager()->IsOOPJITEnabled() && PHASE_VERBOSE_TRACE(Js::BackEndPhase, workItem->GetFunctionBody()))
    {LOGMEIN("NativeCodeGenerator.cpp] 963\n");
        LARGE_INTEGER freq;
        LARGE_INTEGER end_time;
        QueryPerformanceCounter(&end_time);
        QueryPerformanceFrequency(&freq);

        Output::Print(
            _u("BackendMarshalOut - function: %s time:%8.6f mSec\r\n"),
            workItem->GetFunctionBody()->GetDisplayName(),
            (((double)((end_time.QuadPart - jitWriteData.startTime)* (double)1000.0 / (double)freq.QuadPart))) / (1));
        Output::Flush();
    }

    workItem->GetFunctionBody()->SetFrameHeight(workItem->GetEntryPoint(), jitWriteData.frameHeight);

    if (workItem->Type() == JsFunctionType)
    {LOGMEIN("NativeCodeGenerator.cpp] 979\n");
        Js::FunctionEntryPointInfo * funcEP = (Js::FunctionEntryPointInfo*)workItem->GetEntryPoint();
        funcEP->localVarSlotsOffset = jitWriteData.localVarSlotsOffset;
        funcEP->localVarChangedOffset = jitWriteData.localVarChangedOffset;
    }

    if (jitWriteData.hasJittedStackClosure != FALSE)
    {LOGMEIN("NativeCodeGenerator.cpp] 986\n");
        workItem->GetEntryPoint()->SetHasJittedStackClosure();
    }

    if (jitWriteData.numberPageSegments)
    {LOGMEIN("NativeCodeGenerator.cpp] 991\n");
        if (jitWriteData.numberPageSegments->pageAddress == 0)
        {LOGMEIN("NativeCodeGenerator.cpp] 993\n");
            midl_user_free(jitWriteData.numberPageSegments);
            jitWriteData.numberPageSegments = nullptr;
        }
        else
        {
            // TODO: when codegen fail, need to return the segment as well
            epInfo->SetNumberPageSegment(jitWriteData.numberPageSegments);
        }
    }

    if (JITManager::GetJITManager()->IsOOPJITEnabled())
    {LOGMEIN("NativeCodeGenerator.cpp] 1005\n");
        if (jitWriteData.nativeDataFixupTable)
        {LOGMEIN("NativeCodeGenerator.cpp] 1007\n");
            for (unsigned int i = 0; i < jitWriteData.nativeDataFixupTable->count; i++)
            {LOGMEIN("NativeCodeGenerator.cpp] 1009\n");
                auto& record = jitWriteData.nativeDataFixupTable->fixupRecords[i];
                auto updateList = record.updateList;

                if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                {LOGMEIN("NativeCodeGenerator.cpp] 1014\n");
                    Output::Print(_u("NativeCodeData Fixup: allocIndex:%d, len:%x, totalOffset:%x, startAddress:%p\n"),
                        record.index, record.length, record.startOffset, jitWriteData.buffer->data + record.startOffset);
                }

                while (updateList)
                {LOGMEIN("NativeCodeGenerator.cpp] 1020\n");
                    void* addrToFixup = jitWriteData.buffer->data + record.startOffset + updateList->addrOffset;
                    void* targetAddr = jitWriteData.buffer->data + updateList->targetTotalOffset;

                    if (PHASE_TRACE1(Js::NativeCodeDataPhase))
                    {LOGMEIN("NativeCodeGenerator.cpp] 1025\n");
                        Output::Print(_u("\tEntry: +%x %p(%p) ==> %p\n"), updateList->addrOffset, addrToFixup, *(void**)(addrToFixup), targetAddr);
                    }

                    *(void**)(addrToFixup) = targetAddr;
                    auto current = updateList;
                    updateList = updateList->next;
                    midl_user_free(current);
                }
            }
            midl_user_free(jitWriteData.nativeDataFixupTable);
            jitWriteData.nativeDataFixupTable = nullptr;

            // change the address with the fixup information
            *epInfo->GetNativeDataBufferRef() = (char*)jitWriteData.buffer->data;

#if DBG
            if (PHASE_TRACE1(Js::NativeCodeDataPhase))
            {LOGMEIN("NativeCodeGenerator.cpp] 1043\n");
                Output::Print(_u("NativeCodeData Client Buffer: %p, len: %x\n"), jitWriteData.buffer->data, jitWriteData.buffer->len);
            }
#endif
        }

        epInfo->GetJitTransferData()->SetRuntimeTypeRefs(jitWriteData.pinnedTypeRefs);

        if (jitWriteData.throwMapCount > 0)
        {LOGMEIN("NativeCodeGenerator.cpp] 1052\n");
            Js::ThrowMapEntry * throwMap = (Js::ThrowMapEntry *)(jitWriteData.buffer->data + jitWriteData.throwMapOffset);
            Js::SmallSpanSequenceIter iter;
            for (uint i = 0; i < jitWriteData.throwMapCount; ++i)
            {LOGMEIN("NativeCodeGenerator.cpp] 1056\n");
                workItem->RecordNativeThrowMap(iter, throwMap[i].nativeBufferOffset, throwMap[i].statementIndex);
            }
        }
    }

    if (workItem->GetJitMode() != ExecutionMode::SimpleJit)
    {LOGMEIN("NativeCodeGenerator.cpp] 1063\n");
        epInfo->RecordInlineeFrameOffsetsInfo(jitWriteData.inlineeFrameOffsetArrayOffset, jitWriteData.inlineeFrameOffsetArrayCount);

        epInfo->GetJitTransferData()->SetEquivalentTypeGuardOffsets(jitWriteData.equivalentTypeGuardOffsets);
        epInfo->GetJitTransferData()->SetTypeGuardTransferData(&jitWriteData);

        Assert(jitWriteData.ctorCacheEntries == nullptr || epInfo->GetConstructorCacheCount() > 0);
        epInfo->GetJitTransferData()->SetCtorCacheTransferData(&jitWriteData);

        workItem->GetEntryPoint()->GetJitTransferData()->SetIsReady();
    }

#if defined(_M_X64)
    XDataAllocation * xdataInfo = HeapNewZ(XDataAllocation);
    xdataInfo->address = (byte*)jitWriteData.xdataAddr;
    XDataAllocator::Register(xdataInfo, jitWriteData.codeAddress, jitWriteData.codeSize);
    epInfo->SetXDataInfo(xdataInfo);
#endif


#if defined(_M_ARM)
    // for in-proc jit we do registration in encoder
    if (JITManager::GetJITManager()->IsOOPJITEnabled())
    {LOGMEIN("NativeCodeGenerator.cpp] 1086\n");
        XDataAllocation * xdataInfo = HeapNewZ(XDataAllocation);
        xdataInfo->pdataCount = jitWriteData.pdataCount;
        xdataInfo->xdataSize = jitWriteData.xdataSize;
        if (jitWriteData.buffer)
        {LOGMEIN("NativeCodeGenerator.cpp] 1091\n");
            xdataInfo->address = jitWriteData.buffer->data + jitWriteData.xdataOffset;
            for (ushort i = 0; i < xdataInfo->pdataCount; ++i)
            {LOGMEIN("NativeCodeGenerator.cpp] 1094\n");
                RUNTIME_FUNCTION *function = xdataInfo->GetPdataArray() + i;
                // if flag is 0, then we have separate .xdata, for which we need to fixup the address
                if (function->Flag == 0)
                {LOGMEIN("NativeCodeGenerator.cpp] 1098\n");
                    // UnwindData was set on server as the offset from the beginning of xdata buffer
                    function->UnwindData = (DWORD)(xdataInfo->address + function->UnwindData);
                    Assert(((DWORD)function->UnwindData & 0x3) == 0); // 4 byte aligned
                }
            }
        }
        else
        {
            xdataInfo->address = nullptr;
        }
        // unmask thumb mode from code address
        XDataAllocator::Register(xdataInfo, jitWriteData.codeAddress & ~0x1, jitWriteData.codeSize);
        epInfo->SetXDataInfo(xdataInfo);
    }
#endif

    if (!CONFIG_FLAG(OOPCFGRegistration))
    {LOGMEIN("NativeCodeGenerator.cpp] 1116\n");
        scriptContext->GetThreadContext()->SetValidCallTargetForCFG((PVOID)jitWriteData.codeAddress);
    }

    workItem->SetCodeAddress((size_t)jitWriteData.codeAddress);

    workItem->GetEntryPoint()->SetCodeGenRecorded((Js::JavascriptMethod)jitWriteData.codeAddress, jitWriteData.codeSize);

    if (jitWriteData.hasBailoutInstr != FALSE)
    {LOGMEIN("NativeCodeGenerator.cpp] 1125\n");
        body->SetHasBailoutInstrInJittedCode(true);
    }
    if (!jitWriteData.isInPrereservedRegion)
    {LOGMEIN("NativeCodeGenerator.cpp] 1129\n");
        scriptContext->GetThreadContext()->ResetIsAllJITCodeInPreReservedRegion();
    }

    body->m_argUsedForBranch |= jitWriteData.argUsedForBranch;

    if (body->HasDynamicProfileInfo())
    {LOGMEIN("NativeCodeGenerator.cpp] 1136\n");
        if (jitWriteData.disableAggressiveIntTypeSpec)
        {LOGMEIN("NativeCodeGenerator.cpp] 1138\n");
            body->GetAnyDynamicProfileInfo()->DisableAggressiveIntTypeSpec(workItem->Type() == JsLoopBodyWorkItemType);
        }
        if (jitWriteData.disableStackArgOpt)
        {LOGMEIN("NativeCodeGenerator.cpp] 1142\n");
            body->GetAnyDynamicProfileInfo()->DisableStackArgOpt();
        }
        if (jitWriteData.disableSwitchOpt)
        {LOGMEIN("NativeCodeGenerator.cpp] 1146\n");
            body->GetAnyDynamicProfileInfo()->DisableSwitchOpt();
        }
        if (jitWriteData.disableTrackCompoundedIntOverflow)
        {LOGMEIN("NativeCodeGenerator.cpp] 1150\n");
            body->GetAnyDynamicProfileInfo()->DisableTrackCompoundedIntOverflow();
        }
    }

    if (jitWriteData.disableInlineApply)
    {LOGMEIN("NativeCodeGenerator.cpp] 1156\n");
        body->SetDisableInlineApply(true);
    }
    if (jitWriteData.disableInlineSpread)
    {LOGMEIN("NativeCodeGenerator.cpp] 1160\n");
        body->SetDisableInlineSpread(true);
    }

#ifdef PROFILE_BAILOUT_RECORD_MEMORY
    if (Js::Configuration::Global.flags.ProfileBailOutRecordMemory)
    {LOGMEIN("NativeCodeGenerator.cpp] 1166\n");
        scriptContext->codeSize += workItem->GetEntryPoint()->GetCodeSize();
    }
#endif

    NativeCodeGenerator::LogCodeGenDone(workItem, &start_time);

#ifdef BGJIT_STATS
    // Must be interlocked because the following data may be modified from the background and foreground threads concurrently
    Js::ScriptContext *scriptContext = workItem->GetScriptContext();
    if (workItem->Type() == JsFunctionType)
    {LOGMEIN("NativeCodeGenerator.cpp] 1177\n");
        InterlockedExchangeAdd(&scriptContext->bytecodeJITCount, workItem->GetByteCodeCount());
        InterlockedIncrement(&scriptContext->funcJITCount);
    }
    else if(workItem->Type() == JsLoopBodyWorkItemType)
    {LOGMEIN("NativeCodeGenerator.cpp] 1182\n");
        InterlockedIncrement(&scriptContext->loopJITCount);
    }
#endif
}

/* static */
void NativeCodeGenerator::LogCodeGenStart(CodeGenWorkItem * workItem, LARGE_INTEGER * start_time)
{LOGMEIN("NativeCodeGenerator.cpp] 1190\n");
    Js::FunctionBody * body = workItem->GetFunctionBody();
    {LOGMEIN("NativeCodeGenerator.cpp] 1192\n");
        if (IS_JS_ETW(EventEnabledJSCRIPT_FUNCTION_JIT_START()))
        {LOGMEIN("NativeCodeGenerator.cpp] 1194\n");
            WCHAR displayNameBuffer[256];
            WCHAR* displayName = displayNameBuffer;
            size_t sizeInChars = workItem->GetDisplayName(displayName, 256);
            if (sizeInChars > 256)
            {LOGMEIN("NativeCodeGenerator.cpp] 1199\n");
                displayName = new WCHAR[sizeInChars];
                workItem->GetDisplayName(displayName, 256);
            }
            JS_ETW(EventWriteJSCRIPT_FUNCTION_JIT_START(
                body->GetFunctionNumber(),
                displayName,
                body->GetScriptContext(),
                workItem->GetInterpretedCount(),
                (const unsigned int)body->LengthInBytes(),
                body->GetByteCodeCount(),
                body->GetByteCodeInLoopCount(),
                (int)workItem->GetJitMode()));

            if (displayName != displayNameBuffer)
            {LOGMEIN("NativeCodeGenerator.cpp] 1214\n");
                delete[] displayName;
            }
        }
    }

#if DBG_DUMP
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::BackEndPhase))
    {LOGMEIN("NativeCodeGenerator.cpp] 1222\n");
        if (workItem->GetEntryPoint()->IsLoopBody())
        {LOGMEIN("NativeCodeGenerator.cpp] 1224\n");
            Output::Print(_u("---BeginBackEnd: function: %s, loop:%d---\r\n"), body->GetDisplayName(), ((JsLoopBodyCodeGen*)workItem)->GetLoopNumber());
        }
        else
        {
            Output::Print(_u("---BeginBackEnd: function: %s---\r\n"), body->GetDisplayName());
        }
        Output::Flush();
    }
#endif

    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

    if (PHASE_TRACE(Js::BackEndPhase, body))
    {LOGMEIN("NativeCodeGenerator.cpp] 1238\n");
        QueryPerformanceCounter(start_time);
        if (workItem->GetEntryPoint()->IsLoopBody())
        {LOGMEIN("NativeCodeGenerator.cpp] 1241\n");
            Output::Print(
                _u("BeginBackEnd - function: %s (%s, line %u), loop: %u, mode: %S"),
                body->GetDisplayName(),
                body->GetDebugNumberSet(debugStringBuffer),
                body->GetLineNumber(),
                ((JsLoopBodyCodeGen*)workItem)->GetLoopNumber(),
                ExecutionModeName(workItem->GetJitMode()));
            if (body->GetIsAsmjsMode())
            {LOGMEIN("NativeCodeGenerator.cpp] 1250\n");
                Output::Print(_u(" (Asmjs)\n"));
            }
            else
            {
                Output::Print(_u("\n"));
            }
        }
        else
        {
            Output::Print(
                _u("BeginBackEnd - function: %s (%s, line %u), mode: %S"),
                body->GetDisplayName(),
                body->GetDebugNumberSet(debugStringBuffer),
                body->GetLineNumber(),
                ExecutionModeName(workItem->GetJitMode()));

            if (body->GetIsAsmjsMode())
            {LOGMEIN("NativeCodeGenerator.cpp] 1268\n");
                Output::Print(_u(" (Asmjs)\n"));
            }
            else
            {
                Output::Print(_u("\n"));
            }
        }
        Output::Flush();
    }

#ifdef FIELD_ACCESS_STATS
    if (PHASE_TRACE(Js::ObjTypeSpecPhase, body) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, body))
    {LOGMEIN("NativeCodeGenerator.cpp] 1281\n");
        if (workItem->RecyclableData()->JitTimeData()->inlineCacheStats)
        {LOGMEIN("NativeCodeGenerator.cpp] 1283\n");
            auto stats = workItem->RecyclableData()->JitTimeData()->inlineCacheStats;
            Output::Print(_u("ObjTypeSpec: jitting function %s (#%s): inline cache stats:\n"), body->GetDisplayName(), body->GetDebugNumberSet(debugStringBuffer));
            Output::Print(_u("    overall: total %u, no profile info %u\n"), stats->totalInlineCacheCount, stats->noInfoInlineCacheCount);
            Output::Print(_u("    mono: total %u, empty %u, cloned %u\n"),
                stats->monoInlineCacheCount, stats->emptyMonoInlineCacheCount, stats->clonedMonoInlineCacheCount);
            Output::Print(_u("    poly: total %u (high %u, low %u), null %u, empty %u, ignored %u, disabled %u, equivalent %u, non-equivalent %u, cloned %u\n"),
                stats->polyInlineCacheCount, stats->highUtilPolyInlineCacheCount, stats->lowUtilPolyInlineCacheCount,
                stats->nullPolyInlineCacheCount, stats->emptyPolyInlineCacheCount, stats->ignoredPolyInlineCacheCount, stats->disabledPolyInlineCacheCount,
                stats->equivPolyInlineCacheCount, stats->nonEquivPolyInlineCacheCount, stats->clonedPolyInlineCacheCount);
        }
        else
        {
            Output::Print(_u("EquivObjTypeSpec: function %s (%s): inline cache stats unavailable\n"), body->GetDisplayName(), body->GetDebugNumberSet(debugStringBuffer));
        }
        Output::Flush();
    }
#endif
}

/* static */
void NativeCodeGenerator::LogCodeGenDone(CodeGenWorkItem * workItem, LARGE_INTEGER * start_time)
{LOGMEIN("NativeCodeGenerator.cpp] 1305\n");
    Js::FunctionBody * body = workItem->GetFunctionBody();
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

    {
        if (IS_JS_ETW(EventEnabledJSCRIPT_FUNCTION_JIT_STOP()))
        {LOGMEIN("NativeCodeGenerator.cpp] 1311\n");
            WCHAR displayNameBuffer[256];
            WCHAR* displayName = displayNameBuffer;
            size_t sizeInChars = workItem->GetDisplayName(displayName, 256);
            if (sizeInChars > 256)
            {LOGMEIN("NativeCodeGenerator.cpp] 1316\n");
                displayName = new WCHAR[sizeInChars];
                workItem->GetDisplayName(displayName, 256);
            }
            void* entryPoint;
            ptrdiff_t codeSize;
            workItem->GetEntryPointAddress(&entryPoint, &codeSize);
            JS_ETW(EventWriteJSCRIPT_FUNCTION_JIT_STOP(
                body->GetFunctionNumber(),
                displayName,
                body->GetScriptContext(),
                workItem->GetInterpretedCount(),
                entryPoint,
                codeSize));

            if (displayName != displayNameBuffer)
            {LOGMEIN("NativeCodeGenerator.cpp] 1332\n");
                delete[] displayName;
            }
        }
    }

#if DBG_DUMP
    if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::BackEndPhase))
    {LOGMEIN("NativeCodeGenerator.cpp] 1340\n");
        Output::Print(_u("---EndBackEnd---\r\n"));
        Output::Flush();
    }
#endif

    if (PHASE_TRACE(Js::BackEndPhase, body))
    {LOGMEIN("NativeCodeGenerator.cpp] 1347\n");
        LARGE_INTEGER freq;
        LARGE_INTEGER end_time;
        QueryPerformanceCounter(&end_time);
        QueryPerformanceFrequency(&freq);
        if (workItem->GetEntryPoint()->IsLoopBody())
        {LOGMEIN("NativeCodeGenerator.cpp] 1353\n");
            Output::Print(
                _u("EndBackEnd - function: %s (%s, line %u), loop: %u, mode: %S, time:%8.6f mSec"),
                body->GetDisplayName(),
                body->GetDebugNumberSet(debugStringBuffer),
                body->GetLineNumber(),
                ((JsLoopBodyCodeGen*)workItem)->GetLoopNumber(),
                ExecutionModeName(workItem->GetJitMode()),
                (((double)((end_time.QuadPart - start_time->QuadPart)* (double)1000.0 / (double)freq.QuadPart))) / (1));

            if (body->GetIsAsmjsMode())
            {LOGMEIN("NativeCodeGenerator.cpp] 1364\n");
                Output::Print(_u(" (Asmjs)\n"));
            }
            else
            {
                Output::Print(_u("\n"));
            }
        }
        else
        {
            Output::Print(
                _u("EndBackEnd - function: %s (%s, line %u), mode: %S time:%8.6f mSec"),
                body->GetDisplayName(),
                body->GetDebugNumberSet(debugStringBuffer),
                body->GetLineNumber(),
                ExecutionModeName(workItem->GetJitMode()),
                (((double)((end_time.QuadPart - start_time->QuadPart)* (double)1000.0 / (double)freq.QuadPart))) / (1));

            if (body->GetIsAsmjsMode())
            {LOGMEIN("NativeCodeGenerator.cpp] 1383\n");
                Output::Print(_u(" (Asmjs)\n"));
            }
            else
            {
                Output::Print(_u("\n"));
            }
        }
        Output::Flush();
    }
}

void NativeCodeGenerator::SetProfileMode(BOOL fSet)
{LOGMEIN("NativeCodeGenerator.cpp] 1396\n");
    this->SetNativeEntryPoint = fSet? Js::FunctionBody::ProfileSetNativeEntryPoint : Js::FunctionBody::DefaultSetNativeEntryPoint;
}

#if _M_IX86
__declspec(naked)
Js::Var
NativeCodeGenerator::CheckAsmJsCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...)
{
    __asm
    {
            push ebp
            mov ebp, esp
            push function
            call NativeCodeGenerator::CheckAsmJsCodeGen
#ifdef _CONTROL_FLOW_GUARD
            // verify that the call target is valid
            push eax
            mov  ecx, eax
            call[__guard_check_icall_fptr]
            pop eax
#endif
            pop ebp
            jmp  eax
    }
}
#elif _M_X64 || _M_ARM || _M_ARM64
    // Do nothing: the implementation of NativeCodeGenerator::CheckCodeGenThunk is declared (appropriately decorated) in
    // Backend\amd64\Thunks.asm and Backend\arm\Thunks.asm and Backend\arm64\Thunks.asm respectively.
#else
    #error Not implemented.
#endif

#if _M_IX86
__declspec(naked)
Js::Var
NativeCodeGenerator::CheckCodeGenThunk(Js::RecyclableObject* function, Js::CallInfo callInfo, ...)
{
    __asm
    {
        push ebp
        mov ebp, esp
        push [esp+8]
        call NativeCodeGenerator::CheckCodeGen
#ifdef _CONTROL_FLOW_GUARD
        // verify that the call target is valid
        push eax
        mov  ecx, eax
        call[__guard_check_icall_fptr]
        pop eax
#endif
        pop ebp
        jmp  eax
    }
}
#elif _M_X64 || _M_ARM || _M_ARM64
    // Do nothing: the implementation of NativeCodeGenerator::CheckCodeGenThunk is declared (appropriately decorated) in
    // Backend\amd64\Thunks.asm and Backend\arm\Thunks.asm and Backend\arm64\Thunks.asm respectively.
#else
    #error Not implemented.
#endif

bool
NativeCodeGenerator::IsThunk(Js::JavascriptMethod codeAddress)
{LOGMEIN("NativeCodeGenerator.cpp] 1460\n");
    return codeAddress == NativeCodeGenerator::CheckCodeGenThunk;
}

bool
NativeCodeGenerator::IsAsmJsCodeGenThunk(Js::JavascriptMethod codeAddress)
{LOGMEIN("NativeCodeGenerator.cpp] 1466\n");
#ifdef ASMJS_PLAT
    return codeAddress == NativeCodeGenerator::CheckAsmJsCodeGenThunk;
#else
    return false;
#endif

}

CheckCodeGenFunction
NativeCodeGenerator::GetCheckCodeGenFunction(Js::JavascriptMethod codeAddress)
{LOGMEIN("NativeCodeGenerator.cpp] 1477\n");
    if (codeAddress == NativeCodeGenerator::CheckCodeGenThunk)
    {LOGMEIN("NativeCodeGenerator.cpp] 1479\n");
        return NativeCodeGenerator::CheckCodeGen;
    }
    return nullptr;
}


Js::Var
NativeCodeGenerator::CheckAsmJsCodeGen(Js::ScriptFunction * function)
{LOGMEIN("NativeCodeGenerator.cpp] 1488\n");
    Assert(function);
    Js::FunctionBody *functionBody = function->GetFunctionBody();
    Js::ScriptContext *scriptContext = functionBody->GetScriptContext();

    NativeCodeGenerator *nativeCodeGen = scriptContext->GetNativeCodeGenerator();
    Assert(scriptContext->GetThreadContext()->IsScriptActive());
    Assert(scriptContext->GetThreadContext()->IsInScript());

    // Load the entry point here to validate it got changed afterwards

    Js::FunctionEntryPointInfo* entryPoint = function->GetFunctionEntryPointInfo();

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if ((PHASE_ON1(Js::AsmJsJITTemplatePhase) && CONFIG_FLAG(MaxTemplatizedJitRunCount) >= 0) || (!PHASE_ON1(Js::AsmJsJITTemplatePhase) && CONFIG_FLAG(MaxAsmJsInterpreterRunCount) >= 0))
    {LOGMEIN("NativeCodeGenerator.cpp] 1503\n");
        nativeCodeGen->Processor()->PrioritizeJobAndWait(nativeCodeGen, entryPoint, function);
    } else
#endif
    if (!nativeCodeGen->Processor()->PrioritizeJob(nativeCodeGen, entryPoint, function))
    {LOGMEIN("NativeCodeGenerator.cpp] 1508\n");
        if (PHASE_TRACE1(Js::AsmjsEntryPointInfoPhase))
        {LOGMEIN("NativeCodeGenerator.cpp] 1510\n");
            Output::Print(_u("Codegen not done yet for function: %s, Entrypoint is CheckAsmJsCodeGenThunk\n"), function->GetFunctionBody()->GetDisplayName());
        }
        return reinterpret_cast<Js::Var>(entryPoint->GetNativeAddress());
    }
    if (PHASE_TRACE1(Js::AsmjsEntryPointInfoPhase))
    {LOGMEIN("NativeCodeGenerator.cpp] 1516\n");
        Output::Print(_u("CodeGen Done for function: %s, Changing Entrypoint to Full JIT\n"), function->GetFunctionBody()->GetDisplayName());
    }
    // we will need to set the functionbody external and asmjs entrypoint to the fulljit entrypoint
    return reinterpret_cast<Js::Var>(CheckCodeGenDone(functionBody, entryPoint, function));
}

Js::JavascriptMethod
NativeCodeGenerator::CheckCodeGen(Js::ScriptFunction * function)
{LOGMEIN("NativeCodeGenerator.cpp] 1525\n");
    Assert(function);
    Assert(function->GetEntryPoint() == NativeCodeGenerator::CheckCodeGenThunk
        || Js::CrossSite::IsThunk(function->GetEntryPoint()));
    // We are not expecting non-deserialized functions here; Error if it hasn't been deserialized by this point
    Js::FunctionBody *functionBody = function->GetFunctionBody();
    Js::ScriptContext *scriptContext = functionBody->GetScriptContext();

    NativeCodeGenerator *nativeCodeGen = scriptContext->GetNativeCodeGenerator();
    Assert(scriptContext->GetThreadContext()->IsScriptActive());
    Assert(scriptContext->GetThreadContext()->IsInScript());

    // Load the entry point here to validate it got changed afterwards
    Js::JavascriptMethod originalEntryPoint = functionBody->GetOriginalEntryPoint();
    Js::FunctionEntryPointInfo* entryPoint = function->GetFunctionEntryPointInfo();

    Js::FunctionEntryPointInfo *const defaultEntryPointInfo = functionBody->GetDefaultFunctionEntryPointInfo();
    if(entryPoint != defaultEntryPointInfo)
    {LOGMEIN("NativeCodeGenerator.cpp] 1543\n");
        // Switch to the latest entry point info
        function->UpdateThunkEntryPoint(defaultEntryPointInfo, functionBody->GetDirectEntryPoint(defaultEntryPointInfo));

        const Js::JavascriptMethod defaultDirectEntryPoint = functionBody->GetDirectEntryPoint(defaultEntryPointInfo);
        if(!IsThunk(defaultDirectEntryPoint))
        {LOGMEIN("NativeCodeGenerator.cpp] 1549\n");
            return defaultDirectEntryPoint;
        }
        entryPoint = defaultEntryPointInfo;
    }

    // If a transition to JIT needs to be forced, JIT right away
    if(Js::Configuration::Global.flags.EnforceExecutionModeLimits &&
        functionBody->GetExecutionMode() != ExecutionMode::SimpleJit &&
        functionBody->TryTransitionToJitExecutionMode())
    {LOGMEIN("NativeCodeGenerator.cpp] 1559\n");
        nativeCodeGen->Processor()->PrioritizeJobAndWait(nativeCodeGen, entryPoint, function);
        return CheckCodeGenDone(functionBody, entryPoint, function);
    }

    if(!nativeCodeGen->Processor()->PrioritizeJob(nativeCodeGen, entryPoint, function))
    {LOGMEIN("NativeCodeGenerator.cpp] 1565\n");
#if defined(ENABLE_SCRIPT_PROFILING) || defined(ENABLE_SCRIPT_DEBUGGING)
#define originalEntryPoint_IS_ProfileDeferredParsingThunk \
            (originalEntryPoint == ProfileDeferredParsingThunk)
#else
#define originalEntryPoint_IS_ProfileDeferredParsingThunk \
            false
#endif
        // Job was not yet processed
        // originalEntryPoint is the last known good entry point for the function body. Here we verify that
        // it either corresponds with this codegen episode (identified by function->entryPointIndex) of the function body
        // or one that was scheduled after. In the latter case originalEntryPoint will get updated if and when
        // that last episode completes successfully.
        Assert(functionBody->GetDefaultEntryPointInfo() == function->GetEntryPointInfo() &&
            (
                originalEntryPoint == DefaultEntryThunk
             || scriptContext->IsDynamicInterpreterThunk(originalEntryPoint)
             || originalEntryPoint_IS_ProfileDeferredParsingThunk
             || originalEntryPoint == DefaultDeferredParsingThunk
             || (
                    functionBody->GetSimpleJitEntryPointInfo() &&
                    originalEntryPoint ==
                        reinterpret_cast<Js::JavascriptMethod>(functionBody->GetSimpleJitEntryPointInfo()->GetNativeAddress())
                )
            ) ||
            functionBody->GetDefaultFunctionEntryPointInfo()->entryPointIndex > function->GetFunctionEntryPointInfo()->entryPointIndex);
        return (scriptContext->CurrentThunk == ProfileEntryThunk) ? ProfileEntryThunk : originalEntryPoint;
    }

    return CheckCodeGenDone(functionBody, entryPoint, function);
}

Js::JavascriptMethod
NativeCodeGenerator::CheckCodeGenDone(
    Js::FunctionBody *const functionBody,
    Js::FunctionEntryPointInfo *const entryPointInfo,
    Js::ScriptFunction * function)
{LOGMEIN("NativeCodeGenerator.cpp] 1602\n");
    Assert(!function || function->GetFunctionBody() == functionBody);
    Assert(!function || function->GetFunctionEntryPointInfo() == entryPointInfo);
    // Job was processed or failed and cleaned up
    // We won't call CheckCodeGenDone if the job is still pending since
    // PrioritizeJob will return false
    Assert(entryPointInfo->IsCodeGenDone() || entryPointInfo->IsCleanedUp() || entryPointInfo->IsPendingCleanup());

    if (!functionBody->GetHasBailoutInstrInJittedCode() && functionBody->GetHasAllocatedLoopHeaders()
#ifdef ASMJS_PLAT
        && (!functionBody->GetIsAsmJsFunction() || !(((Js::FunctionEntryPointInfo*)functionBody->GetDefaultEntryPointInfo())->GetIsTJMode()))
#endif
    )
    {LOGMEIN("NativeCodeGenerator.cpp] 1615\n");
        if (functionBody->GetCanReleaseLoopHeaders())
        {LOGMEIN("NativeCodeGenerator.cpp] 1617\n");
            functionBody->ReleaseLoopHeaders();
        }
        else
        {
            functionBody->SetPendingLoopHeaderRelease(true);
        }
    }

    Js::ScriptContext *scriptContext = functionBody->GetScriptContext();
    if (!functionBody->GetNativeEntryPointUsed())
    {LOGMEIN("NativeCodeGenerator.cpp] 1628\n");
#ifdef BGJIT_STATS
        scriptContext->jitCodeUsed += functionBody->GetByteCodeCount();
        scriptContext->funcJitCodeUsed++;
#endif

        functionBody->SetNativeEntryPointUsed(true);
    }

    // Replace the entry point
    Js::JavascriptMethod jsMethod;
    if (!entryPointInfo->IsCodeGenDone())
    {LOGMEIN("NativeCodeGenerator.cpp] 1640\n");
        if (entryPointInfo->IsPendingCleanup())
        {LOGMEIN("NativeCodeGenerator.cpp] 1642\n");
            entryPointInfo->Cleanup(false /* isShutdown */, true /* capture cleanup stack */);
        }

        // Do not profile WebAssembly functions
        jsMethod = (functionBody->GetScriptContext()->CurrentThunk == ProfileEntryThunk
                    && !functionBody->IsWasmFunction()) ? ProfileEntryThunk : functionBody->GetOriginalEntryPoint();
        entryPointInfo->jsMethod = jsMethod;
    }
    else
    {
        scriptContext->GetNativeCodeGenerator()->SetNativeEntryPoint(
            entryPointInfo,
            functionBody,
            reinterpret_cast<Js::JavascriptMethod>(entryPointInfo->GetNativeAddress()));
        jsMethod = entryPointInfo->jsMethod;

        Assert(!functionBody->NeedEnsureDynamicProfileInfo() || jsMethod == Js::DynamicProfileInfo::EnsureDynamicProfileInfoThunk);
    }

    Assert(!IsThunk(jsMethod));

    if(function)
    {LOGMEIN("NativeCodeGenerator.cpp] 1665\n");
        function->UpdateThunkEntryPoint(entryPointInfo, jsMethod);
    }

    // call the direct entry point, which will ensure dynamic profile info if necessary
    return jsMethod;
}

CodeGenWorkItem *
NativeCodeGenerator::GetJob(Js::EntryPointInfo * const entryPoint) const
{LOGMEIN("NativeCodeGenerator.cpp] 1675\n");
    ASSERT_THREAD();
    Assert(entryPoint);

    return entryPoint->GetWorkItem();
}

bool
NativeCodeGenerator::WasAddedToJobProcessor(JsUtil::Job *const job) const
{LOGMEIN("NativeCodeGenerator.cpp] 1684\n");
    // This function is called from inside the lock

    ASSERT_THREAD();
    Assert(job);

    return static_cast<CodeGenWorkItem *>(job)->IsInJitQueue();
}

bool
NativeCodeGenerator::ShouldProcessInForeground(const bool willWaitForJob, const unsigned int numJobsInQueue) const
{LOGMEIN("NativeCodeGenerator.cpp] 1695\n");
    // This function is called from inside the lock

    ASSERT_THREAD();

    // Process the job synchronously in the foreground thread if we're waiting for the job to be processed, or if the background
    // job queue is long enough and this native code generator is optimized for many instances (web workers)
    return
        willWaitForJob ||
        (numJobsInQueue > (uint)CONFIG_FLAG(HybridFgJitBgQueueLengthThreshold) &&
            (CONFIG_FLAG(HybridFgJit) || isOptimizedForManyInstances));
}

void
NativeCodeGenerator::PrioritizedButNotYetProcessed(JsUtil::Job *const job)
{LOGMEIN("NativeCodeGenerator.cpp] 1710\n");
    // This function is called from inside the lock

    ASSERT_THREAD();
    Assert(job);

#ifdef BGJIT_STATS
    CodeGenWorkItem *const codeGenWorkItem = static_cast<CodeGenWorkItem *>(job);
    if(codeGenWorkItem->Type() == JsFunctionType && codeGenWorkItem->IsInJitQueue())
    {LOGMEIN("NativeCodeGenerator.cpp] 1719\n");
        codeGenWorkItem->GetScriptContext()->interpretedCallsHighPri++;

        if(codeGenWorkItem->GetJitMode() == ExecutionMode::FullJit)
        {LOGMEIN("NativeCodeGenerator.cpp] 1723\n");
            QueuedFullJitWorkItem *const queuedFullJitWorkItem = codeGenWorkItem->GetQueuedFullJitWorkItem();
            if(queuedFullJitWorkItem)
            {LOGMEIN("NativeCodeGenerator.cpp] 1726\n");
                queuedFullJitWorkItems.MoveToBeginning(queuedFullJitWorkItem);
            }
        }
    }
#endif
}


void
NativeCodeGenerator::BeforeWaitForJob(Js::EntryPointInfo *const entryPoint) const
{LOGMEIN("NativeCodeGenerator.cpp] 1737\n");
    ASSERT_THREAD();
    Assert(entryPoint);

#ifdef PROFILE_EXEC
    ProfileBegin(this->foregroundCodeGenProfiler, Js::DelayPhase);
#endif
}

void
NativeCodeGenerator::AfterWaitForJob(Js::EntryPointInfo *const entryPoint) const
{LOGMEIN("NativeCodeGenerator.cpp] 1748\n");
    ASSERT_THREAD();
    Assert(entryPoint);

#ifdef PROFILE_EXEC
    ProfileEnd(this->foregroundCodeGenProfiler, Js::DelayPhase);
#endif
}

/*
* A workitem exceeds JIT limits if we've already generated MaxThreadJITCodeHeapSize
* (currently 7 MB) of code on this thread or MaxProcessJITCodeHeapSize (currently 55 MB)
* in the process. In real world websites we rarely (if at all) hit this limit.
* Also, if this workitem's byte code size is in excess of MaxJITFunctionBytecodeSize instructions,
* it exceeds the JIT limits
*/
bool
NativeCodeGenerator::WorkItemExceedsJITLimits(CodeGenWorkItem *const codeGenWork)
{LOGMEIN("NativeCodeGenerator.cpp] 1766\n");
    return
        (codeGenWork->GetScriptContext()->GetThreadContext()->GetCodeSize() >= Js::Constants::MaxThreadJITCodeHeapSize) ||
        (ThreadContext::GetProcessCodeSize() >= Js::Constants::MaxProcessJITCodeHeapSize) ||
        (codeGenWork->GetByteCodeCount() >= (uint)CONFIG_FLAG(MaxJITFunctionBytecodeSize));
}
bool
NativeCodeGenerator::Process(JsUtil::Job *const job, JsUtil::ParallelThreadData *threadData)
{LOGMEIN("NativeCodeGenerator.cpp] 1774\n");
    const bool foreground = !threadData;
    PageAllocator *pageAllocator;
    if (foreground)
    {LOGMEIN("NativeCodeGenerator.cpp] 1778\n");
        pageAllocator = scriptContext->GetThreadContext()->GetPageAllocator();
    }
    else
    {
        pageAllocator = threadData->GetPageAllocator();
    }

    CodeGenWorkItem *const codeGenWork = static_cast<CodeGenWorkItem *>(job);

    switch (codeGenWork->Type())
    {LOGMEIN("NativeCodeGenerator.cpp] 1789\n");
    case JsLoopBodyWorkItemType:
    {LOGMEIN("NativeCodeGenerator.cpp] 1791\n");
        JsLoopBodyCodeGen* loopBodyCodeGenWorkItem = (JsLoopBodyCodeGen*)codeGenWork;
        Js::FunctionBody* fn = loopBodyCodeGenWorkItem->GetFunctionBody();

        if (fn->GetNativeEntryPointUsed() && fn->GetCanReleaseLoopHeaders()
#ifdef ASMJS_PLAT
            && (!fn->GetIsAsmJsFunction() || !(loopBodyCodeGenWorkItem->loopHeader->GetCurrentEntryPointInfo()->GetIsTJMode()))
#endif
        )
        {LOGMEIN("NativeCodeGenerator.cpp] 1800\n");
            loopBodyCodeGenWorkItem->loopHeader->ResetInterpreterCount();
            return false;
        }
        // Unless we're in a ForceNative configuration, ignore this workitem if it exceeds JIT limits
        if (fn->ForceJITLoopBody() || !WorkItemExceedsJITLimits(codeGenWork))
        {
            CodeGen(pageAllocator, codeGenWork, foreground);
            return true;
        }
        Js::EntryPointInfo * entryPoint = loopBodyCodeGenWorkItem->GetEntryPoint();
        entryPoint->SetJITCapReached();
        return false;
    }
    case JsFunctionType:
    {LOGMEIN("NativeCodeGenerator.cpp] 1815\n");
        // Unless we're in a ForceNative configuration, ignore this workitem if it exceeds JIT limits
        if (IS_PREJIT_ON() || Js::Configuration::Global.flags.ForceNative || !WorkItemExceedsJITLimits(codeGenWork))
        {
            CodeGen(pageAllocator, codeGenWork, foreground);
            return true;
        }
#if ENABLE_DEBUG_CONFIG_OPTIONS
        job->failureReason = Job::FailureReason::ExceedJITLimit;
#endif
        return false;
    }

    default:
        Assume(UNREACHED);
    }
#if ENABLE_DEBUG_CONFIG_OPTIONS
    job->failureReason = Job::FailureReason::Unknown;
#endif
    return false;
}

void
NativeCodeGenerator::Prioritize(JsUtil::Job *const job, const bool forceAddJobToProcessor, void* function)
{LOGMEIN("NativeCodeGenerator.cpp] 1839\n");
    // This function is called from inside the lock

    ASSERT_THREAD();
    Assert(job);
    Assert(static_cast<const CodeGenWorkItem *>(job)->Type() == CodeGenWorkItemType::JsFunctionType);
    Assert(!WasAddedToJobProcessor(job));

    JsFunctionCodeGen *const workItem = static_cast<JsFunctionCodeGen *>(job);
    Js::FunctionBody *const functionBody = workItem->GetFunctionBody();
    Assert(workItem->GetEntryPoint() == functionBody->GetDefaultFunctionEntryPointInfo());

    ExecutionMode jitMode;
    if (functionBody->GetIsAsmjsMode())
    {LOGMEIN("NativeCodeGenerator.cpp] 1853\n");
        jitMode = ExecutionMode::FullJit;
        functionBody->SetExecutionMode(ExecutionMode::FullJit);
    }
    else
    {
        if(!forceAddJobToProcessor && !functionBody->TryTransitionToJitExecutionMode())
        {LOGMEIN("NativeCodeGenerator.cpp] 1860\n");
            return;
        }

        jitMode = functionBody->GetExecutionMode();
        Assert(jitMode == ExecutionMode::SimpleJit || jitMode == ExecutionMode::FullJit);
    }

    workItems.Unlink(workItem);
    workItem->SetJitMode(jitMode);
    try
    {
        // Prioritize full JIT work items over simple JIT work items. This simple solution seems sufficient for now, but it
        // might be better to use a priority queue if it becomes necessary to prioritize recent simple JIT work items relative
        // to the older simple JIT work items.
        AddToJitQueue(
            workItem,
            jitMode == ExecutionMode::FullJit || queuedFullJitWorkItemCount == 0 /* prioritize */,
            false /* lock */,
            function);
    }
    catch (...)
    {LOGMEIN("NativeCodeGenerator.cpp] 1882\n");
        // Add the item back to the list if AddToJitQueue throws. The position in the list is not important.
        workItem->ResetJitMode();
        workItems.LinkToEnd(workItem);
        throw;
    }
}

ExecutionMode NativeCodeGenerator::PrejitJitMode(Js::FunctionBody *const functionBody)
{LOGMEIN("NativeCodeGenerator.cpp] 1891\n");
    Assert(IS_PREJIT_ON() || functionBody->GetIsAsmjsMode());
    Assert(functionBody->DoSimpleJit() || !PHASE_OFF(Js::FullJitPhase, functionBody));

    // Prefer full JIT for prejitting unless it's off or simple JIT is forced
    return
        !PHASE_OFF(Js::FullJitPhase, functionBody) && !(PHASE_FORCE(Js::Phase::SimpleJitPhase, functionBody) && functionBody->DoSimpleJit())
            ? ExecutionMode::FullJit
            : ExecutionMode::SimpleJit;
}

void
NativeCodeGenerator::UpdateQueueForDebugMode()
{LOGMEIN("NativeCodeGenerator.cpp] 1904\n");
    Assert(!this->hasUpdatedQForDebugMode);

    // If we're going to debug mode, drain the job processors queue of
    // all jobs belonging this native code generator
    // JobProcessed will be called for existing jobs, and in debug mode
    // that method will simply add them back to the NativeCodeGen's queue
    Processor()->RemoveManager(this);

    this->hasUpdatedQForDebugMode = true;

    if (Js::Configuration::Global.EnableJitInDebugMode())
    {LOGMEIN("NativeCodeGenerator.cpp] 1916\n");
        Processor()->AddManager(this);
    }
}

void
NativeCodeGenerator::JobProcessed(JsUtil::Job *const job, const bool succeeded)
{LOGMEIN("NativeCodeGenerator.cpp] 1923\n");
    // This function is called from inside the lock

    Assert(job);

    CodeGenWorkItem *workItem = static_cast<CodeGenWorkItem *>(job);

    class AutoCleanup
    {
    private:
        Js::ScriptContext *const scriptContext;
        Js::CodeGenRecyclableData *const recyclableData;

    public:
        AutoCleanup(Js::ScriptContext *const scriptContext, Js::CodeGenRecyclableData *const recyclableData)
            : scriptContext(scriptContext), recyclableData(recyclableData)
        {LOGMEIN("NativeCodeGenerator.cpp] 1939\n");
            Assert(scriptContext);
        }

        ~AutoCleanup()
        {LOGMEIN("NativeCodeGenerator.cpp] 1944\n");
            if(recyclableData)
            {LOGMEIN("NativeCodeGenerator.cpp] 1946\n");
                scriptContext->GetThreadContext()->UnregisterCodeGenRecyclableData(recyclableData);
            }
        }
    } autoCleanup(scriptContext, workItem->RecyclableData());

    const ExecutionMode jitMode = workItem->GetJitMode();
    if(jitMode == ExecutionMode::FullJit && workItem->IsInJitQueue())
    {LOGMEIN("NativeCodeGenerator.cpp] 1954\n");
        QueuedFullJitWorkItem *const queuedFullJitWorkItem = workItem->GetQueuedFullJitWorkItem();
        if(queuedFullJitWorkItem)
        {LOGMEIN("NativeCodeGenerator.cpp] 1957\n");
            queuedFullJitWorkItems.Unlink(queuedFullJitWorkItem);
            --queuedFullJitWorkItemCount;
        }
    }

    Js::FunctionBody* functionBody = nullptr;
    CodeGenWorkItemType workitemType = workItem->Type();

    if (workitemType == JsFunctionType)
    {LOGMEIN("NativeCodeGenerator.cpp] 1967\n");
        JsFunctionCodeGen * functionCodeGen = (JsFunctionCodeGen *)workItem;
        functionBody = functionCodeGen->GetFunctionBody();

        if (succeeded)
        {LOGMEIN("NativeCodeGenerator.cpp] 1972\n");
            Js::FunctionEntryPointInfo* entryPointInfo = static_cast<Js::FunctionEntryPointInfo*>(functionCodeGen->GetEntryPoint());
            entryPointInfo->SetJitMode(jitMode);
            Assert(workItem->GetCodeAddress() != NULL);
            entryPointInfo->SetCodeGenDone();
        }
        else
        {
#if DBG
            functionBody->m_nativeEntryPointIsInterpreterThunk = true;
#endif
            // It's okay if the entry point has been reclaimed at this point
            // since the job failed anyway so the entry point should never get used
            // If it's still around, clean it up. If not, its finalizer would clean
            // it up anyway.
            Js::EntryPointInfo* entryPointInfo = functionCodeGen->GetEntryPoint();

            if (entryPointInfo)
            {LOGMEIN("NativeCodeGenerator.cpp] 1990\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
                switch (job->failureReason)
                {LOGMEIN("NativeCodeGenerator.cpp] 1993\n");
                case Job::FailureReason::OOM: entryPointInfo->SetCleanupReason(Js::EntryPointInfo::CleanupReason::CodeGenFailedOOM); break;
                case Job::FailureReason::StackOverflow: entryPointInfo->SetCleanupReason(Js::EntryPointInfo::CleanupReason::CodeGenFailedStackOverflow); break;
                case Job::FailureReason::Aborted: entryPointInfo->SetCleanupReason(Js::EntryPointInfo::CleanupReason::CodeGenFailedAborted); break;
                case Job::FailureReason::ExceedJITLimit: entryPointInfo->SetCleanupReason(Js::EntryPointInfo::CleanupReason::CodeGenFailedExceedJITLimit); break;
                case Job::FailureReason::Unknown: entryPointInfo->SetCleanupReason(Js::EntryPointInfo::CleanupReason::CodeGenFailedUnknown); break;
                default: Assert(job->failureReason == Job::FailureReason::NotFailed);
                }
#endif

                entryPointInfo->SetPendingCleanup();
            }

            functionCodeGen->OnWorkItemProcessFail(this);
        }

        InterlockedDecrement(&pendingCodeGenWorkItems);
        HeapDelete(functionCodeGen);
    }
    else if (workitemType == JsLoopBodyWorkItemType)
    {LOGMEIN("NativeCodeGenerator.cpp] 2013\n");
        JsLoopBodyCodeGen * loopBodyCodeGen = (JsLoopBodyCodeGen*)workItem;
        functionBody = loopBodyCodeGen->GetFunctionBody();
        Js::EntryPointInfo * entryPoint = loopBodyCodeGen->GetEntryPoint();

        if (succeeded)
        {LOGMEIN("NativeCodeGenerator.cpp] 2019\n");
            Assert(workItem->GetCodeAddress() != NULL);

            uint loopNum = loopBodyCodeGen->GetJITData()->loopNumber;
            functionBody->SetLoopBodyEntryPoint(loopBodyCodeGen->loopHeader, entryPoint, (Js::JavascriptMethod)workItem->GetCodeAddress(), loopNum);
            entryPoint->SetCodeGenDone();
        }
        else
        {
            // We re-use failed loop body entry points.
            // The loop body entry point could have been cleaned up if the parent function JITed,
            // in which case we don't want to reset it.
            if (entryPoint && !entryPoint->IsCleanedUp())
            {LOGMEIN("NativeCodeGenerator.cpp] 2032\n");
                entryPoint->Reset(!entryPoint->IsJITCapReached()); // reset state to NotScheduled if JIT cap hasn't been reached
            }
            loopBodyCodeGen->OnWorkItemProcessFail(this);
        }
        HeapDelete(loopBodyCodeGen);
    }
    else
    {
        AssertMsg(false, "Unknown work item type");
        AssertMsg(workItem->GetCodeAddress() == NULL, "No other types should have native entry point for now.");
    }
}

void
NativeCodeGenerator::UpdateJITState()
{LOGMEIN("NativeCodeGenerator.cpp] 2048\n");
    if (JITManager::GetJITManager()->IsOOPJITEnabled())
    {LOGMEIN("NativeCodeGenerator.cpp] 2050\n");
        // TODO: OOP JIT, move server calls to background thread to reduce foreground thread delay
        if (!scriptContext->GetRemoteScriptAddr())
        {LOGMEIN("NativeCodeGenerator.cpp] 2053\n");
            return;
        }

        if (scriptContext->GetThreadContext()->JITNeedsPropUpdate())
        {LOGMEIN("NativeCodeGenerator.cpp] 2058\n");
            typedef BVSparseNode<JitArenaAllocator> BVSparseNode;
            CompileAssert(sizeof(BVSparseNode) == sizeof(BVSparseNodeIDL));
            BVSparseNodeIDL * bvHead = (BVSparseNodeIDL*)scriptContext->GetThreadContext()->GetJITNumericProperties()->head;
            HRESULT hr = JITManager::GetJITManager()->UpdatePropertyRecordMap(scriptContext->GetThreadContext()->GetRemoteThreadContextAddr(), bvHead);

            JITManager::HandleServerCallResult(hr, RemoteCallType::StateUpdate);
            scriptContext->GetThreadContext()->ResetJITNeedsPropUpdate();
        }
    }
}

JsUtil::Job *
NativeCodeGenerator::GetJobToProcessProactively()
{LOGMEIN("NativeCodeGenerator.cpp] 2072\n");
    ASSERT_THREAD();

    // Look for work, starting with high priority items first, and above LowPri
    CodeGenWorkItem* workItem = workItems.Head();
    while(workItem != nullptr)
    {LOGMEIN("NativeCodeGenerator.cpp] 2078\n");
        if(workItem->ShouldSpeculativelyJit(this->byteCodeSizeGenerated))
        {LOGMEIN("NativeCodeGenerator.cpp] 2080\n");
            workItem->SetJitMode(ExecutionMode::FullJit);

            // Note: This gives a perf regression in fre build, but it is useful for debugging and won't be there for the final build
            //   anyway, so I left it in.
            if (PHASE_TRACE(Js::DelayPhase, workItem->GetFunctionBody())) {LOGMEIN("NativeCodeGenerator.cpp] 2085\n");
                OUTPUT_TRACE(Js::DelayPhase, _u("ScriptContext: 0x%p, Speculative JIT: %-25s, Byte code generated: %d \n"),
                    this->scriptContext, workItem->GetFunctionBody()->GetExternalDisplayName(), this->byteCodeSizeGenerated);
            }
            Js::FunctionBody *fn = workItem->GetFunctionBody();
            Js::EntryPointInfo *entryPoint = workItem->GetEntryPoint();
            const auto recyclableData = GatherCodeGenData(fn, fn, entryPoint, workItem);

            workItems.Unlink(workItem);
            workItem->SetRecyclableData(recyclableData);

            {LOGMEIN("NativeCodeGenerator.cpp] 2096\n");
                AutoOptionalCriticalSection lock(Processor()->GetCriticalSection());
                scriptContext->GetThreadContext()->RegisterCodeGenRecyclableData(recyclableData);
            }
#ifdef BGJIT_STATS
            scriptContext->speculativeJitCount++;
#endif

            QueuedFullJitWorkItem *const queuedFullJitWorkItem = workItem->EnsureQueuedFullJitWorkItem();
            if(queuedFullJitWorkItem) // ignore OOM, this work item just won't be removed from the job processor's queue
            {LOGMEIN("NativeCodeGenerator.cpp] 2106\n");
                queuedFullJitWorkItems.LinkToBeginning(queuedFullJitWorkItem);
                ++queuedFullJitWorkItemCount;
            }
            workItem->OnAddToJitQueue();

            workItem->GetFunctionBody()->TraceExecutionMode("SpeculativeJit (before)");
            workItem->GetFunctionBody()->TransitionToFullJitExecutionMode();
            workItem->GetFunctionBody()->TraceExecutionMode("SpeculativeJit");
            break;
        }
        workItem = static_cast<CodeGenWorkItem*>(workItem->Next());
    }
    return workItem;
}

// Removes all of the proactive jobs from the generator.  Used when switching between attached/detached
// debug modes in order to drain the queue of jobs (since we switch from interpreted to native and back).
void
NativeCodeGenerator::RemoveProactiveJobs()
{LOGMEIN("NativeCodeGenerator.cpp] 2126\n");
    CodeGenWorkItem* workItem = workItems.Head();
    while (workItem)
    {LOGMEIN("NativeCodeGenerator.cpp] 2129\n");
        CodeGenWorkItem* temp = static_cast<CodeGenWorkItem*>(workItem->Next());
        workItem->Delete();
        workItem = temp;
    }
    workItems.Clear();

    //for(JsUtil::Job *job = workItems.Head(); job;)
    //{
    //    JsUtil::Job *const next = job->Next();
    //    JobProcessed(job, /*succeeded*/ false);
    //    job = next;
    //}
}

template<bool IsInlinee>
void
NativeCodeGenerator::GatherCodeGenData(
    Recycler *const recycler,
    Js::FunctionBody *const topFunctionBody,
    Js::FunctionBody *const functionBody,
    Js::EntryPointInfo *const entryPoint,
    InliningDecider &inliningDecider,
    ObjTypeSpecFldInfoList *objTypeSpecFldInfoList,
    Js::FunctionCodeGenJitTimeData *const jitTimeData,
    Js::FunctionCodeGenRuntimeData *const runtimeData,
    Js::JavascriptFunction* function,
    bool isJitTimeDataComputed,
    uint32 recursiveInlineDepth)
{LOGMEIN("NativeCodeGenerator.cpp] 2158\n");
    ASSERT_THREAD();
    Assert(recycler);
    Assert(functionBody);
    Assert(jitTimeData);
    Assert(IsInlinee == !!runtimeData);
    Assert(!IsInlinee || (!inliningDecider.GetIsLoopBody() || !PHASE_OFF(Js::InlineInJitLoopBodyPhase, topFunctionBody)));
    Assert(topFunctionBody != nullptr && (!entryPoint->GetWorkItem() || entryPoint->GetWorkItem()->GetFunctionBody() == topFunctionBody));
    Assert(objTypeSpecFldInfoList != nullptr);

#ifdef FIELD_ACCESS_STATS
    jitTimeData->EnsureInlineCacheStats(recycler);
#define SetInlineCacheCount(counter, value)  jitTimeData->inlineCacheStats->counter = value;
#define IncInlineCacheCount(counter) if(!isJitTimeDataComputed) {LOGMEIN("NativeCodeGenerator.cpp] 2171\n");jitTimeData->inlineCacheStats->counter++;}
#define AddInlineCacheStats(callerData, inlineeData) callerData->AddInlineeInlineCacheStats(inlineeData);
#define InlineCacheStatsArg(jitTimeData) !isJitTimeDataComputed ? jitTimeData->inlineCacheStats : nullptr
#else
#define SetInlineCacheCount(counter, value)
#define IncInlineCacheCount(counter)
#define AddInlineCacheStats(callerData, inlineeData)
#define InlineCacheStatsArg(jitTimeData) nullptr
#endif

#if DBG
    Assert(
        PHASE_ON(Js::Phase::SimulatePolyCacheWithOneTypeForFunctionPhase, functionBody) ==
        CONFIG_ISENABLED(Js::Flag::SimulatePolyCacheWithOneTypeForInlineCacheIndexFlag));
    if(PHASE_ON(Js::Phase::SimulatePolyCacheWithOneTypeForFunctionPhase, functionBody))
    {LOGMEIN("NativeCodeGenerator.cpp] 2186\n");
        const Js::InlineCacheIndex inlineCacheIndex = CONFIG_FLAG(SimulatePolyCacheWithOneTypeForInlineCacheIndex);
        functionBody->CreateNewPolymorphicInlineCache(
            inlineCacheIndex,
            functionBody->GetPropertyIdFromCacheId(inlineCacheIndex),
            functionBody->GetInlineCache(inlineCacheIndex));
        if(functionBody->HasDynamicProfileInfo())
        {LOGMEIN("NativeCodeGenerator.cpp] 2193\n");
            functionBody->GetAnyDynamicProfileInfo()->RecordPolymorphicFieldAccess(functionBody, inlineCacheIndex);
        }
    }
#endif

    if(IsInlinee)
    {
        // This function is recursive
        PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);
    }
    else
    {
        //TryAggressiveInlining adjusts inlining heuristics and walks the call tree. If it can inlining everything it will set the InliningThreshold to be aggressive.
        if (!inliningDecider.GetIsLoopBody())
        {LOGMEIN("NativeCodeGenerator.cpp] 2208\n");
            uint32 inlineeCount = 0;
            if (!PHASE_OFF(Js::TryAggressiveInliningPhase, topFunctionBody))
            {LOGMEIN("NativeCodeGenerator.cpp] 2211\n");
                Assert(topFunctionBody == functionBody);
                inliningDecider.SetAggressiveHeuristics();
                if (!TryAggressiveInlining(topFunctionBody, functionBody, inliningDecider, inlineeCount, 0))
                {LOGMEIN("NativeCodeGenerator.cpp] 2215\n");
                    uint countOfInlineesWithLoops = inliningDecider.GetNumberOfInlineesWithLoop();
                    //TryAggressiveInlining failed, set back to default heuristics.
                    inliningDecider.ResetInlineHeuristics();
                    inliningDecider.SetLimitOnInlineesWithLoop(countOfInlineesWithLoops);
                }
                else
                {
                    jitTimeData->SetIsAggressiveInliningEnabled();
                }
                inliningDecider.ResetState();
            }
        }
        entryPoint->EnsurePolymorphicInlineCacheInfo(recycler, functionBody);
    }

    entryPoint->EnsureJitTransferData(recycler);
#if ENABLE_DEBUG_CONFIG_OPTIONS
    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

#if ENABLE_DEBUG_CONFIG_OPTIONS
    if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_VERBOSE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
    {LOGMEIN("NativeCodeGenerator.cpp] 2238\n");
        Output::Print(_u("ObjTypeSpec: top function %s (%s), function %s (%s): GatherCodeGenData(): \n"),
            topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer));
        Output::Flush();
    }
#endif
    const auto profileData =
        functionBody->HasDynamicProfileInfo()
            ? functionBody->GetAnyDynamicProfileInfo()
            : functionBody->EnsureDynamicProfileInfo();

    bool inlineGetterSetter = false;
    bool inlineApplyTarget = false; //to indicate whether we can inline apply target or not.
    bool inlineCallTarget = false;
    if(profileData)
    {LOGMEIN("NativeCodeGenerator.cpp] 2253\n");
        if (!IsInlinee)
        {LOGMEIN("NativeCodeGenerator.cpp] 2255\n");
            PHASE_PRINT_TRACE(
                Js::ObjTypeSpecPhase, functionBody,
                _u("Objtypespec (%s): Pending cache state on add %x to JIT queue: %d\n"),
                functionBody->GetDebugNumberSet(debugStringBuffer), entryPoint, profileData->GetPolymorphicCacheState());

            entryPoint->SetPendingPolymorphicCacheState(profileData->GetPolymorphicCacheState());
            entryPoint->SetPendingInlinerVersion(profileData->GetInlinerVersion());
            entryPoint->SetPendingImplicitCallFlags(profileData->GetImplicitCallFlags());
        }

        if (functionBody->GetProfiledArrayCallSiteCount() != 0)
        {LOGMEIN("NativeCodeGenerator.cpp] 2267\n");
            RecyclerWeakReference<Js::FunctionBody> *weakFuncRef = recycler->CreateWeakReferenceHandle(functionBody);
            if (!isJitTimeDataComputed)
            {LOGMEIN("NativeCodeGenerator.cpp] 2270\n");
                jitTimeData->SetWeakFuncRef(weakFuncRef);
            }
            entryPoint->AddWeakFuncRef(weakFuncRef, recycler);
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (PHASE_VERBOSE_TESTTRACE(Js::ObjTypeSpecPhase, functionBody) ||
            PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
        {LOGMEIN("NativeCodeGenerator.cpp] 2279\n");
            if (functionBody->GetInlineCacheCount() > 0)
            {LOGMEIN("NativeCodeGenerator.cpp] 2281\n");
                if (!IsInlinee)
                {LOGMEIN("NativeCodeGenerator.cpp] 2283\n");
                    Output::Print(_u("-----------------------------------------------------------------------------\n"));
                }
                else
                {
                    Output::Print(_u("\tInlinee:\t"));
                }
                functionBody->DumpFullFunctionName();
                Output::Print(_u("\n"));
            }
        }
#endif

        SetInlineCacheCount(totalInlineCacheCount, functionBody->GetInlineCacheCount());

        Assert(functionBody->GetProfiledFldCount() == functionBody->GetInlineCacheCount()); // otherwise, isInst inline caches need to be cloned
        for(uint i = 0; i < functionBody->GetInlineCacheCount(); ++i)
        {LOGMEIN("NativeCodeGenerator.cpp] 2300\n");
            const auto cacheType = profileData->GetFldInfo(functionBody, i)->flags;

            PHASE_PRINT_VERBOSE_TESTTRACE(
                Js::ObjTypeSpecPhase, functionBody,
                _u("Cache #%3d, Layout: %s, Profile info: %s\n"),
                i,
                functionBody->GetInlineCache(i)->LayoutString(),
                cacheType == Js::FldInfo_NoInfo ? _u("none") :
                (cacheType & Js::FldInfo_Polymorphic) ? _u("polymorphic") : _u("monomorphic"));

            if (cacheType == Js::FldInfo_NoInfo)
            {LOGMEIN("NativeCodeGenerator.cpp] 2312\n");
                IncInlineCacheCount(noInfoInlineCacheCount);
                continue;
            }

            Js::PolymorphicInlineCache * polymorphicCacheOnFunctionBody = functionBody->GetPolymorphicInlineCache(i);
            bool isPolymorphic = (cacheType & Js::FldInfo_Polymorphic) != 0;
            if (!isPolymorphic)
            {LOGMEIN("NativeCodeGenerator.cpp] 2320\n");
                Js::InlineCache *inlineCache;
                if(function && Js::ScriptFunctionWithInlineCache::Is(function))
                {LOGMEIN("NativeCodeGenerator.cpp] 2323\n");
                    inlineCache = Js::ScriptFunctionWithInlineCache::FromVar(function)->GetInlineCache(i);
                }
                else
                {
                    inlineCache = functionBody->GetInlineCache(i);
                }

                Js::ObjTypeSpecFldInfo* objTypeSpecFldInfo = nullptr;

#if ENABLE_DEBUG_CONFIG_OPTIONS
                if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_VERBOSE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
                {LOGMEIN("NativeCodeGenerator.cpp] 2335\n");
                    char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(i);
                    Js::PropertyRecord const * const propertyRecord = functionBody->GetScriptContext()->GetPropertyName(propertyId);
                    Output::Print(_u("ObTypeSpec: top function %s (%s), function %s (%s): cloning mono cache for %s (#%d) cache %d \n"),
                        topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer),
                        functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer2), propertyRecord->GetBuffer(), propertyId, i);
                    Output::Flush();
                }
#endif

                IncInlineCacheCount(monoInlineCacheCount);

                if (inlineCache->IsEmpty())
                {LOGMEIN("NativeCodeGenerator.cpp] 2349\n");
                    IncInlineCacheCount(emptyMonoInlineCacheCount);
                }

                if(!PHASE_OFF(Js::ObjTypeSpecPhase, functionBody) || !PHASE_OFF(Js::FixedMethodsPhase, functionBody) || !PHASE_OFF(Js::UseFixedDataPropsPhase, functionBody))
                {LOGMEIN("NativeCodeGenerator.cpp] 2354\n");
                    if(cacheType & (Js::FldInfo_FromLocal | Js::FldInfo_FromLocalWithoutProperty | Js::FldInfo_FromProto))
                    {LOGMEIN("NativeCodeGenerator.cpp] 2356\n");
                        // WinBlue 170722: Disable ObjTypeSpec optimization for activation object in debug mode,
                        // as it can result in BailOutFailedTypeCheck before locals are set to undefined,
                        // which can result in using garbage object during bailout/restore values.
                        if (!(functionBody->IsInDebugMode() && inlineCache->GetType() &&
                              inlineCache->GetType()->GetTypeId() == Js::TypeIds_ActivationObject))
                        {LOGMEIN("NativeCodeGenerator.cpp] 2362\n");
                            objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), inlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                            if (objTypeSpecFldInfo)
                            {LOGMEIN("NativeCodeGenerator.cpp] 2365\n");
                                IncInlineCacheCount(clonedMonoInlineCacheCount);

                                if (!PHASE_OFF(Js::InlineApplyTargetPhase, functionBody) && (cacheType & Js::FldInfo_InlineCandidate))
                                {LOGMEIN("NativeCodeGenerator.cpp] 2369\n");
                                    if (IsInlinee || objTypeSpecFldInfo->IsBuiltin())
                                    {LOGMEIN("NativeCodeGenerator.cpp] 2371\n");
                                        inlineApplyTarget = true;
                                    }
                                }

                                if (!PHASE_OFF(Js::InlineCallTargetPhase, functionBody) && (cacheType & Js::FldInfo_InlineCandidate))
                                {LOGMEIN("NativeCodeGenerator.cpp] 2377\n");
                                    inlineCallTarget = true;
                                }
                                if (!isJitTimeDataComputed)
                                {LOGMEIN("NativeCodeGenerator.cpp] 2381\n");
                                    jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                    objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                                }
                            }
                        }
                    }
                }
                if(!PHASE_OFF(Js::FixAccessorPropsPhase, functionBody))
                {LOGMEIN("NativeCodeGenerator.cpp] 2390\n");
                    if (!objTypeSpecFldInfo && (cacheType & Js::FldInfo_FromAccessor) && (cacheType & Js::FldInfo_InlineCandidate))
                    {LOGMEIN("NativeCodeGenerator.cpp] 2392\n");
                        objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), inlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                        if (objTypeSpecFldInfo)
                        {LOGMEIN("NativeCodeGenerator.cpp] 2395\n");
                            inlineGetterSetter = true;
                            if (!isJitTimeDataComputed)
                            {LOGMEIN("NativeCodeGenerator.cpp] 2398\n");
                                IncInlineCacheCount(clonedMonoInlineCacheCount);
                                jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                            }
                        }

                    }
                }
                if (!PHASE_OFF(Js::RootObjectFldFastPathPhase, functionBody))
                {LOGMEIN("NativeCodeGenerator.cpp] 2408\n");
                    if (i >= functionBody->GetRootObjectLoadInlineCacheStart() && inlineCache->IsLocal())
                    {LOGMEIN("NativeCodeGenerator.cpp] 2410\n");
                        void * rawType = inlineCache->u.local.type;
                        Js::Type * type = TypeWithoutAuxSlotTag(rawType);
                        Js::RootObjectBase * rootObject = functionBody->GetRootObject();
                        if (rootObject->GetType() == type)
                        {LOGMEIN("NativeCodeGenerator.cpp] 2415\n");
                            Js::BigPropertyIndex propertyIndex = inlineCache->u.local.slotIndex;
                            if (rawType == type)
                            {LOGMEIN("NativeCodeGenerator.cpp] 2418\n");
                                // type is not tagged, inline slot
                                propertyIndex = rootObject->GetPropertyIndexFromInlineSlotIndex(inlineCache->u.local.slotIndex);
                            }
                            else
                            {
                                propertyIndex = rootObject->GetPropertyIndexFromAuxSlotIndex(inlineCache->u.local.slotIndex);
                            }
                            Js::PropertyAttributes attributes;
                            if (rootObject->GetAttributesWithPropertyIndex(functionBody->GetPropertyIdFromCacheId(i), propertyIndex, &attributes)
                                && (attributes & PropertyConfigurable) == 0
                                && !isJitTimeDataComputed)
                            {LOGMEIN("NativeCodeGenerator.cpp] 2430\n");
                                // non configurable
                                if (objTypeSpecFldInfo == nullptr)
                                {LOGMEIN("NativeCodeGenerator.cpp] 2433\n");
                                    objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), inlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                                    if (objTypeSpecFldInfo)
                                    {LOGMEIN("NativeCodeGenerator.cpp] 2436\n");
                                        IncInlineCacheCount(clonedMonoInlineCacheCount);
                                        jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                        objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                                    }
                                }
                                if (objTypeSpecFldInfo != nullptr)
                                {LOGMEIN("NativeCodeGenerator.cpp] 2443\n");
                                    objTypeSpecFldInfo->SetRootObjectNonConfigurableField(i < functionBody->GetRootObjectStoreInlineCacheStart());
                                }
                            }
                        }
                    }
                }
            }
            // Even if the FldInfo says that the field access may be polymorphic, be optimistic that if the function object has inline caches, they'll be monomorphic
            else if(function && Js::ScriptFunctionWithInlineCache::Is(function) && (cacheType & Js::FldInfo_InlineCandidate || !polymorphicCacheOnFunctionBody))
            {LOGMEIN("NativeCodeGenerator.cpp] 2453\n");
                Js::InlineCache *inlineCache = Js::ScriptFunctionWithInlineCache::FromVar(function)->GetInlineCache(i);
                Js::ObjTypeSpecFldInfo* objTypeSpecFldInfo = nullptr;

                if(!PHASE_OFF(Js::ObjTypeSpecPhase, functionBody) || !PHASE_OFF(Js::FixedMethodsPhase, functionBody))
                {LOGMEIN("NativeCodeGenerator.cpp] 2458\n");
                    if(cacheType & (Js::FldInfo_FromLocal | Js::FldInfo_FromProto))  // Remove FldInfo_FromLocal?
                    {LOGMEIN("NativeCodeGenerator.cpp] 2460\n");

                        // WinBlue 170722: Disable ObjTypeSpec optimization for activation object in debug mode,
                        // as it can result in BailOutFailedTypeCheck before locals are set to undefined,
                        // which can result in using garbage object during bailout/restore values.
                        if (!(functionBody->IsInDebugMode() && inlineCache->GetType() &&
                              inlineCache->GetType()->GetTypeId() == Js::TypeIds_ActivationObject))
                        {LOGMEIN("NativeCodeGenerator.cpp] 2467\n");
                            objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), inlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                            if (objTypeSpecFldInfo)
                            {LOGMEIN("NativeCodeGenerator.cpp] 2470\n");
                                IncInlineCacheCount(clonedMonoInlineCacheCount);

                                if (!PHASE_OFF(Js::InlineApplyTargetPhase, functionBody) && IsInlinee && (cacheType & Js::FldInfo_InlineCandidate))
                                {LOGMEIN("NativeCodeGenerator.cpp] 2474\n");
                                    inlineApplyTarget = true;
                                }
                                if (!isJitTimeDataComputed)
                                {LOGMEIN("NativeCodeGenerator.cpp] 2478\n");
                                    jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                    objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                const auto polymorphicInlineCache = functionBody->GetPolymorphicInlineCache(i);

                if (polymorphicInlineCache != nullptr)
                {LOGMEIN("NativeCodeGenerator.cpp] 2492\n");
                    IncInlineCacheCount(polyInlineCacheCount);
                    if (profileData->GetFldInfo(functionBody, i)->ShouldUsePolymorphicInlineCache())
                    {LOGMEIN("NativeCodeGenerator.cpp] 2495\n");
                        IncInlineCacheCount(highUtilPolyInlineCacheCount);
                    }
                    else
                    {
                        IncInlineCacheCount(lowUtilPolyInlineCacheCount);
                    }

                    if (!PHASE_OFF(Js::EquivObjTypeSpecPhase, topFunctionBody) && !topFunctionBody->GetAnyDynamicProfileInfo()->IsEquivalentObjTypeSpecDisabled())
                    {LOGMEIN("NativeCodeGenerator.cpp] 2504\n");
                        if (!polymorphicInlineCache->GetIgnoreForEquivalentObjTypeSpec() || (polymorphicInlineCache->GetCloneForJitTimeUse() && !PHASE_OFF(Js::PolymorphicInlinePhase, functionBody) && !PHASE_OFF(Js::PolymorphicInlineFixedMethodsPhase, functionBody)))
                        {LOGMEIN("NativeCodeGenerator.cpp] 2506\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
                            if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_VERBOSE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
                            {LOGMEIN("NativeCodeGenerator.cpp] 2509\n");
                                char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

                                Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(i);
                                Js::PropertyRecord const * const propertyRecord = functionBody->GetScriptContext()->GetPropertyName(propertyId);
                                Output::Print(_u("ObTypeSpec: top function %s (%s), function %s (%s): cloning poly cache for %s (#%d) cache %d \n"),
                                    topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer),
                                    functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer2), propertyRecord->GetBuffer(), propertyId, i);
                                Output::Flush();
                            }
#endif
                            Js::ObjTypeSpecFldInfo* objTypeSpecFldInfo = Js::ObjTypeSpecFldInfo::CreateFrom(objTypeSpecFldInfoList->Count(), polymorphicInlineCache, i, entryPoint, topFunctionBody, functionBody, InlineCacheStatsArg(jitTimeData));
                            if (objTypeSpecFldInfo != nullptr)
                            {LOGMEIN("NativeCodeGenerator.cpp] 2522\n");
                                if (!isJitTimeDataComputed)
                                {LOGMEIN("NativeCodeGenerator.cpp] 2524\n");
                                    jitTimeData->GetObjTypeSpecFldInfoArray()->SetInfo(recycler, functionBody, i, objTypeSpecFldInfo);
                                    IncInlineCacheCount(clonedPolyInlineCacheCount);
                                    objTypeSpecFldInfoList->Prepend(objTypeSpecFldInfo);
                                }
                                if (!PHASE_OFF(Js::InlineAccessorsPhase, functionBody) && (cacheType & Js::FldInfo_FromAccessor) && (cacheType & Js::FldInfo_InlineCandidate))
                                {LOGMEIN("NativeCodeGenerator.cpp] 2530\n");
                                    inlineGetterSetter = true;
                                }
                            }
                        }
                        else
                        {
                            IncInlineCacheCount(ignoredPolyInlineCacheCount);
                        }
                    }
                    else
                    {
                        IncInlineCacheCount(disabledPolyInlineCacheCount);
                    }
                }
                else
                {
                    IncInlineCacheCount(nullPolyInlineCacheCount);
                }

                if (polymorphicInlineCache != nullptr)
                {LOGMEIN("NativeCodeGenerator.cpp] 2551\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
                    if (PHASE_VERBOSE_TRACE1(Js::PolymorphicInlineCachePhase))
                    {LOGMEIN("NativeCodeGenerator.cpp] 2554\n");
                        if (IsInlinee) Output::Print(_u("\t"));
                        Output::Print(_u("\t%d: PIC size = %d\n"), i, polymorphicInlineCache->GetSize());
#if DBG_DUMP
                        polymorphicInlineCache->Dump();
#endif
                    }
                    else if (PHASE_TRACE1(Js::PolymorphicInlineCachePhase))
                    {LOGMEIN("NativeCodeGenerator.cpp] 2562\n");
                        Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(i);
                        Js::PropertyRecord const * const propertyRecord = functionBody->GetScriptContext()->GetPropertyName(propertyId);
                        Output::Print(_u("Trace PIC JIT function %s (%s) field: %s (index: %d) \n"), functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer),
                            propertyRecord->GetBuffer(), i);
                    }
#endif

                    byte polyCacheUtil = profileData->GetFldInfo(functionBody, i)->polymorphicInlineCacheUtilization;
                    entryPoint->GetPolymorphicInlineCacheInfo()->SetPolymorphicInlineCache(functionBody, i, polymorphicInlineCache, IsInlinee, polyCacheUtil);
                    if (IsInlinee)
                    {LOGMEIN("NativeCodeGenerator.cpp] 2573\n");
                        Assert(entryPoint->GetPolymorphicInlineCacheInfo()->GetInlineeInfo(functionBody)->GetPolymorphicInlineCaches()->GetInlineCache(functionBody, i) == polymorphicInlineCache);
                    }
                    else
                    {
                        Assert(entryPoint->GetPolymorphicInlineCacheInfo()->GetSelfInfo()->GetPolymorphicInlineCaches()->GetInlineCache(functionBody, i) == polymorphicInlineCache);
                    }
                }
                else if(IsInlinee && CONFIG_FLAG(CloneInlinedPolymorphicCaches))
                {LOGMEIN("NativeCodeGenerator.cpp] 2582\n");
                    // Clone polymorphic inline caches for runtime usage in this inlinee. The JIT should only use the pointers to
                    // the inline caches, as their cached data is not guaranteed to be stable while jitting.
                    Js::InlineCache *const inlineCache =
                        function && Js::ScriptFunctionWithInlineCache::Is(function)
                            ? Js::ScriptFunctionWithInlineCache::FromVar(function)->GetInlineCache(i)
                            : functionBody->GetInlineCache(i);
                    Js::PropertyId propertyId = functionBody->GetPropertyIdFromCacheId(i);
                    const auto clone = runtimeData->ClonedInlineCaches()->GetInlineCache(functionBody, i);
                    if (clone)
                    {LOGMEIN("NativeCodeGenerator.cpp] 2592\n");
                        inlineCache->CopyTo(propertyId, functionBody->GetScriptContext(), clone);
                    }
                    else
                    {
                        runtimeData->ClonedInlineCaches()->SetInlineCache(
                            recycler,
                            functionBody,
                            i,
                            inlineCache->Clone(propertyId, functionBody->GetScriptContext()));
                    }
                }
            }
        }
    }

    // Gather code gen data for inlinees

    if(IsInlinee ? !inliningDecider.InlineIntoInliner(functionBody) : !inliningDecider.InlineIntoTopFunc())
    {LOGMEIN("NativeCodeGenerator.cpp] 2611\n");
        return;
    }

    class AutoCleanup
    {
    private:
        Js::FunctionBody *const functionBody;

    public:
        AutoCleanup(Js::FunctionBody *const functionBody) : functionBody(functionBody)
        {LOGMEIN("NativeCodeGenerator.cpp] 2622\n");
            functionBody->OnBeginInlineInto();
        }

        ~AutoCleanup()
        {LOGMEIN("NativeCodeGenerator.cpp] 2627\n");
            functionBody->OnEndInlineInto();
        }
    } autoCleanup(functionBody);

    const auto profiledCallSiteCount = functionBody->GetProfiledCallSiteCount();
    Assert(profiledCallSiteCount != 0 || functionBody->GetAnyDynamicProfileInfo()->HasLdFldCallSiteInfo());
    if (profiledCallSiteCount && !isJitTimeDataComputed)
    {LOGMEIN("NativeCodeGenerator.cpp] 2635\n");
        jitTimeData->inlineesBv = BVFixed::New<Recycler>(profiledCallSiteCount, recycler);
    }

    // Iterate through profiled call sites recursively and determine what should be inlined
    for(Js::ProfileId profiledCallSiteId = 0; profiledCallSiteId < profiledCallSiteCount; ++profiledCallSiteId)
    {LOGMEIN("NativeCodeGenerator.cpp] 2641\n");
        Js::FunctionInfo *const inlinee = inliningDecider.InlineCallSite(functionBody, profiledCallSiteId, recursiveInlineDepth);
        if(!inlinee)
        {LOGMEIN("NativeCodeGenerator.cpp] 2644\n");
            if (profileData->CallSiteHasProfileData(profiledCallSiteId))
            {LOGMEIN("NativeCodeGenerator.cpp] 2646\n");
                jitTimeData->inlineesBv->Set(profiledCallSiteId);
            }

            //Try and see if this polymorphic call
            Js::FunctionBody* inlineeFunctionBodyArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = {0};
            bool canInlineArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = { 0 };
            uint polyInlineeCount = inliningDecider.InlinePolymorphicCallSite(functionBody, profiledCallSiteId, inlineeFunctionBodyArray,
                Js::DynamicProfileInfo::maxPolymorphicInliningSize, canInlineArray);

            //We should be able to inline at least two functions here.
            if (polyInlineeCount >= 2)
            {LOGMEIN("NativeCodeGenerator.cpp] 2658\n");
                for (uint id = 0; id < polyInlineeCount; id++)
                {LOGMEIN("NativeCodeGenerator.cpp] 2660\n");
                    bool isInlined = canInlineArray[id];

                    Js::FunctionCodeGenRuntimeData  *inlineeRunTimeData = IsInlinee ? runtimeData->EnsureInlinee(recycler, profiledCallSiteId, inlineeFunctionBodyArray[id]) : functionBody->EnsureInlineeCodeGenRuntimeData(recycler, profiledCallSiteId, inlineeFunctionBodyArray[id]);

                    if (!isJitTimeDataComputed)
                    {LOGMEIN("NativeCodeGenerator.cpp] 2666\n");
                        Js::FunctionCodeGenJitTimeData  *inlineeJitTimeData = jitTimeData->AddInlinee(recycler, profiledCallSiteId, inlineeFunctionBodyArray[id]->GetFunctionInfo(), isInlined);
                        if (isInlined)
                        {
                            GatherCodeGenData<true>(
                                recycler,
                                topFunctionBody,
                                inlineeFunctionBodyArray[id],
                                entryPoint,
                                inliningDecider,
                                objTypeSpecFldInfoList,
                                inlineeJitTimeData,
                                inlineeRunTimeData
                                );

                            AddInlineCacheStats(jitTimeData, inlineeJitTimeData);
                        }
                    }
                }
            }
        }
        else
        {

            jitTimeData->inlineesBv->Set(profiledCallSiteId);
            Js::FunctionBody *const inlineeFunctionBody = inlinee->GetFunctionBody();
            if(!inlineeFunctionBody )
            {LOGMEIN("NativeCodeGenerator.cpp] 2693\n");
                if (!isJitTimeDataComputed)
                {LOGMEIN("NativeCodeGenerator.cpp] 2695\n");
                    jitTimeData->AddInlinee(recycler, profiledCallSiteId, inlinee);
                }
                continue;
            }

            // We are at a callsite that can be inlined. Let the callsite be foo().
            // If foo has inline caches on it, we need to be able to get those for cloning.
            // To do this,
            //  1. Retrieve the inline cache associated with the load of "foo",
            //  2. Try to get the fixed function object corresponding to "foo",
            //  3. Pass the fixed function object to GatherCodeGenData which can clone its inline caches.

            uint ldFldInlineCacheIndex = profileData->GetLdFldCacheIndexFromCallSiteInfo(functionBody, profiledCallSiteId);
            Js::InlineCache * inlineCache = nullptr;
            if ((ldFldInlineCacheIndex != Js::Constants::NoInlineCacheIndex) && (ldFldInlineCacheIndex < functionBody->GetInlineCacheCount()))
            {LOGMEIN("NativeCodeGenerator.cpp] 2711\n");
                if(function && Js::ScriptFunctionWithInlineCache::Is(function))
                {LOGMEIN("NativeCodeGenerator.cpp] 2713\n");
                    inlineCache = Js::ScriptFunctionWithInlineCache::FromVar(function)->GetInlineCache(ldFldInlineCacheIndex);
                }
                else
                {
                    inlineCache = functionBody->GetInlineCache(ldFldInlineCacheIndex);
                }
            }

            Js::JavascriptFunction* fixedFunctionObject = nullptr;
            if (inlineCache && (inlineCache->IsLocal() || inlineCache->IsProto()))
            {LOGMEIN("NativeCodeGenerator.cpp] 2724\n");
                inlineCache->TryGetFixedMethodFromCache(functionBody, ldFldInlineCacheIndex, &fixedFunctionObject);
            }

            if (fixedFunctionObject && !fixedFunctionObject->GetFunctionInfo()->IsDeferred() && fixedFunctionObject->GetFunctionBody() != inlineeFunctionBody)
            {LOGMEIN("NativeCodeGenerator.cpp] 2729\n");
                fixedFunctionObject = nullptr;
            }

            if (!PHASE_OFF(Js::InlineRecursivePhase, functionBody))
            {LOGMEIN("NativeCodeGenerator.cpp] 2734\n");
                if (!isJitTimeDataComputed)
                {LOGMEIN("NativeCodeGenerator.cpp] 2736\n");
                    Js::FunctionCodeGenRuntimeData *inlineeRuntimeData = IsInlinee ? runtimeData->EnsureInlinee(recycler, profiledCallSiteId, inlineeFunctionBody) : functionBody->EnsureInlineeCodeGenRuntimeData(recycler, profiledCallSiteId, inlineeFunctionBody);
                    Js::FunctionCodeGenJitTimeData  *inlineeJitTimeData = nullptr;
                    bool doShareJitTimeData = false;

                    // Share the jitTime data if i) it is a recursive call, ii) jitTimeData is not from a polymorphic chain, and iii) all the call sites are recursive
                    if (functionBody == inlineeFunctionBody   // recursive call
                        && jitTimeData->GetNext() == nullptr        // not from a polymorphic  call site
                        && profiledCallSiteCount == functionBody->GetNumberOfRecursiveCallSites() && !inlineGetterSetter) // all the callsites are recursive
                    {LOGMEIN("NativeCodeGenerator.cpp] 2745\n");
                        jitTimeData->SetupRecursiveInlineeChain(recycler, profiledCallSiteId);
                        inlineeJitTimeData = jitTimeData;
                        doShareJitTimeData = true;

                        // If a recursive  inliner has multiple recursive inlinees and if they hit the InlineCountMax
                        // threshold, then runtimeData for the inlinees may not be available (bug 2269097) for the inlinees
                        // as InlineCountMax threshold heuristics has higher priority than recursive inline heuristics. Since
                        // we share runtime data between recursive inliner and recursive inlinees, and all the call sites
                        // are recursive (we only do recursive inlining for functions where all the callsites are recursive),
                        // we can iterate over all the callsites of the inliner and setup the runtime data recursive inlinee chain

                        for (Js::ProfileId id = 0; id < profiledCallSiteCount; id++)
                        {LOGMEIN("NativeCodeGenerator.cpp] 2758\n");
                            inlineeRuntimeData->SetupRecursiveInlineeChain(recycler, id, inlineeFunctionBody);
                        }
                    }
                    else
                    {
                        inlineeJitTimeData = jitTimeData->AddInlinee(recycler, profiledCallSiteId, inlinee);
                    }

                    GatherCodeGenData<true>(
                        recycler,
                        topFunctionBody,
                        inlineeFunctionBody,
                        entryPoint,
                        inliningDecider,
                        objTypeSpecFldInfoList,
                        inlineeJitTimeData,
                        inlineeRuntimeData,
                        fixedFunctionObject,
                        doShareJitTimeData,
                        functionBody == inlineeFunctionBody ? recursiveInlineDepth + 1 : 0);

                    if (jitTimeData != inlineeJitTimeData)
                    {
                        AddInlineCacheStats(jitTimeData, inlineeJitTimeData);
                    }
                }
            }
            else
            {
                Js::FunctionCodeGenJitTimeData *const inlineeJitTimeData = jitTimeData->AddInlinee(recycler, profiledCallSiteId, inlinee);
                GatherCodeGenData<true>(
                    recycler,
                    topFunctionBody,
                    inlineeFunctionBody,
                    entryPoint,
                    inliningDecider,
                    objTypeSpecFldInfoList,
                    inlineeJitTimeData,
                    IsInlinee
                    ? runtimeData->EnsureInlinee(recycler, profiledCallSiteId, inlineeFunctionBody)
                    : functionBody->EnsureInlineeCodeGenRuntimeData(recycler, profiledCallSiteId, inlineeFunctionBody),
                    fixedFunctionObject);

                    AddInlineCacheStats(jitTimeData, inlineeJitTimeData);
            }
        }
    }

    // Iterate through inlineCache getter setter and apply call sites recursively and determine what should be inlined
    if (inlineGetterSetter || inlineApplyTarget || inlineCallTarget)
    {LOGMEIN("NativeCodeGenerator.cpp] 2809\n");
        for(uint inlineCacheIndex = 0; inlineCacheIndex < functionBody->GetInlineCacheCount(); ++inlineCacheIndex)
        {LOGMEIN("NativeCodeGenerator.cpp] 2811\n");
            const auto cacheType = profileData->GetFldInfo(functionBody, inlineCacheIndex)->flags;
            if(cacheType == Js::FldInfo_NoInfo)
            {LOGMEIN("NativeCodeGenerator.cpp] 2814\n");
                continue;
            }

            bool getSetInlineCandidate = inlineGetterSetter && ((cacheType & Js::FldInfo_FromAccessor) != 0);
            bool callApplyInlineCandidate = (inlineCallTarget || inlineApplyTarget) && ((cacheType & Js::FldInfo_FromAccessor) == 0);

            // 1. Do not inline if the x in a.x is both a getter/setter and is followed by a .apply
            // 2. If we were optimistic earlier in assuming that the inline caches on the function object would be monomorphic and asserted that we may possibly inline apply target,
            //    then even if the field info flags say that the field access may be polymorphic, carry that optimism forward and try to inline apply target.
            if (getSetInlineCandidate ^ callApplyInlineCandidate)
            {LOGMEIN("NativeCodeGenerator.cpp] 2825\n");
                Js::ObjTypeSpecFldInfo* info = jitTimeData->GetObjTypeSpecFldInfoArray()->GetInfo(functionBody, inlineCacheIndex);
                if (info == nullptr)
                {LOGMEIN("NativeCodeGenerator.cpp] 2828\n");
                    continue;
                }

                if (!(getSetInlineCandidate && info->UsesAccessor()) && !(callApplyInlineCandidate && !info->IsPoly()))
                {LOGMEIN("NativeCodeGenerator.cpp] 2833\n");
                    continue;
                }

                Js::JavascriptFunction* inlineeFunction = info->GetFieldValueAsFunctionIfAvailable();

                if (inlineeFunction == nullptr)
                {LOGMEIN("NativeCodeGenerator.cpp] 2840\n");
                    continue;
                }

                Js::FunctionInfo* inlineeFunctionInfo = inlineeFunction->GetFunctionInfo();

                Js::FunctionProxy* inlineeFunctionProxy = inlineeFunctionInfo->GetFunctionProxy();
                if (inlineeFunctionProxy != nullptr && !functionBody->CheckCalleeContextForInlining(inlineeFunctionProxy))
                {LOGMEIN("NativeCodeGenerator.cpp] 2848\n");
                    continue;
                }

                const auto inlinee = inliningDecider.Inline(functionBody, inlineeFunctionInfo, false /*isConstructorCall*/, false /*isPolymorphicCall*/, 0, (uint16)inlineCacheIndex, 0, false);
                if(!inlinee)
                {LOGMEIN("NativeCodeGenerator.cpp] 2854\n");
                    continue;
                }

                const auto inlineeFunctionBody = inlinee->GetFunctionBody();
                if(!inlineeFunctionBody)
                {LOGMEIN("NativeCodeGenerator.cpp] 2860\n");
                    if ((
#ifdef ENABLE_DOM_FAST_PATH
                         inlinee->GetLocalFunctionId() == Js::JavascriptBuiltInFunction::DOMFastPathGetter ||
                         inlinee->GetLocalFunctionId() == Js::JavascriptBuiltInFunction::DOMFastPathSetter ||
#endif
                         (inlineeFunctionInfo->GetAttributes() & Js::FunctionInfo::Attributes::BuiltInInlinableAsLdFldInlinee) != 0) &&
                        !isJitTimeDataComputed)
                    {LOGMEIN("NativeCodeGenerator.cpp] 2868\n");
                        jitTimeData->AddLdFldInlinee(recycler, inlineCacheIndex, inlinee);
                    }
                    continue;
                }

                Js::FunctionCodeGenRuntimeData *const inlineeRuntimeData = IsInlinee ? runtimeData->EnsureLdFldInlinee(recycler, inlineCacheIndex, inlineeFunctionBody) :
                    functionBody->EnsureLdFldInlineeCodeGenRuntimeData(recycler, inlineCacheIndex, inlineeFunctionBody);

                if (inlineeRuntimeData->GetFunctionBody() != inlineeFunctionBody)
                {LOGMEIN("NativeCodeGenerator.cpp] 2878\n");
                    //There are obscure cases where profileData has not yet seen the polymorphic LdFld but the inlineCache has the newer object from which getter is invoked.
                    //In this case we don't want to inline that getter. Polymorphic bit will be set later correctly.
                    //See WinBlue 54540
                    continue;
                }

                Js::FunctionCodeGenJitTimeData *inlineeJitTimeData =  jitTimeData->AddLdFldInlinee(recycler, inlineCacheIndex, inlinee);
                GatherCodeGenData<true>(
                    recycler,
                    topFunctionBody,
                    inlineeFunctionBody,
                    entryPoint,
                    inliningDecider,
                    objTypeSpecFldInfoList,
                    inlineeJitTimeData,
                    inlineeRuntimeData,
                    nullptr);

                AddInlineCacheStats(jitTimeData, inlineeJitTimeData);
            }
        }
    }

#ifdef FIELD_ACCESS_STATS
    if (PHASE_VERBOSE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_VERBOSE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
    {LOGMEIN("NativeCodeGenerator.cpp] 2904\n");
        if (jitTimeData->inlineCacheStats)
        {LOGMEIN("NativeCodeGenerator.cpp] 2906\n");
            Output::Print(_u("ObTypeSpec: gathered code gen data for function %s (#%u) inlined %s (#%u): inline cache stats:\n"),
                topFunctionBody->GetDisplayName(), topFunctionBody->GetFunctionNumber(), functionBody->GetDisplayName(), functionBody->GetFunctionNumber());
            Output::Print(_u("    overall: total %u, no profile info %u\n"),
                jitTimeData->inlineCacheStats->totalInlineCacheCount, jitTimeData->inlineCacheStats->noInfoInlineCacheCount);
            Output::Print(_u("    mono: total %u, empty %u, cloned %u\n"),
                jitTimeData->inlineCacheStats->monoInlineCacheCount, jitTimeData->inlineCacheStats->emptyMonoInlineCacheCount,
                jitTimeData->inlineCacheStats->clonedMonoInlineCacheCount);
            Output::Print(_u("    poly: total %u (high %u, low %u), empty %u, equivalent %u, cloned %u\n"),
                jitTimeData->inlineCacheStats->polyInlineCacheCount, jitTimeData->inlineCacheStats->highUtilPolyInlineCacheCount,
                jitTimeData->inlineCacheStats->lowUtilPolyInlineCacheCount, jitTimeData->inlineCacheStats->emptyPolyInlineCacheCount,
                jitTimeData->inlineCacheStats->equivPolyInlineCacheCount, jitTimeData->inlineCacheStats->clonedPolyInlineCacheCount);
        }
        else
        {
            Output::Print(_u("ObTypeSpec: function %s (%s): inline cache stats unavailable\n"), topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer));
        }
        Output::Flush();
    }
#endif

#undef SetInlineCacheCount
#undef IncInlineCacheCount
#undef AddInlineCacheStats
}

Js::CodeGenRecyclableData *
NativeCodeGenerator::GatherCodeGenData(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const functionBody, Js::EntryPointInfo *const entryPoint, CodeGenWorkItem* workItem, void* function)
{LOGMEIN("NativeCodeGenerator.cpp] 2934\n");
    ASSERT_THREAD();
    Assert(functionBody);

#ifdef PROFILE_EXEC
    class AutoProfile
    {
    private:
        Js::ScriptContextProfiler *const codeGenProfiler;

    public:
        AutoProfile(Js::ScriptContextProfiler *const codeGenProfiler) : codeGenProfiler(codeGenProfiler)
        {
            ProfileBegin(codeGenProfiler, Js::DelayPhase);
            ProfileBegin(codeGenProfiler, Js::GatherCodeGenDataPhase);
        }

        ~AutoProfile()
        {
            ProfileEnd(codeGenProfiler, Js::GatherCodeGenDataPhase);
            ProfileEnd(codeGenProfiler, Js::DelayPhase);
        }
    } autoProfile(foregroundCodeGenProfiler);
#endif

    UpdateJITState();

    const auto recycler = scriptContext->GetRecycler();
    {LOGMEIN("NativeCodeGenerator.cpp] 2962\n");
        const auto jitTimeData = RecyclerNew(recycler, Js::FunctionCodeGenJitTimeData, functionBody->GetFunctionInfo(), entryPoint);
        InliningDecider inliningDecider(functionBody, workItem->Type() == JsLoopBodyWorkItemType, functionBody->IsInDebugMode(), workItem->GetJitMode());

        BEGIN_TEMP_ALLOCATOR(gatherCodeGenDataAllocator, scriptContext, _u("GatherCodeGenData"));

        ObjTypeSpecFldInfoList* objTypeSpecFldInfoList = JitAnew(gatherCodeGenDataAllocator, ObjTypeSpecFldInfoList, gatherCodeGenDataAllocator);

#if ENABLE_DEBUG_CONFIG_OPTIONS
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        if (PHASE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
        {LOGMEIN("NativeCodeGenerator.cpp] 2974\n");
            Output::Print(_u("ObjTypeSpec: top function %s (%s), function %s (%s): GatherCodeGenData(): \n"),
                topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer), functionBody->GetDisplayName(), functionBody->GetDebugNumberSet(debugStringBuffer2));
        }
#endif
        GatherCodeGenData<false>(recycler, topFunctionBody, functionBody, entryPoint, inliningDecider, objTypeSpecFldInfoList, jitTimeData, nullptr, function ? Js::JavascriptFunction::FromVar(function) : nullptr, 0);

        jitTimeData->sharedPropertyGuards = entryPoint->GetSharedPropertyGuards(jitTimeData->sharedPropertyGuardCount);

#ifdef FIELD_ACCESS_STATS
        Js::FieldAccessStats* fieldAccessStats = entryPoint->EnsureFieldAccessStats(recycler);
        fieldAccessStats->Add(jitTimeData->inlineCacheStats);
        entryPoint->GetScriptContext()->RecordFieldAccessStats(topFunctionBody, fieldAccessStats);
#endif

#ifdef FIELD_ACCESS_STATS
        if (PHASE_TRACE(Js::ObjTypeSpecPhase, topFunctionBody) || PHASE_TRACE(Js::EquivObjTypeSpecPhase, topFunctionBody))
        {LOGMEIN("NativeCodeGenerator.cpp] 2991\n");
            auto stats = jitTimeData->inlineCacheStats;
            Output::Print(_u("ObjTypeSpec: gathered code gen data for function %s (%s): inline cache stats:\n"), topFunctionBody->GetDisplayName(), topFunctionBody->GetDebugNumberSet(debugStringBuffer));
            Output::Print(_u("    overall: total %u, no profile info %u\n"), stats->totalInlineCacheCount, stats->noInfoInlineCacheCount);
            Output::Print(_u("    mono: total %u, empty %u, cloned %u\n"),
                stats->monoInlineCacheCount, stats->emptyMonoInlineCacheCount, stats->clonedMonoInlineCacheCount);
            Output::Print(_u("    poly: total %u (high %u, low %u), null %u, empty %u, ignored %u, disabled %u, equivalent %u, non-equivalent %u, cloned %u\n"),
                stats->polyInlineCacheCount, stats->highUtilPolyInlineCacheCount, stats->lowUtilPolyInlineCacheCount,
                stats->nullPolyInlineCacheCount, stats->emptyPolyInlineCacheCount, stats->ignoredPolyInlineCacheCount, stats->disabledPolyInlineCacheCount,
                stats->equivPolyInlineCacheCount, stats->nonEquivPolyInlineCacheCount, stats->clonedPolyInlineCacheCount);
        }
#endif

        uint objTypeSpecFldInfoCount = objTypeSpecFldInfoList->Count();
        jitTimeData->SetGlobalObjTypeSpecFldInfoArray(RecyclerNewArray(recycler, Field(Js::ObjTypeSpecFldInfo*), objTypeSpecFldInfoCount), objTypeSpecFldInfoCount);
        uint propertyInfoId = objTypeSpecFldInfoCount - 1;
        FOREACH_SLISTCOUNTED_ENTRY(Js::ObjTypeSpecFldInfo*, info, objTypeSpecFldInfoList)
        {LOGMEIN("NativeCodeGenerator.cpp] 3008\n");
            // Clear field values we don't need so we don't unnecessarily pin them while JIT-ing.
            if (!info->GetKeepFieldValue() && !(info->IsPoly() && info->DoesntHaveEquivalence()))
            {LOGMEIN("NativeCodeGenerator.cpp] 3011\n");
                info->SetFieldValue(nullptr);
            }
            jitTimeData->SetGlobalObjTypeSpecFldInfo(propertyInfoId--, info);
        }
        NEXT_SLISTCOUNTED_ENTRY;

        END_TEMP_ALLOCATOR(gatherCodeGenDataAllocator, scriptContext);


        auto jitData = workItem->GetJITData();

        JITTimePolymorphicInlineCacheInfo::InitializeEntryPointPolymorphicInlineCacheInfo(
            recycler,
            entryPoint->EnsurePolymorphicInlineCacheInfo(recycler, workItem->GetFunctionBody()),
            jitData);

        jitTimeData->SetPolymorphicInlineInfo(jitData->inlineeInfo, jitData->selfInfo, jitData->selfInfo->polymorphicInlineCaches);

        return RecyclerNew(recycler, Js::CodeGenRecyclableData, jitTimeData);
    }
}

bool
NativeCodeGenerator::IsBackgroundJIT() const
{LOGMEIN("NativeCodeGenerator.cpp] 3036\n");
    return Processor()->ProcessesInBackground();
}

void
NativeCodeGenerator::EnterScriptStart()
{LOGMEIN("NativeCodeGenerator.cpp] 3042\n");
    // We should be in execution
    Assert(scriptContext->GetThreadContext()->IsScriptActive());
    Assert(scriptContext->GetThreadContext()->IsInScript());

    if(CONFIG_FLAG(BgJitDelay) == 0 ||
        Js::Configuration::Global.flags.EnforceExecutionModeLimits ||
        scriptContext->GetThreadContext()->GetCallRootLevel() > 2)
    {LOGMEIN("NativeCodeGenerator.cpp] 3050\n");
        return;
    }

    if (pendingCodeGenWorkItems == 0 || pendingCodeGenWorkItems > (uint)CONFIG_FLAG(BgJitPendingFuncCap))
    {LOGMEIN("NativeCodeGenerator.cpp] 3055\n");
        // We have already finish code gen for this script context
        // Only wait if the script is small and we can easily pre-JIT all of it.
        return;
    }

    if (this->IsClosed())
    {LOGMEIN("NativeCodeGenerator.cpp] 3062\n");
        return;
    }

    // Don't need to do anything if we're in debug mode
    if (this->scriptContext->IsScriptContextInDebugMode() && !Js::Configuration::Global.EnableJitInDebugMode())
    {LOGMEIN("NativeCodeGenerator.cpp] 3068\n");
        return;
    }

    // We've already done a few calls to this scriptContext, don't bother waiting.
    if (scriptContext->callCount >= 3)
    {LOGMEIN("NativeCodeGenerator.cpp] 3074\n");
        return;
    }

    scriptContext->callCount++;

    if (scriptContext->GetDeferredBody())
    {LOGMEIN("NativeCodeGenerator.cpp] 3081\n");
        OUTPUT_TRACE(Js::DelayPhase, _u("No delay because the script has a deferred body\n"));
        return;
    }

    if(CONFIG_FLAG(BgJitDelayFgBuffer) >= CONFIG_FLAG(BgJitDelay))
    {LOGMEIN("NativeCodeGenerator.cpp] 3087\n");
        return;
    }

    class AutoCleanup
    {
    private:
        Js::ScriptContextProfiler *const codeGenProfiler;

    public:
        AutoCleanup(Js::ScriptContextProfiler *const codeGenProfiler) : codeGenProfiler(codeGenProfiler)
        {
            EDGE_ETW_INTERNAL(EventWriteJSCRIPT_NATIVECODEGEN_DELAY_START(this, 0));
#ifdef PROFILE_EXEC
            ProfileBegin(codeGenProfiler, Js::DelayPhase);
            ProfileBegin(codeGenProfiler, Js::SpeculationPhase);
#endif
        }

        ~AutoCleanup()
        {LOGMEIN("NativeCodeGenerator.cpp] 3107\n");
#ifdef PROFILE_EXEC
            ProfileEnd(codeGenProfiler, Js::SpeculationPhase);
            ProfileEnd(codeGenProfiler, Js::DelayPhase);
#endif
            EDGE_ETW_INTERNAL(EventWriteJSCRIPT_NATIVECODEGEN_DELAY_STOP(this, 0));
        }
    } autoCleanup(
#ifdef PROFILE_EXEC
        this->foregroundCodeGenProfiler
#else
        nullptr
#endif
        );

    Processor()->PrioritizeManagerAndWait(this, CONFIG_FLAG(BgJitDelay) - CONFIG_FLAG(BgJitDelayFgBuffer));
}

void
FreeNativeCodeGenAllocation(Js::ScriptContext *scriptContext, Js::JavascriptMethod address)
{LOGMEIN("NativeCodeGenerator.cpp] 3127\n");
    if (!scriptContext->GetNativeCodeGenerator())
    {LOGMEIN("NativeCodeGenerator.cpp] 3129\n");
        return;
    }

    scriptContext->GetNativeCodeGenerator()->QueueFreeNativeCodeGenAllocation((void*)address);
}

bool TryReleaseNonHiPriWorkItem(Js::ScriptContext* scriptContext, CodeGenWorkItem* workItem)
{LOGMEIN("NativeCodeGenerator.cpp] 3137\n");
    if (!scriptContext->GetNativeCodeGenerator())
    {LOGMEIN("NativeCodeGenerator.cpp] 3139\n");
        return false;
    }

    return scriptContext->GetNativeCodeGenerator()->TryReleaseNonHiPriWorkItem(workItem);
}

// Called from within the lock
// The work item cannot be used after this point if it returns true
bool NativeCodeGenerator::TryReleaseNonHiPriWorkItem(CodeGenWorkItem* workItem)
{LOGMEIN("NativeCodeGenerator.cpp] 3149\n");
    // If its the highest priority, don't release it, let the job continue
    if (workItem->IsInJitQueue())
    {LOGMEIN("NativeCodeGenerator.cpp] 3152\n");
        return false;
    }

    workItems.Unlink(workItem);
    Assert(!workItem->RecyclableData());

    workItem->Delete();
    return true;
}

void
NativeCodeGenerator::FreeNativeCodeGenAllocation(void* address)
{LOGMEIN("NativeCodeGenerator.cpp] 3165\n");
    if(this->backgroundAllocators)
    {LOGMEIN("NativeCodeGenerator.cpp] 3167\n");
        ThreadContext * context = this->scriptContext->GetThreadContext();
        if (JITManager::GetJITManager()->IsOOPJITEnabled())
        {LOGMEIN("NativeCodeGenerator.cpp] 3170\n");
            // OOP JIT TODO: need error handling?
            JITManager::GetJITManager()->FreeAllocation(context->GetRemoteThreadContextAddr(), (intptr_t)address);
        }
        else
        {
            this->backgroundAllocators->emitBufferManager.FreeAllocation(address);
        }
    }
}

void
NativeCodeGenerator::QueueFreeNativeCodeGenAllocation(void* address)
{LOGMEIN("NativeCodeGenerator.cpp] 3183\n");
    ASSERT_THREAD();

    if(IsClosed())
    {LOGMEIN("NativeCodeGenerator.cpp] 3187\n");
        return;
    }

    if (!JITManager::GetJITManager()->IsOOPJITEnabled() || !CONFIG_FLAG(OOPCFGRegistration))
    {LOGMEIN("NativeCodeGenerator.cpp] 3192\n");
        //DeRegister Entry Point for CFG
        ThreadContext::GetContextForCurrentThread()->SetValidCallTargetForCFG(address, false);
    }
    
    if ((!JITManager::GetJITManager()->IsOOPJITEnabled() && !this->scriptContext->GetThreadContext()->GetPreReservedVirtualAllocator()->IsInRange((void*)address)) ||
        (JITManager::GetJITManager()->IsOOPJITEnabled() && !PreReservedVirtualAllocWrapper::IsInRange((void*)this->scriptContext->GetThreadContext()->GetPreReservedRegionAddr(), (void*)address)))
    {LOGMEIN("NativeCodeGenerator.cpp] 3199\n");
        this->scriptContext->GetJitFuncRangeCache()->RemoveFuncRange((void*)address);
    }

    // The foreground allocators may have been used
    ThreadContext * context = this->scriptContext->GetThreadContext();
    if(this->foregroundAllocators)
    {LOGMEIN("NativeCodeGenerator.cpp] 3206\n");
        if (JITManager::GetJITManager()->IsOOPJITEnabled())
        {LOGMEIN("NativeCodeGenerator.cpp] 3208\n");
            // TODO: OOP JIT, should we always just queue this in background?
            // OOP JIT TODO: need error handling?
            JITManager::GetJITManager()->FreeAllocation(context->GetRemoteThreadContextAddr(), (intptr_t)address);
            return;
        }
        else
        {
            if (this->foregroundAllocators->emitBufferManager.FreeAllocation(address))
            {LOGMEIN("NativeCodeGenerator.cpp] 3217\n");
                return;
            }
        }
    }

    // The background allocators were used. Queue a job to free the allocation from the background thread.
    this->freeLoopBodyManager.QueueFreeLoopBodyJob(address);
}

void NativeCodeGenerator::FreeLoopBodyJobManager::QueueFreeLoopBodyJob(void* codeAddress)
{LOGMEIN("NativeCodeGenerator.cpp] 3228\n");
    Assert(!this->isClosed);

    FreeLoopBodyJob* job = HeapNewNoThrow(FreeLoopBodyJob, this, codeAddress);

    if (job == nullptr)
    {LOGMEIN("NativeCodeGenerator.cpp] 3234\n");
        FreeLoopBodyJob stackJob(this, codeAddress, false /* heapAllocated */);

        {LOGMEIN("NativeCodeGenerator.cpp] 3237\n");
            AutoOptionalCriticalSection lock(Processor()->GetCriticalSection());
#if DBG
            this->waitingForStackJob = true;
#endif
            this->stackJobProcessed = false;
            Processor()->AddJob(&stackJob);
        }
        Processor()->PrioritizeJobAndWait(this, &stackJob);
    }
    else
    {
        AutoOptionalCriticalSection lock(Processor()->GetCriticalSection());
        if (Processor()->HasManager(this))
        {
            Processor()->AddJobAndProcessProactively<FreeLoopBodyJobManager, FreeLoopBodyJob*>(this, job);
        }
        else
        {
            HeapDelete(job);
        }
    }
}

#ifdef PROFILE_EXEC
void
NativeCodeGenerator::CreateProfiler(Js::ScriptContextProfiler * profiler)
{LOGMEIN("NativeCodeGenerator.cpp] 3264\n");
    Assert(this->foregroundCodeGenProfiler == nullptr);

    this->foregroundCodeGenProfiler = profiler;
    profiler->AddRef();

}

Js::ScriptContextProfiler *
NativeCodeGenerator::EnsureForegroundCodeGenProfiler()
{LOGMEIN("NativeCodeGenerator.cpp] 3274\n");
    if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
    {LOGMEIN("NativeCodeGenerator.cpp] 3276\n");
        Assert(this->foregroundCodeGenProfiler != nullptr);
        Assert(this->foregroundCodeGenProfiler->IsInitialized());
    }

    return this->foregroundCodeGenProfiler;
}


void
NativeCodeGenerator::SetProfilerFromNativeCodeGen(NativeCodeGenerator * nativeCodeGen)
{LOGMEIN("NativeCodeGenerator.cpp] 3287\n");
    Assert(Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag));

    Assert(this->foregroundCodeGenProfiler != nullptr);
    Assert(this->foregroundCodeGenProfiler->IsInitialized());
    Assert(nativeCodeGen->foregroundCodeGenProfiler != nullptr);
    Assert(nativeCodeGen->foregroundCodeGenProfiler->IsInitialized());

    this->foregroundCodeGenProfiler->Release();
    this->foregroundCodeGenProfiler = nativeCodeGen->foregroundCodeGenProfiler;
    this->foregroundCodeGenProfiler->AddRef();
}

void
NativeCodeGenerator::ProfilePrint()
{LOGMEIN("NativeCodeGenerator.cpp] 3302\n");
    Js::ScriptContextProfiler *codegenProfiler = this->backgroundCodeGenProfiler;
    if (Js::Configuration::Global.flags.Verbose)
    {LOGMEIN("NativeCodeGenerator.cpp] 3305\n");
        //Print individual CodegenProfiler information in verbose mode
        while (codegenProfiler)
        {LOGMEIN("NativeCodeGenerator.cpp] 3308\n");
            codegenProfiler->ProfilePrint(Js::Configuration::Global.flags.Profile.GetFirstPhase());
            codegenProfiler = codegenProfiler->next;
        }
    }
    else
    {
        //Merge all the codegenProfiler for single snapshot.
        Js::ScriptContextProfiler* mergeToProfiler = codegenProfiler;

        // find the first initialized profiler
        while (mergeToProfiler != nullptr && !mergeToProfiler->IsInitialized())
        {LOGMEIN("NativeCodeGenerator.cpp] 3320\n");
            mergeToProfiler = mergeToProfiler->next;
        }
        if (mergeToProfiler != nullptr)
        {LOGMEIN("NativeCodeGenerator.cpp] 3324\n");
            // merge the rest profiler to the above initialized profiler
            codegenProfiler = mergeToProfiler->next;
            while (codegenProfiler)
            {LOGMEIN("NativeCodeGenerator.cpp] 3328\n");
                if (codegenProfiler->IsInitialized())
                {LOGMEIN("NativeCodeGenerator.cpp] 3330\n");
                    mergeToProfiler->ProfileMerge(codegenProfiler);
                }
                codegenProfiler = codegenProfiler->next;
            }
            mergeToProfiler->ProfilePrint(Js::Configuration::Global.flags.Profile.GetFirstPhase());
        }
    }
}

void
NativeCodeGenerator::ProfileBegin(Js::ScriptContextProfiler *const profiler, Js::Phase phase)
{LOGMEIN("NativeCodeGenerator.cpp] 3342\n");
    AssertMsg((profiler != nullptr) == Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag),
        "Profiler tag is supplied but the profiler pointer is NULL");
    if (profiler)
    {LOGMEIN("NativeCodeGenerator.cpp] 3346\n");
        profiler->ProfileBegin(phase);
    }
}

void
NativeCodeGenerator::ProfileEnd(Js::ScriptContextProfiler *const profiler, Js::Phase phase)
{LOGMEIN("NativeCodeGenerator.cpp] 3353\n");
    AssertMsg((profiler != nullptr) == Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag),
        "Profiler tag is supplied but the profiler pointer is NULL");
    if (profiler)
    {LOGMEIN("NativeCodeGenerator.cpp] 3357\n");
        profiler->ProfileEnd(phase);
    }
}

#endif

void NativeCodeGenerator::AddToJitQueue(CodeGenWorkItem *const codeGenWorkItem, bool prioritize, bool lock, void* function)
{LOGMEIN("NativeCodeGenerator.cpp] 3365\n");
    codeGenWorkItem->VerifyJitMode();

    Js::CodeGenRecyclableData* recyclableData = GatherCodeGenData(codeGenWorkItem->GetFunctionBody(), codeGenWorkItem->GetFunctionBody(), codeGenWorkItem->GetEntryPoint(), codeGenWorkItem, function);
    codeGenWorkItem->SetRecyclableData(recyclableData);

    AutoOptionalCriticalSection autoLock(lock ? Processor()->GetCriticalSection() : nullptr);
    scriptContext->GetThreadContext()->RegisterCodeGenRecyclableData(recyclableData);

    // If we have added a lot of jobs that are still waiting to be jitted, remove the oldest job
    // to ensure we do not spend time jitting stale work items.
    const ExecutionMode jitMode = codeGenWorkItem->GetJitMode();
    if(jitMode == ExecutionMode::FullJit &&
        queuedFullJitWorkItemCount >= (unsigned int)CONFIG_FLAG(JitQueueThreshold))
    {LOGMEIN("NativeCodeGenerator.cpp] 3379\n");
        CodeGenWorkItem *const workItemRemoved = queuedFullJitWorkItems.Tail()->WorkItem();
        Assert(workItemRemoved->GetJitMode() == ExecutionMode::FullJit);
        if(Processor()->RemoveJob(workItemRemoved))
        {LOGMEIN("NativeCodeGenerator.cpp] 3383\n");
            queuedFullJitWorkItems.UnlinkFromEnd();
            --queuedFullJitWorkItemCount;
            workItemRemoved->OnRemoveFromJitQueue(this);
        }
    }
    Processor()->AddJob(codeGenWorkItem, prioritize);   // This one can throw (really unlikely though), OOM specifically.
    if(jitMode == ExecutionMode::FullJit)
    {LOGMEIN("NativeCodeGenerator.cpp] 3391\n");
        QueuedFullJitWorkItem *const queuedFullJitWorkItem = codeGenWorkItem->EnsureQueuedFullJitWorkItem();
        if(queuedFullJitWorkItem) // ignore OOM, this work item just won't be removed from the job processor's queue
        {LOGMEIN("NativeCodeGenerator.cpp] 3394\n");
            if(prioritize)
            {LOGMEIN("NativeCodeGenerator.cpp] 3396\n");
                queuedFullJitWorkItems.LinkToBeginning(queuedFullJitWorkItem);
            }
            else
            {
                queuedFullJitWorkItems.LinkToEnd(queuedFullJitWorkItem);
            }
            ++queuedFullJitWorkItemCount;
        }
    }
    codeGenWorkItem->OnAddToJitQueue();
}

void NativeCodeGenerator::AddWorkItem(CodeGenWorkItem* workitem)
{LOGMEIN("NativeCodeGenerator.cpp] 3410\n");
    workitem->ResetJitMode();
    workItems.LinkToEnd(workitem);
}

Js::ScriptContextProfiler * NativeCodeGenerator::GetBackgroundCodeGenProfiler(PageAllocator *allocator)
{LOGMEIN("NativeCodeGenerator.cpp] 3416\n");
#ifdef  PROFILE_EXEC
    if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
    {LOGMEIN("NativeCodeGenerator.cpp] 3419\n");
        Js::ScriptContextProfiler *codegenProfiler = this->backgroundCodeGenProfiler;
        while (codegenProfiler)
        {LOGMEIN("NativeCodeGenerator.cpp] 3422\n");
            if (codegenProfiler->pageAllocator == allocator)
            {LOGMEIN("NativeCodeGenerator.cpp] 3424\n");
                if (!codegenProfiler->IsInitialized())
                {LOGMEIN("NativeCodeGenerator.cpp] 3426\n");
                    codegenProfiler->Initialize(allocator, nullptr);
                }
                return codegenProfiler;
            }
            codegenProfiler = codegenProfiler->next;
        }
        Assert(false);
    }
    return nullptr;
#else
    return nullptr;
#endif
}

void NativeCodeGenerator::AllocateBackgroundCodeGenProfiler(PageAllocator *pageAllocator)
{LOGMEIN("NativeCodeGenerator.cpp] 3442\n");
#ifdef PROFILE_EXEC
if (Js::Configuration::Global.flags.IsEnabled(Js::ProfileFlag))
{LOGMEIN("NativeCodeGenerator.cpp] 3445\n");
    Js::ScriptContextProfiler *codegenProfiler = NoCheckHeapNew(Js::ScriptContextProfiler);
    codegenProfiler->pageAllocator = pageAllocator;
    codegenProfiler->next = this->backgroundCodeGenProfiler;
    this->backgroundCodeGenProfiler = codegenProfiler;
}
#endif
}

bool NativeCodeGenerator::TryAggressiveInlining(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const inlineeFunctionBody, InliningDecider &inliningDecider, uint& inlineeCount, uint recursiveInlineDepth)
{
    PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

    if (!inlineeFunctionBody->GetProfiledCallSiteCount())
    {LOGMEIN("NativeCodeGenerator.cpp] 3459\n");
        // Nothing to inline. See this as fully inlinable function.
        return true;
    }

    class AutoCleanup
    {
    private:
        Js::FunctionBody *const functionBody;

    public:
        AutoCleanup(Js::FunctionBody *const functionBody) : functionBody(functionBody)
        {LOGMEIN("NativeCodeGenerator.cpp] 3471\n");
            functionBody->OnBeginInlineInto();
        }

        ~AutoCleanup()
        {LOGMEIN("NativeCodeGenerator.cpp] 3476\n");
            functionBody->OnEndInlineInto();
        }
    } autoCleanup(inlineeFunctionBody);

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    class AutoTrace
    {

        Js::FunctionBody *const topFunc;
        Js::FunctionBody *const inlineeFunc;
        uint32& inlineeCount;
        bool done;

        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
        char16 debugStringBuffer2[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

    public:
        AutoTrace(Js::FunctionBody *const topFunctionBody, Js::FunctionBody *const inlineeFunctionBody, uint32& inlineeCount) : topFunc(topFunctionBody),
            inlineeFunc(inlineeFunctionBody), done(false), inlineeCount(inlineeCount)
        {LOGMEIN("NativeCodeGenerator.cpp] 3496\n");
            if (topFunc == inlineeFunc)
            {LOGMEIN("NativeCodeGenerator.cpp] 3498\n");
                INLINE_TESTTRACE(_u("INLINING: Recursive tryAggressiveInlining started topFunc: %s (%s)\n"), topFunc->GetDisplayName(),
                    topFunc->GetDebugNumberSet(debugStringBuffer))
            }
        }
        void Done(bool success)
        {LOGMEIN("NativeCodeGenerator.cpp] 3504\n");
            if (success)
            {LOGMEIN("NativeCodeGenerator.cpp] 3506\n");
                done = true;
                if (topFunc == inlineeFunc)
                {LOGMEIN("NativeCodeGenerator.cpp] 3509\n");
                    INLINE_TESTTRACE(_u("INLINING: Recursive tryAggressiveInlining succeeded topFunc: %s (%s), inlinee count: %d\n"), topFunc->GetDisplayName(),
                        topFunc->GetDebugNumberSet(debugStringBuffer), inlineeCount);
                }
                else
                {
                    INLINE_TESTTRACE(_u("INLINING: TryAggressiveInlining succeeded topFunc: %s (%s), inlinee: %s (%s) \n"), topFunc->GetDisplayName(),
                        topFunc->GetDebugNumberSet(debugStringBuffer),
                        inlineeFunc->GetDisplayName(),
                        inlineeFunc->GetDebugNumberSet(debugStringBuffer2));
                }
            }
            else
            {
                Assert(done == false);
            }
        }
        void TraceFailure(const char16 *message)
        {LOGMEIN("NativeCodeGenerator.cpp] 3527\n");

            INLINE_TESTTRACE(_u("INLINING: TryAggressiveInlining failed topFunc (%s): %s (%s), inlinee: %s (%s) \n"), message, topFunc->GetDisplayName(),
                topFunc->GetDebugNumberSet(debugStringBuffer),
                inlineeFunc->GetDisplayName(),
                inlineeFunc->GetDebugNumberSet(debugStringBuffer2));
        }
        ~AutoTrace()
        {LOGMEIN("NativeCodeGenerator.cpp] 3535\n");
            if (!done)
            {LOGMEIN("NativeCodeGenerator.cpp] 3537\n");
                if (topFunc == inlineeFunc)
                {LOGMEIN("NativeCodeGenerator.cpp] 3539\n");
                    INLINE_TESTTRACE(_u("INLINING: Recursive tryAggressiveInlining failed topFunc: %s (%s)\n"), topFunc->GetDisplayName(),
                        topFunc->GetDebugNumberSet(debugStringBuffer));
                }
                else
                {
                    INLINE_TESTTRACE(_u("INLINING: TryAggressiveInlining failed topFunc: %s (%s), inlinee: %s (%s) \n"), topFunc->GetDisplayName(),
                        topFunc->GetDebugNumberSet(debugStringBuffer),
                        inlineeFunc->GetDisplayName(),
                        inlineeFunc->GetDebugNumberSet(debugStringBuffer2));
                }

            }
        }
    };

    AutoTrace trace(topFunctionBody, inlineeFunctionBody, inlineeCount);
#endif

    if (inlineeFunctionBody->GetProfiledSwitchCount())
    {LOGMEIN("NativeCodeGenerator.cpp] 3559\n");
#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
        trace.TraceFailure(_u("Switch statement in inlinee"));
#endif
        return false;
    }

    bool isInlinee = topFunctionBody != inlineeFunctionBody;

    if (isInlinee ? !inliningDecider.InlineIntoInliner(inlineeFunctionBody) : !inliningDecider.InlineIntoTopFunc())
    {LOGMEIN("NativeCodeGenerator.cpp] 3569\n");
        return false;
    }

    const auto profiledCallSiteCount = inlineeFunctionBody->GetProfiledCallSiteCount();
    for (Js::ProfileId profiledCallSiteId = 0; profiledCallSiteId < profiledCallSiteCount; ++profiledCallSiteId)
    {LOGMEIN("NativeCodeGenerator.cpp] 3575\n");
        bool isConstructorCall = false;
        bool isPolymorphicCall = false;

        if (!inliningDecider.HasCallSiteInfo(inlineeFunctionBody, profiledCallSiteId))
        {LOGMEIN("NativeCodeGenerator.cpp] 3580\n");
            //There is no callsite information. We should hit bailonnoprofile for these callsites. Ignore.
            continue;
        }

        Js::FunctionInfo *inlinee = inliningDecider.GetCallSiteFuncInfo(inlineeFunctionBody, profiledCallSiteId, &isConstructorCall, &isPolymorphicCall);
        if (!inlinee)
        {LOGMEIN("NativeCodeGenerator.cpp] 3587\n");
            if (isPolymorphicCall)
            {LOGMEIN("NativeCodeGenerator.cpp] 3589\n");
                //Try and see if this polymorphic call
                Js::FunctionBody* inlineeFunctionBodyArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = { 0 };
                bool canInlineArray[Js::DynamicProfileInfo::maxPolymorphicInliningSize] = { 0 };
                uint polyInlineeCount = inliningDecider.InlinePolymorphicCallSite(inlineeFunctionBody, profiledCallSiteId, inlineeFunctionBodyArray,
                    Js::DynamicProfileInfo::maxPolymorphicInliningSize, canInlineArray);

                //We should be able to inline everything here.
                if (polyInlineeCount >= 2)
                {LOGMEIN("NativeCodeGenerator.cpp] 3598\n");
                    for (uint i = 0; i < polyInlineeCount; i++)
                    {LOGMEIN("NativeCodeGenerator.cpp] 3600\n");
                        bool isInlined = canInlineArray[i];
                        if (isInlined)
                        {LOGMEIN("NativeCodeGenerator.cpp] 3603\n");
                            ++inlineeCount;
                            if (!TryAggressiveInlining(topFunctionBody, inlineeFunctionBodyArray[i], inliningDecider, inlineeCount, inlineeFunctionBody == inlineeFunctionBodyArray[i] ? recursiveInlineDepth + 1 : 0))
                            {LOGMEIN("NativeCodeGenerator.cpp] 3606\n");
                                return false;
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                }
                else
                {
                    return false;
                }
            }
            else
            {
                return false;
            }
        }
        else
        {
            inlinee = inliningDecider.Inline(inlineeFunctionBody, inlinee, isConstructorCall, false, inliningDecider.GetConstantArgInfo(inlineeFunctionBody, profiledCallSiteId), profiledCallSiteId, inlineeFunctionBody->GetFunctionInfo() == inlinee ? recursiveInlineDepth + 1 : 0, true);
            if (!inlinee)
            {LOGMEIN("NativeCodeGenerator.cpp] 3630\n");
                return false;
            }
            Js::FunctionBody *const functionBody = inlinee->GetFunctionBody();
            if (!functionBody)
            {LOGMEIN("NativeCodeGenerator.cpp] 3635\n");
                //Built-in
                continue;
            }
            //Recursive call
            ++inlineeCount;
            if (!TryAggressiveInlining(topFunctionBody, functionBody, inliningDecider, inlineeCount, inlineeFunctionBody == functionBody ? recursiveInlineDepth + 1 : 0 ))
            {LOGMEIN("NativeCodeGenerator.cpp] 3642\n");
                return false;
            }
        }
    }

#if defined(DBG_DUMP) || defined(ENABLE_DEBUG_CONFIG_OPTIONS)
    trace.Done(true);
#endif
    return true;
}

#if _WIN32
void
JITManager::HandleServerCallResult(HRESULT hr, RemoteCallType callType)
{LOGMEIN("NativeCodeGenerator.cpp] 3657\n");
    // handle the normal hresults
    switch (hr)
    {LOGMEIN("NativeCodeGenerator.cpp] 3660\n");
    case S_OK:
        return;
    case E_ABORT:
        throw Js::OperationAbortedException();
    case E_OUTOFMEMORY:
        Js::Throw::OutOfMemory();
    case VBSERR_OutOfStack:
        throw Js::StackOverflowException();
    default:
        break;
    }
    // if jit process is terminated, we can handle certain abnormal hresults

    // we should not have RPC failure if JIT process is still around
    // since this is going to be a failfast, lets wait a bit in case server is in process of terminating
    if (WaitForSingleObject(GetJITManager()->GetServerHandle(), CONFIG_FLAG(RPCFailFastWait)) != WAIT_OBJECT_0)
    {LOGMEIN("NativeCodeGenerator.cpp] 3677\n");
        RpcFailure_fatal_error(hr);
    }

    // we only expect to see these hresults in case server has been closed. failfast otherwise
    if (hr != HRESULT_FROM_WIN32(RPC_S_CALL_FAILED) &&
        hr != HRESULT_FROM_WIN32(RPC_S_CALL_FAILED_DNE) &&
        hr != HRESULT_FROM_WIN32(RPC_X_SS_IN_NULL_CONTEXT))
    {LOGMEIN("NativeCodeGenerator.cpp] 3685\n");
        RpcFailure_fatal_error(hr);
    }
    switch (callType)
    {LOGMEIN("NativeCodeGenerator.cpp] 3689\n");
    case RemoteCallType::CodeGen:
        // inform job manager that JIT work item has been cancelled
        throw Js::OperationAbortedException();
    case RemoteCallType::HeapQuery:
    case RemoteCallType::ThunkCreation:
        Js::Throw::OutOfMemory();
    case RemoteCallType::StateUpdate:
        // if server process is gone, we can ignore failures updating its state
        return;
    default:
        Assert(UNREACHED);
        RpcFailure_fatal_error(hr);
    }
}
#endif
