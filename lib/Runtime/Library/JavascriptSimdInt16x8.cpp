//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft Corporation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


#include "RuntimeLibraryPch.h"


namespace Js
{
    const char16 JavascriptSIMDInt16x8::TypeName[] = _u("SIMD.Int16x8");

    JavascriptSIMDInt16x8::JavascriptSIMDInt16x8(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 14\n");
        Assert(type->GetTypeId() == TypeIds_SIMDInt16x8);
    }

    JavascriptSIMDInt16x8::JavascriptSIMDInt16x8(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 19\n");
        Assert(type->GetTypeId() == TypeIds_SIMDInt16x8);
    }

    JavascriptSIMDInt16x8* JavascriptSIMDInt16x8::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 24\n");
        return (JavascriptSIMDInt16x8 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDInt16x8, val, requestContext->GetLibrary()->GetSIMDInt16x8TypeStatic());
    }

    bool  JavascriptSIMDInt16x8::Is(Var instance)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 29\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDInt16x8;
    }

    JavascriptSIMDInt16x8* JavascriptSIMDInt16x8::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 34\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDInt16x8'");

        return reinterpret_cast<JavascriptSIMDInt16x8 *>(aValue);
    }

    Var JavascriptSIMDInt16x8::CallToLocaleString(RecyclableObject& obj, ScriptContext& requestContext, SIMDValue simdValue,
        const Var* args, uint numArgs, CallInfo callInfo)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 43\n");
        const char16 *typeString = _u("SIMD.Int16x8(");
        return JavascriptSIMDObject::FromVar(&obj)->ToLocaleString<int16, 8>(args, numArgs, typeString,
            simdValue.i16, &callInfo, &requestContext);
    }

    RecyclableObject * JavascriptSIMDInt16x8::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 50\n");
        return JavascriptSIMDInt16x8::New(&value, requestContext);
    }

    const char16* JavascriptSIMDInt16x8::GetTypeName()
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 55\n");
        return JavascriptSIMDInt16x8::TypeName;
    }

    Var JavascriptSIMDInt16x8::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 60\n");
        return JavascriptSIMDInt16x8::New(&this->value, requestContext);
    }

    Var JavascriptSIMDInt16x8::CopyAndSetLane(uint index, int value, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 65\n");
        AssertMsg(index < 8, "Out of range lane index");
        Var instance = Copy(requestContext);
        JavascriptSIMDInt16x8 *insValue = JavascriptSIMDInt16x8::FromVar(instance);
        Assert(insValue);
        insValue->value.i16[index] = (short)value;
        return instance;
    }

    inline Var  JavascriptSIMDInt16x8::GetLaneAsNumber(uint index, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdInt16x8.cpp] 75\n");
        // convert value.i32[index] to TaggedInt
        AssertMsg(index < 8, "Out of range lane index");
        return JavascriptNumber::ToVar(value.i16[index], requestContext);
    }
}

