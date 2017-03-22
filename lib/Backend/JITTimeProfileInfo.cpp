//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "Backend.h"

JITTimeProfileInfo::JITTimeProfileInfo(ProfileDataIDL * profileData) :
    m_profileData(*profileData)
{LOGMEIN("JITTimeProfileInfo.cpp] 9\n");
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
{LOGMEIN("JITTimeProfileInfo.cpp] 21\n");
    if (profileInfo == nullptr)
    {LOGMEIN("JITTimeProfileInfo.cpp] 23\n");
        return;
    }

    CompileAssert(sizeof(LdElemIDL) == sizeof(Js::LdElemInfo));
    CompileAssert(sizeof(StElemIDL) == sizeof(Js::StElemInfo));

    data->profiledLdElemCount = functionBody->GetProfiledLdElemCount();
    data->profiledStElemCount = functionBody->GetProfiledStElemCount();

    if (JITManager::GetJITManager()->IsOOPJITEnabled() || isForegroundJIT)
    {LOGMEIN("JITTimeProfileInfo.cpp] 34\n");
        data->ldElemData = (LdElemIDL*)profileInfo->GetLdElemInfo();
        data->stElemData = (StElemIDL*)profileInfo->GetStElemInfo();
    }
    else
    {
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
{LOGMEIN("JITTimeProfileInfo.cpp] 141\n");
    m_profileData.flags |= isLoopBody ? Flags_disableAggressiveIntTypeSpec_jitLoopBody : Flags_disableAggressiveIntTypeSpec;
}

void
JITTimeProfileInfo::DisableStackArgOpt()
{LOGMEIN("JITTimeProfileInfo.cpp] 147\n");
    m_profileData.flags |= Flags_disableStackArgOpt;
}

void
JITTimeProfileInfo::DisableSwitchOpt()
{LOGMEIN("JITTimeProfileInfo.cpp] 153\n");
    m_profileData.flags |= Flags_disableSwitchOpt;
}

void
JITTimeProfileInfo::DisableTrackCompoundedIntOverflow()
{LOGMEIN("JITTimeProfileInfo.cpp] 159\n");
    m_profileData.flags |= Flags_disableTrackCompoundedIntOverflow;
}

const Js::LdElemInfo *
JITTimeProfileInfo::GetLdElemInfo(Js::ProfileId ldElemId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 165\n");
    return &(reinterpret_cast<Js::LdElemInfo*>(m_profileData.ldElemData)[ldElemId]);
}

const Js::StElemInfo *
JITTimeProfileInfo::GetStElemInfo(Js::ProfileId stElemId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 171\n");
    return &(reinterpret_cast<Js::StElemInfo*>(m_profileData.stElemData)[stElemId]);
}

Js::ArrayCallSiteInfo *
JITTimeProfileInfo::GetArrayCallSiteInfo(Js::ProfileId index) const
{LOGMEIN("JITTimeProfileInfo.cpp] 177\n");
    Assert(index < GetProfiledArrayCallSiteCount());
    return &(reinterpret_cast<Js::ArrayCallSiteInfo*>(m_profileData.arrayCallSiteData)[index]);
}

intptr_t
JITTimeProfileInfo::GetArrayCallSiteInfoAddr(Js::ProfileId index) const
{LOGMEIN("JITTimeProfileInfo.cpp] 184\n");
    Assert(index < GetProfiledArrayCallSiteCount());
    return m_profileData.arrayCallSiteDataAddr + index * sizeof(ArrayCallSiteIDL);
}

Js::FldInfo *
JITTimeProfileInfo::GetFldInfo(uint fieldAccessId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 191\n");
    Assert(fieldAccessId < GetProfiledFldCount());
    return &(reinterpret_cast<Js::FldInfo*>(m_profileData.fldData)[fieldAccessId]);
}

intptr_t
JITTimeProfileInfo::GetFldInfoAddr(uint fieldAccessId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 198\n");
    Assert(fieldAccessId < GetProfiledFldCount());
    return m_profileData.fldDataAddr + fieldAccessId;
}

ValueType
JITTimeProfileInfo::GetSlotLoad(Js::ProfileId slotLoadId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 205\n");
    Assert(slotLoadId < GetProfiledSlotCount());
    return reinterpret_cast<ValueType*>(m_profileData.slotData)[slotLoadId];
}

Js::ThisInfo
JITTimeProfileInfo::GetThisInfo() const
{LOGMEIN("JITTimeProfileInfo.cpp] 212\n");
    return *reinterpret_cast<const Js::ThisInfo*>(&m_profileData.thisData);
}

ValueType
JITTimeProfileInfo::GetReturnType(Js::OpCode opcode, Js::ProfileId callSiteId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 218\n");
    if (opcode < Js::OpCode::ProfiledReturnTypeCallI || (opcode > Js::OpCode::ProfiledReturnTypeCallIFlags && opcode < Js::OpCode::ProfiledReturnTypeCallIExtended) || opcode > Js::OpCode::ProfiledReturnTypeCallIExtendedFlags)
    {LOGMEIN("JITTimeProfileInfo.cpp] 220\n");
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
{LOGMEIN("JITTimeProfileInfo.cpp] 232\n");
    Assert(divideId < GetProfiledDivOrRemCount());
    return reinterpret_cast<ValueType*>(m_profileData.divideTypeInfo)[divideId];
}

ValueType
JITTimeProfileInfo::GetSwitchProfileInfo(Js::ProfileId switchId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 239\n");
    Assert(switchId < GetProfiledSwitchCount());
    return reinterpret_cast<ValueType*>(m_profileData.switchTypeInfo)[switchId];
}

ValueType
JITTimeProfileInfo::GetParameterInfo(Js::ArgSlot index) const
{LOGMEIN("JITTimeProfileInfo.cpp] 246\n");
    Assert(index < GetProfiledInParamsCount());
    return reinterpret_cast<ValueType*>(m_profileData.parameterInfo)[index];
}

Js::ImplicitCallFlags
JITTimeProfileInfo::GetLoopImplicitCallFlags(uint loopNum) const
{LOGMEIN("JITTimeProfileInfo.cpp] 253\n");
    // TODO: michhol OOP JIT, investigate vaibility of reenabling this assert
    // Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(functionBody));
    Assert(loopNum < GetLoopCount());

    // Mask out the dispose implicit call. We would bailout on reentrant dispose,
    // but it shouldn't affect optimization.
    return (Js::ImplicitCallFlags)(m_profileData.loopImplicitCallFlags[loopNum] & Js::ImplicitCall_All);
}

Js::ImplicitCallFlags
JITTimeProfileInfo::GetImplicitCallFlags() const
{LOGMEIN("JITTimeProfileInfo.cpp] 265\n");
    return static_cast<Js::ImplicitCallFlags>(m_profileData.implicitCallFlags);
}

Js::LoopFlags
JITTimeProfileInfo::GetLoopFlags(uint loopNum) const
{LOGMEIN("JITTimeProfileInfo.cpp] 271\n");
    Assert(GetLoopFlags() != nullptr);
    return GetLoopFlags()->GetRange<Js::LoopFlags>(loopNum * Js::LoopFlags::COUNT, Js::LoopFlags::COUNT);
}

uint16
JITTimeProfileInfo::GetConstantArgInfo(Js::ProfileId callSiteId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 278\n");
    return GetCallSiteInfo()[callSiteId].isArgConstant;
}

bool
JITTimeProfileInfo::IsModulusOpByPowerOf2(Js::ProfileId profileId) const
{LOGMEIN("JITTimeProfileInfo.cpp] 284\n");
    return GetDivProfileInfo(profileId).IsLikelyTaggedInt();
}

bool
JITTimeProfileInfo::IsAggressiveIntTypeSpecDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 290\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 292\n");
        return TestFlag(Flags_disableAggressiveIntTypeSpec_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableAggressiveIntTypeSpec);
    }
}

bool
JITTimeProfileInfo::IsSwitchOptDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 303\n");
    return TestFlag(Flags_disableSwitchOpt);
}

bool
JITTimeProfileInfo::IsEquivalentObjTypeSpecDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 309\n");
    return TestFlag(Flags_disableEquivalentObjTypeSpec);
}

bool
JITTimeProfileInfo::IsObjTypeSpecDisabledInJitLoopBody() const
{LOGMEIN("JITTimeProfileInfo.cpp] 315\n");
    return TestFlag(Flags_disableObjTypeSpec_jitLoopBody);
}

bool
JITTimeProfileInfo::IsAggressiveMulIntTypeSpecDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 321\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 323\n");
        return TestFlag(Flags_disableAggressiveMulIntTypeSpec_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableAggressiveMulIntTypeSpec);
    }
}

