//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLibraryPch.h"

#ifdef ENABLE_WASM
#include "../WasmReader/WasmReaderPch.h"
// Included for AsmJsDefaultEntryThunk
#include "Language/InterpreterStackFrame.h"
namespace Js
{

Var GetImportVariable(Wasm::WasmImport* wi, ScriptContext* ctx, Var ffi)
{LOGMEIN("WebAssemblyInstance.cpp] 15\n");
    PropertyRecord const * modPropertyRecord = nullptr;
    const char16* modName = wi->modName;
    uint32 modNameLen = wi->modNameLen;
    ctx->GetOrAddPropertyRecord(modName, modNameLen, &modPropertyRecord);
    Var modProp = JavascriptOperators::OP_GetProperty(ffi, modPropertyRecord->GetPropertyId(), ctx);

    const char16* name = wi->importName;
    uint32 nameLen = wi->importNameLen;
    PropertyRecord const * propertyRecord = nullptr;
    ctx->GetOrAddPropertyRecord(name, nameLen, &propertyRecord);

    if (!RecyclableObject::Is(modProp))
    {LOGMEIN("WebAssemblyInstance.cpp] 28\n");
        JavascriptError::ThrowTypeError(ctx, WASMERR_InvalidImport);
    }
    return JavascriptOperators::OP_GetProperty(modProp, propertyRecord->GetPropertyId(), ctx);
}

WebAssemblyInstance::WebAssemblyInstance(WebAssemblyModule * wasmModule, DynamicType * type) :
    DynamicObject(type),
    m_module(wasmModule)
{LOGMEIN("WebAssemblyInstance.cpp] 37\n");
}

/* static */
bool
WebAssemblyInstance::Is(Var value)
{LOGMEIN("WebAssemblyInstance.cpp] 43\n");
    return JavascriptOperators::GetTypeId(value) == TypeIds_WebAssemblyInstance;
}

/* static */
WebAssemblyInstance *
WebAssemblyInstance::FromVar(Var value)
{LOGMEIN("WebAssemblyInstance.cpp] 50\n");
    Assert(WebAssemblyInstance::Is(value));
    return static_cast<WebAssemblyInstance*>(value);
}

// Implements "new WebAssembly.Instance(moduleObject [, importObject])" as described here:
// https://github.com/WebAssembly/design/blob/master/JS.md#webassemblyinstance-constructor
Var
WebAssemblyInstance::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    ScriptContext* scriptContext = function->GetScriptContext();

    AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

    Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
    bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
    Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

