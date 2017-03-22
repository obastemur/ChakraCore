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
    {LOGMEIN("TypedArrayIndexEnumerator.cpp] 12\n");
        Reset();
    }

    Var TypedArrayIndexEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("TypedArrayIndexEnumerator.cpp] 17\n");
        // TypedArrayIndexEnumerator follows the same logic in JavascriptArrayEnumerator,
        // but the implementation is slightly different as we don't have sparse array
        // in typed array, and typed array is a DynamicObject instead of JavascriptArray.
        propertyId = Constants::NoProperty;
        ScriptContext *scriptContext = this->GetScriptContext();

        if (!doneArray)
        {LOGMEIN("TypedArrayIndexEnumerator.cpp] 25\n");
            while (true)
            {LOGMEIN("TypedArrayIndexEnumerator.cpp] 27\n");
                uint32 lastIndex = index;
                index++;
                if ((uint32)index >= typedArrayObject->GetLength()) // End of array
                {LOGMEIN("TypedArrayIndexEnumerator.cpp] 31\n");
                    index = lastIndex;
                    doneArray = true;
                    break;
                }

                if (attributes != nullptr)
                {LOGMEIN("TypedArrayIndexEnumerator.cpp] 38\n");
                    *attributes = PropertyEnumerable;
                }

                return scriptContext->GetIntegerString(index);
            }
        }
        return nullptr;
    }

    void TypedArrayIndexEnumerator::Reset()
    {LOGMEIN("TypedArrayIndexEnumerator.cpp] 49\n");
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
    }
}
