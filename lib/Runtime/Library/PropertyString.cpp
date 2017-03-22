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
    {LOGMEIN("PropertyString.cpp] 13\n");
    }

    PropertyString* PropertyString::New(StaticType* type, const Js::PropertyRecord* propertyRecord, ArenaAllocator *arena)
    {LOGMEIN("PropertyString.cpp] 17\n");
        PropertyString * propertyString = (PropertyString *)Anew(arena, ArenaAllocPropertyString, type, propertyRecord);
        propertyString->propCache = AllocatorNewStructZ(InlineCacheAllocator, type->GetScriptContext()->GetInlineCacheAllocator(), PropertyCache);
        return propertyString;
    }


    PropertyString* PropertyString::New(StaticType* type, const Js::PropertyRecord* propertyRecord, Recycler *recycler)
    {LOGMEIN("PropertyString.cpp] 25\n");
        PropertyString * propertyString =  RecyclerNewPlusZ(recycler, sizeof(PropertyCache), PropertyString, type, propertyRecord);
        propertyString->propCache = (PropertyCache*)(propertyString + 1);
        return propertyString;
    }

    PropertyCache const * PropertyString::GetPropertyCache() const
    {LOGMEIN("PropertyString.cpp] 32\n");
        Assert(!propCache->type  || propCache->type->GetScriptContext() == this->GetScriptContext());
        return propCache;
    }

    void PropertyString::ClearPropertyCache()
    {LOGMEIN("PropertyString.cpp] 38\n");
        this->propCache->type = nullptr;
    }
    void const * PropertyString::GetOriginalStringReference()
    {LOGMEIN("PropertyString.cpp] 42\n");
        // Property record is the allocation containing the string buffer
        return this->m_propertyRecord;
    }

    RecyclableObject * PropertyString::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("PropertyString.cpp] 48\n");
        return requestContext->GetLibrary()->CreatePropertyString(this->m_propertyRecord);
    }

    void PropertyString::UpdateCache(Type * type, uint16 dataSlotIndex, bool isInlineSlot, bool isStoreFieldEnabled)
    {LOGMEIN("PropertyString.cpp] 53\n");
        Assert(type);
        
        if (type->GetScriptContext() != this->GetScriptContext())
        {LOGMEIN("PropertyString.cpp] 57\n");
            return;
        }

        if (this->IsArenaAllocPropertyString())
        {LOGMEIN("PropertyString.cpp] 62\n");
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
    {LOGMEIN("PropertyString.cpp] 77\n");}
}
