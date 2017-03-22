//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

FunctionJITTimeInfo::FunctionJITTimeInfo(FunctionJITTimeDataIDL * data) : m_data(*data)
{LOGMEIN("FunctionJITTimeInfo.cpp] 8\n");
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
{LOGMEIN("FunctionJITTimeInfo.cpp] 22\n");
    jitData->functionInfoAddr = (intptr_t)codeGenData->GetFunctionInfo();

    if (codeGenData->GetFunctionBody() && codeGenData->GetFunctionBody()->GetByteCode())
    {LOGMEIN("FunctionJITTimeInfo.cpp] 26\n");
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
    {LOGMEIN("FunctionJITTimeInfo.cpp] 40\n");
        Assert(isInlinee == !!runtimeData);
        const Js::FunctionCodeGenRuntimeData * targetRuntimeData = nullptr;
        if (runtimeData)
        {LOGMEIN("FunctionJITTimeInfo.cpp] 44\n");
            // may be polymorphic, so seek the runtime data matching our JIT time data
            targetRuntimeData = runtimeData->GetForTarget(codeGenData->GetFunctionInfo()->GetFunctionBody());
        }
        Js::FunctionBody * functionBody = codeGenData->GetFunctionBody();
        if (functionBody->HasDynamicProfileInfo())
        {LOGMEIN("FunctionJITTimeInfo.cpp] 50\n");
            Assert(jitData->bodyData != nullptr);
            ProfileDataIDL * profileData = AnewStruct(alloc, ProfileDataIDL);
            JITTimeProfileInfo::InitializeJITProfileData(alloc, functionBody->GetAnyDynamicProfileInfo(), functionBody, profileData, isForegroundJIT);

            jitData->bodyData->profileData = profileData;

            if (isInlinee)
            {LOGMEIN("FunctionJITTimeInfo.cpp] 58\n");
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
        {LOGMEIN("FunctionJITTimeInfo.cpp] 71\n");
            jitData->inlineeCount = jitData->bodyData->profiledCallSiteCount;
            // using arena because we can't recycler allocate (may be on background), and heap freeing this is slightly complicated
            jitData->inlinees = AnewArrayZ(alloc, FunctionJITTimeDataIDL*, jitData->bodyData->profiledCallSiteCount);
            jitData->inlineesRecursionFlags = AnewArrayZ(alloc, boolean, jitData->bodyData->profiledCallSiteCount);

            for (Js::ProfileId i = 0; i < jitData->bodyData->profiledCallSiteCount; ++i)
            {LOGMEIN("FunctionJITTimeInfo.cpp] 78\n");
                const Js::FunctionCodeGenJitTimeData * inlineeJITData = codeGenData->GetInlinee(i);
                if (inlineeJITData == codeGenData)
                {LOGMEIN("FunctionJITTimeInfo.cpp] 81\n");
                    jitData->inlineesRecursionFlags[i] = TRUE;
                }
                else if (inlineeJITData != nullptr)
                {LOGMEIN("FunctionJITTimeInfo.cpp] 85\n");
                    const Js::FunctionCodeGenRuntimeData * inlineeRuntimeData = nullptr;
                    if (inlineeJITData->GetFunctionInfo()->HasBody())
                    {LOGMEIN("FunctionJITTimeInfo.cpp] 88\n");
                        inlineeRuntimeData = isInlinee ? targetRuntimeData->GetInlinee(i) : functionBody->GetInlineeCodeGenRuntimeData(i);
                    }
                    jitData->inlinees[i] = AnewStructZ(alloc, FunctionJITTimeDataIDL);
                    BuildJITTimeData(alloc, inlineeJITData, inlineeRuntimeData, jitData->inlinees[i], true, isForegroundJIT);
                }
            }
        }
        jitData->profiledRuntimeData = AnewStructZ(alloc, FunctionJITRuntimeIDL);
        if (isInlinee && targetRuntimeData->ClonedInlineCaches()->HasInlineCaches())
        {LOGMEIN("FunctionJITTimeInfo.cpp] 98\n");
            jitData->profiledRuntimeData->clonedCacheCount = jitData->bodyData->inlineCacheCount;
            jitData->profiledRuntimeData->clonedInlineCaches = AnewArray(alloc, intptr_t, jitData->profiledRuntimeData->clonedCacheCount);
            for (uint j = 0; j < jitData->bodyData->inlineCacheCount; ++j)
            {LOGMEIN("FunctionJITTimeInfo.cpp] 102\n");
                jitData->profiledRuntimeData->clonedInlineCaches[j] = (intptr_t)targetRuntimeData->ClonedInlineCaches()->GetInlineCache(j);
            }
        }
        if (jitData->bodyData->inlineCacheCount > 0)
        {LOGMEIN("FunctionJITTimeInfo.cpp] 107\n");
            jitData->ldFldInlineeCount = jitData->bodyData->inlineCacheCount;
            jitData->ldFldInlinees = AnewArrayZ(alloc, FunctionJITTimeDataIDL*, jitData->bodyData->inlineCacheCount);

            Field(Js::ObjTypeSpecFldInfo*)* objTypeSpecInfo = codeGenData->GetObjTypeSpecFldInfoArray()->GetInfoArray();
            if(objTypeSpecInfo)
            {LOGMEIN("FunctionJITTimeInfo.cpp] 113\n");
                jitData->objTypeSpecFldInfoCount = jitData->bodyData->inlineCacheCount;
                jitData->objTypeSpecFldInfoArray = AnewArrayZ(alloc, ObjTypeSpecFldIDL, jitData->bodyData->inlineCacheCount);
                JITObjTypeSpecFldInfo::BuildObjTypeSpecFldInfoArray(alloc, objTypeSpecInfo, jitData->objTypeSpecFldInfoCount, jitData->objTypeSpecFldInfoArray);
            }
            for (Js::InlineCacheIndex i = 0; i < jitData->bodyData->inlineCacheCount; ++i)
            {LOGMEIN("FunctionJITTimeInfo.cpp] 119\n");
                const Js::FunctionCodeGenJitTimeData * inlineeJITData = codeGenData->GetLdFldInlinee(i);
                const Js::FunctionCodeGenRuntimeData * inlineeRuntimeData = isInlinee ? targetRuntimeData->GetLdFldInlinee(i) : functionBody->GetLdFldInlineeCodeGenRuntimeData(i);
                if (inlineeJITData != nullptr)
                {LOGMEIN("FunctionJITTimeInfo.cpp] 123\n");
                    jitData->ldFldInlinees[i] = AnewStructZ(alloc, FunctionJITTimeDataIDL);
                    BuildJITTimeData(alloc, inlineeJITData, inlineeRuntimeData, jitData->ldFldInlinees[i], true, isForegroundJIT);
                }
            }
        }
        if (!isInlinee && codeGenData->GetGlobalObjTypeSpecFldInfoCount() > 0)
        {LOGMEIN("FunctionJITTimeInfo.cpp] 130\n");
            Field(Js::ObjTypeSpecFldInfo*)* globObjTypeSpecInfo = codeGenData->GetGlobalObjTypeSpecFldInfoArray();
            Assert(globObjTypeSpecInfo != nullptr);

            jitData->globalObjTypeSpecFldInfoCount = codeGenData->GetGlobalObjTypeSpecFldInfoCount();
            jitData->globalObjTypeSpecFldInfoArray = AnewArrayZ(alloc, ObjTypeSpecFldIDL, jitData->globalObjTypeSpecFldInfoCount);
            JITObjTypeSpecFldInfo::BuildObjTypeSpecFldInfoArray(alloc, globObjTypeSpecInfo, jitData->globalObjTypeSpecFldInfoCount, jitData->globalObjTypeSpecFldInfoArray);
        }
        const Js::FunctionCodeGenJitTimeData * nextJITData = codeGenData->GetNext();
        if (nextJITData != nullptr)
        {LOGMEIN("FunctionJITTimeInfo.cpp] 140\n");
            // only inlinee should be polymorphic
            Assert(isInlinee);
            jitData->next = AnewStructZ(alloc, FunctionJITTimeDataIDL);
            BuildJITTimeData(alloc, nextJITData, runtimeData, jitData->next, true, isForegroundJIT);
        }
    }
}

uint
FunctionJITTimeInfo::GetInlineeCount() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 151\n");
    return m_data.inlineeCount;
}

bool
FunctionJITTimeInfo::IsLdFldInlineePresent() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 157\n");
    return m_data.ldFldInlineeCount != 0;
}

bool
FunctionJITTimeInfo::HasSharedPropertyGuards() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 163\n");
    return m_data.sharedPropGuardCount != 0;
}

bool
FunctionJITTimeInfo::HasSharedPropertyGuard(Js::PropertyId id) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 169\n");
    for (uint i = 0; i < m_data.sharedPropGuardCount; ++i)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 171\n");
        if (m_data.sharedPropertyGuards[i] == id)
        {LOGMEIN("FunctionJITTimeInfo.cpp] 173\n");
            return true;
        }
    }
    return false;
}

