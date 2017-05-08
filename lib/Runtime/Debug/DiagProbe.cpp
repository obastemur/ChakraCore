//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language/InterpreterStackFrame.h"

#define InvalidScriptId 0xFFFFFFFF

namespace Js
{

    InterpreterHaltState::InterpreterHaltState(StopType _stopType, const FunctionBody* _executingFunction, MutationBreakpoint* _activeMutationBP/*= nullptr*/) :
        stopType(_stopType),
        executingFunction(_executingFunction),
        topFrame(nullptr),
        framePointers(nullptr),
        referencedDiagnosticArena(nullptr),
        exceptionObject(nullptr),
        stringBuilder(nullptr),
        activeMutationBP(_activeMutationBP)
    {TRACE_IT(42966);
        Assert(executingFunction || (stopType == STOP_EXCEPTIONTHROW || stopType == STOP_MUTATIONBREAKPOINT));
    }

    FunctionBody* InterpreterHaltState::GetFunction()
    {TRACE_IT(42967);
        Assert(IsValid());
        return this->topFrame->GetFunction();
    }

    int InterpreterHaltState::GetCurrentOffset()
    {TRACE_IT(42968);
        Assert(IsValid());
        return this->topFrame->GetByteCodeOffset();
    }

    void InterpreterHaltState::SetCurrentOffset(int offset)
    {TRACE_IT(42969);
        Assert(IsValid());
        if (this->topFrame->IsInterpreterFrame())
        {TRACE_IT(42970);
            // For interpreter frames, actual scenarios we need changed offset are: set next in topmost frame, ignore exception.
            // For throw exception we don't need it, but it doesn't hurt because interpreter will ignore the offset
            // and rather just throw the exception.
            this->topFrame->AsInterpreterFrame()->GetReader()->SetCurrentOffset(offset);
        }
        else
        {TRACE_IT(42971);
            // For native frames, the only scenario we need to record changed offset is when we ignore exception.
            if (this->exceptionObject && this->exceptionObject->IsDebuggerSkip())
            {TRACE_IT(42972);
                this->exceptionObject->SetByteCodeOffsetAfterDebuggerSkip(offset);
            }
        }
    }

    bool InterpreterHaltState::IsValid() const
    {TRACE_IT(42973);
        // "executingFunction == nullptr" when dispatching exception or mutation bp.
        return topFrame && (topFrame->GetFunction() == executingFunction || executingFunction == nullptr);
    }


    StepController::StepController()
        : stepType(STEP_NONE),
        byteOffset(0),
        statementMap(NULL),
        frameCountWhenSet(0),
        frameAddrWhenSet((size_t)-1),
        stepCompleteOnInlineBreakpoint(false),
        pActivatedContext(NULL),
        scriptIdWhenSet(InvalidScriptId),
        returnedValueRecordingDepth(0),
        returnedValueList(nullptr)
    {TRACE_IT(42974);
    }

    bool StepController::IsActive()
    {TRACE_IT(42975);
        return stepType != STEP_NONE;
    }

    void StepController::Activate(StepType stepType, InterpreterHaltState* haltState)
    {TRACE_IT(42976);
        this->stepType = stepType;
        this->byteOffset = haltState->GetCurrentOffset();
        this->pActivatedContext = haltState->framePointers->Peek()->GetScriptContext();
        Assert(this->pActivatedContext);

        Js::FunctionBody* functionBody = haltState->GetFunction();

        this->body.Root(functionBody, this->pActivatedContext->GetRecycler());
        this->statementMap = body->GetMatchingStatementMapFromByteCode(byteOffset, false);
        this->frameCountWhenSet = haltState->framePointers->Count();

        if (stepType != STEP_DOCUMENT)
        {TRACE_IT(42977);
            this->frameAddrWhenSet = (size_t)haltState->framePointers->Peek(0)->GetStackAddress();
        }
        else
        {TRACE_IT(42978);
            // for doc mode, do not bail out automatically on frame changes
            this->frameAddrWhenSet = (size_t)-1;
        }

        this->scriptIdWhenSet = GetScriptId(functionBody);

        if (this->returnedValueList == nullptr)
        {TRACE_IT(42979);
            this->returnedValueList = JsUtil::List<ReturnedValue*>::New(this->pActivatedContext->GetRecycler());
            this->pActivatedContext->GetThreadContext()->SetReturnedValueList(this->returnedValueList);
        }
    }

