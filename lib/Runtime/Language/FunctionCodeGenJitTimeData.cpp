//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "JITTimeFunctionBody.h"

#if ENABLE_NATIVE_CODEGEN
namespace Js
{
    FunctionCodeGenJitTimeData::FunctionCodeGenJitTimeData(FunctionInfo *const functionInfo, EntryPointInfo *const entryPoint, bool isInlined) :
        functionInfo(functionInfo), entryPointInfo(entryPoint), globalObjTypeSpecFldInfoCount(0), globalObjTypeSpecFldInfoArray(nullptr),
        weakFuncRef(nullptr), inlinees(nullptr), inlineeCount(0), ldFldInlineeCount(0), isInlined(isInlined), isAggressiveInliningEnabled(false),
#ifdef FIELD_ACCESS_STATS
        inlineCacheStats(nullptr),
#endif
        next(nullptr),
        ldFldInlinees(nullptr),
        globalThisObject(GetFunctionBody() && GetFunctionBody()->GetByteCode() ? GetFunctionBody()->GetScriptContext()->GetLibrary()->GetGlobalObject()->ToThis() : 0),
        profiledIterations(GetFunctionBody() && GetFunctionBody()->GetByteCode() ? GetFunctionBody()->GetProfiledIterations() : 0),
        sharedPropertyGuards(nullptr),
        sharedPropertyGuardCount(0)
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 22\n");
    }

    uint16 FunctionCodeGenJitTimeData::GetProfiledIterations() const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 26\n");
        return profiledIterations;
    }

    FunctionInfo *FunctionCodeGenJitTimeData::GetFunctionInfo() const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 31\n");
        return this->functionInfo;
    }

    FunctionBody *FunctionCodeGenJitTimeData::GetFunctionBody() const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 36\n");
        FunctionProxy *proxy = this->functionInfo->GetFunctionProxy();
        return proxy && proxy->IsFunctionBody() ? proxy->GetFunctionBody() : nullptr;
    }

    Var FunctionCodeGenJitTimeData::GetGlobalThisObject() const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 42\n");
        return this->globalThisObject;
    }

    bool FunctionCodeGenJitTimeData::IsPolymorphicCallSite(const ProfileId profiledCallSiteId) const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 47\n");
        Assert(GetFunctionBody());
        Assert(profiledCallSiteId < GetFunctionBody()->GetProfiledCallSiteCount());

        return inlinees ? inlinees[profiledCallSiteId]->next != nullptr : false;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetInlinee(const ProfileId profiledCallSiteId) const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 55\n");
        Assert(GetFunctionBody());
        Assert(profiledCallSiteId < GetFunctionBody()->GetProfiledCallSiteCount());

        return inlinees ? inlinees[profiledCallSiteId] : nullptr;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetJitTimeDataFromFunctionInfo(FunctionInfo *polyFunctionInfo) const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 63\n");
        const FunctionCodeGenJitTimeData *next = this;
        while (next && next->functionInfo != polyFunctionInfo)
        {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 66\n");
            next = next->next;
        }
        return next;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetLdFldInlinee(const InlineCacheIndex inlineCacheIndex) const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 73\n");
        Assert(GetFunctionBody());
        Assert(inlineCacheIndex < GetFunctionBody()->GetInlineCacheCount());

        return ldFldInlinees ? ldFldInlinees[inlineCacheIndex] : nullptr;
    }

    FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::AddInlinee(
        Recycler *const recycler,
        const ProfileId profiledCallSiteId,
        FunctionInfo *const inlinee,
        bool isInlined)
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 85\n");
        Assert(recycler);
        const auto functionBody = GetFunctionBody();
        Assert(functionBody);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());
        Assert(inlinee);

        if (!inlinees)
        {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 93\n");
            inlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenJitTimeData *), functionBody->GetProfiledCallSiteCount());
        }

        FunctionCodeGenJitTimeData *inlineeData = nullptr;
        if (!inlinees[profiledCallSiteId])
        {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 99\n");
            inlineeData = RecyclerNew(recycler, FunctionCodeGenJitTimeData, inlinee, nullptr /* entryPoint */, isInlined);
            inlinees[profiledCallSiteId] = inlineeData;
            if (++inlineeCount == 0)
            {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 103\n");
                Js::Throw::OutOfMemory();
            }
        }
        else
        {
            inlineeData = RecyclerNew(recycler, FunctionCodeGenJitTimeData, inlinee, nullptr /* entryPoint */, isInlined);
            // This is polymorphic, chain the data.
            inlineeData->next = inlinees[profiledCallSiteId];
            inlinees[profiledCallSiteId] = inlineeData;
        }
        return inlineeData;
    }

    FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::AddLdFldInlinee(
        Recycler *const recycler,
        const InlineCacheIndex inlineCacheIndex,
        FunctionInfo *const inlinee)
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 121\n");
        Assert(recycler);
        const auto functionBody = GetFunctionBody();
        Assert(functionBody);
        Assert(inlineCacheIndex < GetFunctionBody()->GetInlineCacheCount());
        Assert(inlinee);

        if (!ldFldInlinees)
        {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 129\n");
            ldFldInlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenJitTimeData*), GetFunctionBody()->GetInlineCacheCount());
        }

        const auto inlineeData = RecyclerNew(recycler, FunctionCodeGenJitTimeData, inlinee, nullptr);
        Assert(!ldFldInlinees[inlineCacheIndex]);
        ldFldInlinees[inlineCacheIndex] = inlineeData;
        if (++ldFldInlineeCount == 0)
        {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 137\n");
            Js::Throw::OutOfMemory();
        }
        return inlineeData;
    }

    uint FunctionCodeGenJitTimeData::InlineeCount() const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 144\n");
        return inlineeCount;
    }

    uint FunctionCodeGenJitTimeData::LdFldInlineeCount() const
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 149\n");
        return ldFldInlineeCount;
    }

#ifdef FIELD_ACCESS_STATS
    void FunctionCodeGenJitTimeData::EnsureInlineCacheStats(Recycler* recycler)
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 155\n");
        this->inlineCacheStats = RecyclerNew(recycler, FieldAccessStats);
    }

    void FunctionCodeGenJitTimeData::AddInlineeInlineCacheStats(FunctionCodeGenJitTimeData* inlineeJitTimeData)
    {LOGMEIN("FunctionCodeGenJitTimeData.cpp] 160\n");
        Assert(this->inlineCacheStats != nullptr);
        Assert(inlineeJitTimeData != nullptr && inlineeJitTimeData->inlineCacheStats != nullptr);
        this->inlineCacheStats->Add(inlineeJitTimeData->inlineCacheStats);
    }
#endif
}
#endif
