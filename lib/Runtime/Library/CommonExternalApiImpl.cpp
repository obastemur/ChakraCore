//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Base/Exception.h"
#include "Base/ThreadContextTlsEntry.h"

void JsUtil::ExternalApi::RecoverUnusedMemory()
{TRACE_IT(54523);
    Js::Exception::RecoverUnusedMemory();
}

bool JsUtil::ExternalApi::RaiseOnIntOverflow()
{TRACE_IT(54524);
    ::Math::DefaultOverflowPolicy();
}

bool JsUtil::ExternalApi::RaiseOutOfMemoryIfScriptActive()
{TRACE_IT(54525);
    return Js::Exception::RaiseIfScriptActive(nullptr, Js::Exception::ExceptionKind_OutOfMemory);
}

bool JsUtil::ExternalApi::RaiseStackOverflowIfScriptActive(Js::ScriptContext * scriptContext, PVOID returnAddress)
{TRACE_IT(54526);
    return Js::Exception::RaiseIfScriptActive(scriptContext, Js::Exception::ExceptionKind_StackOverflow, returnAddress);
}

ThreadContextId JsUtil::ExternalApi::GetCurrentThreadContextId()
{TRACE_IT(54527);
    return ThreadContextTLSEntry::GetCurrentThreadContextId();
}

#if DBG || defined(EXCEPTION_CHECK)
BOOL JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext()
{TRACE_IT(54528);
    return ThreadContext::GetContextForCurrentThread() != nullptr &&
        ThreadContext::GetContextForCurrentThread()->IsScriptActive();
}
#endif

