//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "JsrtRuntime.h"
class ChakraCoreHostScriptContext;

class JsrtContextCore sealed : public JsrtContext
{
public:
    static JsrtContextCore *New(JsrtRuntime * runtime);
    virtual void Finalize(bool isShutdown) override;
    virtual void Dispose(bool isShutdown) override;
    ChakraCoreHostScriptContext* GetHostScriptContext() const {TRACE_IT(27499); return hostContext; }

    void OnScriptLoad(Js::JavascriptFunction * scriptFunction, Js::Utf8SourceInfo* utf8SourceInfo, CompileScriptException* compileException);
private:
    DEFINE_VTABLE_CTOR(JsrtContextCore, JsrtContext);
    JsrtContextCore(JsrtRuntime * runtime);
    Js::ScriptContext* EnsureScriptContext();

    FieldNoBarrier(ChakraCoreHostScriptContext*) hostContext;
};

class ChakraCoreHostScriptContext sealed : public HostScriptContext
{
public:
    ChakraCoreHostScriptContext(Js::ScriptContext* scriptContext)
        : HostScriptContext(scriptContext),
        notifyModuleReadyCallback(nullptr),
        fetchImportedModuleCallback(nullptr)
    {TRACE_IT(27500);
    }
    ~ChakraCoreHostScriptContext()
    {TRACE_IT(27501);
    }

    virtual void Delete()
    {TRACE_IT(27502);
        HeapDelete(this);
    }

    HRESULT GetPreviousHostScriptContext(__deref_out HostScriptContext** previousScriptSite)
    {TRACE_IT(27503);
        *previousScriptSite = GetScriptContext()->GetThreadContext()->GetPreviousHostScriptContext();
        return NOERROR;
    }

    HRESULT SetCaller(IUnknown *punkNew, IUnknown **ppunkPrev)
    {TRACE_IT(27504);
        return NOERROR;
    }

    BOOL HasCaller()
    {TRACE_IT(27505);
        return FALSE;
    }

    HRESULT PushHostScriptContext()
    {TRACE_IT(27506);
        GetScriptContext()->GetThreadContext()->PushHostScriptContext(this);
        return NOERROR;
    }

    void PopHostScriptContext()
    {TRACE_IT(27507);
        GetScriptContext()->GetThreadContext()->PopHostScriptContext();
    }

    HRESULT GetDispatchExCaller(_Outptr_result_maybenull_ void** dispatchExCaller)
    {TRACE_IT(27508);
        *dispatchExCaller = nullptr;
        return NOERROR;
    }

    void ReleaseDispatchExCaller(__in void* dispatchExCaller)
    {TRACE_IT(27509);
        return;
    }

    Js::ModuleRoot * GetModuleRoot(int moduleID)
    {TRACE_IT(27510);
        Assert(false);
        return nullptr;
    }

    HRESULT CheckCrossDomainScriptContext(__in Js::ScriptContext* scriptContext) override
    {
        // no cross domain for jsrt. Return S_OK
        return S_OK;
    }

    HRESULT GetHostContextUrl(__in DWORD_PTR hostSourceContext, __out BSTR& pUrl) override
    {
        Assert(false);
        return E_NOTIMPL;
    }

    void CleanDynamicCodeCache() override
    {
        // Don't need this for jsrt core.
        return;
    }

    HRESULT VerifyDOMSecurity(Js::ScriptContext* targetContext, Js::Var obj) override
    {
        Assert(false);
        return E_NOTIMPL;
    }

#if DBG
    bool IsHostCrossSiteThunk(Js::JavascriptMethod address) override
    {
        Assert(false);
        return false;
    }
#endif

    bool SetCrossSiteForFunctionType(Js::JavascriptFunction * function) override
    {
        return false;
    }

    HRESULT CheckEvalRestriction() override
    {
        Assert(false);
        return E_NOTIMPL;
    }

    HRESULT HostExceptionFromHRESULT(HRESULT hr, Js::Var* outError) override
    {
        Assert(false);
        return E_NOTIMPL;
    }

    HRESULT GetExternalJitData(ExternalJitData id, void *data) override
    {
        Assert(false);
        return E_NOTIMPL;
    }

    HRESULT SetDispatchInvoke(Js::JavascriptMethod dispatchInvoke) override
    {
        AssertMsg(false, "no hostdispatch in jsrt");
        return E_NOTIMPL;
    }

    HRESULT ArrayBufferFromExternalObject(__in Js::RecyclableObject *obj,
        __out Js::ArrayBuffer **ppArrayBuffer) override
    {
        // there is no IBuffer in chakracore.
        *ppArrayBuffer = nullptr;
        return S_FALSE;
    }

    Js::JavascriptError* CreateWinRTError(IErrorInfo* perrinfo, Js::RestrictedErrorStrings * proerrstr) override
    {
        AssertMsg(false, "no winrt support in chakracore");
        return nullptr;
    }

    HRESULT EnqueuePromiseTask(Js::Var taskVar) override
    {
        AssertMsg(false, "jsrt should have set the promise callback");
        return E_NOTIMPL;
    }

    HRESULT FetchImportedModule(Js::ModuleRecordBase* referencingModule, LPCOLESTR specifier, Js::ModuleRecordBase** dependentModuleRecord) override;

    HRESULT NotifyHostAboutModuleReady(Js::ModuleRecordBase* referencingModule, Js::Var exceptionVar) override;

    void SetNotifyModuleReadyCallback(NotifyModuleReadyCallback notifyCallback) {TRACE_IT(27511); this->notifyModuleReadyCallback = notifyCallback; }
    NotifyModuleReadyCallback GetNotifyModuleReadyCallback() const {TRACE_IT(27512); return this->notifyModuleReadyCallback; }

    void SetFetchImportedModuleCallback(FetchImportedModuleCallBack fetchCallback) {TRACE_IT(27513); this->fetchImportedModuleCallback = fetchCallback ; }
    FetchImportedModuleCallBack GetFetchImportedModuleCallback() const {TRACE_IT(27514); return this->fetchImportedModuleCallback; }

#if DBG_DUMP || defined(PROFILE_EXEC) || defined(PROFILE_MEM)
    void EnsureParentInfo(Js::ScriptContext* scriptContext = NULL) override
    {
        // nothing to do in jsrt.
        return;
    }
#endif

private:
    FetchImportedModuleCallBack fetchImportedModuleCallback;
    NotifyModuleReadyCallback notifyModuleReadyCallback;
};
