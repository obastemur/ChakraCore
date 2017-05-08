//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"
namespace Js
{
    JavascriptRegExpEnumerator::JavascriptRegExpEnumerator(JavascriptRegExpConstructor* regExpObject, EnumeratorFlags flags, ScriptContext * requestContext) :
        JavascriptEnumerator(requestContext),
        flags(flags),
        regExpObject(regExpObject)        
    {TRACE_IT(61168);
        index = (uint)-1;
    }

    void JavascriptRegExpEnumerator::Reset()
    {TRACE_IT(61169);
        index = (uint)-1;
    }

    Var JavascriptRegExpEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(61170);
        propertyId = Constants::NoProperty;
        ScriptContext* scriptContext = this->GetScriptContext();

        Var item;
        if (regExpObject->GetSpecialEnumerablePropertyName(++index, &item, scriptContext))
        {TRACE_IT(61171);
            if (attributes != nullptr)
            {TRACE_IT(61172);
                *attributes = PropertyEnumerable;
            }
            return item;
        }

        index = regExpObject->GetSpecialEnumerablePropertyCount();
        return nullptr;
    }
}
