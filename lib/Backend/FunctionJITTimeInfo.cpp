//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

FunctionJITTimeInfo::FunctionJITTimeInfo(FunctionJITTimeDataIDL * data) : m_data(*data)
{TRACE_IT(2823);
    // we will cast the data (i.e. midl struct) pointers into info pointers so we can extend with methods
    CompileAssert(sizeof(FunctionJITTimeDataIDL) == sizeof(FunctionJITTimeInfo));
}

/* static */
void
FunctionJITTimeInfo::BuildJITTimeData(
    __in ArenaAllocator * alloc,
    __in const Js::FunctionCodeGenJitTimeData * codeGenData,
    __in_opt const Js::FunctionCodeGenRuntimeData * runtimeData,
    __out FunctionJITTimeDataIDL * jitData,
    bool isInlinee,
    bool isForegroundJIT)
{TRACE_IT(2824);
    jitData->functionInfoAddr = (intptr_t)codeGenData->GetFunctionInfo();

    if (codeGenData->GetFunctionBody() && codeGenData->GetFunctionBody()->GetByteCode())
    {TRACE_IT(2825);
        Js::FunctionBody * body = codeGenData->GetFunctionInfo()->GetParseableFunctionInfo()->GetFunctionBody();
        jitData->bodyData = AnewStructZ(alloc, FunctionBodyDataIDL);
        JITTimeFunctionBody::InitializeJITFunctionData(alloc, body, jitData->bodyData);
    }

    jitData->localFuncId = codeGenData->GetFunctionInfo()->GetLocalFunctionId();
    jitData->isAggressiveInliningEnabled = codeGenData->GetIsAggressiveInliningEnabled();
    jitData->isInlined = codeGenData->GetIsInlined();
    jitData->weakFuncRef = (intptr_t)codeGenData->GetWeakFuncRef();

    jitData->inlineesBv = (BVFixedIDL*)(const BVFixed*)codeGenData->inlineesBv;

    if (codeGenData->GetFunctionInfo()->HasBody() && codeGenData->GetFunctionInfo()->GetFunctionProxy()->IsFunctionBody())
    {TRACE_IT(2826);
        Assert(isInlinee == !!runtimeData);
        const Js::FunctionCodeGenRuntimeData * targetRuntimeData = nullptr;
        if (runtimeData)
        {TRACE_IT(2827);
            // may be polymorphic, so seek the runtime data matching our JIT time data
            targetRuntimeData = runtimeData->GetForTarget(codeGenData->GetFunctionInfo()->GetFunctionBody());
        }
        Js::FunctionBody * functionBody = codeGenData->GetFunctionBody();
        if (functionBody->HasDynamicProfileInfo())
        {TRACE_IT(2828);
            Assert(jitData->bodyData != nullptr);
            ProfileDataIDL * profileData = AnewStruct(alloc, ProfileDataIDL);
            JITTimeProfileInfo::InitializeJITProfileData(alloc, functionBody->GetAnyDynamicProfileInfo(), functionBody, profileData, isForegroundJIT);

            jitData->bodyData->profileData = profileData;

            if (isInlinee)
            {TRACE_IT(2829);
                // if not inlinee, NativeCodeGenerator will provide the address
                // REVIEW: OOP JIT, for inlinees, is this actually necessary?
                Js::ProxyEntryPointInfo *defaultEntryPointInfo = functionBody->GetDefaultEntryPointInfo();
                Assert(defaultEntryPointInfo->IsFunctionEntryPointInfo());
                Js::FunctionEntryPointInfo *functionEntryPointInfo = static_cast<Js::FunctionEntryPointInfo*>(defaultEntryPointInfo);
                jitData->callsCountAddress = (intptr_t)&functionEntryPointInfo->callsCount;
                
                jitData->sharedPropertyGuards = codeGenData->sharedPropertyGuards;
                jitData->sharedPropGuardCount = codeGenData->sharedPropertyGuardCount;
            }
        }
        if (jitData->bodyData->profiledCallSiteCount > 0)
        {TRACE_IT(2830);
            jitData->inlineeCount = jitData->bodyData->profiledCallSiteCount;
            // using arena because we can't recycler allocate (may be on background), and heap freeing this is slightly complicated
            jitData->inlinees = AnewArrayZ(alloc, FunctionJITTimeDataIDL*, jitData->bodyData->profiledCallSiteCount);
            jitData->inlineesRecursionFlags = AnewArrayZ(alloc, boolean, jitData->bodyData->profiledCallSiteCount);

            for (Js::ProfileId i = 0; i < jitData->bodyData->profiledCallSiteCount; ++i)
            {TRACE_IT(2831);
                const Js::FunctionCodeGenJitTimeData * inlineeJITData = codeGenData->GetInlinee(i);
                if (inlineeJITData == codeGenData)
                {TRACE_IT(2832);
                    jitData->inlineesRecursionFlags[i] = TRUE;
                }
                else if (inlineeJITData != nullptr)
                {TRACE_IT(2833);
                    const Js::FunctionCodeGenRuntimeData * inlineeRuntimeData = nullptr;
                    if (inlineeJITData->GetFunctionInfo()->HasBody())
                    {TRACE_IT(2834);
                        inlineeRuntimeData = isInlinee ? targetRuntimeData->GetInlinee(i) : functionBody->GetInlineeCodeGenRuntimeData(i);
                    }
                    jitData->inlinees[i] = AnewStructZ(alloc, FunctionJITTimeDataIDL);
                    BuildJITTimeData(alloc, inlineeJITData, inlineeRuntimeData, jitData->inlinees[i], true, isForegroundJIT);
                }
            }
        }
        jitData->profiledRuntimeData = AnewStructZ(alloc, FunctionJITRuntimeIDL);
        if (isInlinee && targetRuntimeData->ClonedInlineCaches()->HasInlineCaches())
        {TRACE_IT(2835);
            jitData->profiledRuntimeData->clonedCacheCount = jitData->bodyData->inlineCacheCount;
            jitData->profiledRuntimeData->clonedInlineCaches = AnewArray(alloc, intptr_t, jitData->profiledRuntimeData->clonedCacheCount);
            for (uint j = 0; j < jitData->bodyData->inlineCacheCount; ++j)
            {TRACE_IT(2836);
                jitData->profiledRuntimeData->clonedInlineCaches[j] = (intptr_t)targetRuntimeData->ClonedInlineCaches()->GetInlineCache(j);
            }
        }
        if (jitData->bodyData->inlineCacheCount > 0)
        {TRACE_IT(2837);
            jitData->ldFldInlineeCount = jitData->bodyData->inlineCacheCount;
            jitData->ldFldInlinees = AnewArrayZ(alloc, FunctionJITTimeDataIDL*, jitData->bodyData->inlineCacheCount);

            Field(Js::ObjTypeSpecFldInfo*)* objTypeSpecInfo = codeGenData->GetObjTypeSpecFldInfoArray()->GetInfoArray();
            if(objTypeSpecInfo)
            {TRACE_IT(2838);
                jitData->objTypeSpecFldInfoCount = jitData->bodyData->inlineCacheCount;
                jitData->objTypeSpecFldInfoArray = AnewArrayZ(alloc, ObjTypeSpecFldIDL, jitData->bodyData->inlineCacheCount);
                JITObjTypeSpecFldInfo::BuildObjTypeSpecFldInfoArray(alloc, objTypeSpecInfo, jitData->objTypeSpecFldInfoCount, jitData->objTypeSpecFldInfoArray);
            }
            for (Js::InlineCacheIndex i = 0; i < jitData->bodyData->inlineCacheCount; ++i)
            {TRACE_IT(2839);
                const Js::FunctionCodeGenJitTimeData * inlineeJITData = codeGenData->GetLdFldInlinee(i);
                const Js::FunctionCodeGenRuntimeData * inlineeRuntimeData = isInlinee ? targetRuntimeData->GetLdFldInlinee(i) : functionBody->GetLdFldInlineeCodeGenRuntimeData(i);
                if (inlineeJITData != nullptr)
                {TRACE_IT(2840);
                    jitData->ldFldInlinees[i] = AnewStructZ(alloc, FunctionJITTimeDataIDL);
                    BuildJITTimeData(alloc, inlineeJITData, inlineeRuntimeData, jitData->ldFldInlinees[i], true, isForegroundJIT);
                }
            }
        }
        if (!isInlinee && codeGenData->GetGlobalObjTypeSpecFldInfoCount() > 0)
        {TRACE_IT(2841);
            Field(Js::ObjTypeSpecFldInfo*)* globObjTypeSpecInfo = codeGenData->GetGlobalObjTypeSpecFldInfoArray();
            Assert(globObjTypeSpecInfo != nullptr);

            jitData->globalObjTypeSpecFldInfoCount = codeGenData->GetGlobalObjTypeSpecFldInfoCount();
            jitData->globalObjTypeSpecFldInfoArray = AnewArrayZ(alloc, ObjTypeSpecFldIDL, jitData->globalObjTypeSpecFldInfoCount);
            JITObjTypeSpecFldInfo::BuildObjTypeSpecFldInfoArray(alloc, globObjTypeSpecInfo, jitData->globalObjTypeSpecFldInfoCount, jitData->globalObjTypeSpecFldInfoArray);
        }
        const Js::FunctionCodeGenJitTimeData * nextJITData = codeGenData->GetNext();
        if (nextJITData != nullptr)
        {TRACE_IT(2842);
            // only inlinee should be polymorphic
            Assert(isInlinee);
            jitData->next = AnewStructZ(alloc, FunctionJITTimeDataIDL);
            BuildJITTimeData(alloc, nextJITData, runtimeData, jitData->next, true, isForegroundJIT);
        }
    }
}

