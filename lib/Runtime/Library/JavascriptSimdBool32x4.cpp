//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDBool32x4::TypeName[] = _u("SIMD.Bool32x4");

    JavascriptSIMDBool32x4::JavascriptSIMDBool32x4(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 11\n");
        Assert(type->GetTypeId() == TypeIds_SIMDBool32x4);
    }

    JavascriptSIMDBool32x4::JavascriptSIMDBool32x4(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 16\n");
        Assert(type->GetTypeId() == TypeIds_SIMDBool32x4);
    }

    JavascriptSIMDBool32x4* JavascriptSIMDBool32x4::AllocUninitialized(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 21\n");
        return (JavascriptSIMDBool32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool32x4, requestContext->GetLibrary()->GetSIMDBool32x4TypeStatic());
    }

    JavascriptSIMDBool32x4* JavascriptSIMDBool32x4::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 26\n");
        return (JavascriptSIMDBool32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDBool32x4, val, requestContext->GetLibrary()->GetSIMDBool32x4TypeStatic());
    }

    bool  JavascriptSIMDBool32x4::Is(Var instance)
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 31\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDBool32x4;
    }

    JavascriptSIMDBool32x4* JavascriptSIMDBool32x4::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 36\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDBool32x4'");

        return reinterpret_cast<JavascriptSIMDBool32x4 *>(aValue);
    }

    const char16* JavascriptSIMDBool32x4::GetTypeName()
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 44\n");
        return JavascriptSIMDBool32x4::TypeName;
    }

    Var JavascriptSIMDBool32x4::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 49\n");
        return JavascriptSIMDBool32x4::New(&this->value, requestContext);
    }

    RecyclableObject * JavascriptSIMDBool32x4::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdBool32x4.cpp] 54\n");
        return JavascriptSIMDBool32x4::New(&value, requestContext);
    }
}
