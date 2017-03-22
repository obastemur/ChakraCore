//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ES5ArrayIndexEnumerator::ES5ArrayIndexEnumerator(ES5Array* arrayObject, EnumeratorFlags flags, ScriptContext* scriptContext)
        : JavascriptArrayIndexEnumeratorBase(arrayObject, flags, scriptContext)
    {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 10\n");
        Reset();
    }

    Var ES5ArrayIndexEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 15\n");
        propertyId = Constants::NoProperty;

        if (!doneArray)
        {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 19\n");
            while (true)
            {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 21\n");
                if (index == dataIndex)
                {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 23\n");
                    dataIndex = arrayObject->GetNextIndex(dataIndex);
                }
                if (index == descriptorIndex || !GetArray()->IsValidDescriptorToken(descriptorValidationToken))
                {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 27\n");
                    Js::IndexPropertyDescriptor * pResultDescriptor = nullptr;
                    void* tmpDescriptorValidationToken = nullptr;
                    descriptorIndex = GetArray()->GetNextDescriptor(
                        index, &pResultDescriptor, &tmpDescriptorValidationToken);
                    this->descriptor = pResultDescriptor;
                    this->descriptorValidationToken = tmpDescriptorValidationToken;
                }

                index = min(dataIndex, descriptorIndex);
                if (index >= initialLength) // End of array
                {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 38\n");
                    doneArray = true;
                    break;
                }

                if (!!(flags & EnumeratorFlags::EnumNonEnumerable)
                    || index < descriptorIndex
                    || (descriptor->Attributes & PropertyEnumerable))
                {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 46\n");
                    if (attributes != nullptr)
                    {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 48\n");
                        if (index < descriptorIndex)
                        {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 50\n");
                            *attributes = PropertyEnumerable;
                        }
                        else
                        {
                            *attributes = descriptor->Attributes;
                        }
                    }

                    return this->GetScriptContext()->GetIntegerString(index);
                }
            }
        }
        return nullptr;
    }

    void ES5ArrayIndexEnumerator::Reset()
    {LOGMEIN("ES5ArrayIndexEnumerator.cpp] 67\n");
        initialLength = arrayObject->GetLength();
        dataIndex = JavascriptArray::InvalidIndex;
        descriptorIndex = JavascriptArray::InvalidIndex;
        descriptor = nullptr;
        descriptorValidationToken = nullptr;

        index = JavascriptArray::InvalidIndex;
        doneArray = false;
    }
}
