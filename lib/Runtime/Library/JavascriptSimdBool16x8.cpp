//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDBool16x8::TypeName[] = _u("SIMD.Bool16x8");

    JavascriptSIMDBool16x8::JavascriptSIMDBool16x8(StaticType *type) : JavascriptSIMDType(type)
    {TRACE_IT(61400);
        Assert(type->GetTypeId() == TypeIds_SIMDBool16x8);
    }

    JavascriptSIMDBool16x8::JavascriptSIMDBool16x8(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {TRACE_IT(61401);
        Assert(type->GetTypeId() == TypeIds_SIMDBool16x8);
    }

    JavascriptSIMDBool16x8* JavascriptSIMDBool16x8::AllocUninitialized(ScriptContext* requestContext)
    {TRACE_IT(61402);
        return (JavascriptSIMDBool16x8 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool16x8, requestContext->GetLibrary()->GetSIMDBool16x8TypeStatic());
    }

    JavascriptSIMDBool16x8* JavascriptSIMDBool16x8::New(SIMDValue *val, ScriptContext* requestContext)
    {TRACE_IT(61403);
        return (JavascriptSIMDBool16x8 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool16x8, val, requestContext->GetLibrary()->GetSIMDBool16x8TypeStatic());
    }

    bool  JavascriptSIMDBool16x8::Is(Var instance)
    {TRACE_IT(61404);
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDBool16x8;
    }

    JavascriptSIMDBool16x8* JavascriptSIMDBool16x8::FromVar(Var aValue)
    {TRACE_IT(61405);
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDBool16x8'");

        return reinterpret_cast<JavascriptSIMDBool16x8 *>(aValue);
    }

    RecyclableObject * JavascriptSIMDBool16x8::CloneToScriptContext(ScriptContext* requestContext)
    {TRACE_IT(61406);
        return JavascriptSIMDBool16x8::New(&value, requestContext);
    }

    const char16* JavascriptSIMDBool16x8::GetTypeName()
    {TRACE_IT(61407);
        return JavascriptSIMDBool16x8::TypeName;
    }

    Var JavascriptSIMDBool16x8::Copy(ScriptContext* requestContext)
    {TRACE_IT(61408);
        return JavascriptSIMDBool16x8::New(&this->value, requestContext);
    }
}
