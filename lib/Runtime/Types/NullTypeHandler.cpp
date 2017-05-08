//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

#include "Types/NullTypeHandler.h"
#include "Types/SimpleTypeHandler.h"

namespace Js
{
    int NullTypeHandlerBase::GetPropertyCount()
    {TRACE_IT(66414);
        return 0;
    }


    PropertyId NullTypeHandlerBase::GetPropertyId(ScriptContext* scriptContext, PropertyIndex index)
    {TRACE_IT(66415);
        return Constants::NoProperty;
    }

    PropertyId NullTypeHandlerBase::GetPropertyId(ScriptContext* scriptContext, BigPropertyIndex index)
    {TRACE_IT(66416);
        return Constants::NoProperty;
    }


    BOOL NullTypeHandlerBase::FindNextProperty(ScriptContext* scriptContext, PropertyIndex& index, JavascriptString** propertyString, PropertyId* propertyId,
        PropertyAttributes* attributes, Type* type, DynamicType *typeToEnumerate, EnumeratorFlags flags)
    {TRACE_IT(66417);
        Assert(propertyString);
        Assert(propertyId);
        Assert(type);
        return FALSE;
    }


    PropertyIndex NullTypeHandlerBase::GetPropertyIndex(PropertyRecord const* propertyRecord)
    {TRACE_IT(66418);
        return Constants::NoSlot;
    }

    bool NullTypeHandlerBase::GetPropertyEquivalenceInfo(PropertyRecord const* propertyRecord, PropertyEquivalenceInfo& info)
    {TRACE_IT(66419);
        info.slotIndex = Constants::NoSlot;
        info.isAuxSlot = false;
        info.isWritable = false;
        return false;
    }

    bool NullTypeHandlerBase::IsObjTypeSpecEquivalent(const Type* type, const TypeEquivalenceRecord& record, uint& failedPropertyIndex)
    {TRACE_IT(66420);
        uint propertyCount = record.propertyCount;
        EquivalentPropertyEntry* properties = record.properties;
        for (uint pi = 0; pi < propertyCount; pi++)
        {TRACE_IT(66421);
            const EquivalentPropertyEntry* refInfo = &properties[pi];
            if (!this->NullTypeHandlerBase::IsObjTypeSpecEquivalent(type, refInfo))
            {TRACE_IT(66422);
                failedPropertyIndex = pi;
                return false;
            }
        }
        return true;
    }

    bool NullTypeHandlerBase::IsObjTypeSpecEquivalent(const Type* type, const EquivalentPropertyEntry *entry)
    {TRACE_IT(66423);
        return entry->slotIndex == Constants::NoSlot && !entry->mustBeWritable;
    }

    BOOL NullTypeHandlerBase::HasProperty(DynamicObject* instance, PropertyId propertyId, __out_opt bool *noRedecl)
    {TRACE_IT(66424);
        // Check numeric propertyId only if objectArray is available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();

        if (noRedecl != nullptr)
        {TRACE_IT(66425);
            *noRedecl = false;
        }

        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(66426);
            return DynamicTypeHandler::HasItem(instance, indexVal);
        }

