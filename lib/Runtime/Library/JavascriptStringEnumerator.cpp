//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptStringEnumerator::JavascriptStringEnumerator(JavascriptString* stringObject, ScriptContext * requestContext) :
        JavascriptEnumerator(requestContext),
        stringObject(stringObject),
        index(-1)
    {LOGMEIN("JavascriptStringEnumerator.cpp] 12\n");
    }

    void JavascriptStringEnumerator::Reset()
    {LOGMEIN("JavascriptStringEnumerator.cpp] 16\n");
        index = -1;
    }


    Var JavascriptStringEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("JavascriptStringEnumerator.cpp] 22\n");
        propertyId = Constants::NoProperty;
        if (++index < stringObject->GetLengthAsSignedInt())
        {LOGMEIN("JavascriptStringEnumerator.cpp] 25\n");
            if (attributes != nullptr)
            {LOGMEIN("JavascriptStringEnumerator.cpp] 27\n");
                *attributes = PropertyEnumerable;
            }

            return this->GetScriptContext()->GetIntegerString(index);
        }
        else
        {
            index = stringObject->GetLength();
            return nullptr;
        }
    }
}
