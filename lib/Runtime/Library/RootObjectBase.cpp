//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    RootObjectInlineCache::RootObjectInlineCache(InlineCacheAllocator * allocator):
        inlineCache(nullptr), refCount(1)
    {LOGMEIN("RootObjectBase.cpp] 10\n");
        inlineCache = AllocatorNewZ(InlineCacheAllocator,
            allocator, Js::InlineCache);
    }

    RootObjectBase::RootObjectBase(DynamicType * type) :
        DynamicObject(type), hostObject(nullptr), loadInlineCacheMap(nullptr), loadMethodInlineCacheMap(nullptr), storeInlineCacheMap(nullptr)
    {LOGMEIN("RootObjectBase.cpp] 17\n");}

    RootObjectBase::RootObjectBase(DynamicType * type, ScriptContext* scriptContext) :
        DynamicObject(type, scriptContext), hostObject(nullptr), loadInlineCacheMap(nullptr), loadMethodInlineCacheMap(nullptr), storeInlineCacheMap(nullptr)
    {LOGMEIN("RootObjectBase.cpp] 21\n");}

    bool RootObjectBase::Is(Var var)
    {LOGMEIN("RootObjectBase.cpp] 24\n");
        return RecyclableObject::Is(var) && RootObjectBase::Is(RecyclableObject::FromVar(var));
    }

    bool RootObjectBase::Is(RecyclableObject* obj)
    {LOGMEIN("RootObjectBase.cpp] 29\n");
        TypeId id = obj->GetTypeId();
        return id == TypeIds_GlobalObject || id == TypeIds_ModuleRoot;
    }

    RootObjectBase * RootObjectBase::FromVar(Var var)
    {LOGMEIN("RootObjectBase.cpp] 35\n");
        Assert(RootObjectBase::Is(var));
        return static_cast<Js::RootObjectBase *>(var);
    }

    HostObjectBase * RootObjectBase::GetHostObject() const
    {LOGMEIN("RootObjectBase.cpp] 41\n");
        Assert(hostObject == nullptr || Js::JavascriptOperators::GetTypeId(hostObject) == TypeIds_HostObject);

        return this->hostObject;
    }

    void RootObjectBase::SetHostObject(HostObjectBase * hostObject)
    {LOGMEIN("RootObjectBase.cpp] 48\n");
        Assert(hostObject == nullptr || Js::JavascriptOperators::GetTypeId(hostObject) == TypeIds_HostObject);
        this->hostObject = hostObject;
    }

    Js::InlineCache *
    RootObjectBase::GetInlineCache(Js::PropertyRecord const* propertyRecord, bool isLoadMethod, bool isStore)
    {LOGMEIN("RootObjectBase.cpp] 55\n");
        Js::RootObjectInlineCache * rootObjectInlineCache = GetRootInlineCache(propertyRecord, isLoadMethod, isStore);
        return rootObjectInlineCache->GetInlineCache();
    }

    Js::RootObjectInlineCache*
    RootObjectBase::GetRootInlineCache(Js::PropertyRecord const* propertyRecord, bool isLoadMethod, bool isStore)
    {LOGMEIN("RootObjectBase.cpp] 62\n");
        RootObjectInlineCacheMap * inlineCacheMap = isStore ? storeInlineCacheMap :
            isLoadMethod ? loadMethodInlineCacheMap : loadInlineCacheMap;
        Js::RootObjectInlineCache * rootObjectInlineCache;
        if (inlineCacheMap == nullptr)
        {LOGMEIN("RootObjectBase.cpp] 67\n");
            Recycler * recycler = this->GetLibrary()->GetRecycler();
            inlineCacheMap = RecyclerNew(recycler, RootObjectInlineCacheMap, recycler);
            if (isStore)
            {LOGMEIN("RootObjectBase.cpp] 71\n");
                this->storeInlineCacheMap = inlineCacheMap;
            }
            else if (isLoadMethod)
            {LOGMEIN("RootObjectBase.cpp] 75\n");
                this->loadMethodInlineCacheMap = inlineCacheMap;
            }
            else
            {
                this->loadInlineCacheMap = inlineCacheMap;
            }
        }
        else if (inlineCacheMap->TryGetValue(propertyRecord, &rootObjectInlineCache))
        {LOGMEIN("RootObjectBase.cpp] 84\n");
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
    {LOGMEIN("RootObjectBase.cpp] 100\n");
        uint unregisteredInlineCacheCount = 0;

        RootObjectInlineCacheMap * inlineCacheMap = isStore ? storeInlineCacheMap :
            isLoadMethod ? loadMethodInlineCacheMap : loadInlineCacheMap;
        bool found = false;
        inlineCacheMap->RemoveIfWithKey(propertyId,
            [this, isShutdown, &unregisteredInlineCacheCount, &found](PropertyRecord const * propertyRecord, RootObjectInlineCache * rootObjectInlineCache)
            {
                found = true;
                if (rootObjectInlineCache->Release() == 0)
                {LOGMEIN("RootObjectBase.cpp] 111\n");
                    // If we're not shutting down, we need to remove this cache from thread context's invalidation list (if any),
                    // and release memory back to the arena.  During script context shutdown, we leave everything in place, because
                    // the inline cache arena will stay alive until script context is destroyed (as in destructor called as opposed to
                    // Close called) and thus the invalidation lists are safe to keep references to caches from this script context.
                    if (!isShutdown)
                    {LOGMEIN("RootObjectBase.cpp] 117\n");
                        if (rootObjectInlineCache->GetInlineCache()->RemoveFromInvalidationList())
                        {LOGMEIN("RootObjectBase.cpp] 119\n");
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
        {LOGMEIN("RootObjectBase.cpp] 132\n");
            this->GetScriptContext()->GetThreadContext()->NotifyInlineCacheBatchUnregistered(unregisteredInlineCacheCount);
        }
    }

    BOOL
    RootObjectBase::EnsureProperty(PropertyId propertyId)
    {LOGMEIN("RootObjectBase.cpp] 139\n");
        if (!RootObjectBase::HasOwnPropertyCheckNoRedecl(propertyId))
        {LOGMEIN("RootObjectBase.cpp] 141\n");
            this->InitProperty(propertyId, this->GetLibrary()->GetUndefined(),
                static_cast<Js::PropertyOperationFlags>(PropertyOperation_PreInit | PropertyOperation_SpecialValue));
        }
        return true;
    }

    BOOL
    RootObjectBase::EnsureNoRedeclProperty(PropertyId propertyId)
    {LOGMEIN("RootObjectBase.cpp] 150\n");
        RootObjectBase::HasOwnPropertyCheckNoRedecl(propertyId);
        return true;
    }

    BOOL
    RootObjectBase::HasOwnPropertyCheckNoRedecl(PropertyId propertyId)
    {LOGMEIN("RootObjectBase.cpp] 157\n");
        bool noRedecl = false;
        if (!GetTypeHandler()->HasRootProperty(this, propertyId, &noRedecl))
        {LOGMEIN("RootObjectBase.cpp] 160\n");
            return FALSE;
        }
        else if (noRedecl)
        {LOGMEIN("RootObjectBase.cpp] 164\n");
            JavascriptError::ThrowReferenceError(GetScriptContext(), ERRRedeclaration);
        }
        return true;
    }


    BOOL
    RootObjectBase::HasRootProperty(PropertyId propertyId)
    {LOGMEIN("RootObjectBase.cpp] 173\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->HasRootProperty(this, propertyId);
    }

    BOOL
    RootObjectBase::GetRootProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("RootObjectBase.cpp] 180\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL
    RootObjectBase::GetRootPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("RootObjectBase.cpp] 187\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootProperty(this, originalInstance, propertyId, value, info, requestContext);
    }

    BOOL
    RootObjectBase::SetRootProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("RootObjectBase.cpp] 194\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->SetRootProperty(this, propertyId, value, flags, info);
    }

    DescriptorFlags
    RootObjectBase::GetRootSetter(PropertyId propertyId, Var* setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("RootObjectBase.cpp] 201\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->GetRootSetter(this, propertyId, setterValue, info, requestContext);
    }

    BOOL
    RootObjectBase::DeleteRootProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("RootObjectBase.cpp] 208\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->DeleteRootProperty(this, propertyId, flags);
    }

    PropertyIndex
    RootObjectBase::GetRootPropertyIndex(PropertyId propertyId)
    {LOGMEIN("RootObjectBase.cpp] 215\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        Assert(propertyId != Constants::NoProperty);
        return GetTypeHandler()->GetRootPropertyIndex(this->GetScriptContext()->GetPropertyName(propertyId));
    }

    void
    RootObjectBase::EnsureNoProperty(PropertyId propertyId)
    {LOGMEIN("RootObjectBase.cpp] 223\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        bool isDeclared = false;
        bool isNonconfigurable = false;
        if (GetTypeHandler()->HasRootProperty(this, propertyId, nullptr, &isDeclared, &isNonconfigurable) &&
            (isDeclared || isNonconfigurable))
        {LOGMEIN("RootObjectBase.cpp] 229\n");
            JavascriptError::ThrowReferenceError(this->GetScriptContext(), ERRRedeclaration);
        }
    }

#if DBG
    bool
    RootObjectBase::IsLetConstGlobal(PropertyId propertyId)
    {LOGMEIN("RootObjectBase.cpp] 237\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        return GetTypeHandler()->IsLetConstGlobal(this, propertyId);
    }
#endif
}