        return false;
    }


    BOOL NullTypeHandlerBase::HasProperty(DynamicObject* instance, JavascriptString* propertyNameString)
    {TRACE_IT(66427);
        PropertyRecord const* propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return NullTypeHandlerBase::HasProperty(instance, propertyRecord->GetPropertyId());
    }


    BOOL NullTypeHandlerBase::GetProperty(DynamicObject* instance, Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66428);
        // Check numeric propertyId only if objectArray is available
        uint32 indexVal;
        ScriptContext* scriptContext = instance->GetScriptContext();
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(66429);
            return DynamicTypeHandler::GetItem(instance, originalInstance, indexVal, value, requestContext);
        }

        *value = requestContext->GetMissingPropertyResult();
        return false;
    }


    BOOL NullTypeHandlerBase::GetProperty(DynamicObject* instance, Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(66430);
        PropertyRecord const* propertyRecord;
        instance->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return NullTypeHandlerBase::GetProperty(instance, originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }


    BOOL NullTypeHandlerBase::SetProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66431);
        return ConvertToSimpleType(instance)->SetProperty(instance, propertyId, value, flags, info);
    }


    BOOL NullTypeHandlerBase::SetProperty(DynamicObject* instance, JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(66432);
        return ConvertToSimpleType(instance)->SetProperty(instance, propertyNameString, value, flags, info);
    }

    BOOL NullTypeHandlerBase::AddProperty(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(66433);
        if (this->isPrototype && (ChangeTypeOnProto() || (GetIsShared() && IsolatePrototypes())))
        {TRACE_IT(66434);
            ScriptContext* scriptContext = instance->GetScriptContext();
            return ConvertToSimpleDictionaryType(instance)->AddProperty(instance, scriptContext->GetPropertyName(propertyId), value, attributes, info, flags, possibleSideEffects);
        }
        else
        {TRACE_IT(66435);
            return ConvertToSimpleType(instance)->AddProperty(instance, propertyId, value, attributes, info, flags, possibleSideEffects);
        }
    }

    BOOL NullTypeHandlerBase::DeleteProperty(DynamicObject* instance, PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(66436);
        // Check numeric propertyId only if objectArray is available
        ScriptContext* scriptContext = instance->GetScriptContext();
        uint32 indexVal;
        if (instance->HasObjectArray() && scriptContext->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(66437);
            return DynamicTypeHandler::DeleteItem(instance, indexVal, flags);
        }

        return true;
    }


    BOOL NullTypeHandlerBase::IsEnumerable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(66438);
        return true;
    }


    BOOL NullTypeHandlerBase::IsWritable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(66439);
        return true;
    }


    BOOL NullTypeHandlerBase::IsConfigurable(DynamicObject* instance, PropertyId propertyId)
    {TRACE_IT(66440);
        return true;
    }


    BOOL NullTypeHandlerBase::SetEnumerable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(66441);
        return false;
    }


    BOOL NullTypeHandlerBase::SetWritable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(66442);
        return false;
    }


    BOOL NullTypeHandlerBase::SetConfigurable(DynamicObject* instance, PropertyId propertyId, BOOL value)
    {TRACE_IT(66443);
        return false;
    }


    BOOL NullTypeHandlerBase::SetAccessors(DynamicObject* instance, PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(66444);
        return ConvertToDictionaryType(instance)->SetAccessors(instance, propertyId, getter, setter, flags);
    }


    BOOL NullTypeHandlerBase::PreventExtensions(DynamicObject* instance)
    {TRACE_IT(66445);
        return ConvertToDictionaryType(instance)->PreventExtensions(instance);
    }


    BOOL NullTypeHandlerBase::Seal(DynamicObject* instance)
    {TRACE_IT(66446);
        return ConvertToDictionaryType(instance)->Seal(instance);
    }


    BOOL NullTypeHandlerBase::FreezeImpl(DynamicObject* instance, bool isConvertedType)
    {TRACE_IT(66447);
        return ConvertToDictionaryType(instance)->Freeze(instance, true);
    }

    template <typename T>
    T* NullTypeHandlerBase::ConvertToTypeHandler(DynamicObject* instance)
    {TRACE_IT(66448);
        ScriptContext* scriptContext = instance->GetScriptContext();
        Recycler* recycler = scriptContext->GetRecycler();

        T * newTypeHandler = RecyclerNew(recycler, T, recycler);
        Assert((newTypeHandler->GetFlags() & IsPrototypeFlag) == 0);
        // EnsureSlots before updating the type handler and instance, as EnsureSlots allocates and may throw.
        instance->EnsureSlots(0, newTypeHandler->GetSlotCapacity(), scriptContext, newTypeHandler);
        Assert(((this->GetFlags() & IsPrototypeFlag) != 0) == this->isPrototype);
        newTypeHandler->SetFlags(IsPrototypeFlag, this->GetFlags());
        newTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection | PropertyTypesInlineSlotCapacityLocked, this->GetPropertyTypes());
        if (instance->HasReadOnlyPropertiesInvisibleToTypeHandler())
        {TRACE_IT(66449);
            newTypeHandler->ClearHasOnlyWritableDataProperties();
        }
        newTypeHandler->SetInstanceTypeHandler(instance);

        return newTypeHandler;
    }

    SimpleTypeHandler<1>* NullTypeHandlerBase::ConvertToSimpleType(DynamicObject* instance)
    {TRACE_IT(66450);
        SimpleTypeHandler<1>* newTypeHandler = ConvertToTypeHandler<SimpleTypeHandler<1>>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertNullToSimpleCount++;
#endif
        return newTypeHandler;
    }


    SimpleDictionaryTypeHandler * NullTypeHandlerBase::ConvertToSimpleDictionaryType(DynamicObject * instance)
    {TRACE_IT(66451);
        SimpleDictionaryTypeHandler* newTypeHandler = ConvertToTypeHandler<SimpleDictionaryTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertNullToSimpleDictionaryCount++;
#endif
        return newTypeHandler;
    }


    DictionaryTypeHandler * NullTypeHandlerBase::ConvertToDictionaryType(DynamicObject * instance)
    {TRACE_IT(66452);
        DictionaryTypeHandler* newTypeHandler = ConvertToTypeHandler<DictionaryTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertNullToDictionaryCount++;
#endif
        return newTypeHandler;
    }


    ES5ArrayTypeHandler* NullTypeHandlerBase::ConvertToES5ArrayType(DynamicObject * instance)
    {TRACE_IT(66453);
        ES5ArrayTypeHandler* newTypeHandler = ConvertToTypeHandler<ES5ArrayTypeHandler>(instance);

#ifdef PROFILE_TYPES
        instance->GetScriptContext()->convertNullToDictionaryCount++;
#endif
        return newTypeHandler;
    }


    BOOL NullTypeHandlerBase::SetPropertyWithAttributes(DynamicObject* instance, PropertyId propertyId, Var value, PropertyAttributes attributes, PropertyValueInfo* info, PropertyOperationFlags flags, SideEffects possibleSideEffects)
    {TRACE_IT(66454);
        // Always check numeric propertyId. May create objectArray.
        uint32 indexVal;
        if (instance->GetScriptContext()->IsNumericPropertyId(propertyId, &indexVal))
        {TRACE_IT(66455);
            return NullTypeHandlerBase::SetItemWithAttributes(instance, indexVal, value, attributes);
        }

        return this->AddProperty(instance, propertyId, value, attributes, info, flags, possibleSideEffects);
    }


    BOOL NullTypeHandlerBase::SetAttributes(DynamicObject* instance, PropertyId propertyId, PropertyAttributes attributes)
    {TRACE_IT(66456);
        return false;
    }


    BOOL NullTypeHandlerBase::GetAttributesWithPropertyIndex(DynamicObject * instance, PropertyId propertyId, BigPropertyIndex index, PropertyAttributes * attributes)
    {TRACE_IT(66457);
        return false;
    }


    DynamicTypeHandler* NullTypeHandlerBase::ConvertToTypeWithItemAttributes(DynamicObject* instance)
    {TRACE_IT(66458);
        return JavascriptArray::Is(instance) ?
            ConvertToES5ArrayType(instance) : ConvertToDictionaryType(instance);
    }

    void NullTypeHandlerBase::SetIsPrototype(DynamicObject* instance)
    {TRACE_IT(66459);
        if (!this->isPrototype)
        {TRACE_IT(66460);
            // We don't force a type transition even when ChangeTypeOnProto() == true, because objects with NullTypeHandlers don't
            // have any properties, so there is nothing to invalidate.  Types with NullTypeHandlers also aren't cached in typeWithoutProperty
            // caches, so there will be no fast property add path that could skip prototype cache invalidation.
            NullTypeHandler<true>* protoTypeHandler = NullTypeHandler<true>::GetDefaultInstance();
            AssertMsg(protoTypeHandler->GetFlags() == (GetFlags() | IsPrototypeFlag), "Why did we change the flags of a NullTypeHandler?");
            Assert(this->GetIsInlineSlotCapacityLocked() == protoTypeHandler->GetIsInlineSlotCapacityLocked());
            protoTypeHandler->SetPropertyTypes(PropertyTypesWritableDataOnly | PropertyTypesWritableDataOnlyDetection, GetPropertyTypes());
            SetInstanceTypeHandler(instance, protoTypeHandler);
        }
    }

    template<bool IsPrototypeTemplate>
    NullTypeHandler<IsPrototypeTemplate> NullTypeHandler<IsPrototypeTemplate>::defaultInstance;

    template<bool IsPrototypeTemplate>
    NullTypeHandler<IsPrototypeTemplate> * NullTypeHandler<IsPrototypeTemplate>::GetDefaultInstance() {TRACE_IT(66461); return &defaultInstance; }

    template class NullTypeHandler<false>;
    template class NullTypeHandler<true>;
}
