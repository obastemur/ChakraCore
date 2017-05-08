//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    TypedArrayIndexEnumerator::TypedArrayIndexEnumerator(TypedArrayBase* typedArrayBase, EnumeratorFlags flags, ScriptContext* scriptContext) :
        JavascriptEnumerator(scriptContext),
        typedArrayObject(typedArrayBase),
        flags(flags)
    {TRACE_IT(64224);
        Reset();
    }

    Var TypedArrayIndexEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(64225);
        // TypedArrayIndexEnumerator follows the same logic in JavascriptArrayEnumerator,
        // but the implementation is slightly different as we don't have sparse array
        // in typed array, and typed array is a DynamicObject instead of JavascriptArray.
        propertyId = Constants::NoProperty;
        ScriptContext *scriptContext = this->GetScriptContext();

        if (!doneArray)
        {TRACE_IT(64226);
            while (true)
            {TRACE_IT(64227);
                uint32 lastIndex = index;
                index++;
                if ((uint32)index >= typedArrayObject->GetLength()) // End of array
                {TRACE_IT(64228);
                    index = lastIndex;
                    doneArray = true;
                    break;
                }

                if (attributes != nullptr)
                {TRACE_IT(64229);
                    *attributes = PropertyEnumerable;
                }

                return scriptContext->GetIntegerString(index);
            }
        }
        return nullptr;
    }

    void TypedArrayIndexEnumerator::Reset()
    {TRACE_IT(64230);
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
    }
}
