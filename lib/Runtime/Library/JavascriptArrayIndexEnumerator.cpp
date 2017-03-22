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
    {LOGMEIN("JavascriptArrayIndexEnumerator.cpp] 11\n");
#if ENABLE_COPYONACCESS_ARRAY
        JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray<Var>(arrayObject);
#endif
        Reset();
    }

    Var JavascriptArrayIndexEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("JavascriptArrayIndexEnumerator.cpp] 19\n");
        // TypedArrayIndexEnumerator follow the same logic but implementation is slightly
        // different as we don't have sparse array in typed array, and typed array
        // is DynamicObject instead of JavascriptArray.
        propertyId = Constants::NoProperty;

        if (!doneArray)
        {LOGMEIN("JavascriptArrayIndexEnumerator.cpp] 26\n");
            while (true)
            {LOGMEIN("JavascriptArrayIndexEnumerator.cpp] 28\n");
                uint32 lastIndex = index;
                index = arrayObject->GetNextIndex(index);
                if (index == JavascriptArray::InvalidIndex) // End of array
                {LOGMEIN("JavascriptArrayIndexEnumerator.cpp] 32\n");
                    index = lastIndex;
                    doneArray = true;
                    break;
                }

                if (attributes != nullptr)
                {LOGMEIN("JavascriptArrayIndexEnumerator.cpp] 39\n");
                    *attributes = PropertyEnumerable;
                }

                return this->GetScriptContext()->GetIntegerString(index);
            }
        }
        return nullptr;
    }

    void JavascriptArrayIndexEnumerator::Reset()
    {LOGMEIN("JavascriptArrayIndexEnumerator.cpp] 50\n");
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
    }
}
