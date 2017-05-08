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
    {TRACE_IT(51572);
        return (Js::ArgumentsObject *)((void **)this)[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    Js::Var* JavascriptCallStackLayout::GetArgumentsObjectLocation() const
    {TRACE_IT(51573);
        return (Js::Var *)&((void **)this)[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    void JavascriptCallStackLayout::SetArgumentsObject(Js::ArgumentsObject * obj)
    {TRACE_IT(51574);
        ((void **)this)[JavascriptFunctionArgIndex_ArgumentsObject] =  obj;
    }

    Js::Var JavascriptCallStackLayout::GetOffset(int offset) const
    {TRACE_IT(51575);
        Js::Var *varPtr = (Js::Var *)(((char *)this) + offset);
        Assert(FAligned(varPtr, Js::Var));
        return *varPtr;
    }
    double JavascriptCallStackLayout::GetDoubleAtOffset(int offset) const
    {TRACE_IT(51576);
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
    {TRACE_IT(51577);
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
    {TRACE_IT(51578);
        return  *((SIMDValue *)(((char *)this) + offset));
    }

    char * JavascriptCallStackLayout::GetValueChangeOffset(int offset) const
    {TRACE_IT(51579);
        Js::Var *varPtr = (Js::Var *)(((char *)this) + offset);
        Assert(FAligned(varPtr, Js::Var));
        return (char *)varPtr;
    }

    ForInObjectEnumerator * JavascriptCallStackLayout::GetForInObjectEnumeratorArrayAtOffset(int offset) const
    {TRACE_IT(51580);
        return (ForInObjectEnumerator *)(((char *)this) + offset);
    }

    JavascriptCallStackLayout *JavascriptCallStackLayout::FromFramePointer(void *const framePointer)
    {TRACE_IT(51581);
        return
            reinterpret_cast<JavascriptCallStackLayout *>(
                static_cast<void **>(framePointer) + (JavascriptFunctionArgIndex_Function - JavascriptFunctionArgIndex_Frame));
    }


    void* const JavascriptCallStackLayout::ToFramePointer(JavascriptCallStackLayout* callstackLayout)
    {TRACE_IT(51582);
        return
            reinterpret_cast<void * const>(
                reinterpret_cast<void **>(callstackLayout) - (JavascriptFunctionArgIndex_Function - JavascriptFunctionArgIndex_Frame));
    }

    Js::Var* JavascriptCallStackLayout::GetArgv() const
    {TRACE_IT(51583);
        return const_cast<Js::Var*>(&this->args[0]);
    }

    ScriptContext* JavascriptStackWalker::GetCurrentScriptContext() const
    {TRACE_IT(51584);
        return this->GetCurrentInterpreterFrame() ? this->GetCurrentInterpreterFrame()->GetScriptContext() : this->scriptContext;
    }

    Var JavascriptStackWalker::GetCurrentArgumentsObject() const
    {TRACE_IT(51585);
#if ENABLE_PROFILE_INFO
        if (interpreterFrame)
#else
        Assert(interpreterFrame);
#endif
        {TRACE_IT(51586);
            return interpreterFrame->GetArgumentsObject();
        }
#if ENABLE_NATIVE_CODEGEN
        else
        {TRACE_IT(51587);
            if (inlinedFramesBeingWalked)
            {TRACE_IT(51588);
                return inlinedFrameWalker.GetArgumentsObject();
            }
            else
            {TRACE_IT(51589);
                return this->GetCurrentNativeArgumentsObject();
            }
        }
#endif
    }

    void JavascriptStackWalker::SetCurrentArgumentsObject(Var args)
    {TRACE_IT(51590);
#if ENABLE_NATIVE_CODEGEN
        if (interpreterFrame)
#else
        Assert(interpreterFrame);
#endif
        {TRACE_IT(51591);
            interpreterFrame->SetArgumentsObject(args);
        }
#if ENABLE_NATIVE_CODEGEN
        else
        {TRACE_IT(51592);
            if (inlinedFramesBeingWalked)
            {TRACE_IT(51593);
                inlinedFrameWalker.SetArgumentsObject(args);
            }
            else
            {TRACE_IT(51594);
                this->SetCurrentNativeArgumentsObject(args);
            }
        }
#endif
    }

    Var JavascriptStackWalker::GetPermanentArguments() const
    {TRACE_IT(51595);
        Assert(IsJavascriptFrame());
        AssertMsg(this->GetCurrentFunction()->IsScriptFunction(), "GetPermanentArguments should not be called for non-script function as there is no slot allocated for it.");

        const uint32 paramCount = GetCallInfo().Count;
        if (paramCount == 0)
        {TRACE_IT(51596);
            // glob function doesn't allocate ArgumentsObject slot on stack
            return nullptr;
        }

        // Get the heap-allocated args for this frame.
        Var args = this->GetCurrentArgumentsObject();
        if (args && ArgumentsObject::Is(args))
        {TRACE_IT(51597);
            args = ((ArgumentsObject*)args)->GetHeapArguments();
        }
        return args;
    }

    BOOL JavascriptStackWalker::WalkToArgumentsFrame(ArgumentsObject *args)
    {TRACE_IT(51598);
        // Move the walker up the stack until we find the given arguments object on the frame.
        while (this->Walk(/*includeInlineFrame*/ true))
        {TRACE_IT(51599);
            if (this->IsJavascriptFrame())
            {TRACE_IT(51600);
                Var currArgs = this->GetCurrentArgumentsObject();
                if (currArgs == args)
                {TRACE_IT(51601);
                    return TRUE;
                }
            }
        }
        return FALSE;
    }

    bool JavascriptStackWalker::GetThis(Var* pVarThis, int moduleId) const
    {TRACE_IT(51602);
#if ENABLE_NATIVE_CODEGEN
        if (inlinedFramesBeingWalked)
        {TRACE_IT(51603);
            if (inlinedFrameWalker.GetArgc() == 0)
            {TRACE_IT(51604);
                *pVarThis = JavascriptOperators::OP_GetThis(this->scriptContext->GetLibrary()->GetUndefined(), moduleId, scriptContext);
                return false;
            }

            *pVarThis = inlinedFrameWalker.GetThisObject();
            Assert(*pVarThis);

            return true;
        }
        else
#endif
        {TRACE_IT(51605);
            const CallInfo callInfo = this->GetCallInfo();
            if (callInfo.Count == 0)
            {TRACE_IT(51606);
                *pVarThis = JavascriptOperators::OP_GetThis(scriptContext->GetLibrary()->GetUndefined(), moduleId, scriptContext);
                return false;
            }

            *pVarThis = this->GetThisFromFrame();
            return (*pVarThis) != nullptr;
        }
    }

    BOOL IsEval(CallInfo callInfo)
    {TRACE_IT(51607);
        return (callInfo.Flags & CallFlags_Eval) != 0;
    }

    BOOL JavascriptStackWalker::IsCallerGlobalFunction() const
    {TRACE_IT(51608);
        const CallInfo callInfo = this->GetCallInfo();

        JavascriptFunction* function = this->GetCurrentFunction();
        if (IsLibraryStackFrameEnabled(this->scriptContext) && !function->IsScriptFunction())
        {TRACE_IT(51609);
            return false; // native library code can't be global function
        }

        FunctionInfo* funcInfo = function->GetFunctionInfo();
        if (funcInfo->HasParseableInfo())
        {TRACE_IT(51610);
            return funcInfo->GetParseableFunctionInfo()->GetIsGlobalFunc() || IsEval(callInfo);
        }
        else
        {
            AssertMsg(FALSE, "Here we should only have script functions which were already parsed/deserialized.");
            return callInfo.Count == 0 || IsEval(callInfo);
        }
    }

    BOOL JavascriptStackWalker::IsEvalCaller() const
    {TRACE_IT(51611);
        const CallInfo callInfo = this->GetCallInfo();
        return (callInfo.Flags & CallFlags_Eval) != 0;
    }

    Var JavascriptStackWalker::GetCurrentNativeArgumentsObject() const
    {TRACE_IT(51612);
        Assert(this->IsJavascriptFrame() && this->interpreterFrame == nullptr);
        return this->GetCurrentArgv()[JavascriptFunctionArgIndex_ArgumentsObject];
    }

    void JavascriptStackWalker::SetCurrentNativeArgumentsObject(Var args)
    {TRACE_IT(51613);
        Assert(this->IsJavascriptFrame() && this->interpreterFrame == nullptr);
        this->GetCurrentArgv()[JavascriptFunctionArgIndex_ArgumentsObject] = args;
    }

    Js::Var * JavascriptStackWalker::GetJavascriptArgs() const
    {TRACE_IT(51614);
        Assert(this->IsJavascriptFrame());

#if ENABLE_NATIVE_CODEGEN
        if (inlinedFramesBeingWalked)
        {TRACE_IT(51615);
            return inlinedFrameWalker.GetArgv(/* includeThis = */ false);
        }
        else
#endif
            if (this->GetCurrentFunction()->GetFunctionInfo()->IsCoroutine())
        {TRACE_IT(51616);
            JavascriptGenerator* gen = JavascriptGenerator::FromVar(this->GetCurrentArgv()[JavascriptFunctionArgIndex_This]);
            return gen->GetArguments().Values;
        }
        else
        {TRACE_IT(51617);
            return &this->GetCurrentArgv()[JavascriptFunctionArgIndex_SecondScriptArg];
        }
    }

    uint32 JavascriptStackWalker::GetByteCodeOffset() const
    {TRACE_IT(51618);
        uint32 offset = 0;
        if (this->IsJavascriptFrame())
        {TRACE_IT(51619);
            if (this->interpreterFrame)
            {TRACE_IT(51620);
                if (this->TryGetByteCodeOffsetFromInterpreterFrame(offset))
                {TRACE_IT(51621);
                    return offset;
                }
            }

#if ENABLE_NATIVE_CODEGEN
            if (TryGetByteCodeOffsetFromNativeFrame(offset))
            {TRACE_IT(51622);
                return offset;
            }
#endif
        }
        return offset;
    }

    bool JavascriptStackWalker::TryGetByteCodeOffsetFromInterpreterFrame(uint32& offset) const
    {TRACE_IT(51623);
#if ENABLE_NATIVE_CODEGEN
        if (this->lastInternalFrameInfo.codeAddress != nullptr)
        {TRACE_IT(51624);
            return false;
        }
#endif
        offset = this->interpreterFrame->GetReader()->GetCurrentOffset();
        if (offset == 0)
        {TRACE_IT(51625);
            // This will be the case when we are broken on the debugger on very first statement (due to async break).
            // Or the interpreter loop can throw OOS on entrance before executing bytecode.
        }
        else
        {TRACE_IT(51626);
            // Note : For many cases, we move the m_currentLocation of ByteCodeReader already to next available opcode.
            // This could create problem in binding the exception to proper line offset.
            // Reducing by 1 will make sure the current offset falls under, current executing opcode.
            offset--;
        }
        return true;
    }

#if ENABLE_NATIVE_CODEGEN
    bool JavascriptStackWalker::TryGetByteCodeOffsetFromNativeFrame(uint32& offset) const
    {TRACE_IT(51627);
        DWORD_PTR pCodeAddr;
        if (this->lastInternalFrameInfo.codeAddress != nullptr)
        {TRACE_IT(51628);
            pCodeAddr = (DWORD_PTR)this->lastInternalFrameInfo.codeAddress;
        }
        else
        {TRACE_IT(51629);
            pCodeAddr = (DWORD_PTR)this->GetCurrentCodeAddr();
        }

        // If the current instruction's return address is the beginning of the next statement then we will show error for the next line, which would be completely wrong.
        // The quick fix would be to look the address which is at least lesser than current return address.

        // Assert to verify at what places this can happen.
        Assert(pCodeAddr);

        if (pCodeAddr)
        {TRACE_IT(51630);
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
        {TRACE_IT(51631);
            return true;
        }

        StatementData data;
        if (function->GetFunctionBody() && function->GetFunctionBody()->GetMatchingStatementMapFromNativeAddress(pCodeAddr, data, loopNum, inlinee))
        {TRACE_IT(51632);
            offset = data.bytecodeBegin;
            return true;
        }

        return false;
    }

    uint JavascriptStackWalker::GetLoopNumber(bool& usedInternalFrameInfo) const
    {TRACE_IT(51633);
        uint loopNum = LoopHeader::NoLoop;
        if (this->lastInternalFrameInfo.codeAddress != nullptr)
        {TRACE_IT(51634);
            if (this->lastInternalFrameInfo.frameType == InternalFrameType_LoopBody)
            {TRACE_IT(51635);
                AnalysisAssert(this->interpreterFrame);
                loopNum = this->interpreterFrame->GetCurrentLoopNum();
                Assert(loopNum != LoopHeader::NoLoop);
                usedInternalFrameInfo = true;
            }
        }
        else
        {TRACE_IT(51636);
            if (this->IsCurrentPhysicalFrameForLoopBody())
            {TRACE_IT(51637);
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
    {TRACE_IT(51638);
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
        {TRACE_IT(51639);
            // Inlined frames are being walked right now. The top most frame is where the IP is.
            if (!inlinedFrameWalker.IsTopMostFrame())
            {TRACE_IT(51640);
                inlineeOffset = inlinedFrameWalker.GetCurrentInlineeOffset();
            }
        }
        else if (ScriptFunction::Test(parentFunction) && HasInlinedFramesOnStack())
        {TRACE_IT(51641);
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
        {TRACE_IT(51642);
            offset = data.bytecodeBegin;
            return true;
        }

        return false;
    }

    bool JavascriptStackWalker::PreviousInterpreterFrameIsFromBailout() const
    {TRACE_IT(51643);
        if (lastInternalFrameInfo.codeAddress)
        {TRACE_IT(51644);
            return lastInternalFrameInfo.previousInterpreterFrameIsFromBailout;
        }

        return this->previousInterpreterFrameIsFromBailout;
    }

    bool JavascriptStackWalker::InlinedFramesBeingWalked() const
    {TRACE_IT(51645);
        if (lastInternalFrameInfo.codeAddress)
        {TRACE_IT(51646);
            return false;
        }

        return this->inlinedFramesBeingWalked;
    }

    bool JavascriptStackWalker::HasInlinedFramesOnStack() const
    {TRACE_IT(51647);
        if (lastInternalFrameInfo.codeAddress)
        {TRACE_IT(51648);
            return lastInternalFrameInfo.hasInlinedFramesOnStack;
        }

        return this->hasInlinedFramesOnStack;
    }
#endif

    bool JavascriptStackWalker::GetSourcePosition(const WCHAR** sourceFileName, ULONG* line, LONG* column)
    {TRACE_IT(51649);
        uint byteCodeoffset = this->GetByteCodeOffset();
        if(byteCodeoffset)
        {TRACE_IT(51650);
            Js::FunctionBody* functionBody = this->GetCurrentFunction()->GetFunctionBody();
            if (functionBody->GetLineCharOffset(byteCodeoffset, line, column))
            {TRACE_IT(51651);
                if(functionBody->GetUtf8SourceInfo()->IsDynamic())
                {TRACE_IT(51652);
                    *sourceFileName = _u("Dynamic Code");
                }
                else
                {TRACE_IT(51653);
                    *sourceFileName = functionBody->GetUtf8SourceInfo()->GetSrcInfo()->sourceContextInfo->url;
                }
                return true;
            }
        }
        return false;
    }

    Js::JavascriptFunction * JavascriptStackWalker::UpdateFrame(bool includeInlineFrames)
    {TRACE_IT(51654);
        this->isJavascriptFrame = this->CheckJavascriptFrame(includeInlineFrames);

        if (this->IsJavascriptFrame())
        {TRACE_IT(51655);
            // In case we have a cross site thunk, update the script context
            Js::JavascriptFunction *function = this->GetCurrentFunction();

#if ENABLE_NATIVE_CODEGEN
            bool isCurrentPhysicalFrameForLoopBody = this->IsCurrentPhysicalFrameForLoopBody();
#endif
            if (this->interpreterFrame)
            {TRACE_IT(51656);
#if ENABLE_NATIVE_CODEGEN
                if (lastInternalFrameInfo.codeAddress != nullptr)
                {TRACE_IT(51657);
                    this->previousInterpreterFrameIsForLoopBody = true;
                }
                else
#endif
                {TRACE_IT(51658);
                    this->previousInterpreterFrameIsForLoopBody = false;
                }

                // We might've bailed out of an inlinee, so check if there were any inlinees.
                if (this->interpreterFrame->GetFlags() & InterpreterStackFrameFlags_FromBailOut)
                {TRACE_IT(51659);
                    previousInterpreterFrameIsFromBailout = true;

#if ENABLE_NATIVE_CODEGEN
                    Assert(!inlinedFramesBeingWalked);
                    if (includeInlineFrames)
                    {TRACE_IT(51660);
                        int loopNum = -1;
                        if (isCurrentPhysicalFrameForLoopBody)
                        {TRACE_IT(51661);
                            loopNum = this->tempInterpreterFrame->GetCurrentLoopNum();
                        }

                        bool hasInlinedFramesOnStack = InlinedFrameWalker::FromPhysicalFrame(inlinedFrameWalker, currentFrame,
                            ScriptFunction::FromVar(function), true /*fromBailout*/, loopNum, this, false /*useInternalFrameInfo*/);
                        if (hasInlinedFramesOnStack)
                        {TRACE_IT(51662);
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
                        {TRACE_IT(51663);
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
                {TRACE_IT(51664);
                    Assert(StackScriptFunction::GetCurrentFunctionObject(this->interpreterFrame->GetJavascriptFunction()) == function);
                    previousInterpreterFrameIsFromBailout = false;
                }
            }
            else if (!this->isNativeLibraryFrame)
            {TRACE_IT(51665);
#if ENABLE_NATIVE_CODEGEN
                Assert(!HasInlinedFramesOnStack() || (includeInlineFrames && isCurrentPhysicalFrameForLoopBody));

                if (!HasInlinedFramesOnStack() && includeInlineFrames)
                {TRACE_IT(51666);
                    // Check whether there are inlined frames nested in this native frame. The corresponding check for
                    // a jitted loop body frame should have been done in CheckJavascriptFrame
                    Assert(lastInternalFrameInfo.codeAddress == nullptr);
                    if (InlinedFrameWalker::FromPhysicalFrame(inlinedFrameWalker, currentFrame, ScriptFunction::FromVar(function)))
                    {TRACE_IT(51667);
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
    {TRACE_IT(51668);
        if (scriptContext == NULL)
        {TRACE_IT(51669);
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
        {TRACE_IT(51670);
            // We're currently outside the script, so grab the frame from which we left.
            this->scriptContext = entryExitRecord->scriptContext;
            this->isInitialFrame = this->currentFrame.InitializeByFrameId(entryExitRecord->frameIdOfScriptExitFunction, this->scriptContext);
        }
        else
        {TRACE_IT(51671);
            // Just start with the caller
            this->isInitialFrame = this->currentFrame.InitializeByReturnAddress(_ReturnAddress(), this->scriptContext);
        }

        if (useEERContext)
        {TRACE_IT(51672);
            this->tempInterpreterFrame = this->scriptContext->GetThreadContext()->GetLeafInterpreterFrame();
        }
        else
        {TRACE_IT(51673);
            // We need to generate stack for the passed script context, so use the leaf interpreter frame for passed script context
            this->tempInterpreterFrame = scriptContext->GetThreadContext()->GetLeafInterpreterFrame();
        }

        inlinedFramesBeingWalked = false;
    }

    BOOL JavascriptStackWalker::Walk(bool includeInlineFrames)
    {TRACE_IT(51674);
        // Walk one frame up the call stack.
        this->interpreterFrame = NULL;

#if ENABLE_NATIVE_CODEGEN
        if (lastInternalFrameInfo.codeAddress != nullptr && this->previousInterpreterFrameIsForLoopBody)
        {TRACE_IT(51675);
            ClearCachedInternalFrameInfo();
        }

        if (inlinedFramesBeingWalked)
        {TRACE_IT(51676);
            Assert(includeInlineFrames);
            inlinedFramesBeingWalked = inlinedFrameWalker.Next(inlinedFrameCallInfo);
            if (!inlinedFramesBeingWalked)
            {TRACE_IT(51677);
                inlinedFrameWalker.Close();
                if ((this->IsCurrentPhysicalFrameForLoopBody()))
                {TRACE_IT(51678);
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
        {TRACE_IT(51679);
            this->isInitialFrame = false; // Only walk initial frame once
        }
        else if (!this->currentFrame.Next())
        {TRACE_IT(51680);
            this->isJavascriptFrame = false;
            return false;
        }

        // If we're at the entry from a host frame, hop to the frame from which we left the script.
        if (this->currentFrame.GetInstructionPointer() == this->entryExitRecord->returnAddrOfScriptEntryFunction)
        {TRACE_IT(51681);
            BOOL hasCaller = this->entryExitRecord->hasCaller || this->forceFullWalk;

#ifdef CHECK_STACKWALK_EXCEPTION
            BOOL ignoreStackWalkException = this->entryExitRecord->ignoreStackWalkException;
#endif

            this->entryExitRecord = this->entryExitRecord->next;
            if (this->entryExitRecord == NULL)
            {TRACE_IT(51682);
                this->isJavascriptFrame = false;
                return false;
            }

            if (!hasCaller && !this->scriptContext->IsDiagnosticsScriptContext())
            {TRACE_IT(51683);
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
    {TRACE_IT(51684);
        return GetCaller(ppFunc, /*includeInlineFrames*/ false);
    }

    BOOL JavascriptStackWalker::GetCaller(JavascriptFunction ** ppFunc, bool includeInlineFrames)
    {TRACE_IT(51685);
        while (this->Walk(includeInlineFrames))
        {TRACE_IT(51686);
            if (this->IsJavascriptFrame())
            {TRACE_IT(51687);
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
    {TRACE_IT(51688);
        while (this->GetCaller(ppFunc))
        {TRACE_IT(51689);
            if (!(*ppFunc)->IsLibraryCode())
            {TRACE_IT(51690);
                return true;
            }
        }
        return false;
    }

    /*static*/
    bool JavascriptStackWalker::IsLibraryStackFrameEnabled(Js::ScriptContext * scriptContext)
    {TRACE_IT(51691);
        Assert(scriptContext != nullptr);
        return CONFIG_FLAG(LibraryStackFrame);
    }

    // Check if a function is a display caller: user code, or native library / boundary script library code
    bool JavascriptStackWalker::IsDisplayCaller(JavascriptFunction* func)
    {TRACE_IT(51692);
        FunctionBody* body = func->GetFunctionBody();
        if (IsLibraryStackFrameEnabled(func->GetScriptContext()))
        {TRACE_IT(51693);
            return !func->IsScriptFunction() || !body->GetUtf8SourceInfo()->GetIsLibraryCode() || body->IsPublicLibraryCode();
        }
        else
        {TRACE_IT(51694);
            return !body->GetUtf8SourceInfo()->GetIsLibraryCode();
        }
    }

    bool JavascriptStackWalker::GetDisplayCaller(JavascriptFunction ** ppFunc)
    {TRACE_IT(51695);
        while (this->GetCaller(ppFunc))
        {TRACE_IT(51696);
            if (IsDisplayCaller(*ppFunc))
            {TRACE_IT(51697);
                return true;
            }
        }
        return false;
    }

    PCWSTR JavascriptStackWalker::GetCurrentNativeLibraryEntryName() const
    {TRACE_IT(51698);
        Assert(IsLibraryStackFrameEnabled(this->scriptContext)
            && this->prevNativeLibraryEntry
            && this->prevNativeLibraryEntry->next == this->nativeLibraryEntry);
        return this->prevNativeLibraryEntry->name;
    }

    // WalkToTarget skips internal frames
    BOOL JavascriptStackWalker::WalkToTarget(JavascriptFunction * funcTarget)
    {TRACE_IT(51699);
        // Walk up the call stack until we find the frame that belongs to the given function.

        while (this->Walk(/*includeInlineFrames*/ true))
        {TRACE_IT(51700);
            if (this->IsJavascriptFrame() && this->GetCurrentFunction() == funcTarget)
            {TRACE_IT(51701);
                // Skip internal names
                Assert( !(this->GetCallInfo().Flags & CallFlags_InternalFrame) );
                return true;
            }
        }

        return false;
    }

    void ** JavascriptStackWalker::GetCurrentArgv() const
    {TRACE_IT(51702);
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
    {TRACE_IT(51703);
        this->isNativeLibraryFrame = false; // Clear previous result

        void * codeAddr = this->currentFrame.GetInstructionPointer();
        if (this->tempInterpreterFrame && codeAddr == this->tempInterpreterFrame->GetReturnAddress())
        {TRACE_IT(51704);
            bool isBailoutInterpreter = (this->tempInterpreterFrame->GetFlags() & Js::InterpreterStackFrameFlags_FromBailOut) != 0;

            // We need to skip over the first interpreter frame on the stack if it is the partially initialized frame
            // otherwise it is a real frame and we should continue.
            // For fully initialized frames (PushPopHelper was called) the thunk stack addr is equal or below addressOfReturnAddress
            // as the latter one is obtained in InterpreterStackFrame::InterpreterThunk called by the thunk.
            bool isPartiallyInitializedFrame = this->shouldDetectPartiallyInitializedInterpreterFrame &&
                this->currentFrame.GetAddressOfReturnAddress(isBailoutInterpreter /*isCurrentContextNative*/, false /*shouldCheckForNativeAddr*/) < this->tempInterpreterFrame->GetAddressOfReturnAddress();
            this->shouldDetectPartiallyInitializedInterpreterFrame = false;

            if (isPartiallyInitializedFrame)
            {TRACE_IT(51705);
                return false; // Skip it.
            }

            void ** argv = this->currentFrame.GetArgv(isBailoutInterpreter /*isCurrentContextNative*/, false /*shouldCheckForNativeAddr*/);
            if (argv == nullptr)
            {TRACE_IT(51706);
                // NOTE: When we switch to walking the stack ourselves and skip non engine frames, this should never happen.
                return false;
            }

            this->interpreterFrame = this->tempInterpreterFrame;

            this->tempInterpreterFrame = this->interpreterFrame->GetPreviousFrame();

#if ENABLE_NATIVE_CODEGEN
#if DBG
            if (((CallInfo const *)&argv[JavascriptFunctionArgIndex_CallInfo])->Flags & CallFlags_InternalFrame)
            {TRACE_IT(51707);
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
        {TRACE_IT(51708);
            void* addressOfReturnAddress = this->currentFrame.GetAddressOfReturnAddress();
            AssertMsg(addressOfReturnAddress <= this->nativeLibraryEntry->addr, "Missed matching native library entry?");
            if (addressOfReturnAddress == this->nativeLibraryEntry->addr)
            {TRACE_IT(51709);
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
        {TRACE_IT(51710);
            this->shouldDetectPartiallyInitializedInterpreterFrame = false;
            void ** argv = this->currentFrame.GetArgv(true /*isCurrentContextNative*/, false /*shouldCheckForNativeAddr*/);
            if (argv == nullptr)
            {TRACE_IT(51711);
                // NOTE: When we switch to walking the stack ourselves and skip non engine frames, this should never happen.
                return false;
            }

            ScriptFunction* funcObj = Js::ScriptFunction::FromVar(argv[JavascriptFunctionArgIndex_Function]);
            if (funcObj->GetFunctionBody()->GetIsAsmjsMode())
            {TRACE_IT(51712);
                return false;
            }

            // Note: this check has to happen after asm.js check, because call info is not valid for asm.js
            if (((CallInfo const *)&argv[JavascriptFunctionArgIndex_CallInfo])->Flags & CallFlags_InternalFrame)
            {TRACE_IT(51713);
                if (includeInlineFrames &&
                    InlinedFrameWalker::FromPhysicalFrame(inlinedFrameWalker, currentFrame, Js::ScriptFunction::FromVar(argv[JavascriptFunctionArgIndex_Function]),
                        false /*fromBailout*/, this->tempInterpreterFrame->GetCurrentLoopNum(), this, false /*useInternalFrameInfo*/))
                {TRACE_IT(51714);
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
    {TRACE_IT(51715);
        return this->currentFrame.GetInstructionPointer();
    }

    JavascriptFunction * JavascriptStackWalker::GetCurrentFunction(bool includeInlinedFrames /* = true */) const
    {TRACE_IT(51716);
        Assert(this->IsJavascriptFrame());

#if ENABLE_NATIVE_CODEGEN
        if (includeInlinedFrames && inlinedFramesBeingWalked)
        {TRACE_IT(51717);
            return inlinedFrameWalker.GetFunctionObject();
        }
        else
#endif
            if (this->isNativeLibraryFrame)
        {TRACE_IT(51718);
            // Return saved function. Do not read from stack as compiler may stackpack/optimize args.
            return JavascriptFunction::FromVar(this->prevNativeLibraryEntry->function);
        }
        else
        {TRACE_IT(51719);
            return StackScriptFunction::GetCurrentFunctionObject((JavascriptFunction *)this->GetCurrentArgv()[JavascriptFunctionArgIndex_Function]);
        }
    }

    void JavascriptStackWalker::SetCurrentFunction(JavascriptFunction * function)
    {TRACE_IT(51720);
        Assert(this->IsJavascriptFrame());
#if ENABLE_NATIVE_CODEGEN
        if (inlinedFramesBeingWalked)
        {TRACE_IT(51721);
            inlinedFrameWalker.SetFunctionObject(function);
        }
        else
#endif
        {TRACE_IT(51722);
            this->GetCurrentArgv()[JavascriptFunctionArgIndex_Function] = function;
        }
    }

    JavascriptFunction *JavascriptStackWalker::GetCurrentFunctionFromPhysicalFrame() const
    {TRACE_IT(51723);
        return GetCurrentFunction(false);
    }

    CallInfo JavascriptStackWalker::GetCallInfo(bool includeInlinedFrames /* = true */) const
    {TRACE_IT(51724);
        Assert(this->IsJavascriptFrame());
        CallInfo callInfo;
        if (includeInlinedFrames && inlinedFramesBeingWalked)
        {TRACE_IT(51725);
            // Since we don't support inlining constructors yet, its questionable if we should handle the
            // hidden frame display here?
            callInfo = inlinedFrameCallInfo;
        }
        else if (this->GetCurrentFunction()->GetFunctionInfo()->IsCoroutine())
        {TRACE_IT(51726);
            JavascriptGenerator* gen = JavascriptGenerator::FromVar(this->GetCurrentArgv()[JavascriptFunctionArgIndex_This]);
            callInfo = gen->GetArguments().Info;
        }
        else if (this->isNativeLibraryFrame)
        {TRACE_IT(51727);
            // Return saved callInfo. Do not read from stack as compiler may stackpack/optimize args.
            callInfo = this->prevNativeLibraryEntry->callInfo;
        }
        else
        {TRACE_IT(51728);
            callInfo = *(CallInfo const *)&this->GetCurrentArgv()[JavascriptFunctionArgIndex_CallInfo];
        }

        if (callInfo.Flags & Js::CallFlags_ExtraArg)
        {TRACE_IT(51729);
            callInfo.Flags = (CallFlags)(callInfo.Flags & ~Js::CallFlags_ExtraArg);
            callInfo.Count--;
        }

        return callInfo;
    }

    CallInfo JavascriptStackWalker::GetCallInfoFromPhysicalFrame() const
    {TRACE_IT(51730);
        return GetCallInfo(false);
    }

    Var JavascriptStackWalker::GetThisFromFrame() const
    {TRACE_IT(51731);
        Assert(!inlinedFramesBeingWalked);
        Assert(this->IsJavascriptFrame());

        if (this->GetCurrentFunction()->GetFunctionInfo()->IsCoroutine())
        {TRACE_IT(51732);
            JavascriptGenerator* gen = JavascriptGenerator::FromVar(this->GetCurrentArgv()[JavascriptFunctionArgIndex_This]);
            return gen->GetArguments()[0];
        }

        return this->GetCurrentArgv()[JavascriptFunctionArgIndex_This];
    }

#if ENABLE_NATIVE_CODEGEN
    void JavascriptStackWalker::ClearCachedInternalFrameInfo()
    {TRACE_IT(51733);
        this->lastInternalFrameInfo.Clear();
    }

    void JavascriptStackWalker::SetCachedInternalFrameInfo(InternalFrameType frameType, JavascriptFunction* function, bool hasInlinedFramesOnStack, bool previousInterpreterFrameIsFromBailout)
    {TRACE_IT(51734);
        if (!this->lastInternalFrameInfo.codeAddress)
        {TRACE_IT(51735);
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
    {TRACE_IT(51736);
        return !!(this->GetCallInfoFromPhysicalFrame().Flags & CallFlags_InternalFrame);
    }

    bool JavascriptStackWalker::IsWalkable(ScriptContext *scriptContext)
    {TRACE_IT(51737);
        if (scriptContext == NULL)
        {TRACE_IT(51738);
            return false;
        }

        ThreadContext *threadContext = scriptContext->GetThreadContext();
        if (threadContext == NULL)
        {TRACE_IT(51739);
            return false;
        }

        return (threadContext->GetScriptEntryExit() != NULL);
    }

    BOOL JavascriptStackWalker::GetCaller(JavascriptFunction** ppFunc, ScriptContext* scriptContext)
    {TRACE_IT(51740);
        if (!IsWalkable(scriptContext))
        {TRACE_IT(51741);
            *ppFunc = nullptr;
            return FALSE;
        }

        JavascriptStackWalker walker(scriptContext);
        return walker.GetCaller(ppFunc);
    }

    BOOL JavascriptStackWalker::GetCaller(JavascriptFunction** ppFunc, uint32* byteCodeOffset, ScriptContext* scriptContext)
    {TRACE_IT(51742);
        JavascriptStackWalker walker(scriptContext);
        if (walker.GetCaller(ppFunc))
        {TRACE_IT(51743);
            *byteCodeOffset = walker.GetByteCodeOffset();
            return TRUE;
        }
        return FALSE;
    }

    bool JavascriptStackWalker::GetThis(Var* pThis, int moduleId, ScriptContext* scriptContext)
    {TRACE_IT(51744);
        JavascriptStackWalker walker(scriptContext);
        JavascriptFunction* caller;
        return walker.GetCaller(&caller) && walker.GetThis(pThis, moduleId);
    }

    bool JavascriptStackWalker::GetThis(Var* pThis, int moduleId, JavascriptFunction* func, ScriptContext* scriptContext)
    {TRACE_IT(51745);
        JavascriptStackWalker walker(scriptContext);
        JavascriptFunction* caller;
        while (walker.GetCaller(&caller))
        {TRACE_IT(51746);
            if (caller == func)
            {TRACE_IT(51747);
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
    {TRACE_IT(51748);
        Assert(scriptContext);
        Assert(isNative);

        Js::JavascriptFunction* caller;
        Js::JavascriptStackWalker walker(scriptContext);

        BOOL isSuccess;
        if (ignoreLibraryCode)
        {TRACE_IT(51749);
            isSuccess = walker.GetNonLibraryCodeCaller(&caller);
        }
        else
        {TRACE_IT(51750);
            isSuccess = walker.GetCaller(&caller);
        }

        if (isSuccess)
        {TRACE_IT(51751);
            *isNative = (walker.GetCurrentInterpreterFrame() == NULL);
            return true;
        }
        return false;
    }

#if ENABLE_NATIVE_CODEGEN
    bool InlinedFrameWalker::FromPhysicalFrame(InlinedFrameWalker& self, StackFrame& physicalFrame, Js::ScriptFunction *parent, bool fromBailout,
        int loopNum, const JavascriptStackWalker * const stackWalker, bool useInternalFrameInfo, bool noAlloc)
    {TRACE_IT(51752);
        bool inlinedFramesFound = false;
        FunctionBody* parentFunctionBody = parent->GetFunctionBody();
        EntryPointInfo *entryPointInfo;

        if (loopNum != -1)
        {TRACE_IT(51753);
            Assert(stackWalker);
        }

        void *nativeCodeAddress = nullptr;
        void *framePointer = nullptr;
        if (loopNum != -1 && useInternalFrameInfo)
        {TRACE_IT(51754);
            Assert(stackWalker->GetCachedInternalFrameInfo().codeAddress != nullptr);
            InternalFrameInfo lastInternalFrameInfo = stackWalker->GetCachedInternalFrameInfo();

            nativeCodeAddress = lastInternalFrameInfo.codeAddress;
            framePointer = lastInternalFrameInfo.framePointer;
        }
        else
        {TRACE_IT(51755);
            nativeCodeAddress = physicalFrame.GetInstructionPointer();
            framePointer = physicalFrame.GetFrame();
        }

        if (loopNum != -1)
        {TRACE_IT(51756);
            entryPointInfo = (Js::EntryPointInfo*)parentFunctionBody->GetLoopEntryPointInfoFromNativeAddress((DWORD_PTR)nativeCodeAddress, loopNum);
        }
        else
        {TRACE_IT(51757);
            entryPointInfo = (Js::EntryPointInfo*)parentFunctionBody->GetEntryPointFromNativeAddress((DWORD_PTR)nativeCodeAddress);
        }

        AssertMsg(entryPointInfo != nullptr, "Inlined frame should resolve to the right parent address");
        if (entryPointInfo->HasInlinees())
        {TRACE_IT(51758);
            void *entry = reinterpret_cast<void*>(entryPointInfo->GetNativeAddress());
            InlinedFrameWalker::InlinedFrame *outerMostFrame = InlinedFrame::FromPhysicalFrame(physicalFrame, stackWalker, entry, entryPointInfo, useInternalFrameInfo);

            if (!outerMostFrame)
            {TRACE_IT(51759);
                return inlinedFramesFound;
            }

            if (!fromBailout)
            {TRACE_IT(51760);
                InlineeFrameRecord* record = entryPointInfo->FindInlineeFrame((void*)nativeCodeAddress);

                if (record)
                {TRACE_IT(51761);
                    record->RestoreFrames(parent->GetFunctionBody(), outerMostFrame, JavascriptCallStackLayout::FromFramePointer(framePointer));
                }
            }

            if (outerMostFrame->callInfo.Count)
            {TRACE_IT(51762);
                inlinedFramesFound = true;
                if (noAlloc)
                {TRACE_IT(51763);
                    return inlinedFramesFound;
                }
                int32 frameCount = 0;
                InlinedFrameWalker::InlinedFrame *frameIterator = outerMostFrame;
                while (frameIterator->callInfo.Count)
                {TRACE_IT(51764);
                    frameCount++;
                    frameIterator = frameIterator->Next();
                }

                InlinedFrameWalker::InlinedFrame **frames = HeapNewArray(InlinedFrameWalker::InlinedFrame*, frameCount);

                frameIterator = outerMostFrame;
                for (int index = frameCount - 1; index >= 0; index--)
                {TRACE_IT(51765);
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
    {TRACE_IT(51766);
        parentFunction = nullptr;
        HeapDeleteArray(frameCount, frames);
        frames = nullptr;
        currentIndex = -1;
        frameCount = 0;
    }

    bool InlinedFrameWalker::Next(CallInfo& callInfo)
    {TRACE_IT(51767);
        MoveNext();
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        if (currentFrame)
        {TRACE_IT(51768);
            callInfo.Flags = CallFlags_None;
            callInfo.Count = (currentFrame->callInfo.Count & 0xFFFF);
        }

        return currentFrame != nullptr;
    }

    size_t InlinedFrameWalker::GetArgc() const
    {TRACE_IT(51769);
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        return currentFrame->callInfo.Count;
    }

    Js::Var *InlinedFrameWalker::GetArgv(bool includeThis /* = true */) const
    {TRACE_IT(51770);
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        uint firstArg = includeThis ? InlinedFrameArgIndex_This : InlinedFrameArgIndex_SecondScriptArg;
        Js::Var *args = &currentFrame->argv[firstArg];

        this->FinalizeStackValues(args, this->GetArgc() - firstArg);

        return args;
    }

    void InlinedFrameWalker::FinalizeStackValues(__in_ecount(argCount) Js::Var args[], size_t argCount) const
    {TRACE_IT(51771);
        ScriptContext *scriptContext = this->GetFunctionObject()->GetScriptContext();

        for (size_t i = 0; i < argCount; i++)
        {TRACE_IT(51772);
            args[i] = Js::JavascriptOperators::BoxStackInstance(args[i], scriptContext);
        }
    }

    Js::JavascriptFunction *InlinedFrameWalker::GetFunctionObject() const
    {TRACE_IT(51773);
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        return StackScriptFunction::GetCurrentFunctionObject(currentFrame->function);
    }

    void InlinedFrameWalker::SetFunctionObject(Js::JavascriptFunction * function)
    {TRACE_IT(51774);
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        currentFrame->function = function;
    }

    Js::Var InlinedFrameWalker::GetArgumentsObject() const
    {TRACE_IT(51775);
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        return currentFrame->arguments;
    }

    void InlinedFrameWalker::SetArgumentsObject(Js::Var arguments)
    {TRACE_IT(51776);
        InlinedFrameWalker::InlinedFrame *currentFrame = (InlinedFrameWalker::InlinedFrame *)GetCurrentFrame();
        Assert(currentFrame);

        currentFrame->arguments = arguments;
    }

    Js::Var InlinedFrameWalker::GetThisObject() const
    {TRACE_IT(51777);
        InlinedFrameWalker::InlinedFrame *const currentFrame = GetCurrentFrame();
        Assert(currentFrame);

        return currentFrame->argv[InlinedFrameArgIndex_This];
    }

    bool InlinedFrameWalker::IsCallerPhysicalFrame() const
    {TRACE_IT(51778);
        return currentIndex == (frameCount - 1);
    }

    bool InlinedFrameWalker::IsTopMostFrame() const
    {TRACE_IT(51779);
        return currentIndex == 0;
    }

    uint32 InlinedFrameWalker::GetCurrentInlineeOffset() const
    {TRACE_IT(51780);
        Assert(!IsTopMostFrame());
        Assert(currentIndex);

        return GetFrameAtIndex(currentIndex - 1)->callInfo.InlineeStartOffset;
    }

    uint32 InlinedFrameWalker::GetBottomMostInlineeOffset() const
    {TRACE_IT(51781);
        Assert(frameCount);

        return GetFrameAtIndex(frameCount - 1)->callInfo.InlineeStartOffset;
    }

    Js::JavascriptFunction *InlinedFrameWalker::GetBottomMostFunctionObject() const
    {TRACE_IT(51782);
        Assert(frameCount);

        return GetFrameAtIndex(frameCount - 1)->function;
    }

    InlinedFrameWalker::InlinedFrame *const InlinedFrameWalker::GetCurrentFrame() const
    {TRACE_IT(51783);
        return GetFrameAtIndex(currentIndex);
    }

    InlinedFrameWalker::InlinedFrame *const InlinedFrameWalker::GetFrameAtIndex(signed index) const
    {TRACE_IT(51784);
        Assert(frames);
        Assert(frameCount);

        InlinedFrameWalker::InlinedFrame *frame = nullptr;
        if (index < frameCount)
        {TRACE_IT(51785);
            frame = frames[index];
        }

        return frame;
    }

    void InlinedFrameWalker::MoveNext()
    {TRACE_IT(51786);
        currentIndex++;
    }

    void InlinedFrameWalker::Initialize(int32 frameCount, __in_ecount(frameCount) InlinedFrame **frames, Js::ScriptFunction *parent)
    {TRACE_IT(51787);
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
    {TRACE_IT(51788);
        // If the current javascript frame is a native frame, get the inlined frame from it, otherwise
        // it may be possible that current frame is the interpreter frame for a jitted loop body
        // If the loop body had some inlinees in it, retrieve the inlined frame using the cached info,
        // viz. instruction pointer, frame pointer, and stackCheckCodeHeight, about the loop body frame.
        struct InlinedFrame *inlinedFrame = nullptr;
        void *codeAddr, *framePointer;
        size_t stackCheckCodeHeight;

        if (useInternalFrameInfo)
        {TRACE_IT(51789);
            codeAddr = stackWalker->GetCachedInternalFrameInfo().codeAddress;
            framePointer = stackWalker->GetCachedInternalFrameInfo().framePointer;
            stackCheckCodeHeight = stackWalker->GetCachedInternalFrameInfo().stackCheckCodeHeight;
        }
        else
        {TRACE_IT(51790);
            codeAddr = currentFrame.GetInstructionPointer();
            framePointer = currentFrame.GetFrame();
            stackCheckCodeHeight = currentFrame.GetStackCheckCodeHeight();
        }

        if (!StackFrame::IsInStackCheckCode(entry, codeAddr, stackCheckCodeHeight))
        {TRACE_IT(51791);
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
    {TRACE_IT(51792);
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
    {TRACE_IT(51793);
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
    {TRACE_IT(51794);
        if (!Configuration::Global.flags.ValidateInlineStack)
        {TRACE_IT(51795);
            return true;
        }
        Js::JavascriptStackWalker walker(scriptContext);
        Js::JavascriptFunction* function;
        while (walker.GetCaller(&function))
        {TRACE_IT(51796);
            Assert(function);
            if (walker.GetCurrentInterpreterFrame() != nullptr)
            {TRACE_IT(51797);
                break;
            }
        }
        // If no asserts have fired yet - we should have succeeded.
        return true;
    }
#endif
}
