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
{LOGMEIN("DynamicProfileMutator.cpp] 14\n");
    Js::ScriptContext * scriptContext = functionBody->GetScriptContext();
    DynamicProfileMutator * dynamicProfileMutator = scriptContext->GetThreadContext()->dynamicProfileMutator;
    if (dynamicProfileMutator != nullptr)
    {LOGMEIN("DynamicProfileMutator.cpp] 18\n");
        if (functionBody->dynamicProfileInfo == nullptr)
        {LOGMEIN("DynamicProfileMutator.cpp] 20\n");
            functionBody->dynamicProfileInfo = Js::DynamicProfileInfo::New(scriptContext->GetRecycler(), functionBody);
        }

        dynamicProfileMutator->Mutate(functionBody->dynamicProfileInfo);
        // Save the profile information, it will help in case of Crash/Failure
        Js::DynamicProfileInfo::Save(scriptContext);
    }
}

DynamicProfileMutator *
DynamicProfileMutator::GetMutator()
{LOGMEIN("DynamicProfileMutator.cpp] 32\n");
    if (!Js::Configuration::Global.flags.IsEnabled(Js::DynamicProfileMutatorFlag))
    {LOGMEIN("DynamicProfileMutator.cpp] 34\n");
        return nullptr;
    }

    char16 const * dllname = Js::Configuration::Global.flags.DynamicProfileMutatorDll;
    HMODULE hModule = ::LoadLibraryW(dllname);
    if (hModule == nullptr)
    {LOGMEIN("DynamicProfileMutator.cpp] 41\n");
        Output::Print(_u("ERROR: Unable to load dynamic profile mutator dll %s\n"), dllname);
        Js::Throw::FatalInternalError();
    }

    CreateMutatorFunc procAddress = (CreateMutatorFunc)::GetProcAddress(hModule, CreateMutatorProcName);

    if (procAddress == nullptr)
    {LOGMEIN("DynamicProfileMutator.cpp] 49\n");
        Output::Print(_u("ERROR: Unable to get function %S from dll %s\n"), CreateMutatorProcName, dllname);
        Js::Throw::FatalInternalError();
    }

    DynamicProfileMutator * mutator = procAddress();
    if (mutator == nullptr)
    {LOGMEIN("DynamicProfileMutator.cpp] 56\n");
        Output::Print(_u("ERROR: Failed to create mutator from dll %s\n"), dllname);
        Js::Throw::FatalInternalError();
    }
    mutator->Initialize(Js::Configuration::Global.flags.DynamicProfileMutator);
    return mutator;
}

#endif
#endif
