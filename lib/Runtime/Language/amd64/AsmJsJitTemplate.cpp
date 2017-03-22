//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#ifdef ASMJS_PLAT
namespace Js
{
#if DBG_DUMP
    FunctionBody* AsmJsJitTemplate::Globals::CurrentEncodingFunction = nullptr;
#endif

    void* AsmJsJitTemplate::InitTemplateData()
    {LOGMEIN("AsmJsJitTemplate.cpp] 14\n");
        __debugbreak();
        return nullptr;
    }

    void AsmJsJitTemplate::FreeTemplateData(void* userData)
    {LOGMEIN("AsmJsJitTemplate.cpp] 20\n");
        __debugbreak();
    }
}
#endif
