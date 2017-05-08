//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    CompileAssert(sizeof(ES5Array) == sizeof(JavascriptArray));

    ES5ArrayType::ES5ArrayType(DynamicType* type)
        : DynamicType(type->GetScriptContext(), TypeIds_ES5Array, type->GetPrototype(), type->GetEntryPoint(), type->GetTypeHandler(), false, false)
    {TRACE_IT(55119);
    }

    bool ES5Array::Is(Var instance)
    {TRACE_IT(55120);
        return JavascriptOperators::GetTypeId(instance) == TypeIds_ES5Array;
    }

    ES5Array* ES5Array::FromVar(Var instance)
    {TRACE_IT(55121);
        Assert(Is(instance));
        return static_cast<ES5Array*>(instance);
    }

    DynamicType* ES5Array::DuplicateType()
    {TRACE_IT(55122);
        return RecyclerNew(GetScriptContext()->GetRecycler(), ES5ArrayType, this->GetDynamicType());
    }

    bool ES5Array::IsLengthWritable() const
    {TRACE_IT(55123);
        return GetTypeHandler()->IsLengthWritable();
    }

    BOOL ES5Array::HasProperty(PropertyId propertyId)
    {TRACE_IT(55124);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(55125);
            return true;
        }

        // Skip JavascriptArray override
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL ES5Array::IsWritable(PropertyId propertyId)
    {TRACE_IT(55126);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(55127);
            return IsLengthWritable();
        }

        return __super::IsWritable(propertyId);
    }

    BOOL ES5Array::SetEnumerable(PropertyId propertyId, BOOL value)
    {TRACE_IT(55128);
        // Skip JavascriptArray override
        return DynamicObject::SetEnumerable(propertyId, value);
    }

    BOOL ES5Array::SetWritable(PropertyId propertyId, BOOL value)
    {TRACE_IT(55129);
        // Skip JavascriptArray override
        return DynamicObject::SetWritable(propertyId, value);
    }

    BOOL ES5Array::SetConfigurable(PropertyId propertyId, BOOL value)
    {TRACE_IT(55130);
        // Skip JavascriptArray override
        return DynamicObject::SetConfigurable(propertyId, value);
    }

    BOOL ES5Array::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
    {TRACE_IT(55131);
        // Skip JavascriptArray override
        return DynamicObject::SetAttributes(propertyId, attributes);
    }

    BOOL ES5Array::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(55132);
        BOOL result;
        if (GetPropertyBuiltIns(propertyId, value, &result))
        {TRACE_IT(55133);
            return result;
        }

        // Skip JavascriptArray override
        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL ES5Array::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(55134);
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, &result))
        {TRACE_IT(55135);
            return result;
        }

        // Skip JavascriptArray override
        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool ES5Array::GetPropertyBuiltIns(PropertyId propertyId, Var* value, BOOL* result)
    {TRACE_IT(55136);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(55137);
            *value = JavascriptNumber::ToVar(this->GetLength(), GetScriptContext());
            *result = true;
            return true;
        }

        return false;
    }

    BOOL ES5Array::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(55138);
        return ES5Array::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    // Convert a Var to array length, throw RangeError if value is not valid for array length.
    uint32 ES5Array::ToLengthValue(Var value, ScriptContext* scriptContext)
    {TRACE_IT(55139);
        if (TaggedInt::Is(value))
        {TRACE_IT(55140);
            int32 newLen = TaggedInt::ToInt32(value);
            if (newLen < 0)
            {TRACE_IT(55141);
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
            }
            return static_cast<uint32>(newLen);
        }
        else
        {TRACE_IT(55142);
            uint32 newLen = JavascriptConversion::ToUInt32(value, scriptContext);
            if (newLen != JavascriptConversion::ToNumber(value, scriptContext))
            {TRACE_IT(55143);
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
            }
            return newLen;
        }
    }

    DescriptorFlags ES5Array::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(55144);
        DescriptorFlags result;
        if (GetSetterBuiltIns(propertyId, info, &result))
        {TRACE_IT(55145);
            return result;
        }

        return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags ES5Array::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(55146);
        DescriptorFlags result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetSetterBuiltIns(propertyRecord->GetPropertyId(), info, &result))
        {TRACE_IT(55147);
            return result;
        }

        return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool ES5Array::GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* result)
    {TRACE_IT(55148);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(55149);
            PropertyValueInfo::SetNoCache(info, this);
            *result =  IsLengthWritable() ? WritableData : Data;
            return true;
        }

        return false;
    }

    BOOL ES5Array::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags propertyOperationFlags, PropertyValueInfo* info)
    {TRACE_IT(55150);
        BOOL result;
        if (SetPropertyBuiltIns(propertyId, value, propertyOperationFlags, &result))
        {TRACE_IT(55151);
            return result;
        }

        return __super::SetProperty(propertyId, value, propertyOperationFlags, info);
    }

    BOOL ES5Array::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags propertyOperationFlags, PropertyValueInfo* info)
    {TRACE_IT(55152);
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && SetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, propertyOperationFlags, &result))
        {TRACE_IT(55153);
            return result;
        }

        return __super::SetProperty(propertyNameString, value, propertyOperationFlags, info);
    }

    bool ES5Array::SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags propertyOperationFlags, BOOL* result)
    {TRACE_IT(55154);
        ScriptContext* scriptContext = GetScriptContext();

        if (propertyId == PropertyIds::length)
        {TRACE_IT(55155);
            if (!GetTypeHandler()->IsLengthWritable())
            {TRACE_IT(55156);
                *result = false; // reject
                return true;
            }

            uint32 newLen = ToLengthValue(value, scriptContext);
            GetTypeHandler()->SetLength(this, newLen, propertyOperationFlags);
            *result = true;
            return true;
        }

        return false;
    }

    BOOL ES5Array::SetPropertyWithAttributes(PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(55157);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(55158);
            Assert(attributes == PropertyWritable);
            Assert(IsWritable(propertyId) && !IsConfigurable(propertyId) && !IsEnumerable(propertyId));

            uint32 newLen = ToLengthValue(value, GetScriptContext());
            GetTypeHandler()->SetLength(this, newLen, PropertyOperation_None);
            return true;
        }

        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL ES5Array::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(55159);
        // Skip JavascriptArray override
        return DynamicObject::DeleteItem(index, flags);
    }

    BOOL ES5Array::HasItem(uint32 index)
    {TRACE_IT(55160);
        // Skip JavascriptArray override
        return DynamicObject::HasItem(index);
    }

    BOOL ES5Array::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(55161);
        // Skip JavascriptArray override
        return DynamicObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL ES5Array::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(55162);
        // Skip JavascriptArray override
        return DynamicObject::GetItemReference(originalInstance, index, value, requestContext);
    }

    DescriptorFlags ES5Array::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {TRACE_IT(55163);
        // Skip JavascriptArray override
        return DynamicObject::GetItemSetter(index, setterValue, requestContext);
    }

    BOOL ES5Array::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(55164);
        // Skip JavascriptArray override
        return DynamicObject::SetItem(index, value, flags);
    }

    BOOL ES5Array::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(55165);
        // Skip JavascriptArray override
        return DynamicObject::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL ES5Array::PreventExtensions()
    {TRACE_IT(55166);
        // Skip JavascriptArray override
        return DynamicObject::PreventExtensions();
    }

    BOOL ES5Array::Seal()
    {TRACE_IT(55167);
        // Skip JavascriptArray override
        return DynamicObject::Seal();
    }

    BOOL ES5Array::Freeze()
    {TRACE_IT(55168);
        // Skip JavascriptArray override
        return DynamicObject::Freeze();
    }

    BOOL ES5Array::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(55169);
        return enumerator->Initialize(nullptr, this, this, flags, requestContext, forInCache);
    }

    JavascriptEnumerator * ES5Array::GetIndexEnumerator(EnumeratorFlags flags, ScriptContext* requestContext)
    {TRACE_IT(55170);
        // ES5Array does not support compat mode, ignore preferSnapshotSemantics
        return RecyclerNew(GetScriptContext()->GetRecycler(), ES5ArrayIndexEnumerator, this, flags, requestContext);
    }

    BOOL ES5Array::IsItemEnumerable(uint32 index)
    {TRACE_IT(55171);
        return GetTypeHandler()->IsItemEnumerable(this, index);
    }

    BOOL ES5Array::SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {TRACE_IT(55172);
        return GetTypeHandler()->SetItemWithAttributes(this, index, value, attributes);
    }

    BOOL ES5Array::SetItemAttributes(uint32 index, PropertyAttributes attributes)
    {TRACE_IT(55173);
        return GetTypeHandler()->SetItemAttributes(this, index, attributes);
    }

    BOOL ES5Array::SetItemAccessors(uint32 index, Var getter, Var setter)
    {TRACE_IT(55174);
        return GetTypeHandler()->SetItemAccessors(this, index, getter, setter);
    }

    BOOL ES5Array::IsObjectArrayFrozen()
    {TRACE_IT(55175);
        return GetTypeHandler()->IsObjectArrayFrozen(this);
    }

    BOOL ES5Array::IsValidDescriptorToken(void * descriptorValidationToken) const
    {TRACE_IT(55176);
        return GetTypeHandler()->IsValidDescriptorToken(descriptorValidationToken);
    }
    uint32 ES5Array::GetNextDescriptor(uint32 key, IndexPropertyDescriptor** descriptor, void ** descriptorValidationToken)
    {TRACE_IT(55177);
        return GetTypeHandler()->GetNextDescriptor(key, descriptor, descriptorValidationToken);
    }

    BOOL ES5Array::GetDescriptor(uint32 index, Js::IndexPropertyDescriptor **ppDescriptor)
    {TRACE_IT(55178);
        return GetTypeHandler()->GetDescriptor(index, ppDescriptor);
    }

