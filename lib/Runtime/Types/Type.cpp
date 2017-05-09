//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(Type);

    InternalString Type::ObjectTypeNameString    = InternalString(NO_WRITE_BARRIER_TAG(_u("object")), 6);
    InternalString Type::UndefinedTypeNameString = InternalString(NO_WRITE_BARRIER_TAG(_u("undefined")), 9);
    InternalString Type::BooleanTypeNameString   = InternalString(NO_WRITE_BARRIER_TAG(_u("boolean")), 7);
    InternalString Type::StringTypeNameString    = InternalString(NO_WRITE_BARRIER_TAG(_u("string")), 6);
    InternalString Type::NumberTypeNameString    = InternalString(NO_WRITE_BARRIER_TAG(_u("number")), 6);
    InternalString Type::FunctionTypeNameString  = InternalString(NO_WRITE_BARRIER_TAG(_u("function")), 8);

    Type::Type(ScriptContext* scriptContext, TypeId typeId, RecyclableObject* prototype, JavascriptMethod entryPoint) :
        javascriptLibrary(scriptContext->GetLibrary()),
        typeId(typeId),
        prototype(prototype),
        propertyCache(nullptr),
        flags(TypeFlagMask_None)
    {TRACE_IT(67664);
#ifdef PROFILE_TYPES
        if (typeId < sizeof(scriptContext->typeCount)/sizeof(int))
        {TRACE_IT(67665);
            scriptContext->typeCount[typeId]++;
        }
#endif
        this->entryPoint = entryPoint != nullptr ? entryPoint : RecyclableObject::DefaultEntryPoint;
        if (prototype)
        {TRACE_IT(67666);
            Assert(! CrossSite::NeedMarshalVar(prototype,scriptContext));
            prototype->SetIsPrototype();
        }
    }

    Type::Type(Type * type) :
        typeId(type->typeId),
        javascriptLibrary(type->javascriptLibrary),
        prototype(type->prototype),
        entryPoint(type->entryPoint),
        flags(type->flags),
        propertyCache(nullptr)
    {TRACE_IT(67667);
#ifdef PROFILE_TYPES
        if (typeId < sizeof(javascriptLibrary->GetScriptContext()->typeCount)/sizeof(int))
        {TRACE_IT(67668);
            javascriptLibrary->GetScriptContext()->typeCount[typeId]++;
        }
#endif
        flags = flags & TypeFlagMask(~TypeFlagMask_HasBeenCached);
        Assert(! (prototype && CrossSite::NeedMarshalVar(prototype, javascriptLibrary->GetScriptContext())));

        // If the type property cache is copied over to this new type, then if a property ID caused the type to be changed for
        // the purpose of invalidating caches due to the property being deleted or its attributes being changed, then the cache
        // for that property ID must be cleared on this new type after the type property cache is copied. Also, types are not
        // changed consistently to use this copy constructor, so those would need to be fixed as well.

        if(type->AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties())
        {TRACE_IT(67669);
            SetAreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties(true);
        }
        if(type->IsFalsy())
        {TRACE_IT(67670);
            SetIsFalsy(true);
        }
    }

    ScriptContext *
    Type::GetScriptContext() const
    {
        return GetLibrary()->GetScriptContext();
    }

    Recycler *
    Type::GetRecycler() const
    {TRACE_IT(67672);
        return GetLibrary()->GetRecycler();
    }

    TypePropertyCache *Type::GetPropertyCache()
    {TRACE_IT(67673);
        return propertyCache;
    }

    TypePropertyCache *Type::CreatePropertyCache()
    {TRACE_IT(67674);
        Assert(!propertyCache);

        propertyCache = RecyclerNew(GetRecycler(), TypePropertyCache);
        return propertyCache;
    }

    void Type::SetAreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties(const bool truth)
    {TRACE_IT(67675);
        if (truth)
        {TRACE_IT(67676);
            if (GetScriptContext()->IsClosed())
            {TRACE_IT(67677);
                // The cache is disabled after the script context is closed, to avoid issues between being closed and being deleted,
                // where the cache of these types in JavascriptLibrary may be reclaimed at any point
                return;
            }

            flags |= TypeFlagMask_AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties;
            javascriptLibrary->TypeAndPrototypesAreEnsuredToHaveOnlyWritableDataProperties(this);
        }
        else
        {TRACE_IT(67678);
            flags &= ~TypeFlagMask_AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties;
        }
    }

    BOOL Type::AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties() const
    {TRACE_IT(67679);
        return flags & TypeFlagMask_AreThisAndPrototypesEnsuredToHaveOnlyWritableDataProperties;
    }

    void Type::SetIsFalsy(const bool truth)
    {TRACE_IT(67680);
        if (truth)
        {TRACE_IT(67681);
            Assert(this->GetScriptContext()->GetThreadContext()->CanBeFalsy(this->GetTypeId()));
            flags |= TypeFlagMask_IsFalsy;
        }
        else
        {TRACE_IT(67682);
            flags &= ~TypeFlagMask_IsFalsy;
        }
    }

    void Type::SetHasSpecialPrototype(const bool truth)
    {TRACE_IT(67683);
        if (truth)
        {TRACE_IT(67684);
            flags |= TypeFlagMask_HasSpecialPrototype;
        }
        else
        {TRACE_IT(67685);
            flags &= ~TypeFlagMask_HasSpecialPrototype;
        }
    }

    uint32 Type::GetOffsetOfTypeId()
    {TRACE_IT(67686);
        return offsetof(Type, typeId);
    }

    uint32 Type::GetOffsetOfFlags()
    {TRACE_IT(67687);
        return offsetof(Type, flags);
    }

    uint32 Type::GetOffsetOfEntryPoint()
    {TRACE_IT(67688);
        return offsetof(Type, entryPoint);
    }

    uint32 Type::GetOffsetOfPrototype()
    {TRACE_IT(67689);
        return offsetof(Type, prototype);
    }

#if defined(PROFILE_RECYCLER_ALLOC) && defined(RECYCLER_DUMP_OBJECT_GRAPH)
    bool Type::DumpObjectFunction(type_info const * typeinfo, bool isArray, void * objectAddress)
    {TRACE_IT(67690);
        if (isArray)
        {TRACE_IT(67691);
            // Don't deal with array
            return false;
        }

        Output::Print(_u("%S{%x} %p"), typeinfo->name(), ((Type *)objectAddress)->GetTypeId(), objectAddress);
        return true;
    }
#endif

#if ENABLE_TTD
    void Type::ExtractSnapType(TTD::NSSnapType::SnapType* sType, TTD::NSSnapType::SnapHandler* optHandler, TTD::SlabAllocator& alloc) const
    {TRACE_IT(67692);
        sType->TypePtrId = TTD_CONVERT_TYPEINFO_TO_PTR_ID(this);
        sType->JsTypeId = this->GetTypeId();

        sType->PrototypeVar = this->GetPrototype();

        sType->ScriptContextLogId = this->GetScriptContext()->ScriptContextLogTag;
        sType->TypeHandlerInfo = optHandler;

        sType->HasNoEnumerableProperties = false;
        if(Js::DynamicType::Is(this->typeId))
        {TRACE_IT(67693);
            sType->HasNoEnumerableProperties = static_cast<const Js::DynamicType*>(this)->GetHasNoEnumerableProperties();
        }
    }
#endif
}