intptr_t
FunctionJITTimeInfo::GetFunctionInfoAddr() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 182\n");
    return m_data.functionInfoAddr;
}

intptr_t
FunctionJITTimeInfo::GetWeakFuncRef() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 188\n");
    return m_data.weakFuncRef;
}

uint
FunctionJITTimeInfo::GetLocalFunctionId() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 194\n");
    return m_data.localFuncId;
}

bool
FunctionJITTimeInfo::IsAggressiveInliningEnabled() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 200\n");
    return m_data.isAggressiveInliningEnabled != FALSE;
}

bool
FunctionJITTimeInfo::IsInlined() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 206\n");
    return m_data.isInlined != FALSE;
}

const BVFixed *
FunctionJITTimeInfo::GetInlineesBV() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 212\n");
    return reinterpret_cast<const BVFixed *>(m_data.inlineesBv);
}

const FunctionJITTimeInfo *
FunctionJITTimeInfo::GetJitTimeDataFromFunctionInfoAddr(intptr_t polyFuncInfo) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 218\n");
    const FunctionJITTimeInfo *next = this;
    while (next && next->GetFunctionInfoAddr() != polyFuncInfo)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 221\n");
        next = next->GetNext();
    }
    return next;
}

const FunctionJITRuntimeInfo *
FunctionJITTimeInfo::GetInlineeForTargetInlineeRuntimeData(const Js::ProfileId profiledCallSiteId, intptr_t inlineeFuncBodyAddr) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 229\n");
    const FunctionJITTimeInfo *inlineeData = GetInlinee(profiledCallSiteId);
    while (inlineeData && inlineeData->GetBody()->GetAddr() != inlineeFuncBodyAddr)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 232\n");
        inlineeData = inlineeData->GetNext();
    }
    __analysis_assume(inlineeData != nullptr);
    return inlineeData->GetRuntimeInfo();
}

