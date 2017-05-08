//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {
    const DWORD  ExceptionParameters = 1;
    const int    ExceptionObjectIndex = 0;

    class JavascriptExceptionContext;

    class JavascriptExceptionObject
    {
    public:
        typedef Var (__stdcall *HostWrapperCreateFuncType)(Var var, ScriptContext * sourceScriptContext, ScriptContext * destScriptContext);

        JavascriptExceptionObject(Var object, ScriptContext * scriptContext, JavascriptExceptionContext* exceptionContextIn, bool isPendingExceptionObject = false) :
            thrownObject(object), isPendingExceptionObject(isPendingExceptionObject),
            scriptContext(scriptContext), tag(true), isDebuggerSkip(false), byteCodeOffsetAfterDebuggerSkip(Constants::InvalidByteCodeOffset), hasDebuggerLogged(false),
            isFirstChance(false), isExceptionCaughtInNonUserCode(false), ignoreAdvanceToNextStatement(false), hostWrapperCreateFunc(nullptr), isGeneratorReturnException(false)
        {TRACE_IT(49767);
            if (exceptionContextIn)
            {TRACE_IT(49768);
                exceptionContext = *exceptionContextIn;
            }
            else
            {
                memset(&exceptionContext, 0, sizeof(exceptionContext));
            }
#if ENABLE_DEBUG_STACK_BACK_TRACE
            this->stackBackTrace = nullptr;
#endif
        }

        Var GetThrownObject(ScriptContext * requestingScriptContext);

        // ScriptContext can be NULL in case of OOM exception.
        ScriptContext * GetScriptContext() const
        {TRACE_IT(49769);
            return scriptContext;
        }

        FunctionBody * GetFunctionBody() const;
        JavascriptFunction* GetFunction() const
        {TRACE_IT(49770);
            return exceptionContext.ThrowingFunction();
        }

        const JavascriptExceptionContext* GetExceptionContext() const
        {TRACE_IT(49771);
            return &exceptionContext;
        }
#if ENABLE_DEBUG_STACK_BACK_TRACE
        void FillStackBackTrace();
#endif

        void FillError(JavascriptExceptionContext& exceptionContext, ScriptContext *scriptContext, HostWrapperCreateFuncType hostWrapperCreateFunc = NULL);
        void ClearError();

        void SetDebuggerSkip(bool skip)
        {TRACE_IT(49772);
            isDebuggerSkip = skip;
        }

        bool IsDebuggerSkip()
        {TRACE_IT(49773);
            return isDebuggerSkip;
        }

        int GetByteCodeOffsetAfterDebuggerSkip()
        {TRACE_IT(49774);
            return byteCodeOffsetAfterDebuggerSkip;
        }

        void SetByteCodeOffsetAfterDebuggerSkip(int offset)
        {TRACE_IT(49775);
            byteCodeOffsetAfterDebuggerSkip = offset;
        }

        void SetDebuggerHasLogged(bool has)
        {TRACE_IT(49776);
            hasDebuggerLogged = has;
        }

        bool HasDebuggerLogged()
        {TRACE_IT(49777);
            return hasDebuggerLogged;
        }

        void SetIsFirstChance(bool is)
        {TRACE_IT(49778);
            isFirstChance = is;
        }

        bool IsFirstChanceException()
        {TRACE_IT(49779);
            return isFirstChance;
        }
        void SetIsExceptionCaughtInNonUserCode(bool is)
        {TRACE_IT(49780);
            isExceptionCaughtInNonUserCode = is;
        }

        bool IsExceptionCaughtInNonUserCode()
        {TRACE_IT(49781);
            return isExceptionCaughtInNonUserCode;
        }

        void SetHostWrapperCreateFunc(HostWrapperCreateFuncType hostWrapperCreateFunc)
        {TRACE_IT(49782);
            this->hostWrapperCreateFunc = hostWrapperCreateFunc;
        }

        uint32 GetByteCodeOffset()
        {TRACE_IT(49783);
            return exceptionContext.ThrowingFunctionByteCodeOffset();
        }

        void ReplaceThrownObject(Var object)
        {TRACE_IT(49784);
            AssertMsg(RecyclableObject::Is(object), "Why are we replacing a non recyclable thrown object?");
            AssertMsg(this->GetScriptContext() != RecyclableObject::FromVar(object)->GetScriptContext(), "If replaced thrownObject is from same context what's the need to replace?");
            this->thrownObject = object;
        }

        void SetThrownObject(Var object)
        {TRACE_IT(49785);
            // Only pending exception object and generator return exception use this API.
            Assert(this->isPendingExceptionObject || this->isGeneratorReturnException);
            this->thrownObject = object;
        }
        JavascriptExceptionObject* CloneIfStaticExceptionObject(ScriptContext* scriptContext);

        void ClearStackTrace()
        {TRACE_IT(49786);
            exceptionContext.SetStackTrace(NULL);
        }

        bool IsPendingExceptionObject() const {TRACE_IT(49787); return isPendingExceptionObject; }

        void SetIgnoreAdvanceToNextStatement(bool is)
        {TRACE_IT(49788);
            ignoreAdvanceToNextStatement = is;
        }

        bool IsIgnoreAdvanceToNextStatement()
        {TRACE_IT(49789);
            return ignoreAdvanceToNextStatement;
        }

        void SetGeneratorReturnException(bool is)
        {TRACE_IT(49790);
            isGeneratorReturnException = is;
        }

        bool IsGeneratorReturnException()
        {TRACE_IT(49791);
            // Used by the generators to throw an exception to indicate the return from generator function
            return isGeneratorReturnException;
        }

    private:
        Field(Var)      thrownObject;
        Field(ScriptContext *) scriptContext;

        Field(int)        byteCodeOffsetAfterDebuggerSkip;
        Field(const bool) tag : 1;               // Tag the low bit to prevent possible GC false references
        Field(bool)       isPendingExceptionObject : 1;
        Field(bool)       isGeneratorReturnException : 1;

        Field(bool)       isDebuggerSkip : 1;
        Field(bool)       hasDebuggerLogged : 1;
        Field(bool)       isFirstChance : 1;      // Mentions whether the current exception is a handled exception or not
        Field(bool)       isExceptionCaughtInNonUserCode : 1; // Mentions if in the caller chain the exception will be handled by the non-user code.
        Field(bool)       ignoreAdvanceToNextStatement : 1;  // This will be set when user had setnext while sitting on the exception
                                                // So the exception eating logic shouldn't try and advance to next statement again.

        FieldNoBarrier(HostWrapperCreateFuncType) hostWrapperCreateFunc;

        Field(JavascriptExceptionContext) exceptionContext;
#if ENABLE_DEBUG_STACK_BACK_TRACE
        Field(StackBackTrace*) stackBackTrace;
        static const int StackToSkip = 2;
        static const int StackTraceDepth = 30;
#endif
    };

    class GeneratorReturnExceptionObject : public JavascriptExceptionObject
    {
    public:
        GeneratorReturnExceptionObject(Var object, ScriptContext * scriptContext)
            : JavascriptExceptionObject(object, scriptContext, nullptr)
        {TRACE_IT(49792);
            this->SetDebuggerSkip(true);
            this->SetIgnoreAdvanceToNextStatement(true);
            this->SetGeneratorReturnException(true);
        }
    };
}
