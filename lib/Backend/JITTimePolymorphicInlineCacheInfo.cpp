//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimePolymorphicInlineCacheInfo::JITTimePolymorphicInlineCacheInfo()
{LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 8\n");
    CompileAssert(sizeof(JITTimePolymorphicInlineCacheInfo) == sizeof(PolymorphicInlineCacheInfoIDL));
}

/* static */
void
JITTimePolymorphicInlineCacheInfo::InitializeEntryPointPolymorphicInlineCacheInfo(
    __in Recycler * recycler,
    __in Js::EntryPointPolymorphicInlineCacheInfo * runtimeInfo,
    __out CodeGenWorkItemIDL * jitInfo)
{LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 18\n");
    if (runtimeInfo == nullptr)
    {LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 20\n");
        return;
    }
    Js::PolymorphicInlineCacheInfo * selfInfo = runtimeInfo->GetSelfInfo();
    SListCounted<Js::PolymorphicInlineCacheInfo*, Recycler> * inlineeList = runtimeInfo->GetInlineeInfo();
    PolymorphicInlineCacheInfoIDL* selfInfoIDL = RecyclerNewStructZ(recycler, PolymorphicInlineCacheInfoIDL);
    PolymorphicInlineCacheInfoIDL* inlineeInfoIDL = nullptr;

    JITTimePolymorphicInlineCacheInfo::InitializePolymorphicInlineCacheInfo(recycler, selfInfo, selfInfoIDL);
    
    if (!inlineeList->Empty())
    {LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 31\n");
        inlineeInfoIDL = RecyclerNewArray(recycler, PolymorphicInlineCacheInfoIDL, inlineeList->Count());
        SListCounted<Js::PolymorphicInlineCacheInfo*, Recycler>::Iterator iter(inlineeList);
        uint i = 0;
        while (iter.Next())
        {LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 36\n");
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
{LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 55\n");
#pragma warning(suppress: 6001)
    jitInfo->polymorphicCacheUtilizationArray = runtimeInfo->GetUtilByteArray();
    jitInfo->functionBodyAddr = runtimeInfo->GetFunctionBody();

    if (runtimeInfo->GetPolymorphicInlineCaches()->HasInlineCaches())
    {LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 61\n");
        jitInfo->polymorphicInlineCacheCount = runtimeInfo->GetFunctionBody()->GetInlineCacheCount();
        jitInfo->polymorphicInlineCaches = RecyclerNewArrayZ(recycler, PolymorphicInlineCacheIDL, jitInfo->polymorphicInlineCacheCount);
        for (uint j = 0; j < jitInfo->polymorphicInlineCacheCount; ++j)
        {LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 65\n");
            Js::PolymorphicInlineCache * pic = runtimeInfo->GetPolymorphicInlineCaches()->GetInlineCache(j);
            if (pic != nullptr)
            {LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 68\n");
                jitInfo->polymorphicInlineCaches[j].size = pic->GetSize();
                jitInfo->polymorphicInlineCaches[j].addr = pic;
                jitInfo->polymorphicInlineCaches[j].inlineCachesAddr = (intptr_t)pic->GetInlineCaches();
            }
        }
    }
}

JITTimePolymorphicInlineCache *
JITTimePolymorphicInlineCacheInfo::GetInlineCache(uint index) const
{LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 79\n");
    Assert(index < m_data.polymorphicInlineCacheCount);
    if (!m_data.polymorphicInlineCaches[index].addr)
    {LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 82\n");
        return nullptr;
    }
    return (JITTimePolymorphicInlineCache *)&m_data.polymorphicInlineCaches[index];
}

bool
JITTimePolymorphicInlineCacheInfo::HasInlineCaches() const
{LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 90\n");
    return m_data.polymorphicInlineCacheCount != 0;
}

byte
JITTimePolymorphicInlineCacheInfo::GetUtil(uint index) const
{LOGMEIN("JITTimePolymorphicInlineCacheInfo.cpp] 96\n");
    Assert(index < m_data.polymorphicInlineCacheCount);
    return m_data.polymorphicCacheUtilizationArray[index];
}