bool
JITTimeProfileInfo::IsDivIntTypeSpecDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 334\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 336\n");
        return TestFlag(Flags_disableDivIntTypeSpec_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableDivIntTypeSpec);
    }
}

bool
JITTimeProfileInfo::IsLossyIntTypeSpecDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 347\n");
    return TestFlag(Flags_disableLossyIntTypeSpec);
}

bool
JITTimeProfileInfo::IsMemOpDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 353\n");
    return TestFlag(Flags_disableMemOp);
}

bool
JITTimeProfileInfo::IsTrackCompoundedIntOverflowDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 359\n");
    return TestFlag(Flags_disableTrackCompoundedIntOverflow);
}

bool
JITTimeProfileInfo::IsFloatTypeSpecDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 365\n");
    return TestFlag(Flags_disableFloatTypeSpec);
}

bool
JITTimeProfileInfo::IsCheckThisDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 371\n");
    return TestFlag(Flags_disableCheckThis);
}

bool
JITTimeProfileInfo::IsArrayCheckHoistDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 377\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 379\n");
        return TestFlag(Flags_disableArrayCheckHoist_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableArrayCheckHoist);
    }
}

bool
JITTimeProfileInfo::IsArrayMissingValueCheckHoistDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 390\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 392\n");
        return TestFlag(Flags_disableArrayMissingValueCheckHoist_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableArrayMissingValueCheckHoist);
    }
}

