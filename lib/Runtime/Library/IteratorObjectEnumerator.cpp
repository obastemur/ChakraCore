//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    IteratorObjectEnumerator * IteratorObjectEnumerator::Create(ScriptContext* scriptContext, Var iterator)
    {TRACE_IT(55686);
        return RecyclerNew(scriptContext->GetRecycler(), IteratorObjectEnumerator, scriptContext, iterator);
    }

    IteratorObjectEnumerator::IteratorObjectEnumerator(ScriptContext* scriptContext, Var iterator) :
        JavascriptEnumerator(scriptContext),
        done(false),
        value(nullptr)
    {TRACE_IT(55687);
        Assert(JavascriptOperators::IsObject(iterator));
        iteratorObject = RecyclableObject::FromVar(iterator);
    }

    Var IteratorObjectEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(55688);
        ScriptContext* scriptContext = GetScriptContext();
        Var resultValue = nullptr;
        if (JavascriptOperators::IteratorStepAndValue(iteratorObject, scriptContext, &resultValue))
        {TRACE_IT(55689);
            this->value = resultValue;
            if (attributes != nullptr)
            {TRACE_IT(55690);
                *attributes = PropertyEnumerable;
            }

            Var currentIndex = value;
            const PropertyRecord* propertyRecord = nullptr;
            if (!TaggedInt::Is(currentIndex) && JavascriptString::Is(currentIndex) &&
                VirtualTableInfo<Js::PropertyString>::HasVirtualTable(JavascriptString::FromVar(currentIndex)))
            {TRACE_IT(55691);
                propertyRecord = ((PropertyString *)PropertyString::FromVar(currentIndex))->GetPropertyRecord();
            }
            else if (JavascriptSymbol::Is(currentIndex))
            {TRACE_IT(55692);
                propertyRecord = JavascriptSymbol::FromVar(currentIndex)->GetValue();
            }
            else
            {TRACE_IT(55693);
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
    {TRACE_IT(55694);
        Assert(FALSE);
    }
};
