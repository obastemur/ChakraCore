//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

#ifdef ENABLE_DOM_FAST_PATH
#include "Library\JavascriptTypedObjectSlotAccessorFunction.h"
namespace Js
{
    JavascriptTypedObjectSlotAccessorFunction::JavascriptTypedObjectSlotAccessorFunction(DynamicType* type, FunctionInfo* functionInfo, int allowedTypeId, PropertyId nameId) :
        RuntimeFunction(type, functionInfo),
        allowedTypeId(allowedTypeId)
    {TRACE_IT(62196);
        DebugOnly(VerifyEntryPoint());
        SetFunctionNameId(Js::TaggedInt::ToVarUnchecked(nameId));
    }


    bool JavascriptTypedObjectSlotAccessorFunction::Is(Var instance)
    {TRACE_IT(62197);
        if (VirtualTableInfo<Js::JavascriptTypedObjectSlotAccessorFunction>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::CrossSiteObject<Js::JavascriptTypedObjectSlotAccessorFunction>>::HasVirtualTable(instance) )
        {TRACE_IT(62198);
            return true;
        }
        return false;
    }


    void JavascriptTypedObjectSlotAccessorFunction::ValidateThisInstance(Js::Var thisObj)
    {TRACE_IT(62199);
        if (!InstanceOf(thisObj))
        {TRACE_IT(62200);
            Js::JavascriptError::ThrowTypeError(GetType()->GetScriptContext(), JSERR_FunctionArgument_NeedObject, _u("DOM object"));
        }
    }

    bool JavascriptTypedObjectSlotAccessorFunction::InstanceOf(Var thisObj)
    {TRACE_IT(62201);
        int allowedTypeId = GetAllowedTypeId();
        TypeId typeId = Js::JavascriptOperators::GetTypeId(thisObj);
        if (typeId == allowedTypeId)
        {TRACE_IT(62202);
            return true;
        }
        Type* type = RecyclableObject::FromVar(thisObj)->GetType();
        if (ExternalTypeWithInheritedTypeIds::Is(type))
        {TRACE_IT(62203);
            return ((Js::ExternalTypeWithInheritedTypeIds*)type)->InstanceOf();
        }
        return false;
    }

    JavascriptTypedObjectSlotAccessorFunction* JavascriptTypedObjectSlotAccessorFunction::FromVar(Var instance)
    {TRACE_IT(62204);
        Assert(Js::JavascriptTypedObjectSlotAccessorFunction::Is(instance));
        Assert((Js::JavascriptFunction::FromVar(instance)->GetFunctionInfo()->GetAttributes() & Js::FunctionBody::Attributes::NeedCrossSiteSecurityCheck) != 0);
        return static_cast<JavascriptTypedObjectSlotAccessorFunction*>(instance);
    }

    void JavascriptTypedObjectSlotAccessorFunction::ValidateThis(Js::JavascriptTypedObjectSlotAccessorFunction* func, Var thisObject)
    {TRACE_IT(62205);
        func->ValidateThisInstance(thisObject);
    }
}
#endif
