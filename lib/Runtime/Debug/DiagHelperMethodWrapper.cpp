//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language/JavascriptStackWalker.h"

namespace Js
{
    AutoRegisterIgnoreExceptionWrapper::AutoRegisterIgnoreExceptionWrapper(ThreadContext* threadContext) :
        m_threadContext(threadContext)
    {
        AssertMsg(!IsRegistered(threadContext), "BuiltInWrapper is already registered.");
        m_threadContext->GetDebugManager()->GetDebuggingFlags()->SetIsBuiltInWrapperPresent(true);
    }

    AutoRegisterIgnoreExceptionWrapper::~AutoRegisterIgnoreExceptionWrapper()
    {TRACE_IT(42271);
        m_threadContext->GetDebugManager()->GetDebuggingFlags()->SetIsBuiltInWrapperPresent(false);
    }

    // static
    bool AutoRegisterIgnoreExceptionWrapper::IsRegistered(ThreadContext* threadContext)
    {TRACE_IT(42272);
        return threadContext->GetDebugManager()->GetDebuggingFlags()->IsBuiltInWrapperPresent();
    }

    // These are wrappers for helpers that can throw non-OOM / non-SO exceptions.
    // Under debugger, if "continue after exception" is on, we catch the exception and bail out to next statement.

    // IMPORTANT note:
    // - we are taking advantage of stack alignment, that's why we can say all args have size not greater than sizeof(Var),
    //   for args that have less size, stack will be aligned, and next arg will start from alignment position,
    //   while we can take the value of current arg at current position and ignore remaining bytes used for alignment.
    // - all these wrappers expect that arguments are not float/double
    //   (double takes 8 bytes != stack alignment on x86 and ARM, double and float use different registers (VFP) rather than Var on ARM).

