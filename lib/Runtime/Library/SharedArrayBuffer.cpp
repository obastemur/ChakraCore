//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
#if DBG
    void SharedContents::AddAgent(DWORD_PTR agent)
    {LOGMEIN("SharedArrayBuffer.cpp] 10\n");
        if (allowedAgents == nullptr)
        {LOGMEIN("SharedArrayBuffer.cpp] 12\n");
            allowedAgents = HeapNew(SharableAgents, &HeapAllocator::Instance);
        }

        allowedAgents->Add(agent);
    }

    bool SharedContents::IsValidAgent(DWORD_PTR agent)
    {LOGMEIN("SharedArrayBuffer.cpp] 20\n");
        return allowedAgents != nullptr && allowedAgents->Contains(agent);
    }
#endif

    void SharedContents::Cleanup()
    {LOGMEIN("SharedArrayBuffer.cpp] 26\n");
        Assert(refCount == 0);
        buffer = nullptr;
        bufferLength = 0;
#if DBG
        if (allowedAgents != nullptr)
        {LOGMEIN("SharedArrayBuffer.cpp] 32\n");
            HeapDelete(allowedAgents);
            allowedAgents = nullptr;
        }
#endif

        if (indexToWaiterList != nullptr)
        {LOGMEIN("SharedArrayBuffer.cpp] 39\n");
            indexToWaiterList->Map([](uint index, WaiterList *waiters) {
                if (waiters != nullptr)
                {LOGMEIN("SharedArrayBuffer.cpp] 42\n");
                    waiters->Cleanup();
                    HeapDelete(waiters);
                    waiters = nullptr;
                }
            });

            HeapDelete(indexToWaiterList);
            indexToWaiterList = nullptr;
        }
    }

    Var SharedArrayBuffer::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Var newTarget = callInfo.Flags & CallFlags_NewTarget ? args.Values[args.Info.Count] : args[0];
        bool isCtorSuperCall = (callInfo.Flags & CallFlags_New) && newTarget != nullptr && !JavascriptOperators::IsUndefined(newTarget);
        Assert(isCtorSuperCall || !(callInfo.Flags & CallFlags_New) || args[0] == nullptr);

        if (!(callInfo.Flags & CallFlags_New) || (newTarget && JavascriptOperators::IsUndefinedObject(newTarget)))
        {LOGMEIN("SharedArrayBuffer.cpp] 68\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_ClassConstructorCannotBeCalledWithoutNew, _u("SharedArrayBuffer"));
        }

        uint32 byteLength = 0;
        if (args.Info.Count > 1)
        {LOGMEIN("SharedArrayBuffer.cpp] 74\n");
            byteLength = ArrayBuffer::ToIndex(args[1], JSERR_ArrayLengthConstructIncorrect, scriptContext, MaxSharedArrayBufferLength);
        }

        RecyclableObject* newArr = scriptContext->GetLibrary()->CreateSharedArrayBuffer(byteLength);

        return isCtorSuperCall ?
            JavascriptOperators::OrdinaryCreateFromConstructor(RecyclableObject::FromVar(newTarget), newArr, nullptr, scriptContext) :
            newArr;
    }

    // SharedArrayBuffer.prototype.byteLength
    Var SharedArrayBuffer::EntryGetterByteLength(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        Assert(!(callInfo.Flags & CallFlags_New));

        if (args.Info.Count == 0 || !SharedArrayBuffer::Is(args[0]))
        {LOGMEIN("SharedArrayBuffer.cpp] 96\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedSharedArrayBufferObject);
        }

        SharedArrayBuffer* sharedArrayBuffer = SharedArrayBuffer::FromVar(args[0]);
        return JavascriptNumber::ToVar(sharedArrayBuffer->GetByteLength(), scriptContext);
    }

    // SharedArrayBuffer.prototype.slice
    Var SharedArrayBuffer::EntrySlice(RecyclableObject* function, CallInfo callInfo, ...)
    {
        PROBE_STACK(function->GetScriptContext(), Js::Constants::MinStackDefault);

        ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        AssertMsg(args.Info.Count > 0, "Should always have implicit 'this'");

        Assert(!(callInfo.Flags & CallFlags_New));

        if (!SharedArrayBuffer::Is(args[0]))
        {LOGMEIN("SharedArrayBuffer.cpp] 117\n");
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedSharedArrayBufferObject);
        }

        JavascriptLibrary* library = scriptContext->GetLibrary();
        SharedArrayBuffer* currentBuffer = SharedArrayBuffer::FromVar(args[0]);

        int64 currentLen = (int64)currentBuffer->GetByteLength();
        int64 start = 0, end = 0;
        int64 newLen = 0;

        // If no start or end arguments, use the entire length
        if (args.Info.Count < 2)
        {LOGMEIN("SharedArrayBuffer.cpp] 130\n");
            newLen = currentLen;
        }
        else
        {
            start = JavascriptArray::GetIndexFromVar(args[1], currentLen, scriptContext);

            // If no end argument, use length as the end
            if (args.Info.Count < 3 || args[2] == library->GetUndefined())
            {LOGMEIN("SharedArrayBuffer.cpp] 139\n");
                end = currentLen;
            }
            else
            {
                end = JavascriptArray::GetIndexFromVar(args[2], currentLen, scriptContext);
            }

            newLen = end > start ? end - start : 0;
        }

        // We can't have allocated an SharedArrayBuffer with byteLength > MaxArrayBufferLength.
        // start and end are clamped to valid indices, so the new length also cannot exceed MaxArrayBufferLength.
        // Therefore, should be safe to cast down newLen to uint32.
        Assert(newLen < MaxSharedArrayBufferLength);
        uint32 newbyteLength = static_cast<uint32>(newLen);

        SharedArrayBuffer* newBuffer = nullptr;

        if (scriptContext->GetConfig()->IsES6SpeciesEnabled())
        {LOGMEIN("SharedArrayBuffer.cpp] 159\n");
            Var constructorVar = JavascriptOperators::SpeciesConstructor(currentBuffer, scriptContext->GetLibrary()->GetSharedArrayBufferConstructor(), scriptContext);

            JavascriptFunction* constructor = JavascriptFunction::FromVar(constructorVar);

            Js::Var constructorArgs[] = { constructor, JavascriptNumber::ToVar(newbyteLength, scriptContext) };
            Js::CallInfo constructorCallInfo(Js::CallFlags_New, _countof(constructorArgs));
            Js::Var newVar = JavascriptOperators::NewScObject(constructor, Js::Arguments(constructorCallInfo, constructorArgs), scriptContext);

            if (!SharedArrayBuffer::Is(newVar))
            {LOGMEIN("SharedArrayBuffer.cpp] 169\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedSharedArrayBufferObject);
            }

            newBuffer = SharedArrayBuffer::FromVar(newVar);

            if (newBuffer == currentBuffer)
            {LOGMEIN("SharedArrayBuffer.cpp] 176\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedSharedArrayBufferObject);
            }

            if (newBuffer->GetByteLength() < newbyteLength)
            {LOGMEIN("SharedArrayBuffer.cpp] 181\n");
                JavascriptError::ThrowTypeError(scriptContext, JSERR_ArgumentOutOfRange, _u("SharedArrayBuffer.prototype.slice"));
            }
        }
        else
        {
            newBuffer = library->CreateSharedArrayBuffer(newbyteLength);
        }

        Assert(newBuffer);
        Assert(newBuffer->GetByteLength() >= newbyteLength);

        // Don't bother doing memcpy if we aren't copying any elements
        if (newbyteLength > 0)
        {LOGMEIN("SharedArrayBuffer.cpp] 195\n");
            AssertMsg(currentBuffer->GetBuffer() != nullptr, "buffer must not be null when we copy from it");

            js_memcpy_s(newBuffer->GetBuffer(), newbyteLength, currentBuffer->GetBuffer() + start, newbyteLength);
        }

        return newBuffer;
    }

    Var SharedArrayBuffer::EntryGetterSymbolSpecies(RecyclableObject* function, CallInfo callInfo, ...)
    {
        ARGUMENTS(args, callInfo);

        Assert(args.Info.Count > 0);

        return args[0];
    }

    SharedArrayBuffer* SharedArrayBuffer::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "var must be an SharedArrayBuffer");

        return static_cast<SharedArrayBuffer *>(RecyclableObject::FromVar(aValue));
    }

    bool  SharedArrayBuffer::Is(Var aValue)
    {LOGMEIN("SharedArrayBuffer.cpp] 221\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_SharedArrayBuffer;
    }

    DetachedStateBase* SharedArrayBuffer::GetSharableState(Var object)
    {LOGMEIN("SharedArrayBuffer.cpp] 226\n");
        Assert(SharedArrayBuffer::Is(object));
        SharedArrayBuffer * sab = SharedArrayBuffer::FromVar(object);
        SharedContents * contents = sab->GetSharedContents();

#if ENABLE_FAST_ARRAYBUFFER
        if (sab->IsValidVirtualBufferLength(contents->bufferLength))
        {LOGMEIN("SharedArrayBuffer.cpp] 233\n");
            return HeapNew(SharableState, contents, ArrayBufferAllocationType::MemAlloc);
        }
        else
        {
            return HeapNew(SharableState, contents, ArrayBufferAllocationType::Heap);
        }
#else
        return HeapNew(SharableState, contents, ArrayBufferAllocationType::Heap);
#endif
    }

    SharedArrayBuffer* SharedArrayBuffer::NewFromSharedState(DetachedStateBase* state, JavascriptLibrary *library)
    {LOGMEIN("SharedArrayBuffer.cpp] 246\n");
        Assert(state->GetTypeId() == TypeIds_SharedArrayBuffer);

        SharableState* sharableState = static_cast<SharableState *>(state);
        if (sharableState->allocationType == ArrayBufferAllocationType::Heap || sharableState->allocationType == ArrayBufferAllocationType::MemAlloc)
        {LOGMEIN("SharedArrayBuffer.cpp] 251\n");
            return library->CreateSharedArrayBuffer(sharableState->contents);
        }

        Assert(false);
        return nullptr;
    }

    template <class Allocator>
    SharedArrayBuffer::SharedArrayBuffer(uint32 length, DynamicType * type, Allocator allocator) :
        ArrayBufferBase(type), sharedContents(nullptr)
    {LOGMEIN("SharedArrayBuffer.cpp] 262\n");
        BYTE * buffer = nullptr;
        if (length > MaxSharedArrayBufferLength)
        {LOGMEIN("SharedArrayBuffer.cpp] 265\n");
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_FunctionArgument_Invalid);
        }
        else if (length > 0)
        {LOGMEIN("SharedArrayBuffer.cpp] 269\n");
            Recycler* recycler = GetType()->GetLibrary()->GetRecycler();
            if (recycler->ReportExternalMemoryAllocation(length))
            {LOGMEIN("SharedArrayBuffer.cpp] 272\n");
                buffer = (BYTE*)allocator(length);
                if (buffer == nullptr)
                {LOGMEIN("SharedArrayBuffer.cpp] 275\n");
                    recycler->ReportExternalMemoryFree(length);
                }
            }

            if (buffer == nullptr)
            {LOGMEIN("SharedArrayBuffer.cpp] 281\n");
                recycler->CollectNow<CollectOnTypedArrayAllocation>();

                if (recycler->ReportExternalMemoryAllocation(length))
                {LOGMEIN("SharedArrayBuffer.cpp] 285\n");
                    buffer = (BYTE*)allocator(length);
                    if (buffer == nullptr)
                    {LOGMEIN("SharedArrayBuffer.cpp] 288\n");
                        recycler->ReportExternalMemoryFailure(length);
                    }
                }
                else
                {
                    JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                }
            }

            if (buffer != nullptr)
            {
                ZeroMemory(buffer, length);
                sharedContents = HeapNew(SharedContents, buffer, length);
                if (sharedContents == nullptr)
                {LOGMEIN("SharedArrayBuffer.cpp] 303\n");
                    recycler->ReportExternalMemoryFailure(length);

                    // What else could we do?
                    JavascriptError::ThrowOutOfMemoryError(GetScriptContext());
                }
#if DBG
                sharedContents->AddAgent((DWORD_PTR)GetScriptContext());
#endif
            }

        }
    }

    SharedArrayBuffer::SharedArrayBuffer(SharedContents * contents, DynamicType * type) :
        ArrayBufferBase(type), sharedContents(contents)
    {LOGMEIN("SharedArrayBuffer.cpp] 319\n");
        if (sharedContents == nullptr || sharedContents->bufferLength > MaxSharedArrayBufferLength)
        {LOGMEIN("SharedArrayBuffer.cpp] 321\n");
            JavascriptError::ThrowTypeError(GetScriptContext(), JSERR_FunctionArgument_Invalid);
        }
        InterlockedIncrement(&sharedContents->refCount);
#if DBG
        sharedContents->AddAgent((DWORD_PTR)GetScriptContext());
#endif
    }

    WaiterList *SharedArrayBuffer::GetWaiterList(uint index)
    {LOGMEIN("SharedArrayBuffer.cpp] 331\n");
        if (sharedContents != nullptr)
        {LOGMEIN("SharedArrayBuffer.cpp] 333\n");
            if (sharedContents->indexToWaiterList == nullptr)
            {LOGMEIN("SharedArrayBuffer.cpp] 335\n");
                sharedContents->indexToWaiterList = HeapNew(IndexToWaitersMap, &HeapAllocator::Instance);
            }

            WaiterList * waiters = nullptr;
            if (!sharedContents->indexToWaiterList->TryGetValue(index, &waiters))
            {LOGMEIN("SharedArrayBuffer.cpp] 341\n");
                waiters = HeapNew(WaiterList);
                sharedContents->indexToWaiterList->Add(index, waiters);
            }
            return waiters;
        }

        Assert(false);
        return nullptr;
    }

    uint32 SharedArrayBuffer::GetByteLength() const 
    {LOGMEIN("SharedArrayBuffer.cpp] 353\n");
        return sharedContents != nullptr ? sharedContents->bufferLength : 0;
    }

    BYTE* SharedArrayBuffer::GetBuffer() const
    {LOGMEIN("SharedArrayBuffer.cpp] 358\n");
        return sharedContents != nullptr ? sharedContents->buffer : nullptr;
    }

    BOOL SharedArrayBuffer::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("SharedArrayBuffer.cpp] 363\n");
        stringBuilder->AppendCppLiteral(_u("Object, (SharedArrayBuffer)"));
        return TRUE;
    }

    BOOL SharedArrayBuffer::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("SharedArrayBuffer.cpp] 369\n");
        stringBuilder->AppendCppLiteral(_u("[object SharedArrayBuffer]"));
        return TRUE;
    }

    JavascriptSharedArrayBuffer::JavascriptSharedArrayBuffer(uint32 length, DynamicType * type) :
        SharedArrayBuffer(length, type, (IsValidVirtualBufferLength(length)) ? AsmJsVirtualAllocator : malloc)
    {LOGMEIN("SharedArrayBuffer.cpp] 376\n");
    }
    JavascriptSharedArrayBuffer::JavascriptSharedArrayBuffer(SharedContents *sharedContents, DynamicType * type) :
        SharedArrayBuffer(sharedContents, type)
    {LOGMEIN("SharedArrayBuffer.cpp] 380\n");
    }

    JavascriptSharedArrayBuffer* JavascriptSharedArrayBuffer::Create(uint32 length, DynamicType * type)
    {LOGMEIN("SharedArrayBuffer.cpp] 384\n");
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        JavascriptSharedArrayBuffer* result = RecyclerNewFinalized(recycler, JavascriptSharedArrayBuffer, length, type);
        Assert(result);
        recycler->AddExternalMemoryUsage(length);
        return result;
    }

    JavascriptSharedArrayBuffer* JavascriptSharedArrayBuffer::Create(SharedContents *sharedContents, DynamicType * type)
    {LOGMEIN("SharedArrayBuffer.cpp] 393\n");
        Recycler* recycler = type->GetScriptContext()->GetRecycler();
        JavascriptSharedArrayBuffer* result = RecyclerNewFinalized(recycler, JavascriptSharedArrayBuffer, sharedContents, type);
        Assert(result != nullptr);
        if (sharedContents != nullptr)
        {LOGMEIN("SharedArrayBuffer.cpp] 398\n");
            // REVIEW : why do we need to increase this? it is the same memory we are sharing.
            recycler->AddExternalMemoryUsage(sharedContents->bufferLength);
        }
        return result;
    }

    bool JavascriptSharedArrayBuffer::IsValidVirtualBufferLength(uint length)
    {LOGMEIN("SharedArrayBuffer.cpp] 406\n");
#if ENABLE_FAST_ARRAYBUFFER
        /*
        1. length >= 2^16
        2. length is power of 2 or (length > 2^24 and length is multiple of 2^24)
        3. length is a multiple of 4K
        */
        return (!PHASE_OFF1(Js::TypedArrayVirtualPhase) &&
            (length >= 0x10000) &&
            (((length & (~length + 1)) == length) ||
                (length >= 0x1000000 &&
                    ((length & 0xFFFFFF) == 0)
                    )
                ) &&
            ((length % AutoSystemInfo::PageSize) == 0)
            );
#else
        return false;
#endif
    }

    void JavascriptSharedArrayBuffer::Finalize(bool isShutdown)
    {LOGMEIN("SharedArrayBuffer.cpp] 428\n");
        if (sharedContents == nullptr)
        {LOGMEIN("SharedArrayBuffer.cpp] 430\n");
            return;
        }

        uint ref = InterlockedDecrement(&sharedContents->refCount);
        if (ref == 0)
        {LOGMEIN("SharedArrayBuffer.cpp] 436\n");
#if ENABLE_FAST_ARRAYBUFFER
            //AsmJS Virtual Free
            //TOD - see if isBufferCleared need to be added for free too
            if (IsValidVirtualBufferLength(sharedContents->bufferLength) && !sharedContents->isBufferCleared)
            {LOGMEIN("SharedArrayBuffer.cpp] 441\n");
                FreeMemAlloc(sharedContents->buffer);
                sharedContents->isBufferCleared = true;
            }
            else
            {
                free(sharedContents->buffer);
            }
#else
            free(sharedContents->buffer);
#endif
            Recycler* recycler = GetType()->GetLibrary()->GetRecycler();
            recycler->ReportExternalMemoryFree(sharedContents->bufferLength);

            sharedContents->Cleanup();
            HeapDelete(sharedContents);
        }

        sharedContents = nullptr;
    }

    void JavascriptSharedArrayBuffer::Dispose(bool isShutdown)
    {LOGMEIN("SharedArrayBuffer.cpp] 463\n");
        /* See JavascriptArrayBuffer::Finalize */
    }

    WaiterList::WaiterList()
        : m_waiters(nullptr)
    {LOGMEIN("SharedArrayBuffer.cpp] 469\n");
        InitWaiterList();
    }

    void WaiterList::InitWaiterList()
    {LOGMEIN("SharedArrayBuffer.cpp] 474\n");
        Assert(m_waiters == nullptr);
        m_waiters = HeapNew(Waiters, &HeapAllocator::Instance);
    }

    void WaiterList::Cleanup()
    {LOGMEIN("SharedArrayBuffer.cpp] 480\n");
        if (m_waiters != nullptr)
        {LOGMEIN("SharedArrayBuffer.cpp] 482\n");
            HeapDelete(m_waiters);
            m_waiters = nullptr;
        }
    }

    bool WaiterList::Contains(DWORD_PTR agent)
    {LOGMEIN("SharedArrayBuffer.cpp] 489\n");
        Assert(m_waiters != nullptr);
        for (int i = 0; i < m_waiters->Count(); i++)
        {LOGMEIN("SharedArrayBuffer.cpp] 492\n");
            if (m_waiters->Item(i).identity == agent)
            {LOGMEIN("SharedArrayBuffer.cpp] 494\n");
                return true;
            }
        }

        return false;
    }

    bool WaiterList::AddAndSuspendWaiter(DWORD_PTR waiter, uint32 timeout)
    {LOGMEIN("SharedArrayBuffer.cpp] 503\n");
#ifdef _WIN32
        Assert(m_waiters != nullptr);
        Assert(waiter != NULL);
        Assert(!Contains(waiter));

        AgentOfBuffer agent(waiter, CreateEvent(NULL, TRUE, FALSE, NULL));
        m_waiters->Add(agent);

        csForAccess.Leave();
        DWORD result = WaitForSingleObject(agent.event, timeout);
        csForAccess.Enter();
        return result == WAIT_OBJECT_0;
#else
        // TODO for xplat
        return false;
#endif
    }

    void WaiterList::RemoveWaiter(DWORD_PTR waiter)
    {LOGMEIN("SharedArrayBuffer.cpp] 523\n");
#ifdef _WIN32
        Assert(m_waiters != nullptr);
        for (int i = m_waiters->Count() - 1; i >= 0; i--)
        {LOGMEIN("SharedArrayBuffer.cpp] 527\n");
            if (m_waiters->Item(i).identity == waiter)
            {LOGMEIN("SharedArrayBuffer.cpp] 529\n");
                CloseHandle(m_waiters->Item(i).event);
                m_waiters->RemoveAt(i);
                return;
            }
        }

        Assert(false);
#endif
        // TODO for xplat
    }

    uint32 WaiterList::RemoveAndWakeWaiters(int32 count)
    {LOGMEIN("SharedArrayBuffer.cpp] 542\n");
        Assert(m_waiters != nullptr);
        Assert(count >= 0);
        uint32 removed = 0;
#ifdef _WIN32
        while (count > 0 && m_waiters->Count() > 0)
        {LOGMEIN("SharedArrayBuffer.cpp] 548\n");
            AgentOfBuffer agent = m_waiters->Item(0);
            m_waiters->RemoveAt(0);
            count--; removed++;
            SetEvent(agent.event);
            // This agent will be closed when their respective call to wait has returned
        }
#endif
        return removed;
    }

    bool AgentOfBuffer::AgentCanSuspend(ScriptContext *scriptContext)
    {LOGMEIN("SharedArrayBuffer.cpp] 560\n");
        // Currently we are allowing every agent (including main page) to suspend. If we want only web-worker to suspend un-comment below code.
        // return scriptContext != nullptr && scriptContext->webWorkerId != Constants::NonWebWorkerContextId;

        return true;
    }
}
