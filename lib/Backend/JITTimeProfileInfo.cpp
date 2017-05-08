//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimeProfileInfo::JITTimeProfileInfo(ProfileDataIDL * profileData) :
    m_profileData(*profileData)
{TRACE_IT(9954);
    CompileAssert(sizeof(JITTimeProfileInfo) == sizeof(ProfileDataIDL));
}

/* static */
void
JITTimeProfileInfo::InitializeJITProfileData(
    __in ArenaAllocator * alloc,
    __in Js::DynamicProfileInfo * profileInfo,
    __in Js::FunctionBody *functionBody,
    __out ProfileDataIDL * data,
    bool isForegroundJIT)
{TRACE_IT(9955);
    if (profileInfo == nullptr)
    {TRACE_IT(9956);
        return;
    }

    CompileAssert(sizeof(LdElemIDL) == sizeof(Js::LdElemInfo));
    CompileAssert(sizeof(StElemIDL) == sizeof(Js::StElemInfo));

    data->profiledLdElemCount = functionBody->GetProfiledLdElemCount();
    data->profiledStElemCount = functionBody->GetProfiledStElemCount();

    if (JITManager::GetJITManager()->IsOOPJITEnabled() || isForegroundJIT)
    {TRACE_IT(9957);
        data->ldElemData = (LdElemIDL*)profileInfo->GetLdElemInfo();
        data->stElemData = (StElemIDL*)profileInfo->GetStElemInfo();
    }
    else
    {TRACE_IT(9958);
        // for in-proc background JIT we need to explicitly copy LdElem and StElem info
        data->ldElemData = AnewArray(alloc, LdElemIDL, data->profiledLdElemCount);
        memcpy_s(
            data->ldElemData,
            data->profiledLdElemCount * sizeof(LdElemIDL),
            profileInfo->GetLdElemInfo(),
            functionBody->GetProfiledLdElemCount() * sizeof(Js::LdElemInfo)
        );

        data->stElemData = AnewArray(alloc, StElemIDL, data->profiledStElemCount);
        memcpy_s(
            data->stElemData,
            data->profiledStElemCount * sizeof(StElemIDL),
            profileInfo->GetStElemInfo(),
            functionBody->GetProfiledStElemCount() * sizeof(Js::StElemInfo)
        );
    }

    CompileAssert(sizeof(ArrayCallSiteIDL) == sizeof(Js::ArrayCallSiteInfo));
    data->profiledArrayCallSiteCount = functionBody->GetProfiledArrayCallSiteCount();
    data->arrayCallSiteData = (ArrayCallSiteIDL*)profileInfo->GetArrayCallSiteInfo();
    data->arrayCallSiteDataAddr = (intptr_t)profileInfo->GetArrayCallSiteInfo();

    CompileAssert(sizeof(FldIDL) == sizeof(Js::FldInfo));
    data->inlineCacheCount = functionBody->GetProfiledFldCount();
    data->fldData = (FldIDL*)profileInfo->GetFldInfo();
    data->fldDataAddr = (intptr_t)profileInfo->GetFldInfo();

    CompileAssert(sizeof(ThisIDL) == sizeof(Js::ThisInfo));
    data->thisData = *reinterpret_cast<ThisIDL*>(&profileInfo->GetThisInfo());

    CompileAssert(sizeof(CallSiteIDL) == sizeof(Js::CallSiteInfo));
    data->profiledCallSiteCount = functionBody->GetProfiledCallSiteCount();
    data->callSiteData = reinterpret_cast<CallSiteIDL*>(profileInfo->GetCallSiteInfo());

    CompileAssert(sizeof(BVUnitIDL) == sizeof(BVUnit));
    data->loopFlags = (BVFixedIDL*)profileInfo->GetLoopFlags();

    CompileAssert(sizeof(ValueType) == sizeof(uint16));

    data->profiledSlotCount = functionBody->GetProfiledSlotCount();
    data->slotData = reinterpret_cast<uint16*>(profileInfo->GetSlotInfo());

    data->profiledReturnTypeCount = functionBody->GetProfiledReturnTypeCount();
    data->returnTypeData = reinterpret_cast<uint16*>(profileInfo->GetReturnTypeInfo());

    data->profiledDivOrRemCount = functionBody->GetProfiledDivOrRemCount();
    data->divideTypeInfo = reinterpret_cast<uint16*>(profileInfo->GetDivideTypeInfo());

    data->profiledSwitchCount = functionBody->GetProfiledSwitchCount();
    data->switchTypeInfo = reinterpret_cast<uint16*>(profileInfo->GetSwitchTypeInfo());

    data->profiledInParamsCount = functionBody->GetProfiledInParamsCount();
    data->parameterInfo = reinterpret_cast<uint16*>(profileInfo->GetParameterInfo());

    data->loopCount = functionBody->GetLoopCount();
    data->loopImplicitCallFlags = reinterpret_cast<byte*>(profileInfo->GetLoopImplicitCallFlags());

    data->implicitCallFlags = static_cast<byte>(profileInfo->GetImplicitCallFlags());

    data->flags = 0;
    data->flags |= profileInfo->IsAggressiveIntTypeSpecDisabled(false) ? Flags_disableAggressiveIntTypeSpec : 0;
    data->flags |= profileInfo->IsAggressiveIntTypeSpecDisabled(true) ? Flags_disableAggressiveIntTypeSpec_jitLoopBody : 0;
    data->flags |= profileInfo->IsAggressiveMulIntTypeSpecDisabled(false) ? Flags_disableAggressiveMulIntTypeSpec : 0;
    data->flags |= profileInfo->IsAggressiveMulIntTypeSpecDisabled(true) ? Flags_disableAggressiveMulIntTypeSpec_jitLoopBody : 0;
    data->flags |= profileInfo->IsDivIntTypeSpecDisabled(false) ? Flags_disableDivIntTypeSpec : 0;
    data->flags |= profileInfo->IsDivIntTypeSpecDisabled(true) ? Flags_disableDivIntTypeSpec_jitLoopBody : 0;
    data->flags |= profileInfo->IsLossyIntTypeSpecDisabled() ? Flags_disableLossyIntTypeSpec : 0;
    data->flags |= profileInfo->IsTrackCompoundedIntOverflowDisabled() ? Flags_disableTrackCompoundedIntOverflow : 0;
    data->flags |= profileInfo->IsFloatTypeSpecDisabled() ? Flags_disableFloatTypeSpec : 0;
    data->flags |= profileInfo->IsArrayCheckHoistDisabled(false) ? Flags_disableArrayCheckHoist : 0;
    data->flags |= profileInfo->IsArrayCheckHoistDisabled(true) ? Flags_disableArrayCheckHoist_jitLoopBody : 0;
    data->flags |= profileInfo->IsArrayMissingValueCheckHoistDisabled(false) ? Flags_disableArrayMissingValueCheckHoist : 0;
    data->flags |= profileInfo->IsArrayMissingValueCheckHoistDisabled(true) ? Flags_disableArrayMissingValueCheckHoist_jitLoopBody : 0;
    data->flags |= profileInfo->IsJsArraySegmentHoistDisabled(false) ? Flags_disableJsArraySegmentHoist : 0;
    data->flags |= profileInfo->IsJsArraySegmentHoistDisabled(true) ? Flags_disableJsArraySegmentHoist_jitLoopBody : 0;
    data->flags |= profileInfo->IsArrayLengthHoistDisabled(false) ? Flags_disableArrayLengthHoist : 0;
    data->flags |= profileInfo->IsArrayLengthHoistDisabled(true) ? Flags_disableArrayLengthHoist_jitLoopBody : 0;
    data->flags |= profileInfo->IsTypedArrayTypeSpecDisabled(false) ? Flags_disableTypedArrayTypeSpec : 0;
    data->flags |= profileInfo->IsTypedArrayTypeSpecDisabled(true) ? Flags_disableTypedArrayTypeSpec_jitLoopBody : 0;
    data->flags |= profileInfo->IsLdLenIntSpecDisabled() ? Flags_disableLdLenIntSpec : 0;
    data->flags |= profileInfo->IsBoundCheckHoistDisabled(false) ? Flags_disableBoundCheckHoist : 0;
    data->flags |= profileInfo->IsBoundCheckHoistDisabled(true) ? Flags_disableBoundCheckHoist_jitLoopBody : 0;
    data->flags |= profileInfo->IsLoopCountBasedBoundCheckHoistDisabled(false) ? Flags_disableLoopCountBasedBoundCheckHoist : 0;
    data->flags |= profileInfo->IsLoopCountBasedBoundCheckHoistDisabled(true) ? Flags_disableLoopCountBasedBoundCheckHoist_jitLoopBody : 0;
    data->flags |= profileInfo->IsFloorInliningDisabled() ? Flags_disableFloorInlining : 0;
    data->flags |= profileInfo->IsNoProfileBailoutsDisabled() ? Flags_disableNoProfileBailouts : 0;
    data->flags |= profileInfo->IsSwitchOptDisabled() ? Flags_disableSwitchOpt : 0;
    data->flags |= profileInfo->IsEquivalentObjTypeSpecDisabled() ? Flags_disableEquivalentObjTypeSpec : 0;
    data->flags |= profileInfo->IsObjTypeSpecDisabledInJitLoopBody() ? Flags_disableObjTypeSpec_jitLoopBody : 0;
    data->flags |= profileInfo->IsMemOpDisabled() ? Flags_disableMemOp : 0;
    data->flags |= profileInfo->IsCheckThisDisabled() ? Flags_disableCheckThis : 0;
    data->flags |= profileInfo->HasLdFldCallSiteInfo() ? Flags_hasLdFldCallSiteInfo : 0;
    data->flags |= profileInfo->IsStackArgOptDisabled() ? Flags_disableStackArgOpt : 0;
    data->flags |= profileInfo->IsLoopImplicitCallInfoDisabled() ? Flags_disableLoopImplicitCallInfo : 0;
    data->flags |= profileInfo->IsPowIntIntTypeSpecDisabled() ? Flags_disablePowIntIntTypeSpec : 0;
    data->flags |= profileInfo->IsTagCheckDisabled() ? Flags_disableTagCheck : 0;
}

