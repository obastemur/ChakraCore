//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Library/StackScriptFunction.h"

namespace Js
{
    void JavascriptExceptionObject::FillError(Js::JavascriptExceptionContext& exceptionContext, ScriptContext *scriptContext, HostWrapperCreateFuncType hostWrapperCreateFunc)
    {TRACE_IT(49731);
        this->scriptContext  = scriptContext;
        this->exceptionContext = exceptionContext;
        this->SetHostWrapperCreateFunc(hostWrapperCreateFunc);
    }

    void JavascriptExceptionObject::ClearError()
    {TRACE_IT(49732);
        Assert(this->isPendingExceptionObject);
        memset(this, 0, sizeof(JavascriptExceptionObject));
        this->isPendingExceptionObject = true;
    }

    JavascriptExceptionObject* JavascriptExceptionObject::CloneIfStaticExceptionObject(ScriptContext* scriptContext)
    {TRACE_IT(49733);
        Assert(scriptContext);

        ThreadContext *threadContext = scriptContext->GetThreadContext();
        JavascriptExceptionObject* exceptionObject = this;

        if (this == threadContext->GetPendingOOMErrorObject())
        {TRACE_IT(49734);
            AssertMsg(this->thrownObject == NULL, "ThrownObject should be NULL since at time of OOM we will not be able to allocate the thrownObject");

            // Let's hope that unwinding has released enough pointers that the
            // recycler will find some memory to allocate the real OutOfMemory object.
            // If not, it will rethrow outOfMemory
            Var thrownObject = scriptContext->GetLibrary()->CreateOutOfMemoryError();
            exceptionObject = RecyclerNew(scriptContext->GetRecycler(),
                JavascriptExceptionObject,
                thrownObject,
                scriptContext,
                &this->exceptionContext);
            threadContext->ClearPendingOOMError();
        }

        if (this == threadContext->GetPendingSOErrorObject())
        {TRACE_IT(49735);
            Var thrownObject = NULL;

            if (this->thrownObject == NULL)
            {TRACE_IT(49736);
                AssertMsg(threadContext->IsJSRT(), "ThrownObject could be NULL for Jsrt scenarios because it is cleared in ~EnterScriptEnd. For non-jsrt cases, we should always have an allocated thrown object.");
                thrownObject = scriptContext->GetLibrary()->CreateStackOverflowError();
            }
            else
            {TRACE_IT(49737);
                thrownObject = this->GetThrownObject(scriptContext);
            }

            exceptionObject = RecyclerNew(scriptContext->GetRecycler(),
                JavascriptExceptionObject,
                thrownObject,
                scriptContext,
                &this->exceptionContext);
            threadContext->ClearPendingSOError();
        }

        return exceptionObject;
    }

    // Returns NULL if the exception object is the static out of memory object.
    Var JavascriptExceptionObject::GetThrownObject(ScriptContext * requestingScriptContext)
    {TRACE_IT(49738);
        // requestingScriptContext == this->scriptContext when we have A->(cross site thunk)B->(IDispatch)A using and nested A window return
        // exception backup. we can go back down to normal code path below.
        if (requestingScriptContext != nullptr && hostWrapperCreateFunc != nullptr && (requestingScriptContext != this->scriptContext))
        {TRACE_IT(49739);
            return hostWrapperCreateFunc(thrownObject, scriptContext, requestingScriptContext);
        }
        // We can have cross script context throw in both fastDOM and IE8 mode now.
        if (requestingScriptContext && (thrownObject != nullptr))
        {TRACE_IT(49740);
            Var rethrownObject = CrossSite::MarshalVar(requestingScriptContext, thrownObject);
            // For now, there is no known host for which we need to support cross-domain
            // scenario for JSRT. So skip the cross domain check for now.
            if (scriptContext->GetThreadContext()->IsJSRT())
            {TRACE_IT(49741);
                return rethrownObject;
            }
            if (rethrownObject)
            {TRACE_IT(49742);
                if (JavascriptError::Is(rethrownObject))
                {TRACE_IT(49743);

                    JavascriptError* jsErrorObject = JavascriptError::FromVar(rethrownObject);
                    if (jsErrorObject->GetScriptContext() != requestingScriptContext )
                    {TRACE_IT(49744);
                        Assert(requestingScriptContext->GetHostScriptContext());
                        HRESULT hr = requestingScriptContext->GetHostScriptContext()->CheckCrossDomainScriptContext(jsErrorObject->GetScriptContext());

                        if ( S_OK != hr )
                        {TRACE_IT(49745);
                            JavascriptError* jsNewErrorObject = requestingScriptContext->GetLibrary()->CreateTypeError();
                            JavascriptError::SetErrorMessage(jsNewErrorObject, VBSERR_PermissionDenied, nullptr, requestingScriptContext);
                            return jsNewErrorObject;
                        }
                    }
                }
                else
                {TRACE_IT(49746);
                    if (RecyclableObject::Is(rethrownObject))
                    {TRACE_IT(49747);
                        if (((RecyclableObject*)rethrownObject)->GetScriptContext() != requestingScriptContext)
                        {TRACE_IT(49748);
                            Assert(requestingScriptContext->GetHostScriptContext());
                            HRESULT hrSecurityCheck = requestingScriptContext->GetHostScriptContext()->CheckCrossDomainScriptContext(((RecyclableObject*)rethrownObject)->GetScriptContext());

                            if (hrSecurityCheck != S_OK)
                            {TRACE_IT(49749);
                                AssertMsg(hrSecurityCheck != E_ACCESSDENIED, "Invalid cross domain throw. HRESULT must either be S_OK or !E_ACCESSDENIED.");

                                // DOM should not throw cross domain object at all. This is defend in depth that we'll return something in requestScriptContext if they do throw
                                // something bad.
                                return requestingScriptContext->GetLibrary()->GetUndefined();
                            }
                        }
                    }

                }
            }
            return rethrownObject;
        }
        return thrownObject;
    }

