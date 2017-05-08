//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

#if ENABLE_NATIVE_CODEGEN
namespace Js
{
#ifdef DYNAMIC_PROFILE_STORAGE
    DynamicProfileInfo::DynamicProfileInfo()
    {TRACE_IT(47478);
        hasFunctionBody = false;
    }
#endif

    struct Allocation
    {
        uint offset;
        size_t size;
    };

#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
    bool DynamicProfileInfo::NeedProfileInfoList()
    {TRACE_IT(47479);
#pragma prefast(suppress: 6235 6286, "(<non-zero constant> || <expression>) is always a non-zero constant. - This is wrong, DBG_DUMP is not set in some build variants")
        return DBG_DUMP
#ifdef DYNAMIC_PROFILE_STORAGE
            || DynamicProfileStorage::IsEnabled()
#endif
#ifdef RUNTIME_DATA_COLLECTION
            || (Configuration::Global.flags.RuntimeDataOutputFile != nullptr)
#endif
            ;
    }
#endif

    void ArrayCallSiteInfo::SetIsNotNativeIntArray()
    {TRACE_IT(47480);
        OUTPUT_TRACE_WITH_STACK(Js::NativeArrayConversionPhase, _u("SetIsNotNativeIntArray \n"));
        bits |= NotNativeIntBit;
    }

    void ArrayCallSiteInfo::SetIsNotNativeFloatArray()
    {TRACE_IT(47481);
        OUTPUT_TRACE_WITH_STACK(Js::NativeArrayConversionPhase, _u("SetIsNotNativeFloatArray \n"));
        bits |= NotNativeFloatBit;
    }

    void ArrayCallSiteInfo::SetIsNotNativeArray()
    {TRACE_IT(47482);
        OUTPUT_TRACE_WITH_STACK(Js::NativeArrayConversionPhase, _u("SetIsNotNativeArray \n"));
        bits = NotNativeIntBit | NotNativeFloatBit;
    }

    DynamicProfileInfo* DynamicProfileInfo::New(Recycler* recycler, FunctionBody* functionBody, bool persistsAcrossScriptContexts)
    {TRACE_IT(47483);
        size_t totalAlloc = 0;
        Allocation batch[] =
        {
            { (uint)offsetof(DynamicProfileInfo, callSiteInfo), functionBody->GetProfiledCallSiteCount() * sizeof(CallSiteInfo) },
            { (uint)offsetof(DynamicProfileInfo, ldElemInfo), functionBody->GetProfiledLdElemCount() * sizeof(LdElemInfo) },
            { (uint)offsetof(DynamicProfileInfo, stElemInfo), functionBody->GetProfiledStElemCount() * sizeof(StElemInfo) },
            { (uint)offsetof(DynamicProfileInfo, arrayCallSiteInfo), functionBody->GetProfiledArrayCallSiteCount() * sizeof(ArrayCallSiteInfo) },
            { (uint)offsetof(DynamicProfileInfo, fldInfo), functionBody->GetProfiledFldCount() * sizeof(FldInfo) },
            { (uint)offsetof(DynamicProfileInfo, divideTypeInfo), functionBody->GetProfiledDivOrRemCount() * sizeof(ValueType) },
            { (uint)offsetof(DynamicProfileInfo, switchTypeInfo), functionBody->GetProfiledSwitchCount() * sizeof(ValueType)},
            { (uint)offsetof(DynamicProfileInfo, slotInfo), functionBody->GetProfiledSlotCount() * sizeof(ValueType) },
            { (uint)offsetof(DynamicProfileInfo, parameterInfo), functionBody->GetProfiledInParamsCount() * sizeof(ValueType) },
            { (uint)offsetof(DynamicProfileInfo, returnTypeInfo), functionBody->GetProfiledReturnTypeCount() * sizeof(ValueType) },
            { (uint)offsetof(DynamicProfileInfo, loopImplicitCallFlags), (EnableImplicitCallFlags(functionBody) ? (functionBody->GetLoopCount() * sizeof(ImplicitCallFlags)) : 0) },
            { (uint)offsetof(DynamicProfileInfo, loopFlags), functionBody->GetLoopCount() ? BVFixed::GetAllocSize(functionBody->GetLoopCount() * LoopFlags::COUNT) : 0 }
        };

        for (uint i = 0; i < _countof(batch); i++)
        {TRACE_IT(47484);
            totalAlloc += batch[i].size;
        }

        DynamicProfileInfo* info = nullptr;

        // In the profile storage case (-only), always allocate a non-leaf profile
        // In the regular profile case, we need to allocate it as non-leaf only if it's
        // a profile being used in the in-memory cache. This is because in that case, the profile
        // also allocates dynamicProfileFunctionInfo, which it uses to match functions across
        // script contexts. In the normal case, since we don't allocate that structure, we
        // can be a leaf allocation.
        if (persistsAcrossScriptContexts)
        {TRACE_IT(47485);
            info = RecyclerNewPlusZ(recycler, totalAlloc, DynamicProfileInfo, functionBody);
#if DBG
            info->persistsAcrossScriptContexts = true;
#endif
        }
        else
        {TRACE_IT(47486);
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
            if (DynamicProfileInfo::NeedProfileInfoList())
            {TRACE_IT(47487);
                info = RecyclerNewPlusZ(recycler, totalAlloc, DynamicProfileInfo, functionBody);
            }
            else
#endif
            {TRACE_IT(47488);
                info = RecyclerNewPlusLeafZ(recycler, totalAlloc, DynamicProfileInfo, functionBody);
            }
        }
        BYTE* current = (BYTE*)info + sizeof(DynamicProfileInfo);

        for (uint i = 0; i < _countof(batch); i++)
        {TRACE_IT(47489);
            if (batch[i].size > 0)
            {TRACE_IT(47490);
                Field(BYTE*)* field = (Field(BYTE*)*)(((BYTE*)info + batch[i].offset));
                *field = current;
                current += batch[i].size;
            }
        }
        Assert(current - reinterpret_cast<BYTE*>(info) - sizeof(DynamicProfileInfo) == totalAlloc);

        info->Initialize(functionBody);
        return info;
    }

    DynamicProfileInfo::DynamicProfileInfo(FunctionBody * functionBody)
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        : functionBody(DynamicProfileInfo::NeedProfileInfoList() ? functionBody : nullptr)
#endif
    {
        hasFunctionBody = true;
#if DBG
        persistsAcrossScriptContexts = true;
#endif
    }

    void DynamicProfileInfo::Initialize(FunctionBody *const functionBody)
    {TRACE_IT(47491);
        // Need to make value types uninitialized, which is not equivalent to zero
        thisInfo.valueType = ValueType::Uninitialized;
        const BVIndex loopFlagsCount = functionBody->GetLoopCount() * LoopFlags::COUNT;
        if (loopFlagsCount)
        {TRACE_IT(47492);
            this->loopFlags->Init(loopFlagsCount);
            LoopFlags defaultValues;
            for (uint i = 0; i < functionBody->GetLoopCount(); ++i)
            {TRACE_IT(47493);
                this->loopFlags->SetRange(&defaultValues, i * LoopFlags::COUNT, LoopFlags::COUNT);
            }
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledCallSiteCount(); ++i)
        {TRACE_IT(47494);
            callSiteInfo[i].returnType = ValueType::Uninitialized;
            callSiteInfo[i].u.functionData.sourceId = NoSourceId;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledLdElemCount(); ++i)
        {TRACE_IT(47495);
            ldElemInfo[i].arrayType = ValueType::Uninitialized;
            ldElemInfo[i].elemType = ValueType::Uninitialized;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledStElemCount(); ++i)
        {TRACE_IT(47496);
            stElemInfo[i].arrayType = ValueType::Uninitialized;
        }
        for (uint i = 0; i < functionBody->GetProfiledFldCount(); ++i)
        {TRACE_IT(47497);
            fldInfo[i].flags = FldInfo_NoInfo;
            fldInfo[i].valueType = ValueType::Uninitialized;
            fldInfo[i].polymorphicInlineCacheUtilization = PolymorphicInlineCacheUtilizationThreshold;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledDivOrRemCount(); ++i)
        {TRACE_IT(47498);
            divideTypeInfo[i] = ValueType::Uninitialized;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledSwitchCount(); ++i)
        {TRACE_IT(47499);
            switchTypeInfo[i] = ValueType::Uninitialized;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledSlotCount(); ++i)
        {TRACE_IT(47500);
            slotInfo[i] = ValueType::Uninitialized;
        }
        for (ArgSlot i = 0; i < functionBody->GetProfiledInParamsCount(); ++i)
        {TRACE_IT(47501);
            parameterInfo[i] = ValueType::Uninitialized;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledReturnTypeCount(); ++i)
        {TRACE_IT(47502);
            returnTypeInfo[i] = ValueType::Uninitialized;
        }

        this->rejitCount = 0;
        this->bailOutOffsetForLastRejit = Js::Constants::NoByteCodeOffset;
#if DBG
        for (ProfileId i = 0; i < functionBody->GetProfiledArrayCallSiteCount(); ++i)
        {TRACE_IT(47503);
            arrayCallSiteInfo[i].functionNumber = functionBody->GetFunctionNumber();
            arrayCallSiteInfo[i].callSiteNumber = i;
        }
#endif

#if TTD_NATIVE_PROFILE_ARRAY_WORK_AROUND
        if(functionBody->GetScriptContext()->GetThreadContext()->IsRuntimeInTTDMode())
        {TRACE_IT(47504);
            for(ProfileId i = 0; i < functionBody->GetProfiledArrayCallSiteCount(); ++i)
            {TRACE_IT(47505);
                arrayCallSiteInfo[i].SetIsNotNativeArray();
            }
        }
#endif
    }

    bool DynamicProfileInfo::IsEnabledForAtLeastOneFunction(const ScriptContext *const scriptContext)
    {TRACE_IT(47506);
        return IsEnabled_OptionalFunctionBody(nullptr, scriptContext);
    }

    bool DynamicProfileInfo::IsEnabled(const FunctionBody *const functionBody)
    {TRACE_IT(47507);
        Assert(functionBody);
        return IsEnabled_OptionalFunctionBody(functionBody, functionBody->GetScriptContext());
    }

    bool DynamicProfileInfo::IsEnabled_OptionalFunctionBody(const FunctionBody *const functionBody, const ScriptContext *const scriptContext)
    {TRACE_IT(47508);
        Assert(scriptContext);

        return
            !PHASE_OFF_OPTFUNC(DynamicProfilePhase, functionBody) &&
            (
#if ENABLE_DEBUG_CONFIG_OPTIONS
                PHASE_FORCE_OPTFUNC(DynamicProfilePhase, functionBody) ||
#else
                Js::Configuration::Global.flags.ForceDynamicProfile ||
#endif
                !scriptContext->GetConfig()->IsNoNative() ||
                (functionBody && functionBody->IsInDebugMode())
#ifdef DYNAMIC_PROFILE_STORAGE
                || DynamicProfileStorage::DoCollectInfo()
#endif
                );
    }

    bool DynamicProfileInfo::IsEnabledForAtLeastOneFunction(const Js::Phase phase, const ScriptContext *const scriptContext)
    {TRACE_IT(47509);
        return IsEnabled_OptionalFunctionBody(phase, nullptr, scriptContext);
    }

    bool DynamicProfileInfo::IsEnabled(const Js::Phase phase, const FunctionBody *const functionBody)
    {TRACE_IT(47510);
        Assert(functionBody);
        return IsEnabled_OptionalFunctionBody(phase, functionBody, functionBody->GetScriptContext());
    }

    bool DynamicProfileInfo::IsEnabled_OptionalFunctionBody(
        const Js::Phase phase,
        const FunctionBody *const functionBody,
        const ScriptContext *const scriptContext)
    {TRACE_IT(47511);
        if (!DynamicProfileInfo::IsEnabled_OptionalFunctionBody(functionBody, scriptContext))
        {TRACE_IT(47512);
            return false;
        }

        switch (phase)
        {
        case Phase::TypedArrayPhase:
        case Phase::AggressiveIntTypeSpecPhase:
        case Phase::CheckThisPhase:
        case Phase::ProfileBasedFldFastPathPhase:
        case Phase::ObjTypeSpecPhase:
        case Phase::ArrayCheckHoistPhase:
        case Phase::SwitchOptPhase:
        case Phase::FixedNewObjPhase:
            return !PHASE_OFF_PROFILED_BYTE_CODE_OPTFUNC(phase, functionBody);

        case Phase::NativeArrayPhase:
        case Phase::FloatTypeSpecPhase:
            return !PHASE_OFF_PROFILED_BYTE_CODE_OPTFUNC(phase, functionBody)
#ifdef _M_IX86
                && AutoSystemInfo::Data.SSE2Available()
#endif
                ;

        case Phase::InlinePhase:
            return !PHASE_OFF_PROFILED_BYTE_CODE_OPTFUNC(Phase::InlinePhase, functionBody);
        }
        return false;
    }

    bool DynamicProfileInfo::EnableImplicitCallFlags(const FunctionBody *const functionBody)
    {TRACE_IT(47513);
        return DynamicProfileInfo::IsEnabled(functionBody);
    }

#ifdef _M_IX86
    __declspec(naked)
        Var
        DynamicProfileInfo::EnsureDynamicProfileInfoThunk(RecyclableObject* function, CallInfo callInfo, ...)
    {
        __asm
        {
            push ebp
            mov ebp, esp
                push[esp + 8]     // push function object
                call DynamicProfileInfo::EnsureDynamicProfileInfo;
#ifdef _CONTROL_FLOW_GUARD
            // verify that the call target is valid
            mov  ecx, eax
                call[__guard_check_icall_fptr]
                mov eax, ecx
#endif
                pop ebp
                jmp eax
        }
    }
#endif