uint
FunctionJITTimeInfo::GetInlineeCount() const
{TRACE_IT(2843);
    return m_data.inlineeCount;
}

bool
FunctionJITTimeInfo::IsLdFldInlineePresent() const
{TRACE_IT(2844);
    return m_data.ldFldInlineeCount != 0;
}

bool
FunctionJITTimeInfo::HasSharedPropertyGuards() const
{TRACE_IT(2845);
    return m_data.sharedPropGuardCount != 0;
}

bool
FunctionJITTimeInfo::HasSharedPropertyGuard(Js::PropertyId id) const
{TRACE_IT(2846);
    for (uint i = 0; i < m_data.sharedPropGuardCount; ++i)
    {TRACE_IT(2847);
        if (m_data.sharedPropertyGuards[i] == id)
        {TRACE_IT(2848);
            return true;
        }
    }
    return false;
}

intptr_t
FunctionJITTimeInfo::GetFunctionInfoAddr() const
{TRACE_IT(2849);
    return m_data.functionInfoAddr;
}

intptr_t
FunctionJITTimeInfo::GetWeakFuncRef() const
{TRACE_IT(2850);
    return m_data.weakFuncRef;
}

uint
FunctionJITTimeInfo::GetLocalFunctionId() const
{TRACE_IT(2851);
    return m_data.localFuncId;
}

