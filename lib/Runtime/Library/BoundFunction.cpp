//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    FunctionInfo BoundFunction::functionInfo(FORCE_NO_WRITE_BARRIER_TAG(BoundFunction::NewInstance), FunctionInfo::DoNotProfile);

    BoundFunction::BoundFunction(DynamicType * type)
        : JavascriptFunction(type, &functionInfo),
        targetFunction(nullptr),
        boundThis(nullptr),
        count(0),
        boundArgs(nullptr)
    {TRACE_IT(54420);
        // Constructor used during copy on write.
        DebugOnly(VerifyEntryPoint());
    }

    BoundFunction::BoundFunction(Arguments args, DynamicType * type)
        : JavascriptFunction(type, &functionInfo),
        count(0),
        boundArgs(nullptr)
    {TRACE_IT(54421);

        DebugOnly(VerifyEntryPoint());
        AssertMsg(args.Info.Count > 0, "wrong number of args in BoundFunction");

        ScriptContext *scriptContext = this->GetScriptContext();
        targetFunction = RecyclableObject::FromVar(args[0]);

        // Let proto be targetFunction.[[GetPrototypeOf]]().
        RecyclableObject* proto = JavascriptOperators::GetPrototype(targetFunction);
        if (proto != type->GetPrototype())
        {TRACE_IT(54422);
            if (type->GetIsShared())
            {TRACE_IT(54423);
                this->ChangeType();
                type = this->GetDynamicType();
            }
            type->SetPrototype(proto);
        }
        // If targetFunction is proxy, need to make sure that traps are called in right order as per 19.2.3.2 in RC#4 dated April 3rd 2015.
        // Here although we won't use value of length, this is just to make sure that we call traps involved with HasOwnProperty(Target, "length") and Get(Target, "length")
        if (JavascriptProxy::Is(targetFunction))
        {TRACE_IT(54424);
            if (JavascriptOperators::HasOwnProperty(targetFunction, PropertyIds::length, scriptContext) == TRUE)
            {TRACE_IT(54425);
                int len = 0;
                Var varLength;
                if (targetFunction->GetProperty(targetFunction, PropertyIds::length, &varLength, nullptr, scriptContext))
                {TRACE_IT(54426);
                    len = JavascriptConversion::ToInt32(varLength, scriptContext);
                }
            }
            GetTypeHandler()->EnsureObjectReady(this);
        }

        if (args.Info.Count > 1)
        {TRACE_IT(54427);
            boundThis = args[1];

            // function object and "this" arg
            const uint countAccountedFor = 2;
            count = args.Info.Count - countAccountedFor;

            // Store the args excluding function obj and "this" arg
            if (args.Info.Count > 2)
            {TRACE_IT(54428);
                boundArgs = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), count);

                for (uint i=0; i<count; i++)
                {TRACE_IT(54429);
                    boundArgs[i] = args[i+countAccountedFor];
                }
            }
        }
        else
        {TRACE_IT(54430);
            // If no "this" is passed, "undefined" is used
            boundThis = scriptContext->GetLibrary()->GetUndefined();
        }
    }

    BoundFunction::BoundFunction(RecyclableObject* targetFunction, Var boundThis, Var* args, uint argsCount, DynamicType * type)
        : JavascriptFunction(type, &functionInfo),
        count(argsCount),
        boundArgs(nullptr)
    {TRACE_IT(54431);
        DebugOnly(VerifyEntryPoint());

        this->targetFunction = targetFunction;
        this->boundThis = boundThis;

        if (argsCount != 0)
        {TRACE_IT(54432);
            this->boundArgs = RecyclerNewArray(this->GetScriptContext()->GetRecycler(), Field(Var), argsCount);

            for (uint i = 0; i < argsCount; i++)
            {TRACE_IT(54433);
                this->boundArgs[i] = args[i];
            }
        }
    }

    BoundFunction* BoundFunction::New(ScriptContext* scriptContext, ArgumentReader args)
    {TRACE_IT(54434);
        Recycler* recycler = scriptContext->GetRecycler();

        BoundFunction* boundFunc = RecyclerNew(recycler, BoundFunction, args,
            scriptContext->GetLibrary()->GetBoundFunctionType());
        return boundFunc;
    }

    Var BoundFunction::NewInstance(RecyclableObject* function, CallInfo callInfo, ...)
    {
        RUNTIME_ARGUMENTS(args, callInfo);
        ScriptContext* scriptContext = function->GetScriptContext();

        if (args.Info.Count == 0)
        {TRACE_IT(54435);
            JavascriptError::ThrowTypeError(scriptContext, JSERR_NeedFunction /* TODO-ERROR: get arg name - args[0] */);
        }

        BoundFunction *boundFunction = (BoundFunction *) function;
        Var targetFunction = boundFunction->targetFunction;

        //
        // var o = new boundFunction()
        // a new object should be created using the actual function object
        //
        Var newVarInstance = nullptr;
        if (callInfo.Flags & CallFlags_New)
        {TRACE_IT(54436);
          if (JavascriptProxy::Is(targetFunction))
          {TRACE_IT(54437);
            JavascriptProxy* proxy = JavascriptProxy::FromVar(targetFunction);
            Arguments proxyArgs(CallInfo(CallFlags_New, 1), &targetFunction);
            args.Values[0] = newVarInstance = proxy->ConstructorTrap(proxyArgs, scriptContext, 0);
          }
          else
          {TRACE_IT(54438);
            args.Values[0] = newVarInstance = JavascriptOperators::NewScObjectNoCtor(targetFunction, scriptContext);
          }
        }

        Js::Arguments actualArgs = args;

        if (boundFunction->count > 0)
        {TRACE_IT(54439);
            BOOL isCrossSiteObject = boundFunction->IsCrossSiteObject();
            // OACR thinks that this can change between here and the check in the for loop below
            const unsigned int argCount = args.Info.Count;

            if ((boundFunction->count + argCount) > CallInfo::kMaxCountArgs)
            {TRACE_IT(54440);
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgListTooLarge);
            }

            Field(Var) *newValues = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), boundFunction->count + argCount);

            uint index = 0;

            //
            // For [[Construct]] use the newly created var instance
            // For [[Call]] use the "this" to which bind bound it.
            //
            if (callInfo.Flags & CallFlags_New)
            {TRACE_IT(54441);
                newValues[index++] = args[0];
            }
            else
            {TRACE_IT(54442);
                newValues[index++] = boundFunction->boundThis;
            }

            // Copy the bound args
            if (!isCrossSiteObject)
            {TRACE_IT(54443);
                for (uint i = 0; i < boundFunction->count; i++)
                {TRACE_IT(54444);
                    newValues[index++] = boundFunction->boundArgs[i];
                }
            }
            else
            {TRACE_IT(54445);
                // it is possible that the bound arguments are not marshalled yet.
                for (uint i = 0; i < boundFunction->count; i++)
                {TRACE_IT(54446);
                    //warning C6386: Buffer overrun while writing to 'newValues':  the writable size is 'boundFunction->count+argCount*8' bytes, but '40' bytes might be written.
                    // there's throw with args.Info.Count == 0, so here won't hit buffer overrun, and __analyze_assume(argCount>0) does not work
#pragma warning(suppress: 6386)
                    newValues[index++] = CrossSite::MarshalVar(scriptContext, boundFunction->boundArgs[i]);
                }
            }

            // Copy the extra args
            for (uint i=1; i<argCount; i++)
            {TRACE_IT(54447);
                newValues[index++] = args[i];
            }

            actualArgs = Arguments(args.Info, (Var*)newValues);
            actualArgs.Info.Count = boundFunction->count + argCount;
        }
        else
        {TRACE_IT(54448);
            if (!(callInfo.Flags & CallFlags_New))
            {TRACE_IT(54449);
                actualArgs.Values[0] = boundFunction->boundThis;
            }
        }

        RecyclableObject* actualFunction = RecyclableObject::FromVar(targetFunction);
        Var aReturnValue = JavascriptFunction::CallFunction<true>(actualFunction, actualFunction->GetEntryPoint(), actualArgs);

        //
        // [[Construct]] and call returned a non-object
        // return the newly created var instance
        //
        if ((callInfo.Flags & CallFlags_New) && !JavascriptOperators::IsObject(aReturnValue))
        {TRACE_IT(54450);
            aReturnValue = newVarInstance;
        }

        return aReturnValue;
    }

    void BoundFunction::MarshalToScriptContext(Js::ScriptContext * scriptContext)
    {TRACE_IT(54451);
        Assert(this->GetScriptContext() != scriptContext);
        AssertMsg(VirtualTableInfo<BoundFunction>::HasVirtualTable(this), "Derived class need to define marshal to script context");
        VirtualTableInfo<Js::CrossSiteObject<BoundFunction>>::SetVirtualTable(this);
        this->targetFunction = (RecyclableObject*)CrossSite::MarshalVar(scriptContext, this->targetFunction);
        this->boundThis = (RecyclableObject*)CrossSite::MarshalVar(this->GetScriptContext(), this->boundThis);
        for (uint i = 0; i < count; i++)
        {TRACE_IT(54452);
            this->boundArgs[i] = CrossSite::MarshalVar(this->GetScriptContext(), this->boundArgs[i]);
        }
    }