void
JITTimeProfileInfo::DisableAggressiveIntTypeSpec(bool isLoopBody)
{TRACE_IT(9959);
    m_profileData.flags |= isLoopBody ? Flags_disableAggressiveIntTypeSpec_jitLoopBody : Flags_disableAggressiveIntTypeSpec;
}

void
JITTimeProfileInfo::DisableStackArgOpt()
{TRACE_IT(9960);
    m_profileData.flags |= Flags_disableStackArgOpt;
}

void
JITTimeProfileInfo::DisableSwitchOpt()
{TRACE_IT(9961);
    m_profileData.flags |= Flags_disableSwitchOpt;
}

void
JITTimeProfileInfo::DisableTrackCompoundedIntOverflow()
{TRACE_IT(9962);
    m_profileData.flags |= Flags_disableTrackCompoundedIntOverflow;
}

const Js::LdElemInfo *
JITTimeProfileInfo::GetLdElemInfo(Js::ProfileId ldElemId) const
{TRACE_IT(9963);
    return &(reinterpret_cast<Js::LdElemInfo*>(m_profileData.ldElemData)[ldElemId]);
}

const Js::StElemInfo *
JITTimeProfileInfo::GetStElemInfo(Js::ProfileId stElemId) const
{TRACE_IT(9964);
    return &(reinterpret_cast<Js::StElemInfo*>(m_profileData.stElemData)[stElemId]);
}

