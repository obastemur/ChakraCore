//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include <JsrtPch.h>
#include "JsrtRuntime.h"
#include "jsrtHelper.h"
#include "Base/ThreadContextTlsEntry.h"
#include "Base/ThreadBoundThreadContextManager.h"
JsrtRuntime::JsrtRuntime(ThreadContext * threadContext, bool useIdle, bool dispatchExceptions)
{TRACE_IT(28469);
    Assert(threadContext != NULL);
    this->threadContext = threadContext;
    this->contextList = NULL;
    this->collectCallback = NULL;
    this->beforeCollectCallback = NULL;
    this->callbackContext = NULL;
    this->allocationPolicyManager = threadContext->GetAllocationPolicyManager();
    this->useIdle = useIdle;
    this->dispatchExceptions = dispatchExceptions;
    if (useIdle)
    {TRACE_IT(28470);
        this->threadService.Initialize(threadContext);
    }
    threadContext->SetJSRTRuntime(this);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    serializeByteCodeForLibrary = false;
#endif
    this->jsrtDebugManager = nullptr;
}

JsrtRuntime::~JsrtRuntime()
{TRACE_IT(28471);
    HeapDelete(allocationPolicyManager);
    if (this->jsrtDebugManager != nullptr)
    {TRACE_IT(28472);
        HeapDelete(this->jsrtDebugManager);
        this->jsrtDebugManager = nullptr;
    }
}

// This is called at process detach.
// threadcontext created from runtime should not be destroyed in ThreadBoundThreadContext
// we should clean them up at process detach only as runtime can be used in other threads
// even after the current physical thread was destroyed.
// This is called after ThreadBoundThreadContext are cleaned up, so the remaining items
// in the globalthreadContext linklist should be for jsrt only.
void JsrtRuntime::Uninitialize()
{TRACE_IT(28473);
    ThreadContext* currentThreadContext = ThreadContext::GetThreadContextList();
    ThreadContext* tmpThreadContext;
    while (currentThreadContext)
    {TRACE_IT(28474);
        Assert(!currentThreadContext->IsScriptActive());
        JsrtRuntime* currentRuntime = static_cast<JsrtRuntime*>(currentThreadContext->GetJSRTRuntime());
        tmpThreadContext = currentThreadContext;
        currentThreadContext = currentThreadContext->Next();

#ifdef CHAKRA_STATIC_LIBRARY
        // xplat-todo: Cleanup staticlib shutdown. This only shuts down threads.
        // Other closing contexts / finalizers having trouble with current
        // runtime/context.
        RentalThreadContextManager::DestroyThreadContext(tmpThreadContext);
#else
        currentRuntime->CloseContexts();
        RentalThreadContextManager::DestroyThreadContext(tmpThreadContext);
        HeapDelete(currentRuntime);
#endif
    }
}

void JsrtRuntime::CloseContexts()
{TRACE_IT(28475);
    while (this->contextList != NULL)
    {TRACE_IT(28476);
        this->contextList->Dispose(false);
        // This will remove it from the list
    }
}

void JsrtRuntime::SetBeforeCollectCallback(JsBeforeCollectCallback beforeCollectCallback, void * callbackContext)
{TRACE_IT(28477);
    if (beforeCollectCallback != NULL)
    {TRACE_IT(28478);
        if (this->collectCallback == NULL)
        {TRACE_IT(28479);
            this->collectCallback = this->threadContext->AddRecyclerCollectCallBack(RecyclerCollectCallbackStatic, this);
        }

        this->beforeCollectCallback = beforeCollectCallback;
        this->callbackContext = callbackContext;
    }
    else
    {TRACE_IT(28480);
        if (this->collectCallback != NULL)
        {TRACE_IT(28481);
            this->threadContext->RemoveRecyclerCollectCallBack(this->collectCallback);
            this->collectCallback = NULL;
        }

        this->beforeCollectCallback = NULL;
        this->callbackContext = NULL;
    }
}

void JsrtRuntime::RecyclerCollectCallbackStatic(void * context, RecyclerCollectCallBackFlags flags)
{TRACE_IT(28482);
    if (flags & Collect_Begin)
    {TRACE_IT(28483);
        JsrtRuntime * _this = reinterpret_cast<JsrtRuntime *>(context);
        try
        {TRACE_IT(28484);
            JsrtCallbackState scope(reinterpret_cast<ThreadContext*>(_this->GetThreadContext()));
            _this->beforeCollectCallback(_this->callbackContext);
        }
        catch (...)
        {
            AssertMsg(false, "Unexpected non-engine exception.");
        }
    }
}

unsigned int JsrtRuntime::Idle()
{TRACE_IT(28485);
    return this->threadService.Idle();
}

void JsrtRuntime::EnsureJsrtDebugManager()
{TRACE_IT(28486);
    if (this->jsrtDebugManager == nullptr)
    {TRACE_IT(28487);
        this->jsrtDebugManager = HeapNew(JsrtDebugManager, this->threadContext);
    }
    Assert(this->jsrtDebugManager != nullptr);
}

void JsrtRuntime::DeleteJsrtDebugManager()
{TRACE_IT(28488);
    if (this->jsrtDebugManager != nullptr)
    {TRACE_IT(28489);
        HeapDelete(this->jsrtDebugManager);
        this->jsrtDebugManager = nullptr;
    }
}

JsrtDebugManager * JsrtRuntime::GetJsrtDebugManager()
{TRACE_IT(28490);
    return this->jsrtDebugManager;
}
