//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#ifdef ENABLE_WASM
#include "../WasmReader/WasmReaderPch.h"
// Included for AsmJsDefaultEntryThunk
#include "Language/InterpreterStackFrame.h"
#include "Language/AsmJsUtils.h"

namespace Js
{
Var WebAssembly::EntryCompile(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
    ScriptContext* scriptContext = function->GetScriptContext();

    Assert(!(callInfo.Flags & CallFlags_New));

    WebAssemblyModule * module = nullptr;
    try
    {TRACE_IT(64320);
        if (args.Info.Count < 2)
        {TRACE_IT(64321);
            JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedBufferSource);
        }

        BYTE* buffer;
        uint byteLength;
        WebAssembly::ReadBufferSource(args[1], scriptContext, &buffer, &byteLength);

        module = WebAssemblyModule::CreateModule(scriptContext, buffer, byteLength);
    }
    catch (JavascriptError & e)
    {TRACE_IT(64322);
        return JavascriptPromise::CreateRejectedPromise(&e, scriptContext);
    }

    Assert(module);

    return JavascriptPromise::CreateResolvedPromise(module, scriptContext);
}

Var WebAssembly::EntryInstantiate(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
    ScriptContext* scriptContext = function->GetScriptContext();

    Assert(!(callInfo.Flags & CallFlags_New));

    Var resultObject = nullptr;
    try
    {TRACE_IT(64323);
        if (args.Info.Count < 2)
        {TRACE_IT(64324);
            JavascriptError::ThrowTypeError(scriptContext, WASMERR_InvalidInstantiateArgument);
        }

        Var importObject = scriptContext->GetLibrary()->GetUndefined();
        if (args.Info.Count >= 3)
        {TRACE_IT(64325);
            importObject = args[2];
        }

        if (WebAssemblyModule::Is(args[1]))
        {TRACE_IT(64326);
            resultObject = WebAssemblyInstance::CreateInstance(WebAssemblyModule::FromVar(args[1]), importObject);
        }
        else
        {TRACE_IT(64327);
            BYTE* buffer;
            uint byteLength;
            WebAssembly::ReadBufferSource(args[1], scriptContext, &buffer, &byteLength);

            WebAssemblyModule * module = WebAssemblyModule::CreateModule(scriptContext, buffer, byteLength);

            WebAssemblyInstance * instance = WebAssemblyInstance::CreateInstance(module, importObject);

            resultObject = JavascriptOperators::NewJavascriptObjectNoArg(scriptContext);

            JavascriptOperators::OP_SetProperty(resultObject, PropertyIds::module, module, scriptContext);

            JavascriptOperators::OP_SetProperty(resultObject, PropertyIds::instance, instance, scriptContext);
        }
    }
    catch (JavascriptError & e)
    {TRACE_IT(64328);
        return JavascriptPromise::CreateRejectedPromise(&e, scriptContext);
    }

    Assert(resultObject != nullptr);

    return JavascriptPromise::CreateResolvedPromise(resultObject, scriptContext);
}

Var WebAssembly::EntryValidate(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");
    ScriptContext* scriptContext = function->GetScriptContext();

    Assert(!(callInfo.Flags & CallFlags_New));

    if (args.Info.Count < 2)
    {TRACE_IT(64329);
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedBufferSource);
    }

    BYTE* buffer;
    uint byteLength;
    WebAssembly::ReadBufferSource(args[1], scriptContext, &buffer, &byteLength);

    if (WebAssemblyModule::ValidateModule(scriptContext, buffer, byteLength))
    {TRACE_IT(64330);
        return scriptContext->GetLibrary()->GetTrue();
    }
    else
    {TRACE_IT(64331);
        return scriptContext->GetLibrary()->GetFalse();
    }
}

uint32
WebAssembly::ToNonWrappingUint32(Var val, ScriptContext * ctx)
{TRACE_IT(64332);
    double i = JavascriptConversion::ToInteger(val, ctx);
    if (i < 0 || i > (double)UINT32_MAX)
    {TRACE_IT(64333);
        JavascriptError::ThrowRangeError(ctx, JSERR_ArgumentOutOfRange);
    }
    return (uint32)i;
}

void
WebAssembly::ReadBufferSource(Var val, ScriptContext * ctx, _Out_ BYTE** buffer, _Out_ uint *byteLength)
{TRACE_IT(64334);
    const BOOL isTypedArray = Js::TypedArrayBase::Is(val);
    const BOOL isArrayBuffer = Js::ArrayBuffer::Is(val);

    *buffer = nullptr;
    *byteLength = 0;

    if (isTypedArray)
    {TRACE_IT(64335);
        Js::TypedArrayBase* array = Js::TypedArrayBase::FromVar(val);
        *buffer = array->GetByteBuffer();
        *byteLength = array->GetByteLength();
    }
    else if (isArrayBuffer)
    {TRACE_IT(64336);
        Js::ArrayBuffer* arrayBuffer = Js::ArrayBuffer::FromVar(val);
        *buffer = arrayBuffer->GetBuffer();
        *byteLength = arrayBuffer->GetByteLength();
    }

    if (*buffer == nullptr || *byteLength == 0)
    {TRACE_IT(64337);
        JavascriptError::ThrowTypeError(ctx, WASMERR_NeedBufferSource);
    }
}

void
WebAssembly::CheckSignature(ScriptContext * scriptContext, Wasm::WasmSignature * sig1, Wasm::WasmSignature * sig2)
{TRACE_IT(64338);
    if (!sig1->IsEquivalent(sig2))
    {TRACE_IT(64339);
        JavascriptError::ThrowWebAssemblyRuntimeError(scriptContext, WASMERR_SignatureMismatch);
    }
}

uint
WebAssembly::GetSignatureSize()
{TRACE_IT(64340);
    return sizeof(Wasm::WasmSignature);
}

}
#endif // ENABLE_WASM
