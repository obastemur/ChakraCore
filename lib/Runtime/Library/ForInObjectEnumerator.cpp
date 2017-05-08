//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#include "Library/ForInObjectEnumerator.h"

namespace Js
{
    ForInObjectEnumerator::ShadowData::ShadowData(
        RecyclableObject * initObject,
        RecyclableObject * firstPrototype,
        Recycler * recycler)
        : currentObject(initObject),
          firstPrototype(firstPrototype),
          propertyIds(recycler)
    {TRACE_IT(55257);
    }

    ForInObjectEnumerator::ForInObjectEnumerator(RecyclableObject* object, ScriptContext * scriptContext, bool enumSymbols)
    {
        Initialize(object, scriptContext, enumSymbols);
    }

    void ForInObjectEnumerator::Clear()
    {TRACE_IT(55258);
        // Only clear stuff that are not useful for the next enumerator
        shadowData = nullptr;
    }

    void ForInObjectEnumerator::Initialize(RecyclableObject* initObject, ScriptContext * requestContext, bool enumSymbols, ForInCache * forInCache)
    {TRACE_IT(55259);
        this->enumeratingPrototype = false;

        if (initObject == nullptr)
        {TRACE_IT(55260);
            enumerator.Clear(EnumeratorFlags::None, requestContext);
            this->shadowData = nullptr;
            this->canUseJitFastPath = false;
            return;
        }

        Assert(JavascriptOperators::GetTypeId(initObject) != TypeIds_Null
            && JavascriptOperators::GetTypeId(initObject) != TypeIds_Undefined);

        EnumeratorFlags flags;
        RecyclableObject * firstPrototype = nullptr;
        RecyclableObject * firstPrototypeWithEnumerableProperties = GetFirstPrototypeWithEnumerableProperties(initObject, &firstPrototype);
        if (firstPrototypeWithEnumerableProperties != nullptr)
        {TRACE_IT(55261);
            Recycler *recycler = requestContext->GetRecycler();
            this->shadowData = RecyclerNew(recycler, ShadowData, initObject, firstPrototype, recycler);
            flags = EnumeratorFlags::UseCache | EnumeratorFlags::SnapShotSemantics | EnumeratorFlags::EnumNonEnumerable | (enumSymbols ? EnumeratorFlags::EnumSymbols : EnumeratorFlags::None);
        }
        // no enumerable properties in the prototype chain, no need to search it
        else
        {TRACE_IT(55262);
            this->shadowData = nullptr;
            flags = EnumeratorFlags::UseCache | EnumeratorFlags::SnapShotSemantics | (enumSymbols ? EnumeratorFlags::EnumSymbols : EnumeratorFlags::None);
        }

        if (InitializeCurrentEnumerator(initObject, flags, requestContext, forInCache))
        {TRACE_IT(55263);
            canUseJitFastPath = this->enumerator.CanUseJITFastPath();
        }
        else
        {TRACE_IT(55264);
            // Nothing to enumerate.
            // We keep the shadowData so that it may walk up the prototype chain (e.g. primitive type)
            enumerator.Clear(flags, requestContext);
            canUseJitFastPath = false;
        }
    }

    RecyclableObject* ForInObjectEnumerator::GetFirstPrototypeWithEnumerableProperties(RecyclableObject* object, RecyclableObject** pFirstPrototype)
    {TRACE_IT(55265);
        RecyclableObject* firstPrototype = nullptr;
        RecyclableObject* firstPrototypeWithEnumerableProperties = nullptr;

        if (JavascriptOperators::GetTypeId(object) != TypeIds_HostDispatch)
        {TRACE_IT(55266);
            firstPrototypeWithEnumerableProperties = object;
            while (true)
            {TRACE_IT(55267);
                firstPrototypeWithEnumerableProperties = firstPrototypeWithEnumerableProperties->GetPrototype();

                if (firstPrototypeWithEnumerableProperties == nullptr)
                {TRACE_IT(55268);
                    break;
                }

                if (JavascriptOperators::GetTypeId(firstPrototypeWithEnumerableProperties) == TypeIds_Null)
                {TRACE_IT(55269);
                    firstPrototypeWithEnumerableProperties = nullptr;
                    break;
                }

                if (firstPrototype == nullptr)
                {TRACE_IT(55270);
                    firstPrototype = firstPrototypeWithEnumerableProperties;
                }

                if (!DynamicType::Is(firstPrototypeWithEnumerableProperties->GetTypeId())
                    || !DynamicObject::FromVar(firstPrototypeWithEnumerableProperties)->GetHasNoEnumerableProperties())
                {TRACE_IT(55271);
                    break;
                }
            }
        }

        if (pFirstPrototype != nullptr)
        {TRACE_IT(55272);
            *pFirstPrototype = firstPrototype;
        }

        return firstPrototypeWithEnumerableProperties;
    }

    BOOL ForInObjectEnumerator::InitializeCurrentEnumerator(RecyclableObject * object, ForInCache * forInCache)
    {TRACE_IT(55273);
        EnumeratorFlags flags = enumerator.GetFlags();
        RecyclableObject * prototype = object->GetPrototype();
        if (prototype == nullptr || prototype->GetTypeId() == TypeIds_Null)
        {TRACE_IT(55274);
            // If this is the last object on the prototype chain, we don't need to get the non-enumerable properties any more to track shadowing
            flags &= ~EnumeratorFlags::EnumNonEnumerable;
        }
        return InitializeCurrentEnumerator(object, flags, GetScriptContext(), forInCache);
    }

