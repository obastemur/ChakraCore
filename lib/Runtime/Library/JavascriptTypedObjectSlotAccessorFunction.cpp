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
    {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 13\n");
        DebugOnly(VerifyEntryPoint());
        SetFunctionNameId(Js::TaggedInt::ToVarUnchecked(nameId));
    }


    bool JavascriptTypedObjectSlotAccessorFunction::Is(Var instance)
    {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 20\n");
        if (VirtualTableInfo<Js::JavascriptTypedObjectSlotAccessorFunction>::HasVirtualTable(instance) ||
            VirtualTableInfo<Js::CrossSiteObject<Js::JavascriptTypedObjectSlotAccessorFunction>>::HasVirtualTable(instance) )
        {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 23\n");
            return true;
        }
        return false;
    }


    void JavascriptTypedObjectSlotAccessorFunction::ValidateThisInstance(Js::Var thisObj)
    {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 31\n");
        if (!InstanceOf(thisObj))
        {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 33\n");
            Js::JavascriptError::ThrowTypeError(GetType()->GetScriptContext(), JSERR_FunctionArgument_NeedObject, _u("DOM object"));
        }
    }

    bool JavascriptTypedObjectSlotAccessorFunction::InstanceOf(Var thisObj)
    {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 39\n");
        int allowedTypeId = GetAllowedTypeId();
        TypeId typeId = Js::JavascriptOperators::GetTypeId(thisObj);
        if (typeId == allowedTypeId)
        {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 43\n");
            return true;
        }
        Type* type = RecyclableObject::FromVar(thisObj)->GetType();
        if (ExternalTypeWithInheritedTypeIds::Is(type))
        {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 48\n");
            return ((Js::ExternalTypeWithInheritedTypeIds*)type)->InstanceOf();
        }
        return false;
    }

    JavascriptTypedObjectSlotAccessorFunction* JavascriptTypedObjectSlotAccessorFunction::FromVar(Var instance)
    {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 55\n");
        Assert(Js::JavascriptTypedObjectSlotAccessorFunction::Is(instance));
        Assert((Js::JavascriptFunction::FromVar(instance)->GetFunctionInfo()->GetAttributes() & Js::FunctionBody::Attributes::NeedCrossSiteSecurityCheck) != 0);
        return static_cast<JavascriptTypedObjectSlotAccessorFunction*>(instance);
    }

    void JavascriptTypedObjectSlotAccessorFunction::ValidateThis(Js::JavascriptTypedObjectSlotAccessorFunction* func, Var thisObject)
    {LOGMEIN("JavascriptTypedObjectSlotAccessorFunction.cpp] 62\n");
        func->ValidateThisInstance(thisObject);
    }
}
#endif
