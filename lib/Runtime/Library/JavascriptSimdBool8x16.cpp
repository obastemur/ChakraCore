//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


#include "RuntimeLibraryPch.h"


namespace Js
{
    const char16 JavascriptSIMDBool8x16::TypeName[] = _u("SIMD.Bool8x16");

    JavascriptSIMDBool8x16::JavascriptSIMDBool8x16(StaticType *type) : JavascriptSIMDType(type)
    {TRACE_IT(61424);
        Assert(type->GetTypeId() == TypeIds_SIMDBool8x16);
    }

    JavascriptSIMDBool8x16::JavascriptSIMDBool8x16(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {TRACE_IT(61425);
        Assert(type->GetTypeId() == TypeIds_SIMDBool8x16);
    }

    JavascriptSIMDBool8x16* JavascriptSIMDBool8x16::AllocUninitialized(ScriptContext* requestContext)
    {TRACE_IT(61426);
        return (JavascriptSIMDBool8x16 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool8x16, requestContext->GetLibrary()->GetSIMDBool8x16TypeStatic());
    }

    JavascriptSIMDBool8x16* JavascriptSIMDBool8x16::New(SIMDValue *val, ScriptContext* requestContext)
    {TRACE_IT(61427);
        return (JavascriptSIMDBool8x16 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool8x16, val, requestContext->GetLibrary()->GetSIMDBool8x16TypeStatic());
    }

    bool  JavascriptSIMDBool8x16::Is(Var instance)
    {TRACE_IT(61428);
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDBool8x16;
    }

    JavascriptSIMDBool8x16* JavascriptSIMDBool8x16::FromVar(Var aValue)
    {TRACE_IT(61429);
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDBool8x16'");

        return reinterpret_cast<JavascriptSIMDBool8x16 *>(aValue);
    }

    RecyclableObject * JavascriptSIMDBool8x16::CloneToScriptContext(ScriptContext* requestContext)
    {TRACE_IT(61430);
        return JavascriptSIMDBool8x16::New(&value, requestContext);
    }

    const char16* JavascriptSIMDBool8x16::GetTypeName()
    {TRACE_IT(61431);
        return JavascriptSIMDBool8x16::TypeName;
    }

    Var JavascriptSIMDBool8x16::Copy(ScriptContext* requestContext)
    {TRACE_IT(61432);
        return JavascriptSIMDBool8x16::New(&this->value, requestContext);
    }
}
