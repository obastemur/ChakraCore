//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

#pragma once

namespace Js
{
#if FLOATVAR
    inline JavascriptNumber::JavascriptNumber(double value, StaticType*
#if DBG
        , bool oopJIT /*= false*/
#endif
    )
    {TRACE_IT(60318);
        AssertMsg(!IsNan(value) || ToSpecial(value) == k_Nan || ToSpecial(value) == 0x7FF8000000000000ull, "We should only produce a NaN with this value");
        SetSpecial(ToSpecial(value) ^ FloatTag_Value);
    }
#else
    inline JavascriptNumber::JavascriptNumber(double value, StaticType * type
#if DBG
        , bool oopJIT /*= false*/
#endif
    ) : RecyclableObject(type), m_value(value)
    {TRACE_IT(60319);
        // for oopjit type will be pointing to address of StaticType on other proc, so don't dereference it
        Assert(oopJIT || type->GetTypeId() == TypeIds_Number);
    }
#endif

    __forceinline Var JavascriptNumber::ToVar(int32 nValue, ScriptContext* scriptContext)
    {TRACE_IT(60320);
        if (!TaggedInt::IsOverflow(nValue))
        {TRACE_IT(60321);
            return TaggedInt::ToVarUnchecked(nValue);
        }
        else
        {TRACE_IT(60322);
            return JavascriptNumber::NewInlined((double) nValue, scriptContext);
        }
    }

#if defined(__clang__) && defined(_M_IX86)
    __forceinline Var JavascriptNumber::ToVar(intptr_t nValue, ScriptContext* scriptContext)
    {TRACE_IT(60323);
        if (!TaggedInt::IsOverflow(nValue))
        {TRACE_IT(60324);
            return TaggedInt::ToVarUnchecked(nValue);
        }
        else
        {TRACE_IT(60325);
            return JavascriptNumber::NewInlined((double) nValue, scriptContext);
        }
    }
#endif

    inline Var JavascriptNumber::ToVar(uint32 nValue, ScriptContext* scriptContext)
    {TRACE_IT(60326);
        return !TaggedInt::IsOverflow(nValue) ? TaggedInt::ToVarUnchecked(nValue) :
            JavascriptNumber::New((double) nValue,scriptContext);
    }

    inline Var JavascriptNumber::ToVar(int64 nValue, ScriptContext* scriptContext)
    {TRACE_IT(60327);
        return !TaggedInt::IsOverflow(nValue) ?
                TaggedInt::ToVarUnchecked((int) nValue) :
                JavascriptNumber::New((double) nValue,scriptContext);
    }

    inline Var JavascriptNumber::ToVar(uint64 nValue, ScriptContext* scriptContext)
    {TRACE_IT(60328);
        return !TaggedInt::IsOverflow(nValue) ?
                TaggedInt::ToVarUnchecked((int) nValue) :
                JavascriptNumber::New((double) nValue,scriptContext);
    }

    inline bool JavascriptNumber::TryToVarFast(int32 nValue, Var* result)
    {TRACE_IT(60329);
        if (!TaggedInt::IsOverflow(nValue))
        {TRACE_IT(60330);
            *result = TaggedInt::ToVarUnchecked(nValue);
            return true;
        }

#if FLOATVAR
        *result = JavascriptNumber::ToVar((double)nValue);
        return true;
#else
        return false;
#endif
    }

    inline bool JavascriptNumber::TryToVarFastWithCheck(double value, Var* result)
    {TRACE_IT(60331);
#if FLOATVAR
        if (IsNan(value))
        {TRACE_IT(60332);
            value = JavascriptNumber::NaN;
        }

        *result = JavascriptNumber::ToVar(value);
        return true;
#else
        return false;
#endif
    }

#if FLOATVAR
    inline bool JavascriptNumber::Is(Var aValue)
    {TRACE_IT(60333);
        return Is_NoTaggedIntCheck(aValue);
    }

