//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------
#pragma once

#if PROFILE_DICTIONARY
#include "DictionaryStats.h"
#endif

template<typename T>
class Bucket
{
public:
    T       element;
    int     value;

public:
    Bucket() : element(), value(0) {TRACE_IT(21495);}
    static void Copy(Bucket<T> const& bucket, Bucket<T>& newBucket)
    {TRACE_IT(21496);
        bucket.element.Copy(&(newBucket.element));
        newBucket.value = bucket.value;
    }
};

template<typename T, typename TAllocator = ArenaAllocator>
class HashTable
{
public:
    TAllocator *        alloc;
    uint                tableSize;
    SListBase<Bucket<T>> *  table;

public:
    static HashTable<T, TAllocator> * New(TAllocator *allocator, DECLSPEC_GUARD_OVERFLOW uint tableSize)
    {TRACE_IT(21497);
        return AllocatorNewPlus(TAllocator, allocator, (tableSize*sizeof(SListBase<Bucket<T>>)), HashTable, allocator, tableSize);
    }

    void Delete()
    {
        AllocatorDeletePlus(TAllocator, alloc, (tableSize*sizeof(SListBase<Bucket<T>>)), this);
    }

    ~HashTable()
    {TRACE_IT(21498);
        for (uint i = 0; i< tableSize; i++)
        {TRACE_IT(21499);
            table[i].Clear(alloc);
        }
    }

    SListBase<Bucket<T>> * SwapBucket(SListBase<Bucket<T>> * newTable)
    {TRACE_IT(21500);
        SListBase<Bucket<T>> * retTable = table;
        table = newTable;
        return retTable;
    }

