//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


#include "RuntimeLibraryPch.h"


namespace Js
{
    const char16 JavascriptSIMDBool8x16::TypeName[] = _u("SIMD.Bool8x16");

    JavascriptSIMDBool8x16::JavascriptSIMDBool8x16(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 14\n");
        Assert(type->GetTypeId() == TypeIds_SIMDBool8x16);
    }

    JavascriptSIMDBool8x16::JavascriptSIMDBool8x16(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 19\n");
        Assert(type->GetTypeId() == TypeIds_SIMDBool8x16);
    }

    JavascriptSIMDBool8x16* JavascriptSIMDBool8x16::AllocUninitialized(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 24\n");
        return (JavascriptSIMDBool8x16 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool8x16, requestContext->GetLibrary()->GetSIMDBool8x16TypeStatic());
    }

    JavascriptSIMDBool8x16* JavascriptSIMDBool8x16::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 29\n");
        return (JavascriptSIMDBool8x16 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool8x16, val, requestContext->GetLibrary()->GetSIMDBool8x16TypeStatic());
    }

    bool  JavascriptSIMDBool8x16::Is(Var instance)
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 34\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDBool8x16;
    }

    JavascriptSIMDBool8x16* JavascriptSIMDBool8x16::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 39\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDBool8x16'");

        return reinterpret_cast<JavascriptSIMDBool8x16 *>(aValue);
    }

    RecyclableObject * JavascriptSIMDBool8x16::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 47\n");
        return JavascriptSIMDBool8x16::New(&value, requestContext);
    }

    const char16* JavascriptSIMDBool8x16::GetTypeName()
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 52\n");
        return JavascriptSIMDBool8x16::TypeName;
    }

    Var JavascriptSIMDBool8x16::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool8x16.cpp] 57\n");
        return JavascriptSIMDBool8x16::New(&this->value, requestContext);
    }
}
