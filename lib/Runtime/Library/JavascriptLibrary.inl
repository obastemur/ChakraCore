//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Create a string literal from a C++ string (const char16 array with compile-time determined size)
    // Note: The template arg is the string length in characters, including the NUL terminator.
    template< size_t N > JavascriptString* JavascriptLibrary::CreateStringFromCppLiteral(const char16(&value)[N]) const
    {TRACE_IT(59983);
        CompileAssert(N>2); // Other values are handled by the specializations below
        return LiteralString::New(GetStringTypeStatic(), value, N - 1 /*don't include terminating NUL*/, this->GetRecycler());
    }

    // Specialization for the empty string
    template<> inline JavascriptString* JavascriptLibrary::CreateStringFromCppLiteral(const char16(&value)[1]) const
    {TRACE_IT(59984);
        return GetEmptyString();
    }
    
    // Specialization for single-char strings
    template<> inline JavascriptString* JavascriptLibrary::CreateStringFromCppLiteral(const char16(&value)[2]) const
    {TRACE_IT(59985);
        return charStringCache.GetStringForChar(value[0]);
    }

    template <size_t N>
    JavascriptFunction * JavascriptLibrary::AddFunctionToLibraryObjectWithPropertyName(DynamicObject* object, const char16(&propertyName)[N], FunctionInfo * functionInfo, int length)
    {TRACE_IT(59986);
        // The PID need to be tracked because it is assigned to the runtime function's nameId
        return AddFunctionToLibraryObject(object, scriptContext->GetOrAddPropertyIdTracked(propertyName), functionInfo, length);
    }

    inline void JavascriptLibrary::CheckAndInvalidateIsConcatSpreadableCache(PropertyId propertyId, ScriptContext *scriptContext)
    {TRACE_IT(59987);
        if (!PHASE_OFF1(IsConcatSpreadableCachePhase) && propertyId == PropertyIds::_symbolIsConcatSpreadable)
        {TRACE_IT(59988);
            OUTPUT_TRACE(Phase::IsConcatSpreadableCachePhase, _u("IsConcatSpreadableCache invalidated\n"));
            scriptContext->GetThreadContext()->GetIsConcatSpreadableCache()->Invalidate();
        }
    }

#if ENABLE_COPYONACCESS_ARRAY
    template <>
    inline void JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray(const Var instance)
    {TRACE_IT(59989);
        if (instance && JavascriptCopyOnAccessNativeIntArray::Is(instance))
        {TRACE_IT(59990);
            JavascriptCopyOnAccessNativeIntArray::FromVar(instance)->ConvertCopyOnAccessSegment();
        }
    }

    template <typename T>
    void JavascriptLibrary::CheckAndConvertCopyOnAccessNativeIntArray(const T instance)
    {TRACE_IT(59991);
        // dummy template function
    }
#endif
}