    T * FindOrInsertNew(int value)
    {TRACE_IT(21501);
        uint hash = this->Hash(value);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[hash], iter)
        {TRACE_IT(21502);
            if (bucket.value <= value)
            {TRACE_IT(21503);
                if (bucket.value == value)
                {TRACE_IT(21504);
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        Bucket<T> * newBucket = iter.InsertNodeBefore(this->alloc);
        newBucket->value = value;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return &newBucket->element;
    }

    T * FindOrInsertNewNoThrow(int value)
    {TRACE_IT(21505);
        uint hash = this->Hash(value);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[hash], iter)
        {TRACE_IT(21506);
            if (bucket.value <= value)
            {TRACE_IT(21507);
                if (bucket.value == value)
                {TRACE_IT(21508);
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        Bucket<T> * newBucket = iter.InsertNodeBeforeNoThrow(this->alloc);
        if (newBucket == nullptr)
        {TRACE_IT(21509);
            return nullptr;
        }
        newBucket->value = value;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return &newBucket->element;
    }

    T * FindOrInsert(T element, int value)
    {TRACE_IT(21510);
        uint hash = this->Hash(value);

#if PROFILE_DICTIONARY
        uint depth = 1;
#endif
        // Keep sorted
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[hash], iter)
        {TRACE_IT(21511);
            if (bucket.value <= value)
            {TRACE_IT(21512);
                if (bucket.value == value)
                {TRACE_IT(21513);
                    return &(bucket.element);
                }
                break;
            }
#if PROFILE_DICTIONARY
            ++depth;
#endif
        } NEXT_SLISTBASE_ENTRY_EDITING;

        Bucket<T> * newBucket = iter.InsertNodeBefore(this->alloc);
        Assert(newBucket != nullptr);
        newBucket->value = value;
        newBucket->element = element;
#if PROFILE_DICTIONARY
        if (stats)
            stats->Insert(depth);
#endif
        return nullptr;
    }

    T * Get(int value)
    {
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY(Bucket<T>, bucket, &this->table[this->Hash(value)])
        {TRACE_IT(21514);
            if (bucket.value <= value)
            {TRACE_IT(21515);
                if (bucket.value == value)
                {TRACE_IT(21516);
                    return &(bucket.element);
                }
                break;
            }
        } NEXT_SLISTBASE_ENTRY;

        return nullptr;
    }

    T GetAndClear(int value)
    {TRACE_IT(21517);
        SListBase<Bucket<T>> * list = &this->table[this->Hash(value)];

#if PROFILE_DICTIONARY
        bool first = true;
#endif
        // Assumes sorted lists
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, list, iter)
        {TRACE_IT(21518);
            if (bucket.value <= value)
            {TRACE_IT(21519);
                if (bucket.value == value)
                {TRACE_IT(21520);
                    T retVal = bucket.element;
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
        return T();
    }

    void Clear(int value)
    {TRACE_IT(21521);
        SListBase<Bucket<T>> * list = &this->table[this->Hash(value)];

        // Assumes sorted lists
#if PROFILE_DICTIONARY
        bool first = true;
#endif
        FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, list, iter)
        {TRACE_IT(21522);
            if (bucket.value <= value)
            {TRACE_IT(21523);
                if (bucket.value == value)
                {TRACE_IT(21524);
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

    void And(HashTable<T> *this2)
    {TRACE_IT(21525);
        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(21526);
            _TYPENAME SListBase<Bucket<T>>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[i], iter)
            {TRACE_IT(21527);
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {TRACE_IT(21528);
                    iter2.Next();
                }

                if (!iter2.IsValid() || bucket.value != iter2.Data().value || bucket.element != iter2.Data().element)
                {TRACE_IT(21529);
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(false);
#endif
                    continue;
                }
                else
                {TRACE_IT(21530);
                    AssertMsg(bucket.value == iter2.Data().value && bucket.element == iter2.Data().element, "Huh??");
                }
                iter2.Next();
            } NEXT_SLISTBASE_ENTRY_EDITING;
        }
    }

    // "And" with fixup actions to take when data don't make it to the result.
    template <class FnFrom, class FnTo>
    void AndWithFixup(HashTable<T> *this2, FnFrom fnFixupFrom, FnTo fnFixupTo)
    {TRACE_IT(21531);
        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(21532);
            _TYPENAME SListBase<Bucket<T>>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[i], iter)
            {TRACE_IT(21533);
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {TRACE_IT(21534);
                    // Skipping a this2 value.
                    fnFixupTo(iter2.Data());
                    iter2.Next();
                }

                if (!iter2.IsValid() || bucket.value != iter2.Data().value || bucket.element != iter2.Data().element)
                {TRACE_IT(21535);
                    // Skipping a this value.
                    fnFixupFrom(bucket);
                    iter.RemoveCurrent(this->alloc);
#if PROFILE_DICTIONARY
                    if (stats)
                        stats->Remove(false);
#endif
                    continue;
                }
                else
                {TRACE_IT(21536);
                    AssertMsg(bucket.value == iter2.Data().value && bucket.element == iter2.Data().element, "Huh??");
                }
                iter2.Next();
            } NEXT_SLISTBASE_ENTRY_EDITING;
            while (iter2.IsValid())
            {TRACE_IT(21537);
                // Skipping a this2 value.
                fnFixupTo(iter2.Data());
                iter2.Next();
            }
        }
    }

    template <class Fn>
    void Or(HashTable<T> * this2, Fn fn)
    {TRACE_IT(21538);
        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(21539);
            _TYPENAME SListBase<Bucket<T>>::Iterator iter2(&this2->table[i]);
            iter2.Next();
            FOREACH_SLISTBASE_ENTRY_EDITING(Bucket<T>, bucket, &this->table[i], iter)
            {TRACE_IT(21540);
                while (iter2.IsValid() && bucket.value < iter2.Data().value)
                {TRACE_IT(21541);
                    Bucket<T> * newBucket = iter.InsertNodeBefore(this->alloc);
                    newBucket->value = iter2.Data().value;
                    newBucket->element = fn(nullptr, iter2.Data().element);
                    iter2.Next();
                }

                if (!iter2.IsValid())
                {TRACE_IT(21542);
                    break;
                }
                if (bucket.value == iter2.Data().value)
                {TRACE_IT(21543);
                    bucket.element = fn(bucket.element, iter2.Data().element);
                    iter2.Next();
                }
            } NEXT_SLISTBASE_ENTRY_EDITING;

            while (iter2.IsValid())
            {TRACE_IT(21544);
                Bucket<T> * newBucket = iter.InsertNodeBefore(this->alloc);
                newBucket->value = iter2.Data().value;
                newBucket->element = fn(nullptr, iter2.Data().element);
                iter2.Next();
            }
        }
    }

    HashTable<T> *Copy()
    {TRACE_IT(21545);
        HashTable<T> *newTable = HashTable<T>::New(this->alloc, this->tableSize);

        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(21546);
            this->table[i].template CopyTo<Bucket<T>::Copy>(this->alloc, newTable->table[i]);
        }
#if PROFILE_DICTIONARY
        if (stats)
            newTable->stats = stats->Clone();
#endif
        return newTable;
    }

    void ClearAll()
    {TRACE_IT(21547);
        for (uint i = 0; i < this->tableSize; i++)
        {TRACE_IT(21548);
            this->table[i].Clear(this->alloc);
        }
#if PROFILE_DICTIONARY
        // To not lose previously collected data, we will treat cleared dictionary as a separate instance for stats tracking purpose
        stats = DictionaryStats::Create(typeid(this).name(), tableSize);
#endif
    }

#if DBG_DUMP
    void Dump(uint newLinePerEntry = 2);
    void Dump(void (*valueDump)(int));
#endif

protected:
    HashTable(TAllocator * allocator, DECLSPEC_GUARD_OVERFLOW uint tableSize) : alloc(allocator), tableSize(tableSize)
    {TRACE_IT(21549);
        Init();
#if PROFILE_DICTIONARY
        stats = DictionaryStats::Create(typeid(this).name(), tableSize);
#endif
    }
    void Init()
    {TRACE_IT(21550);
        table = (SListBase<Bucket<T>> *)(((char *)this) + sizeof(HashTable<T>));
        for (uint i = 0; i < tableSize; i++)
        {TRACE_IT(21551);
            // placement new
            ::new (&table[i]) SListBase<Bucket<T>>();
        }
    }
private:
    uint         Hash(int value) {TRACE_IT(21552); return (value % this->tableSize); }

#if PROFILE_DICTIONARY
    DictionaryStats *stats;
#endif
};

template <typename T, uint size, typename TAllocator = ArenaAllocator>
class HashTableS : public HashTable<T, TAllocator>
{
public:
    HashTableS(TAllocator * allocator) : HashTable(allocator, size) {TRACE_IT(21553);}
    void Reset()
    {TRACE_IT(21554);
        __super::Init();
    }
private:
    char tableSpace[size * sizeof(SListBase<Bucket<T>>)];
};

#define FOREACH_HASHTABLE_ENTRY(T, bucket, hashTable) \
    for (uint _iterHash = 0; _iterHash < (hashTable)->tableSize; _iterHash++)  \
    {   \
        FOREACH_SLISTBASE_ENTRY(Bucket<T>, bucket, &(hashTable)->table[_iterHash]) \
        {TRACE_IT(21555);

#define NEXT_HASHTABLE_ENTRY \
        } \
        NEXT_SLISTBASE_ENTRY; \
    }

#if DBG_DUMP
template <typename T, typename TAllocator>
inline void
HashTable<T, TAllocator>::Dump(uint newLinePerEntry)
{
    FOREACH_HASHTABLE_ENTRY(T, bucket, this)
    {TRACE_IT(21556);

        Output::Print(_u("%4d  =>  "), bucket.value);
        ::Dump<T>(bucket.element);
        for (uint i = 0; i < newLinePerEntry; i++)
        {TRACE_IT(21557);
            Output::Print(_u("\n"));
        }
    }
    NEXT_HASHTABLE_ENTRY;
}

template <typename T, typename TAllocator>
inline void
HashTable<T, TAllocator>::Dump(void (*valueDump)(int))
{
    FOREACH_HASHTABLE_ENTRY(T, bucket, this)
    {TRACE_IT(21558);
        valueDump(bucket.value);
        Output::Print(_u("  =>  "), bucket.value);
        ::Dump<T>(bucket.element);
        Output::Print(_u("\n"));
    }
    NEXT_HASHTABLE_ENTRY;
}

template <typename T>
inline void Dump(T const& t)
{TRACE_IT(21559);
    t.Dump();
}
#endif
