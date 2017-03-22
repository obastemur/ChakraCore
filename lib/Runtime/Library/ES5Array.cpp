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
    {LOGMEIN("ES5Array.cpp] 12\n");
    }

    bool ES5Array::Is(Var instance)
    {LOGMEIN("ES5Array.cpp] 16\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_ES5Array;
    }

    ES5Array* ES5Array::FromVar(Var instance)
    {LOGMEIN("ES5Array.cpp] 21\n");
        Assert(Is(instance));
        return static_cast<ES5Array*>(instance);
    }

    DynamicType* ES5Array::DuplicateType()
    {LOGMEIN("ES5Array.cpp] 27\n");
        return RecyclerNew(GetScriptContext()->GetRecycler(), ES5ArrayType, this->GetDynamicType());
    }

    bool ES5Array::IsLengthWritable() const
    {LOGMEIN("ES5Array.cpp] 32\n");
        return GetTypeHandler()->IsLengthWritable();
    }

    BOOL ES5Array::HasProperty(PropertyId propertyId)
    {LOGMEIN("ES5Array.cpp] 37\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5Array.cpp] 39\n");
            return true;
        }

        // Skip JavascriptArray override
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL ES5Array::IsWritable(PropertyId propertyId)
    {LOGMEIN("ES5Array.cpp] 48\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5Array.cpp] 50\n");
            return IsLengthWritable();
        }

        return __super::IsWritable(propertyId);
    }

    BOOL ES5Array::SetEnumerable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ES5Array.cpp] 58\n");
        // Skip JavascriptArray override
        return DynamicObject::SetEnumerable(propertyId, value);
    }

    BOOL ES5Array::SetWritable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ES5Array.cpp] 64\n");
        // Skip JavascriptArray override
        return DynamicObject::SetWritable(propertyId, value);
    }

    BOOL ES5Array::SetConfigurable(PropertyId propertyId, BOOL value)
    {LOGMEIN("ES5Array.cpp] 70\n");
        // Skip JavascriptArray override
        return DynamicObject::SetConfigurable(propertyId, value);
    }

    BOOL ES5Array::SetAttributes(PropertyId propertyId, PropertyAttributes attributes)
    {LOGMEIN("ES5Array.cpp] 76\n");
        // Skip JavascriptArray override
        return DynamicObject::SetAttributes(propertyId, attributes);
    }

    BOOL ES5Array::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5Array.cpp] 82\n");
        BOOL result;
        if (GetPropertyBuiltIns(propertyId, value, &result))
        {LOGMEIN("ES5Array.cpp] 85\n");
            return result;
        }

        // Skip JavascriptArray override
        return DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    BOOL ES5Array::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5Array.cpp] 94\n");
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, &result))
        {LOGMEIN("ES5Array.cpp] 100\n");
            return result;
        }

        // Skip JavascriptArray override
        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool ES5Array::GetPropertyBuiltIns(PropertyId propertyId, Var* value, BOOL* result)
    {LOGMEIN("ES5Array.cpp] 109\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5Array.cpp] 111\n");
            *value = JavascriptNumber::ToVar(this->GetLength(), GetScriptContext());
            *result = true;
            return true;
        }

        return false;
    }

    BOOL ES5Array::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5Array.cpp] 121\n");
        return ES5Array::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    // Convert a Var to array length, throw RangeError if value is not valid for array length.
    uint32 ES5Array::ToLengthValue(Var value, ScriptContext* scriptContext)
    {LOGMEIN("ES5Array.cpp] 127\n");
        if (TaggedInt::Is(value))
        {LOGMEIN("ES5Array.cpp] 129\n");
            int32 newLen = TaggedInt::ToInt32(value);
            if (newLen < 0)
            {LOGMEIN("ES5Array.cpp] 132\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
            }
            return static_cast<uint32>(newLen);
        }
        else
        {
            uint32 newLen = JavascriptConversion::ToUInt32(value, scriptContext);
            if (newLen != JavascriptConversion::ToNumber(value, scriptContext))
            {LOGMEIN("ES5Array.cpp] 141\n");
                JavascriptError::ThrowRangeError(scriptContext, JSERR_ArrayLengthAssignIncorrect);
            }
            return newLen;
        }
    }

    DescriptorFlags ES5Array::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5Array.cpp] 149\n");
        DescriptorFlags result;
        if (GetSetterBuiltIns(propertyId, info, &result))
        {LOGMEIN("ES5Array.cpp] 152\n");
            return result;
        }

        return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags ES5Array::GetSetter(JavascriptString* propertyNameString, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ES5Array.cpp] 160\n");
        DescriptorFlags result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetSetterBuiltIns(propertyRecord->GetPropertyId(), info, &result))
        {LOGMEIN("ES5Array.cpp] 166\n");
            return result;
        }

        return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool ES5Array::GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* result)
    {LOGMEIN("ES5Array.cpp] 174\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5Array.cpp] 176\n");
            PropertyValueInfo::SetNoCache(info, this);
            *result =  IsLengthWritable() ? WritableData : Data;
            return true;
        }

        return false;
    }

    BOOL ES5Array::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags propertyOperationFlags, PropertyValueInfo* info)
    {LOGMEIN("ES5Array.cpp] 186\n");
        BOOL result;
        if (SetPropertyBuiltIns(propertyId, value, propertyOperationFlags, &result))
        {LOGMEIN("ES5Array.cpp] 189\n");
            return result;
        }

        return __super::SetProperty(propertyId, value, propertyOperationFlags, info);
    }

    BOOL ES5Array::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags propertyOperationFlags, PropertyValueInfo* info)
    {LOGMEIN("ES5Array.cpp] 197\n");
        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && SetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, propertyOperationFlags, &result))
        {LOGMEIN("ES5Array.cpp] 203\n");
            return result;
        }

        return __super::SetProperty(propertyNameString, value, propertyOperationFlags, info);
    }

    bool ES5Array::SetPropertyBuiltIns(PropertyId propertyId, Var value, PropertyOperationFlags propertyOperationFlags, BOOL* result)
    {LOGMEIN("ES5Array.cpp] 211\n");
        ScriptContext* scriptContext = GetScriptContext();

        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5Array.cpp] 215\n");
            if (!GetTypeHandler()->IsLengthWritable())
            {LOGMEIN("ES5Array.cpp] 217\n");
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
    {LOGMEIN("ES5Array.cpp] 232\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("ES5Array.cpp] 234\n");
            Assert(attributes == PropertyWritable);
            Assert(IsWritable(propertyId) && !IsConfigurable(propertyId) && !IsEnumerable(propertyId));

            uint32 newLen = ToLengthValue(value, GetScriptContext());
            GetTypeHandler()->SetLength(this, newLen, PropertyOperation_None);
            return true;
        }

        return __super::SetPropertyWithAttributes(propertyId, value, attributes, info, flags, possibleSideEffects);
    }

    BOOL ES5Array::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("ES5Array.cpp] 247\n");
        // Skip JavascriptArray override
        return DynamicObject::DeleteItem(index, flags);
    }

    BOOL ES5Array::HasItem(uint32 index)
    {LOGMEIN("ES5Array.cpp] 253\n");
        // Skip JavascriptArray override
        return DynamicObject::HasItem(index);
    }

    BOOL ES5Array::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {LOGMEIN("ES5Array.cpp] 259\n");
        // Skip JavascriptArray override
        return DynamicObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL ES5Array::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {LOGMEIN("ES5Array.cpp] 265\n");
        // Skip JavascriptArray override
        return DynamicObject::GetItemReference(originalInstance, index, value, requestContext);
    }

    DescriptorFlags ES5Array::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("ES5Array.cpp] 271\n");
        // Skip JavascriptArray override
        return DynamicObject::GetItemSetter(index, setterValue, requestContext);
    }

    BOOL ES5Array::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("ES5Array.cpp] 277\n");
        // Skip JavascriptArray override
        return DynamicObject::SetItem(index, value, flags);
    }

    BOOL ES5Array::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("ES5Array.cpp] 283\n");
        // Skip JavascriptArray override
        return DynamicObject::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL ES5Array::PreventExtensions()
    {LOGMEIN("ES5Array.cpp] 289\n");
        // Skip JavascriptArray override
        return DynamicObject::PreventExtensions();
    }

    BOOL ES5Array::Seal()
    {LOGMEIN("ES5Array.cpp] 295\n");
        // Skip JavascriptArray override
        return DynamicObject::Seal();
    }

    BOOL ES5Array::Freeze()
    {LOGMEIN("ES5Array.cpp] 301\n");
        // Skip JavascriptArray override
        return DynamicObject::Freeze();
    }

    BOOL ES5Array::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("ES5Array.cpp] 307\n");
        return enumerator->Initialize(nullptr, this, this, flags, requestContext, forInCache);
    }

    JavascriptEnumerator * ES5Array::GetIndexEnumerator(EnumeratorFlags flags, ScriptContext* requestContext)
    {LOGMEIN("ES5Array.cpp] 312\n");
        // ES5Array does not support compat mode, ignore preferSnapshotSemantics
        return RecyclerNew(GetScriptContext()->GetRecycler(), ES5ArrayIndexEnumerator, this, flags, requestContext);
    }

    BOOL ES5Array::IsItemEnumerable(uint32 index)
    {LOGMEIN("ES5Array.cpp] 318\n");
        return GetTypeHandler()->IsItemEnumerable(this, index);
    }

    BOOL ES5Array::SetItemWithAttributes(uint32 index, Var value, PropertyAttributes attributes)
    {LOGMEIN("ES5Array.cpp] 323\n");
        return GetTypeHandler()->SetItemWithAttributes(this, index, value, attributes);
    }

    BOOL ES5Array::SetItemAttributes(uint32 index, PropertyAttributes attributes)
    {LOGMEIN("ES5Array.cpp] 328\n");
        return GetTypeHandler()->SetItemAttributes(this, index, attributes);
    }

    BOOL ES5Array::SetItemAccessors(uint32 index, Var getter, Var setter)
    {LOGMEIN("ES5Array.cpp] 333\n");
        return GetTypeHandler()->SetItemAccessors(this, index, getter, setter);
    }

    BOOL ES5Array::IsObjectArrayFrozen()
    {LOGMEIN("ES5Array.cpp] 338\n");
        return GetTypeHandler()->IsObjectArrayFrozen(this);
    }

    BOOL ES5Array::IsValidDescriptorToken(void * descriptorValidationToken) const
    {LOGMEIN("ES5Array.cpp] 343\n");
        return GetTypeHandler()->IsValidDescriptorToken(descriptorValidationToken);
    }
    uint32 ES5Array::GetNextDescriptor(uint32 key, IndexPropertyDescriptor** descriptor, void ** descriptorValidationToken)
    {LOGMEIN("ES5Array.cpp] 347\n");
        return GetTypeHandler()->GetNextDescriptor(key, descriptor, descriptorValidationToken);
    }

    BOOL ES5Array::GetDescriptor(uint32 index, Js::IndexPropertyDescriptor **ppDescriptor)
    {LOGMEIN("ES5Array.cpp] 352\n");
        return GetTypeHandler()->GetDescriptor(index, ppDescriptor);
    }

