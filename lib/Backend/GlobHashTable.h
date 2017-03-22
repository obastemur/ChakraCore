//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if PROFILE_DICTIONARY
#include "DictionaryStats.h"
#endif

template <typename TData, typename TElement>
class HashBucket
{
public:
    TElement element;
    TData   value;

public:
    HashBucket() : element(NULL), value(NULL) {LOGMEIN("GlobHashTable.h] 18\n");}
    static void Copy(HashBucket const& bucket, HashBucket& newBucket)
    {LOGMEIN("GlobHashTable.h] 20\n");
        newBucket.element = bucket.element;
        newBucket.value = bucket.value;
    }
};

class Key
{
public:
    static uint Get(Sym *sym) {LOGMEIN("GlobHashTable.h] 29\n"); return static_cast<uint>(sym->m_id); }
    static uint Get(ExprHash hash) {LOGMEIN("GlobHashTable.h] 30\n"); return static_cast<uint>(hash); }
};

#define FOREACH_GLOBHASHTABLE_ENTRY(bucket, hashTable) \
    for (uint _iterHash = 0; _iterHash < (hashTable)->tableSize; _iterHash++)  \
    {   \
        FOREACH_SLISTBASE_ENTRY(GlobHashBucket, bucket, &(hashTable)->table[_iterHash]) \
        {LOGMEIN("GlobHashTable.h] 37\n");


#define NEXT_GLOBHASHTABLE_ENTRY \
        } \
        NEXT_SLISTBASE_ENTRY; \
    }

template<typename TData, typename TElement>
class ValueHashTable
{
private:
    typedef HashBucket<TData, TElement> HashBucket;

public:
    JitArenaAllocator *        alloc;
    uint                    tableSize;
    SListBase<HashBucket> * table;

public:
    static ValueHashTable * New(JitArenaAllocator *allocator, DECLSPEC_GUARD_OVERFLOW uint tableSize)
    {LOGMEIN("GlobHashTable.h] 58\n");
        return AllocatorNewPlus(JitArenaAllocator, allocator, (tableSize*sizeof(SListBase<HashBucket>)), ValueHashTable, allocator, tableSize);
    }

    void Delete()
    {
        AllocatorDeletePlus(JitArenaAllocator, alloc, (tableSize*sizeof(SListBase<HashBucket>)), this);
    }

    ~ValueHashTable()
    {LOGMEIN("GlobHashTable.h] 68\n");
        for (uint i = 0; i< tableSize; i++)
        {LOGMEIN("GlobHashTable.h] 70\n");
            table[i].Clear(alloc);
        }
    }

    SListBase<HashBucket> * SwapBucket(SListBase<HashBucket> * newTable)
    {LOGMEIN("GlobHashTable.h] 76\n");
        SListBase<HashBucket> * retTable = table;
        table = newTable;
        return retTable;
    }

