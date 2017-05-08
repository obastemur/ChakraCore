//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLanguagePch.h"

namespace Js
{
#if ENABLE_PROFILE_INFO
    Var ProfilingHelpers::ProfiledLdElem(
        const Var base,
        const Var varIndex,
        FunctionBody *const functionBody,
        const ProfileId profileId)
    {TRACE_IT(52055);
        Assert(base);
        Assert(varIndex);
        Assert(functionBody);
        Assert(profileId != Constants::NoProfileId);

        LdElemInfo ldElemInfo;

        // Only enable fast path if the javascript array is not cross site
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(base);
#endif
        const bool isJsArray = !TaggedNumber::Is(base) && VirtualTableInfo<JavascriptArray>::HasVirtualTable(base);
        const bool fastPath = isJsArray;
        if(fastPath)
        {TRACE_IT(52056);
            JavascriptArray *const array = JavascriptArray::FromVar(base);
            ldElemInfo.arrayType = ValueType::FromArray(ObjectType::Array, array, TypeIds_Array).ToLikely();

            const Var element = ProfiledLdElem_FastPath(array, varIndex, functionBody->GetScriptContext(), &ldElemInfo);

            ldElemInfo.elemType = ldElemInfo.elemType.Merge(element);
            functionBody->GetDynamicProfileInfo()->RecordElementLoad(functionBody, profileId, ldElemInfo);
            return element;
        }

        Assert(!isJsArray);
        bool isObjectWithArray;
        TypeId arrayTypeId;
        JavascriptArray *const array =
            JavascriptArray::GetArrayForArrayOrObjectWithArray(base, &isObjectWithArray, &arrayTypeId);

        do // while(false)
        {TRACE_IT(52057);
            // The fast path is only for JavascriptArray and doesn't cover native arrays, objects with internal arrays, or typed
            // arrays, but we still need to profile the array

            uint32 headSegmentLength;
            if(array)
            {TRACE_IT(52058);
                ldElemInfo.arrayType =
                    (
                        isObjectWithArray
                            ? ValueType::FromObjectArray(array)
                            : ValueType::FromArray(ObjectType::Array, array, arrayTypeId)
                    ).ToLikely();

                SparseArraySegmentBase *const head = array->GetHead();
                Assert(head->left == 0);
                headSegmentLength = head->length;
            }
            else if(TypedArrayBase::TryGetLengthForOptimizedTypedArray(base, &headSegmentLength, &arrayTypeId))
            {TRACE_IT(52059);
                bool isVirtual = (VirtualTableInfoBase::GetVirtualTable(base) == ValueType::GetVirtualTypedArrayVtable(arrayTypeId));
                ldElemInfo.arrayType = ValueType::FromTypeId(arrayTypeId, isVirtual).ToLikely();
            }
            else
            {TRACE_IT(52060);
                break;
            }

            if(!TaggedInt::Is(varIndex))
            {TRACE_IT(52061);
                ldElemInfo.neededHelperCall = true;
                break;
            }

            const int32 index = TaggedInt::ToInt32(varIndex);
            const uint32 offset = index;
            if(index < 0 || offset >= headSegmentLength || (array && array->IsMissingHeadSegmentItem(offset)))
            {TRACE_IT(52062);
                ldElemInfo.neededHelperCall = true;
                break;
            }
        } while(false);

        const Var element = JavascriptOperators::OP_GetElementI(base, varIndex, functionBody->GetScriptContext());

        const ValueType arrayType(ldElemInfo.GetArrayType());
        if(!arrayType.IsUninitialized())
        {TRACE_IT(52063);
            if(arrayType.IsLikelyObject() && arrayType.GetObjectType() == ObjectType::Array && !arrayType.HasIntElements())
            {TRACE_IT(52064);
                JavascriptOperators::UpdateNativeArrayProfileInfoToCreateVarArray(
                    array,
                    arrayType.HasFloatElements(),
                    arrayType.HasVarElements());
            }

            ldElemInfo.elemType = ValueType::Uninitialized.Merge(element);
            functionBody->GetDynamicProfileInfo()->RecordElementLoad(functionBody, profileId, ldElemInfo);
            return element;
        }

        functionBody->GetDynamicProfileInfo()->RecordElementLoadAsProfiled(functionBody, profileId);
        return element;
    }

    Var ProfilingHelpers::ProfiledLdElem_FastPath(
        JavascriptArray *const array,
        const Var varIndex,
        ScriptContext *const scriptContext,
        LdElemInfo *const ldElemInfo)
    {TRACE_IT(52065);
        Assert(array);
        Assert(varIndex);
        Assert(scriptContext);

        do // while(false)
        {TRACE_IT(52066);
            Assert(!array->IsCrossSiteObject());
            if (!TaggedInt::Is(varIndex))
            {TRACE_IT(52067);
                break;
            }

            int32 index = TaggedInt::ToInt32(varIndex);

            if (index < 0)
            {TRACE_IT(52068);
                break;
            }

            if(ldElemInfo)
            {TRACE_IT(52069);
                SparseArraySegment<Var> *const head = static_cast<SparseArraySegment<Var> *>(array->GetHead());
                Assert(head->left == 0);
                const uint32 offset = index;
                if(offset < head->length)
                {TRACE_IT(52070);
                    const Var element = head->elements[offset];
                    if(!SparseArraySegment<Var>::IsMissingItem(&element))
                    {TRACE_IT(52071);
                        // Successful fastpath
                        return element;
                    }
                }

                ldElemInfo->neededHelperCall = true;
            }

            SparseArraySegment<Var> *seg = (SparseArraySegment<Var>*)array->GetLastUsedSegment();
            if ((uint32) index < seg->left)
            {TRACE_IT(52072);
                break;
            }

            uint32 index2 = index - seg->left;

            if (index2 < seg->length)
            {TRACE_IT(52073);
                Var elem = seg->elements[index2];
                if (elem != SparseArraySegment<Var>::GetMissingItem())
                {TRACE_IT(52074);
                    // Successful fastpath
                    return elem;
                }
            }
        } while(false);

        if(ldElemInfo)
        {TRACE_IT(52075);
            ldElemInfo->neededHelperCall = true;
        }
        return JavascriptOperators::OP_GetElementI(array, varIndex, scriptContext);
    }

