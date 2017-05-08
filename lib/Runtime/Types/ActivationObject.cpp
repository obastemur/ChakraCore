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
    {TRACE_IT(65231);
        return VirtualTableInfo<Js::ActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::ActivationObjectEx>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::PseudoActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::BlockActivationObject>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::ConsoleScopeActivationObject>::HasVirtualTable(instance);
    }

    BOOL ActivationObject::HasOwnPropertyCheckNoRedecl(PropertyId propertyId)
    {TRACE_IT(65232);
        bool noRedecl = false;
        if (!GetTypeHandler()->HasProperty(this, propertyId, &noRedecl))
        {TRACE_IT(65233);
            return FALSE;
        }
        else if (noRedecl)
        {TRACE_IT(65234);
            JavascriptError::ThrowReferenceError(GetScriptContext(), ERRRedeclaration);
        }
        return TRUE;
    }

    BOOL ActivationObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65235);
        return DynamicObject::SetProperty(propertyId, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::SetProperty(JavascriptString* propertyNameString, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65236);
        return DynamicObject::SetProperty(propertyNameString, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::SetInternalProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65237);
        return DynamicObject::SetProperty(propertyId, value, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue), info);
    }

    BOOL ActivationObject::InitProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(65238);
        return DynamicObject::SetPropertyWithAttributes(propertyId, value, PropertyWritable|PropertyEnumerable, info, (PropertyOperationFlags)(flags | PropertyOperation_NonFixedValue));
    }

    BOOL ActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {TRACE_IT(65239);
        DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
        return true;
    }

    BOOL ActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {TRACE_IT(65240);
        // Var binding of functions declared in eval are elided when conflicting
        // with function scope let/const variables, so do not actually set the
        // property if it exists and is a let/const variable.
        bool noRedecl = false;
        if (!GetTypeHandler()->HasProperty(this, propertyId, &noRedecl) || !noRedecl)
        {TRACE_IT(65241);
            DynamicObject::InitProperty(propertyId, value, PropertyOperation_NonFixedValue);
        }
        return true;
    }

    BOOL ActivationObject::EnsureProperty(PropertyId propertyId)
    {TRACE_IT(65242);
        if (!DynamicObject::HasOwnProperty(propertyId))
        {TRACE_IT(65243);
            DynamicObject::SetPropertyWithAttributes(propertyId, this->GetLibrary()->GetUndefined(), PropertyDynamicTypeDefaults, nullptr, PropertyOperation_NonFixedValue);
        }
        return true;
    }

    BOOL ActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {TRACE_IT(65244);
        ActivationObject::HasOwnPropertyCheckNoRedecl(propertyId);
        return true;
    }

    BOOL ActivationObject::DeleteItem(uint32 index, PropertyOperationFlags flags)
    {TRACE_IT(65245);
        return false;
    }

    BOOL ActivationObject::GetDiagValueString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(65246);
        stringBuilder->AppendCppLiteral(_u("{ActivationObject}"));
        return TRUE;
    }

    BOOL ActivationObject::GetDiagTypeString(StringBuilder<ArenaAllocator>* stringBuilder, ScriptContext* requestContext)
    {TRACE_IT(65247);
        stringBuilder->AppendCppLiteral(_u("Object, (ActivationObject)"));
        return TRUE;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ActivationObject::GetSnapTag_TTD() const
    {TRACE_IT(65248);
        return TTD::NSSnapObjects::SnapObjectType::SnapActivationObject;
    }

    void ActivationObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(65249);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapActivationObject>(objData, nullptr);
    }