    JavascriptMethod DynamicProfileInfo::EnsureDynamicProfileInfo(ScriptFunction * function)
    {TRACE_IT(47514);
        // If we're creating a dynamic profile, make sure that the function
        // has an entry point and this entry point is the "default" entrypoint
        // created when a function body is created.
        Assert(function->GetEntryPointInfo() != nullptr);
        Assert(function->GetFunctionEntryPointInfo()->entryPointIndex == 0);
        FunctionBody * functionBody = function->GetFunctionBody();

        // This is used only if the first entry point codegen completes.
        // So there is no concurrency concern with background code gen thread modifying the entry point.
        EntryPointInfo * entryPoint = functionBody->GetEntryPointInfo(0);
        Assert(entryPoint == function->GetEntryPointInfo());
        Assert(entryPoint->IsCodeGenDone());

        JavascriptMethod directEntryPoint = entryPoint->jsMethod;

        // Check if it has changed already
        if (directEntryPoint == DynamicProfileInfo::EnsureDynamicProfileInfoThunk)
        {TRACE_IT(47515);
            functionBody->EnsureDynamicProfileInfo();
            if (functionBody->GetScriptContext()->CurrentThunk == ProfileEntryThunk)
            {TRACE_IT(47516);
                directEntryPoint = ProfileEntryThunk;
            }
            else
            {TRACE_IT(47517);
                directEntryPoint = (JavascriptMethod)entryPoint->GetNativeAddress();
            }

            entryPoint->jsMethod = directEntryPoint;
        }
        else
        {TRACE_IT(47518);
            Assert(directEntryPoint == ProfileEntryThunk || functionBody->GetScriptContext()->IsNativeAddress((void*)directEntryPoint));
            Assert(functionBody->HasExecutionDynamicProfileInfo());
        }

        return function->UpdateThunkEntryPoint(static_cast<FunctionEntryPointInfo*>(entryPoint), directEntryPoint);
    }

    bool DynamicProfileInfo::HasLdFldCallSiteInfo() const
    {TRACE_IT(47519);
        return bits.hasLdFldCallSite;
    }

    bool DynamicProfileInfo::RecordLdFldCallSiteInfo(FunctionBody* functionBody, RecyclableObject* callee, bool callApplyTarget)
    {TRACE_IT(47520);
        auto SetBits = [&]() -> bool
        {
            this->bits.hasLdFldCallSite = true;
            this->currentInlinerVersion++; // we don't mind if this overflows
            return true;
        };

        FunctionInfo* calleeFunctionInfo = callee->GetTypeId() == TypeIds_Function ? JavascriptFunction::FromVar(callee)->GetFunctionInfo() : nullptr;
        if (calleeFunctionInfo == nullptr)
        {TRACE_IT(47521);
            return false;
        }
        else if (!calleeFunctionInfo->HasBody())
        {TRACE_IT(47522);
            // We can inline fastDOM getter/setter.
            // We can directly call Math.max/min as apply targets.
            if ((calleeFunctionInfo->GetAttributes() & Js::FunctionInfo::Attributes::NeedCrossSiteSecurityCheck) ||
                (callApplyTarget && (calleeFunctionInfo->GetAttributes() & Js::FunctionInfo::Attributes::BuiltInInlinableAsLdFldInlinee)))
            {TRACE_IT(47523);
                if (functionBody->GetScriptContext() == callee->GetScriptContext())
                {TRACE_IT(47524);
                    return SetBits();
                }
            }
            return false;
        }
        else if (functionBody->CheckCalleeContextForInlining(calleeFunctionInfo->GetFunctionProxy()))
        {TRACE_IT(47525);
            // If functionInfo !HasBody(), the previous 'else if' branch is executed; otherwise it has a body and therefore it has a proxy
            return SetBits();
        }
        return false;
    }

    void DynamicProfileInfo::RecordConstParameterAtCallSite(ProfileId callSiteId, int argNum)
    {TRACE_IT(47526);
        Assert(argNum < Js::InlineeCallInfo::MaxInlineeArgoutCount);
        Assert(callSiteId < functionBody->GetProfiledCallSiteCount());
        callSiteInfo[callSiteId].isArgConstant = callSiteInfo[callSiteId].isArgConstant | (1 << argNum);
    }

    uint16 DynamicProfileInfo::GetConstantArgInfo(ProfileId callSiteId)
    {TRACE_IT(47527);
        return callSiteInfo[callSiteId].isArgConstant;
    }

    void DynamicProfileInfo::RecordCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, FunctionInfo* calleeFunctionInfo, JavascriptFunction* calleeFunction, ArgSlot actualArgCount, bool isConstructorCall, InlineCacheIndex ldFldInlineCacheId)
    {TRACE_IT(47528);
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        // If we persistsAcrossScriptContext, the dynamic profile info may be referred to by multiple function body from
        // different script context
        Assert(!DynamicProfileInfo::NeedProfileInfoList() || this->persistsAcrossScriptContexts || this->functionBody == functionBody);
#endif
        bool doInline = true;
        // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo
        if (actualArgCount > Js::InlineeCallInfo::MaxInlineeArgoutCount)
        {TRACE_IT(47529);
            doInline = false;
        }

        // Mark the callsite bit where caller and callee is same function
        if (calleeFunctionInfo && functionBody == calleeFunctionInfo->GetFunctionProxy() && callSiteId < 32)
        {TRACE_IT(47530);
            this->m_recursiveInlineInfo = this->m_recursiveInlineInfo | (1 << callSiteId);
        }

        if (!callSiteInfo[callSiteId].isPolymorphic)
        {TRACE_IT(47531);
            Js::SourceId oldSourceId = callSiteInfo[callSiteId].u.functionData.sourceId;
            if (oldSourceId == InvalidSourceId)
            {TRACE_IT(47532);
                return;
            }

            Js::LocalFunctionId oldFunctionId = callSiteInfo[callSiteId].u.functionData.functionId;

            Js::SourceId sourceId = InvalidSourceId;
            Js::LocalFunctionId functionId;
            if (calleeFunctionInfo == nullptr)
            {TRACE_IT(47533);
                functionId = CallSiteNonFunction;
            }
            else if (!calleeFunctionInfo->HasBody())
            {TRACE_IT(47534);
                Assert(calleeFunction); // calleeFunction can only be passed as null if the calleeFunctionInfo was null (which is checked above)
                if (functionBody->GetScriptContext() == calleeFunction->GetScriptContext())
                {TRACE_IT(47535);
                    sourceId = BuiltInSourceId;
                    functionId = calleeFunctionInfo->GetLocalFunctionId();
                }
                else
                {TRACE_IT(47536);
                    functionId = CallSiteCrossContext;
                }
            }
            else
            {TRACE_IT(47537);
                // We can only inline function that are from the same script context. So only record that data
                // We're about to call this function so deserialize it right now
                FunctionProxy* calleeFunctionProxy = calleeFunctionInfo->GetFunctionProxy();
                if (functionBody->GetScriptContext() == calleeFunctionProxy->GetScriptContext())
                {TRACE_IT(47538);
                    if (functionBody->GetSecondaryHostSourceContext() == calleeFunctionProxy->GetSecondaryHostSourceContext())
                    {TRACE_IT(47539);
                        if (functionBody->GetHostSourceContext() == calleeFunctionProxy->GetHostSourceContext())
                        {TRACE_IT(47540);
                            sourceId = CurrentSourceId; // Caller and callee in same file
                        }
                        else
                        {TRACE_IT(47541);
                            sourceId = (Js::SourceId)calleeFunctionProxy->GetHostSourceContext(); // Caller and callee in different files
                        }
                        functionId = calleeFunctionProxy->GetLocalFunctionId();
                    }
                    else
                    {TRACE_IT(47542);
                        // Pretend that we are cross context when call is crossing script file.
                        functionId = CallSiteCrossContext;
                    }
                }
                else
                {TRACE_IT(47543);
                    functionId = CallSiteCrossContext;
                }
            }

            if (oldSourceId == NoSourceId)
            {TRACE_IT(47544);
                callSiteInfo[callSiteId].u.functionData.sourceId = sourceId;
                callSiteInfo[callSiteId].u.functionData.functionId = functionId;
                this->currentInlinerVersion++; // we don't mind if this overflows
            }
            else if (oldSourceId != sourceId || oldFunctionId != functionId)
            {TRACE_IT(47545);
                if (oldFunctionId != CallSiteMixed)
                {TRACE_IT(47546);
                    this->currentInlinerVersion++; // we don't mind if this overflows
                }

                if (doInline && IsPolymorphicCallSite(functionId, sourceId, oldFunctionId, oldSourceId))
                {
                    CreatePolymorphicDynamicProfileCallSiteInfo(functionBody, callSiteId, functionId, oldFunctionId, sourceId, oldSourceId);
                }
                else
                {TRACE_IT(47547);
                    callSiteInfo[callSiteId].u.functionData.functionId = CallSiteMixed;
                }
            }
            callSiteInfo[callSiteId].isConstructorCall = isConstructorCall;
            callSiteInfo[callSiteId].dontInline = !doInline;
            callSiteInfo[callSiteId].ldFldInlineCacheId = ldFldInlineCacheId;
        }
        else
        {TRACE_IT(47548);
            Assert(doInline);
            Assert(callSiteInfo[callSiteId].isConstructorCall == isConstructorCall);
            RecordPolymorphicCallSiteInfo(functionBody, callSiteId, calleeFunctionInfo);
        }

        return;
    }

    bool DynamicProfileInfo::IsPolymorphicCallSite(Js::LocalFunctionId curFunctionId, Js::SourceId curSourceId, Js::LocalFunctionId oldFunctionId, Js::SourceId oldSourceId)
    {TRACE_IT(47549);
        AssertMsg(oldSourceId != NoSourceId, "There is no previous call in this callsite, we shouldn't be checking for polymorphic");
        if (oldSourceId == NoSourceId || oldSourceId == InvalidSourceId || oldSourceId == BuiltInSourceId)
        {TRACE_IT(47550);
            return false;
        }
        if (curFunctionId == CallSiteCrossContext || curFunctionId == CallSiteNonFunction || oldFunctionId == CallSiteMixed || oldFunctionId == CallSiteCrossContext)
        {TRACE_IT(47551);
            return false;
        }
        Assert(oldFunctionId != CallSiteNonFunction);
        Assert(curFunctionId != oldFunctionId || curSourceId != oldSourceId);
        return true;
    }

    void DynamicProfileInfo::CreatePolymorphicDynamicProfileCallSiteInfo(FunctionBody *funcBody, ProfileId callSiteId, Js::LocalFunctionId functionId, Js::LocalFunctionId oldFunctionId, Js::SourceId sourceId, Js::SourceId oldSourceId)
    {TRACE_IT(47552);
        PolymorphicCallSiteInfo *localPolyCallSiteInfo = RecyclerNewStructZ(funcBody->GetScriptContext()->GetRecycler(), PolymorphicCallSiteInfo);

        Assert(maxPolymorphicInliningSize >= 2);
        localPolyCallSiteInfo->functionIds[0] = oldFunctionId;
        localPolyCallSiteInfo->functionIds[1] = functionId;
        localPolyCallSiteInfo->sourceIds[0] = oldSourceId;
        localPolyCallSiteInfo->sourceIds[1] = sourceId;
        localPolyCallSiteInfo->next = funcBody->GetPolymorphicCallSiteInfoHead();

        for (int i = 2; i < maxPolymorphicInliningSize; i++)
        {TRACE_IT(47553);
            localPolyCallSiteInfo->functionIds[i] = CallSiteNoInfo;
        }

        callSiteInfo[callSiteId].isPolymorphic = true;
        callSiteInfo[callSiteId].u.polymorphicCallSiteInfo = localPolyCallSiteInfo;
        funcBody->SetPolymorphicCallSiteInfoHead(localPolyCallSiteInfo);
    }

    void DynamicProfileInfo::ResetAllPolymorphicCallSiteInfo()
    {TRACE_IT(47554);
        if (dynamicProfileFunctionInfo)
        {TRACE_IT(47555);
            for (ProfileId i = 0; i < dynamicProfileFunctionInfo->callSiteInfoCount; i++)
            {TRACE_IT(47556);
                if (callSiteInfo[i].isPolymorphic)
                {
                    ResetPolymorphicCallSiteInfo(i, CallSiteMixed);
                }
            }
        }
    }

    void DynamicProfileInfo::ResetPolymorphicCallSiteInfo(ProfileId callSiteId, Js::LocalFunctionId functionId)
    {TRACE_IT(47557);
        callSiteInfo[callSiteId].isPolymorphic = false;
        callSiteInfo[callSiteId].u.functionData.sourceId = CurrentSourceId;
        callSiteInfo[callSiteId].u.functionData.functionId = functionId;
        this->currentInlinerVersion++;
    }

    void DynamicProfileInfo::SetFunctionIdSlotForNewPolymorphicCall(ProfileId callSiteId, Js::LocalFunctionId curFunctionId, Js::SourceId curSourceId, Js::FunctionBody *inliner)
    {TRACE_IT(47558);
        for (int i = 0; i < maxPolymorphicInliningSize; i++)
        {TRACE_IT(47559);
            if (callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->functionIds[i] == curFunctionId &&
                callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->sourceIds[i] == curSourceId)
            {TRACE_IT(47560);
                // we have it already
                return;
            }
            else if (callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->functionIds[i] == CallSiteNoInfo)
            {TRACE_IT(47561);
                callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->functionIds[i] = curFunctionId;
                callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->sourceIds[i] = curSourceId;
                this->currentInlinerVersion++;
                return;
            }
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::PolymorphicInlinePhase))
        {TRACE_IT(47562);
            char16 debugStringBuffer[MAX_FUNCTION_BODY_DEBUG_STRING_SIZE];

            Output::Print(_u("INLINING (Polymorphic): More than 4 functions at this call site \t callSiteId: %d\t calleeFunctionId: %d TopFunc %s (%s)\n"),
                callSiteId,
                curFunctionId,
                inliner->GetDisplayName(),
                inliner->GetDebugNumberSet(debugStringBuffer)
                );
            Output::Flush();
        }
