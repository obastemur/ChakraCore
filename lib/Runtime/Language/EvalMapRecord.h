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
        {LOGMEIN("EvalMapRecord.h] 15\n");
            return (left.hash == right.hash) &&
                (left.moduleID == right.moduleID) &&
                (left.IsStrict() == right.IsStrict());
        }

        static hash_t GetHashCode(T t)
        {LOGMEIN("EvalMapRecord.h] 22\n");
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
            singleValue(true), value(nullptr) {LOGMEIN("EvalMapRecord.h] 37\n");}

        SecondaryDictionary* GetDictionary()
        {LOGMEIN("EvalMapRecord.h] 40\n");
            Assert(!singleValue);
            return nestedMap;
        }

        bool TryGetValue(TKey& key, TValue* value)
        {LOGMEIN("EvalMapRecord.h] 46\n");
            if (IsValue())
            {LOGMEIN("EvalMapRecord.h] 48\n");
                *value = GetValue();
                return true;
            }
            return GetDictionary()->TryGetValue(key, value);
        }

        void Add(const TKey& key, const TValue& newValue)
        {LOGMEIN("EvalMapRecord.h] 56\n");
            Assert(!singleValue);
            NestedKey nestedKey;
            ConvertKey(key, nestedKey);
            nestedMap->Item(nestedKey, newValue);
#ifdef PROFILE_EVALMAP
            if (Configuration::Global.flags.ProfileEvalMap)
            {LOGMEIN("EvalMapRecord.h] 63\n");
                Output::Print(_u("EvalMap fastcache collision:\t key = %d count = %d\n"), (hash_t)key, nestedMap->Count());
            }
#endif
        }

        void Remove(const TKey& key)
        {LOGMEIN("EvalMapRecord.h] 70\n");
            Assert(!singleValue);
            NestedKey nestedKey;
            ConvertKey(key, nestedKey);
            nestedMap->Remove(nestedKey);
        }

        void ConvertToDictionary(TKey& key, Recycler* recycler)
        {LOGMEIN("EvalMapRecord.h] 78\n");
            Assert(singleValue);
            SecondaryDictionary* dictionary = RecyclerNew(recycler, SecondaryDictionary, recycler);
            auto newValue = value;
            nestedMap = dictionary;
            singleValue = false;
            Add(key, newValue);
        }

        bool IsValue() const {LOGMEIN("EvalMapRecord.h] 87\n"); return singleValue; }
        TValue GetValue() const {LOGMEIN("EvalMapRecord.h] 88\n"); Assert(singleValue); return value; }
        bool IsDictionaryEntry() const {LOGMEIN("EvalMapRecord.h] 89\n"); return !singleValue; }

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
            {LOGMEIN("EvalMapRecord.h] 111\n");
                instance->SetIsInAdd(value);
            }
            ~AutoRestoreSetInAdd()
            {LOGMEIN("EvalMapRecord.h] 115\n");
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
        {LOGMEIN("EvalMapRecord.h] 128\n");
        }

        bool TryGetValue(const Key& key, Value* value)
        {LOGMEIN("EvalMapRecord.h] 132\n");
            EntryRecord* const * entryRecord;
            Key cachedKey;
            int index;
            bool success = dictionary->TryGetReference(key, &entryRecord, &index);
            if (success && ((*entryRecord) != nullptr))
            {LOGMEIN("EvalMapRecord.h] 138\n");
                cachedKey = dictionary->GetKeyAt(index);
                if ((*entryRecord)->IsValue())
                {LOGMEIN("EvalMapRecord.h] 141\n");
                    success = (cachedKey == key);
                    if (success)
                    {LOGMEIN("EvalMapRecord.h] 144\n");
                        *value = (*entryRecord)->GetValue();
                    }
                }
                else
                {
                    NestedKey nestedKey;
                    ConvertKey(key, nestedKey);
                    success = (*entryRecord)->GetDictionary()->TryGetValue(nestedKey, value);
                }
            }
            else
            {
                success = false;
            }
            return success;
        }

        TopLevelDictionary* GetDictionary() const {LOGMEIN("EvalMapRecord.h] 162\n"); return dictionary; }
        void NotifyAdd(const Key& key)
        {LOGMEIN("EvalMapRecord.h] 164\n");
            dictionary->NotifyAdd(key);
        }

        void Add(const Key& key, Value value)
        {LOGMEIN("EvalMapRecord.h] 169\n");
            EntryRecord* const * entryRecord;
            int index;
            bool success = dictionary->TryGetReference(key, &entryRecord, &index);
            if (success && ((*entryRecord) != nullptr))
            {
                AutoRestoreSetInAdd<TopLevelDictionary, bool> autoRestoreSetInAdd(this->dictionary, true);
                if ((*entryRecord)->IsValue())
                {LOGMEIN("EvalMapRecord.h] 177\n");
                    Key oldKey = dictionary->GetKeyAt(index);
                    (*entryRecord)->ConvertToDictionary(oldKey, recycler);
                }
                (*entryRecord)->Add(key, value);
            }
            else
            {
                EntryRecord* newRecord = RecyclerNew(recycler, EntryRecord, value);
                dictionary->Add(key, newRecord);
#ifdef PROFILE_EVALMAP
                if (Configuration::Global.flags.ProfileEvalMap)
                {LOGMEIN("EvalMapRecord.h] 189\n");
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
