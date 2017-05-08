//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeLibraryPch.h"

namespace Js
{
    DEFINE_RECYCLER_TRACKER_PERF_COUNTER(SubString);

    inline SubString::SubString(void const * originalFullStringReference, const char16* subString, charcount_t length, ScriptContext *scriptContext) :
        JavascriptString(scriptContext->GetLibrary()->GetStringTypeStatic())
    {TRACE_IT(63765);
        this->SetBuffer(subString);
        this->originalFullStringReference = originalFullStringReference;
        this->SetLength(length);

#ifdef PROFILE_STRINGS
        StringProfiler::RecordNewString( scriptContext, this->UnsafeGetBuffer(), this->GetLength() );
#endif
    }

    JavascriptString* SubString::New(JavascriptString* string, charcount_t start, charcount_t length)
    {
        AssertMsg( IsValidCharCount(start), "start is out of range" );
        AssertMsg( IsValidCharCount(length), "length is out of range" );

        ScriptContext *scriptContext = string->GetScriptContext();
        if (!length)
        {TRACE_IT(63766);
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        Recycler* recycler = scriptContext->GetRecycler();

        Assert(string->GetLength() >= start + length);
        const char16 * subString = string->GetString() + start;
        void const * originalFullStringReference = string->GetOriginalStringReference();

        return RecyclerNew(recycler, SubString, originalFullStringReference, subString, length, scriptContext);
    }

    JavascriptString* SubString::New(const char16* string, charcount_t start, charcount_t length, ScriptContext *scriptContext)
    {
        AssertMsg( IsValidCharCount(start), "start is out of range" );
        AssertMsg( IsValidCharCount(length), "length is out of range" );

        if (!length)
        {TRACE_IT(63767);
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        Recycler* recycler = scriptContext->GetRecycler();
        return RecyclerNew(recycler, SubString, string, string + start, length, scriptContext);
    }

    const char16* SubString::GetSz()
    {TRACE_IT(63768);
        if (originalFullStringReference)
        {TRACE_IT(63769);
            Recycler* recycler = this->GetScriptContext()->GetRecycler();
            char16 * newInstance = AllocateLeafAndCopySz(recycler, UnsafeGetBuffer(), GetLength());
            this->SetBuffer(newInstance);

            // We don't need the string reference anymore, set it to nullptr and use this to know our string is nullptr terminated
            originalFullStringReference = nullptr;
        }

        return UnsafeGetBuffer();
    }

    const void * SubString::GetOriginalStringReference()
    {TRACE_IT(63770);
        if (originalFullStringReference != nullptr)
        {TRACE_IT(63771);
            return originalFullStringReference;
        }
        return __super::GetOriginalStringReference();
    }

    size_t SubString::GetAllocatedByteCount() const
    {TRACE_IT(63772);
        if (originalFullStringReference)
        {TRACE_IT(63773);
            return 0;
        }
        return __super::GetAllocatedByteCount();
    }

    bool SubString::IsSubstring() const
    {TRACE_IT(63774);
        if (originalFullStringReference)
        {TRACE_IT(63775);
            return true;
        }
        return false;
    }

}
