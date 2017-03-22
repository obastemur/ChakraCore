//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDFloat32x4::TypeName[] = _u("SIMD.Float32x4");

    JavascriptSIMDFloat32x4::JavascriptSIMDFloat32x4(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 11\n");
        Assert(type->GetTypeId() == TypeIds_SIMDFloat32x4);
    }

    JavascriptSIMDFloat32x4::JavascriptSIMDFloat32x4(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 16\n");
        Assert(type->GetTypeId() == TypeIds_SIMDFloat32x4);
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::AllocUninitialized(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 21\n");
        return (JavascriptSIMDFloat32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDFloat32x4, requestContext->GetLibrary()->GetSIMDFloat32x4TypeStatic());
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 26\n");
        return (JavascriptSIMDFloat32x4 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDFloat32x4, val, requestContext->GetLibrary()->GetSIMDFloat32x4TypeStatic());
    }

    bool  JavascriptSIMDFloat32x4::Is(Var instance)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 31\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDFloat32x4;
    }

    JavascriptSIMDFloat32x4* JavascriptSIMDFloat32x4::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 36\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDFloat32x4'");

        return reinterpret_cast<JavascriptSIMDFloat32x4 *>(aValue);
    }

    Var JavascriptSIMDFloat32x4::CallToLocaleString(RecyclableObject& obj, ScriptContext& requestContext, SIMDValue simdValue,
        const Var* args, uint numArgs, CallInfo callInfo)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 45\n");
        const char16 *typeString = _u("SIMD.Float32x4(");
        return JavascriptSIMDObject::FromVar(&obj)->ToLocaleString<float, 4>(args, numArgs, typeString,
            simdValue.f32, &callInfo, &requestContext);
    }

    void JavascriptSIMDFloat32x4::ToStringBuffer(SIMDValue& value, __out_ecount(countBuffer) char16* stringBuffer, size_t countBuffer, ScriptContext* scriptContext)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 52\n");
        const char16* f0 = JavascriptNumber::ToStringRadix10((double)value.f32[0], scriptContext)->GetSz();
        const char16* f1 = JavascriptNumber::ToStringRadix10((double)value.f32[1], scriptContext)->GetSz();
        const char16* f2 = JavascriptNumber::ToStringRadix10((double)value.f32[2], scriptContext)->GetSz();
        const char16* f3 = JavascriptNumber::ToStringRadix10((double)value.f32[3], scriptContext)->GetSz();

        swprintf_s(stringBuffer, countBuffer, _u("SIMD.Float32x4(%s, %s, %s, %s)"), f0, f1, f2, f3);
    }

    const char16* JavascriptSIMDFloat32x4::GetTypeName()
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 62\n");
        return JavascriptSIMDFloat32x4::TypeName;
    }

    RecyclableObject * JavascriptSIMDFloat32x4::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 67\n");
        return JavascriptSIMDFloat32x4::New(&value, requestContext);
    }

    bool JavascriptSIMDFloat32x4::GetPropertyBuiltIns(PropertyId propertyId, Var* value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 72\n");
        return false;
    }

    Var JavascriptSIMDFloat32x4::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdFloat32x4.cpp] 77\n");
        return JavascriptSIMDFloat32x4::New(&this->value, requestContext);
    }

}
