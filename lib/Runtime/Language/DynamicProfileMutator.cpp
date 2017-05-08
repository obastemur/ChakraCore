//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if ENABLE_PROFILE_INFO
#ifdef DYNAMIC_PROFILE_MUTATOR
#include "DynamicProfileMutator.h"

char const * const DynamicProfileMutator::CreateMutatorProcName = STRINGIZE(CREATE_MUTATOR_PROC_NAME);

void
DynamicProfileMutator::Mutate(Js::FunctionBody * functionBody)
{TRACE_IT(47954);
    Js::ScriptContext * scriptContext = functionBody->GetScriptContext();
    DynamicProfileMutator * dynamicProfileMutator = scriptContext->GetThreadContext()->dynamicProfileMutator;
    if (dynamicProfileMutator != nullptr)
    {TRACE_IT(47955);
        if (functionBody->dynamicProfileInfo == nullptr)
        {TRACE_IT(47956);
            functionBody->dynamicProfileInfo = Js::DynamicProfileInfo::New(scriptContext->GetRecycler(), functionBody);
        }

        dynamicProfileMutator->Mutate(functionBody->dynamicProfileInfo);
        // Save the profile information, it will help in case of Crash/Failure
        Js::DynamicProfileInfo::Save(scriptContext);
    }
}

DynamicProfileMutator *
DynamicProfileMutator::GetMutator()
{TRACE_IT(47957);
    if (!Js::Configuration::Global.flags.IsEnabled(Js::DynamicProfileMutatorFlag))
    {TRACE_IT(47958);
        return nullptr;
    }

    char16 const * dllname = Js::Configuration::Global.flags.DynamicProfileMutatorDll;
    HMODULE hModule = ::LoadLibraryW(dllname);
    if (hModule == nullptr)
    {TRACE_IT(47959);
        Output::Print(_u("ERROR: Unable to load dynamic profile mutator dll %s\n"), dllname);
        Js::Throw::FatalInternalError();
    }

    CreateMutatorFunc procAddress = (CreateMutatorFunc)::GetProcAddress(hModule, CreateMutatorProcName);

    if (procAddress == nullptr)
    {TRACE_IT(47960);
        Output::Print(_u("ERROR: Unable to get function %S from dll %s\n"), CreateMutatorProcName, dllname);
        Js::Throw::FatalInternalError();
    }

    DynamicProfileMutator * mutator = procAddress();
    if (mutator == nullptr)
    {TRACE_IT(47961);
        Output::Print(_u("ERROR: Failed to create mutator from dll %s\n"), dllname);
        Js::Throw::FatalInternalError();
    }
    mutator->Initialize(Js::Configuration::Global.flags.DynamicProfileMutator);
    return mutator;
}

#endif
#endif