#endif

#ifdef PERF_HINT
        if (PHASE_TRACE1(Js::PerfHintPhase))
        {TRACE_IT(47563);
            WritePerfHint(PerfHints::PolymorphicInilineCap, inliner);
        }
#endif

        // We reached the max allowed to inline, no point in continuing collecting the information. Reset and move on.
        ResetPolymorphicCallSiteInfo(callSiteId, CallSiteMixed);
    }

    void DynamicProfileInfo::RecordPolymorphicCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, FunctionInfo * calleeFunctionInfo)
    {TRACE_IT(47564);
        Js::LocalFunctionId functionId;
        if (calleeFunctionInfo == nullptr || !calleeFunctionInfo->HasBody())
        {TRACE_IT(47565);
            return ResetPolymorphicCallSiteInfo(callSiteId, CallSiteMixed);
        }

        // We can only inline function that are from the same script context. So only record that data
        // We're about to call this function so deserialize it right now.
        FunctionProxy* calleeFunctionProxy = calleeFunctionInfo->GetFunctionProxy();

        if (functionBody->GetScriptContext() == calleeFunctionProxy->GetScriptContext())
        {TRACE_IT(47566);
            if (functionBody->GetSecondaryHostSourceContext() == calleeFunctionProxy->GetSecondaryHostSourceContext())
            {TRACE_IT(47567);
                Js::SourceId sourceId = (Js::SourceId)calleeFunctionProxy->GetHostSourceContext();
                if (functionBody->GetHostSourceContext() == sourceId)  // if caller and callee in same file
                {TRACE_IT(47568);
                    sourceId = CurrentSourceId;
                }
                functionId = calleeFunctionProxy->GetLocalFunctionId();
                SetFunctionIdSlotForNewPolymorphicCall(callSiteId, functionId, sourceId, functionBody);
                return;
            }
        }

        // Pretend that we are cross context when call is crossing script file.
        ResetPolymorphicCallSiteInfo(callSiteId, CallSiteCrossContext);
    }

    /* static */
    bool DynamicProfileInfo::HasCallSiteInfo(FunctionBody* functionBody)
    {TRACE_IT(47569);
        SourceContextInfo *sourceContextInfo = functionBody->GetSourceContextInfo();
        return !functionBody->GetScriptContext()->IsNoContextSourceContextInfo(sourceContextInfo);
    }

    bool DynamicProfileInfo::GetPolymorphicCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, bool *isConstructorCall, __inout_ecount(functionBodyArrayLength) FunctionBody** functionBodyArray, uint functionBodyArrayLength)
    {TRACE_IT(47570);
        Assert(functionBody);
        const auto callSiteCount = functionBody->GetProfiledCallSiteCount();
        Assert(callSiteId < callSiteCount);
        Assert(HasCallSiteInfo(functionBody));
        Assert(functionBodyArray);
        Assert(functionBodyArrayLength == DynamicProfileInfo::maxPolymorphicInliningSize);

        *isConstructorCall = callSiteInfo[callSiteId].isConstructorCall;
        if (callSiteInfo[callSiteId].dontInline)
        {TRACE_IT(47571);
            return false;
        }
        if (callSiteInfo[callSiteId].isPolymorphic)
        {TRACE_IT(47572);
            PolymorphicCallSiteInfo *polymorphicCallSiteInfo = callSiteInfo[callSiteId].u.polymorphicCallSiteInfo;

            for (uint i = 0; i < functionBodyArrayLength; i++)
            {TRACE_IT(47573);
                Js::LocalFunctionId localFunctionId;
                Js::SourceId localSourceId;
                if (!polymorphicCallSiteInfo->GetFunction(i, &localFunctionId, &localSourceId))
                {TRACE_IT(47574);
                    AssertMsg(i >= 2, "We found at least two function Body");
                    return true;
                }

                FunctionBody* matchedFunctionBody;

                if (localSourceId == CurrentSourceId)  // caller and callee in same file
                {TRACE_IT(47575);
                    matchedFunctionBody = functionBody->GetUtf8SourceInfo()->FindFunction(localFunctionId);
                    if (!matchedFunctionBody)
                    {TRACE_IT(47576);
                        return false;
                    }
                    functionBodyArray[i] = matchedFunctionBody;
                }
                else if (localSourceId == NoSourceId || localSourceId == InvalidSourceId)
                {TRACE_IT(47577);
                    return false;
                }
                else
                {TRACE_IT(47578);
                    // For call across files find the function from the right source
                    typedef JsUtil::List<RecyclerWeakReference<Utf8SourceInfo>*, Recycler, false, Js::FreeListedRemovePolicy> SourceList;
                    SourceList * sourceList = functionBody->GetScriptContext()->GetSourceList();
                    bool found = false;
                    for (int j = 0; j < sourceList->Count() && !found; j++)
                    {TRACE_IT(47579);
                        if (sourceList->IsItemValid(j))
                        {TRACE_IT(47580);
                            Utf8SourceInfo *srcInfo = sourceList->Item(j)->Get();
                            if (srcInfo && srcInfo->GetHostSourceContext() == localSourceId)
                            {TRACE_IT(47581);
                                matchedFunctionBody = srcInfo->FindFunction(localFunctionId);
                                if (!matchedFunctionBody)
                                {TRACE_IT(47582);
                                    return false;
                                }
                                functionBodyArray[i] = matchedFunctionBody;
                                found = true;
                            }
                        }
                    }
                    if (!found)
                    {TRACE_IT(47583);
                        return false;
                    }
                }
            }
            return true;
        }
        return false;
    }

    bool DynamicProfileInfo::HasCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId)
    {TRACE_IT(47584);
        Assert(functionBody);
        const auto callSiteCount = functionBody->GetProfiledCallSiteCount();
        Assert(callSiteId < callSiteCount);
        Assert(HasCallSiteInfo(functionBody));

        if (callSiteInfo[callSiteId].isPolymorphic)
        {TRACE_IT(47585);
            return true;
        }
        return callSiteInfo[callSiteId].u.functionData.sourceId != NoSourceId;
    }

    FunctionInfo * DynamicProfileInfo::GetCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, bool *isConstructorCall, bool *isPolymorphicCall)
    {TRACE_IT(47586);
        Assert(functionBody);
        const auto callSiteCount = functionBody->GetProfiledCallSiteCount();
        Assert(callSiteId < callSiteCount);
        Assert(HasCallSiteInfo(functionBody));

        *isConstructorCall = callSiteInfo[callSiteId].isConstructorCall;
        if (callSiteInfo[callSiteId].dontInline)
        {TRACE_IT(47587);
            return nullptr;
        }
        if (!callSiteInfo[callSiteId].isPolymorphic)
        {TRACE_IT(47588);
            Js::SourceId sourceId = callSiteInfo[callSiteId].u.functionData.sourceId;
            Js::LocalFunctionId functionId = callSiteInfo[callSiteId].u.functionData.functionId;
            if (sourceId == BuiltInSourceId)
            {TRACE_IT(47589);
                return JavascriptBuiltInFunction::GetFunctionInfo(functionId);
            }

            if (sourceId == CurrentSourceId) // caller and callee in same file
            {TRACE_IT(47590);
                FunctionProxy *inlineeProxy = functionBody->GetUtf8SourceInfo()->FindFunction(functionId);
                return inlineeProxy ? inlineeProxy->GetFunctionInfo() : nullptr;
            }

            if (sourceId != NoSourceId && sourceId != InvalidSourceId)
            {TRACE_IT(47591);
                // For call across files find the function from the right source
                JsUtil::List<RecyclerWeakReference<Utf8SourceInfo>*, Recycler, false, Js::FreeListedRemovePolicy> * sourceList = functionBody->GetScriptContext()->GetSourceList();
                for (int i = 0; i < sourceList->Count(); i++)
                {TRACE_IT(47592);
                    if (sourceList->IsItemValid(i))
                    {TRACE_IT(47593);
                        Utf8SourceInfo *srcInfo = sourceList->Item(i)->Get();
                        if (srcInfo && srcInfo->GetHostSourceContext() == sourceId)
                        {TRACE_IT(47594);
                            FunctionProxy *inlineeProxy = srcInfo->FindFunction(functionId);
                            return inlineeProxy ? inlineeProxy->GetFunctionInfo() : nullptr;
                        }
                    }
                }
            }
        }
        else
        {TRACE_IT(47595);
            *isPolymorphicCall = true;
        }
        return nullptr;
    }

    uint DynamicProfileInfo::GetLdFldCacheIndexFromCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId)
    {TRACE_IT(47596);
        Assert(functionBody);
        const auto callSiteCount = functionBody->GetProfiledCallSiteCount();
        Assert(callSiteId < callSiteCount);
        Assert(HasCallSiteInfo(functionBody));

        return callSiteInfo[callSiteId].ldFldInlineCacheId;
    }

    void DynamicProfileInfo::RecordElementLoad(FunctionBody* functionBody, ProfileId ldElemId, const LdElemInfo& info)
    {TRACE_IT(47597);
        Assert(ldElemId < functionBody->GetProfiledLdElemCount());
        Assert(info.WasProfiled());

        ldElemInfo[ldElemId].Merge(info);
    }

    void DynamicProfileInfo::RecordElementLoadAsProfiled(FunctionBody *const functionBody, const ProfileId ldElemId)
    {TRACE_IT(47598);
        Assert(ldElemId < functionBody->GetProfiledLdElemCount());
        ldElemInfo[ldElemId].wasProfiled = true;
    }

    void DynamicProfileInfo::RecordElementStore(FunctionBody* functionBody, ProfileId stElemId, const StElemInfo& info)
    {TRACE_IT(47599);
        Assert(stElemId < functionBody->GetProfiledStElemCount());
        Assert(info.WasProfiled());

        stElemInfo[stElemId].Merge(info);
    }

    void DynamicProfileInfo::RecordElementStoreAsProfiled(FunctionBody *const functionBody, const ProfileId stElemId)
    {TRACE_IT(47600);
        Assert(stElemId < functionBody->GetProfiledStElemCount());
        stElemInfo[stElemId].wasProfiled = true;
    }

    ArrayCallSiteInfo * DynamicProfileInfo::GetArrayCallSiteInfo(FunctionBody *functionBody, ProfileId index) const
    {TRACE_IT(47601);
        Assert(index < functionBody->GetProfiledArrayCallSiteCount());
        return &arrayCallSiteInfo[index];
    }

    void DynamicProfileInfo::RecordFieldAccess(FunctionBody* functionBody, uint fieldAccessId, Var object, FldInfoFlags flags)
    {TRACE_IT(47602);
        Assert(fieldAccessId < functionBody->GetProfiledFldCount());
        FldInfoFlags oldFlags = fldInfo[fieldAccessId].flags;
        if (object) // if not provided, the saved value type is not changed
        {TRACE_IT(47603);
            fldInfo[fieldAccessId].valueType = fldInfo[fieldAccessId].valueType.Merge(object);
        }
        const auto mergedFlags = MergeFldInfoFlags(oldFlags, flags);
        fldInfo[fieldAccessId].flags = mergedFlags;
        if (flags & FldInfo_Polymorphic)
        {TRACE_IT(47604);
            bits.hasPolymorphicFldAccess = true;
            if (!(oldFlags & FldInfo_Polymorphic))
            {TRACE_IT(47605);
                this->SetHasNewPolyFieldAccess(functionBody);
            }
            if (fldInfo[fieldAccessId].polymorphicInlineCacheUtilization < (PolymorphicInlineCacheUtilizationMaxValue - PolymorphicInlineCacheUtilizationIncrement))
            {TRACE_IT(47606);
                fldInfo[fieldAccessId].polymorphicInlineCacheUtilization += PolymorphicInlineCacheUtilizationIncrement;
            }
            else
            {TRACE_IT(47607);
                fldInfo[fieldAccessId].polymorphicInlineCacheUtilization = PolymorphicInlineCacheUtilizationMaxValue;
            }
        }
        else if (flags != FldInfo_NoInfo &&
            fldInfo[fieldAccessId].polymorphicInlineCacheUtilization != PolymorphicInlineCacheUtilizationMaxValue)
        {TRACE_IT(47608);
            if (fldInfo[fieldAccessId].polymorphicInlineCacheUtilization > (PolymorphicInlineCacheUtilizationMinValue + PolymorphicInlineCacheUtilizationDecrement))
            {TRACE_IT(47609);
                fldInfo[fieldAccessId].polymorphicInlineCacheUtilization -= PolymorphicInlineCacheUtilizationDecrement;
            }
            else
            {TRACE_IT(47610);
                fldInfo[fieldAccessId].polymorphicInlineCacheUtilization = PolymorphicInlineCacheUtilizationMinValue;
            }
        }
    }

    void DynamicProfileInfo::RecordDivideResultType(FunctionBody* body, ProfileId divideId, Var object)
    {TRACE_IT(47611);
        Assert(divideId < body->GetProfiledDivOrRemCount());
        divideTypeInfo[divideId] = divideTypeInfo[divideId].Merge(object);
    }

    // We are overloading the value types to store whether it is a mod by power of 2.
    // TaggedInt:
    void DynamicProfileInfo::RecordModulusOpType(FunctionBody* body,
                                 ProfileId profileId, bool isModByPowerOf2)
    {TRACE_IT(47612);
        Assert(profileId < body->GetProfiledDivOrRemCount());
        /* allow one op of the modulus to be optimized - anyway */
        if (divideTypeInfo[profileId].IsUninitialized())
        {TRACE_IT(47613);
            divideTypeInfo[profileId] = ValueType::GetInt(true);
        }
        else
        {TRACE_IT(47614);
            if (isModByPowerOf2)
            {TRACE_IT(47615);
                divideTypeInfo[profileId] = divideTypeInfo[profileId]
                                           .Merge(ValueType::GetInt(true));
            }
            else
            {TRACE_IT(47616);
                divideTypeInfo[profileId] = divideTypeInfo[profileId]
                                           .Merge(ValueType::Float);
            }
        }
    }

    bool DynamicProfileInfo::IsModulusOpByPowerOf2(FunctionBody* body, ProfileId profileId) const
    {TRACE_IT(47617);
        Assert(profileId < body->GetProfiledDivOrRemCount());
        return divideTypeInfo[profileId].IsLikelyTaggedInt();
    }

    ValueType DynamicProfileInfo::GetDivideResultType(FunctionBody* body, ProfileId divideId) const
    {TRACE_IT(47618);
        Assert(divideId < body->GetProfiledDivOrRemCount());
        return divideTypeInfo[divideId];
    }

    void DynamicProfileInfo::RecordSwitchType(FunctionBody* body, ProfileId switchId, Var object)
    {TRACE_IT(47619);
        Assert(switchId < body->GetProfiledSwitchCount());
        switchTypeInfo[switchId] = switchTypeInfo[switchId].Merge(object);
    }

    ValueType DynamicProfileInfo::GetSwitchType(FunctionBody* body, ProfileId switchId) const
    {TRACE_IT(47620);
        Assert(switchId < body->GetProfiledSwitchCount());
        return switchTypeInfo[switchId];
    }

    void DynamicProfileInfo::SetHasNewPolyFieldAccess(FunctionBody *functionBody)
    {TRACE_IT(47621);
        this->polymorphicCacheState = functionBody->GetScriptContext()->GetThreadContext()->GetNextPolymorphicCacheState();

        PHASE_PRINT_TRACE(
            Js::ObjTypeSpecPhase, functionBody,
            _u("New profile cache state: %d\n"), this->polymorphicCacheState);
    }

    void DynamicProfileInfo::RecordPolymorphicFieldAccess(FunctionBody* functionBody, uint fieldAccessId)
    {TRACE_IT(47622);
        this->RecordFieldAccess(functionBody, fieldAccessId, nullptr, FldInfo_Polymorphic);
    }

    void DynamicProfileInfo::RecordSlotLoad(FunctionBody* functionBody, ProfileId slotLoadId, Var object)
    {TRACE_IT(47623);
        Assert(slotLoadId < functionBody->GetProfiledSlotCount());
        slotInfo[slotLoadId] = slotInfo[slotLoadId].Merge(object);
    }

    FldInfoFlags DynamicProfileInfo::MergeFldInfoFlags(FldInfoFlags oldFlags, FldInfoFlags newFlags)
    {
        return static_cast<FldInfoFlags>(oldFlags | newFlags);
    }

    void DynamicProfileInfo::RecordParameterInfo(FunctionBody *functionBody, ArgSlot index, Var object)
    {TRACE_IT(47625);
        Assert(this->parameterInfo != nullptr);
        Assert(index < functionBody->GetProfiledInParamsCount());
        parameterInfo[index] = parameterInfo[index].Merge(object);
    }

    ValueType DynamicProfileInfo::GetParameterInfo(FunctionBody* functionBody, ArgSlot index) const
    {TRACE_IT(47626);
        Assert(this->parameterInfo != nullptr);
        Assert(index < functionBody->GetProfiledInParamsCount());
        return parameterInfo[index];
    }

    void DynamicProfileInfo::RecordReturnTypeOnCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, Var object)
    {TRACE_IT(47627);
        Assert(callSiteId < functionBody->GetProfiledCallSiteCount());
        this->callSiteInfo[callSiteId].returnType = this->callSiteInfo[callSiteId].returnType.Merge(object);
    }

    void DynamicProfileInfo::RecordReturnType(FunctionBody* functionBody, ProfileId callSiteId, Var object)
    {TRACE_IT(47628);
        Assert(callSiteId < functionBody->GetProfiledReturnTypeCount());
        this->returnTypeInfo[callSiteId] = this->returnTypeInfo[callSiteId].Merge(object);
    }

    ValueType DynamicProfileInfo::GetReturnType(FunctionBody* functionBody, Js::OpCode opcode, ProfileId callSiteId) const
    {TRACE_IT(47629);
        if (opcode < Js::OpCode::ProfiledReturnTypeCallI)
        {TRACE_IT(47630);
            Assert(IsProfiledCallOp(opcode));
            Assert(callSiteId < functionBody->GetProfiledCallSiteCount());
            return this->callSiteInfo[callSiteId].returnType;
        }
        Assert(IsProfiledReturnTypeOp(opcode));
        Assert(callSiteId < functionBody->GetProfiledReturnTypeCount());
        return this->returnTypeInfo[callSiteId];
    }

    void DynamicProfileInfo::RecordThisInfo(Var object, ThisType thisType)
    {TRACE_IT(47631);
        this->thisInfo.valueType = this->thisInfo.valueType.Merge(object);
        this->thisInfo.thisType = max(this->thisInfo.thisType, thisType);
    }

    ThisInfo DynamicProfileInfo::GetThisInfo() const
    {TRACE_IT(47632);
        return this->thisInfo;
    }

    void DynamicProfileInfo::RecordLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum, ImplicitCallFlags flags)
    {TRACE_IT(47633);
        Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(functionBody));
        Assert(loopNum < functionBody->GetLoopCount());
        this->loopImplicitCallFlags[loopNum] = (ImplicitCallFlags)(this->loopImplicitCallFlags[loopNum] | flags);
    }

    ImplicitCallFlags DynamicProfileInfo::GetLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum) const
    {TRACE_IT(47634);
        Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(functionBody));
        Assert(loopNum < functionBody->GetLoopCount());

        // Mask out the dispose implicit call. We would bailout on reentrant dispose,
        // but it shouldn't affect optimization.
        return (ImplicitCallFlags)(this->loopImplicitCallFlags[loopNum] & ImplicitCall_All);
    }

    void DynamicProfileInfo::RecordImplicitCallFlags(ImplicitCallFlags flags)
    {TRACE_IT(47635);
        this->implicitCallFlags = (ImplicitCallFlags)(this->implicitCallFlags | flags);
    }

    ImplicitCallFlags DynamicProfileInfo::GetImplicitCallFlags() const
    {TRACE_IT(47636);
        // Mask out the dispose implicit call. We would bailout on reentrant dispose,
        // but it shouldn't affect optimization.
        return (ImplicitCallFlags)(this->implicitCallFlags & ImplicitCall_All);
    }

    void DynamicProfileInfo::UpdateFunctionInfo(FunctionBody* functionBody, Recycler* recycler)
    {TRACE_IT(47637);
        Assert(this->persistsAcrossScriptContexts);

        if (!this->dynamicProfileFunctionInfo)
        {TRACE_IT(47638);
            this->dynamicProfileFunctionInfo = RecyclerNewStructLeaf(recycler, DynamicProfileFunctionInfo);
        }
        this->dynamicProfileFunctionInfo->callSiteInfoCount = functionBody->GetProfiledCallSiteCount();
        this->dynamicProfileFunctionInfo->paramInfoCount = functionBody->GetProfiledInParamsCount();
        this->dynamicProfileFunctionInfo->divCount = functionBody->GetProfiledDivOrRemCount();
        this->dynamicProfileFunctionInfo->switchCount = functionBody->GetProfiledSwitchCount();
        this->dynamicProfileFunctionInfo->returnTypeInfoCount = functionBody->GetProfiledReturnTypeCount();
        this->dynamicProfileFunctionInfo->loopCount = functionBody->GetLoopCount();
        this->dynamicProfileFunctionInfo->ldElemInfoCount = functionBody->GetProfiledLdElemCount();
        this->dynamicProfileFunctionInfo->stElemInfoCount = functionBody->GetProfiledStElemCount();
        this->dynamicProfileFunctionInfo->arrayCallSiteCount = functionBody->GetProfiledArrayCallSiteCount();
        this->dynamicProfileFunctionInfo->fldInfoCount = functionBody->GetProfiledFldCount();
        this->dynamicProfileFunctionInfo->slotInfoCount = functionBody->GetProfiledSlotCount();
    }

    void DynamicProfileInfo::Save(ScriptContext * scriptContext)
    {TRACE_IT(47639);
        // For now, we only support our local storage
#ifdef DYNAMIC_PROFILE_STORAGE
        if (!DynamicProfileStorage::IsEnabled())
        {TRACE_IT(47640);
            return;
        }

        if (scriptContext->GetSourceContextInfoMap() == nullptr)
        {TRACE_IT(47641);
            // We don't have savable code
            Assert(!scriptContext->GetProfileInfoList() || scriptContext->GetProfileInfoList()->Empty() || scriptContext->GetNoContextSourceContextInfo()->nextLocalFunctionId != 0);
            return;
        }
        DynamicProfileInfo::UpdateSourceDynamicProfileManagers(scriptContext);

        scriptContext->GetSourceContextInfoMap()->Map([&](DWORD_PTR dwHostSourceContext, SourceContextInfo * sourceContextInfo)
        {
            if (sourceContextInfo->sourceDynamicProfileManager != nullptr && sourceContextInfo->url != nullptr
                && !sourceContextInfo->IsDynamic())
            {TRACE_IT(47642);
                sourceContextInfo->sourceDynamicProfileManager->SaveToDynamicProfileStorage(sourceContextInfo->url);
            }
        });
#endif
    }

    bool DynamicProfileInfo::MatchFunctionBody(FunctionBody * functionBody)
    {TRACE_IT(47643);
        // This function is called to set a function body to the dynamic profile loaded from cache.
        // Need to verify that the function body matches with the profile info
        Assert(this->dynamicProfileFunctionInfo);
        if (this->dynamicProfileFunctionInfo->paramInfoCount != functionBody->GetProfiledInParamsCount()
            || this->dynamicProfileFunctionInfo->ldElemInfoCount != functionBody->GetProfiledLdElemCount()
            || this->dynamicProfileFunctionInfo->stElemInfoCount != functionBody->GetProfiledStElemCount()
            || this->dynamicProfileFunctionInfo->arrayCallSiteCount != functionBody->GetProfiledArrayCallSiteCount()
            || this->dynamicProfileFunctionInfo->fldInfoCount != functionBody->GetProfiledFldCount()
            || this->dynamicProfileFunctionInfo->slotInfoCount != functionBody->GetProfiledSlotCount()
            || this->dynamicProfileFunctionInfo->callSiteInfoCount != functionBody->GetProfiledCallSiteCount()
            || this->dynamicProfileFunctionInfo->returnTypeInfoCount != functionBody->GetProfiledReturnTypeCount()
            || this->dynamicProfileFunctionInfo->loopCount != functionBody->GetLoopCount()
            || this->dynamicProfileFunctionInfo->switchCount != functionBody->GetProfiledSwitchCount()
            || this->dynamicProfileFunctionInfo->divCount != functionBody->GetProfiledDivOrRemCount())
        {TRACE_IT(47644);
            // Reject, the dynamic profile information doesn't match the function body
            return false;
        }

#ifdef DYNAMIC_PROFILE_STORAGE
        this->functionBody = functionBody;
#endif

        this->hasFunctionBody = true;

        return true;
    }

    FldInfo * DynamicProfileInfo::GetFldInfo(FunctionBody* functionBody, uint fieldAccessId) const
    {TRACE_IT(47645);
        Assert(fieldAccessId < functionBody->GetProfiledFldCount());
        return &fldInfo[fieldAccessId];
    }

    ValueType DynamicProfileInfo::GetSlotLoad(FunctionBody* functionBody, ProfileId slotLoadId) const
    {TRACE_IT(47646);
        Assert(slotLoadId < functionBody->GetProfiledSlotCount());
        return slotInfo[slotLoadId];
    }

    FldInfoFlags DynamicProfileInfo::FldInfoFlagsFromCacheType(CacheType cacheType)
    {TRACE_IT(47647);
        switch (cacheType)
        {
        case CacheType_Local:
            return FldInfo_FromLocal;

        case CacheType_Proto:
            return FldInfo_FromProto;

        case CacheType_LocalWithoutProperty:
            return FldInfo_FromLocalWithoutProperty;

        case CacheType_Getter:
        case CacheType_Setter:
            return FldInfo_FromAccessor;

        default:
            return FldInfo_NoInfo;
        }
    }

    FldInfoFlags DynamicProfileInfo::FldInfoFlagsFromSlotType(SlotType slotType)
    {TRACE_IT(47648);
        switch (slotType)
        {
        case SlotType_Inline:
            return FldInfo_FromInlineSlots;

        case SlotType_Aux:
            return FldInfo_FromAuxSlots;

        default:
            return FldInfo_NoInfo;
        }
    }

