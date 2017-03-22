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
    {LOGMEIN("JavascriptRegExpEnumerator.cpp] 11\n");
        index = (uint)-1;
    }

    void JavascriptRegExpEnumerator::Reset()
    {LOGMEIN("JavascriptRegExpEnumerator.cpp] 16\n");
        index = (uint)-1;
    }

    Var JavascriptRegExpEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("JavascriptRegExpEnumerator.cpp] 21\n");
        propertyId = Constants::NoProperty;
        ScriptContext* scriptContext = this->GetScriptContext();

        Var item;
        if (regExpObject->GetSpecialEnumerablePropertyName(++index, &item, scriptContext))
        {LOGMEIN("JavascriptRegExpEnumerator.cpp] 27\n");
            if (attributes != nullptr)
            {LOGMEIN("JavascriptRegExpEnumerator.cpp] 29\n");
                *attributes = PropertyEnumerable;
            }
            return item;
        }

        index = regExpObject->GetSpecialEnumerablePropertyCount();
        return nullptr;
    }
}
