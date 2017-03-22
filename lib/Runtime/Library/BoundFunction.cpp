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
    {LOGMEIN("BoundFunction.cpp] 16\n");
        // Constructor used during copy on write.
        DebugOnly(VerifyEntryPoint());
    }

    BoundFunction::BoundFunction(Arguments args, DynamicType * type)
        : JavascriptFunction(type, &functionInfo),
        count(0),
        boundArgs(nullptr)
    {LOGMEIN("BoundFunction.cpp] 25\n");

        DebugOnly(VerifyEntryPoint());
        AssertMsg(args.Info.Count > 0, "wrong number of args in BoundFunction");

        ScriptContext *scriptContext = this->GetScriptContext();
        targetFunction = RecyclableObject::FromVar(args[0]);

        // Let proto be targetFunction.[[GetPrototypeOf]]().
        RecyclableObject* proto = JavascriptOperators::GetPrototype(targetFunction);
        if (proto != type->GetPrototype())
        {LOGMEIN("BoundFunction.cpp] 36\n");
            if (type->GetIsShared())
            {LOGMEIN("BoundFunction.cpp] 38\n");
                this->ChangeType();
                type = this->GetDynamicType();
            }
            type->SetPrototype(proto);
        }
        // If targetFunction is proxy, need to make sure that traps are called in right order as per 19.2.3.2 in RC#4 dated April 3rd 2015.
        // Here although we won't use value of length, this is just to make sure that we call traps involved with HasOwnProperty(Target, "length") and Get(Target, "length")
        if (JavascriptProxy::Is(targetFunction))
        {LOGMEIN("BoundFunction.cpp] 47\n");
            if (JavascriptOperators::HasOwnProperty(targetFunction, PropertyIds::length, scriptContext) == TRUE)
            {LOGMEIN("BoundFunction.cpp] 49\n");
                int len = 0;
                Var varLength;
                if (targetFunction->GetProperty(targetFunction, PropertyIds::length, &varLength, nullptr, scriptContext))
                {LOGMEIN("BoundFunction.cpp] 53\n");
                    len = JavascriptConversion::ToInt32(varLength, scriptContext);
                }
            }
            GetTypeHandler()->EnsureObjectReady(this);
        }

        if (args.Info.Count > 1)
        {LOGMEIN("BoundFunction.cpp] 61\n");
            boundThis = args[1];

            // function object and "this" arg
            const uint countAccountedFor = 2;
            count = args.Info.Count - countAccountedFor;

            // Store the args excluding function obj and "this" arg
            if (args.Info.Count > 2)
            {LOGMEIN("BoundFunction.cpp] 70\n");
                boundArgs = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), count);

                for (uint i=0; i<count; i++)
                {LOGMEIN("BoundFunction.cpp] 74\n");
                    boundArgs[i] = args[i+countAccountedFor];
                }
            }
        }
        else
        {
            // If no "this" is passed, "undefined" is used
            boundThis = scriptContext->GetLibrary()->GetUndefined();
        }
    }

    BoundFunction::BoundFunction(RecyclableObject* targetFunction, Var boundThis, Var* args, uint argsCount, DynamicType * type)
        : JavascriptFunction(type, &functionInfo),
        count(argsCount),
        boundArgs(nullptr)
    {LOGMEIN("BoundFunction.cpp] 90\n");
        DebugOnly(VerifyEntryPoint());

        this->targetFunction = targetFunction;
        this->boundThis = boundThis;

        if (argsCount != 0)
        {LOGMEIN("BoundFunction.cpp] 97\n");
            this->boundArgs = RecyclerNewArray(this->GetScriptContext()->GetRecycler(), Field(Var), argsCount);

            for (uint i = 0; i < argsCount; i++)
            {LOGMEIN("BoundFunction.cpp] 101\n");
                this->boundArgs[i] = args[i];
            }
        }
    }

    BoundFunction* BoundFunction::New(ScriptContext* scriptContext, ArgumentReader args)
    {LOGMEIN("BoundFunction.cpp] 108\n");
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
        {LOGMEIN("BoundFunction.cpp] 122\n");
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
        {LOGMEIN("BoundFunction.cpp] 135\n");
          if (JavascriptProxy::Is(targetFunction))
          {LOGMEIN("BoundFunction.cpp] 137\n");
            JavascriptProxy* proxy = JavascriptProxy::FromVar(targetFunction);
            Arguments proxyArgs(CallInfo(CallFlags_New, 1), &targetFunction);
            args.Values[0] = newVarInstance = proxy->ConstructorTrap(proxyArgs, scriptContext, 0);
          }
          else
          {
            args.Values[0] = newVarInstance = JavascriptOperators::NewScObjectNoCtor(targetFunction, scriptContext);
          }
        }

        Js::Arguments actualArgs = args;

        if (boundFunction->count > 0)
        {LOGMEIN("BoundFunction.cpp] 151\n");
            BOOL isCrossSiteObject = boundFunction->IsCrossSiteObject();
            // OACR thinks that this can change between here and the check in the for loop below
            const unsigned int argCount = args.Info.Count;

            if ((boundFunction->count + argCount) > CallInfo::kMaxCountArgs)
            {LOGMEIN("BoundFunction.cpp] 157\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArgListTooLarge);
            }

            Field(Var) *newValues = RecyclerNewArray(scriptContext->GetRecycler(), Field(Var), boundFunction->count + argCount);

            uint index = 0;

            //
            // For [[Construct]] use the newly created var instance
            // For [[Call]] use the "this" to which bind bound it.
            //
            if (callInfo.Flags & CallFlags_New)
            {LOGMEIN("BoundFunction.cpp] 170\n");
                newValues[index++] = args[0];
            }
            else
            {
                newValues[index++] = boundFunction->boundThis;
            }

            // Copy the bound args
            if (!isCrossSiteObject)
            {LOGMEIN("BoundFunction.cpp] 180\n");
                for (uint i = 0; i < boundFunction->count; i++)
                {LOGMEIN("BoundFunction.cpp] 182\n");
                    newValues[index++] = boundFunction->boundArgs[i];
                }
            }
            else
            {
                // it is possible that the bound arguments are not marshalled yet.
                for (uint i = 0; i < boundFunction->count; i++)
                {LOGMEIN("BoundFunction.cpp] 190\n");
                    //warning C6386: Buffer overrun while writing to 'newValues':  the writable size is 'boundFunction->count+argCount*8' bytes, but '40' bytes might be written.
                    // there's throw with args.Info.Count == 0, so here won't hit buffer overrun, and __analyze_assume(argCount>0) does not work
#pragma warning(suppress: 6386)
                    newValues[index++] = CrossSite::MarshalVar(scriptContext, boundFunction->boundArgs[i]);
                }
            }

            // Copy the extra args
            for (uint i=1; i<argCount; i++)
            {LOGMEIN("BoundFunction.cpp] 200\n");
                newValues[index++] = args[i];
            }

            actualArgs = Arguments(args.Info, (Var*)newValues);
            actualArgs.Info.Count = boundFunction->count + argCount;
        }
        else
        {
            if (!(callInfo.Flags & CallFlags_New))
            {LOGMEIN("BoundFunction.cpp] 210\n");
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
        {LOGMEIN("BoundFunction.cpp] 223\n");
            aReturnValue = newVarInstance;
        }

        return aReturnValue;
    }

    void BoundFunction::MarshalToScriptContext(Js::ScriptContext * scriptContext)
    {LOGMEIN("BoundFunction.cpp] 231\n");
        Assert(this->GetScriptContext() != scriptContext);
        AssertMsg(VirtualTableInfo<BoundFunction>::HasVirtualTable(this), "Derived class need to define marshal to script context");
        VirtualTableInfo<Js::CrossSiteObject<BoundFunction>>::SetVirtualTable(this);
        this->targetFunction = (RecyclableObject*)CrossSite::MarshalVar(scriptContext, this->targetFunction);
        this->boundThis = (RecyclableObject*)CrossSite::MarshalVar(this->GetScriptContext(), this->boundThis);
        for (uint i = 0; i < count; i++)
        {LOGMEIN("BoundFunction.cpp] 238\n");
            this->boundArgs[i] = CrossSite::MarshalVar(this->GetScriptContext(), this->boundArgs[i]);
        }
    }

#if ENABLE_TTD
    void BoundFunction::MarshalCrossSite_TTDInflate()
    {LOGMEIN("BoundFunction.cpp] 245\n");
        AssertMsg(VirtualTableInfo<BoundFunction>::HasVirtualTable(this), "Derived class need to define marshal");
        VirtualTableInfo<Js::CrossSiteObject<BoundFunction>>::SetVirtualTable(this);
    }
#endif

    JavascriptFunction * BoundFunction::GetTargetFunction() const
    {LOGMEIN("BoundFunction.cpp] 252\n");
        if (targetFunction != nullptr)
        {LOGMEIN("BoundFunction.cpp] 254\n");
            RecyclableObject* _targetFunction = targetFunction;
            while (JavascriptProxy::Is(_targetFunction))
            {LOGMEIN("BoundFunction.cpp] 257\n");
                _targetFunction = JavascriptProxy::FromVar(_targetFunction)->GetTarget();
            }

            if (JavascriptFunction::Is(_targetFunction))
            {LOGMEIN("BoundFunction.cpp] 262\n");
                return JavascriptFunction::FromVar(_targetFunction);
            }

            // targetFunction should always be a JavascriptFunction.
            Assert(FALSE);
        }
        return nullptr;
    }

    JavascriptString* BoundFunction::GetDisplayNameImpl() const
    {LOGMEIN("BoundFunction.cpp] 273\n");
        JavascriptString* displayName = GetLibrary()->GetEmptyString();
        if (targetFunction != nullptr)
        {LOGMEIN("BoundFunction.cpp] 276\n");
            Var value = JavascriptOperators::GetProperty(targetFunction, PropertyIds::name, targetFunction->GetScriptContext());
            if (JavascriptString::Is(value))
            {LOGMEIN("BoundFunction.cpp] 279\n");
                displayName = JavascriptString::FromVar(value);
            }
        }
        return LiteralString::Concat(LiteralString::NewCopySz(_u("bound "), this->GetScriptContext()), displayName);
    }

    RecyclableObject* BoundFunction::GetBoundThis()
    {LOGMEIN("BoundFunction.cpp] 287\n");
        if (boundThis != nullptr && RecyclableObject::Is(boundThis))
        {LOGMEIN("BoundFunction.cpp] 289\n");
            return RecyclableObject::FromVar(boundThis);
        }
        return NULL;
    }

    inline BOOL BoundFunction::IsConstructor() const
    {LOGMEIN("BoundFunction.cpp] 296\n");
        if (this->targetFunction != nullptr)
        {LOGMEIN("BoundFunction.cpp] 298\n");
            return JavascriptOperators::IsConstructor(this->GetTargetFunction());
        }

        return false;
    }

    BOOL BoundFunction::HasProperty(PropertyId propertyId)
    {LOGMEIN("BoundFunction.cpp] 306\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("BoundFunction.cpp] 308\n");
            return true;
        }

        return JavascriptFunction::HasProperty(propertyId);
    }

    BOOL BoundFunction::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("BoundFunction.cpp] 316\n");
        BOOL result;
        if (GetPropertyBuiltIns(originalInstance, propertyId, value, info, requestContext, &result))
        {LOGMEIN("BoundFunction.cpp] 319\n");
            return result;
        }

        return JavascriptFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL BoundFunction::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("BoundFunction.cpp] 327\n");
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext, &result))
        {LOGMEIN("BoundFunction.cpp] 333\n");
            return result;
        }

        return JavascriptFunction::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool BoundFunction::GetPropertyBuiltIns(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext, BOOL* result)
    {LOGMEIN("BoundFunction.cpp] 341\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("BoundFunction.cpp] 343\n");
            // Get the "length" property of the underlying target function
            int len = 0;
            Var varLength;
            if (targetFunction->GetProperty(targetFunction, PropertyIds::length, &varLength, nullptr, requestContext))
            {LOGMEIN("BoundFunction.cpp] 348\n");
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
    {LOGMEIN("BoundFunction.cpp] 365\n");
        return BoundFunction::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL BoundFunction::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("BoundFunction.cpp] 370\n");
        BOOL result;
        if (SetPropertyBuiltIns(propertyId, value, flags, info, &result))
        {LOGMEIN("BoundFunction.cpp] 373\n");
            return result;
        }

        return JavascriptFunction::SetProperty(propertyId, value, flags, info);
    }

    BOOL BoundFunction::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("BoundFunction.cpp] 381\n");
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && SetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, flags, info, &result))
        {LOGMEIN("BoundFunction.cpp] 387\n");
            return result;
        }

        return JavascriptFunction::SetProperty(propertyNameString, value, flags, info);
    }

    bool BoundFunction::SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info, BOOL* result)
    {LOGMEIN("BoundFunction.cpp] 395\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("BoundFunction.cpp] 397\n");
            JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

            *result = false;
            return true;
        }

        return false;
    }

    BOOL BoundFunction::GetAccessors(PropertyId propertyId, Var *getter, Var *setter, ScriptContext * requestContext)
    {LOGMEIN("BoundFunction.cpp] 408\n");
        return DynamicObject::GetAccessors(propertyId, getter, setter, requestContext);
    }

    DescriptorFlags BoundFunction::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("BoundFunction.cpp] 413\n");
        return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags BoundFunction::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("BoundFunction.cpp] 418\n");
        return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    BOOL BoundFunction::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("BoundFunction.cpp] 423\n");
        return SetProperty(propertyId, value, PropertyOperation_None, info);
    }

    BOOL BoundFunction::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("BoundFunction.cpp] 428\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("BoundFunction.cpp] 430\n");
            return false;
        }

        return JavascriptFunction::DeleteProperty(propertyId, flags);
    }

    BOOL BoundFunction::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("BoundFunction.cpp] 438\n");
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {LOGMEIN("BoundFunction.cpp] 441\n");
            return false;
        }

        return JavascriptFunction::DeleteProperty(propertyNameString, flags);
    }

    BOOL BoundFunction::IsWritable(PropertyId propertyId)
    {LOGMEIN("BoundFunction.cpp] 449\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("BoundFunction.cpp] 451\n");
            return false;
        }

        return JavascriptFunction::IsWritable(propertyId);
    }

    BOOL BoundFunction::IsConfigurable(PropertyId propertyId)
    {LOGMEIN("BoundFunction.cpp] 459\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("BoundFunction.cpp] 461\n");
            return false;
        }

        return JavascriptFunction::IsConfigurable(propertyId);
    }

    BOOL BoundFunction::IsEnumerable(PropertyId propertyId)
    {LOGMEIN("BoundFunction.cpp] 469\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("BoundFunction.cpp] 471\n");
            return false;
        }

        return JavascriptFunction::IsEnumerable(propertyId);
    }

    BOOL BoundFunction::HasInstance(Var instance, ScriptContext* scriptContext, IsInstInlineCache* inlineCache)
    {LOGMEIN("BoundFunction.cpp] 479\n");
        return this->targetFunction->HasInstance(instance, scriptContext, inlineCache);
    }

