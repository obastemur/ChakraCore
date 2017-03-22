//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeTypePch.h"

namespace Js
{
    bool WithScopeObject::Is(Var aValue)
    {LOGMEIN("WithScopeObject.cpp] 9\n");
        return JavascriptOperators::GetTypeId(aValue) == TypeIds_WithScopeObject;
    }
    WithScopeObject* WithScopeObject::FromVar(Var aValue)
    {LOGMEIN("WithScopeObject.cpp] 13\n");
        Assert(WithScopeObject::Is(aValue));
        return static_cast<WithScopeObject*>(aValue);
    }

    BOOL WithScopeObject::HasProperty(PropertyId propertyId)
    {LOGMEIN("WithScopeObject.cpp] 19\n");
        return JavascriptOperators::HasPropertyUnscopables(wrappedObject, propertyId);
    }

    BOOL WithScopeObject::HasOwnProperty(PropertyId propertyId)
    {LOGMEIN("WithScopeObject.cpp] 24\n");
        Assert(!Js::IsInternalPropertyId(propertyId));
        return HasProperty(propertyId);
    }

    BOOL WithScopeObject::SetProperty(PropertyId propertyId, Var value, PropertyOperationFlags flags, PropertyValueInfo* info)
    {LOGMEIN("WithScopeObject.cpp] 30\n");
        return JavascriptOperators::SetPropertyUnscopable(wrappedObject, wrappedObject, propertyId, value, info, wrappedObject->GetScriptContext());
    }

    BOOL WithScopeObject::GetProperty(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("WithScopeObject.cpp] 35\n");
        return JavascriptOperators::GetPropertyUnscopable(wrappedObject, wrappedObject, propertyId, value, requestContext, info);
    }


    BOOL WithScopeObject::DeleteProperty(PropertyId propertyId, PropertyOperationFlags flags)
    {LOGMEIN("WithScopeObject.cpp] 41\n");
        return JavascriptOperators::DeletePropertyUnscopables(wrappedObject, propertyId, flags);
    }

    DescriptorFlags WithScopeObject::GetSetter(PropertyId propertyId, Var *setterValue, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("WithScopeObject.cpp] 46\n");
        return JavascriptOperators::GetterSetterUnscopable(wrappedObject, propertyId, setterValue, info, requestContext);
    }

    BOOL WithScopeObject::GetPropertyReference(Var originalInstance, PropertyId propertyId, Var* value, PropertyValueInfo* info, ScriptContext* requestContext)
    {LOGMEIN("WithScopeObject.cpp] 51\n");
        RecyclableObject* copyState = wrappedObject;
        return JavascriptOperators::PropertyReferenceWalkUnscopable(wrappedObject, &copyState, propertyId, value, info, requestContext);
    }

} // namespace Js