#if DBG_DUMP
    void DynamicProfileInfo::DumpProfiledValue(char16 const * name, CallSiteInfo * callSiteInfo, uint count)
    {TRACE_IT(47649);
        if (count != 0)
        {TRACE_IT(47650);
            Output::Print(_u("    %-16s(%2d):"), name, count);
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47651);
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:"), i);
                if (!callSiteInfo[i].isPolymorphic)
                {TRACE_IT(47652);
                    switch (callSiteInfo[i].u.functionData.sourceId)
                    {
                    case NoSourceId:
                        Output::Print(_u(" ????"));
                        break;

                    case BuiltInSourceId:
                        Output::Print(_u(" b%03d"), callSiteInfo[i].u.functionData.functionId);
                        break;

                    case InvalidSourceId:
                        if (callSiteInfo[i].u.functionData.functionId == CallSiteMixed)
                        {TRACE_IT(47653);
                            Output::Print(_u("  mix"));
                        }
                        else if (callSiteInfo[i].u.functionData.functionId == CallSiteCrossContext)
                        {TRACE_IT(47654);
                            Output::Print(_u("    x"));
                        }
                        else if (callSiteInfo[i].u.functionData.functionId == CallSiteNonFunction)
                        {TRACE_IT(47655);
                            Output::Print(_u("  !fn"));
                        }
                        else
                        {TRACE_IT(47656);
                            Assert(false);
                        }
                        break;

                    default:
                        Output::Print(_u(" %4d:%4d"), callSiteInfo[i].u.functionData.sourceId, callSiteInfo[i].u.functionData.functionId);
                        break;
                    };
                }
                else
                {TRACE_IT(47657);
                    Output::Print(_u(" poly"));
                    for (int j = 0; j < DynamicProfileInfo::maxPolymorphicInliningSize; j++)
                    {TRACE_IT(47658);
                        if (callSiteInfo[i].u.polymorphicCallSiteInfo->functionIds[j] != CallSiteNoInfo)
                        {TRACE_IT(47659);
                            Output::Print(_u(" %4d:%4d"), callSiteInfo[i].u.polymorphicCallSiteInfo->sourceIds[j], callSiteInfo[i].u.polymorphicCallSiteInfo->functionIds[j]);
                        }
                    }
                }
            }
            Output::Print(_u("\n"));

            Output::Print(_u("    %-16s(%2d):"), _u("Callsite RetType"), count);
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47660);
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:"), i);
                char returnTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
                callSiteInfo[i].returnType.ToString(returnTypeStr);
                Output::Print(_u("  %S"), returnTypeStr);
            }
            Output::Print(_u("\n"));
        }
    }

    void DynamicProfileInfo::DumpProfiledValue(char16 const * name, ArrayCallSiteInfo * arrayCallSiteInfo, uint count)
    {TRACE_IT(47661);
        if (count != 0)
        {TRACE_IT(47662);
            Output::Print(_u("    %-16s(%2d):"), name, count);
            Output::Print(_u("\n"));
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47663);
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%4d:"), i);
                Output::Print(_u("  Function Number:  %2d, CallSite Number:  %2d, IsNativeIntArray:  %2d, IsNativeFloatArray:  %2d"),
                    arrayCallSiteInfo[i].functionNumber, arrayCallSiteInfo[i].callSiteNumber, !arrayCallSiteInfo[i].isNotNativeInt, !arrayCallSiteInfo[i].isNotNativeFloat);
                Output::Print(_u("\n"));
            }
            Output::Print(_u("\n"));
        }
    }

    void DynamicProfileInfo::DumpProfiledValue(char16 const * name, ValueType * value, uint count)
    {TRACE_IT(47664);
        if (count != 0)
        {TRACE_IT(47665);
            Output::Print(_u("    %-16s(%2d):"), name, count);
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47666);
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:"), i);
                char valueStr[VALUE_TYPE_MAX_STRING_SIZE];
                value[i].ToString(valueStr);
                Output::Print(_u("  %S"), valueStr);
            }
            Output::Print(_u("\n"));
        }
    }

    void DynamicProfileInfo::DumpProfiledValue(char16 const * name, uint * value, uint count)
    {TRACE_IT(47667);
        if (count != 0)
        {TRACE_IT(47668);
            Output::Print(_u("    %-16s(%2d):"), name, count);
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47669);
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:%-4d"), i, value[i]);
            }
            Output::Print(_u("\n"));
        }
    }

    char16 const * DynamicProfileInfo::GetImplicitCallFlagsString(ImplicitCallFlags flags)
    {TRACE_IT(47670);
        // Mask out the dispose implicit call. We would bailout on reentrant dispose,
        // but it shouldn't affect optimization
        flags = (ImplicitCallFlags)(flags & ImplicitCall_All);
        return flags == ImplicitCall_HasNoInfo ? _u("???") : flags == ImplicitCall_None ? _u("no") : _u("yes");
    }

    void DynamicProfileInfo::DumpProfiledValue(char16 const * name, ImplicitCallFlags * loopImplicitCallFlags, uint count)
    {TRACE_IT(47671);
        if (count != 0)
        {TRACE_IT(47672);
            Output::Print(_u("    %-16s(%2d):"), name, count);
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47673);
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:%-4s"), i, GetImplicitCallFlagsString(loopImplicitCallFlags[i]));
            }
            Output::Print(_u("\n"));
        }
    }

    bool DynamicProfileInfo::IsProfiledCallOp(OpCode op)
    {TRACE_IT(47674);
        return Js::OpCodeUtil::IsProfiledCallOp(op) || Js::OpCodeUtil::IsProfiledCallOpWithICIndex(op) || Js::OpCodeUtil::IsProfiledConstructorCall(op);
    }

    bool DynamicProfileInfo::IsProfiledReturnTypeOp(OpCode op)
    {TRACE_IT(47675);
        return Js::OpCodeUtil::IsProfiledReturnTypeCallOp(op);
    }

    template<class TData, class FGetValueType>
    void DynamicProfileInfo::DumpProfiledValuesGroupedByValue(
        const char16 *const name,
        const TData *const data,
        const uint count,
        const FGetValueType GetValueType,
        ArenaAllocator *const dynamicProfileInfoAllocator)
    {TRACE_IT(47676);
        JsUtil::BaseDictionary<ValueType, bool, ArenaAllocator> uniqueValueTypes(dynamicProfileInfoAllocator);
        for (uint i = 0; i < count; i++)
        {TRACE_IT(47677);
            const ValueType valueType(GetValueType(data, i));
            if (!valueType.IsUninitialized())
            {TRACE_IT(47678);
                uniqueValueTypes.Item(valueType, false);
            }
        }
        uniqueValueTypes.Map([&](const ValueType groupValueType, const bool)
        {
            bool header = true;
            uint lastTempFld = (uint)-1;
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47679);
                const ValueType valueType(GetValueType(data, i));
                if (valueType == groupValueType)
                {TRACE_IT(47680);
                    if (lastTempFld == (uint)-1)
                    {TRACE_IT(47681);
                        if (header)
                        {TRACE_IT(47682);
                            char valueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
                            valueType.ToString(valueTypeStr);
                            Output::Print(_u("    %s %S"), name, valueTypeStr);
                            Output::SkipToColumn(24);
                            Output::Print(_u(": %d"), i);
                        }
                        else
                        {TRACE_IT(47683);
                            Output::Print(_u(", %d"), i);
                        }
                        header = false;
                        lastTempFld = i;
                    }
                }
                else
                {TRACE_IT(47684);
                    if (lastTempFld != (uint)-1)
                    {TRACE_IT(47685);
                        if (lastTempFld != i - 1)
                        {TRACE_IT(47686);
                            Output::Print(_u("-%d"), i - 1);
                        }
                        lastTempFld = (uint)-1;
                    }
                }
            }
            if (lastTempFld != (uint)-1 && lastTempFld != count - 1)
            {TRACE_IT(47687);
                Output::Print(_u("-%d\n"), count - 1);
            }
            else if (!header)
            {TRACE_IT(47688);
                Output::Print(_u("\n"));
            }
        });
    }

    void DynamicProfileInfo::DumpFldInfoFlags(char16 const * name, FldInfo * fldInfo, uint count, FldInfoFlags value, char16 const * valueName)
    {TRACE_IT(47689);
        bool header = true;
        uint lastTempFld = (uint)-1;
        for (uint i = 0; i < count; i++)
        {TRACE_IT(47690);
            if (fldInfo[i].flags & value)
            {TRACE_IT(47691);
                if (lastTempFld == (uint)-1)
                {TRACE_IT(47692);
                    if (header)
                    {TRACE_IT(47693);
                        Output::Print(_u("    %s %s"), name, valueName);
                        Output::SkipToColumn(24);
                        Output::Print(_u(": %d"), i);
                    }
                    else
                    {TRACE_IT(47694);
                        Output::Print(_u(", %d"), i);
                    }
                    header = false;
                    lastTempFld = i;
                }
            }
            else
            {TRACE_IT(47695);
                if (lastTempFld != (uint)-1)
                {TRACE_IT(47696);
                    if (lastTempFld != i - 1)
                    {TRACE_IT(47697);
                        Output::Print(_u("-%d"), i - 1);
                    }
                    lastTempFld = (uint)-1;
                }
            }
        }
        if (lastTempFld != (uint)-1 && lastTempFld != count - 1)
        {TRACE_IT(47698);
            Output::Print(_u("-%d\n"), count - 1);
        }
        else if (!header)
        {TRACE_IT(47699);
            Output::Print(_u("\n"));
        }
    }

    void DynamicProfileInfo::DumpLoopInfo(FunctionBody *fbody)
    {TRACE_IT(47700);
        if (fbody->DoJITLoopBody())
        {TRACE_IT(47701);
            uint count = fbody->GetLoopCount();
            Output::Print(_u("    %-16s(%2d):"), _u("Loops"), count);
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47702);
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:%-4d"), i, fbody->GetLoopHeader(i)->interpretCount);
            }
            Output::Print(_u("\n"));

            Output::Print(_u("    %-16s(%2d):"), _u("Loops JIT"), count);
            for (uint i = 0; i < count; i++)
            {TRACE_IT(47703);
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:%-4d"), i, fbody->GetLoopHeader(i)->nativeCount);
            }
            Output::Print(_u("\n"));
        }
    }

    void DynamicProfileInfo::Dump(FunctionBody* functionBody, ArenaAllocator * dynamicProfileInfoAllocator)
    {TRACE_IT(47704);
        functionBody->DumpFunctionId(true);
        Js::ArgSlot paramcount = functionBody->GetProfiledInParamsCount();
        Output::Print(_u(": %-20s Interpreted:%6d, Param:%2d, ImpCall:%s, Callsite:%3d, ReturnType:%3d, LdElem:%3d, StElem:%3d, Fld%3d\n"),
            functionBody->GetDisplayName(), functionBody->GetInterpretedCount(), paramcount, DynamicProfileInfo::GetImplicitCallFlagsString(this->GetImplicitCallFlags()),
            functionBody->GetProfiledCallSiteCount(),
            functionBody->GetProfiledReturnTypeCount(),
            functionBody->GetProfiledLdElemCount(),
            functionBody->GetProfiledStElemCount(),
            functionBody->GetProfiledFldCount());

        if (Configuration::Global.flags.Verbose)
        {TRACE_IT(47705);
            DumpProfiledValue(_u("Div result type"), this->divideTypeInfo, functionBody->GetProfiledDivOrRemCount());
            DumpProfiledValue(_u("Switch opt type"), this->switchTypeInfo, functionBody->GetProfiledSwitchCount());
            DumpProfiledValue(_u("Param type"), this->parameterInfo, paramcount);
            DumpProfiledValue(_u("Callsite"), this->callSiteInfo, functionBody->GetProfiledCallSiteCount());
            DumpProfiledValue(_u("ArrayCallSite"), this->arrayCallSiteInfo, functionBody->GetProfiledArrayCallSiteCount());
            DumpProfiledValue(_u("Return type"), this->returnTypeInfo, functionBody->GetProfiledReturnTypeCount());
            if (dynamicProfileInfoAllocator)
            {TRACE_IT(47706);
                DumpProfiledValuesGroupedByValue(
                    _u("Element load"),
                    static_cast<LdElemInfo*>(this->ldElemInfo),
                    this->functionBody->GetProfiledLdElemCount(),
                    [](const LdElemInfo *const ldElemInfo, const uint i) -> ValueType
                {
                    return ldElemInfo[i].GetElementType();
                },
                    dynamicProfileInfoAllocator);
                DumpProfiledValuesGroupedByValue(
                    _u("Fld"),
                    static_cast<FldInfo *>(this->fldInfo),
                    functionBody->GetProfiledFldCount(),
                    [](const FldInfo *const fldInfos, const uint i) -> ValueType
                {
                    return fldInfos[i].valueType;
                },
                    dynamicProfileInfoAllocator);
            }
            DumpFldInfoFlags(_u("Fld"), this->fldInfo, functionBody->GetProfiledFldCount(), FldInfo_FromLocal, _u("FldInfo_FromLocal"));
            DumpFldInfoFlags(_u("Fld"), this->fldInfo, functionBody->GetProfiledFldCount(), FldInfo_FromProto, _u("FldInfo_FromProto"));
            DumpFldInfoFlags(_u("Fld"), this->fldInfo, functionBody->GetProfiledFldCount(), FldInfo_FromLocalWithoutProperty, _u("FldInfo_FromLocalWithoutProperty"));
            DumpFldInfoFlags(_u("Fld"), this->fldInfo, functionBody->GetProfiledFldCount(), FldInfo_FromAccessor, _u("FldInfo_FromAccessor"));
            DumpFldInfoFlags(_u("Fld"), this->fldInfo, functionBody->GetProfiledFldCount(), FldInfo_Polymorphic, _u("FldInfo_Polymorphic"));
            DumpFldInfoFlags(_u("Fld"), this->fldInfo, functionBody->GetProfiledFldCount(), FldInfo_FromInlineSlots, _u("FldInfo_FromInlineSlots"));
            DumpFldInfoFlags(_u("Fld"), this->fldInfo, functionBody->GetProfiledFldCount(), FldInfo_FromAuxSlots, _u("FldInfo_FromAuxSlots"));
            DumpLoopInfo(functionBody);
            if (DynamicProfileInfo::EnableImplicitCallFlags(functionBody))
            {TRACE_IT(47707);
                DumpProfiledValue(_u("Loop Imp Call"), this->loopImplicitCallFlags, functionBody->GetLoopCount());
            }
            if (functionBody->GetLoopCount())
            {TRACE_IT(47708);
                Output::Print(_u("    Loop Flags:\n"));
                for (uint i = 0; i < functionBody->GetLoopCount(); ++i)
                {TRACE_IT(47709);
                    Output::Print(_u("      Loop %d:\n"), i);
                    LoopFlags lf = this->GetLoopFlags(i);
                    Output::Print(
                        _u("        isInterpreted        : %s\n")
                        _u("        memopMinCountReached : %s\n"),
                        IsTrueOrFalse(lf.isInterpreted),
                        IsTrueOrFalse(lf.memopMinCountReached)
                        );
                }
            }
            Output::Print(
                _u("    Settings:")
                _u(" disableAggressiveIntTypeSpec : %s")
                _u(" disableAggressiveIntTypeSpec_jitLoopBody : %s")
                _u(" disableAggressiveMulIntTypeSpec : %s")
                _u(" disableAggressiveMulIntTypeSpec_jitLoopBody : %s")
                _u(" disableDivIntTypeSpec : %s")
                _u(" disableDivIntTypeSpec_jitLoopBody : %s")
                _u(" disableLossyIntTypeSpec : %s")
                _u(" disableMemOp : %s")
                _u(" disableTrackIntOverflow : %s")
                _u(" disableFloatTypeSpec : %s")
                _u(" disableCheckThis : %s")
                _u(" disableArrayCheckHoist : %s")
                _u(" disableArrayCheckHoist_jitLoopBody : %s")
                _u(" disableArrayMissingValueCheckHoist : %s")
                _u(" disableArrayMissingValueCheckHoist_jitLoopBody : %s")
                _u(" disableJsArraySegmentHoist : %s")
                _u(" disableJsArraySegmentHoist_jitLoopBody : %s")
                _u(" disableArrayLengthHoist : %s")
                _u(" disableArrayLengthHoist_jitLoopBody : %s")
                _u(" disableTypedArrayTypeSpec: %s")
                _u(" disableTypedArrayTypeSpec_jitLoopBody: %s")
                _u(" disableLdLenIntSpec: %s")
                _u(" disableBoundCheckHoist : %s")
                _u(" disableBoundCheckHoist_jitLoopBody : %s")
                _u(" disableLoopCountBasedBoundCheckHoist : %s")
                _u(" disableLoopCountBasedBoundCheckHoist_jitLoopBody : %s")
                _u(" hasPolymorphicFldAccess : %s")
                _u(" hasLdFldCallSite: %s")
                _u(" disableFloorInlining: %s")
                _u(" disableNoProfileBailouts: %s")
                _u(" disableSwitchOpt : %s")
                _u(" disableEquivalentObjTypeSpec : %s\n")
                _u(" disableObjTypeSpec_jitLoopBody : %s\n")
                _u(" disablePowIntTypeSpec : %s\n")
                _u(" disableStackArgOpt : %s\n")
                _u(" disableTagCheck : %s\n"),
                IsTrueOrFalse(this->bits.disableAggressiveIntTypeSpec),
                IsTrueOrFalse(this->bits.disableAggressiveIntTypeSpec_jitLoopBody),
                IsTrueOrFalse(this->bits.disableAggressiveMulIntTypeSpec),
                IsTrueOrFalse(this->bits.disableAggressiveMulIntTypeSpec_jitLoopBody),
                IsTrueOrFalse(this->bits.disableDivIntTypeSpec),
                IsTrueOrFalse(this->bits.disableDivIntTypeSpec_jitLoopBody),
                IsTrueOrFalse(this->bits.disableLossyIntTypeSpec),
                IsTrueOrFalse(this->bits.disableMemOp),
                IsTrueOrFalse(this->bits.disableTrackCompoundedIntOverflow),
                IsTrueOrFalse(this->bits.disableFloatTypeSpec),
                IsTrueOrFalse(this->bits.disableCheckThis),
                IsTrueOrFalse(this->bits.disableArrayCheckHoist),
                IsTrueOrFalse(this->bits.disableArrayCheckHoist_jitLoopBody),
                IsTrueOrFalse(this->bits.disableArrayMissingValueCheckHoist),
                IsTrueOrFalse(this->bits.disableArrayMissingValueCheckHoist_jitLoopBody),
                IsTrueOrFalse(this->bits.disableJsArraySegmentHoist),
                IsTrueOrFalse(this->bits.disableJsArraySegmentHoist_jitLoopBody),
                IsTrueOrFalse(this->bits.disableArrayLengthHoist),
                IsTrueOrFalse(this->bits.disableArrayLengthHoist_jitLoopBody),
                IsTrueOrFalse(this->bits.disableTypedArrayTypeSpec),
                IsTrueOrFalse(this->bits.disableTypedArrayTypeSpec_jitLoopBody),
                IsTrueOrFalse(this->bits.disableLdLenIntSpec),
                IsTrueOrFalse(this->bits.disableBoundCheckHoist),
                IsTrueOrFalse(this->bits.disableBoundCheckHoist_jitLoopBody),
                IsTrueOrFalse(this->bits.disableLoopCountBasedBoundCheckHoist),
                IsTrueOrFalse(this->bits.disableLoopCountBasedBoundCheckHoist_jitLoopBody),
                IsTrueOrFalse(this->bits.hasPolymorphicFldAccess),
                IsTrueOrFalse(this->bits.hasLdFldCallSite),
                IsTrueOrFalse(this->bits.disableFloorInlining),
                IsTrueOrFalse(this->bits.disableNoProfileBailouts),
                IsTrueOrFalse(this->bits.disableSwitchOpt),
                IsTrueOrFalse(this->bits.disableEquivalentObjTypeSpec),
                IsTrueOrFalse(this->bits.disableObjTypeSpec_jitLoopBody),
                IsTrueOrFalse(this->bits.disablePowIntIntTypeSpec),
                IsTrueOrFalse(this->bits.disableStackArgOpt),
                IsTrueOrFalse(this->bits.disableTagCheck));
        }
    }

    void DynamicProfileInfo::DumpList(
        DynamicProfileInfoList * profileInfoList, ArenaAllocator * dynamicProfileInfoAllocator)
    {TRACE_IT(47710);
        AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
        if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
        {TRACE_IT(47711);
            FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, profileInfoList)
            {TRACE_IT(47712);
                if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase, info->GetFunctionBody()->GetSourceContextId(), info->GetFunctionBody()->GetLocalFunctionId()))
                {TRACE_IT(47713);
                    info->Dump(info->GetFunctionBody(), dynamicProfileInfoAllocator);
                }
            }
            NEXT_SLISTBASE_ENTRY;
        }

        if (Configuration::Global.flags.Dump.IsEnabled(JITLoopBodyPhase) && !Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
        {TRACE_IT(47714);
            FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, profileInfoList)
            {TRACE_IT(47715);
                if (info->functionBody->GetLoopCount() > 0)
                {TRACE_IT(47716);
                    info->functionBody->DumpFunctionId(true);
                    Output::Print(_u(": %-20s\n"), info->functionBody->GetDisplayName());
                    DumpLoopInfo(info->functionBody);
                }
            }
            NEXT_SLISTBASE_ENTRY;
        }

        if (PHASE_STATS1(DynamicProfilePhase))
        {TRACE_IT(47717);
            uint estimatedSavedBytes = sizeof(uint); // count of functions
            uint functionSaved = 0;
            uint loopSaved = 0;
            uint callSiteSaved = 0;
            uint elementAccessSaved = 0;
            uint fldAccessSaved = 0;

            FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, profileInfoList)
            {TRACE_IT(47718);
                bool hasHotLoop = false;
                if (info->functionBody->DoJITLoopBody())
                {TRACE_IT(47719);
                    for (uint i = 0; i < info->functionBody->GetLoopCount(); i++)
                    {TRACE_IT(47720);
                        if (info->functionBody->GetLoopHeader(i)->interpretCount >= 10)
                        {TRACE_IT(47721);
                            hasHotLoop = true;
                            break;
                        }
                    }
                }

                if (hasHotLoop || info->functionBody->GetInterpretedCount() >= 10)
                {TRACE_IT(47722);
                    functionSaved++;
                    loopSaved += info->functionBody->GetLoopCount();

                    estimatedSavedBytes += sizeof(uint) * 5; // function number, loop count, call site count, local array, temp array
                    estimatedSavedBytes += (info->functionBody->GetLoopCount() + 7) / 8; // hot loop bit vector
                    estimatedSavedBytes += (info->functionBody->GetProfiledCallSiteCount() + 7) / 8; // call site bit vector
                    // call site function number
                    for (ProfileId i = 0; i < info->functionBody->GetProfiledCallSiteCount(); i++)
                    {TRACE_IT(47723);
                        // TODO poly
                        if ((info->callSiteInfo[i].u.functionData.sourceId != NoSourceId) && (info->callSiteInfo[i].u.functionData.sourceId != InvalidSourceId))
                        {TRACE_IT(47724);
                            estimatedSavedBytes += sizeof(CallSiteInfo);
                            callSiteSaved++;
                        }
                    }

                    elementAccessSaved += info->functionBody->GetProfiledLdElemCount() + info->functionBody->GetProfiledStElemCount();
                    fldAccessSaved += info->functionBody->GetProfiledFldCount();
                    estimatedSavedBytes += (info->functionBody->GetProfiledLdElemCount() + info->functionBody->GetProfiledStElemCount() + 7) / 8; // temp array access
                }
            }
            NEXT_SLISTBASE_ENTRY;

            if (estimatedSavedBytes != sizeof(uint))
            {TRACE_IT(47725);
                Output::Print(_u("Estimated save size (Memory used): %6d (%6d): %3d %3d %4d %4d %3d\n"),
                    estimatedSavedBytes, dynamicProfileInfoAllocator->Size(), functionSaved, loopSaved, callSiteSaved,
                    elementAccessSaved, fldAccessSaved);
            }
        }
    }

    void DynamicProfileInfo::DumpScriptContext(ScriptContext * scriptContext)
    {TRACE_IT(47726);
        if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
        {TRACE_IT(47727);
            Output::Print(_u("Sources:\n"));
            if (scriptContext->GetSourceContextInfoMap() != nullptr)
            {TRACE_IT(47728);
                scriptContext->GetSourceContextInfoMap()->Map([&](DWORD_PTR dwHostSourceContext, SourceContextInfo * sourceContextInfo)
                {
                    if (sourceContextInfo->sourceContextId != Js::Constants::NoSourceContext)
                    {TRACE_IT(47729);
                        Output::Print(_u("%2d: %s (Function count: %d)\n"), sourceContextInfo->sourceContextId, sourceContextInfo->url, sourceContextInfo->nextLocalFunctionId);
                    }
                });
            }

            if (scriptContext->GetDynamicSourceContextInfoMap() != nullptr)
            {TRACE_IT(47730);
                scriptContext->GetDynamicSourceContextInfoMap()->Map([&](DWORD_PTR dwHostSourceContext, SourceContextInfo * sourceContextInfo)
                {
                    Output::Print(_u("%2d: %d (Dynamic) (Function count: %d)\n"), sourceContextInfo->sourceContextId, sourceContextInfo->hash, sourceContextInfo->nextLocalFunctionId);
                });
            }
        }
        DynamicProfileInfo::DumpList(scriptContext->GetProfileInfoList(), scriptContext->DynamicProfileInfoAllocator());
        Output::Flush();
    }
