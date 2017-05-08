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
    {TRACE_IT(48165);
    }

    uint16 FunctionCodeGenJitTimeData::GetProfiledIterations() const
    {TRACE_IT(48166);
        return profiledIterations;
    }

    FunctionInfo *FunctionCodeGenJitTimeData::GetFunctionInfo() const
    {TRACE_IT(48167);
        return this->functionInfo;
    }

    FunctionBody *FunctionCodeGenJitTimeData::GetFunctionBody() const
    {TRACE_IT(48168);
        FunctionProxy *proxy = this->functionInfo->GetFunctionProxy();
        return proxy && proxy->IsFunctionBody() ? proxy->GetFunctionBody() : nullptr;
    }

    Var FunctionCodeGenJitTimeData::GetGlobalThisObject() const
    {TRACE_IT(48169);
        return this->globalThisObject;
    }

    bool FunctionCodeGenJitTimeData::IsPolymorphicCallSite(const ProfileId profiledCallSiteId) const
    {TRACE_IT(48170);
        Assert(GetFunctionBody());
        Assert(profiledCallSiteId < GetFunctionBody()->GetProfiledCallSiteCount());

        return inlinees ? inlinees[profiledCallSiteId]->next != nullptr : false;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetInlinee(const ProfileId profiledCallSiteId) const
    {TRACE_IT(48171);
        Assert(GetFunctionBody());
        Assert(profiledCallSiteId < GetFunctionBody()->GetProfiledCallSiteCount());

        return inlinees ? inlinees[profiledCallSiteId] : nullptr;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetJitTimeDataFromFunctionInfo(FunctionInfo *polyFunctionInfo) const
    {TRACE_IT(48172);
        const FunctionCodeGenJitTimeData *next = this;
        while (next && next->functionInfo != polyFunctionInfo)
        {TRACE_IT(48173);
            next = next->next;
        }
        return next;
    }

    const FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::GetLdFldInlinee(const InlineCacheIndex inlineCacheIndex) const
    {TRACE_IT(48174);
        Assert(GetFunctionBody());
        Assert(inlineCacheIndex < GetFunctionBody()->GetInlineCacheCount());

        return ldFldInlinees ? ldFldInlinees[inlineCacheIndex] : nullptr;
    }

    FunctionCodeGenJitTimeData *FunctionCodeGenJitTimeData::AddInlinee(
        Recycler *const recycler,
        const ProfileId profiledCallSiteId,
        FunctionInfo *const inlinee,
        bool isInlined)
    {TRACE_IT(48175);
        Assert(recycler);
        const auto functionBody = GetFunctionBody();
        Assert(functionBody);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());
        Assert(inlinee);

        if (!inlinees)
        {TRACE_IT(48176);
            inlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenJitTimeData *), functionBody->GetProfiledCallSiteCount());
        }

        FunctionCodeGenJitTimeData *inlineeData = nullptr;
        if (!inlinees[profiledCallSiteId])
        {TRACE_IT(48177);
            inlineeData = RecyclerNew(recycler, FunctionCodeGenJitTimeData, inlinee, nullptr /* entryPoint */, isInlined);
            inlinees[profiledCallSiteId] = inlineeData;
            if (++inlineeCount == 0)
            {TRACE_IT(48178);
                Js::Throw::OutOfMemory();
            }
        }
        else
        {TRACE_IT(48179);
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
    {TRACE_IT(48180);
        Assert(recycler);
        const auto functionBody = GetFunctionBody();
        Assert(functionBody);
        Assert(inlineCacheIndex < GetFunctionBody()->GetInlineCacheCount());
        Assert(inlinee);

        if (!ldFldInlinees)
        {TRACE_IT(48181);
            ldFldInlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenJitTimeData*), GetFunctionBody()->GetInlineCacheCount());
        }

        const auto inlineeData = RecyclerNew(recycler, FunctionCodeGenJitTimeData, inlinee, nullptr);
        Assert(!ldFldInlinees[inlineCacheIndex]);
        ldFldInlinees[inlineCacheIndex] = inlineeData;
        if (++ldFldInlineeCount == 0)
        {TRACE_IT(48182);
            Js::Throw::OutOfMemory();
        }
        return inlineeData;
    }

    uint FunctionCodeGenJitTimeData::InlineeCount() const
    {TRACE_IT(48183);
        return inlineeCount;
    }

    uint FunctionCodeGenJitTimeData::LdFldInlineeCount() const
    {TRACE_IT(48184);
        return ldFldInlineeCount;
    }

#ifdef FIELD_ACCESS_STATS
    void FunctionCodeGenJitTimeData::EnsureInlineCacheStats(Recycler* recycler)
    {TRACE_IT(48185);
        this->inlineCacheStats = RecyclerNew(recycler, FieldAccessStats);
    }

    void FunctionCodeGenJitTimeData::AddInlineeInlineCacheStats(FunctionCodeGenJitTimeData* inlineeJitTimeData)
    {TRACE_IT(48186);
        Assert(this->inlineCacheStats != nullptr);
        Assert(inlineeJitTimeData != nullptr && inlineeJitTimeData->inlineCacheStats != nullptr);
        this->inlineCacheStats->Add(inlineeJitTimeData->inlineCacheStats);
    }
#endif
}
#endif