bool
JITTimeProfileInfo::IsJsArraySegmentHoistDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 403\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 405\n");
        return TestFlag(Flags_disableJsArraySegmentHoist_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableJsArraySegmentHoist);
    }
}

bool
JITTimeProfileInfo::IsArrayLengthHoistDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 416\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 418\n");
        return TestFlag(Flags_disableArrayLengthHoist_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableArrayLengthHoist);
    }
}

bool
JITTimeProfileInfo::IsTypedArrayTypeSpecDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 429\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 431\n");
        return TestFlag(Flags_disableTypedArrayTypeSpec_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableTypedArrayTypeSpec);
    }
}

bool
JITTimeProfileInfo::IsLdLenIntSpecDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 442\n");
    return TestFlag(Flags_disableLdLenIntSpec);
}

bool
JITTimeProfileInfo::IsBoundCheckHoistDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 448\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 450\n");
        return TestFlag(Flags_disableBoundCheckHoist_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableBoundCheckHoist);
    }
}

bool
JITTimeProfileInfo::IsLoopCountBasedBoundCheckHoistDisabled(const bool isJitLoopBody) const
{LOGMEIN("JITTimeProfileInfo.cpp] 461\n");
    if (isJitLoopBody)
    {LOGMEIN("JITTimeProfileInfo.cpp] 463\n");
        return TestFlag(Flags_disableLoopCountBasedBoundCheckHoist_jitLoopBody);
    }
    else
    {
        return TestFlag(Flags_disableLoopCountBasedBoundCheckHoist);
    }
}

bool
JITTimeProfileInfo::IsStackArgOptDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 474\n");
    return TestFlag(Flags_disableStackArgOpt);
}

bool
JITTimeProfileInfo::IsLoopImplicitCallInfoDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 480\n");
    return TestFlag(Flags_disableLoopImplicitCallInfo);
}

bool
JITTimeProfileInfo::IsPowIntIntTypeSpecDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 486\n");
    return TestFlag(Flags_disablePowIntIntTypeSpec);
}

bool
JITTimeProfileInfo::IsFloorInliningDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 492\n");
    return TestFlag(Flags_disableFloorInlining);
}

bool
JITTimeProfileInfo::IsNoProfileBailoutsDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 498\n");
    return TestFlag(Flags_disableNoProfileBailouts);
}

bool
JITTimeProfileInfo::IsTagCheckDisabled() const
{LOGMEIN("JITTimeProfileInfo.cpp] 504\n");
    return TestFlag(Flags_disableTagCheck);
}

bool
JITTimeProfileInfo::HasLdFldCallSiteInfo() const
{LOGMEIN("JITTimeProfileInfo.cpp] 510\n");
    return TestFlag(Flags_hasLdFldCallSiteInfo);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledArrayCallSiteCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 516\n");
    return static_cast<Js::ProfileId>(m_profileData.profiledArrayCallSiteCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledCallSiteCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 522\n");
    return static_cast<Js::ProfileId>(m_profileData.profiledCallSiteCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledReturnTypeCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 528\n");
    return static_cast<Js::ProfileId>(m_profileData.profiledReturnTypeCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledDivOrRemCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 534\n");
    return static_cast<Js::ProfileId>(m_profileData.profiledDivOrRemCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledSwitchCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 540\n");
    return static_cast<Js::ProfileId>(m_profileData.profiledSwitchCount);
}

Js::ProfileId
JITTimeProfileInfo::GetProfiledSlotCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 546\n");
    return static_cast<Js::ProfileId>(m_profileData.profiledSlotCount);
}

Js::ArgSlot
JITTimeProfileInfo::GetProfiledInParamsCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 552\n");
    return static_cast<Js::ArgSlot>(m_profileData.profiledInParamsCount);
}

uint
JITTimeProfileInfo::GetProfiledFldCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 558\n");
    return m_profileData.inlineCacheCount;
}

uint
JITTimeProfileInfo::GetLoopCount() const
{LOGMEIN("JITTimeProfileInfo.cpp] 564\n");
    return m_profileData.loopCount;
}

Js::CallSiteInfo *
JITTimeProfileInfo::GetCallSiteInfo() const
{LOGMEIN("JITTimeProfileInfo.cpp] 570\n");
    return reinterpret_cast<Js::CallSiteInfo*>(m_profileData.callSiteData);
}

bool
JITTimeProfileInfo::TestFlag(ProfileDataFlags flag) const
{LOGMEIN("JITTimeProfileInfo.cpp] 576\n");
    return (m_profileData.flags & flag) != 0;
}

BVFixed *
JITTimeProfileInfo::GetLoopFlags() const
{LOGMEIN("JITTimeProfileInfo.cpp] 582\n");
    return (BVFixed*)m_profileData.loopFlags;
}
