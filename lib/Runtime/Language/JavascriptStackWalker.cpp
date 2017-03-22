//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Language/JavascriptFunctionArgIndex.h"
#include "Language/InterpreterStackFrame.h"

#define FAligned(VALUE, TYPE) ((((LONG_PTR)VALUE) & (sizeof(TYPE)-1)) == 0)

#define AlignIt(VALUE, TYPE) (~(~((LONG_PTR)(VALUE) + (sizeof(TYPE)-1)) | (sizeof(TYPE)-1)))

namespace Js
{
    Js::ArgumentsObject * JavascriptCallStackLayout::GetArgumentsObject() const
    {LOGMEIN("JavascriptStackWalker.cpp] 15\n");
        return (Js::ArgumentsObject *)((void **)this)[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    Js::Var* JavascriptCallStackLayout::GetArgumentsObjectLocation() const
    {LOGMEIN("JavascriptStackWalker.cpp] 20\n");
        return (Js::Var *)&((void **)this)[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    void JavascriptCallStackLayout::SetArgumentsObject(Js::ArgumentsObject * obj)
    {LOGMEIN("JavascriptStackWalker.cpp] 25\n");
        ((void **)this)[JavascriptFunctionArgIndex_ArgumentsObject] =  obj;
    }

    Js::Var JavascriptCallStackLayout::GetOffset(int offset) const
    {LOGMEIN("JavascriptStackWalker.cpp] 30\n");
        Js::Var *varPtr = (Js::Var *)(((char *)this) + offset);
        Assert(FAligned(varPtr, Js::Var));
        return *varPtr;
    }
    double JavascriptCallStackLayout::GetDoubleAtOffset(int offset) const
    {LOGMEIN("JavascriptStackWalker.cpp] 36\n");
        double *dblPtr = (double *)(((char *)this) + offset);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::CheckAlignmentFlag))
        {
            Assert(FAligned(dblPtr, double));
        }
#endif
        return *dblPtr;
    }

    int32 JavascriptCallStackLayout::GetInt32AtOffset(int offset) const
    {LOGMEIN("JavascriptStackWalker.cpp] 48\n");
        int32 *intPtr = (int32 *)(((char *)this) + offset);
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.IsEnabled(Js::CheckAlignmentFlag))
        {
            Assert(FAligned(intPtr, int32));
        }
#endif
        return *intPtr;
    }

    SIMDValue JavascriptCallStackLayout::GetSimdValueAtOffset(int offset) const
    {LOGMEIN("JavascriptStackWalker.cpp] 60\n");
        return  *((SIMDValue *)(((char *)this) + offset));
    }

    char * JavascriptCallStackLayout::GetValueChangeOffset(int offset) const
    {LOGMEIN("JavascriptStackWalker.cpp] 65\n");
        Js::Var *varPtr = (Js::Var *)(((char *)this) + offset);
        Assert(FAligned(varPtr, Js::Var));
        return (char *)varPtr;
    }

    ForInObjectEnumerator * JavascriptCallStackLayout::GetForInObjectEnumeratorArrayAtOffset(int offset) const
    {LOGMEIN("JavascriptStackWalker.cpp] 72\n");
        return (ForInObjectEnumerator *)(((char *)this) + offset);
    }

    JavascriptCallStackLayout *JavascriptCallStackLayout::FromFramePointer(void *const framePointer)
    {LOGMEIN("JavascriptStackWalker.cpp] 77\n");
        return
            reinterpret_cast<JavascriptCallStackLayout *>(
                static_cast<void **>(framePointer) + (JavascriptFunctionArgIndex_Function - JavascriptFunctionArgIndex_Frame));
    }


    void* const JavascriptCallStackLayout::ToFramePointer(JavascriptCallStackLayout* callstackLayout)
    {LOGMEIN("JavascriptStackWalker.cpp] 85\n");
        return
            reinterpret_cast<void * const>(
                reinterpret_cast<void **>(callstackLayout) - (JavascriptFunctionArgIndex_Function - JavascriptFunctionArgIndex_Frame));
    }

    Js::Var* JavascriptCallStackLayout::GetArgv() const
    {LOGMEIN("JavascriptStackWalker.cpp] 92\n");
        return const_cast<Js::Var*>(&this->args[0]);
    }

    ScriptContext* JavascriptStackWalker::GetCurrentScriptContext() const
    {LOGMEIN("JavascriptStackWalker.cpp] 97\n");
        return this->GetCurrentInterpreterFrame() ? this->GetCurrentInterpreterFrame()->GetScriptContext() : this->scriptContext;
    }

    Var JavascriptStackWalker::GetCurrentArgumentsObject() const
    {LOGMEIN("JavascriptStackWalker.cpp] 102\n");
#if ENABLE_PROFILE_INFO
        if (interpreterFrame)
#else
        Assert(interpreterFrame);
#endif
        {LOGMEIN("JavascriptStackWalker.cpp] 108\n");
            return interpreterFrame->GetArgumentsObject();
        }
#if ENABLE_NATIVE_CODEGEN
        else
        {
            if (inlinedFramesBeingWalked)
            {LOGMEIN("JavascriptStackWalker.cpp] 115\n");
                return inlinedFrameWalker.GetArgumentsObject();
            }
            else
            {
                return this->GetCurrentNativeArgumentsObject();
            }
        }
#endif
    }

    void JavascriptStackWalker::SetCurrentArgumentsObject(Var args)
    {LOGMEIN("JavascriptStackWalker.cpp] 127\n");
#if ENABLE_NATIVE_CODEGEN
        if (interpreterFrame)
#else
        Assert(interpreterFrame);
#endif
        {LOGMEIN("JavascriptStackWalker.cpp] 133\n");
            interpreterFrame->SetArgumentsObject(args);
        }
#if ENABLE_NATIVE_CODEGEN
        else
        {
            if (inlinedFramesBeingWalked)
            {LOGMEIN("JavascriptStackWalker.cpp] 140\n");
                inlinedFrameWalker.SetArgumentsObject(args);
            }
            else
            {
                this->SetCurrentNativeArgumentsObject(args);
            }
        }
#endif
    }

    Var JavascriptStackWalker::GetPermanentArguments() const
    {LOGMEIN("JavascriptStackWalker.cpp] 152\n");
        Assert(IsJavascriptFrame());
        AssertMsg(this->GetCurrentFunction()->IsScriptFunction(), "GetPermanentArguments should not be called for non-script function as there is no slot allocated for it.");

        const uint32 paramCount = GetCallInfo()->Count;
        if (paramCount == 0)
        {LOGMEIN("JavascriptStackWalker.cpp] 158\n");
            // glob function doesn't allocate ArgumentsObject slot on stack
            return nullptr;
        }

        // Get the heap-allocated args for this frame.
        Var args = this->GetCurrentArgumentsObject();
        if (args && ArgumentsObject::Is(args))
        {LOGMEIN("JavascriptStackWalker.cpp] 166\n");
            args = ((ArgumentsObject*)args)->GetHeapArguments();
        }
        return args;
    }

    BOOL JavascriptStackWalker::WalkToArgumentsFrame(ArgumentsObject *args)
    {LOGMEIN("JavascriptStackWalker.cpp] 173\n");
        // Move the walker up the stack until we find the given arguments object on the frame.
        while (this->Walk(/*includeInlineFrame*/ true))
        {LOGMEIN("JavascriptStackWalker.cpp] 176\n");
            if (this->IsJavascriptFrame())
            {LOGMEIN("JavascriptStackWalker.cpp] 178\n");
                Var currArgs = this->GetCurrentArgumentsObject();
                if (currArgs == args)
                {LOGMEIN("JavascriptStackWalker.cpp] 181\n");
                    return TRUE;
                }
            }
        }
        return FALSE;
    }

    bool JavascriptStackWalker::GetThis(Var* pVarThis, int moduleId) const
    {LOGMEIN("JavascriptStackWalker.cpp] 190\n");
#if ENABLE_NATIVE_CODEGEN
        if (inlinedFramesBeingWalked)
        {LOGMEIN("JavascriptStackWalker.cpp] 193\n");
            if (inlinedFrameWalker.GetArgc() == 0)
            {LOGMEIN("JavascriptStackWalker.cpp] 195\n");
                *pVarThis = JavascriptOperators::OP_GetThis(this->scriptContext->GetLibrary()->GetUndefined(), moduleId, scriptContext);
                return false;
            }

            *pVarThis = inlinedFrameWalker.GetThisObject();
            Assert(*pVarThis);

            return true;
        }
        else
#endif
        {
            CallInfo const *callInfo = this->GetCallInfo();
            if (callInfo->Count == 0)
            {LOGMEIN("JavascriptStackWalker.cpp] 210\n");
                *pVarThis = JavascriptOperators::OP_GetThis(scriptContext->GetLibrary()->GetUndefined(), moduleId, scriptContext);
                return false;
            }

            *pVarThis = this->GetThisFromFrame();
            return (*pVarThis) != nullptr;
        }
    }

    BOOL IsEval(const CallInfo* callInfo)
    {LOGMEIN("JavascriptStackWalker.cpp] 221\n");
        return (callInfo->Flags & CallFlags_Eval) != 0;
    }

    BOOL JavascriptStackWalker::IsCallerGlobalFunction() const
    {LOGMEIN("JavascriptStackWalker.cpp] 226\n");
        CallInfo const* callInfo = this->GetCallInfo();

        JavascriptFunction* function = this->GetCurrentFunction();
        if (IsLibraryStackFrameEnabled(this->scriptContext) && !function->IsScriptFunction())
        {LOGMEIN("JavascriptStackWalker.cpp] 231\n");
            return false; // native library code can't be global function
        }

        FunctionInfo* funcInfo = function->GetFunctionInfo();
        if (funcInfo->HasParseableInfo())
        {LOGMEIN("JavascriptStackWalker.cpp] 237\n");
            return funcInfo->GetParseableFunctionInfo()->GetIsGlobalFunc() || IsEval(callInfo);
        }
        else
        {
            AssertMsg(FALSE, "Here we should only have script functions which were already parsed/deserialized.");
            return callInfo->Count == 0 || IsEval(callInfo);
        }
    }

    BOOL JavascriptStackWalker::IsEvalCaller() const
    {LOGMEIN("JavascriptStackWalker.cpp] 248\n");
        CallInfo const* callInfo = this->GetCallInfo();
        return (callInfo->Flags & CallFlags_Eval) != 0;
    }

    Var JavascriptStackWalker::GetCurrentNativeArgumentsObject() const
    {LOGMEIN("JavascriptStackWalker.cpp] 254\n");
        Assert(this->IsJavascriptFrame() && this->interpreterFrame == nullptr);
        return this->GetCurrentArgv()[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    void JavascriptStackWalker::SetCurrentNativeArgumentsObject(Var args)
    {LOGMEIN("JavascriptStackWalker.cpp] 260\n");
        Assert(this->IsJavascriptFrame() && this->interpreterFrame == nullptr);
        this->GetCurrentArgv()[JavascriptFunctionArgIndex_ArgumentsObject] = args;
    }

    Js::Var * JavascriptStackWalker::GetJavascriptArgs() const
    {LOGMEIN("JavascriptStackWalker.cpp] 266\n");
        Assert(this->IsJavascriptFrame());

#if ENABLE_NATIVE_CODEGEN
        if (inlinedFramesBeingWalked)
        {LOGMEIN("JavascriptStackWalker.cpp] 271\n");
            return inlinedFrameWalker.GetArgv(/* includeThis = */ false);
        }
        else
#endif
            if (this->GetCurrentFunction()->GetFunctionInfo()->IsCoroutine())
        {LOGMEIN("JavascriptStackWalker.cpp] 277\n");
            JavascriptGenerator* gen = JavascriptGenerator::FromVar(this->GetCurrentArgv()[JavascriptFunctionArgIndex_This]);
            return gen->GetArguments().Values;
        }
        else
        {
            return &this->GetCurrentArgv()[JavascriptFunctionArgIndex_SecondScriptArg];
        }
    }

    uint32 JavascriptStackWalker::GetByteCodeOffset() const
    {LOGMEIN("JavascriptStackWalker.cpp] 288\n");
        uint32 offset = 0;
        if (this->IsJavascriptFrame())
        {LOGMEIN("JavascriptStackWalker.cpp] 291\n");
            if (this->interpreterFrame)
            {LOGMEIN("JavascriptStackWalker.cpp] 293\n");
                if (this->TryGetByteCodeOffsetFromInterpreterFrame(offset))
                {LOGMEIN("JavascriptStackWalker.cpp] 295\n");
                    return offset;
                }
            }

#if ENABLE_NATIVE_CODEGEN
            if (TryGetByteCodeOffsetFromNativeFrame(offset))
            {LOGMEIN("JavascriptStackWalker.cpp] 302\n");
                return offset;
            }
#endif
        }
        return offset;
    }

    bool JavascriptStackWalker::TryGetByteCodeOffsetFromInterpreterFrame(uint32& offset) const
    {LOGMEIN("JavascriptStackWalker.cpp] 311\n");
#if ENABLE_NATIVE_CODEGEN
        if (this->lastInternalFrameInfo.codeAddress != nullptr)
        {LOGMEIN("JavascriptStackWalker.cpp] 314\n");
            return false;
        }
#endif
        offset = this->interpreterFrame->GetReader()->GetCurrentOffset();
        if (offset == 0)
        {LOGMEIN("JavascriptStackWalker.cpp] 320\n");
            // This will be the case when we are broken on the debugger on very first statement (due to async break).
            // Or the interpreter loop can throw OOS on entrance before executing bytecode.
        }
        else
        {
            // Note : For many cases, we move the m_currentLocation of ByteCodeReader already to next available opcode.
            // This could create problem in binding the exception to proper line offset.
            // Reducing by 1 will make sure the current offset falls under, current executing opcode.
            offset--;
        }
        return true;
    }

#if ENABLE_NATIVE_CODEGEN
    bool JavascriptStackWalker::TryGetByteCodeOffsetFromNativeFrame(uint32& offset) const
    {LOGMEIN("JavascriptStackWalker.cpp] 336\n");
        DWORD_PTR pCodeAddr;
        if (this->lastInternalFrameInfo.codeAddress != nullptr)
        {LOGMEIN("JavascriptStackWalker.cpp] 339\n");
            pCodeAddr = (DWORD_PTR)this->lastInternalFrameInfo.codeAddress;
        }
        else
        {
            pCodeAddr = (DWORD_PTR)this->GetCurrentCodeAddr();
        }

        // If the current instruction's return address is the beginning of the next statement then we will show error for the next line, which would be completely wrong.
        // The quick fix would be to look the address which is at least lesser than current return address.

        // Assert to verify at what places this can happen.
        Assert(pCodeAddr);

        if (pCodeAddr)
        {LOGMEIN("JavascriptStackWalker.cpp] 354\n");
#if defined(_M_ARM32_OR_ARM64)
            // Note that DWORD_PTR is not actually a pointer type (!) but is simple unsigned long/__int64 (see BaseTsd.h).
            // Thus, decrement would be by 1 byte and not 4 bytes as in pointer arithmetic. That's exactly what we need.
            // For ARM the 'return address' is always odd and is 'next instr addr' + 1 byte, so to get to the BLX instr, we need to subtract 2 bytes from it.
            AssertMsg(pCodeAddr % 2 == 1, "Got even number for pCodeAddr! It's expected to be return address, which should be odd.");
            pCodeAddr--;
#endif
            pCodeAddr--;
        }

        bool usedInternalFrameInfo = false;
        uint loopNum = GetLoopNumber(usedInternalFrameInfo);

        JavascriptFunction *function = nullptr;
        FunctionBody *inlinee = nullptr;

        function = usedInternalFrameInfo ? this->GetCachedInternalFrameInfo().function : this->GetCurrentFunctionFromPhysicalFrame();

        // If there are inlined frames on the stack, we have to be able to return the byte code offsets of those inlined calls
        // from their respective callers. But, we can use the current native address as IP for only the topmost inlined frame.
        // TryGetByteCodeOffsetOfInlinee takes care of these conditions and sets up the offset of an inlinee in 'offset', if the
        // current inlinee frame is not the topmost of the inlinee frames.
        if (HasInlinedFramesOnStack() && TryGetByteCodeOffsetOfInlinee(function, loopNum, pCodeAddr, &inlinee, offset, usedInternalFrameInfo))
        {LOGMEIN("JavascriptStackWalker.cpp] 378\n");
            return true;
        }

        StatementData data;
        if (function->GetFunctionBody() && function->GetFunctionBody()->GetMatchingStatementMapFromNativeAddress(pCodeAddr, data, loopNum, inlinee))
        {LOGMEIN("JavascriptStackWalker.cpp] 384\n");
            offset = data.bytecodeBegin;
            return true;
        }

        return false;
    }

    uint JavascriptStackWalker::GetLoopNumber(bool& usedInternalFrameInfo) const
    {LOGMEIN("JavascriptStackWalker.cpp] 393\n");
        uint loopNum = LoopHeader::NoLoop;
        if (this->lastInternalFrameInfo.codeAddress != nullptr)
        {LOGMEIN("JavascriptStackWalker.cpp] 396\n");
            if (this->lastInternalFrameInfo.frameType == InternalFrameType_LoopBody)
            {LOGMEIN("JavascriptStackWalker.cpp] 398\n");
                AnalysisAssert(this->interpreterFrame);
                loopNum = this->interpreterFrame->GetCurrentLoopNum();
                Assert(loopNum != LoopHeader::NoLoop);
                usedInternalFrameInfo = true;
            }
        }
        else
        {
            if (this->IsCurrentPhysicalFrameForLoopBody())
            {LOGMEIN("JavascriptStackWalker.cpp] 408\n");
                // Internal frame but codeAddress on lastInternalFrameInfo not set. We must be in an inlined frame in the loop body.
                Assert(this->tempInterpreterFrame);
                loopNum = this->tempInterpreterFrame->GetCurrentLoopNum();
                Assert(loopNum != LoopHeader::NoLoop);
                usedInternalFrameInfo = false;
            }
        }

        return loopNum;
    }

    bool JavascriptStackWalker::TryGetByteCodeOffsetOfInlinee(Js::JavascriptFunction* parentFunction, uint loopNum, DWORD_PTR pCodeAddr, Js::FunctionBody** inlinee, uint32& offset, bool useInternalFrameInfo) const
    {LOGMEIN("JavascriptStackWalker.cpp] 421\n");
        // For inlined frames, translation from native offset -> source code happens in two steps.
        // The native offset is first translated into a statement index using the physical frame's
        // source context info. This statement index is then looked up in the *inlinee*'s source
        // context info to get the bytecode offset.
        //
        // For all inlined frames contained within a physical frame we have only one offset == (IP - entry).
        // Since we can't use that to get the other inlined callers' IPs, we save the IP of all inlined
        // callers in their "callinfo" (See InlineeCallInfo). The top most inlined frame uses the IP
        // of the physical frame. All other inlined frames use the InlineeStartOffset stored in their call info
        // to calculate the byte code offset of the callsite of the inlinee they called.

        StatementData data;
        uint32 inlineeOffset = 0;
        *inlinee = InlinedFramesBeingWalked() ? inlinedFrameWalker.GetFunctionObject()->GetFunctionBody() : nullptr;

        InlinedFrameWalker  tmpFrameWalker;
        if (InlinedFramesBeingWalked())
        {LOGMEIN("JavascriptStackWalker.cpp] 439\n");
            // Inlined frames are being walked right now. The top most frame is where the IP is.
            if (!inlinedFrameWalker.IsTopMostFrame())
            {LOGMEIN("JavascriptStackWalker.cpp] 442\n");
                inlineeOffset = inlinedFrameWalker.GetCurrentInlineeOffset();
            }
        }
        else if (ScriptFunction::Is(parentFunction) && HasInlinedFramesOnStack())
        {LOGMEIN("JavascriptStackWalker.cpp] 447\n");
            // Inlined frames are not being walked right now. However, if there
            // are inlined frames on the stack the InlineeCallInfo of the first inlined frame
            // has the native offset of the current physical frame.
            Assert(!*inlinee);
            InlinedFrameWalker::FromPhysicalFrame(tmpFrameWalker, currentFrame, ScriptFunction::FromVar(parentFunction), PreviousInterpreterFrameIsFromBailout(), loopNum, this, useInternalFrameInfo);
            inlineeOffset = tmpFrameWalker.GetBottomMostInlineeOffset();
            tmpFrameWalker.Close();
        }

        if (inlineeOffset != 0 &&
            parentFunction->GetFunctionBody()->GetMatchingStatementMapFromNativeOffset(pCodeAddr, inlineeOffset, data, loopNum, *inlinee))
        {LOGMEIN("JavascriptStackWalker.cpp] 459\n");
            offset = data.bytecodeBegin;
            return true;
        }

        return false;
    }

    bool JavascriptStackWalker::PreviousInterpreterFrameIsFromBailout() const
    {LOGMEIN("JavascriptStackWalker.cpp] 468\n");
        if (lastInternalFrameInfo.codeAddress)
        {LOGMEIN("JavascriptStackWalker.cpp] 470\n");
            return lastInternalFrameInfo.previousInterpreterFrameIsFromBailout;
        }

        return this->previousInterpreterFrameIsFromBailout;
    }

    bool JavascriptStackWalker::InlinedFramesBeingWalked() const
    {LOGMEIN("JavascriptStackWalker.cpp] 478\n");
        if (lastInternalFrameInfo.codeAddress)
        {LOGMEIN("JavascriptStackWalker.cpp] 480\n");
            return false;
        }

        return this->inlinedFramesBeingWalked;
    }

    bool JavascriptStackWalker::HasInlinedFramesOnStack() const
    {LOGMEIN("JavascriptStackWalker.cpp] 488\n");
        if (lastInternalFrameInfo.codeAddress)
        {LOGMEIN("JavascriptStackWalker.cpp] 490\n");
            return lastInternalFrameInfo.hasInlinedFramesOnStack;
        }

        return this->hasInlinedFramesOnStack;
    }
#endif

    bool JavascriptStackWalker::GetSourcePosition(const WCHAR** sourceFileName, ULONG* line, LONG* column)
    {LOGMEIN("JavascriptStackWalker.cpp] 499\n");
        uint byteCodeoffset = this->GetByteCodeOffset();
        if(byteCodeoffset)
        {LOGMEIN("JavascriptStackWalker.cpp] 502\n");
            Js::FunctionBody* functionBody = this->GetCurrentFunction()->GetFunctionBody();
            if (functionBody->GetLineCharOffset(byteCodeoffset, line, column))
            {LOGMEIN("JavascriptStackWalker.cpp] 505\n");
                if(functionBody->GetUtf8SourceInfo()->IsDynamic())
                {LOGMEIN("JavascriptStackWalker.cpp] 507\n");
                    *sourceFileName = _u("Dynamic Code");
                }
                else
                {
                    *sourceFileName = functionBody->GetUtf8SourceInfo()->GetSrcInfo()->sourceContextInfo->url;
                }
                return true;
            }
        }
        return false;
    }

    Js::JavascriptFunction * JavascriptStackWalker::UpdateFrame(bool includeInlineFrames)
    {LOGMEIN("JavascriptStackWalker.cpp] 521\n");
        this->isJavascriptFrame = this->CheckJavascriptFrame(includeInlineFrames);

        if (this->IsJavascriptFrame())
        {LOGMEIN("JavascriptStackWalker.cpp] 525\n");
            // In case we have a cross site thunk, update the script context
            Js::JavascriptFunction *function = this->GetCurrentFunction();

#if ENABLE_NATIVE_CODEGEN
            bool isCurrentPhysicalFrameForLoopBody = this->IsCurrentPhysicalFrameForLoopBody();
#endif
            if (this->interpreterFrame)
            {LOGMEIN("JavascriptStackWalker.cpp] 533\n");
#if ENABLE_NATIVE_CODEGEN
                if (lastInternalFrameInfo.codeAddress != nullptr)
                {LOGMEIN("JavascriptStackWalker.cpp] 536\n");
                    this->previousInterpreterFrameIsForLoopBody = true;
                }
                else
#endif
                {
                    this->previousInterpreterFrameIsForLoopBody = false;
                }

                // We might've bailed out of an inlinee, so check if there were any inlinees.
                if (this->interpreterFrame->GetFlags() & InterpreterStackFrameFlags_FromBailOut)
                {LOGMEIN("JavascriptStackWalker.cpp] 547\n");
                    previousInterpreterFrameIsFromBailout = true;

#if ENABLE_NATIVE_CODEGEN
                    Assert(!inlinedFramesBeingWalked);
                    if (includeInlineFrames)
                    {LOGMEIN("JavascriptStackWalker.cpp] 553\n");
                        int loopNum = -1;
                        if (isCurrentPhysicalFrameForLoopBody)
                        {LOGMEIN("JavascriptStackWalker.cpp] 556\n");
                            loopNum = this->tempInterpreterFrame->GetCurrentLoopNum();
                        }

                        bool hasInlinedFramesOnStack = InlinedFrameWalker::FromPhysicalFrame(inlinedFrameWalker, currentFrame,
                            ScriptFunction::FromVar(function), true /*fromBailout*/, loopNum, this, false /*useInternalFrameInfo*/);
                        if (hasInlinedFramesOnStack)
                        {LOGMEIN("JavascriptStackWalker.cpp] 563\n");
                            // We're now back in the state where currentFrame == physical frame of the inliner, but
                            // since interpreterFrame != null, we'll pick values from the interpreterFrame (the bailout
                            // frame of the inliner). Set a flag to tell the stack walker that it needs to start from the
                            // inlinee frames on the stack when Walk() is called.
                            this->inlinedFramesBeingWalked = inlinedFrameWalker.Next(inlinedFrameCallInfo);
                            this->hasInlinedFramesOnStack = hasInlinedFramesOnStack;
                            Assert(inlinedFramesBeingWalked);
                            Assert(StackScriptFunction::GetCurrentFunctionObject(this->interpreterFrame->GetJavascriptFunction()) == inlinedFrameWalker.GetFunctionObject());
                        }
                        else
                        {
                            Assert(!isCurrentPhysicalFrameForLoopBody);
                        }
                    }
                    else if (isCurrentPhysicalFrameForLoopBody)
                    {
                        // Getting here is only possible when the current interpreterFrame is for a function which
                        // encountered a bailout after getting inlined in a jitted loop body. If we are not including
                        // inlined frames in the stack walk, we need to set the codeAddress on lastInternalFrameInfo,
                        // which would have otherwise been set upon closing the inlinedFrameWalker, now.
                        // Note that we already have an assert in CheckJavascriptFrame to ensure this.
                        SetCachedInternalFrameInfo(InternalFrameType_LoopBody, function, false /*hasInlinedFramesOnStack*/, true /*previousInterpreterFrameIsFromBailout*/);
                    }
#else
                    // How did we bail out when JIT was disabled?
                    Assert(false);
#endif
                }
                else
                {
                    Assert(StackScriptFunction::GetCurrentFunctionObject(this->interpreterFrame->GetJavascriptFunction()) == function);
                    previousInterpreterFrameIsFromBailout = false;
                }
            }
            else if (!this->isNativeLibraryFrame)
            {LOGMEIN("JavascriptStackWalker.cpp] 599\n");
#if ENABLE_NATIVE_CODEGEN
                Assert(!HasInlinedFramesOnStack() || (includeInlineFrames && isCurrentPhysicalFrameForLoopBody));

                if (!HasInlinedFramesOnStack() && includeInlineFrames)
                {LOGMEIN("JavascriptStackWalker.cpp] 604\n");
                    // Check whether there are inlined frames nested in this native frame. The corresponding check for
                    // a jitted loop body frame should have been done in CheckJavascriptFrame
                    Assert(lastInternalFrameInfo.codeAddress == nullptr);
                    if (InlinedFrameWalker::FromPhysicalFrame(inlinedFrameWalker, currentFrame, ScriptFunction::FromVar(function)))
                    {LOGMEIN("JavascriptStackWalker.cpp] 609\n");
                        this->inlinedFramesBeingWalked = inlinedFrameWalker.Next(inlinedFrameCallInfo);
                        this->hasInlinedFramesOnStack = true;
                        Assert(inlinedFramesBeingWalked);
                    }
                }
#endif
            }
            this->scriptContext = function->GetScriptContext();
            return function;
        }
        return nullptr;
    }

    // Note: noinline is to make sure that when we unwind to the unwindToAddress, there is at least one frame to unwind.
    _NOINLINE
    JavascriptStackWalker::JavascriptStackWalker(ScriptContext * scriptContext, bool useEERContext, PVOID returnAddress, bool _forceFullWalk /*=false*/) :
        inlinedFrameCallInfo(CallFlags_None, 0), shouldDetectPartiallyInitializedInterpreterFrame(true), forceFullWalk(_forceFullWalk),
        previousInterpreterFrameIsFromBailout(false), previousInterpreterFrameIsForLoopBody(false), hasInlinedFramesOnStack(false)
    {LOGMEIN("JavascriptStackWalker.cpp] 628\n");
        if (scriptContext == NULL)
        {LOGMEIN("JavascriptStackWalker.cpp] 630\n");
            Throw::InternalError();
        }
        this->scriptContext = scriptContext;

        // Pull the current script state from the thread context.

        ThreadContext * threadContext = scriptContext->GetThreadContext();
        this->entryExitRecord = threadContext->GetScriptEntryExit();

        this->nativeLibraryEntry = threadContext->PeekNativeLibraryEntry();
        this->prevNativeLibraryEntry = nullptr;

        this->interpreterFrame = NULL;
        this->isJavascriptFrame = false;
        this->isNativeLibraryFrame = false;

        if (entryExitRecord->frameIdOfScriptExitFunction != NULL)
        {LOGMEIN("JavascriptStackWalker.cpp] 648\n");
            // We're currently outside the script, so grab the frame from which we left.
            this->scriptContext = entryExitRecord->scriptContext;
            this->isInitialFrame = this->currentFrame.InitializeByFrameId(entryExitRecord->frameIdOfScriptExitFunction, this->scriptContext);
        }
        else
        {
            // Just start with the caller
            this->isInitialFrame = this->currentFrame.InitializeByReturnAddress(_ReturnAddress(), this->scriptContext);
        }

        if (useEERContext)
        {LOGMEIN("JavascriptStackWalker.cpp] 660\n");
            this->tempInterpreterFrame = this->scriptContext->GetThreadContext()->GetLeafInterpreterFrame();
        }
        else
        {
            // We need to generate stack for the passed script context, so use the leaf interpreter frame for passed script context
            this->tempInterpreterFrame = scriptContext->GetThreadContext()->GetLeafInterpreterFrame();
        }

        inlinedFramesBeingWalked = false;
    }

    BOOL JavascriptStackWalker::Walk(bool includeInlineFrames)
    {LOGMEIN("JavascriptStackWalker.cpp] 673\n");
        // Walk one frame up the call stack.
        this->interpreterFrame = NULL;

#if ENABLE_NATIVE_CODEGEN
        if (lastInternalFrameInfo.codeAddress != nullptr && this->previousInterpreterFrameIsForLoopBody)
        {LOGMEIN("JavascriptStackWalker.cpp] 679\n");
            ClearCachedInternalFrameInfo();
        }

        if (inlinedFramesBeingWalked)
        {LOGMEIN("JavascriptStackWalker.cpp] 684\n");
            Assert(includeInlineFrames);
            inlinedFramesBeingWalked = inlinedFrameWalker.Next(inlinedFrameCallInfo);
            if (!inlinedFramesBeingWalked)
            {LOGMEIN("JavascriptStackWalker.cpp] 688\n");
                inlinedFrameWalker.Close();
                if ((this->IsCurrentPhysicalFrameForLoopBody()))
                {LOGMEIN("JavascriptStackWalker.cpp] 691\n");
                    // Done walking inlined frames in a loop body, cache the native code address now
                    // in order to skip the loop body frame.
                    this->SetCachedInternalFrameInfo(InternalFrameType_LoopBody, this->GetCurrentFunctionFromPhysicalFrame(), true /*hasInlinedFramesOnStack*/, previousInterpreterFrameIsFromBailout);
                    isJavascriptFrame = false;
                }
            }

            return true;
        }
#endif
        this->hasInlinedFramesOnStack = false;
        if (this->isInitialFrame)
        {LOGMEIN("JavascriptStackWalker.cpp] 704\n");
            this->isInitialFrame = false; // Only walk initial frame once
        }
        else if (!this->currentFrame.Next())
        {LOGMEIN("JavascriptStackWalker.cpp] 708\n");
            this->isJavascriptFrame = false;
            return false;
        }

        // If we're at the entry from a host frame, hop to the frame from which we left the script.
        if (this->currentFrame.GetInstructionPointer() == this->entryExitRecord->returnAddrOfScriptEntryFunction)
        {LOGMEIN("JavascriptStackWalker.cpp] 715\n");
            BOOL hasCaller = this->entryExitRecord->hasCaller || this->forceFullWalk;

#ifdef CHECK_STACKWALK_EXCEPTION
            BOOL ignoreStackWalkException = this->entryExitRecord->ignoreStackWalkException;
#endif

            this->entryExitRecord = this->entryExitRecord->next;
            if (this->entryExitRecord == NULL)
            {LOGMEIN("JavascriptStackWalker.cpp] 724\n");
                this->isJavascriptFrame = false;
                return false;
            }

            if (!hasCaller && !this->scriptContext->IsDiagnosticsScriptContext())
            {LOGMEIN("JavascriptStackWalker.cpp] 730\n");
#ifdef CHECK_STACKWALK_EXCEPTION
                if (!ignoreStackWalkException)
                {
                    AssertMsg(false, "walk pass no caller frame");
                }
#endif
                this->isJavascriptFrame = false;
                return false;
            }

            this->scriptContext = this->entryExitRecord->scriptContext;
            this->currentFrame.SkipToFrame(this->entryExitRecord->frameIdOfScriptExitFunction);
        }

        this->UpdateFrame(includeInlineFrames);
        return true;
    }

    BOOL JavascriptStackWalker::GetCallerWithoutInlinedFrames(JavascriptFunction ** ppFunc)
    {LOGMEIN("JavascriptStackWalker.cpp] 750\n");
        return GetCaller(ppFunc, /*includeInlineFrames*/ false);
    }

    BOOL JavascriptStackWalker::GetCaller(JavascriptFunction ** ppFunc, bool includeInlineFrames)
    {LOGMEIN("JavascriptStackWalker.cpp] 755\n");
        while (this->Walk(includeInlineFrames))
        {LOGMEIN("JavascriptStackWalker.cpp] 757\n");
            if (this->IsJavascriptFrame())
            {LOGMEIN("JavascriptStackWalker.cpp] 759\n");
                Assert(entryExitRecord != NULL);
                *ppFunc = this->GetCurrentFunction();
                AssertMsg(!this->shouldDetectPartiallyInitializedInterpreterFrame, "must have skipped first frame if needed");
                return true;
            }
        }
        *ppFunc = (JavascriptFunction*)this->scriptContext->GetLibrary()->GetNull();
        return false;
    }

    BOOL JavascriptStackWalker::GetNonLibraryCodeCaller(JavascriptFunction ** ppFunc)
    {LOGMEIN("JavascriptStackWalker.cpp] 771\n");
        while (this->GetCaller(ppFunc))
        {LOGMEIN("JavascriptStackWalker.cpp] 773\n");
            if (!(*ppFunc)->IsLibraryCode())
            {LOGMEIN("JavascriptStackWalker.cpp] 775\n");
                return true;
            }
        }
        return false;
    }

    /*static*/
    bool JavascriptStackWalker::IsLibraryStackFrameEnabled(Js::ScriptContext * scriptContext)
    {LOGMEIN("JavascriptStackWalker.cpp] 784\n");
        Assert(scriptContext != nullptr);
        return CONFIG_FLAG(LibraryStackFrame);
    }

    // Check if a function is a display caller: user code, or native library / boundary script library code
    bool JavascriptStackWalker::IsDisplayCaller(JavascriptFunction* func)
    {LOGMEIN("JavascriptStackWalker.cpp] 791\n");
        FunctionBody* body = func->GetFunctionBody();
        if (IsLibraryStackFrameEnabled(func->GetScriptContext()))
        {LOGMEIN("JavascriptStackWalker.cpp] 794\n");
            return !func->IsScriptFunction() || !body->GetUtf8SourceInfo()->GetIsLibraryCode() || body->IsPublicLibraryCode();
        }
        else
        {
            return !body->GetUtf8SourceInfo()->GetIsLibraryCode();
        }
    }

    bool JavascriptStackWalker::GetDisplayCaller(JavascriptFunction ** ppFunc)
    {LOGMEIN("JavascriptStackWalker.cpp] 804\n");
        while (this->GetCaller(ppFunc))
        {LOGMEIN("JavascriptStackWalker.cpp] 806\n");
            if (IsDisplayCaller(*ppFunc))
            {LOGMEIN("JavascriptStackWalker.cpp] 808\n");
                return true;
            }
        }
        return false;
    }

    PCWSTR JavascriptStackWalker::GetCurrentNativeLibraryEntryName() const
    {LOGMEIN("JavascriptStackWalker.cpp] 816\n");
        Assert(IsLibraryStackFrameEnabled(this->scriptContext)
            && this->prevNativeLibraryEntry
            && this->prevNativeLibraryEntry->next == this->nativeLibraryEntry);
        return this->prevNativeLibraryEntry->name;
    }

    // WalkToTarget skips internal frames
    BOOL JavascriptStackWalker::WalkToTarget(JavascriptFunction * funcTarget)
    {LOGMEIN("JavascriptStackWalker.cpp] 825\n");
        // Walk up the call stack until we find the frame that belongs to the given function.

        while (this->Walk(/*includeInlineFrames*/ true))
        {LOGMEIN("JavascriptStackWalker.cpp] 829\n");
            if (this->IsJavascriptFrame() && this->GetCurrentFunction() == funcTarget)
            {LOGMEIN("JavascriptStackWalker.cpp] 831\n");
                // Skip internal names
                Assert( !(this->GetCallInfo()->Flags & CallFlags_InternalFrame) );
                return true;
            }
        }

        return false;
    }

    void ** JavascriptStackWalker::GetCurrentArgv() const
    {LOGMEIN("JavascriptStackWalker.cpp] 842\n");
        Assert(this->IsJavascriptFrame());
        Assert(this->interpreterFrame != nullptr ||
               (this->prevNativeLibraryEntry && this->currentFrame.GetAddressOfReturnAddress() == this->prevNativeLibraryEntry->addr) ||
               JavascriptFunction::IsNativeAddress(this->scriptContext, (void*)this->currentFrame.GetInstructionPointer()));

        bool isNativeAddr = (this->interpreterFrame == nullptr) &&
                            (!this->prevNativeLibraryEntry || (this->currentFrame.GetAddressOfReturnAddress() != this->prevNativeLibraryEntry->addr));
        void ** argv = currentFrame.GetArgv(isNativeAddr, false /*shouldCheckForNativeAddr*/);
        Assert(argv);
        return argv;
    }

    bool JavascriptStackWalker::CheckJavascriptFrame(bool includeInlineFrames)
    {LOGMEIN("JavascriptStackWalker.cpp] 856\n");
        this->isNativeLibraryFrame = false; // Clear previous result

        void * codeAddr = this->currentFrame.GetInstructionPointer();
        if (this->tempInterpreterFrame && codeAddr == this->tempInterpreterFrame->GetReturnAddress())
        {LOGMEIN("JavascriptStackWalker.cpp] 861\n");
            bool isBailoutInterpreter = (this->tempInterpreterFrame->GetFlags() & Js::InterpreterStackFrameFlags_FromBailOut) != 0;

            // We need to skip over the first interpreter frame on the stack if it is the partially initialized frame
            // otherwise it is a real frame and we should continue.
            // For fully initialized frames (PushPopHelper was called) the thunk stack addr is equal or below addressOfReturnAddress
            // as the latter one is obtained in InterpreterStackFrame::InterpreterThunk called by the thunk.
            bool isPartiallyInitializedFrame = this->shouldDetectPartiallyInitializedInterpreterFrame &&
                this->currentFrame.GetAddressOfReturnAddress(isBailoutInterpreter /*isCurrentContextNative*/, false /*shouldCheckForNativeAddr*/) < this->tempInterpreterFrame->GetAddressOfReturnAddress();
            this->shouldDetectPartiallyInitializedInterpreterFrame = false;

            if (isPartiallyInitializedFrame)
            {LOGMEIN("JavascriptStackWalker.cpp] 873\n");
                return false; // Skip it.
            }

            void ** argv = this->currentFrame.GetArgv(isBailoutInterpreter /*isCurrentContextNative*/, false /*shouldCheckForNativeAddr*/);
            if (argv == nullptr)
            {LOGMEIN("JavascriptStackWalker.cpp] 879\n");
                // NOTE: When we switch to walking the stack ourselves and skip non engine frames, this should never happen.
                return false;
            }

            this->interpreterFrame = this->tempInterpreterFrame;

            this->tempInterpreterFrame = this->interpreterFrame->GetPreviousFrame();

#if ENABLE_NATIVE_CODEGEN
#if DBG
            if (((CallInfo const *)&argv[JavascriptFunctionArgIndex_CallInfo])->Flags & CallFlags_InternalFrame)
            {LOGMEIN("JavascriptStackWalker.cpp] 891\n");
                // The return address of the interpreterFrame is the same as the entryPoint for a jitted loop body.
                // This can only ever happen when we have bailed out from a function inlined in the loop body. We
                // wouldn't have created a new interpreterFrame if the bailout were from the loop body itself.
                Assert((this->interpreterFrame->GetFlags() & Js::InterpreterStackFrameFlags_FromBailOut) != 0);
                InlinedFrameWalker tmpFrameWalker;
                Assert(InlinedFrameWalker::FromPhysicalFrame(tmpFrameWalker, currentFrame, Js::ScriptFunction::FromVar(argv[JavascriptFunctionArgIndex_Function]),
                    true /*fromBailout*/, this->tempInterpreterFrame->GetCurrentLoopNum(), this, false /*useInternalFrameInfo*/, true /*noAlloc*/));
                tmpFrameWalker.Close();
            }
#endif //DBG
#endif //ENABLE_NATIVE_CODEGEN

            return true;
        }

        if (IsLibraryStackFrameEnabled(this->scriptContext) && this->nativeLibraryEntry)
        {LOGMEIN("JavascriptStackWalker.cpp] 908\n");
            void* addressOfReturnAddress = this->currentFrame.GetAddressOfReturnAddress();
            AssertMsg(addressOfReturnAddress <= this->nativeLibraryEntry->addr, "Missed matching native library entry?");
            if (addressOfReturnAddress == this->nativeLibraryEntry->addr)
            {LOGMEIN("JavascriptStackWalker.cpp] 912\n");
                this->isNativeLibraryFrame = true;
                this->shouldDetectPartiallyInitializedInterpreterFrame = false;
                this->prevNativeLibraryEntry = this->nativeLibraryEntry; // Saves match in prevNativeLibraryEntry
                this->nativeLibraryEntry = this->nativeLibraryEntry->next;
                return true;
            }
        }

#if ENABLE_NATIVE_CODEGEN
        BOOL isNativeAddr = JavascriptFunction::IsNativeAddress(this->scriptContext, codeAddr);
        if (isNativeAddr)
        {LOGMEIN("JavascriptStackWalker.cpp] 924\n");
            this->shouldDetectPartiallyInitializedInterpreterFrame = false;
            void ** argv = this->currentFrame.GetArgv(true /*isCurrentContextNative*/, false /*shouldCheckForNativeAddr*/);
            if (argv == nullptr)
            {LOGMEIN("JavascriptStackWalker.cpp] 928\n");
                // NOTE: When we switch to walking the stack ourselves and skip non engine frames, this should never happen.
                return false;
            }

            ScriptFunction* funcObj = Js::ScriptFunction::FromVar(argv[JavascriptFunctionArgIndex_Function]);
            if (funcObj->GetFunctionBody()->GetIsAsmjsMode())
            {LOGMEIN("JavascriptStackWalker.cpp] 935\n");
                return false;
            }

            // Note: this check has to happen after asm.js check, because call info is not valid for asm.js
            if (((CallInfo const *)&argv[JavascriptFunctionArgIndex_CallInfo])->Flags & CallFlags_InternalFrame)
            {LOGMEIN("JavascriptStackWalker.cpp] 941\n");
                if (includeInlineFrames &&
                    InlinedFrameWalker::FromPhysicalFrame(inlinedFrameWalker, currentFrame, Js::ScriptFunction::FromVar(argv[JavascriptFunctionArgIndex_Function]),
                        false /*fromBailout*/, this->tempInterpreterFrame->GetCurrentLoopNum(), this, false /*useInternalFrameInfo*/))
                {LOGMEIN("JavascriptStackWalker.cpp] 945\n");
                    // Found inlined frames in a jitted loop body. We dont want to skip the inlined frames; walk all of them before setting codeAddress on lastInternalFrameInfo.
                    this->inlinedFramesBeingWalked = inlinedFrameWalker.Next(inlinedFrameCallInfo);
                    this->hasInlinedFramesOnStack = true;
                    Assert(inlinedFramesBeingWalked);
                    return true;
                }

                SetCachedInternalFrameInfo(InternalFrameType_LoopBody, funcObj, false /*hasInlinedFramesOnStack*/, previousInterpreterFrameIsFromBailout);
                return false;
            }

            return true;
        }
#endif
        return false;
    }

    void * JavascriptStackWalker::GetCurrentCodeAddr() const
    {LOGMEIN("JavascriptStackWalker.cpp] 964\n");
        return this->currentFrame.GetInstructionPointer();
    }

    JavascriptFunction * JavascriptStackWalker::GetCurrentFunction(bool includeInlinedFrames /* = true */) const
    {LOGMEIN("JavascriptStackWalker.cpp] 969\n");
        Assert(this->IsJavascriptFrame());

#if ENABLE_NATIVE_CODEGEN
        if (includeInlinedFrames && inlinedFramesBeingWalked)
        {LOGMEIN("JavascriptStackWalker.cpp] 974\n");
            return inlinedFrameWalker.GetFunctionObject();
        }
        else
#endif
            if (this->isNativeLibraryFrame)
        {LOGMEIN("JavascriptStackWalker.cpp] 980\n");
            // Return saved function. Do not read from stack as compiler may stackpack/optimize args.
            return JavascriptFunction::FromVar(this->prevNativeLibraryEntry->function);
        }
        else
        {
            return StackScriptFunction::GetCurrentFunctionObject((JavascriptFunction *)this->GetCurrentArgv()[JavascriptFunctionArgIndex_Function]);
        }
    }

    void JavascriptStackWalker::SetCurrentFunction(JavascriptFunction * function)
    {LOGMEIN("JavascriptStackWalker.cpp] 991\n");
        Assert(this->IsJavascriptFrame());
#if ENABLE_NATIVE_CODEGEN
        if (inlinedFramesBeingWalked)
        {LOGMEIN("JavascriptStackWalker.cpp] 995\n");
            inlinedFrameWalker.SetFunctionObject(function);
        }
        else
#endif
        {
            this->GetCurrentArgv()[JavascriptFunctionArgIndex_Function] = function;
        }
    }

    JavascriptFunction *JavascriptStackWalker::GetCurrentFunctionFromPhysicalFrame() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1006\n");
        return GetCurrentFunction(false);
    }

    CallInfo const * JavascriptStackWalker::GetCallInfo(bool includeInlinedFrames /* = true */) const
    {LOGMEIN("JavascriptStackWalker.cpp] 1011\n");
        Assert(this->IsJavascriptFrame());
        if (includeInlinedFrames && inlinedFramesBeingWalked)
        {LOGMEIN("JavascriptStackWalker.cpp] 1014\n");
            // Since we don't support inlining constructors yet, its questionable if we should handle the
            // hidden frame display here?
            return (CallInfo const *)&inlinedFrameCallInfo;
        }
        else if (this->GetCurrentFunction()->GetFunctionInfo()->IsCoroutine())
        {LOGMEIN("JavascriptStackWalker.cpp] 1020\n");
            JavascriptGenerator* gen = JavascriptGenerator::FromVar(this->GetCurrentArgv()[JavascriptFunctionArgIndex_This]);
            return &gen->GetArguments().Info;
        }
        else if (this->isNativeLibraryFrame)
        {LOGMEIN("JavascriptStackWalker.cpp] 1025\n");
            // Return saved callInfo. Do not read from stack as compiler may stackpack/optimize args.
            return &this->prevNativeLibraryEntry->callInfo;
        }
        else
        {
            return (CallInfo const *)&this->GetCurrentArgv()[JavascriptFunctionArgIndex_CallInfo];
        }
    }

    CallInfo const *JavascriptStackWalker::GetCallInfoFromPhysicalFrame() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1036\n");
        return GetCallInfo(false);
    }

    Var JavascriptStackWalker::GetThisFromFrame() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1041\n");
        Assert(!inlinedFramesBeingWalked);
        Assert(this->IsJavascriptFrame());

        if (this->GetCurrentFunction()->GetFunctionInfo()->IsCoroutine())
        {LOGMEIN("JavascriptStackWalker.cpp] 1046\n");
            JavascriptGenerator* gen = JavascriptGenerator::FromVar(this->GetCurrentArgv()[JavascriptFunctionArgIndex_This]);
            return gen->GetArguments()[0];
        }

        return this->GetCurrentArgv()[JavascriptFunctionArgIndex_This];
    }

#if ENABLE_NATIVE_CODEGEN
    void JavascriptStackWalker::ClearCachedInternalFrameInfo()
    {LOGMEIN("JavascriptStackWalker.cpp] 1056\n");
        this->lastInternalFrameInfo.Clear();
    }