Js::ArrayCallSiteInfo *
JITTimeProfileInfo::GetArrayCallSiteInfo(Js::ProfileId index) const
{TRACE_IT(9965);
    Assert(index < GetProfiledArrayCallSiteCount());
    return &(reinterpret_cast<Js::ArrayCallSiteInfo*>(m_profileData.arrayCallSiteData)[index]);
}

intptr_t
JITTimeProfileInfo::GetArrayCallSiteInfoAddr(Js::ProfileId index) const
{TRACE_IT(9966);
    Assert(index < GetProfiledArrayCallSiteCount());
    return m_profileData.arrayCallSiteDataAddr + index * sizeof(ArrayCallSiteIDL);
}

Js::FldInfo *
JITTimeProfileInfo::GetFldInfo(uint fieldAccessId) const
{TRACE_IT(9967);
    Assert(fieldAccessId < GetProfiledFldCount());
    return &(reinterpret_cast<Js::FldInfo*>(m_profileData.fldData)[fieldAccessId]);
}

intptr_t
JITTimeProfileInfo::GetFldInfoAddr(uint fieldAccessId) const
{TRACE_IT(9968);
    Assert(fieldAccessId < GetProfiledFldCount());
    return m_profileData.fldDataAddr + fieldAccessId;
}

ValueType
JITTimeProfileInfo::GetSlotLoad(Js::ProfileId slotLoadId) const
{TRACE_IT(9969);
    Assert(slotLoadId < GetProfiledSlotCount());
    return reinterpret_cast<ValueType*>(m_profileData.slotData)[slotLoadId];
}

