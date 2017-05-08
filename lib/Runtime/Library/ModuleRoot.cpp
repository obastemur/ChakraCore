//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ModuleRoot::ModuleRoot(DynamicType * type):
        RootObjectBase(type)
    {TRACE_IT(62549);
    }

    void ModuleRoot::SetHostObject(ModuleID moduleID, HostObjectBase * hostObject)
    {TRACE_IT(62550);
        this->moduleID = moduleID;
        __super::SetHostObject(hostObject);
    }

    BOOL ModuleRoot::HasProperty(PropertyId propertyId)
    {TRACE_IT(62551);
        if (DynamicObject::HasProperty(propertyId))
        {TRACE_IT(62552);
            return TRUE;
        }
        else if (this->hostObject && JavascriptOperators::HasProperty(this->hostObject, propertyId))
        {TRACE_IT(62553);
            return TRUE;
        }
        return this->GetLibrary()->GetGlobalObject()->GlobalObject::HasProperty(propertyId);
    }

    BOOL ModuleRoot::EnsureProperty(PropertyId propertyId)
    {TRACE_IT(62554);
        if (!RootObjectBase::HasOwnPropertyCheckNoRedecl(propertyId))
        {TRACE_IT(62555);
            // Cannot pass the extra PropertyOperation_PreInit flag, because module root uses SetSlot directly from
            // SetRootProperty. If the property is not yet initialized SetSlot will (correctly) assert.
            this->InitProperty(propertyId, this->GetLibrary()->GetUndefined(), (PropertyOperationFlags)(PropertyOperation_SpecialValue | PropertyOperation_NonFixedValue));
        }
        return true;
    }

    BOOL ModuleRoot::HasRootProperty(PropertyId propertyId)
    {TRACE_IT(62556);
        if (__super::HasRootProperty(propertyId))
        {TRACE_IT(62557);
            return TRUE;
        }
        else if (this->hostObject && JavascriptOperators::HasProperty(this->hostObject, propertyId))
        {TRACE_IT(62558);
            return TRUE;
        }
        return this->GetLibrary()->GetGlobalObject()->GlobalObject::HasRootProperty(propertyId);
    }

    BOOL ModuleRoot::HasOwnProperty(PropertyId propertyId)
    {TRACE_IT(62559);
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL ModuleRoot::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62560);
        PropertyIndex index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {TRACE_IT(62561);
            *value = this->GetSlot(index);
            if (info) // Avoid testing IsWritable if info not being queried
            {TRACE_IT(62562);
                PropertyValueInfo::Set(info, this, index, IsWritable(propertyId) ? PropertyWritable : PropertyNone);
                if (this->IsFixedProperty(propertyId))
                {TRACE_IT(62563);
                    PropertyValueInfo::DisableStoreFieldCache(info);
                }
            }
            return TRUE;
        }
        if (this->hostObject && JavascriptOperators::GetProperty(this->hostObject, propertyId, value, requestContext))
        {TRACE_IT(62564);
            return TRUE;
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //

        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        return globalObj->GlobalObject::GetProperty(originalInstance, propertyId, value, NULL, requestContext);
    }

    BOOL ModuleRoot::GetRootProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62565);
        PropertyIndex index = GetRootPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {TRACE_IT(62566);
            *value = this->GetSlot(index);
            if (info) // Avoid testing IsWritable if info not being queried
            {TRACE_IT(62567);
                PropertyValueInfo::Set(info, this, index, IsWritable(propertyId) ? PropertyWritable : PropertyNone);
                if (this->IsFixedProperty(propertyId))
                {TRACE_IT(62568);
                    PropertyValueInfo::DisableStoreFieldCache(info);
                }
            }
            return TRUE;
        }
        if (this->hostObject && JavascriptOperators::GetProperty(this->hostObject, propertyId, value, requestContext))
        {TRACE_IT(62569);
            return TRUE;
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //

        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        return globalObj->GlobalObject::GetRootProperty(originalInstance, propertyId, value, NULL, requestContext);
    }

    BOOL ModuleRoot::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62570);
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return ModuleRoot::GetProperty(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }

    BOOL ModuleRoot::GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext)
    {TRACE_IT(62571);
        if (DynamicObject::GetAccessors(propertyId, getter, setter, requestContext))
        {TRACE_IT(62572);
            return TRUE;
        }
        if (this->hostObject)
        {TRACE_IT(62573);
            return this->hostObject->GetAccessors(propertyId, getter, setter, requestContext);
        }

        // Try checking the global object
        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        return globalObj->GlobalObject::GetAccessors(propertyId, getter, setter, requestContext);
    }

    BOOL ModuleRoot::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info,
        ScriptContext* requestContext)
    {TRACE_IT(62574);
        PropertyIndex index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {TRACE_IT(62575);
            *value = this->GetSlot(index);
            if (info) // Avoid testing IsWritable if info not being queried
            {TRACE_IT(62576);
                PropertyValueInfo::Set(info, this, index, IsWritable(propertyId) ? PropertyWritable : PropertyNone);
                if (this->IsFixedProperty(propertyId))
                {TRACE_IT(62577);
                    PropertyValueInfo::DisableStoreFieldCache(info);
                }
            }
            return TRUE;
        }
        if (this->hostObject && JavascriptOperators::GetPropertyReference(this->hostObject, propertyId, value, requestContext))
        {TRACE_IT(62578);
            return TRUE;
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //

        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        return globalObj->GlobalObject::GetPropertyReference(originalInstance, propertyId, value, NULL, requestContext);
    }

    BOOL ModuleRoot::GetRootPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info,
        ScriptContext* requestContext)
    {TRACE_IT(62579);
        PropertyIndex index = GetRootPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {TRACE_IT(62580);
            *value = this->GetSlot(index);
            if (info) // Avoid testing IsWritable if info not being queried
            {TRACE_IT(62581);
                PropertyValueInfo::Set(info, this, index, IsWritable(propertyId) ? PropertyWritable : PropertyNone);
                if (this->IsFixedProperty(propertyId))
                {TRACE_IT(62582);
                    PropertyValueInfo::DisableStoreFieldCache(info);
                }
            }
            return TRUE;
        }
        if (this->hostObject && JavascriptOperators::GetPropertyReference(this->hostObject, propertyId, value, requestContext))
        {TRACE_IT(62583);
            return TRUE;
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //

        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        return globalObj->GlobalObject::GetRootPropertyReference(originalInstance, propertyId, value, NULL, requestContext);
    }

    BOOL ModuleRoot::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(62584);
        PropertyIndex index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {TRACE_IT(62585);
            if (this->IsWritable(propertyId) == FALSE)
            {TRACE_IT(62586);
                JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

                if (!this->IsFixedProperty(propertyId))
                {TRACE_IT(62587);
                    PropertyValueInfo::Set(info, this, index, PropertyNone); // Try to cache property info even if not writable
                }
                else
                {TRACE_IT(62588);
                    PropertyValueInfo::SetNoCache(info, this);
                }
                return FALSE;
            }
            this->SetSlot(SetSlotArguments(propertyId, index, value));
            if (!this->IsFixedProperty(propertyId))
            {TRACE_IT(62589);
                PropertyValueInfo::Set(info, this, index);
            }
            else
            {TRACE_IT(62590);
                PropertyValueInfo::SetNoCache(info, this);
            }
            return TRUE;
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {TRACE_IT(62591);
            return this->hostObject->SetProperty(propertyId, value, flags, NULL);
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //
        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        BOOL setAttempted = TRUE;
        if (globalObj->SetExistingProperty(propertyId, value, NULL, &setAttempted))
        {TRACE_IT(62592);
            return TRUE;
        }

        //
        // Set was attempted. But the set operation returned false.
        // This happens, when the property is read only.
        // In those scenarios, we should be setting the property with default attributes
        //
        if (setAttempted)
        {TRACE_IT(62593);
            return FALSE;
        }

        return DynamicObject::SetProperty(propertyId, value, flags, info);
    }

    BOOL ModuleRoot::SetRootProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(62594);
        PropertyIndex index = GetRootPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {TRACE_IT(62595);
            if (this->IsWritable(propertyId) == FALSE)
            {TRACE_IT(62596);
                JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

                if (!this->IsFixedProperty(propertyId))
                {TRACE_IT(62597);
                    PropertyValueInfo::Set(info, this, index, PropertyNone); // Try to cache property info even if not writable
                }
                else
                {TRACE_IT(62598);
                    PropertyValueInfo::SetNoCache(info, this);
                }
                return FALSE;
            }
            this->SetSlot(SetSlotArgumentsRoot(propertyId, true, index, value));
            if (!this->IsFixedProperty(propertyId))
            {TRACE_IT(62599);
                PropertyValueInfo::Set(info, this, index);
            }
            else
            {TRACE_IT(62600);
                PropertyValueInfo::SetNoCache(info, this);
            }
            return TRUE;
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {TRACE_IT(62601);
            return this->hostObject->SetProperty(propertyId, value, flags, NULL);
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //
        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        BOOL setAttempted = TRUE;
        if (globalObj->SetExistingRootProperty(propertyId, value, NULL, &setAttempted))
        {TRACE_IT(62602);
            return TRUE;
        }

        //
        // Set was attempted. But the set operation returned false.
        // This happens, when the property is read only.
        // In those scenarios, we should be setting the property with default attributes
        //
        if (setAttempted)
        {TRACE_IT(62603);
            return FALSE;
        }

        return __super::SetRootProperty(propertyId, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ModuleRoot::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(62604);
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return ModuleRoot::SetProperty(propertyRecord->GetPropertyId(), value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ModuleRoot::InitPropertyScoped(PropertyId propertyId, Var value)
    {TRACE_IT(62605);
        return DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
    }

    BOOL ModuleRoot::InitFuncScoped(PropertyId propertyId, Var value)
    {TRACE_IT(62606);
        // Var binding of functions declared in eval are elided when conflicting
        // with global scope let/const variables, so do not actually set the
        // property if it exists and is a let/const variable.
        bool noRedecl = false;
        if (!GetTypeHandler()->HasRootProperty(this, propertyId, &noRedecl) || !noRedecl)
        {TRACE_IT(62607);
            DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
        }
        return true;
    }

    BOOL ModuleRoot::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {TRACE_IT(62608);
        if (DynamicObject::SetAccessors(propertyId, getter, setter, flags))
        {TRACE_IT(62609);
            return TRUE;
        }
        if (this->hostObject)
        {TRACE_IT(62610);
            return this->hostObject->SetAccessors(propertyId, getter, setter, flags);
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //
        GlobalObject* globalObj = GetScriptContext()->GetGlobalObject();
        return globalObj->GlobalObject::SetAccessors(propertyId, getter, setter, flags);
    }

    BOOL ModuleRoot::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(62611);
        int index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {TRACE_IT(62612);
            return FALSE;
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {TRACE_IT(62613);
            return this->hostObject->DeleteProperty(propertyId, flags);
        }
        return this->GetLibrary()->GetGlobalObject()->GlobalObject::DeleteProperty(propertyId, flags);
    }

    BOOL ModuleRoot::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {TRACE_IT(62614);
        PropertyRecord const *propertyRecord = nullptr;
        if (JavascriptOperators::ShouldTryDeleteProperty(this, propertyNameString, &propertyRecord))
        {TRACE_IT(62615);
            Assert(propertyRecord);
            return DeleteProperty(propertyRecord->GetPropertyId(), flags);
        }

        return TRUE;
    }

    BOOL ModuleRoot::DeleteRootProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(62616);
        int index = GetRootPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {TRACE_IT(62617);
            return FALSE;
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {TRACE_IT(62618);
            return this->hostObject->DeleteProperty(propertyId, flags);
        }
        return this->GetLibrary()->GetGlobalObject()->GlobalObject::DeleteRootProperty(propertyId, flags);
    }

    BOOL ModuleRoot::HasItem(uint32 index)
    {TRACE_IT(62619);
        return DynamicObject::HasItem(index)
            || (this->hostObject && JavascriptOperators::HasItem(this->hostObject, index));
    }

    BOOL ModuleRoot::HasOwnItem(uint32 index)
    {TRACE_IT(62620);
        return DynamicObject::HasItem(index);
    }

    BOOL ModuleRoot::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(62621);
        if (DynamicObject::GetItemReference(originalInstance, index, value, requestContext))
        {TRACE_IT(62622);
            return TRUE;
        }
        if (this->hostObject && this->hostObject->GetItemReference(originalInstance, index, value, requestContext))
        {TRACE_IT(62623);
            return TRUE;
        }
        *value = requestContext->GetMissingItemResult();
        return FALSE;
    }

    BOOL ModuleRoot::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {TRACE_IT(62624);
        if (DynamicObject::SetItem(index, value, flags))
        {TRACE_IT(62625);
            return TRUE;
        }

        if (this->hostObject)
        {TRACE_IT(62626);
            return this->hostObject->SetItem(index, value, flags);
        }
        return FALSE;
    }

    BOOL ModuleRoot::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {TRACE_IT(62627);
        if (DynamicObject::GetItem(originalInstance, index, value, requestContext))
        {TRACE_IT(62628);
            return TRUE;
        }
        if (this->hostObject && this->hostObject->GetItem(originalInstance, index, value, requestContext))
        {TRACE_IT(62629);
            return TRUE;
        }
        *value = requestContext->GetMissingItemResult();
        return FALSE;
    }

    BOOL ModuleRoot::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(62630);
        stringBuilder->AppendCppLiteral(_u("{Named Item}"));
        return TRUE;
    }

    BOOL ModuleRoot::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(62631);
        stringBuilder->AppendCppLiteral(_u("Object, (Named Item)"));
        return TRUE;
    }
}
