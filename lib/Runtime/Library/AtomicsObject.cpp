//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#define ATOMICS_FUNCTION_ENTRY_CHECKS(length, methodName) \
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault); \
        ARGUMENTS(args, callInfo); \
        ScriptContext* scriptContext = function->GetScriptContext(); \
        Assert(!(callInfo.Flags & CallFlags_New)); \
        if (args.Info.Count <= length) \
        {LOGMEIN("AtomicsObject.cpp] 12\n"); \
            JavascriptError::ThrowRangeError(scriptContext, JSERR_WinRTFunction_TooFewArguments, _u(methodName)); \
        } \


namespace Js
{
    Var AtomicsObject::ValidateSharedIntegerTypedArray(Var typedArray, ScriptContext *scriptContext, bool onlyInt32)
    {LOGMEIN("AtomicsObject.cpp] 20\n");
        if (!TypedArrayBase::Is(typedArray))
        {LOGMEIN("AtomicsObject.cpp] 22\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedTypedArrayObject);
        }

        if (onlyInt32)
        {LOGMEIN("AtomicsObject.cpp] 27\n");
            if (!Int32Array::Is(typedArray))
            {LOGMEIN("AtomicsObject.cpp] 29\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidOperationOnTypedArray);
            }
        }
        else
        {
            if (!(Int8Array::Is(typedArray) || Uint8Array::Is(typedArray) || Int16Array::Is(typedArray) ||
                Uint16Array::Is(typedArray) || Int32Array::Is(typedArray) || Uint32Array::Is(typedArray)))
            {LOGMEIN("AtomicsObject.cpp] 37\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_InvalidOperationOnTypedArray);
            }
        }

        TypedArrayBase *typedArrayBase = TypedArrayBase::FromVar(typedArray);
        ArrayBufferBase* arrayBuffer = typedArrayBase->GetArrayBuffer();
        if (arrayBuffer == nullptr || !ArrayBufferBase::Is(arrayBuffer) || !arrayBuffer->IsSharedArrayBuffer())
        {LOGMEIN("AtomicsObject.cpp] 45\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedSharedArrayBufferObject);
        }

        return arrayBuffer;
    }

    uint32 AtomicsObject::ValidateAtomicAccess(Var typedArray, Var requestIndex, ScriptContext *scriptContext)
    {LOGMEIN("AtomicsObject.cpp] 53\n");
        int32 accessIndex = -1;
        if (TaggedInt::Is(requestIndex))
        {LOGMEIN("AtomicsObject.cpp] 56\n");
            accessIndex = TaggedInt::ToInt32(requestIndex);
        }
        else
        {
            accessIndex = JavascriptConversion::ToInt32_Full(requestIndex, scriptContext);
            double dblValue = JavascriptConversion::ToNumber(requestIndex, scriptContext);
            if (dblValue != accessIndex)
            {LOGMEIN("AtomicsObject.cpp] 64\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidTypedArrayIndex);
            }
        }

        Assert(TypedArrayBase::Is(typedArray));
        
        if (accessIndex < 0 || accessIndex >= (int32)TypedArrayBase::FromVar(typedArray)->GetLength())
        {LOGMEIN("AtomicsObject.cpp] 72\n");
            JavascriptError::ThrowRangeError(scriptContext, JSERR_InvalidTypedArrayIndex);
        }

        return (uint32)accessIndex;
    }

    TypedArrayBase * AtomicsObject::ValidateAndGetTypedArray(Var typedArray, Var index, __out uint32 *accessIndex, ScriptContext *scriptContext, bool onlyInt32)
    {
        ValidateSharedIntegerTypedArray(typedArray, scriptContext, onlyInt32);
        uint32 i = ValidateAtomicAccess(typedArray, index, scriptContext);
        if (accessIndex != nullptr)
        {LOGMEIN("AtomicsObject.cpp] 84\n");
            *accessIndex = i;
        }

        return TypedArrayBase::FromVar(typedArray);
    }

    Var AtomicsObject::EntryAdd(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(3, "Atomics.add");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);
        return typedArrayBase->TypedAdd(accessIndex, args[3]);
    }

    Var AtomicsObject::EntryAnd(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(3, "Atomics.and");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);
        return typedArrayBase->TypedAnd(accessIndex, args[3]);
    }

    Var AtomicsObject::EntryCompareExchange(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(4, "Atomics.compareExchange");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);

        return typedArrayBase->TypedCompareExchange(accessIndex, args[3], args[4]);
    }

    Var AtomicsObject::EntryExchange(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(3, "Atomics.exchange");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);
        return typedArrayBase->TypedExchange(accessIndex, args[3]);
    }

    Var AtomicsObject::EntryIsLockFree(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(1, "Atomics.isLockFree");
        uint32 size = JavascriptConversion::ToUInt32(args[1], scriptContext);
        // TODO : Currently for the size 1, 2 and 4 we are treating them lock free, we will take a look at this later.
        return (size == 1 || size == 2 || size == 4) ? scriptContext->GetLibrary()->GetTrue() : scriptContext->GetLibrary()->GetFalse();
    }

    Var AtomicsObject::EntryLoad(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(2, "Atomics.load");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);
        return typedArrayBase->TypedLoad(accessIndex);
    }

