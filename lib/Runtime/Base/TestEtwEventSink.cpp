//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#ifdef TEST_ETW_EVENTS
#include "Base/EtwTrace.h"

char const * const TestEtwEventSink::CreateEventSinkProcName = STRINGIZE(CREATE_EVENTSINK_PROC_NAME);
TestEtwEventSink* TestEtwEventSink::Instance = NULL;

bool TestEtwEventSink::Load()
{TRACE_IT(36902);
    char16 const * dllname = Js::Configuration::Global.flags.TestEtwDll;
    if(!dllname)
    {TRACE_IT(36903);
        return false;
    }
    HMODULE hModule = ::LoadLibraryW(dllname);
    if (hModule == nullptr)
    {TRACE_IT(36904);
        Output::Print(_u("ERROR: Unable to load ETW event sink %s\n"), dllname);
        Js::Throw::FatalInternalError();
    }

    CreateEventSink procAddress = (CreateEventSink)::GetProcAddress(hModule, CreateEventSinkProcName);

    if (procAddress == nullptr)
    {TRACE_IT(36905);
        Output::Print(_u("ERROR: Unable to get function %S from dll %s\n"), CreateEventSinkProcName, dllname);
        Js::Throw::FatalInternalError();
    }

    // CONSIDER: pass null and skip rundown testing (if a command line switch is present).
    Instance = procAddress(&EtwTrace::PerformRundown, PHASE_TRACE1(Js::EtwPhase));
    if (Instance == nullptr)
    {TRACE_IT(36906);
        Output::Print(_u("ERROR: Failed to create ETW event sink from dll %s\n"), dllname);
        Js::Throw::FatalInternalError();
    }
    return true;
}

bool TestEtwEventSink::IsLoaded()
{TRACE_IT(36907);
    return Instance != NULL;
}

void TestEtwEventSink::Unload()
{TRACE_IT(36908);
    if(Instance != NULL)
    {TRACE_IT(36909);
        Instance->UnloadInstance();
        Instance = NULL;
    }
}
#endif
