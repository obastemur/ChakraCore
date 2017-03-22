//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once
namespace Js
{
    class MutationBreakpoint;

    enum StopType
    {
        STOP_BREAKPOINT,
        STOP_INLINEBREAKPOINT,
        STOP_STEPCOMPLETE,
        STOP_EXCEPTIONTHROW,
        STOP_ASYNCBREAK,
        STOP_MUTATIONBREAKPOINT
    };

    struct ReturnedValue
    {
        ReturnedValue() {LOGMEIN("DiagProbe.h] 21\n");}
        ReturnedValue(Js::Var _returnedValue, Js::JavascriptFunction * _calledFunction, bool _isValueOfReturnStatement)
            : returnedValue(_returnedValue), calledFunction(_calledFunction), isValueOfReturnStatement(_isValueOfReturnStatement)
        {LOGMEIN("DiagProbe.h] 24\n");
            if (isValueOfReturnStatement)
            {LOGMEIN("DiagProbe.h] 26\n");
                Assert(returnedValue == nullptr);
                Assert(calledFunction == nullptr);
            }
        }

        Field(Js::Var) returnedValue;
        Field(Js::JavascriptFunction *) calledFunction;
        Field(bool) isValueOfReturnStatement;
    };

    typedef JsUtil::List<ReturnedValue*> ReturnedValueList;

    class DiagStackFrame;
    typedef JsUtil::Stack<DiagStackFrame*> DiagStack;
    typedef WeakArenaReference<DiagStack> WeakDiagStack;

    struct InterpreterHaltState
    {
        StopType stopType;
        const FunctionBody* executingFunction;
        DiagStackFrame* topFrame;
        DiagStack* framePointers;
        ReferencedArenaAdapter* referencedDiagnosticArena;
        JavascriptExceptionObject* exceptionObject;
        StringBuilder<ArenaAllocator>* stringBuilder;
        MutationBreakpoint* activeMutationBP;

        InterpreterHaltState(StopType _stopType, const FunctionBody* _executingFunction, MutationBreakpoint* _activeMutationBP = nullptr);

        FunctionBody* GetFunction();
        int GetCurrentOffset();
        void SetCurrentOffset(int offset);
        bool IsValid() const;
    };


    struct HaltCallback
    {
        virtual bool CanHalt(InterpreterHaltState* pHaltState) = 0;
        virtual void DispatchHalt(InterpreterHaltState* pHaltState) = 0;
        virtual void CleanupHalt() = 0;
        virtual bool IsInClosedState() {LOGMEIN("DiagProbe.h] 68\n"); return false; }

        // Mentions the policy if the hitting a breakpoint is allowed (based on the fact whether we are at callback from the breakpoint)
        virtual bool CanAllowBreakpoints() {LOGMEIN("DiagProbe.h] 71\n"); return false; }
    };

    struct Probe : HaltCallback
    {
        virtual bool Install(Js::ScriptContext* pScriptContext) = 0;
        virtual bool Uninstall(Js::ScriptContext* pScriptContext) = 0;
    };

    enum StepType : BYTE
    {
        STEP_NONE,
        STEP_IN         = 0x01,
        STEP_OVER       = 0x02,
        STEP_OUT        = 0x04,
        STEP_DOCUMENT   = 0x08,

        // On entry of a jitted function, need to bailout to handle stepping if in STEP_IN mode,
        // or STEP_OVER (e.g. STEP_OVER at the end of this function, and it is called again by a
        // library caller).
        STEP_BAILOUT    = STEP_IN | STEP_OVER,
    };

    struct DebuggerOptionsCallback
    {
        virtual bool IsExceptionReportingEnabled() {LOGMEIN("DiagProbe.h] 96\n"); return true; }
        virtual bool IsFirstChanceExceptionEnabled() {LOGMEIN("DiagProbe.h] 97\n"); return false; }
        virtual bool IsNonUserCodeSupportEnabled() {LOGMEIN("DiagProbe.h] 98\n"); return false; }
        virtual bool IsLibraryStackFrameSupportEnabled() {LOGMEIN("DiagProbe.h] 99\n"); return false; }
    };

    class StepController
    {
        friend class ProbeManager;
        friend class ProbeContainer;

        StepType stepType;
        int byteOffset;
        RecyclerRootPtr<FunctionBody> body;
        FunctionBody::StatementMap* statementMap;

        int frameCountWhenSet;
        int returnedValueRecordingDepth;

        DWORD_PTR frameAddrWhenSet;
        uint scriptIdWhenSet;

        bool stepCompleteOnInlineBreakpoint;
        ScriptContext *pActivatedContext;

        ReturnedValueList *returnedValueList;

    public:

        StepController();
        ~StepController()
        {LOGMEIN("DiagProbe.h] 127\n");
            this->Deactivate();
        }

        bool IsActive();
        void Activate(StepType stepType, InterpreterHaltState* haltState);
        void Deactivate(InterpreterHaltState* haltState = nullptr);
        bool IsStepComplete_AllowingFalsePositives(InterpreterStackFrame * stackFrame);
        bool IsStepComplete(InterpreterHaltState* haltState, HaltCallback *haltCallback, OpCode originalOpcode);
        bool ContinueFromInlineBreakpoint();

        ScriptContext* GetActivatedContext() const
        {LOGMEIN("DiagProbe.h] 139\n");
            return this->pActivatedContext;
        }

        const StepType* GetAddressOfStepType() const
        {LOGMEIN("DiagProbe.h] 144\n");
            return &stepType;
        }

        void* GetAddressOfScriptIdWhenSet() const
        {LOGMEIN("DiagProbe.h] 149\n");
            return (void*)&scriptIdWhenSet;
        }

        void* GetAddressOfFrameAddress() const
        {LOGMEIN("DiagProbe.h] 154\n");
            return (void*)&frameAddrWhenSet;
        }

        void SetFrameAddr(DWORD_PTR value)
        {LOGMEIN("DiagProbe.h] 159\n");
            this->frameAddrWhenSet = value;
        }

        void AddToReturnedValueContainer(Js::Var returnValue, Js::JavascriptFunction * function, bool isValueOfReturnStatement);
        void AddReturnToReturnedValueContainer();
        void StartRecordingCall();
        void EndRecordingCall(Js::Var returnValue, Js::JavascriptFunction * function);

        ReturnedValueList* GetReturnedValueList() const {LOGMEIN("DiagProbe.h] 168\n"); return this->returnedValueList; }
        void ResetReturnedValueList();
        void HandleResumeAction(Js::InterpreterHaltState* haltState, BREAKRESUMEACTION resumeAction);

    private:
        uint GetScriptId(_In_ FunctionBody* body);
    };

    // This is separate from the step controller because it is the only case where activation
    // happens while the script is running.

    class AsyncBreakController
    {
    private:
        HaltCallback* haltCallback;

     public:

        AsyncBreakController();
        void Activate(HaltCallback* haltCallback);
        void Deactivate();
        bool IsBreak();
        bool IsAtStoppingLocation(InterpreterHaltState* haltState);
        void DispatchAndReset(InterpreterHaltState* haltState);
    };

    typedef JsUtil::List<Probe*, ArenaAllocator> ProbeList;
}
