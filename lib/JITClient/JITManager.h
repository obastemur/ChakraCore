//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

// We need real JITManager code when on _WIN32 or explict ENABLE_OOP_NATIVE_CODEGEN.
// Otherwise we use a dummy JITManager which disables OOP JIT to reduce code noise.

enum class RemoteCallType
{
    CodeGen,
    ThunkCreation,
    HeapQuery,
    StateUpdate
};

#if _WIN32 || ENABLE_OOP_NATIVE_CODEGEN
class JITManager
{
public:
    HRESULT ConnectRpcServer(__in HANDLE jitProcessHandle, __in_opt void* serverSecurityDescriptor, __in UUID connectionUuid);

    bool IsConnected() const;
    bool IsJITServer() const;
    void SetIsJITServer();
    bool IsOOPJITEnabled() const;
    void EnableOOPJIT();

    HANDLE GetServerHandle() const;

    HRESULT InitializeThreadContext(
        __in ThreadContextDataIDL * data,
        __out PPTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __out intptr_t *prereservedRegionAddr);

    HRESULT CleanupThreadContext(
        __inout PPTHREADCONTEXT_HANDLE threadContextInfoAddress);

    HRESULT UpdatePropertyRecordMap(
        __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __in_opt BVSparseNodeIDL * updatedPropsBVHead);

    HRESULT DecommitInterpreterBufferManager(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in boolean asmJsThunk);

    HRESULT NewInterpreterThunkBlock(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in InterpreterThunkInputIDL * thunkInput,
        __out InterpreterThunkOutputIDL * thunkOutput);

    HRESULT AddDOMFastPathHelper(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in intptr_t funcInfoAddr,
        __in int helper);

    HRESULT AddModuleRecordInfo(
            /* [in] */ PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
            /* [in] */ unsigned int moduleId,
            /* [in] */ intptr_t localExportSlotsAddr);

    HRESULT SetWellKnownHostTypeId(
        __in  PTHREADCONTEXT_HANDLE threadContextRoot,
        __in  int typeId);

    HRESULT InitializeScriptContext(
        __in ScriptContextDataIDL * data,
        __in  PTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __out PPSCRIPTCONTEXT_HANDLE scriptContextInfoAddress);

    HRESULT CleanupScriptContext(
        __inout PPSCRIPTCONTEXT_HANDLE scriptContextInfoAddress);

    HRESULT CloseScriptContext(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress);

    HRESULT FreeAllocation(
        __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __in intptr_t address);

    HRESULT SetIsPRNGSeeded(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in boolean value);

    HRESULT IsNativeAddr(
        __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __in intptr_t address,
        __out boolean * result);

    HRESULT RemoteCodeGenCall(
        __in CodeGenWorkItemIDL *workItemData,
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __out JITOutputIDL *jitData);

#if DBG
    HRESULT IsInterpreterThunkAddr(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in intptr_t address,
        __in boolean asmjsThunk,
        __out boolean * result);
#endif

    HRESULT Shutdown();


    static JITManager * GetJITManager();
    static void HandleServerCallResult(HRESULT hr, RemoteCallType callType);
private:
    JITManager();
    ~JITManager();

    HRESULT CreateBinding(
        __in HANDLE serverProcessHandle,
        __in_opt void* serverSecurityDescriptor,
        __in UUID* connectionUuid,
        __out RPC_BINDING_HANDLE* bindingHandle);

    RPC_BINDING_HANDLE m_rpcBindingHandle;
    HANDLE m_serverHandle;
    UUID m_jitConnectionId;
    bool m_oopJitEnabled;
    bool m_isJITServer;

    static JITManager s_jitManager;

};

#else  // !ENABLE_OOP_NATIVE_CODEGEN
class JITManager
{
public:
    HRESULT ConnectRpcServer(__in HANDLE jitProcessHandle, __in_opt void* serverSecurityDescriptor, __in UUID connectionUuid)
        {TRACE_IT(27329); Assert(false); return E_FAIL; }

