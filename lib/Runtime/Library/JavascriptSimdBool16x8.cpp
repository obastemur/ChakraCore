//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDBool16x8::TypeName[] = _u("SIMD.Bool16x8");

    JavascriptSIMDBool16x8::JavascriptSIMDBool16x8(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 11\n");
        Assert(type->GetTypeId() == TypeIds_SIMDBool16x8);
    }

    JavascriptSIMDBool16x8::JavascriptSIMDBool16x8(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 16\n");
        Assert(type->GetTypeId() == TypeIds_SIMDBool16x8);
    }

    JavascriptSIMDBool16x8* JavascriptSIMDBool16x8::AllocUninitialized(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 21\n");
        return (JavascriptSIMDBool16x8 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool16x8, requestContext->GetLibrary()->GetSIMDBool16x8TypeStatic());
    }

    JavascriptSIMDBool16x8* JavascriptSIMDBool16x8::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 26\n");
        return (JavascriptSIMDBool16x8 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool16x8, val, requestContext->GetLibrary()->GetSIMDBool16x8TypeStatic());
    }

    bool  JavascriptSIMDBool16x8::Is(Var instance)
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 31\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDBool16x8;
    }

    JavascriptSIMDBool16x8* JavascriptSIMDBool16x8::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 36\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDBool16x8'");

        return reinterpret_cast<JavascriptSIMDBool16x8 *>(aValue);
    }

    RecyclableObject * JavascriptSIMDBool16x8::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 44\n");
        return JavascriptSIMDBool16x8::New(&value, requestContext);
    }

    const char16* JavascriptSIMDBool16x8::GetTypeName()
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 49\n");
        return JavascriptSIMDBool16x8::TypeName;
    }

    Var JavascriptSIMDBool16x8::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool16x8.cpp] 54\n");
        return JavascriptSIMDBool16x8::New(&this->value, requestContext);
    }
}