    void ProfilingHelpers::ProfiledStElem_DefaultFlags(
        const Var base,
        const Var varIndex,
        const Var value,
        FunctionBody *const functionBody,
        const ProfileId profileId)
    {
        ProfiledStElem(base, varIndex, value, functionBody, profileId, PropertyOperation_None);
    }

    void ProfilingHelpers::ProfiledStElem(
        const Var base,
        const Var varIndex,
        const Var value,
        FunctionBody *const functionBody,
        const ProfileId profileId,
        const PropertyOperationFlags flags)
    {TRACE_IT(52076);
        Assert(base);
        Assert(varIndex);
        Assert(value);
        Assert(functionBody);
        Assert(profileId != Constants::NoProfileId);

        StElemInfo stElemInfo;

        // Only enable fast path if the javascript array is not cross site
        const bool isJsArray = !TaggedNumber::Is(base) && VirtualTableInfo<JavascriptArray>::HasVirtualTable(base);
        ScriptContext *const scriptContext = functionBody->GetScriptContext();
        const bool fastPath = isJsArray && !JavascriptOperators::SetElementMayHaveImplicitCalls(scriptContext);
        if(fastPath)
        {TRACE_IT(52077);
            JavascriptArray *const array = JavascriptArray::FromVar(base);
            stElemInfo.arrayType = ValueType::FromArray(ObjectType::Array, array, TypeIds_Array).ToLikely();
            stElemInfo.createdMissingValue = array->HasNoMissingValues();

            ProfiledStElem_FastPath(array, varIndex, value, scriptContext, flags, &stElemInfo);

            stElemInfo.createdMissingValue &= !array->HasNoMissingValues();
            functionBody->GetDynamicProfileInfo()->RecordElementStore(functionBody, profileId, stElemInfo);
            return;
        }

        JavascriptArray *array;
        bool isObjectWithArray;
        TypeId arrayTypeId;
        if(isJsArray)
        {TRACE_IT(52078);
            array = JavascriptArray::FromVar(base);
            isObjectWithArray = false;
            arrayTypeId = TypeIds_Array;
        }
        else
        {TRACE_IT(52079);
            array = JavascriptArray::GetArrayForArrayOrObjectWithArray(base, &isObjectWithArray, &arrayTypeId);
        }

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(base);
#endif

        do // while(false)
        {TRACE_IT(52080);
            // The fast path is only for JavascriptArray and doesn't cover native arrays, objects with internal arrays, or typed
            // arrays, but we still need to profile the array

            uint32 length;
            uint32 headSegmentLength;
            if(array)
            {TRACE_IT(52081);
                stElemInfo.arrayType =
                    (
                        isObjectWithArray
                            ? ValueType::FromObjectArray(array)
                            : ValueType::FromArray(ObjectType::Array, array, arrayTypeId)
                    ).ToLikely();
                stElemInfo.createdMissingValue = array->HasNoMissingValues();

                length = array->GetLength();
                SparseArraySegmentBase *const head = array->GetHead();
                Assert(head->left == 0);
                headSegmentLength = head->length;
            }
            else if(TypedArrayBase::TryGetLengthForOptimizedTypedArray(base, &headSegmentLength, &arrayTypeId))
            {TRACE_IT(52082);
                length = headSegmentLength;
                bool isVirtual = (VirtualTableInfoBase::GetVirtualTable(base) == ValueType::GetVirtualTypedArrayVtable(arrayTypeId));
                stElemInfo.arrayType = ValueType::FromTypeId(arrayTypeId, isVirtual).ToLikely();
            }
            else
            {TRACE_IT(52083);
                break;
            }

            if(!TaggedInt::Is(varIndex))
            {TRACE_IT(52084);
                stElemInfo.neededHelperCall = true;
                break;
            }

            const int32 index = TaggedInt::ToInt32(varIndex);
            if(index < 0)
            {TRACE_IT(52085);
                stElemInfo.neededHelperCall = true;
                break;
            }

            const uint32 offset = index;
            if(offset >= headSegmentLength)
            {TRACE_IT(52086);
                stElemInfo.storedOutsideHeadSegmentBounds = true;
                if(!isObjectWithArray && offset >= length)
                {TRACE_IT(52087);
                    stElemInfo.storedOutsideArrayBounds = true;
                }
                break;
            }

            if(array && array->IsMissingHeadSegmentItem(offset))
            {TRACE_IT(52088);
                stElemInfo.filledMissingValue = true;
            }
        } while(false);

        JavascriptOperators::OP_SetElementI(base, varIndex, value, scriptContext, flags);

        if(!stElemInfo.GetArrayType().IsUninitialized())
        {TRACE_IT(52089);
            if(array)
            {TRACE_IT(52090);
                stElemInfo.createdMissingValue &= !array->HasNoMissingValues();
            }
            functionBody->GetDynamicProfileInfo()->RecordElementStore(functionBody, profileId, stElemInfo);
            return;
        }

        functionBody->GetDynamicProfileInfo()->RecordElementStoreAsProfiled(functionBody, profileId);
    }

