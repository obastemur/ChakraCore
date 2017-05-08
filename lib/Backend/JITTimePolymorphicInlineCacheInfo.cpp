//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimePolymorphicInlineCacheInfo::JITTimePolymorphicInlineCacheInfo()
{TRACE_IT(9941);
    CompileAssert(sizeof(JITTimePolymorphicInlineCacheInfo) == sizeof(PolymorphicInlineCacheInfoIDL));
}

/* static */
void
JITTimePolymorphicInlineCacheInfo::InitializeEntryPointPolymorphicInlineCacheInfo(
    __in Recycler * recycler,
    __in Js::EntryPointPolymorphicInlineCacheInfo * runtimeInfo,
    __out CodeGenWorkItemIDL * jitInfo)
{TRACE_IT(9942);
    if (runtimeInfo == nullptr)
    {TRACE_IT(9943);
        return;
    }
    Js::PolymorphicInlineCacheInfo * selfInfo = runtimeInfo->GetSelfInfo();
    SListCounted<Js::PolymorphicInlineCacheInfo*, Recycler> * inlineeList = runtimeInfo->GetInlineeInfo();
    PolymorphicInlineCacheInfoIDL* selfInfoIDL = RecyclerNewStructZ(recycler, PolymorphicInlineCacheInfoIDL);
    PolymorphicInlineCacheInfoIDL* inlineeInfoIDL = nullptr;

    JITTimePolymorphicInlineCacheInfo::InitializePolymorphicInlineCacheInfo(recycler, selfInfo, selfInfoIDL);
    
    if (!inlineeList->Empty())
    {TRACE_IT(9944);
        inlineeInfoIDL = RecyclerNewArray(recycler, PolymorphicInlineCacheInfoIDL, inlineeList->Count());
        SListCounted<Js::PolymorphicInlineCacheInfo*, Recycler>::Iterator iter(inlineeList);
        uint i = 0;
        while (iter.Next())
        {TRACE_IT(9945);
            Js::PolymorphicInlineCacheInfo * inlineeInfo = iter.Data();
            __analysis_assume(i < inlineeList->Count());
            JITTimePolymorphicInlineCacheInfo::InitializePolymorphicInlineCacheInfo(recycler, inlineeInfo, &inlineeInfoIDL[i]);
            ++i;
        }
        Assert(i == inlineeList->Count());
    }
    jitInfo->inlineeInfoCount = inlineeList->Count();
    jitInfo->selfInfo = selfInfoIDL;
    jitInfo->inlineeInfo = inlineeInfoIDL;
}

/* static */
void
JITTimePolymorphicInlineCacheInfo::InitializePolymorphicInlineCacheInfo(
    __in Recycler * recycler,
    __in Js::PolymorphicInlineCacheInfo * runtimeInfo,
    __out PolymorphicInlineCacheInfoIDL * jitInfo)
{TRACE_IT(9946);
#pragma warning(suppress: 6001)
    jitInfo->polymorphicCacheUtilizationArray = runtimeInfo->GetUtilByteArray();
    jitInfo->functionBodyAddr = runtimeInfo->GetFunctionBody();

    if (runtimeInfo->GetPolymorphicInlineCaches()->HasInlineCaches())
    {TRACE_IT(9947);
        jitInfo->polymorphicInlineCacheCount = runtimeInfo->GetFunctionBody()->GetInlineCacheCount();
        jitInfo->polymorphicInlineCaches = RecyclerNewArrayZ(recycler, PolymorphicInlineCacheIDL, jitInfo->polymorphicInlineCacheCount);
        for (uint j = 0; j < jitInfo->polymorphicInlineCacheCount; ++j)
        {TRACE_IT(9948);
            Js::PolymorphicInlineCache * pic = runtimeInfo->GetPolymorphicInlineCaches()->GetInlineCache(j);
            if (pic != nullptr)
            {TRACE_IT(9949);
                jitInfo->polymorphicInlineCaches[j].size = pic->GetSize();
                jitInfo->polymorphicInlineCaches[j].addr = pic;
                jitInfo->polymorphicInlineCaches[j].inlineCachesAddr = (intptr_t)pic->GetInlineCaches();
            }
        }
    }
}

JITTimePolymorphicInlineCache *
JITTimePolymorphicInlineCacheInfo::GetInlineCache(uint index) const
{TRACE_IT(9950);
    Assert(index < m_data.polymorphicInlineCacheCount);
    if (!m_data.polymorphicInlineCaches[index].addr)
    {TRACE_IT(9951);
        return nullptr;
    }
    return (JITTimePolymorphicInlineCache *)&m_data.polymorphicInlineCaches[index];
}

bool
JITTimePolymorphicInlineCacheInfo::HasInlineCaches() const
{TRACE_IT(9952);
    return m_data.polymorphicInlineCacheCount != 0;
}

byte
JITTimePolymorphicInlineCacheInfo::GetUtil(uint index) const
{TRACE_IT(9953);
    Assert(index < m_data.polymorphicInlineCacheCount);
    return m_data.polymorphicCacheUtilizationArray[index];
}

