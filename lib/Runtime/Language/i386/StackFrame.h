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
        X86StackFrame() : frame(nullptr), codeAddr(nullptr), stackCheckCodeHeight(0), addressOfCodeAddr(nullptr) {LOGMEIN("StackFrame.h] 15\n");};

        bool InitializeByFrameId(void * frameAddress, ScriptContext* scriptContext);
        bool InitializeByReturnAddress(void * returnAddress, ScriptContext* scriptContext);

        bool Next();

        void *  GetInstructionPointer() {LOGMEIN("StackFrame.h] 22\n"); return codeAddr; }
        void ** GetArgv(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) {LOGMEIN("StackFrame.h] 23\n"); return frame + 2; } // parameters unused for x86, arm and arm64
        void *  GetReturnAddress(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) {LOGMEIN("StackFrame.h] 24\n"); return frame[1]; } // parameters unused for x86, arm and arm64
        void *  GetAddressOfReturnAddress(bool isCurrentContextNative = false, bool shouldCheckForNativeAddr = true) {LOGMEIN("StackFrame.h] 25\n"); return &frame[1]; } // parameters unused for x86, arm and arm64
        void *  GetAddressOfInstructionPointer() const {LOGMEIN("StackFrame.h] 26\n"); return addressOfCodeAddr; }
        void *  GetFrame() const {LOGMEIN("StackFrame.h] 27\n"); return (void *)frame; }

        void SetReturnAddress(void * address) {LOGMEIN("StackFrame.h] 29\n"); frame[1] = address; }
        bool SkipToFrame(void * frameAddress);

        size_t GetStackCheckCodeHeight() {LOGMEIN("StackFrame.h] 32\n"); return this->stackCheckCodeHeight; }
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