const FunctionJITRuntimeInfo *
FunctionJITTimeInfo::GetInlineeRuntimeData(const Js::ProfileId profiledCallSiteId) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 241\n");
    return GetInlinee(profiledCallSiteId) ? GetInlinee(profiledCallSiteId)->GetRuntimeInfo() : nullptr;
}

const FunctionJITRuntimeInfo *
FunctionJITTimeInfo::GetLdFldInlineeRuntimeData(const Js::InlineCacheIndex inlineCacheIndex) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 247\n");
    return GetLdFldInlinee(inlineCacheIndex) ? GetLdFldInlinee(inlineCacheIndex)->GetRuntimeInfo() : nullptr;
}

const FunctionJITRuntimeInfo *
FunctionJITTimeInfo::GetRuntimeInfo() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 253\n");
    return reinterpret_cast<const FunctionJITRuntimeInfo*>(m_data.profiledRuntimeData);
}

JITObjTypeSpecFldInfo *
FunctionJITTimeInfo::GetObjTypeSpecFldInfo(uint index) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 259\n");
    Assert(index < GetBody()->GetInlineCacheCount());
    if (m_data.objTypeSpecFldInfoArray == nullptr)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 262\n");
        return nullptr;
    }
    if (!m_data.objTypeSpecFldInfoArray[index].inUse)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 266\n");
        return nullptr;
    }

    return reinterpret_cast<JITObjTypeSpecFldInfo *>(&m_data.objTypeSpecFldInfoArray[index]);
}

