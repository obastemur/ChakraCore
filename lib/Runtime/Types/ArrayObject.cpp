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
    {TRACE_IT(65305);
    }

    void ArrayObject::ThrowItemNotConfigurableError(PropertyId propId /*= Constants::NoProperty*/)
    {TRACE_IT(65306);
        ScriptContext* scriptContext = GetScriptContext();
        JavascriptError::ThrowTypeError(scriptContext, JSERR_DefineProperty_NotConfigurable,
            propId != Constants::NoProperty ?
                scriptContext->GetThreadContext()->GetPropertyName(propId)->GetBuffer() : nullptr);
    }

    void ArrayObject::VerifySetItemAttributes(PropertyId propId, PropertyAttributes attributes)
    {TRACE_IT(65307);
        if (attributes != (PropertyEnumerable | PropertyWritable))
        {TRACE_IT(65308);
            ThrowItemNotConfigurableError(propId);
        }
    }

    BOOL ArrayObject::SetItemAttributes(uint32 index, PropertyAttributes attributes)
    {TRACE_IT(65309);
        VerifySetItemAttributes(Constants::NoProperty, attributes);
        return TRUE;
    }

    BOOL ArrayObject::SetItemAccessors(uint32 index, Var getter, Var setter)
    {TRACE_IT(65310);
        ThrowItemNotConfigurableError();
    }

    BOOL ArrayObject::IsObjectArrayFrozen()
    {TRACE_IT(65311);
        return this->IsFrozen();
    }
} // namespace Js