#endif

#ifdef DYNAMIC_PROFILE_STORAGE
#if DBG_DUMP
    void BufferWriter::Log(DynamicProfileInfo* info)
    {TRACE_IT(47731);
        if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase, info->GetFunctionBody()->GetSourceContextId(), info->GetFunctionBody()->GetLocalFunctionId()))
        {TRACE_IT(47732);
            Output::Print(_u("Saving:"));
            info->Dump(info->GetFunctionBody());
        }
    }
#endif

    template <typename T>
    bool DynamicProfileInfo::Serialize(T * writer)
    {TRACE_IT(47733);
#if DBG_DUMP
        writer->Log(this);
#endif

        FunctionBody * functionBody = this->GetFunctionBody();
        Js::ArgSlot paramInfoCount = functionBody->GetProfiledInParamsCount();
        if (!writer->Write(functionBody->GetLocalFunctionId())
            || !writer->Write(paramInfoCount)
            || !writer->WriteArray(this->parameterInfo, paramInfoCount)
            || !writer->Write(functionBody->GetProfiledLdElemCount())
            || !writer->WriteArray(this->ldElemInfo, functionBody->GetProfiledLdElemCount())
            || !writer->Write(functionBody->GetProfiledStElemCount())
            || !writer->WriteArray(this->stElemInfo, functionBody->GetProfiledStElemCount())
            || !writer->Write(functionBody->GetProfiledArrayCallSiteCount())
            || !writer->WriteArray(this->arrayCallSiteInfo, functionBody->GetProfiledArrayCallSiteCount())
            || !writer->Write(functionBody->GetProfiledFldCount())
            || !writer->WriteArray(this->fldInfo, functionBody->GetProfiledFldCount())
            || !writer->Write(functionBody->GetProfiledSlotCount())
            || !writer->WriteArray(this->slotInfo, functionBody->GetProfiledSlotCount())
            || !writer->Write(functionBody->GetProfiledCallSiteCount())
            || !writer->WriteArray(this->callSiteInfo, functionBody->GetProfiledCallSiteCount())
            || !writer->Write(functionBody->GetProfiledDivOrRemCount())
            || !writer->WriteArray(this->divideTypeInfo, functionBody->GetProfiledDivOrRemCount())
            || !writer->Write(functionBody->GetProfiledSwitchCount())
            || !writer->WriteArray(this->switchTypeInfo, functionBody->GetProfiledSwitchCount())
            || !writer->Write(functionBody->GetProfiledReturnTypeCount())
            || !writer->WriteArray(this->returnTypeInfo, functionBody->GetProfiledReturnTypeCount())
            || !writer->Write(functionBody->GetLoopCount())
            || !writer->WriteArray(this->loopImplicitCallFlags, functionBody->GetLoopCount())
            || !writer->Write(this->implicitCallFlags)
            || !writer->Write(this->thisInfo)
            || !writer->Write(this->bits)
            || !writer->Write(this->m_recursiveInlineInfo)
            || (this->loopFlags && !writer->WriteArray(this->loopFlags->GetData(), this->loopFlags->WordCount())))
        {TRACE_IT(47734);
            return false;
        }
        return true;
    }

    template <typename T>
    DynamicProfileInfo * DynamicProfileInfo::Deserialize(T * reader, Recycler* recycler, Js::LocalFunctionId * functionId)
    {TRACE_IT(47735);
        Js::ArgSlot paramInfoCount = 0;
        ProfileId ldElemInfoCount = 0;
        ProfileId stElemInfoCount = 0;
        ProfileId arrayCallSiteCount = 0;
        ProfileId slotInfoCount = 0;
        ProfileId callSiteInfoCount = 0;
        ProfileId returnTypeInfoCount = 0;
        ProfileId divCount = 0;
        ProfileId switchCount = 0;
        uint fldInfoCount = 0;
        uint loopCount = 0;
        ValueType * paramInfo = nullptr;
        LdElemInfo * ldElemInfo = nullptr;
        StElemInfo * stElemInfo = nullptr;
        ArrayCallSiteInfo * arrayCallSiteInfo = nullptr;
        FldInfo * fldInfo = nullptr;
        ValueType * slotInfo = nullptr;
        CallSiteInfo * callSiteInfo = nullptr;
        ValueType * divTypeInfo = nullptr;
        ValueType * switchTypeInfo = nullptr;
        ValueType * returnTypeInfo = nullptr;
        ImplicitCallFlags * loopImplicitCallFlags = nullptr;
        BVFixed * loopFlags = nullptr;
        ImplicitCallFlags implicitCallFlags;
        ThisInfo thisInfo;
        Bits bits;
        uint32 recursiveInlineInfo = 0;

        try
        {TRACE_IT(47736);
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);

            if (!reader->Read(functionId))
            {TRACE_IT(47737);
                return nullptr;
            }

            if (!reader->Read(&paramInfoCount))
            {TRACE_IT(47738);
                return nullptr;
            }

            if (paramInfoCount != 0)
            {TRACE_IT(47739);
                paramInfo = RecyclerNewArrayLeaf(recycler, ValueType, paramInfoCount);
                if (!reader->ReadArray(paramInfo, paramInfoCount))
                {TRACE_IT(47740);
                    goto Error;
                }
            }

            if (!reader->Read(&ldElemInfoCount))
            {TRACE_IT(47741);
                goto Error;
            }

            if (ldElemInfoCount != 0)
            {TRACE_IT(47742);
                ldElemInfo = RecyclerNewArrayLeaf(recycler, LdElemInfo, ldElemInfoCount);
                if (!reader->ReadArray(ldElemInfo, ldElemInfoCount))
                {TRACE_IT(47743);
                    goto Error;
                }
            }

            if (!reader->Read(&stElemInfoCount))
            {TRACE_IT(47744);
                goto Error;
            }

            if (stElemInfoCount != 0)
            {TRACE_IT(47745);
                stElemInfo = RecyclerNewArrayLeaf(recycler, StElemInfo, stElemInfoCount);
                if (!reader->ReadArray(stElemInfo, stElemInfoCount))
                {TRACE_IT(47746);
                    goto Error;
                }
            }

            if (!reader->Read(&arrayCallSiteCount))
            {TRACE_IT(47747);
                goto Error;
            }

            if (arrayCallSiteCount != 0)
            {TRACE_IT(47748);
                arrayCallSiteInfo = RecyclerNewArrayLeaf(recycler, ArrayCallSiteInfo, arrayCallSiteCount);
                if (!reader->ReadArray(arrayCallSiteInfo, arrayCallSiteCount))
                {TRACE_IT(47749);
                    goto Error;
                }
            }

            if (!reader->Read(&fldInfoCount))
            {TRACE_IT(47750);
                goto Error;
            }

            if (fldInfoCount != 0)
            {TRACE_IT(47751);
                fldInfo = RecyclerNewArrayLeaf(recycler, FldInfo, fldInfoCount);
                if (!reader->ReadArray(fldInfo, fldInfoCount))
                {TRACE_IT(47752);
                    goto Error;
                }
            }

            if (!reader->Read(&slotInfoCount))
            {TRACE_IT(47753);
                goto Error;
            }

            if (slotInfoCount != 0)
            {TRACE_IT(47754);
                slotInfo = RecyclerNewArrayLeaf(recycler, ValueType, slotInfoCount);
                if (!reader->ReadArray(slotInfo, slotInfoCount))
                {TRACE_IT(47755);
                    goto Error;
                }
            }

            if (!reader->Read(&callSiteInfoCount))
            {TRACE_IT(47756);
                goto Error;
            }

            if (callSiteInfoCount != 0)
            {TRACE_IT(47757);
                // CallSiteInfo contains pointer "polymorphicCallSiteInfo", but
                // we explicitly save that pointer in FunctionBody. Safe to
                // allocate CallSiteInfo[] as Leaf here.
                callSiteInfo = RecyclerNewArrayLeaf(recycler, CallSiteInfo, callSiteInfoCount);
                if (!reader->ReadArray(callSiteInfo, callSiteInfoCount))
                {TRACE_IT(47758);
                    goto Error;
                }
            }

            if (!reader->Read(&divCount))
            {TRACE_IT(47759);
                goto Error;
            }

            if (divCount != 0)
            {TRACE_IT(47760);
                divTypeInfo = RecyclerNewArrayLeaf(recycler, ValueType, divCount);
                if (!reader->ReadArray(divTypeInfo, divCount))
                {TRACE_IT(47761);
                    goto Error;
                }
            }

            if (!reader->Read(&switchCount))
            {TRACE_IT(47762);
                goto Error;
            }

            if (switchCount != 0)
            {TRACE_IT(47763);
                switchTypeInfo = RecyclerNewArrayLeaf(recycler, ValueType, switchCount);
                if (!reader->ReadArray(switchTypeInfo, switchCount))
                {TRACE_IT(47764);
                    goto Error;
                }
            }

            if (!reader->Read(&returnTypeInfoCount))
            {TRACE_IT(47765);
                goto Error;
            }

            if (returnTypeInfoCount != 0)
            {TRACE_IT(47766);
                returnTypeInfo = RecyclerNewArrayLeaf(recycler, ValueType, returnTypeInfoCount);
                if (!reader->ReadArray(returnTypeInfo, returnTypeInfoCount))
                {TRACE_IT(47767);
                    goto Error;
                }
            }

            if (!reader->Read(&loopCount))
            {TRACE_IT(47768);
                goto Error;
            }

            if (loopCount != 0)
            {TRACE_IT(47769);
                loopImplicitCallFlags = RecyclerNewArrayLeaf(recycler, ImplicitCallFlags, loopCount);
                if (!reader->ReadArray(loopImplicitCallFlags, loopCount))
                {TRACE_IT(47770);
                    goto Error;
                }
            }

            if (!reader->Read(&implicitCallFlags) ||
                !reader->Read(&thisInfo) ||
                !reader->Read(&bits) ||
                !reader->Read(&recursiveInlineInfo))
            {TRACE_IT(47771);
                goto Error;
            }

            if (loopCount != 0)
            {TRACE_IT(47772);
                loopFlags = BVFixed::New(loopCount * LoopFlags::COUNT, recycler);
                if (!reader->ReadArray(loopFlags->GetData(), loopFlags->WordCount()))
                {TRACE_IT(47773);
                    goto Error;
                }
            }

            DynamicProfileFunctionInfo * dynamicProfileFunctionInfo = RecyclerNewStructLeaf(recycler, DynamicProfileFunctionInfo);
            dynamicProfileFunctionInfo->paramInfoCount = paramInfoCount;
            dynamicProfileFunctionInfo->ldElemInfoCount = ldElemInfoCount;
            dynamicProfileFunctionInfo->stElemInfoCount = stElemInfoCount;
            dynamicProfileFunctionInfo->arrayCallSiteCount = arrayCallSiteCount;
            dynamicProfileFunctionInfo->fldInfoCount = fldInfoCount;
            dynamicProfileFunctionInfo->slotInfoCount = slotInfoCount;
            dynamicProfileFunctionInfo->callSiteInfoCount = callSiteInfoCount;
            dynamicProfileFunctionInfo->divCount = divCount;
            dynamicProfileFunctionInfo->switchCount = switchCount;
            dynamicProfileFunctionInfo->returnTypeInfoCount = returnTypeInfoCount;
            dynamicProfileFunctionInfo->loopCount = loopCount;

            DynamicProfileInfo * dynamicProfileInfo = RecyclerNew(recycler, DynamicProfileInfo);
            dynamicProfileInfo->dynamicProfileFunctionInfo = dynamicProfileFunctionInfo;
            dynamicProfileInfo->parameterInfo = paramInfo;
            dynamicProfileInfo->ldElemInfo = ldElemInfo;
            dynamicProfileInfo->stElemInfo = stElemInfo;
            dynamicProfileInfo->arrayCallSiteInfo = arrayCallSiteInfo;
            dynamicProfileInfo->fldInfo = fldInfo;
            dynamicProfileInfo->slotInfo = slotInfo;
            dynamicProfileInfo->callSiteInfo = callSiteInfo;
            dynamicProfileInfo->divideTypeInfo = divTypeInfo;
            dynamicProfileInfo->switchTypeInfo = switchTypeInfo;
            dynamicProfileInfo->returnTypeInfo = returnTypeInfo;
            dynamicProfileInfo->loopImplicitCallFlags = loopImplicitCallFlags;
            dynamicProfileInfo->implicitCallFlags = implicitCallFlags;
            dynamicProfileInfo->loopFlags = loopFlags;
            dynamicProfileInfo->thisInfo = thisInfo;
            dynamicProfileInfo->bits = bits;
            dynamicProfileInfo->m_recursiveInlineInfo = recursiveInlineInfo;

            // Fixed functions and object type data is not serialized. There is no point in trying to serialize polymorphic call site info.
            dynamicProfileInfo->ResetAllPolymorphicCallSiteInfo();

            return dynamicProfileInfo;
        }
        catch (OutOfMemoryException)
        {TRACE_IT(47774);
        }

    Error:
        return nullptr;
    }

    // Explicit instantiations - to force the compiler to generate these - so they can be referenced from other compilation units.
    template DynamicProfileInfo * DynamicProfileInfo::Deserialize<BufferReader>(BufferReader*, Recycler*, Js::LocalFunctionId *);
    template bool DynamicProfileInfo::Serialize<BufferSizeCounter>(BufferSizeCounter*);
    template bool DynamicProfileInfo::Serialize<BufferWriter>(BufferWriter*);

    void DynamicProfileInfo::UpdateSourceDynamicProfileManagers(ScriptContext * scriptContext)
    {TRACE_IT(47775);
        // We don't clear old dynamic data here, because if a function is inlined, it will never go through the
        // EnsureDynamicProfileThunk and thus not appear in the list. We would want to keep those data as well.
        // Just save/update the data from function that has execute.

        // That means that the data will never go away, probably not a good policy if this is cached for web page in WININET.

        DynamicProfileInfoList * profileInfoList = scriptContext->GetProfileInfoList();
        FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, profileInfoList)
        {TRACE_IT(47776);
            FunctionBody * functionBody = info->GetFunctionBody();
            SourceDynamicProfileManager * sourceDynamicProfileManager = functionBody->GetSourceContextInfo()->sourceDynamicProfileManager;
            sourceDynamicProfileManager->SaveDynamicProfileInfo(functionBody->GetLocalFunctionId(), info);
        }
        NEXT_SLISTBASE_ENTRY
    }
