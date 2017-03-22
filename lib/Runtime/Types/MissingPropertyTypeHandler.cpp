//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    void MissingPropertyTypeHandler::SetUndefinedPropertySlot(DynamicObject* instance)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 9\n");
        Field(Var)* slots = reinterpret_cast<Field(Var)*>(reinterpret_cast<size_t>(instance) + sizeof(DynamicObject));
        slots[0] = instance->GetLibrary()->GetUndefined();
    }

    MissingPropertyTypeHandler::MissingPropertyTypeHandler() :
        DynamicTypeHandler(1, 1, (uint16)sizeof(DynamicObject)) {LOGMEIN("MissingPropertyTypeHandler.cpp] 15\n");}

    PropertyId MissingPropertyTypeHandler::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 18\n");
        return Constants::NoProperty;
    }

    PropertyId MissingPropertyTypeHandler::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 23\n");
        return Constants::NoProperty;
    }

    BOOL MissingPropertyTypeHandler::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 29\n");
        return FALSE;
    }

    PropertyIndex MissingPropertyTypeHandler::GetPropertyIndex(PropertyRecord const* propertyRecord)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 34\n");
        return 0;
    }

    bool MissingPropertyTypeHandler::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 39\n");
        info.slotIndex = Constants::NoSlot;
        info.isWritable = false;
        return false;
    }

    bool MissingPropertyTypeHandler::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 46\n");
        failedPropertyIndex = 0;
        return false;
    }

    bool MissingPropertyTypeHandler::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 52\n");
        return false;
    }

    BOOL MissingPropertyTypeHandler::HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *noRedecl)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 57\n");
        if (noRedecl != nullptr)
        {LOGMEIN("MissingPropertyTypeHandler.cpp] 59\n");
            *noRedecl = false;
        }

        return false;
    }


    BOOL MissingPropertyTypeHandler::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 68\n");
        return false;
    }

    BOOL MissingPropertyTypeHandler::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 73\n");
        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL MissingPropertyTypeHandler::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 79\n");
        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL MissingPropertyTypeHandler::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 85\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 90\n");
        Throw::FatalInternalError();
    }

    DescriptorFlags MissingPropertyTypeHandler::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 95\n");
        PropertyValueInfo::SetNoCache(info, instance);
        return None;
    }

    DescriptorFlags MissingPropertyTypeHandler::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 101\n");
        PropertyValueInfo::SetNoCache(info, instance);
        return None;
    }

    BOOL MissingPropertyTypeHandler::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 107\n");
        Throw::FatalInternalError();
    }


    BOOL MissingPropertyTypeHandler::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 113\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 118\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 123\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 128\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 133\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 138\n");
        Throw::FatalInternalError();
    }

    //
    // Set an attribute bit. Return true if change is made.
    //
    BOOL MissingPropertyTypeHandler::SetAttribute(DynamicObject* instance, int index, PropertyAttributes attribute)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 146\n");
        Throw::FatalInternalError();
    }

    //
    // Clear an attribute bit. Return true if change is made.
    //
    BOOL MissingPropertyTypeHandler::ClearAttribute(DynamicObject* instance, int index, PropertyAttributes attribute)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 154\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 159\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::PreventExtensions(DynamicObject* instance)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 164\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::Seal(DynamicObject* instance)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 169\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 174\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 179\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 184\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 189\n");
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::AddProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 194\n");
        Throw::FatalInternalError();
    }

    void MissingPropertyTypeHandler::SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 199\n");
        Throw::FatalInternalError();
    }

    void MissingPropertyTypeHandler::MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 204\n");
        Throw::FatalInternalError();
    }

    DynamicTypeHandler* MissingPropertyTypeHandler::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 209\n");
        Throw::FatalInternalError();
    }

    void MissingPropertyTypeHandler::SetIsPrototype(DynamicObject* instance)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 214\n");
        Throw::FatalInternalError();
    }

#if DBG
    bool MissingPropertyTypeHandler::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {LOGMEIN("MissingPropertyTypeHandler.cpp] 220\n");
        Throw::FatalInternalError();
    }
#endif
}
