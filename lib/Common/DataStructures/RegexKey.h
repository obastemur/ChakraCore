//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace UnifiedRegex
{
    enum RegexFlags : uint8;

    class RegexKey
    {
    private:
        Field(const char16 *) source;
        Field(int) length;
        Field(RegexFlags) flags;

    public:
        RegexKey() : source(nullptr), length(0), flags(static_cast<RegexFlags>(0))
        {TRACE_IT(21988);
        }

        RegexKey(const char16 *const source, const int length, const RegexFlags flags)
            : source(source), length(length), flags(flags)
        {TRACE_IT(21989);
            Assert(source);
            Assert(length >= 0);
        }

        RegexKey(const RegexKey& other)
            : source(other.source), length(other.length), flags(other.flags)
        {TRACE_IT(21990);}

        RegexKey &operator =(const void *const nullValue)
        {TRACE_IT(21991);
            // Needed to support KeyValueEntry::Clear for dictionaries
            Assert(!nullValue);

            source = nullptr;
            length = 0;
            flags = static_cast<RegexFlags>(0);
            return *this;
        }

        const char16 *Source() const
        {TRACE_IT(21992);
            return source;
        }

        int Length() const
        {TRACE_IT(21993);
            return length;
        }

        RegexFlags Flags() const
        {TRACE_IT(21994);
            return flags;
        }
    };

    struct RegexKeyComparer
    {
        inline static bool Equals(const RegexKey &key1, const RegexKey &key2)
        {TRACE_IT(21995);
            return
                Js::InternalStringComparer::Equals(
                    Js::InternalString(key1.Source(), key1.Length()),
                    Js::InternalString(key2.Source(), key2.Length())) &&
                key1.Flags() == key2.Flags();
        }

        inline static hash_t GetHashCode(const RegexKey &key)
        {TRACE_IT(21996);
            return Js::InternalStringComparer::GetHashCode(Js::InternalString(key.Source(), key.Length()));
        }
    };
}

template<>
struct DefaultComparer<UnifiedRegex::RegexKey> : public UnifiedRegex::RegexKeyComparer
{
};