    if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
    {LOGMEIN("WebAssemblyInstance.cpp] 72\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("WebAssembly.Instance"));
    }

    if (args.Info.Count < 2 || !WebAssemblyModule::Is(args[1]))
    {LOGMEIN("WebAssemblyInstance.cpp] 77\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedModule);
    }
    WebAssemblyModule * module = WebAssemblyModule::FromVar(args[1]);

    Var importObject = scriptContext->GetLibrary()->GetUndefined();
    if (args.Info.Count >= 3)
    {LOGMEIN("WebAssemblyInstance.cpp] 84\n");
        importObject = args[2];
    }

    return CreateInstance(module, importObject);
}

WebAssemblyInstance *
WebAssemblyInstance::CreateInstance(WebAssemblyModule * module, Var importObject)
{LOGMEIN("WebAssemblyInstance.cpp] 93\n");
    if (!JavascriptOperators::IsUndefined(importObject) && !JavascriptOperators::IsObject(importObject))
    {LOGMEIN("WebAssemblyInstance.cpp] 95\n");
        JavascriptError::ThrowTypeError(module->GetScriptContext(), JSERR_NeedObject);
    }

    if (module->GetImportCount() > 0 && !JavascriptOperators::IsObject(importObject))
    {LOGMEIN("WebAssemblyInstance.cpp] 100\n");
        JavascriptError::ThrowTypeError(module->GetScriptContext(), JSERR_NeedObject);
    }

    ScriptContext * scriptContext = module->GetScriptContext();
    WebAssemblyEnvironment environment(module);
    WebAssemblyInstance * newInstance = RecyclerNewZ(scriptContext->GetRecycler(), WebAssemblyInstance, module, scriptContext->GetLibrary()->GetWebAssemblyInstanceType());
    try
    {
        LoadImports(module, scriptContext, importObject, &environment);
        LoadGlobals(module, scriptContext, &environment);
        LoadFunctions(module, scriptContext, &environment);
        ValidateTableAndMemory(module, scriptContext, &environment);
        try
        {
            LoadDataSegs(module, scriptContext, &environment);
            LoadIndirectFunctionTable(module, scriptContext, &environment);
        }
        catch (...)
        {
            AssertMsg(UNREACHED, "By spec, we should not have any exceptions possible here");
            throw;
        }
        Js::Var exportsNamespace = BuildObject(module, scriptContext, &environment);
        JavascriptOperators::OP_SetProperty(newInstance, PropertyIds::exports, exportsNamespace, scriptContext);
    }
    catch (Wasm::WasmCompilationException& e)
    {LOGMEIN("WebAssemblyInstance.cpp] 127\n");
        JavascriptError::ThrowWebAssemblyLinkErrorVar(scriptContext, WASMERR_WasmLinkError, e.ReleaseErrorMessage());
    }

    uint32 startFuncIdx = module->GetStartFunction();
    if (startFuncIdx != Js::Constants::UninitializedValue)
    {LOGMEIN("WebAssemblyInstance.cpp] 133\n");
        AsmJsScriptFunction* start = environment.GetWasmFunction(startFuncIdx);
        Js::CallInfo info(Js::CallFlags_New, 1);
        Js::Arguments startArg(info, (Var*)&start);
        Js::JavascriptFunction::CallFunction<true>(start, start->GetEntryPoint(), startArg);
    }

    return newInstance;
}

void WebAssemblyInstance::LoadFunctions(WebAssemblyModule * wasmModule, ScriptContext* ctx, WebAssemblyEnvironment* env)
{LOGMEIN("WebAssemblyInstance.cpp] 144\n");
    FrameDisplay * frameDisplay = RecyclerNewPlus(ctx->GetRecycler(), sizeof(void*), FrameDisplay, 1);
    frameDisplay->SetItem(0, env->GetStartPtr());

    for (uint i = 0; i < wasmModule->GetWasmFunctionCount(); ++i)
    {LOGMEIN("WebAssemblyInstance.cpp] 149\n");
        if (i < wasmModule->GetImportedFunctionCount() && env->GetWasmFunction(i) != nullptr)
        {LOGMEIN("WebAssemblyInstance.cpp] 151\n");
            continue;
        }
        AsmJsScriptFunction * funcObj = ctx->GetLibrary()->CreateAsmJsScriptFunction(wasmModule->GetWasmFunctionInfo(i)->GetBody());
        FunctionBody* body = funcObj->GetFunctionBody();
        funcObj->SetModuleMemory((Field(Var)*)env->GetStartPtr());
        funcObj->SetSignature(body->GetAsmJsFunctionInfo()->GetWasmSignature());
        funcObj->SetEnvironment(frameDisplay);
        env->SetWasmFunction(i, funcObj);

        if (!PHASE_OFF(WasmDeferredPhase, body))
        {LOGMEIN("WebAssemblyInstance.cpp] 162\n");
            // if we still have WasmReaderInfo we haven't yet parsed
            if (body->GetAsmJsFunctionInfo()->GetWasmReaderInfo())
            {LOGMEIN("WebAssemblyInstance.cpp] 165\n");
                WasmLibrary::SetWasmEntryPointToInterpreter(funcObj, true);
            }
        }
        else
        {
            AsmJsFunctionInfo* info = body->GetAsmJsFunctionInfo();
            if (info->GetWasmReaderInfo())
            {LOGMEIN("WebAssemblyInstance.cpp] 173\n");
                WasmLibrary::SetWasmEntryPointToInterpreter(funcObj, false);
#if ENABLE_DEBUG_CONFIG_OPTIONS
                // Do MTJRC/MAIC:0 check
                const bool noJit = PHASE_OFF(BackEndPhase, body) || PHASE_OFF(FullJitPhase, body) || ctx->GetConfig()->IsNoNative();
                if (!noJit && (CONFIG_FLAG(ForceNative) || CONFIG_FLAG(MaxAsmJsInterpreterRunCount) == 0))
                {LOGMEIN("WebAssemblyInstance.cpp] 179\n");
                    GenerateFunction(ctx->GetNativeCodeGenerator(), body, funcObj);
                    body->SetIsAsmJsFullJitScheduled(true);
                }
#endif
                info->SetWasmReaderInfo(nullptr);
            }
        }
    }
}

void WebAssemblyInstance::LoadDataSegs(WebAssemblyModule * wasmModule, ScriptContext* ctx, WebAssemblyEnvironment* env)
{LOGMEIN("WebAssemblyInstance.cpp] 191\n");
    WebAssemblyMemory* mem = env->GetMemory(0);
    Assert(mem);
    ArrayBuffer* buffer = mem->GetBuffer();

    for (uint32 iSeg = 0; iSeg < wasmModule->GetDataSegCount(); ++iSeg)
    {LOGMEIN("WebAssemblyInstance.cpp] 197\n");
        Wasm::WasmDataSegment* segment = wasmModule->GetDataSeg(iSeg);
        Assert(segment != nullptr);
        const uint32 offset = env->GetDataSegmentOffset(iSeg);
        const uint32 size = segment->GetSourceSize();

        if (size > 0)
        {LOGMEIN("WebAssemblyInstance.cpp] 204\n");
            js_memcpy_s(buffer->GetBuffer() + offset, (uint32)buffer->GetByteLength() - offset, segment->GetData(), size);
        }
    }
}

Var WebAssemblyInstance::BuildObject(WebAssemblyModule * wasmModule, ScriptContext* scriptContext, WebAssemblyEnvironment* env)
{LOGMEIN("WebAssemblyInstance.cpp] 211\n");
    Js::Var exportsNamespace = JavascriptOperators::NewJavascriptObjectNoArg(scriptContext);
    for (uint32 iExport = 0; iExport < wasmModule->GetExportCount(); ++iExport)
    {LOGMEIN("WebAssemblyInstance.cpp] 214\n");
        Wasm::WasmExport* wasmExport = wasmModule->GetExport(iExport);
        Assert(wasmExport);
        if (wasmExport)
        {LOGMEIN("WebAssemblyInstance.cpp] 218\n");
            PropertyRecord const * propertyRecord = nullptr;
            scriptContext->GetOrAddPropertyRecord(wasmExport->name, wasmExport->nameLength, &propertyRecord);

            Var obj = scriptContext->GetLibrary()->GetUndefined();
            switch (wasmExport->kind)
            {LOGMEIN("WebAssemblyInstance.cpp] 224\n");
            case Wasm::ExternalKinds::Table:
                obj = env->GetTable(wasmExport->index);
                break;
            case Wasm::ExternalKinds::Memory:
                obj = env->GetMemory(wasmExport->index);
                break;
            case Wasm::ExternalKinds::Function:
                obj = env->GetWasmFunction(wasmExport->index);
                break;
            case Wasm::ExternalKinds::Global:
                Wasm::WasmGlobal* global = wasmModule->GetGlobal(wasmExport->index);
                if (global->IsMutable())
                {LOGMEIN("WebAssemblyInstance.cpp] 237\n");
                    JavascriptError::ThrowTypeError(wasmModule->GetScriptContext(), WASMERR_MutableGlobal);
                }
                Wasm::WasmConstLitNode cnst = env->GetGlobalValue(global);
                switch (global->GetType())
                {LOGMEIN("WebAssemblyInstance.cpp] 242\n");
                case Wasm::WasmTypes::I32:
                    obj = JavascriptNumber::ToVar(cnst.i32, scriptContext);
                    break;
                case Wasm::WasmTypes::F32:
                    obj = JavascriptNumber::New(cnst.f32, scriptContext);
                    break;
                case Wasm::WasmTypes::F64:
                    obj = JavascriptNumber::New(cnst.f64, scriptContext);
                    break;
                case Wasm::WasmTypes::I64:
                    JavascriptError::ThrowTypeError(wasmModule->GetScriptContext(), WASMERR_InvalidTypeConversion);
                default:
                    Assert(UNREACHED);
                    break;
                }
            }
            JavascriptOperators::OP_SetProperty(exportsNamespace, propertyRecord->GetPropertyId(), obj, scriptContext);
        }
    }
    return exportsNamespace;
}

void WebAssemblyInstance::LoadImports(
    WebAssemblyModule * wasmModule,
    ScriptContext* ctx,
    Var ffi,
    WebAssemblyEnvironment* env)
{LOGMEIN("WebAssemblyInstance.cpp] 270\n");
    const uint32 importCount = wasmModule->GetImportCount();
    if (importCount > 0 && (!ffi || !RecyclableObject::Is(ffi)))
    {LOGMEIN("WebAssemblyInstance.cpp] 273\n");
        JavascriptError::ThrowTypeError(ctx, WASMERR_InvalidImport);
    }

    uint32 counters[Wasm::ExternalKinds::Limit];
    memset(counters, 0, sizeof(counters));
    for (uint32 i = 0; i < importCount; ++i)
    {LOGMEIN("WebAssemblyInstance.cpp] 280\n");
        Wasm::WasmImport* import = wasmModule->GetImport(i);
        Var prop = GetImportVariable(import, ctx, ffi);
        uint32& counter = counters[import->kind];
        switch (import->kind)
        {LOGMEIN("WebAssemblyInstance.cpp] 285\n");
        case Wasm::ExternalKinds::Function:
        {LOGMEIN("WebAssemblyInstance.cpp] 287\n");
            if (!JavascriptFunction::Is(prop))
            {LOGMEIN("WebAssemblyInstance.cpp] 289\n");
                JavascriptError::ThrowWebAssemblyLinkError(ctx, JSERR_Property_NeedFunction);
            }
            Assert(counter < wasmModule->GetImportedFunctionCount());
            Assert(wasmModule->GetFunctionIndexType(counter) == Wasm::FunctionIndexTypes::ImportThunk);

            env->SetImportedFunction(counter, prop);
            if (AsmJsScriptFunction::IsWasmScriptFunction(prop))
            {LOGMEIN("WebAssemblyInstance.cpp] 297\n");
                Assert(env->GetWasmFunction(counter) == nullptr);
                AsmJsScriptFunction* func = AsmJsScriptFunction::FromVar(prop);
                if (!wasmModule->GetWasmFunctionInfo(counter)->GetSignature()->IsEquivalent(func->GetSignature()))
                {LOGMEIN("WebAssemblyInstance.cpp] 301\n");
                    JavascriptError::ThrowWebAssemblyLinkError(ctx, WASMERR_SignatureMismatch);
                }
                // Imported Wasm functions can be called directly
                env->SetWasmFunction(counter, func);
            }
            break;
        }
        case Wasm::ExternalKinds::Memory:
        {LOGMEIN("WebAssemblyInstance.cpp] 310\n");
            Assert(wasmModule->HasMemoryImport());
            if (wasmModule->HasMemoryImport())
            {LOGMEIN("WebAssemblyInstance.cpp] 313\n");
                if (!WebAssemblyMemory::Is(prop))
                {LOGMEIN("WebAssemblyInstance.cpp] 315\n");
                    JavascriptError::ThrowWebAssemblyLinkError(ctx, WASMERR_NeedMemoryObject);
                }
                WebAssemblyMemory * mem = WebAssemblyMemory::FromVar(prop);

                if (mem->GetInitialLength() < wasmModule->GetMemoryInitSize())
                {LOGMEIN("WebAssemblyInstance.cpp] 321\n");
                    throw Wasm::WasmCompilationException(_u("Imported memory initial size (%u) is smaller than declared (%u)"), mem->GetInitialLength(), wasmModule->GetMemoryInitSize());
                }
                if (mem->GetMaximumLength() > wasmModule->GetMemoryMaxSize())
                {LOGMEIN("WebAssemblyInstance.cpp] 325\n");
                    throw Wasm::WasmCompilationException(_u("Imported memory maximum size (%u) is larger than declared (%u)"),mem->GetMaximumLength(), wasmModule->GetMemoryMaxSize());
                }
                env->SetMemory(counter, mem);
            }
            break;
        }
        case Wasm::ExternalKinds::Table:
        {LOGMEIN("WebAssemblyInstance.cpp] 333\n");
            Assert(wasmModule->HasTableImport());
            if (wasmModule->HasTableImport())
            {LOGMEIN("WebAssemblyInstance.cpp] 336\n");
                if (!WebAssemblyTable::Is(prop))
                {LOGMEIN("WebAssemblyInstance.cpp] 338\n");
                    JavascriptError::ThrowWebAssemblyLinkError(ctx, WASMERR_NeedTableObject);
                }
                WebAssemblyTable * table = WebAssemblyTable::FromVar(prop);

                if (!wasmModule->IsValidTableImport(table))
                {LOGMEIN("WebAssemblyInstance.cpp] 344\n");
                    JavascriptError::ThrowWebAssemblyLinkError(ctx, WASMERR_NeedTableObject);
                }
                env->SetTable(counter, table);
            }
            break;
        }
        case Wasm::ExternalKinds::Global:
        {LOGMEIN("WebAssemblyInstance.cpp] 352\n");
            Wasm::WasmGlobal* global = wasmModule->GetGlobal(counter);
            if (global->IsMutable() || (!JavascriptNumber::Is(prop) && !TaggedInt::Is(prop)))
            {LOGMEIN("WebAssemblyInstance.cpp] 355\n");
                JavascriptError::ThrowWebAssemblyLinkError(ctx, WASMERR_InvalidImport);
            }

            Assert(global->GetReferenceType() == Wasm::GlobalReferenceTypes::ImportedReference);
            Wasm::WasmConstLitNode cnst = {0};
            switch (global->GetType())
            {LOGMEIN("WebAssemblyInstance.cpp] 362\n");
            case Wasm::WasmTypes::I32: cnst.i32 = JavascriptConversion::ToInt32(prop, ctx); break;
            case Wasm::WasmTypes::F32: cnst.f32 = (float)JavascriptConversion::ToNumber(prop, ctx); break;
            case Wasm::WasmTypes::F64: cnst.f64 = JavascriptConversion::ToNumber(prop, ctx); break;
            case Wasm::WasmTypes::I64: Js::JavascriptError::ThrowTypeError(ctx, WASMERR_InvalidTypeConversion);
            default:
                Js::Throw::InternalError();
            }
            env->SetGlobalValue(global, cnst);
            break;
        }
        default:
            Js::Throw::InternalError();
        }
        ++counter;
    }
}

void WebAssemblyInstance::LoadGlobals(WebAssemblyModule * wasmModule, ScriptContext* ctx, WebAssemblyEnvironment* env)
{LOGMEIN("WebAssemblyInstance.cpp] 381\n");
    uint count = wasmModule->GetGlobalCount();
    for (uint i = 0; i < count; i++)
    {LOGMEIN("WebAssemblyInstance.cpp] 384\n");
        Wasm::WasmGlobal* global = wasmModule->GetGlobal(i);
        Wasm::WasmConstLitNode cnst = {};

        if (global->GetReferenceType() == Wasm::GlobalReferenceTypes::ImportedReference)
        {LOGMEIN("WebAssemblyInstance.cpp] 389\n");
            // the value should already be resolved
            continue;
        }

        if (global->GetReferenceType() == Wasm::GlobalReferenceTypes::LocalReference)
        {LOGMEIN("WebAssemblyInstance.cpp] 395\n");
            Wasm::WasmGlobal* sourceGlobal = wasmModule->GetGlobal(global->GetGlobalIndexInit());
            if (sourceGlobal->GetReferenceType() != Wasm::GlobalReferenceTypes::Const &&
                sourceGlobal->GetReferenceType() != Wasm::GlobalReferenceTypes::ImportedReference)
            {LOGMEIN("WebAssemblyInstance.cpp] 399\n");
                JavascriptError::ThrowTypeError(ctx, WASMERR_InvalidGlobalRef);
            }
            
            if (sourceGlobal->GetType() != global->GetType())
            {LOGMEIN("WebAssemblyInstance.cpp] 404\n");
                JavascriptError::ThrowTypeError(ctx, WASMERR_InvalidTypeConversion);
            }
            cnst = env->GetGlobalValue(sourceGlobal);
        }
        else
        {
            cnst = global->GetConstInit();
        }

        env->SetGlobalValue(global, cnst);
    }
}

void WebAssemblyInstance::LoadIndirectFunctionTable(WebAssemblyModule * wasmModule, ScriptContext* ctx, WebAssemblyEnvironment* env)
{LOGMEIN("WebAssemblyInstance.cpp] 419\n");
    WebAssemblyTable* table = env->GetTable(0);
    Assert(table != nullptr);

    for (uint elementsIndex = 0; elementsIndex < wasmModule->GetElementSegCount(); ++elementsIndex)
    {LOGMEIN("WebAssemblyInstance.cpp] 424\n");
        Wasm::WasmElementSegment* eSeg = wasmModule->GetElementSeg(elementsIndex);

        if (eSeg->GetNumElements() > 0)
        {LOGMEIN("WebAssemblyInstance.cpp] 428\n");
            uint offset = env->GetElementSegmentOffset(elementsIndex);
            for (uint segIndex = 0; segIndex < eSeg->GetNumElements(); ++segIndex)
            {LOGMEIN("WebAssemblyInstance.cpp] 431\n");
                uint funcIndex = eSeg->GetElement(segIndex);
                Var funcObj = env->GetWasmFunction(funcIndex);
                table->DirectSetValue(segIndex + offset, funcObj);
            }
        }
    }
}


void WebAssemblyInstance::ValidateTableAndMemory(WebAssemblyModule * wasmModule, ScriptContext* ctx, WebAssemblyEnvironment* env)
{LOGMEIN("WebAssemblyInstance.cpp] 442\n");
    WebAssemblyTable* table = env->GetTable(0);
    if (wasmModule->HasTableImport())
    {LOGMEIN("WebAssemblyInstance.cpp] 445\n");
        if (table == nullptr)
        {LOGMEIN("WebAssemblyInstance.cpp] 447\n");
            JavascriptError::ThrowWebAssemblyLinkError(ctx, WASMERR_NeedTableObject);
        }
    }
    else
    {
        table = wasmModule->CreateTable();
        env->SetTable(0, table);
    }

    WebAssemblyMemory* mem = env->GetMemory(0);
    if (wasmModule->HasMemoryImport())
    {LOGMEIN("WebAssemblyInstance.cpp] 459\n");
        if (mem == nullptr)
        {LOGMEIN("WebAssemblyInstance.cpp] 461\n");
            JavascriptError::ThrowWebAssemblyLinkError(ctx, WASMERR_NeedMemoryObject);
        }
    }
    else
    {
        mem = wasmModule->CreateMemory();
        env->SetMemory(0, mem);
    }
    ArrayBuffer * buffer = mem->GetBuffer();
    if (buffer->IsDetached())
    {LOGMEIN("WebAssemblyInstance.cpp] 472\n");
        JavascriptError::ThrowTypeError(wasmModule->GetScriptContext(), JSERR_DetachedTypedArray);
    }

    env->CalculateOffsets(table, mem);
}

} // namespace Js
#endif // ENABLE_WASM
