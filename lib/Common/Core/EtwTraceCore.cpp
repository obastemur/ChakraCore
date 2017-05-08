//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonCorePch.h"
#include "Core/EtwTraceCore.h"

#ifdef ENABLE_JS_ETW
extern "C" {
    ETW_INLINE
        VOID EtwCallback(
        ULONG controlCode,
        PVOID callbackContext)
    {TRACE_IT(20028);
        EtwCallbackApi::OnSessionChange(controlCode, callbackContext);
    }
}

bool EtwTraceCore::s_registered = false;

//
// Registers the ETW provider - this is usually done on Jscript DLL load
// After registration, we will receive callbacks when ETW tracing is enabled/disabled.
//
void EtwTraceCore::Register()
{TRACE_IT(20029);
    if (!s_registered)
    {TRACE_IT(20030);
        s_registered = true;

#ifdef NTBUILD
        JS_ETW(EventRegisterMicrosoft_IE());
#endif
        JS_ETW(EventRegisterMicrosoft_JScript());
#ifdef NTBUILD
        JS_ETW(EventRegisterMicrosoft_JScript_Internal());
#endif

        // This will be used to distinguish the provider we are getting the callback for.
        PROVIDER_JSCRIPT9_Context.RegistrationHandle = Microsoft_JScriptHandle;

#ifdef NTBUILD
        BERP_IE_Context.RegistrationHandle = Microsoft_IEHandle;
#endif
    }
}

//
// Unregister to ensure we do not get callbacks.
//
void EtwTraceCore::UnRegister()
{TRACE_IT(20031);
    if (s_registered)
    {TRACE_IT(20032);
        s_registered = false;

#ifdef NTBUILD
        JS_ETW(EventUnregisterMicrosoft_IE());
#endif
        JS_ETW(EventUnregisterMicrosoft_JScript());
#ifdef NTBUILD
        JS_ETW(EventUnregisterMicrosoft_JScript_Internal());
#endif
    }
}

#endif