    typedef Var (__stdcall *OrigHelperMethod0)();
    typedef Var (__stdcall *OrigHelperMethod1)(Var arg1);
    typedef Var (__stdcall *OrigHelperMethod2)(Var arg1, Var arg2);
    typedef Var (__stdcall *OrigHelperMethod3)(Var arg1, Var arg2, Var arg3);
    typedef Var (__stdcall *OrigHelperMethod4)(Var arg1, Var arg2, Var arg3, Var arg4);
    typedef Var (__stdcall *OrigHelperMethod5)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5);
    typedef Var (__stdcall *OrigHelperMethod6)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6);
    typedef Var (__stdcall *OrigHelperMethod7)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7);
    typedef Var (__stdcall *OrigHelperMethod8)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8);
    typedef Var (__stdcall *OrigHelperMethod9)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9);
    typedef Var (__stdcall *OrigHelperMethod10)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10);
    typedef Var (__stdcall *OrigHelperMethod11)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11);
    typedef Var (__stdcall *OrigHelperMethod12)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12);
    typedef Var (__stdcall *OrigHelperMethod13)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12, Var arg13);
    typedef Var (__stdcall *OrigHelperMethod14)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12, Var arg13, Var arg14);
    typedef Var (__stdcall *OrigHelperMethod15)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12, Var arg13, Var arg14, Var arg15);
    typedef Var (__stdcall *OrigHelperMethod16)(Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12, Var arg13, Var arg14, Var arg15, Var arg16);

    template<typename Fn>
    Var HelperMethodWrapper(ScriptContext* scriptContext, Fn fn)
    {TRACE_IT(42273);
        if (AutoRegisterIgnoreExceptionWrapper::IsRegistered(scriptContext->GetThreadContext()))
        {TRACE_IT(42274);
            return fn();
        }
        else
        {TRACE_IT(42275);
            AutoRegisterIgnoreExceptionWrapper autoWrapper(scriptContext->GetThreadContext());
            return HelperOrLibraryMethodWrapper<false>(scriptContext, fn);
        }
    }

    Var HelperMethodWrapper0(ScriptContext* scriptContext, void* origHelperAddr)
    {TRACE_IT(42276);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod0)origHelperAddr)();
        });
    }

    Var HelperMethodWrapper1(ScriptContext* scriptContext, void* origHelperAddr, Var arg1)
    {TRACE_IT(42277);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod1)origHelperAddr)(arg1);
        });
    }

    Var HelperMethodWrapper2(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2)
    {TRACE_IT(42278);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod2)origHelperAddr)(arg1, arg2);
        });
    }

    Var HelperMethodWrapper3(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3)
    {TRACE_IT(42279);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod3)origHelperAddr)(arg1, arg2, arg3);
        });
    }

    Var HelperMethodWrapper4(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4)
    {TRACE_IT(42280);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod4)origHelperAddr)(arg1, arg2, arg3, arg4);
        });
    }

    Var HelperMethodWrapper5(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5)
    {TRACE_IT(42281);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod5)origHelperAddr)(arg1, arg2, arg3, arg4, arg5);
        });
    }

    Var HelperMethodWrapper6(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6)
    {TRACE_IT(42282);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod6)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6);
        });
    }

    Var HelperMethodWrapper7(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7)
    {TRACE_IT(42283);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod7)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7);
        });
    }

    Var HelperMethodWrapper8(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8)
    {TRACE_IT(42284);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod8)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8);
        });
    }

    Var HelperMethodWrapper9(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9)
    {TRACE_IT(42285);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod9)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9);
        });
    }

    Var HelperMethodWrapper10(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10)
    {TRACE_IT(42286);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod10)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10);
        });
    }

    Var HelperMethodWrapper11(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11)
    {TRACE_IT(42287);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod11)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11);
        });
    }

    Var HelperMethodWrapper12(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12)
    {TRACE_IT(42288);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod12)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12);
        });
    }

    Var HelperMethodWrapper13(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12, Var arg13)
    {TRACE_IT(42289);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod13)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13);
        });
    }

    Var HelperMethodWrapper14(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12, Var arg13, Var arg14)
    {TRACE_IT(42290);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod14)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14);
        });
    }

    Var HelperMethodWrapper15(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12, Var arg13, Var arg14, Var arg15)
    {TRACE_IT(42291);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod15)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15);
        });
    }

    Var HelperMethodWrapper16(ScriptContext* scriptContext, void* origHelperAddr, Var arg1, Var arg2, Var arg3, Var arg4, Var arg5, Var arg6, Var arg7, Var arg8, Var arg9, Var arg10, Var arg11, Var arg12, Var arg13, Var arg14, Var arg15, Var arg16)
    {TRACE_IT(42292);
        Assert(origHelperAddr);
        return HelperMethodWrapper(scriptContext, [=] {
            return ((OrigHelperMethod16)origHelperAddr)(arg1, arg2, arg3, arg4, arg5, arg6, arg7, arg8, arg9, arg10, arg11, arg12, arg13, arg14, arg15, arg16);
        });
    }

    template <bool doCheckParentInterpreterFrame>
    void HandleHelperOrLibraryMethodWrapperException(ScriptContext * scriptContext, JavascriptExceptionObject * exceptionObject)
    {TRACE_IT(42293);
        Assert(scriptContext);
        Assert(exceptionObject);

        // Note: there also could be plain OutOfMemoryException and StackOverflowException, no special handling for these.
        if (!exceptionObject->IsDebuggerSkip() ||
            exceptionObject == scriptContext->GetThreadContext()->GetPendingOOMErrorObject() ||
            exceptionObject == scriptContext->GetThreadContext()->GetPendingSOErrorObject())
        {TRACE_IT(42294);
            JavascriptExceptionOperators::DoThrowCheckClone(exceptionObject, scriptContext);
        }

        if (doCheckParentInterpreterFrame)
        {TRACE_IT(42295);
            // Note: JavascriptStackWalker is slow, but this is not hot path at all.
            // Note: we treat internal script code (such as Intl) as library code, thus
            //       ignore isLibraryCode=true callers.
            bool isTopUserFrameNative;
            bool isTopUserFrameJavaScript = Js::JavascriptStackWalker::TryIsTopJavaScriptFrameNative(
                scriptContext, &isTopUserFrameNative, /* ignoreLibraryCode = */ true);
            AssertMsg(isTopUserFrameJavaScript, "How could we get non-javascript frame on exception?");

            if (isTopUserFrameJavaScript && !isTopUserFrameNative)
            {TRACE_IT(42296);
                // If parent frame is interpreter frame, it already has try-catch around all calls,
                // so that we don't need any special handling here.
                JavascriptExceptionOperators::DoThrowCheckClone(exceptionObject, scriptContext);
            }
        }

        Assert(exceptionObject->IsDebuggerSkip());
        int nextStatementOffset;
        int offsetFromDebugger = exceptionObject->GetByteCodeOffsetAfterDebuggerSkip();
        if (offsetFromDebugger != DebuggingFlags::InvalidByteCodeOffset)
        {TRACE_IT(42297);
            // The offset is already set for us by debugger (such as by set next statement).
            nextStatementOffset = offsetFromDebugger;
        }
        else
        {TRACE_IT(42298);
            ByteCodeReader reader;
            reader.Create(exceptionObject->GetFunctionBody(), exceptionObject->GetByteCodeOffset());
            // Determine offset for next statement here.
            if (!scriptContext->GetDebugContext()->GetProbeContainer()->GetNextUserStatementOffsetForAdvance(
                exceptionObject->GetFunctionBody(), &reader, exceptionObject->GetByteCodeOffset(), &nextStatementOffset))
            {TRACE_IT(42299);
                // Can't advance.
                JavascriptExceptionOperators::DoThrowCheckClone(exceptionObject, scriptContext);
            }
        }

        // Continue after exception.
        // Note: for this scenario InterpreterStackFrame::DebugProcess resets its state,
        // looks like we don't need to that because we start with brand new interpreter frame.

        // Indicate to bailout check that we should bail out for/into debugger and set the byte code offset to one of next statement.
        scriptContext->GetThreadContext()->GetDebugManager()->GetDebuggingFlags()->SetByteCodeOffsetAndFuncAfterIgnoreException(
            nextStatementOffset, exceptionObject->GetFunctionBody()->GetFunctionNumber());
    }

    template void HandleHelperOrLibraryMethodWrapperException<true>(ScriptContext * scriptContext, JavascriptExceptionObject * exceptionObject);
    template void HandleHelperOrLibraryMethodWrapperException<false> (ScriptContext * scriptContext, JavascriptExceptionObject * exceptionObject);
} // namespace Js
