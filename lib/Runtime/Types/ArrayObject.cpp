//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
// Implementation for typed arrays based on ArrayBuffer.
// There is one nested ArrayBuffer for each typed array. Multiple typed array
// can share the same array buffer.
//----------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    ArrayObject::ArrayObject(ArrayObject * instance)
        : DynamicObject(instance),
        length(instance->length)
    {LOGMEIN("ArrayObject.cpp] 15\n");
    }

    void ArrayObject::ThrowItemNotConfigurableError(PropertyId propId /*= Constants::NoProperty*/)
    {LOGMEIN("ArrayObject.cpp] 19\n");
        ScriptContext* scriptContext = GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
            propId != Constants::NoProperty ?
                scriptContext->GetThreadContext()->GetPropertyName(propId)->GetBuffer() : nullptr);
    }

    void ArrayObject::VerifySetItemAttributes(PropertyId propId, PropertyAttributes attributes)
    {LOGMEIN("ArrayObject.cpp] 27\n");
        if (attributes != (PropertyEnumerable | PropertyWritable))
        {LOGMEIN("ArrayObject.cpp] 29\n");
            ThrowItemNotConfigurableError(propId);
        }
    }

    BOOL ArrayObject::SetItemAttributes(uint32 index, PropertyAttributes attributes)
    {LOGMEIN("ArrayObject.cpp] 35\n");
        VerifySetItemAttributes(Constants::NoProperty, attributes);
        return TRUE;
    }

    BOOL ArrayObject::SetItemAccessors(uint32 index, Var getter, Var setter)
    {LOGMEIN("ArrayObject.cpp] 41\n");
        ThrowItemNotConfigurableError();
    }

    BOOL ArrayObject::IsObjectArrayFrozen()
    {LOGMEIN("ArrayObject.cpp] 46\n");
        return this->IsFrozen();
    }
} // namespace Js
