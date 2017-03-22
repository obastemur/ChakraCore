//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
#include "Language/JavascriptFunctionArgIndex.h"
#include "Language/InterpreterStackFrame.h"
#include "Library/StackScriptFunction.h"

namespace Js
{
    JavascriptFunction *
    StackScriptFunction::EnsureBoxed(BOX_PARAM(JavascriptFunction * function, void * returnAddress, char16 const * reason))
    {LOGMEIN("StackScriptFunction.cpp] 13\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
        char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif
        if (!ThreadContext::IsOnStack(function))
        {LOGMEIN("StackScriptFunction.cpp] 18\n");
            return function;
        }

        // Only script function can be on the stack
        StackScriptFunction * stackScriptFunction = StackScriptFunction::FromVar(function);
        ScriptFunction * boxedFunction = stackScriptFunction->boxedScriptFunction;
        if (boxedFunction != nullptr)
        {LOGMEIN("StackScriptFunction.cpp] 26\n");
            // We have already boxed this stack function before, and the function
            // wasn't on any slot or not a caller that we can replace.
            // Just give out the function we boxed before
            return boxedFunction;
        }

        PHASE_PRINT_TESTTRACE(Js::StackFuncPhase, function->GetFunctionProxy(),
            _u("StackScriptFunction (%s): box and disable stack function: %s (function %s)\n"),
            reason, function->GetFunctionProxy()->IsDeferredDeserializeFunction()?
            _u("<DeferDeserialize>") : function->GetParseableFunctionInfo()->GetDisplayName(),
            function->GetFunctionProxy()->GetDebugNumberSet(debugStringBuffer));

        // During the box workflow we reset all the parents of all nested functions and up. If a fault occurs when the stack function
        // is created this will cause further issues when trying to use the function object again. So failing faster seems to make more sense
        try
        {LOGMEIN("StackScriptFunction.cpp] 42\n");
           boxedFunction = StackScriptFunction::Box(stackScriptFunction, returnAddress);
        }
        catch (Js::OutOfMemoryException)
        {LOGMEIN("StackScriptFunction.cpp] 46\n");
           FailedToBox_OOM_fatal_error((ULONG_PTR)stackScriptFunction);
        }
        return boxedFunction;
    }

    JavascriptFunction *
    StackScriptFunction::GetCurrentFunctionObject(JavascriptFunction * function)
    {LOGMEIN("StackScriptFunction.cpp] 54\n");
        if (!ThreadContext::IsOnStack(function))
        {LOGMEIN("StackScriptFunction.cpp] 56\n");
            return function;
        }

        ScriptFunction * boxed = StackScriptFunction::FromVar(function)->boxedScriptFunction;
        return boxed ? boxed : function;
    }

    StackScriptFunction *
    StackScriptFunction::FromVar(Var var)
    {LOGMEIN("StackScriptFunction.cpp] 66\n");
        Assert(ScriptFunction::Is(var));
        Assert(ThreadContext::IsOnStack(var));
        return static_cast<StackScriptFunction *>(var);
    }

    ScriptFunction *
    StackScriptFunction::Box(StackScriptFunction *stackScriptFunction, void * returnAddress)
    {LOGMEIN("StackScriptFunction.cpp] 74\n");
        Assert(ThreadContext::IsOnStack(stackScriptFunction));
        Assert(stackScriptFunction->boxedScriptFunction == nullptr);

        FunctionInfo * functionInfoParent = stackScriptFunction->GetFunctionBody()->GetStackNestedFuncParentStrongRef();
        Assert(functionInfoParent != nullptr);
        FunctionBody * functionParent = functionInfoParent->GetFunctionBody();

        ScriptContext * scriptContext = stackScriptFunction->GetScriptContext();
        ScriptFunction * boxedFunction;
        BEGIN_TEMP_ALLOCATOR(tempAllocator, scriptContext, _u("BoxStackFunction"));
        {LOGMEIN("StackScriptFunction.cpp] 85\n");
            BoxState state(tempAllocator, functionParent, scriptContext, returnAddress);
            state.Box();
            boxedFunction = stackScriptFunction->boxedScriptFunction;
            Assert(boxedFunction != nullptr);
        }
        END_TEMP_ALLOCATOR(tempAllocator, scriptContext);

        return boxedFunction;
    }

    void StackScriptFunction::Box(Js::FunctionBody * parent, ScriptFunction ** functionRef)
    {LOGMEIN("StackScriptFunction.cpp] 97\n");
        ScriptContext * scriptContext = parent->GetScriptContext();
        BEGIN_TEMP_ALLOCATOR(tempAllocator, scriptContext, _u("BoxStackFunction"));
        {LOGMEIN("StackScriptFunction.cpp] 100\n");
            BoxState state(tempAllocator, parent, scriptContext);
            state.Box();

            if (functionRef != nullptr && ThreadContext::IsOnStack(*functionRef))
            {LOGMEIN("StackScriptFunction.cpp] 105\n");
                ScriptFunction * boxedScriptFunction = StackScriptFunction::FromVar(*functionRef)->boxedScriptFunction;
                if (boxedScriptFunction != nullptr)
                {LOGMEIN("StackScriptFunction.cpp] 108\n");
                    *functionRef = boxedScriptFunction;
                }
            }
        }
        END_TEMP_ALLOCATOR(tempAllocator, scriptContext);
    }

    StackScriptFunction::BoxState::BoxState(ArenaAllocator * alloc, FunctionBody * functionBody, ScriptContext * scriptContext, void * returnAddress) :
        frameToBox(alloc), functionObjectToBox(alloc), boxedValues(alloc), scriptContext(scriptContext), returnAddress(returnAddress)
    {LOGMEIN("StackScriptFunction.cpp] 118\n");
        Assert(functionBody->DoStackNestedFunc() && functionBody->GetNestedCount() != 0);
        FunctionBody * current = functionBody;
        do
        {LOGMEIN("StackScriptFunction.cpp] 122\n");
            frameToBox.Add(current);

            for (uint i = 0; i < current->GetNestedCount(); i++)
            {LOGMEIN("StackScriptFunction.cpp] 126\n");
                FunctionProxy * nested = current->GetNestedFunctionProxy(i);
                functionObjectToBox.Add(nested);
                if (nested->IsFunctionBody())
                {LOGMEIN("StackScriptFunction.cpp] 130\n");
                    nested->GetFunctionBody()->ClearStackNestedFuncParent();
                }
            }
            FunctionInfo * functionInfo = current->GetAndClearStackNestedFuncParent();
            current = functionInfo ? functionInfo->GetFunctionBody() : nullptr;
        }
        while (current && current->DoStackNestedFunc());
    }

    bool StackScriptFunction::BoxState::NeedBoxFrame(FunctionBody * functionBody)
    {LOGMEIN("StackScriptFunction.cpp] 141\n");
        return frameToBox.Contains(functionBody);
    }

    bool StackScriptFunction::BoxState::NeedBoxScriptFunction(ScriptFunction * scriptFunction)
    {LOGMEIN("StackScriptFunction.cpp] 146\n");
        return functionObjectToBox.Contains(scriptFunction->GetFunctionProxy());
    }

    void StackScriptFunction::BoxState::Box()
    {LOGMEIN("StackScriptFunction.cpp] 151\n");
        JavascriptStackWalker walker(scriptContext, true, returnAddress);
        JavascriptFunction * caller;
        bool hasInlineeToBox = false;
        while (walker.GetCaller(&caller))
        {LOGMEIN("StackScriptFunction.cpp] 156\n");
            if (!caller->IsScriptFunction())
            {LOGMEIN("StackScriptFunction.cpp] 158\n");
                continue;
            }

            ScriptFunction * callerScriptFunction = ScriptFunction::FromVar(caller);
            FunctionBody * callerFunctionBody = callerScriptFunction->GetFunctionBody();
            if (hasInlineeToBox || this->NeedBoxFrame(callerFunctionBody))
            {LOGMEIN("StackScriptFunction.cpp] 165\n");
                // Box the frame display, but don't need to box the function unless we see them
                // in the slots.

                // If the frame display has any stack nested function, then it must have given to one of the
                // stack functions.  If it doesn't appear in  on eof the stack function , the frame display
                // doesn't contain any stack function
                InterpreterStackFrame * interpreterFrame = walker.GetCurrentInterpreterFrame();

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
                if (this->NeedBoxFrame(callerFunctionBody) || (hasInlineeToBox && !walker.IsInlineFrame()))
                {LOGMEIN("StackScriptFunction.cpp] 176\n");
                    char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
                    char16 const * frameKind;
                    if (interpreterFrame)
                    {LOGMEIN("StackScriptFunction.cpp] 180\n");
                        Assert(!hasInlineeToBox);
                        frameKind = walker.IsBailedOutFromInlinee()? _u("Interpreted from Inlined Bailout (Pending)") :
                            walker.IsBailedOutFromFunction()? _u("Interpreted from Bailout") : _u("Interpreted");
                    }
                    else if (walker.IsInlineFrame())
                    {LOGMEIN("StackScriptFunction.cpp] 186\n");
                        Assert(this->NeedBoxFrame(callerFunctionBody));
                        frameKind = _u("Native Inlined (Pending)");
                    }
                    else if (this->NeedBoxFrame(callerFunctionBody))
                    {LOGMEIN("StackScriptFunction.cpp] 191\n");
                        frameKind = (hasInlineeToBox? _u("Native and Inlinee") : _u("Native"));
                    }
                    else
                    {
                        frameKind = _u("Native for Inlinee");
                    }

                    PHASE_PRINT_TESTTRACE(Js::StackFuncPhase, callerFunctionBody,
                        _u("Boxing Frame [%s]: %s %s\n"), frameKind,
                        callerFunctionBody->GetDisplayName(), callerFunctionBody->GetDebugNumberSet(debugStringBuffer));
                }
#endif

                if (interpreterFrame)
                {LOGMEIN("StackScriptFunction.cpp] 206\n");
                    Assert(!hasInlineeToBox);

                    Assert(StackScriptFunction::GetCurrentFunctionObject(interpreterFrame->GetJavascriptFunction()) == caller);

                    if (callerFunctionBody->DoStackFrameDisplay())
                    {LOGMEIN("StackScriptFunction.cpp] 212\n");
                        Js::FrameDisplay *stackFrameDisplay = interpreterFrame->GetLocalFrameDisplay();
                        // Local frame display may be null if bailout didn't restore it, which means we don't need it.
                        if (stackFrameDisplay)
                        {LOGMEIN("StackScriptFunction.cpp] 216\n");
                            Js::FrameDisplay *boxedFrameDisplay = this->BoxFrameDisplay(stackFrameDisplay);
                            interpreterFrame->SetLocalFrameDisplay(boxedFrameDisplay);
                        }
                    }
                    if (callerFunctionBody->DoStackScopeSlots())
                    {LOGMEIN("StackScriptFunction.cpp] 222\n");
                        Var* stackScopeSlots = (Var*)interpreterFrame->GetLocalClosure();
                        if (stackScopeSlots)
                        {LOGMEIN("StackScriptFunction.cpp] 225\n");
                            // Scope slot pointer may be null if bailout didn't restore it, which means we don't need it.
                            Var* boxedScopeSlots = this->BoxScopeSlots(stackScopeSlots, ScopeSlots(stackScopeSlots).GetCount());
                            interpreterFrame->SetLocalClosure((Var)boxedScopeSlots);
                        }
                    }

                    uint nestedCount = callerFunctionBody->GetNestedCount();
                    for (uint i = 0; i < nestedCount; i++)
                    {LOGMEIN("StackScriptFunction.cpp] 234\n");
                        // Box the stack function, even if they might not be "created" in the byte code yet.
                        // Some of them will not be captured in slots, so we just need to box them and record it with the
                        // stack func so that when we can just use the boxed value when we need it.
                        StackScriptFunction * stackFunction = interpreterFrame->GetStackNestedFunction(i);
                        ScriptFunction * boxedFunction = this->BoxStackFunction(stackFunction);
                        Assert(stackFunction->boxedScriptFunction == boxedFunction);
                        this->UpdateFrameDisplay(stackFunction);
                    }

                    if (walker.IsBailedOutFromInlinee())
                    {LOGMEIN("StackScriptFunction.cpp] 245\n");
                        if (!walker.IsCurrentPhysicalFrameForLoopBody())
                        {LOGMEIN("StackScriptFunction.cpp] 247\n");
                            // this is the interpret frame from bailing out of inline frame
                            // Just mark we have inlinee to box so we will walk the native frame's list when we get there.
                            hasInlineeToBox = true;
                        }
                    }
                    else if (walker.IsBailedOutFromFunction())
                    {LOGMEIN("StackScriptFunction.cpp] 254\n");
                        // The current interpret frame is from bailing out of a native frame.
                        // Walk native frame that was bailed out as well.
                        // The stack walker is pointing to the native frame already.
                        this->BoxNativeFrame(walker, callerFunctionBody);

                        // We don't need to box this frame, but we may still need to box the scope slot references
                        // within nested frame displays if the slots they refer to have been boxed.
                        if (callerFunctionBody->GetNestedCount() != 0)
                        {LOGMEIN("StackScriptFunction.cpp] 263\n");
                            this->ForEachStackNestedFunctionNative(walker, callerFunctionBody, [&](ScriptFunction *nestedFunc)
                            {
                                this->UpdateFrameDisplay(nestedFunc);
                            });
                        }
                    }
                }
                else
                {
                    if (walker.IsInlineFrame())
                    {LOGMEIN("StackScriptFunction.cpp] 274\n");
                        if (!walker.IsCurrentPhysicalFrameForLoopBody())
                        {LOGMEIN("StackScriptFunction.cpp] 276\n");
                            // We may have function that are not in slots.  So we have to walk the stack function list of the inliner
                            // to box all the needed function to catch those
                            hasInlineeToBox = true;
                        }
                    }
                    else
                    {
                        hasInlineeToBox = false;

                        if (callerFunctionBody->DoStackFrameDisplay())
                        {LOGMEIN("StackScriptFunction.cpp] 287\n");
                            Js::FrameDisplay *stackFrameDisplay =
                                this->GetFrameDisplayFromNativeFrame(walker, callerFunctionBody);
                            // Local frame display may be null if bailout didn't restore it, which means we don't need it.
                            if (stackFrameDisplay)
                            {LOGMEIN("StackScriptFunction.cpp] 292\n");
                                this->BoxFrameDisplay(stackFrameDisplay);
                            }
                        }
                        if (callerFunctionBody->DoStackScopeSlots())
                        {LOGMEIN("StackScriptFunction.cpp] 297\n");
                            Var* stackScopeSlots = this->GetScopeSlotsFromNativeFrame(walker, callerFunctionBody);
                            if (stackScopeSlots)
                            {LOGMEIN("StackScriptFunction.cpp] 300\n");
                                // Scope slot pointer may be null if bailout didn't restore it, which means we don't need it.
                                this->BoxScopeSlots(stackScopeSlots, ScopeSlots(stackScopeSlots).GetCount());
                            }
                        }

                        // walk native frame
                        this->BoxNativeFrame(walker, callerFunctionBody);

                        // We don't need to box this frame, but we may still need to box the scope slot references
                        // within nested frame displays if the slots they refer to have been boxed.
                        if (callerFunctionBody->GetNestedCount() != 0)
                        {LOGMEIN("StackScriptFunction.cpp] 312\n");
                            this->ForEachStackNestedFunctionNative(walker, callerFunctionBody, [&](ScriptFunction *nestedFunc)
                            {
                                this->UpdateFrameDisplay(nestedFunc);
                            });
                        }
                    }
                }
            }
            else if (callerFunctionBody->DoStackFrameDisplay() && !walker.IsInlineFrame())
            {LOGMEIN("StackScriptFunction.cpp] 322\n");
                // The case here is that a frame need not be boxed, but the closure environment in that frame
                // refers to an outer boxed frame.
                // Find the FD and walk it looking for a slot array that refers to a FB that must be boxed.
                // Everything from that point outward must be boxed.
                FrameDisplay *frameDisplay;
                InterpreterStackFrame *interpreterFrame = walker.GetCurrentInterpreterFrame();
                if (interpreterFrame)
                {LOGMEIN("StackScriptFunction.cpp] 330\n");
                    frameDisplay = interpreterFrame->GetLocalFrameDisplay();
                }
                else
                {
                    frameDisplay = (Js::FrameDisplay*)walker.GetCurrentArgv()[
#if _M_IX86 || _M_AMD64
                        callerFunctionBody->GetInParamsCount() == 0 ?
                        JavascriptFunctionArgIndex_StackFrameDisplayNoArg :
#endif
                        JavascriptFunctionArgIndex_StackFrameDisplay];
                }
                if (ThreadContext::IsOnStack(frameDisplay))
                {LOGMEIN("StackScriptFunction.cpp] 343\n");
                    int i;
                    for (i = 0; i < frameDisplay->GetLength(); i++)
                    {LOGMEIN("StackScriptFunction.cpp] 346\n");
                        Var *slotArray = (Var*)frameDisplay->GetItem(i);
                        ScopeSlots slots(slotArray);
                        if (slots.IsFunctionScopeSlotArray())
                        {LOGMEIN("StackScriptFunction.cpp] 350\n");
                            FunctionProxy *functionProxy = slots.GetFunctionInfo()->GetFunctionProxy();
                            if (functionProxy->IsFunctionBody() && this->NeedBoxFrame(functionProxy->GetFunctionBody()))
                            {LOGMEIN("StackScriptFunction.cpp] 353\n");
                                break;
                            }
                        }
                    }
                    for (; i < frameDisplay->GetLength(); i++)
                    {LOGMEIN("StackScriptFunction.cpp] 359\n");
                        Var *scopeSlots = (Var*)frameDisplay->GetItem(i);
                        size_t count = ScopeSlots(scopeSlots).GetCount();
                        if (count < ScopeSlots::MaxEncodedSlotCount)
                        {LOGMEIN("StackScriptFunction.cpp] 363\n");
                            Var *boxedSlots = this->BoxScopeSlots(scopeSlots, static_cast<uint>(count));
                            frameDisplay->SetItem(i, boxedSlots);
                        }
                    }
                }
            }

            ScriptFunction * boxedCaller = nullptr;
            if (this->NeedBoxScriptFunction(callerScriptFunction))
            {LOGMEIN("StackScriptFunction.cpp] 373\n");
                // TODO-STACK-NESTED-FUNC: Can't assert this yet, JIT might not do stack func allocation
                // if the function hasn't been parsed or deserialized yet.
                // Assert(ThreadContext::IsOnStack(callerScriptFunction));
                if (ThreadContext::IsOnStack(callerScriptFunction))
                {LOGMEIN("StackScriptFunction.cpp] 378\n");
                    boxedCaller = this->BoxStackFunction(StackScriptFunction::FromVar(callerScriptFunction));
                    walker.SetCurrentFunction(boxedCaller);

                    InterpreterStackFrame * interpreterFrame = walker.GetCurrentInterpreterFrame();
                    if (interpreterFrame)
                    {LOGMEIN("StackScriptFunction.cpp] 384\n");
                        interpreterFrame->SetExecutingStackFunction(boxedCaller);
                    }

                    // We don't need to box this frame, but we may still need to box the scope slot references
                    // within nested frame displays if the slots they refer to have been boxed.
                    if (callerFunctionBody->GetNestedCount() != 0)
                    {LOGMEIN("StackScriptFunction.cpp] 391\n");
                        this->ForEachStackNestedFunction(walker, callerFunctionBody, [&](ScriptFunction *nestedFunc)
                        {
                            this->UpdateFrameDisplay(nestedFunc);
                        });
                    }
                }
            }
        }

        Assert(!hasInlineeToBox);

        // We have to find one nested function
        this->Finish();
    }

    void StackScriptFunction::BoxState::UpdateFrameDisplay(ScriptFunction *nestedFunc)
    {LOGMEIN("StackScriptFunction.cpp] 408\n");
        // In some cases a function's frame display need not be boxed, but it may include outer scopes that
        // have been boxed. If that's the case, make sure that those scopes are updated.
        FrameDisplay *frameDisplay = nestedFunc->GetEnvironment();
        if (ThreadContext::IsOnStack(frameDisplay))
        {LOGMEIN("StackScriptFunction.cpp] 413\n");
            // The case here is a frame that doesn't define any captured locals, so it blindly grabs the parent
            // function's environment, which may have been boxed.
            FrameDisplay *boxedFrameDisplay;
            if (boxedValues.TryGetValue(frameDisplay, (void **)&boxedFrameDisplay))
            {LOGMEIN("StackScriptFunction.cpp] 418\n");
                nestedFunc->SetEnvironment(boxedFrameDisplay);
                return;
            }
        }

        for (uint i = 0; i < frameDisplay->GetLength(); i++)
        {LOGMEIN("StackScriptFunction.cpp] 425\n");
            Var* stackScopeSlots = (Var*)frameDisplay->GetItem(i);
            Var* boxedScopeSlots;
            if (boxedValues.TryGetValue(stackScopeSlots, (void**)&boxedScopeSlots))
            {LOGMEIN("StackScriptFunction.cpp] 429\n");
                frameDisplay->SetItem(i, boxedScopeSlots);
            }
        }
    }

    uintptr_t StackScriptFunction::BoxState::GetNativeFrameDisplayIndex(FunctionBody * functionBody)
    {LOGMEIN("StackScriptFunction.cpp] 436\n");
#if _M_IX86 || _M_AMD64
        if (functionBody->GetInParamsCount() == 0)
        {LOGMEIN("StackScriptFunction.cpp] 439\n");
            return (uintptr_t)JavascriptFunctionArgIndex_StackFrameDisplayNoArg;
        }
        else
#endif
        {
            return (uintptr_t)JavascriptFunctionArgIndex_StackFrameDisplay;
        }
    }

    uintptr_t StackScriptFunction::BoxState::GetNativeScopeSlotsIndex(FunctionBody * functionBody)
    {LOGMEIN("StackScriptFunction.cpp] 450\n");
#if _M_IX86 || _M_AMD64
        if (functionBody->GetInParamsCount() == 0)
        {LOGMEIN("StackScriptFunction.cpp] 453\n");
            return (uintptr_t)JavascriptFunctionArgIndex_StackScopeSlotsNoArg;
        }
        else
#endif
        {
            return (uintptr_t)JavascriptFunctionArgIndex_StackScopeSlots;
        }
    }

    FrameDisplay * StackScriptFunction::BoxState::GetFrameDisplayFromNativeFrame(JavascriptStackWalker const& walker, FunctionBody * callerFunctionBody)
    {LOGMEIN("StackScriptFunction.cpp] 464\n");
        uintptr_t frameDisplayIndex = GetNativeFrameDisplayIndex(callerFunctionBody);
        void **argv = walker.GetCurrentArgv();
        return (Js::FrameDisplay*)argv[frameDisplayIndex];
    }

    Var * StackScriptFunction::BoxState::GetScopeSlotsFromNativeFrame(JavascriptStackWalker const& walker, FunctionBody * callerFunctionBody)
    {LOGMEIN("StackScriptFunction.cpp] 471\n");
        uintptr_t scopeSlotsIndex = GetNativeScopeSlotsIndex(callerFunctionBody);
        void **argv = walker.GetCurrentArgv();
        return (Var*)argv[scopeSlotsIndex];
    }

    void StackScriptFunction::BoxState::SetFrameDisplayFromNativeFrame(JavascriptStackWalker const& walker, FunctionBody * callerFunctionBody, FrameDisplay * frameDisplay)
    {LOGMEIN("StackScriptFunction.cpp] 478\n");
        uintptr_t frameDisplayIndex = GetNativeFrameDisplayIndex(callerFunctionBody);
        void **argv = walker.GetCurrentArgv();
        ((FrameDisplay**)argv)[frameDisplayIndex] = frameDisplay;
    }

    void StackScriptFunction::BoxState::SetScopeSlotsFromNativeFrame(JavascriptStackWalker const& walker, FunctionBody * callerFunctionBody, Var * scopeSlots)
    {LOGMEIN("StackScriptFunction.cpp] 485\n");
        uintptr_t scopeSlotsIndex = GetNativeScopeSlotsIndex(callerFunctionBody);
        void **argv = walker.GetCurrentArgv();
        ((Var**)argv)[scopeSlotsIndex] = scopeSlots;
    }

    void StackScriptFunction::BoxState::BoxNativeFrame(JavascriptStackWalker const& walker, FunctionBody * callerFunctionBody)
    {LOGMEIN("StackScriptFunction.cpp] 492\n");
        this->ForEachStackNestedFunctionNative(walker, callerFunctionBody, [&](ScriptFunction *curr)
        {
            StackScriptFunction * func = StackScriptFunction::FromVar(curr);
            // Need to check if we need the script function as the list of script function
            // include inlinee stack function that doesn't necessary need to be boxed
            if (this->NeedBoxScriptFunction(func))
            {LOGMEIN("StackScriptFunction.cpp] 499\n");
                // Box the stack function, even if they might not be "created" in the byte code yet.
                // Some of them will not be captured in slots, so we just need to box them and record it with the
                // stack func so that when we can just use the boxed value when we need it.
                this->BoxStackFunction(func);
            }
        });

        // Write back the boxed stack closure pointers at the designated stack locations.
        Js::FrameDisplay *stackFrameDisplay = this->GetFrameDisplayFromNativeFrame(walker, callerFunctionBody);
        if (ThreadContext::IsOnStack(stackFrameDisplay))
        {LOGMEIN("StackScriptFunction.cpp] 510\n");
            Js::FrameDisplay *boxedFrameDisplay;
            if (boxedValues.TryGetValue(stackFrameDisplay, (void**)&boxedFrameDisplay))
            {LOGMEIN("StackScriptFunction.cpp] 513\n");
                this->SetFrameDisplayFromNativeFrame(walker, callerFunctionBody, boxedFrameDisplay);
                callerFunctionBody->GetScriptContext()->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_Accessor);
            }
        }

        Var              *stackScopeSlots = this->GetScopeSlotsFromNativeFrame(walker, callerFunctionBody);
        if (ThreadContext::IsOnStack(stackScopeSlots))
        {LOGMEIN("StackScriptFunction.cpp] 521\n");
            Var              *boxedScopeSlots;
            if (boxedValues.TryGetValue(stackScopeSlots, (void**)&boxedScopeSlots))
            {LOGMEIN("StackScriptFunction.cpp] 524\n");
                this->SetScopeSlotsFromNativeFrame(walker, callerFunctionBody, boxedScopeSlots);
                callerFunctionBody->GetScriptContext()->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_Accessor);
            }
        }
    }

    template<class Fn>
    void StackScriptFunction::BoxState::ForEachStackNestedFunction(
        JavascriptStackWalker const& walker,
        FunctionBody *callerFunctionBody,
        Fn fn)
    {LOGMEIN("StackScriptFunction.cpp] 536\n");
        if (!callerFunctionBody->DoStackNestedFunc())
        {LOGMEIN("StackScriptFunction.cpp] 538\n");
            return;
        }
        InterpreterStackFrame *interpreterFrame = walker.GetCurrentInterpreterFrame();
        if (interpreterFrame)
        {LOGMEIN("StackScriptFunction.cpp] 543\n");
            this->ForEachStackNestedFunctionInterpreted(interpreterFrame, callerFunctionBody, fn);
        }
        else
        {
            this->ForEachStackNestedFunctionNative(walker, callerFunctionBody, fn);
        }
    }

    template<class Fn>
    void StackScriptFunction::BoxState::ForEachStackNestedFunctionInterpreted(
        InterpreterStackFrame *interpreterFrame,
        FunctionBody *callerFunctionBody,
        Fn fn)
    {LOGMEIN("StackScriptFunction.cpp] 557\n");
        uint nestedCount = callerFunctionBody->GetNestedCount();
        for (uint i = 0; i < nestedCount; i++)
        {LOGMEIN("StackScriptFunction.cpp] 560\n");
            ScriptFunction *scriptFunction = interpreterFrame->GetStackNestedFunction(i);
            fn(scriptFunction);
        }
    }

    template<class Fn>
    void StackScriptFunction::BoxState::ForEachStackNestedFunctionNative(
        JavascriptStackWalker const& walker,
        FunctionBody *callerFunctionBody,
        Fn fn)
    {LOGMEIN("StackScriptFunction.cpp] 571\n");
        if (walker.IsInlineFrame())
        {LOGMEIN("StackScriptFunction.cpp] 573\n");
            return;
        }

        void **argv = walker.GetCurrentArgv();
        // On arm, we always have an argument slot fo frames that has stack nested func
        Js::Var curr =
#if _M_IX86 || _M_AMD64
            callerFunctionBody->GetInParamsCount() == 0?
            argv[JavascriptFunctionArgIndex_StackNestedFuncListWithNoArg]:
#endif
            argv[JavascriptFunctionArgIndex_StackNestedFuncList];

        // TODO: It is possible to have a function that is marked as doing stack function
        // and we end up not JIT'ing any of them because they are deferred or don't
        // have the default type allocated already.  We can turn this into an assert
        // when we start support JIT'ing that.

        if (curr != nullptr)
        {LOGMEIN("StackScriptFunction.cpp] 592\n");
            do
            {LOGMEIN("StackScriptFunction.cpp] 594\n");
                StackScriptFunction *func = StackScriptFunction::FromVar(curr);
                fn(func);
                curr = *(Js::Var *)(func + 1);
            }
            while (curr != nullptr);
        }
    }

    void StackScriptFunction::BoxState::Finish()
    {LOGMEIN("StackScriptFunction.cpp] 604\n");
        frameToBox.Map([](FunctionBody * body)
        {
            body->SetStackNestedFunc(false);
        });
    }

    FrameDisplay * StackScriptFunction::BoxState::BoxFrameDisplay(FrameDisplay * frameDisplay)
    {LOGMEIN("StackScriptFunction.cpp] 612\n");
        Assert(frameDisplay != nullptr);
        if (frameDisplay == &Js::NullFrameDisplay)
        {LOGMEIN("StackScriptFunction.cpp] 615\n");
            return frameDisplay;
        }

        FrameDisplay * boxedFrameDisplay;
        if (boxedValues.TryGetValue(frameDisplay, (void **)&boxedFrameDisplay))
        {LOGMEIN("StackScriptFunction.cpp] 621\n");
            return boxedFrameDisplay;
        }

        // Create new frame display when we allocate the frame display on the stack
        uint16 length = frameDisplay->GetLength();
        if (!ThreadContext::IsOnStack(frameDisplay))
        {LOGMEIN("StackScriptFunction.cpp] 628\n");
            boxedFrameDisplay = frameDisplay;
        }
        else
        {
            boxedFrameDisplay = RecyclerNewPlus(scriptContext->GetRecycler(), length * sizeof(Var), FrameDisplay, length);
        }
        boxedValues.Add(frameDisplay, boxedFrameDisplay);

        for (uint16 i = 0; i < length; i++)
        {LOGMEIN("StackScriptFunction.cpp] 638\n");
            // TODO: Once we allocate the slots on the stack, we can only look those slots
            Var * scopeSlots = (Var *)frameDisplay->GetItem(i);
            size_t scopeSlotcount = ScopeSlots(scopeSlots).GetCount(); // (size_t)scopeSlots[Js::ScopeSlots::EncodedSlotCountSlotIndex];
            // We don't do stack slots if we exceed max encoded slot count
            if (scopeSlotcount < ScopeSlots::MaxEncodedSlotCount)
            {LOGMEIN("StackScriptFunction.cpp] 644\n");
                scopeSlots = BoxScopeSlots(scopeSlots, static_cast<uint>(scopeSlotcount));
            }
            boxedFrameDisplay->SetItem(i, scopeSlots);
            frameDisplay->SetItem(i, scopeSlots);
        }
        return boxedFrameDisplay;
    }

    Var * StackScriptFunction::BoxState::BoxScopeSlots(Var * slotArray, uint count)
    {LOGMEIN("StackScriptFunction.cpp] 654\n");
        Assert(slotArray != nullptr);
        Assert(count != 0);
        Field(Var) * boxedSlotArray;
        if (boxedValues.TryGetValue(slotArray, (void **)&boxedSlotArray))
        {LOGMEIN("StackScriptFunction.cpp] 659\n");
            return (Var*)boxedSlotArray;
        }

        if (!ThreadContext::IsOnStack(slotArray))
        {LOGMEIN("StackScriptFunction.cpp] 664\n");
            boxedSlotArray = (Field(Var)*)slotArray;
        }
        else
        {
            // Create new scope slots when we allocate them on the stack
            boxedSlotArray = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), count + ScopeSlots::FirstSlotIndex);
        }
        boxedValues.Add(slotArray, boxedSlotArray);

        ScopeSlots scopeSlots(slotArray);
        ScopeSlots boxedScopeSlots((Js::Var*)boxedSlotArray);

        boxedScopeSlots.SetCount(count);
        boxedScopeSlots.SetScopeMetadata(scopeSlots.GetScopeMetadataRaw());

        // Box all the stack function in the parent's scope slot as well
        for (uint i = 0; i < count; i++)
        {LOGMEIN("StackScriptFunction.cpp] 682\n");
            Js::Var slotValue = scopeSlots.Get(i);
            if (ScriptFunction::Is(slotValue))
            {LOGMEIN("StackScriptFunction.cpp] 685\n");
                ScriptFunction * stackFunction = ScriptFunction::FromVar(slotValue);
                slotValue = BoxStackFunction(stackFunction);
            }
            boxedScopeSlots.Set(i, slotValue);
        }
        return (Var*)boxedSlotArray;
    }

    ScriptFunction * StackScriptFunction::BoxState::BoxStackFunction(ScriptFunction * scriptFunction)
    {LOGMEIN("StackScriptFunction.cpp] 695\n");
        // Box the frame display first, which may in turn box the function
        FrameDisplay * frameDisplay = scriptFunction->GetEnvironment();
        FrameDisplay * boxedFrameDisplay = BoxFrameDisplay(frameDisplay);

        if (!ThreadContext::IsOnStack(scriptFunction))
        {LOGMEIN("StackScriptFunction.cpp] 701\n");
            return scriptFunction;
        }

        StackScriptFunction * stackFunction = StackScriptFunction::FromVar(scriptFunction);
        ScriptFunction * boxedFunction = stackFunction->boxedScriptFunction;
        if (boxedFunction != nullptr)
        {LOGMEIN("StackScriptFunction.cpp] 708\n");
            return boxedFunction;
        }

        if (PHASE_TESTTRACE(Js::StackFuncPhase, stackFunction->GetFunctionProxy()))
        {LOGMEIN("StackScriptFunction.cpp] 713\n");
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

            Output::Print(_u("Boxing StackScriptFunction Object: %s (function Id: %s)"),
                stackFunction->GetFunctionProxy()->IsDeferredDeserializeFunction()?
                    _u("<DeferDeserialize>") : stackFunction->GetParseableFunctionInfo()->GetDisplayName(),
                stackFunction->GetFunctionProxy()->GetDebugNumberSet(debugStringBuffer));
            if (PHASE_VERBOSE_TESTTRACE(Js::StackFuncPhase, stackFunction->GetFunctionProxy()))
            {LOGMEIN("StackScriptFunction.cpp] 721\n");
                Output::Print(_u(" %p\n"), stackFunction);
            }
            else
            {
                Output::Print(_u("\n"));
            }
            Output::Flush();
        }

        FunctionInfo * functionInfo = stackFunction->GetFunctionInfo();
        boxedFunction = ScriptFunction::OP_NewScFunc(boxedFrameDisplay,
            reinterpret_cast<FunctionInfoPtrPtr>(&functionInfo));
        stackFunction->boxedScriptFunction = boxedFunction;
        stackFunction->SetEnvironment(boxedFrameDisplay);
        return boxedFunction;
    }

    ScriptFunction * StackScriptFunction::OP_NewStackScFunc(FrameDisplay *environment, FunctionInfoPtrPtr infoRef, ScriptFunction * stackFunction)
    {LOGMEIN("StackScriptFunction.cpp] 740\n");
        if (stackFunction)
        {LOGMEIN("StackScriptFunction.cpp] 742\n");
#if ENABLE_DEBUG_CONFIG_OPTIONS
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];
#endif

            FunctionProxy* functionProxy = (*infoRef)->GetFunctionProxy();
            AssertMsg(functionProxy != nullptr, "BYTE-CODE VERIFY: Must specify a valid function to create");
            Assert(stackFunction->GetFunctionInfo()->GetFunctionProxy() == functionProxy);
            Assert(!functionProxy->IsFunctionBody() || functionProxy->GetFunctionBody()->GetStackNestedFuncParentStrongRef() != nullptr);
            stackFunction->SetEnvironment(environment);


            PHASE_PRINT_VERBOSE_TRACE(Js::StackFuncPhase, functionProxy,
                _u("Stack alloc nested function: %s %s (address: %p)\n"),
                    functionProxy->IsFunctionBody()?
                        functionProxy->GetFunctionBody()->GetDisplayName() : _u("<deferred>"),
                        functionProxy->GetDebugNumberSet(debugStringBuffer), stackFunction);
            return stackFunction;
        }
        return ScriptFunction::OP_NewScFunc(environment, infoRef);
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType StackScriptFunction::GetSnapTag_TTD() const
    {LOGMEIN("StackScriptFunction.cpp] 766\n");
        //Make sure this isn't accidentally handled by parent class
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void StackScriptFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("StackScriptFunction.cpp] 772\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::Invalid>(objData, nullptr);
    }
#endif
 }
