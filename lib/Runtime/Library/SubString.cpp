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
    {LOGMEIN("SubString.cpp] 12\n");
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
        {LOGMEIN("SubString.cpp] 29\n");
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
        {LOGMEIN("SubString.cpp] 48\n");
            return scriptContext->GetLibrary()->GetEmptyString();
        }

        Recycler* recycler = scriptContext->GetRecycler();
        return RecyclerNew(recycler, SubString, string, string + start, length, scriptContext);
    }

    const char16* SubString::GetSz()
    {LOGMEIN("SubString.cpp] 57\n");
        if (originalFullStringReference)
        {LOGMEIN("SubString.cpp] 59\n");
            Recycler* recycler = this->GetScriptContext()->GetRecycler();
            char16 * newInstance = AllocateLeafAndCopySz(recycler, UnsafeGetBuffer(), GetLength());
            this->SetBuffer(newInstance);

            // We don't need the string reference anymore, set it to nullptr and use this to know our string is nullptr terminated
            originalFullStringReference = nullptr;
        }

        return UnsafeGetBuffer();
    }

    const void * SubString::GetOriginalStringReference()
    {LOGMEIN("SubString.cpp] 72\n");
        if (originalFullStringReference != nullptr)
        {LOGMEIN("SubString.cpp] 74\n");
            return originalFullStringReference;
        }
        return __super::GetOriginalStringReference();
    }

    size_t SubString::GetAllocatedByteCount() const
    {LOGMEIN("SubString.cpp] 81\n");
        if (originalFullStringReference)
        {LOGMEIN("SubString.cpp] 83\n");
            return 0;
        }
        return __super::GetAllocatedByteCount();
    }

    bool SubString::IsSubstring() const
    {LOGMEIN("SubString.cpp] 90\n");
        if (originalFullStringReference)
        {LOGMEIN("SubString.cpp] 92\n");
            return true;
        }
        return false;
    }

}
