//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeDebugPch.h"
#include "Language/SourceDynamicProfileManager.h"

using namespace Js;

// The general idea is that nextLocalFunctionId is assigned sequentially during parse
// EnsureInitialized does one things:
//  - It ensures that the startup function bit vector on the profile manager has sufficient capacity
// The startup function bitvector might have to be resized when we call this function
void SourceContextInfo::EnsureInitialized()
{LOGMEIN("SourceContextInfo.cpp] 14\n");
#if ENABLE_PROFILE_INFO
    uint oldFunctionBodyArraySize = (this->sourceDynamicProfileManager ? this->sourceDynamicProfileManager->GetStartupFunctionsLength() : 0);
    if (oldFunctionBodyArraySize >= this->nextLocalFunctionId)
    {LOGMEIN("SourceContextInfo.cpp] 18\n");
        return;
    }

    // Match the dictionaries resize policy in calculating the amount to grow by
    uint newFunctionBodyCount = max(this->nextLocalFunctionId, UInt32Math::Add(oldFunctionBodyArraySize, oldFunctionBodyArraySize / 3));

    if(sourceDynamicProfileManager)
    {LOGMEIN("SourceContextInfo.cpp] 26\n");
        sourceDynamicProfileManager->EnsureStartupFunctions(newFunctionBodyCount);
    }
#endif
}

bool SourceContextInfo::IsSourceProfileLoaded() const
{LOGMEIN("SourceContextInfo.cpp] 33\n");
#if ENABLE_PROFILE_INFO
    return sourceDynamicProfileManager != nullptr && sourceDynamicProfileManager->IsProfileLoaded();
#else
    return false;
#endif
}

SourceContextInfo* SourceContextInfo::Clone(Js::ScriptContext* scriptContext) const
{LOGMEIN("SourceContextInfo.cpp] 42\n");
    IActiveScriptDataCache* profileCache = NULL;
    
#if ENABLE_PROFILE_INFO
    if (this->sourceDynamicProfileManager != NULL)
    {LOGMEIN("SourceContextInfo.cpp] 47\n");
        profileCache = this->sourceDynamicProfileManager->GetProfileCache();
    }
#endif

    SourceContextInfo * newSourceContextInfo = scriptContext->GetSourceContextInfo(dwHostSourceContext, profileCache);
    if (newSourceContextInfo == nullptr)
    {LOGMEIN("SourceContextInfo.cpp] 54\n");
        char16 const * oldUrl = this->url;
        char16 const * oldSourceMapUrl = this->sourceMapUrl;
        newSourceContextInfo = scriptContext->CreateSourceContextInfo(
            dwHostSourceContext,
            oldUrl,
            oldUrl? wcslen(oldUrl) : 0,
            NULL,
            oldSourceMapUrl,
            oldSourceMapUrl ? wcslen(oldSourceMapUrl) : 0);
        newSourceContextInfo->nextLocalFunctionId = this->nextLocalFunctionId;
        newSourceContextInfo->sourceContextId = this->sourceContextId;
        newSourceContextInfo->EnsureInitialized();
    }
    return newSourceContextInfo;
}
