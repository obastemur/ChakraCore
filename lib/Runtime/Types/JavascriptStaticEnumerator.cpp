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
    {LOGMEIN("JavascriptStaticEnumerator.cpp] 12\n");
        this->prefixEnumerator = prefixEnumerator;
        this->arrayEnumerator = arrayToEnumerate ? arrayToEnumerate->GetIndexEnumerator(flags, requestContext) : nullptr;
        this->currentEnumerator = prefixEnumerator ? prefixEnumerator : PointerValue(arrayEnumerator);
        return this->propertyEnumerator.Initialize(objectToEnumerate, flags, requestContext, forInCache);
    }

    void JavascriptStaticEnumerator::Clear(EnumeratorFlags flags, ScriptContext * requestContext)
    {LOGMEIN("JavascriptStaticEnumerator.cpp] 20\n");
        this->prefixEnumerator = nullptr;
        this->arrayEnumerator = nullptr;
        this->currentEnumerator = nullptr;
        this->propertyEnumerator.Clear(flags, requestContext);
    }

    void JavascriptStaticEnumerator::Reset()
    {LOGMEIN("JavascriptStaticEnumerator.cpp] 28\n");
        if (this->prefixEnumerator)
        {LOGMEIN("JavascriptStaticEnumerator.cpp] 30\n");
            this->prefixEnumerator->Reset();
            this->currentEnumerator = this->prefixEnumerator;
            if (this->arrayEnumerator)
            {LOGMEIN("JavascriptStaticEnumerator.cpp] 34\n");
                this->arrayEnumerator->Reset();
            }
        }
        else if (this->arrayEnumerator)
        {LOGMEIN("JavascriptStaticEnumerator.cpp] 39\n");
            this->currentEnumerator = this->arrayEnumerator;
            this->arrayEnumerator->Reset();
        }
        this->propertyEnumerator.Reset();
    }

    bool JavascriptStaticEnumerator::IsNullEnumerator() const
    {LOGMEIN("JavascriptStaticEnumerator.cpp] 47\n");
        return this->prefixEnumerator == nullptr && this->arrayEnumerator == nullptr && this->propertyEnumerator.IsNullEnumerator();
    }

    bool JavascriptStaticEnumerator::CanUseJITFastPath() const
    {LOGMEIN("JavascriptStaticEnumerator.cpp] 52\n");
        return this->propertyEnumerator.CanUseJITFastPath() && this->currentEnumerator == nullptr;
    }

    uint32 JavascriptStaticEnumerator::GetCurrentItemIndex()
    {LOGMEIN("JavascriptStaticEnumerator.cpp] 57\n");
        if (currentEnumerator)
        {LOGMEIN("JavascriptStaticEnumerator.cpp] 59\n");
            return currentEnumerator->GetCurrentItemIndex();
        }
        else
        {
            return JavascriptArray::InvalidIndex;
        }
    }

    Var JavascriptStaticEnumerator::MoveAndGetNextFromEnumerator(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("JavascriptStaticEnumerator.cpp] 69\n");
        while (this->currentEnumerator)
        {LOGMEIN("JavascriptStaticEnumerator.cpp] 71\n");
            Var currentIndex = this->currentEnumerator->MoveAndGetNext(propertyId, attributes);
            if (currentIndex != nullptr)
            {LOGMEIN("JavascriptStaticEnumerator.cpp] 74\n");
                return currentIndex;
            }
            this->currentEnumerator = (this->currentEnumerator == this->prefixEnumerator) ? this->arrayEnumerator : nullptr;
        }

        return nullptr;
    }

    Var JavascriptStaticEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("JavascriptStaticEnumerator.cpp] 84\n");
        Var currentIndex = MoveAndGetNextFromEnumerator(propertyId, attributes);
        if (currentIndex == nullptr)
        {LOGMEIN("JavascriptStaticEnumerator.cpp] 87\n");
            currentIndex = propertyEnumerator.MoveAndGetNext(propertyId, attributes);
        }
        Assert(!currentIndex || !CrossSite::NeedMarshalVar(currentIndex, this->propertyEnumerator.GetScriptContext()));
        Assert(!currentIndex || JavascriptString::Is(currentIndex) || (this->propertyEnumerator.GetEnumSymbols() && JavascriptSymbol::Is(currentIndex)));
        return currentIndex;
    }
}