#endif

    BOOL BlockActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {TRACE_IT(65250);
        // eval, etc., should not create var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {TRACE_IT(65251);
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::EnsureProperty(PropertyId propertyId)
    {TRACE_IT(65252);
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BOOL BlockActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {TRACE_IT(65253);
        // eval, etc., should not create function var properties on block scope
        return false;
    }

    BlockActivationObject* BlockActivationObject::Clone(ScriptContext *scriptContext)
    {TRACE_IT(65254);
        DynamicType* type = this->GetDynamicType();
        type->GetTypeHandler()->ClearSingletonInstance(); //We are going to share the type.

        BlockActivationObject* blockScopeClone = DynamicObject::NewObject<BlockActivationObject>(scriptContext->GetRecycler(), type);
        int slotCapacity = this->GetTypeHandler()->GetSlotCapacity();

        for (int i = 0; i < slotCapacity; i += 1)
        {TRACE_IT(65255);
            DebugOnly(PropertyId propId = this->GetPropertyId(i));
            Var value = this->GetSlot(i);
            blockScopeClone->SetSlot(SetSlotArguments(propId, i, value));
        }

        return blockScopeClone;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType BlockActivationObject::GetSnapTag_TTD() const
    {TRACE_IT(65256);
        return TTD::NSSnapObjects::SnapObjectType::SnapBlockActivationObject;
    }

    void BlockActivationObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(65257);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapBlockActivationObject>(objData, nullptr);
    }
#endif

    BOOL PseudoActivationObject::InitPropertyScoped(PropertyId propertyId, Var value)
    {TRACE_IT(65258);
        // eval, etc., should not create function properties on something like a "catch" scope
        return false;
    }

    BOOL PseudoActivationObject::InitFuncScoped(PropertyId propertyId, Var value)
    {TRACE_IT(65259);
        // eval, etc., should not create function properties on something like a "catch" scope
        return false;
    }

    BOOL PseudoActivationObject::EnsureProperty(PropertyId propertyId)
    {TRACE_IT(65260);
        return false;
    }

    BOOL PseudoActivationObject::EnsureNoRedeclProperty(PropertyId propertyId)
    {TRACE_IT(65261);
        return false;
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType PseudoActivationObject::GetSnapTag_TTD() const
    {TRACE_IT(65262);
        return TTD::NSSnapObjects::SnapObjectType::SnapPseudoActivationObject;
    }

    void PseudoActivationObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(65263);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapPseudoActivationObject>(objData, nullptr);
    }
#endif

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ConsoleScopeActivationObject::GetSnapTag_TTD() const
    {TRACE_IT(65264);
        return TTD::NSSnapObjects::SnapObjectType::SnapConsoleScopeActivationObject;
    }

    void ConsoleScopeActivationObject::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {TRACE_IT(65265);
        TTD::NSSnapObjects::StdExtractSetKindSpecificInfo<void*, TTD::NSSnapObjects::SnapObjectType::SnapConsoleScopeActivationObject>(objData, nullptr);
    }

#endif

    /* static */
    const PropertyId * ActivationObjectEx::GetCachedScopeInfo(const PropertyIdArray *propIds)
    {TRACE_IT(65266);
        // Cached scope info is appended to the "normal" prop ID array elements.
        return &propIds->elements[propIds->count];
    }

    BOOL ActivationObjectEx::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {TRACE_IT(65267);
        // No need to invalidate the cached scope even if the property is a cached function object.
        // The caller won't be using the object itself.
        return __super::GetProperty(originalInstance, propertyId, value, info, requestContext);
    }

    void ActivationObjectEx::GetPropertyCore(PropertyValueInfo *info, ScriptContext *requestContext)
    {TRACE_IT(65268);
        if (info)
        {TRACE_IT(65269);
            PropertyIndex slot = info->GetPropertyIndex();
            if (slot >= this->firstFuncSlot && slot <= this->lastFuncSlot)
            {TRACE_IT(65270);
                this->parentFunc->InvalidateCachedScopeChain();

                // If the caller is an eval, then each time we execute the eval we need to invalidate the
                // cached scope chain. We can't rely on detecting the escape each time, because inline
                // cache hits may keep us from entering the runtime. So set a flag to make sure the
                // invalidation always happens.
                JavascriptFunction *currentFunc = nullptr;
                JavascriptStackWalker walker(requestContext);
                while (walker.GetCaller(&currentFunc))
                {TRACE_IT(65271);
                    if (walker.IsEvalCaller())
                    {TRACE_IT(65272);
                        //We are walking the stack, so the function body must have been deserialized by this point.
                        currentFunc->GetFunctionBody()->SetFuncEscapes(true);
                        break;
                    }
                }
            }
        }
    }

    BOOL ActivationObjectEx::GetProperty(Var originalInstance, PropertyId propertyId, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {TRACE_IT(65273);
        if (__super::GetProperty(originalInstance, propertyId, value, info, requestContext))
        {
            GetPropertyCore(info, requestContext);
            return TRUE;
        }
        return FALSE;
    }

    BOOL ActivationObjectEx::GetProperty(Var originalInstance, JavascriptString* propertyNameString, Var *value, PropertyValueInfo *info, ScriptContext *requestContext)
    {TRACE_IT(65274);
        if (__super::GetProperty(originalInstance, propertyNameString, value, info, requestContext))
        {
            GetPropertyCore(info, requestContext);
            return TRUE;
        }
        return FALSE;
    }

    void ActivationObjectEx::InvalidateCachedScope()
    {TRACE_IT(65275);
        if (this->cachedFuncCount != 0)
        {TRACE_IT(65276);
            // Clearing the cached functions and types isn't strictly necessary for correctness,
            // but we want those objects to be collected even if the scope object is part of someone's
            // closure environment.
            memset(this->cache, 0, this->cachedFuncCount * sizeof(FuncCacheEntry));
        }
        this->parentFunc->SetCachedScope(nullptr);
    }

    void ActivationObjectEx::SetCachedFunc(uint i, ScriptFunction *func)
    {TRACE_IT(65277);
        Assert(i < cachedFuncCount);
        cache[i].func = func;
        cache[i].type = (DynamicType*)func->GetType();
    }

#if ENABLE_TTD
    TTD::NSSnapObjects::SnapObjectType ActivationObjectEx::GetSnapTag_TTD() const
    {TRACE_IT(65278);
        return TTD::NSSnapObjects::SnapObjectType::Invalid;
    }

    void ActivationObjectEx::ExtractSnapObjectDataInto(TTD::NSSnapObjects::SnapObject* objData, TTD::SlabAllocator& alloc)
    {
        TTDAssert(false, "Not implemented yet!!!");
    }
#endif
};
