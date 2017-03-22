//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "Backend.h"

namespace Js
{
    void SimpleJitHelpers::ProfileParameters(void* framePtr)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 9\n");
        auto layout = JavascriptCallStackLayout::FromFramePointer(framePtr);
        FunctionBody* executeFunction = layout->functionObject->GetFunctionBody();
        const auto dynamicInfo = executeFunction->GetDynamicProfileInfo();
        ArgSlot requiredInParamCount = executeFunction->GetInParamsCount();
        AssertMsg(requiredInParamCount > 1, "This function shouldn't be called unless we're expecting to have args to profile!");

        CallInfo callInfo(layout->callInfo);
        Js::ArgumentReader args(&callInfo, layout->args);

        // Ignore the first param. 'this' is profiled as part of LdThis.
        ArgSlot paramIndex = 1;

        const auto maxValid = min((ArgSlot)args.Info.Count, requiredInParamCount);
        for (; paramIndex < maxValid; paramIndex++)
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 24\n");
            dynamicInfo->RecordParameterInfo(executeFunction, paramIndex - 1, args[paramIndex]);
        }

        // If they didn't provide enough parameters, record the fact that the rest of the params are undefined.
        if (paramIndex < requiredInParamCount)
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 30\n");
            Var varUndef = executeFunction->GetScriptContext()->GetLibrary()->GetUndefined();

            do
            {LOGMEIN("SimpleJitProfilingHelpers.cpp] 34\n");
                dynamicInfo->RecordParameterInfo(executeFunction, paramIndex - 1, varUndef);
                ++paramIndex;
            } while (paramIndex < requiredInParamCount);
        }

        // Clearing of the implicit call flags is done in jitted code
    }

    void SimpleJitHelpers::CleanImplicitCallFlags(FunctionBody* body)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 44\n");
        // Probably could inline this straight into JITd code.
        auto flags = body->GetScriptContext()->GetThreadContext()->GetImplicitCallFlags();

        body->GetDynamicProfileInfo()->RecordImplicitCallFlags(flags);
    }

    void SimpleJitHelpers::ProfileCall_DefaultInlineCacheIndex(void* framePtr, ProfileId profileId, Var retval, JavascriptFunction* callee, CallInfo info)
    {
        ProfileCall(framePtr, profileId, Constants::NoInlineCacheIndex, retval, callee, info);
    }

    void SimpleJitHelpers::ProfileCall(void* framePtr, ProfileId profileId, InlineCacheIndex inlineCacheIndex, Var retval, Var callee, CallInfo info)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 57\n");
        JavascriptFunction* caller = JavascriptCallStackLayout::FromFramePointer(framePtr)->functionObject;

        FunctionBody* callerFunctionBody = caller->GetFunctionBody();
        DynamicProfileInfo * dynamicProfileInfo = callerFunctionBody->GetDynamicProfileInfo();

        JavascriptFunction *const calleeFunction =
            JavascriptFunction::Is(callee) ? JavascriptFunction::FromVar(callee) : nullptr;
        FunctionInfo* calleeFunctionInfo = calleeFunction ? calleeFunction->GetFunctionInfo() : nullptr;

        auto const ctor = !!(info.Flags & CallFlags_New);
        dynamicProfileInfo->RecordCallSiteInfo(callerFunctionBody, profileId, calleeFunctionInfo, calleeFunction, info.Count, ctor, inlineCacheIndex);

        if (info.Flags & CallFlags_Value)
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 71\n");
            dynamicProfileInfo->RecordReturnTypeOnCallSiteInfo(callerFunctionBody, profileId, retval);
        }
    }

    void SimpleJitHelpers::ProfileReturnTypeCall(void* framePtr, ProfileId profileId, Var retval, JavascriptFunction*callee, CallInfo info)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 77\n");
        Assert(info.Flags & CallFlags_Value);
        JavascriptFunction* caller = JavascriptCallStackLayout::FromFramePointer(framePtr)->functionObject;

        FunctionBody* functionBody = caller->GetFunctionBody();
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        dynamicProfileInfo->RecordReturnType(functionBody, profileId, retval);
    }

    Var SimpleJitHelpers::ProfiledLdLen_A(FunctionBody* body, DynamicObject * instance, ProfileId profileId)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 87\n");
        FunctionBody*const functionBody = body;
        const auto profileData = functionBody->GetDynamicProfileInfo();

        LdElemInfo ldElemInfo;
        ldElemInfo.arrayType = ValueType::Uninitialized.Merge(instance);

        Var length = JavascriptOperators::OP_GetProperty(instance, PropertyIds::length, body->GetScriptContext());

        ldElemInfo.elemType = ldElemInfo.elemType.Merge(length);
        profileData->RecordElementLoad(functionBody, profileId, ldElemInfo);

        return length;
    }

    Var SimpleJitHelpers::ProfiledStrictLdThis(Var thisVar, FunctionBody* functionBody)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 103\n");
        //Adapted from InterpreterStackFrame::OP_ProfiledStrictLdThis
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        TypeId typeId = JavascriptOperators::GetTypeId(thisVar);

        if (typeId == TypeIds_ActivationObject)
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 109\n");
            thisVar = functionBody->GetScriptContext()->GetLibrary()->GetUndefined();
            dynamicProfileInfo->RecordThisInfo(thisVar, ThisType_Mapped);
            return thisVar;
        }

        dynamicProfileInfo->RecordThisInfo(thisVar, ThisType_Simple);
        return thisVar;
    }


    Var SimpleJitHelpers::ProfiledLdThis(Var thisVar, int moduleID, FunctionBody* functionBody)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 121\n");
        //Adapted from InterpreterStackFrame::OP_ProfiledLdThis
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        TypeId typeId = JavascriptOperators::GetTypeId(thisVar);

        if (JavascriptOperators::IsThisSelf(typeId))
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 127\n");
            Assert(typeId != TypeIds_GlobalObject || ((GlobalObject*)thisVar)->ToThis() == thisVar);
            Assert(typeId != TypeIds_ModuleRoot || JavascriptOperators::GetThisFromModuleRoot(thisVar) == thisVar);

            dynamicProfileInfo->RecordThisInfo(thisVar, ThisType_Simple);

            return thisVar;
        }

        thisVar = JavascriptOperators::OP_GetThis(thisVar, moduleID, functionBody->GetScriptContext());
        dynamicProfileInfo->RecordThisInfo(thisVar, ThisType_Mapped);

        return thisVar;
    }

    Var SimpleJitHelpers::ProfiledSwitch(FunctionBody* functionBody, ProfileId profileId, Var exp)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 143\n");
        functionBody->GetDynamicProfileInfo()->RecordSwitchType(functionBody, profileId, exp);
        return exp;
    }

    Var SimpleJitHelpers::ProfiledDivide(FunctionBody* functionBody, ProfileId profileId, Var aLeft, Var aRight)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 149\n");
        Var result = JavascriptMath::Divide(aLeft, aRight,functionBody->GetScriptContext());
        functionBody->GetDynamicProfileInfo()->RecordDivideResultType(functionBody, profileId, result);
        return result;
    }

    Var SimpleJitHelpers::ProfiledRemainder(FunctionBody* functionBody, ProfileId profileId, Var aLeft, Var aRight)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 156\n");
        if(TaggedInt::IsPair(aLeft, aRight))
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 158\n");
            int nLeft    = TaggedInt::ToInt32(aLeft);
            int nRight   = TaggedInt::ToInt32(aRight);

            // nLeft is positive and nRight is +2^i
            // Fast path for Power of 2 divisor
            if (nLeft > 0 && ::Math::IsPow2(nRight))
            {LOGMEIN("SimpleJitProfilingHelpers.cpp] 165\n");
                functionBody->GetDynamicProfileInfo()->RecordModulusOpType(functionBody, profileId, /*isModByPowerOf2*/ true);
                return TaggedInt::ToVarUnchecked(nLeft & (nRight - 1));
            }
        }
        functionBody->GetDynamicProfileInfo()->RecordModulusOpType(functionBody, profileId, /*isModByPowerOf2*/ false);
        return JavascriptMath::Modulus(aLeft, aRight,functionBody->GetScriptContext());
    }

    void SimpleJitHelpers::StoreArrayHelper(Var arr, uint32 index, Var value)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 175\n");
        //Adapted from InterpreterStackFrame::OP_SetArrayItemC_CI4
        JavascriptArray* array = JavascriptArray::FromAnyArray(arr);

        TypeId typeId = array->GetTypeId();
        if (typeId == TypeIds_NativeIntArray)
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 181\n");
            JavascriptArray::OP_SetNativeIntElementC(reinterpret_cast<JavascriptNativeIntArray*>(array), index, value, array->GetScriptContext());
        }
        else if (typeId == TypeIds_NativeFloatArray)
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 185\n");
            JavascriptArray::OP_SetNativeFloatElementC(reinterpret_cast<JavascriptNativeFloatArray*>(array), index, value, array->GetScriptContext());
        }
        else
        {
            array->SetArrayLiteralItem(index, value);
        }
    }

    void SimpleJitHelpers::StoreArraySegHelper(Var arr, uint32 index, Var value)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 195\n");
        //Adapted from InterpreterStackFrame::OP_SetArraySegmentItem_CI4
        SparseArraySegment<Var> * segment = (SparseArraySegment<Var> *)arr;

        Assert(segment->left == 0);
        Assert(index < segment->length);

        segment->elements[index] = value;
    }

    LoopEntryPointInfo* SimpleJitHelpers::GetScheduledEntryPoint(void* framePtr, uint loopnum)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 206\n");
        auto layout = JavascriptCallStackLayout::FromFramePointer(framePtr);
        FunctionBody* functionBody = layout->functionObject->GetFunctionBody();

        //REVIEW: Simplejit: Optimization: Don't emit this call if this is true during JIT time

        auto loopHeader = functionBody->GetLoopHeader(loopnum);
        LoopEntryPointInfo * entryPointInfo = loopHeader->GetCurrentEntryPointInfo();

        if (entryPointInfo->IsNotScheduled())
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 216\n");
            // Not scheduled yet! It will be impossible for this loop to be scheduled for a JIT unless we're the ones doing it,
            //   so there's no need to check whether the existing job is finished.
            return nullptr;
        }

        // The entry point has been scheduled, it is finished, or something else. Just return the entry point and the
        //   SimpleJitted function will check this on each iteration.
        return entryPointInfo;
    }

    bool SimpleJitHelpers::IsLoopCodeGenDone(LoopEntryPointInfo* info)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 228\n");
        // It is possible for this work item to be removed from the queue after we found out that it was at some point in the queue.
        // In that case, we must manually increment the interpretCount. Once the work item gets scheduled again, it will trigger
        // a bailout when the threshold is reached.
        if (info->IsNotScheduled())
        {LOGMEIN("SimpleJitProfilingHelpers.cpp] 233\n");
            ++info->loopHeader->interpretCount;
            return false;
        }

        return info->IsCodeGenDone();
    }

    void SimpleJitHelpers::RecordLoopImplicitCallFlags(void* framePtr, uint loopNum, int restoreCallFlags)
    {LOGMEIN("SimpleJitProfilingHelpers.cpp] 242\n");
        auto layout = JavascriptCallStackLayout::FromFramePointer(framePtr);
        FunctionBody* functionBody = layout->functionObject->GetFunctionBody();
        DynamicProfileInfo * dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        ThreadContext * threadContext = functionBody->GetScriptContext()->GetThreadContext();
        auto flags = threadContext->GetAddressOfImplicitCallFlags();

        const auto fls = *flags;
        dynamicProfileInfo->RecordLoopImplicitCallFlags(functionBody, loopNum, fls);

        // Due to how JITed code is lowered, even we requested the Opnd to be the size of ImplicitCallArgs, the full
        //     register is pushed, with whatever junk is in the upper bits. So mask them off.
        restoreCallFlags = restoreCallFlags & ((1 << sizeof(ImplicitCallFlags)*8) - 1);

        // If the caller doesn't want to add in any call flags they will pass zero in restoreCallFlags.
        //    and since ORing with 0 is idempotent, we can just OR it either way
        *flags = (ImplicitCallFlags)(restoreCallFlags | fls);
    }
}
