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
        CharacterBuffer() : string(nullptr), len((charcount_t)-1) {TRACE_IT(20948);}
        CharacterBuffer(T const * string, charcount_t len) : string(string), len(len) {TRACE_IT(20949);}
        CharacterBuffer(const CharacterBuffer& other) : string(other.string), len(other.len) {TRACE_IT(20950);}

        bool operator==(CharacterBuffer const& other) const
        {TRACE_IT(20951);
            Assert(string != nullptr);
            if (this->len != other.len)
            {TRACE_IT(20952);
                return false;
            }
            return this->string == other.string || StaticEquals(string, other.string, this->len);
        }

        operator hash_t() const
        {TRACE_IT(20953);
            Assert(string != nullptr);
            return StaticGetHashCode(string, len);
        }

        int FastHash() const
        {TRACE_IT(20954);
            Assert(string != nullptr);
            return InternalGetHashCode<true>(string, len);
        }

        CharacterBuffer& operator=(T const * s)
        {TRACE_IT(20955);
            Assert(s == nullptr);
            string = nullptr;
            len = (charcount_t)-1;
            return *this;
        }

        static bool StaticEquals(__in_z T const * s1, __in_z T const* s2, __in charcount_t length);

        static int StaticGetHashCode(__in_z T const * s, __in charcount_t length)
        {TRACE_IT(20956);
            return InternalGetHashCode<false>(s, length);
        }

        // This must be identical to Trident's getHash function in fastDOMCompiler.pl
        template <bool fastHash>
        static int InternalGetHashCode(__in_z T const * s, __in charcount_t length)
        {TRACE_IT(20957);
            // TODO: This hash performs poorly on small strings, consider finding a better hash function
            // now that some type handlers hash by string instead of PropertyId.
            int hash = 0;
            charcount_t hashLength = length;
            if (fastHash)
            {TRACE_IT(20958);
                hashLength = min(length, MAX_FAST_HASH_LENGTH);
            }
            for (charcount_t i = 0; i < hashLength; i++)
            {TRACE_IT(20959);
                hash = _rotl(hash, 7);
                hash ^= s[i];
            }
            return hash;
        }

        T const * GetBuffer() const {TRACE_IT(20960); return string; }
        charcount_t GetLength() const {TRACE_IT(20961); return len; }
    private:
        Field(T const *) string;
        Field(charcount_t) len;
    };

    template<>
    inline bool
    CharacterBuffer<WCHAR>::StaticEquals(__in_z WCHAR const * s1, __in_z WCHAR const * s2, __in charcount_t length)
    {TRACE_IT(20962);
        return wmemcmp(s1, s2, length) == 0;
    }

    template<>
    inline bool
    CharacterBuffer<unsigned char>::StaticEquals(__in_z unsigned char const * s1, __in_z unsigned char const *s2, __in charcount_t length)
    {TRACE_IT(20963);
        return memcmp(s1, s2, length) == 0;
    }
}