bool
FunctionJITTimeInfo::IsAggressiveInliningEnabled() const
{TRACE_IT(2852);
    return m_data.isAggressiveInliningEnabled != FALSE;
}

bool
FunctionJITTimeInfo::IsInlined() const
{TRACE_IT(2853);
    return m_data.isInlined != FALSE;
}

const BVFixed *
FunctionJITTimeInfo::GetInlineesBV() const
{TRACE_IT(2854);
    return reinterpret_cast<const BVFixed *>(m_data.inlineesBv);
}

const FunctionJITTimeInfo *
FunctionJITTimeInfo::GetJitTimeDataFromFunctionInfoAddr(intptr_t polyFuncInfo) const
{TRACE_IT(2855);
    const FunctionJITTimeInfo *next = this;
    while (next && next->GetFunctionInfoAddr() != polyFuncInfo)
    {TRACE_IT(2856);
        next = next->GetNext();
    }
    return next;
}

const FunctionJITRuntimeInfo *
FunctionJITTimeInfo::GetInlineeForTargetInlineeRuntimeData(const Js::ProfileId profiledCallSiteId, intptr_t inlineeFuncBodyAddr) const
{TRACE_IT(2857);
    const FunctionJITTimeInfo *inlineeData = GetInlinee(profiledCallSiteId);
    while (inlineeData && inlineeData->GetBody()->GetAddr() != inlineeFuncBodyAddr)
    {TRACE_IT(2858);
        inlineeData = inlineeData->GetNext();
    }
    __analysis_assume(inlineeData != nullptr);
    return inlineeData->GetRuntimeInfo();
}

const FunctionJITRuntimeInfo *
FunctionJITTimeInfo::GetInlineeRuntimeData(const Js::ProfileId profiledCallSiteId) const
{TRACE_IT(2859);
    return GetInlinee(profiledCallSiteId) ? GetInlinee(profiledCallSiteId)->GetRuntimeInfo() : nullptr;
}

const FunctionJITRuntimeInfo *
FunctionJITTimeInfo::GetLdFldInlineeRuntimeData(const Js::InlineCacheIndex inlineCacheIndex) const
{TRACE_IT(2860);
    return GetLdFldInlinee(inlineCacheIndex) ? GetLdFldInlinee(inlineCacheIndex)->GetRuntimeInfo() : nullptr;
}

const FunctionJITRuntimeInfo *
FunctionJITTimeInfo::GetRuntimeInfo() const
{TRACE_IT(2861);
    return reinterpret_cast<const FunctionJITRuntimeInfo*>(m_data.profiledRuntimeData);
}

JITObjTypeSpecFldInfo *
FunctionJITTimeInfo::GetObjTypeSpecFldInfo(uint index) const
{TRACE_IT(2862);
    Assert(index < GetBody()->GetInlineCacheCount());
    if (m_data.objTypeSpecFldInfoArray == nullptr)
    {TRACE_IT(2863);
        return nullptr;
    }
    if (!m_data.objTypeSpecFldInfoArray[index].inUse)
    {TRACE_IT(2864);
        return nullptr;
    }

    return reinterpret_cast<JITObjTypeSpecFldInfo *>(&m_data.objTypeSpecFldInfoArray[index]);
}

