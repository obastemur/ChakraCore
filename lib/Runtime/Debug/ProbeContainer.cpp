//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language/JavascriptStackWalker.h"
#include "Language/InterpreterStackFrame.h"

namespace Js
{
    ProbeContainer::ProbeContainer() :
        diagProbeList(nullptr),
        pScriptContext(nullptr),
        debugManager(nullptr),
        haltCallbackProbe(nullptr),
        debuggerOptionsCallback(nullptr),
        pAsyncHaltCallback(nullptr),
        jsExceptionObject(nullptr),
        framePointers(nullptr),
        debugSessionNumber(0),
        tmpRegCount(0),
        bytecodeOffset(0),
        IsNextStatementChanged(false),
        isThrowInternal(false),
        forceBypassDebugEngine(false),
        isPrimaryBrokenToDebuggerContext(false),
        isForcedToEnterScriptStart(false),
        registeredFuncContextList(nullptr)
    {LOGMEIN("ProbeContainer.cpp] 28\n");
    }

    ProbeContainer::~ProbeContainer()
    {LOGMEIN("ProbeContainer.cpp] 32\n");
        this->Close();
    }

    void ProbeContainer::Close()
    {LOGMEIN("ProbeContainer.cpp] 37\n");
        // Probe manager instance may go down early.
        if (this->pScriptContext)
        {LOGMEIN("ProbeContainer.cpp] 40\n");
            debugManager = this->pScriptContext->GetThreadContext()->GetDebugManager();
        }
        else
        {
            debugManager = nullptr;
        }
        if (debugManager != nullptr && debugManager->stepController.pActivatedContext == pScriptContext)
        {LOGMEIN("ProbeContainer.cpp] 48\n");
            debugManager->stepController.Deactivate();
        }
#ifdef ENABLE_MUTATION_BREAKPOINT
        this->RemoveMutationBreakpointListIfNeeded();
#endif
        pScriptContext = nullptr;
        debugManager = nullptr;
    }

    void ProbeContainer::Initialize(ScriptContext* pScriptContext)
    {LOGMEIN("ProbeContainer.cpp] 59\n");
        if (!diagProbeList)
        {LOGMEIN("ProbeContainer.cpp] 61\n");
            ArenaAllocator* global = pScriptContext->AllocatorForDiagnostics();

            diagProbeList = ProbeList::New(global);

            pendingProbeList = ProbeList::New(global);

            this->pScriptContext = pScriptContext;
            this->debugManager = this->pScriptContext->GetThreadContext()->GetDebugManager();
            this->pinnedPropertyRecords = JsUtil::List<const Js::PropertyRecord*>::New(this->pScriptContext->GetRecycler());
            this->pScriptContext->BindReference((void *)this->pinnedPropertyRecords);
        }
    }

    void ProbeContainer::StartRecordingCall()
    {LOGMEIN("ProbeContainer.cpp] 76\n");
        this->debugManager->stepController.StartRecordingCall();
    }

    void ProbeContainer::EndRecordingCall(Js::Var returnValue, Js::JavascriptFunction * function)
    {LOGMEIN("ProbeContainer.cpp] 81\n");
        this->debugManager->stepController.EndRecordingCall(returnValue, function);
    }

    ReturnedValueList* ProbeContainer::GetReturnedValueList() const
    {LOGMEIN("ProbeContainer.cpp] 86\n");
        return this->debugManager->stepController.GetReturnedValueList();
    }

    void ProbeContainer::ResetReturnedValueList()
    {LOGMEIN("ProbeContainer.cpp] 91\n");
        this->debugManager->stepController.ResetReturnedValueList();
    }

    void ProbeContainer::UpdateFramePointers(bool fMatchWithCurrentScriptContext, DWORD_PTR dispatchHaltFrameAddress)
    {LOGMEIN("ProbeContainer.cpp] 96\n");
        ArenaAllocator* pDiagArena = debugManager->GetDiagnosticArena()->Arena();
        framePointers = Anew(pDiagArena, DiagStack, pDiagArena);

        JavascriptStackWalker walker(pScriptContext, !fMatchWithCurrentScriptContext, nullptr/*returnAddress*/, true/*forceFullWalk*/);
        DiagStack* tempFramePointers = Anew(pDiagArena, DiagStack, pDiagArena);
        const bool isLibraryFrameEnabledDebugger = IsLibraryStackFrameSupportEnabled();

        walker.WalkUntil([&](JavascriptFunction* func, ushort frameIndex) -> bool
        {
            if (isLibraryFrameEnabledDebugger || !func->IsLibraryCode())
            {LOGMEIN("ProbeContainer.cpp] 107\n");
                DiagStackFrame* frm = nullptr;
                InterpreterStackFrame *interpreterFrame = walker.GetCurrentInterpreterFrame();
                ScriptContext* frameScriptContext = walker.GetCurrentScriptContext();
                Assert(frameScriptContext);

                if (!fMatchWithCurrentScriptContext && !frameScriptContext->IsScriptContextInDebugMode() && tempFramePointers->Count() == 0)
                {LOGMEIN("ProbeContainer.cpp] 114\n");
                    // this means the top frame is not in the debug mode. We shouldn't be stopping for this break.
                    // This could happen if the exception happens on the diagnosticsScriptEngine.
                    return true;
                }

                // Ignore frames which are not in debug mode, which can happen when diag engine calls into user engine under debugger
                // -- topmost frame is under debugger but some frames could be in non-debug mode as they are from diag engine.
                if (frameScriptContext->IsScriptContextInDebugMode() &&
                    (!fMatchWithCurrentScriptContext || frameScriptContext == pScriptContext))
                {LOGMEIN("ProbeContainer.cpp] 124\n");
                    if (interpreterFrame)
                    {LOGMEIN("ProbeContainer.cpp] 126\n");
                        if (dispatchHaltFrameAddress == 0 || interpreterFrame->GetStackAddress() > dispatchHaltFrameAddress)
                        {LOGMEIN("ProbeContainer.cpp] 128\n");
                            frm = Anew(pDiagArena, DiagInterpreterStackFrame, interpreterFrame);
                        }
                    }
                    else
                    {
                        void* stackAddress = walker.GetCurrentArgv();
                        if (dispatchHaltFrameAddress == 0 || reinterpret_cast<DWORD_PTR>(stackAddress) > dispatchHaltFrameAddress)
                        {LOGMEIN("ProbeContainer.cpp] 136\n");
#if ENABLE_NATIVE_CODEGEN
                            if (func->IsScriptFunction())
                            {LOGMEIN("ProbeContainer.cpp] 139\n");
                                frm = Anew(pDiagArena, DiagNativeStackFrame,
                                    ScriptFunction::FromVar(walker.GetCurrentFunction()), walker.GetByteCodeOffset(), stackAddress, walker.GetCurrentCodeAddr());
                            }
                            else
#else
                            Assert(!func->IsScriptFunction());
#endif
                            {
                                frm = Anew(pDiagArena, DiagRuntimeStackFrame, func, walker.GetCurrentNativeLibraryEntryName(), stackAddress);
                            }
                        }
                    }
                }

                if (frm)
                {LOGMEIN("ProbeContainer.cpp] 155\n");
                    tempFramePointers->Push(frm);
                }
            }

            return false;
        });

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::UpdateFramePointers: detected %d frames (this=%p, fMatchWithCurrentScriptContext=%d)\n"),
            tempFramePointers->Count(), this, fMatchWithCurrentScriptContext);