    void ProfilingHelpers::ProfiledStElem_FastPath(
        JavascriptArray *const array,
        const Var varIndex,
        const Var value,
        ScriptContext *const scriptContext,
        const PropertyOperationFlags flags,
        StElemInfo *const stElemInfo)
    {TRACE_IT(52091);
        Assert(array);
        Assert(varIndex);
        Assert(value);
        Assert(scriptContext);
        Assert(!JavascriptOperators::SetElementMayHaveImplicitCalls(scriptContext));

        do // while(false)
        {TRACE_IT(52092);
            if (!TaggedInt::Is(varIndex))
            {TRACE_IT(52093);
                break;
            }

            int32 index = TaggedInt::ToInt32(varIndex);

            if (index < 0)
            {TRACE_IT(52094);
                break;
            }

            if(stElemInfo)
            {TRACE_IT(52095);
                SparseArraySegmentBase *const head = array->GetHead();
                Assert(head->left == 0);
                const uint32 offset = index;
                if(offset >= head->length)
                {TRACE_IT(52096);
                    stElemInfo->storedOutsideHeadSegmentBounds = true;
                    if(offset >= array->GetLength())
                    {TRACE_IT(52097);
                        stElemInfo->storedOutsideArrayBounds = true;
                    }
                }

                if(offset < head->size)
                {TRACE_IT(52098);
                    array->DirectProfiledSetItemInHeadSegmentAt(offset, value, stElemInfo);
                    return;
                }
            }

            SparseArraySegment<Var>* lastUsedSeg = (SparseArraySegment<Var>*)array->GetLastUsedSegment();
            if (lastUsedSeg == NULL ||
                (uint32) index < lastUsedSeg->left)
            {TRACE_IT(52099);
                break;
            }

            uint32 index2 = index - lastUsedSeg->left;

            if (index2 < lastUsedSeg->size)
            {TRACE_IT(52100);
                // Successful fastpath
                array->DirectSetItemInLastUsedSegmentAt(index2, value);
                return;
            }
        } while(false);

        if(stElemInfo)
        {TRACE_IT(52101);
            stElemInfo->neededHelperCall = true;
        }
        JavascriptOperators::OP_SetElementI(array, varIndex, value, scriptContext, flags);
    }

    JavascriptArray *ProfilingHelpers::ProfiledNewScArray(
        const uint length,
        FunctionBody *const functionBody,
        const ProfileId profileId)
    {TRACE_IT(52102);
        Assert(functionBody);
        Assert(profileId != Constants::NoProfileId);

        // Not creating native array here if the function is unoptimized, because it turns out to be tricky to
        // get the initialization right if GlobOpt doesn't give us bailout. It's possible, but we should see
        // a use case before spending time on it.
        ArrayCallSiteInfo *const arrayInfo =
            functionBody->GetDynamicProfileInfo()->GetArrayCallSiteInfo(functionBody, profileId);
        Assert(arrayInfo);
        if (length > SparseArraySegmentBase::INLINE_CHUNK_SIZE || (functionBody->GetHasTry() && PHASE_OFF((Js::OptimizeTryCatchPhase), functionBody)))
        {TRACE_IT(52103);
            arrayInfo->SetIsNotNativeArray();
        }

        ScriptContext *const scriptContext = functionBody->GetScriptContext();
        JavascriptArray *array;
        if (arrayInfo->IsNativeIntArray())
        {TRACE_IT(52104);
            JavascriptNativeIntArray *const intArray = scriptContext->GetLibrary()->CreateNativeIntArrayLiteral(length);
            Recycler *recycler = scriptContext->GetRecycler();
            intArray->SetArrayCallSite(profileId, recycler->CreateWeakReferenceHandle(functionBody));
            array = intArray;
        }
        else if (arrayInfo->IsNativeFloatArray())
        {TRACE_IT(52105);
            JavascriptNativeFloatArray *const floatArray = scriptContext->GetLibrary()->CreateNativeFloatArrayLiteral(length);
            Recycler *recycler = scriptContext->GetRecycler();
            floatArray->SetArrayCallSite(profileId, recycler->CreateWeakReferenceHandle(functionBody));
            array = floatArray;
        }
        else
        {TRACE_IT(52106);
            array = scriptContext->GetLibrary()->CreateArrayLiteral(length);
        }

#ifdef ENABLE_DEBUG_CONFIG_OPTIONS
        array->CheckForceES5Array();
#endif

        return array;
    }

