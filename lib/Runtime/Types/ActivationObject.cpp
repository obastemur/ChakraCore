//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"
#include "cmperr.h"
#include "Language/JavascriptStackWalker.h"

namespace Js
{
    bool ActivationObject::Is(void* instance)
    {LOGMEIN("ActivationObject.cpp] 11\n");
        return VirtualTableInfo<Js::ActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::ActivationObjectEx>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::PseudoActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::BlockActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::ConsoleScopeActivationObject>::HasVirtualTable(instance);
    }

    BOOL ActivationObject::HasOwnPropertyCheckNoRedecl(PropertyId propertyId)
    {LOGMEIN("ActivationObject.cpp] 20\n");
        bool noRedecl = false;
        if (!GetTypeHandler()->HasProperty(this, propertyId, &noRedecl))
        {LOGMEIN("ActivationObject.cpp] 23\n");
            return FALSE;
        }
        else if (noRedecl)
        {LOGMEIN("ActivationObject.cpp] 27\n");
            JavascriptError::ThrowReferenceError(GetScriptContext(), ERRRedeclaration);
        }
        return TRUE;
    }

    BOOL ActivationObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("ActivationObject.cpp] 34\n");
        return DynamicObject::SetProperty(propertyId, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("ActivationObject.cpp] 39\n");
        return DynamicObject::SetProperty(propertyNameString, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::SetInternalProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("ActivationObject.cpp] 44\n");
        return DynamicObject::SetProperty(propertyId, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("ActivationObject.cpp] 49\n");
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, PropertyWritable|PropertyEnumerable, info, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue));
    }

    BOOL ActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {LOGMEIN("ActivationObject.cpp] 54\n");
        DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
        return true;
    }

    BOOL ActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {LOGMEIN("ActivationObject.cpp] 60\n");
        // Var binding of functions declared in eval are elided when conflicting
        // with function scope let/const variables, so do not actually set the
        // property if it exists and is a let/const variable.
        bool noRedecl = false;
        if (!GetTypeHandler()->HasProperty(this, propertyId, &noRedecl) || !noRedecl)
        {LOGMEIN("ActivationObject.cpp] 66\n");
            DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
        }
        return true;
    }

    BOOL ActivationObject::EnsureProperty(PropertyId propertyId)
    {LOGMEIN("ActivationObject.cpp] 73\n");
        if (!DynamicObject::HasOwnProperty(propertyId))
        {LOGMEIN("ActivationObject.cpp] 75\n");
            DynamicObject::SetPropertyWithAttributes(propertyId, this->GetLibrary()->GetUndefined(), PropertyDynamicTypeDefaults, nullptr, PropertyOperation_NonFixedValue);
        }
        return true;
    }

    BOOL ActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {LOGMEIN("ActivationObject.cpp] 82\n");
        ActivationObject::HasOwnPropertyCheckNoRedecl(propertyId);
        return true;
    }

    BOOL ActivationObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {LOGMEIN("ActivationObject.cpp] 88\n");
        return false;
    }

    BOOL ActivationObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ActivationObject.cpp] 93\n");
        stringBuilder->AppendCppLiteral(_u("{ActivationObject}"));
        return TRUE;
    }

    BOOL ActivationObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {LOGMEIN("ActivationObject.cpp] 99\n");
        stringBuilder->AppendCppLiteral(_u("Object, (ActivationObject)"));
        return TRUE;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ActivationObject::GetSnapTag_TTD() const
    {LOGMEIN("ActivationObject.cpp] 106\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapActivationObject;
    }

    void ActivationObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ActivationObject.cpp] 111\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapActivationObject>(objData, nullptr);
    }
