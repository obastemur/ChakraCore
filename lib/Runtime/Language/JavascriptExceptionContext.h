//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js {

    class JavascriptExceptionContext
    {
    public:
        struct StackFrame
        {
        private:
            // Real script frames: functionBody, byteCodeOffset
            // Native library builtin (or potentially virtual) frames: name
            Field(FunctionBody*) functionBody;
            union
            {
                Field(uint32) byteCodeOffset;  // used for script functions        (functionBody != nullptr)
                Field(PCWSTR) name;            // used for native/virtual frames   (functionBody == nullptr)
            };
            Field(StackTraceArguments) argumentTypes;

        public:
            StackFrame() {LOGMEIN("JavascriptExceptionContext.h] 25\n");}
            StackFrame(JavascriptFunction* func, const JavascriptStackWalker& walker, bool initArgumentTypes);
            StackFrame(const StackFrame& other)
                :functionBody(other.functionBody), name(other.name), argumentTypes(other.argumentTypes)
            {LOGMEIN("JavascriptExceptionContext.h] 29\n");}
            StackFrame& operator=(const StackFrame& other)
            {LOGMEIN("JavascriptExceptionContext.h] 31\n");
                functionBody = other.functionBody;
                name = other.name;
                argumentTypes = other.argumentTypes;
                return *this;
            }

            bool IsScriptFunction() const;
            FunctionBody* GetFunctionBody() const;
            uint32 GetByteCodeOffset() const {LOGMEIN("JavascriptExceptionContext.h] 40\n"); return byteCodeOffset; }
            LPCWSTR GetFunctionName() const;
            HRESULT GetFunctionNameWithArguments(_In_ LPCWSTR *outResult) const;
        };

        typedef JsUtil::List<StackFrame> StackTrace;

    public:
        JavascriptExceptionContext() :
            m_throwingFunction(nullptr),
            m_throwingFunctionByteCodeOffset(0),
            m_stackTrace(nullptr),
            m_originalStackTrace(nullptr)
        {LOGMEIN("JavascriptExceptionContext.h] 53\n");
        }

        JavascriptFunction* ThrowingFunction() const {LOGMEIN("JavascriptExceptionContext.h] 56\n"); return m_throwingFunction; }
        uint32 ThrowingFunctionByteCodeOffset() const {LOGMEIN("JavascriptExceptionContext.h] 57\n"); return m_throwingFunctionByteCodeOffset; }
        void SetThrowingFunction(JavascriptFunction* function, uint32 byteCodeOffset, void * returnAddress);

        bool HasStackTrace() const {LOGMEIN("JavascriptExceptionContext.h] 60\n"); return m_stackTrace && m_stackTrace->Count() > 0; }
        StackTrace* GetStackTrace() const {LOGMEIN("JavascriptExceptionContext.h] 61\n"); return m_stackTrace; }
        void SetStackTrace(StackTrace *stackTrace) {LOGMEIN("JavascriptExceptionContext.h] 62\n"); m_stackTrace = stackTrace; }
        void SetOriginalStackTrace(StackTrace *stackTrace) {LOGMEIN("JavascriptExceptionContext.h] 63\n"); Assert(m_originalStackTrace == nullptr); m_originalStackTrace = stackTrace; }
        StackTrace* GetOriginalStackTrace() const {LOGMEIN("JavascriptExceptionContext.h] 64\n"); return m_originalStackTrace; }

    private:
        Field(JavascriptFunction*) m_throwingFunction;
        Field(uint32) m_throwingFunctionByteCodeOffset;
        Field(StackTrace *) m_stackTrace;
        Field(StackTrace *) m_originalStackTrace;
    };
}