        while (tempFramePointers->Count())
        {LOGMEIN("ProbeContainer.cpp] 167\n");
            framePointers->Push(tempFramePointers->Pop());
        }
    }

    WeakDiagStack * ProbeContainer::GetFramePointers(DWORD_PTR dispatchHaltFrameAddress)
    {LOGMEIN("ProbeContainer.cpp] 173\n");
        if (framePointers == nullptr || this->debugSessionNumber < debugManager->GetDebugSessionNumber())
        {
            UpdateFramePointers(/*fMatchWithCurrentScriptContext*/true, dispatchHaltFrameAddress);
            this->debugSessionNumber = debugManager->GetDebugSessionNumber();

            if ((framePointers->Count() > 0) &&
                debugManager->IsMatchTopFrameStackAddress(framePointers->Peek(0)))
            {LOGMEIN("ProbeContainer.cpp] 181\n");
                framePointers->Peek(0)->SetIsTopFrame();
            }
        }

        ReferencedArenaAdapter* pRefArena = debugManager->GetDiagnosticArena();
        return HeapNew(WeakDiagStack,pRefArena,framePointers);
    }

    bool ProbeContainer::InitializeLocation(InterpreterHaltState* pHaltState, bool fMatchWithCurrentScriptContext)
    {LOGMEIN("ProbeContainer.cpp] 191\n");
        Assert(debugManager);
        debugManager->SetCurrentInterpreterLocation(pHaltState);

        ArenaAllocator* pDiagArena = debugManager->GetDiagnosticArena()->Arena();

        UpdateFramePointers(fMatchWithCurrentScriptContext);
        pHaltState->framePointers = framePointers;
        pHaltState->stringBuilder = Anew(pDiagArena, StringBuilder<ArenaAllocator>, pDiagArena);

        if (pHaltState->framePointers->Count() > 0)
        {LOGMEIN("ProbeContainer.cpp] 202\n");
            pHaltState->topFrame = pHaltState->framePointers->Peek(0);
            pHaltState->topFrame->SetIsTopFrame();
        }

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::InitializeLocation (end): this=%p, pHaltState=%p, fMatch=%d, topFrame=%p\n"),
            this, pHaltState, fMatchWithCurrentScriptContext, pHaltState->topFrame);

        return true;
    }

    void ProbeContainer::DestroyLocation()
    {LOGMEIN("ProbeContainer.cpp] 214\n");
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DestroyLocation (start): this=%p, IsNextStatementChanged=%d, haltCallbackProbe=%p\n"),
            this, this->IsNextStatementChanged, haltCallbackProbe);

        if (IsNextStatementChanged)
        {LOGMEIN("ProbeContainer.cpp] 219\n");
            Assert(bytecodeOffset != debugManager->stepController.byteOffset);
            // Note: when we dispatching an exception bytecodeOffset would be same as pProbeManager->pCurrentInterpreterLocation->GetCurrentOffset().

            debugManager->pCurrentInterpreterLocation->SetCurrentOffset(bytecodeOffset);
            IsNextStatementChanged = false;
        }

        framePointers = nullptr;

        // Reset the exception object.

        jsExceptionObject = nullptr;

        Assert(debugManager);
        debugManager->UnsetCurrentInterpreterLocation();

        pinnedPropertyRecords->Reset();

        // Guarding if the probe engine goes away when we are sitting at breakpoint.
        if (haltCallbackProbe)
        {LOGMEIN("ProbeContainer.cpp] 240\n");
            // The clean up is called here to scriptengine's object to remove all DebugStackFrames
            haltCallbackProbe->CleanupHalt();
        }
    }

    bool ProbeContainer::CanDispatchHalt(InterpreterHaltState* pHaltState)
    {LOGMEIN("ProbeContainer.cpp] 247\n");
        if (!haltCallbackProbe || haltCallbackProbe->IsInClosedState() || debugManager->IsAtDispatchHalt())
        {LOGMEIN("ProbeContainer.cpp] 249\n");
            OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, _u("ProbeContainer::CanDispatchHalt: Not in break mode. pHaltState = %p\n"), pHaltState);
            return false;
        }
        return true;
    }

    void ProbeContainer::DispatchStepHandler(InterpreterHaltState* pHaltState, OpCode* pOriginalOpcode)
    {LOGMEIN("ProbeContainer.cpp] 257\n");
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchStepHandler: start: this=%p, pHaltState=%p, pOriginalOpcode=0x%x\n"), this, pHaltState, pOriginalOpcode);

        if (!CanDispatchHalt(pHaltState))
        {LOGMEIN("ProbeContainer.cpp] 261\n");
            return;
        }

        TryFinally([&]()
          {
            InitializeLocation(pHaltState);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchStepHandler: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n"),
                pHaltState, pHaltState->IsValid());

            if (pHaltState->IsValid()) // Only proceed if we find a valid top frame and that is the executing function
            {LOGMEIN("ProbeContainer.cpp] 272\n");
                if (debugManager->stepController.IsStepComplete(pHaltState, haltCallbackProbe, *pOriginalOpcode))
                {LOGMEIN("ProbeContainer.cpp] 274\n");
                    OpCode oldOpcode = *pOriginalOpcode;
                    pHaltState->GetFunction()->ProbeAtOffset(pHaltState->GetCurrentOffset(), pOriginalOpcode);
                    pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                    debugManager->stepController.Deactivate(pHaltState);
                    haltCallbackProbe->DispatchHalt(pHaltState);

                    if (oldOpcode == OpCode::Break && debugManager->stepController.stepType == STEP_DOCUMENT)
                    {LOGMEIN("ProbeContainer.cpp] 283\n");
                        // That means we have delivered the stepping to the debugger, where we had the breakpoint
                        // already, however it is possible that debugger can initiate the step_document. In that
                        // case debugger did not break due to break. So we have break as a breakpoint reason.
                        *pOriginalOpcode = OpCode::Break;
                    }
                    else if (OpCode::Break == *pOriginalOpcode)
                    {LOGMEIN("ProbeContainer.cpp] 290\n");
                        debugManager->stepController.stepCompleteOnInlineBreakpoint = true;
                    }
                }
            }
          },
          [&](bool) 
          {LOGMEIN("ProbeContainer.cpp] 297\n");
            DestroyLocation();
          });

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchStepHandler: end: pHaltState=%p\n"), pHaltState);
    }

    void ProbeContainer::DispatchAsyncBreak(InterpreterHaltState* pHaltState)
    {LOGMEIN("ProbeContainer.cpp] 305\n");
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchAsyncBreak: start: this=%p, pHaltState=%p\n"), this, pHaltState);

        if (!this->pAsyncHaltCallback || !CanDispatchHalt(pHaltState))
        {LOGMEIN("ProbeContainer.cpp] 309\n");
            return;
        }

        TryFinally([&]()
        {
            InitializeLocation(pHaltState, /* We don't need to match script context, stop at any available script function */ false);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchAsyncBreak: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n"),
                pHaltState, pHaltState->IsValid());

            if (pHaltState->IsValid())
            {LOGMEIN("ProbeContainer.cpp] 320\n");
                // Activate the current haltCallback with asyncStepController.
                debugManager->asyncBreakController.Activate(this->pAsyncHaltCallback);
                if (debugManager->asyncBreakController.IsAtStoppingLocation(pHaltState))
                {LOGMEIN("ProbeContainer.cpp] 324\n");
                    OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchAsyncBreak: IsAtStoppingLocation: pHaltState=%p\n"), pHaltState);

                    pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                    debugManager->stepController.Deactivate(pHaltState);
                    debugManager->asyncBreakController.DispatchAndReset(pHaltState);
                }
            }
        },
        [&](bool)
        {LOGMEIN("ProbeContainer.cpp] 335\n");
            DestroyLocation();
        });

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchAsyncBreak: end: pHaltState=%p\n"), pHaltState);
    }

    void ProbeContainer::DispatchInlineBreakpoint(InterpreterHaltState* pHaltState)
    {LOGMEIN("ProbeContainer.cpp] 343\n");
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchInlineBreakpoint: start: this=%p, pHaltState=%p\n"), this, pHaltState);

        if (!CanDispatchHalt(pHaltState))
        {LOGMEIN("ProbeContainer.cpp] 347\n");
            return;
        }

        Assert(pHaltState->stopType == STOP_INLINEBREAKPOINT);

        TryFinally([&]()
        {
            InitializeLocation(pHaltState);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchInlineBreakpoint: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n"),
                pHaltState, pHaltState->IsValid());

            Assert(pHaltState->IsValid());

            // The ByteCodeReader should be available at this point, but because of possibility of garbled frame, we shouldn't hit AV
            if (pHaltState->IsValid())
            {LOGMEIN("ProbeContainer.cpp] 363\n");
#if DBG
                pHaltState->GetFunction()->MustBeInDebugMode();
#endif

                // an inline breakpoint is being dispatched deactivate other stopping controllers
                debugManager->stepController.Deactivate(pHaltState);
                debugManager->asyncBreakController.Deactivate();

                pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                haltCallbackProbe->DispatchHalt(pHaltState);
            }
        },
        [&](bool)
        {LOGMEIN("ProbeContainer.cpp] 378\n");
            DestroyLocation();
        });
        
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchInlineBreakpoint: end: pHaltState=%p\n"), pHaltState);
    }

    bool ProbeContainer::DispatchExceptionBreakpoint(InterpreterHaltState* pHaltState)
    {LOGMEIN("ProbeContainer.cpp] 386\n");
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchExceptionBreakpoint: start: this=%p, pHaltState=%p\n"), this, pHaltState);
        bool fSuccess = false;
        if (!haltCallbackProbe || haltCallbackProbe->IsInClosedState() || debugManager->IsAtDispatchHalt())
        {LOGMEIN("ProbeContainer.cpp] 390\n");
            OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchExceptionBreakpoint: not in break mode: pHaltState=%p\n"), pHaltState);
            // Will not be able to handle multiple break-hits.
            return fSuccess;
        }

        Assert(pHaltState->stopType == STOP_EXCEPTIONTHROW);

        jsExceptionObject = pHaltState->exceptionObject->GetThrownObject(nullptr);

        // Will store current offset of the bytecode block.
        int currentOffset = -1;

        TryFinally([&]()       
        {
            InitializeLocation(pHaltState, false);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchExceptionBreakpoint: initialized location: pHaltState=%p, IsInterpreterFrame=%d\n"),
                pHaltState, pHaltState->IsValid(), pHaltState->topFrame && pHaltState->topFrame->IsInterpreterFrame());

            // The ByteCodeReader should be available at this point, but because of possibility of garbled frame, we shouldn't hit AV
            if (pHaltState->IsValid() && pHaltState->GetFunction()->GetScriptContext()->IsScriptContextInDebugMode())
            {LOGMEIN("ProbeContainer.cpp] 411\n");
#if DBG
                pHaltState->GetFunction()->MustBeInDebugMode();
#endif

                // For interpreter frames, change the current location pointer of bytecode block, as it might be pointing to the next statement on the body.
                // In order to generated proper binding of break on exception to the statement, the bytecode offset needed to be on the same span
                // of the statement.
                // For native frames the offset is always current.
                // Move back a single byte to ensure that it falls under on the same statement.
                if (pHaltState->topFrame->IsInterpreterFrame())
                {LOGMEIN("ProbeContainer.cpp] 422\n");
                    currentOffset = pHaltState->GetCurrentOffset();
                    Assert(currentOffset > 0);
                    pHaltState->SetCurrentOffset(currentOffset - 1);
                }

                // an inline breakpoint is being dispatched deactivate other stopping controllers
                debugManager->stepController.Deactivate(pHaltState);
                debugManager->asyncBreakController.Deactivate();

                pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                ScriptContext *pTopFuncContext = pHaltState->GetFunction()->GetScriptContext();

                // If the top function's context is different from the current context, that means current frame is not alive anymore and breaking here cannot not happen.
                // So in that case we will consider the top function's context and break on that context.
                if (pTopFuncContext != pScriptContext)
                {LOGMEIN("ProbeContainer.cpp] 439\n");
                    OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchExceptionBreakpoint: top function's context is different from the current context: pHaltState=%p, haltCallbackProbe=%p\n"),
                        pHaltState, pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe);
                    if (pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe)
                    {LOGMEIN("ProbeContainer.cpp] 443\n");
                        pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe->DispatchHalt(pHaltState);
                        fSuccess = true;
                    }
                }
                else
                {
                    haltCallbackProbe->DispatchHalt(pHaltState);
                    fSuccess = true;
                }
            }
        },
        [&](bool)
        {LOGMEIN("ProbeContainer.cpp] 456\n");
            // If the next statement has changed, we need to log that to exception object so that it will not try to advance to next statement again.
            pHaltState->exceptionObject->SetIgnoreAdvanceToNextStatement(IsNextStatementChanged);

            // Restore the current offset;
            if (currentOffset != -1 && pHaltState->topFrame->IsInterpreterFrame())
            {LOGMEIN("ProbeContainer.cpp] 462\n");
                pHaltState->SetCurrentOffset(currentOffset);
            }

            DestroyLocation();
        });

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchExceptionBreakpoint: end: pHaltState=%p, fSuccess=%d\n"), pHaltState, fSuccess);
        return fSuccess;
    }

    void ProbeContainer::DispatchMutationBreakpoint(InterpreterHaltState* pHaltState)
    {LOGMEIN("ProbeContainer.cpp] 474\n");
        Assert(pHaltState->stopType == STOP_MUTATIONBREAKPOINT);

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchMutationBreakpoint: start: this=%p, pHaltState=%p\n"), this, pHaltState);
        if (!CanDispatchHalt(pHaltState))
        {LOGMEIN("ProbeContainer.cpp] 479\n");
            return;
        }

        // will store Current offset of the bytecode block.
        int currentOffset = -1;

        TryFinally([&]()        
        {
            InitializeLocation(pHaltState);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchMutationBreakpoint: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n"),
                pHaltState, pHaltState->IsValid());

            if (pHaltState->IsValid())
            {LOGMEIN("ProbeContainer.cpp] 493\n");
                // For interpreter frames, change the current location pointer of bytecode block, as it might be pointing to the next statement on the body.
                // In order to generated proper binding of mutation statement, the bytecode offset needed to be on the same span of the statement.
                // For native frames the offset is always current.
                // Move back a single byte to ensure that it falls under on the same statement.
                if (pHaltState->topFrame->IsInterpreterFrame())
                {LOGMEIN("ProbeContainer.cpp] 499\n");
                    currentOffset = pHaltState->GetCurrentOffset();
                    Assert(currentOffset > 0);
                    pHaltState->SetCurrentOffset(currentOffset - 1);
                }
                debugManager->stepController.Deactivate(pHaltState);
                debugManager->asyncBreakController.Deactivate();

                pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                Assert(pHaltState->GetFunction()->GetScriptContext() == pScriptContext);

                haltCallbackProbe->DispatchHalt(pHaltState);
            }
        },
        [&](bool)
        {LOGMEIN("ProbeContainer.cpp] 515\n");
            // Restore the current offset;
            if (currentOffset != -1 && pHaltState->topFrame->IsInterpreterFrame())
            {LOGMEIN("ProbeContainer.cpp] 518\n");
                pHaltState->SetCurrentOffset(currentOffset);
            }
            DestroyLocation();
        });

    }

    void ProbeContainer::DispatchProbeHandlers(InterpreterHaltState* pHaltState)
    {LOGMEIN("ProbeContainer.cpp] 527\n");
        if (!CanDispatchHalt(pHaltState))
        {LOGMEIN("ProbeContainer.cpp] 529\n");
            return;
        }

        TryFinally([&]()        
        {
            InitializeLocation(pHaltState);

            if (pHaltState->IsValid())
            {LOGMEIN("ProbeContainer.cpp] 538\n");
                Js::ProbeList * localPendingProbeList = this->pendingProbeList;
                diagProbeList->Map([pHaltState, localPendingProbeList](int index, Probe * probe)
                {
                    if (probe->CanHalt(pHaltState))
                    {LOGMEIN("ProbeContainer.cpp] 543\n");
                        localPendingProbeList->Add(probe);
                    }
                });

                if (localPendingProbeList->Count() != 0)
                {LOGMEIN("ProbeContainer.cpp] 549\n");
                    localPendingProbeList->MapUntil([&](int index, Probe * probe)
                    {
                        if (haltCallbackProbe && !haltCallbackProbe->IsInClosedState())
                        {LOGMEIN("ProbeContainer.cpp] 553\n");
                            debugManager->stepController.Deactivate(pHaltState);
                            debugManager->asyncBreakController.Deactivate();
                            haltCallbackProbe->DispatchHalt(pHaltState);
                        }
                        // If SetNextStatement happened between multiple BPs on same location, IP changed so rest of dispatch are not valid.
                        return this->IsSetNextStatementCalled();
                    });
                }
            }
        },
        [&](bool)
        {LOGMEIN("ProbeContainer.cpp] 565\n");
            pendingProbeList->Clear();
            DestroyLocation();
        });
    }

    void ProbeContainer::UpdateStep(bool fDuringSetupDebugApp/*= false*/)
    {LOGMEIN("ProbeContainer.cpp] 572\n");
        // This function indicate that when the page is being refreshed and the last action we have done was stepping.
        // so update the state of the current stepController.
        if (debugManager)
        {LOGMEIN("ProbeContainer.cpp] 576\n");
            // Usually we need to be in debug mode to UpdateStep. But during setting up new engine to debug mode we have an
            // ordering issue and the new engine will enter debug mode after this. So allow non-debug mode if fDuringSetupDebugApp.
            AssertMsg(fDuringSetupDebugApp || (pScriptContext && pScriptContext->IsScriptContextInDebugMode()), "Why UpdateStep when we are not in debug mode?");
            debugManager->stepController.stepType = STEP_IN;
        }
    }

    void ProbeContainer::DeactivateStep()
    {LOGMEIN("ProbeContainer.cpp] 585\n");
        if (debugManager)
        {LOGMEIN("ProbeContainer.cpp] 587\n");
            debugManager->stepController.stepType = STEP_NONE;
        }
    }

    void ProbeContainer::InitializeInlineBreakEngine(HaltCallback* probe)
    {LOGMEIN("ProbeContainer.cpp] 593\n");
        AssertMsg(!haltCallbackProbe || probe == haltCallbackProbe, "Overwrite of Inline bp probe with different probe");
        haltCallbackProbe = probe;
    }

    void ProbeContainer::UninstallInlineBreakpointProbe(HaltCallback* probe)
    {LOGMEIN("ProbeContainer.cpp] 599\n");
        haltCallbackProbe = nullptr;
    }

    void ProbeContainer::InitializeDebuggerScriptOptionCallback(DebuggerOptionsCallback* debuggerOptionsCallback)
    {LOGMEIN("ProbeContainer.cpp] 604\n");
        Assert(this->debuggerOptionsCallback == nullptr);
        this->debuggerOptionsCallback = debuggerOptionsCallback;
    }

    void ProbeContainer::UninstallDebuggerScriptOptionCallback()
    {LOGMEIN("ProbeContainer.cpp] 610\n");
        this->debuggerOptionsCallback = nullptr;
    }

    void ProbeContainer::AddProbe(Probe* pProbe)
    {LOGMEIN("ProbeContainer.cpp] 615\n");
        if (pProbe->Install(nullptr))
        {LOGMEIN("ProbeContainer.cpp] 617\n");
            diagProbeList->Add(pProbe);
        }
    }

    void ProbeContainer::RemoveProbe(Probe* pProbe)
    {LOGMEIN("ProbeContainer.cpp] 623\n");
        if (pProbe->Uninstall(nullptr))
        {LOGMEIN("ProbeContainer.cpp] 625\n");
            diagProbeList->Remove(pProbe);
        }
    }

    void ProbeContainer::RemoveAllProbes()
    {LOGMEIN("ProbeContainer.cpp] 631\n");
#ifdef ENABLE_MUTATION_BREAKPOINT
        if (HasMutationBreakpoints())
        {LOGMEIN("ProbeContainer.cpp] 634\n");
            ClearMutationBreakpoints();
        }
#endif
        for (int i = 0; i < diagProbeList->Count(); i++)
        {LOGMEIN("ProbeContainer.cpp] 639\n");
            diagProbeList->Item(i)->Uninstall(nullptr);
        }
        diagProbeList->Clear();
    }

    // Retrieves the offset of next statement in JavaScript user code for advancing from current statement
    // (normal flow-control is respected).
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetForAdvance(Js::FunctionBody* functionBody, ByteCodeReader* reader, int currentOffset, int* nextStatementOffset)
    {LOGMEIN("ProbeContainer.cpp] 649\n");
        int originalCurrentOffset = currentOffset;
        while (GetNextUserStatementOffsetHelper(functionBody, currentOffset, FunctionBody::SAT_FromCurrentToNext, nextStatementOffset))
        {LOGMEIN("ProbeContainer.cpp] 652\n");
            Js::DebuggerScope *debuggerScope = functionBody->GetDiagCatchScopeObjectAt(currentOffset);
            if (debuggerScope != nullptr && !debuggerScope->IsOffsetInScope(*nextStatementOffset))
            {LOGMEIN("ProbeContainer.cpp] 655\n");
                // Our next statement is not within this catch block, So we cannot just jump to it, we need to return false so the stack unwind will happen.
                return false;
            }

            Assert(currentOffset < *nextStatementOffset);

            if (IsTmpRegCountIncreased(functionBody, reader, originalCurrentOffset, *nextStatementOffset, true /*restoreOffset*/))
            {LOGMEIN("ProbeContainer.cpp] 663\n");
                currentOffset = *nextStatementOffset;
            }
            else
            {
                return true;
            }
        }

        return false;
    }

    // Retrieves the offset of beginning of next statement in JavaScript user code for explicit set next statement
    // (normal flow-control is not respected, just get start next statement).
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetForSetNext(Js::FunctionBody* functionBody, int currentOffset, int* nextStatementOffset)
    {LOGMEIN("ProbeContainer.cpp] 679\n");
        return GetNextUserStatementOffsetHelper(functionBody, currentOffset, FunctionBody::SAT_NextStatementStart, nextStatementOffset);
    }

    // Retrieves the offset of beginning of next statement in JavaScript user code for scenario specified by adjType.
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetHelper(
        Js::FunctionBody* functionBody, int currentOffset, FunctionBody::StatementAdjustmentType adjType, int* nextStatementOffset)
    {LOGMEIN("ProbeContainer.cpp] 687\n");
        Assert(functionBody);
        Assert(nextStatementOffset);

        FunctionBody::StatementMapList* pStatementMaps = functionBody->GetStatementMaps();
        if (pStatementMaps && pStatementMaps->Count() > 1)
        {LOGMEIN("ProbeContainer.cpp] 693\n");
            for (int index = 0; index < pStatementMaps->Count() - 1; index++)
            {LOGMEIN("ProbeContainer.cpp] 695\n");
                FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);

                if (!pStatementMap->isSubexpression && pStatementMap->byteCodeSpan.Includes(currentOffset))
                {LOGMEIN("ProbeContainer.cpp] 699\n");
                    int nextMapIndex = index;
                    FunctionBody::StatementMap* pNextStatementMap = Js::FunctionBody::GetNextNonSubexpressionStatementMap(pStatementMaps, ++nextMapIndex);
                    if (!pNextStatementMap)
                    {LOGMEIN("ProbeContainer.cpp] 703\n");
                        break;
                    }

                    // We are trying to find out the Branch opcode, between current and next statement. Skipping that would give use incorrect execution order.
                    FunctionBody::StatementAdjustmentRecord adjRecord;
                    if (pNextStatementMap->byteCodeSpan.begin > pStatementMap->byteCodeSpan.end &&
                        functionBody->GetBranchOffsetWithin(pStatementMap->byteCodeSpan.end, pNextStatementMap->byteCodeSpan.begin, &adjRecord) &&
                        (adjRecord.GetAdjustmentType() & adjType))
                    {LOGMEIN("ProbeContainer.cpp] 712\n");
                        Assert(adjRecord.GetByteCodeOffset() > (uint)pStatementMap->byteCodeSpan.end);
                        *nextStatementOffset = adjRecord.GetByteCodeOffset();
                    }
                    else
                    {
                        *nextStatementOffset = pNextStatementMap->byteCodeSpan.begin;
                    }
                    return true;
                }
            }
        }

        *nextStatementOffset = -1;
        return false;
    }

    bool ProbeContainer::FetchTmpRegCount(Js::FunctionBody * functionBody, Js::ByteCodeReader * reader, int atOffset, uint32 *pTmpRegCount, Js::OpCode *pOp)
    {LOGMEIN("ProbeContainer.cpp] 730\n");
        Assert(pTmpRegCount);
        Assert(pOp);

        Js::LayoutSize layoutSize;
        reader->SetCurrentOffset(atOffset);
        *pOp = reader->ReadOp(layoutSize);

        if (*pOp == Js::OpCode::Break)
        {LOGMEIN("ProbeContainer.cpp] 739\n");
            // User might have put breakpoint on the skipped or target statement, get the original opcode;
            if (functionBody->ProbeAtOffset(atOffset, pOp))
            {LOGMEIN("ProbeContainer.cpp] 742\n");
                if (Js::OpCodeUtil::IsPrefixOpcode(*pOp))
                {LOGMEIN("ProbeContainer.cpp] 744\n");
                    *pOp = reader->ReadPrefixedOp(layoutSize, *pOp);
                }
            }
        }

        if (*pOp == Js::OpCode::EmitTmpRegCount)
        {LOGMEIN("ProbeContainer.cpp] 751\n");
            switch (layoutSize)
            {LOGMEIN("ProbeContainer.cpp] 753\n");
            case Js::SmallLayout:
            {LOGMEIN("ProbeContainer.cpp] 755\n");
                const unaligned Js::OpLayoutReg1_Small * playout = reader->Reg1_Small();
                *pTmpRegCount = (uint32)playout->R0;
            }
                break;
            case Js::MediumLayout:
            {LOGMEIN("ProbeContainer.cpp] 761\n");
                const unaligned Js::OpLayoutReg1_Medium * playout = reader->Reg1_Medium();
                *pTmpRegCount = (uint32)playout->R0;
            }
                break;
            case Js::LargeLayout:
            {LOGMEIN("ProbeContainer.cpp] 767\n");
                const unaligned Js::OpLayoutReg1_Large * playout = reader->Reg1_Large();
                *pTmpRegCount = (uint32)playout->R0;
            }
                break;
            default:
                Assert(false);
                __assume(false);
            }
            return true;
        }
        return false;
    }

    // The logic below makes use of number of tmp (temp) registers of A and B.
    // Set next statement is not allowed.
    // if numberOfTmpReg(A) < numberOfTmpReg(B)
    // or if any statement between A and B has number of tmpReg more than the lowest found.
    //
    // Get the temp register count for the A
    // This is a base and will store the lowest tmp reg count we have got yet, while walking the skipped statements.
    bool ProbeContainer::IsTmpRegCountIncreased(Js::FunctionBody* functionBody, ByteCodeReader* reader, int currentOffset, int nextStmOffset, bool restoreOffset)
    {LOGMEIN("ProbeContainer.cpp] 789\n");
        Js::FunctionBody::StatementMapList* pStatementMaps = functionBody->GetStatementMaps();
        Assert(pStatementMaps && pStatementMaps->Count() > 0);

        int direction = currentOffset < nextStmOffset ? 1 : -1;
        int startIndex = functionBody->GetEnclosingStatementIndexFromByteCode(currentOffset, true);
        uint32 tmpRegCountLowest = 0;

        // In the native code-gen (or interpreter which created from bailout points) the EmitTmpRegCount is not handled,
        // so lets calculate it by going through all statements backward from the current offset
        int index = startIndex;
        for (; index > 0; index--)
        {LOGMEIN("ProbeContainer.cpp] 801\n");
            Js::FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);
            Js::OpCode op;
            if (!pStatementMap->isSubexpression && FetchTmpRegCount(functionBody, reader, pStatementMap->byteCodeSpan.begin, &tmpRegCountLowest, &op))
            {LOGMEIN("ProbeContainer.cpp] 805\n");
                break;
            }
        }

        // Reset to the current offset.
        reader->SetCurrentOffset(currentOffset);

        uint32 tmpRegCountOnNext = tmpRegCountLowest; // Will fetch the tmp reg count till the B and skipped statements.
        Assert(startIndex != -1);
        index = startIndex + direction;
        while (index > 0 && index < pStatementMaps->Count())
        {LOGMEIN("ProbeContainer.cpp] 817\n");
            Js::FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);
            if (pStatementMap->isSubexpression)
            {LOGMEIN("ProbeContainer.cpp] 820\n");
                index += direction;
                continue;
            }

            if (direction == 1) // NOTE: Direction & corresponding condition
            {LOGMEIN("ProbeContainer.cpp] 826\n");
                if (nextStmOffset < pStatementMap->byteCodeSpan.begin) // check only till nextstatement offset
                {LOGMEIN("ProbeContainer.cpp] 828\n");
                    break;
                }
            }

            Js::OpCode op;
            FetchTmpRegCount(functionBody, reader, pStatementMap->byteCodeSpan.begin, &tmpRegCountOnNext, &op);

            if (tmpRegCountOnNext < tmpRegCountLowest)
            {LOGMEIN("ProbeContainer.cpp] 837\n");
                tmpRegCountLowest = tmpRegCountOnNext;
            }

            // On the reverse direction stop only when we find the tmpRegCount info for the setnext or below.
            if (direction == -1 && (op == Js::OpCode::EmitTmpRegCount))
            {LOGMEIN("ProbeContainer.cpp] 843\n");
                if (nextStmOffset >= pStatementMap->byteCodeSpan.begin)
                {LOGMEIN("ProbeContainer.cpp] 845\n");
                    break;
                }
            }
            index += direction;
        }

        // On the reverse way if we have reached the first statement, then our tmpRegCountOnNext is 0.
        if (direction == -1 && index == 0)
        {LOGMEIN("ProbeContainer.cpp] 854\n");
            tmpRegCountOnNext = 0;
        }

        if (restoreOffset)
        {LOGMEIN("ProbeContainer.cpp] 859\n");
            // Restore back the original IP.
            reader->SetCurrentOffset(currentOffset);
        }

        return (tmpRegCountOnNext > tmpRegCountLowest);
    }

    bool ProbeContainer::AdvanceToNextUserStatement(Js::FunctionBody* functionBody, ByteCodeReader* reader)
    {LOGMEIN("ProbeContainer.cpp] 868\n");
        // Move back a byte to make sure we are within the bounds of
        // our current statement (See DispatchExceptionBreakpoint)
        int currentOffset = reader->GetCurrentOffset() - 1;
        int nextStatementOffset;

        if (this->GetNextUserStatementOffsetForAdvance(functionBody, reader, currentOffset, &nextStatementOffset))
        {LOGMEIN("ProbeContainer.cpp] 875\n");
            reader->SetCurrentOffset(nextStatementOffset);
            return true;
        }
        return false;
    }

    void ProbeContainer::SetNextStatementAt(int _bytecodeOffset)
    {LOGMEIN("ProbeContainer.cpp] 883\n");
        Assert(_bytecodeOffset != debugManager->pCurrentInterpreterLocation->GetCurrentOffset());
        this->bytecodeOffset = _bytecodeOffset;

        Assert(IsNextStatementChanged == false);
        this->IsNextStatementChanged = true;
    }

    void ProbeContainer::AsyncActivate(HaltCallback* haltCallback)
    {LOGMEIN("ProbeContainer.cpp] 892\n");
        OUTPUT_TRACE(Js::DebuggerPhase, _u("Async break activated\n"));
        InterlockedExchangePointer((PVOID*)&this->pAsyncHaltCallback, haltCallback);

        Assert(debugManager);
        debugManager->asyncBreakController.Activate(haltCallback);
    }

    void ProbeContainer::AsyncDeactivate()
    {LOGMEIN("ProbeContainer.cpp] 901\n");
        InterlockedExchangePointer((PVOID*)&this->pAsyncHaltCallback, nullptr);

        Assert(debugManager);
        debugManager->asyncBreakController.Deactivate();
    }

    bool ProbeContainer::IsAsyncActivate() const
    {LOGMEIN("ProbeContainer.cpp] 909\n");
        return this->pAsyncHaltCallback != nullptr;
    }

    void ProbeContainer::PrepDiagForEnterScript()
    {LOGMEIN("ProbeContainer.cpp] 914\n");
        // This will be called from ParseScriptText.
        // This is to ensure the every script will call EnterScript back to host once, in-order to synchronize PDM with document.
        Assert(this->pScriptContext);
        if (this->pScriptContext->IsScriptContextInDebugMode())
        {LOGMEIN("ProbeContainer.cpp] 919\n");
            isForcedToEnterScriptStart = true;
        }
    }

    void ProbeContainer::RegisterContextToDiag(DWORD_PTR context, ArenaAllocator *alloc)
    {LOGMEIN("ProbeContainer.cpp] 925\n");
        Assert(this->pScriptContext->IsScriptContextInSourceRundownOrDebugMode());
        Assert(alloc);

        if (registeredFuncContextList == nullptr)
        {LOGMEIN("ProbeContainer.cpp] 930\n");
            registeredFuncContextList = JsUtil::List<DWORD_PTR, ArenaAllocator>::New(alloc);
        }

        registeredFuncContextList->Add(context);
    }

    bool ProbeContainer::IsContextRegistered(DWORD_PTR context)
    {LOGMEIN("ProbeContainer.cpp] 938\n");
        return registeredFuncContextList != nullptr && registeredFuncContextList->Contains(context);
    }

    FunctionBody * ProbeContainer::GetGlobalFunc(ScriptContext* scriptContext, DWORD_PTR secondaryHostSourceContext)
    {LOGMEIN("ProbeContainer.cpp] 943\n");
        return scriptContext->FindFunction([&secondaryHostSourceContext] (FunctionBody* pFunc) {
            return ((pFunc->GetSecondaryHostSourceContext() == secondaryHostSourceContext) &&
                     pFunc->GetIsGlobalFunc());
        });
    }

    bool ProbeContainer::HasAllowedForException(__in JavascriptExceptionObject* exceptionObject)
    {LOGMEIN("ProbeContainer.cpp] 951\n");
        // We do not want to break on internal exception.
        if (isThrowInternal)
        {LOGMEIN("ProbeContainer.cpp] 954\n");
            return false;
        }

        bool fIsFirstChance = false;
        bool fHasAllowed = false;
        bool fIsInNonUserCode = false;

        if (this->IsExceptionReportingEnabled() && (debugManager != nullptr))
        {LOGMEIN("ProbeContainer.cpp] 963\n");
            fHasAllowed = !debugManager->pThreadContext->HasCatchHandler();
            if (!fHasAllowed)
            {LOGMEIN("ProbeContainer.cpp] 966\n");
                if (IsFirstChanceExceptionEnabled())
                {LOGMEIN("ProbeContainer.cpp] 968\n");
                    fHasAllowed = fIsFirstChance = true;
                }

                // We must determine if the exception is in user code AND if it's first chance as some debuggers
                // ask for both and filter later.

                // first validate if the throwing function is NonUserCode function, if not then verify if the exception is being caught in nonuser code.
                if (exceptionObject && exceptionObject->GetFunctionBody() != nullptr && !exceptionObject->GetFunctionBody()->IsNonUserCode())
                {LOGMEIN("ProbeContainer.cpp] 977\n");
                    fIsInNonUserCode = IsNonUserCodeSupportEnabled() && !debugManager->pThreadContext->IsUserCode();
                }

                if (!fHasAllowed)
                {LOGMEIN("ProbeContainer.cpp] 982\n");
                    fHasAllowed = fIsInNonUserCode;
                }
            }
        }

        if (exceptionObject)
        {LOGMEIN("ProbeContainer.cpp] 989\n");
            exceptionObject->SetIsFirstChance(fIsFirstChance);
            exceptionObject->SetIsExceptionCaughtInNonUserCode(fIsInNonUserCode);
        }

        return fHasAllowed;
    }

    bool ProbeContainer::IsExceptionReportingEnabled()
    {LOGMEIN("ProbeContainer.cpp] 998\n");
        return this->debuggerOptionsCallback == nullptr || this->debuggerOptionsCallback->IsExceptionReportingEnabled();
    }

    bool ProbeContainer::IsFirstChanceExceptionEnabled()
    {LOGMEIN("ProbeContainer.cpp] 1003\n");
        return this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsFirstChanceExceptionEnabled();
    }

    // Mentions if the debugger has enabled the support to differentiate the exception kind.
    bool ProbeContainer::IsNonUserCodeSupportEnabled()
    {LOGMEIN("ProbeContainer.cpp] 1009\n");
        return this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsNonUserCodeSupportEnabled();
    }

    // Mentions if the debugger has enabled the support to display library stack frame.
    bool ProbeContainer::IsLibraryStackFrameSupportEnabled()
    {LOGMEIN("ProbeContainer.cpp] 1015\n");
        return CONFIG_FLAG(LibraryStackFrameDebugger) || (this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsLibraryStackFrameSupportEnabled());
    }

    void ProbeContainer::PinPropertyRecord(const Js::PropertyRecord *propertyRecord)
    {LOGMEIN("ProbeContainer.cpp] 1020\n");
        Assert(propertyRecord);
        this->pinnedPropertyRecords->Add(propertyRecord);
    }