#if ENABLE_TTD
    void BoundFunction::MarshalCrossSite_TTDInflate()
    {TRACE_IT(54453);
        AssertMsg(VirtualTableInfo<BoundFunction>::HasVirtualTable(this), "Derived class need to define marshal");
        VirtualTableInfo<Js::CrossSiteObject<BoundFunction>>::SetVirtualTable(this);
    }
#endif

    JavascriptFunction * BoundFunction::GetTargetFunction() const
    {TRACE_IT(54454);
        if (targetFunction != nullptr)
        {TRACE_IT(54455);
            RecyclableObject* _targetFunction = targetFunction;
            while (JavascriptProxy::Is(_targetFunction))
            {TRACE_IT(54456);
                _targetFunction = JavascriptProxy::FromVar(_targetFunction)->GetTarget();
            }

            if (JavascriptFunction::Is(_targetFunction))
            {TRACE_IT(54457);
                return JavascriptFunction::FromVar(_targetFunction);
            }

            // targetFunction should always be a JavascriptFunction.
            Assert(FALSE);
        }
        return nullptr;
    }

    JavascriptString* BoundFunction::GetDisplayNameImpl() const
    {TRACE_IT(54458);
        JavascriptString* displayName = GetLibrary()->GetEmptyString();
        if (targetFunction != nullptr)
        {TRACE_IT(54459);
            Var value = JavascriptOperators::GetProperty(targetFunction, PropertyIds::name, targetFunction->GetScriptContext());
            if (JavascriptString::Is(value))
            {TRACE_IT(54460);
                displayName = JavascriptString::FromVar(value);
            }
        }
        return LiteralString::Concat(LiteralString::NewCopySz(_u("bound "), this->GetScriptContext()), displayName);
    }

    RecyclableObject* BoundFunction::GetBoundThis()
    {TRACE_IT(54461);
        if (boundThis != nullptr && RecyclableObject::Is(boundThis))
        {TRACE_IT(54462);
            return RecyclableObject::FromVar(boundThis);
        }
        return NULL;
    }

    inline BOOL BoundFunction::IsConstructor() const
    {TRACE_IT(54463);
        if (this->targetFunction != nullptr)
        {TRACE_IT(54464);
            return JavascriptOperators::IsConstructor(this->GetTargetFunction());
        }

        return false;
    }

    BOOL BoundFunction::HasProperty(PropertyId propertyId)
    {TRACE_IT(54465);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(54466);
            return true;
        }

        return JavascriptFunction::HasProperty(propertyId);
    }

    BOOL BoundFunction::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(54467);
        BOOL result;
        if (GetPropertyBuiltIns(originalInstance, propertyId, value, info, requestContext, &result))
        {TRACE_IT(54468);
            return result;
        }

        return JavascriptFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL BoundFunction::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(54469);
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext, &result))
        {TRACE_IT(54470);
            return result;
        }

        return JavascriptFunction::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool BoundFunction::GetPropertyBuiltIns(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext, BOOL* result)
    {TRACE_IT(54471);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(54472);
            // Get the "length" property of the underlying target function
            int len = 0;
            Var varLength;
            if (targetFunction->GetProperty(targetFunction, PropertyIds::length, &varLength, nullptr, requestContext))
            {TRACE_IT(54473);
                len = JavascriptConversion::ToInt32(varLength, requestContext);
            }

            // Reduce by number of bound args
            len = len - this->count;
            len = max(len, 0);

            *value = JavascriptNumber::ToVar(len, requestContext);
            *result = true;
            return true;
        }

        return false;
    }

    BOOL BoundFunction::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(54474);
        return BoundFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL BoundFunction::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(54475);
        BOOL result;
        if (SetPropertyBuiltIns(propertyId, value, flags, info, &result))
        {TRACE_IT(54476);
            return result;
        }

        return JavascriptFunction::SetProperty(propertyId, value, flags, info);
    }

    BOOL BoundFunction::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(54477);
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && SetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, flags, info, &result))
        {TRACE_IT(54478);
            return result;
        }

        return JavascriptFunction::SetProperty(propertyNameString, value, flags, info);
    }

    bool BoundFunction::SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, BOOL* result)
    {TRACE_IT(54479);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(54480);
            JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

            *result = false;
            return true;
        }

        return false;
    }

    BOOL BoundFunction::GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext)
    {TRACE_IT(54481);
        return DynamicObject::GetAccessors(propertyId, getter, setter, requestContext);
    }

    DescriptorFlags BoundFunction::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(54482);
        return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags BoundFunction::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(54483);
        return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    BOOL BoundFunction::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(54484);
        return SetProperty(propertyId, value, PropertyOperation_None, info);
    }

    BOOL BoundFunction::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(54485);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(54486);
            return false;
        }

        return JavascriptFunction::DeleteProperty(propertyId, flags);
    }

    BOOL BoundFunction::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(54487);
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {TRACE_IT(54488);
            return false;
        }

        return JavascriptFunction::DeleteProperty(propertyNameString, flags);
    }

    BOOL BoundFunction::IsWritable(PropertyId propertyId)
    {TRACE_IT(54489);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(54490);
            return false;
        }

        return JavascriptFunction::IsWritable(propertyId);
    }

    BOOL BoundFunction::IsConfigurable(PropertyId propertyId)
    {TRACE_IT(54491);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(54492);
            return false;
        }

        return JavascriptFunction::IsConfigurable(propertyId);
    }

    BOOL BoundFunction::IsEnumerable(PropertyId propertyId)
    {TRACE_IT(54493);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(54494);
            return false;
        }

        return JavascriptFunction::IsEnumerable(propertyId);
    }

    BOOL BoundFunction::HasInstance(Var instance, ScriptContext* scriptContext, IsInstInlineCache* inlineCache)
    {TRACE_IT(54495);
        return this->targetFunction->HasInstance(instance, scriptContext, inlineCache);
    }

