//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDInt32x4::TypeName[] = _u("SIMD.Int32x4");

    JavascriptSIMDInt32x4::JavascriptSIMDInt32x4(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 11\n");
        Assert(type->GetTypeId() == TypeIds_SIMDInt32x4);
    }

    JavascriptSIMDInt32x4::JavascriptSIMDInt32x4(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 16\n");
        Assert(type->GetTypeId() == TypeIds_SIMDInt32x4);
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::AllocUninitialized(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 21\n");
        return (JavascriptSIMDInt32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDInt32x4, requestContext->GetLibrary()->GetSIMDInt32x4TypeStatic());
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 26\n");
        return (JavascriptSIMDInt32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDInt32x4, val, requestContext->GetLibrary()->GetSIMDInt32x4TypeStatic());
    }

    bool  JavascriptSIMDInt32x4::Is(Var instance)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 31\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDInt32x4;
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 36\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDInt32x4'");

        return reinterpret_cast<JavascriptSIMDInt32x4 *>(aValue);
    }

    JavascriptSIMDInt32x4* JavascriptSIMDInt32x4::FromFloat64x2(JavascriptSIMDFloat64x2 *instance, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 44\n");
        SIMDValue result = SIMDInt32x4Operation::OpFromFloat64x2(instance->GetValue());
        return JavascriptSIMDInt32x4::New(&result, requestContext);
    }

    RecyclableObject * JavascriptSIMDInt32x4::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 50\n");
        return JavascriptSIMDInt32x4::New(&value, requestContext);
    }

    bool JavascriptSIMDInt32x4::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 55\n");
        return false;
    }

    const char16* JavascriptSIMDInt32x4::GetTypeName()
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 60\n");
        return JavascriptSIMDInt32x4::TypeName;
    }

    Var JavascriptSIMDInt32x4::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 65\n");
        return JavascriptSIMDInt32x4::New(&this->value, requestContext);
    }

    Var JavascriptSIMDInt32x4::CallToLocaleString(RecyclableObject& obj, ScriptContext& requestContext, SIMDValue simdValue,
        const Var* args, uint numArgs, CallInfo callInfo)
    {LOGMEIN("JavascriptSimdInt32x4.cpp] 71\n");
        const char16 *typeString = _u("SIMD.Int32x4(");
        return JavascriptSIMDObject::FromVar(&obj)->ToLocaleString<int, 4>(args, numArgs, typeString,
            simdValue.i32, &callInfo, &requestContext);
    }
}
