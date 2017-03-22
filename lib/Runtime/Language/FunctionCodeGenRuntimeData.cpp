//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"
#include "Language/FunctionCodeGenRuntimeData.h"

namespace Js
{
    FunctionCodeGenRuntimeData::FunctionCodeGenRuntimeData(FunctionBody *const functionBody)
        : functionBody(functionBody), inlinees(nullptr), next(nullptr)
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 11\n");
    }

    FunctionBody *FunctionCodeGenRuntimeData::GetFunctionBody() const
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 15\n");
        return functionBody;
    }

    const InlineCachePointerArray<InlineCache> *FunctionCodeGenRuntimeData::ClonedInlineCaches() const
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 20\n");
        return &clonedInlineCaches;
    }

    InlineCachePointerArray<InlineCache> *FunctionCodeGenRuntimeData::ClonedInlineCaches()
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 25\n");
        return &clonedInlineCaches;
    }

    const FunctionCodeGenRuntimeData * FunctionCodeGenRuntimeData::GetForTarget(FunctionBody *targetFuncBody) const
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 30\n");
        const FunctionCodeGenRuntimeData * target = this;
        while (target && target->GetFunctionBody() != targetFuncBody)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 33\n");
            target = target->next;
        }
        // we should always find the info
        Assert(target);
        return target;
    }

    const FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::GetInlinee(const ProfileId profiledCallSiteId) const
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 42\n");
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());

        return inlinees ? inlinees[profiledCallSiteId] : nullptr;
    }

    const FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::GetInlineeForTargetInlinee(const ProfileId profiledCallSiteId, FunctionBody *inlineeFuncBody) const
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 49\n");
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());

        if (!inlinees)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 53\n");
            return nullptr;
        }
        FunctionCodeGenRuntimeData *runtimeData = inlinees[profiledCallSiteId];
        while (runtimeData && runtimeData->GetFunctionBody() != inlineeFuncBody)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 58\n");
            runtimeData = runtimeData->next;
        }
        return runtimeData;
    }

    void FunctionCodeGenRuntimeData::SetupRecursiveInlineeChain(
        Recycler *const recycler,
        const ProfileId profiledCallSiteId,
        FunctionBody *const inlinee)
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 68\n");
        Assert(recycler);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());
        Assert(inlinee == functionBody);
        if (!inlinees)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 73\n");
            inlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenRuntimeData*), functionBody->GetProfiledCallSiteCount());
        }
        inlinees[profiledCallSiteId] = this;
    }

    FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::EnsureInlinee(
        Recycler *const recycler,
        const ProfileId profiledCallSiteId,
        FunctionBody *const inlinee)
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 83\n");
        Assert(recycler);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());
        Assert(inlinee);

        if (!inlinees)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 89\n");
            inlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenRuntimeData *), functionBody->GetProfiledCallSiteCount());
        }
        const auto inlineeData = inlinees[profiledCallSiteId];

        if (!inlineeData)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 95\n");
            return inlinees[profiledCallSiteId] = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
        }

        // Find the right code gen runtime data
        FunctionCodeGenRuntimeData *next = inlineeData;
        while (next && next->functionBody != inlinee)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 102\n");
            next = next->next;
        }

        if (next)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 107\n");
            return next;
        }

        FunctionCodeGenRuntimeData *runtimeData = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
        runtimeData->next = inlineeData;
        return inlinees[profiledCallSiteId] = runtimeData;
    }

    const FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::GetLdFldInlinee(const InlineCacheIndex inlineCacheIndex) const
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 117\n");
        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());

        return ldFldInlinees ? ldFldInlinees[inlineCacheIndex] : nullptr;
    }

    const FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::GetRuntimeDataFromFunctionInfo(FunctionInfo *polyFunctionInfo) const
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 124\n");
        const FunctionCodeGenRuntimeData *next = this;
        FunctionProxy *polyFunctionProxy = polyFunctionInfo->GetFunctionProxy();
        while (next && next->functionBody != polyFunctionProxy)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 128\n");
            next = next->next;
        }
        return next;
    }

    FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::EnsureLdFldInlinee(
        Recycler *const recycler,
        const InlineCacheIndex inlineCacheIndex,
        FunctionBody *const inlinee)
    {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 138\n");
        Assert(recycler);
        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());
        Assert(inlinee);

        if (!ldFldInlinees)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 144\n");
            ldFldInlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenRuntimeData *), functionBody->GetInlineCacheCount());
        }
        const auto inlineeData = ldFldInlinees[inlineCacheIndex];
        if (!inlineeData)
        {LOGMEIN("FunctionCodeGenRuntimeData.cpp] 149\n");
            return ldFldInlinees[inlineCacheIndex] = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
        }
        return inlineeData;
    }
}
