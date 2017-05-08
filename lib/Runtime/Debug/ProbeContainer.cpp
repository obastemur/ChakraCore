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
    {TRACE_IT(43218);
    }

    ProbeContainer::~ProbeContainer()
    {TRACE_IT(43219);
        this->Close();
    }

    void ProbeContainer::Close()
    {TRACE_IT(43220);
        // Probe manager instance may go down early.
        if (this->pScriptContext)
        {TRACE_IT(43221);
            debugManager = this->pScriptContext->GetThreadContext()->GetDebugManager();
        }
        else
        {TRACE_IT(43222);
            debugManager = nullptr;
        }
        if (debugManager != nullptr && debugManager->stepController.pActivatedContext == pScriptContext)
        {TRACE_IT(43223);
            debugManager->stepController.Deactivate();
        }
#ifdef ENABLE_MUTATION_BREAKPOINT
        this->RemoveMutationBreakpointListIfNeeded();
#endif
        pScriptContext = nullptr;
        debugManager = nullptr;
    }

    void ProbeContainer::Initialize(ScriptContext* pScriptContext)
    {TRACE_IT(43224);
        if (!diagProbeList)
        {TRACE_IT(43225);
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
    {TRACE_IT(43226);
        this->debugManager->stepController.StartRecordingCall();
    }

    void ProbeContainer::EndRecordingCall(Js::Var returnValue, Js::JavascriptFunction * function)
    {TRACE_IT(43227);
        this->debugManager->stepController.EndRecordingCall(returnValue, function);
    }

    ReturnedValueList* ProbeContainer::GetReturnedValueList() const
    {TRACE_IT(43228);
        return this->debugManager->stepController.GetReturnedValueList();
    }

    void ProbeContainer::ResetReturnedValueList()
    {TRACE_IT(43229);
        this->debugManager->stepController.ResetReturnedValueList();
    }

    void ProbeContainer::UpdateFramePointers(bool fMatchWithCurrentScriptContext, DWORD_PTR dispatchHaltFrameAddress)
    {TRACE_IT(43230);
        ArenaAllocator* pDiagArena = debugManager->GetDiagnosticArena()->Arena();
        framePointers = Anew(pDiagArena, DiagStack, pDiagArena);

        JavascriptStackWalker walker(pScriptContext, !fMatchWithCurrentScriptContext, nullptr/*returnAddress*/, true/*forceFullWalk*/);
        DiagStack* tempFramePointers = Anew(pDiagArena, DiagStack, pDiagArena);
        const bool isLibraryFrameEnabledDebugger = IsLibraryStackFrameSupportEnabled();

        walker.WalkUntil([&](JavascriptFunction* func, ushort frameIndex) -> bool
        {
            if (isLibraryFrameEnabledDebugger || !func->IsLibraryCode())
            {TRACE_IT(43231);
                DiagStackFrame* frm = nullptr;
                InterpreterStackFrame *interpreterFrame = walker.GetCurrentInterpreterFrame();
                ScriptContext* frameScriptContext = walker.GetCurrentScriptContext();
                Assert(frameScriptContext);

                if (!fMatchWithCurrentScriptContext && !frameScriptContext->IsScriptContextInDebugMode() && tempFramePointers->Count() == 0)
                {TRACE_IT(43232);
                    // this means the top frame is not in the debug mode. We shouldn't be stopping for this break.
                    // This could happen if the exception happens on the diagnosticsScriptEngine.
                    return true;
                }

                // Ignore frames which are not in debug mode, which can happen when diag engine calls into user engine under debugger
                // -- topmost frame is under debugger but some frames could be in non-debug mode as they are from diag engine.
                if (frameScriptContext->IsScriptContextInDebugMode() &&
                    (!fMatchWithCurrentScriptContext || frameScriptContext == pScriptContext))
                {TRACE_IT(43233);
                    if (interpreterFrame)
                    {TRACE_IT(43234);
                        if (dispatchHaltFrameAddress == 0 || interpreterFrame->GetStackAddress() > dispatchHaltFrameAddress)
                        {TRACE_IT(43235);
                            frm = Anew(pDiagArena, DiagInterpreterStackFrame, interpreterFrame);
                        }
                    }
                    else
                    {TRACE_IT(43236);
                        void* stackAddress = walker.GetCurrentArgv();
                        if (dispatchHaltFrameAddress == 0 || reinterpret_cast<DWORD_PTR>(stackAddress) > dispatchHaltFrameAddress)
                        {TRACE_IT(43237);
#if ENABLE_NATIVE_CODEGEN
                            if (func->IsScriptFunction())
                            {TRACE_IT(43238);
                                frm = Anew(pDiagArena, DiagNativeStackFrame,
                                    ScriptFunction::FromVar(walker.GetCurrentFunction()), walker.GetByteCodeOffset(), stackAddress, walker.GetCurrentCodeAddr());
                            }
                            else
#else
                            Assert(!func->IsScriptFunction());
#endif
                            {TRACE_IT(43239);
                                frm = Anew(pDiagArena, DiagRuntimeStackFrame, func, walker.GetCurrentNativeLibraryEntryName(), stackAddress);
                            }
                        }
                    }
                }

                if (frm)
                {TRACE_IT(43240);
                    tempFramePointers->Push(frm);
                }
            }

            return false;
        });

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::UpdateFramePointers: detected %d frames (this=%p, fMatchWithCurrentScriptContext=%d)\n"),
            tempFramePointers->Count(), this, fMatchWithCurrentScriptContext);

        while (tempFramePointers->Count())
        {TRACE_IT(43241);
            framePointers->Push(tempFramePointers->Pop());
        }
    }

    WeakDiagStack * ProbeContainer::GetFramePointers(DWORD_PTR dispatchHaltFrameAddress)
    {TRACE_IT(43242);
        if (framePointers == nullptr || this->debugSessionNumber < debugManager->GetDebugSessionNumber())
        {
            UpdateFramePointers(/*fMatchWithCurrentScriptContext*/true, dispatchHaltFrameAddress);
            this->debugSessionNumber = debugManager->GetDebugSessionNumber();

            if ((framePointers->Count() > 0) &&
                debugManager->IsMatchTopFrameStackAddress(framePointers->Peek(0)))
            {TRACE_IT(43243);
                framePointers->Peek(0)->SetIsTopFrame();
            }
        }

        ReferencedArenaAdapter* pRefArena = debugManager->GetDiagnosticArena();
        return HeapNew(WeakDiagStack,pRefArena,framePointers);
    }

    bool ProbeContainer::InitializeLocation(InterpreterHaltState* pHaltState, bool fMatchWithCurrentScriptContext)
    {TRACE_IT(43244);
        Assert(debugManager);
        debugManager->SetCurrentInterpreterLocation(pHaltState);

        ArenaAllocator* pDiagArena = debugManager->GetDiagnosticArena()->Arena();

        UpdateFramePointers(fMatchWithCurrentScriptContext);
        pHaltState->framePointers = framePointers;
        pHaltState->stringBuilder = Anew(pDiagArena, StringBuilder<ArenaAllocator>, pDiagArena);

        if (pHaltState->framePointers->Count() > 0)
        {TRACE_IT(43245);
            pHaltState->topFrame = pHaltState->framePointers->Peek(0);
            pHaltState->topFrame->SetIsTopFrame();
        }

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::InitializeLocation (end): this=%p, pHaltState=%p, fMatch=%d, topFrame=%p\n"),
            this, pHaltState, fMatchWithCurrentScriptContext, pHaltState->topFrame);

        return true;
    }

    void ProbeContainer::DestroyLocation()
    {TRACE_IT(43246);
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DestroyLocation (start): this=%p, IsNextStatementChanged=%d, haltCallbackProbe=%p\n"),
            this, this->IsNextStatementChanged, haltCallbackProbe);

        if (IsNextStatementChanged)
        {TRACE_IT(43247);
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
        {TRACE_IT(43248);
            // The clean up is called here to scriptengine's object to remove all DebugStackFrames
            haltCallbackProbe->CleanupHalt();
        }
    }

    bool ProbeContainer::CanDispatchHalt(InterpreterHaltState* pHaltState)
    {TRACE_IT(43249);
        if (!haltCallbackProbe || haltCallbackProbe->IsInClosedState() || debugManager->IsAtDispatchHalt())
        {TRACE_IT(43250);
            OUTPUT_VERBOSE_TRACE(Js::DebuggerPhase, _u("ProbeContainer::CanDispatchHalt: Not in break mode. pHaltState = %p\n"), pHaltState);
            return false;
        }
        return true;
    }

    void ProbeContainer::DispatchStepHandler(InterpreterHaltState* pHaltState, OpCode* pOriginalOpcode)
    {TRACE_IT(43251);
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchStepHandler: start: this=%p, pHaltState=%p, pOriginalOpcode=0x%x\n"), this, pHaltState, pOriginalOpcode);

        if (!CanDispatchHalt(pHaltState))
        {TRACE_IT(43252);
            return;
        }

        TryFinally([&]()
          {
            InitializeLocation(pHaltState);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchStepHandler: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n"),
                pHaltState, pHaltState->IsValid());

            if (pHaltState->IsValid()) // Only proceed if we find a valid top frame and that is the executing function
            {TRACE_IT(43253);
                if (debugManager->stepController.IsStepComplete(pHaltState, haltCallbackProbe, *pOriginalOpcode))
                {TRACE_IT(43254);
                    OpCode oldOpcode = *pOriginalOpcode;
                    pHaltState->GetFunction()->ProbeAtOffset(pHaltState->GetCurrentOffset(), pOriginalOpcode);
                    pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                    debugManager->stepController.Deactivate(pHaltState);
                    haltCallbackProbe->DispatchHalt(pHaltState);

                    if (oldOpcode == OpCode::Break && debugManager->stepController.stepType == STEP_DOCUMENT)
                    {TRACE_IT(43255);
                        // That means we have delivered the stepping to the debugger, where we had the breakpoint
                        // already, however it is possible that debugger can initiate the step_document. In that
                        // case debugger did not break due to break. So we have break as a breakpoint reason.
                        *pOriginalOpcode = OpCode::Break;
                    }
                    else if (OpCode::Break == *pOriginalOpcode)
                    {TRACE_IT(43256);
                        debugManager->stepController.stepCompleteOnInlineBreakpoint = true;
                    }
                }
            }
          },
          [&](bool) 
          {TRACE_IT(43257);
            DestroyLocation();
          });

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchStepHandler: end: pHaltState=%p\n"), pHaltState);
    }

    void ProbeContainer::DispatchAsyncBreak(InterpreterHaltState* pHaltState)
    {TRACE_IT(43258);
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchAsyncBreak: start: this=%p, pHaltState=%p\n"), this, pHaltState);

        if (!this->pAsyncHaltCallback || !CanDispatchHalt(pHaltState))
        {TRACE_IT(43259);
            return;
        }

        TryFinally([&]()
        {
            InitializeLocation(pHaltState, /* We don't need to match script context, stop at any available script function */ false);
            OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchAsyncBreak: initialized location: pHaltState=%p, pHaltState->IsValid()=%d\n"),
                pHaltState, pHaltState->IsValid());

            if (pHaltState->IsValid())
            {TRACE_IT(43260);
                // Activate the current haltCallback with asyncStepController.
                debugManager->asyncBreakController.Activate(this->pAsyncHaltCallback);
                if (debugManager->asyncBreakController.IsAtStoppingLocation(pHaltState))
                {TRACE_IT(43261);
                    OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchAsyncBreak: IsAtStoppingLocation: pHaltState=%p\n"), pHaltState);

                    pHaltState->GetFunction()->CheckAndRegisterFuncToDiag(pScriptContext);

                    debugManager->stepController.Deactivate(pHaltState);
                    debugManager->asyncBreakController.DispatchAndReset(pHaltState);
                }
            }
        },
        [&](bool)
        {TRACE_IT(43262);
            DestroyLocation();
        });

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchAsyncBreak: end: pHaltState=%p\n"), pHaltState);
    }

    void ProbeContainer::DispatchInlineBreakpoint(InterpreterHaltState* pHaltState)
    {TRACE_IT(43263);
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchInlineBreakpoint: start: this=%p, pHaltState=%p\n"), this, pHaltState);

        if (!CanDispatchHalt(pHaltState))
        {TRACE_IT(43264);
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
            {TRACE_IT(43265);
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
        {TRACE_IT(43266);
            DestroyLocation();
        });
        
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchInlineBreakpoint: end: pHaltState=%p\n"), pHaltState);
    }

    bool ProbeContainer::DispatchExceptionBreakpoint(InterpreterHaltState* pHaltState)
    {TRACE_IT(43267);
        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchExceptionBreakpoint: start: this=%p, pHaltState=%p\n"), this, pHaltState);
        bool fSuccess = false;
        if (!haltCallbackProbe || haltCallbackProbe->IsInClosedState() || debugManager->IsAtDispatchHalt())
        {TRACE_IT(43268);
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
            {TRACE_IT(43269);
#if DBG
                pHaltState->GetFunction()->MustBeInDebugMode();
#endif

                // For interpreter frames, change the current location pointer of bytecode block, as it might be pointing to the next statement on the body.
                // In order to generated proper binding of break on exception to the statement, the bytecode offset needed to be on the same span
                // of the statement.
                // For native frames the offset is always current.
                // Move back a single byte to ensure that it falls under on the same statement.
                if (pHaltState->topFrame->IsInterpreterFrame())
                {TRACE_IT(43270);
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
                {TRACE_IT(43271);
                    OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchExceptionBreakpoint: top function's context is different from the current context: pHaltState=%p, haltCallbackProbe=%p\n"),
                        pHaltState, pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe);
                    if (pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe)
                    {TRACE_IT(43272);
                        pTopFuncContext->GetDebugContext()->GetProbeContainer()->haltCallbackProbe->DispatchHalt(pHaltState);
                        fSuccess = true;
                    }
                }
                else
                {TRACE_IT(43273);
                    haltCallbackProbe->DispatchHalt(pHaltState);
                    fSuccess = true;
                }
            }
        },
        [&](bool)
        {TRACE_IT(43274);
            // If the next statement has changed, we need to log that to exception object so that it will not try to advance to next statement again.
            pHaltState->exceptionObject->SetIgnoreAdvanceToNextStatement(IsNextStatementChanged);

            // Restore the current offset;
            if (currentOffset != -1 && pHaltState->topFrame->IsInterpreterFrame())
            {TRACE_IT(43275);
                pHaltState->SetCurrentOffset(currentOffset);
            }

            DestroyLocation();
        });

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchExceptionBreakpoint: end: pHaltState=%p, fSuccess=%d\n"), pHaltState, fSuccess);
        return fSuccess;
    }

    void ProbeContainer::DispatchMutationBreakpoint(InterpreterHaltState* pHaltState)
    {TRACE_IT(43276);
        Assert(pHaltState->stopType == STOP_MUTATIONBREAKPOINT);

        OUTPUT_TRACE(Js::DebuggerPhase, _u("ProbeContainer::DispatchMutationBreakpoint: start: this=%p, pHaltState=%p\n"), this, pHaltState);
        if (!CanDispatchHalt(pHaltState))
        {TRACE_IT(43277);
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
            {TRACE_IT(43278);
                // For interpreter frames, change the current location pointer of bytecode block, as it might be pointing to the next statement on the body.
                // In order to generated proper binding of mutation statement, the bytecode offset needed to be on the same span of the statement.
                // For native frames the offset is always current.
                // Move back a single byte to ensure that it falls under on the same statement.
                if (pHaltState->topFrame->IsInterpreterFrame())
                {TRACE_IT(43279);
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
        {TRACE_IT(43280);
            // Restore the current offset;
            if (currentOffset != -1 && pHaltState->topFrame->IsInterpreterFrame())
            {TRACE_IT(43281);
                pHaltState->SetCurrentOffset(currentOffset);
            }
            DestroyLocation();
        });

    }

    void ProbeContainer::DispatchProbeHandlers(InterpreterHaltState* pHaltState)
    {TRACE_IT(43282);
        if (!CanDispatchHalt(pHaltState))
        {TRACE_IT(43283);
            return;
        }

        TryFinally([&]()        
        {
            InitializeLocation(pHaltState);

            if (pHaltState->IsValid())
            {TRACE_IT(43284);
                Js::ProbeList * localPendingProbeList = this->pendingProbeList;
                diagProbeList->Map([pHaltState, localPendingProbeList](int index, Probe * probe)
                {
                    if (probe->CanHalt(pHaltState))
                    {TRACE_IT(43285);
                        localPendingProbeList->Add(probe);
                    }
                });

                if (localPendingProbeList->Count() != 0)
                {TRACE_IT(43286);
                    localPendingProbeList->MapUntil([&](int index, Probe * probe)
                    {
                        if (haltCallbackProbe && !haltCallbackProbe->IsInClosedState())
                        {TRACE_IT(43287);
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
        {TRACE_IT(43288);
            pendingProbeList->Clear();
            DestroyLocation();
        });
    }

    void ProbeContainer::UpdateStep(bool fDuringSetupDebugApp/*= false*/)
    {TRACE_IT(43289);
        // This function indicate that when the page is being refreshed and the last action we have done was stepping.
        // so update the state of the current stepController.
        if (debugManager)
        {TRACE_IT(43290);
            // Usually we need to be in debug mode to UpdateStep. But during setting up new engine to debug mode we have an
            // ordering issue and the new engine will enter debug mode after this. So allow non-debug mode if fDuringSetupDebugApp.
            AssertMsg(fDuringSetupDebugApp || (pScriptContext && pScriptContext->IsScriptContextInDebugMode()), "Why UpdateStep when we are not in debug mode?");
            debugManager->stepController.stepType = STEP_IN;
        }
    }

    void ProbeContainer::DeactivateStep()
    {TRACE_IT(43291);
        if (debugManager)
        {TRACE_IT(43292);
            debugManager->stepController.stepType = STEP_NONE;
        }
    }

    void ProbeContainer::InitializeInlineBreakEngine(HaltCallback* probe)
    {TRACE_IT(43293);
        AssertMsg(!haltCallbackProbe || probe == haltCallbackProbe, "Overwrite of Inline bp probe with different probe");
        haltCallbackProbe = probe;
    }

    void ProbeContainer::UninstallInlineBreakpointProbe(HaltCallback* probe)
    {TRACE_IT(43294);
        haltCallbackProbe = nullptr;
    }

    void ProbeContainer::InitializeDebuggerScriptOptionCallback(DebuggerOptionsCallback* debuggerOptionsCallback)
    {TRACE_IT(43295);
        Assert(this->debuggerOptionsCallback == nullptr);
        this->debuggerOptionsCallback = debuggerOptionsCallback;
    }

    void ProbeContainer::UninstallDebuggerScriptOptionCallback()
    {TRACE_IT(43296);
        this->debuggerOptionsCallback = nullptr;
    }

    void ProbeContainer::AddProbe(Probe* pProbe)
    {TRACE_IT(43297);
        if (pProbe->Install(nullptr))
        {TRACE_IT(43298);
            diagProbeList->Add(pProbe);
        }
    }

    void ProbeContainer::RemoveProbe(Probe* pProbe)
    {TRACE_IT(43299);
        if (pProbe->Uninstall(nullptr))
        {TRACE_IT(43300);
            diagProbeList->Remove(pProbe);
        }
    }

    void ProbeContainer::RemoveAllProbes()
    {TRACE_IT(43301);
#ifdef ENABLE_MUTATION_BREAKPOINT
        if (HasMutationBreakpoints())
        {TRACE_IT(43302);
            ClearMutationBreakpoints();
        }
#endif
        for (int i = 0; i < diagProbeList->Count(); i++)
        {TRACE_IT(43303);
            diagProbeList->Item(i)->Uninstall(nullptr);
        }
        diagProbeList->Clear();
    }

    // Retrieves the offset of next statement in JavaScript user code for advancing from current statement
    // (normal flow-control is respected).
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetForAdvance(Js::FunctionBody* functionBody, ByteCodeReader* reader, int currentOffset, int* nextStatementOffset)
    {TRACE_IT(43304);
        int originalCurrentOffset = currentOffset;
        while (GetNextUserStatementOffsetHelper(functionBody, currentOffset, FunctionBody::SAT_FromCurrentToNext, nextStatementOffset))
        {TRACE_IT(43305);
            Js::DebuggerScope *debuggerScope = functionBody->GetDiagCatchScopeObjectAt(currentOffset);
            if (debuggerScope != nullptr && !debuggerScope->IsOffsetInScope(*nextStatementOffset))
            {TRACE_IT(43306);
                // Our next statement is not within this catch block, So we cannot just jump to it, we need to return false so the stack unwind will happen.
                return false;
            }

            Assert(currentOffset < *nextStatementOffset);

            if (IsTmpRegCountIncreased(functionBody, reader, originalCurrentOffset, *nextStatementOffset, true /*restoreOffset*/))
            {TRACE_IT(43307);
                currentOffset = *nextStatementOffset;
            }
            else
            {TRACE_IT(43308);
                return true;
            }
        }

        return false;
    }

    // Retrieves the offset of beginning of next statement in JavaScript user code for explicit set next statement
    // (normal flow-control is not respected, just get start next statement).
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetForSetNext(Js::FunctionBody* functionBody, int currentOffset, int* nextStatementOffset)
    {TRACE_IT(43309);
        return GetNextUserStatementOffsetHelper(functionBody, currentOffset, FunctionBody::SAT_NextStatementStart, nextStatementOffset);
    }

    // Retrieves the offset of beginning of next statement in JavaScript user code for scenario specified by adjType.
    // Returns true on success, false if it's not possible to get next statement for advance from current.
    bool ProbeContainer::GetNextUserStatementOffsetHelper(
        Js::FunctionBody* functionBody, int currentOffset, FunctionBody::StatementAdjustmentType adjType, int* nextStatementOffset)
    {TRACE_IT(43310);
        Assert(functionBody);
        Assert(nextStatementOffset);

        FunctionBody::StatementMapList* pStatementMaps = functionBody->GetStatementMaps();
        if (pStatementMaps && pStatementMaps->Count() > 1)
        {TRACE_IT(43311);
            for (int index = 0; index < pStatementMaps->Count() - 1; index++)
            {TRACE_IT(43312);
                FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);

                if (!pStatementMap->isSubexpression && pStatementMap->byteCodeSpan.Includes(currentOffset))
                {TRACE_IT(43313);
                    int nextMapIndex = index;
                    FunctionBody::StatementMap* pNextStatementMap = Js::FunctionBody::GetNextNonSubexpressionStatementMap(pStatementMaps, ++nextMapIndex);
                    if (!pNextStatementMap)
                    {TRACE_IT(43314);
                        break;
                    }

                    // We are trying to find out the Branch opcode, between current and next statement. Skipping that would give use incorrect execution order.
                    FunctionBody::StatementAdjustmentRecord adjRecord;
                    if (pNextStatementMap->byteCodeSpan.begin > pStatementMap->byteCodeSpan.end &&
                        functionBody->GetBranchOffsetWithin(pStatementMap->byteCodeSpan.end, pNextStatementMap->byteCodeSpan.begin, &adjRecord) &&
                        (adjRecord.GetAdjustmentType() & adjType))
                    {TRACE_IT(43315);
                        Assert(adjRecord.GetByteCodeOffset() > (uint)pStatementMap->byteCodeSpan.end);
                        *nextStatementOffset = adjRecord.GetByteCodeOffset();
                    }
                    else
                    {TRACE_IT(43316);
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
    {TRACE_IT(43317);
        Assert(pTmpRegCount);
        Assert(pOp);

        Js::LayoutSize layoutSize;
        reader->SetCurrentOffset(atOffset);
        *pOp = reader->ReadOp(layoutSize);

        if (*pOp == Js::OpCode::Break)
        {TRACE_IT(43318);
            // User might have put breakpoint on the skipped or target statement, get the original opcode;
            if (functionBody->ProbeAtOffset(atOffset, pOp))
            {TRACE_IT(43319);
                if (Js::OpCodeUtil::IsPrefixOpcode(*pOp))
                {TRACE_IT(43320);
                    *pOp = reader->ReadPrefixedOp(layoutSize, *pOp);
                }
            }
        }

        if (*pOp == Js::OpCode::EmitTmpRegCount)
        {TRACE_IT(43321);
            switch (layoutSize)
            {
            case Js::SmallLayout:
            {TRACE_IT(43322);
                const unaligned Js::OpLayoutReg1_Small * playout = reader->Reg1_Small();
                *pTmpRegCount = (uint32)playout->R0;
            }
                break;
            case Js::MediumLayout:
            {TRACE_IT(43323);
                const unaligned Js::OpLayoutReg1_Medium * playout = reader->Reg1_Medium();
                *pTmpRegCount = (uint32)playout->R0;
            }
                break;
            case Js::LargeLayout:
            {TRACE_IT(43324);
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
    {TRACE_IT(43325);
        Js::FunctionBody::StatementMapList* pStatementMaps = functionBody->GetStatementMaps();
        Assert(pStatementMaps && pStatementMaps->Count() > 0);

        int direction = currentOffset < nextStmOffset ? 1 : -1;
        int startIndex = functionBody->GetEnclosingStatementIndexFromByteCode(currentOffset, true);
        uint32 tmpRegCountLowest = 0;

        // In the native code-gen (or interpreter which created from bailout points) the EmitTmpRegCount is not handled,
        // so lets calculate it by going through all statements backward from the current offset
        int index = startIndex;
        for (; index > 0; index--)
        {TRACE_IT(43326);
            Js::FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);
            Js::OpCode op;
            if (!pStatementMap->isSubexpression && FetchTmpRegCount(functionBody, reader, pStatementMap->byteCodeSpan.begin, &tmpRegCountLowest, &op))
            {TRACE_IT(43327);
                break;
            }
        }

        // Reset to the current offset.
        reader->SetCurrentOffset(currentOffset);

        uint32 tmpRegCountOnNext = tmpRegCountLowest; // Will fetch the tmp reg count till the B and skipped statements.
        Assert(startIndex != -1);
        index = startIndex + direction;
        while (index > 0 && index < pStatementMaps->Count())
        {TRACE_IT(43328);
            Js::FunctionBody::StatementMap* pStatementMap = pStatementMaps->Item(index);
            if (pStatementMap->isSubexpression)
            {TRACE_IT(43329);
                index += direction;
                continue;
            }

            if (direction == 1) // NOTE: Direction & corresponding condition
            {TRACE_IT(43330);
                if (nextStmOffset < pStatementMap->byteCodeSpan.begin) // check only till nextstatement offset
                {TRACE_IT(43331);
                    break;
                }
            }

            Js::OpCode op;
            FetchTmpRegCount(functionBody, reader, pStatementMap->byteCodeSpan.begin, &tmpRegCountOnNext, &op);

            if (tmpRegCountOnNext < tmpRegCountLowest)
            {TRACE_IT(43332);
                tmpRegCountLowest = tmpRegCountOnNext;
            }

            // On the reverse direction stop only when we find the tmpRegCount info for the setnext or below.
            if (direction == -1 && (op == Js::OpCode::EmitTmpRegCount))
            {TRACE_IT(43333);
                if (nextStmOffset >= pStatementMap->byteCodeSpan.begin)
                {TRACE_IT(43334);
                    break;
                }
            }
            index += direction;
        }

        // On the reverse way if we have reached the first statement, then our tmpRegCountOnNext is 0.
        if (direction == -1 && index == 0)
        {TRACE_IT(43335);
            tmpRegCountOnNext = 0;
        }

        if (restoreOffset)
        {TRACE_IT(43336);
            // Restore back the original IP.
            reader->SetCurrentOffset(currentOffset);
        }

        return (tmpRegCountOnNext > tmpRegCountLowest);
    }

    bool ProbeContainer::AdvanceToNextUserStatement(Js::FunctionBody* functionBody, ByteCodeReader* reader)
    {TRACE_IT(43337);
        // Move back a byte to make sure we are within the bounds of
        // our current statement (See DispatchExceptionBreakpoint)
        int currentOffset = reader->GetCurrentOffset() - 1;
        int nextStatementOffset;

        if (this->GetNextUserStatementOffsetForAdvance(functionBody, reader, currentOffset, &nextStatementOffset))
        {TRACE_IT(43338);
            reader->SetCurrentOffset(nextStatementOffset);
            return true;
        }
        return false;
    }

    void ProbeContainer::SetNextStatementAt(int _bytecodeOffset)
    {TRACE_IT(43339);
        Assert(_bytecodeOffset != debugManager->pCurrentInterpreterLocation->GetCurrentOffset());
        this->bytecodeOffset = _bytecodeOffset;

        Assert(IsNextStatementChanged == false);
        this->IsNextStatementChanged = true;
    }

    void ProbeContainer::AsyncActivate(HaltCallback* haltCallback)
    {TRACE_IT(43340);
        OUTPUT_TRACE(Js::DebuggerPhase, _u("Async break activated\n"));
        InterlockedExchangePointer((PVOID*)&this->pAsyncHaltCallback, haltCallback);

        Assert(debugManager);
        debugManager->asyncBreakController.Activate(haltCallback);
    }

    void ProbeContainer::AsyncDeactivate()
    {TRACE_IT(43341);
        InterlockedExchangePointer((PVOID*)&this->pAsyncHaltCallback, nullptr);

        Assert(debugManager);
        debugManager->asyncBreakController.Deactivate();
    }

    bool ProbeContainer::IsAsyncActivate() const
    {TRACE_IT(43342);
        return this->pAsyncHaltCallback != nullptr;
    }

    void ProbeContainer::PrepDiagForEnterScript()
    {TRACE_IT(43343);
        // This will be called from ParseScriptText.
        // This is to ensure the every script will call EnterScript back to host once, in-order to synchronize PDM with document.
        Assert(this->pScriptContext);
        if (this->pScriptContext->IsScriptContextInDebugMode())
        {TRACE_IT(43344);
            isForcedToEnterScriptStart = true;
        }
    }

    void ProbeContainer::RegisterContextToDiag(DWORD_PTR context, ArenaAllocator *alloc)
    {TRACE_IT(43345);
        Assert(this->pScriptContext->IsScriptContextInSourceRundownOrDebugMode());
        Assert(alloc);

        if (registeredFuncContextList == nullptr)
        {TRACE_IT(43346);
            registeredFuncContextList = JsUtil::List<DWORD_PTR, ArenaAllocator>::New(alloc);
        }

        registeredFuncContextList->Add(context);
    }

    bool ProbeContainer::IsContextRegistered(DWORD_PTR context)
    {TRACE_IT(43347);
        return registeredFuncContextList != nullptr && registeredFuncContextList->Contains(context);
    }

    FunctionBody * ProbeContainer::GetGlobalFunc(ScriptContext* scriptContext, DWORD_PTR secondaryHostSourceContext)
    {TRACE_IT(43348);
        return scriptContext->FindFunction([&secondaryHostSourceContext] (FunctionBody* pFunc) {
            return ((pFunc->GetSecondaryHostSourceContext() == secondaryHostSourceContext) &&
                     pFunc->GetIsGlobalFunc());
        });
    }

    bool ProbeContainer::HasAllowedForException(__in JavascriptExceptionObject* exceptionObject)
    {TRACE_IT(43349);
        // We do not want to break on internal exception.
        if (isThrowInternal)
        {TRACE_IT(43350);
            return false;
        }

        bool fIsFirstChance = false;
        bool fHasAllowed = false;
        bool fIsInNonUserCode = false;

        if (this->IsExceptionReportingEnabled() && (debugManager != nullptr))
        {TRACE_IT(43351);
            fHasAllowed = !debugManager->pThreadContext->HasCatchHandler();
            if (!fHasAllowed)
            {TRACE_IT(43352);
                if (IsFirstChanceExceptionEnabled())
                {TRACE_IT(43353);
                    fHasAllowed = fIsFirstChance = true;
                }

                // We must determine if the exception is in user code AND if it's first chance as some debuggers
                // ask for both and filter later.

                // first validate if the throwing function is NonUserCode function, if not then verify if the exception is being caught in nonuser code.
                if (exceptionObject && exceptionObject->GetFunctionBody() != nullptr && !exceptionObject->GetFunctionBody()->IsNonUserCode())
                {TRACE_IT(43354);
                    fIsInNonUserCode = IsNonUserCodeSupportEnabled() && !debugManager->pThreadContext->IsUserCode();
                }

                if (!fHasAllowed)
                {TRACE_IT(43355);
                    fHasAllowed = fIsInNonUserCode;
                }
            }
        }

        if (exceptionObject)
        {TRACE_IT(43356);
            exceptionObject->SetIsFirstChance(fIsFirstChance);
            exceptionObject->SetIsExceptionCaughtInNonUserCode(fIsInNonUserCode);
        }

        return fHasAllowed;
    }

    bool ProbeContainer::IsExceptionReportingEnabled()
    {TRACE_IT(43357);
        return this->debuggerOptionsCallback == nullptr || this->debuggerOptionsCallback->IsExceptionReportingEnabled();
    }

    bool ProbeContainer::IsFirstChanceExceptionEnabled()
    {TRACE_IT(43358);
        return this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsFirstChanceExceptionEnabled();
    }

    // Mentions if the debugger has enabled the support to differentiate the exception kind.
    bool ProbeContainer::IsNonUserCodeSupportEnabled()
    {TRACE_IT(43359);
        return this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsNonUserCodeSupportEnabled();
    }

    // Mentions if the debugger has enabled the support to display library stack frame.
    bool ProbeContainer::IsLibraryStackFrameSupportEnabled()
    {TRACE_IT(43360);
        return CONFIG_FLAG(LibraryStackFrameDebugger) || (this->debuggerOptionsCallback != nullptr && this->debuggerOptionsCallback->IsLibraryStackFrameSupportEnabled());
    }

    void ProbeContainer::PinPropertyRecord(const Js::PropertyRecord *propertyRecord)
    {TRACE_IT(43361);
        Assert(propertyRecord);
        this->pinnedPropertyRecords->Add(propertyRecord);
    }
#ifdef ENABLE_MUTATION_BREAKPOINT
    bool ProbeContainer::HasMutationBreakpoints()
    {TRACE_IT(43362);
        return mutationBreakpointList && !mutationBreakpointList->Empty();
    }

    void ProbeContainer::InsertMutationBreakpoint(MutationBreakpoint *mutationBreakpoint)
    {TRACE_IT(43363);
        Assert(mutationBreakpoint);

        RecyclerWeakReference<Js::MutationBreakpoint>* weakBp = nullptr;
        pScriptContext->GetRecycler()->FindOrCreateWeakReferenceHandle(mutationBreakpoint, &weakBp);
        Assert(weakBp);

        // Make sure list is created prior to insertion
        InitMutationBreakpointListIfNeeded();
        if (mutationBreakpointList->Contains(weakBp))
        {TRACE_IT(43364);
            return;
        }
        mutationBreakpointList->Add(weakBp);
    }

    void ProbeContainer::ClearMutationBreakpoints()
    {TRACE_IT(43365);
        mutationBreakpointList->Map([=](uint i, RecyclerWeakReference<Js::MutationBreakpoint>* weakBp) {
            if (mutationBreakpointList->IsItemValid(i))
            {TRACE_IT(43366);
                Js::MutationBreakpoint* mutationBreakpoint = weakBp->Get();
                if (mutationBreakpoint)
                {TRACE_IT(43367);
                    mutationBreakpoint->Reset();
                }
            }
        });
        mutationBreakpointList->ClearAndZero();
    }

    void ProbeContainer::InitMutationBreakpointListIfNeeded()
    {TRACE_IT(43368);
        if (!mutationBreakpointList && Js::MutationBreakpoint::IsFeatureEnabled(pScriptContext))
        {TRACE_IT(43369);
            Recycler *recycler = pScriptContext->GetRecycler();
            mutationBreakpointList.Root(RecyclerNew(recycler, MutationBreakpointList, recycler), recycler);
        }
    }

    void ProbeContainer::RemoveMutationBreakpointListIfNeeded()
    {TRACE_IT(43370);
        if (mutationBreakpointList)
        {TRACE_IT(43371);
            if (HasMutationBreakpoints())
            {TRACE_IT(43372);
                ClearMutationBreakpoints();
            }
            else
            {TRACE_IT(43373);
                mutationBreakpointList->ClearAndZero();
            }
            mutationBreakpointList.Unroot(pScriptContext->GetRecycler());
        }
    }
#endif
} // namespace Js.