#endif

#ifdef RUNTIME_DATA_COLLECTION
    CriticalSection DynamicProfileInfo::s_csOutput;

    template <typename T>
    void DynamicProfileInfo::WriteData(const T& data, FILE * file)
    {
        fwrite(&data, sizeof(T), 1, file);
    }

    template <>
    void DynamicProfileInfo::WriteData<char16 const *>(char16 const * const& sz, FILE * file)
    {TRACE_IT(47777);
        if (sz)
        {TRACE_IT(47778);
            charcount_t len = static_cast<charcount_t>(wcslen(sz));
            utf8char_t * tempBuffer = HeapNewArray(utf8char_t, len * 3);
            size_t cbNeeded = utf8::EncodeInto(tempBuffer, sz, len);
            fwrite(&cbNeeded, sizeof(cbNeeded), 1, file);
            fwrite(tempBuffer, sizeof(utf8char_t), cbNeeded, file);
            HeapDeleteArray(len * 3, tempBuffer);
        }
        else
        {TRACE_IT(47779);
            charcount_t len = 0;
            fwrite(&len, sizeof(len), 1, file);
        }
    }

    template <typename T>
    void DynamicProfileInfo::WriteArray(uint count, T * arr, FILE * file)
    {
        WriteData(count, file);
        for (uint i = 0; i < count; i++)
        {TRACE_IT(47780);
            WriteData(arr[i], file);
        }
    }

    template <typename T>
    void DynamicProfileInfo::WriteArray(uint count, WriteBarrierPtr<T> arr, FILE * file)
    {
        WriteArray(count, static_cast<T*>(arr), file);
    }

    template <>
    void DynamicProfileInfo::WriteData<FunctionBody *>(FunctionBody * const& functionBody, FILE * file)
    {TRACE_IT(47781);
        WriteData(functionBody->GetSourceContextInfo()->sourceContextId, file);
        WriteData(functionBody->GetLocalFunctionId(), file);
    }

    void DynamicProfileInfo::DumpScriptContextToFile(ScriptContext * scriptContext)
    {TRACE_IT(47782);
        if (Configuration::Global.flags.RuntimeDataOutputFile == nullptr)
        {TRACE_IT(47783);
            return;
        }

        AutoCriticalSection autocs(&s_csOutput);
        FILE * file;
        if (_wfopen_s(&file, Configuration::Global.flags.RuntimeDataOutputFile, _u("ab+")) != 0 || file == nullptr)
        {TRACE_IT(47784);
            return;
        }

        WriteData(scriptContext->GetAllocId(), file);
        WriteData(scriptContext->GetCreateTime(), file);
        WriteData(scriptContext->GetUrl(), file);
        WriteData(scriptContext->GetSourceContextInfoMap() != nullptr ? scriptContext->GetSourceContextInfoMap()->Count() : 0, file);

        if (scriptContext->GetSourceContextInfoMap())
        {TRACE_IT(47785);
            scriptContext->GetSourceContextInfoMap()->Map([&](DWORD_PTR dwHostSourceContext, SourceContextInfo * sourceContextInfo)
            {
                WriteData(sourceContextInfo->sourceContextId, file);
                WriteData(sourceContextInfo->nextLocalFunctionId, file);
                WriteData(sourceContextInfo->url, file);
            });
        }

        FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, scriptContext->GetProfileInfoList())
        {TRACE_IT(47786);
            WriteData((byte)1, file);
            WriteData(info->functionBody, file);
            WriteData(info->functionBody->GetDisplayName(), file);
            WriteData(info->functionBody->GetInterpretedCount(), file);
            uint loopCount = info->functionBody->GetLoopCount();
            WriteData(loopCount, file);
            for (uint i = 0; i < loopCount; i++)
            {TRACE_IT(47787);
                if (info->functionBody->DoJITLoopBody())
                {TRACE_IT(47788);
                    WriteData(info->functionBody->GetLoopHeader(i)->interpretCount, file);
                }
                else
                {
                    WriteData(-1, file);
                }
            }
            WriteArray(info->functionBody->GetProfiledLdElemCount(), info->ldElemInfo, file);
            WriteArray(info->functionBody->GetProfiledStElemCount(), info->stElemInfo, file);
            WriteArray(info->functionBody->GetProfiledArrayCallSiteCount(), info->arrayCallSiteInfo, file);
            WriteArray(info->functionBody->GetProfiledCallSiteCount(), info->callSiteInfo, file);
        }
        NEXT_SLISTBASE_ENTRY;

        WriteData((byte)0, file);
        fflush(file);
        fclose(file);
    }