#ifdef ENABLE_MUTATION_BREAKPOINT
    bool ProbeContainer::HasMutationBreakpoints()
    {LOGMEIN("ProbeContainer.cpp] 1026\n");
        return mutationBreakpointList && !mutationBreakpointList->Empty();
    }

    void ProbeContainer::InsertMutationBreakpoint(MutationBreakpoint *mutationBreakpoint)
    {LOGMEIN("ProbeContainer.cpp] 1031\n");
        Assert(mutationBreakpoint);

        RecyclerWeakReference<Js::MutationBreakpoint>* weakBp = nullptr;
        pScriptContext->GetRecycler()->FindOrCreateWeakReferenceHandle(mutationBreakpoint, &weakBp);
        Assert(weakBp);

        // Make sure list is created prior to insertion
        InitMutationBreakpointListIfNeeded();
        if (mutationBreakpointList->Contains(weakBp))
        {LOGMEIN("ProbeContainer.cpp] 1041\n");
            return;
        }
        mutationBreakpointList->Add(weakBp);
    }

    void ProbeContainer::ClearMutationBreakpoints()
    {LOGMEIN("ProbeContainer.cpp] 1048\n");
        mutationBreakpointList->Map([=](uint i, RecyclerWeakReference<Js::MutationBreakpoint>* weakBp) {
            if (mutationBreakpointList->IsItemValid(i))
            {LOGMEIN("ProbeContainer.cpp] 1051\n");
                Js::MutationBreakpoint* mutationBreakpoint = weakBp->Get();
                if (mutationBreakpoint)
                {LOGMEIN("ProbeContainer.cpp] 1054\n");
                    mutationBreakpoint->Reset();
                }
            }
        });
        mutationBreakpointList->ClearAndZero();
    }

    void ProbeContainer::InitMutationBreakpointListIfNeeded()
    {LOGMEIN("ProbeContainer.cpp] 1063\n");
        if (!mutationBreakpointList && Js::MutationBreakpoint::IsFeatureEnabled(pScriptContext))
        {LOGMEIN("ProbeContainer.cpp] 1065\n");
            Recycler *recycler = pScriptContext->GetRecycler();
            mutationBreakpointList.Root(RecyclerNew(recycler, MutationBreakpointList, recycler), recycler);
        }
    }

    void ProbeContainer::RemoveMutationBreakpointListIfNeeded()
    {LOGMEIN("ProbeContainer.cpp] 1072\n");
        if (mutationBreakpointList)
        {LOGMEIN("ProbeContainer.cpp] 1074\n");
            if (HasMutationBreakpoints())
            {LOGMEIN("ProbeContainer.cpp] 1076\n");
                ClearMutationBreakpoints();
            }
            else
            {
                mutationBreakpointList->ClearAndZero();
            }
            mutationBreakpointList.Unroot(pScriptContext->GetRecycler());
        }
    }
#endif
} // namespace Js.