    void JavascriptStackWalker::SetCachedInternalFrameInfo(InternalFrameType frameType, JavascriptFunction* function, bool hasInlinedFramesOnStack, bool previousInterpreterFrameIsFromBailout)
    {LOGMEIN("JavascriptStackWalker.cpp] 1061\n");
        if (!this->lastInternalFrameInfo.codeAddress)
        {LOGMEIN("JavascriptStackWalker.cpp] 1063\n");
            this->lastInternalFrameInfo.Set(
                this->GetCurrentCodeAddr(),
                this->currentFrame.GetFrame(),
                this->currentFrame.GetStackCheckCodeHeight(),
                frameType,
                function,
                hasInlinedFramesOnStack,
                previousInterpreterFrameIsFromBailout);
        }
    }
#endif

    bool JavascriptStackWalker::IsCurrentPhysicalFrameForLoopBody() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1077\n");
        return !!(this->GetCallInfoFromPhysicalFrame()->Flags & CallFlags_InternalFrame);
    }

    bool JavascriptStackWalker::IsWalkable(ScriptContext *scriptContext)
    {LOGMEIN("JavascriptStackWalker.cpp] 1082\n");
        if (scriptContext == NULL)
        {LOGMEIN("JavascriptStackWalker.cpp] 1084\n");
            return false;
        }

        ThreadContext *threadContext = scriptContext->GetThreadContext();
        if (threadContext == NULL)
        {LOGMEIN("JavascriptStackWalker.cpp] 1090\n");
            return false;
        }

        return (threadContext->GetScriptEntryExit() != NULL);
    }

    BOOL JavascriptStackWalker::GetCaller(JavascriptFunction** ppFunc, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptStackWalker.cpp] 1098\n");
        if (!IsWalkable(scriptContext))
        {LOGMEIN("JavascriptStackWalker.cpp] 1100\n");
            *ppFunc = nullptr;
            return FALSE;
        }

        JavascriptStackWalker walker(scriptContext);
        return walker.GetCaller(ppFunc);
    }

    BOOL JavascriptStackWalker::GetCaller(JavascriptFunction** ppFunc, uint32* byteCodeOffset, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptStackWalker.cpp] 1110\n");
        JavascriptStackWalker walker(scriptContext);
        if (walker.GetCaller(ppFunc))
        {LOGMEIN("JavascriptStackWalker.cpp] 1113\n");
            *byteCodeOffset = walker.GetByteCodeOffset();
            return TRUE;
        }
        return FALSE;
    }

    bool JavascriptStackWalker::GetThis(Var* pThis, int moduleId, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptStackWalker.cpp] 1121\n");
        JavascriptStackWalker walker(scriptContext);
        JavascriptFunction* caller;
        return walker.GetCaller(&caller) && walker.GetThis(pThis, moduleId);
    }

    bool JavascriptStackWalker::GetThis(Var* pThis, int moduleId, JavascriptFunction* func, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptStackWalker.cpp] 1128\n");
        JavascriptStackWalker walker(scriptContext);
        JavascriptFunction* caller;
        while (walker.GetCaller(&caller))
        {LOGMEIN("JavascriptStackWalker.cpp] 1132\n");
            if (caller == func)
            {LOGMEIN("JavascriptStackWalker.cpp] 1134\n");
                walker.GetThis(pThis, moduleId);
                return true;
            }
        }
        return false;
    }

    // Try to see whether there is a top-most javascript frame, and if there is return true if it's native.
    // Returns true if top most frame is javascript frame, in this case the isNative parameter receives true
    // when top-most frame is native, false otherwise.
    // Returns false if top most frame is not a JavaScript frame.

    /* static */
    bool JavascriptStackWalker::TryIsTopJavaScriptFrameNative(ScriptContext* scriptContext, bool* isNative, bool ignoreLibraryCode /* = false */)
    {LOGMEIN("JavascriptStackWalker.cpp] 1149\n");
        Assert(scriptContext);
        Assert(isNative);

        Js::JavascriptFunction* caller;
        Js::JavascriptStackWalker walker(scriptContext);

        BOOL isSuccess;
        if (ignoreLibraryCode)
        {LOGMEIN("JavascriptStackWalker.cpp] 1158\n");
            isSuccess = walker.GetNonLibraryCodeCaller(&caller);
        }
        else
        {
            isSuccess = walker.GetCaller(&caller);
        }

        if (isSuccess)
        {LOGMEIN("JavascriptStackWalker.cpp] 1167\n");
            *isNative = (walker.GetCurrentInterpreterFrame() == NULL);
            return true;
        }
        return false;
    }