    BOOL ForInObjectEnumerator::InitializeCurrentEnumerator(RecyclableObject * object, EnumeratorFlags flags,  ScriptContext * scriptContext, ForInCache * forInCache)
    {TRACE_IT(55275);
        Assert(object);
        Assert(scriptContext);

        if (VirtualTableInfo<DynamicObject>::HasVirtualTable(object))
        {TRACE_IT(55276);
            DynamicObject* dynamicObject = (DynamicObject*)object;
            return dynamicObject->DynamicObject::GetEnumerator(&enumerator, flags, scriptContext, forInCache);
        }

        return object->GetEnumerator(&enumerator, flags, scriptContext, forInCache);
    }

    BOOL ForInObjectEnumerator::TestAndSetEnumerated(PropertyId propertyId)
    {TRACE_IT(55277);
        Assert(this->shadowData != nullptr);
        Assert(!Js::IsInternalPropertyId(propertyId));

        return !(this->shadowData->propertyIds.TestAndSet(propertyId));
    }

    Var ForInObjectEnumerator::MoveAndGetNext(PropertyId& propertyId)
    {TRACE_IT(55278);        
        PropertyRecord const * propRecord;
        PropertyAttributes attributes = PropertyNone;

        while (true)
        {TRACE_IT(55279);
            propertyId = Constants::NoProperty;
            Var currentIndex = enumerator.MoveAndGetNext(propertyId, &attributes);

            // The object type may have changed and we may not be able to use Jit fast path anymore.
            // canUseJitFastPath is determined in ForInObjectEnumerator::Initialize, once we decide we can't use
            // Jit fast path we will never go back to use fast path so && with current value  - if it's already
            // false we don't call CanUseJITFastPath()

            this->canUseJitFastPath = this->canUseJitFastPath && enumerator.CanUseJITFastPath();

            if (currentIndex)
            {TRACE_IT(55280);
                if (this->shadowData == nullptr)
                {TRACE_IT(55281);
                    // There are no prototype that has enumerable properties,
                    // don't need to keep track of the propertyIds we visited.

                    // We have asked for enumerable properties only, so don't need to check the attribute returned.
                    Assert(attributes & PropertyEnumerable);

                    return currentIndex;
                }

                // Property Id does not exist.
                if (propertyId == Constants::NoProperty)
                {TRACE_IT(55282);
                    if (!JavascriptString::Is(currentIndex)) //This can be undefined
                    {TRACE_IT(55283);
                        continue;
                    }
                    JavascriptString *pString = JavascriptString::FromVar(currentIndex);
                    if (VirtualTableInfo<Js::PropertyString>::HasVirtualTable(pString))
                    {TRACE_IT(55284);
                        // If we have a property string, it is assumed that the propertyId is being
                        // kept alive with the object
                        PropertyString * propertyString = (PropertyString *)pString;
                        propertyId = propertyString->GetPropertyRecord()->GetPropertyId();
                    }
                    else
                    {TRACE_IT(55285);
                        ScriptContext* scriptContext = pString->GetScriptContext();
                        scriptContext->GetOrAddPropertyRecord(pString->GetString(), pString->GetLength(), &propRecord);
                        propertyId = propRecord->GetPropertyId();

                        // We keep the track of what is enumerated using a bit vector of propertyID.
                        // so the propertyId can't be collected until the end of the for in enumerator
                        // Keep a list of the property string.
                        this->shadowData->newPropertyStrings.Prepend(GetScriptContext()->GetRecycler(), propRecord);
                    }
                }

                if (TestAndSetEnumerated(propertyId) //checks if the property is already enumerated or not
                    && (attributes & PropertyEnumerable))
                {TRACE_IT(55286);
                    return currentIndex;
                }
            }
            else
            {TRACE_IT(55287);
                if (this->shadowData == nullptr)
                {TRACE_IT(55288);
                    Assert(!this->enumeratingPrototype);
                    return nullptr;
                }

                RecyclableObject * object;
                if (!this->enumeratingPrototype)
                {TRACE_IT(55289);  
                    this->enumeratingPrototype = true;
                    object = this->shadowData->firstPrototype;
                    this->shadowData->currentObject = object;
                }
                else
                {TRACE_IT(55290);
                    //walk the prototype chain
                    object = this->shadowData->currentObject->GetPrototype();
                    this->shadowData->currentObject = object;
                    if ((object == nullptr) || (JavascriptOperators::GetTypeId(object) == TypeIds_Null))
                    {TRACE_IT(55291);
                        return nullptr;
                    }
                }

                do
                {TRACE_IT(55292);
                    if (!InitializeCurrentEnumerator(object))
                    {TRACE_IT(55293);
                        return nullptr;
                    }

                    if (!enumerator.IsNullEnumerator())
                    {TRACE_IT(55294);
                        break;
                    }

                     //walk the prototype chain
                    object = object->GetPrototype();
                    this->shadowData->currentObject = object;
                    if ((object == nullptr) || (JavascriptOperators::GetTypeId(object) == TypeIds_Null))
                    {TRACE_IT(55295);
                        return nullptr;
                    }
                }
                while (true);
            }
        }
    }
}
