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
    {LOGMEIN("WebAssembly.cpp] 26\n");
        if (args.Info.Count < 2)
        {LOGMEIN("WebAssembly.cpp] 28\n");
            JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedBufferSource);
        }

        BYTE* buffer;
        uint byteLength;
        WebAssembly::ReadBufferSource(args[1], scriptContext, &buffer, &byteLength);

        module = WebAssemblyModule::CreateModule(scriptContext, buffer, byteLength);
    }
    catch (JavascriptError & e)
    {LOGMEIN("WebAssembly.cpp] 39\n");
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
    {LOGMEIN("WebAssembly.cpp] 60\n");
        if (args.Info.Count < 2)
        {LOGMEIN("WebAssembly.cpp] 62\n");
            JavascriptError::ThrowTypeError(scriptContext, WASMERR_InvalidInstantiateArgument);
        }

        Var importObject = scriptContext->GetLibrary()->GetUndefined();
        if (args.Info.Count >= 3)
        {LOGMEIN("WebAssembly.cpp] 68\n");
            importObject = args[2];
        }

        if (WebAssemblyModule::Is(args[1]))
        {LOGMEIN("WebAssembly.cpp] 73\n");
            resultObject = WebAssemblyInstance::CreateInstance(WebAssemblyModule::FromVar(args[1]), importObject);
        }
        else
        {
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
    {LOGMEIN("WebAssembly.cpp] 94\n");
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
    {LOGMEIN("WebAssembly.cpp] 114\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedBufferSource);
    }

    BYTE* buffer;
    uint byteLength;
    WebAssembly::ReadBufferSource(args[1], scriptContext, &buffer, &byteLength);

    if (WebAssemblyModule::ValidateModule(scriptContext, buffer, byteLength))
    {LOGMEIN("WebAssembly.cpp] 123\n");
        return scriptContext->GetLibrary()->GetTrue();
    }
    else
    {
        return scriptContext->GetLibrary()->GetFalse();
    }
}

uint32
WebAssembly::ToNonWrappingUint32(Var val, ScriptContext * ctx)
{LOGMEIN("WebAssembly.cpp] 134\n");
    double i = JavascriptConversion::ToInteger(val, ctx);
    if (i < 0 || i > (double)UINT32_MAX)
    {LOGMEIN("WebAssembly.cpp] 137\n");
        JavascriptError::ThrowRangeError(ctx, JSERR_ArgumentOutOfRange);
    }
    return (uint32)i;
}

void
WebAssembly::ReadBufferSource(Var val, ScriptContext * ctx, _Out_ BYTE** buffer, _Out_ uint *byteLength)
{LOGMEIN("WebAssembly.cpp] 145\n");
    const BOOL isTypedArray = Js::TypedArrayBase::Is(val);
    const BOOL isArrayBuffer = Js::ArrayBuffer::Is(val);

    *buffer = nullptr;
    *byteLength = 0;

    if (isTypedArray)
    {LOGMEIN("WebAssembly.cpp] 153\n");
        Js::TypedArrayBase* array = Js::TypedArrayBase::FromVar(val);
        *buffer = array->GetByteBuffer();
        *byteLength = array->GetByteLength();
    }
    else if (isArrayBuffer)
    {LOGMEIN("WebAssembly.cpp] 159\n");
        Js::ArrayBuffer* arrayBuffer = Js::ArrayBuffer::FromVar(val);
        *buffer = arrayBuffer->GetBuffer();
        *byteLength = arrayBuffer->GetByteLength();
    }

    if (*buffer == nullptr || *byteLength == 0)
    {LOGMEIN("WebAssembly.cpp] 166\n");
        JavascriptError::ThrowTypeError(ctx, WASMERR_NeedBufferSource);
    }
}

void
WebAssembly::CheckSignature(ScriptContext * scriptContext, Wasm::WasmSignature * sig1, Wasm::WasmSignature * sig2)
{LOGMEIN("WebAssembly.cpp] 173\n");
    if (!sig1->IsEquivalent(sig2))
    {LOGMEIN("WebAssembly.cpp] 175\n");
        JavascriptError::ThrowWebAssemblyRuntimeError(scriptContext, WASMERR_SignatureMismatch);
    }
}

uint
WebAssembly::GetSignatureSize()
{LOGMEIN("WebAssembly.cpp] 182\n");
    return sizeof(Wasm::WasmSignature);
}

}
#endif // ENABLE_WASM
