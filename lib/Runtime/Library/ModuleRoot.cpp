//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ModuleRoot::ModuleRoot(DynamicType * type):
        RootObjectBase(type)
    {LOGMEIN("ModuleRoot.cpp] 10\n");
    }

    void ModuleRoot::SetHostObject(ModuleID moduleID, HostObjectBase * hostObject)
    {LOGMEIN("ModuleRoot.cpp] 14\n");
        this->moduleID = moduleID;
        __super::SetHostObject(hostObject);
    }

    BOOL ModuleRoot::HasProperty(PropertyId propertyId)
    {LOGMEIN("ModuleRoot.cpp] 20\n");
        if (DynamicObject::HasProperty(propertyId))
        {LOGMEIN("ModuleRoot.cpp] 22\n");
            return TRUE;
        }
        else if (this->hostObject && JavascriptOperators::HasProperty(this->hostObject, propertyId))
        {LOGMEIN("ModuleRoot.cpp] 26\n");
            return TRUE;
        }
        return this->GetLibrary()->GetGlobalObject()->GlobalObject::HasProperty(propertyId);
    }

    BOOL ModuleRoot::EnsureProperty(PropertyId propertyId)
    {LOGMEIN("ModuleRoot.cpp] 33\n");
        if (!RootObjectBase::HasOwnPropertyCheckNoRedecl(propertyId))
        {LOGMEIN("ModuleRoot.cpp] 35\n");
            // Cannot pass the extra PropertyOperation_PreInit flag, because module root uses SetSlot directly from
            // SetRootProperty. If the property is not yet initialized SetSlot will (correctly) assert.
            this->InitProperty(propertyId, this->GetLibrary()->GetUndefined(), (PropertyOperationFlags)(PropertyOperation_SpecialValue | PropertyOperation_NonFixedValue));
        }
        return true;
    }

    BOOL ModuleRoot::HasRootProperty(PropertyId propertyId)
    {LOGMEIN("ModuleRoot.cpp] 44\n");
        if (__super::HasRootProperty(propertyId))
        {LOGMEIN("ModuleRoot.cpp] 46\n");
            return TRUE;
        }
        else if (this->hostObject && JavascriptOperators::HasProperty(this->hostObject, propertyId))
        {LOGMEIN("ModuleRoot.cpp] 50\n");
            return TRUE;
        }
        return this->GetLibrary()->GetGlobalObject()->GlobalObject::HasRootProperty(propertyId);
    }

    BOOL ModuleRoot::HasOwnProperty(PropertyId propertyId)
    {LOGMEIN("ModuleRoot.cpp] 57\n");
        return DynamicObject::HasProperty(propertyId);
    }

    BOOL ModuleRoot::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("ModuleRoot.cpp] 62\n");
        PropertyIndex index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {LOGMEIN("ModuleRoot.cpp] 65\n");
            *value = this->GetSlot(index);
            if (info) // Avoid testing IsWritable if info not being queried
            {LOGMEIN("ModuleRoot.cpp] 68\n");
                PropertyValueInfo::Set(info, this, index, IsWritable(propertyId) ? PropertyWritable : PropertyNone);
                if (this->IsFixedProperty(propertyId))
                {LOGMEIN("ModuleRoot.cpp] 71\n");
                    PropertyValueInfo::DisableStoreFieldCache(info);
                }
            }
            return TRUE;
        }
        if (this->hostObject && JavascriptOperators::GetProperty(this->hostObject, propertyId, value, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 78\n");
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
    {LOGMEIN("ModuleRoot.cpp] 92\n");
        PropertyIndex index = GetRootPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {LOGMEIN("ModuleRoot.cpp] 95\n");
            *value = this->GetSlot(index);
            if (info) // Avoid testing IsWritable if info not being queried
            {LOGMEIN("ModuleRoot.cpp] 98\n");
                PropertyValueInfo::Set(info, this, index, IsWritable(propertyId) ? PropertyWritable : PropertyNone);
                if (this->IsFixedProperty(propertyId))
                {LOGMEIN("ModuleRoot.cpp] 101\n");
                    PropertyValueInfo::DisableStoreFieldCache(info);
                }
            }
            return TRUE;
        }
        if (this->hostObject && JavascriptOperators::GetProperty(this->hostObject, propertyId, value, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 108\n");
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
    {LOGMEIN("ModuleRoot.cpp] 122\n");
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return ModuleRoot::GetProperty(originalInstance, propertyRecord->GetPropertyId(), value, info, requestContext);
    }

    BOOL ModuleRoot::GetAccessors(PropertyId propertyId, Var* getter, Var* setter, ScriptContext * requestContext)
    {LOGMEIN("ModuleRoot.cpp] 129\n");
        if (DynamicObject::GetAccessors(propertyId, getter, setter, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 131\n");
            return TRUE;
        }
        if (this->hostObject)
        {LOGMEIN("ModuleRoot.cpp] 135\n");
            return this->hostObject->GetAccessors(propertyId, getter, setter, requestContext);
        }

        // Try checking the global object
        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        return globalObj->GlobalObject::GetAccessors(propertyId, getter, setter, requestContext);
    }

    BOOL ModuleRoot::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info,
        ScriptContext* requestContext)
    {LOGMEIN("ModuleRoot.cpp] 146\n");
        PropertyIndex index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {LOGMEIN("ModuleRoot.cpp] 149\n");
            *value = this->GetSlot(index);
            if (info) // Avoid testing IsWritable if info not being queried
            {LOGMEIN("ModuleRoot.cpp] 152\n");
                PropertyValueInfo::Set(info, this, index, IsWritable(propertyId) ? PropertyWritable : PropertyNone);
                if (this->IsFixedProperty(propertyId))
                {LOGMEIN("ModuleRoot.cpp] 155\n");
                    PropertyValueInfo::DisableStoreFieldCache(info);
                }
            }
            return TRUE;
        }
        if (this->hostObject && JavascriptOperators::GetPropertyReference(this->hostObject, propertyId, value, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 162\n");
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
    {LOGMEIN("ModuleRoot.cpp] 177\n");
        PropertyIndex index = GetRootPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {LOGMEIN("ModuleRoot.cpp] 180\n");
            *value = this->GetSlot(index);
            if (info) // Avoid testing IsWritable if info not being queried
            {LOGMEIN("ModuleRoot.cpp] 183\n");
                PropertyValueInfo::Set(info, this, index, IsWritable(propertyId) ? PropertyWritable : PropertyNone);
                if (this->IsFixedProperty(propertyId))
                {LOGMEIN("ModuleRoot.cpp] 186\n");
                    PropertyValueInfo::DisableStoreFieldCache(info);
                }
            }
            return TRUE;
        }
        if (this->hostObject && JavascriptOperators::GetPropertyReference(this->hostObject, propertyId, value, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 193\n");
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
    {LOGMEIN("ModuleRoot.cpp] 207\n");
        PropertyIndex index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {LOGMEIN("ModuleRoot.cpp] 210\n");
            if (this->IsWritable(propertyId) == FALSE)
            {LOGMEIN("ModuleRoot.cpp] 212\n");
                JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

                if (!this->IsFixedProperty(propertyId))
                {LOGMEIN("ModuleRoot.cpp] 216\n");
                    PropertyValueInfo::Set(info, this, index, PropertyNone); // Try to cache property info even if not writable
                }
                else
                {
                    PropertyValueInfo::SetNoCache(info, this);
                }
                return FALSE;
            }
            this->SetSlot(SetSlotArguments(propertyId, index, value));
            if (!this->IsFixedProperty(propertyId))
            {LOGMEIN("ModuleRoot.cpp] 227\n");
                PropertyValueInfo::Set(info, this, index);
            }
            else
            {
                PropertyValueInfo::SetNoCache(info, this);
            }
            return TRUE;
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {LOGMEIN("ModuleRoot.cpp] 237\n");
            return this->hostObject->SetProperty(propertyId, value, flags, NULL);
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //
        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        BOOL setAttempted = TRUE;
        if (globalObj->SetExistingProperty(propertyId, value, NULL, &setAttempted))
        {LOGMEIN("ModuleRoot.cpp] 248\n");
            return TRUE;
        }

        //
        // Set was attempted. But the set operation returned false.
        // This happens, when the property is read only.
        // In those scenarios, we should be setting the property with default attributes
        //
        if (setAttempted)
        {LOGMEIN("ModuleRoot.cpp] 258\n");
            return FALSE;
        }

        return DynamicObject::SetProperty(propertyId, value, flags, info);
    }

    BOOL ModuleRoot::SetRootProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("ModuleRoot.cpp] 266\n");
        PropertyIndex index = GetRootPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {LOGMEIN("ModuleRoot.cpp] 269\n");
            if (this->IsWritable(propertyId) == FALSE)
            {LOGMEIN("ModuleRoot.cpp] 271\n");
                JavascriptError::ThrowCantAssignIfStrictMode(flags, this->GetScriptContext());

                if (!this->IsFixedProperty(propertyId))
                {LOGMEIN("ModuleRoot.cpp] 275\n");
                    PropertyValueInfo::Set(info, this, index, PropertyNone); // Try to cache property info even if not writable
                }
                else
                {
                    PropertyValueInfo::SetNoCache(info, this);
                }
                return FALSE;
            }
            this->SetSlot(SetSlotArgumentsRoot(propertyId, true, index, value));
            if (!this->IsFixedProperty(propertyId))
            {LOGMEIN("ModuleRoot.cpp] 286\n");
                PropertyValueInfo::Set(info, this, index);
            }
            else
            {
                PropertyValueInfo::SetNoCache(info, this);
            }
            return TRUE;
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {LOGMEIN("ModuleRoot.cpp] 296\n");
            return this->hostObject->SetProperty(propertyId, value, flags, NULL);
        }

        //
        // Try checking the global object
        // if the module root doesn't have the property and the host object also doesn't have it
        //
        GlobalObject* globalObj = this->GetLibrary()->GetGlobalObject();
        BOOL setAttempted = TRUE;
        if (globalObj->SetExistingRootProperty(propertyId, value, NULL, &setAttempted))
        {LOGMEIN("ModuleRoot.cpp] 307\n");
            return TRUE;
        }

        //
        // Set was attempted. But the set operation returned false.
        // This happens, when the property is read only.
        // In those scenarios, we should be setting the property with default attributes
        //
        if (setAttempted)
        {LOGMEIN("ModuleRoot.cpp] 317\n");
            return FALSE;
        }

        return __super::SetRootProperty(propertyId, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ModuleRoot::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("ModuleRoot.cpp] 325\n");
        PropertyRecord const * propertyRecord;
        this->GetScriptContext()->GetOrAddPropertyRecord(propertyNameString->GetString(), propertyNameString->GetLength(), &propertyRecord);
        return ModuleRoot::SetProperty(propertyRecord->GetPropertyId(), value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ModuleRoot::InitPropertyScoped(PropertyId propertyId, Var value)
    {LOGMEIN("ModuleRoot.cpp] 332\n");
        return DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
    }

    BOOL ModuleRoot::InitFuncScoped(PropertyId propertyId, Var value)
    {LOGMEIN("ModuleRoot.cpp] 337\n");
        // Var binding of functions declared in eval are elided when conflicting
        // with global scope let/const variables, so do not actually set the
        // property if it exists and is a let/const variable.
        bool noRedecl = false;
        if (!GetTypeHandler()->HasRootProperty(this, propertyId, &noRedecl) || !noRedecl)
        {LOGMEIN("ModuleRoot.cpp] 343\n");
            DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
        }
        return true;
    }

    BOOL ModuleRoot::SetAccessors(PropertyId propertyId, Var getter, Var setter, PropertyOperationFlags flags)
    {LOGMEIN("ModuleRoot.cpp] 350\n");
        if (DynamicObject::SetAccessors(propertyId, getter, setter, flags))
        {LOGMEIN("ModuleRoot.cpp] 352\n");
            return TRUE;
        }
        if (this->hostObject)
        {LOGMEIN("ModuleRoot.cpp] 356\n");
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
    {LOGMEIN("ModuleRoot.cpp] 369\n");
        int index = GetPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {LOGMEIN("ModuleRoot.cpp] 372\n");
            return FALSE;
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {LOGMEIN("ModuleRoot.cpp] 376\n");
            return this->hostObject->DeleteProperty(propertyId, flags);
        }
        return this->GetLibrary()->GetGlobalObject()->GlobalObject::DeleteProperty(propertyId, flags);
    }

    BOOL ModuleRoot::DeleteProperty(JavascriptString *propertyNameString, PropertyOperationFlags flags)
    {LOGMEIN("ModuleRoot.cpp] 383\n");
        PropertyRecord const *propertyRecord = nullptr;
        if (JavascriptOperators::ShouldTryDeleteProperty(this, propertyNameString, &propertyRecord))
        {LOGMEIN("ModuleRoot.cpp] 386\n");
            Assert(propertyRecord);
            return DeleteProperty(propertyRecord->GetPropertyId(), flags);
        }

        return TRUE;
    }

    BOOL ModuleRoot::DeleteRootProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("ModuleRoot.cpp] 395\n");
        int index = GetRootPropertyIndex(propertyId);
        if (index != Constants::NoSlot)
        {LOGMEIN("ModuleRoot.cpp] 398\n");
            return FALSE;
        }
        else if (this->hostObject && this->hostObject->HasProperty(propertyId))
        {LOGMEIN("ModuleRoot.cpp] 402\n");
            return this->hostObject->DeleteProperty(propertyId, flags);
        }
        return this->GetLibrary()->GetGlobalObject()->GlobalObject::DeleteRootProperty(propertyId, flags);
    }

    BOOL ModuleRoot::HasItem(uint32 index)
    {LOGMEIN("ModuleRoot.cpp] 409\n");
        return DynamicObject::HasItem(index)
            || (this->hostObject && JavascriptOperators::HasItem(this->hostObject, index));
    }

    BOOL ModuleRoot::HasOwnItem(uint32 index)
    {LOGMEIN("ModuleRoot.cpp] 415\n");
        return DynamicObject::HasItem(index);
    }

    BOOL ModuleRoot::GetItemReference(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {LOGMEIN("ModuleRoot.cpp] 420\n");
        if (DynamicObject::GetItemReference(originalInstance, index, value, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 422\n");
            return TRUE;
        }
        if (this->hostObject && this->hostObject->GetItemReference(originalInstance, index, value, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 426\n");
            return TRUE;
        }
        *value = requestContext->GetMissingItemResult();
        return FALSE;
    }

    BOOL ModuleRoot::SetItem(uint32 index, Var value, PropertyOperationFlags flags)
    {LOGMEIN("ModuleRoot.cpp] 434\n");
        if (DynamicObject::SetItem(index, value, flags))
        {LOGMEIN("ModuleRoot.cpp] 436\n");
            return TRUE;
        }

        if (this->hostObject)
        {LOGMEIN("ModuleRoot.cpp] 441\n");
            return this->hostObject->SetItem(index, value, flags);
        }
        return FALSE;
    }

    BOOL ModuleRoot::GetItem(Var originalInstance, uint32 index, Var* value, ScriptContext * requestContext)
    {LOGMEIN("ModuleRoot.cpp] 448\n");
        if (DynamicObject::GetItem(originalInstance, index, value, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 450\n");
            return TRUE;
        }
        if (this->hostObject && this->hostObject->GetItem(originalInstance, index, value, requestContext))
        {LOGMEIN("ModuleRoot.cpp] 454\n");
            return TRUE;
        }
        *value = requestContext->GetMissingItemResult();
        return FALSE;
    }

    BOOL ModuleRoot::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ModuleRoot.cpp] 462\n");
        stringBuilder->AppendCppLiteral(_u("{Named Item}"));
        return TRUE;
    }

    BOOL ModuleRoot::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ModuleRoot.cpp] 468\n");
        stringBuilder->AppendCppLiteral(_u("Object, (Named Item)"));
        return TRUE;
    }
}
