//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    void MissingPropertyTypeHandler::SetUndefinedPropertySlot(DynamicObject* instance)
    {TRACE_IT(66373);
        Field(Var)* slots = reinterpret_cast<Field(Var)*>(reinterpret_cast<size_t>(instance) + sizeof(DynamicObject));
        slots[0] = instance->GetLibrary()->GetUndefined();
    }

    MissingPropertyTypeHandler::MissingPropertyTypeHandler() :
        DynamicTypeHandler(1, 1, (uint16)sizeof(DynamicObject)) {TRACE_IT(66374);}

    PropertyId MissingPropertyTypeHandler::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {TRACE_IT(66375);
        return Constants::NoProperty;
    }

    PropertyId MissingPropertyTypeHandler::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {TRACE_IT(66376);
        return Constants::NoProperty;
    }

    BOOL MissingPropertyTypeHandler::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyStringName,
        PropertyId* propertyId, PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {TRACE_IT(66377);
        return FALSE;
    }

    PropertyIndex MissingPropertyTypeHandler::GetPropertyIndex(PropertyRecord const* propertyRecord)
    {TRACE_IT(66378);
        return 0;
    }

    bool MissingPropertyTypeHandler::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {TRACE_IT(66379);
        info.slotIndex = Constants::NoSlot;
        info.isWritable = false;
        return false;
    }

    bool MissingPropertyTypeHandler::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {TRACE_IT(66380);
        failedPropertyIndex = 0;
        return false;
    }

    bool MissingPropertyTypeHandler::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {TRACE_IT(66381);
        return false;
    }

    BOOL MissingPropertyTypeHandler::HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *noRedecl)
    {TRACE_IT(66382);
        if (noRedecl != nullptr)
        {TRACE_IT(66383);
            *noRedecl = false;
        }

        return false;
    }


    BOOL MissingPropertyTypeHandler::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {TRACE_IT(66384);
        return false;
    }

    BOOL MissingPropertyTypeHandler::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66385);
        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL MissingPropertyTypeHandler::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66386);
        *value = requestContext->GetMissingPropertyResult();
        return false;
    }

    BOOL MissingPropertyTypeHandler::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66387);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66388);
        Throw::FatalInternalError();
    }

    DescriptorFlags MissingPropertyTypeHandler::GetSetter(DynamicObject* instance, PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66389);
        PropertyValueInfo::SetNoCache(info, instance);
        return None;
    }

    DescriptorFlags MissingPropertyTypeHandler::GetSetter(DynamicObject* instance, JavascriptString* propertyNameString, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66390);
        PropertyValueInfo::SetNoCache(info, instance);
        return None;
    }

    BOOL MissingPropertyTypeHandler::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags propertyOperationFlags)
    {TRACE_IT(66391);
        Throw::FatalInternalError();
    }


    BOOL MissingPropertyTypeHandler::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(66392);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(66393);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(66394);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(66395);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(66396);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(66397);
        Throw::FatalInternalError();
    }

    //
    // Set an attribute bit. Return true if change is made.
    //
    BOOL MissingPropertyTypeHandler::SetAttribute(DynamicObject* instance, int index, PropertyAttributes attribute)
    {TRACE_IT(66398);
        Throw::FatalInternalError();
    }

    //
    // Clear an attribute bit. Return true if change is made.
    //
    BOOL MissingPropertyTypeHandler::ClearAttribute(DynamicObject* instance, int index, PropertyAttributes attribute)
    {TRACE_IT(66399);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(66400);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::PreventExtensions(DynamicObject* instance)
    {TRACE_IT(66401);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::Seal(DynamicObject* instance)
    {TRACE_IT(66402);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {TRACE_IT(66403);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(66404);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {TRACE_IT(66405);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {TRACE_IT(66406);
        Throw::FatalInternalError();
    }

    BOOL MissingPropertyTypeHandler::AddProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(66407);
        Throw::FatalInternalError();
    }

    void MissingPropertyTypeHandler::SetAllPropertiesToUndefined(DynamicObject* instance, bool invalidateFixedFields)
    {TRACE_IT(66408);
        Throw::FatalInternalError();
    }

    void MissingPropertyTypeHandler::MarshalAllPropertiesToScriptContext(DynamicObject* instance, ScriptContext* targetScriptContext, bool invalidateFixedFields)
    {TRACE_IT(66409);
        Throw::FatalInternalError();
    }

    DynamicTypeHandler* MissingPropertyTypeHandler::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {TRACE_IT(66410);
        Throw::FatalInternalError();
    }

    void MissingPropertyTypeHandler::SetIsPrototype(DynamicObject* instance)
    {TRACE_IT(66411);
        Throw::FatalInternalError();
    }

#if DBG
    bool MissingPropertyTypeHandler::CanStorePropertyValueDirectly(const DynamicObject* instance, PropertyId propertyId, bool allowLetConst)
    {TRACE_IT(66412);
        Throw::FatalInternalError();
    }
#endif
}