Js::ThisInfo
JITTimeProfileInfo::GetThisInfo() const
{TRACE_IT(9970);
    return *reinterpret_cast<const Js::ThisInfo*>(&m_profileData.thisData);
}

ValueType
JITTimeProfileInfo::GetReturnType(Js::OpCode opcode, Js::ProfileId callSiteId) const
{TRACE_IT(9971);
    if (opcode < Js::OpCode::ProfiledReturnTypeCallI || (opcode > Js::OpCode::ProfiledReturnTypeCallIFlags && opcode < Js::OpCode::ProfiledReturnTypeCallIExtended) || opcode > Js::OpCode::ProfiledReturnTypeCallIExtendedFlags)
    {TRACE_IT(9972);
        Assert(Js::DynamicProfileInfo::IsProfiledCallOp(opcode));
        Assert(callSiteId < GetProfiledCallSiteCount());
        return GetCallSiteInfo()[callSiteId].returnType;
    }
    Assert(Js::DynamicProfileInfo::IsProfiledReturnTypeOp(opcode));
    Assert(callSiteId < GetProfiledReturnTypeCount());
    return reinterpret_cast<ValueType*>(m_profileData.returnTypeData)[callSiteId];
}

ValueType
JITTimeProfileInfo::GetDivProfileInfo(Js::ProfileId divideId) const
{TRACE_IT(9973);
    Assert(divideId < GetProfiledDivOrRemCount());
    return reinterpret_cast<ValueType*>(m_profileData.divideTypeInfo)[divideId];
}

ValueType
JITTimeProfileInfo::GetSwitchProfileInfo(Js::ProfileId switchId) const
{TRACE_IT(9974);
    Assert(switchId < GetProfiledSwitchCount());
    return reinterpret_cast<ValueType*>(m_profileData.switchTypeInfo)[switchId];
}

ValueType
JITTimeProfileInfo::GetParameterInfo(Js::ArgSlot index) const
{TRACE_IT(9975);
    Assert(index < GetProfiledInParamsCount());
    return reinterpret_cast<ValueType*>(m_profileData.parameterInfo)[index];
}

Js::ImplicitCallFlags
JITTimeProfileInfo::GetLoopImplicitCallFlags(uint loopNum) const
{TRACE_IT(9976);
    // TODO: michhol OOP JIT, investigate vaibility of reenabling this assert
    // Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(functionBody));
    Assert(loopNum < GetLoopCount());

    // Mask out the dispose implicit call. We would bailout on reentrant dispose,
    // but it shouldn't affect optimization.
    return (Js::ImplicitCallFlags)(m_profileData.loopImplicitCallFlags[loopNum] & Js::ImplicitCall_All);
}

Js::ImplicitCallFlags
JITTimeProfileInfo::GetImplicitCallFlags() const
{TRACE_IT(9977);
    return static_cast<Js::ImplicitCallFlags>(m_profileData.implicitCallFlags);
}

Js::LoopFlags
JITTimeProfileInfo::GetLoopFlags(uint loopNum) const
{TRACE_IT(9978);
    Assert(GetLoopFlags() != nullptr);
    return GetLoopFlags()->GetRange<Js::LoopFlags>(loopNum * Js::LoopFlags::COUNT, Js::LoopFlags::COUNT);
}

uint16
JITTimeProfileInfo::GetConstantArgInfo(Js::ProfileId callSiteId) const
{TRACE_IT(9979);
    return GetCallSiteInfo()[callSiteId].isArgConstant;
}

bool
JITTimeProfileInfo::IsModulusOpByPowerOf2(Js::ProfileId profileId) const
{TRACE_IT(9980);
    return GetDivProfileInfo(profileId).IsLikelyTaggedInt();
}

bool
JITTimeProfileInfo::IsAggressiveIntTypeSpecDisabled(const bool isJitLoopBody) const
{TRACE_IT(9981);
    if (isJitLoopBody)
    {TRACE_IT(9982);
        return TestFlag(Flags_disableAggressiveIntTypeSpec_jitLoopBody);
    }
    else
    {TRACE_IT(9983);
        return TestFlag(Flags_disableAggressiveIntTypeSpec);
    }
}

bool
JITTimeProfileInfo::IsSwitchOptDisabled() const
{TRACE_IT(9984);
    return TestFlag(Flags_disableSwitchOpt);
}