#if ENABLE_TTD
    void BoundFunction::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("BoundFunction.cpp] 485\n");
        extractor->MarkVisitVar(this->targetFunction);

        if(this->boundThis != nullptr)
        {LOGMEIN("BoundFunction.cpp] 489\n");
            extractor->MarkVisitVar(this->boundThis);
        }

        for(uint32 i = 0; i < this->count; ++i)
        {LOGMEIN("BoundFunction.cpp] 494\n");
            extractor->MarkVisitVar(this->boundArgs[i]);
        }
    }

    void BoundFunction::ProcessCorePaths()
    {LOGMEIN("BoundFunction.cpp] 500\n");
        this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->targetFunction, _u("!targetFunction"));
        this->GetScriptContext()->TTDWellKnownInfo->EnqueueNewPathVarAsNeeded(this, this->boundThis, _u("!boundThis"));

        TTDAssert(this->count == 0, "Should only have empty args in core image");
    }

    TTD::NSSnapObjects::SnapObjectType BoundFunction::GetSnapTag_TTD() const
    {LOGMEIN("BoundFunction.cpp] 508\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapBoundFunctionObject;
    }

    void BoundFunction::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("BoundFunction.cpp] 513\n");
        TTD::NSSnapObjects::SnapBoundFunctionInfo* bfi = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapBoundFunctionInfo>();

        bfi->TargetFunction = TTD_CONVERT_VAR_TO_PTR_ID(static_cast<RecyclableObject*>(this->targetFunction));
        bfi->BoundThis = (this->boundThis != nullptr) ?
            TTD_CONVERT_VAR_TO_PTR_ID(static_cast<Var>(this->boundThis)) : TTD_INVALID_PTR_ID;

        bfi->ArgCount = this->count;
        bfi->ArgArray = nullptr;

        if(bfi->ArgCount > 0)
        {LOGMEIN("BoundFunction.cpp] 524\n");
            bfi->ArgArray = alloc.SlabAllocateArray<TTD::TTDVar>(bfi->ArgCount);
        }

        TTD_PTR_ID* depArray = alloc.SlabReserveArraySpace<TTD_PTR_ID>(bfi->ArgCount + 2 /*this and bound function*/);

        depArray[0] = bfi->TargetFunction;
        uint32 depCount = 1;

        if(this->boundThis != nullptr && TTD::JsSupport::IsVarComplexKind(this->boundThis))
        {LOGMEIN("BoundFunction.cpp] 534\n");
            depArray[depCount] = bfi->BoundThis;
            depCount++;
        }

        if(bfi->ArgCount > 0)
        {LOGMEIN("BoundFunction.cpp] 540\n");
            for(uint32 i = 0; i < bfi->ArgCount; ++i)
            {LOGMEIN("BoundFunction.cpp] 542\n");
                bfi->ArgArray[i] = this->boundArgs[i];

                //Primitive kinds always inflated first so we only need to deal with complex kinds as depends on
                if(TTD::JsSupport::IsVarComplexKind(this->boundArgs[i]))
                {LOGMEIN("BoundFunction.cpp] 547\n");
                    depArray[depCount] = TTD_CONVERT_VAR_TO_PTR_ID(this->boundArgs[i]);
                    depCount++;
                }
            }
        }
        alloc.SlabCommitArraySpace<TTD_PTR_ID>(depCount, depCount + bfi->ArgCount);

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapBoundFunctionInfo*, TTD::NSSnapObjects::SnapObjectType::SnapBoundFunctionObject>(objData, bfi, alloc, depCount, depArray);
    }

    BoundFunction* BoundFunction::InflateBoundFunction(ScriptContext* ctx, RecyclableObject* function, Var bThis, uint32 ct, Var* args)
    {LOGMEIN("BoundFunction.cpp] 559\n");
        BoundFunction* res = RecyclerNew(ctx->GetRecycler(), BoundFunction, ctx->GetLibrary()->GetBoundFunctionType());

        res->boundThis = bThis;
        res->count = ct;
        res->boundArgs = (Field(Var)*)args;

        res->targetFunction = function;

        return res;
    }
#endif
} // namespace Js