    TElement * FindOrInsertNew(TData value)
    {LOGMEIN("GlobHashTable.h] 83\n");
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {LOGMEIN("GlobHashTable.h] 92\n");
            if (Key::Get(bucket.value) <= key)
            {LOGMEIN("GlobHashTable.h] 94\n");
                if (Key::Get(bucket.value) == key)
                {LOGMEIN("GlobHashTable.h] 96\n");
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
        newBucket->value = value;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return &newBucket->element;
    }

    TElement * FindOrInsertNewNoThrow(TData * value)
    {LOGMEIN("GlobHashTable.h] 116\n");
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {LOGMEIN("GlobHashTable.h] 125\n");
            if (Key::Get(bucket.value) <= key)
            {LOGMEIN("GlobHashTable.h] 127\n");
                if (Key::Get(bucket.value) == key)
                {LOGMEIN("GlobHashTable.h] 129\n");
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        HashBucket * newBucket = iter.InsertNodeBeforeNoThrow(this->alloc);
        if (newBucket == nullptr)
        {LOGMEIN("GlobHashTable.h] 141\n");
            return nullptr;
        }
        newBucket->value = value;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return &newBucket->element;
    }

    TElement * FindOrInsert(TElement element, TData value)
    {LOGMEIN("GlobHashTable.h] 153\n");
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {LOGMEIN("GlobHashTable.h] 162\n");
            if (Key::Get(bucket.value) <= key)
            {LOGMEIN("GlobHashTable.h] 164\n");
                if (Key::Get(bucket.value) == key)
                {LOGMEIN("GlobHashTable.h] 166\n");
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
        Assert(newBucket != nullptr);
        newBucket->value = value;
        newBucket->element = element;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return NULL;
    }

    TElement * Get(TData value)
    {LOGMEIN("GlobHashTable.h] 188\n");
        uint key = Key::Get(value);
        return Get(key);
    }

    TElement * Get(uint key)
    {LOGMEIN("GlobHashTable.h] 194\n");
        uint hash = this->Hash(key);
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY(HashBucket, bucket, &this->table[hash])
        {LOGMEIN("GlobHashTable.h] 198\n");
            if (Key::Get(bucket.value) <= key)
            {LOGMEIN("GlobHashTable.h] 200\n");
                if (Key::Get(bucket.value) == key)
                {LOGMEIN("GlobHashTable.h] 202\n");
                    return &(bucket.element);
                }
                break;
            }
        } NEXT_SLISTBASE_ENTRY;

        return NULL;
    }

    HashBucket * GetBucket(uint key)
    {LOGMEIN("GlobHashTable.h] 213\n");
        uint hash = this->Hash(key);
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY(HashBucket, bucket, &this->table[hash])
        {LOGMEIN("GlobHashTable.h] 217\n");
            if (Key::Get(bucket.value) <= key)
            {LOGMEIN("GlobHashTable.h] 219\n");
                if (Key::Get(bucket.value) == key)
                {LOGMEIN("GlobHashTable.h] 221\n");
                    return &bucket;
                }
                break;
            }
        } NEXT_SLISTBASE_ENTRY;

        return nullptr;
    }

    TElement GetAndClear(TData * value)
    {LOGMEIN("GlobHashTable.h] 232\n");
        uint key = Key::Get(value);
        uint hash = this->Hash(key);
        SListBase<HashBucket> * list = &this->table[hash];

#if PROFILE_DICTIONARY
        bool first = true;
#endif
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, list, iter)
        {LOGMEIN("GlobHashTable.h] 242\n");
            if (Key::Get(bucket.value) <= key)
            {LOGMEIN("GlobHashTable.h] 244\n");
                if (Key::Get(bucket.value) == key)
                {LOGMEIN("GlobHashTable.h] 246\n");
                    TElement retVal = bucket.element;
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(first && !(iter.Next()));
#endif
                    return retVal;
                }
                break;
            }
#if PROFILE_DICTIONARY
            first = false;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;
        return nullptr;
    }

    void Clear(uint key)
    {LOGMEIN("GlobHashTable.h] 265\n");
        uint hash = this->Hash(key);
        SListBase<HashBucket> * list = &this->table[hash];

        // Assumes sorted lists
#if PROFILE_DICTIONARY
        bool first = true;
#endif
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, list, iter)
        {LOGMEIN("GlobHashTable.h] 274\n");
            if (Key::Get(bucket.value) <= key)
            {LOGMEIN("GlobHashTable.h] 276\n");
                if (Key::Get(bucket.value) == key)
                {LOGMEIN("GlobHashTable.h] 278\n");
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(first && !(iter.Next()));
#endif
                }
                return;
            }
#if PROFILE_DICTIONARY
        first = false;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;
    }

    void And(ValueHashTable *this2)
    {LOGMEIN("GlobHashTable.h] 294\n");
        for (uint i = 0; i < this->tableSize; i++)
        {LOGMEIN("GlobHashTable.h] 296\n");
            _TYPENAME SListBase<HashBucket>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[i], iter)
            {LOGMEIN("GlobHashTable.h] 300\n");
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {LOGMEIN("GlobHashTable.h] 302\n");
                    iter2.Next();
                }

                if (!iter2.IsValid() || bucket.value != iter2.Data().value || bucket.element != iter2.Data().element)
                {LOGMEIN("GlobHashTable.h] 307\n");
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(false);
#endif
                    continue;
                }
                else
                {
                    AssertMsg(bucket.value == iter2.Data().value && bucket.element == iter2.Data().element, "Huh??");
                }
                iter2.Next();
            } NEXT_SLISTBASE_ENTRY_EDITING;
        }
    }

    template <class Fn>
    void Or(ValueHashTable * this2, Fn fn)
    {LOGMEIN("GlobHashTable.h] 326\n");
        for (uint i = 0; i < this->tableSize; i++)
        {LOGMEIN("GlobHashTable.h] 328\n");
            _TYPENAME SListBase<HashBucket>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING((HashBucket), bucket, &this->table[i], iter)
            {LOGMEIN("GlobHashTable.h] 332\n");
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {LOGMEIN("GlobHashTable.h] 334\n");
                    HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
                    newBucket->value = iter2.Data().value;
                    newBucket->element = fn(null, iter2.Data().element);
                    iter2.Next();
                }

                if (!iter2.IsValid())
                {LOGMEIN("GlobHashTable.h] 342\n");
                    break;
                }
                if (bucket.value == iter2.Data().value)
                {LOGMEIN("GlobHashTable.h] 346\n");
                    bucket.element = fn(bucket.element, iter2.Data().element);
                    iter2.Next();
                }
            } NEXT_SLISTBASE_ENTRY_EDITING;

            while (iter2.IsValid())
            {LOGMEIN("GlobHashTable.h] 353\n");
                HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
                newBucket->value = iter2.Data().value;
                newBucket->element = fn(null, iter2.Data().element);
                iter2.Next();
            }
        }
    }

    ValueHashTable *Copy()
    {LOGMEIN("GlobHashTable.h] 363\n");
        ValueHashTable *newTable = ValueHashTable::New(this->alloc, this->tableSize);

        for (uint i = 0; i < this->tableSize; i++)
        {LOGMEIN("GlobHashTable.h] 367\n");
            this->table[i].template CopyTo<HashBucket::Copy>(this->alloc, newTable->table[i]);
        }
#if PROFILE_DICTIONARY
        if (stats)
            newTable->stats = stats->Clone();
#endif
        return newTable;
    }

    void ClearAll()
    {LOGMEIN("GlobHashTable.h] 378\n");
        for (uint i = 0; i < this->tableSize; i++)
        {LOGMEIN("GlobHashTable.h] 380\n");
            this->table[i].Clear(this->alloc);
        }
#if PROFILE_DICTIONARY
        // To not lose previously collected data, we will treat cleared dictionary as a separate instance for stats tracking purpose
        stats = DictionaryStats::Create(typeid(this).name(), tableSize);
#endif
    }

#if DBG_DUMP
    void Dump()
    {
        FOREACH_GLOBHASHTABLE_ENTRY(bucket, this)
        {LOGMEIN("GlobHashTable.h] 393\n");

            Output::Print(_u("%4d  =>  "), bucket.value);
            bucket.element->Dump();
            Output::Print(_u("\n"));
            Output::Print(_u("\n"));
        }
        NEXT_GLOBHASHTABLE_ENTRY;
    }

    void Dump(void (*valueDump)(TData))
    {LOGMEIN("GlobHashTable.h] 404\n");
        Output::Print(_u("\n-------------------------------------------------------------------------------------------------\n"));
        FOREACH_GLOBHASHTABLE_ENTRY(bucket, this)
        {LOGMEIN("GlobHashTable.h] 407\n");
            valueDump(bucket.value);
            Output::Print(_u("  =>  "), bucket.value);
            bucket.element->Dump();
            Output::Print(_u("\n"));
        }
        NEXT_GLOBHASHTABLE_ENTRY;
    }
#endif

protected:
    ValueHashTable(JitArenaAllocator * allocator, uint tableSize) : alloc(allocator), tableSize(tableSize)
    {LOGMEIN("GlobHashTable.h] 419\n");
        Init();
#if PROFILE_DICTIONARY
        stats = DictionaryStats::Create(typeid(this).name(), tableSize);
#endif
    }
    void Init()
    {LOGMEIN("GlobHashTable.h] 426\n");
        table = (SListBase<HashBucket> *)(((char *)this) + sizeof(ValueHashTable));
        for (uint i = 0; i < tableSize; i++)
        {LOGMEIN("GlobHashTable.h] 429\n");
            // placement new
            ::new (&table[i]) SListBase<HashBucket>();
        }
    }
private:
    uint         Hash(uint key) {LOGMEIN("GlobHashTable.h] 435\n"); return (key % this->tableSize); }

#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
};