    FunctionBody* JavascriptExceptionObject::GetFunctionBody() const
    {TRACE_IT(49750);
        // If it is a throwing function; it must be deserialized
        if (exceptionContext.ThrowingFunction())
        {TRACE_IT(49751);
            ParseableFunctionInfo *info = exceptionContext.ThrowingFunction()->GetParseableFunctionInfo();
            if (info->IsFunctionBody())
            {TRACE_IT(49752);
                return info->GetFunctionBody();
            }
        }
        return nullptr;
    }

    JavascriptExceptionContext::StackFrame::StackFrame(JavascriptFunction* func, const JavascriptStackWalker& walker, bool initArgumentTypes)
    {TRACE_IT(49753);
        this->functionBody = func->GetFunctionBody();

        if (this->functionBody)
        {TRACE_IT(49754);
            this->byteCodeOffset = walker.GetByteCodeOffset();
        }
        else
        {TRACE_IT(49755);
            this->name = walker.GetCurrentNativeLibraryEntryName();
        }

        if (this->functionBody && initArgumentTypes)
        {TRACE_IT(49756);
            this->argumentTypes.Init(walker);
        }
    }

    bool JavascriptExceptionContext::StackFrame::IsScriptFunction() const
    {TRACE_IT(49757);
        return functionBody != nullptr;
    }

    // Get function body -- available for script functions, null for native library builtin functions.
    FunctionBody* JavascriptExceptionContext::StackFrame::GetFunctionBody() const
    {TRACE_IT(49758);
        return functionBody;
    }

    LPCWSTR JavascriptExceptionContext::StackFrame::GetFunctionName() const
    {TRACE_IT(49759);
        return IsScriptFunction() ?
            GetFunctionBody()->GetExternalDisplayName() : PointerValue(this->name);
    }

    // Get function name with arguments info. Used by script WER.
    HRESULT JavascriptExceptionContext::StackFrame::GetFunctionNameWithArguments(_In_ LPCWSTR *outResult) const
    {TRACE_IT(49760);
        PCWSTR name = GetFunctionName();
        HRESULT hr = S_OK;
        if (IsScriptFunction())
        {TRACE_IT(49761);
            hr = argumentTypes.ToString(name, functionBody->GetScriptContext(), outResult);
        }
        else
        {TRACE_IT(49762);
            *outResult = name;
        }

        return hr;
    }

    void JavascriptExceptionContext::SetThrowingFunction(JavascriptFunction * function, uint32 byteCodeOffset, void * returnAddress)
    {TRACE_IT(49763);
        // Unfortunately, window.onerror can ask for argument.callee.caller
        // and we will return the thrown function, but the stack already unwound.
        // We will need to just box the function

        m_throwingFunction = StackScriptFunction::EnsureBoxed(BOX_PARAM(function, returnAddress, _u("throw")));
        m_throwingFunctionByteCodeOffset = byteCodeOffset;
    }

#if ENABLE_DEBUG_STACK_BACK_TRACE
    void JavascriptExceptionObject::FillStackBackTrace()
    {TRACE_IT(49764);
        // Note: this->scriptContext can be NULL when we throw Out Of Memory exception.
        if (this->stackBackTrace == NULL && this->scriptContext != NULL)
        {TRACE_IT(49765);
            Recycler* recycler = scriptContext->GetThreadContext()->GetRecycler();
            HRESULT hr = NOERROR;
            BEGIN_TRANSLATE_OOM_TO_HRESULT_NESTED
            {TRACE_IT(49766);
                this->stackBackTrace = StackBackTrace::Capture(recycler, JavascriptExceptionObject::StackToSkip, JavascriptExceptionObject::StackTraceDepth);
            }
            END_TRANSLATE_OOM_TO_HRESULT(hr)
        }
    }
#endif
}
