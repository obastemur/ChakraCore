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
{LOGMEIN("StackFrame.cpp] 14\n");
    this->frame = (void **)frame;

    this->stackCheckCodeHeight =
        scriptContext->GetThreadContext()->DoInterruptProbe() ? stackCheckCodeHeightWithInterruptProbe
        : scriptContext->GetThreadContext()->IsThreadBound() ? stackCheckCodeHeightThreadBound
        : stackCheckCodeHeightNotThreadBound;

    return Next();
}

bool
X86StackFrame::InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext)
{LOGMEIN("StackFrame.cpp] 27\n");
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
    {LOGMEIN("StackFrame.cpp] 41\n");
        if (this->codeAddr == returnAddress)
        {LOGMEIN("StackFrame.cpp] 43\n");
            return true;
        }
    }
    return false;
}

bool
X86StackFrame::Next()
{LOGMEIN("StackFrame.cpp] 52\n");
    this->addressOfCodeAddr = this->GetAddressOfReturnAddress();
    this->codeAddr = this->GetReturnAddress();
    this->frame = (void **)this->frame[0];
    return frame != nullptr;
}

bool
X86StackFrame::SkipToFrame(void * frameAddress)
{LOGMEIN("StackFrame.cpp] 61\n");
    this->frame = (void **)frameAddress;
    return Next();
}

bool
X86StackFrame::IsInStackCheckCode(void *entry, void *codeAddr, size_t stackCheckCodeHeight)
{LOGMEIN("StackFrame.cpp] 68\n");
    return ((size_t(codeAddr) - size_t(entry)) <= stackCheckCodeHeight);
}

};
