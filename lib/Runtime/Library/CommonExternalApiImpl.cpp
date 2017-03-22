//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Base/Exception.h"
#include "Base/ThreadContextTlsEntry.h"

void JsUtil::ExternalApi::RecoverUnusedMemory()
{LOGMEIN("CommonExternalApiImpl.cpp] 9\n");
    Js::Exception::RecoverUnusedMemory();
}

bool JsUtil::ExternalApi::RaiseOnIntOverflow()
{LOGMEIN("CommonExternalApiImpl.cpp] 14\n");
    ::Math::DefaultOverflowPolicy();
}

bool JsUtil::ExternalApi::RaiseOutOfMemoryIfScriptActive()
{LOGMEIN("CommonExternalApiImpl.cpp] 19\n");
    return Js::Exception::RaiseIfScriptActive(nullptr, Js::Exception::ExceptionKind_OutOfMemory);
}

bool JsUtil::ExternalApi::RaiseStackOverflowIfScriptActive(Js::ScriptContext * scriptContext, PVOID returnAddress)
{LOGMEIN("CommonExternalApiImpl.cpp] 24\n");
    return Js::Exception::RaiseIfScriptActive(scriptContext, Js::Exception::ExceptionKind_StackOverflow, returnAddress);
}

ThreadContextId JsUtil::ExternalApi::GetCurrentThreadContextId()
{LOGMEIN("CommonExternalApiImpl.cpp] 29\n");
    return ThreadContextTLSEntry::GetCurrentThreadContextId();
}

#if DBG || defined(EXCEPTION_CHECK)
BOOL JsUtil::ExternalApi::IsScriptActiveOnCurrentThreadContext()
{LOGMEIN("CommonExternalApiImpl.cpp] 35\n");
    return ThreadContext::GetContextForCurrentThread() != nullptr &&
        ThreadContext::GetContextForCurrentThread()->IsScriptActive();
}
#endif

