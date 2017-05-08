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
    HashBucket() : element(NULL), value(NULL) {TRACE_IT(2882);}
    static void Copy(HashBucket const& bucket, HashBucket& newBucket)
    {TRACE_IT(2883);
        newBucket.element = bucket.element;
        newBucket.value = bucket.value;
    }
};

class Key
{
public:
    static uint Get(Sym *sym) {TRACE_IT(2884); return static_cast<uint>(sym->m_id); }
    static uint Get(ExprHash hash) {TRACE_IT(2885); return static_cast<uint>(hash); }
};

#define FOREACH_GLOBHASHTABLE_ENTRY(bucket, hashTable) \
    for (uint _iterHash = 0; _iterHash < (hashTable)->tableSize; _iterHash++)  \
    {   \
        FOREACH_SLISTBASE_ENTRY(GlobHashBucket, bucket, &(hashTable)->table[_iterHash]) \
        {TRACE_IT(2886);


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
    {TRACE_IT(2887);
        return AllocatorNewPlus(JitArenaAllocator, allocator, (tableSize*sizeof(SListBase<HashBucket>)), ValueHashTable, allocator, tableSize);
    }

    void Delete()
    {
        AllocatorDeletePlus(JitArenaAllocator, alloc, (tableSize*sizeof(SListBase<HashBucket>)), this);
    }

    ~ValueHashTable()
    {TRACE_IT(2888);
        for (uint i = 0; i< tableSize; i++)
        {TRACE_IT(2889);
            table[i].Clear(alloc);
        }
    }

    SListBase<HashBucket> * SwapBucket(SListBase<HashBucket> * newTable)
    {TRACE_IT(2890);
        SListBase<HashBucket> * retTable = table;
        table = newTable;
        return retTable;
    }

    TElement * FindOrInsertNew(TData value)
    {TRACE_IT(2891);
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {TRACE_IT(2892);
            if (Key::Get(bucket.value) <= key)
            {TRACE_IT(2893);
                if (Key::Get(bucket.value) == key)
                {TRACE_IT(2894);
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
    {TRACE_IT(2895);
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {TRACE_IT(2896);
            if (Key::Get(bucket.value) <= key)
            {TRACE_IT(2897);
                if (Key::Get(bucket.value) == key)
                {TRACE_IT(2898);
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
        {TRACE_IT(2899);
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
    {TRACE_IT(2900);
        uint key = Key::Get(value);
        uint hash = this->Hash(key);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[hash], iter)
        {TRACE_IT(2901);
            if (Key::Get(bucket.value) <= key)
            {TRACE_IT(2902);
                if (Key::Get(bucket.value) == key)
                {TRACE_IT(2903);
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
    {TRACE_IT(2904);
        uint key = Key::Get(value);
        return Get(key);
    }

    TElement * Get(uint key)
    {TRACE_IT(2905);
        uint hash = this->Hash(key);
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY(HashBucket, bucket, &this->table[hash])
        {TRACE_IT(2906);
            if (Key::Get(bucket.value) <= key)
            {TRACE_IT(2907);
                if (Key::Get(bucket.value) == key)
                {TRACE_IT(2908);
                    return &(bucket.element);
                }
                break;
            }
        } NEXT_SLISTBASE_ENTRY;

        return NULL;
    }

    HashBucket * GetBucket(uint key)
    {TRACE_IT(2909);
        uint hash = this->Hash(key);
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY(HashBucket, bucket, &this->table[hash])
        {TRACE_IT(2910);
            if (Key::Get(bucket.value) <= key)
            {TRACE_IT(2911);
                if (Key::Get(bucket.value) == key)
                {TRACE_IT(2912);
                    return &bucket;
                }
                break;
            }
        } NEXT_SLISTBASE_ENTRY;

        return nullptr;
    }

    TElement GetAndClear(TData * value)
    {TRACE_IT(2913);
        uint key = Key::Get(value);
        uint hash = this->Hash(key);
        SListBase<HashBucket> * list = &this->table[hash];

#if PROFILE_DICTIONARY
        bool first = true;
#endif
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, list, iter)
        {TRACE_IT(2914);
            if (Key::Get(bucket.value) <= key)
            {TRACE_IT(2915);
                if (Key::Get(bucket.value) == key)
                {TRACE_IT(2916);
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
    {TRACE_IT(2917);
        uint hash = this->Hash(key);
        SListBase<HashBucket> * list = &this->table[hash];

        // Assumes sorted lists
#if PROFILE_DICTIONARY
        bool first = true;
#endif
        FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, list, iter)
        {TRACE_IT(2918);
            if (Key::Get(bucket.value) <= key)
            {TRACE_IT(2919);
                if (Key::Get(bucket.value) == key)
                {TRACE_IT(2920);
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
    {TRACE_IT(2921);
        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(2922);
            _TYPENAME SListBase<HashBucket>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(HashBucket, bucket, &this->table[i], iter)
            {TRACE_IT(2923);
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {TRACE_IT(2924);
                    iter2.Next();
                }

                if (!iter2.IsValid() || bucket.value != iter2.Data().value || bucket.element != iter2.Data().element)
                {TRACE_IT(2925);
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(false);
#endif
                    continue;
                }
                else
                {TRACE_IT(2926);
                    AssertMsg(bucket.value == iter2.Data().value && bucket.element == iter2.Data().element, "Huh??");
                }
                iter2.Next();
            } NEXT_SLISTBASE_ENTRY_EDITING;
        }
    }

    template <class Fn>
    void Or(ValueHashTable * this2, Fn fn)
    {TRACE_IT(2927);
        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(2928);
            _TYPENAME SListBase<HashBucket>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING((HashBucket), bucket, &this->table[i], iter)
            {TRACE_IT(2929);
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {TRACE_IT(2930);
                    HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
                    newBucket->value = iter2.Data().value;
                    newBucket->element = fn(null, iter2.Data().element);
                    iter2.Next();
                }

                if (!iter2.IsValid())
                {TRACE_IT(2931);
                    break;
                }
                if (bucket.value == iter2.Data().value)
                {TRACE_IT(2932);
                    bucket.element = fn(bucket.element, iter2.Data().element);
                    iter2.Next();
                }
            } NEXT_SLISTBASE_ENTRY_EDITING;

            while (iter2.IsValid())
            {TRACE_IT(2933);
                HashBucket * newBucket = iter.InsertNodeBefore(this->alloc);
                newBucket->value = iter2.Data().value;
                newBucket->element = fn(null, iter2.Data().element);
                iter2.Next();
            }
        }
    }

    ValueHashTable *Copy()
    {TRACE_IT(2934);
        ValueHashTable *newTable = ValueHashTable::New(this->alloc, this->tableSize);

        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(2935);
            this->table[i].template CopyTo<HashBucket::Copy>(this->alloc, newTable->table[i]);
        }
#if PROFILE_DICTIONARY
        if (stats)
            newTable->stats = stats->Clone();
#endif
        return newTable;
    }

    void ClearAll()
    {TRACE_IT(2936);
        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(2937);
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
        {TRACE_IT(2938);

            Output::Print(_u("%4d  =>  "), bucket.value);
            bucket.element->Dump();
            Output::Print(_u("\n"));
            Output::Print(_u("\n"));
        }
        NEXT_GLOBHASHTABLE_ENTRY;
    }

    void Dump(void (*valueDump)(TData))
    {TRACE_IT(2939);
        Output::Print(_u("\n-------------------------------------------------------------------------------------------------\n"));
        FOREACH_GLOBHASHTABLE_ENTRY(bucket, this)
        {TRACE_IT(2940);
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
    {TRACE_IT(2941);
        Init();
#if PROFILE_DICTIONARY
        stats = DictionaryStats::Create(typeid(this).name(), tableSize);
#endif
    }
    void Init()
    {TRACE_IT(2942);
        table = (SListBase<HashBucket> *)(((char *)this) + sizeof(ValueHashTable));
        for (uint i = 0; i < tableSize; i++)
        {TRACE_IT(2943);
            // placement new
            ::new (&table[i]) SListBase<HashBucket>();
        }
    }
private:
    uint         Hash(uint key) {TRACE_IT(2944); return (key % this->tableSize); }

#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
};

