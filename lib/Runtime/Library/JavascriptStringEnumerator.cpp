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
    {TRACE_IT(62050);
    }

    void JavascriptStringEnumerator::Reset()
    {TRACE_IT(62051);
        index = -1;
    }


    Var JavascriptStringEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(62052);
        propertyId = Constants::NoProperty;
        if (++index < stringObject->GetLengthAsSignedInt())
        {TRACE_IT(62053);
            if (attributes != nullptr)
            {TRACE_IT(62054);
                *attributes = PropertyEnumerable;
            }

            return this->GetScriptContext()->GetIntegerString(index);
        }
        else
        {TRACE_IT(62055);
            index = stringObject->GetLength();
            return nullptr;
        }
    }
}