    Var ProfilingHelpers::ProfiledNewScObjArray_Jit(
        const Var callee,
        void *const framePointer,
        const ProfileId profileId,
        const ProfileId arrayProfileId,
        CallInfo callInfo,
        ...)
    {
        ARGUMENTS(args, callee, framePointer, profileId, arrayProfileId, callInfo);
        return
            ProfiledNewScObjArray(
                callee,
                args,
                ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject),
                profileId,
                arrayProfileId);
    }

    Var ProfilingHelpers::ProfiledNewScObjArraySpread_Jit(
        const Js::AuxArray<uint32> *spreadIndices,
        const Var callee,
        void *const framePointer,
        const ProfileId profileId,
        const ProfileId arrayProfileId,
        CallInfo callInfo,
        ...)
    {
        ARGUMENTS(args, callInfo);

        Js::ScriptFunction *function = ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        ScriptContext* scriptContext = function->GetScriptContext();

        // GetSpreadSize ensures that spreadSize < 2^24
        uint32 spreadSize = 0;
        if (spreadIndices != nullptr)
        {TRACE_IT(52107);
            Arguments outArgs(CallInfo(args.Info.Flags, 0), nullptr);
            spreadSize = JavascriptFunction::GetSpreadSize(args, spreadIndices, scriptContext);
            Assert(spreadSize == (((1 << 24) - 1) & spreadSize));
            // Allocate room on the stack for the spread args.
            outArgs.Info.Count = spreadSize;
            const unsigned STACK_ARGS_ALLOCA_THRESHOLD = 8; // Number of stack args we allow before using _alloca
            Var stackArgs[STACK_ARGS_ALLOCA_THRESHOLD];
            size_t outArgsSize = 0;
            if (outArgs.Info.Count > STACK_ARGS_ALLOCA_THRESHOLD)
            {
                PROBE_STACK(scriptContext, outArgs.Info.Count * sizeof(Var) + Js::Constants::MinStackDefault); // args + function call
                outArgsSize = outArgs.Info.Count * sizeof(Var);
                outArgs.Values = (Var*)_alloca(outArgsSize);
                ZeroMemory(outArgs.Values, outArgsSize);
            }
            else
            {TRACE_IT(52108);
                outArgs.Values = stackArgs;
                outArgsSize = STACK_ARGS_ALLOCA_THRESHOLD * sizeof(Var);
                ZeroMemory(outArgs.Values, outArgsSize); // We may not use all of the elements
            }
            JavascriptFunction::SpreadArgs(args, outArgs, spreadIndices, scriptContext);
            return
                ProfiledNewScObjArray(
                    callee,
                    outArgs,
                    function,
                    profileId,
                    arrayProfileId);
        }
        else
        {TRACE_IT(52109);
            return
                ProfiledNewScObjArray(
                    callee,
                    args,
                    function,
                    profileId,
                    arrayProfileId);
        }
    }

    Var ProfilingHelpers::ProfiledNewScObjArray(
        const Var callee,
        const Arguments args,
        ScriptFunction *const caller,
        const ProfileId profileId,
        const ProfileId arrayProfileId)
    {TRACE_IT(52110);
        Assert(callee);
        Assert(args.Info.Count != 0);
        Assert(caller);
        Assert(profileId != Constants::NoProfileId);
        Assert(arrayProfileId != Constants::NoProfileId);

        FunctionBody *const callerFunctionBody = caller->GetFunctionBody();
        DynamicProfileInfo *const profileInfo = callerFunctionBody->GetDynamicProfileInfo();
        ArrayCallSiteInfo *const arrayInfo = profileInfo->GetArrayCallSiteInfo(callerFunctionBody, arrayProfileId);
        Assert(arrayInfo);

        ScriptContext *const scriptContext = callerFunctionBody->GetScriptContext();
        FunctionInfo *const calleeFunctionInfo = JavascriptOperators::GetConstructorFunctionInfo(callee, scriptContext);
        if (calleeFunctionInfo != &JavascriptArray::EntryInfo::NewInstance)
        {TRACE_IT(52111);
            // It may be worth checking the object that we actually got back from the ctor, but
            // we should at least not keep bailing out at this call site.
            arrayInfo->SetIsNotNativeArray();
            return ProfiledNewScObject(callee, args, callerFunctionBody, profileId);
        }

        profileInfo->RecordCallSiteInfo(
            callerFunctionBody,
            profileId,
            calleeFunctionInfo,
            caller,
            args.Info.Count,
            true);

        args.Values[0] = nullptr;
        Var array;
        if (arrayInfo->IsNativeIntArray())
        {TRACE_IT(52112);
            array = JavascriptNativeIntArray::NewInstance(RecyclableObject::FromVar(callee), args);
            if (VirtualTableInfo<JavascriptNativeIntArray>::HasVirtualTable(array))
            {TRACE_IT(52113);
                JavascriptNativeIntArray *const intArray = static_cast<JavascriptNativeIntArray *>(array);
                intArray->SetArrayCallSite(arrayProfileId, scriptContext->GetRecycler()->CreateWeakReferenceHandle(callerFunctionBody));
            }
            else
            {TRACE_IT(52114);
                arrayInfo->SetIsNotNativeIntArray();
                if (VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(array))
                {TRACE_IT(52115);
                    JavascriptNativeFloatArray *const floatArray = static_cast<JavascriptNativeFloatArray *>(array);
                    floatArray->SetArrayCallSite(arrayProfileId, scriptContext->GetRecycler()->CreateWeakReferenceHandle(callerFunctionBody));
                }
                else
                {TRACE_IT(52116);
                    arrayInfo->SetIsNotNativeArray();
                }
            }
        }
        else if (arrayInfo->IsNativeFloatArray())
        {TRACE_IT(52117);
            array = JavascriptNativeFloatArray::NewInstance(RecyclableObject::FromVar(callee), args);
            if (VirtualTableInfo<JavascriptNativeFloatArray>::HasVirtualTable(array))
            {TRACE_IT(52118);
                JavascriptNativeFloatArray *const floatArray = static_cast<JavascriptNativeFloatArray *>(array);
                floatArray->SetArrayCallSite(arrayProfileId, scriptContext->GetRecycler()->CreateWeakReferenceHandle(callerFunctionBody));
            }
            else
            {TRACE_IT(52119);
                arrayInfo->SetIsNotNativeArray();
            }
        }
        else
        {TRACE_IT(52120);
            array = JavascriptArray::NewInstance(RecyclableObject::FromVar(callee), args);
        }

        return CrossSite::MarshalVar(scriptContext, array);
    }

    Var ProfilingHelpers::ProfiledNewScObject(
        const Var callee,
        const Arguments args,
        FunctionBody *const callerFunctionBody,
        const ProfileId profileId,
        const InlineCacheIndex inlineCacheIndex,
        const Js::AuxArray<uint32> *spreadIndices)
    {TRACE_IT(52121);
        Assert(callee);
        Assert(args.Info.Count != 0);
        Assert(callerFunctionBody);
        Assert(profileId != Constants::NoProfileId);

        ScriptContext *const scriptContext = callerFunctionBody->GetScriptContext();
        if(!TaggedNumber::Is(callee))
        {TRACE_IT(52122);
            const auto calleeObject = JavascriptOperators::GetCallableObjectOrThrow(callee, scriptContext);
            const auto calleeFunctionInfo =
                calleeObject->GetTypeId() == TypeIds_Function
                    ? JavascriptFunction::FromVar(calleeObject)->GetFunctionInfo()
                    : nullptr;
            DynamicProfileInfo *profileInfo = callerFunctionBody->GetDynamicProfileInfo();
            profileInfo->RecordCallSiteInfo(
                callerFunctionBody,
                profileId,
                calleeFunctionInfo,
                calleeFunctionInfo ? static_cast<JavascriptFunction *>(calleeObject) : nullptr,
                args.Info.Count,
                true,
                inlineCacheIndex);
            // We need to record information here, most importantly so that we handle array subclass
            // creation properly, since optimizing those cases is important
            Var retVal = JavascriptOperators::NewScObject(callee, args, scriptContext, spreadIndices);
            profileInfo->RecordReturnTypeOnCallSiteInfo(callerFunctionBody, profileId, retVal);
            return retVal;
        }

        return JavascriptOperators::NewScObject(callee, args, scriptContext, spreadIndices);
    }

    void ProfilingHelpers::ProfileLdSlot(const Var value, FunctionBody *const functionBody, const ProfileId profileId)
    {TRACE_IT(52123);
        Assert(value);
        Assert(functionBody);
        Assert(profileId != Constants::NoProfileId);

        functionBody->GetDynamicProfileInfo()->RecordSlotLoad(functionBody, profileId, value);
    }

    Var ProfilingHelpers::ProfiledLdFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        void *const framePointer)
    {TRACE_IT(52124);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        return
            ProfiledLdFld<false, false, false>(
                instance,
                propertyId,
                GetInlineCache(scriptFunction, inlineCacheIndex),
                inlineCacheIndex,
                scriptFunction->GetFunctionBody(),
                instance);
    }

    Var ProfilingHelpers::ProfiledLdSuperFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        void *const framePointer,
        const Var thisInstance)
        {TRACE_IT(52125);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        return
            ProfiledLdFld<false, false, false>(
            instance,
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            scriptFunction->GetFunctionBody(),
            thisInstance);
    }

    Var ProfilingHelpers::ProfiledLdFldForTypeOf_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        void *const framePointer)
    {TRACE_IT(52126);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);

        return ProfiledLdFldForTypeOf<false, false, false>(
            instance,
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            scriptFunction->GetFunctionBody());
    }


    Var ProfilingHelpers::ProfiledLdFld_CallApplyTarget_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        void *const framePointer)
    {TRACE_IT(52127);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        return
            ProfiledLdFld<false, false, true>(
                instance,
                propertyId,
                GetInlineCache(scriptFunction, inlineCacheIndex),
                inlineCacheIndex,
                scriptFunction->GetFunctionBody(),
                instance);
    }

    Var ProfilingHelpers::ProfiledLdMethodFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        void *const framePointer)
    {TRACE_IT(52128);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        return
            ProfiledLdFld<false, true, false>(
                instance,
                propertyId,
                GetInlineCache(scriptFunction, inlineCacheIndex),
                inlineCacheIndex,
                scriptFunction->GetFunctionBody(),
                instance);
    }

    Var ProfilingHelpers::ProfiledLdRootFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        void *const framePointer)
    {TRACE_IT(52129);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        return
            ProfiledLdFld<true, false, false>(
                instance,
                propertyId,
                GetInlineCache(scriptFunction, inlineCacheIndex),
                inlineCacheIndex,
                scriptFunction->GetFunctionBody(),
                instance);
    }

    Var ProfilingHelpers::ProfiledLdRootFldForTypeOf_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        void *const framePointer)
    {TRACE_IT(52130);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);

        return ProfiledLdFldForTypeOf<true, false, false>(
            instance,
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            scriptFunction->GetFunctionBody());
    }

    Var ProfilingHelpers::ProfiledLdRootMethodFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        void *const framePointer)
    {TRACE_IT(52131);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        return
            ProfiledLdFld<true, true, false>(
                instance,
                propertyId,
                GetInlineCache(scriptFunction, inlineCacheIndex),
                inlineCacheIndex,
                scriptFunction->GetFunctionBody(),
                instance);
    }

    template<bool Root, bool Method, bool CallApplyTarget>
    Var ProfilingHelpers::ProfiledLdFld(
        const Var instance,
        const PropertyId propertyId,
        InlineCache *const inlineCache,
        const InlineCacheIndex inlineCacheIndex,
        FunctionBody *const functionBody,
        const Var thisInstance)
    {TRACE_IT(52132);
        Assert(instance);
        Assert(thisInstance);
        Assert(propertyId != Constants::NoProperty);
        Assert(inlineCache);
        Assert(functionBody);
        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());
        Assert(!Root || inlineCacheIndex >= functionBody->GetRootObjectLoadInlineCacheStart());

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif
        ScriptContext *const scriptContext = functionBody->GetScriptContext();
        DynamicProfileInfo *const dynamicProfileInfo = functionBody->GetDynamicProfileInfo();
        Var value;
        FldInfoFlags fldInfoFlags = FldInfo_NoInfo;
        if (Root || (RecyclableObject::Is(instance) && RecyclableObject::Is(thisInstance)))
        {TRACE_IT(52133);
            RecyclableObject *const object = RecyclableObject::FromVar(instance);
            RecyclableObject *const thisObject = RecyclableObject::FromVar(thisInstance);

            if (!Root && Method && (propertyId == PropertyIds::apply || propertyId == PropertyIds::call) && ScriptFunction::Is(object))
            {TRACE_IT(52134);
                // If the property being loaded is "apply"/"call", make an optimistic assumption that apply/call is not overridden and
                // undefer the function right here if it was defer parsed before. This is required so that the load of "apply"/"call"
                // happens from the same "type". Otherwise, we will have a polymorphic cache for load of "apply"/"call".
                ScriptFunction *fn = ScriptFunction::FromVar(object);
                if (fn->GetType()->GetEntryPoint() == JavascriptFunction::DeferredParsingThunk)
                {TRACE_IT(52135);
                    JavascriptFunction::DeferredParse(&fn);
                }
            }

            PropertyCacheOperationInfo operationInfo;
            PropertyValueInfo propertyValueInfo;
            PropertyValueInfo::SetCacheInfo(&propertyValueInfo, functionBody, inlineCache, inlineCacheIndex, true);
            if (!CacheOperators::TryGetProperty<true, true, true, !Root && !Method, true, !Root, true, false, true>(
                    thisObject,
                    Root,
                    object,
                    propertyId,
                    &value,
                    scriptContext,
                    &operationInfo,
                    &propertyValueInfo))
            {TRACE_IT(52136);
                const auto PatchGetValue = &JavascriptOperators::PatchGetValueWithThisPtrNoFastPath;
                const auto PatchGetRootValue = &JavascriptOperators::PatchGetRootValueNoFastPath_Var;
                const auto PatchGetMethod = &JavascriptOperators::PatchGetMethodNoFastPath;
                const auto PatchGetRootMethod = &JavascriptOperators::PatchGetRootMethodNoFastPath_Var;
                const auto PatchGet =
                    Root
                        ? Method ? PatchGetRootMethod : PatchGetRootValue
                        : PatchGetMethod ;
                value = (!Root && !Method) ? PatchGetValue(functionBody, inlineCache, inlineCacheIndex, object, propertyId, thisObject) :
                    PatchGet(functionBody, inlineCache, inlineCacheIndex, object, propertyId);
                CacheOperators::PretendTryGetProperty<true, false>(object->GetType(), &operationInfo, &propertyValueInfo);
            }
            else if (!Root && !Method)
            {TRACE_IT(52137);
                // Inline cache hit. oldflags must match the new ones. If not there is mark it as polymorphic as there is likely
                // a bailout to interpreter and change in the inline cache type.
                const FldInfoFlags oldflags = dynamicProfileInfo->GetFldInfo(functionBody, inlineCacheIndex)->flags;
                if ((oldflags != FldInfo_NoInfo) &&
                    !(oldflags & DynamicProfileInfo::FldInfoFlagsFromCacheType(operationInfo.cacheType)))
                {TRACE_IT(52138);
                    fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_Polymorphic);
                }
            }

            if (propertyId == Js::PropertyIds::arguments)
            {TRACE_IT(52139);
                fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_FromAccessor);
                scriptContext->GetThreadContext()->AddImplicitCallFlags(ImplicitCall_Accessor);
            }

            if (!Root && operationInfo.isPolymorphic)
            {TRACE_IT(52140);
                fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_Polymorphic);
            }
            fldInfoFlags =
                DynamicProfileInfo::MergeFldInfoFlags(
                    fldInfoFlags,
                    DynamicProfileInfo::FldInfoFlagsFromCacheType(operationInfo.cacheType));
            fldInfoFlags =
                DynamicProfileInfo::MergeFldInfoFlags(
                    fldInfoFlags,
                    DynamicProfileInfo::FldInfoFlagsFromSlotType(operationInfo.slotType));

            if (!Method)
            {
                UpdateFldInfoFlagsForGetSetInlineCandidate(
                    object,
                    fldInfoFlags,
                    operationInfo.cacheType,
                    inlineCache,
                    functionBody);
                if (!Root && CallApplyTarget)
                {
                    UpdateFldInfoFlagsForCallApplyInlineCandidate(
                        object,
                        fldInfoFlags,
                        operationInfo.cacheType,
                        inlineCache,
                        functionBody);
                }
            }
        }
        else
        {TRACE_IT(52141);
            Assert(!Root);
            const auto PatchGetValue = &JavascriptOperators::PatchGetValue<false, InlineCache>;
            const auto PatchGetMethod = &JavascriptOperators::PatchGetMethod<false, InlineCache>;
            const auto PatchGet = Method ? PatchGetMethod : PatchGetValue;
            value = PatchGet(functionBody, inlineCache, inlineCacheIndex, instance, propertyId);
        }

        dynamicProfileInfo->RecordFieldAccess(functionBody, inlineCacheIndex, value, fldInfoFlags);
        return value;
    }

    template<bool Root, bool Method, bool CallApplyTarget>
    Var ProfilingHelpers::ProfiledLdFldForTypeOf(
        const Var instance,
        const PropertyId propertyId,
        InlineCache *const inlineCache,
        const InlineCacheIndex inlineCacheIndex,
        FunctionBody *const functionBody)
    {TRACE_IT(52142);
        Var val = nullptr;
        ScriptContext *scriptContext = functionBody->GetScriptContext();

        BEGIN_PROFILED_TYPEOF_ERROR_HANDLER(scriptContext);
        val = ProfiledLdFld<Root, Method, CallApplyTarget>(
            instance,
            propertyId,
            inlineCache,
            inlineCacheIndex,
            functionBody,
            instance);
        END_PROFILED_TYPEOF_ERROR_HANDLER(scriptContext, val, functionBody, inlineCacheIndex);

        return val;
    }

    void ProfilingHelpers::ProfiledStFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        const Var value,
        void *const framePointer)
    {TRACE_IT(52143);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        ProfiledStFld<false>(
            instance,
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            value,
            PropertyOperation_None,
            scriptFunction,
            instance);
    }

    void ProfilingHelpers::ProfiledStSuperFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        const Var value,
        void *const framePointer,
        const Var thisInstance)
    {TRACE_IT(52144);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        ProfiledStFld<false>(
            instance,
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            value,
            PropertyOperation_None,
            scriptFunction,
            thisInstance);
    }

    void ProfilingHelpers::ProfiledStFld_Strict_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        const Var value,
        void *const framePointer)
    {TRACE_IT(52145);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        ProfiledStFld<false>(
            instance,
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            value,
            PropertyOperation_StrictMode,
            scriptFunction,
            instance);
    }

    void ProfilingHelpers::ProfiledStRootFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        const Var value,
        void *const framePointer)
    {TRACE_IT(52146);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        ProfiledStFld<true>(
            instance,
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            value,
            PropertyOperation_Root,
            scriptFunction,
            instance);
    }

    void ProfilingHelpers::ProfiledStRootFld_Strict_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        const Var value,
        void *const framePointer)
    {TRACE_IT(52147);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        ProfiledStFld<true>(
            instance,
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            value,
            PropertyOperation_StrictModeRoot,
            scriptFunction,
            instance);
    }

    template<bool Root>
    void ProfilingHelpers::ProfiledStFld(
        const Var instance,
        const PropertyId propertyId,
        InlineCache *const inlineCache,
        const InlineCacheIndex inlineCacheIndex,
        const Var value,
        const PropertyOperationFlags flags,
        ScriptFunction *const scriptFunction,
        const Var thisInstance)
    {TRACE_IT(52148);
        Assert(instance);
        Assert(thisInstance);
        Assert(propertyId != Constants::NoProperty);
        Assert(inlineCache);

        Assert(scriptFunction);
        FunctionBody *const functionBody = scriptFunction->GetFunctionBody();

        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());
        Assert(value);

