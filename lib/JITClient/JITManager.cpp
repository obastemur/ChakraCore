//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "JITClientPch.h"

_Must_inspect_result_
_Ret_maybenull_ _Post_writable_byte_size_(size)
void * __RPC_USER midl_user_allocate(
#if defined(NTBUILD) || defined(_M_ARM)
    _In_ // starting win8, _In_ is in the signature
#endif
    size_t size)
{TRACE_IT(27264);
    return (HeapAlloc(GetProcessHeap(), 0, size));
}

void __RPC_USER midl_user_free(_Pre_maybenull_ _Post_invalid_ void * ptr)
{TRACE_IT(27265);
    if (ptr != NULL)
    {TRACE_IT(27266);
        HeapFree(GetProcessHeap(), NULL, ptr);
    }
}

JITManager JITManager::s_jitManager = JITManager();

JITManager::JITManager() :
    m_rpcBindingHandle(nullptr),
    m_oopJitEnabled(false),
    m_isJITServer(false),
    m_serverHandle(nullptr),
    m_jitConnectionId()
{TRACE_IT(27267);
}

JITManager::~JITManager()
{TRACE_IT(27268);
    if (m_rpcBindingHandle)
    {TRACE_IT(27269);
        RpcBindingFree(&m_rpcBindingHandle);
    }
    if (m_serverHandle)
    {TRACE_IT(27270);
        CloseHandle(m_serverHandle);
    }
}

/* static */
JITManager * 
JITManager::GetJITManager()
{TRACE_IT(27271);
    return &s_jitManager;
}

// This routine creates a binding with the server.
HRESULT
JITManager::CreateBinding(
    __in HANDLE serverProcessHandle,
    __in_opt void * serverSecurityDescriptor,
    __in UUID * connectionUuid,
    __out RPC_BINDING_HANDLE * bindingHandle)
{TRACE_IT(27272);
    Assert(IsOOPJITEnabled());

    RPC_STATUS status;
    DWORD attemptCount = 0;
    DWORD sleepInterval = 100; // in milliseconds
    RPC_BINDING_HANDLE localBindingHandle;
    RPC_BINDING_HANDLE_TEMPLATE_V1 bindingTemplate;
    RPC_BINDING_HANDLE_SECURITY_V1_W bindingSecurity;

#ifndef NTBUILD
    RPC_SECURITY_QOS_V4 securityQOS;
    ZeroMemory(&securityQOS, sizeof(RPC_SECURITY_QOS_V4));
    securityQOS.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
    securityQOS.IdentityTracking = RPC_C_QOS_IDENTITY_DYNAMIC;
    securityQOS.ImpersonationType = RPC_C_IMP_LEVEL_IDENTIFY;
    securityQOS.Version = 4;
#else
    RPC_SECURITY_QOS_V5 securityQOS;
    ZeroMemory(&securityQOS, sizeof(RPC_SECURITY_QOS_V5));
    securityQOS.Capabilities = RPC_C_QOS_CAPABILITIES_DEFAULT;
    securityQOS.IdentityTracking = RPC_C_QOS_IDENTITY_DYNAMIC;
    securityQOS.ImpersonationType = RPC_C_IMP_LEVEL_IDENTIFY;
    securityQOS.Version = 5;
    securityQOS.ServerSecurityDescriptor = serverSecurityDescriptor;
#endif // NTBUILD

    ZeroMemory(&bindingTemplate, sizeof(bindingTemplate));
    bindingTemplate.Version = 1;
    bindingTemplate.ProtocolSequence = RPC_PROTSEQ_LRPC;
    bindingTemplate.StringEndpoint = NULL;
    memcpy_s(&bindingTemplate.ObjectUuid, sizeof(UUID), connectionUuid, sizeof(UUID));
    bindingTemplate.Flags |= RPC_BHT_OBJECT_UUID_VALID;

    ZeroMemory(&bindingSecurity, sizeof(bindingSecurity));
    bindingSecurity.Version = 1;
    bindingSecurity.AuthnLevel = RPC_C_AUTHN_LEVEL_PKT_PRIVACY;
    bindingSecurity.AuthnSvc = RPC_C_AUTHN_KERNEL;
    bindingSecurity.SecurityQos = (RPC_SECURITY_QOS*)&securityQOS;

    status = RpcBindingCreate(&bindingTemplate, &bindingSecurity, NULL, &localBindingHandle);
    if (status != RPC_S_OK)
    {TRACE_IT(27273);
        return HRESULT_FROM_WIN32(status);
    }

    // We keep attempting to connect to the server with increasing wait intervals in between.
    // This will wait close to 5 minutes before it finally gives up.
    do
    {TRACE_IT(27274);
        DWORD waitStatus;

        status = RpcBindingBind(NULL, localBindingHandle, ClientIChakraJIT_v0_0_c_ifspec);
        if (status == RPC_S_OK)
        {TRACE_IT(27275);
            break;
        }
        else if (status == EPT_S_NOT_REGISTERED)
        {TRACE_IT(27276);
            // The Server side has not finished registering the RPC Server yet.
            // We should only breakout if we have reached the max attempt count.
            if (attemptCount > 600)
            {TRACE_IT(27277);
                break;
            }
        }
        else
        {TRACE_IT(27278);
            // Some unknown error occurred. We are not going to retry for arbitrary errors.
            break;
        }

        // When we come to this point, it means the server has not finished registration yet.
        // We should wait for a while and then reattempt to bind.
        waitStatus = WaitForSingleObject(serverProcessHandle, sleepInterval);
        if (waitStatus == WAIT_OBJECT_0)
        {TRACE_IT(27279);
            DWORD exitCode = (DWORD)-1;

            // The server process died for some reason. No need to reattempt.
            // We use -1 as the exit code if GetExitCodeProcess fails.
            Assert(GetExitCodeProcess(serverProcessHandle, &exitCode));
            status = RPC_S_SERVER_UNAVAILABLE;
            break;
        }
        else if (waitStatus == WAIT_TIMEOUT)
        {TRACE_IT(27280);
            // Not an error. the server is still alive and we should reattempt.
        }
        else
        {TRACE_IT(27281);
            // wait operation failed for an unknown reason.
            Assert(false);
            status = HRESULT_FROM_WIN32(waitStatus);
            break;
        }

        attemptCount++;
        if (sleepInterval < 500)
        {TRACE_IT(27282);
            sleepInterval += 100;
        }
    } while (status != RPC_S_OK); // redundant check, but compiler would not allow true here.

    *bindingHandle = localBindingHandle;

    return HRESULT_FROM_WIN32(status);
}