    inline JavascriptNumber* JavascriptNumber::InPlaceNew(double value, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {TRACE_IT(60334);
        AssertMsg( result != NULL, "Cannot use InPlaceNew without a value result location" );
        result = (JavascriptNumber*)ToVar(value);
        return result;
    }

    inline Var JavascriptNumber::New(double value, ScriptContext* scriptContext)
    {TRACE_IT(60335);
        return ToVar(value);
    }

    inline Var JavascriptNumber::NewWithCheck(double value, ScriptContext* scriptContext)
    {TRACE_IT(60336);
        if (IsNan(value))
        {TRACE_IT(60337);
            value = JavascriptNumber::NaN;
        }
        return ToVar(value);
    }

    inline Var JavascriptNumber::NewInlined(double value, ScriptContext* scriptContext)
    {TRACE_IT(60338);
        return ToVar(value);
    }

#if ENABLE_NATIVE_CODEGEN
    inline Var JavascriptNumber::NewCodeGenInstance(double value, ScriptContext* scriptContext)
    {TRACE_IT(60339);
        return ToVar(value);
    }
#endif

    inline Var JavascriptNumber::ToVar(double value)
    {TRACE_IT(60340);
        uint64 val = *(uint64*)&value;
        AssertMsg(!IsNan(value) || ToSpecial(value) == k_Nan || ToSpecial(value) == 0x7FF8000000000000ull, "We should only produce a NaN with this value");
        return reinterpret_cast<Var>(val ^ FloatTag_Value);
    }

#else
    inline bool JavascriptNumber::Is(Var aValue)
    {TRACE_IT(60341);
        return !TaggedInt::Is(aValue) && Is_NoTaggedIntCheck(aValue);
    }

#if !defined(USED_IN_STATIC_LIB)
    inline bool JavascriptNumber::Is_NoTaggedIntCheck(Var aValue)
    {TRACE_IT(60342);
        RecyclableObject* object = RecyclableObject::FromVar(aValue);
        AssertMsg((object->GetTypeId() == TypeIds_Number) == VirtualTableInfo<JavascriptNumber>::HasVirtualTable(object), "JavascriptNumber has no unique VTABLE?");
        return VirtualTableInfo<JavascriptNumber>::HasVirtualTable(object);
    }
#endif

    inline JavascriptNumber* JavascriptNumber::FromVar(Var aValue)
    {
        AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNumber'");

        return reinterpret_cast<JavascriptNumber *>(aValue);
    }

    inline double JavascriptNumber::GetValue(Var aValue)
     {
         AssertMsg(Is(aValue), "Ensure var is actually a 'JavascriptNumber'");

         return JavascriptNumber::FromVar(aValue)->GetValue();
     }

    inline JavascriptNumber* JavascriptNumber::InPlaceNew(double value, ScriptContext* scriptContext, Js::JavascriptNumber *result)
    {TRACE_IT(60343);
        AssertMsg( result != NULL, "Cannot use InPlaceNew without a value result location" );
        Assume(result != NULL); // Encourage the compiler to omit a NULL check on the return from placement new
        return ::new(result) JavascriptNumber(value, scriptContext->GetLibrary()->GetNumberTypeStatic());
    }

    inline Var JavascriptNumber::New(double value, ScriptContext* scriptContext)
    {TRACE_IT(60344);
        return scriptContext->GetLibrary()->CreateNumber(value, scriptContext->GetNumberAllocator());
    }

    inline Var JavascriptNumber::NewWithCheck(double value, ScriptContext* scriptContext)
    {TRACE_IT(60345);
        return scriptContext->GetLibrary()->CreateNumber(value, scriptContext->GetNumberAllocator());
    }

    inline Var JavascriptNumber::NewInlined(double value, ScriptContext* scriptContext)
    {TRACE_IT(60346);
        return scriptContext->GetLibrary()->CreateNumber(value, scriptContext->GetNumberAllocator());
    }

#if ENABLE_NATIVE_CODEGEN
    inline Var JavascriptNumber::NewCodeGenInstance(CodeGenNumberAllocator *alloc, double value, ScriptContext* scriptContext)
    {TRACE_IT(60347);
        return scriptContext->GetLibrary()->CreateCodeGenNumber(alloc, value);
    }
#endif

#endif

    inline JavascriptString * JavascriptNumber::ToStringNan(ScriptContext* scriptContext)
    {TRACE_IT(60348);
        return ToStringNan(*scriptContext->GetLibrary());
    }

    inline JavascriptString* JavascriptNumber::ToStringNanOrInfinite(double value, ScriptContext* scriptContext)
    {TRACE_IT(60349);
        return ToStringNanOrInfinite(value, *scriptContext->GetLibrary());
    }

    inline Var JavascriptNumber::FormatDoubleToString( double value, Js::NumberUtilities::FormatType formatType, int formatDigits, ScriptContext* scriptContext )
    {TRACE_IT(60350);
        static const int bufSize = 256;
        char16 szBuffer[bufSize] = _u("");
        char16 * psz = szBuffer;
        char16 * pszToBeFreed = NULL;
        int nOut;

        if ((nOut = Js::NumberUtilities::FDblToStr(value, formatType, formatDigits, szBuffer, bufSize)) > bufSize )
        {TRACE_IT(60351);
            int nOut1;
            pszToBeFreed = psz = (char16 *)malloc(nOut * sizeof(char16));
            if(0 == psz)
            {TRACE_IT(60352);
                Js::JavascriptError::ThrowOutOfMemoryError(scriptContext);
            }

            nOut1 = Js::NumberUtilities::FDblToStr(value, Js::NumberUtilities::FormatFixed, formatDigits, psz, nOut);
            Assert(nOut1 == nOut);
        }

        // nOut includes room for terminating NUL
        JavascriptString* result = JavascriptString::NewCopyBuffer(psz, nOut - 1, scriptContext);

        if(pszToBeFreed)
        {TRACE_IT(60353);
            free(pszToBeFreed);
        }

        return result;
    }
}
