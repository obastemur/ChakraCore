//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

NativeCodeGenerator *
NewNativeCodeGenerator(Js::ScriptContext * scriptContext)
{TRACE_IT(44);
    return HeapNew(NativeCodeGenerator, scriptContext);
}

void
DeleteNativeCodeGenerator(NativeCodeGenerator * nativeCodeGen)
{TRACE_IT(45);
    HeapDelete(nativeCodeGen);
}

void
CloseNativeCodeGenerator(NativeCodeGenerator * nativeCodeGen)
{TRACE_IT(46);
    nativeCodeGen->Close();
}

bool
IsClosedNativeCodeGenerator(NativeCodeGenerator * nativeCodeGen)
{TRACE_IT(47);
    return nativeCodeGen->IsClosed();
}

void SetProfileModeNativeCodeGen(NativeCodeGenerator *pNativeCodeGen, BOOL fSet)
{TRACE_IT(48);
    pNativeCodeGen->SetProfileMode(fSet);
}

void UpdateNativeCodeGeneratorForDebugMode(NativeCodeGenerator* nativeCodeGen)
{TRACE_IT(49);
    nativeCodeGen->UpdateQueueForDebugMode();
}

CriticalSection *GetNativeCodeGenCriticalSection(NativeCodeGenerator *pNativeCodeGen)
{TRACE_IT(50);
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
{TRACE_IT(51);
    nativeCodeGen->GenerateFunction(fn, function);
}
InProcCodeGenAllocators* GetForegroundAllocator(NativeCodeGenerator * nativeCodeGen, PageAllocator* pageallocator)
{TRACE_IT(52);
    return nativeCodeGen->GetCodeGenAllocator(pageallocator);
}
#ifdef ENABLE_PREJIT
void
GenerateAllFunctions(NativeCodeGenerator * nativeCodeGen, Js::FunctionBody *fn)
{TRACE_IT(53);
    nativeCodeGen->GenerateAllFunctions(fn);
}
#endif
#ifdef IR_VIEWER
Js::Var
RejitIRViewerFunction(NativeCodeGenerator *nativeCodeGen, Js::FunctionBody *fn, Js::ScriptContext *scriptContext)
{TRACE_IT(54);
    return nativeCodeGen->RejitIRViewerFunction(fn, scriptContext);
}
#endif

void
GenerateLoopBody(NativeCodeGenerator *nativeCodeGen, Js::FunctionBody *fn, Js::LoopHeader * loopHeader, Js::EntryPointInfo* info, uint localCount, Js::Var localSlots[])
{TRACE_IT(55);
    nativeCodeGen->GenerateLoopBody(fn, loopHeader, info, localCount, localSlots);
}

void
NativeCodeGenEnterScriptStart(NativeCodeGenerator * nativeCodeGen)
{TRACE_IT(56);
    if (nativeCodeGen)
    {TRACE_IT(57);
        nativeCodeGen->EnterScriptStart();
    }
}

BOOL IsIntermediateCodeGenThunk(Js::JavascriptMethod codeAddress)
{TRACE_IT(58);
    return NativeCodeGenerator::IsThunk(codeAddress);
}

BOOL IsAsmJsCodeGenThunk(Js::JavascriptMethod codeAddress)
{TRACE_IT(59);
    return NativeCodeGenerator::IsAsmJsCodeGenThunk(codeAddress);
}

CheckCodeGenFunction GetCheckCodeGenFunction(Js::JavascriptMethod codeAddress)
{TRACE_IT(60);
    return NativeCodeGenerator::GetCheckCodeGenFunction(codeAddress);
}

Js::JavascriptMethod GetCheckCodeGenThunk()
{TRACE_IT(61);
    return NativeCodeGenerator::CheckCodeGenThunk;
}

#ifdef ASMJS_PLAT
Js::JavascriptMethod GetCheckAsmJsCodeGenThunk()
{TRACE_IT(62);
    return NativeCodeGenerator::CheckAsmJsCodeGenThunk;
}
#endif

uint GetBailOutRegisterSaveSlotCount()
{TRACE_IT(63);
    // REVIEW: not all registers are used, we are allocating more space then necessary.
    return LinearScanMD::GetRegisterSaveSlotCount();
}

uint
GetBailOutReserveSlotCount()
{TRACE_IT(64);
    return 1; //For arguments id
}


#if DBG
void CheckIsExecutable(Js::RecyclableObject * function, Js::JavascriptMethod entrypoint)
{TRACE_IT(65);
    Js::ScriptContext * scriptContext = function->GetScriptContext();
    // it's easy to call the default entry point from RecyclableObject.
    AssertMsg((Js::JavascriptFunction::Is(function) && Js::JavascriptFunction::FromVar(function)->IsExternalFunction())
        || Js::CrossSite::IsThunk(entrypoint) || !scriptContext->IsActuallyClosed() ||
        (scriptContext->GetThreadContext()->IsScriptActive() && !Js::JavascriptConversion::IsCallable(function)),
        "Can't call function when the script context is closed");

    if (scriptContext->GetThreadContext()->IsScriptActive())
    {TRACE_IT(66);
        return;
    }
    if (function->IsExternal())
    {TRACE_IT(67);
        return;
    }
    
    Js::TypeId typeId = Js::JavascriptOperators::GetTypeId(function);
    if (typeId == Js::TypeIds_HostDispatch)
    {
        AssertMsg(false, "Has to go through CallRootFunction to start calling Javascript function");
    }
    else if (typeId == Js::TypeId::TypeIds_Function)
    {TRACE_IT(68);
        if (((Js::JavascriptFunction*)function)->IsExternalFunction())
        {TRACE_IT(69);
            return;
        }
        else if (((Js::JavascriptFunction*)function)->IsWinRTFunction())
        {TRACE_IT(70);
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
{TRACE_IT(71);
    nativeCodeGen->CreateProfiler(profiler);
}

void
ProfilePrintNativeCodeGen(NativeCodeGenerator * nativeCodeGen)
{TRACE_IT(72);
    nativeCodeGen->ProfilePrint();
}

void
SetProfilerFromNativeCodeGen(NativeCodeGenerator * toNativeCodeGen, NativeCodeGenerator * fromNativeCodeGen)
{TRACE_IT(73);
    toNativeCodeGen->SetProfilerFromNativeCodeGen(fromNativeCodeGen);
}
#endif

void DeleteNativeCodeData(NativeCodeData * data)
{TRACE_IT(74);
    if (data)
    {TRACE_IT(75);
        HeapDelete(data);
    }
}
