//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#include "RuntimeLibraryPch.h"

namespace Js
{
    const char16 JavascriptSIMDUint8x16::TypeName[] = _u("SIMD.Uint8x16");

    JavascriptSIMDUint8x16::JavascriptSIMDUint8x16(StaticType *type) : JavascriptSIMDType(type)
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 12\n");
        Assert(type->GetTypeId() == TypeIds_SIMDUint8x16);
    }

    JavascriptSIMDUint8x16::JavascriptSIMDUint8x16(SIMDValue *val, StaticType *type) : JavascriptSIMDType(val, type)
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 17\n");
        Assert(type->GetTypeId() == TypeIds_SIMDUint8x16);
    }

    JavascriptSIMDUint8x16* JavascriptSIMDUint8x16::New(SIMDValue *val, ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 22\n");
        return (JavascriptSIMDUint8x16 *)AllocatorNew(Recycler, requestContext->GetRecycler(), JavascriptSIMDUint8x16, val, requestContext->GetLibrary()->GetSIMDUint8x16TypeStatic());
    }

    bool  JavascriptSIMDUint8x16::Is(Var instance)
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 27\n");
        return JavascriptOperators::GetTypeId(instance) == TypeIds_SIMDUint8x16;
    }

    JavascriptSIMDUint8x16* JavascriptSIMDUint8x16::FromVar(Var aValue)
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 32\n");
        Assert(aValue);
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptSIMDUint8x16'");

        return reinterpret_cast<JavascriptSIMDUint8x16 *>(aValue);
    }

    Var JavascriptSIMDUint8x16::CallToLocaleString(RecyclableObject& obj, ScriptContext& requestContext, SIMDValue simdValue,
        const Var* args, uint numArgs, CallInfo callInfo)
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 41\n");
        const char16 *typeString = _u("SIMD.Uint8x16(");
        return JavascriptSIMDObject::FromVar(&obj)->ToLocaleString<uint8, 16>(args, numArgs, typeString,
            simdValue.u8, &callInfo, &requestContext);
    }

    RecyclableObject * JavascriptSIMDUint8x16::CloneToScriptContext(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 48\n");
        return JavascriptSIMDUint8x16::New(&value, requestContext);
    }

    const char16* JavascriptSIMDUint8x16::GetTypeName()
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 53\n");
        return JavascriptSIMDUint8x16::TypeName;
    }

    Var JavascriptSIMDUint8x16::Copy(ScriptContext* requestContext)
    {LOGMEIN("JavascriptSimdUint8x16.cpp] 58\n");
        return JavascriptSIMDUint8x16::New(&this->value, requestContext);
    }
}
