//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

class ServerContextManager
{
public:
    static void RegisterThreadContext(ServerThreadContext* threadContext);
    static void UnRegisterThreadContext(ServerThreadContext* threadContext);

    static void RegisterScriptContext(ServerScriptContext* scriptContext);
    static void UnRegisterScriptContext(ServerScriptContext* scriptContext);
    static bool CheckLivenessAndAddref(ServerScriptContext* context);
    static bool CheckLivenessAndAddref(ServerThreadContext* context);


private:
    static JsUtil::BaseHashSet<ServerThreadContext*, HeapAllocator> threadContexts;
    static JsUtil::BaseHashSet<ServerScriptContext*, HeapAllocator> scriptContexts;
    static CriticalSection cs;

#ifdef STACK_BACK_TRACE
public:
    template<class T>
    struct ClosedContextEntry
    {
        __declspec(noinline)
        ClosedContextEntry(T* context)
            :context(context)
        {TRACE_IT(27459);
            stack = StackBackTrace::Capture(&NoThrowHeapAllocator::Instance, 2);
        }
        ~ClosedContextEntry()
        {TRACE_IT(27460);
            if (stack)
            {TRACE_IT(27461);
                stack->Delete(&NoThrowHeapAllocator::Instance);
            }
        }
        T* context;
        union {
            DWORD runtimeProcId;
            ServerThreadContext* threadCtx;
        };
        StackBackTrace* stack;
    };

    static void RecordCloseContext(ServerThreadContext* context)
    {TRACE_IT(27462);
        auto record = HeapNewNoThrow(ClosedContextEntry<ServerThreadContext>, context);
        if (record)
        {TRACE_IT(27463);
            record->runtimeProcId = context->GetRuntimePid();
        }
        ClosedThreadContextList.PrependNoThrow(&NoThrowHeapAllocator::Instance, record);
    }
    static void RecordCloseContext(ServerScriptContext* context)
    {TRACE_IT(27464);
        auto record = HeapNewNoThrow(ClosedContextEntry<ServerScriptContext>, context);
        if (record)
        {TRACE_IT(27465);
            record->threadCtx = context->GetThreadContext();
        }
        ClosedScriptContextList.PrependNoThrow(&NoThrowHeapAllocator::Instance, record);
    }

    static SList<ClosedContextEntry<ServerThreadContext>*, NoThrowHeapAllocator> ClosedThreadContextList;
    static SList<ClosedContextEntry<ServerScriptContext>*, NoThrowHeapAllocator> ClosedScriptContextList;
#endif

    static void Shutdown()
    {TRACE_IT(27466);
#ifdef STACK_BACK_TRACE
        while (!ClosedThreadContextList.Empty())
        {TRACE_IT(27467);
            auto record = ClosedThreadContextList.Pop();
            if (record)
            {TRACE_IT(27468);
                HeapDelete(record);
            }
        }
        while (!ClosedScriptContextList.Empty())
        {TRACE_IT(27469);
            auto record = ClosedScriptContextList.Pop();
            if (record)
            {TRACE_IT(27470);
                HeapDelete(record);
            }
        }
#endif
    }
};

struct ContextClosedException {};

struct AutoReleaseThreadContext
{
    AutoReleaseThreadContext(ServerThreadContext* threadContext)
        :threadContext(threadContext)
    {TRACE_IT(27471);
        if (!ServerContextManager::CheckLivenessAndAddref(threadContext))
        {TRACE_IT(27472);
            // Don't assert here because ThreadContext can be closed before scriptContext closing call
            // and ThreadContext closing causes all related scriptContext be closed
            threadContext = nullptr;
            throw ContextClosedException();
        }
    }

    ~AutoReleaseThreadContext()
    {TRACE_IT(27473);
        if (threadContext)
        {TRACE_IT(27474);
            threadContext->Release();
        }
    }

    ServerThreadContext* threadContext;
};

struct AutoReleaseScriptContext
{
    AutoReleaseScriptContext(ServerScriptContext* scriptContext)
        :scriptContext(scriptContext)
    {TRACE_IT(27475);
        if (!ServerContextManager::CheckLivenessAndAddref(scriptContext))
        {TRACE_IT(27476);
            // Don't assert here because ThreadContext can be closed before scriptContext closing call
            // and ThreadContext closing causes all related scriptContext be closed
            scriptContext = nullptr;
            threadContext = nullptr;
            throw ContextClosedException();
        }
        threadContext = scriptContext->GetThreadContext();
    }

    ~AutoReleaseScriptContext()
    {TRACE_IT(27477);
        if (scriptContext)
        {TRACE_IT(27478);
            scriptContext->Release();
        }
        if (threadContext)
        {TRACE_IT(27479);
            threadContext->Release();
        }
    }

    ServerScriptContext* scriptContext;
    ServerThreadContext* threadContext;
};


template<typename Fn>
HRESULT ServerCallWrapper(ServerThreadContext* threadContextInfo, Fn fn);
template<typename Fn>
HRESULT ServerCallWrapper(ServerScriptContext* scriptContextInfo, Fn fn);
