//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    bool DynamicObjectPropertyEnumerator::GetEnumNonEnumerable() const
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 9\n");
        return !!(flags & EnumeratorFlags::EnumNonEnumerable);
    }
    bool DynamicObjectPropertyEnumerator::GetEnumSymbols() const
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 13\n");
        return !!(flags & EnumeratorFlags::EnumSymbols);
    }
    bool DynamicObjectPropertyEnumerator::GetSnapShotSemantics() const
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 17\n");
        return !!(flags & EnumeratorFlags::SnapShotSemantics);
    }

    bool DynamicObjectPropertyEnumerator::GetUseCache() const
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 22\n");
#if ENABLE_TTD
        if(this->scriptContext->GetThreadContext()->IsRuntimeInTTDMode())
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 25\n");
            return false;
        }
#endif

        return ((flags & (EnumeratorFlags::SnapShotSemantics | EnumeratorFlags::UseCache)) == (EnumeratorFlags::SnapShotSemantics | EnumeratorFlags::UseCache));
    }

    void DynamicObjectPropertyEnumerator::Initialize(DynamicType * type, CachedData * data, Js::BigPropertyIndex initialPropertyCount)
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 34\n");
        this->initialType = type;
        this->cachedData = data;
        this->initialPropertyCount = initialPropertyCount;
    }

    bool DynamicObjectPropertyEnumerator::Initialize(DynamicObject * object, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache)
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 41\n");
        this->scriptContext = requestContext;
        this->object = object;
        this->flags = flags;

        if (!object)
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 47\n");
            this->cachedData = nullptr;
            return true;
        }

        this->objectIndex = Constants::NoBigSlot;
        this->enumeratedCount = 0;

        if (!GetUseCache())
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 56\n");
            if (!object->GetDynamicType()->GetTypeHandler()->EnsureObjectReady(object))
            {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 58\n");
                return false;
            }
            Initialize(object->GetDynamicType(), nullptr, GetSnapShotSemantics() ? this->object->GetPropertyCount() : Constants::NoBigSlot);
            return true;
        }

        DynamicType * type = object->GetDynamicType();

        CachedData * data;
        if (forInCache && type == forInCache->type)
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 69\n");
            // We shouldn't have a for in cache when asking to enum symbols
            Assert(!GetEnumSymbols());
            data = (CachedData *)forInCache->data;

            Assert(data != nullptr);
            Assert(data->scriptContext == this->scriptContext); // The cache data script context should be the same as request context
            Assert(!data->enumSymbols);

            if (data->enumNonEnumerable == GetEnumNonEnumerable())
            {
                Initialize(type, data, data->propertyCount);
                return true;
            }
        }

        data = (CachedData *)requestContext->GetThreadContext()->GetDynamicObjectEnumeratorCache(type);

        if (data != nullptr && data->scriptContext == this->scriptContext && data->enumNonEnumerable == GetEnumNonEnumerable() && data->enumSymbols == GetEnumSymbols())
        {
            Initialize(type, data, data->propertyCount);

            if (forInCache)
            {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 92\n");
                forInCache->type = type;
                forInCache->data = data;
            }
            return true;
        }

        if (!object->GetDynamicType()->GetTypeHandler()->EnsureObjectReady(object))
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 100\n");
            return false;
        }

        // Reload the type after EnsureObjecteReady
        type = object->GetDynamicType();
        if (!type->PrepareForTypeSnapshotEnumeration())
        {
            Initialize(type, nullptr, object->GetPropertyCount());
            return true;
        }

        uint propertyCount = this->object->GetPropertyCount();
        data = RecyclerNewStructPlus(requestContext->GetRecycler(),
            propertyCount * sizeof(Field(PropertyString*)) + propertyCount * sizeof(BigPropertyIndex) + propertyCount * sizeof(PropertyAttributes), CachedData);
        data->scriptContext = requestContext;
        data->cachedCount = 0;
        data->propertyCount = propertyCount;
        data->strings = reinterpret_cast<Field(PropertyString*)*>(data + 1);
        data->indexes = (BigPropertyIndex *)(data->strings + propertyCount);
        data->attributes = (PropertyAttributes*)(data->indexes + propertyCount);
        data->completed = false;
        data->enumNonEnumerable = GetEnumNonEnumerable();
        data->enumSymbols = GetEnumSymbols();
        requestContext->GetThreadContext()->AddDynamicObjectEnumeratorCache(type, data);
        Initialize(type, data, propertyCount);

        if (forInCache)
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 128\n");
            forInCache->type = type;
            forInCache->data = data;
        }
        return true;
    }

    bool DynamicObjectPropertyEnumerator::IsNullEnumerator() const
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 136\n");
        return this->object == nullptr;
    }

    bool DynamicObjectPropertyEnumerator::CanUseJITFastPath() const
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 141\n");
#if ENABLE_TTD
        TTDAssert(this->cachedData == nullptr || !this->scriptContext->GetThreadContext()->IsRuntimeInTTDMode(), "We should always have cachedData null if we are in record or replay mode");
