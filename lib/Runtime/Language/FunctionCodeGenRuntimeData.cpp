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
    {TRACE_IT(48205);
    }

    FunctionBody *FunctionCodeGenRuntimeData::GetFunctionBody() const
    {TRACE_IT(48206);
        return functionBody;
    }

    const InlineCachePointerArray<InlineCache> *FunctionCodeGenRuntimeData::ClonedInlineCaches() const
    {TRACE_IT(48207);
        return &clonedInlineCaches;
    }

    InlineCachePointerArray<InlineCache> *FunctionCodeGenRuntimeData::ClonedInlineCaches()
    {TRACE_IT(48208);
        return &clonedInlineCaches;
    }

    const FunctionCodeGenRuntimeData * FunctionCodeGenRuntimeData::GetForTarget(FunctionBody *targetFuncBody) const
    {TRACE_IT(48209);
        const FunctionCodeGenRuntimeData * target = this;
        while (target && target->GetFunctionBody() != targetFuncBody)
        {TRACE_IT(48210);
            target = target->next;
        }
        // we should always find the info
        Assert(target);
        return target;
    }

    const FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::GetInlinee(const ProfileId profiledCallSiteId) const
    {TRACE_IT(48211);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());

        return inlinees ? inlinees[profiledCallSiteId] : nullptr;
    }

    const FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::GetInlineeForTargetInlinee(const ProfileId profiledCallSiteId, FunctionBody *inlineeFuncBody) const
    {TRACE_IT(48212);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());

        if (!inlinees)
        {TRACE_IT(48213);
            return nullptr;
        }
        FunctionCodeGenRuntimeData *runtimeData = inlinees[profiledCallSiteId];
        while (runtimeData && runtimeData->GetFunctionBody() != inlineeFuncBody)
        {TRACE_IT(48214);
            runtimeData = runtimeData->next;
        }
        return runtimeData;
    }

    void FunctionCodeGenRuntimeData::SetupRecursiveInlineeChain(
        Recycler *const recycler,
        const ProfileId profiledCallSiteId,
        FunctionBody *const inlinee)
    {TRACE_IT(48215);
        Assert(recycler);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());
        Assert(inlinee == functionBody);
        if (!inlinees)
        {TRACE_IT(48216);
            inlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenRuntimeData*), functionBody->GetProfiledCallSiteCount());
        }
        inlinees[profiledCallSiteId] = this;
    }

    FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::EnsureInlinee(
        Recycler *const recycler,
        const ProfileId profiledCallSiteId,
        FunctionBody *const inlinee)
    {TRACE_IT(48217);
        Assert(recycler);
        Assert(profiledCallSiteId < functionBody->GetProfiledCallSiteCount());
        Assert(inlinee);

        if (!inlinees)
        {TRACE_IT(48218);
            inlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenRuntimeData *), functionBody->GetProfiledCallSiteCount());
        }
        const auto inlineeData = inlinees[profiledCallSiteId];

        if (!inlineeData)
        {TRACE_IT(48219);
            return inlinees[profiledCallSiteId] = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
        }

        // Find the right code gen runtime data
        FunctionCodeGenRuntimeData *next = inlineeData;
        while (next && next->functionBody != inlinee)
        {TRACE_IT(48220);
            next = next->next;
        }

        if (next)
        {TRACE_IT(48221);
            return next;
        }

        FunctionCodeGenRuntimeData *runtimeData = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
        runtimeData->next = inlineeData;
        return inlinees[profiledCallSiteId] = runtimeData;
    }

    const FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::GetLdFldInlinee(const InlineCacheIndex inlineCacheIndex) const
    {TRACE_IT(48222);
        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());

        return ldFldInlinees ? ldFldInlinees[inlineCacheIndex] : nullptr;
    }

    const FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::GetRuntimeDataFromFunctionInfo(FunctionInfo *polyFunctionInfo) const
    {TRACE_IT(48223);
        const FunctionCodeGenRuntimeData *next = this;
        FunctionProxy *polyFunctionProxy = polyFunctionInfo->GetFunctionProxy();
        while (next && next->functionBody != polyFunctionProxy)
        {TRACE_IT(48224);
            next = next->next;
        }
        return next;
    }

    FunctionCodeGenRuntimeData *FunctionCodeGenRuntimeData::EnsureLdFldInlinee(
        Recycler *const recycler,
        const InlineCacheIndex inlineCacheIndex,
        FunctionBody *const inlinee)
    {TRACE_IT(48225);
        Assert(recycler);
        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());
        Assert(inlinee);

        if (!ldFldInlinees)
        {TRACE_IT(48226);
            ldFldInlinees = RecyclerNewArrayZ(recycler, Field(FunctionCodeGenRuntimeData *), functionBody->GetInlineCacheCount());
        }
        const auto inlineeData = ldFldInlinees[inlineCacheIndex];
        if (!inlineeData)
        {TRACE_IT(48227);
            return ldFldInlinees[inlineCacheIndex] = RecyclerNew(recycler, FunctionCodeGenRuntimeData, inlinee);
        }
        return inlineeData;
    }
}
