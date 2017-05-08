//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if !defined(_M_IX86)
#error X86StackFrame is not supported on this architecture.
#endif
namespace Js
{

bool
X86StackFrame::InitializeByFrameId(void * frame, ScriptContext* scriptContext)
{TRACE_IT(54058);
    this->frame = (void **)frame;

    this->stackCheckCodeHeight =
        scriptContext->GetThreadContext()->DoInterruptProbe() ? stackCheckCodeHeightWithInterruptProbe
        : scriptContext->GetThreadContext()->IsThreadBound() ? stackCheckCodeHeightThreadBound
        : stackCheckCodeHeightNotThreadBound;

    return Next();
}

bool
X86StackFrame::InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext)
{TRACE_IT(54059);
    void ** framePtr;
    __asm
    {
        mov framePtr, ebp;
    }
    this->frame = framePtr;

    this->stackCheckCodeHeight =
        scriptContext->GetThreadContext()->DoInterruptProbe() ? stackCheckCodeHeightWithInterruptProbe
        : scriptContext->GetThreadContext()->IsThreadBound() ? stackCheckCodeHeightThreadBound
        : stackCheckCodeHeightNotThreadBound;

    while (Next())
    {TRACE_IT(54060);
        if (this->codeAddr == returnAddress)
        {TRACE_IT(54061);
            return true;
        }
    }
    return false;
}

bool
X86StackFrame::Next()
{TRACE_IT(54062);
    this->addressOfCodeAddr = this->GetAddressOfReturnAddress();
    this->codeAddr = this->GetReturnAddress();
    this->frame = (void **)this->frame[0];
    return frame != nullptr;
}

bool
X86StackFrame::SkipToFrame(void * frameAddress)
{TRACE_IT(54063);
    this->frame = (void **)frameAddress;
    return Next();
}

bool
X86StackFrame::IsInStackCheckCode(void *entry, void *codeAddr, size_t stackCheckCodeHeight)
{TRACE_IT(54064);
    return ((size_t(codeAddr) - size_t(entry)) <= stackCheckCodeHeight);
}

};