#if ENABLE_TTD
    void ES5Array::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(55179);
        this->JavascriptArray::MarkVisitKindSpecificPtrs(extractor);

        uint32 length = this->GetLength();
        uint32 descriptorIndex = Js::JavascriptArray::InvalidIndex;
        IndexPropertyDescriptor* descriptor = nullptr;
        void* descriptorValidationToken = nullptr;

        do
        {TRACE_IT(55180);
            descriptorIndex = this->GetNextDescriptor(descriptorIndex, &descriptor, &descriptorValidationToken);
            if(descriptorIndex == Js::JavascriptArray::InvalidIndex || descriptorIndex >= length)
            {TRACE_IT(55181);
                break;
            }

            if((descriptor->Attributes & PropertyDeleted) != PropertyDeleted)
            {TRACE_IT(55182);
                if(descriptor->Getter != nullptr)
                {TRACE_IT(55183);
                    extractor->MarkVisitVar(descriptor->Getter);
                }

                if(descriptor->Setter != nullptr)
                {TRACE_IT(55184);
                    extractor->MarkVisitVar(descriptor->Setter);
                }
            }

        } while(true);
    }

    TTD::NSSnapObjects::SnapObjectType ES5Array::GetSnapTag_TTD() const
    {TRACE_IT(55185);
        return TTD::NSSnapObjects::SnapObjectType::SnapES5ArrayObject;
    }

    void ES5Array::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(55186);
        TTD::NSSnapObjects::SnapArrayInfo<TTD::TTDVar>* sai = TTD::NSSnapObjects::ExtractArrayValues<TTD::TTDVar>(this, alloc);

        TTD::NSSnapObjects::SnapES5ArrayInfo* es5ArrayInfo = alloc.SlabAllocateStruct<TTD::NSSnapObjects::SnapES5ArrayInfo>();

        //
        //TODO: reserving memory for entire length might be a problem if we have very large sparse arrays.
        //

        uint32 length = this->GetLength();

        es5ArrayInfo->IsLengthWritable = this->IsLengthWritable();
        es5ArrayInfo->GetterSetterCount = 0;
        es5ArrayInfo->GetterSetterEntries = alloc.SlabReserveArraySpace<TTD::NSSnapObjects::SnapES5ArrayGetterSetterEntry>(length + 1); //ensure we don't do a 0 reserve

        uint32 descriptorIndex = Js::JavascriptArray::InvalidIndex;
        IndexPropertyDescriptor* descriptor = nullptr;
        void* descriptorValidationToken = nullptr;

        do
        {TRACE_IT(55187);
            descriptorIndex = this->GetNextDescriptor(descriptorIndex, &descriptor, &descriptorValidationToken);
            if(descriptorIndex == Js::JavascriptArray::InvalidIndex || descriptorIndex >= length)
            {TRACE_IT(55188);
                break;
            }

            if((descriptor->Attributes & PropertyDeleted) != PropertyDeleted)
            {TRACE_IT(55189);
                TTD::NSSnapObjects::SnapES5ArrayGetterSetterEntry* entry = es5ArrayInfo->GetterSetterEntries + es5ArrayInfo->GetterSetterCount;
                es5ArrayInfo->GetterSetterCount++;

                entry->Index = (uint32)descriptorIndex;
                entry->Attributes = descriptor->Attributes;

                entry->Getter = nullptr;
                if(descriptor->Getter != nullptr)
                {TRACE_IT(55190);
                    entry->Getter = descriptor->Getter;
                }

                entry->Setter = nullptr;
                if(descriptor->Setter != nullptr)
                {TRACE_IT(55191);
                    entry->Setter = descriptor->Setter;
                }
            }

        } while(true);

        if(es5ArrayInfo->GetterSetterCount != 0)
        {TRACE_IT(55192);
            alloc.SlabCommitArraySpace<TTD::NSSnapObjects::SnapES5ArrayGetterSetterEntry>(es5ArrayInfo->GetterSetterCount, length + 1);
        }
        else
        {TRACE_IT(55193);
            alloc.SlabAbortArraySpace<TTD::NSSnapObjects::SnapES5ArrayGetterSetterEntry>(length + 1);
            es5ArrayInfo->GetterSetterEntries = nullptr;
        }

        es5ArrayInfo->BasicArrayData = sai;

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapES5ArrayInfo*, TTD::NSSnapObjects::SnapObjectType::SnapES5ArrayObject>(objData, es5ArrayInfo);
    }
#endif
}
