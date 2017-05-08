//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptArrayIndexEnumerator::JavascriptArrayIndexEnumerator(
        JavascriptArray* arrayObject, EnumeratorFlags flags, ScriptContext* scriptContext) :
        JavascriptArrayIndexEnumeratorBase(arrayObject, flags, scriptContext)
    {TRACE_IT(58150);
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayObject);
#endif
        Reset();
    }

    Var JavascriptArrayIndexEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(58151);
        // TypedArrayIndexEnumerator follow the same logic but implementation is slightly
        // different as we don't have sparse array in typed array, and typed array
        // is DynamicObject instead of JavascriptArray.
        propertyId = Constants::NoProperty;

        if (!doneArray)
        {TRACE_IT(58152);
            while (true)
            {TRACE_IT(58153);
                uint32 lastIndex = index;
                index = arrayObject->GetNextIndex(index);
                if (index == JavascriptArray::InvalidIndex) // End of array
                {TRACE_IT(58154);
                    index = lastIndex;
                    doneArray = true;
                    break;
                }

                if (attributes != nullptr)
                {TRACE_IT(58155);
                    *attributes = PropertyEnumerable;
                }

                return this->GetScriptContext()->GetIntegerString(index);
            }
        }
        return nullptr;
    }

    void JavascriptArrayIndexEnumerator::Reset()
    {TRACE_IT(58156);
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
    }
}