    void StepController::AddToReturnedValueContainer(Js::Var returnValue, Js::JavascriptFunction * function, bool isValueOfReturnStatement)
    {TRACE_IT(42980);
        if (this->pActivatedContext != nullptr) // This will be null when we execute scripts when on break.
        {TRACE_IT(42981);
            ReturnedValue *valuePair = RecyclerNew(pActivatedContext->GetRecycler(), ReturnedValue, returnValue, function, isValueOfReturnStatement);
            this->returnedValueList->Add(valuePair);
        }
    }

    void StepController::AddReturnToReturnedValueContainer()
    {
        AddToReturnedValueContainer(nullptr/*returnValue*/, nullptr/*function*/, true/*isValueOfReturnStatement*/);
    }

    void StepController::StartRecordingCall()
    {TRACE_IT(42982);
        returnedValueRecordingDepth++;
    }

    void StepController::EndRecordingCall(Js::Var returnValue, Js::JavascriptFunction * function)
    {TRACE_IT(42983);
        if (IsActive() && this->pActivatedContext != nullptr && returnValue != nullptr)
        {TRACE_IT(42984);
            if (this->pActivatedContext->GetThreadContext()->GetDebugManager()->IsAtDispatchHalt())
            {TRACE_IT(42985);
                // OS bug 3050302 - Keeping this FatalError for finding other issues where we can record when we are at break
                Js::Throw::FatalInternalError();
            }
            bool isStepOut = stepType == STEP_OUT || stepType == STEP_DOCUMENT;

            // Record when :
            // If step-out/document : we need to record calls only which are already on the stack, that means the recording-depth is zero or negative.
            // if not step-out (step-in and step-over). only for those, which are called from the current call-site or the ones as if we step-out
            if ((!isStepOut && returnedValueRecordingDepth <= 1) || (isStepOut && returnedValueRecordingDepth <= 0))
            {TRACE_IT(42986);
                // if we are step_document, we should be removing whatever we have collected so-far,
                // since they belong to the current document which is a library code
                if (stepType == STEP_DOCUMENT)
                {TRACE_IT(42987);
                    this->returnedValueList->ClearAndZero();
                }

                AddToReturnedValueContainer(returnValue, function, false/*isValueOfReturnStatement*/);
            }
        }
        returnedValueRecordingDepth--;
    }

    void StepController::ResetReturnedValueList()
    {TRACE_IT(42988);
        returnedValueRecordingDepth = 0;
        if (this->returnedValueList != nullptr)
        {TRACE_IT(42989);
            this->returnedValueList->ClearAndZero();
        }
    }

    void StepController::HandleResumeAction(Js::InterpreterHaltState* haltState, BREAKRESUMEACTION resumeAction)
    {TRACE_IT(42990);
        ResetReturnedValueList();
        switch (resumeAction)
        {
        case BREAKRESUMEACTION_STEP_INTO:
            Activate(Js::STEP_IN, haltState);
            break;
        case BREAKRESUMEACTION_STEP_OVER:
            Activate(Js::STEP_OVER, haltState);
            break;
        case BREAKRESUMEACTION_STEP_OUT:
            Activate(Js::STEP_OUT, haltState);
            break;
        case BREAKRESUMEACTION_STEP_DOCUMENT:
            Activate(Js::STEP_DOCUMENT, haltState);
            break;
        }
    }


