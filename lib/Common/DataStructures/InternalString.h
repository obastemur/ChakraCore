//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    class InternalString
    {
        Field(charcount_t) m_charLength;
        Field(unsigned char) m_offset;
        Field(const CHAR_T*) m_content;

    public:
        InternalString() : m_charLength(0), m_content(nullptr), m_offset(0) { };
        InternalString(const CHAR_T* content, DECLSPEC_GUARD_OVERFLOW charcount_t charLength, unsigned char offset = 0);
        InternalString(const CHAR_T* content, _no_write_barrier_tag, DECLSPEC_GUARD_OVERFLOW charcount_t charLength, unsigned char offset = 0);

        static InternalString* New(ArenaAllocator* alloc, const CHAR_T* content, DECLSPEC_GUARD_OVERFLOW charcount_t length);
        static InternalString* New(Recycler* recycler, const CHAR_T* content, DECLSPEC_GUARD_OVERFLOW charcount_t length);
        static InternalString* NewNoCopy(ArenaAllocator* alloc, const CHAR_T* content, DECLSPEC_GUARD_OVERFLOW charcount_t length);

        inline charcount_t GetLength() const
        {
            return m_charLength;
        }

        inline const CHAR_T* GetBuffer() const
        {
            return m_content + m_offset;
        }
    };

    struct InternalStringComparer
    {
        inline static bool Equals(InternalString const& str1, InternalString const& str2)
        {
            return str1.GetLength() == str2.GetLength() &&
                JsUtil::CharacterBuffer<CHAR_T>::StaticEquals(str1.GetBuffer(), str2.GetBuffer(), str1.GetLength());
        }

        inline static hash_t GetHashCode(InternalString const& str)
        {
            return JsUtil::CharacterBuffer<CHAR_T>::StaticGetHashCode(str.GetBuffer(), str.GetLength());
        }
    };
}

template<>
struct DefaultComparer<Js::InternalString> : public Js::InternalStringComparer {};
