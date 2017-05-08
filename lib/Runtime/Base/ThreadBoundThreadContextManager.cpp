//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"
#include "Base/ThreadContextTlsEntry.h"
#include "Base/ThreadBoundThreadContextManager.h"

ThreadBoundThreadContextManager::EntryList ThreadBoundThreadContextManager::entries(&HeapAllocator::Instance);
#if ENABLE_BACKGROUND_JOB_PROCESSOR
JsUtil::BackgroundJobProcessor * ThreadBoundThreadContextManager::s_sharedJobProcessor = NULL;
#endif
CriticalSection ThreadBoundThreadContextManager::s_sharedJobProcessorCreationLock;

ThreadContext * ThreadBoundThreadContextManager::EnsureContextForCurrentThread()
{TRACE_IT(36910);
    AutoCriticalSection lock(ThreadContext::GetCriticalSection());

    ThreadContextTLSEntry * entry = ThreadContextTLSEntry::GetEntryForCurrentThread();

    if (entry == NULL)
    {TRACE_IT(36911);
        ThreadContextTLSEntry::CreateEntryForCurrentThread();
        entry = ThreadContextTLSEntry::GetEntryForCurrentThread();
        entries.Prepend(entry);
    }

    ThreadContext * threadContext = entry->GetThreadContext();

    // An existing TLS entry may have a null ThreadContext
    // DllCanUnload may have cleaned out all the TLS entry when the module lock count is 0,
    // but the library didn't get unloaded because someone is holding onto ref count via LoadLibrary.
    // Just reinitialize the thread context.
    if (threadContext == nullptr)
    {TRACE_IT(36912);
        threadContext = HeapNew(ThreadContext);
        threadContext->SetIsThreadBound();
        if (!ThreadContextTLSEntry::TrySetThreadContext(threadContext))
        {TRACE_IT(36913);
            HeapDelete(threadContext);
            return NULL;
        }
    }

    Assert(threadContext != NULL);

    return threadContext;
}

void ThreadBoundThreadContextManager::DestroyContextAndEntryForCurrentThread()
{TRACE_IT(36914);
    AutoCriticalSection lock(ThreadContext::GetCriticalSection());

    ThreadContextTLSEntry * entry = ThreadContextTLSEntry::GetEntryForCurrentThread();

    if (entry == NULL)
    {TRACE_IT(36915);
        return;
    }

    ThreadContext * threadContext = static_cast<ThreadContext *>(entry->GetThreadContext());
    entries.Remove(entry);

    if (threadContext != NULL && threadContext->IsThreadBound())
    {TRACE_IT(36916);
        ShutdownThreadContext(threadContext);
    }

    ThreadContextTLSEntry::CleanupThread();
}

void ThreadBoundThreadContextManager::DestroyAllContexts()
{TRACE_IT(36917);
#if ENABLE_BACKGROUND_JOB_PROCESSOR
    JsUtil::BackgroundJobProcessor * jobProcessor = NULL;
#endif

    {
        AutoCriticalSection lock(ThreadContext::GetCriticalSection());

        ThreadContextTLSEntry * currentEntry = ThreadContextTLSEntry::GetEntryForCurrentThread();

        if (currentEntry == NULL)
        {TRACE_IT(36918);
            // We need a current thread entry so that we can use it to release any thread contexts
            // we find below.
            try
            {TRACE_IT(36919);
                AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);
                currentEntry = ThreadContextTLSEntry::CreateEntryForCurrentThread();
                entries.Prepend(currentEntry);
            }
            catch (Js::OutOfMemoryException)
            {TRACE_IT(36920);
                return;
            }
        }
        else
        {TRACE_IT(36921);
            // We need to clear out the current thread entry so that we can use it to release any
            // thread contexts we find below.
            ThreadContext * threadContext = static_cast<ThreadContext *>(currentEntry->GetThreadContext());

            if (threadContext != NULL)
            {TRACE_IT(36922);
                if (threadContext->IsThreadBound())
                {TRACE_IT(36923);
                    ShutdownThreadContext(threadContext);
                    ThreadContextTLSEntry::ClearThreadContext(currentEntry, false);
                }
                else
                {TRACE_IT(36924);
                    ThreadContextTLSEntry::ClearThreadContext(currentEntry, true);
                }
            }
        }

        EntryList::Iterator iter(&entries);

        while (iter.Next())
        {TRACE_IT(36925);
            ThreadContextTLSEntry * entry = iter.Data();
            ThreadContext * threadContext =  static_cast<ThreadContext *>(entry->GetThreadContext());

            if (threadContext != nullptr)
            {TRACE_IT(36926);
                // Found a thread context. Remove it from the containing entry.
                ThreadContextTLSEntry::ClearThreadContext(entry, true);
                // Now set it to our thread's entry.
                ThreadContextTLSEntry::SetThreadContext(currentEntry, threadContext);
                // Clear it out.
                ShutdownThreadContext(threadContext);
                // Now clear it out of our entry.
                ThreadContextTLSEntry::ClearThreadContext(currentEntry, false);
            }
        }

        // We can only clean up our own TLS entry, so we're going to go ahead and do that here.
        entries.Remove(currentEntry);
        ThreadContextTLSEntry::CleanupThread();

#if ENABLE_BACKGROUND_JOB_PROCESSOR
        if (s_sharedJobProcessor != NULL)
        {TRACE_IT(36927);
            jobProcessor = s_sharedJobProcessor;
            s_sharedJobProcessor = NULL;

            jobProcessor->Close();
        }
#endif
    }

