//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    // Declare all the entry infos
#define BUILTIN(c, n, e, i) FunctionInfo c::EntryInfo::n(FORCE_NO_WRITE_BARRIER_TAG(c::e), (Js::FunctionInfo::Attributes)(i), JavascriptBuiltInFunction:: ## c ## _ ## n);
#define BUILTIN_TEMPLATE(c, n, e, i) template<> BUILTIN(c, n, e, i)
#include "JavascriptBuiltInFunctionList.h"
#undef BUILTIN


    FunctionInfo * const JavascriptBuiltInFunction::builtInFunctionInfo[MaxBuiltInEnum] =
    {
    #define BUILTIN(c, n, e, i) &c::EntryInfo::n,
    #include "JavascriptBuiltInFunctionList.h"
    #undef BUILTIN
    };

    FunctionInfo *
    JavascriptBuiltInFunction::GetFunctionInfo(Js::LocalFunctionId builtinId)
    {TRACE_IT(58230);
        if (IsValidId(builtinId))
        {TRACE_IT(58231);
            return builtInFunctionInfo[builtinId];
        }
        return nullptr;
    }

    bool
    JavascriptBuiltInFunction::CanChangeEntryPoint(Js::LocalFunctionId builtInId)
    {TRACE_IT(58232);
        return IsValidId(builtInId);
    }

    bool
    JavascriptBuiltInFunction::IsValidId(Js::LocalFunctionId builtInId)
    {TRACE_IT(58233);
        return builtInId < MaxBuiltInEnum;
    }
};