bool
JITTimeProfileInfo::IsEquivalentObjTypeSpecDisabled() const
{TRACE_IT(9985);
    return TestFlag(Flags_disableEquivalentObjTypeSpec);
}

bool
JITTimeProfileInfo::IsObjTypeSpecDisabledInJitLoopBody() const
{TRACE_IT(9986);
    return TestFlag(Flags_disableObjTypeSpec_jitLoopBody);
}

bool
JITTimeProfileInfo::IsAggressiveMulIntTypeSpecDisabled(const bool isJitLoopBody) const
{TRACE_IT(9987);
    if (isJitLoopBody)
    {TRACE_IT(9988);
        return TestFlag(Flags_disableAggressiveMulIntTypeSpec_jitLoopBody);
    }
    else
    {TRACE_IT(9989);
        return TestFlag(Flags_disableAggressiveMulIntTypeSpec);
    }
}

bool
JITTimeProfileInfo::IsDivIntTypeSpecDisabled(const bool isJitLoopBody) const
{TRACE_IT(9990);
    if (isJitLoopBody)
    {TRACE_IT(9991);
        return TestFlag(Flags_disableDivIntTypeSpec_jitLoopBody);
    }
    else
    {TRACE_IT(9992);
        return TestFlag(Flags_disableDivIntTypeSpec);
    }
}

bool
JITTimeProfileInfo::IsLossyIntTypeSpecDisabled() const
{TRACE_IT(9993);
    return TestFlag(Flags_disableLossyIntTypeSpec);
}

bool
JITTimeProfileInfo::IsMemOpDisabled() const
{TRACE_IT(9994);
    return TestFlag(Flags_disableMemOp);
}

bool
JITTimeProfileInfo::IsTrackCompoundedIntOverflowDisabled() const
{TRACE_IT(9995);
    return TestFlag(Flags_disableTrackCompoundedIntOverflow);
}

bool
JITTimeProfileInfo::IsFloatTypeSpecDisabled() const
{TRACE_IT(9996);
    return TestFlag(Flags_disableFloatTypeSpec);
}

bool
JITTimeProfileInfo::IsCheckThisDisabled() const
{TRACE_IT(9997);
    return TestFlag(Flags_disableCheckThis);
}

bool
JITTimeProfileInfo::IsArrayCheckHoistDisabled(const bool isJitLoopBody) const
{TRACE_IT(9998);
    if (isJitLoopBody)
    {TRACE_IT(9999);
        return TestFlag(Flags_disableArrayCheckHoist_jitLoopBody);
    }
    else
    {TRACE_IT(10000);
        return TestFlag(Flags_disableArrayCheckHoist);
    }
}

bool
JITTimeProfileInfo::IsArrayMissingValueCheckHoistDisabled(const bool isJitLoopBody) const
{TRACE_IT(10001);
    if (isJitLoopBody)
    {TRACE_IT(10002);
        return TestFlag(Flags_disableArrayMissingValueCheckHoist_jitLoopBody);
    }
    else
    {TRACE_IT(10003);
        return TestFlag(Flags_disableArrayMissingValueCheckHoist);
    }
}

bool
JITTimeProfileInfo::IsJsArraySegmentHoistDisabled(const bool isJitLoopBody) const
{TRACE_IT(10004);
    if (isJitLoopBody)
    {TRACE_IT(10005);
        return TestFlag(Flags_disableJsArraySegmentHoist_jitLoopBody);
    }
    else
    {TRACE_IT(10006);
        return TestFlag(Flags_disableJsArraySegmentHoist);
    }
}

bool
JITTimeProfileInfo::IsArrayLengthHoistDisabled(const bool isJitLoopBody) const
{TRACE_IT(10007);
    if (isJitLoopBody)
    {TRACE_IT(10008);
        return TestFlag(Flags_disableArrayLengthHoist_jitLoopBody);
    }
    else
    {TRACE_IT(10009);
        return TestFlag(Flags_disableArrayLengthHoist);
    }
}

bool
JITTimeProfileInfo::IsTypedArrayTypeSpecDisabled(const bool isJitLoopBody) const
{TRACE_IT(10010);
    if (isJitLoopBody)
    {TRACE_IT(10011);
        return TestFlag(Flags_disableTypedArrayTypeSpec_jitLoopBody);
    }
    else
    {TRACE_IT(10012);
        return TestFlag(Flags_disableTypedArrayTypeSpec);
    }
}