#if ENABLE_NATIVE_CODEGEN
    bool InlinedFrameWalker::FromPhysicalFrame(InlinedFrameWalker& self, StackFrame& physicalFrame, Js::ScriptFunction *parent, bool fromBailout,
        int loopNum, const JavascriptStackWalker * const stackWalker, bool useInternalFrameInfo, bool noAlloc)
    {LOGMEIN("JavascriptStackWalker.cpp] 1177\n");
        bool inlinedFramesFound = false;
        FunctionBody* parentFunctionBody = parent->GetFunctionBody();
        EntryPointInfo *entryPointInfo;

        if (loopNum != -1)
        {LOGMEIN("JavascriptStackWalker.cpp] 1183\n");
            Assert(stackWalker);
        }

        void *nativeCodeAddress = nullptr;
        void *framePointer = nullptr;
        if (loopNum != -1 && useInternalFrameInfo)
        {LOGMEIN("JavascriptStackWalker.cpp] 1190\n");
            Assert(stackWalker->GetCachedInternalFrameInfo().codeAddress != nullptr);
            InternalFrameInfo lastInternalFrameInfo = stackWalker->GetCachedInternalFrameInfo();

            nativeCodeAddress = lastInternalFrameInfo.codeAddress;
            framePointer = lastInternalFrameInfo.framePointer;
        }
        else
        {
            nativeCodeAddress = physicalFrame.GetInstructionPointer();
            framePointer = physicalFrame.GetFrame();
        }

        if (loopNum != -1)
        {LOGMEIN("JavascriptStackWalker.cpp] 1204\n");
            entryPointInfo = (Js::EntryPointInfo*)parentFunctionBody->GetLoopEntryPointInfoFromNativeAddress((DWORD_PTR)nativeCodeAddress, loopNum);
        }
        else
        {
            entryPointInfo = (Js::EntryPointInfo*)parentFunctionBody->GetEntryPointFromNativeAddress((DWORD_PTR)nativeCodeAddress);
        }

        AssertMsg(entryPointInfo != nullptr, "Inlined frame should resolve to the right parent address");
        if (entryPointInfo->HasInlinees())
        {LOGMEIN("JavascriptStackWalker.cpp] 1214\n");
            void *entry = reinterpret_cast<void*>(entryPointInfo->GetNativeAddress());
            InlinedFrameWalker::InlinedFrame *outerMostFrame = InlinedFrame::FromPhysicalFrame(physicalFrame, stackWalker, entry, entryPointInfo, useInternalFrameInfo);

            if (!outerMostFrame)
            {LOGMEIN("JavascriptStackWalker.cpp] 1219\n");
                return inlinedFramesFound;
            }

            if (!fromBailout)
            {LOGMEIN("JavascriptStackWalker.cpp] 1224\n");
                InlineeFrameRecord* record = entryPointInfo->FindInlineeFrame((void*)nativeCodeAddress);

                if (record)
                {LOGMEIN("JavascriptStackWalker.cpp] 1228\n");
                    record->RestoreFrames(parent->GetFunctionBody(), outerMostFrame, JavascriptCallStackLayout::FromFramePointer(framePointer));
                }
            }

            if (outerMostFrame->callInfo.Count)
            {LOGMEIN("JavascriptStackWalker.cpp] 1234\n");
                inlinedFramesFound = true;
                if (noAlloc)
                {LOGMEIN("JavascriptStackWalker.cpp] 1237\n");
                    return inlinedFramesFound;
                }
                int32 frameCount = 0;
                InlinedFrameWalker::InlinedFrame *frameIterator = outerMostFrame;
                while (frameIterator->callInfo.Count)
                {LOGMEIN("JavascriptStackWalker.cpp] 1243\n");
                    frameCount++;
                    frameIterator = frameIterator->Next();
                }

                InlinedFrameWalker::InlinedFrame **frames = HeapNewArray(InlinedFrameWalker::InlinedFrame*, frameCount);

                frameIterator = outerMostFrame;
                for (int index = frameCount - 1; index >= 0; index--)
                {LOGMEIN("JavascriptStackWalker.cpp] 1252\n");
                    Assert(frameIterator);
                    frames[index] = frameIterator;
                    frameIterator = frameIterator->Next();
                }

                self.Initialize(frameCount, frames, parent);
            }

        }

        return inlinedFramesFound;
    }

    void InlinedFrameWalker::Close()
    {LOGMEIN("JavascriptStackWalker.cpp] 1267\n");
        parentFunction = nullptr;
        HeapDeleteArray(frameCount, frames);
        frames = nullptr;
        currentIndex = -1;
        frameCount = 0;
    }

    bool InlinedFrameWalker::Next(CallInfo& callInfo)
    {LOGMEIN("JavascriptStackWalker.cpp] 1276\n");
        MoveNext();
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        if (currentFrame)
        {LOGMEIN("JavascriptStackWalker.cpp] 1280\n");
            callInfo.Flags = CallFlags_None;
            callInfo.Count = (currentFrame->callInfo.Count & 0xFFFF);
        }

        return currentFrame != nullptr;
    }

    size_t InlinedFrameWalker::GetArgc() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1289\n");
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        return currentFrame->callInfo.Count;
    }

    Js::Var *InlinedFrameWalker::GetArgv(bool includeThis /* = true */) const
    {LOGMEIN("JavascriptStackWalker.cpp] 1297\n");
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        uint firstArg = includeThis ? InlinedFrameArgIndex_This : InlinedFrameArgIndex_SecondScriptArg;
        Js::Var *args = &currentFrame->argv[firstArg];

        this->FinalizeStackValues(args, this->GetArgc() - firstArg);

        return args;
    }

    void InlinedFrameWalker::FinalizeStackValues(__in_ecount(argCount) Js::Var args[], size_t argCount) const
    {LOGMEIN("JavascriptStackWalker.cpp] 1310\n");
        ScriptContext *scriptContext = this->GetFunctionObject()->GetScriptContext();

        for (size_t i = 0; i < argCount; i++)
        {LOGMEIN("JavascriptStackWalker.cpp] 1314\n");
            args[i] = Js::JavascriptOperators::BoxStackInstance(args[i], scriptContext);
        }
    }

    Js::JavascriptFunction *InlinedFrameWalker::GetFunctionObject() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1320\n");
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        return StackScriptFunction::GetCurrentFunctionObject(currentFrame->function);
    }

    void InlinedFrameWalker::SetFunctionObject(Js::JavascriptFunction * function)
    {LOGMEIN("JavascriptStackWalker.cpp] 1328\n");
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        currentFrame->function = function;
    }

    Js::Var InlinedFrameWalker::GetArgumentsObject() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1336\n");
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        return currentFrame->arguments;
    }

    void InlinedFrameWalker::SetArgumentsObject(Js::Var arguments)
    {LOGMEIN("JavascriptStackWalker.cpp] 1344\n");
        InlinedFrameWalker::InlinedFrame *currentFrame = (InlinedFrameWalker::InlinedFrame *)GetCurrentFrame();
        Assert(currentFrame);

        currentFrame->arguments = arguments;
    }

    Js::Var InlinedFrameWalker::GetThisObject() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1352\n");
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        return currentFrame->argv[InlinedFrameArgIndex_This];
    }

    bool InlinedFrameWalker::IsCallerPhysicalFrame() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1360\n");
        return currentIndex == (frameCount - 1);
    }

    bool InlinedFrameWalker::IsTopMostFrame() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1365\n");
        return currentIndex == 0;
    }

    uint32 InlinedFrameWalker::GetCurrentInlineeOffset() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1370\n");
        Assert(!IsTopMostFrame());
        Assert(currentIndex);

        return GetFrameAtIndex(currentIndex - 1)->callInfo.InlineeStartOffset;
    }

    uint32 InlinedFrameWalker::GetBottomMostInlineeOffset() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1378\n");
        Assert(frameCount);

        return GetFrameAtIndex(frameCount - 1)->callInfo.InlineeStartOffset;
    }

    Js::JavascriptFunction *InlinedFrameWalker::GetBottomMostFunctionObject() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1385\n");
        Assert(frameCount);

        return GetFrameAtIndex(frameCount - 1)->function;
    }

    InlinedFrameWalker::InlinedFrame *const InlinedFrameWalker::GetCurrentFrame() const
    {LOGMEIN("JavascriptStackWalker.cpp] 1392\n");
        return GetFrameAtIndex(currentIndex);
    }

    InlinedFrameWalker::InlinedFrame *const InlinedFrameWalker::GetFrameAtIndex(signed index) const
    {LOGMEIN("JavascriptStackWalker.cpp] 1397\n");
        Assert(frames);
        Assert(frameCount);

        InlinedFrameWalker::InlinedFrame *frame = nullptr;
        if (index < frameCount)
        {LOGMEIN("JavascriptStackWalker.cpp] 1403\n");
            frame = frames[index];
        }

        return frame;
    }

    void InlinedFrameWalker::MoveNext()
    {LOGMEIN("JavascriptStackWalker.cpp] 1411\n");
        currentIndex++;
    }

    void InlinedFrameWalker::Initialize(int32 frameCount, __in_ecount(frameCount) InlinedFrame **frames, Js::ScriptFunction *parent)
    {LOGMEIN("JavascriptStackWalker.cpp] 1416\n");
        Assert(!parentFunction);
        Assert(!this->frames);
        Assert(!this->frameCount);
        Assert(currentIndex == -1);

        this->parentFunction = parent;
        this->frames         = frames;
        this->frameCount     = frameCount;
        this->currentIndex   = -1;
    }

    InlinedFrameWalker::InlinedFrame* InlinedFrameWalker::InlinedFrame::FromPhysicalFrame(StackFrame& currentFrame, const JavascriptStackWalker * const stackWalker, void *entry, EntryPointInfo* entryPointInfo, bool useInternalFrameInfo)
    {LOGMEIN("JavascriptStackWalker.cpp] 1429\n");
        // If the current javascript frame is a native frame, get the inlined frame from it, otherwise
        // it may be possible that current frame is the interpreter frame for a jitted loop body
        // If the loop body had some inlinees in it, retrieve the inlined frame using the cached info,
        // viz. instruction pointer, frame pointer, and stackCheckCodeHeight, about the loop body frame.
        struct InlinedFrame *inlinedFrame = nullptr;
        void *codeAddr, *framePointer;
        size_t stackCheckCodeHeight;

        if (useInternalFrameInfo)
        {LOGMEIN("JavascriptStackWalker.cpp] 1439\n");
            codeAddr = stackWalker->GetCachedInternalFrameInfo().codeAddress;
            framePointer = stackWalker->GetCachedInternalFrameInfo().framePointer;
            stackCheckCodeHeight = stackWalker->GetCachedInternalFrameInfo().stackCheckCodeHeight;
        }
        else
        {
            codeAddr = currentFrame.GetInstructionPointer();
            framePointer = currentFrame.GetFrame();
            stackCheckCodeHeight = currentFrame.GetStackCheckCodeHeight();
        }

        if (!StackFrame::IsInStackCheckCode(entry, codeAddr, stackCheckCodeHeight))
        {LOGMEIN("JavascriptStackWalker.cpp] 1452\n");
            inlinedFrame = (struct InlinedFrame *)(((uint8 *)framePointer) - entryPointInfo->frameHeight);
        }

        return inlinedFrame;
    }

    void InternalFrameInfo::Set(
        void *codeAddress,
        void *framePointer,
        size_t stackCheckCodeHeight,
        InternalFrameType frameType,
        JavascriptFunction* function,
        bool hasInlinedFramesOnStack,
        bool previousInterpreterFrameIsFromBailout)
    {LOGMEIN("JavascriptStackWalker.cpp] 1467\n");
        // We skip a jitted loop body's native frame when walking the stack and refer to the loop body's interpreter frame to get the function.
        // However, if the loop body has inlinees, to retrieve inlinee frames we need to cache some info about the loop body's native frame.
        this->codeAddress = codeAddress;
        this->framePointer = framePointer;
        this->stackCheckCodeHeight = stackCheckCodeHeight;
        this->frameType = frameType;
        this->function = function;
        this->hasInlinedFramesOnStack = hasInlinedFramesOnStack;
        this->previousInterpreterFrameIsFromBailout = previousInterpreterFrameIsFromBailout;
    }

    void InternalFrameInfo::Clear()
    {LOGMEIN("JavascriptStackWalker.cpp] 1480\n");
        this->codeAddress = nullptr;
        this->framePointer = nullptr;
        this->stackCheckCodeHeight = (uint)-1;
        this->frameType = InternalFrameType_None;
        this->function = nullptr;
        this->hasInlinedFramesOnStack = false;
        this->previousInterpreterFrameIsFromBailout = false;
    }
#endif

#if DBG
    // Force a stack walk which till we find an interpreter frame
    // This will ensure inlined frames are decoded.
    bool JavascriptStackWalker::ValidateTopJitFrame(Js::ScriptContext* scriptContext)
    {LOGMEIN("JavascriptStackWalker.cpp] 1495\n");
        if (!Configuration::Global.flags.ValidateInlineStack)
        {LOGMEIN("JavascriptStackWalker.cpp] 1497\n");
            return true;
        }
        Js::JavascriptStackWalker walker(scriptContext);
        Js::JavascriptFunction* function;
        while (walker.GetCaller(&function))
        {LOGMEIN("JavascriptStackWalker.cpp] 1503\n");
            Assert(function);
            if (walker.GetCurrentInterpreterFrame() != nullptr)
            {LOGMEIN("JavascriptStackWalker.cpp] 1506\n");
                break;
            }
        }
        // If no asserts have fired yet - we should have succeeded.
        return true;
    }
#endif
}
