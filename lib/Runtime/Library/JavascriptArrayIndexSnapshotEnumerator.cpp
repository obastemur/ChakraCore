//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    JavascriptArrayIndexSnapshotEnumerator::JavascriptArrayIndexSnapshotEnumerator(
        JavascriptArray* arrayObject, EnumeratorFlags flags, ScriptContext* scriptContext) :
        JavascriptArrayIndexEnumeratorBase(arrayObject, flags, scriptContext),
        initialLength(arrayObject->GetLength())
    {TRACE_IT(58158);
        Reset();
    }

    Var JavascriptArrayIndexSnapshotEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(58159);
        propertyId = Constants::NoProperty;

        if (!doneArray)
        {TRACE_IT(58160);
            uint32 lastIndex = index;
            index = arrayObject->GetNextIndex(index);
            if (index >= initialLength) // End of array
            {TRACE_IT(58161);
                index = lastIndex;
                doneArray = true;
            }
            else
            {TRACE_IT(58162);
                if (attributes != nullptr)
                {TRACE_IT(58163);
                    *attributes = PropertyEnumerable;
                }

                return this->GetScriptContext()->GetIntegerString(index);
            }
        }
        return nullptr;
    }

    void JavascriptArrayIndexSnapshotEnumerator::Reset()
    {TRACE_IT(58164);
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
        initialLength = arrayObject->GetLength();
    }
}