    void StepController::Deactivate(InterpreterHaltState* haltState /*=nullptr*/)
    {TRACE_IT(42991);
        // If we are deactivating the step controller during ProbeContainer close or attach/detach we should clear return value list
        // If we break other than step -> clear the list.
        // If we step in and we land on different function (we are in recording phase the current function) -> clear the list
        if ((haltState == nullptr) || (haltState->stopType != Js::STOP_STEPCOMPLETE || (this->stepType == STEP_IN && this->returnedValueRecordingDepth > 0)))
        {TRACE_IT(42992);
            ResetReturnedValueList();
        }

        if (this->body)
        {TRACE_IT(42993);
            Assert(this->pActivatedContext);
            this->body.Unroot(this->pActivatedContext->GetRecycler());
        }
        this->pActivatedContext = NULL;
        stepType = STEP_NONE;
        byteOffset = Js::Constants::NoByteCodeOffset;
        statementMap = NULL;

        frameCountWhenSet = 0;
        scriptIdWhenSet = InvalidScriptId;
        frameAddrWhenSet = (size_t)-1;
    }

    bool StepController::IsStepComplete_AllowingFalsePositives(InterpreterStackFrame * stackFrame)
    {TRACE_IT(42994);
        Assert(stackFrame);
        if (stepType == STEP_IN)
        {TRACE_IT(42995);
            return true;
        }
        else if (stepType == STEP_DOCUMENT)
        {TRACE_IT(42996);
            Assert(stackFrame->GetFunctionBody());
            return GetScriptId(stackFrame->GetFunctionBody()) != this->scriptIdWhenSet;
        }

        // A STEP_OUT or a STEP_OVER has not completed if we are currently deeper on the callstack.
        return this->frameAddrWhenSet <= stackFrame->GetStackAddress();
    }

    bool StepController::IsStepComplete(InterpreterHaltState* haltState, HaltCallback * haltCallback, OpCode originalOpcode)
    {TRACE_IT(42997);
        int currentFrameCount = haltState->framePointers->Count();
        AssertMsg(currentFrameCount > 0, "In IsStepComplete we must have at least one frame.");

        FunctionBody* body = haltState->framePointers->Peek()->GetJavascriptFunction()->GetFunctionBody();
        bool canPossiblyHalt = haltCallback->CanHalt(haltState);

        OUTPUT_TRACE(Js::DebuggerPhase, _u("StepController::IsStepComplete(): stepType = %d "), stepType);

        uint scriptId = GetScriptId(body);
        AssertMsg(scriptId != InvalidScriptId, "scriptId cannot be 'invalid-reserved'");

        int byteOffset = haltState->GetCurrentOffset();
        bool fCanHalt = false;

        if (this->frameCountWhenSet > currentFrameCount && STEP_DOCUMENT != stepType)
        {TRACE_IT(42998);
            // all steps match once the frame they started on has popped.
            fCanHalt = canPossiblyHalt;
        }
        else if (STEP_DOCUMENT == stepType)
        {TRACE_IT(42999);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("StepController::IsStepComplete(): docId when set=%d, currentDocId = %d, can Halt = %d, will halt = %d "), this->scriptIdWhenSet, scriptId, canPossiblyHalt, fCanHalt);
            fCanHalt = (scriptId != this->scriptIdWhenSet) && canPossiblyHalt;
        }
        else if (STEP_IN != stepType && this->frameCountWhenSet < currentFrameCount)
        {TRACE_IT(43000);
            // Only step into allows the stack to be deeper
            OUTPUT_TRACE(Js::DebuggerPhase, _u("StepController::IsStepComplete(stepType = %d) returning false "), stepType);
            return false;
        }
        else if (STEP_OUT == stepType)
        {TRACE_IT(43001);
            fCanHalt = this->frameCountWhenSet > currentFrameCount && canPossiblyHalt;
        }
        else if (nullptr != this->statementMap && this->statementMap->isSubexpression && STEP_IN != stepType)
        {TRACE_IT(43002);
            // Only step into started from subexpression is allowed to stop on another subexpression
            Js::FunctionBody* pCurrentFuncBody = haltState->GetFunction();
            Js::FunctionBody::StatementMap* map = pCurrentFuncBody->GetMatchingStatementMapFromByteCode(byteOffset, false);
            if (nullptr != map && map->isSubexpression)    // Execute remaining Subexpressions
            {TRACE_IT(43003);
                fCanHalt = false;
            }
            else
            {TRACE_IT(43004);
                Js::FunctionBody::StatementMap* outerMap = pCurrentFuncBody->GetMatchingStatementMapFromByteCode(this->statementMap->byteCodeSpan.begin, true);
                if (nullptr != outerMap && map == outerMap) // Execute the rest of current regular statement
                {TRACE_IT(43005);
                    fCanHalt = false;
                }
                else
                {TRACE_IT(43006);
                    fCanHalt = canPossiblyHalt;
                }
            }
        }
        else
        {TRACE_IT(43007);
            // Match if we are no longer on the original statement.  Stepping means move off current statement.
            if (body != this->body || NULL == this->statementMap ||
                !this->statementMap->byteCodeSpan.Includes(byteOffset))
            {TRACE_IT(43008);
                fCanHalt = canPossiblyHalt;
            }
        }
        // At this point we are verifying of global return opcode.
        // The global returns are alway added as a zero range begin with zero.

