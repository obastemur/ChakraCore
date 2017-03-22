//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    IteratorObjectEnumerator * IteratorObjectEnumerator::Create(ScriptContext* scriptContext, Var iterator)
    {LOGMEIN("IteratorObjectEnumerator.cpp] 9\n");
        return RecyclerNew(scriptContext->GetRecycler(), IteratorObjectEnumerator, scriptContext, iterator);
    }

    IteratorObjectEnumerator::IteratorObjectEnumerator(ScriptContext* scriptContext, Var iterator) :
        JavascriptEnumerator(scriptContext),
        done(false),
        value(nullptr)
    {LOGMEIN("IteratorObjectEnumerator.cpp] 17\n");
        Assert(JavascriptOperators::IsObject(iterator));
        iteratorObject = RecyclableObject::FromVar(iterator);
    }

    Var IteratorObjectEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("IteratorObjectEnumerator.cpp] 23\n");
        ScriptContext* scriptContext = GetScriptContext();
        Var resultValue = nullptr;
        if (JavascriptOperators::IteratorStepAndValue(iteratorObject, scriptContext, &resultValue))
        {LOGMEIN("IteratorObjectEnumerator.cpp] 27\n");
            this->value = resultValue;
            if (attributes != nullptr)
            {LOGMEIN("IteratorObjectEnumerator.cpp] 30\n");
                *attributes = PropertyEnumerable;
            }

            Var currentIndex = value;
            const PropertyRecord* propertyRecord = nullptr;
            if (!TaggedInt::Is(currentIndex) && JavascriptString::Is(currentIndex) &&
                VirtualTableInfo<Js::PropertyString>::HasVirtualTable(JavascriptString::FromVar(currentIndex)))
            {LOGMEIN("IteratorObjectEnumerator.cpp] 38\n");
                propertyRecord = ((PropertyString *)PropertyString::FromVar(currentIndex))->GetPropertyRecord();
            }
            else if (JavascriptSymbol::Is(currentIndex))
            {LOGMEIN("IteratorObjectEnumerator.cpp] 42\n");
                propertyRecord = JavascriptSymbol::FromVar(currentIndex)->GetValue();
            }
            else
            {
                JavascriptString* propertyName = JavascriptConversion::ToString(currentIndex, scriptContext);
                GetScriptContext()->GetOrAddPropertyRecord(propertyName->GetString(), propertyName->GetLength(), &propertyRecord);

                // Need to keep property records alive during enumeration to prevent collection
                // and eventual reuse during the same enumeration. For DynamicObjects, property
                // records are kept alive by type handlers.
                this->propertyRecords.Prepend(iteratorObject->GetRecycler(), propertyRecord);
            }

            propertyId = propertyRecord->GetPropertyId();
            return currentIndex;
        }
        return NULL;
    }

    void IteratorObjectEnumerator::Reset()
    {LOGMEIN("IteratorObjectEnumerator.cpp] 63\n");
        Assert(FALSE);
    }
};
