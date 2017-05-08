//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    bool DynamicObjectPropertyEnumerator::GetEnumNonEnumerable() const
    {TRACE_IT(65981);
        return !!(flags & EnumeratorFlags::EnumNonEnumerable);
    }
    bool DynamicObjectPropertyEnumerator::GetEnumSymbols() const
    {TRACE_IT(65982);
        return !!(flags & EnumeratorFlags::EnumSymbols);
    }
    bool DynamicObjectPropertyEnumerator::GetSnapShotSemantics() const
    {TRACE_IT(65983);
        return !!(flags & EnumeratorFlags::SnapShotSemantics);
    }

    bool DynamicObjectPropertyEnumerator::GetUseCache() const
    {TRACE_IT(65984);
#if ENABLE_TTD
        if(this->scriptContext->GetThreadContext()->IsRuntimeInTTDMode())
        {TRACE_IT(65985);
            return false;
        }
#endif

        return ((flags & (EnumeratorFlags::SnapShotSemantics | EnumeratorFlags::UseCache)) == (EnumeratorFlags::SnapShotSemantics | EnumeratorFlags::UseCache));
    }

    void DynamicObjectPropertyEnumerator::Initialize(DynamicType * type, CachedData * data, Js::BigPropertyIndex initialPropertyCount)
    {TRACE_IT(65986);
        this->initialType = type;
        this->cachedData = data;
        this->initialPropertyCount = initialPropertyCount;
    }

    bool DynamicObjectPropertyEnumerator::Initialize(DynamicObject * object, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache)
    {TRACE_IT(65987);
        this->scriptContext = requestContext;
        this->object = object;
        this->flags = flags;

        if (!object)
        {TRACE_IT(65988);
            this->cachedData = nullptr;
            return true;
        }

        this->objectIndex = Constants::NoBigSlot;
        this->enumeratedCount = 0;

        if (!GetUseCache())
        {TRACE_IT(65989);
            if (!object->GetDynamicType()->GetTypeHandler()->EnsureObjectReady(object))
            {TRACE_IT(65990);
                return false;
            }
            Initialize(object->GetDynamicType(), nullptr, GetSnapShotSemantics() ? this->object->GetPropertyCount() : Constants::NoBigSlot);
            return true;
        }

        DynamicType * type = object->GetDynamicType();

        CachedData * data;
        if (forInCache && type == forInCache->type)
        {TRACE_IT(65991);
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
            {TRACE_IT(65992);
                forInCache->type = type;
                forInCache->data = data;
            }
            return true;
        }

        if (!object->GetDynamicType()->GetTypeHandler()->EnsureObjectReady(object))
        {TRACE_IT(65993);
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
        {TRACE_IT(65994);
            forInCache->type = type;
            forInCache->data = data;
        }
        return true;
    }

    bool DynamicObjectPropertyEnumerator::IsNullEnumerator() const
    {TRACE_IT(65995);
        return this->object == nullptr;
    }

    bool DynamicObjectPropertyEnumerator::CanUseJITFastPath() const
    {TRACE_IT(65996);
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
    {TRACE_IT(65997);
        return
            GetSnapShotSemantics() &&
            initialType->GetIsLocked() &&
            CONFIG_FLAG(TypeSnapshotEnumeration)
            ? PointerValue(initialType)
            : object->GetDynamicType();
    }

    JavascriptString * DynamicObjectPropertyEnumerator::MoveAndGetNextWithCache(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(65998);
#if ENABLE_TTD
        AssertMsg(!this->scriptContext->GetThreadContext()->IsRuntimeInTTDMode(), "We should always trap out to explicit enumeration in this case");
#endif

        Assert(enumeratedCount <= cachedData->cachedCount);
        JavascriptString* propertyStringName;
        PropertyAttributes propertyAttributes = PropertyNone;
        if (enumeratedCount < cachedData->cachedCount)
        {TRACE_IT(65999);
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
        {TRACE_IT(66000);
            propertyStringName = this->MoveAndGetNextNoCache(propertyId, &propertyAttributes);

            if (propertyStringName && VirtualTableInfo<PropertyString>::HasVirtualTable(propertyStringName))
            {TRACE_IT(66001);
                Assert(enumeratedCount < this->initialPropertyCount);
                cachedData->strings[enumeratedCount] = (PropertyString*)propertyStringName;
                cachedData->indexes[enumeratedCount] = this->objectIndex;
                cachedData->attributes[enumeratedCount] = propertyAttributes;
                cachedData->cachedCount = ++enumeratedCount;
            }
            else
            {TRACE_IT(66002);
                cachedData->completed = true;
            }
        }
        else
        {TRACE_IT(66003);
#if DBG
            PropertyId tempPropertyId;
            Assert(this->MoveAndGetNextNoCache(tempPropertyId, attributes) == nullptr);
#endif

            propertyStringName = nullptr;
        }

        if (attributes != nullptr)
        {TRACE_IT(66004);
            *attributes = propertyAttributes;
        }

        return propertyStringName;
    }
    JavascriptString * DynamicObjectPropertyEnumerator::MoveAndGetNextNoCache(PropertyId& propertyId, PropertyAttributes * attributes)
    {TRACE_IT(66005);
        JavascriptString* propertyString = nullptr;

        BigPropertyIndex newIndex = this->objectIndex;
        do
        {TRACE_IT(66006);
            newIndex++;
            if (!object->FindNextProperty(newIndex, &propertyString, &propertyId, attributes,
                GetTypeToEnumerate(), flags, this->scriptContext)
                || (GetSnapShotSemantics() && newIndex >= initialPropertyCount))
            {TRACE_IT(66007);
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
    {TRACE_IT(66008);
        if (this->cachedData && this->initialType == this->object->GetDynamicType())
        {TRACE_IT(66009);
            return MoveAndGetNextWithCache(propertyId, attributes);
        }
        if (this->object)
        {TRACE_IT(66010);
            // Once enters NoCache path, ensure never switches to Cache path above.
            this->cachedData = nullptr;
            return MoveAndGetNextNoCache(propertyId, attributes);
        }
        return nullptr;
    }
}