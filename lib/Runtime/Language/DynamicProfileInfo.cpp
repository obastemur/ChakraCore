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
    {LOGMEIN("DynamicProfileInfo.cpp] 11\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 24\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 38\n");
        OUTPUT_TRACE_WITH_STACK(Js::NativeArrayConversionPhase, _u("SetIsNotNativeIntArray \n"));
        bits |= NotNativeIntBit;
    }

    void ArrayCallSiteInfo::SetIsNotNativeFloatArray()
    {LOGMEIN("DynamicProfileInfo.cpp] 44\n");
        OUTPUT_TRACE_WITH_STACK(Js::NativeArrayConversionPhase, _u("SetIsNotNativeFloatArray \n"));
        bits |= NotNativeFloatBit;
    }

    void ArrayCallSiteInfo::SetIsNotNativeArray()
    {LOGMEIN("DynamicProfileInfo.cpp] 50\n");
        OUTPUT_TRACE_WITH_STACK(Js::NativeArrayConversionPhase, _u("SetIsNotNativeArray \n"));
        bits = NotNativeIntBit | NotNativeFloatBit;
    }

    DynamicProfileInfo* DynamicProfileInfo::New(Recycler* recycler, FunctionBody* functionBody, bool persistsAcrossScriptContexts)
    {LOGMEIN("DynamicProfileInfo.cpp] 56\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 75\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 88\n");
            info = RecyclerNewPlusZ(recycler, totalAlloc, DynamicProfileInfo, functionBody);
#if DBG
            info->persistsAcrossScriptContexts = true;
#endif
        }
        else
        {
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
            if (DynamicProfileInfo::NeedProfileInfoList())
            {LOGMEIN("DynamicProfileInfo.cpp] 98\n");
                info = RecyclerNewPlusZ(recycler, totalAlloc, DynamicProfileInfo, functionBody);
            }
            else
#endif
            {
                info = RecyclerNewPlusLeafZ(recycler, totalAlloc, DynamicProfileInfo, functionBody);
            }
        }
        BYTE* current = (BYTE*)info + sizeof(DynamicProfileInfo);

        for (uint i = 0; i < _countof(batch); i++)
        {LOGMEIN("DynamicProfileInfo.cpp] 110\n");
            if (batch[i].size > 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 112\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 136\n");
        // Need to make value types uninitialized, which is not equivalent to zero
        thisInfo.valueType = ValueType::Uninitialized;
        const BVIndex loopFlagsCount = functionBody->GetLoopCount() * LoopFlags::COUNT;
        if (loopFlagsCount)
        {LOGMEIN("DynamicProfileInfo.cpp] 141\n");
            this->loopFlags->Init(loopFlagsCount);
            LoopFlags defaultValues;
            for (uint i = 0; i < functionBody->GetLoopCount(); ++i)
            {LOGMEIN("DynamicProfileInfo.cpp] 145\n");
                this->loopFlags->SetRange(&defaultValues, i * LoopFlags::COUNT, LoopFlags::COUNT);
            }
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledCallSiteCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 150\n");
            callSiteInfo[i].returnType = ValueType::Uninitialized;
            callSiteInfo[i].u.functionData.sourceId = NoSourceId;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledLdElemCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 155\n");
            ldElemInfo[i].arrayType = ValueType::Uninitialized;
            ldElemInfo[i].elemType = ValueType::Uninitialized;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledStElemCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 160\n");
            stElemInfo[i].arrayType = ValueType::Uninitialized;
        }
        for (uint i = 0; i < functionBody->GetProfiledFldCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 164\n");
            fldInfo[i].flags = FldInfo_NoInfo;
            fldInfo[i].valueType = ValueType::Uninitialized;
            fldInfo[i].polymorphicInlineCacheUtilization = PolymorphicInlineCacheUtilizationThreshold;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledDivOrRemCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 170\n");
            divideTypeInfo[i] = ValueType::Uninitialized;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledSwitchCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 174\n");
            switchTypeInfo[i] = ValueType::Uninitialized;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledSlotCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 178\n");
            slotInfo[i] = ValueType::Uninitialized;
        }
        for (ArgSlot i = 0; i < functionBody->GetProfiledInParamsCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 182\n");
            parameterInfo[i] = ValueType::Uninitialized;
        }
        for (ProfileId i = 0; i < functionBody->GetProfiledReturnTypeCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 186\n");
            returnTypeInfo[i] = ValueType::Uninitialized;
        }

        this->rejitCount = 0;
        this->bailOutOffsetForLastRejit = Js::Constants::NoByteCodeOffset;
#if DBG
        for (ProfileId i = 0; i < functionBody->GetProfiledArrayCallSiteCount(); ++i)
        {LOGMEIN("DynamicProfileInfo.cpp] 194\n");
            arrayCallSiteInfo[i].functionNumber = functionBody->GetFunctionNumber();
            arrayCallSiteInfo[i].callSiteNumber = i;
        }
#endif

#if TTD_NATIVE_PROFILE_ARRAY_WORK_AROUND
        if(functionBody->GetScriptContext()->GetThreadContext()->IsRuntimeInTTDMode())
        {LOGMEIN("DynamicProfileInfo.cpp] 202\n");
            for(ProfileId i = 0; i < functionBody->GetProfiledArrayCallSiteCount(); ++i)
            {LOGMEIN("DynamicProfileInfo.cpp] 204\n");
                arrayCallSiteInfo[i].SetIsNotNativeArray();
            }
        }
#endif
    }

    bool DynamicProfileInfo::IsEnabledForAtLeastOneFunction(const ScriptContext *const scriptContext)
    {LOGMEIN("DynamicProfileInfo.cpp] 212\n");
        return IsEnabled_OptionalFunctionBody(nullptr, scriptContext);
    }

    bool DynamicProfileInfo::IsEnabled(const FunctionBody *const functionBody)
    {LOGMEIN("DynamicProfileInfo.cpp] 217\n");
        Assert(functionBody);
        return IsEnabled_OptionalFunctionBody(functionBody, functionBody->GetScriptContext());
    }

    bool DynamicProfileInfo::IsEnabled_OptionalFunctionBody(const FunctionBody *const functionBody, const ScriptContext *const scriptContext)
    {LOGMEIN("DynamicProfileInfo.cpp] 223\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 243\n");
        return IsEnabled_OptionalFunctionBody(phase, nullptr, scriptContext);
    }

    bool DynamicProfileInfo::IsEnabled(const Js::Phase phase, const FunctionBody *const functionBody)
    {LOGMEIN("DynamicProfileInfo.cpp] 248\n");
        Assert(functionBody);
        return IsEnabled_OptionalFunctionBody(phase, functionBody, functionBody->GetScriptContext());
    }

    bool DynamicProfileInfo::IsEnabled_OptionalFunctionBody(
        const Js::Phase phase,
        const FunctionBody *const functionBody,
        const ScriptContext *const scriptContext)
    {LOGMEIN("DynamicProfileInfo.cpp] 257\n");
        if (!DynamicProfileInfo::IsEnabled_OptionalFunctionBody(functionBody, scriptContext))
        {LOGMEIN("DynamicProfileInfo.cpp] 259\n");
            return false;
        }

        switch (phase)
        {LOGMEIN("DynamicProfileInfo.cpp] 264\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 290\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 318\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 336\n");
            functionBody->EnsureDynamicProfileInfo();
            if (functionBody->GetScriptContext()->CurrentThunk == ProfileEntryThunk)
            {LOGMEIN("DynamicProfileInfo.cpp] 339\n");
                directEntryPoint = ProfileEntryThunk;
            }
            else
            {
                directEntryPoint = (JavascriptMethod)entryPoint->GetNativeAddress();
            }

            entryPoint->jsMethod = directEntryPoint;
        }
        else
        {
            Assert(directEntryPoint == ProfileEntryThunk || functionBody->GetScriptContext()->IsNativeAddress((void*)directEntryPoint));
            Assert(functionBody->HasExecutionDynamicProfileInfo());
        }

        return function->UpdateThunkEntryPoint(static_cast<FunctionEntryPointInfo*>(entryPoint), directEntryPoint);
    }

    bool DynamicProfileInfo::HasLdFldCallSiteInfo() const
    {LOGMEIN("DynamicProfileInfo.cpp] 359\n");
        return bits.hasLdFldCallSite;
    }

    bool DynamicProfileInfo::RecordLdFldCallSiteInfo(FunctionBody* functionBody, RecyclableObject* callee, bool callApplyTarget)
    {LOGMEIN("DynamicProfileInfo.cpp] 364\n");
        auto SetBits = [&]() -> bool
        {
            this->bits.hasLdFldCallSite = true;
            this->currentInlinerVersion++; // we don't mind if this overflows
            return true;
        };

        FunctionInfo* calleeFunctionInfo = callee->GetTypeId() == TypeIds_Function ? JavascriptFunction::FromVar(callee)->GetFunctionInfo() : nullptr;
        if (calleeFunctionInfo == nullptr)
        {LOGMEIN("DynamicProfileInfo.cpp] 374\n");
            return false;
        }
        else if (!calleeFunctionInfo->HasBody())
        {LOGMEIN("DynamicProfileInfo.cpp] 378\n");
            // We can inline fastDOM getter/setter.
            // We can directly call Math.max/min as apply targets.
            if ((calleeFunctionInfo->GetAttributes() & Js::FunctionInfo::Attributes::NeedCrossSiteSecurityCheck) ||
                (callApplyTarget && (calleeFunctionInfo->GetAttributes() & Js::FunctionInfo::Attributes::BuiltInInlinableAsLdFldInlinee)))
            {LOGMEIN("DynamicProfileInfo.cpp] 383\n");
                if (functionBody->GetScriptContext() == callee->GetScriptContext())
                {LOGMEIN("DynamicProfileInfo.cpp] 385\n");
                    return SetBits();
                }
            }
            return false;
        }
        else if (functionBody->CheckCalleeContextForInlining(calleeFunctionInfo->GetFunctionProxy()))
        {LOGMEIN("DynamicProfileInfo.cpp] 392\n");
            // If functionInfo !HasBody(), the previous 'else if' branch is executed; otherwise it has a body and therefore it has a proxy
            return SetBits();
        }
        return false;
    }

    void DynamicProfileInfo::RecordConstParameterAtCallSite(ProfileId callSiteId, int argNum)
    {LOGMEIN("DynamicProfileInfo.cpp] 400\n");
        Assert(argNum < Js::InlineeCallInfo::MaxInlineeArgoutCount);
        Assert(callSiteId < functionBody->GetProfiledCallSiteCount());
        callSiteInfo[callSiteId].isArgConstant = callSiteInfo[callSiteId].isArgConstant | (1 << argNum);
    }

    uint16 DynamicProfileInfo::GetConstantArgInfo(ProfileId callSiteId)
    {LOGMEIN("DynamicProfileInfo.cpp] 407\n");
        return callSiteInfo[callSiteId].isArgConstant;
    }

    void DynamicProfileInfo::RecordCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, FunctionInfo* calleeFunctionInfo, JavascriptFunction* calleeFunction, ArgSlot actualArgCount, bool isConstructorCall, InlineCacheIndex ldFldInlineCacheId)
    {LOGMEIN("DynamicProfileInfo.cpp] 412\n");
#if DBG_DUMP || defined(DYNAMIC_PROFILE_STORAGE) || defined(RUNTIME_DATA_COLLECTION)
        // If we persistsAcrossScriptContext, the dynamic profile info may be referred to by multiple function body from
        // different script context
        Assert(!DynamicProfileInfo::NeedProfileInfoList() || this->persistsAcrossScriptContexts || this->functionBody == functionBody);
#endif
        bool doInline = true;
        // This is a hard limit as we only use 4 bits to encode the actual count in the InlineeCallInfo
        if (actualArgCount > Js::InlineeCallInfo::MaxInlineeArgoutCount)
        {LOGMEIN("DynamicProfileInfo.cpp] 421\n");
            doInline = false;
        }

        // Mark the callsite bit where caller and callee is same function
        if (calleeFunctionInfo && functionBody == calleeFunctionInfo->GetFunctionProxy() && callSiteId < 32)
        {LOGMEIN("DynamicProfileInfo.cpp] 427\n");
            this->m_recursiveInlineInfo = this->m_recursiveInlineInfo | (1 << callSiteId);
        }

        if (!callSiteInfo[callSiteId].isPolymorphic)
        {LOGMEIN("DynamicProfileInfo.cpp] 432\n");
            Js::SourceId oldSourceId = callSiteInfo[callSiteId].u.functionData.sourceId;
            if (oldSourceId == InvalidSourceId)
            {LOGMEIN("DynamicProfileInfo.cpp] 435\n");
                return;
            }

            Js::LocalFunctionId oldFunctionId = callSiteInfo[callSiteId].u.functionData.functionId;

            Js::SourceId sourceId = InvalidSourceId;
            Js::LocalFunctionId functionId;
            if (calleeFunctionInfo == nullptr)
            {LOGMEIN("DynamicProfileInfo.cpp] 444\n");
                functionId = CallSiteNonFunction;
            }
            else if (!calleeFunctionInfo->HasBody())
            {LOGMEIN("DynamicProfileInfo.cpp] 448\n");
                Assert(calleeFunction); // calleeFunction can only be passed as null if the calleeFunctionInfo was null (which is checked above)
                if (functionBody->GetScriptContext() == calleeFunction->GetScriptContext())
                {LOGMEIN("DynamicProfileInfo.cpp] 451\n");
                    sourceId = BuiltInSourceId;
                    functionId = calleeFunctionInfo->GetLocalFunctionId();
                }
                else
                {
                    functionId = CallSiteCrossContext;
                }
            }
            else
            {
                // We can only inline function that are from the same script context. So only record that data
                // We're about to call this function so deserialize it right now
                FunctionProxy* calleeFunctionProxy = calleeFunctionInfo->GetFunctionProxy();
                if (functionBody->GetScriptContext() == calleeFunctionProxy->GetScriptContext())
                {LOGMEIN("DynamicProfileInfo.cpp] 466\n");
                    if (functionBody->GetSecondaryHostSourceContext() == calleeFunctionProxy->GetSecondaryHostSourceContext())
                    {LOGMEIN("DynamicProfileInfo.cpp] 468\n");
                        if (functionBody->GetHostSourceContext() == calleeFunctionProxy->GetHostSourceContext())
                        {LOGMEIN("DynamicProfileInfo.cpp] 470\n");
                            sourceId = CurrentSourceId; // Caller and callee in same file
                        }
                        else
                        {
                            sourceId = (Js::SourceId)calleeFunctionProxy->GetHostSourceContext(); // Caller and callee in different files
                        }
                        functionId = calleeFunctionProxy->GetLocalFunctionId();
                    }
                    else
                    {
                        // Pretend that we are cross context when call is crossing script file.
                        functionId = CallSiteCrossContext;
                    }
                }
                else
                {
                    functionId = CallSiteCrossContext;
                }
            }

            if (oldSourceId == NoSourceId)
            {LOGMEIN("DynamicProfileInfo.cpp] 492\n");
                callSiteInfo[callSiteId].u.functionData.sourceId = sourceId;
                callSiteInfo[callSiteId].u.functionData.functionId = functionId;
                this->currentInlinerVersion++; // we don't mind if this overflows
            }
            else if (oldSourceId != sourceId || oldFunctionId != functionId)
            {LOGMEIN("DynamicProfileInfo.cpp] 498\n");
                if (oldFunctionId != CallSiteMixed)
                {LOGMEIN("DynamicProfileInfo.cpp] 500\n");
                    this->currentInlinerVersion++; // we don't mind if this overflows
                }

                if (doInline && IsPolymorphicCallSite(functionId, sourceId, oldFunctionId, oldSourceId))
                {
                    CreatePolymorphicDynamicProfileCallSiteInfo(functionBody, callSiteId, functionId, oldFunctionId, sourceId, oldSourceId);
                }
                else
                {
                    callSiteInfo[callSiteId].u.functionData.functionId = CallSiteMixed;
                }
            }
            callSiteInfo[callSiteId].isConstructorCall = isConstructorCall;
            callSiteInfo[callSiteId].dontInline = !doInline;
            callSiteInfo[callSiteId].ldFldInlineCacheId = ldFldInlineCacheId;
        }
        else
        {
            Assert(doInline);
            Assert(callSiteInfo[callSiteId].isConstructorCall == isConstructorCall);
            RecordPolymorphicCallSiteInfo(functionBody, callSiteId, calleeFunctionInfo);
        }

        return;
    }

    bool DynamicProfileInfo::IsPolymorphicCallSite(Js::LocalFunctionId curFunctionId, Js::SourceId curSourceId, Js::LocalFunctionId oldFunctionId, Js::SourceId oldSourceId)
    {LOGMEIN("DynamicProfileInfo.cpp] 528\n");
        AssertMsg(oldSourceId != NoSourceId, "There is no previous call in this callsite, we shouldn't be checking for polymorphic");
        if (oldSourceId == NoSourceId || oldSourceId == InvalidSourceId || oldSourceId == BuiltInSourceId)
        {LOGMEIN("DynamicProfileInfo.cpp] 531\n");
            return false;
        }
        if (curFunctionId == CallSiteCrossContext || curFunctionId == CallSiteNonFunction || oldFunctionId == CallSiteMixed || oldFunctionId == CallSiteCrossContext)
        {LOGMEIN("DynamicProfileInfo.cpp] 535\n");
            return false;
        }
        Assert(oldFunctionId != CallSiteNonFunction);
        Assert(curFunctionId != oldFunctionId || curSourceId != oldSourceId);
        return true;
    }

    void DynamicProfileInfo::CreatePolymorphicDynamicProfileCallSiteInfo(FunctionBody *funcBody, ProfileId callSiteId, Js::LocalFunctionId functionId, Js::LocalFunctionId oldFunctionId, Js::SourceId sourceId, Js::SourceId oldSourceId)
    {LOGMEIN("DynamicProfileInfo.cpp] 544\n");
        PolymorphicCallSiteInfo *localPolyCallSiteInfo = RecyclerNewStructZ(funcBody->GetScriptContext()->GetRecycler(), PolymorphicCallSiteInfo);

        Assert(maxPolymorphicInliningSize >= 2);
        localPolyCallSiteInfo->functionIds[0] = oldFunctionId;
        localPolyCallSiteInfo->functionIds[1] = functionId;
        localPolyCallSiteInfo->sourceIds[0] = oldSourceId;
        localPolyCallSiteInfo->sourceIds[1] = sourceId;
        localPolyCallSiteInfo->next = funcBody->GetPolymorphicCallSiteInfoHead();

        for (int i = 2; i < maxPolymorphicInliningSize; i++)
        {LOGMEIN("DynamicProfileInfo.cpp] 555\n");
            localPolyCallSiteInfo->functionIds[i] = CallSiteNoInfo;
        }

        callSiteInfo[callSiteId].isPolymorphic = true;
        callSiteInfo[callSiteId].u.polymorphicCallSiteInfo = localPolyCallSiteInfo;
        funcBody->SetPolymorphicCallSiteInfoHead(localPolyCallSiteInfo);
    }

    void DynamicProfileInfo::ResetAllPolymorphicCallSiteInfo()
    {LOGMEIN("DynamicProfileInfo.cpp] 565\n");
        if (dynamicProfileFunctionInfo)
        {LOGMEIN("DynamicProfileInfo.cpp] 567\n");
            for (ProfileId i = 0; i < dynamicProfileFunctionInfo->callSiteInfoCount; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 569\n");
                if (callSiteInfo[i].isPolymorphic)
                {
                    ResetPolymorphicCallSiteInfo(i, CallSiteMixed);
                }
            }
        }
    }

    void DynamicProfileInfo::ResetPolymorphicCallSiteInfo(ProfileId callSiteId, Js::LocalFunctionId functionId)
    {LOGMEIN("DynamicProfileInfo.cpp] 579\n");
        callSiteInfo[callSiteId].isPolymorphic = false;
        callSiteInfo[callSiteId].u.functionData.sourceId = CurrentSourceId;
        callSiteInfo[callSiteId].u.functionData.functionId = functionId;
        this->currentInlinerVersion++;
    }

    void DynamicProfileInfo::SetFunctionIdSlotForNewPolymorphicCall(ProfileId callSiteId, Js::LocalFunctionId curFunctionId, Js::SourceId curSourceId, Js::FunctionBody *inliner)
    {LOGMEIN("DynamicProfileInfo.cpp] 587\n");
        for (int i = 0; i < maxPolymorphicInliningSize; i++)
        {LOGMEIN("DynamicProfileInfo.cpp] 589\n");
            if (callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->functionIds[i] == curFunctionId &&
                callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->sourceIds[i] == curSourceId)
            {LOGMEIN("DynamicProfileInfo.cpp] 592\n");
                // we have it already
                return;
            }
            else if (callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->functionIds[i] == CallSiteNoInfo)
            {LOGMEIN("DynamicProfileInfo.cpp] 597\n");
                callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->functionIds[i] = curFunctionId;
                callSiteInfo[callSiteId].u.polymorphicCallSiteInfo->sourceIds[i] = curSourceId;
                this->currentInlinerVersion++;
                return;
            }
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        if (Js::Configuration::Global.flags.TestTrace.IsEnabled(Js::PolymorphicInlinePhase))
        {LOGMEIN("DynamicProfileInfo.cpp] 607\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 622\n");
            WritePerfHint(PerfHints::PolymorphicInilineCap, inliner);
        }
#endif

        // We reached the max allowed to inline, no point in continuing collecting the information. Reset and move on.
        ResetPolymorphicCallSiteInfo(callSiteId, CallSiteMixed);
    }

    void DynamicProfileInfo::RecordPolymorphicCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, FunctionInfo * calleeFunctionInfo)
    {LOGMEIN("DynamicProfileInfo.cpp] 632\n");
        Js::LocalFunctionId functionId;
        if (calleeFunctionInfo == nullptr || !calleeFunctionInfo->HasBody())
        {LOGMEIN("DynamicProfileInfo.cpp] 635\n");
            return ResetPolymorphicCallSiteInfo(callSiteId, CallSiteMixed);
        }

        // We can only inline function that are from the same script context. So only record that data
        // We're about to call this function so deserialize it right now.
        FunctionProxy* calleeFunctionProxy = calleeFunctionInfo->GetFunctionProxy();

        if (functionBody->GetScriptContext() == calleeFunctionProxy->GetScriptContext())
        {LOGMEIN("DynamicProfileInfo.cpp] 644\n");
            if (functionBody->GetSecondaryHostSourceContext() == calleeFunctionProxy->GetSecondaryHostSourceContext())
            {LOGMEIN("DynamicProfileInfo.cpp] 646\n");
                Js::SourceId sourceId = (Js::SourceId)calleeFunctionProxy->GetHostSourceContext();
                if (functionBody->GetHostSourceContext() == sourceId)  // if caller and callee in same file
                {LOGMEIN("DynamicProfileInfo.cpp] 649\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 664\n");
        SourceContextInfo *sourceContextInfo = functionBody->GetSourceContextInfo();
        return !functionBody->GetScriptContext()->IsNoContextSourceContextInfo(sourceContextInfo);
    }

    bool DynamicProfileInfo::GetPolymorphicCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, bool *isConstructorCall, __inout_ecount(functionBodyArrayLength) FunctionBody** functionBodyArray, uint functionBodyArrayLength)
    {LOGMEIN("DynamicProfileInfo.cpp] 670\n");
        Assert(functionBody);
        const auto callSiteCount = functionBody->GetProfiledCallSiteCount();
        Assert(callSiteId < callSiteCount);
        Assert(HasCallSiteInfo(functionBody));
        Assert(functionBodyArray);
        Assert(functionBodyArrayLength == DynamicProfileInfo::maxPolymorphicInliningSize);

        *isConstructorCall = callSiteInfo[callSiteId].isConstructorCall;
        if (callSiteInfo[callSiteId].dontInline)
        {LOGMEIN("DynamicProfileInfo.cpp] 680\n");
            return false;
        }
        if (callSiteInfo[callSiteId].isPolymorphic)
        {LOGMEIN("DynamicProfileInfo.cpp] 684\n");
            PolymorphicCallSiteInfo *polymorphicCallSiteInfo = callSiteInfo[callSiteId].u.polymorphicCallSiteInfo;

            for (uint i = 0; i < functionBodyArrayLength; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 688\n");
                Js::LocalFunctionId localFunctionId;
                Js::SourceId localSourceId;
                if (!polymorphicCallSiteInfo->GetFunction(i, &localFunctionId, &localSourceId))
                {LOGMEIN("DynamicProfileInfo.cpp] 692\n");
                    AssertMsg(i >= 2, "We found at least two function Body");
                    return true;
                }

                FunctionBody* matchedFunctionBody;

                if (localSourceId == CurrentSourceId)  // caller and callee in same file
                {LOGMEIN("DynamicProfileInfo.cpp] 700\n");
                    matchedFunctionBody = functionBody->GetUtf8SourceInfo()->FindFunction(localFunctionId);
                    if (!matchedFunctionBody)
                    {LOGMEIN("DynamicProfileInfo.cpp] 703\n");
                        return false;
                    }
                    functionBodyArray[i] = matchedFunctionBody;
                }
                else if (localSourceId == NoSourceId || localSourceId == InvalidSourceId)
                {LOGMEIN("DynamicProfileInfo.cpp] 709\n");
                    return false;
                }
                else
                {
                    // For call across files find the function from the right source
                    typedef JsUtil::List<RecyclerWeakReference<Utf8SourceInfo>*, Recycler, false, Js::FreeListedRemovePolicy> SourceList;
                    SourceList * sourceList = functionBody->GetScriptContext()->GetSourceList();
                    bool found = false;
                    for (int j = 0; j < sourceList->Count() && !found; j++)
                    {LOGMEIN("DynamicProfileInfo.cpp] 719\n");
                        if (sourceList->IsItemValid(j))
                        {LOGMEIN("DynamicProfileInfo.cpp] 721\n");
                            Utf8SourceInfo *srcInfo = sourceList->Item(j)->Get();
                            if (srcInfo && srcInfo->GetHostSourceContext() == localSourceId)
                            {LOGMEIN("DynamicProfileInfo.cpp] 724\n");
                                matchedFunctionBody = srcInfo->FindFunction(localFunctionId);
                                if (!matchedFunctionBody)
                                {LOGMEIN("DynamicProfileInfo.cpp] 727\n");
                                    return false;
                                }
                                functionBodyArray[i] = matchedFunctionBody;
                                found = true;
                            }
                        }
                    }
                    if (!found)
                    {LOGMEIN("DynamicProfileInfo.cpp] 736\n");
                        return false;
                    }
                }
            }
            return true;
        }
        return false;
    }

    bool DynamicProfileInfo::HasCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId)
    {LOGMEIN("DynamicProfileInfo.cpp] 747\n");
        Assert(functionBody);
        const auto callSiteCount = functionBody->GetProfiledCallSiteCount();
        Assert(callSiteId < callSiteCount);
        Assert(HasCallSiteInfo(functionBody));

        if (callSiteInfo[callSiteId].isPolymorphic)
        {LOGMEIN("DynamicProfileInfo.cpp] 754\n");
            return true;
        }
        return callSiteInfo[callSiteId].u.functionData.sourceId != NoSourceId;
    }

    FunctionInfo * DynamicProfileInfo::GetCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, bool *isConstructorCall, bool *isPolymorphicCall)
    {LOGMEIN("DynamicProfileInfo.cpp] 761\n");
        Assert(functionBody);
        const auto callSiteCount = functionBody->GetProfiledCallSiteCount();
        Assert(callSiteId < callSiteCount);
        Assert(HasCallSiteInfo(functionBody));

        *isConstructorCall = callSiteInfo[callSiteId].isConstructorCall;
        if (callSiteInfo[callSiteId].dontInline)
        {LOGMEIN("DynamicProfileInfo.cpp] 769\n");
            return nullptr;
        }
        if (!callSiteInfo[callSiteId].isPolymorphic)
        {LOGMEIN("DynamicProfileInfo.cpp] 773\n");
            Js::SourceId sourceId = callSiteInfo[callSiteId].u.functionData.sourceId;
            Js::LocalFunctionId functionId = callSiteInfo[callSiteId].u.functionData.functionId;
            if (sourceId == BuiltInSourceId)
            {LOGMEIN("DynamicProfileInfo.cpp] 777\n");
                return JavascriptBuiltInFunction::GetFunctionInfo(functionId);
            }

            if (sourceId == CurrentSourceId) // caller and callee in same file
            {LOGMEIN("DynamicProfileInfo.cpp] 782\n");
                FunctionProxy *inlineeProxy = functionBody->GetUtf8SourceInfo()->FindFunction(functionId);
                return inlineeProxy ? inlineeProxy->GetFunctionInfo() : nullptr;
            }

            if (sourceId != NoSourceId && sourceId != InvalidSourceId)
            {LOGMEIN("DynamicProfileInfo.cpp] 788\n");
                // For call across files find the function from the right source
                JsUtil::List<RecyclerWeakReference<Utf8SourceInfo>*, Recycler, false, Js::FreeListedRemovePolicy> * sourceList = functionBody->GetScriptContext()->GetSourceList();
                for (int i = 0; i < sourceList->Count(); i++)
                {LOGMEIN("DynamicProfileInfo.cpp] 792\n");
                    if (sourceList->IsItemValid(i))
                    {LOGMEIN("DynamicProfileInfo.cpp] 794\n");
                        Utf8SourceInfo *srcInfo = sourceList->Item(i)->Get();
                        if (srcInfo && srcInfo->GetHostSourceContext() == sourceId)
                        {LOGMEIN("DynamicProfileInfo.cpp] 797\n");
                            FunctionProxy *inlineeProxy = srcInfo->FindFunction(functionId);
                            return inlineeProxy ? inlineeProxy->GetFunctionInfo() : nullptr;
                        }
                    }
                }
            }
        }
        else
        {
            *isPolymorphicCall = true;
        }
        return nullptr;
    }

    uint DynamicProfileInfo::GetLdFldCacheIndexFromCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId)
    {LOGMEIN("DynamicProfileInfo.cpp] 813\n");
        Assert(functionBody);
        const auto callSiteCount = functionBody->GetProfiledCallSiteCount();
        Assert(callSiteId < callSiteCount);
        Assert(HasCallSiteInfo(functionBody));

        return callSiteInfo[callSiteId].ldFldInlineCacheId;
    }

    void DynamicProfileInfo::RecordElementLoad(FunctionBody* functionBody, ProfileId ldElemId, const LdElemInfo& info)
    {LOGMEIN("DynamicProfileInfo.cpp] 823\n");
        Assert(ldElemId < functionBody->GetProfiledLdElemCount());
        Assert(info.WasProfiled());

        ldElemInfo[ldElemId].Merge(info);
    }

    void DynamicProfileInfo::RecordElementLoadAsProfiled(FunctionBody *const functionBody, const ProfileId ldElemId)
    {LOGMEIN("DynamicProfileInfo.cpp] 831\n");
        Assert(ldElemId < functionBody->GetProfiledLdElemCount());
        ldElemInfo[ldElemId].wasProfiled = true;
    }

    void DynamicProfileInfo::RecordElementStore(FunctionBody* functionBody, ProfileId stElemId, const StElemInfo& info)
    {LOGMEIN("DynamicProfileInfo.cpp] 837\n");
        Assert(stElemId < functionBody->GetProfiledStElemCount());
        Assert(info.WasProfiled());

        stElemInfo[stElemId].Merge(info);
    }

    void DynamicProfileInfo::RecordElementStoreAsProfiled(FunctionBody *const functionBody, const ProfileId stElemId)
    {LOGMEIN("DynamicProfileInfo.cpp] 845\n");
        Assert(stElemId < functionBody->GetProfiledStElemCount());
        stElemInfo[stElemId].wasProfiled = true;
    }

    ArrayCallSiteInfo * DynamicProfileInfo::GetArrayCallSiteInfo(FunctionBody *functionBody, ProfileId index) const
    {LOGMEIN("DynamicProfileInfo.cpp] 851\n");
        Assert(index < functionBody->GetProfiledArrayCallSiteCount());
        return &arrayCallSiteInfo[index];
    }

    void DynamicProfileInfo::RecordFieldAccess(FunctionBody* functionBody, uint fieldAccessId, Var object, FldInfoFlags flags)
    {LOGMEIN("DynamicProfileInfo.cpp] 857\n");
        Assert(fieldAccessId < functionBody->GetProfiledFldCount());
        FldInfoFlags oldFlags = fldInfo[fieldAccessId].flags;
        if (object) // if not provided, the saved value type is not changed
        {LOGMEIN("DynamicProfileInfo.cpp] 861\n");
            fldInfo[fieldAccessId].valueType = fldInfo[fieldAccessId].valueType.Merge(object);
        }
        const auto mergedFlags = MergeFldInfoFlags(oldFlags, flags);
        fldInfo[fieldAccessId].flags = mergedFlags;
        if (flags & FldInfo_Polymorphic)
        {LOGMEIN("DynamicProfileInfo.cpp] 867\n");
            bits.hasPolymorphicFldAccess = true;
            if (!(oldFlags & FldInfo_Polymorphic))
            {LOGMEIN("DynamicProfileInfo.cpp] 870\n");
                this->SetHasNewPolyFieldAccess(functionBody);
            }
            if (fldInfo[fieldAccessId].polymorphicInlineCacheUtilization < (PolymorphicInlineCacheUtilizationMaxValue - PolymorphicInlineCacheUtilizationIncrement))
            {LOGMEIN("DynamicProfileInfo.cpp] 874\n");
                fldInfo[fieldAccessId].polymorphicInlineCacheUtilization += PolymorphicInlineCacheUtilizationIncrement;
            }
            else
            {
                fldInfo[fieldAccessId].polymorphicInlineCacheUtilization = PolymorphicInlineCacheUtilizationMaxValue;
            }
        }
        else if (flags != FldInfo_NoInfo &&
            fldInfo[fieldAccessId].polymorphicInlineCacheUtilization != PolymorphicInlineCacheUtilizationMaxValue)
        {LOGMEIN("DynamicProfileInfo.cpp] 884\n");
            if (fldInfo[fieldAccessId].polymorphicInlineCacheUtilization > (PolymorphicInlineCacheUtilizationMinValue + PolymorphicInlineCacheUtilizationDecrement))
            {LOGMEIN("DynamicProfileInfo.cpp] 886\n");
                fldInfo[fieldAccessId].polymorphicInlineCacheUtilization -= PolymorphicInlineCacheUtilizationDecrement;
            }
            else
            {
                fldInfo[fieldAccessId].polymorphicInlineCacheUtilization = PolymorphicInlineCacheUtilizationMinValue;
            }
        }
    }

    void DynamicProfileInfo::RecordDivideResultType(FunctionBody* body, ProfileId divideId, Var object)
    {LOGMEIN("DynamicProfileInfo.cpp] 897\n");
        Assert(divideId < body->GetProfiledDivOrRemCount());
        divideTypeInfo[divideId] = divideTypeInfo[divideId].Merge(object);
    }

    // We are overloading the value types to store whether it is a mod by power of 2.
    // TaggedInt:
    void DynamicProfileInfo::RecordModulusOpType(FunctionBody* body,
                                 ProfileId profileId, bool isModByPowerOf2)
    {LOGMEIN("DynamicProfileInfo.cpp] 906\n");
        Assert(profileId < body->GetProfiledDivOrRemCount());
        /* allow one op of the modulus to be optimized - anyway */
        if (divideTypeInfo[profileId].IsUninitialized())
        {LOGMEIN("DynamicProfileInfo.cpp] 910\n");
            divideTypeInfo[profileId] = ValueType::GetInt(true);
        }
        else
        {
            if (isModByPowerOf2)
            {LOGMEIN("DynamicProfileInfo.cpp] 916\n");
                divideTypeInfo[profileId] = divideTypeInfo[profileId]
                                           .Merge(ValueType::GetInt(true));
            }
            else
            {
                divideTypeInfo[profileId] = divideTypeInfo[profileId]
                                           .Merge(ValueType::Float);
            }
        }
    }

    bool DynamicProfileInfo::IsModulusOpByPowerOf2(FunctionBody* body, ProfileId profileId) const
    {LOGMEIN("DynamicProfileInfo.cpp] 929\n");
        Assert(profileId < body->GetProfiledDivOrRemCount());
        return divideTypeInfo[profileId].IsLikelyTaggedInt();
    }

    ValueType DynamicProfileInfo::GetDivideResultType(FunctionBody* body, ProfileId divideId) const
    {LOGMEIN("DynamicProfileInfo.cpp] 935\n");
        Assert(divideId < body->GetProfiledDivOrRemCount());
        return divideTypeInfo[divideId];
    }

    void DynamicProfileInfo::RecordSwitchType(FunctionBody* body, ProfileId switchId, Var object)
    {LOGMEIN("DynamicProfileInfo.cpp] 941\n");
        Assert(switchId < body->GetProfiledSwitchCount());
        switchTypeInfo[switchId] = switchTypeInfo[switchId].Merge(object);
    }

    ValueType DynamicProfileInfo::GetSwitchType(FunctionBody* body, ProfileId switchId) const
    {LOGMEIN("DynamicProfileInfo.cpp] 947\n");
        Assert(switchId < body->GetProfiledSwitchCount());
        return switchTypeInfo[switchId];
    }

    void DynamicProfileInfo::SetHasNewPolyFieldAccess(FunctionBody *functionBody)
    {LOGMEIN("DynamicProfileInfo.cpp] 953\n");
        this->polymorphicCacheState = functionBody->GetScriptContext()->GetThreadContext()->GetNextPolymorphicCacheState();

        PHASE_PRINT_TRACE(
            Js::ObjTypeSpecPhase, functionBody,
            _u("New profile cache state: %d\n"), this->polymorphicCacheState);
    }

    void DynamicProfileInfo::RecordPolymorphicFieldAccess(FunctionBody* functionBody, uint fieldAccessId)
    {LOGMEIN("DynamicProfileInfo.cpp] 962\n");
        this->RecordFieldAccess(functionBody, fieldAccessId, nullptr, FldInfo_Polymorphic);
    }

    void DynamicProfileInfo::RecordSlotLoad(FunctionBody* functionBody, ProfileId slotLoadId, Var object)
    {LOGMEIN("DynamicProfileInfo.cpp] 967\n");
        Assert(slotLoadId < functionBody->GetProfiledSlotCount());
        slotInfo[slotLoadId] = slotInfo[slotLoadId].Merge(object);
    }

    FldInfoFlags DynamicProfileInfo::MergeFldInfoFlags(FldInfoFlags oldFlags, FldInfoFlags newFlags)
    {LOGMEIN("DynamicProfileInfo.cpp] 973\n");
        return static_cast<FldInfoFlags>(oldFlags | newFlags);
    }

    void DynamicProfileInfo::RecordParameterInfo(FunctionBody *functionBody, ArgSlot index, Var object)
    {LOGMEIN("DynamicProfileInfo.cpp] 978\n");
        Assert(this->parameterInfo != nullptr);
        Assert(index < functionBody->GetProfiledInParamsCount());
        parameterInfo[index] = parameterInfo[index].Merge(object);
    }

    ValueType DynamicProfileInfo::GetParameterInfo(FunctionBody* functionBody, ArgSlot index) const
    {LOGMEIN("DynamicProfileInfo.cpp] 985\n");
        Assert(this->parameterInfo != nullptr);
        Assert(index < functionBody->GetProfiledInParamsCount());
        return parameterInfo[index];
    }

    void DynamicProfileInfo::RecordReturnTypeOnCallSiteInfo(FunctionBody* functionBody, ProfileId callSiteId, Var object)
    {LOGMEIN("DynamicProfileInfo.cpp] 992\n");
        Assert(callSiteId < functionBody->GetProfiledCallSiteCount());
        this->callSiteInfo[callSiteId].returnType = this->callSiteInfo[callSiteId].returnType.Merge(object);
    }

    void DynamicProfileInfo::RecordReturnType(FunctionBody* functionBody, ProfileId callSiteId, Var object)
    {LOGMEIN("DynamicProfileInfo.cpp] 998\n");
        Assert(callSiteId < functionBody->GetProfiledReturnTypeCount());
        this->returnTypeInfo[callSiteId] = this->returnTypeInfo[callSiteId].Merge(object);
    }

    ValueType DynamicProfileInfo::GetReturnType(FunctionBody* functionBody, Js::OpCode opcode, ProfileId callSiteId) const
    {LOGMEIN("DynamicProfileInfo.cpp] 1004\n");
        if (opcode < Js::OpCode::ProfiledReturnTypeCallI)
        {LOGMEIN("DynamicProfileInfo.cpp] 1006\n");
            Assert(IsProfiledCallOp(opcode));
            Assert(callSiteId < functionBody->GetProfiledCallSiteCount());
            return this->callSiteInfo[callSiteId].returnType;
        }
        Assert(IsProfiledReturnTypeOp(opcode));
        Assert(callSiteId < functionBody->GetProfiledReturnTypeCount());
        return this->returnTypeInfo[callSiteId];
    }

    void DynamicProfileInfo::RecordThisInfo(Var object, ThisType thisType)
    {LOGMEIN("DynamicProfileInfo.cpp] 1017\n");
        this->thisInfo.valueType = this->thisInfo.valueType.Merge(object);
        this->thisInfo.thisType = max(this->thisInfo.thisType, thisType);
    }

    ThisInfo DynamicProfileInfo::GetThisInfo() const
    {LOGMEIN("DynamicProfileInfo.cpp] 1023\n");
        return this->thisInfo;
    }

    void DynamicProfileInfo::RecordLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum, ImplicitCallFlags flags)
    {LOGMEIN("DynamicProfileInfo.cpp] 1028\n");
        Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(functionBody));
        Assert(loopNum < functionBody->GetLoopCount());
        this->loopImplicitCallFlags[loopNum] = (ImplicitCallFlags)(this->loopImplicitCallFlags[loopNum] | flags);
    }

    ImplicitCallFlags DynamicProfileInfo::GetLoopImplicitCallFlags(FunctionBody* functionBody, uint loopNum) const
    {LOGMEIN("DynamicProfileInfo.cpp] 1035\n");
        Assert(Js::DynamicProfileInfo::EnableImplicitCallFlags(functionBody));
        Assert(loopNum < functionBody->GetLoopCount());

        // Mask out the dispose implicit call. We would bailout on reentrant dispose,
        // but it shouldn't affect optimization.
        return (ImplicitCallFlags)(this->loopImplicitCallFlags[loopNum] & ImplicitCall_All);
    }

    void DynamicProfileInfo::RecordImplicitCallFlags(ImplicitCallFlags flags)
    {LOGMEIN("DynamicProfileInfo.cpp] 1045\n");
        this->implicitCallFlags = (ImplicitCallFlags)(this->implicitCallFlags | flags);
    }

    ImplicitCallFlags DynamicProfileInfo::GetImplicitCallFlags() const
    {LOGMEIN("DynamicProfileInfo.cpp] 1050\n");
        // Mask out the dispose implicit call. We would bailout on reentrant dispose,
        // but it shouldn't affect optimization.
        return (ImplicitCallFlags)(this->implicitCallFlags & ImplicitCall_All);
    }

    void DynamicProfileInfo::UpdateFunctionInfo(FunctionBody* functionBody, Recycler* recycler)
    {LOGMEIN("DynamicProfileInfo.cpp] 1057\n");
        Assert(this->persistsAcrossScriptContexts);

        if (!this->dynamicProfileFunctionInfo)
        {LOGMEIN("DynamicProfileInfo.cpp] 1061\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1078\n");
        // For now, we only support our local storage
#ifdef DYNAMIC_PROFILE_STORAGE
        if (!DynamicProfileStorage::IsEnabled())
        {LOGMEIN("DynamicProfileInfo.cpp] 1082\n");
            return;
        }

        if (scriptContext->GetSourceContextInfoMap() == nullptr)
        {LOGMEIN("DynamicProfileInfo.cpp] 1087\n");
            // We don't have savable code
            Assert(!scriptContext->GetProfileInfoList() || scriptContext->GetProfileInfoList()->Empty() || scriptContext->GetNoContextSourceContextInfo()->nextLocalFunctionId != 0);
            return;
        }
        DynamicProfileInfo::UpdateSourceDynamicProfileManagers(scriptContext);

        scriptContext->GetSourceContextInfoMap()->Map([&](DWORD_PTR dwHostSourceContext, SourceContextInfo * sourceContextInfo)
        {
            if (sourceContextInfo->sourceDynamicProfileManager != nullptr && sourceContextInfo->url != nullptr
                && !sourceContextInfo->IsDynamic())
            {LOGMEIN("DynamicProfileInfo.cpp] 1098\n");
                sourceContextInfo->sourceDynamicProfileManager->SaveToDynamicProfileStorage(sourceContextInfo->url);
            }
        });
#endif
    }

    bool DynamicProfileInfo::MatchFunctionBody(FunctionBody * functionBody)
    {LOGMEIN("DynamicProfileInfo.cpp] 1106\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 1121\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1136\n");
        Assert(fieldAccessId < functionBody->GetProfiledFldCount());
        return &fldInfo[fieldAccessId];
    }

    ValueType DynamicProfileInfo::GetSlotLoad(FunctionBody* functionBody, ProfileId slotLoadId) const
    {LOGMEIN("DynamicProfileInfo.cpp] 1142\n");
        Assert(slotLoadId < functionBody->GetProfiledSlotCount());
        return slotInfo[slotLoadId];
    }

    FldInfoFlags DynamicProfileInfo::FldInfoFlagsFromCacheType(CacheType cacheType)
    {LOGMEIN("DynamicProfileInfo.cpp] 1148\n");
        switch (cacheType)
        {LOGMEIN("DynamicProfileInfo.cpp] 1150\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1170\n");
        switch (slotType)
        {LOGMEIN("DynamicProfileInfo.cpp] 1172\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1186\n");
        if (count != 0)
        {LOGMEIN("DynamicProfileInfo.cpp] 1188\n");
            Output::Print(_u("    %-16s(%2d):"), name, count);
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1191\n");
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:"), i);
                if (!callSiteInfo[i].isPolymorphic)
                {LOGMEIN("DynamicProfileInfo.cpp] 1195\n");
                    switch (callSiteInfo[i].u.functionData.sourceId)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1197\n");
                    case NoSourceId:
                        Output::Print(_u(" ????"));
                        break;

                    case BuiltInSourceId:
                        Output::Print(_u(" b%03d"), callSiteInfo[i].u.functionData.functionId);
                        break;

                    case InvalidSourceId:
                        if (callSiteInfo[i].u.functionData.functionId == CallSiteMixed)
                        {LOGMEIN("DynamicProfileInfo.cpp] 1208\n");
                            Output::Print(_u("  mix"));
                        }
                        else if (callSiteInfo[i].u.functionData.functionId == CallSiteCrossContext)
                        {LOGMEIN("DynamicProfileInfo.cpp] 1212\n");
                            Output::Print(_u("    x"));
                        }
                        else if (callSiteInfo[i].u.functionData.functionId == CallSiteNonFunction)
                        {LOGMEIN("DynamicProfileInfo.cpp] 1216\n");
                            Output::Print(_u("  !fn"));
                        }
                        else
                        {
                            Assert(false);
                        }
                        break;

                    default:
                        Output::Print(_u(" %4d:%4d"), callSiteInfo[i].u.functionData.sourceId, callSiteInfo[i].u.functionData.functionId);
                        break;
                    };
                }
                else
                {
                    Output::Print(_u(" poly"));
                    for (int j = 0; j < DynamicProfileInfo::maxPolymorphicInliningSize; j++)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1234\n");
                        if (callSiteInfo[i].u.polymorphicCallSiteInfo->functionIds[j] != CallSiteNoInfo)
                        {LOGMEIN("DynamicProfileInfo.cpp] 1236\n");
                            Output::Print(_u(" %4d:%4d"), callSiteInfo[i].u.polymorphicCallSiteInfo->sourceIds[j], callSiteInfo[i].u.polymorphicCallSiteInfo->functionIds[j]);
                        }
                    }
                }
            }
            Output::Print(_u("\n"));

            Output::Print(_u("    %-16s(%2d):"), _u("Callsite RetType"), count);
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1246\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1258\n");
        if (count != 0)
        {LOGMEIN("DynamicProfileInfo.cpp] 1260\n");
            Output::Print(_u("    %-16s(%2d):"), name, count);
            Output::Print(_u("\n"));
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1264\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1276\n");
        if (count != 0)
        {LOGMEIN("DynamicProfileInfo.cpp] 1278\n");
            Output::Print(_u("    %-16s(%2d):"), name, count);
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1281\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1293\n");
        if (count != 0)
        {LOGMEIN("DynamicProfileInfo.cpp] 1295\n");
            Output::Print(_u("    %-16s(%2d):"), name, count);
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1298\n");
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:%-4d"), i, value[i]);
            }
            Output::Print(_u("\n"));
        }
    }

    char16 const * DynamicProfileInfo::GetImplicitCallFlagsString(ImplicitCallFlags flags)
    {LOGMEIN("DynamicProfileInfo.cpp] 1307\n");
        // Mask out the dispose implicit call. We would bailout on reentrant dispose,
        // but it shouldn't affect optimization
        flags = (ImplicitCallFlags)(flags & ImplicitCall_All);
        return flags == ImplicitCall_HasNoInfo ? _u("???") : flags == ImplicitCall_None ? _u("no") : _u("yes");
    }

    void DynamicProfileInfo::DumpProfiledValue(char16 const * name, ImplicitCallFlags * loopImplicitCallFlags, uint count)
    {LOGMEIN("DynamicProfileInfo.cpp] 1315\n");
        if (count != 0)
        {LOGMEIN("DynamicProfileInfo.cpp] 1317\n");
            Output::Print(_u("    %-16s(%2d):"), name, count);
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1320\n");
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:%-4s"), i, GetImplicitCallFlagsString(loopImplicitCallFlags[i]));
            }
            Output::Print(_u("\n"));
        }
    }

    bool DynamicProfileInfo::IsProfiledCallOp(OpCode op)
    {LOGMEIN("DynamicProfileInfo.cpp] 1329\n");
        return Js::OpCodeUtil::IsProfiledCallOp(op) || Js::OpCodeUtil::IsProfiledCallOpWithICIndex(op) || Js::OpCodeUtil::IsProfiledConstructorCall(op);
    }

    bool DynamicProfileInfo::IsProfiledReturnTypeOp(OpCode op)
    {LOGMEIN("DynamicProfileInfo.cpp] 1334\n");
        return Js::OpCodeUtil::IsProfiledReturnTypeCallOp(op);
    }

    template<class TData, class FGetValueType>
    void DynamicProfileInfo::DumpProfiledValuesGroupedByValue(
        const char16 *const name,
        const TData *const data,
        const uint count,
        const FGetValueType GetValueType,
        ArenaAllocator *const dynamicProfileInfoAllocator)
    {LOGMEIN("DynamicProfileInfo.cpp] 1345\n");
        JsUtil::BaseDictionary<ValueType, bool, ArenaAllocator> uniqueValueTypes(dynamicProfileInfoAllocator);
        for (uint i = 0; i < count; i++)
        {LOGMEIN("DynamicProfileInfo.cpp] 1348\n");
            const ValueType valueType(GetValueType(data, i));
            if (!valueType.IsUninitialized())
            {LOGMEIN("DynamicProfileInfo.cpp] 1351\n");
                uniqueValueTypes.Item(valueType, false);
            }
        }
        uniqueValueTypes.Map([&](const ValueType groupValueType, const bool)
        {
            bool header = true;
            uint lastTempFld = (uint)-1;
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1360\n");
                const ValueType valueType(GetValueType(data, i));
                if (valueType == groupValueType)
                {LOGMEIN("DynamicProfileInfo.cpp] 1363\n");
                    if (lastTempFld == (uint)-1)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1365\n");
                        if (header)
                        {LOGMEIN("DynamicProfileInfo.cpp] 1367\n");
                            char valueTypeStr[VALUE_TYPE_MAX_STRING_SIZE];
                            valueType.ToString(valueTypeStr);
                            Output::Print(_u("    %s %S"), name, valueTypeStr);
                            Output::SkipToColumn(24);
                            Output::Print(_u(": %d"), i);
                        }
                        else
                        {
                            Output::Print(_u(", %d"), i);
                        }
                        header = false;
                        lastTempFld = i;
                    }
                }
                else
                {
                    if (lastTempFld != (uint)-1)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1385\n");
                        if (lastTempFld != i - 1)
                        {LOGMEIN("DynamicProfileInfo.cpp] 1387\n");
                            Output::Print(_u("-%d"), i - 1);
                        }
                        lastTempFld = (uint)-1;
                    }
                }
            }
            if (lastTempFld != (uint)-1 && lastTempFld != count - 1)
            {LOGMEIN("DynamicProfileInfo.cpp] 1395\n");
                Output::Print(_u("-%d\n"), count - 1);
            }
            else if (!header)
            {LOGMEIN("DynamicProfileInfo.cpp] 1399\n");
                Output::Print(_u("\n"));
            }
        });
    }

    void DynamicProfileInfo::DumpFldInfoFlags(char16 const * name, FldInfo * fldInfo, uint count, FldInfoFlags value, char16 const * valueName)
    {LOGMEIN("DynamicProfileInfo.cpp] 1406\n");
        bool header = true;
        uint lastTempFld = (uint)-1;
        for (uint i = 0; i < count; i++)
        {LOGMEIN("DynamicProfileInfo.cpp] 1410\n");
            if (fldInfo[i].flags & value)
            {LOGMEIN("DynamicProfileInfo.cpp] 1412\n");
                if (lastTempFld == (uint)-1)
                {LOGMEIN("DynamicProfileInfo.cpp] 1414\n");
                    if (header)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1416\n");
                        Output::Print(_u("    %s %s"), name, valueName);
                        Output::SkipToColumn(24);
                        Output::Print(_u(": %d"), i);
                    }
                    else
                    {
                        Output::Print(_u(", %d"), i);
                    }
                    header = false;
                    lastTempFld = i;
                }
            }
            else
            {
                if (lastTempFld != (uint)-1)
                {LOGMEIN("DynamicProfileInfo.cpp] 1432\n");
                    if (lastTempFld != i - 1)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1434\n");
                        Output::Print(_u("-%d"), i - 1);
                    }
                    lastTempFld = (uint)-1;
                }
            }
        }
        if (lastTempFld != (uint)-1 && lastTempFld != count - 1)
        {LOGMEIN("DynamicProfileInfo.cpp] 1442\n");
            Output::Print(_u("-%d\n"), count - 1);
        }
        else if (!header)
        {LOGMEIN("DynamicProfileInfo.cpp] 1446\n");
            Output::Print(_u("\n"));
        }
    }

    void DynamicProfileInfo::DumpLoopInfo(FunctionBody *fbody)
    {LOGMEIN("DynamicProfileInfo.cpp] 1452\n");
        if (fbody->DoJITLoopBody())
        {LOGMEIN("DynamicProfileInfo.cpp] 1454\n");
            uint count = fbody->GetLoopCount();
            Output::Print(_u("    %-16s(%2d):"), _u("Loops"), count);
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1458\n");
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:%-4d"), i, fbody->GetLoopHeader(i)->interpretCount);
            }
            Output::Print(_u("\n"));

            Output::Print(_u("    %-16s(%2d):"), _u("Loops JIT"), count);
            for (uint i = 0; i < count; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 1466\n");
                Output::Print(i != 0 && (i % 10) == 0 ? _u("\n                          ") : _u(" "));
                Output::Print(_u("%2d:%-4d"), i, fbody->GetLoopHeader(i)->nativeCount);
            }
            Output::Print(_u("\n"));
        }
    }

    void DynamicProfileInfo::Dump(FunctionBody* functionBody, ArenaAllocator * dynamicProfileInfoAllocator)
    {LOGMEIN("DynamicProfileInfo.cpp] 1475\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 1487\n");
            DumpProfiledValue(_u("Div result type"), this->divideTypeInfo, functionBody->GetProfiledDivOrRemCount());
            DumpProfiledValue(_u("Switch opt type"), this->switchTypeInfo, functionBody->GetProfiledSwitchCount());
            DumpProfiledValue(_u("Param type"), this->parameterInfo, paramcount);
            DumpProfiledValue(_u("Callsite"), this->callSiteInfo, functionBody->GetProfiledCallSiteCount());
            DumpProfiledValue(_u("ArrayCallSite"), this->arrayCallSiteInfo, functionBody->GetProfiledArrayCallSiteCount());
            DumpProfiledValue(_u("Return type"), this->returnTypeInfo, functionBody->GetProfiledReturnTypeCount());
            if (dynamicProfileInfoAllocator)
            {LOGMEIN("DynamicProfileInfo.cpp] 1495\n");
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
            {LOGMEIN("DynamicProfileInfo.cpp] 1524\n");
                DumpProfiledValue(_u("Loop Imp Call"), this->loopImplicitCallFlags, functionBody->GetLoopCount());
            }
            if (functionBody->GetLoopCount())
            {LOGMEIN("DynamicProfileInfo.cpp] 1528\n");
                Output::Print(_u("    Loop Flags:\n"));
                for (uint i = 0; i < functionBody->GetLoopCount(); ++i)
                {LOGMEIN("DynamicProfileInfo.cpp] 1531\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1621\n");
        AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_DisableCheck);
        if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
        {LOGMEIN("DynamicProfileInfo.cpp] 1624\n");
            FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, profileInfoList)
            {LOGMEIN("DynamicProfileInfo.cpp] 1626\n");
                if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase, info->GetFunctionBody()->GetSourceContextId(), info->GetFunctionBody()->GetLocalFunctionId()))
                {LOGMEIN("DynamicProfileInfo.cpp] 1628\n");
                    info->Dump(info->GetFunctionBody(), dynamicProfileInfoAllocator);
                }
            }
            NEXT_SLISTBASE_ENTRY;
        }

        if (Configuration::Global.flags.Dump.IsEnabled(JITLoopBodyPhase) && !Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
        {LOGMEIN("DynamicProfileInfo.cpp] 1636\n");
            FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, profileInfoList)
            {LOGMEIN("DynamicProfileInfo.cpp] 1638\n");
                if (info->functionBody->GetLoopCount() > 0)
                {LOGMEIN("DynamicProfileInfo.cpp] 1640\n");
                    info->functionBody->DumpFunctionId(true);
                    Output::Print(_u(": %-20s\n"), info->functionBody->GetDisplayName());
                    DumpLoopInfo(info->functionBody);
                }
            }
            NEXT_SLISTBASE_ENTRY;
        }

        if (PHASE_STATS1(DynamicProfilePhase))
        {LOGMEIN("DynamicProfileInfo.cpp] 1650\n");
            uint estimatedSavedBytes = sizeof(uint); // count of functions
            uint functionSaved = 0;
            uint loopSaved = 0;
            uint callSiteSaved = 0;
            uint elementAccessSaved = 0;
            uint fldAccessSaved = 0;

            FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, profileInfoList)
            {LOGMEIN("DynamicProfileInfo.cpp] 1659\n");
                bool hasHotLoop = false;
                if (info->functionBody->DoJITLoopBody())
                {LOGMEIN("DynamicProfileInfo.cpp] 1662\n");
                    for (uint i = 0; i < info->functionBody->GetLoopCount(); i++)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1664\n");
                        if (info->functionBody->GetLoopHeader(i)->interpretCount >= 10)
                        {LOGMEIN("DynamicProfileInfo.cpp] 1666\n");
                            hasHotLoop = true;
                            break;
                        }
                    }
                }

                if (hasHotLoop || info->functionBody->GetInterpretedCount() >= 10)
                {LOGMEIN("DynamicProfileInfo.cpp] 1674\n");
                    functionSaved++;
                    loopSaved += info->functionBody->GetLoopCount();

                    estimatedSavedBytes += sizeof(uint) * 5; // function number, loop count, call site count, local array, temp array
                    estimatedSavedBytes += (info->functionBody->GetLoopCount() + 7) / 8; // hot loop bit vector
                    estimatedSavedBytes += (info->functionBody->GetProfiledCallSiteCount() + 7) / 8; // call site bit vector
                    // call site function number
                    for (ProfileId i = 0; i < info->functionBody->GetProfiledCallSiteCount(); i++)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1683\n");
                        // TODO poly
                        if ((info->callSiteInfo[i].u.functionData.sourceId != NoSourceId) && (info->callSiteInfo[i].u.functionData.sourceId != InvalidSourceId))
                        {LOGMEIN("DynamicProfileInfo.cpp] 1686\n");
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
            {LOGMEIN("DynamicProfileInfo.cpp] 1700\n");
                Output::Print(_u("Estimated save size (Memory used): %6d (%6d): %3d %3d %4d %4d %3d\n"),
                    estimatedSavedBytes, dynamicProfileInfoAllocator->Size(), functionSaved, loopSaved, callSiteSaved,
                    elementAccessSaved, fldAccessSaved);
            }
        }
    }

    void DynamicProfileInfo::DumpScriptContext(ScriptContext * scriptContext)
    {LOGMEIN("DynamicProfileInfo.cpp] 1709\n");
        if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase))
        {LOGMEIN("DynamicProfileInfo.cpp] 1711\n");
            Output::Print(_u("Sources:\n"));
            if (scriptContext->GetSourceContextInfoMap() != nullptr)
            {LOGMEIN("DynamicProfileInfo.cpp] 1714\n");
                scriptContext->GetSourceContextInfoMap()->Map([&](DWORD_PTR dwHostSourceContext, SourceContextInfo * sourceContextInfo)
                {
                    if (sourceContextInfo->sourceContextId != Js::Constants::NoSourceContext)
                    {LOGMEIN("DynamicProfileInfo.cpp] 1718\n");
                        Output::Print(_u("%2d: %s (Function count: %d)\n"), sourceContextInfo->sourceContextId, sourceContextInfo->url, sourceContextInfo->nextLocalFunctionId);
                    }
                });
            }

            if (scriptContext->GetDynamicSourceContextInfoMap() != nullptr)
            {LOGMEIN("DynamicProfileInfo.cpp] 1725\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 1740\n");
        if (Configuration::Global.flags.Dump.IsEnabled(DynamicProfilePhase, info->GetFunctionBody()->GetSourceContextId(), info->GetFunctionBody()->GetLocalFunctionId()))
        {LOGMEIN("DynamicProfileInfo.cpp] 1742\n");
            Output::Print(_u("Saving:"));
            info->Dump(info->GetFunctionBody());
        }
    }
#endif

    template <typename T>
    bool DynamicProfileInfo::Serialize(T * writer)
    {LOGMEIN("DynamicProfileInfo.cpp] 1751\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 1786\n");
            return false;
        }
        return true;
    }

    template <typename T>
    DynamicProfileInfo * DynamicProfileInfo::Deserialize(T * reader, Recycler* recycler, Js::LocalFunctionId * functionId)
    {LOGMEIN("DynamicProfileInfo.cpp] 1794\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 1824\n");
            AUTO_NESTED_HANDLED_EXCEPTION_TYPE(ExceptionType_OutOfMemory);

            if (!reader->Read(functionId))
            {LOGMEIN("DynamicProfileInfo.cpp] 1828\n");
                return nullptr;
            }

            if (!reader->Read(&paramInfoCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1833\n");
                return nullptr;
            }

            if (paramInfoCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1838\n");
                paramInfo = RecyclerNewArrayLeaf(recycler, ValueType, paramInfoCount);
                if (!reader->ReadArray(paramInfo, paramInfoCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1841\n");
                    goto Error;
                }
            }

            if (!reader->Read(&ldElemInfoCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1847\n");
                goto Error;
            }

            if (ldElemInfoCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1852\n");
                ldElemInfo = RecyclerNewArrayLeaf(recycler, LdElemInfo, ldElemInfoCount);
                if (!reader->ReadArray(ldElemInfo, ldElemInfoCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1855\n");
                    goto Error;
                }
            }

            if (!reader->Read(&stElemInfoCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1861\n");
                goto Error;
            }

            if (stElemInfoCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1866\n");
                stElemInfo = RecyclerNewArrayLeaf(recycler, StElemInfo, stElemInfoCount);
                if (!reader->ReadArray(stElemInfo, stElemInfoCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1869\n");
                    goto Error;
                }
            }

            if (!reader->Read(&arrayCallSiteCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1875\n");
                goto Error;
            }

            if (arrayCallSiteCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1880\n");
                arrayCallSiteInfo = RecyclerNewArrayLeaf(recycler, ArrayCallSiteInfo, arrayCallSiteCount);
                if (!reader->ReadArray(arrayCallSiteInfo, arrayCallSiteCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1883\n");
                    goto Error;
                }
            }

            if (!reader->Read(&fldInfoCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1889\n");
                goto Error;
            }

            if (fldInfoCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1894\n");
                fldInfo = RecyclerNewArrayLeaf(recycler, FldInfo, fldInfoCount);
                if (!reader->ReadArray(fldInfo, fldInfoCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1897\n");
                    goto Error;
                }
            }

            if (!reader->Read(&slotInfoCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1903\n");
                goto Error;
            }

            if (slotInfoCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1908\n");
                slotInfo = RecyclerNewArrayLeaf(recycler, ValueType, slotInfoCount);
                if (!reader->ReadArray(slotInfo, slotInfoCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1911\n");
                    goto Error;
                }
            }

            if (!reader->Read(&callSiteInfoCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1917\n");
                goto Error;
            }

            if (callSiteInfoCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1922\n");
                // CallSiteInfo contains pointer "polymorphicCallSiteInfo", but
                // we explicitly save that pointer in FunctionBody. Safe to
                // allocate CallSiteInfo[] as Leaf here.
                callSiteInfo = RecyclerNewArrayLeaf(recycler, CallSiteInfo, callSiteInfoCount);
                if (!reader->ReadArray(callSiteInfo, callSiteInfoCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1928\n");
                    goto Error;
                }
            }

            if (!reader->Read(&divCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1934\n");
                goto Error;
            }

            if (divCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1939\n");
                divTypeInfo = RecyclerNewArrayLeaf(recycler, ValueType, divCount);
                if (!reader->ReadArray(divTypeInfo, divCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1942\n");
                    goto Error;
                }
            }

            if (!reader->Read(&switchCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1948\n");
                goto Error;
            }

            if (switchCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1953\n");
                switchTypeInfo = RecyclerNewArrayLeaf(recycler, ValueType, switchCount);
                if (!reader->ReadArray(switchTypeInfo, switchCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1956\n");
                    goto Error;
                }
            }

            if (!reader->Read(&returnTypeInfoCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1962\n");
                goto Error;
            }

            if (returnTypeInfoCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1967\n");
                returnTypeInfo = RecyclerNewArrayLeaf(recycler, ValueType, returnTypeInfoCount);
                if (!reader->ReadArray(returnTypeInfo, returnTypeInfoCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1970\n");
                    goto Error;
                }
            }

            if (!reader->Read(&loopCount))
            {LOGMEIN("DynamicProfileInfo.cpp] 1976\n");
                goto Error;
            }

            if (loopCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1981\n");
                loopImplicitCallFlags = RecyclerNewArrayLeaf(recycler, ImplicitCallFlags, loopCount);
                if (!reader->ReadArray(loopImplicitCallFlags, loopCount))
                {LOGMEIN("DynamicProfileInfo.cpp] 1984\n");
                    goto Error;
                }
            }

            if (!reader->Read(&implicitCallFlags) ||
                !reader->Read(&thisInfo) ||
                !reader->Read(&bits) ||
                !reader->Read(&recursiveInlineInfo))
            {LOGMEIN("DynamicProfileInfo.cpp] 1993\n");
                goto Error;
            }

            if (loopCount != 0)
            {LOGMEIN("DynamicProfileInfo.cpp] 1998\n");
                loopFlags = BVFixed::New(loopCount * LoopFlags::COUNT, recycler);
                if (!reader->ReadArray(loopFlags->GetData(), loopFlags->WordCount()))
                {LOGMEIN("DynamicProfileInfo.cpp] 2001\n");
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
        {LOGMEIN("DynamicProfileInfo.cpp] 2044\n");
        }

    Error:
        return nullptr;
    }

    // Explicit instantiations - to force the compiler to generate these - so they can be referenced from other compilation units.
    template DynamicProfileInfo * DynamicProfileInfo::Deserialize<BufferReader>(BufferReader*, Recycler*, Js::LocalFunctionId *);
    template bool DynamicProfileInfo::Serialize<BufferSizeCounter>(BufferSizeCounter*);
    template bool DynamicProfileInfo::Serialize<BufferWriter>(BufferWriter*);

    void DynamicProfileInfo::UpdateSourceDynamicProfileManagers(ScriptContext * scriptContext)
    {LOGMEIN("DynamicProfileInfo.cpp] 2057\n");
        // We don't clear old dynamic data here, because if a function is inlined, it will never go through the
        // EnsureDynamicProfileThunk and thus not appear in the list. We would want to keep those data as well.
        // Just save/update the data from function that has execute.

        // That means that the data will never go away, probably not a good policy if this is cached for web page in WININET.

        DynamicProfileInfoList * profileInfoList = scriptContext->GetProfileInfoList();
        FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, profileInfoList)
        {LOGMEIN("DynamicProfileInfo.cpp] 2066\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 2086\n");
        if (sz)
        {LOGMEIN("DynamicProfileInfo.cpp] 2088\n");
            charcount_t len = static_cast<charcount_t>(wcslen(sz));
            utf8char_t * tempBuffer = HeapNewArray(utf8char_t, len * 3);
            size_t cbNeeded = utf8::EncodeInto(tempBuffer, sz, len);
            fwrite(&cbNeeded, sizeof(cbNeeded), 1, file);
            fwrite(tempBuffer, sizeof(utf8char_t), cbNeeded, file);
            HeapDeleteArray(len * 3, tempBuffer);
        }
        else
        {
            charcount_t len = 0;
            fwrite(&len, sizeof(len), 1, file);
        }
    }

    template <typename T>
    void DynamicProfileInfo::WriteArray(uint count, T * arr, FILE * file)
    {
        WriteData(count, file);
        for (uint i = 0; i < count; i++)
        {LOGMEIN("DynamicProfileInfo.cpp] 2108\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 2121\n");
        WriteData(functionBody->GetSourceContextInfo()->sourceContextId, file);
        WriteData(functionBody->GetLocalFunctionId(), file);
    }

    void DynamicProfileInfo::DumpScriptContextToFile(ScriptContext * scriptContext)
    {LOGMEIN("DynamicProfileInfo.cpp] 2127\n");
        if (Configuration::Global.flags.RuntimeDataOutputFile == nullptr)
        {LOGMEIN("DynamicProfileInfo.cpp] 2129\n");
            return;
        }

        AutoCriticalSection autocs(&s_csOutput);
        FILE * file;
        if (_wfopen_s(&file, Configuration::Global.flags.RuntimeDataOutputFile, _u("ab+")) != 0 || file == nullptr)
        {LOGMEIN("DynamicProfileInfo.cpp] 2136\n");
            return;
        }

        WriteData(scriptContext->GetAllocId(), file);
        WriteData(scriptContext->GetCreateTime(), file);
        WriteData(scriptContext->GetUrl(), file);
        WriteData(scriptContext->GetSourceContextInfoMap() != nullptr ? scriptContext->GetSourceContextInfoMap()->Count() : 0, file);

        if (scriptContext->GetSourceContextInfoMap())
        {LOGMEIN("DynamicProfileInfo.cpp] 2146\n");
            scriptContext->GetSourceContextInfoMap()->Map([&](DWORD_PTR dwHostSourceContext, SourceContextInfo * sourceContextInfo)
            {
                WriteData(sourceContextInfo->sourceContextId, file);
                WriteData(sourceContextInfo->nextLocalFunctionId, file);
                WriteData(sourceContextInfo->url, file);
            });
        }

        FOREACH_SLISTBASE_ENTRY(DynamicProfileInfo * const, info, scriptContext->GetProfileInfoList())
        {LOGMEIN("DynamicProfileInfo.cpp] 2156\n");
            WriteData((byte)1, file);
            WriteData(info->functionBody, file);
            WriteData(info->functionBody->GetDisplayName(), file);
            WriteData(info->functionBody->GetInterpretedCount(), file);
            uint loopCount = info->functionBody->GetLoopCount();
            WriteData(loopCount, file);
            for (uint i = 0; i < loopCount; i++)
            {LOGMEIN("DynamicProfileInfo.cpp] 2164\n");
                if (info->functionBody->DoJITLoopBody())
                {LOGMEIN("DynamicProfileInfo.cpp] 2166\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 2188\n");
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
{LOGMEIN("DynamicProfileInfo.cpp] 2213\n");
    IR::BailOutKind kindWithoutBits = kind & ~IR::BailOutKindBits;
    return
        kindWithoutBits == IR::BailOutFailedTypeCheck ||
        kindWithoutBits == IR::BailOutFailedFixedFieldTypeCheck ||
        kindWithoutBits == IR::BailOutFailedEquivalentTypeCheck ||
        kindWithoutBits == IR::BailOutFailedEquivalentFixedFieldTypeCheck;
}

bool IR::IsEquivalentTypeCheckBailOutKind(IR::BailOutKind kind)
{LOGMEIN("DynamicProfileInfo.cpp] 2223\n");
    IR::BailOutKind kindWithoutBits = kind & ~IR::BailOutKindBits;
    return
        kindWithoutBits == IR::BailOutFailedEquivalentTypeCheck ||
        kindWithoutBits == IR::BailOutFailedEquivalentFixedFieldTypeCheck;
}

IR::BailOutKind IR::EquivalentToMonoTypeCheckBailOutKind(IR::BailOutKind kind)
{LOGMEIN("DynamicProfileInfo.cpp] 2231\n");
    switch (kind & ~IR::BailOutKindBits)
    {LOGMEIN("DynamicProfileInfo.cpp] 2233\n");
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
{LOGMEIN("DynamicProfileInfo.cpp] 2266\n");
    IR::BailOutKind kindNoBits = bailOutKind & ~IR::BailOutKindBits;
    if (kindNoBits >= IR::BailOutKindBitsStart)
    {LOGMEIN("DynamicProfileInfo.cpp] 2269\n");
        return false;
    }
    return ((bailOutKind & IR::BailOutKindBits) & ~BailOutKindValidBits[kindNoBits]) == 0;
}

// Concats into the buffer, specified by the name parameter, the name of 'bit' bailout kind, specified by the enumEntryOffsetFromBitsStart parameter.
// Returns the number of bytes printed to the buffer.
size_t ConcatBailOutKindBits(_Out_writes_bytes_(dstSizeBytes) char* dst, _In_ size_t dstSizeBytes, _In_ size_t position, _In_ uint enumEntryOffsetFromBitsStart)
{LOGMEIN("DynamicProfileInfo.cpp] 2278\n");
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
{LOGMEIN("DynamicProfileInfo.cpp] 2290\n");
    using namespace IR;

    if (!(kind & BailOutKindBits))
    {LOGMEIN("DynamicProfileInfo.cpp] 2294\n");
        return BailOutKindNames[kind];
    }

    static char name[512];
    size_t position = 0;
    const auto normalKind = kind & ~BailOutKindBits;
    if (normalKind != 0)
    {LOGMEIN("DynamicProfileInfo.cpp] 2302\n");
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
    {LOGMEIN("DynamicProfileInfo.cpp] 2314\n");
        kind ^= BailOutOnOverflow;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnMulOverflow)
    {LOGMEIN("DynamicProfileInfo.cpp] 2320\n");
        kind ^= BailOutOnMulOverflow;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnNegativeZero)
    {LOGMEIN("DynamicProfileInfo.cpp] 2326\n");
        kind ^= BailOutOnNegativeZero;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnPowIntIntOverflow)
    {LOGMEIN("DynamicProfileInfo.cpp] 2332\n");
        kind ^= BailOutOnPowIntIntOverflow;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    // BailOutOnResultConditions

    ++offset;
    if (kind & BailOutOnMissingValue)
    {LOGMEIN("DynamicProfileInfo.cpp] 2341\n");
        kind ^= BailOutOnMissingValue;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutConventionalNativeArrayAccessOnly)
    {LOGMEIN("DynamicProfileInfo.cpp] 2347\n");
        kind ^= BailOutConventionalNativeArrayAccessOnly;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutConvertedNativeArray)
    {LOGMEIN("DynamicProfileInfo.cpp] 2353\n");
        kind ^= BailOutConvertedNativeArray;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnArrayAccessHelperCall)
    {LOGMEIN("DynamicProfileInfo.cpp] 2359\n");
        kind ^= BailOutOnArrayAccessHelperCall;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnInvalidatedArrayHeadSegment)
    {LOGMEIN("DynamicProfileInfo.cpp] 2365\n");
        kind ^= BailOutOnInvalidatedArrayHeadSegment;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnInvalidatedArrayLength)
    {LOGMEIN("DynamicProfileInfo.cpp] 2371\n");
        kind ^= BailOutOnInvalidatedArrayLength;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOnStackArgsOutOfActualsRange)
    {LOGMEIN("DynamicProfileInfo.cpp] 2377\n");
        kind ^= BailOnStackArgsOutOfActualsRange;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    // BailOutForArrayBits

    ++offset;
    if (kind & BailOutForceByFlag)
    {LOGMEIN("DynamicProfileInfo.cpp] 2386\n");
        kind ^= BailOutForceByFlag;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutBreakPointInFunction)
    {LOGMEIN("DynamicProfileInfo.cpp] 2392\n");
        kind ^= BailOutBreakPointInFunction;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutStackFrameBase)
    {LOGMEIN("DynamicProfileInfo.cpp] 2398\n");
        kind ^= BailOutStackFrameBase;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutLocalValueChanged)
    {LOGMEIN("DynamicProfileInfo.cpp] 2404\n");
        kind ^= BailOutLocalValueChanged;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutExplicit)
    {LOGMEIN("DynamicProfileInfo.cpp] 2410\n");
        kind ^= BailOutExplicit;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutStep)
    {LOGMEIN("DynamicProfileInfo.cpp] 2416\n");
        kind ^= BailOutStep;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutIgnoreException)
    {LOGMEIN("DynamicProfileInfo.cpp] 2422\n");
        kind ^= BailOutIgnoreException;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    // BailOutForDebuggerBits

    ++offset;
    if (kind & BailOutOnDivByZero)
    {LOGMEIN("DynamicProfileInfo.cpp] 2431\n");
        kind ^= BailOutOnDivByZero;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    if (kind & BailOutOnDivOfMinInt)
    {LOGMEIN("DynamicProfileInfo.cpp] 2437\n");
        kind ^= BailOutOnDivOfMinInt;
        position += ConcatBailOutKindBits(name, sizeof(name), position, offset);
    }
    ++offset;
    // BailOutOnDivSrcConditions

    ++offset;
    if (kind & BailOutMarkTempObject)
    {LOGMEIN("DynamicProfileInfo.cpp] 2446\n");
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