#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(instance);
#endif

        ScriptContext *const scriptContext = functionBody->GetScriptContext();
        FldInfoFlags fldInfoFlags = FldInfo_NoInfo;
        if(Root || (RecyclableObject::Is(instance) && RecyclableObject::Is(thisInstance)))
        {TRACE_IT(52149);
            RecyclableObject *const object = RecyclableObject::FromVar(instance);
            RecyclableObject *const thisObject = RecyclableObject::FromVar(thisInstance);
            PropertyCacheOperationInfo operationInfo;
            PropertyValueInfo propertyValueInfo;
            PropertyValueInfo::SetCacheInfo(&propertyValueInfo, functionBody, inlineCache, inlineCacheIndex, true);
            if(!CacheOperators::TrySetProperty<true, true, true, true, !Root, true, false, true>(
                    thisObject,
                    Root,
                    propertyId,
                    value,
                    scriptContext,
                    flags,
                    &operationInfo,
                    &propertyValueInfo))
            {TRACE_IT(52150);
                ThreadContext* threadContext = scriptContext->GetThreadContext();
                ImplicitCallFlags savedImplicitCallFlags = threadContext->GetImplicitCallFlags();
                threadContext->ClearImplicitCallFlags();

                Type *const oldType = object->GetType();

                if (Root)
                {TRACE_IT(52151);
                    JavascriptOperators::PatchPutRootValueNoFastPath(functionBody, inlineCache, inlineCacheIndex, object, propertyId, value, flags);
                }
                else
                {TRACE_IT(52152);
                    JavascriptOperators::PatchPutValueWithThisPtrNoFastPath(functionBody, inlineCache, inlineCacheIndex, object, propertyId, value, thisObject, flags);
                }
                CacheOperators::PretendTrySetProperty<true, false>(
                    object->GetType(),
                    oldType,
                    &operationInfo,
                    &propertyValueInfo);

                // Setting to __proto__ property invokes a setter and changes the prototype.So, although PatchPut* populates the cache,
                // the setter invalidates it (since it changes the prototype). PretendTrySetProperty looks at the inline cache type to
                // update the cacheType on PropertyOperationInfo, which is used in populating the field info flags for this operation on
                // the profile. Since the cache was invalidated, we don't get a match with either the type of the object with property or
                // without it and the cacheType defaults to CacheType_None. This leads the profile info to say that this operation doesn't
                // cause an accessor implicit call and JIT then doesn't kill live fields across it and ends up putting a BailOutOnImplicitCalls
                // if there were live fields. This bailout always bails out.
                Js::ImplicitCallFlags accessorCallFlag = (Js::ImplicitCallFlags)(Js::ImplicitCall_Accessor & ~Js::ImplicitCall_None);
                if ((threadContext->GetImplicitCallFlags() & accessorCallFlag) != 0)
                {TRACE_IT(52153);
                    operationInfo.cacheType = CacheType_Setter;
                }
                threadContext->SetImplicitCallFlags((Js::ImplicitCallFlags)(savedImplicitCallFlags | threadContext->GetImplicitCallFlags()));
            }

            // Only make the field polymorphic if we are not using the root object inline cache
            if(operationInfo.isPolymorphic && inlineCacheIndex < functionBody->GetRootObjectStoreInlineCacheStart())
            {TRACE_IT(52154);
                // should not be a load inline cache
                Assert(inlineCacheIndex < functionBody->GetRootObjectLoadInlineCacheStart());
                fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_Polymorphic);
            }
            fldInfoFlags =
                DynamicProfileInfo::MergeFldInfoFlags(
                    fldInfoFlags,
                    DynamicProfileInfo::FldInfoFlagsFromCacheType(operationInfo.cacheType));
            fldInfoFlags =
                DynamicProfileInfo::MergeFldInfoFlags(
                    fldInfoFlags,
                    DynamicProfileInfo::FldInfoFlagsFromSlotType(operationInfo.slotType));

            UpdateFldInfoFlagsForGetSetInlineCandidate(
                object,
                fldInfoFlags,
                operationInfo.cacheType,
                inlineCache,
                functionBody);

            if(scriptFunction->GetConstructorCache()->NeedsUpdateAfterCtor())
            {TRACE_IT(52155);
                // This function has only 'this' statements and is being used as a constructor. When the constructor exits, the
                // function object's constructor cache will be updated with the type produced by the constructor. From that
                // point on, when the same function object is used as a constructor, the a new object with the final type will
                // be created. Whatever is stored in the inline cache currently will cause cache misses after the constructor
                // cache update. So, just clear it now so that the caches won't be flagged as polymorphic.
                inlineCache->Clear();
            }
        }
        else
        {TRACE_IT(52156);
            JavascriptOperators::PatchPutValueNoLocalFastPath<false>(
                functionBody,
                inlineCache,
                inlineCacheIndex,
                instance,
                propertyId,
                value,
                flags);
        }

        functionBody->GetDynamicProfileInfo()->RecordFieldAccess(functionBody, inlineCacheIndex, nullptr, fldInfoFlags);
    }

    void ProfilingHelpers::ProfiledInitFld_Jit(
        const Var instance,
        const PropertyId propertyId,
        const InlineCacheIndex inlineCacheIndex,
        const Var value,
        void *const framePointer)
    {TRACE_IT(52157);
        ScriptFunction *const scriptFunction =
            ScriptFunction::FromVar(JavascriptCallStackLayout::FromFramePointer(framePointer)->functionObject);
        ProfiledInitFld(
            RecyclableObject::FromVar(instance),
            propertyId,
            GetInlineCache(scriptFunction, inlineCacheIndex),
            inlineCacheIndex,
            value,
            scriptFunction->GetFunctionBody());
    }

    void ProfilingHelpers::ProfiledInitFld(
        RecyclableObject *const object,
        const PropertyId propertyId,
        InlineCache *const inlineCache,
        const InlineCacheIndex inlineCacheIndex,
        const Var value,
        FunctionBody *const functionBody)
    {TRACE_IT(52158);
        Assert(object);
        Assert(propertyId != Constants::NoProperty);
        Assert(inlineCache);
        Assert(functionBody);
        Assert(inlineCacheIndex < functionBody->GetInlineCacheCount());
        Assert(value);

        ScriptContext *const scriptContext = functionBody->GetScriptContext();
        FldInfoFlags fldInfoFlags = FldInfo_NoInfo;
        PropertyCacheOperationInfo operationInfo;
        PropertyValueInfo propertyValueInfo;
        PropertyValueInfo::SetCacheInfo(&propertyValueInfo, functionBody, inlineCache, inlineCacheIndex, true);
        if(!CacheOperators::TrySetProperty<true, true, true, true, true, true, false, true>(
                object,
                false,
                propertyId,
                value,
                scriptContext,
                PropertyOperation_None,
                &operationInfo,
                &propertyValueInfo))
        {TRACE_IT(52159);
            Type *const oldType = object->GetType();
            JavascriptOperators::PatchInitValueNoFastPath(
                functionBody,
                inlineCache,
                inlineCacheIndex,
                object,
                propertyId,
                value);
            CacheOperators::PretendTrySetProperty<true, false>(object->GetType(), oldType, &operationInfo, &propertyValueInfo);
        }

        // Only make the field polymorphic if the we are not using the root object inline cache
        if(operationInfo.isPolymorphic && inlineCacheIndex < functionBody->GetRootObjectStoreInlineCacheStart())
        {TRACE_IT(52160);
            // should not be a load inline cache
            Assert(inlineCacheIndex < functionBody->GetRootObjectLoadInlineCacheStart());
            fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_Polymorphic);
        }
        fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, DynamicProfileInfo::FldInfoFlagsFromCacheType(operationInfo.cacheType));
        fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, DynamicProfileInfo::FldInfoFlagsFromSlotType(operationInfo.slotType));

        functionBody->GetDynamicProfileInfo()->RecordFieldAccess(functionBody, inlineCacheIndex, nullptr, fldInfoFlags);
    }

    void ProfilingHelpers::UpdateFldInfoFlagsForGetSetInlineCandidate(
        RecyclableObject *const object,
        FldInfoFlags &fldInfoFlags,
        const CacheType cacheType,
        InlineCache *const inlineCache,
        FunctionBody *const functionBody)
    {TRACE_IT(52161);
        RecyclableObject *callee = nullptr;
        if((cacheType & (CacheType_Getter | CacheType_Setter)) &&
            inlineCache->GetGetterSetter(object->GetType(), &callee))
        {TRACE_IT(52162);
            const bool canInline = functionBody->GetDynamicProfileInfo()->RecordLdFldCallSiteInfo(functionBody, callee, false /*callApplyTarget*/);
            if(canInline)
            {TRACE_IT(52163);
                //updates this fldInfoFlags passed by reference.
                fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_InlineCandidate);
            }
        }
    }

    void ProfilingHelpers::UpdateFldInfoFlagsForCallApplyInlineCandidate(
        RecyclableObject *const object,
        FldInfoFlags &fldInfoFlags,
        const CacheType cacheType,
        InlineCache *const inlineCache,
        FunctionBody *const functionBody)
    {TRACE_IT(52164);
        RecyclableObject *callee = nullptr;
        if(!(fldInfoFlags & FldInfo_Polymorphic) && inlineCache->GetCallApplyTarget(object, &callee))
        {TRACE_IT(52165);
            const bool canInline = functionBody->GetDynamicProfileInfo()->RecordLdFldCallSiteInfo(functionBody, callee, true /*callApplyTarget*/);
            if(canInline)
            {TRACE_IT(52166);
                //updates the fldInfoFlags passed by reference.
                fldInfoFlags = DynamicProfileInfo::MergeFldInfoFlags(fldInfoFlags, FldInfo_InlineCandidate);
            }
        }
    }

    InlineCache *ProfilingHelpers::GetInlineCache(ScriptFunction *const scriptFunction, const InlineCacheIndex inlineCacheIndex)
    {TRACE_IT(52167);
        Assert(scriptFunction);
        Assert(inlineCacheIndex < scriptFunction->GetFunctionBody()->GetInlineCacheCount());

        return
            scriptFunction->GetHasInlineCaches()
                ? ScriptFunctionWithInlineCache::FromVar(scriptFunction)->GetInlineCache(inlineCacheIndex)
                : scriptFunction->GetFunctionBody()->GetInlineCache(inlineCacheIndex);
    }
#endif
}
