//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if !defined(_M_X64)
#error Amd64StackFrame is not supported on this architecture.
#endif

///
/// Amd64StackFrame public API
///
/// Constructor/Destructor
/// GetAddressOfReturnAddress
/// GetInstructionPointer
/// InitializeByReturnAddress
/// Next
/// SkipToFrame
///
/// Amd64ContextsManager
///
namespace Js
{
bool
Amd64StackFrame::InitializeByFrameId(void * frame, ScriptContext* scriptContext)
{LOGMEIN("StackFrame.SystemV.cpp] 26\n");
    this->frame = (void **)frame;

    this->stackCheckCodeHeight =
        scriptContext->GetThreadContext()->DoInterruptProbe() ? stackCheckCodeHeightWithInterruptProbe
        : scriptContext->GetThreadContext()->IsThreadBound() ? stackCheckCodeHeightThreadBound
        : stackCheckCodeHeightNotThreadBound;

    return Next();
}

bool
Amd64StackFrame::InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext)
{LOGMEIN("StackFrame.SystemV.cpp] 39\n");
    void ** framePtr;
    __asm
    {
        mov framePtr, rbp;
    }
    this->frame = framePtr;

    this->stackCheckCodeHeight =
        scriptContext->GetThreadContext()->DoInterruptProbe() ? stackCheckCodeHeightWithInterruptProbe
        : scriptContext->GetThreadContext()->IsThreadBound() ? stackCheckCodeHeightThreadBound
        : stackCheckCodeHeightNotThreadBound;

    while (Next())
    {LOGMEIN("StackFrame.SystemV.cpp] 53\n");
        if (this->codeAddr == returnAddress)
        {LOGMEIN("StackFrame.SystemV.cpp] 55\n");
            return true;
        }
    }
    return false;
}

bool
Amd64StackFrame::Next()
{LOGMEIN("StackFrame.SystemV.cpp] 64\n");
    this->addressOfCodeAddr = this->GetAddressOfReturnAddress();
    this->codeAddr = this->GetReturnAddress();
    this->frame = (void **)this->frame[0];
    return frame != nullptr;
}

bool
Amd64StackFrame::SkipToFrame(void * frameAddress)
{LOGMEIN("StackFrame.SystemV.cpp] 73\n");
    this->frame = (void **)frameAddress;
    return Next();
}

bool
Amd64StackFrame::IsInStackCheckCode(void *entry, void *codeAddr, size_t stackCheckCodeHeight)
{LOGMEIN("StackFrame.SystemV.cpp] 80\n");
    return ((size_t(codeAddr) - size_t(entry)) <= stackCheckCodeHeight);
}

// Dummy constructor since we don't need to manager the contexts here
Amd64ContextsManager::Amd64ContextsManager() {LOGMEIN("StackFrame.SystemV.cpp] 85\n");}
};

