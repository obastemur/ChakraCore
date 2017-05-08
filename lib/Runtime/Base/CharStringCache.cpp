//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "RuntimeBasePch.h"

#include "Library/ProfileString.h"
#include "Library/SingleCharString.h"

namespace Js
{
    CharStringCache::CharStringCache() : charStringCache(nullptr)
    {TRACE_IT(33492);
        ClearArray(charStringCacheA);
    }

    JavascriptString* CharStringCache::GetStringForCharA(char c)
    {TRACE_IT(33493);
        AssertMsg(JavascriptString::IsASCII7BitChar(c), "GetStringForCharA must be called with ASCII 7bit chars only");

        PropertyString * str = charStringCacheA[(int)c];
        if (str == nullptr)
        {TRACE_IT(33494);
            PropertyRecord const * propertyRecord;
            char16 wc = c;
            JavascriptLibrary * javascriptLibrary = JavascriptLibrary::FromCharStringCache(this);
            javascriptLibrary->GetScriptContext()->GetOrAddPropertyRecord(&wc, 1, &propertyRecord);
            str = javascriptLibrary->CreatePropertyString(propertyRecord);
            charStringCacheA[(int)c] = str;
        }

        return str;
    }


    JavascriptString* CharStringCache::GetStringForChar(char16 c)
    {TRACE_IT(33495);
#ifdef PROFILE_STRINGS
        StringProfiler::RecordSingleCharStringRequest(JavascriptLibrary::FromCharStringCache(this)->GetScriptContext());
#endif
        if (JavascriptString::IsASCII7BitChar(c))
        {TRACE_IT(33496);
            return GetStringForCharA(JavascriptString::ToASCII7BitChar(c));
        }

        return GetStringForCharW(c);
    }

    JavascriptString* CharStringCache::GetStringForCharW(char16 c)
    {TRACE_IT(33497);
        Assert(!JavascriptString::IsASCII7BitChar(c));
        JavascriptString* str;
        ScriptContext * scriptContext = JavascriptLibrary::FromCharStringCache(this)->GetScriptContext();
        if (!scriptContext->IsClosed())
        {TRACE_IT(33498);
            if (charStringCache == nullptr)
            {TRACE_IT(33499);
                Recycler * recycler = scriptContext->GetRecycler();
                charStringCache = RecyclerNew(recycler, CharStringCacheMap, recycler, 17);
            }
            if (!charStringCache->TryGetValue(c, &str))
            {TRACE_IT(33500);
                str = SingleCharString::New(c, scriptContext);
                charStringCache->Add(c, str);
            }
        }
        else
        {TRACE_IT(33501);
            str = SingleCharString::New(c, scriptContext);
        }
        return str;
    }

    JavascriptString* CharStringCache::GetStringForCharSP(codepoint_t c)
    {TRACE_IT(33502);
        Assert(c >= 0x10000);
        CompileAssert(sizeof(char16) * 2 == sizeof(codepoint_t));
        char16 buffer[2];

        Js::NumberUtilities::CodePointAsSurrogatePair(c, buffer, buffer + 1);
        JavascriptString* str = JavascriptString::NewCopyBuffer(buffer, 2, JavascriptLibrary::FromCharStringCache(this)->GetScriptContext());
        // TODO: perhaps do some sort of cache for supplementary characters
        return str;
    }
};
