//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDInt8x16::TypeName[] = _u("SIMD.Int8x16");

    JavascriptSIMDInt8x16::JavascriptSIMDInt8x16(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 11\n");
        Assert(type->GetTypeId() == TypeIds_SIMDInt8x16);
    }

    JavascriptSIMDInt8x16::JavascriptSIMDInt8x16(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 16\n");
        Assert(type->GetTypeId() == TypeIds_SIMDInt8x16);
    }

    JavascriptSIMDInt8x16* JavascriptSIMDInt8x16::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 21\n");
        return (JavascriptSIMDInt8x16 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDInt8x16, val, requestContext->GetLibrary()->GetSIMDInt8x16TypeStatic());
    }

    bool  JavascriptSIMDInt8x16::Is(Var instance)
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 26\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDInt8x16;
    }

    JavascriptSIMDInt8x16* JavascriptSIMDInt8x16::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 31\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDInt8x16'");

        return reinterpret_cast<JavascriptSIMDInt8x16 *>(aValue);
    }

    Var JavascriptSIMDInt8x16::CallToLocaleString(RecyclableObject& obj, ScriptContext& requestContext, SIMDValue simdValue,
        const Var* args, uint numArgs, CallInfo callInfo)
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 40\n");
        const char16 *typeString = _u("SIMD.Int8x16(");
        return JavascriptSIMDObject::FromVar(&obj)->ToLocaleString<int8, 16>(args, numArgs, typeString,
            simdValue.i8, &callInfo, &requestContext);
    }

    RecyclableObject * JavascriptSIMDInt8x16::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 47\n");
        return JavascriptSIMDInt8x16::New(&value, requestContext);
    }

    const char16* JavascriptSIMDInt8x16::GetTypeName()
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 52\n");
        return JavascriptSIMDInt8x16::TypeName;
    }

    Var JavascriptSIMDInt8x16::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt8x16.cpp] 57\n");
        return JavascriptSIMDInt8x16::New(&this->value, requestContext);
    }
}
