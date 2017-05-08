//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "Base/ThreadContextTlsEntry.h"

#ifdef _WIN32
uint32 ThreadContextTLSEntry::s_tlsSlot = TLS_OUT_OF_INDEXES;
#define ENTRY_FOR_CURRENT_THREAD() GetEntryForCurrentThread()
#define ASSERT_ENTRY_INITIALIZED() Assert(s_tlsSlot != TLS_OUT_OF_INDEXES)
#else // !_WIN32
// entry doesn't need a special initialization
#define ASSERT_ENTRY_INITIALIZED() Assert(true)
static THREAD_LOCAL ThreadContextTLSEntry* s_tlsSlot = nullptr;
#define TlsSetValue(a,b) a = b
// clang may not optimize the access to THREAD_LOCAL from a sub function
#define ENTRY_FOR_CURRENT_THREAD() s_tlsSlot
#endif

bool ThreadContextTLSEntry::InitializeProcess()
{TRACE_IT(37798);
#ifdef _WIN32
    Assert(s_tlsSlot == TLS_OUT_OF_INDEXES);
    s_tlsSlot = TlsAlloc();
    return (s_tlsSlot != TLS_OUT_OF_INDEXES);
#else
    return true;
#endif
}

void ThreadContextTLSEntry::CleanupProcess()
{TRACE_IT(37799);
#ifdef _WIN32
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);
    TlsFree(s_tlsSlot);
    s_tlsSlot = TLS_OUT_OF_INDEXES;
#endif
}

bool ThreadContextTLSEntry::IsProcessInitialized()
{TRACE_IT(37800);
#ifdef _WIN32
    return s_tlsSlot != TLS_OUT_OF_INDEXES;
#else
    return true;
#endif
}

void ThreadContextTLSEntry::InitializeThread()
{TRACE_IT(37801);
#ifdef _WIN32
    Assert(s_tlsSlot != TLS_OUT_OF_INDEXES);
    Assert(!TlsGetValue(s_tlsSlot));
    TlsSetValue(s_tlsSlot, NULL);
#endif
}

void ThreadContextTLSEntry::CleanupThread()
{TRACE_IT(37802);
    ASSERT_ENTRY_INITIALIZED();
    ThreadContextTLSEntry* entry = ENTRY_FOR_CURRENT_THREAD();
    if (entry != NULL)
    {TRACE_IT(37803);
        HeapDelete(entry);
        TlsSetValue(s_tlsSlot, NULL);
    }
}

bool ThreadContextTLSEntry::TrySetThreadContext(ThreadContext * threadContext)
{TRACE_IT(37804);
    Assert(threadContext != NULL);
    ASSERT_ENTRY_INITIALIZED();

    DWORD threadContextThreadId = threadContext->GetCurrentThreadId();

    // If a thread context is current on another thread, then you cannot set it to current on this one.
    if (threadContextThreadId != ThreadContext::NoThread && threadContextThreadId != ::GetCurrentThreadId())
    {TRACE_IT(37805);
        // the thread doesn't support rental thread and try to set on a different thread???
        Assert(!threadContext->IsThreadBound());
        return false;
    }

    ThreadContextTLSEntry * entry = ENTRY_FOR_CURRENT_THREAD();

    if (entry == NULL)
    {TRACE_IT(37806);
        Assert(!threadContext->IsThreadBound());
        entry = CreateEntryForCurrentThread();
#ifndef _WIN32
        ENTRY_FOR_CURRENT_THREAD() = entry;
#endif
    }
    else if (entry->threadContext != NULL && entry->threadContext != threadContext)
    {TRACE_IT(37807);
        // If the thread has an active thread context and either that thread context is thread
        // bound (in which case it cannot be moved off this thread), or if the thread context
        // is running script, you cannot move it off this thread.
        if (entry->threadContext->IsThreadBound() || entry->threadContext->IsInScript())
        {TRACE_IT(37808);
            return false;
        }

        ClearThreadContext(entry, true);
    }

    SetThreadContext(entry, threadContext);

    return true;
}

void ThreadContextTLSEntry::SetThreadContext(ThreadContextTLSEntry * entry, ThreadContext * threadContext)
{TRACE_IT(37809);
    entry->threadContext = threadContext;
    threadContext->SetStackProber(&entry->prober);
    threadContext->SetCurrentThreadId(::GetCurrentThreadId());
}

bool ThreadContextTLSEntry::ClearThreadContext(bool isValid)
{TRACE_IT(37810);
    return ClearThreadContext(ENTRY_FOR_CURRENT_THREAD(), isValid, false);
}

bool ThreadContextTLSEntry::ClearThreadContext(ThreadContextTLSEntry * entry, bool isThreadContextValid, bool force)
{TRACE_IT(37811);
    ASSERT_ENTRY_INITIALIZED();

    if (entry != NULL)
    {TRACE_IT(37812);
        if (entry->threadContext != NULL && isThreadContextValid)
        {TRACE_IT(37813);
            // If the thread has an active thread context and either that thread context is thread
            // bound (in which case it cannot be moved off this thread), or if the thread context
            // is running script, you cannot move it off this thread.
            if (!force && (entry->threadContext->IsThreadBound() || entry->threadContext->IsInScript()))
            {TRACE_IT(37814);
                return false;
            }
            entry->threadContext->SetCurrentThreadId(ThreadContext::NoThread);
            entry->threadContext->SetStackProber(NULL);
        }

        entry->threadContext = NULL;
    }

    return true;
}

void ThreadContextTLSEntry::Delete(ThreadContextTLSEntry * entry)
{TRACE_IT(37815);
    HeapDelete(entry);
}

ThreadContextTLSEntry * ThreadContextTLSEntry::GetEntryForCurrentThread()
{TRACE_IT(37816);
    ASSERT_ENTRY_INITIALIZED();
#ifdef _WIN32
    return reinterpret_cast<ThreadContextTLSEntry *>(TlsGetValue(s_tlsSlot));
#else
    return ENTRY_FOR_CURRENT_THREAD();
#endif
}

ThreadContextTLSEntry * ThreadContextTLSEntry::CreateEntryForCurrentThread()
{TRACE_IT(37817);
    ASSERT_ENTRY_INITIALIZED();
#ifdef _WIN32
    Assert(TlsGetValue(s_tlsSlot) == NULL);
#endif

    ThreadContextTLSEntry * entry = HeapNewStructZ(ThreadContextTLSEntry);
#pragma prefast(suppress:6001, "Memory from HeapNewStructZ are zero initialized")
    entry->prober.Initialize();
    TlsSetValue(s_tlsSlot, entry);

    return entry;
}

ThreadContext * ThreadContextTLSEntry::GetThreadContext()
{TRACE_IT(37818);
    return this->threadContext;
}

ThreadContextId ThreadContextTLSEntry::GetCurrentThreadContextId()
{TRACE_IT(37819);
    ThreadContextTLSEntry * entry = ENTRY_FOR_CURRENT_THREAD();
    if (entry != NULL && entry->GetThreadContext() != NULL)
    {TRACE_IT(37820);
        return (ThreadContextId)entry->GetThreadContext();
    }

    return NoThreadContextId;
}

ThreadContextId ThreadContextTLSEntry::GetThreadContextId(ThreadContext * threadContext)
{TRACE_IT(37821);
    return threadContext;
}
