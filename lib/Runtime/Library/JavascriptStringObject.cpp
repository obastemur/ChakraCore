//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptStringObject::JavascriptStringObject(DynamicType * type)
        : DynamicObject(type), value(nullptr)
    {TRACE_IT(62064);
        Assert(type->GetTypeId() == TypeIds_StringObject);

        this->GetTypeHandler()->ClearHasOnlyWritableDataProperties(); // length is non-writable
        if(GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {TRACE_IT(62065);

            // No need to invalidate store field caches for non-writable properties here. Since this type is just being created, it cannot represent
            // an object that is already a prototype. If it becomes a prototype and then we attempt to add a property to an object derived from this
            // object, then we will check if this property is writable, and only if it is will we do the fast path for add property.
            GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
    }

    JavascriptStringObject::JavascriptStringObject(JavascriptString* value, DynamicType * type)
        : DynamicObject(type), value(value)
    {TRACE_IT(62066);
        Assert(type->GetTypeId() == TypeIds_StringObject);

        this->GetTypeHandler()->ClearHasOnlyWritableDataProperties(); // length is non-writable
        if(GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {TRACE_IT(62067);
            // No need to invalidate store field caches for non-writable properties here. Since this type is just being created, it cannot represent
            // an object that is already a prototype. If it becomes a prototype and then we attempt to add a property to an object derived from this
            // object, then we will check if this property is writable, and only if it is will we do the fast path for add property.
            GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
    }

    bool JavascriptStringObject::Is(Var aValue)
    {TRACE_IT(62068);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_StringObject;
    }

    JavascriptStringObject* JavascriptStringObject::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptString'");

        return static_cast<JavascriptStringObject *>(RecyclableObject::FromVar(aValue));
    }

    void JavascriptStringObject::Initialize(JavascriptString* value)
    {TRACE_IT(62069);
        Assert(this->value == nullptr);

        this->value = value;
    }

    JavascriptString* JavascriptStringObject::InternalUnwrap()
    {TRACE_IT(62070);
        if (value == nullptr)
        {TRACE_IT(62071);
            ScriptContext* scriptContext = GetScriptContext();
            value = scriptContext->GetLibrary()->GetEmptyString();
        }

        return value;
    }

    /*static*/
    PropertyId const JavascriptStringObject::specialPropertyIds[] =
    {
        PropertyIds::length
    };

    bool JavascriptStringObject::IsValidIndex(PropertyId propertyId, bool conditionMetBehavior)
    {TRACE_IT(62072);
        ScriptContext*scriptContext = GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(62073);
            if (index < (uint32)this->InternalUnwrap()->GetLength())
            {TRACE_IT(62074);
                return conditionMetBehavior;
            }
        }
        return !conditionMetBehavior;
    }

    BOOL JavascriptStringObject::HasProperty(PropertyId propertyId)
    {TRACE_IT(62075);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(62076);
            return true;
        }

        if (DynamicObject::HasProperty(propertyId))
        {TRACE_IT(62077);
            return true;
        }

        return JavascriptStringObject::IsValidIndex(propertyId, true);
    }

    DescriptorFlags JavascriptStringObject::GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62078);
        DescriptorFlags flags;
        if (GetSetterBuiltIns(propertyId, info, &flags))
        {TRACE_IT(62079);
            return flags;
        }

        uint32 index;
        if (requestContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(62080);
            return JavascriptStringObject::GetItemSetter(index, setterValue, requestContext);
        }

        return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags JavascriptStringObject::GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62081);
        DescriptorFlags flags;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr)
        {TRACE_IT(62082);
            PropertyId propertyId = propertyRecord->GetPropertyId();
            if (GetSetterBuiltIns(propertyId, info, &flags))
            {TRACE_IT(62083);
                return flags;
            }

            uint32 index;
            if (requestContext->IsNumericPropertyId(propertyId, &index))
            {TRACE_IT(62084);
                return JavascriptStringObject::GetItemSetter(index, setterValue, requestContext);
            }
        }

        return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool JavascriptStringObject::GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* descriptorFlags)
    {TRACE_IT(62085);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(62086);
            PropertyValueInfo::SetNoCache(info, this);
            *descriptorFlags = Data;
            return true;
        }

        return false;
    }

    BOOL JavascriptStringObject::IsConfigurable(PropertyId propertyId)
    {TRACE_IT(62087);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(62088);
            return false;
        }

        // From DynamicObject::IsConfigurable we can't tell if the result is from a property or just default
        // value. Call HasProperty to find out.
        if (DynamicObject::HasProperty(propertyId))
        {TRACE_IT(62089);
            return DynamicObject::IsConfigurable(propertyId);
        }

        return JavascriptStringObject::IsValidIndex(propertyId, false);
    }

    BOOL JavascriptStringObject::IsEnumerable(PropertyId propertyId)
    {TRACE_IT(62090);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(62091);
            return false;
        }

        // Index properties of String objects are always enumerable, same as default value. No need to test.
        return DynamicObject::IsEnumerable(propertyId);
    }

    BOOL JavascriptStringObject::IsWritable(PropertyId propertyId)
    {TRACE_IT(62092);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(62093);
            return false;
        }

        // From DynamicObject::IsWritable we can't tell if the result is from a property or just default
        // value. Call HasProperty to find out.
        if (DynamicObject::HasProperty(propertyId))
        {TRACE_IT(62094);
            return DynamicObject::IsWritable(propertyId);
        }

        return JavascriptStringObject::IsValidIndex(propertyId, false);
    }

    BOOL JavascriptStringObject::GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext)
    {TRACE_IT(62095);
        if (index == 0)
        {TRACE_IT(62096);
            *propertyName = requestContext->GetPropertyString(PropertyIds::length);
            return true;
        }
        return false;
    }

    // Returns the number of special non-enumerable properties this type has.
    uint JavascriptStringObject::GetSpecialPropertyCount() const
    {TRACE_IT(62097);
        return _countof(specialPropertyIds);
    }

    // Returns the list of special non-enumerable properties for the type.
    PropertyId const * JavascriptStringObject::GetSpecialPropertyIds() const
    {TRACE_IT(62098);
        return specialPropertyIds;
    }

    BOOL JavascriptStringObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62099);
        return JavascriptStringObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }


    BOOL JavascriptStringObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62100);
        BOOL result;
        if (GetPropertyBuiltIns(propertyId, value, requestContext, &result))
        {TRACE_IT(62101);
            return result;
        }

        if (DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {TRACE_IT(62102);
            return true;
        }

        // For NumericPropertyIds check that index is less than JavascriptString length
        ScriptContext*scriptContext = GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {TRACE_IT(62103);
            JavascriptString* str = JavascriptString::FromVar(CrossSite::MarshalVar(requestContext, this->InternalUnwrap()));
            return str->GetItemAt(index, value);
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL JavascriptStringObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62104);
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, requestContext, &result))
        {TRACE_IT(62105);
            return result;
        }

        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool JavascriptStringObject::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext, BOOL* result)
    {TRACE_IT(62106);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(62107);
            *value = JavascriptNumber::ToVar(this->InternalUnwrap()->GetLength(), requestContext);
            *result = true;
            return true;
        }

        return false;
    }

    BOOL JavascriptStringObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(62108);
        bool result;
        if (SetPropertyBuiltIns(propertyId, flags, &result))
        {TRACE_IT(62109);
            return result;
        }

        return DynamicObject::SetProperty(propertyId, value, flags, info);
    }

    BOOL JavascriptStringObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(62110);
        bool result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && SetPropertyBuiltIns(propertyRecord->GetPropertyId(), flags, &result))
        {TRACE_IT(62111);
            return result;
        }
        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    bool JavascriptStringObject::SetPropertyBuiltIns(PropertyId propertyId, PropertyOperationFlags flags, bool* result)
    {TRACE_IT(62112);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(62113);
            JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

            *result = false;
            return true;
        }

        return false;
    }

    BOOL JavascriptStringObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(62114);
        if (propertyId == PropertyIds::length)
        {TRACE_IT(62115);
            JavascriptError::ThrowCantDeleteIfStrictMode(flags, this->GetScriptContext(), this->GetScriptContext()->GetPropertyName(propertyId)->GetBuffer());

            return FALSE;
        }
        return DynamicObject::DeleteProperty(propertyId, flags);
    }

    BOOL JavascriptStringObject::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(62116);
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {TRACE_IT(62117);
            JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, this->GetScriptContext(), propertyNameString->GetString());

            return FALSE;
        }
        return DynamicObject::DeleteProperty(propertyNameString, propertyOperationFlags);
    }

    BOOL JavascriptStringObject::HasItem(uint32 index)
    {TRACE_IT(62118);
        if (this->InternalUnwrap()->HasItem(index))
        {TRACE_IT(62119);
            return true;
        }
        return DynamicObject::HasItem(index);
    }

    BOOL JavascriptStringObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(62120);
        JavascriptString* str = JavascriptString::FromVar(CrossSite::MarshalVar(requestContext, this->InternalUnwrap()));
        if (str->GetItemAt(index, value))
        {TRACE_IT(62121);
            return true;
        }
        return DynamicObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL JavascriptStringObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {TRACE_IT(62122);
        return this->GetItem(originalInstance, index, value, requestContext);
    }

    DescriptorFlags JavascriptStringObject::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {TRACE_IT(62123);
        if (index < (uint32)this->InternalUnwrap()->GetLength())
        {TRACE_IT(62124);
            return DescriptorFlags::Data;
        }
        return DynamicObject::GetItemSetter(index, setterValue, requestContext);
    }

    BOOL JavascriptStringObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(62125);
        if (index < (uint32)this->InternalUnwrap()->GetLength())
        {TRACE_IT(62126);
            return false;
        }
        return DynamicObject::SetItem(index, value, flags);
    }

    BOOL JavascriptStringObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {TRACE_IT(62127);
        return GetEnumeratorWithPrefix(
            RecyclerNew(GetScriptContext()->GetRecycler(), JavascriptStringEnumerator, this->Unwrap(), requestContext),
            enumerator, flags, requestContext, forInCache);
    }

    BOOL JavascriptStringObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(62128);
        stringBuilder->Append(_u('"'));
        stringBuilder->Append(this->InternalUnwrap()->GetString(), this->InternalUnwrap()->GetLength());
        stringBuilder->Append(_u('"'));
        return TRUE;
    }

    BOOL JavascriptStringObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(62129);
        stringBuilder->AppendCppLiteral(_u("String"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptStringObject::SetValue_TTD(Js::Var val)
    {TRACE_IT(62130);
        AssertMsg(val == nullptr || Js::JavascriptString::Is(val), "Only legal values!");

        this->value = static_cast<Js::JavascriptString*>(val);
    }

    void JavascriptStringObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {TRACE_IT(62131);
        if(this->value != nullptr)
        {TRACE_IT(62132);
            extractor->MarkVisitVar(this->value);
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptStringObject::GetSnapTag_TTD() const
    {TRACE_IT(62133);
        return TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject;
    }

    void JavascriptStringObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(62134);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject>(objData, this->value);
    }
#endif
} // namespace Js