        if (fCanHalt && originalOpcode == OpCode::Ret)
        {TRACE_IT(43009);
            Js::FunctionBody* pCurrentFuncBody = haltState->GetFunction();
            Js::FunctionBody::StatementMap* map = pCurrentFuncBody->GetMatchingStatementMapFromByteCode(byteOffset, true);

            fCanHalt = !FunctionBody::IsDummyGlobalRetStatement(&map->sourceSpan);
            if (fCanHalt)
            {TRACE_IT(43010);
                // We are breaking at last line of function, imagine '}'
                AddReturnToReturnedValueContainer();
            }
        }

        OUTPUT_TRACE(Js::DebuggerPhase, _u("StepController::IsStepComplete(stepType = %d) returning %d "), stepType, fCanHalt);
        return fCanHalt;
    }

    bool StepController::ContinueFromInlineBreakpoint()
    {TRACE_IT(43011);
        bool ret = stepCompleteOnInlineBreakpoint;
        stepCompleteOnInlineBreakpoint = false;
        return ret;
    }

    uint StepController::GetScriptId(_In_ FunctionBody* body)
    {TRACE_IT(43012);
        // safe value
        uint retValue = BuiltInFunctionsScriptId;

        if (body != nullptr)
        {TRACE_IT(43013);
            // FYI - Different script blocks within a HTML page will have different source Info ids even though they have the same backing file.
            // It might imply we notify the debugger a bit more than needed - thus can be TODO for performance improvements of the Just-My-Code
            // or step to next document boundary mode.
            AssertMsg(body->GetUtf8SourceInfo() != nullptr, "body->GetUtf8SourceInfo() == nullptr");
            retValue = body->GetUtf8SourceInfo()->GetSourceInfoId();
        }

        return retValue;
    }

    AsyncBreakController::AsyncBreakController()
        : haltCallback(NULL)
    {TRACE_IT(43014);
    }

    void AsyncBreakController::Activate(HaltCallback* haltCallback)
    {TRACE_IT(43015);
        InterlockedExchangePointer((PVOID*)&this->haltCallback, haltCallback);
    }

    void AsyncBreakController::Deactivate()
    {TRACE_IT(43016);
        InterlockedExchangePointer((PVOID*)&this->haltCallback, NULL);
    }

    bool AsyncBreakController::IsBreak()
    {TRACE_IT(43017);
        return haltCallback != NULL;
    }

    bool AsyncBreakController::IsAtStoppingLocation(InterpreterHaltState* haltState)
    {TRACE_IT(43018);
        HaltCallback* callback = this->haltCallback;
        if (callback)
        {TRACE_IT(43019);
            return callback->CanHalt(haltState);
        }
        return false;
    }

    void AsyncBreakController::DispatchAndReset(InterpreterHaltState* haltState)
    {TRACE_IT(43020);
        HaltCallback* callback = this->haltCallback;
        Deactivate();
        if (callback)
        {TRACE_IT(43021);
            callback->DispatchHalt(haltState);
        }
    }
}
