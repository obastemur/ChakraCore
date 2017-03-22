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
        {LOGMEIN("JavascriptExceptionObject.h] 21\n");
            if (exceptionContextIn)
            {LOGMEIN("JavascriptExceptionObject.h] 23\n");
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
        {LOGMEIN("JavascriptExceptionObject.h] 39\n");
            return scriptContext;
        }

        FunctionBody * GetFunctionBody() const;
        JavascriptFunction* GetFunction() const
        {LOGMEIN("JavascriptExceptionObject.h] 45\n");
            return exceptionContext.ThrowingFunction();
        }

        const JavascriptExceptionContext* GetExceptionContext() const
        {LOGMEIN("JavascriptExceptionObject.h] 50\n");
            return &exceptionContext;
        }
#if ENABLE_DEBUG_STACK_BACK_TRACE
        void FillStackBackTrace();
#endif

        void FillError(JavascriptExceptionContext& exceptionContext, ScriptContext *scriptContext, HostWrapperCreateFuncType hostWrapperCreateFunc = NULL);
        void ClearError();

        void SetDebuggerSkip(bool skip)
        {LOGMEIN("JavascriptExceptionObject.h] 61\n");
            isDebuggerSkip = skip;
        }

        bool IsDebuggerSkip()
        {LOGMEIN("JavascriptExceptionObject.h] 66\n");
            return isDebuggerSkip;
        }

        int GetByteCodeOffsetAfterDebuggerSkip()
        {LOGMEIN("JavascriptExceptionObject.h] 71\n");
            return byteCodeOffsetAfterDebuggerSkip;
        }

        void SetByteCodeOffsetAfterDebuggerSkip(int offset)
        {LOGMEIN("JavascriptExceptionObject.h] 76\n");
            byteCodeOffsetAfterDebuggerSkip = offset;
        }

        void SetDebuggerHasLogged(bool has)
        {LOGMEIN("JavascriptExceptionObject.h] 81\n");
            hasDebuggerLogged = has;
        }

        bool HasDebuggerLogged()
        {LOGMEIN("JavascriptExceptionObject.h] 86\n");
            return hasDebuggerLogged;
        }

        void SetIsFirstChance(bool is)
        {LOGMEIN("JavascriptExceptionObject.h] 91\n");
            isFirstChance = is;
        }

        bool IsFirstChanceException()
        {LOGMEIN("JavascriptExceptionObject.h] 96\n");
            return isFirstChance;
        }
        void SetIsExceptionCaughtInNonUserCode(bool is)
        {LOGMEIN("JavascriptExceptionObject.h] 100\n");
            isExceptionCaughtInNonUserCode = is;
        }

        bool IsExceptionCaughtInNonUserCode()
        {LOGMEIN("JavascriptExceptionObject.h] 105\n");
            return isExceptionCaughtInNonUserCode;
        }

        void SetHostWrapperCreateFunc(HostWrapperCreateFuncType hostWrapperCreateFunc)
        {LOGMEIN("JavascriptExceptionObject.h] 110\n");
            this->hostWrapperCreateFunc = hostWrapperCreateFunc;
        }

        uint32 GetByteCodeOffset()
        {LOGMEIN("JavascriptExceptionObject.h] 115\n");
            return exceptionContext.ThrowingFunctionByteCodeOffset();
        }

        void ReplaceThrownObject(Var object)
        {LOGMEIN("JavascriptExceptionObject.h] 120\n");
            AssertMsg(RecyclableObject::Is(object), "Why are we replacing a non recyclable thrown object?");
            AssertMsg(this->GetScriptContext() != RecyclableObject::FromVar(object)->GetScriptContext(), "If replaced thrownObject is from same context what's the need to replace?");
            this->thrownObject = object;
        }

        void SetThrownObject(Var object)
        {LOGMEIN("JavascriptExceptionObject.h] 127\n");
            // Only pending exception object and generator return exception use this API.
            Assert(this->isPendingExceptionObject || this->isGeneratorReturnException);
            this->thrownObject = object;
        }
        JavascriptExceptionObject* CloneIfStaticExceptionObject(ScriptContext* scriptContext);

        void ClearStackTrace()
        {LOGMEIN("JavascriptExceptionObject.h] 135\n");
            exceptionContext.SetStackTrace(NULL);
        }

        bool IsPendingExceptionObject() const {LOGMEIN("JavascriptExceptionObject.h] 139\n"); return isPendingExceptionObject; }

        void SetIgnoreAdvanceToNextStatement(bool is)
        {LOGMEIN("JavascriptExceptionObject.h] 142\n");
            ignoreAdvanceToNextStatement = is;
        }

        bool IsIgnoreAdvanceToNextStatement()
        {LOGMEIN("JavascriptExceptionObject.h] 147\n");
            return ignoreAdvanceToNextStatement;
        }

        void SetGeneratorReturnException(bool is)
        {LOGMEIN("JavascriptExceptionObject.h] 152\n");
            isGeneratorReturnException = is;
        }

        bool IsGeneratorReturnException()
        {LOGMEIN("JavascriptExceptionObject.h] 157\n");
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
        {LOGMEIN("JavascriptExceptionObject.h] 193\n");
            this->SetDebuggerSkip(true);
            this->SetIgnoreAdvanceToNextStatement(true);
            this->SetGeneratorReturnException(true);
        }
    };
}
