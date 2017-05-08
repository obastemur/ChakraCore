//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "Library/ArgumentsObjectEnumerator.h"

namespace Js
{
    BOOL ArgumentsObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(54074);
        return GetEnumeratorWithPrefix(
            RecyclerNew(GetScriptContext()->GetRecycler(), ArgumentsObjectPrefixEnumerator, this, flags, requestContext),
            enumerator, flags, requestContext, forInCache);
    }

    BOOL ArgumentsObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(54075);
        stringBuilder->AppendCppLiteral(_u("{...}"));
        return TRUE;
    }

    BOOL ArgumentsObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(54076);
        stringBuilder->AppendCppLiteral(_u("Object, (Arguments)"));
        return TRUE;
    }

    Var ArgumentsObject::GetCaller(ScriptContext * scriptContext)
    {TRACE_IT(54077);
        JavascriptStackWalker walker(scriptContext);

        if (!this->AdvanceWalkerToArgsFrame(&walker))
        {TRACE_IT(54078);
            return scriptContext->GetLibrary()->GetNull();
        }

        return ArgumentsObject::GetCaller(scriptContext, &walker, false);
    }

    Var ArgumentsObject::GetCaller(ScriptContext * scriptContext, JavascriptStackWalker *walker, bool skipGlobal)
    {TRACE_IT(54079);
        // The arguments.caller property is equivalent to callee.caller.arguments - that is, it's the
        // caller's arguments object (if any). Just fetch the caller and compute its arguments.
        JavascriptFunction* funcCaller = nullptr;

        while (walker->GetCaller(&funcCaller))
        {TRACE_IT(54080);
            if (walker->IsCallerGlobalFunction())
            {TRACE_IT(54081);
                // Caller is global/eval. If we're in IE9 mode, and the caller is eval,
                // keep looking. Otherwise, caller is null.
                if (skipGlobal || walker->IsEvalCaller())
                {TRACE_IT(54082);
                    continue;
                }
                funcCaller = nullptr;
            }
            break;
        }

        if (funcCaller == nullptr || JavascriptOperators::GetTypeId(funcCaller) == TypeIds_Null)
        {TRACE_IT(54083);
            return scriptContext->GetLibrary()->GetNull();
        }

        AssertMsg(JavascriptOperators::GetTypeId(funcCaller) == TypeIds_Function, "non function caller");

        const CallInfo callInfo = walker->GetCallInfo();
        uint32 paramCount = callInfo.Count;
        CallFlags flags = callInfo.Flags;

        if (paramCount == 0 || (flags & CallFlags_Eval))
        {TRACE_IT(54084);
            // The caller is the "global function" or eval, so we return "null".
            return scriptContext->GetLibrary()->GetNull();
        }

        if (!walker->GetCurrentFunction()->IsScriptFunction())
        {TRACE_IT(54085);
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
    {TRACE_IT(54086);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_Arguments;
    }

    HeapArgumentsObject::HeapArgumentsObject(DynamicType * type) : ArgumentsObject(type), frameObject(nullptr), formalCount(0),
        numOfArguments(0), callerDeleted(false), deletedArgs(nullptr)
    {TRACE_IT(54087);
    }

    HeapArgumentsObject::HeapArgumentsObject(Recycler *recycler, ActivationObject* obj, uint32 formalCount, DynamicType * type)
        : ArgumentsObject(type), frameObject(obj), formalCount(formalCount), numOfArguments(0), callerDeleted(false), deletedArgs(nullptr)
    {TRACE_IT(54088);
    }

    void HeapArgumentsObject::SetNumberOfArguments(uint32 len)
    {TRACE_IT(54089);
        numOfArguments = len;
    }

    uint32 HeapArgumentsObject::GetNumberOfArguments() const
    {TRACE_IT(54090);
        return numOfArguments;
    }

    HeapArgumentsObject* HeapArgumentsObject::As(Var aValue)
    {TRACE_IT(54091);
        if (ArgumentsObject::Is(aValue) && static_cast<ArgumentsObject*>(RecyclableObject::FromVar(aValue))->GetHeapArguments() == aValue)
        {TRACE_IT(54092);
            return static_cast<HeapArgumentsObject*>(RecyclableObject::FromVar(aValue));
        }
        return NULL;
    }

    BOOL HeapArgumentsObject::AdvanceWalkerToArgsFrame(JavascriptStackWalker *walker)
    {TRACE_IT(54093);
        // Walk until we find this arguments object on the frame.
        // Note that each frame may have a HeapArgumentsObject
        // associated with it. Look for the HeapArgumentsObject.
        while (walker->Walk())
        {TRACE_IT(54094);
            if (walker->IsJavascriptFrame() && walker->GetCurrentFunction()->IsScriptFunction())
            {TRACE_IT(54095);
                Var args = walker->GetPermanentArguments();
                if (args == this)
                {TRACE_IT(54096);
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
    {TRACE_IT(54097);
        if (attributes != nullptr)
        {TRACE_IT(54098);
            *attributes = PropertyEnumerable;
        }

        if ( ++index < formalCount )
        {TRACE_IT(54099);
            // None of the arguments deleted
            if ( deletedArgs == nullptr )
            {TRACE_IT(54100);
                return index;
            }

            while ( index < formalCount )
            {TRACE_IT(54101);
                if (!this->deletedArgs->Test(index))
                {TRACE_IT(54102);
                    return index;
                }

                index++;
            }
        }

        return JavascriptArray::InvalidIndex;
    }

    BOOL HeapArgumentsObject::HasItemAt(uint32 index)
    {TRACE_IT(54103);
        // If this arg index is bound to a named formal argument, get it from the local frame.
        // If not, report this fact to the caller, which will defer to the normal get-value-by-index means.
        if (IsFormalArgument(index) && (this->deletedArgs == nullptr || !this->deletedArgs->Test(index)) )
        {TRACE_IT(54104);
            return true;
        }

        return false;
    }

    BOOL HeapArgumentsObject::GetItemAt(uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(54105);
        // If this arg index is bound to a named formal argument, get it from the local frame.
        // If not, report this fact to the caller, which will defer to the normal get-value-by-index means.
        if (HasItemAt(index))
        {TRACE_IT(54106);
            *value = this->frameObject->GetSlot(index);
            return true;
        }

        return false;
    }

    BOOL HeapArgumentsObject::SetItemAt(uint32 index, Var value)
    {TRACE_IT(54107);
        // If this arg index is bound to a named formal argument, set it in the local frame.
        // If not, report this fact to the caller, which will defer to the normal set-value-by-index means.
        if (HasItemAt(index))
        {TRACE_IT(54108);
            this->frameObject->SetSlot(SetSlotArguments(Constants::NoProperty, index, value));
            return true;
        }

        return false;
    }

    BOOL HeapArgumentsObject::DeleteItemAt(uint32 index)
    {TRACE_IT(54109);
        if (index < formalCount)
        {TRACE_IT(54110);
            if (this->deletedArgs == nullptr)
            {TRACE_IT(54111);
                Recycler *recycler = GetScriptContext()->GetRecycler();
                deletedArgs = RecyclerNew(recycler, BVSparse<Recycler>, recycler);
            }

            if (!this->deletedArgs->Test(index))
            {TRACE_IT(54112);
                this->deletedArgs->Set(index);
                return true;
            }
        }

        return false;
    }

    BOOL HeapArgumentsObject::IsFormalArgument(uint32 index)
    {TRACE_IT(54113);
        return index < this->numOfArguments && index < formalCount;
    }

    BOOL HeapArgumentsObject::IsFormalArgument(PropertyId propertyId)
    {TRACE_IT(54114);
        uint32 index;
        return IsFormalArgument(propertyId, &index);
    }

    BOOL HeapArgumentsObject::IsFormalArgument(PropertyId propertyId, uint32* pIndex)
    {TRACE_IT(54115);
        return
            this->GetScriptContext()->IsNumericPropertyId(propertyId, pIndex) &&
            IsFormalArgument(*pIndex);
    }

    BOOL HeapArgumentsObject::IsArgumentDeleted(uint32 index) const
    {TRACE_IT(54116);
        return this->deletedArgs != NULL && this->deletedArgs->Test(index);
    }

#if ENABLE_TTD
    void HeapArgumentsObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(54117);
        if(this->frameObject != nullptr)
        {TRACE_IT(54118);
            extractor->MarkVisitVar(this->frameObject);
        }
    }

    TTD::NSSnapObjects::SnapObjectType HeapArgumentsObject::GetSnapTag_TTD() const
    {TRACE_IT(54119);
        return TTD::NSSnapObjects::SnapObjectType::SnapHeapArgumentsObject;
    }

    void HeapArgumentsObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(54120);
        this->ExtractSnapObjectDataInto_Helper<TTD::NSSnapObjects::SnapObjectType::SnapHeapArgumentsObject>(objData, alloc);
    }

    template <TTD::NSSnapObjects::SnapObjectType argsKind>
    void HeapArgumentsObject::ExtractSnapObjectDataInto_Helper(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(54121);
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
        {TRACE_IT(54122);
            argsInfo->IsFrameNullPtr = true;
        }
        else if(Js::JavascriptOperators::GetTypeId(this->frameObject) == TypeIds_Null)
        {TRACE_IT(54123);
            argsInfo->IsFrameJsNull = true;
        }
        else
        {TRACE_IT(54124);
            argsInfo->FrameObject = TTD_CONVERT_VAR_TO_PTR_ID(
                static_cast<ActivationObject*>(this->frameObject));

            //Primitive kinds always inflated first so we only need to deal with complex kinds as depends on
            if(TTD::JsSupport::IsVarComplexKind(this->frameObject))
            {TRACE_IT(54125);
                depOnCount = 1;
                depOnArray = alloc.SlabAllocateArray<TTD_PTR_ID>(depOnCount);
                depOnArray[0] = argsInfo->FrameObject;
            }
        }

        argsInfo->DeletedArgFlags = (this->formalCount != 0) ? alloc.SlabAllocateArrayZ<byte>(argsInfo->FormalCount) : nullptr;
        if(this->deletedArgs != nullptr)
        {TRACE_IT(54126);
            for(uint32 i = 0; i < this->formalCount; ++i)
            {TRACE_IT(54127);
                if(this->deletedArgs->Test(i))
                {TRACE_IT(54128);
                    argsInfo->DeletedArgFlags[i] = true;
                }
            }
        }

        if(depOnCount == 0)
        {TRACE_IT(54129);
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapHeapArgumentsInfo*, argsKind>(objData, argsInfo);
        }
        else
        {TRACE_IT(54130);
            TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapHeapArgumentsInfo*, argsKind>(objData, argsInfo, alloc, depOnCount, depOnArray);
        }
    }

    ES5HeapArgumentsObject* HeapArgumentsObject::ConvertToES5HeapArgumentsObject_TTD()
    {TRACE_IT(54131);
        Assert(VirtualTableInfo<HeapArgumentsObject>::HasVirtualTable(this));
        VirtualTableInfo<ES5HeapArgumentsObject>::SetVirtualTable(this);

        return static_cast<ES5HeapArgumentsObject*>(this);
    }
#endif

    ES5HeapArgumentsObject* HeapArgumentsObject::ConvertToUnmappedArgumentsObject(bool overwriteArgsUsingFrameObject)
    {TRACE_IT(54132);
        ES5HeapArgumentsObject* es5ArgsObj = ConvertToES5HeapArgumentsObject(overwriteArgsUsingFrameObject);

        for (uint i=0; i<this->formalCount && i<numOfArguments; ++i)
        {TRACE_IT(54133);
            es5ArgsObj->DisconnectFormalFromNamedArgument(i);
        }

        return es5ArgsObj;
    }

    ES5HeapArgumentsObject* HeapArgumentsObject::ConvertToES5HeapArgumentsObject(bool overwriteArgsUsingFrameObject)
    {TRACE_IT(54134);
        if (!CrossSite::IsCrossSiteObjectTyped(this))
        {TRACE_IT(54135);
            Assert(VirtualTableInfo<HeapArgumentsObject>::HasVirtualTable(this));
            VirtualTableInfo<ES5HeapArgumentsObject>::SetVirtualTable(this);
        }
        else
        {TRACE_IT(54136);
            Assert(VirtualTableInfo<CrossSiteObject<HeapArgumentsObject>>::HasVirtualTable(this));
            VirtualTableInfo<CrossSiteObject<ES5HeapArgumentsObject>>::SetVirtualTable(this);
        }

        ES5HeapArgumentsObject* es5This = static_cast<ES5HeapArgumentsObject*>(this);

        if (overwriteArgsUsingFrameObject)
        {TRACE_IT(54137);
            // Copy existing items to the array so that they are there before the user can call Object.preventExtensions,
            // after which we can no longer add new items to the objectArray.
            for (uint32 i = 0; i < this->formalCount && i < this->numOfArguments; ++i)
            {TRACE_IT(54138);
                AssertMsg(this->IsFormalArgument(i), "this->IsFormalArgument(i)");
                if (!this->IsArgumentDeleted(i))
                {TRACE_IT(54139);
                    // At this time the value doesn't matter, use one from slots.
                    this->SetObjectArrayItem(i, this->frameObject->GetSlot(i), PropertyOperation_None);
                }
            }
        }
        return es5This;

    }

    BOOL HeapArgumentsObject::HasProperty(PropertyId id)
    {TRACE_IT(54140);
        ScriptContext *scriptContext = GetScriptContext();

        // Try to get a numbered property that maps to an actual argument.
        uint32 index;
        if (scriptContext->IsNumericPropertyId(id, &index) && index < this->HeapArgumentsObject::GetNumberOfArguments())
        {TRACE_IT(54141);
            return HeapArgumentsObject::HasItem(index);
        }

        return DynamicObject::HasProperty(id);
    }

    BOOL HeapArgumentsObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(54142);
        ScriptContext *scriptContext = GetScriptContext();

        // Try to get a numbered property that maps to an actual argument.
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index) && index < this->HeapArgumentsObject::GetNumberOfArguments())
        {TRACE_IT(54143);
            if (this->GetItemAt(index, value, requestContext))
            {TRACE_IT(54144);
                return true;
            }
        }

        if (DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {TRACE_IT(54145);
            // Property has been pre-set and not deleted. Use it.
            return true;
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL HeapArgumentsObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(54146);

        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        if (DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext))
        {TRACE_IT(54147);
            // Property has been pre-set and not deleted. Use it.
            return true;
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL HeapArgumentsObject::GetPropertyReference(Var originalInstance, PropertyId id, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(54148);
        return HeapArgumentsObject::GetProperty(originalInstance, id, value, info, requestContext);
    }

    BOOL HeapArgumentsObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(54149);
        // Try to set a numbered property that maps to an actual argument.
        ScriptContext *scriptContext = GetScriptContext();

        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index) && index < this->HeapArgumentsObject::GetNumberOfArguments())
        {TRACE_IT(54150);
            if (this->SetItemAt(index, value))
            {TRACE_IT(54151);
                return true;
            }
        }

        // TODO: In strict mode, "callee" and "caller" cannot be set.

        // length is also set in the dynamic object
        return DynamicObject::SetProperty(propertyId, value, flags, info);
    }

    BOOL HeapArgumentsObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(54152);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetSz(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        // TODO: In strict mode, "callee" and "caller" cannot be set.

        // length is also set in the dynamic object
        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    BOOL HeapArgumentsObject::HasItem(uint32 index)
    {TRACE_IT(54153);
        if (this->HasItemAt(index))
        {TRACE_IT(54154);
            return true;
        }
        return DynamicObject::HasItem(index);
    }

    BOOL HeapArgumentsObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(54155);
        if (this->GetItemAt(index, value, requestContext))
        {TRACE_IT(54156);
            return true;
        }
        return DynamicObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL HeapArgumentsObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(54157);
        return HeapArgumentsObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL HeapArgumentsObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(54158);
        if (this->SetItemAt(index, value))
        {TRACE_IT(54159);
            return true;
        }
        return DynamicObject::SetItem(index, value, flags);
    }

    BOOL HeapArgumentsObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(54160);
        if (this->DeleteItemAt(index))
        {TRACE_IT(54161);
            return true;
        }
        return DynamicObject::DeleteItem(index, flags);
    }

    BOOL HeapArgumentsObject::SetConfigurable(PropertyId propertyId, BOOL value)
    {TRACE_IT(54162);
        // Try to set a numbered property that maps to an actual argument.
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54163);
            // From now on, make sure that frame object is ES5HeapArgumentsObject.
            return this->ConvertToES5HeapArgumentsObject()->SetConfigurableForFormal(index, propertyId, value);
        }

        // Use 'this' dynamic object.
        // This will use type handler and convert its objectArray to ES5Array is not already converted.
        return __super::SetConfigurable(propertyId, value);
    }

    BOOL HeapArgumentsObject::SetEnumerable(PropertyId propertyId, BOOL value)
    {TRACE_IT(54164);
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54165);
            return this->ConvertToES5HeapArgumentsObject()->SetEnumerableForFormal(index, propertyId, value);
        }
        return __super::SetEnumerable(propertyId, value);
    }

    BOOL HeapArgumentsObject::SetWritable(PropertyId propertyId, BOOL value)
    {TRACE_IT(54166);
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54167);
            return this->ConvertToES5HeapArgumentsObject()->SetWritableForFormal(index, propertyId, value);
        }
        return __super::SetWritable(propertyId, value);
    }

    BOOL HeapArgumentsObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(54168);
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54169);
            return this->ConvertToES5HeapArgumentsObject()->SetAccessorsForFormal(index, propertyId, getter, setter, flags);
        }
        return __super::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL HeapArgumentsObject::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(54170);
        // This is called by defineProperty in order to change the value in objectArray.
        // We have to intercept this call because
        // in case when we are connected (objectArray item is not used) we need to update the slot value (which is actually used).
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54171);
            return this->ConvertToES5HeapArgumentsObject()->SetPropertyWithAttributesForFormal(
                index, propertyId, value, attributes, info, flags, possibleSideEffects);
        }
        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    // This disables adding new properties to the object.
    BOOL HeapArgumentsObject::PreventExtensions()
    {TRACE_IT(54172);
        // It's possible that after call to preventExtensions, the user can change the attributes;
        // In this case if we don't covert to ES5 version now, later we would not be able to add items to objectArray,
        // which would cause not being able to change attributes.
        // So, convert to ES5 now which will make sure the items are there.
        return this->ConvertToES5HeapArgumentsObject()->PreventExtensions();
    }

    // This is equivalent to .preventExtensions semantics with addition of setting configurable to false for all properties.
    BOOL HeapArgumentsObject::Seal()
    {TRACE_IT(54173);
        // Same idea as with PreventExtensions: we have to make sure that items in objectArray for formals
        // are there before seal, otherwise we will not be able to add them later.
        return this->ConvertToES5HeapArgumentsObject()->Seal();
    }

    // This is equivalent to .seal semantics with addition of setting writable to false for all properties.
    BOOL HeapArgumentsObject::Freeze()
    {TRACE_IT(54174);
        // Same idea as with PreventExtensions: we have to make sure that items in objectArray for formals
        // are there before seal, otherwise we will not be able to add them later.
        return this->ConvertToES5HeapArgumentsObject()->Freeze();
    }

    //---------------------- ES5HeapArgumentsObject -------------------------------

    BOOL ES5HeapArgumentsObject::SetConfigurable(PropertyId propertyId, BOOL value)
    {TRACE_IT(54175);
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54176);
            return this->SetConfigurableForFormal(index, propertyId, value);
        }
        return this->DynamicObject::SetConfigurable(propertyId, value);
    }

    BOOL ES5HeapArgumentsObject::SetEnumerable(PropertyId propertyId, BOOL value)
    {TRACE_IT(54177);
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54178);
            return this->SetEnumerableForFormal(index, propertyId, value);
        }
        return this->DynamicObject::SetEnumerable(propertyId, value);
    }

    BOOL ES5HeapArgumentsObject::SetWritable(PropertyId propertyId, BOOL value)
    {TRACE_IT(54179);
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54180);
            return this->SetWritableForFormal(index, propertyId, value);
        }
        return this->DynamicObject::SetWritable(propertyId, value);
    }

    BOOL ES5HeapArgumentsObject::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(54181);
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54182);
            return SetAccessorsForFormal(index, propertyId, getter, setter);
        }
        return this->DynamicObject::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL ES5HeapArgumentsObject::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(54183);
        // This is called by defineProperty in order to change the value in objectArray.
        // We have to intercept this call because
        // in case when we are connected (objectArray item is not used) we need to update the slot value (which is actually used).
        uint32 index;
        if (this->IsFormalArgument(propertyId, &index))
        {TRACE_IT(54184);
            return this->SetPropertyWithAttributesForFormal(index, propertyId, value, attributes, info, flags, possibleSideEffects);
        }

        return this->DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL ES5HeapArgumentsObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(54185);
        ES5ArgumentsObjectEnumerator * es5HeapArgumentsObjectEnumerator = ES5ArgumentsObjectEnumerator::New(this, flags, requestContext, forInCache);
        if (es5HeapArgumentsObjectEnumerator == nullptr)
        {TRACE_IT(54186);
            return false;
        }

        return enumerator->Initialize(es5HeapArgumentsObjectEnumerator, nullptr, nullptr, flags, requestContext, nullptr);
    }

    BOOL ES5HeapArgumentsObject::PreventExtensions()
    {TRACE_IT(54187);
        return this->DynamicObject::PreventExtensions();
    }

    BOOL ES5HeapArgumentsObject::Seal()
    {TRACE_IT(54188);
        return this->DynamicObject::Seal();
    }

    BOOL ES5HeapArgumentsObject::Freeze()
    {TRACE_IT(54189);
        return this->DynamicObject::Freeze();
    }

    BOOL ES5HeapArgumentsObject::GetItemAt(uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(54190);
        return this->IsFormalDisconnectedFromNamedArgument(index) ?
            this->DynamicObject::GetItem(this, index, value, requestContext) :
            __super::GetItemAt(index, value, requestContext);
    }

    BOOL ES5HeapArgumentsObject::SetItemAt(uint32 index, Var value)
    {TRACE_IT(54191);
        return this->IsFormalDisconnectedFromNamedArgument(index) ?
            this->DynamicObject::SetItem(index, value, PropertyOperation_None) :
            __super::SetItemAt(index, value);
    }

    BOOL ES5HeapArgumentsObject::DeleteItemAt(uint32 index)
    {TRACE_IT(54192);
        BOOL result = __super::DeleteItemAt(index);
        if (result && IsFormalArgument(index))
        {TRACE_IT(54193);
            AssertMsg(this->IsFormalDisconnectedFromNamedArgument(index), "__super::DeleteItemAt must perform the disconnect.");
            // Make sure that objectArray does not have the item ().
            if (this->HasObjectArrayItem(index))
            {TRACE_IT(54194);
                this->DeleteObjectArrayItem(index, PropertyOperation_None);
            }
        }

        return result;
    }

    //
    // Get the next valid formal arg index held in this object.
    //
    uint32 ES5HeapArgumentsObject::GetNextFormalArgIndex(uint32 index, BOOL enumNonEnumerable, PropertyAttributes* attributes) const
    {TRACE_IT(54195);
        return GetNextFormalArgIndexHelper(index, enumNonEnumerable, attributes);
    }

    uint32 ES5HeapArgumentsObject::GetNextFormalArgIndexHelper(uint32 index, BOOL enumNonEnumerable, PropertyAttributes* attributes) const
    {TRACE_IT(54196);
        // Formals:
        // - deleted => not in objectArray && not connected -- do not enum, do not advance.
        // - connected,     if in objectArray -- if (enumerable) enum it, advance objectEnumerator.
        // - disconnected =>in objectArray -- if (enumerable) enum it, advance objectEnumerator.

        // We use GetFormalCount and IsEnumerableByIndex which don't change the object
        // but are not declared as const, so do a const cast.
        ES5HeapArgumentsObject* mutableThis = const_cast<ES5HeapArgumentsObject*>(this);
        uint32 formalCount = this->GetFormalCount();
        while (++index < formalCount)
        {TRACE_IT(54197);
            bool isDeleted = mutableThis->IsFormalDisconnectedFromNamedArgument(index) &&
                !mutableThis->HasObjectArrayItem(index);

            if (!isDeleted)
            {TRACE_IT(54198);
                BOOL isEnumerable = mutableThis->IsEnumerableByIndex(index);

                if (enumNonEnumerable || isEnumerable)
                {TRACE_IT(54199);
                    if (attributes != nullptr && isEnumerable)
                    {TRACE_IT(54200);
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
    {TRACE_IT(54201);
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        if (!IsFormalDisconnectedFromNamedArgument(index))
        {TRACE_IT(54202);
            __super::DeleteItemAt(index);
        }
    }

    BOOL ES5HeapArgumentsObject::IsFormalDisconnectedFromNamedArgument(uint32 index)
    {TRACE_IT(54203);
        return this->IsArgumentDeleted(index);
    }

    // Wrapper over IsEnumerable by uint32 index.
    BOOL ES5HeapArgumentsObject::IsEnumerableByIndex(uint32 index)
    {TRACE_IT(54204);
        ScriptContext* scriptContext = this->GetScriptContext();
        Var indexNumber = JavascriptNumber::New(index, scriptContext);
        JavascriptString* indexPropertyName = JavascriptConversion::ToString(indexNumber, scriptContext);
        PropertyRecord const * propertyRecord;
        scriptContext->GetOrAddPropertyRecord(indexPropertyName->GetString(), indexPropertyName->GetLength(), &propertyRecord);
        return this->IsEnumerable(propertyRecord->GetPropertyId());
    }

    // Helper method, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetConfigurableForFormal(uint32 index, PropertyId propertyId, BOOL value)
    {TRACE_IT(54205);
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
    {TRACE_IT(54206);
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        AutoObjectArrayItemExistsValidator autoItemAddRelease(this, index);

        BOOL result = this->DynamicObject::SetEnumerable(propertyId, value);
        autoItemAddRelease.m_isReleaseItemNeeded = !result;

        return result;
    }

    // Helper method, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetWritableForFormal(uint32 index, PropertyId propertyId, BOOL value)
    {TRACE_IT(54207);
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        AutoObjectArrayItemExistsValidator autoItemAddRelease(this, index);

        BOOL isDisconnected = IsFormalDisconnectedFromNamedArgument(index);
        if (!isDisconnected && !value)
        {TRACE_IT(54208);
            // Settings writable to false causes disconnect.
            // It will be too late to copy the value after setting writable = false, as we would not be able to.
            // Since we are connected, it does not matter the value is, so it's safe (no matter if SetWritable fails) to copy it here.
            this->SetObjectArrayItem(index, this->frameObject->GetSlot(index), PropertyOperation_None);
        }

        BOOL result = this->DynamicObject::SetWritable(propertyId, value); // Note: this will convert objectArray to ES5Array.
        if (result && !value && !isDisconnected)
        {TRACE_IT(54209);
            this->DisconnectFormalFromNamedArgument(index);
        }
        autoItemAddRelease.m_isReleaseItemNeeded = !result;

        return result;
    }

    // Helper method for SetPropertyWithAttributes, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetAccessorsForFormal(uint32 index, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(54210);
        AssertMsg(this->IsFormalArgument(index), "SetAccessorsForFormal: called for non-formal");

        AutoObjectArrayItemExistsValidator autoItemAddRelease(this, index);

        BOOL result = this->DynamicObject::SetAccessors(propertyId, getter, setter, flags);
        if (result)
        {TRACE_IT(54211);
            this->DisconnectFormalFromNamedArgument(index);
        }
        autoItemAddRelease.m_isReleaseItemNeeded = !result;

        return result;
    }

    // Helper method for SetPropertyWithAttributes, just to avoid code duplication.
    BOOL ES5HeapArgumentsObject::SetPropertyWithAttributesForFormal(uint32 index, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(54212);
        AssertMsg(this->IsFormalArgument(propertyId), "SetPropertyWithAttributesForFormal: called for non-formal");

        BOOL result = this->DynamicObject::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
        if (result)
        {TRACE_IT(54213);
            if ((attributes & PropertyWritable) == 0)
            {TRACE_IT(54214);
                // Setting writable to false should cause disconnect.
                this->DisconnectFormalFromNamedArgument(index);
            }

            if (!this->IsFormalDisconnectedFromNamedArgument(index))
            {TRACE_IT(54215);
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
        {TRACE_IT(54216);
            m_isReleaseItemNeeded = args->SetObjectArrayItem(index, args->frameObject->GetSlot(index), PropertyOperation_None) != FALSE;
        }
    }

    ES5HeapArgumentsObject::AutoObjectArrayItemExistsValidator::~AutoObjectArrayItemExistsValidator()
    {TRACE_IT(54217);
        if (m_isReleaseItemNeeded)
        {TRACE_IT(54218);
           m_args->DeleteObjectArrayItem(m_index, PropertyOperation_None);
        }
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ES5HeapArgumentsObject::GetSnapTag_TTD() const
    {TRACE_IT(54219);
        return TTD::NSSnapObjects::SnapObjectType::SnapES5HeapArgumentsObject;
    }

    void ES5HeapArgumentsObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(54220);
        this->ExtractSnapObjectDataInto_Helper<TTD::NSSnapObjects::SnapObjectType::SnapES5HeapArgumentsObject>(objData, alloc);
    }
#endif
}
