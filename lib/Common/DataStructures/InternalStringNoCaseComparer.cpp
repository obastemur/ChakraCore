//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#include "CommonDataStructuresPch.h"
#include "DataStructures/CharacterBuffer.h"
#include "DataStructures/InternalStringNoCaseComparer.h"

namespace JsUtil
{
    bool NoCaseComparer<JsUtil::CharacterBuffer<CHAR_T>>::Equals(JsUtil::CharacterBuffer<CHAR_T> const& s1, JsUtil::CharacterBuffer<CHAR_T> const& s2)
    {
        return (s1.GetLength() == s2.GetLength()) && (NoCaseComparer<JsUtil::CharacterBuffer<CHAR_T>>::Compare(s1, s2)==0);
    }

    hash_t NoCaseComparer<JsUtil::CharacterBuffer<CHAR_T>>::GetHashCode(JsUtil::CharacterBuffer<CHAR_T> const& s1)
    {
        const CHAR_T* s = s1.GetBuffer();
        size_t length = s1.GetLength();
        hash_t hash = CC_HASH_OFFSET_VALUE;
        for (size_t i = 0; i < length; i++)
        {
            CC_HASH_LOGIC(hash, tolower(s[i]));
        }
        return ((hash & 0x7fffffff) << 1) | 1;
    }

    int NoCaseComparer<JsUtil::CharacterBuffer<CHAR_T>>::Compare(JsUtil::CharacterBuffer<CHAR_T> const& s1, JsUtil::CharacterBuffer<CHAR_T> const& s2)
    {
        if (s1.GetLength() != s2.GetLength()) return +1;
        int count = s1.GetLength();
        const CHAR_T* buf1 = s1.GetBuffer();
        const CHAR_T* buf2 = s2.GetBuffer();
        for (int i=0; i < count; i++)
        {
            if (tolower(buf1[i]) != tolower(buf2[i]))
            {
                return (buf1[i] < buf2[i] ? -1 : +1);
            }
        }
        return (0);
    }
}
