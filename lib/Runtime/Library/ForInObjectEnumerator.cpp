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
    {LOGMEIN("ForInObjectEnumerator.cpp] 17\n");
    }

    ForInObjectEnumerator::ForInObjectEnumerator(RecyclableObject* object, ScriptContext * scriptContext, bool enumSymbols)
    {
        Initialize(object, scriptContext, enumSymbols);
    }

    void ForInObjectEnumerator::Clear()
    {LOGMEIN("ForInObjectEnumerator.cpp] 26\n");
        // Only clear stuff that are not useful for the next enumerator
        shadowData = nullptr;
    }

    void ForInObjectEnumerator::Initialize(RecyclableObject* initObject, ScriptContext * requestContext, bool enumSymbols, ForInCache * forInCache)
    {LOGMEIN("ForInObjectEnumerator.cpp] 32\n");
        this->enumeratingPrototype = false;

        if (initObject == nullptr)
        {LOGMEIN("ForInObjectEnumerator.cpp] 36\n");
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
        {LOGMEIN("ForInObjectEnumerator.cpp] 50\n");
            Recycler *recycler = requestContext->GetRecycler();
            this->shadowData = RecyclerNew(recycler, ShadowData, initObject, firstPrototype, recycler);
            flags = EnumeratorFlags::UseCache | EnumeratorFlags::SnapShotSemantics | EnumeratorFlags::EnumNonEnumerable | (enumSymbols ? EnumeratorFlags::EnumSymbols : EnumeratorFlags::None);
        }
        // no enumerable properties in the prototype chain, no need to search it
        else
        {
            this->shadowData = nullptr;
            flags = EnumeratorFlags::UseCache | EnumeratorFlags::SnapShotSemantics | (enumSymbols ? EnumeratorFlags::EnumSymbols : EnumeratorFlags::None);
        }

        if (InitializeCurrentEnumerator(initObject, flags, requestContext, forInCache))
        {LOGMEIN("ForInObjectEnumerator.cpp] 63\n");
            canUseJitFastPath = this->enumerator.CanUseJITFastPath();
        }
        else
        {
            // Nothing to enumerate.
            // We keep the shadowData so that it may walk up the prototype chain (e.g. primitive type)
            enumerator.Clear(flags, requestContext);
            canUseJitFastPath = false;
        }
    }

    RecyclableObject* ForInObjectEnumerator::GetFirstPrototypeWithEnumerableProperties(RecyclableObject* object, RecyclableObject** pFirstPrototype)
    {LOGMEIN("ForInObjectEnumerator.cpp] 76\n");
        RecyclableObject* firstPrototype = nullptr;
        RecyclableObject* firstPrototypeWithEnumerableProperties = nullptr;

        if (JavascriptOperators::GetTypeId(object) != TypeIds_HostDispatch)
        {LOGMEIN("ForInObjectEnumerator.cpp] 81\n");
            firstPrototypeWithEnumerableProperties = object;
            while (true)
            {LOGMEIN("ForInObjectEnumerator.cpp] 84\n");
                firstPrototypeWithEnumerableProperties = firstPrototypeWithEnumerableProperties->GetPrototype();

                if (firstPrototypeWithEnumerableProperties == nullptr)
                {LOGMEIN("ForInObjectEnumerator.cpp] 88\n");
                    break;
                }

                if (JavascriptOperators::GetTypeId(firstPrototypeWithEnumerableProperties) == TypeIds_Null)
                {LOGMEIN("ForInObjectEnumerator.cpp] 93\n");
                    firstPrototypeWithEnumerableProperties = nullptr;
                    break;
                }

                if (firstPrototype == nullptr)
                {LOGMEIN("ForInObjectEnumerator.cpp] 99\n");
                    firstPrototype = firstPrototypeWithEnumerableProperties;
                }

                if (!DynamicType::Is(firstPrototypeWithEnumerableProperties->GetTypeId())
                    || !DynamicObject::FromVar(firstPrototypeWithEnumerableProperties)->GetHasNoEnumerableProperties())
                {LOGMEIN("ForInObjectEnumerator.cpp] 105\n");
                    break;
                }
            }
        }

        if (pFirstPrototype != nullptr)
        {LOGMEIN("ForInObjectEnumerator.cpp] 112\n");
            *pFirstPrototype = firstPrototype;
        }

        return firstPrototypeWithEnumerableProperties;
    }

    BOOL ForInObjectEnumerator::InitializeCurrentEnumerator(RecyclableObject * object, ForInCache * forInCache)
    {LOGMEIN("ForInObjectEnumerator.cpp] 120\n");
        EnumeratorFlags flags = enumerator.GetFlags();
        RecyclableObject * prototype = object->GetPrototype();
        if (prototype == nullptr || prototype->GetTypeId() == TypeIds_Null)
        {LOGMEIN("ForInObjectEnumerator.cpp] 124\n");
            // If this is the last object on the prototype chain, we don't need to get the non-enumerable properties any more to track shadowing
            flags &= ~EnumeratorFlags::EnumNonEnumerable;
        }
        return InitializeCurrentEnumerator(object, flags, GetScriptContext(), forInCache);
    }

    BOOL ForInObjectEnumerator::InitializeCurrentEnumerator(RecyclableObject * object, EnumeratorFlags flags,  ScriptContext * scriptContext, ForInCache * forInCache)
    {LOGMEIN("ForInObjectEnumerator.cpp] 132\n");
        Assert(object);
        Assert(scriptContext);

        if (VirtualTableInfo<DynamicObject>::HasVirtualTable(object))
        {LOGMEIN("ForInObjectEnumerator.cpp] 137\n");
            DynamicObject* dynamicObject = (DynamicObject*)object;
            return dynamicObject->DynamicObject::GetEnumerator(&enumerator, flags, scriptContext, forInCache);
        }

        return object->GetEnumerator(&enumerator, flags, scriptContext, forInCache);
    }

    BOOL ForInObjectEnumerator::TestAndSetEnumerated(PropertyId propertyId)
    {LOGMEIN("ForInObjectEnumerator.cpp] 146\n");
        Assert(this->shadowData != nullptr);
        Assert(!Js::IsInternalPropertyId(propertyId));

        return !(this->shadowData->propertyIds.TestAndSet(propertyId));
    }

    Var ForInObjectEnumerator::MoveAndGetNext(PropertyId& propertyId)
    {LOGMEIN("ForInObjectEnumerator.cpp] 154\n");        
        PropertyRecord const * propRecord;
        PropertyAttributes attributes = PropertyNone;

        while (true)
        {LOGMEIN("ForInObjectEnumerator.cpp] 159\n");
            propertyId = Constants::NoProperty;
            Var currentIndex = enumerator.MoveAndGetNext(propertyId, &attributes);

            // The object type may have changed and we may not be able to use Jit fast path anymore.
            // canUseJitFastPath is determined in ForInObjectEnumerator::Initialize, once we decide we can't use
            // Jit fast path we will never go back to use fast path so && with current value  - if it's already
            // false we don't call CanUseJITFastPath()

            this->canUseJitFastPath = this->canUseJitFastPath && enumerator.CanUseJITFastPath();

            if (currentIndex)
            {LOGMEIN("ForInObjectEnumerator.cpp] 171\n");
                if (this->shadowData == nullptr)
                {LOGMEIN("ForInObjectEnumerator.cpp] 173\n");
                    // There are no prototype that has enumerable properties,
                    // don't need to keep track of the propertyIds we visited.

                    // We have asked for enumerable properties only, so don't need to check the attribute returned.
                    Assert(attributes & PropertyEnumerable);

                    return currentIndex;
                }

                // Property Id does not exist.
                if (propertyId == Constants::NoProperty)
                {LOGMEIN("ForInObjectEnumerator.cpp] 185\n");
                    if (!JavascriptString::Is(currentIndex)) //This can be undefined
                    {LOGMEIN("ForInObjectEnumerator.cpp] 187\n");
                        continue;
                    }
                    JavascriptString *pString = JavascriptString::FromVar(currentIndex);
                    if (VirtualTableInfo<Js::PropertyString>::HasVirtualTable(pString))
                    {LOGMEIN("ForInObjectEnumerator.cpp] 192\n");
                        // If we have a property string, it is assumed that the propertyId is being
                        // kept alive with the object
                        PropertyString * propertyString = (PropertyString *)pString;
                        propertyId = propertyString->GetPropertyRecord()->GetPropertyId();
                    }
                    else
                    {
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
                {LOGMEIN("ForInObjectEnumerator.cpp] 213\n");
                    return currentIndex;
                }
            }
            else
            {
                if (this->shadowData == nullptr)
                {LOGMEIN("ForInObjectEnumerator.cpp] 220\n");
                    Assert(!this->enumeratingPrototype);
                    return nullptr;
                }

                RecyclableObject * object;
                if (!this->enumeratingPrototype)
                {LOGMEIN("ForInObjectEnumerator.cpp] 227\n");  
                    this->enumeratingPrototype = true;
                    object = this->shadowData->firstPrototype;
                    this->shadowData->currentObject = object;
                }
                else
                {
                    //walk the prototype chain
                    object = this->shadowData->currentObject->GetPrototype();
                    this->shadowData->currentObject = object;
                    if ((object == nullptr) || (JavascriptOperators::GetTypeId(object) == TypeIds_Null))
                    {LOGMEIN("ForInObjectEnumerator.cpp] 238\n");
                        return nullptr;
                    }
                }

                do
                {LOGMEIN("ForInObjectEnumerator.cpp] 244\n");
                    if (!InitializeCurrentEnumerator(object))
                    {LOGMEIN("ForInObjectEnumerator.cpp] 246\n");
                        return nullptr;
                    }

                    if (!enumerator.IsNullEnumerator())
                    {LOGMEIN("ForInObjectEnumerator.cpp] 251\n");
                        break;
                    }

                     //walk the prototype chain
                    object = object->GetPrototype();
                    this->shadowData->currentObject = object;
                    if ((object == nullptr) || (JavascriptOperators::GetTypeId(object) == TypeIds_Null))
                    {LOGMEIN("ForInObjectEnumerator.cpp] 259\n");
                        return nullptr;
                    }
                }
                while (true);
            }
        }
    }
}
