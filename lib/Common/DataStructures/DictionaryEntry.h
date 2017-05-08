//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace JsUtil
{
    template <class TValue>
    class BaseValueEntry
    {
    protected:
        TValue value;        // data of entry
        void Set(TValue const& value)
        {TRACE_IT(21200);
            this->value = value;
        }

    public:
        int next;        // Index of next entry, -1 if last

        static bool SupportsCleanup()
        {TRACE_IT(21201);
            return false;
        }

        static bool NeedsCleanup(BaseValueEntry<TValue>&)
        {TRACE_IT(21202);
            return false;
        }

        TValue const& Value() const {TRACE_IT(21203); return value; }
        TValue& Value() {TRACE_IT(21204); return value; }
        void SetValue(TValue const& value) {TRACE_IT(21205); this->value = value; }
    };

    template <class TValue>
    class ValueEntry: public BaseValueEntry<TValue>
    {
    public:
        void Clear()
        {TRACE_IT(21206);
        }
    };

    // Class specialization for pointer values to support clearing
    template <class TValue>
    class ValueEntry<TValue*>: public BaseValueEntry<TValue*>
    {
    public:
        void Clear()
        {TRACE_IT(21207);
            this->value = nullptr;
        }
    };

    template <>
    class ValueEntry<bool>: public BaseValueEntry<bool>
    {
    public:
        void Clear()
        {TRACE_IT(21208);
            this->value = false;
        }
    };

    template <>
    class ValueEntry<int>: public BaseValueEntry<int>
    {
    public:
        void Clear()
        {TRACE_IT(21209);
            this->value = 0;
        }
    };

    template <>
    class ValueEntry<uint>: public BaseValueEntry<uint>
    {
    public:
        void Clear()
        {TRACE_IT(21210);
            this->value = 0;
        }
    };

    template<class TKey, class TValue>
    struct ValueToKey
    {
        static TKey ToKey(const TValue &value) {TRACE_IT(21211); return static_cast<TKey>(value); }
    };

    // Used by BaseHashSet,  the default is that the key is the same as the value
    template <class TKey, class TValue>
    class ImplicitKeyValueEntry : public ValueEntry<TValue>
    {
    public:
        inline TKey Key() const {TRACE_IT(21212); return ValueToKey<TKey, TValue>::ToKey(this->value); }

        void Set(TKey const& key, TValue const& value)
        {TRACE_IT(21213);
            __super::Set(value);
        }
    };

    template <class TKey, class TValue>
    class BaseKeyValueEntry : public ValueEntry<TValue>
    {
    protected:
        TKey key;    // key of entry
        void Set(TKey const& key, TValue const& value)
        {TRACE_IT(21214);
            __super::Set(value);
            this->key = key;
        }

    public:
        TKey const& Key() const  {TRACE_IT(21215); return key; }
    };

    template <class TKey, class TValue>
    class KeyValueEntry : public BaseKeyValueEntry<TKey, TValue>
    {
    };

    template <class TKey, class TValue>
    class KeyValueEntry<TKey*, TValue> : public BaseKeyValueEntry<TKey*, TValue>
    {
    public:
        void Clear()
        {TRACE_IT(21216);
            __super::Clear();
            this->key = nullptr;
        }
    };

    template <class TValue>
    class KeyValueEntry<int, TValue> : public BaseKeyValueEntry<int, TValue>
    {
    public:
        void Clear()
        {TRACE_IT(21217);
            __super::Clear();
            this->key = 0;
        }
    };

    template <class TKey, class TValue, template <class K, class V> class THashEntry>
    class DefaultHashedEntry : public THashEntry<TKey, TValue>
    {
    public:
        template<typename Comparer, typename TLookup>
        inline bool KeyEquals(TLookup const& otherKey, hash_t otherHashCode)
        {TRACE_IT(21218);
            return Comparer::Equals(this->Key(), otherKey);
        }

        template<typename Comparer>
        inline hash_t GetHashCode()
        {TRACE_IT(21219);
            return ((Comparer::GetHashCode(this->Key()) & 0x7fffffff) << 1) | 1;
        }

        void Set(TKey const& key, TValue const& value, int hashCode)
        {TRACE_IT(21220);
            __super::Set(key, value);
        }
    };

    template <class TKey, class TValue, template <class K, class V> class THashEntry>
    class CacheHashedEntry : public THashEntry<TKey, TValue>
    {
        hash_t hashCode;    // Lower 31 bits of hash code << 1 | 1, 0 if unused
    public:
        static const int INVALID_HASH_VALUE = 0;
        template<typename Comparer, typename TLookup>
        inline bool KeyEquals(TLookup const& otherKey, hash_t otherHashCode)
        {TRACE_IT(21221);
            Assert(TAGHASH(Comparer::GetHashCode(this->Key())) == this->hashCode);
            return this->hashCode == otherHashCode && Comparer::Equals(this->Key(), otherKey);
        }

        template<typename Comparer>
        inline hash_t GetHashCode()
        {TRACE_IT(21222);
            Assert(TAGHASH(Comparer::GetHashCode(this->Key())) == this->hashCode);
            return hashCode;
        }

        void Set(TKey const& key, TValue const& value, hash_t hashCode)
        {TRACE_IT(21223);
            __super::Set(key, value);
            this->hashCode = hashCode;
        }

        void Clear()
        {TRACE_IT(21224);
            __super::Clear();
            this->hashCode = INVALID_HASH_VALUE;
        }
    };

    template <class TKey, class TValue>
    class SimpleHashedEntry : public DefaultHashedEntry<TKey, TValue, ImplicitKeyValueEntry> {};

    template <class TKey, class TValue>
    class HashedEntry : public CacheHashedEntry<TKey, TValue, ImplicitKeyValueEntry> {};

    template <class TKey, class TValue>
    class SimpleDictionaryEntry : public DefaultHashedEntry<TKey, TValue, KeyValueEntry>  {};

    template <class TKey, class TValue>
    class DictionaryEntry: public CacheHashedEntry<TKey, TValue, KeyValueEntry> {};

    template <class TKey, class TValue>
    class WeakRefValueDictionaryEntry: public SimpleDictionaryEntry<TKey, TValue>
    {
    public:
        void Clear()
        {TRACE_IT(21225);
            this->key = TKey();
            this->value = TValue();
        }

        static bool SupportsCleanup()
        {TRACE_IT(21226);
            return true;
        }

        static bool NeedsCleanup(WeakRefValueDictionaryEntry<TKey, TValue> const& entry)
        {TRACE_IT(21227);
            TValue weakReference = entry.Value();

            return (weakReference == nullptr || weakReference->Get() == nullptr);
        }
    };
}
