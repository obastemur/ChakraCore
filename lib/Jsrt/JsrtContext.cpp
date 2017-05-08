//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "JsrtPch.h"
#include "JsrtRuntime.h"
#include "Base/ThreadContextTlsEntry.h"

static THREAD_LOCAL JsrtContext* s_tlvSlot = nullptr;

JsrtContext::JsrtContext(JsrtRuntime * runtime) :
    runtime(runtime), javascriptLibrary(nullptr)
{TRACE_IT(28010);
}

void JsrtContext::SetJavascriptLibrary(Js::JavascriptLibrary * library)
{TRACE_IT(28011);
    this->javascriptLibrary = library;
    if (this->javascriptLibrary)
    {TRACE_IT(28012);
        this->javascriptLibrary->SetJsrtContext(this);
    }
}

void JsrtContext::Link()
{TRACE_IT(28013);
    // Link this new JsrtContext up in the JsrtRuntime's context list
    this->next = runtime->contextList;
    this->previous = nullptr;

    if (runtime->contextList != nullptr)
    {TRACE_IT(28014);
        Assert(runtime->contextList->previous == nullptr);
        runtime->contextList->previous = this;
    }

    runtime->contextList = this;
}

void JsrtContext::Unlink()
{TRACE_IT(28015);
    // Unlink from JsrtRuntime JsrtContext list
    if (this->previous == nullptr)
    {TRACE_IT(28016);
        // Have to check this because if we failed while creating, it might
        // never have gotten linked in to the runtime at all.
        if (this->runtime->contextList == this)
        {TRACE_IT(28017);
            this->runtime->contextList = this->next;
        }
    }
    else
    {TRACE_IT(28018);
        Assert(this->previous->next == this);
        this->previous->next = this->next;
    }

    if (this->next != nullptr)
    {TRACE_IT(28019);
        Assert(this->next->previous == this);
        this->next->previous = this->previous;
    }
}

/* static */
JsrtContext * JsrtContext::GetCurrent()
{TRACE_IT(28020);
    return s_tlvSlot;
}

/* static */
bool JsrtContext::TrySetCurrent(JsrtContext * context)
{TRACE_IT(28021);
    ThreadContext * threadContext;

    //We are not pinning the context after SetCurrentContext, so if the context is not pinned
    //it might be reclaimed half way during execution. In jsrtshell the runtime was optimized out
    //at time of JsrtContext::Run by the compiler.
    //The change is to pin the context at setconcurrentcontext, and unpin the previous one. In
    //JsDisposeRuntime we'll reject if current context is active, so that will make sure all
    //contexts are unpinned at time of JsDisposeRuntime.
    if (context != nullptr)
    {TRACE_IT(28022);
        threadContext = context->GetScriptContext()->GetThreadContext();

        if (!ThreadContextTLSEntry::TrySetThreadContext(threadContext))
        {TRACE_IT(28023);
            return false;
        }
        threadContext->GetRecycler()->RootAddRef((LPVOID)context);
    }
    else
    {TRACE_IT(28024);
        if (!ThreadContextTLSEntry::ClearThreadContext(true))
        {TRACE_IT(28025);
            return false;
        }
    }

    JsrtContext* originalContext = s_tlvSlot;
    if (originalContext != nullptr)
    {TRACE_IT(28026);
        originalContext->GetScriptContext()->GetRecycler()->RootRelease((LPVOID) originalContext);
    }

    s_tlvSlot = context;
    return true;
}

void JsrtContext::Mark(Recycler * recycler)
{
    AssertMsg(false, "Mark called on object that isn't TrackableObject");
}
