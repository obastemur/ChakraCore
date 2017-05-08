//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#include "ChakraCore.h"
#include "JsrtThreadService.h"
#include "JsrtDebugManager.h"

class JsrtContext;

class JsrtRuntime
{
    friend class JsrtContext;

public:
    JsrtRuntime(ThreadContext * threadContext, bool useIdle, bool dispatchExceptions);
    ~JsrtRuntime();

    ThreadContext * GetThreadContext() {TRACE_IT(28491); return this->threadContext; }

    JsRuntimeHandle ToHandle() {TRACE_IT(28492); return static_cast<JsRuntimeHandle>(this); }
    static JsrtRuntime * FromHandle(JsRuntimeHandle runtimeHandle)
    {TRACE_IT(28493);
        JsrtRuntime * runtime = static_cast<JsrtRuntime *>(runtimeHandle);
        runtime->threadContext->ValidateThreadContext();
        return runtime;
    }
    static void Uninitialize();

    bool UseIdle() const {TRACE_IT(28494); return useIdle; }
    unsigned int Idle();

    bool DispatchExceptions() const {TRACE_IT(28495); return dispatchExceptions; }

    void CloseContexts();
    void SetBeforeCollectCallback(JsBeforeCollectCallback beforeCollectCallback, void * callbackContext);

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    void SetSerializeByteCodeForLibrary(bool set) {TRACE_IT(28496); serializeByteCodeForLibrary = set; }
    bool IsSerializeByteCodeForLibrary() const {TRACE_IT(28497); return serializeByteCodeForLibrary; }
#endif

    void EnsureJsrtDebugManager();
    void DeleteJsrtDebugManager();
    JsrtDebugManager * GetJsrtDebugManager();

private:
    static void __cdecl RecyclerCollectCallbackStatic(void * context, RecyclerCollectCallBackFlags flags);

private:
    ThreadContext * threadContext;
    AllocationPolicyManager* allocationPolicyManager;
    JsrtContext * contextList;
    ThreadContext::CollectCallBack * collectCallback;
    JsBeforeCollectCallback beforeCollectCallback;
    JsrtThreadService threadService;
    void * callbackContext;
    bool useIdle;
    bool dispatchExceptions;
#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
    bool serializeByteCodeForLibrary;
#endif
    JsrtDebugManager * jsrtDebugManager;
};
