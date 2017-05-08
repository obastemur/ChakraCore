//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

class ThreadContextTLSEntry
{
public:
    static bool InitializeProcess();
    static void CleanupProcess();
    static bool IsProcessInitialized();
    static void InitializeThread();
    static void CleanupThread();
    static void Delete(ThreadContextTLSEntry * entry);
    static bool TrySetThreadContext(ThreadContext * threadContext);
    static void SetThreadContext(ThreadContextTLSEntry * entry, ThreadContext * threadContext);
    static bool ClearThreadContext(bool isValid);
    static bool ClearThreadContext(ThreadContextTLSEntry * entry, bool isThreadContextValid, bool force = true);
    static ThreadContextTLSEntry * GetEntryForCurrentThread();
    static ThreadContextTLSEntry * CreateEntryForCurrentThread();
    static ThreadContextId GetThreadContextId(ThreadContext * threadContext);
#ifdef _WIN32
    static uint32 s_tlsSlot;
#endif
    ThreadContext * GetThreadContext();

private:
    friend JsUtil::ExternalApi;
    static ThreadContextId GetCurrentThreadContextId();

private:
    ThreadContext * threadContext;
    StackProber prober;

};

class ThreadContextScope
{
public:
    ThreadContextScope(ThreadContext * threadContext)
    {TRACE_IT(37822);
        if (!threadContext->IsThreadBound())
        {TRACE_IT(37823);
            originalContext = ThreadContextTLSEntry::GetEntryForCurrentThread() ?
                ThreadContextTLSEntry::GetEntryForCurrentThread()->GetThreadContext() : NULL;
            wasInUse = threadContext == originalContext;
            isValid = ThreadContextTLSEntry::TrySetThreadContext(threadContext);
            doCleanup = !wasInUse && isValid;
        }
        else
        {TRACE_IT(37824);
            Assert(ThreadContext::GetContextForCurrentThread() == threadContext);
            isValid = true;
            wasInUse = true;
            doCleanup = false;
        }
    }

    ~ThreadContextScope()
    {TRACE_IT(37825);
        if (doCleanup)
        {TRACE_IT(37826);
            bool cleared = true;

#if DBG
            cleared =
#endif
                ThreadContextTLSEntry::ClearThreadContext(this->isValid);
            Assert(cleared);

            if (originalContext)
            {TRACE_IT(37827);
                bool canSetback = true;
#if DBG
                canSetback =
#endif
                    ThreadContextTLSEntry::TrySetThreadContext(originalContext);
                Assert(canSetback);
            }
        }
    }

    void Invalidate()
    {TRACE_IT(37828);
        this->isValid = false;
    }

    bool IsValid() const
    {TRACE_IT(37829);
        return this->isValid;
    }

    bool WasInUse() const
    {TRACE_IT(37830);
        return this->wasInUse;
    }

private:
    bool doCleanup;
    bool isValid;
    bool wasInUse;
    ThreadContext* originalContext;
};
