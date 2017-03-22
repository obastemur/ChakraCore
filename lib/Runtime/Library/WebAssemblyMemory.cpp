//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLibraryPch.h"

#ifdef ENABLE_WASM

namespace Js
{

WebAssemblyMemory::WebAssemblyMemory(ArrayBuffer * buffer, uint32 initial, uint32 maximum, DynamicType * type) :
    DynamicObject(type),
    m_buffer(buffer),
    m_initial(initial),
    m_maximum(maximum)
{LOGMEIN("WebAssemblyMemory.cpp] 17\n");
}

/* static */
bool
WebAssemblyMemory::Is(Var value)
{LOGMEIN("WebAssemblyMemory.cpp] 23\n");
    return JavascriptOperators::GetTypeId(value) == TypeIds_WebAssemblyMemory;
}

/* static */
WebAssemblyMemory *
WebAssemblyMemory::FromVar(Var value)
{LOGMEIN("WebAssemblyMemory.cpp] 30\n");
    Assert(WebAssemblyMemory::Is(value));
    return static_cast<WebAssemblyMemory*>(value);
}

Var
WebAssemblyMemory::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    ScriptContext* scriptContext = function->GetScriptContext();

    AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

    Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
    bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
    Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

    if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
    {LOGMEIN("WebAssemblyMemory.cpp] 50\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("WebAssembly.Memory"));
    }

    if (args.Info.Count < 2 || !JavascriptOperators::IsObject(args[1]))
    {LOGMEIN("WebAssemblyMemory.cpp] 55\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedObject, _u("memoryDescriptor"));
    }
    DynamicObject * memoryDescriptor = JavascriptObject::FromVar(args[1]);

    Var initVar = JavascriptOperators::OP_GetProperty(memoryDescriptor, PropertyIds::initial, scriptContext);
    uint32 initial = WebAssembly::ToNonWrappingUint32(initVar, scriptContext);

    uint32 maximum = UINT_MAX;
    if (JavascriptOperators::OP_HasProperty(memoryDescriptor, PropertyIds::maximum, scriptContext))
    {LOGMEIN("WebAssemblyMemory.cpp] 65\n");
        Var maxVar = JavascriptOperators::OP_GetProperty(memoryDescriptor, PropertyIds::maximum, scriptContext);
        maximum = WebAssembly::ToNonWrappingUint32(maxVar, scriptContext);
    }

    return CreateMemoryObject(initial, maximum, scriptContext);
}

Var
WebAssemblyMemory::EntryGrow(RecyclableObject* function, CallInfo callInfo, ...)
{
    ScriptContext* scriptContext = function->GetScriptContext();

    PROBE_STACK(scriptContext, Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);

    AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

    Assert(!(callInfo.Flags & CallFlags_New));

    if (!WebAssemblyMemory::Is(args[0]))
    {LOGMEIN("WebAssemblyMemory.cpp] 87\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedMemoryObject);
    }

    WebAssemblyMemory* memory = WebAssemblyMemory::FromVar(args[0]);
    Assert(ArrayBuffer::Is(memory->m_buffer));

    if (memory->m_buffer->IsDetached())
    {LOGMEIN("WebAssemblyMemory.cpp] 95\n");
        JavascriptError::ThrowTypeError(scriptContext, JSERR_DetachedTypedArray);
    }

    Var deltaVar = scriptContext->GetLibrary()->GetUndefined();
    if (args.Info.Count >= 2)
    {LOGMEIN("WebAssemblyMemory.cpp] 101\n");
        deltaVar = args[1];
    }
    uint32 deltaPages = WebAssembly::ToNonWrappingUint32(deltaVar, scriptContext);

    int32 oldPageCount = memory->GrowInternal(deltaPages);
    if (oldPageCount == -1)
    {LOGMEIN("WebAssemblyMemory.cpp] 108\n");
        JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgumentOutOfRange);
    }

    return JavascriptNumber::ToVar(oldPageCount, scriptContext);
}

int32
WebAssemblyMemory::GrowInternal(uint32 deltaPages)
{LOGMEIN("WebAssemblyMemory.cpp] 117\n");
    const uint64 deltaBytes = (uint64)deltaPages * WebAssembly::PageSize;
    if (deltaBytes > ArrayBuffer::MaxArrayBufferLength)
    {LOGMEIN("WebAssemblyMemory.cpp] 120\n");
        return -1;
    }
    const uint32 oldBytes = m_buffer->GetByteLength();
    const uint64 newBytesLong = deltaBytes + oldBytes;
    if (newBytesLong > ArrayBuffer::MaxArrayBufferLength)
    {LOGMEIN("WebAssemblyMemory.cpp] 126\n");
        return -1;
    }
    CompileAssert(ArrayBuffer::MaxArrayBufferLength <= UINT32_MAX);
    const uint32 newBytes = (uint32)newBytesLong;

    const uint32 oldPageCount = oldBytes / WebAssembly::PageSize;
    Assert(oldBytes % WebAssembly::PageSize == 0);

    if (deltaBytes == 0)
    {LOGMEIN("WebAssemblyMemory.cpp] 136\n");
        return (int32)oldPageCount;
    }

    const uint32 newPageCount = oldPageCount + deltaPages;
    if (newPageCount > m_maximum)
    {LOGMEIN("WebAssemblyMemory.cpp] 142\n");
        return -1;
    }

    ArrayBuffer * newBuffer = nullptr;
    JavascriptExceptionObject* caughtExceptionObject = nullptr;
    try
    {LOGMEIN("WebAssemblyMemory.cpp] 149\n");
        newBuffer = m_buffer->TransferInternal(newBytes);
    }
    catch (const JavascriptException& err)
    {LOGMEIN("WebAssemblyMemory.cpp] 153\n");
        caughtExceptionObject = err.GetAndClear();
        Assert(caughtExceptionObject && caughtExceptionObject == ThreadContext::GetContextForCurrentThread()->GetPendingOOMErrorObject());
        return -1;
    }

    Assert(newBuffer);
    m_buffer = newBuffer;
    CompileAssert(ArrayBuffer::MaxArrayBufferLength / WebAssembly::PageSize <= INT32_MAX);
    return (int32)oldPageCount;
}

int32
WebAssemblyMemory::GrowHelper(WebAssemblyMemory * mem, uint32 deltaPages)
{LOGMEIN("WebAssemblyMemory.cpp] 167\n");
    return mem->GrowInternal(deltaPages);
}

Var
WebAssemblyMemory::EntryGetterBuffer(RecyclableObject* function, CallInfo callInfo, ...)
{
    PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

    ARGUMENTS(args, callInfo);
    ScriptContext* scriptContext = function->GetScriptContext();

    Assert(!(callInfo.Flags & CallFlags_New));

    if (args.Info.Count == 0 || !WebAssemblyMemory::Is(args[0]))
    {LOGMEIN("WebAssemblyMemory.cpp] 182\n");
        JavascriptError::ThrowTypeError(scriptContext, WASMERR_NeedMemoryObject);
    }

    WebAssemblyMemory* memory = WebAssemblyMemory::FromVar(args[0]);
    Assert(ArrayBuffer::Is(memory->m_buffer));
    return memory->m_buffer;
}

WebAssemblyMemory *
WebAssemblyMemory::CreateMemoryObject(uint32 initial, uint32 maximum, ScriptContext * scriptContext)
{LOGMEIN("WebAssemblyMemory.cpp] 193\n");
    uint32 byteLength = UInt32Math::Mul<WebAssembly::PageSize>(initial);
    ArrayBuffer* buffer;
#if ENABLE_FAST_ARRAYBUFFER
    if (CONFIG_FLAG(WasmFastArray))
    {LOGMEIN("WebAssemblyMemory.cpp] 198\n");
        buffer = scriptContext->GetLibrary()->CreateWebAssemblyArrayBuffer(byteLength);
    }
    else
#endif
    {
        buffer = scriptContext->GetLibrary()->CreateArrayBuffer(byteLength);
    }
    return RecyclerNewFinalized(scriptContext->GetRecycler(), WebAssemblyMemory, buffer, initial, maximum, scriptContext->GetLibrary()->GetWebAssemblyMemoryType());
}

ArrayBuffer *
WebAssemblyMemory::GetBuffer() const
{LOGMEIN("WebAssemblyMemory.cpp] 211\n");
    return m_buffer;
}

uint
WebAssemblyMemory::GetInitialLength() const
{LOGMEIN("WebAssemblyMemory.cpp] 217\n");
    return m_initial;
}

uint
WebAssemblyMemory::GetMaximumLength() const
{LOGMEIN("WebAssemblyMemory.cpp] 223\n");
    return m_maximum;
}

} // namespace Js
#endif // ENABLE_WASM
