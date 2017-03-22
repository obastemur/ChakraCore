//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if !defined(_M_X64)
#error Amd64StackFrame is not supported on this architecture.
#endif

Js::Amd64StackFrame::Amd64StackFrame()
    : scriptContext(nullptr),
      imageBase(0),
      functionEntry(nullptr),
      currentContext(nullptr),
      hasCallerContext(false),
      callerContext(nullptr),
      addressOfCodeAddr(nullptr)
{LOGMEIN("StackFrame.cpp] 18\n");
}

Js::Amd64StackFrame::~Amd64StackFrame()
{LOGMEIN("StackFrame.cpp] 22\n");
    if (currentContext)
    {LOGMEIN("StackFrame.cpp] 24\n");
        scriptContext->GetThreadContext()->GetAmd64ContextsManager()->Release(currentContext);
    }
}

// InitializeByReturnAddress.
// Parameters:
//  unwindToAddress: specifies the address we need to unwind the stack before any walks can be done.
//                   RtlVirtualUnwind API requires that all unwinds are done within same call stack --
//                   this means that we can't capture context here, go 2 frames back (this ctor -> JavascriptStackWalker ctor),
//                   then create new frames (e.g. for Next()) and unwind stack in them.

bool Js::Amd64StackFrame::InitializeByReturnAddress(void *returnAddress, ScriptContext* scriptContext)
{LOGMEIN("StackFrame.cpp] 37\n");
    CONTEXT* pair = scriptContext->GetThreadContext()->GetAmd64ContextsManager()->Allocate();
    this->scriptContext = scriptContext;
    this->currentContext = pair;
    this->callerContext = pair + 1;

    this->stackCheckCodeHeight =
        scriptContext->GetThreadContext()->DoInterruptProbe() ? stackCheckCodeHeightWithInterruptProbe
        : scriptContext->GetThreadContext()->IsThreadBound() ? stackCheckCodeHeightThreadBound
        : stackCheckCodeHeightNotThreadBound;

    // this is the context for the current function
    RtlCaptureContext(currentContext);
    OnCurrentContextUpdated();

    // Unwind stack to the frame where RIP is the returnAddress
    bool found = SkipToFrame(returnAddress);

    if (!found)
    {
        AssertMsg(FALSE, "Amd64StackFrame: can't initialize: can't unwind the stack to specified unwindToAddress.");
        RtlCaptureContext(currentContext); // Restore trashed context, the best we can do.
    }

    return found;
}

bool Js::Amd64StackFrame::Next()
{LOGMEIN("StackFrame.cpp] 65\n");
    if (hasCallerContext)
    {LOGMEIN("StackFrame.cpp] 67\n");
        *currentContext = *callerContext;
        OnCurrentContextUpdated();
        return true;
    }

    if (JavascriptFunction::IsNativeAddress(this->scriptContext, (void*)this->currentContext->Rip))
    {LOGMEIN("StackFrame.cpp] 74\n");
        this->addressOfCodeAddr = this->GetAddressOfReturnAddress(true /*isCurrentContextNative*/, false /*shouldCheckForNativeAddr*/);
        if (NextFromNativeAddress(this->currentContext))
        {LOGMEIN("StackFrame.cpp] 77\n");
            OnCurrentContextUpdated();
            return true;
        }
        return false;
    }

    EnsureFunctionEntry();
    this->addressOfCodeAddr = this->GetAddressOfReturnAddress();
    if (Next(currentContext, imageBase, functionEntry))
    {LOGMEIN("StackFrame.cpp] 87\n");
        OnCurrentContextUpdated();
        return true;
    }

    return false;
}

VOID *Js::Amd64StackFrame::GetInstructionPointer()
{LOGMEIN("StackFrame.cpp] 96\n");
    return (VOID *)currentContext->Rip;
}

void *Js::Amd64StackFrame::GetFrame() const
{LOGMEIN("StackFrame.cpp] 101\n");
    return (void *)currentContext->Rbp;
}

