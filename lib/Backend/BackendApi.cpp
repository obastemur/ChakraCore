//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

NativeCodeGenerator *
NewNativeCodeGenerator(Js::ScriptContext * scriptContext)
{LOGMEIN("BackendApi.cpp] 8\n");
    return HeapNew(NativeCodeGenerator, scriptContext);
}

void
DeleteNativeCodeGenerator(NativeCodeGenerator * nativeCodeGen)
{LOGMEIN("BackendApi.cpp] 14\n");
    HeapDelete(nativeCodeGen);
}

void
CloseNativeCodeGenerator(NativeCodeGenerator * nativeCodeGen)
{LOGMEIN("BackendApi.cpp] 20\n");
    nativeCodeGen->Close();
}

bool
IsClosedNativeCodeGenerator(NativeCodeGenerator * nativeCodeGen)
{LOGMEIN("BackendApi.cpp] 26\n");
    return nativeCodeGen->IsClosed();
}

void SetProfileModeNativeCodeGen(NativeCodeGenerator *pNativeCodeGen, BOOL fSet)
{LOGMEIN("BackendApi.cpp] 31\n");
    pNativeCodeGen->SetProfileMode(fSet);
}

void UpdateNativeCodeGeneratorForDebugMode(NativeCodeGenerator* nativeCodeGen)
{LOGMEIN("BackendApi.cpp] 36\n");
    nativeCodeGen->UpdateQueueForDebugMode();
}

CriticalSection *GetNativeCodeGenCriticalSection(NativeCodeGenerator *pNativeCodeGen)
{LOGMEIN("BackendApi.cpp] 41\n");
    return pNativeCodeGen->Processor()->GetCriticalSection();
}

///----------------------------------------------------------------------------
///
/// GenerateFunction
///
///     This is the main entry point for the runtime to call the native code
///     generator for js function.
///
///----------------------------------------------------------------------------

void
GenerateFunction(NativeCodeGenerator * nativeCodeGen, Js::FunctionBody * fn, Js::ScriptFunction * function)
{LOGMEIN("BackendApi.cpp] 56\n");
    nativeCodeGen->GenerateFunction(fn, function);
}
InProcCodeGenAllocators* GetForegroundAllocator(NativeCodeGenerator * nativeCodeGen, PageAllocator* pageallocator)
{LOGMEIN("BackendApi.cpp] 60\n");
    return nativeCodeGen->GetCodeGenAllocator(pageallocator);
}
#ifdef ENABLE_PREJIT
void
GenerateAllFunctions(NativeCodeGenerator * nativeCodeGen, Js::FunctionBody *fn)
{LOGMEIN("BackendApi.cpp] 66\n");
    nativeCodeGen->GenerateAllFunctions(fn);
}
#endif
#ifdef IR_VIEWER
Js::Var
RejitIRViewerFunction(NativeCodeGenerator *nativeCodeGen, Js::FunctionBody *fn, Js::ScriptContext *scriptContext)
{LOGMEIN("BackendApi.cpp] 73\n");
    return nativeCodeGen->RejitIRViewerFunction(fn, scriptContext);
}
#endif

void
GenerateLoopBody(NativeCodeGenerator *nativeCodeGen, Js::FunctionBody *fn, Js::LoopHeader * loopHeader, Js::EntryPointInfo* info, uint localCount, Js::Var localSlots[])
{LOGMEIN("BackendApi.cpp] 80\n");
    nativeCodeGen->GenerateLoopBody(fn, loopHeader, info, localCount, localSlots);
}

void
NativeCodeGenEnterScriptStart(NativeCodeGenerator * nativeCodeGen)
{LOGMEIN("BackendApi.cpp] 86\n");
    if (nativeCodeGen)
    {LOGMEIN("BackendApi.cpp] 88\n");
        nativeCodeGen->EnterScriptStart();
    }
}

BOOL IsIntermediateCodeGenThunk(Js::JavascriptMethod codeAddress)
{LOGMEIN("BackendApi.cpp] 94\n");
    return NativeCodeGenerator::IsThunk(codeAddress);
}