    Var AtomicsObject::EntryOr(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(3, "Atomics.or");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);
        return typedArrayBase->TypedOr(accessIndex, args[3]);
    }

    Var AtomicsObject::EntryStore(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(3, "Atomics.store");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);
        return typedArrayBase->TypedStore(accessIndex, args[3]);
    }

    Var AtomicsObject::EntrySub(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(3, "Atomics.sub");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);
        return typedArrayBase->TypedSub(accessIndex, args[3]);
    }

    Var AtomicsObject::EntryWait(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(3, "Atomics.wait");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext, true /*onlyInt32*/);

        int32 value = JavascriptConversion::ToInt32(args[3], scriptContext);
        uint32 timeout = INFINITE;

        if (args.Info.Count > 4 && !JavascriptOperators::IsUndefinedObject(args[4]))
        {LOGMEIN("AtomicsObject.cpp] 183\n");
            double t =JavascriptConversion::ToNumber(args[4], scriptContext);
            if (!(NumberUtilities::IsNan(t) || JavascriptNumber::IsPosInf(t)))
            {LOGMEIN("AtomicsObject.cpp] 186\n");
                int32 t1 = JavascriptConversion::ToInt32(t);
                timeout = (uint32)max(0, t1);
            }
        }

        if (!AgentOfBuffer::AgentCanSuspend(scriptContext))
        {LOGMEIN("AtomicsObject.cpp] 193\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_CannotSuspendBuffer);
        }

        Assert(typedArrayBase->GetBytesPerElement() == 4);
        uint32 bufferIndex = (accessIndex * 4) + typedArrayBase->GetByteOffset();
        Assert(bufferIndex < typedArrayBase->GetArrayBuffer()->GetByteLength());
        SharedArrayBuffer *sharedArrayBuffer = typedArrayBase->GetArrayBuffer()->GetAsSharedArrayBuffer();
        WaiterList *waiterList = sharedArrayBuffer->GetWaiterList(bufferIndex);


        bool awoken = false;

        {
            AutoCriticalSection autoCS(waiterList->GetCriticalSectionForAccess());

            int32 w = JavascriptConversion::ToInt32(typedArrayBase->DirectGetItem(accessIndex), scriptContext);
            if (value != w)
            {LOGMEIN("AtomicsObject.cpp] 211\n");
                return scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("not-equal"));
            }

            DWORD_PTR agent = (DWORD_PTR)scriptContext;
            Assert(sharedArrayBuffer->GetSharedContents()->IsValidAgent(agent));
            awoken = waiterList->AddAndSuspendWaiter(agent, timeout);
            waiterList->RemoveWaiter(agent);
        }

        return awoken ? scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("ok"))
            : scriptContext->GetLibrary()->CreateStringFromCppLiteral(_u("timed-out"));
    }

    Var AtomicsObject::EntryWake(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(2, "Atomics.wake");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext, true /*onlyInt32*/);
        int32 count = INT_MAX;
        if (args.Info.Count > 3 && !JavascriptOperators::IsUndefinedObject(args[3]))
        {LOGMEIN("AtomicsObject.cpp] 233\n");
            double d = JavascriptConversion::ToInteger(args[3], scriptContext);
            if (!(NumberUtilities::IsNan(d) || JavascriptNumber::IsPosInf(d)))
            {LOGMEIN("AtomicsObject.cpp] 236\n");
                int32 c = JavascriptConversion::ToInt32(d);
                count = max(0, c);
            }
        }

        Assert(typedArrayBase->GetBytesPerElement() == 4);
        uint32 bufferIndex = (accessIndex * 4) + typedArrayBase->GetByteOffset();
        Assert(bufferIndex < typedArrayBase->GetArrayBuffer()->GetByteLength());
        SharedArrayBuffer *sharedArrayBuffer = typedArrayBase->GetArrayBuffer()->GetAsSharedArrayBuffer();
        WaiterList *waiterList = sharedArrayBuffer->GetWaiterList(bufferIndex);

        uint32 removed = 0;
        {
            AutoCriticalSection autoCS(waiterList->GetCriticalSectionForAccess());
            removed = waiterList->RemoveAndWakeWaiters(count);
        }

        return JavascriptNumber::ToVar(removed, scriptContext);
    }

    Var AtomicsObject::EntryXor(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ATOMICS_FUNCTION_ENTRY_CHECKS(3, "Atomics.xor");

        uint32 accessIndex = 0;
        TypedArrayBase *typedArrayBase = ValidateAndGetTypedArray(args[1], args[2], &accessIndex, scriptContext);
        return typedArrayBase->TypedXor(accessIndex, args[3]);
    }
}