#endif

    void DynamicProfileInfo::InstantiateForceInlinedMembers()
    {TRACE_IT(47789);
        // Force-inlined functions defined in a translation unit need a reference from an extern non-force-inlined function in the
        // same translation unit to force an instantiation of the force-inlined function. Otherwise, if the force-inlined function
        // is not referenced in the same translation unit, it will not be generated and the linker is not able to find the
        // definition to inline the function in other translation units.
        Assert(false);

        FunctionBody *const functionBody = nullptr;
        const Js::Var var = nullptr;

        DynamicProfileInfo *const p = nullptr;
        p->RecordFieldAccess(functionBody, 0, var, FldInfo_NoInfo);
        p->RecordDivideResultType(functionBody, 0, var);
        p->RecordModulusOpType(functionBody, 0, false);
        p->RecordSwitchType(functionBody, 0, var);
        p->RecordPolymorphicFieldAccess(functionBody, 0);
        p->RecordSlotLoad(functionBody, 0, var);
        p->RecordParameterInfo(functionBody, 0, var);
        p->RecordReturnTypeOnCallSiteInfo(functionBody, 0, var);
        p->RecordReturnType(functionBody, 0, var);
        p->RecordThisInfo(var, ThisType_Unknown);
    }
};

bool IR::IsTypeCheckBailOutKind(IR::BailOutKind kind)
{TRACE_IT(47790);
    IR::BailOutKind kindWithoutBits = kind & ~IR::BailOutKindBits;
    return
        kindWithoutBits == IR::BailOutFailedTypeCheck ||
        kindWithoutBits == IR::BailOutFailedFixedFieldTypeCheck ||
        kindWithoutBits == IR::BailOutFailedEquivalentTypeCheck ||
        kindWithoutBits == IR::BailOutFailedEquivalentFixedFieldTypeCheck;
}

