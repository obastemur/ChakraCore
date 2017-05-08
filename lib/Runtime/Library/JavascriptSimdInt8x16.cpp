//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDInt8x16::TypeName[] = _u("SIMD.Int8x16");

    JavascriptSIMDInt8x16::JavascriptSIMDInt8x16(StaticType *type) : JavascriptSIMDType(type)
    {TRACE_IT(61491);
        Assert(type->GetTypeId() == TypeIds_SIMDInt8x16);
    }

    JavascriptSIMDInt8x16::JavascriptSIMDInt8x16(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {TRACE_IT(61492);
        Assert(type->GetTypeId() == TypeIds_SIMDInt8x16);
    }

    JavascriptSIMDInt8x16* JavascriptSIMDInt8x16::New(SIMDValue *val, ScriptContext* requestContext)
    {TRACE_IT(61493);
        return (JavascriptSIMDInt8x16 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDInt8x16, val, requestContext->GetLibrary()->GetSIMDInt8x16TypeStatic());
    }

    bool  JavascriptSIMDInt8x16::Is(Var instance)
    {TRACE_IT(61494);
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDInt8x16;
    }

    JavascriptSIMDInt8x16* JavascriptSIMDInt8x16::FromVar(Var aValue)
    {TRACE_IT(61495);
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDInt8x16'");

        return reinterpret_cast<JavascriptSIMDInt8x16 *>(aValue);
    }

    Var JavascriptSIMDInt8x16::CallToLocaleString(RecyclableObject& obj, ScriptContext& requestContext, SIMDValue simdValue,
        const Var* args, uint numArgs, CallInfo callInfo)
    {TRACE_IT(61496);
        const char16 *typeString = _u("SIMD.Int8x16(");
        return JavascriptSIMDObject::FromVar(&obj)->ToLocaleString<int8, 16>(args, numArgs, typeString,
            simdValue.i8, &callInfo, &requestContext);
    }

    RecyclableObject * JavascriptSIMDInt8x16::CloneToScriptContext(ScriptContext* requestContext)
    {TRACE_IT(61497);
        return JavascriptSIMDInt8x16::New(&value, requestContext);
    }

    const char16* JavascriptSIMDInt8x16::GetTypeName()
    {TRACE_IT(61498);
        return JavascriptSIMDInt8x16::TypeName;
    }

    Var JavascriptSIMDInt8x16::Copy(ScriptContext* requestContext)
    {TRACE_IT(61499);
        return JavascriptSIMDInt8x16::New(&this->value, requestContext);
    }
}