bool
JITManager::IsJITServer() const
{TRACE_IT(27283);
    return m_isJITServer;
}

void
JITManager::SetIsJITServer()
{TRACE_IT(27284);
    m_isJITServer = true;
    m_oopJitEnabled = true;
}

bool
JITManager::IsConnected() const
{TRACE_IT(27285);
    Assert(IsOOPJITEnabled());
    return m_rpcBindingHandle != nullptr;
}

HANDLE
JITManager::GetServerHandle() const
{TRACE_IT(27286);
    return m_serverHandle;
}

void
JITManager::EnableOOPJIT()
{TRACE_IT(27287);
    m_oopJitEnabled = true;
}

bool
JITManager::IsOOPJITEnabled() const
{TRACE_IT(27288);
    return m_oopJitEnabled;
}

HRESULT
JITManager::ConnectRpcServer(__in HANDLE jitProcessHandle, __in_opt void* serverSecurityDescriptor, __in UUID connectionUuid)
{TRACE_IT(27289);
    Assert(IsOOPJITEnabled());
    Assert(m_rpcBindingHandle == nullptr);
    Assert(m_serverHandle == nullptr);

    HRESULT hr = E_FAIL;

    if (IsConnected())
    {TRACE_IT(27290);
        Assert(UNREACHED);
        return E_FAIL;
    }

    if (!DuplicateHandle(GetCurrentProcess(), jitProcessHandle, GetCurrentProcess(), &m_serverHandle, 0, FALSE, DUPLICATE_SAME_ACCESS))
    {TRACE_IT(27291);
        hr = HRESULT_FROM_WIN32(GetLastError());
        goto FailureCleanup;
    }

    hr = CreateBinding(jitProcessHandle, serverSecurityDescriptor, &connectionUuid, &m_rpcBindingHandle);
    if (FAILED(hr))
    {TRACE_IT(27292);
        goto FailureCleanup;
    }

    m_jitConnectionId = connectionUuid;

    return hr;

FailureCleanup:
    if (m_serverHandle)
    {TRACE_IT(27293);
        CloseHandle(m_serverHandle);
        m_serverHandle = nullptr;
    }
    if (m_rpcBindingHandle)
    {TRACE_IT(27294);
        RpcBindingFree(&m_rpcBindingHandle);
        m_rpcBindingHandle = nullptr;
    }

    return hr;
}

HRESULT
JITManager::Shutdown()
{TRACE_IT(27295);
    // this is special case of shutdown called when runtime process is a parent of the server process
    // used for console host type scenarios
    HRESULT hr = S_OK;
    Assert(IsOOPJITEnabled());
    Assert(m_rpcBindingHandle != nullptr);

    RpcTryExcept
    {
        ClientShutdown(m_rpcBindingHandle);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27296);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    m_rpcBindingHandle = nullptr;

    return hr;
}