bool IR::IsEquivalentTypeCheckBailOutKind(IR::BailOutKind kind)
{TRACE_IT(47791);
    IR::BailOutKind kindWithoutBits = kind & ~IR::BailOutKindBits;
    return
        kindWithoutBits == IR::BailOutFailedEquivalentTypeCheck ||
        kindWithoutBits == IR::BailOutFailedEquivalentFixedFieldTypeCheck;
}

IR::BailOutKind IR::EquivalentToMonoTypeCheckBailOutKind(IR::BailOutKind kind)
{TRACE_IT(47792);
    switch (kind & ~IR::BailOutKindBits)
    {
    case IR::BailOutFailedEquivalentTypeCheck:
        return IR::BailOutFailedTypeCheck | (kind & IR::BailOutKindBits);

    case IR::BailOutFailedEquivalentFixedFieldTypeCheck:
        return IR::BailOutFailedFixedFieldTypeCheck | (kind & IR::BailOutKindBits);

    default:
        Assert(0);
        return IR::BailOutInvalid;
    }
}

#if ENABLE_DEBUG_CONFIG_OPTIONS
const char *const BailOutKindNames[] =
{
#define BAIL_OUT_KIND_LAST(n)               "" STRINGIZE(n) ""
#define BAIL_OUT_KIND(n, ...)               BAIL_OUT_KIND_LAST(n),
#define BAIL_OUT_KIND_VALUE_LAST(n, v)      BAIL_OUT_KIND_LAST(n)
#define BAIL_OUT_KIND_VALUE(n, v)           BAIL_OUT_KIND(n)
#include "BailOutKind.h"
};

IR::BailOutKind const BailOutKindValidBits[] =
{
#define BAIL_OUT_KIND(n, bits)               (IR::BailOutKind)bits,
#define BAIL_OUT_KIND_VALUE_LAST(n, v)
#define BAIL_OUT_KIND_VALUE(n, v)
#define BAIL_OUT_KIND_LAST(n)
#include "BailOutKind.h"
};

bool IsValidBailOutKindAndBits(IR::BailOutKind bailOutKind)
{TRACE_IT(47793);
    IR::BailOutKind kindNoBits = bailOutKind & ~IR::BailOutKindBits;
    if (kindNoBits >= IR::BailOutKindBitsStart)
    {TRACE_IT(47794);
        return false;
    }
    return ((bailOutKind & IR::BailOutKindBits) & ~BailOutKindValidBits[kindNoBits]) == 0;
}

// Concats into the buffer, specified by the name parameter, the name of 'bit' bailout kind, specified by the enumEntryOffsetFromBitsStart parameter.
// Returns the number of bytes printed to the buffer.
size_t ConcatBailOutKindBits(_Out_writes_bytes_(dstSizeBytes) char* dst, _In_ size_t dstSizeBytes, _In_ size_t position, _In_ uint enumEntryOffsetFromBitsStart)
{TRACE_IT(47795);
    const char* kindName = BailOutKindNames[IR::BailOutKindBitsStart + static_cast<IR::BailOutKind>(enumEntryOffsetFromBitsStart)];
    int printedBytes =
        sprintf_s(
            &dst[position],
            dstSizeBytes - position * sizeof(dst[0]),
            position == 0 ? "%s" : " | %s",
            kindName);
    return printedBytes;
}

const char* GetBailOutKindName(IR::BailOutKind kind)
{TRACE_IT(47796);
    using namespace IR;

    if (!(kind & BailOutKindBits))
    {TRACE_IT(47797);
        return BailOutKindNames[kind];
    }

    static char name[512];
    size_t position = 0;
    const auto normalKind = kind & ~BailOutKindBits;
    if (normalKind != 0)
    {TRACE_IT(47798);
        kind -= normalKind;
        position +=
            sprintf_s(
                &name[position],
                sizeof(name) / sizeof(name[0]) - position * sizeof(name[0]),
                position == 0 ? "%s" : " | %s",
                BailOutKindNames[normalKind]);
    }

    uint offset = 1;
    if (kind & BailOutOnOverflow)
    {TRACE_IT(47799);
        kind ^= BailOutOnOverflow;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnMulOverflow)
    {TRACE_IT(47800);
        kind ^= BailOutOnMulOverflow;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnNegativeZero)
    {TRACE_IT(47801);
        kind ^= BailOutOnNegativeZero;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnPowIntIntOverflow)
    {TRACE_IT(47802);
        kind ^= BailOutOnPowIntIntOverflow;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    // BailOutOnResultConditions

    ++offset;
    if (kind & BailOutOnMissingValue)
    {TRACE_IT(47803);
        kind ^= BailOutOnMissingValue;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutConventionalNativeArrayAccessOnly)
    {TRACE_IT(47804);
        kind ^= BailOutConventionalNativeArrayAccessOnly;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutConvertedNativeArray)
    {TRACE_IT(47805);
        kind ^= BailOutConvertedNativeArray;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnArrayAccessHelperCall)
    {TRACE_IT(47806);
        kind ^= BailOutOnArrayAccessHelperCall;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnInvalidatedArrayHeadSegment)
    {TRACE_IT(47807);
        kind ^= BailOutOnInvalidatedArrayHeadSegment;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnInvalidatedArrayLength)
    {TRACE_IT(47808);
        kind ^= BailOutOnInvalidatedArrayLength;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOnStackArgsOutOfActualsRange)
    {TRACE_IT(47809);
        kind ^= BailOnStackArgsOutOfActualsRange;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    // BailOutForArrayBits

    ++offset;
    if (kind & BailOutForceByFlag)
    {TRACE_IT(47810);
        kind ^= BailOutForceByFlag;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutBreakPointInFunction)
    {TRACE_IT(47811);
        kind ^= BailOutBreakPointInFunction;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutStackFrameBase)
    {TRACE_IT(47812);
        kind ^= BailOutStackFrameBase;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutLocalValueChanged)
    {TRACE_IT(47813);
        kind ^= BailOutLocalValueChanged;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutExplicit)
    {TRACE_IT(47814);
        kind ^= BailOutExplicit;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutStep)
    {TRACE_IT(47815);
        kind ^= BailOutStep;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutIgnoreException)
    {TRACE_IT(47816);
        kind ^= BailOutIgnoreException;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    // BailOutForDebuggerBits

    ++offset;
    if (kind & BailOutOnDivByZero)
    {TRACE_IT(47817);
        kind ^= BailOutOnDivByZero;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnDivOfMinInt)
    {TRACE_IT(47818);
        kind ^= BailOutOnDivOfMinInt;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    // BailOutOnDivSrcConditions

    ++offset;
    if (kind & BailOutMarkTempObject)
    {TRACE_IT(47819);
        kind ^= BailOutMarkTempObject;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }

    ++offset;
    // BailOutKindBits

    Assert(position != 0);
    Assert(!kind);
    return name;
}
#endif
#endif
