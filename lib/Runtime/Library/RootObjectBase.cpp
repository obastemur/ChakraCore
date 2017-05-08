//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    RootObjectInlineCache::RootObjectInlineCache(InlineCacheAllocator * allocator):
        inlineCache(nullptr), refCount(1)
    {TRACE_IT(62788);
        inlineCache = AllocatorNewZ(InlineCacheAllocator,
            allocator, Js::InlineCache);
    }

    RootObjectBase::RootObjectBase(DynamicType * type) :
        DynamicObject(type), hostObject(nullptr), loadInlineCacheMap(nullptr), loadMethodInlineCacheMap(nullptr), storeInlineCacheMap(nullptr)
    {TRACE_IT(62789);}

    RootObjectBase::RootObjectBase(DynamicType * type, ScriptContext* scriptContext) :
        DynamicObject(type, scriptContext), hostObject(nullptr), loadInlineCacheMap(nullptr), loadMethodInlineCacheMap(nullptr), storeInlineCacheMap(nullptr)
    {TRACE_IT(62790);}

    bool RootObjectBase::Is(Var var)
    {TRACE_IT(62791);
        return RecyclableObject::Is(var) && RootObjectBase::Is(RecyclableObject::FromVar(var));
    }

    bool RootObjectBase::Is(RecyclableObject* obj)
    {TRACE_IT(62792);
        TypeId id = obj->GetTypeId();
        return id == TypeIds_GlobalObject || id == TypeIds_ModuleRoot;
    }

    RootObjectBase * RootObjectBase::FromVar(Var var)
    {TRACE_IT(62793);
        Assert(RootObjectBase::Is(var));
        return static_cast<Js::RootObjectBase *>(var);
    }

    HostObjectBase * RootObjectBase::GetHostObject() const
    {TRACE_IT(62794);
        Assert(hostObject == nullptr || Js::JavascriptOperators::GetTypeId(hostObject) == TypeIds_HostObject);

        return this->hostObject;
    }

    void RootObjectBase::SetHostObject(HostObjectBase * hostObject)
    {TRACE_IT(62795);
        Assert(hostObject == nullptr || Js::JavascriptOperators::GetTypeId(hostObject) == TypeIds_HostObject);
        this->hostObject = hostObject;
    }

    Js::InlineCache *
    RootObjectBase::GetInlineCache(Js::PropertyRecord const* propertyRecord, bool isLoadMethod, bool isStore)
    {TRACE_IT(62796);
        Js::RootObjectInlineCache * rootObjectInlineCache = GetRootInlineCache(propertyRecord, isLoadMethod, isStore);
        return rootObjectInlineCache->GetInlineCache();
    }

    Js::RootObjectInlineCache*
    RootObjectBase::GetRootInlineCache(Js::PropertyRecord const* propertyRecord, bool isLoadMethod, bool isStore)
    {TRACE_IT(62797);
        RootObjectInlineCacheMap * inlineCacheMap = isStore ? storeInlineCacheMap :
            isLoadMethod ? loadMethodInlineCacheMap : loadInlineCacheMap;
        Js::RootObjectInlineCache * rootObjectInlineCache;
        if (inlineCacheMap == nullptr)
        {TRACE_IT(62798);
            Recycler * recycler = this->GetLibrary()->GetRecycler();
            inlineCacheMap = RecyclerNew(recycler, RootObjectInlineCacheMap, recycler);
            if (isStore)
            {TRACE_IT(62799);
                this->storeInlineCacheMap = inlineCacheMap;
            }
            else if (isLoadMethod)
            {TRACE_IT(62800);
                this->loadMethodInlineCacheMap = inlineCacheMap;
            }
            else
            {TRACE_IT(62801);
                this->loadInlineCacheMap = inlineCacheMap;
            }
        }
        else if (inlineCacheMap->TryGetValue(propertyRecord, &rootObjectInlineCache))
        {TRACE_IT(62802);
            rootObjectInlineCache->AddRef();
            return rootObjectInlineCache;
        }

        Recycler * recycler = this->GetLibrary()->GetRecycler();
        rootObjectInlineCache = RecyclerNewLeaf(recycler, RootObjectInlineCache, this->GetScriptContext()->GetInlineCacheAllocator());
        inlineCacheMap->Add(propertyRecord, rootObjectInlineCache);

        return rootObjectInlineCache;
    }

    // TODO: Switch to take PropertyRecord instead once we clean up the function body to hold onto propertyRecord
    // instead of propertyId.
    void
    RootObjectBase::ReleaseInlineCache(Js::PropertyId propertyId, bool isLoadMethod, bool isStore, bool isShutdown)
    {TRACE_IT(62803);
        uint unregisteredInlineCacheCount = 0;

        RootObjectInlineCacheMap * inlineCacheMap = isStore ? storeInlineCacheMap :
            isLoadMethod ? loadMethodInlineCacheMap : loadInlineCacheMap;
        bool found = false;
        inlineCacheMap->RemoveIfWithKey(propertyId,
            [this, isShutdown, &unregisteredInlineCacheCount, &found](PropertyRecord const * propertyRecord, RootObjectInlineCache * rootObjectInlineCache)
            {
                found = true;
                if (rootObjectInlineCache->Release() == 0)
                {TRACE_IT(62804);
                    // If we're not shutting down, we need to remove this cache from thread context's invalidation list (if any),
                    // and release memory back to the arena.  During script context shutdown, we leave everything in place, because
                    // the inline cache arena will stay alive until script context is destroyed (as in destructor called as opposed to
                    // Close called) and thus the invalidation lists are safe to keep references to caches from this script context.
                    if (!isShutdown)
                    {TRACE_IT(62805);
                        if (rootObjectInlineCache->GetInlineCache()->RemoveFromInvalidationList())
                        {TRACE_IT(62806);
                            unregisteredInlineCacheCount++;

                        }
                        AllocatorDelete(InlineCacheAllocator, this->GetScriptContext()->GetInlineCacheAllocator(), rootObjectInlineCache->GetInlineCache());
                    }
                    return true; // Remove from the map
                }
                return false; // don't remove from the map
            }
        );
        Assert(found);
        if (unregisteredInlineCacheCount > 0)
        {TRACE_IT(62807);
            this->GetScriptContext()->GetThreadContext()->NotifyInlineCacheBatchUnregistered(unregisteredInlineCacheCount);
        }
    }

    BOOL
    RootObjectBase::EnsureProperty(PropertyId propertyId)
    {TRACE_IT(62808);
        if (!RootObjectBase::HasOwnPropertyCheckNoRedecl(propertyId))
        {TRACE_IT(62809);
            this->InitProperty(propertyId, this->GetLibrary()->GetUndefined(),
                static_cast<Js::PropertyOperationFlags>(PropertyOperation_PreInit | PropertyOperation_SpecialValue));
        }
        return true;
    }

    BOOL
    RootObjectBase::EnsureNoRedeclProperty(PropertyId propertyId)
    {TRACE_IT(62810);
        RootObjectBase::HasOwnPropertyCheckNoRedecl(propertyId);
        return true;
    }

    BOOL
    RootObjectBase::HasOwnPropertyCheckNoRedecl(PropertyId propertyId)
    {TRACE_IT(62811);
        bool noRedecl = false;
        if (!GetTypeHandler()->HasRootProperty(this, propertyId, &noRedecl))
        {TRACE_IT(62812);
            return FALSE;
        }
        else if (noRedecl)
        {TRACE_IT(62813);
            JavascriptError::ThrowReferenceError(GetScriptContext(), ERRRedeclaration);
        }
        return true;
    }


    BOOL
    RootObjectBase::HasRootProperty(PropertyId propertyId)
    {TRACE_IT(62814);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->HasRootProperty(this, propertyId);
    }

    BOOL
    RootObjectBase::GetRootProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62815);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL
    RootObjectBase::GetRootPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62816);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL
    RootObjectBase::SetRootProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(62817);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->SetRootProperty(this, propertyId, value, flags, info);
    }

    DescriptorFlags
    RootObjectBase::GetRootSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(62818);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootSetter(this, propertyId, setterValue, info, requestContext);
    }

    BOOL
    RootObjectBase::DeleteRootProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(62819);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->DeleteRootProperty(this, propertyId, flags);
    }

    PropertyIndex
    RootObjectBase::GetRootPropertyIndex(PropertyId propertyId)
    {TRACE_IT(62820);
        Assert(!Js::IsInternalPropertyId(propertyId));
        Assert(propertyId != Constants::NoProperty);
        return GetTypeHandler()->GetRootPropertyIndex(this->GetScriptContext()->GetPropertyName(propertyId));
    }

    void
    RootObjectBase::EnsureNoProperty(PropertyId propertyId)
    {TRACE_IT(62821);
        Assert(!Js::IsInternalPropertyId(propertyId));
        bool isDeclared = false;
        bool isNonconfigurable = false;
        if (GetTypeHandler()->HasRootProperty(this, propertyId, nullptr, &isDeclared, &isNonconfigurable) &&
            (isDeclared || isNonconfigurable))
        {TRACE_IT(62822);
            JavascriptError::ThrowReferenceError(this->GetScriptContext(), ERRRedeclaration);
        }
    }

#if DBG
    bool
    RootObjectBase::IsLetConstGlobal(PropertyId propertyId)
    {TRACE_IT(62823);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->IsLetConstGlobal(this, propertyId);
    }
#endif
}