#if ENABLE_BACKGROUND_JOB_PROCESSOR
    if (jobProcessor != NULL)
    {TRACE_IT(36928);
        HeapDelete(jobProcessor);
    }
#endif
}

void ThreadBoundThreadContextManager::DestroyAllContextsAndEntries()
{TRACE_IT(36929);
    AutoCriticalSection lock(ThreadContext::GetCriticalSection());

    while (!entries.Empty())
    {TRACE_IT(36930);
        ThreadContextTLSEntry * entry = entries.Head();
        ThreadContext * threadContext =  static_cast<ThreadContext *>(entry->GetThreadContext());

        entries.RemoveHead();

        if (threadContext != nullptr)
        {TRACE_IT(36931);
#if DBG
            PageAllocator* pageAllocator = threadContext->GetPageAllocator();
            if (pageAllocator)
            {TRACE_IT(36932);
                pageAllocator->SetConcurrentThreadId(::GetCurrentThreadId());
            }
#endif

            threadContext->ShutdownThreads();

            HeapDelete(threadContext);
        }

        ThreadContextTLSEntry::Delete(entry);
    }

#if ENABLE_BACKGROUND_JOB_PROCESSOR
    if (s_sharedJobProcessor != NULL)
    {TRACE_IT(36933);
        s_sharedJobProcessor->Close();

        HeapDelete(s_sharedJobProcessor);
        s_sharedJobProcessor = NULL;
    }
#endif
}

JsUtil::JobProcessor * ThreadBoundThreadContextManager::GetSharedJobProcessor()
{TRACE_IT(36934);
#if ENABLE_BACKGROUND_JOB_PROCESSOR
    if (s_sharedJobProcessor == NULL)
    {TRACE_IT(36935);
        // Don't use ThreadContext::GetCriticalSection() because it's also locked during thread detach while the loader lock is
        // held, and that may prevent the background job processor's thread from being started due to contention on the loader
        // lock, leading to a deadlock
        AutoCriticalSection lock(&s_sharedJobProcessorCreationLock);

        if (s_sharedJobProcessor == NULL)
        {TRACE_IT(36936);
            // We don't need to have allocation policy manager for web worker.
            s_sharedJobProcessor = HeapNew(JsUtil::BackgroundJobProcessor, NULL, NULL, false /*disableParallelThreads*/);
        }
    }

    return s_sharedJobProcessor;
#else
    return nullptr;
#endif
}

void RentalThreadContextManager::DestroyThreadContext(ThreadContext* threadContext)
{TRACE_IT(36937);
    bool deleteThreadContext = true;

#ifdef CHAKRA_STATIC_LIBRARY
    // xplat-todo: Cleanup staticlib shutdown. Deleting contexts / finalizers having
    // trouble with current runtime/context.
    deleteThreadContext = false;
#endif

    ShutdownThreadContext(threadContext, deleteThreadContext);
}

void ThreadContextManagerBase::ShutdownThreadContext(
    ThreadContext* threadContext, bool deleteThreadContext /*= true*/)
{TRACE_IT(36938);

#if DBG
    PageAllocator* pageAllocator = threadContext->GetPageAllocator();
    if (pageAllocator)
    {TRACE_IT(36939);
        pageAllocator->SetConcurrentThreadId(::GetCurrentThreadId());
    }
#endif
    threadContext->ShutdownThreads();

    if (deleteThreadContext)
    {TRACE_IT(36940);
        HeapDelete(threadContext);
    }
}