HRESULT
JITManager::InitializeThreadContext(
    __in ThreadContextDataIDL * data,
    __out PPTHREADCONTEXT_HANDLE threadContextInfoAddress,
    __out intptr_t * prereservedRegionAddr)
{TRACE_IT(27297);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientInitializeThreadContext(m_rpcBindingHandle, data, threadContextInfoAddress, prereservedRegionAddr);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27298);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::CleanupThreadContext(
    __inout PPTHREADCONTEXT_HANDLE threadContextInfoAddress)
{TRACE_IT(27299);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientCleanupThreadContext(m_rpcBindingHandle, threadContextInfoAddress);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27300);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::AddDOMFastPathHelper(
    __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
    __in intptr_t funcInfoAddr,
    __in int helper)
{TRACE_IT(27301);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientAddDOMFastPathHelper(m_rpcBindingHandle, scriptContextInfoAddress, funcInfoAddr, helper);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27302);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::SetIsPRNGSeeded(
    __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
    __in boolean value)
{TRACE_IT(27303);
    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientSetIsPRNGSeeded(m_rpcBindingHandle, scriptContextInfoAddress, value);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27304);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;

}

HRESULT
JITManager::DecommitInterpreterBufferManager(
    __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
    __in boolean asmJsThunk)
{TRACE_IT(27305);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientDecommitInterpreterBufferManager(m_rpcBindingHandle, scriptContextInfoAddress, asmJsThunk);
    }
        RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27306);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::NewInterpreterThunkBlock(
    __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
    __in InterpreterThunkInputIDL * thunkInput,
    __out InterpreterThunkOutputIDL * thunkOutput)
{TRACE_IT(27307);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientNewInterpreterThunkBlock(m_rpcBindingHandle, scriptContextInfoAddress, thunkInput, thunkOutput);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27308);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT 
JITManager::AddModuleRecordInfo(
    /* [in] */ PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
    /* [in] */ unsigned int moduleId,
    /* [in] */ intptr_t localExportSlotsAddr)
{TRACE_IT(27309);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientAddModuleRecordInfo(m_rpcBindingHandle, scriptContextInfoAddress, moduleId, localExportSlotsAddr);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27310);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}


HRESULT
JITManager::SetWellKnownHostTypeId(
    __in  PTHREADCONTEXT_HANDLE threadContextRoot,
    __in  int typeId)
{TRACE_IT(27311);

    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientSetWellKnownHostTypeId(m_rpcBindingHandle, threadContextRoot, typeId);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27312);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;

}

HRESULT
JITManager::UpdatePropertyRecordMap(
    __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
    __in_opt BVSparseNodeIDL * updatedPropsBVHead)
{TRACE_IT(27313);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientUpdatePropertyRecordMap(m_rpcBindingHandle, threadContextInfoAddress, updatedPropsBVHead);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27314);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::InitializeScriptContext(
    __in ScriptContextDataIDL * data,
    __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
    __out PPSCRIPTCONTEXT_HANDLE scriptContextInfoAddress)
{TRACE_IT(27315);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientInitializeScriptContext(m_rpcBindingHandle, data, threadContextInfoAddress, scriptContextInfoAddress);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27316);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::CleanupScriptContext(
    __inout PPSCRIPTCONTEXT_HANDLE scriptContextInfoAddress)
{TRACE_IT(27317);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientCleanupScriptContext(m_rpcBindingHandle, scriptContextInfoAddress);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27318);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::CloseScriptContext(
    __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress)
{TRACE_IT(27319);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientCloseScriptContext(m_rpcBindingHandle, scriptContextInfoAddress);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27320);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::FreeAllocation(
    __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
    __in intptr_t address)
{TRACE_IT(27321);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientFreeAllocation(m_rpcBindingHandle, threadContextInfoAddress, address);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27322);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::IsNativeAddr(
    __in PTHREADCONTEXT_HANDLE threadContextInfoAddress,
    __in intptr_t address,
    __out boolean * result)
{TRACE_IT(27323);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientIsNativeAddr(m_rpcBindingHandle, threadContextInfoAddress, address, result);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27324);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

HRESULT
JITManager::RemoteCodeGenCall(
    __in CodeGenWorkItemIDL *workItemData,
    __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
    __out JITOutputIDL *jitData)
{TRACE_IT(27325);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientRemoteCodeGen(m_rpcBindingHandle, scriptContextInfoAddress, workItemData, jitData);
    }
    RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27326);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}

#if DBG
HRESULT
JITManager::IsInterpreterThunkAddr(
    __in PSCRIPTCONTEXT_HANDLE scriptContextInfoAddress,
    __in intptr_t address,
    __in boolean asmjsThunk,
    __out boolean * result)
{TRACE_IT(27327);
    Assert(IsOOPJITEnabled());

    HRESULT hr = E_FAIL;
    RpcTryExcept
    {
        hr = ClientIsInterpreterThunkAddr(m_rpcBindingHandle, scriptContextInfoAddress, address, asmjsThunk, result);
    }
        RpcExcept(RpcExceptionFilter(RpcExceptionCode()))
    {TRACE_IT(27328);
        hr = HRESULT_FROM_WIN32(RpcExceptionCode());
    }
    RpcEndExcept;

    return hr;
}
#endif
