//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once


namespace Js {

#ifndef _M_IX86
#error This is only for x86
#endif

    class X86StackFrame {
    public:
        X86StackFrame() : frame(nullptr), codeAddr(nullptr), stackCheckCodeHeight(0), addressOfCodeAddr(nullptr) {TRACE_IT(54065);};

        bool InitializeByFrameId(void * frameAddress, ScriptContext* scriptContext);
        bool InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext);

        bool Next();

        void *  GetInstructionPointer() {TRACE_IT(54066); return codeAddr; }
        void ** GetArgv(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) {TRACE_IT(54067); return frame + 2; } // parameters unused for x86, arm and arm64
        void *  GetReturnAddress(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) {TRACE_IT(54068); return frame[1]; } // parameters unused for x86, arm and arm64
        void *  GetAddressOfReturnAddress(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) {TRACE_IT(54069); return &frame[1]; } // parameters unused for x86, arm and arm64
        void *  GetAddressOfInstructionPointer() const {TRACE_IT(54070); return addressOfCodeAddr; }
        void *  GetFrame() const {TRACE_IT(54071); return (void *)frame; }

        void SetReturnAddress(void * address) {TRACE_IT(54072); frame[1] = address; }
        bool SkipToFrame(void * frameAddress);

        size_t GetStackCheckCodeHeight() {TRACE_IT(54073); return this->stackCheckCodeHeight; }
        static bool IsInStackCheckCode(void *entry, void *codeAddr, size_t stackCheckCodeHeight);

    private:
        void ** frame;      // ebp
        void * codeAddr;    // eip
        void * addressOfCodeAddr;
        size_t stackCheckCodeHeight;

        static const size_t stackCheckCodeHeightThreadBound = StackFrameConstants::StackCheckCodeHeightThreadBound;
        static const size_t stackCheckCodeHeightNotThreadBound = StackFrameConstants::StackCheckCodeHeightNotThreadBound;
        static const size_t stackCheckCodeHeightWithInterruptProbe = StackFrameConstants::StackCheckCodeHeightWithInterruptProbe;
    };

};