    bool IsConnected() const {TRACE_IT(27330); return false; }
    bool IsJITServer() const {TRACE_IT(27331); return false; }
    void SetIsJITServer() {TRACE_IT(27332); Assert(false); }
    bool IsOOPJITEnabled() const {TRACE_IT(27333); return false; }
    void EnableOOPJIT() {TRACE_IT(27334); Assert(false); }

    HANDLE GetServerHandle() const
    {TRACE_IT(27335);
        Assert(false); return HANDLE();
    }

    HRESULT InitializeThreadContext(
        __in ThreadContextDataIDL * data,
        __out PPTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __out intptr_t *prereservedRegionAddr)
        {TRACE_IT(27336); Assert(false); return E_FAIL; }

    HRESULT DecommitInterpreterBufferManager(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in boolean asmJsThunk)
        {TRACE_IT(27337); Assert(false); return E_FAIL; }

    HRESULT CleanupThreadContext(
        __inout PPTHREADCONTEXT_HANDLE threadContextInfoAddress)
        {TRACE_IT(27338); Assert(false); return E_FAIL; }

    HRESULT UpdatePropertyRecordMap(
        __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __in_opt BVSparseNodeIDL * updatedPropsBVHead)
        {TRACE_IT(27339); Assert(false); return E_FAIL; }

    HRESULT AddDOMFastPathHelper(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in intptr_t funcInfoAddr,
        __in int helper)
        {TRACE_IT(27340); Assert(false); return E_FAIL; }

    HRESULT AddModuleRecordInfo(
            /* [in] */ PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
            /* [in] */ unsigned int moduleId,
            /* [in] */ intptr_t localExportSlotsAddr)
        {TRACE_IT(27341); Assert(false); return E_FAIL; }

    HRESULT SetWellKnownHostTypeId(
        __in  PTHREADCONTEXT_HANDLE threadContextRoot,
        __in  int typeId)
        {TRACE_IT(27342); Assert(false); return E_FAIL; }

    HRESULT InitializeScriptContext(
        __in ScriptContextDataIDL * data,
        __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __out PPSCRIPTCONTEXT_HANDLE scriptContextInfoAddress)
        {TRACE_IT(27343); Assert(false); return E_FAIL; }

    HRESULT CleanupScriptContext(
        __inout PPSCRIPTCONTEXT_HANDLE scriptContextInfoAddress)
        {TRACE_IT(27344); Assert(false); return E_FAIL; }

    HRESULT CloseScriptContext(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress)
        {TRACE_IT(27345); Assert(false); return E_FAIL; }

    HRESULT FreeAllocation(
        __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __in intptr_t address)
        {TRACE_IT(27346); Assert(false); return E_FAIL; }

    HRESULT SetIsPRNGSeeded(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in boolean value)
        {TRACE_IT(27347); Assert(false); return E_FAIL; }

    HRESULT IsNativeAddr(
        __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
        __in intptr_t address,
        __out boolean * result)
        {TRACE_IT(27348); Assert(false); return E_FAIL; }

    HRESULT RemoteCodeGenCall(
        __in CodeGenWorkItemIDL *workItemData,
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __out JITOutputIDL *jitData)
        {TRACE_IT(27349); Assert(false); return E_FAIL; }

#if DBG
    HRESULT IsInterpreterThunkAddr(
        __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
        __in intptr_t address,
        __in boolean asmjsThunk,
        __out boolean * result)
        {TRACE_IT(27350); Assert(false); return E_FAIL; }
#endif

    HRESULT Shutdown()
        {TRACE_IT(27351); Assert(false); return E_FAIL; }

    static JITManager * GetJITManager()
        {TRACE_IT(27352); return &s_jitManager; }
    static void HandleServerCallResult(HRESULT hr, RemoteCallType callType) {TRACE_IT(27353); Assert(UNREACHED); }

private:
    static JITManager s_jitManager;
};
#endif  // !ENABLE_OOP_NATIVE_CODEGEN