JITObjTypeSpecFldInfo *
FunctionJITTimeInfo::GetGlobalObjTypeSpecFldInfo(uint index) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 275\n");
    Assert(index < m_data.globalObjTypeSpecFldInfoCount);
    if (!m_data.globalObjTypeSpecFldInfoArray[index].inUse)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 278\n");
        return nullptr;
    }

    return reinterpret_cast<JITObjTypeSpecFldInfo *>(&m_data.globalObjTypeSpecFldInfoArray[index]);
}

uint
FunctionJITTimeInfo::GetGlobalObjTypeSpecFldInfoCount() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 287\n");
    return m_data.globalObjTypeSpecFldInfoCount;
}

uint
FunctionJITTimeInfo::GetSourceContextId() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 293\n");
    Assert(HasBody());

    return GetBody()->GetSourceContextId();
}

const FunctionJITTimeInfo *
FunctionJITTimeInfo::GetLdFldInlinee(Js::InlineCacheIndex inlineCacheIndex) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 301\n");
    Assert(inlineCacheIndex < m_data.bodyData->inlineCacheCount);
    if (!m_data.ldFldInlinees)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 304\n");
        return nullptr;
    }
    Assert(inlineCacheIndex < m_data.ldFldInlineeCount);


    return reinterpret_cast<const FunctionJITTimeInfo*>(m_data.ldFldInlinees[inlineCacheIndex]);
}

const FunctionJITTimeInfo *
FunctionJITTimeInfo::GetInlinee(Js::ProfileId profileId) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 315\n");
    Assert(profileId < m_data.bodyData->profiledCallSiteCount);
    if (!m_data.inlinees)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 318\n");
        return nullptr;
    }
    Assert(profileId < m_data.inlineeCount);

    auto inlinee = reinterpret_cast<const FunctionJITTimeInfo *>(m_data.inlinees[profileId]);
    if (inlinee == nullptr && m_data.inlineesRecursionFlags[profileId])
    {LOGMEIN("FunctionJITTimeInfo.cpp] 325\n");
        inlinee = this;
    }
    return inlinee;
}

const FunctionJITTimeInfo *
FunctionJITTimeInfo::GetNext() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 333\n");
    return reinterpret_cast<const FunctionJITTimeInfo *>(m_data.next);
}

JITTimeFunctionBody *
FunctionJITTimeInfo::GetBody() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 339\n");
    return reinterpret_cast<JITTimeFunctionBody *>(m_data.bodyData);
}

bool
FunctionJITTimeInfo::HasBody() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 345\n");
    return m_data.bodyData != nullptr;
}

bool
FunctionJITTimeInfo::IsPolymorphicCallSite(Js::ProfileId profiledCallSiteId) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 351\n");
    Assert(profiledCallSiteId < m_data.bodyData->profiledCallSiteCount);

    if (!m_data.inlinees)
    {LOGMEIN("FunctionJITTimeInfo.cpp] 355\n");
        return false;
    }
    Assert(profiledCallSiteId < m_data.inlineeCount);

    return ((FunctionJITTimeDataIDL*)this->GetInlinee(profiledCallSiteId))->next != nullptr;
}

bool
FunctionJITTimeInfo::ForceJITLoopBody() const
{LOGMEIN("FunctionJITTimeInfo.cpp] 365\n");
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
{LOGMEIN("FunctionJITTimeInfo.cpp] 382\n");
    return GetBody()->GetDisplayName();
}

char16*
FunctionJITTimeInfo::GetDebugNumberSet(wchar(&bufferToWriteTo)[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE]) const
{LOGMEIN("FunctionJITTimeInfo.cpp] 388\n");
    // (#%u.%u), #%u --> (source file Id . function Id) , function Number
    int len = swprintf_s(bufferToWriteTo, MAX_FUNCTION_BODY_DEBUG_STRING_SIZE, _u(" (#%d.%u), #%u"),
        (int)GetSourceContextId(), GetLocalFunctionId(), GetBody()->GetFunctionNumber());
    Assert(len > 8);
    return bufferToWriteTo;
}
