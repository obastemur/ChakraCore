//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class CharStringCache
    {
    public:
        CharStringCache();
        // TODO (obastemur): Fix this!
        JavascriptString* GetStringForCharA(char c);    // ASCII 7-bit
        JavascriptString* GetStringForCharW(CHAR_T c); // Unicode
        JavascriptString* GetStringForChar(CHAR_T c);  // Either
        JavascriptString* GetStringForCharSP(codepoint_t c); // Supplementary char

        // For JIT
        static const int CharStringCacheSize = 0x80; /*range of ASCII 7-bit chars*/
        static DWORD GetCharStringCacheAOffset() { return offsetof(CharStringCache, charStringCacheA); }

        static JavascriptString* GetStringForChar(CharStringCache *charStringCache, CHAR_T c) { return charStringCache->GetStringForChar(c); }
        static JavascriptString* GetStringForCharCodePoint(CharStringCache *charStringCache, codepoint_t c)
        {
            return (c >= 0x10000 ? charStringCache->GetStringForCharSP(c) : charStringCache->GetStringForCharW((CHAR_T)c));
        }

    private:
        Field(PropertyString *) charStringCacheA[CharStringCacheSize];

        typedef JsUtil::BaseDictionary<CHAR_T, JavascriptString *, Recycler, PowerOf2SizePolicy> CharStringCacheMap;
        Field(CharStringCacheMap *) charStringCache;

        friend class CharStringCacheValidator;
    };

    class CharStringCacheValidator
    {
        // Lower assert that charStringCacheA is at the 0 offset
        CompileAssert(offsetof(CharStringCache, charStringCacheA) == 0);
    };
};
