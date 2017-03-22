//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    struct RestrictedErrorStrings
    {
        BSTR restrictedErrStr;
        BSTR referenceStr;
        BSTR capabilitySid;
    };

    class JavascriptError : public DynamicObject
    {
    private:
        DEFINE_MARSHAL_OBJECT_TO_SCRIPT_CONTEXT(JavascriptError);

        Field(ErrorTypeEnum) m_errorType;

    protected:
        DEFINE_VTABLE_CTOR(JavascriptError, DynamicObject);

    public:

        JavascriptError(DynamicType* type, BOOL isExternalError = FALSE, BOOL isPrototype = FALSE) :
            DynamicObject(type), originalRuntimeErrorMessage(nullptr), isExternalError(isExternalError), isPrototype(isPrototype), isStackPropertyRedefined(false)
        {LOGMEIN("JavascriptError.h] 29\n");
            Assert(type->GetTypeId() == TypeIds_Error);
            exceptionObject = nullptr;
            m_errorType = kjstCustomError;
        }

        static bool Is(Var aValue);
        static bool IsRemoteError(Var aValue);

        ErrorTypeEnum GetErrorType() {LOGMEIN("JavascriptError.h] 38\n"); return m_errorType; }

        virtual bool HasDebugInfo();

        static JavascriptError* FromVar(Var aValue)
        {
            AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptError'");

            return static_cast<JavascriptError *>(RecyclableObject::FromVar(aValue));
        }

        void SetNotEnumerable(PropertyId propertyId);

        static Var NewInstance(RecyclableObject* function, JavascriptError* pError, CallInfo callInfo, Var newTarget, Var message);
        class EntryInfo
        {
        public:
            static FunctionInfo NewErrorInstance;
            static FunctionInfo NewEvalErrorInstance;
            static FunctionInfo NewRangeErrorInstance;
            static FunctionInfo NewReferenceErrorInstance;
            static FunctionInfo NewSyntaxErrorInstance;
            static FunctionInfo NewTypeErrorInstance;
            static FunctionInfo NewURIErrorInstance;
            static FunctionInfo NewWebAssemblyCompileErrorInstance;
            static FunctionInfo NewWebAssemblyRuntimeErrorInstance;
            static FunctionInfo NewWebAssemblyLinkErrorInstance;
#ifdef ENABLE_PROJECTION
            static FunctionInfo NewWinRTErrorInstance;
#endif
            static FunctionInfo ToString;
        };

        static Var NewErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewEvalErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewRangeErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewReferenceErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewSyntaxErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewTypeErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewURIErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewWebAssemblyCompileErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewWebAssemblyRuntimeErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
        static Var NewWebAssemblyLinkErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
#ifdef ENABLE_PROJECTION
        static Var NewWinRTErrorInstance(RecyclableObject* function, CallInfo callInfo, ...);
#endif

        static Var EntryToString(RecyclableObject* function, CallInfo callInfo, ...);

        static void __declspec(noreturn) MapAndThrowError(ScriptContext* scriptContext, HRESULT hr);
        static void __declspec(noreturn) MapAndThrowError(ScriptContext* scriptContext, HRESULT hr, ErrorTypeEnum errorType, EXCEPINFO *ei);
        static void __declspec(noreturn) SetMessageAndThrowError(ScriptContext* scriptContext, JavascriptError *pError, int32 hCode, EXCEPINFO* pei);
        static JavascriptError* MapError(ScriptContext* scriptContext, ErrorTypeEnum errorType);

        //HELPERCALL needs a non-overloaded function pointer
        static void __declspec(noreturn) ThrowUnreachable(ScriptContext* scriptContext);

#define THROW_ERROR_DECL(err_method) \
        static void __declspec(noreturn) err_method(ScriptContext* scriptContext, int32 hCode, EXCEPINFO* ei); \
        static void __declspec(noreturn) err_method(ScriptContext* scriptContext, int32 hCode, PCWSTR varName = nullptr); \
        static void __declspec(noreturn) err_method(ScriptContext* scriptContext, int32 hCode, JavascriptString* varName); \
        static void __declspec(noreturn) err_method##Var(ScriptContext* scriptContext, int32 hCode, ...);

        THROW_ERROR_DECL(ThrowError)
        THROW_ERROR_DECL(ThrowRangeError)
        THROW_ERROR_DECL(ThrowReferenceError)
        THROW_ERROR_DECL(ThrowSyntaxError)
        THROW_ERROR_DECL(ThrowTypeError)
        THROW_ERROR_DECL(ThrowURIError)
        THROW_ERROR_DECL(ThrowWebAssemblyCompileError)
        THROW_ERROR_DECL(ThrowWebAssemblyRuntimeError)
        THROW_ERROR_DECL(ThrowWebAssemblyLinkError)

#undef THROW_ERROR_DECL
        static void __declspec(noreturn) ThrowDispatchError(ScriptContext* scriptContext, HRESULT hCode, PCWSTR message);
        static void __declspec(noreturn) ThrowOutOfMemoryError(ScriptContext *scriptContext);
        static void __declspec(noreturn) ThrowParserError(ScriptContext* scriptContext, HRESULT hrParser, CompileScriptException* se);
        static ErrorTypeEnum MapParseError(int32 hCode);
        static JavascriptError* MapParseError(ScriptContext* scriptContext, int32 hCode);
        static HRESULT GetRuntimeError(RecyclableObject* errorObject, __out_opt LPCWSTR * pMessage);
        static HRESULT GetRuntimeErrorWithScriptEnter(RecyclableObject* errorObject, __out_opt LPCWSTR * pMessage);
        static void __declspec(noreturn) ThrowStackOverflowError(ScriptContext *scriptContext, PVOID returnAddress = nullptr);
        static void SetErrorMessageProperties(JavascriptError *pError, HRESULT errCode, PCWSTR message, ScriptContext* scriptContext);
        static void SetErrorMessage(JavascriptError *pError, HRESULT errCode, PCWSTR varName, ScriptContext* scriptContext);
        static void SetErrorMessage(JavascriptError *pError, HRESULT hr, ScriptContext* scriptContext, va_list argList);
        static void SetErrorType(JavascriptError *pError, ErrorTypeEnum errorType);

        static bool ThrowCantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext, PropertyId propertyId);
        static bool ThrowCantAssign(PropertyOperationFlags flags, ScriptContext* scriptContext, uint32 index);
        static bool ThrowCantAssignIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext);
        static bool ThrowCantExtendIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext);
        static bool ThrowCantDeleteIfStrictMode(PropertyOperationFlags flags, ScriptContext* scriptContext, PCWSTR varName);
        static bool ThrowCantDelete(PropertyOperationFlags flags, ScriptContext* scriptContext, PCWSTR varName);
        static bool ThrowIfStrictModeUndefinedSetter(PropertyOperationFlags flags, Var setterValue, ScriptContext* scriptContext);
        static bool ThrowIfNotExtensibleUndefinedSetter(PropertyOperationFlags flags, Var setterValue, ScriptContext* scriptContext);

        BOOL IsExternalError() const {LOGMEIN("JavascriptError.h] 134\n"); return isExternalError; }
        BOOL IsPrototype() const {LOGMEIN("JavascriptError.h] 135\n"); return isPrototype; }
        bool IsStackPropertyRedefined() const {LOGMEIN("JavascriptError.h] 136\n"); return isStackPropertyRedefined; }
        void SetStackPropertyRedefined(const bool value) {LOGMEIN("JavascriptError.h] 137\n"); isStackPropertyRedefined = value; }
        virtual BOOL GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;
        virtual BOOL GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext) override;

        void SetJavascriptExceptionObject(JavascriptExceptionObject *exceptionObject)
        {LOGMEIN("JavascriptError.h] 142\n");
            Assert(exceptionObject);
            this->exceptionObject = exceptionObject;
        }

        JavascriptExceptionObject *GetJavascriptExceptionObject() {LOGMEIN("JavascriptError.h] 147\n"); return exceptionObject; }

        static DWORD GetAdjustedResourceStringHr(DWORD hr, bool isFormatString);

        static int32 GetErrorNumberFromResourceID(int32 resourceId);

        virtual JavascriptError* CreateNewErrorOfSameType(JavascriptLibrary* targetJavascriptLibrary);
        JavascriptError* CloneErrorMsgAndNumber(JavascriptLibrary* targetJavascriptLibrary);
        static void TryThrowTypeError(ScriptContext * checkScriptContext, ScriptContext * scriptContext, int32 hCode, PCWSTR varName = nullptr);
        static JavascriptError* CreateFromCompileScriptException(ScriptContext* scriptContext, CompileScriptException* cse);

    private:

        Field(BOOL) isExternalError;
        Field(BOOL) isPrototype;
        Field(bool) isStackPropertyRedefined;
        Field(char16 const *) originalRuntimeErrorMessage;
        Field(JavascriptExceptionObject *) exceptionObject;

#ifdef ERROR_TRACE
        static void Trace(const char16 *form, ...) // const
        {
            if (Js::Configuration::Global.flags.Trace.IsEnabled(Js::ErrorPhase))
            {LOGMEIN("JavascriptError.h] 170\n");
                va_list argptr;
                va_start(argptr, form);
                Output::Print(_u("Error: "));
                Output::VPrint(form, argptr);
                Output::Flush();
            }
        }
#endif

#if ENABLE_TTD
    public:
        virtual TTD::NSSnapObjects::SnapObjectType GetSnapTag_TTD() const override;
        virtual void ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc) override;
#endif
    };
}
