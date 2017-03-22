//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "Library/ArgumentsObjectEnumerator.h"

namespace Js
{
    BOOL ArgumentsObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("ArgumentsObject.cpp] 11\n");
        return GetEnumeratorWithPrefix(
            RecyclerNew(GetScriptContext()->GetRecycler(), ArgumentsObjectPrefixEnumerator, this, flags, requestContext),
            enumerator, flags, requestContext, forInCache);
    }

    BOOL ArgumentsObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 18\n");
        stringBuilder->AppendCppLiteral(_u("{...}"));
        return TRUE;
    }

    BOOL ArgumentsObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 24\n");
        stringBuilder->AppendCppLiteral(_u("Object, (Arguments)"));
        return TRUE;
    }

    Var ArgumentsObject::GetCaller(ScriptContext * scriptContext)
    {LOGMEIN("ArgumentsObject.cpp] 30\n");
        JavascriptStackWalker walker(scriptContext);

        if (!this->AdvanceWalkerToArgsFrame(&walker))
        {LOGMEIN("ArgumentsObject.cpp] 34\n");
            return scriptContext->GetLibrary()->GetNull();
        }

        return ArgumentsObject::GetCaller(scriptContext, &walker, false);
    }

    Var ArgumentsObject::GetCaller(ScriptContext * scriptContext, JavascriptStackWalker *walker, bool skipGlobal)
    {LOGMEIN("ArgumentsObject.cpp] 42\n");
        // The arguments.caller property is equivalent to callee.caller.arguments - that is, it's the
        // caller's arguments object (if any). Just fetch the caller and compute its arguments.
        JavascriptFunction* funcCaller = nullptr;

        while (walker->GetCaller(&funcCaller))
        {LOGMEIN("ArgumentsObject.cpp] 48\n");
            if (walker->IsCallerGlobalFunction())
            {LOGMEIN("ArgumentsObject.cpp] 50\n");
                // Caller is global/eval. If we're in IE9 mode, and the caller is eval,
                // keep looking. Otherwise, caller is null.
                if (skipGlobal || walker->IsEvalCaller())
                {LOGMEIN("ArgumentsObject.cpp] 54\n");
                    continue;
                }
                funcCaller = nullptr;
            }
            break;
        }

        if (funcCaller == nullptr || JavascriptOperators::GetTypeId(funcCaller) == TypeIds_Null)
        {LOGMEIN("ArgumentsObject.cpp] 63\n");
            return scriptContext->GetLibrary()->GetNull();
        }

        AssertMsg(JavascriptOperators::GetTypeId(funcCaller) == TypeIds_Function, "non function caller");

        CallInfo const *callInfo = walker->GetCallInfo();
        uint32 paramCount = callInfo->Count;
        CallFlags flags = callInfo->Flags;

        if (paramCount == 0 || (flags & CallFlags_Eval))
        {LOGMEIN("ArgumentsObject.cpp] 74\n");
            // The caller is the "global function" or eval, so we return "null".
            return scriptContext->GetLibrary()->GetNull();
        }

        if (!walker->GetCurrentFunction()->IsScriptFunction())
        {LOGMEIN("ArgumentsObject.cpp] 80\n");
            // builtin function do not have an argument object - return null.
            return scriptContext->GetLibrary()->GetNull();
        }

        // Create new arguments object, everytime this is requested for, with the actuals value.
        Var args = nullptr;

        args = JavascriptOperators::LoadHeapArguments(
            funcCaller,
            paramCount - 1,
            walker->GetJavascriptArgs(),
            scriptContext->GetLibrary()->GetNull(),
            scriptContext->GetLibrary()->GetNull(),
            scriptContext,
            /* formalsAreLetDecls */ false);

        return args;
    }

    bool ArgumentsObject::Is(Var aValue)
    {LOGMEIN("ArgumentsObject.cpp] 101\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Arguments;
    }

    HeapArgumentsObject::HeapArgumentsObject(DynamicType * type) : ArgumentsObject(type), frameObject(nullptr), formalCount(0),
        numOfArguments(0), callerDeleted(false), deletedArgs(nullptr)
    {LOGMEIN("ArgumentsObject.cpp] 107\n");
    }

    HeapArgumentsObject::HeapArgumentsObject(Recycler *recycler, ActivationObject* obj, uint32 formalCount, DynamicType * type)
        : ArgumentsObject(type), frameObject(obj), formalCount(formalCount), numOfArguments(0), callerDeleted(false), deletedArgs(nullptr)
    {LOGMEIN("ArgumentsObject.cpp] 112\n");
    }

    void HeapArgumentsObject::SetNumberOfArguments(uint32 len)
    {LOGMEIN("ArgumentsObject.cpp] 116\n");
        numOfArguments = len;
    }

    uint32 HeapArgumentsObject::GetNumberOfArguments() const
    {LOGMEIN("ArgumentsObject.cpp] 121\n");
        return numOfArguments;
    }

    HeapArgumentsObject* HeapArgumentsObject::As(Var aValue)
    {LOGMEIN("ArgumentsObject.cpp] 126\n");
        if (ArgumentsObject::Is(aValue) && static_cast<ArgumentsObject*>(RecyclableObject::FromVar(aValue))->GetHeapArguments() == aValue)
        {LOGMEIN("ArgumentsObject.cpp] 128\n");
            return static_cast<HeapArgumentsObject*>(RecyclableObject::FromVar(aValue));
        }
        return NULL;
    }

    BOOL HeapArgumentsObject::AdvanceWalkerToArgsFrame(JavascriptStackWalker *walker)
    {LOGMEIN("ArgumentsObject.cpp] 135\n");
        // Walk until we find this arguments object on the frame.
        // Note that each frame may have a HeapArgumentsObject
        // associated with it. Look for the HeapArgumentsObject.
        while (walker->Walk())
        {LOGMEIN("ArgumentsObject.cpp] 140\n");
            if (walker->IsJavascriptFrame() && walker->GetCurrentFunction()->IsScriptFunction())
            {LOGMEIN("ArgumentsObject.cpp] 142\n");
                Var args = walker->GetPermanentArguments();
                if (args == this)
                {LOGMEIN("ArgumentsObject.cpp] 145\n");
                    return TRUE;
                }
            }
        }

        return FALSE;
    }

    //
    // Get the next valid formal arg index held in this object.
    //
    uint32 HeapArgumentsObject::GetNextFormalArgIndex(uint32 index, BOOL enumNonEnumerable, PropertyAttributes* attributes) const
    {LOGMEIN("ArgumentsObject.cpp] 158\n");
        if (attributes != nullptr)
        {LOGMEIN("ArgumentsObject.cpp] 160\n");
            *attributes = PropertyEnumerable;
        }

        if ( ++index < formalCount )
        {LOGMEIN("ArgumentsObject.cpp] 165\n");
            // None of the arguments deleted
            if ( deletedArgs == nullptr )
            {LOGMEIN("ArgumentsObject.cpp] 168\n");
                return index;
            }

            while ( index < formalCount )
            {LOGMEIN("ArgumentsObject.cpp] 173\n");
                if (!this->deletedArgs->Test(index))
                {LOGMEIN("ArgumentsObject.cpp] 175\n");
                    return index;
                }

                index++;
            }
        }

        return JavascriptArray::InvalidIndex;
    }

    BOOL HeapArgumentsObject::HasItemAt(uint32 index)
    {LOGMEIN("ArgumentsObject.cpp] 187\n");
        // If this arg index is bound to a named formal argument, get it from the local frame.
        // If not, report this fact to the caller, which will defer to the normal get-value-by-index means.
        if (IsFormalArgument(index) && (this->deletedArgs == nullptr || !this->deletedArgs->Test(index)) )
        {LOGMEIN("ArgumentsObject.cpp] 191\n");
            return true;
        }

        return false;
    }

    BOOL HeapArgumentsObject::GetItemAt(uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 199\n");
        // If this arg index is bound to a named formal argument, get it from the local frame.
        // If not, report this fact to the caller, which will defer to the normal get-value-by-index means.
        if (HasItemAt(index))
        {LOGMEIN("ArgumentsObject.cpp] 203\n");
            *value = this->frameObject->GetSlot(index);
            return true;
        }

        return false;
    }

    BOOL HeapArgumentsObject::SetItemAt(uint32 index, Var value)
    {LOGMEIN("ArgumentsObject.cpp] 212\n");
        // If this arg index is bound to a named formal argument, set it in the local frame.
        // If not, report this fact to the caller, which will defer to the normal set-value-by-index means.
        if (HasItemAt(index))
        {LOGMEIN("ArgumentsObject.cpp] 216\n");
            this->frameObject->SetSlot(SetSlotArguments(Constants::NoProperty, index, value));
            return true;
        }

        return false;
    }

    BOOL HeapArgumentsObject::DeleteItemAt(uint32 index)
    {LOGMEIN("ArgumentsObject.cpp] 225\n");
        if (index < formalCount)
        {LOGMEIN("ArgumentsObject.cpp] 227\n");
            if (this->deletedArgs == nullptr)
            {LOGMEIN("ArgumentsObject.cpp] 229\n");
                Recycler *recycler = GetScriptContext()->GetRecycler();
                deletedArgs = RecyclerNew(recycler, BVSparse<Recycler>, recycler);
            }

            if (!this->deletedArgs->Test(index))
            {LOGMEIN("ArgumentsObject.cpp] 235\n");
                this->deletedArgs->Set(index);
                return true;
            }
        }

        return false;
    }

    BOOL HeapArgumentsObject::IsFormalArgument(uint32 index)
    {LOGMEIN("ArgumentsObject.cpp] 245\n");
        return index < this->numOfArguments && index < formalCount;
    }

    BOOL HeapArgumentsObject::IsFormalArgument(PropertyId propertyId)
    {LOGMEIN("ArgumentsObject.cpp] 250\n");
        uint32 index;
        return IsFormalArgument(propertyId, &index);
    }

    BOOL HeapArgumentsObject::IsFormalArgument(PropertyId propertyId, uint32* pIndex)
    {LOGMEIN("ArgumentsObject.cpp] 256\n");
        return
            this->GetScriptContext()->IsNumericPropertyId(propertyId, pIndex) &&
            IsFormalArgument(*pIndex);
    }

    BOOL HeapArgumentsObject::IsArgumentDeleted(uint32 index) const
    {LOGMEIN("ArgumentsObject.cpp] 263\n");
        return this->deletedArgs != NULL && this->deletedArgs->Test(index);
    }

#if ENABLE_TTD
    void HeapArgumentsObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("ArgumentsObject.cpp] 269\n");
        if(this->frameObject != nullptr)
        {LOGMEIN("ArgumentsObject.cpp] 271\n");
            extractor->MarkVisitVar(this->frameObject);
        }
    }

    TTD::NSSnapObjects::SnapObjectType HeapArgumentsObject::GetSnapTag_TTD() const
    {LOGMEIN("ArgumentsObject.cpp] 277\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapHeapArgumentsObject;
    }

    void HeapArgumentsObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ArgumentsObject.cpp] 282\n");
        this->ExtractSnapObjectDataInto_Helper<TTD::NSSnapObjects::SnapObjectType::SnapHeapArgumentsObject>(objData, alloc);
    }

    template <TTD::NSSnapObjects::SnapObjectType argsKind>
    void HeapArgumentsObject::ExtractSnapObjectDataInto_Helper(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ArgumentsObject.cpp] 288\n");
        TTD::NSSnapObjects::SnapHeapArgumentsInfo* argsInfo = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapHeapArgumentsInfo>();

        TTDAssert(this->callerDeleted == 0, "This never seems to be set but I want to assert just to be safe.");
        argsInfo->NumOfArguments = this->numOfArguments;
        argsInfo->FormalCount = this->formalCount;

        uint32 depOnCount = 0;
        TTD_PTR_ID* depOnArray = nullptr;

        argsInfo->IsFrameNullPtr = false;
        argsInfo->IsFrameJsNull = false;
        argsInfo->FrameObject = TTD_INVALID_PTR_ID;
        if(this->frameObject == nullptr)
        {LOGMEIN("ArgumentsObject.cpp] 302\n");
            argsInfo->IsFrameNullPtr = true;
        }
        else if(Js::JavascriptOperators::GetTypeId(this->frameObject) == TypeIds_Null)
        {LOGMEIN("ArgumentsObject.cpp] 306\n");
            argsInfo->IsFrameJsNull = true;
        }
        else
        {
            argsInfo->FrameObject = TTD_CONVERT_VAR_TO_PTR_ID(
                static_cast<ActivationObject*>(this->frameObject));

            //Primitive kinds always inflated first so we only need to deal with complex kinds as depends on
            if(TTD::JsSupport::IsVarComplexKind(this->frameObject))
            {LOGMEIN("ArgumentsObject.cpp] 316\n");
                depOnCount = 1;
                depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(depOnCount);
                depOnArray[0] = argsInfo->FrameObject;
            }
        }

        argsInfo->DeletedArgFlags = (this->formalCount != 0) ? alloc.SlabAllocateArrayZ<byte>(argsInfo->FormalCount) : nullptr;
        if(this->deletedArgs != nullptr)
        {LOGMEIN("ArgumentsObject.cpp] 325\n");
            for(uint32 i = 0; i < this->formalCount; ++i)
            {LOGMEIN("ArgumentsObject.cpp] 327\n");
                if(this->deletedArgs->Test(i))
                {LOGMEIN("ArgumentsObject.cpp] 329\n");
                    argsInfo->DeletedArgFlags[i] = true;
                }
            }
        }

        if(depOnCount == 0)
        {LOGMEIN("ArgumentsObject.cpp] 336\n");
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapHeapArgumentsInfo*, argsKind>(objData, argsInfo);
        }
        else
        {
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapHeapArgumentsInfo*, argsKind>(objData, argsInfo, alloc, depOnCount, depOnArray);
        }
    }

    ES5HeapArgumentsObject* HeapArgumentsObject::ConvertToES5HeapArgumentsObject_TTD()
    {LOGMEIN("ArgumentsObject.cpp] 346\n");
        Assert(VirtualTableInfo<HeapArgumentsObject>::HasVirtualTable(this));
        VirtualTableInfo<ES5HeapArgumentsObject>::SetVirtualTable(this);

        return static_cast<ES5HeapArgumentsObject*>(this);
    }