BOOL IsAsmJsCodeGenThunk(Js::JavascriptMethod codeAddress)
{LOGMEIN("BackendApi.cpp] 99\n");
    return NativeCodeGenerator::IsAsmJsCodeGenThunk(codeAddress);
}

CheckCodeGenFunction GetCheckCodeGenFunction(Js::JavascriptMethod codeAddress)
{LOGMEIN("BackendApi.cpp] 104\n");
    return NativeCodeGenerator::GetCheckCodeGenFunction(codeAddress);
}

Js::JavascriptMethod GetCheckCodeGenThunk()
{LOGMEIN("BackendApi.cpp] 109\n");
    return NativeCodeGenerator::CheckCodeGenThunk;
}

#ifdef ASMJS_PLAT
Js::JavascriptMethod GetCheckAsmJsCodeGenThunk()
{LOGMEIN("BackendApi.cpp] 115\n");
    return NativeCodeGenerator::CheckAsmJsCodeGenThunk;
}
#endif

uint GetBailOutRegisterSaveSlotCount()
{LOGMEIN("BackendApi.cpp] 121\n");
    // REVIEW: not all registers are used, we are allocating more space then necessary.
    return LinearScanMD::GetRegisterSaveSlotCount();
}

uint
GetBailOutReserveSlotCount()
{LOGMEIN("BackendApi.cpp] 128\n");
    return 1; //For arguments id
}


#if DBG
void CheckIsExecutable(Js::RecyclableObject * function, Js::JavascriptMethod entrypoint)
{LOGMEIN("BackendApi.cpp] 135\n");
    Js::ScriptContext * scriptContext = function->GetScriptContext();
    // it's easy to call the default entry point from RecyclableObject.
    AssertMsg((Js::JavascriptFunction::Is(function) && Js::JavascriptFunction::FromVar(function)->IsExternalFunction())
        || Js::CrossSite::IsThunk(entrypoint) || !scriptContext->IsActuallyClosed() ||
        (scriptContext->GetThreadContext()->IsScriptActive() && !Js::JavascriptConversion::IsCallable(function)),
        "Can't call function when the script context is closed");

    if (scriptContext->GetThreadContext()->IsScriptActive())
    {LOGMEIN("BackendApi.cpp] 144\n");
        return;
    }
    if (function->IsExternal())
    {LOGMEIN("BackendApi.cpp] 148\n");
        return;
    }
    if (Js::JavascriptOperators::GetTypeId(function) == Js::TypeIds_HostDispatch)
    {
        AssertMsg(false, "Has to go through CallRootFunction to start calling Javascript function");
    }
    else if (Js::JavascriptFunction::Is(function))
    {LOGMEIN("BackendApi.cpp] 156\n");
        if (((Js::JavascriptFunction*)function)->IsExternalFunction())
        {LOGMEIN("BackendApi.cpp] 158\n");
            return;
        }
        else if (((Js::JavascriptFunction*)function)->IsWinRTFunction())
        {LOGMEIN("BackendApi.cpp] 162\n");
            return;
        }
        else
        {
            AssertMsg(false, "Has to go through CallRootFunction to start calling Javascript function");
        }
    }
    else
    {
        AssertMsg(false, "Has to go through CallRootFunction to start calling Javascript function");
    }
}
#endif

#ifdef PROFILE_EXEC
void
CreateProfilerNativeCodeGen(NativeCodeGenerator * nativeCodeGen, Js::ScriptContextProfiler * profiler)
{LOGMEIN("BackendApi.cpp] 180\n");
    nativeCodeGen->CreateProfiler(profiler);
}

void
ProfilePrintNativeCodeGen(NativeCodeGenerator * nativeCodeGen)
{LOGMEIN("BackendApi.cpp] 186\n");
    nativeCodeGen->ProfilePrint();
}

void
SetProfilerFromNativeCodeGen(NativeCodeGenerator * toNativeCodeGen, NativeCodeGenerator * fromNativeCodeGen)
{LOGMEIN("BackendApi.cpp] 192\n");
    toNativeCodeGen->SetProfilerFromNativeCodeGen(fromNativeCodeGen);
}
#endif

void DeleteNativeCodeData(NativeCodeData * data)
{LOGMEIN("BackendApi.cpp] 198\n");
    delete data;
}
