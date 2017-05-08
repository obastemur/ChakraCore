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
    {TRACE_IT(53290);
        __debugbreak();
        return nullptr;
    }

    void AsmJsJitTemplate::FreeTemplateData(void* userData)
    {TRACE_IT(53291);
        __debugbreak();
    }
}
#endif
