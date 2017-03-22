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
    {LOGMEIN("JavascriptArrayIndexSnapshotEnumerator.cpp] 12\n");
        Reset();
    }

    Var JavascriptArrayIndexSnapshotEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("JavascriptArrayIndexSnapshotEnumerator.cpp] 17\n");
        propertyId = Constants::NoProperty;

        if (!doneArray)
        {LOGMEIN("JavascriptArrayIndexSnapshotEnumerator.cpp] 21\n");
            uint32 lastIndex = index;
            index = arrayObject->GetNextIndex(index);
            if (index >= initialLength) // End of array
            {LOGMEIN("JavascriptArrayIndexSnapshotEnumerator.cpp] 25\n");
                index = lastIndex;
                doneArray = true;
            }
            else
            {
                if (attributes != nullptr)
                {LOGMEIN("JavascriptArrayIndexSnapshotEnumerator.cpp] 32\n");
                    *attributes = PropertyEnumerable;
                }

                return this->GetScriptContext()->GetIntegerString(index);
            }
        }
        return nullptr;
    }

    void JavascriptArrayIndexSnapshotEnumerator::Reset()
    {LOGMEIN("JavascriptArrayIndexSnapshotEnumerator.cpp] 43\n");
        index = JavascriptArray::InvalidIndex;
        doneArray = false;
        initialLength = arrayObject->GetLength();
    }
}
