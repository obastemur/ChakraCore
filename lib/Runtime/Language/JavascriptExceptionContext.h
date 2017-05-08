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
            StackFrame() {TRACE_IT(49719);}
            StackFrame(JavascriptFunction* func, const JavascriptStackWalker& walker, bool initArgumentTypes);
            StackFrame(const StackFrame& other)
                :functionBody(other.functionBody), name(other.name), argumentTypes(other.argumentTypes)
            {TRACE_IT(49720);}
            StackFrame& operator=(const StackFrame& other)
            {TRACE_IT(49721);
                functionBody = other.functionBody;
                name = other.name;
                argumentTypes = other.argumentTypes;
                return *this;
            }

            bool IsScriptFunction() const;
            FunctionBody* GetFunctionBody() const;
            uint32 GetByteCodeOffset() const {TRACE_IT(49722); return byteCodeOffset; }
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
        {TRACE_IT(49723);
        }

        JavascriptFunction* ThrowingFunction() const {TRACE_IT(49724); return m_throwingFunction; }
        uint32 ThrowingFunctionByteCodeOffset() const {TRACE_IT(49725); return m_throwingFunctionByteCodeOffset; }
        void SetThrowingFunction(JavascriptFunction* function, uint32 byteCodeOffset, void * returnAddress);

        bool HasStackTrace() const {TRACE_IT(49726); return m_stackTrace && m_stackTrace->Count() > 0; }
        StackTrace* GetStackTrace() const {TRACE_IT(49727); return m_stackTrace; }
        void SetStackTrace(StackTrace *stackTrace) {TRACE_IT(49728); m_stackTrace = stackTrace; }
        void SetOriginalStackTrace(StackTrace *stackTrace) {TRACE_IT(49729); Assert(m_originalStackTrace == nullptr); m_originalStackTrace = stackTrace; }
        StackTrace* GetOriginalStackTrace() const {TRACE_IT(49730); return m_originalStackTrace; }

    private:
        Field(JavascriptFunction*) m_throwingFunction;
        Field(uint32) m_throwingFunctionByteCodeOffset;
        Field(StackTrace *) m_stackTrace;
        Field(StackTrace *) m_originalStackTrace;
    };
}