#endif

        return !this->IsNullEnumerator() && !GetEnumNonEnumerable() && this->cachedData != nullptr;
    }

    void DynamicObjectPropertyEnumerator::Clear(EnumeratorFlags flags, ScriptContext * requestContext)
    {
        Initialize(nullptr, flags, requestContext, nullptr);
    }

    void DynamicObjectPropertyEnumerator::Reset()
    {
        Initialize(object, flags, scriptContext, nullptr);
    }

    DynamicType * DynamicObjectPropertyEnumerator::GetTypeToEnumerate() const
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 160\n");
        return
            GetSnapShotSemantics() &&
            initialType->GetIsLocked() &&
            CONFIG_FLAG(TypeSnapshotEnumeration)
            ? PointerValue(initialType)
            : object->GetDynamicType();
    }

    JavascriptString * DynamicObjectPropertyEnumerator::MoveAndGetNextWithCache(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 170\n");
#if ENABLE_TTD
        AssertMsg(!this->scriptContext->GetThreadContext()->IsRuntimeInTTDMode(), "We should always trap out to explicit enumeration in this case");
#endif

        Assert(enumeratedCount <= cachedData->cachedCount);
        JavascriptString* propertyStringName;
        PropertyAttributes propertyAttributes = PropertyNone;
        if (enumeratedCount < cachedData->cachedCount)
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 179\n");
            PropertyString * propertyString = cachedData->strings[enumeratedCount];
            propertyStringName = propertyString;
            propertyId = propertyString->GetPropertyRecord()->GetPropertyId();

#if DBG
            PropertyId tempPropertyId;
            /* JavascriptString * tempPropertyString = */ this->MoveAndGetNextNoCache(tempPropertyId, attributes);

            Assert(tempPropertyId == propertyId);
            Assert(this->objectIndex == cachedData->indexes[enumeratedCount]);
#endif

            this->objectIndex = cachedData->indexes[enumeratedCount];
            propertyAttributes = cachedData->attributes[enumeratedCount];

            enumeratedCount++;
        }
        else if (!cachedData->completed)
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 198\n");
            propertyStringName = this->MoveAndGetNextNoCache(propertyId, &propertyAttributes);

            if (propertyStringName && VirtualTableInfo<PropertyString>::HasVirtualTable(propertyStringName))
            {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 202\n");
                Assert(enumeratedCount < this->initialPropertyCount);
                cachedData->strings[enumeratedCount] = (PropertyString*)propertyStringName;
                cachedData->indexes[enumeratedCount] = this->objectIndex;
                cachedData->attributes[enumeratedCount] = propertyAttributes;
                cachedData->cachedCount = ++enumeratedCount;
            }
            else
            {
                cachedData->completed = true;
            }
        }
        else
        {
#if DBG
            PropertyId tempPropertyId;
            Assert(this->MoveAndGetNextNoCache(tempPropertyId, attributes) == nullptr);
#endif

            propertyStringName = nullptr;
        }

        if (attributes != nullptr)
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 225\n");
            *attributes = propertyAttributes;
        }

        return propertyStringName;
    }
    JavascriptString * DynamicObjectPropertyEnumerator::MoveAndGetNextNoCache(PropertyId& propertyId, PropertyAttributes * attributes)
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 232\n");
        JavascriptString* propertyString = nullptr;

        BigPropertyIndex newIndex = this->objectIndex;
        do
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 237\n");
            newIndex++;
            if (!object->FindNextProperty(newIndex, &propertyString, &propertyId, attributes,
                GetTypeToEnumerate(), flags, this->scriptContext)
                || (GetSnapShotSemantics() && newIndex >= initialPropertyCount))
            {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 242\n");
                // No more properties
                newIndex--;
                propertyString = nullptr;
                break;
            }
        } while (Js::IsInternalPropertyId(propertyId));

        this->objectIndex = newIndex;
        return propertyString;
    }

    Var DynamicObjectPropertyEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes * attributes)
    {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 255\n");
        if (this->cachedData && this->initialType == this->object->GetDynamicType())
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 257\n");
            return MoveAndGetNextWithCache(propertyId, attributes);
        }
        if (this->object)
        {LOGMEIN("DynamicObjectPropertyEnumerator.cpp] 261\n");
            // Once enters NoCache path, ensure never switches to Cache path above.
            this->cachedData = nullptr;
            return MoveAndGetNextNoCache(propertyId, attributes);
        }
        return nullptr;
    }
}