#endif

    BOOL BlockActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {LOGMEIN("ActivationObject.cpp] 117\n");
        // eval, etc., should not create var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {LOGMEIN("ActivationObject.cpp] 123\n");
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::EnsureProperty(PropertyId propertyId)
    {LOGMEIN("ActivationObject.cpp] 129\n");
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {LOGMEIN("ActivationObject.cpp] 135\n");
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BlockActivationObject* BlockActivationObject::Clone(ScriptContext *scriptContext)
    {LOGMEIN("ActivationObject.cpp] 141\n");
        DynamicType* type = this->GetDynamicType();
        type->GetTypeHandler()->ClearSingletonInstance(); //We are going to share the type.

        BlockActivationObject* blockScopeClone = DynamicObject::NewObject<BlockActivationObject>(scriptContext->GetRecycler(), type);
        int slotCapacity = this->GetTypeHandler()->GetSlotCapacity();

        for (int i = 0; i < slotCapacity; i += 1)
        {LOGMEIN("ActivationObject.cpp] 149\n");
            DebugOnly(PropertyId propId = this->GetPropertyId(i));
            Var value = this->GetSlot(i);
            blockScopeClone->SetSlot(SetSlotArguments(propId, i, value));
        }

        return blockScopeClone;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType BlockActivationObject::GetSnapTag_TTD() const
    {LOGMEIN("ActivationObject.cpp] 160\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapBlockActivationObject;
    }

    void BlockActivationObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ActivationObject.cpp] 165\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapBlockActivationObject>(objData, nullptr);
    }
#endif

    BOOL PseudoActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {LOGMEIN("ActivationObject.cpp] 171\n");
        // eval, etc., should not create function properties on something like a "catch" scope
        return false;
    }

    BOOL PseudoActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {LOGMEIN("ActivationObject.cpp] 177\n");
        // eval, etc., should not create function properties on something like a "catch" scope
        return false;
    }

    BOOL PseudoActivationObject::EnsureProperty(PropertyId propertyId)
    {LOGMEIN("ActivationObject.cpp] 183\n");
        return false;
    }

    BOOL PseudoActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {LOGMEIN("ActivationObject.cpp] 188\n");
        return false;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType PseudoActivationObject::GetSnapTag_TTD() const
    {LOGMEIN("ActivationObject.cpp] 194\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapPseudoActivationObject;
    }

    void PseudoActivationObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ActivationObject.cpp] 199\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapPseudoActivationObject>(objData, nullptr);
    }
#endif

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ConsoleScopeActivationObject::GetSnapTag_TTD() const
    {LOGMEIN("ActivationObject.cpp] 206\n");
        return TTD::NSSnapObjects::SnapObjectType::SnapConsoleScopeActivationObject;
    }

    void ConsoleScopeActivationObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {LOGMEIN("ActivationObject.cpp] 211\n");
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapConsoleScopeActivationObject>(objData, nullptr);
    }

#endif

    /* static */
    const PropertyId * ActivationObjectEx::GetCachedScopeInfo(const PropertyIdArray *propIds)
    {LOGMEIN("ActivationObject.cpp] 219\n");
        // Cached scope info is appended to the "normal" prop ID array elements.
        return &propIds->elements[propIds->count];
    }

    BOOL ActivationObjectEx::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {LOGMEIN("ActivationObject.cpp] 225\n");
        // No need to invalidate the cached scope even if the property is a cached function object.
        // The caller won't be using the object itself.
        return __super::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    void ActivationObjectEx::GetPropertyCore(PropertyValueInfo *info, ScriptContext *requestContext)
    {LOGMEIN("ActivationObject.cpp] 232\n");
        if (info)
        {LOGMEIN("ActivationObject.cpp] 234\n");
            PropertyIndex slot = info->GetPropertyIndex();
            if (slot >= this->firstFuncSlot && slot <= this->lastFuncSlot)
            {LOGMEIN("ActivationObject.cpp] 237\n");
                this->parentFunc->InvalidateCachedScopeChain();

                // If the caller is an eval, then each time we execute the eval we need to invalidate the
                // cached scope chain. We can't rely on detecting the escape each time, because inline
                // cache hits may keep us from entering the runtime. So set a flag to make sure the
                // invalidation always happens.
                JavascriptFunction *currentFunc = nullptr;
                JavascriptStackWalker walker(requestContext);
                while (walker.GetCaller(&currentFunc))
                {LOGMEIN("ActivationObject.cpp] 247\n");
                    if (walker.IsEvalCaller())
                    {LOGMEIN("ActivationObject.cpp] 249\n");
                        //We are walking the stack, so the function body must have been deserialized by this point.
                        currentFunc->GetFunctionBody()->SetFuncEscapes(true);
                        break;
                    }
                }
            }
        }
    }

    BOOL ActivationObjectEx::GetProperty(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {LOGMEIN("ActivationObject.cpp] 260\n");
        if (__super::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {
            GetPropertyCore(info, requestContext);
            return TRUE;
        }
        return FALSE;
    }

    BOOL ActivationObjectEx::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {LOGMEIN("ActivationObject.cpp] 270\n");
        if (__super::GetProperty(originalInstance, propertyNameString, value, info, requestContext))
        {
            GetPropertyCore(info, requestContext);
            return TRUE;
        }
        return FALSE;
    }

    void ActivationObjectEx::InvalidateCachedScope()
    {LOGMEIN("ActivationObject.cpp] 280\n");
        if (this->cachedFuncCount != 0)
        {LOGMEIN("ActivationObject.cpp] 282\n");
            // Clearing the cached functions and types isn't strictly necessary for correctness,
            // but we want those objects to be collected even if the scope object is part of someone's
            // closure environment.
            memset(this->cache, 0, this->cachedFuncCount * sizeof(FuncCacheEntry));
        }
        this->parentFunc->SetCachedScope(nullptr);
    }

    void ActivationObjectEx::SetCachedFunc(uint i, ScriptFunction *func)
    {LOGMEIN("ActivationObject.cpp] 292\n");
        Assert(i < cachedFuncCount);
        cache[i].func = func;
        cache[i].type = (DynamicType*)func->GetType();
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ActivationObjectEx::GetSnapTag_TTD() const
    {LOGMEIN("ActivationObject.cpp] 300\n");
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void ActivationObjectEx::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Not implemented yet!!!");
    }
#endif
};
