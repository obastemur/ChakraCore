//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

namespace Js
{
    // Use as the top level comparer for two level dictionary. Note that two
    // values are equal as long as their fastHash is the same (and moduleID/isStrict is the same).
    // This comparer is used for the top level dictionary in two level evalmap dictionary.
    template <class T>
    struct FastEvalMapStringComparer
    {
        static bool Equals(T left, T right)
        {TRACE_IT(48136);
            return (left.hash == right.hash) &&
                (left.moduleID == right.moduleID) &&
                (left.IsStrict() == right.IsStrict());
        }

        static hash_t GetHashCode(T t)
        {TRACE_IT(48137);
            return (hash_t)t;
        }
    };

    // The value in top level of two level dictionary. It might contain only the single value
    // (TValue), or a second level dictionary.
    template <class TKey, class TValue, class SecondaryDictionary, class NestedKey>
    class TwoLevelHashRecord
    {
    public:
        TwoLevelHashRecord(TValue newValue) :
            singleValue(true), value(newValue) {}

        TwoLevelHashRecord() :
            singleValue(true), value(nullptr) {TRACE_IT(48138);}

        SecondaryDictionary* GetDictionary()
        {TRACE_IT(48139);
            Assert(!singleValue);
            return nestedMap;
        }

        bool TryGetValue(TKey& key, TValue* value)
        {TRACE_IT(48140);
            if (IsValue())
            {TRACE_IT(48141);
                *value = GetValue();
                return true;
            }
            return GetDictionary()->TryGetValue(key, value);
        }

        void Add(const TKey& key, const TValue& newValue)
        {TRACE_IT(48142);
            Assert(!singleValue);
            NestedKey nestedKey;
            ConvertKey(key, nestedKey);
            nestedMap->Item(nestedKey, newValue);
#ifdef PROFILE_EVALMAP
            if (Configuration::Global.flags.ProfileEvalMap)
            {TRACE_IT(48143);
                Output::Print(_u("EvalMap fastcache collision:\t key = %d count = %d\n"), (hash_t)key, nestedMap->Count());
            }
#endif
        }

        void Remove(const TKey& key)
        {TRACE_IT(48144);
            Assert(!singleValue);
            NestedKey nestedKey;
            ConvertKey(key, nestedKey);
            nestedMap->Remove(nestedKey);
        }

        void ConvertToDictionary(TKey& key, Recycler* recycler)
        {TRACE_IT(48145);
            Assert(singleValue);
            SecondaryDictionary* dictionary = RecyclerNew(recycler, SecondaryDictionary, recycler);
            auto newValue = value;
            nestedMap = dictionary;
            singleValue = false;
            Add(key, newValue);
        }

        bool IsValue() const {TRACE_IT(48146); return singleValue; }
        TValue GetValue() const {TRACE_IT(48147); Assert(singleValue); return value; }
        bool IsDictionaryEntry() const {TRACE_IT(48148); return !singleValue; }

    private:
        Field(bool) singleValue;
        union
        {
            Field(TValue) value;
            Field(SecondaryDictionary*) nestedMap;
        };
    };

    // The two level dictionary. top level needs to be either simple hash value, or
    // key needs to be equals for all nested values.
    template <class Key, class Value, class EntryRecord, class TopLevelDictionary, class NestedKey>
    class TwoLevelHashDictionary
    {
        template <class T, class Value>
        class AutoRestoreSetInAdd
        {
        public:
            AutoRestoreSetInAdd(T* instance, Value value) :
                instance(instance), value(value)
            {TRACE_IT(48149);
                instance->SetIsInAdd(value);
            }
            ~AutoRestoreSetInAdd()
            {TRACE_IT(48150);
                instance->SetIsInAdd(!value);
            }

        private:
            T* instance;
            Value value;
        };

    public:
        TwoLevelHashDictionary(TopLevelDictionary* cache, Recycler* recycler) :
            dictionary(cache),
            recycler(recycler)
        {TRACE_IT(48151);
        }

        bool TryGetValue(const Key& key, Value* value)
        {TRACE_IT(48152);
            EntryRecord* const * entryRecord;
            Key cachedKey;
            int index;
            bool success = dictionary->TryGetReference(key, &entryRecord, &index);
            if (success && ((*entryRecord) != nullptr))
            {TRACE_IT(48153);
                cachedKey = dictionary->GetKeyAt(index);
                if ((*entryRecord)->IsValue())
                {TRACE_IT(48154);
                    success = (cachedKey == key);
                    if (success)
                    {TRACE_IT(48155);
                        *value = (*entryRecord)->GetValue();
                    }
                }
                else
                {TRACE_IT(48156);
                    NestedKey nestedKey;
                    ConvertKey(key, nestedKey);
                    success = (*entryRecord)->GetDictionary()->TryGetValue(nestedKey, value);
                }
            }
            else
            {TRACE_IT(48157);
                success = false;
            }
            return success;
        }

        TopLevelDictionary* GetDictionary() const {TRACE_IT(48158); return dictionary; }
        void NotifyAdd(const Key& key)
        {TRACE_IT(48159);
            dictionary->NotifyAdd(key);
        }

        void Add(const Key& key, Value value)
        {TRACE_IT(48160);
            EntryRecord* const * entryRecord;
            int index;
            bool success = dictionary->TryGetReference(key, &entryRecord, &index);
            if (success && ((*entryRecord) != nullptr))
            {
                AutoRestoreSetInAdd<TopLevelDictionary, bool> autoRestoreSetInAdd(this->dictionary, true);
                if ((*entryRecord)->IsValue())
                {TRACE_IT(48161);
                    Key oldKey = dictionary->GetKeyAt(index);
                    (*entryRecord)->ConvertToDictionary(oldKey, recycler);
                }
                (*entryRecord)->Add(key, value);
            }
            else
            {TRACE_IT(48162);
                EntryRecord* newRecord = RecyclerNew(recycler, EntryRecord, value);
                dictionary->Add(key, newRecord);
#ifdef PROFILE_EVALMAP
                if (Configuration::Global.flags.ProfileEvalMap)
                {TRACE_IT(48163);
                    Output::Print(_u("EvalMap fastcache set:\t key = %d \n"), (hash_t)key);
                }
#endif
            }
        }

    private:
        Field(TopLevelDictionary*) dictionary;
        FieldNoBarrier(Recycler*) recycler;
    };
}