bool
JITTimeProfileInfo::IsLdLenIntSpecDisabled() const
{TRACE_IT(10013);
    return TestFlag(Flags_disableLdLenIntSpec);
}

bool
JITTimeProfileInfo::IsBoundCheckHoistDisabled(const bool isJitLoopBody) const
{TRACE_IT(10014);
    if (isJitLoopBody)
    {TRACE_IT(10015);
        return TestFlag(Flags_disableBoundCheckHoist_jitLoopBody);
    }
    else
    {TRACE_IT(10016);
        return TestFlag(Flags_disableBoundCheckHoist);
    }
}

bool
JITTimeProfileInfo::IsLoopCountBasedBoundCheckHoistDisabled(const bool isJitLoopBody) const
{TRACE_IT(10017);
    if (isJitLoopBody)
    {TRACE_IT(10018);
        return TestFlag(Flags_disableLoopCountBasedBoundCheckHoist_jitLoopBody);
    }
    else
    {TRACE_IT(10019);
        return TestFlag(Flags_disableLoopCountBasedBoundCheckHoist);
    }
}

bool
JITTimeProfileInfo::IsStackArgOptDisabled() const
{TRACE_IT(10020);
    return TestFlag(Flags_disableStackArgOpt);
}

bool
JITTimeProfileInfo::IsLoopImplicitCallInfoDisabled() const
{TRACE_IT(10021);
    return TestFlag(Flags_disableLoopImplicitCallInfo);
}

bool
JITTimeProfileInfo::IsPowIntIntTypeSpecDisabled() const
{TRACE_IT(10022);
    return TestFlag(Flags_disablePowIntIntTypeSpec);
}

bool
JITTimeProfileInfo::IsFloorInliningDisabled() const
{TRACE_IT(10023);
    return TestFlag(Flags_disableFloorInlining);
}

bool
JITTimeProfileInfo::IsNoProfileBailoutsDisabled() const
{TRACE_IT(10024);
    return TestFlag(Flags_disableNoProfileBailouts);
}

bool
JITTimeProfileInfo::IsTagCheckDisabled() const
{TRACE_IT(10025);
    return TestFlag(Flags_disableTagCheck);
}

bool
JITTimeProfileInfo::HasLdFldCallSiteInfo() const
{TRACE_IT(10026);
    return TestFlag(Flags_hasLdFldCallSiteInfo);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledArrayCallSiteCount() const
{TRACE_IT(10027);
    return static_cast<Js::ProfileId>(m_profileData.profiledArrayCallSiteCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledCallSiteCount() const
{TRACE_IT(10028);
    return static_cast<Js::ProfileId>(m_profileData.profiledCallSiteCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledReturnTypeCount() const
{TRACE_IT(10029);
    return static_cast<Js::ProfileId>(m_profileData.profiledReturnTypeCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledDivOrRemCount() const
{TRACE_IT(10030);
    return static_cast<Js::ProfileId>(m_profileData.profiledDivOrRemCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledSwitchCount() const
{TRACE_IT(10031);
    return static_cast<Js::ProfileId>(m_profileData.profiledSwitchCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledSlotCount() const
{TRACE_IT(10032);
    return static_cast<Js::ProfileId>(m_profileData.profiledSlotCount);
}

Js::ArgSlot
JITTimeProfileInfo::GetProfiledInParamsCount() const
{TRACE_IT(10033);
    return static_cast<Js::ArgSlot>(m_profileData.profiledInParamsCount);
}

uint
JITTimeProfileInfo::GetProfiledFldCount() const
{TRACE_IT(10034);
    return m_profileData.inlineCacheCount;
}

uint
JITTimeProfileInfo::GetLoopCount() const
{TRACE_IT(10035);
    return m_profileData.loopCount;
}

Js::CallSiteInfo *
JITTimeProfileInfo::GetCallSiteInfo() const
{TRACE_IT(10036);
    return reinterpret_cast<Js::CallSiteInfo*>(m_profileData.callSiteData);
}

bool
JITTimeProfileInfo::TestFlag(ProfileDataFlags flag) const
{TRACE_IT(10037);
    return (m_profileData.flags & flag) != 0;
}

BVFixed *
JITTimeProfileInfo::GetLoopFlags() const
{TRACE_IT(10038);
    return (BVFixed*)m_profileData.loopFlags;
}
