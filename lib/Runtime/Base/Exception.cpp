//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#include "Exception.h"

namespace Js
{
    bool Exception::RaiseIfScriptActive(ScriptContext *scriptContext, unsigned kind, PVOID returnAddress)
    {TRACE_IT(33821);
        ThreadContext *threadContext = ThreadContext::GetContextForCurrentThread();

        if (threadContext != nullptr && threadContext->IsScriptActive())
        {TRACE_IT(33822);
            switch (kind) {
            case ExceptionKind_OutOfMemory:
                AssertMsg(returnAddress == NULL, "should not have returnAddress passed in");
                JavascriptError::ThrowOutOfMemoryError(scriptContext);

            case ExceptionKind_StackOverflow:
                JavascriptError::ThrowStackOverflowError(scriptContext, returnAddress);

            default:
                AssertMsg(false, "Invalid ExceptionKind");
            }
        }

        return false;
    }

    // Recover/Release unused memory and give it back to OS.
    // The function doesn't throw if the attempt to recover memory fails, in which case it simply does nothing.
    // Useful when running out of memory e.g. for Arena but there is some recycler memory which has been committed but is unused.
    void Exception::RecoverUnusedMemory()
    {TRACE_IT(33823);
        ThreadContext* threadContext = ThreadContext::GetContextForCurrentThread();
        if (threadContext)
        {TRACE_IT(33824);
            Recycler* threadRecycler = threadContext->GetRecycler();
            if (threadRecycler)
            {TRACE_IT(33825);
                try
                {TRACE_IT(33826);
                    threadRecycler->CollectNow<CollectOnRecoverFromOutOfMemory>();
                }
                catch (...)
                {TRACE_IT(33827);
                    // Technically, exception is a valid scenario: we asked to recover mem, and it couldn't.
                    // Do not let the exception leak out.
                }
            }
        }
    }
}