JITObjTypeSpecFldInfo *
FunctionJITTimeInfo::GetGlobalObjTypeSpecFldInfo(uint index) const
{TRACE_IT(2865);
    Assert(index < m_data.globalObjTypeSpecFldInfoCount);
    if (!m_data.globalObjTypeSpecFldInfoArray[index].inUse)
    {TRACE_IT(2866);
        return nullptr;
    }

    return reinterpret_cast<JITObjTypeSpecFldInfo *>(&m_data.globalObjTypeSpecFldInfoArray[index]);
}

uint
FunctionJITTimeInfo::GetGlobalObjTypeSpecFldInfoCount() const
{TRACE_IT(2867);
    return m_data.globalObjTypeSpecFldInfoCount;
}

uint
FunctionJITTimeInfo::GetSourceContextId() const
{TRACE_IT(2868);
    Assert(HasBody());

    return GetBody()->GetSourceContextId();
}

const FunctionJITTimeInfo *
FunctionJITTimeInfo::GetLdFldInlinee(Js::InlineCacheIndex inlineCacheIndex) const
{TRACE_IT(2869);
    Assert(inlineCacheIndex < m_data.bodyData->inlineCacheCount);
    if (!m_data.ldFldInlinees)
    {TRACE_IT(2870);
        return nullptr;
    }
    Assert(inlineCacheIndex < m_data.ldFldInlineeCount);


    return reinterpret_cast<const FunctionJITTimeInfo*>(m_data.ldFldInlinees[inlineCacheIndex]);
}

const FunctionJITTimeInfo *
FunctionJITTimeInfo::GetInlinee(Js::ProfileId profileId) const
{TRACE_IT(2871);
    Assert(profileId < m_data.bodyData->profiledCallSiteCount);
    if (!m_data.inlinees)
    {TRACE_IT(2872);
        return nullptr;
    }
    Assert(profileId < m_data.inlineeCount);

    auto inlinee = reinterpret_cast<const FunctionJITTimeInfo *>(m_data.inlinees[profileId]);
    if (inlinee == nullptr && m_data.inlineesRecursionFlags[profileId])
    {TRACE_IT(2873);
        inlinee = this;
    }
    return inlinee;
}

const FunctionJITTimeInfo *
FunctionJITTimeInfo::GetNext() const
{TRACE_IT(2874);
    return reinterpret_cast<const FunctionJITTimeInfo *>(m_data.next);
}

JITTimeFunctionBody *
FunctionJITTimeInfo::GetBody() const
{TRACE_IT(2875);
    return reinterpret_cast<JITTimeFunctionBody *>(m_data.bodyData);
}

bool
FunctionJITTimeInfo::HasBody() const
{TRACE_IT(2876);
    return m_data.bodyData != nullptr;
}

bool
FunctionJITTimeInfo::IsPolymorphicCallSite(Js::ProfileId profiledCallSiteId) const
{TRACE_IT(2877);
    Assert(profiledCallSiteId < m_data.bodyData->profiledCallSiteCount);

    if (!m_data.inlinees)
    {TRACE_IT(2878);
        return false;
    }
    Assert(profiledCallSiteId < m_data.inlineeCount);

    return ((FunctionJITTimeDataIDL*)this->GetInlinee(profiledCallSiteId))->next != nullptr;
}

bool
FunctionJITTimeInfo::ForceJITLoopBody() const
{TRACE_IT(2879);
    return
        !PHASE_OFF(Js::JITLoopBodyPhase, this) &&
        !PHASE_OFF(Js::FullJitPhase, this) &&
        !GetBody()->IsGenerator() &&
        !GetBody()->HasTry() &&
        (
            PHASE_FORCE(Js::JITLoopBodyPhase, this)
#ifdef ENABLE_PREJIT
            || Js::Configuration::Global.flags.Prejit
#endif
            );
}


char16*
FunctionJITTimeInfo::GetDisplayName() const
{TRACE_IT(2880);
    return GetBody()->GetDisplayName();
}

char16*
FunctionJITTimeInfo::GetDebugNumberSet(wchar(&bufferToWriteTo)[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE]) const
{TRACE_IT(2881);
    // (#%u.%u), #%u --> (source file Id . function Id) , function Number
    int len = swprintf_s(bufferToWriteTo, MAX_FUNCTION_BODY_DEBUG_STRING_SIZE, _u(" (#%d.%u), #%u"),
        (int)GetSourceContextId(), GetLocalFunctionId(), GetBody()->GetFunctionNumber());
    Assert(len > 8);
    return bufferToWriteTo;
}
