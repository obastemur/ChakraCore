//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    ES5ArrayIndexEnumerator::ES5ArrayIndexEnumerator(ES5Array* arrayObject, EnumeratorFlags flags, ScriptContext* scriptContext)
        : JavascriptArrayIndexEnumeratorBase(arrayObject, flags, scriptContext)
    {TRACE_IT(55194);
        Reset();
    }

    Var ES5ArrayIndexEnumerator::MoveAndGetNext(PropertyId& propertyId, PropertyAttributes* attributes)
    {TRACE_IT(55195);
        propertyId = Constants::NoProperty;

        if (!doneArray)
        {TRACE_IT(55196);
            while (true)
            {TRACE_IT(55197);
                if (index == dataIndex)
                {TRACE_IT(55198);
                    dataIndex = arrayObject->GetNextIndex(dataIndex);
                }
                if (index == descriptorIndex || !GetArray()->IsValidDescriptorToken(descriptorValidationToken))
                {TRACE_IT(55199);
                    Js::IndexPropertyDescriptor * pResultDescriptor = nullptr;
                    void* tmpDescriptorValidationToken = nullptr;
                    descriptorIndex = GetArray()->GetNextDescriptor(
                        index, &pResultDescriptor, &tmpDescriptorValidationToken);
                    this->descriptor = pResultDescriptor;
                    this->descriptorValidationToken = tmpDescriptorValidationToken;
                }

                index = min(dataIndex, descriptorIndex);
                if (index >= initialLength) // End of array
                {TRACE_IT(55200);
                    doneArray = true;
                    break;
                }

                if (!!(flags & EnumeratorFlags::EnumNonEnumerable)
                    || index < descriptorIndex
                    || (descriptor->Attributes & PropertyEnumerable))
                {TRACE_IT(55201);
                    if (attributes != nullptr)
                    {TRACE_IT(55202);
                        if (index < descriptorIndex)
                        {TRACE_IT(55203);
                            *attributes = PropertyEnumerable;
                        }
                        else
                        {TRACE_IT(55204);
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
    {TRACE_IT(55205);
        initialLength = arrayObject->GetLength();
        dataIndex = JavascriptArray::InvalidIndex;
        descriptorIndex = JavascriptArray::InvalidIndex;
        descriptor = nullptr;
        descriptorValidationToken = nullptr;

        index = JavascriptArray::InvalidIndex;
        doneArray = false;
    }
}