#if ENABLE_TTD
    void ES5Array::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("ES5Array.cpp] 358\n");
        this->JavascriptArray::MarkVisitKindSpecificPtrs(extractor);

        uint32 length = this->GetLength();
        uint32 descriptorIndex = Js::JavascriptArray::InvalidIndex;
        IndexPropertyDescriptor* descriptor = nullptr;
        void* descriptorValidationToken = nullptr;

        do
        {LOGMEIN("ES5Array.cpp] 367\n");
            descriptorIndex = this->GetNextDescriptor(descriptorIndex, &descriptor, &descriptorValidationToken);
            if(descriptorIndex == Js::JavascriptArray::InvalidIndex || descriptorIndex >= length)
            {LOGMEIN("ES5Array.cpp] 370\n");
                break;
            }

            if((descriptor->Attributes & PropertyDeleted) != PropertyDeleted)
            {LOGMEIN("ES5Array.cpp] 375\n");
                if(descriptor->Getter != nullptr)
                {LOGMEIN("ES5Array.cpp] 377\n");
                    extractor->MarkVisitVar(descriptor->Getter);
                }

                if(descriptor->Setter != nullptr)
                {LOGMEIN("ES5Array.cpp] 382\n");
                    extractor->MarkVisitVar(descriptor->Setter);
                }
            }

        } while(true);
    }

    TTD::NSSnapObjects::SnapObjectType ES5Array::GetSnapTag_TTD() const
    {LOGMEIN("ES5Array.cpp] 391\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapES5ArrayObject;
    }

    void ES5Array::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ES5Array.cpp] 396\n");
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
        {LOGMEIN("ES5Array.cpp] 416\n");
            descriptorIndex = this->GetNextDescriptor(descriptorIndex, &descriptor, &descriptorValidationToken);
            if(descriptorIndex == Js::JavascriptArray::InvalidIndex || descriptorIndex >= length)
            {LOGMEIN("ES5Array.cpp] 419\n");
                break;
            }

            if((descriptor->Attributes & PropertyDeleted) != PropertyDeleted)
            {LOGMEIN("ES5Array.cpp] 424\n");
                TTD::NSSnapObjects::SnapES5ArrayGetterSetterEntry* entry = es5ArrayInfo->GetterSetterEntries + es5ArrayInfo->GetterSetterCount;
                es5ArrayInfo->GetterSetterCount++;

                entry->Index = (uint32)descriptorIndex;
                entry->Attributes = descriptor->Attributes;

                entry->Getter = nullptr;
                if(descriptor->Getter != nullptr)
                {LOGMEIN("ES5Array.cpp] 433\n");
                    entry->Getter = descriptor->Getter;
                }

                entry->Setter = nullptr;
                if(descriptor->Setter != nullptr)
                {LOGMEIN("ES5Array.cpp] 439\n");
                    entry->Setter = descriptor->Setter;
                }
            }

        } while(true);

        if(es5ArrayInfo->GetterSetterCount != 0)
        {LOGMEIN("ES5Array.cpp] 447\n");
            alloc.SlabCommitArraySpace<TTD::NSSnapObjects::SnapES5ArrayGetterSetterEntry>(es5ArrayInfo->GetterSetterCount, length + 1);
        }
        else
        {
            alloc.SlabAbortArraySpace<TTD::NSSnapObjects::SnapES5ArrayGetterSetterEntry>(length + 1);
            es5ArrayInfo->GetterSetterEntries = nullptr;
        }

        es5ArrayInfo->BasicArrayData = sai;

        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::NSSnapObjects::SnapES5ArrayInfo*, TTD::NSSnapObjects::SnapObjectType::SnapES5ArrayObject>(objData, es5ArrayInfo);
    }
#endif
}
