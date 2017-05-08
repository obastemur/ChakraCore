//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if !defined(_M_ARM)
#error ArmStackFrame is not supported on this architecture.
#endif

// For ARM, we walk the r11 chain (similar to the EBP chain on x86). This allows us to do what the internal
// stack walker needs to do - find and visit non-leaf Javascript frames on the call stack and retrieve return
// addresses and parameter values. Note that we don't need the equivalent of CONTEXT_UNWOUND_TO_CALL here,
// or any PC adjustment to account for return from tail call, because the PC is only used to determine whether
// we're in a Javascript frame or not, not to control unwinding (via pdata). So we only require that Javascript
// functions not end in call instructions.
// We also require that register parameters be homed on entry to a Javascript function, something that jitted
// functions and the ETW interpreter thunk do, and which C++ vararg functions currently do as well. The guidance
// from C++ is that we not rely on this behavior in future. If we have to visit interpreted Javascript frames
// that don't pass through the ETW thunk, we'll have to use some other mechanism to force homing of parameters.

namespace Js
{

bool
ArmStackFrame::InitializeByFrameId(void * frame, ScriptContext* scriptContext)
{TRACE_IT(53352);
    return SkipToFrame(frame);
}

// InitializeByReturnAddress.
// Parameters:
//   unwindToAddress: specifies the address we need to unwind the stack before any walks can be done.
//     This is expected to be return address i.e. address of the instruction right after the blx instruction
//     and not the address of blx itself.
bool
ArmStackFrame::InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext)
{TRACE_IT(53353);
    this->frame = (void**)arm_GET_CURRENT_FRAME();
    while (Next())
    {TRACE_IT(53354);
        if (this->codeAddr == returnAddress)
        {TRACE_IT(53355);
            return true;
        }
    }
    return false;
}

bool
ArmStackFrame::Next()
{TRACE_IT(53356);
    this->addressOfCodeAddr = this->GetAddressOfReturnAddress();
    this->codeAddr = this->GetReturnAddress();
    this->frame = (void **)this->frame[0];
    return frame != nullptr;
}

bool
ArmStackFrame::SkipToFrame(void * frameAddress)
{TRACE_IT(53357);
    this->frame = (void **)frameAddress;
    return Next();
}

void *
ArmStackFrame::GetInstructionPointer()
{TRACE_IT(53358);
    return codeAddr;
}

void **
ArmStackFrame::GetArgv(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{TRACE_IT(53359);
    UNREFERENCED_PARAMETER(isCurrentContextNative);
    UNREFERENCED_PARAMETER(shouldCheckForNativeAddr);
    return this->frame + ArgOffsetFromFramePtr;
}

void *
ArmStackFrame::GetReturnAddress(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{TRACE_IT(53360);
    UNREFERENCED_PARAMETER(isCurrentContextNative);
    UNREFERENCED_PARAMETER(shouldCheckForNativeAddr);
    return this->frame[ReturnAddrOffsetFromFramePtr];
}

void *
ArmStackFrame::GetAddressOfReturnAddress(bool isCurrentContextNative, bool shouldCheckForNativeAddr)
{TRACE_IT(53361);
    UNREFERENCED_PARAMETER(isCurrentContextNative);
    UNREFERENCED_PARAMETER(shouldCheckForNativeAddr);
    return &this->frame[ReturnAddrOffsetFromFramePtr];
}

bool
ArmStackFrame::IsInStackCheckCode(void *entry, void *codeAddr, size_t stackCheckCodeHeight)
{TRACE_IT(53362);
    return ((size_t(codeAddr) - size_t(entry)) <= stackCheckCodeHeight);
}

}
