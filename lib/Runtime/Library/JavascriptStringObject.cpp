//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptStringObject::JavascriptStringObject(DynamicType * type)
        : DynamicObject(type), value(nullptr)
    {LOGMEIN("JavascriptStringObject.cpp] 10\n");
        Assert(type->GetTypeId() == TypeIds_StringObject);

        this->GetTypeHandler()->ClearHasOnlyWritableDataProperties(); // length is non-writable
        if(GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {LOGMEIN("JavascriptStringObject.cpp] 15\n");

            // No need to invalidate store field caches for non-writable properties here. Since this type is just being created, it cannot represent
            // an object that is already a prototype. If it becomes a prototype and then we attempt to add a property to an object derived from this
            // object, then we will check if this property is writable, and only if it is will we do the fast path for add property.
            GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
    }

    JavascriptStringObject::JavascriptStringObject(JavascriptString* value, DynamicType * type)
        : DynamicObject(type), value(value)
    {LOGMEIN("JavascriptStringObject.cpp] 26\n");
        Assert(type->GetTypeId() == TypeIds_StringObject);

        this->GetTypeHandler()->ClearHasOnlyWritableDataProperties(); // length is non-writable
        if(GetTypeHandler()->GetFlags() & DynamicTypeHandler::IsPrototypeFlag)
        {LOGMEIN("JavascriptStringObject.cpp] 31\n");
            // No need to invalidate store field caches for non-writable properties here. Since this type is just being created, it cannot represent
            // an object that is already a prototype. If it becomes a prototype and then we attempt to add a property to an object derived from this
            // object, then we will check if this property is writable, and only if it is will we do the fast path for add property.
            GetLibrary()->NoPrototypeChainsAreEnsuredToHaveOnlyWritableDataProperties();
        }
    }

    bool JavascriptStringObject::Is(Var aValue)
    {LOGMEIN("JavascriptStringObject.cpp] 40\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_StringObject;
    }

    JavascriptStringObject* JavascriptStringObject::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptString'");

        return static_cast<JavascriptStringObject *>(RecyclableObject::FromVar(aValue));
    }

    void JavascriptStringObject::Initialize(JavascriptString* value)
    {LOGMEIN("JavascriptStringObject.cpp] 52\n");
        Assert(this->value == nullptr);

        this->value = value;
    }

    JavascriptString* JavascriptStringObject::InternalUnwrap()
    {LOGMEIN("JavascriptStringObject.cpp] 59\n");
        if (value == nullptr)
        {LOGMEIN("JavascriptStringObject.cpp] 61\n");
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
    {LOGMEIN("JavascriptStringObject.cpp] 76\n");
        ScriptContext*scriptContext = GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptStringObject.cpp] 80\n");
            if (index < (uint32)this->InternalUnwrap()->GetLength())
            {LOGMEIN("JavascriptStringObject.cpp] 82\n");
                return conditionMetBehavior;
            }
        }
        return !conditionMetBehavior;
    }

    BOOL JavascriptStringObject::HasProperty(PropertyId propertyId)
    {LOGMEIN("JavascriptStringObject.cpp] 90\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptStringObject.cpp] 92\n");
            return true;
        }

        if (DynamicObject::HasProperty(propertyId))
        {LOGMEIN("JavascriptStringObject.cpp] 97\n");
            return true;
        }

        return JavascriptStringObject::IsValidIndex(propertyId, true);
    }

    DescriptorFlags JavascriptStringObject::GetSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 105\n");
        DescriptorFlags flags;
        if (GetSetterBuiltIns(propertyId, info, &flags))
        {LOGMEIN("JavascriptStringObject.cpp] 108\n");
            return flags;
        }

        uint32 index;
        if (requestContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptStringObject.cpp] 114\n");
            return JavascriptStringObject::GetItemSetter(index, setterValue, requestContext);
        }

        return DynamicObject::GetSetter(propertyId, setterValue, info, requestContext);
    }

    DescriptorFlags JavascriptStringObject::GetSetter(JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 122\n");
        DescriptorFlags flags;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr)
        {LOGMEIN("JavascriptStringObject.cpp] 128\n");
            PropertyId propertyId = propertyRecord->GetPropertyId();
            if (GetSetterBuiltIns(propertyId, info, &flags))
            {LOGMEIN("JavascriptStringObject.cpp] 131\n");
                return flags;
            }

            uint32 index;
            if (requestContext->IsNumericPropertyId(propertyId, &index))
            {LOGMEIN("JavascriptStringObject.cpp] 137\n");
                return JavascriptStringObject::GetItemSetter(index, setterValue, requestContext);
            }
        }

        return DynamicObject::GetSetter(propertyNameString, setterValue, info, requestContext);
    }

    bool JavascriptStringObject::GetSetterBuiltIns(PropertyId propertyId, PropertyValueInfo* info, DescriptorFlags* descriptorFlags)
    {LOGMEIN("JavascriptStringObject.cpp] 146\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptStringObject.cpp] 148\n");
            PropertyValueInfo::SetNoCache(info, this);
            *descriptorFlags = Data;
            return true;
        }

        return false;
    }

    BOOL JavascriptStringObject::IsConfigurable(PropertyId propertyId)
    {LOGMEIN("JavascriptStringObject.cpp] 158\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptStringObject.cpp] 160\n");
            return false;
        }

        // From DynamicObject::IsConfigurable we can't tell if the result is from a property or just default
        // value. Call HasProperty to find out.
        if (DynamicObject::HasProperty(propertyId))
        {LOGMEIN("JavascriptStringObject.cpp] 167\n");
            return DynamicObject::IsConfigurable(propertyId);
        }

        return JavascriptStringObject::IsValidIndex(propertyId, false);
    }

    BOOL JavascriptStringObject::IsEnumerable(PropertyId propertyId)
    {LOGMEIN("JavascriptStringObject.cpp] 175\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptStringObject.cpp] 177\n");
            return false;
        }

        // Index properties of String objects are always enumerable, same as default value. No need to test.
        return DynamicObject::IsEnumerable(propertyId);
    }

    BOOL JavascriptStringObject::IsWritable(PropertyId propertyId)
    {LOGMEIN("JavascriptStringObject.cpp] 186\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptStringObject.cpp] 188\n");
            return false;
        }

        // From DynamicObject::IsWritable we can't tell if the result is from a property or just default
        // value. Call HasProperty to find out.
        if (DynamicObject::HasProperty(propertyId))
        {LOGMEIN("JavascriptStringObject.cpp] 195\n");
            return DynamicObject::IsWritable(propertyId);
        }

        return JavascriptStringObject::IsValidIndex(propertyId, false);
    }

    BOOL JavascriptStringObject::GetSpecialPropertyName(uint32 index, Var *propertyName, ScriptContext * requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 203\n");
        if (index == 0)
        {LOGMEIN("JavascriptStringObject.cpp] 205\n");
            *propertyName = requestContext->GetPropertyString(PropertyIds::length);
            return true;
        }
        return false;
    }

    // Returns the number of special non-enumerable properties this type has.
    uint JavascriptStringObject::GetSpecialPropertyCount() const
    {LOGMEIN("JavascriptStringObject.cpp] 214\n");
        return _countof(specialPropertyIds);
    }

    // Returns the list of special non-enumerable properties for the type.
    PropertyId const * JavascriptStringObject::GetSpecialPropertyIds() const
    {LOGMEIN("JavascriptStringObject.cpp] 220\n");
        return specialPropertyIds;
    }

    BOOL JavascriptStringObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 225\n");
        return JavascriptStringObject::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }


    BOOL JavascriptStringObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 231\n");
        BOOL result;
        if (GetPropertyBuiltIns(propertyId, value, requestContext, &result))
        {LOGMEIN("JavascriptStringObject.cpp] 234\n");
            return result;
        }

        if (DynamicObject::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {LOGMEIN("JavascriptStringObject.cpp] 239\n");
            return true;
        }

        // For NumericPropertyIds check that index is less than JavascriptString length
        ScriptContext*scriptContext = GetScriptContext();
        uint32 index;
        if (scriptContext->IsNumericPropertyId(propertyId, &index))
        {LOGMEIN("JavascriptStringObject.cpp] 247\n");
            JavascriptString* str = JavascriptString::FromVar(CrossSite::MarshalVar(requestContext, this->InternalUnwrap()));
            return str->GetItemAt(index, value);
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL JavascriptStringObject::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 257\n");
        AssertMsg(!PropertyRecord::IsPropertyNameNumeric(propertyNameString->GetString(), propertyNameString->GetLength()),
            "Numeric property names should have been converted to uint or PropertyRecord*");

        BOOL result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && GetPropertyBuiltIns(propertyRecord->GetPropertyId(), value, requestContext, &result))
        {LOGMEIN("JavascriptStringObject.cpp] 266\n");
            return result;
        }

        return DynamicObject::GetProperty(originalInstance, propertyNameString, value, info, requestContext);
    }

    bool JavascriptStringObject::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext, BOOL* result)
    {LOGMEIN("JavascriptStringObject.cpp] 274\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptStringObject.cpp] 276\n");
            *value = JavascriptNumber::ToVar(this->InternalUnwrap()->GetLength(), requestContext);
            *result = true;
            return true;
        }

        return false;
    }

    BOOL JavascriptStringObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptStringObject.cpp] 286\n");
        bool result;
        if (SetPropertyBuiltIns(propertyId, flags, &result))
        {LOGMEIN("JavascriptStringObject.cpp] 289\n");
            return result;
        }

        return DynamicObject::SetProperty(propertyId, value, flags, info);
    }

    BOOL JavascriptStringObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("JavascriptStringObject.cpp] 297\n");
        bool result;
        PropertyRecord const* propertyRecord;
        this->GetScriptContext()->FindPropertyRecord(propertyNameString, &propertyRecord);

        if (propertyRecord != nullptr && SetPropertyBuiltIns(propertyRecord->GetPropertyId(), flags, &result))
        {LOGMEIN("JavascriptStringObject.cpp] 303\n");
            return result;
        }
        return DynamicObject::SetProperty(propertyNameString, value, flags, info);
    }

    bool JavascriptStringObject::SetPropertyBuiltIns(PropertyId propertyId, PropertyOperationFlags flags, bool* result)
    {LOGMEIN("JavascriptStringObject.cpp] 310\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptStringObject.cpp] 312\n");
            JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

            *result = false;
            return true;
        }

        return false;
    }

    BOOL JavascriptStringObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptStringObject.cpp] 323\n");
        if (propertyId == PropertyIds::length)
        {LOGMEIN("JavascriptStringObject.cpp] 325\n");
            JavascriptError::ThrowCantDeleteIfStrictMode(flags, this->GetScriptContext(), this->GetScriptContext()->GetPropertyName(propertyId)->GetBuffer());

            return FALSE;
        }
        return DynamicObject::DeleteProperty(propertyId, flags);
    }

    BOOL JavascriptStringObject::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("JavascriptStringObject.cpp] 334\n");
        JsUtil::CharacterBuffer<WCHAR> propertyName(propertyNameString->GetString(), propertyNameString->GetLength());
        if (BuiltInPropertyRecords::length.Equals(propertyName))
        {LOGMEIN("JavascriptStringObject.cpp] 337\n");
            JavascriptError::ThrowCantDeleteIfStrictMode(propertyOperationFlags, this->GetScriptContext(), propertyNameString->GetString());

            return FALSE;
        }
        return DynamicObject::DeleteProperty(propertyNameString, propertyOperationFlags);
    }

    BOOL JavascriptStringObject::HasItem(uint32 index)
    {LOGMEIN("JavascriptStringObject.cpp] 346\n");
        if (this->InternalUnwrap()->HasItem(index))
        {LOGMEIN("JavascriptStringObject.cpp] 348\n");
            return true;
        }
        return DynamicObject::HasItem(index);
    }

    BOOL JavascriptStringObject::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 355\n");
        JavascriptString* str = JavascriptString::FromVar(CrossSite::MarshalVar(requestContext, this->InternalUnwrap()));
        if (str->GetItemAt(index, value))
        {LOGMEIN("JavascriptStringObject.cpp] 358\n");
            return true;
        }
        return DynamicObject::GetItem(originalInstance, index, value, requestContext);
    }

    BOOL JavascriptStringObject::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 365\n");
        return this->GetItem(originalInstance, index, value, requestContext);
    }

    DescriptorFlags JavascriptStringObject::GetItemSetter(uint32 index, Var* setterValue, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 370\n");
        if (index < (uint32)this->InternalUnwrap()->GetLength())
        {LOGMEIN("JavascriptStringObject.cpp] 372\n");
            return DescriptorFlags::Data;
        }
        return DynamicObject::GetItemSetter(index, setterValue, requestContext);
    }

    BOOL JavascriptStringObject::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("JavascriptStringObject.cpp] 379\n");
        if (index < (uint32)this->InternalUnwrap()->GetLength())
        {LOGMEIN("JavascriptStringObject.cpp] 381\n");
            return false;
        }
        return DynamicObject::SetItem(index, value, flags);
    }

    BOOL JavascriptStringObject::GetEnumerator(JavascriptStaticEnumerator * enumerator, EnumeratorFlags flags, ScriptContext* requestContext, ForInCache * forInCache)
    {LOGMEIN("JavascriptStringObject.cpp] 388\n");
        return GetEnumeratorWithPrefix(
            RecyclerNew(GetScriptContext()->GetRecycler(), JavascriptStringEnumerator, this->Unwrap(), requestContext),
            enumerator, flags, requestContext, forInCache);
    }

    BOOL JavascriptStringObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 395\n");
        stringBuilder->Append(_u('"'));
        stringBuilder->Append(this->InternalUnwrap()->GetString(), this->InternalUnwrap()->GetLength());
        stringBuilder->Append(_u('"'));
        return TRUE;
    }

    BOOL JavascriptStringObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("JavascriptStringObject.cpp] 403\n");
        stringBuilder->AppendCppLiteral(_u("String"));
        return TRUE;
    }

#if ENABLE_TTD
    void JavascriptStringObject::SetValue_TTD(Js::Var val)
    {LOGMEIN("JavascriptStringObject.cpp] 410\n");
        AssertMsg(val == nullptr || Js::JavascriptString::Is(val), "Only legal values!");

        this->value = static_cast<Js::JavascriptString*>(val);
    }

    void JavascriptStringObject::MarkVisitKindSpecificPtrs(TTD::SnapshotExtractor* extractor)
    {LOGMEIN("JavascriptStringObject.cpp] 417\n");
        if(this->value != nullptr)
        {LOGMEIN("JavascriptStringObject.cpp] 419\n");
            extractor->MarkVisitVar(this->value);
        }
    }

    TTD::NSSnapObjects::SnapObjectType JavascriptStringObject::GetSnapTag_TTD() const
    {LOGMEIN("JavascriptStringObject.cpp] 425\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject;
    }

    void JavascriptStringObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("JavascriptStringObject.cpp] 430\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<TTD::TTDVar, TTD::NSSnapObjects::SnapObjectType::SnapBoxedValueObject>(objData, this->value);
    }
#endif
} // namespace Js
