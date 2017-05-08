//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"
#include "Library/JavascriptSymbol.h"

namespace Js
{

    bool JavascriptStaticEnumerator::Initialize(JavascriptEnumerator * prefixEnumerator, ArrayObject * arrayToEnumerate,
        DynamicObject * objectToEnumerate, EnumeratorFlags flags, ScriptContext * requestContext, ForInCache * forInCache)
    {TRACE_IT(66343);
        this->prefixEnumerator = prefixEnumerator;
        this->arrayEnumerator = arrayToEnumerate ? arrayToEnumerate->GetIndexEnumerator(flags, requestContext) : nullptr;
        this->currentEnumerator = prefixEnumerator ? prefixEnumerator : PointerValue(arrayEnumerator);
        return this->propertyEnumerator.Initialize(objectToEnumerate, flags, requestContext, forInCache);
    }

    void JavascriptStaticEnumerator::Clear(EnumeratorFlags flags, ScriptContext * requestContext)
    {TRACE_IT(66344);
        this->prefixEnumerator = nullptr;
        this->arrayEnumerator = nullptr;
        this->currentEnumerator = nullptr;
        this->propertyEnumerator.Clear(flags, requestContext);
    }

    void JavascriptStaticEnumerator::Reset()
    {TRACE_IT(66345);
        if (this->prefixEnumerator)
        {TRACE_IT(66346);
            this->prefixEnumerator->Reset();
            this->currentEnumerator = this->prefixEnumerator;
            if (this->arrayEnumerator)
            {TRACE_IT(66347);
                this->arrayEnumerator->Reset();
            }
        }
        else if (this->arrayEnumerator)
        {TRACE_IT(66348);
            this->currentEnumerator = this->arrayEnumerator;
            this->arrayEnumerator->Reset();
        }
        this->propertyEnumerator.Reset();
    }

    bool JavascriptStaticEnumerator::IsNullEnumerator() const
    {TRACE_IT(66349);
        return this->prefixEnumerator == nullptr && this->arrayEnumerator == nullptr && this->propertyEnumerator.IsNullEnumerator();
    }

    bool JavascriptStaticEnumerator::CanUseJITFastPath() const
    {TRACE_IT(66350);
        return this->propertyEnumerator.CanUseJITFastPath() && this->currentEnumerator == nullptr;
    }

    uint32 JavascriptStaticEnumerator::GetCurrentItemIndex()
    {TRACE_IT(66351);
        if (currentEnumerator)
        {TRACE_IT(66352);
            return currentEnumerator->GetCurrentItemIndex();
        }
        else
        {TRACE_IT(66353);
            return JavascriptArray::InvalidIndex;
        }
    }

    Var JavascriptStaticEnumerator::MoveAndGetNextFromEnumerator(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(66354);
        while (this->currentEnumerator)
        {TRACE_IT(66355);
            Var currentIndex = this->currentEnumerator->MoveAndGetNext(propertyId, attributes);
            if (currentIndex != nullptr)
            {TRACE_IT(66356);
                return currentIndex;
            }
            this->currentEnumerator = (this->currentEnumerator == this->prefixEnumerator) ? this->arrayEnumerator : nullptr;
        }

        return nullptr;
    }

    Var JavascriptStaticEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(66357);
        Var currentIndex = MoveAndGetNextFromEnumerator(propertyId, attributes);
        if (currentIndex == nullptr)
        {TRACE_IT(66358);
            currentIndex = propertyEnumerator.MoveAndGetNext(propertyId, attributes);
        }
        Assert(!currentIndex || !CrossSite::NeedMarshalVar(currentIndex, this->propertyEnumerator.GetScriptContext()));
        Assert(!currentIndex || JavascriptString::Is(currentIndex) || (this->propertyEnumerator.GetEnumSymbols() && JavascriptSymbol::Is(currentIndex)));
        return currentIndex;
    }
}