#endif

    ES5HeapArgumentsObject* HeapArgumentsObject::ConvertToUnmappedArgumentsObject(bool overwriteArgsUsingFrameObject)
    {LOGMEIN("ArgumentsObject.cpp] 355\n");
        ES5HeapArgumentsObject* es5ArgsObj = ConvertToES5HeapArgumentsObject(overwriteArgsUsingFrameObject);

        for (uint i=0; i<this->formalCount && i<numOfArguments; ++i)
        {LOGMEIN("ArgumentsObject.cpp] 359\n");
            es5ArgsObj->DisconnectFormalFromNamedArgument(i);
        }

        return es5ArgsObj;
    }

    ES5HeapArgumentsObject* HeapArgumentsObject::ConvertToES5HeapArgumentsObject(bool overwriteArgsUsingFrameObject)
    {LOGMEIN("ArgumentsObject.cpp] 367\n");
        if (!CrossSite::IsCrossSiteObjectTyped(this))
        {LOGMEIN("ArgumentsObject.cpp] 369\n");
            Assert(VirtualTableInfo<HeapArgumentsObject>::HasVirtualTable(this));
            VirtualTableInfo<ES5HeapArgumentsObject>::SetVirtualTable(this);
        }
        else
        {
            Assert(VirtualTableInfo<CrossSiteObject<HeapArgumentsObject>>::HasVirtualTable(this));
            VirtualTableInfo<CrossSiteObject<ES5HeapArgumentsObject>>::SetVirtualTable(this);
        }

        ES5HeapArgumentsObject* es5This = static_cast<ES5HeapArgumentsObject*>(this);

        if (overwriteArgsUsingFrameObject)
        {LOGMEIN("ArgumentsObject.cpp] 382\n");
            // Copy existing items to the array so that they are there before the user can call Object.preventExtensions,
            // after which we can no longer add new items to the objectArray.
            for (uint32 i = 0; i < this->formalCount && i < this->numOfArguments; ++i)
            {LOGMEIN("ArgumentsObject.cpp] 386\n");
                AssertMsg(this->IsFormalArgument(i), "this->IsFormalArgument(i)");
                if (!this->IsArgumentDeleted(i))
                {LOGMEIN("ArgumentsObject.cpp] 389\n");
                    // At this time the value doesn't matter, use one from slots.
                    this->SetObjectArrayItem(i, this->frameObject->GetSlot(i), PropertyOperation_None);
                }
            }
        }
        return es5This;

    }

    BOOL HeapArgumentsObject::HasProperty(PropertyId id)
    {LOGMEIN("ArgumentsObject.cpp] 400\n");
        ScriptContext *scriptContext = GetScriptContext();

        // Try to get a numbered property that maps to an actual argument.
        uint32 index;
        if (scriptContext->IsNumericPropertyId(id, &index) && index < this->HeapArgumentsObject::GetNumberOfArguments())
        {LOGMEIN("ArgumentsObject.cpp] 406\n");
            return HeapArgumentsObject::HasItem(index);
        }

        return DynamicObject::HasProperty(id);
    }

    BOOL HeapArgumentsObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 414\n");
        ScriptContext *scriptContext = GetScriptContext();

        // Try to get a numbered property that maps to an actual argument.
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index) && index < this->HeapArgumentsObject::GetNumberOfArguments())
        {LOGMEIN("ArgumentsObject.cpp] 420\n");
            if (this->GetItemAt(index, value, requestContext))
            {LOGMEIN("ArgumentsObject.cpp] 422\n");
                return true;
            }
        }

        if (DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {LOGMEIN("ArgumentsObject.cpp] 428\n");
            // Property has been pre-set and not deleted. Use it.
            return true;
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL HeapArgumentsObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 438\n");

        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        if (DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext))
        {LOGMEIN("ArgumentsObject.cpp] 444\n");
            // Property has been pre-set and not deleted. Use it.
            return true;
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL HeapArgumentsObject::GetPropertyReference(Var originalInstance, PropertyId id, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 454\n");
        return HeapArgumentsObject::GetProperty(originalInstance, id, value, info, requestContext);
    }

    BOOL HeapArgumentsObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("ArgumentsObject.cpp] 459\n");
        // Try to set a numbered property that maps to an actual argument.
        ScriptContext *scriptContext = GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index) && index < this->HeapArgumentsObject::GetNumberOfArguments())
        {LOGMEIN("ArgumentsObject.cpp] 465\n");
            if (this->SetItemAt(index, value))
            {LOGMEIN("ArgumentsObject.cpp] 467\n");
                return true;
            }
        }

        // TODO: In strict mode, "callee" and "caller" cannot be set.

        // length is also set in the dynamic object
        return DynamicObject::SetProperty(propertyId, value, flags, info);
    }

    BOOL HeapArgumentsObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("ArgumentsObject.cpp] 479\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetSz(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        // TODO: In strict mode, "callee" and "caller" cannot be set.

        // length is also set in the dynamic object
        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    BOOL HeapArgumentsObject::HasItem(uint32 index)
    {LOGMEIN("ArgumentsObject.cpp] 490\n");
        if (this->HasItemAt(index))
        {LOGMEIN("ArgumentsObject.cpp] 492\n");
            return true;
        }
        return DynamicObject::HasItem(index);
    }

    BOOL HeapArgumentsObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 499\n");
        if (this->GetItemAt(index, value, requestContext))
        {LOGMEIN("ArgumentsObject.cpp] 501\n");
            return true;
        }
        return DynamicObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL HeapArgumentsObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 508\n");
        return HeapArgumentsObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL HeapArgumentsObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("ArgumentsObject.cpp] 513\n");
        if (this->SetItemAt(index, value))
        {LOGMEIN("ArgumentsObject.cpp] 515\n");
            return true;
        }
        return DynamicObject::SetItem(index, value, flags);
    }

    BOOL HeapArgumentsObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("ArgumentsObject.cpp] 522\n");
        if (this->DeleteItemAt(index))
        {LOGMEIN("ArgumentsObject.cpp] 524\n");
            return true;
        }
        return DynamicObject::DeleteItem(index, flags);
    }

    BOOL HeapArgumentsObject::SetConfigurable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 531\n");
        // Try to set a numbered property that maps to an actual argument.
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 535\n");
            // From now on, make sure that frame object is ES5HeapArgumentsObject.
            return this->ConvertToES5HeapArgumentsObject()->SetConfigurableForFormal(index, propertyId, value);
        }

        // Use 'this' dynamic object.
        // This will use type handler and convert its objectArray to ES5Array is not already converted.
        return __super::SetConfigurable(propertyId, value);
    }

    BOOL HeapArgumentsObject::SetEnumerable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 546\n");
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 549\n");
            return this->ConvertToES5HeapArgumentsObject()->SetEnumerableForFormal(index, propertyId, value);
        }
        return __super::SetEnumerable(propertyId, value);
    }

    BOOL HeapArgumentsObject::SetWritable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 556\n");
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 559\n");
            return this->ConvertToES5HeapArgumentsObject()->SetWritableForFormal(index, propertyId, value);
        }
        return __super::SetWritable(propertyId, value);
    }

    BOOL HeapArgumentsObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("ArgumentsObject.cpp] 566\n");
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 569\n");
            return this->ConvertToES5HeapArgumentsObject()->SetAccessorsForFormal(index, propertyId, getter, setter, flags);
        }
        return __super::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL HeapArgumentsObject::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {LOGMEIN("ArgumentsObject.cpp] 576\n");
        // This is called by defineProperty in order to change the value in objectArray.
        // We have to intercept this call because
        // in case when we are connected (objectArray item is not used) we need to update the slot value (which is actually used).
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 582\n");
            return this->ConvertToES5HeapArgumentsObject()->SetPropertyWithAttributesForFormal(
                index, propertyId, value, attributes, info, flags, possibleSideEffects);
        }
        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    // This disables adding new properties to the object.
    BOOL HeapArgumentsObject::PreventExtensions()
    {LOGMEIN("ArgumentsObject.cpp] 591\n");
        // It's possible that after call to preventExtensions, the user can change the attributes;
        // In this case if we don't covert to ES5 version now, later we would not be able to add items to objectArray,
        // which would cause not being able to change attributes.
        // So, convert to ES5 now which will make sure the items are there.
        return this->ConvertToES5HeapArgumentsObject()->PreventExtensions();
    }

    // This is equivalent to .preventExtensions semantics with addition of setting configurable to false for all properties.
    BOOL HeapArgumentsObject::Seal()
    {LOGMEIN("ArgumentsObject.cpp] 601\n");
        // Same idea as with PreventExtensions: we have to make sure that items in objectArray for formals
        // are there before seal, otherwise we will not be able to add them later.
        return this->ConvertToES5HeapArgumentsObject()->Seal();
    }

    // This is equivalent to .seal semantics with addition of setting writable to false for all properties.
    BOOL HeapArgumentsObject::Freeze()
    {LOGMEIN("ArgumentsObject.cpp] 609\n");
        // Same idea as with PreventExtensions: we have to make sure that items in objectArray for formals
        // are there before seal, otherwise we will not be able to add them later.
        return this->ConvertToES5HeapArgumentsObject()->Freeze();
    }

    //---------------------- ES5HeapArgumentsObject -------------------------------

    BOOL ES5HeapArgumentsObject::SetConfigurable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 618\n");
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 621\n");
            return this->SetConfigurableForFormal(index, propertyId, value);
        }
        return this->DynamicObject::SetConfigurable(propertyId, value);
    }

    BOOL ES5HeapArgumentsObject::SetEnumerable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 628\n");
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 631\n");
            return this->SetEnumerableForFormal(index, propertyId, value);
        }
        return this->DynamicObject::SetEnumerable(propertyId, value);
    }

    BOOL ES5HeapArgumentsObject::SetWritable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 638\n");
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 641\n");
            return this->SetWritableForFormal(index, propertyId, value);
        }
        return this->DynamicObject::SetWritable(propertyId, value);
    }

    BOOL ES5HeapArgumentsObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("ArgumentsObject.cpp] 648\n");
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 651\n");
            return SetAccessorsForFormal(index, propertyId, getter, setter);
        }
        return this->DynamicObject::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL ES5HeapArgumentsObject::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {LOGMEIN("ArgumentsObject.cpp] 658\n");
        // This is called by defineProperty in order to change the value in objectArray.
        // We have to intercept this call because
        // in case when we are connected (objectArray item is not used) we need to update the slot value (which is actually used).
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {LOGMEIN("ArgumentsObject.cpp] 664\n");
            return this->SetPropertyWithAttributesForFormal(index, propertyId, value, attributes, info, flags, possibleSideEffects);
        }

        return this->DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL ES5HeapArgumentsObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("ArgumentsObject.cpp] 672\n");
        ES5ArgumentsObjectEnumerator * es5HeapArgumentsObjectEnumerator = ES5ArgumentsObjectEnumerator::New(this, flags, requestContext, forInCache);
        if (es5HeapArgumentsObjectEnumerator == nullptr)
        {LOGMEIN("ArgumentsObject.cpp] 675\n");
            return false;
        }

        return enumerator->Initialize(es5HeapArgumentsObjectEnumerator, nullptr, nullptr, flags, requestContext, nullptr);
    }

    BOOL ES5HeapArgumentsObject::PreventExtensions()
    {LOGMEIN("ArgumentsObject.cpp] 683\n");
        return this->DynamicObject::PreventExtensions();
    }

    BOOL ES5HeapArgumentsObject::Seal()
    {LOGMEIN("ArgumentsObject.cpp] 688\n");
        return this->DynamicObject::Seal();
    }

    BOOL ES5HeapArgumentsObject::Freeze()
    {LOGMEIN("ArgumentsObject.cpp] 693\n");
        return this->DynamicObject::Freeze();
    }

    BOOL ES5HeapArgumentsObject::GetItemAt(uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("ArgumentsObject.cpp] 698\n");
        return this->IsFormalDisconnectedFromNamedArgument(index) ?
            this->DynamicObject::GetItem(this, index, value, requestContext) :
            __super::GetItemAt(index, value, requestContext);
    }

    BOOL ES5HeapArgumentsObject::SetItemAt(uint32 index, Var value)
    {LOGMEIN("ArgumentsObject.cpp] 705\n");
        return this->IsFormalDisconnectedFromNamedArgument(index) ?
            this->DynamicObject::SetItem(index, value, PropertyOperation_None) :
            __super::SetItemAt(index, value);
    }

    BOOL ES5HeapArgumentsObject::DeleteItemAt(uint32 index)
    {LOGMEIN("ArgumentsObject.cpp] 712\n");
        BOOL result = __super::DeleteItemAt(index);
        if (result && IsFormalArgument(index))
        {LOGMEIN("ArgumentsObject.cpp] 715\n");
            AssertMsg(this->IsFormalDisconnectedFromNamedArgument(index), "__super::DeleteItemAt must perform the disconnect.");
            // Make sure that objectArray does not have the item ().
            if (this->HasObjectArrayItem(index))
            {LOGMEIN("ArgumentsObject.cpp] 719\n");
                this->DeleteObjectArrayItem(index, PropertyOperation_None);
            }
        }

        return result;
    }

    //
    // Get the next valid formal arg index held in this object.
    //
    uint32 ES5HeapArgumentsObject::GetNextFormalArgIndex(uint32 index, BOOL enumNonEnumerable, PropertyAttributes* attributes) const
    {LOGMEIN("ArgumentsObject.cpp] 731\n");
        return GetNextFormalArgIndexHelper(index, enumNonEnumerable, attributes);
    }

    uint32 ES5HeapArgumentsObject::GetNextFormalArgIndexHelper(uint32 index, BOOL enumNonEnumerable, PropertyAttributes* attributes) const
    {LOGMEIN("ArgumentsObject.cpp] 736\n");
        // Formals:
        // - deleted => not in objectArray && not connected -- do not enum, do not advance.
        // - connected,     if in objectArray -- if (enumerable) enum it, advance objectEnumerator.
        // - disconnected =>in objectArray -- if (enumerable) enum it, advance objectEnumerator.

        // We use GetFormalCount and IsEnumerableByIndex which don't change the object
        // but are not declared as const, so do a const cast.
        ES5HeapArgumentsObject* mutableThis = const_cast<ES5HeapArgumentsObject*>(this);
        uint32 formalCount = this->GetFormalCount();
        while (++index < formalCount)
        {LOGMEIN("ArgumentsObject.cpp] 747\n");
            bool isDeleted = mutableThis->IsFormalDisconnectedFromNamedArgument(index) &&
                !mutableThis->HasObjectArrayItem(index);

            if (!isDeleted)
            {LOGMEIN("ArgumentsObject.cpp] 752\n");
                BOOL isEnumerable = mutableThis->IsEnumerableByIndex(index);

                if (enumNonEnumerable || isEnumerable)
                {LOGMEIN("ArgumentsObject.cpp] 756\n");
                    if (attributes != nullptr && isEnumerable)
                    {LOGMEIN("ArgumentsObject.cpp] 758\n");
                        *attributes = PropertyEnumerable;
                    }

                    return index;
                }
            }
        }

        return JavascriptArray::InvalidIndex;
    }

    // Disconnects indexed argument from named argument for frame object property.
    // Remove association from them map. From now on (or still) for this argument,
    // named argument's value is no longer associated with arguments[] item.
    void ES5HeapArgumentsObject::DisconnectFormalFromNamedArgument(uint32 index)
    {LOGMEIN("ArgumentsObject.cpp] 774\n");
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        if (!IsFormalDisconnectedFromNamedArgument(index))
        {LOGMEIN("ArgumentsObject.cpp] 778\n");
            __super::DeleteItemAt(index);
        }
    }

    BOOL ES5HeapArgumentsObject::IsFormalDisconnectedFromNamedArgument(uint32 index)
    {LOGMEIN("ArgumentsObject.cpp] 784\n");
        return this->IsArgumentDeleted(index);
    }

    // Wrapper over IsEnumerable by uint32 index.
    BOOL ES5HeapArgumentsObject::IsEnumerableByIndex(uint32 index)
    {LOGMEIN("ArgumentsObject.cpp] 790\n");
        ScriptContext* scriptContext = this->GetScriptContext();
        Var indexNumber = JavascriptNumber::New(index, scriptContext);
        JavascriptString* indexPropertyName = JavascriptConversion::ToString(indexNumber, scriptContext);
        PropertyRecord const * propertyRecord;
        scriptContext->GetOrAddPropertyRecord(indexPropertyName->GetString(), indexPropertyName->GetLength(), &propertyRecord);
        return this->IsEnumerable(propertyRecord->GetPropertyId());
    }

    // Helper method, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetConfigurableForFormal(uint32 index, PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 801\n");
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        // In order for attributes to work, the item must exist. Make sure that's the case.
        // If we were connected, we have to copy the value from frameObject->slots, otherwise which value doesn't matter.
        AutoObjectArrayItemExistsValidator autoItemAddRelease(this, index);

        BOOL result = this->DynamicObject::SetConfigurable(propertyId, value);
        autoItemAddRelease.m_isReleaseItemNeeded = !result;

        return result;
    }

    // Helper method, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetEnumerableForFormal(uint32 index, PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 816\n");
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        AutoObjectArrayItemExistsValidator autoItemAddRelease(this, index);

        BOOL result = this->DynamicObject::SetEnumerable(propertyId, value);
        autoItemAddRelease.m_isReleaseItemNeeded = !result;

        return result;
    }

    // Helper method, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetWritableForFormal(uint32 index, PropertyId propertyId, BOOL value)
    {LOGMEIN("ArgumentsObject.cpp] 829\n");
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        AutoObjectArrayItemExistsValidator autoItemAddRelease(this, index);

        BOOL isDisconnected = IsFormalDisconnectedFromNamedArgument(index);
        if (!isDisconnected && !value)
        {LOGMEIN("ArgumentsObject.cpp] 836\n");
            // Settings writable to false causes disconnect.
            // It will be too late to copy the value after setting writable = false, as we would not be able to.
            // Since we are connected, it does not matter the value is, so it's safe (no matter if SetWritable fails) to copy it here.
            this->SetObjectArrayItem(index, this->frameObject->GetSlot(index), PropertyOperation_None);
        }

        BOOL result = this->DynamicObject::SetWritable(propertyId, value); // Note: this will convert objectArray to ES5Array.
        if (result && !value && !isDisconnected)
        {LOGMEIN("ArgumentsObject.cpp] 845\n");
            this->DisconnectFormalFromNamedArgument(index);
        }
        autoItemAddRelease.m_isReleaseItemNeeded = !result;

        return result;
    }

    // Helper method for SetPropertyWithAttributes, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetAccessorsForFormal(uint32 index, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("ArgumentsObject.cpp] 855\n");
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        AutoObjectArrayItemExistsValidator autoItemAddRelease(this, index);

        BOOL result = this->DynamicObject::SetAccessors(propertyId, getter, setter, flags);
        if (result)
        {LOGMEIN("ArgumentsObject.cpp] 862\n");
            this->DisconnectFormalFromNamedArgument(index);
        }
        autoItemAddRelease.m_isReleaseItemNeeded = !result;

        return result;
    }

    // Helper method for SetPropertyWithAttributes, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetPropertyWithAttributesForFormal(uint32 index, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {LOGMEIN("ArgumentsObject.cpp] 872\n");
        AssertMsg(this->IsFormalArgument(propertyId), "SetPropertyWithAttributesForFormal: called for non-formal");

        BOOL result = this->DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
        if (result)
        {LOGMEIN("ArgumentsObject.cpp] 877\n");
            if ((attributes & PropertyWritable) == 0)
            {LOGMEIN("ArgumentsObject.cpp] 879\n");
                // Setting writable to false should cause disconnect.
                this->DisconnectFormalFromNamedArgument(index);
            }

            if (!this->IsFormalDisconnectedFromNamedArgument(index))
            {LOGMEIN("ArgumentsObject.cpp] 885\n");
                // Update the (connected with named param) value, as above call could cause change of the value.
                BOOL tempResult = this->SetItemAt(index, value);  // Update the value in frameObject.
                AssertMsg(tempResult, "this->SetItem(index, value)");
            }
        }

        return result;
    }

    //---------------------- ES5HeapArgumentsObject::AutoObjectArrayItemExistsValidator -------------------------------
    ES5HeapArgumentsObject::AutoObjectArrayItemExistsValidator::AutoObjectArrayItemExistsValidator(ES5HeapArgumentsObject* args, uint32 index)
        : m_args(args), m_index(index), m_isReleaseItemNeeded(false)
    {
        AssertMsg(args, "args");
        AssertMsg(args->IsFormalArgument(index), "AutoObjectArrayItemExistsValidator: called for non-formal");

        if (!args->HasObjectArrayItem(index))
        {LOGMEIN("ArgumentsObject.cpp] 903\n");
            m_isReleaseItemNeeded = args->SetObjectArrayItem(index, args->frameObject->GetSlot(index), PropertyOperation_None) != FALSE;
        }
    }

    ES5HeapArgumentsObject::AutoObjectArrayItemExistsValidator::~AutoObjectArrayItemExistsValidator()
    {LOGMEIN("ArgumentsObject.cpp] 909\n");
        if (m_isReleaseItemNeeded)
        {LOGMEIN("ArgumentsObject.cpp] 911\n");
           m_args->DeleteObjectArrayItem(m_index, PropertyOperation_None);
        }
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ES5HeapArgumentsObject::GetSnapTag_TTD() const
    {LOGMEIN("ArgumentsObject.cpp] 918\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapES5HeapArgumentsObject;
    }

    void ES5HeapArgumentsObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ArgumentsObject.cpp] 923\n");
        this->ExtractSnapObjectDataInto_Helper<TTD::NSSnapObjects::SnapObjectType::SnapES5HeapArgumentsObject>(objData, alloc);
    }
#endif
}
