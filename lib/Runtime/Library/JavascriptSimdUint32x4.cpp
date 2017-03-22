//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDUint32x4::TypeName[] = _u("SIMD.Uint32x4");

    JavascriptSIMDUint32x4::JavascriptSIMDUint32x4(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 11\n");
        Assert(type->GetTypeId() == TypeIds_SIMDUint32x4);
    }

    JavascriptSIMDUint32x4::JavascriptSIMDUint32x4(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 16\n");
        Assert(type->GetTypeId() == TypeIds_SIMDUint32x4);
    }

    JavascriptSIMDUint32x4* JavascriptSIMDUint32x4::AllocUninitialized(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 21\n");
        return (JavascriptSIMDUint32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDUint32x4, requestContext->GetLibrary()->GetSIMDUInt32x4TypeStatic());
    }

    JavascriptSIMDUint32x4* JavascriptSIMDUint32x4::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 26\n");
        return (JavascriptSIMDUint32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDUint32x4, val, requestContext->GetLibrary()->GetSIMDUInt32x4TypeStatic());
    }

    bool  JavascriptSIMDUint32x4::Is(Var instance)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 31\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDUint32x4;
    }

    JavascriptSIMDUint32x4* JavascriptSIMDUint32x4::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 36\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDUint32x4'");

        return reinterpret_cast<JavascriptSIMDUint32x4 *>(aValue);
    }

    Var JavascriptSIMDUint32x4::CallToLocaleString(RecyclableObject& obj, ScriptContext& requestContext, SIMDValue simdValue,
        const Var* args, uint numArgs, CallInfo callInfo)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 45\n");
        const char16 *typeString = _u("SIMD.Uint32x4(");
        return JavascriptSIMDObject::FromVar(&obj)->ToLocaleString<int32, 4>(args, numArgs, typeString,
            simdValue.i32, &callInfo, &requestContext);
    }

    RecyclableObject * JavascriptSIMDUint32x4::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 52\n");
        return JavascriptSIMDUint32x4::New(&value, requestContext);
    }

    const char16* JavascriptSIMDUint32x4::GetTypeName()
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 57\n");
        return JavascriptSIMDUint32x4::TypeName;
    }

    Var JavascriptSIMDUint32x4::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdUint32x4.cpp] 62\n");
        return JavascriptSIMDUint32x4::New(&this->value, requestContext);
    }

}