VOID **Js::Amd64StackFrame::GetArgv(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{LOGMEIN("StackFrame.cpp] 106\n");
    if (EnsureCallerContext(isCurrentContextNative || (shouldCheckForNativeAddr && JavascriptFunction::IsNativeAddress(this->scriptContext, (void*)this->currentContext->Rip))))
    {LOGMEIN("StackFrame.cpp] 108\n");
        return (VOID **)callerContext->Rsp;
    }

    return nullptr;
}

VOID *Js::Amd64StackFrame::GetReturnAddress(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{LOGMEIN("StackFrame.cpp] 116\n");
    if (EnsureCallerContext(isCurrentContextNative || (shouldCheckForNativeAddr && JavascriptFunction::IsNativeAddress(this->scriptContext, (void*)this->currentContext->Rip))))
    {LOGMEIN("StackFrame.cpp] 118\n");
        return (VOID *)callerContext->Rip;
    }

    return nullptr;
}

void *Js::Amd64StackFrame::GetAddressOfReturnAddress(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{LOGMEIN("StackFrame.cpp] 126\n");
    if (EnsureCallerContext(isCurrentContextNative || (shouldCheckForNativeAddr && JavascriptFunction::IsNativeAddress(this->scriptContext, (void*)this->currentContext->Rip))))
    {LOGMEIN("StackFrame.cpp] 128\n");
        return (void*)((VOID **)callerContext->Rsp - 1);
    }

    return nullptr;
}

bool Js::Amd64StackFrame::Next(CONTEXT *context, ULONG64 imageBase, RUNTIME_FUNCTION *functionEntry)
{LOGMEIN("StackFrame.cpp] 136\n");
    Assert(context);

    VOID *handlerData = nullptr;
    ULONG64 establisherFrame = 0;

    if (!context->Rip)
        return false;

    if (functionEntry)
    {
        RtlVirtualUnwind(0 /* UNW_FLAG_NHANDLER */,
                         imageBase,
                         context->Rip,
                         functionEntry,
                         context,
                         &handlerData,
                         &establisherFrame,
                         nullptr);
    }
    else
    {
        // Leaf frames are not listed in the PDATA section because they
        // don't use the stack.
        // Manually crawl to the next frame.
        context->Rip = *((DWORD64 *)context->Rsp);
        context->Rsp += 8;
    }

    return true;
}

bool
Js::Amd64StackFrame::NextFromNativeAddress(CONTEXT * context)
{LOGMEIN("StackFrame.cpp] 170\n");
    if (!context->Rip)
    {LOGMEIN("StackFrame.cpp] 172\n");
        return false;
    }

    //Restore Rip, Rsp and Rbp
    // Rip - to check if the context is in native address range
    //     - to check if the current frame is javascript frame
    //     - to do virtual unwind
    //     - to pass to RtlLookupFunctionEntry if the next frame is not native
    //
    // Rsp - To easily get to the arguments passed in
    //
    // Rbp - to walk to the next frame

    context->Rip = *((DWORD64*)context->Rbp + 1);
    context->Rsp = (DWORD64)((DWORD64*)context->Rbp + 2);
    context->Rbp = *((DWORD64*)context->Rbp);

    return true;
}

bool
Js::Amd64StackFrame::SkipToFrame(void * returnAddress)
{LOGMEIN("StackFrame.cpp] 195\n");
    bool found = false;
    while (Next())
    {LOGMEIN("StackFrame.cpp] 198\n");
        if (((PVOID)currentContext->Rip) == returnAddress)
        {LOGMEIN("StackFrame.cpp] 200\n");
            found = true;
            break;
        }
        else if (!ThreadContext::IsOnStack((PVOID)currentContext->Rsp))
        {
            AssertMsg(FALSE, "Amd64StackFrame: while doing initial unwind SP got out of stack.");
            break;
        }
    }
    return found;
}

bool
Js::Amd64StackFrame::IsInStackCheckCode(void *entry, void *codeAddr, size_t stackCheckCodeHeight)
{LOGMEIN("StackFrame.cpp] 215\n");
    return ((size_t(codeAddr) - size_t(entry)) <= stackCheckCodeHeight);
}

Js::Amd64ContextsManager::Amd64ContextsManager()
    : curIndex(GENERAL_CONTEXT)
{LOGMEIN("StackFrame.cpp] 221\n");
}

_Ret_writes_(CONTEXT_PAIR_COUNT)
CONTEXT* Js::Amd64ContextsManager::InternalGet(
    _In_range_(GENERAL_CONTEXT, OOM_CONTEXT) ContextsIndex index)
{LOGMEIN("StackFrame.cpp] 227\n");
    Assert(index < NUM_CONTEXTS);
    return &contexts[CONTEXT_PAIR_COUNT * index];
}

_Ret_writes_(CONTEXT_PAIR_COUNT)
CONTEXT* Js::Amd64ContextsManager::Allocate()
{LOGMEIN("StackFrame.cpp] 234\n");
    CONTEXT* pair = NULL;

    switch(curIndex)
    {LOGMEIN("StackFrame.cpp] 238\n");
    case GENERAL_CONTEXT: //0
        pair = InternalGet(curIndex++);
        Assert(curIndex == OOM_CONTEXT); // Next available is OOM_CONTEXT
        break;

    case OOM_CONTEXT: //1
        pair = HeapNewNoThrowArray(CONTEXT, CONTEXT_PAIR_COUNT);
        if (!pair)
        {LOGMEIN("StackFrame.cpp] 247\n");
            pair = InternalGet(curIndex++);
            Assert(curIndex == NUM_CONTEXTS); // Used up all stock contexts
        }
        break;

    default:
        AssertMsg(false, "Unexpected usage of JavascriptStackWalker. We run out of CONTEXTs on amd64.");
        Amd64StackWalkerOutOfContexts_fatal_error((ULONG_PTR)this);
    }

    AnalysisAssert(pair);
    memset(pair, 0, sizeof(CONTEXT) * CONTEXT_PAIR_COUNT);
    return pair;
}

void Js::Amd64ContextsManager::Release(_In_ CONTEXT* contexts)
{LOGMEIN("StackFrame.cpp] 264\n");
    switch(curIndex)
    {LOGMEIN("StackFrame.cpp] 266\n");
    case GENERAL_CONTEXT:
        AssertMsg(false, "Unexpected release of CONTEXTs. No contexts allocated.");
        break;

    case OOM_CONTEXT:
        if (contexts != InternalGet(curIndex - 1))
        {
            HeapDeleteArray(CONTEXT_PAIR_COUNT, contexts);
        }
        else
        {
            --curIndex;
            Assert(curIndex == GENERAL_CONTEXT); // GENERAL_CONTEXT is now available
        }
        break;

    case NUM_CONTEXTS:
        AssertMsg(contexts == InternalGet(curIndex - 1), "Invalid CONTEXT releasing sequence. Expect to release stock contexts for OOM.");
        --curIndex;
        Assert(curIndex == OOM_CONTEXT); // OOM_CONTEXT is now available
        break;

    default:
        Assert(false); // Invalid state
        break;
    }
}
