//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    static const charcount_t MAX_FAST_HASH_LENGTH = 256;

    // A buffer of characters, may have embedded null.
    template <typename T>
    class CharacterBuffer
    {
    public:
        CharacterBuffer() : string(nullptr), len((charcount_t)-1), hashCode(0) {}
        CharacterBuffer(T const * string, charcount_t len) : string(string), len(len), hashCode(0) {}
        CharacterBuffer(const CharacterBuffer& other) : string(other.string), len(other.len), hashCode(0) {}

        bool operator==(CharacterBuffer const& other) const
        {
            Assert(string != nullptr);
            if (this->len != other.len)
            {
                return false;
            }
            return this->string == other.string || StaticEquals(string, other.string, this->len);
        }

        bool HasHashCode() const
        {
            return hashCode != 0;
        }

        void SetHashCode(hash_t code)
        {
            AssertMsg(StaticGetHashCode(string, len) == code, "well... hash code is not matching?");
            hashCode = code;
        }

        hash_t GetHashCodeNoCache() const
        {
            if (hashCode != 0) return hashCode;
            return StaticGetHashCode(string, len);
        }

        operator hash_t() const
        {
            return GetHashCodeNoCache();
        }

        hash_t GetHashCode()
        {
            Assert(string != nullptr);
            if (hashCode) return hashCode;
            hashCode = StaticGetHashCode(string, len);
            return hashCode;
        }

        int FastHash()
        {
            Assert(string != nullptr);
            if (hashCode) return hashCode;
            hashCode = InternalGetHashCode<true>(string, len);
            return hashCode;
        }

        CharacterBuffer& operator=(T const * s)
        {
            Assert(s == nullptr);
            string = nullptr;
            len = (charcount_t)-1;
            return *this;
        }

        static bool StaticEquals(__in_z T const * s1, __in_z T const* s2, __in charcount_t length);

        static int StaticGetHashCode(__in_z T const * s, __in charcount_t length)
        {
            return InternalGetHashCode<false>(s, length);
        }
private:
        // This must be identical to Trident's getHash function in fastDOMCompiler.pl
        template <bool fastHash>
        static int InternalGetHashCode(__in_z T const * s, __in charcount_t length)
        {
            // TODO: This hash performs poorly on small strings, consider finding a better hash function
            // now that some type handlers hash by string instead of PropertyId.
            int hash = 0;
            charcount_t hashLength = length;
            if (fastHash)
            {
                hashLength = min(length, MAX_FAST_HASH_LENGTH);
            }
            for (charcount_t i = 0; i < hashLength; i++)
            {
                CC_HASH_LOGIC(hash, s[i]);
            }
            return hash;
        }
public:
        T const * GetBuffer() const { return string; }
        charcount_t GetLength() const { return len; }
    private:
        Field(T const *) string;
        Field(charcount_t) len;
        Field(hash_t) hashCode;
    };

    template<>
    inline bool
    CharacterBuffer<WCHAR>::StaticEquals(__in_z WCHAR const * s1, __in_z WCHAR const * s2, __in charcount_t length)
    {
        return wmemcmp(s1, s2, length) == 0;
    }

    template<>
    inline bool
    CharacterBuffer<unsigned char>::StaticEquals(__in_z unsigned char const * s1, __in_z unsigned char const *s2, __in charcount_t length)
    {
        return memcmp(s1, s2, length) == 0;
    }
}
