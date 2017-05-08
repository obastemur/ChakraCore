//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    bool WithScopeObject::Is(Var aValue)
    {TRACE_IT(68063);
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_WithScopeObject;
    }
    WithScopeObject* WithScopeObject::FromVar(Var aValue)
    {TRACE_IT(68064);
        Assert(WithScopeObject::Is(aValue));
        return static_cast<WithScopeObject*>(aValue);
    }

    BOOL WithScopeObject::HasProperty(PropertyId propertyId)
    {TRACE_IT(68065);
        return JavascriptOperators::HasPropertyUnscopables(wrappedObject, propertyId);
    }

    BOOL WithScopeObject::HasOwnProperty(PropertyId propertyId)
    {TRACE_IT(68066);
        Assert(!Js::IsInternalPropertyId(propertyId));
        return HasProperty(propertyId);
    }

    BOOL WithScopeObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {TRACE_IT(68067);
        return JavascriptOperators::SetPropertyUnscopable(wrappedObject, wrappedObject, propertyId, value, info, wrappedObject->GetScriptContext());
    }

    BOOL WithScopeObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(68068);
        return JavascriptOperators::GetPropertyUnscopable(wrappedObject, wrappedObject, propertyId, value, requestContext, info);
    }


    BOOL WithScopeObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {TRACE_IT(68069);
        return JavascriptOperators::DeletePropertyUnscopables(wrappedObject, propertyId, flags);
    }

    DescriptorFlags WithScopeObject::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(68070);
        return JavascriptOperators::GetterSetterUnscopable(wrappedObject, propertyId, setterValue, info, requestContext);
    }

    BOOL WithScopeObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {TRACE_IT(68071);
        RecyclableObject* copyState = wrappedObject;
        return JavascriptOperators::PropertyReferenceWalkUnscopable(wrappedObject, &copyState, propertyId, value, info, requestContext);
    }

} // namespace Js
