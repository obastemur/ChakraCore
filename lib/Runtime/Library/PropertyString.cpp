//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(PropertyString);
    DEFINE_RECYCLER_TRACKER_WEAKREF_PERF_COUNTER(PropertyString);

    PropertyString::PropertyString(StaticType* type, const Js::PropertyRecord* propertyRecord)
        : JavascriptString(type, propertyRecord->GetLength(), propertyRecord->GetBuffer()), m_propertyRecord(propertyRecord)
    {TRACE_IT(62702);
    }

    PropertyString* PropertyString::New(StaticType* type, const Js::PropertyRecord* propertyRecord, ArenaAllocator *arena)
    {TRACE_IT(62703);
        PropertyString * propertyString = (PropertyString *)Anew(arena, ArenaAllocPropertyString, type, propertyRecord);
        propertyString->propCache = AllocatorNewStructZ(InlineCacheAllocator, type->GetScriptContext()->GetInlineCacheAllocator(), PropertyCache);
        return propertyString;
    }


    PropertyString* PropertyString::New(StaticType* type, const Js::PropertyRecord* propertyRecord, Recycler *recycler)
    {TRACE_IT(62704);
        PropertyString * propertyString =  RecyclerNewPlusZ(recycler, sizeof(PropertyCache), PropertyString, type, propertyRecord);
        propertyString->propCache = (PropertyCache*)(propertyString + 1);
        return propertyString;
    }

    PropertyCache const * PropertyString::GetPropertyCache() const
    {TRACE_IT(62705);
        Assert(!propCache->type  || propCache->type->GetScriptContext() == this->GetScriptContext());
        return propCache;
    }

    void PropertyString::ClearPropertyCache()
    {TRACE_IT(62706);
        this->propCache->type = nullptr;
    }
    void const * PropertyString::GetOriginalStringReference()
    {TRACE_IT(62707);
        // Property record is the allocation containing the string buffer
        return this->m_propertyRecord;
    }

    RecyclableObject * PropertyString::CloneToScriptContext(ScriptContext* requestContext)
    {TRACE_IT(62708);
        return requestContext->GetLibrary()->CreatePropertyString(this->m_propertyRecord);
    }

    void PropertyString::UpdateCache(Type * type, uint16 dataSlotIndex, bool isInlineSlot, bool isStoreFieldEnabled)
    {TRACE_IT(62709);
        Assert(type);
        
        if (type->GetScriptContext() != this->GetScriptContext())
        {TRACE_IT(62710);
            return;
        }

        if (this->IsArenaAllocPropertyString())
        {TRACE_IT(62711);
            this->GetScriptContext()->SetHasUsedInlineCache(true);
        }

        type->SetHasBeenCached();
        this->propCache->type = type;
        this->propCache->preventdataSlotIndexFalseRef = 1;
        this->propCache->dataSlotIndex = dataSlotIndex;
        this->propCache->preventFlagsFalseRef = 1;
        this->propCache->isInlineSlot = isInlineSlot;
        this->propCache->isStoreFieldEnabled = isStoreFieldEnabled;
    }

    ArenaAllocPropertyString::ArenaAllocPropertyString(StaticType* type, const Js::PropertyRecord* propertyRecord)
        :PropertyString(type, propertyRecord)
    {TRACE_IT(62712);}
}