#if ENABLE_TTD
    void BoundFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(54496);
        extractor->MarkVisitVar(this->targetFunction);

        if(this->boundThis != nullptr)
        {TRACE_IT(54497);
            extractor->MarkVisitVar(this->boundThis);
        }

        for(uint32 i = 0; i < this->count; ++i)
        {TRACE_IT(54498);
            extractor->MarkVisitVar(this->boundArgs[i]);
        }
    }

    void BoundFunction::ProcessCorePaths()
    {TRACE_IT(54499);
        this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->targetFunction, _u("!targetFunction"));
        this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->boundThis, _u("!boundThis"));

        TTDAssert(this->count == 0, "Should only have empty args in core image");
    }

    TTD::NSSnapObjects::SnapObjectType BoundFunction::GetSnapTag_TTD() const
    {TRACE_IT(54500);
        return TTD::NSSnapObjects::SnapObjectType::SnapBoundFunctionObject;
    }

    void BoundFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(54501);
        TTD::NSSnapObjects::SnapBoundFunctionInfo* bfi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapBoundFunctionInfo>();

        bfi->TargetFunction = TTD_CONVERT_VAR_TO_PTR_ID(static_cast<RecyclableObject*>(this->targetFunction));
        bfi->BoundThis = (this->boundThis != nullptr) ?
            TTD_CONVERT_VAR_TO_PTR_ID(static_cast<Var>(this->boundThis)) : TTD_INVALID_PTR_ID;

        bfi->ArgCount = this->count;
        bfi->ArgArray = nullptr;

        if(bfi->ArgCount > 0)
        {TRACE_IT(54502);
            bfi->ArgArray = alloc.SlabAllocateArray<TTD::TTDVar>(bfi->ArgCount);
        }

        TTD_PTR_ID* depArray = alloc.SlabReserveArraySpace<TTD_PTR_ID>(bfi->ArgCount + 2 /*this and bound function*/);

        depArray[0] = bfi->TargetFunction;
        uint32 depCount = 1;

        if(this->boundThis != nullptr && TTD::JsSupport::IsVarComplexKind(this->boundThis))
        {TRACE_IT(54503);
            depArray[depCount] = bfi->BoundThis;
            depCount++;
        }

        if(bfi->ArgCount > 0)
        {TRACE_IT(54504);
            for(uint32 i = 0; i < bfi->ArgCount; ++i)
            {TRACE_IT(54505);
                bfi->ArgArray[i] = this->boundArgs[i];

                //Primitive kinds always inflated first so we only need to deal with complex kinds as depends on
                if(TTD::JsSupport::IsVarComplexKind(this->boundArgs[i]))
                {TRACE_IT(54506);
                    depArray[depCount] = TTD_CONVERT_VAR_TO_PTR_ID(this->boundArgs[i]);
                    depCount++;
                }
            }
        }
        alloc.SlabCommitArraySpace<TTD_PTR_ID>(depCount, depCount + bfi->ArgCount);

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapBoundFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapBoundFunctionObject>(objData, bfi, alloc, depCount, depArray);
    }

    BoundFunction* BoundFunction::InflateBoundFunction(ScriptContext* ctx, RecyclableObject* function, Var bThis, uint32 ct, Var* args)
    {TRACE_IT(54507);
        BoundFunction* res = RecyclerNew(ctx->GetRecycler(), BoundFunction, ctx->GetLibrary()->GetBoundFunctionType());

        res->boundThis = bThis;
        res->count = ct;
        res->boundArgs = (Field(Var)*)args;

        res->targetFunction = function;

        return res;
    }
#endif
} // namespace Js
